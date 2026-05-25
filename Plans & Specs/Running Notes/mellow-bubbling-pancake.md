# Running Notes — QA-AudioMeters (mellow-bubbling-pancake)

> Append-only mid-batch log. A new `## YYYY-MM-DD — Task N — <name>` entry is
> appended at every checkpoint (commit landed / sub-task verified / finding
> captured / spec call resolved / scope pivot) per
> `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close,
> `/draft-doc batch-close` consumes this file as the primary input for the
> single Implemented Work Log entry. Never edited retroactively.

**Pair:** `Plans & Specs/Batch Plans/mellow-bubbling-pancake.md` (the plan).
**Conventions:** Main Plan §0 — Document Formatting Conventions + running-notes
required sections (locked 2026-05-11) + Rule 4 (Diagnostic Instrumentation Catalog).

---

## 2026-05-24 — Task 0 — Batch open (docs)

- **Scope.** QA-AudioMeters applies the QA-Eg G1 migration pattern to the per-row
  Builder audio meter surface — the last remaining instance of the centralized
  `PluginProcessor::*Run` mirror pattern. Migrates the 50-row audio strip surface
  off the `PluginProcessor::mAudioRowPeakDb*Run` intermediate mirrors onto
  node-owned atomics + `VibeGraph::audioRowPeakDb*` public-member arrays + a
  unified `drainMeterAtomicsForUI` G1 drain loop. Supersedes the `*Run`
  indirection that QA-Eg's Task 8 NEEDS-FIX-2 sweep already labelled "deferred
  to a separate batch per S2".
- **Pre-batch.** `/standup` (QA-Eg closed `888a01b`; tree clean of source apart
  from untracked `Templates/My Templates/` from prior batches; branch ahead of
  origin by the QA-Eg cluster commits, no push expected this batch). Full direct
  self-read of Main Plan §0 (lines 1-500 covering Rules 1-4 + Document Formatting
  Conventions incl. the locked Batch Plans + Running Notes required-sections rule
  + the federated-bouncing-cupcake exemplar + canonical buckets + Agent
  Orchestration Rules). `/read-doc`-style targeted reads: §5 QA-AudioMeters
  entry (`Main Plan.md:1163-1178`), §6 sequencing arrow + footnotes
  (`:1830-1840`), §9 thirty-first Forks entry (`:4312-4351`). CLAUDE.md status
  claims spot-checked vs Work Log: QA-Eg shown closed in §5 STATUS banner; commit
  ladder matches Work Log entries through 2026-05-24 18:00 PT.
- **Pre-plan source reads** (per `feedback_check_code_before_calling_it_expected.md`):
  - Per-row mirror surface ([PluginProcessor.h:620-629](Source/PluginProcessor.h:620)
    — snapshot + `*Run` arrays; [PluginProcessor.cpp:155-163](Source/PluginProcessor.cpp:155)
    — initialiser loop; [:2107-2112](Source/PluginProcessor.cpp:2107) — G3 drain).
  - 8 publishing sites confirmed via grep:
    [CompositeAudioInsertTask.cpp:113-115](Source/Engine/Tasks/CompositeAudioInsertTask.cpp:113)
    + [PluginProcessor.cpp:585-587](Source/PluginProcessor.cpp:585) (the two
    CAS-max sites) + `:415` / `:420` / `:448` / `:642` / `:647` / `:668` (the
    six force-reset stores).
  - InsertNode primitives confirmed at [VibeGraph.cpp:1241](Source/VibeGraph.cpp:1241)
    — `peakDb.store(juce::jmax(newL, newR), ...)` simple-store under L7 / Sub-B
    Option B requires CAS-max upgrade.
  - VibeGraph public-member atomics pattern at [VibeGraph.h:634-672](Source/VibeGraph.h:634).
  - QA-Eg post-Task-8 drain loop at [PluginProcessor.cpp:2096-2108](Source/PluginProcessor.cpp:2096)
    (the G1 loop covering 13 buses; QA-AudioMeters extends with a per-row loop).
  - QA-Eg plan + running notes (`squishy-scribbling-flurry.md`) re-read as the
    precedent for task structure + Task 1 inventory shape + per-bus migration
    pattern.
- **Spec calls resolved with Jeff (2026-05-24 at ExitPlanMode):**
  - **L6 / Sub-A = 5-task structure** (Task 0 open / Task 1 read-only pre-flight
    inventory / Task 2 per-row migration bundle / Task 3 cleanup + sweep / Task
    4 close). Matches QA-Eg's Task-1 inventory precedent.
  - **L7 / Sub-B = Option B** — single exchange-store at end of
    `CompositeAudioInsertTask::run`; both flows publish into InsertNode peakDb
    via `processInsert`; per-flow drains at CompositeAudioInsertTask:113-115 +
    PluginProcessor:585-587 are REMOVED; InsertNode publish site at
    `VibeGraph.cpp:1241` gets a one-line CAS-max upgrade so consecutive publishes
    accumulate. Cleaner architectural match to the bus migration; ends QA-Eg's
    bus-vs-row architectural inconsistency.
  - **L8 / Sub-C = B2** — DELETE all 6 force-reset stores. Per-row meters decay
    naturally over ~20ms (DBFSMeter ballistic) on mute / choke / file-end — same
    visible behavior as every bus meter. Jeff confirmed the 1-block-snap →
    20ms-decay change is desirable (aligns to bus behavior).
- **Plan file** seeded as `mellow-bubbling-pancake` (plan-mode runtime). Plan
  file mirrored from `~/.claude/plans/` to `Plans & Specs/Batch Plans/` and the
  home-dir copy deleted per `feedback_plan_mirror_one_way.md`. §5 entry's
  `**Plan file:**` line flipped to the canonical-path form. Running notes seed
  (this entry) is the §0-conformant header + Task 0 entry.
- **Next action.** Surface full git status + dispatch `/draft-commit` for the
  Task 0 open commit (plan file + running-notes seed + §5 pointer flip).

---

## 2026-05-24 — Task 0 — Commit landed (14400fb)

- **Commit.** `14400fb` — QA-AudioMeters Task 0 open. 3 files: `Plans & Specs/Main Plan.md` (§5 QA-AudioMeters entry's `**Plan file:**` pointer flipped from `<silly-name>.md (when started)` placeholder to the canonical `Plans & Specs/Batch Plans/mellow-bubbling-pancake.md`), `Plans & Specs/Batch Plans/mellow-bubbling-pancake.md` (NEW — 5-task plan per L6/L7/L8), `Plans & Specs/Running Notes/mellow-bubbling-pancake.md` (NEW — §0-conformant seed + Task 0 entry above).
- **Drafted via `/draft-commit`** per `feedback_every_commit_via_draft_commit.md`. Surfaced to Jeff verbatim per `feedback_drafter_output_verbatim_no_restyle.md`; Jeff requested two factual edits before approving:
  - Dropped an unsupported "Diagnostic Instrumentation Catalog stub" claim from the body (the seed running-notes file does not include a DIC stub — Rule 4 catalogs are populated as instrumentation lands, not at batch open).
  - Rephrased the 6-force-reset-deletion summary from "now cause visual pops" framing to the alignment-to-bus framing Jeff prefers (DBFSMeter ~20ms ballistic decay matches every bus meter; per L8 spec call this IS the desired behavior, not a side-effect to caveat).
- **Working tree.** `Templates/My Templates/` left untracked per prior-batches' convention (out of scope, not staged). No source changes this commit. Branch now 12 commits ahead of `origin/main` — no push expected this batch (matches QA-Eg + QA-Md + QA-A precedent).
- **Next action.** Begin Task 1 (read-only pre-flight inventory). Confirm safety of the InsertNode `VibeGraph.cpp:1241` simple-store -> CAS-max upgrade (check whether other call sites in the same `processInsert` block depend on the simple-store semantic); check whether a `VibeGraph::getInsertNode(InsertKind, int)` accessor already exists (needed for the per-row drain loop) or whether one must be added; verify `drainAndMerge` semantic on the existing per-bus G1 drain pattern transfers cleanly to the 50-row array. Surface inventory findings to Jeff before any Task 2 source-touching work.

---

## 2026-05-24 — Task 1 — Pre-flight inventory + L7 pivot to Option 2 (re-spec)

- **Method.** Read-only Task 1 per the plan — direct reads of all 7 inventory targets from the plan against current post-QA-Eg source (no source edits, no commit). Findings below DO NOT match the plan's L7 / Sub-B Option B assumptions — surfaced to Jeff, re-scoped to Option 2 restructure.

#### Plan assumption that DOES NOT hold

- **InsertNode publish is NOT a simple-store.** [VibeGraph.cpp:1231-1242](Source/VibeGraph.cpp:1231) does `load -> apply per-block decay -> max-merge against juce::jmax(newL, newR) -> store`, not the plan's assumed `peakDb.store(juce::jmax(newL, newR))` simple-store. The one-line CAS-max upgrade L7 / Sub-B Option B prescribes won't compose — the existing decay + load + max chain is the InsertNode equivalent of the bus `publishPeakReading` helper, just open-coded.
- **`peakDbSnap` snapshot-promotion layer sits between `peakDb` and the UI consumer.** [VibeGraph.cpp:2437-2459 drainInsertPeakDbStereo](Source/VibeGraph.cpp:2437) reads `peakDbSnap` (NOT `peakDb`); [`:2470+ promoteAllInsertPeakSnapshots`](Source/VibeGraph.cpp:2470) does the `peakDb -> peakDbSnap` exchange. This is an extra mirror layer the per-bus G1 pattern does NOT have.
- **`peakDbSnap` has TWO consumers, not one.** `drainInsertPeakDbStereo` serves BOTH the per-row Builder audio meter surface AND the per-insert Mixer-strip surface (the `getInsertPeakDbStereoExchange` name in [publishPeakReading's comment at VibeGraph.cpp:115](Source/VibeGraph.cpp:115) is a stale alias for the same function). Option B's single exchange-store would corrupt one or both reads on consecutive polls.

#### L7 pivot — Option 2 restructure (Jeff, 2026-05-24)

- **Decision (verbatim, two phrases bracketing the direction):** "Let's embrace the scope" + "Let's do the surgery and get the architecture right" — rewrite InsertNode publish to use the bus-pattern `publishPeakReading` helper, remove the `peakDbSnap` intermediate layer entirely, all 8 InsertKinds (Layer / Bass / Drum / Audio / Aux / Vox / Inst / Rusty) adopt the standard, restructure per-insert Mixer-strip + per-row Builder-grid consumers to read from new PluginProcessor parallel mirrors.
- **L9-new — PluginProcessor parallel mirrors as the UI poll target.** 8 sets of `m<Kind>InsertPeakDb*` x 3 axes (mono/L/R), one set per InsertKind. `mAudioRowPeakDb*` name preserved for Builder backward compat (Builder consumer doesn't care about the rename underneath).

#### L6 re-spec to 10-task structure

- **Task 0 open (DONE 14400fb) / Task 1 read-only inventory (THIS TASK) / Task 2 structural one-shot (InsertNode + publish helper + `peakDbSnap` removal + drain helper + 8 mirror sets) / Tasks 3-7 per-kind verify (Layer / Bass / Drum / Audio / Aux+Vox+Inst+Rusty bundle) / Task 8 cleanup + sweep / Task 9 close.** Mirrors QA-Eg's per-bus rhythm. L8 / Sub-C B2 (6 force-reset stores deleted) carried forward unchanged.

#### Minor findings folded into Task 8 cleanup

- **`peakDecayDbPerBlock` on InsertNode is LIVE** — used at [VibeGraph.cpp:1235-1236](Source/VibeGraph.cpp:1235), NOT dead like QA-Eg's S6 BusNode case. Under L7-revised it gets deleted because `publishPeakReading` replaces the decay-store machinery entirely.
- **Stale inline comment** at [PluginProcessor.cpp:2059-2066](Source/PluginProcessor.cpp:2059) — `"Group 1: bus mirrors (Layers/Bass/Drums/Master)"` — actually 13 buses post-QA-Eg.
- **Stale `getInsertPeakDbStereoExchange` name** in [publishPeakReading's comment at VibeGraph.cpp:115](Source/VibeGraph.cpp:115) — function was renamed to `drainInsertPeakDbStereo`, comment never updated.
- **`drainAndMerge` semantic confirmed** at [PluginProcessor.cpp:2059-2066](Source/PluginProcessor.cpp:2059) — CAS-max promote with `-inf` sentinel; matches expected pattern for the new G1 drain loop the per-row + per-insert mirrors will hang off.

#### Effort estimate revised

- **~12-18 hours** (was ~3-5 hours per §5 entry). Scope-up acknowledged at re-spec. Per `feedback_qa_batches_fix_bugs_dont_defer.md` the right architectural foundation is in-scope, not a "maybe future" suggestion.

#### Plan file edits (targeted per `feedback_targeted_edits_not_wholesale_rewrite.md`)

- **Context risk + effort bullet** updated with the revised hours + scope-up note.
- **Spec calls table** — L6 / L7 / L8 rows revised + new L9 row added (PluginProcessor parallel mirrors).
- **Sub-spec section** — post-exit re-spec note appended explaining the Option 2 pivot.
- **Files to modify section** — Task 2 rewritten as structural one-shot + Tasks 3-7 added (per-kind verify) + old Tasks 3/4 renumbered to 8/9.
- **Tasks section** — same restructure with per-task checklists.
- **Verification, MT-awareness static-analysis, Routing notes** — all updated to reflect the 10-task shape.

#### Source / commit / build

- None this task — Task 1 is read-only by design. No diagnostic instrumentation added.

#### Next action

- Surface a Task 1 docs commit (plan-file revisions per the edit map above + THIS running-notes entry) for Jeff's approval via `/draft-commit`. Once landed, proceed to Task 2 structural one-shot.

---

## 2026-05-24 — Task 2 — Structural one-shot landed (0fd9b91)

- **Commit.** `0fd9b91` — QA-AudioMeters Task 2 structural one-shot. 6 source files, +362 / -283. Drafted via `/draft-commit` per `feedback_every_commit_via_draft_commit.md`; surfaced verbatim for Jeff's approval per `feedback_drafter_output_verbatim_no_restyle.md` + full pre-commit git status surfaced per `feedback_surface_full_git_status_before_commit.md`. Release + Debug both clean, 0 new warnings — Jeff confirmed.

#### Source-file summary (6 files, per the Task 2 plan)

- **[Source/VibeGraph.h](Source/VibeGraph.h)** — added 24 per-kind public-member `std::array<std::atomic<float>, N>` arrays (8 InsertKinds x 3 axes mono/L/R) + `kMaxAudioInserts` constant; deleted 3 legacy accessor decls (`drainInsertPeakDbStereo` + `getInsertPeakDbStereo` + `getInsertPeakDb`); renamed `promoteAllInsertPeakSnapshots` -> `promoteAllRackSlotSnapshots` (the rack-promotion half is what survives; the insert-peak-snapshot half is folded into the new G1 drain).
- **[Source/VibeGraph.cpp](Source/VibeGraph.cpp)** — deleted `InsertNode::peakDbSnap` / `peakDbLSnap` / `peakDbRSnap` fields + the `peakDecayDbPerBlock` field + the per-block decay machinery from `InsertNode::process`; replaced the open-coded inline publish (load-decay-max-store at the old `:1231-1242` site) with a single `publishPeakReading(...)` helper call (matches the bus pattern); added per-kind exchange-stores in `processInsert` so each of the 8 InsertKinds writes into the new VibeGraph public-member arrays; split + renamed `promoteAllInsertPeakSnapshots` (rack-promotion half kept, insert-peak-snapshot half removed); deleted 3 legacy accessor bodies; updated `publishPeakReading`'s documentation comment to reference the new flow; added `prepare()` init loops for the 24 new arrays (all initialized to `-60.f` matching the existing bus atomic in-class init pattern).
- **[Source/PluginProcessor.h](Source/PluginProcessor.h)** — added 21 per-kind mirror array decls (`mLayerInsertPeakDb*` / `mBassInsertPeakDb*` / `mDrumInsertPeakDb*` / `mAuxInsertPeakDb*` / `mVoxInsertPeakDb*` / `mInstInsertPeakDb*` / `mRustyInsertPeakDb*` x 3 axes; Audio reuses the existing `mAudioRowPeakDb*` arrays per the L9 backward-compat naming); deleted the 3 `mAudioRowPeakDb*Run` mirror decls; added the new `drainInsertPeakDbStereo(InsertKind, int)` accessor decl.
- **[Source/PluginProcessor.cpp](Source/PluginProcessor.cpp)** — rewrote the `:155-163` initialiser loop to seed all 8 sets of per-kind mirrors with `-60.0f` (UI poll target floor); added a `static_assert` that `VibeGraph::kMaxAudioInserts == kMaxAudioRows` (since VibeGraph.cpp doesn't include PluginProcessor.h — avoids circular include); added the ~55-line `drainInsertPeakDbStereo` accessor body (per-kind switch + bounds-check + exchange-reset of the matching `m<Kind>InsertPeakDb*L/R[index]` mirror pair, sentinel `-inf`); deleted the old `arCasMax` lambda + the per-flow drain block at `:583-587` of `renderAudioClipsForRow`; deleted all 6 force-reset stores per L8 / Sub-C B2 (Audio-row meters now decay over ~20ms via DBFSMeter ballistic, aligning per-row to per-bus visible behavior); rewrote `drainMeterAtomicsForUI`'s G3 per-row loop into 8 per-kind drainAndMerge loops; renamed the `promoteAll*` call to match `VibeGraph.h`.
- **[Source/Engine/Tasks/CompositeAudioInsertTask.cpp](Source/Engine/Tasks/CompositeAudioInsertTask.cpp)** — deleted the ~17-line Flow A drain block at `:100-116`; replaced with an explanatory comment that points at the new `processInsert` publish path + the G1 drain in `PluginProcessor::drainMeterAtomicsForUI` (so a future reader doesn't re-add the per-flow drain).
- **[Source/Standalone/MixerPage.cpp](Source/Standalone/MixerPage.cpp)** — rewired the `drainStereoInsert` lambda at `:3278-3283` to call `mProcessor.drainInsertPeakDbStereo` (dropped the `mVibeGraph.` middle — accessor moved off VibeGraph onto PluginProcessor) and unified the Audio-row drain with the other 7 kinds (single call shape, no per-kind branching at the consumer side).

#### Build + grep cleanliness

- **Release + Debug both clean, 0 new warnings.** Per `feedback_no_full_release_reverify_at_batch_close.md` no separate Release re-verify gate — Jeff's per-task verify cycle covers both configs.
- **Post-edit grep sweep clean.** Zero remaining source references to `mAudioRowPeakDb*Run` / `peakDbSnap` / `peakDbLSnap` / `peakDbRSnap` / `mVibeGraph.drainInsertPeakDbStereo` (old signature) / `mVibeGraph.getInsertPeakDb*` — all remaining hits are inside updated documentation comments.

#### Heredoc-with-apostrophes commit-mechanics gotcha (recovery noted)

- Initial `git commit -m "$(cat <<'EOF' ... EOF)"` failed bash parse on Windows for the multi-paragraph drafted message containing embedded apostrophes; the heredoc closed early at one of the inner quote runs. Recovered by writing the drafted message to a temp file `.commit-msg.tmp` -> `git commit -F .commit-msg.tmp` -> delete temp file. No source impact, no message content change — pure commit-mechanics workaround. Surface noted here so the pattern is captured for future multi-paragraph drafted-commit messages on Windows bash.

#### Task 8 cleanup carry-forward

- **Stale `promoteAllInsertPeakSnapshots` reference** at [EffectRack.h:230](Source/EffectRack.h:230) — comment-only mention of the old function name (function itself was renamed to `promoteAllRackSlotSnapshots` in Task 2). No compile impact; folded into Task 8 cleanup sweep alongside any other stale-comment hits discovered during Tasks 3-7 verify passes.

#### Diagnostic Instrumentation Catalog (Rule 4)

- **Nil entry.** No new instrumentation added this task — all changes are structural code edits (field deletions / helper additions / per-kind array wiring / drain-loop restructure). No `DBG`, no `juce::Logger::writeToLog`, no temp `jassert`, no diagnostic `AlertWindow`. Catalog remains empty post-Task-2.

#### Next action

- Jeff runs **Task 3** — Layer kind end-to-end runtime verify per the 5-scenario script from the plan: (1) Debug new project, (2) Layer 1 with engine picked, (3) audition note -> Mixer per-strip Layer meter reads the post-rack peak, (4) meter decays smoothly on note-off matching bus DBFSMeter ballistic + MT-on / MT-off parity intact, (5) save -> reload -> re-audition still reads correctly. On pass: dispatch `/draft-commit` for the Task 3 verify checkpoint commit (docs-only — this running-notes file appends a "verified clean" Task 3 entry, no source edits) then transition to **Task 4** (Bass kind verify, same 5-scenario shape against the Bass insert tree).

---

## 2026-05-24 — Task 3 — Stress-file verify PASS + L6 re-collapse + Future State routing

- **Verify rhythm pivot (Jeff, mid-Task-3 setup).** The per-kind verify workflow locked into the L7-pivot's 10-task structure required constant tab-switching between piano-roll auditioning + Mixer-page meter watching per InsertKind (Layer / Bass / Drum / Audio / Aux+Vox+Inst+Rusty). Jeff observed that this broke the rhythm — his preference is to use his existing big stress-file test arrangement that already exercises all 8 InsertKinds + the 13 G1 buses in parallel in one verify session. Resolution: collapse Tasks 3-7 (per-kind verify) into a single Task 3 stress-file verify covering all 8 kinds at once. Mirrors how every prior batch verified at the end of structural one-shots rather than per-component.

#### Watchlist correction — fabricated Builder-grid agreement point

- **My fabricated claim caught.** My initial Task 3 verify watchlist included "Builder grid per-row meters (Audio kind's other consumer) — should agree with Mixer-page Audio strip meters." Jeff caught this — there is **NO DBFS strip on Builder tracks today.** Post-call grep verified: `mAudioRowPeakDb*` is consumed ONLY by [MixerPage.cpp](Source/Standalone/MixerPage.cpp) (the Audio insert per-strip meter); zero references in [BuilderPage.cpp](Source/Standalone/BuilderPage.cpp) or anywhere else under `Source/Standalone/`. The §5 / §9 "per-row Builder audio meters" naming refers to per-row STORAGE indexed by Builder row number, not a Builder-grid display widget. I apologized + acknowledged the error in chat. Point dropped from the watchlist + from the plan-file Verification section.

#### L6 re-collapse to 6-task structure

- **Decision (Jeff, 2026-05-24).** Tasks 3-7 from the L7-pivot's 10-task structure collapsed into a single **Task 3 all-kinds stress-file verify.** Old Tasks 8 / 9 renumbered to **Task 4 (cleanup + sweep)** + **Task 5 (close).** Final structure: Task 0 open / Task 1 inventory / Task 2 structural / Task 3 stress-file verify / Task 4 cleanup / Task 5 close.

#### Task 3 stress-file verify PASS

- **Result — Jeff confirmed "these all pass" running his stress-test arrangement.** 6 corrected watchlist points (Builder-grid agreement removed):
  1. **Per-strip meters on Mixer page (8 InsertKinds).** Layer / Bass / Drum / Audio insert / Aux / Vox / Inst / Rusty — all read activity matching source, decay smoothly.
  2. **Bus regression (13 G1 buses from QA-Eg).** All still read correctly post-structural rewrite.
  3. **Mute decay (L8 / B2 alignment).** ~20ms ballistic on any strip mute, matches bus mute behavior.
  4. **MT-on / MT-off parity.** Identical behavior both modes.
  5. **Save + reload.** Meters still work post-reload.
  6. **EffectRack slot meter spot-check.** The surviving `promoteAllRackSlotSnapshots` path still updates slot meters correctly.

#### Future State routing — [CL-293 / WP] Builder-grid per-row DBFS meter

- **Routed to Future State, NOT in-scope for QA-AudioMeters.** Backing storage already plumbed by this batch (Audio-row mirrors live in PluginProcessor + drain through the unified G1 path); remaining work is a Builder-side widget add. New `### Batch-surfaced (QA-AudioMeters 2026-05-24)` sub-cluster under `## System Pages` in [Future State.md](Future State.md), entry `**[CL-293 / WP]** Builder-grid per-row DBFS meter`, priority MEDIUM.

#### Plan-file edits (targeted per `feedback_targeted_edits_not_wholesale_rewrite.md`)

- **Spec calls table.** L6 row re-collapsed to the 6-task shape.
- **Sub-spec calls section.** Post-Task-2 re-spec note added documenting the verify-rhythm pivot + Builder-grid fabrication correction.
- **Files to modify section.** Task 3 collapsed to a single sub-section (was 5 per-kind sub-sections); Task 4 / Task 5 renumbered (was Task 8 / Task 9).
- **Tasks section.** Same collapse + renumber with the single-Task-3 stress-file verify shape.
- **Verification section.** Dropped the Builder-grid agreement claim; renumbered references to Task 4 / Task 5; added a pointer to Future State CL-293.

#### Diagnostic Instrumentation Catalog (Rule 4)

- **Nil entry.** Task 3 is verify-only — no instrumentation added. Catalog remains empty post-Task-3.

#### Next action

- Surface a Task 3 docs commit (Task 2 running-notes entry + plan-file L6 collapse + Verification correction + Future State CL-293 add + THIS Task 3 running-notes entry) for Jeff's approval via `/draft-commit`. Once landed, transition to **Task 4 (cleanup + comment sweep + grep cleanliness)** — carry-forward items include the stale `promoteAllInsertPeakSnapshots` reference at [EffectRack.h:230](Source/EffectRack.h:230) + any stale-comment hits surfaced during the stress-file verify pass.

---

## 2026-05-24 — Task 4 — Cleanup + comment sweep + grep cleanliness landed (11b4fe7)

- **Commit.** `11b4fe7` — Task 4 cleanup. 3 comment-only source edits, +22/-12. Release + Debug both clean (Jeff confirmed). No semantic code change but 2 of 3 edits are in headers ([EffectRack.h](Source/EffectRack.h) + [PluginProcessor.h](Source/PluginProcessor.h)) so dependent `.cpp` files recompiled.

#### Stale-comment carry-forwards closed (3 sites)

- **[EffectRack.h:230-235](Source/EffectRack.h:230)** — `promoteSlotPeakSnapshots()`'s declaration-preceding comment referenced the deleted `VibeGraph::promoteAllInsertPeakSnapshots()` (renamed to `promoteAllRackSlotSnapshots` in Task 2 when the per-insert peakDbSnap promotion half was removed). Updated to reference the new name + parenthetical noting the rename history (so rename archaeology lands in the right spot without commit-log spelunking).
- **[PluginProcessor.cpp:2135](Source/PluginProcessor.cpp:2135)** — inline label inside `drainMeterAtomicsForUI` said "Group 1: bus mirrors (Layers/Bass/Drums/Master)". Pre-QA-Eg the loop covered 4 buses; post-QA-Eg it covers 12 (Layers / Bass / Drums / Master / FX / AudioClips / Vox / Vox2 / Inst / Inst2 / Inst3 / Rusty). QA-Eg Task 8 NEEDS-FIX-2 rewrote the function-header docstring but missed this inline label — Task 4 closes the miss. Label updated to "Unified G1 bus drain" + the 12-bus enumeration + a short chain note (BusNode peakDb -> VibeGraph member atomic -> mirror via drainAndMerge). Bus count side-note: QA-Eg's close commit messages framed the count as "13 buses now share" — same miscount frozen in commit history (Master is in the 12, not separate); Task 4's new comment uses the accurate 12.
- **[PluginProcessor.h:436-444](Source/PluginProcessor.h:436)** — comment block above `kPeakAtomicNegInf` described the pre-batch "running-max companion atomics" model (the *Run mirrors that no longer exist post-Task-2). Block rewritten as "unified G1 meter snapshot sentinel" documenting the post-batch publish chain end-to-end (audio publishPeakReading -> VibeGraph atomic exchange-store -> drainAndMerge into mirror -> UI poll) covering both buses (12) + 8 InsertKinds. Sentinel name `kPeakAtomicNegInf` retained (used by drainAndMerge's skip-on-INF clause).

#### Grep cleanliness verified

- ZERO remaining live source references to `mAudioRowPeakDb*Run` / live `peakDbSnap*` fields / `mVibeGraph.drainInsertPeakDbStereo` / `mVibeGraph.getInsertPeakDb*` / "Group 1: bus mirrors" / "Group 2:" / "Group 3:" / "running-max companion" / stale `getInsertPeakDbStereoExchange`.
- 2 remaining `promoteAllInsertPeakSnapshots` hits are in MY intentional explanatory rename comments (`EffectRack.h:231` + `PluginProcessor.cpp:2118` history note) — documenting what changed for future archaeologists.

#### Diagnostic Instrumentation Catalog (Rule 4)

- **Nil entry.** Task 4 is cleanup-only — no instrumentation added. Catalog remains empty across the entire batch (Tasks 0-4 all nil).

#### Next action

- Begin **Task 5 close sequence**: dispatch `/draft-doc batch-close` (synthesizes the Implemented Work Log entry from this running-notes file) + `/review-batch QA-AudioMeters` (audits diff vs plan + CLAUDE.md rules + memory gotchas) in parallel; apply the batch-close draft to `Plans & Specs/Implemented Work Log.md` via Edit; address any BLOCKER / NEEDS-FIX from `/review-batch` in-batch (defer NITs into the close-entry routing table); surface full git status + drafted close commit message; commit on approval.

---

## 2026-05-24 — Task 5 — `/review-batch` BLOCKER + NEEDS-FIX fix-up (pre-close)

- **`/review-batch QA-AudioMeters` outcome.** 1 BLOCKER + 3 NEEDS-FIX + 5 NITs surfaced post-Task-4.  All 4 mandatory fixes landed in this fix-up commit; all 5 NITs deferred into the close-entry routing table per `feedback_closed_batch_carryforward_via_forks.md`.

#### BLOCKER — multi-call-per-block peak metering regression

- **Surface:** `VibeGraph::processInsert`'s new `storeAxes` lambda did a **plain `.store`** of the exchanged InsertNode peak into the per-kind VibeGraph atomic.  For InsertKinds called multiple times per block (**Audio** via Flow A + Flow B inside `CompositeAudioInsertTask::run`; **Vox** / **Inst** via per-FilePlay-player loops in `VoxStripTask` / `InstStripTask`), each subsequent call overwrote the prior call's published peak.  Pre-batch the deleted load-decay-max-store chain accumulated correctly across calls because the InsertNode's own `peakDb` retained state.  Post-batch the `exchange(-inf)` resets InsertNode peakDb between calls AND the lambda's plain `.store` overwrites the VibeGraph mirror.
- **Fix:** changed `storeAxes` from plain `.store` to a **CAS-max merge** using `compare_exchange_weak` + skip-on-INF clause (same shape as `drainAndMerge` + the existing `casMax` inside `publishPeakReading`).  ~10 lines added; preserves max-across-calls within a single audio block.  C++26's `std::atomic<T>::fetch_max` isn't available in MSVC/JUCE 7, so the CAS loop is portable.
- **Verify status — UNVERIFIED-BY-EAR, QA-J re-verify required.**  The natural observable test (two overlapping clips on the same audio row) hits **DSP-06 (the multi-clip stacking bug deferred to QA-J)** which silences the summed audio today; the meter has no signal to measure regardless of the storeAxes path semantics.  **QA-AudioMeters BLOCKER fix is architecturally correct per the `/review-batch` analysis but unverified-by-ear in QA-AudioMeters because the observable test (overlapping clips on same row) hits DSP-06; QA-J re-verify required** -- matches the existing `Fork-out (QA-J re-verify required)` pattern QA-F / QA-Fa / QA-Fb use for the same DSP-06 dependency.  Jeff's stress-file regression check on the original 6-point watchlist PASSED post-fix (no observable regression from the cleanup).

#### N1 — `kPeakAtomicNegInf` constant comment lied about usage

- **Surface:** `PluginProcessor.h:436-444` had a comment block above `kPeakAtomicNegInf` claiming "drainAndMerge's skip-on-INF clause uses this sentinel".  Grep confirms the constant was never referenced — every site (`drainAndMerge`, `storeAxes`, bus exchange-stores, `drainInsertPeakDbStereo`) defines its own local `kNI` / `kPeakNegInf` / `kBusNegInf`.  The constant was already dead pre-batch (introduced by QA-Eg) but Task 4's cleanup-pass rewrite confirmed the false claim rather than fixing it.
- **Fix:** deleted the constant + the comment block entirely.  Replaced with a brief explanatory comment noting the deletion + reasoning.

#### N2 — 8 mono `m<Kind>InsertPeakDb` mirror atomics were dead writes

- **Surface:** the 8 mono per-kind mirrors I added in Task 2 (`mAudioRowPeakDb` + 7 `m<Kind>InsertPeakDb`) were never read by any UI consumer — `VibeSynthProcessor::drainInsertPeakDbStereo` only returns L/R from the `*L/R` mirrors.  ~21 atomic stores per audio block of wasted work (8 from `drainAndMerge` + 8 from `storeAxes` exchange-stores + 8 init writes in ctor).  Bus mono mirrors (pre-existing pre-batch) have the same dead-write pattern; out-of-scope for QA-AudioMeters fix-up but a candidate for a separate cleanup batch.
- **Fix:** deleted all 8 mono mirror decls from `PluginProcessor.h` (lines 629, 633, 636, 639, 642, 645, 648, 651) + the matching 8 mono `initMirrorArr` calls from the ctor + the 8 mono `drainAndMerge` lines from `drainMeterAtomicsForUI`'s per-kind loops + the 8 mono VibeGraph `std::array` members from `VibeGraph.h` + the 8 mono `initArray` calls from `VibeGraph::prepare()` + the mono branch from the `storeAxes` lambda (the `dM` parameter + the matching mono store).  InsertNode `peakDb` mono atomic kept (still written by `publishPeakReading`, which is shared with BusNodes — full deletion would require restructuring the shared helper, out of scope).

#### N3 — `drainInsertPeakDbStereo` doc-comment was too permissive

- **Surface:** `PluginProcessor.h:655-658` doc-comment said "Wait-free; safe to call from UI thread."  The accessor IS technically wait-free + `noexcept` + relaxed-order, BUT it also exchange-resets the mirror to `-inf` — a second concurrent caller would receive `-inf` as the first call's exchange already cleared the mirror.
- **Fix:** rewrote the doc-comment to call out the single-consumer assumption: "**Single-consumer (UI vblank); concurrent callers race on the exchange-reset** -- a second concurrent caller would receive -inf as the first call's exchange already cleared the mirror.  Currently the only consumer is MixerPage::onVBlank."

#### NITs deferred (5)

Per `feedback_closed_batch_carryforward_via_forks.md` (NITs default to deferred into the close-entry routing table unless explicit fix-now call):

1. **Em-dash (U+2014) in source comment** at `PluginProcessor.cpp:2135` (`// Unified G1 bus drain — every bus...`).  Code-comment scope; em-dashes already used in other source comments; ASCII-only rule per `feedback_ascii_only_ui_strings.md` is user-facing-strings scope.  Deferred.
2. **Empty `if () { continue; }` style** at `PluginProcessor.cpp:433-440 / :654-661` left by L8 force-reset removal.  Pre-existing braces style; cosmetic.  Deferred.
3. **`bufferPeakDb` (mono) pre-existing C4505 unreferenced warning** at `VibeGraph.cpp:24`.  Pre-existing pre-batch via the stereo helper.  Deferred.
4. **One-sided `QA-AudioMeters` tagging in `publishPeakReading` comment** at `VibeGraph.cpp:113-120` (bus branch untagged; insert branch tagged).  Cosmetic.  Deferred.
5. **Redundant in-class `peakDb { -60.f }` initializers** on `InsertNode` now that `VibeGraph::prepare()` explicit-inits the same atomic.  Belt-and-suspenders; harmless.  Deferred.

#### Build + regression check

- **Release + Debug both clean** post-fix-up (Jeff confirmed "We're good").  No new warnings.
- **Original 6-point stress-file watchlist regression check PASSED** — re-ran stress arrangement post-fix; per-strip meters on all 8 InsertKinds + 12 G1 bus regression + mute decay + MT parity + save+reload + EffectRack slot meter spot-check all still read correctly.  Mono-mirror deletions don't affect any observable behavior (consumers were L/R-only).

#### Diagnostic Instrumentation Catalog (Rule 4)

- **Nil entry.** Fix-up commit is structural code + comment edits only — no instrumentation added.  Catalog remains empty across the entire batch (Tasks 0-5 all nil).

#### Next action

- Surface full git status + dispatch `/draft-commit` for the fix-up commit (4 source files + Task 4 running-notes entry already in tree + THIS Task 5 fix-up entry).  Once landed: apply the Implemented Work Log batch-close entry via Edit + update Main Plan §5 QA-AudioMeters STATUS banner with CLOSE summary + add Task 5 close-pass running-notes section (if any final notes needed) + `/draft-commit` for the final close commit.
