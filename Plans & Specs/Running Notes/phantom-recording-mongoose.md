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
- Task 0 commit landed: `606ec15` — "QA-E open: plan file + Main Plan pointer + Sec 0 convention + running notes seed."
- Files in commit: `Plans & Specs/Main Plan.md` (§5 QA-E **Plan file:** pointer + new §0 "Batch Plans + Running Notes layout (locked 2026-05-11)" sub-section), `Plans & Specs/Batch Plans/phantom-recording-mongoose.md` (new — per-batch plan), `Plans & Specs/Running Notes/phantom-recording-mongoose.md` (new — this file, with pre-Task-0 section capturing the corrective + scoping work that preceded this batch open).
- Branch now 12 commits ahead of `origin/main` (was 11 pre-commit).
- Working tree clean post-commit.
- Pre-open commit reference: `54c99dd` (yesterday's QA-E pre-open corrective + scoping work — §9 Forks 11th/12th/13th + §5 routing + QA-D NIT correction).

### Convention lock — implications

- The new Main Plan §0 "Batch Plans + Running Notes layout (locked 2026-05-11)" sub-section is now canonical.  Every batch plan + running-notes file authored from QA-E onward MUST match the documented shape: checkbox `- [ ]` task steps + embedded code blocks for non-trivial fix patterns + explicit "Tell Jeff: ..." verify scripts with numbered test scenarios + per-task Files-to-modify section.  Pre-QA-D plan files do not retroactively match; this is a forward-only convention.
- `phantom-recording-mongoose.md` itself is the first plan file authored under the locked convention.

### Next action

- **Task 1 — Mute verify-and-close (M1).** Source review at QA-E open confirmed mute dispatch gates already present in source: pattern dispatch at [Source/PluginProcessor.cpp:1194-1195](../../Source/PluginProcessor.cpp) (`isRowAudible` + `blk.muted` checks); audio rendering at [Source/PluginProcessor.cpp:493-501](../../Source/PluginProcessor.cpp) (`rowMuted || builderRowMuted` gate); automation block mute at [Source/Standalone/StandaloneEditor.cpp:2406](../../Source/Standalone/StandaloneEditor.cpp); LED click handler at [Source/Standalone/BuilderPage.cpp:4092](../../Source/Standalone/BuilderPage.cpp).  `git log -L` confirms gates date from `cc011e0` (MT engine batches) + initial commit `d595ee3` — both pre-date QA-0's #16a / #16b / #21 finding capture by months.  Either the original findings were captured inaccurately, OR an unrelated commit since incidentally fixed them.
- Jeff to run **Debug + Release** across the 4 verify scenarios in the plan file's Task 1 section:
  - (1) Pattern row mute via LED — pattern silences on click, resumes on second click.
  - (2) Audio row mute via LED — audio silences on click, resumes on second click (must NOT stick).
  - (3) Right-click pattern block → Mute / Unmute — pattern silences then resumes.
  - (4) Right-click audio block → Mute / Unmute — audio silences then resumes.
- All 4 scenarios pass clean → close as no-longer-reproducible via close-time §9 Forks entry (or Implemented Work Log close-routing table if scope fits); no source commit.  Any scenario fails → escalate to M2 dig-deeper; surface root cause to Jeff with options before implementing fix.

---

## 2026-05-11 — Task 1 hold — Clips routing unification finding surfaced

### Found along the way

- **Architectural finding (pre-Task-1, surfaced during FILE-01 / FILE-02 / per-clip discussion):** Clips audio routing diverges from intended design.  Jeff stated intent — "regardless of where you add it is available both in the piano roll and on the builder page all playing through one place" (one clip, one chain regardless of trigger source).  Actual implementation: piano-roll-triggered Clips → Clips engine's InsertNode; Builder-grid-placed Clips audio → row audio insert (different chain).  Surfaced during the per-clip vs summed-input discussion when Jeff asked whether the FilePlay quirk also affects Clips simultaneous-trigger scenarios.  Source-grep confirmed split-routing: Pass 1 FilePlay loop at [Source/PluginProcessor.cpp:2415-2425](../../Source/PluginProcessor.cpp) filters to `isVox || isInst` only; Clips channels fall through to Pass 2 non-FilePlay rendering, which targets row audio insert rather than Clips InsertNode.  Likely a Batch 9b Item 9 oversight when FilePlay was added for live-input engines (Vox/Inst) without considering that Clips clips also benefit from engine-routed playback.
- **Test invalidation:** the prior DSP-12 simultaneous-case verification ("Builder + piano roll both placed, both play summed") tested simultaneous playback but did NOT test "through the same chain."  Jeff was told the system matched the intended design and verified accordingly.  The test passed against a wrong premise.  Owning the misstatement — I should have grounded the design claim in source before telling Jeff the setup matched intent.

### Routing decisions

- **CL-A folded:** §9 thirteenth Forks entry (committed in `54c99dd`) amended same day to expand decision scope from "FilePlay path (Vox/Inst only)" to "FilePlay path (Vox/Inst/Clips) + Clips routing unification."  Pass 1 loop's `isVox || isInst` filter expands to include Clips channels; grid-placed audio clips referencing Clips-page-loaded files default `routeChannel` to that Clips page's channel ID (currently 0 / row audio insert).
- **§5 QA-J entry updated:** folded sub-bullet rewritten to cover both FilePlay multi-clip restructure AND Clips routing unification; effort estimate bumped to ~13-18 hours.
- **§5 QA-B entry updated:** new "Test premise correction (2026-05-11)" line added.  Open sequencing call surfaced (slide QA-B to after QA-J close vs split QA-B into two passes vs other) — Jeff decides at QA-J open or post-QA-E close.

### Spec calls resolved

- **CL-A** (route the Clips routing-unification fix to QA-J): confirmed.
- **Intent** (one clip, one chain regardless of source): confirmed.
- **Test premise correction routing** (fold into the same §9 amendment vs separate): fold into the §9 amendment.
- **Task 1 hold:** mute verify-and-close held until QA-J update is approved + committed.

### Next action

- Surface the diff + dispatch `/draft-commit` for the amendment commit.  After Jeff approves + the commit lands, resume Task 1 (Mute verify-and-close M1).

---

(Subsequent entries appended below at every commit / sub-task verify / finding / spec call / scope pivot.)
