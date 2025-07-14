# Deduplicate Text Component - Examples

This directory contains example configurations for the `deduplicate_text` component.

## Files

### `basic-usage.yaml`
Complete ESPHome configuration demonstrating multiple `deduplicate_text` sensors with different use cases:
- Device status that changes periodically
- Static system information 
- Random message generation

**Usage**: Copy to your ESPHome root directory and modify as needed.

### `simple-test.yaml`
Minimal test configuration to validate the component works correctly. The sensor always returns the same value to test that deduplication prevents duplicate publications.

**Usage**: Copy to your ESPHome root directory and run:
```bash
esphome config simple-test.yaml
```

## How to Use Examples

1. Copy the desired example file to your ESPHome root directory (where your other `.yaml` configs are)
2. Ensure you have the `components/` directory in your ESPHome root with the `deduplicate_text` component
3. Modify the configuration as needed for your hardware and requirements
4. Compile and flash as usual

## Component Structure

```
your-esphome-project/
├── your-config.yaml          # Your main configs here
├── components/
│   └── deduplicate_text/     # Component files
│       ├── examples/         # ← These examples
│       ├── deduplicate_text.h
│       ├── deduplicate_text.cpp
│       └── ...
└── ...
```

The examples assume this directory structure with the component located at `components/deduplicate_text/`. 