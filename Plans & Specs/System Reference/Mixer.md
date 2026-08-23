# Mixer

**Purpose** - The Mixer is where every sound in the project gets its volume, its
position in the stereo picture, and its path to the speakers. Each thing that
makes noise (an instrument tab, a recorded audio row, a live microphone) owns one
vertical **channel strip**; strips feed **buses**, buses feed **Master**, and
Master feeds the audio device. It is also where you send a copy of a channel to a
shared effect, and where you pick which channel the Effects page is editing.

---

## How it operates

The Mixer is one page (`Source/Standalone/MixerPage.cpp/.h`) hosted in its own
workspace window. It is a **view**: it owns no audio. Every control is bound to a
parameter in the processor's APVTS (`Source/PluginProcessor.cpp`,
`addParamsForMixerStrip`), and the audio path reads those parameters directly.
Closing the Mixer window changes nothing about the sound.

**The channel-id vocabulary.** Every strip has a unique integer channel id
(`MixerChannelIds`, `Source/BaySickGraph.h`). That id is the key for routing, for
the cable overlay, for the Effects page, and for the APVTS prefix a strip's
parameters live under (`prefixFromChannelId`, e.g. id 200 -> `mixer_layer_0`).

| Range | Family |
|---|---|
| 0 | Output (terminal sink; only Master routes here) |
| 1-18 | Buses (see the bus table below) |
| 100-117 | Aux strips 1-18 |
| 200-219 | Layers inserts 1-20 |
| 300-309 | Bass inserts 1-10 |
| 400-499 | Audio (Clips) inserts 1-100 |
| 500-531 | Drum inserts 1-32 |
| 600-609 | Vox inserts 1-10 |
| 700-729 | Inst inserts 1-30 |
| 800-812 | Rusty Drums inserts 1-13 |
| 900-919 | Plugin inserts 1-20 |

Per-family caps are derived from the page caps in `BaySickConstants.h`, so a
strip cap is always its page cap.

**Signal flow through one insert strip** (`BaySickGraph::InsertNode::processBlock`,
`Source/BaySickGraph.cpp`): source audio -> pre-rack EQ -> polarity -> stereo width
-> effects rack (bypassable) -> post-rack EQ -> fader x mute x solo (gain-ramped
so moves do not zipper) -> pan (project pan law) -> sidechain tap -> latency
compensation delay -> peak and RMS publish.

**Signal flow through one bus** (`BaySickGraph::InstrChannelNode::processChainOnly`):
input sum -> pre-rack EQ -> rack -> post-rack EQ -> fader x mute x solo ->
polarity -> stereo width -> pan -> sidechain stash -> compensation delay -> peak.
Note the order difference: a bus applies its fader **before** polarity and width;
an insert applies it after. Master (`processMasterChain`) runs pre-EQ -> rack ->
post-EQ -> master gain x fader x mute -> pan -> width, then the loudness (EBU
R128 LUFS), true-peak and spectrum taps.

**Routing** is resolved from APVTS every audio block (`RoutingGraph::rebuildFromApvts`).
Each strip contributes one main-out edge (`_sendTo`), up to four send edges, and
up to four sidechain edges. A Kahn topological sort orders the graph and drops
cycles; the UI calls `wouldCreateCycle` before it offers a target, so an illegal
cable cannot be made in the first place.

**Threads.** Audio-thread code reads cached raw atomic pointers, never
string-keyed parameter lookups. Meters go the other way: the audio thread
CAS-maxes peak and RMS values into atomics, and the UI drains them on each
monitor refresh (`MixerPage::onVBlank`) and resets them to silence, so a missing
reader costs nothing. A separate 30 Hz timer watches for `_sendTo` changes and
re-groups the strips when a channel is rerouted.

---

## User-facing behavior

### Reading the console

The Mixer looks like a hardware mixing desk seen from the front. **Master** is
pinned on the far left and never scrolls. Everything else lives in a horizontally
scrolling area with a permanent slim scrollbar along the bottom.

Strips are arranged in **groups**. Each group starts with a colored **bus** strip
and is followed by every channel currently routed into that bus, packed flush
against it with a glowing divider line between them. If you reroute a channel to
a different bus, its strip physically moves into that bus's group. Channels sent
straight to Master collect under a vertical **Direct Routing** label placed before
the FX group.

Group order, left to right: Direct Routing (if any), FX Bus + aux strips, Clips
Bus, Vox Bus, Inst Bus, Plugins Bus, Layers Bus, Bass Bus, RustyDrums Bus, Drums
Bus, Drums Bus 2. Each may be followed by its secondary bus when one exists.

Every strip in the scrolling area is 80 pixels wide; the pinned Master panel on
the left is 100.

### The buses

| Bus | What feeds it | When it is visible |
|---|---|---|
| Master | Every bus | Always (pinned left) |
| FX Bus | Aux strips | Always |
| Layers Bus | Layers tabs | When something routes to it |
| Bass Bus | Bass tabs | When something routes to it |
| Drums Bus | Drum tabs 1-16 (kit 1) | When something routes to it |
| Drums Bus 2 | Drum tabs 17-32 (kit 2) | When something routes to it |
| Clips Bus | Audio rows from the Builder | When something routes to it |
| Vox Bus | Vox (microphone) strips | When something routes to it |
| Inst Bus | Inst (live instrument) strips | When something routes to it |
| RustyDrums Bus | The 13 Rusty Drums kit strips | When a Rusty kit is loaded |
| Plugins Bus | Hosted VST3 instrument tabs | When something routes to it |
| Layers / Bass / Clips / Plugins / Vox / Inst Bus 2 (and Inst Bus 3) | Whatever you route there | After you add them from the Add menu |

The **two drum buses** exist because the Drum Kit grid's "1-16" and "17-32"
buttons are two independent kits, not one kit behind a filter. Drum tabs 1-16
land on Drums Bus, tabs 17-32 land on Drums Bus 2, and a drum channel can only be
rerouted to *its own* kit's drums bus or straight to Master - never across the
banks.

An added secondary bus appears immediately, even before anything routes into it.
Once it has had members and then empties again, it disappears on its own.

### The channel strip, top to bottom

| Control | What it does | Range / default |
|---|---|---|
| **Name** | The channel's label. Double-click to rename where editing is allowed (Layers, Bass, Audio/Clips, Plugin, Aux, Vox, Inst strips, plus the Vox Bus and Inst Bus strips). Drum, Rusty Drums, Master and the other system buses are fixed. For a strip that belongs to a tab, renaming the strip also renames the tab, and renaming the tab renames the strip. An Audio row whose Clips tab has been deleted has nowhere to store a name, so that rename is refused and the label snaps back with an explanation. | text |
| **Collapse arrow** (bus strips only) | Hides that bus's channel strips and closes the gap they leave. Grayed out when the bus has no members. | on / off, default expanded |
| **Meter** | The tall bar down the right side of the strip. The lower part is a peak meter with a ~1 second peak-hold marker; the upper part is a scrolling picture of the recent loudness (RMS). Range -60 dB up to +6 dB, so peaks past the digital ceiling are still visible. Hover for a live "L: -3.2 dB / R: -5.7 dB" readout. Master shows a full-height peak bar instead - it carries the loudness box. | display only |
| **M** | Mute. Silences this channel. | off |
| **S** | Solo. See "How solo behaves" below. | off |
| **FX Rack** | Jumps to the Effects page with this channel's rack selected. | button |
| **A** (Vox / Inst only) | Arm for recording. Left-click arms or disarms. Right-click opens the input picker (below). Hidden on Guitars / Basses Inst strips, which are played by an engine and have no live input. | off |
| **Headphones** (Vox / Inst only) | Listen. When on you hear this input through the bus and Master. When off the channel still processes, meters and records - it is just silenced on the way out, so you can arm a mic next to speakers without feedback. Right-click picks a monitor mode. | off |
| **FX Bypass** | Bypasses this channel's whole effects rack while keeping every setting. | off |
| **Master FX Bypass** (Master only) | Kill-all: bypasses every effects rack in the project regardless of each channel's own bypass. | off |
| **Pan knob** | Moves the channel left or right. Double-click to re-center. A popup while you drag reads "L 42%", "Center" or "R 17%". | -1 to +1, default 0 (Center) |
| **Polarity** (bus + insert strips) | Reads "Standard" or "Reverse". Reverse flips the waveform upside down - useful when two mics on one source cancel each other. Nothing about the volume changes. | Standard |
| **Width knob** | Stereo width. 100% is untouched, 0% collapses to mono, 200% exaggerates the stereo. Double-click returns to 100%. | 0-200%, default 100% |
| **Loudness box** (Master only) | Shows one of Momentary / Short Term / Integrated loudness (LUFS). Click it for a menu of the three; the choice is remembered on this computer. Reads `--` until there is signal. | display only |
| **Fader** | The channel volume. Double-click for 0 dB. At the bottom of its travel the readout reads "-inf" (silent). | -60 dB to +5.6 dB, default 0 dB |
| **dB readout** | The fader's current value in decibels. | display only |
| **Socket + "+"** | The green ring is the cable socket the routing lines attach to. The "+" button opens the routing menu (below). On Master this button reads **Analyzer** instead and opens the master analyzer window - a send *from* Master has nowhere to go. | button |

### How solo behaves

There are two independent solo layers.

* **Bus solo.** Soloing any bus silences every other bus at the master mix.
* **Channel solo.** Soloing any channel strip silences every other channel
  strip.

They do not affect each other: soloing one channel does not mute whole buses,
and soloing a bus does not change which channels inside it are audible.

Master is the exception in both directions. It has **no Polarity control** at all
(inverting the entire output just sounds broken), and its **S button is inert**:
no solo parameter is registered for Master, because there is no sibling to solo
it against, so clicking it lights the LED and changes nothing. Master's M button
does work.

### Routing: the "+" menu

Click the **"+"** at the bottom of any strip except Master - Master's button in
that position reads **Analyzer** and opens the master analyzer instead. Five
submenus on a normal strip; a strip whose main output is fixed (Master, every
bus, the Rusty Drums channels) gets only the first two:

* **Send...** - lists the aux strips you can send a copy of this channel to.
  Targets that would create a feedback loop, and targets on a strip whose four
  send slots are already full, are grayed out. **New Aux Strip** at the bottom
  creates an aux and wires the send to it in one step.
* **Sidechain...** - lists every other strip; the ones whose four sidechain
  receive lines are all in use, and any that would create a loop, are grayed
  out. A sidechain sends this channel's finished output to another channel as a
  *control* signal, so an effect over there (a compressor, a dynamic EQ band, or
  a hosted VST3 effect that has a side-chain input) can react to it. Each strip
  can receive four sidechains. Routing the cable is only half the job: the
  effect at the far end still has to pick that line from its own
  **Menu > `SC: Off...`** - see *Effect Racks.md*.
* **Move Output...** - changes where this channel's main output goes. Its
  current destination is ticked. Missing entirely on strips whose output is
  fixed: Master, every bus, and the Rusty Drums channels.
* **Add Main Out...** - gives this channel a second, third or fourth main
  output, so one strip can feed more than one destination at once. A strip
  already using all four lines, a destination one of its lines already feeds,
  and anything that would create a loop are all grayed out. Missing on the same
  fixed-output strips as Move Output.
* **Remove Main Out...** - drops one of those extra lines. The strip's original
  output is listed too, but grayed and marked `(main output)`, because a strip
  always keeps one.

Which destinations are legal depends on the family:

| Strip family | Main output may go to |
|---|---|
| Layers | Layers Bus, Bass Bus, their secondaries, Master |
| Bass | Bass Bus, Layers Bus, their secondaries, Master |
| Drums | that kit's own Drums Bus, Master |
| Plugin | Plugins Bus, Layers Bus, Bass Bus, their secondaries, Master |
| Audio (Clips) | any instrument or clips or vox or inst bus, Master (FX only via an aux send) |
| Vox | Vox Bus, Vox Bus 2, Clips Bus, Master |
| Inst | Inst Bus, Inst Bus 2/3, Clips Bus, Master |
| Aux | FX Bus, another Aux, Master |
| Rusty Drums | fixed to RustyDrums Bus (sends still allowed) |
| Buses and Master | fixed; sends may only go to aux strips |

### The cables

Routing is drawn as patch-bay cables that hang down between the strips' green
sockets. Main-output cables, sends and sidechains each have their own look.

* **Right-click a send cable** for a small panel with the send **Amount**
  (-60 dB to +6 dB, default 0 dB), a **Pre-Fader** toggle (off = the send is
  taken after the channel fader, so pulling the fader down also pulls the send
  down; on = the send ignores the fader), and **Delete Send**.
* **Right-click a sidechain cable** for a panel naming the source, the
  destination and the receive line, plus **Delete**. Sidechains have no level -
  they are always unity gain.
* **Main-output cables have no right-click panel.** Use **Move Output...** on
  the strip's "+" menu to change one.
* When several cables overlap under the cursor, right-click gives a chooser
  first.

### The Add menu (title strip of the Mixer window)

`Add` sits next to `Menu` on the Mixer window's title strip:

| Entry | Effect |
|---|---|
| Aux Strip | Creates a new aux (effect-send) strip |
| Vox Bus | Adds Vox Bus 2 |
| Inst Bus | Adds Inst Bus 2, then Inst Bus 3 |
| Layers Bus | Adds Layers Bus 2 |
| Bass Bus | Adds Bass Bus 2 |
| Clips Bus | Adds Clips Bus 2 |
| Plugins Bus | Adds Plugins Bus 2 |

Each bus row grays out once that bus exists. Vox and Inst *strips* are added from
the ribbon's "+" instead, not from here. Delete an aux or a secondary bus by
right-clicking the strip and confirming; anything pointing at it falls back to
its natural parent.

### The Menu (title strip of the Mixer window)

| Entry | What it does |
|---|---|
| **Pan Law** | How a pan sweep behaves, project-wide - every pan knob in the app follows it (strips, buses, master, the engine pan knobs, per-note pan, timeline clips). Two entries with a tick on the current one and a tooltip on hover: Ramped - the default - keeps a sound at its level at center and rises by up to 3 dB as it pans toward one side, so it feels equally loud anywhere in the field; Flat holds the side you pan toward at its level while the other side fades, so a sound is loudest at center and about 3 dB quieter at the sides. Both leave a centered sound untouched. Strips, buses and the master pan the FL way: the far side folds into the near side, so a 100% pan is the mono sum of both sides in one channel. |
| **Master Output** | Which physical outputs of your audio interface the mix goes to - each stereo pair, or any single output as mono. |
| **Latency-compensate meters** | Off by default. On, the meters are delayed to line up with what you are actually hearing through the speakers. |
| **Multi-core Rendering** | On by default. Off makes the audio engine do all the work on one thread - a diagnostic, not a feature. Takes effect on the next audio block, no restart. |


### Input picker (Vox / Inst strips)

Right-click the **A** (Arm) LED. The menu lists your audio interface's inputs -
stereo pairs first, then each channel on its own as mono - with the current pick
ticked. Choosing one only assigns the channel; it does **not** arm the strip, so
you can monitor an input without committing to record. Left-clicking the LED is
what arms. **Disarm** appears at the bottom while the strip is armed.

Vox strips get an extra **Builder Grid Default** section: which of the four
recorded takes (Dry, Dry Cleaned, Wet, Wet Cleaned) lands on the arrangement
grid. With nothing picked the app chooses automatically - the highest-order type
you enabled in File Settings, in the order Dry < Dry Cleaned < Wet < Wet
Cleaned. A pick holds until the project closes, and it only sticks on the first
six Vox strips; on Vox 7-10 the rows still draw but the automatic rule applies.
See `Vox Page.md`.

### Monitor modes (Listen LED right-click)

* **Vox:** True Dry (bare voice, nothing applied), Bypass Pitch Corrector (the
  chain's character without the correction), With Effect (the full chain
  including correction - the default). The *recorded* take is corrected in all
  three modes; this only changes what you hear live.
* **Inst:** Dry (raw input, lowest possible delay) or With Effect (the page's
  full processed chain - the default). Again, the recording and playback are
  identical either way.

---

## Parameters and persistence

Parameters are registered lazily, once per strip prefix, and are never removed -
so closing a tab and re-opening it at the same index restores that strip's
settings exactly.

**Per-strip APVTS parameters** (`prefix` is e.g. `mixer_master`, `mixer_layers`,
`mixer_layer_3`, `mixer_aux_0`):

| Parameter | Type | Range | Default | Applies to |
|---|---|---|---|---|
| `<prefix>_level` | Float (dB) | -60 to +5.6 | 0 | every strip |
| `<prefix>_pan` | Float | -1 to +1 | 0 | every strip |
| `<prefix>_width` | Float | 0 to 2 | 1.0 | every strip |
| `<prefix>_mute` | Bool | - | false | bus, insert, Master |
| `<prefix>_solo` | Bool | - | false | bus, insert |
| `<prefix>_polarity` | Bool | - | false | bus, insert |
| `<prefix>_bypass` | Bool | - | false | every strip |
| `<prefix>_collapsed` | Bool | - | false | buses only (view state) |
| `<prefix>_arm` | Bool | - | false | `mixer_vox_*` / `mixer_inst_*` only |
| `<prefix>_sendTo` | Int | 0-999 | natural parent bus | every strip |
| `<prefix>_send{0..3}_to` | Int | -1 to 999 | -1 (unused) | every strip |
| `<prefix>_send{0..3}_amount` | Float (dB) | -60 to +6 | 0 | every strip |
| `<prefix>_send{0..3}_prepost` | Bool | - | false (post) | every strip |
| `<prefix>_sc_recv{0..3}_from` | Int | -1 to 999 | -1 (empty) | every strip |
| `<prefix>_chokeGroup` | Int | 0-16 | 0 (none) | inserts only |
| `<prefix>_playNote` | Int | 0-127 | 60 | drum inserts only |

Vox and Inst strips additionally get `_inputChannelIdx` (Int -1..127, default -1),
`_listen` (Bool, default false), `_inputChannelStereo` (Bool, default false), and
`_monitorMode` (Vox: Int 0-2, default 2; Inst: Int 0-1, default 1). The chosen
input's display *name* is not a parameter - it is stored as a property on the
APVTS state tree under `<prefix>_inputChannelName`.

Every strip also carries two full 8-band mid/side EQ banks, post-rack at
`<prefix>_{mid|side}_eq{b}<Suffix>` and pre-rack at
`<prefix>_preeq_{mid|side}_eq{b}<Suffix>`. Those are documented with the Effects
and EQ systems, not here.

Global mixer parameters: `master_fx_bypass` (Bool, default false),
`master_pan_law` (Int 0-1, default 0 = Ramped; 1 = Flat). The hidden `masterGain`
parameter (default 0.8, never bound to a control) was deleted in QA-TrueLevel -
the master fader is the only gain on the master chain.

**Saved with the project:** the whole APVTS state (so every fader, pan, mute,
solo, width, polarity, bypass, collapse state, routing and send setting), plus -
in the editor's `<UIState>` element - the Mixer's horizontal scroll position
(`mixerScrollX`), the aux / Vox / Inst strip custom names and display orders, the
active secondary buses and their "has ever been routed" flags, and the
meter-latency-compensation toggle.

**Saved per machine, not per project:** the Master Output channel choice
(`master_output.xml` beside `audio_settings.xml` - different rigs have different
interfaces) and the Multi-core Rendering toggle (in `settings.xml`).

**Not saved at all:** meter readings, cable hit-test caches, and the
`_inputChannelIdx` picker's transient menu state.

**Undo.** A legacy `MixerState` snapshot still backs Master / Layers / Bass /
Drums / Clips bus level, pan, mute and solo, per-drum-slot level and pan, and
per-audio-row level and mute; dragging one of those faders or pans takes a
before/after snapshot as a single undo step. Everything else - every other bus,
every other insert control, all routing - lives only in APVTS and undoes through
the standard parameter-gesture mechanism.

---

## Lifetime and teardown

`MixerPage` is created once by `StandaloneEditor` and lives for the session; its
window is one of the four default tabs opened at launch. The page registers
Master and every bus parameter set eagerly at construction
(`ensureMixerBusAndMasterParams`); insert strips register theirs lazily on first
creation.

Strip widgets are created when a tab opens (`addLayerChannel`,
`addBassChannel`, `addDrumChannel`, `addAudioChannel`, `addPluginChannel`,
`addVoxChannelAtIndex`, `addInstChannelAtIndex`) and removed when it closes.
**Removing a strip removes the widget only** - the audio node and the APVTS
parameters survive, which is why re-adding at the same index restores prior
settings.

The Rusty Drums family is different: its 13 strips are created and destroyed in a
batch by the kit-load lifecycle, not by any user button, and `clearAllRustyChannels`
is the only teardown path.

Order that matters:

* An empty bus is hidden and consumes no width. That decision is made every
  layout pass from live membership, so it cannot go stale.
* Deleting an aux or a secondary bus first sweeps every strip's send and main-out
  parameters and resets anything pointing at the doomed channel back to its
  natural parent, *then* removes the node and the widget. Skipping the sweep
  would leave routing pointing at a channel that no longer renders.
* The 30 Hz poll and the vblank meter drain are started and stopped by
  `parentHierarchyChanged` keyed on whether the page is attached to a live
  window - never in the constructor, because a page can exist without a window.
* On project open, `clearDynamicStrips` wipes every non-bus, non-Master strip and
  resets the next-index counters before the new project's strips are built.

---

## Cross-references

* **Transport and Playback.md** - what starts and stops the audio the Mixer is
  metering; record-arm interacts with the Vox / Inst Arm LEDs.
* **Workspace and Windows.md** - the window the Mixer lives in, and how its
  position is remembered.
* **Keyboard Shortcuts.md** - F6 shows the Mixer.* **Master Analyzer.md** - the window the Master strip's Analyzer button opens.
* **Effect Racks.md** and **EQ.md** - the Mixer's FX Rack button and FX Bypass
  LED are the entry points into them.
* **Builder Page.md** / **Clips Page.md** - the audio rows that are the source of
  the Audio (Clips) strips.
* **Vox Page.md**, **Inst Page.md** - the two strip families with Arm and Listen
  LEDs.

---

## Differs from Carry-Forward

* **Bus solo is global, not per-group.** Carry-Forward section 3 describes solo as
  "pairwise within group" with a `silenced = muted || (inGroupSolo &&
  useGroupSolo && !soloed)` formula and lists it as an open defect (DSP-09). The
  code now has one formula for every bus, `silenced = muted || (anyBusSoloed &&
  !soloed)`, computed once per block by `BaySickGraph::anyBusSoloed()`, which reads
  bus `_solo` parameters only and is forbidden from consulting channel-level
  solo.
* **The five hand-written bus node types are gone.** Carry-Forward describes
  per-bus structs; every bus and Master now share one `InstrChannelNode`
  implementation.
* **The channel-id space has grown.** Carry-Forward predates Plugins Bus (13),
  Layers/Bass/Clips/Plugins Bus 2 (14-17), Drums Bus 2 (18), the Plugin insert
  family (900+), and the raised per-family caps.
* **MIX-01 (a Vox tab close not removing its mixer strip) is closed.** Every
  strip family now has a matching remove path.
* **Cable dragging was retired.** Carry-Forward's era had click-to-place send and
  sidechain cables and a draggable main-out socket; routing is now made entirely
  from the per-strip "+" menu, and only the right-click property popup survives
  on the cables themselves.
* **The five "Add ..." buttons on the Mixer strip are gone**, replaced by the
  `Add` menu on the window's title strip.
