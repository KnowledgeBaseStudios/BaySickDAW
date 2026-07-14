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

`blocks:` `44d5c015` (QA-Eb Task 1). Debug first, then Release.

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

`blocks:` `67bd4f6e` (QA-Ec Tasks 1-3). G1 ear-check batch — verify by ear + the readout.
Debug first, then Release.

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

### §B.6 — QA-F (BaySickAlign build-out + shared composite/shifters + realtime pitch quality)

`blocks:` `9262c746` (QA-F Tasks 1-5) + `35ac9928` (Vocal Chain two-way wiring follow-on;
F-12). G2 ear-check batch — the after-QA-F ear-check (F-9/F-10-adjacent listening) already ran
in-batch; these scenarios are the full campaign walk. Setup for most: two Vox tabs — record or
drop a "leader" take on Vox 1 and a slightly-off "follower" take on Vox 2 (sequential same-row
clips only; overlapping-same-row is §C item C2). BaySickAlign lives on the FOLLOWER's Vox tab.
Debug first, then Release.
NOTE (QA-Fa recovery round, 2026-07-10): F-2 through F-6 were rewritten to the recovered
design — applied maps play LIVE through the chain, Render is export-only, staleness
auto-re-analyzes stop-gated. Failures in the rewritten expectations indict the "QA-Fa
recovery round" commit, not the QA-F commit. The recovery's own scenarios are §B.7
FA-12/FA-13.

- [ ] **F-1 — composite renderer feeds the lanes.** Put two audio clips on the follower Vox
      channel at bars 1 and 5. Open BaySickAlign, pick the Leader channel on the Leader lane.
      Expected: the Follower lane draws BOTH clips' waveforms at their grid positions (gap between
      them, no overlap error); the Leader lane draws its channel; the shared ruler lines the two
      lanes up (bar-1 content starts at the same x in both when both start at bar 1).
      `D:__ R:__` notes:
- [ ] **F-2 — Analyze/Apply builds the warp, playback goes LIVE.** With Leader picked:
      Analyze/Apply. Expected: no error; the Output lane (red) fills with a preview waveform that
      visibly rides the Leader's timing; AND pressing global Play immediately plays the follower
      channel time-aligned (onsets land with the leader BY EAR, no render step, nothing new on
      the grid). Analyze with NO leader picked = clean error dialog, not a crash.
      `D:__ R:__` notes:
- [ ] **F-3 — Mode = RESIDUAL tightness (QA-Fd rewrite, locked 9a/10a).** Same pair:
      Analyze/Apply under Loose, Play; then Tight, wait for the stop-gated re-analysis (or
      re-Analyze), Play. Expected: matching itself no longer depends on the Mode (the pairing
      window is internal, ~+/-400 ms — Tight finds the SAME pairs Loose does); what changes is
      how much natural timing survives: Tight = every paired word lands ON the leader (fully
      locked); Loose = words already within ~100 ms keep their own timing and only the
      outliers get pulled to the cap edge; Close sits between (~50 ms). Fine Tune trims the
      cap +/-50 ms (sum clamps at 0 = fully locked — valid, not a footgun). All three modes
      play CLEAN (the G2 chop fixes stand). A failed Analyze still reports word-start counts +
      pairs and states the previous alignment stays applied — but the "Try Close or Loose"
      advice is gone (modes no longer change matching). `D:__ R:__` notes:
- [ ] **F-4 — +Pitch = Blend / Variation / Types, LIVE at publish (QA-Fd rewrite, 12a/13a)
      [EAR-CHECK].** Close-Align+Pitch on a deliberately-flat follower note: Analyze/Apply,
      Play. Expected: the follower pulls toward the leader's pitch LIVE. Now turn the
      "Blend" knob (the renamed Range; free 0-100) DURING playback — the pull amount follows
      the knob with NO re-analysis and no timing change; "Variation" up leaves that much
      natural tuning deviation untouched (only the excess pulls — human feel survives);
      Mode changes PRESET the Blend value (Loose 0 / Close 50 / Tight 100) instead of
      re-windowing the knob. The per-side "Leader Type"/"Follower Type" pickers change
      DETECTION (next analysis) — flip one and confirm the stale badge arms. The Algos
      dropdown drives the RENDER's pitch pass: Render under each of the three Algos (PSOLA /
      Granular / Phase Vocoder), audition each export via "+ Add New Vox From Export" (mute
      the new strip after each listen) — each is audibly shifted, artifact-bounded.
      `D:__ R:__` notes:
- [ ] **F-5 — Render is EXPORT ONLY + history.** Render twice (tweak something between).
      Expected: `<project>/Aligned/{name}_align_v1.wav` and `_v2.wav` on disk (project must be
      saved first — unsaved project = clean error); HistoryScrubber lists v1 + v2 with dates;
      Del removes the entry from the list but leaves the file on disk. NOTHING appears on the
      Builder grid, no rows mute, and playback sounds IDENTICAL before vs after a Render (the
      live warp was already playing; the render is a file export). Re-analysis after a render is
      unchanged by the export's existence. Save/reload: the history list persists.
      `D:__ R:__` notes:
- [ ] **F-6 — stale detection auto-re-analyzes, STOP-GATED (QA-Fd amendments).** After an
      Analyze/Apply: with the transport STOPPED, move (or resize/mute) a follower clip on the
      Builder grid. Expected: the amber RE-ANALYZE badge appears, then ~1 s after the edit
      settles the analysis re-runs BY ITSELF (badge clears, a new version appears in the
      Versions dropdown, playback follows the new map). Same edit while PLAYING: badge reads
      "RE-ANALYZE ON STOP", playback keeps applying the LAST-applied map, and the re-analysis
      fires at the next transport stop. QA-Fd additions: the TIME-map knobs (Mode / Fine Tune
      / Flexibility / Max Shift / the Type pickers) also arm the stale badge — the map swaps
      at the next stop, honoring "maps only change while stopped" — while Blend/Variation
      republish live and do NOT arm it. RE-analysis stays stop-gated (Analyze + Versions +
      Undo/Redo grey during playback with stop-first tooltips), BUT a NEVER-analyzed pair's
      FIRST Analyze runs even mid-play (4a carve-out — the button stays live until a map
      exists; an ANALYZING... badge shows while it runs). Editing a sync point / protected
      area raises the badge but does NOT auto-run (mid-edit gestures — commit with a manual
      Analyze/Apply). `D:__ R:__` notes:
- [ ] **F-7 — presets + persistence round-trip.** Set Mode/Fine Tune/Pitch controls off-preset
      (green dirty dot lights), Save as a user preset (name it), tweak more, Load it back —
      values + dot state restore. Save the project with an analysis + sync points + protected
      areas + render history; close, reopen. Expected: WarpMap (Output preview after re-open via
      Analyze state), sync points, protected areas, and the history list all restore; the two
      channel picks restore. `D:__ R:__` notes:
- [ ] **F-8 — sync points + protected areas drive the result.** Place a sync point between two
      onset clusters (click the strip; drag its top handle = leader side, bottom = follower side).
      Analyze. Expected: pairing respects it as a hard boundary (no pair crosses it; the point
      itself is an anchor). Draw a protected area over a phrase; right-click → leave only
      "Protect Pitch" checked; Analyze + Render with a +Pitch preset. Expected: that phrase keeps
      its own pitch while still time-aligning; "Protect Timing" instead = the phrase keeps its
      own timing. Right-click → Delete removes. Undo/Redo walks these edits (and an Analyze) back
      and forward. `D:__ R:__` notes:
- [ ] **F-9 — [EAR-CHECK] realtime pitch quality at defaults.** Live/monitored vocal (or FilePlay
      OFF path), Realtime Pitch ON, defaults (Retune 60 ms / Strength 80%): correction reads
      musical, NOT chipmunk/robotic; vibrato around a note boundary doesn't warble between
      targets (the hysteresis); on-key notes pass through essentially unaltered (EXPECTED — not a
      bug); monitoring latency stays unobtrusive (~2 pitch periods; no phase-vocoder smear).
      Retune 0 ms + Strength 100% still gives the deliberate hard-tune effect. `D:__ R:__` notes:
- [ ] **F-10 — [EAR-CHECK] Formant Preserve + Throat Shift are audible.** Same setup, shift a few
      semitones via Root/Scale forcing (or sing off-key hard): toggle Formant Preserve — timbre
      naturalness audibly changes (chipmunk tamed when ON). Throat Shift +/-4 st with Preserve
      off: character (thin/full) changes WITHOUT re-pitching. Expect ~20 ms extra wet-path
      latency only while either is engaged (toggle edge may click once — engage resets).
      `D:__ R:__` notes:
- [ ] **F-11 — brand-safety visual pass.** Walk every BaySickVocal sub-tab's tooltips, labels,
      menus, dialogs + the Align editor end to end. Expected: no third-party product/brand names
      in USER-FACING strings and no trade-dress "clone" phrasing in the Vocal/Align source
      comments; the engine names BaySickAlign/BaySickPitch + universal keybinds remain BY DESIGN
      (section 12); nominative modeled-gear refs in code comments (1176/LA-2A) remain per the
      20a fair-use ruling. (BaySickPitch's editor still carries reference-product comments — that
      file is QA-Fa's full-rewrite surface; verify it under §B.7.) `D:__ R:__` notes:
- [ ] **F-12 — Vocal Chain panel wiring (owner catch at QA-F close; separate fix commit).** On
      the Vocal Chain sub-tab, while playing a vocal: turn De-esser Thresh, Compressor Ratio, a
      Saturation knob, and Limiter Ceiling — every knob audibly sticks (the old behavior snapped
      De-esser/Comp/Sat knobs back within a block). Flip the comp Knee selector, Peak/RMS, and
      the de-esser M/S selector — same. Toggle the de-esser Spectral engine (transport stopped) —
      sticks + persists. Save the project, reopen: ALL chain edits restore — bound knobs via the
      bsv_ params (incl. the new bsv_limiter_* set), saturation tube-land knobs via the per-slot
      state blob — and the panels DISPLAY the restored values (remount hook). Save a Vox page
      preset, load it on a fresh Vox tab: chain settings travel with it. `D:__ R:__` notes:

### §B.7 — QA-Fa (BaySickPitch composite-driven editor + DSP)

`blocks:` the QA-Fa source commits — the `d8cc9494` checkpoint AND the `62895ca8` recovery
round; FA-4/FA-6/FA-9 rewrites + FA-12/FA-13
verify the recovery commit specifically. G2 ear-check batch — the pitch-edit ear-check folds
into the G2 boundary smoke (owner call 2026-07-10). Setup: a Vox tab with 1-2 vocal clips
back-to-back on its channel (sequential only — overlap is §C item C2). BaySickPitch is the Vox
tab's third sub-tab. Debug first, then Release.
NOTE (QA-Fd, 2026-07-11): the pitch editor was REBUILT (§B.10). FA-2's edge-trim, FA-3's
draggable sub-curves, and FA-5's preset combo describe RETIRED behavior — their superseded
parts are marked inline and re-covered by the FD-* scenarios; walk the remainder here.

- [ ] **FA-1 — composite auto-resolve (no Load button).** Two vocal clips on the Vox channel at
      bars 1 and 3; open BaySickPitch. Expected: note pills (purple, teal waveform inside)
      auto-appear over BOTH clips at their positions — no file dialog anywhere; the Bass-green
      detected pitch curve threads through them; empty channel shows the "notes appear here
      automatically" empty state instead. `D:__ R:__` notes:
- [ ] **FA-2 — Slice / Edit modes.** Slice mode: click mid-pill → it splits into two pills at the
      click. Edit mode: drag a pill up a lane → pitch shift +1 st (green edit dot appears);
      InfoBar tracks Pitch/Cents/Length live during the drag. (SUPERSEDED by §B.10 FD-10:
      edge drags are now STRETCH/SQUEEZE, not trims — edge-trim retired at QA-Fd.)
      `D:__ R:__` notes:
- [ ] **FA-3 — sub-edit display box under the selected pill.** Select a pill: the display box
      (VIB / FRM / VOL / PITCH bars) appears under it. (SUPERSEDED by §B.10 FD-16: the bars
      are DISPLAY-ONLY at QA-Fd — editing moved to the popup sub-editor + on-pill handles;
      verify the box mirrors popup edits there.) `D:__ R:__` notes:
- [ ] **FA-4 — realtime applicator (no bake) + ON/OFF glide.** Nudge one flat note up a
      semitone; global Play. Expected: the note plays CORRECTED in place (FilePlay), other notes
      unaltered; a second zero-edit clip on the channel plays bit-identical (lazy-activate); the
      playhead line runs across the canvas during playback (Auto-Scroll follows when A is lit).
      Focus knob at 0 = untouched channel plays exactly as recorded even after analysis.
      ON/OFF (toolbar row 2): toggle OFF mid-note — the correction GLIDES back to the raw take
      (no click, no splice, Speed-rate glide); toggle ON mid-note — glides back in; with OFF
      settled the channel is bit-identical to pre-analysis. Render while OFF still prints the
      edits (export = the edits, not the monitor state). `D:__ R:__` notes:
- [ ] **FA-5 — Focus / Mod / Speed + user presets.** Raise Focus → notes pull toward centers
      audibly; Speed low = audible glide between notes, high = instant; Save/Load a user
      preset round-trips the three knob values. (SUPERSEDED in part by §B.10: the
      Loose/Close/Tight preset combo + dirty dot were RETIRED at QA-Fd — 7a; Focus now pulls
      to the Root/Scale set when Snap is on, FD-10.) `D:__ R:__` notes:
- [ ] **FA-6 — Render is EXPORT ONLY + "+ Add New Vox From Export".** Render (project saved
      first). Expected: `Pitched/{name}_pitch_v1.wav` on disk with edits printed; a second
      Render → `_v2`; NOTHING placed on the grid, playback unchanged (the edits were already
      live). THE RE-IMPORT FLOW — Vox ribbon dropdown arrow → "+ Add New Vox From Export":
      (1) grey rules: entry DISABLED when the project is unsaved, when no exports exist, or when
      all 6 Vox slots are used; (2) submenu lists the Aligned/ and Pitched/ wavs grouped under
      folder headers; (3) picking one spawns a NEW Vox strip + tab (Mixer strip + chain appear)
      with the export placed as a clip at its ORIGINAL timeline position (bar-check against the
      source clips); (4) PROMPT "Clone the source tab's vocal chain settings?" — Yes copies the
      chain knobs (spot-check one obvious chain setting) but NOT the source's analyses/history
      (new strip's BaySickPitch shows the empty/analyze state); (5) PROMPT "Mute the original
      Vox strip?" — Yes mutes the source strip on the Mixer (A/B by unmuting); No leaves both
      audible. The new strip's clip is first-class: BaySickPitch on the NEW tab analyzes it, and
      it appears as a Leader candidate in another strip's Align picker. `D:__ R:__` notes:
- [ ] **FA-7 — Send Notes to... (MIDI only).** With a Layers tab open: Send Notes to → "Layer 1".
      Expected: the target tab's piano roll (current pattern) gains the detected contour as notes
      starting at beat 0 (rhythm at the current tempo); NO audio moves; the Clips target appears
      only when a Clips tab exists; empty list message when no target tabs are open.
      `D:__ R:__` notes:
- [ ] **FA-8 — persistence.** Save the project with slices + pitch/vib/formant/vol edits + a
      render history; close, reopen, open BaySickPitch. Expected: pills, edits (green dots),
      history all restore without re-analysis (signature matches); playback still applies the
      edits (applicator snapshot rebuilt on load). Undo/Redo walk the note edits (incl. an
      accidental Reset). `D:__ R:__` notes:
- [ ] **FA-9 — stale detection auto-re-analyzes, STOP-GATED + the QA-Fd first-analysis
      carve-out (4a).** After analysis: with the transport STOPPED, move a clip on the Builder
      grid (or change the project tempo / add a ruler tempo flag). Expected: amber RE-ANALYZE
      appears, then ~1 s after the edit settles the analysis re-runs BY ITSELF — no tab switch
      needed — and edits carry over for notes that still line up within 50 ms (a version
      snapshot is appended first).  Same edit while PLAYING: badge reads "RE-ANALYZE ON STOP",
      playback keeps applying the last-analyzed regions, and the re-analysis fires at the next
      stop.  The auto also works with the BaySickPitch sub-tab CLOSED.  QA-Fd carve-out:
      opening the sub-tab on a NEVER-analyzed channel analyzes IMMEDIATELY even during
      playback (no edits exist — provably inaudible; the felt "go to Align first" coupling is
      dead) with a visible ANALYZING... badge / canvas state; a STALE (already-analyzed)
      channel during playback shows DEFERRED UNTIL STOP instead of a silent empty canvas and
      re-runs at the stop; an analysis FAILURE names itself on the canvas instead of silence.
      `D:__ R:__` notes:
- [ ] **FA-10 — [EAR-CHECK, at the G2 boundary smoke] pitch-edit quality.** A corrected note
      sounds natural — no chipmunk (per-note formant left at 0 keeps timbre; deliberate FRM
      shifts change character not pitch); an exaggerated pill drag (+7 st) is audibly artifact-
      bounded (PSOLA, no phase-vocoder smear on the live path). `D:__ R:__` notes:
- [ ] **FA-11 — brand-safety visual pass.** Walk BaySickPitch tooltips/labels/menus + its source
      files. Expected: zero third-party product/brand strings (tree-wide Newtone grep is clean at
      code-complete); Slice/Edit + Focus/Mod/Speed naming everywhere; engine name BaySickPitch
      retained by design. `D:__ R:__` notes:
- [ ] **FA-12 — [recovery round] Align live warp at the decode layer.** Two-strip leader/follower
      setup (the §B.6 rig), follower audibly late by a constant offset. Analyze/Apply on the
      Align tab, then press Play with NO render: the follower plays time-aligned (onsets land
      with the leader by ear).  The Align editor's ON toggle mid-play: OFF glides the follower
      back to its natural (late) timing as a varispeed slur bent no harder than 2:1 (G2-boundary
      recalibration — glide time ~ the offset traveled; a ~200 ms-late take lands in ~0.2-0.4 s)
      — NO click, NO dropout, NO splice, NO position drift after repeated toggling (failures
      here indict the G2 boundary commit); ON glides it back into alignment.  With OFF settled,
      playback is bit-identical to
      pre-align.  Tempo-stretched case: change the project tempo away from the takes' BPM (clips
      stretch) — the warp still applies on top of the stretch, and the ON/OFF glide stays clean.
      Transport seek/loop-wrap while ON: playback resumes at the warped position instantly (a
      seek is a jump, not a glide).  DSP meter: a vox channel with NO applied map costs the same
      as before the recovery round (fast-path). `D:__ R:__` notes:
- [ ] **FA-13 — [recovery round] version histories + revert, both editors.** Align: three
      Analyze/Apply passes under different Modes → Versions dropdown lists v1..v3 newest-first
      with timestamps; during playback the Versions button greys with a stop-first tooltip
      (revert is stop-gated, G2 boundary); stopped, revert to v1 → press Play: playback
      audibly returns to v1's alignment; revert entries whose grid has since changed carry
      "(grid changed)" and reverting to one lights the stale badge immediately.  Pitch: analyze (auto v1), drag some
      pills, Snapshot (v2), drag more, then revert to v2 → the canvas + playback return to v2's
      edits; Ctrl+Z after a revert undoes the revert locally.  Save/close/reopen: BOTH dropdowns
      still list their versions and revert still works (persisted in project XML).
      `D:__ R:__` notes:

### §B.8 — QA-Fb' (recording lifecycle: bleed gate + monitor merge + conditional-WET + dirty)

`blocks:` `66fea472` (QA-Fb' Tasks 1-3). Setup: a Vox
tab with a mic on an ASIO input (arm LED → input picker) + an Inst tab with a DI line for FB-6.
Multi-take = record take 1, stop, then record take 2 WHILE take 1's clip plays back on the same
channel. Recording auto-places each take on its own NEW row — do NOT hand-stack two clips on
the SAME row (overlapping-same-row is campaign §C items 1-2, QA-J-Verify territory). Sound-
quality listening (glides, comp feel) rides the G2 boundary ear-check; these scenarios are
functional. Debug first, then Release.

- [ ] **FB-1 — multi-take no-bleed (Vox).** Realtime pitch ON (BaySickVocals sub-tab, bypass
      off). Record take 1 (sing a melody), stop. Record take 2 over it (count "1-2-3-4"
      distinctly) — you HEAR take 1 while singing. Stop. Mute take 1's row, solo/play take 2's
      clip. Expected: take 2 contains ONLY the counting — zero melody bleed; same for its DRY
      file (drag `Samples/... - DRY.wav` onto an Audio row and play it). `D:__ R:__` notes:
- [ ] **FB-2 — live capture survives overlap (no missing stretches).** During FB-1's take 2,
      land a count sharply ON a downbeat mid-overlap. Expected: take 2's clip plays that count
      exactly on its downbeat — not shifted early (the old early-return dropped overlap blocks
      from the file entirely, so everything after slid earlier); the take-2 WAV's length covers
      the full armed duration. `D:__ R:__` notes:
- [ ] **FB-3 — conditional-WET.** (1) Realtime pitch BYPASSED (default) + record → `Samples/`
      gains ONLY a `- DRY.wav` (no `- WET.wav` at all), and the DRY clip lands on the grid +
      browser once (no duplicate entry). (2) Realtime pitch ON + record → BOTH files; WET on
      the grid, DRY in the browser. (Case 3 RETIRED at QA-Fd: the page-master Bypass button
      and its bsv_bypass condition were removed — 3a/12b.) `D:__ R:__` notes:
- [ ] **FB-4 — monitor = playback preview (Option A / A1, Vox).** Before take 2: crank an
      obvious vocal-chain setting (heavy saturation or comp) AND drag one of take 1's
      BaySickPitch pills +2 st. While recording take 2: (a) take 1 sounds PROCESSED (the
      chain character is audible, not a dry/raw version); (b) take 1's edited note plays at its
      EDITED pitch (A1 — the monitor applicator); (c) your own voice monitors pitch-corrected
      through the same chain. After stop: play the stack — it sounds like what you heard while
      tracking (the comp reacting to voice+takes together IS the playback behavior). DSP meter:
      an idle Vox channel (no recording, no overlap) costs the same as before this batch.
      `D:__ R:__` notes:
- [ ] **FB-5 — armed with monitor off (listen off).** Arm the Vox strip, listen OFF, record
      take 2 over take 1. Expected: you do NOT hear yourself, take 1 keeps playing (produced,
      not raw) the whole time, and after stop take 2's clip still contains everything you sang
      (capture ran without the monitor). `D:__ R:__` notes:
- [ ] **FB-6 — Inst multi-take (same fix, DI).** On the Inst tab (pedals/amp set audibly hot):
      record DI take 1, stop; record take 2 while take 1 plays. Expected: take 1 is audible
      THROUGH the pedals/amp while tracking (one chain over the summed stack — same as
      playback); take 2's file contains only the fresh DI (no take-1 bleed, no missing
      stretches — repeat the FB-2 downbeat check). `D:__ R:__` notes:
- [ ] **FB-7 — listen-only monitoring coexists with clips (#5 fold-in).** UNARMED Vox strip,
      listen ON, transport playing over the channel's clips. Expected: you hear the clips AND
      your live mic together (pre-batch, the clips silenced the live monitor for their whole
      duration); works the same on the Inst tab with a DI line. `D:__ R:__` notes:
- [ ] **FB-8 — dirty on page-create.** New project (title clean, no `*`). Add a Vox page →
      `*` appears. Save (clean), add an Inst page → `*` again. Save, close, reopen → title
      stays CLEAN (the restore path must not phantom-dirty). Duplicate-tab and "+ Add New Vox
      From Export" adds also flip `*`. `D:__ R:__` notes:
- [ ] **FB-9 — dirty on record-finalize (regression confirm).** Record + stop → `*` shows;
      save → clean; reopen → clean. `D:__ R:__` notes:
- [ ] **FB-10 — clip-resize + composite (regression confirms).** Resize a Vox audio clip on the
      Builder grid: plain drag trims/extends the playable length, Shift/Stretch re-fits — playback
      follows the new edge (QA-Ec chain). Then open BaySickPitch on a channel with 2 clips —
      pills appear over both (the QA-F composite renderer feeding analysis, shared-dependency
      smoke). `D:__ R:__` notes:
- [ ] **FB-11 — [G2 boundary, QA-Fd amendment] realtime board locks during capture.** Arm the
      Vox strip and record. Expected: the moment capture starts (count-in included) the ENTIRE
      realtime section (Realtime Pitch toggle, Root/Scale, Retune/Strength/Humanize/Throat,
      Formant Preserve) PLUS the A/B slot grey to 40% and ignore input (Mix stays live —
      smooth param); the locked controls' tooltips read "Locked while recording - ...".
      Stop — everything re-enables. (QA-Fd: the page-wide chain Bypass LEFT the gate set with
      its removal — 3a/12b — and the "Key" label is now "Root".)
      Armed-with-listen-off capture locks the same way; monitoring WITHOUT recording (listen
      ON, unarmed) stays fully editable — that is the setup flow. Per-strip: a second Vox
      tab's board stays live unless that strip is also capturing. (Rationale: an engage-edge
      toggle mid-take clicks and prints into the WET file — owner call 2026-07-10 after the
      Part 6 tick listen. Failures indict the G2 boundary commit.) `D:__ R:__` notes:

---

### §B.9 — QA-Fc (BaySickNAMIR dual-mic stack: parallel Mic B, summed)

`blocks:` `a36ed3cc` (QA-Fc Tasks 1-3). Dual-mic lives
on the NAM/IR editor — the Vox tab's NAM/IR sub-tab or the Inst tab's NAM/IR sub-tab. The mic
stages only run when the page has a NAM model or cab IR loaded (existing engine gate — with
neither loaded the whole NAM/IR chain is a passthrough, Mic A included), so load a .nam or a
cab IR first. Feed the chain something sustained: an Inst DI, a held Vox note, or FilePlay
clips on the channel. "Mic B Active" is the OFF/ON switch at the top-right of the Mic B
column. Sound-quality listening rides the G2 boundary ear-check; these scenarios are
functional. Debug first, then Release.

- [ ] **FC-1 — regression (off = identical).** Mic B Active OFF (the default). Expected: output
      identical to before this batch (single-mic chain untouched); the Mic B column renders
      dimmed and its controls ignore clicks; a pre-batch project loads with Mic B off and
      sounds unchanged. `D:__ R:__` notes:
- [ ] **FC-2 — correlated sum (+6 dB, not a blend).** Set Mic B identical to Mic A (same Sim
      mode/model, same Placement values). Toggle Mic B ON while watching the strip meter.
      Expected: ≈ doubled amplitude (+6 dB) — the sound gets bigger, nothing crossfades away
      (that jump IS the parallel-sum design). `D:__ R:__` notes:
- [ ] **FC-3 — comb colouration from path difference.** Mic A distance 30 cm, Mic B distance
      120 cm, everything else identical, Mic B ON. Expected: audibly "fuller"/hollower
      character than either mic alone (comb filtering); slowly sweeping Mic B's Distance
      moves that character. `D:__ R:__` notes:
- [ ] **FC-4 — per-slot A/B + UI follow.** On slot A: Mic B ON, Built-in "Ribbon", Figure-8,
      distance 120. Switch to slot B: leave Mic B OFF (or configure it differently). Flip
      `A`/`B` back and forth. Expected: the tone snaps to each slot's stored dual-mic state
      AND every Mic B control follows (Active switch + dimming, mode/model/polar selectors,
      IR filename); the Mic A mode/model/polar selectors now follow slot switches too (same
      sync fix, both columns). `D:__ R:__` notes:
- [ ] **FC-5 — persistence (params + per-slot Mic B user IRs).** Load a user .wav IR into Mic B
      (mode auto-flips to User IR), configure slots A/B differently, save the project, close,
      reopen. Expected: Mic B state restores per slot including the user IR (filename label
      shows the active slot's file); the dimmed/enabled state matches the restored toggle.
      `D:__ R:__` notes:
- [ ] **FC-6 — no-click toggle mid-play.** With sustained audio playing through the chain,
      click Mic B Active ON and OFF repeatedly. Expected: each flip glides in/out over ~15 ms —
      no click, pop, or hard step (the param snaps, the DSP ramps). `D:__ R:__` notes:
- [ ] **FC-7 — CPU fast-path.** DSP meter with Mic B OFF ≈ the pre-batch idle cost (the off
      path is one param read + a branch); ON shows a small steady delta (the second mic
      chain). `D:__ R:__` notes:
- [ ] **FC-8 — both pages, independent instances.** Repeat FC-2 briefly on the OTHER page type
      (Inst if you used Vox, or vice versa). Expected: identical dual-mic UI + behavior; the
      two pages' NAM/IR settings stay independent (Mic B on one page doesn't affect the
      other). `D:__ R:__` notes:

---

### §B.10 — QA-Fd (vocal editor rework: align semantics + time-edit engine + editor rebuild + sub-edits + engage fix)

`blocks:` (backfill the QA-Fd close commit hash at commit). The consolidated rework batch —
locked dockets 1-20 + Newtone parity 1-14 (plan: `Batch Plans/snug-orbiting-catmull.md`).
Setup: the §B.6 two-strip leader/follower rig for FD-1..FD-5; a single Vox tab with 1-2 vocal
clips for the rest; Jeff's two G2 test wavs (the align screenshot pair + the
missing-first-note case) are the FD-6 material. Rewritten F-3/F-4/F-6/FA-9/FB-3/FB-11 above
carry this batch's amendments — failures there indict the QA-Fd commit. Calibrations
(match window, residual caps, flex ratios, Hz bands, crossfade, merge thresholds) are
Claude's first-pass values, TUNED at this smoke. Debug first, then Release.

- [ ] **FD-1 — residual semantics by ear.** Leader/follower pair, follower loosely late.
      Tight: words lock hard onto the leader (default residual 25 ms; Fine full-left = 0 =
      exact lock). Loose: the take keeps its human feel — only words further than ~150 ms
      out get pulled (to the cap edge, not fully). The same words pair in all three modes
      (internal matching): Tight no longer "loses" pairs. Fine sweeps the mode's window
      (Tight 0-50 / Close 50-150 / Loose 100-200 ms, 12 o'clock = center) and the readout
      tracks in ms across the whole travel — no dead half. *(Amended 2026-07-11: owner
      respec from the bipolar-offset model.)* `D:__ R:__` notes:
- [ ] **FD-2 — Blend / Variation are LIVE knobs.** With +Pitch analyzed and playing: sweep
      Blend 0 -> 100 — the pitch pull follows audibly mid-play, timing untouched, no
      re-analysis, no stale badge; raise Variation — natural tuning deviation returns while
      big misses stay corrected. `D:__ R:__` notes:
- [ ] **FD-3 — Max Shift guard.** A take with one badly-timed word: Max Shift wound down to
      ~50 ms caps how far ANY word moves (listen for the residual offset staying put); at
      full-right No Limit (the default) the guard never binds. Time-map knob: each change arms
      the stale badge and applies at stop. *(Amended 2026-07-11: (a) Flexibility picker
      REMOVED — the DSP runs a fixed Normal 2:1 slope bound in the background, so there's no
      rung to test; (b) Max Shift knob is 10-150 ms + a No Limit top stop, was 10-400.)*
      `D:__ R:__` notes:
- [ ] **FD-4 — Pitch Types per-side bands.** Set the Leader type to its material (e.g. High
      Vocal for a high harmony) and re-analyze: pitch detection locks the right octave (the
      +Pitch pull stops chasing octave errors, if any were audible under Normal). Band flips
      arm the stale badge (detection-time controls). `D:__ R:__` notes:
- [ ] **FD-5 — publish-vs-stop reconciliation.** Playing: change Mode (time knob) — badge arms,
      playback does NOT lurch, the new map lands at stop; change Blend (pitch knob) — applies
      immediately, badge stays dark. `D:__ R:__` notes:
- [ ] **FD-6 — no-silent-drop segmentation [Jeff's two wavs].** Analyze the missing-first-note
      wav: the first note now HAS a pill (the onset glide merged forward — the rising curve is
      visible INSIDE the pill, the pill center sits on the stable note); the align screenshot
      pair shows pills matching what Align's lanes show. Unvoiced/short material (consonants,
      breaths) shows as gray SLICE pills in the bottom lane — time-editable, no pitch handle;
      silence stays pill-free. `D:__ R:__` notes:
- [ ] **FD-7 — analysis states + first-analysis carve-out (both editors).** Fresh channel,
      transport PLAYING, open BaySickPitch: ANALYZING... shows, pills appear WITHOUT stopping
      (never-analyzed = immediate). Make it stale and reopen during playback: DEFERRED UNTIL
      STOP, fires at stop. Align: never-analyzed pair mid-play — Analyze button is live and
      works; after a map exists it greys during playback. An empty channel's analysis failure
      names itself on the canvas. `D:__ R:__` notes:
- [ ] **FD-8 — Bypass button is GONE, edits always audible.** The BaySickVocals sub-tab has no
      page-master Bypass (Mix + A/B remain); pitch/align edits are audible in every page
      state; a pre-QA-Fd project that had bsv_bypass saved ON loads fine and PLAYS (the param
      is ignored). `D:__ R:__` notes:
- [ ] **FD-9 — time edits are the performance (pitch upstream of align).** Align OFF: drag a
      word's pill right ~200 ms — playback moves the WORD (neighbor gaps counter-warp, no
      other word moves); drag it back. Align ON over a leader: make the same time edit — the
      align badge arms, at stop align re-analyzes against the EDITED timing and the correction
      follows the word (no wrong-syllable artifacts at offsets). Ctrl+drag = detach: the pill
      relocates freely; crossing its old neighborhood plays as a clean cut (brief, no garble
      spray). `D:__ R:__` notes:
- [ ] **FD-10 — motion model + snap.** Body drag: vertical = 0.1 st steps (Ctrl = 0.01 fine),
      horizontal = elastic move clamped at neighbor contact (pills never cross). Edge drag =
      stretch/squeeze (the pill lengthens/shortens; edge-TRIM is retired — re-partition via
      Slice + merge; flag if trim is missed). Snap ON: vertical drags land on in-scale lanes
      for the picked Root/Scale; Focus pulls toward the scale AND the pills visibly render at
      the corrected pitch while the green curve stays as-sung; Snap OFF: nearest-semitone
      behavior. Knobs detent at their defaults (Shift bypasses), Ctrl-drag = fine, the value
      box types in. `D:__ R:__` notes:
- [ ] **FD-11 — pill menu + merge + batch re-pitch + scrub.** Right-click a pill: Restore to
      Original State (everything back as-analyzed incl. time edits), Snap to Semitone (cents
      -> 0). Select two adjacent pills -> Merge Selected = one pill spanning both. Select
      several pills + click a keyboard key = all re-center on that note (hold = hear the
      focused pill). Dragging a pill while STOPPED re-renders + previews it (scrub-audition).
      Double-click = the sub-editor popup. Zoomed in, pills show their note name inline.
      `D:__ R:__` notes:
- [ ] **FD-12 — view + navigation set.** Ctrl+wheel h-zoom (cursor-anchored), Alt+wheel v-zoom
      (lane height changes, cursor-anchored), Shift+wheel h-scroll, bare wheel v-scroll,
      middle-drag pan. Ctrl+RMB-drag a rect = zoom to it; plain RMB on empty space = jump back
      to the pre-zoom view; pill menu "Zoom to Selection" works the same. The view floor stops
      at C0 — no lanes render below it and scrolling can't go past. `D:__ R:__` notes:
- [ ] **FD-13 — playhead + auto-scroll.** Play: the canvas playhead tracks the MAIN transport
      smoothly (30 Hz). STOP: the playhead returns to the transport's stop-seek position
      (start or the loop selection) — it no longer freezes where playback died. Zoom in +
      scroll away with A on: the view only page-flips when the ADVANCING playhead crosses the
      right edge — it never resets to the start on its own; with A off it never moves.
      `D:__ R:__` notes:
- [ ] **FD-14 — selection + clipboard.** Marquee-drag selects pills (Shift adds); Ctrl+A all.
      Ctrl+C on a treated pill, select others, Ctrl+V: the treatment (pitch offset, curves,
      vib/frm/variation) transfers in ONE undo step (audio does not relocate); Ctrl+B =
      duplicate the focused pill's treatment across the selection. Shift+Up/Down nudges pitch
      0.1 st; Shift+Left/Right nudges time ~10 ms (clamped at neighbors). Delete on pills =
      Restore to Original State (pills are analysis segments, not deletable objects).
      `D:__ R:__` notes:
- [ ] **FD-15 — global undo (9a).** Drag a pill, Ctrl+Z: the app-wide undo reverts it (the
      InfoBar's [edited] tag clears — #9); redo restores. The undo HISTORY window lists the
      pitch edits by label among app edits. Undo works across popup sub-editor edits and knob
      drags. Toolbar Undo/Redo = the same global stack. `D:__ R:__` notes:
- [ ] **FD-16 — sub-edit system end-to-end.** Select a pill: the display box shows 4 bars
      (VIB / FRM / VOL / PITCH), display-only (drags on it do nothing). EDIT (or double-click)
      opens the popup: pill waveform ghosted behind the lane; click adds points, drag moves,
      right-click deletes; Volume/Pitch lane toggle; the PITCH lane audibly bends pitch inside
      the one pill on preview; Vib/Frm knobs shape the note; VARIATION at 0 flattens the
      note's own wiggle, at 2 exaggerates it. Play button + SPACE preview the pill through
      current edits while stopped. The pill browser lists pills in order (filtered to the
      selection when opened from a multi-select) and switches the popup. RESET on the box
      clears the boxed params of the selection only (pitch offset + timing kept); a
      multi-select Reset prompts once with a working never-show-again checkbox (persists
      across app restarts). On-pill corner handles (selected pill, zoomed in): top = volume
      fade in/out (drag length), bottom = pitch approach/release (length + semitones,
      green=in/red=out, hover arrows); handle ramps appear as points IN the popup lanes and
      fades show on the pill waveform itself; re-grabbing a handle over hand-drawn points
      replaces that span (one undo step). `D:__ R:__` notes:
- [ ] **FD-17 — render parity + High-Res.** Align render vs live playback: A/B by ear — the
      export no longer steps/chops at word boundaries (the smooth-map port; re-render an old
      project's map after re-analyzing). A time-edited channel's renders (both editors)
      reflect the moved words. All three Algos render sane. High Resolution render completes
      and sounds at least as good as Standard (slower is expected; long takes use several
      hundred MB transiently — flag if that hurts). `D:__ R:__` notes:
- [ ] **FD-18 — engage-tick fix [realtime FIRST real listen].** Live monitoring, Realtime
      Pitch OFF -> ON mid-phrase: no click, no doubled-voice artifact beyond a brief (~40 ms)
      fade; OFF again — same. THEN the first honest hard-tune listen (every prior impression
      was formed on the broken shifter): Retune 0 / Strength 100 snaps hard; defaults sound
      musical; NIT-4 (the sync auto re-analyze UI hitch on long channels) gets Jeff's
      deliberate judgment during this session. OFF-path monitoring latency is unchanged from
      pre-batch. `D:__ R:__` notes:
- [ ] **FD-19 — persistence round-trip.** Make one of each: a time edit, a detached pill, a
      pitch-lane curve, a Variation tweak, Root/Scale/Snap picks, new align knob positions.
      Save, close, reopen: everything restores (pills at edited positions, curves in the
      popup, knob values); playback matches pre-save; a PRE-QA-Fd project loads with its
      pills at source positions and neutral new fields (nothing shifts). `D:__ R:__` notes:
- [ ] **FD-20 — realtime board Root + 13-scale list (17b).** The realtime section's first
      combo reads "Root"; the Scale list reads IDENTICALLY to the piano roll's picker (same
      13 names, same order — Chromatic through Blues, no Custom entry). A pre-QA-Fd project
      with a saved non-default scale loads SHIFTED (accepted at spec — the corrector was
      inaudibly broken before `703f06e4`); correction snaps to the picked scale's notes,
      including near the root wrap (a B-ish note pulls to the nearest in-scale note, never an
      octave down — the wrap fix). `D:__ R:__` notes:

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
