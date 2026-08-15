---
name: feedback-memory-privacy
description: This memory dir is committed to a public repo; infra details go to the global memory index instead
metadata:
  type: feedback
---

This project's `.claude/memory/` is tracked in the public repo.

**Why:** the repo must stay self-sufficient and leak nothing private.

**How to apply:**
- Only memories that make sense for any user of this public repo
  belong here: project conventions, hardware facts about the configs
  themselves, workflow feedback.
- Anything describing the user's own infrastructure or machines goes
  in the global memory index at `~/.claude/memory/` (dotfiles-tracked)
  instead.
- Never add links pointing outside the repo to this index; other
  users will not have those files.
