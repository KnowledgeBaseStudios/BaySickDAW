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
  to Jeff.  All functional verification stays deferred to TS8's batch smoke per the
  batch plan (nothing here has been ear-tested; the compile gates + the census-derived
  design are the current evidence).

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
