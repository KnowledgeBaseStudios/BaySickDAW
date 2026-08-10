# BaySickBasses

**Purpose** - BaySickBasses is a deeply sampled electric-bass instrument. It is
one of the two sound sources an Inst tab can use instead of a live microphone or
DI input, and it plays from the piano roll. Like every Inst tab, its output feeds
that tab's pedalboard and amp/cabinet modeler, so the sampled bass is the front
of a chain rather than the finished tone.

The sound engine is **sfizz**, an SFZ sample-format player. The programs, samples
and the whole control panel come from a kit installed in the Core Library - no
bass samples ship inside the executable.

---

## How it operates

`BaySickBassesProcessor` (`Source/BaySickBasses/BaySickBassesProcessor.h/.cpp`)
is a `juce::AudioProcessor` wrapping one `sfz::Sfizz` instance. One processor
exists per Inst tab in Basses mode, each with its own parameter prefix
`bbb_<instIdx>_`, so several basses can be open at once. The implementation is
the twin of `BaySickGuitarsProcessor`; the differences are the prefix, the kit,
and a bass-only mono-choke behavior noted below.

**Chain position.** `InstPage::rebuildEngineChain` builds the tab's stage list.
In Basses mode it is:

```
BaySickBasses (sfizz)  ->  BaySickPedals  ->  BaySickNAM/IR  ->  Inst mixer strip
```

The tab's registered engine is an `EngineChainProcessor` wrapper whose
`processBlock` runs those stages in order.

**Ownership.** The sfizz trio (Guitars, Basses, RustyDrums) is
**processor-owned**, not owned by `EngineRig` - it lives in `VibeSynthProcessor`
and is reached through `getBaySickBasses(pageIndex)`. The page never caches the
pointer; a kit load can replace the instance. Teardown order matters:
`rig.removeTab` runs **before** `destroyBaySickBasses`, because the rig-owned
chain holds the spliced sfizz stage pointer.

**Kit loading** (`VibeSynthProcessor::loadBaySickBassesKit`):

1. Clear the tab's active flag and the engine's own processing gate.
2. Raise the host processBlock shield and settle the audio thread, so no render
   is inside sfizz while its regions and file pool are freed.
3. Create the processor if this is the first load; hand it the transport playhead
   and prepare it.
4. `BaySickBassesProcessor::loadKit` parses the SFZ.
5. Re-open the processing gate, lower the shield, set the active flag, and fire
   `onSfizzEngineReady`, which registers an automation lane for every parameter.

Step 2 is load-bearing: sfizz has no internal guard between `renderBlock` and
`loadSfzFile`, so the shield is the only thing keeping the two apart.

`loadKit` also walks the program's `#include` chain to a depth of 4, collecting
`#define $name value` macros, `set_cc<N>=<int>` starting positions, and
`label_cc<N>=<text>` display names. All 512 CC parameters are reset to **0**
first, then the collected `set_cc` values are applied on top. The reset stops a
previous program's values leaking into a program that never set that control;
a control the kit leaves unset means **off**, not half-on.

**CC dispatch is queued, never direct.** `sfz::Sfizz::cc()` mutates state that
`renderBlock` reads, with no lock inside sfizz. `parameterChanged` therefore only
sets a bit in `mCcDirty`; `processBlock` drains the bits at the top of the block,
reads the value from a cached parameter atom, and calls `mSfizz->cc()` itself on
the audio thread. Last write wins.

**Slide and Bend.** The processor owns a `SlideSampler`
(`Source/SlideSampler/`) - a separate voiced sampler that produces continuous
pitch slides by picking the sample at or just below the current pitch and bending
it up by at most a semitone, crossfading zones at semitone boundaries. `loadKit`
extracts the program's blended-slide table and hands it over. The piano roll
drives it through a CC transport that `processBlock` intercepts and never
forwards to sfizz:

| CC | Meaning |
|---|---|
| 84 | Slide anchor note |
| 5 + 37 | Glide time in milliseconds (14-bit pair) |
| 85 | Slide target note - this is what arms the gesture |
| 86 | Fallback loudness when the anchor was never struck |
| 87 | Bend amount in semitones, offset by 64 |
| 88 | Bend shape |

**Bass-only mono choke.** When the loaded program declares a mono set and the
kit's Mono control (CC105) is at or above 64, starting a new slide gesture chokes
whatever is already sounding through the patch's own off time. That happens
independently of the Cut Self buttons.

A Bend note ramps sfizz's own pitch wheel, scaled to the patch's real bend range
read at load. Unlike the guitar kit, the bass patch reports a usable range in
both directions, so bends up **and** down are offered. Every other controller,
note, pitch-wheel and aftertouch message passes through to sfizz untouched.

**Idle suspend.** The dispatcher skips this tab's whole chain when its MIDI is
empty and it reports no activity. Three predicates keep that from swallowing
sound: `getNumActiveVoices()`, `isAuditionPending()` (a queued keyboard click),
and `isSlideActive()` (slide voices and ring-out tails do not count in sfizz's
voice total).

---

## User-facing behavior

### Getting one

The instrument's content is **not installed with the app**. It lives in the Core
Library at:

```
%LOCALAPPDATA%\BaySickDAW\CoreLibrary\Black&Blue Basses\
```

Without the Core Library installed, the tab still opens but no program loads:
the panel shows the placeholder text *"Loading control surface..."* instead of
the kit's knobs, the tab keeps its plain `Basses N` name, and the instrument
makes no sound. Nothing in the app downloads the library - it is a separate
install.

Open the **Inst** ribbon dropdown and choose **+ Add BaySickBasses**. That spawns
the tab, its mixer strip, and immediately loads a default program
(`01-darkblack_keysw.sfz`). The tab is first named `Basses N` and then renamed to
the loaded program's display name. Inst tabs share a cap of 30 pages across
live-input, Guitars and Basses; the menu entry grays out when it is reached.

The piano roll opens with the view scrolled to the bass register (top note
MIDI 48) so the playable range is on screen without hunting for it.

### The window

The window's title strip shows **BaySickBasses** centered in navy, and carries:

| Control | What it does |
|---|---|
| **Menu** | Opens **Pedals** and **NAM/IR** in their own windows, jumps to the **Piano Roll**, and holds Lock / Rename / Duplicate / Save + Load Page Preset / Delete Inst. |
| Program name label | The program currently loaded, prettified (`05-darkblack_pluck.sfz` reads as *Darkblack Pluck*). Reads `(no program loaded)` when empty. |
| **Load Bass** | Opens the program picker. |
| FX Rack button | Jumps to this tab's effects rack. |
| Swing knob | Per-player swing amount. |

The body of the window is the **ARIA control panel** - the kit author's own
graphics and controls. Two small buttons overlay its top-left corner:

| Control | Default | What it does |
|---|---|---|
| **CUT SELF** | off | When on, each new note cuts what is already sounding. |
| **SAME PITCH / CUT ALL** | SAME PITCH | What Cut Self cuts. *Same Pitch* only cuts the note you just retriggered. *Cut All* silences everything on every new note. Keyswitch presses are exempt either way. |

### Picking a program

**Load Bass** lists every `.sfz` in the kit's `Programs` folder, sorted, with a
tick on the one loaded. The shipped kit has eleven programs covering two
different basses, each with a warmer tone variant, plus fixed-articulation
programs (pluck, ghost note, staccato, behind-the-bridge) and keyswitched
programs that let you change articulation from the keyboard.

Switching is not destructive and asks no questions. Your knob tweaks on the
program you are leaving are remembered for the session and come back if you
switch back; the first visit to a program shows the kit author's defaults. The
tab, its mixer strip and the piano-roll label all follow the program name.

If the picker cannot load your choice, a box names the file and nothing changes.

### The ARIA control panel

The panel is drawn from the kit's own `GUI/<program>.xml`: its background art,
its labels, and one widget per control the kit author exposed. Four widget kinds
appear:

| Widget | Behavior |
|---|---|
| Knob | Drag up/down. Value shows in a bubble while you drag. |
| Vertical fader | Drag up/down. The bass kit uses these for depth-style controls. |
| On/off button | Click toggles between off and on. |
| Dropdown | Click for a menu of the kit's named choices. |

Every widget, whatever it looks like, is one MIDI controller value from 0 to 127.

- **Hover** shows the kit author's own name for the control and its current
  value, plus a plain-English explanation for a set of common terms.
- **Double-click** resets to the kit author's intended starting value (0 for a
  control the kit never set).
- **Right-click** offers *Automate*, *Type in value...* and the MIDI Learn items,
  so any kit control can be automated or mapped to a hardware knob.

Bass kits ship one panel page per program, so no section tab row appears.
(Drum kits ship several and get one - see `BaySickRustyDrums.md`.)

### Playing it

- Clicking a key on the piano-roll keyboard auditions that note.
- On a keyswitched program the keyswitch keys are highlighted on the roll
  keyboard and labeled with the kit author's own names.
- The note properties panel on a Basses roll offers three note types instead of
  the usual four:

| Note type | What you hear |
|---|---|
| **Flat** | A normal note. |
| **RP Slide** | Slides from the previous note into this one, keeping the string ringing instead of re-plucking it. |
| **Bend** | Bends the string over the note, up or down. |

  A Bend note adds two dropdowns: **Bend** (the size in semitones, offered only
  within the loaded patch's real bend range) and **Shape** - *Ramp + Hold* (rises
  in about 120 ms then holds), *Ramp (whole)* (rises across the whole note),
  *Up + Back* (bends up then returns), and *Instant* (jumps straight to the bent
  pitch).
  A yellow notice on the panel says it plainly: RP Slide and Bend move **every**
  playing note together, so they suit single-note lines, not chords.

### When the kit is missing

If a saved project references a kit file no longer on this machine, the tab
loads the shipped default program instead of falling silent, marks itself as
substituted (visible on the ribbon tab and mixer strip), and the missing file is
listed in the missing-files dialog under **Bass kit**. The marker is display only
and deliberately not saved, so reinstalling the kit clears it.

---

## Parameters and persistence

Every parameter id is prefixed `bbb_<instIdx>_`.

| Parameter | Type | Range | Default | Where it appears |
|---|---|---|---|---|
| `outVol` | float | 0 .. 1 | 0.8 | No on-screen control. Automatable. |
| `cc0` .. `cc511` | int | 0 .. 127 | 0 | Whichever ARIA panel widget the kit maps to that number. Ids above 127 exist so kit "extended CCs" bind the same way. |
| `cutSelf` | bool | - | false | CUT SELF button |
| `cutSelfMode` | bool | - | false (Same Pitch) | SAME PITCH / CUT ALL button |

All of them - including `outVol` and every CC - get an automation lane and a
MIDI-Learn target, because registration walks the engine's whole parameter list
rather than a hand-kept table. The lane id is the parameter id.

**Saved with the project, twice over.**

1. The processor's own blob: a `<BaySickBassesState>` root holding the APVTS copy
   plus a `<KitPath path>` child. The path is written through
   `SampleLibrary::refForPersist`, so a kit under the Core Library persists as a
   portable `library:` reference instead of an absolute path containing your
   Windows user name.
2. Project-level attributes on the tab record: `source` (`BaySickBasses`),
   `kitPath` (the same stable reference), `sfizzEngineData` (the blob above,
   base64), and a `<ProgramStateCache>` child holding every program's tweaked
   values so program switching round-trips across a save.

**Restore order is load-bearing:** the kit loads **first** (stamping the kit's
`set_cc` defaults over everything), then the saved state is applied on top so
your edits win. The engine's own `setStateInformation` implements that order, but
the project and undo paths deliberately do their own inline decode instead of
calling it, because they need the processing-gate protection the
`loadBaySickBassesKit` wrapper provides.

**Saved with a Page Preset.** `Save Page Preset As...` captures the whole Inst
chain - the sfizz engine (with a kit-load callback so the preset restores its
program), the pedalboard, the NAM/IR stage, this tab's mixer strip and its
effects rack - into
`Documents\BaySickDAW\Presets\Inst Page\My Presets\<name>.xml`.

**Per machine, not per project.** The Core Library itself and everything in it.

**Not saved at all.** The kit-substituted marker, the audition atomic, live
slide/bend gesture state, and sfizz's internal voice state.

---

## Lifetime and teardown

- The processor is created lazily on the **first kit load** for that page index,
  not when the tab is added - `loadBaySickBassesKit` constructs it if the slot is
  empty. Before that, the tab's chain has no sfizz stage and produces silence.
- `destroyBaySickBasses(instIdx)` clears the tab's active flag first (the audio
  thread reads the flag before touching the engine), then frees the instance
  under a spin lock.
- **Ordering rule:** `rig.removeTab` runs before `destroyBaySickBasses`. The
  rig-owned chain holds the spliced sfizz stage pointer, and one audio block in
  the wrong order is a use-after-free.
- The ARIA panel's widgets hold parameter attachments rooted in the engine's
  APVTS. Any path that frees the engine must clear the panel first.
- The processing gate (`setProcessingEnabled`) is the engine's own guard: while
  false, `processBlock` clears its buffer, drops incoming MIDI and returns
  rather than rendering against a half-parsed kit. CC marks made during the load
  accumulate and flush on the first block after the gate reopens.
- The APVTS carries a stable undo tag (`sfizz:bbb_<instIdx>_`) so undo and redo
  survive the engine being rebuilt by a program switch.
- Deleting the tab warns that it also removes the tab's pedalboard, NAM/IR,
  mixer strip, effects rack and the audio-library entries for anything recorded
  on it. Audio files in the project's Samples folder stay on disk.

---

## Cross-references

- `BaySickGuitars.md` - the sibling engine, same architecture and the same Inst
  page cap. Read it for the shared details written out at length.
- `BaySickRustyDrums.md` - the third sfizz engine, and the one that uses the
  ARIA panel's section tab row and multi-output strips.
- `BaySickPlayer.md` - the non-sfizz sample player used by Layers, Bass, Drums
  and Clips tabs. Note that a "Bass" tab (BaySickPlayer or a synth) is a
  different thing from a "Basses" tab (this engine, on an Inst page).
- **Pedalboard.md** and **NAM Amp and Cab.md** - the two stages that follow this
  engine in the chain.
- **Inst Page.md** - the tab that hosts all three.

---

## Differs from Carry-Forward

- Carry-Forward's idle-suspend entry lists the audition wake condition as
  **missing** ("contract specified but not implemented", DSP-10). It is
  implemented: `isAuditionPending()` exists on all three sfizz engines and the
  Inst and Rusty gates consult it. `isSlideActive()` was added to the same gate
  later, for slide ring-out tails.
- Carry-Forward's "Engine audition pattern" row lists 4 engines carrying
  `auditionNote`. There are 7; this one takes a velocity argument as well
  (`auditionNote(int midiNote, int velocity = 100)`).
- Source comments in `BaySickBassesProcessor.h` still say "up to 20 instances
  coexist". The real cap is `kMaxInstPages` = **30**, shared with live-input
  Inst tabs and BaySickGuitars.
