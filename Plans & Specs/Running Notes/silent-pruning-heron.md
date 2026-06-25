# Running Notes — QA-Rules (silent-pruning-heron)

> Append-only running log for QA-Rules.  A new entry lands at every checkpoint
> (commit landed / sub-task verified / finding captured / spec call resolved /
> scope pivot), per `feedback_draft_doc_running_notes_every_checkpoint.md` +
> Main Plan §0.  At batch close, `/draft-doc batch-close` consumes this file to
> compile the Implemented Work Log entry.

Pair: `Plans & Specs/Batch Plans/silent-pruning-heron.md`
Conventions: Main Plan §0 "Document Formatting Conventions" + "Batch Plans + Running Notes layout (locked 2026-05-11)".

## 2026-06-24 — Task 0 — open

- Batch opened.  Plan mirrored to `Plans & Specs/Batch Plans/silent-pruning-heron.md`; home-dir plan-mode copy deleted.
- QA-Rules = rules-only batch.  Adds Main Plan §0 Rules 6-9 (Comment Policy / Communication Style / Technical Approach / Commit Brevity), mirrors condensed into CLAUDE.md, and adds/rewrites/deletes/annotates memory entries per the plan's Reconciliation Audit.  NO source, NO build.
- Planning scope pivoted twice: "strip all comments" -> "selective keep-6-categories strip" -> "no retroactive strip; forward Comment Policy rule + clean-as-you-go in edited regions only" (S1/S4).  Batch renamed QA-CommentStrip -> QA-Rules (S2).
- Rule 9 (commit brevity) reconciles four conflicting commit conventions + the comment-rule conflict; full audit in the plan's Reconciliation Audit section.
- §5 entry + §6 arrow slot inserted directly before QA-EffectsReview (Jeff's confirmed slot); QA-EffectsReview resumes under the new rules afterward.
