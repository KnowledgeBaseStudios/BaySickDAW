# Running Notes — QA-N (honest-summing-falcon)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/honest-summing-falcon.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval. Scout truth in the plan: no per-worker accounting exists;
meter cap is live 10.0 w/ HOLD-FOR-Phase-6 comment (marathon 12d's 200% applies at Phase 6,
NOT here — cap untouched this batch). Coding starts after QA-M.

## 2026-07-18 — Task 1 — Per-worker busy accounting

VibeThreadPool gains a per-block busy-tick accumulator (alignas(64)
std::atomic<juce::int64> mBusyTicks — cache-line isolated so 8 concurrent worker
fetch_adds never false-share mShutdown/mNumWorkers) + inline resetBusyTicks() /
getBusyTicks() (relaxed). Accumulation lives in runOneTask — the SINGLE funnel both
workerLoop AND the audio thread's runUntilOrTimeout pump call, so every executor's
per-task wall-clock lands in one place (no separate worker-vs-main plumbing). Gated on
gMultiThreadedEngineEnabled (relaxed load): MT on -> wrap task->run() with hi-res tick
delta + fetch_add; MT off (workers parked, serial-diagnostic) -> plain task->run(), the
audio thread's own processBlock wall-clock is already the total so ticks would be pure
overhead = zero cost when MT off (plan contract). RenderEngineFlags.h already included
(MtDiagnostic uses it). Dispatcher resets the accumulator at the block boundary
(dispatchBlock, right before the dep-counter reset loop) so measureDspLoadAndOverload
reads a clean per-block sum after dispatch returns. Contention is ~17-50 fetch_adds/block
across 8 cores (not a per-sample loop) — negligible. No spec calls, no findings, no
diagnostics added (mBusyTicks is measurement, wired to the meter in Task 2 — not a
gCaptureOn diagnostic counter).

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — Task 2 — Meter consumes the sum

measureDspLoadAndOverload (PluginProcessor.cpp:3462) restructured: the up-front
t1/elapsed pair replaced with a single branched `workSeconds` — MT
(gMultiThreadedEngineEnabled acquire-load) reads mRenderPool.getBusyTicks()/ticksPerSec
(total task work across pump + workers); ST reads (getHighResolutionTicks() -
t0Ticks)/ticksPerSec (audio-thread wall-clock, byte-identical to pre-batch — workers
parked = audio thread runs the whole graph serially so wall-clock IS the total). rawLoad
now divides workSeconds/bufDur; EVERYTHING downstream untouched — 0..10 measurement cap,
0.85/0.15 smoothing, mAudioDspLoad atomic, 95%/85% overload thresholds, voice-steal.
HOLD-FOR-Phase-6 cap comment (2026-05-09 QA-Md) left verbatim between the bufDur + rawLoad
lines (marathon 12d owns the release value at QA-Audit — untouched here). Rule 6 comment
updates: function header (:3455 — the old "reads lower than a single-threaded run" note)
+ processBlock call site (:2876 — the old "measures audio-thread wall-clock; reads lower"
note) both rewritten to the sum-of-cores behavior. GlobalTransportBar display unchanged
(reads the same mAudioDspLoad atomic + its own 0..999 clamp). No spec calls, no findings,
no diagnostics added.

Build-confirm: Jeff "clean" (Release+Debug), 2026-07-18.

## 2026-07-18 — QA-N CODE-COMPLETE

Tasks 1-2 shipped, both build gates clean. No spec calls surfaced (measurement-only,
no branch points). §B.20 authored (3 scenarios; `blocks:` hash backfills at the next
docs commit per precedent). Work Log entry drafted + HELD below. ONE batch commit
surfaced for approval (carries the QA-M doc straggler: the B.19 hash backfill).

## Held Work Log entry

> HELD per bulk-run R2 — apply to `Implemented Work Log.md` verbatim when §B.20 passes.

### 2026-07-18 23:45 PT — QA-N — DSP meter sum-of-cores (DIAG-02): meter reports total parallel render work under MT

**Bucket:** Cross-cutting Infrastructure

#### Done

- **Task 1 — per-worker busy accounting:** `VibeThreadPool` gains a per-block busy-tick
  accumulator (`alignas(64) std::atomic<juce::int64> mBusyTicks`, cache-line isolated so
  concurrent worker `fetch_add`s never false-share the pool's other members) with inline
  `resetBusyTicks()` / `getBusyTicks()` (relaxed). Accumulation lives in `runOneTask` —
  the single funnel that both `workerLoop` and the audio thread's `runUntilOrTimeout`
  pump call — so every executor's per-task wall-clock is summed in one place. Gated on
  `gMultiThreadedEngineEnabled`: MT on wraps `task->run()` with a hi-res tick delta; MT
  off (workers parked) runs plain, since the audio thread's own `processBlock`
  wall-clock is already the total — zero cost when MT is off. The dispatcher resets the
  accumulator at the block boundary (in `dispatchBlock`, alongside the dep-counter
  reset).
- **Task 2 — meter consumes the sum:** `measureDspLoadAndOverload` branches — MT path
  divides the pool's summed busy ticks by the buffer duration (total render work as "%
  of one core"); ST path keeps the audio-thread wall-clock measure, byte-identical to
  pre-batch (workers parked → the audio thread runs the whole graph serially, so its
  wall-clock is the total). Everything downstream is untouched: the 1000% measurement
  cap, the 0.85/0.15 exponential smoothing, the `mAudioDspLoad` atomic, the 95%/85%
  overload thresholds, and the sustained-overload voice-steal. The HOLD-FOR-Phase-6 cap
  comment stays (marathon 12d — the 200% V1 release cap lands at Phase 6 / QA-Audit,
  not here). Rule 6 comment refresh at the function header + the processBlock call site
  (the old "reads lower under MT" notes). GlobalTransportBar display is unchanged (same
  atomic, its own 0..999 clamp).

#### Found along the way

Nothing routed out — measurement-only change, no dispatcher hot spots surfaced (the
plan's Rule-3 note reserved /perf-audit for that; not triggered). Plan's line refs
(:3164-3185 / :125) were stale post-QA-G/H; re-located (`measureDspLoadAndOverload`
:3462, dispatch reset at the dep-counter loop).

#### Known seams (campaign-visible)

The MT meter now excludes the audio thread's NON-task overhead (MIDI dispatch, choke,
metronome, the measure itself) — by design it reports total DSP TASK work, the honest
sum-of-cores number §5 locked, not wall-clock-plus-parallel. A mid-block MT-flag flip
(rare, message-thread toggle) counts some tasks and not others for that one block;
self-corrects next block via the per-block reset.

**Verification:** bulk-run R2 — campaign section §B.20 (3 scenarios). Build-confirmed
clean (Release+Debug) at both task gates, 2026-07-18.
