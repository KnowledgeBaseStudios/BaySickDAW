# QA-Fe Session Breakdown (Tasks 1-3 complete) -- resume at Task 4

> Consolidated handoff for the QA-Fe session (2026-07-13/14).  Companion to the
> append-only execution log `prancy-crunching-bear.md` (checkpoint-by-checkpoint)
> and the plan `Plans & Specs/Batch Plans/prancy-crunching-bear.md`.  This doc is
> the synthesized "what happened + what's left" for resuming in a fresh session.
> **Delete at QA-Fe close** (the Implemented Work Log + running notes are the
> permanent record).

---

## 1. Batch + the pivot (why this batch exists)

**QA-Fe = Vocal Pitch Engine: adopt library engines, retire PSOLA.**  Sixth batch
of group G2 (QA-F -> Fa -> Fb -> Fc -> Fd -> **Fe** -> QA-G).  QA-Fe rides the
**OPEN G2 boundary**; the boundary closes after QA-Fe's smoke.

The vocal pitch engine (editor / Align / real-time correction) all shared one
`PsolaShifter` (TD-PSOLA).  A 3-day chase proved TD-PSOLA is the wrong engine
(inherent moire warble, worst at the small shifts real-time correction uses; the
clean tools -- Melodyne / elastique / Auto-Tune -- use spectral/source-filter
vocoders, not PSOLA).  **PIVOT (Jeff, 2026-07-13):** adopt three vendored library
engines that A/B-shifted cleanly on Jeff's voice.  See Main Plan §9 fifty-seventh
(the PSOLA-rebuild insert) + fifty-eighth (the library-engine re-scope) Forks
entries.

**The three engines (all vendored in `libs/`, all license-clean for the app's
GPLv3 open-source path):**
- **Rubber Band R3** (GPL v2+, `libs/rubberband`) -- "Balanced", the DEFAULT.
- **Signalsmith Stretch** (MIT, `libs/signalsmith-stretch` + `signalsmith-linear`)
  -- "Lightest (Low CPU)".
- **WORLD** (modified-BSD, `libs/world`) -- "Highest Quality (High CPU)".

---

## 2. Status

| Task | What | Status |
|------|------|--------|
| 1 | Vendor wiring + CMake + repo GPLv3 LICENSE + notices | **DONE, builds clean** |
| 2 | `IPitchShifter` bake seam + background-cache editor + Align swap + delete PSOLA/[PITCH DIAG] | **DONE, builds clean** |
| 3 | `bsp_engine`/`bsa_pitch_algo` params + B2-labelled dropdowns + old-project migration + WORLD popup | **DONE, builds clean** |
| **4** | **Real-time vocal correction -> Rubber Band `R3LiveShifter`, dry-monitor default** | **NEXT (resume here)** |
| 5 | Monitor-button UX (right-click -> Dry / With-Effect popup, default Dry) | pending |
| 6 | Throat/character global control (engine formant param) | pending |
| 7 | Retire + clean (PSOLA/Granular/PvShifter remnants, throwaway Tools/*, stray WAVs) | pending |
| -- | **G2 boundary smoke (Parts 1-6)** = the end-to-end ear verification; CLOSES the G2 boundary | pending (Jeff's hands-on) |

**Execution mode = BULK-RUN:** ONE commit at batch close + Master Test Plan §B.##
backfill.  Per-task lines are BUILD checkpoints only (Jeff builds, says clean, I
move on).  **No per-task ear tests** -- Jeff's hands-on ear verification is the ONE
G2 boundary smoke at the very end.  **Nothing is committed yet** -- the whole batch
is one close commit.

---

## 3. Architecture (the model Tasks 2-3 built)

**Old:** every path ran per-sample `PsolaShifter` on the audio thread.
**New:** the library engines are block/whole-buffer (not per-sample), so:

### The seam -- `Source/DSP/LibraryPitchShifters.h/.cpp`
```
enum class PitchEngine { RubberBand = 0, Signalsmith = 1, World = 2 };
class IPitchShifter { virtual void bake (in, out, n, sr, pitchRatio[], formantScale[]) = 0; };
std::unique_ptr<IPitchShifter> makePitchShifter (PitchEngine);
```
`bake()` = LENGTH-PRESERVED mono shift with a per-sample pitch-ratio envelope + a
per-sample formant/throat scale.  **Worker/message thread only** (allocates; WORLD
transiently needs hundreds of MB).  Three impls, guarded by `BAYSICK_HAS_*`:
- **RubberBandShifter** -- offline mode can't do time-varying pitch, so it drives
  the REAL-TIME engine block-by-block (`OptionEngineFiner | OptionFormantPreserved
  | OptionPitchHighConsistency`, `setPitchScale` per block, pad `getPreferredStartPad()`
  + trim `getStartDelay()`).  Throat = `setFormantScale`.
- **SignalsmithShifter** -- latency-compensated streaming (`inputLatency()` +
  `outputLatency()` discard), per-chunk `setTransposeSemitones`; formant-preserve
  via `setFormantSemitones(0, compensatePitch=true)`, throat via `setFormantSemitones(X)`.
- **WorldShifter** -- Harvest F0 -> CheapTrick envelope -> D4C -> per-frame
  `f0[i] *= ratio` (formant-preserving) -> Synthesis.  `double`-based.  Throat =
  spectral-envelope frequency-axis warp.

### The editor -- `Source/DSP/BaySickPitchDSP.h/.cpp` (Option 3: background re-render)
- `analyzeComposite` stores the mono composite (`shared_ptr<const vector<float>>`).
- `computeEnvelopes` = the old per-sample target math (region shift + focus/snap +
  vibrato + pitch-shape + variation -> pitch/formant/gain envelopes), state passed
  in so the worker doesn't race live members.  Shared `bakeSpan` core
  (computeEnvelopes -> `IPitchShifter::bake` -> gain) used by the worker AND
  `renderOffline` (the Render/Freeze bounce).
- **Background bake worker** (`BakeWorker : juce::Thread`, started in ctor, stopped
  in dtor).  `requestBake()` (message thread; from `publishEdits` + every edit /
  knob / engine setter) captures a `BakeInput` under a mutex + `notify()`s.  The
  worker coalesces via `mBakeDirty`, bakes the whole composite into a fresh
  `CacheSnapshot`, publishes it lock-free (atomic ptr + 8-deep retire ring).
- **Playback** (`processFilePlay`/`Monitor`) just READS the cache at the source
  position (linear-interp, mono -> all strip channels, RT-safe).  Chain-OFF /
  no-cache -> dry passthrough.  Monitor delegates to processFilePlay (no separate
  DSP state).
- **Origin split:** the bake envelope is composite-relative (`snap.startSample=0`);
  the cache carries the TIMELINE origin (`BakeInput.timelineStart = mStartSample`)
  for the playback source-position mapping.
- **This gives Option 3:** RB/Signalsmith bake fast enough (16x/69x realtime) to
  feel live on drag-release; WORLD (3.6x) is delayed (the popup explains) -- the
  UI never freezes because all engine work is off the audio thread.  True
  audio-thread streaming (continuous drag-glide) was deferred -> Future State
  CL-300.

### Align -- `Source/DSP/BaySickAlignDSP.cpp`
`applyWarp` Phase-2 pitch pass: build a per-sample ratio from the anchor-cursor
semis lerp + Transpose, then `makePitchShifter((PitchEngine)pitchAlgo)->bake()`
over the warped result.  (Align is offline apply-then-play, so no live path / no
popup.)

---

## 4. Locked decisions (B1-B10 + session pivots)

From the plan file's spec-call table + the in-session pivots:
- **B1/B2** -- editor + Align = 3-engine dropdown; Rubber Band default; labels pair
  quality + CPU.
- **Option 3 (supersedes B7 "uniform bake"):** RB/Signalsmith feel-live via fast
  background re-render; **WORLD = background (async) bake -- delayed, NOT a hard
  freeze** (Jeff's explicit pick).  All engines bake on a background thread; the
  audio thread only reads the cache.
- **B3** -- real-time VOCAL correction (`PitchCorrectorDSP`) = Rubber Band
  `R3LiveShifter`, dry-monitor default.  **(Task 4.)**
- **B4** -- monitor button: right-click -> 2-option popup at the mouse (Dry / With
  Effect), default Dry.  **(Task 5, needs a loud paper-trail on the behavior
  change.)**
- **B5** -- PSOLA (`PsolaShifter`) + `PitchShifters.h::GranularShifter` retired from
  the vocal paths.  (`OctaveStyleDSP::GranularShifter` is a DIFFERENT class --
  untouched.)
- **B6** -- throat maps to each engine's native formant control.  **(Task 6.)**
- **B9** -- repo-root GPLv3 `LICENSE` (done Task 1).
- **B10** -- octave/instrument pedal is OUT (that's QA-OctavePedal, group G3).
- **WORLD popup (BaySickPitch only):** "WORLD works offline" InfoIcon notice with a
  persisted "Do not show this again"; fired only on a user pick of WORLD.
- **Migration:** `bsp_engine` is net-new; its ABSENCE in a loaded project's APVTS
  state marks a pre-QA-Fe project -> force `bsa_pitch_algo` to 0 (Rubber Band) on
  load.  (Clean version detector, no new marker.)

---

## 5. What was built, by task

**Task 1** -- `CMakeLists.txt`: `BaySickWorld` static lib (11 `world/src/*.cpp`,
manual like NAM), `BaySickRubberBand` static lib (single-file `RubberBandSingle.cpp`,
NOMINMAX), Signalsmith header-only include dirs; `/MD` inherited from the global
`CMAKE_MSVC_RUNTIME_LIBRARY`; `BAYSICK_HAS_{WORLD,RUBBERBAND,SIGNALSMITH}=1` defines;
standalone-only.  Repo-root `LICENSE` (canonical GPLv3, fetched byte-exact) +
`THIRD_PARTY_LICENSES.md`.

**Task 2** -- `LibraryPitchShifters.h/.cpp` (seam + 3 bakers); `BaySickPitchDSP`
rewrite (composite storage + worker + cache + playback-reads-cache + delete PSOLA
path + `Diag`); `BaySickAlignDSP::applyWarp` swap; `[PITCH DIAG]` stripped
(BaySickPitchEditor + PsolaShifter counters).  `PsolaShifter` + `CepstralFormantEngine`
classes KEPT (PitchCorrectorDSP + BaySickVocalProcessor use them until Tasks 4/6);
`GranularShifter` + `PvShifter` classes now orphaned -> **Task 7** deletes them.

**Task 3** -- `bsp_engine` param + push to `mPitch.setEngine`; old-project
migration; B2 labels on both combos; editor `mEngineCombo` + WORLD popup.

---

## 6. Remaining work (detail for Task 4+)

### Task 4 (RESUME HERE) -- Real-time vocal correction -> Rubber Band R3LiveShifter
- **File:** `Source/DSP/PitchCorrectorDSP.h/.cpp`.  It currently runs
  `std::array<PsolaShifter, 2> mShifters` + `std::array<CepstralFormantEngine, 2>
  mFormant` on the audio thread (the live pitch-correction path -- distinct from the
  editor; this is the live-tracking corrector).
- **Do:** swap `PsolaShifter` -> `RubberBand::RubberBandLiveShifter`
  (`#include <rubberband/RubberBandLiveShifter.h>`, `OptionFormantPreserved |
  OptionWindowShort`, ~48 ms latency).  Dry-monitor default (B3).
- **LiveShifter API (mapped this session):** constructor
  `RubberBandLiveShifter(sr, channels, options)`; **`getBlockSize()` is FIXED** --
  every `shift(const float* const* in, float* const* out)` must be handed exactly
  that many frames per channel, so you MUST ring-buffer / repartition around the
  JUCE block size (accumulate to blockSize, drain output).  `getStartDelay()` =
  frames to discard for input/output alignment (set pitch scale before querying).
  `setPitchScale` / `setFormantScale` may change between `shift()` calls (RT-safe).
  De-interleaved I/O (JUCE planar maps directly).
- **Reference harness:** `Tools/rubberband-test/rb_live_render.cpp` (the tested
  offline LiveShifter harness -- the working call sequence).  It's throwaway,
  stripped in Task 7.
- After Task 4, `PsolaShifter` is fully orphaned (editor + Align already off it) ->
  Task 7 deletes it + `CepstralFormantEngine` if Task 6 moved throat off it.

### Task 5 -- Monitor-button UX
Mixer-strip monitor button: right-click -> 2-option popup at the mouse (Dry / With
Effect), default Dry; wire the "With Effect" processed-monitor path.  Read the
current right-click handler first + LOUD paper-trail on the behavior change
(`feedback_option_removal_needs_loud_paper_trail`).  The strip that owns the vocal
monitor button is `Source/Standalone/MixerTrackStrip.cpp` or the Vox strip -- verify.

### Task 6 -- Throat/character (global)
A global throat APVTS param (`bsp_throat`?) + editor knob near Focus/Mod/Speed,
feeding the `formantScale` envelope (per-region `formantSemis` already flows through
the bake; this is the GLOBAL knob).  Maps to RB `setFormantScale` / Signalsmith
`setFormantSemitones` / WORLD envelope warp -- the bake seam already accepts a
formant envelope.

### Task 7 -- Retire + clean
Delete `PsolaShifter` (after Task 4) + `GranularShifter` + `PvShifter` classes from
`PitchShifters.h`; `[PITCH DIAG]` already stripped; remove throwaway
`Tools/signalsmith-test/`, `Tools/rubberband-test/`, `Tools/pitch-sim/`; strip
repo-root `enable_pitch_diag.txt` + the stray `SIM_*/PV_*/RBLIVE_*/SIGSMITH_*/
WORLD_*/RUBBERBAND_*.wav` A/B renders.

### Verification -- G2 boundary smoke (Jeff, at the end)
The plan's Verification section (Parts 1-6): smoke ladder, Align quality, pitch-edit
quality, real-time correction, real-time engage artifact.  Debug first, then Release.
On pass: ONE close commit + Master Test Plan §B backfill + `/draft-doc batch-close`
-> Implemented Work Log + the **G2 boundary CLOSES**.

---

## 7. Build environment (IMPORTANT)

Mid-session, **Visual Studio auto-updated its MSVC toolset (14.50.35717 ->
14.51.36231)** and removed the old one, so the `build/` CMake cache pinned a dead
compiler -> configure failed.  **Fix shipped: `do_configure.bat`** (repo root,
self-healing) -- tries a normal reconfigure, and on the stale-toolset failure it
clears the CMake cache metadata + reconfigures fresh, logging to
`configure_log.txt`.  **For any future toolset update: run `do_configure.bat` then
`do_build.bat`.**  (`do_build.bat` alone only builds; it can't reconfigure a dead
cache.)  Jeff runs ALL builds/configures -- Claude never does.

---

## 8. Routed out (do NOT do in QA-Fe)
- **Full-build warning cleanup + dead-code sweep** (incl. two C4702 unreachable-code
  warnings from a retired `ProjectBrowserWindow` block in
  `StandaloneEditor::doFileSetDefaultTemplate`) -> **QA-Cleanup-1** (folded into its
  §5 scope; §9 back-ref at QA-Fe close).  Capture the full warning list via a FRESH
  full build at that batch (incremental builds hide unchanged-file warnings).
- Octave/instrument pedal -> QA-OctavePedal (G3).  Builder-grid clip-stretch engine
  pick -> QA-H.  Dev vendored-dep watcher -> QA-Updater.

---

## 9. Working rules reinforced this session (memories written)
- **`feedback_never_run_builds_jeff_runs_all`** -- never run do_build.bat / cmake /
  configure; on a broken cache, EDIT the stale value in place (or hand Jeff a
  script), never delete the cache or run it.
- **`feedback_jeffs_word_is_the_evidence`** -- Jeff says "builds clean" -> that IS
  the evidence; don't re-verify with build_log greps / filesystem checks.
- **`feedback_no_task_fragmentation_no_per_chunk_eartest`** -- one plan task = one
  pass = one build checkpoint; never split into 2a/2b/2c; never ask for per-chunk
  ear tests (ear verification is the end-of-batch smoke).
- **`feedback_surface_full_research_recommendations`** -- surface the FULL research/
  agent recommendation incl. the harder "do it right" option; don't steer to the
  easy read.

---

## 10. Resume
Read this doc + the plan (`Plans & Specs/Batch Plans/prancy-crunching-bear.md`) +
the running notes (`Plans & Specs/Running Notes/prancy-crunching-bear.md`).  Start
Task 4 (PitchCorrectorDSP -> R3LiveShifter, dry-monitor default) per §6 above.
Tasks 1-3 build clean; nothing committed (bulk-run, one close commit).
