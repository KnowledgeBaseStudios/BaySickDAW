# Carry-Forward Reference — Post-Batch-10 QA Triage

> **STATUS: FROZEN as of 2026-05-07.** This document is the reference snapshot
> captured during the triage session that produced
> [Main Plan.md](Main Plan.md).
> **Never edit.** New findings during execution go into
> [Implemented Work Log.md](Implemented Work Log.md).
> If a section here turns out to be wrong, the implemented-work doc records
> "carry-forward §X said Y, verified to be Z" — this doc stays as the
> historical snapshot of what was true on 2026-05-07.

**How to use this doc:** read §1-§3 minimum at the start of every per-batch
plan. Skim §4-§6 to confirm decisions and patterns. §7-§8 are reference
sections you grep when needed.

## Header conventions

Cross-doc rules live in [Main Plan.md](Main Plan.md) §0 "Document
Formatting Conventions". Local layout for this doc:

- `#` — document title.
- `## §N. <Title>` — top-level numbered section (§1-§9).
- `### <Sub-topic>` — sub-cluster within a section.
- `- <bullet>` — primitive entry (typically a file:line reference,
  decision, or pattern). Inline backticks for symbols and paths.

Grep patterns:

- `^## §` finds all top-level sections.
- `^### ` finds all sub-clusters.
- File:line citations use `[Source/path:line](path)` markdown links
  (relative paths from this doc's location, hence the `../Source/`
  prefix in older entries).

This doc is **frozen as of 2026-05-07**. Add nothing here — log new
findings in [Implemented Work Log.md](Implemented Work Log.md) instead.

---

## §1. MT Render Path Primitives (file:line)

### Flag and toggle

- `RenderEngine::gMultiThreadedEngineEnabled` — `inline std::atomic<bool>`
  declaration: [Source/Engine/RenderEngineFlags.h:44](../Source/Engine/RenderEngineFlags.h)
- Audio-thread acquire-load (decides serial vs MT branch each block):
  [Source/PluginProcessor.cpp:1831](../Source/PluginProcessor.cpp)
- Mixer hamburger toggle handler (release-store + persistence call):
  [Source/Standalone/StandaloneEditor.cpp:4450-4497](../Source/Standalone/StandaloneEditor.cpp)
- settings.xml persistence:
  `VibesynthStandaloneApp::saveMultiCoreRenderingPref()`
  at [Source/Standalone/StandaloneApp.cpp:240-264](../Source/Standalone/StandaloneApp.cpp)
- Persisted load (called before `mDeviceManager->initialise` so first audio
  callback sees correct value): same file, around `loadMultiCoreRenderingPref`.

### Dispatcher and scheduling

- `RenderGraphDispatcher::rebuildLinks()` — runs **every block** (also under
  serial flag=false for "no dead wiring"):
  [Source/Engine/RenderGraphDispatcher.cpp](../Source/Engine/RenderGraphDispatcher.cpp)
- Called from: [Source/PluginProcessor.cpp:1737](../Source/PluginProcessor.cpp)
  (right after `rebuildRoutingFromApvts`).
- `dispatchBlock(const BlockContext&)` — parallel pump under MT: reset
  counters → seed leaves → `mPool.runUntilOrTimeout(mAllDone)` → MasterTask
  publishes done → main thread copies arena master slot to host buffer.
- **Most-recent-registration-wins rule** at `registerTask` — when a second
  registration arrives at the same channel id, the existing one is
  unregistered. **This is the DSP-12 root.** Comment acknowledging the
  unresolved composite case: [Source/PluginProcessor.cpp:4240-4245](../Source/PluginProcessor.cpp).

### BlockContext

- [Source/Engine/BlockContext.h](../Source/Engine/BlockContext.h)
- Carries: `numSamples`, `bpm`, `posInfo`, `anySolo`, `busAnySolo`,
  `panLaw`, 7 per-engine MIDI buffers (layer/bass/drum/clip/vox/inst/rusty),
  `liveInputSnapshot` ref.

### Task subclasses (Source/Engine/Tasks/)

- `EngineInsertTask` — Layer / Bass / Drum. Pushes SC predecessors → engine
  process → `pullSidechainPredecessorsToGraph` → `processInsert`.
- `PassiveStripTask` — Aux / Bus accumulator. Pulls predecessors with edge
  gain → `processInsert(Aux)` or `processBus(Bus)`.
- `MasterTask` — terminal sink. Pulls 11 buses + direct-to-master sends →
  `processMasterBus` → `mDoneFlag.store(release)`.
- `VoxStripTask` / `InstStripTask` — read `mLiveInputSnapshot`; armed-input
  gating; Inst is source-mode aware (LiveInput / BaySickGuitars / BaySickBasses)
  via runtime atomics (no re-registration on source swap).
- `ClipPageTask` / `AudioInsertTask` — **conflict at `audioInsert(N)` channel
  id**. ClipPageTask is sampler-style MIDI-triggered; AudioInsertTask is
  arrangement-timeline pulled from AudioClipStreamer. Both register at the
  same channel id; under MT, most-recent wins — silenced loser produces no
  audio. **DSP-12 root cause.**
- `RustyDrumsProducerTask` + `RustyInsertTask` — special 1-to-13 fan-out
  (one engine call emits 13 strip outputs).

### Helpers

- `pullSidechainPredecessorsToGraph` — inline helper at
  [Source/Engine/SidechainPullHelper.h:42-63](../Source/Engine/SidechainPullHelper.h).
  Consumer-side mirror of `routeInsertOutput`. Called BEFORE
  `processInsert` / `processBus` in tasks.
- `measureDspLoadAndOverload` — [Source/PluginProcessor.cpp:2951-2956](../Source/PluginProcessor.cpp).
  Wall-clock measurement; called from BOTH branches (serial line 2859,
  MT line 1890). Audio-thread-only under MT — sum-of-cores reading is
  DIAG-02 work item (lands as QA-N).
- `drainMeterAtomicsForUI` — [Source/PluginProcessor.cpp:2880](../Source/PluginProcessor.cpp).
  Drains 12 meter atomics on audio thread at end of processBlock. Called
  from BOTH branches (serial line 1880, MT line 2864).

---

## §2. Lock-Free + Lifecycle Primitives (Batch 9c — reuse, do not reinvent)

### AudioClipSnapshot RCU

- Struct definition: [Source/PluginProcessor.h:512-516](../Source/PluginProcessor.h)
  — `struct { std::vector<AudioClipPlayer> players; uint64_t generation; }`.
- Atomic holder: [Source/PluginProcessor.h:539](../Source/PluginProcessor.h)
  — `std::atomic<AudioClipSnapshot*> mActiveAudioClips`.
- Mutator (message thread): `rebuildAudioClipPlayers()` — builds new snapshot,
  `mActiveAudioClips.exchange(newSnap.release(), std::memory_order_acq_rel)`,
  retires old to `mClipRetirement` queue.
- Audio thread (acquires once at top of processBlock):
  `mCurrentBlockClipSnapshot = mActiveAudioClips.load(std::memory_order_acquire)`.
- Same pointer used by: FilePlay scan, Pass 2, applyChokeGroupDispatch,
  AudioInsertTask, VoxStripTask, InstStripTask. **Lock-free worker reads.**

### RetirementQueue<T>

- Template: [Source/Engine/RetirementQueue.h:101-223](../Source/Engine/RetirementQueue.h).
- Generation-stamped retirement queue + dedicated drainer thread (constructor
  spawns `std::thread(drainerLoop)` named "RetirementQueue"; destructor joins
  + unconditionally destroys remaining entries).
- Currently used for `RetirementQueue<AudioClipSnapshot> mClipRetirement` only.
  (`AudioClipStreamer` is owned BY `AudioClipPlayer`, retired transitively.)

### closeAllDynamicTabs

- [Source/Standalone/StandaloneEditor.cpp:8672-8719](../Source/Standalone/StandaloneEditor.cpp).
- Steps: `setProjectLoadInProgress(true)` → 30 ms sleep → close every
  dynamic tab (Layer/Bass/Drum/Inst/Vox/Clip/Rusty) → `mRibbon->clearAllDynamicTabs()`
  → `setProjectLoadInProgress(false)`.
- **First step of `~StandaloneEditor`**: [:1304-1326](../Source/Standalone/StandaloneEditor.cpp).
- **Called before project-OPEN** at [:7670, :8076](../Source/Standalone/StandaloneEditor.cpp)
  (Open Recent + Open Project Browser paths).
- **STATE-04 nuance**: `ProjectManager::openProject()` itself does NOT call
  `StandalonePlayHead::stop()`. The fix is to add that call — barrier
  alone doesn't kill playback, the playhead keeps ticking.

### mProjectLoadInProgress barrier

- [Source/PluginProcessor.h:885-889](../Source/PluginProcessor.h).
- Audio thread acquire-loads at top of processBlock (line ~942), clears
  buffer to silence if true.

### mShuttingDown gate (BaySickVocal only)

- [Source/BaySickVocal/BaySickVocalProcessor.h:170](../Source/BaySickVocal/BaySickVocalProcessor.h)
  — `std::atomic<bool> mShuttingDown { false }`.
- Destructor sets to true (release): [BaySickVocalProcessor.cpp:152](../Source/BaySickVocal/BaySickVocalProcessor.cpp).
- `processBlock` checks acquire at line 282; clears buffer if true.
- **Other engines do NOT have this gate** — they rely on the shared
  `mProjectLoadInProgress` barrier. The pattern is reusable if NAM/IR
  or sfizz engines show similar shutdown crashes.

### Audio settings persistence

- Resolver: `VibesynthStandaloneApp::getAudioSettingsFile()` at
  [Source/Standalone/StandaloneApp.cpp:168-185](../Source/Standalone/StandaloneApp.cpp)
  — path is `Documents/BaySickDAW/audio_settings.xml` (post-P4b migration).
- `AudioSettingsDialog::applySettings` at [StandaloneEditor.cpp:261-321](../Source/Standalone/StandaloneEditor.cpp)
  writes `audio_settings_pending.xml` as a SIBLING of `getAudioSettingsFile()`,
  NOT a hardcoded Roaming path.
- ASIO Control Panel button: NOT WIRED. APP-05 is fully open (single
  call to `juce::AudioIODevice::showControlPanel`).

### Process priority

- `SetPriorityClass` / `HIGH_PRIORITY_CLASS` / `MMCSS`: ZERO usage in
  BaySickDAW code. APP-04 is fully open.

---

## §3. Mixer / Page Lifecycle File:Line Index

### Spawn cascades (MixerPage.cpp)

- `addVoxChannelAtIndex` — :1677
- `addInstChannelAtIndex` — :1999
- `removeVoxChannel` — :2331
- `removeInstChannel` — :2323
- `mVoxStrips` map — header :311
- `mInstStrips` map — header :316

### Tab close dispatch (StandaloneEditor.cpp)

- Vox branch — :3525-3535. **Calls `unregisterVoxEngine()` but NOT
  `removeVoxChannel`.** This is MIX-01 (confirmed open).
- Inst branch — :3631-3632. **Correctly calls the Inst-strip removal.**
  Mirror this pattern for Vox in QA-C.

### Project XML restore walker (StandaloneEditor.cpp)

- Vox tab restore — :6571 → calls `addVoxChannelAtIndex(idx)`.
- Inst tab restore — :6623 → calls `addInstChannelAtIndex(idx)`.
- Spawn calls ARE present, but MIX-02/04/06 still happens — bug is
  downstream of spawn (post-spawn teardown OR guard fail in spawn helpers).
  **Don't fix the spawn; find the teardown.**

### Recording lifecycle (per-armed-strip WAV capture, post-FILE-01)

**StripRecorder** ([Source/PluginProcessor.h:653-666](Source/PluginProcessor.h:653))
- One per armed Vox/Inst strip; per `_arm` APVTS flag scan in `startRecording` ([Source/PluginProcessor.cpp:3463-3539](Source/PluginProcessor.cpp:3463)).
- Vox: dry writer (raw pre-chain ASIO input) + wet writer (post-realtime-pitch BaySickVocalProcessor tap pushed via `setWetRecorder` at [PluginProcessor.cpp:3514-3517](Source/PluginProcessor.cpp:3514)).
- Inst: dry writer only (no realtime stage to bake into a wet capture).

**Tap helpers** ([Source/PluginProcessor.cpp:3581-3625](Source/PluginProcessor.cpp:3581))
- `tapDryRecorder(channelId, monoSource, numSamples)` writes raw mono into the dry file.
- Called from the serial Vox/Inst armed paths in `processBlock` + (under MT flag) from `VoxStripTask` / `InstStripTask`.
- Wet tap: `BaySickVocalProcessor::setWetRecorder(...)` pushed at `startRecording`; engine's `processBlock` writes post-realtime / pre-vocal-chain audio into the wet file.  Cleared BEFORE stopping the writer at `stopRecording` ([PluginProcessor.cpp:3555-3562](Source/PluginProcessor.cpp:3555)) so the audio thread can't push into a stopped recorder.

**Finalize** ([Source/Standalone/StandaloneEditor.cpp:9842-9947](Source/Standalone/StandaloneEditor.cpp:9842))
- `stopRecording()` returns `RecordResult` with `stripFiles` (dry) + `stripWetFiles` (wet, Vox only) maps + master fallback file + MIDI notes + start beat.
- For each **Vox** strip: WET file goes on the grid + library via `dropWavAsClip(wetFile, chId)` ([:9924](Source/Standalone/StandaloneEditor.cpp:9924)); DRY file goes in audioLibrary only (not on grid) via `addAudioToLibrary(dryRel, {}, chId)` ([:9921](Source/Standalone/StandaloneEditor.cpp:9921)) so the BaySickPitch offline editor can load it.  Fallback: if WET capture failed, DRY also goes on the grid.
- For each **Inst** strip: single DRY file → grid + library via `dropWavAsClip(dryFile, chId)` ([:9930](Source/Standalone/StandaloneEditor.cpp:9930)).
- **Master fallback** (no Vox/Inst armed): the master capture lands via `dropWavAsClip(res.masterFile, /*routeChannel=*/0)` ([:9893](Source/Standalone/StandaloneEditor.cpp:9893)) and is treated as an Audio-row clip (auto-spawns a new Audio row + InsertNode + mixer strip).

**File naming convention** ([Source/PluginProcessor.cpp:3498-3510](Source/PluginProcessor.cpp:3498))
- Dry: `<project> - <Vox|Inst> N - <ts> - DRY.wav`
- Wet: `<project> - <Vox|Inst> N - <ts> - WET.wav` (Vox only)
- Master fallback: `<project> - Master - <ts>.wav`
- `<ts>` is Windows-filename-safe `YYYY-MM-DD HH-MM-SS`.
- Filename suffix is the wet/dry tag; browser display falls back to filename when no alias is set.

**Page-binding (post-Task-4 library-driven model)** ([Source/Standalone/StandaloneEditor.cpp:9870 + :9921](Source/Standalone/StandaloneEditor.cpp:9870))
- Recording finalize tags `pageOwnerChannelId` on the library entry via the 3rd arg to `addAudioToLibrary(path, {}, routeChannel)` (DRY entries on Vox) and via `dropWavAsClip(..., chId)` which calls `addAudioToLibrary(..., routeChannel)` internally (WET on Vox, DRY on Inst, master fallback).
- The pre-Task-4 `VoxPage::setDryClipPath` direct-binding API was removed in the FILE-01 rewrite; the library entry's `pageOwnerChannelId` tag is now the single source of truth for "which page does this recording belong to."
- Browser visibility walks the audioLibrary by `pageOwnerChannelId` range at `onEnumerateAudio` ([Source/Standalone/StandaloneEditor.cpp:2216-2310](Source/Standalone/StandaloneEditor.cpp:2216)) — entries are grouped by their owning page's channel id (Vox/Inst/Clips/Audio).

### Builder grid drop & block resize (BuilderPage.cpp)

- File drop handler — `ArrangementGrid::filesDropped` at :2790; same
  `importAudioFile` path for WAV and MP3.
- `importAudioFile` at :2664-2728 — creates `ArrangementBlock` + adds to
  library + fires `onAudioClipAdded`.
- `onAudioClipAdded` callback in StandaloneEditor at :1914-1949 — registers
  audio-row channel + ensureAudioInsert + addAudioChannel + rebuildAudioClipPlayers
  + `spawnClipsTabIfMissing(row, filePath)`. **This is the cascade that
  triggers DSP-12 — both AudioInsertTask AND ClipPageTask end up registered
  at `audioInsert(row)`.**
- Block resize handler `mouseUp` at :3505-3509 — sets `block.lengthBars`,
  clears `lengthBeats=-1.f`, calls `commitEdit() → onArrangementChanged()`.
  **Does NOT call `rebuildAudioClipPlayers()`.** BUILD-06 confirmed.
  `clipStartBeat`/`clipEndBeat` populated at
  [PluginProcessor.cpp:3095-3099](../Source/PluginProcessor.cpp)
  stay stale until next rebuild trigger.

### Right-click → Automate (SharedUI.cpp)

- `VKnob::mouseDown` at :1751-1794. Outer right-click correctly gated by
  `isRightButtonDown()`; opens `juce::PopupMenu` via `showMenuAsync`.
- **UI-01 is JUCE PopupMenu's default behavior** — accepts ANY mouse button
  as item-activation. Right-click on the open menu activates the item.
  Fix needs a wrapper, not a logic change in our handler.

### Automation lane UUID resolver (StandaloneEditor.cpp)

- "(deleted slot)" resolver at :2538-2544.
- Reachable from `sOnAutomate` callback at :2416-2420.
- UI-02 is "auto-lane bound to stale UUID at creation" — diagnose alongside
  UI-01 (likely shared root cause: right-click activation reads stale
  target context that doesn't carry the menu-item's intended target).

### Effects-page channel dropdown (EffectsPage.cpp)

- `onInstrChannelListChanged` callback at :28 → triggers `rebuildChannelDropdown()`.
- Fires from VibeGraph at :2253, :2262, :2297.
- **Callback IS wired but doesn't fire on certain delete paths in practice.**
  MIX-07 is in the callback chain, not the dropdown itself.

### Idle-suspend gate (InstStripTask.cpp)

- :115-119 — `if (midiEmpty && noVoices)` test.
- **Missing**: `auditionPending = eng->mAuditionNote.load() != -1` check.
- Doc-comment at [PluginProcessor.h:807](../Source/PluginProcessor.h)
  promises "audition" as a wake condition — contract specified but not
  implemented.
- DSP-10 fix: add the predicate. Touches both Inst (Guitars + Basses
  via InstStripTask) AND `RustyDrumsProducerTask` (parallel gate location).

### Bus solo (VibeGraph.cpp)

- LayersBus `anySolo = thisSolo || bassSolo || drumSolo` at :358.
  **Per code intent: pairwise within group.**
- Silencing formula (all receive-group buses) at :1775:
  `silenced = muted || (inGroupSolo && useGroupSolo && !soloed)`.
- ClipsBus `localAnySolo` at :1698-1702 (checks 6 buses).
- RustyDrumsBus standalone at :1727-1728 (`inGroupSolo=false, useGroupSolo=false`).
- **DSP-09 disconnect**: code intent is per-group, but observed behavior
  doesn't even match per-group (Drums plays when Layers solos despite
  `drumSolo` being in the formula). User-specified target behavior:
  solo a bus → that bus + everything routed into it plays; every other
  bus silenced at master mix. Diagnose the drums-still-plays issue first,
  then implement the new behavior.

### Dead Properties duplicate (BuilderPage.cpp)

- :2561 — `m.addItem(7, "Properties...");` added unconditionally for all
  block types, no `case 7` in switch at :2588+. **Dead. Delete this line
  entirely.** Cleanup happens inside QA-E.

---

## §4. Decisions Already Made (Not To Re-Litigate)

| Topic | Decision |
|-------|----------|
| Sequencing | **Option A** — confidence-first (QA-0 → QA-A → QA-B → QA-C → QA-D → QA-E → ...) |
| DSP-12 fix shape | **Composite RenderTask.** Single new task type owns BOTH render flows (AudioInsertTask + ClipPageTask) and sums them internally before insert DSP. Matches serial-mode summation. |
| DSP-09 target behavior | **Solo a bus** → that bus + everything routed into it plays normally; **every other bus silenced** at master mix. NOT FL-style global "any solo silences every other strip". |
| FILE-02 routing dropdown | **Vox + Inst + Clips** options (not every player page). Cross-over allowed. Supports remote-collab "drop someone else's vocal/guitar wav into this project" use case. |
| FILE-02 reassignment timing | **Immediate** — next audio block reflects new routing. Triggers `rebuildRoutingFromApvts` on message thread; audio thread picks up new graph at next block boundary. |
| NAV-04 Piano Roll buttons | Deep-link buttons keyed to **active piano-roll dropdown selection**. Pressing them takes user to that player's page or that player's effects rack. NOT in-PR sub-tabs. Visually look like standard nav buttons elsewhere. |
| NAV-03 FX Rack button routing | Layers/Bass/Vox/Inst pages → that player's per-strip FX rack. Individual drum tab → that drum's individual rack. Drum Kit page → kit bus rack. Rusty's main page → Rusty's drum bus rack. |
| NAV-05 Builder hamburger | **Remove.** Reclaim vertical space. (Original spec suggestion of snap-to-grid / time signature was wrong — those don't fit Builder-level scope.) |
| MIX-03 | **Symptom of MIX-02**, not separate work. Fixes when MIX-02 fixes (Vox strip stops disappearing on reload → orphan recordings stop becoming clips strips). |
| STATE-03 | **Folded into APP-03** modal load progress dialog UX. Symptom only. |
| Active queue size | 64 backlog → **54 batched items** (2 folded, 3 parked, 5 long-horizon). |

---

## §5. Per-Item Status Snapshot (2026-05-07 Triage Findings)

### Confirmed open (assigned to batches)

| Item | Batch | Note |
|------|-------|------|
| DSP-01 | QA-K | Harmless lazersaw silent — needs preset audit harness |
| DSP-02 / 03 / 05 | QA-F | Vox FX bypassed, pitch correction inert, BaySickAlign review |
| DSP-04 | QA-Fa | BaySickPitch missing audio import (additive) |
| DSP-06 | QA-J | Multi-clip stacking attenuation |
| DSP-08 | QA-K | Tascam Model 24 outputs 21/22 stereo bug |
| DSP-09 | QA-E | Bus solo (target behavior in §4) |
| DSP-10 | QA-C | Idle-suspend audition wake (1-line predicate) |
| DSP-11 | QA-K | Live ASIO buffer-size change |
| **DSP-12** | **QA-0** | **Composite RenderTask — TOP PRIORITY** |
| STATE-01 / 02 / 04 | QA-D | Project state reset + dirty + playhead-stop |
| FILE-01 | QA-E | Vox wet delete → browser bin (RetirementQueue tie-in) |
| FILE-02 | QA-E | Multi-record routing via Properties popup |
| FILE-03 | QA-L | Browser delete-all-instances of duplicate name |
| MIX-01 | QA-C | Vox tab close missing `removeVoxChannel` (1-line) |
| MIX-02 / 04 / 06 | QA-E | Vox/Inst tab reload destroys strip |
| MIX-05 | QA-L | Mixer strip overlap after delete (resized() trigger) |
| MIX-07 | QA-L | Effects dropdown stale entries |
| REC-01 | QA-E | Recording library hand-off |
| NAV-01 | QA-L | Window resize layout (FlexBox/Grid + min size) |
| NAV-02 | QA-I | Engine swap loading sign |
| NAV-03 / 04 | QA-L | FX Rack + Piano Roll deep-link buttons |
| NAV-05 | QA-H | Remove Builder hamburger |
| UI-01 / 02 | QA-L | PopupMenu wrapper + automation UUID |
| BUILD-01 / 02 / 03 | QA-G | Timeline geometry (100 tracks, ruler freeze, zoom) |
| BUILD-04 / 05 / 06 | QA-H | Ghost notes static, 's' keybind, WAV-stretch rebuild |
| MIDI-01 / 02 / 03 / 04 | QA-H | Piano roll modifiers, control lane viz, Humanize |
| STYLE-01..06 | QA-A | Unified TitleBar component |
| APP-02 / 03 | QA-I | Shutdown overlay + load progress dialog |
| APP-04 / 05 | QA-K | SetPriorityClass + ASIO Control Panel button |
| LIFE-01 / 02 | QA-M | Drum kit-load destroys Rusty + Rusty re-add reload |
| DIAG-02 | QA-N | DSP meter sum-of-cores |

### Parked (no batch unless conditions change)

- DIAG-01 — synthetic test for `rebuildLinks`. jassert sufficient.
- APP-01 — shutdown wait climbing. Test-scenario inflation.
- DSP-07 — single observed silent-first-drop, didn't repro. Watch-item.

### Folded (resolves with another item)

- MIX-03 → MIX-02
- STATE-03 → APP-03

### Long-horizon (deferred but not killed)

- OPT-01 — per-stage parallelism inside a strip
- OPT-02 — worker thread priority elevation
- OPT-03 — TSAN integration in CI
- OPT-04 — replace serial path entirely (~6+ months post-Batch 10)
- NEVER-01 — per-band EQ parallelism (softened: "not actively planned;
  not blocked from future reconsideration if EQ topology changes")

---

## §6. Patterns To Reuse (don't reinvent)

These are established BaySickDAW patterns. Reach for them before designing
new abstractions for similar problems.

| Pattern | Anchor | Use when |
|---------|--------|----------|
| RCU snapshot via atomic pointer + RetirementQueue<T> drain | `mActiveAudioClips` ([PluginProcessor.h:539](../Source/PluginProcessor.h)) + [Engine/RetirementQueue.h](../Source/Engine/RetirementQueue.h) | Audio thread needs to read a structure that the message thread mutates. Lock-free reads, slow destruction off-thread. |
| `closeAllDynamicTabs` barrier | [StandaloneEditor.cpp:8672](../Source/Standalone/StandaloneEditor.cpp) | Tearing down tabs / project state safely. Sets `mProjectLoadInProgress(true)` + 30 ms sleep + tab close + barrier reset. |
| `mProjectLoadInProgress` audio-thread gate | [PluginProcessor.h:885](../Source/PluginProcessor.h) | Silence audio output during load/teardown windows. |
| `mShuttingDown` per-engine gate | [BaySickVocalProcessor.h:170](../Source/BaySickVocal/BaySickVocalProcessor.h) | Engine-specific shutdown crash protection (null-vtable in mid-block). Reusable if NAM/IR or sfizz show similar crashes. |
| "No dead wiring" rule | (process-level) | Every prep change gets actively exercised in serial AND MT before flag flips. Don't add code that's elided at runtime. |
| APVTS-synced DSP isIdentity + dirty flag | (memory + various DSP modules) | Every APVTS-synced DSP module pairs `isIdentity()` on process side with ValueTree-listener-driven dirty flag on sync side. Avoids per-block recompute thrash. |
| Audio-thread fast-path bypass via single atomic load | (memory + D1.2 mAnyDrumPageActive) | Feature-flagged per-block iteration that's empty until flag flips → gate behind one acquire-load atomic, set in register/unregister. |
| ASCII-only UI strings | (memory + 94-occurrence sweep) | Every user-facing string literal must be pure ASCII. Non-ASCII renders as box glyphs in current font setup. |
| Single source of truth for filesystem paths | (memory + audio_settings.xml fix) | Every reader+writer of a path calls the central resolver function. Never hardcode parallel path strings. |
| Switch-style toggle is opt-in | (memory) | `juce::ToggleButton` with VibeLAF renders as checkbox by default. Opt into switch filmstrip via `getProperties().set("switchToggle", true)`. Reserved for FX rack slot, player switch panels, mixer pre/post send. |
| Engine audition pattern | (CLAUDE.md key technical notes) | All 4 engine processors (BaySickSynth/Bass/Harmless/VibePlayer) have `auditionNote(int midiNote)` + `std::atomic<int> mAuditionNote { -1 }`. processBlock opens with `int n = mAuditionNote.exchange(-1); if (n >= 0) { noteOff-any, noteOn n }`. |
| CPU safeguarding standing rule | (CLAUDE.md) | Every DSP update function must guard numeric setters with value-change comparisons. Only call setter if new value differs from current DSP state. |
| Lock-after-pick for picker buttons | (CLAUDE.md Phase D) | Picker button transforms to show current selection name with `[L]` prefix when locked; both clicks (left + post-lock) open per-page context menu. Used in DrumPage / LayersPage / BassPage. |
| Composite RenderTask (NEW — to be established by QA-0) | new in QA-0 | When two render flows target the same channel id, build a composite task that owns both internally and sums them before insert DSP. Reusable by QA-J for multi-clip stacking. |

---

## §7. Recent Commit Map (MT path execution history)

For when a per-batch plan author needs to look up "when did X land?"

| Commit | Phase | What |
|--------|-------|------|
| `c68b7c8` | Repo cleanup | Fix: actually commit the .gitignore additions claimed by 639a661 |
| `639a661` | Repo cleanup | Repo cleanup: vendor libs/* inline + gitignore runtime + untrack auto-saves |
| `6c05200` | Batch 10 P3 | settings.xml persistence for the Multi-core Rendering toggle |
| `35ca8c6` | Batch 10 P1+2 | Runtime MT toggle + Mixer hamburger menu + DSP meter under MT |
| `47ba7a2` | Batch 9c | Flag flip + watchdog + meter drain + SC pull + bus solo fix |
| `3b2c85a` | Batch 9c B2 + N1 | refreshWindowTitle marshal + BaySickVocal mShuttingDown gate + ~StandaloneEditor teardown ordering |
| `fdbe9e1` | Batch 9c B1 | RetirementQueue + AudioClipSnapshot RCU snapshot |
| `0cf6c96` | Batch 9b Item 10 | Per-task scratch on AudioInsertTask |
| `42d9a30` | Batch 9b Item 9 | renderFilePlayPlayer helper for Vox/Inst FilePlay |
| `04150c7` | Batch 9b Item 8 | tapDryRecorder helper for armed-input recording |
| `a19c6e3` | Batch 9b (REVERTED) | mAudioClipPlayers snapshot pattern — reverted; superseded by `fdbe9e1` |

---

## §8. Anti-Patterns To Avoid

Carried forward from user corrections + memory entries. Per-batch plan
authors: don't make these mistakes again.

- **Don't speculate about FL Studio behavior.** Jeff is a daily FL user
  and treats it as canonical UX. Ask before claiming "FL does X".
- **Don't write/modify plan documents unprompted.** Jeff owns plan docs.
  When asked "do you know what we're doing", confirm in one line and stop.
- **Don't make unilateral spec calls.** Surface trade-offs; let Jeff pick.
  No "default if you say you pick", no "the only sensible reading".
- **Don't ribbon-expand when adding new top-level tab.** New ribbon slots
  compact into existing total width.
- **Don't over-prune vendored library directories.** Grep their CMake
  files for unconditional `configure_file()` references before deleting
  subdirs (sfizz precedent on 2026-05-03).
- **Read code before calling something "expected".** When Jeff pushes
  back on a behavior or asks "where did this come from", grep / read
  the actual implementation BEFORE defending it as expected.
- **Diagnose before fixing.** When an audio bug's cause isn't obvious,
  ask for A/B + mute + sweep diagnostics before shipping any code change.
  Speculative fixes that turn out wrong erode trust.
- **Walk Jeff through any debug step-by-step.** Jeff doesn't code but
  WILL debug if walked through every step. Never use bare jargon like
  "set a breakpoint" without explaining the action; prefer a diagnostic
  AlertWindow over a debugger session.
- **Don't ship non-ASCII UI text.** Use pure ASCII in every user-facing
  string literal.
- **Use the closeAllDynamicTabs barrier rather than inventing teardowns.**
  STATE-04 and similar lifecycle work should reuse this, not parallel it.
- **Pick the right strip-type reference for new strips.** Engine-driven
  strips (auto-spawn from engine registration, no arm/monitor) mirror
  Layer/Bass/Drum. Live-input strips (user-click Add button, arm + monitor
  LEDs) mirror Vox/Inst.
- **Verify disk names before writing matchers.** `ls` actual filenames
  BEFORE writing `compareIgnoreCase("X")==0` or similar. Documentation
  labels often differ from on-disk names.
- **Commit proactively at logical checkpoints.** At verified-working
  checkpoints (fix lands + builds + runtime-tested), commit immediately
  without asking. Stage specific files only (never `git add -A`).

---

## §9. Three-Doc System Reminder

| Doc | Cadence |
|-----|---------|
| Plan (`Main Plan.md`) | Append-only on scope changes — never overwrite. |
| Carry-Forward (this doc) | **FROZEN.** Never edited. |
| Implemented Work & Findings (`Implemented Work Log.md`) | Append-only running log. |

Carry-over discipline at every stopping point: write a 5-10 line block
under `## Carry-Over` in the active per-batch plan covering Completed /
In-flight / Assumptions changed / Resume action / Implemented-work
entry needed.

---

**End of carry-forward reference. Don't edit this file.**
