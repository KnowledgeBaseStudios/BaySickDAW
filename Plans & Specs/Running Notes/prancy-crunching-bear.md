# Running Notes — QA-Fe (prancy-crunching-bear)

> Append-only log. Populated at every checkpoint (build checkpoint / sub-task verify / finding / spec-call resolution / scope pivot) via `/draft-doc running-notes`, and consumed at batch close by `/draft-doc batch-close` as the primary input for the single Implemented Work Log entry.

Pair file: `Plans & Specs/Batch Plans/prancy-crunching-bear.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout (locked 2026-05-11)".

---

## 2026-07-12 — Batch open — QA-Fe execution begins

**Batch-open reads (three-doc system, Main Plan §0 Rule 1):** full direct
self-read of Main Plan §0 (all 9 standing rules + formatting/orchestration
conventions); Carry-Forward Reference checked (11 pitch/vocal hits — all the
frozen 2026-05-07 snapshot predates BaySickVocal, so no material pitch-DSP
carry-forward); Implemented Work Log confirmed latest entry = QA-UICleanup
(2026-07-08) — QA-F..QA-Fd close entries are HELD per R2 (bulk-run, apply at
their Master Test Plan §B section pass), so their absence from the log is
expected, not a gap.  Plan file (prancy-crunching-bear.md) + the backing
research report read in full.

**Main Plan registration (this session, per the batch-open instruction):**
- §5 — QA-Fe batch row inserted after QA-Fd (Bucket: Players, Effects; STATUS
  in-execution; the G2 boundary closes after QA-Fe's smoke).
- §6 — arrow now `... -> QA-Fd -> QA-Fe -> QA-G ...`; QA-Fe carries a new
  42-asterisk footnote (QA-Fd was the prior max at 41).
- §9 — fifty-seventh Forks entry (next ordinal after the fifty-sixth QA-Fd
  entry): trigger (the continuous-read-pointer change that deleted the shift),
  G2 composition change (five -> six batches), 8-task scope, the two Jeff
  spec-call gates (Task-5 WORLD GO/NO-GO + Task-6 stream-vs-bake).
- §5.5 left untouched — matches the QA-Fd precedent (QA-Fd is also absent from
  the §5.5 bucket table; the per-batch `**Bucket:**` line carries it instead).

**Working-tree starting state (uncommitted, inherited):** QA-Fd's code-complete
vocal-editor rework sits uncommitted (the M files in `Source/BaySickVocal/`,
`Source/DSP/`, `Source/Standalone/`, etc.), plus this session's pitch work on
top of committed `703f06e4`: the median-period stutter fix in
`BaySickPitchDSP.cpp` (KEEP), the read-pointer reseed in `PitchShifters.h`
(Task 1 REVERTS -> nearest-epoch snap), and the `[PITCH DIAG]` scaffolding
(Task 2 STRIPS).  QA-Fe layers on top; its ONE close commit is separate from
QA-Fd's pending close commit.

**Execution mode:** BULK-RUN — ONE commit at batch close + Master Test Plan
§B.## backfill; per-task lines are build checkpoints (no commit).  Jeff runs
all builds; Jeff's hands-on ear verification is the G2 boundary smoke.  Spec
calls surfaced, not decided (Task-5 WORLD gate + Task-6 bake UX).

**Cadence note:** running-notes entries written inline by the main session
(live context, accurate, cheap) at each checkpoint; `/draft-doc batch-close`
+ `/review-batch` dispatched once at close.  Consistent with the "don't burn
agents per-unit" discipline.

---

## Diagnostic Instrumentation Catalog (Rule 4)

`[PITCH DIAG]` scaffolding was shipped this session (committed in `703f06e4`
as the "temp PITCH DIAG applicator tracer") to fingerprint the non-shifting
PSOLA.  Per plan Task 2 it is stripped in-batch (the PSOLA grain scheduler is
rewritten there anyway).  Disposition surfaced to Jeff before the strip pass.
Unrelated pre-existing diagnostics (`MtDiagnostic`, `ClipDropDiag`,
`AlignAnalyzeDiag`) are NOT in scope — different subsystems, `Keep`.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `PitchShifters.h:183-200` (silence-gap counters in `processSample` + `diagFloorHits`/`diagGrainMin`/`diagPeriod`/`diagClear`) | `[PITCH DIAG]` | Silence-gap fingerprint: floor hits + min concurrent-grain count separate a spawn/overlap failure from a garble chop | Remove at Task 2 close |
| `PitchShifters.h:395-396` (`mDiagFloor`/`mDiagGrainMin` members) | `[PITCH DIAG]` | Backing state for the above | Remove at Task 2 close |
| `BaySickPitchDSP.h:322-343` (`struct Diag` + `Diag mDiag`) | `[PITCH DIAG]` | Applicator gate-tracer atomics (which gate eats the shift) | Remove at Task 2 close |
| `BaySickPitchDSP.h:386-387` + `AppState::diag*` fields | `[PITCH DIAG]` | Per-call plain counters copied into `Diag` atomics after each block | Remove at Task 2 close |
| `BaySickPitchDSP.cpp:563-564, 607-608, 760-766, 780-838` | `[PITCH DIAG]` | Applicator counters + fold-in of the shifter's silence-gap counters | Remove at Task 2 close |
| `BaySickPitchEditor.h:171-173` (`mDiagTick`/`mDiagArmed`) + `BaySickPitchEditor.cpp:1897-1932` | `[PITCH DIAG]` | Slow-path status-bar readout gated on `Documents/BaySickDAW/enable_pitch_diag.txt` | Remove at Task 2 close |
| repo-root `enable_pitch_diag.txt` (untracked) | flag file | Stray flag-enable file at repo root (the app reads the one under `Documents/BaySickDAW/`, not here) | Remove at Task 2 close (confirm w/ Jeff) |

---

## 2026-07-12 — Task 1 — Restore the pitch shift (code ported; build/ear-verify pending Jeff)

**Sim gate FIRST (per Jeff's instruction — prototype before porting).** The
prior sim suite (`psola_sim.py` / `sim_on_real.py`) only measured beat%/rough%
(CLEANLINESS) — never output F0. That is exactly why the continuous-read-
pointer ("reseed") scheme shipped: it scored cleanest while deleting the shift.
Added the missing measurement:
- `Tools/pitch-sim/f0_proof.py` — synthetic-voice F0 proof + a YIN (CMNDF)
  octave-robust estimator.
- `Tools/pitch-sim/f0_proof_real.py` — authoritative proof on the real dry
  vocal (`SIM_dry_reference.wav`); renders SNAP outputs for ear-check.

**Result (real dry vocal, dry F0 ~201 Hz):**
- RESEED (current broken C++): output F0 ratio **0.998-1.014** on -7/-3/+3/+7
  AND on the two already-rendered `SIM_*_reseed.wav` — the shift is deleted,
  confirming the research-report root cause on real audio.
- SNAP (revert target): tracks 2^(semis/12) — **+3 = 1.182 (0.6% err), -3 =
  0.860 (2.3% err)**; at +7 the YIN auto-read is fouled by the moire (locks a
  ÷3 subharmonic), but flooring the search >220 Hz reads **283.8 Hz** (quartiles
  224/284/320, expected ~302) — genuine up-shift, just moire-noisy. The moire is
  the Task-2 problem; Task 1 restores the shift and the moire returns (accepted).

**Port (Task 1 = smallest change, faithful revert to the validated 'snap').**
`Source/DSP/PitchShifters.h` `PsolaShifter::processSample`: replaced the
continuous phase-locked read-pointer block with per-mark nearest-epoch snap
(`e = nearestEpoch(outAbs - hw)`; clamp `if (e+hw > outAbs) e = target`;
`spawnGrain((double)e, hw)`; keep `mNextSynthAbs += pOut` as the cadence).
Removed `mReadPtr`/`mReadValid` members + their resets in `reset()` /
`resyncToWriteHead()` / the synth-clock-snap branch. Grep confirms zero
dangling refs. New keeper comment (Rule 6 cat.1) states the mark-snap IS the
shift mechanism and a wall-clock read must never come back. `[PITCH DIAG]`
scaffolding left intact (strips at Task 2). `hw` (ratio-coupled grain length)
untouched here — Task 2 changes it to fixed 2-period.

**Rendered for Jeff's ear-check** (in `Documents/BaySickDAW/`):
`SIM_up3_snap.wav`, `SIM_down3_snap.wav`, `SIM_up7_snap.wav` — A/B against the
broken `SIM_up3_reseed.wav` / `SIM_down3_reseed.wav`.

**Build checkpoint (no commit) — pending Jeff's build + ear verify.**

---

## 2026-07-13 — Task 1 — FINDING: shift restored in the DSP/sim but NOT reaching the app output (integration bug)

**Build was clean.** Jeff verified by ear + supplied 6 rendered WAVs (3 master
recordings of live playback: Clean/Up/down; 3 offline renders: Vox_1_pitch_v2/3/4
Clean/Up/Down), with up/down at MAXIMUM pill shift.

**Measurement (warble-robust, after autocorr/YIN/cepstrum/comb all got fooled by
the warble's amplitude modulation).** The decisive tool was a LOG-FREQUENCY
cross-correlation of the averaged spectra (up-vs-clean, down-vs-clean) over the
moved note (2.30-2.62s) -- it reads "by how many semitones is this spectrum
shifted" with no F0 estimator, and warble only adds a floor:
- clean-vs-clean = +0.0 st, corr 1.00 (sanity).
- MASTER: UP = +0.0 st, DOWN = +0.0 st.
- RENDER: UP = +0.5 st, DOWN = +0.0 st.
At MAXIMUM shift, both paths shift ~0 semitones. The averaged-spectrum overlay
confirms: clean/up/down all peak at ~263 Hz; up/down differ only by a raised
inter-harmonic floor (the warble). Signature = PsolaShifter running at ratio ~1.0
(near-identity pitch + grain-scheduler warble).

**This is the batch's founding trap resurfacing** (`verify-primary-effect-before-
optimizing`): do NOT proceed to Task 2 (warble cleanup) on a non-shifting engine.
Task 1 restored the shift in the DSP + sim, but the app integration isn't
delivering it.

**Code trace (apply path looks correct on paper):** editor pill-drag sets
`mRegions[i].shiftSemis` DIRECTLY on the DSP region list (`BaySickPitchEditor.cpp:656`,
`regions = pitch.regions()`); drag-release -> `commitEdit()` -> `publishEdits()`
copies `snap->regions = mRegions` wholesale (`BaySickPitchDSP.cpp:345`) incl.
shiftSemis + sets anyEdits; applicator reads `r.shiftSemis` -> `targetSemis` ->
`ratio = 2^((smoothedSemis+fastSemis)/12)` (`:751`) -> `shifters[ch].processSample`.
Warble-everywhere is explained + NOT a bug (whole-buffer shifter runs whenever
anyEdits; clean has no edits -> bails). The unexplained part: `ratio ~ 1` at the
moved note => `smoothedSemis ~ 0` => either (a) region-match `inRegion` never true
at playback (QA-Fd source-domain tSec/origin mismatch), (b) `shiftSemis` ~0 in the
snapshot (e.g. Snap-to-scale snapping the drag back), or (c) applicator computes
the shift but PsolaShifter doesn't deliver.

**Next: runtime bisection via the already-armed `[PITCH DIAG]` InfoBar readout**
(`maxSemi:` = peak|smoothedSemis|, `inReg:`, `regs:`, `chg:`, `per:`). Flag file
`Documents/BaySickDAW/enable_pitch_diag.txt` already exists -> readout is live.
maxSemi~0 + inReg 0 => region match broken; maxSemi~0 + inReg>0 => shiftSemis lost;
maxSemi large + no audible shift => PsolaShifter/period. Asked Jeff to move a pill
to max, play, and read the InfoBar line. **Task 1 NOT closed** until the shift
reaches the output.

---

## 2026-07-13 — Task 1 — DIAG result + measurement ceiling; correcting my earlier "0 shift" call

**DIAG readout (max pill drag, playing over the note):**
`blk:8793 null:0 off:0 neut:764 app:8029 inReg:128 regs:51 tSec:2.59
maxSemi:23.09 in:0.256 out:0.253 chg:128 floor:497 gMin:4 per:161.847`

Reading: the editor/snapshot/region pipeline is NOT the problem. The applicator
computes `maxSemi:23.09` (near the +-24 clamp = the max drag), feeds
`ratio = 2^(23.09/12) ~ 3.79` to PsolaShifter; `inReg:128` (region matches),
`per:161.8` (= 272 Hz input period, correct), `chg:128` (shifter modifies the
signal). So the shift IS requested and fed to the DSP correctly.

**Measurement ceiling (important — corrects an earlier wrong call).** I earlier
claimed "app shifts 0 st" from a log-frequency spectral cross-correlation. That
method is WRONG for a formant-preserving shift: PSOLA holds the formant envelope
fixed and moves only the harmonic comb, so full-spectrum xcorr locks onto the
(unmoving) envelope and always reports ~0. Retract that conclusion. Tried instead:
cepstrum (failed its own validation - octave errors), YIN/autocorr (octave errors
on warble), comb-spacing (subharmonic artifacts), per-frame classify (noise). At
+-23 st the warble depth ~ f0*|ratio-1| ~ 760 Hz obliterates the harmonic
structure -> the output pitch is genuinely UNMEASURABLE by tool or ear at extreme
shift. That is the Task-2 problem at its worst, not a distinct failure.

**What IS reliable:** (1) DIAG proves the app requests + feeds the shift
correctly. (2) My cleanest sim measurement -- modest shift, mild warble, median
YIN with octave-folding (`f0_proof_real.py`) -- shows the SNAP scheme shifts BOTH
ways: +3 -> 1.182x (+3 st), -3 -> 0.860x (-2.5 st), +7 -> genuine upshift. So the
scheme works at musical shifts. Jeff's ear at MAX: "up changed but not as high as
put; down same as up" == both directions buried in mush at extreme drag.

**Plan pivot:** verifying pitch on a +-23 st mush output is a dead end. The
definitive, hearable test is a SMALL musical shift (+-3 to +-5 st) where the
warble is mild. If a +3 pill clearly rises ~3 st and -3 clearly falls by ear in
the app, the engine is functional for normal use and the max-drag mush is Task 2's
job (kill the warble -> big shifts become usable too; also add extreme-ratio
sanity). Asked Jeff for the small-shift ear test. Tooling added this session
(numpy/matplotlib installed; `f0_proof.py`/`f0_proof_real.py` in Tools/pitch-sim).

---

## 2026-07-13 — Task 1 -> Task 2 boundary — ROOT CAUSE: ratio-coupled grain length kills downshift + undershoots up

**The reliable measurement finally landed** (small +-3 app renders, per-frame YIN
folded to the sample-aligned clean render; SELF-VALIDATING -- sanity = 0.00 st
outside the moved note). At the moved note, requested +-3:
- **UP  +3 requested -> +1.17 st  (undershoots to ~40%)**
- **DOWN -3 requested -> -0.02 st (dead, no shift)**
Jeff confirmed he dragged +-3 exactly. So both directions are wrong: up
undershoots, down is dead. Matches Jeff's day-one ear read ("up changed but not
as high as I put it").

**Root cause (mechanism, first-principles + sim-confirmed direction):** the grain
half-window `hw = jmax(pOut, jmin(P, 2*pOut))` (`PitchShifters.h:115`) is
RATIO-COUPLED. On a DOWNSHIFT `pOut = P/ratio > P`, so `hw = pOut` and the grain
length `2*hw = 2*pOut` spans SEVERAL original pitch periods -> each grain just
replays the original pitch -> downshift is impossible by construction. On upshift
`hw = P` (proper 2-period grain), which is why up at least partly works.

**Fix = the plan's Task-2 item 1 (fixed 2-period grain, `hw = P` independent of
ratio) -- but it is REQUIRED for the shift itself, not just warble.** So that one
Task-2 change belongs in restoring the shift. Sim prototype (`sim_grainfix2.py`,
self-validated sanity=0): at -7 the FIXED grain shifts -2.9 st vs the current
grain's 0.0 -- direction confirmed; clean magnitudes are defeated by the warble's
original-pitch energy even in the sim (same reason the app down-render read 0),
so the definitive magnitude check is an app re-render at +-3 with the reliable
aligned-clean method.

**Up undershoot (+1.17 of +3):** likely the applicator's pitch GLIDE (smoothing
`smoothedSemis` toward target over the note) -- the sim has NO glide and upshifts
nearly full. Re-check after the grain fix; if up still undershoots in the app,
investigate the glide/Speed default next.

**Next (pending Jeff's go):** port `hw = P` (Task-2 item 1) into
`PsolaShifter::processSample`, keep the kMaxGrains cap for big upshifts, Jeff
rebuilds + re-renders +-3, I measure with the aligned-clean method. Then the rest
of Task 2 (GCI centering already present; sub-sample placement) for warble polish.
This reorders the plan: the fixed-grain-length change moves from "Task 2 quality"
to "Task 1 restore-the-shift" (surface to Jeff -- it's a plan sequencing change).

**PORTED (Jeff go, 2026-07-13):** `PitchShifters.h:117` `hw = P` (was
`jmax(pOut, jmin(P, 2*pOut))`); grain-count cap (kMaxGrains=16) retained for big
upshifts; keeper comment (Rule 6 cat.1/3) records the downshift-correctness why;
stale `spawnGrain` half-window comment fixed. `pOut` still drives the synthesis
cadence. Pending Jeff rebuild + re-render +-3 -> aligned-clean re-measure.

---

## 2026-07-13 — MAJOR PIVOT — off PSOLA-grind, onto a library engine (Jeff-driven)

**hw=P fix did NOT restore the app shift** (Jeff rebuilt + re-rendered +-3:
aligned-clean measure = up +1.5 st of +3, down ~0 of -3, both STEADY across the
note, sanity=0). The output is a shifted+warble BLEND no estimator (or ear) can
read cleanly. Sub-sample fractional placement prototyped in the sim = NO moire
reduction (47%->47%): the moire is the discrete period-repetition inherent to
nearest-epoch TD-PSOLA, not sub-sample jitter. Also: the sim isn't a faithful
warble predictor (raw-peak epochs, no polarity-lock/LPC-GCI the real engine has)
-> can't prototype the warble fix in the sim.

**Owner-level reframe (Jeff, hard + fair):** the approved research question was
"what do the real tools use" -- the honest answer is spectral/vocoder methods,
NOT pure TD-PSOLA. I led with PSOLA (the artifacty low-latency engine) and ground
on it for ~3 days. Melodyne/elastique/modern Auto-Tune clean modes are
spectral/source-filter, not PSOLA; WSOLA is a marginal time-domain improvement,
not Melodyne-clean. Owned the deflection + the wrong "0-shift" call.

**Licensing re-scoped (app is OPEN-SOURCE giveaway, JUCE GPLv3 path -- splash
`JUCE_DISPLAY_SPLASH_SCREEN=0` confirms it; no top-level LICENSE file yet, add
GPLv3):** all three library options are free + legal to ship --
- WORLD: modified-BSD (verified on repo) -- permissive.
- Signalsmith Stretch: MIT (verified) -- permissive; general-purpose (vocal +
  Builder-grid stretch).
- Rubber Band R3: GPL v2-*or-later* (verified in its README) -> compatible with
  the app's GPLv3; ALREADY vendored at `libs/rubberband/` (headers-only, not
  linked); general-purpose. The June `octave-pitch-shift-engine` report only
  benched it for the live-guitar 10ms budget -- N/A to the latency-tolerant editor.

**WORLD proven (Python `pyworld`, on Jeff's dry vocal Clean.wav, dry F0 132 Hz):**
formant-preserving, CLEAN (no warble), accurate BOTH ways -- up3 +3.20 st, down3
-2.80 st, up7 +7.17 st (measured with WORLD's own harvest F0; reliable because the
output is clean). Rendered `WORLD_up3/down3/up7.wav` to Documents/BaySickDAW for
Jeff's A/B. This is the working clean shift PSOLA never delivered.

**Plan direction (pending Jeff's pick after the A/B):** adopt a library engine for
the editor/Align/render path; PSOLA stays ONLY for the live pedal (low latency,
spec A2). A/B in progress: WORLD (done) vs Rubber Band R3 (vendored; needs a link
harness) vs Signalsmith (MIT; needs a build harness) vs -- for reference only --
our chipmunking vocoder. This supersedes the Task 2-8 PSOLA-quality sequence; a
formal plan re-scope + §9 Forks entry follows once Jeff picks the engine.

---

## 2026-07-13 — RE-SCOPE LOCKED + engine A/B complete + QA-OctavePedal batch added

**Full engine A/B (measured, on Jeff's dry vocal, all clean + both directions):**
- WORLD (BSD): +3.20 / -2.80 / +7.17 st; CPU 3.6x realtime; ~0.2-0.3 MB; vocal-only.
- Signalsmith (MIT): +3.18 / -2.76 / +7.53 st; CPU 69x; 0.15 MB; general-purpose.
- Rubber Band R3 (GPL v2+): +3.53 / -2.61 / +7.37 st; CPU 16.6x; 0.65 MB; general.
  `R3LiveShifter` (real-time) = ~48-58 ms latency, clean at +0.5/+1 corrections.
Jeff's ear: WORLD best, Rubber Band closest to WORLD. Installed pyworld/pyrubberband-
free harnesses (`Tools/signalsmith-test/`, `Tools/rubberband-test/`) for the A/B —
throwaway, stripped in QA-Fe Task 7.

**Decisions locked (B1-B10, in the plan file):** editor/Align 3-engine dropdown,
Rubber Band default, quality+CPU labels; real-time vocal correction -> Rubber Band
`R3LiveShifter`, dry-monitor default; monitor-button right-click -> Dry/With-Effect
popup (default Dry); PSOLA + `PitchShifters.h::GranularShifter` retired; throat via
engine formant params; bake-on-edit; repo GPLv3 LICENSE; octave pedal + Builder-
stretch pick routed OUT.

**Boundary correction:** the 3 PSOLA consumers are editor / Align / `PitchCorrectorDSP`
(vocal real-time correction). The octave/instrument pedal is `OctaveStyleDSP`
(granular + period-doubler + YIN + POG voicing) — NEVER PSOLA. The plan-file "live
pedal = PSOLA" label had conflated the vocal corrector with the instrument pedals.

**QA-OctavePedal (NEW, Phase 3 after QA-Fe):** the octave pedal "rings like a broken
bell" — its engine WAS built per the June octave research (verified in the code) but
doesn't deliver clean low-latency octave-down. Batch = fix that gap + rework the
pedal-mode UI (overlapping knobs; rack view fine) + low-latency live instrument
monitoring. Slot = Phase 3 (DSP/effects follow-up to QA-EffectsReview).

**Docs written this session (pending Jeff's commit sign-off):** `prancy-crunching-
bear.md` full rewrite; Main Plan §5 (QA-Fe re-scope + QA-OctavePedal) / §6 arrow /
§9 fifty-eighth Forks / QA-Updater dev-watcher task; this running-notes entry;
memory `feedback_surface_full_research_recommendations`.

---

## 2026-07-13 — Task 1 (LIBRARY-ENGINE) — vendor wiring + LICENSE code-complete (pending Jeff build)

**New Task 1** (the re-scoped one, NOT the retired PSOLA-restore Task 1 above).
Vendor wiring only -- no DSP yet.  Files touched:

- **`LICENSE`** (repo root, NEW, B9) -- canonical GPLv3 fetched byte-exact from
  gnu.org/licenses/gpl-3.0.txt via `curl --ssl-no-revoke` (schannel CRL check
  fails on this box; cert chain still validated).  35,149 bytes, 674 lines,
  "Version 3, 29 June 2007".  Hand-transcription avoided deliberately.
- **`THIRD_PARTY_LICENSES.md`** (repo root, NEW) -- table of the four vendored
  pitch engines (WORLD BSD / Rubber Band GPLv2+ / Signalsmith Stretch+Linear
  MIT) + pointer to each bundled `libs/<lib>/LICENSE.txt`.  Scoped to the QA-Fe
  vendoring; a full manifest is a pre-release `/audit-licenses` job.  *(Placement
  flagged to Jeff -- easy to move/rename/fold.)*
- **`CMakeLists.txt`** -- three static-lib/include blocks after the LunaSVG block
  + a link block after the LunaSVG link block + a stale-comment fix on the old
  rubberband headers-only include (Rule 6 edited-region cleanup):
  - **WORLD** -> `BaySickWorld` STATIC, 11 `src/*.cpp` collected manually (the
    NAM pattern; upstream CMakeLists also builds an unconditional `world_tool` +
    examples we don't want).  PUBLIC include `src/` (sources include `world/*.h`).
  - **Rubber Band** -> `BaySickRubberBand` STATIC from the single-file unit
    `single/RubberBandSingle.cpp` (built-in FFT + BQ resampler + NO_THREADING;
    upstream has no CMakeLists, meson-only).  PUBLIC include `libs/rubberband`;
    MSVC `NOMINMAX`/`WIN32_LEAN_AND_MEAN` (sysutils.cpp pulls in Windows.h).
  - **Signalsmith** -> header-only, no target; include dirs
    `signalsmith-stretch/include` + `signalsmith-linear/include` (stretch's
    header `#include`s `signalsmith-linear/stft.h`, which resolves `./fft.h`
    locally).
  - `/MD` runtime is inherited from the global `CMAKE_MSVC_RUNTIME_LIBRARY` CACHE
    FORCE at the top of the file (`reference_msvc_runtime_md_md_match`) -- no
    per-lib patch needed (unlike sfizz).
  - `/W0` on both compiled libs (vendored third-party; not our warnings).
  - Each engine gets a `BAYSICK_HAS_<engine>=1` define for the Task-2 `IPitchShifter`
    `#if` guards; all standalone-only (pitch DSP is not in the VST target).

**What the build proves:** `do_build.bat` builds `--target BaySickDAWStandalone`
(not ALL_BUILD), so linking BaySickWorld + BaySickRubberBand into the standalone
is what forces them to compile.  A clean build => WORLD (11 files) + Rubber Band
(single unit) COMPILE + LINK on MSVC /MD, both configs.  **Signalsmith is
header-only** -- nothing includes it until the Task-2 wrapper, so its MSVC
compile is verified THEN, not here (told Jeff).  `cmake --build` auto-reconfigures
via ZERO_CHECK when CMakeLists changes, so the new targets appear without a manual
configure step.

**No spec calls** in Task 1 (integration mechanics are Rule-8 my call).  One flag:
the `THIRD_PARTY_LICENSES.md` file placement (Jeff can redirect).

**Build checkpoint (no commit) -- pending Jeff's build of both configs + confirm
all four engines wire in.**

---

## 2026-07-13 — Task 1 — BUILD-ENV FINDING: VS toolset update invalidated the CMake cache (not a QA-Fe bug)

**Symptom:** first Task-1 build failed at `project()` with
`CMAKE_C/CXX_COMPILER: .../MSVC/14.50.35717/.../cl.exe is not a full path to an
existing compiler tool` (both the top-level configure and juceaide's nested one).

**Root cause:** Visual Studio (18 2026 / Community) auto-updated its MSVC toolset
from **14.50.35717 -> 14.51.36231**; the old toolset dir was removed.  The
existing `build/` cache still pinned the deleted `14.50` `cl.exe` in two
`CMakeCache.txt` (main + `juce_build/tools/`) + the `CMakeFiles/*/CMakeC*Compiler.cmake`
detection files.  My CMakeLists edit merely *triggered* the ZERO_CHECK
reconfigure that exposed it -- `do_build.bat` only runs `cmake --build`, so the
dead toolset had sat latent since the VS update.  **NOT caused by the vendor
wiring.**

**Why a full rebuild is forced (unavoidable):** every target's MSBuild
`.lastbuildstate` records `VCToolsVersion=14.50.35717`.  On the next build with
14.51 MSBuild sees the toolset mismatch and force-rebuilds that target -- so the
9.8 GB of existing objects are invalidated by the toolset bump regardless.  The
`14.50` string also appears baked inside `.obj` debug records (harmless) and the
`PlatformToolset` in the .vcxproj is the stable label `v145` (resolves to 14.51
now; not the problem).

**Fix applied (surgical, keeps the object tree):** deleted only the stale CMake
metadata -- `build/CMakeCache.txt`, `build/CMakeFiles/`,
`build/juce_build/tools/CMakeCache.txt`, `build/juce_build/tools/CMakeFiles/`.
Objects preserved (159 Release .obj intact).  Because `CMakeCache.txt` is gone,
`cmake --build` alone can't run -- a one-time explicit reconfigure is needed
first, then `do_build.bat`.  After this, the cache is fresh + future builds go
back to `do_build.bat`-only.  *(Candidate reference-memory once the reconfigure
is confirmed working.)*

**Recovery shipped:** `do_configure.bat` (repo root, NEW) -- self-healing
reconfigure: tries a normal reconfigure, and on the stale-toolset failure it
clears the CMake cache metadata + reconfigures fresh, logging to
`configure_log.txt`.  For any future VS-toolset update: run `do_configure.bat`
then `do_build.bat`.  Reconfigure confirmed working (Jeff ran it, cache
regenerated clean, new BaySickWorld/BaySickRubberBand vcxproj present).  Process
misstep this session: I ran the reconfigure once + deleted the cache instead of
version-bumping in place -> memory `feedback_never_run_builds_jeff_runs_all` +
`feedback_jeffs_word_is_the_evidence`.

**Task 1 CLOSE:** Jeff built both configs -- BUILDS CLEAN.  WORLD + Rubber Band
compile + link; Signalsmith include paths wired (header-only, first real compile
lands with the Task-2 wrapper).  All four engines in.  Build checkpoint passed;
on to Task 2.

---

## 2026-07-13 — Task 2 — SPEC PIVOT (Jeff): B7 uniform-bake -> Option 3 HYBRID (live-edit + WORLD bake)

**Library API extraction (3 background agents, complete):** precise call-contracts
mapped for Rubber Band, Signalsmith, WORLD.  Key finding driving the design: Rubber
Band's OFFLINE mode fixes pitch for the whole pass, so time-varying edits (glide /
vibrato / pill curves) go through its REAL-TIME engine block-by-block
(`OptionEngineFiner | OptionFormantPreserved | OptionPitchHighConsistency`,
`setPitchScale` per block); Signalsmith does per-chunk `setTransposeSemitones` over
`outputSeek`/`process`/`flush`; WORLD scales per-frame `f0[i] *= ratio` (time-varying
is native).  Formant/throat: RB `setFormantScale`, Signalsmith
`setFormantSemitones`, WORLD spectral-envelope warp.  (Full specs archived in the
agent transcripts; latency + length-preservation recipes captured.)

**Design surfaced to Jeff -> he reopened B7 himself.**  I first framed sync-vs-async
bake; Jeff pushed the right question -- do real pitch editors let you edit DURING
playback?  They do (Melodyne / Auto-Tune graphical / Flex Pitch / Newtone, at the UX
level), and the ONLY reason we were baking is WORLD (offline, heavy, can't stream).
Rubber Band + Signalsmith CAN stream live during playback of a recorded take.

**DECISION (Jeff, 2026-07-13) -- Option 3, HYBRID (supersedes B7 "uniform bake for
all three"):**
- **Rubber Band + Signalsmith = live edit-during-playback** (Melodyne-style): the
  engine streams under the playhead on the audio thread; edits are heard live.
- **WORLD = background (async) bake** -- **delayed, NOT a hard freeze** (Jeff's
  explicit pick).  Edits made during playback apply a moment later when the
  off-thread bake completes + swaps in (atomic-snapshot, clean crossfade on swap);
  the app never freezes.
- **WORLD-select popup** (BaySickPitch only -- Align applies offline regardless):
  on picking WORLD, show once-until-suppressed:
  > **WORLD works offline**
  > WORLD is the highest-quality pitch engine, but it processes offline: edits you
  > make while audio is playing apply a moment later, not live.  For instant edits
  > during playback, use Rubber Band or Signalsmith.
  > `[ ] Don't show this again`
  Suppress-flag persists to app settings.  **No** dropdown "offline" tag (Jeff
  declined the extra clutter).

**Plan impact:** B7 (uniform bake) SUPERSEDED by the hybrid model.  Task 2 now
carries BOTH a live-streaming audio-thread path (RB/Signalsmith) and the async WORLD
bake + cache + popup, on top of the `IPitchShifter` seam + PSOLA/Granular retirement.
The plan-file B7 line needs a targeted edit (revised wording surfaced to Jeff before
it touches the plan file, per the doc-discipline).  Recorded here per Jeff's "update
the running notes with this and proceed."

**Latency-alignment note (implementation, my call):** the live engines carry real
latency (RB ~50 ms, Signalsmith ~120 ms).  For playback of a RECORDED take that's a
sync problem the old ~13 ms PSOLA didn't have -- solved by the editor owning/reading
the analyzed composite and feeding the engine look-ahead (playhead + engine latency)
so the shifted output lands time-aligned.  Standard "editor owns its audio" approach.

**Process note:** Jeff flagged that a prior competitive pass had me steering toward
the easy read instead of surfacing the harder "do it right" option
(`feedback_surface_full_research_recommendations`).  Owned; no re-research (he
declined).

---

## 2026-07-13 — Task 2 — Live-path mechanism DECIDED: uniform background re-render (true streaming -> Future State)

**Finding surfaced to Jeff:** true audio-thread streaming vs a fast background
re-render give the SAME edit-latency for the region CURRENTLY playing (you can't
change audio already inside the engine's ~50-120 ms latency buffer); editing a
region AHEAD of the playhead is on-time in both.  The ONLY difference is
continuous drag-glide-during-playback (true streaming) vs hear-on-drag-release
(background re-render) -- and background re-render is far simpler + safer (the
heavy library engine never runs on the audio thread).

**DECISION (Jeff, 2026-07-13):** **uniform background re-render for all three
engines.**  Rubber Band + Signalsmith are fast enough (16x / 69x realtime) that
the re-render completes ahead of / on drag-release => feels live; WORLD (3.6x) is
delayed (the popup explains).  No engine touches the audio thread -- all bake work
is on a background worker; the audio thread only reads the published cache
(atomic-snapshot, same liveness contract as the existing edit snapshot).  This
supersedes the earlier "RB/Signalsmith stream on the audio thread" framing.

**True audio-thread streaming (continuous drag-glide) -> Future State CL-300**
(`Batch-surfaced (QA-Fe)`), deferred by Jeff.

**Architecture (rest of Task 2, my implementation call):** BaySickPitchDSP stores
the analyzed composite; the applicator's per-sample target math (region shift +
focus/snap + vibrato + pitch-shape + variation -> pitch/formant/gain envelopes) is
reused OFFLINE by the background baker; `IPitchShifter::bake()` produces the
pitched cache (gain applied after); `processFilePlay` reads the cache at the
source position instead of running PSOLA per block.  Deletes the PSOLA per-sample
path + `mShifters`/`mFormant` + the geometry-period code + `[PITCH DIAG]` from the
editor; retires `PsolaShifter`/`GranularShifter` usage on the editor + Align paths
(the `PsolaShifter` CLASS stays until Task 4 swaps `PitchCorrectorDSP`).

**Chunk 2b (code-complete, pending build):** offline render now uses the bakers.
`applyEditsToBuffer` gained ENVELOPE MODE (outRatio/outFormant/outGain != nullptr
-> write resolved per-sample pitch/formant/gain, skip the PSOLA shift; shifters may
be null) so the SAME target math feeds the bake with no duplication.  `renderOffline`
now: envelope mode -> `makePitchShifter(mEngine)->bake()` (length-preserved) -> apply
gain; old PSOLA lead-in removed (engines self-compensate latency).  New `mEngine`
atomic (default Rubber Band, B1) + inline `setEngine()` (Task 3 wires the param).
`processFilePlay`/`Monitor` still PSOLA (unchanged) -> swaps to the cache in 2c.
Build = compile-check the signature change across call sites + first RB bake path;
optional Render/Freeze ear-check on a pill-edited take.  Files: BaySickPitchDSP.h/.cpp.

**Routed to QA-Cleanup-1 (Jeff-directed 2026-07-13):** the full-rebuild surfaced
two C4702 unreachable-code warnings at `StandaloneEditor.cpp:9666-9667` -- a retired
`ProjectBrowserWindow` block left after an early `return;` in
`doFileSetDefaultTemplate()` (pre-existing, NOT a QA-Fe change; re-surfaced only
because the VS-toolset full rebuild recompiled the whole tree).  Jeff routed BOTH
(a) that dead-code deletion and (b) a full-build compiler-warning cleanup pass (run a
fresh full build at the cleanup batch, capture the complete our-Source warning set,
clean everything safely fixable -- the `juce::Font`->`FontOptions` C4996 migration is
the bulk) to **QA-Cleanup-1** for open-source code cleanliness.  Folded into
QA-Cleanup-1 §5 scope now (not-yet-started batch, Rule 3); the §9 Forks back-ref lands
at QA-Fe close.  Jeff's call: do NOT capture the warning list mid-QA-Fe (incremental
builds miss unchanged files) -- capture it via the fresh full build at QA-Cleanup-1.

---

## 2026-07-13 — Task 2 chunk 2c (EDITOR) — background-cache architecture code-complete

Full rewrite of `BaySickPitchDSP`'s audio-thread + threading path to the uniform
background-re-render model.  **Editor side done; Align (BaySickAlignDSP) is the
remaining Task-2 piece (2d) -- still on the old shifters, so the tree builds now.**

- **Composite storage:** `analyzeComposite` keeps the mono composite in a
  `shared_ptr<const vector<float>>` so the worker reads it race-free.
- **Envelope extraction:** `applyEditsToBuffer` -> `computeEnvelopes` (envelope-ONLY;
  no chans/shifters; snapOn/root/scale PASSED IN so the worker doesn't race live
  members).  Shared `bakeSpan` core (computeEnvelopes -> `IPitchShifter::bake` ->
  gain) used by BOTH the worker and `renderOffline`.
- **Background worker:** `BakeWorker : juce::Thread` started in the ctor, stopped in
  the dtor.  `requestBake()` (message thread; called from `publishEdits` + every
  edit/knob/engine setter) captures a `BakeInput` under a mutex + `notify()`s; the
  worker coalesces via `mBakeDirty`, bakes into a fresh `CacheSnapshot`, publishes it
  lock-free (atomic ptr + 8-deep retire ring).  WORLD's slow bake runs here without
  freezing the UI; RB/Signalsmith finish fast -> feels live.
- **Playback:** `processFilePlay`/`Monitor` now just READ the cache at the source
  position (linear-interp, RT-safe, mono -> all strip channels) -- no PSOLA, no
  per-sample DSP.  Chain OFF or no-cache -> dry passthrough; outside the composite
  span -> left dry.  Monitor delegates to processFilePlay (no separate DSP state).
- **Origin fix:** the bake envelope is composite-relative (`snap.startSample = 0`);
  the cache carries the TIMELINE origin (`BakeInput.timelineStart = mStartSample`) so
  the playback read maps device-rate source position -> composite cache index.
- **Deleted:** the PSOLA per-sample applicator + `mShifters`/`mFormant`/`mMonState`
  twins + `mActive`/`mRetired` edit snapshot + the geometry-period stabilizer +
  ApplicatorState geom fields + the whole `Diag` struct.  **[PITCH DIAG] stripped:**
  editor InfoBar readout (BaySickPitchEditor.h/.cpp) + `PsolaShifter`'s silence-gap
  counters (PitchShifters.h) -- catalog fully cleared for the editor.  (`PsolaShifter`
  CLASS stays: PitchCorrectorDSP still uses it until Task 4.)
- **Behavior notes:** editing is now bake-then-play (RB default feels ~immediate);
  the bake is MONO (matches mono analysis) so a stereo strip collapses to mono when
  corrected; chain-off toggles dry/corrected with a hard switch (crossfade = polish
  if it clicks).
- Files: BaySickPitchDSP.h/.cpp, BaySickPitchEditor.h/.cpp, PitchShifters.h.

**Align swap (completes Task 2):** `BaySickAlignDSP::applyWarp` Phase-2 pitch pass
rewritten -- the PSOLA/Granular/PhaseVocoder branches (streaming + PV-crossfade)
are replaced by ONE path: build a per-sample pitch ratio from the anchor-cursor
semis lerp (guide-time) + Transpose, then `makePitchShifter((PitchEngine)pitchAlgo)
->bake()` length-preserved over the warped result.  `pitchAlgo` is now the engine
index (0=RubberBand/1=Signalsmith/2=WORLD); old-value migration + the dropdown are
Task 3.  Include swapped PitchShifters.h -> LibraryPitchShifters.h.

**Orphaned by the swaps -> Task 7 remnant sweep** (per the plan's "remove all
PSOLA/Granular remnants"; Task 2 removed the USAGE, the class removal is Task 7):
`PitchShifters.h::GranularShifter` + `PitchShifters.h::PvShifter` now have zero
users (editor + Align were their only consumers).  `PsolaShifter` +
`CepstralFormantEngine` STAY (PitchCorrectorDSP + BaySickVocalProcessor use them
until Tasks 4/6).  `OctaveStyleDSP::GranularShifter` is a different class, untouched.

**Task 2 code-complete.** Files: + BaySickAlignDSP.cpp.

---

## 2026-07-13 — Task 3 — engine dropdown + params + labels + migration + WORLD popup

- **`bsp_engine`** (NEW, `addPI("engine",...0,2,0)`) registered + pushed to
  `mPitch.setEngine((PitchEngine) bsp_engine)` alongside the other pitch setters.
  Default 0 = Rubber Band (B1).
- **Migration (old Align -> RB):** `bsp_engine` is net-new, so its ABSENCE in a
  loaded project's APVTS state marks a pre-QA-Fe project (whose `bsa_pitch_algo`
  meant PSOLA/Granular/PV) -> `setStateInformation` forces `bsa_pitch_algo` to 0
  (Rubber Band) after `replaceState`.  New projects (have `bsp_engine`) keep their
  pick.  Clean version detector, no new marker.  (Old default PSOLA(0) already
  maps to RB(0); this catches the explicit Granular(1)/PV(2) old picks.)
- **B2 labels (ASCII, value order 0/1/2):** "Rubber Band - Balanced" /
  "Signalsmith - Lightest (Low CPU)" / "WORLD - Highest Quality (High CPU)" on
  BOTH the Align combo (`bsa_pitch_algo`, relabeled) and the new editor combo.
- **Editor dropdown:** `mEngineCombo` in the BaySickPitch toolbar (row 1, after
  Send), `ComboBoxAttachment` -> `bsp_engine`.  The Toolbar is now a
  `ComboBox::Listener`; `comboBoxChanged` (separate from the attachment's onChange)
  fires the WORLD notice on a user pick.  `mEnginePopupArmed` + the listener being
  added AFTER the attachment's construction sync suppress the ctor-time fire.
- **WORLD popup (BaySickPitch only):** `showWorldOfflineNotice()` -- InfoIcon
  AlertWindow ("WORLD works offline" + the B4 body) with a "Do not show this again"
  toggle persisted to `ui_prefs.xml` (`pitchWorldOfflineNoPrompt`), mirroring the
  existing multi-reset prompt (`openUiPrefs()`).  Align has no popup (it applies
  offline regardless).
- Files: BaySickVocalProcessor.cpp, BaySickAlignEditor.cpp, BaySickPitchEditor.h/.cpp.
- **Task 3 code-complete.**

---

## 2026-07-14 — Task 4 — real-time vocal correction -> Rubber Band R3LiveShifter (code-complete, pending Jeff build)

Swapped `PitchCorrectorDSP`'s per-sample PSOLA path (`std::array<PsolaShifter,2>`
+ `std::array<CepstralFormantEngine,2>`) for one `RubberBand::RubberBandLiveShifter`
(`OptionFormantPreserved | OptionWindowShort`, ~48 ms).  Files:
`Source/DSP/PitchCorrectorDSP.h/.cpp` only.

**Architecture (the block adapter):** the LiveShifter's `getBlockSize()` is FIXED
at construction and unrelated to the JUCE block, so `process()` re-partitions
through a preallocated mono `MonoFifo` pair: mono input accumulates into `mInFifo`;
whole `getBlockSize()` blocks drain through `shift()` into `mOutFifo`; each JUCE
block pops `numSamples` of wet back out.  `mOutFifo` is primed with one block of
zeros at reset (guarantees no underflow for any JUCE block size -- proof: available
`= B + floor(n/B)*B >= n` for all n).  **Total added latency = `getStartDelay()`
+ one block = `getLatencySamples()`** (new public getter).  All state preallocated
in `prepare()`; `shift()`/`setPitchScale`/`setFormantScale`/`setFormantOption` are
RT-safe per the RB docs.

**Correction is MONO** (implementation call): the vocal source is a mono mic
duplicated L=R, so `process()` averages to mono, runs one shifter, and writes the
wet stream to every channel -- half the CPU, no L/R divergence, and matches the
editor's established mono bake.  (The old path ran two independent shifters that
"may have diverged the channels slightly" -- BaySickVocalProcessor.cpp:557 comment;
with mono correction they can't, but that comment is in another file + moot since
the wet recorder sums to mono anyway -- left untouched, not a symbol rename.)

**Formant / throat mapping (Task-4 boundary interpretation):** the LiveShifter's
NATIVE formant handling replaces the retired `CepstralFormantEngine` on the
realtime corrector path -- Formant Preserve toggle -> `setFormantOption(Preserved/
Shifted)`; Throat -> `setFormantScale(2^(semis/12))` (0.0 == auto-preserve),
matching the LibraryPitchShifters throat convention (`>1` raises formants / smaller
throat).  Sign verified against the old `CepstralFormantEngine::processFrame`
(`scale = 2^(-throatSemis/12)` sampling => +semis raises formants).  Rationale for
doing the realtime throat here (not Task 6): it's the corrector's OWN param
(`bsv_pitch_throatShift`), part of this swap; **Task 6 is the EDITOR/offline global
throat** (a NEW `bsp_throat` knob feeding the bake seam) -- a separate throat.
`CepstralFormantEngine` the CLASS stays (still used at
BaySickVocalProcessor.cpp:1165, the Align offline throat) until Task 6 moves that
off it + Task 7 deletes it.

**Engage/disengage = cold-start (per B6):** settled-bypass is a fast path (no
shifter CPU while realtime pitch is off -- the default);
`reference_audio_thread_fast_path_bypass`.  The engine is `reset()` + fed on the
engage edge, so the ~48 ms delay is ACQUIRED at engage (the documented one-time
delay jump, B6 accept-or-flip at the smoke).  The existing ~40 ms equal-power
dry<->wet crossfade is retained to mask the dry-side click; it can't hide the
latency step (that's the B6 artifact).  `mLive->reset()` runs on the audio thread
but only at the toggle edge (rare), not per block.

**Guarding:** the whole RB path is `#if BAYSICK_HAS_RUBBERBAND` (member + include +
usage); `#else` = dry passthrough -- preserves the graceful-degradation the batch
keeps if the vendored lib is absent (before the swap, the corrector had no vendored
dep, so the guard is a net-new requirement made optional).  `PitchCorrectorDSP.cpp`
is standalone-only (CMake line 429, not in the VST target), where the define is
always on.

**Rule 6 comment cleanup (edited regions):** rewrote the class header block +
member comments for the LiveShifter; fixed two now-WRONG comments -- the `mFormantPreserve`/
`mThroatSemis` "DSP no-op for H-5" tags (they're live now) and the engage-crossfade
"input rings stay warm while bypassed (feedSample copy)" comment (removed -- now
cold-start).

**SPEC CALL (latency of the recorded WET take) -- RESOLVED (Jeff, 2026-07-14 =
option (b), realign, fold into Task 4):** the corrector now adds ~48 ms + one block
to the wet path (was ~10 ms PSOLA).  The WET recorder (BaySickVocalProcessor.cpp:553)
captures the corrector output, so an uncompensated take plays back ~48 ms late vs
the transport.  This is a RECORDING concern, NOT a monitoring one -- monitoring is
handled by the Task-5 dry default (dry = zero latency); and it's ONLY the realtime
LiveShifter path (the editor/Align offline bakers pad + trim their own start delay,
so those are already time-aligned).

**Fix folded in:** in the WET tap, an audio-thread arm-edge detector (`mPrevWetRecorder`)
primes `mWetLatencySkip = mPitchCorrector.getLatencySamples()` when the recorder
arms, then drops that many leading samples (pre-roll / primed silence) before
`writeBlock`.  So WET[0] aligns to the record-arm moment, matching the parallel
zero-latency DRY take (both anchor at the same `startBeat`; the clip anchor doesn't
move, so this realigns rather than truncates the performance).  The WET recorder is
armed ONLY when realtime pitch is active (PluginProcessor.cpp:4117), so the skip is
unconditionally `getLatencySamples()` -- no bypass branch needed.  Known minor:
the corrector's last ~48 ms of latency-buffered tail never flushes to the WET file
at stop (release/silence for a normal take; the DRY file keeps the full length but
WET replaces it when rtPitch was on).  Files: BaySickVocalProcessor.h/.cpp.

**Note:** the dry-monitor DEFAULT (B3) is delivered by Task 5 (monitor-button
right-click Dry/With-Effect).  In Task 4 the live monitor is still the wet
(delayed) path; the intermediate never ships (bulk-run, one close commit).

- Files: PitchCorrectorDSP.h, PitchCorrectorDSP.cpp, BaySickVocalProcessor.h,
  BaySickVocalProcessor.cpp.
- **Task 4 code-complete — pending Jeff build (both configs).**

**Post-Task-4 UI tweak (Jeff, 2026-07-14):** the BaySickPitch engine dropdown
(`mEngineCombo`) was sitting on row 1 on top of the analyze-state notice.  Moved
to row 2, right-aligned immediately left of the Snapshot button (off the notice).
File: BaySickPitchEditor.cpp (Toolbar::resized).  Builds clean.

---

## 2026-07-14 — Task 5 — monitor-button 3-way selector (True Dry / Bypass Pitch Corrector / With Effect)

**Design pivot mid-spec (Jeff, 2026-07-14):** I first surfaced this as a BINARY
"what does Dry mean" (bypass-corrector-only vs fully-raw).  Jeff correctly called
that a false either/or -- a right-click *selector* is a choice among options, so
it should be **3 options**, not one-or-the-other.  Corrected design (B4 widened
from 2 -> 3 options, Jeff's call):
- **True Dry** (mode 0) -- bare voice, no corrector + no chain (comp/de-ess/sat/
  limiter/NAM).  Zero latency.
- **Bypass Pitch Corrector** (mode 1, **DEFAULT** -- Jeff) -- voice through the
  chain's character but no pitch correction, so the ~48 ms is gone.
- **With Effect** (mode 2) -- full processed chain incl. the corrector's ~48 ms.
The RECORDED take is corrected in **every** mode (the WET tap sits before the split).
Labels approved as-is.  Right-click the Vox strip's **Listen LED** (headphones) to
pick; the popup shows at the mouse with a tick on the current mode.

**!! LOUD PAPER-TRAIL -- default live-monitor behavior CHANGES !!**
Before QA-Fe, the live vocal monitor passed straight THROUGH the corrector + chain,
so monitoring = the fully-corrected + processed signal (and post-Task-4 that carries
the LiveShifter's ~48 ms).  QA-Fe Task 5 makes the **default = Bypass Pitch
Corrector**: by default you now monitor your live voice through the chain WITHOUT
pitch correction and WITHOUT the ~48 ms.  The old always-processed monitor is still
available per-strip via right-click -> With Effect.  Not a removal (the mode still
exists) but a default change users will hear -- flagged here + carries to the
Master Test Plan / Implemented Work Log at close.  New param defaults to 1 on every
Vox strip incl. loaded projects (lazy-registered, default applies -- no migration).

**Implementation:**
- **Param (Vox-only):** `mixer_vox_<n>_monitorMode` (AudioParameterInt 0..2,
  default 1) registered in `VibeSynthProcessor::addLiveInputParams` gated on the
  `mixer_vox_` prefix (Inst strips have no corrector -> no param, no selector).
- **UI:** `MixerTrackStrip::setApvts` wires `mListenBtn.onRightClick` (Vox only) to
  a 3-item PopupMenu writing the param (mirrors the existing `mArmBtn.onRightClick`
  pattern; captures `&apvts` as a pointer since the APVTS outlives the strip).
- **Forward:** `VoxStripTask::run` reads `_monitorMode` next to `_listen` and calls
  `mVocalEngine->setMonitorMode(mode)` in the live branch (beside the existing
  `setForcePitchBypass(false)`).
- **Audio split (`BaySickVocalProcessor::processBlock`):** the corrector ALWAYS runs
  (so the WET tap captures corrected).  Then, after the WET tap:
  - mode 1 (Bypass) -> copy the pre-corrector raw live (`mMonitorLiveDry`, stashed
    before the corrector) back into the buffer -> chain processes raw-live + takes.
  - mode 0 (True Dry) -> `buffer.clear()` (pull live out of the chain) so the rack/
    NAM process ONLY the prior takes; the raw live is re-added at the very END,
    post-Mix, so it reaches the monitor with zero effect while takes stay processed
    (the split Jeff asked for -- avoids "stuck raw takes").  For True Dry the live is
    also excluded from `mDryScratch` so the global Mix crossfade doesn't double it,
    and the post-add is gated on `!monMuteLive` (armed && !listen).
  - mode 2 (With Effect) -> untouched = exactly the old behavior.
- Files: PluginProcessor.cpp, BaySickVocalProcessor.h/.cpp, VoxStripTask.cpp,
  MixerTrackStrip.cpp.
- **Task 5 code-complete — pending Jeff build (both configs).**

---

## 2026-07-14 — Task 6 — global Throat/character knob (editor / offline bake)

Editor-side global throat (the realtime corrector already got its throat in Task 4
via `setFormantScale`).  This is the BaySickPitch editor knob feeding the offline
bake seam's per-sample formant envelope (B6).  The bake seam already accepted a
`formantScale`; per-region `formantSemis` already flowed through -- this adds the
GLOBAL knob on top.

**Mechanism:** the global throat is a flat semitone offset ADDED to each region's
smoothed `formSemis` right before the bake's `formScale = 2^((formSemis + throat)/12)`
conversion in `bakeSpan` (no per-sample smoothing needed -- it's a constant for the
whole bake; a knob change re-bakes like Focus/Mod/Speed).  Positive raises formants
(smaller/brighter throat), negative lowers them (bigger/darker) -- matches the
LibraryPitchShifters convention + the Task-4 realtime sign.  Pitch is untouched.

**DSP (`BaySickPitchDSP`):** new `mThroat` atomic (semis, default 0) + `setThroat()`
(jlimit +/-12, re-bakes on change, mirrors setSpeedMs); `throat` added to `BakeInput`
+ `requestBake` + both `bakeSpan` call sites (worker `bakeToCache`, `renderOffline`);
`bakeSpan` takes `throat`, folds it into `formScale` AND the `neutral` early-out (so
throat-alone bakes instead of falling through to dry) -- same fix mirrored in
`bakeToCache`'s neutral gate.  Default (0 semis) keeps the "analyzed-but-untouched
plays bit-identical" invariant.

**Param + push (`BaySickVocalProcessor`):** `bsp_throat` Float 0..100 default 50
(neutral), matching the Focus/Mod/Speed knob convention; pushApvtsToDsp maps
`(v - 50) * 12/50` -> -12..+12 semis into `mPitch.setThroat`.

**Editor (`BaySickPitchEditor` Toolbar):** 4th knob `mThroat`/`mThroatAtt` next to
Focus/Mod/Speed (same `knob()` builder, default 50), "Throat" paint label, and the
knobs row widened 3 -> 4 slots.

**Watch-item (layout):** the knobs row grew by one 56 px slot; on the default-width
window there's ample room (engine combo moved to row 2 in the Task-4 tweak freed row
1), but if the BaySickPitch toolbar looks cramped on a narrow window, the knob
width / row-2 engine-combo width is the tweak.  Judge at build.

- Files: BaySickPitchDSP.h/.cpp, BaySickVocalProcessor.cpp, BaySickPitchEditor.cpp.
- **Task 6 code-complete — pending Jeff build (both configs).**

---

## 2026-07-14 — Task 7 — retire + clean

**Orphaned classes deleted** from `Source/DSP/PitchShifters.h`: `PsolaShifter`,
`GranularShifter`, `PvShifter` (grep-confirmed zero users outside the file --
editor + Align dropped them in Task 2, the realtime corrector in Task 4).
`OctaveStyleDSP::GranularShifter` is a DIFFERENT nested class (octave pedal),
untouched.  `CepstralFormantEngine` KEPT -- still used by the Align offline throat
(`BaySickVocalProcessor.cpp:1165` `formantShiftMono`).  Also removed the now-dead
`#include "PhaseVocoder.h"` (was only for `PvShifter`; grep-confirmed no consumer of
PitchShifters.h uses PhaseVocoder) and rewrote the stale file-top trio comment to
describe the remaining CepstralFormantEngine.  File 810 -> 243 lines.

**Note (left as-is):** `BaySickPitchDSP.h` still `#include`s PitchShifters.h purely
to transitively hand `CepstralFormantEngine` to BaySickVocalProcessor.cpp -- a
fragile but working chain; moving the include is out of Task 7's scope (Task 6/7
don't migrate the :1165 Align throat off the cepstral engine).

**Throwaway artifacts removed** (all untracked -- don't appear as git `D`):
- `Tools/rubberband-test/`, `Tools/signalsmith-test/`, `Tools/pitch-sim/` (the A/B +
  sim harnesses; served their purpose).  Legit `Tools/` scripts untouched.
- Repo-root `enable_pitch_diag.txt` (0-byte flag; `[PITCH DIAG]` code stripped in
  Task 2 -> Diagnostic Instrumentation Catalog fully cleared).
- 27 loose A/B render WAVs at the repo root (`PV_* RBLIVE_* RUBBERBAND_* SIGSMITH_*
  SIM_* WORLD_*`).  Enumerated first: the repo root held ONLY these 27 (0 wav left);
  Jeff's source recordings live in a subfolder and were NOT touched.

- Files: PitchShifters.h (+ filesystem deletions above).
- **Task 7 code-complete — pending Jeff build (both configs).  Last code task; next
  is the reconciled G2 boundary smoke (Jeff's hands-on).**

---

## 2026-07-14 — G2 boundary smoke RECONCILED + synced to the plan file

All 7 code tasks build clean.  Per Jeff's standing instruction (memory
`feedback_reconcile_smoke_against_batch_changes`), reviewed the plan's Verification
ladder (Parts 1-6) against the shipped Tasks 4-7 and rewrote the stale steps, then
synced the reconciled ladder into `prancy-crunching-bear.md` `## Verification`
(intro blockquote gained a dated reconciliation note).  Deltas folded in:
- **Part 5 monitor:** 2-way "Dry / With Effect (default Dry)" -> 3-way **True Dry /
  Bypass Pitch Corrector / With Effect** on the **Listen LED** right-click, default
  **Bypass Pitch Corrector**; added the True-Dry split check (live raw, takes still
  processed) + the recorded-take **latency-alignment** check (no ~48 ms drag).
- **Part 4:** engine dropdown now on editor row 2 (right of Snapshot); the **Throat
  knob** (Task 6) is a real control to exercise; edits heard on drag-release (bake;
  WORLD delayed); WORLD-pick offline notice.
- **Part 6:** delay-jump is specifically the **With Effect** boundary (True Dry <->
  Bypass don't engage the corrector); added a mode-switch click listen; clarified
  the Throat here = the REALTIME board knob, not the editor knob.
- **Part 1:** added the old-project engine-migration load check (Task 3).
- Confirmed the realtime BaySickVocals board exposes a `Throat` knob
  (`mThroatShift` -> `bsv_pitch_throatShift`) so Part 6's throat step is executable.
- Jeff running the smoke now; on pass -> the ONE close commit + backfill + batch-close
  draft + delete breakdown doc + G2 boundary CLOSES.

---

## 2026-07-14 — Part-4 smoke FINDINGS (3 bugs) + fixes (blueprint + adversarial-verify workflows)

Jeff's Part-4 hands-on surfaced three bugs.  Fixed via an ultracode orchestration:
a **blueprint workflow** (3 parallel deep-map agents + adversarial plan-verify) ->
implement from the verified blueprint -> a **review workflow** (3 lenses: build /
DSP-RT / NewTone-spec + synthesis) = **GO, zero confirmed defects**.

**#1 "vinyl scratch" on a sideways/TIME pill move (every pill).** Root cause (agent-
confirmed): the QA-Fe Task-2 bake produced a SOURCE-timed pitch cache, and
`processFilePlay` read it at `srcX0 + i*srcRate`; a time edit made `srcRate != 1`
(cubic-Hermite-varying) so the STATIC cache was varispeed-RESAMPLED = pitch-bend +
scrub.  A regression from Task 2's cache model (old PSOLA resynthesized per block).

**#2 WORLD "water/electric/buzz" background, always-on, on BaySickPitch but CLEAN on
BaySickAlign.** Same channel composite both paths (both `onRenderComposite`, same
gaps -- my earlier "continuous take vs composite" claim was WRONG, retracted).  Real
difference: Align runs the composite through a PhaseVocoder (`applyWarp`) BEFORE the
WORLD bake; Pitch fed WORLD the RAW composite.  WORLD's Harvest/D4C resynthesis is
noisy on raw vocal, clean on a PV'd signal.

**Fix #1+#2 (unified) -- BaySickPitch renders like BaySickAlign:** phase-vocoder-warp
the composite to the EDITED timeline, THEN bake pitch, play the result DIRECTLY.
- `BaySickPitchDSP` `bakeSpan` gained a `timeMap` param + a warp-then-bake tail:
  `runWarp = anyTime || engine==World`; time-edit -> `applyWarp` warps source->edited
  + the source-timed pitch/formant/gain envelopes resample onto the edited timeline
  via the SAME `lookupAtGuideSec`; WORLD-no-time-edit -> identity 2-anchor PV pass
  (clean input, matching Align); RB/Signalsmith-no-time-edit -> bake raw (unchanged,
  no PV smear).  File-local `warpMapFromSnapshot` + `lerpClamp`.  Cache flips SOURCE
  -> EDITED domain; `bakeToCache` stores `baked.getNumSamples()`.  `buildTimeMapSnapshot`
  extracted from `publishTimeMap` so the bake + the published map share one build.
  New `hasBakedCache()`.  `processFilePlay` read loop UNCHANGED (edited cache read at
  the align-only position; linear when no align -> #1 gone).
- **Decode (MINIMAL, per the adversarial-verify correction -- the first blueprint's
  stamp rewrite would've reintroduced an align-toggle click):** `AlignBlockEntry.pitchBaked`
  + `pitchActive &= !pitchBaked` + `e.pitchBaked = vp->mPitch.hasBakedCache()`.  Baked
  => `wantWarp` collapses to align-only; `sourcePosAt` auto-reduces to align-only.  No
  stamp/lambda surgery.
- Export (`renderPitchedTake`) unchanged: `renderOffline`->`bakeSpan(timeMap=nullptr)`
  now PV's WORLD (identity) before baking, then `applyWarp` handles timing -> WORLD-
  clean export.
- **!! LOUD PAPER-TRAIL (render model change):** the pitched playback cache changed
  from source-timed (+ srcRate resample) to an EDITED-timeline cache read directly.
  Behavior-visible only as the bug fix (time moves no longer scratch; WORLD clean).
- **Known limitation (documented in-code + routed):** for DETACHED pills (backward-
  jump maps) the single-`lookupAtGuideSec` envelope resample diverges from `applyWarp`'s
  internal monotone segmentation at the splice -> pitch/gain slightly misplaced right
  at a detach cut.  Detached pills are already hard cuts; a segmentation-aware resample
  is a follow-up.

**#3 Snap/Center did not match Newtone (Jeff pasted the authoritative spec).** Center
(= our Focus knob) is micro-tonal: pull to the nearest-semitone CENTER, ALWAYS,
scale-independent.  Snap-to-Scale is a manual-DRAG "wall" (skip out-of-scale lanes,
KEEP cents; no processing on toggle).  Ctrl+A then right-click the grid = force all
out-of-scale to nearest-scale-note center.
- **Center decouple:** `computeEnvelopes` Focus target + the pill preview `drawMidiFor`
  both now `std::round(base)` unconditionally (was `snapScale ? scaleNote : round`).
  `setSnapOn/setRoot/setScaleIdx` dropped their `requestBake()` (bake no longer reads
  them -> no re-bake/move on Snap toggle, satisfying "no auto-correction on toggle").
- **Snap drag-wall:** the PitchMove drag now snaps the note's whole-semitone HOME to a
  valid scale lane (`laneInt - round(r.midi)`), preserving the natural cents (was
  forcing the absolute pitch to an integer, killing cents).
- **Force-to-Scale (Jeff: gesture A + menu B, all+partial):** new `forceSelectionToScale`
  (out-of-scale-only, snaps center to nearest valid scale note, one undo step, Chromatic
  no-op, slices skipped).  Gesture = right-click the EMPTY grid with a live selection +
  non-Chromatic scale (narrowed from the agent's right-click-anywhere so pills keep their
  menu).  Menu item "Force Selected to Scale" in `showPillMenu`.  Keybinds VocalEditors
  row added (`KeyBindings.cpp`); Snap tooltip refined (wall keeps cents).
- **!! LOUD PAPER-TRAIL (behavior change):** old projects with Focus>0 + Snap ON on an
  out-of-key note now pull to the nearest SEMITONE center (the Newtone-correct Center)
  instead of the nearest in-scale note.  That old coupling was the bug; per-note pitch
  re-bakes from live params on load (nothing stored changes).

**Routed (Rule 3):** dead `computeEnvelopes` params (`snapScale/rootPc/scaleIdx`) +
the now-only-wastefully-populated `mSnapOn/mRootPc/mScaleIdx` DSP atomics (the editor
reads the APVTS params directly) -> **QA-Cleanup-1** dead-code sweep (harmless; kept to
bound this change's blast radius).

- Files: BaySickPitchDSP.h/.cpp, PluginProcessor.h/.cpp, BaySickVocalProcessor.cpp,
  BaySickPitchEditor.h/.cpp, KeyBindings.cpp.
- **Code-complete, adversarial-review GO -- pending Jeff build (both configs) + the
  Part-4 re-listen (time move clean, WORLD clean, Center/Snap/Force per Newtone).**

---

## 2026-07-14 — GRAVE THREADING BUG (Jeff-flagged) — scrub preview ran the heavy bake ON THE MESSAGE THREAD during a drag

Jeff (5 days in) asked me to check the threading before anything else.  He was right,
and it's the worst error of the batch.

**Root cause (traced with evidence):** the BaySickPitch scrub preview rendered the
pitch bake SYNCHRONOUSLY on the MESSAGE (UI) thread on every drag scrub:
`PitchCanvas::mouseDrag -> maybeScrub()` (150 ms throttle) `-> startRegionPreview ->
BaySickVocalProcessor::startPitchPreview -> mPitch.renderOffline()` = the full
`bakeSpan` (engine bake + my new `applyWarp` PhaseVocoder pass).  WORLD bakes at
~3.6x realtime, so a multi-second region render exceeds the 150 ms throttle -> the UI
thread is blocked back-to-back for the whole drag = freeze/stutter/glitch, worst on
WORLD.  **My Part-4 `applyWarp` addition made `renderOffline` heavier, worsening a
pre-existing (QA-Fe Task-2) message-thread-scrub problem.**  Owned.

**Fix (Jeff's prescription -- drags update params, heavy work off-thread, hand off via
atomic):** the scrub preview now READS the background-baked cache instead of rendering.
- New `BaySickPitchDSP::copyCacheSpan(startEditedSec, endEditedSec)` -- a lock-free
  `mCacheActive.load` + a plain span copy (no engine bake).  Thread-safe on the
  message thread: the published snapshot is immutable + kept alive by the worker's
  8-deep retire ring (a sub-ms copy vs 8 more full bakes = seconds of margin).
- `startPitchPreview` reads `copyCacheSpan(editedSpan)`; only the dry FALLBACK (no
  cache yet) copies the raw source span.  The heavy bake stays on the `BakeWorker`
  (async), where it already was for playback.  `renderOffline` is now used ONLY by
  the explicit Render/Freeze export (`renderPitchedTake`), never the drag.
- The preview now trails the drag by the (async) bake latency instead of freezing --
  RB/Signalsmith feel near-live, WORLD lags (already the accepted model).

**Adversarial review (single focused agent): GO for build, memory-safe, off-thread,
index math correct** -- caught ONE regression, fixed:
- **Fallback span (fixed):** a TIME-ONLY edit (no pitch edit, Focus=0) bakes as
  neutral (`hasEdits()` excludes `hasTimeEdit()`) -> empty cache -> the fallback must
  play the note's raw SOURCE span, not the moved (edited) position (which would
  audition the wrong material, worst on slice pills).  So `startPitchPreview` now
  takes BOTH spans: cache read = EDITED span; dry fallback = SOURCE span.

**Scope honesty:** this fixes the FREEZE/stutter (threading).  It does NOT fix the
other two: (2) the horizontal time-edit RENDER output is genuinely garbled + drops
~half level (a warp-correctness bug, offline -- measured in Jeff's 4 WORLD renders:
horizontal move/stretch show garble spikes + half RMS, vertical is clean), and (3)
WORLD's water/electric/buzz on any adjustment (DSP artifact; my identity-PV may not be
the real fix -- ratio-jitter is a live alternative hypothesis).  Those are next.

- Files: BaySickPitchDSP.h/.cpp, BaySickVocalProcessor.h/.cpp, BaySickPitchEditor.cpp.
- **Threading fix code-complete, review GO -- pending Jeff build.**

---

## 2026-07-14 — Cache-handoff CROSSFADE + lock-free hazard pointers (Jeff-flagged, second grave-error class)

Jeff, before accepting the threading fix, told me to verify the cache handoff:
"even async, if the audio engine instantly hard-swaps the old buffer for the new
async-baked buffer, that's a waveform discontinuity -> stutter/glitch right after a
pill move.  Use a rapid 20-50 ms crossfade, and confirm the handoff is completely
lock-free."  He was right on both counts.

**Verified state (before fix):** `processFilePlay` (audio thread) did a single
`mCacheActive.load(acquire)` and read the current cache directly -- lock-free, YES,
but a HARD SWAP.  The instant a background re-bake published, the next block read the
new cache -> the waveform stepped -> a click/glitch after editing a pill during
playback.  (Pre-existing from the Task-2 cache model, flagged then as "crossfade =
polish if it clicks"; Jeff called it in.)

**Fix 1 -- ~30 ms lock-free crossfade on the handoff.**  `processFilePlay` detects a
swap (a fresh bake is a new pointer) and crossfades old->new over ~30 ms (equal-gain,
per-output-sample ramp) instead of hard-swapping.  Because the caches are highly
correlated (same take, slightly different edit) a linear fade doesn't dip.  No locks,
no allocation on the audio thread.  `mCacheFadeLen` set in `prepare` (0.030 * sr).

**Fix 2 -- two-slot HAZARD POINTER (the actually-grave bug the crossfade exposed).**
A crossfade spans blocks, so the audio thread now holds the old cache pointer ACROSS
blocks -- which the plain 8-deep retire ring did NOT guarantee (it was sized for a
within-block hold).  Adversarial review (dedicated agent, three rounds) found a real
audio-thread USE-AFTER-FREE on the *common* path, not just the fade:
`mCachePlaying` (the last-adopted cache, dereferenced at the swap check) is also held
across blocks.  Kill sequence: Play (adopts C0) -> **Stop** -> tune 8+ notes while
stopped (8+ bakes free C0 from the retire ring) -> **Play** -> `processFilePlay`
dereferences freed C0 = crash.  A pause-edit-resume is a normal vocal workflow.
- Fix: two atomic hazard pointers (`mCacheHazardPlaying` + `mCacheHazardFading`) name
  the two caches the audio thread holds across blocks; the worker's retire-ring erase
  skips BOTH, so neither can be freed out from under playback -- even across a paused
  fade.  Audio thread writes (release), worker reads (acquire).  Invariant
  `mCacheHazardPlaying == mCachePlaying` maintained at every swap.
- Erase-loop bound: the erase SKIPS the (<=2) pinned caches and keeps trimming the
  rest to 8 (a "stop at the first hazard" version would let the ring grow without
  bound during a stopped tuning session -- memory spike, self-healing but real).
  Bounded at 8, terminates, never frees a pinned cache.

**Adversarial review verdict (3 rounds, same agent): GO** -- "the crossfade +
two-slot-hazard + bounded-skip-erase design is now correct and RT-safe."  RT-safe (no
alloc/locks on the audio thread), no OOB, UAF closed on all paths (pause, empty-cache,
first-call), normal playback unaffected, worker-thread-only erase.

- Files: BaySickPitchDSP.h (fade state + 2 hazard atomics), BaySickPitchDSP.cpp
  (prepare fade len; processFilePlay crossfade + hazard set/clear; bakeToCache
  hazard-skip erase).
- **Crossfade + hazard-pointer code-complete, 3-round review GO -- pending Jeff build.**

---

## 2026-07-15 — Bug #2 ROOT-CAUSE FIX: unified NewTone note model (retire the note/slice split)

Owner call, unambiguous: the pill/slice split I built was never NewTone and was the
direct cause of the horizontal-move garble.  Confirmed diagnostic: garble scales with
move distance = a moved vowel's span vs its stationary orphaned-consonant span leaving a
widening degenerate gap in the edited->source time map (phase-vocoder smear + level
loss).  Same split also made words "look like parts are missing" (pills showed only the
vowel core; consonants were carved off to a bottom lane).

**Owner's spec (NewTone, mirrored exactly):** true silence stays empty (no region);
breaths/consonants that lead directly into a phrase fold into the FOLLOWING note (trailing
into the preceding), so one pill owns consonant+vowel and moves as one unit -> no orphan,
no time-map gap, no garble.  The Cut tool (blade -- already exists) is the escape hatch
for the tradeoff (a tuned syllable would pitch-shift its wrapped consonant).

**Process note (owner correction):** I wrongly went into plan-mode + framed "is this its
own batch" + "build the Cut tool" for work that is obviously the QA-Fe #2 fix and where
Cut already exists.  Retracted, deleted the plan file, did the work.

**What shipped (analysis + editor, NOT the audio thread -- slices were always resolved
offline):**
- Analysis (BaySickPitchDSP.cpp Stages 3-4): every voiced run -> a NOTE (short runs =
  tiny notes; no slice regions).  New Stage 4 folds each energetic-unvoiced span into the
  FOLLOWING note (leading consonant/breath) or PRECEDING note (trailing), within
  kAttachGapFrames (=3, ~35 ms); an isolated breath (silence both sides) + true silence
  stay uncovered.  noteFrames tracks frame spans + region index for the fold.
- Flag repurpose: isSlice "auto slice" -> "unpitched (excluded from correction)", set ONLY
  by Cut (not yet wired -- see open items).  computeEnvelopes volume-only branch unchanged
  (already correct for a Cut-off consonant); buildTimeMapSnapshot needs NO change (no
  separate slice spans -> no gap).
- Editor (BaySickPitchEditor.cpp): removed the bottom "SLICES" lane (kSliceLaneH deleted,
  layout reclaimed in zoomToRegions/drawDisplayBox); unpitched pieces render INLINE at the
  sibling's pitch row (greyed, no waveform); unified the collision domain
  (prevBound/nextBound + nudgeSelectionTime no longer segregate note vs slice, so a Cut
  piece stays in contact with its vowel).
- Naming reconciliation (Slice -> Cut / Unpitched): toolbar button + tooltip, commit label
  "Pitch: Cut", info-bar "Unpitched", sub-editor browser label, KeyBindings VocalEditors
  entry, bsp_mode param comment.  (Other editors' "Slice Tool" left alone.)
- Migration (stateFromValueTree): old projects' UNtouched auto-slices dropped on load
  (clears their old garble; re-analysis re-folds the audio); EDITED slices kept inline so
  no user work is lost.

**Adversarial review (dedicated agent, full source trace): SAFE-TO-BUILD, zero defects.**
Compile surface clean (local NoteFrame struct, foldSpan [&] lambda, remove_if, no dangling
kSliceLaneH); fold can't overlap (kGapFrames 8 > kAttachGapFrames 3 -> a span reaches at
most one note boundary); migration never drops a real note; dormant isSlice sites don't
crash.

**Open (surfaced to owner, NOT guessed):** (1) how Cut marks the consonant piece
"unpitched" so tuning the vowel doesn't shift it -- auto-detect the unvoiced side vs a
manual exclude toggle; (2) what "snap the cut point to the grid" should mean (beat grid vs
the voiced/unvoiced boundary).  Cut currently still splits into two like-kind pieces.

- Files: BaySickPitchDSP.cpp/.h, BaySickPitchEditor.cpp, BaySickPitchSubEditor.cpp,
  KeyBindings.cpp, BaySickVocalProcessor.cpp (param comment).
- **Note-model rework code-complete, review SAFE-TO-BUILD -- pending Jeff build.**

---

## 2026-07-15 (cont.) — owner corrections + Slice behavior + bars/beats grid

**Owner corrections applied:**
- **REVERTED the Slice->Cut/Unpitched rename in full.** "Slice" is the established term
  app-wide (drum/wave editors have a Slice tool); renaming only the vocal one was
  unrequested + inconsistent.  Button/tooltip/commit "Pitch: Slice"/info-bar/sub-editor/
  KeyBindings/bsp_mode comment/all internal comments back to "slice".  The isSlice region
  is a "slice" (made by the Slice tool); flag name unchanged.
- Prior entry's "Naming reconciliation (Slice -> Cut)" line is SUPERSEDED by this revert.

**Slice behavior (owner call #1 -- "both"):**
- Auto-detect: on slice, the more-unvoiced half (voicedFraction < 0.5) is marked
  isSlice=true (excluded from correction) so tuning the vowel doesn't shift the consonant;
  slicing mid-vowel leaves two plain notes.  New DSP query `voicedFraction(startSec,endSec)`
  over the F0 track (BaySickPitchDSP.h).
- Manual override: pill right-click menu item "Include/Exclude from Pitch Correction"
  (id 8) -> new `toggleSliceExcluded(idx)` flips the flag under one undo.

**Bars/beats grid + Slice snap (owner call #2):**
- The pitch editor timeline drew a SECONDS ruler; owner wants FL/NewTone bars/beats (which
  the piano roll already has).  Reworked the ruler + gridlines to bars/beats (major bar
  lines + numbers, minor beat lines, sub-beat when zoomed) via new canvas helpers
  beatForSec/secForBeat/gridBeatStep using the existing TempoMap (project beats) + the
  composite's startSample; 4/4 assumed (app default).  ADDITIVE -- the pills/drags/zoom
  keep the seconds coordinate system; only the ruler/grid render + snap changed.
- Slice snaps the cut point to the visible grid (gridBeatStep resolution); hold ALT to
  slice anywhere -- exactly NewTone's behavior.

**Adversarial review (2 dedicated agents): SAFE-TO-BUILD, zero defects** on both the
note-model rework AND the grid/snap/auto-detect/toggle additions (TempoMap in scope,
loops terminate, no div-by-zero, no dangling `r` before insert, menu id unique).

- Files: BaySickPitchDSP.h (voicedFraction), BaySickPitchEditor.cpp/.h (grid helpers +
  ruler + snap + auto-detect + toggle menu), + the rename reverts across the 6 files above.
- **All of #2-fix + Slice behavior + bars/beats grid code-complete, review SAFE-TO-BUILD
  -- pending Jeff build.**

---

## 2026-07-15 (cont.) — Metronome / time-signature: correction + regression lead

**My error (owned):** I told Jeff time-signature is "decorative / uniform 4 everywhere for
playback."  WRONG.  Time signature IS functional: the metronome accent reads
`currentPattern().tsNum` for BOTH the count-in (PluginProcessor.cpp:2818-2820) and the
transport metro (:2857-2859), accenting every Nth beat.  The song-level TS markers on the
builder ruler feed a pattern's `tsNum` via `autoDerivePatternTimeSig` (PatternManager.cpp:
538-548) or the direct setter `setPatternTimeSig` (:551).  I read ONE narrow comment
(PatternManager.h:261 -- song-level markers "decorative for playback POSITION", i.e. the
beat<->sample timeline math is uniform-4) and wrongly generalized it to "TS does nothing."
Two different aspects of the markers; I conflated them.

**Regression lead (Jeff: "metronome used to play the signature, now it doesn't"):**
- The accent CODE is UNCHANGED -- git: `accentEvery`/`currentPattern().tsNum` has exactly
  ONE commit ever (16488287, Phase D), nothing since.  So the break is UPSTREAM of the
  accent, in how `tsNum` gets onto the pattern being played.
- Prime suspects: (1) `currentPattern()` returns the SELECTED pattern (mCurrentPattern),
  NOT the pattern under the playhead -> in SONG/arrangement playback the accent follows the
  selected pattern's TS (often pattern 0 = 4/4), while single-PATTERN playback would be
  correct; (2) `tsLocked` blocks a later marker change from re-deriving (:542); (3) `tsNum`
  loads default 4 (:1299) for pre-field projects.
- NOT touched by any QA-Fe / this-session work (all vocal-pitch-editor + DSP).  Awaiting
  Jeff's repro (which mode + accents-every-4 vs no-accent) to trace + fix.  Separate from
  QA-Fe.

---

## 2026-07-15 (cont.) — QA-Fe #2: fold-fix did NOT resolve the garble + fold visually broken

Jeff built + tested.  TWO problems remain:
1. **Garble persists** on ANY left/right MOVE or STRETCH (vertical pitch-only clean).  So
   removing the orphaned slice regions did NOT fix it -> the garble is in the time-WARP
   mechanics themselves (buildTimeMapSnapshot / AlignPlaySnapshot / applyWarp / bake
   sequencing), not the slice-gap hypothesis.  Confirmed engine-independent (RB/SS/WORLD).
2. **The consonant FOLD is not working.**  Jeff: the note pills are UNCHANGED in size +
   position, and the consonant/breath sections are now MISSING (silent) -- no slice, and
   not attached to any note.  So Stage 4 `foldSpan` never fires (strong suspect:
   kAttachGapFrames=3 is far too small -- the energetic-unvoiced spans that used to be
   slices sit >3 frames from the note's voiced run, so they fall outside the fold window,
   get dropped, and the audio goes missing).

**Action:** launched a Workflow (7 parallel extraction agents) to assemble ONE review doc
with the VERBATIM code of the entire pipeline (segmentation/fold, region model, editor
drag, time-map, applyWarp, bake sequencing, composite+playback) + per-stage
holes-analysis, for Jeff + Gemini to review.  Doc = PITCH_PIPELINE_BREAKDOWN.md (repo
root, 214 KB, 54 verbatim code blocks).

---

## 2026-07-15 (cont.) — Gemini review + adversarial re-verification (10 claims vs source)

Gemini reviewed the doc; I ran a 7-agent adversarial verification of every Gemini claim
against the real code (each agent tried to REFUTE).  Net verdicts:

**Bug A (garble on time edits) -- REAL, engine is `BaySickAlignDSP::applyWarp` (internal
PhaseVocoder), reached on EVERY time edit (runWarp = anyTime||WORLD); pitch-only bypasses
it (runWarp=false) -> exact match for "pitch-only clean, time-move garbles, all 3 engines".**
Two CONFIRMED feeders + one edge:
- **Claim 8 CONFIRMED (export order inversion):** export (renderPitchedTake) bakes PITCH
  first (renderOffline passes timeMap=nullptr) then post-hoc applyWarp on the ALREADY-
  pitched audio; playback is warp-then-bake.  Engine-independent -> the garbled exported
  WAVs.  Fix: make export warp-then-bake like playback.
- **Claim 6 PARTIAL (right suspect, wrong forensics):** the garble is NOT the constant-hop
  windowScale (that tracks per-window, avg level unity).  It is OVERLAP COLLAPSE at steep
  ratios: rReq clamped [0.25,4]; at the rails Hs->kFFTSize -> Hann^2 OLA overlap->1 ->
  amplitude-modulation/spikes/RMS-drop that scales with warp steepness.  Steep ratios come
  from a rigid note move offloading ALL the stretch onto the small gap segments.
- **Claim 10a CONFIRMED (edge):** dragging the FIRST note to the origin crams leading
  anacrusis into 1 ms (railed decimation) -> spike.  Narrow (first-note-to-origin only).
- **REFUTED as causes:** Claim 5 (dst-nudge tangent-zero smear -- applyWarp SPLITS at
  backward jumps + per-segment monotone-clamps BEFORE the audio read, and it only fires on
  region REORDER, not "any move"); Claim 9 (bakeSpan si=dubSec*sr unit mismatch -- LATENT,
  the only path reaching it (bakeToCache) passes spanStart=0); Claim 7 (no double-warp in
  playback -- CONFIRMED correct, decode gates pitchActive off via `&& !pitchBaked`);
  Claim 10b (copyCacheSpan vs processFilePlay origin -- SAME origin audio[0]=edited t0).

**Bug B (fold not extending / sections missing) -- BOTH my AND Gemini's hypotheses REFUTED:**
- **Claim 1 REFUTED (my kAttachGapFrames/YIN-lag theory was WRONG):** estimateF0Track uses
  a FORWARD-looking window, so runStart LEADS the onset (not lags); the energetic-span
  builder absorbs loud unvoiced onset frames so nextI.f0 - z ~= 0 -> the fold DOES fire for
  a consonant running straight into the vowel.  It only FAILS to fire when a genuinely
  QUIET (below-gate) stretch sits between consonant and vowel (gap > 3 frames) -- e.g. a
  stop consonant's VOT closure.
- **Claim 3 REFUTED (Gemini's "silence" theory was WRONG):** the bake covers the WHOLE
  composite continuously; a non-region (unfolded) consonant bakes NEUTRAL (ratio 1/gain 1)
  and IS present in the cache -> audible, not silent (or fully dry if no edits at all).
- So "pills unchanged + sections missing" is most likely VISUAL (unfolded consonants get no
  pill) rather than audio-silent -- UNLESS on WORLD (unvoiced degrades at ratio 1) or with a
  time edit (gap squash).  NEEDS Jeff's clarification: visual-missing vs audio-silent, which
  engine, with/without a time move.

**No code changed.**  Reporting synthesized findings to Jeff; Bug A fix direction is clear
(export order + warp-quality/steep-ratio); Bug B awaits the visual-vs-audio clarification.

---

## 2026-07-15 (cont.) — Bug A + Bug B FIX implemented (Gemini CLI wired as 2nd reviewer)

Gemini CLI set up as an on-demand second reviewer (installed v0.50.0; OAuth free-tier dead
-> API-key path via ~/.gemini/.env + settings.json selectedType=gemini-api-key; reads the
real repo code directly).  Gemini confirmed my verification (conceded claims 1 + 3) and
supplied the fix plan.  Jeff picked gap-squash design **E** + "do the whole thing."

**Bug A -- the garble (the real cause = the internal applyWarp phase vocoder railing at
steep ratios, fed impossible micro-ratios by a rigid note-move offloading all stretch onto
tiny gaps; NOT engine-specific):**
- **Gap-squash cap (design E), BaySickPitchEditor.cpp:** new `kMaxWarpRatio = 3.0`; new
  `Bound{dst,src}` + `prevBoundEx`/`nextBoundEx` (neighbor dst bound + its source pos);
  `clampElasticDelta` (move) now bounds the delta so neither adjacent GAP's dst/src ratio
  leaves [1/3, 3]; Stretch handlers cap the NOTE's own dst/src ratio the same way.  Detach
  bypasses all caps (free hard-cut for big moves).  Also caps the leading gap -> kills the
  origin-crush edge too.
- **Export reorder, renderOffline + renderPitchedTake:** renderOffline now builds the time
  map and passes it into bakeSpan (WARP-THEN-BAKE, matching playback); renderPitchedTake
  dropped its post-hoc applyWarp (which ran on already-pitched audio = phase-coherence
  destruction = the engine-independent WAV garble).  (highRes warp-oversampling now a
  follow-up: the bake's internal warp runs at os=1 like playback.)

**Bug B -- missing visual handles (audio was never lost; the fold just dropped isolated
spans):**  Stage 4 foldSpan `else` branch now MATERIALIZES an isolated span (plosive
closure / silence-bounded breath) as an inline `isSlice` block at the nearest note's midi
lane (NewTone shows these inline, not a bottom lane).  drawMidiFor returns raw midi for
slices (no Focus drift).  Contiguous consonants still fold into their note (word stays
whole) -- only genuinely silence-separated material becomes a block.

**Adversarial review (dedicated agent, 15 VERIFY items): SAFE-TO-BUILD, zero defects** --
clamp sign-math correct, no jlimit lo>hi/UB, delta=0 always allowed, no div-by-zero,
push_back doesn't invalidate noteFrames, export const/lifetime/signature OK, sort correct.

- Files: BaySickPitchEditor.cpp (cap + bounds + drawMidiFor), BaySickPitchDSP.cpp
  (foldSpan slice + renderOffline timeMap + Stage-4 comment), BaySickVocalProcessor.cpp
  (renderPitchedTake reorder).
- **All three fixes code-complete, review SAFE-TO-BUILD -- pending Jeff build.**

---

## 2026-07-15 (cont.) — Garble DIAGNOSIS breakthrough: it's the pitch-editor MAP, not the engine/bake

Jeff built the three fixes.  Garble STILL present on ANY horizontal move/stretch.  Ran a
surgical bypass probe (temp `anyTime=false` in bakeSpan -> bake does NO time-warp): garble
PERSISTED -> **ruled the bake out entirely.**  Traced it: a pure horizontal move has no
pitch change, so `hasEdits()` (which excludes `hasTimeEdit()`) makes it "neutral" -> NO
cache -> `pitchBaked=false` -> `pitchActive=true` -> the timing is applied by the REALTIME
DECODER (`PluginProcessor.cpp:1147/1211-1216` sourcePosAt -> lookupAtGuideSec file-read),
a path the bypass never touched.  BUT Jeff then confirmed a DIAGONAL edit (time+pitch,
which DOES bake) ALSO garbles -> so BOTH the decode warp AND the bake warp garble.

**Common denominator (the actual root): the pitch-editor's TIME MAP / warp read.**  Two
hard facts:
- **Align's time-warp is CLEAN** (Jeff verified: mute leader, hear the dub warp+correct --
  clean).  Same warp machinery (AlignPlaySnapshot / lookupAtGuideSec).  So the warp ENGINE
  is fine; the PITCH-EDITOR MAP CONSTRUCTION (buildTimeMapSnapshot: rigid note-segments +
  warped gaps, sparse anchors) is what differs from Align's smooth dense map and breaks it.
- **The artifact is "cutting IN AND OUT"** (dropout/gating), NOT smear/misplacement -> the
  reader is hitting stretches that output SILENCE (out-of-range reads / map-coverage gaps),
  not a phase-vocoder smear.

**Next:** Jeff to send renders (garbled simple move + clean reference, RB/SS) for WAV
forensic (cut-in/out period -> block-boundary vs out-of-range).  In parallel: DIFF
buildTimeMapSnapshot (pitch, broken) vs the Align map builder (clean) to find the
structural difference.  NO bandaid until the warp map is fixed.

**Also retracted:** the earlier "engines were tried for the time-STRETCH" claim was
imprecise -- the time-warp is ALWAYS applyWarp; the RB/SS/WORLD engines only do PITCH after.

### DEFERRED (push aside until the garble is fixed -- NO bandaids first)
- **Pitch-editor mouse-control rework (Jeff's spec, to confirm):** plain drag = pitch
  (vertical) + edge-stretch, NO free horizontal move (so dragging up never knocks a note
  out of line); **Ctrl+drag = detach stretch**; **Ctrl+Alt+drag = full horizontal move**.
  Makes the destructive/garbling move deliberate + decouples pitch from accidental move.
  Explicitly NOT a substitute for fixing the warp (stretch still garbles).
- **Bug B slice/block refinement:** inline consonant blocks reinstated (foldSpan else ->
  isSlice at nearest-note lane; kAttachGapFrames 3->2 to break out plosives).  Cosmetic,
  secondary to the garble; tune after.
- **highRes export oversampling:** lost in the renderPitchedTake warp-then-bake reorder
  (bake's internal warp runs os=1); thread highRes -> bakeSpan os as a follow-up.
- **Metronome / time-signature regression:** accent code unchanged since commit D; break is
  upstream (currentPattern=selected not playhead, or tsLocked, or load default).  Awaiting
  Jeff's repro (mode + accents-every-4 vs no-accent).  Separate from QA-Fe.
- **Gap-squash cap (kMaxWarpRatio) + export reorder:** in the tree, unbuilt-clean; the cap
  was built on the (wrong) ratio-rail hypothesis -- keep as a sane limit but it is NOT the
  garble fix; the export reorder is a genuine correctness fix (retain).

---

## 2026-07-16 -- WORLD buzz/water: FULL investigation record + v1 tabling recommendation

Scope: at Jeff's request, the complete WORLD engineering record -- everything
tried, everything discussed, every angle covered -- consolidated from the six
transcripts spanning 2026-07-15 -> 07-16 (e91daf87 / 5b424108 / 60b9de37 ->
f83d414a / d6485a88 -> de34fc20).  This is the permanent record AND the input to
an external Fable review.  The garble/wobble fix (Option A) is the confirmed win
of this arc and is committed (`bfb345ec`); WORLD's separate residual buzz/water
survived every fix and is unresolved.  Prior WORLD context this entry does NOT
repeat (cross-refs): adoption A/B + measured stats at L298-303; offline popup at
L603-623; the ORIGINAL identity-PV "fix" at L889-929 (superseded by Option A);
the raw-vs-PV integration rationale at L895-900.

### What WORLD is, and why it is here
WORLD is the third, "highest quality" vocal pitch engine behind the
`IPitchShifter` seam, next to Rubber Band and Signalsmith.  It is a DIFFERENT
KIND of thing: RB/Signalsmith are time-domain stretchers that PRESERVE the
original waveform's fine structure + phase; WORLD is an analysis->resynthesis
VOCODER -- Harvest (F0) / CheapTrick (spectral envelope) / D4C (aperiodicity)
-> `Synthesis()` (a pitch-synchronous pulse+noise vocoder that REBUILDS PHASE
FROM SCRATCH every frame).  That phase rebuild is why it never wobbles on a
sharp edit map -- and, per the final read, why it is phase-incoherent with the
take and carries an intrinsic vocoder character.  Adopted for best ear-quality
(Jeff's A/B pick), license-clean (modified-BSD, patent-free), lowest memory,
~3.6x realtime.  User-facing: combo item 3 "WORLD - Highest Quality (High CPU)",
offline-only (one-time notice popup), default engine is Rubber Band (index 0).

### The unmasking event (root of this whole arc)
Pre-Option-A, WORLD was force-fed through Align's `applyWarp` phase vocoder as a
cleanup pre-pass EVEN WITH NO TIME EDIT (`runWarp = anyTime || engine==World`),
recorded rationale: "WORLD's Harvest/D4C resynthesis is noisy on raw vocal,
clean on a PV'd signal."  The PV was doing double duty -- warping time AND
masking WORLD's own buzz.  Option A (2026-07-15) pulled `applyWarp` out of the
pitch path so each engine warps natively (`bakeWarped`).  Jeff's call, made with
eyes open -- his words: "WORLD wasn't clean it just didn't have the 3 sounds it
was making ... so it probably was just masking its problem so lets do A."
Result: the mask came off and WORLD's buzz/water came back on every voiced note.

### The symptom (Jeff verbatim + conditions)
- "I waited for ever and WORLD's weird buzz and water sound remain" (survives a
  fully finished bake -- NOT the pre-bake window).
- "still there even if turn it down" + RMS-normalize did nothing -> LEVEL-
  INDEPENDENT (not clipping/heat).
- "wouldn't that display itself in all 3 engines?" -> WORLD-only; RB/Signalsmith
  clean on every edit type, every render, every master recording.
- DECISIVE (clips-channel test): "a render played in a clips channel does make
  the noise I just had to turn it up alot to hear it as its way quieter on a
  clips channel so this isn't just a vocal chain issue" -> the buzz is BAKED INTO
  WORLD's synthesis output, present even with NO vocal chain, merely much
  quieter without the chain's boost.
- MOST DIAGNOSTIC CLUE: "they both only seem to hit when there is a pill at the
  playhead point so the one or two gaps where there is legit nothing it stops for
  half a second" -> gated to VOICED material; the textbook pulse-train signature.

### Every hypothesis floated, with disposition
1. DJ-scratch = decode varispeed (`usePV=false` resample path). -> That was the
   GARBLE, correctly fixed by Option A. Not the buzz.
2. Wobble = Align `applyWarp` PV chasing the sharp per-note move map. -> Correct
   for the WOBBLE, fixed by Option A. Not the buzz.
3. Pre-bake async window (WORLD bakes slow -> long fallback window). -> Explains
   the "rolling/funky first play or two"; RULED OUT for the buzz (10s wait).
4. WORLD runs hot -> clipping upstream (RMS ~1.14-1.3x, crest 7.4 vs RB 6.7).
   -> RULED OUT (fader-down + RMS-normalize both did nothing).
5. Cache-read linear-interp aliasing (2-pt read aliases WORLD's hot HF if
   cache-rate != device-rate). -> Jeff rejected the rate-mismatch premise
   repeatedly; composite builds at device rate; never confirmed. Effectively out.
6. PDC / latency comb (Jeff's hypothesis). -> `CompDelayLine` is a pure integer
   delay, cannot comb/alias. RULED OUT.
7. Global dry/wet Mix comb (`wet = dry + mix*(wet-dry)`). -> Real latent path but
   Mix default 1.0 + Jeff confirmed 100% -> path off. RULED OUT.
8. WORLD non-determinism (random breath differs cache vs render). -> `randn`
   uses FIXED seeds reseeded every `Synthesis` -> cache bit-identical to render.
   RULED OUT.
9. Always-on vocal chain (DeEsser->Comp->Saturation->Limiter) distorting WORLD's
   peaky/HF-rich/phase-rebuilt signal. -> The confident "found it," backed by a
   clean pre-chain diagnostic capture. RETRACTED -- the clips test demolished it.
10. HF tilt (pulse-train excitation brighter top octave -> Saturation grabs it).
    -> HF de-emphasis barely moved it; NOT just the top octave.
11. Intrinsic pulse-train buzz from D4C-underestimated aperiodicity (the research
    agent's rank-1). -> The best-grounded theory; DISPROVEN BY ITS OWN FIX (see
    the 0.50 AP floor below).
12. Our own tanh soft-clip coloring the whole F0-pulse signal -> F0-correlated
    buzz baked into the bare render. -> Gated to peaks-only; no audible change.

### Every fix attempt (file / function / intent / result)
- **Option A -- engine-native `bakeWarped`** (`LibraryPitchShifters.cpp` +
  `bakeSpan` restructure, `applyWarp` out of the pitch path). -> **THE WIN**:
  garble/wobble GONE for RB + Signalsmith across ~18 render WAVs. Committed.
- **`hasTimeEdit()` neutral-gate + `renderOffline` export twin.** Pure time moves
  were "neutral" so bake/export bailed -> exports were bit-identical to Normal
  (peak 0.320 / RMS 0.0465 / F0 170.1). -> Correct; export now matches playback.
- **Retire realtime pitch-decode warp (`pitchActive=false`).** Not-yet-baked
  edit plays the DRY raw take through the bake window. -> "rolling gone after
  ~1s." Confirmed. (Dead pitch-decode branch left for a follow-up cleanup.)
- **WORLD RMS-normalize (`writeNormalized`).** Level-match to input (WORLD ~1.14x
  hot). -> NO audible difference. Killed the level theory. KEPT anyway (WORLD
  shouldn't be louder than the other two).
- **WORLD peak soft-clip (tanh at source peak).** Limiter is peak-driven. ->
  "a little fainter but still noticeable." Peaks were PART of it, not the core.
- **WORLD HF de-emphasis (`deEmphasizeHF`, 11 kHz one-pole).** -> "still there,
  a little fainter" then "barely moved it." NOT just the top octave.
- **Aperiodicity floor >4 kHz (`floorAperiodicity`).** Research rank-1 cure. ->
  buzz + water still there; Jeff added the pill-at-playhead clue here.
- **Tanh gate (peaks-only, not whole-signal).** Stop our own tanh coloring the
  F0 pulses. -> no audible change.
- **Aperiodicity floor WHOLE BAND 0.15@DC -> 0.50@Nyquist ("one last swing").**
  Should inject MASSIVE noise between harmonics. -> **NO AUDIBLE DIFFERENCE.**
  This is THE key negative: 0.50 aperiodicity doing nothing DISPROVES the entire
  pulse-train model the AP floor was built on.

The fixes that made ZERO audible difference (most diagnostic): RMS-normalize,
the peaks-only tanh gate, and -- most damning -- the whole-band 0.50 AP floor.
Only the fixes that were actually about the GARBLE (Option A + the two gates)
fully worked.

### Decisive tests + what they proved
- 18 render WAVs + master recordings, full spectral battery (HF%, flatness,
  roughness, spectral flux, HNR, hiss): WORLD ~= Rubber Band on EVERY metric,
  and playback ~= render. The metrics NEVER caught the buzz Jeff clearly heard.
  "Your ears beat my metrics" became the theme -- the artifact was never once
  measured, only heard.
- Wait-10s: rolling gone, buzz persists -> not the window.
- Fader -6 dB: unchanged -> not clipping.
- Render dropped into a FRESH project (empty strip): clean -> misled toward
  "the specific vocal chain" (wrong -- see clips test).
- Pre-chain diagnostic capture (`DEBUG_liveread`): clean -> ALSO misled toward
  the chain. It read "quieter pre-chain" as "clean"; the buzz was just too quiet
  to notice without the chain's ~+14 dB boost. (Diagnostic removed before commit.)
- CLIPS-CHANNEL TEST (decisive): WORLD render on a bare channel, no vocal chain
  -> STILL buzzes, just very quiet. Proves the buzz is intrinsic to WORLD's
  synthesis, not the chain. Forced the chain retraction.
- Pill-at-playhead: buzz on voiced pills, silence in true gaps -> WORLD
  synthesizing voiced content = pulse-train signature.
- Whole-band 0.50 AP floor -> nothing: disproved the pulse-train model itself.

### Rubber Band / Signalsmith comparison
Both clean all arc, on every edit type, every render, every master recording.
Same playback path / cache read / strip / PDC / mixer as WORLD -> the cause is
WORLD-specific. Physical differences left standing: WORLD rebuilds phase from
scratch (RB/SS preserve it), is ~1.14-1.2x hotter + peakier, and is impulse/
pulse-train excited. Implication: the buzz is intrinsic to what WORLD IS -- a
pulse/noise vocoder -- the exact artifact the WORLD paper exists to mitigate and
why the field moved to neural vocoders. RB + Signalsmith already deliver the
clean garble fix, so WORLD is expendable for v1.

### Where I was wrong (retractions, for the record)
- "The `anyTime=false` bypass is still live" -> grep-confirmed already reverted;
  the screenshot was from when the diagnostic was applied last session.
- "WORLD render == playback, it's just the window" -> retracted after the 10s
  test; "you said 10 seconds, I said one -- my mistake."
- Level/clipping -> retracted twice (fader + RMS both did nothing).
- Sample-rate re-litigation -> Jeff killed it 3x; "dropping it for good."
- Always-on vocal chain -> the BIGGEST retraction; the clips test demolished a
  confident "found it."
- Pulse-train / aperiodicity model -> disproven by its own 0.50 AP-floor fix; no
  replacement diagnosis was found. THIS is the crack Fable is asked to probe.

### Current code state (what is in the tree, uncommitted at time of writing)
Three WORLD-only buzz-fix helpers survive in `LibraryPitchShifters.cpp`, all
load-bearing on EVERY WORLD bake (both `bake` + `bakeWarped`), NONE audibly
effective against the buzz:
- `floorAperiodicity` (:527) -- 0.15@DC -> 0.50@Nyquist band floor.
- `deEmphasizeHF` (:550) -- 11 kHz one-pole LP.
- `writeNormalized` (:559) -- RMS-match (gain cap 4.0) + tanh soft-clip ABOVE the
  source peak only. The RMS match is KEPT for level parity regardless of the buzz.
No dead WORLD code; the only superseded thing (the old whole-signal tanh) lives
as a regression comment, not code.

### Loose threads for the reviewer (never resolved -- what we may be missing)
1. **Why did a 0.50 whole-band AP floor produce ZERO change?** If the floor truly
   reaches `Synthesis`, that much injected noise MUST change the output. Suspects:
   (a) `floorAperiodicity` mutates the SOURCE `ap` rows but `bakeWarped`
   resynthesizes from the REMAPPED `apEd` rows (interpolated at :494-498) -- check
   the floor is applied to the array `Synthesis` actually reads; (b) stale build;
   (c) the buzz genuinely is NOT in the AP/periodic balance -> look at CheapTrick's
   envelope, `writeNormalized`, or `deEmphasizeHF` itself.
2. **D4C low-F0 revision** (`d4c.cpp` ~314-316): subtracts aperiodicity for
   F0 < 100 Hz -> pushes low/male voices MORE periodic -> MORE buzz. Never guarded
   or tested. What F0 does Jeff's test take actually sit at?
3. **StoneMask never wired in** -- pipeline is Harvest -> (no StoneMask) ->
   CheapTrick/D4C. Standard WORLD refines F0 with StoneMask. Never tried.
4. **The metrics never once registered the buzz.** No time-aligned / F0-synchronous
   / perceptual metric was built. Any next pass should START by capturing a buzzy
   playback and finding a metric that actually SEES it -- the whole arc flew blind.
5. **`bakeWarped` frame-remap vs base `bake` never differentially tested** for the
   buzz -- but the buzz appears on UNEDITED WORLD too, pointing at base
   `Synthesis()` character, not the time-remap.
6. **Cache->output 2-pt linear read never runtime-instrumented** at matched rates.
7. Running-notes "ratio-jitter" alt hypothesis (L1006) still untested.

### Recommendation (SPEC CALL -- surfaced, Jeff's to decide)
Table WORLD for v1. Ship Rubber Band + Signalsmith (both clean). Pull the WORLD
combo item, or park it behind an "Experimental" label, so no beginner lands on
the buzz. Default is already Rubber Band, so nobody is forced into WORLD today.
If pulled, that is a user-facing OPTION REMOVAL and gets its own explicit removal
line here at the moment it happens. Alternative: keep chasing per the loose
threads above (starting with #1 -- confirm the AP floor even reaches synthesis).

---

## 2026-07-16 -- Companion: pitch-editor grid + monitor de-click + playhead (shipped alongside)

Recorded for commit completeness (these ship in the same commit as the WORLD
experiments, all on top of `bfb345ec`):
- **Monitor-swap de-click** (`BaySickVocalProcessor`): 10 ms per-sample crossfade
  between the 3 monitor modes (True Dry / Bypass Corrector / With Effect) so
  switching modes no longer clicks. Jeff: "Monitor click is good."
- **Pitch-editor playhead** (`BaySickPitchEditor`): green (`VC::Green`) triangle +
  body playhead, bidirectionally synced to the song/builder playhead
  (`onTransportBeat` get + new `onTransportSeek` set -> `mPlayHead.seekTo`).
- **Ruler seek + range select** (`BaySickPitchEditor`): click ruler to position;
  Ctrl+drag = red (`VC::Highlight`) time-selection, 96-tick snap, two-way synced
  to the builder time selection via new `onSetSongTimeSel`/`onGetSongTimeSel`.
- **H+V scroll bars** on the pitch editor (piano-roll parity).
- **Grid zoom = EXACT piano-roll parity** (`setView`/`setLaneHeight`): H zoom-out
  `max(canvasW/kDefaultPianoRollEmptyPx, contentBars + kPianoRollZoomPadBars)`
  bars, H zoom-in `kMaxZoomInBeatsAcross` (0.5 beat), V 48/12 notes over the FULL
  canvas height, wheel step 1.15, clamp-then-anchor. Now pulls the piano roll's
  own constants (`VibesynthConstants.h`) so it tracks any future baseline change.
  (Earlier attempts dropped the empty-baseline term -> zoomed too far out; fixed.)
- **1-based bar labels** (display only): playlist/builder (`BuilderPage`), normal
  piano roll (`PianoRoll`), drum-kit piano roll (`DrumKitGrid`).

---

## 2026-07-16 -- WORLD buzz: external Fable review findings -> fix arc opened (Jeff's call)

Scope: the independent adversarial review Jeff requested (input = the FULL
investigation record entry above) came back; Jeff reviewed it and ordered a
FIX ARC instead of tabling.  This entry records (1) the review's corrections
to the record, (2) Jeff's calls on the review, (3) the ordered work.  The
implementation record follows in the next entry.

### Review finding 1 -- the 0.50 AP-floor "disproof" is INVALID (record correction)
The record's key negative ("whole-band 0.50 aperiodicity floor -> no audible
change -> pulse-train model disproven") is wrong in its INFERENCE, not its
observation:
- The floor DID reach `Synthesis` in BOTH paths.  `bake()`: floors at
  LibraryPitchShifters.cpp:402, Synthesis reads the same rows.  `bakeWarped()`:
  floors the SOURCE rows (:457) BEFORE the :479-498 remap; the remap is a
  convex interpolation of floored rows, which stays >= the floor.  Loose
  thread #1 suspect (a) is refuted -- the plumbing was fine all along.
- But the intervention was physically WEAK: WORLD SQUARES stored aperiodicity
  (synthesis.cpp:172); periodic power scales by (1-ap^2), noise by ap^2.  A
  0.50 floor = noise at 25% power (-6 dB below the envelope) + harmonics down
  all of 1.25 dB -- AT NYQUIST ONLY.  At the 0.15 low end: noise -16.5 dB,
  harmonics -0.1 dB.
- And it was aimed BACKWARDS: D4C anchors ap at -60 dB @ 0 Hz and ~0 dB @
  Nyquist BY CONSTRUCTION (d4c.cpp:373-379; measured points only at
  3/6/9/12/15 kHz) -- so the 0.50 end landed where ap is already ~1 (no-op),
  the 0.15 end landed where the buzz harmonics live (masked), and
  deEmphasizeHF ate whatever HF noise was added.  ZERO audible change was the
  EXPECTED outcome.  The pulse-train model was never disproven; the experiment
  had no power.

### Review finding 2 -- buzz and water are TWO artifacts with opposite mechanisms
- WATER: D4C's LoveTrain gate (d4c.cpp:386, default threshold 0.85) SKIPS any
  voiced frame whose low-band energy ratio fails the test, leaving that frame
  at ap ~= 1.0 (InitializeAperiodicity, d4c.cpp:327) -> synthesis.cpp:111
  zeroes the periodic part -> the frame renders as PURE NOISE mid-note.
  Scattered noise frames inside voiced notes = the watery/gurgle.  Community-
  validated fix: threshold 0.25 (straycat, the dominant OpenUtau WORLD
  resampler, ships 0.25; the pyworld maintainer docstring documents 0 =
  "voiced frames will be kept voiced").  Honesty note: this fixes WATER, not
  buzz -- it keeps MORE frames periodic.
- BUZZ: minimum-phase envelope + mono-pulse excitation with per-period phase
  reset is a DOCUMENTED intrinsic "buzzy" limitation of this vocoder class
  (High-Quality Vocoding Design, arXiv 2101.10278; the differentiable-WORLD
  neural-vocoder work exists because of it).  D4C MEASURES group delay but
  Synthesis never RESTORES phase dispersion -- that is the physical difference
  from RB/Signalsmith, which preserve the take's natural phase.  Fix lever:
  restore phase dispersion to the excitation -- not aperiodicity/magnitude
  knobs.  This also explains the original unmasking cleanly: a PV pre-pass
  hands WORLD an already-phase-coherent signal, so WORLD's re-render adds far
  less audible delta.

### Review finding 3 -- why the metrics never saw it (loose thread #4 resolved)
HF%, flatness, HNR, flux, roughness are all magnitude-domain, long-window
stats -- and WORLD reproduces the magnitude spectrum nearly perfectly (same
envelope, same harmonics).  The buzz is a PHASE / temporal-fine-structure
artifact: every harmonic fires phase-aligned once per glottal cycle.
Magnitude metrics CANNOT see it -- the whole 18-WAV battery was structurally
blind, not unlucky.  Metric that can see it: band-limited (2-6 kHz) envelope
modulation spectrum -- buzz index = modulation energy at F0/2F0/3F0 over total
20-800 Hz modulation energy; simpler proxy: kurtosis/crest of the 2-6 kHz
band per 50 ms window.

### Loose threads closed by the review
- #1: RESOLVED above (plumbing fine, inference wrong).
- #2 D4C low-F0 revision: irrelevant for this material -- Jeff's takes sit at
  F0 ~132-201 Hz (Clean.wav 132 / SIM_dry 201 / export stat 170.1); above
  100 Hz the d4c.cpp:314-316 revision RAISES aperiodicity (anti-buzz
  direction).
- #3 StoneMask: designed to refine DIO; redundant after Harvest (maintainer
  guidance; vendored Harvest already runs full candidate refinement +
  FixF0Contour + SmoothF0Contour).  Would not have touched the buzz.
- #5 bake-vs-bakeWarped differential: dispatch confirmed (pitch-only channel
  -> bake(); any time edit anywhere -> whole composite via bakeWarped();
  BaySickPitchDSP.cpp:880); buzz heard under both -> base Synthesis character,
  as the record suspected.
- #6 cache 2-pt linear read: at matched rates the read index steps by exactly
  cr/sr = 1.0 (BaySickPitchDSP.cpp:1019); worst case a static gentle HF shelf
  at a fractional offset; incapable of producing an F0-locked artifact; and it
  is the identical path for all three engines.
- #7 ratio-jitter: no mechanism exists -- computeEnvelopes one-pole-smooths
  the semitone target and WORLD samples that smooth envelope once per 5 ms
  frame center.  Nothing jitters.

### Jeff's calls on the review (2026-07-16)
- "The Normal take WAS the copy-synthesis control" -- checked against code:
  the QA-Fe Task 6 neutral gate (BaySickPitchDSP.cpp:461-463 / :859-861)
  returns the DRY composite when nothing is edited, so a Normal take never
  reaches WORLD at all.  The record's own "exports bit-identical to Normal"
  stat is the proof.  Right instinct; the gate silently made it a bypass, not
  a control.  (With the helpers removed below, any unedited note inside an
  edited bake IS the true bare control from now on.)
- Clips-channel test: Jeff rules it irrelevant (not the intended playback
  design).  Noted for the record; the offline experiments in the next entry
  settle the intrinsic-vs-our-code question objectively regardless.
- ORDER: fix arc, not tabling -- (1) update this record, (2) REMOVE the three
  buzz-fix helpers, (3) execute review items #1 + #3-#5 (whisper/AP
  validation, D4C threshold, buzz metric, phase dispersion).  If it still
  sounds wrong after those, WORLD moves to Future State.

### LOUD REMOVAL LINE (internal DSP, Jeff's explicit order this session)
`floorAperiodicity` (0.15->0.50 AP floor), `deEmphasizeHF` (11 kHz one-pole),
and `writeNormalized`'s above-source-peak tanh soft-clip are REMOVED from
`WorldShifter` -- all three shipped in 6bbb8650 as ineffective-against-the-
buzz experiments and colored every WORLD bake for nothing.  The RMS level
match (gain cap 4.0) is KEPT (WORLD Synthesis runs ~1.14x hot vs the take).

---

## 2026-07-16 -- WORLD fix arc: implementation + offline validation (unverified in-app)

### Code shipped (uncommitted; Jeff builds + ear-verifies)
- `LibraryPitchShifters.cpp` / `WorldShifter`:
  - Helpers REMOVED per the loud line above; `writeNormalized` reduced to the
    plain RMS match.
  - `dopt.threshold = 0.25` in BOTH `bake` + `bakeWarped` (LoveTrain water
    fix; matches the straycat/OpenUtau shipping value).
  - NEW `disperseHF()` after `Synthesis()` in both paths: static allpass FIR
    restoring excitation phase dispersion.  Zero phase below 2 kHz, smoothstep
    ramp 2->5 kHz, pseudo-random group delay up to 4 ms (fixed LCG seed
    0x5EEDCAFE -> deterministic; cache == export).  LTI post-pass == in-
    synthesis dispersion for the periodic path; noise floor statistically
    invariant.  10 ms FIR, direct convolution, offline bake-worker cost only.
- NEW `Tools/buzz_metric.py`: the phase-sensitive metric the whole arc lacked
  -- buzz index (F0/2F0/3F0 energy share of the 2-6 kHz envelope modulation
  spectrum) + band kurtosis + band crest, voiced-gated.  numpy-only.

### Offline validation (pyworld 0.3.5 + the QA-Fe battery WAVs, project
### "Untitled Project (100)")
- NEUTRAL-GATE PROOF: `WORLD Normal.wav` is BIT-IDENTICAL to `Rubber Band
  Normal.wav` (302848 samples, maxdiff 0.00e+00) -> Normal takes never reach
  any engine; they are the dry composite.  Settles the "Normal was the
  copy-synthesis control" question objectively.
- LOVETRAIN COUNTS (dry Clean.wav, fs 44100, 1030 voiced frames, median F0
  138.4 Hz): threshold 0.85 forces 22/1030 voiced frames to full noise
  (scattered 5 ms breath bursts mid-note = the water); 0.25 -> 2/1030.
- WHISPER TEST PASSED: ap := 1.0 -> band kurtosis collapses 2.82 -> 0.19
  (Gaussian), crest 4.62 -> 3.57, output is pitchless breath -> the
  aperiodicity array demonstrably reaches Synthesis and controls the output;
  most F0-rate modulation rides the periodic path.  Loose thread #1 fully
  closed (plumbing fine; the 0.50 floor was just weak).
- METRIC SEPARATES THE ENGINES (same edit, "Vertical Move" renders):
  WORLD buzz 0.171 / kurt 2.17  vs  RB 0.077 / 1.35  and  SS 0.080 / 1.69
  -- 2.2x separation where the magnitude battery saw nothing.
- METRIC HONESTY NOTE: the ABSOLUTE buzz index does not equal perceptual buzz
  -- the dry take itself measures 0.307 (natural voice pulses at F0 by
  definition) yet sounds clean.  Valid use = MATCHED comparisons (same
  material, same path, engine/param delta only).
- DISPERSION LADDER (metric): 2 ms / 3-6 kHz = measured no-op (render 0.171 ->
  0.178).  4 ms / 2-5 kHz = render 0.171 -> 0.100 buzz, kurt 2.17 -> 1.13 --
  WORLD's band fine structure lands at/below Rubber Band's.  Shipped constants
  = the 4 ms variant; the constants are the tuning knob if the ear disagrees.
- THRESHOLD SIDE-EFFECT (as predicted): copy-synth buzz index 0.330 (t=0.85)
  -> 0.388 (t=0.25) -- keeping more frames periodic nudges pulsation up; the
  win is the 22 -> 2 noise-frame kill.  Net call belongs to the ear test.

### Ear-ladder WAVs for Jeff (in `Untitled Project (100)/Pitched/`, playable
### now, NO build needed -- all 16-bit PCM)
- `FABLE_copysynth_t085.wav` / `FABLE_copysynth_t025.wav` -- bare WORLD
  copy-synthesis of the dry take (the control that never ran), old vs new
  threshold.  A/B against `Samples/Clean.wav`.
- `FABLE_whisper.wav` -- the ap=1 whisper test (demo that the AP knob works).
- `FABLE_WORLDVerticalMove_dispersed.wav` (2 ms) /
  `FABLE_WORLDVerticalMove_dispersed4ms.wav` (4 ms, = shipped constants) --
  the buzzy app render post-dispersed.  A/B against `WORLD Vertical Move.wav`
  and `Rubber Band Vertical Move.wav`.
- `FABLE_copysynth_t025_dispersed.wav` / `..._dispersed4ms.wav` -- same ladder
  on the bare copy-synth.
Caveat: the existing app renders were baked WITH the old helpers; the offline
prototypes disperse that already-colored signal.  The in-app rebuild (helpers
gone + threshold + dispersion at bake time) is the true test.

### Verify (Jeff, after do_build)
1. Debug exe first (jassert screen), then Release.
2. WORLD engine, same project: re-bake a pitch edit -> listen for (a) water
   gone on sustained notes (threshold), (b) buzz reduced/gone (dispersion),
   (c) no new smear/phasey tinge on note attacks (dispersion cost).
3. Compare against Rubber Band on the same edit.
4. If it still sounds wrong: WORLD moves to Future State per Jeff's call
   (2026-07-16); the constants in `disperseHF` are the one knob to try first
   (kDispMaxMs up = more de-buzz, more smear risk).

### VERIFY RESULT (Jeff, same day): FAILED -- "the 2 sounds are still there
### and now it sounds worse."  Bake-side fixes were aimed at the wrong signal.

---

## 2026-07-16 -- BREAKTHROUGH: the noise is manufactured by the PLAYBACK path
## from WORLD's signal shape (master-recording differential)

Jeff's two corrections that unlocked it: (1) the render path skips the strip
processing and the renders DO NOT have the sound -- so every render-WAV
comparison (including the Fable review's offline validation) measured the
wrong signal; (2) supplied the missing ground truth: `WORLD Stretch Master
Recording.wav` (master-out capture, nothing armed = full playback path,
artifact AUDIBLE) alongside the matching RB/Signalsmith master recordings
(all three recorded 19:23-19:25 on 07-15) and the same-edit renders.
NOTE: this un-retracts the record's biggest retraction -- the "always-on
vocal chain" theory was killed by the clips test, which Jeff has ruled
irrelevant; the chain (or another playback stage) is prime suspect again.

### The subtraction experiment (scratchpad isolate_noise.py, results in-repo
### as FABLE_playback_minus_render_{WORLD,RB,SS}.wav in Pitched/)
Aligned each master recording to its same-edit render (xcorr lag 92 all
three), least-squares gain fit, subtract:
- Path gain identical for all engines: x2.62-2.68 (+8.4-8.6 dB).
- Residual (what playback ADDS) identical in LEVEL for all engines:
  -18.5..-18.7 dB rel, same band split (57-59% in 250-1000 Hz).
- THE FINDING -- residual FINE STRUCTURE (2-6 kHz band):
    WORLD residual: kurtosis 4.62, crest 5.22   <- spikier than even the
                    WORLD render itself (2.88)
    RB residual:    kurtosis 0.28, crest 3.64
    SS residual:    kurtosis 0.37, crest 3.74
  A linear path leaves residual spikiness ~= source spikiness; 4.62 > 2.88
  means a NONLINEAR playback stage EMPHASIZES WORLD's per-period peaks --
  it manufactures F0-locked spiky products it does not produce for RB/SS.
  At -18.7 dB, pulse-concentrated: quiet enough that every global metric
  washed it out, spiky enough that the ear locks on.  This is why nothing
  ever measured it.
- Code-confirmed path delta: `renderPitchedTake` writes bare `renderOffline`
  output (BaySickVocalProcessor.cpp:1402) -- no chain, no gain.  Playback
  runs the cache through the vocal chain rack DeEsser->Comp->Sat->Limiter
  (slots 0-3, :395-398) + strip/master gain staging (+8.5 dB net measured).
- Mechanism candidates (per-stage): Saturation (memoryless waveshaper on a
  kurt-2.9 pulse signal -> per-pulse distortion bursts), Limiter (1 ms
  attack / 2 ms lookahead CAN track individual F0 periods at 145 Hz ->
  F0-rate gain ripple), Compressor (10 ms attack, marginal), DeEsser (band
  gain-mod).  Pinning = the bypass ladder.
- BLOCKER for the ladder: `bsv_{deesser,comp,sat,limiter}_bypass` exist in
  APVTS + are forwarded to rack slots, but NO UI anywhere attaches them
  (grep-confirmed: only the processor touches them).  Ladder needs 4 toggle
  buttons added to the Vocal editor (or equivalent) -- surfaced to Jeff.
- Ear checks available NOW, no build: (1) FABLE_playback_minus_render_WORLD
  vs _RB (the isolated added-noise, normalized ~+19 dB for audibility);
  (2) FABLE_WORLD_render_levelmatched.wav (render x2.6229 = master loudness)
  vs the WORLD master recording -- separates "quiet seed amplified" from
  "path-manufactured" by ear.
- Status of the 07-16 bake-side changes (helpers removed / threshold 0.25 /
  disperseHF): verdict deferred until the stage is pinned; "worse" is
  plausibly the removed tanh crest-cap (it was taming exactly the peaks the
  nonlinear stage grabs -- record: tanh made it "a little fainter", the one
  helper that measurably moved it).  Keep/revert is Jeff's docket call.

### Jeff's ear verdicts on the isolation set (same day) -- MODEL CORRECTED
1. The isolated playback residual does NOT contain the sounds -> the path's
   nonlinear contribution (the kurt-4.62 finding above) is real but is NOT
   the artifact.  Chain-manufactured theory DEAD.
2. The sounds ARE in the level-matched render (render x2.62) AND the master
   -> THE ARTIFACT IS IN THE BAKE ITSELF, sitting ~8-9 dB below unity-render
   audibility.  "Renders don't have the sound" was a LOUDNESS-threshold
   effect, not a path effect.  The original clips-test read ("way quieter,
   not just a vocal chain issue") was right all along.
3. Chain-bypassed master recording (Master 2026-07-16 11-01-03, stage
   bypasses engaged from the vocal editor panels -- correction: working
   bypass toggles DO exist in the UI; the earlier "no UI anywhere" claim was
   wrong) STILL has the sounds.  Chain exonerated as the source for good.
   Measured: bypassed master kurt 1.69 / crest 3.76 vs chain-on 2.11 / 4.44
   -- the chain adds peak emphasis (seasoning) but is not the artifact.
CONSEQUENCE: every listen this arc made at render level was a false
negative; ALL future WORLD listening happens at master loudness.

### Jeff's call: RESTORE last session's DSP state
`git restore Source/DSP/LibraryPitchShifters.cpp` executed -> back to the
committed 6bbb8650 WorldShifter (floor/deemph/tanh in, LoveTrain threshold
0.85 default, no disperseHF).  `Tools/buzz_metric.py` kept (untracked).
Note for later: if the component test below pins "water" on the noise path,
the restored `floorAperiodicity` ADDS noise-path energy and would be actively
feeding the water -- permanent removal becomes the indicated fix.

### Next diagnostic: WORLD component solo at master loudness (files ready,
### no build needed -- Pitched/ folder)
WORLD synthesis output == periodic pulse part + period-gated noise part.
pyworld decomposition of the dry take (threshold 0.85 = restored baseline),
one common gain (x2.62 master loudness), relative balance preserved:
- `FABLE_component_full_masterloud.wav`        (both parts; RMS -18.6 dBFS pre-boost)
- `FABLE_component_periodiconly_masterloud.wav` (pulse train only)
- `FABLE_component_noiseonly_masterloud.wav`    (noise part only; sits -22.2 dB
  under the full mix -- the prime "quiet layer" candidate)
Question for the ear: WHICH file carries the "water", which the "buzz"?
- water == noiseonly -> lever = scale the aperiodic part down (targeted,
  two-line) + kill floorAperiodicity permanently.
- buzz == periodiconly -> lever = periodic-path phase treatment (dispersion
  family, re-tuned BY EAR AT MASTER LOUDNESS offline before any build; last
  round's "worse" was confounded three ways).
- both sounds only in FULL, neither solo objectionable -> interaction case,
  hardest; Future State gate looms per Jeff's 2026-07-16 call.

### Jeff's component verdicts (same day): BUZZ = PERIODIC PATH (confirmed);
### noiseonly innocuous (VERY quiet -- that IS its true in-mix level,
### -22 dB under full); WATER = only in FULL -> an ALTERNATION artifact
Water is not the noise itself: it is the voice FLICKERING between pulse
texture and noise texture at 5 ms frame granularity (per-frame aperiodicity
flicker; the 22 LoveTrain-flipped frames are the extreme case).  In
periodiconly the flickers are just level dips (no water heard); in noiseonly
they are noise beside quiet noise (nothing); in full they are texture swaps
(gurgle).  Both sounds now have testable, single-variable levers.

### Offline fix ladder (files in Pitched/, master loudness, ONE variable per
### file, compare each against FABLE_ladder_baselinefull.wav; no build)
Water candidates (ap-side, threshold 0.85 baseline kept):
- `FABLE_ladder_flipfix.wav`  -- ONLY the 22 LoveTrain-flipped voiced frames
  repaired (ap copied from nearest healthy voiced frame).
- `FABLE_ladder_apsmooth.wav` -- flip repair + 25 ms temporal median of ap
  inside voiced runs (kills frame-rate aperiodicity flicker, preserves
  average breathiness).
Buzz candidates (periodic-path phase dispersion on the full synthesis,
2-5 kHz ramp, one strength per file):
- `FABLE_ladder_disp1ms.wav` / `disp2ms` / `disp4ms`.
Readout: water dies in flipfix -> C++ fix = LoveTrain frame repair (or
threshold) alone; water needs apsmooth -> C++ fix = ap temporal smoothing
pre-Synthesis; buzz dies at disp N ms without smearing the voice -> C++ fix
= disperseHF at that strength.  Winners get combined into ONE variant for a
final confirm listen before any code lands.

### Ladder 1 verdicts (Jeff, same day): both PARTIAL
- Water: NEITHER flipfix nor apsmooth killed it (apsmooth slightly better)
  -> aperiodicity flicker largely exonerated as the water's core.
- Buzz: disp4ms leaves buzz but LESS than before -> dispersion mechanism
  confirmed directionally; remainder is either below the 2 kHz ramp corner
  (harmonics 3-14 still fully phase-locked at F0 ~140 Hz), needs more
  strength, or is the metronomic-pulse component (zero cycle-to-cycle
  variation; natural jitter is why real voices don't buzz).

### Ladder 2 (FABLE_ladder2_* in Pitched/, master loudness, one variable
### per file vs FABLE_ladder_baselinefull.wav)
Water candidates:
- `spsmooth`  -- CheapTrick envelope temporal median (25 ms, log domain)
  inside voiced runs: tests envelope flicker as the water.
- `f0smooth`  -- Harvest F0 temporal median: tests F0-jitter-driven wobble.
- `contnoise` -- periodic part + CONTINUOUS spectrally-shaped noise replacing
  WORLD's per-period gated noise bursts (level-matched to the gated noise):
  tests Jeff's own caveat -- breath re-rendered as F0-pulsed bursts instead
  of continuous noise may BE the water.
- `noiseonly_loud` -- the noise solo +12 dB over true level, so the noise
  TEXTURE is finally characterizable by ear (answers the "too quiet to
  judge" caveat from the component test).
Buzz candidates:
- `dispdeep` -- 4 ms but ramp lowered to 800 Hz-3 kHz (disperses the mid
  harmonics the 2 kHz corner left untouched).
- `disp6ms`  -- same 2-5 kHz band, strength up to 6 ms.
- `jitter`   -- +/-0.4% band-limited (20 Hz walk) F0 jitter, voiced only:
  breaks the metronomic pulse identity (humanization lever).
Ceiling check:
- `combo` -- current best-of (flipfix + apsmooth ap, disp4ms): where WORLD
  lands if we ship everything validated so far.

### Ladder 2 verdicts (Jeff) + the WATER IDENTIFIED
- `noiseonly_loud` DEFINITELY has the water -> the water IS the noise path;
  the earlier "innocuous" solo read was a level issue (true level -22 dB).
- Best water reducer: JITTER (a buzz-lever!) -- because the water is the
  take's continuous noise layer (breath/room/floor) RE-RENDERED by WORLD as
  noise bursts gated once per glottal cycle; jitter makes the burst spacing
  irregular -> rhythmic bubbling reverts toward breath.  RB/SS pass the same
  layer through untouched (steady -> ear ignores it) -- why they never water.
- Best buzz reducer: disp6ms.  Everything still partial; best-of beat combo.
- Jeff's screenshot question (WORLD residual "looks quiet"): peak-
  normalization artifact of the FABLE residual files -- WORLD's residual RMS
  was actually the HIGHEST (0.0161 vs 0.0142/0.0141); its energy sits in rare
  tall spikes (crest 5.2), so peak-norm draws the body thin.  The visual IS
  the spikiness signature.
- Jeff's "squash" theory disposition: literal version refuted (bake is
  RMS-matched; measured path gains near-identical x2.62/2.67/2.68; no
  differential squash-recover).  The core instinct ADOPTED in transformed
  form: the recording's own noise layer sets the water's LEVEL; WORLD makes
  it rhythmic.  Not a take defect -- any real take feeds the mechanism.

### TOOLING BUG FOUND + FIXED: dispersion FIR was NOT flat (v1)
Level audit (triggered by the ladder-3 RMS printout) caught every v1
dispersion render losing ~3.3 dB RMS.  Cause: allpass FIR built at zero
nominal delay -> acausal pre-ring wraps to negative time and gets truncated;
only ~3% of time energy, but spread coherently across ALL bins: -4 dB low
shelf, +2.2 dB bump at 3 kHz (probed).  CONSEQUENCES:
- All v1 offline dispersion ear-rankings carry loudness + tilt bias (disp
  strength verdicts VOID; "dispersion reduces buzz" direction survives via
  the level-invariant modulation metric).
- The reverted in-app disperseHF shared the construction, and writeNormalized
  re-matched RMS AFTER it -> net in-app effect ~= +3-4 dB presence-region
  tilt vs lows = brighter/harsher vocal.  Plausibly a large chunk of Jeff's
  "now it sounds worse" verdict on the 07-16 build.
FIX (v2, scratchpad disperse2.py): add constant group delay D=3 ms to every
bin so the pre-ring is causal, FIR window = D + tau_max + 6 ms ring room
(662 taps @ 44.1k), taper both ends, advance output by D samples after
convolution -> zero net latency.  Verified: worst probe deviation 0.05 dB,
RMS delta 0.00 dB.  ANY future C++ port uses the v2 construction.

### Ladder 3 (FABLE_ladder3_* in Pitched/, level-true, vs ladder_baselinefull)
- `cleantake` -- take denoised ~12 dB (spectral gate) BEFORE analysis, stock
  WORLD after: tests "water level tracks the take's noise floor".
- `apscale` -- D4C ap x0.6 in voiced frames: rendered noise layer -4.4 dB.
- `disp4v2` / `disp6v2` -- CORRECTED flat dispersion, re-rank strength.
- `jitterdisp6` -- jitter + disp6 v2 stack (regenerated flat).
- `kitchen` -- jitter + apscale 0.6 + disp6 v2 on the raw take (denoise
  deliberately excluded; it is attributed separately via cleantake).

### Ladder 3 verdicts (Jeff, partial): INPUT NOISE IS THE DOMINANT CAUSE
- `cleantake` (take denoised ~12 dB pre-analysis): "doesn't sound perfect
  but it sounds a hell of a lot better" -- the noise-layer model confirmed
  as the main mechanism.  "It's gotta be the noise in the file" (Jeff).
- `kitchen` (jitter + apscale + disp6 v2): "considerably worse" -- the
  synthesis-mangling levers are net-negative in combination.  Direction
  locked: CLEAN THE INPUT, don't warp the synthesis.
- Still unreported: disp4v2 / disp6v2 / jitterdisp6 / apscale solo reads,
  and WHAT remains imperfect in cleantake (buzz vs water residue) -- both
  needed before any C++ recipe is drafted.

### True Dry question (Jeff) -- answered from code
Monitor split (BaySickVocalProcessor.cpp:547-564): True Dry (0) routes the
RAW live mic to the monitor and skips the rack; Bypass Corrector (1) and
With Effect (2) route the monitor THROUGH the rack (deess->comp->sat->lim),
whose compressor + saturation add ~14 dB small-signal gain -> room noise
between/under phrases is boosted hard in modes 1/2.  True Dry's cleanliness
is the ABSENCE of makeup gain, not noise removal.  No denoiser exists
anywhere in the app today.

### Ladder 3 final verdicts + Jeff's build order (2026-07-16)
- The 4 remaining ladder-3 files (disp4v2/disp6v2/apscale/jitterdisp6) all
  still have issues -> synthesis-side levers CLOSED.  Jeff: "I completely
  now think this is due to the takes being amplified" -- input-noise cause
  locked.
- ORDERED: (1) Noise Cleanup feature, Option A NON-DESTRUCTIVE (record raw,
  per-take Keep/Remove), strengths Off/Light/Strong; (2) GATE as a 5th
  vocal-chain panel, FIRST in the chain; (3) then wrap the G2 boundary.
- Scope mapping done (3-agent workflow, file:line evidence): playback
  streams clips per-block via AudioClipStreamer (NOT the composite) -> the
  cleaned result must exist as a FILE; exactly 2 real audio open sites
  (PluginProcessor.cpp:3389 players, :3530 composite) + thumbnail :1562;
  per-clip property rides ArrangementBlock + PatternManager save/load
  (routeChannel pattern); clip UI surface = Builder clip Properties dialog
  (BuilderPage.cpp:3443) / right-click menu (:3188); project subfolder
  convention = inline two-liner (Pitched/Aligned precedent); vocal rack has
  6 fixed slots (4 used, 2 spare) so Gate-at-slot-0 = re-slot types 0..4 at
  prepareToPlay + shift index literals (:328-335,:395-398,:403/421/446/456)
  + kNumChainSlots 4->5 + <VocalChainState> s0..s3 blob MIGRATION (index-
  keyed, would mis-restore without a shift); EXISTING NoiseGateStyleDSP
  (EffectType 104, panel + DynamicsLAF) is a reuse candidate vs a new
  channel-strip GateDSP -- surfaced as a spec call.  Plan surfaced in chat;
  awaiting Jeff's approval + sub-call picks.

### DESIGN PIVOT (Jeff, 2026-07-16): lazy-cache model DROPPED -> record-time
### 4-take model (LOUD REMOVAL: per-clip Off/Light/Strong toggle, Cleaned/
### cache folder, and the reader-swap resolver are all OUT of the plan)
Jeff's calls: 1=B (new vocal channel gate, not the pedal reuse), 2=B (WORLD
back to full stock, helpers out, RMS match stays), 3=B (live auto-learn IN,
and moved EARLIER: starts at interface-track assignment when monitoring
begins, not at arm).  Pre-v1 rule declared permanent: NO backward-compat /
save-migration work until v1 (all projects are Jeff's own) -> saved to
memory; the Gate s0..s3 blob-migration item is deleted.
Jeff's technical catch that killed the lazy model's fuzzy edges: a WET take
(through the realtime corrector) has a TRANSFORMED noise floor -- a profile
learned from the raw mic cannot exactly describe the wet file's noise.
Design answer: DUAL live learners from track-assignment on -- one on the
raw-input tap, one on the post-corrector (pre-chain) tap -- so each cleaned
variant uses its matching-domain profile; wet cleaning is inherently
slightly less exact (noise under notes shifts with the voice); per-file
self-learn stays as fallback for imported takes.
Jeff's replacement flow (his spec, verbatim-faithful):
- Track assigned to strip -> auto-learn starts (live feed exists pre-arm).
- ARM -> popup (native-Windows style, not custom-LAF) asking which of the
  4 takes (Dry / Dry Cleaned / Wet / Wet Cleaned) lands on the Builder
  grid, with a notice that unselected types are driven by Options > File
  Settings; picking an already-checked type keeps defaults; picking an
  unchecked type adds it (semantics to confirm -- Q2 below).  All written
  takes land in the Builder browser; the chosen one lands on the grid.
- Options > File Settings (currently a dead menu name) -> the 4 take-type
  checkboxes, >=1 always enforced, + cleanup strength default.
- Browser restructure: Vox/Inst recordings group -> one ENTRY per recording
  (base name minus the variant tag) -> child rows showing only Dry / Dry
  Cleaned / Wet / Wet Cleaned.  Right-click variant = existing per-file
  actions (+ move/copy-to-page gains a join-that-page's-group option).
  Right-click group name = RENAME: disk-renames all variants (base swapped
  through the timestamp, variant tags kept) -> must rewrite every
  ArrangementBlock.audioFilePath + library entry referencing them.
Open questions posed to Jeff in chat (popup nativeness/4-choice limit,
arm-popup repeat behavior, defaults-union vs defaults-update, strength
regeneration + profile persistence in project file, imported-take cleanup
path, group/page semantics).  NO code until answered.

### Jeff's answers (2026-07-16, all six locked)
1. Popup = stock-looking plain dialog (system style, radio buttons, no
   custom LAF).  2. Picking an unchecked type writes it THAT ONCE only --
   File Settings defaults never silently change ("I definitely want A").
3. Right-click "Regenerate cleaned (Light/Strong)" on cleaned takes, and
   the learned profiles ARE STORED IN THE PROJECT FILE (both domains, per
   recording).  4. NO cleanup path for imported/pre-existing takes -- new
   projects will be used for testing.  5. Arm popup once per strip per
   session; re-ask after interface-track reassignment (reassignment also
   restarts the learners).  6. Browser design (screenshot provided):
   Clips/Vox/Inst main groups stay; under Vox + Inst, one collapsible
   RECORDING-GROUP entry per recording (name up to the type suffix),
   children showing only Dry / Dry Cleaned / Wet / Wet Cleaned; renaming
   the group entry disk-renames every take in it keeping type tags;
   individual-entry edits affect only that entry; the file Properties
   popup gains a destination-group option when moving/copying to Vox or
   Inst; right-click on the Clips/Vox/Inst headers -> "create group" for
   manual after-the-fact grouping; Clips gets manual groups too (never
   auto-grouped -- uploads, not recordings).

### NEW ASK (Jeff, same message): de-reverb vocal-chain panel, RX-style
Difficulty assessment delivered in chat: honest tiering (RX-grade = ML,
out of reach; achievable = late-tail spectral suppression sharing the
denoiser's STFT skeleton, Lebart-style delayed-decay estimate + per-bin
gain; reduces room wash convincingly at moderate settings, will not
de-room a bathroom).  Would be the 6th slot -- exactly fills the rack
(Gate -> De-Reverb -> DeEsser -> Comp -> Sat -> Limiter, de-reverb before
compression so the comp can't pump the tail back up).  One-FFT-frame
latency, PDC precedent = spectral DeEsser.  Slotting (this batch / next
batch / Future State) = Jeff's call, pending.

## 2026-07-16 -- QA-Fe2 Task 1 -- De-noise core: BUILT + CLEAN (Jeff confirmed)

Batch plan: `Batch Plans/gentle-scrubbing-otter.md` (bulk-run mode: no
per-task ear tests; all verify consolidated in Task 5).
- NEW `Source/DSP/DenoiseDSP.h/.cpp`: DenoiseProfile (base64 project-
  persistable, FFT 1024 / 513 bins), DenoiseLearner (wait-free ring push on
  the audio thread, FFT on a 15 Hz pump, asymmetric min-follow floor with
  +12 dB voice gate + digital-silence skip), Denoise::cleanFile (offline
  STFT 1024/256, Light 1.25x/-6 dB floor, Strong 2.0x/-12 dB floor =
  ear-validated cleantake constants, 3-bin freq + one-pole time gain
  smoothing, output length/rate/bit-depth identical, stereo keeps image
  via shared mono gain field), Denoise::learnFromFile fallback.
- `BaySickVocalProcessor`: dual learners (raw tap at the mMonitorLiveDry
  stash, corrector-domain tap at the WET-recorder point, BOTH monitoring-
  time not just capture), prepare in prepareToPlay, mono-fold scratch (no
  audio-thread alloc), setDenoiseLearnersEnabled/reset/getDenoiseProfiles.
- `VibeSynthProcessor`: mDenoiseProfiles store keyed by recording base name
  + <DenoiseProfiles> serialize (after PatternManager node) / deserialize
  (cleared unconditionally -- no cross-project room leaks).
- `StandaloneEditor`: 5 Hz DenoisePollTimer (arm edges + inputChannelIdx
  changes -> learner enable/reset + popup re-arm), showArmTakePopup (stock
  LookAndFeel_V4, AlertWindow combo, Dry-pair-only when corrector bypassed,
  preselect = legacy rule), File Settings dialog (case 502, 4 checkboxes
  >=1 enforced + strength, ui_prefs.xml keys fsWrite*/fsDenoiseStrength),
  commitRecordingResult Vox branch rewritten (union set, cleaned
  generation w/ self-learn fallback, pick->grid, rest->library, unselected
  sources DELETED, clean-failure falls back to the uncleaned source),
  regenerateDenoise (stored-profile re-clean, error dialog on held files).
- `BuilderPage`: "Regenerate De-noise > Light/Strong" on * CLEANED tree
  leaves via new BrowserPanel::onRegenerateDenoise.
- CMakeLists: DenoiseDSP.cpp added.
- Defaults picked (Jeff veto-able, surfaced in chat): File Settings starts
  Dry+Wet checked (today's behavior), strength default Strong.
- OPEN SUB-CALL (asked in chat): Inst strips left dry-only as today -- no
  Inst Dry Cleaned generation yet (no live learner on sfizz engines;
  file-self-learn add is cheap if wanted).
- KNOWN LIMITATION: regenerating a cleaned take currently on the grid can
  fail on Windows (file held by playback); error dialog guides removal.
- Jeff: "Clean" (build confirmed, 2026-07-16).

## 2026-07-16 -- QA-Fe2 Task 2 -- Browser groups + resize: BUILT + CLEAN (Jeff)

- `PatternManager`: AudioLibraryEntry.groupName (persisted per Entry as
  "group"), manual-group registry mManualAudioGroups (persisted as
  <AudioGroups>, cleared at both reset sites), APIs get/setAudioLibraryGroup
  + add/rename/getManualAudioGroups + replaceAudioPath (library + every
  arrangement block; returns hit count).
- `BuilderPage`: new AudioGroupItem tree node (collapsible, accent-tinted,
  right-click menu); AudioCategoryItem gained onContextMenu ("Create
  Group..." on Clips/Vox/Inst headers; Clips manual-only); rebuildAudioRows
  now buckets manual-group -> auto recording-group (take-tag suffix match,
  CLEANED before plain -- order load-bearing) -> flat, with leaf labels
  inside auto groups showing only Dry/Dry Cleaned/Wet/Wet Cleaned; refresh
  hash extended with groupName + registry so group edits rebuild; Properties
  move/copy to Vox/Inst ends with the "Add to Group?" prompt (page's auto
  bases + category manual groups, "(none)" default); browser right edge =
  BrowserEdgeGrip drag (default=min=180, max=540, session-local).
- `StandaloneEditor`: enumerate fills groupName; renameRecordingGroup =
  pre-flight target collision check -> repoint refs (replaceAudioPath) ->
  player rebuild (releases old readers) -> disk moves w/ full rollback ->
  profile re-key (renameDenoiseProfiles) -> rebuild + markDirty.
- MID-TASK ADD (Jeff's order): Regenerate De-noise + recording-group Rename
  are STOP-GATED -- greyed "(stop playback)" menu items + hard guards via
  DSPBase::isTransportPlaying() (the Align-editor stop-gate precedent).
  Manual-group renames stay live (label-only).  Honest residual: a stopped
  overwrite of an on-grid take may still hit a Windows file hold depending
  on share flags; rollback+dialog covers it; Task 5 verification decides if
  the release-retry dance is needed (Jeff's call then).
- COMMIT MODE LOCKED (Jeff): ONE combined commit at batch close (Task 6).
- Jeff: "build is clean" (2026-07-16).

## 2026-07-16 -- QA-Fe2 Task 3 -- Gate + De-reverb: BUILT + CLEAN (Jeff)

- NEW `Source/DSP/GateDSP.h/.cpp`: channel gate (Threshold/Range/Attack/
  Hold/Release), stereo-linked peak detector, 3 dB Schmitt hysteresis, hold
  stage, per-sample gain smoothing, GR atomic; defaults transparent
  (threshold -80).
- NEW `Source/DSP/DeReverbDSP.h/.cpp`: Lebart-family late-tail suppressor,
  STFT 2048/512 identity OLA (Sibilance skeleton, single config), per-bin
  delayed-decay tail estimate (3-frame direct-sound skip, T60 decay from
  Tail), Wiener gain w/ -14 dB floor, 3-bin freq + asymmetric time
  smoothing, Mix = spectral lerp to unity, latency 2048 reported for PDC
  (spectral De-esser precedent), shared gain field keeps stereo image.
- EffectType Gate=119 / DeReverb=120 (stable values), createEffect cases,
  SlotComponent display names; DELIBERATELY absent from the general FX-rack
  picker (vocal-chain-only per scope; two-line add later if wanted).
  EffectPresetIO skipped (locked slots never hit preset save).
- Vocal chain re-slotted 6/6: Gate -> De-reverb -> De-esser -> Comp -> Sat
  -> Limiter; createLayout bsv_gate_*/bsv_dereverb_* params; bypass +
  stage pushes reindexed 0..5; <VocalChainState> blob loops 4->6 (NO
  migration, pre-v1 rule); stale slot comments fixed incl. EffectRack.h
  DeEsser entry; kNumChainSlots 6; mode-map slotIdx 3/4.
- MID-TASK FIXES (Jeff's calls after eyeballing the 6-panel view):
  (1) both new panels restyled to EXACT De-esser scaffolding --
  DynamicsLAF::paintLA2APanel cream/wood plate + right-edge dbfs/output-
  knob + layoutKnobsH row (my originals skipped the plate paint + used a
  divergent layout); (2) text GR readouts replaced with the REAL GRMeter
  widget (Compressor-family asset), 30 Hz setGainReduction feed, left-strip
  placement per the FET panel idiom.
- Viewport fallback for short windows NOT added (6 rows clip below ~410 px
  page height; acceptable risk, revisit if Task 5 shows clipping).
- CMakeLists: GateDSP.cpp + DeReverbDSP.cpp.
- Jeff: "Builds clean" (2026-07-16).
- SIDE THREAD (same day): De-reverb Future State research completed via two
  workflow sweeps (classical WPE w/ NTT patent gate to ~2029; pretrained
  model licensing incl. openMHA AGPL dead-end + UVR weights-license gap +
  Roformer relicense/in-house-train path).  JEFF UPDATED Future State.md
  HIMSELF with these findings -- batch close must NOT double-draft them.

## 2026-07-16 -- QA-Fe2 Task 4 -- WORLD to stock: BUILT + CLEAN (Jeff)

- `LibraryPitchShifters.cpp` WorldShifter: floorAperiodicity + deEmphasizeHF
  + peak-tanh DELETED from both bake paths (removal-record comment left at
  writeNormalized so the helpers don't get re-invented); writeNormalized =
  plain RMS match (cap 4.0) only; D4C threshold stays stock 0.85.  This is
  byte-for-byte the ear-validated "cleantake" configuration -- stock WORLD
  fed by De-noised takes.  Leftover grep: clean (1 hit = the record comment).
- Jeff: "Clean" (2026-07-16).  Task 5 (consolidated verification) handed
  over in chat with the reconciled numbered script (stale plan steps purged:
  popup is a combo not radios; File Settings defaults leave cleaned boxes
  OFF so the endgame test requires enabling Dry Cleaned; stop-gates added
  mid-batch are in the script; commits deferred to close per Jeff).

## 2026-07-16 -- QA-Fe2 Task 5 verification, finding #1: Gate GR meter scale

Jeff, first look at the 6-panel chain: the shared GRMeter is a compressor
meter -- scale bottoms at -20 (gate legitimately sits at -60 closed ->
needle pinned) and its red zone is at the DEEP end (backwards for a gate,
where closed is the normal resting state).  Fix per Jeff's call: NEW
`GateGRMeter` in SharedUI.h/.cpp -- identical chassis (chrome bezel / cream
plate / 120-degree needle / brass cap / LCD), scale 0..-80 (marks every
20), red zone at the OPEN end (-6..0), LCD reads "GATE dB"; GatePanel
switched to it.  De-reverb stays on stock GRMeter (its -14 dB max cut fits
0..-20 correctly).  Fixed in-batch per Rule 3.

## 2026-07-16 -- QA-Fe2 Task 5 verification, finding #2 + WORLD verdict

- FINDING #2 (Jeff): arm popup had OK but no Cancel (mis-click = forced
  channel change to re-ask) -> Jeff's redesign call: POPUP REMOVED ENTIRELY.
  Replacement: "Builder Grid Default" section in the Vox arm-LED right-click
  picker (MixerPage::showInputChannelPicker, item IDs 300..303) -- Dry /
  Dry Cleaned / Wet / Wet Cleaned, tick = locked pick.  Semantics: -1 = auto
  (Wet when realtime correction on, else Dry -- the legacy rule); a user
  pick LOCKS until the project closes (reset in
  restoreAudioStripsFromArrangement load path; deliberately SURVIVES track
  reassignment).  New MixerPage hooks onGet/onSetGridDefault wired from
  StandaloneEditor (state stays beside commitRecordingResult).
  showArmTakePopup + mVoxArmPopupShown/mVoxArmLast DELETED (grep-clean).
  LOUD REMOVAL: the arm popup (S5) is gone; S5's once-per-session +
  reassignment-re-ask semantics are superseded by the lock-until-close menu.
- WORLD VERDICT (Jeff, Task 5 endgame): "doesn't sound perfect but we're
  gonna call it good enough cause it's been a week."  WORLD SHIPS in v1 as
  the third engine, fed by De-noised takes, stock synthesis.  The buzz/water
  arc closes as: root cause = take noise re-rendered F0-gated; fix =
  De-noise at the source; residual character accepted.

## 2026-07-16 -- QA-Fe2 close-out routing (Jeff's calls on the to-address list)

- Items 1-5 -> IN THIS BATCH, now (Jeff): (1) pitch-editor mouse-control
  rework (plain drag = pitch + edge-stretch, NO free horizontal; Ctrl+drag =
  detach stretch; Ctrl+Alt+drag = full horizontal move -- Jeff's deferred
  spec, now confirmed by the in-batch order); (2) highRes export
  oversampling follow-up; (3) dead realtime pitch-decode branch cleanup;
  (4) detached-pill segmentation-aware envelope resample; (5) Bug B
  consonant-slice tune (kAttachGapFrames 3->2).
- Item 6 (metronome/time-signature regression) -> ROUTED TO G3 (my
  placement per Jeff's delegation, group plan reviewed: G3 = Builder/UX/
  engine polish where QA-J' residual fixes live; G4 = mechanical sweeps/
  data layer, wrong shape).  Gated on Jeff's repro.  Forks entry at close.
- Item 7 (Inst cleaned takes): VERIFIED STRUCTURALLY IMPOSSIBLE -- cleaned
  generation + File Settings union live only in commitRecordingResult's Vox
  branch; Inst branch = dry dropWavAsClip only; Regenerate menu keys off
  "* CLEANED" names Inst never makes; Grid Default picker section is
  Vox-only.  No code change.  The stopped-overwrite file-hold residual dies
  as theoretical (untested by Jeff, never observed).

## 2026-07-16 -- QA-Fe2 close-out items 2-5 executed

- ITEM 5: ALREADY SHIPPED (stale deferred note) -- kAttachGapFrames is
  already 2 with the calibration comment and the foldSpan else-branch
  isSlice materialization is in the tree (BaySickPitchDSP.cpp:45, :344-364).
  No change.
- ITEM 3 (dead realtime pitch-decode branch): REMOVED -- pitchActive kill
  switch + pitchOrigin block + the dead sourcePosAt pitch stage
  (PluginProcessor.cpp ~:1144-1221), the AlignBlockEntry pitchMap/
  pitchChainOn/pitchBaked fields + their writers (:1852-1854), the orphaned
  isPitchEditChainOn accessor + mBspOnRaw cached pointer
  (BaySickVocalProcessor).  loadTimeMapSnapshot STAYS (live Align-render
  caller at BaySickVocalProcessor.cpp:1264).  Stale "4 locked slots" +
  chain-order header comments fixed while in the file.
- ITEM 4 (detach-cut envelope resample): bakeSpan now detects detach ramps
  in the time map (guide window <= 2 ms with a backward or > 50 ms forward
  dub jump -- impossible under the 3:1 warp rails) and SNAPS samples inside
  a ramp to the nearest side of the cut.  Kills the Hermite sweep that baked
  unrelated notes' pitch/formant/gain at the cut AND hands the shifters a
  clean instantaneous step for their own detach detection.  Hermite
  endpoint easing in neighboring segments remains (mild, monotone) --
  accepted.
- ITEM 2 (Jeff: 2a): !! LOUD OPTION REMOVAL -- the Pitch render dialog's
  "High Resolution (slower)" button is RETIRED (BaySickPitchEditor::
  runRender now Render/Cancel); renderPitchedTake's highRes param deleted.
  It had been a NO-OP since QA-Fe Option A removed the applyWarp PV from
  the pitch path (the oversampling had nothing left to oversample).
  BaySickAlign's High Resolution render is REAL (oversampled PV warp, os
  2-8) and is untouched.
- ITEM 1 (Jeff: option d): GESTURE MAP SHIPPED -- body plain drag =
  vertical pitch ONLY; Ctrl = fine pitch (0.01 st); Ctrl+Alt = elastic move
  (neighbor-walled, the old plain-drag move); Ctrl+Shift = DETACH move
  (hard cut, no walls, >3 px); edges unchanged (drag = stretch, Ctrl =
  detach stretch).  Shift-toggle-select now gated to Shift-without-Ctrl so
  Ctrl+Shift can't deselect mid-gesture.  Edit-button tooltip updated.
  !! LOUD BEHAVIOR CHANGE: plain drags can no longer move a note in time --
  timing moves are deliberate modifier gestures (Jeff's spec, deferred from
  the garble arc, confirmed + refined to option d today).
- KEYBINDS MENU (Jeff's order): Vocal Editor section rewritten to the new
  gesture map (+ Ctrl+Alt / Ctrl+Shift rows); Builder section gained the
  QA-Fe2 gestures -- Create Group / Rename Group / Regenerate De-noise /
  browser-edge resize / Vox arm-LED right-click (input + Builder Grid
  Default).

## 2026-07-16 -- QA-Fe2 Task 5 COMPLETE + final build CLEAN (Jeff) -> Task 6

Final build (Grid Default menu, dead-branch removal, detach-cut resample,
High-Res retirement, gesture map, keybinds menu): Jeff "clean".  Task 5
verification verdicts across the session: WORLD ships ("good enough"),
6-panel chain approved after the two panel fixes + GateGRMeter, popup
replaced by the Grid Default menu, groups/resize/regenerate exercised
during the finding rounds.  Plan registration completed (Main Plan section 5
entry + section 6 arrow/footnote + section 9 fifty-ninth Forks; group-plan
G2 composition drift fixed [Fd/Fe were missing too] + G3 metronome item).
Task 6 open: /review-batch over the full batch diff -> fix findings -> ONE
commit (message + FULL git status surfaced for Jeff's approval) -> G2
boundary wrap.  NOTE: the Implemented Work Log batch-close entry is HELD
per bulk-run R2 (applies at the Master Test Plan section-B backfill), so
the close writes NO Work Log entry now -- my plan file's Task 6 line said
"/draft-doc batch-close" but the bulk-run convention governs.

## 2026-07-16 -- QA-Fe2 Task 6: /review-batch findings fixed + REAL PDC wired
## (Jeff: option c, "FIX IT NOW" -- the S11 "w/ PDC" clause I wrote was never
## actually true; owning that)

/review-batch verdict: zero blockers; five NEEDS-FIX mechanical items ALL
fixed (stale arm-popup string in File Settings + 2 comments -> Builder Grid
Default wording; stale "4 SlotComponents" header comment; kDenoiseMaxVox
16 -> 6 [the real kMaxVoxStrips]; renameManualAudioGroup member rewrite now
category-filtered via owner-channel ranges [cross-category same-name bug];
/ui_prefs.xml gitignored].  Review NITs recorded: forward-detach jumps
< 50 ms keep the old envelope sweep (bounded); collapsed browser groups
re-expand on tree rebuild; stereo clean-path per-hop alloc (offline).

PDC EXTENSION (the real fix for the De-reverb/spectral-De-esser latency):
- `BaySickVocalProcessor::getChainLatencySamples()` (bypass-aware rack
  total, message thread).
- `VibeGraph::onGetVoxStripChainLatency` hook (wired in VibeSynthProcessor
  before the initial updateBusLatencies call).
- `updateBusLatencies` rewritten: compensation target = max(Layers, Bass,
  Drums, maxVoxChain); the 3 synth buses keep their bus-level comp delays;
  SOURCE-FED inserts on buses without bus-level comp (Audio 400-449, Vox
  600-605 minus own chain latency, Inst 700-719, Rusty 800-812) get
  INSERT-level compensation via the already-live InsertNode::compDelay
  (:1290) -- value-only, no new audio-path code.  Layer/Bass/Drum inserts
  skipped (bus comp covers them; insert-level would double-delay); Aux
  skipped (receives post-compensation sends).  setDelay calls now skip
  no-ops (setDelay reallocs+clears; the poll must not glitch a live mix).
- Refresh: the 5 Hz StandaloneEditor poll watches per-strip chain latency
  and re-runs setLatencySamples(updateBusLatencies()) on change (bypass
  toggles, engine switches, project load).  <= 200 ms re-align lag at a
  live toggle -- inherent to toggling latent FX mid-play.
- HONEST BOUNDS (disclosed): (1) live MONITITORING through an active
  De-reverb still hears ~46 ms -- physics; bypass slot 1 or True Dry while
  tracking; (2) the metronome renders post-mix and sits EARLY by the
  compensation amount whenever any path carries latency -- PRE-EXISTING
  model class (was equally true for latent bus racks before this batch);
  (3) De-reverb remains ACTIVE by default (Reduction 50 / Mix 100) -- the
  timing is now compensated, the sound default ships as approved in Task 5.

## 2026-07-16 -- PDC full-graph audit + Jeff's fix-everything directive +
## SESSION HANDOFF (context ceiling)

- Jeff asked whether the 3-bus PDC model was the whole story -> full audit
  commissioned (2 agents, file:line evidence), saved durable at
  `Plans & Specs/Research Reports/pdc-coverage-audit-2026-07-16.md`.
- Headline findings: only 4 bus rack+postEq pairs + the 6 Vox engine chains
  are measured; ~130 per-insert mixer racks invisible; FX bus + 7 sibling
  bus nodes unmeasured AND uncompensable (no delay lines); preEq omitted
  from all 4 measured sums; METRONOME leads the compensated mix by total
  PDC (default-on now that De-reverb ships active); master recordings land
  late vs beat grid; SC keys skew when latency exists; NAMIR OS latency
  written-but-unread.
- CORRECTION (Jeff, forceful): the pitch-corrector ~48 ms MONITOR latency
  is NOT "by design / none needed" as this session labeled it -- the
  dry-monitor default was a mitigation, not a fix.  A real corrected-
  monitoring fix (e.g. dedicated low-latency monitor shifter, WET keeps R3)
  is IN SCOPE; options = Jeff's spec call.
- JEFF'S CALL: fix EVERY audit gap IN-BATCH before the QA-Fe2 close.
  Handed to a FRESH session (this one is at context ceiling) via a handoff
  prompt referencing the saved audit; the new session also runs the close
  (re-review, ONE commit w/ approval gate, G2 wrap).  Batch remains OPEN;
  tree remains fully uncommitted vs 6bbb8650.

### Jeff's final calls (2026-07-16): De-reverb IN this batch; naming; resize
- De-reverb ships in this batch, titled "De-reverb"; the cleanup feature is
  titled "De-noise" -- uniform with De-esser.  (Take-type file tags stay
  "DRY CLEANED"/"WET CLEANED" as Jeff spec'd verbatim; flagged at approval
  in case he wants "De-noised" tags instead.)
- Vocal chain final order (6/6 rack slots, exactly full): Gate -> De-reverb
  -> De-esser -> Compressor -> Saturation -> Limiter.
- Builder browser panel right edge becomes drag-resizable: default = min =
  current width, max = 3x current width (not persisted across sessions
  unless Jeff asks).
- Full consolidated task breakdown presented in chat for approval; plan
  file to be written on Jeff's go.

### Jeff feature idea (2026-07-16, workshop): armed-mic noise cleanup
Learn the room's noise profile from the armed-but-not-recording feed;
user chooses keep/remove ("noise clean up option"); explicitly NOT just a
gate; separately, a Gate panel for the vocal chain is wanted regardless.
Assessment recorded in chat: standard profile-based spectral denoise
(= exactly the cleantake prototype), auto-learn-while-armed is sound UX for
the beginner audience; the non-destructive record-raw + apply-at-bake shape
would land at the same pipeline point where it fixes WORLD (denoise before
analysis) -- one feature, two wins.  Scope/slot/shape = Jeff's spec calls;
options surfaced in chat, NOT decided.

## 2026-07-16 -- QA-Fe2 PDC full-graph pass, work item A: BUILT + CLEAN (Jeff)
## + spec picks

WORK ITEM A = all 7 gaps from
`Plans & Specs/Research Reports/pdc-coverage-audit-2026-07-16.md` (Jeff's
fix-everything directive, prior entry).  Built; Jeff confirmed clean.  Tree
stays fully uncommitted per bulk-run ONE-commit-at-close.

- `updateBusLatencies` (VibeGraph.cpp) rewritten as a TWO-STAGE minimal-
  latency solve.  Stage 1: every live insert strip aligns WITHIN its actual
  main-out bus (reads `_sendTo`, natural-parent fallback; own path latency
  = engine chain via the Vox/Inst hooks + preEq + rack + postEq) via the
  existing per-insert delay.  Stage 2: each bus (natural = A(bus) +
  busChain) aligns cross-path to target T via bus delay lines.  Replaces
  the single-max model; ~130 previously-invisible insert racks now
  measured; preEq now in every sum (was omitted from all 4 measured buses).
- FX bus (EffectsBusNode) + the 7 InstrChannelNode buses (Clips/Vox/Inst/
  Vox2/Inst2/Inst3/Rusty) gained CompDelayLine members, applied post-pan/
  pre-meter -- previously unmeasured AND uncompensable.  CompDelayLine
  kMaxSamples 8192 -> 32768 (vocal chain + HQ-Linear strip EQ can pass
  8192; alloc is per-active-line so idle lines cost nothing).
- Aux strips align to EACH OTHER (auxStage); FX input reference = maxA +
  auxStage, so the most latent source's send return lands exactly on the
  aligned mix.  Net CPU slightly DOWN at the shipped default: ~83
  per-insert full-compensation delay lines (Audio/Inst/Rusty) collapse
  into single bus-level lines.
- NAMIR oversampling latency now CONSUMED: BaySickVocalProcessor::
  getChainLatencySamples adds its owned NAM/IR stage; NEW
  `VibeGraph::onGetInstStripEngineLatency` hook wired in VibeSynthProcessor
  prepareToPlay -> EngineChainProcessor::getChainLatencySamples (new
  method, sums stages under the spinlock).
- Metronome (applyPostMixRecordAndMetro, PluginProcessor.cpp): ALL clicks
  defer by totalLatencySamples (audio-thread atomic read).  Count-in
  defers as ONE unit via new MetroDSP::countInDelaySamp armed at the
  rising edge (beat spacing through the transport handoff stays exact).
  Transport click grid derives from the DELAYED clock (TempoMap spans
  built from smp0 - pdc; TempoMap extrapolates segment 0 linearly below
  sample 0; negative-beat clicks suppressed).  !! LOUD BEHAVIOR CHANGE:
  play-press mid-beat no longer fires an immediate catch-up click (new
  MetroDSP::transportWasPlaying edge seeds lastBeatFloor = ceil(beat0)-1)
  -- first click lands on the next real crossing; the old immediate click
  also fired the PREVIOUS beat's accent (mislabeled).
- Master recorder: AudioFileRecorder::startRecording gained
  skipInitialSamples (default 0); writeBlock consumes the countdown before
  writing so the 5 ms fade-in ramps the first WRITTEN samples; master
  fallback capture passes totalLatencySamples -> master WAVs land on the
  beat grid.  Strip DRY recorders (pre-chain tap) + Vox WET recorder
  (pre-chain tap + existing corrector skip) unchanged.
- 5 Hz poll (StandaloneEditor::pollDenoiseState): vox-chain-only latency
  watch replaced by a FULL updateBusLatencies re-solve every tick
  (no-op-guarded setDelay); host report refreshed on total change;
  mVoxChainLatLast member deleted, new mPdcTotalLast.  Learner-enable
  logic untouched.
- HONEST RESIDUALS (per-NODE delays cannot express per-EDGE timing;
  recorded in the updateBusLatencies header comment): cross-bus sends
  into one aux mix at A(b)-relative offsets; bus->bus / bus->aux sends +
  re-cabled aux main-outs arrive a stage off; direct strip->FX send early
  by auxStage when an aux carries latent FX.  Exact per-edge PDC = future
  work.

SPEC CALLS RESOLVED (Jeff, this chat):
- Docket 1 (sidechain key timing) = option (b): pre-delay key taps (keys
  at natural time via per-node stash) + per-receive delay lines set to
  max(0, consumerPos - sourceNaturalPos).  Residual ACCEPTED: a source
  naturally later than its consumer (e.g. vocal chain keys a drum gate)
  stays late -- full per-edge graph PDC is the only complete fix.
  Grounding surfaced: (b) matches modern pro-DAW behavior (REAPER/
  Cubase/modern Pro Tools compensate SC as routed inputs).
- Docket 2 (corrected live monitoring, the ~48 ms R3 LiveShifter) =
  option (a): NEW dedicated time-domain monitor shifter (dual-tap
  crossfade class, ~10-15 ms effective) feeds the live monitor/mix path;
  the WET recording keeps R3 quality.  Grounding: the industry pattern --
  every shipping realtime tracking tuner is time-domain few-ms class;
  spectral engines are the quality path.  Assumption surfaced, not vetoed:
  the live monitor path stays OUT of the PDC target (delaying the backing
  to match the cue would worsen tracking); wet FILE alignment already
  handled at write.
- CL-301 -> Future State.md (Jeff's routing call): bus-node consolidation
  into InstrChannelNode; ALREADY APPLIED under Mixer / Routing, new
  "Batch-surfaced (QA-Fe2 2026-07-16)" sub-cluster -- batch close must
  NOT double-draft it.  Origin: Jeff asked whether the L/B/D-vs-generic
  processBus split is normal DAW construction; answer: historical
  accretion, three on-record divergence incidents, industry norm is one
  generic channel type.

Work item B (the two docket picks) builds next; its own checkpoint
follows its build-clean.

## 2026-07-17 -- QA-Fe2 PDC work item B (SC delay-match + monitor shifter):
## BUILT + CLEAN (Jeff)

Work item B = the two docket picks (1b + 2a) from the work-item-A entry.
Built late in the 2026-07-16 session; Jeff's build-clean landed
2026-07-17.  Tree stays fully uncommitted -- ONE commit at close.

DOCKET 1b SHIPPED -- sidechain key delay-match:
- Pre-compensation key taps: every node type (LayersBusNode/BassBusNode/
  DrumsBusNode via one shared-text edit, EffectsBusNode, InstrChannelNode
  in processBus, InsertNode) gained an scTap stereo stash + scTapArmed
  atomic; the copy runs right BEFORE that node's compDelay so alignment
  delays never leak into keys.  Master needs no stash (no comp delay).
  Arming = new VibeGraph::armScSourceTaps() called at the tail of
  rebuildRoutingFromApvts (audio thread, block rate, relaxed stores,
  allocation-free): clears all flags then arms every RoutingGraph scEdge
  source.
- Per-receive key alignment: new fixed array VibeGraph::mScRecvDelays
  [kMaxStripChannels=1000][kMaxScRecvSlots=4] of CompDelayLine --
  deliberately NOT inside the mScRecv map (that map is audio-thread-
  owned; the message-thread solver must never race it).
  updateBusLatencies now solves want = max(0, consumerPos(dst) -
  srcNatural(src)) per SC receive param (_sc_recv{N}_from read straight
  from APVTS), with srcNatural/consumerPos lambdas over the two-stage
  solve results (new ownByCh/engineByCh bookkeeping in the strip sweep).
  Applied in SidechainPullHelper via new VibeGraph::applyScRecvDelay
  (audio thread, allocation-free, n-sample view).
- Pull helper reads the pre-delay tap via new
  VibeGraph::getScSourceTap(chId); falls back to the post-everything
  output buffer for tap-less sources.
- Engine SC unified: all FOUR setSidechainBuffers sites (EngineInsertTask,
  VoxStripTask live branch, InstStripTask, CompositeAudioInsertTask) now
  run the pull FIRST and hand the engine the SAME filled + delay-matched
  receive buffers via getScRecvArray -- previously three of the four
  pushed raw post-compensation predecessor pointers BEFORE the receive
  buffers were even filled.  EngineInsertTask/InstStripTask pulls hoisted
  above the engine render (were after).
- CompDelayLine struct MOVED from VibeGraph.cpp file scope into
  VibeGraph.h as a private nested struct (mScRecvDelays needs the
  complete type); node structs unchanged consumers.
- Residuals (accepted at the docket): source naturally later than its
  consumer (vocal chain keys a drum gate) stays late -- per-edge graph
  PDC territory; engine-internal SC readers sit deeper than the chain
  input (consumer position approximated at chain input; engine-stage
  term dominates); cross-order +1-block policy unchanged.

DOCKET 2a SHIPPED -- low-latency corrected monitor:
- NEW `Source/DSP/MonitorPitchShifter.h` (header-only,
  PolyphaseOversampler precedent -- no CMake change): dual-tap delay-line
  crossfade shifter, 24 ms window, half-sine tap gains (zero exactly at
  wrap -- no splice clicks), ~12 ms effective latency (gain-weighted mean
  tap delay = W/2), linear-interp taps, power-of-2 ring with the write
  index itself wrapped (& mask, incl. two's-complement negative-index
  wrap) per the wrap-the-accumulator rule.  Mono, allocation-free after
  prepare.
- BaySickVocalProcessor: new TD stage between the WET tap and the monitor
  split.  While realtime correction is live (bsv_pitch_realtime_bypass
  off), the With-Effect monitor/mix target swaps the R3 stream (~48 ms)
  for the TD shift of the RAW stash driven by the corrector's published
  applied shift (getCurrentShiftCents -> ratio; includes strength/
  humanize/retune smoothing).  The R3 stream above the split still feeds
  the WET recorder -- recordings + playback keep R3 quality.  ~40 ms
  substitution fade on correction-toggle edges (fade is a pure function
  of the sample index so all channels blend identically; member advances
  once per block); shifter cold-restarts on the engage edge (ring primes
  silent under the fade).  Total heard latency while tracking corrected:
  ~12 ms + device RT (~6-12 ms) = ~18-24 ms, down from ~55-60 ms.
- Honest bounds: no formant handling on the monitor path (R3 keeps it on
  recordings); at exactly-unity momentary shift with the tap phase parked
  mid-window the dual-tap can transiently blend two copies ~12 ms apart
  (mild thickening; parks on a single clean tap at engage and corrections
  keep the phase moving).

!! LOUD DEFAULT CHANGE (Jeff pick "a", 2026-07-17):
mixer_vox_{n}_monitorMode DEFAULT flipped 1 -> 2 (Bypass Pitch Corrector
-> With Effect) in all three places (param default in
VibeSynthProcessor::addLiveInputParams, VoxStripTask no-param fallback,
BaySickVocalProcessor::mMonitorMode member default).  The dry-monitor
default was the ~48 ms mitigation; with the ~12 ms monitor shifter,
corrected live monitoring ships ON by default.  Listen-LED right-click
menu unchanged (never labeled a default).

Jeff: "Builds clean" (2026-07-17).  Close opens: /review-batch over the
FULL combined diff.

## 2026-07-17 -- QA-Fe2 close: full-diff /review-batch CLEAN + fixes;
## commit gate

- /review-batch re-ran over the FULL combined uncommitted diff (working
  tree vs 6bbb8650, 39 tracked files +3450/-371 plus 10 untracked):
  ZERO BLOCKERS.  Reviewer verified hard: audio-thread allocation-
  freedom (armScSourceTaps relaxed-stores-only, applyScRecvDelay
  non-inserting find + pointer view, metronome one relaxed atomic,
  recorder plain countdown); the single message-thread setDelay call
  site; MonitorPitchShifter ring-wrap compliance + correct shift
  direction; no double SC delay advance (FilePlay early-returns); all
  four engine-push conversions covering every mScEngine holder; the
  two-stage solve's non-negative wants; L/B/D scTap+compDelay landing
  in the live processChainOnly path; ASCII/US-spelling/CPU-guard sweeps
  clean; the prior review's five fixes still in place.
- Three NEEDS-FIX, all fixed: (1) three stale "1 BypassCorr [default]"
  comments contradicting the flipped monitor default (VoxStripTask.cpp
  fallback site, BaySickVocalProcessor.h setMonitorMode + mMonitorMode
  member) -> reworded to "2 WithEffect [default since QA-Fe2 docket
  2a]"; (2) stale "highRes oversamples that warp (16a)" clause above
  renderPitchedTake (the param was retired this batch; Align-side
  highRes untouched) -> clause dropped; (3) running-notes/plan-file lag
  vs the Main Plan section-9 back-reference -> already resolved
  mid-review (the work-item-B checkpoint + plan-file scope addition
  landed while the reviewer ran; timing artifact, no extra work).
- Three actionable NITs also fixed in-batch: CompDelayLine header
  comment no longer claims need-proportional allocation (setDelay
  allocates the full 256 KB cap per activated line; only never-
  activated lines are free); countInBeatsFired comment updated for the
  in-loop beat-1 trigger (was "rising-edge"); applyScRecvDelay channel
  guard tightened < 1 -> < 2 (CompDelayLine::process iterates mNumCh=2)
  with a one-line why.  Fourth NIT = no-action record: the
  message-thread setDelay-vs-live-audio realloc hazard is the
  pre-existing PDC contract; this batch grows the line population but
  not the trigger frequency.
- FOR THE MASTER TEST PLAN CAMPAIGN (reviewer's explicit ask, recorded
  so the held Work Log backfill picks it up): add line items for
  (1) metronome-on-the-beat with a latent chain loaded, (2) SC key
  timing under compensation, (3) the TD monitor's sound -- it now ships
  as the DEFAULT monitor mode, so every new Vox strip hears it first.
  Suggested Work Log buckets at backfill: Players, Effects, System
  Pages PLUS Mixer / Routing for the PDC/SC work.
- SUPERSEDED SAME DAY (Jeff's order at the commit gate): the test-plan
  update happened IN-BATCH -- v1-master-test-plan.md gained section
  B.11 (FE2-1..14: the batch feature re-verify set + the three PDC
  items above as FE2-10/13/14 plus master-recorder trim FE2-11 and
  per-insert-rack PDC FE2-12); blocks: placeholder backfills with the
  close commit hash at the campaign, per the B.10 precedent.  QA-Fe
  still has NO section-B block of its own -- flagged to Jeff, his call.
- Reviewer recommendation: READY-TO-COMMIT after the comment fixes
  (done).  Commit gate opens: ONE batch commit, message + FULL git
  status surfaced for Jeff's approval; then the G2 boundary wrap
  (Jeff's smoke).
