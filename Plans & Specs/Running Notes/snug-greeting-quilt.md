# Running Notes — QA-DispatcherAffinity (snug-greeting-quilt)

> **Purpose:** append-only mid-batch log populated at every checkpoint (commit landed / sub-task verified / finding captured / spec call resolved / scope pivot / diagnostic instrumentation added) per Main Plan §0 Rule 4 + the `/draft-doc running-notes` cadence.  At batch close, `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry.  Never edit prior entries — surprise findings get their own new entry below.

> **Pair file:** `Plans & Specs/Batch Plans/snug-greeting-quilt.md` (per-batch plan).
> **Convention reference:** Main Plan §0 "Running notes file required sections" (locked 2026-05-11; exemplar `federated-bouncing-cupcake.md`).

---

## 2026-05-28 — Task 0 — Batch open

**Mirror + delete:** plan file `snug-greeting-quilt.md` mirrored from `~/.claude/plans/snug-greeting-quilt.md` → `Plans & Specs/Batch Plans/snug-greeting-quilt.md`; home-dir copy deleted per `feedback_plan_mirror_one_way.md`.

**Main Plan updates:**
- §5 QA-DispatcherAffinity entry — STATUS banner ADDED summarizing the plan-mode double pivot (global barrier rejected → DAG upgrade rejected post-exploration → investigation-first sfizz Candidate B reframe); `**Plan file:**` updated from `<silly-name>.md (when started)` placeholder to backticked-path form `Plans & Specs/Batch Plans/snug-greeting-quilt.md`.
- §9 forty-first Forks entry ADDED — full pivot chronology + Jeff's verbatim quotes + source-verification findings + post-pivot task structure.
- §6 — no arrow change; QA-DispatcherAffinity 25-asterisk footnote stays as-is.

**Plan-mode spec calls resolved (carried over to plan file's Spec calls already locked table S1-S12):**
- S1: Q1 / Q1' = Option (e) investigation-first; all fix-shape picks deferred to Task 2 mid-batch spec call.
- S2: Q2 = REJECTED (no global synchronization barrier — Jeff verbatim).
- S3: Q2 pivot = DAG + topological sort upgrade — REJECTED post-exploration (dispatcher already implements dep-driven DAG via `mDeps` + `mInitialDeps` + `mChildren` + `mPredecessors` on `RenderTask` + `addSyntheticDep` at `PluginProcessor.cpp:4142`).
- S4: §9 fortieth Candidate A cross-block race hypothesis = dead (`mAllDone` gating).
- S5: Q3 (Candidate B implementation shape) = DEFERRED to Task 2.
- S6: Q4 = Option (C); re-interpreted post-pivot as "B.1 in scope for trace investigation alongside B.2/B.3/B.4".
- S7: Sub-K retirement = conditional on Task 3 cure verify.
- S8: Verify gate = BaySickRustyDrums 6-cymbal crash MT-on test (same as QA-Sfizz Sub-K).
- S9: Trace captures entry+exit timestamps + thread IDs (Sub-F=(e) entry-only-trace lesson).
- S10: Silly-name = `snug-greeting-quilt` (harness-assigned).
- S11: One commit per task structure (Task 0 / Task 1 / Task 3 / Task 4 conditional / Task 5 close).
- S12: Trace instrumentation Disposition = `Remove at Task 4 close` (or batch close if Task 4 skipped).

**Sub-spec calls genuinely deferred to mid-batch:**
- Sub-A: Task 3 fix shape — pick after Task 1 trace data lands at Task 2 spec call.

**Working tree state at Task 0 commit time:** 3 CRLF-residue files (`Source/BaySickBasses/BaySickBassesProcessor.h`, `Source/BaySickGuitars/BaySickGuitarsProcessor.h`, `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h`) — pure CRLF normalization residue from QA-Sfizz Sub-G + Sub-I `git restore --source=HEAD` reverts; verified zero content diff via `git diff --ignore-all-space`; documented harmless in QA-Sfizz Task 6 close commit `5079c5d`.  Left untouched (not staged) per pre-commit discipline.

**Task 0 commit:** landed at `16217b8` (2026-05-28).  `git commit -F .git/COMMIT_EDITMSG_QA-DispatcherAffinity-task-0.txt` per the project's long-message convention (CLAUDE.md "Git Commit Mechanics"); temp file `rm`'d post-commit.  Staged scope = 3 files (M Main Plan.md / A Batch Plans/snug-greeting-quilt.md / A Running Notes/snug-greeting-quilt.md, total +418 / -1 lines); unstaged + intentionally left untouched = 3 CRLF-residue *Processor.h files (documented harmless from QA-Sfizz Sub-G + Sub-I reverts, surfaced in the commit body's "Still-pending working-tree carry" paragraph).  Local branch now 32 commits ahead of `origin/main` (still not pushed).

---

## 2026-05-28 — Task 0 — Batch open commit landed

Task 0 closes with no source code touched (doc-only).  Plan + Running Notes + §5 STATUS banner + §9 forty-first Forks entry all in place; QA-DispatcherAffinity is now open for execution.

**Next:** Task 1 — Timestamped trace instrumentation on the 14 sfizz tasks (RustyDrumsProducerTask + 13 RustyInsertTask + 2 InstStripTask sfizz-engine branches).  Will start with end-to-end reads of `Source/Engine/RenderEngineFlags.h` + `RustyDrumsProducerTask.cpp` + `RustyInsertTask.cpp` + `InstStripTask.cpp` to identify exact `run()` body boundaries + the sfizz-engine branch in InstStripTask, per the plan's Task 1 checklist.

---

## 2026-05-28 — Task 1 — Timestamped trace instrumentation

Per the plan's Task 1 checklist, the entry+exit timestamp trace ring + 14-task instrumentation + Mixer hamburger toggle landed across 6 files.  Audio-thread-safe via lock-free atomic ring (no allocation, no lock, no OS calls on the hot path).  Dump runs on the message thread after the toggle flips off + a 20 ms settle window.

**Files touched:**
- `Source/Engine/RenderEngineFlags.h` — extended `MtDiagnostic` namespace with TraceEvent POD + 65536-slot lock-free ring + gTraceTaskTimestamps atomic flag + gTraceWriteIndex + gBlockIndex + recordTraceEvent + resetTrace + RAII `TraceScope` helper.  Includes `<chrono>` + `<array>` + `<cstdint>` + `<functional>` + `<thread>` added at top (no JuceHeader.h pollution; the ring + recording functions are stdlib-only).
- `Source/Engine/RenderGraphDispatcher.cpp` — gBlockIndex.fetch_add at the existing gBlockCount.fetch_add site (~line 230) when gTraceTaskTimestamps is on.
- `Source/Engine/Tasks/RustyDrumsProducerTask.cpp` — RAII TraceScope at top of `run()`; engineInstance = `mProcessor->mRustyDrumsEngine.get()` (the BaySickRustyDrumsProcessor singleton).  Added `#include "../RenderEngineFlags.h"`.
- `Source/Engine/Tasks/RustyInsertTask.cpp` — RAII TraceScope at top of `run()`; same engineInstance source as the producer so the analyzer can correlate the 14 Rusty tasks by engine.  Added `#include "../RenderEngineFlags.h"`.
- `Source/Engine/Tasks/InstStripTask.h` — new `bool mIsSfizzEngine = false` member (cached sfizz-engine detection; decoupled from `mAudioThreadOnly` so the trace continues to fire on sfizz instances even when Sub-K is overridden at Task 2 Stage B).
- `Source/Engine/Tasks/InstStripTask.cpp` — constructor sets `mIsSfizzEngine` via `dynamic_cast<BaySickGuitarsProcessor*>` / `dynamic_cast<BaySickBassesProcessor*>`; RAII TraceScope at top of `run()` gated on `mIsSfizzEngine` (non-sfizz Inst tasks construct trivial-no-op TraceScope).  Added `#include "../RenderEngineFlags.h"`.
- `Source/Standalone/StandaloneEditor.cpp` — Mixer hamburger menu item 204 "QA-DispatcherAffinity Trace" (checkable; reflects current gTraceTaskTimestamps state) + handler that arms/disarms the trace and dumps the ring as sorted CSV to `Documents\BaySickDAW\qa-dispatcheraffinity-trace.log` on toggle-off + AlertWindow confirmation with event count + wrap warning.

**Design notes:**
- Steady-clock nanoseconds for entry/exit timestamps (std::chrono::steady_clock; monotonic, sub-microsecond precision on Windows; no JUCE dependency).
- Thread ID hash = lower 32 bits of `std::hash<std::thread::id>{}` (discriminates the ~9 entities — 8 workers + audio thread — without collisions while keeping TraceEvent compact).
- Ring capacity 65536 = 2^16 (power of 2 → mask-based slot indexing instead of modulo).  Covers ~27 seconds of trace history at ~14 sfizz tasks × ~170 blocks/sec — comfortably above the 6-second reproducer.
- TraceScope ctor on the false-flag path: one relaxed atomic load + 3 stack assignments + dtor early-return.  Negligible hot-path overhead when the trace is off.
- Dump uses `juce::FileOutputStream` on the message thread; sort by entryNs for analysis ergonomics; CSV columns `blockIndex,channelId,engineInstance,threadIdHash,entryNs,exitNs,durationNs`.

**Task 1 commit:** TBD (after Jeff's Debug + Release verify pass + `/draft-commit` + approval).

**Build hand-off:** verify script in plan Task 1 step 11 (8 numbered scenarios).

## Diagnostic Instrumentation Catalog

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `Source/Engine/RenderEngineFlags.h` MtDiagnostic namespace — TraceEvent struct + gTraceTaskTimestamps + gTraceWriteIndex + gBlockIndex + gTraceRing + traceNowNs + traceThreadHash + recordTraceEvent + resetTrace + TraceScope | `QA-DispatcherAffinity (2026-05-28)` (comment marker; no log string) | Trace infrastructure — lock-free ring + RAII scope helper | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
| `Source/Engine/RenderGraphDispatcher.cpp` ~line 230 — gBlockIndex.fetch_add | `QA-DispatcherAffinity (2026-05-28)` | Per-block index bump for trace block-boundary correlation | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
| `Source/Engine/Tasks/RustyDrumsProducerTask.cpp` top of `run()` — TraceScope qaTrace | `QA-DispatcherAffinity (2026-05-28)` | Entry+exit ticks + thread ID + engine instance for the Rusty producer task | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
| `Source/Engine/Tasks/RustyInsertTask.cpp` top of `run()` — TraceScope qaTrace | `QA-DispatcherAffinity (2026-05-28)` | Same as producer; captures all 13 RustyInsertTask invocations per block | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
| `Source/Engine/Tasks/InstStripTask.h` — `bool mIsSfizzEngine = false` field | `QA-DispatcherAffinity (2026-05-28)` | Cached sfizz-engine detection flag (gates trace; decoupled from Sub-K's `mAudioThreadOnly`) | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
| `Source/Engine/Tasks/InstStripTask.cpp` constructor — `dynamic_cast<BaySickGuitarsProcessor*>` / `<BaySickBassesProcessor*>` setter for mIsSfizzEngine | `QA-DispatcherAffinity (2026-05-28)` | Sfizz-engine detection at construction (message thread, one-shot per task) | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
| `Source/Engine/Tasks/InstStripTask.cpp` top of `run()` — TraceScope qaTrace (shouldTrace=mIsSfizzEngine) | `QA-DispatcherAffinity (2026-05-28)` | Entry+exit ticks for BaySickGuitars / BaySickBasses InstStripTasks; non-sfizz Inst tasks construct trivial-no-op TraceScope | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
| `Source/Standalone/StandaloneEditor.cpp` ~line 5175 — Mixer hamburger menu item 204 "QA-DispatcherAffinity Trace" + handler | `QA-DispatcherAffinity (2026-05-28)` | Runtime arm/disarm toggle + CSV dump to `Documents\BaySickDAW\qa-dispatcheraffinity-trace.log` | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
