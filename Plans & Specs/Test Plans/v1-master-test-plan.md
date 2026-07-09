# BaySickDAW V1 Master Test Plan

> **Canonical path:** `Plans & Specs/Test Plans/v1-master-test-plan.md` — created 2026-07-08 at
> bulk-run pre-flight per [`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md)
> (the governing run plan; structure per its "Master Test Plan" section).
>
> **How this doc works (R2 close model):** every bulk-run batch gets a §B section authored at its
> code-complete — numbered scenarios derived from the batch plan file's verify scripts (read from
> actual page/component code, physically executable). Jeff walks sections in commit order during
> the CAMPAIGN phase (after G4). A scenario failure -> fix commit referencing the owning batch ->
> re-run that scenario. **When a section passes:** the batch's HELD Work Log entry (in its running
> notes) applies via Edit, its Main Plan §5 line flips STATUS:CLOSED, Rule 3 routing runs for its
> logged findings, and the close commit lands. That is the ONLY doc-close path for G1-G4 batches.
> §A also re-runs as the smoke ladder at every group boundary during the code phase.

## §A — Global smoke ladder

Re-run at EVERY group boundary (15-30 min) and as the campaign opener. Debug exe FIRST (screenshot
any jassert dialog), then confirm in Release. Don't run both exes simultaneously (ASIO exclusivity).

- [ ] A1. Both configs build clean (`do_build.bat` — RELEASE_EXIT_CODE and DEBUG_EXIT_CODE both zero).
- [ ] A2. Launch + audio plays (default project, transport runs, meters move, no jasserts).
- [ ] A3. Big project loads (multi-engine stress project) — all tabs restore, plays without glitching.
- [ ] A4. Save -> close -> reopen round-trip — state identical (tabs, patterns, mixer, engine params).
- [ ] A5. Any spot-checks the group's /review-batch flagged.

**Group-boundary ear-checks (run with §A at that group's boundary):**

- **G1:** tempo-change sample accuracy by ear (use the QA-TransportDisplay readout); Stretch vs
  Resample behavior on tempo change AND block resize; chord stamping.
- **G2 (mid-group, not boundary-only):** after QA-F — align/warp quality by ear; after QA-Fa —
  pitch-edit quality by ear.

## §B — Per-batch sections (commit order)

Authored per batch at code-complete. Section format: numbered scenarios, each with steps / expected
result / Debug-first + Release-confirm columns / checkbox / PASS-FAIL + notes, and a `blocks:` field
naming the batch's source commit (bisect anchor on failure).

*(Sections append below as batches reach code-complete. Campaign position = first unchecked section.)*

---

### §B.1 — QA-TransportDisplay (position readout + D-4 typing keyboard)

`blocks:` the QA-TransportDisplay source commit (message-tagged "QA-TransportDisplay"; hash
backfilled at the next test-plan touch). Debug exe FIRST (screenshot any jassert), then confirm in
Release — mark each scenario `D:` and `R:`.

- [ ] **TD-1 — readout tracks song mode.** Song mode, arrangement with content, play from the top;
      watch bars 2-5. Expected: beats mode reads `N:1:00` exactly at each audible downbeat,
      advancing smoothly. `D:__ R:__` notes:
- [ ] **TD-2 — pattern-relative wrap.** Pattern mode, 4-bar pattern, loop 3+ passes. Expected:
      readout runs `1:1:00 -> 4:4:xx` and wraps to `1:1:00` exactly at the loop point every pass.
      `D:__ R:__` notes:
- [ ] **TD-3 — click-toggle + persistence.** Click the readout (beats -> time), play (expect live
      `M:SS.mmm`), quit, relaunch. Expected: time mode remembered across launch. `D:__ R:__` notes:
- [ ] **TD-4 — consistency across BPM change.** Play, change BPM mid-play (field, then tap), toggle
      display modes. Expected: no position jump in either display; time stays true wall-clock.
      `D:__ R:__` notes:
- [ ] **TD-5 — stopped + seek.** Stop mid-song; use prev/next-bar keys to scrub. Expected: readout
      shows the playhead position while stopped and follows seeks. `D:__ R:__` notes:
- [ ] **TD-6 — width floor.** (After QA-Eb lands) shrink the window to its minimum width. Expected:
      readout fully legible, ribbon still usable, bar height unchanged at 40px. `D:__ R:__` notes:
- [ ] **TD-7 — typing-mode toggle parity.** Toggle via Ctrl+T, then via the piano-keys button.
      Expected: both flip the same lit state; they never disagree. `D:__ R:__` notes:
- [ ] **TD-8 — chord + hold.** Layers tab, typing ON, hold `Z`+`C`+`B`. Expected: 3-note chord
      sounds and HOLDS; releasing one key releases only that note. `D:__ R:__` notes:
- [ ] **TD-9 — layout map + octave shift.** Play the Z-row + `S D G H J` sharps, then the Q-row +
      number sharps; PgUp/PgDn between phrases AND while holding notes. Expected: correct two-octave
      map; shift is +-1 octave; shifting while holding releases cleanly (no stuck notes).
      `D:__ R:__` notes:
- [ ] **TD-10 — tool keys bypassed + text fields safe.** Typing ON with a piano roll focused: press
      `Z X C S T M`. Expected: notes only — no tool switch, no note-type cycle, no keyboard-column
      toggle. Click into the BPM field and type: normal text entry, zero notes. Typing OFF: tool
      keys behave normally again. `D:__ R:__` notes:
- [ ] **TD-11 — record parity.** Record-arm (MIDI mode) on a Layers tab, play, type a phrase.
      Expected: notes land in the piano roll like a hardware MIDI keyboard; the on-screen keyboard
      lights while typing. `D:__ R:__` notes:
- [ ] **TD-12 — release guards.** Hold notes and (a) switch tabs, (b) Ctrl+T off. Expected: no stuck
      notes in either case; after the tab switch, typed notes play the NEW tab's engine.
      `D:__ R:__` notes:
- [ ] **TD-13 — Vox tab parity (not a bug).** Typing ON on a Vox tab, press keys. Expected: silence —
      identical to a hardware MIDI keyboard on a Vox tab today. `D:__ R:__` notes:

---

## §C — Deferred re-verify ledger

Parked items from closed batches. Lands INSIDE QA-J-Verify's §B section when that section is
authored (locked at marathon item 1). The four:

- [ ] C1. QA-AudioMeters BLOCKER re-verify — storeAxes CAS-max under overlapping same-row clips.
- [ ] C2. QA-F / QA-Fa / QA-Fb overlapping-same-row scenarios (§9 eighteenth Forks entry).
- [ ] C3. DSP-12 matrix re-test under the corrected premise (both flows through the SAME chain — QA-B content).
- [ ] C4. BUILD-06 resize-rebuild half (PhaseVocoder silence fixed 2026-07-02; resize retest pending).

## §D — Cross-cutting regression

- [ ] D1. Cross-batch multi-take session — record Vox takes over a Layers + Bass + Drums
      arrangement, edit, save/reload, re-record.
- [ ] D2. MT stress arrangement (high track/clip count) — no dropouts vs baseline, DSP meter sane.
- [ ] D3. DSP-meter sanity across buffer sizes (64 / 256 / 1024).

## §E — Preset + patch-save walk

Four families, ALL round-trip-verified (save -> reload -> identical state + audio), not just
menu-clicked. Runs AFTER both PRESET-BREAKs land (QA-ClipPlayback bipolar-stereo; QA-ApvtsAutomation
BLU-492). Source-ref detail lives in the run plan's §E notes (line refs re-resolve at section
authoring).

- [ ] E1. Engine presets — 10 engines x every factory preset (params restore + audio as expected);
      user preset save/reload identical; presets survive project save/load.
- [ ] E2. Effect-rack presets — per-effect preset menus + EffectPresetIO factory presets; user
      effect-preset save/reload round-trips.
- [ ] E3. Engine "Save Current Patch As..." flows — Layers / Bass / Drum (+ synth submenu + per-drum
      context menu) / Clips; saved patch reloads identical via the picker.
- [ ] E4. Page-level "Save Page Preset As..." / Load — all 7 page types; PLUS the "Save Page Preset
      & Delete" 3-button prompt path on page delete, and BaySickPedals "Save as Default".

## §F — RC-grade audits

Split model: Claude pre-audits by code-read and produces candidate-discrepancy lists; Jeff
spot-verifies the flagged items instead of walking everything cold.

- [ ] F1. Full menu walk — every menu item does what it says; nothing dead.
- [ ] F2. Keybind audit — Key Binds window vs actual handlers.
- [ ] F3. Tooltip review — presence + accuracy + ASCII-only.
- [ ] F4. Global FX bypass behavior.

## §G — Test-to-failure

Long-soak scenarios, scheduled as background soaks during G5:

- [ ] G-1. 100-track arrangement soak.
- [ ] G-2. 50-clip audio soak.
- [ ] G-3. Hours-long playback — leak/drift watch (memory, DSP%, position readout vs wall clock).
- [ ] G-4. Sample-rate + buffer-size switch matrix mid-session.
