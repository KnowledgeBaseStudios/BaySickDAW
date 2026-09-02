# BaySickSynth

**Purpose** - BaySickSynth is the app's general-purpose subtractive synthesizer: it makes
sound by generating a raw waveform and then carving it with a filter and two envelopes.
It is one of the players you can load on a Layers tab or a Drums tab, and it is the
engine to reach for when you want a lead, a pad, a stab, a synthetic drum, or a noise
effect built from scratch rather than from a sample. It shares its entire DSP core with
BaySickBass, which is the same instrument shipped with bass-friendly starting values.

---

## How it operates

Three layers, each in its own file:

| Layer | Files | Job |
|---|---|---|
| Processor | `Source/BaySickSynth/BaySickSynthProcessor.h` / `.cpp` | Owns the APVTS parameter set, reads it once per block, pushes changed values into the DSP, injects audition notes, applies the master out gain, scrubs NaN/Inf. |
| Voice manager | `Source/BaySickSynth/BaySickSynthDSP.h` / `.cpp` | Owns 16 `BaySickSynthVoice` objects inside a `BroadcastSynthesiser`. Rewrites the incoming MIDI stream to implement Mono / Legato and the Cut Self behavior. |
| Voice | `Source/BaySickSynth/BaySickSynthVoice.h` / `.cpp` | All per-note DSP, sample by sample. |

The editor is `BaySickSynthEditor.h` / `.cpp`, with three helper components:
`BaySickVisualizerScreen` (the animated display strip), `BssEditorComponents`
(`BssLedRadio` button rows and `BssFilterXYPad`), and `BaySickSynthLAF.h` (colors).

**Per-voice signal path** (`BaySickSynthVoice::renderNextBlock`, in order):

1. Slide ramps for loudness and pan (per-note expression, see below).
2. Glide - the current frequency is multiplied toward the target each sample.
3. LFO sample, boosted by the mod wheel if the mod wheel is routed to LFO.
4. Pitch = note + Transpose + pitch wheel + LFO (if routed to Pitch) + pitch envelope + analog drift.
5. Filter cutoff = base cutoff, multiplied by keyboard tracking, velocity tracking,
   filter envelope, LFO (if routed to Filter), mod wheel (if routed to Filter), and the
   per-note CC74 offset. Clamped to 20 Hz - 20 kHz.
6. Oscillator sample for the selected waveform (skipped entirely in Noise Only mode).
7. Noise added (white, pink, or brown).
8. State-variable filter (`Source/SynthFilter.cpp`).
9. Amp envelope times velocity.
10. Burst multiplier, if Burst is on.
11. One-millisecond declick fade-in.
12. Transient click added **after** the filter and envelope, so it always cuts through.
13. Unison stack - extra detuned saw copies, panned across the stereo field.
14. Cut-self fade, if a hard cut is running.
15. Per-note pan, then summed into the stereo output.

After the synthesizer returns, the processor multiplies by **Out Vol**, then by a fixed
-12 dB calibration trim (`kOutputHeadroom` in `BaySickSynthProcessor.cpp`), and replaces
any non-finite sample with zero. The trim is a constant baked into the output stage, not a
control - the oscillator table runs at full scale, so without it a three-note chord clipped
before the mixer fader ever saw it.

**Threading.** `processBlock` runs on the audio thread. `updateFromApvts` is gated by an
`ApvtsDirtyTracker`, so the ~50 parameter reads only happen on blocks where something
actually changed (a host tempo change also counts, so tempo-synced LFO keeps tracking).
The editor's LED rows, XY pad and visualizer never repaint from the parameter callback -
that callback can arrive on an offline render thread - they set an atomic flag and a
30 Hz timer does the repaint.

**Note auditioning.** Clicking a key in the piano roll reaches the engine through
`auditionNote` / `auditionNoteOn` / `auditionNoteOff`, which are plain atomics drained at
the top of `processBlock`. Note-offs accumulate in a 128-bit mask so a fast drag across
the keyboard cannot drop one and leave a note ringing.

**Sample-rate note (verified in code).** The per-voice filter (`SynthFilter`) and LFO
(`LFO`) are never handed the device sample rate - `BaySickSynthVoice` calls
`prepare()` on its two oscillators but not on those two objects, so both keep their
built-in 44100 Hz assumption. At a 48 kHz device the filter's real cutoff and the LFO's
real speed both run about 9 percent above the numbers on screen. Everything else in the
voice reads the live rate through `getSampleRate()` and is correct.

---

## User-facing behavior

### Where it lives

You pick BaySickSynth when you create the tab, from the ribbon's add menu
(`+ Add BaySickSynth` under Layers or Drums). **The choice locks** - a tab keeps the
player it was created with, so make a new tab if you want a different one.

The panel opens in its own window. The window's title strip shows the engine name and
carries the **Preset** button. The window will not shrink below 558 x 455 pixels.

Across the top of the panel is a 120-pixel animated display, and under it a row of six
tabs. Only one tab's controls are on screen at a time.

| Tab | What it holds |
|---|---|
| OSC | Waveform choice, tuning, voice mode, master level, mod wheel |
| OSC ENV | The volume envelope and the pitch envelope |
| FILTER | The filter XY pad, filter type, keyboard and velocity tracking |
| FLT ENV | The filter's own envelope, and how strongly it moves the filter |
| LFO | The wobble generator - shape, speed, target, depth |
| MOD | Noise, transient click, burst, drift, unison |

The display changes with the tab: OSC draws the selected waveform, OSC ENV and FLT ENV
draw the envelope shape as you move it, FILTER draws the filter's frequency response
curve including the resonance peak, and LFO draws a scrolling wave moving at the real
LFO speed. On the MOD tab the display keeps showing the LFO.

Every knob shows its value in a pop-up while you drag it. **Double-click any knob to
return it to its factory default.** Right-click a knob for the app-wide Automate and
Type-in-value menu.

### OSC tab

**WAVEFORM** (drop-down). This is the raw sound before anything else touches it.

| Setting | What you hear | What MODIFIER does here |
|---|---|---|
| SAW | Bright, buzzy, full of harmonics. The classic synth lead and string sound. | Nothing |
| SAW+SAW | Two saws stacked and detuned against each other - thick and moving. | Detune amount |
| PULSE | Hollow and reedy. Narrow settings sound thin and nasal, centered sounds like a square. | Pulse width, 5 to 95 percent |
| SAW+SQUARE | A saw and a square together - buzz plus hollow. | Detune amount |
| SQUARE+SQUARE | Two squares - woody and hollow, good for basses and bells. | Detune amount |
| SUPERSAW | Seven saws in one oscillator, already detuned. Huge trance-style lead. | Spread of the seven, 0 to half a semitone |
| BELL | Two sine waves where one bends the other (FM). Metallic, glassy, bell-like. | Amount of bend - low is a pure tone, high is clangorous |
| DEAF SAW | A saw run through a gentle low-pass built into the oscillator. Soft and dull. | Brightness, roughly 100 Hz to 3 kHz |
| SPREAD OCT | The note plus an octave up and an octave down, mixed. Big and organ-like. | How much of the octaves you hear |
| SPREAD 5TH | The note plus a fifth above and a fifth below. Power-chord flavor. | How much of the fifths you hear |
| SINE | One pure tone, no harmonics. Sub bass, soft leads, synthetic kick drums. | Nothing |

**DUAL-OSC TUNING** (drop-down, under the waveform). Only bites on SAW+SAW,
SAW+SQUARE and SQUARE+SQUARE - it changes what MODIFIER means for the second
oscillator.

| Setting | MODIFIER becomes |
|---|---|
| Musical (default) | Musical detune - the two oscillators spread apart by up to 50 cents. Slow beating, thickness. |
| Hz Offset | A fixed offset in hertz added to the second oscillator, 0 to 2000 Hz. Inharmonic, metallic. |
| Absolute Hz | The second oscillator is pinned to one frequency, 20 Hz to 20 kHz, and does not follow the keyboard. Useful for metallic percussion. |

The MODIFIER knob's read-out and tooltip change to match the mode you pick.

**SYNC** (toggle, default off). Hard sync. The second oscillator's cycle is forced to
restart every time the first one completes. Only active on the three dual-oscillator
waveforms. On its own it adds a hard edge; sweep the second oscillator's pitch with
MODIFIER in one of the Hz modes and you get the classic tearing sync sweep.

**RING** (toggle, default off). Ring modulation - the two oscillators are multiplied
together instead of added. Metallic, clangy, bell-like, often atonal. Only active on the
three dual-oscillator waveforms. Stacks with SYNC.

**TRANSPOSE** (knob, -24 to +24 semitones, default 0). Shifts the whole instrument up or
down in semitones. -12 is one octave down, +12 is one octave up.

**MODIFIER** (knob, 0 to 1, default 0.5). See the waveform table above - its meaning
depends on the waveform and on the dual-osc tuning mode.

**NOISE** (knob, 0 to 1, default 0). Mixes hiss on top of the oscillator. A little adds
breath and grit; a lot turns a note into a wind or snare sound. The color of the noise is
set on the MOD tab.

**VOICE MODE** (three buttons: Poly / Mono / Legato, default Poly).

| Mode | Behavior |
|---|---|
| Poly | Full chords. Up to 16 notes ring at once. |
| Mono | One note at a time - each new note cuts the one before it and retriggers the envelopes. |
| Legato | One note at a time, but overlapping notes **slide** into each other without restarting the envelopes. Release the newer note while the older one is still held and the pitch slides back. This is the mode that makes SLIDE musical. |

**CUT SELF** (toggle, default off) and the mode button beside it (**SAME PITCH** /
**CUT ALL**, default Same Pitch). Poly mode only - the other modes already cut. With Cut
Self on, playing a note performs an instant, click-free hard cut first: Same Pitch cuts
only a still-ringing copy of the same note (stops the doubling you get from fast
retriggers), Cut All cuts every ringing voice on each new note (gives a choked, gated
feel). The button's caption changes to show which mode is selected.

**SLIDE** (knob, 0 to 2 seconds, default 0). Glide / portamento time. With SLIDE above
zero, a new note bends up or down from the note before it instead of jumping - but only
if the previous note is still held. In Legato mode this is the main expressive control.

**OUT VOL** (knob, 0 to 1, default 0.8). The engine's own output level, before the mixer
strip. On top of whatever this knob is set to, the output stage applies a fixed -12 dB
calibration trim that is not exposed as a control and does not show in any readout. That
trim is deliberate: the oscillators run at full scale, so it is what leaves room for a
chord. BaySickSynth is therefore quiet on its own, by design - it is meant to be brought
up by what comes after it rather than by pinning OUT VOL to the top.

**MOD WHEEL** group. Two buttons pick the destination - **Filter** (default) or **LFO** -
and **AMOUNT** (knob, 0 to 1, default 0) sets how far the wheel goes. On Filter, a fully
raised wheel opens the cutoff by up to 2 octaves. On LFO, the wheel adds LFO depth on top
of whatever the LFO tab's AMOUNT is set to. At AMOUNT 0 the wheel does nothing.

### OSC ENV tab

Two boxes side by side.

**AMP ENV** - four vertical faders shaping the note's volume over time.

| Control | Range | Default | What it does |
|---|---|---|---|
| ATTACK | 0.001 - 10 s | 0.01 s | How long the note takes to reach full volume. Short is a click or pluck; long is a swell. |
| DECAY | 0.001 - 10 s | 0.10 s | How long it takes to fall from full volume to the sustain level. |
| SUSTAIN | 0 - 1 | 0.8 | The level it holds at while the key is down. |
| RELEASE | 0.001 - 10 s | 0.30 s | How long the note fades after you let go. |
| VEL (knob) | 0 - 1 | 1.0 | How much playing harder makes it louder. 1 = fully velocity-sensitive, 0 = every note the same volume (useful for pads and for drum parts you want even). |

**PITCH ENV** - the same four stages, but they bend the pitch instead of the volume.
The **AMOUNT** knob (-24 to +24 semitones, bipolar, default 0) sets how far and which
way. Positive means the pitch starts high and settles; negative means it starts low and
rises. At 0 the whole pitch envelope does nothing. A short decay with a large negative
amount is how you build a synthetic kick drum.

### FILTER tab

**The XY pad** (left half). Click or drag anywhere in it. Left-to-right is **CUTOFF**
(20 Hz to 20 kHz on a logarithmic scale, default fully open at 20 kHz); bottom-to-top is
**RES** - resonance (0 to 1, default 0). Crosshairs and a glowing dot show where you are.
Cutoff decides how much of the sound gets through; resonance boosts a narrow band right
at the cutoff, which is what makes a filter sweep whistle and sing.

**TYPE** (four buttons: LP / HP / BP / Notch, default LP).

| Type | What it keeps |
|---|---|
| LP - Low Pass | Everything below the cutoff. Turn the cutoff down for warm and muffled. |
| HP - High Pass | Everything above the cutoff. Turn the cutoff up for thin and tinny. |
| BP - Band Pass | Only a band around the cutoff. Nasal, telephone-like. |
| Notch | Everything except a band at the cutoff. Hollow, phasing-like. |

**TRACKING** group, two knobs, both 0 to 1 and both defaulting to 0:

- **KEYBOARD** - how much the cutoff follows the notes you play. At 1 the cutoff moves a
  full octave for every octave you play, so high notes stay as bright as low ones.
  At 0 the cutoff is fixed and high notes sound progressively duller.
- **VELOCITY** - how much playing harder opens the filter. At 1, a hard hit opens the
  cutoff up to about 2 octaves above a soft one.

### FLT ENV tab

Four faders and a knob. The four faders are the same ATTACK / DECAY / SUSTAIN / RELEASE
shape as the volume envelope (defaults 0.01 s / 0.10 s / 0.5 / 0.30 s), but this envelope
moves the **filter cutoff** rather than the volume.

**AMOUNT** (knob, -1 to +1, default 0) decides how far and which way. At +1 the envelope
sweeps the cutoff up to 4 octaves open; at -1 it sweeps 4 octaves closed. At 0 the filter
envelope does nothing at all - which is the factory setting, so nothing happens until you
turn this up.

### LFO tab

The LFO is a slow wave that wobbles something for you.

- **SHAPE** (three buttons: Sine / Saw / Square, default Sine). Sine is a smooth
  back-and-forth; Saw ramps and snaps back; Square jumps between two values.
- **RATE** (knob, 0.01 - 30 Hz, default 1 Hz). How fast the wobble is. Grayed out while
  SYNC is on.
- **SYNC** (toggle, default off) plus the division drop-down (1/1, 1/2, 1/4, 1/8, 1/16,
  1/32, default 1/4). With SYNC on, the LFO locks to the song tempo and the division picks
  the note value. The drop-down is grayed out while SYNC is off.
- **DEST** (three buttons: Filter / Pitch / Osc Mod, default Filter). Filter wobbles the
  cutoff (up to plus or minus 2 octaves at full depth); Pitch wobbles the tuning (up to
  plus or minus 4 semitones) for vibrato; Osc Mod wobbles the MODIFIER knob's value, which
  is how you get a moving pulse width or a moving detune.
- **AMOUNT** (knob, 0 to 1, default 0). Depth. At 0 the LFO does nothing, so this is the
  knob to raise first.

### MOD tab

Five groups across one row.

**NOISE**
- **NOISE ONLY** (toggle, default off). Mutes the oscillator entirely and makes noise the
  whole sound. The NOISE knob on the OSC tab then acts as the level - and if that knob is
  at zero, noise-only mode plays at full level rather than silence.
- **Color** (drop-down: White / Pink / Brown, default White). White is flat and hissy;
  Pink rolls off 3 dB per octave and sounds warmer and more musical; Brown rolls off
  6 dB per octave and sounds deep and rumbling. This affects both the mixed-in noise and
  noise-only mode.

**TRANSIENT** - a short burst of filtered noise fired at the very start of each note,
added after the filter and envelope so it always punches through.
- **AMT** (0 to 1, default 0). Level of the click. At 0 the whole feature is off.
- **DUR** (0 to 20 ms, default 5 ms). How long the click lasts.
- **COLOUR** (200 Hz to 10 kHz, default 5 kHz). The click's brightness - low is a thump,
  high is a tick.

**BURST ENV** - fires several quick volume pulses at the start of the note, on top of the
normal envelope. This is how you build hand-clap and flam sounds.
- **BURST** (toggle, default off).
- **COUNT** (1 to 8, default 4). How many pulses.
- **SPACING** (1 to 100 ms, default 20 ms). Gap between them.

**DRIFT** (knob, 0 to 1, default 0). Slow random pitch wander, independent per voice, up
to about a quarter tone at maximum. 0 is dead-perfect digital tuning; small amounts make
chords and pads breathe. Most audible on long sustained notes.

**UNISON** - stacks extra detuned copies of the note.
- **VOICES** (1 to 7, default 1). 1 means off. 3 to 5 gives a fat stacked lead.
- **DETUNE** (0 to 1, default 0.2). How far apart the copies are tuned, up to plus or
  minus 50 cents.
- **SPREAD** (0 to 1, default 0.8). How far the copies are panned apart. 0 is dead
  center, 1 is hard left to hard right.

The extra unison copies are always sawtooth waves regardless of the WAVEFORM setting.

### Presets

The **Preset** button on the window's title strip opens the preset menu. Factory presets
live in `Documents\BaySickDAW\Presets\BaySickSynth\`, and each folder in there becomes a
submenu. **Save preset...** writes to `Presets\BaySickSynth\My Presets\`.

On a **Layers** tab, loading a preset also renames the tab, the mixer strip and the
piano-roll label to the preset's filename. On a **Drums** tab it does not - `DrumPage` does
not wire the engine editor's patch-loaded notification, so the tab keeps whatever name it
had.

Presets are portable between tabs: the loader rewrites the saved parameter prefix to the
destination tab's prefix, so a preset saved on Layers 1 loads correctly on Drums 3.

### Per-note expression from the piano roll

Note properties drawn in the piano roll reach the engine as MIDI controllers and are
applied per note:

| Controller | Effect |
|---|---|
| CC1 (mod wheel) | Whatever MOD WHEEL is routed to |
| CC10 | Per-note pan; a note already sounding glides to the new position over about 8 ms |
| CC71 | Per-note resonance offset, plus or minus half the resonance range |
| CC72 | Per-note release scale, from a quarter to four times the RELEASE setting |
| CC74 | Per-note brightness, plus or minus 2 octaves of filter cutoff |
| CC84 (+ CC5 / CC37) | Slide: the note starts at another pitch and glides in, with an optional glide time in milliseconds |
| CC85 / CC86 / CC89 | Slide takeover of a sounding note, with loudness and pan ramping over the same span |

---

## Parameters and persistence

All 52 parameters live in the engine's own APVTS. Ids are
`tk_<trackId>_bss_<name>`, where `<trackId>` is `lay_<n>` for a Layers tab or
`drm_<n>` for a Drums tab - so `tk_lay_0_bss_flt_cutoff` is the first Layers tab's filter
cutoff. Every id is globally unique, which is what lets several BaySickSynth tabs coexist
and hold independent automation lanes.

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
| `voiceMode` | choice | Poly, Mono, Legato | Poly |
| `glide` | float | 0 - 2 s | 0 |
| `cutSelf` | bool | - | off |
| `cutSelfMode` | bool | false = Same Pitch, true = Cut All | Same Pitch |
| `modWheelDest` | choice | Filter, LFO | Filter |
| `modWheelAmt` | float | 0 - 1 | 0 |
| `amp_attack` / `amp_decay` / `amp_release` | float | 0.001 - 10 s | 0.01 / 0.10 / 0.30 |
| `amp_sustain` | float | 0 - 1 | 0.8 |
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
| `flt_cutoff` | float | 20 - 20000 Hz | 20000 |
| `flt_res` | float | 0 - 1 | 0 |
| `flt_env_amt` | float | -1 - +1 | 0 |
| `flt_kbtrack` | float | 0 - 1 | 0 |
| `flt_veltrack` | float | 0 - 1 | 0 |
| `flt_attack` / `flt_decay` / `flt_release` | float | 0.001 - 10 s | 0.01 / 0.10 / 0.30 |
| `flt_sustain` | float | 0 - 1 | 0.5 |
| `lfo_shape` | choice | Sine, Saw, Square | Sine |
| `lfo_dest` | choice | Filter, Pitch, Osc Modifier | Filter |
| `lfo_rate` | float | 0.01 - 30 Hz | 1 |
| `lfo_sync` | bool | - | off |
| `lfo_division` | choice | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 | 1/4 |
| `lfo_amount` | float | 0 - 1 | 0 |

**What is saved where.**

- **With the project** - everything above. `getStateInformation` is
  `apvts.copyState()` to XML to binary; the tab record stores that blob base64-encoded as
  its `engineData` attribute. There is no file reference and no side state, so a
  BaySickSynth tab always restores fully.
- **With a preset file** - the same APVTS tree, written as plain XML by the Preset menu.
  Preset files carry the saving tab's parameter prefix; the loader rewrites it on load.
- **Per-machine, not in the project** - the panel's active tab (OSC / OSC ENV / ...) and
  the window's position and size. Window placement is handled by the workspace, not by
  the engine.
- **Not saved at all** - pending audition notes, the visualizer's animation phase, and
  the per-note expression state a voice is holding.

**Undo.** The engine's APVTS is bound to the one app-wide `UndoManager` at construction
and tagged with a stable rig identity (`rig:<kind>:<pageIndex>`), so parameter edits join
the global undo history and survive the engine being torn down and rebuilt. Preset loads
and the internal state swap are single undoable actions
(`replaceStateKeepingUndoHistory`).

---

## Lifetime and teardown

The engine is **owned by the model**, not by the panel. `EngineRig` constructs it in
`createEngineFor` when the tab is created, keyed on `(TabKind, pageIndex)`, prepares it,
and registers it for audio dispatch. `LayersPage` / `DrumPage` hold a **non-owning**
pointer plus the editor they created from it.

Order that matters:

- The page is a disposable view. Closing the window destroys the editor; the engine keeps
  running and the tab keeps playing.
- The engine choice locks on first pick (`selectEngine` early-returns once
  `mEngineLocked` is set), so an existing tab cannot be switched to another player.
- Automation applicators are registered **model-side** at engine creation
  (`registerModelEngineAutomation`), never against a widget, and they re-resolve their
  target through the rig at apply time - which is what lets automation keep working after
  a window has been closed and reopened.
- The editor removes its two APVTS parameter listeners and its state listener in its
  destructor, and every cross-thread repaint hop is guarded by a `SafePointer`, because a
  callback already queued cannot be retracted.

---

## Cross-references

- `BaySickBass.md` - the same DSP with bass-tuned defaults; read that one for the Bass tab.
- `BaySickSolstice.md` - the additive engine, the other synth you can load on Layers and Bass.

---

## Differs from Carry-Forward

- Carry-Forward's engine-audition entry says "All 4 engine processors
  (BaySickSynth/Bass/BaySickSolstice/BaySickPlayer)". There are now **seven** engines carrying the
  `auditionNote` pattern (the three sfizz engines were added), and BaySickSynth's version
  has grown a press-and-hold pair (`auditionNoteOn` / `auditionNoteOff`) with an
  accumulating note-off mask alongside the original one-shot exchange.
- Carry-Forward predates model-owned engines. BaySickSynth is no longer owned by its page:
  `EngineRig` owns it, and the page holds a non-owning view pointer.
