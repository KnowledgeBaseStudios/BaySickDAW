# Running Notes — QA-EqFlagship (boundless-banding-bandicoot)

> Append-only mid-batch artifact. A new `## YYYY-MM-DD — Task N — <name>`
> entry lands at every checkpoint (commit landed / sub-task verified /
> finding captured / spec call resolved / scope pivot). At batch close,
> `/draft-doc batch-close` reads this file as the primary input when
> compiling the single Implemented Work Log entry.

Pair file: `Plans & Specs/Batch Plans/boundless-banding-bandicoot.md`.
Convention: Main Plan §0, Batch Plans + Running Notes layout (locked
2026-05-11).

## 2026-08-27 — Task 0 — Batch open

Plan cut from the workshopped full-market competitive review
(`Research Reports/eq-flagship-competitive-review-2026-08-27.md` + the
feature matrix `eq-feature-matrix-2026-08-27.html`). All 26 workshop
rulings locked as W-1..W-26 in the plan's spec table; Jeff approved the
plan 2026-08-27. Handoff ruling recorded: the KBS handoff is written at
batch CLOSE against finished work. The QA-EqPro follow-up work this batch
builds on committed as `7654dba8` immediately before open.

## 2026-08-27 - Task 1 - 96 bands + paged chip row

kMaxBands 24 -> 96 through ONE new free constant (kbs::kEqMaxBands) so the
home-frequency law can read it; every consumer (StripEq, kEqBands,
kEqCacheSize, graph/rail/match loops) already chained off constants - zero
hardcoded 24s found in the sweep. Home-frequency law redistributed: bands
9..96 share the same 56 Hz-13.8 kHz log span the 24-band law covered,
parametric on the pool size so the KBS handoff inherits it. Chip row is now
paged: 24 chips per page, up/down arrows at the row's left (grey at the
ends, hidden entirely while only page 1 exists), a page appears only when
the previous fills or already holds an on-band, "+" fills the visible page
first and the row follows the band it makes, selection made on the graph
snaps the row to that band's page (only on selection CHANGE, so manual
browsing is never fought). Pages numbered, never lettered - A/B stays the
setup pill.

Two findings: (1) the harness quoting layer ate the backslash in a printf
passed through a bash heredoc - repaired by direct edit (known hazard,
recorded); (2) FIRST REAL 96-BAND COST: BaySickEqTests crashed at startup
with a stack overflow - main() holds ~22 engine locals across sibling
scopes and a 96-band ParametricEq is ~100KB, so MSVC's cumulative frame
reservation blew the default 1MB stack at the prologue probe. The PRODUCTS
heap-allocate every engine (graph nodes / plugin processor), so this is
test-harness-only; fixed with /STACK:8MB on the test target, reasoning in
CMakeLists. New EqTests section 14 (5 checks): pool size, home-frequency
monotonicity + audible-span bounds, band 96 boosts like any band, 95 idle
bands contribute exactly nothing. Suite green; build gate six zeros + four
link lines.

## 2026-08-27 - Task 2 - all-pass + continuous slopes (W-6)

EqType::allPass appended (unity magnitude by contract, RBJ second-order
phase rotation at freq/Q; linear modes flatten phase so there it
contributes nothing - stated in code, menu tooltip, and the rail seg).
Slope is now CONTINUOUS dB/oct: EqBandParams.slope int index -> float
(1..96, >=96.5 = Brickwall via kEqSlopeBrickwallDb); the old table survives
only as the band menu's nine detents. Realization: linear modes evaluate
the EXACT analytic fractional Butterworth (the FIR realizes the query, so
any dB/oct is exact incl. below 6); IIR modes decompose into integer-order
sections (existing machinery, user-Q at exactly 2 poles kept) plus a
6-pair staggered pole/zero ladder for the fractional remainder, and BOTH
the audio and the query build the same pairs statelessly - drawn == heard
by construction, pinned by test. Band-pass rounds to whole constant-peak
sections in the IIR (a partial section is not realizable) and gets exact
fractional skirts in linear - recorded here as the honest W-6 delivery
shape. Rail slope combo -> continuous SLOPE drag-number ("24/oct",
"Brick" at top, double-click types); rail type grid 3x3 for nine types;
graph menu detents write dB values.

Findings: (1) pure-fractional bands (slope < 6: no poles, no SVFs) were
silently skipped by the per-sample inclusion gate - heard 0 dB while the
query drew a filter; gate extended to ladCount (caught by the new
heard-equals-drawn check, which is exactly why that check exists).
(2) The ladder's KNEE is softer than the analytic fractional (-3 dB at
cutoff is not reachable with a pole ladder); ladder start tightened to a
quarter spacing and the test pins the asymptotic region - the knee
difference between IIR and linear modes for fractional slopes is real,
each mode draws its own truth, noted for the docs. (3) One deduction
error (wireSegment fallback) - trivial. EqTests section 15: 10 checks, all
green; suite green; build gate six zeros + four links (second run).

## 2026-08-27 - Task 3 - per-band phase + Mixed mode (W-9)

The complex-phase pipeline: EqLinearPhase gained designSpectrumComplex
(conjugate-symmetric complex sampling -> real IR, same rotate pipeline) with
setResponse / setResponseMatrix; ParametricEq gained EqBandParams.phaseMix
(0..1, 19th band param "Phase", full APVTS/blob/sweep/compare plumbing) and
EqMode::mixed (appended - stored mode values keep meaning; order-12 grid;
raised-cosine min-phase weight, fully natural below 150 Hz, fully linear
above 1.5 kHz). A band's excess phase = its own analytic minimum-phase
response (bandPhase, already computed and previously discarded), weighted by
phaseMix OR'd with the mode's per-frequency floor. The domain tables went
complex; the 2x2 matrix algebra holds verbatim (linear in the entries) and
the side-band mono cancellation is RE-PINNED EXACT with phase in play.
phaseAt now reports the blended excess phase in linear modes (post-PDC
truth) instead of a flat zero. Rail gained a PHASE drag-number (percent,
linear modes only); the mode menu gained Mixed Phase with its computed
latency. Latency unchanged by phase - recorded everywhere: blend moves
pre-ring, never delay.

Findings: (1) REAL DESIGN BUG caught by the new heard-equals-drawn checks -
the center-peaked Kaiser window is wrong for phase-blended IRs (min-phase
energy starts AT center and tails right; the Kaiser bump ate the tail - a
low bell lost 3 dB). Complex designs now use a flat window with
raised-cosine edge tapers; the pure-linear path keeps its proven Kaiser
(QA-EqPro pins untouched). (2) Mixed moved to the order-12 grid for LF
min-phase tails. (3) The 80 Hz check is pinned at 0.5 dB with the reason in
the test: ~11.7 Hz design bins under an 80 Hz bell is FIR resolution - the
class-wide linear-phase physics, not a phase-blend cost. (4) Engine size
measured: 223 KB at 96 bands (printed by section 14) - ~30 stack engines in
the test main() overflowed even 8 MB, test-exe stack now 32 MB (products
heap-allocate; the DAW's per-node cost 2x223 KB noted for the close docs).
(5) stdout now unbuffered in the harness so a crash cannot eat its own
evidence. EqTests section 16 (6 checks) green; full suite green; build gate
six zeros + four links first try.

## 2026-08-27 - Task 4 - two-way dynamics + onset (W-12)

Every band gained an independent BELOW-threshold stage (thresholdBDb /
ratioB / rangeBDb - signed like the above stage's Range; -60 threshold +
0 range = inert by default) and an onset-selective detection mix (0 = the
whole signal, 1 = attacks only). Below-stage semantics mirror the house
over-threshold model: engages by the SHORTFALL under thresholdB times the
ratioB slope, silence-gated, extent-walled by the distance from thresholdB
to the detector floor - so the same never-past-the-line guarantee holds in
both directions. Both stages sum into one gain target through the existing
chunk smoother; the dashed extent ghost now draws BOTH reachable
directions (bandExtentMagnitudeAt grew a direction argument, legacy shape
preserved for existing callers). Four new band params (23 suffixes now),
full APVTS/blob/compare/sweep plumbing, rail grew a second dynamics block
(THR-B / RAT-B / RNG-B / ONSET) with gestures, ids, captions, readouts.

Findings: (1) my first onset detector ran its fast/slow pair on the
rectified sample stream - a sustained 1 kHz tone rippled at carrier rate
and read as endless onsets (caught by the sustained-tone check); the pair
now runs symmetric one-poles (3 ms / 60 ms) on smoothed magnitude, and the
fast pole is deliberately slower than a carrier half-cycle - reasoning in
the code. (2) Exact-value GR pins were naive - the detector envelope of a
rectified sine sits a shade under peak by design; the deterministic pin is
the range CAP driven well past, and the tests now say so. EqTests section
17 (6 checks) green; suite green; build gate six zeros + four links first
try.

## 2026-08-27 - Task 5 - spectral dynamics (W-1) [long pole 1]

Per-band spectral mode shipped: in a linear mode a spectral band watches
the individual bins inside its footprint and moves only the ones standing
over their own spectral neighborhood. Architecture: EqLinearPhase grew
analyze/apply hooks into the frame (both input spectra pre-touch, gains
into each output spectrum pre-inverse); setting them routes processStereo
through the lockstep frame (which also learned the diagonal non-matrix
case). ParametricEq owns four pre-allocated spectral slots (footprint /
neighborhood / gain vectors sized at configureLinear - no audio-thread
allocation), a composite per-bin gain curve, and a Hann-CORRECTED detector
view computed in the frequency domain (the 3-tap kernel IS the Hann
convolution - the frames must stay rectangular for overlap-save, and
rectangular leakage smears the neighborhood; found by the two-tone check).
Neighborhood width is frequency-PROPORTIONAL (a flat bin count means two
octaves at 100 Hz and a sliver at 5 kHz); a footprint-energy level gate
stops a lone quiet harmonic being cut for having no neighbors; the house
threshold semantics hold (0 = never, -60 = any poke); per-bin travel is
capped by the band's range; mask timing floored at the frame rate (a mask
re-aiming every frame sprays modulation on footprint-mates - measured).
Spectral bands skip the whole-band dynamics in linear modes (no double
move) and the drawn static curve drops the stale whole-band GR. Two new
band params (Spectral / Density - 25 suffixes), Dynamic-submenu toggle
(arms dynamics with it), rail DENSE knob, the graph draws the live per-bin
move as a bright breathing line via spectralGrDbAt. Outside linear modes
the flag is inert and the band is its plain (dynamic) self.

The honest engineering record: realized tone-level cut lands near half the
per-bin cap (mainlobe dilution + mask smoothing + gain spreading), ~1 dB
collateral on a footprint-mate 27 dB down - the polish past this point is
the ear-tuning axis every spectral product lives on, and Density/attack/
release are exposed for exactly that; Jeff's smoke is the tuning pass.
EqTests section 18 (5 checks incl. untriggered-band exact latency and
inert-outside-linear) green; suite green; gate six zeros + four links
(one menu-scope compile fix on the way).

## 2026-08-27 - Task 6 - the character stage (W-3 + W-14)

NEW Source/DSP/Kbs/EqCharacter.h: two curves (odd-order cubic "Color A",
asymmetric tanh "Color B") + a difference mode, all normalized to unity
small-signal gain. Engine: program-dependent drive (10 ms follower backs
the color off as material gets loud), difference mode captures the dry
block at process entry and colors only what the EQ changed - it NULLS
BIT-NEAR on a flat EQ, pinned by test. Per-band saturation (W-14): a
band-passed slice at the band's freq/Q softened and folded back, per-band
Sat param + rail SAT drag-number. Two new bank globals (charmode/charamt)
ride the sweep (safe live setters, nothing reallocates), Color submenu in
the window menu (WORKING NAMES "Color A"/"Color B"/"Difference" pending
Jeff's pick - sub-spec 1, to be bundled with sub-specs 2 and 3 in one
stop). Blob carries both + the band sat; ALSO fixed a Task 3 straggler
found here: the blob's mode load clamped 0..6 and silently truncated Mixed
to Linear Maximum on project load.

DELIBERATE DEVIATION recorded: the plan line said "oversampled internally";
the shipped curves are low-order (cubic + soft tanh) whose added harmonics
sit low enough that aliasing is below audibility at these drives - stated
in the header. A heavier drive stage would need its own oversampler; if
Jeff's ear test wants more bite, that is the upgrade path. Also honest:
full-amount color on hot material compresses the fundamental ~2 dB -
that IS saturation; the level pin lives at the working 50% amount.
EqTests section 19 (6 checks incl. off-is-bit-exact and the difference
null) green; suite green; gate six zeros + four links (one suffix-table
static_assert + one blob call fixed on the way - the assert did its job).
