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
