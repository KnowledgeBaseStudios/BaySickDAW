# Lite Shell Map - code facts for the KBS DAW fork (2026-09-02)

Produced by the `lite-shell-map` workflow: six parallel read-only subsystem readers (mixer/routing, tabs+engines, VST3 hosting, build/packaging, sequencer+persistence, live input), one synthesis, a completeness critic, five second-round readers, one merge.  14 agents, 755 file reads.  Facts with file:line; "inferred" where a reader said so.  No recommendations.  Part 1 is the first-round synthesis (the fuller document); Part 2 is the merge agent's second-round output, kept separately because it condensed rather than extended.  Reader reports are in `lite-shell-map-2026-09-02-reports.md`.

---

# PART 1 - first-round synthesis

# BaySickDAW Lite fork - gap map

Synthesis of six read-only subsystem reads (mixer-routing, tabs-engines, hosting, build-packaging, sequencer-persistence, live-input). Facts and gaps only. Two report-vs-report contradictions were resolved with a grep (see section 10); nothing else was re-explored.

---

## 1. What the shell IS today

Line counts are `.cpp+.h` from the build report where given.

| Component | Files |
|---|---|
| App / device owner / splash / main `DocumentWindow` | `Source/Standalone/StandaloneApp.cpp/.h` (1,429+221) - owns the one `AudioDeviceManager` (`:762`), audio/MIDI settings persistence (`:428-464`), MIDI callback (`:1367-1390`) |
| Contained-window shell (native WS_CHILD peers inside the fixed frame) | `Source/Standalone/WorkspaceWindow.cpp/.h` (1,522+553), `WindowChrome`, `BaySickTitleBar`, `HeavyOperationOverlay` |
| Editor / orchestrator (tab records, page windows, automation applicator maps, `<UIState>`, recording commit) | `Source/Standalone/StandaloneEditor.cpp/.h` (21,314+1,518) |
| Ribbon / tab bar + "+" menu + per-type dropdowns | `Source/Standalone/RibbonTabBar.cpp/.h` (1,266+298) |
| Per-window hamburger menu, swing knob, LAF, VKnob, right-click Automate / MIDI Learn menus | `PageMenuBar` + `GlobalAutoRightClick` in `Source/Standalone/SharedUI.cpp/.h` (4,809+1,667) |
| Builder: arrangement grid, audio-clip rows, automation sub-page, the ONE offline render loop | `Source/Standalone/BuilderPage.cpp/.h` (10,984+1,756); `runOfflineLoop :9278`, `renderToFile :9597`, `measureRender :10157`, `renderFreezeFile :9858` |
| Pattern / arrangement / marker / tempo / audio-library model, `evalAutomationLaneAt` | `Source/PatternManager.cpp/.h` (2,026+1,012) |
| Piano Roll + Event Editor + Riff Machine + ControlLane | `Source/Standalone/PianoRoll.cpp/.h` (6,379+815), `PianoRollPage.cpp/.h` (316+206), `EventEditor.cpp/.h` (2,123+364) - closure-based, zero engine references |
| Mixer page + strip widget | `Source/Standalone/MixerPage.cpp/.h` (4,475+623), `MixerTrackStrip.cpp/.h` (821+440) |
| Routing graph, insert/bus nodes, PDC, taps, `MixerChannelIds` | `Source/BaySickGraph.cpp/.h` (3,288+1,245) |
| Multithreaded render engine (thread pool, dispatcher, arena, tasks) | `Source/Engine/` (33 files, 4,274) - `RenderGraphDispatcher`, `PassiveStripTask`, `MasterTask`, `EngineInsertTask`, `CompositeAudioInsertTask`, `DirectFileTask`, `BlockContext.h`, `UpstreamLink.h`, `FrozenSourceRead.h` |
| Processor: engine registration, scheduler, live MIDI, recording, offline render bracket, project (de)serialize | `Source/PluginProcessor.cpp/.h` (9,992+2,447) |
| Model-owned engine registry | `Source/EngineRig.cpp/.h` (1,107+404) |
| Transport bar + metronome | `Source/Standalone/GlobalTransportBar.cpp/.h` (1,135+262), `MetroPanel.h` (127) |
| Audio device + ASIO settings dialog | `AudioSettingsDialog` in `StandaloneEditor.cpp:137-564`; ASIO name/mask handling `StandaloneApp.cpp:839-953` |
| MIDI input + MIDI Learn | `StandaloneApp.cpp:1165-1199, 1367-1390`; `Source/MidiLearn/MidiLearnRegistry.cpp/.h`, `MidiLearnUI.h` (folder 1,496 incl. `DrumTriggerMap`, which goes) |
| VST3 hosting, both bridge ends, scanner, allowlist | `Source/Hosting/` (13 files, 5,057): `PluginManager`, `HostedPlugin`, `HostedPluginEffect`, `SandboxedPluginClient`, `OutOfProcessScanner.h`, `PluginBridgeProtocol.h`, `BridgeSharedMemory.h`, `Helper/PluginHostMain.cpp`, `Helper/CMakeLists.txt` |
| Plugins tab page + plugin manager window | `Source/Standalone/PluginsPage.cpp/.h` (778+188), `PluginsManagerWindow.cpp/.h` (513+148) |
| Effect rack (6 slots per strip/bus/master) - survives as a VST3-only host | `Source/EffectRack.cpp/.h` (991+410) plus the VST3 arms of `EffectsPage.cpp/.h` (1,795+268), `SlotComponent.cpp/.h` (1,371+219), `EffectWindows.cpp/.h` (2,418+331), `EffectEditorPanels.cpp` (`HostedPluginEditor` case `:7386-7394`), `EffectPresetIO`, `FxRackPresetIO` |
| Project files, bundling, missing-file handling, safe XML | `Source/ProjectManager.cpp/.h` (772+322), `Standalone/ProjectBundler` (632+135), `ProjectFileResolver.h`, `MissingFileReport.h`, `SafeXml.h`, `ProjectBrowserWindow`, `VersionCapture`, `AppPaths.h` |
| Undo | `Source/Standalone/UndoActions.h` (586), `UndoHistoryWindow`, one app-wide `UndoManager` on `StandaloneEditor` |
| Settings / keymap | `ProjectManager` (`settings.xml`), `KeyBindings.cpp/.h` (897+161), `KeyBindsWindow`, `ui_prefs.xml` (`StandaloneEditor.cpp:20334-20342`) |
| Audio clips (decode cache, streamer, vocoder, recorders, formats) | `Source/DSP/AudioClipStreamer.*`, `Source/DSP/PhaseVocoder.*`, `PluginProcessor.h:889-1050`, `AudioFileRecorder`, `MidiRecorder`, `SafeAudioReader.h`, `SafeAudioFormats.h`, `MpglibAudioFormat.h`, `DSP/Mp3Writer.cpp`, `SampleLibrary.cpp/.h` (minus Core Library), `TempoMapRead.h`, `TsMapRead.h` |
| Offline export + freeze | `PluginProcessor.cpp:7939-8039` (`beginOfflineRender`), `BuilderPage.cpp:9218-10040`, `Engine/FrozenSourceRead.h`, freeze state on `EngineTab` (`EngineRig.h:83-212`), `LoudnessReportWriter`, `MasterAnalyzerWindow` |
| Manuals window + figure pipeline | `Source/Standalone/ManualsWindow.cpp/.h` (112+45, WebView2), `ShotHarness.cpp/.h` (2,125+21), `ShotMenuHook.h`, `ShotFactories.h`, `Manuals/assets/generate-manual.py` (1,259), `control-blurbs.py`, `marker-coords.py` |
| Build + installer | `CMakeLists.txt` (923), `do_build.bat`, `make_installer.bat`, `Installer/BaySickDAW-Tester.nsi` (429), `Source/Hosting/Helper/CMakeLists.txt` |
| Shared UI assets | `Resources/Filmstrips/` (9 PNGs, `SharedUI.cpp:10`), `libs/fontaudio`, `Assets/BaySickDAWLogo.png`, `control_tab.png`, `PatternColorPicker`, `PagePresetIO.cpp/.h` (1,095 - serves every page type) |

---

## 2. What gets deleted

Source total is 411 files / 234,169 lines; the build report's engine+DSP+effect-UI bucket is **114,322 lines (48.8%)**. Adjustments to that bucket are flagged in section 10.

| Bucket | Lines | Detail |
|---|---|---|
| Engine folders (11) | 43,578 | `BaySickSolstice` 9,539 · `BaySickVocal` 9,459 · `BaySickSynth` 5,513 · `BaySickPlayer` 4,656 · `BaySickNAMIR` 3,922 · `BaySickBass` 2,387 · `SlideSampler` 2,162 · `BaySickPedals` 2,028 · `BaySickRustyDrums` 1,907 · `BaySickGuitars` 1,012 · `BaySickBasses` 993 |
| `Source/DSP/` (118 files) | 36,347 counted; a subset survives | Effects/pedals/EQ DSP, `EffectParamMap` (937), `Kbs/ParametricEq.h` (2,256), pitch engines, NAM pedal DSP, `EngineSidechainHelper.h`. **Survivors inside this bucket:** `AudioClipStreamer.*`, `PhaseVocoder.*`, `Mp3Writer.cpp`, `DSPBase.h` (the rack slot base type), `LibraryPitchShifters` only if the clip stretch path needs it (not determined by any report) |
| Effect UI in `Standalone/` | 15,440 counted; VST3 arms survive | `EffectEditorPanels` 7,424+229 (all `EditorPanelBase` panels go; `HostedPluginEditor` case stays), `EffectWindows` 2,418+331, `EffectsPage` 1,795+268, `SlotComponent` 1,371+219, `EffectPresetIO` 767+113, `FxRackPresetIO` 169+54, `EffectVisual.h` 282 |
| `Standalone/EqWindowUI/` (strip EQ) | 5,244 | `EqGraphView` 2,222 · `EqRailView` 1,501 · `EqMatchPanel` 777 · `EqInstanceBrowser` 326 · `EqAnalyser` 320 · `EqPresets` 98 |
| Engine pages in `Standalone/` | 11,801 | `DrumKitGrid` 4,137+634 · `DrumPage` 1,761+252 · `LayersPage` 1,161+184 · `BassPage` 1,122+160 · `AriaControlPanel` 1,084+169 · `BaySickRustyDrumsPage` 765+198 · `RustyDrumsMapWindow` 128+46 |
| Boundary pages (counted shell by the build report, engine-bound per the tabs report) | 3,711 | `Source/Inst/` 1,768 (`InstPage`, `EngineChainProcessor` wrapper 82+79 in Standalone) · `Source/Clips/` 1,022 (`ClipsPage`, one `BaySickPlayer` per tab) · `Source/Vox/` 921 (`VoxPage`) |
| `Source/EffectRack.cpp/.h` | 1,401 counted as engine; **stays** as VST3-only rack (34-enumerator `EffectType` collapses to `None` + `VST3Plugin = 121`) | |
| Top-level synth primitives | 511 | `WavetableOscillator`, `SynthFilter`, `AdsrEnvelope`, `LFO`, `SynthSound`, `BroadcastSynthesiser.h` |
| Core Library fetcher | 1,104 | `Source/CoreLibraryInstaller.h`; `juce_cryptography` link exists only for its SHA-256 (`CMakeLists.txt:238-240`) |
| Safe loaders for engine content | 671 | `SafeSfzKit.h` 464, `SafeNamModel.h` 207 |
| Drum trigger map | part of 1,496 | `Source/MidiLearn/DrumTriggerMap.cpp/.h`, `<DrumTriggers>` project node |
| Tests | 1,982 | `Tools/EqTests/main.cpp` (`BaySickEqTests` target, strip-EQ only) |
| Binary/resource content | - | `big_rusty_drums.png` (`BaySickDAWAssets`), `Resources/Acoustic IRs/`, `Resources/Tape/` (25 MB with Filmstrips), `Presets\*`, `Templates\*`, `Kits\Factory\*` (NSI required inputs `:104-111`), factory reseed stamp `factory_seed_version.txt` |
| Vendored libs | - | sfizz, NeuralAmpModelerCore (+Eigen, nlohmann/json), WORLD, Rubber Band R3, Signalsmith Stretch + Linear, LunaSVG |
| Manual figure groups | 18 of 43 | `synth family`, `baysicksolstice`, `pedals`, `rusty keys`, `effects`, `fx panels`, `pedal panels`, `player`, `vox family`, `rusty family`, `drum kit grid`, `guitars`, `basses`, `kit menus`, `fx picker`, `pedals menus`, `fx rack menu`, `fx panel menu`, `engine menus`; Instrument prose group (40 figures minus `BSPLUG`/`BSPLUGM`); `FXPICK`, `FX`, `FXV`, `FXM`, `EQ`, `EQB` |

---

## 3. The coupling ledger

### 3a. By file (shell files that survive the fork)

Two metrics appear in the reports and are not interchangeable: **sites** = processor-class refs + engine-type strings + built-in `EffectType::` enumerators (tabs report); **lines** = `grep -c` of the eleven engine class names, comments included (build report).

| File | Sites | Lines | Notes |
|---|---|---|---|
| `Standalone/StandaloneEditor.cpp` | 120 | 406 | Player 19, Vocal 13, Synth 12, Solstice 10, Bass 5, Pedals 4, Rusty 4, Guitars 2, Basses 2, NAMIR 2; strings: Rusty 16, Guitars 11, Basses 9, Synth 4, Player 3, Solstice 2, Bass 2; per-page `dynamic_cast` sites: DrumPage 38, InstPage 37, ClipsPage 35, LayersPage 28, BassPage 28, VoxPage 28, RustyPage 23, PluginsPage 19 |
| `Standalone/EffectPresetIO.cpp` | 90 | - | all built-in `EffectType::` |
| `Standalone/SlotComponent.cpp` | 89 | - | built-in `EffectType::` (only 2 `VST3Plugin` sites) |
| `Standalone/ShotHarness.cpp` | 58 | - | manual harness, not shipping UI; strings Bass 5, Player 4, Solstice 3, Synth 2, Vocal/Guitars/Basses/Rusty 1 each; Pedals 4, Vocal 3 class refs; 33 `EffectType::` |
| `DSP/EffectParamMap.cpp` | 51 | - | built-in `EffectType::` |
| `Standalone/EffectEditorPanels.cpp` | 43 | - | built-in `EffectType::` (1 `VST3Plugin`) |
| `EffectRack.cpp` / `.h` | 33 / 18 | 3 | built-in `EffectType::`; enum at `EffectRack.h:19-80` |
| `EngineRig.cpp` / `.h` | 30 / - | 29 / 8 | 3 refs each to Solstice/Synth/Bass/Player/Pedals/NAMIR, 4 Vocal, 1 Guitars/Basses/Rusty; 8 engine strings |
| `Standalone/RibbonTabBar.cpp` | 29 | 47 | all engine-name strings in `buildAddMenu :561-652` and `showInstanceDropdown :845-939` |
| `PluginProcessor.cpp` / `.h` | 24 / 12 | 76 / 36 | Vocal 11, Player 3, Guitars 3, Basses 3, Rusty 3, Synth 1 (cpp); header holds the sfizz engine arrays + 4 engine strings |
| `Standalone/BuilderPage.cpp` | - | - | includes `BaySickRustyDrumsProcessor.h`, `BaySickPedalsProcessor.h`, `BaySickSolsticeProcessor.h`, `BaySickVocalProcessor.h` (`:5, 21, 22, 23`); Solstice 2, Vocal 2, Pedals 2, Rusty 1 refs |
| `Standalone/PagePresetIO.cpp` / `.h` | - | - | strings Guitars 3+1, Basses 3+1, Rusty 2; `PageKind` 8-way enum (`.h:29`) |
| `Engine/Tasks/*` | - | - | `CompositeAudioInsertTask.h/.cpp` Player 2+2; `VoxStripTask.h/.cpp` Vocal 2+2; `InstStripTask.cpp` Guitars 1, Basses 1; `RustyInsertTask.h/.cpp`, `RustyDrumsProducerTask.h/.cpp` Rusty 1 each |
| `BaySickGraph.h` / `.cpp` | - | 9 / 2 | family constants and accessors, not class refs |
| `Standalone/MixerPage.cpp` | 0 | 8 | **all 8 are comment lines** (`:1489, 2595, 2663-2665, 3242, 4131, 4375`) - resolved in section 10 |
| `Standalone/EffectWindows.cpp` | 10 | - | built-in `EffectType::` |
| `Standalone/RustyDrumsMapWindow.h/.cpp` | 4+4 | - | deleted with Rusty |
| `DSP/BaySickAlignDSP.h`, `DSP/PitchShifters.h`, `DSP/BaySickPitchDSP.*`, `DSP/PolyphaseOversampler.h`, `DSP/MicSimDSP.*`, `DSP/MicPlacementDSP.h` | 3+1+2+1+2+1 | - | all go with Vocal/NAMIR |
| `ProjectBundler.cpp` | 1 | - | NAMIR |
| `Standalone/EffectEditorPanels.h` | 1 | - | Pedals |

Reference totals: `EffectType::` built-in enumerators ~430 sites across the tree vs 11 `VST3Plugin` sites and 50 `None` sites.

### 3b. By KIND of coupling

**Type-cast dispatch (`dynamic_cast` / `make_unique` on built-in processor classes)**
- `EngineRig::apvtsOf` - 7 casts (`EngineRig.cpp:47-61`); `createEngineFor` switch (`:536-657`); `registerWithProcessor` 7-arm (`:673-758`) incl. Vocal cast for `onPitchAlignEditsChanged` (`:747`); `unregisterFromProcessor` (`:760-772`).
- `StandaloneEditor::wireEngineDirtyHook` - nine-way cast to call `setOnAnyStateChange` (`:11053-11080`).
- `applyEngineToNewestTabOfType` 4-way (`:17480-17498`); `onTabDeleteRequested` 8-arm (`:1740-1758`); `buildPageWindowRows` 8-arm (`:6567-6679`); piano-roll dropdown page walk (`:2320-2376`); `updateActiveTabState` Layers/Bass/Drum casts (`:5858-5885`); `serializeTabsInto` ladder (`:15050-15278`) and restore mirror (`:18202-18400+`); `onEnumerateRoutablePages` Clip/Vox/Inst (`:2893-2925`).
- `LayersPage.cpp:125-127, 158-176, 245-256, 994-1002` (editor + APVTS casts); same shape in `BassPage`, `DrumPage`.
- `PluginProcessor.cpp:7978` (`sweepNonRealtime` Vocal cast) + sfizz arrays `:7981-7986`; `:6664-6689` (WET recorder Vocal cast); `readClipCtl(BaySickPlayerProcessor*)` `:977-1058`, call `:1085`.
- `CompositeAudioInsertTask::mClipPlayer` (`.h:82`, `.cpp:29, 156`); `InstStripTask.cpp:52-94, 210-266` (sfizz active flags, idle-suspend); `VoxStripTask.cpp:230-234, 248-271` (vocal monitor APIs).
- `MixerPage::setInstStripNoLiveInput` (`MixerPage.cpp:2597`) called for sfizz Inst tabs.

**Engine-type strings**
- `RibbonTabBar::buildAddMenu` rows `:586-649` (`"BaySickVocal"`, `"BaySickLiveInst"`, ids 1/2/3 for Guitars/Basses/Rusty, `"BaySickSolstice"`, `"BaySickSynth"`, `"BaySickPlayer"`, `"BaySickBass"`, `"BaySickDrums"`); `showInstanceDropdown` add rows `:845-939`; `Edit > New Tab` embeds the same builder (`StandaloneEditor.cpp:11793`).
- `EngineRig::createEngineFor` string chain `:562-565, 575-581, 634` (`"Chain"`); `trackIdFor` prefixes `:64-84`.
- `LayersPage.cpp:391` `kLayerEngines`, `BassPage.cpp:376` `kBassEngines`, `DrumPage.cpp` 8 Player + 6 Synth string sites, `ClipsPage.cpp` 2, `VoxPage.cpp` 2, `InstPage.cpp` Guitars 2 + Basses 2 + `"Chain"` 2.
- Tab record `type=` strings `"Layers" | "Bass" | "Drum" | "Clips" | "Vox" | "Inst" | "BaySickRustyDrums" | "Plugins"` (`StandaloneEditor.cpp:15060-15191`); tag map `bso/bsp/bss/bsb` (`:4381-4385`); freeze `type -> TabKind` maps in two places that must agree (`:15228-15236`, `:18175-18183`); `freezeFileFor` kind names (`PluginProcessor.cpp:4428-4452`).
- `PluginProcessor.h` 4 engine strings; `PagePresetIO` Guitars/Basses/Rusty strings.
- Family-named swing params (`swing_layer_<N>_mix`, `swing_plugin_<N>_mix`, `swing_rusty_mix`) consumed at `PluginProcessor.cpp:3350-3404`.

**Automation registration**
- `registerModelEngineAutomation` switches on `TabKind::{Layers,Bass,Drums,Clips,Vox,Inst}`, Vocal cast at `:15543, :15617`, no Plugins case (`StandaloneEditor.cpp:15528-15640`); `registerSfizzEngineAutomation` (`:15642-15702`); `registerBaySickSolsticeModAutomation` (`:15704-15791`); `registerPedalAutomation` (`:15793-15840`).
- `applyOfflineLaneValue` branches: vox/inst/pedal `BuilderPage.cpp:10383-10441`, sfizz `:10472-10481`, Solstice mod `:10484-10528`.
- `unregisterAutomationForTab` one prefix per `TabKind`, Rusty returns (`:17521-17569`); `resolveAutomationDisplayName` `inst<N>_`/`vox<N>_` branches (`:4158-4230`).
- Survivors: `registerStaticAutomationHandlers` (`:17572-17645`), `registerPluginTabAutomation` (`:15857-15904`), `EffectsPage::registerRackAutomationForAllChannels` + `vst_` fork (`BuilderPage.cpp:10545-10585`).

**Preset IO**
- `EffectPresetIO.cpp` 90 `EffectType::` sites, folder map incl. `"VST3 Plugins"` (`:78`); `FxRackPresetIO`.
- `PagePresetIO::PageKind` 8-way (`PagePresetIO.h:29`) + Inst `source`/`kitPath`/`sfizzEngineData` extras.
- Page menus `Save Current Patch As... / Load Preset >` (Layers `:396-512`, Bass `:381-492`, Drum `:1160-1291`), Drum empty-slot sound menu (`DrumPage.cpp:428-500`), kit save/load (`StandaloneEditor.cpp:9002-9314`), Rusty player presets (`BaySickRustyDrumsPage.cpp:351-374`), Inst pedalboard presets (`InstPage.cpp:1318-1330`).
- Factory reseed (`ProjectManager.cpp:435`, `kFactorySeedVersion`); `Presets\<engine>\`, `Templates\Factory`, `Kits\Factory` trees; `CoreLibraryInstaller.h` + `SampleLibrary.cpp:24-35, 62`.

**Page menus / windows**
- Per-page `PageMenuBar` builders listed above; Plugins menu (`PluginsPage.cpp:618-730`) is the only survivor.
- Vox satellites `openVoxSatelliteWindow :16572`, `closeVoxSatellites :16898`; Inst `openInstPedalsWindow :16648`, `openInstNamIrWindow :16787`, `installInstNavMenu :16850`, `closeInstSatellites :16907`; `showRustyDrumsMapWindow :10035`.
- Drum-kit subsystem `StandaloneEditor.cpp:8010-8530, 9002-9314` + `mUsedDrumIndices` (`.h:990`), `mActiveDrumBank`.
- Tab spawn/duplicate: `addBaySickRustyDrumsTab :10099`, `addBaySickGuitarsTab :10331`, `addBaySickBassesTab :10444`, `createClipStripAndPage :10538`, `addClipPageFromFile :10575`, `spawnClipsTabIfMissing :10702`, `spawnVoxTabIfMissing :11216`, `spawnInstTabIfMissing :11345`, `spawnDuplicate{Layer,Bass,Drum,Clips,Vox,Inst}Tab :2553, :2610, :2667, :10860, :11481, :11536`.
- `RibbonTabBar.h:64-81` callbacks `onAddBaySickGuitarsRequest`, `onAddBaySickBassesRequest`, `onAddBaySickRustyDrumsRequest`, `onIsBaySickRustyDrumsActive`, `onIsInstCapReached`; dead Drums arm in `showSubPageDropdown :736-741`.
- `RenameFamily` (`StandaloneEditor.h:583`); `pageIsLocked` (`:15038-15047`, no Plugins arm).

**Audition**
- Three near-identical cascades `StandaloneEditor.cpp:8289-8301, 8333-8353, 8379-8393` (Layer / Bass / Drum) + `:8421`; sfizz `auditionNote` wiring; `isAuditionPending()` peeks on the 3 sfizz engines.
- Survivor: hosted plugin audition through the live-MIDI collector via `sendTypingNote` (`:10988-11044`).

**Sidechain**
- Engine-level SC via `ISidechainEngine` (`DSP/EngineSidechainHelper.h:13-19`), pushed by `EngineInsertTask.cpp:81-88` - only built-ins implement it; `HostedPluginInstance` does not (`HostedPlugin.h:42-43`).
- `VoxStripTask.cpp:280-288`, `InstStripTask.cpp:316-324` pull SC predecessors for the engine.
- Per-EQ-band `scSourceId` (`DSP/DSPBase.h:110`) goes with the strip EQ.
- Survivors: 4 `_sc_recv{N}_from` lines per strip (`PluginProcessor.cpp:9019-9023`), rack `Slot::scPick` (`EffectRack.h:169, 283`), key taps + alignment delays (`BaySickGraph.cpp:2954, 2978`), SC in cycle check/topo (`:2662-2677, 2801-2803`).

**Freeze**
- `insertKindForTab` (`PluginProcessor.cpp:4405-4422`, exhaustive, no default), `renderTaskForTab` (`:4372-4398`), `freezeFileFor` (`:4428-4452`) - all per `TabKind`.
- `renderKitFreezeFiles` (`BuilderPage.cpp:10029-10040`); Rusty 13-task special case (`PluginProcessor.cpp:4676, 4745, 5138`; `EngineRig.cpp:276`).
- `wireFreezeSlotForVisiblePage` Rusty/Vox special cases (`StandaloneEditor.cpp:7813-7814, 7859-7861, 7882-7883, 7891`); `onIsTabFrozen` `TabType -> TabKind` map (`:1896-1906`).
- Survivors: generic substitution in `EngineInsertTask.cpp:112-120`; hosted-plugin staleness via `AudioProcessorListener` (`EngineRig.cpp:387-410`); revival re-publish (`:436-533`).

**Export / offline**
- `sweepNonRealtime` (`PluginProcessor.cpp:7971-7990`): Vocal cast + embedded NAM/IR, 30 Guitars + 30 Basses arrays, Rusty; rack-slot sweep `:7987-7989` survives (that is what reaches hosted effects).
- `ClipCtl` shaping of grid decode reads a `BaySickPlayer` APVTS (`:977-1058`); decode falls back to raw when the pointer is null (`:975-978`).

**Recording**
- `startRecording` scans `mixer_vox_`/`mixer_inst_` only (`PluginProcessor.cpp:6694-6697`), WET path Vocal-only (`:6664-6689`, `:6736-6744`); `commitRecordingResult` classifies by `kVoxBase`/`kInstBase` ranges (`StandaloneEditor.cpp:20923-20926`), Vox denoise branch (`:20938-21037`), `mVoxTakePick[kDenoiseMaxVox]` (`:5272-5281`).
- MIDI record: `LastRollKind { Layer, Bass, Drums }` (`StandaloneEditor.h:1081`), refusal (`.cpp:1134-1148`), commit switch (`:21110-21215`).
- `DrumTriggerMap` + `dispatchDrumTriggers` (`PluginProcessor.cpp:8566-8569`).

**Live MIDI routing**
- `PluginProcessor.cpp:3585-3594` switch on `EngineKind` ordinal (1 Layer / 2 Bass / 3 Drum / 4 Clip / 7 Guitars / 8 Basses / 9 Rusty / 10 Plugin); kind 8 hard-coded -12 transpose (`:3604`); roll dropdown skips `InstPage::Source::LiveInput` (`StandaloneEditor.cpp:2350-2366`).

**Scheduler / pattern data (family-indexed, not bus-indexed)**
- `Pattern` roll arrays `layerRoll, bassRoll, drumRoll, drumRolls, clipRoll, voxRoll, instRoll, pluginRoll, baySickRustyDrumsRoll` (`PatternManager.h:269-303`); snapshots (`:552-573`); `BlockContext` MIDI arrays (`Engine/BlockContext.h:54-57`); roll XML tags (`PatternManager.cpp:1337-1397`); `sched()` fan-out (`PluginProcessor.cpp:3308-3404`); PR-target bases summed from page caps (`BaySickConstants.h:26-39`).

**Mixer family plumbing (not engine classes, but family enums that die with the engines)**
- `MixerChannelIds` families Layer/Bass/Drum/Vox/Inst/Rusty (`BaySickGraph.h:33-303`, 8 sites per family); `InsertKind` (`.h:631`); `MixerTrackStrip::StripType` (`MixerTrackStrip.h:92-102`); `MixerPage::StripKind` (`.h:76`); `addXxxChannel/removeXxxChannel` (`MixerPage.cpp:1668-3330`); `isRouteAllowed` 10 family branches (`:482-587`); per-kind task arrays (`PluginProcessor.h:2379-2380`); per-kind peak arrays (`.h:985-994`); EQ strip-slot table (`.h:2083-2089`); `EffectsPage::channelToMixerId` (`:353-366`).

**Build / manual**
- `CMakeLists.txt:157-233, 512-681, 703`; `BaySickDAWAssets` (`:32-39`); `Resources/` staging (`:814-820`); NSI required inputs (`:104-111`).
- `ShotHarness.cpp` 18 engine figure groups; `mixer` group seeds via `TabKind::Bass` + `"BaySickBass"` (`:1012-1043`); `eq` group (`:1392-1425`); vox family `Thread::sleep(550)` timer exception; `engine menus` / `kit menus` consume earlier kit loads; `editor menus` must run last (`:1960-1965`).

---

## 4. Mixer/routing today vs the FL-style target

| Property | Exists today (file:line) | Gap | Files that change |
|---|---|---|---|
| Bank of inserts that pre-exist | Only Master + 17 buses are eager: params `PluginProcessor.cpp:9065-9102`, nodes `BaySickGraph.cpp:1091`, routing head `:3174-3215`, tasks `PluginProcessor.cpp:879-905`. Aux strips are engine-less inserts (`PassiveStripTask.cpp:65-71`) but lazy, cap 18 (`BaySickGraph.h:102`), monotonic `mNextAuxIdx` (`MixerPage.h:436`). Flat arrays `mInsertsByChannel[1000]` / `mTasksByChannel[1000]` (`BaySickGraph.h:1078-1079`; `RenderGraphDispatcher.cpp:75`) | Every insert is created by a tab/engine event (creation table `PluginProcessor.cpp:8146-9362`). No startup loop creates N inserts. Params are never removed (`:9044`). UI groups by destination bucket, not bank order (`MixerPage.cpp:4062-4076`). Aux widgets have no arm/listen row | `BaySickGraph.h` (all 8 `MixerChannelIds` sites `:36-302`), `PluginProcessor.cpp` (`ensureAuxInsert :9265` or a new family, eager loop beside `kBusChannelIds`), `MixerPage.cpp` (`addAuxChannelAtIndex`, `layoutScrollContent :4033-4395`, `clearDynamicStrips :2731`), `EffectsPage.cpp:353-366` |
| Any hosted instrument routes to any insert | Engine->strip plumbing is generic and already carries a hosted VST3 (`registerPluginEngine` `PluginProcessor.cpp:8167-8186`); dispatcher can re-register a task on a channel (`RenderGraphDispatcher.cpp:48-132`) | Binding is `chId = base + pageIndex`, fixed at registration (`PluginProcessor.cpp:8156-8184`), arena slot bound once (`RenderGraphDispatcher.cpp:97`), params live under the same index-derived prefix. No instrument->insert param exists in `addParamsForMixerStrip` (`:8916-9038`). Nothing reassigns a channel id | `PluginProcessor.cpp` (`register*Engine`), `Engine/Tasks/EngineInsertTask.*`, `RenderGraphDispatcher.cpp:48-132`, `BaySickGraph.h` (new binding param), `MixerPage.cpp` (strip identity, `pickStripColor :68`, `onChannelRenamed`) |
| Any insert -> any insert with a level (bus) | Aux->aux main-out is legal and sums (`MixerPage.cpp:567-568`; `PassiveStripTask.cpp:38-57`). Sends carry level -60..+6 dB + pre/post (`PluginProcessor.cpp:9005-9011`). `RoutingGraph` imposes no legality (`BaySickGraph.cpp:2682-2787`). Cycle DFS over audio+SC edges (`:2658-2680`); Kahn topo drops cycles (`:2789-2857`) | (a) Main-out edges are unity by construction: `e.amountDb = 0.f` (`BaySickGraph.cpp:2748`), `isMainOut ? 1.0f` (`MasterTask.cpp:54`, `PassiveStripTask.cpp:45`). (b) Sends are aux-only (`isValidBusSendTarget` `BaySickGraph.h:219-222`; menu filter `MixerPage.cpp:711`). (c) `isRouteAllowed` is a 10-branch family whitelist (`MixerPage.cpp:482-587`). (d) Only `PassiveStripTask` and `MasterTask` sum `mPredecessors`; every other task clears and renders its own source (`EngineInsertTask.cpp:71, 80`; `CompositeAudioInsertTask.cpp:66`; `DirectFileTask.cpp:27`) so an insert->insert edge is built, ordered, and silently dropped (`RenderGraphDispatcher.cpp:151-237`). (e) Layout assumes destination is a bus (`laidOutBus :4239`, aux special-case `:4328-4332`) | `Engine/Tasks/*.cpp` (predecessor-sum prologue), `BaySickGraph.cpp:2733-2760`, `BaySickGraph.h:219`, `MixerPage.cpp:482-587, 4033-4395`, `Engine/UpstreamLink.h` if a new link flavour |
| Sends per insert | 4 per strip: `_send{0..3}_to/_amount/_prepost` (`PluginProcessor.cpp:9008-9010`); `kMaxSendsPerStrip = 4` (`BaySickGraph.h:334`) | Count fixed at 4; destination limited to aux range | `BaySickGraph.h:219-222, 334`, `MixerPage.cpp:684-871` |
| Multi main-out | Up to 4 lines, each a full-level copy (`BaySickGraph.h:197-215`; `_mainOut{1..3}_to` `PluginProcessor.cpp:9000-9002`); duplicate dst dropped (`BaySickGraph.cpp:2741-2744`) | No level on any line | `BaySickGraph.cpp:2735-2750` |
| Sidechain flags | Complete and strip-generic: 4 `_sc_recv{N}_from` per every strip (`PluginProcessor.cpp:9019-9023`), target-side encoding (`BaySickGraph.cpp:2765-2771`), delay-matched key taps (`:2954, 2978`), `Slot::scPick` (`EffectRack.h:169, 283`), cycle/topo participation | None at the strip level. Hosted-side gaps in section 6 | - |
| Live audio input per insert | `_inputChannelIdx`, `_inputChannelStereo`, `_listen`, `_monitorMode` from `addLiveInputParams` (`PluginProcessor.cpp:9926-9976`); `BlockContext::liveInputSnapshot` (`Engine/BlockContext.h:62`; fill `PluginProcessor.cpp:2724-2749`) | Gated three ways on Vox/Inst: `addLiveInputParams` called only at `:9318, :9330`; `_arm` only for `mixer_vox_`/`mixer_inst_` prefixes (`:8979-8983`); UI rows only on `StripType::Vox/Inst` (`MixerTrackStrip.cpp:361-375, 387-450`); capture lives in `VoxStripTask`/`InstStripTask`, which require an engine (`PluginProcessor.cpp:8813-8820, 8862-8870`) | Section 7 |
| Arm / record per insert | `_arm` bool (`:8979-8983`); `tapDryRecorder` keys on channel id (`:6824-6852`); `StripRecorder` container (`PluginProcessor.h:1567-1569`) | `startRecording` scan hard-codes two prefixes + bases (`:6694-6697`) | `PluginProcessor.cpp:6591-6727`, `StandaloneEditor.cpp:20760-21092` |
| Input gain / trim | none (repo grep: only `tape_inputGain` inside `SaturationDSP`) | missing entirely | `PluginProcessor.cpp:8916-9038`, `MixerTrackStrip` |
| Strip identity / naming | Strip = `(StripKind, pageIndex)` -> `MixerChannelIds` -> `mixer_<family>_<i>` (`MixerPage.h:76, 163-172, 273`; `BaySickGraph.h:135-169`); rename both ways (`StandaloneEditor.cpp:5683`; `MixerPage.h:85`); names/orders persisted in `<UIState>` (`StandaloneEditor.cpp:15306-15314, 18984-18990`) | Identity is family+index, not a bank slot. `mixer_*` prefixes and 0..999 ids are frozen persistence (`BaySickGraph.h:200-208`; `PluginProcessor.cpp:8993-8994`) | `BaySickGraph.h`, `MixerPage.*`, `StandaloneEditor.cpp` UIState |
| Bus definition | 17 hard-coded ids (`isBus` literal enumeration `BaySickGraph.h:172-182`); each a distinct `unique_ptr<InstrChannelNode>` member (`.h:1008-1107`) with hand-written accessor triples (`.h:469-564`) and `switch`es in `processBus` (`.cpp:1259-1480`), `drainBusRms` (`:2440`), `busNodeForChannel` (`:2879`), `rebindBusApvts` (`:2579`); bus chain order fader-before-polarity/width, the reverse of an insert | A bus is not a role a strip can take. Bus visibility is membership-driven (`laidOutBus MixerPage.cpp:4239-4301`, always-visible set `:3167-3177`, ever-routed lifecycle `:4259-4272`) | `BaySickGraph.h/.cpp` (9 sites B9-B17), `MixerPage.cpp` bus members `.h:362-401`, `buildAddMenu :658-682`, `activate*`/`deleteSecondaryBus :1916-2226, 3257` |
| Strip widget type | `StripType { Master, Bus, DrumChannel, LayerChannel, BassChannel, Aux, Vox, Inst }` (`MixerTrackStrip.h:92-102`); Plugin/Direct reuse `LayerChannel` with an accent (`MixerPage.cpp:1708-1709, 1741-1742`); 80 px every type | No generic "insert" type with the utility row | `MixerTrackStrip.h/.cpp` (`hasUtilityRow`, `setApvts :317-450`) |
| PDC | `updateBusLatencies` (`BaySickGraph.cpp:1589`), rack sum (`EffectRack.cpp:764-772`) | Engine latency term only for Vox/Inst (`BaySickGraph.cpp:1681-1686`; hooks `.h:579, 585`); hosted instrument latency not in PDC | `BaySickGraph.cpp:1681-1686`, `PluginProcessor.cpp:842, 853` |
| Solo | Two independent axes: `isAnyInsertSoloed` (`.cpp:2620`), `anyBusSoloed` (`:2643`, 17 cached pointers `.h:1039`) with a no-cross-talk guardrail (`.h:811-824`) | Bus solo is a 17-pointer table; a strip-as-bus has no solo axis | `BaySickGraph.cpp:2579-2643` |
| Meters | Per-kind peak arrays (`PluginProcessor.h:985-994`) -> `drainInsertPeakDbStereo` (`.cpp:5233`); `onVBlank` `drainStereoInsert`/`drainStereoBus` (`MixerPage.cpp:3854-3926`) | Keyed by family | `PluginProcessor.h/.cpp`, `MixerPage.cpp` |
| Legacy `MixerState` undo snapshot | Backs Master/Layers/Bass/Drums/Clips bus level/pan/mute/solo + per-drum/per-audio-row values (`applyMixerSnapshot`/`syncApvtsFromMixerState`, `MixerPage.h:605-608`) | Second, partial mixer model keyed on removed families | `MixerPage.h:605-608`, `PatternManager` `<Mixer>` node (`.cpp:1254`) |
| Routing UI | Per-strip "+" menu, five submenus (`MixerPage.cpp:684-871`), undo-gestured writes; cable overlay paint + right-click only (`:473-476, 589-593`, `showCablePopup :1176`); 30 Hz `_sendTo` hash relayout (`:3770-3838`) | All submenus filter on family whitelists | `MixerPage.cpp:684-871, 482-587` |
| Physical master out | `MasterOutputRouting::gFirstOutputChannel` / `gMasterIsMono`, `master_output.xml` (`StandaloneApp.cpp:157, 440-464`; `StandaloneEditor.cpp:7745-7747`) | none | - |
| Channel ceiling | `kMaxStripChannels = 1000` sizes three arrays (`BaySickGraph.h:1078-1079, 1190-1191, 1220`; `RenderGraphDispatcher.cpp:23, 75`) | Bank size bounded by 1000 minus reserved ranges | `BaySickGraph.h`, `RenderGraphDispatcher.cpp` |

---

## 5. Tabs-from-buses

### What a tab IS as data today

Three records with no single owner, plus a fourth copy on the page:

1. **Ribbon record** `RibbonTabBar::Tab { int id; TabType type; String name; bool locked; bool kitMissing; }` (`RibbonTabBar.h:26-38`). No engine pointer, page index, colour, or strip binding. Colour is a pure function of `TabType` (`RibbonTabBar.cpp:7-35`). Per-type last-visited `mLastUsedByType` (`.h:284`).
2. **Editor record** `StandaloneEditor::PageEntry { ribbonTabId, TabType, unique_ptr<Component> component, unique_ptr<WorkspaceWindow> window, pageIndexHint }` (`StandaloneEditor.h:212-226`), append-ordered in `mPages` (`.h:963`); "newest of type" = last matching entry (`.cpp:17486-17488`).
3. **Model record** `EngineTab { kind, pageIndex, engineType, engine, ownedStages, pedals, namIr, freeze block }` (`EngineRig.h:51-213`). Deliberately no name (`:52-57`). Hosted plugin `engineType` is the `PluginDescription` identifier string (`EngineRig.cpp:590-629`).
4. Page component copy of name/lock/engineType (`LayersPage.h:153-155`; `PluginsPage.h:39-41`).

Five parallel enums, mapped by hand in at least five places: `TabKind` (`EngineRig.h:49`, 8 values incl. Rusty), `RibbonTabBar::TabType` (`RibbonTabBar.h:24`, 11 values, no Rusty), `PianoRollPage::EngineKind` (`.h:38`, 11 values, also the live-MIDI target encoding), `MixerPage::StripKind` (`.h:76`), `BaySickGraph::InsertKind` (`.h:631`). Maps at `StandaloneEditor.cpp:1896-1906, 7777-7788, 15228-15236, 18175-18182` and `PluginProcessor.cpp:4405-4422`. `TabKind` and `TabType` are persisted as raw ints, append-only (`EngineRig.h:43-48`; `RibbonTabBar.h:22-23`).

Persisted form: one `<Tab type pageIndex name engine engineData locked frozen ...>` per page via a `dynamic_cast` ladder (`StandaloneEditor.cpp:15050-15278`); Plugins tab always writes `locked=0` because `pageIsLocked` has no Plugins arm (`:15038-15047`).

Strip creation is tab-driven: a Layer/Bass/Drum/Plugin strip appears on first engine pick (`:5654-5657, 5763, 5801, 5713`); Vox/Inst are inverted - the strip is created first and fires the page spawn (`MixerPage.h:90-100`; `StandaloneEditor.cpp:11216, 11345`), which is why `canRebuildType` excludes them (`StandaloneEditor.h:277-278`).

### What survives with hosted plugins only

- Required ribbon slots Builder / Mixer / Effects / PianoRoll (`RibbonTabBar.cpp:38-44`); `visibleSlotTypes` already hides zero-instance types (`:46-63`), so the ribbon degrades to five slots + "+" with no change.
- `TabKind` keeps `Plugins` only; `capacityOf` (`EngineRig.cpp:28-45`), `trackIdFor` (`:64-84`), `apvtsOf` (`:47-61` -> `nullptr`), `createEngineFor`, `registerWithProcessor`, `unregisterFromProcessor` each collapse to one arm.
- `PluginsPage` ("deliberately the thinnest page", `PluginsPage.h:9-19`) + four permanent pages. Plugins menu keeps Rename / Replace Plugin / Duplicate / Save-Load Page Preset / Automate / Retry / FX Rack / Freeze / Delete (`PluginsPage.cpp:618-730`; shared tail `SharedUI.h:432-436`).
- `PageMenuBar` mechanics, `WorkspaceWindow`, `onListPageWindowRows`/`onPageWindowRowPicked`, `showSubPageDropdown` for Effects/Builder.

### What a tab would be (per the brief) and which UI pieces key off it

A tab per **bus** listing the instruments routed into it. Today no record anywhere expresses "bus" as a tab identity; the bus->members enumeration exists in two reusable places: `MixerPage::layoutScrollContent` bucketing by `_sendTo` (`MixerPage.cpp:4060-4076`) and `EffectsPage::addBusAndMembers` fed by `MixerPage::getStemPickEntries` / `getXxxStripIndices` (`EffectsPage.cpp:415-478`; `MixerPage.h:146-152`).

UI pieces that key off the current tab identity and would re-key:

| Surface | Today keys on | Site |
|---|---|---|
| Ribbon slot order + colour | hard-coded `order[]` and `switch(TabType)` | `RibbonTabBar.cpp:51-56`, `:7-35`; `getBadgeCount`/`countTabsOfType` (`.h:265-266`) |
| Ribbon frozen dot | folds `isFrozen`/`isFreezeStale` over every page index of the mapped `TabKind` (one slot per type) | `RibbonTabBar.h:82-85`; `StandaloneEditor.cpp:1889-1920` |
| "+" Add menu / per-type dropdown / `Edit > New Tab` | engine names; only `VSTPlugin >` survives | `RibbonTabBar.cpp:561-652, 766-1030`; `StandaloneEditor.cpp:11793` |
| `Pages:` rows in the dropdown | 8-arm page cast -> `Player, Piano Roll, Pre EQ, Post EQ` (EQ rows keyed on `eqChannelId`, which dies with the strip EQ) | `StandaloneEditor.cpp:6567-6679` |
| Page window title / hamburger Freeze slot | `visiblePageTabIdentity` -> `wireFreezeSlotForVisiblePage` | `StandaloneEditor.cpp:7775-7892` |
| Piano-roll context label `"{tabName} - {engineType}"` and dropdown | `EngineId { EngineKind, index }` pushed per tab (`setEngineType :5661`, `setEngineDisplayName :5684`, plugin `:5710-5730`); dropdown walks `mPages` (`:2320-2376`) | `PianoRollPage.h:40-68` |
| Live-MIDI target | `PianoRollPage::onEngineSelected` -> `setLiveMidiTarget((int) kind, index)` | `StandaloneEditor.cpp:2378-2396`; `PluginProcessor.cpp:3585-3594` |
| `<PianoRollSelection kind index>` | `EngineKind` **name**, exhaustive 11-way switch | `StandaloneEditor.cpp:15008-15021` |
| Mixer strip <-> tab rename sync | `(StripKind, pageIndex)` | `StandaloneEditor.cpp:5683`; `MixerPage.h:85` |
| Builder rows | **not tabs**: 500 free rows, name/mute/solo/group only (`PatternManager.cpp:1289-1311`); a pattern block on any row schedules every family (`PluginProcessor.cpp:3308-3404`); the only row<->tab identity is Clips page index == audio row == `audioInsert(row)` (`StandaloneEditor.cpp:10539-10572`); audio clips route by `ArrangementBlock::routeChannel` (`PatternManager.h:402`; `PluginProcessor.cpp:1113-1124`) | - |
| Builder automation lane display names | lane prefix -> tab name (`plugtab<N>_vst_` via `mPages` + `getTabById`) | `StandaloneEditor.cpp:4158-4230` |
| Audio-clip "Routes to:" picker | walks `mPages` for Clip/Vox/Inst types | `StandaloneEditor.cpp:2893-2925`; `BuilderPage.cpp:5310-5392` |
| Tab XML record + freeze maps | `type=` string per page class, two `type -> TabKind` maps | `StandaloneEditor.cpp:15050-15278, 18175-18305` |
| Automation de-registration | one lane prefix per `TabKind` | `StandaloneEditor.cpp:17521-17569` |
| Freeze task/kind/file | per `TabKind` | `PluginProcessor.cpp:4372-4452` |
| Page presets | `PagePresetIO::PageKind` 8-way | `PagePresetIO.h:29` |
| F7 "Show Player (Most Recent)" | `isPlayerTabType` | `StandaloneEditor.cpp:5889-5909` |

---

## 6. Hosting gaps

From the hosting report; tag = complete / partial / missing for "VST3 hosting is the only instrument and effect path".

| Capability | Tag | Evidence |
|---|---|---|
| Scan, allowlist (`plugins.xml`), skip-with-reason, per-file helper crash isolation, blacklist self-clearing, arch split before load | complete | `PluginManager.cpp:396-553, 613-637`; `OutOfProcessScanner.h:39-176` |
| Instrument as first-class engine (strip, roll, rack, freeze, page preset, undo, rename/replace/duplicate) | complete | `PluginProcessor.cpp:8167-8186`; `EngineRig.cpp:589-620`; `PluginsPage.cpp:608-750` |
| Effect in any of the 6 rack slots on any strip/bus/master | complete | `EffectRack.cpp:88-90, 146-190`; `SlotComponent.cpp:820-856` |
| Editor hosting in-process + bridged, self-resize tracking, dead-marker | complete | `HostedPlugin.cpp:768-1273`; `PluginsPage.cpp:249-293`; `EffectWindows.cpp:130-170` |
| State in project + presets, survives a dead plugin (`mLastKnownState`), allowlist-checked on restore | complete | `HostedPlugin.cpp:648-763, 675-719`; `HostedPluginEffect.cpp:181-217` |
| Automation by stable param id, live + offline, bridged async param list | complete | `HostedPlugin.cpp:231-311`; `StandaloneEditor.cpp:15857-15904`; `EffectsPage.cpp:735-790` |
| Transport/playhead + non-realtime across both surfaces | complete | `HostedPlugin.cpp:512-528`; `HostedPluginEffect.cpp:83-127`; `PluginProcessor.cpp:7971-7989` |
| Crash / deadline containment (per-slot silence, no audio stall) | complete | `SandboxedPluginClient.cpp:245-388` |
| 32-bit + 64-bit helpers built, staged, installed | complete | `Helper/CMakeLists.txt`; `do_build.bat:237-288`; `.nsi:71-72` |
| Rack-slot sidechain into the plugin's SC bus | partial (in-process only) | `HostedPlugin.cpp:472-492, 596-613`; bridged never reports SC (`:402-409`; `PluginHostMain.cpp:346`) |
| Sidechain into a hosted **instrument** | missing | `HostedPlugin.h:42-43` (no `ISidechainEngine`); `EngineInsertTask.cpp:81-88` |
| Hosted instrument latency in PDC | missing | `BaySickGraph.cpp:1681-1686` (engine term only Vox/Inst); rack-slot latency is complete (`EffectRack.cpp:764-772`) |
| Bridged latency updates after load | missing | no message in `PluginBridgeProtocol.h:69-95`; read once `HostedPlugin.cpp:404-408` |
| Multi-output instruments | missing (bus 0 only) | `HostedPlugin.cpp:41-46, 615-621`; one strip per tab `PluginProcessor.cpp:8167-8186`; bridged helper prepares 2/2 (`PluginHostMain.cpp:324-347`) |
| MIDI out from a plugin / plugin-to-plugin MIDI / MIDI effects | missing | `HostedPlugin.h:127` (`producesMidi` false); `HostedPluginEffect.cpp:59-67`; protocol has no helper->host MIDI |
| Per-track MIDI input device / channel filter / more than one live target | missing | one collector, one `(kind,index)` target (`PluginProcessor.h:640-657`; `.cpp:3579-3595`); all devices merged (`StandaloneApp.cpp:1184-1199`); scheduled notes always channel 1 |
| Instrument/effect classes disjoint by `isInstrument` | partial | `PluginManager.cpp:157-177`; `SlotComponent.cpp:835`; `PluginsPage.cpp:629`. A 32-bit VST3 enters as `isInstrument=false` (`PluginManager.cpp:467`) so it is rack-picker-only until a rack load refines it; refinement changes `createIdentifierString()` (inferred in the report) |
| `.vstpreset` / `.fxp` / program browsing | missing | repo-wide absence; `setCurrentProgram` has no caller (`HostedPlugin.cpp:387-392`); page presets + rack presets store the opaque blob instead |
| MIDI Learn onto plugin params | missing | `MidiLearnRegistry.cpp:154, 183` (APVTS-only) |
| Bridge toggle on a Plugins tab | missing (rack-slot only) | `EffectWindows.cpp:347-370`; `EngineRig.cpp:477-484` |
| Bridged parameter reads | partial (fallback constant) | `HostedPlugin.cpp:305-311` |
| Bridged MIDI per block | partial (4096-byte cap, silent truncation) | `SandboxedPluginClient.cpp:290-291` |
| Bridge as security boundary | missing (plain `CreateProcess`) | `HostedPlugin.cpp:699-701` |
| Rack retry of a user-bridged 64-bit plugin | partial (loads in-process first) | `EffectsPage.cpp:970-980` |
| Plugin-tab window user-resizable | missing (surface owns size) | `PluginsPage.cpp:270`; `EffectWindows.cpp:145` |
| Caps | fixed | 20 plugin tabs (`BaySickConstants.h:25`), 6 rack slots (`EffectRack.h`), 4096 MIDI bytes |
| Bridged path tested with a real 32-bit VST3 | missing | `Main Plan.md:1998` "UNTESTED (no 32-bit VST3 on hand)" |
| `SetDefaultDllDirectories` hardening | reverted pending a hosting test | `StandaloneApp.cpp:1392-1420` |
| Doc drift | - | `Plugins Page.md:85-91, 110` describe resize behaviours and a menu without Rename/Duplicate that the code no longer matches |

---

## 7. Live input as a strip property

### Reusable as-is

| Piece | File:line | Why |
|---|---|---|
| `mLiveInputSnapshot` + `BlockContext::liveInputSnapshot` | `PluginProcessor.cpp:2724-2749`; `Engine/BlockContext.h:62` | processor-wide, prefix-agnostic, taken before `buffer.clear()` |
| `addLiveInputParams(prefix)` | `PluginProcessor.cpp:9926-9976` | takes any prefix; only the two `_monitorMode` `startsWith` guards (`:9960, :9971`) are family-specific |
| `setInputChannelName` / `getInputChannelName` | `:9978-9991` | pure `<prefix>_inputChannelName` property |
| `tapDryRecorder` | `:6824-6852` | keys on channel id only; bails on `isNonRealtime()` |
| `StripRecorder` / `AudioFileRecorder`, `mStripTapsLive` gate, `settleAudioThread` | `PluginProcessor.h:1567-1569`; `.cpp:6598-6608, 6729-6751` | `{channelId, file, recorder}` with no type assumptions |
| `showInputChannelPicker` list half | `MixerPage.cpp:2452-2506, 2529-2580` | works off `prefixFromChannelId`; `computeChannelGroups` (`:2296-2444`) is a free function |
| Inst-style Dry/Wet monitor crossfade | `InstStripTask.cpp:332-372` | buffer-level, ~15 ms ramp, no engine named |
| Listen gate + pre-fader-tap correction | `InstStripTask.cpp:389-395` | generic |
| `dropWavAsClip(file, routeChannel)` | `StandaloneEditor.cpp:20783-20890` | `routeChannel != 0` already means "replay through that insert" |
| `PassiveStripTask` | `Engine/Tasks/PassiveStripTask.*` | an insert that renders with no engine (Aux/Bus) |
| Master-output fallback capture | `PluginProcessor.cpp:6707-6726` | independent of families |
| Record mode chevron (`RecordMode { Audio, Midi }`) | `GlobalTransportBar.h:129-131`; `.cpp:481-508` | labels read "ASIO" / "MIDI (piano roll tabs only)" |

### Welded to page/engine

| Weld | File:line |
|---|---|
| Task exists only when an engine is registered; both `run()`s call `mEngine->processBlock` unconditionally | `PluginProcessor.cpp:8813-8820, 8862-8870`; `VoxStripTask.cpp:296`; `InstStripTask.cpp:346` |
| Fixed per-kind task arrays | `PluginProcessor.h:2379-2380` |
| `_arm` minted for two prefixes only | `PluginProcessor.cpp:8979-8983` |
| `addLiveInputParams` called from two sites; Aux (`:9280`) and Rusty (`:9341-9349`) skip it | `:9318, :9330` |
| `_monitorMode` range chosen by prefix (Vox 0..2 default 2; Inst 0..1 default 1) | `:9960, :9971` |
| Recorder scan hard-codes prefixes + bases | `:6694-6697` |
| WET recorder is Vocal-only | `:6664-6689, 6736-6744` |
| Commit routes by id range, Vox branch calls `voxEngineAt` + `Denoise::*` | `StandaloneEditor.cpp:20923-20926, 20938-21037, 21039-21041` |
| Grid-default pick is a Vox-indexed array | `StandaloneEditor.cpp:5272-5281`; `MixerPage.cpp:2507-2521, 2539` |
| Strip UI gated on `StripType::Vox || Inst`; monitor menus hardcoded per type | `MixerTrackStrip.cpp:361-375, 387-450`; `.h:432-435`; `setNoLiveInput` is a suppression flag (`.h:189`) |
| Picker wired only on the two strip factories | `MixerPage.cpp:2257, 2642` |
| Vox monitor path calls vocal APIs | `VoxStripTask.cpp:230-234, 248-271` |
| Inst task sfizz coupling | `InstStripTask.cpp:52-57, 91-94, 210-266` |
| Strip-first tab spawn cascade | `MixerPage.cpp:2269, 2652` -> `StandaloneEditor.cpp:5304-5321` |
| Live-MIDI target excludes live-input strips | `StandaloneEditor.cpp:2350-2366`; `PluginProcessor.cpp:3667-3670` |
| Input-channel index vs picker list may diverge under non-ASIO partial masks (inferred) | `StandaloneApp.cpp:932-953` |
| No input gain / trim parameter anywhere | repo grep |
| No take lanes / comping - each take is a new `trackRow` | `StandaloneEditor.cpp:20844-20872` |

---

## 8. Sequencer + persistence changes

**Builder rows** - survive intact: 500 free-form rows, mute/solo/name/group/colour only (`PatternManager.cpp:1289-1311`); row mute gates pattern blocks (`PluginProcessor.cpp:3313`). Audio clips route by `routeChannel` (`PatternManager.h:402`); legacy migration stamps `routeChannel = audioInsert(trackRow)` (`PluginProcessor.cpp:5800-5801`). The Clips tab is optional; the audio-insert strip is not: grid clips decode in `CompositeAudioInsertTask` (`.cpp:120-167`) and `restoreAudioStripsFromArrangement` rebuilds strips with no tab (`StandaloneEditor.cpp:19828-19846`). With `BaySickPlayer` gone, `ClipCtl` shaping (volume/pan/filter/drive/ADSR/tune/reverse/stretch/vibrato) is lost and the decode stays raw - already the documented no-engine behaviour (`PluginProcessor.cpp:975-978`). `readClipCtl` (`:977-1058`) and `CompositeAudioInsertTask::mClipPlayer` (`.h:82`) are compile-breaking references. `AudioClipStreamer`, `PhaseVocoder`, decoded-clip cache, snapshot/retirement machinery all survive (`DSP/AudioClipStreamer.h`, `PluginProcessor.h:889-1050`).

**Pattern data** - family-indexed, not bus-indexed: `Pattern` roll arrays (`PatternManager.h:269-303`), snapshots (`:552-573`), `BlockContext` MIDI arrays (`Engine/BlockContext.h:54-57`), XML tags `LayerRoll ... BaySickRustyDrumsRoll` (`PatternManager.cpp:1337-1397`), `sched()` fan-out (`PluginProcessor.cpp:3308-3404`) with the Plugins family gating on the engine pointer (`:3396-3399`). PR-target bases are summed from page caps in order (`BaySickConstants.h:26-39`); changing any cap invalidates saved projects' pending note-offs.

**Automation lanes** - survive: `registerStaticAutomationHandlers` (mixer strip params, sends, `global_tempo`, `_fader` alias; `StandaloneEditor.cpp:17572-17645`), `registerPluginTabAutomation` (`plugtab<N>_vst_<id>`, `:15857-15904`), rack-slot lanes by channel prefix + slot UUID incl. `vst_` (`BuilderPage.cpp:10545-10585`), `evalAutomationLaneAt` and all four replay paths. Go: `registerModelEngineAutomation` (`:15528-15640`, has no Plugins case), sfizz (`:15642-15702`), Solstice mod (`:15704-15791`), pedals (`:15793-15840`), the matching `applyOfflineLaneValue` branches (`BuilderPage.cpp:10383-10441, 10472-10481, 10484-10528`) and the four engine `#include`s (`:5, 21, 22, 23`). `unregisterAutomationForTab` prefixes (`:17530-17554`) and `resolveAutomationDisplayName` (`:4158-4230`) are per-family. Both EQ banks' lanes (`kEqNumBusSlots`/`kEqNumInsertSlots`, `PluginProcessor.h:2083-2089`) go with the strip EQ.

**Project XML** (`PluginProcessor.cpp:6992-7060`; root `<BaySickDAWProject version="1">`, `ProjectManager.cpp:250, 333`):
- `<Processor>` / `<APVTSState>` / `<BaySickRackStates>` - survive; hosted effects live here as `VST3Plugin` slots (`EffectsPage.cpp:900-1003`).
- `<PatternManager>` - `<Mixer>` (`MixerState`, family-keyed), `<Rolls>` (family tags), `<AudioLibrary Entry pageOwnerChannelId>` reference audio-insert ids.
- `<DenoiseProfiles>` (`:7028`) - Vocal only. `<DrumTriggers>` (`:7052`) - drums only. `<MidiCCMappings>` (`:7049`) - survives (project-only since 2026-08-24, `MidiLearnRegistry.h:32-40`). `<DirectToMaster>` (`:8391`) - survives.
- `<UIState>`: `<Tab>` records with `type=` strings + freeze attributes; strip names/orders `AuxNames/VoxNames/InstNames` (`StandaloneEditor.cpp:15306-15314`); `<Buses>` active + ever-routed flags (`:15340, :19022`); `drumKitBank`; `<PianoRollSelection kind index>` with an exhaustive `EngineKind` name switch (`:15008-15021`); `<Windows>` keyed per page.
- A hosted-plugins-only project today: `<Tab type="Plugins" pageIndex name engine engineData locked>` per instrument, `<PluginPageRoll page=N>` notes, `plugtab<N>_vst_<id>` lanes, rack effects under `<Processor>`, restore order at `StandaloneEditor.cpp:18202-18305` with `stashPluginRestoreDescription` (`:18234`) and `MissingFileReport::add("VST3 instrument", ...)` (`:18249`).
- Persistence freezes: `mixer_*` prefixes + 0..999 ids (`BaySickGraph.h:200-208`); `TabKind`/`TabType`/`EffectType`/`clipType`/`CurveType` ordinals append-only (`EngineRig.h:43-48`; `RibbonTabBar.h:22-23`; `EffectRack.h:21-27`; `PatternManager.h:342-350, 21-28`).
- `ProjectFileResolver` (`ProjectFileResolver.h:34-75`), `MissingFileReport` (`MissingFileReport.h:24-60`), `UndoActions.h` (all seven action types), `TransactionTracker` dirty logic - survive unchanged.

**Recording** - audio: `_arm` exists only on Vox/Inst strips (`PluginProcessor.cpp:8979-8983`); `startRecording` scans those two families (`:6694-6697`); no strips armed -> master capture to `<project>/Exports/`, not auto-dropped (`StandaloneEditor.cpp:20892-20918`); per-strip takes -> `<project>/Samples/` + `ClipType::Audio` block with `routeChannel` = originating insert (`:20784-20887`); only `routeChannel == 0` spawns a new audio row/strip (`:20876-20889`). MIDI: `LastRollKind { Layer, Bass, Drums }` (`StandaloneEditor.h:1081`), refusal dialog (`:1134-1148`), commit into `layerRoll/bassRoll/drumRolls` (`:21110-21215`). There is no MIDI-record path to Clips, Vox, Inst, Plugins or Rusty rolls today; with the built-ins gone, MIDI recording has no destination.

**Export** - `runOfflineLoop` (`BuilderPage.cpp:9278`) and its three consumers survive; `beginOfflineRender` (`PluginProcessor.cpp:7939-8039`) needs the graph + engine registry only, except the Vocal cast (`:7978`) and two sfizz arrays (`:7981-7986`); `mVibeGraph.setAllRackSlotsNonRealtime` (`:7987-7989`) is what reaches hosted effects; `enterOfflineRender`/`leaveOfflineRender` must run on the message thread because they activate/deactivate hosted VST3s (`BuilderPage.cpp:9251`; `PluginProcessor.cpp:8000-8012`). Stems: `getStemPickEntries` feeds both the "+" menus and the stems dialog (`MixerPage.h:146-152`).

**Freeze** - tap is pre-rack (`graph.armFreezeTap`, `BuilderPage.cpp:9894`; stale guard `:9950-9962`); state on `EngineTab` (`EngineRig.h:83-212`); files `<project>/Freeze/tab_<kindName>_<idx>_song.wav` / `_pat<N>.wav` with the kind spelled as a name (`PluginProcessor.cpp:4428-4452`); `insertKindForTab`/`renderTaskForTab` per `TabKind` (`:4372-4422`); Clips freeze covers both engine trigger and grid decode (`mAudioRenderTasks`); Rusty 13-task path (`BuilderPage.cpp:10029-10040`; `EngineRig.cpp:276`) goes; staleness axes (`EngineRig.h:248-256`) and the hosted-plugin listener path (`EngineRig.cpp:387-410`) survive; rack/EQ/fader/send changes do not invalidate (downstream of the tap).

---

## 9. Build / packaging

**Targets** (`CMakeLists.txt`, 923 lines, one file): `BaySickDAWStandalone` (`:504-508`, `PRODUCT_NAME "BaySickDAW"`, `BUNDLE_ID com.knowledgebasestudios.baysickdaw`); `BaySickDAW` legacy `juce_add_plugin` (`:463-475`, "NOT shipped ... Do not distribute", never built by `do_build.bat`); `BaySickPluginHost` x64 (`:862-891`, one source file, links only `juce_audio_processors`/`juce_audio_utils`/`juce_gui_basics`, no vendored libs - load-bearing `:853-857`); x86 helper as a separate CMake project (`Source/Hosting/Helper/CMakeLists.txt:23-68`, `-A Win32`, `build32`, `do_build.bat:252-273`); `BaySickEqTests` (`:913-923`, dies with the EQ); `BaySickDAWAssets` (`:32-39`, 3 PNGs, one is Rusty art); `fontaudio` module (`:62`). Helper is found by filename via `PRODUCT_NAME` and `CMAKE_SIZEOF_VOID_P` suffix (`:47-51, 858-861`).

**The CMake cut** - there is no shell/engine split in CMake: `VIBESYNTH_DSP_SOURCES` (`:157-233`) mixes Solstice/Player/Synth/Bass (`:179-204`) with `PluginProcessor.cpp`, `BaySickGraph.cpp`, `ProjectManager.cpp`, `PatternManager.cpp`, `Hosting/*`, `Engine/*`; the standalone's `target_sources` is a flat inline block (`:512-681`) interleaving pages, every `Source/DSP/*` effect, `EffectRack.cpp`, effect panels, and every remaining engine; include dirs are one line naming every engine folder (`:703`). No `option()`, no per-family variable. Two of six `BAYSICK_HAS_*` gates are dead: `BAYSICK_HAS_SFIZZ` and `BAYSICK_HAS_NAM_CORE` have zero `#if` tests in `Source/` (sfizz/NAM headers are included unconditionally by the engine headers, `BaySickGraph.h`, `EngineRig.h`, `Engine/Tasks/*`, `SafeNamModel.h`), so removing those libs before the engines is a compile error. Post-build staging: `Resources/` (`:814-820`; keep `Filmstrips/`, drop `Acoustic IRs/` + `Tape/`), `Manuals/` (`:828-834`), `WebView2Loader.dll` (`:841-845`), helper into both configs (`:898-907`). `do_build.bat` gate strings `RELEASE_EXIT_CODE`, `DEBUG_EXIT_CODE`, `HELPER64_EXIT_CODE`, `HELPER32_CONFIG_EXIT_CODE`, `HELPER32_EXIT_CODE`, `ARTEFACTS_EXIT_CODE` (`do_build.bat:12-19, 289`) are grepped downstream.

**Vendored libs the shell keeps**: JUCE 8.0.12 (`juce_audio_utils`, `juce_dsp`; `juce_cryptography` only for Core Library SHA-256, `:238-240`); VST3 SDK via JUCE (`JUCE_PLUGINHOST_VST3=1` `:127`; `JUCE_PLUGINHOST_VST` off `:123-126`); ASIO - `libs/asiosdk` is a presence gate only (`:135-154`), `JUCE_ASIO_USE_EXTERNAL_SDK` never set, so JUCE's bundled `native/asio/` headers are what compile; concurrentqueue (`:492, 710`, MT engine); WebView2 (`:683-695`, manuals only); fontaudio (`:718`; shot harness hard-fails without it); LAME (`:414-443`) for MP3 export (`DSP/Mp3Writer.cpp:5`) **and** import (`MpglibAudioFormat.h:4, 57, 198`; JUCE MP3 + WindowsMedia off at `:85, :97`). Drop: sfizz, NeuralAmpModelerCore, WORLD, Rubber Band, Signalsmith Stretch + Linear.

**Every path / name / identifier to rename**

| Item | Value | Where |
|---|---|---|
| User-data root | `<Documents>\BaySickDAW` | `AppPaths.h:10-14` (single authority) |
| `settings.xml`, `audio_settings.xml`, `master_output.xml`, `ui_prefs.xml` (`applicationName="BaySickDAW", folderName="BaySickDAW"`), `keymap.xml`, `plugins.xml` | | `ProjectManager.cpp:28`; `StandaloneApp.cpp:428-443`; `StandaloneEditor.cpp:20334-20342`; `KeyBindings.cpp:869`; `Hosting/PluginManager.cpp:12-20` |
| Project XML root tag | `BaySickDAWProject` | `ProjectManager.cpp:250, 333` |
| Project layout | `Projects\<name>\project.xml` + `Samples\`, `Backups\`, `Backups\Unsaved\`, `Exports\`, `Freeze\` | `ProjectManager.h:19-31`, `.cpp:132-135, 197-198, 312`; `PluginProcessor.cpp:4428-4452` |
| Presets / Templates / Kits / My Samples / Core Library / `Sample Library.lnk` -> `"BaySickDAW Core Library"` | | `ProjectManager.cpp:435`; `SampleLibrary.cpp:24-39, 62`; NSI `:290-305` |
| Sample path tokens | `library:` / `mysamples:` | `ProjectManager.h:39-43` |
| Manual at run time / shot output / docs JSON | `<exe>\Manuals\manual.html`; `<Documents>\BaySickDAW\Manuals\shots-staging`; `...\Manuals\assets\bsd-docs.json` | `ManualsWindow.cpp:44-50`; `ShotHarness.cpp run()` |
| Window/product strings | `DocumentWindow("BaySickDAW")` `StandaloneApp.cpp:23`; title composition `StandaloneEditor.cpp:21290-21309`; splash `StandaloneApp.cpp:709`; labels `:1486, :1969`; About box "BaySickDAW v1.0" / "Built with JUCE 7" (`:12037-12047`, menu `:11874`, stale vs `project()` 1.2.0 and JUCE 8.0.12) | |
| CMake identity | `project(BaySickDAW VERSION ...)` `:2`; `PLUGIN_MANUFACTURER_CODE Kbst`, `PLUGIN_CODE Bsdw`, `COMPANY_NAME "KnowledgeBase Studios"` `:468-474`; JUCE-missing message says 7.0.12 `:25` | |
| Helper exe names | `BaySickPluginHost64.exe` / `BaySickPluginHost32.exe`, resolved beside the running exe | `SandboxedPluginClient.cpp:102-112`; `.nsi:20, 71-72`; `make_installer.bat:75-76` |
| Bridge handshake id | `"BaySickPluginBridge"`, protocol version 6 | `PluginBridgeProtocol.h:62, 67` |
| Installer | `APP_NAME/APP_EXE/APP_PUBLISHER` `.nsi:68-70`; reg keys `HKCU\Software\KnowledgeBase Studios\BaySickDAW` `:76`, Uninstall `:77`; install dir `$LOCALAPPDATA\Programs\BaySickDAW` `:52`; source dir `:88`; output `BaySickDAW-<ver>-<stamp>-Tester-Setup.exe` (`make_installer.bat:109`); **no GUID anywhere**; required inputs include `Presets\*`, `Templates\*`, `Kits\Factory\*` (`:104-111`) |
| Core Library endpoint | `https://github.com/KnowledgeBaseStudios/BaySickDAW-Downloads/releases/download/Content-v1/` + 10 SHA-256'd assets (~4.04 GB) | `CoreLibraryInstaller.h:62-64, 106-127` (deleted) |
| Undo owner tags / lane prefixes | `rig:<kind>:<page>` (`EngineRig.cpp:545-546`); `lay_`, `bas_`, `drm_`, `clip_<n>_` (`:64-84`); `plugtab<N>_vst_`, `mixer_*` | |
| File extensions | none custom - everything is `.xml`, bundles `.zip` or folder (`ProjectBundler.h:13`) | |
| APVTS globals | `master_fx_bypass`, `master_pan_law`, `global_tempo` | `BaySickGraph.cpp:410-411` |

**Legal prerequisites for a closed-source shell** (current repo `LICENSE` is GPL v3; `THIRD_PARTY_LICENSES.md:21-25` covers only the four pitch engines and says a full manifest is a pre-release deliverable):
1. **JUCE 8 is AGPLv3 or commercial** (`juce/LICENSE.md:4-8`); closed source needs the paid licence for all four binaries; `JUCE_DISPLAY_SPLASH_SCREEN=0` (`:68`) is only legitimate under it.
2. **ASIO SDK** is Steinberg proprietary or GPLv3 (`libs/asiosdk/common/LICENSE.txt:15-26`; identical text in JUCE's bundled copy); closed source requires the signed Steinberg agreement before publishing; the shipped headers are JUCE's copy.
3. **LAME 3.100 LGPL, statically linked** (`:420, :762`); the relink provision applies (source or written offer + relinkable objects); the About-box disclosure (`StandaloneEditor.cpp:12040-12047`) does not satisfy it; dropping LAME drops MP3 import too.
4. Notice-only: VST3 SDK MIT (`VST3_SDK/LICENSE.txt:2-22`) plus the separate `VST3_Usage_Guidelines.pdf` trademark terms (the report infers a signed Steinberg VST 3 agreement for name/logo use; not verified); WebView2 BSD-style (`libs/webview2/LICENSE.txt`, `NOTICE.txt` incl. an LGPL carve-out) and the redistributed `WebView2Loader.dll`; fontaudio - CMake claims OFL 1.1 + MIT + CC BY 4.0 (`:56-61`) but only the MIT text is in-tree, and the font binary is compiled into the exe; concurrentqueue (`README.txt` not read); JUCE's bundled deps enumerated at `juce/LICENSE.md:36-56`.
5. Falls away with the engines: sfizz (BSD-2), NAM core + Eigen + nlohmann/json, **Rubber Band R3 (GPL v2+, the other copyleft blocker)**, WORLD, Signalsmith, LunaSVG.
6. The tester NSI states it has no EULA page, no third-party notice review, no code signature (`.nsi:1-11`); the About "Powered by" list is marked INCOMPLETE (`StandaloneEditor.cpp:12034-12037`); no `NOTICES` file is staged beside the exe anywhere in CMake.

**Manual pipeline** - 24 of 43 figure groups are engine-free; `mixer` (`ShotHarness.cpp:1012-1043`) seeds itself with `TabKind::Bass` + `"BaySickBass"` + `addBassChannel/addVoxChannelAtIndex/addInstChannelAtIndex`; `eq` (`:1392-1425`) dies with the EQ; figure order carries state (`Manual Pipeline.md:50-54`; `editor menus` last, `:1960-1965`); Callout Registry: Shell 35 / Instrument 40 / Mixing & Effects 16 figures, prose mirrored in `Manuals/src-m2/{shell,instrument,mixing-effects}`; 137 PNGs in `Manuals/figures/`, 88 of 90 automated, `Main frame.png` + `Hosted Plugin.png` hand captures; `MIXADD` is a grouped-callout menu figure whose dot count breaks (`DOT MISMATCH`) when the menu grows; `ANLZM`, `MIXADD`, `PRC` are the surviving grouped exceptions.

---

## 10. Contradictions or uncertainties between the reports

1. **`MixerPage.cpp` engine coupling - resolved.** Tabs report: "appears in none of the built-in-symbol grep results". Build report: "8 lines mention a built-in engine". Both are right: grep of the eleven engine class names in `Source/Standalone/MixerPage.cpp` returns exactly 8 hits, all comments (`:1489, :2595, :2663, :2664, :2665, :3242, :4131, :4375`); zero code references.
2. **`addLiveInputParams` line range - resolved.** Mixer report `~9921-9955`, sequencer report `9921-9946`, live-input report `9926-9976`. The function opens at `Source/PluginProcessor.cpp:9926`; params at `:9929` (`_inputChannelIdx`), `:9939` (`_listen`), `:9947` (`_inputChannelStereo`), `:9961/:9972` (`_monitorMode`, two prefix-gated arms). The live-input report is the accurate one.
3. **`EngineConnection` vs `PianoRollConnection`.** `CLAUDE.md` names `EngineConnection::auditionMomentary`; the sequencer report says no such type exists. Grep confirms: the only definition is `struct PianoRollConnection` at `Source/Standalone/PianoRollPage.h:53`. CLAUDE.md's name is stale.
4. **Deleted-line accounting.** The build report puts all 36,347 lines of `Source/DSP/` and all 15,440 of effect UI in the "engines + DSP" (deleted) bucket, yet the sequencer report lists `DSP/AudioClipStreamer.*` and `DSP/PhaseVocoder.*` as surviving, the build report itself says `DSP/Mp3Writer.cpp` is needed for MP3, `DSPBase.h` is the rack-slot base type, and the hosting report shows `EffectsPage`/`SlotComponent`/`EffectWindows`/`EffectEditorPanels` carry the VST3 rack path. The 114,322 / 119,847 split therefore overstates the deletable total by an amount no report measured. `EffectRack.cpp/.h` (1,401) is likewise counted engine-side but stays.
5. **Coupling metrics are not comparable.** Tabs report "sites" (class refs + strings + `EffectType::`) vs build report "lines" (`grep -c` of class names, comments included): `StandaloneEditor.cpp` 120 vs 406, `RibbonTabBar.cpp` 29 vs 47, `PluginProcessor.cpp` 24 vs 76, `EngineRig.cpp` 30 vs 29. Neither counts `dynamic_cast` on page classes, which the tabs report tallies separately (DrumPage 38 ... PluginsPage 19).
6. **`Source/Inst/`, `Source/Clips/`, `Source/Vox/` (3,711 lines)** are counted as shell by the build report and as engine-bound pages by the tabs and sequencer reports. `InstPage` also owns the `EngineChainProcessor` wrapper (Standalone, 82+79) that exists only to chain Pedals + NAM/IR.
7. **Two channel-id numberings.** The mixer report documents `MixerChannelIds` (aux 100+, layer 200+, ... plugin 900+, direct 950+; `BaySickGraph.h:33-93`); the sequencer report documents a second lane-id / Effects-dropdown space (drums 100+, layers 200+, ... aux 600+, plugin 1000+, direct 1100+; `EffectsPage.cpp:1483-1525`, walked in `BuilderPage.cpp:10589-10603`); the mixer report cites `channelToMixerId` at `EffectsPage.cpp:353-366`. Whether `:353-366` and `:1483-1525` are the same mapping or two was not determined.
8. **Recording line ranges.** Sequencer report `startRecording` `:6628-6696`; live-input report `:6591-6727`. Overlapping, not contradictory; the live-input range is the whole function.
9. **`Plugins Page.md` vs code** (hosting report): the doc says no Rename/Duplicate, one plugin per tab for life, and three user-resize behaviours; the code has Rename / Replace Plugin / Duplicate (`PluginsPage.cpp:608-643, 741-750`) and `setUserResizable(false)` on both surfaces (`PluginsPage.cpp:270`; `EffectWindows.cpp:145`). The tabs report's menu table matches the code.
10. **`MIDI Learn.md:117`** says mappings also save to `Documents\BaySickDAW\MidiMappings.xml` as global defaults; code header records a 2026-08-24 ruling to project-only (`MidiLearnRegistry.h:32-40`); two stale comments still name the file (`PluginProcessor.h:676`; `MidiLearnUI.h:116`).
11. **Memory file `reference_mixer_strip_pattern_audit.md`** says `VibeGraph.h` and "8 per-kind maps"; the file is `BaySickGraph.h` and the maps were flattened into `mInsertsByChannel` + `mLiveInsertChannels` (`BaySickGraph.h:1064-1080`). The mixer report counts 38 concrete sites across 7 files against the memory's "~15".
12. **Not determined by any report:** whether `EngineRig::recreateEngine` / `retryDeadPluginTab` (`EngineRig.cpp:417-534`) depend on non-Plugins arms (shield nesting past `:810` unread); whether the `LibraryPitchShifters`/`PitchCorrectorDSP` `BAYSICK_HAS_*` gated files are reached by the clip stretch path or only by Vocal; the contents of `libs/concurrentqueue/README.txt`; whether the 90 `Manuals/src-m3` files vs 65 declared implementation topics are retired leftovers (inferred, not verified); whether the `VST3_Usage_Guidelines.pdf` actually requires a signed agreement (inferred); whether a partial non-ASIO input mask offsets `chIdx` vs the picker list (inferred, `StandaloneApp.cpp:932-953`); the bridged 32-bit relay path has never been exercised with a real 32-bit VST3 (`Main Plan.md:1998`).
13. **Hosted-effect `producesMidi`/`acceptsMidi`.** Hosting report: `acceptsMidi = mDesc.isInstrument` so a hosted effect never receives MIDI, while bridged loads report the plugin's real `acceptsMidi` and ignore it (`HostedPlugin.cpp:119-137`, `ignoreUnused(midi)` at `:121`). No other report touches this; recorded here as a single-source fact.

---

# PART 2 - second-round merge

) `enginePlayHead()` propagation must reach every insert's hosted instrument/effect under the bus model, not per-tab engines (`PluginProcessor.h:449-458`; today's single choke point `EngineRig.cpp:685`); (3) `onGetLoopBeats :1280-1368` pattern-mode branch re-sourced once "the piano roll the user was last editing" is bus-derived (inferred); (4) record arm `PluginProcessor.h:1386, :1447-1466` becomes "any insert with live audio input armed"; (5) `setSongMode` snapshot `PluginProcessor.cpp:9207-9223` survives only with stable insert/bus APVTS ids; (6) pattern-mode TS reads (`PluginProcessor.cpp:4114-4119`, `StandaloneEditor.cpp:1267-1279`) unchanged while `PatternManager` stays. Observed asymmetry: `TsMap` unbounded spin (section 10 item 19).

### 11.2 Settings stores

All paths resolve through `AppPaths::appRoot()` = `Documents\BaySickDAW\` (`AppPaths.h:10-15`); `ProjectManager::getSettingsFile()` = `<appRoot>/settings.xml` (`ProjectManager.cpp:18-29`). Root tag `BaySickDAWSettings` (`:720`). Every writer is read-modify-write, removing only its own child. **Six independent writers** (plus `<TransportDisplay>` from the editor):

| Writer | Section / keys | Lines | Fork |
|---|---|---|---|
| `ProjectManager` (`loadSettings :685-708`, `saveSettings :710-751`; ctor call `:84`; write-failure alert once per session `:742-750` via `mSettingsWarnShown`) | `<RecentProjects><Project path>` max 10 (`:681`, `:722-728`) | | KEEP |
| | `shortcutCreated` `:729` (one-shot `Sample Library.lnk` `:451-462`) | | DIES |
| | `migratedFromRoaming` `:730` (moves `audio_settings.xml` `:427-430` + built-in `Presets/` `:432-443`) | | keep-or-drop |
| | `skipGlobalLockPromptBank0/Bank1` `:731-732`; `skipKitReplacePrompt` `:733`; `skipCoreContentPrompt` `:734` | | DIE |
| | `defaultTemplate` `:735-738` (consumed `StandaloneEditor.cpp:11703-11713`, `:11913-11920`) | | KEEP |
| `StandaloneApp` | `<MultiCoreRendering on>` -> `RenderEngine::gMultiThreadedEngineEnabled` (`load :476-495`, store `:493`, `save :497-520`; decl `StandaloneApp.h:216-217`; Mixer hamburger toggle `StandaloneEditor.cpp:7738-7739`) | | KEEP |
| | `<MidiTriggerVelocity fixed>` -> `DrumTriggerVelocity::gUseFixed` (`load :526-537`, store `:535`, `save :539-558`; decl `.h:218-220`) | | see weld below |
| `PatternColorPicker` (file-local `getSettingsFile()` `:10`) | `<RecentPatternColors><Color argb>` (`loadRecents :115-135`, `saveRecents :137-162`, `pushRecent :164+`) | | KEEP |
| `WorkspaceWindow` | `<WorkspaceWindows><W key,w,h[,x,y][,rx,ry,rw,rh]>` (`kRootTag`/`kWindowTag` `:18-19`; write `:1345-1372`; read `loadSavedBounds :1249-1261`, restore-rect seed `:888-900`); written once at exit by `writeSessionToSettings()` `:1310-1381` from `shutdown` `StandaloneApp.cpp:1321` after `flushWindowBoundsNow()` `:1296-1298`; root-tag match requirement `:1316-1319` | | KEEP |
| `SharedUI` / `LufsReadoutBox` | `<MasterLufsMode mode>` (read `SharedUI.cpp:3080-3085`, write `applyMode(..., persist=true)` `:3109-3128`) | | KEEP |
| `BaySickNAMIREditor` | `<RecentNAMFiles>` / `<RecentIRFiles>` (`:1388-1426`, path `:1385`, doc `.h:64`; wrong root tag when creating, `:1412`) | | DIES |
| `StandaloneEditor` | `<TransportDisplay showTime>` (`load :12169-12177` called `:1385`; `save :12179-12196`; wired `mPosReadout->onDisplayModeChanged :1384`; doc `Transport and Playback.md:204`) | | KEEP |

Load ordering the fork must preserve: `loadMultiCoreRenderingPref()` / `loadMidiTriggerVelocityPref()` at `StandaloneApp.cpp:803/807` before `mDeviceManager->initialise` (`:798-807`); `loadMasterOutputRouting()` `:1127`; `writeSessionToSettings()` `:1321` after `clearContentComponent()` `:1313` (`:1316-1320`). `runFirstLaunchHousekeeping()` is called from `StandaloneEditor.cpp:816` and unconditionally ends in `offerCoreContentDownload(false)` (`ProjectManager.cpp:466`).

**Sibling per-machine files:** `audio_settings.xml` (`StandaloneApp.h:198`; `saveAudioSettings` `StandaloneApp.cpp:560+`, empty-name guard `:567-589`); `audio_settings_pending.xml` (section 7); `master_output.xml` (`StandaloneApp.h:200-205`, `StandaloneApp.cpp:445-467`); `keymap.xml` (`KeyBindings.cpp:864-870`, `saveMappings :872+`, `UserFileSave::showWriteFailure :878+`); `plugins.xml` + `plugins_scan_crashes.txt` (`PluginManager.cpp:19-32`); `ui_prefs.xml`.

**`ui_prefs.xml`** - opened by a deliberately duplicated file-local `openUiPrefs()` (`StandaloneEditor.cpp:20333-20342`, comment `:20326-20332`; `BaySickPitchEditor.cpp:65-74`; only hits `BaySickPitchEditor.cpp:73`, `StandaloneEditor.cpp:20341`, comment `PluginManager.cpp:13`). Keys: `fsWriteDry/DryCleaned/Wet/WetCleaned` (write `:20495-20498`, read `:20540-20553`, consumed `:20938-20960`) DIE; `fsDenoiseStrength` (`:20499-20500`, `:20551`) DIES; `fsAutoFreezeCpu` (`:20503`; `:19537`, `:19663`; 101 = Off `:20385`) KEEP; `fsCaptureRetain` (`:20504`; `:2029`, `:20574`) KEEP; `fsCaptureAudio` (`:20505`; `:2026`, `:20573`) KEEP; `fsInstrumentFreeze` (`:20506`; `:7878-7881`, reason `:7870-7877`) KEEP; `exSpecId` (`:13942`; `:13684`, `:19646`) KEEP; `exSpecCustom` (`:13943`; `:19647`) KEEP; `pitchMultiResetNoPrompt` (`BaySickPitchEditor.cpp:2821`, `:2793`) / `pitchWorldOfflineNoPrompt` (`:2857`, `:2832`) DIE. With Vocal gone the duplicate opener disappears by itself (inferred).

**File Settings dialog** - `FileSettingsComp` `StandaloneEditor.cpp:20346-20537` (hoisted for the shot harness `:20344-20345`; factory `:20555-20558`, `ShotFactories.h:17`, `ShotHarness.cpp:754`); launcher `showFileSettingsDialog :20560-20584` (Options 502, `:12003-12005`). Widgets in `resized :20510-20536` order: four take-type toggles `:20399-20406` with >=1 interlock `onBoxToggled :20485-20491` (DIE); de-noise combo `:20409-20414` (DIES); `FreezeSlider` `:20356-20364`, `:20424-20441` + `freezeText :20387-20391` (KEEP); captured-takes combo `:20444-20451` (KEEP); keep-audio toggle `:20457-20464` (KEEP); instrument-level freeze `:20471-20480` (KEEP); `note` label `:20415-20417` (text is all take types, DIES). `save() :20492-20509` writes all nine keys then fires `onCaptureSettingsChanged` (`:20570-20579`, pushes capture settings live + `wireFreezeSlotForVisiblePage()`). `setSize(400, 416)` `:20482`.

**Options menu** (`getMenuForIndex` case 4 `:11827-11860`; bar names `:11658-11661`): 530/531 Set/Clear Default Template (`:11917-11920`) KEEP; 502 File Settings (`:12003-12005`) KEEP shrunk; 503 Audio Settings (`:12008-12020`) KEEP; 504 Plugins (`:12023-12025`) KEEP; **505 Get Sound Content** (`:12031-12034`) DIES; 510-513 Undo History Size (`:11993-12000`, submenu `:11845-11857`) KEEP, not persisted; 520 "MIDI is Omni (all devices) - Read Only" disabled row (`:11859`) KEEP.

**`DrumTriggerVelocity` weld** - section 3b "Settings / prefs welded to deleted subsystems". After the fork the pref, its `settings.xml` key, its two `StandaloneApp` functions and its dialog row would persist a value nothing reads unless a hosted-instrument meaning is given to it; the namespace is a free atomic + constant with no dependency on the `DrumTriggerMap` class.

**Other:** `AudioSettingsDialog` is defined inside `StandaloneEditor.cpp` (`:111-...`), not its own file (inferred: splitting it out is the only way to keep it while shrinking the editor). `UserFileSave::showWriteFailure` is used by keymap (`KeyBindings.cpp:878+`); `ProjectManager::saveSettings` uses its own inline alert (`:742-750`).

### 11.3 Keybindings / command catalog / typing keyboard

**Catalog** (`KeyBindings.cpp:26-243`): 37 commands - 36 `Category::General`, 1 `Category::Builder` (`cmdToggleSlipStretchMode` `:233-236`). Reference rows (`:246-807`): General 2, Builder 43, PianoRoll 54, DrumKit 36 (`:578-694`, incl. five "(Piano Roll only)" rows `:680-694`), VocalEditors 24 (`:698-770`; BaySickPitch `:698-759`, BaySickAlign `:762-770`), EventEditor 11. `Category::MouseReference = 4` dead (`KeyBindings.h:99`, `KeyBindings.cpp:16`). `findHardcodedConflicts` `:846-861`.

**Wiring:** `registerAllCommandsForTarget(this)`, `setFirstCommandTarget(this)` (pages live in parentless desktop windows), `resetToDefaultMappings()` then `BSCommands::loadMappings`, `addKeyListener(set)` `StandaloneEditor.cpp:1980-1996`; typing-note gate registered after the mapping set `:1998-2005` (reverse-order dispatch); repeated per contained window `:16084-16087`, aux window `:16150-16153`, Undo History `:12502-12503`, inverted for Event Editor `:3795-3801`. Key Binds window: Help id 603 (`:11866`, `:12055-12056`, `showKeyBindsWindow :10061-10068`), 880x680, self-deleting (`KeyBindsWindow.cpp:476-495`); table/Set/Reset/capture modal/conflict prompts `KeyBindsWindow.cpp:19-429`. `keymap.xml` stores diffs only (`createXml(true)` `KeyBindings.cpp:878`); an unknown id on restore is silently dropped (`juce_KeyPressMappingSet.cpp:222-245` -> `addKeyPress` only when `getCommandForID != nullptr`), so a user's F7/F11 rebind vanishes rather than crashes.

**Engine-bound entries:** `cmdShowPlayer` `0x10017` F7 and `cmdShowDrumKit` `0x10019` F11 (section 2, section 5 table); `Category::DrumKit` + `Category::VocalEditors` (section 2); surviving rows with engine text `KeyBindings.cpp:63-66` (neutral), `:73-76`, `:385-399`, `:423-425` (Piano Roll `S` = RP Slide / RT Slide / Portamento, a built-in-synth note model).

**Not engine-bound:** `cmdShowBuilder` F5 `:9680-9682`, `cmdShowMixer` F6 `:9683-9685`, `cmdShowEffectsRack` `:9689-9695`, `cmdShowEffectPanel` F9 `:9696-9700`, `cmdShowEventEditor` F12 `:9707-9710`, `cmdShowManuals` F1 `:9711-9713`, file ops `:9716-9720`, pattern nav/list `:9723-9733`, transport `:9736-9762`, undo/redo `:9770-9771`, precount `:9774-9776`, Slip/Stretch `:9782-9786`; View 404/405/406/407/409/411 (`:11817-11824`, `:11980-11987`).

**Typing keyboard:** `TypingKeyboardMap.h` - `gActive :16`, `semitoneForKey :21-45`, `isOctaveShiftKey :47-51`, `shouldBypassLocalKeys :56-63`. Editor state `StandaloneEditor.h:895-908` (`mTypingKeyboardOn`, `mTypingOctaveOffset` -5..+3, `mTypingHeldNotes`, `mPluginAuditionHeldNote`); `toggleTypingKeyboard :12198-12204`; `sendTypingNote :12206-12215` (`noteOn(ch1, note, 0.8f)`, self-timestamped); `releaseAllTypingNotes :12217-12234` (public, `StandaloneEditor.h:929-933`; called `:6755`, `StandaloneApp.cpp:46`); `keyPressed :12236-12266`; `keyStateChanged :12268-12284`. Entry points: Ctrl+T `cmdToggleTypingKeyboard 0x10071` (`KeyBindings.h:88`, `KeyBindings.cpp:239-242`, `:9765-9767`) and `KeyboardMidiButton` (`GlobalTransportBar.cpp:341-353`, click `:431`, LED `:970-977`, wired `StandaloneEditor.cpp:1392`). Destination chain in section 3b; dependency: hard on the tab model, on built-ins only for dead branches, on the mixer not at all.

### 11.4 Undo system

**Machinery (no engine deps):** one `UndoManager` on the processor (`PluginProcessor.h:287-289`), editor reference `StandaloneEditor.h:846`, ctor init `StandaloneEditor.cpp:747`; `setMaxNumberOfStoredUnits(1, N)` `:766`; `ChangeListener` `:773`, callback `:12351-12403`; `doUndoAction :12289-12310`; `globalUndo/globalRedo :12312-12349` (open `MissingFileReport::ScopedGesture("restored tab")`); `rebuildHistoryLabels :12405-12416`; `historyDisplayFor :12418-12435`; `UndoHistoryWindow.h:15-86`, `.cpp:24-47, 88-105`, created lazily `:12485-12507`; `TransactionTracker` `PluginProcessor.h:304-321` fed from `:12364-12391`; load boundary `ProjectManager.cpp:123-125`, startup sweep `StandaloneEditor.cpp:777`, teardown `:2126-2128`; `cmdGlobalUndo/Redo 0x10050/0x10051` `KeyBindings.h:73-74`, `KeyBindings.cpp:212-217`, menu 201/202/203 `:11780-11783`.

**14 action classes** (`UndoActions.h`): `PianoRollEditAction :51-88` (`PianoRoll.cpp:703`); `PitchEditAction :95-132` (Vocal-only); `ArrangementEditAction :137-171` (`BuilderPage.cpp:3912`); `MixerStateAction :176-199` (8 sites, section 4); `FloatParamAction :205-226` (`EffectEditorPanels.cpp:165` only); `EffectRackAction :236-290` (`EffectsPage.cpp:1335, 1382, 1403`; `EffectType` + `kNumSlots`); `AutomationLaneEditAction :296-323` (`EventEditor.cpp:151, 1731, 1960, 2060`); `StructuralOpAction :334-370` (owned snapshot files deleted in dtor `:345-349`, skip-first perform `:351-356`); `PatternListAction :392-417` + `PatternListSnapshot :381-390` (`BuilderPage.cpp:1798, 4386, 4452, 4718`; `StandaloneEditor.cpp:21241`); `PatternRenameAction :423-444` (`BuilderPage.cpp:2113`, `StandaloneEditor.cpp:1592`); `PatternColorAction :446-467` (`BuilderPage.cpp:1997`, `StandaloneEditor.cpp:1620`); `MarkerSetAction :484-505` (`BuilderPage.cpp:3943, 4425, 4455, 4723`); `AudioLibraryAction :521-546` (`BuilderPage.cpp:1848, 5727`; `StandaloneEditor.cpp:3059, 3365, 10692, 21247, 21252`); `AutomationTemplateAction :560-586` (`BuilderPage.cpp:1889, 9050`; `StandaloneEditor.cpp:4107`).

**Structural undo:** `UndoSnapshotStore.h` - `dir() :15-18`, `writeNew :27-39` (empty `File` on failure; enforced at `StandaloneEditor.cpp:3436-3440`, `:13076-13082`, `:13119-13124` -> `UserFileSave::kTabNotDeleted`), `sweepAll :41-47`. Sites: add tab `StandaloneEditor.cpp:1818-1838`; delete (record) `:3433-3456`; rename `:5084-5089`; lock/unlock kit `:8564-8567`; Load Kit `:9245-9309`; rename/color pattern `:9902-9905`; Rusty swap `:10181-10207`; duplicate `:12561-12580`; delete with `AudioLibrarySnapshot`/`PatternListSnapshot` `:13060-13112`; delete page-preset path `:13117-13159`; `wrapTabAddUndo :13194-13225`; pages `LayersPage.cpp:962, 1090-1105`, `BassPage.cpp:1004, 1082`, `DrumPage.cpp:837, 864`, `InstPage.cpp:1180, 1203`, `ClipsPage.cpp:123` (`:121-125` bypasses `UndoContext`, calls `mFullProcessor->mUndoManager` directly); `PluginsPage.cpp:504-543` (VST3-generic).

**`ApvtsDirtyTracker`** (`ApvtsDirtyTracker.h:31-81`): `onAny` -> project dirty `:45, 60`; `hasChangedSinceLastBlock()` audio-thread gate `:51-54`, armed `:78`, re-armed `:72-75`. Ten built-in owners (section 3a); `PluginsPage` has none.

**Keep / re-key / drop:** section 2 (drops) and section 5 table (re-keys: `MixerStateAction` payload, `ownerKeyForParamId`, `resolveOwnerPage`, `captureTabRecord`/`resurrectTabFromRecordImpl`, `EffectRackAction` slot descriptor). Keeps: `UndoBracket.h`, `UndoSnapshotStore.h`, `UndoHistoryWindow.*`, `UndoContext`, actions 1, 3, 7-14, the dispatch spine, all seven JUCE patches in 11.5.

### 11.5 Vendored JUCE patches

14 files under `juce/modules/`:

| # | File | Site | Class | Fork |
|---|---|---|---|---|
| 1 | `juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.cpp` | `:39-135, :227-231, :274-279, :331-333, :398, :405, :446-453, :470-488, :558-628, :643-646` | new API + new undo action | KEEP |
| 2 | `.../juce_AudioProcessorValueTreeState.h` | `:406-426, :445-466, :468-478, :480-488` | new public API | KEEP |
| 3 | `.../juce_ParameterAttachments.cpp` | `:71-84` | gesture naming + boundary flush | KEEP |
| 4 | `juce_gui_basics/native/juce_Windowing_windows.cpp` | `:3371-3385` | double-undo fix (contained windows) | KEEP |
| 5 | `juce_gui_basics/menus/juce_PopupMenu.cpp` | `:1492-1499` | left-button-only trigger | KEEP |
| 6 | `juce_events/interprocess/juce_InterprocessConnection.h` | `:135-156` (`setPipeMessageTimeout`) | new API | KEEP |
| 7 | `juce_events/interprocess/juce_ConnectedChildProcess.h` | `:229-247` (`setWorkerPipeTimeout`) | new API | KEEP |
| 8 | `juce_events/interprocess/juce_ConnectedChildProcess.cpp` | `:166-174` | impl | KEEP |
| 9 | `juce_core/files/juce_File.cpp` | `:176-187` | assert suppression (relative `audioFilePath`) | KEEP |
| 10 | `juce_data_structures/values/juce_ValueTree.cpp` | `:777-783` | assert suppression (lazy APVTS); `:776` left active | KEEP |
| 11 | `juce_audio_devices/native/juce_ASIO_windows.cpp` | `:1523-1530` | assert suppression (in/out name); `:1531` left active | KEEP |
| 12 | `juce_gui_basics/menus/juce_MenuBarModel.cpp` | `:78-85` | assert suppression (shared model outliving menu bars) | KEEP or drop (item 12) |
| 13 | `juce_audio_basics/utilities/juce_ADSR.h` | `:117-121` | engine-caused | DROPPABLE |
| 14 | `juce_dsp/processors/juce_StateVariableTPTFilter.cpp` | `:73-78` | engine-caused | DROPPABLE |

**A1 programmatic-write phase** (`.h:445-466`, `.cpp:41, :227-231, :274-279, :331-333`): thread_local phase recorded into `pendingIsProgrammatic` on the writer thread, consumed at flush. 29 call sites; shell keeps 17 (section 8 list); engine sites `BaySickBassesProcessor.cpp:664`, `BaySickGuitarsProcessor.cpp:675`, `BaySickNAMIRProcessor.cpp:933, 1076`, `BaySickRustyDrumsProcessor.cpp:708`, `BaySickVocalProcessor.cpp:1876, 1950, 2198`, `InstPage.cpp:1131`, `BaySickRustyDrumsPage.cpp:526`, `PluginProcessor.cpp:9843, 9864`. Shell-load-bearing independent of engines (MIDI Learn `MidiLearnRegistry.cpp:193`).

**A2 `undoOwnerTag` / `findByUndoOwnerTag` / `ApvtsParamValueUndoAction`** (`.h:468-478`, `.cpp:43-135, :398, :405`): replaces tree-bound `SetPropertyAction`; `liveApvtsInstances()` registry `.cpp:47-51`; dead-owner guard must return `true` (`.cpp:99-100, :577-583` - "a false return makes UndoManager wipe the entire history"). Shell tag `"main"` `PluginProcessor.cpp:563` (`:560-562`). Hosted plugins own no APVTS; resurrection narrows to zero for params if insert params stay on the main APVTS, but the guard hazard remains if dropped.

**A3 `replaceStateKeepingUndoHistory`** (`.h:406-426`, `.cpp:558-628`; stock `replaceState` clears history `.cpp:548-556`, `:555`; `StateSwapAction` `.cpp:566-607`, "Jeff ruling 3a" `.h:418-424`): callers in section 3b; may reach zero live callers post-cull (section 10 item 12).

**A4 lazy registration** (`.cpp:446-453, :470-488`; `juce_ValueTree.cpp:777-783`): the fork-critical patch - section 4 row "Lazy APVTS registration".

**A5 gesture naming + flush** (`juce_ParameterAttachments.cpp:80-84`; `.h:480-488`, `.cpp:62-67`): consumers `UndoBracket.h:17-34` (`:7-9`), `historyDisplayFor :12422` (`:12420-12421`), `StandaloneEditor.cpp:12297, 12322, 12340`, `ClipsPage.cpp:119`. Without the flush two quick gestures land in one transaction (`.h:481-486`). App-side rewrite needed: `ownerKeyForParamId` (section 5), not the JUCE file.

**A6 double-undo** (`juce_Windowing_windows.cpp:3371-3385`): `forwardMessageToParent` guarded by `getOwnerOfWindow(parentH) == nullptr`; cause is contained `WorkspaceWindow`s (`:3372-3381`; `CMakeLists.txt:119-121`). Pure shell.

**B interprocess** (files 6-8): sole consumer `SandboxedPluginClient.cpp:134` (section 6).

**C shell-caused suppressions** (files 9-12): `juce_File.cpp:176-184` (Builder relative paths, section 8); `juce_ValueTree.cpp:777-780`; `juce_ASIO_windows.cpp:1523-1529` + `StandaloneApp.cpp:929-930`; `juce_MenuBarModel.cpp:78-84` (root cause fixed per `Implemented Work Log.md:543`).

**D engine-only** (files 13, 14): source-side fix landed in all four engine constructors (`Implemented Work Log.md:131`); no `juce::Synthesiser`/`ADSR`/`StateVariableTPTFilter` voice construction remains in a hosted-only fork.

---

## 12. Unverified claims

1. §8/§1: "UndoActions.h (all seven action types) survive unchanged" - wrong count and no line evidence; the file defines 14 UndoableAction classes (UndoActions.h:51-560), one of which (PitchEditAction :95) is Vocal-only and one (MixerStateAction :176) wraps the family-keyed MixerState.
2. §1: "AudioSettingsDialog in StandaloneEditor.cpp:137-564" - the class starts at :111; :137 is a comment inside it.
3. §9: "Manual at run time <exe>\Manuals\manual.html ... ManualsWindow.cpp:44-50" - :44 is a closing brace; the resolver manualsIndexFile() is :46-53.
4. §1 shell table rows with no file:line at all: LoudnessReportWriter, MasterAnalyzerWindow, ProjectBrowserWindow, VersionCapture, UndoHistoryWindow, KeyBindsWindow, AudioFileRecorder, MidiRecorder, SafeAudioReader.h, SafeAudioFormats.h, MpglibAudioFormat.h, TempoMapRead.h, TsMapRead.h, WindowChrome, BaySickTitleBar, HeavyOperationOverlay, PatternColorPicker, DSPBase.h ("the rack slot base type"), "one app-wide UndoManager on StandaloneEditor", "ProjectManager (settings.xml)" (path cited at ProjectManager.cpp:28 only; the key set is never enumerated).
5. §2/§10.4: the DSP survivor list (AudioClipStreamer, PhaseVocoder, Mp3Writer, DSPBase, maybe LibraryPitchShifters) has no include-level evidence and is incomplete - shell files also include DSP/LoudnessSpec.h, LufsMeterDSP.h, TruePeakMeter.h, SpectrumFeed.h, PanLaw.h, BpmDetect.h, StripEq.h, EffectParamMap.h and EngineSidechainHelper.h (verified via grep of #include lines in BaySickGraph.*, PluginProcessor.*, BuilderPage.*, EffectsPage.*, EffectWindows.h, MasterAnalyzerWindow.h, VersionCapture.h).
6. §2: "DSP/EffectParamMap (937)" placed in the deleted bucket - but it is called by the surviving rack automation path (EffectsPage.cpp:803-841, BuilderPage.cpp:10583-10584, StandaloneEditor.cpp:15812-15846); no evidence was given that those call sites are built-in-only.
7. §9: "JUCE 8.0.12" as a keep - no file:line, and it omits that juce/modules is locally patched in nine files (see missing list); the licence discussion assumes a stock tree.
8. §9: "fontaudio - CMake claims OFL 1.1 + MIT + CC BY 4.0 (:56-61) but only the MIT text is in-tree" - the in-tree-text half has no path evidence.
9. §9: "no GUID anywhere" (installer) and "no NOTICES file is staged beside the exe anywhere in CMake" - negative grep claims with no cited search scope.
10. §9: "Manual pipeline - 24 of 43 figure groups are engine-free ... 88 of 90 automated, Main frame.png + Hosted Plugin.png hand captures" - only the mixer/eq/editor-menus groups carry ShotHarness.cpp lines; the 43-group list and the 88/90 split are uncited (137 PNGs in Manuals/figures verified).
11. §2: "Vendored libs ... LunaSVG" - no line and no statement of which engine consumes it.
12. §2: "Tests 1,982 Tools/EqTests/main.cpp (BaySickEqTests target)" - target cited as CMakeLists.txt:913-923 but the 1,982 count and "strip-EQ only" scope have no evidence.
13. §4: "Input gain / trim: none (repo grep: only tape_inputGain inside SaturationDSP)" - a grep result, no file:line for the search.
14. §6: "refinement changes createIdentifierString()" and §12 "VST3_Usage_Guidelines.pdf requires a signed agreement", §7 "input-channel index vs picker list may diverge (StandaloneApp.cpp:932-953)" - all self-flagged as inferred; still unverified.
15. §1: "Shared UI assets: Resources/Filmstrips/ (9 PNGs, SharedUI.cpp:10), libs/fontaudio, Assets/BaySickDAWLogo.png, control_tab.png" - only the Filmstrips half has a line.
16. §10.12 "EngineRig::recreateEngine / retryDeadPluginTab (EngineRig.cpp:417-534) ... unread" - confirmed recreateEngine starts at :417; the dependency question remains open.
17. Adjacent stale name (same class of error as §10.3's EngineConnection): CLAUDE.md's "the app's ONE VibeTooltip" - the only tooltip class is BaySickTooltip (SharedUI.h:179); the map repeats neither name but a fork plan copying CLAUDE.md would.
18. §1/§8: "MIDI Learn ... MidiLearnUI.h" - MidiMapView.h (the Help > View Projects MidiMap window) is in the MidiLearn folder line count (1,496) but never named, so its survival is unstated.