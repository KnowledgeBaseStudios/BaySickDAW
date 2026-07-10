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

`blocks:` `d6d46cf` (QA-TransportDisplay Tasks 1+2). Debug exe FIRST (screenshot any jassert), then
confirm in Release — mark each scenario `D:` and `R:`.

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

### §B.2 — QA-Chords (multi-select resize + scale-aware dual-mode chord stamp)

`blocks:` `805ca03` (QA-Chords Tasks 1-3). Debug exe FIRST, then Release — mark each scenario
`D:` and `R:`.

- [ ] **CH-1 — chord resizes as a unit.** Stamp a Major 7 (Chords menu → click the grid), then
      immediately drag one member's right edge. Expected: all four notes stretch together by the
      same amount; ONE Ctrl+Z restores all four. `D:__ R:__` notes:
- [ ] **CH-2 — mixed-length same-delta.** Marquee-select notes of different lengths, drag one
      selected note's edge. Expected: each note changes by the same amount (relative differences
      preserved); the shortest floors at the minimum length without stopping the others.
      `D:__ R:__` notes:
- [ ] **CH-3 — unselected grab = single resize.** With a selection active, grab the edge of a note
      OUTSIDE it. Expected: only that note resizes; the selection is untouched. `D:__ R:__` notes:
- [ ] **CH-4 — groups resize together.** Shift+G a set of notes, click empty space (deselect), grab
      one member's edge. Expected: the whole group resizes (consistent with group-move). Alt+G
      ungroups and they resize individually again. `D:__ R:__` notes:
- [ ] **CH-5 — left-edge multi-resize.** Ctrl+Alt+Home (left-edge mode), repeat CH-1. Expected: all
      selected notes' starts move by the same amount, right edges pinned; undo restores.
      `D:__ R:__` notes:
- [ ] **CH-6 — Mode 1 natural quality.** Scale menu: Root A, Scale Minor, Snap-to-Scale OFF. Stamp
      "Major" on E. Expected: E-G-B (the degree's natural minor triad), every note in A minor —
      NOT the literal E-G#-B. `D:__ R:__` notes:
- [ ] **CH-7 — Mode 1 structures.** Same scale, stamp Major 7 / Sus2 / Add 9 on various rows.
      Expected: 7th = four stacked scale-thirds; Sus2 = degrees 1-2-5; Add 9 = triad + ninth; zero
      out-of-scale notes anywhere. `D:__ R:__` notes:
- [ ] **CH-8 — Mode 2 collision resolver.** Scale Pentatonic Min, Snap-to-Scale ON, stamp Major 9
      and Dim 7 on several rows. Expected: no two chord notes merge — collisions resolve an octave
      up on valid scale degrees; note count = chord size (unless clamped off the top of the range).
      `D:__ R:__` notes:
- [ ] **CH-9 — ghost preview honesty.** Hover with the stamp in both modes before clicking.
      Expected: the preview outline shows EXACTLY the notes the click then places. `D:__ R:__` notes:
- [ ] **CH-10 — defaults + fallbacks.** Fresh roll (Scale menu untouched), Snap OFF: stamping
      degree-stacks in C Major sensibly. Scale = Chromatic: stamping places the literal chord
      shapes (old behavior). `D:__ R:__` notes:
- [ ] **CH-11 — no regressions.** Single-note draw/click/resize, click-memory, P-key Generate
      Chords, Alt+M mute, copy/paste all behave exactly as before; save → reload round-trips
      (stamped notes are plain notes, no format change). `D:__ R:__` notes:

---

### §B.3 — QA-TempoMap (stepped tempo timeline + ruler tempo flags)

`blocks:` `753dddc` (QA-TempoMap Tasks 1-4; also carries the QA-TransportDisplay readout z-order
fix — the readout was invisible until this commit). Debug exe FIRST — this batch touched the
transport core; screenshot ANY jassert. G1 ear-check batch: use the QA-TransportDisplay readout +
metronome as the measuring instruments.

- [ ] **TM-1 — marker lands on the downbeat (the headline ear-check).** Song mode, project at 140,
      metronome on. Ruler right-click at bar 5 → "Add Tempo Change" → 90. Play from bar 1.
      Expected: the click and the music audibly slow EXACTLY on bar 5's downbeat (readout `5:1:00`);
      no early/late tick around the boundary. `D:__ R:__` notes:
- [ ] **TM-2 — readout continuity.** Same setup, watch the readout across the marker. Expected:
      beats roll continuously (no jump at bar 5); time display's rate visibly changes slope.
      `D:__ R:__` notes:
- [ ] **TM-3 — multiple markers + song loop.** Markers 140 → 90 (bar 5) → 160 (bar 9); loop the song
      across both. Expected: every pass hits both changes tight; loop wrap returns to 140 cleanly;
      no drift pass-to-pass. `D:__ R:__` notes:
- [ ] **TM-4 — BPM field displays live, edits base.** Play across the bar-5 marker. Expected: field
      reads 140 before, flips to 90 at the marker. While past the marker, type 120 into the field.
      Expected: the pre-marker section is now 120 (verify by replaying from bar 1); the marker
      section stays 90. `D:__ R:__` notes:
- [ ] **TM-5 — automation override + marker re-assert (last-writer-wins).** Tempo automation clip
      spanning bars 3-7 (right-click BPM field → Automate tempo, draw values), marker 90 at bar 9.
      Expected: automation values win while its points play (bars 3-7); past the clip the tempo
      holds until bar 9, where the marker re-asserts 90. `D:__ R:__` notes:
- [ ] **TM-6 — clips follow the step (ear-check).** A Stretch-mode audio clip spanning bar 5
      (marker 140→90): rate slows at the boundary, pitch locked. A Resample-mode clip: slows AND
      pitches down (vinyl). `D:__ R:__` notes:
- [ ] **TM-7 — pattern mode = base only.** Switch to pattern mode with the bar-5 marker present.
      Expected: pattern loops at the base tempo throughout (marker ignored); the BPM field edits
      take effect immediately; back in song mode the marker applies again. `D:__ R:__` notes:
- [ ] **TM-8 — flags UI round-trip.** Edit the bar-5 flag to 100 (right-click → Edit), hover
      tooltip shows "Tempo: 100.0 BPM from Bar 5"; delete it; undo is NOT expected (flags are
      prompt-driven, not undoable — matches D-2 markers). Save with two flags → reload → both
      restored, playback identical. `D:__ R:__` notes:
- [ ] **TM-9 — seek/stop/start around a boundary.** Seek before/after the marker while playing and
      stopped; stop mid-marker-section and restart. Expected: position readout consistent, no stuck
      notes, tempo always matches the section the playhead is in. `D:__ R:__` notes:
- [ ] **TM-10 — tempo edits while stopped keep the bar.** Stopped at bar 7 (past the marker), edit
      the base tempo. Expected: the playhead stays at bar 7 (readout unchanged in beats mode; time
      display re-derives). `D:__ R:__` notes:
- [ ] **TM-11 — MIDI record across a boundary.** Record-arm (MIDI), play a phrase across bar 5
      while typing/hardware-playing. Expected: recorded notes land where they were played relative
      to the beat grid on BOTH sides of the marker (no cumulative shear after it).
      `D:__ R:__` notes:
- [ ] **TM-12 — MT stress regression.** The big MT stress arrangement with 2 markers added: no
      dropouts vs the pre-batch baseline, DSP% comparable, no Debug jasserts. `D:__ R:__` notes:
- [ ] **TM-13 — pause/resume past a marker never edits the base (the G1 review BLOCKER's repro).**
      Base 140, flag at bar 5 = 90. Play past bar 5 → Pause (field shows 90) → Play → replay from
      bar 1. Expected: bars 1-4 still play 140 (the base survived resume). Repeat with Stop while a
      Builder time-selection starts past the marker, and with pause-resume inside a tempo-automation
      clip. `D:__ R:__` notes:
- [ ] **TM-14 — long automation ramp map health (the G1 review NEEDS-FIX repro).** A 60+ second
      tempo-automation ramp playing continuously: tempo follows smoothly the whole way; afterwards
      loop braces and clip starts behind the playhead are still exactly where they were (no
      position drift from map saturation). `D:__ R:__` notes:
- [ ] **TM-15 — mid-play BASE edit is a rule change, not a recorded event (G1 smoke).** No markers,
      no automation, playing: change the BPM field mid-song. Playback keeps its bar (position does
      not jump musically) and continues at the new tempo with at most a brief resync blip — NOT
      seconds of static. Stop, replay from bar 1: the new tempo from the very start, NO phantom
      tempo change at the spot where the field was edited. Repeat with a marker present: marker
      spans keep their own tempo, only base-owned spans change. `D:__ R:__` notes:

---

### §B.4 — QA-Eb (window resizability, min-size-clamp shape)

`blocks:` the QA-Eb source commit (message-tagged "QA-Eb"; hash backfilled at the next test-plan
touch). Debug first, then Release.

- [ ] **EB-1 — launch + restore-down.** Launch → still opens maximized. Restore-down → a movable
      window with drag-resizable borders. `D:__ R:__` notes:
- [ ] **EB-2 — smooth resize.** Drag-resize in both axes and diagonally. Expected: pages reflow
      live, no paint artifacts, no Debug jasserts. `D:__ R:__` notes:
- [ ] **EB-3 — maximize round-trip.** Maximize button toggles maximize/restore; restored position
      + size come back. `D:__ R:__` notes:
- [ ] **EB-4 — the floor (1100x700 — TUNE HERE).** Shrink hard toward zero: window stops at the
      floor. At the floor: transport bar fully usable (all buttons + readout + pattern dropdown +
      some ribbon), piano roll scrolls via its own bars, Builder grid via its viewport, Mixer via
      its bottom scrollbar, EffectsPage slots clickable. If anything is unusable, name it — the
      floor values are starting points for you to tune. `D:__ R:__` notes:
- [ ] **EB-5 — every page at two sizes.** Visit every page type at the floor and at ~1400x900.
      Expected: no clipped-off controls; exactly one scroll authority per surface (no double
      scrollbars, no scroll fights). `D:__ R:__` notes:
- [ ] **EB-6 — save/reopen at a small size.** Save a project while the window is small, close,
      reopen. Expected: no layout-dependent state weirdness. `D:__ R:__` notes:
- [ ] **EB-7 — window size persists (G1 smoke request).** Resize to a custom size, close, relaunch:
      same size + position. Maximize, close, relaunch: maximized. Delete `<WindowState>` from
      settings.xml (or first launch): maximized default. `D:__ R:__` notes:

---

### §B.5 — QA-Ec (true-length import + Resample follow + Shift+drag re-fit)

`blocks:` the QA-Ec source commit (message-tagged "QA-Ec"; hash backfilled at the next test-plan
touch). G1 ear-check batch — verify by ear + the readout. Debug first, then Release.

- [ ] **EC-1 — true-length import at any tempo.** Project at 90: import a ~2s WAV. Expected: the
      block spans exactly 3 beats (2s at 90 — check via the readout/grid), plays IDENTICAL to the
      source (no stretch artifacts, correct pitch). Repeat at 140 — different beat-length, still
      1:1 playback. The old behavior (instant stretch at any tempo != 120) must be gone.
      `D:__ R:__` notes:
- [ ] **EC-2 — Stretch mode follows tempo.** Stretch-mode clip, change project 90 → 120 (field).
      Expected: re-fits pitch-locked (same musical length, faster); ear-check quality.
      `D:__ R:__` notes:
- [ ] **EC-3 — Resample mode follows tempo (vinyl).** Same change on a Resample-mode clip
      (right-click → Properties → mode). Expected: speeds up AND pitches up together; at the
      clip's own import tempo it plays exactly 1:1. `D:__ R:__` notes:
- [ ] **EC-4 — Shift+drag re-fit, Stretch.** Shift+drag a Stretch clip's right edge to 2x length.
      Expected: content plays across the full new length, pitch locked. Ctrl+Z restores exactly.
      `D:__ R:__` notes:
- [ ] **EC-5 — Shift+drag re-fit, Resample.** Shift+drag a Resample clip to half length. Expected:
      double speed, one octave up (vinyl). Plain (unshifted) edge-drag still just trims/extends
      with playback speed unchanged (the F split). `D:__ R:__` notes:
- [ ] **EC-6 — sub-bar clip re-fit.** Import a short one-shot (< 1 bar), Shift+drag it longer.
      Expected: correct proportional re-fit (the ratio uses exact beats, not whole bars).
      `D:__ R:__` notes:
- [ ] **EC-7 — the old silence case.** Import at 120, crank tempo to 200. Expected: clip still
      audible with meter movement, correct Stretch behavior (no vanishing clip). `D:__ R:__` notes:
- [ ] **EC-8 — browser re-drag inherits.** Import at 90, delete the block, re-drag the file from
      the browser at project 140. Expected: places at its true length and plays 1:1 relative to
      its stored 90-BPM identity (i.e. Stretch-fits to 140) — no return of the 120 assumption; a
      Properties-set BPM on the entry survives re-drags. `D:__ R:__` notes:
- [ ] **EC-9 — clips stay grid-locked across tempo flags.** Ruler tempo flag at bar 5 (140→90);
      place clips at bars 3 and 7. Expected: both clips START exactly on their grid lines by ear
      and readout (the position seam through the tempo map); the bar-7 clip's length displays true
      at 90. `D:__ R:__` notes:
- [ ] **EC-10 — Vox/Inst path parity.** Route a clip to a Vox/Inst page (FILE-02 routing) and
      repeat EC-2/EC-3. Expected: identical follow behavior (Path B lockstep). `D:__ R:__` notes:
- [ ] **EC-11 — save/reload round-trip.** After re-fits + mode changes: save, reload. Expected:
      modes, lengths, pitches, re-fit identities all identical. `D:__ R:__` notes:
- [ ] **EC-12 — full-song clip across a tempo flag, NO crackle (the G1 smoke bug's repro).** Drop a
      full song file, tempo flag at end of bar 1 (both a speed-up and a slow-down). Stretch mode:
      rate changes at the flag, pitch locked, NO crackling, no audible position jump at the flag.
      Resample mode: rate+pitch change, NO crackling. Any residual Stretch-mode texture on a full
      mix = vocoder quality (report it, don't fail the scenario on it). `D:__ R:__` notes:
- [ ] **EC-13 — Stretch EDIT MODE drag re-fits (the G1 smoke gap's repro).** Toolbar mode =
      Stretch: plain right-edge drag on an audio clip re-fits (both clip modes, per EC-4/EC-5
      expectations). Toolbar mode = Slip: plain drag trims, Shift+drag re-fits. `D:__ R:__` notes:
- [ ] **EC-14 — stretch badge + Reset Stretch (G1 smoke picks, revised at boundary round 2).**
      Stretch a clip: an amber `xN.NN` pill appears bottom-right (exact factor to 0.01).
      Slip-trim it back: badge STAYS (the state is visible now). Ctrl+Z after a stretch reverts
      it AUDIBLY, not just visually. Right-click → Reset Stretch = the ORIGINAL DROP FORM in one
      click: natural speed AND full natural length AND slip offset cleared (position kept), badge
      gone; one Ctrl+Z restores the stretch. Reset is greyed on unstretched clips.
      `D:__ R:__` notes:
- [ ] **EC-15 — tempo detection is DISPLAY-ONLY (G1 smoke ruling, supersedes the A/A pick).**
      Drop ANY file (loop or full song, any length, any project tempo): it lands at its actual
      wall-clock length and plays 1:1 — detection never changes import behavior. Per-clip
      Properties shows "Detected tempo: ~N.N BPM" (read-only) or "(not detectable)"; the editable
      BPM field lives only on the browser entry. Grid-locking a loop is a MANUAL act (set the
      browser entry's BPM, or stretch it). Octave-error check on the display: a known-BPM file
      reading at half/double = note it (detector calibration is campaign-tunable).
      `D:__ R:__` notes:

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
