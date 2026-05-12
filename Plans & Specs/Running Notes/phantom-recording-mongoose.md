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

## 2026-05-11 — Post-amendment — QA-B sequencing decision resolved

### Spec calls resolved

- **QA-B sequencing (Option A):** Jeff picked Option A — slide QA-B entirely to after QA-J close (vs. splitting into single-flow cells after QA-E + simultaneous case after QA-J).  Avoids two-pass execution; keeps QA-B atomic on unified-routing source.
- **QA-J amendment confirmed:** Jeff confirmed the §9 13th Forks entry amendment + §5 QA-J / QA-B updates from commit `ec9c0b5`.

### Done

- §9 fourteenth Forks entry added: "QA-B second deferral resolved (after-QA-E → after-QA-J, Option A)".  Documents Trigger / Diagnosis / Options / Decision / Time-decay risk / Carry-forward contradictions / Inline back-refs / Plan files affected / Verification.
- §6 sequencing arrow updated: `... QA-E → QA-F → ... → QA-J → QA-B → QA-K → ...` (was `... QA-E → QA-B → QA-F → ...`).
- §6 footnote `*******` rewritten to chronicle both QA-B deferrals (2026-05-10 first deferral after-QA-A → after-QA-E; 2026-05-11 second deferral after-QA-E → after-QA-J).
- §6 parallel-group note updated to mention both deferrals.
- §5 QA-B "Test premise correction" line renamed to "Test premise correction + sequencing decision" and Option A locked.
- §9 13th Forks entry "Test premise correction" section gains back-ref to the 14th Forks entry.

### Next action

- Commit boundary: bundle this sequencing-decision edit into a small standalone commit before resuming Task 1.  Diff is doc-only (Main Plan + this running-notes file).
- After commit: resume Task 1 (Mute verify-and-close M1) per the Debug+Release test plan already surfaced.

---

## 2026-05-11 — Task 1 — Mute Verify-and-Close (M1) — PASS

### Done

- Task 1 verify executed by Jeff in BOTH Debug and Release across all 4 scenarios per the plan file's Task 1 "Tell Jeff:" section.
- **Result: all 4 scenarios PASSED in Debug. All 4 scenarios PASSED in Release. 8/8 PASS.**
  - (1) Pattern row mute via LED → pattern silences on click + resumes on second click. PASS.
  - (2) Audio row mute via LED → audio silences on click + resumes on second click (no stick). PASS.
  - (3) Right-click pattern block → Mute / Unmute → toggles cleanly. PASS.
  - (4) Right-click audio block → Mute / Unmute → toggles cleanly. PASS.
- Source-side context already captured at QA-E open + reconfirmed here: mute dispatch gates are present in current source.
  - Pattern dispatch gate: [Source/PluginProcessor.cpp:1194-1195](../../Source/PluginProcessor.cpp) (`if (blk.muted) continue; if (!isRowAudible(blk.trackRow)) continue;`).
  - Audio rendering gate: [Source/PluginProcessor.cpp:493-501](../../Source/PluginProcessor.cpp) (`if (rowMuted || builderRowMuted) continue;`).
  - Block-level mute gate: [Source/Standalone/StandaloneEditor.cpp:2406](../../Source/Standalone/StandaloneEditor.cpp) (`if (block.muted) continue;`).
  - LED click handler: [Source/Standalone/BuilderPage.cpp:4092](../../Source/Standalone/BuilderPage.cpp) (`mPM.setRowMuted(row, !mPM.isRowMuted(row));`).
- `git log -L` traces confirm these gates date from `cc011e0` (MT engine batches) + initial commit `d595ee3` — both pre-date QA-0's #16a / #16b / #21 finding capture by months.

### Disposition

- **#16a / #16b / #21 no longer reproducible** in current source.  Runtime now verifies the dispatch gates work end-to-end across both build configs.  Either the original QA-0 captures were inaccurate, OR an unrelated commit since incidentally fixed them; root cause of original captures unidentified but moot given the clean 8/8 verify.
- **No source commit.**  Task 1 closes as verify-only.
- Final routing of #16a / #16b / #21 (§9 Forks entry vs Implemented Work Log close-routing table) deferred to the batch-close drafter step per the plan file.  The close drafter will pick the right form based on scope at close-time.

### Next action

- Task 1 close commit is running-notes-only (this entry).  Diff is doc-only — no source files touched.  Surface diff + dispatch `/draft-commit` next.
- After commit: **Task 2 — Crash Family SafePointer Fix (Sub-Phase A).**  All 7 page-type branches in `StandaloneEditor::showPageForTab` (lines 4061-4345) get the C-i SafePointer-at-outer-scope pattern applied.  Mechanical; effort ~1.5-2 hr per plan.  I author the source change; Jeff verifies via `do_build.bat` + the test scenarios in plan Task 2's "Tell Jeff:" section (DrumPage Drum Kit sub-tab repro + spot-check the other 6 page types + normal-operation regression sweep).

---

## 2026-05-11 — Task 2 — Crash Family SafePointer Fix (Sub-Phase A) — PASS

### Done

- Task 2 source change landed in [Source/Standalone/StandaloneEditor.cpp](../../Source/Standalone/StandaloneEditor.cpp) across all 7 page-type branches in `StandaloneEditor::showPageForTab` (LayersPage / BassPage / ClipsPage / VoxPage / InstPage / DrumPage / BaySickRustyDrumsPage).
- **Pass 1 (C-i SafePointer-at-outer-scope — the planned pattern from §9 twelfth Forks entry):**
  - Lifted `juce::Component::SafePointer<XxxPage> safe (xp);` to outer scope right after each branch's `dynamic_cast`.
  - Replaced raw `xp` lambda captures with `safe`.
  - Inside lambdas use `if (auto* p = safe.getComponent()) { ... }` guards.
  - Deleted the inner-SafePointer-too-late lines from the 6 branches that previously had them (old pre-edit line refs: Layers 4070, Bass 4104, Clips 4144, Vox 4185, Inst 4219, Drum 4287).
- **Pass 2 (second bug shape surfaced mid-Task — pre-capture into stack locals before destroying call):**
  - First runtime crash (BaySickBasses Piano Roll click on big project) stack-traced to the InstPage redirect lambda at pre-edit line 4275 — the `if (mPianoRollPage)` access AFTER `onTabSelected(4)`.
  - Diagnosis: `onTabSelected(4)` in the page-switch redirect destroys the source InstPage as a side effect AND triggers `mPageMenuBar->setTabSlots(...)`, which replaces the current lambda's callback slot and frees the lambda's heap-allocated capture struct mid-invocation.  Any access to `this->mPianoRollPage` (or `this->mRibbon`) AFTER `onTabSelected(4)` was reading freed memory.
  - Initial first-pass fix: captured `p->getPageIndex()` (plus `p->getSource()` for InstPage) into local stack variables BEFORE `onTabSelected(4)` in the 5 piano-roll-redirect branches (Layers / Bass / Clips / InstPage / DrumPage i==2).  Build + verify reproduced the same stack trace at pre-edit line 4292 — same family, still freed.
  - Second-pass fix (final): captured BOTH `mPianoRollPage` AND `mRibbon` into local stack variables BEFORE `onTabSelected(4)` in ALL 7 redirect paths (the 5 above PLUS DrumPage i==0 PLUS BaySickRustyDrumsPage i==2).  Pattern: `auto* prp = mPianoRollPage; auto* rbn = mRibbon.get();` BEFORE the state-mutating call; AFTER, only the locals are used: `if (prp != nullptr) prp->selectEngine(...)`.
- **Compile error mid-Pass-2:** initial `auto* rbn = mRibbon;` failed because `mRibbon` is `std::unique_ptr<RibbonTabBar>`, not a raw pointer.  Fixed by extracting via `.get()`.  `mPianoRollPage` is already `class PianoRollPage*` raw, so no `.get()` needed there.  All 7 sites now use `auto* rbn = mRibbon.get();`.  Owning the slip — should have grounded the type in source before using `auto*` (per `feedback_check_code_before_calling_it_expected.md`).
- Jeff verified Debug + Release: all deep-link buttons (Piano Roll / Drum Kit / Player sub-tabs) across all 7 page types work cleanly, no crashes, normal-operation regression sweep clean.

### Architectural narrative — two distinct bug shapes in one function, both required fixing

- **Shape 1 (the §9 twelfth Forks entry pattern, landed in Pass 1):** raw `xp` captured in lambdas survived past the source page's destruction.  Fix is `SafePointer<XxxPage>` at outer scope so `safe.getComponent()` returns null after destruction instead of dereferencing freed memory.  This is the C-i pattern locked in commit `54c99dd`'s §9 twelfth Forks entry.
- **Shape 2 (surfaced during Task 2 verify, fixed in Pass 2):** even with SafePointer in place, `onTabSelected(4)` cascades through `mPageMenuBar->setTabSlots(...)` which replaces the active lambda's callback slot and frees its captures mid-invocation.  Reading `p->...` OR `this->...` AFTER that call is unsafe regardless of `p` validity — `this` itself isn't freed but the lambda's heap-allocated capture struct is.  SafePointer does not help here.  Fix is pre-capture into stack locals BEFORE the state-mutating call; the lambda body after the call uses only stack values that survive capture-struct destruction.
- Both fixes had to land for the function to be crash-safe.  The §9 twelfth Forks entry should be amended at batch close to record that Sub-Phase A surfaced + closed this second shape — initial scope locked SafePointer-at-outer-scope only; the pre-capture-before-destroying-call discipline is now a second documented invariant for `showPageForTab` and any future function with the same lambda + setTabSlots structure.

### Pre-existing finding spotted (NOT Task 2 regression, NOT QA-E scope)

- Jeff reported a separate crash at **app close**: `BuilderPage::~BuilderPage` -> `TreeView::~TreeView` -> `TreeViewItem::setOwnerView` walks a dangling subItem pointer.  Source family identical in shape to the QA-D MenuBarModel listener-dangle fix but for `TreeView` + `TreeViewItem` destructor ordering.
- Confirmed via grep this is **QA-0 finding #17, already folded into QA-H scope** per Main Plan §5 (QA-H entry, lines 945-950).  Pre-existing destructor-ordering bug.  Save-before-quit and the close-crash is cosmetic at shutdown; no data loss, no in-session impact.
- **No new routing action.**  Mentioned here only so the trail is captured; QA-H already owns it.

### Disposition

- Source change for Sub-Phase A complete in working tree (uncommitted alongside this running-notes append).  Both files get committed in the Task 2 close commit.
- **#13 / #14 / #40 / #55 (the original showPageForTab use-after-free family) are now fixed at source** across all 7 branches via the combined Pass 1 + Pass 2 patterns.

### Next action

- Surface diff (Source/Standalone/StandaloneEditor.cpp + this running-notes file) + full pre-commit git status per `feedback_surface_full_git_status_before_commit.md`.
- Dispatch `/draft-commit` for the Task 2 close commit message.  Surface drafted message + git status to Jeff for explicit approval before any `git commit` runs.
- After commit lands: **Task 3 — Vox / Inst Lifecycle (MIX-02 / MIX-04 / MIX-06).**  Per plan file Task 3 section.
- Amendment to §9 twelfth Forks entry recording the second bug shape (pre-capture-before-destroying-call) deferred to batch close — the close drafter folds it into either an amendment line on the existing entry OR a new Forks entry depending on shape at close-time.

---

(Subsequent entries appended below at every commit / sub-task verify / finding / spec call / scope pivot.)
