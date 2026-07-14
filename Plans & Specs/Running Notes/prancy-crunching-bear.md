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
