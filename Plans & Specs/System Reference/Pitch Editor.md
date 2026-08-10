# Pitch Editor (BaySickPitch)

**Purpose** - BaySickPitch is where you fix a vocal note by note after it has
been recorded. It listens to everything on this Vox tab's track, finds each sung
note, and draws it as a colored pill you can drag up or down to retune, drag
sideways to retime, stretch, split, or shape from the inside. Edits play back
live - there is no bake step - and a Render button exists only for when you want
the result as a file.

## How it operates

`BaySickPitchEditor` (`Source/BaySickVocal/BaySickPitchEditor.cpp` / `.h`) is the
UI; `BaySickPitchDSP` (`Source/DSP/BaySickPitchDSP.h`) holds the analysis and the
edits. Both hang off the tab's `BaySickVocalProcessor`.

- **Analysis** takes the whole channel composite - every audio clip on this Vox
  tab's arrangement row, rendered into one buffer via the page-injected
  `onRenderComposite` hook - runs pitch detection over it once, and segments it
  into note regions with absolute timeline positions. Message thread only.
- **A note region** (`PitchNoteRegion`) stores what was detected (start, end,
  median MIDI pitch, median F0, measured vibrato depth and rate) and what you
  changed (pitch shift, formant shift, vibrato depth and rate multipliers,
  variation, a destination time span, a volume curve and an additive pitch curve).
- **Playback** goes through a realtime applicator on the audio thread. During
  clip playback the tab's realtime corrector is force-bypassed and
  `mPitch.processFilePlay` runs instead, mapping the running strip audio to the
  overlapping region by timeline position. The applicator reads an immutable
  snapshot published by atomic pointer swap; retired snapshots are kept alive on
  the message thread in a ring of eight.
- **Time edits are upstream of Align.** Moving a pill publishes a channel time
  map; BaySickAlign consumes the edited performance, and a time edit here marks
  an existing alignment stale.
- **Staleness and auto re-analysis.** If the clips on the row change, the analysis
  is stale. The Vox page's 4 Hz poller re-runs it automatically once the layout
  has been stable about a second and the transport is stopped. While the
  transport runs the toolbar shows `RE-ANALYZE ON STOP`.
- **Freeze.** Pitch edits are baked into a frozen Vox track, so committing an edit
  fires the hook that invalidates the freeze.

## User-facing behavior

Open it from the Vox tab's **Menu -> BaySickPitch**. It opens in its own window
(1534 x 724 minimum).

The layout is a toolbar, a piano keyboard down the left, the canvas, a status
line at the bottom, and scroll bars on the right and bottom edges.

### What you see on the canvas

- A green curve is the pitch you actually sang, as sung.
- Purple pills are the detected notes, drawn at their **corrected** pitch and
  filled with that note's waveform. So the pill shows where a note is going and
  the green curve shows where it came from.
- Gray pills are **slices** - pieces excluded from pitch correction (typically a
  consonant chopped off a vowel). They sit at the note's own detected pitch, have
  no waveform interior, and keep their volume shaping while being skipped by
  pitch, focus and vibrato.
- A pill you have detached from its neighbors (Ctrl-dragged) carries an amber
  outline.
- Selected pills carry a white outline.
- A playhead follows the main transport at 30 Hz, including the reset to zero
  when you stop.

### Toolbar

| Control | What it does |
|---|---|
| **Save** / **Load** | Save or recall the Focus, Mod and Speed knob values as a named user preset. (Throat is not included in the preset file.) |
| **Slice** / **Edit** | The two working modes. Edit is the default. |
| **Send Notes to...** | Sends the detected notes as MIDI into an open Layers, Bass, Drums or Clips tab, so you can double a sung line with an instrument. The riff is normalized so the first note starts at zero, and slices are skipped. |
| **Root** / **Scale** | The key that Snap and Focus aim at. Same 13-scale list as the piano roll, and deliberately independent of the realtime board's key. |
| **Snap** | On: Focus pulls toward the Root/Scale notes and vertical drags land on in-scale lanes. Off: nearest semitone. Hold Ctrl while dragging for free fine movement either way. |
| **ON** (toggle) | Plays the pitch edits live through the chain. Turning it off glides back to the untouched take - it is never a hard switch. |
| Engine dropdown | **Rubber Band - Balanced** (default), **Signalsmith - Lightest (Low CPU)**, **WORLD - Highest Quality (High CPU)**. The first two edit live; picking WORLD shows a one-time notice that it processes offline, so edits made during playback apply a moment later. The notice has a "Do not show this again" box. |
| **Snapshot** | Saves the current edits as a restore point. |
| **Versions** | Reverts to an earlier restore point (every analysis creates one too). Grayed during playback - stop first. |
| **Reset** | Clears every pitch edit on this channel, as one undo step. |
| **Render** | Bakes the edited channel to `Pitched/{name}_pitch_v{N}.wav` in the project folder. File only - playback is already live. |
| **Undo** / **Redo** | Drive the app-wide undo history. Ctrl+Z works too. |
| **A** | Auto-scroll the canvas to follow playback. The `A` key toggles it. |

Four knobs sit at the right of the toolbar and apply to the whole channel:

| Knob | What it does | Range | Default |
|---|---|---|---|
| **Focus** | How strongly note centers are pulled to the target - the nearest semitone, or the Root/Scale note when Snap is on. 0 leaves the take alone. | 0-100 | 0 |
| **Mod** | Vibrato movement. 50 is natural; above 50 adds synthesised vibrato per note. | 0-100 | 50 |
| **Speed** | How fast pitch moves between notes. Low glides smoothly, high is instant. (Internally 0 maps to a 300 ms glide and 100 to about 5 ms.) | 0-100 | 50 |
| **Throat** | Character. 50 is natural; left is bigger and darker, right is smaller and brighter. Formant only - the pitch stays put. (Internally -12 to +12 semitones of formant shift.) | 0-100 | 50 |

A monospace status line at the bottom shows the pitch, cents offset and length of
whatever you hover or select; the toolbar shows a LENGTH readout and any analysis
badge (`ANALYZING...`, `RE-ANALYZE`, `RE-ANALYZE ON STOP`, or a failure message).

### Editing - mouse

In **Edit** mode:

| Gesture | Result |
|---|---|
| Drag a pill's body | Retune it up or down. 0.1 semitone steps; hold **Ctrl** for 0.01. With Snap on, it lands on in-scale lanes. Dragging up can never knock a note out of line - plain body drags are vertical only. |
| Drag a pill's left or right edge | Stretch or squeeze the note in time. |
| **Ctrl** + edge drag | Detached stretch - the note's boundary is free to cut rather than push its neighbor. |
| **Ctrl+Alt** + drag | Move the note in time, elastically: the gaps either side counter-warp and pills never cross. |
| **Ctrl+Shift** + drag | Detached move - a hard cut, no walls. |
| Click empty canvas and drag | Marquee select. |
| **Shift**+click a pill | Add or remove it from the selection. |
| Double-click a pill | Opens the sub-editor popup for that note. |
| Right-click a pill | The pill menu (below). |
| Right-click empty canvas | With a selection and a real scale chosen, forces the selection to scale. With nothing selected, returns to the view you zoomed away from. |
| **Ctrl** + right-drag | Zoom to the dragged rectangle. Right-click afterwards to go back. |
| Click a key on the left keyboard | Retunes the whole selection to that note, and auditions it while the transport is stopped. |

In **Slice** mode a pill click is a blade, not a selector: it splits the note at
the click point, snapped to the beat grid (hold **Alt** to cut anywhere). If one
half is mostly unvoiced it is automatically marked as a slice, so tuning the
vowel does not drag the consonant noise with it.

Right-click pill menu: Restore to Original State, Snap to Semitone, Force to
Scale (enabled only when a real scale is picked), Merge Selected Pills / Merge
With Next Pill, Open Sub-Editor..., Zoom to Selection, and Include in / Exclude
from Pitch Correction. Each item says "Selected" when more than one pill is
selected.

### Editing - keyboard

| Key | Result |
|---|---|
| **A** | Toggle auto-scroll |
| **Ctrl+A** | Select all notes |
| **Ctrl+C** / **Ctrl+V** | Copy / paste the *treatment* (shift, formant, vibrato, variation, curves) - the audio never moves |
| **Ctrl+B** | Apply the focused note's treatment to the whole selection in one step |
| **Shift+Up / Down** | Nudge selected notes by 0.1 semitone |
| **Shift+Left / Right** | Nudge selected notes by 10 ms |
| **Delete** / **Backspace** | Restore the selection to its original state (pills are analysis segments, not deletable objects) |
| **Esc** | Close the sub-editor if open, otherwise clear the selection |

### View

Ctrl+wheel zooms time, Alt+wheel zooms the note lane height, Shift+wheel scrolls
sideways, plain wheel scrolls up and down, and middle-drag pans. Clicking the
ruler seeks the main transport - the same playhead the Builder grid uses.
Ctrl+dragging the ruler selects a time range, which drives song-mode looping.

### The sub-editor popup

Double-click a note (or use the pill menu) to open a floating window for that one
note. It shows the note's length as an editable lane with the waveform ghosted
behind, plus a browser panel listing the other notes so you can step through them.

| Control | What it does | Range | Default |
|---|---|---|---|
| **Volume** / **Pitch** (lane toggle) | Which curve the lane edits - the note's volume shape, or an additive pitch curve that audibly bends pitch inside the note. | | Volume |
| **Play** | Previews this note through the current edits. **Space** does the same. | | |
| **Vib** knob | Vibrato depth for this note. 1 is natural, below flattens, above deepens. | 0-2 | 1 |
| **Frm** knob | Formant shift for this note, in semitones. | -6 to +6 | 0 |
| **Var** knob | Variation - scales the note's own natural pitch wiggle around its center. 0 flattens it, 2 exaggerates it. | 0-2 | 1 |

In the lane, click empty space to add a point, drag to move one, right-click a
point to delete it. These are the *same* points the small corner handles on the
pill write, so a ramp made with a handle appears here and vice versa. Every
gesture lands as one step on the app-wide undo history.

### Analysis behavior you will notice

- Opening the editor on a channel that has never been analyzed analyzes it
  immediately, even during playback - there are no edits yet, so nothing can jump.
- Re-analyzing an already-analyzed channel is held until the transport stops
  (`RE-ANALYZE ON STOP`), because swapping the map mid-play would be an
  unbounded change.
- Only one analysis runs at a time. A failure leaves a visible failed state
  carrying the reason (for example "This channel has no audio clips to analyze.").

## Parameters and persistence

Editor parameters live on the Vox tab's APVTS under `bsp_`:

| Parameter | Range | Default |
|---|---|---|
| `bsp_focus` | 0-100 | 0 |
| `bsp_mod` | 0-100 | 50 |
| `bsp_speed` | 0-100 | 50 |
| `bsp_throat` | 0-100 | 50 |
| `bsp_root` | 0-11 | 0 (C) |
| `bsp_scale` | 0-12 | 0 (Chromatic) |
| `bsp_snap` | bool | false |
| `bsp_mode` | 0 Slice / 1 Edit | 1 |
| `bsp_on` | bool (chain switch) | true |
| `bsp_engine` | 0 Rubber Band / 1 Signalsmith / 2 WORLD | 0 |

`bsp_focus` defaults to 0 deliberately, so a channel that has been analyzed but
not touched plays back untouched.

**Saved with the project**, inside the Vox engine's state as a `<PitchEdits>`
child:

- `pSig` - the clip signature captured at analysis time, which is what makes the
  analysis go stale when the row changes;
- a `PitchState` tree - the analyzed flag, the analysis frame (start beat, start
  sample, sample rate, composite length) and one `<N>` element per note region
  carrying both the detected values and every edit, including the destination
  span for time moves and the packed volume and pitch curves;
- the render history (`Pitched/` files with date, version and origin beat);
- up to 20 version snapshots.

On restore, auto-detected slice pills the user never touched are dropped (that
detection model was retired), but any slice that was shaped or moved is kept.

**Saved as a user preset file** (`Presets/BaySickPitch/My Presets/<name>.xml`):
`bsp_focus`, `bsp_mod`, `bsp_speed` only.

**Per machine:** the WORLD-offline "do not show again" flag and the multi-select
reset "do not show again" flag, in the app's UI preferences file.

**Not saved:** the canvas view (zoom, scroll, lane height), the selection, the
clipboard, and the analysis staleness state, which is recomputed on load.

## Lifetime and teardown

- The editor object is created with the Vox tab's `BaySickVocalEditor` and lives
  as long as the tab, whether or not its window is open. That is deliberate: an
  analysis must survive closing the window.
- The satellite window hosts the panel without owning it, and resolves it through
  the page each time, so a page rebuild cannot leave a dangling host.
- The destructor stops any preview playback. The processor's destructor deletes
  the published preview buffer and the align playback snapshot after the audio
  thread has been settled.
- The sub-editor window is owned by the editor and torn down when closed or when
  Esc is pressed.

## Cross-references

- `Vox Page.md` - the tab, the composite this editor analyzes, the auto
  re-analysis poller.
- `Align Editor.md` - consumes this editor's time edits; a time edit here marks
  an alignment stale.
- `Vocal Chain.md` - the chain the previews and playback are voiced through.

## Differs from Carry-Forward

Carry-Forward (2026-05-07) records BaySickPitch only as a deferred defect line -
"DSP-04 / QA-Fa / BaySickPitch missing audio import (additive)". The editor as it
now stands resolves the channel composite automatically from the arrangement row
with no manual import step, and the whole note-editing surface described above
postdates that snapshot.
