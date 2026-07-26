# Running Notes — QA-ApvtsAutomation (wired-lassoing-crane)

> Append-only mid-batch log. Entries land at every checkpoint (commit landed / finding captured
> / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At code-complete, `/draft-doc batch-close` consumes this file to compile the Implemented Work
> Log entry, which is HELD here (bulk-run R2) and applied at the batch's campaign section pass.
>
> Pair file: [`Plans & Specs/Batch Plans/wired-lassoing-crane.md`](../Batch%20Plans/wired-lassoing-crane.md).
> Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11).

## 2026-07-25 — Task 1 — Engine-param application (the confirmed gap)

**Done.** Applicator + reader registration added so instrument-engine lanes actually apply.
Both build configs clean (`RELEASE_EXIT_CODE=0` / `DEBUG_EXIT_CODE=0`).

- Three shared helpers added in `namespace VKnobAutomation`
  ([SharedUI.h](../../Source/Standalone/SharedUI.h) decls,
  [SharedUI.cpp:1717](../../Source/Standalone/SharedUI.cpp:1717) defs):
  `registerSliderAutomation` / `registerButtonAutomation` / `registerSelectorAutomation`.
  Chosen over pasting the ~20-line MixerTrackStrip block into 7 sites; Task 2 reuses all three.
- Registration wired at the single componentID-stamping lambda in each of the four instrument
  editors — `wireMeta` ([HarmlessEditor.cpp:476](../../Source/Harmless/HarmlessEditor.cpp:476))
  and `wireID` ([BaySickSynthEditor.cpp:389](../../Source/BaySickSynth/BaySickSynthEditor.cpp:389),
  [BaySickBassEditor.cpp:383](../../Source/BaySickBass/BaySickBassEditor.cpp:383),
  [VibePlayerEditor.cpp:212](../../Source/VibePlayer/VibePlayerEditor.cpp:212)).
- Two surfaces the plan's per-editor framing did not cover, both found by census:
  - [HarmlessFilterRow.cpp:47-57](../../Source/Harmless/HarmlessFilterRow.cpp:47) — 8 stamped,
    attached knobs (2 rows x freq/res/env/kbTrack) live in a sub-component file, not the editor.
  - [VibePlayerEditor.cpp:253-255](../../Source/VibePlayer/VibePlayerEditor.cpp:253) — the two
    `DualLabelToggle` buttons + the `ChickenHeadSelector` detune-mode picker (plan Task 1's
    "attachment-bound combos and toggles" bullet).

**Implementation calls (no user-visible choice to surface):**

- Applicator drives the CONTROL, not the parameter — the existing attachment carries the value
  to the engine APVTS. Matches the two pre-existing registration sites
  ([MixerTrackStrip.cpp:454](../../Source/Standalone/MixerTrackStrip.cpp:454),
  [EffectEditorPanels.cpp:211](../../Source/Standalone/EffectEditorPanels.cpp:211)) and is
  unconditionally memory-safe (only ever touches a `SafePointer`-guarded Component).
- Range is read AT APPLY TIME, not captured at registration — deliberate deviation from the two
  sites above. Required by the Harmless dual A/B set (below) and by
  [VibePlayerEditor.cpp:253-255](../../Source/VibePlayer/VibePlayerEditor.cpp:253), where the
  componentID stamp precedes attachment construction.

**Scope addition (mine, beyond the plan):** engine-prefix erase on tab close, added to the
Layers / Bass / Drum branches of `closeTab` in
[StandaloneEditor.cpp](../../Source/Standalone/StandaloneEditor.cpp) alongside the existing
`mixer_*` / rack-slot erases. Without it every engine registration this task adds would survive
tab churn as a dead entry — unbounded map growth plus dead paramIds listed in the Event Editor
param browser, which is the exact regression `eraseAutomationEntriesWithPrefix` was written to
fix ([StandaloneEditor.h:738-742](../../Source/Standalone/StandaloneEditor.h:738)). Erase
prefixes `tk_lay_{i}_` / `tk_bas_{i}_` / `tk_drm_{i}_`; the trailing underscore prevents
`tk_lay_1_` from matching `tk_lay_10_*`.

**Verified by read (plan checkboxes, no code change needed):**

- G3 mode baselines pick up the new readers with zero special-casing —
  [StandaloneEditor.cpp:882-909](../../Source/Standalone/StandaloneEditor.cpp:882) walks every
  lane, skips main-APVTS ids, and baselines anything with a registered reader.
- Close-tab -> reopen re-registers cleanly: both registration hooks assign via
  `map[pid] = std::move(fn)` ([StandaloneEditor.cpp:632](../../Source/Standalone/StandaloneEditor.cpp:632)
  and :639), so a rebuilt editor overwrites any stale key.
- Engine componentIDs are ALREADY per-instance — `pid()` resolves through each engine
  processor's instance `mPrefix`, e.g. `"tk_" + trackId + "_bss_"`
  ([BaySickSynthProcessor.cpp:10](../../Source/BaySickSynth/BaySickSynthProcessor.cpp:10)).
  Plan Verification scenario 3 (two Layers tabs, same engine, lanes stay independent) needs no
  extra work in this task.
- No id collision with the dead `tk_` mirror set, so Task 1's applicators are not shadowed by
  the main-APVTS branch at [StandaloneEditor.cpp:3307](../../Source/Standalone/StandaloneEditor.cpp:3307)
  and Task 1 is testable before Task 3 lands.

## 2026-07-25 — Task 1 addendum — Harmless Part A/B spec call RESOLVED (Jeff)

**Asked mid-Task-1** once registration made the latent ambiguity real. **Answer: separate lanes
per part; do NOT automate the A/B selector.** Jeff's reasoning, verbatim in substance: Part B is
a second layer that plays *simultaneously* with Part A, not an alternate mode — so both parts'
params should be independently automatable rather than the lane following a view toggle.

My original framing was wrong and he corrected it: I described A/B accurately as a shared-knob
view toggle, then posed the options as though switching parts were a sound decision. It is not.

**Facts established while answering his "is the A/B itself automatable?" question:**

- `part_sel` IS a real APVTS param ([HarmlessProcessor.cpp:518](../../Source/Harmless/HarmlessProcessor.cpp:518),
  Int 0-1) and WAS componentID-stamped, so it advertised "Automate: Part Select".
- **No DSP anywhere reads `part_sel`.** Both parts always render; the audible A->B control is
  `timbre_blend`. The pre-existing tooltip said as much ("editor-side, no DSP effect today").
- `rebindToPart()` is called ONLY from the two A/B button `onClick`s and once at editor
  construction ([HarmlessEditor.cpp:296-313](../../Source/Harmless/HarmlessEditor.cpp:296), :600).
  There is no parameter listener, so writing `part_sel` from any other source (automation, preset
  load) does not flip the editor.
- `mPartSel` is `addAndMakeVisible`'d ([:261](../../Source/Harmless/HarmlessEditor.cpp:261)) but
  never given bounds in `resized()`, so it renders 0x0. The comment at :293-294 claiming it is
  "invisible by virtue of never being addAndMakeVisible'd" is **wrong about the mechanism**
  though right about the outcome. Minor; left alone (untouched region, Rule 6 scoping).

**Implemented:**

1. New `VKnobAutomation::registerParameterAutomation (paramId, RangedAudioParameter&,
   Component& lifetimeGuard)` ([SharedUI.cpp](../../Source/Standalone/SharedUI.cpp)) — writes the
   PARAM, not a control. Required here: one knob is time-shared between two params, so a
   knob-driven applicator writes whichever part is bound and collapses two lanes onto one target.
   Lifetime guard is the shared control; engine editors are destroyed BEFORE their processor
   ([LayersPage.cpp:49-52](../../Source/Standalone/LayersPage.cpp:49), :151-154), so a live guard
   proves the param pointer is valid — this matters because an engine SWAP leaves stale keys that
   the tab-close erase does not cover.
2. Applicators + readers registered for BOTH `paramA` and `paramB` of all 8 `mDualSliders` and
   the 1 `mDualButton`, once at construction. Deliberately supersedes the knob-driven
   registrations `wireMeta` made for the Part A ids (later `map[id] =` assignment wins).
3. `rebindToPart()` now retargets each dual control's componentID to the visible part, so
   right-click Automate creates a lane for the part being edited. Registrations are untouched by
   the rebind, so both lanes keep working regardless of which part is on screen.
4. **`part_sel` stamp removed** — tooltip retained, componentID dropped. Kills an Automate menu
   entry whose lane could never do anything. Matches docket 5=A (view selectors stay local).

**Open item deferred to Jeff, not blocking:** `timbre_blend` is the real audible A->B crossfade
and is already stamped + now applying. If the intent was "sweep between the two timbres", that
control already covers it — flagged to him in chat.

## 2026-07-25 — Task 2 — NAMIR / Vocal / Pedals (+ per-instance ids) — IN PROGRESS

**Per-instance id premise CONFIRMED** (unusual for G4 — most premises this group were void).
NAMIR ids are bare literals (`nam_bypass`, and notably un-prefixed generics `input_gain`,
`output`, `low_cut`, `oversampling` — [BaySickNAMIRProcessor.cpp:80-102](../../Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:80));
vocal ids are bare `bsv_*` ([BaySickVocalEditor.cpp:26](../../Source/BaySickVocal/BaySickVocalEditor.cpp:26)).
Each Inst page builds its OWN NAMIR processor+apvts
([InstPage.cpp:84](../../Source/Inst/InstPage.cpp:84)) and `kMaxInstPages = 20` /
`kMaxVoxPages = 6` ([VibesynthConstants.h:17-18](../../Source/VibesynthConstants.h:17)), so a
shared registry key would let the last-built tab win every lane. **Checked and clear:** none of
the bare NAMIR ids exist in the MAIN apvts, so no lane is hijacked by the main-APVTS branch.

**Premise corrections (3):**

1. **NAMIR does NOT "tag nothing".** Its 7 `bindKnob` knobs already set `VKnob::paramId`
   ([BaySickNAMIREditor.cpp:190](../../Source/BaySickNAMIR/BaySickNAMIREditor.cpp:190)) and so
   already offer an Automate menu. What was missing is REGISTRATION — the lanes did nothing.
2. **Pedals knobs do not carry wrong rack ids — they carry NO ids.** `setSlotContext` (the
   function that stamps paramIds AND registers applicators for effect panels) is called from
   exactly one site, the FX rack page ([EffectsPage.cpp:842](../../Source/Standalone/EffectsPage.cpp:842)).
   The pedals editor builds the same panels via `createEffectEditor`
   ([BaySickPedalsEditor.cpp:219](../../Source/BaySickPedals/BaySickPedalsEditor.cpp:219)) and
   never calls it. Pedal knob values are not params at all — the pedals apvts "only holds bypass
   + per-pedal CC params" ([BaySickPedalsProcessor.h:91-93](../../Source/BaySickPedals/BaySickPedalsProcessor.h:91)) —
   and pedals slots have **no uuid** (:105), which the FX rack relies on to keep lanes valid
   across reorder (C13).
3. **The capture-lock gate list in the plan is stale.** Actual gated set from
   [BaySickVocalEditor.cpp:373-394](../../Source/BaySickVocal/BaySickVocalEditor.cpp:373) is
   9 controls; **chain Bypass is NOT among them** — the in-code comment records it "left the gate
   set with its removal" at QA-Fd.

**Spec call — pedals scope — ANSWERED: option B (Jeff).** Full job in this batch: add permanent
per-slot tags to pedals, then wire the knobs. He was told this affects saved pedal setups; per
`feedback_no_backward_compat_pre_v1` no migration is written — but pre-uuid saves MINT a fresh
uuid on restore rather than restoring empty, so old pedalboards load and are automatable; only
their pre-existing lanes (of which there are none, since pedals were never automatable) would be
affected. **DONE, build-clean.**

Pedals implementation (mirrors `EffectRack::Slot::uuid` / C13 exactly):

- `BaySickPedalsProcessor::Slot` gained `juce::String uuid` + move-ctor/assignment carry
  ([BaySickPedalsProcessor.h](../../Source/BaySickPedals/BaySickPedalsProcessor.h)). This is what
  makes reorder safe: `moveSlot` moves whole `Slot` objects, so the identity travels with the
  pedal instead of the index.
- `loadEffect` gained `uuidOverride` (fresh uuid on a user swap, saved uuid on restore);
  `clearSlot` clears it; `captureFullState` / `restoreFullState` persist it.
- `getSlotUuid(slot)` accessor added.
- `BaySickPedalsEditor::setAutomationPrefix()` + `getAutomationPrefix()`; each
  `PedalSlotComponent` gained `applyAutomationContext()` which calls the panels' existing
  `setSlotContext (prefix, uuid)` — the same call the FX rack makes, which stamps paramIds AND
  registers applicators. Called from `rebuild()` and retroactively from `setAutomationPrefix`
  (tiles are constructed before the page supplies the prefix).
- Wired from [InstPage.cpp](../../Source/Inst/InstPage.cpp) as `inst{N}_pedals`.

Net: pedal knobs are automatable for the first time, keyed `inst{N}_pedals_{uuid}_{knob}`, and the
lanes survive both pedal reordering and project reload.

**Done + build-clean:**

- `registerParameterAutomation` gained an optional `suppressWhen` predicate for the capture lock.
- **NAMIR** `setAutomationPrefix(prefix)` ([BaySickNAMIREditor.cpp](../../Source/BaySickNAMIR/BaySickNAMIREditor.cpp)):
  registers every param under `prefix + paramID` and re-stamps all 15 knobs / 3 toggles /
  5 selectors / 2 combos with the prefixed id. Wired from
  [InstPage.cpp](../../Source/Inst/InstPage.cpp) as `inst{N}_`.
- **Vocal** `setAutomationPrefix(prefix)` — registers every `bsv_*` param under the prefix, with
  the 9 capture-gated ids wrapped in an `onIsStripRecording()` veto; forwards the prefix to the
  hosted NAM/IR editor. Wired from [VoxPage.cpp:436](../../Source/Vox/VoxPage.cpp:436) as `vox{N}_`.
  Vox reaches NAM/IR through `BaySickVocalEditor::NAMIRHostPanel`, not directly — the InstPage.h
  comment claiming Vox hosts it the same way as Inst is misleading.

**Registration deliberately walks each engine's own parameter list** instead of a per-control
table: coverage is complete by construction and cannot rot as params are added. This is sound
because the Event Editor's param browser enumerates the applicator registry directly
([StandaloneEditor.cpp:3184](../../Source/Standalone/StandaloneEditor.cpp:3184)) — a registered
param is discoverable and automatable even with no componentID on its control, so the per-control
stamp is right-click convenience rather than a functional requirement.

**Task 2 leftovers — BOTH CLOSED 2026-07-25 on Jeff's "fix 3 and 4" instruction:**

1. **Vocal componentID stamps — done.** `VocalChainPanel` now stamps the bare param id on all 9
   automatable controls, and `BaySickVocalEditor::setAutomationPrefix` re-stamps the whole child
   tree with `vox{N}_`. Written as a recursive walk rather than a per-control list precisely
   because the controls live in private nested classes — the walk picks up any stamp added to the
   Align / Pitch sub-editors later without touching that function again. The NAM/IR subtree is
   skipped (its own editor prefixed it moments earlier; re-prefixing would double it).
2. **`resolveAutomationDisplayName` prefix stripping — done.** Strips a leading `inst{N}_` /
   `vox{N}_`, resolves the remainder through the existing renderer, and prepends the tab label, so
   a lane reads `Inst 4 - nam_bypass` rather than `inst3_nam_bypass`. Pedals keys get a dedicated
   branch that DROPS the 32-hex slot uuid — it exists for reorder-stability and must never surface
   in the UI — yielding `Inst 4 - Pedals - drive`.

## 2026-07-25 — Task 3 — Retire the dead `tk_` mirror set — DONE (build 0/0)

Scope came out LARGER than the plan's framing, because the audit found a second dead family
alongside the engine mirrors.

**Removed:** `registerParamsForTrack`, `unregisterParamsForTrack`, `isTrackRegistered`,
`addParamsForHarmless`, `addParamsForVibePlayer`, `addParamsForBaySickSynth`,
`addParamsForBaySickBass`, `addParamsForEffectRack` (decls + defs), plus the six call sites in
LayersPage / BassPage / DrumPage. Grep confirms zero live references remain.

- The four engine mirror helpers registered `tk_{trackId}_{param}` ids that were id-MISMATCHED
  with the ids the engine editors actually stamp (`tk_lay_0_oscMode` vs `tk_lay_0_bss_noise`) —
  automatable to nowhere, exactly as the plan described.
- **`addParamsForEffectRack` was equally dead and the plan did not flag it.** It registered
  6 slots x 15 params (`tk_{id}_rack_slot{s}_type/_bypass/_output/_p0..p11`) per track;
  `_rack_slot` has **zero readers** anywhere in the tree. Real racks live on InsertNodes under
  `mixer_*` with uuid-keyed automation ids. Removed under
  `feedback_clean_own_batch_dead_code_in_batch` — leaving it would have left an uncalled helper.
- **`mRegisteredTrackParams` deliberately KEPT.** `ensureMixerStripParams` and the EQ-bank
  helpers use it as their id accumulator. Safe because the two paths keyed it differently — the
  dead path by `trackId` ("lay_0"), the live path by mixer prefix ("mixer_layer_0") — so they
  never shared entries.
- Stale comments fixed in the edited regions per Rule 6: the "Lazy APVTS registration" header
  block in [PluginProcessor.cpp](../../Source/PluginProcessor.cpp) (described the removed naming
  conventions and `isTrackRegistered`), plus the identical stale header comment in
  [LayersPage.h:20](../../Source/Standalone/LayersPage.h:20) and
  [BassPage.h:20](../../Source/Standalone/BassPage.h:20).

Old projects carrying saved mirror values load fine — APVTS ignores values for unregistered
params. Covered by §B.27 scenario 8.

## 2026-07-25 — Task 4 — BLU-492 selector audit (rule 5=A) — DONE, ZERO conversions needed

**BLU-492's premise is VOID.** The item assumed tone-affecting selectors "lack an APVTS twin" and
therefore do not persist. They persist — just not through APVTS. Effects are serialized through
each DSP's own `getStateInformation` ValueTree, and every tone selector's backing field is in it.
Verified field-by-field, not sampled:

| Selector | Panel | Backing field | Serialized at |
|---|---|---|---|
| ratioSel / All-in | Compressor | `ratio`, `fetAllButtons` | [CompressorDSP.cpp:837](../../Source/DSP/CompressorDSP.cpp:837), :857 |
| kneeSel | Compressor | `kneeType` | CompressorDSP.cpp:844 |
| Type | Compressor | `type` | CompressorDSP.cpp:856 |
| modeSel | Reverb | `mode` | [ReverbDSP.cpp:1020](../../Source/DSP/ReverbDSP.cpp:1020) |
| tailShapeSel | Reverb | `tailModShape` | ReverbDSP.cpp:1040 (restored :1090) |
| syncDivSel | Reverb / Phaser / Flanger | `syncDivIdx` | ReverbDSP.cpp:1052, PhaserDSP.cpp:415, FlangerDSP.cpp:361 |
| tubeTypeSel | Saturation | `tubeType` | [SaturationDSP.cpp:653](../../Source/DSP/SaturationDSP.cpp:653) |
| harmModeSel | Saturation | `harmonicsMode` | SaturationDSP.cpp:664 |
| osSel | Saturation / Overdrive / Tape / TransientShaper | `osLog2` | SaturationDSP.cpp:659, OverdriveDSP.cpp:477, TapeDSP.cpp:494, TransientShaperDSP.cpp:445 |
| lfoWaveSel | Phaser | `lfoWaveIdx` | [PhaserDSP.cpp:414](../../Source/DSP/PhaserDSP.cpp:414) |
| shape | Flanger | `shape` | FlangerDSP.cpp:363 |
| fbFilterTypeSel | Delay | `fbFilterType` | [DelayDSP.cpp:946](../../Source/DSP/DelayDSP.cpp:946) (restored :1020) |
| meterSel (x2) | Compressor / Limiter | none — DISPLAY ONLY | correctly local per 5=A |

**The plan's one named conversion target is also void.** The Pedals EQ-type picker
([BaySickPedalsEditor.cpp:125-129](../../Source/BaySickPedals/BaySickPedalsEditor.cpp:125)) does
not hold separate tone state at all: `onEqPickerChanged` calls `loadEffect` to change the SLOT
TYPE (:524-535), the slot type is persisted in `captureFullState`, and `rebuild()` re-syncs the
picker from it on load (:196-203). It already round-trips.

**Net: no selector needs an APVTS param, so the pre-accepted PRESET-BREAK (marathon 18) is not
spent.** Nothing in the preset format changed in this task. Docket 5=A's classification rule was
applied and every selector already lands on the correct side of it.

## 2026-07-25 — Task 5 — BLU-378/379 closure sweep — DONE (build 0/0)

- **BaySickPlayer `cutSelfMode`** (the plan's named straggler): attached at
  [VibePlayerEditor.cpp:265](../../Source/VibePlayer/VibePlayerEditor.cpp:265) but never stamped —
  the only control in that editor with no Automate menu. ComponentID + button registration added
  alongside its `reverse` / `cutSelf` siblings.
- **Harmless `lfo_rate` + `lfo_shape`**: two VISIBLE, attached, DSP-consumed knobs that offered no
  Automate menu purely because of a false comment. The S4-era note read "lfo_rate / lfo_shape
  wireMeta removed (params ripped)" — the params were never ripped: registered at
  [HarmlessProcessor.cpp:494-495](../../Source/Harmless/HarmlessProcessor.cpp:494), read by the DSP
  at :960-961, attached at [HarmlessEditor.cpp:406-407](../../Source/Harmless/HarmlessEditor.cpp:406).
  Both stamped; the false comment replaced with the evidence (a wrong comment gets fixed, not
  scoped — `feedback_no_docs_only_commit_fix_wrong_comments`).
- **BLU-379 (attachment sync):** no double-writer found. Every applicator this batch registers
  either drives a control that owns exactly one attachment, or drives the parameter directly
  (Harmless dual A/B, NAMIR, Vocal). The one manual-push family — the NAMIR combo/selector
  `onChange` handlers that call `setValueNotifyingHost` by hand — has no competing attachment, so
  there is nothing to double-write against.

**Straggler follow-up — Jeff asked for the list, then 11 of 14 were closed the same sitting.**
The Task 1 census count of "~14 other Harmless controls" contained two errors, corrected on a
re-read of the source:

- `pluck_blur` was counted as unstamped, but it is a Part A/B DUAL button — the A/B rework already
  stamps and registers it via `rebindToPart`. Not a straggler.
- The "4 hidden shape sliders" are not user-facing controls at all. Not stragglers.

Actual set and disposition:

| Controls | Param ids | State |
|---|---|---|
| 6 `HarmlessRoutingMatrix` faders | `rm_sub` `rm_prot` `rm_clip` `rm_fx` `rm_vol` `rm_env` | **stamped + registered** |
| 3 `HarmlessXYZPad` knobs | `mod_x` `mod_y` `mod_z` | **stamped + registered** |
| 2 filter-row type selectors | `flt1_type` `flt2_type` | **stamped + registered** |
| 5 toggle buttons | `legato` `unison_alt` `vel_link` `cutSelf` `cutSelfMode` | **stamped + registered** |

Both `attachToApvts` methods already receive the param ids as arguments, so stamping landed at the
attachment site with no plumbing. The filter-row type selectors needed a new
`VKnobAutomation::registerComboAutomation` (juce::ComboBox twin of the existing selector helper;
0..1 maps across item INDEX so a lane sweep steps the list).

**Buttons added on Jeff's "add the toggle buttons" (2026-07-25) — and the count was wrong AGAIN.**
I had reported 4; there are **5**. Harmless has its OWN `cutSelfMode` button
([HarmlessEditor.cpp:454](../../Source/Harmless/HarmlessEditor.cpp:454)) distinct from the
BaySickPlayer `cutSelfMode` stamped earlier in Task 5 — same suffix, different engine, and I had
mentally de-duplicated them. Third counting error in this straggler set; every one came from
trusting the census summary instead of re-reading the source. All five now stamped + registered.
**Harmless straggler set is now empty.**

### Found along the way

1. **CLAUDE.md was factually wrong about `oeq_mix`** — it claimed the param is absent from
   `createLayout` and the attachment commented out. Both were fixed 2026-04-19 (T1a): param
   registered [HarmlessProcessor.cpp:319](../../Source/Harmless/HarmlessProcessor.cpp:319),
   attachment live [HarmlessEditor.cpp:388](../../Source/Harmless/HarmlessEditor.cpp:388),
   consumed at :839. **Fixed in this batch** — bullet replaced with the dual A/B gotcha (2).
   Census confirms Harmless has ZERO phantom stamped ids across all 60 `wireMeta` calls.
2. **Harmless dual A/B controls carry a componentID that is not a stable pointer to what they
   drive.** [`rebindToPart()`](../../Source/Harmless/HarmlessEditor.cpp:611) destroys and
   recreates the attachment for 8 sliders + 1 button against either the Part A or Part B param.
   The stamped id is always Part A's, so an automation lane (and MIDI Learn) follows whichever
   part is on screen rather than the part the lane was created against. Mode-baseline
   capture/restore has the same exposure if the part is switched mid-song. Pre-existing;
   registration inherits it rather than causing it. **Surfaced as a spec call and FIXED** — see
   the Task 1 addendum entry above (separate lanes per part). The CLAUDE.md "Harmless-specific"
   bullet was rewritten twice this batch: first to replace the stale `oeq_mix` claim with this
   gotcha, then again once the fix shipped, since the fix invalidated the gotcha's own wording.
3. **Automated writes push a parameter gesture per automation tick (30 Hz).** Slider-driven
   writes go through the attachment; `detuneMode` goes through
   `ParameterAttachment::setValueAsCompleteGesture`
   ([VibePlayerEditor.cpp:307](../../Source/VibePlayer/VibePlayerEditor.cpp:307)). Undo-stack and
   dirty-flag consequences belong to QA-UndoCoverage / QA-DirtyFlag per this plan's Routing
   notes — **logged, not fixed**.
4. **Task 5's straggler list is far larger than "known: VibePlayer `cutSelfMode`".** Harmless
   alone has ~16 attached-but-unstamped controls: the 6 `HarmlessRoutingMatrix` sliders, the 3
   `HarmlessXYZPad` knobs, both filter-row type combos, 6 buttons (`legato`, `unison_alt`,
   `vel_link`, `cutSelf`, `cutSelfMode`, `pluck_blur`), and 4 hidden shape sliders. Two of them —
   `mLfoRate` / `mLfoShape` — are VISIBLE knobs skipped because of a stale comment at
   [HarmlessEditor.cpp:532](../../Source/Harmless/HarmlessEditor.cpp:532) claiming the params
   were ripped; both params exist ([HarmlessProcessor.cpp:494-495](../../Source/Harmless/HarmlessProcessor.cpp:494)).
   `cutSelfMode` confirmed attached-but-unstamped at
   [VibePlayerEditor.cpp:262](../../Source/VibePlayer/VibePlayerEditor.cpp:262).
   **Process miss (mine):** this line said the full cross-engine count + the hidden-control
   question would go to Jeff as a docket at Task 5 open. That docket was never posed — at Task 5
   I stamped the three highest-value stragglers and deferred the rest on my own call. The
   deferral is recorded in the Task 5 entry and surfaced to Jeff on the commit, but it should
   have been asked when Task 5 opened rather than reported after.

## Held Work Log entry (apply at section pass)

> Drafted at code-complete via `/draft-doc batch-close`; NOT yet applied to `Implemented Work Log.md`.
> Applies with the §5 STATUS flip when §B.27 passes the campaign walk (R2). Backfill the commit hash
> (here and in the §B.27 `blocks:` line) at commit; stamp the full `HH:MM PT` at apply.

### 2026-07-25 <HH:MM> PT — QA-ApvtsAutomation — The founding gap is closed: instrument-engine automation lanes now APPLY (applicator + reader registration across all four engine editors and the two sub-component surfaces the plan's per-editor framing missed); Harmless Part A and Part B became independently automatable per owner call and `part_sel` was de-stamped; NAMIR / Vocal / Pedals reach the registry for the first time under per-instance keys, with pedals given permanent per-slot uuids (owner option B) and the vocal capture lock vetoing lane writes mid-take; the dead `tk_` engine mirror set retired along with a second dead family (`tk_*_rack_slot*`) the plan never flagged; BLU-492 closed with ZERO conversions. Four plan premises turned out void or wrong and the pre-accepted PRESET-BREAK was never spent

**Bucket:** Players, Cross-cutting Infrastructure. Batch `wired-lassoing-crane`. `blocks:` `<hash>`.
*(Main Plan §5 and the §6 footnote pre-assigned "Cross-cutting Infrastructure, UI / L&F / Theming"
back when this batch was framed as an audit sweep. Nothing in the diff touches VibeLAF, palette,
theme or layout. What shipped is the four instrument-engine editors plus BaySickNAMIR /
BaySickVocal / BaySickPedals (Players) and the `VKnobAutomation` registry, `PluginProcessor` param
registration and the `StandaloneEditor` tab-close lifecycle (Cross-cutting Infrastructure).
Correcting the §5 + §6 lines in place is Jeff's call, per the QA-NativeDialogs precedent. Whether
**Effects** belongs as a third bucket is also his call — the batch modifies the pedals processor
and reaches `EffectEditorPanels::setSlotContext`, and Task 4 audited every effect panel's
selectors; QA-NativeDialogs took Effects for a smaller pedals touch the same week.)*

#### Done

- **Task 1 — engine-param application, the confirmed founding gap.** Three shared helpers added in `namespace VKnobAutomation` (`registerSliderAutomation` / `registerButtonAutomation` / `registerSelectorAutomation`) instead of pasting the ~20-line MixerTrackStrip block into seven sites; Task 2 reuses all three. Registration wired at the single componentID-stamping lambda in each of the four instrument editors — `wireMeta` ([HarmlessEditor.cpp:476](Source/Harmless/HarmlessEditor.cpp:476)) and `wireID` ([BaySickSynthEditor.cpp:389](Source/BaySickSynth/BaySickSynthEditor.cpp:389), [BaySickBassEditor.cpp:383](Source/BaySickBass/BaySickBassEditor.cpp:383), [VibePlayerEditor.cpp:212](Source/VibePlayer/VibePlayerEditor.cpp:212)). **Two surfaces the plan's per-editor framing did not cover, both found by census:** [HarmlessFilterRow.cpp:47-57](Source/Harmless/HarmlessFilterRow.cpp:47) (8 stamped, attached knobs living in a sub-component file, not the editor), and [VibePlayerEditor.cpp:253-255](Source/VibePlayer/VibePlayerEditor.cpp:253) (two `DualLabelToggle` buttons + the `ChickenHeadSelector` detune-mode picker).
- **Task 1 — two implementation calls (no user-visible choice to surface).** (a) The applicator drives the CONTROL, not the parameter — the existing attachment carries the value to the engine APVTS. Matches the two pre-existing registration sites ([MixerTrackStrip.cpp:454](Source/Standalone/MixerTrackStrip.cpp:454), [EffectEditorPanels.cpp:211](Source/Standalone/EffectEditorPanels.cpp:211)) and is unconditionally memory-safe (only ever touches a `SafePointer`-guarded Component). (b) Range is read AT APPLY TIME rather than captured at registration — a deliberate deviation from those two sites, required by the Harmless dual A/B set and by VibePlayer, where the componentID stamp precedes attachment construction.
- **Task 1 — scope addition beyond the plan (mine): engine-prefix erase on tab close.** Added to the Layers / Bass / Drum branches alongside the existing `mixer_*` / rack-slot erases, with prefixes `tk_lay_{i}_` / `tk_bas_{i}_` / `tk_drm_{i}_` (the trailing underscore stops `tk_lay_1_` matching `tk_lay_10_*`). Without it every engine registration this task adds would survive tab churn as a dead entry — unbounded map growth plus dead paramIds listed in the Event Editor param browser, the exact regression `eraseAutomationEntriesWithPrefix` was written to fix. Covered by §B.27 AP-17.
- **Task 1 — four plan checkboxes closed by code-read, no change needed.** G3 mode baselines pick the new readers up with zero special-casing ([StandaloneEditor.cpp:882-909](Source/Standalone/StandaloneEditor.cpp:882) walks every lane, skips main-APVTS ids, baselines anything with a registered reader). Close-tab -> reopen re-registers cleanly (both hooks assign via `map[pid] = std::move(fn)`). Engine componentIDs were ALREADY per-instance — `pid()` resolves through each engine processor's instance `mPrefix` (`"tk_" + trackId + "_bss_"`), so the plan's "two Layers tabs, same engine" scenario needed no extra work. And no id collides with the dead `tk_` mirror set, so Task 1's applicators were never shadowed and were testable before Task 3 landed.
- **Task 1 addendum — Harmless Part A/B: OWNER SPEC CALL, separate lanes per part.** Asked mid-task once registration made the latent ambiguity real. Jeff's answer: **both parts are simultaneous layers, not alternate modes**, so each part's param owns an independent lane; the A/B selector itself is NOT automatable. My original framing was wrong and he corrected it — I described A/B accurately as a shared-knob view toggle, then posed the options as though switching parts were a sound decision. It is not. Implemented as: (1) new `VKnobAutomation::registerParameterAutomation (paramId, RangedAudioParameter&, Component& lifetimeGuard)` which writes the PARAM, not a control — required because one knob is time-shared between two params, so a knob-driven applicator writes whichever part is bound and collapses two lanes onto one target; the lifetime guard is the shared control, and engine editors are destroyed BEFORE their processor, so a live guard proves the param pointer is valid (this matters because an engine SWAP leaves stale keys the tab-close erase does not cover). (2) Applicators + readers registered for BOTH `paramA` and `paramB` of all 8 `mDualSliders` and the 1 `mDualButton`, once at construction, deliberately superseding the knob-driven registrations `wireMeta` made for the Part A ids. (3) `rebindToPart()` now retargets each dual control's componentID to the visible part, so right-click Automate creates a lane for the part being edited; registrations are untouched by the rebind, so both lanes keep working regardless of which part is on screen. (4) **`part_sel` stamp removed** (tooltip retained) — it killed an Automate menu entry whose lane could never do anything, matching docket 5=A.
- **Task 1 addendum — what the A/B question established in code.** `part_sel` IS a real APVTS param ([HarmlessProcessor.cpp:518](Source/Harmless/HarmlessProcessor.cpp:518), Int 0-1) and WAS stamped, so it advertised "Automate: Part Select" — but **no DSP anywhere reads it**; both parts always render and the audible A->B control is `timbre_blend` (already stamped and now applying). `rebindToPart()` is called only from the two A/B `onClick`s and once at construction, with no parameter listener, so writing `part_sel` from automation or a preset load never flips the editor.
- **Task 2 — per-instance keys, premise CONFIRMED (unusual for this group).** NAMIR ids are bare literals, including the un-prefixed generics `input_gain` / `output` / `low_cut` / `oversampling` ([BaySickNAMIRProcessor.cpp:80-102](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:80)); vocal ids are bare `bsv_*`. Each Inst page builds its OWN NAMIR processor + apvts ([InstPage.cpp:84](Source/Inst/InstPage.cpp:84)) and `kMaxInstPages = 20` / `kMaxVoxPages = 6`, so a shared registry key would have let the last-built tab win every lane. Checked and clear: none of the bare NAMIR ids exist in the MAIN apvts, so no lane is hijacked by the main-APVTS branch.
- **Task 2 — NAMIR.** New `setAutomationPrefix(prefix)` registers every param under `prefix + paramID` and re-stamps all 15 knobs / 3 toggles / 5 selectors / 2 combos with the prefixed id. Wired from [InstPage.cpp](Source/Inst/InstPage.cpp) as `inst{N}_`.
- **Task 2 — Vocal + the capture lock (docket 4=A).** `registerParameterAutomation` gained an optional `suppressWhen` predicate; `BaySickVocalEditor::setAutomationPrefix(prefix)` registers every `bsv_*` param under the prefix with the 9 capture-gated ids wrapped in an `onIsStripRecording()` veto (the lane is not consumed — the next tick after the veto clears applies the current value normally), and forwards the prefix to the hosted NAM/IR editor. Wired from [VoxPage.cpp:436](Source/Vox/VoxPage.cpp:436) as `vox{N}_`. Vox reaches NAM/IR through `BaySickVocalEditor::NAMIRHostPanel`, not directly — the `InstPage.h` comment claiming Vox hosts it the same way as Inst is misleading.
- **Task 2 — Pedals, OWNER SPEC CALL: option B, the full job.** Pedal slots got the permanent identity the FX rack already had, mirroring `EffectRack::Slot::uuid` / C13 exactly: `BaySickPedalsProcessor::Slot` gained `juce::String uuid` with move-ctor/assignment carry (this is what makes reorder safe — `moveSlot` moves whole `Slot` objects, so identity travels with the pedal instead of the index); `loadEffect` gained `uuidOverride` (fresh uuid on a user swap, saved uuid on restore); `clearSlot` clears it; `captureFullState` / `restoreFullState` persist it; `getSlotUuid(slot)` accessor added. On the editor side, `setAutomationPrefix()` / `getAutomationPrefix()` plus a per-tile `applyAutomationContext()` that calls the panels' existing `setSlotContext (prefix, uuid)` — the same call the FX rack makes, which stamps paramIds AND registers applicators — invoked from `rebuild()` and retroactively from `setAutomationPrefix` (tiles are constructed before the page supplies the prefix). Wired from [InstPage.cpp](Source/Inst/InstPage.cpp) as `inst{N}_pedals`. **Net: pedal knobs are automatable for the first time**, keyed `inst{N}_pedals_{uuid}_{knob}`, and the lanes survive both pedal reordering and project reload (§B.27 AP-9, MUST-PASS). Per `feedback_no_backward_compat_pre_v1` no migration was written — pre-uuid saves MINT a fresh uuid on restore rather than restoring empty, so old pedalboards load and are automatable.
- **Task 2 — registration walks each engine's own parameter list, not a per-control table.** Coverage is complete by construction and cannot rot as params are added. Sound because the Event Editor's param browser enumerates the applicator registry directly ([StandaloneEditor.cpp:3184](Source/Standalone/StandaloneEditor.cpp:3184)) — a registered param is discoverable and automatable even with no componentID on its control, so the per-control stamp is right-click convenience, not a functional requirement.
- **Task 3 — dead `tk_` mirror retirement, LARGER than the plan's framing.** Removed `registerParamsForTrack`, `unregisterParamsForTrack`, `isTrackRegistered`, `addParamsForHarmless`, `addParamsForVibePlayer`, `addParamsForBaySickSynth`, `addParamsForBaySickBass` **and `addParamsForEffectRack`** (decls + defs), plus the six call sites in LayersPage / BassPage / DrumPage; grep confirms zero live references remain. The four engine mirror helpers registered `tk_{trackId}_{param}` ids that were id-MISMATCHED with what the engine editors actually stamp (`tk_lay_0_oscMode` vs `tk_lay_0_bss_noise`) — automatable to nowhere, exactly as the plan described. **`addParamsForEffectRack` was equally dead and the plan did not flag it:** 6 slots x 15 params (`tk_{id}_rack_slot{s}_type/_bypass/_output/_p0..p11`) per track, with **zero readers anywhere in the tree** — real racks live on InsertNodes under `mixer_*` with uuid-keyed automation ids. Removed under `feedback_clean_own_batch_dead_code_in_batch`; leaving it would have left an uncalled helper behind.
- **Task 3 — `mRegisteredTrackParams` deliberately KEPT.** `ensureMixerStripParams` and the EQ-bank helpers use it as their id accumulator. Safe because the two paths keyed it differently — the dead path by `trackId` ("lay_0"), the live path by mixer prefix ("mixer_layer_0") — so they never shared entries. Stale comments fixed in the edited regions per Rule 6: the "Lazy APVTS registration" header block in [PluginProcessor.cpp](Source/PluginProcessor.cpp) plus the identical stale header comment in [LayersPage.h:20](Source/Standalone/LayersPage.h:20) and [BassPage.h:20](Source/Standalone/BassPage.h:20). Old projects carrying saved mirror values load fine — APVTS ignores values for unregistered params (§B.27 AP-13).
- **Task 4 — BLU-492 selector audit: ZERO conversions needed, no code changed.** Verified field-by-field, not sampled, across 13 selector families: Compressor ratio/All-in/knee/type, Reverb mode/tail shape, the shared sync-division selector (Reverb / Phaser / Flanger), Saturation tube type/harmonics mode, the shared oversampling selector (Saturation / Overdrive / Tape / TransientShaper), Phaser LFO wave, Flanger shape, Delay feedback-filter type. Every one has its backing field serialized in its own DSP's `getStateInformation` ValueTree. The two meter-mode selectors are display-only and correctly local per docket 5=A. **The plan's one named conversion target is also void:** the Pedals EQ-type picker holds no separate tone state — `onEqPickerChanged` calls `loadEffect` to change the SLOT TYPE, the slot type is persisted in `captureFullState`, and `rebuild()` re-syncs the picker from it on load. It already round-trips. Full table is in the running notes.
- **Task 5 — BLU-378/379 straggler sweep.** VibePlayer `cutSelfMode` stamped and registered (attached but never stamped — the one control on that editor with no Automate menu). Harmless `lfo_rate` + `lfo_shape` restored to `wireMeta`: two VISIBLE, working knobs had been skipped on the strength of a stale "S4: params ripped" comment, when both params are registered ([HarmlessProcessor.cpp:494-495](Source/Harmless/HarmlessProcessor.cpp:494)), read by the DSP, and attached. BLU-379: no double-writer found — every applicator either drives a control owning exactly one attachment, or drives the parameter directly. Covered by §B.27 AP-16.
- **Build.** Per-task gate at the end of every task, BOTH configs clean each time (`RELEASE_EXIT_CODE=0` / `DEBUG_EXIT_CODE=0`).
- **Master Test Plan §B.27 authored — 17 scenarios (AP-1..AP-17)**, reconciled against what shipped rather than transcribed from the plan's verify ladder, with a scope note recording the void premises so the campaign walker is not hunting conversions that never happened. Three MUST-PASS: AP-4 (Part A and Part B separately automatable), AP-7 (per-instance keys across two Inst tabs), AP-9 (a pedal lane survives reorder + reload). AP-13 / AP-14 / AP-15 are the regression guards. §E's expected second PRESET-BREAK was struck in the same edit.

#### Found along the way

- **PREMISE FINDING 1 — NAMIR does NOT "tag nothing".** Its 7 `bindKnob` knobs already set `VKnob::paramId` ([BaySickNAMIREditor.cpp:190](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:190)) and so already offered an Automate menu. What was missing was REGISTRATION.
- **PREMISE FINDING 2 — pedals knobs do not carry wrong rack ids; they carry NO ids, and pedals slots had no stable identity at all.** `setSlotContext` is called from exactly one site, the FX rack page ([EffectsPage.cpp:842](Source/Standalone/EffectsPage.cpp:842)). The pedals editor builds the same panels and never called it. Pedal knob values are not params at all, and pedals slots had **no uuid** ([BaySickPedalsProcessor.h:105](Source/BaySickPedals/BaySickPedalsProcessor.h:105)) — the thing the FX rack relies on to keep lanes valid across reorder (C13). So "wire the knobs" was not reachable without first building the identity.
- **PREMISE FINDING 3 — the capture-lock gate list in the plan was stale.** The actual gated set ([BaySickVocalEditor.cpp:373-394](Source/BaySickVocal/BaySickVocalEditor.cpp:373)) is 9 controls, and **chain Bypass is NOT among them** — the in-code comment records that it "left the gate set with its removal" at QA-Fd.
- **PREMISE FINDING 4 — BLU-492 needed ZERO conversions.** Tone selectors persist, just not through APVTS: effects serialize through each DSP's own `getStateInformation` ValueTree. **The pre-accepted PRESET-BREAK (marathon 18) was therefore never spent and nothing in the preset format changed.**
- **CLAUDE.md was factually wrong about `oeq_mix`** — it claimed the param is absent from `createLayout` and the attachment commented out. Both were fixed 2026-04-19 (T1a). The census confirms Harmless has ZERO phantom stamped ids across all 60 `wireMeta` calls.
- **Automated writes push a parameter gesture per automation tick (30 Hz).** Slider-driven writes go through the attachment; `detuneMode` goes through `ParameterAttachment::setValueAsCompleteGesture`.
- **Task 5's straggler list was far larger than the plan's "known: VibePlayer `cutSelfMode`".** Harmless alone has ~16 attached-but-unstamped controls.
- **Cosmetic, left alone:** `mPartSel` is `addAndMakeVisible`'d but never given bounds in `resized()`, so it renders 0x0; the comment claiming it is "invisible by virtue of never being addAndMakeVisible'd" is wrong about the mechanism though right about the outcome. Untouched region, Rule 6 scoping.

#### What was done about each finding

- **Premise findings 1 + 3: absorbed into the shipped scope, no plan re-open.** NAMIR got registration plus a prefixed re-stamp; the vocal capture-lock veto was built from the gated set read out of the code, not the plan's list.
- **Premise finding 2: escalated as a spec call and answered option B (Jeff) — full slot-tagging shipped this batch.** He was told up front that it touches saved pedal setups.
- **Premise finding 4: recorded, no code.** §B.27's scope note and §E were both updated to say the expected second PRESET-BREAK did not happen and that AP-15 proves the audit's conclusion.
- **Harmless A/B: the automation half is FIXED by the owner call** — lanes are keyed to the PARAM, so they no longer follow the view, and `rebindToPart` retargets the stamp so right-click Automate always creates a lane for the part on screen. The residual exposure is anything ELSE keyed off the stamped componentID (MIDI Learn being the live example); recorded in CLAUDE.md "Harmless-specific" rather than fixed here.
- **CLAUDE.md `oeq_mix` bullet: FIXED in this batch** — replaced with the dual A/B gotcha, then rewritten a second time once the A/B fix shipped, since the fix invalidated the gotcha's own wording.
- **Gesture-per-tick: LOGGED, not fixed** — routed to QA-UndoCoverage / QA-DirtyFlag per this plan's Routing notes (both batches already exist in §5/§6, so no new routing was created).
- **Task 5 stragglers: a process miss, then closed.** The Task 1 finding said the full cross-engine list would go to Jeff as a docket at Task 5 open; it never was — `cutSelfMode` + Harmless `lfo_rate` / `lfo_shape` were stamped and the rest deferred on my own call. Surfaced on the commit; Jeff asked for the list, and re-reading the source corrected the count twice over: `pluck_blur` is a Part A/B dual already handled by the A/B rework, and the "4 hidden shape sliders" are not user-facing controls. Of the real set, **11 were then stamped + registered** — the 6 `HarmlessRoutingMatrix` faders (`rm_*`), the 3 `HarmlessXYZPad` knobs (`mod_x/y/z`), and the 2 filter-row type selectors (`flt1_type` / `flt2_type`, which needed a new `registerComboAutomation` helper). Both `attachToApvts` methods already receive the ids, so stamping landed at the attachment site with no plumbing. Jeff then said "add the toggle buttons", which surfaced a THIRD counting error — 5, not 4: Harmless has its own `cutSelfMode` button distinct from the BaySickPlayer one stamped earlier in this task. All five (`legato`, `unison_alt`, `vel_link`, `cutSelf`, `cutSelfMode`) stamped + registered. **The Harmless straggler set is now empty.** `pluck_blur` is correctly absent throughout — it is a Part A/B dual already covered by `rebindToPart`.
- **Task 2 leftovers: BOTH CLOSED on Jeff's instruction.** (1) `VocalChainPanel` now stamps the bare param id on all 9 automatable controls and `setAutomationPrefix` re-stamps the child tree with `vox{N}_` — written as a recursive walk, not a per-control list, so stamps added to the Align / Pitch sub-editors later are picked up without revisiting that function; the NAM/IR subtree is skipped since its own editor prefixed it moments earlier. (2) `resolveAutomationDisplayName` now strips a leading `inst{N}_` / `vox{N}_`, resolves the remainder through the existing renderer and prepends the tab label (`Inst 4 - nam_bypass`); pedals keys get a branch that DROPS the 32-hex slot uuid, which exists for reorder-stability and must never surface (`Inst 4 - Pedals - drive`).

#### Group review (R3)

- **Pending — runs at the G4 boundary** (after `clean-pointing-stoat`'s commit) over the group's combined diff.

#### Carry-forward contradictions

- None. Carry-Forward §1's automation/registry primitives were EXTENDED, not rewired, and Audio-thread rules §2 are untouched — applicators run message-thread only; this batch adds no audio-thread writes. Three notes to carry: (1) engine params are reachable by automation ONLY through the applicator registry, since the audio-thread pass still resolves main-APVTS ids exclusively; (2) `mAutomationApplicators` has no erase-on-destroy path, so registration closures are `SafePointer`-guarded no-ops after a control dies and a rebuilt tab re-registers over the stale key — the tab-close prefix erase is what keeps the map bounded; (3) pedals slots now persist a `uuid`, and pre-uuid saves mint a fresh one on restore.

#### Diagnostic Instrumentation Catalog

- **NONE added this batch.** No `DBG` / `juce::Logger` / temp `jassert` / debug `AlertWindow` / temp-file trace — every task was resolved by reading code plus the per-task compile gates. Nothing to strip.

#### Commit(s)

`<hash>` (whole batch — Tasks 1-5 + §B.27 + the CLAUDE.md corrections + the QA-NativeDialogs hash backfill + held entry + running notes; single batch commit per the bulk-run model). Preceded by QA-NativeDialogs `f4112b17`. Build clean in BOTH configs at every task gate; behavioral verification deferred to the R2 campaign pass against §B.27.

#### Next action

- Proceed to **QA-Verify** ([`sturdy-tagging-pangolin.md`](../Batch%20Plans/sturdy-tagging-pangolin.md)), G4 batch 4 of 8.
