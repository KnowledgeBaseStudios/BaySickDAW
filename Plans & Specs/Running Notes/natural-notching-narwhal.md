# Running Notes — QA-EqPro (natural-notching-narwhal)

> Append-only mid-batch log. A new entry goes in at EVERY checkpoint: a commit
> landing, a sub-task verified, a finding captured, a spec call resolved, a
> scope pivot. Per `feedback_draft_doc_running_notes_every_checkpoint.md`,
> capture as it happens rather than reconstructing at the end. At batch close
> `/draft-doc batch-close` reads this file as the primary input for the single
> Implemented Work Log entry.

**Paired plan file:** `Plans & Specs/Batch Plans/natural-notching-narwhal.md`
**Convention:** Main Plan section 0, "Batch Plans + Running Notes layout
(locked 2026-05-11)".

---

## 2026-08-26 - Batch open (plan cut; work not started)

The batch came out of Jeff's instruction to upgrade the DAW EQ to everything
KBS EQ Pro now has, reviewing the three handoff docs (`Files For Claude/EQ
Build Notes.md` / `EQ Fixes.md` / `EQ Future Updates.md`) and the KBS source
in DAW context. The workshop ran in this session: an eight-agent review
(three KBS readers, three DAW readers, two adversarial verifiers) plus
first-hand verification of every load-bearing claim.

Findings that shaped the rulings (details + file:line in the plan):
- The DAW has REAL full-graph delay compensation; the linear engine's wrong
  latency report (B1/B2) therefore actively misaligns buses today - worse
  than the standalone review assumed.
- B4 confirmed real on exactly two paths (EQ options menu, page-preset
  import incl. clipboard paste); every load-shaped path already shielded.
- C3 (per-band routing ignored by the linear path) is NOT fixed in the KBS
  engine despite the build-notes claim - verified in source; fix-back
  recorded as a plan task.
- All 18 bus strips' params register eagerly at startup (9,792 EQ params =
  ~600 KB of every project file; Display Project measured) - the blunt shape
  of the BLU-447 routing fix, not a requested design. Jeff ruled the grain
  split (routing eager, EQ lazy).
- The legacy per-audio-row EQ node was never actually removed (three live
  creation callers); Jeff ordered the removal into the batch.
- No export leading-latency trim exists (verified against the recent export
  work, which was rate-fix + normalize + session ownership only).

All fourteen spec calls locked in the plan's table (Jeff's rulings
2026-08-26), including the late addendum: the window keeps our dark look,
not the KBS brushed-plate; the look is one-way (KBS keeps theirs).
Four sub-spec calls open at plan cut (mode-param automation handling, A/B
storage location, user preset folder, band view-move gesture).

Work NOT started - waiting on Jeff's batch-start.

## 2026-08-26 - The four open sub-spec calls resolved (Jeff)

1a / 2A / 3a / 4a, locked into the plan as SC-15..SC-18: mode/oversampling
params carry state + undo but are EXCLUDED from automation and apply through
the shielded message-thread path; A/B stays DSP-side (spare bank serializes
with each EQ point, button beside the "+" chip); user EQ presets in
Documents\BaySickDAW\Presets\EQ; band view-move via right-click "Move to
Mid / Side / Stereo view" (settings kept, domain rewritten). The plan's
sub-spec section now reads no-calls-open. Work still not started - waiting
on Jeff's batch-start.

## 2026-08-26 - Task 1 + Task 2 - engine vendored, extended, proven

Task 1 (534cd609): the ten KBS Core files vendored into Source/DSP/Kbs
verbatim (kbs namespace kept; Feeds.h added to the plan's nine - the
spectrum-feed/analyser dependency).  Extensions, both fed back as reference:
- SC-3 per-domain linear phase: EqLinearPhase gained a 2x2 matrix mode
  (designSpectrum refactored out of setMagnitude with a clamp flag - cross
  terms are SIGNED; setMagnitudeMatrix designs LL/LR/RL/RR; processStereo
  runs both channels in lockstep and convolveMatrixFrame mixes both
  transforms into both outputs with the same one-position rotation).
  ParametricEq::rebuildLinearCurve detects any non-stereo active band, fills
  five per-bin domain-product tables (allocated at configureLinear - rebuilds
  run on the audio thread), composes HLL=((m+s)/2)*st*l etc., and falls back
  to the old single-spectrum path when everything is stereo.  Composition
  order fixed as L/R-then-M/S, documented in the code: band-index order
  cannot be honoured across the two groups by one frequency-domain design.
- SC-4 sidechain slots: EqBandParams.scSource (-1..3), setSidechainSlot
  copies per block like setSidechain, detector picks slot > scExternal >
  internal, slot validity cleared per block.
Task 1's build gate is app-only by nature (headers not yet included
anywhere); real compile proof was Task 2's job, one commit later.

Task 2: Tools/EqTests/main.cpp - the KBS test_core.cpp harness
(check/near/levelAt) + parametric EQ sections 10-11 ported verbatim, plus
new section 12: C3 regressions (side-band-in-linear leaves mono untouched
EXACTLY - the matrix design is linear so the mono sum of LL+LR is the
designed identity; mid leaves pure side; left/right split; mixed domains;
matrix-path impulse latency to the sample) and SC-4 (slot 2 drives, slot 1
does not).  CMake target BaySickEqTests (EXCLUDE_FROM_ALL - do_build.bat's
six-exit-code gate contract untouched) + run_eq_tests.bat (two-exit-code
log contract).  FIRST RUN: 54 checks, all passed - including the two
extensions.  The engine compiles clean under MSVC in our tree.

## 2026-08-26 - Task 3 - StripEq wrapper + graph swap (gate green first try)

New Source/DSP/StripEq.h/.cpp: one kbs::ParametricEq per bank behind the
DSPBase interface.  Identity fast path keeps the feeds alive (EQ8MsDSP
convention) and REFUSES to short-circuit whenever latency > 0 - the B2 fix
by construction.  pushBand = full-struct compare then engine setBand;
pushGlobals for the five sweep-driven globals; setMode/setOversampling are
the shielded config actions (SC-15); DSP-side A/B spare kept (SC-16,
params-only swap, engine re-pushed); resetToDefaults = the load-boundary
slate; state blob tag "StripEq" (old EQ8MsDSP blobs simply do not parse -
SC-14's reset falls out of the tag check); 4-slot SC forward into the
engine's copied slots (SC-4 wrapper half); kbs::SpectrumFeed pre/post taps.

Graph swap: InstrChannelNode + InsertNode carry StripEq (bulk type rename
through BaySickGraph + EffectsPage incl. the 44 getters and resolveChannelDsp
/ preEqForChannelId); chainLat and save/restore blobs ride the DSPBase
virtuals unchanged.  updateEQFromCache is a DELIBERATE NO-OP until Task 4
(old mid/side x 8 param layout has no mapping onto the new engine);
resetEqStatesToDefaults now sweeps StripEq::resetToDefaults per EQ point.
EffectEqWindow binds its inert fallback EQ8MsDSP (window alive but dead
until Tasks 5-6, recorded as the planned mid-batch state).  Old EQ8 files
still compile beside the new world; deletion lands with Task 6.

## 2026-08-26 - Task 4 - the parameter rework (gate green on retry; one
## visibility fix)

The single-set scheme is live: ids {prefix}_{eq_|preeq_}b{N}{Suffix} (18
suffixes matching EqBandParamSlot: Channel/Place/Isolate/AutoRelease in,
Solo/Upward and the mid/side dimension out) + 7 bank globals
({prefix}_{eq_|preeq_}{mode|os|propq|autogain|agamt|outgain|polarity}).
Defaults are the plugin's rulings verbatim: bands 1-8 ship on-and-flat
(identity-skipped so it costs nothing), Threshold 0 dB, home frequencies
40..12500 + log-spaced 9-24, Gain/Range +-30, Q to 30, Slope 9 entries.

Registration is touch-lazy at BOTH grains (SC-2 + SC-9): NOTHING registers
with the strip anymore - ensureStripEqParams (globals + bands 1-8, both
banks) fires on EQ window open / preset load / load-time tree scan
(restoreEqParamsFromState, the aux-restore sibling, wired into both load
paths), ensureEqBandParams registers bands 9-24 one at a time.  Late
registration adopts saved values via the house replaceState(copyState())
rebind (the applyPendingRackStates recipe) under ScopedProgrammaticParamWrites.
A fresh build now registers ZERO EQ params (was 9,792).

The audio sweep (updateEQFromCache) builds kbs::EqBandParams from the
nullable pointer cache (Freq stays the acquire-published flag; unregistered
bands skip) and drives StripEq::pushBand + pushGlobals; cache reshaped to
strips x banks x 24 with a parallel 5-slot bank-globals cache.  SC-15: mode
+ os are params but never enter the sweep or the automation registry (belt
in registerStaticAutomationHandlers) - an APVTS listener applies them on
the message thread under the nest-aware shield, updating PDC after.
FxRackPresetIO moved to the new spelling + ensures blocks/bands before
writing; formatMixerSuffix labels "{Pre }EQ B{n} {Param}"; the Rusty reset
sweep and the mEQsDirty filter follow the new ids.

Gate failed once - the lazy API landed private while the EQ window and
preset IO call it - fixed by making the three touch points public.  Retry
green (six exit codes 0, four link lines).

FOUND while wiring, routed to Task 8's list: FxRackPresetIO::load applies
the rack BLOB unshielded on the message thread (same class as the
page-preset Path I) - added to the T8 shield set.

## 2026-08-26 - Task 5 - display port I: fontaudio + analyser + graph with views

fontaudio vendored (libs/fontaudio from the KBS tree, MIT module + OFL font +
CC BY 4.0 SVGs - all attribution-required, flagged for the pre-release
/audit-licenses sweep) as a juce_add_module linked into the app target.

New Source/Standalone/EqWindowUI/ (namespace eqview):
- EqAnalyser.h: the KBS analyser near-verbatim over the vendored kbs FFT +
  SpectrumFeed (8192-pt, N/4 normalization, tilt 4.5 default, speeds,
  freeze, peak hold, arm-hold, findPeakNear, match averaging, heat-mapped
  spectrogram; the ring-sized poll buffer lesson kept in the comment).
- EqGraphView.h: the graph adapted to the strip world.  Live StripEq
  resolver (nodes rebuild under windows; null-safe empty plot), params via
  the processor's one id spelling (eqBandParamId statics + the KBS field
  vocabulary mapped in ONE table), ensureBand hook for bands 9-24 (SC-2),
  beginParamUndoGesture on every gesture, the full six-pass interaction
  map.  THE VIEWS (SC-5, 1b): Stereo/Mid/Side; view = domain (band created
  in a view gets that channel; menus offer Left/Right only in the Stereo
  view; Move-to-view per SC-18 keeps settings and re-domains in place;
  reset keeps the band in ITS view - domain is structure, not a value);
  per-view summed curve = product of that view's bands' own engine queries;
  the other views ghost as dimmed live curves + faint dots, never
  interactive.  House dark look (Jeff's addendum): VC::EQGridBg ground, no
  brushed plate, white glow curve, band hues rotated from VC::Blue, post
  analyser in the app's yellow.
- StripEq gained the per-EQ-point viewTree (serialized in the blob) and an
  SC spectrum feed (picked receive slot or first connected + alive flag)
  for the analyser overlay + collision view; processor gained the
  eqBandParamId/eqBankGlobalParamId statics.

Graph compiled via EffectWindows.cpp ahead of the T6 window rebuild.  Gate
green (six exit codes 0, four link lines, no errors).
