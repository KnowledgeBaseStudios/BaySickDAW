---
description: Review a QA-* batch's diff against its plan + project conventions before commit. Run at batch close.
argument-hint: <batch ID like QA-Md, optional plan path>
---

Dispatch the `batch-code-reviewer` agent with `$ARGUMENTS` as the batch ID (and optionally the plan-file path).

The agent will:
1. Read the batch's plan file (`Plans & Specs/Batch Plans/<silly-name>.md` if exists).
2. Diff the batch's commits against pre-batch state.
3. Check against CLAUDE.md rules, JUCE gotchas, canonical conventions, and recurring memory-tracked pitfalls.
4. Return findings tagged BLOCKER / NEEDS-FIX / NIT plus a final recommendation.

Show the output. If there are BLOCKERS, DO NOT proceed to commit until they're addressed.
