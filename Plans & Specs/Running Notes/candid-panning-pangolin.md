# Running Notes - QA-TrueLevel (candid-panning-pangolin)

> Append-only mid-batch log. A new entry goes in at EVERY checkpoint: a commit
> landing, a sub-task verified, a finding captured, a spec call resolved, a
> scope pivot. Per `feedback_draft_doc_running_notes_every_checkpoint.md`,
> capture as it happens rather than reconstructing at the end. At batch close
> `/draft-doc batch-close` reads this file as the primary input for the single
> Implemented Work Log entry.

**Paired plan file:** `Plans & Specs/Batch Plans/candid-panning-pangolin.md`
**Convention:** Main Plan section 0, "Batch Plans + Running Notes layout
(locked 2026-05-11)".

---

## 2026-08-22 - Batch open

The whole investigation that produced this batch (the LUFS comparison, the
hidden Master Gain, the two-system pan law, the dead browser menu, the empty
Reports section, the analyzer spec miss) is logged in the QA-Manuals running
notes under 2026-08-22 and in
`Research Reports/daw-architecture-pan-law-stages-2026-08-22.md`. Not repeated
here. Every ruling is in the plan's spec-call table (SC-1..SC-18).

State at open: the QA-Manuals ruling-B work (offline session on the message
thread, 5 files) is STAGED and uncommitted - verified by Jeff on installer
20260822-1438 (his normalize test ran on it) but never approved as a commit
because the normalize question came in first. Surfaced again at batch open; it
commits as a QA-Manuals commit before Task 1's.

Jeff's FL measurement that pinned the fold coefficient (SC-3): left-only
material panned 100% right reads -3 (so the far side lands in the near channel
at 0.707); both-sides material reads +3 on the near side (0.707 x (L + R) for
matching content); left-only at center reads 0 (center-unity confirmed).

Task 1 started.
