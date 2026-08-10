# Harmless

**Purpose** - Harmless is the app's additive synthesizer. Instead of starting from a raw
waveform and filtering it, it builds every sound out of 516 individual harmonics, reshapes
that harmonic stack with a set of spectral tools, and only then runs it through ordinary
filters and envelopes. It carries two independent sound layers, Part A and Part B, which
always play together, and a per-destination modulation matrix where you draw the shape of
each modulation by hand. It is one of the players you can load on a Layers tab or a Bass
tab, and it is the widest control surface in the app.

---

## How it operates

| Layer | Files | Job |
|---|---|---|
| Processor | `Source/Harmless/HarmlessProcessor.h` / `.cpp` | Owns the APVTS (102 parameters) and the modulation registry. Reads parameters once per block and pushes changes into the synth. |
| Synth | `Source/Harmless/HarmlessSynth.h` / `.cpp` | Owns two `HarmonicEngine` templates (Part A and Part B), 16 `AdditiveVoice` objects in a `BroadcastSynthesiser`, the output phaser, the output tilt EQ, the strum staggerer, and the background wavetable-rebuild thread. |
| Voice | `Source/Harmless/AdditiveVoice.h` / `.cpp` | Per-note DSP: up to 9 unison slots reading both parts' wavetables, two filters with their own envelopes, tremolo, vibrato, glide, sub oscillator, amp envelope, per-note pan. |
| Harmonic engine | `Source/Harmless/HarmonicEngine.h` / `.cpp`, `SpectralModules.h` | Holds 516 partial amplitudes and phases; applies the spectral modules; runs a 2048-point inverse FFT into a double-buffered wavetable. |
| Mod matrix | `Source/Harmless/HarmlessModRegistry.h` / `.cpp` | 16 modulation targets x 7 sources, each with a hand-drawn curve plus depth, length and four warp knobs. |
| Editor | `Source/Harmless/HarmlessEditor.h` / `.cpp` | The panel, plus `HarmlessFilterRow`, `HarmlessRoutingMatrix`, `HarmlessXYZPad`, `HarmlessModEditor`, `HarmlessWaveformButton`, `VisualizerScreen` and `HarmlessLAF.h` (accent `#FF6600`). |

**How a sound is made.** Each part is a `HarmonicEngine` holding 516 partial amplitudes.
Picking a shape (Sine / Saw / Square / Triangle) fills those amplitudes with the classic
harmonic series for that shape. The spectral modules - Brownian rolloff, Prism, Pluck,
Blur, Filter Mask, Phaser Mask - then rewrite the amplitudes and phases, and a 2048-point
inverse FFT bakes the result into a single-cycle wavetable. The wavetable is double
buffered and published atomically, so voices always read a complete table. Rebuilds
requested from the UI run on a background `TimeSliceThread`, not on the audio thread.

**Part A and Part B are simultaneous layers, not modes.** Both always render. Every voice
reads both wavetables and mixes them by the per-part levels. `timbre_blend` crossfades
between them. `part_sel` - the A / B buttons in the TIMBRE box - is **editor view state
that no DSP reads**: it only decides which part the shared spectral knobs are currently
editing. Because both parts really play, each part's parameters own an independent
automation lane.

**Per-voice signal path** (`AdditiveVoice::renderNextBlock`): glide toward the target
frequency, vibrato, pitch offset (semitones + cents + the pitch fraction multiplier),
unison slot phases advanced through both wavetables, Part A and Part B mixed by level x
velocity x blend, sub oscillator added, filter 1, filter 2 (each with its own envelope,
keyboard tracking and cutoff offset), tremolo, amp envelope (scaled by the routing
matrix's ENV control), per-note pan, stereo out. After the synthesiser returns,
`HarmlessSynth` applies the output phaser, the tilt EQ, and the routing matrix's output
gain and saturation. The processor then replaces any non-finite sample with zero.

**Audio-thread care.** `updateFromApvts` is gated by an `ApvtsDirtyTracker` so the ~100
parameter reads only happen on blocks where something changed, and it assembles parameter
ids into a stack buffer rather than concatenating `juce::String` (which would allocate on
the audio thread). The processor pre-warms its parameter cache in the constructor so the
very first audio block does not fire a dozen FFT rebuilds.

**Note auditioning.** Piano-roll key clicks reach the engine through `auditionNote` /
`auditionNoteOn` / `auditionNoteOff`, plain atomics drained at the top of `processBlock`.
Note-offs accumulate in a 128-bit mask so a fast drag across the keyboard cannot drop one
and leave a note ringing.

**Modulation.** Each of the 16 targets holds 7 source slots. A source carries a curve
(a list of points you drew), a bipolar depth, a length, a tempo-sync flag, and an LFO
shape. Voices cache pointers to the target structs at note-on and re-read the generation
counter once per block, so an edit in the mod editor lands on already-sounding notes at
the next block boundary. The editor takes a spin lock only when it adds or removes a
curve point; everything else is a leaf write.

---

## User-facing behavior

### Where it lives

Pick Harmless when you create the tab, from the ribbon's add menu (`+ Add Harmless`, on
the Layers or Bass row). **The choice locks** - a tab keeps the player it was created
with.

The panel opens in its own window. The window's title strip shows the engine name in
orange and carries the **Preset** button. The window will not shrink below 1047 x 455
pixels - it is a wide, short panel, and everything is on screen at once. There are no
tabs to switch between.

Every knob shows its value in a pop-up while you drag it, and hovering shows a tooltip
naming the parameter and its units. **Double-click any knob to return it to its factory
default.** Right-click a knob for the app-wide Automate and Type-in-value menu; knobs that
have a modulation target also offer **Modulate envelope...**, which jumps the mod editor
straight to that destination.

### Reading the panel

The window is split into a top band and a bottom band.

Top band, left to right:

| Box | Contents |
|---|---|
| OUTPUT | VOL, PAN, VEL, CUT SELF, SAME PITCH / CUT ALL, AG |
| TREMOLO | WAVE, DEPTH, SPEED, GAP |
| ROUTING | SUB, PROT, CLIP, FX, VOL, ENV |
| VIBRATO / LEGATO | WAVE, DEPTH, SPEED, ENV, GLIDE, LIMIT, LEGATO |
| UNISON | VOICES, TYPE, ALT, PAN, PITCH, PHASE |
| FILTER 1 + ADSR | type, ENV, FREQ, RES, KB, and ATK / DEC / SUS / REL |
| FILTER 2 + ADSR | the same set for the second filter |
| TIMBRE | A / B, PART A, PART B, MIX, VOICE A, VOICE B, BROWN, F1 OFS, F2 OFS, A MASK, B MASK |

Bottom band, left to right:

| Box | Contents |
|---|---|
| PITCH | FREQ, DETUNE, FRAC, OCT, Hz |
| LFO MOD | RATE, SHAPE, TEMPO, VEL, VOL, PITCH |
| STRUM | DIR, TIME, TNS |
| FX - PLUCK / PHASER / EQ | PLUCK, BLUR, MIX, DEPTH, RATE, WIDTH, OFS, MASK, EQ |
| AMP ENV / PHASE | ATK, DEC, SUS, REL, START, RAND |
| BLUR / PRISM | BLUR, TIME, HARM, PRISM, MODE |
| (unlabeled pad) | The XYZ modulation pad and its three knobs |
| SPECTROGRAM | A live display of every audible harmonic |
| (unlabeled, right-hand third) | The modulation editor |

### The A / B buttons - read this first

The two small **A** and **B** buttons at the left of the TIMBRE box choose **which layer
the shared spectral knobs are editing**. Both layers always play; the buttons do not mute
or switch anything.

Nine controls are shared this way. With **A** selected they edit Part A's copy; with
**B** selected they edit Part B's:

BROWN, BLUR, TIME, HARM, PRISM, MODE, PLUCK, MASK (the phaser mask in the FX box), and the
FX box's BLUR toggle.

Everything else on the panel is a single control that applies to the whole instrument.
The exceptions worth knowing are **PART A** / **PART B** (the two waveform icons, one per
layer), **VOICE A** / **VOICE B** (one level knob per layer), and **A MASK** / **B MASK**
(one timbre filter mask per layer) - those four pairs are always both visible, so you do
not need the A / B buttons to reach them.

### TIMBRE

**PART A** and **PART B** (waveform icons). Click to cycle the layer's starting harmonic
shape: sine, saw, square, triangle. Sine is a single pure tone; saw is bright and buzzy;
square is hollow and woody; triangle is soft and flute-like. Part A starts as **saw**,
Part B starts as **square**.

**MIX** (0 to 1, default 0). Crossfades from Part A toward Part B. At 0 you hear the two
layers exactly at the levels their own knobs set. As you turn it up, Part A fades out and
its level is handed to Part B. **Interaction to know:** moving MIX recalculates both
layers' working levels from VOICE A and VOICE B. Moving VOICE A or VOICE B afterwards
re-sets those working levels from the knobs alone, which drops the MIX offset until you
touch MIX again. Set your levels first, then set MIX.

**VOICE A** (0 to 1, default 1.0) and **VOICE B** (0 to 1, default 0.0). The level of each
layer. Out of the box only Part A is audible; raise VOICE B to bring the second layer in.

**BROWN** (0 to 1, default 1.0, per-part). Brownian rolloff - how fast the harmonics get
quieter as they go up. At 1 the stack rolls off about 6 dB per octave, which is why
Harmless sounds warm and natural on first load. At 0 no rolloff is applied and every
harmonic keeps its full amplitude, which is much brighter and much harsher.

**F1 OFS** and **F2 OFS** (-24 to +24 semitones, default 0). Shift filter 1's and
filter 2's cutoff up or down relative to the note being played. Used together with the KB
knob on each filter row.

**A MASK** and **B MASK** (20 Hz to 20 kHz, default 5000 Hz, one per layer). A ceiling on
which harmonics that layer is allowed to produce. Lower it to remove the top of the
harmonic stack before it is ever baked into the wavetable - a much cleaner way to darken
an additive sound than closing a filter afterwards. At 20 kHz nothing is masked.

### OUTPUT

**VOL** (0 to 1, default 0.8) - the engine's master level, before the mixer strip.
**PAN** (-1 to +1, default center) - stereo position; the knob draws from the center out.

**VEL** (toggle, default off). Velocity always scales Part A. With VEL **on**, velocity
scales Part B as well. With it **off**, Part B ignores velocity and holds a constant
level - useful when Part B is an underlying pad you want steady while Part A responds to
your playing.

**CUT SELF** (toggle, default off) and its mode button (**SAME PITCH** / **CUT ALL**,
default Same Pitch). With Cut Self on, each new note performs an instant, click-free hard
cut first: Same Pitch cuts only a still-ringing copy of the same note, which stops the
doubling and phase-smearing you get from fast retriggers; Cut All cuts every ringing voice
on each new note, which gives a choked, gated feel. Harmless is always polyphonic, so this
is the only way to get a cut-off behavior out of it.

**AG** (toggle, caption reads **AG: REL** or **AG: ABS**, default REL). Auto-gain, which
decides what happens when the 516 harmonics add up to more than the engine can output.
**REL** scales every harmonic down by the same factor, keeping the tone's balance intact
but dropping the level. **ABS** caps each harmonic on its own, keeping individual
amplitudes but changing the balance at extremes. REL is the safe default; ABS is a
character choice.

### TREMOLO

Wobbles the volume.

- **WAVE** (icon, click to cycle). Picks the wobble shape.
- **DEPTH** (0 to 1, default 0). How much the volume moves. At 0 tremolo is off.
- **SPEED** (0.1 to 20 Hz, default 3 Hz). How fast.
- **GAP** (0 to 1, default 0). A dead zone around the middle of the wave. Raise it and the
  wobble stops being a smooth swell and starts behaving like an on/off chop.

**Known mismatch (verified in code).** The WAVE icon draws its four positions in the order
sine, saw, square, triangle, but the shapes the tremolo and vibrato actually play for
those four positions are sine, **triangle**, **saw**, **square**. Position 1 is correct;
positions 2, 3 and 4 draw a different wave from the one you hear. Judge these by ear, not
by the picture.

### VIBRATO / LEGATO

Vibrato wobbles the pitch.

- **WAVE** (icon). Same four positions, same mismatch as tremolo above.
- **DEPTH** (0 to 2 semitones, default 0). How far the pitch moves. At 0 vibrato is off.
- **SPEED** (0.1 to 20 Hz, default 5 Hz).
- **ENV** (0 to 1, default 0). Onset delay. At 0 the vibrato is there from the instant the
  note starts; turn it up and the vibrato fades in over the note, which is how a singer or
  a string player actually does it.

Legato:

- **GLIDE** (0 to 2 seconds, default 0). How long a new note takes to slide into place from
  the note before it.
- **LIMIT** (0.001 to 1 second, default 0.5). A ceiling on GLIDE. The glide the engine
  actually uses is the smaller of the two, so LIMIT lets you keep a long GLIDE setting
  without long slides.
- **LEGATO** (toggle, default off). When on, a note that arrives while another is still
  held does not restart the envelopes - it slides and continues.

### ROUTING

Six knobs that shape the output stage and the harmonic build. They are not a mixer;
each one does a specific job.

| Knob | Range | Default | What it does |
|---|---|---|---|
| SUB | 0 - 1 | 0 | Adds an extra sine one octave below every note. Instant weight underneath the sound. |
| PROT | 0 - 1 | 0 | Rolls off the highest harmonics before the wavetable is built. Raise it if very high notes sound gritty or aliased. |
| CLIP | 0 - 1 | 0 | Output saturation. Gently rounds the peaks; at high settings it audibly distorts. |
| FX | 0 - 1 | 1 | A master amount for the spectral modules (Prism, Pluck, Blur and friends). At 0 they are bypassed; at 1 they act at their knob settings. |
| VOL | 0 - 1 | 1 | Output gain trim. **Behaves unusually:** at exactly 1.0 the trim stage is skipped entirely and the signal passes at unity. At any other value the gain applied is the knob value times 1.5 - so 0.667 is also unity, below that gets quieter down to silence at 0, and just under 1.0 is about 3.5 dB **louder** than 1.0. Expect a jump if you nudge it down from the top. |
| ENV | 0 - 1 | 1 | How much the amp envelope shapes the note. At 1 the envelope is fully in charge; at 0 the note plays at a constant level (the envelope still decides when the voice is alive, it just stops changing the volume). |

### UNISON

Stacks detuned copies of the note inside a single voice.

- **VOICES** (1 to 9, default 1). 1 means unison is off.
- **TYPE** (a four-position selector, default Pure). **Pure** spreads the copies evenly
  from flat to sharp. **Random** gives each copy a random offset. **Drifting** is Pure with
  an extra static per-slot wander, which sounds more alive. **Alt-only** alternates the
  copies sharp and flat with nothing in the middle.
- **ALT** (toggle, default off). Flips the sign of every other copy on top of whatever TYPE
  is doing - another way to open up the stack.
- **PAN** (0 to 1, default 0.7). How far the copies are spread across the stereo field.
- **PITCH** (0 to 100 cents, default 15). How far apart the copies are tuned. Small values
  give slow beating and thickness; large values give a chorused, out-of-tune wash.
- **PHASE** (0 to 1, default 0). Staggers where each copy starts in the wavetable, so the
  stack sounds detuned from the very first sample instead of taking a moment to spread.

### FILTER 1 and FILTER 2

Two identical filter rows, each with its own four-stage envelope beside it. They run one
after the other, so you can (for example) low-pass with one and high-pass with the other
to make a band.

Each row, left to right:

| Control | Range | Filter 1 default | Filter 2 default |
|---|---|---|---|
| Type (drop-down) | LP / HP / BP / Notch | **LP** | **Notch** |
| ENV | -1 to +1 | 0 | 0 |
| FREQ | 20 - 20000 Hz | 20000 (fully open) | 20000 |
| RES | 0.1 - 1 | 0.7071 | 0.7071 |
| KB | 0 - 1 | 0 | 0 |

- **Type** - LP keeps everything below the cutoff (warm), HP keeps everything above it
  (thin), BP keeps only a band (nasal), Notch removes a band (hollow). Filter 2 arrives set
  to Notch at 20 kHz, which is effectively transparent - it is out of the way until you
  move it.
- **ENV** - how far this filter's envelope moves the cutoff, and in which direction.
  Positive opens the filter over the note; negative closes it. At 0 the envelope beside it
  does nothing.
- **FREQ** - the cutoff.
- **RES** - resonance. 0.7071 is the flat, transparent setting; higher values put a peak at
  the cutoff and make sweeps sing.
- **KB** - keyboard tracking. At 1 the cutoff follows the notes you play so high notes stay
  as bright as low ones; at 0 the cutoff is fixed.
- **ATK / DEC / SUS / REL** (0.001 - 10 s / 0.001 - 10 s / 0 - 1 / 0.001 - 10 s, defaults
  0.01 / 0.10 / 0.5 / 0.30) - the envelope that drives the ENV knob.

The **F1 OFS** and **F2 OFS** knobs over in the TIMBRE box shift each filter's cutoff in
semitones relative to the played note.

### AMP ENV / PHASE

| Control | Range | Default | What it does |
|---|---|---|---|
| ATK | 0.001 - 10 s | 0.01 s | Time to reach full volume. |
| DEC | 0.001 - 10 s | 0.10 s | Time to fall to the sustain level. |
| SUS | 0 - 1 | 0.8 | Level held while the key is down. |
| REL | 0.001 - 10 s | 0.30 s | Fade time after you let go. |
| START | 0 - 1 | 0 | Where in the waveform each note begins. |
| RAND | 0 - 1 | 1 | How much that starting point is randomized per note. At 1 (the default) every note starts somewhere different, which sounds natural. Set RAND to 0 and every note starts at exactly the START position, which makes repeated notes sound identical and makes very short percussive sounds consistent. |

### BLUR / PRISM (per-part)

These reshape the harmonic stack before it becomes a waveform. They are per-part - the
A / B buttons decide which layer you are editing.

- **BLUR** (0 to 1, default 0). Smears energy between neighboring harmonics. Small amounts
  soften and thicken; large amounts turn a clear tone into a wash.
- **TIME** (0 to 2, default 1). Scales how wide the smear is - at 0 the smear is as narrow
  as it gets, at 2 it is twice the default width.
- **HARM** (0 to 1, default 0). Below 0.5 the smear averages every neighboring partial.
  Above 0.5 it only averages every other one, which keeps more of the harmonic
  relationships intact while still softening the tone. It behaves as a switch at the
  halfway point rather than as a gradual blend.
- **PRISM** (-1 to +1, default 0, center = off). Pushes the harmonics off their exact
  whole-number relationships. Small amounts add shimmer and metal; large amounts make the
  sound inharmonic and bell-like. The sign flips which way they move.
- **MODE** (three-position selector, default Stretched). How the harmonics are moved:
  **Stretched** spreads them apart, **Bunched** pulls them together, **Scattered**
  displaces them irregularly.

### FX - PLUCK / PHASER / EQ

- **PLUCK** (0 to 1, default 0, per-part). Makes the high harmonics die away faster than
  the low ones, which is what a plucked string does. Turn it up and a sustained pad becomes
  a decaying pluck without touching the amp envelope.
- **BLUR** (toggle, default off, per-part). Adds a softened attack curve on top of the
  pluck decay.
- **MIX** (0 to 1, default 0). Output phaser wet amount. At 0 the phaser is off.
- **DEPTH** (0 to 1, default 0.5). How far the phaser sweeps.
- **RATE** (0.01 to 10 Hz, default 1 Hz). How fast it sweeps.
- **WIDTH** (0 to 0.95, default 0.5). Phaser feedback - the higher it goes, the more
  pronounced and resonant the phasing gets.
- **OFS** (200 to 2000 Hz, default 1000 Hz). The center frequency the phaser sweeps around.
- **MASK** (0.1 to 10, default 1, per-part). A spectral phaser applied inside the harmonic
  stack rather than to the output - it notches harmonics directly. This knob sets the
  spacing of those notches.
- **EQ** (0 to 1, default 0). Output tilt EQ. At 0 it is flat; turn it up and the balance
  tilts around 1 kHz, up to about 6 dB.

### PITCH

- **FREQ** (-24 to +24 semitones, default 0). Transposes the whole instrument.
- **DETUNE** (-100 to +100 cents, default 0). Fine tuning between semitones.
- **FRAC** (seven-position selector, default 1/1). Multiplies the note's frequency:
  1/1 (no change), 1/2 (one octave down), 1/4, 1/8 (three octaves down), x2 (one octave
  up), x4, x8.
- **OCT** and **Hz** (toggles, display only). These change how FREQ reads and drags, not
  what it does. With **OCT** on, dragging FREQ snaps in whole octaves. With **Hz** on, the
  pop-up shows the resulting frequency in hertz instead of semitones. Neither affects the
  sound and neither is saved as a sound setting.

### LFO MOD

These are macros. They write straight into the modulation matrix, overwriting whatever the
mod editor holds for the destination they touch.

- **RATE** (13-position selector, default "1 beat"). Cycle length, from 1/8 of a beat up to
  32 beats. Sets the LFO cycle for **every** modulation target at once.
- **SHAPE** (four-position selector, default Sine: Sine / Triangle / Saw / Square). Sets
  the LFO shape for every target at once.
- **TEMPO** (toggle, default on). On, the RATE is in beats and follows the song tempo. Off,
  it is in seconds and free-runs.
- **VEL** (-1 to +1, default 0). The LFO scales the velocity of each note as it starts, up
  to plus or minus 50 percent at full depth. Gives you a slow rise and fall in how hard the
  part appears to be played.
- **VOL** (-1 to +1, default 0). LFO depth on the Volume target - tremolo, via the matrix.
- **PITCH** (-1 to +1, default 0). LFO depth on the Pitch target - vibrato, via the matrix.

Negative values invert the swing. Because these are macros, moving RATE, SHAPE, TEMPO,
VOL or PITCH replaces the corresponding per-target setting in the mod editor.

### STRUM

Only does anything when several notes start at exactly the same moment.

- **DIR** (three-position selector, default Up). Up plays low to high, Down plays high to
  low, Random shuffles.
- **TIME** (0 to 0.5 s, default 0). The total spread across the chord. At 0 strum is off.
- **TNS** (-1 to +1, default 0). Tension - how the delays are distributed. 0 is even,
  positive bunches the notes toward the start of the strum, negative bunches them toward
  the end.

### The XYZ pad

A square pad with three knobs. Drag inside the pad to set **X** (left-right) and **Y**
(up-down); the **Z** knob is separate. All three run -1 to +1 and default to center.

On their own they do nothing audible. They are three of the seven modulation sources - to
use them, open the mod editor, pick a destination, choose Mod X (or Y, or Z) as the source,
and set a depth. Then the pad becomes a hands-on control over whatever you assigned.

### The spectrogram

A live, black display of all 516 harmonics, drawn as amber bars, summed across every
sounding voice and weighted by each voice's envelope and layer level. It updates 30 times a
second with a peak-fall smoothing so bars drop gently rather than flicker. It is a display
only - you cannot edit it. It is the fastest way to see what Prism, Blur, Pluck, the masks
and the Brownian rolloff are actually doing to your sound.

### The modulation editor

The right-hand third of the panel. This is where you draw modulation shapes.

**Top row - two drop-downs.** The left one picks the **destination** (what gets modulated),
the right one picks the **source** (what does the modulating).

The sixteen destinations are: Volume, Pan, Pitch, Timbre Blend, Pluck Decay, Prism Amount,
Blur Harm, Filter 1 Cutoff, Filter 2 Cutoff, Filter 1 Resonance, Filter 2 Resonance,
Phaser Mix, Phaser Width, Unison Pitch Thickness, Part A Level, Part B Level.

The seven sources are: **Envelope** (the shape you draw, played once per note),
**LFO** (the shape repeats at the chosen rate), **Velocity** (how hard you played),
**Keyboard** (where on the keyboard you played), and **Mod X / Mod Y / Mod Z** (the pad).
Every destination keeps its own independent settings for all seven sources.

**The graph.** Click in empty space to add a point. Drag a point to move it.
Double-click a point to cycle how the line leaves it - linear, smooth, or a hard step.
Ctrl+Z and Ctrl+Y undo and redo inside the graph.

**Tool buttons above the graph:**
- **ENV** - the only tab. (An image-resynthesis tab exists in the data model but is not
  selectable in this version.)
- **CURVE** / **STEP** - whether newly added points are smooth or stair-stepped.
- **SNAP** - new points and drags snap to the grid.
- The **snap-resolution drop-down** uses the same division names as the Piano Roll and the
  Builder, triplets included.
- **FREEZE** - locks the curve so you cannot disturb it while auditioning.
- **+** / **-** - zoom in up to 8x and back out; the scroll bar underneath pans.

**Bottom strip:**
- **DEPTH** (-1 to +1, center detent, default 0). Strength. Center is no effect; right
  applies the curve as drawn; left applies it inverted. **This is the knob that switches a
  modulation on** - a drawn curve with a depth of 0 does nothing.
- **LENGTH** (13 discrete steps: 1/8, 1/4, 3/8, 1/2, 5/8, 3/4, 7/8, 1, 2, 4, 8, 16, 32).
  How long the curve takes to play from left to right. Shown only for the Envelope and LFO
  sources.
- **TEMPO** (toggle, default on). On, LENGTH is in beats and follows the song tempo (4 = one
  bar in 4/4). Off, LENGTH is in seconds.
- **SHAPE** - LFO source only: Sine / Triangle / Saw / Square.
- **SPD / TNS / SKEW / PW** (each 0 to 1, all defaulting to 0.5 = neutral). Four warps on
  the drawn curve. **SPD** changes how fast the curve advances. **TNS** reshapes the line
  between points - below center eases in, above center eases out. **SKEW** slides the whole
  curve horizontally, compressing it toward the start or stretching it toward the end.
  **PW** is asymmetry: it remaps the curve so its midpoint lands wherever you set PW.

A note shorter than LENGTH simply stops partway through the curve and releases over the
amp release time; a note longer than LENGTH holds the curve's final value.

### Presets

The **Preset** button on the window's title strip opens the preset menu. Factory presets
live in `Documents\BaySickDAW\Presets\Harmless\`, with each folder becoming a submenu.
The menu also offers **Save preset...** (which writes to `Presets\Harmless\My Presets\`)
and **Init (reset to default)**, which returns every control to its factory value in one
undoable step.

Loading a preset renames the tab, the mixer strip and the piano-roll label to the preset's
filename.

### Per-note expression from the piano roll

| Controller | Effect |
|---|---|
| CC10 | Per-note pan; a sounding note glides to the new position over about 8 ms |
| CC71 | Per-note resonance offset |
| CC72 | Per-note release scale, from a quarter to four times the amp REL setting |
| CC74 | Per-note brightness, plus or minus 2 octaves on both filter cutoffs |
| CC84 (+ CC5 / CC37) | Slide: the note starts at another pitch and glides in, with an optional glide time in milliseconds |
| CC85 / CC86 / CC89 | Slide takeover of a sounding note, with loudness and pan ramping over the same span |

Harmless has no mod wheel destination, so CC1 does nothing here.

---

## Parameters and persistence

### APVTS parameters

102 parameters, ids `tk_<trackId>_harm_<name>`, where `<trackId>` is `lay_<n>` for a
Layers tab or `bas_<n>` for a Bass tab.

**Timbre and parts**

| Id | Type | Range | Default |
|---|---|---|---|
| `timbre_shape` | choice | Sine, Saw, Square, Triangle | Saw |
| `partB_timbre_shape` | choice | Sine, Saw, Square, Triangle | Square |
| `timbre_blend` | float | 0 - 1 | 0 |
| `partA_level` | float | 0 - 1 | 1.0 |
| `partB_level` | float | 0 - 1 | 0.0 |
| `part_sel` | int | 0 - 1 | 0 |
| `vel_link` | bool | - | off |
| `auto_gain_mode` | int | 0 = Relative, 1 = Absolute | 0 |

`part_sel` is deliberately **not** stamped for automation - it is view state that no DSP
reads, so a lane on it could never do anything.

**Spectral modules** - each has a Part A id and a `partB_`-prefixed twin with the same
range and default.

| Id (Part A) | Part B twin | Type | Range | Default |
|---|---|---|---|---|
| `brownian_amount` | `partB_brownian_amount` | float | 0 - 1 | 1.0 |
| `prism_amount` | `partB_prism_amount` | float | -1 - +1 | 0 |
| `prism_mode` | `partB_prism_mode` | int | 0 - 2 | 0 |
| `pluck_decay` | `partB_pluck_decay` | float | 0 - 1 | 0 |
| `pluck_blur` | `partB_pluck_blur` | bool | - | off |
| `blur_size` | `partB_blur_size` | float | 0 - 1 | 0 |
| `blur_time` | `partB_blur_time` | float | 0 - 2 | 1.0 |
| `blur_harm` | `partB_blur_harm` | float | 0 - 1 | 0 |
| `filter_mask_cutoff` | `partB_filter_mask_cutoff` | float | 20 - 20000 Hz | 5000 |
| `phaser_mask_rate` | `partB_phaser_mask_rate` | float | 0.1 - 10 | 1.0 |

**Filters**

| Id | Type | Range | Default |
|---|---|---|---|
| `flt1_type` / `flt2_type` | int | 0 LP, 1 HP, 2 BP, 3 Notch | 0 / **3** |
| `flt_cutoff` / `flt2_cutoff` | float | 20 - 20000 Hz | 20000 |
| `flt_res` / `flt2_res` | float | 0.1 - 1 | 0.7071 |
| `flt_env_amt` / `flt2_env_amt` | float | -1 - +1 | 0 |
| `flt1_kb_track` / `flt2_kb_track` | float | 0 - 1 | 0 |
| `flt1_cutoff_ofs` / `flt2_cutoff_ofs` | float | -24 - +24 semitones | 0 |
| `flt_a` / `flt_d` / `flt_r`, `flt2_a` / `flt2_d` / `flt2_r` | float | 0.001 - 10 s | 0.01 / 0.10 / 0.30 |
| `flt_s` / `flt2_s` | float | 0 - 1 | 0.5 |

**Amp, phase, output**

| Id | Type | Range | Default |
|---|---|---|---|
| `amp_a` / `amp_d` / `amp_r` | float | 0.001 - 10 s | 0.01 / 0.10 / 0.30 |
| `amp_s` | float | 0 - 1 | 0.8 |
| `phase_start` | float | 0 - 1 | 0 |
| `phase_rand` | float | 0 - 1 | 1.0 |
| `volume` | float | 0 - 1 | 0.8 |
| `pan` | float | -1 - +1 | 0 |
| `oeq_mix` | float | 0 - 1 | 0 |
| `cutSelf` | bool | - | off |
| `cutSelfMode` | bool | false = Same Pitch, true = Cut All | Same Pitch |

**Routing matrix**

| Id | Range | Default |
|---|---|---|
| `rm_sub` / `rm_prot` / `rm_clip` | 0 - 1 | 0 |
| `rm_fx` / `rm_vol` / `rm_env` | 0 - 1 | 1 |

**Unison, pitch, performance**

| Id | Type | Range | Default |
|---|---|---|---|
| `unison_voices` | int | 1 - 9 | 1 |
| `unison_detune` | float | 0 - 100 cents | 15 |
| `unison_spread` | float | 0 - 1 | 0.7 |
| `unison_type` | int | 0 Pure, 1 Random, 2 Drifting, 3 Alt-only | 0 |
| `unison_alt` | bool | - | off |
| `unison_phase` | float | 0 - 1 | 0 |
| `pitch_semitones` | float | -24 - +24 | 0 |
| `pitch_cents` | float | -100 - +100 | 0 |
| `pitch_freq_frac` | int | 0 - 6 (1/1, 1/2, 1/4, 1/8, x2, x4, x8) | 0 |
| `glide_time` | float | 0 - 2 s | 0 |
| `legato` | bool | - | off |
| `legato_limit` | float | 0.001 - 1 s | 0.5 |
| `strum_time` | float | 0 - 0.5 s | 0 |
| `strum_dir` | int | 0 up, 1 down, 2 random | 0 |
| `strum_tns` | float | -1 - +1 | 0 |

**Tremolo, vibrato, phaser, LFO, pad**

| Id | Type | Range | Default |
|---|---|---|---|
| `trem_shape` / `vib_shape` | int | 0 - 3 | 0 |
| `trem_depth` | float | 0 - 1 | 0 |
| `trem_speed` | float | 0.1 - 20 Hz | 3 |
| `trem_gap` | float | 0 - 1 | 0 |
| `vib_depth` | float | 0 - 2 semitones | 0 |
| `vib_speed` | float | 0.1 - 20 Hz | 5 |
| `vib_env` | float | 0 - 1 | 0 |
| `ophaser_mix` | float | 0 - 1 | 0 |
| `ophaser_depth` | float | 0 - 1 | 0.5 |
| `ophaser_rate` | float | 0.01 - 10 Hz | 1 |
| `ophaser_width` | float | 0 - 0.95 | 0.5 |
| `ophaser_ofs` | float | 200 - 2000 Hz | 1000 |
| `lfo_rate` | int | 0 - 12 (13-step table) | 7 (= 1 beat) |
| `lfo_shape` | int | 0 Sine, 1 Triangle, 2 Saw, 3 Square | 0 |
| `lfo_tempo` | bool | - | on |
| `lfo_vel` / `lfo_vol` / `lfo_pitch` | float | -1 - +1 | 0 |
| `mod_x` / `mod_y` / `mod_z` | float | -1 - +1 | 0 |

### The modulation matrix - state outside the APVTS

The mod matrix is **not** an APVTS parameter set. It lives in `HarmlessModRegistry` and is
serialized as a `harmlessMod` child ValueTree. Its shape is 16 targets x 7 sources; each
source holds a curve point list (time, value, curve type per point), a depth, a length, a
tempo-sync flag, a global flag, an LFO shape, and the four warp values (spd, tns, skew,
pw). Restore is field-complete: unknown target ids are skipped, and an empty point list is
repaired to the default ramp so sampling can never hit an empty vector.

The matrix **is** automatable, through lanes named `<targetParamId>_mod<N>_depth` and
`<targetParamId>_mod<N>_length`, where N is the source index 0-6. Length lanes exist only
for the Envelope and LFO sources, because the other five have no time behavior and the
editor hides the control for them; a length lane lands on the same 13 discrete steps the
knob offers.

### What is saved where

- **With the project** - the whole APVTS **plus** the mod matrix.
  `HarmlessProcessor::getStateInformation` copies the APVTS state, strips any previous
  matrix child, appends a fresh `mModRegistry.toValueTree()`, and writes XML to binary.
  The tab record stores that blob base64-encoded. `setStateInformation` pulls the matrix
  child out, applies it to the registry, then replaces the APVTS state.
- **With a preset file** - the Preset menu's Save writes `apvts.copyState()` as plain XML
  and does **not** attach the matrix the way the project path does. Treat the modulation
  matrix as project state, not preset state: a preset reliably carries the knobs, not the
  drawn curves.
- **Per-machine, not in the project** - the window's position and size (owned by the
  workspace), and the mod editor's view state: zoom level, scroll position, FREEZE,
  CURVE / STEP, SNAP and the snap-resolution choice, plus its own undo stack.
- **Not saved at all** - pending audition notes, the spectrogram's smoothing buffer, the
  OCT and Hz display toggles in the PITCH box (they are display-only and have no
  parameter), and the per-note expression state a voice is holding.

**Undo.** The APVTS is bound to the one app-wide `UndoManager` and tagged with a stable rig
identity (`rig:<kind>:<pageIndex>`), so parameter edits join the global history and survive
the engine being torn down and rebuilt. Preset loads and Init are single undoable actions.
The mod editor's graph keeps its **own** Ctrl+Z history, separate from the app's undo.

---

## Lifetime and teardown

`EngineRig` owns the engine, constructed in `createEngineFor` keyed on
`(TabKind, pageIndex)`, prepared and registered for audio dispatch there.
`LayersPage` / `BassPage` hold a **non-owning** pointer plus the editor they created.

Order that matters:

- The constructor registers the 16 modulation targets **before** handing the registry to
  the synth, and then pre-warms the parameter cache with a full `updateFromApvts` on the
  message thread - so the first audio block finds every value already matching and skips
  every setter. Without that pre-warm the first block would run a dozen FFT wavetable
  rebuilds on the real-time thread.
- Wavetable rebuilds requested by the UI run on a background `TimeSliceThread` owned by
  `HarmlessSynth`. Per-voice rebuilds triggered by modulation stay on the audio thread by
  design (they are sparse and need to be immediately audible).
- The registry's target pointers are stable for the registry's lifetime, which is what lets
  voices cache them; the audio thread never takes the registry's edit lock.
- The editor installs two global right-click hooks (`sShouldOfferModulate` and
  `sOnModulateEnvelope`) and clears them in its destructor. These are process-global: with
  two Harmless windows open, the last one opened owns the hooks.
- Automation applicators - both the parameter lanes and the mod-matrix lanes - are
  registered **model-side** at engine creation, never against a widget, and re-resolve their
  target through the rig at apply time. That is what lets automation keep working after a
  window has been closed, and across a tab swapping engines and back.
- The A / B rebinding destroys and rebuilds nine attachments each time you click A or B.
  Both parts' automation lanes are unaffected, because those lanes target the parameters,
  not the shared knobs.

---

## Cross-references

- `BaySickSynth.md` - the subtractive engine, the other synth on a Layers tab.
- `BaySickBass.md` - the subtractive engine with bass defaults, the other synth on a Bass
  tab.

---

## Differs from Carry-Forward

- Carry-Forward's engine-audition entry says "All 4 engine processors
  (BaySickSynth/Bass/Harmless/VibePlayer)". There are now **seven** engines carrying the
  `auditionNote` pattern, and Harmless's version has grown a press-and-hold pair
  (`auditionNoteOn` / `auditionNoteOff`) with an accumulating note-off mask alongside the
  original one-shot exchange.
- Carry-Forward predates model-owned engines. Harmless is no longer owned by its page:
  `EngineRig` owns it and the page holds a non-owning view pointer, so the engine keeps
  playing when the window is closed.
- Carry-Forward's section 5 lists **DSP-01, "Harmless lazersaw silent - needs preset audit
  harness"**, as confirmed open against batch QA-K. That entry describes a preset-library
  problem, not an engine defect, and nothing in the current engine code corresponds to it;
  its present status is not determined from the code and must be read from the Main Plan
  and the Implemented Work Log rather than from here.
