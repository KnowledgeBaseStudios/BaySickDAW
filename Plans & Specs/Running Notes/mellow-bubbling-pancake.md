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
