/*
 * DieselHeaterRF.cpp
 * Copyright (c) 2020 Jarno Kyttälä
 * https://github.com/jakkik/DieselHeaterRF
 *
 * Modified by Dmitry Mokretsov (2026) — see DieselHeaterRF.h for change log.
 *
 * ---------------------------------
 *
 * Simple class for Arduino to control an inexpensive Chinese diesel
 * heater through 433 MHz RF by using a TI CC1101 transceiver.
 * Replicates the protocol used by the four button "red LCD remote" with
 * an OLED screen, and should probably work if your heater supports this
 * type of remote controller.
 *
 * Protocol notes (reverse-engineered):
 *   TX packet: 10 bytes — [0x09 (len), cmd, addr[3:0], seq, crc_hi, crc_lo, 0x00]
 *   RX packet: 26 bytes — 23 data bytes + 2 CC1101 APPEND_STATUS bytes (RSSI, LQI/CRC_OK)
 *   HEATER_CMD_GET_STATUS (0x23): requests a state packet; also acts as a keep-alive ping
 *   HEATER_CMD_MODE (0x24), HEATER_CMD_POWER (0x2B): toggle commands — each unique-seq
 *     packet fires one toggle, so send exactly 1 packet per desired toggle
 *   HEATER_CMD_UP (0x3C), HEATER_CMD_DOWN (0x3E): accumulator commands — heater applies
 *     the most recent direction once per burst; 14-packet bursts improve hit probability
 *   The heater responds to any valid command regardless of its WOR (Wake-On-Radio) sleep
 *     state — no explicit wake sequence is required before issuing commands
 *
 * Happy hacking!
 *
 */

#include <Arduino.h>
#include <SPI.h>
#include "DieselHeaterRF.h"

void DieselHeaterRF::begin() {
  begin(0);
}

void DieselHeaterRF::begin(uint32_t heaterAddr) {

  _heaterAddr = heaterAddr;

  pinMode(_pinSck, OUTPUT);
  pinMode(_pinMosi, OUTPUT);
  pinMode(_pinMiso, INPUT);
  pinMode(_pinSs, OUTPUT);
  pinMode(_pinGdo2, INPUT);

  digitalWrite(_pinSs, HIGH);  // CS idle-high before SPI init
  SPI.begin(_pinSck, _pinMiso, _pinMosi, _pinSs);

  delay(100);

  initRadio();

}

void DieselHeaterRF::setAddress(uint32_t heaterAddr) {
  _heaterAddr = heaterAddr;
}

bool DieselHeaterRF::getState(heater_state_t *state) {
  return getState(state, HEATER_RX_TIMEOUT);
}

bool DieselHeaterRF::getState(heater_state_t *state, uint32_t timeout) {
  return getState(&state->state, &state->power, &state->voltage, &state->ambientTemp, &state->caseTemp, &state->setpoint, &state->pumpFreq, &state->autoMode, &state->rssi, timeout);
}

bool DieselHeaterRF::getState(uint8_t *state, uint8_t *power, float *voltage, int8_t *ambientTemp, uint8_t *caseTemp, int8_t *setpoint, float *pumpFreq, uint8_t *autoMode, int16_t *rssi, uint32_t timeout) {

  char buf[26];

  if (receivePacket(buf, timeout)) {

    uint32_t address = parseAddress(buf);

    if (address != _heaterAddr) return false;

    *state = buf[6];
    *power = buf[7];
    *voltage = buf[9] / 10.0f;
    *ambientTemp = buf[10];
    *caseTemp = buf[12];
    *setpoint = buf[13];
    *autoMode = buf[14] == 0x32; // 0x32 = auto (thermostat), 0xCD = manual (Hertz mode)
    *pumpFreq = buf[15] / 10.0f;
    *rssi = (buf[24] - (buf[24] >= 128 ? 256 : 0)) / 2 - 74;  // APPEND_STATUS byte 0
    return true;
  }

  return false;

}

void DieselHeaterRF::sendCommand(uint8_t cmd) {
  if (_heaterAddr == 0x00) return;
  sendCommand(cmd, _heaterAddr, HEATER_TX_REPEAT);
}

void DieselHeaterRF::sendCommand(uint8_t cmd, uint32_t addr) {
  sendCommand(cmd, addr, HEATER_TX_REPEAT);
}

void DieselHeaterRF::sendCommand(uint8_t cmd, uint32_t addr, uint8_t numTransmits) {

  unsigned long t;
  char buf[10];

  buf[0] = 9; // Packet length, excl. self
  buf[1] = cmd;
  buf[2] = (addr >> 24) & 0xFF;
  buf[3] = (addr >> 16) & 0xFF;
  buf[4] = (addr >> 8) & 0xFF;
  buf[5] = addr & 0xFF;
  buf[6] = _packetSeq++;
  buf[9] = 0;

  uint16_t crc = crc16_2(buf, 7);

  buf[7] = (crc >> 8) & 0xFF;
  buf[8] = crc & 0xFF;

  memcpy(_lastTxBuf, buf, 10);

  _lastTxActive = false;

  // CCA_MODE configurable via setCcaMode(): 0 = always TX (default), 3 = RSSI+no RX.
  // TXOFF_MODE=FSTXON: synthesizer stays locked between packets — single calibration keeps
  // all N packets on the same frequency (eliminates spectral drift seen on waterfall).
  writeReg(0x17, (_ccaMode << 4) | 0x01); // MCSM1: CCA_MODE=_ccaMode, TXOFF_MODE=FSTXON

  txFlush();  // SIDLE + SFTX → reach IDLE, flush TX FIFO; first STX triggers one auto-cal

  for (int i = 0; i < numTransmits; i++) {
    writeBurst(0x7F, 10, buf);  // load TX FIFO
    writeStrobe(0x35);           // STX (IDLE→TX with cal on i=0; FSTXON→TX no cal on i>0)

    // Phase 1 (no delay): spin until CC1101 leaves IDLE/FSTXON, confirming STX was accepted
    // and calibration or TX has actually started.
    t = millis();
    uint8_t ms;
    do {
      ms = writeReg(0xF5, 0xFF);
      if (millis() - t > 50) {   // should exit in <1 ms with CCA disabled
        writeReg(0x17, (_ccaMode << 4));
        writeStrobe(0x36);
        return;
      }
    } while (ms == 0x01 || ms == 0x12);  // 0x01=IDLE, 0x12=FSTXON

    // Phase 1 exits only when ms is neither IDLE nor FSTXON → STX was accepted.
    // During i=0 this is a calibration state (0x03–0x0C); during i>0 it is TX (0x13).
    _lastTxActive = true;

    // Phase 2: wait for packet to finish (synthesizer returns to FSTXON=0x12)
    t = millis();
    do {
      ms = writeReg(0xF5, 0xFF);
      delay(1);
      if (millis() - t > 100) {
        writeReg(0x17, (_ccaMode << 4));
        writeStrobe(0x36);
        return;
      }
    } while (ms != 0x12);  // 0x12 = FSTXON: packet done, synthesizer still locked
  }

  // Restore MCSM1 and leave radio in IDLE
  writeReg(0x17, (_ccaMode << 4)); // MCSM1: CCA_MODE=_ccaMode, TXOFF_MODE=IDLE
  writeStrobe(0x36);    // SIDLE

}

void DieselHeaterRF::resendLastCommand(uint8_t numTransmits) {
  if (_lastTxBuf[0] == 0) return;  // no prior command (buf[0]=9 for valid packets)

  unsigned long t;

  writeReg(0x17, (_ccaMode << 4) | 0x01); // MCSM1: CCA_MODE=_ccaMode, TXOFF_MODE=FSTXON
  txFlush();

  for (int i = 0; i < numTransmits; i++) {
    writeBurst(0x7F, 10, (char*)_lastTxBuf);
    writeStrobe(0x35);  // STX

    t = millis();
    uint8_t ms;
    do {
      ms = writeReg(0xF5, 0xFF);
      if (millis() - t > 50) { writeReg(0x17, (_ccaMode << 4)); writeStrobe(0x36); return; }
    } while (ms == 0x01 || ms == 0x12);

    _lastTxActive = true;

    t = millis();
    do {
      ms = writeReg(0xF5, 0xFF);
      delay(1);
      if (millis() - t > 100) { writeReg(0x17, (_ccaMode << 4)); writeStrobe(0x36); return; }
    } while (ms != 0x12);
  }

  writeReg(0x17, (_ccaMode << 4)); // MCSM1: CCA_MODE=_ccaMode, TXOFF_MODE=IDLE
  writeStrobe(0x36);  // SIDLE
}

uint8_t DieselHeaterRF::getPartNum()   { return writeReg(0xF0, 0xFF); }
uint8_t DieselHeaterRF::getVersion()   { return writeReg(0xF1, 0xFF); }
uint8_t DieselHeaterRF::getMarcstate() { return writeReg(0xF5, 0xFF); }
void DieselHeaterRF::getFreqRegisters(uint8_t *freq2, uint8_t *freq1, uint8_t *freq0) {
  *freq2 = writeReg(0x8D, 0xFF);  // read FREQ2
  *freq1 = writeReg(0x8E, 0xFF);  // read FREQ1
  *freq0 = writeReg(0x8F, 0xFF);  // read FREQ0
}

void DieselHeaterRF::setFrequency(uint8_t freq2, uint8_t freq1, uint8_t freq0) {
  _freq2 = freq2;
  _freq1 = freq1;
  _freq0 = freq0;
}

bool DieselHeaterRF::receiveRaw(char *bytes, uint8_t *len, uint16_t timeout) {
  unsigned long t = millis();
  *len = 0;

  writeReg(0x00, 0x01); // IOCFG2: assert when RX FIFO ≥ threshold or end of packet
  rxFlush();
  rxEnable();

  while (!digitalRead(_pinGdo2)) {
    if (millis() - t > timeout) {
      writeReg(0x00, 0x07); // restore IOCFG2
      return false;
    }
    yield();
  }

  uint8_t rxLen = writeReg(0xFB, 0xFF);  // RXBYTES
  if (rxLen == 0 || rxLen > 64) {
    writeReg(0x00, 0x07); // restore IOCFG2
    rxFlush();
    return false;
  }

  rx(rxLen, bytes);
  rxFlush();
  writeReg(0x00, 0x07); // restore IOCFG2
  *len = rxLen;
  return true;
}

uint32_t DieselHeaterRF::findAddress(uint16_t timeout) {

  char buf[26];

  if (receivePacket(buf, timeout)) {
    uint32_t address = parseAddress(buf);
    return address;
  }

  return 0;

}

uint32_t DieselHeaterRF::parseAddress(char *buf) {
  uint32_t address = 0;
  address |= (buf[2] << 24);
  address |= (buf[3] << 16);
  address |= (buf[4] << 8);
  address |= buf[5];
  return address;
}

bool DieselHeaterRF::receivePacket(char *bytes, uint16_t timeout) {

  unsigned long t = millis();
  uint8_t rxLen;

  rxFlush();
  rxEnable();

  while (1) {

    if (millis() - t > timeout) { rxFlush(); return false; }

    while (!digitalRead(_pinGdo2)) {
      if (millis() - t > timeout) { rxFlush(); return false; }
      yield();
    }

    rxLen = writeReg(0xFB, 0xFF);

    if (rxLen == 26) break;  // length byte 0x17=23 + 2 APPEND_STATUS = 26 total

    rxFlush();
    rxEnable();

  }

  // Read RX FIFO
  rx(rxLen, bytes);

  rxFlush();

  // Hardware CRC validated by CC1101 (APPEND_STATUS: buf[25] bit7 = CRC_OK)
  // Software CRC check removed — heater variant CRC bytes don't match crc16_2 positions
  if ((uint8_t)bytes[25] & 0x80) {
    return true;
  }

  return false;

}

void DieselHeaterRF::initRadio() {

  writeStrobe(0x30); // SRES

  delay(100);

  writeReg(0x00, 0x07); // IOCFG2: assert when packet received with CRC OK
  writeReg(0x02, 0x06); // IOCFG0
  writeReg(0x03, 0x47); // FIFOTHR
  writeReg(0x07, 0x04); // PKTCTRL1
  writeReg(0x08, 0x05); // PKTCTRL0
  writeReg(0x0A, 0x00); // CHANNR
  writeReg(0x0B, 0x06); // FSCTRL1
  writeReg(0x0C, 0x00); // FSCTRL0
  writeReg(0x0D, _freq2); // FREQ2
  writeReg(0x0E, _freq1); // FREQ1
  writeReg(0x0F, _freq0); // FREQ0
  writeReg(0x10, 0xF8); // MDMCFG4
  writeReg(0x11, 0x93); // MDMCFG3
  writeReg(0x12, 0x13); // MDMCFG2
  writeReg(0x13, 0x22); // MDMCFG1: NUM_PREAMBLE=4 bytes (matches original remote)
  writeReg(0x14, 0xF8); // MDMCFG0
  writeReg(0x15, 0x26); // DEVIATN
  writeReg(0x17, (_ccaMode << 4)); // MCSM1: CCA_MODE=_ccaMode, TXOFF_MODE=IDLE
  writeReg(0x18, 0x18); // MCSM0
  writeReg(0x19, 0x17); // FOCCFG — FOC_LIMIT=±BW/2=±29 kHz (was 0x16=±14.5 kHz)
  writeReg(0x1A, 0x6C); // BSCFG
  writeReg(0x1B, 0x03); // AGCTRL2
  writeReg(0x1C, 0x40); // AGCTRL1
  writeReg(0x1D, 0x91); // AGCTRL0
  writeReg(0x20, 0xFB); // WORCTRL
  writeReg(0x21, 0x56); // FREND1
  writeReg(0x22, 0x17); // FREND0
  writeReg(0x23, 0xE9); // FSCAL3
  writeReg(0x24, 0x2A); // FSCAL2
  writeReg(0x25, 0x00); // FSCAL1
  writeReg(0x26, 0x1F); // FSCAL0
  writeReg(0x2C, 0x81); // TEST2
  writeReg(0x2D, 0x35); // TEST1
  writeReg(0x2E, 0x09); // TEST0
  writeReg(0x09, 0x00); // ADDR
  writeReg(0x04, 0x7E); // SYNC1
  writeReg(0x05, 0x3C); // SYNC0

  char patable[8] = {0x00, 0x12, 0x0E, 0x34, 0x60, (char)0xC5, (char)0xC1, (char)0xC0};
  writeBurst(0x7E, 8, patable); // PATABLE

  writeStrobe(0x31); // SFSTXON
  writeStrobe(0x36); // SIDLE
  writeStrobe(0x3B); // SFTX
  writeStrobe(0x36); // SIDLE
  writeStrobe(0x3A); // SFRX

  delay(136);

}

void DieselHeaterRF::txBurst(uint8_t len, char *bytes) {
    txFlush();
    writeBurst(0x7F, len, bytes);
    writeStrobe(0x35); // STX
}

void DieselHeaterRF::txFlush() {
  writeStrobe(0x36); // SIDLE
  writeStrobe(0x3B); // SFTX
}

void DieselHeaterRF::rx(uint8_t len, char *bytes) {
  for (int i = 0; i < len; i++) {
    bytes[i] = writeReg(0xBF, 0xFF);
  }
}

void DieselHeaterRF::rxFlush() {
  writeStrobe(0x36); // SIDLE
  writeReg(0xBF, 0xFF); // Dummy read to de-assert GDO2
  writeStrobe(0x3A); // SFRX
  delay(16);
}

void DieselHeaterRF::rxEnable() {
  writeStrobe(0x34); // SRX
}

uint8_t DieselHeaterRF::writeReg(uint8_t addr, uint8_t val) {
  spiStart();
  SPI.transfer(addr);
  uint8_t tmp = SPI.transfer(val);
  spiEnd();
  return tmp;
}

void DieselHeaterRF::writeBurst(uint8_t addr, uint8_t len, char *bytes) {
  spiStart();
  SPI.transfer(addr);
  for (int i = 0; i < len; i++) {
    SPI.transfer(bytes[i]);
  }
  spiEnd();
}

void DieselHeaterRF::writeStrobe(uint8_t addr) {
  spiStart();
  SPI.transfer(addr);
  spiEnd();
}

void DieselHeaterRF::spiStart() {
  digitalWrite(_pinSs, LOW);
  // Wait for CC1101 chip-ready (MISO goes LOW) with 5 ms timeout
  unsigned long t = micros();
  while (digitalRead(_pinMiso)) {
    if (micros() - t > 5000) break;
  }
}

void DieselHeaterRF::spiEnd() {
  digitalWrite(_pinSs, HIGH);
}

/*
 * CRC-16/MODBUS
 */
uint16_t DieselHeaterRF::crc16_2(char *buf, int len) {

  uint16_t crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++) {
    crc ^= (byte)buf[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}
