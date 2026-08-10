# Templates

**Purpose** - A template is a starting point: every tab you want, each with its
sound already picked, the mixer levels and routing set, and the effects and EQ in
place - but with no music in it. Start a new song from a template and you are
straight into writing instead of setting up. BaySickDAW ships a set of premade
templates and lets you save your own.

## How it operates

Templates are single XML files under `Documents\BaySickDAW\Templates\`, split into
`Factory\` (shipped) and `My Templates\` (yours). The code lives in
`Source/Standalone/StandaloneEditor.cpp` (`templatesDir`, `factoryTemplatesDir`,
`userTemplatesDir`, `loadTemplate`, `applyTemplate`, `saveTemplateAs`).

There are **two on-disk formats**, and both still load:

- **v2** - what `Save as Template...` writes. It is deliberately the *project*
  shape minus the music: a `<Processor>` child (the entire parameter tree plus
  every effect rack - identical to what a project save emits) and a `<UIState>`
  child holding the structural half only (tabs with their engine state, strip
  names and orders). Because it reuses the project shape, every tab type
  round-trips and the loader is the project restore path rather than a second
  implementation.
- **v1 factory** - the shipped set. Attribute-only: a `<Kit path>` element and a
  list of `<Layer>` / `<Bass>` elements each naming an engine, a preset path and
  a locked flag.

`loadTemplate` is the single gate every template pick goes through. It checks the
file exists, then runs the unsaved-changes prompt, then calls `applyTemplate`.
Putting the gate here rather than on each menu entry means anything added to the
template menu later inherits it automatically.

`applyTemplate` raises the project-load shield (audio renders silence during the
rebuild), clears the previous project's aux inserts, and then branches:

- **v2** behaves as a new project: close every dynamic tab, clear dynamic mixer
  strips, reset the processor to blank, then apply the template's `<Processor>`
  and `<UIState>` on top. The previous song - patterns, notes, arrangement, audio
  library - goes with it. A template is a rig you start from, not a layer you
  merge.
- **v1** rebuilds only Layers, Bass and Drums, and deliberately leaves Clips, Vox,
  Inst and Rusty Drums tabs alone, because it cannot restore them. It loads the
  named kit, then spawns one tab per `<Layer>` / `<Bass>` entry in order, then
  selects the first new Layer tab.

Both branches finish by refreshing the kit views, rebuilding the Effects page
channel list, lowering the shield, and reporting any missing files under the
heading "template" (so a broken reference reads as a fault in the template you
just picked, not in your own project).

## User-facing behavior

### Starting from a template

**File > New from Template** opens a submenu with three parts:

| Item | What it does |
|---|---|
| New from Default Template (`<name>`) | Loads whatever you set as your default. Grayed out with no name when you have not set one, and grayed with " - missing" appended when the file you chose has since been moved or deleted. |
| Premade Templates | The shipped set. Subfolders appear as cascading submenus; a folder with nothing loadable in it is not shown. |
| My Templates | Your own saved templates, same folder-nesting behavior. |

If either section is empty you see a disabled "(no premade templates)" or "(no
user templates)" row rather than an empty menu.

Picking any template first raises the standard **Unsaved changes** prompt if you
have work that is not on disk. Cancel there and nothing happens at all.

A template file that is missing raises "Load Template - The template file is
missing: `<path>`". A file that is not a BaySickDAW template, or is damaged,
raises "Could not load template: '`<name>`' is not a BaySickDAW template file (or
is damaged)."

**File > New Project always starts blank.** Your default template is applied only
by the dedicated "New from Default Template" item - a plain New never applies it.

### Saving your own template

**File > Save as Template...** asks for a name and explains what it captures:

> Templates save your whole setup - every tab and its sound, the mixer levels,
> routing, effects and EQ - but no patterns or arrangement, so you start writing
> on a blank canvas.

The name goes through the shared save helper, so an existing name raises the
**Replace / Save a Copy / Cancel** prompt (see *Presets*), and an unusable name is
rejected before anything is written to disk.

Because a template is one loose file with no folder beside it, anything it refers
to from outside a stable location is **copied into `My Samples`** as part of the
save, and the template then refers to the copy. Without that, a template that
points at a file in your Downloads folder silently breaks the moment you clean
that folder out. Two exceptions: Core Library content is never copied (it is
already in its own stable location on machines that have it installed, and a
sampler product folder can be hundreds of megabytes), and a bare `.sfz` file is never copied either (it is a pointer into a
sample tree, so copying it alone would produce a reference to nothing).

If you cancel at the collision prompt or the save fails, any copies that save
just made into `My Samples` are removed again. A copy that was already there from
an earlier save is left alone.

### Choosing a default template

**Options > General** carries:

| Item | Behavior |
|---|---|
| Set Default Template... | Opens a file picker in the Templates folder. When one is set, the menu item shows the current choice in brackets. |
| Clear Default Template | Grayed out until one is set. |

The choice is remembered across restarts.

### What a template does and does not carry

| Carried | Not carried |
|---|---|
| Every tab, its engine and that engine's full settings | Patterns and the notes in them |
| Mixer strip levels, pan, width, mute, solo, polarity, routing, sends | The arrangement |
| Every effect rack and both EQ banks on every strip | The audio library and any clips |
| Strip names and their display order | Frozen-track state and freeze files |
| Whether a tab is locked | Which tab was active, window positions, scroll and selection, metronome settings, VU calibration, song loop |
| | MIDI Learn bindings and drum trigger bindings |

The session extras in the right-hand column are properties of a *session*, not of
a skeleton you start new songs from, so the template save leaves them out on
purpose. Freeze state is left out for a harder reason: the render files live
inside a project folder, and a template applied to a brand-new project has no
project folder yet.

## Parameters and persistence

Template files live at `Documents\BaySickDAW\Templates\Factory\` and
`...\My Templates\`, one `.xml` per template, and are per machine - a template is
not carried inside a project.

**v2 file shape:**

```
<BaySickTemplate name="..." version="2">
  <Processor>   the full APVTS tree + every rack's state
  <UIState version="1">
    <Tabs>      one <Tab> per dynamic tab: type, pageIndex, name,
                engine, base64 engine state, per-kind extras
    strip name and order elements
</BaySickTemplate>
```

**v1 factory file shape:**

```
<BaySickTemplate name="..." version="1">
  <Kit   path="TR-808/TR-808 Full.xml"/>       relative to Kits\Factory\
  <Layer slot="N" engine="X" presetPath="..." locked="1"/>
  <Bass  slot="N" engine="X" presetPath="..." locked="1"/>
</BaySickTemplate>
```

`presetPath` is relative to `Documents\BaySickDAW\Presets\`. A v1 entry whose
preset is missing still spawns its tab, at engine defaults, and the missing preset
is listed in the missing-files dialog as "Template preset".

The default-template pointer is stored as a `defaultTemplate` attribute in
`Documents\BaySickDAW\settings.xml`. Nothing is copied when it is set - it is a
path to a template file that is loaded through the same path as any other pick.

A third format used to exist: v1 *user* templates, written with inline per-tab
state that the loader never read back. Those never round-tripped, and v2
supersedes them outright - there is no loader for them.

## Lifetime and teardown

Templates are files; there is no runtime object to tear down. The ordering that
matters is inside the load:

- The unsaved-changes gate runs **before** anything is torn down. On a clean or
  blank project it runs its continuation immediately, so those load with no
  prompt; Cancel simply never runs it.
- `applyTemplate` raises the audio shield before the rebuild and restores the
  previous shield state at the end (rather than clearing it), so a template load
  nested inside another load does not lower the shield early.
- In the v2 branch, clearing the dynamic mixer strips must happen **before** the
  processor state is applied. It resets the aux / vox / inst index counters, and
  running it afterwards would rewind them past inserts the state apply had just
  registered.
- Per-insert effect racks are replayed later in the restore, after the audio
  strips exist. Replaying them earlier consumes the stash before the targets are
  built, and the racks come back empty.
- The sample adoption in `Save as Template` runs **before** the write (the file
  has to carry the adopted references), which is why canceling has to undo it.

## Cross-references

- *Projects and Saving* - the project shape a v2 template reuses, and the
  unsaved-changes prompt.
- *Presets* - the name-collision prompt, and the per-tab equivalent of a template.
- *Sample Library* - `My Samples`, stable references, and what "adopting" a sample
  means.
- *Freeze and Export* - why freeze state is excluded from templates.

## Differs from Carry-Forward

Nothing to record. The frozen Carry-Forward snapshot does not cover templates.
