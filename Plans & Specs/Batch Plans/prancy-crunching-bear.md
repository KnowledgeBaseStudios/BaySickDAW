# QA-Fe — Vocal Pitch Engine: adopt library engines, retire PSOLA — Plan (prancy-crunching-bear)

> **Canonical path:** `Plans & Specs/Batch Plans/prancy-crunching-bear.md`. **Paired running notes:** `Plans & Specs/Running Notes/prancy-crunching-bear.md`.
> **For execution:** BULK-RUN — **ONE commit at batch close** + Master Test Plan §B.## backfill; per-task lines are **Build checkpoints (no commit)**. Jeff's hands-on ear verification happens at the **G2 boundary smoke**. QA-Fe rides the **OPEN G2 boundary**; the boundary closes after QA-Fe's smoke.
> **Sixth batch of G2:** QA-F → QA-Fa → QA-Fb → QA-Fc → QA-Fd → **QA-Fe** → QA-G.

---

## RESCOPE NOTE (2026-07-13) — why this plan changed

This batch was originally "restore + rebuild the shared PSOLA (`PsolaShifter`) engine" (Tasks 1-8 below the fold in git history). That approach was **abandoned after 3 days** of execution proved TD-PSOLA is the wrong engine, per the full findings in the §9 fifty-seventh Forks entry. The short version:

- The PSOLA revert + grain-length fix restored *some* shift, but the app still under-delivered (up ~50%, down ~0) and the **moiré warble is inherent to nearest-epoch TD-PSOLA** — worst at the small shifts real-time correction uses, and sub-sample placement did not reduce it.
- An honest read of "what do the real tools use" (Melodyne / elastique / Auto-Tune) is **spectral / source-filter vocoders, NOT PSOLA.** PSOLA is the low-latency-but-artifacty engine.
- A/B on Jeff's vocal of three library engines — **WORLD** (BSD), **Signalsmith Stretch** (MIT), **Rubber Band R3** (GPL v2+) — all shifted cleanly, formant-preserving, both directions, at a fraction of the measurement pain. Jeff's ear: WORLD best, Rubber Band closest to WORLD.
- App is open-source (JUCE GPLv3 path) → all three are free + legal to ship.

**New direction:** editor + Align get a user-selectable **3-engine dropdown** (Rubber Band / Signalsmith / WORLD); the real-time **vocal** correction moves to **Rubber Band `R3LiveShifter`** (~48 ms, clean, dry-monitor default); **PSOLA + both GranularShifters are retired** from the vocal paths. The octave *instrument* pedal is a SEPARATE engine (`OctaveStyleDSP`, never PSOLA) and its fix is a new batch (QA-OctavePedal), NOT this one.

---

## Context

**Why this batch.** The vocal pitch engine (editor, Align, real-time correction) was broken and, after the 3-day PSOLA investigation, judged unfixable in-kind. QA-Fe replaces it with vendored library engines that measurably shift cleanly on Jeff's voice. Risk: high-touch — three vendored libraries + APVTS/UI + a real-time engine swap + a monitor-path UX change. Mitigations: engines already A/B-validated on the real vocal; Rubber Band already vendored + LiveShifter latency measured (~48 ms); adversarial review on the audio-thread changes at close.

**Dependencies.** QA-Fd code-complete (2026-07-11). QA-Fe UNBLOCKS the G2 boundary smoke (halted at Part 4 by the pitch editor). Nothing downstream starts until QA-Fe's smoke passes.

**Vendored engines (all in `libs/` after the 2026-07-13 vendoring):** `rubberband` (GPL v2+, R3 offline + R3LiveShifter), `world` (modified-BSD), `signalsmith-stretch` + `signalsmith-linear` (MIT). All compile statically into the exe (no user-facing DLLs).

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| B1 | **Editor + Align = 3-engine dropdown: Rubber Band / Signalsmith / WORLD.** Default **Rubber Band.** | Jeff, 2026-07-13. All three clean + license-clean; Rubber Band closest to WORLD by ear + general-purpose + moderate CPU. |
| B2 | **Engine labels pair quality + CPU:** "Rubber Band — Balanced" (default) / "WORLD — Highest Quality (High CPU)" / "Signalsmith — Lightest (Low CPU)". | Jeff, 2026-07-13. Beginners must read WORLD as *the good one that costs more*, not *the expensive one*. |
| B3 | **Real-time VOCAL correction (`PitchCorrectorDSP`) = Rubber Band `R3LiveShifter`.** Dry-monitor default. | Jeff, 2026-07-13. Clean at small corrections where PSOLA warbled worst; ~48 ms is invisible under dry monitoring. |
| B4 | **Monitor button UX:** on the mixer-strip monitor button, **right-click → 2-option popup at the mouse (Dry / With Effect), default Dry.** | Jeff, 2026-07-13. Zero-latency dry monitor by default; opt-in processed monitor (accepts the ~48 ms) for those who want it. |
| B5 | **PSOLA (`PsolaShifter`) + `PitchShifters.h::GranularShifter` retired** from the editor / Align / vocal-correction paths. `OctaveStyleDSP::GranularShifter` is a DIFFERENT class (the octave pedal) — untouched by QA-Fe. | Jeff + code audit. The three vocal PSOLA consumers are the only ones. |
| B6 | **Throat/character control** maps to each engine's native formant control (Rubber Band `setFormantScale` / Signalsmith `--formant` / WORLD envelope shift), NOT the old cepstral PSOLA stage. | A9 carried forward onto the new engines. |
| B7 | **Editor/Align offline engines integrate via bake-on-edit → cache → playback reads cache** (per A12). WORLD (~3.6x realtime) especially benefits; Rubber Band offline + Signalsmith are light enough to bake fast. | A1 (post-recording, latency-tolerant) + CPU measurements. |
| B8 | **Bulk-run cadence** — one commit at close + Master Test Plan backfill; per-task = build checkpoints. | A11 + Jeff. |
| B9 | **Add a GPLv3 LICENSE file to the repo** (JUCE GPLv3 open-source path; makes the vendored GPL Rubber Band airtight). | Jeff, 2026-07-13 (app is open-source giveaway). |
| B10 | **The octave/instrument pedal is OUT of QA-Fe** — new batch QA-OctavePedal (bulk-run G3 / Main Plan Phase 5). Builder-grid clip-stretch engine pick (Signalsmith vs Rubber Band) folds into the Builder batch (QA-H / BUILD-06). | Jeff, 2026-07-13. Separate engine + separate concern. |

---

## Files to modify

- **`CMakeLists.txt`** — compile in `world` + `signalsmith-stretch`(+`signalsmith-linear`); link/enable `rubberband` (currently headers-only); MSVC `/MD` runtime match (`reference_msvc_runtime_md_md_match`); add each license notice.
- **New `LICENSE`** at repo root — GPLv3 (B9).
- **`Source/DSP/`** — new engine wrappers behind a thin `IPitchShifter` seam (offline: Rubber Band R3 / Signalsmith / WORLD; real-time: Rubber Band `R3LiveShifter`); bake-cache in `BaySickPitchDSP`; delete `PsolaShifter` + `PitchShifters.h::GranularShifter` + strip `[PITCH DIAG]`.
- **`Source/DSP/BaySickAlignDSP.cpp/.h`** — engine dropdown branches (drop Granular, add the three); `bsa_pitch_algo` remap + old-project migration.
- **`Source/DSP/PitchCorrectorDSP.cpp/.h`** — swap `PsolaShifter` → Rubber Band `R3LiveShifter`; dry-monitor default.
- **`Source/BaySickVocal/BaySickVocalProcessor.cpp`** — engine param (`bsp_engine` 0..2, default 1=Rubber Band); throat param; forward.
- **`Source/BaySickVocal/BaySickPitchEditor.cpp` / `BaySickAlignEditor.cpp`** — engine selector UI (labels per B2); throat control.
- **Mixer-strip monitor button** (`Source/Standalone/MixerTrackStrip.cpp` or the strip that owns the monitor button — verify) — right-click → 2-option popup (B4) + the processed-monitor path.

---

## Tasks

### Task 1 — Vendor wiring + build
- [ ] CMake: compile `world` + `signalsmith-stretch`(+`signalsmith-linear`) statically; enable/link `rubberband`; MSVC `/MD` match; per-lib license notice; add repo-root GPLv3 `LICENSE` (B9).
- [ ] **Tell Jeff:** rebuild — app builds clean with all four engines linked (Debug + Release).
- [ ] **Build checkpoint (no commit).**

### Task 2 — Engine seam + bake-cache (editor + Align, offline)
- [ ] Thin `IPitchShifter` over the three offline engines (Rubber Band R3 / Signalsmith / WORLD). `BaySickPitchDSP::applyEditsToBuffer` selects by the engine param. Bake-on-edit → cache → playback reads cache (B7); invalidate on edit change.
- [ ] Delete `PsolaShifter` + `PitchShifters.h::GranularShifter` from the editor/Align paths; strip `[PITCH DIAG]` (record the removal in the running-notes Diagnostic Instrumentation Catalog).
- [ ] **Build checkpoint (no commit).**

### Task 3 — Editor + Align dropdown + labels
- [ ] Register `bsp_engine` (0=Rubber Band default, 1=Signalsmith, 2=WORLD; Align mirrors on `bsa_pitch_algo`). UI selector with the B2 labels; migrate old Align Granular(1) selections → default. Default Rubber Band.
- [ ] **Tell Jeff (verify):** pick each engine in editor + Align, play — each shifts cleanly, formant-preserving, both directions; labels read right; old Granular project loads on Rubber Band.
- [ ] **Build checkpoint (no commit).**

### Task 4 — Real-time vocal correction → LiveShifter
- [ ] Swap `PitchCorrectorDSP`'s `PsolaShifter` → Rubber Band `R3LiveShifter` (block-based, ~48 ms, `OptionFormantPreserved | OptionWindowShort`). Dry-monitor default (B3).
- [ ] **Tell Jeff (verify):** real-time correction on a small (cents-level) correction sounds clean, no warble; recorded/output signal is corrected; dry monitor is lag-free.
- [ ] **Build checkpoint (no commit).**

### Task 5 — Monitor-button UX
- [ ] Mixer-strip monitor button: **right-click → 2-option popup at the mouse (Dry / With Effect), default Dry** (B4). Wire the "With Effect" processed-monitor path. **Loud paper-trail** on the right-click behavior change (`feedback_option_removal_needs_loud_paper_trail`) — read the current right-click handler first.
- [ ] **Tell Jeff (verify):** right-click the monitor button → popup at the cursor; Dry = lag-free own voice; With Effect = processed (accepts delay); default Dry.
- [ ] **Build checkpoint (no commit).**

### Task 6 — Throat/character control
- [ ] Expose throat as an APVTS param mapped to each engine's formant control (Rubber Band `setFormantScale` / Signalsmith formant / WORLD envelope) — B6. Editor knob near Focus/Mod/Speed.
- [ ] **Tell Jeff (verify):** move throat — voice bigger/smaller (formant only, pitch unchanged) on each engine.
- [ ] **Build checkpoint (no commit).**

### Task 7 — Retire + clean
- [ ] Remove all PSOLA/Granular remnants + `[PITCH DIAG]` + the throwaway `Tools/signalsmith-test/` + `Tools/rubberband-test/` scaffolding + `Tools/pitch-sim/` (the sims served their purpose). Strip the repo-root `enable_pitch_diag.txt` + the loose `SIM_*.wav` / `PV_*.wav` / `RBLIVE_*.wav` / `SIGSMITH_*.wav` / `WORLD_*.wav` / `RUBBERBAND_*.wav` A/B renders.
- [ ] **Build checkpoint (no commit).**

---

## Verification — the G2 boundary smoke + ear-checks (RE-RUN)

> This is the boundary smoke that **halted at Part 4 during QA-Fd** and was never
> completed — the 3-day PSOLA chase blocked it.  QA-Fe unblocks it; re-run the FULL
> ladder below after QA-Fe builds clean.  **Release exe for all listening** (Debug
> first only for the smoke ladder, per standing rule).  Don't run both exes at once.
> Overlapping-same-row multi-take is deliberately NOT in this pass — campaign territory.
> Adapted 2026-07-13 for the library-engine model (dropdown engines, LiveShifter,
> dry-monitor); engine-agnostic timing/lifecycle checks left as-is.

### Part 1 — Smoke ladder (Debug exe first, watch for assert dialogs; then repeat in Release)
- Launch, default project, press Play — audio plays, meters move, no error dialogs (screenshot any that pop).
- Load the big multi-engine stress project — all tabs restore, playback doesn't glitch.
- Save -> close the app -> reopen -> load — tabs, patterns, mixer, engine settings all identical.
- Review spot-checks: none needed — the QA-Fd review came back clean.

### Part 2 — Ear-check rig (one-time setup, Release)
- Vox 1 = a "leader" take.  Vox 2 = the same phrase sung again, deliberately a little late + a few flat/sharp notes (record or drop files).  Clips side-by-side per channel — never stacked same-row.
- Mic on your ASIO input for Parts 4-6 (arm LED on the Vox strip -> input picker), your normal 128-sample buffer.

### Part 3 — Align quality (listen) [was QA-F]
- On Vox 2's tab open the BaySickAlign sub-tab; pick Vox 1's channel on the Leader lane.  **Engine dropdown = Rubber Band (default)**; optionally try Signalsmith / WORLD.
- Analyze/Apply, Play.  Vox 2's words now land WITH Vox 1, and the warped+pitched stream stays clean: no garble, no doubling/flam at word starts, no dropouts, no drift out of tune.
- Mid-play flip the editor ON/OFF a few times: OFF slides back to natural (late) timing as a short slur — no click/splice; ON slides back.
- Loose-Align, listen; Close-Align, listen — the difference in how hard timing pulls is obvious.
- Close-Align+Pitch: the flat notes pull toward the leader's pitch WITHOUT sounding robotic or erasing natural variation — formant-preserving, no chipmunk, on all three engines.
- **Judgment:** is this warped playback you'd actually use?  Does one engine sound clearly best?

### Part 4 — Pitch-edit quality (listen) [was QA-Fa — THIS is where the boundary halted]
- Open BaySickPitch (third Vox sub-tab) on a channel with a take — pills appear on their own.
- **Cycle the engine dropdown (Rubber Band / Signalsmith / WORLD)** — each shifts cleanly, formant-preserving; default Rubber Band.
- Edit mode: drag one slightly-off pill to the correct lane.  Play — sounds like YOU singing it right; timbre intact, no chipmunk, **no warble** (the whole reason for the engine swap).
- Drag a pill +7 semitones (deliberately extreme).  Play — obviously shifted but contained; no smearing/garbling/dropouts on any engine.
- Move the Throat control off-center: character changes (thinner/fuller) while pitch stays put — on each engine.
- Bake/engine ON/OFF mid-note: the correction glides out and back — no click.
- **Judgment:** musical enough to ship?  Which engine is your confirmed default?
- **Watch item (QA-Fd review NIT 4):** after you move a clip on the Builder grid (transport stopped), the analysis re-runs ~1 s later ON THE UI THREAD — on a LONG channel the app visibly freezes for that duration.  Locked design; judge whether the freeze is acceptable on real material — if not, it's a call.

### Part 5 — Real-time correction / tracking quality (listen) [was QA-Fb']
- Vox tab's BaySickVocals sub-tab: **Realtime Pitch ON** (bypass off) — now runs the Rubber Band `R3LiveShifter`.  Crank one obvious chain setting (heavy saturation or compression).
- **Monitor default = Dry** (right-click the strip's monitor button -> Dry / With Effect popup).  Record take 1 (short melody), stop.
- Record take 2 while take 1 plays.  As you track: your OWN voice comes back **dry, zero-latency** (the default); take 1 plays back processed (the chain's character).  No clicks/dropouts at record start/stop.
- Now right-click monitor -> **With Effect**: your live voice returns pitch-corrected through the chain with the LiveShifter's ~48 ms delay (a slapback you'll feel) — the opt-in path.  Confirm the processed result is clean.
- Stop, play the stack — the RECORDED take is corrected + processed regardless of which monitor mode you tracked in.
- **Judgment:** is dry-monitor tracking comfortable, and is the recorded correction good enough to ship?

### Part 6 — Real-time engage artifact (accept-or-flip) [reframed from the old docket-4 first-engage tick]
- Plain English: the `R3LiveShifter` carries ~48 ms latency.  When the "With Effect" monitor (or the Realtime Pitch bypass) engages/disengages mid-note, the wet path acquires/drops that delay — a one-time jump/tick at the toggle edge, both directions (bigger than the old ~13 ms PSOLA tick).
- Monitor your mic, Realtime Pitch bypassed.  Hold a steady note, right-click monitor -> With Effect mid-note — listen for the delay-jump as it engages.  Toggle back a few times.
- With correction ON, dial Throat away from zero mid-note — listen for any artifact at the change.
- **Decision:** a) accept — a documented one-time artifact at the toggle edges; or b) flip — smooth it (a crossfade-on-engage fix becomes its own discussion; I'll pose options if you pick this).

On smoke pass: **ONE commit** (QA-Fe close — brief Rule-9 one-liner, message + full `git status` surfaced for approval), Master Test Plan §B.## backfill, `/draft-doc batch-close` -> Implemented Work Log entry, and the **G2 boundary CLOSES**.

---

## Routing notes (Rule 3)
- Octave/instrument pedal fix + pedal-mode UI + low-latency instrument monitoring → **QA-OctavePedal** (bulk-run G3 / Main Plan Phase 5) — NOT this batch.
- Builder-grid clip-stretch engine pick (Signalsmith vs Rubber Band) → the Builder batch (QA-H / BUILD-06).
- Dev vendored-dependency update watcher → **QA-Updater** (dev-facing task).

## Carry-Forward Reference touch points
- **Task 1:** `reference_msvc_runtime_md_md_match`, `feedback_dont_overprune_vendored_libs`, the vendored `libs/{rubberband,world,signalsmith-stretch,signalsmith-linear}`.
- **Task 4:** the tested `Tools/rubberband-test/rb_live_render.cpp` (LiveShifter offline harness) for the R3LiveShifter API.
- **Engine A/B evidence:** the WORLD/Signalsmith/Rubber Band renders + CPU/latency measurements in the running notes.
