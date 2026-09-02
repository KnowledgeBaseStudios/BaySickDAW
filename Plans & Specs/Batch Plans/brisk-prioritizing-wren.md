# QA-K — Audio Engine Polish + Factory-Preset Audit — Plan (brisk-prioritizing-wren)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/brisk-prioritizing-wren.md`.
> Paired running notes: `Running Notes/brisk-prioritizing-wren.md`.
> **Execution mode: bulk run** (R1-R5): §B at code-complete; Work Log HELD; ONE commit;
> spec calls ASKED.

## Context

§5 items APP-04/APP-05/DSP-08/DSP-11/DSP-01. Scout truth: no SetPriorityClass/MMCSS anywhere
(render workers are raw std::thread at OS-default priority); no showControlPanel (the custom
AudioSettingsDialog deliberately never touches the live device — pending-file + restart);
DSP-11's prior crash evidence is the WASAPI-exclusive hot-swap comments; Lasersaw's silence is
preset DATA (amp sustain 0.0 → dead in ~0.4 s; sibling uses 0.85); NO test/self-audit infra
exists. Docket #9 re-scoped DSP-01: no in-app tool — a data-read audit of all ~500 factory
presets producing a flagged candidates list for Jeff to spot-test. Risk: low-medium
(DSP-11 is the risky one, feasibility-gated). Effort: ~8-12h. Dependencies: none.

## Spec calls already locked

| ID | Decision |
|----|----------|
| Marathon 10a | DSP-08 (Tascam 21/22) = campaign hardware test; code map is ready, no G3 code |
| Marathon 10b | DSP-11 IN, feasibility-gated: build live ASIO buffer-size change + diagnose the prior crash; if the driver layer provably can't do it safely, surface evidence + fall back to the documented workaround (Jeff decides) |
| #9 | NO in-app audit tool. Fix Lasersaw's data; Claude runs a full data-read audit of ALL factory presets (every engine) for silent/broken setups; deliverable = flagged candidates report for Jeff's spot-testing |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

- `Source/Standalone/StandaloneApp.cpp` — `initialise` top (~:538) SetPriorityClass; live
  setAudioDeviceSetup precedent (:726-:729)
- `Source/Engine/VibeThreadPool.cpp` — `workerLoop` (:155) MMCSS "Pro Audio"
  (AvSetMmThreadCharacteristics) + revert on exit
- `Source/Standalone/StandaloneEditor.cpp` — AudioSettingsDialog (:70-405): control-panel
  button (near Apply/Close :108-121/:164-166), `applySettings` (:264-354) buffer-size live
  path
- `Presets/BaySickSolstice/Leads & Solos/Lasersaw.xml` — amp sustain data fix
- New report artifact: `Plans & Specs/Research Reports/factory-preset-audit-2026-07.md`

## Tasks

### Task 1 — APP-04 process priority + MMCSS
- [ ] `SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS)` at initialise top;
      MMCSS "Pro Audio" characteristics on each render worker at workerLoop entry (handle
      released on thread exit). RetirementQueue drainer + writer threads untouched.
- [ ] Build-confirm gate + running-notes checkpoint.

### Task 2 — APP-05 ASIO Control Panel button
- [ ] "Open ASIO Control Panel" button in AudioSettingsDialog next to Apply/Close; enabled
      only when the live device `hasControlPanel()`; calls `showControlPanel()`.
- [ ] Build-confirm gate + checkpoint.

### Task 3 — DSP-11 live buffer-size change (feasibility-gated, 10b)
- [ ] Buffer-size-ONLY changes take a live path: `setAudioDeviceSetup` with device/SR
      unchanged (startup live-reconfigure precedent); device/type/SR changes keep the
      pending-file + restart flow untouched.
- [ ] Diagnose against the documented WASAPI-exclusive hot-swap crash class: audio callback
      quiesced around the call (shield pattern), Debug-first verify.
- [ ] If the driver layer provably can't do it safely: evidence write-up in running notes +
      the pending+restart fallback stays → Jeff decides at approval of the §B section.
- [ ] Build-confirm gate + checkpoint.

### Task 4 — DSP-01 Lasersaw fix + factory-preset data audit (#9)
- [ ] Lasersaw.xml amp sustain 0.0 → sibling-class value (0.85; Jeff ear-tunes at campaign).
- [ ] Data-read audit of ALL factory presets, every engine (BaySickSolstice / BaySickSynth / Bass /
      Player / Vocal / Pedals / NAMIR / drum patches / effect + EffectPresetIO presets):
      flag silent/broken patterns — zero sustain with short decay, zeroed output/level
      params, filter closed with no env amount, out-of-range values vs each param's range,
      missing sample/kit references, duplicate-name collisions. Agent-swept, premises
      verified before listing.
- [ ] Deliverable: `factory-preset-audit-2026-07.md` — per-preset flag + reason + suspected
      audible symptom, grouped by engine; Jeff spot-tests from the list (campaign §E covers
      the rest).
- [ ] Build-confirm gate (data-only changes still ride the batch build) + checkpoint.

### Task 5 — Close (bulk run)
- [ ] §B section authored; Work Log entry HELD; ONE commit (message + full git status →
      Jeff approves).

## Verification (§B-destined scenarios)

1. Task Manager: process priority above normal; audio survives a UI-heavy stress (MMCSS
   regression check = no new glitches at 128 buffer).
2. Audio Settings: Control Panel button opens the ASIO panel on the Tascam/ASIO4ALL; greyed
   on non-ASIO.
3. Buffer-size change applies without restart (if feasible-path shipped): playback continues
   after apply, new size reflected in LAT readout; device/SR changes still prompt restart.
4. Lasersaw audibly sustains; spot-test N presets from the audit report's flag list.
5. DSP-08 stays campaign (hardware): outputs 21/22 pair test per the code map.

## Routing notes (Rule 3)

DSP-11 infeasibility = evidence + fallback (locked 10b), not silent scope-drop. Audit
findings needing CODE fixes (engine param-range bugs, loader defects) get ASKED for routing —
the audit deliverable itself stays data/report-only.

## Carry-Forward Reference touch points

- Audio-device infrastructure sections + the ASIO in/out name + enumeration records (G1
  smoke findings #1-#2) before touching device setup.
