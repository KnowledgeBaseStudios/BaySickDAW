# QA-TransportDisplay — Transport-Bar Position Readout + D-4 Typing-Keyboard MIDI — Plan (punctual-gliding-otter)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/punctual-gliding-otter.md` (mirrored
> at G1 group approval; home-dir copy deleted per convention). **For execution:** BULK-RUN mode
> (swift-stampeding-caribou R1-R5) — no per-task verify pauses; the Verify scripts below author into
> Master Test Plan §B; Work Log entry drafted at code-complete and HELD; one source commit for the batch.

## Context

First batch of the bulk run (G1). Two halves:

1. **Position readout** (Jeff request 2026-07-08): live playback-position display on the 40px
   transport bar — click-to-toggle between time (`M:SS.mmm`) and `bars:beats:ticks` (96 PPQ) — in
   BOTH song and pattern mode. It is the measuring instrument for G1's tempo work and the whole
   campaign, hence first.
2. **D-4 typing-keyboard MIDI** (folded 2026-07-08 at marathon — the 2026-05-08 triage-gap finding):
   play the active tab's engine from the PC keyboard, toggled by a button in the transport bar's
   reserved ~40px slot (next to the metronome) or Ctrl+T. Full hardware-MIDI-keyboard parity
   (records when recording).

- Risk: low-medium — UI-only readout (read-only clock consumer) + a message-thread MIDI injector
  into an existing lock-free queue. No DSP, no audio-thread structural change.
- Effort: ~5-8h (readout ~2-3h, D-4 ~3-5h).
- Dependencies: QA-Ed int64 clock + QA-Ee tick base (both closed). Survives QA-TempoMap unchanged
  (consumes the derived position, not anchor internals).
- **Bucket:** UI / L&F / Theming, Cross-cutting Infrastructure.

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| 17a | Pattern mode shows pattern-relative position (resets each loop pass) | Jeff, marathon. The clock already loop-wraps in pattern mode, so `getCurrentBeat()` is naturally pattern-relative — free. |
| 17b | Beats format = `bars:beats:ticks` (96 PPQ) | Jeff, marathon. |
| 17c | Time format = `M:SS.mmm` | Jeff, marathon. |
| 17d | Display-mode persists app-wide in settings.xml | Jeff, marathon. Clone the MT-pref pattern. |
| 17e | Placement: between pattern dropdown and ribbon | Jeff, marathon. Readout takes width from the ribbon's proportional allotment (ribbon compacts per standing no-expand rule). |
| A1 | Two-row key layout: `Z X C V B N M` = lower octave white keys (+ `S D G H J` black); `Q W E R T Y U I O P` = upper octave (+ number row black) | Jeff, 2026-07-08. |
| A2 | Octave shift on PgUp/PgDn (±1, clamped) | Jeff delegated PgUp/PgDn-vs-arrows; arrows collide with roll navigation/transpose bindings, PgUp/PgDn are free. |
| A3 | Typed-note velocity fixed at 0.8 (matches placed-note default) | Jeff, 2026-07-08. |
| A4 | Full record parity — typed notes record like a hardware MIDI keyboard | Jeff, 2026-07-08. Drives the mechanism choice: inject into `getLiveMidiCollector()` (chord-safe, feeds recorder + on-screen keyboard + active-tab dispatch for free). |
| A5 | Typed notes target the ACTIVE tab's engine | Jeff confirmed. Already how `mLiveMidiTargetKind/Index` works — no new targeting code. |
| — | D-4 slot + Ctrl+T binding | From the shipped reserved slot (GlobalTransportBar.cpp:200/:856) + Final Stretch Work D-4 spec. |

Derived (not new calls): bars/beats display 1-based (bar 1 : beat 1 : tick 0 at start — musical
convention; a "bar 0" would confuse the target audience); song-mode bars use uniform 4 beats/bar
(matches Builder playhead math = the audible grid), pattern-mode beats-per-bar uses the pattern's
tsNum (matches metronome accents) — both per the locked verify criterion "displayed bars line up
with audible downbeats"; typed notes on a Vox/live-Inst tab do nothing (identical to a hardware
keyboard on those tabs today = the parity Jeff asked for).

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open — all resolved at the 2026-07-08 marathon + follow-ups.

## Files to modify

### Task 1 — readout
- `Source/Standalone/GlobalTransportBar.h` (new `PositionReadout` child; decl near `.h:118-157` inventory)
- `Source/Standalone/GlobalTransportBar.cpp` (construct + `resized()`; left cluster ends x≈476, `resized()` at `.cpp:822-867`)
- `Source/Standalone/StandaloneEditor.cpp:9020-9055` (overlay layout: insert readout between pattern dropdown end x≈704 and ribbon `ribX :9041`; shrink `ribW`)
- `Source/Standalone/StandaloneApp.cpp` (settings.xml load/save clone of `load/saveMultiCoreRenderingPref` `.cpp:317-359`)
- Position sources: `StandalonePlayHead::getCurrentBeat()` (`StandaloneApp.h:23`), `mSamplePos`/sample rate for time; pattern tsNum via the existing `onGetTimeSig` chain (`StandaloneEditor.cpp:774-781`)

### Task 2 — D-4 typing keyboard
- `Source/Standalone/GlobalTransportBar.h/.cpp` (new `KeyboardMidiButton` in the reserved slot after `mMetroArrowBtn`, `.cpp:858-859`; consume ~32px of the 476→520 gap)
- `Source/Standalone/KeyBindings.h` (add `cmdToggleTypingKeyboard = 0x10071` — next free ID per `.h:75`)
- `Source/Standalone/KeyBindings.cpp` `buildCatalog()` (CommandInfo, Ctrl+T; pattern at `.cpp:152-155`)
- `Source/Standalone/StandaloneEditor.h/.cpp` (mode state + `perform()` case near `.cpp:7039`; key listener; injection into `mProcessor.getLiveMidiCollector()` — hardware precedent `StandaloneApp.cpp:747-763`; dispatch consumes it at `PluginProcessor.cpp:1813-1867`)

## Tasks

### Task 1 — Position readout

- [ ] `PositionReadout` component (LCD-style label matching the BPM field's amber styling): owns a
      30 Hz `juce::Timer` (the bar's shared 10 Hz timer stays untouched — ms digits need ~30 Hz).
- [ ] Callbacks wired from StandaloneEditor: `onGetBeat` (`mPlayHead.getCurrentBeat()`),
      `onGetTimeSeconds` (`mSamplePos / sampleRate` — sample-derived so time and beats always agree),
      `onGetSongMode`, `onGetPatternTsNum` (reuse the `onGetTimeSig` chain).
- [ ] Formatting: beats mode `B:b:tt` — song: `bar = (int)(beat/4)+1`, `beatInBar = fmod(beat,4)+1`;
      pattern: beats-per-bar = pattern tsNum. Ticks = `(int)llround(fmod(beat,1)*96)` displayed 0-95,
      two digits. Time mode `M:SS.mmm` from seconds. Stopped state shows position at playhead (both
      modes), not blank.
- [ ] Click toggles mode; persist via new `<TransportDisplay mode="time|beats"/>` child in
      settings.xml (clone `load/saveMultiCoreRenderingPref`, preserve siblings). Load at startup.
- [ ] Layout: fixed ~100px (tune to fit `999:4:95` / `99:59.999` in the LCD font) inserted at
      `ribX`; ribbon `ribX += readoutW + 8`, `ribW` shrinks; keep the `ribW > 60` guard.
- [ ] Rule 6 pass on touched regions.

### Task 2 — D-4 typing-keyboard MIDI

- [ ] `KeyboardMidiButton` (piano-icon toggle, lit when active) in the reserved slot; tooltip
      "Play notes with your computer keyboard (Ctrl+T)" (ASCII).
- [ ] Command: `cmdToggleTypingKeyboard` 0x10071 + catalog entry + `perform()` case toggling
      editor-owned `mTypingKeyboardOn` (syncs button state both ways).
- [ ] Key handling: editor-level `juce::KeyListener` on the existing `addKeyListener` path —
      `keyStateChanged` diffs a held-note map against `KeyPress::isKeyCurrentlyDown` for the A1
      mapping. KeyDown → `MidiMessage::noteOn(ch1, note, 0.8f)`, keyUp → matching `noteOff`, both
      `addMessageToQueue`d into `mProcessor.getLiveMidiCollector()` (timestamped like
      `StandaloneApp.cpp:752`). Chords work (queue-based). PgUp/PgDn shift octave ±1 (clamp so the
      full two-row range stays in 0-127); shifting releases held notes first (noteOff all) to avoid
      stuck notes.
- [ ] Guards: mode consumes ONLY mapped keys + PgUp/PgDn; everything else falls through to the
      command system. Bypass entirely when a `juce::TextEditor` has keyboard focus (BPM field,
      rename fields). All held notes get noteOffs on: mode off, app focus loss, tab switch.
- [ ] No new targeting: the collector drain already routes to `mLiveMidiTargetKind/Index` (active
      tab), lights the on-screen keyboard, and feeds `allMidi` → the MIDI recorder (A4 parity free).
      Vox/live-Inst tabs drop notes — hardware parity, documented in the §B section.
- [ ] Rule 6 pass on touched regions.

### Task 3 — batch close (bulk-run shape)

- [ ] Author Master Test Plan §B section "QA-TransportDisplay" from the Verify scripts below
      (`blocks:` = this batch's source commit).
- [ ] Draft Work Log close entry → HOLD under `## Held Work Log entry (apply at section pass)` in
      `Running Notes/punctual-gliding-otter.md`; append code-complete running-notes entry.
- [ ] One source commit (Rule 9): surface message + full git status → Jeff approves → commit.

## Verify scripts (→ Master Test Plan §B; Debug first, then Release)

1. Play in song mode → readout advances smoothly; at bar N's audible downbeat the readout shows
   `N:1:00`; time mode agrees with a stopwatch over ~30 s.
2. Pattern mode with a 4-bar pattern looping → readout runs 1:1:00 → 4:4:95 and wraps to 1:1:00
   exactly at the loop point (pattern-relative per 17a).
3. Click the readout → toggles beats↔time; quit + relaunch → mode remembered (settings.xml).
4. Change BPM mid-play → time and beats stay mutually consistent (no jump in either).
5. Transport bar at the min window width: readout fully legible, ribbon still usable, nothing grew
   past 40px height.
6. Ctrl+T (and the button) toggles typing mode; button lights. With a Layers tab focused, hold
   `Z X C` → 3-note chord sounds and holds; release → stops. PgUp/PgDn shifts octave (held notes
   release cleanly).
7. Number-row/`S D G H J` black keys sound sharps per the two-row layout; keys outside the map
   still do their normal jobs; typing in the BPM field never triggers notes.
8. Record-arm + record: typed notes land in the active tab's piano roll like a hardware keyboard
   (A4); on-screen keyboard lights while typing.
9. Switch tabs while holding notes → no stuck notes; typed notes now play the new tab's engine (A5).
10. Toggle typing mode off mid-hold → all notes release.

## Routing notes (Rule 3 application during execution)

Findings here likely touch GlobalTransportBar (QA-TempoMap's BPM-field interplay — route forward to
its batch), KeyBindings (QA-H's keybind audit surface), or the live-MIDI dispatch (Future State
CL-272 multi-event ring). Log in running notes; route at section pass.

## Carry-Forward Reference touch points

- §1 (transport/clock primitives) before Task 1; §3 (decisions: QA-Ed int-sample clock, QA-Ee ticks)
  before formatting work. The 2026-07-08 surface map (in session transcript + running notes seed)
  supersedes stale line refs.
