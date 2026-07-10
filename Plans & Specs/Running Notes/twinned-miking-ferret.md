# Running Notes — QA-Fc (twinned-miking-ferret)

> Append-only running log for QA-Fc. New `## YYYY-MM-DD — <checkpoint>` entry at every checkpoint per `feedback_draft_doc_running_notes_every_checkpoint.md`. Under BULK-RUN mode ([`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md)) there are no per-task verify entries; the Work Log close entry is drafted + HELD here under `## Held Work Log entry (apply at section pass)` at code-complete, applied at Master Test Plan §B.9 section pass (R2).
>
> Pair file: [`Plans & Specs/Batch Plans/twinned-miking-ferret.md`](../Batch Plans/twinned-miking-ferret.md). Conventions: Main Plan §0.

## 2026-07-09 — Group open (G2) — seeded

Plan approved 2026-07-09 (G2 group approval, R5). QA-Fc is fully locked by §23; zero open spec calls. Orthogonal to the rest of G2; sequenced last.

### Locked spec calls
- **§23** — parallel Mic B path, **summed** (not blended). 8 new `_b_` params; `nam_micb_active` default false = byte-identical when off. Editor Mic A|Mic B column split. Picks up on both Vox + Inst automatically.
- SlotSnapshot gains 9 fields (8 params + `micbUserIrPath`); A/B slots preserve dual-mic state independently.

### Surface map (current code, verified 2026-07-09 via Explore agent C)
- 18 params (`createLayout` `BaySickNAMIRProcessor.cpp:69-136`), incl. `nam_micsim_*` (3) + `nam_placement_*` (4). No `_b_` params exist.
- Chain (`processBlock :240-457`): … → Cab IR (:405-427) → Mic Sim (:429-439) → Mic Placement (:441-452) → Master (:454-456). ONE `mMicSim`/`mMicPlacement` (`:h258-259`). Need a 2nd of each + `mPreMicScratch`/`mMicBScratch` (scratch buffers `:h244-248`, prepared `:186-191`).
- `SlotSnapshot` 17 fields (`:h139-168`; §23 said 16 — use 17); serializers `:704-746`; capture/restore `:751-830`; state `:860-1017`, per-slot IR reload `:969-978`.
- Editor fixed 760×560 (`:9-10`), single-column: Mic Sim row `:465-484`, Mic Placement `:486-499` (`kMicSimRowH=100`); members `:h111-136`. Split into Mic A|Mic B columns; bump `kEditorH` only if needed + update host bounds (`BaySickVocalEditor.cpp:37-40/:500`, `InstPage.cpp:84/:345`).
- Hosted per-page on BOTH Vox + Inst (each owns its own `BaySickNAMIRProcessor`) → change lands once, propagates to both.

## 2026-07-10 — Tasks 1-3 code-complete — dual-mic stack coded, build pending

- **Cross-check (session open, resume protocol):** Fc plan body vs the two 2026-07-10 rebuilds (QA-Fa recovery `62895ca8`, QA-Fb' Option A `66fea472`) — NO contradiction. QA-Fc touches only `Source/BaySickNAMIR/`, untouched since QA-A `27a10bd2`; every plan line ref verified live. QA-Fb's Option A merge point ("buffer += takes → rack + NAM process the summed stack") actually CONFIRMS the plan's premise — the Mic B sum happens inside NAMIR's own processBlock, in-engine + MT-orthogonal.
- **Task 1 — processor (`BaySickNAMIRProcessor.h/.cpp`) — CODED.**
  - 8 `_b_` params added to createLayout: `nam_micb_active` (Bool, false), `nam_micsim_b_mode` (Choice None/Built-in/User IR), `nam_micsim_b_model` (Choice 10, Live Vocal Dynamic), `nam_micsim_b_mix` (0-100, 100), `nam_placement_b_distance_cm` (1-150, 30), `nam_placement_b_angle_deg` (+/-90, 0), `nam_placement_b_polar` (Choice 5, Cardioid), `nam_placement_b_mix` (0-100, 100).
  - New members `mMicSimB`/`mMicPlacementB` (prepared in prepareToPlay) + scratch `mPreMicScratch`/`mMicBScratch` (2ch × host block).
  - processBlock: new stage 7b-pre taps the post-cab buffer into `mPreMicScratch` BEFORE Mic A processes in-place; new stage 7d copies the tap to `mMicBScratch`, runs Mic Sim B + Placement B on it, and SUMS into the main buffer through a 15 ms activation ramp (`mMicBGain`) — toggling `nam_micb_active` mid-play glides instead of stepping the +6 dB correlated jump (no-click; satisfies scenario 6). 15 ms ramp constant = my calibration (commented in source).
  - `micBRun = active || gain > 0` keeps the branch alive through the ramp-out; settled-off cost = one param read + two float compares + a branch (audio-thread fast-path rule honored).
  - Rising-edge reset of both B DSPs on re-activation — the B convolution FIFO + filter state hold stale audio from the last active period. MicSim/MicPlacement `reset()` verified RT-safe (IIR state zero + Convolution::reset + buffer clear, no allocation).
- **Task 2 — SlotSnapshot + serialization — CODED.** SlotSnapshot 17 → 26 fields (9 new: `micbActive`, `micSimBMode`, `micSimBModel`, `micSimBMix`, `micbUserIrPath`, `placementBDistance`, `placementBAngle`, `placementBPolar`, `placementBMix`); defaults = param defaults, so pre-dual-mic saves (no B properties in the tree) restore to Mic B off, byte-identical. toValueTree / fromValueTree / captureSnapshotFromCurrent / applySnapshotToCurrent all extended; apply mirrors the A-side load-if-different user-IR logic against `mMicSimB`; setStateInformation's per-slot IR loop loads B IRs the identical way + syncs `mMicSimB.setActiveSlot(mLastSlot)`; parameterChanged's ab_slot branch also points `mMicSimB` at the new slot (per-slot IRs stay resident → instant switch). New processor API mirrors A: `getMicSimB()` / `getMicPlacementB()` / `loadUserMicIrB()` / `clearUserMicIrB()` (same `mLoadLock`). **Judgment (logged):** NO `mic_user_ir_path_b` legacy global property written — the existing global is only the pre-A/B-snapshot fallback, and no pre-dual-mic project can carry a B IR; snapshots are the sole B carrier.
- **Task 3 — editor (`BaySickNAMIREditor.h/.cpp`) — CODED.** Mic Sim + Mic Placement rows split into Mic A | Mic B columns (colW = (760 - 3×12)/2 = 362; A sections relabeled "MIC SIM A" / "MIC PLACEMENT A"). "Mic B Active" DualLabelToggle (OFF/ON, white labels, ButtonAttachment to `nam_micb_active`) top-right of column B inside a 48 px header band (DualLabelToggle's Named layout needs 12+1+22+1+12 = 48). Full B control set mirrors A, bound to the `_b_` params: mode + polar ChickenHeadSelectors, model combo, user-IR button/filename label, 4 VKnobs (72 px). Dim/disable when off via `updateMicBEnabled()` (setEnabled + alpha 0.4; the two selectors use `setLocked` so hover/tooltips stay readable while locked). The user-IR filename label now sits IN the model combo's slot (the two are mode-exclusive: Built-in shows combo, User IR shows label) — needed to fit the half-width column. Painted vertical divider between columns (`mMicColumnDivider`). **Layout FITS the existing 760×560** (placement content bottom = 522 → 38 px margin) — NO `kEditorH` bump, NO host-bounds changes (plan had allowed one if needed). Dead constants `kMicSimRowH`/`kMicPlaceRowH` deleted (the rewrite orphaned the first; the second was already unreferenced).
- **§B.9 AUTHORED** in [`Test Plans/v1-master-test-plan.md`](../Test Plans/v1-master-test-plan.md): FC-1 regression-off (incl. dimmed column + pre-batch project), FC-2 correlated +6 dB sum, FC-3 comb 30-vs-120 cm, FC-4 per-slot A/B + UI follow (covers the resync fix below), FC-5 persistence incl. per-slot B user IRs, FC-6 no-click toggle mid-play, FC-7 CPU fast-path, FC-8 both pages independent. Setup notes the engine gate: mic stages only run when the page has a NAM model or cab IR loaded (existing behavior, Mic A identical).
- **Test-plan-touch backfills (ride the QA-Fc commit):** §B.8 `blocks:` → `66fea472` (mandated by the resume protocol); the doc's own "backfilled at next touch" convention resolved for §B.4 → `44d5c015`, §B.5 → `67bd4f6e`, §B.6 → `9262c746` + `35ac9928`, §B.7 recovery → `62895ca8`. Held-entry placeholders resolved: `doubled-tracking-badger.md` Commit → `66fea472` (mandated); `melodic-bending-finch.md` Commits line + recovery-round heading → `62895ca8` (same convention).

### Found along the way — FIXED IN-BATCH (both columns)

- **Pre-existing UI staleness since H-6d (2026-05-02):** an A/B slot switch rewrites `nam_micsim_mode` / `nam_micsim_model` / `nam_placement_polar` via applySnapshotToCurrent, but those widgets are manual-sync (ChickenHeadSelector has no APVTS attachment; the model combo was wired manually) — the selectors kept showing the OUTGOING slot's positions while the audio switched correctly. Fixed: editor param listeners on the 3 A params + the 3 B twins (+ `nam_micb_active` for the dim state) → callAsync resync with change guards. Rationale for in-batch: §23 scenario 4 (per-slot A/B) would trip over it visually on the brand-new B column; fix-in-batch default per QA-batch discipline.

### Scenario adaptation (logged — not a spec change)

- §23 scenario 6 says "automate `nam_micb_active` in-DAW" — NAMIR params live in the engine's private APVTS, which DAW automation lanes cannot target (lanes resolve main-processor params). §B.9's FC-6 exercises the same code path with the executable gesture: click the toggle mid-play → the ramp glides.
- The Fc plan's Context line says "+3 dB on correlated content" — that's a misstatement (two identical summed signals = +6 dB; +3 dB is the uncorrelated power sum). §23's own scenario 2 (~+6 dB) is the correct physics and is what's implemented + tested.

### Status

- Rule 4 diagnostic catalog: **EMPTY** — no diagnostic instrumentation added in QA-Fc.
- Working tree: 4 source files modified (`BaySickNAMIRProcessor.h/.cpp`, `BaySickNAMIREditor.h/.cpp`) + test plan + 3 running-notes files (this one + the two held-entry backfills), UNCOMMITTED, BUILD NOT YET RUN.
- Next: Jeff's `do_build.bat` (both configs to clean) → held Work Log entry → commit message + FULL git status → approval → ONE commit.

## 2026-07-10 — Mid-batch interrupt (Jeff) — loop-seam staccato regression: diagnosed + fixed (own commit)

- **Interrupt (owner call — takes precedence over QA-Fc flow; Fc code-complete state untouched):** while setting up the Harmless-automation discriminating test (the engine-param automation question from earlier this session), Jeff hit a playback bug: ONE note spanning a FULL pattern (2-bar note, 2-bar pattern, pattern-mode loop) plays complete on pass 1, then every repeat plays a short staccato blip and goes silent for the rest of the pass. Pulled for immediate diagnosis.
- **Diagnosis (code-traced, then confirmed by Jeff's discriminator tests):** the QA-Ee Task 1c one-shot loop-wrap flush (`1bdc1552`, 2026-06-03) collides with the QA-Ed straddle path for exactly one gesture — a note that starts at loopStart AND ends exactly at loopEnd (offHi clamps its pending note-off to exactly loopEndBeat). Sequence: straddle block K emits the old off + refires the note at the exact wrap sample and queues the restarted note's off (== loopEndBeat); the playhead wraps at K's `advanceBlock` (process-then-advance order confirmed at `StandaloneApp.cpp:135`) and raises `mLoopWrapped`; block K+1's flush rule (`loopFlush && off.beatOff >= loopEndBeat` -> emit at 0) then kills the RESTARTED note's off one block into the new pass — Harmless's ~0.3 s default release turns that into the audible staccato blip, then silence until the next seam. First pass clean (no wrap yet). Only a full-pattern note arms it: a mid-pattern note ending at loopEnd has no freshly-queued off at loopEnd in the first post-wrap block (why the QA-Ee soak + a month of use never surfaced it).
- **Jeff's discriminators (all played clean — each breaks the trap condition, confirming the mechanism):** (1) same note moved to start at beat 2 — clean; (2) pattern extended past the note via a tiny extra note — clean; (3) Jeff's own variant: tiny note at the bar end making end-meets-start read as one continuous note — clean (needed Cut Self ON to avoid same-pitch overlap doubling — expected engine behavior, not part of the bug).
- **Attribution (honest, unresolved):** every group-era change through this code (QA-TempoMap's absolute-map conversions in scheduler + playhead) traced behavior-identical at constant tempo; the trap appears armed since the QA-Ed/QA-Ee seam rework (early June), latent until this gesture. Jeff reports the gesture worked pre-groups; attribution unresolved and doesn't change the fix. NOT caused by any QA-Fc code (NAMIR-only; likely not even in the binary under test).
- **Fix (coded, build pending):** new audio-thread-only member `mPRLastBlockStraddled` (`PluginProcessor.h:1133`, declared beside `mPRPendingOffs` with a thread-ownership + trap comment). Scheduler (`PluginProcessor.cpp`): the loop-wrap flush becomes `loopEndFlush = loopFlush && !mPRLastBlockStraddled` (`:2023`) — after a straddle block the flush is redundant (every pre-wrap off at/past loopEnd already fired at the wrap sample), so an off found at loopEnd right after a straddle belongs to the seam-restarted note and is KEPT. Flag updated to `straddle` after the pending-off pass each block (`:2040`); reset false in the transport-stopped branch (`:1898`). Case walk: full-pattern straddle seam FIXED; QA-Ee wrap-on-block-boundary case PRESERVED (no straddle precedes -> flush fires; restart + flush land in the same block, off-before-on order correct); mid-pattern note ending at loopEnd unchanged; sub-block degenerate loops unchanged (straddle always false); consecutive-straddle short loops consistent; song-loop mode gets the same guard; single scheduling site shared by serial + MT.
- **Process:** fix rides as its OWN commit (`PluginProcessor.h/.cpp` — file-disjoint from QA-Fc's 4 NAMIR files; two clean commits from one tree). Verify folded into Jeff's next build cycle alongside QA-Fc's build: (1) original repro — the full-pattern note sustains every pass; (2) QA-Ee regression guard — mid-pattern note ending exactly at loop end still releases at the seam, no hang; (3) then the deferred Harmless-automation lane test. Rule 3: finding + fix logged here; §9 Forks entry (QA-Ed/QA-Ee-era latent bug surfaced + fixed mid-QA-Fc) rides the section pass. Rule 4: no diagnostic instrumentation added — diagnosis was git/code archaeology + owner-run UI discriminators; the catalog stays EMPTY.

## 2026-07-10 — Interrupt closed + batch committed — seam fix verified, automation gap routed

- Jeff verified the seam fix (Debug + Release build CLEAN): (1) the full-pattern-note repro sustains every pass; (2) QA-Ee regression guard — a mid-pattern note ending exactly at the loop point still releases cleanly at the seam. Fix committed as `32930dca` (own commit, PluginProcessor only).
- QA-Fc committed as `a36ed3cc` (8 files: 4 NAMIR source + test plan §B.9/backfills + 3 notes files). Batch code-complete + build-clean; §B.9 verification rides the campaign (R2).
- Harmless-automation discriminator (the deferred test): Jeff confirms lanes do NOT drive engine-page params — the application gap from the earlier code trace is REAL. Jeff's picks (numbered docket, 2026-07-10): **(2a)** the seam fix gets its OWN Work Log entry at section pass (held below); **(3a)** the automation gap stays with QA-ApvtsAutomation at its G4 slot — §5 docket annotated as CONFIRMED (not audit-hypothesis); §9 Forks entry rides the section pass.
- Held entries below updated to match: the QA-Fc entry's Routed section amended (interrupt + automation-gap routings); the seam fix's own held entry appended after it.

## Held Work Log entry (apply at section pass)

> Apply to `Implemented Work Log.md` when §B.9 passes (R2). Stamp `HH:MM PT` at apply time.

```markdown
### 2026-07-10 — QA-Fc — BaySickNAMIR dual-mic stack (parallel Mic B, summed)

**Bucket:** Players
**Plan:** `Batch Plans/twinned-miking-ferret.md` · **Running notes:** `Running Notes/twinned-miking-ferret.md` · **Commit:** `<hash at commit>`

#### Done

- **Task 1 — processor: parallel summed Mic B stage (`BaySickNAMIRProcessor.h/.cpp`).** 8 new `_b_` params in createLayout: `nam_micb_active` (Bool, default false — off = byte-identical output) + Mic Sim B mode/model/mix + Placement B distance/angle/polar/mix, defaults mirroring the A set (model Live Vocal Dynamic, polar Cardioid, 30 cm / 0 deg / 100% mixes). New `mMicSimB` + `mMicPlacementB` DSPs (prepared in prepareToPlay) + scratch `mPreMicScratch`/`mMicBScratch`. processBlock: new stage 7b-pre taps the post-cab buffer BEFORE Mic A processes in-place; new stage 7d runs Mic Sim B + Placement B on the tap and SUMS it into the main buffer through a 15 ms activation ramp (`mMicBGain`) — a mid-play toggle glides instead of stepping the +6 dB correlated jump (15 ms = own calibration, commented in source). `micBRun = active || gain > 0` keeps the branch alive through ramp-out; settled-off cost = one param read + two float compares + a branch (audio-thread fast-path rule). Rising-edge reset of both B DSPs on re-activation (the B convolution FIFO + filter state hold stale audio from the last active period; both resets verified RT-safe). Hosted per-page on both Vox + Inst (each owns its own NAMIR processor) — the change lands once, propagates to both.
- **Task 2 — SlotSnapshot + serialization.** SlotSnapshot 17 -> 26 fields (9 new: the 8 B params + `micbUserIrPath`); defaults = param defaults, so pre-dual-mic saves (no B properties in the tree) restore to Mic B off, byte-identical — zero migration. toValueTree / fromValueTree / captureSnapshotFromCurrent / applySnapshotToCurrent all extended; apply mirrors the A-side load-if-different user-IR logic against `mMicSimB`; setStateInformation's per-slot IR loop loads B IRs the identical way + syncs `mMicSimB.setActiveSlot(mLastSlot)`; the ab_slot `parameterChanged` branch also points `mMicSimB` at the new slot (per-slot IRs stay resident -> instant A/B switch). New processor API mirrors A: `getMicSimB()` / `getMicPlacementB()` / `loadUserMicIrB()` / `clearUserMicIrB()` (same `mLoadLock`). Judgment (logged): NO `mic_user_ir_path_b` legacy global property — the existing global is only the pre-A/B-snapshot fallback and no pre-dual-mic project can carry a B IR; snapshots are the sole B carrier.
- **Task 3 — editor: Mic A | Mic B column split (`BaySickNAMIREditor.h/.cpp`).** Mic Sim + Mic Placement rows split into Mic A | Mic B columns (colW 362, painted vertical divider; A sections relabeled MIC SIM A / MIC PLACEMENT A). "Mic B Active" DualLabelToggle (ButtonAttachment to `nam_micb_active`) top-right of column B in a 48 px header band; full B control set mirrors A bound to the `_b_` params (mode + polar ChickenHeadSelectors, model combo, user-IR button/filename label, 4 VKnobs at 72 px). Column dims/disables when off via `updateMicBEnabled()` (setEnabled + alpha 0.4; the two selectors use setLocked so hover/tooltips stay readable). The user-IR filename label shares the model combo's slot (the two are mode-exclusive) to fit the half-width column. Layout FITS the existing 760x560 (content bottom 522 -> 38 px margin) — NO kEditorH bump, NO host-bounds changes (the plan had allowed one if needed). Dead constants kMicSimRowH / kMicPlaceRowH deleted (the rewrite orphaned the first; the second was already unreferenced).
- **Test plan.** §B.9 authored (FC-1..FC-8): regression-off incl. dimmed column + pre-batch project, correlated +6 dB sum, comb 30-vs-120 cm, per-slot A/B + UI follow (covers the resync fix below), persistence incl. per-slot B user IRs, no-click toggle mid-play, CPU fast-path, Vox + Inst pages independent. Setup notes the engine gate: mic stages only run when the page has a NAM model or cab IR loaded (existing behavior, Mic A identical).
- **Test-plan-touch hash backfills (rode this commit).** §B.8 `blocks:` -> `66fea472` (mandated by the resume protocol); the doc's own "backfilled at next touch" convention resolved §B.4 -> `44d5c015`, §B.5 -> `67bd4f6e`, §B.6 -> `9262c746` + `35ac9928`, §B.7 recovery -> `62895ca8`. Held-entry placeholders resolved: `doubled-tracking-badger.md` Commit -> `66fea472`; `melodic-bending-finch.md` Commits line + recovery-round heading -> `62895ca8`.
- **Files:** `Source/BaySickNAMIR/BaySickNAMIRProcessor.h/.cpp`, `Source/BaySickNAMIR/BaySickNAMIREditor.h/.cpp` + test plan (§B.9 new + §B.4-B.8 hash backfills) + running notes (this batch + the badger/finch placeholder resolutions).

#### Found along the way

- **Pre-existing UI staleness since H-6d (2026-05-02) — FIXED in-batch, both columns.** An A/B slot switch rewrites `nam_micsim_mode` / `nam_micsim_model` / `nam_placement_polar` via applySnapshotToCurrent, but those widgets are manual-sync (ChickenHeadSelector has no APVTS attachment; the model combo was wired manually) — the selectors kept showing the OUTGOING slot's positions while the audio switched correctly. Fix: editor param listeners on the 3 A params + the 3 B twins (+ `nam_micb_active` for the dim state) -> callAsync resync with change guards. In-batch rationale: §23 scenario 4 (per-slot A/B) would trip over it visually on the brand-new B column; fix-in-batch default per QA-batch discipline.
- **The Fc plan Context's "+3 dB on correlated content" is a misstatement** — two identical summed signals = +6 dB (+3 dB is the uncorrelated power sum). §23's own scenario 2 (~+6 dB) is the correct physics and is what shipped + what FC-2 tests. Plan-assumption correction only; no code impact.

#### Spec calls

- Locked pre-batch: G2-23 (parallel Mic B, SUMMED not blended; 8 `_b_` params; off = byte-identical), G2-23snap (SlotSnapshot +9 fields, per-slot dual-mic state), G2-23ui (Mic A | Mic B column split + Active toggle) — all per `Running Notes/phantom-recording-mongoose.md` §23 (locked 2026-05-14). ZERO new spec calls surfaced mid-batch; the G2-23ui height-bump allowance resolved as an implementation calc (no bump needed).
- Scenario adaptation (logged — not a spec change): §23 scenario 6's "automate `nam_micb_active` in-DAW" is unreachable — NAMIR params live in the engine's private APVTS and DAW automation lanes resolve main-processor params only. §B.9 FC-6 exercises the same ramp code path via the executable gesture: click the toggle mid-play -> the ramp glides.

#### Routed (Rule 3)

- The H-6d selector staleness finding was pulled in-batch (fix above); the batch's own surface stayed orthogonal (`Source/BaySickNAMIR/` only, untouched since QA-A `27a10bd2`).
- Mid-batch interrupt (Jeff): loop-seam staccato on full-pattern notes — diagnosed + fixed same session as its OWN commit `32930dca` with its OWN Work Log entry (Jeff pick 2a, 2026-07-10); see the second held entry below + the running-notes interrupt entries.
- Engine-param automation application gap CONFIRMED by owner test (lanes draw but never drive engine-page params — both application sites resolve main-APVTS ids + the applicator registry only): routed to QA-ApvtsAutomation (Jeff pick 3a, 2026-07-10 — stays at its G4 slot; §5 docket annotated). BaySickNAMIR's missing componentID stamping (no Automate menu at all) rides the same batch's audit scope.

#### Diagnostic Instrumentation Catalog

- Empty — no diagnostic instrumentation added in QA-Fc (Rule 4); nothing to strip.
```

## Held Work Log entry — loop-seam fix (apply at section pass, own entry per Jeff pick 2a)

> Apply to `Implemented Work Log.md` alongside the QA-Fc entry when §B.9 passes (R2). Stamp `HH:MM PT` at apply time. Already owner-verified 2026-07-10 (both configs) — no campaign scenarios gate it; it applies with the section pass for chronology only.

```markdown
### 2026-07-10 — Loop-seam fix (mid-QA-Fc interrupt) — full-pattern-note staccato on loop repeats

**Bucket:** Cross-cutting Infrastructure
**Plan:** none (owner-pulled interrupt; diagnosed + fixed same session) · **Running notes:** `Running Notes/twinned-miking-ferret.md` (interrupt entries) · **Commit:** `32930dca`

#### Done

- **Symptom (Jeff, mid-QA-Fc):** a note spanning a FULL pattern (starts at loopStart, ends exactly at loopEnd; 2-bar note in a 2-bar pattern, pattern-mode loop) played complete on pass 1, then staccato-blipped and went silent on every repeat.
- **Root cause:** the QA-Ee Task 1c loop-wrap flush (`1bdc1552`, built for the wrap-on-a-block-boundary case the straddle test misses) is REDUNDANT in the block after the scheduler's straddle path already handled the wrap sample-exactly — and its `beatOff >= loopEndBeat` rule caught the seam-RESTARTED note's own pending off (clamped to exactly loopEndBeat by offHi), emitting it one block into the new pass; Harmless's ~0.3 s default release rendered that as the staccato. Only a full-pattern note arms it (a mid-pattern note ending at loopEnd has no freshly-queued off at loopEnd in the first post-wrap block) — why the QA-Ee soak and a month of use never surfaced it. Mechanism confirmed by owner discriminator tests before the fix was written (note moved off loopStart -> clean; pattern extended past the note -> clean).
- **Fix:** new audio-thread-only `mPRLastBlockStraddled` (PluginProcessor.h, beside `mPRPendingOffs`); the flush becomes `loopEndFlush = loopFlush && !mPRLastBlockStraddled` so it only fires for wraps the straddle did NOT handle; flag updated per block after the pending-off pass, reset in the transport-stopped branch. Case walk in the running notes: QA-Ee boundary case preserved (no straddle precedes -> flush fires; restart + flush land in the same block, off-before-on order correct); mid-pattern notes ending at loopEnd, sub-block degenerate loops, song-loop mode, stop/restart, serial + MT all unchanged.
- **Verified by Jeff same session** (Debug + Release build clean): the full-pattern repro sustains on every pass; the regression guard (mid-pattern note ending exactly at the loop point) still releases at the seam with no hang.
- **Attribution:** latent since the QA-Ed/QA-Ee seam rework (June 2026), armed only by this gesture; group-era changes through this code (QA-TempoMap's absolute-map conversions) traced behavior-identical at constant tempo. Not QA-Fc-related (NAMIR-only surface).

#### Found along the way

- The Harmless-automation test that triggered the gesture confirmed the engine-param automation application gap — routed to QA-ApvtsAutomation per Jeff pick 3a (see the QA-Fc entry's Routed section + the §5 docket annotation).

#### Diagnostic Instrumentation Catalog

- Empty — diagnosis was git/code archaeology plus owner-run UI discriminators (Rule 4); nothing to strip.
```
