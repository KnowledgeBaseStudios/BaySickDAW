# Running Notes — QA-VibeSlider (gentle-swapping-gecko)

> Append-only mid-batch log. Entries land at every checkpoint (commit landed / finding captured
> / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At code-complete, `/draft-doc batch-close` consumes this file to compile the Implemented Work
> Log entry, which is HELD here (bulk-run R2) and applied at the batch's campaign section pass.
>
> Pair file: [`Plans & Specs/Batch Plans/gentle-swapping-gecko.md`](../Batch%20Plans/gentle-swapping-gecko.md).
> Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11).

## 2026-07-25 — Task 1/2 — census gap: VKnob's inner slider (spec call resolved)

**Finding.** The plan's Context lists VKnob-based surfaces as NOT targets ("all EffectEditorPanels
(VKnob-based...), BaySickPedals (composes panels), BaySickNAMIR (VKnob)"). True for those panels —
they declare no plain sliders — but `VKnob` itself holds `juce::Slider slider;`
([SharedUI.h:801](Source/Standalone/SharedUI.h) pre-edit), a 113th declaration the scout census
(~112 lines) never counted, in the most-used knob widget in the app.

**Desk-verified before surfacing** (JUCE source, not assumption):
- `Slider::Pimpl::mouseDown` ([juce_Slider.cpp:852](JUCE/modules/juce_gui_basics/widgets/juce_Slider.cpp))
  sends a right-click into the drag path (VKnob never calls `setPopupMenuEnabled`, so `menuEnabled`
  is false) and calls `mouseDrag(e)` at :901. For VKnob's `RotaryVerticalDrag` that reaches
  `handleAbsoluteDrag`, where the delta is `mouseDragStartPos.y - e.position.y` = 0 at mouseDown —
  **the value does not jump.** The BLU-493 jump needs the `snapsToMousePos` linear branch (:802-808),
  which is why MetroPanel volume + the two modal LinearHorizontal hosts genuinely were in scope.
- Right-click on a VKnob today still fires a spurious `ScopedDragNotification` pair + the value
  bubble. Harmless: `EffectEditorPanels.cpp:156` early-returns on `approximatelyEqual(before, after)`,
  so no undo-stack pollution.
- The Automate menu **survives** a VibeSlider swap: `VKnob::mouseDown` fires via
  `slider.addMouseListener(this, false)` (SharedUI.cpp:1838), and `Component::internalMouseDown`
  ([juce_Component.cpp:2269-2276](JUCE/modules/juce_gui_basics/components/juce_Component.cpp)) calls
  `target->mouseDown(me)` *then* `MouseListenerList::sendMouseEvent`. VibeSlider's early return does
  not suppress the listener dispatch.

**Spec call posed** (literal-sweep scope vs the plan's explicit exclusion) — **Jeff: A, swap it.**

**Consequence.** `VibeSlider` was defined at SharedUI.h:1029, *after* `VKnob` at :797 — a value
member needs a complete type, so the class definition moved above `VKnob` (now :811, VKnob :836,
`SnapSlider` unchanged at :1056 and still deriving it). Same edit dropped the now-false trailing
paragraph of the VibeSlider rationale block ("App-wide refactor ... scheduled as a separate
session" — this batch *is* that refactor) and re-anchored the orphaned "Fader with 0 dB snap"
header onto `SnapSlider`, where it belongs.

## 2026-07-25 — process — G4 plan files were missing per-task build gates

**Finding.** Jeff caught me running Task 1 into Task 2 without a build. I had read the bulk-run
"no per-task verify pauses" (run plan R-loop step 2) as covering the compile gate. It does not —
it retires the per-task *functional test* + running-notes checkpoint only. The G3 exemplar
[`silky-gliding-lynx.md`](../Batch%20Plans/silky-gliding-lynx.md) closes Tasks 1-6 with
`- [ ] Build gate.` and states it outright in its Verification section: "scenarios author into §B
at code-complete (Task 7), not run mid-batch. **Build gates only.**"

**Root cause.** The eight G4 plan files omit the gate checkboxes entirely — same omission class as
the per-task commit steps Jeff caught mid-write at group open. `gentle-swapping-gecko.md` Task 1
has five checkboxes and Task 2 four; the only build step sits in "Batch close".

**Resolved (Jeff: A).** `- [ ] Build gate.` retrofitted as the closing checkbox of every task in
the seven remaining G4 plan files — pigeon 2, crane 5, pangolin 3, walrus 4, badger 6, yak 6,
stoat 4 (30 gates, insert-only). `gentle-swapping-gecko.md` deliberately NOT retrofitted: its tasks
were already coded past the gate, and adding a checkbox that was never honored would be a false
record. Per-task gating is standing practice for the rest of G4.

## 2026-07-25 — code-complete

- Both tasks coded; BOTH configs build clean (Jeff, 2026-07-25 10:46 — and re-confirmed by my own
  run at 11:25 under the new build protocol, `RELEASE_EXIT_CODE=0` / `DEBUG_EXIT_CODE=0`).
- Master Test Plan **§B.25** authored — 10 scenarios (VS-1..VS-10). Reconciled against what
  actually shipped rather than transcribed from the plan file's 8-item ladder: the linear surfaces
  (VS-1 routing matrix, VS-6 metronome volume, VS-7 mixer send, VS-8 modal scale hosts) are flagged
  as the REAL regression tests since the value-jump is a snap-to-mouse linear defect only, and
  **VS-9 is net-new** for the VKnob scope addition. VS-4/VS-5 are negative tests guarding the three
  deliberately-excluded right-click-feature subclasses.
- **Process change adopted mid-batch (Jeff, 2026-07-25):** I now run `do_build.bat` myself per task
  (background, both exit codes read from `build_log.txt`), replacing the per-task hand-off. Verified
  working: the script is self-contained — it resets PATH and calls `vcvars64.bat` itself (lines 3-4),
  so the long-standing CLAUDE.md claim "MSVC env not available" was stale. Spec calls and per-batch
  commit approval remain hard stops and did NOT change; the commit gate is now Jeff's only routine
  per-batch checkpoint, so the surfaced commit message carries a reviewable plain-English summary.
- **Diagnostic Instrumentation Catalog (Rule 4): NONE.** No `DBG` / `juce::Logger` / temp `jassert` /
  debug `AlertWindow` / temp-file trace added this batch. The VKnob question was settled by reading
  JUCE source, not by instrumenting. Nothing to strip at close.

## Held Work Log entry (apply at section pass)

> Drafted at code-complete via `/draft-doc batch-close`; NOT yet applied to `Implemented Work Log.md`.
> Applies with the §5 STATUS flip when §B.25 passes the campaign walk (R2). Commit hash is filled
> (`bd49d066`); stamp the full `HH:MM PT` at apply.

### 2026-07-25 <HH:MM> PT — QA-VibeSlider — BLU-493 app-wide sweep: every plain `juce::Slider` member declaration -> `VibeSlider` across 19 files (five engine/vocal families + 7 scattered singles), so right-click can no longer jog a value on its way to the Automate menu; mid-batch scope addition swapped VKnob's inner slider (the census-missed declaration in the app's most-used knob widget), which forced the `VibeSlider` definition above `VKnob` in SharedUI.h; three right-click-feature subclasses deliberately excluded; process finding retrofitted per-task build gates into the seven remaining G4 plan files

**Bucket:** UI / L&F / Theming. Batch `gentle-swapping-gecko`. `blocks:` `bd49d066`.

#### Done

- **Task 1 — instrument + vocal editor sweep.** Every plain `juce::Slider` member declaration swapped to `VibeSlider` across the five families, matching the scout census file-for-file: **Harmless 54** (`HarmlessEditor.h` 44, `HarmlessModEditor.h` 4, `HarmlessFilterRow.h` 4, `HarmlessRoutingMatrix.h` 1 array, `HarmlessXYZPad.h` 1), **BaySickSynth 16**, **BaySickBass 16**, **BaySickPlayer 15**, **Vocal family 8** (`BaySickVocalEditor.cpp` 5, `BaySickAlignEditor.cpp` 1, `BaySickPitchEditor.cpp` 1, `BaySickPitchSubEditor.h` 1) = 109 declaration lines. Style-argument constructions kept their `(SliderStyle, TextEntryBoxPosition)` form unchanged — `VibeSlider`'s ctor forwards. Two headers needed the include (`HarmlessModEditor.h`, `BaySickPitchSubEditor.h` both gained `#include "../Standalone/SharedUI.h"   // VibeSlider`); every other target already saw `SharedUI.h` directly or transitively. Grep for `setPopupMenuEnabled` / `mouseDown` overrides on the swapped members came back zero, as scouted.
- **Task 2 — scattered singles + edge cases.** `MixerPage.cpp:1247` send-amount slider, the two hidden ARIA driver members (`AriaControlPanel.cpp` :437 / :578), and the two modal scale-velocity hosts (`DrumKitGrid.cpp:2651`, `PianoRoll.cpp:4697`) swapped as declarations; `MetroPanel.h` volume + the GLOBAL transport Swing knob (`GlobalTransportBar.cpp:438`) swapped at the `std::make_unique<VibeSlider>` construction site with the `unique_ptr<juce::Slider>` member type left alone. The ARIA drivers + modal hosts are literal-completeness picks — behavior-neutral (they never receive a user right-click and carry no componentID).
- **Exclusions honored (verified by object type, not declared type).** `PageSwingKnob` — the trap the plan called out: it hides behind `std::unique_ptr<juce::Slider> mSwingKnob` at `SharedUI.h:360`, and its right-click is the "Truncate Swing Notes" menu. `AriaKnob` (`AriaControlPanel.cpp:136`) and `AriaSlider` (:234) — their right-click is the ARIA param popup. All three untouched.
- **Scope addition mid-batch (Jeff's spec call = A) — VKnob's inner slider.** `VKnob` held `juce::Slider slider;` — a declaration the plan's "VKnob-based surfaces are NOT targets" line implicitly protected and the scout census never counted. Swapped to `VibeSlider`. **Consequence:** the `VibeSlider` class definition had to move ABOVE `VKnob` (a value member needs a complete type) — it now sits at `SharedUI.h:811`, `VKnob` at :836, `SnapSlider` unchanged at :1056 and still deriving `VibeSlider`. Desk-verified before the swap: the rotary path never jumped in the first place (`RotaryVerticalDrag` -> `handleAbsoluteDrag` with a zero delta at mouseDown), so this is prophylactic plus the removal of a spurious drag notification, not a bug fix; and the Automate menu survives because `VKnob` registers via `slider.addMouseListener(this, false)` (`SharedUI.cpp:1838`) and `Component::internalMouseDown` dispatches to listeners AFTER the target's own (swallowed) `mouseDown`. The inner slider also carries `getProperties().set("vknob_slider", true)` so `GlobalAutoRightClick` skips it — the menu comes from `VKnob`'s own handler, exactly the path the swap preserves.
- **Rule 6 pass over edited regions.** Stripped pure date/batch tags on touched declaration lines (`// 2026-04-25 master out parity (BaySickPlayer)` on `mOutVolKnob`, BaySickSynth + BaySickBass; `// S2 SLA #8/#9`; `// SLA-Impl #43/#44`; `// T2-H:`). Dropped the now-false trailing paragraph of the `VibeSlider` rationale block ("App-wide refactor (replace juce::Slider everywhere) scheduled as a separate session") — this batch IS that refactor. Re-anchored the orphaned `// Fader with 0 dB snap` header, which the original ordering had stranded above the `VibeSlider` block, back onto `SnapSlider`. Added two keeper comments (category 1/4): why the Automate menu still fires on `VKnob::slider`, and the class-order dependency.
- **Build.** BOTH configs clean.

#### Found along the way

- **FINDING (census gap) — the scout census missed `VKnob`'s inner `juce::Slider`.** The plan's Context excluded VKnob-BASED surfaces (all `EffectEditorPanels`, BaySickPedals, BaySickNAMIR) — correct, those panels declare no plain sliders — but the exclusion silently swallowed `VKnob` itself, the most-used knob widget in the app. Surfaced as a spec call rather than absorbed, since the plan's exclusion list was explicit.
- **FINDING (JUCE behavior, desk-verified before surfacing) — the BLU-493 jump is a linear/snap-to-mouse defect only.** `Slider::Pimpl::mouseDown` (`juce_Slider.cpp:852`) routes a right-click into the drag path whenever `setPopupMenuEnabled` was never called, and calls `mouseDrag(e)` at :901. For a `RotaryVerticalDrag` knob that reaches `handleAbsoluteDrag` with `mouseDragStartPos.y - e.position.y` = 0, so the value does not move; the value-jump needs the `snapsToMousePos` linear branch (:802-808). That is why the MetroPanel volume slider and the two modal `LinearHorizontal` hosts genuinely WERE in scope, and why the rotary majority of this sweep is a guard against future style changes rather than a live defect fix. §B.25 is shaped around this.
- **Sub-finding — pre-swap right-click on a VKnob fired a spurious `ScopedDragNotification` pair plus the value bubble.** Harmless in practice: `EffectEditorPanels.cpp:156` early-returns on `approximatelyEqual(before, after)`, so no undo-stack pollution ever resulted. Gone after the swap.
- **PROCESS FINDING — the eight G4 plan files omitted per-task build gates.** Root cause: I read the bulk-run "no per-task verify pauses" (run plan R-loop step 2) as covering the compile gate. It does not — it retires the per-task FUNCTIONAL test + running-notes checkpoint only. The G3 exemplar [`silky-gliding-lynx.md`](Batch Plans/silky-gliding-lynx.md) closes every task with `- [ ] Build gate.` and states it outright. Same omission class as the per-task commit steps caught mid-write at G4 group open.
- **Cross-batch note (restated for the record):** the vocal / NAMIR / Pedals editors gain their Automate MENUS in QA-ApvtsAutomation (componentID work, `wired-lassoing-crane`), not here. This batch only installs the right-click guard on those surfaces.

#### What was done about each finding

- **VKnob census gap: SWAPPED (Jeff = A).** Scope addition taken in-batch rather than routed — same literal-sweep charter, and leaving the app's most-used knob on a raw `juce::Slider` would have made the sweep's own claim false. Cost was the `SharedUI.h` class reorder, contained to that header.
- **Rotary-vs-linear characterization: recorded, no code consequence.** It reframes the sweep's value (guard + consistency across ~200 widgets, real fix on the handful of snap-to-mouse linear sliders) and drove the §B.25 scenario shaping.
- **Build-gate omission: FIXED (Jeff = A).** `- [ ] Build gate.` retrofitted as the closing checkbox of every task in the seven REMAINING G4 plan files — pigeon 2, crane 5, pangolin 3, walrus 4, badger 6, yak 6, stoat 4 = 30 gates, insert-only. `gentle-swapping-gecko.md` deliberately NOT retrofitted (its tasks were already coded past the gate; a checkbox never honored would be a false record).
- **Nothing routed out.** No §9 Forks entry, no §5/§6 structural change from this batch's own findings.

#### Group review (R3)

- **Pending — runs at the G4 boundary** (after `clean-pointing-stoat`'s commit) over the group's combined diff. Fill with the outcome for this batch at that point.

#### Carry-forward contradictions

- None. Carry-Forward §1 (UI primitives) was skim-only per the plan — no audio thread, no APVTS layout, no persistence surface touched. One structural note to carry: `VibeSlider` is now defined ahead of `VKnob` in `SharedUI.h`, so anything added between :793 and :836 must not depend on `VKnob` being first.

#### Diagnostic Instrumentation Catalog

- **NONE added this batch.** Nothing to strip.

#### Commit(s)

`bd49d066` (whole batch — Tasks 1+2 + the VKnob scope addition + Rule 6 pass + §B.25 + the 30 G4 build-gate retrofits + CLAUDE.md build-ownership reversal + held entry + running notes; single batch commit per the bulk-run model). Batch opened under the G4 group-open commit `b6b47685`. Build verified clean in BOTH configs; behavioral verification deferred to the R2 campaign pass against §B.25.

#### Next action

- Proceed to **QA-NativeDialogs'** ([`polite-homing-pigeon.md`](Batch Plans/polite-homing-pigeon.md)), G4 batch 2 of 8.
