# Running Notes — QA-ModelShell (grand-inverting-mammoth)

> Append-only mid-batch log. Entries land at every checkpoint (commit landed / finding captured
> / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At code-complete, `/draft-doc batch-close` consumes this file to compile the Implemented Work
> Log entry, which is HELD here (bulk-run R2) and applied at the batch's campaign section pass.
>
> Pair file: [`Plans & Specs/Batch Plans/grand-inverting-mammoth.md`](../Batch%20Plans/grand-inverting-mammoth.md).
> Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11).

## 2026-07-27 — Batch open — session start at `b933b54a`, backfills applied

- QA-ProjectSave (deep-packing-badger) confirmed closed at `b933b54a`, tree clean. G4 run
  order: badger -> **mammoth (this)** -> yak -> stoat -> heron.
- Backfilled `b933b54a` into §B.30's `blocks:` line (test plan) and the badger held Work Log
  entry's two `<final commit hash>` placeholders. These doc edits ride this batch's first
  commit (TS1) per the session-open directive.
- Batch shape (locked at approval, 2026-07-27): 8 task sets; commit + compile gate at the end
  of EVERY set; ALL functional verification deferred to the single TS8 batch smoke. Five
  sub-spec calls open, each resolved at its owning set's start (TS1 batch-ID rename offer,
  TS2 stems granularity, TS4 window floors on screen with Jeff, TS6 crash-protection process
  model, TS7 freeze tap point + presentation).
- Next: Task set 1 (the inversion) — opening scout pass across the six page types + Rusty,
  the register*Engine family, and page-side wiring.

## 2026-07-27 — TS1 — scout pass (~90%) + batch-ID sub-spec call posed

- **register*/unregister* family mapped** ([PluginProcessor.cpp:5452-5862](../../Source/PluginProcessor.cpp:5452)):
  Layer/Bass/Drum registration creates mixer-strip params + InsertNode + EngineInsertTask
  (InsertNode deliberately retained on unregister); Drum additionally publishes the playNote
  pointer + recomputes the fast-path flag; Clip sets the per-row Composite's clip-engine
  pointer (QA-0 Strategy 1a — no separate task); Vox/Inst register dispatch-only tasks
  (their strips/InsertNodes come from MixerPage's add-strip paths, not registration).
- **Page-side lifecycle mapped:** LayersPage/BassPage/DrumPage construct engines inside
  `selectEngine` (raw `new` + `prepareToPlay(sr, 512)` + `createEditor`), register with the
  processor, and their DTORS unregister + `Thread::sleep(20)` before destroying the engine.
  DrumPage's picker is swap-aware (teardown-then-recreate). ClipsPage lazy-creates a
  VibePlayer with `clip_<N>_` prefix; VoxPage create-once BaySickVocal (transport-beat/seek
  + song-time-sel hooks re-installed on rebuild); InstPage ctor unconditionally builds
  NAM + Pedals + EngineChainProcessor — the CHAIN is the registered engine pointer.
- **The model precedent is already in-tree:** the sfizz trio is PROCESSOR-OWNED
  (`mGuitarsEngine`/`mBassesEngine` per-instance arrays + `mRustyDrumsEngine` singleton)
  with the active-flag dance, per-slot SpinLocks, setProcessingEnabled gate, and the Rusty
  load shield ([PluginProcessor.cpp:6339-6533](../../Source/PluginProcessor.cpp:6339)).
  The inversion generalizes THIS pattern to the remaining engines.
- **UndoManager wrinkle for the dormant pre-wire (task 8):** main processor + 7 engines
  pass `nullptr` to their APVTS ctor; the sfizz trio passes a PRIVATE `&mUndoManager`
  (Guitars :22 / Rusty :18). The pre-wire must not silently repoint those — that would be
  a semantics change, which the conflict call forbids.
- **Automation registry home survives the shell:** applicators/readers + owner tracking
  live on StandaloneEditor ([StandaloneEditor.h:774-818](../../Source/Standalone/StandaloneEditor.h:774)),
  and StandaloneEditor itself is NOT destroyed by destroy-on-close (only pages die).
  Model-side registration can keep the registry where it is and re-key closures to resolve
  through the model (null-owner, the rack pattern badger proved).
- **Wiring census per tab spawn:** ~7 callbacks per page (onEngineSelected /
  onDeleteRequested / onDuplicateRequested / onLockChanged / onRenameRequested /
  onSoundNameChanged + the piano-roll connection) duplicated across the initial / add /
  duplicate / restore paths — the collapse target for a single bind-view function.
  Piano-roll connections ([registerLayerPianoRoll :6451](../../Source/Standalone/StandaloneEditor.cpp:6451))
  capture PAGE pointers but resolve engines per-call — re-point at model tabs.
- **Capacity constants:** kMaxLayerPages 8 / Bass 4 / Drum 16 / Clip 50 / Vox 6 / Inst 20 /
  kMaxAudioRows 50 ([VibesynthConstants.h:5-18](../../Source/VibesynthConstants.h:5)).
- **CL-301 target confirmed:** the 5 structs live at [VibeGraph.cpp](../../Source/VibeGraph.cpp)
  :239 (Layers) / :449 (Bass) / :622 (Drums) / :793 (Master) / :947 (Effects);
  InstrChannelNode already serves Clips/Vox/Inst/Vox2/Inst2/Inst3/Rusty + the per-insert
  map. Special-case content survives via flags per the Future State entry.
- **Pending-ledger ACCRUAL (apply at G4 close):** Main Plan :6373 (sixty-fourth Forks
  entry) still says "QA-DirtyFlag's commit closes G4 code" — stale since mammoth + heron
  slotted after stoat; heron closes G4 code now. Surfaced by /standup, source-verified.
- **Persistence call graph mapped** (agent-enumerated, spot-verified in source: the
  `applyEngineState` lambda + its 6 call sites, `reloadForProjectRestore`, and the
  ordering comment all check out):
  - ONE encoder — the `encodeEngineState` lambda inside `serializeTabsInto`
    ([StandaloneEditor.cpp:11113-11242](../../Source/Standalone/StandaloneEditor.cpp:11113));
    ONE decoder — the `applyEngineState` lambda inside `deserializeUIState`
    ([:11647-11653](../../Source/Standalone/StandaloneEditor.cpp:11647), call sites
    :11715 Layers / :11763 Bass / :11854 Clips / :11886 Vox / :12040 Inst-live / :12093 Drum).
  - Restore order per L/B/D branch: create page -> wire callbacks -> `selectEngine`
    (CREATES the engine) -> `applyEngineState` (setStateInformation) -> mPages insert.
    Clips: strip+insert FIRST (`createClipStripAndPage`), then page, then blob (VibePlayer's
    own setStateInformation re-resolves `bsp_loadPath`, so the blob wins over the ctor load).
    Vox: spawn -> K-6 idempotent strip net -> selectEngine -> blob.
  - Inst is TWO sub-paths: chain XML (`importInstState` -> NAM + Pedals setStateInformation)
    applies FIRST for both; sfizz sources then do setSource -> kit load via the processor
    wrappers -> `apvts.replaceState` (NOT setStateInformation) + force-fire params ->
    program cache -> piano-roll register -> `notifySourceEngineChanged()` LAST (splices the
    engine into the chain — audio never flows without it).
  - Rusty: tab -> decode kit path -> `reloadForProjectRestore(kitFile)` (page-level
    race-safe load; spawns the 13 strips via onKitLoaded) -> `apvts.replaceState` overlay.
    Direct setStateInformation on the engine is the documented crash path (:12122-12124).
  - Global ordering invariant (project open): `applyProcessorState` (:5156) ->
    PatternManager -> `deserializeUIState` (:5187 — engines inherit APVTS-driven defaults,
    comment :5184-5185) -> `restoreAudioStripsFromArrangement` -> `applyPendingRackStates`
    (:12477). Model-side engine creation must stay AFTER applyProcessorState and BEFORE
    the rack replay.
  - Project load does NOT use PagePresetIO — the race-safe sfizz discipline exists as two
    independent inline copies (Inst :11970-12021, Rusty :12125-12172) that
    [PagePresetIO.cpp:103-205](../../Source/Standalone/PagePresetIO.cpp:103) already
    generalizes. Convergence target for the model factory.
  - Inst's `engineData` attribute is empty in practice (the chain wrapper has no state,
    comment :11187-11191) — the real state is `instChainState` + `sfizzEngineData`.
- **STOPPED** at the TS1 batch-ID sub-spec call (posed to Jeff in chat).

## 2026-07-27 — TS1 — batch-ID call resolved (a) + process correction

- Jeff: **(a) — QA-ModelShell stays.** With a correction that retires the question class
  itself: "how is this even a question being asked you do the plan not ask me hey wanna
  rename the plan." Naming plan artifacts — silly-names AND batch IDs — is my job, never
  a docket item. Memory updated (`feedback_silly_name_is_my_pick.md` extended); the plan's
  sub-spec list item 5 struck with the same note. The four remaining sub-spec calls
  (TS2 stems / TS4 floors / TS6 process model / TS7 freeze) are real design forks and
  stay open at their owning sets.
- Proceeding into task 2: model tab registry + engine factory (EngineRig).

## 2026-07-27 — TS1 — EngineRig landed + L/B/D pages flipped to views (2 green builds)

- **New `Source/EngineRig.h/.cpp`** — the model-side owner of dynamic-tab identity +
  engines, keyed `(TabKind, pageIndex)` over Layers/Bass/Drums/Clips/Vox/Inst (sfizz trio
  deliberately stays on its existing processor-owned paths). Tab = `{kind, pageIndex, name,
  engineType, engine unique_ptr, ownedStages}` — the Inst chain trio rides `ownedStages` +
  typed views, so a future hosted VST3i is one more factory case (the generic-slot shape).
  API: addTab / removeTab / clearEngine / setEngineType (swap-aware, DrumPage guard
  generalized) / restoreEngineFromBlob / engineFor / teardownAll + onEngineCreated /
  onEngineDestroying model events. Teardown keeps the page-era discipline verbatim:
  unregister-first, settle 20 ms on full teardown, NO settle on swap/clear; chain always
  destroyed before its stages. Creation-prep values preserved per path (L/B/D/Clips
  rate-or-44100 + 512; Vox/Inst 44100/512 then live-config re-prep before registration —
  the old onEngineChanged flow).
- **Dormant UndoManager pre-wire COMPLETE (task 8, conflict call 2=b):** processor-owned
  `mUndoManager` declared BEFORE apvts and bound into it; all 7 page-family engine APVTS
  ctors gained `juce::UndoManager* undoMgr = nullptr` (Harmless / BaySickSynth /
  BaySickBass / VibePlayer / BaySickVocal — which forwards to its embedded NAM/IR — /
  BaySickPedals / BaySickNAMIR); the rig's factory passes the processor's manager. The
  sfizz trio's PRIVATE UndoManagers left untouched (repointing them = semantics change).
  Nothing consumes the manager — QA-UndoCoverage flips semantics (its Task 2 = verify).
- **`mEngineRig` declared LAST in VibeSynthProcessor** so it destructs FIRST — engines
  unregister through a still-alive dispatcher + task arrays. In practice the editor dtor's
  closeAllDynamicTabs -> onTabClosed -> rig.removeTab tears everything down earlier;
  teardownAll is the backstop.
- **LayersPage / BassPage / DrumPage flipped to views:** `mEngineProcessor` is a non-owning
  raw pointer; selectEngine delegates construct+register to the rig and builds only the
  editor; dtors touch ONLY the editor (engines survive view death — the destroy-on-close
  prerequisite); DrumPage::clearSound uses the new rig.clearEngine (engine gone, tab
  identity kept). Page-side register*/unregister* calls: ZERO remain in Standalone/.
- **onTabClosed tail:** rig.removeTab for all six kinds added AFTER `mPages.remove(i)`,
  joining the sfizz destroys at the same page-first-engine-second ordering point. No-ops
  for kinds not yet flipped (Clips/Vox/Inst) and for engineless tabs. Teardown paths
  audited: closeDynamicTabs routes per-tab through onTabClosed; the editor dtor calls
  closeAllDynamicTabs BEFORE mPages.clear() — no path destroys a dynamic page without the
  rig hearing about it.
- **Two intermediate builds green (both configs)** at rig-landing and at the L/B/D flip.
  These are hygiene compiles; the task-set GATE runs at TS1 close per the batch plan.
- Pre-existing observation flagged for TS8 smoke, NOT touched: Layers/Bass
  `loadPagePreset` calls `selectEngine` for a type-switching preset, but `selectEngine` is
  lock-guarded (`if (mEngineLocked) return`) — a locked page + different-engine page preset
  may apply state to the wrong engine type. Behavior preserved exactly through the flip.

## 2026-07-27 — TS1 — Clips/Vox/Inst flipped + tab identity + automation hooks (builds green)

- **Clips/Vox/Inst flipped to views (task 3 DONE — the vox/inst export prerequisite):**
  ClipsPage lazy-creates through the rig (`clip_<N>_` prefix preserved via trackIdFor;
  sample load + editor stay view-side); VoxPage + InstPage ctors gained a
  `VibeSynthProcessor&` param (sibling convention — both create engines at construction);
  the Inst trio (Pedals + NAM + EngineChainProcessor) is rig-owned with typed views bound
  from the tab record. The spawn handlers' register/unregister/prep bodies are GONE —
  onEngineChanged wiring is dirty-hook-only now (Clips/Vox), Inst's deleted outright with
  its explicit post-add fire. Clips joined the Vox/Inst live-config re-prep branch in the
  rig (the old handler re-prepped at real device block size — preserved).
- **Ordering hazard CAUGHT + fixed before it ever ran:** my first onTabClosed tail put
  rig.removeTab AFTER destroyBaySickGuitars/Basses — but the rig-owned Inst chain still
  holds the spliced sfizz stage pointer, and the chain's processBlock calls that engine
  directly WITHOUT checking the active flags (the documented K-5 fix-#5 gap). One audio
  block in that window = use-after-free. Reordered: rig.removeTab (unregisters the strip
  task + destroys the chain + settles) runs BEFORE the sfizz engine destroys. Verified
  destroyBaySickGuitars/Basses have exactly ONE call site (that tail), so the hazard class
  is closed.
- **Task 4 DONE — tab identity is model-side:** L/B/D page ctors + ClipsPage::setProcessor
  create the rig tab at birth (idempotent); every page's setTabName syncs rig.renameTab
  (the one funnel all rename paths — ribbon commit, patch load, restore — already flow
  through); onTabClosed's rig.removeTab covers close (incl. closeDynamicTabs + editor-dtor
  paths); DrumPage::clearSound uses rig.clearEngine. Save/load/templates/PagePresetIO
  operate on rig-owned engines through the existing page indirection (capture format
  untouched). Index allocation + name counters stay editor-side as implementation detail —
  the rig's allocateFreeIndex exists for TS4's tab-bar model to consume.
- **Task 5 DONE (TS1 scope) — automation registration keys to MODEL events:**
  (1) `EngineRig::onEngineCreated` -> new `StandaloneEditor::registerModelEngineAutomation`
  — walks the engine APVTS and registers null-owner, param-targeting applicators/readers
  that RE-RESOLVE tab->engine through the rig at apply time. Lane vocabulary matches the
  editors' stamps: L/B/D/Clips = engine param ids verbatim; Vox = "vox<N>_" + id across
  the vocal AND its embedded NAM/IR; Inst = "inst<N>_" + id for the NAM stage. Pedal
  uuid-lanes + the vocal capture-lock veto ride TS3 (wrappers still overwrite these ids
  while views exist — identical targets, so no behavior change today).
  (2) Wire-at-load: `EffectsPage::registerSlotAutomation` split — view method delegates to
  a static `registerSlotAutomationFor(vg, chId, prefix, rack, slot)` (same closures,
  registration-time view state now parameters), `getChannelPrefix` extracted to a static
  `channelPrefixForId` (1-based layer/bass quirk preserved verbatim), and a new
  `registerRackAutomationForAllChannels` sweeps every channel id (buses 1-12 / drums 100+ /
  layers 200+ / basses 300+ / audio 400+ / aux 600+ / vox 700+ / inst 800+ / rusty 900+)
  right after `applyPendingRackStates` in restoreAudioStripsFromArrangement — one of the
  two census registration-timing gaps closed at its model trigger.
  (3) New `EngineRig::apvtsOf` — the type-agnostic APVTS resolver, model-side ON PURPOSE:
  TS2's offline lane replay resolves engine lanes through the same seam, UI-free.
- **Generic-slot shape (task 7) satisfied by construction:** a tab's registered engine is
  `unique_ptr<juce::AudioProcessor>` + `ownedStages` for support processors — a hosted
  VST3 instrument is one more factory case; nothing keys on concrete engine types outside
  the factory + apvtsOf.
- Builds green (both configs) at the six-type flip, at the tab-identity chunk, AND at the
  automation-hooks chunk (verified: RELEASE_EXIT_CODE=0 / DEBUG_EXIT_CODE=0, zero error
  lines).
- **Remaining in TS1:** CL-301 bus-node consolidation (task 6) + task-7 shape note in the
  plan + the TS1 gate/commit (task 10). TS3 unchanged: wrapper retirement, pedal tables,
  mixer/EQ lanes, statics re-widen.

## 2026-07-27 — TS1 — CL-301 executed: five bus-node structs folded into InstrChannelNode

- **The fold:** LayersBusNode / BassBusNode / DrumsBusNode / MasterBusNode / EffectsBusNode
  (~860 lines, [VibeGraph.cpp](../../Source/VibeGraph.cpp)) DELETED; `InstrChannelNode` is
  the ONE bus/channel type for all 11 buses. It grew the full cached-pointer set
  (polarity/width/bypass/pan/panLaw/level/mute/solo/globalFxBypass/masterGain), the
  L/B/D-pattern `processChainOnly` (preEq -> rack bypass -> rack -> eq -> fader x mute x
  unified solo -> polarity+width -> pan -> SC stash -> compDelay -> peak publish), a
  by-value LUFS meter, and a separate `processMasterChain` (terminal: masterGain x fader,
  mute only, pan BEFORE width kept verbatim, no polarity / compDelay / SC stash, LUFS) —
  special cases live as members/methods, not types.
- **processBus collapsed:** the generic 7's inline chain (per-block STRING-KEYED APVTS
  lookups for bypass/level/mute/solo/pan — a standing CPU-rule violation) and the
  applyXxxBusPolarityWidth helper switch are gone; every non-master bus runs
  processChainOnly with pointers cached in rebindBusApvts (which already bound all 12
  prefixes — the unified rebind just binds the full set for everyone). FX bus folded in
  too: `processEffectsBus` DELETED (single caller was processBus itself; its SC-array push
  is kept in the new kFxBus branch — caught mid-fold when the real body surfaced). The 7
  applyXxx wrappers + node applyPolarityWidth deleted (zero external callers, verified).
- **Dead code out with the types:** the L/B/D synth-render fallback `processBlock`s
  (dead since QA-Ea Part A; zero callers verified) and their Synthesiser&/BassSynth&/
  BusMix& refs; the pSiblingBass/pSiblingLayers/pSiblingDrum dead pointers (QA-Ea);
  buildFixedTopology keeps its (synth, bass, apvts) signature for the PluginProcessor
  caller with ignoreUnused.
- **FOURTH divergence incident found + closed at fold time** (Future State's entry lists
  three): the generic 7 never called `rack.setHostBPM(bpm)` — tempo-synced rack effects
  (Delay sync etc.) on Clips/Vox/Inst/Vox2/Inst2/Inst3/Rusty buses ran at DEFAULT BPM.
  The unified chain gives every bus the same call. Deliberate divergence-closure, not an
  unprompted change — this hazard class is exactly what CL-301 exists to kill; flagged
  for the TS8 smoke ear-pass.
- **Behavior-parity audit of the unification (recorded):** fader-vs-polarity/width order
  differed between the two shapes — commutative (all linear ops), unified on the L/B/D
  order. applyStereoPan self-guards zero pan (matches the old generic guard). panLaw:
  the old generic/FX paths took the caller's arg (BlockContext snapshot of
  master_pan_law); the chain reads the cached pointer to the SAME param — L/B/D already
  did this. masterGain: was a per-block string lookup in MasterBusNode; now cached
  (value-identical, rule-compliant). busEq member renamed to the surviving `eq` (~15
  sites). Non-master buses each carry an idle LufsMeterDSP (prepare-time memory only;
  process() never called off-master).
- Stale comments naming the dead types/functions fixed tree-wide (PluginProcessor x3,
  PassiveStripTask.h, VibeGraph.h topology diagram + 3 more).
- **Task 7 confirmed by construction:** the generic engine slot is VST3i-ready — a tab's
  registered engine is a base-class unique_ptr + ownedStages for support processors; the
  factory and apvtsOf are the only type-aware points. TS6 adds one factory case.
