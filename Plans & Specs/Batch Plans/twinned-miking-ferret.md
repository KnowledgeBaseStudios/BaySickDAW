# QA-Fc — BaySickNAMIR Dual-Mic Stack (parallel Mic B, summed) — Plan (twinned-miking-ferret)

> **Canonical path** (mirrored after group approval):
> `Plans & Specs/Batch Plans/twinned-miking-ferret.md`
> Paired running notes: `Plans & Specs/Running Notes/twinned-miking-ferret.md`

> **For execution (BULK-RUN mode — see [`Batch Plans/swift-stampeding-caribou.md`](swift-stampeding-caribou.md)):** code ALL tasks, NO per-task verify pause. Jeff runs `do_build.bat`; fix to clean. Verify scenarios author into Master Test Plan **§B.9**. **ONE** source commit (Rule 9). Work Log close entry drafted + HELD; applied at §B.9 section pass (R2).

## Context

QA-Fc adds a second, parallel microphone path (Mic B) to BaySickNAMIR. The existing single-mic chain becomes Mic A; Mic B mirrors it and its output **sums** into the main buffer (not a blend/crossfade) — exactly how two real mics on one source behave (+3 dB on correlated content, comb-filter colouration from path differences). Orthogonal to the rest of G2; sequenced last so all engine-side dust has settled. **Fully locked by §23** — no open spec calls.

**Current state (verified 2026-07-09, Explore agent C):**
- `BaySickNAMIRProcessor::createLayout` registers exactly **18 params** ([BaySickNAMIRProcessor.cpp:69-136](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:69)), including `nam_micsim_*` (3) + `nam_placement_*` (4). **No `nam_micb_*`/`_b_` params exist.**
- Single-mic chain in `processBlock` ([:240-457](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:240)): … → Cab IR ([:405-427](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:405)) → **Mic Sim** ([:429-439](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:429)) → **Mic Placement** ([:441-452](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:441)) → Master ([:454-456](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:454)). **One** instance each: `MicSimDSP mMicSim` / `MicPlacementDSP mMicPlacement` ([.h:258-259](Source/BaySickNAMIR/BaySickNAMIRProcessor.h:258)).
- `SlotSnapshot` ([.h:139-168](Source/BaySickNAMIR/BaySickNAMIRProcessor.h:139)) holds **17 fields** (§23 said 16 — use 17); serializers [.cpp:704-746](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:704); capture [:751-779](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:751) / restore [:781-830](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:781); driven from `parameterChanged` [:682-698](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:682).
- Scratch buffers [.h:244-248](Source/BaySickNAMIR/BaySickNAMIRProcessor.h:244) sized in `prepareToPlay` [:186-191](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:186). No `mPreMicScratch`/`mMicBScratch`.
- Editor is fixed **760×560** ([.cpp:9-10](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:9)); Mic Sim row `resized()` [:465-484](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:465), Mic Placement [:486-499](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:486) (`kMicSimRowH=100`); members [.h:111-136](Source/BaySickNAMIR/BaySickNAMIREditor.h:111); construction [:231-362](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:231).
- **Hosted per-page on BOTH Vox + Inst** (each owns its own `BaySickNAMIRProcessor` instance of the same class) → the dual-mic change lands ONCE and propagates to both pages.

**Risk:** medium — new params + new DSP path on a heavily-used engine. Worst case: Mic B bypass broken → silent Mic B (harmless) or always-on (changes Mic A tone, caught immediately by ear). **Effort:** medium-large (~7-12h). **Dependencies:** clean BaySickNAMIR foundation (audit confirmed 2026-05-14, §20). **Bucket:** Players.

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| G2-23 | Full dual-mic spec per [`Running Notes/phantom-recording-mongoose.md`](../Running Notes/phantom-recording-mongoose.md) §23 (locked 2026-05-14). Parallel paths, **summed** (not blended). 8 new `_b_` params; `nam_micb_active` default false = byte-identical to today when off. | §23 locked. Additive sum = real two-mic behavior. |
| G2-23snap | `SlotSnapshot` gains 9 fields (8 params + `micbUserIrPath`); A/B slots preserve dual-mic state independently. | §23 per-slot snapshot extension. |
| G2-23ui | Editor splits Mic Sim + Mic Placement rows into Mic A \| Mic B columns; Mic B Active toggle (`DualLabelToggle`) at top of Mic B column dims/disables when off. Height bump if the split doesn't fit. | §23 editor layout (owner's "squish to half + mirror" mental model). Height calc is an implementation detail. |

## Sub-spec calls surfaced for approval
**None open.** §23 locks every decision (params/defaults/architecture/snapshot/UI). Editor height bump is an implementation calc, not a call. Any mid-execution call stops that piece and surfaces to Jeff.

## Files to modify

### Task 1 — Processor: params + parallel Mic B path
- [Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp) `createLayout` (:69-136) — add 8 params: `nam_micb_active` (Bool, false), `nam_micsim_b_mode` (Choice None/Built-in/User IR), `nam_micsim_b_model` (Choice 10, Live Vocal Dynamic), `nam_micsim_b_mix` (Float 0-100, 100), `nam_placement_b_distance_cm` (Float 1-150, 30), `nam_placement_b_angle_deg` (Float ±90, 0), `nam_placement_b_polar` (Choice 5, Cardioid), `nam_placement_b_mix` (Float 0-100, 100).
- [.h:258-259](Source/BaySickNAMIR/BaySickNAMIRProcessor.h:258) — add `MicSimDSP mMicSimB; MicPlacementDSP mMicPlacementB;`. [.h:244-248](Source/BaySickNAMIR/BaySickNAMIRProcessor.h:244) — add `juce::AudioBuffer<float> mPreMicScratch, mMicBScratch;` (sized in `prepareToPlay` [:186-191](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:186), host block × 2ch; `prepare` both new DSPs).
- `processBlock` (:429-452) — before Mic A processes in-place, copy post-cab buffer → `mPreMicScratch`. After Mic A (Placement, :452), if `nam_micb_active`: copy `mPreMicScratch`→`mMicBScratch`, run Mic Sim B + Mic Placement B on it, **sum `mMicBScratch` into buffer** before Master. When inactive: skip entirely (single param read + branch, zero added cost — RT fast-path).

### Task 2 — SlotSnapshot + serialization
- [.h:139-168](Source/BaySickNAMIR/BaySickNAMIRProcessor.h:139) — add 9 fields: `micbActive`, `micSimBMode`, `micSimBModel`, `micSimBMix`, `micbUserIrPath`, `placementBDistance`, `placementBAngle`, `placementBPolar`, `placementBMix` (defaults per §23).
- `toValueTree`/`fromValueTree` ([.cpp:704-746](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:704)), `captureSnapshotFromCurrent` ([:751-779](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:751)), `applySnapshotToCurrent` ([:781-830](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:781)) — handle the 9 new fields.
- `getStateInformation`/`setStateInformation` (:860-1017) — Mic B user-IR path gets the same per-slot reload treatment as the existing IRs ([:969-978](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:969)).

### Task 3 — Editor: Mic A | Mic B columns
- [Source/BaySickNAMIR/BaySickNAMIREditor.h](Source/BaySickNAMIR/BaySickNAMIREditor.h) / [.cpp](Source/BaySickNAMIR/BaySickNAMIREditor.cpp) — duplicate the Mic Sim + Mic Placement member sets ([.h:111-136](Source/BaySickNAMIR/BaySickNAMIREditor.h:111)) for Mic B (bound to `_b_` params); split each row into Mic A \| Mic B columns in `resized()` ([:465-499](Source/BaySickNAMIR/BaySickNAMIREditor.cpp:465)); add a "Mic B Active" `DualLabelToggle` (same pattern as `nam_bypass`/`cab_bypass`) at the top of the Mic B column; dim/disable Mic B controls when off. Bump `kEditorH` (currently 560) only if the side-by-side layout doesn't fit — recompute during implementation, and update the host bounds where `NAMIRHostPanel`/`mNamIrEditor` set size ([BaySickVocalEditor.cpp:37-40/:500](Source/BaySickVocal/BaySickVocalEditor.cpp:37) / [InstPage.cpp:84/:345](Source/Standalone/InstPage.cpp:84)).

## Tasks

### Task 1 — Processor: params + parallel Mic B path
- [ ] Add 8 `_b_` params (defaults per §23); add `mMicSimB`/`mMicPlacementB` + `mPreMicScratch`/`mMicBScratch` (prepared/sized).
- [ ] `processBlock`: copy post-cab→scratch; if active, run Mic Sim B + Placement B on scratch, sum into main; inactive = fast-path skip.

### Task 2 — SlotSnapshot + serialization
- [ ] Add 9 fields; extend `toValueTree`/`fromValueTree` + capture/restore; per-slot Mic B user-IR reload in setStateInformation.

### Task 3 — Editor: Mic A | Mic B columns
- [ ] Duplicate Mic Sim + Placement members for Mic B (`_b_` attachments); split rows into columns; add Mic B Active toggle (dim when off); height bump if needed + update host bounds.

### Batch close (one commit)
- [ ] Jeff runs `do_build.bat`; fix Release+Debug to clean.
- [ ] Author Master Test Plan **§B.9** from Verify scenarios.
- [ ] Draft + HOLD Work Log close entry; append code-complete entry + Rule 4 rows.
- [ ] Surface message + FULL git status → approval → **one** commit: `QA-Fc Tasks 1-3: BaySickNAMIR dual-mic stack (parallel Mic B, summed) + SlotSnapshot + editor Mic A|Mic B split (BaySickNAMIR, test plan B.9 + running notes)`.

## Verify scenarios (→ Master Test Plan §B.9; `blocks:` QA-Fc commit)
Directly from §23:
1. **Regression (off)** — Mic B Active OFF → output byte-identical to today's single-mic chain.
2. **Correlated sum** — Mic B ON, identical settings to Mic A → output ≈ 2× Mic A amplitude (+~6 dB) — confirms parallel-sum (not blend).
3. **Comb colouration** — Mic A distance 30cm, Mic B 120cm → audible comb-filter "fuller" tone from path difference.
4. **Per-slot A/B** — different Mic A vs Mic B configs per slot; switch `ab_slot` → tone snaps to the slot's stored dual-mic state.
5. **Persistence** — save project → reopen → all Mic B params + Mic B user IRs restore (incl. per-slot snapshots).
6. **Automation** — automate `nam_micb_active` in-DAW → smooth per-block crossover in/out of dual-mic (no click).
7. **CPU** — Mic B OFF vs ON delta ≈ zero when off (single param read + branch).
8. **Both pages** — dual-mic appears + works identically on a Vox NAM/IR sub-tab AND an Inst NAM/IR sub-tab (shared engine).

## Routing notes (Rule 3)
- Orthogonal batch; findings on other engines → fold into their batch/route at §B.9 section pass.
- New Mic B user-IR path routes through the existing IR-load audit surface — no new file-dialog work (native already, QA-NativeDialogs).

## Carry-Forward Reference touch points
- Read §4 (MT render path) before Task 1 — BaySickNAMIR runs inside the Vox/Inst engine chain per page instance; the Mic B sum is in-engine, MT-orthogonal.
