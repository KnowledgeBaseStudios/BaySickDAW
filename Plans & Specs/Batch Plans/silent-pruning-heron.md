# QA-Rules — Four standing rules (comment policy, communication, technical approach, commit brevity) — Plan (silent-pruning-heron)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/silent-pruning-heron.md`
> Paired running notes: `Plans & Specs/Running Notes/silent-pruning-heron.md`
> (Plan-mode working copy is `~/.claude/plans/we-need-to-fix-shiny-wreath.md` — mirrored to the heron path on approval, home copy deleted.)

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkboxes. **No build** — this batch touches no source. Verify = Jeff reviews the rule wording before each commit.

## Context

A rules-only batch — four new standing rules join the existing enumerated list in Main Plan §0 (Rules 1-5). No source changes.

1. **Comment policy (Rule 6).** Claude writes low-value comments that narrate WHAT the code does, lets them go stale, then reads the stale comment and treats it as truth instead of reading the code. Fix is forward-looking, not a retroactive purge: write comments ONLY for six keeper categories (the why / RT-thread danger / DSP-domain refs / framework workarounds / magic-number calibration / thread-ownership), never narrate WHAT, and clean non-conforming comments in the **regions we edit, as we go**. No mass strip — a retro pass over 306 files is high cost + real risk of nuking a keeper for a low payoff, and a stale comment only misleads when read (at which point the cleanup clause fires).
2. **Communication style + technical approach (Rules 7-8).** Direct, no cheerleading; challenge assumptions and surface the hard questions.
3. **Commit brevity (Rule 9).** The full narrative already lives in the in-repo docs (Work Log + running notes); duplicating it in commit bodies doubles the work + tokens.

**The four new rules collide with existing rules/feedback/docs.** Every conflict is enumerated in the Reconciliation Audit below and resolved in Task 1 — nothing is left fighting the new rules.

**Dependencies:** none. Tree is clean. Slots before QA-EffectsReview, which resumes under these rules afterward.

**Risk:** near-zero. Docs + memory only — no source, no build.

**Effort estimate:** ~45-60 min (the reconciliation audit adds a handful of memory/doc edits).

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| S1 | **No retroactive comment strip.** Forward-only: Rule 6 governs new comments + cleanup of edited regions. | Retro strip across 306 files / ~26.7k comments = high cost + real risk of misclassifying a keeper, low payoff. Jeff's call. |
| S2 | Batch = **QA-Rules**. Formal §5 entry + §6 slot directly before QA-EffectsReview. Rules-only — no source, no build. | Jeff. No strip means "CommentStrip" misnames it. |
| S3 | 4 new rules (6-9) → Main Plan §0 authoritative + CLAUDE.md condensed mirror + memory entries. | Jeff: rules in the real §0 rules section. |
| S4 | Rule 6 cleanup scope = **edited regions only** (the function/block being changed), same edit pass — never a whole-file audit. Sanctioned hygiene, not a "don't expand scope" violation. | Jeff: "just the regions we are editing." |
| S5 | Keeper categories = the 6 (why / RT-thread / DSP-refs / framework-workarounds / magic-number / thread-ownership). Edges: keep named-arg hints (`/*keepContent=*/`) + the existing `// HOLD-FOR-<reason>` convention (= category-1 keepers); strip baked-in date/batch tags (`// H-9 (2026-05-02):`); keep real TODO/FIXME/HACK. | Jeff's keeper list + my edge leans (unobjected) + audit finding (HOLD-FOR). |
| S6 | **Full old-rule reconciliation** (not just Rule 9) folded into Task 1 — every conflict in the Reconciliation Audit below gets rewritten / deleted / annotated. Compatible rules + the cross-project `commit-drafter` agent left untouched (BaySickDAW just stops invoking it). | Jeff: the plan must show what old rules get removed/updated, not hand-wave it. |
| S7 | Brief commits SKIP `/draft-commit` — write the one-liner directly, surface message + full git status, wait for approval. | Jeff: "skip it." Surface-and-wait still applies. |
| S8 | Brief format: `<Batch> Task N: <one-line what> (<scope>)` + `Co-Authored-By` trailer; `git commit -m`. | Accepted. |
| S9 | Commits: Task 0 open / Task 1 rules+reconciliation / Task 2 close. One per task. No source commit. | Rules-only batch. |
| S10 | Silly-name = `silent-pruning-heron`. | My pick. |

---

## Reconciliation Audit (every old rule/doc/memory the four new rules touch)

Swept: memory dir, CLAUDE.md, Main Plan §0, boilerplate `batch_session_boilerplate.md`.

| Action | Target | Rule | What |
|--------|--------|------|------|
| REWRITE | memory `feedback_commit_at_checkpoints.md` | R9 | Drop "HEREDOC + multi-paragraph body explaining why" → brief + direct (`-m`). |
| REWRITE | memory `feedback_surface_drafted_commit_message_for_approval.md` | R9 | Reframe from "drafted/drafter" → "the brief message you wrote"; surface + git status + wait still required. |
| REWRITE | [CLAUDE.md](CLAUDE.md) `## Git Commit Mechanics` | R9 | Multi-paragraph-narrative + `-F`-because-long → brief; `-F` demoted to quoting-hazard fallback. |
| REWRITE | [Main Plan.md](Plans & Specs/Main Plan.md) §0 batch-lifecycle commit bullets (~L458-475) | R9 | Same as CLAUDE.md. |
| REWRITE | boilerplate L36-37 (commit) + L69 (comments) | R9, R6 | L36-37 → brief + skip-drafter. L69 "comments only when WHY is non-obvious" → the six keeper categories + clean-edited-regions clause. |
| DELETE | memory `feedback_every_commit_via_draft_commit.md` | R9 | Fully superseded — brief commits skip the drafter. New `feedback_commit_message_brevity.md` carries the lineage. |
| ANNOTATE | memory `feedback_drafter_output_verbatim_no_restyle.md` | R9 | Add: applies only if a drafter is used; BaySickDAW brief commits are written directly. |
| ANNOTATE | memory `feedback_ascii_only_ui_strings.md` | R6 | It blesses `─ ═ │` comment dividers (for unicode); note Rule 6 treats decorative dividers as bloat — Rule 6 wins; the ascii blessing only governs unicode IF a comment legitimately exists. |
| ADD | memory `feedback_comment_policy.md` | R6 | New. Six categories + cleanup-edited-regions + edges. |
| ADD | memory `feedback_communication_style_direct.md` | R7,R8 | New. Direct/no-cheerleading + challenge-assumptions. |
| ADD | memory `feedback_commit_message_brevity.md` | R9 | New. Brief format + skip-drafter + supersession note. |
| KEEP (compatible, cross-link only) | `feedback_surface_full_git_status_before_commit`, `feedback_no_mid_task_commits`, `feedback_no_source_edits_to_shape_commits`, `feedback_us_english_spelling`, `feedback_no_brand_names_in_user_facing_strings`, `feedback_no_corporate_cliches`, `feedback_dont_overcorrect_user_terminology`, `feedback_own_the_codebase_no_git_alibi`, `feedback_dont_make_unilateral_spec_calls`, `feedback_diagnose_before_fixing`, `feedback_read_governing_docs_yourself` | all | No conflict. Two boundaries to cross-link: (R8) "challenge assumptions" must NOT become deciding for Jeff (`dont_make_unilateral_spec_calls`); (R6) `no_brand_names` confirms code comments MAY name modeled gear (supports category-3 keepers) + "don't mass-scrub comments" aligns with no-retro-strip. |

---

## Sub-spec calls surfaced for ExitPlanMode

**No sub-spec calls open.** All resolved in chat: forward-rule-only (S1), rename (S2), cleanup scope (S4), keeper categories + edges (S5), full reconciliation scope (S6), skip-drafter + format (S7-S8). Rule 6-9 draft text below is for Jeff to approve/edit at the gate.

---

## Files to modify

### Task 0 — Open
- `~/.claude/plans/we-need-to-fix-shiny-wreath.md` → mirror to `Plans & Specs/Batch Plans/silent-pruning-heron.md` (Write); delete home copy.
- [Plans & Specs/Main Plan.md](Plans & Specs/Main Plan.md) §5 — insert `### QA-Rules — <title>` immediately before `### QA-EffectsReview` (grep). Include `**Plan file:**` pointer.
- [Plans & Specs/Main Plan.md](Plans & Specs/Main Plan.md) §6 — insert `QA-Rules` into the arrow directly before QA-EffectsReview (grep).
- `Plans & Specs/Running Notes/silent-pruning-heron.md` — seed.

### Task 1 — Rules 6-9 + full reconciliation (per the Audit table)
- [Plans & Specs/Main Plan.md](Plans & Specs/Main Plan.md) §0 — insert Rule 6/7/8/9 after Rule 5 (~L195); rewrite batch-lifecycle commit bullets (~L458-475).
- [CLAUDE.md](CLAUDE.md) — add `## Working Rules (standing)`; rewrite `## Git Commit Mechanics`.
- `Files For Claude/batch_session_boilerplate.md` (gitignored): L36-37 + L69.
- Memory (outside repo): ADD 3, REWRITE 2, DELETE 1, ANNOTATE 2 (per Audit); update `MEMORY.md` index (terse — index is already over budget).

### Task 2 — Close
- [Plans & Specs/Implemented Work Log.md](Plans & Specs/Implemented Work Log.md) — close entry.

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror home plan → `Plans & Specs/Batch Plans/silent-pruning-heron.md`; delete home copy.
- [ ] Insert §5 QA-Rules entry before QA-EffectsReview (+ `**Plan file:**` pointer); insert into §6 arrow before QA-EffectsReview.
- [ ] Seed `Plans & Specs/Running Notes/silent-pruning-heron.md`.
- [ ] Surface full git status. Write brief commit directly (no `/draft-commit`, per S7), surface + commit on approval:
  ```
  QA-Rules Task 0: open batch (plan mirror + §5/§6 + running notes)

  Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
  ```
- [ ] `/draft-doc running-notes` → apply.

### Task 1 — Rules 6-9 + full reconciliation
- [ ] Insert Rule 6/7/8/9 into Main Plan §0 after Rule 5. Draft text (Jeff approves/edits):
  ```markdown
  **Rule 6 — Comment Policy: comments only for the six keeper
  categories; the code is the source of truth.** Write a comment ONLY
  when it falls in one of six keeper categories: (1) architectural intent
  / the "why" a non-obvious approach was chosen (incl. the existing
  `// HOLD-FOR-<reason>` markers); (2) real-time audio-thread danger
  zones (no allocation, no locks, lock-free only); (3) DSP / domain
  references (formulas, papers, hardware/schematic refs — modeled-gear
  names are allowed here per the brand-names rule); (4) framework quirks /
  workarounds (the JUCE/OS idiosyncrasy the "ugly" code exists for);
  (5) magic-number calibrations + how the value was derived; (6) thread-
  safety / ownership (who owns a resource, which thread reads vs writes).
  Never narrate WHAT the code does — restating the code is the bloat that
  goes stale and misleads. The code is the single source of truth; never
  trust a comment over the code. **Cleanup clause:** when editing code,
  strip or fix non-conforming comments in the regions you touch (the
  function/block being changed), same edit pass — scoped to edited regions
  only, never a whole-file audit. Sanctioned hygiene, not a "don't expand
  scope" violation. No retroactive mass strip: existing comments in
  untouched files stay. Edges: keep named-arg hints (`/*keepContent=*/`);
  strip date/batch tags baked into comments (`// H-9 (2026-05-02):`) and
  classify the rest normally; keep real TODO/FIXME/HACK; decorative
  section dividers are bloat (the ascii-only rule governs unicode in a
  legitimate comment, not whether to keep a divider). Supersedes the prior
  "comments only when WHY is non-obvious" guidance. Adopted 2026-06-24.

  **Rule 7 — Communication Style: direct, no cheerleading.** Be direct
  and straightforward. No cheerleading phrases ("that's absolutely
  right," "great question"). Tell Jeff when an idea is flawed, incomplete,
  or poorly thought through. Casual language and occasional profanity when
  it fits. Focus on practical problems and realistic solutions over
  positivity or encouragement. Adopted 2026-06-24.

  **Rule 8 — Technical Approach: challenge assumptions.** Challenge
  assumptions, point out potential issues, and ask the hard questions
  about implementation, scalability, and real-world viability. If
  something won't work, say so directly and explain why — don't just
  dismiss it, and don't rubber-stamp it. (Challenging is not deciding:
  spec calls still go to Jeff per Rule 5.) Adopted 2026-06-24.

  **Rule 9 — Commit messages stay brief.** Commit messages contain only
  the files/areas touched + base-level what-was-done. No multi-paragraph
  narrative. The full narrative lives in the Implemented Work Log +
  running notes (in-repo) — duplicating it in the commit body doubles the
  work and tokens for zero added record. Brief commits skip
  `/draft-commit`: write the one-liner directly, surface message + full
  git status, wait for approval. Format: `<Batch> Task N: <one-line what>
  (<scope>)` + `Co-Authored-By` trailer; `git commit -m` (`-F` only on a
  quoting hazard). Supersedes the prior multi-paragraph-narrative
  convention + the every-commit-via-drafter rule. Adopted 2026-06-24.
  ```
- [ ] Rewrite Main Plan §0 batch-lifecycle commit bullets (~L458-475) → brief + skip-drafter + `-m`. Keep surface-git-status + surface-and-wait.
- [ ] CLAUDE.md: add `## Working Rules (standing)` (Rules 6-9 condensed, point to §0); rewrite `## Git Commit Mechanics` → brief.
- [ ] Boilerplate (gitignored): L36-37 → brief + skip-drafter; L69 → six categories + clean-edited-regions.
- [ ] Memory edits per the Reconciliation Audit: ADD `feedback_comment_policy.md` / `feedback_communication_style_direct.md` / `feedback_commit_message_brevity.md`; REWRITE `feedback_commit_at_checkpoints.md` / `feedback_surface_drafted_commit_message_for_approval.md`; DELETE `feedback_every_commit_via_draft_commit.md`; ANNOTATE `feedback_drafter_output_verbatim_no_restyle.md` / `feedback_ascii_only_ui_strings.md`; update `MEMORY.md` index (drop the deleted line, add the new ones, keep terse).
- [ ] No build. Tell Jeff: "Read the Rule 6-9 text + the rewritten Git Commit Mechanics + the Reconciliation Audit results — confirm before I commit."
- [ ] On approval: surface full git status, write brief commit directly, surface + commit:
  ```
  QA-Rules Task 1: add Rules 6-9 to Main Plan §0 + CLAUDE.md; reconcile conflicting commit/comment rules

  Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
  ```
  (Committed: Main Plan.md + CLAUDE.md. Boilerplate + memory are outside the commit.)
- [ ] `/draft-doc running-notes` → apply.

### Task 2 — Close sequence
- [ ] `/draft-doc batch-close` → compile Work Log entry from running notes.
- [ ] Apply close entry to `Plans & Specs/Implemented Work Log.md` via Edit.
- [ ] `/review-batch QA-Rules`.
- [ ] Address BLOCKERs / NEEDS-FIX; defer NITs into close-entry routing table.
- [ ] Route side findings per Rule 3 (in-batch → close table; outside-batch → §9 Forks + surface placement).
- [ ] Surface full git status. Write brief close commit directly, surface + commit:
  ```
  QA-Rules Task 2: close batch (Work Log entry + review)

  Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
  ```

---

## Verification (rules-only — doc review, no build)

1. **Rules present + correct.** §0 shows Rule 6/7/8/9 with the agreed wording.
2. **Mirror complete.** CLAUDE.md `## Working Rules` + rewritten `## Git Commit Mechanics`; memory entries added/rewritten/deleted/annotated per the Audit; boilerplate updated.
3. **No conflicts left.** Every row in the Reconciliation Audit is resolved — grep memory + CLAUDE.md + §0 for "multi-paragraph" / "HEREDOC" / "via /draft-commit" turns up nothing live.
4. **Eats its own rules.** Every commit in this batch uses the brief format with the drafter skipped.
5. **Cleanup clause live going forward.** First real test = QA-EffectsReview resuming: edited regions get their comments brought to the six-category standard.

---

## Routing notes (Rule 3 application during execution)

- Rule 6's cleanup clause is NOT retroactive — do not open a comment-audit sub-task here.
- If `MEMORY.md` blows further past its size budget after the net +2 entries, note it for a future `consolidate-memory` pass — don't consolidate now.
- Findings unrelated to the four rules → §9 Forks + surface placement; don't fold in.

## Carry-Forward Reference touch points

- None. No source, no audio-thread path, no DSP module touched.
