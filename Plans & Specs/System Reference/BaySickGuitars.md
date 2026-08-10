# BaySickGuitars

**Purpose** - BaySickGuitars is a deeply sampled electric-guitar instrument. It
is one of the two sound sources an Inst tab can use instead of a live microphone
or DI input, and it plays from the piano roll. Because it is an Inst tab, its
output runs straight into that tab's pedalboard and amp/cabinet modeler, so a
guitar program is only the front of the chain, not the finished tone.

The sound engine is **sfizz**, an SFZ sample-format player. The instrument's
programs, samples and its whole control panel come from a kit installed in the
Core Library - the app ships no guitar samples inside the executable.

---

## How it operates

`BaySickGuitarsProcessor` (`Source/BaySickGuitars/BaySickGuitarsProcessor.h/.cpp`)
is a `juce::AudioProcessor` wrapping one `sfz::Sfizz` instance. One processor
exists per Inst tab that is in Guitars mode, each with its own parameter prefix
`bgg_<instIdx>_`, so several guitars can be open at once.

**Chain position.** `InstPage::rebuildEngineChain` builds the tab's stage list.
In Guitars mode it is:

```
BaySickGuitars (sfizz)  ->  BaySickPedals  ->  BaySickNAM/IR  ->  Inst mixer strip
```

The registered engine for the tab is an `EngineChainProcessor` wrapper whose
`processBlock` runs those stages in order. A live-input Inst tab is the same
chain with the sfizz stage absent.

**Ownership.** Unlike most engines, the sfizz trio (Guitars, Basses, RustyDrums)
is **processor-owned**, not owned by `EngineRig` - it lives in
`VibeSynthProcessor` and is reached through `getBaySickGuitars(pageIndex)`. The
page never caches the pointer; every call re-queries, because a kit load can
replace the instance. Teardown order matters: `rig.removeTab` must run **before**
`destroyBaySickGuitars`, because the rig-owned chain holds the spliced sfizz
stage pointer.

**Kit loading** (`VibeSynthProcessor::loadBaySickGuitarsKit`):

1. Clear the tab's active flag and the engine's own processing gate.
2. Raise the host processBlock shield and settle the audio thread, so no render
   is inside sfizz while its regions and file pool are freed.
3. Create the processor if this is the first load; hand it the transport playhead
   and prepare it.
4. `BaySickGuitarsProcessor::loadKit` parses the SFZ.
5. Re-open the processing gate, lower the shield, set the active flag, and fire
   `onSfizzEngineReady` (which registers automation lanes for every parameter).

Step 2 is load-bearing. sfizz has no internal guard between `renderBlock` and
`loadSfzFile`; the shield is the only thing keeping the two apart.

`loadKit` also walks the program's `#include` chain to a depth of 4, collecting:

- `#define $name value` macros, so `set_cc` lines that reference a macro resolve
  to the kit author's number;
- `set_cc<N>=<int>` - the kit's intended starting position for control N;
- `label_cc<N>=<text>` - the kit author's display name for control N.

Every one of the 512 CC parameters is first reset to **0** and then the collected
`set_cc` values are written on top. The reset matters because switching programs
would otherwise leak the previous program's values into a program that never set
that control. A control the kit leaves unset means **off** (sfizz's natural 0),
not half-on.

**CC dispatch is queued, never direct.** `sfz::Sfizz::cc()` mutates state that
`renderBlock` reads, with no lock inside sfizz. So `parameterChanged` (which
fires on whatever thread wrote the parameter - a knob drag, the automation
applicator, a project restore) only sets a bit in `mCcDirty`. `processBlock`
drains those bits at the top of the block, reads the current value from a cached
parameter atom, and calls `mSfizz->cc()` itself. Last write wins; a re-pushed
identical value is harmless.

**Slide and Bend.** Alongside sfizz the processor owns a `SlideSampler`
(`Source/SlideSampler/`) - a separate voiced sampler that produces continuous
pitch slides by picking the sample at or just below the current pitch and bending
it up by at most a semitone, crossfading zones at semitone boundaries. `loadKit`
extracts the program's blended-slide table (`extractSlideRegions`) and hands it
over. The piano roll drives it through a small CC transport that `processBlock`
intercepts and never forwards to sfizz:

| CC | Meaning |
|---|---|
| 84 | Slide anchor note |
| 5 + 37 | Glide time in milliseconds (14-bit pair) |
| 85 | Slide target note - this is what arms the gesture |
| 86 | Fallback loudness when the anchor was never struck |
| 87 | Bend amount in semitones, offset by 64 |
| 88 | Bend shape |

A Bend note instead ramps sfizz's own pitch wheel, scaled to the patch's real
bend range (read from the program, defaulting to 200 cents when the program does
not declare one). Every other controller, note, pitch-wheel and aftertouch
message passes through to sfizz untouched.

**Idle suspend.** The dispatcher skips this tab's whole chain when its MIDI is
empty and it reports no activity. Three separate predicates keep that from
swallowing sound: `getNumActiveVoices()`, `isAuditionPending()` (a queued
keyboard click), and `isSlideActive()` (slide voices and ring-out tails do not
count in sfizz's voice total).

---

## User-facing behavior

### Getting one

The instrument's content is **not installed with the app**. It lives in the Core
Library at:

```
%LOCALAPPDATA%\BaySickDAW\CoreLibrary\Black&Green Guitars\
```

If the Core Library is not installed, the tab still opens but no program loads:
the panel shows the placeholder text *"Loading control surface..."* instead of
the kit's knobs, the tab keeps its plain `Guitar N` name, and the instrument
makes no sound. Nothing in the app downloads the library - it is a separate
install.

Open the **Inst** ribbon dropdown and choose **+ Add BaySickGuitars**. That
spawns the tab, its mixer strip, and immediately loads a default program
(`01-green_keyswitch.sfz`). The tab is named `Guitar N` and then renamed to the
loaded program's display name (for example *Green Keyswitch*). Inst tabs share a
cap of 30 pages across live-input, Guitars and Basses; the menu entry grays out
when it is reached.

### The window

The window's title strip shows **BaySickGuitars** centered in navy, and carries:

| Control | What it does |
|---|---|
| **Menu** | Opens **Pedals** and **NAM/IR** in their own windows, jumps to the **Piano Roll**, and holds Lock / Rename / Duplicate / Save + Load Page Preset / Delete Inst. |
| Program name label | The program currently loaded, prettified (`03-Clean_Chorus.sfz` reads as *Clean Chorus*). Reads `(no program loaded)` when empty. |
| **Load Guitar** | Opens the program picker. |
| FX Rack button | Jumps to this tab's effects rack. |
| Swing knob | Per-player swing amount. |

The body of the window is the **ARIA control panel** - the kit author's own
graphics and knobs, the same panel the kit shows in any SFZ host. Two small
buttons are overlaid on its top-left corner:

| Control | Default | What it does |
|---|---|---|
| **CUT SELF** | off | When on, each new note cuts what is already sounding. |
| **SAME PITCH / CUT ALL** | SAME PITCH | What Cut Self cuts. *Same Pitch* only cuts the note you just retriggered. *Cut All* silences everything on every new note. Keyswitch presses are exempt either way. |

### Picking a program

**Load Guitar** lists every `.sfz` in the kit's `Programs` folder, sorted, with
a tick on the one loaded. The shipped kit has eleven, each a different guitar
and playing style - keyswitched programs that let you change articulation from
the keyboard, plus fixed-articulation programs (twang, staccato, hammer-on,
behind-the-bridge) and combination programs.

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
| Vertical fader | Drag up/down. Used for depth-style controls. |
| On/off button | Click toggles between off and on. |
| Dropdown | Click for a menu of the kit's named choices. |

Every widget, whatever it looks like, is one MIDI controller value from 0 to 127.

- **Hover** shows the kit author's own name for the control and its current
  value. For a set of common drum and mic terms the app appends a plain-English
  sentence explaining what the control does.
- **Double-click** resets to the kit author's intended starting value (0 for a
  control the kit never set).
- **Right-click** offers *Automate*, *Type in value...* and the MIDI Learn items,
  so any kit control can be automated or mapped to a hardware knob.

Guitar kits ship one panel page per program, so no section tab row appears.
(Drum kits ship several and get one - see `BaySickRustyDrums.md`.)

### Playing it

- Clicking a key on the piano-roll keyboard auditions that note.
- On a keyswitched program the keyswitch keys are highlighted on the roll
  keyboard and labeled with the kit author's own names, so you can see which key
  changes to which articulation without leaving the roll.
- The note properties panel on a Guitars roll offers three note types instead of
  the usual four:

| Note type | What you hear |
|---|---|
| **Flat** | A normal note. |
| **RP Slide** | Slides from the previous note into this one, keeping the string ringing instead of re-plucking it. |
| **Bend** | Bends the string up (or down, where the instrument allows it) over the note. |

  A Bend note adds two dropdowns: **Bend** (the size, in semitones, offered only
  within the loaded patch's real bend range) and **Shape** - *Ramp + Hold* (rises
  in about 120 ms then holds), *Ramp (whole)* (rises across the whole note),
  *Up + Back* (bends up then returns), and *Instant* (jumps straight to the bent
  pitch).
  A yellow notice on the panel says it plainly: RP Slide and Bend move **every**
  playing note together, so they suit single-note lines, not chord bends.

### When the kit is missing

If a saved project references a kit file that is no longer on this machine, the
tab loads the shipped default program instead of falling silent, marks itself as
substituted (the ribbon tab and mixer strip show it), and the missing file is
listed in the missing-files dialog under **Guitar kit**. The marker is display
only - it is deliberately not saved, so reinstalling the kit clears it.

---

## Parameters and persistence

Every parameter id is prefixed `bgg_<instIdx>_`.

| Parameter | Type | Range | Default | Where it appears |
|---|---|---|---|---|
| `outVol` | float | 0 .. 1 | 0.8 | No on-screen control. Automatable. |
| `cc0` .. `cc511` | int | 0 .. 127 | 0 | Whichever ARIA panel widget the kit maps to that number. Ids above 127 exist so kit "extended CCs" bind the same way. |
| `cutSelf` | bool | - | false | CUT SELF button |
| `cutSelfMode` | bool | - | false (Same Pitch) | SAME PITCH / CUT ALL button |

All of them - including `outVol` and every CC - get an automation lane and a
MIDI-Learn target, because the registration walks the engine's whole parameter
list rather than a hand-kept table. The lane id is the parameter id.

**Saved with the project, twice over.**

1. The processor's own blob, a `<BaySickGuitarsState>` root holding the APVTS
   copy plus a `<KitPath path>` child. The path is written through
   `SampleLibrary::refForPersist`, so a kit under the Core Library persists as a
   portable `library:` reference rather than an absolute path containing your
   Windows user name.
2. Project-level attributes on the tab record: `source` (`BaySickGuitars`),
   `kitPath` (the same stable reference), `sfizzEngineData` (the blob above,
   base64), and a `<ProgramStateCache>` child holding every program's tweaked
   values so program switching round-trips across a save.

**Restore order is load-bearing:** the kit loads **first** (which stamps the
kit's `set_cc` defaults over everything), then the saved state is applied on
top, so your edits win. The engine's own `setStateInformation` implements that
order, but the project and undo paths deliberately do their own inline decode
instead of calling it, because they need the processing-gate protection that the
`loadBaySickGuitarsKit` wrapper provides.

**Saved with a Page Preset.** `Save Page Preset As...` captures the whole Inst
chain - the sfizz engine (with a kit-load callback so the preset can restore its
program), the pedalboard, the NAM/IR stage, this tab's mixer strip and its
effects rack - into
`Documents\BaySickDAW\Presets\Inst Page\My Presets\<name>.xml`.

**Per machine, not per project.** The Core Library itself and everything in it.

**Not saved at all.** The kit-substituted marker, the audition atomic, live
slide/bend gesture state, and sfizz's internal voice state.

---

## Lifetime and teardown

- The processor is created lazily on the **first kit load** for that page index,
  not when the tab is added - `loadBaySickGuitarsKit` constructs it if the slot
  is empty. Before that, the tab's chain has no sfizz stage and produces silence.
- `destroyBaySickGuitars(instIdx)` clears the tab's active flag first (the audio
  thread reads the flag before touching the engine), then frees the instance
  under a spin lock.
- **Ordering rule:** `rig.removeTab` runs before `destroyBaySickGuitars`. The
  rig-owned chain holds the spliced sfizz stage pointer, and one audio block in
  the wrong order is a use-after-free.
- The ARIA panel's widgets hold parameter attachments rooted in the engine's
  APVTS. Any path that frees the engine must clear the panel first.
- The processing gate (`setProcessingEnabled`) is the engine's own guard: while
  false, `processBlock` clears its buffer, drops incoming MIDI and returns rather
  than rendering against a half-parsed kit. CC marks made during the load
  accumulate and flush on the first block after the gate reopens.
- The APVTS carries a stable undo tag (`sfizz:bgg_<instIdx>_`) so undo and redo
  survive the engine being rebuilt by a program switch.
- Deleting the tab warns that it also removes the tab's pedalboard, NAM/IR,
  mixer strip, effects rack and the audio-library entries for anything recorded
  on it. Audio files in the project's Samples folder stay on disk.

---

## Cross-references

- `BaySickBasses.md` - the sibling engine. Same architecture, different kit and
  a different default program; the two share the Inst page cap.
- `BaySickRustyDrums.md` - the third sfizz engine, and the one that uses the
  ARIA panel's section tab row and multi-output strips.
- `BaySickPlayer.md` - the non-sfizz sample player used by Layers, Bass, Drums
  and Clips tabs.
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
- Source comments in `BaySickGuitarsProcessor.h` still say "up to 20 instances
  coexist". The real cap is `kMaxInstPages` = **30**, shared with live-input Inst
  tabs and BaySickBasses.
