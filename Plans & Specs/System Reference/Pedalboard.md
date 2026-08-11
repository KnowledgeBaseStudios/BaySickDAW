# Pedalboard

**Purpose** - BaySickPedals is a guitar/bass pedal chain that sits at the front
of an Inst tab, between what you plug in and the amp. It is an eight-position
board: a tuner that is always first, six positions you fill and reorder
yourself, and an EQ that is always last. It is separate from the six-slot
effect rack every mixer strip has - the board is part of the instrument's
sound, the rack is mixing.

---

## How it operates

`Source/BaySickPedals/BaySickPedalsProcessor.h/.cpp` is the board;
`BaySickPedalsEditor.cpp` is its UI. The processor is a `juce::AudioProcessor`
with `kNumSlots = 8`, stereo in and out.

**Slot policy** (`isEffectAllowedInSlot`):

| Position | Index | Rule |
|---|---|---|
| Tuner | 0 (`kSlotTuner`) | Locked. Holds `EffectType::TunerStyle` and nothing else. Cannot be cleared or moved |
| Free positions | 1-6 | Any effect type except the four slot-locked ones. Reorderable |
| EQ | 7 (`kSlotEQ`) | Locked position. Holds exactly one of Graphic EQ, Bass Graphic EQ or Pro Parametric EQ. Cannot be cleared or moved |

The board is constructed with slot 0 = Tuner and slot 7 = Graphic EQ already
built; slots 1-6 start empty.

**Where it sits in the signal path.** An Inst tab drives an
`EngineChainProcessor` (`Source/Standalone/EngineChainProcessor.*`) which calls
its stages in order on the same buffer. For a live-input tab the chain is
`Pedals -> NAM/IR`; for a tab whose source is one of the sfizz instruments the
sfizz engine is prepended: `sfizz -> Pedals -> NAM/IR` (`InstPage.cpp`,
`setChain`). The Inst strip's own six-slot rack and its two EQs come after all
of that, on the mixer side.

**Threading** mirrors `EffectRack`. Each `Slot` is an `active`/`pending`
`unique_ptr<DSPBase>` pair with a `swapPending` atomic; single-slot loads and
clears publish through that flag and never block audio. `moveSlot` takes
`mSlotsLock` (a `juce::SpinLock`) which the audio thread try-locks and skips a
block on. `mLoadLock` serializes message-thread writers. `processBlock` walks
the eight slots in index order, skipping any whose APVTS bypass parameter is
set.

Unlike the rack, a board `Slot` carries only `active`/`pending`/`swapPending`/
`type`/`uuid`: per-slot bypass lives in APVTS, and there is no per-slot output
gain and no per-slot sidechain pick - each pedal owns its own Level knob.

**Delay compensation.** `getChainLatencySamples()` sums the live per-slot
latencies of the non-bypassed slots (the Octave's doubler, the drive pedals'
oversamplers). It is pulled on the message thread by
`EngineChainProcessor::getChainLatencySamples`, which special-cases the Pedals
stage because the board never calls `setLatencySamples` itself. The 5 Hz
latency solve picks up slot swaps and bypass flips for free.

**UI refresh.** `BaySickPedalsEditor` runs a 10 Hz timer comparing each slot's
type against a cached copy and rebuilds that tile when it changes, which is
what makes undo, project loads and pedalboard-preset loads visible.
Same-type-different-DSP swaps (a bulk restore) are caught by the processor's
`onSlotsExternallyChanged` callback, which rebuilds every tile; without it the
tiles' widgets would stay bound to a destroyed DSP.

---

## User-facing behavior

### Opening the board

The pedalboard belongs to an Inst tab. Open the tab's window menu and choose
**Pedals**. On a live-input tab that window *is* the player, so it is titled
with the tab's name; on an instrument tab it is titled `<tab> - Pedals`.

### The board window

**Standard view** is a 4 x 2 grid, read left to right, top row then bottom:

```
[ Tuner ] [ slot 1 ] [ slot 2 ] [ slot 3 ]
[ slot 4] [ slot 5 ] [ slot 6 ] [   EQ   ]
```

Sound flows in that order: through the tuner first, through your six pedals,
into the EQ last, and on to the amp.

The window's title strip carries:

| Item | What it does |
|---|---|
| **Menu** | The Inst page's own actions menu |
| **View** | Standard or Compact (below) |
| **NAM/IR** | Opens the amp and cabinet window that follows the board. Reads `N/I` in Compact |
| **Preset** | The pedalboard preset menu (below) |

**Compact view** replaces the grid with a dropdown listing all eight positions
(`1  Tuner`, `2  (empty)`, ...) and shows one pedal at a time in a small window
about the size of an effect window. Nothing is removed; a chain of separate
boxes pages naturally. The mode is remembered with the project.

### A pedal tile

| Part | What it does |
|---|---|
| **Title** | The pedal's name. A NAM Pedal shows the name of the capture file you loaded instead, or `NAM Pedal (no file)` |
| **Up / Down triangles** | Move this pedal one position earlier or later. Grayed out at the ends of the movable range |
| **"..."** | The per-pedal preset menu |
| **X** | Removes the pedal, leaving the position empty |
| **ON** footswitch (bottom) | Green lettering means the pedal is engaged; click to bypass it |
| The middle | That pedal's controls |

Empty positions paint a dashed outline with a large `+`.

**To add a pedal:** click an empty position, or right-click any position.
**To swap a pedal:** right-click it and pick a different one, or use `Clear`.
**To reorder:** use the Up/Down triangles, or drag a pedal by its title bar and
drop it on another position - the drop target highlights while you drag.

The Tuner position has no Change menu at all. The EQ position's menu offers
only the three EQ choices, and it also has a dropdown in its own title row for
the same purpose.

### The Change Pedal menu

| Group | Entries |
|---|---|
| User NAM Pedal | Load NAM Pedal |
| Dynamics | Bass Compressor, Compressor, Noise Gate |
| Harmonics | Bass Driver, Bass Overdrive, Blues Drive, Distortion, Fuzz, High-Gain, Octave, Overdrive, Saturation |
| Modulation | Acoustic Simulator, Chorus, Flanger, Phaser, Polyphonic Synth, Wah |
| Time | Acoustic Preamp, Delay, Reverb |
| *(below a separator)* | Clear |

Choosing **Load NAM Pedal** immediately opens a file chooser for a `.nam`
capture, because the pedal does nothing without one. The chooser starts in
`Documents\BaySickDAW\Presets\Effects\Pedals\User NAM Pedals`. If the file
fails to load you get a `NAM Load Failed` box carrying the reason; a capture
stopped by the sanity gate reads `This NAM capture was refused: <reason>.` A
capture that fails when a **project** or **page preset** restores the board is
reported differently: that reason is discarded, the pedal's name gains a
` (missing)` suffix, and the path is listed in the **Missing files** dialog under
`NAM capture (failed to load)`.

Two pedals behave differently here than in a mixer rack:

- **Compressor** loaded on the board comes up in **Pedal** mode (the
  four-knob sustainer). In a rack it comes up in Modern.
- **Overdrive** on the board comes up in **Pedal** mode (three knobs). In a
  rack it comes up in Rack mode.

Seven effects have a smaller pedal face for use on the board - Limiter,
Saturation, Chorus, Flanger, Phaser, Delay and Reverb. They are the same
processors with a reduced control set and no meters. **The pedal face declares
its own knob ranges and starting values**, so several of the numbers below
differ from the same effect's full rack panel in *Effect Modules.md* - that is
the face's own spec, not a second opinion about the DSP:

| Pedal face | Controls (range, default) |
|---|---|
| Limiter | Ceiling -24..0 dB (-1); Release 10..1000 ms (100) |
| Saturation | Drive 0..10 (3); Mix 0..100 % (100); mode selector Tube / Console / Tape |
| Chorus | Rate 0.05..5 Hz (0.5); Depth 0..20 ms (4); Mix 0..1 (0.5) |
| Flanger | Rate 0.05..5 Hz (0.5); Depth 0..10 ms (3); Feedback -1..+1 (0.5); Mix 0..1 (0.5) |
| Phaser | Rate 0.05..10 Hz (0.5); Depth 0..1 (0.5); Feedback -1.2..+1.2 (0.5); Mix 0..1 (0.5) |
| Delay | Time 1..2000 ms (375), with a Sync switch that locks it to the song tempo; Feedback 0..1.2 (0.4); Mix 0..1 (0.4) |
| Reverb | Decay 0.1..10 s (1.5); Damp 500..20000 Hz (6000); Mix 0..1 (0.3); algorithm selector Plate / Hall / Chamber / Room / VocalBooth |

Six of those seven are in the Change Pedal menu above. **Limiter is not** - the
board's slot policy allows it, but no menu offers it, so a Limiter tile can
only appear on a board restored from a saved state that already contained one.

Every other pedal shows exactly the same panel it shows in a rack. See
*Effect Modules.md* for the full control list of each one, including the five
board-only modules that the rack picker does not offer: Tuner, Graphic EQ, Bass
Graphic EQ, Pro Parametric EQ and User NAM Pedal.

### The per-pedal preset menu ("...")

| Item | What it does |
|---|---|
| Save Preset As... | Names and writes a preset for this pedal type |
| Factory Presets | The presets that ship with the app for this pedal |
| My Presets | Your own |
| Restore Defaults | Back to the pedal's factory settings |
| Save as Default | Makes the current settings this pedal's default |
| Reveal Folder... | Opens the folder in Explorer |

Loading a preset or restoring defaults rebuilds the tile, so the title and the
knobs follow what was loaded - important for the NAM Pedal, whose title is the
capture's name.

### The pedalboard preset menu (title-strip **Preset**)

| Item | What it does |
|---|---|
| Save Pedalboard As... | Saves all eight positions - which pedals, their settings, and the bypass switches - under a name you type |
| *(list of saved boards)* | Loads one, replacing the whole board |
| Reveal Folder... | Opens the folder |

A whole-board load restores every pedal's DSP, including any NAM capture and
any user impulse response. If one of those files has gone missing you get a
report naming what could not be found rather than a silent partial load.

---

## Parameters and persistence

### APVTS

The board owns exactly eight parameters, all booleans, all default false:

| Parameter id | Label | Meaning |
|---|---|---|
| `bsp_slot0_bypass` .. `bsp_slot7_bypass` | `Slot N Bypass` | Per-slot bypass, driven by the tile's ON footswitch |

There are no chain-level parameters - each pedal carries its own level and
output controls.

### Automation

The hosting Inst page hands the board an automation prefix of the form
`inst<N>_pedals` (`InstPage.cpp`). A lane id is
`inst<N>_pedals_<slotUuid>_<controlSuffix>`, where the suffix is the control's
on-screen label lowercased, resolved through `EffectParamMap` exactly as a rack
slot's is. The key is the slot's **UUID, not its index**, so reordering the
board cannot repoint a lane at a different pedal. Applicators are registered
model-side (`StandaloneEditor::registerPedalAutomation`) so the lanes work with
no pedals window open.

Loading, clearing or restoring a slot mints a new UUID and fires
`onSlotAutomationChanged` so the model re-registers; the outgoing pedal's lanes
are retired. `moveSlot` deliberately does **not** fire it - UUIDs travel with
the slot.

### Saved with the project

`captureFullState()` writes a `<BaySickPedalsRoot>` tree with a `version`
property, a deep copy of the APVTS tree (the eight bypass flags), and a
`<Slots>` list of `<Slot index type uuid data>` where `data` is the pedal's own
state blob, base64-encoded with `MemoryBlock::toBase64Encoding`.
`restoreFullState()` replays the APVTS tree first, then rebuilds each DSP off
the audio thread and installs them, minting a UUID for any slot saved before
UUIDs existed. Loading accepts the older outer tag `BaySickPedalsState` as well
as the current one, and finds the APVTS child by the APVTS's own type rather
than a hard-coded string.

That whole tree is embedded in the Inst page's state
(`InstPage::exportInstState`), so the board is saved with the project and with
an Inst page preset.

The board's **view mode** (Standard or Compact) is owned by the Inst page, not
the editor, and is saved with the project. The pedals window's position and
size are saved as an aux window (`instsat:<N>:pedals`).

### Saved with a preset

| Preset kind | Location | Contents |
|---|---|---|
| Pedalboard | `Documents\BaySickDAW\Presets\Pedalboards\<name>.xml` | The whole eight-slot board wrapped in a `<Pedalboard>` envelope |
| Per pedal | `Documents\BaySickDAW\Presets\Effects\Pedals\<Type>\Factory` and `\My Presets`, plus `Default.xml` | One pedal's state blob |

Saving through either path goes through `UserFileSave`, so an existing file of
the same name prompts to replace or save a copy rather than being overwritten
silently.

### Not saved

Tuner detection, meter values, and the drag-reorder highlight.

---

## Lifetime and teardown

The board processor is owned by the Inst tab's engine record and is created
with that tab; the editor is created by `createEditor()` when the pedals window
opens and destroyed when it closes. The board keeps processing with the window
shut - closing a window never silences a pedal.

Order that matters:

- A pedal DSP is built and prepared outside every lock and destroyed on the
  message thread outside every lock, the same rule the rack follows.
- `restoreFullState` builds every replacement DSP first and installs the set
  under both locks, then posts `onSlotsExternallyChanged` and
  `onSlotAutomationChanged` asynchronously - the editor must rebuild its tiles
  *after* the swap, or its widgets point at destroyed DSPs.
- The editor's destructor stops its 10 Hz timer first, then clears
  `onSlotsExternallyChanged`, then destroys the tiles (which releases their
  APVTS bypass attachments) before the base destructor runs.
- Slots 0 and 7 are never cleared: `clearSlot` returns false for them, so the
  tuner and the EQ always exist.

---

## Cross-references

- **Effect Modules.md** - the full control list for every pedal, including the
  five board-only modules.
- **Effect Racks.md** - the six-slot mixer rack the Inst strip also has,
  downstream of this board.
- **EQ.md** - the 8-band parametric EQ on the mixer side, which is a different
  thing from the board's EQ position.

---

## Differs from Carry-Forward

Not applicable - the Carry-Forward Reference snapshot does not cover
BaySickPedals.
