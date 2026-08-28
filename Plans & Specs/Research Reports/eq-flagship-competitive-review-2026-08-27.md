# EQ Flagship Competitive Review - 2026-08-27

The full-market architecture review Jeff requested for taking KBS EQ Pro to
market at $49: our EQ against every major EQ, what they do that we don't,
what we do that nobody does, and the complete opportunity list.

**Method.** One agent inventoried our shipped feature set from code + docs
(not memory); four researched the market in parallel with 170 web fetches,
each claim marked VERIFIED (official product page/manual actually fetched)
or REPORTED (reviews/snippets). Products covered: FabFilter Pro-Q 4,
Kirchhoff-EQ, sonible smart:EQ 4 + pure:EQ, Soundtheory Gullfoss, oeksound
soothe2/soothe3 + bloom, iZotope Neutron 5 + Ozone 11 (EQ + Match), TDR Nova
/ Nova GE / SlickEQ M, MeldaProduction MDynamicEq / MAutoDynamicEq, DMG
EQuilibrium + EQuick, Sonnox Oxford, Softube Weiss EQ1, Crave EQ, Newfangled
EQuivocate/Elevate, Pulsar Massive, Slate Infinity EQ 2, Voxengo PrimeEQ +
CurveEQ, Waves F6 + H-EQ, Harrison AVA, Wavesfactory Spectre + Equalizer,
ToneBoosters Equalizer 4, DDMF IIEQ Pro, Blue Cat Liny EQ, Techivation M-EQ,
Maag EQ4, Acustica Sand, PSP preQursor3, Mastering The Mix FUSER, and the
stock baseline (Ableton EQ Eight, Logic Channel EQ, Cubase Frequency 2, FL
PEQ2, Studio One Pro EQ3, ReaEQ). Raw per-product teardowns preserved in the
session scratchpad; this report is the synthesis.

**Prices as of today** (the market's real prices are sale prices): Pro-Q 4
$199. soothe3 $259. DMG EQuilibrium $238. Gullfoss $199. Weiss EQ1 EUR 549.
Kirchhoff-EQ $149 list / $79 on its perpetual rotation. Infinity EQ 2 $149.
smart:EQ 4 $129 / $79 sale. EQuick $102. Ozone 11 Std ~$99 street. Nova GE
EUR 60. Crave EQ $69. Waves F6 ~$30 perpetual sale. TB Equalizer 4 EUR 35.
Harrison AVA exactly $49. Free: Nova, ReaEQ, Ozone EQ module, every stock EQ.

---

## Part 1 - The verdict up front

The honest review-site sentence we should be engineering toward:

> "It's Pro-Q 4 minus spectral dynamics and the Instance List, plus four
> things not even Pro-Q does, at a quarter of the price."

Today we are genuinely close to that sentence. We already match or beat the
field on: band count parity with Pro-Q (24), more linear-phase precision
options than anyone but FabFilter, a dynamics model with per-band GR
metering, mid/side done better than anyone (Part 2), an EQ Match that is
architecturally ahead of everyone including FabFilter (Part 2), an analyzer
at Pro-Q's maximum published resolution (8192 - theirs is 1024-8192
selectable), spectrum grab (only Pro-Q and Kirchhoff have it), collision
view (only Pro-Q, Neutron, and FUSER have it), and a published proof suite
(nobody has one).

What stands between us and the sentence is a short Tier-1 list (Part 3):
spectral/resonance dynamics, a character stage, draw-to-EQ, note-aware
frequency entry, all-pass + continuous slopes, plugin-side undo history, and
- strategically biggest for our audience - an assisted "make it better"
layer. Everything on that list has a known reference implementation in the
market and nothing on it invalidates our architecture; most of it composes
onto the engine we already have.

---

## Part 2 - What WE do that THEY don't (and whether to keep it)

These are verified against the official documentation of every competitor.
Each is a marketing bullet nobody else can print. Verdict on all seven:
keep, and say them loudly.

### 2.1 Per-band mid/side/left/right that survives linear phase in ONE stage
The market's dirty secret. VERIFIED positions: **Pro-Q 4** supports M/S
placement in linear phase, but mixing L/R bands with M/S bands runs TWO
cascaded convolution stages and **doubles the latency** (their manual says
so). **Cubase Frequency 2**: enabling linear phase on a band kills that
band's dynamics. **Kirchhoff**: undocumented. **Weiss EQ1** (EUR 549):
linear phase and dynamics are mutually exclusive architectures. **Melda**:
linear phase only in the resampler. Ours: one 2x2 matrix convolver, every
domain, one latency, always. Nobody else has this design at any price.

### 2.2 Dynamics that keep working in EVERY linear-phase precision
Pro-Q 4 allows dynamic bands in linear phase only up to its High resolution
- Very High and Maximum disable them with a warning (VERIFIED, manual).
Kirchhoff silently degrades dynamic bands to minimum phase. Weiss makes you
choose. Ours ride minimum-phase deltas on the linear bed at all five
precisions including Maximum. Combined with 2.1 this is the "linear phase
with no asterisks" story.

### 2.3 The three-view domain model
Every competitor exposes mid/side as a per-band picker (Pro-Q, Kirchhoff,
Infinity, Crave) or a global dual-curve switch (EQ Eight, Ozone). Nobody has
views where the view IS the domain, with the other domains ghosted live
behind it. For an audience that has never made music, "the Side view shows
what's happening on the sides" beats "set this band's channel parameter" by
a mile. Unique, and it was Jeff's design.

### 2.4 EQ Match that matches in mid/side and can produce DYNAMIC bands
VERIFIED across the field: Ozone's Match is an 8,000-band linear-phase curve
(stereo only, not onto editable bands). Melda matches onto editable bands
(stereo only). Pro-Q 4's Match makes ordinary bands (stereo only). CurveEQ
has M/S + match but as a drawn spline. Nova GE has Static/Dynamic Match but
global-channel only. **Nobody matches mid and side from one band budget with
a published stereo-vs-separate decision rule, and nobody's match can decide
a problem is intermittent and emit a dynamic band for it.** Both are ours,
both are tested, and the dynamic-detection one is a genuine first.

### 2.5 The 14-genre mid+side reference library
sonible ships learned profiles; iZotope ships target curves; nobody ships
measured per-genre mid AND side spectra feeding an M/S match. Ships spectra,
never audio - no licensing exposure.

### 2.6 Measured auto-gain
Pro-Q 4's manual admits its Auto Gain is "an educated guess based on the
current EQ settings, NOT a dynamic process based on measured levels."
Ours measures - it watches its own input and output and rides the output so
loudness actually holds, with an Amount control. TDR and DMG have
loudness-compensated gain too, so this is "better than Pro-Q," not unique -
but "our auto-gain measures; theirs guesses" is a printable line.

### 2.7 The engine-truth display + the proof suite
Every curve pixel is the engine's own arithmetic - there is no drawing-side
formula to drift, and 65 assertion sites pin the engine's claims (identity,
decramped Nyquist shapes, linear-phase flatness, impulse-exact latency, the
extent bound, match round-trip). Nobody in this market publishes a test
suite. "The graph cannot lie - and we can prove it" is a trust story no
competitor can copy quickly, and the never-made-music audience needs trust
more than features.

Also ours-and-rare (not unique): Natural Phase decramping at zero latency
(Pro-Q has it; FL's HQ+ approximates it; nobody else), the drawn dynamic
extent line derived from the same expression that bounds the gain computer
(nobody draws a bound the curve provably cannot cross), and in the DAW the
4-slot per-band sidechain picker (only stock Cubase exceeds it, with 8; no
paid plugin offers more than one external SC source).

---

## Part 3 - What THEY do that WE don't

### Tier 1 - close these to claim "no reason it wouldn't be the best"

**3.1 Spectral dynamics / resonance suppression.** The category-defining
feature of this generation. Pro-Q 4's headline (per-frequency dynamic
attenuation inside one band, Density + Tilt controls); soothe2/3 built a
$200-259 product on nothing else; Waves' new Curves line opens with a
resonance suppressor; Kirchhoff has a Sword filter type + onset-selective
detection; Nova GE and Melda do one-shot auto-deresonate. We have the
de-resonator dynamic notch (good) but nothing continuous/multi-frequency.
This is the single biggest gap a reviewer will name. Recommendation: DO IT -
as a per-band "spectral" mode like Pro-Q's, not a separate product. Overlaps
Future State CL-250. The honest engineering note: this is the one Tier-1
item that is real DSP work, not composition - Pro-Q pays for it by forcing
linear phase on spectral bands, which our overlap-save machinery already
provides a natural home for.

**3.2 An assisted layer.** For OUR audience this may matter more than 3.1.
The market has three tiers (verified): continuous adaptive (Gullfoss's
two-knob perceptual unmasking, smart:EQ 4's profile-driven smart:filter),
continuous problem-specific (soothe), and one-shot assists (Nova GE's Smart
Operations, Melda's auto-EQ + resonance removal, Neutron's EQ Learn
auto-node-placement). One-shot assists are now table stakes - a $60 TDR
plugin does auto-match AND auto-deresonate. We have Match and spectrum grab
but no "press this and it helps" button. Recommendation: DO the one-shot
tier now (we own most of the parts: the analyzer's averaging + variance,
findPeakNear, the match fitter - an "Auto Cleanup" that finds resonances and
drops pre-aimed dynamic cuts is largely assembled from existing pieces), and
treat a continuous Gullfoss-class model as a v2 flagship feature. Overlaps
CL-031/032.

**3.3 A character/saturation stage.** Pro-Q 4 added Clean/Subtle/Warm and
reviewers treated it as validation that even the surgical king needs color.
Techivation puts per-band saturation in a linear-phase EQ; Spectre built a
whole product on saturating only the EQ difference; Nova's "+" quality modes
add internal nonlinearity; TB Equalizer 4 has per-band analog modeling at
EUR 35. We are 100% clean. Recommendation: DO a global Clean/Subtle/Warm
first (output-stage, program-dependent, cheap); consider per-band saturation
later - Spectre's saturate-the-difference trick is the interesting variant
and nobody has it in a full parametric. No brand names anywhere per standing
rule.

**3.4 EQ Sketch / draw-to-EQ.** Pro-Q 4's most demoed v4 feature; EQuivocate
has Draw Curve; Wavesfactory Equalizer lets you draw the effect-weighting
curve. For never-made-music users, drawing the shape you want is the single
most approachable gesture an EQ can offer. We have nothing. Recommendation:
DO IT - infer band type from stroke position and slope from steepness,
exactly the Pro-Q model. Overlaps CL-261. Composition work over our existing
band-create path, not new DSP.

**3.5 Note-aware frequency.** Pro-Q 4 accepts "A4" and even "C#2+13" (cents)
as frequency input and quantizes bands to notes in piano view; FL PEQ2
note-snaps free; Waves H-EQ picks frequencies from a piano. We draw a piano
strip but it is decoration - no snap, no note entry, no note readout on the
band. Recommendation: DO IT - it is cheap, and for our audience "cut the E1
of the bass" is how they think. Make the piano strip clickable, add note
names to the readouts and text entry.

**3.6 All-pass band type + continuous slopes.** All-pass exists in Pro-Q 4,
Kirchhoff, DMG, and FREE ReaEQ - its absence is a checkbox loss to a free
plugin. Continuous/fractional slopes (Pro-Q 4 continuous everything incl.
3 dB/oct fractional; Kirchhoff 0-96 continuous; Infinity continuous on all
types) vs our 9 steps. Recommendation: DO all-pass (trivial - one more
EqType, the biquad already exists in every DSP textbook and phaseAt already
draws phase); continuous slopes are engine surgery with modest payoff -
consider, lower priority, the 9 steps + Brickwall cover practice.

**3.7 Plugin-side undo/redo history + A/B copy.** Kirchhoff made in-plugin
Ctrl+Z history a famous feature; Pro-Q 4 has per-gesture undo; soothe2 has
per-state undo with A>B/B>A copy. In the DAW we ride the app-wide
UndoManager (done); the PLUGIN currently has no undo story of its own.
Recommendation: MUST-DO for the plugin before market - a reviewer WILL
Ctrl+Z, and A/B without copy-both-ways reads unfinished. (Our A/B has Copy
A-to-B + Lock; add copy-the-other-way.)

**3.8 Per-band external spectrum / cross-instance awareness (plugin).**
Pro-Q 4's Instance List (control every instance from one window, collision
against a designated reference track) and smart:EQ 4's 10-instance group
unmasking are the two strongest cross-instance stories; Neutron's Masking
Meter is detection-only. In the DAW we already have the real thing (4
receive lines feed the collision view - that IS a masking meter against any
track). The PLUGIN has one SC bus and no instance sharing. Recommendation:
for the plugin, spectrum-sharing between instances (see each other's post
spectra, collision against a chosen instance) is the high-value half of the
Instance List at a fraction of its scope; full remote-control-everything is
v2. This one is genuinely significant engineering (shared-memory IPC) - do
not schedule it casually.

### Tier 2 - the ownable "nobody under $100 has it" list

From the premium sweep, features that at REGULAR price exist only above
$100 (Kirchhoff's perpetual $79 sale noted). We already own the first two.

1. Dynamics in linear phase - **we ship it** (2.2). Nobody at ANY price.
2. Per-band M/S surviving linear phase single-stage - **we ship it** (2.1).
3. **Variable per-band phase** - Ozone 11's per-band 0-100% linear-to-min
   slider; Kirchhoff's Mixed mode (min-phase LF, linear HF, per-frequency
   morph); DMG's free phase. Nothing under $100. Recommendation: a global
   Mixed mode is the achievable form for us (split the FIR's design between
   min-phase low half and linear high half); per-band free phase is DMG-tier
   engine work - defer.
4. **Loudness-compensated auto-listen** (DMG only): band solo whose level is
   loudness-matched so soloing doesn't shout. Cheap on our SVF listen path.
5. **Global response transforms** (EQuick Range/Shift; Ozone's Amount
   0-200%): scale the whole curve, invert it, shift its frequency. Tiny
   implementation over our param model; big "power user" feel; Match Amount
   (apply 70% of the match) falls out of the same control.
6. **Two-way thresholds + onset-selective dynamics** (Kirchhoff only): an
   Above stage AND a Below stage per band, plus transient-only detection.
   Our over-threshold model is one direction per band (sign of Range).
   Recommendation: consider a per-band transient/sustain detector mix (the
   Neutron transient/sustain channel mode is the same insight); two full
   stages per band is Kirchhoff's moat and chasing it is v2.
7. **Band-parameter modulators** (Melda only: LFO/env/pitch-follow on band
   params). Niche pro feature; SKIP for v1 - our DAW-side automation lanes
   already cover the musical use.
8. **Per-band saturation** (Techivation $129) - see 3.3.

### Tier 3 - worth having, cheap, checkbox-level

- **Band count**: we're at Pro-Q parity (24); Kirchhoff/DMG/PrimeEQ run 32,
  Infinity unlimited. The engine's kMaxBands is a constant; raising to 32
  is mostly UI (chip row density). Consider at leisure.
- **Multi-band select + group edit** (Pro-Q rubber-band select; Infinity's
  persistent Linked Groups; EQuick's control linking). We have single-band
  gestures only. The rubber-band select + proportional gain drag is the
  useful 80%.
- **Split band into L+R pair** (Pro-Q 4's Split button) - one menu item for
  us; our engine already supports the result.
- **Solo behavior for cuts** (Pro-Q solos what's being REMOVED on
  cut/notch bands - i.e. delta). soothe made delta a workflow standard. We
  have band-pass Listen; add a delta variant (hear what the EQ removes).
  Cheap: output minus input.
- **A/B morph** (Melda's A-D X-Y morph) - skip; gimmick outside sound
  design.
- **Analyzer freeze-compare / stored spectra** (Pro-Q stores reference
  spectra for reuse) - we have Freeze; storing named spectra piggybacks on
  the .kbsref machinery.
- **Waterfall/phase-plot analyzer variants** (Studio One waterfall, ReaEQ
  phase plot) - we already draw phase; spectrogram covers waterfall's job.

### Explicit SKIP list (with reasons, so they stay decided)

- **Surround/Atmos** (Pro-Q 9.1.6, Melda ambisonics): wrong audience, wrong
  product stage; the standalone DAW is stereo.
- **Vintage hardware model buffet** (Kirchhoff's 32 models, DMG's circuit
  list, Pulsar/Acustica emulations): enormous effort, crowded lane,
  brand-name minefield, and against our "the graph cannot lie" identity.
  The Character stage (3.3) is our answer to color.
- **117-bit precision marketing** (Kirchhoff): 64-bit double is audibly
  transparent; competing on this number is marketing-by-spec-sheet.
- **Sampled convolution character** (Acustica): CPU-hostile, alien to the
  architecture.
- **Full DMG-style reconfigurable UI**: our one-modifier-one-meaning map is
  a feature, not a limitation.

---

## Part 4 - The $49 case

**The price landscape has a hole exactly where we're aiming.** Below us:
free/stock (no matching, no collision, no linear-phase-with-no-asterisks
anywhere in stock), F6 at $30 (dynamic EQ commodity, nothing else), TB Eq4
at EUR 35 (deep but unknown-brand utilitarian), Harrison AVA at exactly $49
(a 31-band graphic - not a feature competitor). Above us: Nova GE EUR 60
(no linear phase, 6 bands), Crave $69 (no dynamics, no match), Kirchhoff's
$79 sale floor (no match, no collision, no cross-instance, dynamic bands
drop to min-phase), then the $100-260 tier. **Nothing at or below $49
offers: EQ match of any kind + per-band dynamics + true linear phase +
per-band M/S + collision view.** We offer all five today, before Tier 1.

**The composite stock baseline is the real enemy** (a Cubase owner already
has per-band dynamics, 8 external sidechains, per-band M/S, per-band linear
phase for $0), and our clean beats against it are exactly the Part 2 list:
their linear phase kills their dynamics per band, ours doesn't; nobody
stock has matching, a reference library, collision, spectrum grab, or an
assisted anything.

**Suggested marketing spine** (each line is verifiable, which is the
point):
1. Linear phase with no asterisks - your mid/side bands AND your dynamic
   bands keep working at every precision. (They can check every
   competitor's manual; we did.)
2. The only EQ that matches in mid/side - and knows when a problem is
   intermittent and makes the band dynamic.
3. The view is the domain - see the sides, work the sides.
4. Auto-gain that measures instead of guessing.
5. The graph cannot lie - every pixel is the engine's own arithmetic, and
   the test suite that proves it ships with the product.
6. $49. Not a sale price.

**What must be true before the sentence in Part 1 holds:** 3.1 (spectral),
3.3 (character), 3.4 (sketch), 3.5 (notes), 3.6 (all-pass), 3.7
(plugin undo) - plus 3.2's one-shot tier to own the beginner story no
premium EQ bothers to tell. 3.8 (cross-instance) is the one Pro-Q feature
we should concede at v1 and answer with "your DAW's version has the real
thing built in."

---

## Part 5 - Bookkeeping

- **Future State overlaps** (existing entries this report touches, for
  Jeff's routing): CL-250 (spectral dynamics), CL-261 (EQ sketch),
  CL-031/032 (adaptive/resonance), CL-129 (unmask - FUSER's auto
  phase-rotation half is NOT covered by it), CL-248/249 (AIR/SUB bands -
  the Maag angle), BLU-255 (match - now shipped).
- **Corrections to things we might have believed:** "PSP splendEQ" does not
  exist (their current flagship parametric is preQursor3, 4 bands,
  character-first). Kirchhoff's "psychoacoustic frequency detection" did
  not verify as a named feature (the verified name is Psychoacoustic
  Adaptive Filter Topologies - an internal filter-fitting detail). Pro-Q 4
  has no headphone/binaural preview (rumor conflates band solo + surround
  speaker select). Pro-Q 4 has NO user-facing oversampling control at all.
- **Stale doc note:** EQ.md says "54 checks"; the suite is at 65 assertion
  sites after the close-review fixes.
- **Verification caveats:** Logic Channel EQ and Studio One Pro EQ3 are
  REPORTED end-to-end (Apple/Fender pages are JS shells). Gullfoss is
  REPORTED (vendor site is a JS app). Kirchhoff's M/S-in-linear-phase
  behavior is undocumented - if we ever print a direct comparison against
  them on that axis, buy a license and measure it first.
- Prices are 2026-08-27 snapshots; Waves/PA/Melda "regular" prices are
  fiction - their sale prices are the market price.
