# BaySickRustyDrums

**Purpose** - BaySickRustyDrums is a full sampled acoustic drum kit. Unlike every
other instrument in the app it is a **singleton**: there is one Rusty tab or
none. Its distinguishing feature is that each drum comes out on its own mixer
strip, so you can EQ the snare without touching the kick, exactly as you would
with a real multi-mic recording.

The sound engine is **sfizz**, an SFZ sample-format player. The kit, its samples,
its control panel and its note map all come from a kit installed in the Core
Library - no drum samples ship inside the executable.

---

## How it operates

`BaySickRustyDrumsProcessor`
(`Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h/.cpp`) is a
`juce::AudioProcessor` wrapping one `sfz::Sfizz` instance. It is
**processor-owned** (held by `VibeSynthProcessor`, not by `EngineRig`) and
reached through `getBaySickRustyDrums()`. Its parameter prefix is `brd_`, with no
page index, because there is only ever one.

**Multi-output.** sfizz can route regions to numbered output buses via the
`output=` opcode. At kit-load time `buildOutputRoutedSfzWrapper` reads the
program file, inlines each drum piece's mapping file, and appends `output=N` to
every `<master>` and `<group>` line in it, then loads the result as a string. One
sfizz instance therefore emits one stereo pair per drum piece, which keeps the
kit's native choke groups (`off_by`) working across pieces - something separate
instances could not do. If the wrapper cannot be built the code falls back to a
plain file load and the kit plays summed to a single output.

**Render path.** Two task types share the work:

- `RustyDrumsProducerTask` calls `processStrips()` once per block, filling the
  engine's internal per-piece buffers. It publishes to no channel.
- One `RustyInsertTask` per piece reads its stereo pair back out with
  `getStripBuffer(i)` and runs it through that piece's mixer insert. Each has a
  synthetic dependency on the producer so it always finishes after it.

`getStripRenderSeq()` is bumped inside `processStrips` only. On a block where the
producer was skipped (idle suspend, or a frozen kit) the scratch still holds the
previous block's audio; live playback never notices because nothing reads the
strips on such a block, but the offline freeze render checks the counter so it
does not bake a repeat of the last block into every gap.

**Piece discovery.** `discoverChannels` scans
`<kitRoot>/Programs/mappings/` for `<piece>_map.sfz` master files, excluding
articulation variants (`<piece>_map_<articulation>.sfz`), and matches each stem
against a fixed rule table of thirteen sound types in drummer order:

> Kick, Snare, Tom 22, Tom 18, Tom 15, Tom 14, Hi-hat, Ride 22, Ride Sizzle 19,
> Crash 17, Crash Sizzle 17, China 18, Stack

Masters that map to the same sound type collapse into one channel (both kick
masters become the single "Kick" strip). When a program file is supplied, the
list is then filtered to the pieces that program actually includes. That is what
makes the two programs differ: **Full** resolves to 13 pieces, **Basic** to 8
(Kick, Snare, Hi-hat, Tom 18, Tom 15, Tom 14, Ride 22, Crash 17). Detection is
purely lexical on filenames; the SFZ contents are never parsed for this.

**Note map.** `discoverPianoRollKeymap` parses only
`<kitRoot>/Programs/keymap/keymap.sfz`, one entry per `#define $name <midi>`,
sorted by note. The piano roll speaks **raw kit MIDI** - there is no anchoring
and no remap layer. A curated table turns each internal name into a readable
sound plus articulation ("Snare / Sidestick", "Hi-hat / Half Open Tip"); an
unknown name falls back to a title-cased version of itself.

**Kit loading** (`VibeSynthProcessor::loadBaySickRustyDrumsKit`) keeps the
engine's active flag **false** for the whole load and raises the host processBlock
shield around the entire mutation window - engine creation, prepare, producer-task
registration, the SFZ parse, dispatcher task-list edits and the strip-insert loop.
sfizz mutates internal state for seconds during a parse and has no guard of its
own between that and `renderBlock`. Only when everything is built does the active
flag go true and `onSfizzEngineReady` fire (registering automation lanes for
every parameter).

Like the other sfizz engines, `loadKit` walks the program's `#include` chain to
depth 4 collecting `#define $name value` macros, `set_cc<N>` starting positions
and `label_cc<N>` display names, resets all 512 CC parameters to **0**, then
applies the kit's `set_cc` values on top. That reset is what stops a Full-program
tweak leaking into Basic.

**CC dispatch is queued, never direct.** `sfz::Sfizz::cc()` mutates state
`renderBlock` reads. `parameterChanged` only marks a dirty bit; `processStrips`
drains the marks at the top of the block and calls `cc()` on the audio thread.

**Idle suspend.** The producer skips `processStrips` entirely when MIDI is empty
and the engine reports zero active voices for several consecutive blocks. It
wakes on the first block where any gate fails, including a queued audition
(`isAuditionPending()`).

---

## User-facing behavior

### Getting one

The kit is **not installed with the app**. It lives in the Core Library at:

```
%LOCALAPPDATA%\BaySickDAW\CoreLibrary\Big Rusty Drums\
```

Without the Core Library installed, the tab opens and the Program dropdown
offers its two entries, but picking either one raises a box saying the program
file could not be found, and the tab stays empty. Nothing in the app downloads
the library - it is a separate install.

Open the **Drums** ribbon dropdown and choose **+ Add BaySickRustyDrums**. The
entry disappears once the tab exists, because there can only be one.

A fresh tab opens on the **Player** view, which sits empty until a program is
picked - the Program dropdown lives on the title strip, so that is where you can
see it. Switch to the **Drum Kit** view before loading and the kit photo is
dimmed to half brightness with **"Pick a program to begin"** across it; clicking
a drum does nothing until a program is loaded.

### Loading a program

The **Program** dropdown on the window title strip reads *Load Player* until you
pick one:

| Program | What you get |
|---|---|
| **Full** | The whole kit - 13 drums and cymbals, every articulation, 13 mixer strips. |
| **Basic** | A smaller kit - kick, snare, hi-hat, three toms, ride and crash, 8 mixer strips. Lighter on memory and CPU. |

The first pick loads immediately. **Changing programs afterwards asks first**,
because it is destructive: switching resets all Rusty mixer settings, clears the
Rusty piano roll across **every** pattern, and reloads the kit. Cancel puts the
dropdown back. Either way the whole switch is one undo step, so Ctrl+Z takes you
back - including undoing the very first load, which returns you to the empty
player.

### The three views

The window's **Menu** switches between them:

| View | What it is |
|---|---|
| **Drum Kit** | The clickable kit photo. |
| **Player** | The kit author's ARIA control panel. This is where a new tab lands. |
| **Piano Roll** | A shortcut that jumps to the Piano Roll page with this kit selected. |

### Drum Kit - the clickable photo

A full-bleed photograph of the kit with 25 invisible click zones laid over it -
24 playable articulations plus the hi-hat pedal. The gaps beside the photo are
filled with a rotated copy of the kit's own control artwork rather than black.

- **Hover** shows the articulation name as a tooltip. **Click** plays it, with a
  thin blue ring while the mouse is down.
- **Where you click inside a zone sets how hard you hit it.** Dead center is
  full velocity; toward the edge drops to about 45 percent. On a pressure-
  sensitive pen, pen pressure scales it further.
- Pieces the loaded program does not contain are painted over with a dark
  overlay - on **Basic** that is the Stack, China, Tom 22, Crash Sizzle and Ride
  Sizzle zones.

The zones, and the note each one plays:

| Zone | Note | Zone | Note |
|---|---|---|---|
| Kick | 36 | Ride 22 Bow | 51 |
| Snare Center | 38 | Ride 22 Edge Crash | 52 |
| Snare Edge | 39 | Ride 22 Bell | 53 |
| Snare Rim | 40 | Ride Sizzle 19 Bow | 60 |
| Snare Sidestick | 37 | Ride Sizzle 19 Edge Crash | 61 |
| Tom 14 | 47 | Ride Sizzle 19 Bell | 62 |
| Tom 15 | 45 | Crash 17 | 49 |
| Tom 18 | 43 | Crash Sizzle 17 Bow | 64 |
| Tom 22 | 41 | Crash Sizzle 17 Crash | 65 |
| Hi-hat Tip | 42 closed / 46 open | Crash Sizzle 17 Bell | 67 |
| Hi-hat Shaft | 54 closed / 58 open | China 18 | 57 |
| Stack Mid | 71 | Stack Edge | 72 |

**Hi-hat Pedal** is the twenty-fifth zone and is a toggle, not a hit. It draws a
ring - red and labeled `PEDAL: CLOSED`, or green and `PEDAL: OPEN` - and clicking
flips it. The state drives the kit's pedal-position controller (CC4, sent as 0
for closed and 127 for open) **and** swaps which note the two hi-hat zones play,
so the tip and shaft zones sound tight when the pedal is closed and loose when it
is open.

### Player - the ARIA control panel

The panel is drawn from the kit's own `GUI/<program>.xml`: the kit's background
art, its labels, and one widget per control the kit author exposed. Because the
drum kit ships zoom-in pages, a row of **section tabs** appears in the band above
the artwork:

> Main | Kick | Snare | Toms | Hi-hat | Cymbals | Noises

*Main* is the loaded program's own page; the rest are the kit's per-section
close-ups. Four widget kinds appear:

| Widget | Behavior |
|---|---|
| Knob | Drag up/down. Value shows in a bubble while you drag. |
| Vertical fader | Drag up/down. |
| On/off button | Click toggles between off and on. |
| Dropdown | Click for a menu of the kit's named choices (for example the snare and tom stick/brush/mallet sets). |

Every widget, whatever it looks like, is one MIDI controller value from 0 to 127.

- **Hover** shows the kit author's own name for the control and its current
  value. Drum kits label knobs with terse abbreviations - *Close*, *OH*, *Btm*,
  *Punch*, *Dirt*, *Deaden*, *Snap*, *Buzz*, *Epic* - so the app appends a
  plain-English sentence explaining what each means. For example *OH* is
  explained as the overhead mic that captures the drum from above with the air
  around it, and *Deaden* as reducing the drum's ring-out so it sounds shorter
  and tighter.
- **Double-click** resets to the kit author's intended starting value (0 for a
  control the kit never set).
- **Right-click** offers *Automate*, *Type in value...* and the MIDI Learn items,
  so any kit control can be automated or mapped to a hardware knob.

### The rest of the title strip

| Control | What it does |
|---|---|
| **Menu** | The three view entries, Freeze, and Save / Load Page Preset. |
| **Player Preset** | Save / load the kit's own sound settings only (see below). |
| **Program** | Full / Basic. |
| Swing knob | Per-player swing amount. |

There is no FX Rack button on this strip - a Rusty kit has 13 racks, one per
drum, reached from the Mixer.

### Player Preset vs Page Preset

Two different things, deliberately:

| | What it captures | Where it lives |
|---|---|---|
| **Player Preset** | The kit's sound only: every kit control value plus the engine output level, tagged with the program it was saved on. | `Documents\BaySickDAW\Presets\Rusty Player\My Presets\` |
| **Page Preset** | Everything: the engine, all 13 drum mixer strips, the RustyDrums Bus, and every drum's effects rack with pre and post EQ. Piano-roll notes are not included. | `Documents\BaySickDAW\Presets\Rusty Drums Page\My Presets\` |

A Player Preset saved on Full and loaded while Basic is active will offer to
switch programs first, with the same destructive warning as the dropdown -
without that, the Full-only knob values would be written but have no visible
control behind them.

### Mixer strips

Loading a program spawns one mixer strip per drum, named after the piece
("Kick", "Snare", "Hi-hat", "Tom 14"...), all feeding a dedicated **RustyDrums
Bus**. Each strip has the standard fader, pan, width, mute, solo, polarity,
bypass and its own effects rack. A Rusty strip's main output is **locked** to the
RustyDrums Bus and cannot be re-pointed - but the per-strip "+" send button can
still send it out to an aux strip.

### The note map reference

**Help > Rusty Drums Map...** opens a scrollable table of every kit-native note:
key name, MIDI number, sound, and articulation. It reads the loaded kit when
there is one, and falls back to parsing the installed kit directly when no Rusty
tab has been spawned yet - so you can look the map up before adding the tab. If
the Core Library is not installed the table is empty.

Keyswitch keys are also highlighted and labeled on the piano-roll keyboard
itself, using the kit author's own names.

---

## Parameters and persistence

All parameter ids are prefixed `brd_`. There is no page index - the engine is a
singleton.

| Parameter | Type | Range | Default | Where it appears |
|---|---|---|---|---|
| `brd_outVol` | float | 0 .. 1 | 0.8 | No on-screen control. Automatable, and captured by Player Presets. |
| `brd_cc0` .. `brd_cc511` | int | 0 .. 127 | 0 | Whichever ARIA panel widget the kit maps to that number. Ids above 127 exist because this kit uses extended controllers (for example CC400 and CC401 for its limiter). |

`brd_cc4` is the hi-hat pedal position and is also written by the kit graphic's
pedal toggle, so pedal moves are undoable and saved with the project like any
other control change.

Every parameter gets an automation lane and a MIDI-Learn target, because
registration walks the engine's whole parameter list. The lane id is the
parameter id.

Mixer state uses the standard strip parameter families:
`mixer_rusty_0` .. `mixer_rusty_12` for the drum inserts (channel ids 800-812)
and `mixer_rustybus` for the bus (channel id 12).

**Saved with the project.** A `<BaySickRustyDrumsState>` root holding the APVTS
copy plus a `<KitPath path>` child, written through
`SampleLibrary::refForPersist` so the shipped kit persists as a portable
`library:` reference rather than an absolute path containing your Windows user
name. That blob is stored base64 as the tab record's `engineData`.

**Restore order is load-bearing.** The project and undo paths decode the blob,
resolve the kit path, and route the load through
`BaySickRustyDrumsPage::reloadForProjectRestore` - which loads under the
active-flag guard, spawns the mixer strips, syncs the Program dropdown and
`mCurrentProgram` so a later re-pick is not treated as a switch, and rebuilds
the ARIA panel - **then** applies the saved control values on top of the kit's
own defaults. `reloadForProjectRestore` refuses any kit file that is not
`01-full.sfz` or `02-basic.sfz`, because the page's whole surface (dropdown,
panel, program-tagged presets) is defined over those two only; the caller then
reports it as a missing file. A missing kit at restore leaves the page empty.

**Per machine, not per project.** The Core Library itself and everything in it.

**Not saved at all.** `isHiHatPedalClosed()` mirrors CC4 but is itself transient
state; the piece list and note map, both re-derived from the kit at every load;
the audition atomic; and sfizz's internal voice state.

---

## Lifetime and teardown

- The processor is created lazily on the **first program pick**, not when the
  tab is added. Before that the page exists, the kit photo is dimmed, and clicks
  do nothing.
- `tearDownCurrentProgram` runs in a fixed order and every step matters:
  1. Detach the dirty listener (the engine is about to die).
  2. Reset every `mixer_rusty_*` and `mixer_rustybus_*` parameter to its default.
  3. **Clear the ARIA panel and unbind the kit graphic** - the panel's widgets
     hold parameter attachments rooted in the engine's APVTS and the kit graphic
     holds a raw engine pointer, so destroying them after the engine is gone
     would be a use-after-free.
  4. `destroyBaySickRustyDrums()`, which retracts any frozen sources, removes all
     13 insert nodes and the producer task under the message-thread shield,
     clears the Rusty roll on every pattern, republishes the roll snapshots, and
     drops the engine.
- If a program load fails after the teardown, `mCurrentProgram` is honestly reset
  to none and the dropdown cleared - leaving it on the old value would make both
  dropdown entries dead behind a silent page.
- The freeze system keys the kit as a rig tab (`TabKind::Rusty`, index 0) created
  at kit-load time rather than at page-show, because restoring a saved freeze has
  to work before any page has been opened. A freeze content stamp hashes the
  engine's blob explicitly, since the rig's own blob cannot see a
  processor-owned engine.
- The APVTS carries a stable undo tag (`rusty`) so undo and redo survive the
  engine being rebuilt by a program switch.

---

## Cross-references

- `BaySickGuitars.md` and `BaySickBasses.md` - the other two sfizz engines. They
  share the kit loader, the CC queueing model and the ARIA control panel; they
  differ in being per-tab rather than singleton, and in having a single stereo
  output rather than per-piece strips.
- `BaySickPlayer.md` - the non-sfizz sample player behind ordinary Drums tabs.
  A Drums tab and the Rusty tab are different things: Drums tabs are per-pad
  engines you load your own samples into, Rusty is one fixed multi-mic kit.
- The Core Library and the stable-reference path format are shared
  (`Source/SampleLibrary.h`).

---

## Differs from Carry-Forward

- Carry-Forward's idle-suspend entry lists the audition wake condition as
  **missing** ("contract specified but not implemented", DSP-10). It is
  implemented: `isAuditionPending()` exists and the Rusty producer gate consults
  it.
- Carry-Forward's "Engine audition pattern" row lists 4 engines carrying
  `auditionNote`. There are 7; this one takes a velocity argument
  (`auditionNote(int midiNote, int velocity = 100)`), which is what lets the kit
  graphic turn click position and pen pressure into how hard the drum is hit.
- Carry-Forward describes the RustyDrums Bus solo behavior as standalone
  (`inGroupSolo = false, useGroupSolo = false`) - that is still the case and is
  noted here only because it is the reason soloing a Rusty strip behaves
  differently from soloing a Layer.

---

## Not determined

- The count of hi-hat, snare and tom **articulations** reachable from the piano
  roll is whatever the installed kit's `keymap.sfz` declares; the app parses it
  at load rather than hard-coding it, so this document does not state a number.
  The Help > Rusty Drums Map window is the authoritative list for whatever kit
  is installed.
- `BaySickRustyDrumsProcessor.h` carries a comment saying discovery "resolves to
  14 channels". It does not - the two kick masters collapse into one bucket, so
  Full resolves to 13, which is also the hard cap (`kMaxRustyStrips`). The
  comment is stale; 13 is the verified number.
