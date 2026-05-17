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

## 2026-05-11 — Task 3 — Vox/Inst Lifecycle (MIX-02 / MIX-04 / MIX-06) — Vox K-6 mirror fix applied + Clips parallel investigated

### Done

- Task 3 investigation phase complete.  Diagnosis: MIX-02 root cause is an **asymmetric K-6 fix history** — Vox restore path is missing the parallel safety-net `addVoxChannelAtIndex(pageIndex)` call that the Inst restore path got on 2026-05-05.  Details:
  - Two restore mechanisms feed mixer strips: `restoreStripNames("VoxNames"/"InstNames"/"AuxNames")` ([Source/Standalone/StandaloneEditor.cpp:9695-9703](../../Source/Standalone/StandaloneEditor.cpp)) only creates strips with non-default names (legacy "persist renames only" semantics, line 9690); and `spawnXxxTabIfMissing(pageIndex)` in the tab-restore loop creates page + ribbon tab but does NOT add the mixer strip.
  - The K-6 fix at line 9323 ([2026-05-05 commit]) explicitly bridged the gap for Inst by adding `mMixerPage->addInstChannelAtIndex(pageIndex)` after `spawnInstTabIfMissing`.  Comment cites the legacy persist-renames-only semantics + the idempotency of the spawn helper.
  - Vox restore at line 9285 was never updated with the parallel fix.  Result: unrenamed Vox tabs (which save no entry in `<VoxNames>`) get their strip dropped on every save/load cycle, leaving the VoxPage with no audio path to the bus.

- **F-A applied:** added the parallel safety-net call after `spawnVoxTabIfMissing` in the Vox branch of `deserializeUIState`.  Single-line code change plus a comment block explaining the K-6 parallel.  Mechanical mirror of the established Inst pattern; risk very low (`addVoxChannelAtIndex` is idempotent, early-returns when the strip already exists).

- **F-B audit completed (Clips parallel question):**
  - **Architectural finding:** Clips mixer strips are arrangement-block-keyed (via `restoreAudioStripsFromArrangement` walking `ClipType::Audio` blocks), NOT page-keyed like Vox/Inst.  The K-6 parallel does not cleanly transfer.
  - **Options surfaced:** F-B-a (mechanical mirror, risk: naming-source-of-truth conflict between tab name and block displayAlias), F-B-b (skip source fix + document via §9 Forks), F-B-c (defer F-B decision entirely).
  - **Jeff's pick: F-B-b.**  Skip the source fix at QA-E Task 3; document the investigation result + routing decision in §9 Forks.  If a real Clips-strip-lifecycle bug surfaces in user testing, route to QA-J's Clips routing unification scope.

- **§9 fifteenth Forks entry applied to Main Plan:** "Clips strip restore audit: no K-6 parallel fix needed (QA-E Task 3 spec call)" — full canonical shape (Trigger / Diagnosis / Options / Decision / Carry-forward contradictions / Inline back-refs / Plan files affected / Verification).  Documents the architectural distinction between Vox/Inst (page-keyed) and Clips (arrangement-block-keyed) strip restore paths so future readers find the routing decision in the canonical chronological log.

### Spec calls resolved

- **F-A** (apply K-6 mirror for Vox in `deserializeUIState`): applied as proposed.
- **F-B** (Clips parallel investigation + Forks entry): F-B-b locked.  Source skipped; §9 fifteenth Forks entry documents the routing decision.

### Next action

- Surface diff + ask Jeff to `do_build.bat` and verify in **Debug + Release** per Task 3 verify script in the plan file (full Vox lifecycle walk: create → record → save → close → reopen → record → reload → delete; spot-check Inst parity; verify no phantom strips after delete).
- After Jeff confirms verify clean: dispatch `/draft-commit`, surface drafted message + git status, commit on approval.
- Task 3 close commit will include: `Source/Standalone/StandaloneEditor.cpp` (the F-A source change), `Plans & Specs/Main Plan.md` (the §9 fifteenth Forks entry), and this running notes file (Task 3 section).
- After commit lands: dispatch `/draft-doc running-notes` post-verify per the locked memory rule.

---

## 2026-05-12 — Task 3 (cont.) — Vox K-6 verify surfaced MIX-03 (routeChannel persistence bug); W-2 audit + R-1 fix

### Found along the way

- **First verify attempt (Vox K-6 mirror fix alone) surfaced a deeper bug.** Jeff confirmed the Vox strip is now restored on project reload (F-A working), BUT clicking Play on the restored project routes Vox-recorded audio through an auto-spawned Clips strip rather than the Vox strip.  Both strips exist after reload but audio doesn't flow through the Vox engine's chain.
- **Diagnosis:** `ArrangementBlock::routeChannel` (added 2026-05-03 for FilePlay routing per the "I-16 G-9" struct comment) is **NOT persisted** in `PatternManager::toValueTree` / `fromValueTree` ([Source/PatternManager.cpp:975-993](../../Source/PatternManager.cpp) + [:1330-1352](../../Source/PatternManager.cpp)).  On save the block writes 13 named fields + automationLane child node but skips routeChannel; on load the block deserializes with `routeChannel=0` (C++ default).  routeChannel=0 makes the FilePlay Pass 1 loop at [Source/PluginProcessor.cpp:2415-2425](../../Source/PluginProcessor.cpp) skip the block (`if (! isVox && ! isInst) continue;`), so the block falls through to Pass 2 (non-FilePlay) → row audio insert → `spawnClipsTabIfMissing` auto-spawns a Clips strip → audio plays through the Clips path.
- **This is the actual MIX-03 root cause.**  Carry-Forward §5 predicted "MIX-03 fixes when MIX-02 fixes" — that prediction was wrong.  MIX-02 (Vox strip missing) and MIX-03 (audio routes through Clips strip) are two independent bugs: F-A fixes MIX-02 (Vox strip restored), routeChannel persistence fixes MIX-03 (audio routes through the right chain).  Both needed.

### Spec calls resolved

- **R-1 (routing for the routeChannel persistence fix):** Jeff picked R-1 — fold into Task 3's source commit alongside F-A.  Same coherent Task 3 unit: Vox/Inst lifecycle fixes.
- **W-2 (full PatternManager save/load audit):** Jeff picked W-2 — audit every struct's fields against its serialize/deserialize before applying the routeChannel fix.

### W-2 audit result

Walked all 15 structs in [Source/PatternManager.h](../../Source/PatternManager.h) against `toValueTree` / `fromValueTree` in [Source/PatternManager.cpp](../../Source/PatternManager.cpp).  Total ~110 fields covered.

**Single gap: `ArrangementBlock::routeChannel`.**  Every other field has matching save/load (or uses the intentional implicit-presence pattern, e.g. `BasicStep::active` / `ComplexStep::active` where only-active-steps-are-written; `PianoNote` optional fields only-written-if-non-default).

Structs audited (all clean except ArrangementBlock):
- `ArrangementBlock` (15 fields, 14/15 covered — **routeChannel missing**)
- `PianoNote` (11/11)
- `PianoRollData` (3/3)
- `AutomationLane` (8/8)
- `ControlPoint` (4/4)
- `Pattern` (20/20 incl. all sub-rolls: layerRoll / bassRoll / drumRoll / drumRolls / clipRoll / voxRoll / instRoll / baySickRustyDrumsRoll)
- `PageSequenceData` (7/7 incl. nested ComplexStep + ComplexEnvelope)
- `BasicStep` (4 — implicit-presence pattern)
- `ComplexStep` (5 — implicit-presence pattern)
- `BasicEnvelope` (5/5)
- `ComplexEnvelope` (7/7)
- `TimeMarker` (2/2)
- `TimeSigChange` (3/3)
- `MixerState` (22/22 incl. all drumSlot / audioRow arrays as CSV)
- `AudioLibraryEntry` (3/3)

Codebase has good save/load discipline overall — `routeChannel` is the sole oversight, likely from when it was added on 2026-05-03 (FilePlay routing work) but the developer forgot to mirror the addition into the save/load pair.

### Done

- **routeChannel persistence fix applied** to [Source/PatternManager.cpp](../../Source/PatternManager.cpp).  Two-line change with explanatory comment blocks on both sides:
  - Serialize: `bNode.setProperty("routeChannel", b.routeChannel, nullptr);` after the muted line at line ~989.
  - Deserialize: `b.routeChannel = (int) bNode.getProperty("routeChannel", 0);` after the muted line at line ~1345.
  - Default 0 on deserialize preserves backward compatibility — pre-fix saves load with `routeChannel=0` which matches the C++ struct default + the legacy "no Vox/Inst routing" behavior.

### Disposition

- Task 3 source change now covers TWO fixes:
  - F-A: Vox K-6 mirror (`StandaloneEditor.cpp` `deserializeUIState` Vox branch — closes MIX-02)
  - R-1: routeChannel persistence (`PatternManager.cpp` toValueTree + fromValueTree — closes MIX-03)
- MIX-02 + MIX-03 + MIX-04 + MIX-06 all share the family.  Both fixes are required to restore the full Vox lifecycle end-to-end.

### Next action

- Surface diff + ask Jeff to `do_build.bat` and verify Debug + Release.  The verify sequence is now richer: F-A test (Vox strip exists post-reload) + R-1 test (audio routes through Vox chain post-reload, no auto-spawned Clips strip).
- After verify clean: dispatch `/draft-commit`, surface drafted message + git status, commit on approval.

---

## 2026-05-12 — Task 3 verify failure → MT FilePlay root-cause diagnosis

Verify cycle of F-A + R-1 failed: with both fixes applied, pre-save fresh-record was silent through Vox/Inst on playback, AND post-save + reload was silent (regression vs. pre-R-1 which played through phantom Clips strips).

Multi-stage diagnosis with several wrong theories along the way (Pattern-mode gate, JUCE Optional<bool> semantics, project-load barrier stuck true, audio-thread dead).  Each theory ruled out by adding instrumentation that contradicted the hypothesis.  Final root cause:

**The MT engine path (when `gMultiThreadedEngineEnabled = true`, the default) early-returns at [Source/PluginProcessor.cpp:1934](../../Source/PluginProcessor.cpp:1934) BEFORE reaching the FilePlay pre-scan at line 2143.  The pre-scan sets `mVoxFilePlayActive` / `mInstFilePlayActive`, which VoxStripTask + InstStripTask gate their FilePlay branch on.  Without the pre-scan running, those flags stay false → MT tasks take the live-input branch → recorded clip audio never decodes.**

User A/B confirmed: toggling MT off via the Mixer hamburger menu makes audio play correctly through Vox + Inst strips on the same reloaded file.  Toggling MT back on reproduces the silence.

CLAUDE.md's stale "MT no-op under Debug" note is misleading — MT engine works in both Debug and Release; the bug is the orphaned pre-scan, NOT MT itself being broken.  See §9 sixteenth Forks entry for full diagnosis chronology.

### Done

- **Fix 1 — Pre-scan move.**  Lifted the FilePlay pre-scan block from line 2143 (after MT early-return) to before the MT branch at line 1860 in [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp).  Mechanical move, no logic change.  Both serial AND MT paths now see correctly-populated `mVoxFilePlayActive` / `mInstFilePlayActive`.
- **Fix 2 — Clips-strip route-guard.**  Added `if (b.routeChannel != 0) continue;` at line 9811 in [Source/Standalone/StandaloneEditor.cpp](../../Source/Standalone/StandaloneEditor.cpp) `deserializeUIState` block-loop.  Vox/Inst-routed blocks no longer spawn phantom Audio strips on reload.  Mirrors existing `if (routeChannel == 0)` guard inside `dropWavAsClip` at line 9912.
- **Verify pass 2026-05-12:** with all four fixes applied (F-A + R-1 + Fix 1 + Fix 2) + MT-on, reload + Play routes audio through Vox + 2 Inst strips correctly.  No phantom Clips strips appear on the mixer.

### Scope adds folded mid-Task-3

- **Task 9 — Dirty-flag investigation** (new).  User observation during verify: post-record + save still shows dirty `*` on reopen.  Does NOT happen until WAV files land on the Builder grid (i.e., post-`commitRecordingResult`'s `markDirty()` call).  Inserted between Task 8 Sub-Phase Z and the close sequence (which renumbers to Task 10).  Per `feedback_qa_batches_fix_bugs_dont_defer.md`, real bugs surfaced mid-QA-batch get fixed in-batch.
- **Task 7 fold-in — "Add a new Page" entries in Routing dropdown** (new sub-bullet).  User feature request during diagnostic: the FILE-02 Routing dropdown should include "Add a new Clip Page", "Add a new Vox Page", "Add a new Inst Page" entries so the user can route a clip to a NEWLY-created page without first navigating to the ribbon.

### Main Plan §0 Rule 4 locked

Main Plan §0 Rule 4 (2026-05-12) — every diagnostic addition (DBG, Logger, temp jassert, debug AlertWindow) gets logged in the per-batch Diagnostic Instrumentation Catalog below with disposition (`Remove at task/batch close` / `Keep`).  Established after I'd shipped ~15 `[QA-E DIAG]` sites with no running record.

### Disposition

- Task 3 source change now covers FOUR fixes:
  - F-A: Vox K-6 mirror (`StandaloneEditor.cpp` `deserializeUIState` Vox branch — closes MIX-02)
  - R-1: routeChannel persistence (`PatternManager.cpp` toValueTree + fromValueTree — closes MIX-03)
  - Fix 1: pre-scan move (`PluginProcessor.cpp` lift to before MT branch — closes MT-side FilePlay gap, root cause of MIX-04/06)
  - Fix 2: Clips-strip route-guard (`StandaloneEditor.cpp` deserialize block-loop — closes phantom Clips strip on reload)
- All four fixes interdependent: R-1 produces correct routeChannel on reload, which Fix 1's pre-scan reads to set the FilePlay flags, which the MT tasks gate their FilePlay branch on.  F-A ensures the Vox mixer strip exists for routing graph wiring.  Fix 2 prevents the deserialize-side phantom strip.
- §9 sixteenth Forks entry locked.  §5 QA-E entry updated with three fold-in notes (audio-routing fix family, Task 9 dirty-flag, Task 7 routing-dropdown sub-bullet).

---

## Diagnostic Instrumentation Catalog

Per Main Plan §0 Rule 4.  Every diagnostic addition tracked here with disposition.  At task/batch close, walk the catalog and strip every `Remove` entry.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| [Source/Standalone/StandaloneEditor.cpp](../../Source/Standalone/StandaloneEditor.cpp) `~9908` in `commitRecordingResult` `dropWavAsClip` lambda | `[QA-E DIAG] commitRecord block added` | Verify recorded blocks land in PatternManager with correct routeChannel + path + trackRow at record-finalize time | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~3274` in `rebuildAudioClipPlayers` | `[QA-E DIAG] RACP skipped block` | Detect when a block's audio file fails `createReaderFor` on snapshot build | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~3322` in `rebuildAudioClipPlayers` | `[QA-E DIAG] RACP added player` | Verify each AudioClipPlayer's routeChannel + clip range + streamer on snapshot build | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~3333` in `rebuildAudioClipPlayers` | `[QA-E DIAG] RACP publishing snapshot` | Confirm snapshot size at publication | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~963` top of `processBlock` | `[QA-E DIAG] BARRIER transition` | Detect every `mProjectLoadInProgress` flag transition | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~972` in `processBlock` | `[QA-E DIAG] processBlock ENTRY heartbeat` | Confirm `processBlock` is alive (every 1024 blocks ~ 3s at 128/44.1kHz) | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~988` after barrier check | `[QA-E DIAG] processBlock PAST-BARRIER heartbeat` | Confirm `processBlock` runs past the barrier (every 1024 blocks) | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~1872` in pre-scan (post-fix location) | `[QA-E DIAG] PreScan enter` | Verify pre-scan reaches iteration phase + frame timing | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~1882` in pre-scan loop | `[QA-E DIAG] PreScan player` | Per-player route + overlap state during pre-scan | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~2412` before Pass 1/Pass 2 gate | `[QA-E DIAG] PreGate2396 heartbeat` | Live values of the three gate components (songMode, isPlaying, patternMgr) | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~2450` Pass 1 entry | `[QA-E DIAG] Pass1 enter` | Confirm Pass 1 entered (when serial path active) | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~2462` Pass 1 player iteration | `[QA-E DIAG] Pass1 iter` | Per-player Pass 1 iteration state | Remove at Task 3 close |
| [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) `~685-960` in `renderFilePlayPlayer` (qaDiag lambda + tagged early-returns) | `[QA-E DIAG] renderFilePlay route=X <tag>` | Trace entry + every early-return path inside renderFilePlayPlayer with unique tag per return point | Remove at Task 3 close |
| TBD (pre-existing — exact lines to be resolved during strip pass) | `[NAMIR]` state log | User-noted pre-existing diagnostic; long-term value confirmed by user 2026-05-12 | Keep |
| TBD (pre-existing — exact lines to be resolved during strip pass) | `[Pedals]` state log | User-noted pre-existing diagnostic; long-term value confirmed by user 2026-05-12 | Keep |
| TBD (pre-existing — exact lines to be resolved during strip pass) | Audio Setup log | User-noted pre-existing diagnostic; long-term value confirmed by user 2026-05-12 | Keep |
| [Source/Standalone/StandaloneEditor.cpp:4660+](../../Source/Standalone/StandaloneEditor.cpp:4660) — Mixer hamburger menu item 203 | "Multi-core diagnostic capture" QA-Md MT thread distribution AlertWindow | QA-Md MT investigation diagnostic; long-term value | Keep |
| ~~[Source/Standalone/BuilderPage.cpp](../../Source/Standalone/BuilderPage.cpp) `parseBrowserDragDescription`~~ | ~~`[QA-E DIAG T4] parseBrowserDragDescription` (input + parsed)~~ | Diagnose why Vox-category browser drags reject on the Builder grid post-Task-4 library-driven model | **Stripped at Task 4 close 2026-05-14** (root cause found: `File(relative).existsAsFile()` resolves against CWD = Debug exe folder, not project folder; fixed via `onResolveStoredPath` fallback) |
| ~~[Source/Standalone/BuilderPage.cpp](../../Source/Standalone/BuilderPage.cpp) `importAudioFile`~~ | ~~`[QA-E DIAG T4] importAudioFile` (path + existsAsFile)~~ | Confirm whether the incoming relative path resolves to an existing file | **Stripped at Task 4 close 2026-05-14** |
| ~~[Source/Standalone/BuilderPage.cpp](../../Source/Standalone/BuilderPage.cpp) `itemDropped`~~ | ~~`[QA-E DIAG T4] itemDropped` (desc + localPos)~~ | Confirm drop event reaches the grid handler at all | **Stripped at Task 4 close 2026-05-14** |

### Pre-existing diagnostic resolution at Task 3 close

When stripping QA-E DIAG entries, also verify exact line numbers of the `Keep` entries (TBD locations) and add to catalog for completeness.  Don't auto-strip anything not explicitly tagged `Remove`.

---

## 2026-05-12 — Task 3 closed (commit `c0f57c9`)

Audio-routing fix family (F-A + R-1 + Fix 1 + Fix 2) landed in `c0f57c9` covering MIX-02 / MIX-03 / MIX-04 / MIX-06.  Diagnostic instrumentation stripped per Rule 4 catalog convention before commit.  Verified working 2026-05-12 by user (reload + Play with MT-on routes audio through Vox + 2 Inst strips correctly; no phantom Clips strips).

Doc work bundled into the same commit: §9 sixteenth Forks entry, §5 QA-E fold-in notes, §0 Rule 4, plan-file Task 7 routing-dropdown sub-bullet, Task 9 (Dirty-flag investigation) inserted, Close renumbered to Task 10, Files-to-modify summary refreshed, running-notes Task 3 verify-pass entry + Diagnostic Instrumentation Catalog section.

**Next: Task 4 (FILE-01 — Vox wet+dry + Inst dry browser visibility).**

---

## 2026-05-12 — Task 4 plan-review surfaced architectural gap → scope expansion (library-driven model)

Started Task 4 plan walk-through with the user.  In confirming "what will the behavior be for multiple recordings on one page?", the original plan's single-`mClipPath`/`mDryClipPath` per page shape was identified as fundamentally incompatible with multi-take-per-page (each new take overwrites the bound paths; browser shows latest only).  Same gap blocks Task 7's "route multiple files to one source" intent — the planned `mClipPath` shape destroys any prior assignment on every re-route.

User stated intent: "at every point I asked you to set it up so multiple files could be recorded to one player page and all played through the same page" + "Task 7's whole point is to route multiple files to one source so what was your plan there to let the new source just destroy the old one?"

### Done

Documentation-only commit lands the scope expansion:

- **§9 seventeenth Forks entry** locked in [Plans & Specs/Main Plan.md](../../Plans%20%26%20Specs/Main%20Plan.md) — full diagnosis chronology + Option A (per-page list) vs Option B (library-driven) trade-off analysis + user spec call locking Option B + the three sub-spec-calls (delete mClipPath on Vox/Inst, retain on Clips for engine preload, no migration heuristic).
- **Main Plan §5 QA-E entry** updated with fold-in note for the Task 4 scope expansion.
- **Batch plan Task 4 detail rewritten** to library-driven model: `AudioLibraryEntry.pageOwnerChannelId` field, addAudioToLibrary overload, browser walk rewrite, Vox/Inst deletion, Clips transitional retention.
- **Files-to-modify summary** at top of plan file updated to reflect new scope.

### Disposition

- Task 4 source implementation lands in the NEXT commit (this commit is doc-only to lock the spec).
- Verify scenarios in the rewritten plan cover both single-take AND multi-take cases — multi-take is the regression-prevention check for this scope expansion.
- Per spec call: VoxPage/InstPage mClipPath fully deleted; ClipsPage mClipPath retained transitionally (engine preloads from it); deletable post-QA-J's Clips routing unification.
- No migration heuristic for legacy projects — user will make new test projects.

### Memory rule reminder

Per Main Plan §0 Rule 4: any diagnostic instrumentation added during Task 4 source implementation gets logged in the Diagnostic Instrumentation Catalog below at the moment of the code change, not after.

### Task 3 follow-up landed (commit `54f41c8`)

Mid-Task-4-plan-review verify of the existing Task 3 commit (`c0f57c9`) with MT-on + 1 Vox + 2 Inst takes surfaced a cross-strip pollution bug: all 3 clips' audio mixed into every strip's output.  Diagnosis: Task 3's pre-scan move activated MT FilePlay (previously a no-op behind a `constexpr false` flag), which turned a long-acknowledged race in `renderFilePlayPlayer` + `VoxStripTask` + `InstStripTask` on three shared processor-level scratches (`mAudioClipScratch`, `mVoxEngineScratch`, `mInstEngineScratch`) from latent to live.  `VoxStripTask.cpp:83-85` had a stale comment explicitly tracking the future fix.

Fix (Option B per user spec call): each task owns `mClipScratch` + `mEngineScratch` members sized per-block; `renderFilePlayPlayer` signature gains a required `engineScratch` reference parameter; serial Pass 1 picks the matching processor member based on the per-player route (single-threaded, member-sharing safe).  Three options considered (A: SpinLock band-aid / B: per-task scratches via signature refactor / C: inline FilePlay code into each task) — Option B picked as architecturally cleanest (preserves MT parallelism, single source of truth for FilePlay decode + engine drive, no code duplication).

User-verified 2026-05-12: MT-on + 1 Vox + 2 Inst now plays each track through its own strip cleanly; no cross-pollution; mute test isolates correctly.  Landed as Commit A before this Task 4 doc-scope commit (seq-1 per user call).

Task 3 conceptually now covers: F-A + R-1 + Fix 1 + Fix 2 (in `c0f57c9`) PLUS the MT race fix (in `54f41c8`).  No reopen of `c0f57c9`; the follow-up commit is the canonical Task 3 close.

---

## 2026-05-12 — Task 4 source implementation applied (awaiting verify)

Library-driven page-owner model implemented per §9 17th Forks entry.  Source diff covers 7 files:

### Done

- **PatternManager.h** — `AudioLibraryEntry` struct gains `int pageOwnerChannelId { 0 };` 4th field.  `addAudioToLibrary` signature extends with `int pageOwnerChannelId = 0` optional param.  New `getAudioLibraryPageOwner(idx)` + `setAudioLibraryPageOwner(idx, channelId)` accessors.
- **PatternManager.cpp** — `addAudioToLibrary` impl: when path already present, updates ownerChannelId if caller passes non-zero (this enables Properties-dropdown re-routing to retag entries without removing them).  New setter impl.  Serialize: writes `pageOwnerChannelId` attribute on `<Entry>`.  Deserialize: reads with default 0 (back-compat for pre-fix saves).
- **VoxPage.h + VoxPage.cpp** — DELETED: `mClipPath`, `getClipFilePath`, `setClipFilePath`, `mClipFileLabel`, `getClipFileLabel`, `mLinkedClipPath` + accessors.  Constructor's mClipFileLabel setup deleted.  Also DELETED: the 2026-04-29 debug `isInterestedInFileDrag` + `filesDropped` drop-onto-Vox-tab handlers (debug scaffolding never intended as user surface; user caught the mistake when I'd initially rerouted them to library tagging).  Vox tabs no longer inherit from `FileDragAndDropTarget`.  Dead `if (mClipPath.isNotEmpty()) setClipFilePath(mClipPath)` re-bind at end of importVoxState deleted (mClipPath was never serialized so always empty here).
- **InstPage.h + InstPage.cpp** — DELETED: `mClipPath`, `getClipFilePath`, `setClipFilePath`, `mLinkedClipPath` + accessors.  `mClipFileLabel` + `getClipFileLabel` RETAINED -- the label is dual-purpose, also driven by sfizz kit name display via setSource() (lines 915 + 937 in InstPage.cpp).  Inst has no file-drag handler (only Vox does), so no reroute needed.  Dead re-bind at end of importInstState deleted.
- **ClipsPage.cpp** — `setClipFilePath` extended to ALSO tag the library entry with `audioInsert(mPageIndex)` after engine preload.  `mClipPath` RETAINED for engine preload (deletable post-QA-J Clips routing unification per §9 17th Forks entry).  Library tracks all N files routed to a Clips page; mClipPath is the "currently preloaded sample" for piano-roll trigger.
- **StandaloneEditor.cpp** —
  - `commitRecordingResult`: `dropWavAsClip` lambda's `mPM->addAudioToLibrary(...)` call extended with routeChannel as ownerChannelId (Vox/Inst routeChannels naturally match their voxInsert/instInsert channel ids; master/generic gets 0).  Explicit Vox DRY library add at line 9913 also passes chId as ownerChannelId so DRY appears under the Vox category alongside WET.
  - Browser walk rewrite (~line 2228-2270): replaces per-page mClipPath round-trip with single library walk pass.  Iterates `mPM->getNumAudioLibrary()`, reads each entry's `pageOwnerChannelId`, groups by range (kVoxBase / kInstBase / kAudioBase / generic).  `findLibIdx` helper deleted (was only used by the old branches).  onDuplicateClipSpawn's Vox/Inst find branches deleted (they were no-ops -- savedVoxState/savedInstState captured but never applied downstream).
  - Vox `getClipFileLabel` call site at line 4205 deleted (label is gone).  Inst `getClipFileLabel` call site at line 4279 RETAINED (label stays for kit display).
- **Running notes** — this entry.

### Disposition

- Build pending Jeff verify.
- All grep checks pass (no remaining `vp->getClipFilePath`, `vp->setClipFilePath`, `vp->getClipFileLabel`, `vp->getLinkedClipPath`, `vp->setLinkedClipPath`, `ip->getClipFilePath`, `ip->setClipFilePath`, `ip->getLinkedClipPath`, `ip->setLinkedClipPath` references anywhere in Source/).  `cp->getClipFilePath` references intentionally preserved -- Clips keeps its accessors.
- Verify scenarios from the rewritten plan file Task 4 (7 tests covering single-take Vox + multi-take Vox + single-take Inst + multi-take Inst + drop onto Clips + save/reload + rename in browser) drive the verification cycle.  The 8th "drag-onto-Vox-tab" test in the earlier plan draft is removed -- that handler was 2026-04-29 debug scaffolding, never an intended user surface.

### Next action

- Surface diff + commit message draft.  Awaiting Jeff approval to commit.

---

## 2026-05-14 — Post-Task-4-verify design decisions (recording-lifecycle + offline editors)

Task 4 source verify cycle landed `1d928fc` (library-driven page-owner model: AudioLibraryEntry + browser walk rewrite + Vox/Inst mClipPath removal + Clips transitional retention).  Verify pass surfaced a recording-side architectural bug + opened a longer design discussion about how the recording lifecycle, BaySickPitch, and BaySickAlign should actually work end-to-end.  This entry captures the WHY behind each decision so the dedicated batches that pick this work up later have the full design context, not just bullet points.

Two architectural bugs surfaced + got routed:

1. Multi-take WET recording captures playback bleed (recording-lifecycle architecture batch).
2. Audio-clip block resize is visual-only — backend doesn't time-stretch (routing TBD).

Three offline-editor-shaped features got their design locked:

3. BaySickPitch — Mode C (realtime applicator + Render/bake).
4. BaySickAlign — channel-composite-driven, single warp map per channel pair.
5. BaySickAlign existing editor needs full redesign with distinct visual identity + functional DSP wiring.

Plus one prerequisite (channel-composite renderer) + one mid-batch clarification (Task 9 dirty-flag scope) + one sequencing note.

### 1. Multi-take recording capture-bleed bug

**What we agreed:** route to its own dedicated recording-lifecycle architecture batch.  NOT a quick fix; needs the dual-buffer model in §2 below to land first.

**Symptom (surfaced during Task 4 verify):** record take 2 on a Vox page that already has take 1 on the grid → take 2's WET file captures take 1's playback bleed instead of fresh live mic input.

**Root cause:** with take 1 on the grid, the FilePlay Pass 1 pre-scan sets `mVoxFilePlayActive[vi] = true` for that page during the overlap window.  Active FilePlay drives the BaySickVocal engine with the decoded take-1 audio → the engine's WET tap captures the FilePlay-decoded audio, not the live mic input.  In the same engine loop, the live-input branch is skipped while FilePlay is active.  DRY recorder is also broken in this state because `tapDryRecorder` lives in the now-skipped live branch.

**Why it can't be a one-line patch:** the engine architecture currently picks ONE input source per processBlock call (live OR FilePlay), and recorder taps live inside that branch.  Fixing the bleed requires the engine to accept BOTH inputs simultaneously during armed recording AND to expose distinct pre-merge tap points for the recorder.  That's a signature / API change to BaySickVocalProcessor (and a parallel one for BaySickInst), not a flag flip.

**Status:** open finding; dedicated batch to be named.  Not folded into QA-E.  QA-E Task 4 close stands; recording-lifecycle batch slots after the BaySickAlign-redesign-planning detour closes.

### 2. Dual-buffer recording architecture (the model the recording-lifecycle batch implements)

**What we agreed (Vox path during armed recording):**

```
live mic in ─┬─[DRY tap → DRY recorder]──┐
             │                            │
             └─[realtime pitch if         │
                bsv_pitch_realtime_bypass │
                is OFF]                   │
                     │                    │
                     └─[WET tap → WET     │
                        recorder, only    │
                        when realtime     │
                        pitch is ON]      │
                                          ├─ post-pitch-live ──┐
                                                                │
                                                                ├─ summed ─→ rack → EQ → out
                                                                │
existing FilePlay clip ─→ [decoded clip, bypasses realtime ────┘
overlapping playhead       pitch entirely]
```

**Inst parallel (no pitch stage / no WET tap):**

```
live in ──[DRY tap → DRY recorder]──┐
                                     ├─ summed ─→ BaySickPedals → BaySickNAMIR → out
existing FilePlay clip ─────────────┘
```

**Why:** the user has to HEAR both live + already-recorded clips through the same channel during armed recording (so they can perform along to take 1 while recording take 2), AND the recorder MUST capture only fresh live audio (no bleed from playback).  The single-input branch architecture can't satisfy both.  Dual-buffer means the engine takes two input streams, runs pitch correction (Vox) on the live stream only, taps recorder OUTPUT at pre-merge live-only points, and only sums into one stream AFTER the recorder taps fire.

**API shape:** BaySickVocalProcessor needs a new secondary-input setter or a new processBlock signature variant that accepts both buffers.  Same for BaySickInst.  Recorder taps move out of the (current) live-vs-FilePlay branch and into the pre-merge point on the live stream only.

**Status:** design locked; implementation lives in the recording-lifecycle batch (S1-S6 series implied).

### 3. Conditional WET recording

**What we agreed:** `startRecording` reads `bsv_pitch_realtime_bypass` from APVTS; if pitch is bypassed, the WET recorder is never allocated.  Only DRY recording happens.

**Why:** when realtime pitch is OFF the WET signal == the DRY signal (no pitch correction in the chain).  Writing two identical files is wasted disk + confusing semantics ("WET" implies post-effect; if no effect is applied there's no wet).  Storage win + correctness win.

**Status:** design locked; folds into the recording-lifecycle batch.

### 4. BaySickPitch — Mode C (realtime applicator + Render/bake)

**What we agreed:**

- **Default mode: realtime applicator.**  User opens a clip in the BaySickPitch editor, edits the pitch curve, hits Play; playback applies the edits per-block via PSOLA / PhaseVocoder DSP.  Iterative editing — try / listen / tweak without re-rendering each time.
- **Render / Freeze action available:** user-triggered action bakes the edits into a new wav file at `<project>/Pitched/<name>_pitched_v<N>.wav` (path subject to spec call).  Per-clip `usesOfflineRender` flag set; subsequent playback loads the rendered wav directly, dropping the runtime DSP cost.
- **Per-clip metadata storage as XML.**  Editor's file picker lists library entries filtered by `pageOwnerChannelId == this Vox page's voxInsert(pageIdx)`.  User picks any clip; editor loads that clip's saved edits.  Switching to a different clip in the picker loads that clip's edits.  Edits are per-clip-and-page, not global.
- **CPU/DSP at playback:** ~10-30% per active clip with edits applied.  Same order of magnitude as our existing PhaseVocoder-driven BPM stretch on audio clips.  Lazy-activate: clips with zero edits cost zero — the applicator only spins up when a clip actually has stored edits.
- **DSP not yet built.**  Editor + PitchTrackerYIN exist already from prior work; the realtime applicator DSP is net-new and lands in the BaySickPitch dedicated batch.

**Why realtime-applicator-default vs render-default:** the iterative editing UX wins.  Users editing pitch curves want immediate audible feedback as they drag points around — render-default forces a bake/listen/edit/re-bake loop that breaks flow.  Render/Freeze is offered as an opt-in optimization for when the user is done editing AND the runtime cost matters (e.g., 8 simultaneous Pitched clips during mixdown).  `usesOfflineRender` flag is per-clip so freezing one clip doesn't lock the others out of further editing.

**Why per-clip metadata vs global:** different clips need different edits.  A vocal take on bar 1 has different note targets than a vocal take on bar 32.  Per-clip storage scoped by `pageOwnerChannelId` keeps each Vox page's clip library independent.

**Status:** design locked; implementation lives in the BaySickPitch dedicated batch.  Lands AFTER the recording-lifecycle batch (Pitch needs the dual-buffer recording semantics established first so it has well-defined DRY clips to edit).

### 5. BaySickAlign — channel-composite-driven, single warp map per channel pair

**What we agreed:**

- **User picks Guide channel + Dub channel** (NOT clip-by-clip).  System renders both channels' grid composites — timeline-summed audio of every clip on each channel at its grid position.
- **`BaySickAlignDSP::analyzeOffline(guideComposite, dubComposite, ...)` is pure offline function that already exists.**  Takes two mono buffers in, returns one `WarpMap` out.  We DO NOT do realtime-capture-while-playing because we already have all the audio on disk as clips — feeding the offline analyzer directly with channel composites is strictly better than re-recording in realtime.
- **Single warp map per channel pair (NOT per-clip).**  Stored in project XML under the dub channel's state.  Per-clip timing variance is preserved naturally because each clip's onsets contribute anchors at their grid position; the warp curve at any timeline position reflects whatever clip(s) on the dub channel sit there.
- **Pitch matching extends the warp map** with per-anchor pitch shift values.  At each anchor's time, YIN detects F0 on both buffers; the semitone delta gets stored alongside the time-warp ratio.  Same data structure (`WarpMap`), two values per anchor.
- **Playback applicator:** when a dub-channel clip plays via FilePlay, look up the dub channel's warp map; for each block render, apply the slice corresponding to the current playhead position.  PhaseVocoder does both time-stretch (from warp ratio) AND pitch shift (from anchor semitones) in one pass.
- **Render / Freeze action:** bakes aligned + pitch-matched output to `<project>/Aligned/<name>_aligned_v<N>.wav` (matches the existing Option C design in `BaySickAlignDSP.h`).
- **Editor UI: three-lane composite display** — guide composite (top), dub composite (middle), aligned output preview (bottom).  Strength slider.  Analyze button.
- **Stale handling:** when either guide channel or dub channel grid changes (clip added / removed / moved), the warp map's reference data is outdated.  Mark stale; show visible "re-analyze" button.  NOT auto-re-analyze (analysis is expensive + user-disrupting).
- **Two-stage workflow with BaySickPitch:** BaySickAlign first (time alignment + coarse channel-level pitch match) → BaySickPitch second (fine note-level pitch correction on individual clips).

**Why channel-composite vs clip-by-clip:** realistic backing-vocal alignment is across-the-whole-take, not clip-by-clip.  A dub take that's 30ms early on bar 5 and 50ms late on bar 12 needs ONE consistent warp curve, not 47 independent clip warps that fight each other at the boundaries.  Channel-composite analysis sees the full timeline and produces a single coherent warp.

**Why offline-analyze (not realtime capture):** we own all the audio on disk before the user clicks Analyze.  Realtime capture is what you do when you don't have the data yet (live tracking session); offline analyze is strictly better when you do.  This is a real advantage of the in-box workflow.

**Why single warp map per pair vs storing per-clip:** the warp map is keyed by timeline position, not clip identity.  Clip onsets contribute anchors; the resulting curve handles clip boundaries naturally.  Storing one map per pair (instead of N maps for N clips) also means re-rendering is O(channels), not O(clips), and stale-detection has one trigger surface, not N.

**Status:** design locked; implementation lives in the BaySickAlign dedicated batch.

### 6. BaySickAlign existing editor needs full redesign

**What we agreed:**

- The current `BaySickAlignEditor` is a non-functional UI shell.  The Match Pitch panel + all its controls (MATCH PITCH, MAX DIFFERENCE, TARGET MODE, PITCH TARGET, SMART PITCH, ALGORITHM, TRANSPOSE) have no DSP backing — paint-only widgets.
- **Editor will be redesigned with a distinct visual identity AND distinct control names + layout.**  This is a design / scope decision: BaySickAlign is a BaySickDAW first-class feature and needs an editor whose visual + naming identity is its own.
- **Every UI control must wire to actual DSP functionality before commit.**  No paint-only widgets ship.  "No stubs" gate applies to the entire dedicated batch: every line item ships functional code + UI together, end-to-end verifiable before that line item commits.
- **Routed to its own dedicated batch.**  NOT folded into QA-E.  The redesign is significant enough scope that it deserves its own plan + close cycle.

**Why distinct identity:** BaySickAlign is one of BaySickDAW's headline workflow features (in-box, channel-composite-driven, two-stage with BaySickPitch).  The editor's visual + naming language should match the rest of BaySickDAW's L&F so it feels native, not vestigial.

**Why "no stubs" gate:** the current editor's paint-only widgets are exactly the trap we want to avoid going forward.  Shipping UI without DSP makes the feature look complete in screenshots but breaks when users actually try it.  Wiring DSP + UI in lockstep, per line item, keeps the demo and the product in sync.

**Status:** design locked.  Inventory pass on the existing editor + cross-reference to the §5 BaySickAlign dedicated batch entry is the next-action work in §10 below.

### 7. Composite renderer (prerequisite for both BaySickAlign and BaySickPitch)

**What we agreed:** build a function that walks a channel's grid clips and renders their summed playback timeline into a single mono buffer (offline, message thread).  Same code path as the "individual track export" feature already on the main plan — build it once, use it everywhere.

**Why shared:**

- BaySickAlign needs guide-channel + dub-channel composites for `analyzeOffline`.
- BaySickPitch (in non-default modes) may need a clip-summed buffer to bake against.
- Individual track export needs the same thing for WAV-out.
- Any future feature that operates on "this channel's playback as a single buffer" needs the same thing.

Three-way duplication of the same walk + sum logic is the smell to avoid.  Centralizing once makes each consumer trivial.

**Status:** prerequisite work; lands first in whichever dedicated batch needs it first (most likely BaySickAlign, since that's the next dedicated batch slotted).

### 8. Audio-clip-resize-doesn't-stretch finding

**What we agreed:** open finding; routing TBD by Jeff.

**Symptom (surfaced separately during Task 4 verify):** user resizes an audio clip block on the Builder grid.  Visual stretches / compacts as expected.  Audio doesn't time-stretch — clip plays a different portion of itself instead of stretching its existing content.

**Architectural gap:** block-resize event updates the visual representation + the block's `endBar` field, but doesn't propagate to the playback engine state.  AudioClipPlayer's stretch ratio isn't recomputed; PhaseVocoder isn't told the new target duration.

**Pattern blocks have similar shape:** visual feedback present, backend doesn't follow.  Same family of bug.

**Status:** captured here as a finding; routing decision (own dedicated batch / fold into existing audio-engine batch / Future State) is Jeff's call.  NOT inside QA-E.

### 9. Dirty-flag scope clarification (Task 9 in current plan)

**What we agreed:** the existing Task 9 (Dirty-flag investigation) verify steps should cover BOTH triggers — page-creation AND record-finalize — not be narrowed to one.

**Why:** the original observation that surfaced Task 9 mid-Task-3 was "post-record + save still shows dirty `*` on reopen".  In tracing that I'd informally narrowed Task 9 to the record-finalize path.  Page-creation is the second trigger worth checking in the same investigation — same underlying machinery (`markDirty()`), worth confirming both paths once we're in there.

**Status:** clarification only; folded into the existing Task 9 plan entry.  No new batch, no §9 Forks entry needed.

### 10. Sequencing decision

**What we agreed:**

- Task 4 source committed at `1d928fc`.  Stands as-is.
- The recording-lifecycle batch (dual-buffer architecture per §2 above, conditional WET per §3, all BaySickVocal API work) is a NEW dedicated batch — NOT folded into QA-E.
- The BaySickAlign redesign batch (per §5 + §6 above) is ALSO a NEW dedicated batch — NOT folded into QA-E.
- The BaySickPitch dedicated batch (per §4 above) is ALSO new.
- **QA-E resumes at Task 5** after the current detour into BaySickAlign-redesign-planning closes.  The detour is short — inventory the existing editor + cross-reference to the §5 BaySickAlign dedicated batch entry — and then control returns to QA-E.

**Why three separate batches not one mega-batch:**

- Each has its own well-defined scope + verify cycle + risk profile.
- Recording-lifecycle is mostly engine-API work; BaySickAlign is mostly editor + DSP wiring; BaySickPitch is mostly applicator DSP.  Different surface areas, different reviewers' mental models.
- Sequencing matters: recording-lifecycle first (clean DRY clips established), THEN BaySickAlign (operates on the clean clips), THEN BaySickPitch (fine corrections on top).  Forcing one batch would couple them artificially.

**Status:** sequencing decision locked.  Three new dedicated batches will appear in Main Plan §5 when their respective `/draft-doc forks-entry` + scoping passes complete.

### Disposition

- **QA-E open scope (still active):** Tasks 5-10 per the current plan file.  Resumes after the BaySickAlign-redesign-planning detour closes.
- **Routed to new dedicated batches (NOT in QA-E):**
  - Recording-lifecycle (dual-buffer + conditional WET + BaySickVocal API).
  - BaySickAlign redesign (editor + DSP wiring + composite renderer + warp map).
  - BaySickPitch (realtime applicator + Render/bake + per-clip XML metadata).
- **Open finding, routing TBD:** audio-clip-resize-doesn't-stretch (§8 above).
- **Task 9 scope clarified, no batch change:** dirty-flag investigation covers both page-creation AND record-finalize triggers.

### Next action

- Inventory the existing `BaySickAlignEditor` (current controls, current paint-only widgets, current DSP wiring or lack thereof) and cross-reference to the BaySickAlign-dedicated-batch §5 entry once that entry exists.  This is the BaySickAlign-redesign-planning detour that must close before QA-E Task 5 resumes.
- After the detour closes: resume QA-E at Task 5.
- The three new dedicated batches' `/draft-doc forks-entry` passes + Main Plan §5 entries are deferred until the BaySickAlign-redesign-planning detour produces enough scope clarity to write them.

---

(Subsequent entries appended below at every commit / sub-task verify / finding / spec call / scope pivot.)

---

## 2026-05-14 — Post-detour design lock-in: BaySickAlign + BaySickPitch redesigns; QA-Fb slotted; QA-J overlap interaction routed

Continuation of the 2026-05-14 detour started in the previous entry.  Where the previous entry locked the high-level shapes (dual-buffer recording, conditional WET, BaySickPitch Mode C, BaySickAlign channel-composite + warp map, editor-redesign-needed, composite renderer, audio-clip-resize finding, dirty-flag scope, sequencing into three new batches), THIS entry captures the concrete per-control redesign specs, the trade-dress framing clarification, sequencing position calls (QA-Fb slot + QA-J overlap interaction), and the consolidation pass queued for the Main Plan.

The previous entry's design narrative does NOT get re-captured here — it stands.  This entry appends what was decided AFTER it landed.

### 11. Existing editors are paint-only shells — confirmed at source

Sanity-checked the existing `BaySickAlignEditor` and `BaySickPitchEditor` against source before drafting redesigns.  Both editors hold a `BaySickVocalProcessor&` reference but never read from it.  Knobs / combos / toggles in both editors are pure local component state — no APVTS attachments, no DSP wiring, no parameter binding.

- `Source/DSP/BaySickAlignDSP.h/.cpp` exists with a working `analyzeOffline` pure function (FFT spectral-flux onset detection + greedy pairing producing a `WarpMap`).  **The class is never instantiated anywhere.**  `BaySickVocalProcessor` has no `BaySickAlignDSP` member — the offline analyzer is dead code.
- `applyWarp` in `BaySickAlignDSP.cpp` is a passthrough stub (`memcpy in → out`); no PhaseVocoder integration despite the file existing for weeks.
- **No `BaySickPitchDSP` class exists at all** — not in `Source/DSP/`, not anywhere.  Editor draws a Newtone-style canvas with pitch curves but there is no DSP that produces those curves nor any DSP that consumes user edits.
- `BaySickAlignEditor.cpp:8` source comment literally reads `"VocAlign-clone visual + interaction model"`.
- `BaySickPitchEditor.cpp:8` source comment literally reads `"Newtone-clone visual + interaction model"`.

Owner has flagged the paint-only-shell shape multiple times across prior sessions; this session confirmed at the source level before drafting the redesigns.  All UI in both editors is replaced or rewired in the dedicated batches.

### 12. Trade-dress framing — not trademark, and not universal idioms

Earlier in the conversation I used "trademark" loosely; clarified to the legally accurate framing.  The concern is **trade-dress** (visual + interaction identity creating a "passing off" appearance), NOT literal trademark on names or terms.

What is NOT a concern (universal DAW idioms — every DAW has these):
- Showing waveforms, pitch grids, piano keyboards, lane layouts.
- Tool names like "Slice" (universal across Ableton / Logic / Reaper / Cubase / FL Studio).
- Individual keybind reuse (Slice = S, etc. — universal).
- Concept-level features (pitch curves, time alignment, sync points).

What IS the concern (the cumulative bundle):
- The two source comments literally name VocAlign and Newtone as the visual + interaction model.
- Identical layout signatures + identical control names (MATCH PITCH / MAX DIFFERENCE / TARGET MODE / PITCH TARGET / SMART PITCH / ALGORITHM / TRANSPOSE on the Align side; the Newtone-style monospace bar at the top of Pitch with TEMPO / SYNC / Cut/Adv/Vib mode buttons).
- The convergence of those signatures with our identical engine names ("BaySickAlign" feeling like a VocAlign rebrand; "BaySickPitch" feeling like a Newtone rebrand).

Owner's framing: distinct visual identity + distinct control naming + identical engine names + identical keybinds together create the trade-dress risk.  Breaking ANY of those legs sufficiently neutralizes the cumulative bundle.  The redesigns below break the visual + control-naming legs hard while keeping the engine names + universal-DAW keybinds.

### 13. BaySickAlign — full redesign locked (every per-control decision)

#### 13a. Toolbar

- BaySickAlign title (BaySickEngineLabel teal `#0FAFA5`) — **KEEP**.
- Preset combo — **REPLACED** with 6 entries:
  - Loose-Align
  - Loose-Align+Pitch
  - Close-Align
  - Close-Align+Pitch
  - Tight-Align
  - Tight-Align+Pitch
  - Selecting a preset sets Align Mode + Pitch ON/OFF together (the +Pitch variants enable the Pitch box).
- **Save Preset + Load Preset — ADD**.  User presets save to `Documents/BaySickDAW/Presets/BaySickAlign/My Presets/{name}.xml`.  Mirrors the BaySickPlayer / BaySickSynth save-patch flow established in Phase D.
- **Modified-indicator green dot** — **REPURPOSED** as preset-dirty flag.  Lights when current Align + Pitch param values diverge from the selected preset's stored values.  Tooltip: "Preset modified — save changes".  Same dot, new semantic.
- **Auto-Preview button** — **REMOVED**.  No realtime auto-preview pass; analyze on user click.
- **Settings button** (had been planned earlier in the session as a way to hide the right panel) — **DROPPED**.  Right panel stays always visible.  Reasoning: user picking a preset should see the knobs change immediately so they LEARN what each preset is doing.  Hiding the panel breaks that learning UX.
- **Undo / Redo** — **KEEP**.

#### 13b. Lanes

- 3-lane vertical layout — **KEEP**.
- Lane name renames:
  - Guide → **Leader**.
  - Dub → **Follower**.
  - Output → **Output** (unchanged).
- **Lane colors (palette CORRECTION applied mid-conversation):**
  - Leader = **Bass active green**.
  - Follower = **Vox active teal**.
  - Output = **Drums active red**.
  - Earlier in this session I'd written "Layers active green" for the Leader lane.  Owner corrected: Layers is actually **orange** and Bass is **green**.  Corrected here AND it propagates back to BaySickPitch's pitch-curve overlay color (see §14b below).  Hex values to be looked up in source when implementing.
- **Capture buttons (Leader + Follower)** — **REMOVED**.  No realtime capture path; channel-composite renderer feeds the lanes from grid clips.
- **Render button** (currently in toolbar / lane area) — **REMOVED as a separate button**.  Replaced by the DSP-side render-to-bake action — toolbar Render button + DSP function combined into a single Render that bakes aligned output.
- **Sidechain picker** — **REMOVED**.  Channel-composite auto-resolves the source — user picks Leader channel + Follower channel, no sidechain wiring needed.
- **Dub Process-Group combo** — **REMOVED**.  No group concept; channel-pair is the unit of work.
- **Centerline-only paint** — **REPLACED** with real composite waveform rendering (uses the shared composite renderer per §7 in the prior entry).
- **Time ruler** — **REDESIGNED**: shared across all 3 lanes (currently Guide-only).  Single ruler at top of the 3-lane stack synchronizes timeline reference across Leader / Follower / Output.

#### 13c. Sync Points + Protected Areas

- **Sync Points — KEEP.**  Functional spec:
  - User-placed manual anchors that override auto-pairing.
  - Click empty strip → drop marker at (Follower=X seconds, Leader=Y seconds).
  - Drag handles to adjust either side's time independently.
  - Right-click → delete.
  - "Automatic Sync Points" menu item seeds high-confidence onset pairs as a starting baseline that user can then adjust.
  - The pairing algorithm only auto-pairs onsets that fall BETWEEN user-placed sync points (sync points act as hard boundaries that segment the timeline into pairing regions).
- **Protected Areas — KEEP.**  Drag-create regions on the Follower-side that exempt parts from time-warp / pitch-shift / both.  Right-click toggles which dimensions the area protects.

#### 13d. Bottom controls

- **ViewModeBar (Wave / Pitch / Energy buttons)** — **KEEP buttons, ADD rendering branches**.  Currently the buttons exist but only wave-mode renders.  Pitch mode renders YIN-detected F0 contour on each lane; Energy mode renders RMS envelope.
- **HistoryScrubber (Del / + / - buttons)** — **KEEP**.
- **HistoryScrubber render-list paint** — **ADD**.  Currently the buttons exist but the render list paints empty.  Each entry shows render version + date.

#### 13e. Right-side panel — single panel, two boxes, always visible

Replaces the existing Match Timing / Match Pitch / OTHER three-panel structure with a single always-visible two-box panel.

**Box 1 — Align** (renamed from "Match Timing"):

- **ON master toggle** — **KEEP**.  Master enable for the Align box.
- **Mode dropdown** (renamed from ALIGNMENT RULE) — three options:
  - Loose
  - Close
  - Tight
  - This is the **single source of truth** that drives:
    1. Fine Tune knob's base value (Loose=150ms / Close=100ms / Tight=50ms — see below).
    2. Pitch box Range knob's center position + min/max travel (see §13f).
    3. Pitch algorithm strength behavior at the current Range setting.
- **Fine Tune knob** (renamed from MAX DIFFERENCE) — bipolar ±50ms travel from the Mode-set base value.  Total reach by Mode:
  - Tight: 0-100ms (base 50ms).
  - Close: 50-150ms (base 100ms).
  - Loose: 100-200ms (base 150ms).
  - Knob label visible.  Bipolar visual (12 o'clock = Mode base; left = tighter; right = looser).
- **Smart Align toggle** — **REMOVED**.
- **MAXIMUM SHIFT control** — **REMOVED**.
- **High Resolution toggle** — **REMOVED**.

**Box 2 — Pitch** (renamed from "Match Pitch"):

- **ON master toggle** — **KEEP**.  Master enable for the Pitch box.  Independent of preset — user can toggle Pitch off/on after selecting a +Pitch preset.
- **Range knob** (renamed from MAX DIFFERENCE) — mode-dependent center + range:
  - Loose: default 0%, range 0-50%.
  - Close: default 50%, range 25-75%.
  - Tight: default 100%, range 50-100%.
  - The Mode dropdown in Box 1 sets both the default position and the min/max travel of this knob.
- **TARGET MODE** — **REMOVED**.
- **PITCH TARGET** — **REMOVED**.
- **SMART PITCH** — **REMOVED**.
- **Algos dropdown** (renamed from ALGORITHM) — three options:
  - PSOLA
  - Granular
  - Phase Vocoder
- **Transpose** (-12..12 st) — **KEEP**.
- **Formant Shift ON toggle + rotary** — **MOVED HERE** from the old OTHER panel.

**OTHER panel** — **REMOVED ENTIRELY**.  Pitch Ranges combos dropped (replaced by Mode-driven Range knob behavior).  Formant Shift moved to the Pitch box.

**SidePanelTabs (T/P/O glyphs)** — **REMOVED**.  Panel always visible per the no-Settings-button decision in §13a above.

#### 13f. Pitch behavior per preset — algorithm spec captured verbatim from owner

Per-preset pitch processing strength:

- **Loose:** algorithm applies 0% of the Leader's localized pitch variations to the Follower.  Pitch box may be enabled but Range knob sits at 0% by default — minimum coercion.
- **Close:** plugin interpolates the difference by pulling the Follower's pitch closer to the Leader's pitch center while flattening wild deviations and pulling flat/sharp notes toward the Leader, WITHOUT completely erasing the Follower's natural human variation.
- **Tight:** overrides the Follower's pitch entirely — forces pitch envelope to match the Leader's pitch contour and every micro-variation, pitch scoop, and vibrato wave from the Leader vocal is mathematically mapped onto the Follower.

These three strength bands are the algorithmic distinction the +Pitch presets target.  The Range knob lets the user fine-tune within the Mode's band.

#### 13g. DSP additions confirmed for the BaySickAlign batch

- **BaySickAlignDSP instance on processor** — new member on `BaySickVocalProcessor` (or wherever the channel-pair state lives).  Currently no instance exists anywhere.
- **Composite renderer** — shared with BaySickPitch per §7 in the prior entry.  Walks a channel's grid clips and produces a single mono buffer.
- **applyWarp PhaseVocoder integration** — current `applyWarp` stub gets replaced with real time-stretch + pitch-shift via PhaseVocoder.
- **YIN pitch detection** — for the Pitch box's per-anchor semitone-delta computation.
- **PSOLA / Granular / Phase Vocoder pitch shifters** — three algos per the Algos dropdown.  Shared with BaySickPitch.
- **Formant-preserve** — for the Formant Shift rotary.  Shared with BaySickPitch.
- **Sync points data model** — user-placed anchors persisted in project XML under the channel pair's state.
- **Protected areas data model** — user-drawn regions persisted in project XML.
- **Render-to-bake** — output writes `<project>/Aligned/{name}_align_v{N}.wav` matching the existing Option C design comment in `BaySickAlignDSP.h`.
- **Render history persistence** — per-channel-pair list of bake versions stored in project XML.
- **~20 APVTS params** — every Align box control + every Pitch box control + Mode dropdown + preset selection + dirty flag get APVTS-backed.

### 14. BaySickPitch — full redesign locked (every per-control decision)

#### 14a. Composite mode CORRECTION — works for Pitch too

Earlier in the conversation I'd said composite-mode "can't" work for Pitch.  Corrected mid-conversation by owner:

- Same channel-composite renderer as BaySickAlign produces a mono buffer of the channel's grid clips.
- YIN runs once over the composite.
- Note segmenter produces note regions with **absolute timeline positions** (bar.beat coordinates, not clip-relative).
- User edits (pitch shifts, formant shifts, vibrato curves, volume curves) stored at the **channel level**, keyed by absolute timeline position.
- Realtime applicator (Mode C from the prior entry §4) maps the running clips' audio to the composite slices during playback — when clip X is currently being played by FilePlay, the applicator looks up which note region(s) in the channel composite overlap with the clip's current playhead and applies the stored edits.
- **Edits flow through to normal playback with no bake step required.**  Render/Freeze is still available as the opt-in optimization, but the default flow is realtime applicator on the channel composite.

This makes BaySickPitch composite-driven in the same architectural shape as BaySickAlign.  Both editors are channel-level operations.  The two are siblings, not unrelated tools.

#### 14b. Toolbar redesign (composite mode collapses Newtone-style bloat)

- BaySickPitch title (BaySickEngineLabel teal `#0FAFA5`) — **KEEP**.
- **File label** — **REMOVED**.
- **TEMPO display** — **REMOVED**.
- **SYNC info display** — **REMOVED**.
- **LENGTH label** — **RETAINED** with format `X bars / M:SS.f`:
  - Default: shows composite length in bars + minutes:seconds.fractional (e.g., `16 bars / 0:32.0`).
  - When selection active: toggles to `SEL X bars / M:SS.f` showing selection extent.
- **Preset combo + Save / Load + dirty dot** — **ADD**.  Mirrors the Align machinery from §13a above.  Presets save to `Documents/BaySickDAW/Presets/BaySickPitch/My Presets/{name}.xml`.  Independent preset library from Align (a "Loose" Pitch preset is not the same XML as the "Loose-Align+Pitch" Align preset).
- **Undo / Redo** — **ADD**.
- **Loop button** — **REMOVED**.  Internal-transport loop region UI removed.
- **Play / Stop internal transport** — **REMOVED**.  Uses the global Builder transport — composite playback is sync'd to the global playhead, no separate transport.
- **Cut / Adv / Vib edit modes** — **COLLAPSED to Slice / Edit (2 modes)**:
  - Vibrato / Formant / Volume become per-note sub-curves drawn UNDERNEATH the note region.  Always visible when the parent note is selected.  Draggable depth / rate / value points on the sub-curves.  No mode toggle needed to access them.
  - Slice mode: split a note region at click position.
  - Edit mode: drag note regions vertically (pitch) or horizontally (start/end).  Drag sub-curve points (vibrato depth, formant amount, volume shape).
- **Slice keybind** — safe (universal DAW term per §12 above).  Existing piano-roll / Builder Slice tool keybind shape is fine to reuse.
- **Slaved Playback Mode** — **REMOVED**.  Composite is always sync'd to the global Builder transport — no slaved/free distinction.
- **Auto-Scroll keybind (currently A)** — **KEEP** (individual keybinds not a legal concern per §12).
- **Load button** — **REMOVED**.  Composite auto-resolves from the channel's grid clips — no manual file load.
- **Save / Render button** — **KEEP function, RENAME to "Render"**.  Wires to the shared bake pipeline (writes to `<project>/Pitched/{name}_pitch_v{N}.wav`).
- **Reset button** — **KEEP**.  Clears all edits on the composite.
- **"→PR" button** — **REDESIGNED** to "Send Notes to..." popup:
  - Target list = active Layers tabs + active Bass tabs + active Drums tabs + active Clips tabs.
  - Clips appears as a target ONLY when an active Clips page exists (Clips supports MIDI playback through its sample-player mode).
  - Only MIDI notes are sent to the target (the detected pitch contour quantized to notes), not the vocal audio itself.
- **Three global knobs renamed with labels visible:**
  - CENTER → **Focus**.
  - VARIATION → **Mod**.
  - TRANS → **Speed**.

#### 14c. Canvas

- **Vertical MIDI keyboard on the left edge** — **KEEP**.
- **Bar + beat ruler on the top edge** — **KEEP**.
- **Grid lines** — **KEEP**.
- **Note regions = Pills (Option B from the 3-option matrix surfaced earlier in the conversation):**
  - Rounded rectangles with subtle 4-6px corner radius.
  - NOT the Newtone-style heavily-rounded blob shape.
  - Fill color = **Effects page active purple**.
- **Waveform drawn inside pills** — color = **Vox active teal**.
- **Pitch curve overlay** — color = **Bass active green** (replaces Newtone's orange `#e89c5a`).  Same green as the Align Leader lane per §13b above.
- **Playhead during composite playback** — **ADD**.  Currently no playhead visualization.
- **Loop region UI** — **REMOVED** (Loop button gone per §14b).
- **Per-edit-mode mouse interaction** — **ADD**:
  - Slice mode: cursor splits note on click.
  - Edit mode: drag note regions + sub-curve points.
- **Per-note sub-curves below pills (Volume / Formant / Vibrato)** — **ADD**.  Always visible for the selected note.  Three sub-curves stacked under the pill.  Draggable depth / rate / shape points on each.

#### 14d. InfoBar (bottom)

- **Monospace readout (Pitch / Cents / Length)** — **KEEP concept, ADD population**.  Currently the bar exists but never populates.  Wire to detected pitch + cents-deviation + composite length at the hover/selection position.
- **"Loop" field in the readout** — **DROP** (Loop button + region gone).

#### 14e. Scroll / Zoom

- **Horizontal + vertical scroll** — **KEEP** all (generic JUCE Viewport mechanisms).
- **Zoom in / out** — **KEEP** (Ctrl+Scroll standard).

#### 14f. Preset structure

- **3 mode presets** — Loose / Close / Tight — drive the three global knobs (Focus / Mod / Speed).
- **Save Preset + Load Preset + dirty dot** — same machinery as Align per §13a.

#### 14g. DSP additions confirmed for the BaySickPitch batch

- **Composite renderer** — shared with Align per §7 in the prior entry.
- **YIN pitch tracker** — runs once over the composite at analyze time.
- **Note segmentation** — produces note regions with absolute timeline positions.
- **Pitch shifter (PSOLA / Granular / PhaseVocoder)** — shared with Align.
- **Vibrato analyze + synthesize** — detect existing vibrato depth/rate per note, allow user override via sub-curve.
- **Formant shifter per-note** — sub-curve drives per-note formant amount.
- **Volume envelope per-note** — sub-curve drives per-note gain shape.
- **Per-channel pitch-edit ValueTree storage** — edits stored under the channel's project XML state, keyed by absolute timeline position.
- **Render-to-bake pipeline** — shared with Align.  Writes `<project>/Pitched/{name}_pitch_v{N}.wav`.
- **Realtime applicator** (Mode C) — maps clip playback to composite slices.
- **Render history** — separate list from Align's; per-channel list of bake versions in project XML.
- **~10-15 APVTS params** — global knobs + preset selection + dirty flag + Mode-driven internals.

### 15. Color palette correction — cross-references the BaySickAlign + BaySickPitch batches

Earlier in this session I'd written "Layers active green" for the Align Leader lane.  Owner corrected mid-conversation: Layers is actually **orange**, Bass is **green**.

Canonical color references for the redesign batches:

| Bucket | Active color |
|---|---|
| Layers | **orange** |
| Bass | **green** |
| Vox | **teal** |
| Drums | **red** |
| Effects page | **purple** |

This correction propagates to:

- BaySickAlign Leader lane (§13b) = Bass active **green**.
- BaySickAlign Follower lane (§13b) = Vox active **teal**.
- BaySickAlign Output lane (§13b) = Drums active **red**.
- BaySickPitch note pills (§14c) = Effects page active **purple**.
- BaySickPitch waveform inside pills (§14c) = Vox active **teal**.
- BaySickPitch pitch curve overlay (§14c) = Bass active **green** (same as Align Leader lane — both editors use Bass green for the "pitch" visual signal).

Hex values for each active color to be looked up in source (VibeLAF palette constants) at implementation time.

### 16. QA-J overlap interaction — Option 1 picked (test with sequential clips only)

QA-J (Main Plan §5 line 1031, sequencing arrow line 1387) restructures per-row audio-clip rendering so that the rack + EQ chain runs ONCE on the SUM of overlapping clips on a row, instead of N times sequentially through shared engine state.

The new BaySickAlign / BaySickPitch / recording-lifecycle test scenarios will involve overlapping audio clips on the same Vox row (multiple takes recorded on top of each other, or backing-vocal alignment of layered takes).  Those scenarios WILL hit the existing per-row pass-through-N-times bug — comp envelope / reverb tail / LFO phase will bleed from clip A into clip B's processing pass.

Three options surfaced for handling the interaction:

- **Option 1: Design QA-F / QA-Fa / QA-Fb tests with sequential clips on the same row only.**  Defer overlapping-same-row scenarios to QA-J re-verify.
- **Option 2: Block QA-F / QA-Fa / QA-Fb on QA-J landing first.**  Forces QA-J ahead in the sequence.
- **Option 3: Land QA-F / QA-Fa / QA-Fb knowing the overlap bug exists; re-verify those batches' overlap behavior post-QA-J.**

**Owner's pick: Option 1.**  Confirmed verbatim: "i'll line clips back to back to test things".

Implication for the verify cycles in QA-F / QA-Fa / QA-Fb plan files: every test scenario uses sequential clips on a row (back-to-back, no overlap).  Overlapping-same-row testing comes back as a QA-J re-verify scenario.

#### Fork-note structure agreed

- QA-F + QA-Fa §5 entries get inline "QA-J re-verify required for overlapping-same-row scenarios" notes.
- QA-Fb §5 entry (when written) gets the same inline note.
- QA-J §5 entry gets an inline "re-verify QA-F / QA-Fa / QA-Fb overlapping-same-row scenarios at close" note.
- §9 Forks gets a new entry dated 2026-05-14 documenting the fork itself + the Option 1 decision + the cross-references.

### 17. Recording-lifecycle batch slotted as QA-Fb

The "dedicated recording-lifecycle batch" referenced in the prior entry's sequencing decision (§10) gets the slot **QA-Fb** in the Main Plan §6 sequencing arrow.

Updated sequence: `... QA-E → QA-F → QA-Fa → QA-Fb → QA-G → ...`

Scope (per the prior entry's §2 + §3 + §8 + §9 plus this entry's audio-clip-resize):

- Channel-composite renderer (shared dependency for QA-F + QA-Fa, but its first home is the batch where it first ships — likely QA-F if implementation begins there, else QA-Fb).
- Dual-buffer recording (live + FilePlay simultaneously) per the prior entry §2.
- Conditional WET tap (skip when realtime pitch bypassed) per the prior entry §3.
- Multi-take capture-bleed fix per the prior entry §1.
- Audio-clip-resize-doesn't-stretch finding per the prior entry §8 (slotted into QA-Fb after this entry's sequencing pass — the audio-clip-resize bug shares the "block state doesn't propagate to backend playback" family with the recording-lifecycle work).
- Dirty-flag investigation (page-creation + record-finalize) — per Task 9 fold-out from QA-E.  QA-E's Task 9 was inserted mid-Task-3; the dirty-flag work now lives in QA-Fb where the recording-lifecycle architecture is the right surface to investigate.

**Slot rationale (owner's call):**

- QA-F is the BaySickVocal DSP audit / regression fix cluster — fixes the BaySickAlign + BaySickPitch + realtime pitch correction DSP that was broken at QA-Inventory.
- QA-Fa is the BaySickPitch audio-import additive feature.
- QA-Fb sits between Fa and QA-G as the recording-lifecycle architecture lift.  Comes AFTER the DSP work in QA-F is functional (the dual-buffer model needs the DSP it's routing around to actually exist), and BEFORE QA-G's Builder UX work (which assumes recordings land cleanly).

Per `feedback_slot_placement_is_spec_call.md`, the slot pick is the owner's, not mine.  Owner picked QA-Fb between QA-Fa and QA-G.

### 18. Planned plan-doc consolidation pass (NOT YET APPLIED — running notes capture first)

The following Main Plan edits are queued for the consolidation pass that lands AFTER this running-notes append commits:

**§5 entries:**

- **QA-F entry (line 944):** replace existing scope with consolidated BaySickAlign design from §13 above + add QA-J overlap-interaction fork-out note per §16.
- **QA-Fa entry (line 958):** replace existing scope with consolidated BaySickPitch design from §14 above + add QA-J overlap-interaction fork-out note per §16.
- **§5 INSERT new QA-Fb entry** after QA-Fa.  Scope per §17 above (channel-composite renderer / dual-buffer recording / conditional WET / multi-take capture-bleed fix / audio-clip-resize / dirty-flag).  Add QA-J overlap-interaction fork-out note per §16.
- **QA-J entry (line 1031):** add fork-in note for "re-verify QA-F / QA-Fa / QA-Fb overlapping-same-row scenarios" per §16.

**§6 sequencing arrow (line 1386):**

- Insert `→ QA-Fb` between `QA-Fa` and `QA-G`.
- Update §6 footnotes if any reference the QA-F / QA-Fa pair to also reference QA-Fb.

**§9 Forks (new eighteenth entry dated 2026-05-14):**

- Title: "BaySickAlign + BaySickPitch redesign scope + recording-lifecycle batch (QA-Fb) slot + QA-J overlap-interaction fork."
- Trigger: QA-E Task 4 verify surfaced multi-take capture-bleed + opened a longer design discussion; this entry's §11-§17 are the consolidated locks.
- Decision: scope-lock QA-F (BaySickAlign redesign) + QA-Fa (BaySickPitch redesign) + QA-Fb (recording-lifecycle), all three to follow the no-stubs gate (every UI line item ships functional code + UI together).
- Plan files affected: Main Plan §5 (QA-F / QA-Fa / new QA-Fb / QA-J updates), §6 sequencing arrow.
- Verification: at QA-F / QA-Fa / QA-Fb close, batch-close drafter confirms the no-stubs gate held + the overlap-interaction note flowed through to QA-J's re-verify list.

Owner asked for this running-notes capture FIRST before the Main Plan consolidation pass.  Order: running-notes commit → Main Plan consolidation commit → resume QA-E Task 5.

### 19. QA-E status — still paused at Task 5

- QA-E Tasks 5-10 remain after this detour.
- Task 5 onward in the current QA-E plan file is NOT touched by this detour — the BaySickAlign + BaySickPitch redesigns route entirely to QA-F / QA-Fa / QA-Fb.
- After the Main Plan consolidation pass commits, control returns to QA-E Task 5.

### 20. BaySickNAM/IR wiring audit — CLEAN

Owner asked for a sanity-check pass on BaySickNAM/IR to confirm every UI element is wired to a real APVTS param + actually does something.  Audit was read-only; no source changes.

**File ownership clarification (corrected mid-conversation):**

- `BaySickNAMIR` lives at `Source/BaySickNAMIR/BaySickNAMIREditor.h/.cpp` + `BaySickNAMIRProcessor.h/.cpp`.
- Used on **both Vox and Inst pages** (Inst hosts it as a sub-tab "BaySickNAM/IR" peer to BaySickPedals; Vox hosts it as a sub-tab in its chain — confirmed by `Source/Inst/InstPage.h:20`: "same setup as the Vox page's NAM/IR sub-tab").
- I'd initially routed audit findings to QA-F (Vox DSP) which was wrong — BaySickNAM/IR is an independent engine module, not part of BaySickVocal.

**APVTS layout — 18 params declared in `createLayout()`:**

| Param | Type | Range | Default |
|---|---|---|---|
| `input_gain` | Float | -24..+24 dB | 0 |
| `output` | Float | -24..+12 dB | 0 |
| `gate_threshold` | Float | -60..0 dB | -50 |
| `gate_release` | Float | 5..500 ms | 100 |
| `nam_bypass` | Bool | — | false |
| `cab_bypass` | Bool | — | false |
| `low_cut` | Float | 20..500 Hz | 20 |
| `high_cut` | Float | 3000..20000 Hz | 20000 |
| `cab_mix` | Float | 0..100 % | 100 |
| `oversampling` | Choice | 1x/2x/4x | 1x |
| `ab_slot` | Choice | A/B | A |
| `nam_micsim_mode` | Choice | None/Built-in/User IR | None |
| `nam_micsim_model` | Choice | 10 archetypes | Live Vocal Dynamic |
| `nam_micsim_mix` | Float | 0..100 % | 100 |
| `nam_placement_distance_cm` | Float | 1..150 cm | 30 |
| `nam_placement_angle_deg` | Float | -90..+90 deg | 0 |
| `nam_placement_polar` | Choice | 5 patterns | Cardioid |
| `nam_placement_mix` | Float | 0..100 % | 100 |

**Editor → APVTS wiring — all 18 controls correctly bound:**

- **Standard JUCE attachment helpers (12 controls)**: `mInGainKnob`, `mGateThreshKnob`, `mGateReleaseKnob`, `mLowCutKnob`, `mHighCutKnob`, `mCabMixKnob`, `mOutputKnob`, `mMicSimMixKnob`, `mMicPlacementDistanceKnob`, `mMicPlacementAngleKnob`, `mMicPlacementMixKnob`, `mNamBypassToggle.btn()`, `mCabBypassToggle.btn()` — `SliderAttachment` / `ButtonAttachment`.
- **Manually-synced APVTS controls (6 controls)**: `mOSSelector` (chicken head ↔ `oversampling`), `mSlotABtn` / `mSlotBBtn` (radio TextButtons ↔ `ab_slot`), `mMicSimMode` (chicken head ↔ `nam_micsim_mode`), `mMicSimModelCombo` (ComboBox ↔ `nam_micsim_model`), `mMicPlacementPolar` (chicken head ↔ `nam_placement_polar`).  These use custom selector widgets where the stock JUCE attachment helpers don't fit (chicken head is custom; ButtonAttachment only works for `AudioParameterBool` + ToggleButton).  All 6 manually sync via `addParameterListener` + `onChange` callback that calls `setValueNotifyingHost` — equivalent semantics to the standard attachment, fully automatable.

**Action UI (truly non-parameter — by design):**

- `mNamBrowseBtn` / `mIrBrowseBtn` / `mMicSimUserIrBtn` — file pickers, call `loadNamModel` / `loadImpulseResponse` / `loadUserMicIr`.  Not automatable by design (you don't automate "load file X").
- Recent-files popup (right-click on browse buttons) — reads `Documents/BaySickDAW/settings.xml`.
- File drag-drop — handles `.nam` + `.wav` correctly via `FileDragAndDropTarget`.

**Processor → APVTS reads in `processBlock`:**

All 18 params are read via `apvts.getRawParameterValue(...)` in the per-block snapshot.  No dead params, no orphaned reads.  The 2026-05-06 DSP gate at the top of `processBlock` cleanly short-circuits the entire chain when no NAM model + no IR are loaded (CPU saver, semantically correct).

**A/B per-slot snapshot system — fully wired:**

`SlotSnapshot` struct captures 16 per-slot fields (knob values + bypass toggles + Mic Sim / Placement params + per-slot user IR path).  On `ab_slot` change, `captureSnapshotFromCurrent(outgoing)` + `applySnapshotToCurrent(incoming)` fire via `parameterChanged` listener.  Serialized as `<SlotA>` / `<SlotB>` children in `getStateInformation` / `setStateInformation`.  Pre-H-6d projects with no snapshot tags fall back to default-constructed snapshots without crashing.

**Verdict:** zero dead UI elements, zero unwired params.  BaySickNAM/IR is in excellent shape.  No follow-on QA-Fd batch needed.

### 21. BaySickPedals wiring audit — CLEAN

Same audit pattern on `Source/BaySickPedals/BaySickPedalsEditor.h/.cpp` + `BaySickPedalsProcessor.h/.cpp`.  Inst-only engine (Vox does not have a pedals sub-tab — typical: vocal chains skip stomp-pedal stages).

**APVTS layout — 8 params declared:**

`bsp_slot0_bypass` through `bsp_slot7_bypass` — per-slot bypass `AudioParameterBool`s only.  Each pedal's internal params live inside its own `DSPBase` instance, not at the processor's APVTS level.  This is by-design and architecturally clean — mirrors the `EffectRack` pattern (per-effect params owned by each effect's APVTS, host APVTS holds only slot-level concerns like bypass).

**Editor → APVTS wiring:**

- Each `PedalSlotComponent` creates a `ButtonAttachment` from its bypass LED footswitch (`mBypassBtn`) to `bsp_slot{N}_bypass`.  8/8 bypass toggles correctly attached.
- Per-pedal control panels are created via `createEffectEditor(dsp, type, EditorPanelBase::PanelMode::Pedal)` — each `DSPBase` subclass's own panel handles its internal param bindings.  The Pedals editor doesn't try to micromanage per-pedal params; it delegates to the established effect-panel system.

**Processor → APVTS reads in `processBlock`:**

For each slot, reads `isSlotBypassed(slotIdx)` which queries `bsp_slot{N}_bypass`.  Bypassed slots skip processing; active slots delegate to `eff->process(buffer)`.  Process order: 0 (Tuner) → 1..6 (user-adjustable) → 7 (EQ).

**Action UI (non-parameter — by design):**

- `mPresetBtn` (per-tile "..." menu) — per-pedal preset save/load via `EffectPresetIO`.
- `mRemoveBtn` ("X") — clears slot via `mProc.clearSlot(mSlot)`.
- `mUpBtn` / `mDownBtn` — drag-reorder via `mParent.performMove(...)`.
- `mEqPicker` (slot 7 only) — ComboBox swaps EQ type via `onEqPickerChanged` → `loadEffect(7, newEqType)`.
- Title-bar `mPresetBtn` (pedalboard-level Save/Load) — wired via `onPedalboardPresetMenu` callback that `InstPage` hooks up.
- Drag-to-reorder (mouseDown on title bar) — for slots 1-6 only; locked-position slots 0 / 7 never move.

**Per-slot locking enforcement:**

`isEffectAllowedInSlot` correctly enforces:
- Slot 0 (Tuner): only `EffectType::None` or `TunerStyle`.
- Slot 7 (EQ): only `None` / `GraphicEQStyle` / `BassGraphicEQStyle` / `FurmanEQStyle`.
- Slots 1-6: any `EffectType` EXCEPT the 4 slot-locked types above.

**Snapshot capture / restore — fully wired:**

`captureFullState` / `restoreFullState` round-trip via `kStateRootTag = "BaySickPedalsState"` with version 1.  Per-slot snapshot includes type + base64-encoded DSP state blob.  2026-05-05 Bug B fix: bulk-restore fires `onSlotsExternallyChanged` callback to force editor rebuild even when slot types match (DSP pointers swapped underneath).  Pedalboard preset library at `Documents/BaySickDAW/Presets/Pedalboards/{name}.xml`.

**Verdict:** zero dead UI elements, zero unwired params.  The per-pedal-params-owned-by-DSPBase architecture is correct — keeps the BaySickPedalsProcessor APVTS surface minimal (8 params for 8 slots).  No follow-on QA-Fd batch needed.

### 22. APVTS attachment terminology clarification

Mid-audit owner asked about whether `ab_slot` was automatable since I'd categorized the Slot A / B buttons under "Non-APVTS UI elements."  That categorization was misleading.

**The accurate framing:**

- **APVTS-backed parameters** are those declared in `createLayout()` and accessible via `apvts.getRawParameterValue` / `apvts.getParameter`.  These are visible to DAW host automation, MIDI mapping, project serialization, etc.  Whether the UI uses a standard JUCE attachment helper is a separate concern.
- **Standard attachment helpers** (`SliderAttachment` / `ButtonAttachment` / `ComboBoxAttachment`) are convenience wrappers for the common cases.  They only support specific control-to-param-type pairs (e.g., `ButtonAttachment` only binds `AudioParameterBool` to `ToggleButton`).
- **Manually-synced controls** are UI widgets that bind to APVTS params without using a stock attachment helper — typically because the widget is custom (chicken head, radio-style TextButton) or the param type doesn't fit a stock helper (Choice param driven by 2 radio buttons instead of a ComboBox).  Manual sync uses `addParameterListener` + `onChange` callbacks that call `setValueNotifyingHost` — same semantics, fully automatable.

**Applied to the NAM/IR audit:**

All 18 params (including `ab_slot`) are APVTS-backed and automatable.  My earlier prose mixed "no standard attachment helper" with "non-APVTS" — those aren't the same thing.  The §20 audit table above uses the corrected framing.

### 23. QA-Fc spec — BaySickNAMIR Dual-Mic Stack

**Purpose:** simulate two microphones on the same source (the existing single-mic chain becomes Mic A; a parallel Mic B path is added).  Real-recording workflow — most pro recordings use 2+ mics on guitar cabs, vocals, drums, etc.  Each mic captures different spatial information; summed result has more energy + dimension than either alone.

**Architecture — parallel paths, NOT a blend:**

Current single-mic chain in `processBlock` (BaySickNAMIRProcessor.cpp:430-452):
```
post-cab buffer
  → Mic Sim (mode + model + mix)
  → Mic Placement (distance + angle + polar + mix)
  → Master Output
```

New dual-mic chain:
```
post-cab buffer
  ├── (copy to mPreMicScratch)
  ├── Mic A path (in-place on buffer):
  │     → Mic Sim A (mode + model + mix)
  │     → Mic Placement A (distance + angle + polar + mix)
  │
  └── if Mic B Active:
        copy mPreMicScratch → mMicBScratch
        → Mic Sim B  (mode + model + mix)
        → Mic Placement B (distance + angle + polar + mix)
        sum mMicBScratch into buffer
  → Master Output
```

The summing step amplifies correlated signal naturally (two mics on same source → +3 dB on perfectly correlated content, less on uncorrelated content, plus comb-filter colouration depending on the path differences — exactly how two real mics behave).  This is NOT crossfaded / blended; both mics contribute their full output and the result is additive.

**New APVTS params — 8 total (mirrors existing Mic Sim + Placement, with `_b_` infix):**

| Param | Type | Range | Default | Purpose |
|---|---|---|---|---|
| `nam_micb_active` | Bool | — | false | Master enable for Mic B path (off = identical to today's single-mic) |
| `nam_micsim_b_mode` | Choice | None/Built-in/User IR | None | Mic Sim B mode |
| `nam_micsim_b_model` | Choice | 10 archetypes | Live Vocal Dynamic | Mic Sim B model |
| `nam_micsim_b_mix` | Float | 0..100 % | 100 | Mic Sim B wet/dry |
| `nam_placement_b_distance_cm` | Float | 1..150 cm | 30 | Mic Placement B distance |
| `nam_placement_b_angle_deg` | Float | -90..+90 deg | 0 | Mic Placement B off-axis angle |
| `nam_placement_b_polar` | Choice | 5 patterns | Cardioid | Mic Placement B polar pattern |
| `nam_placement_b_mix` | Float | 0..100 % | 100 | Mic Placement B wet/dry |

When `nam_micb_active == false`, the entire Mic B path is bypassed in `processBlock` (zero CPU cost beyond the param read) and the chain behavior is byte-identical to the current single-mic chain.

**Per-slot snapshot extension:**

`SlotSnapshot` (BaySickNAMIRProcessor.h:139-168) currently has 16 fields.  Add 8 new fields mirroring the new APVTS params, plus serialize/deserialize:

```cpp
// Mic B (new in QA-Fc)
bool         micbActive          { false };
int          micSimBMode         { 0 };
int          micSimBModel        { 0 };
float        micSimBMix          { 100.0f };
juce::String micbUserIrPath;
float        placementBDistance  { 30.0f };
float        placementBAngle     { 0.0f };
int          placementBPolar     { 1 };
float        placementBMix       { 100.0f };
```

`captureSnapshotFromCurrent` / `applySnapshotToCurrent` extended to handle the 9 new fields (8 APVTS params + the user IR path).  A/B slot switching preserves dual-mic settings independently per slot — slot A might use single-mic, slot B might use dual-mic with specific mic configs.

**New processor members:**

```cpp
MicSimDSP        mMicSimB;         // parallel Mic Sim instance for Mic B path
MicPlacementDSP  mMicPlacementB;   // parallel Mic Placement instance for Mic B path
juce::AudioBuffer<float> mPreMicScratch;  // saves post-cab buffer state for Mic B input
juce::AudioBuffer<float> mMicBScratch;    // Mic B processing buffer (summed into main at end)
```

`prepareToPlay` calls `prepare` on both new DSPs and sizes both scratch buffers to host block size × 2 channels.

**Editor layout changes:**

Current Mic Sim row (kMicSimRowH=100) + Mic Placement row (kMicPlaceRowH=100) span the full editor width.  In QA-Fc:

- Add a "Mic A | Mic B" column split inside each row.  Each column takes ~half the editor width.
- Mic A column = existing controls (Mode chickenhead, Model combo, User IR picker, Mix knob for Mic Sim row; Polar chickenhead, Distance / Angle / Mix knobs for Mic Placement row).
- Mic B column = mirror of Mic A controls bound to the new `_b_` params.
- New "Mic B Active" toggle (DualLabelToggle, same as nam_bypass / cab_bypass pattern) lives at the top of the Mic B column.  When OFF, Mic B controls dim/disable visually.
- Owner's exact mental model: "the two mix knobs would need to be doubled so 2 knobs for each setup basically taking the current mix setup and squishing it to half the screen and adding second one right next to it."  Layout follows that mental model — current single-mic UI squished to half width, second mic mirrors it on the right half.

Editor height may need a small bump (existing kEditorH=560) if the side-by-side layout doesn't fit the current row heights — calculate during implementation.

**Picks up automatically on both Vox + Inst:**

QA-Fc lives at the BaySickNAMIR engine surface.  Both Vox and Inst pages embed the editor unchanged — single batch of work; both pages get dual-mic for free.

**Scope:** medium-large.  ~2-4 hours for processor changes, ~3-5 hours for editor layout work, ~1 hour for snapshot expansion + state migration testing, ~1-2 hours verify.  Total: ~7-12 hours.

**Risk:** medium.  New APVTS params + new DSP path on a heavily-used engine.  Worst case: Mic B bypass logic broken → either silent Mic B (zero impact, just unused feature) or Mic B always active (changes Mic A's behavior — would be caught immediately by ear).

**Dependencies:**

- QA-F (BaySickVocal DSP fix) does NOT block QA-Fc — BaySickNAMIR is its own engine, audit just confirmed it's already cleanly wired.
- QA-Fb (recording-lifecycle) does NOT block QA-Fc — orthogonal surfaces.
- BUT: QA-Fc requires the audit to confirm dual-mic isn't being silently added on top of a broken single-mic.  Per §20 the audit is clean, so QA-Fc has a verified-functional foundation to build on.

**Verify scenarios:**

- Mic B Active OFF → byte-identical output to today's single-mic chain (regression check).
- Mic B Active ON with identical settings to Mic A → output is 2× Mic A's amplitude (correlated sum) — sanity check for the parallel-paths-not-blend architecture.
- Mic A Distance=30cm, Mic B Distance=120cm → audible comb-filter colouration from path difference (the natural reason dual-mic captures sound "fuller").
- A/B slot switch with different Mic A vs Mic B configs per slot → tone snaps correctly to the new slot's stored dual-mic state.
- Project save → reopen → all Mic B params + Mic B user IRs restore correctly (including per-slot snapshots).
- Mic B Active toggle automation in DAW → smooth crossover into / out of dual-mic mode (per-block bypass, not click-prone).
- CPU cost with Mic B Active vs OFF → measure delta; OFF should be ~zero added cost (single param read + branch).

### 24. Updated sequencing — QA-Fd not needed, QA-Fc remains

After the audits in §20 + §21 came back clean, the conditional QA-Fd batch (NAM/IR + Pedals wiring fixes) is **not needed**.  No follow-on wiring work surfaced.

Updated sequence:

```
... QA-E → QA-F → QA-Fa → QA-Fb → QA-Fc → QA-G → QA-H → QA-I → QA-J → ...
```

QA-Fc dual-mic sits after QA-Fb recording-lifecycle, before QA-G Builder UX.  Both BaySickNAM/IR (engine surface for QA-Fc) and BaySickPedals (Inst-only sibling engine) are clean foundations; QA-Fc is additive feature work, not wiring repair.

§9 Forks entry (eighteenth) implication: the entry already drafted in §18 above gets one additional sub-bullet noting the audits came back clean + QA-Fd was queued and dropped + QA-Fc remains.  Eighteenth Forks entry is now a four-decision package: BaySickAlign redesign + BaySickPitch redesign + QA-Fb slot + QA-Fc slot.

### Disposition

- Detour scope-lock complete.  **Four** dedicated batches' scope locked via this running-notes entry + the queued Main Plan consolidation pass (QA-F redesign, QA-Fa redesign, QA-Fb recording-lifecycle, QA-Fc dual-mic).
- No source code changed in this detour — pure design + scoping work + read-only audits.
- QA-E Task 4 close commit (`1d928fc`) + Task 3 follow-up commit (`54f41c8`) stand unchanged.
- Task 5 (next QA-E task) resumes after the Main Plan consolidation pass commits.

### Next action

- Surface this running-notes diff + dispatch `/draft-commit` for the running-notes-only commit (now covering §11-§24 in a single commit).
- After Jeff approves + the commit lands: apply the Main Plan consolidation pass (§5 QA-F / QA-Fa / new QA-Fb / new QA-Fc / QA-J updates + §6 sequencing arrow + §9 eighteenth Forks entry covering all four batch decisions).  Surface that diff + dispatch `/draft-commit` for the consolidation commit.
- After the consolidation commit lands: resume QA-E Task 5 per the current QA-E plan file.

## 2026-05-15 — Task 5 — REC-01 R-1-c (BLU-470 doc + verify) — verified-complete, pre-close capture

QA-E execution RESUMED at Task 5 after the 2026-05-14 BaySickAlign/BaySickPitch design-lock detour closed (QA-F / QA-Fa / QA-Fb / QA-Fc spec work landed in `8784edf` + `9a10d59`; that detour did NOT touch QA-E Tasks 5-10 per §19 above).  Task 5 = "REC-01 R-1-c (BLU-470 documentation + verify + fix)".  Original Task 5 scope was a single doc deliverable + 5 recording-lifecycle verify scenarios.  Verify surfaced ~9 bugs; all fixed in-batch per the no-defer rule (`feedback_qa_batches_fix_bugs_dont_defer.md`), with Jeff explicitly picking Option A (full scope in-batch) for the largest sub-cluster (drag dedupe + delete cascade).  Task 5 is now verified-complete in Debug + Release; no source changed AFTER verify passed.  This running-notes capture precedes the close commit.

### 25. Doc deliverable applied (original Task 5 scope)

The stale `### Recording finalize (StandaloneEditor.cpp)` entry in [Plans & Specs/Carry-Forward Reference.md](../../Plans%20%26%20Specs/Carry-Forward%20Reference.md) §3 was REPLACED (not added-alongside) with a new `### Recording lifecycle (per-armed-strip WAV capture, post-FILE-01)` entry.

**Why replace (Jeff picked Option B over Option A — add-alongside):**

- The old entry's line refs were ALL stale: it cited `:9455`-`:9517` vs the actual current finalize span `StandaloneEditor.cpp:9842-9947`.
- Its diagnostic claim ("Calls present, library doesn't show entries -- REC-01 bug is in `addAudioToLibrary` itself or browser refresh path") was a pre-QA-E diagnosis that Task 4's library-driven page-owner model (commit `1d928fc`) invalidated wholesale.  Keeping it alongside a correct entry would leave a contradicting stale diagnosis in a frozen reference doc.

**New entry documents:**

- `StripRecorder` struct location (`PluginProcessor.h:653-666`).
- Tap helper entry points (`PluginProcessor.cpp:3581+`).
- Finalize span (`StandaloneEditor.cpp:9842-9947`).
- File-naming convention for per-armed-strip WAV capture.
- The post-Task-4 `pageOwnerChannelId` page-binding model — explicitly noting the pre-Task-4 `VoxPage::setDryClipPath` API was removed in FILE-01 (so future readers don't go looking for it).

### 26. Verify scenarios + the multi-take master-record investigation

5 scenarios run in Debug (Jeff's standing per-task verify also covers Release):

1. Full Vox record — PASS.
2. Full Inst record — PASS.
3. Master-mix fallback record — see Issues 1+2 below.
4. Per-track arm — drove the arm/channel-select decouple cluster (§28).
5. Debug pops — no Debug-only regression observed in the recording path.

**Master-record "Issues 1+2" (ruled non-bugs):**

- Issue 1: master-record produced a silent WAV.
- Issue 2: master-record stopped after ~1 measure.
- Both were transient — neither reproduced after an app restart.  Ruled non-bugs; most likely cause is stale Pattern/Song transport state left over from earlier testing in the same session (not a code defect in the recording path).
- Pattern mode also rechecked clean after restart.

**Issue 3 (real):** the drag/delete behavior cluster.  This became the bulk of Task 5's surfaced work — see §29.

### 27. Bug fix — lengthBeats (drag-from-browser block sizing)

`BuilderPage::importAudioFile` (and the new `placeAudioLibraryEntry`, §29) now set `block.lengthBeats` to the exact file-duration beat count instead of leaving it at `-1.f`.  The `-1.f` sentinel fell back to `lengthBars * 4` — bar-rounded, so a 2.3-bar clip rendered as a 3-bar block.  Fix mirrors the recording-finalize `dropWavAsClip` path, which already computed exact beats from file duration.

### 28. Bug fix cluster — arm/channel-select decouple + monitor-without-arm

**Arm rework (decouple input-channel picker from arm state):**

- Right-click the Arm LED now opens the input-channel picker WITHOUT arming.
- Left-click toggles `_arm` directly via a standard `ButtonAttachment`.
- REMOVED the C3 (2026-05-04) manual parameter-listener workaround entirely: dead-code deleted from `MixerTrackStrip.h/.cpp` — `mApvtsForListener` + `mArmParamId` members gone, the `juce::AudioProcessorValueTreeState::Listener` base class dropped, `parameterChanged` override deleted.  The standard `ButtonAttachment` now does what the manual listener was hand-rolling.
- `MixerLedButton` gained an `onRightClick` callback + `mouseDown` override (`SharedUI.h`) to support the new right-click-to-pick gesture.
- `MixerPage::showInputChannelPicker` no longer auto-writes `_arm`.  `_arm` is only written by the explicit "Disarm" item in the picker.  The channel tick now reflects the current input-channel selection regardless of arm state (previously the picker conflated "pick a channel" with "arm").

**Monitor-without-arm:**

- Live input now flows through the Vox/Inst chain when EITHER `_arm` OR `_listen` is engaged (was armed-only).
- Fixed across all 3 surfaces: `VoxStripTask.cpp`, `InstStripTask.cpp`, and the serial Vox+Inst path in `PluginProcessor.cpp`.
- `tapDryRecorder` stays armed-only — monitoring produces no recording (monitor is audition, not capture).
- Confirmed: master-mix-fallback now captures monitored input because a monitoring strip routes to master, so the master tap sees it.

### 29. Bug fix cluster — drag dedupe + library schema + disk-dup prompt + delete cascade + tab-close cascade

Jeff approved Proposal 1 + Proposal 2 + Option A (full scope in-batch) for this cluster after its size was surfaced and flagged (including the library schema change).

**Library schema change (`PatternManager`):**

- `addAudioToLibrary` dedup key changed from path-only to (path, `pageOwnerChannelId`).  One file can now route to multiple pages — this is what enables the "New page" prompt option below.
- Legacy compat: re-adding with `channelId=0` still upgrades an existing entry's owner (preserves the Task 4 retag-on-reroute behavior).
- New helpers: `findAudioLibraryIndexByPath`, `countAudioLibraryEntriesForChannel`, `removeAudioFromLibraryAt`.

**Browser->grid bypass (kills the spurious duplicate-library-entry bug):**

- `BuilderPage::itemDropped` "audio" branch now calls the new `placeAudioLibraryEntry` (resolves the stored path, reads metadata, drops a routed block) INSTEAD of `importAudioFile`.
- Root cause of the spurious dup: `importSample`'s `(2)`-rename fallback fired because of a Windows `juce::File` stored-vs-resolved path-equality mismatch — the relative stored path and the resolved absolute path compared unequal, so the importer thought it was a brand-new file and minted a duplicate library entry with a `(2)` suffix.  `placeAudioLibraryEntry` reuses the existing entry instead of round-tripping through the importer.

**Disk-dup prompt:**

- `filesDropped` now detects a dropped file that already exists in the library (resolve-and-compare absolute paths) and fires a new `onDuplicateFileDrop` callback.
- StandaloneEditor shows a "Use Existing / New Page / Cancel" prompt.
  - Use Existing -> `placeAudioLibraryEntry` (reuse the existing routed entry).
  - New Page -> spawn a forced-duplicate Clips tab + a second library entry tagged to the new channelId.
  - Cancel -> no-op.

**Browser delete cascade:**

- New `BrowserPanel::confirmAndDeleteLibraryEntry` — confirmation prompt before any delete.
- Last-file-out (the owning page owns ONLY this entry) closes the owning page via new `onClosePageForChannelId` (StandaloneEditor walks `mPages` -> `closeTab`).
- Partial delete (page still owns other entries) keeps the page open.
- Both browser-delete sites route through this; the flat-list site converts path -> libIdx first.

**Tab-close library cascade:**

- `onTabClosed` for Clips/Vox/Inst now removes owned library entries + their matching (path, channel) blocks.
- This REPLACES the pre-Task-5 "no-file-delete contract": the audio file on disk is still preserved (we only remove library entries + blocks, never the file).
- Prompt verbiage in `ClipsPage`/`VoxPage`/`InstPage` `requestDelete` updated to state that the library entries are removed.

### 30. Bug fix — block coloring + relative-path resolve + disk-drop retag

`drawAudioClip` now colours audio blocks by `routeChannel`:

| Range | Channel | Colour |
|---|---|---|
| 400-449 | Clips | `0xffd4a017` (amber) |
| 600-605 | Vox | `0xff0fafa5` (teal) |
| 700-705 | Inst | `0xff1c3a8a` (navy) |
| other / unrouted | generic | teal-grey |

Ranges mirror `MixerChannelIds` (`Source/VibeGraph.h`: kAudioBase=400, kVoxBase=600, kInstBase=700) but are kept as literals in `BuilderPage` deliberately (avoids pulling the heavy header into BuilderPage just for 3 constant ranges).

**Root cause #1 (spurious red on Vox/Inst + disk-drop blocks):**

- The `missingFile` check ran `juce::File(b.audioFilePath).existsAsFile()` on RELATIVE paths (e.g. `"Samples/..."`), which resolve against the EXE CWD and therefore always fail -> the block got the red "missing file" treatment, which shadowed the route colour.
- Fixed by resolving the stored path via `onResolveStoredPath` first (mirrors the lengthBeats fix §27 + `placeAudioLibraryEntry` §29).
- Why Clips-from-browser worked "by accident": ClipsPage stores ABSOLUTE paths in the library, while Vox/Inst recordings + disk drops store RELATIVE — so only the relative-path producers hit the false-missing path.

**Root cause #2 (disk-drop blocks coloured generic/blue):**

- Disk-drop blocks are created with `routeChannel=0` BY REQUIREMENT — `routeChannel=0` is what makes the `onAudioClipAdded` callback fire and spawn the Clips page.  They were never retagged after the page spawned, so they stayed generic-coloured.
- Fixed: after `spawnClipsTabIfMissing`, the handler retags matching `routeChannel==0` blocks to `audioInsert(row)`, guarded on a Clips page actually existing at that row.
- This is functionally a no-op for playback — `routeChannel 0` and `audioInsert(row)` resolve to the same insert — it only makes the routing explicit and the colour correct.

### 31. Bug fix — delete-last-page navigation

- Symptom: deleting the last library entry of a page from the Builder browser closed the owning tab, and `onTabClosed`'s G-7 empty-state surfacing then yanked the view to the (now-empty) Clips/Vox/Inst empty-state page — disorienting because the user was on the Builder, not the page being closed.
- Fix: capture `closedPageWasVisible` BEFORE `mPages.remove`, and only show the empty-state if the closed page was the one currently on screen.
- Result: a browser-cascade close from the Builder now stays on the Builder.

### 32. Design decisions / spec calls locked

- **Live colour derivation.** Block colour is derived LIVE from `routeChannel` every paint — there is NO stored colour field.  Re-routing a block recolours it automatically.  Jeff confirmed this is intended: "colour follows routing, not creation origin."
- **Future user-colour-override compatibility.** A future per-block user-colour override (Builder work) is compatible: the route logic becomes the auto fallback when no user colour is set.  Precedent exists (F-1 per-pattern user colours).  OPEN spec for later: does a user-set colour survive a re-route?  NOT decided now — flagged for whenever the Builder user-colour work is specced.
- **Carry-Forward §3.** Replace (Option B) chosen over add-alongside (Option A) — see §25.
- **Old-project recolour: Option A (no load-time migration).** Pre-fix saved blocks keep `routeChannel=0` -> they show cosmetic blue/generic on reopen; audio is CORRECT (`routeChannel 0` and `audioInsert(row)` hit the same insert).  Jeff accepted the cosmetic inconsistency over reopening Task 5 for a load migration.  A load migration (Option B) was specced but declined.
- **Scope growth.** Task 5 grew from "doc + 5 verify" to ~9 in-batch fixes via the no-defer rule.  Jeff explicitly picked Option A (full scope in-batch) when the drag/delete cluster's size — including the library schema change — was surfaced and flagged.

### 33. Files touched (16, all Task 5)

- [Plans & Specs/Carry-Forward Reference.md](../../Plans%20%26%20Specs/Carry-Forward%20Reference.md)
- Source/Clips/ClipsPage.cpp
- Source/Engine/Tasks/InstStripTask.cpp
- Source/Engine/Tasks/VoxStripTask.cpp
- Source/Inst/InstPage.cpp
- Source/PatternManager.cpp
- Source/PatternManager.h
- Source/PluginProcessor.cpp
- Source/Standalone/BuilderPage.cpp
- Source/Standalone/BuilderPage.h
- Source/Standalone/MixerPage.cpp
- Source/Standalone/MixerTrackStrip.cpp
- Source/Standalone/MixerTrackStrip.h
- Source/Standalone/SharedUI.h
- Source/Standalone/StandaloneEditor.cpp
- Source/Vox/VoxPage.cpp

### Disposition

- Task 5 verified-complete in Debug + Release (Jeff's standing per-task verify covers both — no separate Release re-verify gate at task close per `feedback_no_full_release_reverify_at_batch_close.md`).
- No source code changed AFTER verify passed; this running-notes capture precedes the close commit.
- Scope grew per the no-defer rule; all growth was surfaced + Jeff picked Option A (full scope in-batch).  No deferrals to Future State / new batch.
- Carry-Forward §3 doc deliverable applied (replace, Option B).

### Next action

- /draft-commit -> surface drafted commit message + full pre-commit git status -> commit on Jeff's explicit approval (per `feedback_surface_drafted_commit_message_for_approval.md`).
- Then QA-E Task 6 (DSP-09 Bus solo) is next.

## 2026-05-15 — Task 6 — DSP-09 (Bus solo) spec call — outcome: punt to new batch QA-Ea

QA-E Task 6 = "DSP-09 (Bus solo)".  Per the mandatory pre-task spec-call protocol the
task opened by reading Carry-Forward §3 "Bus solo" + §4 "Decisions Already Made",
reading the bus-solo dispatch in `Source/VibeGraph.cpp` + `Source/PluginProcessor.cpp`,
and surfacing still-open sub-calls to Jeff.  The spec-call investigation produced a
**topology finding** that materially changed the implementation approach and led Jeff
to **punt the whole of DSP-09 to a new dedicated batch QA-Ea**.  NO source changed in
Task 6 — pure spec / diagnosis work.  Task 5's close commit `6b044aa` stands.  This
running-notes capture is COMPACTION-CRITICAL: the next session works from this entry +
the queued Main Plan / batch-plan edits, not from conversation memory.

### 34. Locked baseline (§4 Carry-Forward — NOT re-litigated)

DSP-09 target behavior, already locked in Carry-Forward §4:

- Solo a bus -> that bus + everything routed into it plays normally; every OTHER bus
  is silenced at the master mix.
- This is **NOT** FL-style global "any solo silences every other strip."  Bus solo is
  scoped to the bus layer only.

Frozen, not reopened.  Task 6 work was the spec call on still-open sub-questions + a
diagnosis of the reported "Drums plays when Layers solos" bug.

### 35. Diagnosis — why "Drums plays when Layers solos"

Bus-solo logic is scattered across **THREE inconsistent sites** with three different
formulas — the user-visible root cause.

- **Site 1 — `BusNode::processBlock` (Layers/Bass/Drums):** pairwise triad at
  `Source/VibeGraph.cpp:355-363` (mirrored :522 Bass, :682 Drums):
  `anySolo = thisSolo || bassSolo || drumSolo`, `g = (thisMuted || (anySolo && !thisSolo)) ? 0 : fad`.
  This part DOES correctly zero the Drums BusNode's own output when Layers is soloed —
  not the leak.
- **Site 2 — `processBus` (Clips/Vox/Inst/Vox2/Inst2/Inst3/Rusty):** a DIFFERENT
  `useGroupSolo` formula.  ClipsBus special 6-bus check at `VibeGraph.cpp:1698-1702`.
  Rusty standalone (`inGroupSolo=false; useGroupSolo=false;`) at `VibeGraph.cpp:1727-1728`
  — Rusty never participates in the bus-solo group.
- **Site 3 — receive-group `busAnySolo`** at `PluginProcessor.cpp:2572-2577` checks ONLY
  clips/vox/inst/vox2/inst2/inst3/fx solo — EXCLUDES layers/bass/drums bus solo.  So
  soloing Layers leaves it `false` and Vox/Inst/Clips keep playing.
- **Master-sum stage ungated:** `VibeGraph.cpp:1547-1570` sums
  `layersBuf + bassBuf + drumsBuf + audioClipsPreRendered + masterExtra` with ZERO solo
  gating; the `kMaster` accumulator (`masterExtra`, `VibeGraph.cpp:1566`) is fully ungated.

Root cause: even though the Drums BusNode is correctly silenced (Site 1), any drum
insert/send fanning into a path that lands in the ungated `masterExtra` (or the excluded
Site-3 formula) bypasses the gate -> drums still audible while Layers is soloed.
Structural, not a single wrong comparison.

### 36. Topology finding — the legacy split (key architectural discovery)

- **Bus INPUTS uniform:** every bus (incl. Layers/Bass/Drums) gets input from a
  per-channel accumulator that `routeInsertOutput` fans per-strip outputs into.
- **Bus OUTPUTS NOT uniform:** Layers/Bass/Drums bus outputs are hardcoded into
  dedicated `layersBuf`/`bassBuf`/`drumsBuf` and explicitly summed into the master-input
  buffer at `VibeGraph.cpp:1551-1553`.  Vox/Inst/Vox2/Inst2/Inst3/FX/Rusty bus outputs
  go through the generic `routeInsertOutput(busChId,...)` -> `kMaster` accumulator
  (`masterExtra`) path (`PluginProcessor.cpp:2593`).
- All paths converge into the SAME master-input `sumBuf` — not separate outputs, two
  different internal assembly mechanisms feeding one buffer.
- **Phase-1 (original AudioProcessorGraph shim: Layers/Bass/Drums + bespoke master sum)
  vs 5F-4b (April 2026 unified routeInsertOutput + channel-accumulator model) LEGACY
  SPLIT.**  The expansion added the generic path for every new bus and never retrofitted
  the original three.  BusNode self-documents dead legacy synth-fallback paths.
- **This asymmetry IS the structural root of the scattered DSP-09 solo logic.**  Three
  gate sites exist because there are two output-assembly paths plus the ungated final
  sum; a clean single-gate fix is only possible after the output paths are unified.

### 37. Sub-calls surfaced + Jeff's decisions

- **A — Solo+Mute same bus:** MUTE WINS (silent regardless of solo).  Confirmed.
- **B — direct-to-Master bypass routes during a bus solo:** original proposal (gate
  `masterExtra` wholesale) was FLAWED — Jeff caught it: the soloed Vox/Inst bus's own
  post-FX output is routed INTO that same `masterExtra` accumulator (bus `_sendTo`
  default -> kMaster), so a wholesale gate would zero the soloed bus itself.  **Resolved
  = B1: direct-to-Master bypass routes are NOT silenced by bus solo** (an explicit user
  main-cable to Master is intentional, survives solo).  **B2 rejected** (gate at
  `routeInsertOutput` source = re-spreading solo logic into the router).  Gate stays in
  `processBus` + `BusNode` only.
- **C — Multi-bus solo:** ADDITIVE — a bus plays iff itself soloed; solo Layers+Bass ->
  both audible, others silent.  Confirmed.
- **D — strip-solo vs bus-solo (CORRECTED):** per-strip `_solo` is ALREADY GLOBAL —
  `VibeGraph::isAnyInsertSoloed()` (`VibeGraph.cpp:2688-2695`) scans ALL EIGHT insert
  maps (Layer/Bass/Drum/Audio/Aux/Vox/Inst/Rusty).  Solo one strip -> only it heard;
  solo multiple -> all soloed heard.  Strip-solo NOT subordinate to bus-solo, NOT
  group-local.  **DSP-09 does not touch strip-solo.**  **CRITICAL GUARDRAIL (carry into
  QA-Ea):** code warns at `VibeGraph.cpp:1876-1885` that bus-solo logic must NEVER be
  fed `isAnyInsertSoloed()` (prior serial bug muted whole buses when one strip soloed).
  The bus-solo gate must read BUS `_solo` params ONLY (`mixer_layersbus_solo`,
  `mixer_bassbus_solo`, `mixer_drumsbus_solo`, `mixer_clipsbus_solo`, `mixer_voxbus_solo`,
  `mixer_instbus_solo`, `mixer_voxbus2_solo`, `mixer_instbus2_solo`,
  `mixer_instbus3_solo`, `mixer_fx_solo`, `mixer_rustybus_solo`).  QA-Ea `/review-batch`
  must verify the new helper does not read `isAnyInsertSoloed()`.
- **E — Persistence:** automatic — `mixer_{bus}_solo` are APVTS params, serialized with
  project XML.  Factual, not a spec call.

### 38. Implementation options analyzed

- **Option 1 — shared `anyBusSoloed()` helper, both gate sites.**  New
  `VibeGraph::anyBusSoloed()` (true if any bus `_solo` set, BUS params only per §37-D).
  Replace the BusNode triad (Site 1) + the processBus `useGroupSolo`/ClipsBus-6-bus/
  Rusty-standalone variants (Site 2) with one formula
  `silenced = thisMuted || (anyBusSoloed && !thisBusSoloed)`.  Delete dead
  `busAnySolo`/`fxBusAnySolo`/`localAnySolo`.  Fixes the user-visible bug; two
  output-assembly paths still exist but gated identically from one source of truth.
  Risk MEDIUM (hot-path behavior replacement, net simplification).
- **Option 2 — ALSO unify the output path.**  Option 1 PLUS route Layers/Bass/Drums bus
  outputs through `routeInsertOutput` -> kMaster like every other bus; delete
  `layersBuf`/`bassBuf`/`drumsBuf` + the bespoke master sum (`VibeGraph.cpp:1547-1570`).
  One output path, one gate site, removes the bug CLASS.  Risk HIGH — touches
  master-sum, MT `MasterTask` structure, BusNode buffer model.  A real audio-engine
  refactor, well beyond "fix bus solo."

### 39. DECISION (Jeff's call) — punt DSP-09 + Option 2 to new batch QA-Ea

- **Option 2 is the architecturally-correct fix and what we actually want.**  BOTH the
  DSP-09 bus-solo fix AND the Layers/Bass/Drums output-path unification (Option 2) are
  **PUNTED to a NEW dedicated batch QA-Ea**.
- QA-Ea gets its own plan file + its own `/review-batch` pass specifically targeting the
  hot-path safety concerns (master-sum, MT `MasterTask`, BusNode buffer model, the
  §37-D `isAnyInsertSoloed()` guardrail).
- **Rationale (Jeff):** same code area + same test material as QA-E, executed in "a
  slightly different order" — QA-Ea runs **adjacent to QA-E**.  Slot/sequencing is
  Jeff's confirmed call per `feedback_slot_placement_is_spec_call.md`.
- **DSP-09 REMOVED from QA-E Task 6 entirely.**  "Drums still plays" is NOT fixed in
  QA-E — it moves to QA-Ea.  QA-E Task 6 fully vacated (Task 6 was 100% DSP-09).
- Scope-reduction + new-batch creation on the OPEN QA-E batch (NOT a closed-batch
  carry-forward) -> documented via a §9 Forks entry per Main Plan §0 Rule 3, no prior
  commit reopened.

### 40. QA-E impact + bookkeeping queued for next session (NOT yet applied)

- QA-E proceeds from Task 5 (closed, `6b044aa`) directly to **Task 7 — FILE-02
  (Properties dialog consolidation + Routing dropdown)**.  Task 6 slot vacated.
- **Plan-doc edits queued (next session, Jeff drives):**
  - (a) Main Plan §5 — new **QA-Ea** entry (scope: DSP-09 bus-solo fix + Layers/Bass/
    Drums output-path unification; own plan file + own `/review-batch` for hot-path
    safety); strike/annotate DSP-09 out of QA-E scope.
  - (b) Main Plan §6 sequencing arrow — insert **QA-Ea** adjacent to QA-E.
  - (c) Main Plan §9 Forks — new entry (likely the **nineteenth**; confirm count
    against the live §9 — the eighteenth was the 2026-05-14 BaySickAlign/Pitch/QA-Fb/
    QA-Fc package): DSP-09 spec-call topology finding + legacy-split root cause
    (Phase-1 vs 5F-4b) + Option 1 vs 2 + punt-to-QA-Ea decision + rationale + QA-Ea
    scope.
  - (d) QA-E batch plan `Plans & Specs/Batch Plans/phantom-recording-mongoose.md` —
    Task 6 section: mark DSP-09 moved to QA-Ea; renumber/annotate so Task 7 (FILE-02)
    is the next executable task.

#### FILE-02 locked §4 decisions (note for the Task 7 pre-task)

Captured durably so the Task 7 pre-task spec-call doesn't re-litigate them:

- Routing dropdown options = **Vox + Inst + Clips**.
- **Cross-over allowed** (a clip may route across these page types).
- Use case = **remote-collab** workflow.
- Reassignment timing = **immediate**, via `rebuildRoutingFromApvts`.

The Task 7 pre-task still runs the standard mandatory spec-call read, but these four
items are LOCKED inputs, not open questions.

### Disposition

- QA-E Task 6 (DSP-09 Bus solo) spec-call **complete**.  Outcome = punt to new batch
  QA-Ea (DSP-09 bus-solo fix + Option-2 Layers/Bass/Drums output-path unification).
- **No source changed in Task 6** — pure spec / diagnosis / topology work.
- QA-E Task 5 close commit `6b044aa` stands; no prior commit reopened.
- Scope-reduction + new-batch creation on the OPEN QA-E batch, recorded via §9 Forks
  per Main Plan §0 Rule 3 (NOT a closed-batch carry-forward).
- QA-E Task 6 slot fully vacated; QA-E resumes at Task 7 (FILE-02).

### Next action

- Apply the §40 plan-doc bookkeeping in order: (1) Main Plan §5 QA-Ea entry +
  strike/annotate DSP-09 out of QA-E; (2) Main Plan §6 sequencing arrow QA-Ea adjacent
  to QA-E; (3) Main Plan §9 Forks new entry; (4) QA-E batch plan Task 6 -> QA-Ea
  annotation, Task 7 becomes next executable.
- Surface the consolidated plan-doc diff + dispatch `/draft-commit`; commit on Jeff's
  explicit approval.
- After the bookkeeping commit lands: resume QA-E at **Task 7 — FILE-02**, running the
  standard mandatory pre-task spec-call read with the four locked §4 decisions above as
  fixed inputs.

---

## 2026-05-15 — Task 7 — FILE-02 (Properties dialog + Routing dropdown) — implemented as full two-level routing model

QA-E resumed at **Task 7 — FILE-02** after the §40 plan-doc bookkeeping landed
(Task 6 vacated -> QA-Ea).  The standard mandatory pre-task spec-call read ran
with the four LOCKED §4 inputs from §40 ("FILE-02 locked §4 decisions") as
fixed inputs (Vox+Inst+Clips options, cross-over allowed, remote-collab use
case, immediate reassignment).  This entry is the durable mid-batch capture:
the implementation is complete but UNCOMMITTED, awaiting Jeff's Debug-then-
Release verify.  Compaction-critical — the next session works from this entry,
not conversation memory.

### 41. Task 7 spec calls locked (Jeff's calls during the Task 7 conversation)

Per-clip **Audio Clip Properties dialog**:

- New **"Routes to:"** dropdown (label string EXACTLY `Routes to:`).  Lists
  every Vox / Inst / Clips page, plus 3 entries: `Add a new Clip Page`,
  `Add a new Vox Page`, `Add a new Inst Page`.
- The dialog's **"OK" button is renamed to "Apply"** — same apply-and-close
  behavior; "Cancel" still discards.
- Picking an `Add a new ___ Page` entry: creates the page, routes the clip to
  it, and navigates to the newly created page.

**Two-level routing model** (Jeff's design call — see §42 for the
not-a-scope-expansion ruling):

- The audio LIBRARY entry's `pageOwnerChannelId` is the **source-of-truth**
  route.
- Each grid block gains a new **`routeIsOverride`** flag (`PatternManager`).
- **Per-block Properties** (the per-clip dialog) = override THIS copy only:
  sets `routeIsOverride=true`, detaching that block from the library entry.
- **Browser-entry Properties** (NEW `Properties...` context-menu item on the
  audio tree) = source-of-truth edit: retags the library entry AND propagates
  the new route to every grid copy still following (`routeIsOverride==false`).
- New blocks default to **follow** (`routeIsOverride=false`).
- **Deserialize defaults `routeIsOverride=true`** so pre-Task-7 projects keep
  their exact saved routes on reopen — NO silent reroute of old projects.

### 42. Spec ruling — two-level model is original FILE-02 intent, NOT scope expansion

Jeff confirmed the two-level (library source-of-truth + per-block override)
model was **FILE-02's original intent**, not a scope expansion.  Therefore
**NO §9 Forks scope-expansion entry is created for this** — FILE-02 is being
implemented as specced, not grown.

### 43. Audio-thread safety — zero hot-path code touched

- `PluginProcessor.cpp:3249` (`p.routeChannel = blk.routeChannel`) is
  **unchanged**.  Playback-time route resolution is untouched.
- "Follow" is **edit-time propagation**, not playback-time resolution — the
  follower-propagation walk runs in the UI/edit domain via
  `notifyArrangementChanged`, never on the audio thread.
- The browser-level (source-of-truth) edit is **non-undoable**, matching the
  adjacent **Choke Group** precedent in the same browser context menu.

### 44. Files touched (Task 7 — implementation complete, uncommitted)

- `Source/PatternManager.h` / `Source/PatternManager.cpp` — `routeIsOverride`
  field + serialize / deserialize (deserialize defaults `true` per §41).
- `Source/Standalone/BuilderPage.h` / `Source/Standalone/BuilderPage.cpp`:
  - `RoutablePageInfo` struct relocated above `BrowserPanel`.
  - Per-block **"Routes to:"** combo + **Apply** button on the per-clip
    Properties dialog.
  - Dead duplicate menu **item 7 deleted**; **item 6 renamed `Properties...`**.
  - Browser `showAudioTreeContextMenu` gains a **`Properties...`** item +
    new `showLibraryRoutingDialog`.
  - `BrowserPanel` gains `onEnumerateRoutablePages` /
    `onCreateRoutablePage` / `onApplyLibraryRouting` callbacks.
- `Source/Standalone/StandaloneEditor.cpp` — grid `onEnumerateRoutablePages`
  / `onCreateRoutablePage`; browser reuses them; `onApplyLibraryRouting`
  drives the follower-propagation pass via `notifyArrangementChanged`.

### 45. Side finding + routing decision — dead `BrowserItem::Kind::Audio` flat-list paths (Choke Group)

Surfaced while verifying Jeff's choke-grouping concern; confirmed by code read:

- The `BrowserItem::Kind::Audio` code paths in the **flat-list** browser menu
  are **DEAD post-FILE-01**.  No Audio-kind `BrowserItem` is ever constructed
  (only Pattern at `BuilderPage.cpp:317`, Automation at `BuilderPage.cpp:776`).
- Therefore the orphaned **Choke Group submenu** inside
  `BrowserPanel::showItemContextMenu` (`BuilderPage.cpp:911-920`, handler
  `:935-938`) plus the related `kind == BrowserItem::Kind::Audio`
  switch/rename cases (~`:863`, ~`:993`) are **unreachable duplicates** of the
  LIVE audio choke path in `showAudioTreeContextMenu` (`:429-435`, applied
  `:488`).
- **Audio-clip choke grouping is NOT lost** — the folder-view (tree) menu is
  the live path and the choke value lives on the library entry.
- **Jeff's decision: route the dead-code item to QA-Cleanup-1** (Main Plan §5
  "Source code cleanup", the established dead-code docket).  Slot/placement is
  Jeff-decided, not unilateral (per `feedback_slot_placement_is_spec_call.md`).
- **Cleanup scope note for QA-Cleanup-1:** before deleting the flat-list audio
  call site, verify `renameAudioAt` is not still shared by the tree path.
- Formal §9 Forks routing + the Main Plan §5 QA-Cleanup-1 scope line are
  applied at **QA-E close** (Task 10 side-finding routing step).  This
  running-notes entry is the durable mid-batch capture so the finding is not
  lost to compaction.

### Disposition

- Task 7 FILE-02 implemented as the full two-level routing model per Jeff's
  §41 spec calls.  Implementation complete; **no commit yet** — awaiting
  Jeff's per-task Debug-then-Release verify (covers both per
  `feedback_no_full_release_reverify_at_batch_close.md`).
- Two-level model is original FILE-02 intent per §42 — **no §9 Forks
  scope-expansion entry** for FILE-02 itself.
- Zero audio-thread code touched (§43); browser-level edit non-undoable,
  matching the Choke Group precedent.
- Side finding (§45): dead flat-list `BrowserItem::Kind::Audio` Choke paths
  routed to **QA-Cleanup-1** (Jeff's placement call); formal §9 Forks +
  Main Plan §5 scope line deferred to QA-E close (Task 10), captured here
  durably.

### Next action

- Jeff verifies Task 7 in Debug then Release.
- Fix-in-batch anything surfaced (no-defer rule, `feedback_qa_batches_fix_bugs_dont_defer.md`).
- On clean verify: dispatch `/draft-commit` -> surface drafted commit message
  + full pre-commit git status -> commit on Jeff's explicit approval
  (`feedback_surface_drafted_commit_message_for_approval.md`).
- Carry the §45 dead-code item to **QA-E close** for formal §9 Forks routing +
  the Main Plan §5 QA-Cleanup-1 scope line (Task 10 side-finding routing step).

---

## 2026-05-16 — Task 7 — FILE-02 redesigned to Move/Copy menu-tree model (supersedes §41-§45)

> **Supersession note.**  §41-§45 (2026-05-15 header) described an EARLIER
> INTERIM Task-7 model: one `routeIsOverride` flag + a per-clip "Routes to:"
> combo + a browser `showLibraryRoutingDialog` that was routing-only.  That
> model was iterated through a long design conversation with Jeff and is now
> **REPLACED** by the final two-level Move/Copy menu-tree model below.  Where
> §41-§45 and §46+ conflict, §46+ governs.  Items NOT superseded: the §42
> ruling (two-level model is original FILE-02 intent, no §9 Forks
> scope-expansion entry for FILE-02), the §43 audio-thread-safety stance, and
> the §45 dead-code carry-forward to QA-Cleanup-1 (all re-affirmed below).
> This is the durable compaction-critical capture — the next session works
> from §46+, not conversation memory.

### 46. Design evolution — Jeff's calls, in order (final model locked)

The §41-§45 one-flag / whole-copy + routing-only-combo model evolved through
the design conversation into the FINAL two-level Move/Copy menu-tree model.
Spec calls Jeff locked, in order:

- **Task-5 drop-existing prompt kept** — the existing Task-5 "file already in
  library" prompt stays as-is; not removed by this rework.
- **"One entry per file path"** — no two library entries share an identical
  file path.  Achieved by **Copy producing an auto-numbered DISTINCT physical
  file** (a new path), NOT by collapsing / de-duping identical-path entries.
- **Routing control = menu-tree** — a button that opens a `PopupMenu`.  NOT a
  combo box; NOT a second modal dialog.  (Supersedes §41's combo + §44's
  `showLibraryRoutingDialog` second-modal approach.)
- **Per-clip dialog routing = Copy only** — the acted-on grid block BECOMES
  the renamed copy, carrying ALL its box props.  No Move from the per-clip
  path.
- **Browser dialog routing = per-target submenu {Move, Copy}**:
  - **Move** = relocate the SINGLE library entry (owner + props); propagate to
    every following block.  No new file.
  - **Copy** = force an auto-numbered physical duplicate + a NEW library entry
    on the target; the original entry + file are left untouched.
- **Option (B) kept** — retain the per-block follow/detached flag
  (`routeIsOverride` renamed to `isOverride` and broadened from route-only to
  all source-of-truth props).
- **Symmetric note + detached marker** mitigations:
  - Per-clip box note, text EXACTLY: `These settings apply to this clip only.
    Changing them stops this clip from following the file's master settings.`
  - A detached-block marker (dot) on the grid for any `isOverride==true` block.
- **Auto-rename = `ProjectManager` automatic " (N)" numbering** — reuse what
  exists (Jeff's call); no new naming scheme.

**Re-affirms §42:** Jeff confirmed this two-level model is FILE-02's original
intent, NOT a scope expansion → **no §9 Forks scope-expansion entry for
FILE-02 itself**.

### 47. New bug folded in — browser rename didn't update connected grid block (no-defer)

- **Symptom:** renaming an audio asset in the browser didn't update the name
  on the connected grid block.
- **Root cause:** `renameAudioAt` correctly stamps `b.displayAlias`, but the
  grid audio-block label always re-derived from the file path
  (`BuilderPage.cpp` audio-block paint) and never read `displayAlias`.
- **Fix:** grid audio-block paint now prefers `displayAlias` (mirrors the
  existing `StandaloneEditor.cpp:10242` pattern).  `displayAlias` is already
  serialized — read-side-only fix.

### 48. Files changed this rework (implementation complete, UNCOMMITTED)

Awaiting Jeff's per-task Debug-then-Release verify.

- `Source/ProjectManager.h/.cpp` — new `duplicateSample()`: ALWAYS forces a
  fresh " (N)" auto-numbered copy (unlike `importSample`, which early-returns
  when the asset already exists — that early-return is why a forced duplicate
  wasn't possible before).
- `Source/PatternManager.h/.cpp` — (earlier pass, RETAINED)
  `ArrangementBlock::isOverride` + `AudioLibraryEntry` source-of-truth
  pitch/BPM/stretch + accessors + serialize; deserialize defaults
  `isOverride=true` (pre-Task-7 projects keep exact saved props/routes).
- `Source/Standalone/BuilderPage.h` — `PendingRoute` (in .cpp); menu-tree
  `buildAudioPropsControls` fwd decl; `onCopyFileForRoute` on ArrangementGrid
  + BrowserPanel; `onApplyLibraryProperties`; `showLibraryPropertiesDialog`.
- `Source/Standalone/BuilderPage.cpp` — menu-tree `buildAudioPropsControls`
  (button→PopupMenu, `offerMove` param, shared_ptr out-params for modal
  lifetime, `AlertWindow::addCustomComponent`); `showAudioClipProperties`
  (note; Copy-on-route block-becomes-copy; prop-only = block+isOverride);
  `showLibraryPropertiesDialog` (Move vs Copy); rename-bug fix; detached
  marker dot.
- `Source/Standalone/StandaloneEditor.cpp` — `onCopyFileForRoute` wired (grid
  lambda: `duplicateSample`→`addAudioToLibrary` tagged→`setAudioLibraryClip
  Defaults`→`notifyArrangementChanged`; browser reuses grid's);
  `onApplyLibraryProperties` propagates all 4 to `!isOverride` followers;
  Task-5 "New Page" path now `duplicateSample`s (kills last identical-path
  dupe source).

### 49. Architecture / safety notes

- **Zero audio-thread code touched** — `PluginProcessor.cpp:3249` unchanged;
  follower propagation is edit-time via `notifyArrangementChanged` (re-affirms
  §43).
- **Browser-level edit non-undoable** — matches the adjacent Choke Group
  precedent (re-affirms §43).
- **AlertWindow custom-component lifetime** — handled via `shared_ptr`
  captured in the modal lambda; the built controls outlive the modal.

### Disposition

- Task 7 FILE-02 **redesigned** from the §41-§45 interim model to the final
  two-level Move/Copy menu-tree model per Jeff's §46 calls; §41-§45
  superseded.
- One new bug folded in-batch, no-defer (§47).
- Implementation **complete; UNCOMMITTED** — awaiting Jeff Debug-then-Release.
- No §9 Forks scope-expansion entry for FILE-02 (§46/§42).
- §45 dead-code carry-forward UNCHANGED — flat-list `BrowserItem::Kind::Audio`
  → QA-Cleanup-1, formal §9 Forks + Main Plan §5 line still at QA-E close.

### Next action

- Jeff verifies redesigned Task 7 in Debug then Release.
- Fix-in-batch anything surfaced (`feedback_qa_batches_fix_bugs_dont_defer.md`).
- On clean verify: `/draft-commit` → surface message + full git status →
  commit on Jeff's explicit approval.
- Carry §45 dead-code to QA-E close (Task 10).

---

## 2026-05-17 — Task 7 FILE-02 verify-PASSED + late root-cause fixes + QA-Eb decision

> **Continuation note.**  This block EXTENDS §46-§49 (the 2026-05-16 header,
> which described the Move/Copy menu-tree implementation BEFORE verify and
> before two late root-cause fixes).  It does NOT supersede §46-§49 — the
> final model is unchanged; this records the PASSED verify, the two
> root-cause fixes found during it, post-§48 refinements, and the QA-Eb
> slotting decision.  Closes **Task 7 only** — QA-E is NOT closing; Tasks 8
> (Sub-Phase Z / QA-D NIT corrections), 9 (dirty-flag investigation),
> 10 (close) remain.  Compaction-critical — the next session works from
> §46+ plus this block.

### 50. Verify outcome — Task 7 FILE-02 PASSED (Debug + Release)

Jeff ran the full Task 7 verify on his per-task cycle (Debug then Release,
covers both per `feedback_no_full_release_reverify_at_batch_close.md`).
Verdict: **"This all passes"** (2026-05-17).  Task 7 (FILE-02 — Properties
consolidation + Routing + two-level Move/Copy menu-tree model) is
**COMPLETE and verified**.

- This closes **Task 7 only**.  **QA-E does NOT close here** — Tasks 8
  (Sub-Phase Z / QA-D NIT corrections), 9 (dirty-flag investigation),
  10 (close) still remain.  QA-E resumes at Task 8 after this commit.

### 51. Late root-cause fixes found during verify (no-defer, fixed in-batch)

Per `feedback_qa_batches_fix_bugs_dont_defer.md` — both surfaced during the
verify pass and fixed in-batch, not deferred.

**(a) Copy double-entry bug — "Copy to a new Clip Page" produced TWO
library entries.**

- **Root cause:** the Clips-page spawn (`spawnClipsTabIfMissing`) resolves
  the path to ABSOLUTE, and `ClipsPage::setClipFilePath` registered the
  library entry with that ABSOLUTE path.  Everything else — blocks, browser
  walk, `PluginProcessor`, the Copy tag — uses the STORED / RELATIVE path.
  `addAudioToLibrary` dedups by exact string match, so absolute != relative
  → two distinct entries for one physical file.
- **Fix (1) — Copy plumbing split:** the earlier single `onCopyFileForRoute`
  is replaced by `onDuplicateFileForCopy` (physical auto-numbered copy
  ONLY) + `onTagCopiedEntry` (dedup-safe register).  Copy now duplicates
  FIRST, then creates the page bound to the duplicate.
- **Fix (2) — ROOT-CAUSE fix:** `ClipsPage::setClipFilePath` gained a
  defaulted `libraryPath` param.  The engine still loads from the resolved
  ABSOLUTE path, but the library tag now uses the STORED / RELATIVE path;
  `spawnClipsTabIfMissing` passes the original relative path.  The other 2
  `setClipFilePath` callers use the default (no regression).  Side benefit:
  the latent normal-Clips-drop pairing is now dedup-able too.

**(b) Task-5 drop-existing prompt never fired + same-name multi-entry.**

- **Root cause:** `filesDropped` detected "already in library" by exact
  resolved-path equality, but copy-on-drop relocates imports into
  `Samples/` — so the external drag path never equals the stored
  `Samples/` path → the Use Existing / New Page / Cancel prompt was skipped,
  and every re-drop silently re-imported (new page + duplicate same-name
  entry).
- **Fix (Jeff's call (a), 2026-05-17):** match by FILENAME
  (case-insensitive, Windows) as well as exact path; the existing Use
  Existing / New Page / Cancel prompt is the disambiguation.  Fixes both
  symptoms (prompt now fires; no silent duplicate).

### 52. Other Task 7 refinements landed (recap, post-§48)

Refinements applied between §48 and the passed verify:

- **Per-clip dialog:** gained a symmetric note + a new **"Reset to Browser
  Entry"** button (re-attach: snap to library master, clear override).
- **`isOverride` is now DERIVED on Apply** — matches-master auto-unflags
  (green) and resumes following; no longer a one-way latch.
- **Freshly-Copied block = green** (follows its own new entry).
- **Detached marker dot** moved bottom-LEFT, always-shown:
  **green = following / red = customized**.
- **Pre-Task-7 projects load all-red** (accepted back-compat, Jeff's call).
- **Rename-bug fix (§47)** + Task-5 New-Page → `duplicateSample` (§48)
  verified working.

### 53. QA-Eb decision — window resizability becomes its own batch (Jeff, 2026-05-17)

- Window resizability (the **NAV-01** area) becomes its **OWN new batch
  QA-Eb**, slotted **adjacent to QA-E**.
- **Rationale (Jeff):** doing it next to QA-E vastly speeds up HIS testing
  (no window-juggling during verify).  The two don't naturally group, but
  the testing-efficiency win justifies adjacency.
- Slot / placement is **Jeff's confirmed call**
  (`feedback_slot_placement_is_spec_call.md`) — not unilateral.
- **Scope sketch:** resizable window + min-size clamp + Viewport scrollbars
  when smaller than design size (NOT a full per-page proportional
  relayout); pages with their own scrollbars (Piano Roll, Builder grid)
  must not double-wrap.
- **Bookkeeping (pending, immediately after this commit):** Main Plan §5
  QA-Eb entry + §6 sequencing arrow + §9 Forks entry.  Jeff: "we will at
  that point update the main plan to add QA-Eb."

### 54. Files changed since §48 (uncommitted, about to commit)

- `Source/ProjectManager.h` / `Source/ProjectManager.cpp` — `duplicateSample`
  (from §48, RETAINED).
- `Source/Clips/ClipsPage.h` / `Source/Clips/ClipsPage.cpp` —
  `setClipFilePath` `libraryPath` param (NEW root-cause fix, §51(a)).
- `Source/PatternManager.h` / `Source/PatternManager.cpp` —
  source-of-truth / override plumbing (continued).
- `Source/Standalone/BuilderPage.h` / `Source/Standalone/BuilderPage.cpp` —
  menu-tree, "Reset to Browser Entry" button, derived `isOverride`, marker,
  filename-match detection, `onDuplicateFileForCopy` / `onTagCopiedEntry`.
- `Source/Standalone/StandaloneEditor.cpp` — Copy wiring split,
  `setClipFilePath` call (passes relative path), Task-5 New-Page
  `duplicateSample`.

### Disposition

- Task 7 FILE-02 **COMPLETE and verified** (§50 — Jeff "This all passes",
  Debug + Release, 2026-05-17).
- **QA-E does NOT close** — Tasks 8 / 9 / 10 remain; QA-E resumes at Task 8
  after this commit.
- Two late root-cause fixes found during verify, fixed in-batch, no-defer
  (§51 — Copy double-entry + Task-5 drop-existing prompt).
- §46-§49 model UNCHANGED — this block extends, does not supersede.
- No §9 Forks scope-expansion entry for FILE-02 (re-affirms §42 / §46).
- QA-Eb slotted adjacent to QA-E (§53, Jeff's placement call); Main Plan
  §5 / §6 / §9 bookkeeping pending immediately after this commit.
- §45 dead-code carry-forward UNCHANGED — flat-list
  `BrowserItem::Kind::Audio` → QA-Cleanup-1; formal §9 Forks + Main Plan
  §5 line STILL at QA-E close (Task 10).

### Next action

- Dispatch `/draft-commit` → surface drafted commit message + full
  pre-commit git status → commit on Jeff's explicit approval
  (`feedback_surface_drafted_commit_message_for_approval.md`), staging
  specific source files only (~1092-line diff across 10 files).
- Then apply Main Plan **QA-Eb** bookkeeping — §5 entry + §6 arrow + §9
  Forks — via `/draft-doc`, surfaced for review before apply (§53).
- §45 dead-code → QA-Cleanup-1 routing STILL deferred to QA-E close
  (Task 10), unchanged.
- QA-E resumes at **Task 8** (Sub-Phase Z / QA-D NIT corrections) after
  this commit + QA-Eb bookkeeping.
