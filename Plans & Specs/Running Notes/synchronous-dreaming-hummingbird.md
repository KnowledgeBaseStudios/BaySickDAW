# Running Notes — QA-Ef (synchronous-dreaming-hummingbird)

> Append-only mid-batch log. A new `## YYYY-MM-DD — Task N — <name>` entry is
> appended at every checkpoint (commit landed / sub-task verified / finding
> captured / spec call resolved / scope pivot) per
> `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close,
> `/draft-doc batch-close` consumes this file as the primary input for the
> single Implemented Work Log entry. Never edited retroactively.

**Pair:** `Plans & Specs/Batch Plans/synchronous-dreaming-hummingbird.md` (the plan).
**Conventions:** Main Plan §0 — Document Formatting Conventions + running-notes
required sections (locked 2026-05-11) + Rule 4 (Diagnostic Instrumentation Catalog).

---

## 2026-05-21 — Task 0 — Batch open (docs)

- **Scope.** QA-Ef deletes the serial (ST) render path so MT
  (`RenderGraphDispatcher`) is the single, unconditional render path. The
  serial-execution bisect tool ("is the bug in the parallelism or the logic?")
  is preserved without a duplicate code path via a worker-park serial-diagnostic
  mode.
- **Pre-batch.** `/standup` (QA-Ea closed; tree clean; 2 unpushed commits;
  QA-Ef next per §6). Full direct self-read of Main Plan §0. `/read-doc`
  extractions: §5 QA-Ef + §6 arrow + §9 twenty-fifth (ST-only/mirrored/inert
  inventory) + §9 twenty-seventh (re-slot) + Carry-Forward §1/§4/§6 + Work Log
  QA-Ea/QA-E closes.
- **Spec calls resolved with Jeff (2026-05-21):**
  - SC-diag-ui = **reuse** the Mixer "Multi-core Rendering" toggle (OFF = serial
    diagnostic); keep `<MultiCoreRendering>` persistence. User-facing meaning of
    the toggle is unchanged.
  - SC-serial-meaning = **fully serial** (all workers park, audio thread does
    100%). Answers Jeff's "which most resembles the setup we are removing" — the
    deleted serial path ran with zero workers.
  - SC-cleanup-scope = **both cleanups** folded in (dead busAnySolo +
    BlockContext slim-down; orphaned L/B/D buffers + Part B Task 2 params).
  - SC-relabel (minor sub-call) = lean keep "Multi-core Rendering"; Jeff may
    override at any point.
- **Code grounding done in plan mode (exact bounds re-verified in Task 1):**
  MT branch at `PluginProcessor.cpp:1931`; early `return` `:2005`; serial tail
  ~`:2008` → `processBlock` close (~`:2838`). Shared feeds called from the MT
  body: `applyPostMixRecordAndMetro` `:1987`, `drainMeterAtomicsForUI` `:1994`,
  `measureDspLoadAndOverload` `:2004` (defs `:3046` / `:3117`, outside the tail).
  `VibeThreadPool` clamps workers `jlimit(1, kMaxWorkers=8, …)` + the audio
  thread drains via `runUntilOrTimeout` → fully-serial = park all workers.
  `CompositeAudioInsertTask` present (DSP-12 resolution).
- **CLAUDE.md staleness noted (NOT this batch's job):** "Next batch QA-Md" and
  the "Bus (5 total)" mixer table are both stale (QA-Ef is next; code registers
  11 buses — flagged in the QA-Ea close).
- **Task 0 actions:** plan mirrored to `Batch Plans/` (+ home copy deleted);
  §5 QA-Ef `**Plan file:**` pointer set; this running-notes file seeded.

## Diagnostic Instrumentation Catalog

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| _(none added yet — QA-Ef is mostly deletion)_ | | | |

**Pre-existing (Keep):** MT diagnostic counters (`RenderEngineFlags.h`
`MtDiagnostic` namespace) + "Run MT Diagnostic (2s capture)" Mixer menu item 203
(`StandaloneEditor.cpp`) — CL-292; instruments the MT dispatch path and pairs
with the new serial-diagnostic mode (shows ~100% main-thread when serial). DSP
meter cap `10.f` in `measureDspLoadAndOverload` — CL-291 (HOLD-FOR-Phase-6).
