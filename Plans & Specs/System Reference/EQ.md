# EQ

**Purpose** - An equalizer changes how bright, warm, thick or thin something
sounds by turning specific frequency ranges up or down. BaySickDAW gives every
channel two of them: one before its effect rack and one after. Each is a
96-band parametric EQ built on the kbs engine (the KBS EQ Pro engine taken
back at QA-EqPro; the flagship pass QA-EqFlagship 2026-08-27): three domain
VIEWS (Stereo / Mid / Side), nine band types with CONTINUOUS slopes to
96 dB/oct plus Brickwall, per-band phase, Natural Phase and Mixed Phase plus
five true linear-phase precisions, two-way + onset-selective + SPECTRAL
dynamics, per-band LFO/envelope modulators, a program-dependent color stage
with per-band saturation, whole-curve transforms, delta listen,
loudness-matched solo, domain audition, multi-select with linked groups, EQ
Sketch, note-aware frequency, A/B compare with a morph strip, spectrum grab,
EQ Match (live capture, offline track scan, Auto Cleanup, stored spectra,
Match Amount), presets, and a project-wide instance browser with window
re-pointing.

---

## How it operates

- **One engine per bank.** Every mixer node (18 buses + every insert strip)
  carries `StripEq preEq` and `StripEq eq` (Source/DSP/StripEq.h) - each a
  wrapper around ONE `kbs::ParametricEq` (Source/DSP/Kbs/ParametricEq.h,
  96 pre-allocated bands - `kbs::kEqMaxBands`, the ONE home; the engine
  never grows storage under a running audio thread). The old mid+side pair
  of 8-band engines is gone: a band's domain is its own `EqChannel`
  (stereo / mid / side / left / right), so one engine covers what the pair
  covered with half the instances and half the linear-mode latency (the
  pair ran its halves in series).
- **The engine is the vendored KBS core** (Source/DSP/Kbs/: ParametricEq,
  EqLinearPhase, EqMatch, EqCharacter, SpectrumScan +
  Devices/SVF/Oversampler/FFT/Feeds, JUCE-free, namespace `kbs`), with the
  BaySickDAW extensions fed back as reference: per-domain linear phase (the
  2x2 matrix convolver), the four-slot per-band sidechain, and the whole
  QA-EqFlagship feature set (96-band pool, fractional-slope ladder,
  all-pass, per-band phase + Mixed, two-way/onset/spectral dynamics, color,
  modulators, curve transforms, delta/matched-solo/domain audition, the
  cleanup fitter, the offline scan accumulator).
- **Every question the display asks is answered by the engine's own
  arithmetic** (`magnitudeAt`, `bandMagnitudeAt`, `bandExtentMagnitudeAt`,
  `phaseAt`, `bandGrDb`). There is no drawing-side formula to drift - the
  rule that buried the old display's defect class.
- **Sync**: band and global values are APVTS parameters; the audio thread's
  per-block sweep (`updateEQFromCache`, gated by the EQ dirty flag) builds
  `kbs::EqBandParams` from a nullable pointer cache and pushes through
  `StripEq::pushBand` / `pushGlobals` (full-struct compares - an untouched
  band costs a compare). Processing Mode and Oversampling are parameters
  too but NEVER ride the sweep or automation: they reallocate the linear
  engine, so an APVTS listener applies them on the message thread under the
  nest-aware project-load shield (SC-15).
- **Identity fast path**: `StripEq::isIdentity` short-circuits a strip whose
  bands are off or flat - but never when latency > 0 (a flat EQ in a linear
  mode must impose its delay or the strip plays early against the PDC
  solve). Latency reports exactly (`hop + (taps-1)/2`), pinned by an
  impulse test to the sample, and enters `updateBusLatencies` as before.
- **The proof target**: `run_eq_tests.bat` builds and runs `BaySickEqTests`
  (Tools/EqTests, the KBS test sections + the extension regressions +
  QA-EqFlagship sections 14-22 - 128 checks). Deliberately outside
  do_build.bat's six-exit-code gate.

## User-facing behavior

### Opening an EQ

Unchanged surfaces: the Effects page's Pre EQ / Post EQ buttons, the ribbon
Effects sub-page rows, every instance page's window-row dropdown, and
workspace restore. Each window is fixed Pre or Post for life; the title
strip's other tab OPENS the sibling. Opening a window is the strip's EQ
"first touch" - its parameters register then (see Parameters below).
Window floor is 720x420 (the KBS budget - the rail provably fits at that
minimum); growth is the WorkspaceWindow's own resize.

### The window

Top strip: the ST / MID / SIDE view row (HOLD Mid or Side to audition
that domain alone - release restores, a quick click still switches), then
the chip row: 24 numbered chips per PAGE of the 96-band pool (page arrows
appear only once a page fills or holds a band - a beginner sees exactly
one row of 24), the A/B morph strip, "+", and the A/B pill. Below: the
graph (analyser, optional spectrogram / phase / piano strip), and the
collapsible right rail (click its left edge).

**The views ARE the domains** (Jeff's ruling): a band made in the Side view
works the sides; Mid view, the center; no routing gesture exists. The other
views stay visible as a dimmed, live, non-clickable ghost. Left/Right exist
only as a Channel pick on Stereo-view bands (amber badge); "Move to ...
view" on the band menu re-domains a band keeping its settings. All 96 bands
are one shared pool; a chip in another view wears a tiny M/S tick.

### The graph

Interaction (one modifier, one meaning): drag = freq + gain; Shift = fine;
Ctrl = gain only; Ctrl+Shift = freq only; wheel ON a dot = Q (Shift fine);
double-click empty = add band; double-click a dot = MUTE; Alt+click =
reset (bands 1-8 return to home frequencies 40/100/250/630/1600/4000/
8000/12500); right-click = the band menu at the mouse; Delete removes.
Arrows nudge, Tab cycles, hold L listens, hold G arms the grab (keys work
while the window has focus; the headphone and crosshair buttons do the
same jobs by mouse).

Multi-select (W-16): drag on EMPTY space rubber-bands a selection;
Ctrl+click adds/removes dots (Ctrl+drag on the lone primary keeps its
gain-only meaning). Dragging any selected dot moves the whole set as a
shape - frequency as a ratio (a log shift), gain as an offset; starting
the drag with Alt scales the gains proportionally instead (Alt on a lone
dot still resets it). Set members wear a light outer ring. Persistent
LINKED GROUPS (band menu) ride drags even with nothing selected and are
serialized in the EQ point's view tree; linked bands wear a small corner
dot.

The piano strip (View menu) is an instrument: CLICK a key and the
selected band snaps to that note (no selection: a band is born on it).
EQ Sketch (the window menu): the next stroke drawn across the graph
becomes bands - bells where it bulges with the Q read off the stroke's
width, shelves where it holds to an edge - one undo step, so plain Undo
is the escape; right-click cancels.

Handles: filled dot + band number, fontaudio type glyph above (Tilt drawn),
thin second ring + a fixed 3x22 px mini GR meter on dynamic bands, L/R
badge, isolate glyph, the selected band's headphone latch button. A
spectral band's live per-bin move draws as a bright breathing line; a
modulating band's curve breathes with its LFO/envelope (IIR modes).

### The band menu

Type (9: Bell / Low Pass / High Pass / Low Shelf / High Shelf / Notch /
Band Pass / Tilt / All Pass - All Pass is unity magnitude, phase-only,
deliberately inert in the linear modes), Slope (detents 6..96 dB/oct +
Brickwall; the rail's SLOPE number covers every fractional value between;
Brickwall is linear-phase-only and processes as 96 elsewhere), Channel
(Stereo view only: Stereo / Left / Right), Move to view, Dynamic (Make
Dynamic / Auto Release / Spectral in linear modes), Listen, Isolate,
Delta Listen (this band's effect alone: Delta composed with Isolate),
Mute, Split to Left + Right (a Stereo band becomes its own L and R
copies, every setting kept, one undo step), Link Selected Bands / Unlink,
Reset Band, Delete Band.

### The rail

BAND header + colour dot; GAIN / SAT / PAN knobs (SAT is the band's
saturation, 2026-08-28 - was a drag-number; PAN splits the band's effect
across the stereo image - gain types, Stereo view only; the drawn curve
deliberately does not move with it); FREQ / Q drag-numbers (double-click
types - FREQ also accepts note names: "A4", "Bb2", "C#2+13", and its
caption reads the live note name while a band is selected; on Low Pass /
High Pass the Q is RESONANCE - the final cascade pair's Butterworth Q
scaled by q/0.7071, so the cutoff gain lands at exactly Q at ANY slope,
neutral Q stays pure Butterworth, and slope 12 keeps its old meaning;
mirrored in the queries and, past neutral Q, realized identically by the
linear modes); the type glyph grid (9); ST/L/R; SLOPE (continuous
dB/oct) and PHASE (per-band phase mix, linear modes) drag-numbers
(each row laid out only when visible; the MOD block lays out for every
band - the fixed height budget that parked hidden rows at stale bounds
is gone, 2026-08-28); DYNAMICS:
DYN / AUTO / EXT, DOWN / UP direction, THR / RATIO / ATK / REL, the
second stage's THR B / RATIO B / RANGE B, ONSET, DENSE (spectral
neighborhood width) and a real GR meter; the MOD block: RATE / LFO / ENV
knobs with F/G/Q target rows for the LFO and the envelope follower. EXT
opens the strip's four sidechain receive lines by name - the band detects
from the picked line (that line also feeds the analyser's Sidechain
overlay and the collision tint). Rail knobs carry their parameter ids, so
the app's global right-click (Automate / Type in value / MIDI Learn)
works on them.

### Dynamics (two-way + onset + spectral)

The over-threshold model, unchanged: both directions engage when the
detector EXCEEDS the threshold, by the excess times the ratio slope -
DOWN compresses, UP expands; THR at 0 dB never engages, at -60
everything; travel bounded by min(|threshold| x slope, |range|), the
dashed extent lines drawn from the same expression (both directions now
that two stages exist). Release can be programme-dependent (AUTO). A
dynamic Notch is a de-resonator (a GR-driven cut bell), identity at rest.

The SECOND stage (THR B / RATIO B / RANGE B) engages when the detector
drops UNDER its threshold, by the shortfall times its slope - a positive
Range B lifts the quiet (upward compression), negative pushes it further
down. 0 dB of Range B is inert; both stages run independently on the one
detector. ONSET (0..100%) reshapes what the detector hears: 0 is the
whole signal, 100% only transients (a fast-minus-slow envelope pair), so
dynamics can hit the attack of a drum and ignore its ring.

SPECTRAL (linear modes only, Dynamic submenu): the band stops moving as
one gain and instead watches the individual bins inside its footprint,
ducking only the ones standing over their own spectral neighborhood -
the harsh bin moves, the note beside it does not. DENSE sets how
surgical the neighborhood is; attack/release keep their meaning at frame
granularity; per-bin travel is capped by the band's range; the graph
draws the live per-bin move as a bright breathing line. Outside the
linear modes the flag is inert and the band is its plain dynamic self.

### Modes + phase

Zero Latency (RBJ IIR), Natural Phase (Orfanidis decramped bells - analog
shape at Nyquist, still zero latency), five Linear precisions (FFT
1024..16384; overlap-save FIR - no ripple, edits reach the audio, exact
reported latency; dynamics ride minimum-phase deltas on the linear bed;
per-band domains survive via the matrix path), and MIXED PHASE: the
linear machinery with a frequency-weighted phase floor - minimum-phase
character in the lows (no bass pre-ring), linear behavior in the highs,
one raised-cosine blend 150 Hz -> 1.5 kHz, one latency figure. Per-band
PHASE (linear modes): 0 = the mode's phase, 100% = this band
minimum-phase - excess phase is granted per band, so one low bell can
opt out of pre-ring while the rest of the curve stays linear. Menu
latency figures are computed from the engine's constants at the session
rate. Oversampling 2x serves the IIR path only.

### Color (the character stage)

Options > Color: None (bit-exact off), Smooth (odd-order cubic), Warm
(asymmetric soft clip), Changed (colors ONLY what the EQ altered -
difference-mode saturation that nulls exactly on a flat EQ), plus an
amount. Program-dependent drive: a 10 ms follower backs the color off as
material gets loud, unity small-signal gain by construction. Per-band
SAT folds a softened copy of just that band's region back in. Names per
Jeff's ruling (sub-spec 1, 2026-08-27).

### Modulators (per band, IIR modes)

Every band carries one LFO (RATE 0.02..20 Hz, depth, target Freq / Gain /
Q) and one envelope follower (SIGNED depth - positive follows the
material, negative ducks against it; same targets). Depth semantics:
freq swings half an octave, gain 6 dB, Q an octave of width, times
depth. They fold into the same glide targets dynamics uses, so
modulation is always a sweep, never a step. IIR MODES ONLY by design: a
linear-phase FIR is a periodically rebuilt snapshot, and an LFO wobbling
its design target would rebuild the kernel at frame rate for a wobble
quantized to frames - so in linear/mixed modes the depths are inert and
the band renders static. All five are ordinary automatable parameters
(sub-spec 2a).

### Tools

**Listen** is loudness-matched (W-10): the band-passed slice rides paired
followers toward the program's own level (+-24 dB cap), so soloing a
narrow band is no longer a level drop - and it is still exactly what the
band's detector hears. **Isolate** mutes the other bands. **Delta
Listen** (window menu): the output becomes out-minus-in, latency-aligned
in every mode - you hear exactly what the EQ is doing; a flat EQ is
silence. Per-band delta (band menu) is Delta composed with Isolate.
**Domain audition**: hold the MID or SIDE view button. **Spectrum grab**:
arm, the analyser max-holds, any empty click drops a pre-aimed cut bell
on the found peak, one grab per arming. **Whole Curve** (window menu):
Scale (to +-200%; negative inverts - the handles keep their set points,
the curve moves through one xfFreq/xfGain choke every design and query
reads) and Shift (+-24 semitones), both ordinary automatable bank
parameters.

**EQ Match** (the panel): Current from a live capture (pre feed), a
loaded audio file, or the OFFLINE TRACK SCAN (W-22, DAW-only) - one
offline pass over the timeline (a ruler selection wins; else the whole
song) taps this strip pre-EQ through `kbs::SpectrumScan` and reports
"Scanned: selection, 0:42". Reference from the picked SC line, a file, a
STORED SPECTRUM, or another INSTANCE (browser row menu). Fit = mid/side
bells against the difference (Detail slider); AMOUNT applies 0..200% of
the fit (gains and dynamic ranges scale). **Auto Cleanup** (W-2): no
reference - the Current capture is matched against its own broad-stroke
self; resonances get static cuts, problems that come and go get dynamic
bands (the capture's per-point swing decides), ADDED over the existing
curve as ordinary editable bands, one undo step, tally reported.
**Stored spectra** (W-19/W-24): save Current or Reference under a name
(`Presets/EQ/References/*.kbsref` - match-grid XML, shareable), load any
as a reference.

**Instances** (W-21, window menu): every EQ point in the project - buses
and strips, pre and post - one list with name, curve thumbnail (the
engine's own magnitude query) and a live mini-spectrum. Click a row and
THIS window re-points at it (title, tabs, bands, rail, A/B, presets and
Match all follow - ids derive from the target, the resolver runs per
use); double-click opens the point's own window; the row menu sets the
Match Reference or the Collision Reference (the collision view then runs
off that instance with no sidechain routed; picking again clears).

**Presets**: Default (the out-of-the-box state, globals included), 12
factory presets (dynamic ones carry their static cut), user presets in
`Documents/BaySickDAW/Presets/EQ` (bank-relative XML - a preset saved
anywhere loads onto any strip's either bank). **A/B**: the chip-row
pill; click swaps two complete setups (B starts blank), right-click Copy
A to B / Lock; the swap lands in the parameters as one undo step. The
MORPH strip beside it drags the live setup toward the other bank
(snapshot-based: freq/Q log, gain/slope/pan linear, discrete fields snap
halfway), one undo step, thumb springs back - what you hear stays.

### The window menu

Processing Mode (8, computed latencies), Oversampling 2x, Proportional Q,
Color (None / Smooth / Warm / Changed + amounts), Auto-Gain (+ amount),
Output Trim, Polarity Flip, Gain Scale (3/6/12/18/30), Analyser (pre /
post / sidechain, speeds, tilt 0/3/4.5, freeze, peak hold), View
(analyser / spectrogram / phase / piano), Whole Curve (Scale / Shift /
Reset Transforms), Delta Listen, Sketch a Curve..., Instances...,
Keyboard & Mouse, Reset All Bands, EQ Match..., Presets.

## Parameters and persistence

### APVTS parameters (touch-lazy, QA-EqPro SC-2 + SC-9)

One 96-band set per bank: `{stripPrefix}_{eq_|preeq_}b{N}{Suffix}` with 31
suffixes (Freq, Gain, Q, Type, On, Slope, Channel, Place, Mute, Isolate,
Dynamic, Threshold, Ratio, Attack, Release, AutoRelease, Range, ScSource,
Phase, ThresholdB, RatioB, RangeB, Onset, Spectral, Density, Sat,
LfoRate, LfoDepth, LfoTarget, EnvDepth, EnvTarget) plus 9 bank globals
(`..._{eq_|preeq_}{mode|os|propq|autogain|agamt|outgain|polarity|
charmode|charamt|curvescale|curveshift}` - mode and os are the two that
never enter the sweep). NOTHING registers with the strip: the block
(globals + bands 1-8) arrives on first touch - EQ window open, preset
load, or the load-time tree scan (`restoreEqParamsFromState`) that keeps
saved projects and their automation lanes working without a window ever
opening; bands 9-96 register one at a time on activation. Late
registration adopts saved values via the `replaceState(copyState())`
rebind. A fresh build holds approximately zero EQ params. Defaults:
bands 1-8 on-and-flat (identity-skipped), the rest off; Threshold 0 dB /
ThresholdB -60 (never engages until pulled up); Slope 12 (1..97, 97 =
Brickwall); CurveScale 100% / CurveShift 0; the KBS home frequencies
(bands 9+ redistribute the same log span across the pool).

Mode + os are automation-excluded (no lane can be created and a saved
lane is refused registration) - they apply through the shielded listener.

### Saved with a project

The APVTS parameters (authoritative), plus the StripEq blob on the rack
state (`<StripEq>`: 96 bands with every flagship field, the A/B spare
bank under `<Spare>`, viewingSpare, mode/os/propq/autogain/agamt/
outgain/polarity/charMode/charAmt/curveScale/curveShift, and the
`<View>` tree - gain scale, analyser toggles, the current domain view,
and the linked-groups string - so a window reopens the way it was left,
per EQ point). Old `<EQ8MsDSP>` blobs fail the tag check and reset -
deliberate (SC-14, pre-v1).

### Saved with presets

FX Rack presets capture the strip's `_eq_*` / `_preeq_*` parameters
(bank-relative suffixes, prefix-rewritten on load, blocks/bands ensured
first). Page presets carry the rack blob including the StripEq state.
User EQ presets: `Documents/BaySickDAW/Presets/EQ/*.xml`, bank-relative.

### Not saved

The listen latch, grab arming, analyser freeze/peak-hold, match captures,
delta listen, domain audition, sketch mode, the multi-selection, the
window's re-point target and its Match/Collision reference picks, and
the spare-bank LOCK (deliberately session-local).

## Lifetime and teardown

StripEq lives on the graph nodes (18 bus nodes always; insert nodes with
their strips). The window resolves its EQ per use through the live graph
(never caches across ticks) and closes itself when the strip dies. Load
boundaries: `resetSessionStateToDefaults` -> `resetEqStatesToDefaults`
sweeps `StripEq::resetToDefaults` on every EQ point under the shield; the
param default sweep covers the registered parameters. Mode changes and
preset/rack-blob applies all run under the nest-aware shield + settle
(the two historically unshielded paths - the EQ options menu and the
page-preset import - were closed in this batch).

## Cross-references

- `Source/DSP/Kbs/` - the vendored engine (see the header preambles; the
  KBS-side ledger is `Files For Claude/EQ Build Notes.md`; the plugin IPC
  spec is `Files For Claude/KBS Spec - EQ Spectra Share Protocol.md`).
- `Source/DSP/StripEq.h/.cpp` - the wrapper (sync, A/B, state, feeds, the
  offline scan tap).
- `Source/Standalone/EqWindowUI/` - graph, rail, analyser, match,
  instance browser, presets.
- `Tools/EqTests` + `run_eq_tests.bat` - the proof target.
- Mixer.md - strips, sends, the four sidechain receive lines.
- Freeze and Export.md - the leading-latency trim (SC-10) and why freeze
  renders opt out.

## Differs from Carry-Forward

The Carry-Forward snapshot (2026-05-07) describes the 8-band EQ8DSP /
EQ8MsDSP world: mid+side engine pairs, per-band channel pickers with
Mid/Side entries, the block-rate dynamic EQ, the Hann-squared linear path
with its recorded defects, eager per-strip EQ registration, and the
ParametricEQDisplay with its own drawing formulas. All of it is replaced
by this document's architecture (QA-EqPro, 2026-08-26).
