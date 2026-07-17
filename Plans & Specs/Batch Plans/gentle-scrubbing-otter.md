# QA-Fe2 — Vocal Cleanup: De-noise / De-reverb / Gate / Browser Groups — Plan (gentle-scrubbing-otter)

Canonical path: `Plans & Specs/Batch Plans/gentle-scrubbing-otter.md`
For execution.  Paired running notes: **`Running Notes/prancy-crunching-bear.md`**
(deviation from the same-name pairing rule, deliberate: this batch grew out of
the WORLD buzz/water investigation recorded there start-to-finish; splitting
the record mid-arc would orphan the evidence chain.  Jeff-approved scope,
2026-07-16 chat.)

**Execution mode (Jeff, 2026-07-16): bulk run.**  Per task: build the task ->
Jeff compiles + confirms clean (his word IS the evidence; no build-log
re-verification) -> running-notes checkpoint -> next task.  NO per-task ear
tests.  ALL functional/ear verification is consolidated in Task 5.  Close is
Task 6.  Spec calls that surface mid-run get ASKED, always.

## Context

The WORLD pitch-engine buzz/water investigation (see prancy-crunching-bear.md
2026-07-16 entries) proved the artifact = the take's own noise layer,
re-rendered F0-gated by WORLD's synthesis; denoising the take pre-analysis
("cleantake") was ear-validated as the fix.  Jeff generalized the fix into a
user feature set: recording-time De-noise takes, a Gate + De-reverb in the
vocal chain, and Builder-browser recording groups.  Risk: medium (record-stop
flow + browser tree are UI-heavy; two new DSP modules).  Effort: largest
batch of the QA-Fe arc.  Dependencies: none beyond committed 6bbb8650 state.

## Spec calls already locked (2026-07-16 chat, all Jeff's)

| ID | Decision | Reasoning |
|----|----------|-----------|
| S1 | File model = record-time real take files (up to DRY / DRY CLEANED / WET / WET CLEANED in `Samples/`); lazy-cache + per-clip toggle + `Cleaned/` folder DROPPED | Files visible + explicit; per-clip model's wet/dry profile ambiguity |
| S2 | Feature titled **De-noise**; chain stage titled **De-reverb** (uniform with De-esser); file tags stay `DRY CLEANED`/`WET CLEANED` | Naming uniformity |
| S3 | Dual live learners (raw tap + post-corrector tap) start at interface-track ASSIGNMENT (monitoring start), restart on reassignment | Wet noise floor is corrector-transformed; each variant cleans against its own domain |
| S4 | Learned profiles STORED in the project file per recording (both domains) | Powers right-click Regenerate De-noise (Light/Strong) |
| S5 | Arm popup: plain stock-looking dialog (system style, NO custom LAF), radio pick of grid-placed take + notice; unchecked pick writes THAT ONCE only (defaults never silently change); once per strip per session; re-ask after track reassignment | Jeff Q1=A, Q2=A, Q5 |
| S6 | Options > File Settings (dead menu -> real): 4 take-type checkboxes, >=1 enforced, De-noise strength default (Light/Strong) | Jeff's design |
| S7 | No cleanup path for imported/pre-existing takes | Jeff Q4; new projects test the flow |
| S8 | Browser: collapsible recording-group entries under Vox + Inst (base name entry, take-tag children); group rename disk-renames all takes + rewrites every clip/library ref; manual "create group" on Clips/Vox/Inst headers (Clips never auto-grouped); Properties move/copy gains destination-group picker | Jeff Q6 + screenshot |
| S9 | Browser panel right edge drag-resizable: default = min = current width, max = 3x; not persisted | Jeff 2026-07-16 final msg |
| S10 | Gate = NEW vocal channel gate (Threshold/Range/Attack/Hold/Release, hysteresis, GR readout), slot 0, DynamicsLAF; default transparent | Jeff 1=B; pedal reuse rejected |
| S11 | De-reverb IN this batch: late-tail spectral suppression (Lebart-family), Reduction/Tail/Mix, 1-FFT-frame latency w/ PDC; slot 1; NOT ML/RX-grade (stated + accepted) | Jeff final msg |
| S12 | Chain order: Gate -> De-reverb -> De-esser -> Comp -> Sat -> Limiter (rack 6/6 full); de-reverb pre-compression | Tail must not be pumped back up |
| S13 | WORLD -> stock: remove floorAperiodicity + deEmphasizeHF + peak-tanh; KEEP RMS match; D4C threshold stays default 0.85 | Jeff 2=B; cleantake = stock config |
| S14 | NO backward-compat/save-migration work (permanent pre-v1 rule) | Jeff, saved to memory |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open — all resolved in the 2026-07-16 chat workshop.
(Standing flag: if Jeff wants `DE-NOISED` file tags instead of `CLEANED`,
say so any time before Task 1's writer lands.)

## Files to modify

Precise file:line integration points for Task 1 sub-surfaces (input-assign
hook, arm observation, commitRecordingResult, Options menu, ui_prefs,
project-state profile persistence, browser menus) are being scouted at batch
open; scout results get appended to the running notes and the checkboxes
below reference them.  Already mapped (2026-07-16 scope workflow):
- `Source/PluginProcessor.cpp` — recorder start :4080-4153 (DRY :4102 / WET
  :4129), players :3389, composite :3530, Samples dir :4083
- `Source/BaySickVocal/BaySickVocalProcessor.cpp` — rack slots :325-336,
  bypass push :395-398, stage pushes :403/:421/:446/:456, save/load blobs
  :1652-1663/:1771-1783, raw tap `mMonitorLiveDry` :553-560, wet tap
  (post-corrector) :562-570, param layout :34-258
- `Source/BaySickVocal/BaySickVocalEditor.cpp` — VocalChainPanel :454-544,
  kNumChainSlots :540, mode-map :471-481
- `Source/Standalone/BuilderPage.cpp` — browser tree + menus :451-484,
  :942-984, :3188-3245, :3443; thumbnail :1562
- `Source/PatternManager.h/.cpp` — ArrangementBlock :311-360, save
  :1097-1143, load :1514-1536, library entries :686-689/:1193
- `Source/EffectRack.h/.cpp` — EffectType enum :15-58, createEffect :50-105
- `Source/Standalone/EffectEditorPanels.h/.cpp` — panel factory :6286+
- `Source/DSP/` — new `DenoiseDSP.h/.cpp` (offline, named De-noise),
  `GateDSP.h/.cpp`, `DeReverbDSP.h/.cpp`; templates: DeEsserDSP (dynamics),
  SibilanceSpectralProcessor (spectral mask skeleton)
- `CMakeLists.txt` — DSP module block :422-480

## Tasks

### Task 1 — De-noise core (engine, learners, record flow, popup, File Settings)
**DONE 2026-07-16 — Jeff: "Clean".  Checkpoint in running notes.**
- [x] `Source/DSP/DenoiseDSP.h/.cpp`: NoiseProfile (per-bin mag, STFT 1024/256
      Hann), rolling low-percentile learner (block feed, voice-gated),
      offline file->file cleaner (Light: 1.25x sub / -6 dB floor; Strong:
      2.0x / -12 dB floor — the ear-validated prototype constants), output
      sample-count identical, 3-bin freq + time gain smoothing
- [x] Dual live learners in BaySickVocalProcessor: SPSC rings off the raw tap
      + post-corrector tap -> background learner; start on interface-track
      assignment, restart on reassignment, snapshot at record stop
- [x] Profile persistence: `<DenoiseProfiles>` node in project state, keyed
      by recording base name, both domains, base64
- [x] Record-stop writer: popup pick UNION File Settings checkboxes ->
      generate `- DRY CLEANED.wav` / `- WET CLEANED.wav` (matching-domain
      profiles); dry pair only when corrector bypassed / Inst; chosen take
      -> grid via existing commit path
- [x] Arm popup per S5 (plain LAF dialog, radio group, notice text,
      once-per-strip-per-session tracking, reassignment re-ask)
- [x] Options > File Settings dialog per S6, persisted in ui_prefs.xml
- [x] Browser right-click "Regenerate De-noise > Light / Strong" on cleaned
      takes (stored profile)
- [x] CMakeLists + running-notes checkpoint; commit (one-liner, surface
      message + full git status, Jeff approves)
- [x] Tell Jeff: run do_build.bat, confirm clean (no functional test)

### Task 2 — Browser recording groups + panel resize
**DONE 2026-07-16 — Jeff: "build is clean".  Mid-task add: playback stop-gates on Regenerate/group-rename (Jeff's order).  Commit mode locked: ONE commit at batch close.**
- [x] Recording-group tree entries under Vox + Inst (collapsible; base-name
      entry, take-tag children); auto-group recordings by shared base name
- [x] Group rename: disk-rename all takes (base swap, tags kept) + rewrite
      every ArrangementBlock.audioFilePath + library entry + re-analyze
      affected channels
- [x] Manual "create group" on Clips/Vox/Inst header right-click (Clips
      manual-only, never auto)
- [x] Properties move/copy: destination-group picker for Vox/Inst targets
- [x] Right-edge drag resize: default = min = current width, max = 3x, not
      persisted
- [x] Checkpoint + commit gates; Tell Jeff: build, confirm clean

### Task 3 — Gate + De-reverb chain stages
**DONE 2026-07-16 — Jeff: "Builds clean" (after two mid-task panel fixes: LA-2A plate styling + real GRMeter assets, Jeff's calls).**
- [x] `Source/DSP/GateDSP.h/.cpp` per S10 (DeEsser skeleton: guarded setters,
      envelope follower, hysteresis, GR atomic, latency 0)
- [x] `Source/DSP/DeReverbDSP.h/.cpp` per S11 (SibilanceSpectralProcessor
      skeleton: per-bin late-tail estimate = delayed decaying mag history,
      Wiener-ish gain w/ floor, Reduction/Tail/Mix, frame latency + PDC)
- [x] EffectType entries (stable values) + createEffect cases + panels
      (DynamicsLAF) + SlotComponent names + preset-IO cases
- [x] Re-slot vocal rack per S12 (6/6); reindex :328-335 / :395-398 / stage
      pushes / save-load blob loops (0..5, NO migration per S14)
- [x] `bsv_gate_*` + `bsv_dereverb_*` params + pushApvtsToDsp blocks
- [x] VocalChainPanel: kNumChainSlots -> 6, mode-map reindex, Viewport
      fallback if 6 x 60px overflows
- [x] Checkpoint + commit gates; Tell Jeff: build, confirm clean

### Task 4 — WORLD to stock
**DONE 2026-07-16 — Jeff: "Clean".**
- [x] Remove floorAperiodicity + deEmphasizeHF + peak-tanh from WorldShifter
      (both bake paths); writeNormalized -> plain RMS match; keep D4C
      threshold at stock default
- [x] Checkpoint + commit gates; Tell Jeff: build, confirm clean

### Task 5 — Verification (consolidated end-to-end)
**DONE 2026-07-16 — verification complete (reconciled script; findings fixed
in-batch: #1 GateGRMeter scale, #2 arm popup RETIRED -> Builder Grid Default
menu).  WORLD verdict: SHIPS ("good enough" — Jeff).**
- [x] Numbered verification script executed by Jeff (reconciled version —
      stale plan steps purged per the running-notes record)
- [x] Findings routed per Rule 3 (both fixed in-batch)

### Task 6 — Close
- [x] /review-batch (first pass): zero blockers, five NEEDS-FIX fixed
- [x] Scope addition below executed (Jeff's fix-everything directive)
- [ ] /review-batch re-run over the FULL combined diff -> fix findings
- [ ] ONE commit (message + full git status surfaced, Jeff approves)
- [ ] Wrap the G2 boundary per Jeff's direction
- [x] Fork entries: metronome/TS regression -> G3 (fifty-ninth entry);
      bus-node consolidation -> Future State CL-301 (Jeff, 2026-07-16)
- NOTE: Implemented Work Log entry HELD per bulk-run R2 (no /draft-doc
      batch-close at this close; backfills at the Master Test Plan campaign)

## Scope addition at close — PDC full-graph pass (2026-07-16 -> 2026-07-17)

The Task-6 /review-batch latency finding (S11's "w/ PDC" clause was not
actually true) grew into a commissioned two-agent PDC audit, saved durable at
`Plans & Specs/Research Reports/pdc-coverage-audit-2026-07-16.md`.  Jeff's
directive: EVERY audit gap fixed IN-BATCH before the close.  Executed across
two work items (bulk-run cadence, both Jeff-confirmed clean):

- Work item A (audit gaps 1-7): `updateBusLatencies` rewritten as a two-stage
  minimal-latency solve (strip stage aligns every live insert within its
  actual `_sendTo` bus; bus stage aligns all 11 buses cross-path); ~130
  per-insert racks measured; preEq in every sum; FX bus + the 7
  InstrChannelNode buses gained CompDelayLines (were uncompensable);
  aux own-rack stage; NAM/IR latency consumed (vocal getter + new
  `onGetInstStripEngineLatency` hook via `EngineChainProcessor`); metronome
  clicks defer by total PDC (count-in as one unit; !! play-press catch-up
  click retired); master-recorder leading-trim
  (`AudioFileRecorder::startRecording` skipInitialSamples); 5 Hz poll
  re-solves the full graph.  CompDelayLine cap 8192 -> 32768.
- Spec dockets surfaced with pro-DAW grounding; Jeff's picks: sidechain =
  option (b), monitor = option (a), CL-301 (bus-node consolidation) routed
  to Future State by Jeff.
- Work item B: docket 1b — pre-compensation SC key taps (per-node scTap
  stash + block-rate arming) + per-receive alignment delays (fixed
  `mScRecvDelays` array solved in `updateBusLatencies`, applied in the pull
  helper); all four engine `setSidechainBuffers` sites unified onto the
  aligned receive buffers.  Docket 2a — new header-only
  `Source/DSP/MonitorPitchShifter.h` (dual-tap, 24 ms window, ~12 ms
  effective); With-Effect monitoring swaps the ~48 ms R3 stream for the TD
  shift while the WET recorder keeps R3; ~40 ms edge fades.
  !! DEFAULT CHANGE (Jeff pick "a", 2026-07-17): `mixer_vox_{n}_monitorMode`
  default 1 -> 2 — corrected live monitoring ships ON by default.
- Honest residuals (disclosed in the `updateBusLatencies` header comment +
  running notes): per-edge send/SC timing corners (cross-bus sends into one
  aux; bus->bus / bus->aux sends; re-cabled aux main-outs; SC source
  naturally later than its consumer).  Exact per-edge graph PDC = future
  work.

Full execution record: `Running Notes/prancy-crunching-bear.md` 2026-07-16/17
PDC entries (work item A checkpoint + work item B checkpoint + the audit
hand-off entry).

## Verification (end-to-end smoke)

See Task 5 — consolidated there by Jeff's bulk-run direction (2026-07-16);
no per-task verify scripts in this batch.

## Routing notes (Rule 3 application during execution)

Findings that surface mid-batch: real bugs fixed in-batch by default
(feedback_qa_batches_fix_bugs_dont_defer); spec-shaped findings get ASKED
(bulk-run rule); de-reverb quality tuning beyond "works as designed" routes
to a Fork entry at close, not endless in-batch iteration.

## Carry-Forward Reference touch points

- Task 1/2 start: §Vox/Inst strip + recording flow sections (recorder,
  commitRecordingResult) if present; else the 2026-07-16 scout reports in
  the running notes are the working map.
- Task 3 start: §EffectRack / vocal chain sections + DeEsser precedent.
- Task 4: none (LibraryPitchShifters.cpp is fully mapped in the notes).
