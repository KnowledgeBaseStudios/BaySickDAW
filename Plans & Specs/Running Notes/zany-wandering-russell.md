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

---

## 2026-05-24 — Task 1 — Pre-flight inventory (read-only)

- **Scope.** Read-only inventory pass per Sub-F / L11 6-task structure: enumerate
  every `std::map<int, std::unique_ptr<InsertNode>>` access surface across
  `VibeGraph.cpp` (call sites + iteration sites + .find/.size/.clear sites),
  verify the §5-entry + Task-0-grep claims against actual code, lock the
  rewrite map for Task 2's structural one-shot, and surface any asymmetries /
  ambiguities as Task-2 spec-call candidates per
  `feedback_dont_make_unilateral_spec_calls.md`.  Zero source touches; the
  output is this inventory table.

- **Complete VibeGraph.cpp call-site inventory (sorted by line).**

  | Line | Kind | What it does | Iteration shape | Post-batch replacement |
  |------|------|--------------|-----------------|------------------------|
  | [1364-1366](Source/VibeGraph.cpp:1364) | range-for | `prepare()` sweep over per-insert nodes -- ONLY 7 KINDS, NO Rusty | pointer-init-list `{ &mLayerInserts, &mBassInserts, &mDrumInserts, &mAudioInserts, &mAuxInserts, &mVoxInserts, &mInstInserts }` then range-for | `for (int chId : mLiveInsertChannels) mInsertsByChannel[chId]->prepare(...)` -- naturally covers Rusty too (asymmetry fix) |
  | [1409-1411](Source/VibeGraph.cpp:1409) | range-for | `reset()` sweep -- ONLY 5 KINDS, NO Vox/Inst/Rusty | pointer-init-list `{ &mLayerInserts, &mBassInserts, &mDrumInserts, &mAudioInserts, &mAuxInserts }` then range-for | same shape -- but flag for Task 2 spec call: keep filter (preserve current behavior) or include all kinds (cleaner end state)? See Finding B below. |
  | [1755-1762](Source/VibeGraph.cpp:1755) | lambda + 8 calls | `walkInserts` for FX-bypass chain walk -- all 8 kinds | helper lambda, called per kind | single `for (int chId : mLiveInsertChannels) walkInserts(mInsertsByChannel[chId].get())` |
  | [1798](Source/VibeGraph.cpp:1798) | direct `.find()` | Legacy Layer-rack fallback lookup | `mLayerInserts.find(idx)` | `mInsertsByChannel[MixerChannelIds::layerInsert(idx)].get()` |
  | [1805](Source/VibeGraph.cpp:1805) | direct `.find()` | Legacy Bass-rack fallback lookup | `mBassInserts.find(idx)` | `mInsertsByChannel[MixerChannelIds::bassInsert(idx)].get()` |
  | [1813](Source/VibeGraph.cpp:1813) | direct `.find()` | Aux-rack lookup | `mAuxInserts.find(idx)` | `mInsertsByChannel[MixerChannelIds::auxStrip(idx)].get()` |
  | [1964-1990](Source/VibeGraph.cpp:1964) | lambda + 8 calls | `addInsertMap` for XML state save -- per-kind string label preserved as `"Layer" / "Bass" / "Drum" / "Audio" / "Aux" / "Vox" / "Inst" / "Rusty"` in `<InsertRack kind="...">` XML attribute (MUST preserve labels for project load compat) | helper lambda, 8 calls with per-kind label arg | rewrite lambda to iterate `mLiveInsertChannels` + derive kind label from `node->kind` enum via helper `kindString(InsertKind)` |
  | [2014-2018](Source/VibeGraph.cpp:2014) | range-for | Rack-wipe sweep post-`wipe(node->rack)` -- all 8 kinds | pointer-init-list `{ &mLayerInserts, ... &mRustyInserts }` then range-for + null-check | single `for (int chId : mLiveInsertChannels) if (auto* n = mInsertsByChannel[chId].get()) wipe(n->rack)` |
  | [2107-2133](Source/VibeGraph.cpp:2107) | lambda + 8 calls | `restoreInsert` for XML state restore -- per-kind label-matched against saved `kind` attribute (MUST preserve label semantics) | helper lambda, 8 calls with per-kind label arg | rewrite lambda to iterate `mLiveInsertChannels` filtered by per-kind label (or by `node->kind` enum match against label string) |
  | [2195](Source/VibeGraph.cpp:2195) | direct `.find()` | Audio-row rack lookup (5F-4a Batch 6) | `mAudioInserts.find(row)` | `mInsertsByChannel[MixerChannelIds::audioInsert(row)].get()` |
  | [2207](Source/VibeGraph.cpp:2207) | direct `.find()` | Audio-row EQ lookup (5F-4a Batch 6) | `mAudioInserts.find(row)` | same as :2195 |
  | [2253-2277](Source/VibeGraph.cpp:2253) | `selectInsertMap` definition | anon-namespace helper -- 8-way kind switch, returns map pointer | switch + 8 cases | DELETE entirely -- replaced by `computeChannelId(kind, index)` |
  | [2281-2314](Source/VibeGraph.cpp:2281) | `ensureInsertNode` | `selectInsertMap` + `map.find` + `map.insert` | uses `selectInsertMap` | rewrite: `chId = computeChannelId`; if `mInsertsByChannel[chId]` exists rebind + return; else construct new InsertNode (pass chId to ctor for L9 cache), push chId onto `mLiveInsertChannels`, install in `mInsertsByChannel[chId]` |
  | [2316-2320](Source/VibeGraph.cpp:2316) | `removeInsertNode` | `selectInsertMap` + `map.erase` | uses `selectInsertMap` | rewrite: `chId = computeChannelId`; `mInsertsByChannel[chId].reset()`; erase `chId` from `mLiveInsertChannels` |
  | [2322-2329](Source/VibeGraph.cpp:2322) | `getInsertNode` | `selectInsertMap` + `map.find` | uses `selectInsertMap` | rewrite: `return mInsertsByChannel[computeChannelId(kind, index)].get()` |
  | [2331-2424](Source/VibeGraph.cpp:2331) | `processInsert` | THE audio-thread hot path; 8-way kind->chId switch at :2343-2353 dies post-batch (uses `node->chId`); rest of function (CAS-max `storeAxes` for QA-AudioMeters meter publish) untouched | switch + 8 cases + `getInsertNode` + `pushScArrayToStrip` + `node->processBlock` + `storeAxes` CAS-max | rewrite: `auto* node = mInsertsByChannel[computeChannelId(kind, index)].get(); if (! node) return; pushScArrayToStrip(node->chId); node->processBlock(...); storeAxes(...)` -- switch dies |
  | [2425-2447](Source/VibeGraph.cpp:2425) | `getInsertRack` / `getInsertEQ` / `getInsertPreEQ` | thin wrappers around `getInsertNode` (each calls it) | no change post-batch | thanks to L7/Sub-B keep-API, these wrappers stay unchanged externally; internally each becomes a single array-index load via the rewritten `getInsertNode` |
  | [2472-2484](Source/VibeGraph.cpp:2472) | lambda + 8 calls | `promoteRacksInMap` for QA-AudioMeters rack-promotion sweep (per-kind) | helper lambda, 8 calls | single `for (int chId : mLiveInsertChannels) promoteRacksInMap(mInsertsByChannel[chId].get())` |
  | [2507-2526](Source/VibeGraph.cpp:2507) | `getInsertChokeGroup` with 2nd `selectInsertMap`-style switch at :2509-2520 then `map.find` at :2522 -- NEW FINDING; this is the §5 entry's "2512-2514" line reference; D3 drum-choke feature | local switch + 8 cases + `map.find` | rewrite: `if (auto* node = mInsertsByChannel[computeChannelId(kind, index)].get()) if (auto* p = node->pChokeGroup) return ...; return 0` |
  | [2566-2572](Source/VibeGraph.cpp:2566) | `isAnyInsertSoloed` -- NEW FINDING beyond initial 12+ count; pointer-init-list range-for over all 8 maps + nested for-each + early-return on first soloed insert | pointer-init-list `{ &mLayerInserts, ... &mRustyInserts }` + nested range-for | rewrite: `for (int chId : mLiveInsertChannels) if (auto* n = mInsertsByChannel[chId].get()) if (n->isSoloed()) return true; return false` |
  | [2853-2890](Source/VibeGraph.cpp:2853) | `pushScArrayToStrip` | takes channelId DIRECTLY; internal switch on chId range decodes to (kind, index) to call `push3(getInsertPreEQ, getInsertRack, getInsertEQ)` -- 3 more `map.find`s per call via the wrappers | takes chId; calls 3 (kind, index)-keyed wrappers internally | the function itself doesn't change (it takes chId already); the wrappers it calls become single-load via L7/Sub-B; net 3 `.find()`s -> 3 array-index loads per call |
  | [2899-2962](Source/VibeGraph.cpp:2899) | `rebuildRoutingFromApvts` -- NEW FINDING; uses `.size()` on all 8 maps at :2905-2908 for `reserve` + 8 separate range-for blocks at :2930-2952 to `emplace_back` per-kind chId + apvtsPrefix into `mActiveChannels` | `.size()` chain + 8 per-kind range-for | rewrite: `mActiveChannels.reserve(13 + mLiveInsertChannels.size())` + single `for (int chId : mLiveInsertChannels) { auto* n = mInsertsByChannel[chId].get(); if (n) mActiveChannels.emplace_back(chId, n->apvtsPrefix); }` -- no per-kind chId helpers needed (chId already known) |
  | [2930-2951](Source/VibeGraph.cpp:2930) | `drainMeterAtomicsForUI` G1 8-per-kind blocks (QA-AudioMeters) | 8 separate per-kind range-for blocks each emplacing chId via per-kind helper | single loop over `mLiveInsertChannels` with switch on `node->kind` for per-kind PluginProcessor mirror routing |
  | [2966-2972](Source/VibeGraph.cpp:2966) | `getAuxIndices()` -- NEW FINDING; range-for over `mAuxInserts` to collect aux indices into vector | range-for over `mAuxInserts` | rewrite: `for (int chId : mLiveInsertChannels) if (chId >= kAuxBase && chId < kAuxBase + kMaxAuxStrips) result.push_back(chId - kAuxBase)` -- filter by chId range (or by `node->kind == Aux`) |
  | [2975-2978](Source/VibeGraph.cpp:2975) | `clearAuxInserts()` -- NEW FINDING; `.clear()` on `mAuxInserts` | `.clear()` | rewrite: iterate `mLiveInsertChannels` filtered to Aux, reset each in `mInsertsByChannel`, erase from `mLiveInsertChannels` (or simpler: iterate + `std::erase_if`) |

- **selectInsertMap callers -- confirmed exactly 3 (matches §5 entry).**
  - [:2285](Source/VibeGraph.cpp:2285) -- `ensureInsertNode`
  - [:2318](Source/VibeGraph.cpp:2318) -- `removeInsertNode`
  - [:2325](Source/VibeGraph.cpp:2325) -- `getInsertNode`

- **pushScArrayToStrip callers -- 4 internal-to-VibeGraph + 1 doc comment
  (CONFIRMED no Engine/Tasks/ direct callers).**
  - [VibeGraph.cpp:1461](Source/VibeGraph.cpp:1461) -- `processMasterBus` (passes `MixerChannelIds::kMaster`)
  - [VibeGraph.cpp:1505](Source/VibeGraph.cpp:1505) -- `processBus` (passes `busChId` from caller)
  - [VibeGraph.cpp:1782](Source/VibeGraph.cpp:1782) -- FX-bus processing (passes `MixerChannelIds::kFxBus`)
  - [VibeGraph.cpp:2354](Source/VibeGraph.cpp:2354) -- `processInsert` (passes `chId` computed from kind+index switch)
  - [VibeGraph.h:592](Source/VibeGraph.h:592) -- public method declaration
  - [MasterTask.cpp:60](Source/Engine/Tasks/MasterTask.cpp:60) -- doc comment only

- **Engine/Tasks/ verification -- CONFIRMED no direct accessor calls.**  Grep
  for `(getInsertNode|getInsertRack|getInsertEQ|getInsertPreEQ|ensureInsertNode|removeInsertNode|processInsert|pushScArrayToStrip)`
  in `Source/Engine/` matched only doc comments in `EngineInsertTask.h:23`,
  `InstStripTask.h:29`, `PassiveStripTask.h:21`, `VoxStripTask.h:26` -- all
  describing what the task ultimately calls via the synth/graph instance, NOT
  actually calling.  The Engine/Tasks/* layer is untouched by this batch.

- **InsertNode full definition** ([VibeGraph.cpp:1033+](Source/VibeGraph.cpp:1033)):
  ```cpp
  struct VibeGraph::InsertNode
  {
      // Identity
      juce::String          name;
      juce::String          apvtsPrefix;
      VibeGraph::InsertKind kind  { VibeGraph::InsertKind::Layer };
      int                   index { 0 };
      // [NEW for L9/Sub-D: add int chId { -1 }; member here]

      // Audio DSP
      EQ8MsDSP              preEq;       // P4.3 pre-rack
      EffectRack            rack;
      EQ8MsDSP              eq;          // post-rack
      CompDelayLine         compDelay;

      // QA-AudioMeters G1-pattern peak fields
      std::atomic<float>    peakDb  { -60.f };
      std::atomic<float>    peakDbL { -60.f };
      std::atomic<float>    peakDbR { -60.f };
      // [...more fields below, including pChokeGroup, fader smoothing, cached APVTS ptrs]
  };
  ```
  Constructor signature (per call at [:2299](Source/VibeGraph.cpp:2299)):
  `InsertNode(kind, index, displayName, apvtsPrefix)` -- for L9/Sub-D add a
  `chId` parameter OR compute internally from kind+index.  The latter requires
  InsertNode to include `MixerChannelIds.h`; the former keeps InsertNode
  dependency-light + `ensureInsertNode` passes the chId it already computed.
  Recommendation surfaced for Task 2: ctor-parameter form (matches Sub-D's
  "InsertNode caches its own chId at construction" wording from ExitPlanMode
  approval).

- **Counts summary.**
  - **Total `.find()` sites in `VibeGraph.cpp`:** 8 (3 via selectInsertMap-routed
    accessors at :2287 / :2325 / :2326 + the `ensureInsertNode` existence check;
    5 direct at :1798 / :1805 / :1813 / :2195 / :2207; 1 in `getInsertChokeGroup`
    at :2522).
  - **Total iteration sites in `VibeGraph.cpp`:** 14 (was 12+ in initial Task 0
    count; added `isAnyInsertSoloed` and `rebuildRoutingFromApvts` blocks):
    :1364, :1409, :1755-1762, :1983-1990, :2014, :2126-2133, :2477-2484, :2567,
    :2930-2952, :2970 (10 distinct iteration blocks; some block-counts have
    multiple per-kind calls within).
  - **Total `.size()` sites:** 1 at :2904-2908 (`mActiveChannels.reserve` sum).
  - **Total `.clear()` sites:** 1 at :2977 (`mAuxInserts.clear()` in
    `clearAuxInserts`).
  - **External call sites in other files** (5 .cpp + 1 .h comment-only):
    unchanged from §5 entry -- `PluginProcessor.cpp` x12, `EffectsPage.cpp` x22,
    `StandaloneEditor.cpp` x5, `MixerPage.cpp` x1, `ClipsPage.cpp` comment-only
    refs, `PluginProcessor.h` comment-only refs.

- **Asymmetry findings (Task 2 spec-call candidates).**

  - **Finding A -- `prepare()` at :1364-1366 excludes Rusty.**  Likely defensive
    (Rusty inserts get prepared on construction via `ensureInsertNode` at
    :2300-2302 if `mSampleRate > 0.0`; the `prepare()` sweep is a fallback for
    inserts that existed before `prepareToPlay`).  Recommendation for Task 2:
    include all 8 kinds via `mLiveInsertChannels` iteration (cleaner end state,
    no behavior risk since the duplicate prepare is safe).  Non-risk; Task 2
    will silently fix.

  - **Finding B -- `reset()` at :1409-1411 excludes Vox/Inst/Rusty.**  More
    concerning -- reset is project-load critical.  Could be intentional
    (Vox/Inst/Rusty handled by a separate reset path elsewhere -- needs further
    verify) OR pre-existing oversight when R1 (Vox/Inst, 2026-04-23) and J-4
    (Rusty, 2026-05-03) extended the InsertKind enum without updating the
    5F-4a reset loop.  Spec-call surface for Task 2:
    1. preserve original 5-kind filter (zero behavior change risk, but the
       asymmetry persists post-batch);
    2. include all 8 kinds via `mLiveInsertChannels` (cleaner; if reset wasn't
       reaching Vox/Inst/Rusty before, fixing it might surface latent state-
       retention bugs but those are arguably real bugs the fix exposes);
    3. keep the original 5-kind filter post-batch but route the asymmetry
       investigation to a separate §9 Forks entry for follow-up.

    My recommendation: surface (1)/(2)/(3) to Jeff at Task 2 plan-finalize per
    `feedback_dont_make_unilateral_spec_calls.md` -- slot/scope call, not mine
    to pick.

- **Rule 4 Diagnostic Instrumentation Catalog:** nil for Task 1 (read-only
  inventory; no source touches; no DBG / Logger / temp jassert / debug
  AlertWindow added).

- **Next action.** Task 2 -- structural one-shot.  Open with a Task 2 plan-
  finalize that surfaces Finding B's `reset()` asymmetry decision to Jeff (the
  only spec call Task 1 surfaced; Finding A's `prepare()` asymmetry is non-risk
  and Task 2 will silently fix).  Then edit `VibeGraph.h` + `VibeGraph.cpp` per
  the rewrite map above; build verify Debug+Release; commit single structural
  commit per L11/Sub-F task structure.

---

## 2026-05-25 — Tasks 2 / 3 / 4 catch-up (running-notes consolidation pre-close)

> Catch-up entry consolidating Tasks 2 / 3 / 4 into the running notes.  Per-task
> `/draft-doc running-notes` appends were skipped during live execution
> (deviation from `feedback_draft_doc_running_notes_every_checkpoint.md`);
> caught up here pre-close so `/draft-doc batch-close` has full input.  Process
> lesson noted at the bottom of this entry.

- **Task 2 plan-finalize (Sub-G + Sub-H + Finding C resolutions, 2026-05-24).**
  Surfaced Task 1's Finding B reset-asymmetry as Sub-G (3 options) +
  InsertNode chId-source as Sub-H (2 options) for Jeff's pick.  Jeff resolved
  Sub-G = Option 2 (include all 8 kinds in reset loop for symmetric end state)
  WITH a pre-execution sanity check on `InsertNode::reset()` to confirm it
  doesn't destructively wipe Vox/Inst/Rusty user state.  Sanity check:
  `InsertNode::reset()` at [VibeGraph.cpp:1101](Source/VibeGraph.cpp:1101)
  only flushes `preEq.reset() + rack.reset() + eq.reset()` (DSP buffer state);
  does NOT touch FilePlay sources, sfizz engine state, loaded sample paths,
  or APVTS values -- safe for all 8 kinds.  Sub-H = Option (a) ctor-parameter
  form (ensureInsertNode computes chId once for the lookup, passes to ctor;
  InsertNode caches without re-computing; keeps InsertNode dependency-light).
  **Finding C surfaced during Sub-G sanity check:** grep for
  `mVibeGraph.reset / vibeGraph.reset / vg.reset / &VibeGraph::reset` across
  `Source/` returned ZERO matches -- `VibeGraph::reset()` is dead code, no
  runtime caller.  PluginProcessor's `.reset()` calls are all on
  `std::unique_ptr<Task>` instances (releasing tasks), never on mVibeGraph.
  The 5-kind asymmetry from Finding B never caused a runtime bug because the
  function never runs.  Jeff routed: Sub-G Option 2 + new §9 Forks entry to
  investigate wiring `VibeGraph::reset()` to transport Stop in a future
  dedicated batch; QA-InsertMaps does NOT delete the reset() function
  (scope-creep avoidance).  §9 thirty-sixth Forks entry appended to Main Plan
  with Jeff's verbatim Title/Context/Impact/Action wording wrapped in §0
  conventional scaffolding (date header / back-refs / sequencing TBD).

- **Task 2 structural one-shot landed at commit `eb718bf` (2026-05-25).**  3
  files changed (`Plans & Specs/Main Plan.md` + `Source/VibeGraph.h` +
  `Source/VibeGraph.cpp`), +294/-215.  Implementation per Task 1 inventory's
  rewrite map: VibeGraph.h replaced 8 `std::map<int, std::unique_ptr<InsertNode>>`
  member decls + the lone Rusty decl at original :750 + the surrounding doc
  comment with `static constexpr int kMaxStripChannels = 1000;
  std::array<std::unique_ptr<InsertNode>, kMaxStripChannels> mInsertsByChannel;
  std::vector<int> mLiveInsertChannels;` + new flat-array-by-ChannelId doc;
  VibeGraph.cpp 20 distinct rewrites covering top-of-file `computeChannelId`
  helper hoist, InsertNode struct `int chId { -1 }` field add + ctor signature
  update (Sub-H Option (a)), `selectInsertMap` deletion + "moved to top"
  pointer comment, 3 accessor rewrites (ensureInsertNode / removeInsertNode /
  getInsertNode), processInsert per-block kind->chId switch DEATH (uses
  `node->chId`), 5 legacy fallback rack-getter rewrites, prepare() loop
  Finding A silent fix (all 8 kinds), reset() loop Finding B + Sub-G Option 2
  fix (all 8 kinds, inline note re: dead-code state + §9 Forks routing),
  walkInserts + addInsertMap + wipe-racks + restoreInsert + promoteRacksInMap
  + getInsertChokeGroup (2nd switch) + isAnyInsertSoloed +
  rebuildRoutingFromApvts + getAuxIndices + clearAuxInserts rewrites.

- **Task 2 build cycle (2026-05-25).**  First build attempt FAILED both
  Release + Debug with `VibeGraph.cpp(2151,26): error C3861: 'computeChannelId':
  identifier not found`.  Root cause: I placed the `computeChannelId` helper
  in the anon namespace where `selectInsertMap` used to be (~line 2253), BEFORE
  which `restoreInsert` lambda lives (~line 2151) -- forward-use error.  Fix:
  moved the `computeChannelId` anon namespace to TOP of VibeGraph.cpp (line
  ~22, right after `#include <algorithm>` and before the bus-node section
  header).  Original location at ~:2253 preserved with a one-line "moved to
  top" pointer comment for future readers doing rename archaeology.  Second
  build attempt: Release + Debug both PASS clean.  Pre-existing warnings
  (C4324 RenderTask alignment padding, C4996 juce::Font::Font deprecation,
  C4189 unused locals, C4100 unreferenced parameters, C4456/4457/4458
  declaration shadowing, C4702 unreachable code) unchanged from baseline --
  same noise as the pre-batch build, no new warnings introduced.

- **Task 3 all-kinds stress-file verify PASS (2026-05-25).**  Jeff ran the
  big stress-test arrangement (same one used at QA-AudioMeters Task 3) +
  walked the 8-point watchlist: (1) all 8 InsertKind per-strip meters read
  activity + decay smoothly; (2) all 13 G1 bus meters read activity (no
  regression on the QA-Eg + QA-AudioMeters bus-meter chain); (3) strip-mute
  ballistic decay over ~20 ms (no snap-to-floor); (4) MT-on (default) vs
  MT-off identical metering + audio behavior; (5) save + reload project --
  every insert routes correctly post-load (XML save/restore exercised through
  new kindString / kindFromString helpers in addInsertMap / restoreInsert);
  (6) EffectRack slot meters animate correctly during playback; (7) add /
  remove a strip during playback -- no audio glitch, no missing meter, no
  orphan post-remove (exercises new mLiveInsertChannels push / erase paths);
  (8) audition gestures fire immediate audible playback through corrected
  lookup path.  All 8 PASS.  No Task 3 source commit per plan (verify-only
  task per L11/Sub-F).

- **Task 4 cleanup + grep cleanliness sweep landed at commit `68050a8`
  (2026-05-25).**  2 source files (`Source/VibeGraph.h` + `Source/PluginProcessor.cpp`),
  +7/-3, comment-only (no semantic change, no re-build needed).  Two stale
  `m<Kind>Inserts` references rewritten: VibeGraph.h:719 -- pre-edit "DEPRECATED
  5F-4a: ... Batch 3 removes these and routes to mLayerInserts/mBassInserts"
  rewritten to reflect post-batch reality (legacy fallback for getLayerPageRack
  / getBassPageRack); PluginProcessor.cpp:4370 -- pre-edit "audio thread will
  see empty mRustyInserts immediately" rewritten to "audio thread will see the
  Rusty chId range emptied in mInsertsByChannel".  Grep cleanliness post-edits:
  zero surviving live `m<Kind>Inserts` references (only the explanatory
  comments I added at Task 2 remain at VibeGraph.h:759 + the 6 intentional
  `selectInsertMap` historical-reference comments).

- **Carry-forward to Task 5 close-entry routing (pre-existing warning
  surfaced during Task 2 build):**  `VibeGraph.cpp:1595` -- `const int n =
  buf.getNumSamples();` declared but unused (C4189 warning) inside
  `VibeGraph::processBus`.  Pre-existing code; processBus was NOT touched in
  this batch.  Defer to close-entry routing matching QA-AudioMeters' C4505
  bufferPeakDb NIT deferral pattern (pre-existing warnings not surfaced BY my
  changes get deferred to close-entry routing, not in-batch).

- **Rule 4 Diagnostic Instrumentation Catalog:** nil for Tasks 2 / 3 / 4 (no
  DBG / Logger / temp jassert / debug AlertWindow added during structural
  rewrite, verify, or cleanup).

- **Process lesson captured pre-close (extends
  `feedback_draft_doc_running_notes_every_checkpoint.md`):**  per-task
  `/draft-doc running-notes` appends were SKIPPED for Tasks 2 / 3 / 4 during
  live execution -- I rolled straight from commit to next task without
  appending.  Caught at Task 5 open when reviewing the running-notes file
  for `/draft-doc batch-close` input; consolidated catch-up appended here
  pre-close (one consolidated entry covering all 3 tasks).  Trigger to
  prevent recurrence: every commit landing closes the task's `[ ]` checkbox
  in the plan AND fires the running-notes append BEFORE moving to the next
  task; if a task has no source commit (e.g. Task 3 verify-only), the append
  still fires at PASS-confirmation.  No process change needed at the plan-
  file level (the plan's "Dispatch `/draft-doc running-notes` and apply" step
  is already in every task's checkbox list) -- the gap was execution
  discipline, not plan-text.  Surfaced for inclusion in the close entry's
  "Process notes" section.

- **Next action.** Task 5 close sequence: dispatch `/draft-doc batch-close` +
  `/review-batch QA-InsertMaps` in parallel; apply close-entry draft to
  Implemented Work Log via Edit; address `/review-batch` findings (fix-up
  commit if any BLOCKERs / NEEDS-FIX, defer NITs to routing table); update
  Main Plan §5 QA-InsertMaps STATUS banner with `/review-batch` outcome +
  close-commit SHA; `/draft-commit` for the close commit; commit on Jeff's
  approval.

---

## 2026-05-25 — Task 5 — Close (close commit pre-application)

- **`/draft-doc batch-close` produced** the consolidated Work Log entry with
  the full Task-by-Task narrative + FND-1 through FND-9 + their routing
  table + Carry-Forward §1 contradiction note + per-task Files-touched +
  Commit ladder + Next action pointing at QA-VoicePool.  Drafter brief
  injected all the in-conversation context that wasn't yet in the running
  notes (Tasks 2 / 3 / 4 execution + spec-call resolutions + build cycle
  fix); the catch-up running-notes entry just landed above also provided
  the same context for any future-session re-read.

- **`/review-batch QA-InsertMaps` close-pass returned** **0 BLOCKER + 0
  NEEDS-FIX + 5 NITs**.  Initial disposition surfaced as "READY-TO-COMMIT
  with all 5 NITs deferred to close-entry routing table" mirroring the
  QA-AudioMeters Task 5 fix-up precedent at `2cba7b7`:
  1. NIT 1 -- `rebuildRoutingFromApvts` reserve(13+) off-by-one + comment
     miscount at [VibeGraph.cpp:2938](Source/VibeGraph.cpp:2938) -- MINE-
     introduced at Task 2 commit `eb718bf` (says "reserve 13 buses" but
     actual emplace count is 12; Master IS one of the 12 buses, not
     separate).  Mirrors the "12 vs 13" miscount QA-Eg / QA-AudioMeters
     carried in their commit messages.  Functionally harmless (over-
     reserving is fine).  Logged as FND-10 in the close-entry routing
     table.
  2. NIT 2 -- `kindFromString` Layer-fallback for unknown XML kind labels
     at [VibeGraph.cpp:2173](Source/VibeGraph.cpp:2173) -- latent forward-
     compat hazard.  Pre-batch per-kind map approach also funneled unknown
     labels through `it == m.end()` skip; my Task 2 Layer-fallback **introduced**
     a subtle regression that could corrupt Layer state for future-added
     kinds whose XML labels land in saved projects (caught at my own NIT
     walk-through, not in `/review-batch`'s initial report).
  3. NIT 3 -- `removeInsertNode` O(n) `std::find` on `mLiveInsertChannels`
     at [VibeGraph.cpp:2371-2373](Source/VibeGraph.cpp:2371) -- DESIGN-
     LOCKED at L8 / Sub-C (single companion list with per-iter `node->kind`
     dispatch).  Message-thread only, rare event.
  4. NIT 4 -- Pre-existing C4189 at [VibeGraph.cpp:1595](Source/VibeGraph.cpp:1595)
     (`const int n = buf.getNumSamples()` unused inside `VibeGraph::processBus`)
     -- PRE-EXISTING pre-batch confirmed via `git blame` to commit `cc011e0e`
     2026-05-06; `processBus` was NOT touched in this batch.  Matches
     QA-AudioMeters' C4505 `bufferPeakDb` NIT deferral pattern.
  5. NIT 5 -- Stale `kInstBase` comment at [VibeGraph.h:60](Source/VibeGraph.h:60)
     -- pre-existing pre-QA-InsertMaps (stale doc from R1 era never updated
     when G-6 bumped `kMaxInstStrips` 6 → 10 → 20).  Out of QA-InsertMaps
     scope; route to Future State or a future sweep batch.

- **Initial NIT-deferral disposition overruled mid-close-sequence.**  Jeff
  caught the bulk-defer anti-pattern: "That's not canonical as things get
  routed at the end not placed on some table and ignored, the hell are you
  doing?"  Same anti-pattern call from QA-D's "QA is to find the bugs and
  take care of it, not suggest maybe in the future fixing the bug would be
  a cool idea" (`feedback_qa_batches_fix_bugs_dont_defer.md`) now extends
  to close-pass NITs.  The QA-AudioMeters Task 5 fix-up precedent at
  `2cba7b7` (which deferred 5 NITs at fix-up time) was the precedent I
  followed; that precedent is now overruled.  Initial follow-up routing
  was "Fix 1+2+5 / Clean up 4 / explain 3 for decision"; refined to "Lets
  do all of them then" after I walked through the NIT-3 forward-compat
  hazard.

- **Task 5 fix-up commit `e9fe545`** (2 files, +33/-22).  Source touches:
  - **NIT 1** ([Source/VibeGraph.cpp:1595](Source/VibeGraph.cpp:1595)):
    deleted unused `const int n = buf.getNumSamples();` declaration from
    `processBus` body (the body uses `buf` directly via `preEq->process` /
    `rack->process` / `applyXxxPolarityWidth` / `applyStereoPan` /
    `publishPeakReading` and never `n`).
  - **NIT 2** ([Source/VibeGraph.cpp:2938](Source/VibeGraph.cpp:2938) in
    `rebuildRoutingFromApvts`): `reserve(13 + mLiveInsertChannels.size())`
    corrected to `reserve(12 + mLiveInsertChannels.size())` + adjacent
    comment rewritten "12 buses (Master IS one of the 12, not separate)"
    + bus ladder added for future readers.
  - **NIT 3** ([Source/VibeGraph.cpp:2173](Source/VibeGraph.cpp:2173) at
    `kindFromString` + `restoreInsert`): return type changed to
    `std::optional<InsertKind>`; 8 known kinds return `InsertKind::<Kind>`;
    unknown labels return `std::nullopt`.  `restoreInsert` checks the
    optional and skips on `nullopt` (matching the pre-batch per-kind map's
    `it == m.end()` skip semantics).  Header `#include <optional>` added
    to `VibeGraph.cpp`.  Restores the pre-batch skip semantics + adds
    explicit nullopt-handling at the call site.
  - **NIT 5** ([Source/VibeGraph.h:60](Source/VibeGraph.h:60)): comment
    rewritten from `// R1: Inst insert 0..5 → 700..705` to `// R1: Inst
    insert 0..19 → 700..719 (kMaxInstStrips bumped 6→10 in G-4 2026-04-28,
    10→20 in G-6 2026-04-29)` -- captures both the current range AND the
    rename history.
  - **2 pre-existing C4505 dead-helper deletions** surfaced during the
    full-warning rebuild post-NIT-1 fix:
    - `calcBusGain` (3-arg fader × mute × solo bus gain helper) -- orphaned
      by QA-Ea Part A's bus-solo cached-atomic restructure (the gain
      computation moved inline into bus-process hot paths so the helper
      has zero callers).
    - `bufferPeakDb` mono variant (single-channel peak-dB reader) --
      orphaned by QA-Eg / QA-AudioMeters' G1 stereo publish chain (the
      stereo `bufferPeakDbStereo` variant subsumed the mono helper at
      every call site).
  - **NIT 4 acknowledged as L8/Sub-C accepted design** (not a deferral):
    Jeff's pick at Task 0 ExitPlanMode (single companion list with per-
    iter `node->kind` dispatch); rare-event message-thread surface;
    `std::unordered_set<int>` companion is a future-cleanup option if
    profiling ever surfaces a hot site.  Reframed in the fix-up commit
    message as accepted design, not "deferred".

- **Fix-up build cycle gotcha -- MSBuild stale-`.obj` cache.**  After
  deleting `calcBusGain` + `bufferPeakDb` (mono) from source, C4505
  warnings persisted in the subsequent build despite source verified clean
  via grep + `git diff` + `awk`.  `.obj` modification times were post-
  source, suggesting MSBuild's incremental build cache returned stale
  compiler output.  Fixed by force-deleting
  `build/BaySickDAWStandalone.dir/{Release,Debug}/VibeGraph.obj` and
  forcing fresh recompile; subsequent build clean (only `VibeGraph.cpp`
  recompiled, no C4505 noise).  Build-system gotcha worth remembering for
  future "deleted but warning still appears" debug cycles: trust the
  source, suspect the cache.

- **0 NITs deferred at close.**  4 NITs fixed in fix-up `e9fe545` (NIT
  1+2+3+5) + 1 NIT acknowledged as L8/Sub-C accepted design (NIT 4) + 2
  pre-existing C4505 dead-helper deletions in the same fix-up.

- **Close commit plan:** lands the Work Log entry + Main Plan §5 STATUS
  banner + this Task 5 running-notes close-pass section.  No source touches
  in the close commit itself (the fix-up at `e9fe545` already landed all
  source fixes; close commit is doc-only).  Per `feedback_no_full_release_reverify_at_batch_close.md`
  NO separate Release-re-verify gate at close (Jeff's per-task verify cycle
  already covered Debug + Release at Task 2 commit `eb718bf` + the fix-up
  at `e9fe545`).

- **Carry-Forward Reference §1 contradiction recorded in the Work Log close
  entry** (per `feedback_closed_batch_carryforward_via_forks.md` Carry-
  Forward Reference is FROZEN; contradictions go in the append-only Work Log,
  not in Carry-Forward itself).  Specifically: per-insert std::map architecture
  (8 `mLayerInserts` ... `mRustyInserts` member decls keyed by per-kind index
  + `selectInsertMap` switch dispatcher) replaced with flat-array-by-ChannelId
  (single `mInsertsByChannel` array + `mLiveInsertChannels` companion list).

- **Rule 4 Diagnostic Instrumentation Catalog:** nil for Task 5 (close paper-
  work only; no DBG / Logger / temp jassert / debug AlertWindow added during
  the close sequence).

- **Next action.** QA-VoicePool open per §6 sequencing arrow (third of the
  QA-Eg close-spawned perf-audit cluster: **QA-VoicePool → QA-EngineApvts**
  before resuming bug-fix sequencing at QA-Ed).  Risk medium-high, effort
  ~8-12 hours per §5 / §9 thirty-fourth Forks entry; Jeff's verbatim 4-section
  blueprint (pre-allocate in prepareToPlay + fat voices + lock-free atomic
  occupancy + voice stealing) carried into the §5 entry.
