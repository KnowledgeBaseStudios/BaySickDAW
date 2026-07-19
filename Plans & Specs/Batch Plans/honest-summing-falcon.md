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

## Carry-Over — 2026-07-18 (G3 session end; QA-N close)

Consolidated session carry-over (one block per the QA-I->QA-J' precedent, not one per
batch — each closed batch already carries a full CODE-COMPLETE + held Work Log entry in
its own running notes).

- **Completed this session (5 batches, 6 commits, each build-confirmed clean at every
  task gate):** QA-J' `b54a44d4` (unmute re-sync across 3 PV seek sites + applicator-map
  hygiene + EffectsPage rack-pid J-6 range fix); QA-K `684cf253` (APP-04 priority/MMCSS
  + APP-05 ASIO panel + DSP-11 live buffer path + DSP-01 Lasersaw/22 preset-data fixes +
  9 deletions + factory-preset audit report + versioned effect-preset seeding + Rusty
  512-CC save); QA-L `2e2df50a` (PopupMenu left-only trigger [vendored] + deleted-slot
  lane names + mixer strip lifecycle/dropdown refresh + NAV-01 header sync + FX Rack /
  Player Page nav buttons + FILE-03 dup handling + LDT-394 roll accuracy + per-drum MIDI
  notes w/ kit fan-out); QA-M `ce4cb33c` (kit-load leaves Rusty alone + Rusty re-add
  auto-reloads last kit); QA-N `2e44ab78` (DSP meter sum-of-cores DIAG-02). QA-I
  `b6f51617` closed the prior session.
- **In-flight:** nothing in code. ONE expected doc straggler in the tree for
  QA-OctavePedal's commit: the §B.20 `blocks:` hash backfill (`2e44ab78`) in
  `v1-master-test-plan.md` (B.13-B.19 precedent — the batch commit lands, then its own
  §B hash backfills as the next commit's straggler).
- **Assumptions changed / notable:** QA-K's versioned effect-preset re-seed FIRED at the
  QA-L gate as forecast (28 factory Effects XMLs rewritten from the fixed table + the new
  stamp file — rode the QA-L commit, expected one-time heal). Docket picks resolved
  in-session: QA-J' docket-1 = a (fix rack-pid range in-batch); QA-K 1a/2c/3c + LOW adds
  (Bell Lead, 808 Claves) + versioned-seeding sub-pick 1; QA-L Task-4 flagged
  interpretation CONFIRMED by Jeff ("This is correct, proceed" — the two piano-roll nav
  buttons); QA-M replace-prompt scan = pick 2 (DrumPage-only scan, text clarified — the
  "count Rusty too" half was vestigial post-teardown-fix). CLAUDE.md Source-layout entry
  for PageMenuBar is stale (lives in SharedUI, not a PageMenuBar.h) — noted, not fixed.
- **Resume action (NEW session):** paste the QA-OctavePedal prompt (handed to Jeff at
  this session's close) — /standup -> Main Plan §0 in full -> caribou G3 sections ->
  confirm `2e44ab78` at HEAD (+ the one B.20 straggler dirty) -> read
  `Batch Plans/locked-doubling-frog.md` IN FULL -> begin QA-OctavePedal Task 1.
  **RE-FLAG BEFORE CODING:** the last baked-pending-veto interpretation — inst monitor
  default = With Effect (T5) — must be re-surfaced to Jeff before that task is coded
  (per the group-open standing list). QA-OctavePedal also carries the folded Rule-3 item:
  BaySickPedals PDC latency reporting (pull model) -> Task 4 + the octave-internal-latency
  companion.
- **After QA-OctavePedal closes = G3 GROUP BOUNDARY:** R3 `/review-batch` over the
  combined G3 diff (QA-G..QA-OctavePedal; findings fixed before proceeding) + Jeff's
  15-30 min smoke (Debug first, then Release; §7 items 1-5). Then G4 opens.
- **Work-Log entries needed:** none new to author — all 6 batches' entries are drafted +
  HELD in their running notes, applying at each §B section pass during the campaign (R2).
  Push: 6 commits unpushed (origin at `50c6eeb9`/QA-H); Jeff pushes when he wants the
  backup current.
