# Event Editor

**Purpose** - The Event Editor is the close-up view of one automation clip. An automation
clip makes a single control - a fader, a knob, the tempo - move on its own while the song
plays. The editor draws that movement as a curve you can shape point by point, and lists
every other automation clip in the project so you can jump between them.

---

## How it operates

`EventEditor` (`Source/Standalone/EventEditor.cpp/.h`) is a floating window, and more than
one can be open at once. Inside it, `EventEditorContent` lays out a menu bar, a control row,
the curve grid (`EEAutomationGrid`) and a list of every automation clip
(`AutomationBrowserPane`).

The editor does not own any data. It binds to one arrangement block by index and edits the
`AutomationLane` living inside it, in `PatternManager`. A lane is a target parameter id, a
list of `ControlPoint` records, and an optional LFO mode. Each control point stores its
position as a fraction of the clip's length (0 to 1), its value as a normalized 0 to 1, a
curve type, and a tension value.

Everything runs on the message thread. Each edit snapshots the lane before and after and
pushes an `AutomationLaneEditAction` into the one app-wide undo history, so Ctrl+Z here is
the same Ctrl+Z as everywhere else. The editor also listens to the undo manager, so an undo
performed in another window repaints this one.

Playback reads the lane through a single evaluator, `evalAutomationLaneAt` in
`PatternManager.h`. Live playback, the audio-thread parameter pass, the export clock and the
offline render all call it, which is what stops the drawn curve and the rendered result
disagreeing.

Automation only drives parameters in **Song mode**. Entering Song mode captures the current
value of every parameter any automation clip targets; leaving Song mode writes those captured
values back, so your knobs return to where you left them rather than wherever the playhead
happened to stop. That restore is a mode-boundary behavior, not a menu command.

Real-unit display (BPM, decibels, hertz) comes from two hooks the host wires in: parameters
report their own text the same way they do for their knob, and `global_tempo` uses its 20-300
BPM mapping.

---

## User-facing behavior

### Getting here

- Double-click an automation clip on the Builder grid, or right-click it and choose **Open in
  Event Editor...**
- Right-click almost any knob or fader in the app and choose **Automate: <name>**. That
  creates an automation clip for that control if one does not exist and opens it.
- **F12** brings the last Event Editor you had open back to the front.

A brand-new automation clip starts as a flat line at the control's *current* value, so
nothing changes until you move a point.

### Layout

Top row, left to right: the clip's name, an **Auto** / **LFO** pair of mode tabs, three LFO
knobs (visible in LFO mode only), a **New Automation Clip** button, a **Snap** dropdown, and a
dark red value readout showing the first point's value in real units.

The middle is the curve grid with a ruler across the top. Below it a strip of six tool
buttons; to the right, the list of every automation clip in the project. A status bar along
the bottom reads out the beat and value under your cursor.

### The tools

Six, selectable from the button strip, the Tools menu, or a bare letter key. The buttons are
labeled with the first letter of the tool name; the keys are different letters:

| Tool | Button | Key | What it does |
|---|---|---|---|
| Draw | D | `P` | Click empty space to add a point. Drag a point to move it. Drag the small diamond between two points to bend the segment. |
| Paint | P | `B` | Drag to lay a run of points along the path of your cursor. |
| Erase | E | `D` | Click a point to reset it to the value of the first point in the lane. It does not delete the point. |
| Interpolate | I | `I` | Click a point, or a segment, to cycle its curve type: Linear, then Stepped, then Spline, then back. |
| Select | S | `E` | Click points to select them; drag on empty space for a marquee. Ctrl+click adds to the selection. |
| Zoom | Z | `Z` | Arms and changes the cursor, but the grid has no zoom handling behind it - clicking and dragging with it does nothing. |

Dragging a point with Draw snaps its value to the exact middle of the lane when you get
within three percent of it, so a "no change here" point is easy to hit.

### Curve types

Each point owns the shape of the segment leading to the *next* point:

| Type | What it sounds like |
|---|---|
| **Linear** | A straight ramp. Dragging the diamond handle on the segment adds tension - the move eases out of the first value, or eases into the second, instead of running evenly. |
| **Stepped** | The value holds flat and then jumps at the next point. Use this for anything that should switch rather than sweep. |
| **Spline** | A smooth S-curve that follows the neighboring points, so a run of points reads as one flowing gesture instead of a chain of ramps. |

Stepped segments draw no diamond handle, because there is nothing to bend.

### Right-clicking a point

| Item | What it does |
|---|---|
| **Set Value...** | Type the value in real units - "140" for BPM, "-6.0" for decibels - instead of dragging for it. |
| **Reset to midpoint** | Sets that point to exactly halfway up the lane (0.5 of the parameter's range). It is a midpoint reset, not a return to whatever the control was before automation. |
| **Delete** | Removes the point. If it is the *last* point, you are asked whether to delete the whole automation clip instead, because a lane with no points controls nothing but would still sit on the Builder grid. |

The value a control had before automation touched it is restored automatically when you leave
Song mode, as described under "How it operates". There is no menu command for it.

### Snap

The Snap dropdown (and the View > Snap submenu) offers Bar, Beat, 1/8, 1/16, 1/32 and None.
Default is 1/16. Hold **Alt** while dragging to bypass snapping for that gesture.

Two caveats worth knowing: **Bar** and **Beat** currently produce the same one-per-beat grid,
and **None** is not truly free - it snaps to a very fine grid of 32 divisions per beat.

### LFO mode

Click the **LFO** tab and the clip stops following its points and instead repeats a shape for
as long as it plays. Three knobs appear:

| Knob | Range | Default | What it does |
|---|---|---|---|
| Min | 0.0-1.0 | 0.0 | The bottom of the sweep, as a fraction of the parameter's range. |
| Max | 0.0-1.0 | 1.0 | The top of the sweep. |
| Rate | 0.1-16.0 | 4.0 | How long one cycle takes. |

Two things to be aware of here:

- **The Rate knob's units are inconsistent between the drawing and the playback.** The
  preview curve is drawn as cycles per bar, while playback treats the value as the fraction of
  the clip's length that one cycle occupies. The drawn shape and the played shape will not
  match. This is recorded here as a known divergence, not as intended behavior.
- **There is no shape control.** The lane can store Sine, Triangle, Saw or Square and the
  playback evaluator honors all four, but no control in the app writes that field, so an LFO
  lane is always Sine.

### The clip list

The pane on the right lists every automation clip in the project by name. Click a row to
switch the editor to that clip; the window title follows. A row whose target no longer exists
- a deleted tab, a cleared effect slot, a removed pedal - is tagged `[stale]` and washed red,
so dead automation is visible at a glance rather than silently doing nothing.

The **New Automation Clip** button lists every control in the project that can be automated
and creates a clip for the one you pick.

### Menus

| Menu | Items |
|---|---|
| **File** | New Event Editor, Close, Save Automation Data |
| **Edit** | Undo (Ctrl+Z), Redo (Ctrl+Alt+Z), Select All (Ctrl+A), Copy (Ctrl+C), Paste (Ctrl+V), Erase to Range Start, Convert to Clip (Douglas-Peucker)... |
| **Tools** | Draw (P), Paint (B), Erase (D), Interpolate (I), Select (E), Zoom (Z) - the active one is ticked |
| **View** | Snap submenu (Bar / Beat / 1/8 / 1/16 / 1/32 / None), Toggle LFO Mode, Sync Zoom with Piano Roll |
| **Target Control** | Add Target..., Remove Target, Edit Targets..., Animate Target, Locate in Mixer |
| **Import MIDI** | Import MIDI CC Data... (Ctrl+M) - opens a `.mid` file from the app's MIDI folder, gathers every controller move in it, asks which controller number to use, and turns those moves into points on this lane |

**Erase to Range Start** flattens the whole lane to the value of its first point. **Convert to
Clip** thins a dense curve down: you type a sensitivity between 0.001 and 0.05 (higher means
fewer points) and it removes points that were not changing the shape.

**Menu items that currently do nothing.** New Event Editor, Save Automation Data, Copy, Paste,
Sync Zoom with Piano Roll, and every item under Target Control are drawn in the menus but have
no handler behind them. Selecting one has no effect.

### Keys

| Key | Action |
|---|---|
| `P` `B` `D` `I` `E` `Z` | Arm Draw / Paint / Erase / Interpolate / Select / Zoom |
| Delete or Backspace | Delete the selected points (last point prompts to delete the clip) |
| Ctrl+A | Select all points |
| Ctrl+Z / Ctrl+Alt+Z | Undo / Redo, in the same history as the rest of the app |
| Ctrl+M | Import MIDI CC data |
| Alt (held) | Bypass snapping during a drag |

---

## Parameters and persistence

The Event Editor registers no parameters of its own. It edits the lane inside an arrangement
block, and everything it changes is saved with the **project**:

`<Block clipType="2">` in `<PatternManager>` -> `<Arrangement>` carries an `<AutomationLane>`
child holding:

| Field | Meaning |
|---|---|
| `paramId` | The stable key of the control being automated. Never shown to the user. |
| `userDisplayName` | Your rename, if any. Takes precedence over the automatic name everywhere. |
| `lastKnownName` | The automatic name captured when the lane was made, so a lane whose target is gone still says what it used to drive. |
| `isLFO`, `lfoShape`, `lfoRate`, `lfoMin`, `lfoMax` | LFO mode state |
| `<Point time value curve tension>` | One per control point |

`curve` is stored as a raw number (0 Linear, 1 Stepped, 2 Spline), so those values are pinned
and only ever appended to.

**Not saved**: the active tool, the snap setting, which clip the window was showing, and the
window's own size and position (Event Editors are ordinary floating windows, not part of the
saved workspace layout).

---

## Lifetime and teardown

An Event Editor is created on demand by `StandaloneEditor::openEventEditor` and owned by the
editor's list of open editors. Closing the window fires `onClosed`, which removes and deletes
it. Any number can be open at once; they all edit the same underlying data and all repaint
when the undo history moves.

Two ordering points:

- The window close handler copies its `onClosed` callback to a local before tearing down, so
  the notification survives the teardown that triggered it.
- Key routing is deliberate: the command keymap is registered first and the content second,
  because listeners fire in reverse order. That gives the editor's single-letter tool keys and
  Ctrl+M priority over the global bindings they would otherwise collide with.

The editor binds to a block by *index*. Deleting a block that an open editor is showing leaves
that editor pointing at a different clip; the bind is not identity-preserving.

---

## Cross-references

- `Builder Page.md` - where automation clips live on the timeline, and the inline point
  editing the grid itself offers.
- `Patterns and Arrangement.md` - how an automation clip's length and position are stored.
- `Piano Roll.md` - the equivalent editor for notes.

---

## Differs from Carry-Forward

Carry-Forward has no Event Editor section, so there is nothing to reconcile. Its "Automation
lane UUID resolver" note describes a lane binding to a stale slot id at creation; lanes now
capture a `lastKnownName` at creation and resolve their display name live, and a lane whose
target is gone is tagged `[stale]` in the clip list rather than silently mislabelled.
