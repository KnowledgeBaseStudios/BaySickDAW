---
description: Audit BaySickDAW codebase for performance opportunities. Context-aware (audio thread vs setup time). Run every 3 batches OR pre-milestone.
---

Dispatch the `performance-auditor` agent.

The agent scans for: audio-thread allocations, lock contention, SIMD candidates, cache layouts, hot-path inlining, voice-mgmt waste, FFT plan reuse, memory-pool opportunities, APVTS dirty-flag pattern compliance, audio-thread fast-path bypass.

**Critical:** findings are context-classified (audio thread / control thread / setup time) BEFORE being flagged so legitimate prepare-time allocations don't get flagged as hot-path issues. A `new` in `prepareToPlay` is NOT a finding; a `new` in `processBlock` is HIGH.

Returns a ranked report saved to `Plans & Specs/Research Reports/performance-audit-<YYYY-MM-DD>.md` (drafter pattern — agent returns text, parent applies via Write).

Show the report. Don't apply fixes — discuss with me first; HIGH findings either fold into existing batch scope or get a dedicated `QA-Perf-Sweep-<N>` batch.

**Frequency:** every 3 batches OR pre-milestone (per Main Plan §0 Agent Orchestration Rules).

**Distinct from:**
- `/architecture <topic>` — researches OTHER DAWs' performance approaches
- `/explain <concept>` — explains what a concept IS
- `/review-batch` — checks one batch's diff against rules
