# Undo History

**Purpose** - One undo history covers the whole app. Whatever you just did - moved
a note, nudged a fader, swapped an effect, deleted a whole tab - Ctrl+Z takes it
back, and it does so in the order you did things regardless of which window you did
them in. There is no separate per-editor undo to get lost in.

## How it operates

There is exactly one `juce::UndoManager`, owned by `StandaloneEditor`
(`Source/Standalone/StandaloneEditor.cpp`). No editing surface owns a manager of
its own. Instead each page and editor holds an `UndoContext` token
(`Source/Standalone/UndoActions.h`) carrying a pointer to the manager plus four
closures: `perform`, `undo`, `redo`, `showHistory`.

Undo and redo always go through `StandaloneEditor::globalUndo` / `globalRedo`
rather than calling the manager directly. Those two do three things around the
manager call: flush any parameter gesture that is still pending (so "undo right
after a knob tweak" undoes *that* tweak), open a missing-file reporting scope (a
resurrected tab may reference a sample that has since moved), and rebuild the
history window's label list afterwards.

**Actions.** Each undoable thing is a typed action in
`Source/Standalone/UndoActions.h`. Most carry a full before/after snapshot plus a
closure that applies one of them:

| Action | Covers |
|---|---|
| `PianoRollEditAction` | Notes in one piano roll. |
| `PitchEditAction` | Pitch-editor regions on one channel. |
| `ArrangementEditAction` | Arrangement blocks, row names, row group / color / mute / solo. |
| `MixerStateAction` | Mixer state. |
| `FloatParamAction` | A single parameter value. |
| `EffectRackAction` | Rack slot contents. |
| `AutomationLaneEditAction` | One automation lane's points. |
| `StructuralOpAction` | Anything that adds, removes or replaces a whole thing - tabs, engines, chain swaps. |
| `PatternListAction` | Pattern add / duplicate / remove, including the arrangement re-indexing it cascades into. |
| `PatternRenameAction` / `PatternColorAction` | Pattern name and color. |
| `MarkerSetAction` | Time markers, tempo changes, time-signature changes. |
| `AudioLibraryAction` | Audio-library entries and their properties. |
| `AutomationTemplateAction` | Saved automation templates. |

Most follow the same convention: the edit has already been applied by the time the
manager is told about it, so the first `perform()` is skipped and only later redos
re-apply.

**Identity-preserving restore.** Undoing a tab deletion creates a *new* page
object, so an action that captured a pointer to the old page would find nothing.
Actions instead resolve the page that currently owns their identity through
`UndoContext::resolveOwnerPage` at apply time. The history's own order guarantees
a tab is re-created before the rows that edit it replay.

**Snapshot files.** A structural action too big to hold in memory - a whole tab's
engine chain - writes its before and after states as XML files under
`Documents\BaySickDAW\UndoSnapshots\`. The action owns those files and its
destructor deletes them, so falling off the depth cap, clearing the history and
tearing the app down all reclaim them. The whole folder is swept at startup, at
every project load, and at exit.

The store has one hard rule: an empty file result means the snapshot is **not** on
disk. A gesture whose next step destroys what it just snapshotted (deleting a tab)
must abort and tell you, rather than leaving a "Delete" row in the history that
cannot actually be undone.

**The dirty marker.** The manager's change broadcasts are also what drive the
window-title asterisk. The processor keeps a transaction counter and the counter
value at the last save; dirty means the two disagree. So undoing back past your
last save clears the asterisk, and redoing forward brings it back.

## User-facing behavior

| Control | Where | What it does |
|---|---|---|
| Undo | Edit menu, **Ctrl+Z** | Steps back one edit. Grayed when there is nothing to undo. |
| Redo | Edit menu, **Ctrl+Alt+Z** | Re-applies the edit you just undid. Grayed when there is nothing to redo. |
| History... | Edit menu | Opens the Undo History window. |
| Undo History Size | Options menu submenu | 100 / 250 / 500 / 1000 steps. |

### The Undo History window

A narrow list, oldest edit at the top. Everything above the marker has already
happened; the marker row reads `>>  Current`; everything below it in dimmed text
is redo-able. **Click any row to jump there** - clicking above the marker undoes
that many steps, clicking below redoes that many. The window updates itself
whenever the history changes, including from edits made elsewhere while it is
open, and Ctrl+Z / Ctrl+Alt+Z still work while it has focus.

Row labels are the names of the edits ("Delete Layer", "Lock Kit 1", "Load Page
Preset"). Parameter moves are labeled with the control's readable name, prefixed
with the tab that owns it where the app can resolve one. An edit that arrived
without a name shows as "(edit)".

### What is undoable

The design target is **every action**. In practice that means:

- Notes and note edits in every piano roll and drum grid.
- The arrangement: blocks, row names, row grouping and color, row mute and solo.
- Patterns: add, duplicate, delete, rename, recolor, reorder.
- Time markers, tempo changes, time-signature changes.
- Every knob, fader and toggle you move by hand - one transaction per gesture, so
  a drag is one step and not two hundred.
- Effect rack changes: adding, removing, bypassing and reordering effects.
- Automation lane edits, and creating an automation clip.
- Audio-library entries and clip properties.
- Structural changes: adding, deleting and duplicating a tab of any kind. A
  deleted tab comes back with its engine, its sound and its settings, because the
  action carries a full snapshot.
- Loading a page preset, which is one step from the state before to the state
  after.
- Per-tab things like lock / unlock, drum program picks, and choke-group changes.

### What is deliberately not undoable

- **Loading or creating a project, or applying a template.** These clear the
  history rather than joining it - a load is not an edit.
- **Saving.** Saving pins the current position as "clean" without touching the
  history, so you can still undo past a save.
- **Freezing and unfreezing.** These do not change what the song is, only how it
  is played back.
- **File operations** - preset and template files written to disk, exports,
  bundles. Nothing outside the project is rolled back.
- **App preferences** - audio device, keyboard shortcuts, File Settings.

### Depth

The Undo History Size setting is the number of steps kept. When you exceed it, the
oldest step drops off (and its snapshot files are deleted with it).

**The setting resets to 100 every time the app starts.** It is not written to disk.

## Parameters and persistence

The undo history is **session-only**. Nothing about it is saved with a project or
between runs:

| Thing | Where it lives | Survives a restart? |
|---|---|---|
| The history itself | memory | No - cleared on project load and at exit |
| Snapshot XML files | `Documents\BaySickDAW\UndoSnapshots\snap_*.xml` | No - swept at startup, at project load, and at exit |
| The depth setting | memory only | No - always 100 at launch |
| The "clean" point | the processor's transaction tracker | No - reset to zero by every load |

Snapshot files are named `snap_<timestamp>_<counter>.xml`.

## Lifetime and teardown

- The manager is created with the editor and lives for the session. The depth cap
  is set as `setMaxNumberOfStoredUnits(1, N)` - one "unit" per action deliberately,
  so N is honestly the number of steps kept rather than an approximation of memory.
- **Loading is a boundary.** A project load clears the undo history, sweeps the
  snapshot folder and resets the clean point in one step, so the first edit after a
  load is step 1 and the project opens clean.
- Undo and redo flush pending parameter gestures **before** the manager call.
  Without that, a knob you were still touching would leak into the transaction
  created after the undo.
- Programmatic parameter writes - restoring a preset's strip values, applying an
  automation baseline, resetting parameters on File > New - are wrapped so they
  never enter the history. If they did, undoing a preset load would walk back
  through several hundred individual parameter writes.
- Both undo and redo open a missing-file reporting scope covering the whole
  gesture, because a structural action re-enters the spawn paths with a full
  preset and can discover that a sample folder, kit or capture has moved since.
- The history window is created on first use and hidden rather than destroyed when
  closed. It stops listening to the manager when it is finally destroyed.

## Cross-references

- *Projects and Saving* - the unsaved-changes asterisk that undo can clear.
- *Presets* - the "Save Page Preset & Delete" gesture and single-step preset
  loads.
- *Automation* - automation clip creation and lane edits as undo steps.
- *Templates* - why applying a template clears the history.

## Differs from Carry-Forward

Nothing to record. The frozen Carry-Forward snapshot does not cover the undo
system.
