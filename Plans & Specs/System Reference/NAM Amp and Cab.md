# NAM Amp and Cab (BaySickNAM/IR)

**Purpose** - BaySickNAM/IR is an amp and speaker simulator. It loads a neural
network "capture" of a real amplifier (a `.nam` file) and an impulse response of a
real speaker cabinet (a `.wav` file), then adds two virtual microphones in front
of that cabinet. On a Vox tab it is the last stage of the vocal chain, useful for
grit and character; the same engine is what Inst tabs use for guitar and bass. It
also carries its own A/B compare so you can hold two complete amp rigs and flip
between them.

## How it operates

`BaySickNAMIRProcessor` (`Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp` / `.h`).
Every Vox tab and every Inst tab owns its own instance with its own parameter set.
Signal order in one audio block:

```
input
  -> input gain
  -> noise gate                 (before the model, so the gate hears the dry signal)
  -> mono sum
  -> [oversample 2x or 4x] -> NAM model -> [downsample]
  -> broadcast mono to stereo
  -> Low Cut (HPF) -> High Cut (LPF)
  -> cabinet IR convolution, blended by Cab Mix
  -> Mic A: MicSimDSP -> MicPlacementDSP        (in place)
  -> Mic B: MicSimDSP -> MicPlacementDSP        (parallel copy of the post-cab tap, SUMMED in)
  -> master output gain
```

- **Passthrough gate.** If neither A/B slot has a NAM model loaded *and* neither
  has a cabinet IR loaded, the whole stage returns immediately and audio passes
  through untouched. Nothing else on the panel does anything until you load a
  model or an IR - including the mic simulation.
- **Two slots, both resident.** `ab_slot` picks A or B. Each slot has its own NAM
  model, its own cabinet IR, its own Mic A and Mic B user IRs and its own snapshot
  of every tone parameter. Both slots' models and IRs stay loaded in memory, so
  switching is instant with no reload.
- **File loading is message thread.** A new NAM model is built and pre-warmed off
  the audio thread, parked under a lock, and adopted by the audio thread at the
  start of the next block by a swap-pending flag - so the model never allocates or
  frees on the audio thread. The audio thread decides what is running from the
  loaded/unloaded atomics, never from the stored file paths.
- **Cabinet IRs are decoded before handing to the convolver**, because JUCE's
  convolution only queues the file to its own thread and would silently turn an
  unreadable file into a pass-through impulse. An unreadable file therefore fails
  loudly instead.
- **Full-rig detection.** If the loaded `.nam` capture's metadata says it already
  includes a cabinet, the panel shows a hint suggesting you bypass the IR.
- **Oversampling** (1x / 2x / 4x) wraps only the NAM inference. Changing it
  re-warms every loaded model at the new rate and updates the reported latency.
- **Mic B ramps.** Turning Mic B on or off glides its summed branch over about
  15 ms rather than stepping, and its filters and convolution are flushed on the
  way in so it cannot replay stale audio. When it has settled at off, the whole B
  path costs one parameter read.

## User-facing behavior

On a Vox tab, open it from the tab's **Menu -> NAM/IR**. It opens in its own
window (843 x 563 minimum). Anywhere on the panel accepts a dropped `.nam`
(loads as the amp) or `.wav` (loads as the cabinet IR).

### Title strip

**A** / **B** buttons pick the slot. Each slot loads its own amp model and cab IR;
click the other letter to compare two rigs.

### AMP row

- **Load .nam...** - left-click opens a file browser (rooted at the app's
  `Presets/BaySickNAMIR/NAM` folder); **right-click** opens a 10-deep recent-files
  menu with a "Clear recent" item. The recents list is stored in the app's
  settings file, not the project.
- An amber readout shows the loaded capture's filename, or `(no model loaded)`. If
  the project remembers a file that could not be loaded, the name is shown in red
  with ` (missing)` appended - the panel never shows a name it did not actually
  load.
- **OFF / ON** switch - bypasses the neural amp model. Up is bypass off (the model
  is processing); down is bypass on (dry input passes straight through to the
  filters and cab).
- When the loaded capture already includes a cabinet, a hint line appears:
  "Full-rig model - cabinet already included. Consider bypassing the IR."

### CAB row

- **Load .wav IR...** - same left-click browse (rooted at `Presets/BaySickNAMIR/IR`)
  and right-click recents behavior.
- A green readout shows the loaded IR filename, `(no IR loaded)`, or the name in
  red with ` (missing)`.
- **OFF / ON** switch - bypasses the cabinet convolution. Use it when the amp
  capture is a full rig.

There is no clear button for the amp model or the cab IR; loading a different file
replaces it.

### Knobs

| Knob | What it does | Range | Default |
|---|---|---|---|
| **Input Gain** | How hard the signal hits the model. Set it so the model is driven at roughly the level it was captured at - too low sounds thin, too high sounds fizzy. | -24 to +24 dB | 0 dB |
| **Gate Thr** | Noise gate threshold. Anything quieter than this is silenced, which kills hiss and hum between phrases. | -60 to 0 dB | -50 dB |
| **Gate Rel** | How long the gate takes to close once the signal drops below the threshold. | 5-500 ms | 100 ms |
| **Low Cut** | High-pass before the cab. Tightens the low end. | 20-500 Hz | 20 Hz |
| **High Cut** | Low-pass before the cab. Tames fizz and harshness. | 3000-20000 Hz | 20000 Hz |
| **Cab Mix** | Blend between the cabinet and the un-cabbed signal. 100 is full cab, 0 is none. | 0-100 % | 100 % |
| **Output** | Master output level, to match this stage against the rest of the mix. | -24 to +12 dB | 0 dB |

**OS** selector - oversampling around the amp model. **1x** is cheapest, **2x**
reduces aliasing on high-gain models, **4x** is best quality and the most CPU.

A red error line under the knobs reports the reason whenever a load fails.

### MIC SIM A and MIC SIM B

Two independent virtual microphones over the same post-cabinet signal. **Mic B is
summed, not blended** - two real mics on one source add together, and with
identical settings the two together are about 6 dB louder than one. A **Mic B
Active** OFF / ON switch turns the second mic on; with it off the chain is
byte-identical to a single-mic chain.

Each mic has a **Mode** selector:

| Mode | What it does |
|---|---|
| **Off** | No mic coloring at all - the cabinet output passes straight through. |
| **Built-in** | Applies one of ten built-in mic voicings. These are **EQ curves**, not impulse responses: four parametric bands per model, shaped to approximate the published frequency response of that kind of microphone. |
| **User IR** | Loads a real captured microphone impulse response from a `.wav` file and convolves the signal through it. Nothing is bundled - you supply the file. |

In **Built-in** mode a **Model** dropdown appears with the ten voicings. Hovering
an entry shows what it is good for.

| On-screen name | What it is good for | What it sounds like |
|---|---|---|
| **Live Vocal Dynamic** | Live vocals, sturdy stage mic | Cuts the low end back hard, pushes a strong 5 kHz presence forward, softens the very top. Cuts through a mix. |
| **Broadcast Dynamic** | Podcasting, voiceover, smooth vocals | Nearly flat with a gentle 4 kHz lift and a softened top. Smooth and close-sounding. |
| **Workhorse Cardioid** | Backing vocals, brighter lead vocals | Lean low end with a big 6 kHz lift and extra air. Bright and forward. |
| **Vintage LDC '87** | Studio lead vocal, classic large-diaphragm sound | A touch of low warmth plus an open, lifted top end. The default "expensive studio vocal" flavor. |
| **Modern LDC** | Modern vocals, voiceover | Clean lows and a strong, very extended top. Detailed and modern. |
| **Multi-Pattern LDC** | Versatile studio work, pop vocals | Slight low-mid trim with a wide air lift up top. Polished. |
| **Tube LDC** | Character vocals, jazz, soul | Warm lows, a gentle low-mid dip, sweet upper-mids and top. Rich rather than clinical. |
| **Pencil SDC** | Acoustic guitar, drum overheads, hi-hat | Almost flat with a small lift of air. Accurate rather than flattering. |
| **Ribbon** | Guitar amps, brass, character vocal | Warm and noticeably dark - the top end is pulled well down. Smooths harsh sources. |
| **Kick Drum** | Kick drum, bass cabinets | Big low boost, deeply scooped mids and a hard click around 4 kHz. |

In **User IR** mode a **Load Mic IR...** button and its own filename readout
appear. Left-click browses (rooted at `Presets/BaySickNAMIR/MIC IR`);
**right-click clears** the loaded IR. A successful load automatically switches the
Mode selector to User IR. Each A/B slot holds its own mic IR, so switching slots
never reloads a file.

A **Mix** knob per mic (0-100 %, default 100 %) blends the mic coloring against
the uncolored signal.

### MIC PLACEMENT A and MIC PLACEMENT B

Where each virtual mic sits in front of the cabinet.

| Control | What it does | Range | Default |
|---|---|---|---|
| **Distance** | How far the mic is from the source. Closer is louder, brighter, and adds proximity-effect bass (peaking near 1 cm and gone by about 20 cm); further is quieter and darker as air absorbs the top end. | 1-150 cm | 30 cm |
| **Angle** | How far off-axis the mic points. 0 is straight on. Moving off-axis past about 15 degrees progressively darkens the top end and reduces level according to the polar pattern. | -90 to +90 degrees | 0 |
| **Polar** | The pickup pattern: **Omni** (equal pickup all round, no proximity effect and no off-axis darkening), **Cardioid** (heart-shaped, rejects the rear), **Supercardioid** (tighter, small rear lobe), **Hypercardioid** (sharper still, pronounced side rejection), **Figure-8** (bidirectional, equal front and rear, strong side rejection). | five patterns | Cardioid |
| **Mix** | Blend between the placed and unplaced signal. | 0-100 % | 100 % |

Offsetting Mic B's distance and angle from Mic A's is what produces the
comb-filtered color you get from two real mics at different distances.

## Parameters and persistence

Parameter ids are bare (no engine prefix), because each host page owns its own
instance. Automation keys are prefixed per page (`vox<N>_`, `inst<N>_`) so lanes
on different pages stay distinct.

| Parameter | Range / values | Default |
|---|---|---|
| `input_gain` | -24 to +24 dB | 0 |
| `output` | -24 to +12 dB | 0 |
| `gate_threshold` | -60 to 0 dB | -50 |
| `gate_release` | 5-500 ms | 100 |
| `nam_bypass` | bool | false |
| `cab_bypass` | bool | false |
| `low_cut` | 20-500 Hz | 20 |
| `high_cut` | 3000-20000 Hz | 20000 |
| `cab_mix` | 0-100 % | 100 |
| `oversampling` | 1x / 2x / 4x | 1x |
| `ab_slot` | A / B | A |
| `nam_micsim_mode` | None / Built-in / User IR | None |
| `nam_micsim_model` | the ten built-in models | Live Vocal Dynamic |
| `nam_micsim_mix` | 0-100 % | 100 |
| `nam_placement_distance_cm` | 1-150 cm | 30 |
| `nam_placement_angle_deg` | -90 to +90 deg | 0 |
| `nam_placement_polar` | Omni / Cardioid / Supercardioid / Hypercardioid / Figure-8 | Cardioid |
| `nam_placement_mix` | 0-100 % | 100 |
| `nam_micb_active` | bool | false |
| `nam_micsim_b_mode` / `_b_model` / `_b_mix` | same as Mic A | same as Mic A |
| `nam_placement_b_distance_cm` / `_b_angle_deg` / `_b_polar` / `_b_mix` | same as Mic A | same as Mic A |

**Saved with the project** (and with a page preset, because a page preset captures
the host engine's state, which embeds this one):

- the parameter tree;
- five path properties - `nam_filepath`, `ir_filepath`, `nam_filepath_b`,
  `ir_filepath_b`, and `mic_user_ir_path` (the last read only as a fallback for
  projects saved before per-slot snapshots existed);
- `<SlotA>` and `<SlotB>` - the per-slot tone snapshots: input gain, output, gate
  threshold and release, low cut, high cut, cab mix, both bypasses, the Mic A and
  Mic B sim mode / model / mix and user-IR path, and both mics' placement distance,
  angle, polar and mix. `ab_slot` and `oversampling` are deliberately excluded -
  they are settings of the whole stage, not of one slot.

A save always captures the live slot's values into its snapshot first, so what is
on screen is what gets stored.

**On load** the paths are restored, then each slot's NAM model and cabinet IR are
loaded, then both snapshots are restored, then each slot's Mic A and Mic B user
IRs are loaded into their dedicated resident buffers, then the active slot's
snapshot is applied to the parameters. Anything that is missing or fails to load
is reported through the project's missing-file report, named "NAM model",
"Amp IR", "Mic A user IR" or "Mic B user IR", with "(failed to load)" appended
when the file existed but could not be read.

**Per machine, not per project:** the recent-`.nam` and recent-IR lists, stored in
`Documents/BaySickDAW/settings.xml`.

**Not saved:** the Mic B activation ramp state, the gate envelope, and the
oversampling filter state.

**Diagnostics:** a Debug-build-only log of every save and load is appended to
`Documents/BaySickDAW/namir_state_log.txt`. Release builds write nothing.

## Lifetime and teardown

- On a **Vox** tab the processor is constructed by the `BaySickVocalProcessor`'s
  constructor and owned by it, which is why its state is captured automatically
  by the Vox page preset. Audio reaches it after the vocal chain rack.
- On an **Inst** tab it is one stage of a rig-owned chain (pedals then NAM/IR
  behind a wrapper processor). Teardown order there is load-bearing: the rig's tab
  must be removed before the sfizz engine is destroyed.
- `prepareToPlay` re-prepares both convolutions, both mic stages, rebuilds the
  oversampling chains, re-warms every loaded model at the active oversampling
  rate, invalidates the gate coefficient cache and re-reports latency.
- The destructor drops its lifetime token first, so any queued A/B swap that is
  already in the message queue bails out instead of touching a dead processor,
  then removes its parameter listeners.
- The editor subscribes to `onStateRestored` so the filename readouts refresh
  after a project or preset load; without it the labels would still show
  "(no model loaded)" while the model was in fact running.

## Cross-references

- `Vocal Chain.md` - the stage this one sits after on a Vox tab.
- `Vox Page.md` - how the NAM/IR window is opened and how the tab's state is
  saved.
- `Inst Page.md` - the other host. Every Inst tab (live input, BaySickGuitars
  or BaySickBasses) owns its own separate instance of this stage.
- `Pedalboard.md` - the stage that runs immediately before this one on an
  Inst tab.

## Differs from Carry-Forward

Carry-Forward (2026-05-07) mentions BaySickNAM/IR only in passing, as a candidate
for the shutdown-gate pattern. It predates the whole mic section: the built-in
mic models, the User IR mode, mic placement, and the second parallel Mic B are all
newer than that snapshot, as is the per-slot A/B snapshot of the full tone state.
