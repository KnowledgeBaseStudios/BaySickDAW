# QA-ModelShell — Engine-ownership inversion + contained-window shell + true offline export + VST3 hosting + model-side automation + freeze/loudness suite — Plan (grand-inverting-mammoth)

> **Canonical path:** `Plans & Specs/Batch Plans/grand-inverting-mammoth.md`.
> **For execution:** slots directly after QA-ProjectSave (deep-packing-badger) per Jeff's
> 2026-07-27 ruling; G4 run plan (`swift-stampeding-caribou.md`) to be updated on approval.
> Batch ID "QA-ModelShell" is provisional pending Jeff's review of this file.
> Eight task sets = the eight approved groups. Task sets are deliberately LARGE; sessions
> may hand off mid-set (write a `## Carry-Over` block in this file per Main Plan §0 Rule 1
> when that happens).

## Context

**Origin.** During QA-ProjectSave Task 7's mandatory applicator sweep (2026-07-27), the
lifetime audit pulled a thread that ended at a structural diagnosis: **the UI constructs the
audio model instead of viewing it.** Pages build and own engine instances and hand raw
pointers to the processor (`registerLayerEngine` family, [PluginProcessor.cpp:5452](../../Source/PluginProcessor.cpp:5452));
page registration is what creates instrument InsertNodes ([:5464](../../Source/PluginProcessor.cpp:5464), [:5502](../../Source/PluginProcessor.cpp:5502), [:5687](../../Source/PluginProcessor.cpp:5687));
automation wiring historically lived on widgets. Consequences verified in source and by
Jeff's own export test:

1. **Export renders instrument tracks SILENT.** `renderToFile` builds a fresh
   `VibeSynthProcessor` ([BuilderPage.cpp:8064-8071](../../Source/Standalone/BuilderPage.cpp:8064))
   which has no pages, therefore no engines and no instrument channel strips. Only audio
   clips (explicit `rebuildAudioClipPlayers`, [PluginProcessor.cpp:3918](../../Source/PluginProcessor.cpp:3918)),
   aux strips, and bus/master processing render. Jeff confirmed by ear: vox/inst exports
   produce nothing.
2. **Export ignores most automation.** The only automation dispatch is the editor's UI
   timer ([StandaloneEditor.h:842](../../Source/Standalone/StandaloneEditor.h:842)); the
   engine-side replay ([PluginProcessor.cpp:2791-2866](../../Source/PluginProcessor.cpp:2791))
   applies only lanes whose id resolves in the MAIN APVTS. Rack lanes, engine lanes,
   `_fader`/`_pan` lanes, and `global_tempo` never reach a render.
3. **Rack automation wiring is view-gated** (registered only in
   [rebuildSlotEditor](../../Source/Standalone/EffectsPage.cpp:988)) and wiped at every
   project boundary ([resetProjectState](../../Source/Standalone/StandaloneEditor.cpp:11416)).

Jeff's ruling (verbatim intent): FL Studio functionality was the requirement all along;
model-owned engines are **a requirement, not future state**. The fix wave then pulled in the
window shell, the full Future State tiers list, and VST3 hosting, because this is the one
moment the shell is being rebuilt.

**Research grounding (read before executing the related task set):**
- [`Research Reports/daw-architecture-non-parameter-automation-binding-2026-07-26.md`](../Research%20Reports/daw-architecture-non-parameter-automation-binding-2026-07-26.md)
  — 8-codebase survey: no system keys automation OR engine ownership to UI; automation
  targets model-owned objects with stable ids (VST3/CLAP/Tracktion/Ardour/Vital/Surge/
  iPlug2/JUCE; REAPER positional-addressing counterexample).
- [`Research Reports/daw-architecture-research-2026-05-08.md`](../Research%20Reports/daw-architecture-research-2026-05-08.md)
  — voice management, FFT plan caching, lock-free MIDI, sample streaming (feeds CL-282 and
  the freeze/streaming work).
- FL rendering evidence (same-instance offline, wrapper "Notify about rendering mode"):
  Image-Line forum t=142738 + t=184183, support KB ans=145, KVR t=588204 (fetched
  2026-07-27). API-level anchor: `AudioProcessor::setNonRealtime`
  ([juce_AudioProcessor.h:991](../../juce/modules/juce_audio_processors_headless/processors/juce_AudioProcessor.h:991));
  note our Source tree currently calls it NOWHERE.
- Sweep + docket trail: [`Running Notes/deep-packing-badger.md`](../Running%20Notes/deep-packing-badger.md)
  (applicator census, three step-3 failure root-causes, export-gap discovery chain).
- Page-preset scope proof (CL-102 already shipped): [PagePresetIO.h:10-26](../../Source/Standalone/PagePresetIO.h:10).

- **Risk:** highest since Phase D. Touches every page type, VibeGraph registration, project
  save/load, export, automation, and replaces the entire window shell. Mitigations: the
  dependency order below is strict; deep-packing-badger already extracted the load/save
  seams (`serializeStructuralUIState`, `applyProcessorState`, `writeProcessorState`); the
  step-3 DSP-targeting architecture is runtime-proven (Jeff's differential test + outbound
  volume knob confirmed working 2026-07-27).
- **Effort:** very large — multi-week, by far the largest batch of the QA era. Rough shape:
  TS1 ~15-25 h, TS2 ~20-30 h, TS3 ~20-30 h, TS4 ~25-40 h, TS5 ~10-15 h, TS6 ~40-70 h
  (BLU-302 alone is entry-estimated 3-6 weeks), TS7 ~15-25 h, TS8 ~4-6 h. Jeff has accepted
  the scale explicitly ("we're doing the entire list").
- **Dependencies:** deep-packing-badger closes first (its Tasks 8-12 disposition = Jeff's
  Group-0 call, made during that batch's close-out). Uncommitted Task 7 partial work
  (registry + diagnostics + Compressor×4 + output_vol conversion) ships with that batch.

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| Inversion | Engine ownership moves to the model: construction, state restore, swap, teardown, graph registration. Tabs are model objects; pages/windows are disposable views. Vox/Inst chains included | Jeff 2026-07-27: FL functionality is the requirement; "engines are the drivers and the pages just hold them." Root cause of export silence + automation view-coupling |
| Order = 1b | Inversion FIRST, export built on the clean model | Jeff picked (b) over export-first-on-live-graph; nothing built twice |
| Export architecture | The model renders itself offline — same engines/graph as playback, offline clock, no replica processor | FL shape (same-instance offline, verified via wrapper "notify" evidence); Jeff's DRY challenge; the fresh-proc replica already rotted once (lost engines at Phase D) |
| Export semantics | Tempo lane FOLLOWED in export; full re-prepare so render rate is independent of device rate; `setNonRealtime` sweep; graph-wide reset before render + restore after; restore set = transport pos, song/pattern mode, current pattern, live-tempo, offline flags; metronome excluded (matches record-master semantics — click is injected post-master-tap; MetroDSP lives processor-side, [PluginProcessor.h:1200](../../Source/PluginProcessor.h:1200), default-off); editor automation timer paused during render | Jeff: tempo=follow, "full reprepare now". Metronome verified 2026-07-27 |
| Export destination | Export creates `<project>\Exports\` and the save dialog opens there (replaces userMusicDirectory defaults at [StandaloneEditor.cpp:10611](../../Source/Standalone/StandaloneEditor.cpp:10611) + [BuilderPage.cpp:8274](../../Source/Standalone/BuilderPage.cpp:8274)). Interlocks with the save-first prompt (project folder guaranteed) | Jeff spec 2026-07-27 |
| Export UX | Modal. Options dialog persists; save dialog appears above it; after save, options box shows a render progress bar + percent readout; Cancel stays live | Jeff spec 2026-07-27 (FL-style export bar) |
| Shell = 2b | FL-style contained workspace: our windows AND plugin editors are REAL NATIVE CHILD WINDOWS inside the fixed main frame (required for correct z-order with foreign plugin surfaces — drawn components cannot paint over native plugin regions) | Jeff picked (b) after the corrected ecosystem survey (FL/REAPER contain; Ableton/Cubase/S1 float; plugin API is parent-HWND-agnostic) |
| Main window | Fixed fullscreen (maximized), resize disabled (currently resizable, [StandaloneApp.cpp:950](../../Source/Standalone/StandaloneApp.cpp:950)) | Jeff spec |
| Rollout = 3a | All specced windows in the one shell rebuild (Builder, Piano Roll, Mixer, Effects surface) | Jeff picked (a) |
| Title bars = 4a | Custom-drawn title bars; each page's hamburger/menu row merges into the title strip (free under contained mode) | Jeff picked (a) |
| Buttons = 5a | Close + resize only; the tab bar is the reopen; windows persist size/pos | Jeff picked (a); minimize meaningless in a contained frame |
| Destroy-on-close | Closing a window DESTROYS the view (engines keep running). This is the CPU/memory win and it FORCES automation registration fully model-side | Jeff: "I want the cpu benefit so definitely destroy on close" |
| Min sizes | Every window gets a resize floor at its layout's knob-collision point (fixed-grid panels: floor = natural size). DPI-aware expression | Jeff spec; precedent = main window's documented floor |
| Tab bar | Required tabs always present: Builder, Mixer, Effects, **Piano Roll**. Type tabs hidden at zero instances; "+" button holds every add option; populated tabs keep ALL current dropdown behavior; deleting a type's last page removes its tab (returns via "+"). RETIRES Task 1 (docket 18) empty-state pages + 0-badge always-visible slots — explicit reversal, Jeff confirmed the mechanics 2026-07-27 | Jeff spec; option-removal paper trail |
| Rack surface | BLU-480 sidebar picker + detail pane as the Effects window. CL-299 Delay cosmetics ride. BLU-499 preset-loader placement designed into the shell. VST3 plugin slot type is REAL (ships in TS6), not a reserved hole | Jeff: tiers list ruling + prior "do the future state idea if possible"; feasibility confirmed |
| Scope = everything | ALL tiers items build: riders CL-040/043/045/056/282/301; pulled-in CL-055+BLU-427 (freeze), CL-057, CL-060 (lazy half only — parallel half DROPPED 2026-07-28), CL-044, CL-227, BLU-344; maximizer suite CL-244+CL-243(+BLU-109)+BLU-108+BLU-110+measure-before-render button; FULL VST3 family BLU-297/298/299/300/301/302+BLU-447 (LDT-219/423 subsumed) | Jeff: "I said everything… I don't want to have to do all of this later when we're building the shell for all of it now" |
| Automation endgame | ALL registration moves model-side: engine params registered by the model at engine creation; rack params via EffectParamMap tables; mixer `_fader`/`_pan` lanes remapped to their real strip params; EQ band ownership decoupled from the display; views only stamp right-click ids. Wire-at-load + wire-at-param-creation dissolve into this (they were view-trigger workarounds) | Destroy-on-close makes every widget-scoped registration a guaranteed defect; industry-uniform per the binding research |
| CL-045 semantics | LUFS-target normalization on bounce is measure-then-gain, BOTH directions, headroom-capped when boosting (true-peak ceiling). NOT a maximizer — that's CL-244's job | Jeff Q&A 2026-07-27 |
| Freeze design forks | Tap point (pre-rack "Source Only" vs post-rack "Full" vs both — Logic precedent) + presentation (invisible swap vs bounce-in-place row) are OPEN sub-spec calls, resolved at TS7 open | Jeff confirmed intent = stop engine computing, grid stays source of truth |
| IRC-style modes | We build OUR OWN limiter character algorithms (Transparent/Punchy/etc.); no branded names in UI (no-brand-names rule); concept parity with Pro-L styles / Ozone IRC | Jeff asked for modes; trademark hygiene |
| CL-102 | OFF the list — already shipped as PagePresetIO (engine(s) + strip params incl. routing/sends + insert rack + pre/post EQ8, all seven page kinds, cross-index prefix rewrite). Future State gets a stale-mark in TS8 | Verified in source 2026-07-27 after Jeff challenged the claim |
| Commits + verification cadence | A commit LANDS at the end of EVERY task set (Rule 9 one-liner + FULL git status surfaced, committed on Jeff's approval — not merely offered). Compile build gates stay per task set: they gate each commit. ALL functional verification is deferred into ONE batch smoke at Task set 8 (bulk-run shape) — no per-set verify pauses | Jeff 2026-07-27 housekeeping ruling |
| Conflict-review calls | (1=b) The G4 boundary R3 review + smoke covers only yak/stoat/heron — this batch verifies via its per-set commits + the TS8 smoke. (2=b) TS1 pre-wires the processor-owned UndoManager + factory pass-through, DORMANT (QA-UndoCoverage ships the semantics; its Task 2 shrinks to verification). (3=a) The Master Test Plan §B reconciliation pass runs inside TS8. (4=a) Dated conflict notes applied to yak/stoat/heron 2026-07-27 | Jeff 2026-07-27 conflict-review docket |

## Sub-spec calls surfaced for ExitPlanMode

Open, to resolve at the owning task set's start (numbered prose to Jeff, lettered options,
no recommendations):
1. **TS7:** freeze tap point (pre-rack / post-rack / both) + freeze presentation
   (invisible swap / bounce-in-place / both).
2. ~~**TS2:** stems (CL-040) granularity — per tab, per bus, or pick-list dialog.~~
   RESOLVED 2026-07-27 (Jeff): **per MIXER STRIP** (FL model) via a **pick-list of all
   active strips** — sends/aux render as their own stems; Master + buses default
   UNCHECKED, every other strip defaults CHECKED; stem files land in `<project>\Exports\`
   (confirmed, same as the locked destination spec). Implementation consequence: ONE
   full-graph offline pass with simultaneous per-strip output taps (never
   mute-others-per-pass, which would kill sidechain keys — the point of his compressor
   example is that stems keep SC-driven behavior).
3. **TS6:** crash-protection process model detail (per-plugin sandbox process vs single
   shared host process) — BLU-302 entry says optional/3-6 weeks; Jeff ruled it IN, model
   choice still open.
4. **TS4:** exact required-window minimum sizes (Jeff wants "larger" floors for Builder /
   Piano Roll / Mixer — numbers picked with him at implementation, on screen).
5. ~~**TS1:** batch-ID name if "QA-ModelShell" isn't to Jeff's taste.~~ RESOLVED
   2026-07-27: (a) — QA-ModelShell stays. Process correction recorded: this should never
   have been a docket item; naming plan artifacts (batch IDs included) is the assistant's
   job, not a spec call (Jeff: "you do the plan not ask me").

## Files to modify

Known anchors (verified this session). Each task set opens with a scout pass to complete
its own list — this batch is too large for exhaustive pre-enumeration, and refs rot.

- **TS1:** `Source/PluginProcessor.h/.cpp` (engine registry -> ownership; register* family
  becomes internal; model-side registration hooks at :5452-5845; note
  [PluginProcessor.h:406-435](../../Source/PluginProcessor.h:406)),
  `Source/VibeGraph.h/.cpp` (InsertNode creation decoupled from page calls; CL-301 folds
  LayersBusNode/BassBusNode/DrumsBusNode/MasterBusNode/EffectsBusNode into
  InstrChannelNode per its Future State entry), `Source/Standalone/LayersPage.cpp` (:44,
  :146-147, :159-173, :232-233), `BassPage.cpp` (:39-48, :136-141, :152-166, :225-226),
  `DrumPage.cpp` (:135-141, :315-319, :336-348, :383, :903-907), `Vox/VoxPage.cpp`
  (:436-442 create-once vocal proc), `Inst/InstPage.cpp` (:84-123 ctor engines, :171-199
  dtor), `BaySickRustyDrumsPage`, `Source/Standalone/StandaloneEditor.cpp` (tab lifecycle
  -> model ops; serialize/deserialize rewire onto model-owned engines).
- **TS2:** `Source/Standalone/BuilderPage.cpp` (renderToFile :7993-8217, OfflineHead
  :7944-7978, chooser :8270-8278), `Source/Standalone/StandaloneEditor.cpp` (export dialog
  :10600-10630; timer pause hook), `Source/PluginProcessor.cpp` (engine replay :2791-2866
  generalized; reset/re-prepare sweeps), `Source/DSP/AudioClipStreamer.h/.cpp` (offline
  synchronous-read mode + CL-282 telemetry), Mp3Writer/writers (CL-043 dither),
  settings dialog (CL-057 hot-swap reusing the re-prepare machinery).
- **TS3:** `Source/DSP/EffectParamMap.h/.cpp` (tables: all EffectTypes × variants incl.
  pedal-native types 100+ per [EffectRack.h:15-60](../../Source/EffectRack.h:15); Reverb
  `freeze` def), `Source/Standalone/EffectsPage.cpp` (registerSlotAutomation :496-594
  relocated to model triggers), `Source/BaySickPedals/BaySickPedalsEditor.cpp` (tile
  registrations :161-244 superseded by model-side), engine editors' 19 wrapper sites
  (retire/replace: BaySickBassEditor:391, BaySickSynthEditor:397, HarmlessEditor:485/562/638,
  HarmlessFilterRow:61-74, HarmlessRoutingMatrix:38, HarmlessXYZPad:46,
  VibePlayerEditor:220/259-262, BaySickNAMIREditor:8-50, BaySickVocalEditor:597-663),
  `MixerTrackStrip.cpp:439-485` (fader/pan lane remap), `SharedUI.cpp:5376-5419` (EQ
  regOne ownership), `StandaloneEditor.cpp` (statics :11461-11505, stale test re-widen,
  owner-index simplification), Harmless mod editor (BLU-344 targets).
- **TS4:** NEW workspace component family (native-child window frame: title strip, close,
  resize border, constrainer), `Source/Standalone/StandaloneApp.cpp` (:948-1015 fixed
  main), `StandaloneEditor.cpp/.h` (page hosting -> window management; destroy-on-close;
  per-window state persistence + off-screen clamp reuse), `RibbonTabBar.cpp/.h` ("+"
  system; zero-hide; retire Task-1 empty states in `SharedUI` EngineEmptyState +
  `hideAllEmptyStates`), `PageMenuBar.cpp/.h` (merge into title strips), KeyBindings /
  BSCommands routing per window, `GlobalTransportBar` (stays in main chrome).
- **TS5:** NEW effects window (sidebar + detail pane) consuming `SlotComponent` picker
  logic + `EffectEditorPanels` panels + both EQ displays; BLU-499 loader; CL-299 Delay
  panel deltas; player-page FX buttons rewire.
- **TS6:** NEW `Source/Hosting/` (scanner, known-plugins list, browser UI, VST3 wrapper
  DSP as EffectType slot; instrument-engine adapter into TS1's generic slot; latency
  passthrough; sandbox process for BLU-302). JUCE hosting modules enablement in
  `CMakeLists.txt` (juce_audio_processors plugin-hosting flags; module is currently the
  _headless variant — scout whether hosting requires the full module swap).
- **TS7:** freeze machinery over TS2's renderer (per-drum + per-tab), `LimiterDSP`
  (CL-244 target mode, CL-243 modes, BLU-108 ceiling, BLU-110 metering — the approved
  Limiter UI spec in `Files For Claude/DSP Review/_APPROVED_CHANGES.md` + `Limiter.txt`
  governs the panel), CL-044 analyzer window (SpectrumFeed reuse), CL-227 scan/report +
  measure-before-render button (TS2 backend).
- **TS8:** `Plans & Specs/Main Plan.md` (§5 entry, §6 sequencing, §9 Forks: CL-087
  promotion + this batch's origin trail + docket-18 partial reversal),
  `Future State.md` (stale-marks: CL-102 -> PagePresetIO; graduated entries),
  `Test Plans/` (Master Test Plan §B section), `swift-stampeding-caribou.md` (already
  updated at slot-in), `CLAUDE.md` (architecture notes: model-owned engines, window shell).

## Tasks

### Task set 1 — The inversion (Group 1)

Target state: `VibeSynthProcessor` (or a new model-layer `EngineRig` it owns) is the single
owner of every engine instance and every tab's identity. A tab = model object
`{type, pageIndex, name, engineType, engine unique_ptr}`. Pages/windows request engines,
never create them.

- [ ] Scout pass: enumerate every engine construction/restore/swap/teardown site in the six
  page types + Rusty; enumerate every `register*Engine` / `unregister*Engine` caller;
  map each page's extra wiring (transport-beat hooks, audition, dirty listeners — these
  become model-side or bind-on-view-attach).
- [ ] Build the model tab registry + engine factory: construct-by-type, restore-from-blob
  (`encodeEngineState`/decode already exist), swap, teardown. sfizz engines keep their
  race-safe kit-load path (see PagePresetIO's `kitLoadCallback` note,
  [PagePresetIO.h:60-69](../../Source/Standalone/PagePresetIO.h:60)).
- [ ] Vox/Inst chains move model-side (BaySickVocalProcessor; Pedals+NAM chain +
  `EngineChainProcessor`). THIS is the vox/inst export fix.
- [ ] Tab lifecycle (add/delete/duplicate/rename, ribbon calls) becomes model operations;
  project save/load drives the model (deep-packing-badger's `serializeStructuralUIState` /
  `applyProcessorState` seams are the insertion points). Templates v2 + PagePresetIO
  re-pointed at model-owned engines (their capture format is unchanged).
- [ ] Model-side automation registration hooks: at engine creation the model registers
  param-targeting applicators/readers for the engine's APVTS (per-instance prefixes as
  today); at rack/InsertNode creation, `registerSlotAutomation`'s logic runs model-side.
  (Full sweep of legacy view-side registrations lands in TS3 — do not delete wrappers yet.)
- [ ] CL-301: fold the 5 hand-written bus-node structs into InstrChannelNode (see its
  Future State entry for the three divergence incidents + survival-by-flags notes).
- [ ] Generic engine slot shaped so a future hosted VST3 instrument is "just another
  engine" (TS6 consumes this).
- [ ] UndoManager pre-wire (conflict call 2=b, DORMANT plumbing only): processor-owned
  `juce::UndoManager` member declared BEFORE apvts (constructor-order rule), threaded as
  `UndoManager&` through the engine factory into every engine APVTS ctor. NO semantics
  change here — StandaloneEditor's manager stays authoritative; QA-UndoCoverage flips the
  semantics on (its Task 2 is now just that flip + verify).
- [ ] Pages become views: bind-to-model on construction; NO ownership. Editor windows in
  TS4 will construct/destroy these views freely.
- [ ] Build gate (`do_build.bat`, both configs green — gates the commit below).
- [ ] Batch-smoke scenarios (DEFERRED to Task set 8 — do not pause here): (1) full
  regression pass of normal use — add/delete/rename every tab type, engine picks, project
  save/reopen, template load; (2) close/reopen app; (3) old projects still restore.
  Expect NO user-visible change from this task set — that is the pass condition.
- [ ] COMMIT (lands at end of every task set per Jeff 2026-07-27): Rule 9 one-liner +
  FULL git status surfaced -> Jeff approves -> commit. `/draft-doc running-notes`
  checkpoint.

### Task set 2 — The export engine (Group 2)

Target state: export = the model rendering itself offline, FL-shape.

- [ ] Offline drive: suspend device; `setNonRealtime(true)` sweep across the graph +
  every engine; graph-wide reset (wet-tail hygiene) before render; render loop drives
  processBlock with the offline clock; restore set after (transport pos, song/pattern
  mode, current pattern — note pattern-scope export today mutates live current pattern
  with no restore — live-tempo, flags, device resume); editor automation timer paused
  during render.
- [ ] Full re-prepare: render rate independent of device rate; prepare graph+engines to
  render rate, back after (Jeff: "full reprepare now").
- [ ] Offline clock follows the TEMPO LANE on top of the tempo map (OfflineHead currently
  map-only, [BuilderPage.cpp:7944-7978](../../Source/Standalone/BuilderPage.cpp:7944));
  extract one shared beats<->seconds resolver used by live + offline.
- [ ] Automation application in-render: generalize the engine replay
  ([PluginProcessor.cpp:2791](../../Source/PluginProcessor.cpp:2791)) to resolve EVERY
  lane class UI-free — main-APVTS (as today), engine-APVTS via per-instance prefix ->
  model engine, rack lanes via prefix+uuid+suffix -> EffectParamMap, `output_vol` ->
  slot gain, `_fader`/`_pan` -> strip params, global_tempo -> clock. Runs on the render
  thread (single-threaded with processBlock there — DSP-setter thread contract holds).
- [ ] Clip streaming offline mode: synchronous/blocking reads when `isNonRealtime`
  (AudioClipStreamer SPSC ring can be outrun by a fast render) + CL-282 underrun
  telemetry (atomic counter + Debug overlay) so the fix is provable, not vibes.
- [ ] Metronome gated out of the render path (or tap pre-injection point, matching
  record-master semantics Jeff described).
- [ ] Destination: create `<project>\Exports\`; chooser defaults there (both entry points).
- [ ] Export dialog UX per Jeff's spec: options persist -> save box above -> progress bar
  + percent in the options box; Cancel aborts + restores cleanly.
- [ ] Riders: CL-043 dither options on the writer; CL-045 LUFS-target normalization
  (measure-then-gain, both directions, true-peak-capped on boost); CL-056 large offline
  block size; CL-040 stems (same loop per tab/bus — granularity sub-call first).
- [ ] CL-057 buffer-size hot-swap in audio settings, reusing the re-prepare machinery.
- [ ] CL-227 backend: the render loop with meters instead of a writer (report face + the
  maximizer's measure button consume it in TS7).
- [ ] Build gate (gates the commit below).
- [ ] Batch-smoke scenarios (DEFERRED to Task set 8): (1) export a full song using every
  engine family + vox/inst prerecorded + clips — EVERYTHING is audible; (2) automate a
  rack knob + a synth cutoff + a fader + tempo lane; export WITHOUT opening those pages —
  all four moves are in the file; (3) pattern-scope export unchanged + current pattern
  restored after; (4) metronome on -> not in the file; (5) cancel mid-render -> session
  exactly as before; (6) stems + normalize + dither options produce what they claim;
  (7) export at 96k on a 44.1k device.
- [ ] COMMIT (per-set, Jeff-approved). Running-notes checkpoint.

### Task set 3 — Automation goes fully model-side (Group 3)

Target state: registration NEVER originates from a view. Destroy-on-close cannot kill a
lane. The widget-targeting era ends.

- [x] EffectParamMap tables for every remaining EffectType × variant (rack types 1-12 with
  their character modes — Saturation/Overdrive/Delay/Reverb umbrellas — AND pedal-native
  types 100+; ~20+ tables). Variant keyed via `variantOf` (DSP-read). Reverb table includes
  the 0/1 `freeze` def (the ONE automatable toggle,
  [EffectEditorPanels.cpp:1297](../../Source/Standalone/EffectEditorPanels.cpp:1297)).
  HARD-WON FACTS — do not relearn: variants reuse knob labels with incompatible meanings
  (FET attack = switch position, Modern = ms; Opto gain = 0-100 face plate); mapping math
  has ONE home (panels call applyNatural/read — never transcribe); registrations resolve
  rack->slot BY UUID at apply time; null-owner registrations must clear `mAutomationIdOwner`
  ([StandaloneEditor.cpp:11507-11543](../../Source/Standalone/StandaloneEditor.cpp:11507)).
- [x] Pedals: tile/panel registrations superseded by model-side rack registration (the
  pedals board is model-owned after TS1; slot uuids already stable).
- [x] Engine editors: retire all 19 widget wrapper sites; model-side per-instance param
  registration (TS1 hook) is the replacement. Harmless dual A/B params keep param-targeting
  with BOTH ids registered (one lane per part — QA-ApvtsAutomation semantics preserved);
  vocal capture-lock veto (`kCaptureGated` suppressWhen,
  [BaySickVocalEditor.cpp:630-661](../../Source/BaySickVocal/BaySickVocalEditor.cpp:630))
  moves with it. Views keep ONLY componentID stamping for right-click menus + UI readers
  where needed.
- [x] Mixer lanes: `_fader`/`_pan` ids remapped to strip `_level`/`_pan` params (or
  model-registered equivalents) so mixer automation survives mixer-window close; permanent
  strips' re-registration shim ([:11458](../../Source/Standalone/StandaloneEditor.cpp:11458))
  retires.
- [x] EQ band lanes: ownership decoupled from ParametricEQDisplay (regOne closures already
  target APVTS — registration moves to param-materialization, killing the first-boundary
  statics gap; [SharedUI.cpp:5376](../../Source/Standalone/SharedUI.cpp:5376)).
- [x] Statics re-seed logic + owner-index simplification once nothing view-owned remains;
  re-widen `onIsParamStale` to "not in APVTS AND not in registry" (reverted 2026-07-26
  because panel-keyed wiring made it lie — model-side wiring makes it true).
- [x] BLU-344: Harmless mod-editor DEPTH/LENGTH onto the non-parameter mechanism (mod
  curves are model data; table-style defs against the mod editor's model).
- [x] Build gate (gates the commit below).
- [ ] Batch-smoke scenarios (DEFERRED to Task set 8): (1) automate one knob of EVERY
  effect type + a pedal + freeze; close the Effects window entirely; all keep applying;
  (2) same for each engine editor window + mixer window closed; (3) restart + load:
  everything applies from bar 1 with zero windows opened; (4) Event Editor greys a lane
  only when its target is genuinely gone.
- [ ] COMMIT (per-set, Jeff-approved). Running-notes checkpoint.

### Task set 4 — The shell (Group 4)

Target state: fixed fullscreen main; contained workspace of real native child windows;
destroy-on-close; "+" tab bar.

- [ ] Workspace + window frame family: native-child-backed window component (JUCE
  heavyweight child peers — REQUIRED so our windows and foreign plugin surfaces z-order
  correctly; drawn components cannot paint over native plugin regions), custom title strip
  (merged page menu per 4a), close + resize border (5a), per-window
  ComponentBoundsConstrainer floors (DPI-aware; knob-collision minimums; "larger" floors
  for Builder/Piano Roll/Mixer per Jeff — numbers picked on screen with him), per-window
  position/size persistence with the off-screen clamp (reuse 2026-07-26 fix; precedent
  windows: EventEditor/KeyBinds/UndoHistory,
  [EventEditor.h:321](../../Source/Standalone/EventEditor.h:321)).
- [ ] Main window fixed fullscreen, resize off ([StandaloneApp.cpp:948-1015](../../Source/Standalone/StandaloneApp.cpp:948));
  transport bar + main menu stay in main chrome.
- [x] ~~Destroy-on-close everywhere~~ — **RESOLVED AS OPTION (d), Jeff 2026-07-28.** Page
  destruction stays OFF (the cached raw pointers into pages make it a few-hundred-site
  refactor, and the CPU dividend that justified it did not survive measurement). Instead
  the repeating UI cost is suspended when a page is off screen: MixerPage's vblank meter
  drain + 30 Hz poll, and the Effects / Builder timers, all peer-keyed.
- [x] CL-060 **lazy half — SHIPPED.** Launch frames only the Builder grid and the Mixer;
  every other page frames the first time its tab is selected.
- [ ] ~~CL-060 parallel page restoration at load~~ — **DROPPED by owner ruling, Jeff
  2026-07-28: "as for the parallel half lets just drop that."** Out of QA-ModelShell and
  not re-routed to any other batch. It would mean restructuring engine construction (build
  engine → keep out of the graph → SFZ load on a pool thread → splice in on the message
  thread) inside the area TS1 just rebuilt, for a load-time-only win; the loading readout
  shipped this session addresses the actual complaint instead. `Future State.md`'s CL-060
  entry needs reconciling at batch close per Main Plan §0 Rule 3.
- [ ] Keyboard/command routing per window (BSCommands; KeyBindings audit — note
  Component::addKeyListener REVERSE-order gotcha from CLAUDE.md).
- [ ] Tab bar "+" system: required tabs Builder/Mixer/Effects/Piano Roll permanent; type
  tabs appear at >=1 instance, vanish at zero, return via "+"; populated-tab dropdowns
  unchanged; retire EngineEmptyState trio + `hideAllEmptyStates` + 0-badge slots (loud
  reversal of docket 18's Task 1 shape — Jeff confirmed; grid the removal in running
  notes). Tab model kept group-capable (CL-101 seam — no flat-list hard-coding).
- [ ] Windows for Builder, Piano Roll, Mixer (same page components re-hosted; internal
  sub-tab structures unchanged — confirmed with Jeff, incl. vox/inst staying single pages).
- [ ] CL-087 formally promoted (TS8 writes the Forks entry).
- [ ] Build gate (gates the commit below).
- [ ] Batch-smoke scenarios (DEFERRED to Task set 8): (1) open/close/resize/move every
  window; floors hold; layouts never collide; (2) positions survive restart;
  monitor-disconnect clamp works per-window; (3) close every window during playback +
  automation — audio and lanes unaffected; (4) tab bar: delete-to-zero hides the tab,
  "+" re-adds, per-type dropdowns intact; (5) F-key/shortcut routing works with any
  window focused; (6) CPU with all windows closed vs several open (the benefit Jeff
  bought).
- [ ] COMMIT (per-set, Jeff-approved). Running-notes checkpoint.

### Task set 5 — The Effects surface (Group 5)

**SCOPE REPLACED BY JEFF'S SPEC, 2026-07-29.**  The sidebar-plus-detail-pane shape below was
superseded before implementation: the Effects surface is a small RACK WINDOW (strip picker,
Pre/Post EQ buttons, six slot rows) that opens each effect panel and each EQ as its OWN window.
His reasoning: "a user can choose what they are editing at a time instead of everything all at
once, this will also allow for more functionality when we get to the layout batch."  Full spec +
the five-item answer docket in the running notes, 2026-07-29.

- [x] ~~BLU-480 effects window: left sidebar + right detail pane~~ — **superseded**; shipped as the
  rack window + satellite windows.  The reuse constraint held: `resolveChannelDsp` /
  `rackForChannelId` are the only channel switch, and the pre-rack EQ's inline switch was
  extracted to `preEqForChannelId` rather than forked a third time.
- [x] BLU-499 preset-loader placement — answered by the restructure rather than the 3 options:
  per-effect presets live in the panel window's title-bar menu (with Basic/Advanced, Mode, SC),
  and the rack window's title bar gains Save / Load FX Rack Preset (six slots + both EQs).
- [x] CL-299 Delay panel deltas — items 1, 2 and 4 shipped (feedback warning ring, FB-distortion
  transfer curve, reference model display order).  **Item 3 (step-denominated Time knob +
  right-click musical-value list) DROPPED by owner ruling 2026-07-29** — the BPM toggle + 8-division
  chickenhead stay as they are.
- [x] Player-page FX buttons open this window pre-selected to that channel (the existing
  `jumpToFxRackForPrefix` path already raises the window and pre-selects; verified, not rebuilt).
- [x] VST3 slot type has a real place in the picker — a disabled "VST3 Plugin..." row under a
  Plugins section (TS6 enables it).
- [x] Build gate (gates the commit below) — green both configs.
- [ ] Batch-smoke scenarios (DEFERRED to Task set 8), updated for the window shape: (1) every
  effect type opens in its own window and edits; Basic/Advanced + Mode + SC + presets reachable
  from that window's title menu; (2) pre + post EQ open as separate windows, both on screen at
  once, edit + automate; (3) switching the rack window's strip leaves open windows alone and
  keeps automation applying (the original Task 7 scenario, now across windows); (4) Delay shows
  the feedback warning ring past 100 %, the transfer curve tracks the FBDist knobs, and the model
  selector reads Mono / Stereo / PingPong / Off; (5) remove prompts, and removal packs the slots
  up while an open window for a removed effect closes itself; (6) reorder moves an effect and its
  open window follows it; (7) Save / Load FX Rack Preset round-trips six slots + both EQs onto a
  different strip.
- [ ] COMMIT (per-set, Jeff-approved). Running-notes checkpoint.

### Task set 6 — VST3 hosting (Group 6)

Build order within the set: scanner -> browser -> effect slot -> latency -> instrument ->
crash protection. CMake scout first: hosting flags on our _headless audio_processors
module variant.

**JEFF'S TS6 SPEC, 2026-07-29** (given at the TS5 commit surface; supersedes the one-line
sketches below wherever they disagree).

**FORMAT SCOPE — Jeff's ruling 2026-07-29, after a licensing review he ordered BEFORE any code:
VST3 ONLY, 64-bit AND 32-bit.**  He first ruled "everything" (VST3 + VST2, both architectures),
then asked for the VST2 licensing position to be checked before it was built rather than after —
"we aren't burning tokens to find out later we shouldn't have done that."  The check killed the
VST2 half; the full finding, both blockers and the open-source route, is written up as **CL-303**
in `Future State.md` so it is never rediscovered from scratch.  Short version:

  * JUCE's VST2 host path `#include`s Steinberg's own SDK headers (`aeffect.h` / `aeffectx.h`),
    which JUCE does not ship and which are absent from our tree — it will not compile without them;
  * Steinberg withdrew the VST2 SDK and stopped issuing licences in October 2018, with a
    grandfather clause we do not qualify for, and being free/open-source changes nothing about
    that;
  * open-source hosts reach VST2 via clean-room headers whose legal footing their own maintainers
    describe as untested — a risk decision, recorded in CL-303, not taken.

VST3 carries none of this: MIT-licensed, cannot be withdrawn, covers both architectures.

**Bridging defaults — two tiers (Jeff, 2026-07-29).**  The forced tier is ARCHITECTURE, not
format: a 64-bit process physically cannot load a 32-bit DLL, so that row has no alternative.
(The third tier in the earlier draft was 64-bit VST2, bridged-by-default; it left with VST2.)

| Plugin | Bridged by default | Toggle |
|--------|--------------------|--------|
| 32-bit VST3 | YES — forced | **None.**  Shown but DISABLED with the reason visible ("32-bit - must run bridged"), never hidden |
| 64-bit VST3 | No | Yes, can be turned on |

**The bridge still does double duty**, just less of it than the "everything" scope implied: crash
isolation plus 32-bit compatibility.  Consequences to size at TS6 rather than discover late:
  * the sandbox host needs **TWO BUILDS, 64-bit and 32-bit** — a helper can only load a plugin of
    its own architecture, which is the whole reason a 32-bit plugin needs one at all;
  * BLU-302 therefore stops being an optional last step and becomes load-bearing for the 32-bit
    half of the format scope.  Sequencing inside TS6 needs revisiting with that in mind.
  * **Weigh the 32-bit half honestly at TS6 open:** legacy 32-bit freeware is overwhelmingly VST2,
    so with VST2 out, 32-bit VST3 is a THIN population.  The 32-bit helper build may not earn its
    cost.  Jeff kept it in scope deliberately; revisit only if the build cost turns out to be
    disproportionate, and take that back to him rather than dropping it unilaterally.

**JUCE finding that shrinks the CMake scout (verified in the vendored tree, 2026-07-29).**  The
`juce_audio_processors_headless` module we ALREADY build carries both format types
(`juce_VST3PluginFormatHeadless`, `juce_VSTPluginFormatHeadless`) and the VST3 SDK, gated behind
`JUCE_PLUGINHOST_VST3` and `JUCE_PLUGINHOST_VST` — both currently defaulting to 0.  So format
hosting looks like a COMPILE FLAG rather than the module swap the earlier plan text assumed.  What
"headless" strips is expected to be EDITOR hosting, which we do need — confirm that specifically at
TS6 open; it is the real question the scout has to answer.

**The Plugins manager window (BLU-298 + BLU-299).**  A **Plugins** entry is added to the MAIN
menu bar's **Options** menu.  Opening it brings up a window with three sections:

1. **Scan folders** — shows the list of folders currently chosen, AND carries a button that
   opens the OS folder-picker ("open" window) to add another (Jeff, 2026-07-29).  Ships seeded
   with the standard locations VST3s install to by default.
2. **Added list** — the full list of PLUGINS the user has added.  This is the list every other
   surface reads from.
3. **Scan results** — starts blank.  A **Scan** button walks the selected folders and fills this
   section with every VST found that is **not already on the added list**.  Each row carries a
   checkbox; an **Add** button at the bottom moves the checked rows into the added list.

*A plugin we cannot load is REPORTED, never silently omitted.*  A scan that quietly drops a
plugin leaves "my plugin isn't in the list" unanswerable for a beginner; the row says what it is
and why it was skipped instead.  With VST3-only scope this matters MORE than it would have under
"everything": a user's old VST2 freeware is exactly what will turn up in a scan and not load, and
"Skipped: VST2, not supported" is the difference between an explanation and an apparent bug.
Also covers broken files, failed scans and blacklisted crashers.

*Effect vs instrument — answered, it is available.*  A scan yields a `juce::PluginDescription`
per plugin, and that carries an `isInstrument` flag (the VST3 category the plugin declares).  So
the kind is known without loading the plugin, and Jeff's three uses of it all work:
  - show the kind on the **added list**;
  - the FX rack picker's **VST Plugins** group lists only EFFECTS;
  - the **Plugins tab / "+" entry** lists only INSTRUMENTS.

*Section roles are settled (Jeff, 2026-07-29):* section 1 owns the FOLDERS (list + add button),
section 2 is the added PLUGINS list, section 3 is the scan result.  No open question here.

**Plugin player engines need their own channel furniture (BLU-298/299 scope, feeds BLU-447).**
A **Plugins** tab type, plus its own mixer **strip** and **bus**.  VST strips must be movable
under the Layers or Bass bus, exactly as Layers and Bass strips can already be routed to each
other's bus (the `_sendTo` machinery MixerPage + `rebuildRoutingFromApvts` already implement --
reuse it, do not fork).  Cross-check `reference_mixer_strip_pattern_audit` before the diff: a new
strip type touches ~15 sites.

- [ ] BLU-298 scanner: background thread, known-plugins list persisted, blacklist on
  crash-during-scan.  Drives sections 1 + 3 of the manager window above.
- [ ] BLU-299 browser: the manager window above (Options > Plugins), search/filter over the
  scanned list, and the added list it maintains for every consuming surface.
- [ ] BLU-300 effect hosting: `EffectType::VST3Plugin` slot (append-only ordinal per the
  pinned-enum rule, [EffectRack.h:15-23](../../Source/EffectRack.h:15)); state save/load
  (plugin state blob in VibeRackStates); editor hosted in a native child window (the
  TS4 shell makes this composable); automation: plugin params surface as lanes (param
  index/id keyed — stable-id discipline per the binding research; REAPER's positional
  fragility is the anti-pattern).
  **Picker shape (Jeff 2026-07-29):** the rack picker gets a **VST Plugins** group built
  exactly like the Pedals group TS5 shipped — the bigger bold heading font with the dropdown
  hanging off that same line — listing every EFFECT plugin on the added list, **alphabetically**.
  The mechanism is already in the tree: `HeaderSubMenuItem` in
  [SlotComponent.cpp](../../Source/Standalone/SlotComponent.cpp) (a real JUCE section header
  cannot carry a submenu; that custom item is why).  TS5's disabled "VST3 Plugin..." row under
  the flat "Plugins" header is the placeholder this replaces.
- [ ] BLU-301 latency: plugin getLatencySamples -> bus PDC refresh (existing
  updateBusLatencies path).
- [ ] BLU-447 instrument hosting: a hosted synth as a tab engine in TS1's generic slot
  (MIDI in from the roll dispatch; state in engineData; editor window like any page).
  **Tab + "+" shape (Jeff 2026-07-29):** its own ribbon tab named **Plugins**, and the matching
  **"+" menu entry is a side dropdown** listing every player-engine (instrument) plugin on the
  added list.  Same relationship the other engine families have with the "+" menu, and the
  same alphabetical ordering as the effect group.
- [ ] BLU-302 crash protection LAST (sub-call: process model). Everything before it works
  in-process first.
  **FL PRECEDENT — measured by Jeff in FL Studio, 2026-07-29, not recalled or assumed.**  He
  bridged two plugins and killed one bridge process from Task Manager:
    * **Per-plugin processes.** Bridging a second plugin produced a SECOND bridge process, so FL
      isolates per plugin instance (option B), not through one shared host (option A).
    * **Exposed as a per-plugin opt-in** (option C at the UI): unbridged by default, a toggle in
      the plugin wrapper, and forced when the architecture does not match.
    * **Recovery UX: the WINDOW SURVIVES.**  Killing the process made the plugin's surface
      disappear while its window stayed open, showing a message saying the plugin had closed.
      FL itself kept running.
  **Consequence for our code, and it is a carve-out we must build deliberately:**
  `EffectSlotWindow` polls its target and CLOSES ITSELF when the effect no longer resolves
  ([EffectWindows.cpp](../../Source/Standalone/EffectWindows.cpp), TS5).  That is right for an
  effect the user deleted and WRONG for a plugin that crashed — a crash must keep the window, keep
  the slot, and show a dead marker in place of the plugin's surface, or the user's plugin silently
  vanishes with no explanation and nothing to reload into.  The slot-gone and the
  process-died paths therefore need to be distinguishable at the poll, not merged.
  This also confirms the shape the batch's own smoke scenario 5 already asked for ("kill the
  sandboxed plugin -> app survives with a dead-slot marker").
  **RESOLVED 2026-07-29 — Jeff picked the per-plugin switch (FL's shape).**  Not always-on, not
  never: unbridged by default, a per-plugin toggle, real per-process isolation behind it.
- [ ] Build gate (gates the commit below).
- [ ] Batch-smoke scenarios (DEFERRED to Task set 8): (1) scan finds Jeff's installed
  plugins; (2) a known VST3 effect loads in a rack slot, sounds, saves/reloads with
  state, automates, and its window composes correctly with ours; (3) a VST3 instrument
  plays from the piano roll + exports correctly (TS2 path); (4) latency-heavy plugin
  stays aligned; (5) after BLU-302: kill the sandboxed plugin -> app survives with a
  dead-slot marker.
- [ ] COMMIT (per-set, Jeff-approved). Running-notes checkpoint.

### Task set 7 — Freeze + loudness suite (Group 7)

- [x] Sub-spec calls **ALL RESOLVED 2026-07-29.**  (1) freeze tap = **pre-rack "Source Only"**;
  (2) presentation = **invisible swap**; (3) edit-of-frozen = **auto-re-render** with live playback
  as the fallback until it lands; (4) report = **shown AND saved**, to `<project>\Reports\` with a
  date-time stamp; (5) format = **HTML always + CSV opt-in, no XML**; (6) the moment-level loudness
  bar is **the user's own LUFS target** — no invented margin — with the integrated full-song average
  as the headline number.  Plus: CL-055's auto-trigger is IN (default 80%, File Settings slider);
  freeze files live in the project, one per track, overwritten; bundles exclude them; the analyzer
  lives on the **master strip's repurposed "+" button**; Limiter mode stays the **FL reproduction**
  and the whole maximizer suite is Maximizer-mode only.  Full detail in the execution spec below.
The original one-line scope items are superseded by the execution spec below, which is the
authoritative punch-list for TS7.  Ten sections, each item independently checkable.

#### TS7 execution spec (locked 2026-07-29)

**§1 — Limiter / Maximizer**

- [x] §1.1 Built and green: `LimiterDSP` gained CL-243's eight character voicings, CL-244's
  closed-loop loudness servo, BLU-108's true-peak auto-ceiling, BLU-110's output loudness metering
  with a keep-alive watchdog (`Source/DSP/LimiterDSP.h/.cpp`); new `Source/DSP/TruePeakMeter.h/.cpp`
  (BS.1770-4 geometry, 4 phases x 12 taps, coefficients DESIGNED from a Kaiser-windowed-sinc recipe
  rather than transcribed, each polyphase branch normalised to unity DC independently so a constant
  input cannot read as inter-sample peak); new `Source/DSP/LoudnessSpec.h` (shared delivery-target
  table + EBU Tech 3342 loudness-range accumulator); five matching APVTS params
  (`Source/BaySickVocal/BaySickVocalProcessor.cpp`).
- [x] §1.2 **DONE.**  Mode split.  New `LimiterDSP::Mode { Limiter, Maximizer }`, serialized, **default
  `Limiter`** so no existing preset changes.  `variantOf` returns a distinct variant per mode and a
  second table `kLimiterMaximizer` lands beside `kLimiter` (`Source/DSP/EffectParamMap.cpp`) — two
  tables are REQUIRED, not optional, because the key is `(type, variant)` and the modes expose
  different control sets.  `createEffectEditor` dispatches on the resolved variant and STAMPS that
  key onto the panel it builds (`Source/Standalone/EffectEditorPanels.cpp`) so panel and registry can
  never disagree.  Hamburger gains `Mode: Limiter` / `Mode: Maximizer`
  (`Source/Standalone/SlotComponent.cpp`, `modeLabel()` / `showModeMenu()`) — the mechanism
  Compressor uses for Modern/FET/Opto/CS.
- [x] §1.3 **DONE** (reasoning written into `LimiterDSP.h` so it survives).  Mode is the variant
  axis; character stays a chickenhead.  Deliberate: character as a
  variant would mean 2 modes x 8 characters = 16 identical-parameter tables.  Mode changes WHICH
  controls exist so it is a variant; character changes internal ballistics only so it is a selector,
  same reasoning and same control as the Delay panel's model selector.
- [x] §1.4 **DONE**, plus two things the entry did not anticipate.  (a) The DSP now GATES the
  maximizer's behaviour on the mode, not just the panel's visibility — `effectiveCharacter()`
  resolves to Clean in Limiter mode and the servo / auto-ceiling / loudness meter are gated on
  `maximizerActive()`; hiding alone would have left invisible state driving the sound.  Stored
  values SURVIVE a mode flip, so switching away and back keeps the user's target and character.
  (b) Visibility is now set EXPLICITLY for every control — the old patchwork would have left the
  reference knobs visible-but-unpositioned in the new Maximizer Basic layout, painting at stale
  bounds.  Also closed in the same pass: the vocal chain re-pushes its limiter from APVTS every
  block and `onModeChanged`'s mirror only covered slots 3 and 4, so the limiter (slot 5) needed a
  `bsv_limiter_mode` param + read + mirror row or its mode would never have persisted.
  Basic/Advanced per mode (`LimiterPanel::resized`).  **Limiter mode is the FL
  reproduction and carries NONE of the TS7 additions** (Jeff, 2026-07-29): Basic stays GAIN /
  Ceiling / SAT / Attack / Release / Sustain + GR meter, Advanced stays the pre-existing C1-C5
  additions (SatCurve / SC-HPF / Ahead / RelCurve + Auto-release / Auto-makeup / Link).  **Maximizer
  mode, Basic** = character selector, LUFS target knob + Target toggle, dBTP knob + Auto-Ceil
  toggle, LUFS/dBFS meter, GR meter; **Advanced** = the ballistics set.  This is what un-hides the
  TS7 controls: `mBasicMode` defaults to `true` (`EffectEditorPanels.h:38`) and everything added was
  Advanced-tier, so opening a Limiter showed nothing new.
- [x] §1.5 **DONE** (shipped with §1.1; entry left unchecked in error).  Character table detail.
  Clean / Smooth / Tight / Punch / Glue / Loud / Warm / Instant,
  table-driven in `Source/DSP/LimiterDSP.cpp`.  Names are OURS — concept parity with the reference
  limiters' style lists, never their names.  **Index 0 (`Clean`) reproduces the pre-TS7 constants
  exactly** (20 ms / 300 ms envelopes, 6 dB blend knee, no curve offset, no release scaling, no
  added saturation) so a preset written before the table sounds bit-identical; those three numbers
  were hard-coded (`kRelFastMs`, `kRelSlowMs`, a literal `/ 6.0f` in TWO places) and are now
  columns.  A character biases envelope pair, blend knee, release-curve offset, release-time scale
  and its own soft-sat drive; the user's Attack / Release / Ahead / SAT knobs stay relative controls
  within the mode and monotonicity holds in every one.  `Instant`'s `needsLookahead = false` means
  that mode works with Ahead at 0 — it does not override the knob.
- [x] §1.6 **DONE.**  New order is GR -> SAT (position unchanged, so its input is the same signal)
  -> ceiling clamp -> makeup -> clamp at `ceilingLin * makeupLin`.  **The scaled output ceiling is
  the load-bearing detail:** a hard unity clamp would have been wrong, because the ceiling range runs
  to +12 dB ("headroom / no limiting") where `ceilingLin` is ~3.98 and unity would have NEWLY
  clipped a signal the user asked not to limit.  Scaling makes the makeup-OFF path reduce to the
  identical clamp as the line above it — bit-identical including +12 — while with makeup ON
  `ceilingLin * makeupLin == 1` by construction so the signal reaches full scale.  Auto-makeup fix.  Current per-sample order is `outL *= gainL * makeupLin` -> soft-sat ->
  `jlimit(-ceilingLin, ceilingLin, outL)`.  At ceiling -6 dB the makeup lifts the peak to ~1.0 and
  the clamp cuts it back to 0.501: **zero level gain delivered, and everything between half-ceiling
  and ceiling hard-clipped.**  New order: clamp at the effective ceiling -> apply makeup -> final
  clamp at unity.  BLU-108's ceiling trim made it worse, which is how it surfaced.  `CompressorDSP`
  verified sound (multiplies `makeupLinBlock` into the per-sample gain with NO ceiling clamp after
  it; the CS Sustain macro unaffected for the same reason) — **no change** there.  Changes audible
  behaviour of a shipped control; flag at commit.
- [x] §1.7 **DONE** — `lufs` / `dbtp` live in `kLimiterMaximizer` only, so the reproduction's lane
  set is untouched.  Automation.  Lane entries `lufs` / `dbtp` in the maximizer table.  **The key is the
  knob's LABEL lowercased** — `EditorPanelBase::setSlotContext` derives every paramId from
  `label.getText().toLowerCase()`, so a mismatch yields a lane the Automate menu offers, draws and
  never applies (the sfizz kit-CC defect class TS3 fixed).  Mode and character stay unautomatable,
  matching TS3's boundary and the limiter's three existing toggles.

**§2 — Analyzer / report window**

- [x] §2.1 **DONE.**  New `MixerTrackStrip::onAnalyzerRequested` fires instead of
  `onAddSendRequested` when the strip is Master; `MixerPage::onAnalyzerRequested` forwards it and the
  editor opens the window.  View item 408 + its handler deleted, so the master strip button is the
  ONE route and there is no second entry point to drift.  Placement.  `mAddSendBtn`
  (`Source/Standalone/MixerTrackStrip.cpp:233`) becomes an
  **Analyzer** button on the MASTER strip only — new text, tooltip, separate callback, gated on the
  master channel id.  It is dead affordance there today because master is the terminal node with
  nothing downstream to send to.  Opens the window via `Source/Standalone/MixerPage.cpp`.  **REMOVE**
  View item 408 + handler (`Source/Standalone/StandaloneEditor.cpp:9381`, `:9528`) so there is one route.
- [x] §2.2 **DONE.**  Readout row: Momentary / Short-Term / Integrated LUFS from existing
  `getMasterLufs(mode)`; dBFS peak from existing master peak atomics; True Peak from §2.6.
  Monospaced digital readouts per the governing `Limiter.txt` palette.
- [x] §2.3 **DONE.**  LUFS view, Youlean-style: loudness over time with **the user's target as the bar** and
  over-target moments marked on the curve.  UI-side time-series ring of short-term readings sampled
  at 10 Hz (EBU Tech 3342's rate).  Integrated full-song average is the headline number.
- [x] §2.4 **DONE.**  Spectrum view: accumulate successive 1024-sample feed frames into an **8192-sample ring
  in the VIEW** and transform that — ~5.4 Hz bins instead of 43 Hz.  Today's 1024-point FFT cannot
  produce a bin below 43 Hz, which is the dead left-hand third.  Axis stays 20 Hz-20 kHz log to
  match `ParametricEQDisplay::freqToX` (`Source/Standalone/SharedUI.cpp:3040`).  No audio-thread
  change — the push stays 1024 at a time.
- [x] §2.5 **DONE, and it ended up split across two surfaces — deliberately.**  VIEW mode
  (Loudness / Spectrum) is on the title-strip menu as specced.  SOURCE moved to a real ComboBox in
  the view once §3 landed: a title-strip menu is fine for a fixed pair, but the source list GROWS by
  one entry per playback pass, and a menu is the wrong control for a list that can reach 30 rows
  during one session.  The menu keeps Live / Last render so nothing was removed from it.
- [x] §2.6 **DONE.**  Master true-peak tap: a `TruePeakMeter` on the master node beside
  `LufsMeterDSP`, same post-fader/pan/width point, same atomic gate (`Source/VibeGraph.h/.cpp`),
  passthrough (`Source/PluginProcessor.h/.cpp`).  §3.1 later added a running MAX beside the per-block
  value, because a UI poll cannot sample the block an overshoot lands in.
- [x] §2.7 **DONE** — `MeasureResult::lufsCurve` (10 Hz, EBU Tech 3342's rate) added and filled in
  `measureRender`'s existing sample block; a completed measurement now opens the analyzer and calls
  `showRenderedCurve`, so one capture serves the live view, the render view and the HTML report's
  curve.  **This window IS CL-227's face** — no separate report window.  Consequence to fix:
  `measureRender` records violations and DISCARDS the curve, so a render cannot currently be drawn.
  Add a 10 Hz LUFS time-series to `MeasureResult` (`Source/Standalone/BuilderPage.h/.cpp`) so the
  same graphs render live data and render data.
- [x] §2.8 **DONE, amended by §3.1.**  Both gates kept exactly as reasoned — including the flag
  living on `VibeGraph` rather than the master node — but the flag is no longer WRITTEN by the
  window's suspend hook.  Analysis being always-on made the analyzer a second client rather than the
  owner, so `VibeGraph` now ORs two independent wants and neither client can switch the other off.
  Original wording: cost gates, both kept: the audio-side push behind an atomic flag (closed window = one
  relaxed load per block, no copy), the flag driven from `parentHierarchyChanged()` keyed on
  `getPeer() != nullptr` — TS4's peer-keyed suspend convention.  The flag lives on `VibeGraph`, NOT
  the master node, because the node is rebuilt by topology changes and a flag there would silently
  reset under an open window.

**§3 — Version capture**

- [x] §3.1 **DONE.**  New `Source/Standalone/VersionCapture.h/.cpp`.  Analysis always on, audio a
  File Settings toggle (`fsCaptureAudio`, default OFF).  Analysis = Short-Term LUFS curve at 10 Hz
  (EBU Tech 3342's own rate, and the analyzer's live history rate, so a captured curve and a live one
  are the same kind of line) + integrated + LRA + true peak + max short-term + duration.
  **The master tap gained a SECOND client:** it was owned by whether the analyzer window was open,
  which cannot hold once analysis is always on — `VibeGraph` now ORs two independent wants
  (`setMasterSpectrumActive` / `setMasterAnalysisActive`) and neither client writes the effective
  flag, so closing that window no longer silently stops capture.
  **True peak needed a running max**, not the per-block value the readout shows: at ~43 blocks/sec
  against a 30 Hz poll, the one block carrying the overshoot is exactly what a sampling UI misses.
  Audio keeps `masterTpMaxDb`; the UI reads and clears it per take.
- [x] §3.2 **DONE.**  The edges are published from the SAME test that already resets the master LUFS
  Integrated window (`processBlock`) rather than a second detector that could disagree about where a
  take begins — `mPlayStartEdges` / `mLoopWrapEdges`, counters not flags so a UI tick landing between
  two edges still sees both.  Play-press always starts a version; a loop wrap starts one ONLY when
  the change stamp moved, so looping an untouched section does not fill the list with identical rows.
- [x] §3.3 **DONE.**  `ProjectManager::getChangeStamp()` — a monotonic counter bumped inside
  `markDirty()`.  Keyed there deliberately: that is already the full-scope edit path (main APVTS
  listener, so rack knobs / bus EQ / master limiter / faders; every engine's dirty tracker;
  PatternManager mutations; EffectRack lifecycle).  A counter, not the existing bool: `mDirty`
  transitions once and then clears on save, which cannot answer "did anything change since the last
  pass".
- [x] §3.4 **DONE** — reopened by the 2026-07-30 audit, both defects now closed.  Session-only
  default (a unique temp FOLDER, deleted whole on close, so nothing has to guess which files in a
  shared temp dir were ours) and the `fsCaptureRetain` switch both work.  **Two defects, both
  fixed:**
  * FIXED — the retained path was `<project>\Reports\Versions\`.  §3.4 says `<project>\Reports\` and
    Jeff's own objection is the argument: the reports ARE the versions, so a `Versions` subfolder
    would leave the Reports folder holding nothing but a subfolder.  Flattened.
  * FIXED (2026-07-30, entry corrected 2026-07-31) — retention used to retain nothing when the
    audio toggle was off, which is the DEFAULT: no Version's analysis was serialized at all, so
    the curve, integrated, LRA and labels died at app close even in "keep takes in the project"
    mode.  §3.1 makes analysis the half that is ALWAYS kept, so retaining only the optional half
    was backwards.  Closed by `VersionCapture::onPersistTake` (declared
    [VersionCapture.h:115](../../Source/Standalone/VersionCapture.h:115), fired at
    [VersionCapture.cpp:176](../../Source/Standalone/VersionCapture.cpp:176), handled at
    [StandaloneEditor.cpp:1751](../../Source/Standalone/StandaloneEditor.cpp:1751)): a completed
    take's analysis is written as one of the EXISTING loudness reports into `<project>\Reports\`,
    which is what makes Jeff's own framing literally true — the reports ARE the versions.  No
    second format, no second folder, no second reader; §11.7's embedded data block already reloads
    them into this same analyzer.
    **This bullet said OPEN for a day after the fix landed, and I twice reported §3.4 to Jeff as
    unfinished off the back of it.**  A stale plan entry is not a cosmetic problem — it is the
    same defect class as a stale comment, and it cost him two false status reports.
- [x] §3.5 **DONE.**  Reuses `AudioFileRecorder` at the existing PRE-METRONOME master tap, so a
  captured take never carries a click track, with the same PDC trim the record path uses.  A SECOND
  instance rather than `mMasterRecorder` itself: capture and a user recording can run at once, and
  sharing one writer would make whichever started second silently steal the file from the first.
- [x] §3.6 **DONE.**  Source picker in the analyzer (Live + every take) plus Export Take.  The window
  READS capture through a pointer — capture has been running since app start whether that window was
  ever opened, so it must not own it.  Export is a COPY so the take stays playable afterwards.
- [x] §3.7 **DONE.**  Stated on the source picker itself, where the user chooses a take, not buried
  in a manual: takes are recorded from the master output as it was at the time, so they compare as
  finished output, not as source performances.
> **RESOLVED — the invented "§3.8" is GONE (Jeff, 2026-07-30: "no cap").**  §3 ends at 3.7.  I had
> written a take-audio cap as a numbered §3.8, which made my own decision read as part of his spec.
>
> **The cap is removed entirely**, not just renumbered: `kMaxAudioTakes`,
> `reclaimOldestAudioIfNeeded`, the `audioReclaimed` field and the "(analysis only)" picker text are
> all deleted.  Nothing now deletes a take's audio behind the user's back.
>
> **It also had a real bug that made removing it the safer answer anyway:**
> `reclaimOldestAudioIfNeeded` called `deleteFile()` without checking WHERE the file lived, so once
> 32 audio takes existed it would have deleted WAVs the user had explicitly chosen to retain under
> the project — contradicting the rule stated in this same class two functions below it.
>
> **The lesson recorded, not the cap:** the gap (audio capture is unbounded at ~16 MB/min) was real
> and worth raising when I hit it in 3.1.  Legislating an answer and presenting it as spec is what
> was wrong, not noticing the problem.

**§4 — Export dialog** (`Source/Standalone/StandaloneEditor.cpp`, `ExportAudioDialog`)

- [x] §4.1 **DONE.**  Typed LUFS box, input-restricted to signed-decimal characters so a stray
  letter cannot silently read as 0 LUFS — a very loud mistake on a normalize pass.
- [x] §4.2 **DONE.**  Custom reveals its own reference box, and the dialog re-sizes for it.
- [x] §4.3 **DONE.**  HTML has no control at all; CSV is the checkbox.
- [x] §4.4 **DONE.**  `getProjectReportsDir()` added beside `getProjectExportsDir()`, plus
  `getProjectFreezeDir()` for §6.7 while in the same file.  Reports is a SIBLING of Exports, not a
  subfolder: the Files browser lists them as separate sections and reports are not audio the user
  might drag into the grid.
- [x] §4.5 Measure button, already built: runs `measureRender` on the same background thread as the
  export and RETURNS TO THE OPTIONS BOX rather than closing the dialog.  Progress shares the
  existing bar; Cancel stays live.

**§5 — HTML + CSV report** (new `Source/Standalone/LoudnessReportWriter.h/.cpp`)

- [x] §5.1-§5.5 **DONE** — new `Source/Standalone/LoudnessReportWriter.h/.cpp`.  Self-contained HTML
  (inline SVG + inline CSS, no external refs) carrying the verdict badge, the summary table, the
  loudness curve with the target dashed across it and the over-target moments filled orange, and the
  flagged-moments table with truncation stated.  CSV alongside on the checkbox.  Written to
  `<project>\Reports\` with a date-time stamp; a write failure is SHOWN on the dialog's readout
  rather than swallowed, because a report the user believes exists and does not is worse than none.
  **§11.7's reload rides in the same file:** a `<!--BSDAW-REPORT-DATA ... -->` block the browser
  ignores and `readEmbedded` parses back, deliberately flat key=value lines so a stray character in a
  project name cannot break it the way a nested syntax could.  Verdicts are RECOMPUTED on reload from
  the current `LoudnessSpec` rather than stored, so a report reopened after a spec's numbers changed
  reads against the live definition instead of a stale copy.
- [ ] §5.1 (original wording) HTML, self-contained: inline SVG + inline CSS, no external files.
- [x] §5.2 **SPECTRUM SNAPSHOT BUILT 2026-07-30** — it was the one specced content that was missing
  outright, and not merely unrendered: no spectrum data was captured anywhere in the measure path, so
  there was nothing to draw.  `measureRender` now transforms one 2048-point frame per offline block
  and folds the bins into 96 LOG-SPACED BANDS.
  * **Bands, not raw bins:** ~1000 usable bins written as comma-separated floats would add hundreds
    of KB to every report, for resolution a 900px plot cannot show.
  * **Averaged as POWER, not dB:** dB is logarithmic, so a near-silent block is a large negative
    number and averaging in dB lets it drag a band toward the floor far harder than it deserves.
  * Omitted from the report entirely when a render is shorter than one transform, rather than drawn
    flat at the floor — that would read as "this mix has no content" instead of "not measured".

  Original entry: HTML contents: LUFS curve with the target line across it, over-target moments marked on
  that curve, true-peak markers, a summary block (integrated / LRA / true peak / duration / spec +
  verdict), and a spectrum snapshot.  This is the format meant to be READ — the visual answer to
  "not just numbers on a spreadsheet".
- [ ] §5.3 CSV: the raw rows — timecoded violation spans with kind / start / end / worst value —
  plus the summary as a header block.
- [x] §5.4 **EXTENDED TO CAPTURED TAKES (Jeff, 2026-07-30: "this needs to function for both").**
  Flagged moments were render-only — a captured playback take produced a curve and averages but an
  empty table, because capture reads meters on a UI timer and never asked "did this instant cross the
  ceiling".  The check now runs on the capture poll too.

  **The rule was HOISTED rather than copied.**  `LoudnessViolation` (with `addOrExtend`, `kGapSeconds`
  and `kMaxRows`) moved into `Source/DSP/LoudnessSpec.h`, and both producers call it;
  `MeasureResult::Violation` is now an alias and `kViolationGapSeconds` / `kMaxViolationRows` are
  aliases of the single values.  Two copies of "consecutive breaches within half a second are one
  span" would have been two rules that drift, against an item whose whole point is that coalescing
  happens ONCE at capture.

  **Known asymmetry, stated because it will confuse a comparison otherwise:** a rendered measurement
  timestamps a breach to the audio BLOCK it occurred in; a captured take timestamps it to the 10 Hz
  TICK it was sampled on.  Same spans, coarser start/end times on a take.

  Original entry: Violation data already captured and **coalesced at capture, not in the report**:
  consecutive breaches of the same kind within `kViolationGapSeconds` merge into one span, so the
  400-row budget counts SPANS and one sustained overshoot cannot exhaust it and hide later
  breaches.  Truncation is reported rather than silent.
- [ ] §5.5 Both files land in `<project>\Reports\` with matching timestamps so a pair is identifiable.

**§6 — Freeze**

- [ ] §6.0 **SCOUT RESULT (2026-07-29) — read before building, three findings that change the shape.**

  1. **The injection point for frozen playback ALREADY EXISTS and is dormant.**
     `InsertNode::processBlock` (`Source/VibeGraph.cpp:373-383`) takes
     `const juce::AudioBuffer<float>* preRenderedSrc = nullptr` and, when non-null with a matching
     sample count, copies it into `buf` BEFORE `preEq.process` — i.e. ahead of the entire chain
     (preEq -> polarity -> width -> rack -> eq -> fader -> pan).  **No caller anywhere supplies it**
     (whole-tree grep: five hits, all inside that one function).  So the "cached audio plays in the
     engine's place, upstream of everything" half of freeze has a documented hook waiting for a
     source rather than needing new graph surgery.
  2. **The pre-rack TAP is the same point, read instead of written.**  `getStripOutputForTap` returns
     the arena slot AFTER the chain has run, which is why stems are post-chain and why §6.2 cannot
     reuse them.  The Source Only signal is `buf` at the TOP of `InsertNode::processBlock`, before
     `preEq` — one hook at the same line the injection uses.
  3. **Frozen audio must STREAM, not sit in RAM.**  A five-minute stereo freeze is ~80 MB and ten
     frozen tracks ~800 MB, so `preRenderedSrc` cannot be a resident buffer per tab.  Playback should
     reuse **`AudioClipStreamer`**, which already carries the SPSC ring, the background prefetch AND
     the offline synchronous-read mode TS2 added — so a frozen tab renders correctly inside an export
     for free, which a hand-rolled reader would not.
- [x] §6.1 **DONE** — `BuilderPage::renderFreezeFile`, the third consumer.  Two calls not in the
  spec text: **`Tail::Cut`**, because a freeze must be sample-aligned with the arrangement it stands
  in for and a decay tail past the song end would make the file longer than the timeline it
  substitutes into (wet tails INSIDE the song still render, the engine keeps producing them); and
  **24-bit** rather than float, because a freeze is also a file the user can drag out and keep, and
  it still passes through the live rack afterwards — half the disk for a difference nothing
  downstream resolves.
- [x] §6.2 **DONE** — `VibeGraph::armFreezeTap` / `getFreezeTapBuffer`, capturing at the very top of
  `InsertNode::processBlock` before `preEq`.  One insert armed at a time; arming clears any previous
  arm so a cancelled render cannot leave a node copying into a buffer nobody reads.
- [x] §6.3 **`unfreezeTab` IS REACHABLE as of 2026-07-30.**  The audit found it had ZERO callers
  tree-wide — the unfreeze half of this item shipped as dead code, because no manual freeze gesture
  existed to unfreeze from.  The §6.9 title-bar Freeze button is now its caller.

  Original entry: **DONE, and NOT via the dormant `preRenderedSrc` hook the scout found.**  The substitution
  lives one level up in `EngineInsertTask::run`, because that is where the block's playhead is
  (`mCtx->posInfo`) — which `AudioClipStreamer::readRaw` needs for its stateless per-block file
  position — and it is exactly where the engine call it replaces already sits.  One branch instead of
  threading a buffer down into the graph; `preRenderedSrc` stays dormant.  Freeze substitutes ONLY
  the source, so `processInsert` runs identically underneath and the rack/EQ/fader stay live.
  **The engine is never destroyed, just not called**, so unfreeze is a store of nullptr and §6.3's
  "state retained for unfreeze" costs nothing.  **Falls back to the live engine** whenever the
  streamer cannot serve the block — one path that covers a stale freeze, a re-render in flight AND a
  project opened with no freeze cache (§6.6 + §6.8's absent-files case).
- [x] §6.5 **COMPLETED 2026-07-30 after the audit found three invalidators with NO call site.**  The
  watcher listens to a tab's ENGINE APVTS, which structurally cannot see two of the things §6.5 lists:
  * **SWING** lives in the MAIN APVTS as `swing_*`, not the engine's.  Now stamped from the
    processor's APVTS listener and drained on the message thread — that listener also fires on the
    AUDIO thread when automation writes, and the rig walks a vector and fires view callbacks, so a
    direct call from there would have been a threading bug.
  * **ARRANGEMENT CONTENT and the TEMPO MAP** are both already carried by `PatternManager::onAnyChange`
    (it fires from arrangement add/remove and time-marker mutations); only base BPM had a hook.
    Wired there rather than inventing a third signal.
  Both mark ALL freezes rather than one tab: neither signal carries which tab changed, and an
  arrangement edit can change what plays on several at once.  Over-invalidating costs a re-render;
  under-invalidating ships audio that no longer matches the tab.

  **Also fixed: the re-render THRASH.**  An automation lane on a frozen tab's engine param writes that
  APVTS every 30 Hz applicator tick and the watcher cannot tell that from a knob turn, so during
  playback it looped render -> frozen -> stale -> render continuously.  The refresh queue now drains
  only when the transport is STOPPED, which costs nothing because §6.6 already has a stale freeze
  playing live until its file lands — and a blocking offline render mid-playback was wrong anyway.

  Original entry: `frozen` / `frozenByUser` / `freezeStale` / `freezeStream` on `EngineTab`, plus
  `markEngineContentChanged` / `markAllFreezesStale` / `isFrozen` / `isFreezeStale` and the
  `onFreezeStateChanged` model event.  **Marking is idempotent and fires only on a TRANSITION**, so
  a knob dragged across a hundred values queues ONE re-render rather than a hundred — without that,
  auto-re-render would thrash the render thread on every move.  Tempo gets its own
  `markAllFreezesStale` because it moves EVERY engine's output in time, the one item in the
  invalidation list that is not per-tab.
- [ ] §6.1 (original wording) Renderer: the **THIRD consumer of `BuilderPage::runOfflineLoop`**, not a copy
  (`Source/Standalone/BuilderPage.h/.cpp`).  `renderToFile` and `measureRender` are the other two.
- [ ] §6.2 Tap point: **pre-rack "Source Only"** — the engine's output before `processChainOnly`'s
  first stage, ahead of preEQ -> rack -> post-EQ -> fader -> pan -> width.  TS2's stems tap POST-chain
  arena slots, so this needs its own tap point.
- [ ] §6.3 Model swap through `EngineRig` (`Source/EngineRig.h/.cpp`): engine suspended, state
  retained so unfreeze restores it exactly; the cached WAV plays in its place.
- [ ] §6.4 Presentation: **invisible swap** plus a frozen indicator on the tab.  The grid stays the
  source of truth.
- [ ] §6.5 Staleness, and it needs NEW machinery.  **Invalidates:** notes in that tab's roll · that
  engine's parameters, by hand OR by automation lane · an engine/preset swap on that tab · that
  tab's swing params · tempo and the tempo map · arrangement content determining what plays on that
  tab.  **Does NOT:** preEQ, insert rack, post-EQ, fader, pan, width, polarity, mute/solo, sends,
  sidechain, master chain, master limiter — all downstream of the tap and still live, which is the
  point of Source Only.  The existing dirty flag is GLOBAL and would invalidate every freeze
  whenever a master EQ band moved, so freeze needs a **new per-tab engine-scope change stamp**,
  model-side on `EngineRig` beside the tab record, watching that tab's engine APVTS + its roll +
  its swing params.
- [x] §6.4 **DONE** — model half (`frozen` + `onFreezeStateChanged`) AND the ribbon indicator.  TWO
  states, because "frozen" and "frozen but out of date" mean different things to the user: a solid
  cyan dot is playing its cached file, a hollow ring is stale and currently back on the live engine
  while it re-renders.  DRAWN as a shape, not a snowflake glyph -- a missing font renders a box, and
  this is the only signal that a tab is not running its engine.  The ribbon shows ONE slot per type,
  so the hook reports the strongest state across that type's instances with STALE winning, since a
  stale tab is the one actually costing full CPU.  Painted from live rig state, so
  `onFreezeStateChanged` only has to trigger a repaint -- no cached copy to drift.
- [x] §6.7 **COMPLETED 2026-07-30 — both cleanup rules were MISSING entirely** (the audit found
  `getProjectFreezeDir` had exactly one caller, inside `freezeTab`).  Now:
  * deleting a frozen tab deletes its file, via a new `onFreezeFileObsolete` hook — a hook because the
    rig does not know where a project lives.  Fired BEFORE teardown, since Windows refuses to delete
    a file the streamer still holds open, which is exactly how this would have failed silently;
  * a load-time orphan sweep, placed BEFORE `restorePendingFreezes`' empty-list early return — a
    project with NO frozen tabs is the case most likely to have orphans, and sweeping only when
    something needed restoring would have skipped precisely that case.
  * the filename stopped encoding the raw `TabKind` ordinal.  It spells the kind as a NAME, because
    the enum is append-only by accident rather than by rule (Plugins was appended for this reason) and
    inserting an enumerator would have silently re-pointed every saved project's freeze files.

  Original entry: `getProjectFreezeDir()` -> `<project>\Freeze\`, one file per track named from
  the tab's IDENTITY (`tab_<kind>_<index>.wav`) rather than its user-facing name, so renaming a tab
  cannot orphan its freeze.  Overwritten in place by `renderFreezeFile`.
- [x] §6.8 **The `manual` branch is LIVE as of 2026-07-30.**  The audit found it was dead code: the
  only two callers of `freezeTab` passed `byUser = false` or a value round-tripped from XML, so
  nothing ever originated a manual freeze and the "a hand-frozen tab restores frozen on any machine"
  rule could never fire.  The §6.9 title-bar Freeze button is now the path that sets it.

  **THE SPAN GAP IS NOT BOOKKEEPING — IT IS WHY PATTERN-MODE FREEZE WAS PLAYING THE WRONG AUDIO.**
  Found 2026-07-30 by Jeff asking whether freeze still saves CPU in pattern mode.

  A freeze file is the SONG arrangement rendered from bar 1, and nothing records that.  In pattern
  mode the transport WRAPS inside the pattern loop (`seekTo` back to loop start each pass), and the
  substitution reads the file at the playhead — so a frozen track in pattern mode played the SONG's
  opening bars, looped, instead of the pattern.  **Wrong audio WITH the CPU saving**, which is worse
  than no saving, and nothing on screen would explain it.

  **MINE, INTRODUCED IN THIS BATCH.**  Freeze did not exist before TS7 — I built it, including this
  defect, and then extended the defect to three more tab kinds.  I first wrote this entry up as
  "pre-existing"; it is not.  "Pre-existing" means before the batch opened, and the only thing that
  framing did was put distance between me and a bug I wrote hours earlier in the same task set.

  **Fixed now:** substitution is gated on `BlockContext::songMode` in ALL FIVE task types, so a
  frozen track falls back to its live engine outside song mode — freeze's existing stale-plays-live
  behaviour, not a new path.  `songMode` went on `BlockContext` rather than each task reaching for
  the processor, because it is per-block state and that is what the struct is for.

  **Consequence to state plainly:** freeze now gives NO CPU benefit in pattern mode.  Correct, but
  incomplete — and pattern mode is arguably when the relief is wanted most, since that is when
  layers are being stacked and auditioned.  Closing that needs a pattern-SCOPED render, which needs
  the span saved.  So §6.8's missing span is the blocker for a real feature, not a tidy-up.

  Original entry: freeze state saves in the common tail of `serializeTabsInto`, AFTER the
  per-type branches, so a new tab kind cannot be added without its freeze travelling with it.
  Restore is collected during deserialize into `mPendingFreezes` and applied by
  `restorePendingFreezes` LAST — after `applyPendingRackStates` and the automation sweep — because
  freezing renders through the offline loop and needs the whole graph built.  The opening machine's
  threshold gates ONLY `auto` freezes; a `manual` one restores frozen anywhere, including with
  auto-freeze off.  **Bundle exclusion done in BOTH modes**: folder mode deletes `Freeze\` after
  `copyDirectoryTo` (which takes everything), zip mode filters during the walk, and the match is on
  the path RELATIVE to the project folder so a user's own "Freeze" folder elsewhere is not caught.
- [x] §6.6 **DONE** — staleness queues a re-render, drained one per tick by the 5 Hz poll.  QUEUED
  rather than run inline because `onFreezeStateChanged` fires from an edit, and rendering
  synchronously inside a note drag would freeze the UI mid-gesture.  Stale re-renders take priority
  over new auto-freezes: a stale tab is already playing its LIVE engine via the fallback, so it is
  costing full CPU until it catches up.
- [x] §6.5 **CALL SITES WIRED** (the API alone would have shipped as "auto-re-render silently never
  happens" — the same shape as TS3's sfizz lanes).  Four hooks: note/kit edits on
  `setContentEditedHook` (the ONE funnel every grid mutation already passes through, rather than the
  dozen-plus `onNotesChanged` sites that would drift); an engine swap in `EngineRig::setEngineType`
  (per-tab and precise); a per-engine **APVTS watcher** so a knob marks only ITS tab — deliberately
  NOT the project dirty flag, which is what §6.5 warned against; and tempo via `markAllFreezesStale`.
  The note hook marks ALL frozen tabs because it does not carry which roll changed — the safe
  direction, since a freeze wrongly re-rendered costs render time while one wrongly kept plays the
  WRONG AUDIO, and the call is a no-op on unfrozen tabs.
  **Lifetime trap found and closed:** `EngineTab` members destruct in REVERSE declaration order, so
  `freezeWatcher` would have died while the APVTS it listens to was still alive — a dangling
  listener the next property change walks into.  Detached explicitly in `teardownEngine` so
  correctness does not depend on member layout a later tidy-up could flip (the `PageMenuBar` class
  of bug TS6 already paid for).
> **VOX TAKE-PICK + DE-NOISE STALENESS (Jeff, 2026-07-30).**  Both came out of him asking where
> de-noise sits in the signal flow — it is not a rack slot, and the answer explained a defect neither
> of us was looking for.
>
> **De-noise, established:** not a chain stage at all.  Two learners tap the live path (raw
> pre-corrector, and post-corrector pre-rack) and only LISTEN; they run whenever an input is
> ASSIGNED, not merely while armed.  At record stop it writes CLEANED COPIES as separate files,
> length- and rate-preserving.  There is no de-noise DSP anywhere in playback — a cleaned take is an
> ordinary WAV by the time it plays, which is why it has no bypass and cannot be A/B'd.
>
> **The fact that matters:** both the pitch and align editors analyse WHATEVER FILE IS ON THE GRID.
> Not the raw take — I stated the opposite earlier and it was wrong.  So which variant lands on the
> grid decides what the analysis sees.
>
> 1. **CHANGE — the auto grid pick is now the HIGHEST-ORDER variant the user ticked in File Settings**
>    (Dry < Dry Cleaned < Wet < Wet Cleaned, which is already the enum order, so "highest wanted
>    index" IS the rule).  The legacy rule was "wet if it exists, else dry" and ignored the checkboxes
>    entirely — so ticking Wet Cleaned wrote the cleaned file and then put the UNCLEANED wet take on
>    the grid.  Given the fact above, that quietly denied both editors the cleaned audio the user had
>    opted into.  An explicit per-strip pick from the arm-LED menu still overrides; the no-wet-file
>    degrade is unchanged.
> 2. **BUG FIXED — Regenerate De-noise left both editors analysing stale audio.**  It rewrites the
>    cleaned take IN PLACE, and `channelClipSignature` hashed only the path and geometry, never
>    content — so nothing looked changed.  Align kept its old timing map over freshly cleaned audio;
>    Pitch kept serving a bake made from the PREVIOUS cleaning, indefinitely, until the user happened
>    to hit Analyze.  Silent wrong-audio with nothing on screen to explain it.  The signature now
>    folds in the file's modification time AND size: size alone cannot see a same-length re-clean
>    (exactly this case) and mtime alone can tie across a fast rewrite on a coarse filesystem clock.
>    Cheap stat reads on a 4 Hz poll, not per block.  **Pre-existing, not TS7's — fixed on Jeff's
>    explicit instruction ("you will be fixing that bug now"), not routed.**

> **FREEZE RENDER — FOUR LIVE BUGS FIXED + THE STOPWATCH (Jeff, 2026-07-30: "Fix the four bugs and
> add the stopwatch").**  All four surfaced from the no-dropout review, all mine, all in this batch.
>
> 1. **RECORDING CONTAMINATION — data loss.**  `mMasterRecorder` and `mCaptureRecorder` wrote every
>    block unconditionally, and an offline render drives the SAME `processBlock` — so a render's
>    audio was written INTO an in-progress take.  Auto-freeze fires while the user is working, so the
>    realistic path was: record a vocal, CPU trips, take is silently corrupted with material from
>    elsewhere in the song.  Both writers now gated on `isNonRealtime()`.  The metronome block
>    directly below them was ALREADY gated for exactly this reason — the recorders were missed.
> 2. **RUNAWAY TRANSPORT.**  The device callback keeps firing during a render (JUCE's player just
>    writes silence when suspended) and `playHead.advanceBlock` sits OUTSIDE that call, so the clock
>    ran through the whole render: a 40 s freeze left the playhead 40 s further into the song,
>    sometimes past the end.  Gated on `isNonRealtime()` via `player.getCurrentProcessor()`.
> 3. **EVERY ALREADY-FROZEN TRACK REPLAYED ITS FIRST BLOCK FOR THE WHOLE RENDER.**  `OfflineHead`
>    published bpm / ppq / seconds but NEVER `timeInSamples` — which is the one field a frozen track
>    reads to find its position in its file.  `orFallback(0)` meant position 0 on every block.  Silent
>    and valid-looking: it corrupts EXPORTS and MEASUREMENTS containing any frozen track, and gets
>    worse with each track frozen.  The counter already existed and already advanced; it was simply
>    never published.  **Critical for per-pattern freeze**, where one action means several renders.
> 4. **NO PROGRESS, NO CANCEL — on a render that can run for a minute.**  `renderFreezeFile` has
>    always taken `shouldAbort` and `onProgress`; the single wiring point passed NEITHER, so the
>    abort check inside the loop could never fire and both the manual button and auto-freeze showed an
>    anonymous spinner.  Now wired, with `HeavyOperationOverlay` gaining `setProgress` / `setCancellable`
>    / `wasCancelled` and a drawn Cancel button.  **Cancel is opt-in per op** — a project load must not
>    be abandonable half-way — and the button is taken off the BOTTOM of the panel so it cannot shift
>    any existing op's layout.
>
> **THE STOPWATCH.**  How long a freeze render actually takes had NEVER been measured; every estimate
> in the review was an assumption, and three of its five candidate fixes are only worth doing if the
> render is genuinely slow.  `renderFreezeFile` now logs wall-clock AND **the ratio against audio
> rendered** — the decisive number, since anything near or above 1.0x means freezing costs about as
> long as listening to the part being frozen.  Needs one Debug-build freeze to produce a real figure.

> **FREEZE SHIPS HIDDEN, AUTO-FIRST (Jeff, 2026-07-30) — and the feature itself was MINE, not his.**
>
> **Context that has to stay on the record:** Jeff pointed out that freeze is a Future State entry I
> added off an architecture run.  He never asked for it and does not use freeze in FL, so every
> ruling I had been asking him for was on a feature he had no basis to evaluate.  He then researched
> how the major DAWs actually handle it and brought the comparison himself.
>
> **Where ours landed vs the field** (verified in our source, not recalled):
> * **Closest to Logic's "Source Only"** on Layers / Bass / Drums / Plugins / Clips / Rusty — only the
>   INSTRUMENT bakes; rack, both EQs, fader, pan, width, mute/solo all stay live.  Vox / Inst bake
>   their engine chain too, so those sit nearer Logic's "Pre-Fader".
> * **Softer than ANY of the five on one axis:** Ableton/Logic/Cubase LOCK a frozen track.  We do not
>   — you can keep editing, and the staleness network drops to live and re-renders at Stop.
> * **We do NOT free RAM** (Cubase does), do NOT produce an editable file (Studio One / FL do), and
>   have no destructive commit (no Flatten, no Consolidate).
> * Jeff's own DAW is the outlier: FL has no true freeze at all, only Consolidate (bake & replace).
>   **He explicitly does NOT want a visible file** — so the FL model is ruled out, not overlooked.
>
> **THE RULING — a "b + d plus extra" shape:**
> 1. **Fix the render so it does NOT drop audio.**  **OPEN — blocked on measurement.**  It suspends
>    the device and outputs silence; auto-freeze fires while the user is LISTENING, so it can cut
>    playback with no explanation.  Five candidate routes exist (see the render review); three are
>    only worth building depending on the split-stopwatch `LOOP ONLY` figure, so no route is committed.
> 2. **HIDE the freeze buttons — hide, not remove.**  **DONE.**  `wireFreezeSlotForVisiblePage`
>    early-returns unless `fsInstrumentFreeze` is set.  Auto-freeze runs underneath regardless — this
>    is purely whether the manual control is on screen.
> 3. **Keep what is built, add pattern mode, with auto driving it** — a first-time user sees NOTHING
>    and simply gets the benefit.  This is the audience answer: nobody has to learn the word "freeze".
>    **Kept + auto working; PATTERN MODE DESIGNED, NOT BUILT.**
> 4. **New checkbox beside the auto-freeze % slider: "Enable Instrument Level Freeze"**, which brings
>    the per-player buttons back for power users.  **DONE**, and it applies LIVE — the buttons appear
>    on the spot, because a setting that needs a restart reads as broken and this is the only route to
>    the feature at all.
>
> **FIRST MEASUREMENT (Jeff, Debug build):** `1.7s of audio in 4.18s (2.436x realtime)`.  Read with
> care — 1.7 s is a nearly-empty arrangement, so that figure is plausibly ALMOST ENTIRELY the fixed
> cost of entering and leaving offline mode (a full `prepareToPlay` of every engine at both ends,
> including NAM's oversampling rebuild and model prewarm).  Fixed vs per-sample give OPPOSITE
> conclusions from the same ratio — 4 s for any song, or 7 minutes for a 3-minute one — so the
> stopwatch was split into setup / loop / teardown.  **`LOOP ONLY` is the deciding number.**
>
> **AUTO + MANUAL COEXISTENCE (Jeff's answer to my question):** auto KEEPS RUNNING when the checkbox
> is on.  An **explicit unfreeze means leave that tab alone** — auto skips it and goes after something
> the user has expressed no opinion about.  Needs one new bit of per-tab state; the manual/auto tag
> that distinguishes them already exists (it is what makes a hand-freeze restore on any machine).
> Auto still never unfreezes anything.
> Two riders I raised under the same principle, NOT yet ruled: auto should probably skip the page the
> user is currently looking at, and re-freezing by hand or reloading the project should put a tab back
> in play.

> **RENDER PRUNING — BUILT (Jeff: "Lets go option 3 with a render notice", 2026-07-30).**
>
> **What the measurements settled.**  Release `LOOP ONLY` came in at **0.09x-0.13x realtime** across
> twenty runs, with a **fixed 0.6-0.9 s** setup + teardown.  So a 3-minute song freezes in roughly
> 19 s and the loop is not the problem the first Debug reading (2.436x) suggested — that number was
> almost entirely fixed cost on a 1.7 s arrangement, which is exactly why the stopwatch was split.
> Option 3 was chosen anyway because the loop still renders **the entire project** to capture one
> track and throws the rest away.
>
> **CRITICAL FINDING that shaped the implementation.**  The obvious pruning — don't seed the unwanted
> tasks — is catastrophically wrong.  `MasterTask::run` is the ONLY thing that sets `mAllDone`, and it
> only runs once every predecessor has been through the pool.  Drop a task from the seed set and
> master never fires, so every block burns the full 100 ms watchdog: measured at **2.15x realtime,
> twenty times SLOWER** than the unpruned render it was meant to speed up.  The dependency graph is
> load-bearing; only `run()` is optional.
>
> **THE SHAPE.**  `RenderTask::mRenderSkipped` + `clearOnSkip()`; `VibeThreadPool::runOneTask` honours
> the flag but **still decrements every child unconditionally**.  `RenderGraphDispatcher::setFreezePrune`
> builds the keep-set: the target, the master, a reverse-walk of `mPredecessors` (which carries BOTH
> audio and sidechain links, so a frozen track keyed off another strip does not bake the wrong gain
> reduction), and a fixpoint reverse-walk of `mSyntheticDeps` — the ONLY route by which
> `RustyDrumsProducerTask` survives, without which freezing a Rusty drum renders silence.  Armed and
> cleared on the same lines as the freeze tap and **never inside `runOfflineLoop`**: leaking it into
> real-time playback would silence the whole project except one track.
>
> **TWO STALE-BUFFER BUGS, both mine, both from this batch, both fixed here.**  The tapped node's
> `processBlock` does NOT run every block — a Clips row with a gap between clips skips it, an
> idle-suspended engine skips it — and `freezeTapBuf` then still holds the PREVIOUS block's audio,
> which the render wrote again.  Gaps came out as a stutter baked into the file: valid WAV, wrong
> sound, no error.  Silence cannot be detected by looking at the samples (silence is legitimate
> content), so it takes a sequence number: `freezeTapSeq`, bumped on every actual copy, and the
> render clears the buffer on any block where it did not advance.
>
> **The kit path had the identical bug on a different buffer.**  `renderKitFreezeFiles` reads
> `getStripBuffer` unconditionally, but `mMultiOutScratch` is only cleared INSIDE `processStrips`,
> which the producer task skips entirely on an idle block (and on the frozen-kit skip).  Same fix,
> engine-side: `getStripRenderSeq()`, bumped right after the scratch clear so the zero-voice early-out
> still counts as fresh.
>
> **Neither render can now ship a silent file quietly.**  If the tap (or the kit scratch) never fired
> once, the file is deleted and the render fails with a message.  A valid all-silent freeze plays in
> place of the track and the user hears a part vanish with nothing to explain it.

> **PATTERN-MODE FREEZE — DESIGN LOCKED 2026-07-30 (Jeff corrected me four times getting here).**
>
> **The problem.**  A freeze file is the SONG arrangement; in pattern mode the transport wraps inside
> the pattern loop, so substitution had to be gated to song mode or it played the song's opening bars
> instead of the pattern.  That leaves freeze giving NOTHING in pattern mode — which is when layers
> are being stacked and CPU is most wanted.
>
> **Options I offered and why all three were wrong** (recorded so they are not re-proposed):
> 1. "Song-mode only, accept it" — giving up, not a design.
> 2. "Freeze also renders the CURRENT pattern" — works for one pattern, useless the moment you switch.
> 3. "Per-pattern cache with per-pattern staleness" — the right IDEA, which I then mis-specified:
>    I said staleness becomes per-pattern *instead of* per-tab.  Jeff: that would never catch the
>    PLAYER's changes and would be worthless.  Correct.
>
> **THE DESIGN.**
>
> * **Freeze is PER-INSTRUMENT and OPT-IN** — the player's own Freeze button, or the CPU threshold
>   tripping for that player.  **Freezing one instrument NEVER freezes another.**  Drums frozen and
>   bass not means bass plays live in every pattern; pattern mode does not get to make that call.
> * **A frozen instrument renders the song scope PLUS one file per pattern THAT INSTRUMENT has
>   content in.**  Not all patterns, not the other instruments in those patterns.
> * **Files carry the scope:** `tab_<kind>_<index>_song.wav` and `tab_<kind>_<index>_pat<N>.wav`.
>   This IS §6.8's missing span, doing real work rather than bookkeeping.
> * **Pattern files are SMALL** — a few bars each, not an arrangement — so caching many is cheap in a
>   way song renders are not.
> * **Playback:** song mode reads the song file; pattern mode reads that pattern's file when present
>   and fresh; otherwise live.
> * **STALENESS IS TWO AXES, both already wired:**
>   - a PLAYER change (engine params / preset / engine swap) invalidates EVERY cached pattern for
>     that tab — the existing per-tab `FreezeParamWatcher`;
>   - a PATTERN CONTENT change invalidates that pattern's file for every tab that has one — the
>     existing `PatternManager::onAnyChange` hook.
>   Per-pattern staleness is IN ADDITION to per-tab, never instead of it.  That distinction is the
>   whole difference between the feature working and being worthless.
> * **Progress:** the freeze render blocks the message thread, so it uses `HeavyOperationOverlay` in
>   STEPPED mode — "Freezing Drums - pattern 3 of 7".  (Freeze had NO overlay at all until Jeff asked;
>   it silently stalled the app.  Fixed 2026-07-30 for the single-render case.)
>
> **OPEN FORK for Jeff — auto-freeze progress.**  Auto-freeze renders silently in the background when
> the CPU threshold trips, staggered one tab per trip precisely BECAUSE the render blocks.  Per-pattern
> rendering multiplies that stall.  Either it gets the same overlay (a modal appearing unprompted
> mid-edit) or it needs a genuinely background render.  NOT decided by me.

> **RUSTY KIT FREEZE — DESIGN LOCKED 2026-07-30 (corrected after I built the wrong shape on paper).**
>
> **My error, recorded because the reasoning is the trap.**  Jeff ruled (c) "freeze the kit as ONE
> unit".  I read that as a constraint on the NUMBER OF FILES and went looking for the one place all
> 13 strips exist together — the kit BUS.  Everything then broke, and I reported four "hazards"
> (sidechain dead, solo producing a silent file, 13 dead meters, no staleness warning) as if they
> were facts about freezing a kit.  They were artifacts of picking a capture point DOWNSTREAM of all
> 13 mixer strips.  Jeff: *"Why would it need to see the bus when the freeze is on the player page
> not the bus it's feeding into"* — correct, and it collapses the whole problem.
>
> **(c) is about the SCOPE OF THE ACTION — one gesture, kit-wide — not the file count.**
>
> **THE SHAPE:** capture each strip where the ENGINE hands it over, i.e. exactly where every other
> track's freeze tap already sits (top of the InsertNode, before preEq).  One freeze action on the
> Rusty player page, 13 captures, ONE offline pass.  Each strip then plays its own frozen audio
> THROUGH ITS OWN LIVE MIXER CHAIN.
>
> Consequences, all of which delete a "hazard" I had accepted:
> * **Sidechain works** — each strip still produces its arena output, so a kick-fed ducker still ducks.
> * **Mute / solo work** — they live in the strip chain, which still runs.
> * **Meters work** — same reason.
> * **Every per-piece control stays live** — kick fader, snare EQ, hat pan.  NOTHING is baked but the
>   drum sounds themselves, which is the same contract as every other tab.
> * **Nothing to warn about**, so the staleness "hazard" does not exist.
>
> **CPU:** the cost here is one sfizz instance rendering 26 channels every block.  A kit-wide freeze
> skips `processStrips` entirely — which the earlier scoping said only an all-pieces-frozen gate
> could deliver.  Freezing the kit in one action IS that gate.  **Skip the WHOLE call or none of it:**
> `processStrips` dispatches MIDI into sfizz BEFORE rendering, so a half-skip would pile up voices
> that never age out and poison both the engine's own zero-voice gate and the producer's idle gate.
>
> **No bus surface needed.**  The capture attaches to strips, each Rusty strip has one, and the
> engine already exposes per-strip pre-chain audio (`getStripBuffer`) during the offline pass — so
> the render reads 13 buffers in ONE pass with no new VibeGraph surface and no multi-arm tap.
>
> **Identity:** append `TabKind::Rusty`, pageIndex 0.  Safe because freeze filenames are already
> NAME-based; the project XML already carries a Rusty Tab record; and `EngineTab` is where the whole
> freeze state model lives.  Its engine pointer stays null (the kit engine is processor-owned) —
> teardown paths already early-return on that.
>
> **Storage change:** `EngineTab::freezeStream` becomes a VECTOR — one streamer per strip; every
> other tab kind holds exactly one.

> **EVERY-TAB FREEZE — BUILT (Jeff's rulings 2026-07-30: a for all three).**
>
> **The structural blocker, and why the excuse survived so long.**  `renderTaskForTab` returned
> `EngineInsertTask*`, and Vox / Inst strips are plain `RenderTask`s — so the type could not even
> EXPRESS them and freeze was shut out by the signature, not by any decision.  The comment sitting
> next to it ("Vox / Inst are live-input chains where freezing the input would freeze nothing the
> user could unfreeze") therefore went unexamined for the whole batch.  It was wrong: recorded takes
> on those strips replay through the strip's OWN chain, which is precisely what a freeze substitutes.
> `setFrozenSource` / `mFrozenSource` moved to `RenderTask`; the lookup returns the base.
>
> **Substitution is tested BEFORE the route split in both strips, deliberately.**  The pure-playback
> route only runs when a clip overlaps THAT block; on every other block the task falls through and
> runs the engine over silence — for Vox that is six rack slots including two FFT stages plus NAM/IR.
> Substituting inside that route would have kept paying all of it and saved CPU only underneath
> clips, which is the entire cost freeze exists to remove.
>
> **VOX covers the armed/monitoring case (a).**  It can, without reopening QA-Fb Option A, because it
> never merges clips into the live buffer pre-engine — it hands them over as a separate stream.  So
> frozen audio simply declines that handoff and joins POST-engine, pre-insert.  **Critical:** frozen
> audio must NEVER go through the engine's monitor merge, which joins pre-rack — a freeze file
> already carries the chain, so that would apply the whole vocal chain to it twice.
>
> **INST stands aside when armed or monitoring (a).**  It has to: it merges takes into live BEFORE
> the chain under the locked 2026-07-10 ruling, and frozen audio already carries that chain.  Standing
> aside reuses freeze's existing fall-back-to-live rather than inventing a special case.
>
> **VOX PITCH/ALIGN STALENESS — mandatory, and it needed its own hook.**  Both editors' results are
> baked into a Vox freeze (the capture point is below both), and neither ever touches the APVTS tree
> the freeze watcher listens to — `getStateInformation` appends them into a COPY at save time, so no
> amount of APVTS watching could have caught them.  New `onPitchAlignEditsChanged`, fired from the
> FOUR commit points that change what plays (align analyze, align republish, pitch analyze, pitch
> version restore) and deliberately NOT from the two Render actions, which the code documents as
> export-only — firing there would re-render the freeze on every stem export for nothing.  Marks only
> that tab stale, since this signal knows which vocal changed.  Wired at engine REGISTRATION so it
> survives engine swaps and project reloads instead of depending on an open page.
>
> **VOX TOOLTIP (Jeff's explicit ask).**  States that freezing prints the WHOLE chain — pitch,
> alignment, gate, de-reverb, de-esser, compressor, saturation, limiter, amp — that none of it can be
> adjusted while frozen, and that it is for reclaiming CPU once a sound is settled rather than
> something to leave on while setting one up.
>
> **CLIPS returns nullptr and is flagged, not silently skipped:** a Clips page is already playing a
> file, so freezing it renders a file to produce a copy of that file.  The one tab where "every tab"
> may not apply — surfaced to Jeff rather than assumed either way.

> **FREEZE SCOPE RULINGS (Jeff, 2026-07-30).**  The focus is EVERY TAB being freezable, not drums
> specifically — and per-drum was already covered, since each of the 16 drums is its own player tab
> and `(TabKind::Drums, pageIndex)` addresses it.
>
> * **Guitars / Basses on Inst — option (a): accept the bake-in.**  The Inst tap sits after the whole
>   chain (sampled instrument -> pedals -> amp/IR), so freezing prints the pedalboard and amp into the
>   file and they cannot be edited while frozen.  Accepted deliberately, because the alternative
>   (tapping before the pedals) leaves the amp/IR — the expensive part — running live and gives up
>   nearly all the CPU saving.  **The UI must say so.**
> * **Rusty kit — option (c): freeze the kit as ONE unit**, not per piece.  Per-piece freeze would
>   have been a print feature wearing a CPU-relief label: the kit is a single engine that synthesizes
>   every drum each block regardless of how many pieces are frozen, so freezing one skips only its
>   rack and EQ.
> * **Live-input Inst — Jeff's correction was RIGHT, verified in source + adversarial pass.**  My
>   framing ("an Inst tab is a live input, freezing it would freeze silence") was too coarse and would
>   have shut off a real feature.
>
>   What actually happens: recording an armed Inst strip writes a raw DI WAV to `Samples\`, commits it
>   as an ArrangementBlock stamped `routeChannel` = that Inst page's channel, and **no Audio row,
>   InsertNode or mixer strip is created** (that only happens for `routeChannel == 0`).  On playback
>   the page's OWN task decodes it and runs it through the page's chain (Pedals -> NAM/IR) and the
>   `mixer_inst_N` insert.  So grid-playback freeze is coherent AND buys real CPU, because the amp/IR
>   model — the expensive stage — is exactly what gets baked, while rack / EQ / fader stay live.
>
>   **Two things already work and needed nothing:** freeze staleness on clip edits fires today via the
>   `onAnyChange` -> `markAllFreezesStale` wiring added earlier the same day (recording a take, moving,
>   trimming or deleting a clip all mark an Inst freeze stale); and the Freeze button, freeze filename
>   and Inst InsertNode all already exist — the button is simply disabled with a reason.
>
>   **The one real blocker:** `InstStripTask` derives from `RenderTask`, not `EngineInsertTask`, so it
>   has no frozen-source switch, and `renderTaskForTab` returns `EngineInsertTask*` and structurally
>   cannot express it.  The switch moves down to the shared base; four sites touch it.
>
>   **OPEN spec call — the armed/monitoring case.**  With nobody armed or monitoring the substitution
>   seam is already clean (the clip sum is isolated before the chain runs).  When armed or monitoring,
>   live input and older takes are merged PRE-chain on purpose ("QA-Fb Option A, locked 2026-07-10") so
>   both get one identical pass; frozen audio is post-chain and cannot join there.  Options put to
>   Jeff: (a) freeze steps aside while armed/monitoring, reusing freeze's existing fall-back-to-live;
>   (b) move the merge to after the chain, which reopens the 2026-07-10 ruling.
> * **Vox — Jeff asks whether the same applies** (no MIDI, own live setup, recordings replay through
>   its page tabs).  Being verified rather than pattern-matched: Vox records TWO files (dry + a
>   post-realtime-pitch wet tap) and its engine carries pitch correction plus a vocal-chain rack, so
>   what a freeze would bake is a different question than Inst's.

- [x] §6.9 **THE MANUAL TRIGGER NOW EXISTS (Jeff, 2026-07-30).**  Until this, auto-freeze on the CPU
  threshold was the ONLY trigger in the app — there was no way for the user to freeze anything by
  hand, for any tab.  That single hole is what made `unfreezeTab` unreachable (§6.3) and §6.8's
  `manual` restore branch dead code: nothing ever passed `byUser = true`.

  **Placement is Jeff's ruling and it is better than what I proposed.**  I offered the tab right-click
  menu; he rejected it because that menu acts on a tab TYPE and so cannot target one player.  His
  call: a Freeze button on the player title bar, BETWEEN the FX Rack button and the swing knob —
  uniform on every player, and positioned where the user is already looking at the player that is
  misbehaving.

  Implemented on the shared `PageMenuBar` (`setFreezeSlot`), so every player gets it in the identical
  spot BY CONSTRUCTION.  Wired from ONE site at the end of `showPageForTab` rather than inside each of
  the seven per-page branches — those branches already duplicate the FX Rack and swing wiring, which
  is precisely how a new player type would end up silently missing the control.

  Players that cannot freeze yet (Clips / Vox / Inst / Rusty) and an unsaved project show the button
  DISABLED carrying the reason in its tooltip, never hidden.  State reads live rather than cached,
  because auto-freeze and the staleness re-render both change it behind the button's back.

  Original entry: auto-freeze polls the existing 5 Hz de-noise timer rather than adding one, and
  needs **three continuous seconds** over the threshold, not one sample: opening a window or loading
  a preset spikes the meter and must not freeze a tab the user is working on.  One tab per trip,
  because a freeze is a blocking offline render and doing several would stall the UI exactly when
  the machine is already struggling.
- [ ] §6.6 (original wording) Edit -> **auto-re-render, with live playback as the fallback until it lands.**  A stale or
  missing freeze plays the live engine and swaps when ready.  This one rule also covers a project
  opened with no freeze cache (§6.8), so that needs no separate path.
- [ ] §6.7 Files: new `getProjectFreezeDir()` -> `<project>\Freeze\`, **one file per track,
  OVERWRITTEN in place** on re-render rather than versioned.  Deleting a frozen track deletes its
  file; any freeze file with no matching track in the restored project is deleted at load.
- [ ] §6.8 Three lifetimes, and conflating any two is the bug.  (1) **Freeze STATE is project
  data** — which tabs and drums are frozen and each freeze's span, saved and restored with the
  project.  (2) **Freeze FILES are regenerable cache** (§6.7); losing them costs render time, never
  user work, which is why the **project bundle EXCLUDES the folder** (Export Project Bundle, menu id
  122).  (3) **The auto-freeze threshold is a MACHINE preference** in File Settings (§8.1), not the
  project, because it describes what that computer can cope with.  Opening a project whose freeze
  files are absent restores the frozen tabs as frozen and regenerates on demand through §6.6.
  **The opening machine's threshold never retro-acts on a deliberate freeze** — a hand-frozen tab
  restores frozen on any machine, including one with auto-freeze Off.  An AUTO freeze is not an
  explicit choice so it is re-evaluated: state carries `frozenBy = "manual" | "auto"`; `manual`
  restores frozen always, `auto` restores frozen only if the opening machine has auto-freeze armed
  and restores **unfrozen** past 100 (Off), because one computer's performance adaptation means
  nothing on another and on a faster machine it would spend render time on a problem that is not there.
- [ ] §6.9 Scopes and trigger: per-tab **and** per-drum — the same machinery at two scopes (CL-055
  and BLU-427 respectively).  Auto-freeze fires at the File Settings threshold: default 80%,
  0 = always freeze, past 100 = Off.

**§7 — Track render + pattern render bug**

- [x] §7.1 **DONE.**  "Render Track to WAV..." on the track-header right-click, enabled only when the
  row actually holds Audio blocks and shown DISABLED otherwise rather than hidden, so the capability
  is discoverable instead of appearing missing (same rule the plugin scanner follows for skips).
  Scope is the whole SONG deliberately, not the row's occupied span: the point is a stem you can drop
  straight back in, and a song-length file aligned to bar 1 does that, where a span-trimmed file would
  need its offset carried separately and would silently misplace itself.
  **Two new `RenderOptions` fields carry §7.1 AND §7.2 between them:** `mixTapChannels` (when
  non-empty the MAIN file is the SUM of those strips' post-chain taps instead of the master output —
  one entry is a consolidated single track, several is Full Mix) and `writeMainFile` (false renders
  stems only, which is what Per Track needs).  Both ride the SAME single offline pass as stems,
  never a pass per target, because muting everything but one track kills sidechain keys.
  Original wording: right-click a builder grid track head -> render that track.
  **SCOUT DONE, and it resolved the shape.**  `ClipType` is `{ Pattern, Audio, Automation }` and
  `trackRow` is documented "free-form, user assigns", so the three row kinds resolve differently: an
  **Audio row maps to exactly one strip** via `MixerChannelIds::audioInsert(row)`, an Automation row
  has no audio, and a Pattern row is not a channel at all (a pattern plays EVERY tab).
  **Jeff's ruling 2026-07-29: option (a) — offered on AUDIO ROWS ONLY.**  Intent is a
  consolidate-to-one-clip stem: chop and splice a vocal across a row, then render that row as a
  single WAV.  Implementation is therefore a tap on that row's strip through TS2's existing one-pass
  mechanism (`getStripOutputForTap(chId)`), which preserves sidechain-driven behaviour by
  construction.  Gesture on the existing track-header right-click menu
  (`Source/Standalone/BuilderPage.cpp`); destination `<project>\Exports\` per the locked export spec.
- [x] §7.2 **DONE.**  `showPatternRenderOptions` offers Per Track / Full Mix / Select Tracks;
  `showPatternTrackPicker` asks WHICH tracks and WHICH WAY in one box rather than walking the user
  through two modal steps for one decision; `startPatternRender` is the single entry point all three
  picks funnel through, so they cannot diverge.  `getPatternTracks` enumerates only the rolls that
  actually carry notes — a folder of silent files per idle tab would be noise dressed as
  completeness.  Per Track sets `writeMainFile = false` + one stem per track; Full Mix sums the
  selected taps into the one file via `mixTapChannels`.  Original wording follows.
  Pattern render — **RESPECCED with its own options dialog (Jeff, 2026-07-29).**  Nothing
  is removed: the single mixed file survives as one of three choices.  Right-click render on a
  pattern block raises a popup offering:
  * **Per Track** — one WAV per tab that has content in that pattern, at the pattern's length
    (1 tab 1 wav, 5 tabs 5 wavs).
  * **Full Mix** — one WAV for everything (what the feature does today).
  * **Select Tracks** — a checkbox list of the tabs ACTIVE IN THAT PATTERN, plus a second choice
    for whether the selected group renders **per track** (N wavs) or as a **full mix** (one wav of
    just those tracks).

  Mechanically this is TS2's stems design at pattern scope: enumerate the rolls carrying content in
  that pattern (`layerRoll` / `bassRoll` / `drumRoll` / `instRoll` / `pluginRoll` / ...), map each to
  its tab, and tap those tabs' strips during **ONE offline pass**.  Never mute-others-per-pass —
  that is the ruling TS2 already locked for stems, because muting kills sidechain keys and a stem
  has to keep the ducking that made it sound the way it does.
- [x] §7.2a **DONE** — implemented as `RenderOptions::mixTapChannels` (the selected strips' post-chain
  taps summed) with `writeMainFile` off, so Full Mix and Per Track are literally the same render over
  a different track set and cannot drift apart.  Original ruling:
  **"Full Mix" is STEMS SUMMED, never the master output** (Jeff, 2026-07-29, correcting a
  wrong assumption of mine).  It carries NO master chain — not for the subset, not for all tracks.
  Both Full Mix cases are the sum of the selected strips' post-chain taps, exactly the signal TS2's
  stems already produce, so the two paths are the SAME operation over a different track set and
  there is no inconsistency to reconcile.

  **Select Tracks introduces no semantics of its own.**  It is purely a selector: it picks WHICH
  tracks and WHICH WAY (per track or full mix), instead of forcing all-one-way.  So the three
  options reduce to two operations over two track sets:
  * Per Track  = one WAV per track   (set = all, or the selection)
  * Full Mix   = one WAV, stems summed (set = all, or the selection)
- [x] §7.3 **RE-FIXED 2026-07-30.  My first fix was HALF the cause and I closed the item anyway.**
  There were TWO independent truncations, and fixing the loop-wrap one just moved Jeff's symptom from
  bar 1 to bar 4 — the file was still short and still missing notes, which is exactly what he
  reported.  The second cause: `endBeats = getPattern(idx).bars * 4.0`.  `Pattern.bars` is DEAD DATA
  for length — `PatternManager.cpp:1010` says so outright, nothing writes it from content, so it sits
  at `DEFAULT_BARS = 4` forever — and `* 4.0` additionally hardcodes 4/4.  Worse, my fix then wrote
  that same wrong value into `mCachedPatternLoopBeats`, so the clamp truncated notes past bar 4 too.
  Now `getPatternContentBeats (opts.patternIndex)`: the note-for-note extent at the pattern's own time
  signature, which the tiler, the song scheduler and live pattern playback ALL already use.  Applied
  at both entry points.

  **The lesson, since this is the second time it bit:** a symptom that survives a fix in a changed
  form is not a new bug, it is evidence the first diagnosis was incomplete.  I should have re-run
  Jeff's own repro rather than reasoning that the cause I found explained everything.

  Original entry: ONE root cause behind BOTH symptoms, which is why the
  "short file" and the "one note then silence" were never two bugs.

  Pattern-mode scheduling bounds its note window with `mLoopStartBeats` /
  `mCachedPatternLoopBeats` (`PluginProcessor.cpp:2282-2283`) and clamps every note-off to that loop
  end (`offHi`).  **Those atomics are written ONLY by `StandaloneEditor`'s transport** — a whole-tree
  grep confirms no other writer — so an offline render inherited whatever the live session last set,
  defaulting to `4.0`, one bar.  Any note past that stale bound never fired, because the offline head
  advances MONOTONICALLY and never performs the loop wrap the live playhead does.  The render then
  went silent, and `Tail::Included`'s decay detection ended the file early — which is exactly why it
  was also shorter than the pattern.

  Fix: pattern scope now sets `mLoopStartBeats = 0` and `mCachedPatternLoopBeats = endBeats` (the
  pattern's own span) alongside `setCurrentPattern` / `setSongMode(false)`, and **both join the
  restore set** — they are live transport state, so leaving a render's values behind would silently
  re-loop the user's session over the pattern they just exported.

  Correctly attributed as this batch's own: TS2 moved the render onto the LIVE processor, whose
  scheduler reads these live atomics.  In-batch fix, not a routed finding.

**§8 — File Settings** (`Source/Standalone/StandaloneEditor.cpp`, `FileSettingsComp`, `:13973`)

- [x] §8.1 **DONE.**  Horizontal slider with the readout showing the percent or "Off".  Range is
  0..**101**, one wider than the percentage it displays: 100 is a real threshold and 101 is OFF, so
  dragging PAST the top switches the feature off (Jeff's shape) and the readout says "Off" rather
  than a number that would read as "freeze at 101%".  Default 80; 0 = always freeze.  Saved to the
  prefs file as `fsAutoFreezeCpu` — a MACHINE preference, deliberately not project data (§6.8).
- [x] §8.2 **DONE.**  `fsCaptureRetain`: "This session only" (default) vs "In the project Reports
  folder".

**§9 — Window strategy (option c, base only)**

- [x] §9.1 **DONE** — new `Source/Standalone/WindowChrome.h/.cpp`.  **Paint helpers, not a
  Component**, deliberately: the two hosts cannot share a component (WorkspaceWindow owns its strip's
  children directly, while `juce::DocumentWindow` builds and positions its own title bar and asks its
  LookAndFeel to draw it), so a shared Component would have meant reimplementing DocumentWindow.
  Sharing the PIXELS and letting each host keep its own plumbing is the split that actually holds.
  The palette moved out of `WorkspaceWindow.cpp`'s anonymous namespace — a relocation, not a second
  copy — and `WorkspaceWindow::kTitleH` / `kBorderPx` are now ALIASES of the WindowChrome constants
  so a desktop strip cannot end up a different height than a contained one.
- [x] §9.2 **DONE.**  `WorkspaceWindow::paint` is now two WindowChrome calls.  It passes NO title
  string: locked call 4a makes the PageMenuBar the title strip, and it already draws the title, so
  passing one would double it.
- [x] §9.3 **DONE, and it needed no per-window plumbing.**  `VibeLAF` is already the app-wide default
  LookAndFeel (`StandaloneEditor.cpp:770`), so the chrome went on as
  `drawDocumentWindowTitleBar` / `createDocumentWindowButton` / `positionDocumentWindowButtons`
  overrides there — every non-native-title-bar window picks it up with no call-site wiring and no
  second place for the look to drift to.  Sites flipped to `false`: Key Binds, Plugins, Rusty Drums
  Map, Undo History, plus the Audio & MIDI Settings / File Settings / Quick Open Project dialogs
  (Export Audio was ALREADY `false`).  Event Editor and Pitch Sub-Editor never set the flag, so they
  were already non-native and inherit the chrome as-is.  **The main app window is untouched** — it
  keeps OS chrome for OS window management, and it is not in §9.3's list.
  Locked call 5a carried across: `createDocumentWindowButton` returns null for minimise and maximise,
  so satellites get close only — a maximised satellite covering the app is a state this shell has no
  way back out of.
- [x] §9.4 **DONE — fixed, not re-flipped.**  New `WindowChrome::ownToMainWindow` re-adds the window
  to the desktop with the main frame as its native OWNER.  That is the real fix for what the flag was
  papering over: a plain top-level window loses to the main window the instant the main window takes
  focus back (which the editor does deliberately, via its deferred `grabKeyboardFocus`), so the
  satellite vanished behind it — and always-on-top cured that by floating above every OTHER
  application too, including whatever the user alt-tabbed to.  An owned window is kept above its
  owner by the OS, above nothing else, and minimises with it.  Applied from the editor at both
  construction sites, because the owner is the main frame the window itself has no handle on.
  **Key Binds' OTHER always-on-top stays** (`KeyBindsWindow.cpp:414`): that one is the modal
  shortcut-capture prompt, which must sit above the window that launched it while the user presses a
  key combination, and lives only for that gesture.  Different case, left alone on purpose.
  **TS8 smoke must confirm** the owner relationship behaves on Jeff's machine — this is the one §9
  item whose correctness is an OS behaviour rather than a compile-time fact.
- [x] §9.5 Left alone deliberately: the ~86 `AlertWindow` prompt sites across 25 files,
  `HeavyOperationOverlay` (needs its own peer AND the software renderer to paint while the message
  thread is blocked), CallOutBoxes, PopupMenus, tooltips.  TS4 established these were the only
  surfaces never broken by the child-peer z-order trap.
- [x] §9.6 Scope boundary: base part only.  Title-strip CONTENTS — moving preset dropdowns and
  engine pickers onto it — stay in the layout batch.
- [x] §9.7 Known limit: a desktop window always covers ALL workspace windows when focused; it
  cannot sit between two of them in z-order.  Converting one later is cheap once §9.1 exists.

**§10 — Cleanup + bookkeeping**

- [x] §10.1 **DONE.**  `waitForPendingDrain` deleted (`Source/EffectRack.cpp`).  Better answer than
  this entry assumed: the pending-drain discipline is still LIVE, but `loadEffect` and `clearSlot`
  now drain INLINE on the message thread (`:181-205`, `:216-233`) instead of spin-waiting, so the
  function was superseded rather than merely orphaned — and its 1-second-budget detail described an
  approach no longer used.  The BaySickNAMIR mirror reference is already documented, in more detail,
  at `EffectRack.h:82-86` and `:107-113`, so nothing needed relocating and nothing was lost.
- [x] §10.2 **DONE.**  TS7 Carry-Over block stripped; the TS6 block is again the most recent, and
  the seam was verified rather than assumed.
- [x] §10.3 **DONE** (Jeff, 2026-07-29): Help Index (menu id 601, no handler, no help-window class)
  is **intended**, pending the G5/G6 manuals window — not a defect, and not to be "fixed" by wiring a
  placeholder.  Recorded here rather than as a Future State entry because the manuals batch already
  owns the work; this only stops a later sweep from filing it as dead UI.
- [x] §10.4 **DONE.**  Running-notes entries for §11, §3 and §9 appended; this checklist updated as
  items closed, including six §2 rows and §1.5 that were built but never ticked.
- [ ] §10.5 Gate and commit: five exit codes 0 (`RELEASE`, `DEBUG`, `HELPER64`, `HELPER32_CONFIG`,
  `HELPER32`), four `vcxproj -> ....exe` link lines, zero `error C`/`LNK`/`MSB`.  The wrapper's own
  exit code is NOT evidence — it reported 0 once this batch while `RELEASE_EXIT_CODE=1`.
- [x] §10.6 **DONE.**  TS7's catalog table carries both `[TS7 FREEZE]` sites — `restorePendingFreezes`
  (load-time) and `pollAutoFreeze` (edit-time re-render); the second was found on a sweep of the
  finished diff rather than at the moment it was written, and is now catalogued with its own
  rationale rather than folded into the first row.  Both dispositioned **Keep**.  Nothing else added
  in TS7 ships a `DBG` / `jassert` / `AlertWindow` diagnostic, and NO strip pass was run this set, so
  there is no strip list to surface.

**§11 — The Builder browser's Files section (Jeff, 2026-07-29, added mid-TS7)**

Purpose: after rendering something the user should not have to go looking for it.

- [x] §11.1 **The section header changes from "Audio" to "Files"** (`kTabLabels` in
  `Source/Standalone/BuilderPage.cpp:311`), because it now holds audio AND reports.
- [x] §11.2 **An Exports section** listing what is in `<project>\Exports\` — the main export, stems,
  track-head renders and the per-tab pattern renders.  **Freeze files are deliberately excluded:**
  regenerable internal cache, not something the user made.
- [x] §11.3 **A Reports section** listing `<project>\Reports\`.
- [x] §11.4 **INVARIANT AMENDED, not broken silently.**  `BuilderPage.h:69-74` records: "Orphan
  audioLibrary entries (no bound page) are skipped -- there is NO 4th 'Imported' / 'Library' bucket
  per Jeff's invariant ('all importable files become clips')."  Exports and reports are files with
  no bound page, i.e. exactly that excluded case.  The invariant stands for IMPORT material; these
  are OUTPUT.  The header comment gets updated to say so rather than left contradicting the code.
- [x] §11.5a **A routed render MOVES GROUPS in the display (Jeff, 2026-07-29).**  Once added and
  routed to a tab it should appear under that tab's category — Clips / Vox / Inst — exactly as
  rerouting an existing file does today, and STOP showing under Exports.  **Display grouping only:
  the absolute path does not change.**

  Two consequences that decide the implementation:
  * the import must register the file **IN PLACE** rather than copying it into `Samples\`, since a
    copy would change the path he explicitly said should not change;
  * the Exports listing must **skip files that already have a library entry**, or the same file
    would appear twice — once under Exports and once under the category it was routed to.

  The move itself then falls out of the existing architecture for free: `onEnumerateAudio` walks
  PAGES and emits an entry per file with a bound page, so a routed render is picked up by its new
  category automatically without a second mechanism.
- [x] §11.5 **Nothing is auto-added to the grid.**  Right-click **reuses the existing
  `BrowserPanel::showItemContextMenu` route-to-a-tab / create-a-new-one flow** already wired for
  clips and vox/inst recordings (`BuilderPage.cpp:459`, `:1200`) — reuse, not a parallel path, so
  the gesture behaves identically wherever it is invoked.

  **DONE as `BrowserPanel::beginAddRenderToProject`.**  Deliberately a panel METHOD rather than an
  editor callback: every piece it needs (`mPM`, `onEnumerateRoutablePages`, `onCreateRoutablePage`,
  `onApplyLibraryProperties`, `maybePromptGroupAssign`) is already on the panel for the Properties
  dialog, so routing it through the editor would have created a second copy of that logic to drift.
  Order is register(owner 0) -> create page -> apply route, matching the Properties "Move to a new
  page" path exactly; `addAudioToLibrary`'s owner-0 upgrade branch is what stops the page factory's
  own re-add from making a second entry.  **Drag lands here too** (`§11.5b`), so both gestures are
  one code path.
- [x] §11.5b **Drag-to-grid raises the SAME prompt (Jeff, 2026-07-29).**  It did not, and would have
  failed silently: browser drags carry `"audio:<libIdx>"` parsed as an INTEGER, and a render has no
  library index (-1), so the drop died on a bounds check with no prompt and no error.  Render leaves
  now emit `"render:<abs path>"`, matched in `isInterestedInDragSource` + `itemDropped` BEFORE the
  index parser (which structurally cannot carry a path), and routed into
  `beginAddRenderToProject` with a completion that places the block at the dropped row/bar.
- [x] §11.6 **A report opens IN THE APP, in the analyzer window** — the same live view, repopulated
  via `MasterAnalyzerView::showRenderedCurve`.  **NOT rendered as HTML in-app:** `JUCE_WEB_BROWSER=0`
  (`CMakeLists.txt:68`) and only `juce_gui_basics` is linked, so `WebBrowserComponent` is
  unavailable, and enabling it would add a Windows **WebView2 runtime dependency** that produces a
  blank report on any machine without it — unacceptable for a beginners' app.  The HTML file remains
  the shareable artifact for outside the app.
- [x] §11.7 **Reload mechanism = EMBEDDED IN THE HTML (Jeff, 2026-07-29, option a).**  Each report
  is ONE self-contained file that is both human-readable in a browser and machine-reloadable by us:
  the curve, summary and violation spans ride inside the HTML (a data block the browser ignores and
  we parse back out).  No sidecar, no third file per report, no new format — the Reports folder holds
  exactly what the user thinks it holds.

- [ ] Build gate (gates the commit below).
- [ ] Batch-smoke scenarios (DEFERRED to Task set 8): (1) freeze a heavy synth tab ->
  CPU drops, sound identical, rack still live (if pre-rack tap); unfreeze restores
  editing; (2) drum-scale same; (3) set -14 LUFS target, maximizer mode A/B, meters
  track target, export lands at target; measure button matches the render's actual
  integrated LUFS; (4) analyzer window runs during playback with acceptable CPU.
- [ ] COMMIT (per-set, Jeff-approved). Running-notes checkpoint.

### Task set 8 — Batch smoke + bookkeeping close-out (Group 8)

- [ ] **BATCH SMOKE with Jeff (the single verification pass for the whole batch, per
  Jeff 2026-07-27):** run every task set's deferred scenario block (TS1-TS7, in order)
  plus the end-to-end Verification list below. Findings fix in-batch (standing rule),
  re-gate, re-smoke the affected scenarios.
- [ ] Master Test Plan §B section authored (blocks = this batch's commits, backfilled).
- [ ] **§B reconciliation pass (conflict call 3=a):** walk §B.1-B.30 and update every step
  that references UI this batch retired or reshaped — B.30's empty-state/zero-badge
  scenarios, B.29's export dialog flow (progress UX + Exports destination), effects-page
  scenarios (stacked rack -> sidebar window), ribbon-slot walks — so every campaign
  scenario is physically walkable against the post-mammoth app. Per-section deltas noted
  inline (the campaign's bisect `blocks:` fields stay pointing at their original commits).
- [ ] `/draft-doc batch-close` -> held Work Log entry in running notes (bulk-run R2).
- [ ] Main Plan edits per the accumulated pending ledger: §5 entry for this batch, §6
  sequencing, §9 Forks entries (CL-087 promotion; the 2026-07-27 origin trail — sweep ->
  export finding -> inversion ruling -> tiers ruling; docket-18 partial reversal), plus
  deep-packing-badger's still-pending ledger items if not yet applied. Jeff approves doc
  diffs before apply (his docs).
- [ ] Future State updates: stale-mark CL-102 (shipped as PagePresetIO); graduated entries
  annotated (CL-087, BLU-480, CL-299, BLU-499, VST3 family, freeze family, maximizer
  items, riders); Section 2 untouched.
- [ ] CLAUDE.md architecture notes refresh (model-owned engines; window shell; export
  path) — surfaced as a diff for approval.
- [ ] Final commit (Rule 9 one-liner) on approval.

## Verification (end-to-end smoke — runs inside the Task set 8 batch smoke, after the per-set deferred scenario blocks)

1. Fresh start, load a real song using every engine family + vox/inst + clips + pedals +
   a hosted VST3 effect and instrument. Plays identically to pre-batch.
2. Export it: every track audible, every automation lane honored (rack, engine, fader,
   EQ, tempo lane), at a non-device sample rate, into `<project>\Exports\`, with the
   progress-bar dialog; session state identical after; cancel mid-way leaves no residue.
3. Stems + LUFS-normalized + dithered variants export as configured.
4. Close EVERY window; play; automate; everything applies. Reopen; views rebuild bound
   to live state.
5. Tab bar: delete all instances of each type -> tab vanishes; "+" restores; required
   four tabs permanent.
6. Effects window: sidebar/detail workflow across all types; VST3 slot; channel switches
   never kill lanes.
7. Freeze/unfreeze round-trips audio-identically; CPU drop visible.
8. Maximizer: target -> meters -> measure button -> export all agree on the number.
9. Old projects + templates v2 + page presets load unchanged (PagePresetIO round-trips
   against model-owned engines).
10. Plugin crash (post-BLU-302): app survives.
11. CPU: several windows open costs more than none (expected, FL-style); all-closed is
    cheaper than the old always-alive tabs (destroy-on-close dividend).

## Routing notes (Rule 3)

Mid-batch findings route into the owning task set if in-family; genuinely foreign findings
go to the pending ledger with a docket to Jeff (no deferral-as-option for self-caused
defects — standing rule). Undo/dirty behavior of new surfaces routes to QA-UndoCoverage /
QA-DirtyFlag ONLY with Jeff's explicit call (those batches' plans get a conflict review
against this batch before they run — Jeff's post-approval step).

## Carry-Forward Reference touch points

§ project-persistence + shield discipline before TS1/TS2 (the load-shield pattern every new
load/apply path must keep); the QA-Ef close entries before touching doFileNew/export
surfaces; STANDALONE_UI_CHANGES.md before TS4/TS5 (deliberate UI decisions log);
`Files For Claude/DSP Review/_APPROVED_CHANGES.md` before TS7's Limiter work.

## Prior Carry-Over (2026-07-29 — TS6 COMMITTED `4ddf25fa` + `467fd0b9`; TS7 opened)

- **Completed:** TS1 `4ea67bd0`, TS2 `e9ecf03e`, TS3 `1dd08437`, TS4 `05b248a8`, and **TS5 across
  three commits** — `28f4ec09` (the Effects surface rebuilt as windows), `c8854429` (the Pedals
  group-heading hover-highlight follow-up), `71781115` (the TS6 spec + the VST3-only format-scope
  decision + Future State CL-303).  TS5 shipped against a REPLACED scope: Jeff respecced the
  Effects surface on 2026-07-29 before any of it was built, so the sidebar-plus-detail-pane shape
  never shipped.  What shipped is the rack window + per-effect windows + separate Pre/Post EQ
  windows, plus a whole-rack preset, plus CL-299 items 1/2/4.  Item-by-item trail in the running
  notes (five entries, 2026-07-29).
- **In-flight: TS6, steps 0-5 of 7 are IN SOURCE AND GREEN, uncommitted.**  Four build gates
  passed, both configs each time, zero errors, two exe link lines.  What is in the tree:
  `JUCE_PLUGINHOST_VST3=1`; new `Source/Hosting/` (PluginManager scanner + added list persisted to
  `plugins.xml`, HostedPluginInstance proxy + its editor with the dead marker, HostedPluginEffect
  rack adapter); new `Source/Standalone/PluginsManagerWindow.*` on Options > Plugins;
  `EffectType::VST3Plugin = 121`; the VST Plugins picker group replacing TS5's disabled row;
  plugin parameter lanes registered live AND resolved offline in the same pass; the
  build-then-replace editor bug closed at both rebuild sites; `instPresetsRootDir` cleaned.
  **Full item-by-item trail is the running-notes entry "TS6 — steps 0-5 LANDED".**
- **Step 6 (BLU-447) is PART DONE — its foundation layer is in and green (gate 5).**  The Plugins
  bus + channel-id space + `InsertKind::Plugin` + meters + PDC + SC + routing are complete across
  `VibeGraph` and `VibeSynthProcessor`; the tree is behaviourally unchanged because nothing creates
  a plugin insert yet.  Reference type was chosen per the audit memory's own rule: engine-driven,
  so it mirrors Layer/Bass/Drum, NOT Vox/Inst.  `isMainOutLocked` deliberately untouched so a VST
  strip reroutes under Layers/Bass like the spec requires.
- **Step 6 audio dispatch + tab kind are DONE and green (gates 6, 7, 8).**  A hosted VST3
  instrument now plays from the piano roll exactly like a Layer: `Pattern::pluginRoll` (serialized
  `PluginPageRoll`, skip-if-empty so pre-TS6 projects are unchanged) -> `pluginNotes` snapshot ->
  both scheduling paths -> `BlockContext::pluginPageMidi` -> `EngineInsertTask::Kind::Plugin` ->
  `registerPluginEngine`'s InsertNode + dispatcher task.  `TabKind::Plugins` is ONE factory case
  keyed on the plugin's identifier string (which `engineType` already persists, so no new save
  format).  `EngineKind::Plugin` appended at value 10 — position is load-bearing, it IS the
  live-MIDI encoding.
- **Step 6 is now MOSTLY done — gates 10-13 green.**  `PluginsPage` (a view; never constructs an
  engine), `registerPluginPianoRoll`, `TabType::Plugins` + its ribbon slot, the "+" side dropdown
  of added instruments (rides the existing `onAddEngineRequest` path — no new callback), and BOTH
  remaining audit sites: 5 (`EffectsPage` dropdown vocabulary at 1000+, bus range 1-13, all four
  resolvers, plus `BuilderPage`'s matching offline sweep) and 4 (`MixerPage` bus strip, strips map,
  colours, both hit-tests, layout, registration, peak drain).  `isRouteAllowed` gives a plugin
  strip `Master | Plugins Bus | Layers Bus | Bass Bus`, which with `isMainOutLocked` left alone is
  exactly the "moves under Layers or Bass like those two already do" spec.
- **STEP 6 (BLU-447) IS COMPLETE — gate 17 green.**  End to end: the "+" dropdown creates a
  `PluginsPage` -> picking a plugin builds it through `EngineRig` -> `registerPluginEngine` spawns
  the InsertNode + dispatcher task -> `onEngineSelected` adds the mixer strip and flips
  `mPluginsBusActive` -> the piano roll targets it and its notes schedule -> `onTabClosed` tears
  the rig tab down after the strip and roll registration are dropped -> the tab round-trips through
  project save/restore carrying the plugin identifier as `engine` and the
  `HostedPluginInstance` blob as `engineData`.
- **STEP 7 (BLU-302) IS BUILT — gates 18-21 green.**  Arch-neutral wire protocol with asserted
  layout (proven by the x86 build compiling the same header), `SandboxedPluginClient` with a hard
  4 ms audio deadline and fail-fast crash handling, the sandbox wired behind the step-3 seam
  (32-bit forced, 64-bit by preference with in-process fallback), the `BaySickPluginHost` helper
  linking only three JUCE modules, and 7b's standalone `-A Win32` project building
  `BaySickPluginHost32.exe` into its own `build32/`.
- **GATE CRITERION FOR THIS BATCH IS NOW:** five exit codes in `build_log.txt` (`RELEASE`,
  `DEBUG`, `HELPER64`, `HELPER32_CONFIG`, `HELPER32`) all 0, and FOUR `vcxproj -> ....exe` link
  lines (two `BaySickDAW.exe` + both helpers).  Supersedes the "two link lines" rule.
- **Jeff ran it and two items came back, both fixed (gate re-run green):** (1) closing a plugin
  window CRASHED — `~EffectSlotWindow` read a freed `PageMenuBar` because `WorkspaceWindow`
  declares `mContent` before `mPageMenu` and members destruct in REVERSE order, so the menu bar
  dies first; both that window and `EffectEqWindow` now hold it as a `SafePointer`.  (2) plugin
  windows left dead space around the plugin's surface — new
  `WorkspaceWindow::sizeToContent` driven by `HostedPluginEditor::onNaturalSizeChanged`, fitting
  the window on mount and on any plugin-initiated resize.  The STRETCH half is held for the layout
  batch with its options written out.
- **TS6 IS COMMITTED, in TWO Jeff-approved commits.**  `4ddf25fa` — VST3 hosting end to end (the
  `JUCE_PLUGINHOST_VST3` flag, `Source/Hosting/` scanner + added list + PE-header arch split, the
  `HostedPluginInstance` proxy seam, `EffectType::VST3Plugin = 121` + the VST Plugins picker group,
  Options > Plugins manager window, BLU-447's Plugins tab + bus + strip + roll, BLU-302's sandbox
  with BOTH helper architectures, plus the `PageMenuBar` SafePointer close-crash fix and
  `WorkspaceWindow::sizeToContent`).  `467fd0b9` — the follow-up: three specced items I had wrongly
  reported as done (BLU-299 search/filter as ONE box over both lists per Jeff's refinement, the
  BLU-302 per-plugin bridge toggle on `EffectSlotWindow`'s hamburger with 32-bit shown DISABLED
  rather than hidden, and an editor for BRIDGED plugins at all via the `mRemoteHost` child peer),
  the editor-outlives-instance crash Jeff reproduced (`HostedPluginEditor` demoted from
  `AudioProcessorEditor` to a plain `Component`), and the Rule 4 strip of all eight `[TS4 SHELL]`
  diagnostics.  Batch so far: TS1 `4ea67bd0`, TS2 `e9ecf03e`, TS3 `1dd08437`, TS4 `05b248a8`,
  TS5 `28f4ec09` + `c8854429` + `71781115`, TS6 `4ddf25fa` + `467fd0b9`.  Tree clean after each.
- **NOTHING in TS6 has been RUN against a real plugin.**  Jeff confirmed the windows open and the
  close-crash is fixed, but he has no plugins installed on that machine, so scan / load / automate /
  window auto-sizing are all unexercised, and the BRIDGED path has never executed a single
  instruction — helper process, wire protocol, shared-audio deadline, crash recovery and the
  child-peer editor reparenting included.  TS8's smoke owns this under the batch's
  deferred-verification ruling; scenario 5 covers the CRASH half only, so the bridged EDITOR half
  needs its own scenario.
- **Assumptions changed (the plan body above is now wrong in three places, all annotated inline):**
  1. BLU-480's sidebar/detail-pane shape is superseded.  Do not build it in a later set.
  2. BLU-499's three approach options were never picked — the restructure answered the question
     differently (per-effect presets on the panel window's menu; a new whole-rack preset on the
     rack window's menu).
  3. CL-299 is a 3-item deliverable now, not 4.  Item 3 is an explicit owner-ruled DROP.
- **Open for TS8 bookkeeping:** `Test Plans/v1-master-test-plan.md` §B.31.0 has ONE "Effects" row,
  written when the surface was one window.  It now needs rows for the rack window, a panel window
  and an EQ window — three different floors.  The rack window already has a real floor (300x250,
  derived from its fixed content); the other two are provisional (620x170, 560x320).
- **SCOUT RESULT — the CMake question is CLOSED, do not re-open it.**  There was never a module to
  swap: `juce_audio_utils` -> `juce_audio_processors` -> `juce_audio_processors_headless`, so the
  headless module is the DEPENDENCY of the full one, not an alternative to it, and we have always
  built the full module (APVTS lives in it).  Editor hosting therefore came with it —
  `VST3PluginInstance` overrides `hasEditor`/`createEditor` to return a `VST3PluginWindow` wrapping
  the plugin's `IPlugView`, DPI + `resizeView` handled.  The entire CMake deliverable was ONE line,
  `JUCE_PLUGINHOST_VST3=1`, and `JUCE_PLUGINHOST_VST` stays 0 per CL-303.  The VST3 SDK is already
  vendored (MIT).  `KnownPluginList`'s blacklist API, `getDefaultLocationsToSearch()` and
  `ChildProcessCoordinator`/`Worker` for BLU-302's IPC are all already in the tree.
- **BUILD ORDER REVISED (proposed to Jeff 2026-07-29, steps 0-2 identical either way so nothing
  waited on it):** 0 CMake flag -> 1 scanner -> 2 manager window -> **3 proxy seam (PROMOTED)** ->
  4 effect slot -> 5 latency -> 6 instrument -> 7a sandbox -> 7b 32-bit.  The promotion is the
  whole change: BLU-302 stopped being optional when 32-bit VST3 entered scope, and building every
  consumer against a concrete `juce::AudioPluginInstance` first would have forced a rewrite of all
  six surfaces when the sandbox arrived.  With the seam first, BLU-302 ADDS an implementation
  behind an existing interface.
- **32-BIT COST, taken to Jeff as the plan required:** the helper links only
  `juce_audio_processors` + `juce_events` + `juce_gui_basics` — none of sfizz / NAM / RubberBand /
  LAME / WORLD / lunasvg — so it is a small self-contained CMake project over vendored JUCE.  The
  32-bit-specific delta over 64-bit-only isolation is (a) a second CMake configure into its own
  build directory, because an MSVC generator is single-platform per tree (ours is x64), plus a
  `do_build.bat` step, and (b) an ARCHITECTURE-NEUTRAL wire protocol — fixed-width types, explicit
  packing, no pointer or `size_t` on the wire.  (b) is free if designed in and expensive if
  retrofitted, so the "revisit if disproportionate" trigger does NOT fire and his ruling stands.
  **One gate consequence to restate when 7b lands:** the "two exe link lines" build-gate criterion
  changes once the helper builds, and in its own tree it will not appear in `build_log.txt` at all
  unless `do_build.bat` is extended.
- **Resume action:** **TS7 (freeze + loudness suite)** is open.  Its FIRST act is its own sub-spec
  call — freeze tap point (pre-rack "Source Only" / post-rack "Full" / both) + freeze presentation
  (invisible swap / bounce-in-place row / both) — posed as a hard stop.  The MAXIMIZER + ANALYZER
  half does NOT depend on either answer, so it builds while the call is pending.  Facts TS7 must
  respect, each already paid for: TS2's `BuilderPage::runOfflineLoop` is the ONE render core (freeze
  is its THIRD consumer — do not copy the loop); `measureRender` already returns integrated LUFS + a
  4x-oversampled Lagrange TRUE-PEAK ESTIMATE and BLU-108 is precisely the upgrade to a BS.1770
  polyphase FIR, whose call sites include CL-045's shipped boost cap; CL-045 already SHIPPED at TS2
  and is NOT CL-244; master LUFS metering already exists (QA-RustyMeter `getMasterLufs(mode)`);
  `_APPROVED_CHANGES.md` + `Limiter.txt` GOVERN the Limiter DSP + panel; CL-243's mode names are
  OURS; any new lane class gets its `applyOfflineLaneValue` branch in the SAME pass as live
  registration; engine swap for freeze goes through `EngineRig`; new windows are `WorkspaceWindow`s
  with their OWN `TooltipWindow` and mapping-set-first key routing; the drawn-overlay z-order trap
  and the empty D2D `performAnyPendingRepaintsNow` both bite any progress surface a blocking freeze
  render needs; peer-keyed suspend is the convention for CL-044's analyzer.

## Prior Carry-Over (2026-07-28 — TS3 COMMITTED `1dd08437`; TS4 IN FLIGHT, chunk A landed)

- **Completed:** TS1 `4ea67bd0`, TS2 `e9ecf03e`, and TS3 code-complete — all eight source
  items closed (EffectParamMap tables for every EffectType x variant; pedals model-side;
  the 19 wrapper sites retired + their five helpers deleted; mixer `_fader` remap + shim
  retirement; EQ band lanes off the display; owner-index removal + `onIsParamStale`
  re-widen; BLU-344), PLUS the two items Jeff ruled on at the commit surface: the sfizz
  automation gap FIXED as a defect, and `TapePanel` DELETED.  Item-by-item trail in the
  running notes.
- **In-flight:** nothing.  TS3 committed `1dd08437` on Jeff's approval (40 files,
  +2040/-1603; tree clean after).  The post-commit running-notes entry + this Carry-Over
  refresh ride TS4's commit, same convention as every prior set.
- **Assumptions changed (recorded here because the plan body still states them):**
  1. The TS3 hard-won-facts note says "null-owner registrations must clear
     `mAutomationIdOwner`".  That guard is GONE with the owner index itself — after TS3
     nothing registers with an owner, so a view-lifetime index over a permanently empty
     set was worse than nothing.  The fact was true for the widget-targeting era it was
     written in; do not re-add it.
  2. `variantOf` is no longer DSP-read ALONE.  Which panel a DSP gets also depends on
     WHERE the slot lives (FX rack vs pedals board), and that is not readable from the
     DSP — a Tape-mode SaturationDSP on the board shows "Drive" as 0..10 into setFlowers
     where the rack shows it as dB into setTapeInputGain.  `PanelContext` supplies that
     dimension from the registration site.  Same class of bug as Modern-vs-FET `attack`.
  3. Retiring the wrappers was a pure deletion, not a migration: every wrapper key was
     already the engine's APVTS param id, which TS1's model walk registers.  Because the
     editor is built after the engine, the VIEW claim was winning — TS3 is what makes
     TS1's registration take effect.
- **TS4 IS OPEN AND PARTLY BUILT — read this before touching it.**
  * DONE: the TS4 scout (full map in the running notes — page hosting today, the
    fixed-10-slot ribbon, the missing PageMenuBar .h/.cpp, the main-window order gotcha)
    and **chunk A**: new `Source/Standalone/WorkspaceWindow.h/.cpp` + CMakeLists entry,
    holding `Workspace` + `WorkspaceWindow`.  Compiles green both configs, zero errors.
    UNCOMMITTED and UNWIRED — nothing hosts pages in it yet, so the app still behaves
    exactly as before.
  * The JUCE child-peer coordinate contract is VERIFIED against the vendored source and
    written into WorkspaceWindow.h as a keeper comment.  Do NOT re-derive it: child peers
    are WS_CHILD, and BOTH setBounds and getBounds work in PARENT-CLIENT space (top-level
    peers use screen space) — that asymmetry is the whole reason
    `Workspace::originInParentClient()` exists.
  * REMAINING TS4, in order: (1) wire `Workspace` into `StandaloneEditor::resized`'s
    content rect, replacing the stacked always-alive pages; (2) flip page hosting onto
    WorkspaceWindows + destroy-on-close, reopen rebuilding the view from the TS1 model;
    (3) the tab-bar "+" rewrite (RibbonTabBar is a FIXED 10-slot bar today — this is a
    real slot-model rewrite) + retire the EngineEmptyState trio, `hideAllEmptyStates` and
    the six `on*EmptyStateRequested` callbacks; (4) per-window keyboard/command routing
    (mind the CLAUDE.md addKeyListener REVERSE-order gotcha); (5) main window pinned
    fullscreen — PRESERVE the StandaloneApp.cpp:956 ordering comment, limits MUST be set
    before setFullScreen or Windows demotes MAXIMIZED to NORMAL; (6) CL-060 lazy views
    DONE, **CL-060 parallel page restoration DROPPED by owner ruling 2026-07-28** (see the
    checklist entry above and the running-notes removal entry — not re-routed anywhere);
    (7) CL-087 promoted (TS8 writes the Forks entry).
  * **FLOORS CALL — RESOLVED AS A PROCESS, numbers still pending (Jeff 2026-07-28).**  He
    cannot drag until he is home, and wants building to continue meanwhile.  Ruling: build
    the shell with provisional floors, and notate every window he must size as the VERY
    FIRST item of the smoke; he does that pass, hands over the numbers, then they get set
    properly.  **`Test Plans/v1-master-test-plan.md` §B.31.0 is written and is that
    checklist** — 12 windows, floor + comfortable per row, "natural" for fixed-grid pages.
    So: DO NOT block TS4 on the numbers, and DO give every window a provisional floor via
    `WorkspaceWindow::setMinimumSize` so nothing is unbounded in the meantime.
  * **DPI: Jeff runs 125%.**  Not a problem for the numbers — JUCE lays out in LOGICAL
    pixels and converts only at the peer boundary, so a collision point is a property of
    the layout, not the display scale; and 125% makes his screen SMALLER in logical pixels,
    so his floors are the conservative case and are automatically safe at 100%.  The real
    125% risk is different and is now §B.31.1: fractional scale means logical<->physical
    does not land on integers, so a 1px save/restore rounding error would ACCUMULATE across
    launches and walk windows off-position.  Test it over THREE relaunches, not one.
- **Resume action:** session-open per the boilerplate (standup + Main Plan §0 + this plan
  IN FULL incl. Carry-Over blocks + the mammoth running notes IN FULL), confirm `1dd08437`
  at HEAD, note the four uncommitted TS4 files, then continue **TS4** at item (1) above.  TS4's FIRST act is its open
  sub-spec call: exact minimum window sizes for Builder / Piano Roll / Mixer, picked with
  Jeff ON SCREEN (he wants "larger" floors).  Scout after that: the native-child window
  frame family, `StandaloneApp.cpp:948-1015` fixed-fullscreen main, RibbonTabBar "+"
  system, and the EngineEmptyState trio to retire.
- **Owner rulings applied 2026-07-28 (both were surfaced at the TS3 commit):**
  1. **sfizz automation = DEFECT, fixed in-batch.**  Jeff: "that's not a new feature that's
     something you never setup and you need to fix."  He was right, and my framing had
     understated it: the Aria panel has always OFFERED "Automate: ..." on every kit CC and
     `sOnAutomate` duly created the lane — with no applicator behind it, so the lane drew and
     played back against nothing.  New `onSfizzEngineReady` model event +
     `registerSfizzEngineAutomation` + `forEachSfizzApvts` for the offline replay.  Lane ids
     are the engines' own globally-unique param ids, so every lane a user already created
     starts working.
  2. **`TapePanel` DELETED** (147 lines + its dead `TapeDSP.h` include + two stale comments),
     after confirming for Jeff that it bound the LEGACY standalone `TapeDSP*` and is NOT the
     Tape option he uses (that is `TapeSatPanel`, `SaturationDSP` + `setTape*`, covered by the
     new `kSatTape` table).
  3. **`TapeDSP` CLASS DELETED** (Jeff, same exchange).  `Source/DSP/TapeDSP.h` + `.cpp`
     git-rm'd, `CMakeLists.txt` entry dropped, two stale includes removed.  Full-tree census
     first: zero code dependencies.  KEPT on purpose: `SaturationDSP.cpp:704`'s
     `getTagName() == "TapeDSP"` string compare (the legacy-preset migration path — deleting it
     would orphan every pre-cutover Tape preset), and the Rule 6 keeper comments explaining that
     SaturationDSP's tape body is a bit-exact port of it.  `EffectRack.cpp`'s comment was the one
     that became FALSE ("stays in the source tree as an emergency-rollback safety net") and was
     corrected.  Joins the earlier accrual (Main Plan :6373's stale "QA-DirtyFlag closes G4
  code" line) and badger's six held items.
- **Implemented-work entry needed:** compiled from running notes at TS8 (bulk-run R2).

## Prior Carry-Over (2026-07-27 — TS2 COMMITTED `e9ecf03e`; superseded by the block above)

- **Completed:** TS1 committed `4ea67bd0`; TS2 committed `e9ecf03e` (Jeff-approved;
  every TS2 checklist item closed — item-by-item trail in the running notes).  Gates
  were green both configs at both commits.
- **In-flight:** none.  Tree dirty ONLY with this plan file + the running notes (the
  TS2-committed entries) — EXPECTED; they ride TS3's commit, same convention as the
  session-open backfills riding TS1's.
- **Resume action:** session-open per the boilerplate (standup + Main Plan §0 +
  this plan IN FULL incl. these Carry-Over blocks + the mammoth running notes IN FULL
  — they are this batch's primary context now), confirm `e9ecf03e` at HEAD, then open
  **TS3 (automation fully model-side)** with its scout: re-read the plan's TS3 section
  + the running-notes pins (lane-resolver rules; EffectParamMap hard-won facts), then
  enumerate the 19 wrapper sites (plan lists them) + the EffectType x variant table
  matrix from `createEffectEditor`'s dispatch before writing tables.  TS3 has NO open
  sub-spec calls; next open calls sit at TS4 (floors), TS6 (process model), TS7
  (freeze).
- **Standing process corrections (this session, verbatim intent):** intermediate
  checkpoints are NOT stopping points — run continuously; the ONLY stops are task-set
  commit approvals and genuine spec calls.  Naming is the assistant's call, never a
  docket item.
- **Implemented-work entry needed:** compiled from running notes at TS8 (bulk-run R2).

## Prior Carry-Over (2026-07-27 — TS1 COMMITTED `4ea67bd0`; TS2 open, stems call pending)

- **Completed:** TS1 in full — committed `4ea67bd0` on Jeff's approval (42 files; gate
  green both configs; all ten punch-list items). Running notes carry the complete trail
  (scout map, sfizz precedent, UM wrinkle, K-5 chain-splice fix, CL-301 parity audit +
  fourth divergence, wire-at-load).
- **In-flight:** TS2 building; skeleton steps 1-4 LANDED IN SOURCE and verified green
  through steps 2+3 (offline drive on the live processor with replica deleted +
  prepare-sweep gap fixes; lane-aware integrating clock + span math; UI-free offline
  lane replay; stems one-pass multi-sink taps + metronome offline gate +
  getProjectExportsDir destination on both choosers). The stems/metro/destination
  chunk compile was RUNNING at checkpoint — verify build_log.txt first on resume.
- **Resume action:** (0) the stems/metro/destination chunk went GREEN both configs —
  re-verify build_log.txt only if source moved after this note; then the REMAINING
  TS2 items, in order: (a) export dialog UX rework — a new persistent modal
  ExportAudioDialog component replacing doExportAudio's AlertWindow
  ([StandaloneEditor.cpp:10486](../../Source/Standalone/StandaloneEditor.cpp:10486);
  carry the combo vocabulary + quality mapping over VERBATIM): options persist, the
  async FileChooser opens ABOVE it, on pick the box flips to progress mode (bar +
  percent readout, controls disabled, Cancel live -> shouldAbort) driving renderToFile
  on a plain background juce::Thread; the STEMS PICK-LIST lives inside it via a NEW
  `MixerPage::getStemPickEntries()` returning {channelId, shownName, defaultChecked}
  for every strip the mixer currently SHOWS, in display order (MixerPage is the single
  truth for active strips + user-visible names; Master + buses defaultChecked=false)
  feeding RenderOptions::stems; the export save-first interlock mirrors badger Task
  12's doFileSaveAs onSaved continuation.  The pattern right-click path keeps
  runExportWithProgress (B.29 reconciliation happens at TS8).
  (b) AudioClipStreamer offline synchronous-read mode under isNonRealtime + CL-282
  underrun telemetry (atomic counter + Debug overlay); (c) riders — CL-043 dither on
  the WAV sink, CL-045 LUFS measure-then-gain (both directions, true-peak-capped),
  CL-056 offline block size; (d) CL-057 buffer-size hot-swap reusing
  begin/endOfflineRender's re-prepare; (e) CL-227 backend (the loop with meters, no
  writer); (f) TS2 GATE + Rule 9 one-liner + FULL git status -> Jeff approves ->
  COMMIT; running-notes checkpoint.
- **Assumptions changed:** none new since the TS1 entries.
- **Implemented-work entry needed:** compiled from running notes at TS8 (bulk-run R2).

## Prior Carry-Over (2026-07-27 — TS1 ~70% done, superseded by the block above)

- **Completed (all builds green, both configs, per chunk):** session open + backfills
  (§B.30 + badger held entry -> `b933b54a`); TS1 scout (full map in running notes);
  batch-ID call resolved (a — QA-ModelShell stays; naming corrected to my job);
  **EngineRig landed** (`Source/EngineRig.h/.cpp` — model tab registry + factory + apvtsOf
  resolver + onEngineCreated/onEngineDestroying events); **dormant UndoManager pre-wire**
  (processor mUndoManager before apvts; 7 engine ctors take `UndoManager* = nullptr`;
  sfizz private UMs untouched); **ALL SIX page types flipped to views** (L/B/D + Clips +
  Vox + Inst — non-owning pointers, rig-delegated construct/swap/teardown, Vox/Inst ctors
  gained `VibeSynthProcessor&`); **onTabClosed** tail does rig.removeTab BEFORE the sfizz
  destroys (dangling-chain-splice hazard caught + fixed pre-runtime); **tab identity
  model-side** (addTab at page birth; setTabName -> renameTab funnel; clearSound ->
  clearEngine); **automation hooks** (onEngineCreated -> registerModelEngineAutomation;
  registerSlotAutomationFor extraction + registerRackAutomationForAllChannels after
  applyPendingRackStates).
- **In-flight:** TS1 SOURCE IS CODE-COMPLETE — all nine source items done including
  CL-301 (fold executed; see the running-notes CL-301 entry for the parity audit + the
  fourth divergence closed) and the task-7 shape confirmation. THE TASK-SET GATE IS
  GREEN (final build after CL-301: both exit codes 0, zero error lines). The TS1 commit
  is SURFACED to Jeff (Rule 9 one-liner + full status) — awaiting his approval; nothing
  commits without it.
- **Assumptions changed:** none beyond the running-notes entries (sfizz precedent, UM
  wrinkle, K-5 chain-splice gap, Layers/Bass loadPagePreset lock-guard observation for
  TS8, stale Forks line accrued for G4 close).
- **Resume action:** (1) verify the task-5 build result; (2) execute CL-301 (task 6):
  fold LayersBusNode/BassBusNode/DrumsBusNode/MasterBusNode/EffectsBusNode
  ([VibeGraph.cpp](../../Source/VibeGraph.cpp) :239/:449/:622/:793/:947) into
  InstrChannelNode per the Future State entry (special cases survive via flags:
  Layers/Bass synth refs, Master LUFS + terminal no-comp-delay role, FX receive-bus
  drive point; calling side already uniform via PassiveStripTask); (3) confirm task-7
  shape note; (4) TS1 GATE + Rule 9 one-liner + FULL git status -> Jeff approves ->
  COMMIT (doc backfills + running notes + this plan ride it); (5) running-notes
  checkpoint; then TS2.
- **Implemented-work entry needed:** compiled from the running notes at TS8's
  batch-close draft (bulk-run R2) — nothing to add beyond what's captured.
