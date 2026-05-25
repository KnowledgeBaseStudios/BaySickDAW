# Running Notes — QA-InsertMaps (zany-wandering-russell)

> Append-only mid-batch log. A new `## YYYY-MM-DD — Task N — <name>` entry is
> appended at every checkpoint (commit landed / sub-task verified / finding
> captured / spec call resolved / scope pivot) per
> `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close,
> `/draft-doc batch-close` consumes this file as the primary input for the
> single Implemented Work Log entry. Never edited retroactively.

**Pair:** `Plans & Specs/Batch Plans/zany-wandering-russell.md` (the plan).
**Conventions:** Main Plan §0 — Document Formatting Conventions + running-notes
required sections (locked 2026-05-11) + Rule 4 (Diagnostic Instrumentation Catalog).

---

## 2026-05-24 — Task 0 — Batch open (docs)

- **Scope.** QA-InsertMaps flattens the 8 `std::map<int, std::unique_ptr<InsertNode>>`
  member tables on `VibeGraph` ([VibeGraph.h:727-733+750](Source/VibeGraph.h:727))
  into a single owning `std::array<std::unique_ptr<InsertNode>, kMaxStripChannels=1000>
  mInsertsByChannel` indexed directly by ChannelId, with a companion
  `std::vector<int> mLiveInsertChannels` for the 12+ iteration sites.  Eliminates
  the `selectInsertMap` switch ([VibeGraph.cpp:2253-2277](Source/VibeGraph.cpp:2253))
  + the 4x red-black-tree walks per insert per audio block (1 in `processInsert`
  via `getInsertNode` :2335 + 3 inside `pushScArrayToStrip` via `getInsertPreEQ`
  / `getInsertRack` / `getInsertEQ` :2887).  External `(kind, index)` accessor
  API kept as thin wrappers (zero call-site churn at the 30+ external sites in
  6 files).  InsertNode caches its own `chId` at construction; `processInsert`'s
  8-way kind->chId switch at :2343-2353 dies on the audio thread.  Mirrors
  `RenderGraphDispatcher::mTasksByChannel`'s already-in-tree flat-array-by-
  ChannelId pattern.  Second batch in the QA-Eg-close-spawned perf-audit cluster
  (after QA-AudioMeters; before QA-VoicePool + QA-EngineApvts).  Also absorbs
  perf-audit M3 (UI-side `getInsertPeakDb` per-vblank `std::map::find`).

- **Pre-batch reads (BaySickDAW boilerplate sequence).** `/standup` (QA-AudioMeters
  closed `65f57ad` 2026-05-24; tree clean of source apart from untracked
  `Templates/My Templates/Test Kit.xml` predating the perf-audit cluster; branch
  ahead of origin by the cluster commits, no push expected).  Full direct self-
  read of Main Plan §0 lines 1-800 (Rules 1-4 + Document Formatting Conventions
  incl. the Batch Plans + Running Notes required-sections rule locked 2026-05-11
  + canonical buckets + Agent Orchestration Rules).  `/read-doc` extractions:
  §5 QA-InsertMaps entry verbatim (`Main Plan.md:1182-1197`), §6 sequencing
  arrow + the 4-before / 4-after footnotes (QA-Eg through QA-DirtyFlag), §9
  thirty-third / thirty-fourth / thirty-fifth Forks entries (QA-InsertMaps /
  QA-VoicePool / QA-EngineApvts routing).  Carry-Forward Reference §-section
  extractions (no §-section relevant to perf-audit M3 — Carry-Forward frozen
  2026-05-07 predates the H1/M3 finding).  Work Log QA-AudioMeters + QA-Eg
  close entries (routing tables + FND items confirmed FND-8 = H1 = QA-InsertMaps
  Jeff-locked Option 2).  Federated-bouncing-cupcake exemplar full self-read
  for plan-file required-sections structure.

- **CLAUDE.md status cross-check vs Work Log.**  CLAUDE.md "Next Steps" section
  states "Next batch: QA-Md" (May-2026-08 vintage) — confirmed stale via Work
  Log: QA-Md closed 2026-05-09; QA-A / QA-C / QA-D / QA-E / QA-Ea / QA-Ef /
  QA-Eg / QA-AudioMeters all subsequently closed; QA-InsertMaps is the actual
  next batch per §6 arrow + Work Log close entry `mellow-bubbling-pancake.md`
  at line :1131.  Did not repeat the stale claim.

- **Pre-plan source-trace (verifying §5 entry's file:line claims before drafting).**
  - 8 `std::map<int, std::unique_ptr<InsertNode>>` member decls confirmed at
    [VibeGraph.h:727-733+750](Source/VibeGraph.h:727) — actual member names
    `mLayerInserts / mBassInserts / mDrumInserts / mAudioInserts / mAuxInserts /
    mVoxInserts / mInstInserts / mRustyInserts` (the §5 grep pattern
    `mInserts<Kind>.find(` was wrong — correct prefix is `m<Kind>Inserts.find(`;
    flagged for Task 4 cleanup sweep target).
  - `selectInsertMap` anon-namespace helper at [VibeGraph.cpp:2253-2277](Source/VibeGraph.cpp:2253);
    3 callers (`ensureInsertNode` :2285, `removeInsertNode` :2318, `getInsertNode` :2325).
  - `processInsert` at [:2331](Source/VibeGraph.cpp:2331) with 8-way kind->chId switch at :2343-2353.
  - `pushScArrayToStrip` at [:2853](Source/VibeGraph.cpp:2853); `push3()` at :2887
    calls `getInsertPreEQ / getInsertRack / getInsertEQ` (3 more map.finds per call).
    Net: 4 map.finds per `processInsert` call — the §5 entry's claim verified.
  - 12+ range-for loops over individual maps grepped: `walkInserts x8`
    at :1755-1762, `addInsertMap x8` at :1983-1990 (XML save), `restoreInsert x8`
    at :2126-2133 (XML restore), `promoteRacksInMap x8` at :2477-2484
    (QA-AudioMeters rack-promotion), drainMeterAtomicsForUI G1 8-per-kind blocks
    at :2930-2951, pointer-initializer-list loops at :1364 / :1409 / :2015 / :2567,
    Aux-specific range-for at :2970.
  - Direct `.find()` call sites outside `selectInsertMap`: 5 — `mLayerInserts`
    :1798, `mBassInserts` :1805, `mAuxInserts` :1813, `mAudioInserts` :2195 / :2207.
  - 2nd selectInsertMap-style switch at :2512-2514 (function context TBD in
    Task 1 inventory).
  - 30+ external call sites across 6 files: `PluginProcessor.cpp` x12
    (8 `ensureInsertNode` + 2 `removeInsertNode` + 2 `getInsertEQ / PreEQ`);
    `EffectsPage.cpp` x22 (`getInsertRack` / `getInsertEQ` per-kind switch +
    hardcoded chId-range-to-(kind, index) preEQ switch at :540-547);
    `StandaloneEditor.cpp` x5 `getInsertRack`; `MixerPage.cpp` x1
    `removeInsertNode(Aux)`; `ClipsPage.cpp` comment-only references.
  - `Source/Engine/Tasks/`: zero matches — `CompositeAudioInsertTask` /
    `VoxStripTask` / `InstStripTask` / `EngineInsertTask` do NOT call the
    VibeGraph insert accessors directly.  All audio-thread entry into the
    insert lookup chain is via `processInsert`.
  - InsertNode forward-declared at [VibeGraph.h:466](Source/VibeGraph.h:466);
    full definition lives in `VibeGraph.cpp` (opaque to consumers — the
    per-kind `getInsertRack / EQ / PreEQ` wrappers exist precisely because
    consumers can't dereference `node->rack / postEq / preEq` through the
    forward decl alone).
  - `MixerChannelIds` namespace at [VibeGraph.h:35-193](Source/VibeGraph.h:35):
    chId range 0..813 max (kRustyBase 800 + kMaxRustyStrips 13); §5's
    kMaxStripChannels=1000 covers the full allocation with ~190-entry headroom
    (~8 KB array at 8 B/ptr, sparsely populated).

- **Spec calls resolved with Jeff (2026-05-24, two-phase: ExitPlanMode + post-
  ExitPlanMode sub-spec dispatch):**
  - **Phase 1 (ExitPlanMode approval):**
    - **L1** = Option 2 (single flat `std::array<...>` by ChannelId, mirror
      `RenderGraphDispatcher::mTasksByChannel`).
    - **L2** = kMaxStripChannels = 1000.
    - **L3** = Sequencing immediately after QA-AudioMeters, before QA-VoicePool.
    - **L4** = Silly-name `zany-wandering-russell` (plan-mode runtime assignment).
    - **L5** = MT verification cadence normal Debug-then-Release per-task verify
      (no separate MT-vs-serial pass).
  - **Phase 2 (post-ExitPlanMode sub-spec dispatch after Jeff interrupted my
    too-fast Task-0 start — see "Sub-spec workflow correction" below):**
    - **Sub-A / L6 = (a)** — `std::array<std::unique_ptr<InsertNode>, kMaxStripChannels>
      mInsertsByChannel` (array IS owning storage; lookup via `[chId].get()`).
    - **Sub-B / L7 = (a)** — keep `(kind, index)` external API; chId computed
      inside via `computeChannelId(kind, index)` helper.  Encapsulation preserved.
    - **Sub-C / L8 = (a)** — single `std::vector<int> mLiveInsertChannels`
      companion list with per-iter `node->kind` dispatch in kind-aware loops.
    - **Sub-D / L9 = (a)** — InsertNode caches `int chId` at construction;
      `processInsert`'s 8-way switch at :2343-2353 dies on the audio thread.
    - **Sub-E / L10 = (a)** — single all-kinds stress-file verify (QA-AudioMeters
      L6 precedent).
    - **Sub-F / L11 = (a)** — 6-task QA-AudioMeters mirror (Task 0 open / Task 1
      inventory / Task 2 structural / Task 3 stress-file verify / Task 4 cleanup
      / Task 5 close).

- **Sub-spec workflow correction (lesson learned).**  Initial draft surfaced the
  6 sub-spec calls via the plan file's "Sub-spec calls surfaced for ExitPlanMode"
  table WITH recommendations + reasoning, then attempted to start Task 0 mirror
  after Jeff's ExitPlanMode approval without explicit per-call resolution.  Jeff
  interrupted ("WAIT, you didn't ask me any of these questions NOT APPROVED").
  Re-surfaced via `AskUserQuestion` box (4 questions at once for Sub-A through
  Sub-D); Jeff dismissed: "Please write them all out with their options instead
  of giving me the box its so much easier to review everything at once."
  Re-re-surfaced via plain-English numbered list with options + recommendations
  + reasoning for each of all 6 sub-spec calls in one message; Jeff confirmed
  Option (a) across all 6 ("Your analysis is flawless... Option A across the
  board").  Reinforces two memories:
  - `feedback_dont_make_unilateral_spec_calls.md` — recommendations in a plan
    file do NOT substitute for explicit per-call resolution at ExitPlanMode.
  - `feedback_design_approval_in_plain_english.md` (extension) — for multi-
    question batches (>2 questions), plain-text discrete-question presentation
    beats the `AskUserQuestion` box for batch review.  Recommendation: when
    surfacing 3+ spec calls at once, use plain-English numbered list.
  Plan file's "Sub-spec calls surfaced for ExitPlanMode" table preserved as the
  plan record (per federated-bouncing-cupcake exemplar pattern) + resolution
  note added inline + L6-L11 rows added to "Spec calls already locked" table
  for one-stop reference.

- **Plan file mirrored.** `~/.claude/plans/zany-wandering-russell.md` ->
  `Plans & Specs/Batch Plans/zany-wandering-russell.md`; home-dir copy deleted
  per `feedback_plan_mirror_one_way.md`.

- **Main Plan §5 updated.** QA-InsertMaps entry's `**Plan file:**` line
  ([Main Plan.md:1184](Plans & Specs/Main Plan.md:1184)) flipped from the
  `<silly-name>.md (when started)` placeholder to
  `Plans & Specs/Batch Plans/zany-wandering-russell.md`.

- **Rule 4 Diagnostic Instrumentation Catalog:** nil for Task 0 (docs-only;
  no source touches).

- **Next action.** Task 1 — pre-flight inventory (read-only).  Expand the
  initial §5 + plan-mode grep findings into a complete file:line surface
  table appended to this running-notes file for Task 2 reference.  Verify the
  2nd selectInsertMap-style switch at `VibeGraph.cpp:2512-2514`, the XML
  save / restore tag-label preservation requirement (`addInsertMap` /
  `restoreInsert` per-kind name string), and InsertNode's current constructor
  signature for the `chId` parameter addition.
