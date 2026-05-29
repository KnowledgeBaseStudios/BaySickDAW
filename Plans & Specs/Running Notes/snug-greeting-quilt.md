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

**Task 1 commit:** landed at `c81aff4` (2026-05-29).  `git commit -F .git/COMMIT_EDITMSG_QA-DispatcherAffinity-task-1.txt`; temp file `rm`'d post-commit.  Staged scope = 8 files (7 source + 1 doc; +331 / -2).  /draft-commit drafted body surfaced verbatim for approval; one factual-error fix applied pre-commit (drafter hallucinated "matching the existing gWorkerStartTimes ring's discipline" — grepped + confirmed gWorkerStartTimes doesn't exist in tree; dropped the false clause + corrected `& 0xFFFF` → `& (capacity-1)` for accuracy).  Per `feedback_drafter_output_verbatim_no_restyle.md` review = factual/scope errors only; the surfaced correction qualifies.  Branch now 33 commits ahead of `origin/main`.

**Build hand-off:** verify script in plan Task 1 step 11 (8 numbered scenarios).  Verify PASSED both configs.

---

## 2026-05-29 — Task 1 — Stage A baseline verify PASS + commit landed

Trace dump statistics (Release: 22,596 events / 1,615 blocks; Debug: 25,694 events / 3,527 blocks): single thread ID per config (0x4b855ce3 Release / 0xfab1ec5b Debug), single engine instance pointer per config (0x28155636b70 Release / 0x2e1dc7154f0 Debug), 14 unique channelIds (-1 producer + 800..812 inserts) — exact match to the expected 14-task Rusty topology, ZERO within-block overlap events across 20,982 Release + 22,168 Debug transitions — Sub-K Serial Fallback confirmed enforcing serial execution across the 14-task sfizz family.  Producer (sfizz `renderBlock`) avg 321 µs Release / 1,124 µs Debug; InsertTask avg 0.9-2.1 µs Release / ~5 µs Debug; inter-task gap avg 255 ns Release / 765 ns Debug.  Debug/Release event-count ratio 1.14 reflects Debug's slower per-block throughput.

**Next:** Task 2 Stage B — disable Sub-K Serial Fallback + re-capture trace under MT-on parallel execution to characterize which Candidate B sub-mechanism (B.1/B.2/B.3/B.4) is firing.  Stage B implementation shape (hard comment-out vs runtime toggle) is a sub-spec call to surface to Jeff before any source touch.

---

## 2026-05-29 — Task 2 Stage B — Sub-K runtime override mechanism landed

**Sub-spec call resolved:** Stage B implementation shape = Option (b) runtime override toggle (Jeff verbatim "real-time A/B test in real-time within the exact same session state is invaluable.  Rebuilding and restarting the DAW introduces too many variables and ruins the rapid feedback loop").  No-rebuild same-session A/B between Sub-K-on (production) and Sub-K-off (Stage B trace).

**Design choice surfaced before edit:** Override gate lives in `VibeThreadPool::submit()` (one-line) rather than at the 3 construction sites Jeff named.  Reason: tasks are constructed once on the message thread (at kit-load / engine-swap); if I gated at construction, the override would only affect FUTURE tasks — runtime toggling would not affect already-built tasks.  Submit-time gate honors Jeff's "real-time" requirement by reading the override on every dispatch.  The 3 sites Jeff named get **comment updates** pointing at the new mechanism (Rusty producer ctor / Rusty insert ctor / `registerInstEngine` Sub-M=(eng-b) tag) so future readers understand the indirection; they keep their `mAudioThreadOnly = true` tagging as the "wants pinning" intent and the override decides per-submit whether to honor it.

**Files touched (Task 2 Stage B):**
- `Source/Engine/RenderEngineFlags.h` — added `MtDiagnostic::gSubKOverride` atomic<bool> {false} alongside the trace infrastructure (+19 lines including the multi-line comment block explaining the override semantics).  Default false = Sub-K active (production); true = override engaged (Stage B test).
- `Source/Engine/VibeThreadPool.cpp` — `submit()` now reads `gSubKOverride` with relaxed ordering and routes to `audioThreadQueue` only when `mAudioThreadOnly && !gSubKOverride`.  One-line semantic change + a multi-line comment block documenting the QA-DispatcherAffinity Task 2 Stage B annotation.
- `Source/Engine/Tasks/RustyDrumsProducerTask.cpp` — comment update at the Sub-K Serial Fallback constructor block; points at the override mechanism + Rust references.
- `Source/Engine/Tasks/RustyInsertTask.cpp` — same comment-only update at the inserts constructor.
- `Source/PluginProcessor.cpp` — comment update at the Sub-M=(eng-b) `registerInstEngine` `dynamic_cast` flag-set block; points at the override mechanism.
- `Source/Standalone/StandaloneEditor.cpp` — Mixer hamburger menu item 205 "Sub-K Serial Fallback" (checkable; checked = Sub-K active = production state) + handler that flips `gSubKOverride` + AlertWindow surfacing the new state with a Stage B trace usage hint.

**Stage B workflow:**
1. Enable QA-DispatcherAffinity Trace via menu item 204.
2. Disable "Sub-K Serial Fallback" via menu item 205.  Expected: AlertWindow says "OVERRIDE engaged".  At this point the 3 sfizz task families return to MT parallel execution.
3. Play the 6-cymbal pattern.  Expected: bit-crusher distortion on long cymbals/hi-hats (the QA-Sfizz Item 3 symptom the Sub-K band-aid bypassed).
4. Stop playback.  Disable trace via menu item 204.  Confirm trace dump file lands; send to Claude for Stage C characterization.
5. Re-enable "Sub-K Serial Fallback" via menu item 205.  Production state restored.

**Task 2 Stage B commit:** TBD (after Jeff's Debug + Release verify pass; trace dump sent for Stage C analysis).

**Follow-up fix folded into the Stage B mechanism commit:** dump function null-byte bug.  JUCE's `FileOutputStream::writeString` appends a null terminator after each write, polluting the CSV (every row got a leading null byte from the previous row's terminator + the header's trailing null made some tools — including bash `grep` — treat the file as binary).  Symptom: `awk -F, '$1+0 == 100'` returns nothing on the Stage B dumps until you preface with `tr -d '\000'`.  Fix: switch to raw-byte `fos.write(text.toRawUTF8(), text.getNumBytesAsUTF8())` for each row + a literal `const char*` write for the header.  Surfaced + fixed in the StandaloneEditor menu handler that does the dump (single 4-line change).  Stage A's Sub-K-baseline traces (Release 22596 + Debug 25694 events) were captured with the buggy dump but still analyzable via the same `tr -d '\000'` workaround; Stage C analysis below uses the fixed-parse approach.

---

## 2026-05-29 — Task 2 Stage C — Sub-K-disabled trace analysis + Sub-A spec call surface

**Stage C trace dumps received from Jeff (Sub-K override engaged for the entirety of each capture):**

| Metric | Release | Debug |
|---|---|---|
| Total events | 38,570 | 42,127 |
| Total blocks captured | 2,755 | 3,009 |
| Unique worker threads | **9** | **9** |
| Unique engine instances | 1 (0x1ab47d75020) | 1 (0x19ddca54bb0) |
| Cross-thread overlapping intervals (same block) | **26,912** | **42,683** |
| Producer-vs-insert overlap (cross-thread) | **0** | **0** |
| Producer (-1) avg duration | 512 µs (vs 321 µs Stage A baseline; 1.6× slower) | 6,102 µs (vs 1,124 µs Stage A; 5.4× slower) |
| Insert avg duration | 0.7-2.4 µs (similar to Stage A) | 1.9-13.6 µs (slower than Stage A's 5 µs) |
| Insert zero-duration events (try-lock failures) | many (varies per channel) | 4-84 per channel |

**Stage C key findings:**

1. **9 unique worker threads spread across all 14 sfizz task channelIds** — Sub-K override is engaged + the MT worker pool is actively executing the previously-pinned tasks.  Sub-K mechanism confirmed working at runtime via Mixer menu toggle.

2. **Producer-vs-insert cross-thread overlap = 0** — the dispatcher's synthetic dep `addSyntheticDep(producer, insert)` at `PluginProcessor.cpp:4142` is honored.  No race between producer's `processStrips` write to `mMultiOutScratch` and inserts' reads via `getStripBuffer`.  Validates the §9 fortieth Forks entry's Pivot #2 finding (dispatcher already implements dep-driven DAG topological execution).

3. **Within-block cross-thread overlap = 26,912 Release / 42,683 Debug** — multiple worker threads concurrently inside the SAME engine instance running InsertTask::run() bodies in overlapping time windows.  ALL overlaps are insert-vs-insert (not producer-vs-insert).

4. **The bit-crusher mechanism (NEW — not in QA-Sfizz §9 fortieth B.1/B.2/B.3/B.4 catalog):** the per-insert engine spin lock `mRustyDrumsEngineLock` causes try-lock failures under MT contention.  Each RustyInsertTask::run() does:
   ```cpp
   juce::SpinLock::ScopedTryLockType lk (mProcessor->mRustyDrumsEngineLock);
   if (! lk.isLocked()) { mOutputBuffer->clear(); return; }   // strip silences!
   ```
   Under MT execution, 13 InsertTasks dispatch concurrently across workers; they all compete for the engine spin lock; whoever wins the try-lock executes; the losers clear their output buffer (silence) and return.  The trace shows this directly: most inserts have normal 1-4 µs durations (successful try-lock + copy), but a noticeable population has sub-µs / 0 ns durations (try-lock FAIL → clear + return).  Over a ~6-second cymbal-crash test, intermittent silencing of different strips per block produces the audible "bit-crusher / gritty / digital" distortion the QA-Sfizz Item 3 symptom describes.  Call it **B.5 — try-lock-failure strip silencing** for catalog parity.

5. **Producer slowdown** — Producer duration grows 1.6× Release / 5.4× Debug under MT-on.  Secondary effect on top of the strip-silencing.  Likely B.3 (false sharing on cache lines containing engine state shared with concurrent insert readers) OR thread-affinity migration cost.  Not the primary symptom but real.

**Sub-A fix shape options for Jeff (per Rule 5 plan-mode discipline — no pre-pick):**

| ID | Approach | Pros | Cons |
|----|----------|------|------|
| (i) | **Remove the spin lock from InsertTasks entirely.**  After the producer finishes (synthetic dep), `mMultiOutScratch` is stable for the block's duration; engine swap only happens at kit-load on the message thread, which is already gated by `mProjectLoadInProgress` barrier.  Audit the kit-load path to confirm no concurrent-read window with InsertTasks.  Inserts proceed lock-free. | Eliminates lock contention + try-lock drops entirely.  Full parallelism on InsertTasks (13 concurrent strip copies).  Best per-block CPU efficiency. | Risks reintroducing a kit-load race if the audit misses something.  Requires careful review of every `mRustyDrumsEngine.reset(...)` / `swap(...)` call site to confirm gating. |
| (ii) | **Replace `ScopedTryLockType` with blocking `ScopedLockType`.**  Each InsertTask blocks until it can acquire the engine lock.  No drops.  Total work for the 13 inserts serializes via the lock (~25 µs per block worst case in Release; ~100 µs Debug). | Smallest code change (`Try` → `` removal at 14 sites).  Zero risk of race (lock provides identical safety to before).  Audio drops eliminated. | Effective behavior equivalent to Sub-K (serialized) but with extra spin-lock overhead vs Sub-K's queue-pop overhead.  Marginal CPU win over Sub-K; not "true MT parallelism for sfizz". |
| (iii) | **RCU pattern on the engine pointer.**  `std::atomic<BaySickRustyDrumsProcessor*> mActiveRustyEngine` with acquire-load on every read; kit-load atomically `exchange()` + retires old via `RetirementQueue<BaySickRustyDrumsProcessor>`.  Inserts read the pointer lock-free + use it for the duration of the block (RCU snapshot).  Matches the existing `AudioClipSnapshot` + `RetirementQueue` patterns from QA-9c. | Cleanest "real solution" — proper lock-free + safe kit-load.  Reusable pattern (could also apply to BaySickGuitars / BaySickBasses).  Full MT parallelism. | Largest code surface (new RetirementQueue<T> instantiation + every engine access site touched + kit-load path refactor).  Risk medium — touches the engine lifecycle code. |
| (iv) | **Accept Sub-K as permanent for sfizz engines.**  Document that the vendored sfizz library's per-instance lock model doesn't benefit from MT execution at the BaySickDAW dispatcher level (because lock contention serializes anyway), so Sub-K Serial Fallback stays permanent for the 3 sfizz engine families.  Task 4 retirement of `mAudioThreadOnly` is SKIPPED. | Zero code change.  Honest description of the architectural reality the trace data revealed. | Doesn't actually fix anything — kicks the can on "real MT parallelism for sfizz".  Future work (e.g. multi-instance Rusty kits, BaySickGuitars + BaySickBasses tabs concurrent) won't benefit. |

**Recommendation deferred per Rule 5 — Jeff picks (i) / (ii) / (iii) / (iv) / hybrid.**

**Sub-A resolution (Jeff 2026-05-29): Option (i) — Remove the spin lock from InsertTasks entirely.**

Jeff verbatim: "This trace data is the exact reason we ran the investigation first.  Incredible catch on the spin-lock contention (B.5).  The 'try-lock lottery' causing rapid-fire buffer clearing explains the bit-crusher artifact perfectly.  We are going with Option (i) Remove the spin lock from InsertTasks entirely.  Because the DAG already guarantees the ProducerTask has finished writing to mMultiOutScratch before the 13 InsertTasks are dispatched, the inserts are strictly concurrent readers.  Multiple threads reading a static buffer is perfectly safe.  We do not need, and should not have, an audio-thread lock here.  For lifecycle safety (like engine swapping or kit loading), we will rely on the existing mProjectLoadInProgress barrier on the message thread.  This gives us the maximum multi-core parallelism the engine was designed for."

**Task 3 scope (locked by Sub-A = (i)):**
- Remove the `juce::SpinLock::ScopedTryLockType lk (mProcessor->mRustyDrumsEngineLock)` block from `RustyInsertTask::run()` ([Source/Engine/Tasks/RustyInsertTask.cpp:50-55](Source/Engine/Tasks/RustyInsertTask.cpp:50)) plus the related early-return-on-fail (`if (! lk.isLocked()) { mOutputBuffer->clear(); return; }`).  The 13 inserts proceed as strict concurrent readers of `mMultiOutScratch` post-producer-finish.
- **Audit kit-load + engine-swap paths** to confirm the `mProjectLoadInProgress` barrier (or equivalent message-thread gate) keeps inserts from running concurrently with a `mRustyDrumsEngine.reset(...)` / engine swap.  Identify every site that mutates `mProcessor->mRustyDrumsEngine`; confirm each is gated by a barrier that holds the audio thread out of `run()` while the swap is in flight.
- Re-test under MT-on with Sub-K disabled: bit-crusher should be absent (no try-lock failures since the lock is gone); all 13 inserts complete their copies in parallel.
- If Task 3 cure verifies → Task 4 cleanly retires the entire Sub-K Serial Fallback infrastructure (mAudioThreadOnly flag + audioThreadQueue MPSC + 4 task-family flag-set sites + gSubKOverride debug flag + Mixer menu item 205 + trace infrastructure per Rule 4 catalog Disposition).

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
| `Source/Engine/RenderEngineFlags.h` MtDiagnostic namespace — `gSubKOverride` atomic<bool> | `QA-DispatcherAffinity Task 2 Stage B (2026-05-29)` | Runtime override for Sub-K Serial Fallback mAudioThreadOnly pinning (Stage B re-capture support) | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
| `Source/Engine/VibeThreadPool.cpp` `submit()` — `&& !gSubKOverride.load(relaxed)` gate on the audioThreadQueue route | `QA-DispatcherAffinity Task 2 Stage B (2026-05-29)` | Runtime gate that lets `gSubKOverride` toggle which queue Sub-K-tagged tasks land in | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
| `Source/Standalone/StandaloneEditor.cpp` ~line 5188 — Mixer hamburger menu item 205 "Sub-K Serial Fallback" + handler | `QA-DispatcherAffinity Task 2 Stage B (2026-05-29)` | Runtime toggle for Sub-K override + AlertWindow surface with Stage B usage hint | Remove at Task 4 close (if Task 3 cure verify passes; else Remove at batch close) |
