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
