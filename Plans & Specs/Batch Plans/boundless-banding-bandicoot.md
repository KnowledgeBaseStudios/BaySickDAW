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
