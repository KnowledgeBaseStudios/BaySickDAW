# BaySickBass

**Purpose** - BaySickBass is the app's bass synthesizer: it builds a note from a raw
waveform and then shapes it with a filter and envelopes. It is the same instrument as
BaySickSynth - identical DSP, identical control set - shipped with different starting
values so that the moment you load it, it is already monophonic, already gliding a little
between notes, and already filtered down into bass territory. It is one of the players
you can load on a Bass tab.

---

## How it operates

BaySickBass is a thin wrapper. `Source/BaySickBass/BaySickBassProcessor.h` / `.cpp` owns
its own APVTS with the `bsb_` parameter tag and drives **`BaySickSynthDSP`** - the exact
same voice manager and voice classes BaySickSynth uses, from
`Source/BaySickSynth/`. `BaySickBassProcessor::updateFromApvts` is line-for-line the same
logic as its BaySickSynth twin; only the parameter prefix and the factory defaults differ.

| Layer | Files | Job |
|---|---|---|
| Processor | `Source/BaySickBass/BaySickBassProcessor.h` / `.cpp` | Owns the APVTS, reads it once per block, pushes changed values into the DSP, injects audition notes, applies the master out gain, scrubs NaN/Inf. |
| Voice manager | `Source/BaySickSynth/BaySickSynthDSP.h` / `.cpp` | 16 voices in a `BroadcastSynthesiser`; rewrites MIDI for Mono / Legato and Cut Self. |
| Voice | `Source/BaySickSynth/BaySickSynthVoice.h` / `.cpp` | All per-note DSP. |
| Editor | `Source/BaySickBass/BaySickBassEditor.h` / `.cpp` | The panel. A parallel copy of `BaySickSynthEditor` with the bass color set (`BaySickBassLAF.h`, accent `#33FF88`). |
| Display | `Source/BaySickBass/BaySickBassVisualizerScreen.h` | A one-line subclass of `BaySickVisualizerScreen` that swaps in the bass green. |

Shared helper components come from `Source/BaySickSynth/BssEditorComponents.h` -
`BssLedRadio` (the button rows) and `BssFilterXYPad` (the filter pad).

**Per-voice signal path** (in order): glide, LFO, pitch (note + Transpose + pitch wheel +
LFO + pitch envelope + drift), filter cutoff (base times keyboard tracking, velocity
tracking, filter envelope, LFO, mod wheel, per-note CC74), oscillator, noise, filter, amp
envelope times velocity, burst multiplier, declick, transient click, unison stack,
per-note pan, stereo out. Then the processor applies **Out Vol**, applies a fixed -12 dB
calibration trim (`kOutputHeadroom` in `BaySickBassProcessor.cpp`), and replaces any
non-finite sample with zero.

**Threading.** `processBlock` is on the audio thread and the ~50 parameter reads are gated
by an `ApvtsDirtyTracker`, so they only run on blocks where something changed (a host
tempo change counts, so tempo-synced LFO keeps tracking). The editor's LED rows, XY pad
and visualizer set an atomic flag from the parameter callback and repaint on a 30 Hz
timer, because that callback can arrive on an offline render thread.

**Note auditioning.** Piano-roll key clicks reach the engine through `auditionNote` /
`auditionNoteOn` / `auditionNoteOff`, plain atomics drained at the top of `processBlock`.
Note-offs accumulate in a 128-bit mask so a fast drag cannot drop one.

**Sample-rate note (verified in code).** The per-voice filter (`SynthFilter`) and LFO
(`LFO`) never receive the device sample rate - the voice calls `prepare()` on its
oscillators but not on those two - so both keep their built-in 44100 Hz assumption. At a
48 kHz device the real filter cutoff and the real LFO speed run about 9 percent above the
numbers on screen. Everything else in the voice reads the live rate and is correct.

---

## User-facing behavior

### Where it lives

Pick BaySickBass when you create the tab, from the ribbon's add menu
(`+ Add BaySickBass`, on the Bass row). **The choice locks** - a tab keeps the player it
was created with, so make a new tab if you want a different one.

The panel opens in its own window; the window's title strip shows the engine name in
neon green and carries the **Preset** button. The window will not shrink below
558 x 455 pixels.

The layout is a 120-pixel animated display across the top, then six tabs:

| Tab | What it holds |
|---|---|
| OSC | Waveform choice, tuning, voice mode, master level, mod wheel |
| OSC ENV | The volume envelope and the pitch envelope |
| FILTER | The filter XY pad, filter type, keyboard and velocity tracking |
| FLT ENV | The filter's own envelope, and how strongly it moves the filter |
| LFO | The wobble generator - shape, speed, target, depth |
| MOD | Noise, transient click, burst, drift, unison |

The display follows the tab: the oscillator waveform, the envelope shape, the filter's
frequency response, or a scrolling LFO wave running at the real LFO speed. On the MOD tab
it keeps showing the LFO.

Every knob shows its value in a pop-up while you drag it. **Double-click any knob to
return it to its factory default.** Right-click for the app-wide Automate and
Type-in-value menu.

### What is different out of the box

These are the only differences from BaySickSynth. Everything else - every control, every
range - is identical.

| Control | BaySickBass default | BaySickSynth default | Why it matters |
|---|---|---|---|
| Voice mode | **Mono** | Poly | One note at a time, like a real bass. Play a chord and you hear the last note only. |
| SLIDE | **0.08 s** | 0 | A short glide between notes is already dialed in - overlapping notes bend into each other. |
| Amp ATTACK | **0.005 s** | 0.01 s | Faster - the note starts immediately. |
| Amp DECAY | **0.15 s** | 0.10 s | Slightly longer fall into the sustain. |
| Amp SUSTAIN | **0.6** | 0.8 | A little more of a plucked shape. |
| Amp RELEASE | **0.20 s** | 0.30 s | Tighter - the note stops sooner after you let go. |
| Filter CUTOFF | **4000 Hz** | 20000 Hz (open) | Already rolled down, so the sound is warm rather than buzzy. |
| Filter RES | **0.2** | 0 | A touch of resonance so filter movement is audible. |
| Filter env AMOUNT | **0.3** | 0 | The filter envelope is already doing something - each note opens up and closes again. |
| Filter ATTACK | **0.005 s** | 0.01 s | The filter opens immediately. |
| Filter DECAY | **0.20 s** | 0.10 s | The classic bass "wow" as the filter closes back down. |
| Filter SUSTAIN | **0.3** | 0.5 | Closes further while the key is held. |
| Filter RELEASE | **0.20 s** | 0.30 s | Tighter tail. |

The combination of Mono + a short SLIDE + a filter envelope with 0.2 s decay is what makes
BaySickBass sound like a bass on the first note you play.

### OSC tab

**WAVEFORM** (drop-down, default SAW). The raw sound before anything else touches it.

| Setting | What you hear | What MODIFIER does here |
|---|---|---|
| SAW | Bright and buzzy, full of harmonics. The default and the workhorse bass tone. | Nothing |
| SAW+SAW | Two saws detuned against each other - thick, moving, wide. | Detune amount |
| PULSE | Hollow and reedy. Narrow settings are thin and nasal; centered is a square. | Pulse width, 5 to 95 percent |
| SAW+SQUARE | A saw and a square together - buzz plus hollow. | Detune amount |
| SQUARE+SQUARE | Two squares - woody and hollow, a very good synth-bass tone. | Detune amount |
| SUPERSAW | Seven saws in one oscillator, already detuned. Huge, but usually too wide for a low bass. | Spread of the seven, 0 to half a semitone |
| BELL | Two sines where one bends the other (FM). Metallic and glassy. | Amount of bend |
| DEAF SAW | A saw run through a soft low-pass inside the oscillator. Dull and round. | Brightness, roughly 100 Hz to 3 kHz |
| SPREAD OCT | The note plus an octave up and an octave down. Big and organ-like. | How much of the octaves you hear |
| SPREAD 5TH | The note plus a fifth above and a fifth below. Power-chord flavor. | How much of the fifths you hear |
| SINE | One pure tone, no harmonics. The purest sub bass there is. | Nothing |

**DUAL-OSC TUNING** (drop-down, default Musical). Only bites on SAW+SAW, SAW+SQUARE and
SQUARE+SQUARE - it changes what MODIFIER means for the second oscillator. **Musical** is
a musical detune up to 50 cents; **Hz Offset** adds a fixed 0 - 2000 Hz offset;
**Absolute Hz** pins the second oscillator to one frequency between 20 Hz and 20 kHz so it
no longer follows the keyboard. The MODIFIER read-out changes to match.

**SYNC** (toggle, default off). Hard sync - the second oscillator restarts every time the
first completes a cycle. Dual-oscillator waveforms only. Adds a hard edge; sweep the
second oscillator with MODIFIER in one of the Hz modes for a tearing sweep.

**RING** (toggle, default off). Ring modulation - the two oscillators are multiplied
instead of added. Metallic and often atonal. Dual-oscillator waveforms only. Stacks with
SYNC.

**TRANSPOSE** (knob, -24 to +24 semitones, default 0). Shifts the whole instrument.
-12 is an octave down.

**MODIFIER** (knob, 0 to 1, default 0.5). Meaning depends on the waveform - see the table
above.

**NOISE** (knob, 0 to 1, default 0). Mixes hiss on top of the oscillator. Small amounts
add grit and attack noise to a bass.

**VOICE MODE** (three buttons: Poly / Mono / Legato, default **Mono**).

| Mode | Behavior |
|---|---|
| Poly | Full chords, up to 16 notes at once. |
| Mono | One note at a time. Each new note cuts the previous one and restarts the envelopes. This is the default. |
| Legato | One note at a time, but overlapping notes **slide** into each other without restarting the envelopes. With the default SLIDE of 0.08 s this is the classic sliding bass line. |

**CUT SELF** (toggle, default off) and its mode button (**SAME PITCH** / **CUT ALL**,
default Same Pitch). Poly mode only - Mono and Legato already cut. Same Pitch cuts
a still-ringing copy of the same note on retrigger; Cut All cuts every ringing voice on
each new note. The cut is an instant, click-free fade, not a note-off, so there is no
tail.

**SLIDE** (knob, 0 to 2 seconds, default **0.08 s**). Glide time. A new note bends into
place from the previous note instead of jumping - but only while the previous note is
still held.

**OUT VOL** (knob, 0 to 1, default 0.8). The engine's output level before the mixer strip.
On top of this knob the output stage applies a fixed -12 dB calibration trim that is not
exposed as a control. The oscillators run at full scale, so the trim is what leaves room
for more than one note at a time. BaySickBass is deliberately quiet on its own - it is
meant to be brought up by what comes after it rather than by pinning OUT VOL to the top.

**MOD WHEEL** group. Two buttons pick the destination - **Filter** (default) or **LFO** -
and **AMOUNT** (knob, 0 to 1, default 0) sets the depth. Routed to Filter, a fully raised
wheel opens the cutoff by up to 2 octaves. Routed to LFO, the wheel adds LFO depth on top
of the LFO tab's AMOUNT. At AMOUNT 0 the wheel does nothing.

### OSC ENV tab

**AMP ENV** - four vertical faders shaping the note's volume.

| Control | Range | Default | What it does |
|---|---|---|---|
| ATTACK | 0.001 - 10 s | 0.005 s | Time to reach full volume. Very short here, so notes start instantly. |
| DECAY | 0.001 - 10 s | 0.15 s | Time to fall from full volume to the sustain level. |
| SUSTAIN | 0 - 1 | 0.6 | Level held while the key is down. |
| RELEASE | 0.001 - 10 s | 0.20 s | Fade time after you let go. |
| VEL (knob) | 0 - 1 | 1.0 | How much playing harder makes it louder. 0 makes every note the same volume. |

**PITCH ENV** - the same four stages applied to pitch instead of volume (defaults
0.01 s / 0.10 s / 0 / 0.30 s). **AMOUNT** (-24 to +24 semitones, bipolar, default 0) sets
how far and which way. A large negative amount with a short decay gives the pitch drop
that turns a SINE note into a kick drum.

### FILTER tab

**The XY pad** (left half). Click or drag. Left-to-right is **CUTOFF** (20 Hz - 20 kHz,
logarithmic, default **4000 Hz**); bottom-to-top is **RES** - resonance (0 - 1, default
**0.2**). Cutoff decides how much of the sound gets through; resonance boosts a narrow
band right at the cutoff, which is what makes a bass filter sweep growl.

**TYPE** (four buttons: LP / HP / BP / Notch, default LP). LP keeps everything below the
cutoff (warm, the normal bass choice); HP keeps everything above it (thin - it takes the
bottom out); BP keeps only a band (nasal); Notch removes a band (hollow).

**TRACKING** - two knobs, both 0 to 1, both defaulting to 0:

- **KEYBOARD** - how much the cutoff follows the note played. At 1 the cutoff moves an
  octave per octave, so high notes stay as bright as low ones. At 0 the cutoff is fixed
  and high notes get progressively duller.
- **VELOCITY** - how much playing harder opens the filter, up to about 2 octaves.

### FLT ENV tab

The four faders are ATTACK / DECAY / SUSTAIN / RELEASE (defaults 0.005 s / 0.20 s /
0.3 / 0.20 s) for an envelope that moves the **filter cutoff**, not the volume.

**AMOUNT** (knob, -1 to +1, default **0.3**). At +1 the envelope sweeps the cutoff up to
4 octaves open; at -1 it sweeps 4 octaves closed; at 0 the filter envelope does nothing.
The 0.3 default is what gives each bass note its opening "wow" at the start.

### LFO tab

- **SHAPE** (Sine / Saw / Square, default Sine). Sine is smooth; Saw ramps and snaps back;
  Square jumps between two values.
- **RATE** (knob, 0.01 - 30 Hz, default 1 Hz). Grayed out while SYNC is on.
- **SYNC** (toggle, default off) plus the division drop-down (1/1, 1/2, 1/4, 1/8, 1/16,
  1/32, default 1/4). With SYNC on the LFO locks to the song tempo. The drop-down is
  grayed out while SYNC is off.
- **DEST** (Filter / Pitch / Osc Mod, default Filter). Filter wobbles the cutoff (up to
  plus or minus 2 octaves at full depth) - a wobble bass; Pitch wobbles the tuning (up to
  plus or minus 4 semitones) for vibrato; Osc Mod wobbles the MODIFIER value, which moves
  the pulse width or the detune.
- **AMOUNT** (knob, 0 - 1, default 0). Depth. At 0 the LFO does nothing, so raise this
  first.

### MOD tab

**NOISE** - **NOISE ONLY** (toggle, default off) mutes the oscillator and makes noise the
whole sound; the OSC tab's NOISE knob then acts as its level, and if that knob is at zero,
noise-only mode plays at full level rather than silence. The **Color** drop-down
(White / Pink / Brown, default White) sets the character: White is flat and hissy, Pink
rolls off 3 dB per octave and is warmer, Brown rolls off 6 dB per octave and is deep and
rumbling.

**TRANSIENT** - a short burst of filtered noise at the start of each note, added after the
filter and envelope so it always cuts through. **AMT** (0 - 1, default 0) is its level and
switches the feature off at 0; **DUR** (0 - 20 ms, default 5) is its length; **COLOUR**
(200 Hz - 10 kHz, default 5000) is its brightness - low is a thump, high is a tick. On a
bass this is how you add finger or pick noise.

**BURST ENV** - fires several quick volume pulses at the start of the note on top of the
normal envelope. **BURST** (toggle, default off), **COUNT** (1 - 8, default 4),
**SPACING** (1 - 100 ms, default 20).

**DRIFT** (knob, 0 - 1, default 0). Slow random pitch wander, independent per voice, up to
about a quarter tone at maximum. 0 is perfect digital tuning; small amounts add analog
looseness.

**UNISON** - **VOICES** (1 - 7, default 1; 1 means off), **DETUNE** (0 - 1, default 0.2,
up to plus or minus 50 cents), **SPREAD** (0 - 1, default 0.8, how far apart they are
panned). The extra unison copies are always sawtooth waves regardless of the WAVEFORM
setting. On a low bass, unison spread can make the low end wander in stereo - keep SPREAD
low if the bass needs to stay centered.

### Presets

The **Preset** button on the window's title strip opens the preset menu. Factory presets
live in `Documents\BaySickDAW\Presets\BaySickBass\`, with each folder becoming a submenu.
**Save preset...** writes to `Presets\BaySickBass\My Presets\`.

Loading a preset renames the tab, the mixer strip and the piano-roll label to the preset's
filename. Presets are portable between tabs - the loader rewrites the saved parameter
prefix (it looks for the `_bsb_` tag) to the destination tab's prefix.

### Per-note expression from the piano roll

| Controller | Effect |
|---|---|
| CC1 (mod wheel) | Whatever MOD WHEEL is routed to |
| CC10 | Per-note pan; a sounding note glides to the new position over about 8 ms |
| CC71 | Per-note resonance offset, plus or minus half the resonance range |
| CC72 | Per-note release scale, from a quarter to four times RELEASE |
| CC74 | Per-note brightness, plus or minus 2 octaves of filter cutoff |
| CC84 (+ CC5 / CC37) | Slide: the note starts at another pitch and glides in, with an optional glide time in milliseconds |
| CC85 / CC86 / CC89 | Slide takeover of a sounding note, with loudness and pan ramping over the same span |

---

## Parameters and persistence

52 parameters, in the engine's own APVTS. Ids are `tk_<trackId>_bsb_<name>`, where
`<trackId>` is `bas_<n>` for a Bass tab - so `tk_bas_0_bsb_flt_cutoff` is the first Bass
tab's filter cutoff. The id **set** is identical to BaySickSynth's; only the `bsb_` tag
and the defaults differ.

| Id suffix | Type | Range | Default |
|---|---|---|---|
| `outVol` | float | 0 - 1 | 0.8 |
| `waveform` | choice | SAW, SAW+SAW, PULSE, SAW+SQUARE, SQUARE+SQUARE, SUPERSAW, BELL, DEAF SAW, SPREAD OCT, SPREAD 5TH, SINE | SAW |
| `transpose` | int | -24 - +24 | 0 |
| `modifier` | float | 0 - 1 | 0.5 |
| `dualOscMode` | choice | Musical, Hz Offset, Absolute Hz | Musical |
| `oscSync` | bool | - | off |
| `ringMod` | bool | - | off |
| `drift` | float | 0 - 1 | 0 |
| `unison_voices` | int | 1 - 7 | 1 |
| `unison_detune` | float | 0 - 1 | 0.2 |
| `unison_spread` | float | 0 - 1 | 0.8 |
| `noise` | float | 0 - 1 | 0 |
| `noiseOnly` | bool | - | off |
| `noiseColor` | choice | White, Pink, Brown | White |
| `voiceMode` | choice | Poly, Mono, Legato | **Mono** |
| `glide` | float | 0 - 2 s | **0.08** |
| `cutSelf` | bool | - | off |
| `cutSelfMode` | bool | false = Same Pitch, true = Cut All | Same Pitch |
| `modWheelDest` | choice | Filter, LFO | Filter |
| `modWheelAmt` | float | 0 - 1 | 0 |
| `amp_attack` | float | 0.001 - 10 s | **0.005** |
| `amp_decay` | float | 0.001 - 10 s | **0.15** |
| `amp_sustain` | float | 0 - 1 | **0.6** |
| `amp_release` | float | 0.001 - 10 s | **0.20** |
| `velAmpTrack` | float | 0 - 1 | 1.0 |
| `pEnv_attack` / `pEnv_decay` / `pEnv_release` | float | 0.001 - 10 s | 0.01 / 0.10 / 0.30 |
| `pEnv_sustain` | float | 0 - 1 | 0 |
| `pEnv_amt` | float | -24 - +24 semitones | 0 |
| `trans_amount` | float | 0 - 1 | 0 |
| `trans_duration` | float | 0 - 20 ms | 5 |
| `trans_colour` | float | 200 - 10000 Hz | 5000 |
| `burst_mode` | bool | - | off |
| `burst_count` | int | 1 - 8 | 4 |
| `burst_spacing` | float | 1 - 100 ms | 20 |
| `flt_type` | choice | Low Pass, High Pass, Band Pass, Notch | Low Pass |
| `flt_cutoff` | float | 20 - 20000 Hz | **4000** |
| `flt_res` | float | 0 - 1 | **0.2** |
| `flt_env_amt` | float | -1 - +1 | **0.3** |
| `flt_kbtrack` | float | 0 - 1 | 0 |
| `flt_veltrack` | float | 0 - 1 | 0 |
| `flt_attack` | float | 0.001 - 10 s | **0.005** |
| `flt_decay` | float | 0.001 - 10 s | **0.20** |
| `flt_sustain` | float | 0 - 1 | **0.3** |
| `flt_release` | float | 0.001 - 10 s | **0.20** |
| `lfo_shape` | choice | Sine, Saw, Square | Sine |
| `lfo_dest` | choice | Filter, Pitch, Osc Modifier | Filter |
| `lfo_rate` | float | 0.01 - 30 Hz | 1 |
| `lfo_sync` | bool | - | off |
| `lfo_division` | choice | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 | 1/4 |
| `lfo_amount` | float | 0 - 1 | 0 |

**What is saved where.**

- **With the project** - everything above. `getStateInformation` is `apvts.copyState()` to
  XML to binary, and the Bass tab record stores that blob base64-encoded. No file
  references, no side state, so a BaySickBass tab always restores fully.
- **With a preset file** - the same APVTS tree as plain XML. Preset files carry the saving
  tab's prefix; the loader rewrites it.
- **Per-machine, not in the project** - the panel's active tab, and the window's position
  and size (owned by the workspace, not by the engine).
- **Not saved at all** - pending audition notes, the visualizer's animation phase, and the
  per-note expression state a voice is holding.

**Undo.** The APVTS is bound to the one app-wide `UndoManager` and tagged with a stable
rig identity (`rig:<kind>:<pageIndex>`), so edits join the global history and survive the
engine being rebuilt. Preset loads and the internal state swap are single undoable actions
(`replaceStateKeepingUndoHistory`).

---

## Lifetime and teardown

`EngineRig` owns the engine, constructed in `createEngineFor` keyed on
`(TabKind::Bass, pageIndex)`, prepared and registered for audio dispatch there.
`BassPage` holds a **non-owning** pointer plus the editor it created from it.

- Closing the window destroys the editor only. The engine keeps running and the tab keeps
  playing.
- The engine choice locks on first pick, so an existing Bass tab cannot be switched to a
  different player.
- Automation applicators are registered model-side at engine creation and re-resolve their
  target through the rig at apply time, so lanes keep working across window open/close.
- The editor removes its APVTS parameter listeners and its state listener in its
  destructor, and cross-thread repaint hops are `SafePointer`-guarded.
- A Bass tab's whole state (engine type, lock flag, engine blob) round-trips through
  `BassPage::exportBassState` / `importBassState`, which rewrites the parameter prefix when
  a page's state is imported into a different page.

---

## Cross-references

- `BaySickSynth.md` - the same engine with general-purpose defaults; the deeper
  explanation of each waveform and of the shared DSP lives there.
- `BaySickSolstice.md` - the additive engine, the other synth available on a Bass tab.

---

## Differs from Carry-Forward

- Carry-Forward's engine-audition entry says "All 4 engine processors
  (BaySickSynth/Bass/BaySickSolstice/BaySickPlayer)". There are now **seven** engines carrying the
  `auditionNote` pattern, and BaySickBass's version has grown a press-and-hold pair
  (`auditionNoteOn` / `auditionNoteOff`) with an accumulating note-off mask alongside the
  original one-shot exchange.
- Carry-Forward predates model-owned engines. BaySickBass is no longer owned by its page:
  `EngineRig` owns it and `BassPage` holds a non-owning view pointer.
