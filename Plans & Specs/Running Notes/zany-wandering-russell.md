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
