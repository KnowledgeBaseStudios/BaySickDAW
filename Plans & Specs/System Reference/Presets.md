# Presets

**Purpose** - A preset is a saved sound you can recall later or use in another
song. BaySickDAW has four separate preset families because there are four
different-sized things you might want to keep: a whole instrument tab, one
instrument's own patch, one effect's knob settings, or a whole channel's effect
chain. All four live as XML files under `Documents\BaySickDAW\Presets\`, so you
can back them up, copy them between machines, or hand one to a friend.

## How it operates

Four modules, each owning one family:

| Family | Module | Root tag |
|---|---|---|
| Page preset | `Source/Standalone/PagePresetIO.cpp/.h` | `<BaySickPagePreset version="2">` |
| Patch (engine-native) | each page's own `savePatchAs` (e.g. `Source/Standalone/LayersPage.cpp`) | the engine's own state tag |
| Effect preset | `Source/Standalone/EffectPresetIO.cpp/.h` | `<EffectPreset version="1">` |
| FX rack preset | `Source/Standalone/FxRackPresetIO.cpp/.h` | `<BaySickFxRackPreset version="1">` |

All naming, collision handling and write-failure reporting for the page-preset,
patch and rack families goes through one helper, `Source/UserFileSave.h`. It turns
the typed name into a legal filename, decides whether a file of that name already
exists, raises the collision prompt, performs the write, and reports failure with
one consistent wording. Because the collision prompt is a modal dialog, the write
completes asynchronously - callers hand it a completion callback and re-check that
their page still exists before doing anything else.

Page presets capture through the live processor: every engine slot on the page
(`getStateInformation`), every parameter whose id begins with the page's mixer
strip prefix, the page's insert effect rack with both its EQ banks, and - for
pages that own a dedicated bus - that bus's rack too. Piano-roll notes are
deliberately excluded: those are music, not sound design.

Loading a page preset rewrites parameter ids from the saved page index to the
destination page index, so a preset saved on Layer 1 loads onto Layer 4. Routing
destinations that no longer exist in this project (a secondary bus that is not
active) fall back to their natural parent bus, and a drum strip's main output is
re-pointed at the drum bus for the bank the destination page is on.

A page-preset load is **not** atomic. If it fails part-way (a damaged engine blob,
a kit that will not load), the slots already applied keep their new state and
nothing is rolled back.

## User-facing behavior

### Where presets live on disk

```
Documents\BaySickDAW\Presets\
+-- <Kind> Page\My Presets\*.xml     page presets: Layer, Bass, Drum, Clip,
|                                    Vox, Inst, Rusty Drums, Plugin
+-- <EngineName>\                    patches for one engine
|   +-- <factory bundle folders>\    shipped patches, grouped in folders
|   \-- My Presets\*.xml             your own patches
+-- Effects\<EffectName>\
|   +-- Factory\*.xml                shipped effect presets
|   +-- My Presets\*.xml             your own
|   \-- Default.xml                  your "Save Current as Default"
+-- Effects\Pedals\<PedalName>\      same three, one level deeper
+-- Effects\Pedals\User NAM Pedals\  amp captures you have added
+-- Effects\IR\                      impulse responses you have added
\-- FX Rack\My Presets\*.xml         whole-channel effect chains
```

### 1. Page presets - "save this whole tab"

Every instrument tab's **Menu** has **Save Page Preset As...** and a **Load Page
Preset** submenu listing what you have saved for that kind of tab.

A page preset holds everything that makes that tab sound the way it does: the
engine and all its settings, the mixer strip (level, pan, width, mute, solo,
polarity, routing and sends), the strip's whole effects rack, and both EQ banks.
It does not hold the notes you wrote.

Loading a page preset onto a different tab of the same kind is expected and
supported.

### 2. Patches - "save just this instrument's sound"

Layers and Bass tabs also have **Save Patch As...** and a **Load Preset**
submenu; a Drums tab carries the same pair inside its sound picker as **Save
Current Patch As...**. This saves only the engine, in that engine's own file
format, so it also shows up in the engine's own preset picker on the tab's title
strip. The Load Preset submenu shows the shipped factory folders as cascading
submenus alongside your own **My Presets**.

Loading a patch renames the tab to the preset's filename.

If a patch refers to a sample that is not on this machine, the settings are still
applied and you get: "This preset's sample is missing from this machine: ... The
preset's settings were applied but it will not make sound."

### 3. Effect presets - "save this one effect"

Reached from the effect's own window: **Menu > Presets...**. (On the Vox tab's
Vocal Chain window the six stages are stacked inline instead, and each slot
header carries its own **Preset** button that opens the same menu; the
pedalboard's per-pedal "..." button is the same idea again.) The menu:

| Item | What it does |
|---|---|
| Save Current Preset... | Asks for a name and writes it to that effect type's **My Presets** folder. |
| Load: Factory | The shipped presets for this effect type. |
| Load: My Presets | Your own. |
| Restore Defaults | Puts every knob back to the effect's built-in starting values. No file involved. |
| Save Current as Default | Writes `Default.xml` for this effect type. |
| Manage Presets... (open folder) | Opens the folder in Windows Explorer. |

Effect presets are the one family that does **not** ask about name collisions: if
"Big Hall" already exists, saving again writes "Big Hall (2)" rather than
prompting. Your existing preset is never overwritten.

Loading an effect preset also restores which *mode* the effect is in (Compressor
Modern / FET / Opto, Saturation Tube / Console / Tape, and so on), and the panel is
rebuilt so you see the right controls for it.

### 4. FX rack presets - "save this channel's whole chain"

The **Effects** page Menu carries **Save FX Rack Preset...** ("Saves all six slots
and both EQs for this channel"), a **Load FX Rack Preset** submenu, and **Open
Presets Folder**.

The Save box pre-fills the channel's own display name, so re-saving a channel's
rack proposes the same name every time - which is exactly the case the collision
prompt exists for. Rack presets are not tied to a channel: save the master's
chain and load it onto a vocal.

Loading a rack preset replaces all six slots and both EQ banks on the currently
selected channel. Any automation lanes that pointed at the old effects will no
longer find their target - the lane data is kept, but it drives nothing until you
re-point it.

### The name-collision prompt

When you save under a name that already exists (page presets, patches, rack
presets, templates and kits), you get **Name Already Used**:

> "Big Hall.xml" is already saved in `<folder>`.
>
> Replace it with what you have now, or save a copy named "Big Hall (2).xml"?

| Button | Result |
|---|---|
| **Replace** | Overwrites the existing file. |
| **Save a Copy** | Writes the suffixed name instead. Both files survive. |
| **Cancel** | Writes nothing, and shows no error - canceling is a decision, not a failure. |

Escape is the same as Cancel.

If the name you typed has no usable characters left in it after illegal
characters are removed, you get **Save failed** with either "Type a name for it
first." (empty box) or the full list of characters to avoid:
`/ \ : ; , ? * # @ ^ | < >` and quote marks. If the write itself fails you get
"Couldn't write `<path>`."

### "Save Page Preset & Delete"

Deleting an instrument tab whose sound has changed since it was last saved or
loaded raises a three-button prompt instead of the usual two:

- **Save Page Preset & Delete** - runs the normal Save Page Preset naming flow
  first, and deletes the tab only after the file is actually on disk. If you
  cancel at the naming box or at the collision prompt, or the write fails, the tab
  is **not** deleted, and any failure message says so.
- **Delete** - deletes without saving.
- **Cancel** - does nothing.

A tab whose sound has not changed gets the plain two-button Delete / Cancel
prompt, because there is nothing to save.

"Changed" here means the engine's current state differs byte-for-byte from the
snapshot taken when the tab last loaded or saved a preset.

## Parameters and persistence

Preset files are **per machine**. They are not carried inside a project, and they
are not copied by Save As. A project stores whatever the preset put into it, not a
reference back to the preset file.

| Family | What is inside the file |
|---|---|
| Page preset | `<Engines>` (one base64 state blob per engine slot on the page, tagged with the slot's label and parameter prefix, plus a `<KitPath>` for sampler-backed engines), `<Strips>` (every strip parameter as `id` + natural-units value), `<Racks>` (the page's insert rack and, where applicable, its bus rack, each with both EQ banks). `pageType` and, for Inst pages, `sourceMode` sit on the root so the loader can set the source before instantiating anything. |
| Patch | The engine's own state document. For sampler engines it also carries a `<Sample kind path>` pair; `kind` is written as "none" when there is no sample so the reader never names a blank file. |
| Effect preset | `version`, `effectType`, `name`, and `blob` - the effect's own state, base64-encoded. The `name` attribute is always the *resolved* filename, so a suffixed copy does not display as its own twin. |
| FX rack preset | `<Rack data>` (the whole six-slot rack state, base64) plus `<Eq prefix>` with one `<Param id v>` per EQ parameter. EQ ids are stored as the *suffix* after the strip prefix, which is what lets the preset load onto a different channel. |

Effect presets are seeded on first launch from a built-in table, and re-seeded
when the seed version advances. Your **My Presets** folder is never touched by
seeding - if you delete a *factory* preset it comes back on the next launch, and
if you delete one of your own it stays deleted.

## Lifetime and teardown

Preset files are ordinary files with no in-app lifetime. The things worth knowing
are about the *gestures*:

- A save that can raise a collision prompt completes long after the call that
  started it. The page might be closed by then, so every completion re-checks
  that its page still exists before touching it. Work that must happen regardless
  (copying a clip's audio into `My Samples`) is done before that check.
- The Clips page-preset save copies the clip's audio into `My Samples` only in
  the success branch. Canceling at the collision prompt leaves no orphan file.
- A page-preset load drains its own missing-file report, so a preset that refers
  to a file you no longer have raises the dialog headed "preset", not attached to
  whatever you load next.
- Effect folders are created on demand (`ensureFolderTree`) - saving into a type
  you have never saved for creates its Factory and My Presets folders first.

## Cross-references

- *Templates* - the whole-app equivalent: every tab and the mixer, saved as one
  starting point.
- *Projects and Saving* - where a loaded preset's settings end up living.
- *Sample Library* - why a preset's sample path can survive being moved, and how
  the Clips preset copies its audio.
- *Automation* - what happens to automation lanes when a preset replaces the
  effect they pointed at.
- *Undo History* - loading a page preset is a single undoable step.

## Differs from Carry-Forward

Nothing to record. The frozen Carry-Forward snapshot does not cover the preset
families.
