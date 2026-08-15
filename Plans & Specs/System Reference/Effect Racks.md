# Effect Racks

**Purpose** - Every channel in BaySickDAW has its own effect rack: six slots
that the signal passes through in order, from slot 1 down to slot 6. A rack is
where you add a compressor, a reverb, a delay or a third-party plugin to one
sound without touching anything else in the song. Every mixer strip and every
bus owns one, so a rack exists for a channel whether or not you have ever
opened it.

---

## How it operates

`Source/EffectRack.h` / `Source/EffectRack.cpp` define the rack.
`EffectRack::kNumSlots` is 6. One rack instance lives inside every audio node
that can carry effects: the bus nodes and the per-strip `InsertNode`s owned by
`BaySickGraph` (`Source/BaySickGraph.cpp`, `struct BaySickGraph::InsertNode`), plus one
owned by `BaySickVocalProcessor` for the Vocal Chain.

The Vocal Chain is the one rack whose slots are **locked**: six fixed stages in
a fixed order - Gate, De-reverb, De-esser, Compressor, Saturation, Limiter. Its
slots cannot be swapped, moved or removed; bypass, the sidechain picker and the
Mode menu all still work. Everywhere else the six slots are free.

**Position in a strip's signal path.** `InsertNode::processBlock` runs, in this
order: the freeze tap, the pre-rack EQ, polarity, stereo width, the rack, the
post-rack EQ, then fader / mute / solo. So the rack sits between the two EQs
that the Effects window's `Pre EQ` and `Post EQ` buttons open.

**Inside the rack.** Each `Slot` holds an `active` and a `pending`
`std::unique_ptr<DSPBase>` plus a `swapPending` atomic. Loading, clearing or
bypassing a single slot is wait-free from the audio thread's point of view: the
message thread builds and `prepare()`s the new DSP outside every lock, parks it
in `pending`, and release-stores the flag; the audio thread swaps the two
pointers at the top of `process()`. Multi-slot changes (`moveSlotUp`,
`moveSlotDown`, `packSlotsToTop`, `setStateInformation`, `prepare`, `reset`)
hold `mSlotsLock` (a `juce::SpinLock`) across the whole rack; the audio thread
try-locks and skips one block rather than waiting. `mLoadLock` (a
`juce::CriticalSection`) serializes message-thread writers against each other
and is never taken on the audio thread.

**Per block**, `EffectRack::process` returns immediately if the whole rack is
bypassed; otherwise for each slot it reads the slot's bypass flag, ramps a
wet/dry crossfade toward it over `kBypassRampMs = 5.0f` milliseconds using a
per-slot dry-snapshot buffer (so toggling a high-gain drive does not click),
skips the DSP entirely once fully bypassed, pushes the strip's sidechain buffer
array and this slot's sidechain pick into the effect, runs the effect, and
applies the slot's output gain. Input peak and output peak dB are stored into
per-slot atomics and promoted to UI-visible snapshots once per audio block by
`promoteSlotPeakSnapshots()`.

**Latency.** `getTotalLatencySamples()` sums `getLatencySamples()` across
active, non-bypassed slots and returns 0 when the rack is bypassed; the graph
uses this for delay compensation.

**The window shell.** The Effects surface is a small index window plus
satellite windows, not one big page:

| Class | File | What it is |
|---|---|---|
| `EffectsPage` | `Source/Standalone/EffectsPage.*` | The rack index window: channel picker, FX Bypass, two EQ buttons, six rows |
| `EffectSlotWindow` | `Source/Standalone/EffectWindows.*` | One window per loaded effect; hosts a `SlotComponent` in `PanelOnly` presentation |
| `EffectEqWindow` | `Source/Standalone/EffectWindows.*` | One window per EQ; fixed to Pre or Post for its whole life |
| `EffectVisualWindow` | `Source/Standalone/EffectWindows.*` | One window per effect slot, showing that effect's live picture |
| `SlotComponent` | `Source/Standalone/SlotComponent.*` | The effect panel host: picker menu, mode menu, sidechain menu, preset menu, meters |

Every satellite window holds a channel id and the slot's UUID and re-resolves
the rack and the DSP on a peer-keyed poll - 10 Hz for an effect window, 30 Hz
for an EQ window, 4 Hz for a visual window (its drawing runs off the shared
30 Hz `EffectVisualClock` instead). Nothing caches
an `EffectRack*` or `DSPBase*` across ticks, because a rack dies with its tab
and a project load replaces a slot's DSP in place. A window whose target stops
resolving closes itself - but only after it has resolved at least once, so a
window restored with a project does not close before the rack is populated.

**Automation** is registered model-side, never against a widget.
`EffectsPage::registerSlotAutomationFor` captures (channel id, slot UUID,
effect type, variant) and resolves rack -> slot -> DSP at apply time.
`EffectsPage::registerRackAutomationForAllChannels` sweeps every channel after
a project load so lanes work without the Effects window ever being opened.

---

## User-facing behavior

### Opening the rack

Click **Effects** in the ribbon along the top. You can also press the **FX
Rack** button on any mixer strip, which opens the Effects window already
pointed at that strip.

### The Effects window

The window is an index. It has four things:

| Control | What it does |
|---|---|
| **Channel:** dropdown | Chooses which channel's rack you are editing. The list is grouped by bus with colored headings (MASTER, FX BUS, CLIPS BUS, LAYERS BUS, BASS BUS, DRUMS BUS, and so on) and shows only channels that currently exist. Switching channels re-points the six rows; it does **not** close or re-point any effect window you already have open. |
| **FX Bypass** button | Turns the whole rack for the selected channel off in one click. The six slots keep their settings; the sound just passes through untouched. |
| **Pre EQ** button | Opens the 8-band EQ that runs *before* the six slots, in its own window. |
| **Post EQ** button | Opens the 8-band EQ that runs *after* the six slots, in its own window. |

Below those are the six slot rows, top to bottom, in the order the sound passes
through them.

### A slot row

Each row reads: `[LED] [ effect name ] [up] [down] [pick] [x]`

| Part | What it does |
|---|---|
| The small round **LED** on the left | Lit means the effect is running; click it to bypass this one effect. Only shown on a filled row. |
| The **name plate** | Shows the effect's name, or `Empty`. Click a filled name to open that effect in its own window. Click an empty one and you get the effect picker (same as the chevron). |
| **Up triangle** | Moves this effect one place earlier in the chain. |
| **Down triangle** | Moves this effect one place later in the chain. |
| **Chevron** | Opens the effect picker for this slot. |
| **Red X** | Removes the effect. You are asked `Remove <name> from this slot?` first. After a removal the remaining effects pack upward so there are no gaps. |

Order matters. A compressor before a reverb compresses the dry sound and then
reverberates it; the same two in the other order reverberates first and then
compresses the whole wet result. Both are valid, they just sound different.

### The effect picker

Grouped, with the rack effects at the top level:

- **Dynamic** - Compressor, De-esser, Gate, Limiter, Transient Shaper
- **Harmonics** - Overdrive, Saturation
- **Modulation** - Chorus, Flanger, Phaser
- **Time** - De-reverb, Delay, Reverb
- **Pedals** (a submenu) - the guitar/bass pedal modules; see *Pedalboard.md*
- **VST Plugins** (a submenu) - every effect plugin you have added under
  Options > Plugins. If you have added none, the heading is still shown with a
  disabled row reading `None added - see Options > Plugins`, so you can tell
  the feature exists.

Choosing an effect loads it and immediately opens its window - picking an
effect is treated as a request to work on it.

Tape is not a separate picker entry: load **Saturation** and set its Mode to
Tape. Five modules are pedalboard-only and are not offered here: the Tuner, the
three pedal EQs (Graphic EQ, Bass Graphic EQ, Pro Parametric EQ) and the User
NAM Pedal.

### An effect's own window

The window shows only the effect's panel. Its chrome lives on the window's
title strip:

| Title-strip item | What it does |
|---|---|
| Bypass **LED** | Same bypass as the row's LED. |
| **Menu** | Everything else, listed below. |

The Menu contains, in order and only when they apply:

| Menu item | What it does |
|---|---|
| **Show Advanced Controls** / **Show Basic Controls** | Flips the panel between the small everyday control set and the full one. Only offered for effects that actually have extra controls to hide. The window resizes to fit: about 691 x 268 in Basic, about 1047 x 268 in Advanced, about 358 x 268 for a pedal-style panel. |
| **Mode: `<name>`...** | Character switch for the six effects that have one - Compressor, Saturation, Delay, Reverb, Overdrive, Limiter. Picking a mode rebuilds the panel, because the modes have different controls. |
| **SC: `<source>`...** | Sidechain source picker. Only offered for effects that actually listen to a sidechain: Compressor, Limiter, Transient Shaper, Reverb, Delay, the pedal Noise Gate, and any hosted VST3 effect that declares a side-chain input bus. A plugin running bridged never reports one, so the row is absent there. |
| **Presets...** | Save Current Preset, Load: Factory, Load: My Presets, Restore Defaults, Save Current as Default, Manage Presets (opens the folder). |
| **Visual** | Opens this effect's picture window. Only shown for effects that have one. |
| *(hosted plugins only)* **Automate**, **Run bridged**, **Retry Loading Plugin** | See the plugin section below. |

Every panel also carries a **Vol** knob at its right edge. That is the slot's
own output level, not a control inside the effect: range -24 dB to +12 dB,
default 0 dB, double-click to reset. Use it to match the level after a
compressor or a drive so bypassing does not jump in loudness. Panels for the
pedal-style modules hide it because those modules have their own Level knob.

Left of the Vol knob is a small output meter in dBFS; most panels also have a
tall input VU meter on the left. The VU meter's 0 VU reference is set app-wide
from the Effects window's own Menu (see below) and can be -18, -17, -16, -15 or
-14 dBFS.

### Sidechain

A sidechain lets one channel's sound control an effect on another - the classic
case is a kick drum pushing down a bass. It takes two steps, and doing only the
first is the usual reason a sidechain appears to do nothing.

1. **Route the cable on the Mixer page.** Click the **"+"** at the bottom of the
   strip you want to use as the *source* (the kick), pick **Sidechain...**, then
   pick the strip whose effect should react (the bass). Every other strip is
   listed by name; the ones whose four receive lines are already full, and
   anything that would create a feedback loop, are grayed out. A strip can
   receive four sidechains.
2. **Point the effect at that line.** Open the effect on the receiving strip and
   use **Menu > `SC: Off...`**. The menu lists `Off` plus one row per routed
   source, each named after the source strip, with a tick on the current pick.
   `Off` means the effect uses its own input. If nothing is routed to the strip
   the menu shows `Off`, a separator, and one disabled row reading
   `(no sidechain cables routed to this strip)`.

A **hosted VST3 effect** takes exactly the same two steps. The slot feeds the
picked line into the plugin's own side-chain input bus, and with `Off` picked
that bus is left silent, which is what an unconnected side-chain looks like to a
plugin. The `SC:` row appears only for plugins that declare a side-chain input,
and a plugin running bridged (**Menu > Run bridged (separate process)**) never
reports one - so if the row is missing on a plugin you know has a side-chain,
check whether it is bridged.

### The visual window

Effects with a picture (Chorus, Compressor, Delay, Flanger, Limiter, Phaser,
Reverb, Saturation and Tape, Transient Shaper) open a small visual window
automatically underneath the effect window, matched to its width. The two are
tethered: drag either half and both move, and closing either one closes the
pair. To break that, open the visual window's own Menu and unlock it; then each
moves and closes on its own.

Closing an **unlocked** visual is a dismissal - it stays closed until you ask
for it again from the effect window's **Menu > Visual**. Closing a **locked**
pair is not a dismissal, because the effect window went with it, so the visual
comes back when you reopen the effect. Both the unlock and the dismissal are
remembered with the project.

A visual costs nothing while it is not on screen - the effect only publishes
data while someone is watching.

### The Effects window's own Menu

| Item | What it does |
|---|---|
| **Save FX Rack Preset...** | Saves all six slots *and* both EQs for the selected channel under a name you type. |
| **Load FX Rack Preset** | Lists your saved rack presets; picking one replaces the whole rack and both EQs on the selected channel. |
| **Open Presets Folder** | Opens the folder those files live in. |
| **VU Calibration (0 VU = ...)** | App-wide reference for every VU meter: -18 through -14 dBFS. The same submenu is offered on the VU Meter window's own menu, off one definition - either place sets the same app-wide value. |
| **VU Meter** | Opens the VU Meter window: one analog VU on the master output. It is the only VU in the app - effect panels carry a dBFS bar and, where the effect has one, a gain-reduction meter, but no VU of their own. |

### Third-party plugins in a slot

Add plugins to BaySickDAW under Options > Plugins first; they then appear in
the picker's **VST Plugins** group. A plugin slot behaves like any other -
bypass, move, remove, per-slot Vol, rack presets - and its window shows the
plugin's own interface at the size the plugin asks for.
Scanning runs in a helper process, so a plugin that crashes while being scanned
never takes BaySickDAW down with it - it lands in Scan results as
`Skipped: failed to load` and the scan carries on. See **Plugins Page.md**,
"When a plugin crashes the scan", for the crash list and how it clears itself. BaySickDAW does not
scale that interface. A resizable plugin follows the window edge you drag (it
can refuse, and the window then re-fits to what the plugin accepted); a
fixed-size one keeps its own size regardless - centered on a flat dark gray
surround when the window is bigger, anchored top-left and clipped when the
window is smaller. There are no scrollbars; the plugin's own magnify control is
the size control.

**`(missing)`** after the plugin's name in the row and window title means the
slot still knows exactly which plugin it wants but does not have a working
instance of it: the file was moved or deleted, the plugin failed to load, it
needs the bridge, or it crashed during this session. What you hear depends on
how it was running - a plugin that was running inside BaySickDAW passes the
sound through untouched, while one running bridged in its own process goes
silent for that slot only. Either way the rest of the song keeps playing and
the audio never stalls.

Two ways back:

1. Put the plugin back in the added list (Options > Plugins). The rack watches
   that list and retries every dead slot automatically the moment it changes.
2. Open the slot's window and use **Menu > Retry Loading Plugin**. The entry
   only appears while the plugin is not alive.

Either way the slot keeps its identity, so your automation and the open window
keep working, and the plugin's saved settings are pushed back into the revived
copy.

**Menu > Run bridged (separate process)** runs that plugin in its own process,
which keeps a crash out of BaySickDAW. It takes effect the next time the plugin
loads, and the app says so when you toggle it. A 32-bit plugin has no choice -
its row is shown but disabled with the reason in the text.

**Menu > Automate** is where a hosted plugin's automation lanes are created,
since a plugin's own interface has no right-click hook of ours. It offers
**Last Touched** (move a control in the plugin first) and the full parameter
list, chunked into groups of 30 when the plugin has many.

---

## Parameters and persistence

### Saved with the project

The rack serializes to an `<EffectRack>` tree with six `<Slot>` children:

| Property | Meaning |
|---|---|
| `index` | Slot number 0-5 |
| `type` | The `EffectType` enum value as a raw int. Ordinals are pinned and append-only; an unknown ordinal builds no DSP and the slot is skipped on the audio path |
| `bypassed` | Per-slot bypass |
| `outputGainDb` | The Vol knob, -24..+12 dB |
| `uuid` | Stable per-slot identity; automation parameter ids key on it |
| `scPick` | Sidechain line, -1 = none, 0-3 = the strip's receive index |
| `basicMode` | Basic (1) or Advanced (0); reads 1 for projects that predate it |
| `data` | Base64 of the effect's own state blob |

The state is captured from `pending` when a swap is in flight, so what is saved
is the effect you can see, not the one the audio thread has promoted.

The rack's **whole-rack bypass is deliberately not in this tree** - it belongs
to the strip's APVTS parameter `<mixerPrefix>_bypass` (for example
`mixer_layer_0_bypass`), which the audio path re-reads every block.

`BaySickGraph::saveRackStates` writes one node per rack into the project with
`rack`, `preEq` and `eq` base64 properties; `applyRackStates` matches by
id/name on load. A bus with no saved record is left as it was rather than
cleared.

### Automation parameter ids

Every lane id is `<channelPrefix>_<slotUuid>_<suffix>`:

- `channelPrefix` comes from `EffectsPage::channelPrefixForId` - `master`,
  `layers_bus`, `bass_bus`, `drums_bus`, `fx_bus`, `clips_bus`, `vox_bus`,
  `inst_bus`, `rusty_bus`, `plugin_bus`, the `*_bus2/3` variants, and the
  per-strip forms `layer_1`, `bass_2`, `drum_0`, `audio_0`, `aux_0`, `vox_0`,
  `inst_0`, `rusty_0`, `plugin_0`.
- `suffix` is either `output_vol` (the slot's Vol knob), a table suffix from
  `EffectParamMap` keyed on (effect type, variant), or `vst_<plugin parameter
  id>` for a hosted plugin.

Because the id carries the slot's UUID and not its index, reordering the rack
does not repoint a lane at a different effect.

### Saved with a preset

- **Per-effect presets** (the slot window's Presets menu) store only that one
  effect's blob:
  `Documents\BaySickDAW\Presets\Effects\<Type>\Factory\<name>.xml`,
  `...\My Presets\<name>.xml`, and an optional `...\Default.xml`. Pedal modules
  nest one level deeper under `Presets\Effects\Pedals\<Type>\`.
- **FX Rack presets** (the Effects window Menu) store the entire six-slot rack
  blob *plus* the strip's four EQ parameter families, written as parameter
  suffixes so a preset saved on one strip loads onto another:
  `Documents\BaySickDAW\Presets\FX Rack\My Presets\<name>.xml`.

### Per machine, not per project

The VU calibration reference is an app-wide UI setting, not project content.

### Not saved at all

Meter values, gain-reduction readouts, the bypass crossfade position, and the
"which effect window is frontmost" bookkeeping. Effect and visual windows
themselves *are* saved - they are registered as aux windows, so the project
records which are open, where, and whether a visual pair is unlocked or was
closed by hand.

---

## Lifetime and teardown

A rack is a member of the graph node that owns it, so it lives as long as the
bus or strip does and dies with it. Racks are long-lived and reused: clearing a
slot resets its type, bypass and UUID *and* its output gain, sidechain pick and
Basic/Advanced flag, so nothing bleeds into the next effect dropped there -
including across a project boundary.

Order that matters:

- A DSP is always **built and prepared outside every lock**, and the outgoing
  DSP is always **destroyed on the message thread outside every lock**. For a
  hosted plugin that teardown includes the plugin and, if bridged, its helper
  process, so it must never run under a lock the audio thread could queue
  behind.
- `clearSlot` completes the swap and destroys the removed DSP immediately
  rather than parking it until the next load on that slot.
- Restoring a project installs each rebuilt DSP straight into `active` under
  both locks rather than going through the pending-swap handshake, because
  during a load the audio thread is not running to consume the flag.
- Pages are views. A slot's panel can be destroyed while its DSP keeps running;
  `EditorPanelBase::liveDsp()` re-asks the rack for the pointer every tick so a
  stale panel can never dereference a freed DSP.
- `EffectSlotWindow`, `EffectEqWindow` and `EffectVisualWindow` each defer
  their self-close through `MessageManager::callAsync`, because the close
  destroys the object whose timer callback asked for it.

---

## Cross-references

- **Effect Modules.md** - every module you can load into a slot, and its controls.
- **EQ.md** - the Pre EQ and Post EQ windows the rack sits between.
- **Pedalboard.md** - the separate 8-slot pedal chain on Inst tabs.

---

## Differs from Carry-Forward

The Carry-Forward Reference snapshot describes the Effects surface as a single
page with sub-tabs and points at `EffectsPage.cpp:28` for the channel-list
callback. Since then the surface was rebuilt as a small rack **index** window
plus one window per effect and one per EQ, and the callback moved (it is now
wired around `EffectsPage.cpp:190`). Carry-Forward's routing notes (NAV-03 -
mixer FX Rack buttons deep-link to the right channel) still hold; what changed
is what opens when you get there.
