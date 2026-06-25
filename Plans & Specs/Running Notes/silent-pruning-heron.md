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
- Task 0 commit `9bdaaef` landed: Main Plan §5 docket + §6 arrow/banner (QA-Rules = 35 asterisks) + §9 fifty-first Forks entry + plan mirror + this seed.  `composed-foraging-rose.md` (QA-EffectsReview notes) left untouched.  Jeff confirmed: keep the §9 entry; bucket = Cross-cutting Infrastructure.

## 2026-06-24 — Task 1 — Rules 6-9 + reconciliation

- Main Plan §0: inserted Rule 6 (Comment Policy) / 7 (Communication) / 8 (Technical Approach) / 9 (Commit Brevity) after Rule 5; rewrote the §0 batch-lifecycle commit bullets (Pre-commit + Long-message block) to brief + skip-drafter + `-m`.
- CLAUDE.md: added `## Working Rules (standing)` section (Rules 6-9 condensed, points to §0 as authoritative); rewrote `## Git Commit Mechanics` to the brief convention (`-F` demoted to quoting/encoding-hazard fallback).
- Boilerplate `batch_session_boilerplate.md` (gitignored) — scope EXPANDED per Jeff mid-task: L36-37 (mid-batch every-commit) + L53 (close-commit) -> brief + skip-drafter; L69 (comments) -> six keeper categories + clean-edited-regions; ADDED two standing-rule bullets for Rule 7 + Rule 8 (they were absent entirely).  My plan had under-scoped this to L36-37 + L69 only — Jeff caught it.
- Memory (outside repo): ADDED feedback_comment_policy / feedback_communication_style_direct / feedback_commit_message_brevity; REWROTE feedback_commit_at_checkpoints (heredoc+4.7 -> brief+`-m`+4.8) + feedback_surface_drafted_commit_message_for_approval (de-draftered); DELETED feedback_every_commit_via_draft_commit; ANNOTATED feedback_drafter_output_verbatim_no_restyle (dormant) + feedback_ascii_only_ui_strings (dividers note); updated MEMORY.md index (-1 deleted, +3 new).
- MEMORY.md further over its size budget (net +2 entries) — flagged for a future consolidate-memory pass per the plan's routing note; not consolidating now.
- No source, no build.  Task 1 commit covers Main Plan.md + CLAUDE.md (boilerplate gitignored, memory outside repo).
