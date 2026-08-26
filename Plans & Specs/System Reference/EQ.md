# EQ

**Purpose** - An equalizer changes how bright, warm, thick or thin something
sounds by turning specific frequency ranges up or down. BaySickDAW gives every
channel two of them: one before its effect rack and one after. Each is a
24-band parametric EQ built on the kbs engine (the KBS EQ Pro engine taken
back, QA-EqPro 2026-08-26): three domain VIEWS (Stereo / Mid / Side), eight
band types with slopes to 96 dB/oct plus Brickwall, Natural Phase (decramped
bells at zero latency), five true linear-phase precisions, dynamic bands with
the over-threshold model, a 4-line sidechain pick per band, spectrum grab,
EQ Match, presets, and an A/B compare pair.

---

## How it operates

- **One engine per bank.** Every mixer node (18 buses + every insert strip)
  carries `StripEq preEq` and `StripEq eq` (Source/DSP/StripEq.h) - each a
  wrapper around ONE `kbs::ParametricEq` (Source/DSP/Kbs/ParametricEq.h,
  24 bands). The old mid+side pair of 8-band engines is gone: a band's
  domain is its own `EqChannel` (stereo / mid / side / left / right), so one
  engine covers what the pair covered with half the instances and half the
  linear-mode latency (the pair ran its halves in series).
- **The engine is the vendored KBS core** (Source/DSP/Kbs/: ParametricEq,
  EqLinearPhase, EqMatch + Devices/SVF/Oversampler/FFT/Feeds, JUCE-free,
  namespace `kbs`), with two BaySickDAW extensions fed back as reference:
  per-domain linear phase (a 2x2 matrix convolver, so mid/side/left/right
  bands survive into the linear modes - the C3 defect's fix) and the
  four-slot per-band sidechain (`EqBandParams::scSource`).
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
  (Tools/EqTests, the KBS test sections + the extension regressions - 54
  checks). Deliberately outside do_build.bat's six-exit-code gate.

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

Top strip: the ST / MID / SIDE view row, then 24 numbered chips + "+" +
the A/B pill. Below: the graph (analyser, optional spectrogram / phase /
piano strip), and the collapsible right rail (click its left edge).

**The views ARE the domains** (Jeff's ruling): a band made in the Side view
works the sides; Mid view, the center; no routing gesture exists. The other
views stay visible as a dimmed, live, non-clickable ghost. Left/Right exist
only as a Channel pick on Stereo-view bands (amber badge); "Move to ...
view" on the band menu re-domains a band keeping its settings. All 24 bands
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

Handles: filled dot + band number, fontaudio type glyph above (Tilt drawn),
thin second ring + a fixed 3x22 px mini GR meter on dynamic bands, L/R
badge, isolate glyph, the selected band's headphone latch button.

### The band menu

Type (8: Bell / Low Pass / High Pass / Low Shelf / High Shelf / Notch /
Band Pass / Tilt), Slope (6..96 dB/oct + Brickwall; filters only;
Brickwall is linear-phase-only and processes as 96 elsewhere), Channel
(Stereo view only: Stereo / Left / Right), Move to view, Dynamic (Make
Dynamic / Auto Release), Listen, Isolate, Mute, Reset Band, Delete Band.

### The rail

BAND header + colour dot; GAIN and PAN knobs (PAN splits the band's effect
across the stereo image - gain types, Stereo view only; the drawn curve
deliberately does not move with it); FREQ / Q drag-numbers (double-click
types); the type glyph grid; ST/L/R; the slope box; DYNAMICS: DYN / AUTO /
EXT, DOWN / UP direction, THR / RATIO / ATK / REL knobs and a real GR
meter. EXT opens the strip's four sidechain receive lines by name - the
band detects from the picked line (that line also feeds the analyser's
Sidechain overlay and the collision tint). Rail knobs carry their
parameter ids, so the app's global right-click (Automate / Type in value /
MIDI Learn) works on them.

### Dynamics (the over-threshold model)

Both directions engage when the detector EXCEEDS the threshold, by the
excess times the ratio slope - DOWN compresses, UP expands. THR at 0 dB:
nothing ever engages; at -60: everything, pinned. Travel is bounded by
min(|threshold| x slope, |range|) - the dashed extent line is drawn from
the same expression, moves with THR, and the live curve cannot cross it.
Release can be programme-dependent (AUTO). A dynamic Notch is a
de-resonator (a GR-driven cut bell), identity at rest.

### Modes

Zero Latency (RBJ IIR), Natural Phase (Orfanidis decramped bells - analog
shape at Nyquist, still zero latency), and five Linear precisions (FFT
1024..16384; overlap-save with a Kaiser-designed FIR - no ripple, edits
reach the audio, exact reported latency; dynamics ride minimum-phase
deltas on the linear bed; per-band domains survive via the matrix path).
Menu latency figures are computed from the engine's constants at the
session rate. Oversampling 2x serves the IIR path only.

### Tools

**Listen** (band-pass audition of the band's region - also exactly what
its detector hears) vs **Isolate** (mute the other bands). **Spectrum
grab**: arm, the analyser max-holds, any empty click drops a pre-aimed cut
bell on the found peak, one grab per arming. **EQ Match**: capture Current
(pre feed) + Reference (picked SC line or a loaded audio file), fit bells
to the difference (smoothness + band budget), replace into the current
view's domain. **Presets**: Default (the out-of-the-box state, globals
included), 12 factory presets (dynamic ones carry their static cut), user
presets in `Documents/BaySickDAW/Presets/EQ` (bank-relative XML - a preset
saved anywhere loads onto any strip's either bank). **A/B**: the chip-row
pill; click swaps two complete setups (B starts blank), right-click Copy
A to B / Lock; the swap lands in the parameters as one undo step.

### The window menu

Processing Mode (computed latencies), Oversampling 2x, Proportional Q,
Auto-Gain (+ amount), Output Trim, Polarity Flip, Gain Scale (3/6/12/18/30),
Analyser (pre / post / sidechain, speeds, tilt 0/3/4.5, freeze, peak hold),
View (analyser / spectrogram / phase / piano), Keyboard & Mouse, Reset All
Bands, EQ Match..., Presets.

## Parameters and persistence

### APVTS parameters (touch-lazy, QA-EqPro SC-2 + SC-9)

One 24-band set per bank: `{stripPrefix}_{eq_|preeq_}b{N}{Suffix}` with 18
suffixes (Freq, Gain, Q, Type, On, Slope, Channel, Place, Mute, Isolate,
Dynamic, Threshold, Ratio, Attack, Release, AutoRelease, Range, ScSource)
plus 7 bank globals (`..._{eq_|preeq_}{mode|os|propq|autogain|agamt|
outgain|polarity}`). NOTHING registers with the strip: the block (globals
+ bands 1-8) arrives on first touch - EQ window open, preset load, or the
load-time tree scan (`restoreEqParamsFromState`) that keeps saved projects
and their automation lanes working without a window ever opening; bands
9-24 register one at a time on activation. Late registration adopts saved
values via the `replaceState(copyState())` rebind. A fresh build holds
approximately zero EQ params (previously 9,792 blank entries in every
project file). Defaults: bands 1-8 on-and-flat (identity-skipped), the
rest off; Threshold 0 dB; Gain/Range +-30; Q 0.1-30; the KBS home
frequencies.

Mode + os are automation-excluded (no lane can be created and a saved
lane is refused registration) - they apply through the shielded listener.

### Saved with a project

The APVTS parameters (authoritative), plus the StripEq blob on the rack
state (`<StripEq>`: 24 bands, the A/B spare bank under `<Spare>`,
viewingSpare, mode/os/propq/autogain/agamt/outgain/polarity, and the
`<View>` tree - gain scale, analyser toggles, the current domain view -
so a window reopens the way it was left, per EQ point). Old `<EQ8MsDSP>`
blobs fail the tag check and reset - deliberate (SC-14, pre-v1).

### Saved with presets

FX Rack presets capture the strip's `_eq_*` / `_preeq_*` parameters
(bank-relative suffixes, prefix-rewritten on load, blocks/bands ensured
first). Page presets carry the rack blob including the StripEq state.
User EQ presets: `Documents/BaySickDAW/Presets/EQ/*.xml`, bank-relative.

### Not saved

The listen latch, grab arming, analyser freeze/peak-hold, match captures,
and the spare-bank LOCK (deliberately session-local, as before).

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
  KBS-side ledger is `Files For Claude/EQ Build Notes.md`).
- `Source/DSP/StripEq.h/.cpp` - the wrapper (sync, A/B, state, feeds).
- `Source/Standalone/EqWindowUI/` - graph, rail, analyser, match, presets.
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
