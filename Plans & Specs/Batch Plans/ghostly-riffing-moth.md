# QA-H — Builder Polish + Piano Roll Features — Plan (ghostly-riffing-moth)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/ghostly-riffing-moth.md`.
> Paired running notes: `Running Notes/ghostly-riffing-moth.md`.
> **Execution mode: bulk run** (swift-stampeding-caribou R1-R5): no per-task verify pauses;
> §B authored at code-complete; Work Log entry HELD; ONE commit at close; spec calls ASKED.

## Context

The big G3 feature batch. Absorption already removed NAV-05 + folded-#15 (shipped earlier);
scout verified BUILD-06 (stretch rebuild) is MOOT (commitEdit fires rebuildAudioClipPlayers on
resize/stretch/undo — campaign §C retest stands). Docket rounds (2026-07-17) locked the FL
replicas (Humanize screenshot + Randomize/Riff manuals captured in-chat), the Note Properties
system, and re-scoped folded #18 from Jeff's live repro. Slide/porta truth: currently drawn +
saved but DROPPED at playback (emit never reads note.type) — D-8 makes them real.
Risk: medium-high (per-note engine DSP + a large generator build). Effort: ~25-35h — the
largest G3 batch (Riff Machine + Note Properties system dominate).
Dependencies: QA-G (geometry first, per §5).

## Spec calls already locked (G3 docket, 2026-07-17, all Jeff's)

| ID | Decision |
|----|----------|
| #1=B | Humanize AND Randomize both ship, both FL replicas (Humanize per Jeff's screenshot; Randomize rebuilt per the FL manual — Pattern + Levels sections) |
| #2=A | Riff Machine FULL replica per the FL manual (8 steps + per-step enables + global process controls) |
| #3=A | Note Properties: double-left-click note → popup (Normal/Slide/Porta + Velocity, Release, Fine Pitch, Panning + Filter Cutoff, Resonance per D=B); multi-select double-click applies to ALL selected; slide/porta made AUDIBLE in the engines |
| #4i=a | Armed-note-type toolbar button between Select and Zoom; grey on Standard, highlighted on Slide/Porta; "Sel" renamed to full "Select" |
| #4ii | S cycles the button's options (with selection: converts selected notes, per marathon lock) |
| #5=a/a | Lane scrub = Ctrl+drag; affects SELECTED notes' dots only |
| #6 | Re-scoped from Jeff's repro: a MUTED block must NOT shorten song playback length — mute silences, length math still counts it |
| #10/#17 | Not this batch (per-drum MIDI → QA-L; Clips gating dropped) |
| D=B | Per-note RESONANCE added as a real property; MODX→Filter Cutoff, MODY→Resonance in both tools; both fields join the Note Properties popup |
| Marathon 4b | Ghost notes feed from ALL instrument tabs, tinted per source tab's roll color |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open — all locked in the 2026-07-17 G3 docket rounds.

## Files to modify

- `Source/Standalone/PianoRoll.h/.cpp` — S-key handler (:1150-1173), toolbar row (:3468-3488,
  button between Select and Zoom; "Sel" label), ghost plumbing (`setGhostData` :988, render
  :2160-2179, View toggle id 56), ControlLane (:2412-2685; scrub + horizontal guides +
  kModeNames FilterCutoff header bug :2515-2523), Tools menu (:4148-4184; Humanize/Randomize/
  Riff entries), double-click note hook, keyboard Ctrl+click (:272-277)
- `Source/Standalone/PianoRollPage.h/.cpp` — ghost producer over `mConns` (skip `mActive`,
  `dataAccessor()` + `conn.noteColor` → active roll)
- `Source/PatternManager.h/.cpp` — PianoNote gains `release` + `resonance` (serialize);
  song-length math counts muted blocks (#6)
- `Source/PluginProcessor.cpp` — `emitPianoNoteOn` (:49-85): emit release/resonance (CC line
  per engine mapping) + note.type consumption; per-engine glide for Slide/Porta (BaySickSynth/
  Bass mono `_glide` precedent :4841; Harmless; BaySickPlayer)
- Engine processors — consume release + resonance + slide/porta glide (each engine's voice)
- `Source/Standalone/BuilderPage.cpp/.h` — #17 BrowserPanel teardown (member order
  `mAudioTree`/`mAudioRoot` .h:302-303, no dtor), #19 `placeAudioLibraryEntry` silent return
  (:4075-4089) + tree rebuild after delete, #20 active-drop-type (browser `mActiveTab` +
  selection → grid enum next to `mBrowserSelection` .h:711; empty-click places it)
- New: `Source/Standalone/PianoRollTools.h/.cpp` (or in-file) — Humanize / Randomize / Riff
  Machine dialogs; starter pattern-preset set for Riff selectors (bundled assets)

## Tasks

### Task 1 — Note-type button + S key (#4i/#4ii)
- [ ] Toolbar button between Select and Zoom showing armed type; grey Standard / highlighted
      Slide + Porta; click cycles; S cycles the same state; with selection S ALSO converts
      selected notes (undoable); "Sel" → "Select" full label; repaint on arm change (kill the
      silent-arm invisibility).
- [ ] Build-confirm gate + running-notes checkpoint.

### Task 2 — Note Properties system (#3, D=B)
- [ ] Data: PianoNote gains `release` + `resonance` (defaults neutral; serialized).
- [ ] Popup: double-left-click a note → properties dialog (Normal/Slide/Porta selector +
      Velocity, Release, Fine Pitch, Panning, Filter Cutoff, Resonance); double-click on one
      of a multi-selection applies edits to ALL selected; undoable as one edit.
- [ ] Engines: release + resonance consumed audibly (emit path + per-engine voice handling);
      Slide = pitch glide from previous note into this note's pitch over the note; Porta =
      per-engine portamento behavior (mono-glide precedent) — per-engine implementation,
      audibly verified per engine at the campaign.
- [ ] Build-confirm gate + checkpoint.

### Task 3 — Humanize (FL replica, #1)
- [ ] Dialog per Jeff's screenshot: Start Time / Duration / Velocity Range + Offset pairs
      (value box + knob), Distribution (Quasi-Normal), Start Time Max Interval (snap list),
      Seed, Preview toggle, Reset / Regenerate / Accept; operates on selection-or-all;
      undoable. Tools menu entry.
- [ ] Build-confirm gate + checkpoint.

### Task 4 — Randomize rebuild (FL replica, #1)
- [ ] Replace toolRandomize guts with the FL tool: Pattern section (Octave, Range, Key, Scale,
      Length, Variation, Population, Stack, Random Portamento, Merge Same Notes, Seed arrows)
      + Levels section (wheels −100..+100% over velocity/pan/fine-pitch/release/cutoff/
      resonance, Reset Before Processing, Bipolar, Seed arrows) + Reset/Accept; Alt+R opens it.
- [ ] Build-confirm gate + checkpoint.

### Task 5 — Riff Machine (FL replica, #2)
- [ ] 8-step dialog per the manual: Progression, Chords, Arpeggiation (Normal/Flip/Alternate,
      Sync Time/Block/Chord, Gate), Mirror, Levels (PAN/VEL/REL/MODX→cutoff/MODY→resonance/
      PITCH + Bipolar + Seed), Articulation, Groove (quantize modes), Fit (Key/Scale/Snap +
      keyboard range); per-step enable + Reset/Random; global Preview-to-step / Work on
      existing score / Length / Start over / Dice / Accept; notes at current snap length.
- [ ] Starter pattern-preset content authored for Progression/Chord/Arp/Groove selectors.
- [ ] Build-confirm gate + checkpoint.

### Task 6 — Control lane (#5, MIDI-03) + header bug
- [ ] Ctrl+drag scrub: horizontal drag sets the active lane value from cursor Y for SELECTED
      notes' dots as the cursor passes them; single-dot plain drag unchanged.
- [ ] Horizontal reference value guides (+labels) in unipolar modes; bipolar center kept.
- [ ] Fix kModeNames: Filter Cutoff header shows "Control > Filter Cutoff" (not "Pitch Bend").
- [ ] Build-confirm gate + checkpoint.

### Task 7 — Ghost notes producer (4b) + pitch-row select (MIDI-01)
- [ ] PianoRollPage feeds the active roll: iterate `mConns`, skip active, `{dataAccessor(),
      noteColor}` → `setGhostData`; refresh on engine select + pattern edits (timer or
      onNotesChanged); View-menu toggle now does something.
- [ ] Ctrl+click a piano key selects every note at that pitch (additive with Ctrl held).
- [ ] Build-confirm gate + checkpoint.

### Task 8 — Folded Builder fixes (#6 + #17 + #19 + #20)
- [ ] #6: song-length/loop-length math counts muted blocks (mute ≠ length change) — locate the
      length derivation that skips muted blocks (Jeff repro: 2-bar muted block → 1-bar song).
- [ ] #17: BrowserPanel teardown — null the tree's root before member destruction (dtor or
      member reorder); no more shutdown UAF.
- [ ] #19: browser→grid re-drop of a missing/moved file gets feedback (no silent return) and
      deletion rebuilds the tree so stale indices can't resolve wrong.
- [ ] #20: browser click sets the active drop type (pattern / audio clip / automation);
      click on empty grid places that type (piano-roll last-block-type parity).
- [ ] Build-confirm gate + checkpoint.

### Task 9 — Close (bulk run)
- [ ] §B section authored (scenarios below); Work Log entry drafted + HELD; ONE commit
      (message + full git status → Jeff approves).

## Verification (§B-destined scenarios)

1. Note-type button cycles + highlights (grey Standard / lit Slide + Porta); S cycles it; S
   with a selection converts; "Select" label full.
2. Double-click note → all 7 properties edit + stick; multi-select double-click applies to all;
   undo restores.
3. Slide note audibly glides into pitch on every engine that has notes; Porta behaves per
   engine; Release + Resonance audibly change.
4. Humanize dialog matches the screenshot control-for-control; Preview auditions; Accept
   undoable.
5. Randomize matches the manual (Pattern + Levels); seeds reproduce; Alt+R opens it.
6. Riff Machine: each of the 8 steps functions; dice + preview-to-step; Work-on-existing;
   Accept lands notes at snap length.
7. Ctrl+drag across the lane scrubs selected dots only; guides visible; Filter Cutoff header
   correct.
8. Ghosts from other tabs render tinted; toggle hides; Ctrl+click a key selects that pitch row.
9. #6 repro: 2-bar block, song mode, mute it — playback length still 2 bars, silent.
10. Quit with Builder browser populated — no crash; delete a library clip then re-drop it —
    works or explains; browser click then empty-grid click places the picked type.

## Routing notes (Rule 3)

Real bugs fixed in-batch; anything spec-shaped ASKED. Riff/Randomize tuning beyond
manual-parity routes to a Forks entry at section pass, not endless iteration.

## Carry-Forward Reference touch points

- Task 2 engines: audition pattern + APVTS binding sections (per-engine glide precedents).
- Task 8 #6: QA-Ed/QA-TempoMap song-length derivation (do not regress loop/tempo work).
- Scout surface maps: group-open running notes (2026-07-17).
