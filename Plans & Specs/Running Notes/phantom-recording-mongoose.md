# Running Notes — QA-E (phantom-recording-mongoose)

> **Purpose.** Append-only mid-batch log of what was done, what was found, what
> was decided, and what was deferred during QA-E execution.  Compiled from
> `/draft-doc running-notes` dispatches at every significant checkpoint
> (commit landed / sub-task verified / finding captured / decision made /
> scope pivot / spec call resolved).
>
> At batch close, `/draft-doc batch-close` reads this file (plus git log,
> memory entries, the per-batch plan, and conversation context) and produces
> the single Implemented Work Log entry that goes into `Plans & Specs/
> Implemented Work Log.md`.  This file is the source-of-truth intermediate
> artifact during the batch; the close entry is the durable summary.
>
> **Pair file:** `Plans & Specs/Batch Plans/phantom-recording-mongoose.md`
> (the per-batch plan).
>
> **Convention:** see `Plans & Specs/Main Plan.md` §0 (folder-scope rule +
> Agent Orchestration Rules' mid-batch checkpoint trigger).  Running-notes
> subfolder convention established 2026-05-09 mid-QA-A.

---

## 2026-05-11 — Pre-Task-0 (QA-E open setup) — pre-batch corrective + scoping

### Done

- Pre-batch reads complete: `/standup`, Main Plan §5 QA-E entry + §6 sequencing context + §0 Agent Orchestration Rules + recent §9 Forks entries (8th-10th), Carry-Forward Reference index pull (§1-§6 + §8 highlights), recent Implemented Work Log close entries (QA-Md / QA-A / QA-C / QA-D).
- Spec-call resolution conversation with Jeff (extensive — see Spec Calls table in plan file): S1 bundled / S2 ordering (mute-first) / S3 memory rule shape 3c / S4 NIT placement 4a / M1 mute disposition / C-i crash capture pattern / R-1-c BLU-470 / R-2-a Vox+Inst playback subsumed / R-3-b-i preset to QA-Verify / F1+F2 FILE-01 page-bound / F-3 dry-as-first-class / 7a-7d FILE-02 details / Q2-A consolidated Properties dialog.
- QA-D close NIT bulk-defer caught + corrected via Rule 3 (Jeff overruled): NIT-1/2/3 fold into QA-E Sub-Phase Z; NIT-4 → QA-Cleanup-1.
- DrumPage "Drum Kit" sub-tab use-after-free crash confirmed by user repro. Source audit at QA-E open expanded crash family from 2 to 7 page-type branches in `StandaloneEditor::showPageForTab`.
- Per-clip vs summed-input FilePlay quirk surfaced during FILE-01 scoping. Jeff overruled "deliberate choice" framing — never spec'd. Routed to QA-J (OPT-A — fold into existing DSP-06 Multi-Clip Stacking).
- FILE-01 wording correction: original "browser bin / RetirementQueue" §5 text drifted from Master QA Backlog. Real bug: Vox wet+dry + Inst dry recordings never appear in browser. Replaced §5 wording.
- 3 §9 Forks entries drafted via doc-drafter + approved: 11th (QA-D NIT carry-forward), 12th (crash audit expansion), 13th (per-clip OPT-A → QA-J).
- §5 QA-E entry rewritten: FILE-01 new wording + folded findings #13/#14 expansion + Sub-Phase Z + R-1-c / R-2-a / R-3-b-i routings + effort bump to ~12-16h.
- §5 QA-Cleanup-1 entry: NIT-4 folded sub-bullet added.
- §5 QA-J entry: FilePlay restructure folded sub-bullet + effort bump to ~12-16h.
- Implemented Work Log QA-D close: "Deferred NITs" section header de-prefixed; amendment note added pointing at §9 11th Forks entry.
- New memory rule `feedback_closed_batch_carryforward_via_forks.md` saved.
- Existing memory rule `feedback_qa_batches_fix_bugs_dont_defer.md`: cross-ref pointer added.
- MEMORY.md index: new entry added.
- Pre-open commit landed: `54c99dd` — "QA-E pre-open: Sec 9 Forks + Sec 5 scope routing + QA-D NIT correction."

### Spec calls resolved (with reasoning)

See `Plans & Specs/Batch Plans/phantom-recording-mongoose.md` "Spec calls already locked" table. No spec calls remain open at Task 0.

### Memory rules locked

- `feedback_silly_name_is_my_pick.md` — silly-name picks are mine, not Jeff's (locked earlier today, distinct from this batch's corrective work).
- `feedback_closed_batch_carryforward_via_forks.md` — closed-batch findings get fixed in current open batch + §9 Forks entry that back-refs prior batch. Saved + indexed.

### Workflow-shaping clarifications (mid-session)

- **Plan file convention:** plan files use checkbox `- [ ]` steps + embedded code blocks for non-trivial fix patterns + explicit "Tell Jeff: ..." verify scripts with numbered test scenarios. Established by Jeff's pushback during plan-mode entry (initial plan draft was prose-only; rewritten to match convention).
- **ExitPlanMode behavior:** the tool reads from the plan file written during plan-mode; it doesn't take plan content as a parameter. Pattern matches QA-D / earlier batches.

---

## 2026-05-11 — Task 0 (open commit)

### Done

- Plan-mode draft authored at `~/.claude/plans/luminous-kindling-horizon.md`, approved by Jeff at ExitPlanMode.
- Plan mirrored to canonical `Plans & Specs/Batch Plans/phantom-recording-mongoose.md`.
- Home-dir copy at `~/.claude/plans/luminous-kindling-horizon.md` deleted (one-way mirror per `feedback_plan_mirror_one_way.md`).
- Main Plan §5 QA-E entry now points at the canonical plan file.
- This running-notes file seeded.
- **Batch Plans + Running Notes layout convention locked in Main Plan §0** (Document Formatting Conventions, new sub-section).  Jeff caught the gap when initial plan-mode draft came back prose-only instead of matching QA-D's `federated-bouncing-cupcake.md` exemplar shape (checkbox steps + embedded code blocks + numbered "Tell Jeff" verify scripts + per-task Files-to-modify section).  Convention applies to every future batch plan; pre-QA-D plan files don't retroactively match.  Folded into Task 0 commit (CB-A) so the convention lock + first batch using it land together.
- Task 0 commit pending — `/draft-commit` + surface + approve + commit next.

---

(Subsequent entries appended below at every commit / sub-task verify / finding / spec call / scope pivot.)
