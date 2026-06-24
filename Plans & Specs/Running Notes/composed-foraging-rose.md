# Running Notes — QA-EffectsReview (composed-foraging-rose)

> Append-only mid-batch log.  A new dated entry is added at EVERY checkpoint
> (commit landed / sub-task verified / finding captured / spec call resolved /
> scope pivot), per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At batch close, `/draft-doc batch-close` reads this file as the primary input
> for the single Implemented Work Log entry.  Never edit prior entries.

**Pair file:** [`Plans & Specs/Batch Plans/composed-foraging-rose.md`](../Batch Plans/composed-foraging-rose.md)
**Conventions:** Main Plan §0 Document Formatting Conventions + the Batch Plans / Running Notes required-sections rule (locked 2026-05-11).

## Diagnostic Instrumentation Catalog

Per §0 Rule 4 — every `DBG` / `juce::Logger` / temp `jassert` / debug `AlertWindow` /
temp-file trace gets a row IN THE SAME EDIT PASS.  Strip every `Remove` at task/batch
close after surfacing the strip list to Jeff.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| (none yet) | | | |

## 2026-06-06 — Task 0 — Batch open

- Plan approved + mirrored to `Batch Plans/composed-foraging-rose.md`; home-dir copy deleted (plan-file hygiene).
- **Batch RE-SCOPED at open:** the 4-bug docket -> a full effects-subsystem max-clone fidelity rework (every rack effect + every pedal graded + reworked vs its reference; per-slot Basic/Advanced toggle; Console Clean/Dirty).  Item **(d) multi-call SPLIT** to a new batch **QA-MultiBlockHazard** (engine/hot-path, not effect fidelity), directly after.
- Step-1 fidelity audit (read-only, 8 research passes) complete; findings written to `Research Reports/effects-fidelity-audit-2026-06-06.md`.
- Spec calls locked at open (see plan SC-* table): wide scope; one cohesive batch; big build on all 4 heavy units (De-Esser, SY-1, AD-2, Tape); Console = Clean(SSL)/Dirty(Neve) reusing the Tube band-split + shapers; Basic/Advanced toggle = per-slot, saved with project, default Basic, FX-rack panels ONLY (pedals + board basic-panels untouched).
- Main Plan edits: §5 QA-EffectsReview STATUS:OPEN + Plan-file pointer + re-scope note; NEW §5 QA-MultiBlockHazard docket; §6 arrow + footnote; §9 2026-06-06 Forks entry.
- **Committed C0: `c0a6150`** (docs-only; working tree clean).
- **Next:** Task 1 — Basic/Advanced toggle infrastructure (foundational; lands + verifies before any per-effect extras tagging).

## 2026-06-06 — Task 1 — Basic/Advanced toggle infrastructure (code complete; awaiting build/verify)

- Implemented across the 5 touch points:
  - `EditorPanelBase` (EffectEditorPanels.h): `bool mBasicMode {true}` + `virtual void applyBasicMode() { resized(); }` (panels with extras override later).
  - `SlotComponent.h/.cpp`: `mBasicBtn` header button (LEFT of Preset, same chrome) + `onBasicModeChanged` callback + `toggleBasicMode()`/`refreshBasicBtnLabel()`; visible under the same non-empty-effect gate as the preset button; laid out via `removeFromRight(72)` left of Preset; `paint()` name-shrink clause added; toggle re-applies layout in place (no editor re-mount → SafePointers/automation intact).
  - `EffectRack.h/.cpp`: `Slot.basicMode {true}` + carried in move-ctor/assign + `set/getSlotBasicMode` (setter fires `onSlotsChanged` → markDirty, no-op-guarded) + get/setStateInformation serialize (`basicMode`, default 1=Basic for old projects).
  - `EffectsPage.cpp`: `buildRackTab` wires `onBasicModeChanged` → `setSlotBasicMode`; `rebuildSlotEditor` stamps `base->mBasicMode` from the slot BEFORE `setEditor()` (first resized picks the right layout).
  - Exclusion by construction: `mBasicBtn` lives only in `SlotComponent`; pedal tiles + `*PedalPanel` board panels never get it.
- **Scope note:** C1 is pure infra — NO panel tags advanced knobs yet (Tasks 3-8 do that), so toggling won't visibly hide/show controls until the first tagged panel (Task 3 EQ8). C1 verify = the MECHANISM only.
- Files: `EffectRack.h/.cpp`, `EffectEditorPanels.h`, `SlotComponent.h/.cpp`, `EffectsPage.cpp`.
- **CORRECTION (Jeff feedback, 2026-06-06):** the toggle must appear ONLY on panels that actually have advanced controls — NOT on every rack effect. Realized pedal-`*Style*` effects (e.g. **Wah**) DO load into the FX rack, so they get a `SlotComponent` + button despite having no extras. Fix: added `virtual bool hasAdvancedControls()` to `EditorPanelBase` (default `false`); `SlotComponent::setEditor` now gates `mBasicBtn` visibility on the loaded panel's `hasAdvancedControls()` (was: any non-empty effect). Consequence: NO button appears until a panel tags advanced knobs — first at **Task 3 (EQ8)**.
- **Per-unit rule for Tasks 3-8:** every panel that tags advanced knobs must override `hasAdvancedControls() { return true; }` (alongside `applyBasicMode()`), else its button won't show.
- **Revised Task 1 verify (mechanism only):** build clean; NO Basic/Advanced button on ANY effect yet (nothing tagged — correct, incl. Wah-in-rack); pedals + board basics unchanged; no crash. First live button + show/hide + persist verified in Task 3.
- Plan SC-extras-scope + Task 1 verify line to be refined at C1 prep (the "button is FX-rack-only" framing corrected: pedal-Style effects appear in the rack too; the `hasAdvancedControls()` gate, not effect-category, suppresses the button on no-extras panels).
- Awaiting Jeff build (Debug then Release) + verify → then `/draft-commit` C1.

## 2026-06-06 — Task 1 (cont.) — scope correction: EQ8 OUT; Reverb/Delay coverage gap flagged

- **EQ8 OUT of scope (Jeff, 2026-06-06):** EQ8 / EQ8 M/S is the mixer/bus M-S parametric EQ (its own Effects-page sub-tab + per-insert EQ), NOT an FX-rack slot effect — no `SlotComponent`, no Basic/Advanced toggle, faithful+ already. Removed from Task 3 + all advanced/basic treatment; not touched at all.
- **First live toggle now = Task 4 (Modulation), not Task 3.** With EQ8 out AND the EQ-trio (GraphicEQ/BassGraphicEQ/Furman) extras eliminated by their own fidelity fixes (−∞ Level kill removed, Furman Q clamped), Task 3 yields NO advanced toggle. The first panel to set `hasAdvancedControls()=true` is in Task 4 (Chorus Voices/waves, Flanger/Phaser BPM-sync, Phaser CrossFB).
- **COVERAGE GAP found (mine):** the plan's task list omitted the **Time family** — **Reverb** (Partial: wire the built-but-hidden Ducking + tag 5-algorithm/HFRatio/WetTone/Freeze/TailShape extras) and **Delay** (Faithful: tag VocalDoubler/duck/Slapback extras + optional FB-filter "Off" + Time range). Both have advanced extras → both need a task + the toggle. Surfaced to Jeff for a Time task + slot. (Octave / Bass Driver / Bass Overdrive stay no-op — faithful, no extras; Bass Driver doc-fix already in Task 2.)
- **Plan refinements to batch at C1 prep:** remove EQ8 from Task 3 + Files-to-modify; add the Time task (Reverb/Delay) at Jeff's slot; SC-extras-scope `hasAdvancedControls()` gate + Wah-in-rack correction; Task-1 verify = mechanism-only (first toggle Task 4).

## 2026-06-06 — Task 1 (cont.) — plan coverage reconciled to the master matrix (EQ8 out; Time group restored)

- **Coverage error caught (Jeff, 2026-06-06):** the plan task list had silently dropped the **Time group** (Delay + Reverb), shoved Acoustic Preamp into the Utility grab-bag, included **EQ8** (not a rack/pedal copy), and mislabeled Octave / Bass Overdrive. Jeff re-pasted his original 7-group audit matrix as the authoritative scope; directive: compare, remove EQ8, do everything else.
- **Plan re-aligned to the matrix's 7 groups via targeted Edits** (not a wholesale rewrite — `feedback_targeted_edits_not_wholesale_rewrite.md`):
  - **Removed:** EQ8 (Task 3 bullet + Files-to-modify + all advanced/basic treatment). EQ8 = bus/insert M-S EQ, faithful+, not a rack/pedal copy.
  - **Added (were dropped):** Octave (OC-5) / Bass Driver (BB-1X) / Bass Overdrive (ODB-3) → Task 5; Wah (PW-3) → Task 4; **Delay + Reverb → NEW Task 9 (Time)**.
  - **Moved to matrix group:** SY-1 (Polyphonic Synth) → Task 4 (Modulation); AD-2 (Acoustic Preamp) → Task 9 (Time).
  - **Renumbered:** Batch close Task 9 → Task 10. Header edits: Files list, Sequencing line, Commit-structure "Tasks 3-9", SC-extras-scope gate, Task-1 verify (mechanism-only), Task-1 1a/1b (`hasAdvancedControls()` gate).
- **Final task→group map (34 units, EQ8 excluded, 0 dropped):** T3 EQ (GraphicEQ / BassGraphicEQ / Furman) · T4 Modulation (Chorus / Flanger / Phaser / Wah / AcousticSim / ⚠SY-1) · T5 Drive+Octave (OD rack / OD pedal / Blues / Distortion / Fuzz / HighGain / Octave / BassDriver / BassOverdrive) · T6 Compressors (Modern / FET / Opto / CS / NoiseGate / BassComp) · T7 Saturation (⚠Console / Tube / ⚠Tape) · T8 Utility (Limiter / Transient / Tuner / ⚠De-Esser) · T9 Time (Delay / Reverb / ⚠AD-2) · T10 close. Count: 3+6+9+6+3+4+3 = 34.
- **Faithful-leaning units (Octave poly-tracking inherent, Bass Driver MDP-adaptive, Bass Overdrive grit-floor):** matrix grades them low-sev; fix-vs-accept is a per-unit call surfaced AT their task (not pre-deferred — `feedback_qa_batches_fix_bugs_dont_defer.md`).
- **Octave / Bass Overdrive classification:** Jeff notes they're pedal-board entries (not rack entries); covered in their family task regardless — rack-vs-board label dropped from the plan to stop the re-categorization churn.
- **No source changes this checkpoint** — plan-doc only. Task 1 infra edits remain uncommitted in the working tree, awaiting Jeff's Debug+Release build/verify → then `/draft-commit` C1.
- **Next:** Jeff builds + verifies Task 1 (mechanism-only); on PASS → C1 commit; then per-family tasks 3-10 in order.

## 2026-06-06 — Task 1 — VERIFIED PASS + C1 landed

- Jeff built Debug + Release; Task 1 verified mechanism-only: builds clean, NO Basic/Advanced button on any rack effect (incl. Wah-in-rack — correct null result), pedals + board unchanged, no crash.
- Pre-commit self-review of the 6-file diff (Ultracode): no compile-breakers; brace balance confirmed (the ctor's old closing brace now closes `toggleBasicMode`, a new brace closes the ctor); Slot move-ctor/assign init-list order matches declaration order (no `-Wreorder`); EffectsPage `dynamic_cast` block braced correctly; diff is purely additive (+139/-0).
- Folded in 2 comment-only accuracy fixes (`SlotComponent.h`/`.cpp` mBasicBtn comments now state the actual `hasAdvancedControls()` gate, not the stale "same gate as mPresetBtn"); inert, no behavior change.
- Plan-doc reconciliation landed first as a separate doc-only commit **`f70f10c`** (EQ8 out; Time group restored; Octave/BassDriver/BassOverdrive/Wah covered; 34-unit matrix alignment).
- **Committed C1: `684acf8`** (6 source files, +139/-0; Task 1 toggle infrastructure). Working tree clean.
- **Next:** Task 3 — EQ family. No Advanced toggle in Task 3 (the EQ trio has no extras; the fidelity fixes eliminate the only candidate — the dead −∞ Level kill — so the first live toggle is still Task 4). Value-picks surfaced to Jeff for confirm before implementing.

## 2026-06-06 — Task 3 — EQ family (code complete; review SAFE-TO-BUILD; awaiting build/verify)

- Implemented across 7 files: `GraphicEQStyleDSP`, `BassGraphicEQStyleDSP`, `FurmanEQStyleDSP` (.h/.cpp each) + `EffectEditorPanels.cpp` (the 3 EQ panels).
- **Grounded in the actual code first (corrected the plan's wording):** the 7 band sliders were ALREADY ±15 (no change); it's the master LEVEL slider that was −60..+12 with a FUNCTIONAL −∞ mute (`buffer.clear()`), not "dead" — changed to ±15 + dropped the mute. All bands were `makePeakFilter` (so the top-band shelf fix is real).
- **GraphicEQ (GE-7):** top band (6.4k) peak→high-shelf (Q 0.707); Level −60..+12→±15, dropped −∞ mute; panel top-band tooltip "peaking"→"high-shelf", Level fader range ±15; header docs updated.
- **BassGraphicEQ (GEB-7):** Level →±15 + drop −∞ mute (bands stay all-peaking, correct); folded the Task-2 doc fix `MXR M-108`→GEB-7 here.
- **Furman (PQ-3):** preamp cap 0..86→0..26 (new `kInputMaxDb`); Q clamp 0.1..10→0.2..3.8; NEW Hi/Lo +20 dB output gain-range switch (`enum class GainRange` + serialize `gainRange`); NEW preamp overload LED (`std::atomic<float> mClipLevel` pre-tanh block peak + `getClipLevel()` + panel `juce::Timer`@20Hz + LED dot top-right of Input + "OL" label); panel Input knob 0..26, Q knobs 0.2..3.8, Hi/Lo button.
- **Task 2 dissolved into family tasks (commit-structure deviation, surfaced to Jeff):** the 3 doc-comment fixes ship with their unit's rework — BassGraphicEQ `MXR`→GEB-7 here (Task 3); BassDriver `SansAmp`→BB-1X + Overdrive `atan`→`x/(1+|x|)` go with Task 5 (Drives). No standalone C2 doc commit. All fixes still land.
- **Adversarial review (general-purpose agent) over the 7-file diff: SAFE-TO-BUILD** — no compile-breakers (`<atomic>` included, enum-class casts valid, `EditorPanelBase + private juce::Timer` MI unambiguous, decl/def signatures match, `makeHighShelf` args correct, ASCII clean), no logic bugs (`mClipLevel` reset on preamp-off, Hi/Lo +20 applied after bands regardless of EQ-bypass, Q skew 0.7 valid in 0.2..3.8, `gainRange` round-trips), no regressions (right cluster + LED lay out in BOTH Full + Pedal mode, LED guarded by `!isEmpty()`).
- **VERIFY LOCATION (review finding):** GE / GEB / Furman are NOT in the FX-rack add menu — `SlotComponent::showAddMenu` comment confirms they are BaySickPedals-only per locked spec. Jeff verifies all three on the pedalboard (Inst page → BaySickPedals), Pedal panel mode; the single panel struct handles both modes and the pedal path lays out the new controls.
- **No Basic/Advanced toggle in Task 3** (EQ trio has no extras AND isn't in the rack — doubly confirmed).
- Diagnostic catalog: none added (no temp instrumentation).
- **Next:** Jeff builds Debug+Release + verifies Task 3 on the pedalboard → on PASS `/draft-commit` the EQ-family commit (includes the BassGraphicEQ doc fix).
- **LANDED — Task 3 committed `200d035`** (7 files, +140/-49) after Jeff verified PASS. Running-notes entries (Task 1 + Task 3) remain uncommitted in the tree pending a disposition surface at the next commit.

## 2026-06-06 — Side-fix (out of effects scope) — Clips-page delete leaves orphan mixer strip

- **Found during Jeff's Task-3 test-rig setup** (prerecorded guitar part → moved under a new Inst page → deleted a Clips page; the Audio strip lingered in the live mixer but was correctly gone on reload). Jeff: "Fold it in now."
- **Root cause (traced, not guessed):** `MixerPage::removeClipChannel(idx)` was defined + declared (`MixerPage.cpp:2704` / `.h:115`) in the 2026-05-05 orphaned-strip cleanup but is **never called anywhere** (confirmed via full-tree grep). `onTabClosed` wires `removeInstChannel` (`StandaloneEditor:4306`) + `removeVoxChannel` (`:4311`), but the parallel `ClipsPage` branch (`:4083`) only tore down the engine + ran the library/block cascade — it never dropped the Audio strip widget. So a deleted Clips page's strip stayed in the live mixer; the save is page-list-driven, so it correctly omitted the strip and reload looked right. = **LIVE-UI-REFRESH GAP** (Jeff's hypothesis confirmed) — harmless/self-healing, but a real bug.
- **Fix (1:1 mirror of the Vox wiring):** declared `int clipStripIdx = -1;` at the close-block scope, set it in the `ClipsPage` branch (`clipStripIdx = idx`, where `idx = cp->getPageIndex()` = the `mAudioStrips` key, confirmed via the `StripKind::Audio` rename at `:1316`), and added `if (clipStripIdx >= 0 && mMixerPage) mMixerPage->removeClipChannel(clipStripIdx);` after the Vox removal. APVTS params kept (matches the Inst/Vox/Aux convention).
- File: `Source/Standalone/StandaloneEditor.cpp` (2 edits). **Out of QA-EffectsReview's effects scope** — folded in at Jeff's request; to be recorded as a §9 Forks side-fix entry at batch close, committed separately from the EQ-family work.
- **Next:** Jeff builds Debug+Release + repro-verifies (delete a Clips page → strip drops live; reload still correct; Inst/Vox unaffected) → on PASS commit as its own focused commit + surface the dangling running-notes disposition.
- Diagnostic catalog: none added.

## 2026-06-06 — Side-fix Part 2 (uncovered by Jeff verifying Part 1) — Clips-page spawn could create a strip-less page

- **Found by Jeff:** copy the same file → duplicate-drop prompt ("Use Existing / New Page / Cancel") → "New Page" → creates the ClipsPage but **no mixer strip**. The inverse of Part 1.
- **Root cause (traced):** the canonical creator `createClipStripAndPage` (StandaloneEditor:7733) does the strip trio (`addAudioRowChannel`+`ensureAudioInsert`+`addAudioChannel`) THEN the page; but **four** call sites bypassed it and called `spawnClipsTabIfMissing` directly (page only, no strip): (a) `onDuplicateFileDrop` "New Page" (Jeff's path, ~:2467), (b) `onCreateRoutablePage` Clip branch (~:2229 — additionally returned an `audioInsert()` routing channel with no InsertNode), (c) Clip-preset load (~:7700), (d) `spawnDuplicateClipsTab` right-click Duplicate (~:8058).
- **Fix (single source of truth):** gave `createClipStripAndPage` a forwarded `bool allowDuplicate=false` param (default in the `.h` decl only) and rerouted all four sites through it with `allowDuplicate=true`. Now every Clips-page spawn creates its strip (+ InsertNode); site (b) also gains a valid routing target. The three pre-existing callers (Add-New-Clip, addClipPageFromFile, reload) keep `allowDuplicate=false` → unchanged.
- **Adversarial review (general-purpose agent) over the full StandaloneEditor diff: SAFE-TO-BUILD** — compile clean (default-arg placement, all four receivers `self->`/`this->`/member correct); strip trio is idempotent + guarded (`count>0 return` everywhere) so no double-create at the fresh free rows; `addAudioToLibrary` dedups on (path,owner) → no double-register; `importClipState`/`loadPagePreset` don't touch the strip → no clobber/mis-name; Part-1 `clipStripIdx` scope/lifetime valid (mirrors `voxStripIdx`). One pre-existing NIT (Move-to-new-page makes two same-named strips on one WAV — intended, not introduced).
- Files: `Source/Standalone/StandaloneEditor.cpp` + `.h` (Part 1 + Part 2 together = +38/-8). One cohesive clip-strip-lifecycle side-fix → ONE commit, separate from the EQ work.
- **Next:** Jeff builds Debug+Release + verifies all four spawn paths + the Part-1 delete + no-regression on normal drop / Inst / Vox → on PASS commit the side-fix + surface dangling running-notes disposition → §9 Forks entry at batch close.
- Diagnostic catalog: none added.

## 2026-06-06 — Side-fix Part 3 (uncovered by Jeff verifying Part 1/2) — deleted clip strip leaves a blank slot

- **Found by Jeff (screenshot):** after delete + re-add of a clip strip, a blank slot appears where the first strip used to be — the audio strips no longer pack contiguously against the bus.
- **Root cause (traced):** `removeClipChannel` (`MixerPage.cpp:2704`) erased only `mAudioStrips`, NOT `mAudioRowOrder` — the strips' order vector that `addAudioChannel` `push_back`s to (`:2904`) and the layout + cable-routing scan iterate (`:3222`, `:3478`). `removeInstChannel`/`removeVoxChannel` clean BOTH their map + order vector; `removeClipChannel` was written against a stale in-code comment claiming "no separate order vector" (wrong — `mAudioRowOrder` exists at `MixerPage.h:301`). So a deleted row lingered in the order vector → a reserved-but-empty slot in the packed layout, and a re-add `push_back`'d a duplicate index. Latent for the life of the helper; **exposed by Part 1** (before it, the strip never left on delete, so the row never emptied).
- **Fix:** `removeClipChannel` now also erases `idx` from `mAudioRowOrder` (`std::remove`+`erase`, the same idiom removeInst/removeVox use) + corrected the stale comment. InsertNode/APVTS params still kept (Aux/Inst/Vox convention).
- File: `Source/Standalone/MixerPage.cpp` (1 edit). Folds into the same clip-strip-lifecycle side-fix → ONE commit (now Parts 1-3: `StandaloneEditor.cpp`/`.h` + `MixerPage.cpp`).
- **Next:** Jeff builds + verifies (delete + re-add a clip strip → no blank slot, strips pack against the bus; mid-list delete packs the rest; Inst/Vox unaffected) → on PASS commit the whole side-fix.
- Diagnostic catalog: none added.
- **LANDED — side-fix committed `13cf8ea`** (4 files incl. this running-notes doc, +100/-10) after Jeff verified PASS.

## 2026-06-06 — Task 4 (Modulation) sub-chunk 1 — (c) Flanger + Phaser un-sync fix (code complete; awaiting build/verify)

- **(c) bug confirmed in code:** both `FlangerDSP::setSyncBPM` and `PhaserDSP::setSyncBPM` only handled turning sync ON (derive rate from BPM); neither had an `else` for turning sync OFF, so `mRate` stayed stuck at the synced value and the user's pre-sync manual rate was lost.
- **Fix (manual-rate shadow, plan SC-c):** added `float mManualRate` to both DSPs. `setRate` (both) + `setSweepFreq` (Phaser's 2nd rate alias) now record `mManualRate` and yield to sync (`if (mSyncBPM) return;` — while synced the BPM derivation / `reapplyBpmSync` owns the rate). `setSyncBPM(false)` now restores `mRate` (Flanger) / `mRate`+`mSweepHz` (Phaser) from `mManualRate`. `mManualRate` is persisted in get/setStateInformation (defaults to the loaded `rate` for pre-fix projects → back-compatible).
- Files: `FlangerDSP.h/.cpp`, `PhaserDSP.h/.cpp` (DSP-only; 11 edits). `reapplyBpmSync` unchanged.
- Self-reviewed: compile-clean (`mManualRate` declared in both headers; no signature changes); while-synced the rate knob shadows (correct FL behavior) and restores on un-sync; host-BPM-change + sync-division paths unaffected.
- **Next:** Jeff builds + verifies on Flanger + Phaser (set manual rate → Sync ON locks to BPM → Sync OFF restores the manual rate; save/reload keeps it; sync-on still locks) → on PASS commit. Then sub-chunk 2 (Flanger Damp remap + Chorus/Phaser/AcousticSim Advanced toggles — first live Basic/Advanced button) → sub-chunk 3 (⚠ SY-1).
- Diagnostic catalog: none added.

## 2026-06-07 — Task 4 (Modulation) — full rework complete + brand-safety pass

- **Remainder of Task 4** beyond sub-chunk 1 (the (c) un-sync fix above — final shape unchanged): the modulation panels' Basic/Advanced toggles + the SY-style poly rework, plus a brand-safety pass that fired mid-task.
- **Chorus panel — Basic/Advanced toggle:** `hasAdvancedControls()` = true. Basic = the exact reference control set; Advanced reveals only our additions (Voices selector + Wet knob). First live toggle button (Task-1 infra).
- **Phaser panel — Basic/Advanced toggle:** Basic = reference set; Advanced reveals our additions (Cross knob + Wave / SyncDiv / InvFB / BPM-sync).
- **Flanger Damp remap:** DampHz knob (200..20000 Hz) reworked to a 0-1 "Damp" control (0 = off/bright, 1 = max-warm); panel maps 0-1 UI to Hz DSP exponentially, DSP API/range unchanged.
- **SY-style synth pedal (`SynthStyleDSP`):** TYPE 4 -> 11 (`kProfiles[11]`; original 4 keep enum values 0/1/2/3 = patch-stable, 7 new take 4..10; panel `kSynthTypeOrder[]` + `synthTypeValueToDisplay()` keep TYPE-knob display order vs value-stable persistence); Guitar/Bass range switch (tracker freq range + poly cap); 8-voice polyphony via NEW `PolyPitchTracker` (FFT harmonic-sum + greedy iterative spectral subtraction, seqlock publish, AbstractFifo SPSC ring + background worker; mirrors PitchTrackerYIN threading); voice pool match/free/steal; `process()` mono(YIN)/poly branch; poly+instrument persisted. Panel: 11-option ChickenHeadSelector (2-char marks) in a row above the knobs; Mono/Poly + Gtr/Bass DualLabelToggles. SharedUI: setOptions cap 10->12, letterPad 10->13. NEW `Source/DSP/PolyPitchTracker.cpp/.h` -> CMakeLists.
- **BRAND-SAFETY PASS (fired mid-task — Jeff caught a real brand name in a shipped tooltip):**
  - **Rule:** real gear/product/model names must NOT appear in user-facing strings (the trademark exposure); code comments + commit messages are nominative fair use and stay. Repo is open source, public since first commit; UI/branding stays brand-free, internal docs keep factual references (Jeff's legal research 2026-06-07). New memory: `feedback_no_brand_names_in_user_facing_strings.md`.
  - **14 user-facing brand strings scrubbed** across the effects UI (Synth / Flanger / Furman / FET-comp / Opto-comp / NAM tooltips + EQ8 prop-Q menu) -> brand-safe generic terms (FET-style / Opto-style / analog console / amp capture / aggressive crush). Fuzz pedal's 3 VISIBLE mode labels renamed by circuit character: Gated / Germanium / Octave (Jeff picked).
  - **6 BaySickVocal comments** softened "X-clone" -> "X-style".
  - **Verified clean:** semantic agent sweep of the effects UI + grep for literal-copying phrasing -> no other brand names in shipped strings; the only remaining "port/clone" hits are internal self-references to our own legacy code (TapeDSP / ArrangementBlock).
  - **Routed:** the BaySickVocal *UI* brand pass -> future **QA-F** batch (§9 Forks + Main Plan §5 docket at batch close; slot is Jeff's call).
- Files (beyond sub-chunk 1): `SynthStyleDSP.cpp/.h`, `PolyPitchTracker.cpp/.h` (new), `EffectEditorPanels.cpp`, `SharedUI.cpp`, `CMakeLists.txt` + comment-only edits in 4 BaySickVocal headers.
- **Verify status:** Task-4 DSP/panels verified by Jeff in Debug+Release; the brand-safety string/comment + Fuzz-label edits are text-only, awaiting a confirm build before commit.
- Diagnostic catalog: none added.
- **Next:** Task 5 (Drive + Octave).
- **LANDED — Task 4 committed `1bb2c18`** (16 files, +735/-125) after Jeff verified PASS in Debug + Release (incl. the brand-safety edits). Commit message is brand-free (direct gear names removed, generic "-style" descriptors kept, per Jeff).

## 2026-06-19 — Task 5 (Drive + Octave) — Chunk A complete (verified); Chunk B/C in progress

- **Chunk A (drive/distortion rack + pedals) — VERIFIED PASS (Jeff, Debug+Release, in two sub-builds):**
  - **Overdrive RACK (Blood Overdrive):** reference-confirmed via the FL manual (6 controls: PreBand/Color/PreAmp/x100/PostFilter/PostGain). DSP reworked: pre-filter bandpass -> lowpass + in-series whole-signal drive (dropped the parallel clean residual + the `mResidualBuf` member; `mBPF` renamed `mPreLpf`); PreBand remapped to lowpass-amount blend; PostGain attenuate-only (-18..0, enforced on legacy load). NEW Basic/Advanced toggle (Basic = the 6 reference controls; Advanced = Bias/Wet/Parallel/Oversampling, our additions). **SPEC CALL:** Jeff confirmed the full reference match (bandpass->lowpass) over the plan's literal "drop residual" alone, which would have thinned the bandpass.
  - **OD pedal (OD-3):** 500 Hz pre-clip mid-notch + 720 Hz 1-pole inter-stage HPF + drive ceiling `1+preAmp*4` -> `*13`.
  - **Distortion (DS-1):** scoop recentred ~800 -> ~500 Hz (kToneLpf 400->250, kToneHpf 2000->1000).
  - **Fuzz (FZ-5):** NEW Boost knob (0..+20 dB, inserted between Fuzz/Level, handlers reindexed; serialized "boost", defaults 0).
  - **Blues (BD-2):** ~100 Hz +3.5 dB fixed body peak + envelope-driven dual-stage shaper (asym tanh cross-fading into a cubic soft-clip by |s1| -> 2nd->3rd harmonic shift as the player digs in). `StereoLPF` alias renamed `StereoIIR`.
  - **High-Gain (MT-2):** pre-boost 700/+9 -> 1k/+36 (research-confirmed: Electric Druid measured "+36 dB at 1 kHz"). Post-clip "V" CORRECTED -- the real pedal does NOT cut mids; it's two fixed gyrator BOOSTS (~100 Hz + ~5 kHz, the V is the valley between). **Both the plan's "-12 cuts" AND my mid-notch guess were wrong; multi-source research (Electric Druid / audio-tk / guitarpedalsvisualized) found the real boosts -- Jeff: "I want what the metal zone pedal actually does."** `mFixedMidScoop` -> `mFixedScoopLo`+`mFixedScoopHi`. dsp-test-signal verdict: provably bounded (tanh ceiling absorbs the +36 dB x 1000x); the 5 kHz +10 dB IS the MT-2's signature harsh treble (faithful, not a bug).
- **3 fix-vs-accept spec calls (Jeff chose ALL the builds, not accept-faithful):** Octave -> build FFT/pitch-sync shifter; Bass Driver -> build adaptive (MDP) drive; Bass Overdrive -> add grit-floor.
- **Chunk C (Octave) design researched + saved** to `Research Reports/daw-architecture-octave-pitch-shift-engine-2026-06-18.md` (hybrid: PSOLA period-doubler for octave-down + small-FFT for up/poly + POG-style voicing; reuses PitchTrackerYIN + the SY-style tracker wiring; NOT the PhaseVocoder / NOT Rubber Band).
- **Chunk B (bass) in progress:** BassDriver already keeps lows clean (3-band, low bypasses clipper) -> the MDP gap is the adaptive/dynamics drive + the SansAmp->BB-1X doc-fix; BassOverdrive is clean at Gain=0 (grit-floor gap confirmed). Research on the real BB-1X MDP + ODB-3 grit-floor running.
- Diagnostic catalog: none added.
- **Next:** implement Chunk B (BassDriver adaptive drive + doc-fix; BassOverdrive grit-floor) per the research -> Chunk C (Octave shifter) -> one Task-5 commit at the end.

## 2026-06-19 — Task 5 (Drive + Octave) — Chunk B + C complete; Task 5 DONE (all 9 units verified)

- **Chunk B (bass) — VERIFIED PASS:**
  - **Bass Driver (re-pointed SansAmp -> BB-1X):** the multiband-clean-lows was already there; added the missing MDP piece -- an envelope-driven **dynamics-adaptive drive** (soft playing = cleaner, dig in = grind; kDriveFloor 0.4 + fast/slow env -> per-base-sample drive on the mid/high band) -- and raised the low/mid split 200 -> 500 Hz (more low end kept clean). Research (official copy + TalkBass measurement): MDP is frequency- AND dynamics-adaptive, keeps lows tight while mids/highs grind, ~600 Hz split, parallel clean Blend. Header/doc rewritten to the BB-1X.
  - **Bass Overdrive (ODB-3):** grit floor added -- drive now `1.6 + mGain*48.4` (never fully clean at Gain=0; diodes always in-circuit), dirties early. Research confirmed the ODB-3 is never clean at min gain + is a harsh/near-fuzz distortion.
- **Chunk C (Octave) -- the big build -- VERIFIED PASS. B/hybrid** (Jeff chose B = PSOLA over the safe period-synced-granular after a full A-vs-B benefit/drawback/future-functionality comparison; B builds reusable pitch-synchronous infra for a future harmonizer / vocal harmony):
  - **C1 -- PSOLA period-doubler (octave-DOWN):** new `PeriodDoubler` (re-emits each pitch period 2x for -1 / 4x for -2; period-aligned seam crossfade; lag guard), driven by `PitchTrackerYIN` (SY-style wiring); confidence-gated.
  - **C2 -- period-synced granular + chord fallback:** granular grain now sized to an integer multiple of the detected period (kills the +1 warble); -1/-2 crossfade the crisp doubler (confident single notes) vs the granular (chords/unvoiced) by smoothed confidence -> Polyphonic works on EVERYTHING.
  - **C3 -- POG-style voicing (Polyphonic only):** a transient duck (dry attack leads, shifted voices fade in behind) + a gentle ~5 kHz LP on the shifted streams to mask smear.
  - **CPU-CLIMB BUG found + fixed (Jeff caught it):** the doubler's `readPos` + the pre-existing granular's `readPos1/2`/`rp` grew UNBOUNDED -> the ring-wrap `while` loops became O(pointer/ringSize) -> DSP% climbed the longer you played + persisted across transport pause (resumed from where it stopped). Fixed by wrapping the read pointers each sample (behavior-preserving). New memory `reference_wrap_ring_read_pointers.md`; the build-review (no-OOB) + /test-signal (audio) both MISSED it.
- **All 9 units verified PASS (Jeff, Debug+Release, across the sub-builds).** Every Octave sub-step (C1/C2/C3) + the CPU fix re-verified by ear/meter.
- Files (Task 5 total): 16 modified (15 source + this running-notes doc) + the Octave research report (new, `Research Reports/daw-architecture-octave-pitch-shift-engine-2026-06-18.md`). Each meaty unit got an adversarial build-safety review (SAFE-TO-BUILD) before Jeff built; HighGain + Octave-C1 also got /test-signal plans.
- Diagnostic catalog: none added (PitchTrackerYIN's worker thread is permanent, not diagnostic).
- **Next:** the single Task-5 commit (brand-free message), then Task 6 (Compressors).
