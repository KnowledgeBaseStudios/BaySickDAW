# Sample Library

**Purpose** - Two places hold the audio you build songs from. The **Core Library**
is BaySickDAW's factory sound content - drum kits, keys, strings, brass, and the
rest - and it is **installed separately from the application**. **My Samples** is
your own folder, for anything you have downloaded or recorded that you want
reachable from every project. Both are treated as "stable" locations, which is
what lets a project refer to a sample by name rather than by a full path that
breaks the moment anything moves.

## How it operates

`Source/SampleLibrary.h/.cpp` owns all of it. It is a singleton, scanned once at
startup (a directory walk, typically under 10 ms).

**The two roots:**

| Root | Path | Contents |
|---|---|---|
| Core Library | `%LOCALAPPDATA%\BaySickDAW\CoreLibrary\` | Factory content. **Installed separately from the application** - the app never creates this folder, it only reads it, and every feature that depends on it degrades gracefully when it is absent. Deliberately outside your Documents folder: it is large, and there is no reason to back it up alongside your work. |
| My Samples | `Documents\BaySickDAW\My Samples\` | Yours. Created at startup if missing, with a `Core Library.lnk` shortcut placed inside so you can reach factory content from any file picker without leaving the dialog. |

A `Sample Library.lnk` shortcut to the Core Library is also placed in
`Documents\BaySickDAW\` on first launch. It is created once and never re-created,
so deleting it makes it stay gone.

**Library structure.** Each top-level folder inside the Core Library is a **pack**.
Inside a pack, a subfolder is a sample folder and a `.sfz` file is an SFZ
instrument. A pack is classified as **drums** if its folder name contains "Drums"
or "Percussion" (case-insensitive, anywhere in the name); everything else is
**melodic**. That one test is the single source of truth, used both by the scan
and by the in-app browsers, so a Drums page's browser shows only drum packs and a
Layer or Bass page's browser shows only melodic ones.

**Stable references.** Rather than storing an absolute path, anything under a
stable root is persisted as a short reference:

```
library:Hip Hop Drums Package/Kicks/Kick_01.wav
mysamples:my vocal chop.wav
```

Forward slashes always, whatever the platform. `refForPersist` decides which form
to write (stable reference when it can, absolute path otherwise) and
`resolvePersistedRef` reverses it and still accepts plain absolute paths, so
older projects keep loading. Every place that writes a file path to disk goes
through that pair rather than calling `getFullPathName` itself.

An absolute path embeds your Windows user name and cannot resolve under another
account. A stable reference resolves on any install.

**Copy or reference?** `ProjectManager::importSample` decides:

- A file **already under a stable root** is referenced, never copied. It is
  reachable from any project on this install, so copying it would duplicate
  shipped content into every project that touched it.
- A file from anywhere else - Downloads, Desktop, a USB stick - is **copied into**
  `<project>\Samples\` and the project stores `Samples/<filename>`. That is the
  case the copy exists for: those sources can vanish.

Filename clashes inside `Samples\` are resolved by comparing size and modification
time. Same size and time means the same asset and the copy is skipped; anything
else gets " (2)", " (3)" and so on. Nothing is ever overwritten.

## User-facing behavior

### Where the app points you

Every file picker that wants audio opens at **My Samples**, and the Core Library
shortcut inside it is one click from factory content. That covers **File > Import
Audio...**, the ribbon's **+ Add New Clip**, and the sample browse items on
instrument pages.

### Browsing factory content in-app

Instrument pages carry a **Core Library** submenu that walks the packs directly, so
you can audition shipped instruments without opening a file dialog:

- A **Drums** tab's sound picker shows **Browse sample folder...**, **Load SFZ
  file...**, then a **Core Library** submenu containing only drum packs, then the
  factory drum presets.
- A **Layers** or **Bass** tab's sampler shows the same shape with only melodic
  packs.

Each pack becomes a submenu, and each sample folder or `.sfz` inside it becomes an
item.

### What happens to a sample you drop in

Drag a WAV onto the arrangement, or add it as a clip, and:

- If it came from **My Samples** or the **Core Library**, the project just points
  at it. Nothing is copied and nothing gets bigger.
- If it came from anywhere else, it is **copied into your project's `Samples`
  folder**. From that point the project owns its own copy, and moving or deleting
  the original does not affect the song.

Adding a clip when no project exists yet prompts you to create one first, and
explains why: "Your audio file will be copied into that project's Samples folder
automatically."

### Adopting a sample into My Samples

Two gestures copy a file *into* `My Samples` on your behalf:

- **Save as Template.** A template is one loose XML file with no folder beside it,
  so anything it refers to from outside a stable root is copied into `My Samples`
  and the template refers to the copy instead. Without that, a template pointing at
  your Downloads folder breaks the first time you clean it out.
- **Saving a Clips page preset.** The clip's audio is copied into `My Samples` and
  the preset stores its name, so the preset travels between projects with its
  sound.

Adoption rules, in both cases:

| Source | What happens |
|---|---|
| Already under Core Library or My Samples | Nothing is copied - the existing reference is kept. Factory content in particular is never duplicated; a single sampler product folder can be most of a gigabyte. |
| A bare `.sfz` file | Never copied. An SFZ is a pointer into a surrounding sample tree, so copying the file alone would give you a reference to nothing. It stays an absolute reference, and the bundler reports it if it ever goes missing. |
| An identical file already adopted | Reused. Matching is on size and modification time, and it is checked against every already-suffixed copy, not just the base name - otherwise each re-save would clone the audio again and orphan the previous copy. |
| A different file with the same name | Copied under " (2)". A file in `My Samples` is never overwritten. |

If the save is then canceled at the name-collision prompt, or fails, the copies
that save just made are removed again. Copies from an earlier save are left alone.

### If a sample goes missing

Loading a project that refers to a file which is no longer there raises one
**Missing files** dialog listing everything that could not be found, with the
advice to re-pick them on the relevant tab or put the files back. The parts that
depend on them load without them, so they may be silent - or, for some engines,
may be playing a substitute you never picked.
### If a sample cannot be read

Separately from a file being absent, every audio file you point the app at is
checked for plausibility before anything reads it: at most 32 channels, 1 to 64
bits per sample, a sample rate above zero and no higher than 768000, and a single
frame no wider than 5760 bytes. A file whose header fails any of those is dropped
exactly as an unreadable file is, and **no reason is produced** - the check has
no error channel at all.

What you see therefore depends on where the file was going. A Drums or Clips tab
says "Nothing playable could be loaded from:" and the path. A BaySickPlayer pick
says "No playable samples could be loaded from:", the path, then "It may be
damaged, or the samples it needs may be missing." An Acoustic Preamp or Acoustic
Simulator IR pick says "This file could not be read as audio:" and the path. An
audio file dropped on the arrangement says nothing at all - the clip is placed at
a fallback four bars.

## Parameters and persistence

The sample library has no parameters. What it persists is the **form of a stored
path**, which appears throughout the project file and inside preset and template
files:

| Stored form | Resolves against |
|---|---|
| `library:<relative path>` | `%LOCALAPPDATA%\BaySickDAW\CoreLibrary\` |
| `mysamples:<relative path>` | `Documents\BaySickDAW\My Samples\` |
| `Samples/<filename>` | The current project folder |
| An absolute path | Itself - used when a file is under none of the above |

Resolution order matters and is fixed: stable references are tested **first**,
because they are not absolute paths and would otherwise be treated as
project-relative and resolve to nonsense inside the project folder.

Per machine, nothing about the library is saved beyond two one-shot flags in
`settings.xml`: `shortcutCreated` (so the `Sample Library.lnk` shortcut is not
re-created after you delete it) and `migratedFromRoaming` (a one-time move of
pre-existing user data into the Documents tree).

The library scan itself is not cached to disk - it is re-walked at every launch.
Files added to the Core Library while the app is running are not picked up until
the next launch.

## Lifetime and teardown

- `SampleLibrary` is a singleton with no teardown. `scan()` is called once at
  editor construction; `ensureUserSamplesDir()` is called at startup and again
  before any gesture that is about to write there, and is a fast no-op when
  everything already exists.
- Copying a **folder** into `My Samples` plants a hidden marker file
  (`.baysick-adopt-incomplete`) before the first child is copied and removes it
  when the copy finishes. A folder copy that fails part-way leaves files behind, so
  "the folder exists" cannot mean "the folder is complete" - the marker is what
  tells the two apart, and a marked folder is redone rather than blessed on the
  next save. If the leftovers cannot even be deleted, the marker is re-planted so
  the stump stays flagged.
- A **file** copy that fails part-way is left as an orphan and is never
  referenced: it fails the size-and-time test, so the next save suffixes past it.
- The path resolver is installed on the processor when the current project folder
  is set, which happens **before** any engine reads a stored reference during a
  load. Engines and effects reach it through a free function because most are
  built by a factory that holds no processor reference.
- Existing per-project copies made before the reference-instead-of-copy rule
  arrived are left exactly as they are. Nothing migrates them.

## Cross-references

- *Projects and Saving* - the `Samples\` folder inside a project, and the
  missing-files dialog.
- *Project Bundles* - how each kind of reference is treated when a project is
  packed to travel.
- *Templates* - the adoption that happens on a template save.
- *Presets* - the Clips page preset's audio copy.

## Differs from Carry-Forward

Nothing to record. The frozen Carry-Forward snapshot does not cover the sample
library.
