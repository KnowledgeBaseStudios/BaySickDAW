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

### §B.11 — QA-Fe (pitch-engine pivot: vendored WORLD / Signalsmith / Rubber Band + LiveShifter realtime, Option A time-warp bake, pitch-editor parity)

`blocks:` `b9f1894f` (vendor drop + re-scope) + `bfb345ec` (garble fix + Option A engine-native
time-warp) + `6bbb8650` (pitch-editor grid parity + monitor-swap de-click + 1-based bar labels).
Authored late — at the QA-Fe2 close, Jeff's order — in its commit-order slot. Debug exe FIRST,
then Release — mark each scenario `D:` and `R:`. WORLD's synthesis config here is the QA-Fe2
"stock" state (the buzz-fix helpers this batch experimented with were deleted there); the WORLD
endgame + Rubber Band A/B is §B.12 FE2-7 — don't double-run it, this section covers the rest of
the pivot.

- [ ] **FE-1 — three-engine bake sanity.** Same edited take (a few clear note pitch edits),
      baked once per engine (WORLD / Signalsmith / Rubber Band — the same picker used for the
      Task-5 A/B). Expected: all three land the notes in tune at the edited targets; character
      differs (WORLD reads synthetic-er, Signalsmith/Rubber Band smoother) but NONE garbles or
      loses words — the garble class the engine pivot retired PSOLA over. `D:__ R:__` notes:
- [ ] **FE-2 — Option A time-warp bake.** One take with BOTH a time edit (elastic move or an
      edge stretch) and a pitch edit; preview, then bake. Expected: the bake honors both edits
      exactly as previewed (engine-native warp — no double-warp, no timing drift vs the
      preview); a take with ONLY time edits (zero pitch moves) still bakes/exports warped
      (the hasTimeEdit neutral-gate fix). `D:__ R:__` notes:
- [ ] **FE-3 — realtime corrector on the WET path.** Record a deliberately pitchy phrase with
      realtime correction ON; play the WET take back. Expected: in tune, natural formants, no
      moire/warble on held notes (the LiveShifter replaced PSOLA exactly for this); post-QA-Fe2
      note — what you MONITORED was the low-latency shifter, so judge the R3 engine on the
      recorded file, not the live phones. `D:__ R:__` notes:
- [ ] **FE-4 — monitor-swap de-click.** While live-monitoring a held note, right-click the
      Listen LED and swap monitor modes mid-note (True Dry -> Bypass Pitch Corrector -> With
      Effect and back). Expected: every swap crossfades (~10 ms) — no click/pop, even though
      the modes sit at different latencies. `D:__ R:__` notes:
- [ ] **FE-5 — pitch-editor piano-roll parity.** In the vocal pitch editor: zoom behaves like
      the piano roll; the playhead tracks transport playback; clicking the ruler seeks;
      dragging on the ruler selects a span; horizontal + vertical scrollbars present and
      functional. `D:__ R:__` notes:
- [ ] **FE-6 — 1-based bar labels.** Builder ruler, piano-roll ruler, and the drum-kit grid
      all label the first bar "1" (never 0) and agree with the transport readout at the same
      position. `D:__ R:__` notes:

---

### §B.12 — QA-Fe2 (vocal cleanup: De-noise / Gate / De-reverb / browser groups + PDC full-graph + monitor shifter)

`blocks:` `479790e8` (QA-Fe2 close, the whole batch in one commit). Debug exe FIRST, then Release —
mark each scenario `D:` and `R:`. The batch's feature set ran a consolidated in-batch
verification (Task 5, 2026-07-16); FE2-1..9 are the campaign re-verify of that set, FE2-10..14
are the PDC scope addition's FIRST functional pass (bulk-run R2 deferred them here).

- [ ] **FE2-1 — De-noise record flow.** Fresh project, assign an interface track to Vox 1
      (learners start on assignment), arm, record a phrase with realtime correction ON.
      Expected: written take set in `Samples/` matches the Options > File Settings checkboxes
      (union rule); the grid gets the Builder Grid Default pick; the browser shows one
      recording group under Vox with Dry / Dry Cleaned / Wet / Wet Cleaned children per the
      settings. `D:__ R:__` notes:
- [ ] **FE2-2 — Grid Default menu semantics.** Right-click the Vox arm LED; use the "Builder
      Grid Default" section (Dry / Dry Cleaned / Wet / Wet Cleaned). Expected: no pick =
      auto (Wet while correction is on, else Dry); a pick shows a tick, LOCKS until the
      project closes, and survives input-track reassignment. `D:__ R:__` notes:
- [ ] **FE2-3 — group rename + stop-gates.** Rename a recording group in the browser while
      its clip sits on the grid (playback STOPPED). Expected: every take renames on disk,
      the grid clip re-resolves and still plays. During playback: Rename Group + Regenerate
      De-noise menu items grey out with "(stop playback)". `D:__ R:__` notes:
- [ ] **FE2-4 — Regenerate De-noise A/B.** On a `* CLEANED` take: Regenerate De-noise >
      Light, listen; then Strong, listen. Expected: audible strength difference (Strong
      cleans harder), file regenerates from the stored profile, no length change.
      `D:__ R:__` notes:
- [ ] **FE2-5 — browser edge resize.** Drag the Builder browser's right edge. Expected:
      minimum = the default width, maximum = 3x; width resets on relaunch (session-local).
      `D:__ R:__` notes:
- [ ] **FE2-6 — Gate + De-reverb panels.** Vocal Chain sub-tab shows 6 slots in order:
      Gate, De-reverb, De-esser, Compressor, Saturation, Limiter. Gate defaults transparent
      (threshold -80); its GR meter scale runs 0..-80 with the red zone at the OPEN end and
      "GATE dB" on the LCD. De-reverb ships active (Reduction 50 / Mix 100); sweeping
      Reduction/Tail/Mix audibly changes a roomy take's tail. `D:__ R:__` notes:
- [ ] **FE2-7 — WORLD endgame regression.** Dry Cleaned take on the grid, WORLD engine pitch
      edit, bake, listen at master loudness. Expected: buzz/water character no worse than
      the accepted 2026-07-16 "good enough" verdict; A/B vs Rubber Band for reference.
      `D:__ R:__` notes:
- [ ] **FE2-8 — pitch-editor gesture map.** In the vocal pitch editor: body plain drag =
      vertical pitch ONLY (no time motion); Ctrl+drag = fine pitch; Ctrl+Alt+drag = elastic
      move (neighbor-walled); Ctrl+Shift+drag = detach move (hard cut, free); edge drag =
      stretch; Ctrl+edge = detach stretch; Shift+click still toggle-selects only when Ctrl
      is up. `D:__ R:__` notes:
- [ ] **FE2-9 — High-Res retirement (Pitch only).** The Pitch render dialog offers
      Render/Cancel only (no High Resolution button); BaySickAlign's render still offers its
      REAL High Resolution option. `D:__ R:__` notes:
- [ ] **FE2-10 — metronome on the beat under latency.** Fresh project with a default Vox
      strip (De-reverb active = latent chain), a tight drum pattern, metronome ON.
      Expected: the click lands ON the drum hits (pre-batch it led by ~45 ms); count-in
      spacing into bar 1 stays exact; transport LAT readout shows the chain latency.
      DELIBERATE CHANGE: pressing play mid-beat no longer fires an immediate catch-up
      click — the first click lands on the next real beat crossing. `D:__ R:__` notes:
- [ ] **FE2-11 — master-recorder trim.** No strips armed, record the master fallback WAV of
      a click-tight pattern (latent vocal chain still loaded), stop, place the WAV on the
      grid next to the source pattern. Expected: transients sit ON the grid (pre-batch the
      capture landed late by the full LAT amount). `D:__ R:__` notes:
- [ ] **FE2-12 — per-insert rack PDC.** Load a Limiter on ONE Layer insert's rack while
      drums play. Expected: LAT readout ticks up; that layer stays in time with the other
      buses (no double-hit/flam against drums); bypassing the Limiter mid-play re-aligns
      within ~200 ms (one soft click at the toggle is the documented cost).
      `D:__ R:__` notes:
- [ ] **FE2-13 — SC key timing under compensation.** With the latent vocal chain loaded:
      (a) kick strip keys a bass-strip compressor — pumping stays tight on the kick (keys
      are not skewed by compensation); (b) kick keys the VOX strip's mixer-side compressor —
      the duck lands ON the vocal's audio (the key waits for the late vocal), not ~45 ms
      early. KNOWN residual: the reverse direction (vocal keys a drum gate) runs late by
      the vocal's real latency — physics, per-edge PDC is future work. `D:__ R:__` notes:
- [ ] **FE2-14 — TD monitor default + sound.** Fresh Vox strip, correction ON, live mic.
      Expected: With Effect is the DEFAULT monitor mode (Listen-LED right-click shows the
      tick); the corrected voice in the phones feels immediate (~20 ms class vs the old
      ~55-60 ms slap); toggling correction on/off while monitoring never clicks (~40 ms
      fades); big shifts may thicken slightly (dual-tap doubling — accepted); the RECORDED
      WET take plays back R3-quality (slight timbre difference vs what was monitored is
      expected and accepted). `D:__ R:__` notes:

---

### §B.13 — QA-G (timeline geometry + TS system + 500 tracks + Split by Player Engine)

`blocks:` `928eca1d` (QA-G, the whole batch in one commit). Debug exe FIRST, then
Release — mark each scenario `D:` and `R:`. Covers the
plan's 6 tasks PLUS the mid-batch owner additions (row alignment + horizontal scrollbar
rework, 500-track cap, Split by Player Engine) and the in-batch bug fixes (rename
persistence, Move Up/Down state carry, drum-note previews, removePattern re-index,
File > New marker leak).

- [ ] **G-1 — ruler pin.** Scroll the Builder track list down (wheel + scrollbar).
      Expected: the timeline ruler stays pinned at the top (like the header corner);
      row labels and grid rows stay aligned; ruler clicks (seek, Ctrl+drag time-select,
      right-click menu) work identically while scrolled. `D:__ R:__` notes:
- [ ] **G-2 — zoom edge alignment.** Deep zoom in/out (Ctrl+wheel AND toolbar +/-).
      Expected: block right edges kiss their grid lines at every zoom; repeated toolbar
      +/- does not creep sideways; stretch badges / follow dots / pre-roll arrows stay
      cornered on their blocks. `D:__ R:__` notes:
- [ ] **G-3 — row alignment + zoom-out fit.** Alt+scroll to the vertical extremes.
      Expected: header rows stay pixel-locked to grid rows at EVERY row height (the old
      drift grew down the list); full zoom-out tiles ruler + rows exactly (no black strip
      below the last visible row); clicking blocks on deep rows at fractional zoom hits
      the right row. `D:__ R:__` notes:
- [ ] **G-4 — horizontal scrollbar.** Shift+scroll far right past the content.
      Expected: the bottom scrollbar thumb tracks the view live; dragging it left works;
      when the view returns to content territory the runway re-tightens on its own;
      Ctrl+zoom / zoom-to-rect / negative-bar pre-roll reveal all stay in sync with it.
      `D:__ R:__` notes:
- [ ] **G-5 — 500 tracks.** Scroll to track 500 (labels "Track 500"); place a block on a
      deep row; save/reopen. Expected: all state survives; zoom-out still shows ~50 rows
      max (500 reached by scrolling). `D:__ R:__` notes:
- [ ] **G-6 — track right-click set.** On a populated row: Insert Track Above (blocks +
      names + mute/solo + groups all shift down; positional "Track N" defaults stay
      positional, custom names travel); Group with Above two rows, Color Group... (live
      preview), Remove from Group; Move Up/Down carries mute/solo/group state WITH the
      track. Save/reopen: groups + colors + custom names all restored (rename
      persistence is NEW — renames never survived before this batch). `D:__ R:__` notes:
- [ ] **G-7 — insert at capacity.** Fill/occupy the bottom row (place a block on track
      500), then Insert Track Above anywhere. Expected: "Maximum Tracks Reached" prompt,
      nothing changes. `D:__ R:__` notes:
- [ ] **G-8 — note preview true positions.** *(SUPERSEDED by §B.22 SS-8 — tiling is now content-length; re-verify there.)* 1-bar pattern on a 4-bar block: notes tile
      4x at true positions (not stretched); 2-bar pattern on a 1-bar block: first bar
      only; a drums-only pattern shows its notes on the block (drum previews were BLANK
      since Phase D). `D:__ R:__` notes:
- [ ] **G-9 — song-mode tiling (DELIBERATE CHANGE).** *(SUPERSEDED by §B.22 SS-8 — re-verify there.)* A 4-bar block of a 1-bar pattern
      now LOOPS the pattern 4x in song mode (pre-batch: one pass + 3 bars silence).
      Audio matches the preview exactly. `D:__ R:__` notes:
- [ ] **G-10 — slice, Jeff's use case.** *(SUPERSEDED by §B.22 SS-9/SS-11/SS-12 — slice re-done; re-verify there.)* 1-bar pattern block, 3 one-beat notes + 2
      half-beat notes in the last beat; snap at 1/4; Slice at the last beat. Expected:
      the cut lands at snap resolution (1-bar blocks are sliceable now); the right piece
      shows AND plays only the 2 half-beat notes at their true timing; copy/paste it
      elsewhere plays identically; the pattern itself and its other blocks unchanged;
      undo restores the un-sliced block. `D:__ R:__` notes:
- [ ] **G-11 — slice continuation (loop/audio/automation).** *(SUPERSEDED by §B.22 SS-12 for pattern loop/mid-note; audio + automation slice continuation still verify HERE.)* Slice a looped 4-bar block
      of a 1-bar pattern mid-loop: both pieces play/display the correct continuation.
      Slice an audio clip off-bar: the right piece continues the file (no restart), in
      Stretch AND Resample modes. Slice an automation clip: both halves keep playing
      their original curve segments (value preserved at the cut). `D:__ R:__` notes:
- [ ] **G-12 — TS markers drive playback (the #14 regression re-run).** Type 7/8 into a
      grid marker mid-song (ruler right-click, free type-in). Expected: from that bar the
      ruler bars resize, bar numbering follows, the transport readout counts 1..7 in 8th
      units, and the METRONOME accents the true downbeats — verify 3/4, 7/8, 5/4 back to
      back, plus a mid-song 4/4 -> 7/8 switch staying sample-tight. Pattern-mode
      metronome follows the pattern's own signature. `D:__ R:__` notes:
- [ ] **G-13 — count-in at the record signature.** Precount + record with a 3/4 marker at
      the playhead (song mode). Expected: count-in lasts ONE 3/4 bar (3 beats, was
      hardcoded 4) with 3 clicks, accent first; a 7/8 position counts seven 8ths.
      `D:__ R:__` notes:
- [ ] **G-14 — pattern TS lifecycle.** Set a pattern to 3/4 via the NEW type-in dialog
      (presets submenu is GONE — deliberate); its roll grid shows 3-beat bars; the
      dropdown shows "Name 3/4". Drop its block: a linked OUTLINE marker pill appears at
      the block's bar; move the block — the marker follows; delete it — the marker goes;
      edit the pattern TS — the marker updates; edit the MARKER itself — it unlinks
      (solid pill, block edits no longer touch it); a manual marker on the same bar wins.
      Reset to Default in the dialog: the pattern re-enters following and its still-linked
      markers vanish. `D:__ R:__` notes:
- [ ] **G-15 — followers.** Never-set pattern placed after a 3/4 marker: its roll grid +
      suffix show 3/4 (live: moving the block or editing the marker re-derives; earliest
      block governs when several). Unplaced never-set patterns follow the sole marker;
      at the 2nd marker the current-TS prompt appears and NEW patterns bind to the pick;
      the transport dropdown's "Current Time Signature" submenu (grayed under 2 markers)
      re-picks manually; deleting the current marker with 2+ left re-prompts.
      `D:__ R:__` notes:
- [ ] **G-16 — move-across-TS prompt.** Follower pattern WITH notes moved from a 4/4
      region into a 3/4 region: prompt offers Proceed / Lock Previous TS. Proceed =
      it follows 3/4; Lock = it stays user-set 4/4 and NO marker appears from the move.
      Empty follower patterns move silently. `D:__ R:__` notes:
- [ ] **G-17 — File > New marker leak (fix).** Add TS + tempo + time markers, File > New.
      Expected: fresh project has a clean ruler (all three marker types cleared — they
      leaked before this batch). `D:__ R:__` notes:
- [ ] **G-18 — Split by Player Engine.** Pattern with MIDI on 3 of 5 tabs (e.g. a layer,
      a bass, a drum), placed twice on the grid, one placement on a grouped row.
      Right-click the pattern in the browser > Split by Player Engine. Expected: ONE
      group prompt (Yes = each stack's new rows join that row's group); 3 new patterns
      named "Original - <tab name>" (tab names, preset-derived); each placement becomes
      3 stacked blocks (original row + 2 directly below, in-the-way rows shifted down);
      everything still SOUNDS identical; the original pattern is gone; ONE Ctrl+Z
      restores the original pattern, its blocks, and the row state. `D:__ R:__` notes:
- [ ] **G-19 — split edge cases.** Split an empty pattern: info dialog, no-op. Split when
      the bottom rows are occupied enough to block the shift: "Maximum Tracks Reached" +
      project untouched. Split a 5-entry BaySickBasses-style case: 5 patterns, 5 stacked
      blocks. `D:__ R:__` notes:
- [ ] **G-20 — pattern delete re-index (fix).** Three patterns with blocks of each on the
      grid; delete the MIDDLE pattern from the dropdown. Expected: the other two
      patterns' blocks keep playing THEIR patterns (pre-batch they silently shifted one
      pattern over); the deleted pattern's blocks disappear. `D:__ R:__` notes:

### §B.14 — QA-H (piano-roll features: note types + properties + FL tools + Builder fixes)

`blocks:` `50c6eeb9` (QA-H, the whole batch in one commit). Debug exe FIRST, then
Release — mark each scenario `D:` and `R:`. Covers the
plan's 8 tasks plus the mid-batch owner calls (BOTH slides ship: Ramp + Retrigger;
"Flat" display name) and the in-batch transport fixes (expression-CC bleed, cold-voice
CC delivery, player CC ordering).

- [ ] **H-1 — armed note-type button + S.** The toolbar button between Select and Zoom
      cycles Flat -> RP Slide -> RT Slide -> Porta on click AND on S (grey at Flat, lit
      otherwise; tool row reads "Select" not "Sel"). With notes selected, S also converts
      the selection to the newly armed type (one undo step; no junk entry when already
      matching). New notes draw as the armed type (RP = filled right triangle, RT =
      outline triangle, Porta = orange arc). `D:__ R:__` notes:
- [ ] **H-2 — Note Properties popup.** *(SUPERSEDED by §B.22 SS-6/SS-7 — re-verify the current popup there.)* Double-left-click a note (Draw + Select tools):
      popup shows Flat/RP Slide/RT Slide/Porta + Velocity, Release, Fine Pitch, Panning,
      Filter Cutoff, Resonance; edits apply LIVE; with the clicked note in a selection
      every selected note takes the edit; ONE undo restores the whole popup session; a
      popup opened and closed untouched adds NO undo entry. Fast double-click-drawing a
      new note does NOT open the popup on it. `D:__ R:__` notes:
- [ ] **H-3 — RT Slide (retrigger).** *(SUPERSEDED by §B.22 SS-1/SS-2 — re-verify there.)* Note A, then an RT Slide note B at another pitch:
      B re-attacks and its pitch GLIDES from A's pitch to B's across B's full length
      (short B = fast glide, long B = slow). Works on BaySickSynth, BaySickBass,
      Harmless, BaySickPlayer (incl. drum tabs on those engines); also works when A ended
      earlier (glides in from A's pitch after silence). Inst/Rusty/Vox (sfizz) rolls:
      slide types are silent no-ops by scope. `D:__ R:__` notes:
- [ ] **H-4 — RP Slide (takeover).** *(SUPERSEDED by §B.22 SS-4 — re-verify there.)* Note A held, RP Slide note B overlapping or
      BUTT-JOINED to A's end: NO new attack — A keeps sounding and bends to B's pitch
      over B's length, and A's audible tail extends through B (release starts at B's
      end). Chain A -> RP1 -> RP2: one continuous voice bending twice. An RP note with
      NOTHING sounding at its start (gap) is silent. Works on all 4 engine families.
      `D:__ R:__` notes:
- [ ] **H-5 — Porta.** *(SUPERSEDED by §B.22 SS-3 — Porta now uses "Porta Length in Beats", re-verify there.)* A porta note re-attacks and glides in QUICKLY from the previous
      note's pitch: BaySickSynth/Bass follow their glide-time param when set; Harmless
      follows glide_time; BaySickPlayer (no glide param) uses the fixed ~60 ms fallback —
      audibly a snap-glide, campaign-tunable. `D:__ R:__` notes:
- [ ] **H-6 — Release + Resonance per note.** Two identical notes, one with Release 100%
      / one at 0%: audibly longer vs clipped tail on Synth/Bass/Harmless/Player. Same
      A/B for Resonance (50% = neutral, 100% = squelch) — on the player it rides the
      hardness filter. Values persist through save/reload ("r"/"q" attrs). `D:__ R:__`
      notes:
- [ ] **H-7 — expression isolation (bleed fix + cold voices).** Note A pan hard-left +
      cutoff high, then neutral note B: B plays CENTERED and neutral (pre-batch it
      inherited A's channel state). Flip side (documented): while A still SOUNDS, B's
      start snaps the shared channel pan/bend — last-note-wins, unchanged in kind.
      Cold-voice check: after 5+ seconds of silence, a single note with a non-neutral
      cutoff/resonance/release STILL applies it on all 4 families (juce only delivered
      CCs to warm voices; the player delivered them one note late). `D:__ R:__` notes:
- [ ] **H-8 — Humanize.** Tools > Humanize... on a quantized 8-note line: Start/Duration/
      Velocity Range+Offset knob pairs, Distribution (Quasi-Normal), Start Time Max
      Interval (snap list), Seed + Regenerate, Preview toggle (on = live), Reset, Accept.
      Same seed reproduces identical results; start shifts are late-biased within the
      interval; Accept = ONE undo; Esc/click-away reverts fully. Selection-or-all.
      `D:__ R:__` notes:
- [ ] **H-9 — Randomize (replaces the old instant jitter).** Alt+R / Tools > Randomize:
      Pattern section (Octave/Range/Key/Scale/Length st/Variation/Population/Stack/
      Random Portamento/Merge Same Notes/Seed arrows) generates into the roll live;
      Levels section (six -100..+100% wheels: Velocity/Pan/Fine Pitch/Release/Cutoff/
      Resonance + Reset Before Processing + Bipolar + Seed arrows) randomizes; Pattern
      OFF = Levels on the selection-or-all only. Seeds reproduce; Accept = ONE undo;
      cancel restores. Random Portamento notes audibly glide (H-5). `D:__ R:__` notes:
- [ ] **H-10 — Riff Machine.** Alt+E / Tools > Riff Machine...: walk the 8 steps
      (Progression presets + rate; Chord presets; Arp pattern/mode/sync/gate — notes land
      at the SNAP length via Sync=Time; Mirror chance; Levels wheels + Bipolar + Seed;
      Articulation presets; Groove presets; Fit key/scale/min-max/snap) with per-step
      enable + Reset + Random. Preview-to-step N kills stages past N; Dice rerolls
      everything; Work on existing score transforms the roll's notes via steps 4-8;
      Length sets the generated bars; Accept = ONE undo; cancel restores. `D:__ R:__`
      notes:
- [ ] **H-11 — lane scrub + guides + header.** Select some notes, Ctrl+drag a sweep
      across the control lane: ONLY the selected notes' dots set as the cursor passes
      them (value follows the cursor height); unselected dots untouched; no selection =
      nothing (and no undo entry); plain drag still edits one dot. Velocity + Filter
      Cutoff lanes show 25/50/75% guides + labels; pan/pitch keep the centre line. The
      Filter Cutoff lane header reads "Control > Filter Cutoff" (was "Pitch Bend").
      `D:__ R:__` notes:
- [ ] **H-12 — ghosts + pitch-row select.** Piano Roll page with notes on 3+ tabs:
      the active roll shows the OTHER rolls' notes tinted with their tabs' colors;
      switching the active roll re-tints correctly; View > Ghost Notes hides/shows them;
      pattern switches track. Ctrl+click a piano key: every note at that pitch selects
      (no audition); Ctrl+click more keys ADDS their rows. `D:__ R:__` notes:
- [ ] **H-13 — Builder #6 + #17 + #19.** (#6) Song mode, one 2-bar block, MUTE it:
      playback still runs 2 bars of silence (loop mode wraps at 2 bars; pre-batch the
      song shrank). (#17) Populate the Builder browser (audio tab expanded), quit the
      app: no crash. (#19) Move/delete a library entry's file on disk, then drag it from
      the browser to the grid: "Audio File Not Found" dialog with the path (was a dead
      click). `D:__ R:__` notes:
- [ ] **H-14 — Builder #20 active drop type.** Click a PATTERN in the browser, click
      empty grid with Draw: that pattern places (as before). Click an AUDIO leaf, empty-
      click: that clip places at its file length. Click an AUTOMATION entry (it now
      highlights on click — they were drag-only), empty-DRAW a 2-bar span: that
      template's automation block places at the drawn length. Switch browser tabs:
      the drop type re-arms to that tab's last pick; a tab with nothing picked places
      nothing. `D:__ R:__` notes:

### §B.15 — QA-I (heavy-op progress overlay: load / shutdown / engine + kit loads)

`blocks:` `b6f51617` (QA-I, the whole batch in one commit). Debug exe FIRST, then
Release — mark each scenario `D:` and `R:`. The
overlay repaints via explicit paint-pump at step boundaries: determinate bars move per
step; indeterminate ops show a sweep segment + wait cursor that only advance at step
boundaries (no animation mid-grind — by design, the ops stay synchronous on the message
thread).

- [ ] **I-1 — project load overlay (APP-03).** FILE > Open Project... (or Open Recent)
      on a multi-tab project: dimmed overlay titled "Loading Project..." with live step
      text — "Closing old tabs..." -> "Reading project state..." -> "Restoring
      patterns..." -> per-tab "Tab N of M - <name>" with the bar filling -> "Rebuilding
      audio strips..." — then the overlay drops and the UI is fully interactive. Wait
      cursor for the duration; clicks landing on the dimmed UI do nothing. A tiny
      project just flashes the steps through; bigger projects hold them readable.
      `D:__ R:__` notes:
- [ ] **I-2 — New from Template + Restore Backup.** FILE > New from Template... (pick a
      sample-heavy template): "Creating Project..." + "Copying template files..." then
      the I-1 step flow. FILE > Restore from Backup... (pick a backup): "Restoring
      Backup..." + the same step flow. Both end fully interactive. `D:__ R:__` notes:
- [ ] **I-3 — failed load never sticks.** Rename a test project's project.xml on disk,
      then open it from Open Recent: "Could not open project" alert shows and the
      overlay is GONE (no stuck dim layer, no stuck wait cursor); rename the file back,
      open again — loads normally. `D:__ R:__` notes:
- [ ] **I-4 — shutdown overlay (APP-02).** Load a heavy session (multiple engine tabs +
      Rusty), quit: the window stays up showing "Shutting Down..." — "Closing tabs and
      engines..." -> "Releasing audio device..." — until the app exits; NO bare black
      window period. Relaunch: maximized/sized window state restores exactly as before
      (QA-Eb regression guard). `D:__ R:__` notes:
- [ ] **I-5 — engine pick busy sign (NAV-02).** Fresh Layers tab -> pick an engine
      (Harmless / BaySickPlayer / BaySickSynth): "Loading <engine>..." busy overlay +
      wait cursor during the build (fast engines may only flash it). Same on a Bass tab
      pick and a Drums tab engine swap via the sound picker. `D:__ R:__` notes:
- [ ] **I-6 — drum sound + kit loads.** Drums tab picker: Browse a big sample folder ->
      "Loading Samples..."; an SFZ -> "Loading SFZ..."; a factory preset -> "Loading
      Preset...". Load a saved drum kit from the Kit menu: "Loading Kit..." +
      "Building drum tabs..." while the kit's drum tabs spawn. Rusty program switch
      (Full <-> Basic): "Loading Kit...". `D:__ R:__` notes:
- [ ] **I-7 — sfizz instrument spawn.** + Add Guitar and + Add Basses: "Loading
      Instrument..." busy overlay over the default keyswitch-SFZ load; the tab lands
      selected and playable after. During a PROJECT LOAD the engine restores surface as
      load-overlay steps only (no double overlay). `D:__ R:__` notes:

### §B.16 — QA-J' (stacking-batch residuals: unmute re-sync + applicator-map hygiene)

`blocks:` `b54a44d4` (QA-J', the whole batch in one commit). Debug exe FIRST, then
Release — mark each scenario `D:` and `R:`.
Background: mute gates skip a clip's whole render body, freezing the stretch reader's
file position while the transport advances — pre-fix, a mute shorter than ~2 seconds
slipped under the seek tolerance and playback resumed OFFSET by the mute length,
permanently (only a >2 s drift re-seeked). Separately, the automation applicator/reader
maps had no erase path at all — closed tabs and prior projects kept entries forever.

- [ ] **J-1 — sub-2 s unmute re-sync (timeline clip, stretch engaged).** Import a WAV
      to the Builder at 120 BPM, then set the BPM field to ~100 so the stretch path
      engages (clip defaults to Stretch mode; tempo mismatch = vocoder active). Play in
      song mode; mute the clip's Builder track for half a bar to a bar, then unmute:
      playback continues at the RIGHT song position immediately — no behind-the-song
      offset persisting after unmute. Repeat using the Clips strip's MUTE LED instead
      of the track mute. Let playback loop-wrap once after — no new artifacts. Repeat
      once in pattern mode. `D:__ R:__` notes:
- [ ] **J-2 — Vox FilePlay path.** Same shape on a Vox page: prerecorded clip on a Vox
      track, project tempo moved off the clip's import tempo, play; mute the clip's
      Builder track under a bar, unmute: correct position immediately. If a warp map is
      applied with the chain ON, repeat once — same result (align-warp path carries the
      same re-sync). `D:__ R:__` notes:
- [ ] **J-3 — long-mute threshold path unharmed.** Same setup as J-1; mute for 5+
      seconds, unmute: still correct (the pre-existing >2 s re-seek — must not have
      regressed). `D:__ R:__` notes:
- [ ] **J-4 — applicator-map hygiene (tab churn + project switch).** Add a Layers tab +
      engine, visit its mixer strip, right-click any knob -> Automate to open the Event
      Editor; the param browser lists the strip's params (mixer_layer_N_...). Close
      that tab; reopen the browser: those entries are GONE. Then load a DIFFERENT
      project: its automation lanes still drive their targets during playback, and a
      global tempo automation lane still changes the BPM (statics re-seed after the
      project-boundary map clear). New Project behaves the same. `D:__ R:__` notes:
- [ ] **J-5 — rack-knob automation on Aux/Vox/Inst/Rusty strips (docket-1 re-key).**
      Add Mixer Strip (aux) -> Effects page -> select the aux channel -> load Chorus in
      a slot -> right-click a rack knob -> "Automate: ..." -> lane created; play: the
      knob follows the lane. Spot-check the same on a Vox strip and a Rusty strip rack
      knob. ACCEPTED BREAK (docket-1): projects saved BEFORE this batch with lanes on
      aux/vox/inst/rusty RACK knobs (incl. Rusty Bus) show those lanes stale in the
      browser — re-automate recreates them; mixer-strip knob lanes are unaffected.
      `D:__ R:__` notes:

### §B.17 — QA-K (priority/MMCSS + ASIO panel + live buffer-size + preset audit fixes)

`blocks:` `684cf253` (QA-K, the whole batch in one commit). Debug exe FIRST, then
Release — mark each scenario `D:` and `R:`.
Background: process now runs Above Normal with MMCSS "Pro Audio" render workers;
buffer-size-only changes apply live (10b feasibility verdict happens HERE); first
launch performs the one-time factory-effect-preset re-seed (versioned seeding).

- [ ] **K-1 — priority + MMCSS regression.** Launch; Task Manager > Details >
      BaySickDAW.exe shows priority "Above normal". Play a busy project at a 128
      buffer while dragging windows / opening pages hard: no NEW glitches vs the
      pre-batch build (MMCSS should only ever help; any regression here is a red
      flag). `D:__ R:__` notes:
- [ ] **K-2 — one-time factory re-seed.** First launch after this build: playback and
      effect racks behave normally; afterward git status shows rewritten
      `Presets/Effects/**` factory XMLs + the new `factory_seed_version.txt` (expected
      heal, not rot). Load Reverb factory presets on a rack slot: "Vocal Tame" now
      sounds tight/dark (VocalBooth — clearly not the big hall), "70s Plate" pins the
      Plate topology, "Cathedral" stays the big hall. `My Presets/` files untouched.
      Second launch: no further rewrites. `D:__ R:__` notes:
- [ ] **K-3 — ASIO Control Panel button.** Audio Settings on the ASIO device: "Open
      ASIO Control Panel" enabled; click opens the vendor panel; change the buffer
      size IN the vendor panel and close it — device restarts, audio resumes, LAT
      readout reflects the new size. Switch the Audio Mode combo to Windows Audio
      WITHOUT applying: button stays keyed to the LIVE device (still enabled). On a
      machine/session where the live device is non-ASIO: button greyed.
      `D:__ R:__` notes:
- [ ] **K-4 — DSP-11 live buffer-size change (Debug FIRST — this is the 10b
      feasibility verdict).** Audio Settings: change ONLY Buffer Size, Apply: NO
      restart prompt; dialog closes; playback continues; LAT readout shows the new
      size. Repeat several sizes up/down during playback. Then change Sample Rate (or
      device): the old pending-file + restart prompt appears as before. If anything
      crashes/hangs in Debug here, capture it — the locked fallback is reverting the
      live path to pending+restart (Jeff decides at this section's pass).
      `D:__ R:__` notes:
- [ ] **K-5 — preset data fixes (ear list).** Lasersaw + Bell Lead sustain held notes;
      Tropical Pluck / Shimmer Drone / Shimmer Pad have their full body back; 606
      Closed Hat + Sleigh Bells are audible hats/bells (bright, not silent); 808
      Claves rings ~40 ms (not a click); the 7 transpose-clamped presets play at the
      same pitch as before (data now matches the clamp). Drum picker: Cabasa Shaker /
      Rimshot Acoustic / Stick-Hit Drum / Tambourine each appear ONCE (Hand
      Percussion only). `D:__ R:__` notes:
- [ ] **K-6 — Rusty extended-CC round-trip.** Rusty page: move the hi-hat pedal macro
      (an extended CC >= 128), save a player preset, tweak the macro elsewhere, reload
      the preset: the macro position restores (old saves silently dropped it).
      `D:__ R:__` notes:

*(DSP-08 Tascam outputs 21/22 pair test stays its own campaign hardware item per
marathon 10a — not part of this section.)*

### §B.18 — QA-L (UI polish + navigation + per-drum MIDI notes)

`blocks:` `2e2df50a` (QA-L, the whole batch in one commit). Debug exe FIRST, then
Release — mark each scenario `D:` and `R:`.

- [ ] **L-1 — right-click menu trigger (UI-01).** Open any menu (drum context menu,
      hamburger, pattern dropdown): RIGHT-click over a highlighted item — nothing
      activates and the menu stays up; left-click still selects. Spot-check in two
      different menus. `D:__ R:__` notes:
- [ ] **L-2 — deleted-slot lane names (UI-02).** Automate a rack knob (right-click ->
      Automate), then delete that effect from its slot: the lane's row + Event Editor
      title read "<Channel - Effect - Param> (deleted)" — no UUID soup; the stale-row
      tint still shows. `D:__ R:__` notes:
- [ ] **L-3 — strip lifecycle (MIX-05/07).** Close a Layers page: its mixer strip
      disappears and neighbors re-flow; the Effects-page channel dropdown no longer
      lists it. Re-add a Layers tab at the same slot: ONE strip, no overlap, prior
      fader/pan restored. Repeat once with a Drum tab. `D:__ R:__` notes:
- [ ] **L-4 — Builder header sync (NAV-01).** Scroll the Builder vertically, then
      resize the window and zoom rows: track header rows stay glued to grid rows at
      every step (no one-tick lag). `D:__ R:__` notes:
- [ ] **L-5 — nav buttons (#18 + C).** Every page type (Layers/Bass/Drums/Clips/Vox/
      Inst both variants): "FX Rack" at the right end of the sub-tab row jumps to that
      strip's rack on the Effects page. Piano Roll page: "Player Page" + "FX Rack"
      right of the roll dropdown act on the selected roll (edge picks: Drum Kit roll ->
      first Drums tab / Drums Bus rack; Rusty -> Rusty Bus rack). `D:__ R:__` notes:
- [ ] **L-6 — duplicate names (FILE-03).** Get two same-named entries (+ Add New Clip
      picking an already-imported file): the second shows auto-numbered "(2)". Rename/
      delete one — the OTHER survives untouched (grid clip titles follow the right
      one). `D:__ R:__` notes:
- [ ] **L-7 — roll click accuracy (LDT-394).** At deep zoom (in and out), click just
      left/right of a beat line and just above/below a row boundary: notes land on the
      intended beat/row; near-edge resize grabs still work. `D:__ R:__` notes:
- [ ] ~~**L-8 — per-drum MIDI notes (#10).**~~ **SUPERSEDED 2026-07-19 — DO NOT RUN.**
      The shipped version was note-only and gated on drum-tab focus, so it fires
      nothing on CC pads and the kit-trigger workflow it existed for was unreachable.
      Redesigned as per-drum kit triggers — see L-9..L-14. `D:n/a R:n/a`

**L-9 .. L-14 (per-drum MIDI kit triggers)** `blocks:` the QA-L-Fix commit (hash
backfilled at close) — plan `Batch Plans/eager-thumping-marmot.md`, design D-1..D-14
locked 2026-07-19. Needs a MIDI controller with pads; a CC-sending pad is required for
L-11.

- [ ] **L-9 — kit-only MIDI menu (D-2).** Right-click a drum on the DRUM KIT: the menu
      shows "Assigned: \<note\>" and "MIDI Learn". Right-click the sound picker on that
      same drum's PAGE: NEITHER item appears (Lock / Polyphony / Copy / Save / Delete
      only). `D:__ R:__` notes:
- [ ] **L-10 — assigned play note + re-pitch (D-4/5/6).** A fresh drum reads
      "Assigned: C5". Put a few hits on the kit. Change that drum's MIDI Note to D5:
      the label reads "Assigned: D5", the drum's hits move from C5 to D5 on its page
      piano roll, and playback is pitched up. ONE Ctrl+Z restores. A hit you
      deliberately placed at some other pitch on the page roll stays where it is.
      `D:__ R:__` notes:
- [ ] **L-11 — Learn a CC pad, fires globally (D-7/8).** Kit right-click a drum ->
      MIDI Learn -> move a CC pad/knob: it captures (NO prompt for CC). That pad now
      fires the drum from ANY focus — on the kit, on a drum page, and on a Layers tab.
      `D:__ R:__` notes:
- [ ] **L-12 — Learn a note pad, kit-focus only (D-7/8/10).** Kit right-click another
      drum -> MIDI Learn -> hit a NOTE pad: it captures AND prompts "Also set this
      drum's play note to \<note\>?" — take Yes on one drum, No on another. With the KIT
      focused, the pad fires its drum. Switch to a drum PAGE: that same pad no longer
      fires the mapped drum — the note just plays the focused drum chromatically (no
      double-fire). `D:__ R:__` notes:
- [ ] **L-13 — trigger plays the assigned note + velocity (D-9/11).** A triggered drum
      sounds at its ASSIGNED MIDI Note, not the pad's note. On a velocity-sensitive pad,
      soft vs hard hits differ. Flip the global "MIDI trigger velocity" setting to
      Fixed: every hit lands at the standard drawn-note velocity. `D:__ R:__` notes:
- [ ] **L-14 — persistence (D-14).** Save the project, close, reopen: assigned notes AND
      learned triggers (both the note one and the CC one) come back and still fire.
      `D:__ R:__` notes:

### §B.19 — QA-M (engine restoration lifecycle: kit-load vs Rusty)

`blocks:` `ce4cb33c` (QA-M, the whole batch in one commit). Debug exe FIRST, then
Release — mark each scenario `D:` and `R:`.
Background: a full-kit load used to destroy the Rusty tab (Drums-typed, caught in the
type-wide teardown) and never re-spawn it; re-adding Rusty gave an empty page.

- [ ] **M-1 — kit load leaves Rusty alone (LIFE-01).** Have a Rusty tab with a kit
      loaded AND one or more DrumPage drum tabs. Load a full kit from the kit menu:
      "Replace Drums?" prompt appears with the "(BaySickRustyDrums is not affected...)"
      line; on Proceed the DrumPage tabs are replaced by the kit's drums and the Rusty
      tab is STILL there, still playing its kit (audition it). `D:__ R:__` notes:
- [ ] **M-2 — Rusty-only project, honest prompt (LIFE-01).** Rusty tab loaded, NO
      DrumPage tabs. Load a kit: NO "Replace Drums?" prompt (nothing to replace); the
      kit's DrumPage drums load in alongside the untouched Rusty. `D:__ R:__` notes:
- [ ] **M-3 — re-add auto-reloads last kit (LIFE-02).** Load a Rusty kit (Full or
      Basic), delete the Rusty tab (confirm the delete prompt), then re-add Rusty from
      the ribbon +menu: the last kit auto-loads — kit graphic populated, program combo
      shows the right program, ARIA panel rendered, and it plays (not the empty
      pick-a-program overlay). Switch program to the other one, delete + re-add again:
      the NEW last program comes back. `D:__ R:__` notes:
- [ ] **M-4 — fresh session + project restore regression.** Brand-new session (no Rusty
      loaded yet): + Add BaySickRustyDrums shows the pick-a-program overlay (no
      phantom kit). Then save a Rusty project, close, reopen: the kit restores exactly
      as before (restore path unchanged). `D:__ R:__` notes:

### §B.20 — QA-N (DSP meter sum-of-cores, DIAG-02)

`blocks:` `2e44ab78` (QA-N, the whole batch in one commit). Debug exe FIRST, then
Release — mark each scenario `D:` and `R:`.
Background: under MT the DSP% used to measure the audio thread's wall-clock only, so
parallel worker time vanished (it read the critical path, not total work). It now sums
per-task busy time across the audio-thread pump + every worker.

- [ ] **N-1 — MT reads higher (total work counted).** Heavy project (many tracks/FX),
      Multi-core Rendering ON: the transport DSP% reads NOTICEABLY HIGHER than the same
      project on the pre-batch build (parallel work now included), and climbs further as
      you add tracks. No audio glitches vs pre-batch at 128 buffer (measurement change
      only). `D:__ R:__` notes:
- [ ] **N-2 — MT OFF unchanged.** Same project, Multi-core Rendering OFF
      (serial-diagnostic): DSP% matches the OLD behavior (single-thread wall-clock — the
      audio thread runs the whole graph, so its wall-clock is the total). Toggling the
      mode live flips the reading between the two without a restart. `D:__ R:__` notes:
- [ ] **N-3 — idle/light near-zero, no jitter.** Empty or light project, both modes:
      DSP% sits near zero; no new meter jitter or audio jitter at 128 buffer vs
      pre-batch. `D:__ R:__` notes:

### §B.21 — QA-OctavePedal (octave engine fix + poly grain + pedal UI + PDC pull + Inst monitor)

`blocks:` `d6abc38b` (QA-OctavePedal, the whole batch in one commit). Debug exe FIRST
(screenshot any jassert), then Release — mark each scenario `D:` and `R:`.
Background: the OC-Style octave pedal (Polyphonic mode) "rang like a broken bell" — the shipped
period-doubler was free-running period-length OLA that, on a stationary tone, reconstructs the
INPUT pitch (an identity), so its -1/-2 voices never actually shifted; the octave content was
seam clicks + period drift. Rebuilt as a mark-anchored 1/N-speed pitch-synchronous shifter.
Setup: an Inst page (Guitars/Basses or a live DI on an Inst strip) with the OC-Style octave
pedal loaded in BaySickPedals; Polyphonic mode unless noted.

- [ ] **OP-1 — bell is dead (octave -1 held note).** Play a held single note (low-mid guitar
      range) with -1 Oct up and Direct at 0 (effect-only). The octave-down is a clean note one
      octave below — NO metallic ring, NO comb/hollow coloration, and no slow beat against the
      dry note when you raise Direct. Sweep a few pitches across the range. `D:__ R:__` notes:
- [ ] **OP-2 — glides + high notes (no snap / no beat).** Under -1 Oct, play a legato slide up
      and down: the octave tracks smoothly with NO click/snap at the turnaround. High-register
      single notes: no amplitude "beating"/tremolo on the sustained octave. `D:__ R:__` notes:
- [ ] **OP-3 — real polyphonic chord tracking.** Hold a chord (3-6 notes) in Polyphonic mode:
      the octave-down follows the chord (sized off the lowest note) — audibly tighter/less
      warbly than a single-note-only engine would manage on a chord. Mono single-note lines
      still sound exactly as in OP-1 (mono behavior preserved). Works on guitar AND bass range
      (auto — no instrument selector). `D:__ R:__` notes:
- [ ] **OP-4 — pedal-mode tile layout.** Open BaySickPedals with the octave in a slot: all five
      knobs (Direct / +1 / -1 / -2 / Range) AND the mode chickenhead are visible, finger-sized,
      NO overlap, no dBFS strip. Then load the octave into the FX Rack (Effects page): the rack
      view is the original horizontal layout, unchanged. `D:__ R:__` notes:
- [ ] **OP-5 — PDC latency (LAT readout + bounce alignment).** On an Inst strip: loading drive
      pedals (distortion/fuzz/overdrive) nudges the transport LAT readout UP (~1 ms class for a
      full board); bypassing a drive pedal mid-play drops it back and re-aligns within ~200 ms
      (ONE soft click is the documented PDC cost, allowed). Delay/chorus pedals do NOT move the
      readout (their wet delay is the effect, not latency). With the octave active + Direct at 0,
      bounce/export a riff: the octave-down lands ON the grid vs a no-pedal bounce. NOTE (by
      design, Jeff 2026-07-18): with Direct RAISED (dry blended in) the dry portion sits ~11 ms
      early — that is expected, not a bug (dry is left instant). `D:__ R:__` notes:
- [ ] **OP-6 — Inst monitor Dry vs With Effect.** Right-click an Inst strip's Listen LED: menu
      shows "Dry" and "With Effect" (default = With Effect on a new strip). With Effect monitors
      the processed tone; Dry monitors the raw input. They're audibly distinct; the flip is
      click-free. With Effect through the active octave feels ~low-latency (~11 ms, NOT a ~50 ms
      slapback). A recorded take sounds identical whichever monitor mode was used while tracking
      (monitor-only fork; recording is the raw DI). `D:__ R:__` notes:

### §B.22 — QA-SlideSliceGlide (note-type slides + note properties + Builder tiling/slice; sfizz slide DEFERRED)

`blocks:` (backfill the QA-SlideSliceGlide close commit hash at commit). Debug exe FIRST
(screenshot any jassert), then Release — mark each scenario `D:` and `R:`.
**Supersedes** the stale §B.14 H-2 / H-3 / H-4 / H-5 (this batch redid the slide DSP + Note
Properties popup) and the §B.13 G-8 / G-9 / G-10 / G-11 (tiling + slice re-done as content-length
tiling + finite-segment slice + mid-note clamp-and-play) — re-verify the current behavior HERE.
Setup for slides: a Layers or Bass tab on an in-house engine (BaySickSynth / BaySickBass / Harmless /
BaySickPlayer) — the slide fixes are on those four. sfizz Inst engines (Guitars/Basses) are DEFERRED
(SS-A).

- [ ] **SS-1 — co-start source (S-1).** Draw a base note, then an RT Slide (also try Porta and RP)
      note STARTING AT THE SAME beat at a different pitch. Pre-batch: same-start did nothing. Now the
      slide finds the co-starting base as its source and glides from it. All 4 in-house families.
      `D:__ R:__` notes:
- [ ] **SS-2 — RT: no twin voice (S-3/S-5).** Base note A, then an RT Slide note B overlapping A.
      Pre-batch: A kept ringing UNDER B (two voices). Now A is CUT at B's start; ONE voice — a fresh
      attack at A's pitch gliding to B over B's full length. `D:__ R:__` notes:
- [ ] **SS-3 — Porta Length in Beats (S-4/S-10).** A Porta note after a previous note. Set its "Porta
      Length" box in the popup (e.g. 2): the glide spans ~2 beats REGARDLESS of the note's own length
      (a 1/16 Porta note still glides over 2 beats). Default = 1 beat. Pre-batch Porta was a near-
      instant ~60 ms snap. `D:__ R:__` notes:
- [ ] **SS-4 — RP takeover + expression + loudness (S-2/S-6).** Base A held, RP Slide B overlapping/
      butt-joined: no re-attack, A bends to B over B's length (as QA-H). NEW: set B's Panning / Filter
      Cutoff / Resonance in the popup — the takeover now REFLECTS them (pre-batch RP emitted only
      pitch). Set B's Velocity different from A's: the sustained note's loudness RAMPS from A's to B's
      over the glide. `D:__ R:__` notes:
- [ ] **SS-5 — app-wide panning (S-7).** A plain (non-slide) note with Panning hard-left in the popup
      plays hard-left on BaySickSynth, BaySickBass, Harmless, AND BaySickPlayer. Pre-batch panning did
      NOTHING (CC10 emitted, no voice consumed it). A centered note is unchanged (bit-identical).
      `D:__ R:__` notes:
- [ ] **SS-6 — double-click-to-default (S-8).** In the popup, double-click each of the 6 sliders:
      snaps to its neutral default (Velocity 80, Release 50, Fine Pitch 0, Panning 0, Filter Cutoff 50,
      Resonance 50). Double-click the Porta Length box: back to 1. `D:__ R:__` notes:
- [ ] **SS-7 — Porta greying + Close (S-9/S-10).** The popup has a "Porta Length" type-in box at the
      bottom, GREYED/disabled unless the note type is Porta (click Porta -> enables live; Flat ->
      greys). A "Close" button dismisses the popup and commits the single-undo edit (same as
      click-away). Popup opened + closed untouched adds NO undo entry. `D:__ R:__` notes:
- [ ] **SS-8 — content-length tiling (B-1).** An >4-bar pattern (e.g. 8 bars of notes) dropped on the
      Builder grid PREVIEWS and PLAYS its full 8-bar length in song mode, looping at 8 (pre-batch:
      chopped to the 4-bar default). A 2-bar pattern loops every 2. A stretched block tiles the REAL
      content. `D:__ R:__` notes:
- [ ] **SS-9 — roll slice = finite segment (B-2).** Piano roll, Slice tool: drag a short line across a
      couple of notes. ONLY notes the drawn segment crosses (between the two dots) are cut — pre-batch
      an infinite line cut notes far above/below the stroke. `D:__ R:__` notes:
- [ ] **SS-10 — roll Shift-snap (B-5).** Slice tool, hold Shift while dragging: the line forces
      VERTICAL at the snap-div-snapped X under the cursor; release cuts every note that vertical line
      crosses at that beat. `D:__ R:__` notes:
- [ ] **SS-11 — Builder slice drag-line + short blocks + seam + Shift (B-4/B-5).** Builder Slice tool:
      DRAG a line across one or more track rows (two-dot preview) and release — each block the line
      crosses splits at the line's X, with a visible SEAM at the cut. Short blocks (previously
      un-sliceable) now slice. Shift = vertical snapped cut. A plain click still slices the block under
      the cursor. `D:__ R:__` notes:
- [ ] **SS-12 — mid-note slice plays, NO copy (B-3).** Stretch/tile a pattern block so a note straddles
      a slice boundary; slice mid-note. The right piece plays the note's FRAGMENT from the boundary
      (pre-batch the straddling note vanished). No pattern copy: edit the source pattern and BOTH pieces
      reflect the edit. `D:__ R:__` notes:
- [ ] **SS-A — sfizz slides DEFERRED (A-1).** *(SUPERSEDED by §B.23 — the sfizz slide is now REAL as of
      QA-SlideSampler; verify the blended slide + Bend there, not here.)* `D:__ R:__` notes:

### §B.23 — QA-SlideSampler (blended multi-sample slide + native Bend + engine-aware note-props for Guitars/Basses)

`blocks:` (backfill the QA-SlideSampler close commit hash at commit). Debug exe FIRST (screenshot any
jassert), then Release — mark each scenario `D:` and `R:`. **Supersedes** §B.22 SS-A (the sfizz slide is
now real). Setup: a **BaySickGuitars** and a **BaySickBasses** Inst tab, each on its default keyswitch
patch (Guitars `01-green_keyswitch`, Basses `01-darkblack_keysw`).
**SS-Q5 tuning:** dial the perceptual knobs by ear during SLS-1/2/5/6 per the SS-Q5 TUNING CHECKLIST in
`Plans & Specs/Running Notes/silky-gliding-lynx.md` (grep `SS-Q5 TUNE` in Source/).

- [ ] **SLS-1 — RP Slide sounds, mono, any direction.** Draw a base note, then an RP Slide note at a
      different pitch — try UP and try DOWN. It plays a continuous blended slide from real samples (no
      chipmunk, no silence). Both directions read as one moving voice. `D:__ R:__` notes:
- [ ] **SLS-2 — landed-note thinness A/B (SS-Q4=a re-check).** Slide up into a target and let it ring;
      then play that same pitch as a normal (non-slide) note at the same velocity. Judge whether the
      slid-into note is noticeably thinner/weaker. If too thin -> enrich the landing (checklist row 5).
      `D:__ R:__` notes:
- [ ] **SLS-3 — velocity liveness.** The same slide at low / medium / high velocity: the band (timbre)
      AND the loudness change audibly and sensibly. Velocity is NOT inert. `D:__ R:__` notes:
- [ ] **SLS-4 — landing decay + slide-into-held-note.** The landed note rings out naturally (one-shot
      tail), not an abrupt cut. Slide into a target immediately followed by a held normal note at that
      pitch: the SlideSampler-tail -> sfizz-note transition doesn't double or click. `D:__ R:__` notes:
- [ ] **SLS-5 — zone-boundary artifacts (SS-Q5 tune).** A SLOW, WIDE slide across many semitones: listen
      for clicks / combing at the semitone (zone) crossings. Dial `mCrossfadeMs` / `mAttackOffsetMs` by
      ear until clean. `D:__ R:__` notes:
- [ ] **SLS-6 — native Bend plays + shape (SS-Q5 tune).** Draw a note, set type = Bend, pick a semitone
      amount + a shape. The note bends by that amount over its duration: Ramp+Hold rises then holds;
      Ramp(whole) rises across the note; Up+Back bends up then releases; Instant jumps + holds. A "+2"
      bend moves ~2 semitones. Dial the Ramp+Hold rise / Up+Back split by ear. `D:__ R:__` notes:
- [ ] **SLS-7 — Bend range gated per engine (SL-6).** On the GUITAR tab the Bend amount dropdown offers
      UP only (+1/+2/+3, no down). On the BASS tab it offers both (-2..+2). Matches each patch's real
      native range. `D:__ R:__` notes:
- [ ] **SLS-8 — engine-aware note-props (strip-list).** On Guitars/Basses the note-props popup shows ONLY
      Flat / RP Slide / Bend + Velocity + the Bend amount + shape dropdowns + the notice — Pan / Filter
      Cutoff / Resonance / Release / Fine Pitch / Porta box + the in-house RT Slide/Porta buttons are
      GONE. On a Layers/Bass (in-house) roll the FULL original panel is unchanged. `D:__ R:__` notes:
- [ ] **SLS-9 — chord-wide notice.** The always-on notice is present + reads correctly (RP Slide and Bend
      move every playing note together). Hold a chord and Bend/slide: all notes move together (expected
      per the notice, not a bug). `D:__ R:__` notes:
- [ ] **SLS-10 — no crash / no stuck notes or wheel.** Interleave normal notes, RP Slides, and Bends on
      Guitars/Basses (and chains base->A->B): no crash, no stuck notes, the pitch wheel returns to center
      after each Bend. `D:__ R:__` notes:
- [ ] **SLS-11 — decode-at-load, no first-slide latency (option b).** Loading a guitar/bass patch takes a
      touch longer ("Loading Instrument..."); the FIRST slide right after the tab appears plays
      immediately (no silent/late first slide). `D:__ R:__` notes:
- [ ] **SLS-12 — serialization.** Save + reload a project with RP Slide notes and Bend notes (varied
      amount + shape): the note types + Bend amount/shape reload correctly and play the same.
      `D:__ R:__` notes:

### §B.24 — QA-G3Smoke (G3 boundary-smoke 37-defect sweep + voiced SlideSampler + Swing + Cut Self)

`blocks:` (backfill the QA-G3Smoke close commit hash at commit). Debug exe FIRST (screenshot any
jassert), then Release — mark each scenario `D:` and `R:`. **Supersedes §B.23** where the voiced
rework changed behavior: SLS-1 (the slide voice now runs the full patch chain), SLS-3 (velocity =
real velcurve bands + base-note anchor), SLS-4 (landing decay = the patch ampeg release), SLS-5
(hops are time-aligned + fraction-mapped) — re-verify via G3-20..G3-27 below; SLS-2's
landing-thinness A/B decision + the SS-Q5 knob dialing remain live here (G3-22).  Setup: a
BaySickGuitars + a BaySickBasses Inst tab (default keyswitch patches), one synth Layers tab, a
drum tab, a Rusty tab; one project saved mid-suite for the round-trip checks.

Playhead + seek:
- [ ] **G3-1 — flag marker parks exact (#30 fixes + final form).** Stop anywhere: 1-px mast ON the
      position, flag hanging right, no clipping at bar 0.  Check roll, kit view, AND Builder (the
      Builder ruler was the straggler — now the same flag).  `D:__ R:__` notes:
- [ ] **G3-2 — seek snap + Alt free (spec 1A).** Ruler click lands snapped; Alt+click lands exactly
      where clicked.  All three surfaces.  `D:__ R:__` notes:
- [ ] **G3-3 — max-zoom alignment.** Max zoom, snap on, click a bar line: mast ON the line, flag tip
      touching it.  KNOWN non-bug: ~5 px trail while PLAYING at high zoom (30 Hz repaint) — do not
      chase.  `D:__ R:__` notes:

Live-edit playback (the #30b guard — MUST-PASS):
- [ ] **G3-4 — place while playing.** While playing, place roll notes on a synth tab AND an Inst
      tab: they sound on the next pass, no stop/start.  `D:__ R:__` notes:
- [ ] **G3-5 — kit live edits.** While playing, place + delete kit hits: both take effect live.
      `D:__ R:__` notes:
- [ ] **G3-6 — pattern switch mid-play.** Clean swap, no stale notes from the old pattern.
      `D:__ R:__` notes:
- [ ] **G3-7 — Rusty teardown/re-add.** Delete the Rusty tab, re-add, load a kit, place hits — they
      play.  `D:__ R:__` notes:

Piano roll + note types:
- [ ] **G3-8 — S-cycle + Bend + arrows (#9/#10).** On an Inst tab the S-cycle includes Bend
      (Flat/RP/RT/Porta/Bend); Porta + Bend notes show the unified black-fill/white-border
      right-edge arrow.  `D:__ R:__` notes:
- [ ] **G3-9 — RP loudness ramp incl. BaySickPlayer (#37).** RP slide ramps loudness base->slide on
      a synth AND a BaySickPlayer tab (pre-batch the player's deferred CC path dropped the ramp
      handshake).  `D:__ R:__` notes:
- [ ] **G3-10 — RP pan ramp + app-wide declick (#11/G-4 + round 2).** Base panned L, slide panned R:
      pan sweeps over the slide's length (spec: as-designed span).  Sequential plain notes at
      opposite hard pans with overlapping releases: NO clicks (sounding voices glide ~8 ms to a new
      channel pan).  `D:__ R:__` notes:
- [ ] **G3-11 — click memory carries the WHOLE note (#12 + round 2).** Click a note carrying
      pan/cutoff/res/release/fine-pitch/porta/bend; the next click-placed note copies ALL of it
      (plus dur/type/vel).  Grouping + mute do NOT carry.  `D:__ R:__` notes:
- [ ] **G3-12 — double-click defaults (#13/#19).** Note-props sliders double-click to neutral;
      Porta box to 1; Humanize + Riff wheels double-click to their factory values.  `D:__ R:__` notes:
- [ ] **G3-13 — Humanize defaults (#13-#16).** Interval combo defaults 1/64 (offers 1/32-1/128);
      distribution 3-way; seed dropdown 1-10; ranges 10/10/20.  `D:__ R:__` notes:
- [ ] **G3-14 — Riff Machine neutral + enables (#17/#20, defaults revoked).** Fresh project, open
      Riff: ALL step enables ON, Levels wheels 0, Artic "None", Groove "Straight" (the interim
      non-neutral picks are gone).  Dice randomizes + enables; Start over resets to the same
      neutrals; accepted riff survives save/reload (riffUsed pre-checks "Work on existing score").
      `D:__ R:__` notes:

Tracks + Builder:
- [ ] **G3-15 — track ops + undo brackets (#21/#22/#23).** Move a track, group two + color them
      (color = ONE undo step incl. the async picker), move the GROUP: rotation stays sane over the
      whole [lo..hi] span; Ctrl+Z single-steps everything back.  `D:__ R:__` notes:
- [ ] **G3-16 — de-tiled block playback (#24).** A pattern longer than 4 bars on the Builder plays
      its full length ONCE per block — content does NOT repeat/tile past its end; MIDI shading
      matches (incl. Inst/clip/Rusty rolls, NOT Vox).  `D:__ R:__` notes:
- [ ] **G3-17 — slice true fragments (#25/#26).** Slicing a long block yields pieces that play
      their REAL fragment (no modulo wrap); short blocks sliceable; visible seam.  `D:__ R:__` notes:
- [ ] **G3-18 — Builder click memory (#26/#27).** Draw a block, resize it, draw another: the new
      block takes the last length/offset; browser-pick resets the memory; delta-snap move keeps
      relative offsets (snap applies to the DRAGGED block's delta, not each block).  `D:__ R:__` notes:
- [ ] **G3-19 — Alt gestures (#45 spec + KeyBindings rows).** LEFT-Alt+drag a block = fine move, no
      snap (and Alt+drag a roll note likewise); RIGHT-Alt+click = mute toggle; Alt+RClick still
      opens the quantize popup.  `D:__ R:__` notes:

Guitar/bass slides (voiced rework — supersedes §B.23 SLS-1/3/4/5):
- [ ] **G3-20 — voiced slide, both directions.** One continuous voice through the full patch chain
      (velcurve gain, AHDSR, LFOs, unison, bass filter), no chipmunk.  `D:__ R:__` notes:
- [ ] **G3-21 — long UP slide clean (#6 + smoke #24/25/28).** A long up-slide: no stutter toward the
      end, lands and RINGS OUT (fraction-mapped hops keep >= 20% body).  Down likewise.
      `D:__ R:__` notes:
- [ ] **G3-22 — landing-thinness A/B (THE decision) + SS-Q5 dial.** Slide into a target, ring, vs
      the same pitch played normally at the same velocity.  Too thin -> the enrich-the-landing
      decision opens; else dial `mCrossfadeMs`/`mAttackOffsetMs` (grep SS-Q5 TUNE) by ear.
      `D:__ R:__` notes:
- [ ] **G3-23 — keyswitch timbre (#2).** Press a keyswitch (e.g. palm mute), slide: the slide plays
      that articulation's samples; switch back, slide again.  Keyswitch presses are cut-self-exempt.
      `D:__ R:__` notes:
- [ ] **G3-24 — live CC voicing (12b/12c).** During a held slide: vibrato/wobble knobs modulate in
      real time; cc118 up = next slide plays the tailpiece table; cc29 up = feedback + noise layers
      ride the knob and hop with the slide; bass swell CC shapes the attack; bass filter
      EG/wah CCs shape tone (wah needs BOTH its CCs up — they multiply).  `D:__ R:__` notes:
- [ ] **G3-25 — copied slide re-plucks (#7/#36).** Copy a slide butt-joined against the original:
      the copy re-triggers as its own gesture (no silent continuation), chains still glide
      continuously.  `D:__ R:__` notes:
- [ ] **G3-26 — anchor-velocity loudness (#3).** A bottom-velocity base note yields a near-silent
      slide; a hot base yields a hot slide (CC86 only covers no-prior-noteOn).  `D:__ R:__` notes:
- [ ] **G3-27 — panic + wheel (#1/#5).** Stop mid-slide: tails die ~7 ms.  After any Bend the wheel
      returns to center; bend shapes scale to the patch's REAL range (wheel convention fixed).
      `D:__ R:__` notes:
- [ ] **G3-28 — Cut Self (G-12/13/14).** Title-bar CUT SELF + SAME PITCH/CUT ALL toggles: ON+Same
      Pitch cuts only the retriggered pitch; CUT ALL sweeps everything; OFF lets slide tails ring
      under the next note.  Mode button swaps text; state survives save/reload.  `D:__ R:__` notes:
- [ ] **G3-29 — bass Mono choke (cc105).** With the bass Mono CC up, a new slide gesture chokes the
      old over ~0.2 s.  `D:__ R:__` notes:
- [ ] **G3-30 — idle tail (idle-suspend term).** ONE slide on an otherwise idle Inst tab, hands off:
      the tail rings to completion, never freezes.  `D:__ R:__` notes:

Drums:
- [ ] **G3-31 — kit move-drag re-stamp (#34).** Drag hits from one drum's row to another's: they
      play the DESTINATION drum (a deliberately re-pitched hit keeps its pitch).  `D:__ R:__` notes:
- [ ] **G3-32 — recording demux (#32).** Record a performance with per-drum MIDI triggers: hits land
      on each drum's own lane, kit view refreshes, playback matches the take.  `D:__ R:__` notes:
- [ ] **G3-33 — song-mode roll playhead (#31).** In song mode the roll/kit playhead tracks the
      pattern-LOCAL beat inside each block (not the absolute song beat).  `D:__ R:__` notes:

Engine + transport:
- [ ] **G3-34 — octave pedal all modes (#35 + smoke #55).** Po mode: -1 Oct clean on notes AND
      chords (no loop-start click, no bell/AM); -2 produces sound (no dropout/clicks); +1 clean and
      gain-stable (anchored half-grain pair; the live-sum divide is gone).  `D:__ R:__` notes:
- [ ] **G3-35 — no stuck audition notes (round 2).** Rapidly drag notes across many pitches on every
      in-house engine: nothing ever hangs ringing (off-mask drain).  `D:__ R:__` notes:
- [ ] **G3-36 — bar-1 notes every pass (Gen-1 watch).** Loop a pattern with bar-1 notes + from-stop
      starts: the first notes sound on EVERY pass.  (Scheduler exonerated by [G3 BAR1]; recurrence
      -> engine side, re-open with the Debug log.)  `D:__ R:__` notes:
- [ ] **G3-37 — automation song-only + restore (round 3).** Pattern mode: an automation clip at bar
      0 drives NOTHING (test BOTH a main-APVTS target and an engine-panel knob).  Song mode: both
      driven over the clip span.  Switch back to pattern: both knobs return to their pre-song
      values (incl. mixer faders).  `D:__ R:__` notes:
- [ ] **G3-38 — Swing SW-1..SW-6.** Transport knob ~50%: off-16ths push late on synth/bass/drum
      rolls.  Per-player Swing Mix knobs live on the PAGE BAR right of FX Rack (visible on every
      sub-tab; Rusty right of its tab cluster): mix 0 isolates a player; double-click = 1.0;
      hover shows the value bubble; right-click = Truncate (colliding same-pitch swung note clips,
      no doubling).  Vox NOT swung; audio-clip EVENTS follow the global knob; direct-dropped
      Builder wavs do NOT swing; swing 0 + a 7/8 project = timing identical.  `D:__ R:__` notes:
- [ ] **G3-39 — Rusty page flow (G-16 + rounds 1/2).** The page OPENS on the Player sub-tab; Load
      Player (Full/Basic) + Player Preset sit on the panel title bar; picking a program loads it;
      re-add after delete auto-reloads the last kit.  `D:__ R:__` notes:
- [ ] **G3-40 — NAM A/B placement (G-16 NAM).** The A/B slot toggles sit BELOW the OS dial on the
      NAM/IR editor (off the title bar).  `D:__ R:__` notes:
- [ ] **G3-41 — round trip.** Save + reload the suite project: note types + bend amount/shape +
      cut-self state + swing settings + accepted-riff flag + per-drum triggers all reload and play
      the same.  `D:__ R:__` notes:

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
