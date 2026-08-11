# Vocal Chain

**Purpose** - The vocal chain is everything a Vox tab does to your voice in real
time: it tunes it as you sing, gates the room out, dries up the echo, tames the
"sss" sounds, evens out the loud and quiet parts, adds warmth, and stops it
clipping. It is one processor (`BaySickVocalProcessor`) per Vox tab, and it also
carries an A/B compare so you can build two complete vocal sounds and flip
between them.

## How it operates

`BaySickVocalProcessor` (`Source/BaySickVocal/BaySickVocalProcessor.cpp` / `.h`)
is the Vox tab's engine. Signal order inside one audio block:

```
input
  -> realtime pitch correction        (PitchCorrectorDSP, outside the rack)
  -> WET recording tap
  -> live-monitor split               (True Dry / Bypass Corrector / With Effect)
  -> prior-take monitor merge         (when singing over earlier takes)
  -> vocal chain rack, six locked slots:
        0 Gate  1 De-reverb  2 De-esser  3 Compressor  4 Saturation  5 Limiter
  -> embedded NAM/IR stage            (see NAM Amp and Cab.md)
  -> global Mix (dry/wet crossfade)
  -> output
```

- **There is no master bypass.** The chain always runs, so pitch and timing edits
  can never be silenced by switching pages. Each stage has its own bypass.
- **The rack is fixed.** `prepareToPlay` loads the six effect types into slots
  0-5 every time, so slot order and type are never persisted - only each slot
  DSP's own state blob is.
- **Parameters are pushed to the DSP once per block** (`pushApvtsToDsp`), which
  makes the `bsv_*` parameters the source of truth for everything they cover.
  Controls a parameter does not cover write straight to the DSP and persist
  through that DSP's own state blob.
- **During playback of recorded clips** the realtime corrector is force-bypassed
  and the offline pitch applicator runs in its place instead, so a take that was
  recorded with correction already baked in is never corrected twice.
- **Reported latency** for delay compensation is the sum of the non-bypassed rack
  slots plus the NAM/IR stage. The realtime corrector's own latency (about 48 ms)
  is deliberately excluded, because it only exists while you are monitoring and
  folding it in would delay everything else to match.
- **Low-latency monitoring.** With correction on, "With Effect" monitoring hears a
  short time-domain pitch shift of the raw voice (about 12 ms) instead of the
  full-quality corrected stream (about 48 ms). The full-quality stream still
  feeds the recorder, so takes keep the better sound.
- **Noise-profile learners** run while an input is assigned: one listens before
  the corrector and one after, because the corrector changes the noise floor too.
  The audio-thread side is a wait-free push; the analysis runs on a 15 Hz message-
  thread timer.
- **Threads.** `processBlock` is audio thread. Parameter pushes happen at the top
  of it. The A/B slot swap, state restore, and every editor action are message
  thread.

## User-facing behavior

### The BaySickVocals panel (the Vox page itself)

Top half:

| Control | What it does | Range | Default |
|---|---|---|---|
| **Mix** (knob) | Blends the untouched voice against the fully processed chain. Turn it down to keep a vocal more natural. | 0-100 % | 100 % |
| **A/B** (dropdown, A / B) | Two complete saved vocal sounds. Build one in A, switch to B and build another, then flip to compare. Each slot remembers Mix, the whole realtime board, and every chain stage. | A / B | A |

Bottom half - **REALTIME PITCH CORRECTION**. This is the "tune me while I sing"
section; it also runs on recorded clips. For fixing individual notes after the
fact, use BaySickPitch instead.

| Control | What it does | Range | Default |
|---|---|---|---|
| **Realtime Pitch ON / OFF** (button) | Switches realtime correction on. The label shows the current state. | on / off | OFF |
| **Root** (dropdown) | The key's root note. Combined with Scale it defines which notes count as in tune. | C, C#, D, D#, E, F, F#, G, G#, A, A#, B | C |
| **Scale** (dropdown) | Which notes correction pulls toward. Chromatic snaps to the nearest semitone and is the most natural; a named scale keeps you inside a key. | Chromatic, Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Harm. Minor, Mel. Minor, Pentatonic Maj, Pentatonic Min, Blues | Chromatic |
| **Retune ms** (knob) | How fast a note snaps to pitch. Low is the hard, robotic tuned sound; high is transparent. | 0-100 ms | 60 ms |
| **Strength** (knob) | How far toward the target note the voice is pulled. 100 % is a full snap, 0 % passes the voice through untouched. 60-80 % is the natural-sounding zone. | 0-100 % | 80 % |
| **Humanize** (knob) | Adds a small random pitch wobble after correction so the result does not sound mechanical. | 0-20 cents | 0 |
| **Throat** (knob) | Moves the vocal-tract resonance without changing the pitch. Left of center sounds chestier and deeper, right sounds brighter and smaller. | -12 to +12 semitones | 0 |
| **Formant Preserve** (button) | Keeps the voice sounding like the same person while correction shifts pitch. Off gives the classic chipmunk-up / demon-down artifacts. | on / off | off |

A monospace line sits at the bottom of the panel. It is a placeholder - it always
reads `Detected: -- Hz   Target: -- Hz   Shift: -- cents` and does not yet show
live values.

**While the strip is recording**, everything in this section plus the A/B
dropdown is grayed out. Changing them mid-take would both click and print into
the recorded file, so the sound is fixed before the take starts. Mix stays live.

Note: the **Throat** knob's on-screen tooltip says "-100..+100 cents". The
parameter is in **semitones**, -12 to +12, and that is what the DSP applies.

### The Vocal Chain window

Six stacked slots, top to bottom in signal order. They are locked: you cannot
swap, move, or remove a stage. Each slot header carries a bypass LED (click it),
the stage name, a **Preset** menu (save / load / restore / set default / manage,
per stage), and a **Basic / Advanced** toggle on panels that have extra
power-user controls. Compressor, Saturation and Limiter also get a **Mode**
dropdown. A sidechain-source button appears on sidechain-capable stages but its
menu does not open here - the Vocal Chain panel does not install a channel
context for it.

**Slot 0 - Gate.** Silences the gaps between phrases. Fully open by default.

| Control | Range | Default |
|---|---|---|
| Thresh | -80 to 0 dB | -80 dB (open) |
| Range (how much it attenuates when closed) | -80 to 0 dB | -60 dB |
| Attack | 0.1-100 ms | 1 ms |
| Hold | 0-500 ms | 50 ms |
| Release | 5-2000 ms | 100 ms |

**Slot 1 - De-reverb.** Pulls the room off a vocal recorded in a live space.
Adds 2048 samples of latency when active.

| Control | Range | Default |
|---|---|---|
| Reduce (how hard the tail is suppressed) | 0-100 % | 50 % |
| Tail (the modeled room length - match it to your room) | 100-1000 ms | 400 ms |
| Mix | 0-100 % | 100 % |

**Slot 2 - De-esser.** Tames harsh "s" and "t" sounds. The Basic view shows
Detect, Threshold, Range, Mode Blend and Monitor; Advanced reveals the rest.

| Control | Range | Default |
|---|---|---|
| Detect (see the note below) | 0-100 | 50 |
| Threshold | -80 to 0 dB | -24 dB |
| Range | -48 to 0 dB | -12 dB |
| Mode Blend (0 = wide band, 100 = split at 4 kHz) | 0-100 | 0 |
| Frequency (which band it listens to) | 4000-12000 Hz | 6500 Hz |
| Q | 0.5-4.0 | 1.4 |
| Attack | 0.1-30 ms | 1 ms |
| Release | 10-500 ms | 80 ms |
| Lookahead | 0-12 ms | 0 ms |
| Mix | 0-100 % | 100 % |
| M/S (Stereo / Mid / Side) | selector | Stereo |
| Listen (solo just what is being removed) | on / off | off |
| Spectral engine | on / off | off |
| Spectral low latency | on / off | off |

The chain's de-esser starting values are its own: `bsv_deesser_*` opens at
Threshold -24 dB, Range -12 dB and Mode Blend 0, where the same module loaded
into an ordinary effect rack starts at -12.5 dB, -14 dB and 50 (see
*Effect Modules.md*). Same DSP, different defaults.

The spectral engine reports its own latency instead of the lookahead value.
**Listen** (labeled *Monitor* on the panel) solos just what the de-esser is
removing; it is deliberately shared between A/B slots, so flipping A/B never
drops you out of an audition. The Spectral and Quality switches lock while the
transport is running, because they change latency.

Note on **Detect**: it is a macro that sets Frequency and Q together (narrow
detection at 7500 Hz and tight Q, wide at 4000 Hz and broad Q). It moves the Freq
and Q knobs without writing their parameters, and the chain re-applies those
parameters to the DSP every audio block - so on a Vox chain a Detect move does not
hold. Set Frequency and Q directly in the Advanced view instead.

**Slot 3 - Compressor.** Evens out loud and quiet parts. **Mode** offers Modern,
FET (Punchy), Opto (Smooth) and Pedal (Sustain); Modern is the default and is
the one whose panel is wired to this chain's parameters.

Modern panel controls: Threshold (-60 to 0 dB, default -12), Ratio (0.4-30,
default 4), Knee (0-18 dB, default 6), Makeup (-30 to +30 dB, default 0), Attack
(0-400 ms, default 10), Release (1-4000 ms, default 100), Mix (0-100 %, default
100), Lookahead (0-5 ms, default 0), Detection (1-100 ms, default 10), Sidechain
HPF (20-2000 Hz, default 20), plus Auto Makeup (off), Stereo Link (on), Peak
Detection (off) and a Knee Type selector (0-7, default 1).

Caveat, verified in code: the FET, Opto and Pedal panels write straight to the
DSP and are not bound to this chain's `bsv_comp_*` parameters, which the engine
re-applies every audio block. On a Vox chain their knob moves are therefore
overwritten. Modern is the mode to use on a vocal chain.

**Fixed 2026-08-11 (QA-Manuals MF-9):** picking Pedal used to mount the Pedal
panel while the engine pushed the compressor straight back to Opto, because
`bsv_comp_type` spanned Modern / FET / Opto only and the fourth value clamped to
the top of that range. The parameter now spans all four, so the mode you pick is
the mode that plays.

**Slot 4 - Saturation.** Adds warmth and body. **Mode** offers Tube, Console and
Tape; **Console** is the default here. The Console panel's Basic view is Drive
(0-10, default 3), Color (0-10, default 3), Output (-18 to +18 dB, default 0), a
Color on/off switch and a Clean / Dirty voicing selector; Advanced adds a Mix
knob (0-100 %, default 70) and a harmonics-routing selector (Keep Low / Normal /
Keep High). Those controls write straight to the DSP; they persist with the
project through the chain state blob, but they are not `bsv_` parameters, so they
have no automation lanes.

Two things to know about the Saturation slot, both verified in code: the chain
pushes `bsv_sat_harmonicsMode` to the DSP every block, so the **Console** panel's
harmonics-routing selector is re-imposed from that parameter (default Normal)
rather than holding what you set - only the **Tube** panel's harmonics selector is
bound to the parameter and holds. And a **Vocal Body** shaping toggle exists as a
parameter with no on-screen control anywhere; it is reachable only through
automation, and defaults off.

**Slot 5 - Limiter.** Stops the vocal clipping and can push loudness. **Mode**
offers Limiter (Reproduction) and Maximizer (Loudness).

| Control | Range | Default |
|---|---|---|
| Input Gain | -12 to +24 dB | 0 dB |
| Ceiling | -24 to +12 dB | -0.3 dB |
| Sat Threshold | 0-1 | 1 |
| Sat Curve | 0-1 | 0.5 |
| Sidechain HPF | 20-2000 Hz | 20 Hz |
| Attack | 0.1-20 ms | 1 ms |
| Release | 10-1000 ms | 100 ms |
| Lookahead | 0-10 ms | 2 ms |
| Release Curve | 0-1 | 0.5 |
| Sustain | 0-1000 ms | 0 ms |
| Auto Release / Auto Makeup / Stereo Link | on / off | off / off / on |
| Character | 0-7 | 0 |
| Loudness Target + LUFS value | on / off, -30 to 0 LUFS | off, -14 LUFS |
| Auto Ceiling + True Peak target | on / off, -6 to 0 dBTP | off, -1 dBTP |

Every limiter default reproduces the behavior it had before the loudness suite
was added, so an older project loads unchanged.

### A/B compare in practice

Flip the A/B dropdown and the whole vocal sound changes: Mix, the entire realtime
pitch board, and all six chain stages' parameters swap. What does **not** swap:
the A/B selector itself, the de-esser's Listen audition, the Pitch and Align
editors' settings and edits, the panel-only chain controls that have no parameter
behind them, and the NAM/IR stage - which has an independent A/B pair of its own.

Both slots start out holding the current sound, so the first flip is silent until
you have changed one side. An A/B flip driven by an automation lane switches the
sound but does **not** bank the outgoing slot - only a flip you perform by hand
captures, so a replay pass cannot overwrite both of your hand-dialled tones.

## Parameters and persistence

All parameters live on the Vox tab's own APVTS. Prefix `bsv_`; the `bsa_` (Align)
and `bsp_` (Pitch) families share the same tree and are documented in
`Align Editor.md` and `Pitch Editor.md`.

| Parameter | Range | Default |
|---|---|---|
| `bsv_mix` | 0-1 | 1 |
| `bsv_ab_slot` | 0 (A) / 1 (B) | 0 |
| `bsv_pitch_realtime_bypass` | bool (true = correction off) | true |
| `bsv_pitch_key` | 0-11 | 0 |
| `bsv_pitch_scale` | 0-12 | 0 |
| `bsv_pitch_retuneSpeed` | 0-100 ms | 60 |
| `bsv_pitch_strength` | 0-1 | 0.8 |
| `bsv_pitch_formantPreserve` | bool | false |
| `bsv_pitch_humanize` | 0-20 cents | 0 |
| `bsv_pitch_throatShift` | -12 to +12 semitones | 0 |
| `bsv_gate_bypass` / `_threshold` / `_range` / `_attack` / `_hold` / `_release` | bool; -80..0 dB; -80..0 dB; 0.1-100 ms; 0-500 ms; 5-2000 ms | false; -80; -60; 1; 50; 100 |
| `bsv_dereverb_bypass` / `_reduction` / `_tail` / `_mix` | bool; 0-100; 100-1000 ms; 0-100 | false; 50; 400; 100 |
| `bsv_deesser_bypass` / `_msMode` / `_freq` / `_q` / `_threshold` / `_range` / `_attack` / `_release` / `_lookahead` / `_mix` / `_listen` / `_modeBlend` / `_spectral` / `_lowlat` | see the table above | see the table above |
| `bsv_comp_bypass` / `_type` / `_threshold` / `_ratio` / `_attack` / `_release` / `_gain` / `_knee` / `_mix` / `_lookahead` / `_stereoLink` / `_autoMakeup` / `_detection` / `_scHpf` / `_kneeType` / `_peakDet` | `_type` is 0 Modern / 1 FET / 2 Opto; rest as above | `_type` 0 |
| `bsv_sat_bypass` / `_type` / `_vocalBody` / `_harmonicsMode` | `_type` 0 Tube / 1 Console / 2 Tape; `_harmonicsMode` 0 Lo / 1 Normal / 2 Hi | false; 1; false; 1 |
| `bsv_limiter_bypass` and the full limiter set | see the table above | see the table above |

A retired `bsv_bypass` page-master parameter no longer exists; old projects
carrying it load fine and the value is ignored.

**What the engine writes when the project is saved** (`getStateInformation`):

- the APVTS tree;
- `<SlotA>` and `<SlotB>` - the two A/B snapshots, one value per snapshotted
  parameter id. A save always captures the live slot first, so what is on screen
  is what is stored. A project saved before a given parameter existed simply
  loads that parameter at whatever the project loaded with - there is no
  migration;
- `<VocalChainState>` - one base64 blob per chain slot, holding each stage DSP's
  own state. This is what carries the panel-only controls that have no `bsv_`
  parameter (Console saturation's knobs, the FET all-buttons flag, the Opto
  Comp/Limit switch, and so on). Bound controls are re-imposed from the
  parameters by the per-block push, so parameters win for everything they cover;
- `<PitchEdits>`, `<AlignEdits>` - see the editor documents;
- `<NamIrState>` - the embedded NAM/IR processor's whole state.

**Restore order matters.** All four child blocks are pulled out of the tree before
the parameters are replaced, a "restoring" flag is held raised across the whole
restore (and cleared on a queued callback so it outlives every hook the restore
posted), the chain blobs are applied in place on the existing DSP objects, and
`onChainStateRestored` then tells the Vocal Chain panel to re-mount its slot
editors so their knobs show restored values rather than pre-restore ones.

**Not saved:** the live monitor mode (that is a mixer strip parameter - see
`Vox Page.md`), the noise-learner state, the monitor crossfade state, and the
analysis staleness counters, which are re-baselined on load.

## Lifetime and teardown

- One processor per Vox tab, created by `EngineRig` when the tab is created and
  prepared at 44100 / 512 immediately. The page holds a non-owning pointer.
- The embedded NAM/IR processor is constructed in this processor's constructor,
  so a page preset capture picks its state up automatically.
- `prepareToPlay` re-seats the six chain slots (idempotent on type match),
  prepares every stage, sizes all scratch buffers, and prepares the NAM/IR stage.
- Teardown: the owner raises `setShuttingDown(true)` and waits for the audio
  thread to acknowledge two block boundaries before destroying. `processBlock`
  bails out at the top when the flag is set. The destructor sets it again as a
  safety net and drops the lifetime token that queued A/B swaps hold, so a
  message-queue swap arriving after destruction does nothing.
- The Vocal Chain panel clears its `onChainStateRestored` hook in its destructor.

## Cross-references

- `Vox Page.md` - the tab, recording, monitoring, page presets.
- `Pitch Editor.md` - BaySickPitch, the offline note editor whose applicator
  occupies the pitch stage during playback.
- `Align Editor.md` - BaySickAlign.
- `NAM Amp and Cab.md` - the stage that sits after this rack.

## Differs from Carry-Forward

- **The chain is six stages, not four.** Carry-Forward predates the Gate and
  De-reverb stages, which now sit ahead of the de-esser.
- **The page-master Bypass is gone.** It was retired so pitch and align edits
  cannot be silenced by a page switch.
- **The chain now has its own A/B compare** with a real per-slot snapshot of the
  whole chain, separate from the NAM/IR stage's A/B.
- Carry-Forward describes the chain as a set of sub-tabs inside the Vox page; it
  is now a separate window.
