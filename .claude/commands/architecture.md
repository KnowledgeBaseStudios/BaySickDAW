---
description: Research how other DAWs solve a specific audio-engine architecture problem, returns a comparative analysis with implementation recommendation for BaySickDAW.
argument-hint: <topic — e.g. "thread-pool work-stealing" or "FFT plan caching">
---

Dispatch the `daw-architecture-research` agent with `$ARGUMENTS` as the architectural topic.

The agent will:
1. Read our current implementation in `Source/Engine/`, `PluginProcessor.{h,cpp}`, `VibeGraph.{h,cpp}` + `Plans & Specs/Carry-Forward Reference.md` to know what we already do.
2. WebSearch + WebFetch vendor engineering blogs / dev talks / open-source repos / academic papers for how others solve the topic.
3. Return a comparative analysis report with a recommendation for BaySickDAW, citing all sources with HIGH/MEDIUM/LOW confidence.

If `$ARGUMENTS` is empty or too vague, the agent will ASK for a specific topic before running. Don't dispatch with a fuzzy brief.

Show the report. Don't apply any architecture changes — discuss with me first.

This agent is **on-demand** (not recurring). Invoke pre-architecture-decision or pre-milestone.

**Distinct from:**
- `/explain <concept>` — explains what a concept IS in our codebase
- `/research <focus>` — competitive feature research for `Future State.md`
- `/perf-audit` — audits our own code for performance issues
