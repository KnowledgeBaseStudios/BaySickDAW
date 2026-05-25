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
