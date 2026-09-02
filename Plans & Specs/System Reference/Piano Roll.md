# Piano Roll

**Purpose** - The Piano Roll is where you write notes. One page holds every instrument's
roll: a dropdown at the top picks which tab's notes you are looking at, and the grid below
shows those notes as bars laid over a piano keyboard, running left to right in time. The
notes you write belong to the pattern that is currently selected, so the same instrument has
a different set of notes in every pattern.

---

## How it operates

`PianoRollPage` (`Source/Standalone/PianoRollPage.cpp/.h`) is the single owner. It holds one
`PianoRollContainer` per registered instrument plus one `DrumKitContainer`, and shows exactly
one of them at a time. Instruments register themselves through
`PianoRollPage::registerEngine(EngineId, PianoRollConnection)`; the connection carries a
*closure* that returns the note data rather than a raw pointer, so when you switch patterns
the roll follows without anything being re-registered.

`PianoRollContainer` (`Source/Standalone/PianoRoll.cpp/.h`) assembles the visible parts:

| Part | Class | Role |
|---|---|---|
| Menu bar | `PianoRollMenuBar` | Edit / Tools / Scale / Chords / View |
| Toolbar | built inline in the container | Snap, tools, armed note type, undo, zoom |
| Keyboard column | `PianoKeyboard` | Click to audition, shows live incoming MIDI |
| Grid | `PianoRollGrid` | The notes |
| Control lane | `ControlLane` | Per-note velocity / pan / fine pitch / filter cutoff |
| Scrollbars | two `juce::ScrollBar` | Horizontal drives the beat offset, vertical the top note |

The notes themselves are `PianoNote` records inside a `PianoRollData` that lives in the
`Pattern` struct in `PatternManager`. Each instrument family has its own array of rolls
indexed by page - Layers, Bass, Drums, Clips, Vox, Inst, Plugins - plus a single roll for the
big drum-kit engine. Every roll edit runs on the message thread and fires
`onContentEdited`, which makes `PatternManager` republish an immutable snapshot of the roll
table. The audio thread reads only that snapshot, one acquire-load per block; it never walks
a live note vector.

The musical grid is 96 ticks per beat. That number is chosen so every straight *and* triplet
division lands on a whole number of ticks, which is why triplets stay exactly in time instead
of drifting.

Undo works the same way as everywhere else: `beginEdit` snapshots the note list, `commitEdit`
pushes the before-and-after pair into the one app-wide history.

---

## User-facing behavior

### Choosing what you are editing

The window's title strip carries three buttons: an instrument pill (click it for the list of
every open instrument tab, with Drum Kit always first), **Player Page** (jump to that
instrument's own tab), and **FX Rack** (jump to its mixer strip's effects).

Beside them sits the **Swing Mix** knob for the instrument you are on. See "Swing" below.

### The toolbar

| Control | What it does |
|---|---|
| **Snap** | Click for the eleven-entry snap menu. Lit = snapping, dim = Off. Default **Line**. This setting is shared by every piano roll and the Drum Kit. |
| **Draw** (`P`) | Click empty grid to place a note; drag while placing to set its length. Click an existing note and drag to move it; grab near its right edge to resize. |
| **Paint** (`B`) | Drag to lay a run of notes along the grid. |
| **Del** (`D`) | Click or drag to erase notes. |
| **Mute** (`T`) | Click a note to mute it. It stays in the pattern and makes no sound. |
| **Slice** (`C`) | Drag a vertical line; every note it crosses is cut there. Alt cuts at the exact mouse position instead of on the grid. |
| **Select** (`E`) | Drag a rectangle to select; drag a selected note to move the whole selection. |
| **Zoom** (`Z`) | Drag a rectangle to zoom into it; right-click zooms back out. PgUp / PgDn zoom while this tool is active. |
| **Note type** | Shows the type new notes will be. Click it, or press `S`, to cycle Flat / RP Slide / RT Slide / Porta. With notes selected, `S` also converts them. |
| **Undo / Redo / H** | Undo, redo, open the history window. |
| **+ / -** | Zoom in and out. |
| Context label | Right-aligned read-only text, e.g. "Layer 1 - BaySickSolstice". |

A Stamp tool also exists but has no toolbar button - it arms itself when you pick a chord
from the Chords menu.

### Note types

| Type | What you hear |
|---|---|
| **Flat** | A normal note. Its own attack, its own pitch. |
| **RP Slide** (ramp) | No new attack. The note already sounding bends up or down into this note's pitch across this note's length. |
| **RT Slide** (retrigger) | A fresh attack that glides in from the previous note's pitch. |
| **Porta** (portamento) | Glides into pitch over a length you set per note, independent of the note's own length. |
| **Bend** | Offered on the Guitars and Basses instruments only. A real pitch-wheel bend by a set amount over a chosen shape. |

On Guitars and Basses, RP Slide and Bend move every sounding note together - good for a
single-line solo, wrong for bending one note of a chord. The note-properties panel says so on
screen.

### The snap ladder

| Entry | Grid |
|---|---|
| Off | No snapping (nudges still move by a sixteenth) |
| Line | The finest grid line currently visible - zoom in and snap gets finer, down to a sixty-fourth |
| Bar | 1 bar |
| Beat | Quarter note |
| 1/2 Beat | Eighth note |
| 1/3 Beat | Eighth-note triplet |
| Step | Sixteenth note |
| 1/2 Step | Thirty-second note |
| 1/3 Step | Thirty-second triplet |
| 1/4 Step | Sixty-fourth note |
| 1/6 Step | Sixty-fourth triplet |

Holding **Alt** while dragging bypasses snapping for that gesture. The triplet entries have a
matching triplet grid drawn behind them, so a triplet snap target always lands on a line you
can see.

### Note properties

Double-click a note (with Draw or Select armed) to open its properties panel. If the note you
double-clicked is part of the selection, every selected note is edited together.

| Control | Range | Default |
|---|---|---|
| Type buttons | Flat / RP Slide / RT Slide / Porta (Guitars and Basses: Flat / RP Slide / Bend) | - |
| Velocity | 0-100 % | 80 % |
| Release | 0-100 % | 50 % |
| Fine Pitch | -100 to +100 cents | 0 |
| Panning | -100 to +100 % | 0 |
| Filter Cutoff | 0-100 % | 50 % |
| Resonance | 0-100 % | 50 % |
| Porta Length | typed, 0-64 beats | 1 |
| Bend / Shape | Guitars and Basses only. Bend amount is limited to what the patch can actually do; shapes are Ramp + Hold, Ramp (whole), Up + Back, Instant | - |

Velocity is the one control every instrument responds to. The other five sliders and the
Porta box are for the built-in engines; the sampled Guitars and Basses patches do not map
them.

Changes apply as you make them, and the whole popup session counts as one undo step.

### The control lane

The strip under the grid draws one bar per note. Click its header for the mode:

| Mode | What the bar height means | Stored range |
|---|---|---|
| Velocity | How hard the note is played | 0 to 1 |
| Panning | Left / right position | -1 to +1 |
| Pitch Bend | Fine pitch offset in cents | -100 to +100 |
| Filter Cutoff | Per-note filter offset | 0 to 1 |

- Drag a bar to set that note's value.
- **Ctrl + drag** sweeps across the lane, setting every selected note's value from the height
  of your cursor as it passes.
- **Alt + wheel** over a bar bumps it by 0.05; **Shift + Alt + wheel** by 0.01.
- When anything is selected, only selected notes can be edited and their bars paint red. That
  is what stops a drag on a stacked chord grabbing the wrong note.
- Dragging the lane's **header** up and down resizes the lane. A clean click still opens the
  mode menu. The height is shared by every lane in the app, so they all resize together.

There is no separate "bend lane". Pitch Bend is one of the four control-lane modes above and
edits each note's own fine-pitch value; it is not a continuous pitch-wheel curve.

### The keyboard column

Click a key to hear the instrument. Press and hold and the note sustains for as long as you
hold it. **Ctrl + click** a key selects every note at that pitch instead (additive, and no
sound fires). Keys light up while you play a hardware MIDI keyboard. Press `M` to hide or
show the whole column - each roll remembers its own setting.

On the big drum engine every key is drawn white and labeled with the kit piece it plays,
because sharps mean nothing on a drum kit. Sampled instruments that use keyswitches show
those keys highlighted in amber with their labels.

### Menus

**Edit** - Select All (Ctrl+A), Deselect, Copy (Ctrl+C), Paste (Ctrl+V), Delete, Duplicate
(Ctrl+B).

**Tools**:

| Item | Key | What it does |
|---|---|---|
| Quantize | Alt+Q | Pull selected notes onto the Quantize resolution set below. |
| Strum | Alt+S | Stagger the start times of a chord so it sounds strummed. |
| Arpeggiate | Alt+A | Turn held notes into a rolling arpeggio. |
| Chop | Alt+U | Submenu: split each note into 2, 3, 4, 6 or 8 equal pieces. |
| Glue | Ctrl+G | Merge selected overlapping notes into one. |
| Articulate | Alt+L | Taper note lengths and velocities across the selection. |
| Randomize | Alt+R | A dialog that can generate notes from a key and scale, and randomize velocity, pan, fine pitch, release, cutoff and resonance. Live preview, one undo step on Accept. |
| Humanize | - | Nudge timing and velocity off the grid slightly. |
| Riff Machine | Alt+E | An eight-step generator (Progression, Chords, Arpeggiation, Mirror, Levels, Articulation, Groove, Fit) with per-step enables and a live preview. |
| Generate Chords | Alt+P | Build scale-aware chords from selected single notes. |
| Quantize Settings | - | 1/4, 1/8, 1/16 or 1/32. Default 1/4. Separate from Snap, and shared with the Drum Kit. |
| Transpose Up / Down | Shift+Up / Down | One semitone. |
| Transpose Up / Down Octave | Ctrl+Up / Down | Twelve semitones. |

**Scale** - a "Snap to Scale" toggle, a Root submenu (C through B) and a Scale submenu. With
it on, notes you place are pulled onto the nearest note in the key.

**Chords** - pick a chord shape and the Stamp tool arms; clicking the grid then places that
whole chord at the clicked note.

**View** - Zoom In, Zoom Out, Zoom In / Out Vertical, Scroll to Playhead, Ghost Notes
(on/off), Velocity Lane (on/off).

**Ghost notes** are the notes belonging to every *other* instrument, drawn faintly behind
yours in that instrument's own color, so you can line a bass line up against the drums
without switching back and forth.

### Mouse and keyboard

| Gesture | Result |
|---|---|
| Wheel | Scroll up and down the pitch range |
| Shift + wheel | Scroll through time |
| Ctrl + wheel | Horizontal zoom anchored under the cursor |
| Alt + wheel over the grid | Vertical zoom anchored under the cursor |
| Right-click a note | Delete it |
| Right button held + wheel | Cycle tools |
| Ctrl + click a note | Add or remove it from the selection |
| Ctrl + drag on empty space | Marquee select, whatever tool is armed |
| Click the ruler | Move the playhead there |
| Ctrl + drag the ruler | Set a loop range for Pattern mode playback |
| Alt + drag a note | Move with snapping off |
| Shift + arrows | Nudge one snap unit sideways, one semitone up or down |
| Alt + arrows | Nudge one pixel in any direction |
| Shift + G / Alt + G | Group / ungroup the selection (grouped notes move and resize together) |
| Shift + I | Invert the selection |
| Ctrl + Q / Ctrl + U / Ctrl + L | Quick quantize to 1/4, quick chop into 4, quick legato (extend each note to the next note's start) |
| Ctrl + Delete | Delete everything in the highlighted time span and close the gap |
| Ctrl + Left / Right | Slide the ruler time selection by its own length |
| Ctrl + Alt + Home | Flip which edge a resize drag grabs (right edge by default, left edge when on) |
| Alt + M / Alt + Shift + M | Mute / unmute the selection |
| Alt + F | Flam - add a thirty-second grace note before each selected note |
| Alt + X | Scale Levels - scale every selected note's velocity by a percentage |
| Delete | Delete the selection |

Clicking an existing note also primes the "brush": the next note you place inherits that
note's length, type, velocity, pan, fine pitch, cutoff, resonance, release, porta length and
bend settings. Group membership, mute state and drum slot never carry over.

### Swing

Two controls work together:

| Control | Where | Range / default |
|---|---|---|
| **Swing** | Main toolbar, next to the typing-keyboard button | 0-100 %, default 0. Double-click resets to 0. |
| **Swing Mix** | The window title strip, per instrument | 0-100 %, default 100 %. Double-click resets to 100 %. Right-click for "Truncate Swing Notes". |

Global Swing pushes every second sixteenth-note step late. The per-instrument Swing Mix
scales how much of that the instrument follows, so the drums can swing hard while the bass
stays straight. At full global swing and full mix the push is one eighth of a beat - half a
sixteenth.

**Truncate Swing Notes** shortens a pushed note if it now overlaps the next note at the same
pitch, so a swung line does not smear. It only compares notes at the same pitch, so chords
stay intact.

Clips pages always follow the global Swing at full amount and have no Mix knob of their own.
Vox pages are excluded from swing entirely.

---

## Parameters and persistence

| Parameter id | Type | Range / default | What it holds |
|---|---|---|---|
| `Unified_PianoRollSnapDiv` | Int | 0-10, default 1 (Line) | The snap division for every piano roll |
| `Unified_QuantizeDiv` | Int | 0-3, default 0 (1/4) | The Quantize action's resolution, shared with the Drum Kit |
| `globalSwing` | Float | 0-1, default 0 | Global swing amount |
| `swing_layer_<N>_mix`, `swing_bass_<N>_mix`, `swing_drum_<N>_mix`, `swing_inst_<N>_mix`, `swing_plugin_<N>_mix`, `swing_rusty_mix` | Float | 0-1, default 1 | Per-instrument swing follow amount |
| `swing_layer_<N>_trunc` and the matching `_trunc` for each family | Bool | default false | Truncate Swing Notes |

Saved with the **project**:

- Notes, inside `<Patterns>` -> `<Pattern>` -> `<Rolls>`. Rolls are tagged per instrument
  family (`LayerRoll`, `BassRoll`, `DrumPageRoll`, `ClipPageRoll`, `VoxPageRoll`,
  `InstPageRoll`, `PluginPageRoll`, `BaySickRustyDrumsRoll`) with a page index. Each note
  writes single-letter attributes to keep the file small: `m` (pitch), `st` / `dt` (start and
  duration in ticks - the authoritative timebase), `v` (velocity), and optional `p`, `f`,
  `t`, `u`, `g`, `c`, `r`, `q`, `sl`, `pl`, `bs`, `bsh` written only when they differ from
  the default.
- `<ControlLane h= visible=>` inside `<UIState>` - one shared lane height and default
  visibility for every roll.
- `<PianoRollSelection kind index>` - which instrument the page reopens on. `kind` is stored
  as a name, not a number.
- The snap, quantize and swing parameters above ride in the main parameter state.

**Not saved**: the active tool, the armed note type, the keyboard column's visibility, scale
and chord selections, zoom and scroll positions, and the note clipboard (which is shared by
every roll - the last copy anywhere wins).

---

## Lifetime and teardown

`PianoRollPage` is a default page and lives for the session. A `PianoRollContainer` is created
when an instrument registers and destroyed when `unregisterEngine` is called for it, which
happens when the instrument's tab is deleted. Note data outlives the container: it belongs to
the pattern, so deleting a tab and undoing the delete brings the notes back with it.

Two ordering points:

- `PianoRollMenuBar`'s model is declared before the menu-bar component that uses it, so the
  component (whose destructor calls back into the model) dies first.
- The container's rolls hold data through a closure, never a cached pointer, so a pattern
  switch or a project load can swap the underlying data without the roll going stale.

---

## Cross-references

- `Patterns and Arrangement.md` - patterns own the notes; switching pattern switches what the
  roll shows.
- `Builder Page.md` - where a pattern's notes get placed on the timeline.
- `Event Editor.md` - the equivalent editor for a moving control value rather than notes.

---

## Differs from Carry-Forward

Carry-Forward has no Piano Roll section, so there is nothing to reconcile.
