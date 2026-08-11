# Effect Modules

**Purpose** - These are the individual effects you can drop into a rack slot or
a pedalboard slot: compressors, reverbs, delays, drives, modulation, and the
guitar/bass pedal set. Each one is a self-contained processor with its own
panel, its own preset library and its own automation lanes. This document lists
every module and every control it exposes.

---

## How it operates

Every module derives from `DSPBase` (`Source/DSP/DSPBase.h`) and lives in
`Source/DSP/`. `EffectRack::createEffect(EffectType)`
(`Source/EffectRack.cpp`) is the one factory; it returns `nullptr` for an
ordinal this build does not know, and a slot holding no DSP is skipped on the
audio path.

`createEffectEditor(DSPBase*, EffectType, PanelMode)`
(`Source/Standalone/EffectEditorPanels.cpp`) builds the panel. Some effect
types build different panels depending on the DSP's own mode - a Compressor set
to FET builds `FETCompressorPanel`, a Saturation set to Tape builds
`TapeSatPanel` - which is why switching Mode rebuilds the panel rather than
hiding knobs. `PanelMode::Pedal` selects the small pedalboard face for the
seven effects that have one (Limiter, Saturation, Chorus, Flanger, Phaser,
Delay, Reverb) and strips the meters.

Panels are DSP-direct: a knob writes straight to the DSP, and a 10 Hz
`DisplaySync` timer reads values back through `EffectParamMap` so an automated
control keeps up with the sound. `EffectParamMap`
(`Source/DSP/EffectParamMap.cpp`) is the single table of (effect type, variant)
-> control suffix -> setter/getter/range; it is what makes an automation lane
and a knob at the same position land on the same value.

Nine DSP classes publish a live picture into `EffectVisualFeed` for their
Visual window: `ChorusDSP`, `CompressorDSP`, `DelayDSP`, `FlangerDSP`,
`LimiterDSP`, `PhaserDSP`, `ReverbDSP`, `SaturationDSP` (all three of its
modes, so Tape is covered) and `TransientShaperDSP`. Publishing costs one
relaxed atomic load per block while nothing is watching.

Six built-in modules consume a sidechain (`usesSidechain()` returns true):
Compressor, Delay, Limiter, Reverb, Transient Shaper, and the pedal Noise Gate.
A hosted VST3 effect is a seventh case and the only one that is not a fixed
property: `HostedPluginEffect::usesSidechain` asks the loaded plugin
(`HostedPluginInstance::hasSidechainInput`), which becomes true once
`prepareToPlay` has found an enabled input bus after bus 0. That discovery pass
sits after the early return taken for a BRIDGED plugin, so a plugin running in
its own process never reports a side-chain input and never gets the picker.

---

## User-facing behavior

### Reading these tables

- **Range** and **Default** are the panel's own numbers.
- **Advanced** marks a control hidden until you choose *Show Advanced Controls*
  from the effect window's Menu. Everything else is always visible. Modules
  with no Advanced column have no hidden controls at all.
- Every panel also has the slot's **Vol** knob (-24..+12 dB, default 0) and an
  output meter; most have an input VU meter. Pedal-face panels replace the Vol
  knob with their own Level knob and show no meters.

### Where each module can be loaded

| Group | Modules | Rack | Pedalboard |
|---|---|---|---|
| Rack effects | Compressor, De-esser, Gate, Limiter, Transient Shaper, Overdrive, Saturation, Chorus, Flanger, Phaser, De-reverb, Delay, Reverb | yes | Compressor, Overdrive, Saturation, Chorus, Flanger, Phaser, Delay, Reverb - the eight the board's Change Pedal menu offers. Limiter has a pedal face but no menu entry (see *Pedalboard.md*) |
| Pedal modules | Bass Compressor, Noise Gate, Bass Driver, Bass Overdrive, Blues Drive, Distortion, Fuzz, High-Gain, Octave, Acoustic Simulator, Polyphonic Synth, Wah, Acoustic Preamp | yes (Pedals submenu) | yes |
| Board-only | Tuner, Graphic EQ, Bass Graphic EQ, Pro Parametric EQ, User NAM Pedal | no | yes (fixed slots for the first four) |

---

## Dynamics

### Compressor

Evens out a performance: it turns the loud parts down so the quiet parts can
come up. Four **Modes**, picked from the effect window's Menu. Each mode is a
different panel because each one works differently. A Compressor loaded into a
rack starts in **Modern**; loaded onto a pedalboard it starts in **Pedal**.

**Mode: Modern** - the full-featured version.

| Control | Range | Default | What it does |
|---|---|---|---|
| Thresh | -60..0 dB | -12 | The level above which it starts turning things down |
| Ratio | 0.4..30 | 4 | How hard. Below 1 it expands instead of compresses |
| Gain | -30..+30 dB | 0 | Makeup gain; brings the level back up afterwards. Ignored when Auto MU is on |
| Attack | 0..400 ms | 10 | How quickly it reacts to a loud sound |
| Release | 1..4000 ms | 100 | How quickly it lets go |
| KneeW *(Advanced)* | 0..18 dB | 6 | Manual knee width; overrides the knee type's own smoothing |
| Mix *(Advanced)* | 0..1 | 1 | Blends the compressed sound with the untouched one (parallel compression) |
| LookA *(Advanced)* | 0..5 ms | 0 | Lets it see transients coming. Adds latency, compensated automatically |
| Det *(Advanced)* | 1..100 ms | 10 | Detection window; smaller reacts faster |
| SCHPF *(Advanced)* | 20..2000 Hz | 20 | Stops bass from triggering the compressor. 20 = off |
| Knee type selector | Hard / Medium / Vintage / Soft, each also with `/R` auto-release | Medium | How gradually compression comes in; the `/R` variants add program-dependent auto release |
| Auto MU *(Advanced)* | on/off | off | Works out the makeup gain for you |
| Link *(Advanced)* | on/off | on | One envelope for both channels, so the stereo image does not wander |
| RMS / Peak *(Advanced)* | two-way | RMS | RMS is smoother and more musical; Peak is aggressive and limiter-like |
| GR meter | - | - | Shows how much it is currently turning down |

**Mode: FET (Punchy)** - fast and aggressive; there is no threshold knob.

| Control | Range | Default | What it does |
|---|---|---|---|
| Input | -60..0 | -12 | Drive. Turning it **up** means **more** compression |
| Output | -30..+30 dB | +12 | Level back out |
| Attack | 0..7 (positions) | 4 | 0 = off, 1 = slowest (800 us), 7 = fastest (20 us) |
| Release | 0..7 (positions) | 4 | 0 = off, 1 = slowest (1100 ms), 7 = fastest (50 ms) |
| Ratio selector | 4:1 / 8:1 / 12:1 / 20:1 / All-buttons-in | - | All-buttons-in is the crushed, gritty setting |
| Meter selector | Gain Reduction / Output +8 / Output +4 / Off | - | What the big needle shows |

**Mode: Opto (Smooth)** - two knobs, slow and forgiving; good on vocals.

| Control | Range | Default | What it does |
|---|---|---|---|
| Peak Reduction | 0..100 | 30 | More = more compression |
| Gain | 0..100 | 50 | Output level |
| Comp / Limit toggle | two-way | Comp | Comp is about 3:1; Limit is effectively unlimited ratio |
| Meter selector | Gain Reduction / Output +10 / Output +4 | - | What the needle shows |

**Mode: Pedal (Sustain)** - a pedal-style sustainer.

| Control | Range | Default | What it does |
|---|---|---|---|
| Level | -12..+12 dB | 0 | Output trim |
| Tone | 0..1 | 0.5 | Right boosts highs, left cuts them, center is flat |
| Attack | 1..50 ms | 10 | Also sets release inversely: fast = more sustain, slow = more punch |
| Sustain | 0..1 | 0 | Drives harder into the compressor; more sustain |

### De-esser

Takes the harsh "ess" out of a vocal without dulling the whole take.

| Control | Range | Default | What it does |
|---|---|---|---|
| Detect | 0..100 | 50 | How wide a sound counts as an ess. Narrow catches only S; wide also catches Sh |
| Thresh | -80..0 dB | -12.5 | How loud an ess has to be before it is ducked |
| Range | -48..0 dB | -14 | The most it is allowed to pull down |
| Mode | 0..100 | 50 | 0 ducks the whole sound, 100 ducks only above 4 kHz, in between blends |
| Freq *(Advanced)* | 4000..12000 Hz | 6500 | Direct control of what Detect sets |
| Q *(Advanced)* | 0.5..4 | 1.4 | Higher = narrower detection band |
| Atk *(Advanced)* | 0.1..30 ms | 1 | Attack |
| Rel *(Advanced)* | 10..500 ms | 80 | Release |
| Look *(Advanced)* | 0..max ms | 0 | Lets the detector see the consonant early. Not used by the Spectral engine |
| Mix *(Advanced)* | 0..1 | 1 | Wet/dry blend |
| Monitor toggle | on/off | off | Solo the detected ess - hear only what is being removed |
| Spectral toggle *(Advanced)* | on/off | off | Per-frequency de-essing instead of a broadband duck |
| LowLat toggle *(Advanced)* | on/off | off | Spectral quality: off = high quality (2048 FFT), on = lower latency |
| Stereo / Mid / Side selector *(Advanced)* | three-way | Stereo | Which part of the stereo image is ducked |

### Gate

Silences a channel when nothing is playing, so you do not hear amp hiss or
headphone bleed between phrases.

| Control | Range | Default | What it does |
|---|---|---|---|
| Thresh | -80..0 dB | -80 | Below this the gate closes. -80 = fully open, i.e. doing nothing |
| Range | -80..0 dB | -60 | How far it turns down when closed |
| Attack | 0.1..100 ms | 1 | How fast it opens |
| Hold | 0..500 ms | 50 | How long it stays open after the sound drops |
| Release | 5..2000 ms | 100 | How fast it closes |
| Gate meter | - | - | Shows the gate opening and closing |

### Limiter

A ceiling. Nothing gets out above it. Two **Modes** from the Menu; a fresh
Limiter starts in **Limiter**.

**Mode: Limiter** - the plain, faithful limiter.

| Control | Range | Default | What it does |
|---|---|---|---|
| InGain | -12..+24 dB | 0 | Drive into the limiter |
| Ceil | -24..+12 dB | -0.3 | The output ceiling. +12 means it never limits |
| SatTh | 0..1 | 1 | Soft-saturation threshold; 1 = off |
| SatCv | 0..1 | 0.5 | Saturation knee shape |
| Atk | 0.1..20 ms | 1 | Attack |
| Rel | 10..1000 ms | 100 | Release |
| Sustain | 0..1000 ms | 0 | Holds the gain reduction over an RMS window instead of releasing early. 0 = off |
| SatCv *(Advanced)* | 0..1 | 0.5 | Saturation knee shape |
| SCHPF *(Advanced)* | 20..2000 Hz | 20 | Stops bass pumping the limiter. 20 = off |
| Ahead *(Advanced)* | 0..10 ms | 2 | Look-ahead. Adds latency, compensated automatically |
| RelCv *(Advanced)* | 0..1 | 0.5 | Release curve: 0 linear, 1 exponential |
| Auto *(Advanced)* | on/off | - | Two-stage adaptive release |
| Auto MU *(Advanced)* | on/off | - | Adds back the level you lose by lowering the ceiling |
| Link *(Advanced)* | on/off | on | One envelope for both channels |
| GR meter | - | - | Gain reduction |

The character selector and the loudness suite do not appear in this mode at
all - they belong to Maximizer.

**Mode: Maximizer** - a loudness tool. Its own six controls are what you see
first; every limiter control above moves to Advanced here.

| Control | Range | Default | What it does |
|---|---|---|---|
| Character selector | Clean / Smooth / Tight / Punch / Glue / Loud / Warm / Instant | Clean | Release voicing and color. Loud and Warm add saturation; Instant is for use with Ahead at 0 |
| LUFS | -30..0 LUFS | -14 | Loudness target |
| Target toggle | on/off | off | Turns the loudness servo on: it slowly trims input gain until the output settles at the LUFS target |
| dBTP | -6..0 dBTP | -1 | True-peak target - the margin streaming services want |
| Auto Ceil toggle | on/off | off | Lowers the ceiling until the measured inter-sample peak is under the dBTP target |
| Loudness meter + GR meter | - | - | Short-term LUFS beside output peak, with the target drawn as a dashed line, next to the gain-reduction meter |
| Everything from the Limiter table *(Advanced here)* | - | - | InGain, Ceil, SatTh, Atk, Rel and Sustain are Advanced in this mode; SatCv, SCHPF, Ahead, RelCv and the three ballistics toggles stay Advanced as before |

### Transient Shaper

Makes a hit punchier or softer without changing its volume.

| Control | Range | Default | What it does |
|---|---|---|---|
| Attack | -100..+100 | 0 | Negative softens the hit, positive sharpens it |
| Release | -100..+100 | 0 | Negative shortens the tail, positive extends it |
| Split | 5..1000 Hz | 260 | Where the low and high band are divided |
| Drive | 0..10 | 0 | Soft saturation on the output |
| Gain | -18..+18 dB | 0 | Output gain |
| Wet | 0..1 | 1 | Blends against the untouched sound |
| Attack shape selector | Sharp / Medium / Soft | Medium | Envelope curve for the attack detector |
| Release shape selector | Sharp / Medium / Soft | Medium | Envelope curve for the release detector |
| Sens *(Advanced)* | 0..1 | 0.5 | Detection sensitivity |
| Balance *(Advanced)* | -100..+100 | 0 | Weights the effect toward the low or high band |
| FastRel *(Advanced)* | 1..50 ms | 10 | Release time of the fast (peak) follower |
| SlowAtt *(Advanced)* | 1..50 ms | 10 | Attack time of the slow (RMS) follower |
| Oversampling selector *(Advanced)* | 2x / 4x / 8x / 16x | 4x | Cleaner drive, more CPU |
| Mono Det / Stereo Det *(Advanced)* | two-way | Mono | One shared envelope, or independent per channel |

---

## Harmonics

### Overdrive

Dirt. Two **Modes** from the Menu. Loaded into a rack it starts in **Rack**
mode; loaded onto a pedalboard it starts in **Pedal** mode.

**Mode: Overdrive (Rack)**

| Control | Range | Default | What it does |
|---|---|---|---|
| Drive | 0..10 | 5 | How hard it is pushed |
| Color | 200..8000 Hz | 1000 | Low-pass before the distortion; darker input = smoother dirt |
| Band | 0..1 | 0.5 | How much of the signal goes through that filter. 0 = full bandwidth |
| Filter | 500..18000 Hz | 8000 | Low-pass after the distortion |
| Out | -18..0 dB | 0 | Output. Attenuate only; maximum is unity |
| x100 toggle | on/off | off | Extreme drive mode, hard-limited at the output. Switching is smoothed so it does not click |
| Bias *(Advanced)* | -1..+1 | 0 | Adds even harmonics (warmer). 0 is symmetric |
| Wet *(Advanced)* | 0..1 | 1 | Wet/dry mix |
| Blend / Parallel *(Advanced)* | two-way | Blend | Blend crossfades dry and wet; Parallel keeps the dry at full and adds the wet on top |
| Oversampling selector *(Advanced)* | 2x / 4x / 8x / 16x | 4x | Less aliasing on heavy drive, more CPU |

**Mode: Overdrive (Pedal)** - a three-knob stomp face: Drive (0..10, default 5),
Tone (200..8000 Hz, default 4000), Level (-18..+18 dB, default 0).

### Saturation

Warmth and harmonics rather than outright distortion. Three **Modes**. A fresh
Saturation starts in **Console**, so if you want the tape machine or the full
tube panel you have to switch Mode.

**Mode: Tube**

| Control | Range | Default | What it does |
|---|---|---|---|
| Flowers | 0..10 | 3 | Even-harmonic content |
| Dabs | 0..10 | 3 | Odd/even harmonic balance for the selected tube type |
| Input | -12..+12 dB | 0 | Input sensitivity |
| BassRlf | 0..100 | 30 | Bass relief - at 100 the low end stays clean |
| TonePre | -9..+9 dB | 0 | High shelf before the saturator |
| TonePost | -9..+9 dB | 0 | High shelf after it |
| Wet | 0..100 % | 70 | Wet/dry blend |
| Out | -18..+18 dB | 0 | Output gain |
| Trans toggle | on/off | - | An always-on even-harmonic base layer |
| Tube type selector | Type A / Type B *(Type C is Advanced)* | A | A is bold and gritty, B gentler, C warm even-harmonic foldback. C stays visible if it is already selected |
| Oversampling selector | 2x / 4x / 8x / 16x | 4x | Quality vs CPU |
| Auto MU toggle *(Advanced)* | on/off | - | Automatic makeup gain, with the amount shown beside it |
| Harmonics routing selector *(Advanced)* | Keep Low / Normal / Keep High | Normal | Saturate only the highs, everything, or only the lows |

**Mode: Console** - a four-knob version.

| Control | Range | Default | What it does |
|---|---|---|---|
| Drive | 0..10 | 3 | Preamp drive, gentler than Tube |
| Color | 0..10 | 3 | Transformer-style second-harmonic color |
| Mix *(Advanced)* | 0..100 % | 70 | Wet/dry blend |
| Output | -18..+18 dB | 0 | Output gain |
| Color toggle | on/off | - | Turns the harmonic color off entirely, leaving a clean soft clip |
| Clean / Dirty selector | two-way | Clean | Dirty weights the saturation toward the low end and blooms it |
| Harmonics routing selector *(Advanced)* | Keep Low / Normal / Keep High | Normal | As above |

**Mode: Tape** - this is the tape machine. There is no separate "Tape" entry in
the picker; load Saturation and switch Mode to Tape.

| Control | Range | Default | What it does |
|---|---|---|---|
| Drive | -24..+24 dB | 0 | How hard the tape is hit |
| Hiss | -80..0 dB | -60 | Tape hiss level |
| LoPass | 5000..22000 Hz | 18000 | Cassette high-end rolloff. Full right is off |
| WowDp | 0..1 | 0 | Wow depth - slow motor drift |
| FlutDp | 0..1 | 0 | Flutter depth - fast shimmer |
| IR toggle | on/off | - | Runs the cassette impulse response for real cassette tone color |
| Cassette selector | Cassette 1..10 | 1 | Which cassette (impulse response plus its paired hiss bed) |
| Vibe *(Advanced)* | 0..1 | 0.5 | Shaper asymmetry |
| Hyst *(Advanced)* | 0..2 | 1 | Hysteresis - how much the tape "remembers" |
| Bias *(Advanced)* | 0..10 | 5 | Tape bias. 5 is neutral; extremes add even harmonics |
| WowHz *(Advanced)* | 0.1..5 Hz | 0.5 | Wow rate |
| FlutHz *(Advanced)* | 1..15 Hz | 5 | Flutter rate |
| PreShf *(Advanced)* | -12..+12 dB | 0 | Pre-emphasis shelf at 5 kHz |
| DeShf *(Advanced)* | -12..+12 dB | 0 | De-emphasis shelf at 4 kHz |
| Tape speed selector *(Advanced)* | 7.5 / 15 / 30 ips | 15 | Slower is squishier, faster is cleaner |
| Oversampling selector *(Advanced)* | 2x / 4x / 8x / 16x | 4x | Quality vs CPU |

---

## Modulation

### Chorus

Thickens a sound by layering slightly detuned copies of it.

| Control | Range | Default | What it does |
|---|---|---|---|
| LFO1 / LFO2 / LFO3 | 0.01..10 Hz each | 0.5 / 0.8 / 1.2 | The three wobble rates |
| LFO waveform selectors (one per LFO) | Sine / Triangle / Multi / Organic | Sine | Multi is the classic digital chorus blend; Organic drifts like hardware |
| Delay | 0.5..30 ms | 8 | Base delay of the copies |
| Depth | 0..20 ms | 8 | How far the wobble swings |
| Stereo | 0..360 deg | 120 | Left/right LFO phase offset - the width |
| CrossHz | 20..10000 Hz | 800 | Crossover between the chorused band and the dry band |
| HP band / LP band toggle | two-way | HP band | Whether the highs or the lows get chorused |
| Wet Only toggle | on/off | off | Output is chorus only, for effect sends |
| Wet *(Advanced)* | 0..1 | 0.5 | Continuous wet level |
| 3 voices / 6 voices *(Advanced)* | two-way | 3 | 6 is thicker and more diffuse |

### Flanger

A jet-plane swoosh. All controls always visible.

| Control | Range | Default | What it does |
|---|---|---|---|
| Rate | 0.05..5 Hz | 0.5 | Sweep speed |
| Depth | 0..10 ms | 5 | Sweep depth |
| Delay | 0..20 ms | 0 | Base delay |
| Feed | 0..100 % | 50 | Feedback; more is more resonant |
| Phase | 0..360 deg | 0 | Right-channel LFO offset |
| Shape | 0..1 | 0 | Morphs the LFO from sine to triangle |
| Damp | 0..1 | 0 | Damps the feedback path; 0 bright, 1 warm |
| Wet | 0..1 | 0.5 | Wet/dry blend |
| Cross | -96..0 dB | -96 | Cross-channel wet mix; raise for a spinning stereo width. -96 = off |
| BPM toggle + Sync division | on/off, 1/1 to 1/8T | off, 1/8 | Locks the sweep to the song tempo |
| InvFB toggle | on/off | off | Inverts the feedback - through-zero flanging |
| InvW toggle | on/off | off | Inverts the wet signal |

### Phaser

Sweeping notches; swirlier and less metallic than a flanger.

| Control | Range | Default | What it does |
|---|---|---|---|
| Rate | 0.05 Hz up to the Range limit | 0.5 | Sweep speed |
| MinHz | 20..2000 Hz | 200 | Bottom of the sweep |
| MaxHz | 200..8000 Hz | 2000 | Top of the sweep |
| Feed | 0..1.2 | 0.5 | Feedback / resonance |
| Stereo | 0..360 deg | 0 | Left/right LFO offset |
| Wet | 0..1 | 0.5 | Wet level |
| Gain | -18..+18 dB | 0 | Output gain |
| Slow / Fast range toggle | two-way | Slow | Slow caps Rate at 2 Hz; Fast opens it to 10 Hz. Changing it rescales the Rate knob |
| Stages selector | 1 / 2 / 4 / 6 / 8 / 12 / 16 / 24 | - | More stages = denser, more vowel-like |
| Cross *(Advanced)* | 0..1 | 0 | Cross-channel feedback; 1 fully swaps |
| LFO wave selector *(Advanced)* | Sine / Triangle / Saw / Sample-and-hold | Sine | Saw snaps back for a jet effect; S&H is stepped and glitchy |
| BPM toggle + Sync division *(Advanced)* | on/off, 1/1 to 1/8T | off, 1/4 | Locks the sweep to tempo |
| InvFB toggle *(Advanced)* | on/off | off | Flips the resonance character |

---

## Time

### Delay

Echoes. Two **Modes**; a fresh Delay starts in **Echo**.

**Mode: Echo** - the full delay.

| Control | Range | Default | What it does |
|---|---|---|---|
| Time | 1..2000 ms | 250 | Gap between repeats. Grayed out while BPM sync is on |
| Feed | 0..1.2 | 0.4 | How many repeats. Above 1 it builds up rather than dying away |
| Wet | 0..1 | 0.3 | Echo level out |
| Dry | 0..1 | 1 | Original level out |
| Tone | -1..+1 | 0 | Tone on the echoes: left low-pass, right high-pass, center off |
| FBDst | 0..10 | 1 | Drive inside the feedback loop - repeats get dirtier |
| FBKnee | 0..1 | 0.5 | Curve of that drive |
| FBSym | 0..1 | 0 | Its symmetry |
| Smooth | 0..1 | 0.5 | Smooths the change when you move Time. Only audible with the Pitch switch on |
| Model selector | Mono / Stereo / PingPong / Off | - | Off gives no echoes at all - the filter, lo-fi and distortion chain runs on the input instantly |
| FB Filter selector | LP / HP / BP / Off | - | Filter inside the feedback loop |
| BPM toggle + Sync division | on/off, 1/1 to 1/8T | off, 1/4 | Locks the delay time to the song tempo |
| Pitch toggle | on/off | off | Keeps the pitch when you move the delay time (crossfade instead of tape-style glide) |
| Limit / Sat selector | two-way | Limit | Whether the feedback drive limits peaks or saturates them |
| LoFiSR *(Advanced)* | 100..48000 Hz | 48000 | Lo-fi sample rate; below about 6000 it is obviously gritty |
| WetIn *(Advanced)* | 0..1 | 1 | Level going *into* the delay line. Drop it to freeze the current repeats |
| FBCut *(Advanced)* | 20..18000 Hz | 80 | Feedback filter cutoff |
| FBReso *(Advanced)* | 0.1..20 | 0.707 | Feedback filter resonance |
| ModHz *(Advanced)* | 0..20 Hz | 0 | Modulation rate. Must be above 0 for ModTime / ModFB to do anything |
| ModTime *(Advanced)* | 0..1 | 0 | Modulation depth on the delay time - chorus/flange character |
| ModFB *(Advanced)* | 0..1 | 0 | Modulation depth on the feedback filter cutoff |
| Diff *(Advanced)* | 0..1 | 0 | Diffusion - smears the repeats |
| DiffSprd *(Advanced)* | 0..1 | 0.5 | How wide that smear is |
| LoBit *(Advanced)* | 1..24 | 24 | Bit depth; 24 = off |
| Spread *(Advanced)* | 0..1 | 0 | Left/right delay difference |
| Pan *(Advanced)* | -1..+1 | 0 | Pans the offset tap |
| Duck / DkThr / DkAtt / DkRel *(Advanced)* | 0..100 %, -60..0 dB, 1..200 ms, 10..1000 ms | 0, -24, 10, 200 | Ducks the echoes out of the way when the trigger (the sidechain source, or the dry input) gets loud. Duck at 0 is off; around 60 % is a good vocal setting |

**Mode: Vocal Doubler** - a minimal two-tap doubler.

| Control | Range | Default | What it does |
|---|---|---|---|
| Time L | 5..50 ms | 13 | Left-side offset |
| Time R | 5..50 ms | 22 | Right-side offset |
| Detune | 0..100 | 50 | How far the copies drift in pitch |
| Width | 0..100 | 80 | Stereo spread |
| Rate | 0.1..2 Hz | 0.7 | Drift speed |
| Mix | 0..100 % | 50 | Blend |
| Duck / DkThr / DkAtt / DkRel *(Advanced)* | as above | 0, -24, 10, 200 | Ducks the doubled signal under the lead |

### Reverb

Space. The **Mode** menu picks the algorithm: Plate, Hall, Chamber, Room or
VocalBooth. A fresh Reverb starts on **Hall**.

| Control | Range | Default | What it does |
|---|---|---|---|
| Room | 0..2 | 0.6 | Size of the space |
| Decay | 0.1..20 s | 2 | How long the tail lasts |
| Diffuse | 0..1 | 0.5 | 0 gives distinct echoes, 1 a dense wash |
| PreDly | 0..200 ms | 10 | Gap before the reverb starts - keeps the dry sound clear |
| Wet | 0..1 | 0.3 | Reverb level |
| Dry | 0..1 | 0.7 | Original level |
| LoCut | 20..800 Hz | 80 | High-pass into the reverb - keeps bass out of the tail |
| HiCut | 1000..20000 Hz | 18000 | Low-pass into the reverb |
| BassMlt | 0.5..3 | 1.2 | Makes the low end ring longer or shorter than the rest |
| BassX | 20..800 Hz | 250 | Where that split happens |
| TailDep | 0..1.5 ms | 0.3 | Tail modulation depth - stops the tail sounding static |
| TailRt | 0.05..2 Hz | 0.35 | Tail modulation rate |
| Stereo | 0..200 % | 100 | Stereo separation |
| HiDamp | 500..20000 Hz | 8000 | How fast the highs die away |
| ER | -60..+12 dB | -6 | Early-reflection level |
| Stereo-processing selector (on the panel, not the Mode menu) | Mid / Side *(Stereo is Advanced)* | Mid | Which part of the stereo image is reverberated. Stereo stays visible if it is already the active choice |
| Sync toggle + Sync division | on/off, 1/1 to 1/8T | off, 1/4 | Locks pre-delay to the song tempo |
| HFRatio *(Advanced)* | 0.3..2 | 0.7 | Below 1 the highs die faster than the lows |
| WetTone *(Advanced)* | -12..+12 dB | 0 | Tilts the reverb warm or bright |
| Freeze toggle *(Advanced)* | on/off | off | Holds the current tail forever. Right-click to automate |
| Tail shape selector *(Advanced)* | Sine / Triangle / Random | - | Shape of the tail modulation |
| HiDmp bypass toggle *(Advanced)* | on/off | off | Turns high-frequency damping off entirely |
| Duck / DkThr / DkAtt / DkRel *(Advanced)* | 0..100 %, -60..0 dB, 1..200 ms, 10..1000 ms | 0, -24, 10, 200 | Ducks the reverb under the trigger signal |

### De-reverb

Removes room sound from a recording made in a live space.

| Control | Range | Default | What it does |
|---|---|---|---|
| Reduce | 0..100 % | 50 | How hard the room tail is suppressed |
| Tail | 100..1000 ms | 400 | The tail length being modeled. Match the room - bigger room, longer |
| Mix | 0..100 % | 100 | Dry/wet blend |
| GR meter | - | - | How much is being removed |

---

## Pedal modules

These are the guitar and bass pedals. They also load into a rack slot from the
picker's **Pedals** submenu. Each has its own Level knob instead of the rack's
Vol knob, and none has an Advanced view.

| Module | Controls (range, default) |
|---|---|
| **Blues Drive** | Drive 0..1 (0.5) - clean to heavy overdrive; Tone 0..1 (0.5) - sweeps a 500 Hz..5 kHz low-pass; Level -24..+12 dB (0) |
| **Distortion** | Dist 0..1 (0.5) - drives a hard clipper toward a square wave; Tone 0..1 (0.5) - tilt EQ, left darker, right brighter, center scooped; Level -24..+12 dB (0) |
| **Fuzz** | Fuzz 0..1 (0.5); Boost 0..20 dB (0) - pushes the clipping harder without moving Fuzz; Level -24..+12 dB (0); Type selector Gated / Germanium / Octave |
| **High-Gain** | Dist 0..1 (0.5); Low -15..+15 dB (0) shelf at 80 Hz; Mid Hz 200..5000 (800); Mid dB -15..+15 (0); High -15..+15 dB (0) shelf at 5 kHz; Level -24..+12 dB (0) |
| **Bass Driver** | Drive 0..1 (0.5) - drives the mid and high bands only, the lows stay clean; Blend 0..1 (0.7); Low 0..1 (0.7); High 0..1 (0.7); Level -24..+12 dB (0) |
| **Bass Overdrive** | Gain 0..1 (0.5); Balance 0..1 (0.5) - clean to fully clipped; Low -15..+15 dB (0) shelf at 100 Hz; High -15..+15 dB (0) shelf at 3 kHz; Level -24..+12 dB (0) |
| **Octave** | Direct 0..1 (1.0) - dry level; +1 0..1 (0); -1 0..1 (0.5); -2 0..1 (0); Range 0..1 (0.6) - input low-pass 300 Hz..3 kHz, lower tracks bass better; Mode selector Polyphonic / Vintage |
| **Noise Gate** | Threshold -60..0 dB (-40); Decay 1..1000 ms (100); Mode selector Reduction (down to -20 dB, some bleed) / Mute (-60 dB floor); Detect selector DI Sidechain / Self. This is the one pedal that reads a sidechain |
| **Bass Compressor** | Thresh -48..0 dB (-24); Ratio 1..10 (4) applied to all three bands; Release 50..500 ms (200); Level -24..+12 dB (0) |
| **Polyphonic Synth** | Var 1..11 (1) - Lead 1, Lead 2, Pad, Bass, Str, Organ, Bell, SFX 1, SFX 2, Seq 1, Seq 2; Tone 0..1 (0.6); Rate 0..1 (0.3); Depth 0..1 (0.4); Effect 0..1 (0.5) - synth level; Direct 0..1 (1.0) - dry level; Mono / Poly toggle; Guitar / Bass range toggle |
| **Wah** | Pedal 0..1 (0.5) - heel-down dark to toe-down bright. Right-click the knob to bind an expression pedal via MIDI Learn; Mode selector Vintage / Rich (Rich keeps the bass fundamental under the sweep) |
| **Acoustic Preamp** | Resonance 0..1 (0.5) - adaptive body resonance depth, or the impulse-response mix in User mode; Ambience 0..1 (0.2); Notch 45..1000 Hz (45) - bottom position is OFF; raise it until feedback rumble disappears; Level -24..+12 dB (0); Body selector Dreadnought / Parlor / Jumbo / User (loads your own impulse response) |
| **Acoustic Simulator** | Top -15..+15 dB (0) - pick attack; Body -15..+15 dB (0) - body depth, or the impulse-response mix in User mode; Reverb 0..1 (0.2); Level -24..+12 dB (0); Mode selector Standard / Jumbo / Enhanced / Piezo / User |

---

## Board-only modules

These live on the pedalboard's fixed slots and are not offered in the rack
picker. See *Pedalboard.md* for where they sit.

| Module | Controls |
|---|---|
| **Tuner** (board slot 1, locked) | Mode selector Chromatic / Guitar / Bass; 440 / 432 Hz reference toggle with a Trim knob whose range follows it (436-445 Hz or 428-437 Hz); Flat selector 0-6 semitones for drop tunings; Mute switch - silences the output while detection keeps running; Display selector Strobe / LED Bar |
| **Graphic EQ** (board slot 8, option 1) | Seven fixed sliders at 100 / 200 / 400 / 800 / 1600 / 3200 / 6400 Hz, each -15..+15 dB, plus a Level slider -15..+15 dB. The 6.4 kHz band is a high shelf, the rest are peaks |
| **Bass Graphic EQ** (board slot 8, option 2) | Seven fixed sliders at 50 / 120 / 400 / 500 / 800 / 4500 / 10000 Hz, each -15..+15 dB, plus a Level slider -15..+15 dB |
| **Pro Parametric EQ** (board slot 8, option 3) | Master Input Volume 0..+26 dB with a soft clipper and an overload light; a Hi/Lo output range switch (+20 dB in Hi); an EQ Bypass switch that bypasses the three bands but *not* the preamp; and three fully parametric bands - Low 25-500 Hz (default 80), Mid 150-2500 Hz (default 1000), High 600-10000 Hz (default 5000), each with Boost -60..+20 dB (-60 acts as full cut) and Bandwidth 0.2-3.8 |
| **User NAM Pedal** (any free board slot) | Loads a `.nam` amp capture file; you are asked for one as soon as you add it, and the tile is named after the loaded capture. Input/Drive -24..+24 dB (0) - pushes a hotter signal into the model to fake more drive; Low -15..+15 dB (0) shelf at 100 Hz; Mid -15..+15 dB (0) peak at 1 kHz; High -15..+15 dB (0) shelf at 5 kHz; Blend 0..1 (1.0) - dry/wet; Output -24..+12 dB (0) |

---

## Parameters and persistence

**Module state** is a `juce::ValueTree` tagged with the class name, one
property per user parameter, serialized through
`getStateInformation`/`setStateInformation`. The tag is checked on the way back
in and a mismatch is ignored rather than half-applied. Every serializer ends by
snapping its smoothers to the restored values and clearing filter and delay
state, so a preset load neither glides nor pops.

The three modules that reference a file on disk - Acoustic Preamp (user
impulse response), Acoustic Simulator (user impulse response) and User NAM
Pedal (`.nam` capture) - persist the path and report through
`MissingFileReport` when the file has gone, rather than failing silently.

**Where module state is stored** depends on the host:

| Host | Storage |
|---|---|
| Rack slot | Base64 inside the slot's `<Slot data>` in the project - see *Effect Racks.md* |
| Pedalboard slot | Base64 inside the board's `<Slot data>` - see *Pedalboard.md* |
| Vox locked chain | The chain rack's own blob, saved with the Vox tab |

**Presets** live per effect type under
`Documents\BaySickDAW\Presets\Effects\<Type>\` with `Factory\`, `My Presets\`
and an optional `Default.xml`. Pedal modules nest under
`Presets\Effects\Pedals\<Type>\`. A preset file is an `<EffectPreset>` wrapper
with the module's own state blob base64 inside it, so a mode change stored in
the preset round-trips faithfully - which is why loading a preset rebuilds the
panel. Factory presets are generated on first launch from a table of
configure-lambdas and re-seeded if deleted; `My Presets` is never touched by
seeding.

**Automation.** A module exposes lanes only for the controls that appear in
`EffectParamMap`'s table for its (type, mode) pair, plus any toggle registered
through `addAutomatableToggle` (today: Reverb's Freeze). The lane suffix is the
control's on-screen label lowercased. Selectors (mode/character/knee/waveform
chicken-heads) are deliberately not automatable - they change which controls
exist, or are pure voicing. Tuner and the three EQ pedals have no lanes at all;
their controls are plain sliders that are never stamped.

Because a suffix means different things in different modes, the automation key
is (type, variant) - "attack" on a Modern compressor is milliseconds, on an FET
compressor it is a switch position 0-7, and on a Pedal-mode compressor it is
milliseconds on a different range.

**Not saved anywhere:** meter readings, gain-reduction values, tuner detection,
the overload light on the Pro Parametric EQ, and every visual feed.

---

## Lifetime and teardown

A module is owned by the slot it sits in - an `EffectRack::Slot` or a
`BaySickPedalsProcessor::Slot`. It is created by `EffectRack::createEffect`,
prepared on the message thread outside all locks, and destroyed on the message
thread outside all locks. A module never outlives its slot, and a slot's module
is replaced whole rather than reconfigured, except when only a mode changes.

Panels are separate objects with their own lifetime: a panel can be destroyed
(window closed) while the module keeps processing, and a panel must re-ask the
rack for its DSP pointer on every tick (`EditorPanelBase::liveDsp()`) because a
project load replaces the module underneath it.

Ordering that matters: a control that changes a module's reported latency
(look-ahead on Compressor, Limiter and De-esser; the EQ's oversampling and
phase modes) has to poke a delay-compensation refresh. Panels do that through
`onLatencyChanged`; automation lanes carry the same poke on the applicator, so
an automated look-ahead cannot leave compensation stale.

---

## Cross-references

- **Effect Racks.md** - the six-slot rack, its windows, presets and automation ids.
- **Pedalboard.md** - the 8-slot pedal chain and its fixed Tuner and EQ slots.
- **EQ.md** - the 8-band parametric EQ, which is not a rack module.

---

## Differs from Carry-Forward

Not applicable - the Carry-Forward Reference snapshot does not describe the
individual effect modules or their controls.
