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

## 2026-07-27 — TS1 COMMITTED `4ea67bd0` (Jeff-approved); TS2 opens with the stems call

- **TS1 commit landed:** `4ea67bd0`, 42 files (+1654/-1580), tree clean after. Gate was
  green both configs on the final (post-CL-301) build. All ten TS1 punch-list items
  closed. The `b933b54a` backfills (test plan §B.30 + badger held entry) rode this commit
  as directed at session open.
- **TS2 (the export engine) OPEN.** Its owning sub-spec call posed to Jeff per the plan:
  stems (CL-040) granularity — per tab / per bus / pick-list dialog. Non-stems TS2 work
  (offline drive, full re-prepare, tempo-lane offline clock, UI-free lane replay, clip
  streaming offline mode, metronome gate, Exports destination, dialog UX, dither, LUFS
  normalization, offline block size, CL-057 hot-swap, CL-227 backend) does not depend on
  the answer and proceeds while it is pending.

## 2026-07-27 — TS2 — stems call RESOLVED: per mixer strip + active-strip pick-list

- **Jeff's spec (refining option a):** stems are **per MIXER STRIP**, the FL model — so
  sends/aux strips render as their own stems, and stems INCLUDE sidechain-driven content
  (his example: a bass strip's compressor keyed off the kick still ducks in the bass
  stem). UI = **pick-list of all ACTIVE mixer strips**; defaults: Master + buses
  UNCHECKED, everything else CHECKED. Destination confirmed: stem files land in
  `<project>\Exports\` like every other export.
- **Architecture consequence (recorded before building):** per-strip-with-sidechain
  forces the ONE-PASS design — render the full graph offline once and tap every ticked
  strip's post-chain output into its own writer per block. The alternative (N passes,
  muting everything but one strip) would silence the kick feeding the bass compressor's
  key and strip the ducking out of the stem — precisely what Jeff's example rules out.
  One pass is also N-fold cheaper. Tap points: each strip's node output at the same
  stage its meter publish runs (post full chain), before downstream summing.

## 2026-07-27 — TS2 — scout complete: the render harness mapped; build skeleton locked

- **What exists ([BuilderPage.cpp:7944-8217](../../Source/Standalone/BuilderPage.cpp:7944)):**
  `OfflineHead` resolves beats through SECONDS against the tempo MAP (lane-blind — TS2
  layers the `global_tempo` automation lane on top via ONE shared beats<->seconds
  resolver for live + offline); scope span math (Song = getSongEndBeats shared with
  transport; Section; Pattern); the REPLICA `VibeSynthProcessor renderProc` at :8064
  (state-cloned, rebuildAudioClipPlayers for clips, no engines — the silent-instrument
  root cause; DIES in TS2); writer stack (WAV/OGG via juce formats, MP3 via Mp3Writer);
  block loop with tail-decay detection (-100 dBFS held 0.25 s, 60 s ceiling) + abort +
  partial-file cleanup; `runExportWithProgress` wraps it in ThreadWithProgressWindow
  (replaced by the FL-style in-dialog progress per the locked UX call). CONFIRMED bug the
  restore-set fixes: Pattern scope does `mPM.setCurrentPattern(opts.patternIndex)` at
  :8079 with NO restore — live session left on the exported pattern.
- **TS2 build skeleton (the order of construction):**
  1. Offline drive on the LIVE processor: suspend device -> setNonRealtime(true) sweep
     (processor + graph + every rig engine + sfizz trio) -> capture restore set
     (transport pos, song/pattern mode, current pattern, live tempo, offline flags) ->
     graph-wide reset -> full re-prepare at RENDER rate/block -> loop drives
     mProcessor.processBlock with OfflineHead -> restore + re-prepare back + device
     resume; editor automation timer stopped for the duration.
  2. Shared beats<->seconds resolver honoring map + tempo lane; OfflineHead consumes it.
  3. UI-free lane replay generalizing [PluginProcessor.cpp:2791](../../Source/PluginProcessor.cpp:2791):
     main-APVTS as today; engine lanes via per-instance prefix -> rig ->
     `EngineRig::apvtsOf` (the TS1 seam, built for exactly this); rack lanes via
     prefix+uuid+suffix -> EffectParamMap; `output_vol` -> rack slot gain;
     `_fader`/`_pan` -> strip params; `global_tempo` -> the clock. Render thread only.
  4. Stems one-pass taps: after each block's dispatch completes, copy every ticked
     strip's output (arena slot / node output at the meter-publish stage) into that
     stem's writer. Pick-list = active strips; Master + buses UNCHECKED by default.
  5. Clip streaming offline mode (synchronous reads under isNonRealtime) + CL-282
     underrun counter; metronome gated (post-master-tap, default-off — verify).
  6. Destination `<project>\Exports\` (both choosers, :10611 + :8274) + the in-dialog
     progress UX; riders (CL-043 dither, CL-045 LUFS measure-then-gain both directions
     true-peak-capped, CL-056 offline block size, CL-057 hot-swap reusing re-prepare,
     CL-227 backend = the loop with meters and no writer).
## 2026-07-27 — TS2 — offline drive LANDED: the live model renders itself (replica deleted)

- **Two never-swept re-prepare gaps closed in `VibeSynthProcessor::prepareToPlay`** —
  found because TS2's "render rate independent of device rate" depends on the sweep
  being complete: (1) DRUM engines were never re-prepared (the old comment claimed
  page-owners did it — true only at creation, false for every device rate change, and
  false, period, post-inversion); (2) the per-instance sfizz Guitars/Basses engines had
  the same gap (prepared only at kit load). Both swept now with their siblings'
  lock discipline. These were latent LIVE bugs too: a mid-session device rate change
  left drums + sfizz insts at the stale rate.
- **New processor API `beginOfflineRender(sr, blk)` / `endOfflineRender()`**
  ([PluginProcessor.cpp](../../Source/PluginProcessor.cpp), before the register*
  family): suspendProcessing(true) + 30 ms settle (the standalone player checks
  isSuspended, so the render loop becomes processBlock's ONLY caller — no shield games);
  restore-set capture (device sr/blk, song mode, playhead); setNonRealtime sweep across
  self + every rig engine + owned stages + the vocal's embedded NAM/IR + the sfizz trio
  (via new `EngineRig::forEachEngine`); `VibeGraph::reset()` for wet-tail hygiene —
  reset()'s FIRST callers ever (the 2026-05-24 dead-code note updated; transport-Stop
  wiring stays future); full prepareToPlay at the render config. end reverses it all and
  resets tails AGAIN so the render's own decay never bleeds into live playback.
- **`renderToFile` rewritten onto the live processor:** the replica
  `VibeSynthProcessor` (the silent-export root cause) is DELETED — zero `renderProc`
  references remain tree-wide. Writer setup moved BEFORE the drive so file failures
  never touch the processor; every exit path (success / abort / write-fail) runs
  endOfflineRender + the pattern-scope current-pattern restore (the mutation bug fixed)
  + the timer-resume hook. rebuildAudioClipPlayers dropped — the live processor already
  holds the editor-published clip snapshot (the call existed BECAUSE the replica had no
  editor).
- **Editor automation timer paused during renders** per the locked call: new
  `BuilderPage::onOfflineRenderActive(bool)` fires on the render thread; StandaloneEditor
  wires it with a message-thread marshal (Timer start/stop is message-thread-only).
  Worst case one 30 Hz tick lands before the async stop — benign today (it applies the
  same values the live timer always did) and overwritten per-block once step 3's offline
  lane replay lands.
- Chunk compile pending at checkpoint time. Next: step 2 (shared beats<->seconds
  resolver + lane-aware OfflineHead), then step 3 (the UI-free lane replay per the
  resolver rules below).

## 2026-07-27 — TS2 — steps 2+3 LANDED: lane-aware clock + UI-free offline lane replay

- **Step-1 chunk verified green (both configs)** before these landed.
- **Shared point evaluator:** the live replay's inline stepped/linear walk extracted to
  `evalAutomationPointsAt` in [PatternManager.h](../../Source/PatternManager.h) (next to
  ControlPoint); the live engine replay now calls it — live and offline literally cannot
  drift (the plan's shared-resolver requirement).
- **OfflineHead rebuilt as an INTEGRATING clock** (BuilderPage anon namespace): advances
  beats blockwise at the effective BPM — a "global_tempo" lane clip is a live override
  while it covers the position (the 30 Hz applicator's truncate-and-append semantics,
  20..300 BPM map preserved), the ruler tempo map rules elsewhere, base BPM last. Lane
  layer is SONG-scope only (matches the live song-mode gate; Pattern scope = map+base).
  Span math now walks the SAME clock (`advanceToBeat`, sub-block remainder corrected):
  a tempo lane changes real-time length, so the old closed-form map-only
  `beatsToSeconds` (deleted) would have mislabeled content spans. Section starts
  fast-forward the render head itself — same stepping as the span probe.
- **Offline lane replay** (`BuilderPage::applyOfflineAutomationAt` +
  `applyOfflineLaneValue`, called per block before processBlock): same block filters as
  live; main-APVTS lanes deliberately SKIPPED (they replay inside processBlock exactly
  as live); "global_tempo" skipped (the clock's job). Resolution order: Vox/Inst page
  lanes ("vox<N>_"/"inst<N>_" + bare id -> rig tab -> vocal + embedded NAM / NAM stage;
  digits-immediately-after-word disambiguates from "vox_bus"/"inst_0" rack prefixes by
  construction) -> engine full-id lanes (rig sweep via apvtsOf) -> legacy "_fader" ->
  "_level" translation -> rack lanes (channel by prefix via channelPrefixForId, slot by
  UUID never index, output_vol -> setSlotOutputGain -24..+12, everything else
  EffectParamMap::applyNorm keyed (type, variantOf)). Pedal uuid-lanes stay unresolved
  until TS3's pedal tables (planned). Steps 2+3 chunk compile RUNNING at checkpoint.
- Steps 2+3 chunk went green (both configs) after one trivial fix — `PatternManager::
  getBlock` is non-const, so the clock holds a non-const `PatternManager&`.

## 2026-07-27 — TS2 — stems + metronome gate + Exports destination LANDED

- **Stems (Jeff's per-strip spec) implemented in the render loop:**
  `RenderOptions::StemTarget {channelId, name}` list; renderToFile's writer stack
  generalized to a `FileSink` (WAV/OGG/MP3-uniform open/write/close) — one sink for the
  main mix + one per ticked strip, ALL fed by the single render pass. Per block, each
  stem copies its strip's ARENA slot (`VibeSynthProcessor::getStripOutputForTap(chId)` —
  the same slot the strip's render task wrote its post-chain output into,
  [ChannelBufferArena::getStripBuffer](../../Source/Engine/ChannelBufferArena.h)) straight
  into that stem's writer. Sends = separate stems and sidechain-driven content stays in
  the stem BY CONSTRUCTION (the kick still feeds the bass comp's key during the pass).
  Stem files: "<destBase> - <stripName>.<ext>" beside the main file, same format
  options; abort deletes ALL partial files. The strip PICK-LIST UI (active strips,
  Master+buses unchecked) rides the dialog-UX chunk next.
- **Metronome gated out of renders:** the click block in processBlock now runs only
  `if (! isNonRealtime())` — under the offline drive the post-master-tap click would
  have printed into the export (locked export-semantics call; record-master convention).
- **Exports destination:** new single-source `VibeSynthProcessor::getProjectExportsDir()`
  (creates `<project>\Exports\` on demand; Music-folder fallback until the save-first
  interlock lands with the dialog chunk); BOTH export choosers re-pointed (Export Audio
  dialog + the pattern-render right-click). Sample-picker userMusicDirectory fallbacks
  elsewhere deliberately untouched (not export surfaces).
- Chunk compile RUNNING at checkpoint. Remaining in TS2: export dialog UX rework
  (persistent options + save-above + in-dialog progress + Cancel) + the stems pick-list
  + save-first interlock; AudioClipStreamer offline synchronous mode + CL-282 counter;
  riders (CL-043 dither, CL-045 LUFS normalize, CL-056 offline block size); CL-057
  buffer hot-swap; CL-227 backend; then the TS2 gate + commit.
- Stems/metro/destination chunk verified GREEN both configs.

## 2026-07-27 — TS2 — export dialog rework + stems pick-list + save-first LANDED

- **New `MixerPage::getStemPickEntries()`** — the mixer is the single truth for the
  pick-list: every strip it currently SHOWS (visibility test covers the badger
  membership-driven bus hiding + the lazy Vox2/Inst2/Inst3/Rusty flags), display-group
  order, user-visible names via MixerTrackStrip::getName, MixerChannelIds per entry;
  Master + all buses defaultChecked=false, everything else true (Jeff's spec).
- **`ExportAudioDialog`** (file-local, StandaloneEditor.cpp) replaces the AlertWindow
  flow with the locked FL-style UX: PERSISTENT options box (combo vocabulary + quality
  mapping carried over verbatim, incl. the no-selection "Selected Section" disable);
  "Export stems" toggle unfolds the strip checklist in a viewport; Export -> save-first
  interlock -> async FileChooser ABOVE the dialog (cancel returns to options intact) ->
  the box flips to progress mode (bar + percent at 30 Hz, controls disabled, Cancel
  LIVE -> signalThreadShouldExit -> the render's abort path deletes partials +
  restores the session) -> completion closes the dialog, error box on real failures
  only. Render runs on a plain background juce::Thread; every cross-thread hop uses
  Component::SafePointer; the dialog dtor joins the thread (bounded — per-block abort
  polling). The pattern right-click path keeps runExportWithProgress (B.29
  reconciliation at TS8).
- **Save-first interlock:** doExportAudio's ensureSaved — a project folder passes
  straight through; an unsaved session prompts and chains through badger Task 12's
  success-only `doFileSaveAs(onSaved)` continuation into the export.
- Dialog chunk compile RUNNING at checkpoint.
- Dialog chunk verified GREEN both configs.

## 2026-07-27 — TS2 — clip streaming offline mode + CL-282 telemetry LANDED

- **The hazard:** AudioClipStreamer is an SPSC ring + background prefetch whose read
  paths return SILENCE when the ring can't serve (not-ready / span outside window) —
  correct for a live callback, but a fast offline render outruns the prefetch BY DESIGN,
  so exports would print silent gaps into clips.
- **The fix:** process-wide `AudioClipStreamer::sOfflineRender` (set/cleared by
  begin/endOfflineRender — the global-atomic toggle pattern already in-tree).  Under the
  flag, both read paths run `ensureRangeBlockingForOffline` — the seek() prefill shape
  executed inline on the render thread under the reader lock (blocking is the point; no
  live callback exists while the device is suspended).  LIVE behavior is bit-identical:
  the helper no-ops when the flag is off, and readAndMix's strict window test is
  preserved for live (the covered-to-EOF relaxation applies ONLY post-refill offline —
  caught my own first cut changing the live EOF case and re-scoped it).  RAM-loaded
  clips were never affected.
- **CL-282 telemetry:** `sUnderrunCount` — offline silence-returns the synchronous path
  could not prevent (EOF-past reads excluded).  Reset at beginOfflineRender; reported at
  endOfflineRender via a `[TS2 EXPORT]` DBG line.  Expected 0 on every render — the fix
  is provable at the TS8 smoke, not vibes.
- **Diagnostic Instrumentation Catalog (Rule 4):** `[TS2 EXPORT]` DBG underrun report in
  `endOfflineRender` — Tag `[TS2 EXPORT]`, Purpose: prove zero silent clip gaps per
  render, Disposition **Keep** (product Debug diagnostic, the CL-282 deliverable's
  proof surface; sibling of badger's dead-lane warn).
- Streamer chunk compile RUNNING at checkpoint. Remaining in TS2: riders (CL-043
  dither, CL-045 LUFS normalize, CL-056 offline block size), CL-057 hot-swap, CL-227
  backend, then the TS2 gate + commit surface.
- Streamer chunk verified GREEN both configs.

## 2026-07-27 — TS2 — riders LANDED: one loop core + dither + LUFS normalize + block size

- **The render loop extracted to `BuilderPage::runOfflineLoop`** — span/scope math, the
  offline drive + restores, the lane-aware clock, per-block lane replay, tail handling,
  ONE implementation; consumers differ only per block: `renderToFile` writes sinks +
  stem arena taps, the new `measureRender` feeds meters, and TS7's freeze render will
  be a third consumer of the same core. Ends the copy-the-loop drift risk before it
  started.
- **CL-056:** offline block = 2048 (in runOfflineLoop, was 512) — markedly faster
  renders while the lane-replay step (~46 ms at 44.1k) stays in the same class as the
  live 30 Hz applicator tick, so automation granularity in the file matches live.
- **CL-043:** "Dither (16-bit WAV)" toggle -> RenderOptions.dither -> TPDF at +-1 LSB
  of the 16-bit grid applied inside FileSink::write via a per-sink scratch (arena stem
  buffers are never modified in place) — main file + every stem uniformly.
- **CL-227 backend:** `BuilderPage::measureRender` — the loop with meters and no
  writers: LufsMeterDSP integrated + a 4x-oversampled Lagrange TRUE-PEAK ESTIMATE
  (documented approximate; TS7's BLU-108 upgrades it to the BS.1770 polyphase FIR).
  TS7's measure-before-render button + report face consume this as-is.
- **CL-045:** "Normalize to" toggle + target combo (-9/-14/-16/-23 LUFS, default -14 —
  streaming standard, my code-shape pick) -> two-phase dialog run: pass 1 =
  measureRender (0..50% of the progress bar), gain = target - measured BOTH directions,
  capped so estimated true peak stays under ceilingDbTp (-1 dBTP default; a TP already
  over the ceiling forces extra reduction — correct per the locked wording); pass 2
  re-renders with postGainDb applied at EVERY writer uniformly, so the stem sum still
  matches the normalized mix.
- Riders chunk compile RUNNING at checkpoint. Remaining: CL-057 buffer hot-swap
  (settings dialog surface), then the TS2 gate + commit surface.
- Riders chunk verified GREEN both configs.

## 2026-07-27 — TS2 CODE-COMPLETE: CL-057 verified already-satisfied; the gate is green

- **CL-057 disposition — no new code, verified in source:** the ASIO-scoped live
  buffer-size reconfigure ALREADY shipped (docket 3=b: `applyBufferSizeLive` in the
  audio settings dialog — quiesce callback -> setAudioDeviceSetup -> re-add; WASAPI/
  DirectSound deliberately keep the pending+restart flow for the exclusive-mode crash
  class). "Reusing the re-prepare machinery" is the setAudioDeviceSetup ->
  prepareToPlay cascade — whose REAL gap was this batch's finding: the cascade never
  swept drum or sfizz engines, so the shipped hot-swap left them prepared at the old
  block size.  That gap is closed by TS2's prepareToPlay sweep fixes.  The TS2
  checklist item is satisfied by the pre-existing feature + this batch's completeness
  fix; building anything more would duplicate a shipped surface.
- **Every TS2 checklist item is now DONE:** offline drive on the live model /
  full re-prepare (+ the two sweep gaps) / lane-aware clock + shared span resolver /
  UI-free lane replay (pedal tables TS3 as planned) / clip-streaming offline + CL-282
  counter / metronome gate / `<project>\Exports\` destination / FL-style dialog UX +
  per-strip stems pick-list + save-first interlock / CL-043 dither / CL-045 LUFS
  normalize / CL-056 offline block size / CL-040 stems / CL-227 backend.
- **THE TS2 GATE IS GREEN:** the riders-chunk build is the current tree (only doc edits
  since) — RELEASE_EXIT_CODE=0 / DEBUG_EXIT_CODE=0, zero error lines.  Commit surfaced
  to Jeff.

## 2026-07-27 — TS2 COMMITTED `e9ecf03e` (Jeff-approved); session ends, TS3 next

- **TS2 commit landed:** `e9ecf03e`, 15 files (+1700/-270), tree clean after (this
  entry + the carry-over refresh ride TS3's commit, same convention as the session-open
  backfills riding TS1's).  TS1 = `4ea67bd0`, TS2 = `e9ecf03e`.
- **Process corrections adopted mid-session (carry into every future session):**
  (1) intermediate "clean checkpoints" are NOT stopping points — the only stops are
  task-set commit approvals and genuine spec calls; Jeff called premature wrap-ups out
  three times before it stuck.  (2) Naming plan artifacts (batch IDs included) is the
  assistant's job, never a docket item (memory updated at the TS1 event).
- **Next: TS3 (automation fully model-side).**  No open sub-spec calls.  Scope per the
  plan + this file's pins: EffectParamMap tables for ALL remaining EffectTypes x
  variants (Saturation/Overdrive/Delay/Reverb umbrellas + pedal-native 100+ — ~20+
  tables; Reverb includes the 0/1 `freeze` def; the (type, variant) key + one-home
  mapping-math rules are HARD-WON, do not relearn); retire the 19 engine-editor wrapper
  sites (vocal capture-lock veto moves with them; Harmless A/B keeps both ids); pedals
  model-side registration; `_fader`/`_pan` lane remap + permanent-strip shim
  retirement; EQ band lane ownership off the display; statics re-seed simplification +
  `onIsParamStale` re-widen; BLU-344 Harmless mod-editor targets.

- **Lane-resolver rules for step 3 (derived from source, do not relearn):** the live
  replay ([PluginProcessor.cpp:2794-2868](../../Source/PluginProcessor.cpp:2794)) walks
  automation blocks + interpolates value01 (stepped/linear) but applies ONLY via
  `apvts.getParameter(paramId)` — every other lane class silently no-ops. The offline
  resolver chain, in match order: (1) main APVTS getParameter(paramId); (2) Vox/Inst
  engine lanes: `vox<N>_`/`inst<N>_` prefix -> strip index -> rig tab -> bare-id
  getParameter on the engine APVTS (Vox also covers the embedded NAM ids; Inst covers
  the NAM stage); (3) other engine lanes: paramId IS the engine param id
  ("tk_<trackId>_<fam>_*", globally unique) -> sweep rig tabs' apvtsOf for a match
  (cacheable per render); (4) rack lanes: `<channelPrefix>_<uuid>_<suffix>` -> INVERSE of
  EffectsPage::channelPrefixForId (mind the 1-based layer/bass ranges) -> rackForChannelId
  -> slot by uuid -> `output_vol` suffix hits EffectRack::setSlotOutputGain (-24..+12 dB
  range), everything else EffectParamMap::applyNorm keyed (type, variantOf(dsp));
  (5) legacy mixer lanes: `<mixerPrefix>_fader` translates to the `<mixerPrefix>_level`
  param (`_pan` is already the real param id) — TS3 retires the legacy spelling;
  (6) `global_tempo` -> the offline clock. The interpolation walk extracts to a shared
  helper so live + offline cannot drift.

## 2026-07-27 — TS3 — scout complete: the wrapper census, the table matrix, and one architectural finding

- **The 19 wrapper sites confirmed exactly (grep of the five `VKnobAutomation::register*Automation`
  helpers, whole tree):** BaySickBassEditor:392 (wireID slider lambda) / BaySickSynthEditor:398
  (same) / VibePlayerEditor:221 (same) + :260 :261 :262 (button) + :263 (selector) /
  HarmlessEditor:486 (wireMeta slider) + :563 (wireBtn button) + :639 (regDualParam PARAM) /
  HarmlessFilterRow:61 :62 :63 :65 (sliders) + :74 (combo) / HarmlessRoutingMatrix:38 /
  HarmlessXYZPad:46 / BaySickNAMIREditor:18 (PARAM, whole-APVTS loop) /
  BaySickVocalEditor:659 (PARAM, whole-APVTS loop + `kCaptureGated` veto).  Count = 19.
  Five of the five helpers are view-scoped: even the two PARAM-targeting ones die with their
  `lifetimeGuard` Component, so destroy-on-close kills them exactly like the widget ones.
- **TS1's `registerModelEngineAutomation` already covers 17 of the 19 by construction** — it walks
  the engine APVTS and registers null-owner param-targeting entries for EVERY RangedAudioParameter,
  which subsumes every per-knob/-button/-combo/-selector wrapper AND Harmless's dual A/B pair (both
  part ids are separate APVTS params, so both lanes register — the QA-ApvtsAutomation semantics hold
  for free).  The two that do NOT survive retirement as-is: the vocal `kCaptureGated` veto
  (`suppressWhen`) and the NAM/IR prefixing, which the model hook already prefixes but without the
  veto.  So wrapper retirement = delete 19 call sites, keep every `setComponentID` stamp, and carry
  the veto into the model hook.
- **The EffectType x variant matrix derived from `createEffectEditor`'s dispatch (both branches).**
  Full-mode: 31 types; DSP-readable variant umbrellas are Compressor (Modern/FET/Opto/CS — SHIPPED),
  Saturation (Tube/Console/Tape), Delay (Echo/VocalDoubler), Overdrive (Rack/Pedal).  Everything
  else is single-panel.  Pedal-mode adds a SECOND dispatch for 7 types
  (Limiter/Saturation+Tape/Chorus/Flanger/Phaser/Delay/Reverb -> `*PedalPanel`).
- **ARCHITECTURAL FINDING — PanelMode is a second variant dimension, and it is not DSP-readable.**
  `variantOf` reads the DSP on purpose (no UI on the automation path), but which of the two panels a
  DSP got depends on WHERE the slot lives, not on the DSP.  Three live collisions, all the same class
  as the Modern-vs-FET `attack` bug:
  (1) **Saturation with `satType == Tape` in a pedal slot** builds `SaturationPedalPanel`, whose
      "Drive" is 0..10 -> `setFlowers`; the rack's Tape panel "Drive" is -24..+24 dB ->
      `setTapeInputGain(decibelsToGain(v))`.  Keying on `variantOf(dsp)` alone would feed a
      face-plate 0..10 into a dB->gain setter.
  (2) **Reverb `decay`** — rack 0.1..20 s, pedal 0.1..10 s (same setter, different range).
  (3) **Phaser `rate`** — rack upper bound is DYNAMIC (`getRateMaxHz()`, 2 Hz Slow / 10 Hz Fast),
      pedal is a fixed 0.05..10.
  The pedals picker really does offer Compressor / Saturation / Chorus / Flanger / Phaser / Delay /
  Reverb / Overdrive ([BaySickPedalsEditor.cpp:495-521](../../Source/BaySickPedals/BaySickPedalsEditor.cpp:495)),
  so all three are reachable, not theoretical.  Resolution (implementation shape inside locked scope,
  same rule the Compressor lesson established): `PanelContext { Rack, Pedal }` becomes part of the
  key, supplied by the REGISTRATION SITE (which always knows whether it is a rack or the board)
  rather than read from the DSP.  `variantOf(type, dsp, ctx)` returns a distinct pedal ordinal for
  the 7 dual-panel types and ignores ctx for everything else.
- **Two ParamDef extensions the census forced:** (a) an optional `rangeOf(dsp, lo, hi)` hook, because
  the old widget applicator read `slider.getMinimum()/getMaximum()` AT APPLY TIME — Phaser `rate` is
  the one param whose range moves, and a static lo/hi would silently change its meaning; (b) an
  `affectsLatency` flag, because the panel lambdas for lookahead-class knobs also fire
  `onLatencyChanged` -> bus PDC refresh.  The shipped Compressor `looka` entry already lost that poke
  at badger; closing it uniformly for Compressor `looka`, Limiter `ahead`, DeEsser `look`.
- **Scope boundary held:** tables cover exactly the suffixes that are STAMPED today (`knobs` +
  `getExtraKnobs()` + `outputVolKnob` + `mAutoToggles`).  Controls that never had a paramId — the
  Graphic/Bass-Graphic EQ faders (plain `juce::Slider`), the Tuner trim knob, the chickenhead
  selectors and mode toggles — stay unautomatable.  Making them automatable would be an unprompted
  behavior change, not a TS3 deliverable.
- **Panel conversion list (where the map must be the ONE home, i.e. real math would otherwise exist
  twice):** Opto `peak_reduction`/`gain` (the 0..100 <-> dB map is ALREADY duplicated between
  [EffectEditorPanels.cpp:620-631](../../Source/Standalone/EffectEditorPanels.cpp:620) and the shipped
  `kCompOpto` table — badger's leftover), Flanger `damp` (log Hz map + its inverse), DeEsser `detect`
  (Freq+Q macro), TapeSat `drive`/`hiss` (dB->gain), VocalDoubler `mix` (x0.01), Delay `tone`
  (negation), AcousticPreamp `notch` (off-below-50 branch).  Trivial pass-throughs keep their direct
  setter — the table lambda calls the identical setter, so there is no math to drift.
- **FINDING (pre-existing dead code, NOT this batch's — routed, not deleted):** `TapePanel`
  ([EffectEditorPanels.cpp:3028-3178](../../Source/Standalone/EffectEditorPanels.cpp:3028), ~150
  lines, binds `TapeDSP*`) has ZERO construction sites tree-wide — `EffectType::Tape` has dispatched
  to `TapeSatPanel` since the H-10 cutover (2026-05-02).  Its only remaining mentions are two
  comments.  No table is written for it (unreachable).  Accrued to the pending ledger for Jeff's
  Rule-3 routing call at G4 close (Phase 6 is the reserved home for dead-code cleanup).
- **Mixer lane facts:** `<prefix>_pan` IS a real main-APVTS param id, so `registerStaticAutomationHandlers`
  already registers it param-targeting — `MixerTrackStrip::setAutomationPrefix`
  ([:439-485](../../Source/Standalone/MixerTrackStrip.cpp:439)) then OVERWRITES it with a widget
  applicator, and `reRegisterStripAutomation` re-installs that widget entry after every project
  boundary.  `<prefix>_fader` has no param twin (the param is `_level`), which is the only reason the
  shim exists.  The remap is therefore: register `_fader` model-side as an ALIAS onto the `_level`
  param at param-materialization time, drop both widget registrations, and the shim + its
  `reRegisterAutomation` helper retire.  Not a save-migration (lane ids are unchanged on disk).

## 2026-07-27 — TS3 — EffectParamMap completed: ~30 tables across every EffectType x variant

- **Shape:** two shorthand macros (`SET_FLD` setter+public-field, `SET_GET` setter+const-getter) keep the
  ~200 honest pass-through entries to one line each, so the genuinely non-trivial mappings stand out
  instead of drowning. Tables: Compressor x4 (shipped at badger), Reverb, Chorus, Delay x2
  (Echo / VocalDoubler), Saturation x3 (Tube / Console / Tape, with EffectType::Tape aliasing the Tape
  table), Flanger, Overdrive x2 (Rack / Pedal), Phaser, TransientShaper, Limiter, DeEsser, Gate,
  DeReverb, the 14 pedal-native types, and 7 pedal-FACE tables. **First build was green both configs
  with zero errors** — the setter/getter names were pre-verified against the DSP headers by script
  rather than by compile-and-fix.
- **PanelContext landed as designed** — `variantOf(type, dsp, ctx)`, `kPedalVariant = 100`. Pedal
  registration passes `PanelContext::Pedal` and gets the pedal table for the 7 dual-panel types, the
  DSP-read variant for everything else, by construction. `createEffectEditor` stamps the resolved key
  onto the panel it builds (both dispatch branches), so panel and registry can never disagree about
  which table a slot uses.
- **Two ParamDef fields the census forced, both used sparingly:**
  `rangeOf` — ONE user, Phaser `rate` (the Slow/Fast switch moves its upper bound between 2 and 10 Hz;
  the applicator this replaced read the live slider bounds every tick). `affectsLatency` — three users
  (Compressor `looka`, Limiter `ahead`, DeEsser `look`); the rack applicator now runs
  `setLatencySamples(updateBusLatencies())`, the same PDC refresh the panel lambdas fire through
  `onLatencyChanged`. The shipped Compressor entry had silently lost that poke at badger.
- **Non-trivial mappings that now have exactly one home** (panels call the map, or the map is the only
  copy): FET input-drive inversion + attack/release switch tables (shipped), Opto's 0..100 face plate
  (the map and the panel each had a copy — badger's leftover, now one), Flanger `damp` (log 20 kHz ->
  200 Hz sweep AND its inverse), DeEsser `detect` (the Freq+Q macro), TapeSat `drive`/`hiss` (dB ->
  linear gain), VocalDoubler `mix` (x0.01), Delay `tone` (sign inversion), AcousticPreamp `notch`
  (the below-50-Hz OFF zone), TransientShaper `attack`/`release` (x100 face scaling), Flanger `feed`
  (percentage vs coefficient).
- **`ChorusDSP::getLFOFreq(int)` added** — a const accessor for a private field, so the three LFO-rate
  lanes have a read-back. No behavior change.
- **Scope boundary held:** the Graphic/Bass-Graphic EQ pedals and the Tuner get NO table. Their
  controls are plain `juce::Slider` faders / knobs outside `knobs` + `getExtraKnobs()`, so
  `setSlotContext` never stamped them and they have no lanes to resolve. Writing tables for them would
  invent automation the UI has no way to create — a feature, not a TS3 deliverable.

## 2026-07-27 — TS3 — panels stop registering; a display refresh replaces what that cost

- **`EditorPanelBase::setSlotContext` is now STAMP-ONLY.** Its knob + toggle applicator registrations
  are deleted; the model registers the same ids against the rack (`registerSlotAutomationFor`) or the
  board (`registerPedalAutomation`).
- **Consequence caught before it shipped, and closed:** rack panels are DSP-direct — no APVTS, no
  attachment — so nothing carries a DSP change back to the knob. While automation drove the KNOB that
  was invisible; with automation driving the DSP, an automated control would sit frozen at its last
  user position while the sound moved underneath it. This was ALREADY live for Compressor + the
  per-slot output knob (their DSP-targeting applicators shipped at badger), so TS3 fixes a shipped
  regression rather than only avoiding a new one.
  **Fix:** `EditorPanelBase` gained a display refresh — a 10 Hz timer-by-composition (several derived
  panels already inherit `juce::Timer`, so inheritance would be ambiguous) that re-reads the stamped
  controls through `EffectParamMap::readNatural` (new). Display only: values are pushed with
  `dontSendNotification` so it can never write back or form a loop; skipped while the panel is hidden
  and while the slider is under the mouse so it cannot fight a drag; `output_vol` has no table entry
  and is left alone by construction. The dtor stops the timer FIRST — `mSyncKnobs` holds raw pointers
  into vectors the DERIVED panel owns, and the derived destructor has already run by then.
  This is the plan's "views keep ... UI readers where needed" clause.
- **Pedals are model-side.** New `BaySickPedalsProcessor::onSlotAutomationChanged`, fired from
  loadEffect / clearSlot / restoreFullState (NOT moveSlot — uuids travel with the Slot and applicators
  resolve by uuid, so a reorder cannot repoint a lane), deliberately separate from the editor-owned
  `onSlotsExternallyChanged` because a `std::function` has one subscriber and this must fire whether or
  not an editor exists. `StandaloneEditor::registerPedalAutomation` walks the 8 slots, resolves board
  -> slot BY UUID -> DSP at apply time, keyed `PanelContext::Pedal`. Wired at Inst-tab creation.
- **The offline path got the pedal branch TS2 deferred** — `inst<N>_pedals_<uuid>_<suffix>` resolves in
  `applyOfflineLaneValue` now. The board is not an EffectRack on a graph channel, so the rack walk
  could never have seen it: without this branch pedal automation was simply absent from every export.

## 2026-07-27 — TS3 — the 19 wrapper sites retired; the five helpers deleted with them

- **All 19 gone, verified by grep returning zero matches** for
  `VKnobAutomation::register(Slider|Button|Combo|Parameter|Selector)Automation` tree-wide. Every
  `setComponentID` stamp stays — the right-click Automate menu reads the id off the clicked component.
- **Why this was a pure deletion, established BEFORE cutting:** every wrapper keyed its registration to
  `p.pid(name)`, and `pid(name) == mPrefix + name` IS the engine's APVTS param id. TS1's
  `registerModelEngineAutomation` walks the whole engine parameter list and registers exactly those
  keys, param-targeting. So the wrappers were a second, view-scoped claim on keys the model already
  owned — and since the editor is built AFTER the engine, the view claim was WINNING. Retiring them is
  what makes TS1's registration actually take effect.
- **Harmless A/B preserved by construction, not by special-casing.** Both part ids are separate APVTS
  params, so the model's parameter-list walk registers both, param-targeting. The `regDualParam` pair
  that used to guarantee this was guarded by the very slider that Part A/B rebinding destroys and
  rebuilds — strictly worse than what replaces it.
- **The vocal capture-lock veto travelled with the wrapper**, as the plan required. The gate list moved
  to `BaySickVocalProcessor::isCaptureGated` — model-side because two consumers need the same set and
  neither is more authoritative (the editor greys these during a take; the applicator vetoes writes for
  the identical reason). It had been two hand-kept copies, editor timer and registration list, one
  edit from disagreeing. `registerModelEngineAutomation` applies the veto for Vox main-engine params.
- **The five helpers are DELETED, not left dangling** (this batch's own dead code, cleaned in-batch).
  Four drove a control; the fifth still took a `Component& lifetimeGuard`. Under destroy-on-close
  every one of them is a guaranteed dead lane, so leaving them would be leaving a trap.

## 2026-07-27 — TS3 — mixer + EQ lanes move to param materialization; the owner index is gone

- **New model event `VibeSynthProcessor::onMixerStripParamsCreated`**, fired from
  `ensureMixerStripParams` the first time a prefix's params are created. Strips materialize lazily,
  long after the startup sweep, which is the whole reason the mixer strip and the EQ display each
  carried their own view-scoped registration. One model event replaces both.
- **`_fader` remapped, and the shim retired.** The lane has always been spelled `<prefix>_fader` while
  the parameter is `<prefix>_level` — the id predates the param, and that mismatch is the ONLY reason
  mixer faders needed a view-scoped applicator plus `MixerPage::reRegisterStripAutomation` to put it
  back after every project boundary. `registerStaticAutomationHandlers` now DERIVES the alias while
  walking params (any `mixer_*_level` gets a `_fader` twin), so it is restored by the same call that
  re-seeds everything else. Saved lanes keep their spelling — nothing on disk changes, and this is not
  a migration. `MixerTrackStrip::setAutomationPrefix` is stamp-only; `reRegisterAutomation` and
  `MixerPage::reRegisterStripAutomation` are deleted. Automating a fader now moves the PARAMETER and
  the attachment moves the cap — the knob follows again, which the widget path also did.
- **`ParametricEQDisplay::registerAutomationForBoundEQ` deleted.** Its closures already wrote the
  parameter, so the display contributed only a lifetime — and the wrong one twice over: band lanes died
  with the EQ view, and did not exist at all until some view had bound the EQ (a band lane silently did
  nothing after a project load until the user happened to visit the page). Safe to delete outright
  because the function early-returned on any id `apvts->getParameter` did not know, so every id it ever
  registered is a real APVTS param the static walk covers.
- **The owner index is DELETED and `StandaloneEditor` is no longer a `juce::ComponentListener`**
  (verified: that base class had exactly one user, this). Gone: `trackAutomationOwner`,
  `componentBeingDeleted`, `mAutomationOwners`, `mAutomationIdOwner`, `mTearingDownAutomation`, and the
  `juce::Component* owner` parameter on both registration hooks. Nothing registers with an owner any
  more, so the index was a lifetime system over a permanently empty set — worse than nothing, because
  it would half-work for anyone who later re-added a view-scoped registration. Both of badger's
  hard-won guards (clear-the-claim on null-owner; revoke-only-what-you-still-own) existed because view
  lifetime and lane lifetime were tangled; TS3 untangles them instead of guarding them.
- **`onIsParamStale` RE-WIDENED** to "not a main-APVTS param AND not in the registry" — the exact test
  reverted on 2026-07-26. The revert was right then (panel-keyed applicators made "unregistered" mean
  "you are looking at another channel"); it is honest now that registration lasts as long as the thing
  it targets, so "not registered" genuinely means the target is gone. Without the widening, deleted
  tabs / cleared rack slots / removed pedals leave lanes that LOOK alive.

## 2026-07-27 — TS3 — BLU-344: the mod editor's DEPTH + LENGTH become automatable

- **The one target class in this batch that is neither an APVTS param nor a rack DSP.** DEPTH and
  LENGTH are fields on `ModSourceState` inside `HarmlessModRegistry`, addressed by (target paramId,
  source). Lane ids `<targetParamId>_mod<sourceIdx>_depth` / `_length` — no APVTS param can collide
  with that shape, since the target id is itself a full engine param id.
- **The 13-step LENGTH table moved to `HarmlessModLength`** in HarmlessModRegistry.h, beside the field
  it writes, with a `nearestIndex` inverse. The editor had the forward table in one function and a
  hand-rolled copy of the inverse in another; both now call the shared one, and the automation
  applicator lands on exactly the same 13 values the knob offers.
- **Registered model-side** at engine creation (`registerHarmlessModAutomation`, a no-op for non-Harmless
  engines) for every (target x source). Applicators re-resolve rig -> tab -> engine -> registry at
  apply time and call `publishSnapshot()` so voices observe the change on their next block — the same
  contract the editor's own edits use. LENGTH is registered only for Envelope + LFO: the editor hides
  the control for the other five sources, so a lane there would address something the user cannot set.
- **The editor stamps both ids in `syncControlsFromState`** — the one place the visible (target, source)
  pair changes — so the id under the cursor always names the pair the knob is actually editing.
- **Offline replay covers them too.** `applyOfflineLaneValue` gained a mod-lane branch; without it,
  DEPTH/LENGTH automation would have played live and been silently missing from every export.

## 2026-07-28 — TS3 — owner ruling: sfizz automation is a DEFECT, fixed; TapePanel deleted

- **Jeff's call on the two items surfaced at the TS3 commit:** (1) delete `TapePanel` outright rather
  than routing it to the G4-close ledger; (2) the sfizz gap is "something you never setup and you need
  to fix" — NOT a new feature, so it ships in this batch. Both done.
- **The sfizz gap was worse than I reported, and the correction matters.** I surfaced it as "these
  engines have no automation lanes." Reading further: `AriaControlPanel::showAriaParamPopup`
  ([AriaControlPanel.cpp:90-113](../../Source/Standalone/AriaControlPanel.cpp:90)) has ALWAYS offered
  "Automate: <label>" on every kit CC knob and fader, and firing it calls
  `VKnobAutomation::sOnAutomate(paramId)` — which creates and draws a real lane. Nothing ever
  registered an applicator for those ids, so the lane played back against nothing. So this was not an
  absent feature: it was a UI that advertised automation, accepted the gesture, drew the clip, and
  silently did nothing. Jeff was right to call it a defect.
- **Why the trio was missed:** they are the only engines that are PROCESSOR-owned rather than
  rig-owned (`mGuitarsEngine`/`mBassesEngine` per-instance arrays + the `mRustyDrumsEngine` singleton,
  deliberately left on their own race-safe load paths at TS1). TS1's model hook keys off
  `EngineRig::onEngineCreated`, which by definition never fires for them.
- **The fix, matching every other family's shape:** new
  `VibeSynthProcessor::onSfizzEngineReady (SfizzEngineKind, instIdx)` fired at the end of each
  successful `loadBaySick{Guitars,Basses,RustyDrums}Kit` — deliberately OUTSIDE the per-slot SpinLock
  (the audio thread try-locks it, and registration is a map insert per param), and on every successful
  load rather than only first construction, so a destroy/recreate or source switch re-registers by
  itself. `StandaloneEditor::registerSfizzEngineAutomation` walks the engine's whole parameter list
  (outVol + the CC bank + the cut-self pair) and registers applicators that RE-RESOLVE the engine
  through the processor at apply time — these engines are destroyed and rebuilt by kit loads, so a
  captured pointer would be exactly the stale-target bug the model-side rewrite exists to prevent.
- **Lane ids need no new vocabulary:** the param ids are already globally unique (`bgg_<idx>_*`,
  `bbb_<idx>_*`, `brd_*`), so the lane id IS the param id — precisely what the Aria menu was already
  passing to `sOnAutomate`. Every lane a user created before this fix starts working; none are
  orphaned.
- **Offline replay extended too** — new `VibeSynthProcessor::forEachSfizzApvts`, consumed by
  `applyOfflineLaneValue`'s engine-lane stage. `EngineRig::forEachEngine` covers rig-owned engines
  only, so without it a sfizz lane would have applied live and been missing from every export: the
  same class of hole TS2 fixed for the rig families.
- **Project-restore path verified, not assumed:** Rusty's `reloadForProjectRestore` forwards to
  `mProcessor.loadBaySickRustyDrumsKit`, and Inst restores go through `loadBaySick{Guitars,Basses}Kit`,
  so the hook fires after `resetProjectState`'s map clear on every load.
- **`TapePanel` DELETED** (147 lines, `EffectEditorPanels.cpp`), with its now-unused
  `#include "../DSP/TapeDSP.h"` and the two comments that referenced it by name. Confirmed for Jeff
  before cutting: `TapePanel` bound the LEGACY standalone `TapeDSP*`; the Tape option he actually uses
  is `TapeSatPanel`, which binds `SaturationDSP*` + its `setTape*` family and is what
  `createEffectEditor`'s `EffectType::Tape` case builds. Covered by the new `kSatTape` table.
- **`TapeDSP` CLASS DELETED TOO** (Jeff, same exchange: "that should be cleaned as well then,
  correct?" — yes).  `Source/DSP/TapeDSP.h` + `.cpp` removed via `git rm`, its `CMakeLists.txt:501`
  build entry dropped, and the two stale `#include "DSP/TapeDSP.h"` lines in `EffectRack.cpp` +
  `EffectPresetIO.cpp` (whose files never touched the type) removed with it.  A full-tree census ran
  BEFORE cutting: zero code dependencies.
- **Two things deliberately KEPT during that sweep, because deleting them would break behaviour or
  erase the reason the current code looks how it does:**
  1. `SaturationDSP.cpp:704`'s `if (xml->getTagName() == "TapeDSP")` — a STRING compare, not a type
     reference.  It is the legacy-preset migration path that lets pre-cutover Tape presets load, and
     removing it would silently orphan every one of them.
  2. The remaining `TapeDSP` mentions in `SaturationDSP.h/.cpp`, `EffectRack.cpp`,
     `EffectEditorPanels.cpp` and `EffectPresetIO.cpp` are Rule 6 keeper comments — DSP/domain
     references explaining that SaturationDSP's tape body is a bit-exact port of the deleted class,
     and why `EffectType::Tape` is an alias.  `EffectRack.cpp`'s was the ONE that became false (it
     claimed the class "stays in the source tree as an emergency-rollback safety net"), so it was
     corrected rather than kept.
- **Follow-on cleanup:** deleting TapePanel orphaned its only helper, the file-local
  `setPink` TextButton colourer (MSVC C4505 flagged it on the next gate).  Removed too --
  this batch's own dead code, cleaned in-batch rather than routed.

## 2026-07-28 — TS3 COMMITTED `1dd08437` (Jeff-approved); TS4 next

- **TS3 commit landed:** `1dd08437`, 40 files (+2040/-1603), tree CLEAN after.  Gate was green
  both configs on the exact committed tree (zero error lines; the C4505 that the TapePanel
  deletion briefly exposed was cleaned before the final gate).  Batch so far: TS1 `4ea67bd0`,
  TS2 `e9ecf03e`, TS3 `1dd08437`.
- **The widget-targeting era is over.**  Zero `VKnobAutomation::register*Automation` call sites
  remain, the five helpers are deleted, the owner index and the `ComponentListener` base are
  gone, and every lane class in the app — main APVTS, engine params, rack DSP, pedal board, EQ
  bands, mixer strips, sfizz kits, and the non-parameter mod-editor fields — is registered by a
  model event and re-resolves its target through the model at apply time.  That is the
  precondition TS4's destroy-on-close windows depend on.
- **Everything TS3 registers, the offline renderer also resolves.**  Pedal-board lanes, the
  BLU-344 mod lanes, and the sfizz trio all got branches in `applyOfflineLaneValue` in the same
  pass they got live registration, so no lane class can play live and go missing from an export
  the way vox/inst did before TS2.
- **Next: TS4 (the shell).**  Its FIRST act is the open sub-spec call — exact minimum window
  sizes for Builder / Piano Roll / Mixer, picked with Jeff ON SCREEN (he wants "larger" floors).
  Nothing else in TS4 starts before that answer.

## 2026-07-28 — TS4 — scout: what the shell replaces

- **Page hosting today:** `StandaloneEditor::mPages` is an `OwnedArray<PageEntry>`
  (`{ribbonTabId, TabType, unique_ptr<Component>}`); EVERY page is a live child of the editor
  simultaneously, and `showPageForTab` just flips `setVisible` across the whole list
  ([StandaloneEditor.cpp](../../Source/Standalone/StandaloneEditor.cpp)).  `resized()` gives every
  page the same content rect `b` (below menu 24 + transport 40 + PageMenuBar).  So "closing" a tab
  today means destroying the PageEntry; there is no window concept at all, and nothing is ever
  hidden-but-alive except by visibility.  That content rect is what becomes the WORKSPACE.
- **No native-child precedent in the tree.**  `addToDesktop` appears exactly once
  (StandaloneEditor.cpp:464, the modal shield).  EventEditor / KeyBinds / UndoHistory are
  `juce::DocumentWindow`s — free-floating TOP-LEVEL desktop windows, not children of the main
  frame.  They are the precedent for bounds persistence + the off-screen clamp, NOT for containment.
  The native-child peer (locked call 2b) is genuinely new machinery: `addToDesktop(flags,
  parentHWND)` per window, parented to the workspace's peer handle.
- **Main window** ([StandaloneApp.cpp:945-1013](../../Source/Standalone/StandaloneApp.cpp:945)):
  currently `setResizable(true,false)` + `setResizeLimits(1100,700,32000,32000)` + a
  saved-bounds restore with a monitor-overlap reachability test (QA-ProjectSave).  TS4 pins it
  fullscreen/non-resizable — but the ORDER COMMENT at :956 is load-bearing and must survive: limits
  must be installed BEFORE any setFullScreen, or Windows silently demotes MAXIMIZED to NORMAL.
  The reachability clamp is the pattern TS4 reuses PER WINDOW.
- **RibbonTabBar is a FIXED 10-slot bar** — `kNumSlots = 10`, `slotType(slotIndex)` is a hard
  index->type map, every slot always painted, no "+" and no close X (its own header comment says
  so).  The TS4 tab bar is therefore a real rewrite of the slot model, not a tweak: required four
  (Builder / Mixer / Effects / PianoRoll) always present, the six type slots appearing only at
  >= 1 instance and returning via "+".  Six `on*EmptyStateRequested` callbacks + the three
  `EngineEmptyState` members + `hideAllEmptyStates` (6 call sites) all retire with docket 18's
  shape.  `mLastUsedByType` and the instance/sub-page dropdown bodies survive unchanged.
- **PageMenuBar has no .h/.cpp of its own** — it lives inside SharedUI (the plan's file list is
  wrong on this point).  `PageMenuBar::kHeight` is consumed directly by `resized()`.  Merging it
  into per-window title strips means moving a shared component, not editing a dedicated pair.

## 2026-07-28 — TS4 — chunk A: the window frame family lands (compiles, both configs)

- **New `Source/Standalone/WorkspaceWindow.h/.cpp`** (+ CMakeLists entry), holding two classes:
  `Workspace` (the region of the fixed frame contained windows live in — supplies the native
  parent handle and the origin their coordinates are measured from) and `WorkspaceWindow` (one
  contained window: custom title strip per locked call 4a, close button + resize border per 5a,
  ComponentBoundsConstrainer floor, drag-to-move, bounds persistence).  NOT yet wired into page
  hosting — that is the next chunk.  Gate green both configs, zero errors.
- **FRAMEWORK CONTRACT VERIFIED IN THE VENDORED JUCE, NOT ASSUMED** — this is the single most
  load-bearing fact in TS4 and there was no existing user of it in the tree (`addToDesktop`
  appears once, for the modal shield).  Read from
  `juce/modules/juce_gui_basics/native/juce_Windowing_windows.cpp`:
  * `addToDesktop (flags, parentHwnd)` with a non-null parent ORs in `WS_CHILD` (:2239) and
    passes the parent to `CreateWindowEx` (:2437) — a real OS child window, which is what puts
    our frames in the same z-order space as a hosted VST3 editor's foreign HWND (TS6).
  * **The coordinate space differs from a top-level peer in BOTH directions, and they agree with
    each other:** `setBounds` feeds `SetWindowPos`, and Windows reads a WS_CHILD's x/y as
    PARENT-CLIENT relative (:1614); `getBounds` returns `getWindowClientRect`, likewise
    parent-relative (:1653 — the screen-relative branch above it is the `parentToAddTo ==
    nullptr` case only).  So a contained window is positioned and read back in the MAIN WINDOW'S
    client space, and the workspace's own offset inside the frame has to be added on.
  That contract is written into the header as a Rule 6 category-4 comment so it is never
  re-derived; `Workspace::originInParentClient()` is the one helper that applies it.
- **Persistence stores WORKSPACE-LOCAL bounds, not the peer's parent-client bounds** — deliberate:
  the workspace's origin moves whenever the main chrome changes height, so storing parent-client
  coordinates would drift every window down by that delta on the next run.  Saved under a
  `WorkspaceWindows` child of settings.xml (global preference, not project data — the user's
  window arrangement should not change when they open a different song), keyed by a stable
  per-logical-window string rather than per object, since destroy-on-close means the object is
  short-lived and the key is the only thing carrying position forward.
- **`Workspace::clampWindowsIntoView`** applies the QA-ProjectSave monitor-overlap lesson per
  window: a window is never wider/taller than the workspace, and always keeps its title strip —
  the only grabbable part — reachable.  A window whose saved spot no longer fits comes BACK
  rather than becoming unreachable.
- **One compile fix on the first gate** (both errors in the same helper): `ComponentPeer::
  getComponent()` returns a `Component&`, not a pointer, and `juce::Point` has no scalar
  `operator*`, so the negate is written out per-axis.

## 2026-07-28 — TS4 — the shell goes live: pages hosted in contained windows, tab bar rewritten

- **Pages now live in contained windows.**  `StandaloneEditor::PageEntry` gained a
  `unique_ptr<WorkspaceWindow> window`, a new `hostPageInWindow` frames the page, and
  `resized()` hands the old content rect to the `Workspace` instead of giving EVERY page the
  full rect at once (which was the always-alive stacking the shell exists to replace).
  `showPageForTab` no longer hides the other pages -- it brings the selected window forward,
  recreating it if it had been closed.  Ownership deliberately did NOT move: the page stays in
  `PageEntry::component` and the window hosts it via a new `setContentNonOwned`, because a large
  amount of existing code reaches through that pointer and moving ownership would have meant
  rewriting all of it for no gain.
- **A REAL BUG the compiler caught, worth recording because the shape recurs.**  The page
  creation sites are not uniform: twelve use `entry->component = ...; addChildComponent(...);
  mPages.add(entry);`, but THREE (Clips / Vox / Inst) use a `unique_ptr<PageEntry>` with
  `addChildComponent (*cpRaw); mPages.add (entry.release());`.  My first conversion pass
  pattern-matched only the first shape, so those three pages would have been hosted in NO window
  -- and since `resized()` no longer lays pages out, they would have rendered at zero size.  It
  surfaced only because removing the empty-state members made those same functions fail to
  compile for an unrelated reason.  All fifteen sites now route through `hostPageInWindow`.
- **RibbonTabBar: fixed 10-slot strip -> dynamic slots + "+".**  `kNumSlots` (a compile-time
  index->type map) is replaced by `visibleSlotTypes()`, built per paint: the four REQUIRED tabs
  (Builder / Mixer / Effects / Piano Roll) always, the six instance types only while
  `countTabsOfType > 0`.  `slotType` became an instance method; `slotRect` / `hitTestSlot` /
  `paint` all run off the live count with `kMaxSlots` only bounding the width solver's stack
  arrays.  The trailing slot is the "+" button, which owns every add route -- including the ones
  that used to be buried inside a POPULATED type's dropdown and were therefore unreachable at
  zero instances, exactly the hole the empty-state pages were papering over.
- **Empty-state machinery RETIRED (the loud docket-18 reversal the plan calls for).**  Gone:
  three `EngineEmptyState` members + `ClipsEmptyState` / `VoxEmptyState` / `InstEmptyState`
  members, six `show*EmptyState` functions, `hideAllEmptyStates` (~140 lines), their ctor
  construction, their `resized()` layout block, and the six `on*EmptyStateRequested` callbacks
  on RibbonTabBar.  The justification is structural, not preference: a type tab is no longer
  DRAWN at zero instances, so "the user is looking at a tab with nothing in it" is not a
  reachable state.  The F8/F9/F10 shortcut fallback that navigated to those pages is now a
  no-op for the same reason.  The `*EmptyState` CLASSES themselves stay -- they are referenced
  from ClipsPage / VoxPage / InstPage and pruning those is a separate sweep, not this one.
- **Main frame pinned fullscreen + non-resizable** (locked call).  The QA-Eb ordering comment is
  PRESERVED verbatim: resize limits must be installed before `setFullScreen`, or Windows
  silently demotes MAXIMIZED to NORMAL and every relaunch comes up windowed-almost-full.  The
  frame's saved-bounds restore AND its `WindowState` writer both retired with resizability
  (there is one valid frame geometry now) -- but the monitor-reachability LESSON did not: it
  moved to `Workspace::clampWindowsIntoView`, which keeps every contained window grabbable.
- **Settings-file bug found and fixed in my own new code before it shipped:**
  `WorkspaceWindow::saveBounds` created a fresh root as `<Settings>` when settings.xml was
  absent, while every other writer uses `<BaySickDAWSettings>` (ProjectManager :605).  If window
  bounds had ever been the first thing written to a missing settings file, the result would have
  parsed fine and been invisible to every other reader.  Now matches.
- **Provisional floors in place** (`setMinimumSize (640, 400)` per window) so nothing is
  unbounded while Jeff's B.31.0 measurements are pending.
- Gate green both configs, zero errors, at every step above.

## 2026-07-28 — TS4 — page menu merged into the title strip (locked call 4a); a self-inflicted detour reverted

- **MY ERROR, recorded because the failure mode matters more than the fix.**  I retired the
  empty-state pages (correct), noticed that `installEmptyStatePagePresetMenu` was orphaned by
  that, and read it as "empty-state code."  It is not -- it calls
  `mPageMenuBar->setMenuBuilder`, i.e. it installs on the PAGE MENU.  Instead of asking where
  the page menu was supposed to end up, I invented a new home for the route on the "+" menu,
  complete with a type-picker submenu the plan never asked for, and then presented Jeff three
  options -- including "drop the capability" -- as if it were an open question.  It was not:
  **locked call 4a says "each page's hamburger/menu row merges into the title strip,"** and the
  TS4 checklist repeats it as "custom title strip (merged page menu per 4a)."
  Root cause: I had SKIPPED the title-strip merge, so when a piece of the page menu came loose
  there was no correct place to put it -- and I treated that as "this needs rescuing" rather
  than the true reading, "you skipped a step."  All the "+"-preset work is reverted.
- **The panic was also unfounded, which the audit showed only after the fact:** a populated
  page's hamburger routes to `showPageActionsMenu`, which is where Save/Load Page Preset
  actually lives.  That menu is now in the title strip, so the capability was never at risk for
  a page that exists.  The single route genuinely lost is spawning a page from a preset when
  ZERO of that type exist -- which the plan already answers ("+" creates the page, its
  title-strip hamburger loads the preset).  `spawnAndLoadFromPagePreset` (174 lines) deleted as
  the orphan it became.
- **The merge itself, and why per-window instances rather than one shared bar.**  The deciding
  fact is that several windows are visible AT ONCE now.  Today's single `PageMenuBar` works only
  because exactly one page is ever visible and the bar is re-pointed on each tab switch -- with
  five windows on screen a shared bar could only ever show one of their menus.  It is
  structurally impossible, not merely inelegant.  And that re-pointing is precisely the
  view-coupling TS3 spent a whole task set removing.  Per-window instances also mean a window's
  menu dies with the window for free.  The CLASS stays shared (all menu-building logic reused
  verbatim); only the instancing changed.
- **How ~77 call sites survived untouched:** `mPageMenuBar` went from
  `unique_ptr<PageMenuBar>` to a raw pointer at the ACTIVE window's bar, set in
  `showPageForTab`.  A `mDetachedPageMenu` null object -- created, never shown -- keeps it valid
  when no window is active.  That is not tidiness: only 2 of the 77 `mPageMenuBar->` sites
  null-check, so a nullable pointer would have been a crash surface.
- The main chrome no longer reserves a row for the page menu; the workspace takes that height.
  `mMenuBar` (File / Edit / Patterns / View / Options / Help) is a DIFFERENT component and stays
  in the chrome, which is what the plan's "transport bar + main menu stay in main chrome" means.
- Gate green both configs, zero errors.

### TS4 status against the plan's checklist (audited, not asserted)

| Item | State |
|------|-------|
| 1. Window frame family | Native child + close + resize border + constrainer + persistence + clamp DONE; **merged page menu DONE this session**; real floors pending Jeff's B.31.0 numbers |
| 2. Main window fixed fullscreen | DONE |
| 3. Destroy-on-close + CL-060 | **RESOLVED AS OPTION (d)**, Jeff 2026-07-28, on measured evidence: page destruction stays OFF, and the repeating UI cost is peer-keyed off instead (MixerPage vblank + 30 Hz poll, Effects + Builder timers).  The CPU dividend the plan assumed was not there -- a maxed-out project measured ~30% with the window-close delta unmeasurable.  CL-060's LAZY half is delivered for launch (only Builder + Mixer frame); its lazy-at-PROJECT-LOAD half is blocked on the layout batch's open/closed persistence decision, and its PARALLEL half is untouched. |
| 4. Per-window keyboard/command routing | DONE -- mapping set + typing-note gate registered per window, in that order (reverse-dispatch rule) |
| 8. Resize containment | DONE -- added after Jeff found the resize path unclamped; drag and resize are separate paths and only drag was covered |
| 5. Tab bar "+" | Dynamic slots + "+" + empty-state retirement DONE; populated-tab dropdowns / 0-badge / CL-101 seam unverified |
| 6. Windows for Builder / Piano Roll / Mixer | DONE (hosted like every page) |
| 7. CL-087 promotion | TS8's job |

## 2026-07-28 — Held for the NEXT batch: page-layout review under the windowed shell

Jeff's ruling 2026-07-28: the contained-window shell changes what a page's layout has to fit
into, so every page gets a layout review — and that review is a SEPARATE batch running directly
after QA-ModelShell, so its fixes do not muddy this batch's work.

**SCOPE CORRECTION (Jeff, 2026-07-28).**  An earlier version of this entry said the batch's scope
"cannot be written before" his B.31.0 sizing pass runs.  That is wrong, and it understates the
batch badly.  His words: the layout batch is "gonna be all encompassing of not just how the windows
look but how everything looks now that windows are a thing."  So it covers the app's WHOLE
appearance under the windowed shell -- every page, every overlay, every piece of chrome, not just
the window frames -- and B.31.0 (Test Plans §B.31.0) is ONE INPUT to it rather than the thing that
defines it.  Do not treat the sizing numbers as a gate on writing the scope.

Known going in:

- **Move the preset dropdown and any engine pickers onto the window TITLE BAR** (Jeff, explicit).
  Rationale is space: the title strip already exists per window and currently carries only the
  page menu + close button, while the preset combo and "Engine: [Select engine...]" picker eat
  full-width rows out of the page's own content area.  Relocating them reclaims that height for
  the layout -- which matters more now that a page is sized to a window rather than to the whole
  content rect.
- **Harmless does not fit at maximum window size**, and some of its knobs render LARGER than they
  did pre-shell (Jeff, observed 2026-07-28 with the window maximised).  Likely the same root
  cause: these pages were written assuming they get the FULL content rect, and now receive a
  window's content area minus chrome, so anything laying out from fixed offsets rather than
  proportionally drifts.  NOT investigated -- deliberately left for the review.
- Every other page needs the same pass; Harmless is just the one that surfaced first.
- **Audit every drawn overlay for the child-peer z-order trap** (Jeff, 2026-07-28, added after it
  bit twice in one day).  With the windowed shell, anything that is a DRAWN component parented to
  the editor and expected to cover the workspace is now invisible behind the contained windows,
  because a native child peer always renders above whatever is painted into its parent's client
  area.  `setAlwaysOnTop` does NOT help -- it orders drawn siblings among themselves and does not
  cross the drawn/native boundary.  Two instances already: the per-window tooltips, and
  `HeavyOperationOverlay` (Jeff: "I don't see the popup for loading").  Both were fixed by giving
  the thing its own desktop window.  By contrast `AlertWindow` / `CallOutBox` / `PopupMenu` were
  never affected, because JUCE already puts those on the desktop as real OS windows -- which is why
  the delete prompts always looked fine.  **The failure is SILENT** -- the component paints
  correctly and is simply never seen -- so this needs a deliberate sweep rather than waiting for a
  third one to surface.  Anything carrying an "always on top" assumption is suspect until checked.

### Window-state persistence — Jeff's ruling 2026-07-28 (added to the held batch)

Prompted by his question about what a project load restores.  Verifying it turned up that the
behavior he assumed does not exist yet, so he specified what it should be.

**THE MODEL IS THREE LIFETIMES, NOT TWO STORES.**  Read on its own, "reopening a window must
restore its spot" and "do not save placement for the players" look contradictory; they are not,
because they describe different durations.  Jeff confirmed the split 2026-07-28.  Getting this
wrong in either direction is the trap here, so it is written out in full:

1. **IN-SESSION -- always, every window, players included.**  Close a window and reopen it and it
   returns to the exact spot and size it was left at.  Nothing touches disk for this; the app just
   has to remember each window's bounds for the life of the session.  This is UNIVERSAL and is not
   qualified by any of the store rules below.
2. **`settings.xml` -- across launches, before any project is loaded.**  See below.
3. **The project file -- across launches, per project.**  See below.

So: "close/reopen returns to the same spot" is universal; "placement survives to DISK" is not.

**IMPLEMENTATION SHAPE -- one map, three write policies.**  Jeff asked whether a temp settings file
written while the app runs would cover the flexible state.  It would work, but it is the wrong
tool and the answer is recorded here so it is not revisited: lifetime 1 needs no file at all.  An
IN-MEMORY map on the editor or the Workspace -- persist key -> bounds, written on move / resize /
close, read on open -- is the whole of it.  Then `settings.xml` is written ON EXIT from a FILTERED
view of that map (sizes for everything, placement for the default tabs only), and the project file
is written on save from the FULL map plus open/closed, and replaces the map on load.

This also removes a live smell rather than adding to it: `saveBounds()` currently parses and
rewrites the ENTIRE `settings.xml` on every window close, and `loadSavedBounds()` re-parses it on
every open.  Disk I/O is standing in for in-memory state, which is a large part of why the
lifetimes are tangled in the first place.

Three reasons the temp file loses to the map: (1) it does not fix the key defect below -- a broken
key is broken in any file; (2) a crash leaves it stale and it needs rules for when it outranks the
real settings; (3) Jeff runs two instances (CLAUDE.md already warns Debug and Release share
`settings.xml`), and a live-written temp file makes that collision worse.  The one thing it would
genuinely buy is LAYOUT SURVIVING A CRASH -- if that is wanted, flush the in-memory map on a timer
into the project autosave rather than standing up a second source of truth.  **Left open: Jeff has
not been asked for a crash-survival ruling and no default was assumed.**

**The two disk stores, different contents, deliberately:**

* **`settings.xml` (global, survives across every project):** how the user has RESIZED all of the
  windows -- SIZE for every window type, players included -- plus the PLACEMENT of the default tabs
  only: Mixer, Builder, Effects, Piano Roll.  Explicitly NOT player PLACEMENT (Layers / Bass /
  Drums / Clip / Vox / Inst).  A player window therefore remembers how big the user likes it and
  NOT where it sat, which is the precise line Jeff drew: those tabs are project content, so a
  position saved from one project means nothing in the next, whereas a preferred size is a habit
  that carries.  The value this store holds is "the size I like my windows" plus "where I keep the
  fixed tabs".
* **The PROJECT FILE:** everything, exactly as the user set it up -- per-instance position, size AND
  open/closed state for every window including the players -- so that opening a project puts them
  straight back where they were.  Jeff: "pull up a project and be right back in it."
  This is the ONLY store that ever holds an individual player window's position.  The global store
  never does; its placement coverage stops at the default tabs, and only as a starting point before
  a project is loaded.  Close/reopen (lifetime 1) is not this store's job -- it works with no
  project at all -- but the bounds it restores are what gets written here on save.

**Current state, measured, so the gap is not re-derived:**

* Only position and size are persisted, into `settings.xml`, via `WorkspaceWindow::saveBounds` /
  `loadSavedBounds` under a `key` attribute.  There is NO saved open/closed flag anywhere.
* `settings.xml` is GLOBAL, so today window positions bleed across projects -- the exact thing the
  split above exists to stop.
* A project load frames EVERY page it recreates (three `hostPageInWindow` call sites in the load
  path), because the launch-only gate has already opened by then.  So a loaded project currently
  opens everything regardless of what the user had closed.

**DEFECT found while verifying this — `StandaloneEditor::persistKeyFor`.**  Its comment states the
correct design and the code does the opposite:

> "Keyed by TYPE + the page's own index rather than the ribbon tab id: tab ids are handed out per
> session, so keying on them would lose a window's saved position every time the project reopened."

...and the body returns `type + ":" + entry.ribbonTabId`.  `PageEntry::pageIndexHint` already exists
and is populated a few lines earlier in `hostPageInWindow` for exactly this purpose, and is never
read.  So saved positions mismatch whenever tab ids come up different -- which is the direct cause
of "reopen does not restore the same spot" surviving as a symptom.

It is NOT a one-line fix, which is why it is held rather than done in QA-ModelShell:
`pageIndexHint` is only filled for Layers, Bass and Drums.  Mixer / Builder / Effects / Piano Roll
are singletons so type alone keys them safely, but **Clip, Vox and Inst are multi-instance and have
no hint** -- they would all collide on one key and share a single saved position.  Correcting the
key means giving those three a stable per-instance index first.  The wrong comment gets fixed at
the same time (it currently documents behavior the code does not have).

### Re-evaluate the instance caps (Jeff, 2026-07-28)

Prompted by a real measurement rather than a guess.  Jeff ran **6 Vox + 20 Inst + 8 Layers +
4 Bass + 1 Rusty** and saw roughly **30% sys and dsp, against a ~10% baseline**.  The significant
part is what that configuration actually is: he was **AT THE HARD CAP on four of the five types**
-- `kMaxVoxPages` 6, `kMaxInstPages` 20, `kMaxLayerPages` 8, `kMaxBassPages` 4 -- with all 16 drum
and all 50 clip slots still unused.  So the most the app currently permits on those four costs 30%,
and the caps look conservative relative to what the machine does with them.

Current values, all in `Source/VibesynthConstants.h` unless noted:

| Type | Cap | Notes |
|------|-----|-------|
| Layers | 8 | channel-id space already reserves 200..215 = **16** -- headroom for 2x with NO id work |
| Bass | 4 | id space reserves 300..315 = **16** -- headroom for 4x with NO id work |
| Drums | 16 | id space 500..515 = 16 -- **at the id limit**, a bump needs id-space work |
| Clips / Audio | 50 | id space 400..449 = 50 -- **at the id limit**; `static_assert` ties it to `kMaxAudioRows` in two places |
| Vox | 6 | must stay equal to `MixerChannelIds::kMaxVoxStrips` |
| Inst | 20 | must stay equal to `kMaxInstStrips`; already bumped 6 -> 10 (G-4) -> 20 (G-6) |
| Aux | 18 | `kMaxAuxStrips`, bumped 16 -> 18 in G-7 |

**HAZARD that has to be handled by whoever raises these.**  The piano-roll target IDs are DERIVED by
summing the caps in order -- `kBassPRTarget = kMaxLayerPages`, `kDrumPRTarget = kMaxLayerPages +
kMaxBassPages`, then Clip, Vox, Inst and Rusty each stacking on the previous.  Changing ANY cap
therefore shifts every downstream target ID.  Pre-v1 that needs no migration (standing rule: no
backward-compat work before v1), but it does mean a cap bump invalidates the PR target IDs in
existing saved projects, and that must be a deliberate call rather than a surprise.

Cheapest first step if this is wanted: Layers 8 -> 16 and Bass 4 -> 16 fit entirely inside the
channel-id space that is already reserved for them.  Drums, Clips, Vox and Inst all need id-range
work first.

**Links to the destroy-on-close decision.**  Per-strip UI cost (a vblank meter drain plus two
listeners per strip) scales linearly with instance count.  It is negligible at 39 instruments --
Jeff's close-all-windows test moved nothing measurable, and the 30% is dominated by DSP, not UI --
but it would not stay negligible if the caps double or quadruple.  So raising the caps raises the
value of stopping hidden pages' timers and vblank attachments, and lowers nothing about the case
against a full page-destruction refactor.

### Rename the Inst entry to "Live Instrument" (Jeff, 2026-07-28)

Held for the layout batch.  "Inst" does not say what the page IS, and it now sits next to
BaySickGuitars / BaySickBasses in the "+" menu, which are ALSO Inst tabs -- so the bare word reads
as a fourth mystery option rather than as "the live-input one".  Rename it to **Live Instrument**.

Scope note for whoever does it: this is a user-facing string, so it is not a single-site change.
`TabType::Inst` is the internal enum and must NOT be renamed with it (persisted state keys off the
enum's integer value).  The sites to sweep are the ribbon tab label, the "+" menu row, the mixer
strip label (`MixerPage.cpp` already says "Instrument Input" -- reconcile to one term rather than
adding a third), and any page title / piano-roll context label that spells "Inst".

Nothing above is fixed in QA-ModelShell.  The review batch calls the issues out; its scope is
authored once Jeff's sizing pass has produced the data.

## 2026-07-28 — TS4 — shell debugging round: five crashes fixed, one defect still open

Jeff ran the shell for the first time.  Everything below came out of that session; the crash
stacks he captured are what made each diagnosis possible rather than guesswork.

### Fixed, with cause

1. **Windows never appeared at all.**  Pages are built in `StandaloneEditor`'s CONSTRUCTOR, which
   runs before the editor is handed to the window -- so there was no HWND to parent to,
   `getNativeParentHandle()` returned null, and `attachTo` skipped its whole `addToDesktop` block.
   Every window was constructed and then silently never attached.  Added a pending queue.
2. **Close button crashed every time** (`~WorkspaceWindow` -> `Workspace::removeWindow`, AV).  The
   button destroyed its own window from INSIDE `juce::Button::sendClickMessage`; `Button::mouseUp`
   keeps running after the callback returns, on a button that no longer exists.  Close is now
   deferred through `MessageManager::callAsync`.
3. **`jlimit` crash while dragging.**  The containment clamp could hand `jlimit` a lower bound
   ABOVE its upper bound (any window wider than the workspace minus the keep-visible margin) and
   `jlimit` has no defence against an inverted range.  Replaced with an explicitly ordered clamp
   shared by the drag and relayout paths.
4. **Paint crash** (`drawLinearSlider` -> `coordsToRectangle`).  The relayout clamp could shrink a
   window BELOW its own minimum, producing a NEGATIVE content area; the first slider that painted
   into it died.  The floor is now applied after the fit, so a negative rect cannot be produced.
5. **`MixerPage::removeLayerChannel` reading 0x8.**  Destroying a page dangles the editor's CACHED
   RAW POINTERS into it -- `mMixerPage`, `mBuilderPage`, `mEffectsPage`, `mPianoRollPage`, and the
   three `mLegacy*`.  Closing the Mixer window left `mMixerPage` pointing at freed memory and the
   next teardown call read a destroyed `std::map`.  **`mMixerPage` alone is dereferenced 103 times
   with essentially no null guards**, so nulling the pointer is NOT a fix -- those call sites have
   to stop assuming the page exists.  **PAGE DESTRUCTION IS THEREFORE OFF**: closing a window
   destroys the window and its child components but keeps the page.  The CPU dividend is not yet
   claimed.  Re-enable per type only after that type's cached pointers are made safe.

Also fixed in the same round: **tooltips** (each contained window needs its OWN
`juce::TooltipWindow` -- it only monitors components inside its own desktop window;
`KeyBindsWindow` already had this exact workaround with a comment saying so), **delete-to-zero**
(a stale `count > 1` gate on Layers/Bass/Drums in the ribbon dropdown survived docket 18 and
silently reinstated the old floor -- with the new tab bar that left no way to remove a type),
**bottom-edge drag** (containment now computed in SCREEN space; deriving the workspace origin in
parent-client space needed both conversions to be right and one was not), and **window move**
(the page menu filled the title strip and swallowed every click -- `setInterceptsMouseClicks
(false, true)` lets empty strip area fall through while its buttons still work).

### RULED OUT -- do not re-test these

- **`ResizableBorderComponent` blocking title-strip clicks.**  Its `hitTest` returns false in the
  centre (verified in the vendored JUCE), so it does not consume the drag area.
- **`ComponentDragger`'s arithmetic.**  Sound.  The move was rewritten to apply a SCREEN-SPACE
  DELTA to the captured bounds anyway, because a delta is identical in both coordinate spaces --
  but the dragger was not the bug.
- **Member destruction order as the explanation for the LIVE crashes.**  `mWorkspace` WAS declared
  after `mPages` (destroyed first -- genuinely wrong, and fixed: it is now declared before), but
  that only affects teardown.  It does not explain a crash mid-session.
- **The deferred-attach queue as the explanation for the remaining defect.**  Jeff's full debug log
  shows NEITHER queue message ever printed, so `mPendingAttach` is never non-empty and that path is
  not being taken.  Driving `attachPendingWindows()` from `resized()` was added anyway (harmless,
  correct) but it is NOT the fix.
- **A dying `Workspace` as the explanation.**  The `outlived its Workspace` DBG fires while the app
  is still running, so the object is alive; the WINDOW'S REFERENCE to it is what is bad.

### STILL OPEN -- the one remaining defect

Symptoms, all from one cause: the first window does not appear until its tab is clicked; magnetism
never engages; windows escape the frame.  **Windows created at PAGE-CREATION time have a null
`Workspace` reference; windows created on REOPEN have a valid one** -- Jeff confirmed by closing
and reopening, after which containment and magnetism both worked.  That single fact explains all
three, because `clampToWorkspace` and `applyMagnetism` both early-return on a null workspace and an
unattached window never completes setup.

`mWorkspace` is assigned ONLY in `attachTo`.  The queue is provably unused.  So exactly one of two
assumptions is false: either `attachTo` is not running for those windows, or it IS running with a
non-null parent handle far earlier than should be possible.

**Diagnostic shipped to answer it in one run** (Rule 4 catalog below): a DBG at the top of
`attachTo` printing the persist key, whether the parent handle was null, and the workspace size;
and one in `hostPageInWindow` printing when it SKIPS because the editor's workspace or the page is
null.  Between them the next run says which assumption is wrong.

### Diagnostic Instrumentation Catalog (Rule 4)

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `WorkspaceWindow::workspace()` | `[TS4 SHELL]` | One-shot warn when a window's Workspace ref is dead -- containment/magnetism silently off | Remove at TS4 close |
| `Workspace::attachPendingWindows` (x2) | `[TS4 SHELL]` | Whether the pending queue drains or stalls with no peer | Remove at TS4 close |
| `WorkspaceWindow::attachTo` | `[TS4 SHELL]` | Persist key + parent-handle state + workspace size at attach | Remove at TS4 close |
| `StandaloneEditor::hostPageInWindow` | `[TS4 SHELL]` | Why a page was not framed (null workspace vs null page) | Remove at TS4 close |

### Not a code bug, recorded so it is not re-diagnosed

A stray `6` was typed into the vendored `juce_ReferenceCountedObject.h` line 418 while Jeff's
debugger sat on that line -- one character, 1062 compile errors across JUCE core.  Restored with
`git checkout`.  If a build ever fails en masse inside `juce/`, check `git status juce/` FIRST.

Related: an exe-locked link failure can leave STALE OBJECTS, and the next build then reports
`RELEASE_EXIT_CODE=0` while a source file still has a live compile error.  Judge a build by the
error grep AND the `vcxproj -> ....exe` link lines, not the exit code alone.

- **Cursor stays stuck to a dragged window** (Jeff spec 2026-07-28).  Previously the window
  stopped at the workspace edge while the mouse kept travelling, so the pointer slid off the title
  bar and the window stopped tracking the grab point.  `mouseDrag` now measures the difference
  between where the drag WANTED the window and where containment/magnetism actually put it, warps
  the cursor by that correction, and re-baselines the drag anchor by the same amount -- so the next
  event computes the identical position, the correction falls to zero, and the warp cannot feed
  back on itself.

### Correction: the `outlived its Workspace` line was a FALSE POSITIVE

Recorded because it cost a full debug round and would cost another one to anybody reading the log
above at face value.  `WorkspaceWindow::workspace()` warns once, latched, when its Workspace
reference is null.  But `hostPageInWindow` calls `setContentNonOwned` BEFORE `attachTo`, and
setting content triggers a layout -- so the very first call to `workspace()` happens while the
reference is still legitimately unset, the latch is spent there, and the warning never once
described the state it was written to catch.  It fired for healthy windows.

That also kills the framing in the entry above: `attachTo` assigns `mWorkspace` on its FIRST line,
before any early return, so a genuinely null reference at drag time cannot mean the Workspace died
-- it can only mean `attachTo` never ran for that window at all.  `workspace()` now warns only
after an attach has been attempted.

Three facts are now established and should not be re-derived: there is exactly ONE `Workspace`
(created in the editor constructor, a `unique_ptr` member that is never reset), the pending-attach
queue is pumped from BOTH `parentHierarchyChanged` and `resized`, and containment plus magnetism
each early-return their input when the workspace is missing or has no size -- which is why they
fail invisibly rather than misbehaving visibly.

The remaining question -- does a window that escapes the frame have a null workspace, or a valid
one whose size is still 0x0 because the frame had not been laid out when the window attached? --
is now answered directly by two additions: `hostPageInWindow` logs every SUCCESSFUL framing with
the tab id, persist key and the workspace's size at that instant, and `mouseDown` dumps, per drag,
the live workspace size, the workspace and window SCREEN rects, and the sibling count.  One run
distinguishes the two.

### Cursor-pin regression and its fix (same day)

The first cut of the cursor pin shipped broken and Jeff hit it immediately -- the pointer detached
on mouse-down and the window flickered around the screen, hard enough to block testing anything
else.  Cause: it warped the cursor by the containment correction AND re-baselined the drag anchor
by the same correction.  Those cancel exactly.  The synthetic move arrives at `screen + corr` while
the anchor has also moved by `corr`, so the delta -- and therefore the desired position -- is
unchanged, the clamp yields the same correction, and it warps again on every event without end.
The window sat pinned at the edge while the cursor ran away one correction per event.

Warping ALONE converges, in one event: the next event's delta grows by `corr`, so the desired
position becomes the already-clamped one, the correction falls to zero and the warping stops.  The
re-baseline was the entire bug; removing it is the fix.  A per-drag warp cap was added as a safety
valve -- not part of the algorithm, but it means a future mistake in the clamp or the magnet
degrades to "the cursor comes unstuck" rather than "the app is unusable".

Worth stating for the layout batch: magnetism composes with this correctly.  A snap produces a
correction like any other, so the cursor follows the window into the snapped position, and the next
event sees desired == snapped and settles.  Escaping the snap still just means moving further than
kSnapPx, so it remains a soft ramp rather than a lock.

### Root cause found: the Workspace was being created inside a lambda

The debug run settled it, and the cause was a bad edit of mine rather than anything subtle about
JUCE.  `mWorkspace = std::make_unique<Workspace>()` plus its `addAndMakeVisible` had been inserted
INSIDE the body of the `onMixerStripParamsCreated` callback instead of into the constructor.
Confirmed against HEAD: that lambda's original body was the single line
`registerStaticAutomationHandlers();`.

Two consequences, and between them they account for every symptom reported since the shell first
ran:

* **The Workspace did not exist until a mixer strip materialized.**  The log shows tabs 1-5 all
  reporting `hostPageInWindow SKIPPED ... workspace=NULL page=OK` -- those pages were never framed
  at all, which is the "first window does not pop up until I click the tab" report.  Clicking the
  tab re-ran the call later, by which time a strip had been created.
* **It was RE-CREATED on every subsequent strip.**  A `unique_ptr` assignment destroys the previous
  object, so every window already attached to the old Workspace was orphaned -- `[TS4 SHELL]
  WorkspaceWindow '6:6' outlived its Workspace` followed by `drag '6:6' NO WORKSPACE`.  Containment
  and magnetism both early-return without a workspace, so those windows silently escaped the frame
  and could be dragged somewhere unretrievable.

The log also caught the size question directly: `attachTo '6:6' ... wsSize=0x0` versus
`attachTo '7:5' ... wsSize=1534x724`.  A window CAN attach before the workspace is laid out and that
is harmless, because the clamp reads the live size at drag time, not the size at attach.  The
0x0-at-attach reading was a symptom of the late creation, not an independent bug.

Fix: the Workspace is now the FIRST statement of the constructor, so it cannot be missing when a
page is framed and cannot be built twice.  The lambda is restored to its original single line.

Also fixed while the evidence was fresh: `hostPageInWindow` now returns early (bringing the window
to front) when the page is ALREADY framed.  The log showed it running twice for two different tabs
-- several callers invoke it both on page-add and on tab selection -- and the second call was
destroying a live, positioned window to build a fresh one in its place.

### The magnet was a trap, not a ramp

Separate defect, reported the same run: "hard snapping to the outer edge ... takes a couple seconds
to pull it off".  Cause was the interaction between the new cursor pin and magnetism.  The pin
warped the cursor by the FULL correction, magnetism included -- so a small move away from a snapped
edge was pulled back by the magnet AND the pointer was dragged back to match, leaving the next
event starting from the snapped position again.  Escaping required out-running the snap inside a
single mouse event, which at normal polling rates cannot be done by moving slowly.  It read as a
30px-strong lock even though kSnapPx is 10.

The correction is now measured from the SNAPPED position rather than the raw desired one, so only
containment moves the cursor.  Magnetism costs nothing: the window offsets under the pointer by up
to kSnapPx and pulling away just works.  Sibling-to-sibling snapping was already behaving well
("the normal magnet ... did seem to work"), and this does not change it.

### Process note

These edits were made with a scripted string replacement rather than the editing tool, and the
replacement matched a location inside a lambda.  It compiled clean and passed every build gate,
because misplacing a statement into the wrong brace level is a RUNTIME fault, not a syntax one.
A sweep of the whole working diff for the same fingerprint -- an added line indented shallower than
the line above it, inside an open block -- turned up three hits, all benign multi-line
continuations.  Source edits go through the editing tool from here.

### Empty title strips at launch, and the launch-open policy

Two items from the next run, both in the same area.

**Title-strip buttons missing on every window framed at launch.**  `showPageForTab` is the function
that points the editor's page-menu pointer at a window's own title-strip menu and then builds its
contents -- tab slots, MID/SIDE, menu builders, the extra right-hand components.  It runs for the
ONE tab that gets selected.  Every other window framed during construction therefore came up with a
bare strip, and closing and reopening it fixed the display because reopening routes through tab
selection.  Fixed by walking the already-framed pages at the end of the constructor and running
that configuration for each, immediately before the startup tab selection -- which runs last and so
still leaves the active window in front and the menu pointer on it.  The walk deliberately skips
pages with no window, so it does not defeat the policy below.

**Launch now opens the Builder grid and the Mixer only** (Jeff, 2026-07-28).  Previously every
statically-created page was framed as it was built, so the Effects and Piano Roll windows came up
uninvited.  `hostPageInWindow` now declines to frame anything but those two until the constructor
finishes; the rest frame the first time their tab is selected, which `showPageForTab` already
handles for a page whose window is null.  The startup selection is Builder (tab id 3), which is in
the open set, so it costs no extra window.

Left deliberately undecided: what should happen on PROJECT LOAD, which recreates pages after
startup and would therefore frame all of them under the current rule.  That is a spec call, not a
default to invent.

### Per-window key routing, and resize containment

**Key routing.**  A contained window is its own desktop component, so a key press inside it bubbles
up to that window and stops -- it never reaches the editor.  Every global binding (transport, undo,
the typing keyboard) was therefore dead in every window except the frame itself.  Each window now
gets the editor's `KeyPressMappingSet` and the editor's own KeyListener registered on it, which is
the same fix the History window already used for the same reason.

ORDER IS LOAD-BEARING and deliberately mirrors the editor's constructor: the mapping set is
registered FIRST and the typing-note gate LAST.  `ComponentPeer::handleKeyPress` iterates a
component's key listeners in REVERSE registration order, so last registered outranks -- and the bare
note letters have to outrank the letter command bindings they collide with.  Registering these two
the other way round would silently break typing-keyboard notes in every window, with no compile or
runtime error to show for it.

Lifetime is safe in the direction that matters: the window holds pointers to the listeners, and the
window dies first.  `Component`'s destructor frees its listener array without calling anything on
the listeners, so even the reverse order would not fault.

**Resize containment** (Jeff, 2026-07-28: "it still lets you stretch the box beyond the border").
The drag path and the resize path are DIFFERENT paths -- mouseDrag clamps, but a resize goes through
`ResizableBorderComponent` -> the bounds constrainer, which knew nothing about the workspace.  The
constrainer is now a small subclass overriding `applyBoundsToComponent`, routing through a new
`clampResizeToWorkspace`.

That clamp TRIMS THE EDGES that went outside rather than sliding the whole window back in the way
the drag clamp does: during a resize the user is dragging ONE edge, and moving the opposite edge to
compensate is not what that gesture means -- the dragged edge should simply stop.  Every bound is
built from an explicit jmax/jmin pair rather than jlimit, because a workspace smaller than a
window's own floor would hand jlimit an inverted range, which is the crash this file already took
once.  Precedence is floor over trim, workspace over floor, so a window can neither be squeezed to
a negative content area nor end up bigger than the region containing it.

The constrainer was also moved to be declared BEFORE `mResizer`: the resizer holds a raw pointer to
it and members destruct in reverse declaration order, so the pointed-at object must be declared
first in order to outlive the thing pointing at it.

### Destroy-on-close resolved as option (d): peer-keyed UI suspend

Jeff's ruling 2026-07-28, taken after measurement rather than from the batch plan's assumption.  The
plan justified full page destruction by a CPU dividend that turned out not to be there: he ran
**6 Vox + 20 Inst + 8 Layers + 4 Bass + 1 Rusty -- the hard cap on four of those five types -- at
about 30% sys and dsp against a ~10% baseline**, and closing every window moved nothing measurable.
Spending a few hundred call sites of dangling-pointer risk (mMixerPage alone is dereferenced 104
times) to reclaim something that does not register is a bad trade.  **PAGE DESTRUCTION STAYS OFF.**

**Correction to the previous entry, which overstated the problem.**  Three of the four vblank owners
-- `VUMeter`, `DBFSMeter` and `SlotComponent` -- ALREADY suspend themselves, each with a
`parentHierarchyChanged()` keyed on `getPeer() != nullptr`.  So the per-strip meter attachments and
the effect-slot attachments were never the leak; they stop the moment a window closes.  Only
`MixerPage`'s own vblank (the per-strip drain loop) and three page timers were unmanaged.

That also meant the right implementation was NOT a new suspend interface with a tree walk, which is
what was first sketched.  The convention already existed three times over, so the fix follows it:

* `MixerPage` -- vblank attachment AND the 30 Hz page poll, both now peer-keyed.
* `EffectsPage` -- 30 Hz poll, peer-keyed.
* `BuilderPage` -- 30 Hz animation poll, peer-keyed.  Deliberately kept SEPARATE from its existing
  `visibilityChanged`, which manages a key listener: different concern, different trigger, and
  merging them would tie key routing to peer state for no reason.
* `PianoRollContainer` -- left alone on purpose.  It runs at 5 Hz (`startTimer(200)` is 200 ms, not
  200 Hz -- an easy misread) and its callback already early-outs on `isShowing()`, so there is
  nothing to reclaim and a hook would be pure churn.

**The constructors no longer start these.**  Starting a timer or attachment in a page constructor
would run it for a page that is built but never framed -- which is now a NORMAL state, since windows
open lazily and `parentHierarchyChanged` never fires for a component that is never parented.  The
hook owns the lifecycle start to finish.

Why this is safe, verified in the code rather than assumed: the audio thread CAS-maxes into the node
peak and stores it into the public atomic every block, whether or not a UI reader exists, so
suspending the reader removes message-thread work only -- audio playback and player-page adjustments
are untouched.  Every meter path is a plain atomic exchange with no ring buffer or queue behind it,
so nothing accumulates a backlog and spikes on reopen (a failure mode this codebase has hit before,
and the reason it was checked).  On reopen the atomics already carry the current block's values, so
meters are live within one block; the only loss is history for the period nobody was watching --
a flat stretch in the scrolling RMS and a peak-hold that restarts from current.

**Scaling note, which is the real reason this was worth doing at all:** per-strip UI cost scales
linearly with instance count.  It is negligible at 39 instruments.  It would not stay negligible if
the caps double or quadruple, which is live (see the cap re-evaluation entry).  This does not make
raising the caps easier to BUILD -- the channel-id work and the PRTarget-shift hazard are unchanged
-- it makes a raised cap cheaper to RUN.

### CL-060's PARALLEL half -- assessed, not built

The lazy half is delivered (launch frames only Builder + Mixer; everything else frames on first tab
selection).  The parallel half was assessed before touching it, and it is not the small item the
plan's one-line entry implies.

**Where project-load time actually goes.**  The restore path walks saved tabs one at a time,
creating each page and then calling `applyEngineState`.  The heavy work inside that is
`mSfizz->loadSfzFile(...)` -- a BLOCKING call, once per sfizz engine, in
`BaySickGuitarsProcessor`, `BaySickBassesProcessor` and `BaySickRustyDrumsProcessor`.  Jeff's own
test project has 20 Inst tabs, so that is 20 sequential SFZ parses plus sample loading on the
message thread, which is why the load overlay steps tab-by-tab.

**Why it cannot simply be threaded.**  JUCE components must be constructed on the message thread, so
the PAGE half is not parallelizable at all.  The SFZ load half is pure data work and genuinely
could move to a pool -- but `loadSfzFile` mutates the sfizz Synth object, and by the time it runs
the engine may already be in the audio graph.  Doing it safely means restructuring the engine
lifecycle: construct the engine, keep it OUT of the graph, load the SFZ on a pool thread, and splice
it in on the message thread once loading completes.  That is a real change to engine construction
ordering -- the area TS1's EngineRig now owns, and the riskiest area in the codebase to get wrong.

**So the shape of the decision is:** the remaining win is load TIME only, the work touches
audio-graph splicing, and nobody has yet said load time is a problem.  Surfaced to Jeff rather than
either building it speculatively or quietly dropping it.

### Loading readout: bar + percent + running ticker; and the missing live-Inst route

**Measurement that prompted it** (Jeff, 2026-07-28): a project with 8 Layers, 4 Bass, 6 Vox,
6 BaySickGuitars, 4 BaySickBasses, 10 Inst, a Rusty setup and a full 16-drum kit took **23 seconds
to load**.  Starting a new project from it took 4 seconds, "the majority of which was the app
tearing down all the windows."

That second number is the useful one, because it bounds the first.  If tearing down ~30 windows
costs a few seconds, BUILDING them costs the same order -- so of the 23 s, windows are a small
minority and the bulk is ENGINE and SAMPLE loading.  Which means lazy windows at load would barely
move it: an engine has to load whether or not anyone opens its page, because audio must work.  The
only change that would actually cut 23 s is parallelising the SFZ loads, and the only change that
fixes the EXPERIENCE is telling the user what is happening.  Jeff chose the latter.

**What was already there.**  `HeavyOperationOverlay` already existed with `beginOp` /
`setStep(i, n, label)` / `setStepLabel`, a determinate progress bar, and -- the part that makes it
work at all -- a synchronous paint pump (`ComponentPeer::performAnyPendingRepaintsNow`) on every
state change, because these ops hold the message thread and a plain `repaint()` would never reach
the screen until the freeze ended.  So this was coverage and detail, not a new feature.

**Added:**

* **Percent**, right-aligned on the title row, and ONLY when the op is determinate -- an
  indeterminate op has no honest number and inventing one is worse than showing none.
* **A running ticker** -- the tail of everything loaded so far under the current step, oldest
  faintest, so a long load reads as motion rather than as a hang.  Bounded at 64 retained / 8 drawn
  (a many-tab project would otherwise grow it unbounded), blanks and consecutive repeats skipped
  (several call sites re-set the same label, and a ticker that repeats itself reads as stuck), and
  the tail deliberately EXCLUDES the newest entry because the current step is already drawn above
  it.  Panel height now grows with the ticker so short ops still get a compact box.
* **Reporting at the actual cost centre.**  The SFZ parse + sample load is the longest blocking step
  in a load and runs once per sfizz tab.  All three kit loads route through `VibeSynthProcessor`,
  which already owns `onLoadProgress`, so that hook is now fired immediately BEFORE each
  `loadKit` -- before, not after, since the overlay pumps paint on every state change and the label
  has to reach the screen ahead of the freeze.  Engine name + instance number + kit name.

**Separate defect, same report: the "+" menu had no live-instrument route.**  It offered
BaySickGuitars and BaySickBasses (both of which are Inst tabs) but no way to create a plain
live-input Inst tab -- the direct counterpart of BaySickVocal -> Vox.  Added as `BaySickPedals`,
named for its engine like every other row in that menu, and gated on the SAME `onIsInstCapReached`
check as the other two, because all three share the Inst cap.  That required giving the engine-row
table an `enabled` field; single-target rows previously had no way to be disabled.

### The load overlay was invisible -- and it is a CLASS of bug, not a one-off

Jeff, 2026-07-28: "I don't see the popup for loading."  It was being drawn the whole time, just
underneath everything.

`HeavyOperationOverlay` is a DRAWN JUCE component parented to the editor, with `setAlwaysOnTop` --
which only orders it among its drawn siblings.  The contained windows are NATIVE CHILD PEERS, and an
OS window always renders above anything painted into its parent's client area.  z-order between an
OS window and a drawn component is not something the OS can express, which is exactly the reasoning
recorded in `WorkspaceWindow.h` for making the windows child peers in the first place (so hosted
VST3 editors could not permanently obscure our frames).  The same property that solves that problem
buries every drawn overlay in the editor.

Fixed by making the overlay actually BE a window while it shows: the outermost `beginOp` promotes it
to its own always-on-top desktop window covering the editor's SCREEN bounds (desktop windows are
screen-relative -- unlike the contained windows, which are child peers in parent-client space), and
the matching `endOp` returns it to being an ordinary hidden child so the editor's teardown owns it
exactly as before.  `windowIsTemporary` keeps it off the taskbar and stops it becoming the app's main
window mid-load.  `parentSizeChanged` now only applies while it is a child, since on the desktop its
bounds are set by the promotion.

**THE GENERAL RULE, because this will happen again:** with the windowed shell, ANY drawn overlay that
lives in the editor and is expected to cover the workspace is now invisible behind the contained
windows.  `setAlwaysOnTop` does not help -- it does not cross the drawn/native boundary.  Such a
thing must be promoted to its own desktop window.  This is the second instance already: the tooltips
needed the same treatment for the same underlying reason (a `TooltipWindow` only monitors components
in its own desktop window).  Anything else that assumed "child of the editor + always-on-top =
covers everything" should be audited against this before it is trusted -- the failure is SILENT,
since the component paints normally and is simply never seen.

### The load overlay's paint pump was a no-op under Direct2D

Jeff: the panel appeared but "just sits at 0% while everything loads and never says any files."  The
z-order fix made it visible; this is why it was still frozen -- and it turns out the overlay has
never worked, before this batch or after.

`HeavyOperationOverlay`'s whole premise is that these ops hold the MESSAGE THREAD, so a plain
`repaint()` never reaches the screen until the op finishes -- and it therefore calls
`ComponentPeer::performAnyPendingRepaintsNow()` on every state change to force the paint through.
Verified in the vendored JUCE 8.0.12 (`juce_Windowing_windows.cpp`), there are two render contexts
and they do NOT agree:

* `GDIRenderContext` (:4897) -- implements it properly: invalidates the deferred rects, then pumps
  `WM_PAINT` messages.
* `D2DRenderContext` (:5214) -- **`void performAnyPendingRepaintsNow() override {}`.  Empty.**

JUCE 8 defaults to Direct2D (the "JUCE D2D swap chain thread" lines in any debug run confirm it), so
the pump has been doing nothing.  The panel paints ONCE -- when the OS first shows the window -- and
then sits at whatever state it had for the entire freeze.  Nothing errors and nothing logs; the
component paints correctly and simply never reaches the screen again.

Fixed by switching the OVERLAY'S OWN PEER to the Software Renderer immediately after
`addToDesktop`.  The panel is flat 2D with no images or effects, so software rendering costs
nothing, and it is the only surface in the app that must paint while the message thread is blocked
-- every other surface repaints normally on the message loop and keeps D2D.  The engine is selected
by NAME (`"Software Renderer"`, matched against `getAvailableRenderingEngines()`) rather than by
index, because the order of `contextDescriptorList` is an implementation detail a JUCE update could
reorder and picking the wrong one fails silently.

**Standing consequence:** any future "show progress while the message thread is blocked" surface has
the same requirement.  Under D2D the synchronous pump is unavailable, so such a surface must own its
own peer AND select the software renderer on it.  The alternative -- moving the heavy work off the
message thread -- is the real fix and is what CL-060's parallel half would begin.

### DROPPED: CL-060's parallel-page-restoration half (Jeff, 2026-07-28)

Recorded as an explicit removal, not a quiet omission.  **Jeff's words: "as for the parallel half
lets just drop that."**  This was TS4 checklist item (6) in the batch plan and is hereby OUT of
QA-ModelShell, and out of the immediate plan entirely -- it is not being re-routed to the layout
batch or to any other batch.

What is dropped: parallelising project-load work, specifically moving the per-tab SFZ parse and
sample load off the message thread.  What is NOT dropped and IS shipped: CL-060's LAZY half at
launch -- only the Builder grid and the Mixer frame at startup, everything else frames the first
time its tab is selected.

Why it was droppable.  The measured load was 23 s on a project with 8 Layers, 4 Bass, 6 Vox,
6 Guitars, 4 Basses, 10 Inst, a Rusty setup and a 16-drum kit.  The bulk of that is the blocking
`loadKit` call once per sfizz tab.  Parallelising it would mean restructuring engine construction --
build the engine, keep it OUT of the audio graph, load the SFZ on a pool thread, splice it in on the
message thread when done -- inside the area TS1 has just rebuilt, which is the riskiest code in the
project to get wrong, for a win that is load TIME only.  The loading readout shipped this session
addresses the actual complaint (a silent multi-second freeze with no explanation) at a fraction of
the risk.

Consequence to handle at batch close: `Future State.md` still carries CL-060 with both halves.  Its
entry needs updating to reflect that the lazy half shipped in QA-ModelShell and the parallel half was
dropped by owner ruling -- routed per Main Plan §0 Rule 3 rather than edited here.

## 2026-07-28 — /doctor run: CLAUDE.md, memory and settings audit (NOT batch work)

Run at Jeff's request mid-TS4, between the TS4 build items and the commit gate.  Logged here
because it leaves REPO changes that the TS4 commit gate has to disposition, and because one of its
outcomes is a governing-rule ruling.

**Repo changes (uncommitted, will appear in the TS4 git status):**

* `CLAUDE.md` 44,814 -> 22,525 chars (~11.1k -> ~5.6k est resident tokens).  Deleted `## Source
  Layout` (92 lines; it was missing TEN of the sixteen `Source/` subdirectories -- every engine
  family added since April, plus WorkspaceWindow / EngineRig / EffectParamMap / AppPaths) and
  `## Completed Work` (95 lines, frozen at Phase D in April, duplicating `Previously
  Implemented.md`).  `## Next Steps` rewritten: it had said "Next batch: QA-Md" for nearly three
  months after QA-Md closed, so the replacement carries an explicit instruction NOT to record a
  current position in that file at all.  Removed the `/draft-commit` table row (three other lines
  said to skip it, and no such command file exists) and corrected both agent counts.
* `.claude/skills/apvts-reference/SKILL.md` NEW -- `## APVTS Parameter IDs` moved out of CLAUDE.md
  into a lazily-loaded skill (~1k tokens off every session; content unchanged).
* `.claude/agents/commit-drafter.md` DELETED -- retired by Rule 9; the user-scope copy remains.

**GOVERNING RULE SETTLED -- commit policy.**  Two memory files gave OPPOSITE instructions: one said
commit at verified checkpoints without asking (and explicitly claimed to override the
never-commit-unasked default), the other said always surface and wait.  Jeff's ruling: **never
commit unprompted -- always surface the brief Rule 9 message plus the FULL git status with
per-entry dispositions, and commit only on his explicit approval.**  The CADENCE is a SEPARATE
per-batch decision he sets ("some I've said commit once at the end of the batch and some at the end
of each task") -- read the batch's own rules for which is in force and never carry the previous
batch's cadence forward.  CLAUDE.md's Git Commit Mechanics section already matched this, so no
reconciliation sweep was needed there.

**Memory hygiene** (84 -> 83 files, outside the repo): every file now has a `description` -- seven
had NO frontmatter at all and so may never have surfaced on recall, including the no-mid-task-commits
and full-git-status rules.  `feedback_approval_is_not_a_go_signal`'s description claimed "Jeff runs
manual mode", which is FALSE and had already misled a session into treating it as a statement about
permission settings; his original words were about PLAN approval in ExitPlanMode.  Two files renamed
(one whose FILENAME asserted the opposite of current truth), one obsolete file deleted.

**Settings** (outside the repo): auto permission mode set as the user-scope default, `github` plugin
disabled (0 uses since install), the stale npm-global CLI duplicate removed, and 6 of 16 permission
allow-rules pruned -- four dead one-offs plus BOTH staging rules (`git add *`, which as a prefix
wildcard also matched `git add -A`, and `git -C ... add -u`).  No staging is pre-approved now, which
matches the stage-by-name rule.

Nothing here touches BaySickDAW source or DSP.  The three repo entries above need a disposition line
at the TS4 commit gate like any other working-tree change.

## 2026-07-29 — TS5 — scout, then a SPEC PIVOT: the Effects surface splits into many windows

- **Scout first (the surface as it stood):** `EffectsPage` = channel combo + FX Bypass + Meters in
  the title strip, three sub-tabs (Pre EQ8 / Rack / Post EQ8), the Rack tab stacking six
  `SlotComponent`s each carrying a header strip (bypass dot, name, Basic/Advanced, Preset, Mode, SC,
  up/down/close) plus an inline panel.  `resolveChannelDsp` / `rackForChannelId` /
  `channelPrefixForId` were already static and view-free from TS1/TS3, so the plan's "do NOT fork a
  second channel switch" was satisfiable by construction -- EXCEPT for the pre-rack EQ, whose
  resolution was still an inline switch inside `onChannelChanged`.  That one is now
  `EffectsPage::preEqForChannelId`, and the EQ windows use it.
- **`SlotComponent` is shared with BaySickVocal's locked Vocal Chain** (6 slots, inline editors), so
  it could not simply be repurposed into a row.  Everything TS5 adds to it is additive: a
  `Presentation` enum (Inline stays the vocal chain's; PanelOnly is the new per-effect window), the
  menus promoted from private to public so a window's title-bar menu can drive them, and a static
  `showEffectPickerMenu` so the rack rows offer the identical grouped list without a second copy.
- **BLU-499 was NOT answered as posed, and correctly so.**  Jeff declined the placement question
  because the restructure below changes what the question means: with each panel in its own window,
  the panel's preset menu lands on that window's title bar next to Mode / SC / Basic-Advanced
  (his answer 3=a), and the RACK gets its own Save/Load FX Rack Preset menu, which is a new thing
  BLU-499 never covered.
- **CL-299 = option (d):** items 1, 2 and 4 ship (feedback-ring warning above 100%, FB-distortion
  transfer-curve graph, reference model-selector display order).  Item 3 (step-denominated Time knob
  + right-click musical-value list) is DROPPED -- the BPM toggle + 8-division chickenhead stay
  exactly as they are.  Recorded as an explicit option removal so it is not re-proposed: the
  reference delta stands accepted.

### The locked TS5 design (Jeff's spec, 2026-07-29)

The Effects surface stops being one page that shows everything and becomes a small INDEX window that
opens the rest.  His words: "a user can choose what they are editing at a time instead of everything
all at once, this will also allow for more functionality when we get to the layout batch."

* **Rack window** (the Effects ribbon tab): strip picker at the top, then Pre EQ / Post EQ buttons,
  then six slot rows.  Row = bypass LED, the effect NAME AS A BUTTON that opens that effect's own
  window, then up/down, a picker chevron, and a remove X that prompts first.  Title bar carries
  Save / Load FX Rack Preset (all six slots + both EQs).
* **EQ windows:** one for Pre, one for Post, each fixed to its own EQ; the two-tab strip on the title
  bar OPENS THE OTHER WINDOW rather than swapping contents, so both can sit on screen.
* **Panel windows:** one per effect, opened from its row, closed and positioned independently.

Five follow-on questions were posed as a docket and answered: (1=b) satellite windows STAY OPEN when
the rack window's strip picker changes, each titled with its strip; (2=b) up/down arrows on the row,
between the name and the two buttons; (3=a) Basic/Advanced + Mode + SC + Presets all in the panel
window's title-bar MENU; (4=a) removal still packs the slots up; (5) placement memory = in-session
only.

**The LED, corrected by Jeff:** I described the panels as having no bypass control and offered to
give them one.  Wrong framing -- the green/red dot on the slot header IS the LED he meant.  It now
appears in both places (row and panel window) from ONE drawing routine, `EffectBypassLed::paint`,
lifted verbatim out of `SlotComponent::paint`.

**Why (5) is not can-kicking, recorded because he asked directly.**  `settings.xml` is the wrong
store for these: they are addressed per STRIP and per SLOT, which makes their placement project
content by the same rule that keeps player-window positions out of the global file -- writing them
there would bleed one project's effect layout into the next.  And `saveBounds()` already parses and
rewrites the whole settings file on every window close, which is the smell the layout batch exists
to remove; adding eight more windows per strip to that path makes the cleanup worse.  The in-memory
map IS lifetime 1 of the three-lifetime model he specced, so building it now is a down payment.
`WorkspaceWindow::Persistence::Session` is that map.

### Two defects found while building, both fixed rather than routed

1. **`SlotComponent::remountEditor` dropped the automation stamps and the variant.**  It rebuilt the
   panel and nothing else -- so a Mode switch (Compressor Modern -> FET) or a preset load that
   changed Type produced a panel with NO paramId stamps, while the slot's registered applicators kept
   the variant captured at the ORIGINAL registration.  Every lane then applied through the wrong
   table: exactly the collision class TS3's `(type, variant)` key exists to prevent, reachable from a
   shipped menu.  Fixed with a `SlotComponent::onEditorMounted` hook fired at the end of every
   `setEditor`, and one shared `EffectsPage::stampAndRegisterSlotEditor` that both stamps and
   re-registers -- so every mount path is now equivalent.
2. **The panel window has to follow its effect by uuid, not by index.**  Removal packs the slots up
   and reorder swaps them, both of which move an effect between indices.  `SlotComponent`'s slot
   index was fixed for life, so a window would have started driving whatever effect moved into the
   old index.  New `setSlotIndex` + a per-poll re-resolve by uuid; a rack pointer that changes
   identity (project load) forces a full rebuild.

## 2026-07-29 — TS5 — built: the rack window, the satellites, the preset, the Delay deltas

- **What landed, by file.**  `EffectsPage` rebuilt as the rack window (picker + FX Bypass, Pre/Post
  EQ buttons, six `RackSlotRow`s); new `EffectWindows.h/.cpp` (`EffectSlotWindow`, `EffectEqWindow`);
  new `FxRackPresetIO.h/.cpp`; `SlotComponent` gained the `PanelOnly` presentation, public menus,
  `showEffectPickerMenu`, `setSlotIndex` and `onEditorMounted`; `SharedUI` gained
  `EffectBypassLed` + `BypassLedButton` + `TimeLAF::kWarnRingFrom`; `StandaloneEditor` gained the
  satellite-window registry; `WorkspaceWindow` gained session-scoped persistence; `RibbonTabBar`'s
  Effects dropdown split "EQ" into "Pre EQ" / "Post EQ".
- **The three sub-tabs are gone**, and with them `switchTab` / `TabKind` / `tabKindForVisibleIndex` /
  `visibleIndexForTabKind` / `currentChannelHasPagePreEQ` / `setEQMid` / `isEQMidActive` /
  `getEQDisplay` / `getPreEQDisplay` / `onTabsNeedRefresh` / `getActiveTab`, plus the `setupEffectsTabs`
  block in `showPageForTab` and the four title-strip extras it installed.  Every one of those existed
  only to serve the sub-tab strip; the whole family had exactly one consumer.
- **Row glyphs are VECTOR PATHS, not font characters.**  The chevron and the cross have no ASCII
  spelling, and a font glyph for either renders as a box on a machine without it -- these are the
  only affordance a row carries, so they are drawn.
- **The rack window's floor is a real number, not the provisional 640x400.**  Its content is a known
  height (picker + EQ row + six 24 px rows), so `hostPageInWindow` gives Effects 300x250 and every
  other page type keeps the provisional floor until B.31.0.  Jeff asked for smaller than the
  provisional minimum; this is that.
- **Satellite floors are provisional and honest about it:** 620x170 for a panel window (panels cap
  their knobs and shrink below that, so it is "before the knobs collide", not a measurement),
  560x320 for an EQ.  B.31.0 needs rows for both -- it currently has ONE "Effects" row, written
  before the surface became four window kinds.
- **CL-299 shipped as three items.**  (1) The Feed knob's warning ring: opt-in via a slider property
  whose value is the normalized start of the warning zone, drawn as an arc OVER the filmstrip --
  TimeLAF's rotary is filmstrip-rendered and returns early, so there was no ring to recolour and a
  tinted strip would read as a different control.  (2) The FB-distortion transfer curve, input
  vertical / output horizontal per the reference, fed by a new `DelayDSP::shapeFeedbackForDisplay`.
  (3) Model selector display order via `kModelOptionValues`; serialized values untouched.
- **Why the curve has a display twin instead of sharing the audio code.**  The Sat branch hoists
  four invariants out of the per-sample loop, one of them a `tanh`.  Sharing would either recompute
  them per sample on the audio thread or force the loop to be restructured around a prepared-shaper
  object -- live DSP surgery for a picture.  Both copies now carry a pointer to the other and the
  rule to edit them together.
- **Three defects found in my own review, before the gate.**  (a) `EffectSlotWindow::timerCallback`
  destroyed its own window from inside its own timer callback when the target vanished -- the exact
  shape of the TS4 close-button crash; deferred through `callAsync` with a SafePointer.  (b) The EQ
  window never re-resolved its DSP, so a strip respawn would have left it drawing into an orphan;
  it now re-binds when the resolved pointer changes.  (c) The panel menu ignored its anchor.
- **Gate:** both configs green, zero errors, two exe link lines.

## 2026-07-29 — TS5 — the FX rack picker was a near-copy of the pedals picker (Jeff caught it)

- **His report:** the rack's effect list "look[s] like the pedal boards effects list and not the
  main effects list," and he asked directly whether I had used the right list, or deleted the rack's.
- **Verified before answering, both halves.**  (1) Nothing was deleted: the diff of that function
  adds exactly two lines (the Plugins header + the disabled VST3 row) and removes no `addItem`.
  (2) I did use the rack's own list -- `SlotComponent::showAddMenu`, which the rack slots and the
  vocal chain call; the board has its own `BaySickPedalsEditor::buildSwappableMenu`.
- **But he was right about the CONTENT, and this is the real finding.**  Put side by side, the rack
  picker WAS the pedals picker plus three items: identical section names and order, 21 shared
  entries, rack-only extras De-esser / Limiter / Transient Shaper, pedals-only extra "Load NAM
  Pedal".  24 entries, of which 13 were pedal-native `*Style` types and only 11 were rack effects.
  Phase I did that deliberately (I-5 through I-11 alpha-merged each pedal into these groups; I-15
  recorded the four kept out), so it is not a TS5 regression -- but it has read wrong for months and
  the rebuild is the first time anyone looked straight at it.
- **My miss, precisely:** the scout confirmed WHICH FUNCTION the rack slots use and stopped there.
  It never diffed that function's CONTENTS against the pedals menu, which would have shown the
  near-duplication in one pass.  "Reuse the picker logic" made me treat the list as settled.
- **Jeff's rulings (2026-07-29):** (1=b) keep the pedals but demote them into a **"Pedals" submenu**
  so the rack list reads rack-first; (2) **add Gate and De-reverb** to the rack picker.
- **Gate + De-reverb were unreachable.**  Both are QA-Fe2 types (119/120) built as locked
  vocal-chain stages, and they appeared in NO picker -- while having a full DSP (`EffectRack`
  factory cases), a panel (`createEffectEditor` cases) and TS3 automation tables (`kGate`,
  `kDeReverb`).  Verified all three before adding them, so a rack slot can now hold either and
  automate it.  The vocal chain is unaffected: its slots are locked and never open this menu.
- **No project impact from the move.**  Slots load by `EffectType`; the picker is only the route to
  ADD one.  A Fuzz already sitting in a rack slot keeps loading, sounding and automating.

## 2026-07-29 — TS5 — "Pedals" becomes a group HEADING that is itself the dropdown

- **Jeff:** the Pedals entry was landing as an ordinary item under the Time group; it should be
  its own group -- the bigger bold heading font -- with the dropdown hanging off that same line.
- **JUCE cannot do this with a real section header, verified in the vendored source.**
  `ItemComponent`'s constructor swaps a header item's component for its own
  `HeaderItemComponent` and calls `setEnabled (false)` (juce_PopupMenu.cpp:128), and both
  `canBeTriggered` and `hasActiveSubMenu` refuse a disabled item -- so an `addSectionHeader` row
  can never open a submenu, whatever else is set on it.
- **The fix:** `HeaderSubMenuItem`, a `PopupMenu::CustomComponent` that paints through the SAME
  LookAndFeel entry points a real header uses (`drawPopupMenuSectionHeader` +
  `getIdealPopupMenuItemSize` with the LAF's own header-height rule, +50 %), so it renders
  identically to the headings above it under any LAF, while its item carries a real `subMenu`.
  Constructed with `CustomComponent (false)` -- not "triggered automatically" -- so clicking the
  row opens the submenu instead of dismissing the menu as a chosen item would.  The submenu arrow
  is drawn by hand, because a custom component replaces the LAF's own item rendering and without
  it the row would claim to be a heading while giving no sign that it opens.
- **TS6 reuses it verbatim** for the VST Plugins group (BLU-300), which Jeff specced the same way.

## 2026-07-29 — TS6 spec captured from Jeff (recorded now, built next set)

Full text landed in the batch plan's TS6 section so it travels with the plan rather than only the
notes.  Shape: an Options > **Plugins** manager window (three sections -- scan folders seeded with
the default VST3 install locations / the added-plugins list / blank scan results with checkboxes +
an Add button); a **VST Plugins** group in the rack picker built like the Pedals group, listing
added EFFECT plugins alphabetically; a **Plugins** ribbon tab whose "+" entry is a side dropdown
of added INSTRUMENT plugins; and plugin players needing their own strip + bus, with VST strips
routable under the Layers or Bass bus the same way those two already move between each other.

- **His open question answered: yes, the kind is knowable without loading the plugin.**  A scan
  produces a `juce::PluginDescription` per plugin carrying `isInstrument`, derived from the
  category the VST3 declares.  That single flag serves all three of his uses -- the label on the
  added list, the effects-only rack picker group, and the instruments-only Plugins tab.
- **The one ambiguity I flagged was closed the same day.**  Jeff: section 1 "both lists the
  folders chosen and has a button to pull up the 'open' window to select a folder to add" -- so
  section 1 owns the folders (list + add button), section 2 is the added PLUGINS list, section 3
  is the scan result.  Nothing open in the manager-window spec now.

## 2026-07-29 — TS5 — a window only took focus from its title bar

- **Jeff:** with windows overlapping, clicking the BODY of a background window did not raise it --
  only its title bar did.
- **Cause.**  `WorkspaceWindow::mouseDown` is where the raise lived, and a Component only sees
  mouse-downs that actually reach it.  The title strip and the resize border reach it; everything
  inside the hosted page consumes its own clicks, so the window never heard about them.
- **Fix, and it is JUCE's own mechanism rather than a hand-rolled listener.**  On mouse-down JUCE
  builds the clicked component's full ancestor chain and calls `toFront` on every link whose
  `setBroughtToFrontOnMouseClick` flag is set (`Component::internalMouseDown` ->
  `HierarchyChecker::forEach`).  Setting that one flag on `WorkspaceWindow` therefore covers a click
  on a knob six levels down.  Verified in the vendored source rather than assumed.
- **The ribbon sync moved with it.**  `onBroughtToFront` used to fire from `mouseDown`; it now fires
  from a `broughtToFront()` override, which every raise route reaches (the flag, the title drag, and
  programmatic `toFront` from tab selection).  Leaving it in `mouseDown` would have meant a
  content-click raised the window without the tab bar following.  `RibbonTabBar::selectTab` was
  checked first: it sets the id and repaints, fires no callback, so running it per click is cheap
  and cannot recurse into `showPageForTab`.
