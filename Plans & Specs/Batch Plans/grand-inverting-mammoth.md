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

- [ ] BLU-480 effects window: left sidebar = channel selector + 6 slot rows (picker per
  slot) + Pre/Post EQ entries; right detail pane hosts the selected panel (min = largest
  panel's natural size). Reuses `resolveChannelDsp`/`rackForChannelId` single-switch
  ([EffectsPage.cpp:496-602](../../Source/Standalone/EffectsPage.cpp:496)) — do NOT fork a
  second channel switch.
- [ ] BLU-499 preset-loader placement in the new shell (3 approach options in its entry —
  pick with Jeff at implementation).
- [ ] CL-299 Delay panel deltas (4 items enumerated in its Future State entry).
- [ ] Player-page FX buttons open this window pre-selected to that channel.
- [ ] VST3 slot type has a real place in the sidebar picker (TS6 fills it).
- [ ] Build gate (gates the commit below).
- [ ] Batch-smoke scenarios (DEFERRED to Task set 8): (1) every effect type's panel
  renders + edits in the detail pane; Basic/Advanced + presets + slot reorder intact;
  (2) pre/post EQ edit + automate; (3) channel switching keeps automation applying (the
  original Task 7 scenario, now in the new shell); (4) Delay panel matches the CL-299
  reference deltas.
- [ ] COMMIT (per-set, Jeff-approved). Running-notes checkpoint.

### Task set 6 — VST3 hosting (Group 6)

Build order within the set: scanner -> browser -> effect slot -> latency -> instrument ->
crash protection. CMake scout first: hosting flags on our _headless audio_processors
module variant.

- [ ] BLU-298 scanner: background thread, known-plugins list persisted, blacklist on
  crash-during-scan.
- [ ] BLU-299 browser: search/filter over scanned list; lives in the "+"/picker surfaces.
- [ ] BLU-300 effect hosting: `EffectType::VST3Plugin` slot (append-only ordinal per the
  pinned-enum rule, [EffectRack.h:15-23](../../Source/EffectRack.h:15)); state save/load
  (plugin state blob in VibeRackStates); editor hosted in a native child window (the
  TS4 shell makes this composable); automation: plugin params surface as lanes (param
  index/id keyed — stable-id discipline per the binding research; REAPER's positional
  fragility is the anti-pattern).
- [ ] BLU-301 latency: plugin getLatencySamples -> bus PDC refresh (existing
  updateBusLatencies path).
- [ ] BLU-447 instrument hosting: a hosted synth as a tab engine in TS1's generic slot
  (MIDI in from the roll dispatch; state in engineData; editor window like any page).
- [ ] BLU-302 crash protection LAST (sub-call: process model). Everything before it works
  in-process first.
- [ ] Build gate (gates the commit below).
- [ ] Batch-smoke scenarios (DEFERRED to Task set 8): (1) scan finds Jeff's installed
  plugins; (2) a known VST3 effect loads in a rack slot, sounds, saves/reloads with
  state, automates, and its window composes correctly with ours; (3) a VST3 instrument
  plays from the piano roll + exports correctly (TS2 path); (4) latency-heavy plugin
  stays aligned; (5) after BLU-302: kill the sandboxed plugin -> app survives with a
  dead-slot marker.
- [ ] COMMIT (per-set, Jeff-approved). Running-notes checkpoint.

### Task set 7 — Freeze + loudness suite (Group 7)

- [ ] Resolve sub-spec calls (tap point, presentation) with Jeff FIRST.
- [ ] Freeze machinery on TS2's renderer: render one tab/drum offline through the chosen
  tap; cached WAV plays in the engine's place (model swap — engine suspended, state
  retained for unfreeze); edit-of-frozen-content prompts or auto-re-renders per the
  presentation ruling. BLU-427 (per-drum) + CL-055 (per-tab) are the same machinery at
  two scopes; smart auto-trigger (CPU >80%) is the last, optional layer.
- [ ] Maximizer suite on LimiterDSP (the approved `_APPROVED_CHANGES.md` Limiter spec +
  `Limiter.txt` UI govern the panel): CL-244 LUFS-target mode; CL-243 character modes
  (BLU-109 voicings fold in; unbranded names); BLU-108 true-peak auto-ceiling; BLU-110
  LUFS/dBFS side-by-side + target line (master LUFS metering exists — QA-RustyMeter).
- [ ] Measure-before-render button: TS2's CL-227 backend, integrated LUFS of the
  arrangement in seconds, shown against target.
- [ ] CL-227 report face: timecoded violations log (target spec selectable) written
  alongside — or shown; format sub-call at implementation.
- [ ] CL-044: floating master analyzer window (SpectrumFeed reuse; TS4 shell).
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

## Carry-Over (2026-07-28 — TS3 COMMITTED `1dd08437`; TS4 IN FLIGHT, chunk A landed)

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
