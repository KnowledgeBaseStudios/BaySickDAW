# Project Bundles

**Purpose** - A project folder can refer to audio that lives outside it: a sample
you dragged in from your Downloads folder, an amp capture, an impulse response.
Copy the folder to another machine and those references break. A project bundle
packs the project plus the files it depends on into one zip or one folder, so the
whole song travels intact.

## How it operates

`Source/Standalone/ProjectBundler.cpp/.h` is a free-standing utility with two
halves - a walker and a writer - and the dialog that drives it is
`StandaloneEditor::doExportProjectBundle`
(`Source/Standalone/StandaloneEditor.cpp`). It does file I/O and allocates, so it
runs on the message thread, never on the audio thread.

**The walk.** `enumerate` collects every file reference the project depends on
from four places:

1. The project's audio library (the Builder browser's clip list).
2. Every arrangement block that carries an audio path.
3. Every dynamic tab, using the same `<Tabs>` element a project save produces.
   This is how engine-held references are found: clip paths and sampler kit paths
   are plain attributes, an Inst chain's state is an XML document stored in a
   string attribute, and a sampler's own sample path sits inside a base64 engine
   blob. The walker descends through all of it, up to four levels of nesting
   (rack, slot, chain, pedal), decoding both base64 forms in use.
4. Every mixer effect rack, because effects that hold user files (the acoustic
   units' user impulse responses) can sit in any bus or insert rack and live
   under the project's `<Processor>` element rather than under `<Tabs>`.

Each reference is classified by where it actually resolves:

| Kind | Meaning |
|---|---|
| ProjectRelative | Already inside the project folder - travels for free. |
| UserSamples | Inside `Documents\BaySickDAW\My Samples`. |
| CoreLibrary | Shipped factory content. |
| Absolute | Anywhere else on this machine. |
| Missing | The stored path resolves to nothing. |

References are de-duplicated on the stored path, so a file used by twenty
arrangement blocks is copied once and reported once.

**The write.** Copying a file relocates it, so the bundle's own `project.xml` is
**rewritten** to point at the new location - including any " (2)" rename forced by
a filename clash. Without that rewrite the copies would sit in the bundle
referenced by nothing and the receiving machine would report every one of them
missing. A rewrite that could not be completed is reported rather than passing as
a clean export.

Two clash rules keep the copies honest. If a file with the same name is already in
the bundle's `Samples\` folder *and* has the same size and modification time, it
is treated as the same asset and reused. If it is a genuinely different file, the
new copy gets a " (2)" name rather than silently overwriting the first.

## User-facing behavior

**File > Export Project Bundle...**

If the project has never been saved, the standard Save As flow runs first and the
bundle continues once the save succeeds. Canceling anywhere aborts the bundle.

The dialog has two dropdowns:

| Control | Options | Default | What it means |
|---|---|---|---|
| Bundle as | Single .zip file / Plain folder | Single .zip file | A zip is one file you can email or upload. A folder is easier to poke around in. |
| Contents | Project files only (smallest) / Include my samples + outside files | Project files only | Whether files from outside the project folder travel with it. |

**Project files only** copies nothing extra - the project folder is packed as it
stands. Use it when you are just archiving a project on the same machine.

**Include my samples + outside files** additionally copies anything in `My Samples`
and anything from elsewhere on your disk that the project refers to. This is the
one to pick when the bundle is going to someone else.

Neither option copies the **Core Library**. It is installed separately from the
app and lives outside the project, and copying it would drag hundreds of
megabytes of factory instruments into every bundle. The consequence is worth
saying plainly: a bundle that uses factory content plays correctly only on a
machine where the Core Library is also installed. If it is not, the receiving
machine reports the missing files on open (see `Sample Library.md`).

Next you pick where to save. Then, if the bundle is going to copy any audio at
all, a confirmation appears first:

> This bundle will copy 412.6 MB of audio alongside the project.
>
> Continue?

so a large export is a decision rather than a surprise.

When it finishes you get a report:

> Bundle written to:
> `<path>`
>
> Extra files copied: 14

and, when something did not make it, one or both warnings, each listing up to 10
entries with an "...and N more" tail:

- **N referenced file(s) could not be found and are NOT in the bundle** - the
  project points at something that is no longer on this machine at all. Fix the
  reference and export again, or accept that the bundle is missing that sound.
- **N file(s) could not be copied and are NOT in the bundle** - the file exists
  but the copy failed (permissions, disk space, a file in use).

A clean bundle shows the information icon; anything reported shows the warning
icon.

**What is left out on purpose.** The `Freeze\` folder is excluded from every
bundle. It is regenerable audio - roughly 16 MB per minute per frozen track - and
the receiving machine re-renders it when it needs it, so shipping it would bloat
every bundle for nothing.

**What you get.** A folder bundle is a copy of the project folder with a `Samples\`
folder holding the copied audio and a rewritten `project.xml`. A zip is the same
thing compressed. Either one is opened on the far end the way any project is
opened - the folder itself is the project.

## Parameters and persistence

The bundler has no settings of its own and stores nothing. The two dropdown
choices are not remembered between exports.

The bundle's own `project.xml` differs from the source project's in exactly one
respect: every reference the export relocated is rewritten to `Samples/<filename>`
(forward slash, matching how the project already stores its own relative paths).
References that were already project-relative, or that point into the Core
Library, are left exactly as they were.

Attribute names the walker looks at are listed explicitly rather than guessed, so
that a path-shaped string that is not a file reference is never reported as
missing: the sampler's sample/folder/SFZ path, sampler kit paths, the amp
capture and cabinet impulse paths for both amp slots, the per-slot and global mic
impulse paths, the pedal capture path, the acoustic units' user impulse, and an
arrangement block's audio path. Three element types also contribute their generic
`path` attribute: `KitPath`, `Sample` and `Entry`.

## Lifetime and teardown

The bundler is a set of free functions - nothing to construct or destroy. Ordering
that matters:

- In the zip branch the copy pass runs **first**, because its renames decide what
  the bundled `project.xml` has to say and a zip entry cannot be rewritten once it
  has been added.
- The rewritten `project.xml` is written to a temporary file that must outlive the
  zip write; the zip substitutes it for the original as it packs the project
  folder.
- In the folder branch the whole project folder is copied first and the freeze
  cache is deleted from the copy afterwards.
- A zip whose write fails is deleted rather than left as a truncated archive.
- The walk takes a fresh `<Tabs>` element and a fresh rack-state snapshot at
  export time, so it always reflects what is in the app right now, not what was
  last written to disk.

## Cross-references

- *Projects and Saving* - the project folder layout the bundle is built from.
- *Sample Library* - stable references, and why Core Library and `My Samples`
  files are treated differently.
- *Freeze and Export* - the `Freeze\` cache that bundles deliberately leave behind.
- *Templates* - the other place a file reference is copied to make it portable.

## Differs from Carry-Forward

Nothing to record. The frozen Carry-Forward snapshot predates project bundles.
