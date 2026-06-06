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
