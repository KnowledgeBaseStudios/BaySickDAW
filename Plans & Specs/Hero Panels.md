# Hero Panels - build spec (2026-08-30)

A HERO PANEL is a specialized, professional-grade graphic tailored to one
effect - in most cases interactive - replacing or sitting alongside the
generic scrolling in/out trace every effect shares today.

This doc says what has to be BUILT, in DSP first and drawing second. It
covers BaySickDAW and KBS Plugins; each entry is tagged **[BSD]**, **[KBS]**
or **[both]**. Where an API already exists it is named so nobody rewrites it.

---

## 0. What exists today (the honest baseline)

### BaySickDAW

| Piece | Where | What it gives you |
|---|---|---|
| `EffectVisualFeed` | `Source/DSP/EffectVisualFeed.h` | Lock-free column ring. `push(lo, hi, a, b)` - **4 floats per column, that is all**. Gated on `isActive()` (watcher count), so a closed window costs one relaxed atomic load. |
| `DSPBase::visualPushInOut(buf, inPk)` | `Source/DSP/DSPBase.h:186` | The standard "in envelope vs out envelope" push every current visual uses. |
| `hasVisualFeed()` / `hasVisual()` | `DSPBase.h:43,54` | Presence gates. **They are deliberately separate**: `hasVisual()` alone supports a PARAMETRIC visual - one that reads DSP state at paint time and streams nothing. That is the cheap path and most heroes should take it. |
| `SpectrumFeed` | `Source/DSP/SpectrumFeed.h` | Seqlock, 1024 floats, wait-free push / droppable poll. For anything spectral. |
| `EffectVisualWindow` | `Source/Standalone/EffectWindows.cpp` (~line 2050+) | The existing per-type draw switch and its captions. |
| Column rate | `DSPBase::visualColumnSeconds()` | 128 samples @ 44.1 kHz per column, fixed, so time axes are labelable. |

**The constraint that shapes everything:** the feed carries 4 floats per
column. Any hero needing a curve, a band set, a tap list or particle state
must either (a) be parametric - read DSP state at paint time, push nothing,
or (b) get a new dedicated feed. Do not try to smuggle structure through
`push(lo, hi, a, b)`.

Query APIs that ALREADY exist and must be reused, not rebuilt:
- `CompressorDSP::computeGainDb(float levelDb) const noexcept` - a pure
  static-curve evaluator. This is exactly what a transfer-curve hero needs
  and it is already there.
- `getGainReductionDb()` on Compressor, Limiter, Gate, De-esser.
- `lfoPhase()` on Phaser and Flanger; `lfoPhase(int voice)` on Chorus.
- `ParametricEq::bandMagnitude()` / `bandPhase()` - full response query.
- `EqMatch` (`Source/DSP/Kbs/EqMatch.h` + `EqWindowUI/EqMatchPanel.h`) -
  already shipped; do NOT re-spec it.

### KBS Plugins

- Hero framework **written and unwired**: `Source/UI/Hero.h` (`HeroDisplay`)
  and `Source/UI/HeroDelay.h` (`DelayTapDisplay`, `DelayPicture`). No product
  defines `makeHero()`. Wiring this is step one on that side.
- Bespoke visuals that ship: EqPro (`UI/EqGraph.h`, `EqAnalyser.h`,
  `EqRail.h`, `EqMatchPanel.h`), Meter (`UI/Meters.h`), NAMHost
  (`UI/MicPage.h` placement plot, `ModelPage.h`), Guitar (photoreal
  `HardwareSkin` + Blender assets).
- Everything else is a knob panel.
- Reusable DSP cores: `Core/FDN.h`, `SVF.h`, `DynamicBell.h`,
  `DynamicShelf.h`, `Envelope.h`, `Hilbert.h`, `PitchShift.h`, `Cabinet.h`,
  `Loudness.h`, `FFT.h`, `SpectrumScan.h`, `Lookahead.h`, `ParametricEq.h`.

### Reference art already drawn (KBS `Assets/Hardware/hero/`, 32 files)

**Hero displays (10)** - usable as the visual target for the panels below:

| File | What it is | Feeds which panel |
|---|---|---|
| `h141_spectrum_band` | Spectrum with a dynamic-EQ band overlay, GR chips, tilt handles | De-esser, Dynamic EQ, De-reverb |
| `h144_wavetable` | 3D wavetable ribbon, current frame lit | Synth pedal, wavetable synth |
| `h145_waveform` | Waveform / transient display | Transient Shaper, Gate |
| `h146_granular` | Waveform with grain particles | Glitch, granular |
| `h147_response` | Filter response curve, filled | Filter, Wah, Graphic EQ, Acoustic Sim |
| `h149_envelope` | ADSR with draggable nodes | Gate, Transient Shaper, envelopes |
| `h150_lfo_path` | LFO curve with nodes | Chorus, Flanger, Tremolo, Auto-Pan |
| `h151_particles` | Hex particle field | Character, Glitch, ambience |
| `h152_tap_timeline` | Delay taps on a timeline | Delay |
| `h153_keyboard` | Piano keyboard, keys lit | Pitch, Octave, EQ piano strip |
| `h154_illustrations` | Icon set (delay, reverb, filter, saturate, imaging, pitch) | Picker art, chapter headers |

**Generic furniture (22)** - reuse directly, do not redraw:
`h136_value_arc`, `h137_dot_ring`, `h139_value_unit`, `h140_mod_ring`
(knob + value art); `h155_bar_slim`, `h156_seg_ladder`, `h157_gr_strip`,
`h158_level_pill` (meters); `h160_tab_bar`, `h161_seg_switch`,
`h162_slider_thin`, `h163_fader_bank`, `h164_readout`, `h165_stepper`,
`h166_toggle_pill`, `h167_selector`, `h168_preset_bar` (controls);
`h169_hairline_frame`, `h170_grid_axis`, `h173_ground_gradient`,
`h174_ground_cream` (framing).

---

## 1. Shared infrastructure to build FIRST

Every panel below assumes these. Build them once.

### 1.1 Static-curve query contract **[both]**
```cpp
// DSPBase addition
virtual bool  hasTransferCurve() const { return false; }
virtual float transferOutDb (float inDb) const noexcept { return inDb; }
```
A pure, allocation-free, thread-safe evaluation of the effect's STATIC gain
law at an arbitrary input level, with no audio processed. Compressor already
implements this shape (`computeGainDb`); Limiter, Gate, Overdrive,
Saturation and every drive pedal must add it.
*Why it matters:* one widget then draws the curve for eight effects.

### 1.2 Frequency-response query contract **[both]**
```cpp
virtual bool  hasFrequencyResponse() const { return false; }
virtual float responseMagDb (float hz) const noexcept { return 0.0f; }
virtual float responsePhase (float hz) const noexcept { return 0.0f; }
```
Filter, Wah, Phaser, Flanger, Graphic EQ pedals, Acoustic Sim, De-esser
detector. `ParametricEq` already answers this; the others must expose their
coefficients through it. Must be evaluable off the audio thread against a
snapshot, so coefficients need a double-buffered or atomically swapped
publish (the EQ's existing pattern is the model).

### 1.3 Detector-state snapshot **[both]**
```cpp
struct DetectorState { float envDb, grDb, thresholdDb; int phase; float phaseProgress; };
virtual bool getDetectorState (DetectorState&) const noexcept { return false; }
```
`phase` = idle/attack/hold/release for gates and dynamics. Lets a hero draw
WHERE in its cycle the effect is, not just how much it is pulling.

### 1.4 A structured visual feed **[both]**
The 4-float column ring stays for envelope traces. Add a second, generic
channel for structured frames:
```cpp
template <typename Frame> class StructFeed;   // seqlock, single writer, droppable read
```
Same discipline as `SpectrumFeed`: wait-free push, poll may fail, UI drops
the frame. Needed by Delay (tap list), Glitch (slice state), Imaging
(correlation cloud), Reverb (ER pattern).

### 1.5 Hero host component **[BSD]**
`EffectVisualWindow` currently switches on type inside one paint. Refactor to
a `HeroPanel` base with one subclass per effect, selected by a factory. KBS
already has this shape in `UI/Hero.h` - port it rather than invent it.

### 1.6 Wire `makeHero()` **[KBS]**
`HeroDisplay` and `DelayTapDisplay` exist and are dead. Give `ProductSpec` a
`makeHero()` hook, have `SuiteEditor` mount it in the effects-skin middle
column (where Meter already puts `MeterPanel`), and light up Delay first.

---

## 2. Dynamics

### 2.1 Compressor **[both]**
- **Draws:** transfer curve (in dB -> out dB) with the knee drawn to scale, a
  live dot riding the current input, GR needle, per-mode face (Modern / FET
  all-buttons / Opto lamp / CS pedal).
- **DSP that exists:** `computeGainDb(levelDb)`, `getGainReductionDb()`, the
  4-width knee table (`kWidths[] = {0, 6, 7, 15}`), 8 knee settings.
- **DSP to add:** implement 1.1 (trivial - wrap `computeGainDb`). Add
  `getDetectorState` (1.3) so attack/release phase is drawable. For the FET
  "all-buttons-in" mode expose the program-dependent ratio actually in force,
  since it climbs with level and a static curve would lie.
- **Plumbing:** parametric. No new feed.
- **Interaction:** drag threshold (x), ratio (curve slope past the knee),
  knee width; double-click resets. Writes the same params the knobs do.
- **Art:** `h147_response` for the plot frame, `h157_gr_strip` for GR,
  `h170_grid_axis`.

### 2.2 Limiter **[both]**
- **Draws:** ceiling wall with the GR trench beneath it, the lookahead window
  as a lead-in shaded region, true-peak overs as flags on the timeline,
  LUFS arc.
- **DSP that exists:** `getGainReductionDb()`, `getLatencySamples()`,
  Maximizer/Limiter mode, the servo constants (`kServoDbPerSec` etc.).
- **DSP to add:** transfer curve (1.1) including the soft-sat knee
  (`satTh`/`satCv`); a rolling **true-peak over counter** with timestamps
  (needs the oversampled peak detector to publish, not just measure);
  expose the lookahead buffer length in seconds; integrated LUFS if the arc
  is wanted (KBS has `Core/Loudness.h`, BSD has `LufsMeterDSP` - reuse).
- **Plumbing:** existing envelope feed for the trench + a small over-event
  ring (structured feed, 1.4).
- **Interaction:** drag ceiling; click an over flag to move the transport
  there (BSD only - KBS has no transport).

### 2.3 Gate **[both]**
- **Draws:** live waveform banded by state - open / hold / closing - with the
  threshold and the 3 dB hysteresis drawn as two lines the signal visibly
  crosses; attack/hold/release as shaded time zones.
- **DSP that exists:** `getGainReductionDb()`, `kHystDb = 3.0f`.
- **DSP to add:** `getDetectorState` (1.3) with the state machine's phase
  and progress - this is the whole point of the panel and nothing publishes
  it today. Also expose the hysteresis value (currently a private constant).
- **Plumbing:** envelope feed for the waveform + detector snapshot at paint.
- **Interaction:** drag open threshold and the hysteresis offset independently.
- **Art:** `h145_waveform`, `h149_envelope`.

### 2.4 De-esser **[both]**
- **Draws:** spectrum with the detection band lit, sibilant events flagged on
  a scrolling strip, Wide<->Split morph shown as the band physically
  narrowing above 4 kHz.
- **DSP that exists:** `getGainReductionDb()`, detector HPF freq/Q params,
  mode morph.
- **DSP to add:** a **detector-band spectrum publish** - the de-esser already
  computes a filtered detection signal; push its magnitude into a
  `SpectrumFeed`. Add a sibilance event ring (time + peak dB + reduction) so
  events can be flagged rather than inferred. Frequency response of the
  detector filter via 1.2.
- **Plumbing:** SpectrumFeed + small event feed.
- **Interaction:** drag band edges (freq + Q) on the spectrum.
- **Art:** `h141_spectrum_band` (this is exactly that picture).

### 2.5 Transient Shaper **[both]**
- **Draws:** one hit, with attack and sustain segments coloured separately,
  drawn twice for the low/high split; the fast and slow envelopes overlaid so
  you see what the detector sees.
- **DSP that exists:** band split freq, fast/slow time constants, `getOsLog2`.
- **DSP to add:** publish BOTH envelope followers (fast + slow) per band -
  currently only the summed result reaches the feed. That is 4 values/column
  and the existing feed carries exactly 4, so it fits without 1.4.
- **Plumbing:** extend the existing push to carry (fastLo, slowLo, fastHi, slowHi).
- **Interaction:** drag the split frequency on the display.
- **Art:** `h145_waveform`, `h149_envelope`.

---

## 3. Saturation and drive

### 3.1 Saturation **[both]**
- **Draws:** per-mode skeuomorph - **Tube** glowing valve, **Console**
  transformer + VU, **Tape** moving reels with a flutter trace - each over a
  live transfer curve and a harmonic bar spectrum.
- **DSP that exists:** Tube/Console/Tape modes, `getTapeSpeed()`,
  `getOversamplingLog2()`, auto-gain.
- **DSP to add:** transfer curve (1.1) for the active mode's shaper; a
  **harmonic analyzer** - run a sine probe through the shaper offline at
  paint time (cheap, no audio thread involvement) and FFT it to get harmonic
  amplitudes. For Tape, publish actual wow/flutter LFO phase so the reels and
  the flutter trace move with the audio rather than decoratively.
- **Plumbing:** parametric + offline probe. No new feed except tape phase.
- **Interaction:** drag drive on the curve.

### 3.2 Overdrive **[both]**
- **Draws:** waveshaper curve with bias asymmetry visible, harmonic bars,
  pre/post filter response overlaid; Rack vs Pedal housings.
- **DSP that exists:** `getBias()`, `getParallel()`, `getOversamplingLog2()`,
  Rack/Pedal type.
- **DSP to add:** transfer curve (1.1); harmonic probe as 3.1; frequency
  response (1.2) for the Color pre-LPF and post filter.
- **Interaction:** drag drive and bias on the curve.

### 3.3 Drive pedals - Blues Drive, Distortion, Fuzz, High-Gain, Bass Driver, Bass Overdrive **[BSD]**
- **Draws:** one shared curve+harmonics widget, each in its own photoreal
  housing.
- **DSP to add:** transfer curve (1.1) on each `*StyleDSP`. They are all
  waveshapers already; this is exposing the shaping function, an hour each.
  Bass Driver additionally needs its per-band split published (low stays
  clean) so the curve can be drawn per band.
- **Note:** do this once as a template and the other five are copy-paste.

---

## 4. Modulation

### 4.1 Chorus **[both]**
- **Draws:** the three LFO voices as moving points in delay x stereo space.
- **DSP that exists:** `lfoPhase(int voice)`, 3 rates, depth, stereo spread.
- **DSP to add:** publish each voice's CURRENT delay in ms (phase alone is
  not enough once depth and base delay are in play) - 3 floats, parametric.
- **Art:** `h150_lfo_path`.

### 4.2 Flanger **[both]**
- **Draws:** the comb response itself, notches sliding with the sweep,
  feedback deepening them; through-zero shown as the notch passing DC.
- **DSP that exists:** `lfoPhase()`, depth, feedback, damping.
- **DSP to add:** frequency response (1.2) computed from the CURRENT delay
  and feedback - a comb's magnitude is analytic, so this is a formula not a
  measurement. Publish current delay in samples.

### 4.3 Phaser **[both]**
- **Draws:** allpass notch positions on a frequency axis, drawn for the
  actual stage count (1-24), sweeping live.
- **DSP that exists:** `lfoPhase()`, stage count (8 options), min/max Hz.
- **DSP to add:** frequency response (1.2) - the allpass chain's notch
  frequencies are derivable from the current allpass coefficient; publish it.
- **Interaction:** drag min/max sweep bounds.

### 4.4 Tremolo / Auto-Pan **[KBS ships, BSD lacks]**
- **Draws:** the amplitude modulation waveform, stereo position tracing.
- **DSP to add (BSD):** the effect itself - amplitude LFO with shape,
  rate/sync, depth, stereo phase offset. Trivial DSP, mostly plumbing.
- **Art:** `h150_lfo_path`.

### 4.5 Rotary **[KBS ships, BSD lacks]**
- **Draws:** rotating horn + drum at their two speeds, mic distance.
- **DSP to add (BSD):** two-band split (crossover ~800 Hz) with independent
  amplitude+delay modulation per band, speed ramping (slow/fast with
  acceleration). Publish both rotor angles.

### 4.6 Barberpole / frequency shifter **[KBS ships, BSD lacks]**
- **Draws:** notches sliding endlessly off the axis.
- **DSP to add (BSD):** Hilbert transform pair + single-sideband modulation.
  KBS has `Core/Hilbert.h` - port it. Publish shift in Hz.

---

## 5. Time

### 5.1 Delay **[both]**
- **Draws:** taps on a timeline with the feedback path drawn, ping-pong
  across stereo, sync grid overlay, lo-fi/bitcrush visible on later repeats.
- **DSP that exists (BSD):** Echo / Vocal Doubler, time, feedback, filters,
  lo-fi bit/rate, ducking.
- **DSP that exists (KBS):** 5 tap geometries x 5 loop colours, in-loop
  diffusion, frequency shift, ducking - a much richer model, and
  `DelayTapDisplay` already drawn.
- **DSP to add:** a **tap geometry query**:
  ```cpp
  struct DelayTap { float timeMs, levelDb, pan, feedbackFrac; int index; };
  int getTaps (DelayTap* out, int maxTaps) const noexcept;
  ```
  BSD must derive taps from time + feedback + cross-feed; KBS can read its
  geometry table directly. Also publish the current LFO phase for ModTime.
- **Plumbing:** structured feed (1.4) or parametric if taps are static
  between parameter changes (they are - prefer parametric).
- **Interaction:** drag a tap's time (snaps to the sync grid), drag feedback.
- **Art:** `h152_tap_timeline` - already drawn, and `HeroDelay.h` already
  implements the widget on the KBS side. **This is the cheapest real hero in
  either product.**
- **Recommended alongside:** expand BSD's delay to KBS's 5x5 model so the
  panel has something to show.

### 5.2 Reverb **[both]**
- **Draws:** top-down room sized by Room/Decay, early reflections as rays,
  the tail as a decay curve; skin per algorithm.
- **DSP that exists (BSD):** Plate/Hall/Chamber/Room/VocalBooth, decay,
  diffuse, ER level, damping, ducking.
- **DSP that exists (KBS):** 8 spaces incl. **Gated** (envelope-cut) and
  **Spring** (dispersion all-pass), full `ReverbSpace` table with erSize,
  modMs, lowDampHz.
- **DSP to add:** an **impulse model query** - not the live signal, the room:
  ```cpp
  struct RoomModel { float rt60Low, rt60Mid, rt60High, predelayMs, erCount;
                     float erTimeMs[16], erLevelDb[16]; float sizeMeters; };
  bool getRoomModel (RoomModel&) const noexcept;
  ```
  The FDN knows its line lengths and damping; converting those to an RT60 per
  band and an ER pattern is real DSP work and is the substance of this panel.
  Gated needs its envelope published; Spring needs its dispersion curve.
- **Interaction:** drag room size and pre-delay in the plan view.
- **Recommended alongside (BSD):** add Gated and Spring - they are genuinely
  different mechanisms, not presets, and the hero makes them legible.

### 5.3 De-reverb **[BSD]**
- **Draws:** split spectrogram, before against after, so the removed tail is
  visible.
- **DSP to add:** publish BOTH the input and the residual spectra into two
  `SpectrumFeed`s. The effect already estimates a tail; expose the estimate.
- **Art:** `h141_spectrum_band`.

---

## 6. EQ and spectral

### 6.1 EQ **[BSD - already flagship]**
Already has graph, analyser, spectrogram, phase overlay, piano strip,
collision tint, EQ Match, band rail, GR meters. **Do not re-spec.**
Remaining upgrades worth doing:
- **Collision against another strip** - the analyser can already take a
  second source; needs a cross-channel spectrum route and a masking metric
  (per-bin overlap integral), then tint where two sources fight.
- **Piano strip promoted** to a real key-mapped ruler with note snapping
  (currently a toggle). Art: `h153_keyboard`.

### 6.2 Graphic EQ / Bass Graphic EQ / Pro Parametric EQ pedals **[BSD]**
- **Draws:** the slider bank with the resulting curve overlaid.
- **DSP to add:** frequency response (1.2) summing the fixed bands. The band
  frequencies and Q are already constants (`kFreqs`, `kNumBands`).
- **Art:** `h163_fader_bank` + `h147_response`.

---

## 7. Amp, cab and mic

### 7.1 NAM / IR + Mic Sim + Mic Placement **[both]**
- **Draws:** a 3D cab-and-mic room; drag the mic, see the polar pattern, the
  proximity boost and the off-axis rolloff as they change; the two mics'
  comb interference drawn as a response curve.
- **DSP that exists:** BSD `MicPlacementDSP` (distance/angle/height,
  proximity, off-axis shelf, 1/sqrt(r) law), `MicSimDSP` (10 archetype EQ
  curves). KBS `Core/Cabinet.h` (10 mic voices, 5 polar patterns) plus a
  drawn `PlacementView` (top + side).
- **DSP to add:** publish the **combined response** of the two mic paths
  including the path-length delay comb - that interference is the reason two
  mics sound different from one, and nothing draws it. Frequency response
  (1.2) over the mic chain.
- **Note:** KBS's `Cabinet.h` also contains a complete, unused `ampVoicing()`
  table (Clean/Crunch/Drive/High Gain/Modern/Fuzz) with a tone stack - dead
  code today, a ready-made amp-sim if either side wants one.

---

## 8. Pedals with functional heroes **[BSD]**

| Pedal | Draws | DSP to add |
|---|---|---|
| **Tuner** | Strobe disc + needle, cents readout | Already tracks pitch; publish detected f0, cents deviation and confidence. The strobe needs phase, not just cents - publish the detector's phase accumulator. |
| **Wah** | Treadle that moves with Pedal, resonant peak sliding on the axis | Frequency response (1.2) of the sweeping bandpass; publish current centre. |
| **Octave** | The -2/-1/+1 voices against dry, with tracking confidence | Publish tracked f0 + per-voice level + a confidence value (it already tracks; nothing exposes it). |
| **Polyphonic Synth** | Voice-type illustration per the 11 Types, filter/LFO animating | Publish oscillator waveform frame + filter cutoff + LFO phase. Art: `h144_wavetable`. |
| **Acoustic Preamp / Simulator** | Body resonance curve with the feedback notch bitten out | Frequency response (1.2) over the body EQ stages - the coefficients already exist in `rebuildBodyEQ`. |
| **NAM Pedal** | Loaded capture name + its measured response | Sweep the loaded model offline once at load and cache the curve. |
| **Bass Compressor / Noise Gate** | The 2.1 / 2.3 heroes at pedal size | Same additions as those entries. |

All 16 pedals additionally want **photoreal housing art** as the baseline -
that is the Guitar-suite `HardwareSkin` approach (Blender render + knob
filmstrip), reusable wholesale.

---

## 9. Effects we do not have yet (build effect + hero together)

Each of these is a NEW DSP module, not just a panel. **[BSD]** unless noted.

### 9.1 Character / lo-fi machine
- **DSP:** 8 machines (Tape, Cassette, Vinyl, Radio, Telephone, Console,
  Digital, Broken) as a parameter table over ONE chain: wow + flutter
  (delay-line modulation), hiss generator, mains hum (fundamental + 3rd),
  impulsive crackle, level-dip dropouts, HF/LF bandwidth loss, oversampled
  asymmetric saturation, bit-crush + sample-rate reduction. An `Age` macro
  interpolates clean -> that machine's destination values.
- **Hero:** the machine itself - reels, cassette shell, spinning platter with
  crackle, radio dial. Age morphs the artwork toward destruction.
- **Publish:** wow/flutter phase, current dropout state, crackle events.
- **KBS already ships the DSP** (`CharacterSuite.h`) - port it rather than
  write it; the hero is new on both sides. Art: `h151_particles`.

### 9.2 Filter
- **DSP:** state-variable (TPT) LP/BP/HP/Notch/Peak plus a three-formant
  **Vowel** bank morphing A-E-I-O-U by interpolating formant positions.
  Modulation sources: Manual / Envelope follower / LFO / Sample-and-hold.
- **Hero:** response curve sweeping live with the modulator drawn as the
  thing moving it; Vowel morphs a mouth diagram.
- **KBS ships this** (`FilterSuite.h`, `Core/SVF.h`). Art: `h147_response`.

### 9.3 Glitch
- **DSP:** 4-second rolling buffer, bar-locked grid capture/replay, 6 modes
  (Stutter, Repeat, Tape Stop, Gate, Reverse, Scatter), 2.5 ms crossfades,
  per-slice pitch/filter/crush. Needs host transport for bar lock.
- **Hero:** the buffer as a filmstrip with the captured slice lit and the
  grid overlaid; Tape Stop draws its rate-decay curve.
- **Publish:** slice origin, playback rate, mode state (structured feed).
- **KBS ships this** (`GlitchSuite.h`). Art: `h146_granular`.

### 9.4 Imaging
- **DSP:** Linkwitz-Riley 3-band split, per-band M/S width, mono-maker below
  a corner, stereo rotation, balance, Haas delay (+/-35 ms), spread.
- **Hero:** goniometer + per-band width fans, with the mono-below corner
  drawn as the point the field collapses.
- **Publish:** downsampled L/R sample pairs for the goniometer (structured
  feed) + per-band correlation.
- **KBS ships the DSP** (`ImagingSuite.h`) and explicitly names the
  goniometer as an unbuilt hero candidate.

### 9.5 Pitch
- **DSP:** two-voice shifter (+/-24 semitones, +/-50 cents, up to 60 ms
  offset) with shared formant correction and dry-path delay compensation.
- **Hero:** keyboard showing dry + both voices at their intervals; formant
  as a separate throat control.
- **Publish:** detected f0 and both target pitches.
- **BSD has the shifters already** (`PitchShifters.h`, `PhaseVocoder.h`,
  `LibraryPitchShifters.cpp`) - this is packaging, not new DSP.
  Art: `h153_keyboard`.

### 9.6 Look-ahead limiter as a distinct device
KBS ships both a feedback limiter (channel strips) and a look-ahead ceiling
limiter (Bus, `Core/Lookahead.h`). BSD ships one. Worth splitting, and the
2.2 hero draws the lookahead window honestly only if the device actually has
one.

---

## 10. Suggested build order

1. **1.1 + 1.2 + 1.5** (the two query contracts and the hero host). Nothing
   good happens before these.
2. **Delay** - art drawn, KBS widget already written, cheapest real win.
3. **Compressor** - the best teaching graphic in audio, and `computeGainDb`
   already exists, so it is mostly drawing.
4. **Character** - biggest visual payoff, DSP portable from KBS.
5. **Gate / Transient Shaper** - both fall out of 1.3 plus the existing feed.
6. **Saturation / Overdrive / the six drive pedals** - one curve widget,
   eight effects.
7. **Reverb room model** - the most DSP work of the "we already have it" set.
8. Everything in section 9, effect by effect.

---

## 11. Cross-product note

BSD and KBS share DSP lineage (the EQ, the synth engine). The query contracts
in section 1 should be spelled the SAME on both sides so a hero widget is
portable. Where one side has the DSP and the other has the art - Delay,
Character, Filter, Glitch, Imaging - port rather than rewrite.
