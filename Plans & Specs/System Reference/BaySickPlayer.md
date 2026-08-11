# BaySickPlayer

**Purpose** - BaySickPlayer is the app's sample player: it takes recorded audio
(a single file, a whole folder of files, or an SFZ instrument) and lets you play
it from a keyboard or the piano roll. It is one of the engine choices on Layers,
Bass and Drums tabs, and it is the engine that sits behind every Clips tab. On
top of plain playback it adds pitch, envelope, filter, drive, stereo and unison
controls, so one recorded note can be shaped into a usable instrument.

Internally the source files still carry the old `BaySickPlayer` name
(`Source/BaySickPlayer/`). The user never sees that name; the engine is called
BaySickPlayer everywhere on screen.

---

## How it operates

Three classes do the work, all in `Source/BaySickPlayer/`:

| Class | File | Role |
|---|---|---|
| `BaySickPlayerProcessor` | `BaySickPlayerProcessor.h/.cpp` | The `juce::AudioProcessor` wrapper. Owns the APVTS, the sample-load entry points, and the audition atomics. |
| `BaySickPlayerSynth` / `BaySickPlayerVoice` / `BaySickSampleManager` | `BaySickPlayerDSP.h/.cpp` | The polyphonic engine, its voices, and the sample map. |
| `BaySickPlayerEditor` | `BaySickPlayerEditor.h/.cpp` | The on-screen player window (7 control boxes plus the Preset button). |

**Sample map.** `BaySickSampleManager` holds a flat `std::vector<BaySickPlayerRegion>`.
Each region carries its decoded audio (a `shared_ptr<juce::AudioBuffer<float>>`),
its root note, key range, velocity range, round-robin position, articulation
group, tune/volume offsets and the SFZ keyswitch fields (`sw_lokey`, `sw_hikey`,
`sw_last`, `sw_down`, `sw_up`, `sw_default`, `sw_label`).

- `loadFolder` reads every `.wav` / `.aiff` / `.aif` / `.flac` directly inside the
  folder (not recursive), one region per file, root note guessed from the
  filename by `detectRootNote`.
- `loadSingleFile` makes exactly one region with root note 60.
- `loadSFZ` runs `parseSFZ`, a four-level SFZ v1 parser
  (`<global>` / `<master>` / `<group>` / `<region>`, with `<control>` handled
  separately for `default_path`).

Every file is decoded to memory at load time, capped at **60 seconds** per file,
and mono files are duplicated to stereo. There is no streaming.

**Voices.** `BaySickPlayerSynth` wraps a `juce::Synthesiser` with a physical pool of
**24** `BaySickPlayerVoice` objects (`kMaxVoices`). The user-facing polyphony limit is
lower (see Voice Cap below, default 16) - the extra 8 are a landing zone so a
stolen voice can finish a ~1.5 ms fade while the new note starts on a free slot.
Each voice owns a forward reader and a reverse reader plus one resampler each, so
a note-on allocates nothing on the audio thread.

**Per-voice signal path** (`BaySickPlayerVoice::renderNextBlock`):

```
region sample -> resampler (pitch, stretch, glide, vibrato)
             -> amp ADSR
             -> drive (tanh)
             -> sample-rate reduction (hold-and-repeat)
             -> state-variable low-pass filter
             -> volume x velocity scale x pan  -> mix into the output buffer
```

**Per-engine post-processing** (`BaySickPlayerSynth::renderNextBlock`, after all voices
are summed): a one-pole high shelf at about 8 kHz (Treble), then mid/side stereo
width (Stereo).
**Vibrato is pitch, not volume.** The LFO is folded into the resampler's read
ratio, never applied to the sample's amplitude. `BaySickPlayerVoice::renderNextBlock`
sums the glide offset and the LFO offset in semitone space and turns the sum
into one multiplicative ratio (`mGlideBaseRatio * pow (2, semis / 12)`), stepped
once per `mPitchModChunk` - about 1.3 ms of the live device rate, clamped to
16..512 samples - so the wobble has the same shape at 44.1 kHz and at 192 kHz.
Full depth is `kVibratoMaxCents = 50.0`: plus or minus 50 cents around the note,
a semitone peak to peak. When neither glide nor vibrato is running the voice
takes a plain unmodulated read, and the modulated branch parks the resampler
back on `mGlideBaseRatio` on the way out so a block that ends mid-wobble cannot
strand the voice detuned.

**Threads.**

- Audio thread: everything in `processBlock`, `BaySickPlayerSynth`, `BaySickPlayerVoice`, and
  `BaySickSampleManager::findRegion` / the keyswitch state handlers.
- Message thread: the three loaders, `clear()`, and `normalizeRootNotes`.
- `BaySickSampleManager` carries **no lock**. Every mutator rebuilds `mRegions`
  in place, which frees memory the audio thread may be indexing. Safety comes
  from `BaySickPlayerProcessor::loadIntoManager`, which raises the host's
  processBlock shield (`setProjectLoadInProgress(true)` + `settleAudioThread()`)
  around every load and lowers it after. A new mutation path that skips that
  bracket is a use-after-free, not a glitch.
- Parameter reads are skipped entirely on blocks where nothing changed:
  `processBlock` calls `updateFromApvts()` only when `ApvtsDirtyTracker`
  reports a change since the last block.

**Keyswitch filtering.** Before the MIDI buffer reaches the synth, `processBlock`
walks it once. Any note-on/note-off whose note is a keyswitch (per the loaded
SFZ) is routed to `handleKeyswitchNoteOn/Off` and **stripped** from the buffer,
so keyswitches never steal a voice. With no SFZ loaded, nothing is a keyswitch
and the buffer passes through unchanged.

**Drum root-note normalization.** `detectRootNote` reads note names *and* bare
numbers out of the filename, so `Kick_01.wav` would be read as MIDI note 1 and
then play about 30x too fast when triggered at note 60. Drum tabs therefore pass
`normalizeRoot = 60` into `loadSampleFolder` / `loadSampleSFZ`, which calls
`normalizeRootNotes(60)` and forces every region's root to 60. Because voice
pitch is `midiNote - rootNote`, a drum then plays at its recorded pitch on the
default trigger note. `loadSingleFile` already sets root 60, so the single-file
path needs no normalization. Melodic tabs (Layers, Bass, Clips) pass `-1`
(no normalization) so filename-detected pitches are honored.

**Timeline audio clips.** Audio clips placed on the arrangement do not go
through the voices. `readClipCtl` in `Source/PluginProcessor.cpp` reads the
Clips tab's BaySickPlayer parameter atoms once per row per block and applies the
equivalent shaping (volume, pan, filter, drive, reduct, treble, width,
ADSR, pitch, reverse, sample start, stretch) directly to the decoded clip audio.
So the player window on a Clips tab also shapes what you hear from the timeline.

**Vibrato on a timeline clip.** The two Vibrato knobs are the one exception to
that chain. They are pitch, so `renderAudioClipsForRow` folds them into the
clip's **read position** instead of into the post-decode chain, on the same
full-depth law as the voices (`kClipVibratoMaxCents = 50.0`, commented in source
as equal to `BaySickPlayerVoice::kVibratoMaxCents`). All three read branches
carry it: the phase-vocoder branch (which runs when the clip is time-stretched
to the project tempo or when Tune / Detune is off zero - the Stretch knob is a
varispeed and does not engage it), the plain reverse branch, and the plain
forward branch. They share one per-clip LFO phase
(`AudioClipPlayer::clipLfoPhase`) advanced by output samples, so they cannot
drift apart. The rate deviation is made zero-mean before it is applied, because
the average of `2^(depth * sine)` is greater than 1 and an uncorrected wobble
would let the read creep permanently ahead of the timeline. On the two plain
branches, turning the depth back to zero walks the carried position offset home
at a bounded rate (`kClipVibGlideRatio`) rather than snapping it, which is what
keeps the knob from clicking on the way down; the phase-vocoder branch takes the
modulation as a stream and carries no offset, so it needs no walk. FilePlay
clips (audio blocks routed to a Vox or Inst page) are decoded on a different
path and get **no** vibrato.

---

## User-facing behavior

### Where you meet it

Add a Layers, Bass or Drums tab and pick **BaySickPlayer** as the engine, or add
a Clips tab (which always uses it). The window is titled **BaySickPlayer** and
has a warm gold accent color.

### Loading a sound

The **Preset** button on the window's title strip opens one menu that covers
both sounds and settings.

| Menu item | What it does |
|---|---|
| **Open Folder...** | Pick a folder; every audio file directly inside it becomes one playable zone (the search is not recursive). All three file pickers open in the Core Library, or in your Music folder if the Core Library is not installed. |
| **Open SFZ...** | Pick a `.sfz` instrument. Key ranges, velocity layers, round robins and keyswitches all come from the file. |
| **Open Sample...** | Pick one audio file (`.wav`, `.aif`, `.aiff`, `.flac`, `.ogg`, `.mp3`) and play it chromatically from middle C. |
| **Core Library** submenu | Browse the installed sample library without a file dialog. Folders that directly contain audio load as a folder; `.sfz` files are marked `[SFZ]`. On a melodic tab the drum packs are hidden; a drum slot hides this submenu entirely because the drum tab's own picker handles it. |
| **Presets** section | Factory and user presets, in cascading submenus. Melodic tabs see the melodic folders, drum tabs see the drum folders, and **My Presets** always shows. Loading a preset also reloads the sound it was saved with. |
| **Save preset...** | Names and saves the current settings plus a reference to the loaded sound into **My Presets**. |

If a load produces nothing playable (a damaged file, or an SFZ whose samples are
missing) a warning box names the path instead of leaving you with a renamed tab
that plays silence.

### The player window

Seven labeled boxes. Every knob shows its value in a bubble while you hover or
drag, **double-click resets it to its factory default**, and **right-click**
offers *Automate*, *Type in value...* and the MIDI Learn items.

**SAMPLE ENGINE**

| Control | Range / default | What it does |
|---|---|---|
| SMPL START | 0 to 1, default 0 | Skips into the sample before it starts playing. Turn it up to cut off a soft attack or find the body of a note. |
| STRETCH | 0.5 to 2.0, default 1.0 | Tape-style speed change. Below 1 is slower and lower, above 1 is faster and higher. It changes pitch and length together. |
| REVERSE | off/on, default off | Plays samples backwards. |
| CUT SELF | off/on, default off | When on, a new note cuts what is already sounding. |
| SAME PITCH / CUT ALL | default CUT ALL | Sets what "Cut Self" cuts. **Same Pitch** only cuts the note you just retriggered. **Cut All** silences everything on each new note - that is the drum-style behavior and it is this engine's default. |

**PITCH & VOICING**

| Control | Range / default | What it does |
|---|---|---|
| TUNE | -24 to +24 semitones, default 0 | Transposes the whole instrument. |
| VOICE CAP | 1 to 16, default 16 | How many notes can sound at once. Set it to 1 for a monophonic bass line. When the cap is reached the oldest fading note is taken first, then a note whose key is already released, and only last a key you are still holding. |
| UNISON | 1 to 8, default 1 | Stacks copies of every note for a thicker sound. Costs one voice per copy. |
| DETUNE | -100 to +100 cents, default 0 | Pitch offset applied to the stacked copies. |
| SPREAD | 0 to 100 cents, default 0 | Spreads the stacked copies symmetrically around the note. |
| DET MODE | S / R / P, default S | How Detune is shared out. **S**imple gives every copy the same offset, **R**andom gives each a random offset, **P**air spreads them evenly across plus and minus. |

**DYNAMICS** - the two arrows drawn between the columns mean "velocity feeds
this knob".

| Control | Range / default | What it does |
|---|---|---|
| SENS | 0 to 1, default 0.5 | How strongly playing harder changes the sound. Low is flat and even, high is very responsive. |
| VEL>MASTER | 0 to 1, default 1.0 | How much velocity controls loudness. At 0 every note is the same volume. |
| VEL>MUFFLE | 0 to 1, default 0 | Lets harder hits escape the Muffle darkening. |
| MUFFLE | 0 to 1, default 0 | Darkens the sound by pulling the filter down toward 200 Hz. |
| VEL>HARD | 0 to 1, default 0 | Lets harder hits sound harder (more resonant). |
| HARDNESS | 0 to 1, default 0 | Adds filter resonance - a sharper, more nasal edge. |

**AMP ENVELOPE**

| Control | Range / default | What it does |
|---|---|---|
| ATTACK | 0.001 to 10 s, default 0.001 s | How long the note takes to reach full volume. |
| DECAY | 0.001 to 10 s, default 0.5 s | How long it takes to fall from full volume to the sustain level. |
| SUSTAIN | 0 to 1, default 1.0 | The level the note holds at while the key is down. |
| RELEASE | 0.001 to 10 s, default 0.3 s | How long the note takes to fade after you let go. |

**LFO**

| Control | Range / default | What it does |
|---|---|---|
| VIB RATE | 0.1 to 20 Hz, default 5.5 Hz | How fast the pitch wobbles. |
| VIB DEPTH | 0 to 1, default 0 | How far the pitch wobbles. This is **pitch** vibrato, not a volume wobble. At full depth the pitch swings plus or minus 50 cents - half a semitone each way, a full semitone peak to peak. At 0 (and anywhere at or below 0.001) vibrato is off and the sample reads unmodulated. On a note you play the wobble restarts from zero at every note-on, so each note starts in tune and then moves. |

**FILTER**

| Control | Range / default | What it does |
|---|---|---|
| CUTOFF | 20 Hz to 20 kHz, default 20 kHz (fully open) | Low-pass filter. Turn it down to make the sound darker and duller. |
| RES | 0 to 1, default 0 | Emphasis right at the cutoff point. High values whistle. |
| REDUCT | 0 to 1, default 0 | Lo-fi sample-rate crushing (holds each sample for 1 to 16 steps). 0 is clean, 1 is heavy digital grit. |

**OUTPUT**

| Control | Range / default | What it does |
|---|---|---|
| PAN | -1 (left) to +1 (right), default 0 | Position in the stereo field. |
| STEREO | -1 to +1, default 0 | Width. -1 collapses to mono, 0 is the sample's natural width, +1 is twice as wide. |
| OVERDRIVE | 0 to 1, default 0 | Soft saturation. A little adds warmth, a lot adds fuzz. |
| TREBLE | -12 to +12, default 0 | Shelf around 8 kHz. Negative removes air, positive adds sparkle. |
| MASTER VOL | 0 to 1, default 0.8 | Overall engine level (drawn as the white volume knob). |

### Playing it

- Clicking a key on the piano-roll keyboard auditions the sound; click-and-hold
  sustains and release stops it.
- Notes from the piano roll can carry per-note expression that this engine
  understands: pan (CC10), brightness (CC74), resonance (CC71), release scale
  (CC72), and the slide/portamento transport (CC84 source note, CC5+CC37 glide
  time, CC85 target, CC86 target loudness, CC89 target pan).
- On an SFZ instrument, keyswitch keys are highlighted on the roll keyboard with
  their names, taken from the file's `sw_label` entries.
- The tab, its mixer strip and the piano-roll label are all renamed to the sound
  you loaded.

---

## Parameters and persistence

Every parameter id is `tk_<trackId>_bsp_<name>`, where `<trackId>` is the tab's
own id: `lay_<n>`, `bas_<n>`, `drm_<n>`, or `clip_<n>_`. Clips ids therefore
contain a doubled underscore (`tk_clip_0__bsp_volume`), because the Clips track
id already ends in one.

| Parameter (suffix) | Type | Range | Default | Screen label |
|---|---|---|---|---|
| `sampleStart` | float | 0 .. 1 | 0 | SMPL START |
| `stretch` | float | 0.5 .. 2.0 | 1.0 | STRETCH |
| `reverse` | bool | - | false | REVERSE |
| `cutSelf` | bool | - | false | CUT SELF |
| `cutSelfMode` | bool | - | **true** (Cut All) | SAME PITCH / CUT ALL |
| `tune` | float | -24 .. +24 semitones | 0 | TUNE |
| `voiceCap` | int | 1 .. 16 | 16 | VOICE CAP |
| `unisonVoices` | int | 1 .. 8 | 1 | UNISON |
| `detune` | float | -100 .. +100 cents | 0 | DETUNE |
| `unisonSpread` | float | 0 .. 100 cents | 0 | SPREAD |
| `detuneMode` | int | 0 .. 2 (simple / random / pair) | 0 | DET MODE |
| `sensitivity` | float | 0 .. 1 | 0.5 | SENS |
| `velToVolume` | float | 0 .. 1 | 1.0 | VEL>MASTER |
| `velToMuffle` | float | 0 .. 1 | 0 | VEL>MUFFLE |
| `muffle` | float | 0 .. 1 | 0 | MUFFLE |
| `velToHardness` | float | 0 .. 1 | 0 | VEL>HARD |
| `hardness` | float | 0 .. 1 | 0 | HARDNESS |
| `attack` | float | 0.001 .. 10 s (skewed) | 0.001 | ATTACK |
| `decay` | float | 0.001 .. 10 s (skewed) | 0.5 | DECAY |
| `sustain` | float | 0 .. 1 | 1.0 | SUSTAIN |
| `release` | float | 0.001 .. 10 s (skewed) | 0.3 | RELEASE |
| `lfo_rate` | float | 0.1 .. 20 Hz (skewed) | 5.5 | VIB RATE |
| `lfoAmt` | float | 0 .. 1 | 0 | VIB DEPTH |
| `cutoff` | float | 20 .. 20000 Hz (skewed) | 20000 | CUTOFF |
| `res` | float | 0 .. 1 | 0 | RES |
| `reduct` | float | 0 .. 1 | 0 | REDUCT |
| `pan` | float | -1 .. +1 | 0 | PAN |
| `stereo` | float | -1 .. +1 | 0 | STEREO |
| `drive` | float | 0 .. 1 | 0 | OVERDRIVE |
| `treble` | float | -12 .. +12 | 0 | TREBLE |
| `volume` | float | 0 .. 1 | 0.8 | MASTER VOL |
| `artic_group` | int | 0 .. 3 | 0 | **no knob** - see below |

`artic_group` has no control on screen. The bundled Core Library ships no SFZ
that uses `group=`, so any non-zero value silences the engine. The parameter is
kept so an automation lane or an old preset that carries a non-zero value still
applies.

`lfo_rate` and `lfoAmt` keep their old spellings even though the knobs now read
VIB RATE and VIB DEPTH and the host-facing parameter names are **Vibrato Rate**
and **Vibrato Depth**. The ids are saved-project keys, automation-lane keys and
the keys the timeline clip path resolves its control atoms by, so renaming them
would break existing songs. One consequence: the right-click **Automate** menu
and the automation lane list build their labels from the id, not from the
parameter name, so they read *Lfo Rate* and *LfoAmt*.

**Saved with the project.** The whole APVTS state, plus three non-parameter
properties stamped onto `apvts.state` by the loaders:

| Property | Value |
|---|---|
| `bsp_loadKind` | `"folder"`, `"sfz"` or `"file"` |
| `bsp_loadPath` | The sound's path, written through `SampleLibrary::refForPersist` - a `library:` or `mysamples:` reference when the file lives under Core Library or My Samples, absolute otherwise |
| `bsp_loadNormalize` | The MIDI root the load normalized to (60 on drum tabs, -1 elsewhere) |

`setStateInformation` replaces the state and then replays the load by kind,
resolving the path through `ProjectFileResolver::resolve`. Two failure cases are
reported to the missing-files dialog: a path that does not resolve (listed as
"Sample folder" / "SFZ instrument" / "Sample file" with the stored reference),
and a path that resolves but yields no regions (listed as "BaySickPlayer sound").
The stored path property is **not** cleared on failure, so a display that reads
it can still show a name for a sound that did not load.

**Saved with a preset.** `Documents\BaySickDAW\Presets\BaySickPlayer\My Presets\<name>.xml`,
in a nested shape: a `<BaySickPlayerState>` wrapper holding the APVTS state child
plus a `<Sample kind= path=>` child. Loading rewrites the parameter-id prefix, so
a patch saved on `tk_lay_0_bsp_` loads cleanly onto `tk_bas_0_bsp_`. If the
preset's sound is missing on this machine, the settings still apply and a box
says the preset will not make sound.

**Per machine, not per project.** The Core Library location itself
(`%LOCALAPPDATA%\BaySickDAW\CoreLibrary`) and the preset folder under
`Documents\BaySickDAW`.

**Not saved at all.** Round-robin counters, live keyswitch state, voice
allocation, and the audition atomics - all rebuilt from scratch on load.

---

## Lifetime and teardown

- The engine is **model-owned**. `EngineRig::createEngineFor` constructs a
  `BaySickPlayerProcessor` for a Layers / Bass / Drums / Clips tab when that tab's
  engine type is set to `"BaySickPlayer"`, prepares it at the current sample rate
  (or 44100) with a 512-sample block, and tags its APVTS with a stable undo
  identity `rig:<kind>:<pageIndex>` so undo survives engine re-creation.
- The page is a **view**. Closing a page window destroys the editor only; the
  engine keeps running and keeps making sound.
- `BaySickDAWProcessor::bindSampleLoadShield` hands the engine a pointer to the
  host processor (`setHostProcessor`) at creation, before any page can issue a
  load. Without it, sample loads run unprotected against a live render - see the
  shield note under *How it operates*.
- Playing voices hold a `shared_ptr` to their region's audio buffer, so a reload
  during playback cannot free audio out from under a sounding note.
- The editor removes its `apvts.state` listener and clears its look-and-feel in
  its destructor; the engine outlives it.

---

## Cross-references

- `BaySickGuitars.md`, `BaySickBasses.md`, `BaySickRustyDrums.md` - the three
  sfizz-backed sample engines. They load kits from the Core Library instead of
  loose files, and their controls come from the kit author rather than from us.
- The Core Library and the stable-reference path format are shared with all four
  (`Source/SampleLibrary.h`).
- Layers, Bass, Drums and Clips pages host this engine; the Drums page is the
  one that passes `normalizeRoot = 60`.
- Missing-file reporting is shared app-wide (`Source/MissingFileReport.h`).

---

## Differs from Carry-Forward

Carry-Forward's "Engine audition pattern" row states that **4** engine
processors carry `auditionNote(int)`. There are now **7**: the four it lists
(BaySickSynth, BaySickBass, Harmless, BaySickPlayer) plus BaySickGuitars,
BaySickBasses and BaySickRustyDrums, whose signature takes a velocity as well
(`auditionNote(int midiNote, int velocity = 100)`). BaySickPlayer additionally
has the hold pair `auditionNoteOn` / `auditionNoteOff`, where the "off" side
accumulates into a bitmask so a fast drag across the keyboard cannot drop a
note-off and leave a note stuck on.
