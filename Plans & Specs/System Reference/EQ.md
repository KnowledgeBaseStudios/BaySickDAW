# EQ

**Purpose** - An equalizer changes how bright, warm, thick or thin something
sounds by turning specific frequency ranges up or down. BaySickDAW gives every
channel two of them: one before its effect rack and one after. Each is an
8-band parametric EQ with a Mid bank and a Side bank, per-band stereo routing,
optional dynamic (level-following) bands, phase modes from ordinary to fully
linear-phase, and an A/B compare pair.

---

## How it operates

Three classes:

| Class | File | Role |
|---|---|---|
| `EQ8DSP` | `Source/DSP/EQ8DSP.*` | One 8-band bank. `kNumBands = 8`, up to `kMaxSections = 4` cascaded biquads per band |
| `EQ8MsDSP` | `Source/DSP/EQ8MsDSP.*` | The thing a strip actually owns: two `EQ8DSP` instances (`mid()` and `side()`) run in series on the full stereo buffer |
| `ParametricEQDisplay` | `Source/Standalone/SharedUI.*` | The widget: graph, band handles, right-hand column of controls, options menu |

The M/S split is a **storage and view** split, not a processing one. Both inner
banks see the full stereo buffer; each band inside them carries its own
`channel` field (Stereo / Mid / Side / L only / R only) and does its own
encode/decode when it needs to. The wrapper seeds the mid bank's bands to
`Mid` and the side bank's to `Side` so the default behavior matches what the
names say. `showMid` on the wrapper is a UI hint and does not affect audio -
both banks always run.

**Two per strip.** `VibeGraph::InsertNode` holds `preEq` and `eq`, and the bus
nodes hold the same pair. The strip's block order is: freeze tap, **preEq**,
polarity, stereo width, the effect rack, **eq**, then fader / mute / solo. The
Effects window's `Pre EQ` and `Post EQ` buttons open these two.

**Per band** the DSP builds 1-4 biquad sections, or uses a zero-delay-feedback
state-variable filter for Low Pass, High Pass and Band Pass (those keep their
analog shape near Nyquist where a biquad cascade warps). Peaking, shelves,
Notch, Off and Tilt stay on biquads. Frequency, gain and Q are smoothed and the
coefficients are rebuilt at block rate only when something actually moved.

**Dynamic bands** run a detector bandpass at the band's own frequency and Q
over a saved copy of the block's *original* input, so band N's own gain
reduction cannot feed its own detector. An envelope follower with separate
attack and release feeds a gain computer that either compresses above the
threshold or expands below it; the resulting gain is folded into the band's
coefficients. Only gain-bearing types support it (Peaking, Low Shelf, High
Shelf, Tilt). A band can be driven by one of the strip's sidechain receive
lines instead of its own input.

**Phase modes** (`EQ8DSP::PhaseMode`) change the engine:

| Mode | Value | What runs | Linear FFT size |
|---|---|---|---|
| Standard | 0 | Ordinary minimum-phase filters | - |
| Linear | 1 | Linear-phase FFT convolution | whatever Linear Phase Precision is set to (default 2048) |
| HQ Plus | 2 | Anti-cramping forced on; no FFT path | - |
| HQ Linear | 3 | Anti-cramping forced on plus linear-phase FFT | 4096, fixed |
| HQ Extended | 4 | Linear-phase FFT, low latency | 512, fixed |

Linear-phase latency is FFT size / 2 per inner bank, and the M/S wrapper runs
Mid then Side, so a wrapped EQ reports twice that. `EQ8MsDSP::getLatencySamples`
sums both banks and the graph feeds it into delay compensation. Every mode
change arms an output fade scaled to the latency shift (up to 4096 samples) so
the seam is not audible.

Two things are restricted while a linear mode is active, and the menu grays
them rather than hiding them: per-band channel routing is forced to Stereo (the
linear-phase impulse is one combined response over L/R), and dynamic bands are
disabled (the impulse is built from the static design).

**Anti-cramping** is a 2x half-band oversampler around the whole band chain.
Coefficients are designed at the oversampled rate so the drawn curve and the
audible curve match. It roughly doubles this EQ's CPU, so it is off by default,
and it reports its own small latency for compensation.

**Spectrum feeds.** `EQ8MsDSP` taps a mono mixdown at the input (`preFeed`) and
the output (`postFeed`) of its process. The display polls both at 30 Hz for the
gray "before" and colored "after" analyzer overlays. These are seqlock feeds:
wait-free on the audio thread, and a frame the UI catches mid-write is simply
dropped.

---

## User-facing behavior

### Opening an EQ

From the **Effects** window, press **Pre EQ** or **Post EQ**. Each opens in its
own window and stays fixed to that one for its life. The window's title strip
has a two-tab strip - clicking the other tab **opens the other window** rather
than swapping this one's contents, so you can have both on screen at once.
Every content tab's own window menu (Layers, Bass, Drums, Rusty Drums, Clips,
Plugins, Vox, Inst) also carries **Pre EQ** and **Post EQ** rows pointing at
that tab's own strip.

Pre EQ shapes the sound going *into* your effects; Post EQ shapes what comes
*out* of them. If you are just making something brighter or thinner, either
works. If you are stopping a compressor from reacting to boomy bass, use Pre.

### The graph

The big display shows frequency left (20 Hz) to right (20 kHz) and gain from
+18 dB at the top to -18 dB at the bottom, with the curve your settings
produce drawn across it.

| Gesture | What it does |
|---|---|
| Drag a handle | Left/right changes that band's frequency, up/down changes its gain. Bands with no gain (Low Pass, High Pass, Notch, Band Pass) only move sideways |
| Hold Ctrl while dragging | Fine adjust - one tenth the sensitivity |
| Scroll wheel over a handle | Q: up narrows, down widens |
| Alt+click a handle | Resets that band's gain, Q, slope, mute and solo |
| Right-click a handle | The band menu (below) |
| Click the small `M` chip under a handle | Mutes that band |
| Click the small `S` chip under a handle | Solos that band - only soloed bands are heard |
| Hover a handle | A panel appears listing that band's type, frequency, gain, Q, routing and state, and, for a dynamic band, its dynamic settings and a live gain-reduction meter |

Two spectrum overlays draw behind the curve: a translucent gray one showing the
sound going into the EQ, and a brighter one showing what comes out. When they
differ, you are seeing exactly what the EQ did.

### The band menu (right-click a handle)

| Item | Choices |
|---|---|
| **Filter Type** | Bell, Low Pass, High Pass, Low Shelf, Hi Shelf, Notch, Off, Band Pass, Tilt |
| **Slope / Order** | Center-2 (12 dB/oct), Steep-4 (24), Steep-6 (36), Steep-8 (48), Gentle-4 (LR 24), Gentle-6 (LR 36), Gentle-8 (LR 48) |
| **Channel** | Stereo, Mid, Side, L Only, R Only. Grayed with `(disabled in Linear modes)` when a linear phase mode is on |
| **Automate** | Freq, Gain, Q, Type, On, Slope, Mute, Solo, Channel - creates an automation lane for that band control |
| **Make Dynamic** | Turns this band into a level-following band. Only available on Bell, Low Shelf, Hi Shelf and Tilt; grayed with `(disabled in Linear modes)` in a linear mode |
| **Dynamic Params...** | Opens the dynamic panel (below). Only when the band is already dynamic and the EQ is a strip EQ with parameters behind it - which is what every Pre EQ and Post EQ window is |
| **Reset Band** | Frequency back to this band's default, gain 0, Q 0.707, slope Center-2. Type, on/mute/solo and channel routing are left alone |

Switching a band to a type that has no gain (Low Pass, High Pass, Notch, Band
Pass, Off) zeroes its gain so the fader and the curve do not show a stale
value.

### The right-hand column

One column per band, plus a ninth for the EQ's own output:

| Control | Range | Default | What it does |
|---|---|---|---|
| Colored dot at the top of the column | on/off | on | Click to switch that band on or off |
| Type dropdown | Bell / LP / HP / LShelf / HShelf / Notch / Off / BPass / Tilt | Bell | Same list as the right-click menu |
| Gain fader | -18..+18 dB | 0 | Vertical, bipolar. Double-click resets to 0. Disabled for types with no gain |
| Freq knob | 20..20000 Hz | 40, 250, 500, 1000, 2000, 4000, 8000, 12000 Hz for bands 1-8 | Double-click resets to that band's default |
| Q knob | 0.1..10 | 0.707 | How wide the band is. Double-click resets |
| Main Level fader (ninth column) | -18..+18 dB | 0 | The EQ's own output trim. Double-click resets |

Under each fader and knob is a numeric readout. Double-click a readout to type
a value in directly; Enter commits, Escape cancels.

### Mid and Side

The title strip has **MID** and **SIDE** buttons. They choose which of the two
banks the graph and the right-hand column are editing. Both banks always
process - switching the view never changes the sound. Mid is the center of the
stereo image (what is common to both speakers, usually vocals, bass and kick);
Side is what differs between them (usually width and ambience). Cutting mud
from Mid without touching Side is a common trick.

### Dynamic bands

Right-click a band and choose **Make Dynamic**, then **Dynamic Params...**. A
small panel opens beside the band with:

| Control | Range | Default | What it does |
|---|---|---|---|
| Threshold | -60..0 dB | -18 | The level at which the band starts moving |
| Ratio | 1..20 | 2 | How much it moves |
| Attack | 0.1..500 ms | 10 | How fast it reacts |
| Release | 1..2000 ms | 100 | How fast it lets go |
| Range | -18..+18 dB | 0 | Bipolar. Negative = cut when the band gets loud. Positive = boost when the band gets quiet. Zero = no movement. Double-click resets to 0 |
| Sidechain source button | Off, or one of the strip's routed lines | Off | Drives this band from another channel instead of its own input |
| GR readout | - | - | Live, showing the band moving |

Turning Make Dynamic on always sets Range to 0, so the dotted "where it can go"
curve starts flat and you dial in the direction yourself. Turning it off leaves
Range alone so re-enabling brings your setting back.

### The EQ window's Menu

| Item | What it does |
|---|---|
| **Reset All Bands to Default** | Every band back to its default frequency, 0 dB, Q 0.707, Bell, Center-2, on, unmuted, unsoloed |
| **A/B Compare (swap to A/B bank)** | Swaps to the other bank. The label tells you where the click will take you |
| **Copy A -> B (seed spare bank)** | Copies the bank you are on into the other one, so you can start comparing from a known match |
| **Lock both banks (freeze A and B)** | Freezes both banks so a drag cannot change either. Useful while you are listening back and forth |
| **Heatmap overlay** | Frequency energy over time drawn behind the curve |
| **Phase curve overlay** | Phase shift against frequency |
| **Anti-cramping (2x OS)** | Doubles the internal sample rate so the top octave keeps its shape. Costs roughly twice the CPU and adds a little latency, which is compensated automatically. Only offered when the window is bound to a live EQ |
| **Processing Mode** | Standard (minimum-phase) / Linear Phase / HQ+ (oversampled) / HQ Linear / HQ Extended (low-latency linear). Each entry shows the latency it adds at the current sample rate, e.g. `[+2048 sp]` |
| **Linear Phase Precision (Linear Phase mode)** | 256 (low CPU) / 512 / 1024 / 2048 (default) / 4096 (high). Sets the FFT size used by the plain Linear Phase mode only - HQ Linear and HQ Extended keep their own fixed sizes, which is why the submenu names the mode it governs |
| **IIR Mod Speed** | Instant (~1 ms) / Fast / Medium / Slow / Slowest (~50 ms). How quickly a knob move is applied. Slow avoids zipper noise; Instant is snappier |
| **Proportional Q (analog console feel)** | On by default. Bell bands narrow as you boost them, the way console EQs behave. No effect on shelves, filters, notch or band pass |

The title strip also carries a **bank indicator** reading `A Bank` (green) or
`B Bank` (red). Click it to swap - it is a shortcut for A/B Compare.

**Plain English on phase modes:** Standard is what every ordinary EQ does and
is the right choice nearly always. Linear Phase keeps every frequency in
perfect time with every other, which can help on a mix bus, but it delays the
sound and can smear sharp transients. The `HQ` modes are the same ideas with
oversampling on top. The latency numbers in the menu are what you are paying.

---

## Parameters and persistence

### APVTS parameters (bus and insert EQs)

Per-band parameters exist for both banks and both positions. The id shape is:

```
<mixerPrefix>_[preeq_]<mid|side>_eq<band><Suffix>
```

for example `mixer_layer_0_mid_eq3Gain` (post-rack) or
`mixer_master_preeq_side_eq0Freq` (pre-rack). `band` is 0-7.

| Suffix | Type | Range | Default |
|---|---|---|---|
| `Freq` | float | 20..20000 Hz | 40, 250, 500, 1000, 2000, 4000, 8000, 12000 by band |
| `Gain` | float | -18..+18 dB | 0 |
| `Q` | float | 0.1..10 | 0.707 |
| `Type` | int | 0..8 | 0 (Bell) |
| `On` | bool | - | true |
| `Slope` | int | 0..6 | 0 |
| `Mute` | bool | - | false |
| `Solo` | bool | - | false |
| `Channel` | int | 0..4 | 1 (Mid) in the mid bank, 2 (Side) in the side bank |
| `Dynamic` | bool | - | false |
| `Threshold` | float | -60..0 dB | -18 |
| `Ratio` | float | 1..20 | 2 |
| `Attack` | float | 0.1..500 ms | 10 |
| `Release` | float | 1..2000 ms | 100 |
| `Range` | float | -18..+18 dB | 0 |
| `Upward` | bool | - | false |
| `ScSource` | int | -1..999 | -1 (internal) |

These parameters are the source of truth for a bus or insert EQ: they are
re-pushed to the DSP every audio block from a pre-resolved pointer cache
(`VibeSynthProcessor::updateEQFromCache`). After an A/B swap the widget writes
the newly-visible bank back into the parameters so the next block cannot undo
the swap.

`Upward` is retained scaffolding and is not user-editable - the direction is
carried by the sign of `Range`.

### Global EQ settings

Main Level, Processing Mode, Linear Phase Precision, IIR Mod Speed,
Proportional Q, Anti-cramping and the A/B bank state have **no** parameter
mirror. The widget writes them straight onto the DSP, so the DSP's own state
blob is where they are stored.

### Saved with a project

`EQ8DSP` writes an `<EQ8DSP>` tree: eight `<Band>` children carrying
index/freq/gainDb/q/type/slope/on/muted/soloed/channel plus the dynamic fields,
a nested `<Spare>` child holding the A/B spare bank's eight bands, and root
properties `viewingSpare`, `mainLevel`, `iirModSpeed`, `proportionalQ`,
`antiCramping`, `phaseMode` and `linearPrec`. A legacy `EQ6DSP` tag is still
accepted on read. `EQ8MsDSP` nests the two inner blobs base64 under `mid` and
`side`.

`antiCramping` and `phaseMode` are re-applied through their setters rather than
stored raw, because both re-prepare the filter chain and change reported
latency. `viewingSpare` is saved so the app knows which bank the stored bands
*are*.

`VibeGraph::saveRackStates` writes each strip's `preEq` and `eq` blobs beside
its rack blob into the project.

### Saved with a preset

An FX Rack preset (Effects window Menu > Save FX Rack Preset) carries the
strip's four EQ parameter families - `<prefix>_mid_eq`, `<prefix>_side_eq`,
`<prefix>_preeq_mid_eq`, `<prefix>_preeq_side_eq` - stored as suffixes with
their values in natural units, so a preset saved on one strip loads onto
another. Note this covers the *parameter* half only: the global settings listed
above ride in the rack blob's EQ state rather than in these entries.

### Not saved

Spectrum and heatmap history, live gain-reduction readouts, which of MID or
SIDE the window is showing, and the lock state of a bus EQ's bands (the widget
holds that in the UI, deliberately not in the DSP blob).

---

## Lifetime and teardown

An `EQ8MsDSP` is a plain member of the graph node that owns it - two per bus
node and two per `InsertNode` - so it is created and destroyed with that node
and is never separately allocated.

`EffectEqWindow` holds a channel id and a Pre/Post flag and re-resolves the EQ
on every 30 Hz tick through `EffectsPage::preEqForChannelId` /
`EffectsPage::resolveChannelDsp`. Nothing is cached across ticks because a
strip respawn or a project load can rebuild the node underneath the window; if
the resolved pointer changes, the display re-binds. Until the window has
resolved once it draws against a private display-only fallback EQ, so the curve
is visible before the graph has built the node. Once it has resolved at least
once, a resolve that then fails means the channel itself is gone and the window
closes.

The dynamic-parameters pop-up asks the display for the live DSP rather than
trusting the pointer it was handed at construction, for the same reason.

Ordering that matters: a phase-mode or anti-cramping change moves the reported
latency, so the display fires `onLatencyChanged`, which the window wires to
`VibeGraph::updateBusLatencies` -> `setLatencySamples`. Both halves must run or
compensation goes stale. The linear-phase processor is allocated and freed only
from the message thread, never inside `process()`.

---

## Cross-references

- **Effect Racks.md** - the six slots between Pre EQ and Post EQ, and the
  windows that open both.
- **Effect Modules.md** - the effects themselves.
- **Pedalboard.md** - the pedal-style EQs in the board's last position, which
  are separate three- and seven-band units, not this EQ.

---

## Differs from Carry-Forward

The Carry-Forward Reference snapshot records `NEVER-01` - per-band EQ
parallelism - as "not actively planned; not blocked from future
reconsideration if EQ topology changes". That still holds; nothing here
implements it. Carry-Forward has no other EQ content, so there is no further
delta.

**Where the audit captures disagree with the code:** an earlier sweep noted
that `EQ8DSP` omitted the A/B spare bank and `mLinearPrec` from its saved
state, and that Linear Phase Precision was inert. Neither is true of the
shipping code: `<Spare>`, `viewingSpare` and `linearPrec` are all serialized,
and `EQ8DSP::linearFftSize` uses the precision to pick the plain Linear mode's
FFT size.
