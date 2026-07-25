# QA-VibeSlider — App-wide juce::Slider -> VibeSlider swap — Plan (gentle-swapping-gecko)

> **Canonical path:** `Plans & Specs/Batch Plans/gentle-swapping-gecko.md` (mirrored at G4 group
> approval; home-dir copy deleted per convention). **For execution:** bulk-run G4 batch 1 of 8
> (run plan `swift-stampeding-caribou.md`, R1-R5). No per-task verify pauses; verify scripts
> author into Master Test Plan §B at code-complete. One source commit for the batch.

## Context

BLU-493. Right-clicking a plain `juce::Slider` with snap-to-mouse behavior moves the value to
the click position before the app-wide Automate listener sees the click — the exact UX bug
`VibeSlider` (right-click swallow on mouseDown/mouseDrag/mouseUp,
[SharedUI.h:1029-1051](Source/Standalone/SharedUI.h)) was built to fix. Only the EQ widget,
DynamicParamsPopout, and MixerTrackStrip use it today; every instrument editor still uses raw
sliders.

Scout census (2026-07-25, source-verified): **~112 clean declaration lines / ~200 widgets.**
Harmless 54 lines (HarmlessEditor.h 44, HarmlessModEditor.h 4, HarmlessFilterRow.h 4,
HarmlessRoutingMatrix.h 1 array, HarmlessXYZPad.h 1), BaySickSynth 16, BaySickBass 16,
VibePlayer 15, Vocal family 8 (VocalEditor 5, AlignEditor 1/6 widgets, PitchEditor 1/4,
PitchSubEditor 1/3), plus MixerPage send-amount slider (:1247), MetroPanel volume (:122),
GlobalTransportBar Swing knob (:189), 2 hidden ARIA drivers (AriaControlPanel.cpp:437/:578),
2 modal scale-velocity hosts (DrumKitGrid.cpp:2651, PianoRoll.cpp:4697).

NOT targets (verified): all EffectEditorPanels (VKnob-based; `EQFader` already derives
VibeSlider), BaySickPedals (composes panels), BaySickNAMIR (VKnob), `SnapSlider` (already
derives VibeSlider), and three subclasses whose right-click is load-bearing — **PageSwingKnob**
(right-click = Truncate Swing Notes menu, [SharedUI.cpp:1516-1554](Source/Standalone/SharedUI.cpp);
note the trap: it hides behind `unique_ptr<juce::Slider> mSwingKnob` at SharedUI.h:360),
**AriaKnob** (:136) and **AriaSlider** (:234) (right-click = ARIA param popup).

- **Risk:** low. Drop-in subclass; behavior change is exactly "right-click no longer moves the
  value." Main hazard = touching one of the three load-bearing subclasses (excluded by object
  type, not declared type).
- **Effort:** ~3-5 h (absorption-corrected from the stale ~5-8 h / "~493 sites" figure).
- **Dependencies:** none. Cross-batch note: the vocal/NAMIR/Pedals editors gain **Automate menus**
  in QA-ApvtsAutomation (componentID work), not here — this batch only adds the right-click
  guard there.

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| Scope | Literal sweep of every plain `juce::Slider` incl. hidden ARIA drivers, modal scale-velocity hosts, MetroPanel volume, transport Swing knob | Baked-pending-veto 2026-07-25, no veto raised; harmless + literal reading of the §5 scope |
| Exclusions | PageSwingKnob / AriaKnob / AriaSlider untouched | Their right-click handlers are features (Truncate menu, ARIA popup); they are subclasses, not plain sliders |
| Vocal scope | Guard-only swap; no componentID/Automate wiring here | That is QA-ApvtsAutomation's charter (docket 2026-07-25) |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open — the 2026-07-25 G4 docket locked everything.

## Files to modify

- Task 1: `Source/Harmless/HarmlessEditor.h`, `HarmlessModEditor.h`, `HarmlessFilterRow.h`,
  `HarmlessRoutingMatrix.h`, `HarmlessXYZPad.h`
- Task 1: `Source/BaySickSynth/BaySickSynthEditor.h`, `Source/BaySickBass/BaySickBassEditor.h`,
  `Source/VibePlayer/VibePlayerEditor.h`
- Task 1: `Source/BaySickVocal/BaySickVocalEditor.cpp` (:420,:432-435),
  `BaySickAlignEditor.cpp` (:975), `BaySickPitchEditor.cpp` (:1792), `BaySickPitchSubEditor.h` (:72)
- Task 2: `Source/Standalone/MixerPage.cpp` (:1247), `Source/Standalone/MetroPanel.h` (:49,:122),
  `Source/Standalone/GlobalTransportBar.h/.cpp` (:189 / :438-453),
  `Source/Standalone/AriaControlPanel.cpp` (:437,:578 — hidden members),
  `Source/Standalone/DrumKitGrid.cpp` (:2651), `Source/Standalone/PianoRoll.cpp` (:4697)

## Tasks

### Task 1 — Instrument + vocal editor sweep

- [ ] For each header above, change member declarations `juce::Slider name;` -> `VibeSlider name;`
  (incl. the `mSliders[kNumSliders]` array in HarmlessRoutingMatrix.h). Where the type is
  constructed with a style argument, use VibeSlider's `(SliderStyle, TextEntryBoxPosition)` ctor
  form unchanged — it forwards.
- [ ] Confirm each TU sees `Source/Standalone/SharedUI.h` (transitively or add the include).
  Engine editors that don't currently include it get the include added at the top of the header
  that declares the members.
- [ ] Grep the edited files for `setPopupMenuEnabled` / `mouseDown` overrides on the swapped
  members — expected zero (scout-verified); anything found gets flagged in running notes before
  proceeding.
- [ ] Leave untouched: `SharedUI.h:360 mSwingKnob` (holds PageSwingKnob), AriaKnob/AriaSlider
  class definitions.
- [ ] Rule 6 pass over edited regions only.

### Task 2 — Scattered singles + edge cases

- [ ] `MixerPage.cpp:1247` send-amount slider -> VibeSlider.
- [ ] `MetroPanel.h` mVolSlider: `make_unique<VibeSlider>` (member type may stay
  `unique_ptr<juce::Slider>`).
- [ ] `GlobalTransportBar` mSwingKnob (the GLOBAL Swing knob, plain slider — distinct from
  PageMenuBar's PageSwingKnob): `make_unique<VibeSlider>`.
- [ ] ARIA hidden drivers ×2 + the two modal scale-velocity `slider` members -> VibeSlider
  (literal-completeness picks; behavior-neutral, they never receive user right-clicks or have
  componentIDs).

## Batch close (bulk-run per-batch loop — one commit per batch)

- [ ] Tell Jeff to run `do_build.bat`; fix until BOTH configs build clean.
- [ ] Author this batch's Master Test Plan §B section from the Verification list below
  (`blocks:` = this batch's commit, hash backfilled at commit).
- [ ] `/draft-doc batch-close` -> append under `## Held Work Log entry (apply at section pass)`
  in the running notes. Do NOT touch the Implemented Work Log or the §5 STATUS line now (R2).
- [ ] Append the running-notes code-complete entry (+ any Rule 4 catalog rows, same edit pass
  as the code that added them).
- [ ] ONE batch commit (Rule 9): `QA-VibeSlider: <one-line what> (<scope>)` + `Co-Authored-By`
  trailer; surface message + FULL git status (every dirty/untracked entry with disposition);
  commit only on Jeff's approval.

## Verification (end-to-end; authors into Master Test Plan §B at code-complete)

1. Harmless routing-matrix vertical slider: right-click — value does NOT jump; Automate menu opens.
2. BaySickSynth + BaySickBass + BaySickPlayer knob each: right-click — no value jump, Automate menu.
3. BaySickVocal Retune/Strength slider: right-click — no value jump, no menu (componentID arrives
   in QA-ApvtsAutomation), left-drag still works.
4. PageMenuBar Swing Mix knob: right-click still opens "Truncate Swing Notes".
5. ARIA knob on the Rusty page: right-click still opens the ARIA param popup.
6. Global transport Swing knob + metronome volume: right-click does nothing, drag + double-click
   default still work.
7. Mixer send-amount slider (cable right-click popup): drag works, right-click no-jump.
8. Spot: double-click-reset and Ctrl-fine-drag unchanged on 2-3 swapped controls.

## Routing notes (Rule 3)

Findings on the Automate-menu path (componentID gaps, wrong ids) route INTO QA-ApvtsAutomation
(same group, later batch — fold directly). Anything on VKnob internals routes to running notes
for close-time slotting.

## Carry-Forward Reference touch points

§1 (UI primitives) skim only — this batch touches no audio thread, no APVTS layout, no
persistence. The VibeSlider rationale comment block (SharedUI.h:1012-1028) is the authoritative
in-code spec.
