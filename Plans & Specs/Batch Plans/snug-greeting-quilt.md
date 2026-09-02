# QA-DispatcherAffinity — sfizz Candidate B sub-mechanism investigation + targeted fix — Plan (snug-greeting-quilt)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/snug-greeting-quilt.md`
> Paired running notes: `Plans & Specs/Running Notes/snug-greeting-quilt.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule).

---

## Context

QA-DispatcherAffinity was inserted into Main Plan §5 / §6 / §9 on 2026-05-28 at QA-Sfizz Task 5 routing time to address the BaySickRustyDrums MT-mode bit-crusher artifact that the QA-Sfizz Sub-K Serial Fallback (`mAudioThreadOnly` per-task flag + `audioThreadQueue` MPSC infrastructure) is currently bypassing — Sub-K is shipping in production as a band-aid that pins the 4 sfizz-driven engine task families (`RustyDrumsProducerTask` + 13 `RustyInsertTask` per Rusty + `InstStripTask` whose engine kind is `BaySickGuitarsProcessor` or `BaySickBassesProcessor`) to the audio thread. The §5 entry framed two candidate fix shapes: Candidate A (BaySickDAW-dispatcher-level cross-block barrier on `mMultiOutScratch`) and Candidate B (sfizz-internal-level per-engine-instance worker affinity, with 4 sub-mechanisms B.1 thread-local-state continuity / B.2 non-atomic RR voice swapping / B.3 false sharing across CPU cores / B.4 disk streaming engine contention).

**Two pivots during plan mode (see §9 forty-first Forks entry):**

1. **Global barrier rejected (Jeff at plan-mode entry).** "A global lock is a massive serialization point that will bottleneck my CPU and artificially degrade the DAW's multi-core headroom." Replaced with a proposed DAG + topological sort upgrade to `VibeThreadPool` + `RenderGraphDispatcher`.

2. **DAG-upgrade framing rejected post-exploration (Jeff after Phase 1 source verification).** Phase 1 exploration confirmed the current dispatcher already implements dep-driven DAG topological execution: `RenderTask` has `mDeps` (atomic counter) + `mInitialDeps` (snapshot) + `mChildren` + `mPredecessors`; `RenderGraphDispatcher::rebuildLinks` wires deps from `RoutingGraph` edges + `mSyntheticDeps`; `RenderGraphDispatcher::addSyntheticDep(RustyDrumsProducerTask, RustyInsertTask)` is already called at [PluginProcessor.cpp:4142](Source/PluginProcessor.cpp:4142) for all 13 inserts; `VibeThreadPool::runOneTask` decrements each child's `mDeps` with acq_rel ordering and submits when counter hits zero. `dispatchBlock` blocks the audio thread on `mAllDone` (set by MasterTask which runs last) — block N+1's `dispatchBlock` cannot start until block N completes. **The §9 fortieth Candidate A cross-block race hypothesis was structurally impossible against the current code.** Jeff verbatim 2026-05-28: "You did exactly what a senior engineer should do: you verified the architectural assumptions against the actual source code before writing the plan... the cross-block race hypothesis is dead. I misread the gating on runUntilOrTimeout. If Block N+1 cannot start until mAllDone fires for Block N, then Candidate A is structurally impossible."

**New batch framing (locked 2026-05-28 mid-plan-mode):** QA-DispatcherAffinity is fundamentally a **data-driven bug hunt** targeting the sfizz Candidate B mechanisms (B.1/B.2/B.3/B.4). No dispatcher refactor; no DAG upgrade; no barrier. Task 1 stands up a timestamped entry+exit trace on the 14 sfizz tasks; Task 2 reviews the data + locks the fix shape; Task 3 implements the locked fix; Task 4 retires Sub-K if the fix verifies.

**Dependencies:** QA-Sfizz closed (commit `5079c5d`, 2026-05-28). Working tree clean except 3 CRLF-residue files (documented harmless in QA-Sfizz Task 6 close commit).

**Risk:** **medium**. Task 1 (instrumentation) is low-risk read-only diagnostic addition; Task 3 (fix) risk depends on the locked scope (B.1 worker-affinity touches dispatcher core medium-high; B.2/B.3 sfizz vendored-library patch touches the vendored sfizz subtree medium; B.4 audit-only with possible upstream sfizz PR low-medium). Worst case: trace data isolates a sub-mechanism whose fix is out of reach in-batch (e.g. requires upstream sfizz refactor) — Sub-K stays as the band-aid + the unresolved scope routes forward via §9 entry.

**Effort estimate:** ~10-14 hours (~3-4 hr Task 1 instrumentation + ~1-2 hr Task 2 analysis + ~3-6 hr Task 3 implementation depending on locked scope + ~1-2 hr Task 4 Sub-K retirement + ~1 hr close).

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| S1 | Q1 / Q1' = Option (e) — investigation-first; all fix-shape picks deferred to Task 2 mid-batch spec call. | Phase 1 source verification killed the cross-block race hypothesis (S4). Without empirical data on which sub-mechanism is firing, any pre-locked fix repeats the QA-Sfizz Sub-G + Sub-I two-failed-cycles pattern. |
| S2 | Q2 = REJECTED — no global synchronization barrier. | Jeff verbatim: "A global lock is a massive serialization point that will bottleneck my CPU and artificially degrade the DAW's multi-core headroom." |
| S3 | Q2 pivot = DAG + topological sort upgrade — REJECTED post-exploration. | The current dispatcher already implements dep-driven DAG topological execution (verified by direct reads of `RenderGraphDispatcher.cpp` + `VibeThreadPool.cpp` + `RenderTask.h`). `addSyntheticDep` already declares the producer→13-consumer edges at `PluginProcessor.cpp:4142`. There is no DAG to add. |
| S4 | §9 fortieth Candidate A cross-block race hypothesis = dead. | `runUntilOrTimeout` blocks the audio thread until `mAllDone` fires; `mAllDone` is set by MasterTask which runs last after all bus + insert tasks complete. Block N+1's `dispatchBlock` cannot start until block N's `dispatchBlock` returns. No cross-block overlap is possible in the current code. |
| S5 | Q3 (Candidate B implementation shape) = DEFERRED to Task 2 mid-batch spec call. | Jeff at plan-mode entry: "We still need to characterize which sub-mechanism is firing." Without Task 1 trace data, picking between per-instance MPSC queue vs static worker assignment vs other affinity shapes is premature. |
| S6 | Q4 = Option (C) — B.1 folded into DAG verify, re-interpreted post-pivot as: B.1 is **in scope** for the trace investigation alongside B.2/B.3/B.4. | Original Q4(C) reasoning ("DAG dependencies naturally pin a sfizz engine's task chain to whatever worker pulls the producer task") no longer applies (DAG framing dropped). But B.1 stays in the investigation Task — trace data will show whether workers rotate across the sfizz task chain block-to-block. |
| S7 | Sub-K Serial Fallback retirement = conditional on Task 3 fix verify. | If Task 3 cures the 6-cymbal crash MT-on test → Task 4 retires `mAudioThreadOnly` + `audioThreadQueue` + the 4 task-family flag-set sites cleanly. If Task 3 does NOT cure → Sub-K stays as the band-aid; the unresolved scope routes forward via §9 entry + a follow-up batch. |
| S8 | Verify gate = BaySickRustyDrums 6-cymbal crash MT-on test (same test as QA-Sfizz Sub-K). | Identical verify scenario to QA-Sfizz Task 5 follow-up (`0e57fc5`); avoids regression risk on the test substrate; user-verified ground-truth for "bit-crusher present" / "bit-crusher absent". |
| S9 | Trace instrumentation captures entry+exit timestamps + thread IDs on every instrumented task invocation. | Lesson from QA-Sfizz Sub-F=(e) entry-only trace misread (cost Sub-G + Sub-I two failed fix cycles). §9 fortieth verbatim: "future trace instrumentation MUST capture entry+exit timestamps to prove concurrent vs sequential overlap definitively." |
| S10 | Plan-file silly-name = `snug-greeting-quilt` (assigned by plan-mode runtime; running-notes file matches). | Locked at plan-mode entry. |
| S11 | One commit per task: Task 0 (open) + Task 1 (trace instrumentation) + Task 3 (fix) + Task 4 (Sub-K retirement, conditional) + Task 5 (close). Task 2 has no source commit (evaluation + spec call surface). | Standard batch structure per `feedback_commit_at_checkpoints.md`. |
| S12 | Trace instrumentation Disposition = `Remove at Task 4 close` (or `Remove at batch close` if Task 4 is skipped due to non-cure). | Diagnostic addition; not permanent. Rule 4 Diagnostic Instrumentation Catalog entries land in running notes WITH the code change. |

---

## Sub-spec calls surfaced for ExitPlanMode (genuinely deferred to Task 2 mid-batch)

| ID | Question | Disposition |
|----|----------|-------------|
| Sub-A | Task 3 fix shape — pick after Task 1 trace data lands. Possible shapes (not exhaustive): (1) per-engine worker affinity in `VibeThreadPool` (each sfizz engine instance pinned to one worker, replaces `mAudioThreadOnly` all-or-nothing pin); (2) atomic-RR-voice-swap patch in `libs/sfizz/src/sfizz/Voice.{h,cpp}` (B.2); (3) `alignas(64)` cache-line padding audit on hot sfizz state (B.3); (4) sfizz disk-streaming thread-safety audit + optional vendored-library patch (B.4); (5) hybrid / combination if multiple sub-mechanisms fire. | Genuinely deferred to Task 2 spec call per Main Plan §0 Rule 5 plan-mode discipline; trace data dictates which shape applies. |

---

## Files to modify

### Task 0 — Batch open
- [Plans & Specs/Main Plan.md](Plans & Specs/Main Plan.md) — §5 QA-DispatcherAffinity entry: replace `**Plan file:** `<silly-name>.md (when started)`` with backticked-path form; append STATUS banner referencing the post-exploration framing pivot.
- [Plans & Specs/Main Plan.md](Plans & Specs/Main Plan.md) — §9 forty-first Forks entry: document the double pivot (barrier rejection → DAG upgrade → investigation-first reframe) with Jeff's verbatim quotes.
- `~/.claude/plans/snug-greeting-quilt.md` — mirror to `Plans & Specs/Batch Plans/snug-greeting-quilt.md` (Write) + delete home-dir copy per `feedback_plan_mirror_one_way.md`.
- `Plans & Specs/Running Notes/snug-greeting-quilt.md` — seed with title / purpose blockquote / pair ref / convention ref / Task 0 entry.

### Task 1 — Timestamped trace instrumentation
- [Source/Engine/RenderEngineFlags.h](Source/Engine/RenderEngineFlags.h) — add `MtDiagnostic::gTraceTaskTimestamps` atomic flag + fixed-size lock-free ring buffer for trace events (`{ uint64_t entryNs, uint64_t exitNs, int channelId, void* engineInstance, std::thread::id threadId, uint32_t blockIndex }`) + `dumpTraceToFile(juce::File)` function.
- [Source/Engine/Tasks/RustyDrumsProducerTask.cpp:21-64](Source/Engine/Tasks/RustyDrumsProducerTask.cpp:21) — wrap `run()` body entry/exit with timestamp capture gated by `gTraceTaskTimestamps`.
- [Source/Engine/Tasks/RustyInsertTask.cpp:30-96](Source/Engine/Tasks/RustyInsertTask.cpp:30) — wrap `run()` body entry/exit; record `mStripIndex` + `mProcessor` ptr for engine-instance correlation.
- [Source/Engine/Tasks/InstStripTask.cpp:100-151](Source/Engine/Tasks/InstStripTask.cpp:100) — wrap the sfizz-engine branch of `run()` (gated by `mAudioThreadOnly` flag already set in `registerInstEngine`) with entry/exit; record `mEngine` ptr for engine-instance correlation.
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — Mixer hamburger menu entry "QA-DispatcherAffinity Trace" (toggle on/off; off triggers `dumpTraceToFile` to `Documents/BaySickDAW/qa-dispatcheraffinity-trace.log`).

### Task 2 — Trace analysis + spec call (no source touches)
- No source modifications.
- Running notes captures trace findings + Sub-A spec call resolution.

### Task 3 — Implement fix (scope TBD, locked at Task 2 spec call)
- Files locked at Task 2 based on Sub-A pick. Plan body amended after Task 2 with the specific file:line list before Task 3 implementation starts.

### Task 4 — Sub-K Serial Fallback retirement (conditional on Task 3 cure verify)
- [Source/Engine/RenderTask.h:46-60](Source/Engine/RenderTask.h:46) — remove `mAudioThreadOnly` field + the QA-Sfizz Sub-K comment block.
- [Source/Engine/VibeThreadPool.h](Source/Engine/VibeThreadPool.h) — no struct-level changes (Impl struct lives in .cpp).
- [Source/Engine/VibeThreadPool.cpp:20-33](Source/Engine/VibeThreadPool.cpp:20) — remove `audioThreadQueue` declaration + comment block.
- [Source/Engine/VibeThreadPool.cpp:73-85](Source/Engine/VibeThreadPool.cpp:73) — remove `mAudioThreadOnly` routing branch in `submit()`.
- [Source/Engine/VibeThreadPool.cpp:139-148](Source/Engine/VibeThreadPool.cpp:139) — remove audio-queue priority-pop in `runUntil()`.
- [Source/Engine/VibeThreadPool.cpp:167-183](Source/Engine/VibeThreadPool.cpp:167) — remove audio-queue priority-pop in `runUntilOrTimeout()`.
- [Source/Engine/VibeThreadPool.cpp:209-213](Source/Engine/VibeThreadPool.cpp:209) — remove `audioThreadQueue` drain in `clearQueues()`.
- [Source/Engine/Tasks/RustyDrumsProducerTask.cpp:18](Source/Engine/Tasks/RustyDrumsProducerTask.cpp:18) — remove `mAudioThreadOnly = true;` line + Sub-K comment.
- [Source/Engine/Tasks/RustyInsertTask.cpp:27](Source/Engine/Tasks/RustyInsertTask.cpp:27) — remove `mAudioThreadOnly = true;` line + Sub-K comment.
- [Source/PluginProcessor.cpp:3768-3790](Source/PluginProcessor.cpp:3768) — remove the `dynamic_cast<BaySickGuitarsProcessor*>` / `<BaySickBassesProcessor*>` flag-set in `registerInstEngine` + Sub-M=(eng-b) comment.

### Task 5 — Close sequence
- [Plans & Specs/Implemented Work Log.md](Plans & Specs/Implemented Work Log.md) — append batch-close entry after QA-Sfizz close.
- [Plans & Specs/Main Plan.md](Plans & Specs/Main Plan.md) — §5 QA-DispatcherAffinity STATUS banner = CLOSED; §9 routings for any unfolded findings.

---

## Tasks

### Task 0 — Batch open

- [ ] Mirror `~/.claude/plans/snug-greeting-quilt.md` → `Plans & Specs/Batch Plans/snug-greeting-quilt.md` (Write tool); delete home-dir copy.
- [ ] Update Main Plan §5 QA-DispatcherAffinity entry: replace placeholder `**Plan file:**` line with backticked-path form `Plans & Specs/Batch Plans/snug-greeting-quilt.md`. Append a STATUS banner above the `**Plan file:**` line summarizing the post-exploration framing pivot (mirrors QA-Sfizz convention at `Main Plan.md:1316`).
- [ ] Add Main Plan §9 forty-first Forks entry titled `2026-05-28 — QA-DispatcherAffinity plan-mode double pivot: global barrier rejected → DAG upgrade rejected post-exploration → investigation-first sfizz Candidate B reframe`. Capture both Jeff's verbatim quotes (barrier rejection + my-read-is-correct confirmation), the source-verification findings (dispatcher already implements dep-driven DAG), and the new task structure (Task 1 trace / Task 2 spec call / Task 3 fix / Task 4 conditional Sub-K retire).
- [ ] Seed `Plans & Specs/Running Notes/snug-greeting-quilt.md` with title (`# Running Notes — QA-DispatcherAffinity (snug-greeting-quilt)`), purpose blockquote (append-only / checkpoint trigger / close-time consumption), pair reference (`Plans & Specs/Batch Plans/snug-greeting-quilt.md`), convention reference (Main Plan §0:277-282), and initial `## 2026-05-28 — Task 0 — Batch open` entry.
- [ ] Surface FULL `git status` (including the 3 CRLF-residue files) to Jeff. Propose: stage Task-0 doc edits ONLY (Main Plan + Batch Plans + Running Notes); leave the 3 CRLF-residue files untouched.
- [ ] Dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval.
- [ ] On approval: commit via `git commit -F .git/COMMIT_EDITMSG_QA-DispatcherAffinity-task-0.txt` (long-message convention per CLAUDE.md Git Commit Mechanics); `rm` the temp file post-commit.
- [ ] Dispatch `/draft-doc running-notes`; apply to `Plans & Specs/Running Notes/snug-greeting-quilt.md` with the Task 0 entry.

### Task 1 — Timestamped trace instrumentation

- [ ] Read [Source/Engine/RenderEngineFlags.h](Source/Engine/RenderEngineFlags.h) end-to-end to see existing `MtDiagnostic` counter pattern + understand the namespace shape.
- [ ] Read [Source/Engine/Tasks/RustyDrumsProducerTask.cpp](Source/Engine/Tasks/RustyDrumsProducerTask.cpp) + [RustyInsertTask.cpp](Source/Engine/Tasks/RustyInsertTask.cpp) + [InstStripTask.cpp](Source/Engine/Tasks/InstStripTask.cpp) end-to-end to identify the exact `run()` body boundaries + the sfizz-engine branch in InstStripTask.
- [ ] Add to `RenderEngineFlags.h` (inside `RenderEngine::MtDiagnostic` namespace):
  ```cpp
  // QA-DispatcherAffinity (2026-05-28): per-task entry+exit timestamp trace.
  // When gTraceTaskTimestamps is true, instrumented task::run() bodies record
  // entry + exit nanosecond timestamps + thread ID + engine instance pointer
  // into gTraceRing.  Audio-thread-safe via lock-free atomic ring buffer.
  // Drained to a file from the message thread via dumpTraceToFile() when
  // the trace toggle is flipped off.
  struct TraceEvent
  {
      std::uint64_t entryNs       { 0 };
      std::uint64_t exitNs        { 0 };
      int           channelId     { -1 };
      void*         engineInstance { nullptr };
      std::uint32_t threadIdHash  { 0 };  // hashed std::thread::id for compactness
      std::uint32_t blockIndex    { 0 };
  };

  inline std::atomic<bool> gTraceTaskTimestamps { false };

  static constexpr std::size_t kTraceRingCapacity = 65536;
  inline std::array<TraceEvent, kTraceRingCapacity> gTraceRing {};
  inline std::atomic<std::uint64_t> gTraceWriteIndex { 0 };

  void recordTraceEvent (int channelId,
                         void* engineInstance,
                         std::uint64_t entryNs,
                         std::uint64_t exitNs,
                         std::uint32_t blockIndex) noexcept;
  void dumpTraceToFile (const juce::File& target);
  void resetTrace() noexcept;
  ```
  Implementation of `recordTraceEvent` uses `fetch_add(1, relaxed)` on `gTraceWriteIndex`, modulo `kTraceRingCapacity` for slot; thread ID hash = `std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFFFFFF`.
- [ ] Add per-block `gBlockIndex` counter increment in `RenderGraphDispatcher::dispatchBlock` (post-existing `gBlockCount.fetch_add` at line 229) — single 32-bit relaxed atomic, only when trace is on.
- [ ] Wrap `RustyDrumsProducerTask::run()` body with timestamp capture:
  ```cpp
  void RustyDrumsProducerTask::run()
  {
      const bool trace = RenderEngine::MtDiagnostic::gTraceTaskTimestamps.load (std::memory_order_relaxed);
      const std::uint64_t entryNs = trace ? juce::Time::getHighResolutionTicks() : 0;

      // ... existing body unchanged ...

      if (trace)
      {
          const std::uint64_t exitNs = juce::Time::getHighResolutionTicks();
          RenderEngine::MtDiagnostic::recordTraceEvent (
              channelId, mProcessor,
              entryNs, exitNs,
              RenderEngine::MtDiagnostic::gBlockIndex.load (std::memory_order_relaxed));
      }
  }
  ```
- [ ] Same wrap pattern for `RustyInsertTask::run()` — engine instance ptr = `mProcessor`.
- [ ] Same wrap pattern for `InstStripTask::run()` — only inside the sfizz-engine branch (gate on `mAudioThreadOnly` being true OR explicit re-check of engine kind via cached `dynamic_cast` result from constructor); engine instance ptr = `mEngine`.
- [ ] Add Mixer hamburger menu entry "QA-DispatcherAffinity Trace" in `StandaloneEditor`'s mixer hamburger menu builder (read existing menu structure first to identify the right insertion point):
  - Item label: "QA-DispatcherAffinity Trace" with toggle checkmark mirroring "Multi-core Rendering".
  - On toggle ON: `MtDiagnostic::resetTrace()` then `gTraceTaskTimestamps.store(true, release)`.
  - On toggle OFF: `gTraceTaskTimestamps.store(false, release)` then `MtDiagnostic::dumpTraceToFile(juce::File(...))` to `Documents/BaySickDAW/qa-dispatcheraffinity-trace.log` (follows `feedback_follow_existing_folder_convention.md` — Documents root, not a subfolder).
- [ ] Implement `dumpTraceToFile`: open file with `FileOutputStream`, write CSV header row, iterate `gTraceRing[0..min(gTraceWriteIndex, kTraceRingCapacity)]` and write one row per event: `blockIndex,channelId,engineInstance,threadIdHash,entryNs,exitNs,durationNs`. Sort by entryNs before write (analysis ergonomics).
- [ ] Append Diagnostic Instrumentation Catalog rows to `Plans & Specs/Running Notes/snug-greeting-quilt.md` per Main Plan §0 Rule 4 — one row per instrumented site:
  - `RustyDrumsProducerTask::run() entry/exit` / `[QA-DispatcherAffinity TRACE]` / `Entry+exit timestamp + thread ID + engine instance for cross-engine race characterization` / `Remove at Task 4 close (if cure verify passes; else Remove at batch close)`
  - `RustyInsertTask::run() entry/exit` / same tag / same purpose / same disposition
  - `InstStripTask::run() sfizz-engine branch entry/exit` / same tag / same purpose / same disposition
  - `MtDiagnostic::recordTraceEvent + gTraceRing + gTraceWriteIndex + dumpTraceToFile` / same tag / Trace infrastructure / same disposition
  - `Mixer hamburger menu "QA-DispatcherAffinity Trace" toggle` / same tag / Trace runtime toggle / same disposition
- [ ] Tell Jeff: "Run `do_build.bat`. In Debug:
  - **(1)** Open BaySickDAW. Confirm Mixer hamburger menu now shows 'QA-DispatcherAffinity Trace' below 'Multi-core Rendering'.
  - **(2)** Verify Multi-core Rendering is ON (default). Load BaySickRustyDrums kit + the 6-cymbal crash MIDI test pattern from QA-Sfizz Task 5 verify.
  - **(3)** Enable 'QA-DispatcherAffinity Trace' via the menu.
  - **(4)** Play the 6-cymbal pattern for ~6 seconds. Listen for the bit-crusher symptom. (Sub-K Serial Fallback should still be active so the bit-crusher should NOT be audible; trace captures the clean serial-execution baseline.)
  - **(5)** Stop playback. Disable 'QA-DispatcherAffinity Trace'.
  - **(6)** Confirm `Documents\BaySickDAW\qa-dispatcheraffinity-trace.log` exists + has ~1000+ rows (~6s × ~170 blocks/s × 14 tasks).
  - **(7)** Send me the file (or paste the first 200 + last 200 rows).
  - **(8)** Re-test in Release for parity (same steps; same dump file)."
- [ ] Wait for Jeff's verify result + trace file.
- [ ] Cross-check: the dump file should show all 14 instrumented tasks running on `threadIdHash == <audio thread hash>` only (Sub-K Serial Fallback pinning is still active). If anything runs on a worker thread, that's a Sub-K-broken finding routed mid-batch.
- [ ] On verify pass: dispatch `/draft-commit`. Surface drafted message + git status. Commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-DispatcherAffinity-task-1.txt`; `rm` temp file.
- [ ] Dispatch `/draft-doc running-notes`; apply to running-notes file.

### Task 2 — Trace analysis + spec call

- [ ] Read the dump file Jeff provides.
- [ ] **Stage A — Sub-K baseline verification.** Confirm all 14 sfizz tasks run on a single thread ID under Sub-K. If any task ran on a worker → Sub-K is leaking (mid-batch finding routed per Rule 3).
- [ ] **Stage B — disable Sub-K + re-capture trace.** Surface Stage A result to Jeff. Ask Jeff to (i) temporarily comment-out the 4 `mAudioThreadOnly = true` lines (or set them to `false`) OR set a runtime override via a new debug-mode toggle (lighter touch; add as a one-line `if (gSubKOverride) flag = false;` in the constructors); (ii) rebuild Debug; (iii) re-run the 6-cymbal MT-on test with trace enabled; (iv) send the new dump file. This produces the "Sub-K-disabled" trace data that reveals which workers pull which sfizz tasks block-by-block.
- [ ] **Stage C — sub-mechanism characterization.** Analyze the Sub-K-disabled dump:
  - **B.1 indicator** — does the SAME engine instance's task chain consistently run on ONE worker across blocks (no migration)? If YES, B.1 is unlikely / not firing. If workers ROTATE across the same engine's tasks block-to-block → B.1 is a viable hypothesis.
  - **B.2 indicator** — do entry/exit timestamps for tasks on the SAME engine instance OVERLAP (entry of one < exit of another for the same engineInstance pointer)? If YES, multiple workers are concurrently inside the same `sfz::Sfizz` instance → B.2 (non-atomic shared state torn mid-block) is a strong hypothesis.
  - **B.3 indicator** — variance in per-task duration. Bursts of slow runs (>10x median duration) co-occurring with fast runs on a sibling worker → cache-line invalidation suggestive. Harder to isolate from trace alone; alignas(64) audit on hot sfizz state may need follow-up profiling.
  - **B.4 indicator** — duration variance correlating with which sample is being streamed. Hard to read from trace alone without correlating with sample IDs (which the trace doesn't capture today; flag as a potential Task 2.5 expansion if B.4 is the leading hypothesis).
- [ ] Surface Sub-A spec call to Jeff with the characterized firing sub-mechanism + 1-3 recommended fix shapes (NOT a pre-pick — concrete options per Rule 5):
  - **If B.1 fires:** options likely include per-engine worker affinity in `VibeThreadPool` (each engine pinned to a fixed worker for its lifetime — gentler than Sub-K's all-or-nothing audio-thread pin); OR engine-instance-keyed work-stealing predicate that rejects steals across engine boundaries.
  - **If B.2 fires:** options likely include atomic-RR-voice-swap patch in `libs/sfizz/src/sfizz/Voice.{h,cpp}` (vendored library patch under §9 forty-first Forks scope-creep guard); OR audio-thread-pin retention (Sub-K stays as the long-term solution for B.2 specifically).
  - **If B.3 fires:** options likely include `alignas(64)` padding audit on hot sfizz state (RR counter, voice metadata pointers) via vendored sfizz patch.
  - **If B.4 fires:** options likely include disk-streaming thread-safety audit + sfizz upstream PR if a worker-affinity fix isn't sufficient.
  - **If MULTIPLE sub-mechanisms fire:** combined intervention scope; surface combined cost-benefit to Jeff.
  - **If NO clear sub-mechanism fires (trace inconclusive):** route forward — Sub-K stays as the band-aid; a follow-up batch with deeper instrumentation (sfizz internal probes) or DSP-output diff capture takes over.
- [ ] Wait for Jeff's Sub-A pick.
- [ ] On pick: amend Task 3's `Files to modify` section above with the specific files + line ranges that Sub-A locks. Dispatch `/draft-doc running-notes`; apply to running-notes file with the Task 2 analysis + Sub-A resolution.

### Task 3 — Implement fix (scope locked at Task 2 Sub-A spec call)

- [ ] Implement the Sub-A-locked fix per amended Files to modify list.
- [ ] If the fix touches the vendored sfizz subtree (`libs/sfizz/`), explicitly call out the §9 forty-first Forks scope-creep guard at amendment time + confirm with Jeff before any vendored-library source edit (mirrors QA-SfzGroup Sub-R/S + QA-Sfizz Sub-D vendored-patch precedents).
- [ ] Tell Jeff: "Run `do_build.bat`. In Debug:
  - **(1)** Confirm Sub-K Serial Fallback is still active (the fix lands ALONGSIDE Sub-K — retirement is Task 4 conditional on cure verify).
  - **(2)** Re-enable 'QA-DispatcherAffinity Trace'. Disable Sub-K via the override mechanism added at Task 2 Stage B.
  - **(3)** Play the 6-cymbal pattern. Listen for the bit-crusher.
  - **(4)** Stop. Re-capture trace dump.
  - **(5)** Verify result: bit-crusher ABSENT → fix candidate cures. Bit-crusher PRESENT → fix did not cure (rare; route per Routing notes below).
  - **(6)** Re-test in Release for parity."
- [ ] Wait for Jeff's verify result.
- [ ] On cure verify pass: dispatch `/draft-commit`. Surface drafted message + git status. Commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-DispatcherAffinity-task-3.txt`; `rm` temp file.
- [ ] On cure verify FAIL: surface options to Jeff per Routing notes (route forward with Sub-K-stays + dedicated follow-up batch; re-enter Task 2 with refined trace data; etc.). Don't auto-retire Sub-K.
- [ ] Dispatch `/draft-doc running-notes`; apply to running-notes file.

### Task 4 — Sub-K Serial Fallback retirement (conditional on Task 3 cure verify)

- [ ] **Gate check:** Task 3 cure verify passed? If NO → skip Task 4 entirely; Sub-K stays in production; jump to Task 5 close sequence + route Sub-K retention via §9 entry.
- [ ] **If YES — clean retirement (mirrors the source-reverse of QA-Sfizz Task 5 follow-up commit `0e57fc5`):**
  - [ ] Remove `bool mAudioThreadOnly = false;` field + the QA-Sfizz Sub-K comment block in [Source/Engine/RenderTask.h:46-60](Source/Engine/RenderTask.h:46).
  - [ ] Remove `audioThreadQueue` declaration + comment block in [Source/Engine/VibeThreadPool.cpp:20-33](Source/Engine/VibeThreadPool.cpp:20).
  - [ ] Restore `submit()` to single-queue path: remove the `if (task->mAudioThreadOnly) { mImpl->audioThreadQueue.enqueue(task); return; }` branch in [VibeThreadPool.cpp:73-85](Source/Engine/VibeThreadPool.cpp:73).
  - [ ] Remove audio-queue priority-pop in `runUntil()` at [VibeThreadPool.cpp:139-148](Source/Engine/VibeThreadPool.cpp:139).
  - [ ] Remove audio-queue priority-pop in `runUntilOrTimeout()` at [VibeThreadPool.cpp:167-183](Source/Engine/VibeThreadPool.cpp:167).
  - [ ] Remove `audioThreadQueue` drain in `clearQueues()` at [VibeThreadPool.cpp:209-213](Source/Engine/VibeThreadPool.cpp:209).
  - [ ] Remove `mAudioThreadOnly = true;` line + Sub-K comment in [RustyDrumsProducerTask.cpp:18](Source/Engine/Tasks/RustyDrumsProducerTask.cpp:18).
  - [ ] Remove `mAudioThreadOnly = true;` line + Sub-K comment in [RustyInsertTask.cpp:27](Source/Engine/Tasks/RustyInsertTask.cpp:27).
  - [ ] Remove the `dynamic_cast<BaySickGuitarsProcessor*>` / `<BaySickBassesProcessor*>` flag-set in `registerInstEngine` + Sub-M=(eng-b) comment block at [PluginProcessor.cpp:3768-3790](Source/PluginProcessor.cpp:3768).
  - [ ] Strip the Task 2 Stage B override mechanism (debug toggle) if one was added.
  - [ ] Strip Task 1 trace instrumentation per Rule 4 catalog Disposition (`Remove at Task 4 close`):
    - Revert [RustyDrumsProducerTask.cpp](Source/Engine/Tasks/RustyDrumsProducerTask.cpp) `run()` entry/exit wrap.
    - Revert [RustyInsertTask.cpp](Source/Engine/Tasks/RustyInsertTask.cpp) `run()` entry/exit wrap.
    - Revert [InstStripTask.cpp](Source/Engine/Tasks/InstStripTask.cpp) sfizz-engine-branch wrap.
    - Remove `gTraceTaskTimestamps` + `gTraceRing` + `gTraceWriteIndex` + `recordTraceEvent` + `dumpTraceToFile` + `resetTrace` from [RenderEngineFlags.h](Source/Engine/RenderEngineFlags.h).
    - Remove `gBlockIndex` increment from [RenderGraphDispatcher.cpp](Source/Engine/RenderGraphDispatcher.cpp).
    - Remove "QA-DispatcherAffinity Trace" menu entry from [StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) Mixer hamburger.
  - [ ] Surface the strip list to Jeff for approval BEFORE running the strip pass (per Main Plan §0 Rule 4).
- [ ] Tell Jeff: "Run `do_build.bat`. In Debug:
  - **(1)** Re-run the 6-cymbal crash MT-on test. Verify bit-crusher ABSENT (Task 3 fix + Sub-K retired = clean MT execution).
  - **(2)** Re-run BaySickGuitars + BaySickBasses tests with sfizz-driven kits to confirm no regression (Sub-K had defensive pinning on these engines too; retirement removes that defense — the Task 3 fix must cover them as well).
  - **(3)** Re-run BaySickSolstice / BaySickSynth / BaySickPlayer tests to confirm no regression on non-sfizz engines.
  - **(4)** Re-test in Release."
- [ ] Wait for Jeff's verify result.
- [ ] On verify pass: dispatch `/draft-commit`. Surface drafted message + git status. Commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-DispatcherAffinity-task-4.txt`; `rm` temp file.
- [ ] On verify FAIL: investigate per finding (Task 3 fix may not cover the BaySickGuitars/Basses surface even if Rusty is clean); surface to Jeff before any rollback. Worst case: revert Task 4 + leave Sub-K on the defensive-pinning engines only.
- [ ] Dispatch `/draft-doc running-notes`; apply to running-notes file.

### Task 5 — Close sequence

- [ ] Dispatch `/draft-doc batch-close` with a synthesis of the running-notes file.
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit (append after QA-Sfizz close entry).
- [ ] Apply the §5 QA-DispatcherAffinity STATUS banner update (CLOSED).
- [ ] Dispatch `/review-batch QA-DispatcherAffinity`.
- [ ] Address BLOCKERs / NEEDS-FIX in-batch. Defer NITs into close-entry routing table.
- [ ] Route side findings per Rule 3 — any unresolved Candidate B sub-mechanisms / Sub-K retention / vendored sfizz scope expansion routes via §9 + targeted §5 / Future State updates. Surface placement options to Jeff; don't pick the slot.
- [ ] Surface full `git status`.
- [ ] Dispatch `/draft-commit` for close commit. Surface message + status. Commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-DispatcherAffinity-task-5-close.txt`; `rm` temp file.

---

## Verification (end-to-end smoke)

After Task 4 commit lands (or Task 3 commit if Task 4 is skipped):

1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **BaySickRustyDrums 6-cymbal crash MT-on test.** Bit-crusher absent (the primary verify gate).
3. **BaySickGuitars MT-on test** with the Aria-host kit that previously triggered RR loss (QA-Sfizz Item 2 Branch A baseline). No regression on RR behavior (kit-content limitation persists per QA-Sfizz close; not a QA-DispatcherAffinity scope).
4. **BaySickBasses MT-on test** with the same kit. No regression.
5. **BaySickSolstice / BaySickSynth / BaySickPlayer MT-on test.** No CPU regression vs pre-batch baseline (Sub-K retirement returns more tasks to the worker pool; net-positive on CPU expected).
6. **MT-off test (Multi-core Rendering disabled via Mixer hamburger).** All 4 sfizz engines clean (Sub-K was a no-op when MT was off; serial-diagnostic mode unchanged).
7. **Trace instrumentation fully stripped** (confirmed via `grep` for `gTraceTaskTimestamps` / `recordTraceEvent` / `[QA-DispatcherAffinity TRACE]` returning zero matches in `Source/`).
8. **No `mAudioThreadOnly` references** in `Source/` (confirmed via `grep`).

---

## Routing notes (Rule 3 application during execution)

- **Task 1 trace finding: Sub-K leaking** (any sfizz task ran on a worker thread despite the `mAudioThreadOnly` flag) → fold a Sub-K leak fix into Task 1 commit + continue per the original plan (Sub-K must hold its line until Task 4 retires it cleanly).
- **Task 2 finding: trace data shows NO sub-mechanism firing under Sub-K-disabled mode** (i.e. the bit-crusher reproduces but the trace doesn't isolate cause) → route forward; Sub-K stays as the band-aid; a new dedicated batch (working name `QA-SfizzInternalProbes`) takes over with deeper sfizz-internal instrumentation. Surface placement to Jeff per `feedback_slot_placement_is_spec_call.md`.
- **Task 3 finding: fix candidate touches vendored sfizz subtree** → vendored-library scope-creep guard applies. Confirm with Jeff before any source edit under `libs/sfizz/` (mirrors QA-SfzGroup Sub-R/S + QA-Sfizz Sub-D precedents).
- **Task 3 cure verify FAIL** → don't auto-retire Sub-K; surface options to Jeff (route to follow-up batch; re-enter Task 2 with refined trace; etc.).
- **Task 4 retirement breaks BaySickGuitars/Basses** (the defensive pinning was the only thing keeping them clean) → revert Task 4; leave Sub-K on the defensive-pinning engines only; route the remaining work via §9 + a follow-up batch.
- **QA-D NIT carry-forward parity:** if `/review-batch` returns NITs at close, address per-finding in-batch unless a NIT is genuinely Phase-6-cleanup-shaped — never bulk-defer (`feedback_qa_batches_fix_bugs_dont_defer.md` + `feedback_closed_batch_carryforward_via_forks.md`).

---

## Carry-Forward Reference touch points

- **§1 Render Engine Primitives** — read at Task 1 start to confirm the MT path documentation matches the current code (Phase 1 exploration verified it does; quick re-verify).
- **§2 Lock-Free + Lifecycle Primitives** — read at Task 1 start; `AudioClipSnapshot` RCU + `RetirementQueue` patterns inform the trace ring buffer design (lock-free, message-thread-drain).

---

## MT-awareness static-analysis

The trace instrumentation operates entirely on the audio thread (entry+exit timestamp captures inside `run()` bodies). The ring buffer write is lock-free (`fetch_add` on `gTraceWriteIndex`; modulo-indexed slot write with relaxed ordering). The dump function runs on the message thread after the trace toggle flips off — at that point `gTraceTaskTimestamps == false` so audio-thread writes have ceased; the message thread reads the ring buffer without coordination with any ongoing audio writes. No new audio-thread allocation; no new audio-thread lock. Sub-K retirement is a pure removal — the prior `mAudioThreadOnly` routing branch returns to single-queue behavior; the acq_rel memory ordering on `mDeps` decrements is unchanged.

---

## Estimated commit count

5 source-touching commits (Task 0 / Task 1 / Task 3 / Task 4 conditional / Task 5 close) + 1 close commit = up to 6 total. Lower bound 4 if Task 3 verify fails (Task 4 skipped + close commit packages the Sub-K-retention routing).
