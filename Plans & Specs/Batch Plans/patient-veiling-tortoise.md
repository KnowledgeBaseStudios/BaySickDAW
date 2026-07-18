# QA-I — Heavy Operation Progress Overlay — Plan (patient-veiling-tortoise)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/patient-veiling-tortoise.md`.
> Paired running notes: `Running Notes/patient-veiling-tortoise.md`.
> **Execution mode: bulk run** (swift-stampeding-caribou R1-R5): no per-task verify pauses;
> §B authored at code-complete; Work Log entry HELD; ONE commit at close; spec calls ASKED.

## Context

§5 scope unchanged (NAV-02 engine-swap busy sign, APP-02 shutdown overlay killing the
black-screen period, APP-03 project-load modal; STATE-03/04 fold in as the visual layer).
Scout truth (2026-07-17): NO overlay/progress/busy-cursor component exists anywhere — full
greenfield; the entire load path runs synchronously ON the message thread (three direct 30 ms
sleeps included), so a naively-added overlay never repaints — the overlay must pump paints at
step boundaries; the shutdown black screen is the window being destroyed FIRST (black
DocumentWindow background) while teardown grinds after it. Risk: medium (UI lifecycle around
shutdown; Windows DWM behavior). Effort: ~6-10h. Dependencies: none open (QA-D shipped).

## Spec calls already locked

| ID | Decision |
|----|----------|
| §5 | Reusable overlay component: step labels + progress bar + busy cursor; wired to load, shutdown, engine-swap/preset ops |

Technical approach (implementation, not behavior): work stays synchronous on the message
thread; the overlay repaints via explicit paint-pump at each step-update dispatch — no
off-thread load re-architecture in this batch.

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open — all locked in the 2026-07-17 G3 docket rounds.

## Files to modify

- New `Source/Standalone/HeavyOperationOverlay.h/.cpp` — dim layer + title/step label +
  progress bar + busy cursor; modal + non-modal; explicit pump on step update
- `Source/Standalone/StandaloneEditor.cpp` — load path step dispatch: per-`<Tab>` loop
  (:10368+), `applyEngineState` (:10360), `restoreAudioStripsFromArrangement` block loop
  (:11130), `closeAllDynamicTabs` (:10199); engine-swap hooks
- `Source/PluginProcessor.cpp` — `deserializeProject` phase boundaries (:4571-4669)
- `Source/Standalone/StandaloneApp.cpp` — `shutdown()` order (:939-993; window destroy :974
  currently precedes teardown), `VibeSynthWindow` background (:15)
- `Source/Standalone/LayersPage.cpp` / `BassPage.cpp` / `DrumPage.cpp` — `selectEngine`
  wrap (+ editor preset-load hooks for the heavy sample loads)

## Tasks

### Task 1 — Overlay component
- [ ] HeavyOperationOverlay: covers the editor (or window), dimmed backdrop, operation title,
      step label, determinate/indeterminate bar, busy cursor; `beginOp/setStep(i,n,label)/
      endOp` API; setStep pumps a paint so progress is visible mid-freeze.
- [ ] Build-confirm gate + running-notes checkpoint.

### Task 2 — Project load (APP-03)
- [ ] Wire steps: parse → tabs close → per-tab engine restore ("Tab N of M — <name>") →
      audio strips rebuild → done. Covers Open Project + New-from-template + startup restore.
- [ ] Build-confirm gate + checkpoint.

### Task 3 — Shutdown (APP-02)
- [ ] Overlay up BEFORE teardown; window stays alive (visible, overlaid) through editor +
      engine teardown, destroyed LAST — the black-screen period dies. Verify against the
      QA-Eb window-state save (maximize restore must not regress).
- [ ] Build-confirm gate + checkpoint.

### Task 4 — Engine swap + heavy preset loads (NAV-02)
- [ ] `selectEngine` bodies (Layers one-time pick; Drum/Bass swap-aware) + the sfizz-class
      kit/preset loads get busy overlay (indeterminate) while they grind.
- [ ] Build-confirm gate + checkpoint.

### Task 5 — Close (bulk run)
- [ ] §B section authored; Work Log entry drafted + HELD; ONE commit (message + full git
      status → Jeff approves).

## Verification (§B-destined scenarios)

1. Open a big project — overlay with live step text/bar the whole load; window never
   white-freezes bare.
2. Quit with a heavy session — overlay through teardown; no black-window period.
3. Pick an engine on a fresh Drums tab + load a big kit — busy indicator during the load.
4. Overlay never sticks: after each op the UI is fully interactive; Debug run clean of
   asserts.

## Routing notes (Rule 3)

Real bugs on the load/shutdown paths fix in-batch (teardown-order findings likely). Anything
that smells like re-architecture (off-thread loading) gets ASKED, not built.

## Carry-Forward Reference touch points

- Shutdown: QA-Eb WindowState save/restore block (StandaloneApp) — do not regress the
  maximize-restore fix (restore-after-setResizeLimits ordering).

## Carry-Over — 2026-07-18 (QA-I close; session end)

- **Completed:** QA-I in full — Tasks 1-4 (overlay component + paint-pump; project-load
  wiring w/ four entry wraps; shutdown teardown reorder, window destroyed LAST;
  engine-swap/sample/kit busy wraps) — ONE batch commit `b6f51617`, every task gate
  build-confirmed clean (one Debug-only C2664 fixed at the Task 3 gate). §B.15 authored
  (7 scenarios); Work Log entry drafted + HELD in the running notes; carries the two
  QA-H doc stragglers.
- **In-flight:** nothing in code. TWO expected doc stragglers in the tree for QA-J''s
  commit (B.13/B.14/B.15 precedent): the §B.15 `blocks:` hash backfill (`b6f51617`) +
  this carry-over block.
- **Assumptions changed:** the plan's "startup restore" load flow does not exist
  (startup = blank + default tabs per QA-Ef #6); the real fourth load path is
  Restore-from-Backup — wired under the §5 "wired to load" lock.  ResizableWindow hides
  addChildComponent behind a JUCE_DEBUG-only misuse trap (Debug-only C2664; the
  sanctioned bypass is an explicit `Component::` base-class call).
- **Resume action (NEW session):** paste the QA-J' prompt (handed to Jeff at this
  session's close) — /standup -> Main Plan §0 -> caribou G3 sections ->
  prompt-reseeking-newt.md IN FULL -> confirm `b6f51617` at HEAD (+ the two stragglers
  dirty) -> QA-J' Task 1.
- **Work-Log entry needed:** none new — QA-I's is drafted + HELD in
  `Running Notes/patient-veiling-tortoise.md` (applies at §B.15 section pass).
