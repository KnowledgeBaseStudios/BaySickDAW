# Keyboard Shortcuts

**Purpose** - This is the complete keyboard and mouse reference for BaySickDAW.
Some shortcuts are **rebindable** - you can change them in the app. The rest are
**page-local**: single letters and mouse gestures that belong to a particular
editor (the Builder grid, the Piano Roll, the Drum Kit, the vocal editors, the
Event Editor) and are fixed. Everything below is also visible inside the app
under **Help > Key Binds...**, with a plain-English tooltip on every row.

---

## How it operates

`Source/Standalone/KeyBindings.cpp` holds one catalog with two halves:

* **`getAllCommands()`** - 36 rebindable commands (35 filed under General, 1
  under Builder). Each has a stable numeric id, a category, a display name, a
  beginner tooltip and a default key.
* **`getMouseRefRows()`** - display-only reference rows: mouse gestures and the
  page-local keys that individual editors handle themselves. These are shown
  dimmed with no Set/Reset buttons because they are not rebindable.

`StandaloneEditor` is the single `ApplicationCommandTarget`. It registers every
catalog id, and `perform()` is one switch that dispatches each to its handler -
transport methods on `GlobalTransportBar`, page switches, file operations,
pattern navigation, undo/redo, the Builder edit-mode toggle and the typing
keyboard.

**Dispatch order matters.** A contained window is its own desktop component, so
each one registers the key-mapping set as a listener itself; without that, every
global binding would be dead inside every window. JUCE runs a component's key
listeners in **reverse** registration order, so the mapping set is registered
first and the typing-keyboard note gate last - the bare note letters have to
outrank the letter commands they collide with. The editor is also pinned as the
command manager's first target, because otherwise the manager would walk up from
the focused component, find no target inside a page window, and silently drop
every global binding.

**Page-local keys win.** An editor that handles a key itself (the Piano Roll's
`P`, `B`, `C` tool letters, `Ctrl+G` glue, and so on) sees it before the command
system does. `findHardcodedConflicts` is what makes this visible: when you assign
a new binding in the Key Binds window it checks the reference rows and warns you
that the binding will only fire when those pages are not in focus.

**Persistence.** Rebinds are written to `keymap.xml` in
`Documents\BaySickDAW\`, next to `audio_settings.xml`, immediately on every
change. At startup the defaults are applied first and the saved file is layered
on top. If the write fails the app tells you so, because otherwise a silent
failure reads as "I must have forgotten to save".

---

## User-facing behavior

### The Key Binds window

**Help > Key Binds...** opens a resizable window with six tabs:

| Tab | Contents |
|---|---|
| General Key Binds | Every rebindable command except one, plus two global mouse rows |
| Builder Page Key Binds | The Slip/Stretch toggle (rebindable) plus the Builder's own tool letters, edit keys and mouse gestures |
| Piano Roll Key Binds | The Piano Roll's own keys and gestures |
| Drum Kit Key Binds | The Drum Kit grid's own keys and gestures (documented in full, so you never have to cross-reference the Piano Roll tab) |
| Vocal Editor Key Binds | BaySickPitch and BaySickAlign gestures |
| Event Editor Key Binds | The automation Event Editor's keys |

Each row shows the action name and its current shortcut. Hovering a row gives a
plain-English explanation of what it does. Rebindable rows have two buttons:

* **Set** - opens a small "Press a key combination for: *action*" prompt. Press
  the combination you want, or Escape to cancel. Setting a shortcut **replaces**
  the command's existing one rather than adding a second.
* **Reset** - puts that one command back to its factory default.

If the key you press is already assigned to another command, you are asked
whether to replace it. If it is a page-local key, you are told which page owns it
and warned that your binding will only work while that page is not in focus - you
can still use it anyway.

Reference rows are dimmed and have no buttons. A thin line separates the
rebindable rows from the reference rows in each tab.

---

## The rebindable commands

### Transport

| Action | Default | What it does |
|---|---|---|
| Start / Stop Playback | `Space` | Press to start, press again to pause. |
| Stop and Disarm Record | `Shift + Space` | Stops playback and clears record-arm. |
| Toggle Recording | `R` | Arms or disarms recording. While playback is running this switches between recording and just monitoring. |
| Pattern / Song Mode Toggle | `L` | Switches between looping the active pattern and playing the Builder arrangement. |
| Seek Playhead to Start | `Home` | Sends the playhead to bar 1. Works in both modes. |
| Fast Forward (4 bars) | `Numpad 0` | Skips 4 bars forward per press. |
| Previous Bar (Song mode) | `/` | Moves the playhead one bar back. Song mode only. |
| Next Bar (Song mode) | `*` | Moves the playhead one bar forward. Song mode only. |
| Toggle Metronome | `Ctrl + M` | Turns the click on or off. |
| Toggle Recording Precount | `Ctrl + P` | When recording is armed, plays a one-bar lead-in click before recording starts. |
| Toggle Typing Keyboard (MIDI) | `Ctrl + T` | Play the active tab's instrument from your computer keys. |

### Page switches

| Action | Default | What it does |
|---|---|---|
| Show Builder | `F5` | Switches to the Builder page. |
| Show Mixer | `F6` | Switches to the Mixer page. |
| Show Player (Most Recent) | `F7` | Goes to the instrument tab you were last on - Layers, Bass, Drums, Clip, Vox, Inst or Plugins. Falls back to the first one you have open; does nothing when you have none. |
| Show Effects Rack (Most Recent) | `F8` | Switches to the Effects page, which comes back up on the channel you last had open there. |
| Show Effect Panel (Most Recent) | `F9` | Brings the single-effect panel you last opened back to the front. Does nothing until you have opened one, and nothing once you have closed it. |
| Show Piano Roll (Most Recent) | `F10` | Switches to the unified Piano Roll page on whichever engine its own dropdown is set to. That choice is saved with the project. |
| Show Drum Kit (Most Recent) | `F11` | Switches to the Piano Roll page and shows the 16-pad Drum Kit grid. |
| Show Event Editor (Most Recent) | `F12` | Brings the Event Editor you last opened back to the front. Does nothing until you have opened one, and nothing once you have closed it. |

### File

| Action | Default | What it does |
|---|---|---|
| New Project | `Ctrl + N` | Closes the current project (prompting to save) and opens a fresh one from your default template, or empty if none is set. |
| Open Project | `Ctrl + O` | Opens an existing project from disk. |
| Save Project | `Ctrl + S` | Saves to the project's existing folder. A first-time save behaves like Save As. |
| Save Project As | `Ctrl + Shift + S` | Saves to a new folder of your choosing. |
| Export Audio | `Ctrl + R` | Renders the whole arrangement to an audio file (WAV, OGG or MP3). |

### Patterns

| Action | Default | What it does |
|---|---|---|
| Rename / Color | `F2` | One dialog holding both the name and the color of the selected pattern. OK applies both, Cancel applies neither. |
| Find Next Empty Pattern | `F3` | Jumps the pattern dropdown to the first pattern with no notes in it. |
| New Pattern | `F4` | Creates a new empty pattern and selects it. |
| Next Pattern | `+` | Cycles the pattern dropdown forward, wrapping at the end. |
| Previous Pattern | `-` | Cycles backward, wrapping at the start. |
| Insert Pattern | `Ctrl + Shift + Insert` | Adds a new empty pattern directly after the selected one. Later patterns shift down a slot and everything pointing at them follows. |
| Clone Pattern | `Alt + C` | Makes a full copy of the selected pattern, notes included, and selects the copy. |
| Move Pattern Up | `Ctrl + Shift + Up` | Swaps the selected pattern with the one above it. |
| Move Pattern Down | `Ctrl + Shift + Down` | Swaps it with the one below. |

There is deliberately **no** Delete Pattern shortcut: bare Delete is claimed by
the Builder grid, the Piano Roll, the Drum Kit, the Event Editor and the pitch
editor for their own selections, so pattern delete stays menu-only.

### Edit

| Action | Default | What it does |
|---|---|---|
| Undo | `Ctrl + Z` | Steps backward through recent edits - note moves, pattern changes, mixer tweaks, all of it, in one shared history. |
| Redo | `Ctrl + Alt + Z` | Re-applies the edit you just undid. |

### Builder (rebindable)

| Action | Default | What it does |
|---|---|---|
| Toggle Slip/Stretch Editing | `S` | Flips the Builder toolbar's Slip/Stretch dropdown. **Slip:** dragging an audio clip's edge reveals pre-roll or trims leading/trailing audio. **Stretch:** dragging its right edge resizes it (time-stretching from the left edge ships later). Pattern and Automation blocks ignore this mode. |

Note that `S` is also the Builder's page-local Slip Edit Tool letter, so on the
Builder page the tool letter is what fires.

---

## Page-local keys and mouse gestures (not rebindable)

### Everywhere

| Gesture | Effect |
|---|---|
| Mouse Wheel | Scrolls the view under the cursor up and down |
| Shift + Wheel | Scrolls left and right |

### The typing keyboard

With **Ctrl+T** on, your computer keyboard plays the active tab's instrument.

* Bottom row `Z X C V B N M` is one octave of white keys; `S D G H J` are its
  black keys.
* Top row `Q W E R T Y U I O P` is the octave above; `2 3 5 6 7 9 0` are its
  black keys.
* `Page Up` / `Page Down` shift the octave (range -5 to +3 octaves from middle).
* Notes record exactly like a hardware MIDI keyboard.

Only **bare** keys become notes. Anything with Ctrl, Alt or Shift held is passed
through as a normal shortcut, so Ctrl+Z still undoes while the typing keyboard is
on. Turning the mode off, or switching tabs, releases anything still held.

### Builder page

**Tools**

| Key | Tool |
|---|---|
| `P` | Draw - click to place a single clip. The clip type follows the last browser entry you clicked. |
| `B` | Paint - drag to place identical clips along a row. |
| `C` | Slice - click a clip to split it in two at that point. |
| `D` | Delete - click clips to remove them. |
| `E` | Select - click and drag selection rectangles. |
| `S` | Slip Edit - drag a clip's contents inside its bounds without moving the clip. |
| `T` | Mute - click clips to mute them; they stay in place but make no sound. |
| `Y` | Playback - click a clip to hear it once from the start. |
| `Z` | Zoom - drag to zoom into a region, right-click to zoom out. While active, PgUp/PgDn zoom. |

**Edit keys**

| Key | Effect |
|---|---|
| `Ctrl + A` | Select every clip |
| `Ctrl + B` | Duplicate the selection immediately after itself (with nothing selected, advances to the next step) |
| `Ctrl + C` / `Ctrl + V` | Copy selection / paste at the playhead |
| `Ctrl + Shift + 1..6` | Snap horizontal zoom to one of six presets |
| `Delete` / `Backspace` | Delete the selected clips |
| `Ctrl + Delete` / `Ctrl + Backspace` | Delete a time region: removes every clip starting inside the highlighted span and slides later clips left to close the gap. Uses the ruler Ctrl-drag range if you have one, otherwise the span of your selection. |
| `Ctrl + Left` / `Ctrl + Right` | Slide the highlighted ruler time-selection box by its own length. Clips do not move. Clamps at bar 0. |
| `Escape` | Deselect everything |
| `Shift + Arrows` | Left/Right nudges the selection by one bar; Up/Down moves it between rows |
| `PgUp` / `PgDn` | Scroll one viewport height - or zoom, when the Zoom tool is active |

**Mouse**

| Gesture | Effect |
|---|---|
| Mouse Wheel | Scroll up/down about one row per click |
| Shift + Wheel | Scroll horizontally (wheel up moves right through the timeline) |
| Ctrl + Wheel | Horizontal zoom anchored on the cursor |
| Alt + Wheel | Vertical zoom anchored on the cursor |
| Shift + Alt + Wheel (over a clip) | Nudge the clip by tiny increments |
| Ctrl + Left-Click | Add the clicked clip to the selection |
| Ctrl + Drag (empty area) | Marquee select, whatever tool is active |
| Ctrl + Drag (ruler) | Define a time range, used as the loop region in Song mode |
| Click on ruler | Move the playhead there |
| Right-click on ruler | Ruler menu - time markers and time-signature changes |
| Alt + Drag (clip) | Fine move with grid snap bypassed |
| Alt + Right-Click | Audition the clip once |
| Right-Alt + Right-Click | Quantize options for the selection |
| Right-Alt + Left-Click | Toggle mute on the clicked clip |
| Ctrl + Shift + Right-Click | Zoom so the selected clip fills the view |
| Ctrl + Right-Click + Drag | Drag a rectangle and release to zoom-fit it |
| Shift + Right-Click + Drag | Pan the view |
| Middle-Click + Drag | Pan the view |

**Browser panel**

| Gesture | Effect |
|---|---|
| Right-click a Clips / Vox / Inst header | Create a group. Vox and Inst recordings auto-group by take; groups made here organize anything else. |
| Right-click a group | Rename it. For a recording group this renames every take file on disk (keeping the Dry / Dry Cleaned / Wet / Wet Cleaned tags) and every clip keeps playing. Stop playback first. |
| Right-click a cleaned take | Regenerate the de-noise at Light or Strong strength from its stored room profile. Stop playback first. |
| Drag the browser's right edge | Widen the panel up to 3x, or back to default. Resets each session. |
| Right-click a Vox strip's Arm LED | Pick the interface input and which recorded take lands on the Builder grid. |

### Piano Roll

**Tools**

| Key | Tool |
|---|---|
| `P` | Draw - click to place a note |
| `B` | Paint - drag to place repeated notes on the grid |
| `C` | Slice - drag a vertical line to slice notes at grid positions (Alt bypasses snap) |
| `D` | Delete |
| `E` | Select |
| `T` | Mute notes |
| `Z` | Zoom |
| `S` | Cycle the armed note type: Flat, RP Slide (the sounding note bends into this pitch, no new attack), RT Slide (new attack gliding in from the previous note's pitch), Portamento. New notes inherit the armed type; with notes selected, `S` also converts them. |
| `M` | Show/hide the on-screen piano keyboard column. Remembered per tab. |

**Edit keys**

| Key | Effect |
|---|---|
| `Ctrl + A` | Select every note |
| `Ctrl + B` | Duplicate the selection immediately after itself |
| `Ctrl + C` / `Ctrl + V` | Copy / paste at the playhead |
| `Ctrl + G` | Glue - merge selected overlapping notes into one |
| `Shift + G` / `Alt + G` | Group the selection so it moves together / ungroup it |
| `Shift + I` | Invert the selection |
| `Delete` / `Backspace` | Delete the selected notes |
| `Ctrl + Delete` / `Ctrl + Backspace` | Delete a time region and close the gap |
| `Shift + Arrows` | Left/Right nudge by one snap unit; Up/Down transpose by a semitone |
| `Alt + Arrows` | Fine nudge by one pixel, no snap |
| `PgUp` / `PgDn` | Zoom, when the Zoom tool is active |
| `Ctrl + Q` | Quick quantize starts to the nearest 1/4 note (selection only, ignores the snap setting) |
| `Ctrl + U` | Quick chop each selected note into four |
| `Ctrl + L` | Quick legato - extend each note to where the next one begins |
| `Ctrl + Up` / `Ctrl + Down` | Transpose the selection an octave |
| `Ctrl + Left` / `Ctrl + Right` | Slide the ruler time-selection box by its own length |
| `Ctrl + Alt + Home` | Flip which edge a note-resize drag grabs (left instead of right) |

**Tool dialogs (Alt + letter)**

| Key | Dialog |
|---|---|
| `Alt + Q` | Quantize |
| `Alt + S` | Strum - stagger selected note starts |
| `Alt + A` | Arpeggiate |
| `Alt + U` | Chop into N equal pieces |
| `Alt + L` | Articulate - apply a curve to the selection |
| `Alt + R` | Randomize - a Pattern section (random notes from an octave/key/scale pool, population, stack, random portamento, merge) and a Levels section (velocity, pan, fine pitch, release, cutoff, resonance with seeded wheels). Live preview; Accept is one undo step. |
| `Alt + E` | Riff Machine - an 8-step generator (Progression, Chords, Arpeggiation, Mirror, Levels, Articulation, Groove, Fit) with per-step enables, preview-to-step, dice and work-on-existing-score mode. Live preview; Accept is one undo step. |
| `Alt + P` | Generate scale-aware chords from selected single notes |
| `Alt + F` | Flam - add a 1/32 grace note before each selected note |
| `Alt + M` | Mute the selection (`Alt + Shift + M` unmutes) |
| `Alt + X` | Scale Levels - a velocity percentage applied to the selection (100% = no change) |

**Mouse**

| Gesture | Effect |
|---|---|
| Mouse Wheel | Scroll through pitches |
| Shift + Wheel | Scroll through time |
| Ctrl + Wheel | Horizontal zoom anchored on the cursor |
| Alt + Wheel (over the grid) | Vertical zoom anchored on the cursor |
| Double-click a note | Note Properties: type (Flat / RP Slide / RT Slide / Porta), Velocity, Release, Fine Pitch, Panning, Filter Cutoff, Resonance. If the note is part of the selection, edits apply to every selected note. Changes apply live; each popup session is one undo step. |
| Alt + Wheel (over the control lane) | Bump the lane's displayed property for the bar under the cursor by +/-0.05 (Shift+Alt+Wheel for +/-0.01). With notes selected only their bars are targetable, and selected bars paint red. |
| Ctrl + Drag (over the control lane) | Scrub values across the selected notes' bars; the value follows the cursor height. One undo step per sweep. |
| Ctrl + Click a piano key | Select every note at that pitch. Additive; no audition fires. |
| Right-Click + Wheel | Cycle through tools |
| Ctrl + Left-Click | Add a note to the selection |
| Ctrl + Drag (empty area) | Marquee select, whatever tool is active |
| Ctrl + Drag (ruler) | Define the loop region |
| Click on ruler | Move the playhead |
| Right-Click a note | Delete it |
| Alt + Drag a note | Fine move, snap bypassed |

### Drum Kit grid

The 16 rows are fixed, so there is no vertical scroll or zoom.

| Key | Effect |
|---|---|
| `P` `B` `C` `D` `E` `T` | Draw / Paint / Slice / Delete / Select / Mute, on drum hits |
| `Ctrl + A` | Select every hit on every row |
| `Ctrl + B` | Duplicate the selection after itself on the same rows |
| `Ctrl + C` / `Ctrl + V` | Copy / paste at the playhead |
| `Ctrl + G` | Glue overlapping hits on the same row |
| `Ctrl + Q` | Quick quantize to 1/4 |
| `Ctrl + U` | Quick chop into four (pieces smaller than a 1/16 are blocked) |
| `Ctrl + Delete` / `Ctrl + Backspace` | Delete a time region and close the gap |
| `Ctrl + Alt + Home` | Flip the resize edge |
| `Ctrl + Left` / `Ctrl + Right` | Slide the ruler time-selection box |
| `Shift + I` | Invert the selection |
| `Delete` / `Backspace` | Delete the selection |
| `Shift + Arrows` | Left/Right nudge by one snap unit; Up/Down move between drum rows |
| `Alt + Q` `Alt + S` `Alt + U` `Alt + L` `Alt + R` `Alt + F` `Alt + X` | Quantize / Strum / Chop / Articulate / Randomize / Flam (a 1/32 grace hit at 60% velocity) / Scale Levels |

| Gesture | Effect |
|---|---|
| Mouse Wheel, Shift + Wheel | Scroll the timeline left and right |
| Ctrl + Wheel | Horizontal zoom anchored on the cursor |
| Alt + Wheel (over the control lane) | Bump velocity/pan for the bar under the cursor by +/-0.05 (Shift+Alt+Wheel for +/-0.01) |

Deliberately unbound here, with an explanation shown in the app: `M` (keyboard
column - the drum grid uses a sidebar), `S` (note type - drum hits have no
slides), `Ctrl+L` (legato), `Ctrl+Up`/`Ctrl+Down` (transpose - rows are slots,
not pitches), `Alt+A` and `Alt+P` (arpeggiate and chords - both pitch-based).

### Vocal editors (BaySickPitch / BaySickAlign)

| Gesture | Effect |
|---|---|
| Click / drag a note | Click selects the pill; dragging repitches it vertically in 0.1-semitone steps. With Snap on the drag walls onto in-scale lanes but keeps the note's natural cents; Center is what flattens cents. A plain drag never moves a note in time. |
| Ctrl + Drag | Fine 0.01-semitone repitch, bypassing Snap |
| Ctrl + Alt + Drag | Move the note in time; neighbors stretch elastically so the phrase stays continuous, and neighboring pills act as walls |
| Ctrl + Shift + Drag | Detach move - a hard cut past 3 px that ignores the walls so the note can jump anywhere |
| Drag a note edge | Stretch or squeeze that note's timing against its neighbors (grab the first or last 6 px). Ctrl past 3 px detaches the edge. |
| Double-click a note | Open the floating sub-editor: volume and pitch point lanes over the note's waveform, Vib / Frm / Variation knobs, a pill browser and its own Play button |
| Shift + Click | Add or remove the note from the selection. Marquee-drag on empty space also selects; Shift adds. |
| Click in Slice mode | Split a note (snaps to the beat grid; Alt slices anywhere). Slice mode never selects - switch back to Edit for selection gestures. |
| Right-click a note | Note menu: restore to original, snap to semitone, merge the selection, re-pitch to a note, copy/paste edits, open the sub-editor, zoom to selection |
| Ctrl + Right-Drag | Zoom to a rectangle. Plain right-click on empty space returns to the saved view. |
| Middle-Drag | Pan time and pitch together |
| Mouse Wheel | Scroll pitch; Shift scrolls time; Ctrl zooms time; Alt zooms lane height |
| `A` | Toggle playhead auto-scroll |
| `Ctrl + A` / `Ctrl + C` / `Ctrl + V` | Select all / copy the focused note's treatment (pitch offset, curves, knob settings) / paste it onto the selection. Audio never moves - only the edit state transfers. |
| `Ctrl + B` | Copy and paste in one stroke, as a single undo step |
| `Shift + Arrows` | Up/Down +/-0.1 semitone; Left/Right +/-10 ms (elastic rules apply) |
| `Delete` / `Backspace` | Restore the selection to its as-sung state. Notes are analysis segments, not deletable objects. |
| `Escape` | Close the sub-editor if open, otherwise clear the selection |
| `Space` (in the sub-editor) | Preview the open note through its current edits; press again to stop. Works while the main transport is stopped. |
| Click / drag / right-click in a sub-editor lane | Add a point / move a point / delete a point |
| `Ctrl + A`, then right-click the grid | Force every out-of-scale note onto the center of its nearest in-key note. In-key notes keep their natural cents. Needs a non-Chromatic scale; independent of the Snap toggle. |

**BaySickAlign**

| Gesture | Effect |
|---|---|
| Drag on the sync-point strip | Move a sync point's leader or follower time. Arms the RE-ANALYZE badge; the re-match runs at the next stop. |
| Right-click the sync-point strip | Delete the clicked sync point, or generate automatic sync points |
| Drag on the protected strip | Paint a region the aligner must leave alone. Right-click it to toggle Protect Timing / Protect Pitch, or delete it. |

### Event Editor (automation)

| Key | Effect |
|---|---|
| `P` | Pencil - click empty space to add a point, drag a point to move it |
| `B` | Paint - drag to lay a run of points along the curve |
| `D` | Erase - click or drag across points to remove them |
| `I` | Interpolate - smooth the curve between the points you drag across |
| `E` | Select - marquee-drag for group edits |
| `Z` | Zoom - drag a rectangle to zoom the lane |
| `Delete` / `Backspace` | Delete the selected points. Deleting the last point asks whether to remove the whole automation clip. |
| `Ctrl + Z` / `Ctrl + Alt + Z` | Undo / redo - lane edits share the app's one history |
| `Ctrl + A` | Select every point in the lane |
| `Ctrl + M` | Import a `.mid` file's CC lane as automation points |

---

## Parameters and persistence

No APVTS parameters. The shortcut set is a `juce::KeyPressMappingSet` owned by
the editor's `ApplicationCommandManager`.

* **Rebinds** are saved to `Documents\BaySickDAW\keymap.xml`, written
  immediately after each Set or Reset. This is **per machine**, not per project -
  your shortcuts follow you across every project.
* **Nothing else here is stored.** Page-local keys and mouse gestures are
  compiled in.
* At startup the factory defaults are applied first and the saved file is
  overlaid; if the file is missing or fails to parse, the defaults stand.

---

## Lifetime and teardown

The command manager, the mapping set and the catalog live for the whole session;
the catalog itself is a static built on first use. Every contained window
registers the mapping set (first) and the editor's typing-keyboard gate (last) as
key listeners when it is created, and drops them when it is destroyed.

**The Key Binds window** is a self-deleting desktop window: closing it deletes
it, and the editor holds only a `SafePointer` to it. It opens at 880 x 680 and is
resizable. It listens to the mapping set for changes and rebuilds its rows, so
two open surfaces cannot disagree.

The shortcut-capture prompt is the one window in the app that keeps
always-on-top, because it must sit above the very window that launched it while
you press a key, and it lives only for that one gesture.

---

## Cross-references

* **Transport and Playback.md** - what Space, Shift+Space, R, L, Home, Ctrl+M and
  Ctrl+P actually do to the transport, and the typing keyboard's note layout in
  context.
* **Workspace and Windows.md** - F5 through F12 select and front page windows;
  why each window has to register the shortcut set itself.
* **Mixer.md** - F6.
* **Builder Page.md**, **Piano Roll.md**, **Engine Tabs (Layers, Bass, Drums).md**
  (the Drum Kit grid), **Pitch Editor.md**, **Align Editor.md** and
  **Event Editor.md** - what each page-local gesture does to that editor's
  content.

---

## Differs from Carry-Forward

Carry-Forward (2026-05-07) has no keyboard-shortcut section, so there is nothing
to reconcile.
