# Projects and Saving

**Purpose** - A BaySickDAW project is the container for one song: every tab, every
sound, the mixer, the arrangement, and the audio you dropped in. It is a folder on
disk, not a single file, so a project can carry its own audio alongside its
settings. This system owns creating, opening, saving, backing up and restoring
that folder, and it owns the unsaved-changes marker that tells you whether your
work is on disk yet.

## How it operates

Three owners split the work:

- `Source/ProjectManager.cpp/.h` (`ProjectManager`) owns the folder: create,
  open, save, Save As, delete, rename, duplicate, the recent list, the autosave
  timer and the backup files. It holds one `juce::File` for "the current project
  folder" - empty means no project is open.
- `Source/PluginProcessor.cpp` (`BaySickDAWProcessor::serializeProject` /
  `deserializeProject`) owns the XML document itself.
- `Source/Standalone/StandaloneEditor.cpp` owns the File menu, every dialog, and
  the `<UIState>` half of the document (tabs, windows, view state) which it
  contributes through the `onSerializeUIState` / `onDeserializeUIState` callbacks.

`Source/AppPaths.h` is the single authority for the user-data root. Everything
below is relative to `Documents\BaySickDAW\`:

```
Documents\BaySickDAW\
+-- Projects\<name>\
|   +-- project.xml            the whole project
|   +-- Samples\               audio copied into the project
|   +-- Backups\               autosave copies (newest 10)
|   +-- Exports\               rendered audio (created on demand)
|   +-- Reports\               loudness reports (created on demand)
|   \-- Freeze\                frozen-track cache (regenerable)
+-- My Samples\                your own sample folder (+ Core Library shortcut)
+-- Presets\  Templates\  Kits\  Recordings\  UndoSnapshots\
+-- settings.xml               recent projects + app preferences
+-- audio_settings.xml         this machine's audio device
+-- ui_prefs.xml               dialog preferences
+-- keymap.xml                 keyboard shortcuts
\-- MidiMappings.xml           global MIDI Learn defaults
```

**Saving.** `serializeProject` builds one `<BaySickDAWProject version="1">`
element and `ProjectManager::saveProject` writes it to
`<project>\project.xml`. Children, in order: `<Processor>` (the whole APVTS
parameter tree plus every effect rack's state), `<PatternManager>` (patterns,
notes, arrangement, audio library, tempo and time-signature maps),
`<DenoiseProfiles>` (only when any exist), `<MidiCCMappings>`, `<DrumTriggers>`,
and `<UIState>`.

**Loading.** `deserializeProject` raises the project-load shield (the audio
thread renders silence while it runs), waits for the in-flight audio block to
finish, tears down the previous project's aux inserts, applies `<Processor>`,
loads `<PatternManager>`, replaces the de-noise profiles, overlays the MIDI
mappings, replaces the drum triggers, fires the editor's UI rebuild, lowers the
shield, then reports missing files. Everything runs on the message thread.

**The unsaved marker.** Dirty is not a flag that any edit sets. The processor
keeps a `TransactionTracker` (`Source/PluginProcessor.h`) holding two numbers: a
current transaction count and the count at the last save. Dirty means "these two
disagree". Undoing back past the save point therefore makes the project clean
again, and redoing forward makes it dirty again. Loading a project resets both to
zero and clears the undo history.

**Missing files.** Engines that persist a path to an external file (an amp
capture, a sampler kit, an impulse response, a clip's audio) record a miss
instead of failing silently (`Source/MissingFileReport.h`). One dialog is raised
at the end of the load listing everything that could not be found.

## User-facing behavior

### Creating and opening

| Control | Where | What it does |
|---|---|---|
| New Project... | File menu, Ctrl+N | Asks for a name, creates `Documents\BaySickDAW\Projects\<name>\`, and saves an empty project into it. Starts completely blank - no tabs are created for you. |
| New from Template | File menu submenu | Starts from a saved rig instead of an empty one. See *Templates*. |
| Open Project... | File menu, Ctrl+O | A file browser pointed at your Projects folder. |
| Quick Open Project... | File menu | A built-in list of your projects with Name / Last Modified / Size columns, sortable, with a right-click menu: Open, Rename..., Duplicate..., Delete, Show in Explorer. Rename and Delete are disabled for the project you currently have open. |
| Open Recent | File menu submenu | Your last 10 projects, newest first. A project whose folder has been moved or deleted still appears, grayed out and marked "(missing)". "Clear Recent Projects" empties the list. |

**Project names** follow Windows filename rules. `< > : " / \ | ? *` are rejected,
as are the reserved device names (CON, PRN, AUX, NUL, COM1-9, LPT1-9), control
characters, and names longer than 255 characters. Trailing dots and spaces are
trimmed off. An invalid name raises "Invalid project name" and re-opens the
naming box so you can try again. If a folder with that name already exists, the
app appends " (2)", " (3)" and so on rather than overwriting anything.

### Saving

| Control | Where | What it does |
|---|---|---|
| Save | File menu, Ctrl+S | Writes `project.xml` in place. If you have never named the project, this behaves as Save As. |
| Save As... | File menu, Shift+Ctrl+S | Asks for a new name, copies the whole current project folder (so your `Samples\` folder comes with it), then writes the current state into the copy. From that point on you are working in the new project; the old one is left exactly as it was at its last save. |

If a save cannot be written you get "Save failed" naming the reason, and the app
stays on the project you were already on - it never silently switches you to a
folder it could not create.

### The unsaved-changes marker and prompt

The window title reads `BaySickDAW - <project name>`, with ` *` appended while
there are unsaved changes. Before you have named a project it reads
`BaySickDAW - Untitled *`. Debug builds add ` [DEBUG]`.

Anything that would throw work away - New, Open, Quick Open, Open Recent, loading
a template, quitting - first raises **Unsaved changes** with three buttons:

- **Save** - saves (asking for a name first if you have never named it) and then
  continues.
- **Don't Save** - continues and loses the changes.
- **Cancel** - does nothing at all.

### Autosave and backups

Autosave runs **every 15 minutes** and writes whether or not there are unsaved
changes. Backups land in `<project>\Backups\` named
`<project>_backup_YYYY-MM-DD_HH-MM.xml`; the **newest 10** are kept and older ones
are deleted. If no project is open yet, backups go to
`Documents\BaySickDAW\Backups\Unsaved\` under the stem "Untitled", so a session
you never named can still be recovered after a crash.

A backup is a full copy of the project state, but it is *not* a save - the
asterisk stays up and your own Save is still pending.

**File > Restore from Backup...** lists the available backups newest-first with
their timestamp and a friendly age ("just now", "12 min ago", "3 hr ago", "2 days
ago"). Picking one warns that current unsaved changes will be replaced and that
any audio deleted from the project folder since the backup was taken will still be
missing. On confirm, the backup is copied over `project.xml` (the file it replaces
is kept as `project.xml.before-restore`) and reloaded. With no project open, the
backup is loaded into memory only and you must Save to commit it.

If a backup or the settings file cannot be written, you get a one-per-session
warning naming the folder and telling you to check disk space and permissions.

### Other File-menu items

- **Import Audio...** opens a file picker (defaulting to `My Samples`) and places
  the file on the arrangement.
- **Export Audio...** and **Export Project Bundle...** - see *Freeze and Export*
  and *Project Bundles*.
- **Save as Template...** - see *Templates*.

### The missing-files dialog

After a load that could not find something, one **Missing files** box appears:

> This project refers to files that are no longer where they were saved.
> The affected parts loaded without them, so they may be silent or may be playing
> a substitute:

...followed by up to 12 entries as `<what>: <path>`, an "...and N more" line if
there are more, and "Re-pick them on the relevant tab, or put the files back."
The word "project" is replaced by "template" or "preset" when that is what was
being loaded. Reports raised in the same moment are merged into a single dialog.

## Parameters and persistence

**Saved with the project (`project.xml`):**

| Element | Contents |
|---|---|
| `<Processor>` | The full APVTS parameter tree: every mixer strip's level, pan, width, mute, solo, polarity, bypass, arm, routing and sends; both 8-band M/S EQ banks per strip; the global parameters (master gain, master FX bypass, pan law, the four snap/quantize divisions). Plus `<BaySickRackStates>` - every bus and insert effect rack. |
| `<PatternManager>` | Patterns and their notes, the arrangement, row names / mute / solo / grouping / colors, the audio library, time markers, tempo changes and time-signature changes, and automation lane data. |
| `<DenoiseProfiles>` | Per-recording noise profiles, keyed by recording name. Written only when some exist; cleared on every load so one project never inherits another's. |
| `<MidiCCMappings>` | MIDI Learn bindings. Overlays the global defaults rather than replacing them. |
| `<DrumTriggers>` | Per-drum pad/key bindings. A project with no such element clears them. |
| `<UIState>` | Every dynamic tab with its engine state, window positions and which windows were open, the active tab, Builder zoom / scroll / time selection, drum bank, piano-roll selection, metronome settings, VU calibration, meter latency compensation, song loop, strip names and orders, bus activation, and per-tab freeze state. |

**Saved per machine, not with the project:**

| File | Contents |
|---|---|
| `settings.xml` | Recent-projects list, the default-template pointer, the "don't ask again" flags for the two drum-kit prompts, remembered window sizes for the four default tabs, recent colors, the multi-core rendering toggle, and the MIDI trigger velocity source. |
| `audio_settings.xml` | The audio device, sample rate and buffer size. Changes made in the Audio Settings dialog are staged to `audio_settings_pending.xml` and applied on restart. |
| `ui_prefs.xml` | File Settings dialog values (take types written at record stop, de-noise strength, auto-freeze CPU threshold, capture retention, "keep the audio of each take", "Enable Instrument Level Freeze") and the export loudness spec. |
| `keymap.xml` | Your keyboard shortcut rebindings. |
| `MidiMappings.xml` | Global MIDI Learn defaults. |

**Not saved at all:** the undo history (and its snapshot files), the frozen-track
audio cache in `<project>\Freeze\` (regenerable, and rebuilt on load when it is
still valid), and the undo depth setting.

## Lifetime and teardown

`ProjectManager` is constructed once with a reference to the processor, reads
`settings.xml`, and starts its 15-minute autosave timer. Its destructor stops the
timer. It lives as long as the app.

Ordering that matters:

- `setCurrentProjectFolder` is called **before** the state is deserialized, so
  every relative path stored in the project resolves correctly while engines are
  restoring.
- Opening a project stops the transport first (through the
  `onBeforeOpenProject` hook, wired by the editor), so a project loaded during
  playback halts cleanly instead of streaming half-built state to the audio
  thread.
- Every load entry point wipes the previous session first: close all dynamic
  tabs, clear dynamic mixer strips, reset the processor to a blank state, then
  load. Skipping that step leaks the previous project's tabs and effects into the
  new one.
- The project-load shield is nest-aware. An inner teardown that raises it leaves
  it raised for the outer load rather than lowering it early.
- `File > New` resets every registered parameter to its default, clears every
  rack slot, clears the drum trigger bindings and the de-noise profiles, and puts
  PatternManager back to one empty pattern.

## Cross-references

- *Presets* - saving one page, one effect or one rack rather than the whole song.
- *Templates* - the starting-point rigs `File > New from Template` loads.
- *Project Bundles* - packing a project up to send somewhere else.
- *Freeze and Export* - the `Freeze\` and `Exports\` folders inside a project.
- *Undo History* - why the asterisk can clear when you undo.
- *Sample Library* - how audio paths are stored so they survive being moved.
- *MIDI Learn* - the `<MidiCCMappings>` and `<DrumTriggers>` elements.

## Differs from Carry-Forward

- Carry-Forward section 2 records that `ProjectManager::openProject()` does **not** stop
  the playhead (item STATE-04, listed open in section 5). It does now: `openProject`
  fires an `onBeforeOpenProject` callback and the editor stops the transport and
  clears the play button before any load work begins.
- Carry-Forward section 5 lists STATE-01 and STATE-02 as open. Load-time dirty
  suppression now extends past `openProject` into the follow-up restore work via
  `isLoadingProject` / `setIgnoreDirty`, and the dirty flag itself is no longer a
  boolean - it is the transaction-pointer comparison described above.
