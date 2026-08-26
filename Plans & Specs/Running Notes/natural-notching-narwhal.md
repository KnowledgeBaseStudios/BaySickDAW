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
