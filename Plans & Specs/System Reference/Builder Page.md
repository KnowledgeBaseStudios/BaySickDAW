# Builder Page

**Purpose** - The Builder is where a song gets assembled. It is a horizontal timeline of
500 numbered track rows on which you place three kinds of clip: pattern clips (a block of
notes you wrote in the Piano Roll), audio clips (a recording or an imported file), and
automation clips (a moving control value). It also owns the song's tempo and time-signature
flags, the song loop range, and the Export Audio dialog that renders everything to a file.

---

## How it operates

`BuilderPage` (`Source/Standalone/BuilderPage.cpp/.h`) is a container. It owns five things
and no song data of its own:

| Part | Class | What it is |
|---|---|---|
| Left panel | `BrowserPanel` | Patterns / Files / Auto lists, drag sources |
| Toolbar | `ArrangementToolbar` | Snap, tools, Slip/Stretch, undo/redo, zoom |
| Row labels | `TrackHeaderPanel` | Row names plus per-row mute and solo LEDs |
| Timeline | `ArrangementGrid` inside a `juce::Viewport` | The clips themselves |
| Scrollbar | `juce::ScrollBar` under the grid | The one horizontal scroll mechanism |

All song data lives in `PatternManager` (`Source/PatternManager.cpp/.h`): the pattern list,
the arrangement block list, per-row names/mute/solo/group state, the audio library, and the
time-marker / time-signature / tempo-change lists. The grid is a view onto that, not a copy
of it. Every edit runs on the message thread and is wrapped in an undo transaction by
`ArrangementGrid::beginEdit` / `commitEdit`, which pushes one before-and-after snapshot of
the whole arrangement into the app-wide undo history.

The audio thread never touches the grid. It reads the block list directly inside
`BaySickDAWProcessor::processBlock` for pattern and automation scheduling, and reads audio
clips through per-clip `AudioClipStreamer` objects that `onArrangementChanged` rebuilds
whenever a block moves, resizes or is deleted.

The vertical scroll is the viewport's; the horizontal scroll is virtual (the grid's own
`mBarOff` bar offset), driven by the external scrollbar so both halves of the page cannot
disagree.

Two draw caches sit on the grid, both keyed on a clip's stored file path: an
`AudioThumbnail` cache (256 entries) and a baked waveform-image cache (32 entries of a
2048x64 image). Both drop their oldest entry past the cap and re-bake on demand, which is
what keeps a slip-edit drag smooth at any zoom.

The offline render core, `BuilderPage::runOfflineLoop`, also lives here. It suspends the
audio device, sweeps the whole engine into non-realtime mode, re-prepares the graph and
renders faster than real time. Audio export, the Measure pass, and every track-freeze render
all drive that same loop; it is never copied.

---

## User-facing behavior

### Reading the page

Bars run left to right along the ruler at the top. Rows run top to bottom; each row is a
"track" and starts out named Track 1, Track 2 and so on. A clip is a colored box on a row
between two bar positions. The vertical white line is the playhead - where the music
currently is.

Rows have no fixed type. Drop a pattern on row 3 and it is a pattern row; drop a WAV on
row 4 and it is an audio row. Nothing stops you mixing them, but an audio row is the only
row that can be rendered to a single WAV on its own (see the row menu below).

### The toolbar

Left to right:

| Control | What it does |
|---|---|
| **Snap** | Click for the snap menu (11 entries). Lit = snap active, dim = Off. Default **Line**. |
| **Draw (P)** | Click on empty space to place one clip. What it places is whatever you last clicked in the browser. |
| **Paint (B)** | Drag along a row to lay a run of identical clips end to end. |
| **Select (E)** | Drag a rectangle to select clips; drag a selected clip to move the whole selection. |
| **Delete (D)** | Click clips to remove them. |
| **Mute (T)** | Click a clip to mute it. It stays where it is and makes no sound. |
| **Slice (C)** | Drag a line across clips; every clip the line crosses is cut in two at that point. |
| **Zoom (Z)** | Drag a rectangle to zoom into it; right-click zooms back out. |
| **Play (Y)** | Selectable, but currently does nothing on click - the grid has no handler for this tool. The Key Binds window still describes it as "click a clip to audition it"; that behavior is not implemented. |
| **Slip / Stretch** | Dropdown (also the `S` key) choosing what dragging an **audio** clip's edge does. See below. |
| **Undo / Redo / H** | Undo, redo, and open the undo history window. Same single history as the rest of the app. |
| **+ / -** | Zoom in and out horizontally, around the middle of the view. |
| Context label | Right-aligned read-only text showing which pattern is selected. |

**Slip vs Stretch.** Only audio clips care. In **Stretch** (the default), dragging the
clip's right edge changes its length. In **Slip**, dragging either edge slides the audio
*inside* the box: drag the left edge left and you reveal audio recorded before the clip
started (this is how you get at a count-in take), drag it right and you trim the head off.
The far edge stays put either way. Pattern and automation clips ignore this setting
completely.

### The snap menu

The same eleven-entry ladder appears in the Builder, the Piano Roll, the Drum Kit and the
record-quantize menu, so one word always means the same thing:

| Entry | Grid |
|---|---|
| Off | No snapping at all |
| Line | Whatever grid lines are currently visible at this zoom - zoom in, snap gets finer |
| Bar | 1 bar |
| Beat | 1 beat (a quarter note) |
| 1/2 Beat | Eighth note |
| 1/3 Beat | Eighth-note triplet |
| Step | Sixteenth note |
| 1/2 Step | Thirty-second note |
| 1/3 Step | Thirty-second triplet |
| 1/4 Step | Sixty-fourth note |
| 1/6 Step | Sixty-fourth triplet |

Holding **Alt** while dragging turns snapping off for that one gesture.

### Track rows (the left column)

Each row shows two small LEDs then its name. The left LED is **mute**, the right is **solo**.
Solo anywhere on the page silences every row that is not soloed. Double-clicking the name
renames the row. Right-click gives:

| Item | What it does |
|---|---|
| Rename... | Type a new row name. |
| Move Up / Move Down | Swap this row with its neighbor. A grouped run of rows moves as one block. |
| Insert Track Above | Push everything below down by one row and leave a blank row here. |
| Group with Above | Join this row to the one above so they move together. |
| Remove from Group | Take this row back out of its group. |
| Color Group... | Pick the color the group's rows are tinted. |
| Render Track to WAV... | Consolidate this row to one audio file. Only enabled on a row that holds audio clips. |
| Delete Track Clips | Remove every clip on this row. |

### The ruler

- **Click** anywhere on the ruler to move the playhead there.
- **Ctrl + drag** on the ruler paints a time selection. In Song mode that selection *is* the
  loop: playback wraps between its ends. Ctrl + Left / Right slides the selection by its own
  length.
- **Right-click** opens the ruler menu, which is where the song's timing flags live:

| Item | What it does |
|---|---|
| Add Time Marker at Bar N... | A named flag on the ruler. Purely a label for you; it does not change playback. |
| Add Time Signature at Bar N... | The signature from that bar forward. |
| Add Tempo Change at Bar N... | Type a BPM (20-300). The song runs at that tempo from that bar until the next flag. |
| Edit / Delete Marker | Shown only when you right-clicked on an existing marker. |
| Edit / Delete Time Signature | Shown only on an existing signature flag. |
| Edit / Delete Tempo Change | Shown only on an existing tempo flag. |

The tempo box on the main toolbar shows the tempo *at the playhead*. The number it edits is
the song's base tempo - the one in force from bar 1 until the first tempo flag.

Note that the Builder's own bar lines stay a uniform four beats wide regardless of
time-signature flags. Signature flags drive playback, the position readout and pattern
signatures; they do not re-space the grid.

### Working with clips

| Gesture | Result |
|---|---|
| Drag a clip | Move it. Alt bypasses snap for fine positioning. |
| Drag a clip's right edge | Resize it (see Slip / Stretch above for audio). |
| Ctrl + click a clip | Add it to the selection. |
| Ctrl + drag on empty space | Marquee-select, whatever tool is active. |
| Ctrl+A / Ctrl+C / Ctrl+V / Ctrl+B | Select all, copy, paste at the playhead, duplicate to the right. |
| Delete | Delete the selection. |
| Ctrl + Delete | Delete everything inside the highlighted time span and close the gap. |
| Shift + arrows | Nudge the selection one bar sideways or one row up/down. |
| Right-Alt + left-click | Toggle mute on the clicked clip. |
| Right-Alt + right-click | Quantize popup for the selection (Bar / 1/2 Bar / Beat / Step). |
| Ctrl + Shift + right-click | Zoom the view so the clicked clip fills it. |
| Ctrl + right-click + drag | Drag a rectangle and release to zoom-fit it. |
| Shift + right-click + drag, or middle-drag | Pan the view. |
| Ctrl + wheel | Horizontal zoom anchored under the cursor. |
| Alt + wheel | Vertical zoom (row height, 16 to 96 pixels) anchored under the cursor. |
| Shift + wheel | Scroll sideways. Bare wheel scrolls up and down. |
| Double-click an automation clip | Open it in the Event Editor. |

**Clip right-click menu:**

| Item | Applies to | What it does |
|---|---|---|
| Cut / Copy / Paste | all | Standard clipboard. |
| Delete | all | Remove. |
| Mute / Unmute | all | Silence the clip without moving it. |
| Properties... | audio | Pitch shift in semitones, the detected original BPM (shown read-only here), Stretch vs Resample mode, and a "Routes to:" picker that can move or copy the clip onto a Clip / Vox / Inst page. "Reset to Browser Entry" snaps every setting back to the file's library entry. |
| Reset Stretch | audio | One click back to the file's natural tempo and full natural length, with any slip offset cleared. Position is kept. Only enabled if the clip has actually been stretched. |
| Open in Event Editor... | automation | Opens the curve editor on that clip. |
| Set Time Signature | pattern | 4/4, 3/4, 2/4, 6/8, 5/4, 7/8, 12/8, 9/8 - sets the referenced pattern's own signature. |

**Stretch vs Resample.** Stretch keeps the pitch where it is and changes only the speed.
Resample changes both together, the way speeding up a tape does.

### The browser (left panel)

Three tabs:

- **Patterns** - one draggable box per pattern, plus **+ Add** and **Delete** buttons.
  Right-click a pattern for Rename, Duplicate, Change Color, Render to WAV, Split by Player
  Engine, Delete.
- **Files** - a tree of everything audio in the project, in five groups: **Clips**, **Vox**,
  **Inst** (files bound to a page), plus **Exports** and **Reports** (files you have
  rendered, read straight out of the project's folders). A Sort button orders the items
  inside each group by newest, oldest or name.
  Right-click a Clips / Vox / Inst file for Rename..., Duplicate..., Reveal in Explorer,
  Properties..., a Regenerate De-noise submenu on a cleaned take, a Choke Group submenu, and
  Delete. Right-click an Exports or Reports file for Open in Analyzer (reports only), Add to
  Project..., and Reveal in Explorer. Right-click a category heading to create a group, or a
  group heading to rename it.
- **Auto** - the reusable automation lanes.

Drag any browser item onto the grid to place it. Clicking an item also arms it, so the Draw
tool will place that kind of thing on the next empty-space click.

The panel's right edge is a drag handle: pull it wider (up to three times its default) or
push it past the left to collapse it to nothing. A small chevron marks the collapsed edge -
drag it back out to reopen. The width resets each session.

### Dropping files in

Drag WAV / MP3 / AIFF / FLAC / OGG files from Windows straight onto a row. A translucent
ghost shows where they will land. Dropping several files at once staggers them onto separate
rows so each gets its own mixer strip. If a file is already in the project you are asked
whether to reuse the existing routing, make a new page and strip, or cancel. If no project
is open yet you are prompted to create one, then the drop completes.

**MP3 carries a size ceiling the other formats do not.** An MP3 is decoded whole into
memory the moment it is opened rather than streamed off disk, so a file over 512 MB on
disk, or one that decodes to more than about 22 minutes of 48 kHz stereo, is refused
outright - it reads as "could not be read" wherever you tried to use it, with no partial
load. Convert a long MP3 to WAV or FLAC first. WAV, AIFF, FLAC and OGG have no such limit.

### Page menus

The window's title strip carries three headings:

- **Menu**: Import Audio..., Rename Pattern (F2 - opens the combined name-and-color dialog),
  Find Next Empty (F3), New Automation Clip..., Render Pattern to WAV... (which offers Per
  Track, Full Mix or Select Tracks).
- **Edit**: Undo, Redo, Select All, Deselect, Copy, Paste, Delete, Duplicate.
- **View**: Zoom In, Zoom Out, Performance Mode (Ctrl+P).

**Performance Mode** tints the part of each clip the playhead has already passed and draws a
progress edge on it, so you can see at a glance how far through every running clip you are.
It changes nothing about the sound.

### Export Audio

`File > Export Audio` (Ctrl+R) opens one dialog. You set everything first, then pick a
destination; the settings box stays on screen and turns into a progress bar with a working
Cancel while the render runs.

| Control | Options | Notes |
|---|---|---|
| Selection | Full Arrangement / Selected Section | "Selected Section" is grayed out unless you have a ruler time selection. |
| Tail | Included / Cut | Included keeps rendering past the last note until the sound actually decays (capped at 60 seconds). Cut stops dead at the end. |
| Format | WAV / OGG / MP3 | |
| Quality | WAV: 16-bit / 24-bit / 32-bit float. OGG: Low / Medium / High / Highest. MP3: 128 / 192 / 256 / 320 kbps | Default 24-bit for WAV, High/256 otherwise. |
| Sample rate | 44100 / 48000 / 88200 / 96000 / 176400 / 192000 Hz | Default 44100 Hz. With Format set to MP3 the four rates above 48000 Hz are grayed out - MPEG Layer III cannot encode them - and a rate already above 48000 Hz drops back to 48000 Hz. |
| Dither | Off / Flat (TPDF) / Noise-Shaped | Applies to 16-bit WAV only. |
| Normalize to | Checkbox plus a typed LUFS box, default -14 | Measures first, then re-renders with a fixed gain, capped so the true peak stays under the ceiling. |
| Export stems (one file per mixer strip) | Checkbox revealing a per-strip checklist | Master and buses start unticked. Stems are tapped in the same single pass, so a bass ducked by the kick keeps its ducking. |
| Check against | Loudness spec list, with a Custom reference box | Remembered between sessions. |
| Measure | Button | Runs the same render with meters instead of files, writes nothing, and reports integrated loudness, loudness range and true peak in two lines inside the dialog. |

Files land in the project's `Exports` folder. If the project has never been saved you are
told so and taken through Save As first; the export then continues on its own. Stems are
named `<file> - <strip name>.<ext>` beside the main file, and the rendered files appear
immediately under **Exports** in the browser's Files tab.

---

## Parameters and persistence

| Parameter id | Type | Range / default | What it holds |
|---|---|---|---|
| `Unified_BuilderSnapDiv` | Int | 0-10, default 1 (Line) | The Builder's snap division. Independent of the Piano Roll's. |

Saved with the **project**:

- `<Arrangement>` inside `<PatternManager>` - one `<Block>` per clip carrying `trackRow`,
  `patternIndex`, `startTicks` (96 ticks per beat, authoritative), `lengthBars` +
  `lengthTicks` (-1 meaning "use bars"), `clipType`, `audioFilePath`, `displayAlias`,
  `pitchSemitones`, `originalBPM`, `stretchMode`, `muted`, `routeChannel`, `alignBake`,
  `isOverride`, `contentStartSamples`, `contentOffsetTicks`, and an `<AutomationLane>` child
  for automation clips.
- `<RowState>` - mute and solo as 500-character strings, plus a sparse child per row that has
  been renamed or grouped.
- `<TimeMarkers>`, `<TimeSigChanges>`, `<TempoChanges>` - all sorted by bar on load.
- `<AudioLibrary>` - one entry per audio file with its alias, choke group, owning page,
  pitch, BPM, stretch mode and browser group. The library entry is the source of truth for a
  file's properties; a grid clip either follows it or has been detached (`isOverride`).
- `<AutomationTemplates>` - the reusable lanes shown on the Auto tab.
- `<Arrangement ppBar barOff selStart selEnd>` inside `<UIState>` - the Builder's zoom,
  horizontal scroll and time selection. The selection pair is only written when one exists.

Saved **per machine**, not with the project: nothing on this page.

**Not saved at all**: the browser panel width, the active browser tab, the current tool, the
Slip/Stretch mode, Performance Mode, and both draw caches.

---

## Lifetime and teardown

The Builder is one of the four default pages. It is created with the editor and lives for
the whole session; its window can be closed and reopened without destroying the page. The
30 Hz animation poll starts and stops in `parentHierarchyChanged` keyed on whether the page
actually has a window, so a closed Builder costs nothing.

Order that matters:

- `BrowserPanel`'s destructor drops the tree's root item before the members unwind, because
  the `TreeView` is declared before the root it points at.
- `purgeWaveformCacheFor` must never run while a draw or a slip-edit drag is holding that
  file's thumbnail pointer, so it is only called from menu callbacks.
- A long export can outlive the page. The background export path holds a safe pointer to the
  Builder and the dialog joins its render thread (bounded, because the render checks for
  cancellation every block and restores the audio device on the way out) before it dies.

---

## Cross-references

- `Patterns and Arrangement.md` - what a pattern is, the pattern list, and how pattern blocks
  behave once placed.
- `Piano Roll.md` - where the notes inside a pattern clip come from.
- `Event Editor.md` - the full editor for an automation clip.

---

## Differs from Carry-Forward

Carry-Forward's "Builder grid drop & block resize" section is a file-and-line index taken
before several rounds of work; the line numbers no longer resolve. Three substantive deltas:

- It records a dead `Properties...` duplicate at item 7 of the clip menu with no handler.
  That line is gone; item 7 is now **Reset Stretch**, and it works.
- It describes block start as a bar index plus a tick offset. Block start is now a single
  beats value (`ArrangementBlock::startBeats`); the bar index is derived for display and
  serialization only, and a block start may be negative when slip-editing exposes pre-roll.
- It notes that a block resize does not rebuild the audio-clip players. Resize now runs
  through `commitEdit` into `onArrangementChanged`, which rebuilds them.
