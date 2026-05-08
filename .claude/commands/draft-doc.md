---
description: Draft a Plans & Specs entry (batch close, Forks entry, Future State addition, etc.). Drafter mode — returns text, doesn't autonomously edit.
argument-hint: <mode: "running-notes" | "batch-close" | "forks-entry" | "future-state" + context>
---

Dispatch the `doc-drafter` agent.

Tell the agent which drafting mode to operate in and pass the context it needs:

- **`running-notes`** — periodically during a batch, gather what's been done so far. Pass current chat context + recent commits. Returns appendable running notes.
- **`batch-close`** — at end of a batch, compile the full Implemented Work Log entry. Pass the running notes (or ask agent to compile from git history).
- **`forks-entry`** — after a finding requires §9 Forks routing per Rule 3. Pass the finding + the routing decision.
- **`future-state`** — a discussion produced a post-V1 candidate. Pass description + value tag (AQ/PE/UT/WP/OT) suggestion.

The agent returns proposed text in a code block. I review and apply via Edit. The agent never autonomously writes to Plans & Specs/.
