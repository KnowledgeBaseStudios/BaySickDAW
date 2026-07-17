# QA-N — DSP Meter Sum-of-Cores (DIAG-02) — Plan (honest-summing-falcon)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/honest-summing-falcon.md`.
> Paired running notes: `Running Notes/honest-summing-falcon.md`.
> **Execution mode: bulk run** (R1-R5): §B at code-complete; Work Log HELD; ONE commit;
> spec calls ASKED.

## Context

Under MT the meter measures audio-thread wall-clock only (processBlock entry → end), so
parallel worker time vanishes — the reading is the max path, not total render work (the code
comment admits it). No per-worker busy accounting exists anywhere in Source/Engine/. Scope:
instrument worker busy time, sum it, meter reads "% of one core" of TOTAL work. Scout
surprise recorded: the live cap is 10.0 (1000%) with a HOLD-FOR-Phase-6 comment — the
marathon's 2.0 release cap applies at Phase 6 (QA-Audit docket), NOT here; this batch leaves
the cap alone. Risk: low (measurement only). Effort: ~3-5h. Dependencies: none.

## Spec calls already locked

| ID | Decision |
|----|----------|
| §5 | Sum audio-thread + per-worker busy time; "% of one core" tracking total render work |
| Marathon 12d | Release cap 200% applied in Phase 6 — out of scope here; cap untouched |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

- `Source/Engine/VibeThreadPool.cpp/.h` — per-worker busy-tick accumulation around task
  run() in `workerLoop` (:155) + the main-thread pump participation (`runUntilOrTimeout`)
- `Source/Engine/RenderGraphDispatcher.cpp/.h` — per-block reset + sum exposure (mAllDone
  :125 lifecycle is the block boundary)
- `Source/PluginProcessor.cpp` — `measureDspLoadAndOverload` (:3164-3185): MT path adds the
  summed busy time; ST path unchanged; smoothing/cap logic untouched (:3178 cap comment
  stays)
- Display: none (GlobalTransportBar reads the same atomic)

## Tasks

### Task 1 — Per-worker busy accounting
- [ ] Accumulate high-res tick deltas around each task execution in every worker AND the
      audio thread's pump participation; per-block reset at dispatch start; relaxed atomics,
      allocation-free, audio-thread safe; zero cost when MT is off.
- [ ] Build-confirm gate + running-notes checkpoint.

### Task 2 — Meter consumes the sum
- [ ] MT path: load = (sum of busy ticks)/bufDur → same smoothing → same atomic; ST path
      byte-identical behavior. Existing 1000% measurement cap + 999% display clamp untouched
      (Phase 6 owns the release value).
- [ ] Rule 6 comment updates where the old "reads lower under MT" note sat.
- [ ] Build-confirm gate + checkpoint.

### Task 3 — Close (bulk run)
- [ ] §B section authored; Work Log entry HELD; ONE commit (message + full git status →
      Jeff approves).

## Verification (§B-destined scenarios)

1. Heavy project, MT ON: DSP% reads HIGHER than the pre-batch build on the same project
   (parallel work now counted) and scales with added tracks.
2. Same project, MT OFF: reading matches the old behavior (single-thread path unchanged).
3. Idle/light project: near-zero both modes; no jitter regressions at 128 buffer.

## Routing notes (Rule 3)

If instrumentation reveals dispatcher hot spots, that's /perf-audit material at the next
trigger — log, don't chase here.

## Carry-Forward Reference touch points

- MT engine records (dispatcher + MasterTask release-store pattern) before touching the
  block lifecycle; QA-Md "MT works in Debug" standing correction.
