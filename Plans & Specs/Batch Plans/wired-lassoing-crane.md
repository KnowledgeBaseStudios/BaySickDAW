# QA-ApvtsAutomation — Full APVTS + automation coverage (engine-param application, componentIDs, BLU-378/379/492) — Plan (wired-lassoing-crane)

> **Canonical path:** `Plans & Specs/Batch Plans/wired-lassoing-crane.md` (mirrored at G4 group
> approval; home-dir copy deleted). **For execution:** bulk-run G4 batch 3 of 8. §B authored at
> code-complete; one source commit.

## Context

The owner-confirmed gap (2026-07-10) survives post-G3 (scout re-verified 2026-07-25 at HEAD):
instrument-engine lanes (`tk_{lay|bas|drm}_{N}_{tag}_*`) create/draw but never apply — the
audio-thread pass resolves ONLY main-APVTS ids ([PluginProcessor.cpp:2861](Source/PluginProcessor.cpp:2861));
the UI pass falls through main-APVTS -> applicator registry
([StandaloneEditor.cpp:3307-3318](Source/Standalone/StandaloneEditor.cpp:3307)) and no
instrument-engine editor registers an applicator (registration sites today: effect panels
:210-263, mixer strips :454-480, M/S EQ SharedUI.cpp:5241-5263, static main-APVTS
:10972-11015). The main-APVTS `tk_` mirror set (`registerParamsForTrack`,
PluginProcessor.cpp:5911+) is id-mismatched with the knobs' engine-tagged ids and has zero
audio-path consumers — a dead, automatable-to-nowhere param family.

componentID census (scout, per editor): BaySickSolstice/BaySickSynth/BaySickBass/VibePlayer tag; **NAMIR,
Vocal (+Align/Pitch sub-editors), Pedals tag nothing** (no Automate menu at all); sfizz engines
(Guitars/Basses/RustyDrums) have no JUCE editor surface. VibePlayer's `cutSelfMode` has an
attachment but no componentID. G3's rework gated both writers to song mode and added mode
baselines (processor `mAutomationBaseline` for main-APVTS lanes; editor `mApplicatorBaseline`
for reader-backed applicator lanes) — new engine applicators inherit that machinery by
registering readers.

BLU-492 reality: instrument-engine combos are ALL already APVTS-backed; true non-param combos
found so far = Pedals EQ-type picker ([BaySickPedalsEditor.cpp:125-129](Source/BaySickPedals/BaySickPedalsEditor.cpp:125),
tone state) + effect-panel meter-mode selectors (:458/:621, view state). Full per-panel selector
audit runs in Task 4.

- **Risk:** medium — applicator lifecycle across dynamic tabs, per-instance id collisions,
  PRESET-BREAK on new combo params.
- **Effort:** ~8-12 h (grew from ~6-10: mechanism + per-instance ids + selector audit + lock gate).
- **Dependencies:** none inside G4 (runs after QA-NativeDialogs per §6).

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| Marathon 18 | BLU-378/379/492 own this batch; PRESET-BREAK accepted | Locked 2026-07-08 |
| Docket 4=A | Automation writes to capture-gated vocal params are SUPPRESSED while that strip records; lane resumes after | The lock exists to keep clicks out of takes; a lane is just a second hand |
| Docket 5=A | Tone-affecting selectors become APVTS params (enter presets); view/display selectors stay local | Classification rule for the Task-4 audit |
| — | Application mechanism = per-editor applicator+reader registration (see below) | Implementation call, stated for R5 |

**Mechanism (implementation call, plain English):** engine knobs get automated the same way
effect-rack knobs already are — each engine editor registers, per control, a tiny "apply this
0-1 value" hook + a "read the current value" hook into the existing registry, keyed by the
control's componentID. The UI automation pass already drives that registry every tick in song
mode, and the G3 baseline system already snapshots/restores registry-backed lanes on mode
switches — so engine params inherit playback application, stopped-seek preview, and mode
baselines with zero new pipeline. (The alternative — teaching the audio-thread pass to search
ten engine APVTSes — duplicates the pipeline and gets none of the baseline machinery for free.)

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open — docket 2026-07-25 locked 4/5; mechanism + id scheme are implementation
(stated above and in Task 2).

## Files to modify

- Task 1: `Source/Standalone/SharedUI.h/.cpp` (VKnobAutomation registration plumbing reuse),
  `Source/BaySickSolstice/BaySickSolsticeEditor.cpp`, `Source/BaySickSynth/BaySickSynthEditor.cpp`,
  `Source/BaySickBass/BaySickBassEditor.cpp`, `Source/VibePlayer/VibePlayerEditor.cpp`
- Task 2: `Source/BaySickNAMIR/BaySickNAMIREditor.cpp`, `Source/BaySickVocal/BaySickVocalEditor.cpp`
  (+ Align/Pitch editors), `Source/BaySickPedals/BaySickPedalsEditor.cpp`,
  `Source/Standalone/InstPage.cpp` / `VoxPage.cpp` (instance-index plumb if not already exposed)
- Task 3: `Source/PluginProcessor.cpp` (`registerParamsForTrack` + `addParamsFor*` mirror set)
- Task 4: `Source/Standalone/EffectEditorPanels.cpp` (+ each panel's DSP header for new params
  as found), `Source/BaySickPedals/BaySickPedalsProcessor.h/.cpp` (EQ-type param)
- Task 5: `Source/VibePlayer/VibePlayerEditor.cpp` (cutSelfMode componentID) + audit-driven sites

## Tasks

### Task 1 — Engine-param application (the confirmed gap)

- [ ] In each of the four instrument-engine editors, at the existing componentID-stamping site
  (`wireMeta`/`wireID`/equivalent), also register applicator + reader for that id via the
  `VKnobAutomation` registration hooks (pattern: MixerTrackStrip.cpp:454-480):
  applicator = engine-apvts `setValueNotifyingHost` on the param; reader = current normalized
  value. Cover the attachment-bound combos and toggles the same way (they already carry ids).
- [ ] Mirror the unregister lifecycle exactly as MixerTrackStrip does on destruction — dynamic
  tabs create/destroy editors; a stale applicator into a dead engine is the failure mode.
  Verify by code-read that close-tab -> reopen-tab re-registers cleanly.
- [ ] Verify lane creation off an engine knob now round-trips: right-click -> Automate ->
  lane block appears -> song-mode playback drives the knob AND the sound.
- [ ] Confirm the G3 `mApplicatorBaseline` capture picks the new readers up automatically
  (it walks reader-backed lanes; no special-casing expected — verify by read).
- [ ] Build gate.

### Task 2 — componentID coverage: NAMIR / Vocal / Pedals (+ per-instance ids)

- [ ] Per-instance id scheme: bare-id engines (NAMIR `nam_*`, Pedals `bsp_*`, Vocal `bsv_*`)
  collide across multiple Inst/Vox tabs. Stamp componentIDs (and registry keys) with the
  owning page index — `inst{N}_` / `vox{N}_` prefix applied at editor construction (the page
  index is already plumbed for the vocal per-strip wiring via `voxInsert(pageIndex)`;
  mirror that for Inst). Lane display names resolve through
  `resolveAutomationDisplayName` — extend it to strip the instance prefix and show the tab name.
- [ ] NAMIR: set `VKnob::paramId` (+ registration) on all knobs/toggles; manual combos get
  componentIDs + applicators.
- [ ] Vocal editor + Align/Pitch sub-editors: componentIDs + registration on the raw sliders
  (now VibeSliders per QA-VibeSlider) and combos.
- [ ] **Capture-lock gate (4=A):** the vocal registrations for the gated set (ab_slot,
  realtime bypass, Key/Scale, Retune/Strength/Humanize/Throat, Formant, chain Bypass) wrap
  their applicator in an `onIsStripRecording()` check — write suppressed while THIS strip
  captures; next tick after capture ends applies the lane value naturally.
- [ ] Pedals: audit what the composed panels' knobs carry (they are EffectEditorPanels
  instances — their VKnob paramIds/applicators may target rack-param ids that don't exist on
  the pedals APVTS). Wire pedals-slot knobs to registry keys that reach the PEDAL DSP
  (per-instance prefixed), or record precisely why a surface stays non-automatable in running
  notes for the close entry.
- [ ] Build gate.

### Task 3 — Retire the dead `tk_` mirror set

- [ ] Grep-confirm `registerParamsForTrack` + its `addParamsFor*` helpers register ONLY the
  mirror family (the consumed `_arm`/`_playNote` reads at :4657/:5718 belong to the mixer
  param family, not this set — verify).
- [ ] Remove the registration calls + helpers; grep zero remaining refs. Old projects carrying
  saved mirror values load fine (values for unregistered params are ignored); note this in the
  running notes + §B regression scenario.
- [ ] Build gate.

### Task 4 — BLU-492 selector audit (rule 5=A)

- [ ] Enumerate every ComboBox/ChickenHeadSelector across all effect panels + pedals; classify
  tone vs view; table into running notes.
- [ ] Tone-state selectors lacking an APVTS twin get a param + attachment (PRESET-BREAK
  accepted — lands before §E preset walk + QA-Templates). Known: Pedals EQ-type picker ->
  new pedals param. Known view-state: meter-mode selectors stay local.
- [ ] Spot-verify preset round-trip on each converted selector (save preset -> change -> load ->
  selector restored).
- [ ] Build gate.

### Task 5 — BLU-378/379 closure sweep

- [ ] componentID coverage check: every automatable control across 10 engines + panels + mixer
  offers the Automate menu (add stragglers — known: VibePlayer `cutSelfMode`).
- [ ] BLU-379: spot-verify SliderAttachment sync (UI <-> param) per editor family; flag any
  manual-push site that double-writes against an attachment.
- [ ] Build gate.

## Batch close (bulk-run per-batch loop — one commit per batch)

- [ ] Tell Jeff to run `do_build.bat`; fix until BOTH configs build clean.
- [ ] Author this batch's Master Test Plan §B section from the Verification list below
  (`blocks:` = this batch's commit, hash backfilled at commit).
- [ ] `/draft-doc batch-close` -> append under `## Held Work Log entry (apply at section pass)`
  in the running notes. Do NOT touch the Implemented Work Log or the §5 STATUS line now (R2).
- [ ] Append the running-notes code-complete entry (+ any Rule 4 catalog rows, same edit pass).
- [ ] ONE batch commit (Rule 9): `QA-ApvtsAutomation: <one-line what> (<scope>)` +
  `Co-Authored-By` trailer; surface message + FULL git status; commit only on Jeff's approval.

## Verification (authors into Master Test Plan §B)

1. BaySickSolstice macro knob: right-click -> Automate -> draw a ramp -> song plays: knob moves AND
   timbre audibly follows; stop + seek mid-lane: knob snaps to lane value.
2. Same end-to-end for one BaySickSynth, one BaySickBass, one BaySickPlayer param.
3. Two Layers tabs, same engine type: lanes on each drive ONLY their own tab (per-instance ids).
4. NAMIR knob + Vocal Retune + a Pedals knob: Automate menu now offered; lane drives the sound.
5. Capture-lock: arm a Vox strip, record; an active ab_slot lane does NOT flip the chain
   mid-take; on stop, the lane value applies. Same spot-check for Retune.
6. Mode baselines: set engine knob, enter song mode with a lane, play, exit song mode: knob
   returns to its hand-set value (G3 baseline machinery covers new lanes).
7. Converted selector (Pedals EQ type): automate-menu N/A but preset save/load round-trips;
   old preset (pre-param) loads with default + no error.
8. Old project with saved `tk_` mirror values loads clean; Event Editor lane browser no longer
   lists the dead mirror ids.
9. Regression: mixer fader lane + rack knob lane + M/S EQ lane still apply (registry untouched
   for existing families).

## Routing notes (Rule 3)

Undo/dirty interactions of automated writes (transactions, dirty pointer) belong to
QA-UndoCoverage/QA-DirtyFlag (later this group) — log, don't fix here. Preset-format findings
route into the §E campaign prep notes.

## Carry-Forward Reference touch points

§1 automation/registry primitives + the G3 running notes' automation section
(`burly-restringing-bison.md` mode-baseline bullets) before Task 1. Audio-thread rules §2 —
applicators run message-thread only; no audio-thread writes added by this batch.
