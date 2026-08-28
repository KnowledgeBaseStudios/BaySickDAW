# QA-EqFlagship — The market-review buildout: 96 bands, spectral dynamics, phase pipeline, character, modulators, assisted match, cross-instance — Plan (boundless-banding-bandicoot)

Canonical path: `Plans & Specs/Batch Plans/boundless-banding-bandicoot.md`.
For execution: read Main Plan §0 + Carry-Forward + Implemented Work Log at
session open (Rule 1). Paired running notes:
`Plans & Specs/Running Notes/boundless-banding-bandicoot.md`.

## Context

**Why.** The 2026-08-27 full-market competitive review
(`Research Reports/eq-flagship-competitive-review-2026-08-27.md`) was
workshopped with Jeff item by item; every gap and opportunity got an explicit
ruling (26 rulings, ledger below). This batch implements the rulings so KBS
EQ Pro ships at $49 with no honest reviewer able to name a reason it isn't
the best product on the market. The DAW and the plugin share the engine and
the window code, so one implementation serves both.

**Handoff ruling (Jeff, 2026-08-27):** the KBS handoff document is written
at BATCH CLOSE, after the work exists - a full picture, not a concept. Until
then nothing here goes to the KBS tree. Plugin-ONLY work (7A in-plugin undo,
the 8B IPC transport) is specified in that handoff for the KBS session to
build; everything engine- and window-side is built HERE and proven in the
DAW first.

**Scale + risk.** Largest batch since QA-ModelShell; larger than QA-EqPro.
Two named long poles: Task 5 (spectral dynamics - genuinely new DSP) and
Task 10 (cross-instance - new infrastructure class). Bulk-run mode per the
standing ruling: per-task compile gate + EqTests always; ear verification
lands in the end-of-batch smoke + the test-plan campaign, not per task. All
mid-run spec calls still go to Jeff.

**Dependencies.** QA-EqPro (closed, c3c51e63) + the uncommitted follow-up
work (EQ Match mid/side port, dots/B-side fixes, resize floor, rail
reclaim+scroll, paint optimization + verified curve cache) which commits
BEFORE this batch opens - this plan builds on that tree.

**Effort.** 11 tasks + close. Tasks 1-4 and 6-9 are QA-EqPro-task-sized;
Task 5 and Task 10 are multiples of that.

## Spec calls already locked (the workshop ledger, 2026-08-27)

| ID | Decision | Reasoning |
|---|---|---|
| W-1 (1A) | Spectral dynamics as a per-band mode on bell/shelf bands, riding the linear-phase machinery, with a Density control | The generation-defining feature (Pro-Q 4 headline, soothe's category); mode-on-a-band beats a separate band type for flexibility |
| W-2 (2A) | Assisted layer = one-shot Auto Cleanup (listen, find problems, emit static + DYNAMIC bands as ordinary editable bands). Gullfoss-class continuous model → Future State | One-shot assembles from proven parts (analyzer variance, peak finder, match fitter); the continuous model is the largest DSP research risk in the codebase's history - parked, not killed |
| W-3 (3B) | Character stage: global 3-mode program-dependent saturation PLUS saturate-the-difference mode | Even Pro-Q added color; the difference-mode trick exists in no full parametric |
| W-4 (4A) | EQ Sketch: draw the curve, band types/slopes inferred from the stroke | Most approachable gesture for the never-made-music audience; composition over the band-create path |
| W-5 (5A) | Note-aware frequency: clickable piano strip (snap to note), note names in readouts, note-name text entry incl. cents | FL note-snaps free; "cut the E1 of the bass" is how the audience thinks |
| W-6 (6B) | All-pass band type AND continuous slopes (free dB/oct on all filter types, incl. fractional below 6) | All-pass exists in free ReaEQ; continuous slopes = the free-drag steepness Jeff confirmed he wants |
| W-7 (7A) | Full in-plugin undo/redo history + A/B copy both ways - PLUGIN-side, specified in the close handoff | Reviewers press Ctrl+Z; DAW already rides app undo. B→A copy lands here (shared window) |
| W-8 (8B-full) | Cross-instance: discovery + spectra sharing + collision/match-from-instance + instance browser + FULL-EDITOR re-point (view and edit any instance from any window) | The re-point gesture exists nowhere in the market (hosts can't open sibling windows; Pro-Q renders curve-rows, not editors); our resolver-driven window makes it natural. Longest pole, accepted |
| W-9 (9 A+B) | Complex-phase design pipeline: per-band linear↔minimum phase control, with global Mixed mode shipped as a preset on the same machinery | A falls out of B; both moats (Ozone per-band slider, Kirchhoff Mixed) for the price of one build. Latency note: mixed reduces LF pre-ring, never delay |
| W-10 (10A) | Loudness-matched band solo | Only DMG ($102+) has it; cheap on the SVF listen path |
| W-11 (11A) | Whole-curve transforms: global scale 0-200% incl. negative (invert), frequency shift, Match Amount | Nobody under $100; small work over the param model |
| W-12 (12B) | Two-way dynamics (independent Above + Below stages per band) + onset/transient-selective detection mix | Kirchhoff's moat, taken outright |
| W-13 (13B) | Minimal per-band modulators: one LFO + one envelope follower per band | Melda's moat, scoped to the useful half |
| W-14 (14A) | Per-band saturation folded into the character work | Same DSP stage, per-band routing |
| W-15 | Band cap 96, PRE-ALLOCATED (never grows on the audio thread); chip row pages of 24 with up/down arrows, unusable arrow greyed, a new page appears only when the previous fills, page 2+ hidden until used. Pages are named 1/2/3/4 - NEVER "A/B" (collision with the A/B setup pill). "+" and A/B pill stay fixed at the right | Jeff's paging UX adopted wholesale; true-unlimited rejected: growing engine storage under a running audio thread is either an audible settle or a second longest-pole redesign, and its payoff begins at band 97. 96 pre-allocated is indistinguishable from unlimited to any real user and triples the market's biggest number (32) |
| W-16 (16B) | Multi-band select (rubber-band drag) + group drag with proportional/absolute gain + PERSISTENT linked groups | Infinity's linked groups + Pro-Q's select, together |
| W-17 (17a) | Split band into identical L-only + R-only pair (band-menu item) | Engine already supports the result |
| W-18 (18a) | Delta listen: global (hear everything the EQ changes) + per-band | soothe made delta a standard; output-minus-input |
| W-19 (19A) | Stored named spectra (save/reuse analyzer snapshots), on the .kbsref machinery | Pro-Q parity |
| W-20 (20B) | A/B morph as a single slider between the A and B setups | Simpler than Melda's 4-corner pad; pairs with modulators |
| W-21 (21A) | DAW instance browser: every EQ in the project (18 buses + every strip, pre+post) browsable from one window, click-through to full editor - graph-backed | The 8B UI, backed by direct pointers; scope even Pro-Q can't match (it sees only its own instances; we see the whole console) |
| W-22 (22A) | Match track/selection scan (DAW-only): selection wins when one exists, else whole track; always reports what it scanned | The only-a-DAW-can Match source; resolves the KBS "source question" |
| W-23 (23A) | Domain audition: hold a view button, hear only that domain | Completes "the view IS the domain" |
| W-24 (24A) | Match-from-instance (falls out of W-8 spectra sharing) + user .kbsref import/export | Reference sharing between users |
| W-25 | SKIPS CONFIRMED: surround/Atmos, vintage-hardware model buffet, DMG-style reconfigurable UI, 117-bit precision marketing, sampled-convolution character | Recorded with reasons in the competitive review; settled |
| W-26 | Gullfoss-class continuous perceptual model → Future State entry (drafted at batch open, Jeff applies) | Jeff's explicit routing |

## Sub-spec calls surfaced for ExitPlanMode

1. **Character mode names** (Task 6): the three global modes need user-facing
   names (the reference product uses Clean/Subtle/Warm - generic words, but
   ours should be ours). Jeff picks from drafts at the task's verify point.
2. **Modulator exposure** (Task 7): do LFO rate/depth get automation lanes in
   the DAW (they are params) or stay lanes-excluded like mode/os? Surfaced at
   task start.
3. **Instance naming** (Task 10): what an instance calls itself in the
   browser when the host provides no track name. Surfaced at task start.

## Files to modify (per task; anchors named, line refs resolved at task start)

- **T1**: `Source/DSP/Kbs/ParametricEq.h` (kMaxBands, arrays), `Source/DSP/StripEq.h/.cpp` (kBands consumers, mCached/mSpare), `Source/PluginProcessor.h/.cpp` (kEqBands, kEqCacheSize, EqBandIds, registration + sweep), `Source/Standalone/EqWindowUI/EqRailView.h` (BandChipRow pager), `EqGraphView.h` (kBands consumers, curve cache arrays), `Tools/EqTests/main.cpp`.
- **T2**: `ParametricEq.h` (EqType::allPass, designBiquads continuous-order path, kEqSlopePoles retirement), `EqGraphView.h` + `EqRailView.h` (slope control → continuous), `Tools/EqTests`.
- **T3**: `Source/DSP/Kbs/EqLinearPhase.h` (complex designSpectrum variant), `ParametricEq.h` (rebuildLinearCurve complex composition, per-band phase param, minimum-phase derivation), `PluginProcessor.cpp` (new per-band Phase param + bank Mixed mode), `EqRailView.h` (phase control), `Tools/EqTests` (mono-cancellation re-pin with phase).
- **T4**: `ParametricEq.h` (BandRt dual gain computers, onset detector, detector mix), `PluginProcessor.cpp` (params: below-stage + onset mix), `EqRailView.h` (dynamics section grows), `Tools/EqTests`.
- **T5**: `EqLinearPhase.h` + `ParametricEq.h` (per-bin gain field on the linear bed, spectral detector, Density), `PluginProcessor.cpp` (spectral mode + density params), `EqGraphView.h` (spectral band drawing), `Tools/EqTests` (new section).
- **T6**: NEW `Source/DSP/Kbs/EqCharacter.h` (global stage + difference mode + per-band), `ParametricEq.h` (integration points), `PluginProcessor.cpp` (params), `EffectWindows.cpp` (menu), `Tools/EqTests`.
- **T7**: `ParametricEq.h` (per-band LFO + env follower, control-rate), `PluginProcessor.cpp` (params), `EqRailView.h` (mod controls), `Tools/EqTests`.
- **T8**: `EqGraphView.h` (sketch, multi-select, transforms, delta hooks, note snap), `EqRailView.h`, `EqAnalyser.h` (note readouts), `EffectWindows.cpp` (menu items, morph slider, domain audition on view row), `ParametricEq.h` (delta tap, loudness-matched listen gain), `PluginProcessor.cpp` (transform params).
- **T9**: `Source/DSP/Kbs/EqMatch.h` (Auto Cleanup fitter entry), `EqMatchPanel.h` (Cleanup button, scan, stored spectra, import/export, Amount), `Source/Standalone/BuilderPage.*` or clip-read path (track scan reader), `EffectWindows.cpp`.
- **T10**: NEW `Source/Standalone/EqWindowUI/EqInstanceBrowser.h`, `EqGraphView.h`/`EffectWindows.cpp`/`StandaloneEditor.cpp` (re-point plumbing, browser wiring, DAW resolver), spectra-share protocol spec (document, for the handoff's IPC half), `EqMatchPanel.h` (match-from-instance source).
- **T11**: Test plans, System Reference EQ.md, Callout Registry, Screenshot List, Verbatim Strings, manuals, Work Log, running notes; the KBS handoff document; Future State remains Jeff-applied.

## Tasks

### Task 0 — Plan open
- [ ] Commit this plan + running-notes stub. Brief one-liner; surface message + full git status; Jeff approves; commit.

### Task 1 — Foundations: 96 bands + paged chip row (W-15)
- [ ] `ParametricEq::kMaxBands` 24 → 96; all engine arrays stay `std::array` (pre-allocated, no audio-thread growth ever).
- [ ] Sweep every `kBands`/`kEqBands` consumer: StripEq caches + spare, processor param cache (`kEqCacheSize` grows ~4x, ~8MB - accepted in workshop), EqBandIds home-frequency law extended past 24 (log-spaced continuation), curve cache arrays, Match budget ceiling.
- [ ] Chip row pager: pages of 24; up/down arrows at the row's left; unusable direction greyed; page N+1 appears only once page N has an on-band; page 2+ hidden until used; chip numbers 25-96 (fitted text already handles digits); "+" adds on the CURRENT page's first free slot, else first free anywhere; "+"/A/B pill fixed at right. Pages labeled 1-4, never A/B.
- [ ] Lazy registration unchanged in shape - fresh build still registers zero EQ params.
- [ ] EqTests: band-96 addressing, home-frequency continuity, identity cost at 96 idle bands.
- [ ] Build gate + EqTests. Commit (surface + approval). Running notes.
- Tell Jeff (deferred to end smoke): (1) add bands past 24, watch page 2 appear; (2) arrows grey correctly at ends; (3) A/B pill unaffected by paging.

### Task 2 — Filter set: all-pass + continuous slopes (W-6)
- [ ] `EqType::allPass` (first-order + second-order-with-Q forms); phase display already shows its effect; magnitude query returns unity.
- [ ] Continuous slope: the slope param becomes continuous dB/oct (floor below 6 for fractional mastering tilts, ceiling 96 + Brickwall kept as the top detent). Non-integer orders realized by magnitude-interpolated design between adjacent integer-order cascades (linear path samples the exact fractional magnitude; IIR path blends adjacent orders).
- [ ] Slope UI: drag-number replaces the 9-step box; detents at the old values.
- [ ] EqTests: all-pass unity magnitude + phase shape; fractional slope monotonicity between integer neighbors; 3 dB/oct below-6 case.
- [ ] Gate + tests. Commit. Running notes.

### Task 3 — The complex-phase pipeline: per-band phase + Mixed (W-9)
- [ ] `EqLinearPhase::designSpectrumComplex` - accepts a complex per-bin response (magnitude + phase), same Kaiser/FIR back half.
- [ ] Minimum-phase derivation for each band's response (analytic from the biquad cascade - the engine already computes per-band phase; reuse `biquadResponse`'s phase output, currently discarded by magnitude callers).
- [ ] Per-band Phase param (0 = linear, 1 = minimum/natural, continuous): each band contributes magnitude with `phase = w_b * minPhase_b(f)`; composition multiplies complex responses; the 2x2 matrix entries become complex (the mono-cancellation algebra is phase-independent - re-pinned by test, not assumed).
- [ ] Global **Mixed** processing mode: the same pipeline with a frequency-weighted phase curve (minimum-phase character at LF, linear at HF) - a menu mode beside the five linear precisions.
- [ ] Record in code + docs: mixed/partial phase reduces LF pre-ring, never latency.
- [ ] EqTests: phase-blend endpoints equal existing modes bit-near; mono-cancellation EXACT with side bands at every blend; impulse latency unchanged.
- [ ] Gate + tests. Commit. Running notes.

### Task 4 — Dynamics II: two-way stages + onset mix (W-12)
- [ ] Each band gains an independent Below-threshold stage beside the existing Above stage (each with threshold/ratio/range sign semantics per the existing over-threshold model; extent bound extended to the pair).
- [ ] Onset mix 0-100%: a transient detector (fast/slow envelope ratio) blended into the band detector, so dynamics can target attacks only.
- [ ] Params + rail: dynamics section grows a Below row + onset knob (rail scroll from QA-EqPro absorbs the height).
- [ ] EqTests: above-only unchanged bit-near; below stage engages under threshold; onset mix at 100% ignores sustained tones.
- [ ] Gate + tests. Commit. Running notes.

### Task 5 — Spectral dynamics (W-1) [LONG POLE 1]
- [ ] Per-band spectral mode on bell/shelf: the band's region is watched per-bin (the linear bed already owns an FFT frame path); bins over threshold get individually attenuated within the band's footprint; **Density** sets how surgical (wide region ↔ narrow spikes).
- [ ] Spectral bands force a linear mode (documented, like the reference product); per-band GR reporting becomes per-band-peak for the meter.
- [ ] Graph: spectral bands draw their live per-bin attenuation inside the band fill.
- [ ] EqTests (new section): a two-tone signal where only the over-threshold tone is cut; the neighbor is untouched within tolerance; density extremes; latency unchanged from the host linear mode.
- [ ] Gate + tests. Commit. Running notes.

### Task 6 — Character: global + difference + per-band (W-3, W-14; sub-spec 1 to Jeff)
- [ ] Global output-stage saturation, 3 modes, program-dependent drive, oversampled internally; 4th mode: saturate-the-difference (color only what the EQ changed).
- [ ] Per-band saturation amount (gain-bearing types), same stage, band-footprint-scoped.
- [ ] Names to Jeff (sub-spec 1). Menu placement in the window menu.
- [ ] EqTests: null at zero amount; difference-mode nulls when the EQ is flat regardless of drive.
- [ ] Gate + tests. Commit. Running notes.

### Task 7 — Modulators (W-13; sub-spec 2 to Jeff)
- [ ] Per band: one LFO (rate/depth/target: freq|gain|Q) + one envelope follower (sens/depth/target), control-rate (the 32-sample dynamics tick), smoothed like dynamics coefficients.
- [ ] Params + serialization; rail exposure behind a MOD disclosure row.
- [ ] EqTests: LFO on freq sweeps between bounds; env follower tracks a burst; zero-depth = bit-near static.
- [ ] Gate + tests. Commit. Running notes.

### Task 8 — Workflow cluster (W-4, W-5, W-10, W-11, W-16, W-17, W-18, W-20, W-23)
- [ ] EQ Sketch: draw mode - stroke → inferred bands (position → type, steepness → slope/Q); plain Undo is the escape.
- [ ] Note-aware: clickable piano strip (click = snap selected/new band to note), note names in readouts, note-name text entry ("A4", "C#2+13").
- [ ] Multi-select: rubber-band + Ctrl-click; group drag; proportional/absolute gain scale; persistent linked groups (serialized in the View tree).
- [ ] Split-to-L/R band-menu item.
- [ ] Delta listen: global (out-minus-in tap) + per-band variant on the listen path.
- [ ] Loudness-matched band solo (listen gain rides the loudness follower).
- [ ] Whole-curve transforms: Scale (0-200%, negative inverts), Freq Shift; menu + two params.
- [ ] A/B morph slider (A↔B parameter interpolation, one gesture).
- [ ] Domain audition: hold ST/MID/SIDE button = hear that domain only.
- [ ] EqTests where testable (transforms: scale -100% inverts gains exactly; shift moves freqs log-exactly).
- [ ] Gate + tests. Commit. Running notes.

### Task 9 — Match cluster (W-2, W-19, W-22, W-24 + Match Amount)
- [ ] Auto Cleanup: one button - averaging + variance capture over a few seconds, resonance/problem detection (the findPeakNear machinery generalized), emits static cuts for constant problems and DYNAMIC bands for intermittent ones, as ordinary editable bands, one undo step, tally reported.
- [ ] Match Amount (rides the W-11 scale machinery): apply N% of a fit.
- [ ] Track/selection scan (DAW): offline read of the strip's timeline audio (selection wins; reports "scanned: <what>, <length>"); feeds mid+side capture like live capture.
- [ ] Stored named spectra: save/load analyzer snapshots (.kbsref-adjacent format), Match source menu.
- [ ] User reference import/export (file-level; the format already exists).
- [ ] EqTests: Auto Cleanup on a synthetic resonance emits a cut at the resonance ±tolerance; steady vs swinging problems split static/dynamic (the existing 12c machinery reused).
- [ ] Gate + tests. Commit. Running notes.

### Task 10 — Cross-instance + the browser + re-point (W-8, W-21, W-24 freebie) [LONG POLE 2]
- [ ] Re-point plumbing: the window's resolver becomes retargetable at ANY EQ point (it already is, per-strip, in the DAW - formalize the switch + title/state handling; rail/graph/menus follow automatically; A/B, presets, Match route through the same target).
- [ ] DAW instance browser (`EqInstanceBrowser`): every EQ point in the project (buses + strips, pre+post), name + live mini-spectrum + curve thumbnail; click = re-point the window (or open the sibling's window - both gestures offered); collision reference pick feeds the existing collision view.
- [ ] Match-from-instance source (DAW: direct; plugin: via the shared channel).
- [ ] Spectra-share protocol SPEC (document): discovery, shared-memory layout, spectrum frames, edit-command channel, parameter-write semantics on the remote (host automation/undo correctness) - the handoff's IPC half, written against the working DAW-side implementation.
- [ ] EqTests where testable; browser is smoke-verified.
- [ ] Gate + tests. Commit. Running notes.

### Task 11 — Close: docs + the handoff
- [ ] Test plan: new §B section (EQ-Flagship scenarios incl. MUST-PASS set).
- [ ] System Reference EQ.md rewrite for the new surface; Callout Registry; Screenshot List (RE-SHOOT set); Verbatim Strings (all new UI strings).
- [ ] Manuals: EQ topics rewritten; regenerate; reprint PDFs.
- [ ] **The KBS handoff document** (per Jeff's ruling: written NOW, against finished work): everything vendored-file-shipped + the plugin-only spec (7A in-plugin undo + A/B copy-both-ways; the 8B IPC transport implementing the Task-10 protocol spec; instance naming sub-spec resolution).
- [ ] /review-batch → fix blockers → Work Log entry (held per R2) + running notes close.
- [ ] Final commit (surface + approval). Report to Jeff with the open verification set.

## Verification (end-to-end smoke)

Deferred per bulk-run to close: (1) 96-band paging walk; (2) continuous
slope drag incl. fractional; (3) per-band phase blend audibly and on the
phase display; (4) two-way dynamics + onset on a drum loop; (5) spectral
band on a harsh vocal; (6) character modes null test by ear; (7) modulator
wobble; (8) sketch a curve; (9) note-snap a bass cut; (10) Auto Cleanup on
a boxy guitar; (11) track scan + match; (12) browser walk + re-point +
collision pick; (13) delta listen A/B; (14) morph slider sweep; (15) the
full QA-EqPro §B.38 regression walk stays green.

## Routing notes (Rule 3)

Findings in THIS batch's scope: fix in-batch. Pre-existing findings: route
per Rule 3 (§9 Forks for closed batches). The Future State entry for the
Gullfoss-class model is drafted at batch open for Jeff to apply. Skips
(W-25) are settled - do not re-raise. Outstanding QA-EqPro tail items
(B.38 walk, EQ figure re-shoots, Main Plan §5/§6 + Future State drafts in
the QA-EqPro plan's routing notes, the Work Log R2 question) remain open
Jeff-side and are unaffected by this batch.

## Carry-Forward Reference touch points

Carry-Forward predates this entire subsystem (snapshot 2026-05-07); its EQ
sections describe the deleted EQ8 world and were superseded at QA-EqPro.
Read §Mixer/§Graph topology notes at Task 10 start (bus/strip enumeration
for the browser); nothing else applies. Contradictions found → running
notes, per the standing convention.

## Held Work Log entry (R2)

Per the plan's close checklist the Implemented Work Log entry is HELD on the
open R2 question.  The compiled entry is parked here VERBATIM (drafted at
close by /draft-doc batch-close, review outcome filled); when Jeff clears R2
it moves to the Work Log unchanged apart from the header timestamp.

### 2026-08-27 23:31 PT — QA-EqFlagship — (boundless-banding-bandicoot) The market-review buildout: all 26 workshop rulings W-1..W-26 shipped - 96-band pre-allocated pool with a paged chip row; all-pass + continuous dB/oct slopes (heard == drawn pinned by test); per-band phase blend + Mixed mode on a complex design pipeline; two-way dynamics + onset-selective detection; per-band spectral dynamics on the linear frames; the character stage (EqCharacter.h - two colors, saturate-the-difference, per-band Sat); per-band LFO + envelope modulators with full automation lanes; the nine-feature workflow cluster (Sketch, note-aware piano strip, multi-select + linked groups, split L/R, delta listen, loudness-matched solo, whole-curve transforms, A/B morph, domain audition); the match cluster (SpectrumScan.h, Auto Cleanup, track/selection scan, stored spectra, Match Amount); cross-instance (window re-point, EqInstanceBrowser, match/collision-from-instance, the spectra-share IPC spec); EqTests grown to 128 checks; KBS handoff written at close per Jeff's ruling

**Bucket:** Effects, UI / L&F / Theming, Cross-cutting Infrastructure

#### Done

- **Task 0 - open (`526b009c`).** Plan + running-notes stub cut from the workshopped 2026-08-27 full-market competitive review (`Research Reports/eq-flagship-competitive-review-2026-08-27.md` + the feature matrix, committed with the plan). All 26 rulings locked as W-1..W-26 in the plan's spec table incl. the W-25 skips (surround/Atmos, hardware-model buffet, reconfigurable UI, precision marketing, sampled-convolution character - settled) and the W-26 routing (Gullfoss-class continuous model to Future State, Jeff applies). Three sub-spec calls staged for mid-batch resolution (character names, modulator lane exposure, instance naming). Handoff ruling: the KBS handoff document is written at batch CLOSE against finished work - nothing goes to the KBS tree before then. Largest batch since QA-ModelShell, larger than QA-EqPro; two named long poles (Task 5 spectral dynamics, Task 10 cross-instance); bulk-run mode (per-task compile gate + EqTests always, ear verification at the end-of-batch smoke). The uncommitted QA-EqPro follow-up tree this batch builds on committed immediately before open as `7654dba8`.
- **Task 1 - 96 bands + paged chip row, W-15 (`5084a246`).** kMaxBands 24 -> 96 through ONE new free constant (`kbs::kEqMaxBands`) so the home-frequency law can read it; the consumer sweep found zero hardcoded 24s (everything already chained off constants). Home-frequency law redistributed: bands 9..96 share the same 56 Hz-13.8 kHz log span the 24-band law covered, parametric on the pool size so the KBS handoff inherits it. Chip row paged: 24 chips per page, up/down arrows at the row's left (greyed at the ends, hidden while only page 1 exists), a page appears only when the previous fills or holds an on-band, "+" fills the visible page first, graph selection snaps the row to that band's page on selection CHANGE only (manual browsing never fought). Pages numbered 1-4, never lettered - A/B stays the setup pill. EqTests section 14 (5 checks: pool size, home-frequency monotonicity + audible-span bounds, band 96 boosts like any band, 95 idle bands contribute exactly nothing).
- **Task 2 - all-pass + continuous slopes, W-6 (`5fbe2b19`).** `EqType::allPass` appended (unity magnitude by contract, RBJ second-order phase rotation; linear modes flatten phase so there it contributes nothing - stated in code, tooltip, and rail seg). Slope became CONTINUOUS dB/oct (float 1..96, >= 96.5 = Brickwall via `kEqSlopeBrickwallDb`; the old nine-step table survives only as band-menu detents). Realization: linear modes evaluate the EXACT analytic fractional Butterworth (the FIR realizes the query, any dB/oct exact incl. below 6); IIR modes decompose into integer-order sections plus a 6-pair staggered pole/zero ladder for the fractional remainder, and BOTH the audio and the query build the same pairs statelessly - drawn == heard by construction, pinned by test. Band-pass rounds to whole constant-peak sections in IIR (a partial section is not realizable) and gets exact fractional skirts in linear - the honest W-6 delivery shape, recorded. Rail slope combo -> continuous SLOPE drag-number ("24/oct", "Brick" at top, double-click types); rail type grid 3x3. EqTests section 15 (10 checks).
- **Task 3 - per-band phase + Mixed mode, W-9 (`4c50e0d1`).** The complex-phase pipeline: `EqLinearPhase::designSpectrumComplex` (conjugate-symmetric complex sampling -> real IR, same rotate pipeline) with `setResponse`/`setResponseMatrix`; `EqBandParams.phaseMix` (0..1, 19th band param "Phase", full APVTS/blob/sweep/compare plumbing); `EqMode::mixed` appended (stored mode values keep meaning; order-12 grid; raised-cosine min-phase weight, fully natural below 150 Hz, fully linear above 1.5 kHz). A band's excess phase = its own analytic minimum-phase response (bandPhase, already computed and previously discarded) weighted by phaseMix OR the mode's per-frequency floor. The domain tables went complex; the 2x2 matrix algebra holds verbatim (linear in the entries) and the side-band mono cancellation is RE-PINNED EXACT with phase in play. `phaseAt` now reports the blended excess phase in linear modes (post-PDC truth) instead of a flat zero. Rail PHASE drag-number (percent, linear modes only); mode menu gained Mixed Phase with its computed latency. Latency unchanged by phase - recorded everywhere: blend moves pre-ring, never delay. EqTests section 16 (6 checks).
- **Task 4 - two-way dynamics + onset, W-12 (`9c7ae9b5`).** Every band gained an independent BELOW-threshold stage (thresholdB/ratioB/rangeB, signed like the above stage; -60 + 0 range = inert by default) mirroring the house over-threshold model - engages by the SHORTFALL under thresholdB times the ratioB slope, silence-gated, extent-walled by the distance to the detector floor, so the never-past-the-line guarantee holds in both directions - plus an onset-selective detection mix (0 = whole signal, 1 = attacks only). Both stages sum into one gain target through the existing chunk smoother; the dashed extent ghost draws BOTH reachable directions (`bandExtentMagnitudeAt` grew a direction argument, legacy shape preserved). Four new band params (23 suffixes), rail second dynamics block (THR-B/RAT-B/RNG-B/ONSET). EqTests section 17 (6 checks).
- **Task 5 - spectral dynamics, W-1, long pole 1 (`9488f8c7`).** Per-band spectral mode: in a linear mode a spectral band watches the individual bins inside its footprint and moves only the ones standing over their own spectral neighborhood. `EqLinearPhase` grew analyze/apply hooks into the frame (both input spectra pre-touch, gains into each output spectrum pre-inverse); setting them routes `processStereo` through the lockstep frame, which also learned the diagonal non-matrix case. `ParametricEq` owns four pre-allocated spectral slots sized at `configureLinear` (no audio-thread allocation), a composite per-bin gain curve, and a Hann-CORRECTED detector view computed in the frequency domain (the 3-tap kernel IS the Hann convolution - frames must stay rectangular for overlap-save, and rectangular leakage smears the neighborhood). Neighborhood width frequency-PROPORTIONAL; a footprint-energy level gate stops a lone quiet harmonic being cut for having no neighbors; house threshold semantics hold; per-bin travel capped by the band's range; mask timing floored at the frame rate (per-frame re-aiming sprays modulation on footprint-mates - measured). Spectral bands skip whole-band dynamics in linear modes (no double move); outside linear modes the flag is inert. Two new band params (Spectral/Density - 25 suffixes), Dynamic-submenu toggle, rail DENSE knob, live per-bin move drawn as a breathing line via `spectralGrDbAt`. EqTests section 18 (5 checks incl. untriggered-band exact latency and inert-outside-linear).
- **Task 6 - the character stage, W-3 + W-14 (`acf1b0b9`).** NEW `Source/DSP/Kbs/EqCharacter.h`: two curves (odd-order cubic, asymmetric tanh) + a difference mode, all normalized to unity small-signal gain; program-dependent drive (10 ms follower backs the color off as material gets loud); difference mode captures the dry block at process entry and colors only what the EQ changed - NULLS BIT-NEAR on a flat EQ, pinned by test. Per-band saturation (W-14): a band-passed slice at the band's freq/Q softened and folded back, per-band Sat param + rail SAT drag-number. Two new bank globals (charmode/charamt) ride the sweep (safe live setters, nothing reallocates); Color submenu in the window menu. Blob carries both + band sat. EqTests section 19 (6 checks incl. off-is-bit-exact and the difference null).
- **Task 7 - per-band modulators, W-13 (`e674ce43`).** Every band grew an LFO (Rate 0.02-20 Hz, Depth, target F/G/Q) and an envelope follower (bipolar Depth, target F/G/Q) - five new band params, 31 suffixes total. The modulators live in the control tick and FOLD INTO THE EXISTING GLIDE TARGETS (no second smoothing system): LFO phase advances per kDynChunk, env is chunk-mean |detector| smoothed 30 ms and soft-normalized, offsets compose as freq x 2^(0.5 x sum), gain + 6 dB x sum, Q x 2^sum. modActive gates the whole block (zero depths = zero cost, pinned bit-near static). SCOPE RULE stated in the header and enforced: modulators are IIR-MODES-ONLY - a linear-phase FIR is a periodically-rebuilt snapshot, and an LFO wobbling the design target would rebuild the kernel at frame rate for a wobble quantized to frame boundaries; in linear/mixed modes the depths are inert. Rail MOD block (RATE/LFO/ENV knobs + two F/G/Q target rows); `CurveKey.modOn` makes the drawn curve breathe in IIR modes only. THE THREE SUB-SPEC RULINGS applied this task: 1a - color names ship as None/Smooth/Warm/Changed ("Changed" is Jeff's word, not "Change Only"); 2a - modulators get FULL automation lanes, not menu-only; 3c - plugin instance naming auto-number + renameable, recorded for Task 10. EqTests section 20.
- **Task 8 - the workflow cluster, W-4/5/10/11/16/17/18/20/23 (`26569f55`).** Nine features, engine first. Whole-curve transforms (W-11): two bank globals (curvescale -200..+200%, curveshift -24..+24 st) riding the sweep through ONE pair of xfFreq/xfGain helpers that every design AND query reads (audio, FIR designer, and drawn curve cannot disagree); handles stay at the user's set points. Delta listen (W-18): out-minus-in with a latency-alignment ring sized once at prepare (enable edge clears it); per-band delta = delta composed with Isolate. Loudness-matched solo (W-10): paired 200 ms power followers toward program level, +-24 dB cap, cuts as well as boosts. Domain audition (W-23): atomic EqChannel collapse at the output tail, driven by a 300 ms hold on the MID/SIDE SegmentRow (quick click still switches views). Multi-select (W-16): rubber-band + Ctrl+click, companions ride drags as a shape (freq as ratio, gain as offset, Alt = proportional), persistent linked groups serialized in the view tree. Split to Left + Right (W-17) via writeBandParams (all 31 fields). EQ Sketch (W-4): menu-armed stroke -> greedy peel in octave/dB space (stroke width reads Q, edge-holding peaks become shelves), one undo step. Piano strip clickable (W-5): click snaps/births a band at the note, FREQ caption doubles as live note name, freq field accepts note-name entry (A4, Bb2, C#2+13). A/B morph strip (W-20) in the chip row: drag blends the live bank toward the spare from a gesture-start snapshot (freq/Q log, gain/slope/place linear, discrete snap at halfway), thumb springs back, one undo step. EqTests section 21 (8 checks: exact -100% inversion, log-exact +12 st shift, flat-EQ delta null, delta of a +6 bell = the added part, mono side-audition silence, mid = the tone, matched solo within 2 dB).
- **Task 9 - the match cluster, W-2/19/22/24 + Match Amount (`6015d639`).** NEW `Source/DSP/Kbs/SpectrumScan.h` (JUCE-free, tested): the offline capture accumulator - 4096-point Hann frames at half-overlap, mid + side dB averages on the match grid plus per-point swing (the same std-dev statistic the fitter already eats), pushed at render speed with no feed ring and no dropped frames. `EqMatch::cleanup` (W-2): resonance detection with NO reference - the capture matched against its own ~1-octave broad-stroke self, CUTS only, 2 dB poke floor, Q floored at 1; the existing spread rule (12c) sends swinging problems to dynamic bands. Track/selection scan (W-22): StripEq grew an atomic pre-EQ scanTap (armed only around an offline session - live audio thread suspended, no race; fires on the identity path too); the editor wires runTrackScan into every EQ window - one offline pass over the timeline (selection wins as Section, else Song, Tail::Cut) behind the heavy-op overlay with cancel + progress, enterOfflineRender/runOfflineLoop/leaveOfflineRender exactly per the freeze discipline; reports "Scanned: selection, 0:42" per the ruling. Panel: Scan Track/Selection; Stored Spectra... (save Current or Reference under a name, load any as reference - .kbsref XML, match-grid mid+side CSV + hasSide, in `Presets/EQ/References` via UserFileSave + SafeXml; W-19 save/reuse and W-24 file import/export are the same files); AMOUNT slider (0-200%, "N% of the fit") scaling every gain and dynamic range at apply - Match and Auto Cleanup both honor it; Auto Cleanup ADDS its cuts over the existing curve (a correction, not a replacement), one undo step, tally reported ("Cleanup: 3 cuts, 1 dynamic"). EqTests section 22 (13 checks).
- **Task 10 - cross-instance + browser + re-point, W-8/21/24, long pole 2 (`26066688`).** Re-point plumbing (W-8): `EffectEqWindow::retargetTo(channelId, pre)` - the window members de-consted, param block ensured on the new target, `EqGraphView::retarget` swaps prefix + bank and drops everything stale (selection, multi-select, listen latch, link cache, cached curve rows, spectrogram); every control follows for free because ids derive from prefix + bank and the resolver runs per use; the aux window keeps its BIRTH key - re-pointing is a view change, not a re-registration; `onOpenOtherEq` carries the CURRENT channel so a re-pointed window's tabs open the sibling of what it shows. NEW `EqInstanceBrowser.h` (W-21): every EQ point via the SAME enumerator the Effects page's channel dropdown uses (the lists can never disagree), pre + post rows filtered by live resolve; each row = name + PRE/POST tag + curve thumbnail off the engine's own magnitudeAt + live mini-spectrum (visible rows only); click re-points THIS window, double-click / row menu opens the point's own window, row menu also sets Match Reference and Collision Reference; rows hold (channelId, pre) only - satellite discipline, resolve per use. Match-from-instance (W-24): `EqMatchPanel::setReferenceInstance` reads the picked instance's postFeed through a per-tick resolver; collision-from-instance via `EqGraphView::altCollisionFeed`. The IPC half: `Files For Claude/KBS Spec - EQ Spectra Share Protocol.md` (v1, gitignored) written against the working DAW implementation as the reference model - Local\KbsEqShare_v1 shared memory, 64 slots, SpectrumFeed-style seqlock frames on the EqMatch 240-pt grid (the .kbsref wire format), heartbeat reaping at 2 s, instance naming per sub-spec 3c, edit-command SPSC ring with a CAS grant, and the non-negotiable host-correctness rule (remote edits apply through the target's OWN beginEdit/performEdit/endEdit - the plugin twin of our applicators-write-the-parameter rule). Plugin transport is KBS-session work; the doc is the handoff's IPC half.
- **Task 11 - docs close + the handoff (the close commit this entry rides in).** Test plan §B.39 (EQF-1..EQF-18, five MUST-PASS); `Plans & Specs/System Reference/EQ.md` updated to the flagship surface; Callout Registry EQ/EQB rows (EQ now 15 callouts, EQB 8); Screenshot List RE-SHOOT scope grown + one NEW master (`EQ Instances.png`); Verbatim Strings EQ section rewritten; manual m2 EQ/EQB + m3 IMP-14/15/16 updated + marker coords + manual regenerated (91 figures / 733 markers / 89 topics) + all three PDFs reprinted; THE KBS HANDOFF written now, against finished work, per the open ruling - `Files For Claude/KBS Handoff - EQ Flagship (QA-EqFlagship).md` (gitignored, beside the IPC spec) carrying the vendored-file ship set + the plugin-only spec (7A in-plugin undo/redo + A/B copy both ways, the 8B IPC transport implementing the protocol spec, the 3c naming resolution); running notes close; this Work Log entry.

#### Found along the way

- **T1 - the first real 96-band cost:** BaySickEqTests crashed at startup with a stack overflow - main() holds ~22 engine locals across sibling scopes and a 96-band ParametricEq is ~100 KB, so MSVC's cumulative frame reservation blew the default 1 MB stack at the prologue probe. Test-harness-only: the PRODUCTS heap-allocate every engine (graph nodes / plugin processor).
- **T2 - pure-fractional bands (slope < 6: no poles, no SVFs) were silently skipped** by the per-sample inclusion gate - heard 0 dB while the query drew a filter. Caught by the new heard-equals-drawn check, which is exactly why that check exists.
- **T2 - the IIR ladder's knee is softer than the analytic fractional** (-3 dB at cutoff is not reachable with a pole ladder) - a real IIR-vs-linear difference for fractional slopes; each mode draws its own truth.
- **T3 - REAL DESIGN BUG caught by the heard-equals-drawn checks:** the center-peaked Kaiser window is wrong for phase-blended IRs (min-phase energy starts AT center and tails right; the Kaiser bump ate the tail - a low bell lost 3 dB).
- **T3 - engine size measured: 223 KB at 96 bands** (printed by section 14) - ~30 stack engines in the test main() overflowed even the T1 8 MB stack; the DAW's per-node cost is 2 x 223 KB (noted for the docs).
- **T4 - the first onset detector rippled at carrier rate** (fast/slow pair on the rectified sample stream - a sustained 1 kHz tone read as endless onsets; caught by the sustained-tone check); and the exact-value GR pins were naive (a rectified sine's detector envelope sits a shade under peak by design).
- **T5 - the honest spectral record:** realized tone-level cut lands near half the per-bin cap (mainlobe dilution + mask smoothing + gain spreading), ~1 dB collateral on a footprint-mate 27 dB down - the polish past this point is the ear-tuning axis every spectral product lives on, and Density/attack/release are exposed for exactly that; Jeff's smoke is the tuning pass.
- **T6 - Task 3 straggler:** the blob's mode load clamped 0..6 and silently truncated Mixed to Linear Maximum on project load.
- **T6 - DELIBERATE DEVIATION recorded:** the plan line said "oversampled internally"; the shipped character curves are low-order (cubic + soft tanh) whose added harmonics sit low enough that aliasing is below audibility at these drives - stated in the header. A heavier drive stage would need its own oversampler; if Jeff's ear test wants more bite, that is the upgrade path. Also honest: full-amount color on hot material compresses the fundamental ~2 dB - that IS saturation; the level pin lives at the working 50% amount.
- **T8 - Task 6 straggler, found + fixed in passing:** menu id 182 was BOTH the Color submenu's "Warm" and "Reset All Bands" - the color branch ate the click, so Reset All silently set the color to Warm and never reset.
- **T8 - the loudness-match first clamp was boost-only** - the raw band-pass slice peaks at Q, so a Q=2 solo sat +6 dB hot; found by test.
- *(The pre-batch findings that shaped the batch - every market gap and opportunity - live in the competitive review + the plan's W-1..W-26 spec table; the W-25 skips are settled there with reasons.)*

#### What was done about each finding

- **T1 stack overflow** - /STACK:8MB on the test target only, reasoning in CMakeLists; products unaffected. Same commit.
- **T2 skipped fractional bands** - inclusion gate extended to ladCount. Same commit.
- **T2 ladder knee** - ladder start tightened to a quarter spacing; the test pins the asymptotic region; the knee difference recorded for the docs. Same commit.
- **T3 Kaiser-vs-complex** - complex designs now use a flat window with raised-cosine edge tapers; the pure-linear path keeps its proven Kaiser (QA-EqPro pins untouched). Same commit.
- **T3 engine size** - test-exe stack to 32 MB; stdout unbuffered in the harness so a crash cannot eat its own evidence. Same commit.
- **T4 onset ripple + naive pins** - the pair now runs symmetric one-poles (3 ms / 60 ms) on smoothed magnitude, the fast pole deliberately slower than a carrier half-cycle (reasoning in code); the deterministic pin is the range CAP driven well past, and the tests say so. Same commit.
- **T5 spectral depth** - shipped as the honest v1 with the tuning controls exposed; the ear-tuning pass rides Jeff's smoke.
- **T6 blob mode clamp** - fixed in T6 (`acf1b0b9`).
- **T6 character deviation** - recorded in the header + here; the oversampler is the named upgrade path if the ear test wants more bite.
- **T8 menu-id 182 collision** - Reset All Bands moved to id 166, fixed in T8 (`26569f55`) at the moment of finding.
- **T8 boost-only clamp** - the match now CUTS as well as boosts, +-24 dB both ways. Same commit.

#### `/review-batch` outcome

Ran at close (batch-code-reviewer, diff 7654dba8..26066688 + the docs tree). NO BLOCKERS. Two NEEDS-FIX: a stale "All 24 bands" line in the rewritten EQ.md (fixed pre-commit) and confirmation that the W-26 Gullfoss Future State draft reached Jeff (it was provided in chat at the workshop; re-attached to the close report so it cannot be lost). Four nits, all fixed pre-commit: en-GB spellings + an em-dash in new comments, a dead double-apply of charMode/charAmt in StripEq's state load, and sketch mode surviving a window re-point. Reviewer independently re-ran the test exe (128 PASS) and verified the six-zero build log, the whole menu-id space, satellite discipline at every new seam, and the value-change guards. Verdict: READY-TO-COMMIT. Both gates re-run green after the fixes.

#### Carry-forward contradictions

- None found. Carry-Forward predates this entire subsystem (snapshot 2026-05-07); its EQ sections describe the deleted EQ8 world and were superseded at QA-EqPro. Its §Mixer/§Graph topology notes were read at Task 10 start for the browser's bus/strip enumeration - no contradiction, and the browser deliberately rides the Effects page's own live channel enumerator rather than any snapshot of the topology.

#### Diagnostic Instrumentation Catalog

- NONE added this batch. Nothing to strip.

#### Files touched

- **Task 1:** `Source/DSP/Kbs/ParametricEq.h`; `Source/PluginProcessor.h`; `Source/Standalone/EqWindowUI/EqRailView.h`; `Tools/EqTests/main.cpp`; `CMakeLists.txt` (test-target /STACK).
- **Task 2:** `Source/DSP/Kbs/ParametricEq.h`; `Source/DSP/StripEq.cpp`; `Source/PluginProcessor.cpp`; `Source/Standalone/EqWindowUI/EqGraphView.h` + `EqRailView.h` + `EqPresets.h`; `Tools/EqTests/main.cpp`.
- **Task 3:** `Source/DSP/Kbs/EqLinearPhase.h` + `ParametricEq.h`; `Source/DSP/StripEq.cpp`; `Source/PluginProcessor.h/.cpp`; `Source/Standalone/EffectWindows.cpp`; `Source/Standalone/EqWindowUI/EqGraphView.h` + `EqRailView.h`; `Tools/EqTests/main.cpp`; `CMakeLists.txt` (32 MB test stack).
- **Task 4:** `Source/DSP/Kbs/ParametricEq.h`; `Source/DSP/StripEq.cpp`; `Source/PluginProcessor.h/.cpp`; `Source/Standalone/EqWindowUI/EqGraphView.h` + `EqRailView.h`; `Tools/EqTests/main.cpp`.
- **Task 5:** `Source/DSP/Kbs/EqLinearPhase.h` + `ParametricEq.h`; `Source/DSP/StripEq.cpp`; `Source/PluginProcessor.h/.cpp`; `Source/Standalone/EqWindowUI/EqGraphView.h` + `EqRailView.h`; `Tools/EqTests/main.cpp`.
- **Task 6:** NEW `Source/DSP/Kbs/EqCharacter.h`; `ParametricEq.h`; `Source/DSP/StripEq.h/.cpp`; `Source/PluginProcessor.h/.cpp`; `Source/Standalone/EffectWindows.cpp`; `Source/Standalone/EqWindowUI/EqGraphView.h` + `EqRailView.h`; `Tools/EqTests/main.cpp`.
- **Task 7:** `Source/DSP/Kbs/ParametricEq.h`; `Source/DSP/StripEq.cpp`; `Source/PluginProcessor.h/.cpp`; `Source/Standalone/EffectWindows.cpp`; `Source/Standalone/EqWindowUI/EqGraphView.h` + `EqRailView.h`; `Tools/EqTests/main.cpp`.
- **Task 8:** `Source/DSP/Kbs/ParametricEq.h`; `Source/DSP/StripEq.h/.cpp`; `Source/PluginProcessor.h/.cpp`; `Source/Standalone/EffectWindows.cpp`; `Source/Standalone/EqWindowUI/EqGraphView.h` + `EqRailView.h`; `Tools/EqTests/main.cpp`.
- **Task 9:** NEW `Source/DSP/Kbs/SpectrumScan.h`; `Source/DSP/Kbs/EqMatch.h`; `Source/DSP/StripEq.h/.cpp`; `Source/Standalone/EffectWindows.h/.cpp`; `Source/Standalone/EqWindowUI/EqMatchPanel.h`; `Source/Standalone/StandaloneEditor.cpp`; `Tools/EqTests/main.cpp`.
- **Task 10:** NEW `Source/Standalone/EqWindowUI/EqInstanceBrowser.h`; `Source/Standalone/EffectWindows.h/.cpp`; `Source/Standalone/EqWindowUI/EqGraphView.h` + `EqMatchPanel.h`; `Source/Standalone/StandaloneEditor.cpp`; NEW `Files For Claude/KBS Spec - EQ Spectra Share Protocol.md` (gitignored).
- **Task 11 / close:** `Plans & Specs/Test Plans/v1-master-test-plan.md` (§B.39); `Plans & Specs/System Reference/EQ.md` + `Callout Registry.md` + `MANUAL-1 Screenshot List.md` + `Verbatim Strings.md`; `Manuals/` (m2 EQ/EQB + m3 IMP-14/15/16 pages, `assets/marker-coords.py`, regenerated `manual.html`, three PDFs reprinted); NEW `Files For Claude/KBS Handoff - EQ Flagship (QA-EqFlagship).md` (gitignored); the batch plan + running notes; this file.

#### Commit(s)

`526b009c` (Task 0 - open), `5084a246` (T1), `5fbe2b19` (T2), `4c50e0d1` (T3), `9c7ae9b5` (T4), `9488f8c7` (T5), `acf1b0b9` (T6), `e674ce43` (T7), `26569f55` (T8), `6015d639` (T9), `26066688` (T10), plus the close commit this entry rides in (T11 docs + handoff + review fixes). Every task closed BOTH gates: `run_eq_tests.bat` green + `do_build.bat` six exit codes 0 + four link lines (T2 on the second run; T5, T6, and T10 each after one compile fix caught by the gate; all others first try). EqTests grew to 128 checks (new sections 14-22 over the QA-EqPro-era suite), and the full suite was re-run green on every task's tree including the final one. The two handoff artifacts (`KBS Spec - EQ Spectra Share Protocol.md`, `KBS Handoff - EQ Flagship (QA-EqFlagship).md`) live gitignored in `Files For Claude/` and appear in no commit by design. Per bulk-run mode the ear verification is NOT per-task: the end-of-batch smoke (plan file, Verification section - 15 scenarios) + the §B.39 walk are pending with Jeff.

#### Next action

- **Jeff's end-of-batch smoke:** the plan's 15-scenario Verification walk (incl. the spectral-band ear-tuning pass - Density/attack/release are the exposed tuning axis - and the character-mode null test by ear), plus the §B.38 regression walk staying green and the new §B.39 (EQF-1..EQF-18, five MUST-PASS). Debug first, then Release.
- **EQ figure re-shoots (Jeff, needs the running app):** the RE-SHOOT scope grew again this batch + the one NEW master (`EQ Instances.png`); marker coords re-measure after.
- **The KBS handoff:** deliver `KBS Handoff - EQ Flagship (QA-EqFlagship).md` + the IPC spec to the KBS session - plugin-only work is 7A in-plugin undo/redo + A/B copy both ways, the 8B IPC transport implementing the protocol spec, and the 3c instance naming.
- **W-26 Future State entry** (Gullfoss-class continuous perceptual model): draft provided to Jeff in the workshop chat + re-attached in the close report; Jeff applies.
- **Main Plan §5/§6 close-out** for QA-EqFlagship (STATUS + pointer + arrow): Jeff applies.
- **The QA-EqPro open tail** (B.38 walk, EQ figure re-shoots now merged into this batch's set, §5/§6 + Future State drafts, the Work Log gap routing) remains open Jeff-side, unaffected by this batch.

## Drafts for Jeff (routing notes appendix, per the QA-EqPro convention)

**Main Plan section 5 entry draft (Jeff applies):**

> **QA-EqFlagship** (boundless-banding-bandicoot) - CLOSED 2026-08-27
> pending the end-of-batch smoke. The market pass over the QA-EqPro
> engine: all 26 workshop rulings shipped - 96-band pool, continuous
> slopes + All Pass, per-band phase + Mixed Phase, two-way/onset/spectral
> dynamics, the color stage + per-band saturation, per-band modulators,
> the nine-feature workflow cluster, the match cluster (Auto Cleanup,
> track scan, stored spectra, Match Amount), and cross-instance (browser
> + window re-point + the IPC spec). EqTests 128 checks. Work Log entry
> parked in the batch plan pending R2. Handoff artifacts in Files For
> Claude (gitignored): the KBS handoff + the spectra-share protocol spec.

**Main Plan section 6 arrow draft (Jeff applies):** QA-EqPro ->
QA-EqFlagship -> (next batch per Jeff's sequencing; the EQ figure
re-shoots + the B.38/B.39 walks ride the Master Test Plan campaign).

**W-26 Future State entry draft (Jeff applies; re-draft of the workshop
chat text, provided again at close because the review found no durable
copy):**

> **FS-EQ-1 - Gullfoss-class continuous perceptual model (AQ).** A
> continuously self-adjusting EQ layer driven by a computational model of
> auditory masking: the analyzer estimates which regions mask which,
> and a smooth correction curve rebalances them in real time (the
> Soundtheory Gullfoss class - Recede/Tame as user intents over a
> perceptual objective, not per-band settings). Parked at the 2026-08-27
> workshop (ruling W-2/W-26): the one-shot Auto Cleanup shipped instead,
> assembled from proven parts; the continuous model is the largest DSP
> research risk in the codebase's history - a psychoacoustic masking
> model, a stability story for a curve that follows its own output, and
> ear-tuning on the scale of a product, not a batch. Prerequisites if
> revived: the spectral-dynamics frame machinery (shipped at
> QA-EqFlagship) is the natural host; the masking model and the
> objective function are the research. Parked, not killed.
