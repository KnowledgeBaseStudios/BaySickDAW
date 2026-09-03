# Lite Shell Map - reader reports (2026-09-02)

Companion to `lite-shell-map-2026-09-02.md`.  One section per reader, verbatim.


---

# READER 1: LIVE AUDIO INPUT + MIDI INPUT — grounded facts

I have what I need.

# LIVE AUDIO INPUT + MIDI INPUT — grounded facts

Docs read first: `Plans & Specs/System Reference/Vox Page.md`, `Inst Page.md`, `MIDI Learn.md`, `Transport and Playback.md`, `Mixer.md`. All code claims below re-confirmed in source.

**Path correction for the plan:** the pages are *not* under `Source/Standalone/`. They are `C:\Users\jeffm\Documents\BaySickDAW\Source\Vox\VoxPage.cpp/.h` and `C:\Users\jeffm\Documents\BaySickDAW\Source\Inst\InstPage.cpp/.h`.

---

## 1. How a live-input strip is created and bound to a physical input

### Creation order — strip first, tab second
- `MixerPage::addVoxChannelAtIndex` / `addInstChannelAtIndex` create the strip, then synchronously fire `onVoxStripAdded` / `onInstStripAdded` (`Source\Standalone\MixerPage.cpp:2269`, and the Inst twin at ~2652), which `StandaloneEditor` binds to `spawnVoxTabIfMissing` / `spawnInstTabIfMissing` (`Source\Standalone\StandaloneEditor.cpp:5304-5321`). Every entry point (ribbon "+", Mixer Add menu, project load) funnels here; idempotent on page index.
- APVTS strip params come from `BaySickDAWProcessor::ensureVoxInsert` / `ensureInstInsert` (`Source\PluginProcessor.cpp:9313-9335`), each of which calls `ensureMixerStripParams(...)` then `addLiveInputParams(prefix)`.
- The **render task** is created only when an *engine* is registered: `registerVoxEngine` builds a `VoxStripTask` (`Source\PluginProcessor.cpp:8813-8820`), `registerInstEngine` builds an `InstStripTask` (`Source\PluginProcessor.cpp:8862-8870`). Both ctors take `juce::AudioProcessor* engine` and both `run()` bodies call `mEngine->processBlock(...)` unconditionally on the live path (`VoxStripTask.cpp:296`, `InstStripTask.cpp:346`). **No engine ⇒ no task ⇒ no live input on that channel.**

### What selects the input — per STRIP, not per page
Params created by `BaySickDAWProcessor::addLiveInputParams` (`Source\PluginProcessor.cpp:9926-9976`), prefix `mixer_vox_<n>` / `mixer_inst_<n>`:

| Param | Type/range | Default | Created where |
|---|---|---|---|
| `_inputChannelIdx` | Int -1..127 | -1 | PluginProcessor.cpp:9929 |
| `_inputChannelStereo` | Bool | false | :9947 |
| `_listen` | Bool | false | :9939 |
| `_monitorMode` | Int 0..2 (Vox) | 2 (With Effect) | :9960 — gated `prefix.startsWith("mixer_vox_")` |
| `_monitorMode` | Int 0..1 (Inst) | 1 (With Effect) | :9971 — gated `prefix.startsWith("mixer_inst_")` |
| `_arm` | Bool | false | **not** in `addLiveInputParams` — created in `ensureMixerStripParams` gated on `kind==Insert && (prefix.startsWith("mixer_vox_") \|\| startsWith("mixer_inst_"))` (`PluginProcessor.cpp:8979-8983`) |

Friendly channel name is **not** a param: `setInputChannelName` / `getInputChannelName` write a property on `apvts.state` under `<prefix>_inputChannelName`, guarded by `mInputChannelNamesLock` (`PluginProcessor.cpp:9978-9991`).

**There is no input-gain / trim parameter anywhere.** Grep for `_inputGain|inputTrim|preampGain` across `Source/**` returns only `SaturationDSP`'s internal `tape_inputGain`. The fader is post-chain (see the insert flow below).

### The input picker
- `MixerTrackStrip::setApvts` wires `mArmBtn.onRightClick → onArmRequested(mChannelId)` only when `mType == StripType::Vox || StripType::Inst` and `!mNoLiveInput` (`Source\Standalone\MixerTrackStrip.cpp:361-375`). Left-click is a plain `ButtonAttachment` on `_arm` (`:364`).
- Both strip factories bind `strip->onArmRequested = showInputChannelPicker(chId)` (`MixerPage.cpp:2257` Vox, `:2642` Inst).
- `MixerPage::showInputChannelPicker` (`MixerPage.cpp:2452-2581`): section header branches on `prefix.startsWith("mixer_vox_")`; channel list comes from `getInputChannelNames()` → `mDeviceManager.getCurrentAudioDevice()->getInputChannelNames()` (`StandaloneEditor.cpp:5256-5262`); pair grouping via `computeChannelGroups` (heuristic L/R suffix + a hard-coded Tascam "Model Mixer" 24-ch profile, `MixerPage.cpp:2296-2321`). Item IDs: 100+idx mono, 200+idx stereo, 300..303 Vox grid-default, 99 disarm. **Picking a channel does not arm** (`:2558-2561`).
- `MixerTrackStrip::setNoLiveInput(true)` hides Arm+Listen (`MixerTrackStrip.cpp:458-473`); `hasArm()` is `!mNoLiveInput && (Vox||Inst)` (`MixerTrackStrip.h:432-435`). Called for sfizz Inst tabs via `MixerPage::setInstStripNoLiveInput` (`MixerPage.cpp:2597`).

### Monitoring
- Both tasks compute `active = channelOK && (armed || listen)` — monitoring works with arm off (`VoxStripTask.cpp:82-89`, `InstStripTask.cpp:90-99`).
- Listen-LED right-click opens the monitor-mode menu, built **inline in the strip** with hardcoded labels: Vox 3-way "True Dry / Bypass Pitch Corrector / With Effect" (`MixerTrackStrip.cpp:398-422`), Inst 2-way "Dry / With Effect" (`:426-449`).
- Vox monitor mode is pushed into the engine: `mVocalEngine->setMonitorMode(monitorMode)` (`VoxStripTask.cpp:233`) — engine-side split, `BaySickVocalProcessor.h:151-157`.
- Inst monitor mode is a **generic, engine-agnostic crossfade** implemented in the task: pre-engine buffer stashed in `mMonitorDryBuf`, ~15 ms ramp back to it (`InstStripTask.cpp:332-372`). No engine type is named.
- Listen gate at the tail: `armed && !listen` clears `blockView` *and* the pre-fader tap (`VoxStripTask.cpp:341-346`, `InstStripTask.cpp:389-395`).

### The audio source
`mLiveInputSnapshot` is a processor-wide, non-cleared copy of the device input channels taken **before `buffer.clear()`** each block (`PluginProcessor.cpp:2724-2749`; member `PluginProcessor.h:1790`). Published to every task as `BlockContext::liveInputSnapshot` (`PluginProcessor.cpp:3812`; field `Source\Engine\BlockContext.h:62`). A strip copies `snapshot[chIdx]` → L and `snapshot[chIdx+1 or chIdx]` → R (`VoxStripTask.cpp:222-226`, `InstStripTask.cpp:281-285`).

*Inferred:* `chIdx` indexes the **active** (negotiated) input channels of the buffer, while the picker lists `getInputChannelNames()` (all device inputs). Under ASIO these coincide because startup force-enables every device input channel (`StandaloneApp.cpp:932-940`); under Windows-audio drivers the masks are left at JUCE defaults (`:949-953`), so a partial input mask could offset the mapping.

### Chain order
- **Vox:** input snapshot → (DRY tap, armed only) → `BaySickVocalProcessor::processBlock` (realtime corrector → WET tap → optional monitor-merge of prior takes → vocal chain rack → embedded NAM/IR) → `BaySickGraph::processInsert(Vox, idx)` → listen gate. (`VoxStripTask.cpp:209-346`; task header contract `VoxStripTask.h:13-36`.)
- **Inst:** input snapshot → (DRY tap, armed only) → optional pre-engine merge of routed clips → `EngineChainProcessor::processBlock` = **Pedals → NAM/IR** for `Source::LiveInput` (`Source\Inst\InstPage.cpp:741-758`) → monitor Dry/Wet fork → `processInsert(Inst, idx)` → listen gate. (`InstStripTask.cpp:268-402`.)
- `processInsert` → `InsertNode::processBlock`: pre-rack EQ → polarity → width → rack → post-rack EQ → fader×mute×solo → pan → SC tap → PDC delay → meters (`Mixer.md:42-46`; entry point `Source\BaySickGraph.cpp:2283-2297`).
- Sidechain predecessors are pulled and handed to the engine *before* the render in both tasks (`VoxStripTask.cpp:280-288`, `InstStripTask.cpp:316-324`).
- Inst-only extras welded to built-ins: sfizz source detection via `mGuitarsActive/mBassesActive` forces arm+listen off (`InstStripTask.cpp:52-94`), and idle-suspend queries `getBaySickGuitars/getBaySickBasses` voices (`:210-266`).

---

## 2. How recording from a live input lands in the project

### Arming and starting
- `GlobalTransportBar::RecordMode { Audio, Midi }` (`Source\Standalone\GlobalTransportBar.h:129-131`, member `:250`, default Audio). The chevron menu literally reads "ASIO" / "MIDI (piano roll tabs only)" (`GlobalTransportBar.cpp:481-508`). Not persisted (`Transport and Playback.md:205`).
- Record is arm-then-play: `StandaloneEditor`'s `onPlay` (`StandaloneEditor.cpp:1058-1154`). Audio mode with no saved project → prompt, `saveProjectAs`, then re-arm and start automatically (`:1069-1120`). MIDI mode with `mLastRollKind == None` → refuse and disarm (`:1134-1145`).

### Writers
`BaySickDAWProcessor::startRecording` (`PluginProcessor.cpp:6591-6727`):
- Lowers `mStripTapsLive` / `mMasterTapLive`, `settleAudioThread()`, clears `mStripRecorders` (`:6600-6608`).
- Scans strips by **hard-coded prefix + channel base**: `scan("mixer_vox_", kMaxVoxStrips, kVoxBase, "Vox", isVox=true)` and `scan("mixer_inst_", kMaxInstStrips, kInstBase, "Inst", false)` (`:6694-6697`). Selection criterion is purely `<prefix>_arm > 0.5` (`:6638`).
- One `StripRecorder` per armed strip, mono, file `<project> - <Vox|Inst N> - <ts> - DRY.wav` in the project Samples folder (`:6641-6647`).
- Vox-only WET writer: `dynamic_cast<BaySickVocalProcessor*>(mVoxEngines[i])`, gated on `bsv_pitch_realtime_bypass` being off, then `vp->setWetRecorder(...)` (`:6664-6689`).
- Zero strips armed ⇒ single master-output capture with leading-latency trim (`:6707-6726`).
- Publishes with `mStripTapsLive.store(true, release)` (`:6701`).

Audio-thread tap: `tapDryRecorder(channelId, monoSource, numSamples)` — bails on `isNonRealtime()` (freeze/export), acquire-loads `mStripTapsLive`, linear-scans `mStripRecorders` for the channel id, writes one mono block (`PluginProcessor.cpp:6824-6852`). Called only from the `armed` branch of the two tasks (`VoxStripTask.cpp:217-220`, `InstStripTask.cpp:276-279`).

`stopRecording` (`:6729-6815`) clears the gates, nulls each Vox engine's wet recorder, settles, then closes writers into `RecordResult{ stripFiles, stripWetFiles, masterFile, failedStrips, droppedTakes, midiNotes, startBeat, preRollSamples }`.

### Landing on the arrangement
`StandaloneEditor::commitRecordingResult` (`StandaloneEditor.cpp:20760-21092`):
- One undoable transaction — audio-library slice + pattern slice captured up front (`:20769-20773`).
- `dropWavAsClip(file, routeChannel)` (`:20783-20890`): reads the WAV header for length, subtracts `preRollSamples` (count-in is *in* the file; the visible clip starts at the downbeat, `block.contentStartSamples = res.preRollSamples`, `:20862`), finds `nextRow = max(trackRow)+1`, builds an `ArrangementBlock{clipType=Audio, audioFilePath="Samples/"+name, routeChannel, originalBPM, stretchMode=true}`, calls `mPM->addAudioToLibrary(path, {}, routeChannel)` + `mPM->addBlock(block)` (`:20844-20872`). **Only `routeChannel == 0` spawns a new Audio row + InsertNode + mixer strip** (`:20878-20889`); Vox/Inst-routed clips replay through the originating strip's FilePlay branch.
- Channel classification is by **id range**: `isVoxCh` = `[kVoxBase, kVoxBase+kMaxVoxStrips)`, `isInstCh` = `[kInstBase, …)` (`:20923-20926`).
- Vox take handling (`:20938-21037`): File-Settings checkboxes ∪ the per-strip Builder-Grid-Default pick; auto rule = highest wanted index in Dry < DryCleaned < Wet < WetCleaned; denoise profiles from `BaySickVocalProcessor::getDenoiseProfiles` or `Denoise::learnFromFile`; `Denoise::cleanFile` for cleaned variants with fallback on failure; exactly one take → `dropWavAsClip`, the rest → `addAudioToLibrary`, unselected source takes deleted.
- Inst take handling is one line: `dropWavAsClip(dryFile, chId)` (`:21041`).
- Grid-default pick is session-only, `mVoxTakePick[]` capped at `kDenoiseMaxVox` (`StandaloneEditor.cpp:5272-5281`) — hence the doc's "first six Vox tabs".
- One consolidated "Recording problems" dialog for failed arms / dropped blocks / denoise failures (`:21054-21081`).
- MIDI notes route to `mLastRollKind` / `mLastRollIndex` with pre-roll shift, Noodling discard, Early-Strike clamp, and `Unified_RecordQuantizeDiv` snap (`:21099-21158`).
- Master capture is moved to the project Exports folder, **not** auto-dropped on the grid (`:20892-20918`).

There is **no take-lane / take-comping system** — each take makes a new `trackRow`.

---

## 3. MIDI input

### Device selection
- One `juce::AudioDeviceManager` owned by `BaySickDAWStandaloneApp` (`StandaloneApp.cpp:762`). At startup: read saved `<MIDIINPUT>` identifiers; if none saved **or none of the saved ids matches an available device**, enable-all (`StandaloneApp.cpp:1165-1187`).
- **One** callback registration with an empty identifier — JUCE's standalone pattern — so hot-plugged / later-enabled devices still deliver (`:1199`, rationale `:1189-1198`).
- UI: the MIDI-input toggle list lives in the custom `AudioSettingsDialog` (`StandaloneEditor.cpp:137-147`, `rebuildMidiToggles`), applied live via `setMidiInputDeviceEnabled`. Trigger Velocity combo sits beside it (`:149-167`). Options menu shows a read-only "MIDI is Omni (all devices)" row (`StandaloneEditor.cpp:11859`).

### Getting to an engine
`BaySickDAWStandaloneApp::handleIncomingMidiMessage` (`StandaloneApp.cpp:1367-1390`) forks into two paths:
1. `mProcessor->getLiveMidiCollector().addMessageToQueue(message)` — gated on `isLiveMidiReady()` (collector needs `reset(sampleRate)`; pre-device messages are dropped).
2. CC / pitch-bend / channel-pressure only → `getMidiLearnEventQueue().push(deviceName, message)` — the only path that preserves the device name.

`processBlock` drains the collector and routes to **exactly one** destination buffer chosen by `mLiveMidiTargetKind` / `mLiveMidiTargetIndex` (`PluginProcessor.cpp:3574-3671`):

| kind | dest |
|---|---|
| 1/2/3 | `layerPageMidi` / `bassPageMidi` / `drumPageMidi` |
| 4 | `clipPageMidi` |
| 7 or 8 | `instPageMidi[idx]` (sfizz Guitars/Basses; kind 8 gets a −12 live transpose, `:3604`) |
| 9 | `mRustyDrumsMidi` |
| 10 | `pluginPageMidi[idx]` — hosted VST3 instrument (`:3594`) |
| anything else | `dest == nullptr`, dropped |

Also in that loop: `allMidi` always receives the raw performance for the MIDI recorder (`:3616`); on-screen held-note lights via `updateLiveHeldNote` (`:3620-3622`); a transport-sync filter drops MIDI clock / start / stop / continue / SPP / active-sense / quarter-frame before they reach a hosted plugin (`:3646-3649`); drum-trigger learn capture and `dispatchDrumTriggers` (`:3659-3662`).

**Can multiple engines listen at once? No.** One `std::atomic<int> mLiveMidiTargetKind` + `mLiveMidiTargetIndex` (`PluginProcessor.h:2275-2276`, setter `:653-657`), pushed solely from `PianoRollPage::onEngineSelected` and one initial push (`StandaloneEditor.cpp:2385`, `:2402`). The only concurrent side-channel is `dispatchDrumTriggers`, which fans a bound pad into `drumPageMidi[di]` regardless of focus — but note triggers are themselves gated on the Drum Kit being the focused engine (`PluginProcessor.cpp:8566-8569`).

**Vox and live-input Inst are explicitly excluded from the roll dropdown**, so they can never be the MIDI target: `if (src == InstPage::Source::LiveInput) continue;` (`StandaloneEditor.cpp:2350-2366`).

### MIDI Learn
- Registry: `Source\MidiLearn\MidiLearnRegistry.h/.cpp`. `Mapping{ paramId, msgType(Cc|PitchBend|ChannelPressure), ccNumber, channel(0=Omni), deviceName(empty=any), formula(reserved) }` (`:54-70`). One mapping per paramId; `std::map` under a `juce::SpinLock`; audio-thread `dispatchEvent` try-locks and skips on contention (`:88-98`, class comment `:23-30`). Linear mapping only.
- **Binds any APVTS paramId** reachable from a right-click. Menu items are appended generically by `VKnobAutomation::appendMidiLearnMenuItems` (`Source\Standalone\SharedUI.cpp:1942-1974`) into both VKnob's own menu and `GlobalAutoRightClick::buildControlMenu` (`SharedUI.h:984-1005`), which fires on **any** component with a non-empty `componentID` — so plain mixer faders qualify. Learn is greyed when `juce::MidiInput::getAvailableDevices()` is empty (`SharedUI.cpp:1955-1959`).
- Lifecycle: `MidiLearnUI::beginLearn` arms the registry, sets a 30 s deadline, starts a 20 Hz poll, and parents a `MidiLearnOutlineOverlay` (dashed yellow) on the target (`MidiLearnUI.h:72-89`). Capture is a lock-free handshake — audio thread fills a pre-built `Mapping` and sets `mCaptureReady`; the message thread commits (`MidiLearnRegistry.h:100-124`).
- MIDI-thread → audio-thread bridge `MidiLearnEventQueue`: 1024-event bound, device names interned into a fixed 32-slot never-resized table, drain releases the lock before callbacks (`MidiLearnRegistry.h:200-312`).
- **Persistence — the doc is stale.** `MIDI Learn.md:117` says mappings save to the project *and* `Documents\BaySickDAW\MidiMappings.xml` as global defaults that a project overlays. The code header records a 2026-08-24 ruling to **PROJECT-ONLY**, with the global layer and the "Save as global default" menu row removed (`MidiLearnRegistry.h:32-40`). Confirmed: `appendMidiLearnMenuItems` has no such item (`SharedUI.cpp:1942-1974`), and mappings round-trip only through `<MidiCCMappings>` in project state (`PluginProcessor.cpp:6898`, `:6953`, `:7047`, `:7398`). Two stale comments still name the file: `PluginProcessor.h:676` and `MidiLearnUI.h:116`.
- Drum trigger map (`Source\MidiLearn\DrumTriggerMap.h/.cpp`) is a separate store, note-or-CC per drum tab, `<DrumTriggers>` in project.xml, cleared on load when absent. **This whole subsystem dies with the built-in drums.**

### The piano-roll keyboard
Two distinct mechanisms, neither of which is MIDI hardware:
- **Typing keyboard** (Ctrl+T / transport piano button): `toggleTypingKeyboard` (`StandaloneEditor.cpp:12198-12204`); `keyPressed` maps keys to notes (`:12236-12266`); `sendTypingNote` synthesises a `MidiMessage`, self-stamps the timestamp, and pushes into **the same `getLiveMidiCollector()`** as hardware (`:12206-12215`). So it obeys `setLiveMidiTarget` identically.
- **Roll keyboard / grid clicks** call per-engine `auditionNote / auditionNoteOn / auditionNoteOff` closures (`StandaloneEditor.cpp:8289-8348`, `:8421`). Hosted VST3 tabs have no such API, so their closures route through `sendTypingNote` into the live collector instead (`:11017-11040`, rationale `:10988-11001`).

### ASIO input/output name matching (confirms the memory)
- Startup, before opening: if `deviceType == "ASIO"` and the saved input name ≠ output name, **force input := output** (`StandaloneApp.cpp:839-845`). Post-open net for the empty-input case at `:929-930`.
- Settings dialog Apply: `if (selType == "ASIO") xml->setAttribute("audioInputDeviceName", mDevBox.getText())` — one driver, names must match (`StandaloneEditor.cpp:458-462`); non-ASIO keeps the existing input only if it belongs to the selected type (`:463-473`).
- ASIO-only: force every device input and output channel active after the open (`StandaloneApp.cpp:923-948`); non-ASIO deliberately left at JUCE defaults (`:949-953`), because forcing them caused a right-speaker-only bug.
- Device changes are pending-file + restart (`StandaloneEditor.cpp:480-522`); buffer-size-only changes apply live (`:526-564`).

---

## 4. Making "live audio input" a per-insert PROPERTY (FL-style)

### Reusable essentially as-is
| Piece | File:line | Why |
|---|---|---|
| `mLiveInputSnapshot` + `BlockContext::liveInputSnapshot` | `PluginProcessor.cpp:2724-2749`; `Engine\BlockContext.h:62` | Processor-wide, prefix-agnostic; any task can read it. No change needed. |
| `addLiveInputParams` param block | `PluginProcessor.cpp:9926-9976` | Takes an arbitrary `prefix`. Only the two `_monitorMode` `startsWith` guards are prefix-specific. |
| `setInputChannelName` / `getInputChannelName` | `PluginProcessor.cpp:9978-9991` | Pure `<prefix>_inputChannelName` property, prefix-agnostic. |
| `tapDryRecorder` | `PluginProcessor.cpp:6824-6852` | Keys purely on `channelId`; no engine, no page. |
| `StripRecorder` / `AudioFileRecorder` + the `mStripTapsLive` gate + `settleAudioThread` discipline | `PluginProcessor.h:1567-1569`, `PluginProcessor.cpp:6598-6608`, `:6729-6751` | Container of `{channelId, file, recorder}`; no type assumptions. |
| `showInputChannelPicker` channel-list half | `MixerPage.cpp:2452-2506`, `:2529-2580` | Works off `prefixFromChannelId(channelId)`; only the header string, the Vox grid-default block (`:2507-2521`) and `kVoxBase` arithmetic (`:2539`) are Vox-specific. |
| `computeChannelGroups` pair detection | `MixerPage.cpp:2296-2444` | Free function in an anon namespace; no strip type. |
| Inst-style monitor Dry/Wet crossfade | `InstStripTask.cpp:332-372` | Buffer-level, no engine type named — the FL-generic monitor fork already exists here. |
| Listen gate + pre-fader-tap correction | `InstStripTask.cpp:389-395` | Generic. |
| `dropWavAsClip` | `StandaloneEditor.cpp:20783-20890` | Takes `(file, routeChannel)`; `routeChannel != 0` already means "replay through that insert, don't make a new row". |
| `PassiveStripTask` | `Engine\Tasks\PassiveStripTask.h`, `.cpp` | Proof an insert can render with **no engine at all** (Aux/Bus). The natural base for an engineless live-input insert. |

### Welded to page/engine — must be cut or rewritten
| Weld | File:line | Nature |
|---|---|---|
| Task exists only if an engine is registered | `PluginProcessor.cpp:8813-8820`, `:8862-8870` | `VoxStripTask`/`InstStripTask` are constructed inside `registerVoxEngine`/`registerInstEngine` with a mandatory `AudioProcessor*`; both `run()`s call `mEngine->processBlock` on the live path. An insert with only VST3 effects has no such engine. |
| Fixed-size per-kind task arrays | `PluginProcessor.h:2379-2380` | `std::array<VoxStripTask, kMaxVoxPages>` / `<InstStripTask, kMaxInstPages>`. |
| `_arm` param only minted for two prefixes | `PluginProcessor.cpp:8979-8983` | Inside `ensureMixerStripParams`. Any other insert silently has no arm param. |
| `addLiveInputParams` called from exactly two sites | `PluginProcessor.cpp:9318`, `:9330` | `ensureVoxInsert` / `ensureInstInsert` only. Aux (`:9280`) and Rusty (`:9341-9349`) deliberately skip it. |
| `_monitorMode` creation gated on prefix | `PluginProcessor.cpp:9960`, `:9971` | Two different ranges (0-2 vs 0-1) chosen by prefix string. |
| Recorder scan hard-codes two prefixes/bases | `PluginProcessor.cpp:6694-6697` | `scan("mixer_vox_", …, kVoxBase, …)` + `scan("mixer_inst_", …, kInstBase, …)`. An insert outside 600-729 can never be recorded. |
| WET recorder path is `BaySickVocalProcessor`-only | `PluginProcessor.cpp:6664-6689`, `:6736-6744` | `dynamic_cast`, `bsv_pitch_realtime_bypass`, `setWetRecorder`. Dies with the vocal engine. |
| Commit routes by channel-id range | `StandaloneEditor.cpp:20923-20926`, `:20938`, `:21039` | `isVoxCh` / `isInstCh` decide take handling; the Vox branch also calls `mProcessor.voxEngineAt(voxIdx)` + `Denoise::*` (`:20988-21021`). |
| Grid-default state is a Vox-indexed array | `StandaloneEditor.cpp:5272-5281`, `MixerPage.cpp:2507-2521`, `:2539` | `mVoxTakePick[kDenoiseMaxVox]`, addressed via `channelId - kVoxBase`. |
| Strip UI gated on `StripType::Vox \|\| Inst` | `MixerTrackStrip.cpp:361-375`, `:387-450`; `MixerTrackStrip.h:432-435` | Arm right-click, Listen LED, and both monitor-mode menus (whose labels are hardcoded per type) all live behind this enum test. |
| Picker wired only on the two strip factories | `MixerPage.cpp:2257`, `:2642` | `onArmRequested` is set nowhere else. |
| Vox monitor path calls vocal-engine APIs | `VoxStripTask.cpp:230-234`, `:248-271` | `setForcePitchBypass`, `setMonitorMode`, `setMonitorMergeForThisBlock`, `setFilePlaySourceStamp` — all `BaySickVocalProcessor`. The whole Vox monitor-merge design goes with the engine. |
| Inst task sfizz coupling | `InstStripTask.cpp:52-57`, `:91-94`, `:210-266` | `mGuitarsActive/mBassesActive` force arm+listen off; idle-suspend calls `getBaySickGuitars/getBaySickBasses`. |
| Tab spawn cascade | `MixerPage.cpp:2269`/`:2652` → `StandaloneEditor.cpp:5304-5321` | Every strip mints a ribbon page. Under bus-derived tabs this cascade is the thing being replaced. |
| Live-MIDI target excludes live-input strips | `StandaloneEditor.cpp:2350-2366`; `PluginProcessor.cpp:3667-3670` | Encodes "Vox/live-Inst take audio, not MIDI". If an insert becomes both a live-audio destination and a VST3 instrument host, this exclusion no longer maps 1:1 onto a strip. |

### Shape of the minimal cut (inferred)
A per-insert live-audio property needs: (a) `addLiveInputParams` + `_arm` moved into the common `ensureMixerStripParams` path for every insert kind rather than two prefix tests; (b) a single engineless strip task modelled on `PassiveStripTask` that reads the same five params, copies from `ctx->liveInputSnapshot`, taps `tapDryRecorder`, runs the Inst-style Dry/Wet monitor fork, and calls `processInsert` — with the VST3 rack standing in for the removed engine; (c) `startRecording`'s scan replaced by an enumeration over inserts that have `_arm` (the arm param's existence becomes the "this insert can take live audio" marker); (d) `commitRecordingResult` reduced to the Inst branch (`dropWavAsClip(dryFile, chId)`) for every insert id, with the master fallback unchanged; (e) `MixerTrackStrip`'s arm/listen gate changed from `StripType::Vox||Inst` to "this strip's `_arm` param exists", and one shared 2-way monitor menu.

---

# READER 2: MIXER & ROUTING — fact sheet with evidence

# MIXER & ROUTING — fact sheet with evidence

Docs read first: `Plans & Specs/System Reference/INDEX.md`, `Mixer.md` (418 lines, authoritative), `Effect Racks.md`, `Plugins Page.md`. Every claim below is confirmed in code; doc-only claims are flagged.

---

## 1. How strips come to exist

### 1a. Channel-id space and caps (the whole vocabulary)

`Source/BaySickGraph.h:33-303` — `namespace MixerChannelIds`.

| Range | Family | Constant | Cap | Cap source |
|---|---|---|---|---|
| 0 | Output (terminal sink) | `kOutput` :35 | — | — |
| 1-18 | 17 buses + Master(4) | :36-69 | fixed | hand-listed |
| 100-117 | Aux | `kAuxBase` :70 | `kMaxAuxStrips = 18` :102 | literal (no page) |
| 200-219 | Layer inserts | `kLayerBase` :71 | `kMaxLayerStrips = kMaxLayerPages` = 20 :90 | `BaySickConstants.h:13` |
| 300-309 | Bass | :72 | 10 :91 | `BaySickConstants.h:14` |
| 400-499 | Audio (Clips) | :73 | 100 :93 | `BaySickConstants.h:20` |
| 500-531 | Drum | :74 | 32 :92 | `BaySickConstants.h:15` |
| 600-609 | Vox | :75 | 10 :94 | `BaySickConstants.h:22` |
| 700-729 | Inst | :76 | 30 :95 | `BaySickConstants.h:23` |
| 800-812 | Rusty | :77 | `kMaxRustyStrips = 13` :103 | literal |
| 900-919 | Plugin (hosted VST3 instr.) | :78 | 20 :96 | `BaySickConstants.h:25` |
| 950-965 | Direct-to-Master | `kDirectBase` :82 | `kMaxDirectStrips = 16` :99 | literal |

Hard ceiling: `kMaxStripChannels = 1000` (`BaySickGraph.h:1078`), which sizes the flat node array `mInsertsByChannel` (:1079) and the dispatcher's `mTasksByChannel` (`RenderGraphDispatcher.cpp:75`).

APVTS prefix is a pure function of chId: `prefixFromChannelId` (`BaySickGraph.h:135-169`), e.g. 200 → `mixer_layer_0`, 4 → `mixer_master`.

### 1b. Strip kinds

- **Node kind enum**: `BaySickGraph::InsertKind { Layer, Bass, Drum, Audio, Aux, Vox, Inst, Rusty, Plugin, Direct }` — `BaySickGraph.h:631`.
- **Param kind enum**: `MixerStripKind { Master, Bus, Insert }` — used by `addParamsForMixerStrip` (`PluginProcessor.cpp:8916`, gating at :8947-8973).
- **UI widget kind**: `MixerTrackStrip::StripType { Master, Bus, DrumChannel, LayerChannel, BassChannel, Aux, Vox, Inst }` — `MixerTrackStrip.h:92-102`. Note there is **no** Plugin/Rusty/Direct StripType: `addPluginChannel` and `addDirectChannel` both construct `StripType::LayerChannel` with a different accent (`MixerPage.cpp:1708-1709`, `1741-1742`). All types are 80 px wide (`MixerTrackStrip.h:257-277`).
- **UI rename kind**: `MixerPage::StripKind { Layer, Bass, Drum, Audio, Plugin, Vox, Inst, Direct }` — `MixerPage.h:76`.

### 1c. Fixed vs lazy

**Eager (fixed bank, exist always):**
- Master + 17 bus **params**: `ensureMixerBusAndMasterParams()` — `PluginProcessor.cpp:9065-9102`, called from `prepareToPlay` (:822) and from the `MixerPage` ctor (`MixerPage.cpp:1441`).
- Master + 17 bus **nodes**: `buildFixedTopology` (`BaySickGraph.cpp:1091`), one-shot (`mTopologyBuilt`).
- Master + 17 bus **routing entries**: `buildFixedBusChannels()` builds the 18-entry constant head of `mActiveChannels` once in the ctor (`BaySickGraph.cpp:3174-3215`, count stored in `mFixedBusChannelCount` :3210).
- Master + 17 bus **render tasks**: `kBusChannelIds` array + loop in `prepareToPlay` (`PluginProcessor.cpp:879-905`), `kNumBatch7Buses = 17` (`PluginProcessor.h:2399`), plus `MasterTask` (`PluginProcessor.h:2405`).
- Master + 9 bus **UI strips** built in the `MixerPage` ctor (`MixerPage.cpp:1443-1517`); the secondary buses' widgets are lazy (`activateVoxBus2` etc., `MixerPage.cpp:1916-2226`).

**Lazy (per tab/engine):** every insert strip. Three-part creation, always in this order:
1. `ensureMixerStripParams(prefix, kind, defaultSendTo)` → `addParamsForMixerStrip` once per prefix, tracked in `mRegisteredMixerStrips` (`PluginProcessor.cpp:9040-9063`). **Params are never removed** (:9044 early-out; doc `Mixer.md:280-283`).
2. `mVibeGraph.ensureInsertNode(kind, index, name, prefix)` (`BaySickGraph.cpp:2216`) → slot in `mInsertsByChannel` + push to `mLiveInsertChannels`.
3. A `RenderTask` registered with the dispatcher, which is what binds an arena buffer (`RenderGraphDispatcher.cpp:97`).

Creation call sites (all message thread):

| Kind | Site | Trigger |
|---|---|---|
| Layer | `PluginProcessor.cpp:8146-8161` (`registerLayerEngine`) | engine registration |
| Plugin | `:8167-8186` (`registerPluginEngine`) | hosted VST3 instr. tab |
| Bass | `:8507-8508` | engine registration |
| Drum | `:8698-8705` | drum slot gets a sound |
| Audio | `:9240-9262` (`ensureAudioInsert`) | clip row |
| Aux | `:9265-9287` (`ensureAuxInsert`) | Mixer "Add > Aux Strip" |
| Vox | `:9313-9323` (+`addLiveInputParams`) | Mixer/ribbon add |
| Inst | `:9325-9335` (+`addLiveInputParams`) | Mixer/ribbon add |
| Rusty | `:9341-9362` | kit load, batch of 13 |
| Direct | `:8251-8258` (`ensureDirectStripInfra`) | file added from browser |

Vox/Inst are the split case: `ensureVoxInsert`/`ensureInstInsert` create node+params, but the **task** is created later by `registerVoxEngine` / `registerInstEngine` (`PluginProcessor.cpp:8801-8820`, `8847-8871`). A strip with a node but no task has no arena slot, so `rebuildLinks` silently skips every edge touching it (`RenderGraphDispatcher.cpp:175-176`).

**Destruction:** `removeInsertNode` (`BaySickGraph.cpp:2258`), `clearAuxInserts` (`:3271`), `MixerPage::remove*Channel` (`MixerPage.cpp:1765, 2948-3060`) remove **widget/node only — APVTS params survive** (`MixerPage.h:182-197`, doc `Mixer.md:355-358`). `clearDynamicStrips` (`MixerPage.cpp:2731`) wipes every non-bus widget on project open.

### 1d. Who owns the params

`BaySickDAWProcessor::addParamsForMixerStrip` (`PluginProcessor.cpp:8916-9038`) is the single writer. Full per-strip table:

| Param | Type/range | Default | Gate | Line |
|---|---|---|---|---|
| `_level` | Float dB −60..+5.6 | 0 | all | 8943 |
| `_pan` | Float −1..1 | 0 | all | 8944 |
| `_width` | Float 0..2 | 1 | all | 8945 |
| `_mute`,`_solo`,`_polarity` | Bool | false | Bus+Insert | 8949-8951 |
| `_mute` only | Bool | false | Master (no solo/polarity) | 8962 |
| `_bypass` | Bool | false | all | 8966 |
| `_collapsed` | Bool | false | Bus only | 8973 |
| `_arm` | Bool | false | Insert **and** prefix startsWith `mixer_vox_`/`mixer_inst_` | 8979-8983 |
| `_sendTo` | Int 0..999 | `defaultSendTo(chId)` | all | 8995 |
| `_mainOut{1..3}_to` | Int −1..999 | −1 | all | 9000-9002 |
| `_send{0..3}_to` | Int −1..999 | −1 | all | 9008 |
| `_send{0..3}_amount` | Float dB −60..+6 | 0 | all | 9009 |
| `_send{0..3}_prepost` | Bool | false=post | all | 9010 |
| `_sc_recv{0..3}_from` | Int −1..999 | −1 | all | 9022 |
| `_chokeGroup` | Int 0..16 | 0 | Insert | 9030 |
| `_playNote` | Int 0..127 | 60 | prefix `mixer_drum_` | 9037 |

Live-input extras via `addLiveInputParams(prefix)` — `PluginProcessor.cpp:~9921-9955`: `_inputChannelIdx` (Int −1..127, −1), `_listen` (Bool false), `_inputChannelStereo` (Bool false); `_monitorMode` also exists per Vox/Inst (read at `VoxStripTask.cpp:63`, `InstStripTask.cpp:75`). Called **only** from `ensureVoxInsert` (:9318) and `ensureInstInsert` (:9330).

Globals: `master_fx_bypass`, `master_pan_law` (read as cached pointers, `BaySickGraph.cpp:410-411`).

---

## 2. The graph edges that exist today

### 2a. Edge model
`RoutingGraph` — `BaySickGraph.h:308-396`:
- `struct Edge { srcId, dstId, amountDb, prePost, isMainOut }` (:313-321)
- `struct ScEdge { srcId, dstId, dstSlot }` (:326-331)
- `kMaxSendsPerStrip = 4` (:334), `kMaxScRecvsPerStrip = 4` (:335), `kMaxMainOutsPerStrip = 4` (`:209`, aliased :336)

### 2b. Storage in APVTS, rebuilt every block
`RoutingGraph::rebuildFromApvts` (`BaySickGraph.cpp:2682-2787`), called from `BaySickGraph::rebuildRoutingFromApvts` (`:3217-3255`) at the top of each `processBlock`.

- Main-out line 0 = `<prefix>_sendTo`, fallback `defaultSendTo(chId)`; lines 1..3 = `<prefix>_mainOut{N}_to`, fallback −1 (`:2735-2750`). Duplicate destinations across two lines of the same strip are dropped (`:2741-2744`) — "two edges src→dst would sum that strip in twice, i.e. +6 dB".
- **Main-out edges carry no level**: `e.amountDb = 0.f; e.prePost = false` (`:2748`) and consumers hard-code unity (`MasterTask.cpp:54-56`, `PassiveStripTask.cpp:45-47`).
- Sends 0..3 (`:2751-2760`) carry `amountDb` + `prePost`.
- SC receive lines are **target-side encoded**: strip reads its own `_sc_recv{N}_from` and creates `src→this` (`:2765-2771`).
- Param-id strings are cached per (chId, prefix) in `mChannelParamIds` to avoid per-block `juce::String` allocation on the audio thread (`BaySickGraph.h:365-390`, `.cpp:2710-2725`).
- `RoutingGraph` itself enforces **no legality rule at all** — any dstId in 0..999 becomes an edge. Legality lives entirely in the UI (§3/§5).

### 2c. Cycles and order
- `wouldCreateCycle(src, dst)` (`BaySickGraph.cpp:2658-2680`): DFS from `dst`, walking **both** `mEdges` and `mScEdges`. Message-thread pre-flight only — every menu item calls it before enabling itself (`MixerPage.cpp:713, 752, 803, 825`).
- `computeTopo` (`:2789-2857`): Kahn's algorithm over main+send+SC edges; on failure it erases edges between all unresolved nodes and re-runs, so a cycle degrades to dropped edges rather than a hang.
- `mHasPreFaderSend` recomputed **after** topo so an edge dropped as a cycle can't leave a tap armed (`:2779-2784`).

### 2d. Who consumes the edges
`RenderGraphDispatcher::rebuildLinks(routing)` (`RenderGraphDispatcher.cpp:151-237`): for each audio edge it pushes an `UpstreamLink` onto **dst->mPredecessors**, appends dst to src->mChildren, and increments `dst->mInitialDeps` (:178-193). SC edges get `isSc=true, scSlot` (:196-214). Synthetic deps (Rusty producer → its 13 inserts) bump the counter without a predecessor entry (:220-225).

`UpstreamLink` — `Engine/UpstreamLink.h:32-39`.

**Only two task types actually sum audio predecessors:**
- `PassiveStripTask::run` (Aux + Bus) — `Engine/Tasks/PassiveStripTask.cpp:38-57`
- `MasterTask::run` — `Engine/Tasks/MasterTask.cpp:47-64`

Every other task **clears its buffer and renders its own source**, pulling only sidechain: `EngineInsertTask.cpp:71, 80` (Layer/Bass/Drum/Plugin), `CompositeAudioInsertTask.cpp:66` (Audio), `InstStripTask.cpp:172,186,316`, `VoxStripTask.cpp:177,196,280`, `RustyInsertTask.cpp:109`, `DirectFileTask.cpp:27`.

### 2e. Pre/post-fader taps
- `SendSourceRead::bufferFor` (`Engine/Tasks/SendSourceRead.h:36`) selects the pre-fader tap only when `prePost && !isMainOut && !isSc`.
- Tap definition and arming: `BaySickGraph.h:850-871` (`getPreFaderTap`, `getPreFaderTapBuffer`), `armPreFaderTaps` (`.cpp:3015-3062`), fast-path flag `mAnyPreFaderSend` (`.h:1217`), per-node `preFaderTap` + `preFaderTapDelay` (`.cpp:359-372`).
- SC source tap: `getScSourceTap` (`.cpp:2909`), `armScSourceTaps` (`:2978`), per-(consumer,slot) alignment delays `mScRecvDelays` (`.h:1190-1191`), applied by `applyScRecvDelay` (`.cpp:2954`).

### 2f. Chains, mastering, multi main-out
- Insert chain order (doc `Mixer.md:42-46`, code `InsertNode::processBlock`, entered via `BaySickGraph::processInsert` `.cpp:2283`): source → pre-EQ → polarity → width → rack → post-EQ → fader×mute×solo (gain-ramped, `mLastFaderGain` `.cpp:335`) → pan → SC tap → PDC delay → peak/RMS.
- Bus chain: `InstrChannelNode::processChainOnly`, dispatched by `processBus` (`.cpp:1259-1480`) — note fader **before** polarity/width, the reverse of an insert (doc `Mixer.md:48-53`).
- Master: `processMasterBus` (`.cpp:1134`) → `processMasterChain` + LUFS/true-peak/spectrum taps (`BaySickGraph.h:663-702`). Master's own `_sendTo` default is `kOutput` (`.h:270`); `MasterTask` writes the final buffer and sets the block-done flag (`MasterTask.cpp:73-78`).
- **Multi main-out**: up to 4 lines per strip, each a *full-level* copy, not a split (`BaySickGraph.h:197-215`).
- **Physical master out** is separate from the graph: `MasterOutputRouting::gFirstOutputChannel` / `gMasterIsMono`, persisted per-machine in `master_output.xml` (`Standalone/StandaloneApp.cpp:157, 440-464`; menu writes at `StandaloneEditor.cpp:7745-7747`).
- Solo is two independent axes: `isAnyInsertSoloed()` (`.cpp:2620`) and `anyBusSoloed()` (`.cpp:2643`, 17 cached pointers, `.h:1039`) with an explicit guardrail comment forbidding cross-talk (`.h:811-824`).
- PDC: `updateBusLatencies()` (`.cpp:1589`), `totalLatencySamples` (`.h:590`), per-node `CompDelayLine` (`.h:1130-1183`).

---

## 3. What "bus" means today

**A bus is one of 17 hard-coded channel ids**, not a role a strip can take on.

- `isBus(chId)` is a literal enumeration of those 17 ids (`BaySickGraph.h:172-182`). Nothing computes it.
- Each bus is a distinct `std::unique_ptr<InstrChannelNode>` **member** of `BaySickGraph` (`.h:1008-1107`), with its own hand-written accessor triples `getXxxBusRack/EQ/PreEQ` (`.h:469-564`) and its own peak/RMS atomic pair (`.h:908-972`).
- Bus selection at process time is a `switch` on channel id (`processBus`, `.cpp:1342-1400+`); so are `drainBusRms` (`.cpp:2440`), `busNodeForChannel` (`.cpp:2879`), `rebindBusApvts` (`.cpp:2579`).

**Can an arbitrary strip receive other strips?**
- Audio-engine answer: **only Aux and Bus and Master**, because only `PassiveStripTask`/`MasterTask` sum `mPredecessors` (§2d). An edge into any other insert would be built by `RoutingGraph`, would be given to `rebuildLinks`, would increment that task's dep counter — and the audio would be **silently discarded**, since the task clears its buffer and never reads the link.
- UI answer: `MixerPage::CableOverlay::isRouteAllowed` (`MixerPage.cpp:482-587`) whitelists destinations per source family; only these two rows allow an insert as a destination:
  - `srcIsAux → dstIsMaster || kFxBus || dstIsAux` (:567-568) — **aux→aux is the one working insert-to-insert main-out today** (unity gain).
  - `srcIsDirect → dstIsMaster || dstIsAux` (:530-531) — but Direct is main-out-locked (`BaySickGraph.h:193-194`), so this only serves its send menu.
- Sends: `isValidBusSendTarget(dst)` returns true **only** for the aux range (`BaySickGraph.h:219-222`); the Send… submenu filters on it (`MixerPage.cpp:711`). So a *levelled* send can go only to an aux.

**Per strip:** 4 sends (`_send{0..3}_to/_amount/_prepost`, amount −60..+6 dB, prepost bool default post), 4 SC receive lines, 4 main-out lines. All four counts are 4 by design (`BaySickGraph.h:197-209`).

Bus *visibility* (not existence) is membership-driven: `laidOutBus` hides a bus with no bucket members and no extra-main-out feeder (`MixerPage.cpp:4239-4301`), except the always-visible set (`:3167-3177`) and the seven user-added secondaries which follow a "has ever been routed" lifecycle (`:4259-4272`, flag map `MixerPage.h:406`).

---

## 4. How an instrument's audio enters a strip

**There is no `trackId → strip` binding.** The binding is **index identity, fixed at engine-registration time**:

```
page index i  →  chId = <family base> + i  →  prefix "mixer_<family>_<i>"  →  arena slot chId
```

- `registerLayerEngine(idx, eng)` creates `EngineInsertTask(eng, Kind::Layer, idx, layerInsert(idx), graph)` (`PluginProcessor.cpp:8156-8160`). Same shape for Plugin (:8180-8184), Bass, Drum, Vox (:8815-8819), Inst (:8864-8869), Rusty (:9356-9358), Direct (:8276-8278).
- `RenderGraphDispatcher::registerTask` assigns `task->mOutputBuffer = mArena.getStripBuffer(chId)` and `mTasksByChannel[chId] = task` (`RenderGraphDispatcher.cpp:97-99`). One task per channel id is enforced (`jassertfalse` + replace, :91-95).
- The engine writes directly into that arena slot and the insert chain runs in place (`EngineInsertTask.cpp:69-124`).

**Can a strip have multiple sources?** Yes, in exactly one place: `CompositeAudioInsertTask` (audio rows) sums a clip-engine flow *and* an arrangement-clip decode flow into one buffer, then runs `processInsert` exactly once (`CompositeAudioInsertTask.h:13-47`, `.cpp:66`). `InstStripTask`/`VoxStripTask` similarly merge live input + engine + FilePlay clips inside one task (`InstStripTask.cpp:87-134, 305`). Otherwise: one task, one source, one channel.

**Can a source change strip at runtime?** No. To move an instrument you would have to unregister and re-register at a different index — `unregisterVoxEngine` / `unregisterInstEngine` (`PluginProcessor.cpp:8823-8845, 8873+`) take down the task under the load shield, and the strip's params live under the old prefix. Nothing reassigns a channel id. (Inferred from the absence of any such path; every `register*Engine` derives chId from `pageIdx` only.)

Hosted **VST3 instrument** specifically: `HostedPluginInstance` is a `juce::AudioProcessor` owned by `EngineRig`; it enters the graph as an ordinary `EngineInsertTask` (`PluginProcessor.cpp:8180`) — doc `Plugins Page.md` "Identity and routing" table (chId `900+i`, prefix `mixer_plugin_<i>`, default Plugins Bus, **not** main-out locked).

---

## 5. The mixer page UI

`Source/Standalone/MixerPage.{h,cpp}` (623 + 4475 lines), strip widget `MixerTrackStrip.{h,cpp}`.

**Layout:** pinned Master panel on the left (`kFixedPanelW = 96+4`, `MixerPage.h:619`) + a horizontally scrolling `Viewport` with an external permanent scrollbar (`MixerPage.cpp:1450-1467`). `layoutScrollContent` (`:4033-4395`) buckets every strip by its **line-0 destination** (`bucketPush` :4069-4076) and lays each bus followed by its members with neon divider lines (`NeonLine`, `MixerPage.h:344-358`). Fixed group order at `:4303-4382`: FX → Clips(+2) → Vox(+2) → Inst(+2,+3) → Plugins(+2) → Layers(+2) → Bass(+2) → Rusty → Drums → Drums 2, with the "Direct Routing" label group before FX (`:4222-4223`, label member `MixerPage.h:462`). Member accent colour comes from `pickStripColor(chId, destChId)` (`:68`, applied `:4211`).

**What one strip shows** (`MixerTrackStrip.h:69-102, 219-254`; bindings in `setApvts` `MixerTrackStrip.cpp:317-450`): name label (editable per type, `setRenameable` :164), collapse arrow (bus only, `MixerCollapseArrow` :39-67), split peak/RMS `DBFSMeter`, **M**, **S**, FX-rack button (`onFXClicked`), **A**/arm + **headphones**/listen (Vox/Inst only, suppressed by `setNoLiveInput` :189), FX-bypass LED, master-FX-bypass (Master only, bound to global `master_fx_bypass` :382-383), pan knob, polarity button (`PolarityButton` :10-31), width knob, LUFS box (Master), fader + dB readout, and the socket + "+" button (`onAddSendRequested`, repurposed to `onAnalyzerRequested` on Master :234-238).

**How routing is changed today** — entirely from the per-strip "+" menu, `MixerPage::onAddCableRequestedFor` (`:684-871`), five submenus:
| Submenu | Filter | Writes |
|---|---|---|
| Send… | `isValidBusSendTarget` + free slot + `!wouldCreateCycle` (:711-713); "New Aux Strip" row creates+wires (:726-737) | `_send{N}_to` |
| Sidechain… | free `_sc_recv` on the **target** + `!wouldCreateCycle` (:750-752) | `<target>_sc_recv{N}_from` |
| Move Output… | `isRouteAllowed` + not already used + `!wouldCreateCycle` (:798-803) | `_sendTo` |
| Add Main Out… | same + free line 1..3 (:821-825) | `_mainOut{N}_to` |
| Remove Main Out… | line 0 shown disabled `(main output)` (:853) | `_mainOut{N}_to = −1` |

All three main-out submenus are skipped entirely when `isMainOutLocked(src)` (`:776`). Every write is wrapped in `beginParamUndoGesture` (`:720, 734, 760, 807, 834, 861`).

**Cable overlay** exists: `MixerPage::CableOverlay` (`MixerPage.h:499-561`). Painting + right-click property popup only — click-to-place and socket-drag were retired (`MixerPage.cpp:473-476, 589-593`). Right-click a cable → `hitTestCablesAll` → chooser when several overlap (`:424-471`) → `showCablePopup` (`:1176`); a main-out cable is actionable only on lines 1..3 (`:432-433`).

**Window "Add" menu**: `buildAddMenu` (`:658-682`) — Aux Strip, then Vox/Inst/Layers/Bass/Clips/Plugins Bus (each greyed once active). Delete via strip right-click → `deleteAuxStrip` (`:3215`) / `deleteSecondaryBus` (`:3257`), both of which sweep every strip's routing params before removing the node.

**Polling:** a 30 Hz `Timer` folds all four main-out lines of every strip into a hash and relayouts on change (`:3770-3838`, cache `MixerPage.h:468`); a `VBlankAttachment` drains meters (`onVBlank` `:3846-3930`). Both are started/stopped from `parentHierarchyChanged` (`MixerPage.h:596`).

**FL-like affordances already present:** FL-style pan (far side folds into near side) and the `master_pan_law` Ramped/Flat choice (doc `Mixer.md:244`); FL-style meter ballistics (`MixerTrackStrip.h:136-140`); bus groups with collapse; a fixed-width 80 px strip for every type.

---

## 6. Which strip hosted VST3 uses

- **Hosted VST3 instrument (Plugins tab)** → its own insert strip `mixer_plugin_<i>` at `pluginInsert(i)` = 900+i, node `InsertKind::Plugin`, task `EngineInsertTask::Kind::Plugin`, default parent `kPluginsBus` (13), **not** main-out-locked (`BaySickGraph.h:52-57`; `PluginProcessor.cpp:8167-8186`; `MixerPage.cpp:1704-1732`). Its legal main-out set is Plugins/Layers/Bass buses (+ their secondaries) + Master (`MixerPage.cpp:537-542`).
- **Hosted VST3 effect** → it is a `DSPBase` occupying one of the **6 slots of whatever strip's `EffectRack`** it was loaded into. Every insert node, every bus node and Master owns exactly one `EffectRack` (`InsertNode::rack` `BaySickGraph.cpp:305`; accessors `getInsertRack` `.h:648`, `getXxxBusRack` `.h:471-504`; doc `Effect Racks.md:13-18`). There is no separate channel id or strip for an effect. Per-slot sidechain selection is `Slot::scPick` (0..3 index into the strip's SC array, `EffectRack.h:169, 283`).

---

## 7. The mixer-strip pattern audit — every site in code

Memory file `~/.claude/projects/…/memory/reference_mixer_strip_pattern_audit.md` (121 days old; it says `VibeGraph.h` — the file is now `BaySickGraph.h`, and its "8 per-kind maps" claim is stale: QA-InsertMaps flattened them into `mInsertsByChannel` + `mLiveInsertChannels`, `BaySickGraph.h:1064-1080`). Sites verified against current code:

**A. `Source/BaySickGraph.h` — MixerChannelIds (8 sites)**
1. `kXxxBus` and/or `kXxxBase` constant — :36-82
2. `kMaxXxxStrips` cap (derive from a page cap where one exists) — :90-103
3. `xxxInsert(idx)` helper — :123-132
4. `prefixFromChannelId` switch **and** range fallback — :135-169
5. `isBus` — :172-182
6. `isMainOutLocked` — :190-195
7. `friendlyName` switch **and** range fallback — :227-261
8. `defaultSendTo` switch **and** range fallback — :266-302

**B. `Source/BaySickGraph.{h,cpp}` (9 sites)**
9. `InsertKind` enum value — `.h:631`; `computeChannelId` mapping — `.cpp:17` region
10. Bus node member + `getXxxRack/EQ/PreEQ` accessor triple — `.h:469-564`, `.h:1008-1107`
11. `buildFixedTopology` allocation + `prepare`/`reset` sweeps — `.cpp:1091, 954, 1053`
12. `rebindBusApvts` + `mBusSoloPtr` / `kNumSoloableBuses` — `.cpp:2579-2618`, `.h:1039`
13. `processBus` node+RMS switch — `.cpp:1259-1480`
14. `drainBusRms` switch — `.cpp:2440`; `busNodeForChannel` — `.cpp:2879`
15. `pushScArrayToStrip` bus+insert dispatch — `.cpp:3114`
16. `saveRackStates` / `applyRackStates` / `clearAllRackStates` / `forEachRack` — `.cpp:1897, 2039, 1995, 2523`
17. **`buildFixedBusChannels` (buses) / `rebuildRoutingFromApvts` `mActiveChannels` (inserts) — `.cpp:3174-3215, 3217-3237`. This is the critical one: a channel missing here has no routing entry at all *and* breaks the SC cycle-check for its neighbours** (`.cpp:3193-3200` records exactly that reasoning).

**C. `Source/PluginProcessor.{h,cpp}` (6 sites)**
18. `ensureMixerBusAndMasterParams` registration line — `.cpp:9065-9102` (the TS6 comment at :9086-9091 is the documented consequence of skipping it)
19. `ensure<Kind>Insert` — `.cpp:9240/9265/9313/9325/9341/8251`
20. `addParamsForMixerStrip` kind-specific extras (`_arm`, `_chokeGroup`, `_playNote`, `_collapsed`) — `.cpp:8947-9037`
21. `register/unregister<Kind>Engine` + render-task creation/teardown — `.cpp:8136, 8167, 8801, 8847, 9354, 8276`
22. `prepareToPlay` `kBusChannelIds` table (bus kinds only) — `.cpp:879-905`, `kNumBatch7Buses` `.h:2399`
23. Meter mirrors: per-kind peak arrays `.h:985-994` → `drainMeterAtomicsForUI` → `drainInsertPeakDbStereo` `.cpp:5233`; EQ strip-slot table `kEqNumBusSlots`/`kEqNumInsertSlots` `.h:2083-2089`

**D. `Source/Standalone/MixerPage.{h,cpp}` (11 sites)**
24. Strip map + order vector (+ next-idx counter) — `.h:420-457`
25. `addXxxChannel` / `removeXxxChannel` — `.cpp:1668, 1704, 1737, 1788, 1817, 2228, 2611, 2667, 2870, 3330`
26. Bus strip member + activation flag + ctor construction — `.h:362-401`, `.cpp:1470-1517`
27. `buildAddMenu` row — `.cpp:658-682`; `activate*` / `isSecondaryBus` / `deleteSecondaryBus` — `.cpp:1916-2226, 2172, 3257`
28. `pickStripColor` — `.cpp:68`
29. `rebuildStripCache` / `findStripByChannelId` — `.cpp:1370`
30. `CableOverlay::isRouteAllowed` `srcIsXxx` rule — `.cpp:482-587`
31. `layoutScrollContent` `bucketPush` block + `laidOutBus` call + `isAlwaysVisibleBus` — `.cpp:4069-4152, 4239-4382, 3167-3177`
32. `clearDynamicStrips` — `.cpp:2731`
33. 30 Hz `_sendTo` change scan — `.cpp:3799-3827`
34. `onVBlank` `drainStereoInsert` / `drainStereoBus` — `.cpp:3854-3926`
35. `getStemPickEntries` (feeds the "+" menus **and** the stems dialog) — `.h:146-152`; `StripKind`/`renameChannel` — `.h:76`, `.cpp:3375`; `OrderKind` — `.h:239`

**E. `Source/Standalone/EffectsPage.cpp` (2 sites)**
36. `channelToMixerId` dropdown-id range — `:353-366`
37. `addBusAndMembers` group row — `:415-478`

**F. Project persistence — `Source/Standalone/StandaloneEditor.cpp` (1 site)**
38. `<UIState>`: `mixerScrollX` (:14943/:18916), `AuxNames`/`VoxNames`/`InstNames` + orders (:15306-15314, :18984-18990), `<Buses>` active + ever-routed flags (:15340, :19022)

That is **38 concrete sites across 7 files**; the "~15" in memory is the coarse grouping (A–F above collapse to roughly that count).

---

## 8. Gap list vs the four FL-style properties

### P1 — "a bank of inserts that already exist"
| | |
|---|---|
| **Exists** | Aux strips are already exactly this shape: no engine, exist independently, `PassiveStripTask::Kind::Aux` sums predecessors then runs the full insert chain (`PassiveStripTask.cpp:65-71`); cap 18 (`BaySickGraph.h:102`); lazily created but with a stable index and preserved params. Bus strips + Master are pre-created eagerly (params `PluginProcessor.cpp:9065`, nodes `BaySickGraph.cpp:1091`, tasks `PluginProcessor.cpp:879-905`, routing entries `BaySickGraph.cpp:3174`). The flat `mInsertsByChannel[1000]` array and `mTasksByChannel[1000]` already support dense pre-allocation with no lookup cost. |
| **Missing** | Nothing pre-creates N inserts at startup: every insert is created by a tab/engine event (§1c table). `mNextAuxIdx` (`MixerPage.h:436`) is a monotonic counter, not a bank. UI grouping is by *destination bucket*, not by a fixed bank order (`MixerPage.cpp:4062-4076`). Aux cap 18 < a typical FL bank of ~100. Aux strip widgets use `StripType::Aux` with no arm/listen row. |
| **Files that change** | `BaySickGraph.h` (aux cap or a new "Insert" family + all 8 MixerChannelIds sites), `PluginProcessor.cpp` (`ensureAuxInsert` → eager loop in `prepareToPlay` beside `kBusChannelIds`), `MixerPage.cpp` (`addAuxChannelAtIndex`, `layoutScrollContent`, `clearDynamicStrips`), `EffectsPage.cpp:353-366`. |

### P2 — "any hosted instrument routes to any insert"
| | |
|---|---|
| **Exists** | The engine→strip plumbing is generic and already carries a hosted VST3 unchanged (`registerPluginEngine` `PluginProcessor.cpp:8167-8186`). `RenderGraphDispatcher::registerTask/unregisterTask` can rebind a task to a channel at runtime (`:48-132`). |
| **Missing** | The instrument→strip binding is `chId = base + pageIndex`, computed at registration and never changed (§4). Task→arena binding is one-shot at register (`:97`). The strip's params are under a prefix derived from that same index, so "move instrument to insert 7" would move its fader/pan/rack too unless the model separates *instrument identity* from *strip identity* — which it currently does not. No `_targetInsert`-style param exists anywhere in `addParamsForMixerStrip`. `MixerPage::StripKind` and `pickStripColor` also key off family+index. |
| **Files that change** | `PluginProcessor.cpp` (`register*Engine` family, a new instrument→chId indirection), `Engine/Tasks/EngineInsertTask.*` (channelId becomes mutable / task re-registration), `RenderGraphDispatcher.cpp:48-132`, `BaySickGraph.h` MixerChannelIds (new binding param), `MixerPage.cpp` (strip identity + naming + `onChannelRenamed`). |

### P3 — "any insert can route into any other insert (a bus) with a level"
| | |
|---|---|
| **Exists** | Aux→aux main-out is already legal and works (`MixerPage.cpp:567-568`, `PassiveStripTask` sums). Levelled routing exists as sends: 4 per strip, `−60..+6 dB` + pre/post (`PluginProcessor.cpp:9005-9011`), consumed at `PassiveStripTask.cpp:45-47`. Cycle detection is already generic over arbitrary src/dst including SC (`BaySickGraph.cpp:2658-2680`), and topo sort drops cycles safely (`:2789-2857`). `RoutingGraph` imposes no bus/insert distinction at all. |
| **Missing** | (a) **Main-out edges are unity-gain by construction** — `e.amountDb = 0.f` at `BaySickGraph.cpp:2748` and `link.isMainOut ? 1.0f` at `MasterTask.cpp:54` / `PassiveStripTask.cpp:45`; a levelled insert→insert route needs either a level on main-out lines or sends unrestricted. (b) **Sends are aux-only** (`BaySickGraph.h:219-222`, `MixerPage.cpp:711`). (c) **`isRouteAllowed` is a per-family whitelist of bus destinations** (`MixerPage.cpp:482-587`) — 10 hard-coded branches. (d) **The receiving side does not exist for non-Aux/Bus inserts**: `EngineInsertTask`/`Composite`/`Inst`/`Vox`/`Rusty`/`Direct` clear their buffer and never read `mPredecessors` (§2d) — an insert→insert edge today is built, ordered, and silently dropped. (e) Layout buckets assume "destination is a bus" (`laidOutBus` `:4239`, aux-to-aux is special-cased at `:4328-4332`). |
| **Files that change** | `Source/Engine/Tasks/*.cpp` (add the predecessor-sum prologue to every insert task, or make every insert use a Passive-style shell), `BaySickGraph.cpp:2733-2760` (level on main-out lines), `BaySickGraph.h:219` (`isValidBusSendTarget`), `MixerPage.cpp:482-587` (`isRouteAllowed`), `MixerPage.cpp:4033-4395` (bus-derived layout), `Engine/UpstreamLink.h` if a new link flavour is needed. |

### P4 — "sidechain flags, and live audio input as a per-insert property"
| | |
|---|---|
| **Exists (sidechain)** | Complete and already strip-generic: 4 receive lines per **every** strip (`PluginProcessor.cpp:9019-9023`), target-side encoding, per-rack-slot pick `Slot::scPick` (`EffectRack.h:169, 283`) and per-EQ-band `scSourceId` (`DSP/DSPBase.h:110`), delay-matched key taps (`BaySickGraph.cpp:2954, 2978`), engine-level SC via `ISidechainEngine` (`DSP/EngineSidechainHelper.h:13-19`), participation in cycle detection and topo order (`:2662-2677, 2801-2803`). |
| **Exists (live input)** | `_inputChannelIdx` / `_inputChannelStereo` / `_listen` / `_monitorMode` params, the input picker UI, arm/listen LEDs, and a per-block `liveInputSnapshot` in `BlockContext` (`Engine/BlockContext.h:62`) that any task can read by channel index (`InstStripTask.cpp:87-99`). |
| **Missing (live input)** | It is **not a per-insert property** — it is gated three ways on the Vox/Inst families: params only registered for those two kinds (`addLiveInputParams` called only at `PluginProcessor.cpp:9318, 9330`); `_arm` only registered when the prefix startsWith `mixer_vox_`/`mixer_inst_` (`:8979-8983`); UI arm/listen rows only exist on `StripType::Vox`/`Inst` (`MixerTrackStrip.cpp:361-391`, `hasUtilityRow()`); and the capture path lives inside `VoxStripTask`/`InstStripTask` rather than in a shared insert shell. `setNoLiveInput` is a *suppression* flag, not an enablement one (`MixerTrackStrip.h:189`). |
| **Files that change** | `PluginProcessor.cpp:8916-9038` + `addLiveInputParams` (make it unconditional, or gated by a per-strip flag), `MixerTrackStrip.{h,cpp}` (`StripType`, `hasUtilityRow`, `setApvts` gating), `MixerPage.cpp:2452-2596` (`showInputChannelPicker`, `refreshLiveInputStrip`), `Engine/Tasks/` (the input-read prologue currently duplicated in `VoxStripTask.cpp:61-99` / `InstStripTask.cpp:68-99` would move into the shared insert task). |

### P5 (context extra) — "UI tabs derived from BUSES"
Today the mixer already derives its **grouping** from buses (`layoutScrollContent` buckets by `_sendTo`, `:4060-4076`), and `EffectsPage` already builds a bus→members tree from the mixer (`EffectsPage.cpp:415-478` `addBusAndMembers`, fed by `MixerPage::getStemPickEntries` / `getXxxStripIndices`). The **ribbon tabs**, by contrast, are per-engine-instance (`MixerPage::StripKind`/`onChannelRenamed` sync a strip to an existing tab, `MixerPage.h:76-85`; strips are created *by* tab/engine events, §1c). So the bus→members enumeration the fork wants already exists in two places and is reusable; what is missing is the tab source-of-truth inversion, which lives outside this subsystem (`RibbonTabBar`, `EngineRig`, `StandaloneEditor`).

---

## Cross-cutting notes for the fork

- Removing every built-in engine removes the Layer/Bass/Drum/Rusty/Vox/Inst **families**, not the strip machinery: `Aux` + `Bus` + `Master` + `Plugin` + `Audio` + `Direct` are the kinds with no BaySick* engine dependency. `Aux` is the only existing kind that is a pure insert with no source at all.
- `mixer_*` prefixes and the 0..999 chId space are **frozen persistence** (`BaySickGraph.h:200-208`, `PluginProcessor.cpp:8993-8994`): renumbering families breaks saved projects.
- `MixerState` (legacy undo snapshot) still backs Master/Layers/Bass/Drums/Clips bus level/pan/mute/solo + per-drum and per-audio-row values (doc `Mixer.md:336-341`); it is a second, partial model of the mixer that a fork inherits unless removed (`applyMixerSnapshot`/`syncApvtsFromMixerState`, `MixerPage.h:605-608`).
- `kMaxStripChannels = 1000` is the ceiling on any bank size and is used as an array size in three places (`BaySickGraph.h:1078-1079, 1190-1191, 1220`; `RenderGraphDispatcher.cpp:23, 75`).

---

# READER 3: VST3 hosting in BaySickDAW — factual survey

## VST3 hosting in BaySickDAW — factual survey

Docs read first: `Plans & Specs/System Reference/Plugins Page.md`, `Effect Racks.md`, `INDEX.md`, `Workspace and Windows.md`, `Freeze and Export.md`. Everything below was then confirmed in code; discrepancies between doc and code are called out.

---

## 1. The Plugins tab, end to end

### 1.1 Format scope + allowlist ("added list")

| Fact | Evidence |
|---|---|
| VST3 only, hard-coded (not `addDefaultFormats()`), by licensing review CL-303 | `Source/Hosting/PluginManager.cpp:38-41`; `PluginManager.h:15-19` |
| Three-part model: scan folders / added list / scan results | `PluginManager.h:60-104` |
| Added list is `juce::KnownPluginList mAdded`, persisted to `Documents\BaySickDAW\plugins.xml` | `PluginManager.h:149`; `PluginManager.cpp:613-637`, `:566-611` |
| `getAddedEffects()` = `!isInstrument`; `getAddedInstruments()` = `isInstrument`. Both alphabetical | `PluginManager.cpp:157-177`, sort at `:133-142` |
| **An instrument VST3 cannot be loaded into a rack slot and an effect VST3 cannot be a Plugins tab** — the two pickers read disjoint filtered lists | `SlotComponent.cpp:835` (`getAddedEffects`), `RibbonTabBar.cpp:605,881` + `PluginsPage.cpp:629` (`getAddedInstruments`) |
| Manager window (Options > Plugins) | `Source/Standalone/PluginsManagerWindow.h/.cpp` (added list at `:346`) |
| Allowlist is also a **security boundary**: a project blob's `PluginDescription.fileOrIdentifier` must match a path on the added list or the load is refused and reported through `MissingFileReport` | `HostedPlugin.cpp:675-719` (comment at `:690-707`) |

### 1.2 Scan

- VST2 pass: any `*.dll` under a scan folder is listed as `"Skipped: VST2 is not supported"` — `PluginManager.cpp:414-437`.
- Architecture split **before** loading (PE machine field / bundle `Contents/<arch>-win`) — `PluginManager.cpp:439-472`, `PluginManager.h:131`. 32-bit is *intake, not skip*: a stub description is synthesised from the filename with `isInstrument=false` (`:459-468`).
- Every 64-bit candidate is loaded in a throwaway helper via `BridgedPluginScanner` installed as `KnownPluginList::CustomScanner` — `PluginManager.cpp:495-504`; scanner class `Source/Hosting/OutOfProcessScanner.h:39-176`, one helper per file (`:34-37`), 30 s per-file timeout (`:171-173`), helper-missing falls back to in-process (`:163-167`).
- Failure reasons published: `"Skipped: failed to load"` (`PluginManager.cpp:524-525`), `"Skipped: crashed during a previous scan"` (`:527-528`). Dead-man's-pedal file `plugins_scan_crashes.txt` at app root — `PluginManager.cpp:30`.
- Scan runs on `PluginManager`'s own `juce::Thread`; UI notified via `AsyncUpdater` — `PluginManager.h:53-55,133-135`; `:555-562`.

### 1.3 Out-of-process helper + protocol

- **Protocol version = 6** — `Source/Hosting/PluginBridgeProtocol.h:62`. Handshake id `"BaySickPluginBridge"` (`:67`). Version mismatch → helper replies error and quits (`Helper/PluginHostMain.cpp:106-113`).
- Message set `PluginBridgeProtocol.h:69-95`; `Process`/`ProcessReply` are **reserved since v3** — per-block audio/MIDI/transport/reply ride a named shared mapping (`BridgeSharedMemory.h`, `SharedBlockControl`) with two auto-reset events.
- Fixed-width packed structs, `static_assert`ed sizes, no pointers on the wire — `PluginBridgeProtocol.h:97-189`.
- Audio thread never blocks on the pipe: `SandboxedPluginClient::processBlock` memcpys into the mapping, serialises MIDI by hand `(int32 pos, int32 nBytes, bytes)` capped at `kMaxMidiBytesPerBlock = 4096`, signals, and waits — `SandboxedPluginClient.cpp:245-388`; cap at `PluginBridgeProtocol.h:124`, overflow silently truncates (`SandboxedPluginClient.cpp:290-291`).
- Deadlines: live = 75 % of this block's period, floor 1 ms (`SandboxedPluginClient.cpp:30,36,239-243`); offline = 30 000 ms per block with a one-shot `mOfflineWedged` latch (`:43-51,310-311,371-372`). A miss → `false` → caller clears the buffer (`HostedPlugin.cpp:555-556`). Resync drain so one miss doesn't mute the slot forever (`:329-340`).
- Startup/handshake timeout 15 s, then the pipe is re-pointed to a 5 s write timeout, with a `mSendWedged` latch so a stuck helper costs the UI once — `SandboxedPluginClient.cpp:53-71,126-138,169-179`.
- Helper is a JUCE GUI app; its audio loop is a dedicated `THREAD_PRIORITY_TIME_CRITICAL` thread — `Helper/PluginHostMain.cpp:358-364,373-450`.

### 1.4 How a hosted instrument gets MIDI (`EngineKind::Plugin` = target kind 10)

- `PianoRollPage::EngineKind` ordering — `Source/Standalone/PianoRollPage.h:38` (Plugin is the 11th, ordinal 10).
- Scheduled (pattern/song) notes: `mPluginPageMidi[20]`, fed by the scheduler at `PluginProcessor.cpp:3398,3456`, all-notes-off at `:2900`, note-offs at `:2887,2933,3040`.
- Live hardware MIDI: one global collector, one global target — `PluginProcessor.cpp:3583-3595`, kind 10 → `pluginPageMidi[idx]` (`:3594`). Target set by `setLiveMidiTarget` from Piano Roll focus — `PluginProcessor.h:653-657`.
- Transport-sync messages (clock/start/stop/continue/SPP/active-sense/quarter-frame) are filtered out of the engine path specifically because hosted plugins act on them — `PluginProcessor.cpp:3640-3649`.
- Roll keyboard / grid audition for a plugin tab goes through the same live collector (there is no `auditionNote` on a hosted plugin) — `StandaloneEditor.cpp:10988-11044`.
- The task hands the buffer to the engine — `Engine/Tasks/EngineInsertTask.cpp:47,114-120`.

### 1.5 Audio out + mixer strip

- `HostedPluginInstance` **is** a `juce::AudioProcessor`, declared stereo-out / stereo-in-only-when-effect — `HostedPlugin.cpp:41-50`.
- Registration creates the strip params, the `InsertNode` and the `EngineInsertTask` — `PluginProcessor.cpp:8163-8186` ("byte-for-byte the Layer shape").
- Identity: `kMaxPluginPages = 20` (`Source/BaySickConstants.h:25`), channel ids 900-919 (`BaySickGraph.h:78,131`), APVTS prefix `mixer_plugin_<i>` (`BaySickGraph.h:166`), default parent bus `kPluginsBus = 13` (`BaySickGraph.h:57,299`), second bus `kPluginsBus2 = 17` (`:64`).
- `EngineRig` special-cases Plugins in create / register / unregister — `EngineRig.cpp:589-620, 712-724, 770`.

### 1.6 Editor window

- `HostedPluginInstance::createEditor()` returns `nullptr` and `hasEditor()` is `false` **deliberately** — surfaces build a plain `Component` (`Hosting::HostedPluginEditor`) instead — `HostedPlugin.h:96-115`, rationale `:100-107`.
- Page builds it and fits the window to the plugin's declared size, and **sets `setUserResizable(false)`** — `PluginsPage.cpp:249-293` (`:270` the false). Rack-slot window does the same — `EffectWindows.cpp:144-145`.
  - **Doc/code discrepancy:** `Plugins Page.md:87-91` describes three user-drag resize behaviours; the code makes a hosted-plugin window non-user-resizable on both surfaces. The `kMinUsableScale = 0.5f` floor logic (`HostedPlugin.h:331`, `PluginsPage.cpp:283-289`, `EffectWindows.cpp:157-165`) still exists.
- Bridged editor: host supplies a native child peer, sends the HWND as `uint64`, helper does `addToDesktop(..., parent)` and replies with the real size — `HostedPlugin.cpp:1086-1116`; `PluginHostMain.cpp:498-534`; `SandboxedPluginClient.h:98-99,142`.
- Native-child window design is *because of* foreign plugin HWNDs — `Workspace and Windows.md:24-42`.
- Dead plugin keeps the window and paints a marker; `~HostedPluginInstance` tells the editor to drop the plugin editor first — `HostedPlugin.cpp:53-68, 794-818`.

### 1.7 State save / restore

- `getStateInformation` writes `<HostedPlugin bridged=… blob=base64>` + the full `<PLUGIN>` description child — `HostedPlugin.cpp:648-673`. A dead instance falls back to `mLastKnownState` so a save never erases settings (`:661-667`, member `HostedPlugin.h:252-261`).
- `setStateInformation` restores the bridge preference and **re-instantiates** if the mode differs, then pushes the blob — `HostedPlugin.cpp:721-763`.
- Rack slot delegates both ways and reports an in-process load failure to `MissingFileReport` — `HostedPluginEffect.cpp:181-217`.
- Tab persistence: the plugin's `createIdentifierString()` is the tab's `engineType`, so it rides existing tab XML — `EngineRig.cpp:584-620`; `PluginsPage.h:39-41`, `PluginsPage.cpp:142-166`.
- Restore-time resolution order: `findAdded(identifier)` then a single-use stashed description — `EngineRig.cpp:590-620`.

### 1.8 Automation of hosted parameters

- Lanes key on the plugin's **stable parameter id** (`HostedParameter::getParameterID`), never index — `HostedPlugin.h:144-156`, `HostedPlugin.cpp:231-303`.
- Tab lane id = `plugtab<N>_vst_<paramId>` — `StandaloneEditor.cpp:15882`, prefix at `:17549`, display-name resolution at `:4162-4193`, creation from the Menu at `:5749, 12762, 18294`.
- Rack lane id = `<channelPrefix>_<slotUuid>_vst_<paramId>` — `EffectsPage.cpp:735-760`; menu at `EffectWindows.cpp:273-347`.
- Bridged parameter lists arrive **after** load, so registration re-arms on `onParamListArrived` — `HostedPlugin.h:173-178`; `StandaloneEditor.cpp:15865-15874`; `EffectsPage.cpp:748-760`.
- `applyParamNorm` writes via `setValueNotifyingHost` in-process, via index-resolved `SetParameter` when bridged — `HostedPlugin.cpp:278-303`.
- **`readParamNorm` returns the fallback for a bridged plugin** (values are not cached host-side) — `HostedPlugin.cpp:305-311`.
- "Last touched": in-process listener with a 1 s state-apply suppression (`HostedPlugin.cpp:207-222`); bridged v5 relay with host-echo and state-storm suppression (`PluginHostMain.cpp:248-287`).
- **Hosted plugin parameters cannot be MIDI-learned**: `MidiLearnRegistry::dispatchEvent` only resolves ids through the main APVTS — `Source/MidiLearn/MidiLearnRegistry.cpp:154,183`; `PluginProcessor.cpp:3693-3699`.

### 1.9 Latency / PDC

- In-process: `setLatencySamples(mInner->getLatencySamples())` at prepare — `HostedPlugin.cpp:495-497`. Bridged: from `LoadReplyPayload.latencySamples` — `HostedPlugin.cpp:404-408`; `PluginHostMain.cpp:303`.
- Rack slots: summed by `EffectRack::getTotalLatencySamples()` (skips bypassed, 0 when rack bypassed) and consumed by `BaySickGraph::updateBusLatencies` — `EffectRack.cpp:764-772`; `BaySickGraph.cpp:1596-1601,1686`.
- **Gap:** the *engine-side* latency of a hosted instrument is **not** in PDC. `updateBusLatencies` adds an `engineLat` term only for Vox and Inst strips — `BaySickGraph.cpp:1681-1686`; the only two hooks are `onGetVoxStripChainLatency` / `onGetInstStripEngineLatency` (`BaySickGraph.h:579,585`, wired `PluginProcessor.cpp:842,853`). A `mixer_plugin_<n>` strip contributes `chainLat(preEq, rack, eq)` only.
- Bridged latency is read once from `LoadReply`; nothing re-reads it if the plugin changes latency later (inferred — no `latencyChanged` message exists in `PluginBridgeProtocol.h:69-95`).

### 1.10 Sidechain

- Discovered at prepare: first enabled input bus after bus 0; `-1` = none — `HostedPlugin.cpp:472-492`; predicate `HostedPlugin.h:65-74`.
- Fed only through the **rack** adapter (`getActiveSidechain()` per block) — `HostedPluginEffect.cpp:54-68`; copied into the wide scratch at the resolved offset — `HostedPlugin.cpp:596-613`.
- Rack UI shows the `SC:` row only when `usesSidechain()` — `HostedPluginEffect.h:49-56`; `SlotComponent.cpp:260,587`.
- **Bridged plugins never report a sidechain**: the sandbox branch of `prepareToPlay` returns before discovery — `HostedPlugin.cpp:402-409`; the helper prepares `setPlayConfigDetails(mChannels, mChannels, …)` = 2/2 — `PluginHostMain.cpp:346`.
- **A hosted instrument on a Plugins tab gets no sidechain into the plugin at all**: `HostedPluginInstance` does not implement `ISidechainEngine`, and `EngineInsertTask` only pushes SC buffers into engines that do — `HostedPlugin.h:42-43`; `EngineInsertTask.cpp:81-88`.

### 1.11 Freeze / offline

- Plugins tabs are freezable — `Freeze and Export.md:118`; freeze substitution is generic in `EngineInsertTask.cpp:112-120`.
- Freeze staleness for a hosted plugin uses the `AudioProcessorListener` path, not APVTS (a hosted plugin has no APVTS) — `EngineRig.cpp:387-410`.
- `setNonRealtime` crosses the seam both ways — `HostedPlugin.cpp:522-528`; rig engines swept at `PluginProcessor.cpp:7971-7986`; rack slots swept separately because a rack slot is a `DSPBase`, not a rig engine — `PluginProcessor.cpp:7987-7989` → `BaySickGraph.cpp:2519` → `HostedPluginEffect.cpp:112-127`.
- Playhead forwarding (tab: `HostedPlugin.cpp:512-520`, rig sweep `PluginProcessor.cpp:2794`; rack slot builds its own `RackPlayHead` — `HostedPluginEffect.h:113-122`, `.cpp:83-107`; bridged: plain values in the control header — `SandboxedPluginClient.cpp:316-321`, rebuilt in `PluginHostMain.cpp:405-412`).
- Revival preserves freeze and re-publishes the frozen source — `EngineRig.cpp:436-533` (freeze re-publish `:519-529`).

---

## 2. Hosted VST3 **effects** in rack slots

| Aspect | Fact | Evidence |
|---|---|---|
| Slot type | `EffectType::VST3Plugin = 121`, ordinals pinned/append-only | `EffectRack.h:79` |
| Factory | builds an **empty** `HostedPluginEffect`; identity arrives from `setPlugin()` or the state blob | `EffectRack.cpp:88-90`, `:155-160`; `HostedPluginEffect.h:16-22` |
| Instantiation | outside every lock; outgoing DSP destroyed on the message thread outside locks | `EffectRack.cpp:146-190`; `Effect Racks.md:361-367` |
| Picker | "VST Plugins" group from `getAddedEffects()`, shown-but-disabled when empty | `SlotComponent.cpp:820-856` |
| Name / `(missing)` | asked live from the instance | `SlotComponent.cpp:1338-1370` |
| Editor | `HostedPluginEditor` is the one non-`EditorPanelBase` case | `EffectEditorPanels.cpp:7386-7394` |
| Window menu | Automate (chunks of 30) / Run bridged / Retry Loading Plugin | `EffectWindows.cpp:269-383` |
| Bridge toggle | **only exists here**, not on a Plugins tab; 32-bit row shown disabled with reason; applies on next load | `EffectWindows.cpp:347-370`; `HostedPlugin.cpp:353-366`; noted as an accident of surface growth in `EngineRig.cpp:477-484` |
| State | rack `<Slot data=base64>`; per-slot uuid keys lanes | `Effect Racks.md:285-309`; `EffectRack.cpp:775-800` |
| Latency | summed into rack total → bus PDC | `HostedPluginEffect.cpp:129-134`; `EffectRack.cpp:764-772` |
| Sidechain | per-slot `scPick` → plugin's SC input bus; in-process only | `HostedPluginEffect.cpp:61-67`; `HostedPlugin.cpp:596-613` |
| Retry | `EffectsPage::retryDeadPluginSlot`, audio-shield + settle, same uuid so lanes/windows survive; probes the file exists first | `EffectsPage.cpp:899-1000` |
| Known hole, written down in-tree | on retry, `loadEffect` instantiates **before** the blob is read, so a 64-bit plugin the user bridged for stability is still loaded in-process during the retry | `EffectsPage.cpp:970-980` |
| Dead behaviour | in-process = pass-through, bridged = silence for that slot | `HostedPlugin.cpp:561-569`; `Effect Racks.md:249-256` |

---

## 3. Multi-output instruments

- **Not supported as multiple outputs.** The wrapper declares exactly one stereo output bus — `HostedPlugin.cpp:41-46`.
- What exists is a *crash fix*, not routing: at prepare the inner plugin's buses are all enabled and a wide `mMultiOutScratch` is allocated so every bus has real storage (`HostedPlugin.cpp:439-470`, rationale `:414-438`); at process the plugin renders into the wide buffer and **only bus 0's channels are copied to the strip** (`:615-621`).
- If the scratch is too short for a block, the block is cleared (`:626-632`); if `mInnerOutChannels > buffer channels` on the narrow path, the block is cleared (`:639-643`).
- One `AudioProcessor` per tab → exactly one `InsertNode` / one mixer strip — `PluginProcessor.cpp:8167-8186`. Nothing anywhere creates a strip per plugin output.
- Bridged: the helper prepares the plugin `setPlayConfigDetails(mChannels, mChannels, …)` with `mChannels` = the host's 2 — `PluginHostMain.cpp:324-347`. No bus-count backing exists helper-side (inferred: a multi-out plugin bridged would hit the same null-bus hazard the in-process path fixes, isolated to the helper).

---

## 4. MIDI questions

| Question | Answer | Evidence |
|---|---|---|
| Two instruments sharing a MIDI input? | Not for **live** MIDI. There is one collector and one `(kind,index)` live target set by Piano Roll focus. Only the focused engine receives hardware notes. | `PluginProcessor.h:640-657`; `PluginProcessor.cpp:3579-3595`; `StandaloneApp.cpp:1367-1380` |
| Per-track MIDI input device? | No. All enabled devices register one empty-identifier callback and merge into the single collector. | `StandaloneApp.cpp:1184-1199` |
| MIDI **channel** filtering? | None anywhere on the live path — messages are forwarded verbatim except the transport-sync class. Scheduled roll notes are always emitted on channel 1. | `PluginProcessor.cpp:3640-3665`; note-offs built as `noteOff(1, …)` at `:2887,2900,2933,3040` |
| MIDI **out** from a plugin? | No. `producesMidi()` is hard-coded `false`; the rack adapter clears a scratch buffer every block and nothing reads it back; the bridge protocol has no helper→host MIDI message. | `HostedPlugin.h:127`; `HostedPluginEffect.h:124-126`, `.cpp:59-67`; `PluginBridgeProtocol.h:69-95` |
| Plugin-to-plugin MIDI? | No path exists. The per-tab buffer is cleared each block (`PluginProcessor.cpp:2857`) and there is no MIDI routing surface. | as above |
| `acceptsMidi` | `mDesc.isInstrument` — a hosted **effect** never receives MIDI. Bridged loads report the plugin's real `acceptsMidi` back but only `isInstrument` is consumed | `HostedPlugin.h:126`; `HostedPlugin.cpp:119-137` (`ignoreUnused(midi)` at `:121`) |
| MIDI Learn onto plugin params | Not possible (APVTS-only registry) | `MidiLearnRegistry.cpp:154,183` |

---

## 5. Presets for hosted plugins

- **No `.vstpreset` / `.fxp` / `.fxb` support anywhere** in the tree (repo-wide grep over `Source/`, `Plans & Specs/`, docs: zero hits).
- No host-side program browser. `getNumPrograms` / `getCurrentProgram` / `getProgramName` exist purely for the **naming linkage** — `HostedPlugin.cpp:373-400`; `PluginsPage.cpp:106-125`.
- `setCurrentProgram` is in-process-only and **has no caller** — `HostedPlugin.cpp:387-392` ("nothing host-side SETS programs today, the linkage is read-only"); repo grep confirms no call site.
- Bridged program info is one-way, current program only (v4) — `PluginBridgeProtocol.h:47-51,162-168`; `PluginHostMain.cpp:236-246,289-296`.
- What *is* offered instead: **page presets** for a Plugins tab (plugin identifier + state blob + `mixer_plugin_<n>` params + rack + EQs) — `PluginsPage.cpp:649-670, 730-775`; and **FX rack presets** / per-effect preset menu that store the plugin's opaque blob — `Effect Racks.md:325-336`; `EffectPresetIO.cpp:78` maps the type to a `"VST3 Plugins"` folder.

---

## 6. The 32-bit bridge

- `isBridgeForced()` ⇔ `PluginArch::X86`; 64-bit honours a per-plugin preference; a forced-bridge start failure is terminal (`HostedState::NeedsBridge`), a 64-bit failure falls back in-process — `HostedPlugin.h:82-94`; `HostedPlugin.cpp:100-167`.
- Helper resolution: `BaySickPluginHost32.exe` / `BaySickPluginHost64.exe` beside the running exe — `SandboxedPluginClient.cpp:102-112`.
- Build: 64-bit helper is a normal root target (`CMakeLists.txt:847-906`, staged next to the app at `:893-906`); 32-bit is a **separate CMake project** configured `-A Win32` into `build32` with no vendored deps — `Source/Hosting/Helper/CMakeLists.txt:1-68`; driven by `do_build.bat:237,265,272-273` and artifact-checked at `:285-288`.
- Installer ships both — `Installer/BaySickDAW-Tester.nsi:20,71-72`; `make_installer.bat:75-76`.
- Bridge preference is per-instance, stored inside the plugin's own state blob (travels with the project, not per-machine) — `HostedPlugin.cpp:651`, `:733-747`; `Plugins Page.md:145`.
- 32-bit metadata is a filename-only guess until the first bridged load corrects it via `refineDescription` — `PluginManager.cpp:339-367`; `HostedPlugin.cpp:130-136`.
  - Consequence (confirmed by reading the two filters): a 32-bit VST3 enters the added list as `isInstrument = false` (`PluginManager.cpp:467`), so it appears **only in the FX-rack picker** and never in the "+" instrument menu until something loads it. The correcting load can only happen through a rack slot.
  - Second consequence (inferred): `createIdentifierString()` embeds `name` and the uid (`juce/modules/juce_audio_processors_headless/processors/juce_PluginDescription.cpp:64-68`), so refinement changes the identifier — a previously-saved `engineType` for that plugin can miss `findAdded` and depend on the stash fallback.
- The bridge is explicitly **not** a security sandbox: "a plain CreateProcess with no job object or token restriction" — `HostedPlugin.cpp:699-701`.
- Untested in practice: "bridged-specific `1cd1f5d6` relays are UNTESTED (no 32-bit VST3 on hand)" — `Plans & Specs/Main Plan.md:1998`.

---

## 7. TODO / FIXME / "not supported" / limitations near hosting code

Literal markers (there are almost none — the codebase encodes limitations as prose comments):

- `"Skipped: VST2 is not supported"` — `PluginManager.cpp:435`, `PluginManager.h:44`.
- Repo grep for `TODO|FIXME|HOLD-FOR|not implemented|unsupported` across `Source/Hosting/**`, `PluginsPage.*`: **no other hits.**

Limitations stated in comments/docs (each is code-confirmed above):

1. VST2 hosting deliberately excluded, SDK unlicensable — `PluginManager.h:15-19`; Future State `CL-303` (`Future State.md:230`).
2. Bridge toggle exists only on the FX-rack slot window; a 64-bit tab instance is pinned in-process — `Plugins Page.md:32`; `EngineRig.cpp:477-484`; `EffectWindows.cpp:347-370`.
3. Bridged plugins never report a sidechain input — `Effect Racks.md:196-202`; `HostedPlugin.cpp:402-409`.
4. Bridged parameter **reads** fall back to a constant — `HostedPlugin.cpp:305-311`.
5. Bridged surfaces cannot be scaled (an `AffineTransform` doesn't reach a native peer) — `HostedPlugin.h:310-335`.
6. Bridged program relay is current-program-only; no program list sync — `PluginBridgeProtocol.h:162-168`; `HostedPlugin.h:130-134`.
7. In-process "last touched" can briefly follow a playing automation lane (no cheap way to tell the echo apart) — `HostedPlugin.cpp:207-215`.
8. `setCurrentProgram` has no bridged relay and no caller — `HostedPlugin.cpp:387-392`.
9. Retry of a bridged rack slot still loads the plugin in-process first — `EffectsPage.cpp:970-980`.
10. Multi-out plugins render into scratch and only bus 0 reaches the strip — `HostedPlugin.cpp:571-621`.
11. `SetDefaultDllDirectories` hardening is reverted pending a plugin-hosting test — `StandaloneApp.cpp:1392-1420`.
12. Bridged MIDI is capped at 4096 bytes/block, excess dropped — `SandboxedPluginClient.cpp:290-291`.

**Future State entries on plugin hosting** (`Plans & Specs/Future State.md`) — all marked SHIPPED except one:

| Entry | Line | Status |
|---|---|---|
| BLU-297 VST3 hosting umbrella | `:222` | SHIPPED v1 |
| BLU-298 scanner | `:223` | SHIPPED (32-bit intake since `93bb158e`) |
| BLU-299 browser UI | `:224` | SHIPPED |
| BLU-300 `EffectType::VST3Plugin` rack slot | `:225` | SHIPPED |
| BLU-301 latency reporting | `:226` | SHIPPED "by construction through the existing rack latency sum + bus PDC" |
| BLU-302 sub-process hosting | `:227` | SHIPPED (x64 + x86, protocol v3, 4 ms→fractional deadline) |
| **CL-303 VST2 hosting** | `:230` | **RESEARCHED AND DELIBERATELY NOT BUILT** — SDK header + distribution blockers, clean-room-header route documented and rejected |
| BLU-447 VST3 instrument hosting (§P8) | `:441-442` | SHIPPED (TS6 creation half + TS7 consumption half, 41 defects) |
| LDT-219 / LDT-423 VST/AU hosting | `:785-786` | SHIPPED as VST3-only; AU never (Windows-only), VST2 excluded |
| LDT-424 VST instrument builds (our engines as VSTs) | `:789` | separate, not hosting |
| CL-290 crash-report pipeline | `:682` | parts (2)/(4) not built — relevant because plugin crashes are the main crash source |

Other doc-vs-code discrepancies found (facts, not recommendations):

- `Plugins Page.md:110` says a Plugins tab has "no Lock, no Rename and no Duplicate on this menu" and `:85` says one plugin per tab for its life. The code (Jeff, 2026-08-16) added **Rename… / Replace Plugin / Duplicate Plugin (new tab)** to that menu, and Replace swaps the plugin in place inside a `performChainSwapGesture` undo transaction — `PluginsPage.cpp:608-643, 741-750`.
- `Plugins Page.md:87-91` resize behaviours vs `setUserResizable(false)` (see §1.6).

---

## 8. What is complete vs missing if hosting were the ONLY instrument/effect path

**Complete (no work implied by the code as it stands):**

| Capability | Where |
|---|---|
| Scan, allowlist, skip-with-reason, crash isolation, blacklist self-clearing | `PluginManager.cpp:396-553`; `OutOfProcessScanner.h` |
| Instrument as a first-class engine (strip, roll, rack, EQs, freeze, page preset, undo, rename/replace/duplicate) | `PluginProcessor.cpp:8167-8186`; `EngineRig.cpp:589-620`; `PluginsPage.cpp` |
| Effect in any of the 6 rack slots on any strip or bus | `EffectRack.cpp:88-90,146-190` |
| Editor hosting for both surfaces, in-process and bridged, self-resize tracking, dead-marker | `HostedPlugin.cpp:768-1273`; `PluginsPage.cpp:249-293`; `EffectWindows.cpp:130-170` |
| State in project + presets, survives a dead plugin, added-list-checked on restore | `HostedPlugin.cpp:648-763`; `HostedPluginEffect.cpp:181-217` |
| Automation by stable param id, live + offline, bridged async list handling | `HostedPlugin.cpp:244-311`; `StandaloneEditor.cpp:15857-15904`; `EffectsPage.cpp:735-790` |
| Transport/playhead both paths; offline/non-realtime both paths | `HostedPlugin.cpp:512-528`; `HostedPluginEffect.cpp:83-127`; `PluginProcessor.cpp:7971-7989` |
| Rack-slot sidechain into the plugin's SC bus (in-process) | `HostedPlugin.cpp:472-492,596-613` |
| Rack latency → bus PDC | `EffectRack.cpp:764-772`; `BaySickGraph.cpp:1596-1601` |
| Crash/miss containment (per-slot silence, never an audio stall) | `SandboxedPluginClient.cpp:245-388` |
| 32-bit path built, staged and installed | `Helper/CMakeLists.txt`; `do_build.bat:237-288`; `.nsi:71-72` |

**Gaps, each with the line that makes it a gap:**

| Gap | Evidence |
|---|---|
| Hosted **instrument** latency is not in PDC (only Vox/Inst strips contribute an engine term) | `BaySickGraph.cpp:1681-1686`, `BaySickGraph.h:579,585` |
| Bridged latency read once at load; no update message | `PluginBridgeProtocol.h:69-95`; `HostedPlugin.cpp:404-408` |
| No sidechain into a hosted **instrument** (tab engines get no SC buffers unless they implement `ISidechainEngine`) | `EngineInsertTask.cpp:81-88`; `HostedPlugin.h:42-43` |
| No sidechain for any **bridged** plugin | `HostedPlugin.cpp:402-409`; `PluginHostMain.cpp:346` |
| Multi-output instruments collapse to bus 0; no per-output strips | `HostedPlugin.cpp:41-46, 615-621` |
| No MIDI out, no plugin→plugin MIDI, no MIDI effects | `HostedPlugin.h:127`; `PluginBridgeProtocol.h:69-95` |
| One live MIDI target at a time; no per-track MIDI input or channel filter | `PluginProcessor.cpp:3579-3595`; `StandaloneApp.cpp:1184-1199` |
| Instrument/effect classes are disjoint by `isInstrument`: a synth cannot go in an insert slot, an effect cannot be a tab | `PluginManager.cpp:157-177`; `SlotComponent.cpp:835`; `PluginsPage.cpp:629` |
| No `.vstpreset` and no program browsing/selection | repo-wide absence; `HostedPlugin.cpp:387-392` |
| Plugin params are outside APVTS → no MIDI Learn, no strip-style param table | `MidiLearnRegistry.cpp:183` |
| Bridge toggle unreachable from a Plugins tab | `EffectWindows.cpp:347-370`; `EngineRig.cpp:477-484` |
| Bridged param **reads** are fake (fallback) | `HostedPlugin.cpp:305-311` |
| Bridged MIDI capped at 4 KB/block, silently truncated | `SandboxedPluginClient.cpp:290-291` |
| Bridge is process separation only, not a security sandbox | `HostedPlugin.cpp:699-701` |
| Rack retry loads a user-bridged 64-bit plugin in-process first | `EffectsPage.cpp:970-980` |
| Caps: 20 plugin tabs, 6 rack slots per channel | `BaySickConstants.h:25`; `EffectRack.h` (`kNumSlots = 6`, `Effect Racks.md:15`) |
| Plugin-tab windows are non-user-resizable (surface owns its size) | `PluginsPage.cpp:270`; `EffectWindows.cpp:145` |

---

# READER 4: TABS, PAGES and their coupling to BUILT-IN ENGINES

# TABS, PAGES and their coupling to BUILT-IN ENGINES

Docs read first (authoritative): `Plans & Specs/System Reference/INDEX.md`, `Engine Tabs (Layers, Bass, Drums).md`, `Plugins Page.md`, `Clips Page.md` (§How it operates), `Inst Page.md` (§How it operates), `Vox Page.md` (§How it operates), `Builder Page.md` (§Track rows), `Workspace and Windows.md` (§How it operates). Every claim below re-confirmed in code with file:line.

---

## 1. The two enums, the page classes, and who owns what

### 1a. The two parallel enums (they are NOT the same list)

| Enum | Where | Values |
|---|---|---|
| `TabKind` (model) | `Source/EngineRig.h:49` | `Layers, Bass, Drums, Clips, Vox, Inst, Plugins, Rusty` |
| `RibbonTabBar::TabType` (view) | `Source/Standalone/RibbonTabBar.h:24` | `Mixer, Effects, Builder, Clip, Vox, Inst, Layers, Bass, Drums, PianoRoll, Plugins` |
| `EngineKind` (piano roll) | `Source/Standalone/PianoRollPage.h:38` | `DrumKit, Layer, Bass, Drum, Clip, Vox, Inst, BaySickGuitars, BaySickBasses, BaySickRustyDrums, Plugin` |
| `MixerPage::StripKind` | `Source/Standalone/MixerPage.h:76` | `Layer, Bass, Drum, Audio, Plugin, Vox, Inst, Direct` |
| `BaySickGraph::InsertKind` | `Source/BaySickGraph.h:631` | `Layer, Bass, Drum, Audio, Aux, Vox, Inst, Rusty, Plugin, Direct` |

Both `TabKind` and `TabType` are **persisted as raw ints** and are documented APPEND-ONLY (`EngineRig.h:43-48`, `RibbonTabBar.h:22-23`). Mapping between them is hand-written in at least four places: `StandaloneEditor.cpp:1896-1906` (`onIsTabFrozen`), `:7777-7788` (`visiblePageTabIdentity`), `:15228-15236` (serialize), `:18175-18182` (restore). `TabKind → InsertKind` is a fifth at `PluginProcessor.cpp:4405-4422`.

`RibbonTabBar::TabType` has **no `Rusty` value** — the Rusty singleton rides `TabType::Drums` (`RibbonTabBar.h:64-69`, `StandaloneEditor.cpp:10099 addBaySickRustyDrumsTab`).

### 1b. Page classes and the engines each hosts

| Page class | File | TabKind | Engines it can hold | Cap |
|---|---|---|---|---|
| `LayersPage` | `Source/Standalone/LayersPage.h/.cpp` | `Layers` | `BaySickSolstice`, `BaySickSynth`, `BaySickPlayer` (`LayersPage.cpp:391`) | `kMaxLayerPages`=20 |
| `BassPage` | `Source/Standalone/BassPage.h/.cpp` | `Bass` | `BaySickSolstice`, `BaySickPlayer`, `BaySickBass` (`BassPage.cpp:376`) | `kMaxBassPages`=10 |
| `DrumPage` | `Source/Standalone/DrumPage.h/.cpp` | `Drums` | `BaySickPlayer`, `BaySickSynth` (`EngineRig.h:279`) | `kMaxDrumPages`=32, split into two kits of 16 (`BaySickGraph.h:109 kDrumPagesPerBank`) |
| `ClipsPage` | `Source/Clips/ClipsPage.h/.cpp` | `Clips` | `enum class EngineType { None=0, BaySickPlayer=1 }` (`ClipsPage.h:39`) | `kMaxClipPages`=100 |
| `VoxPage` | `Source/Vox/VoxPage.h/.cpp` | `Vox` | `BaySickVocal` only — `createEngineFor` rejects anything else (`EngineRig.cpp:575`); enum at `VoxPage.h:44` | `kMaxVoxPages`=10 |
| `InstPage` | `Source/Inst/InstPage.h/.cpp` | `Inst` | engineType is the literal `"Chain"` (`EngineRig.cpp:634`) = `EngineChainProcessor` over rig-owned `BaySickPedalsProcessor` + `BaySickNAMIRProcessor` (`EngineRig.cpp:637-653`); front-end from `enum class Source { LiveInput, BaySickGuitars, BaySickBasses }` (`InstPage.h:44`), the two sfizz engines being **processor-owned, not rig-owned** | `kMaxInstPages`=30, shared across all three sources |
| `PluginsPage` | `Source/Standalone/PluginsPage.h/.cpp` | `Plugins` | one `Hosting::HostedPluginInstance`; `engineType` **is** `PluginDescription::createIdentifierString()` (`EngineRig.cpp:590-629`) | `kMaxPluginPages`=20 |
| `BaySickRustyDrumsPage` | `Source/Standalone/BaySickRustyDrumsPage.h/.cpp` | `Rusty` | **none** — `EngineTab` holds no engine; the kit engine is processor-owned. The tab exists purely as identity + freeze state (`EngineRig.h:46-48`, `StandaloneEditor.cpp:7809-7814`) | 1 (`EngineRig.cpp:41`) |

Non-instance pages (always present, no engine): `MixerPage`, `EffectsPage`, `BuilderPage`, `PianoRollPage`, created in `StandaloneEditor::buildDefaultTabs` with fixed ribbon ids 1/2/3/4 (`StandaloneEditor.cpp:2200-2204`).

### 1c. How `EngineRig` creates / owns / tears down

- **Ownership**: `std::vector<std::unique_ptr<EngineTab>> mTabs` (`EngineRig.h:389`); `EngineTab::engine` is a `std::unique_ptr<juce::AudioProcessor>` (`EngineRig.h:63`) plus `ownedStages` for Inst (`:69`). Identity is `(kind, pageIndex)` and **there is deliberately no name in `EngineTab`** — the display name belongs to the ribbon tab (`EngineRig.h:52-57`).
- **Create**: `EngineRig::setEngineType(kind, pageIndex, engineType)` (`EngineRig.cpp:~350-415`) → `createEngineFor` (`EngineRig.cpp:536-657`). That function is a `switch (tab.kind)` whose Layers/Bass/Drums/Clips arm is a **string chain**:
  - `EngineRig.cpp:562` `"BaySickSolstice"` → `BaySickSolsticeProcessor`
  - `:563` `"BaySickPlayer"` → `BaySickPlayerProcessor`
  - `:564` `"BaySickSynth"` → `BaySickSynthProcessor`
  - `:565` `"BaySickBass"` → `BaySickBassProcessor`
  - `:575-581` Vox: `"BaySickVocal"` → `BaySickVocalProcessor`
  - `:590-629` Plugins: `PluginManager::findAdded(identifier)` + single-use stashed-description fallback
  - `:634-653` Inst: `"Chain"` → `EngineChainProcessor` + Pedals + NAM/IR
- Every engine gets an APVTS `undoOwnerTag = "rig:<int(kind)>:<pageIndex>"` (`EngineRig.cpp:545-546`) and a track-id prefix from `trackIdFor` (`EngineRig.cpp:64-84`: `lay_`, `bas_`, `drm_`, `clip_<n>_`; empty for Vox/Inst/Plugins/Rusty).
- **Register**: `registerWithProcessor` (`EngineRig.cpp:673-758`) — a `switch(kind)` calling `mProc.registerLayerEngine/registerBassEngine/registerDrumEngine/registerPluginEngine/registerClipEngine/registerVoxEngine/registerInstEngine`. Plugins additionally get `setRateAndBufferSizeDetails` (`:719`). Vox gets `dynamic_cast<BaySickVocalProcessor*>` to wire `onPitchAlignEditsChanged` (`:747`).
- **Unregister**: `unregisterFromProcessor` (`EngineRig.cpp:760-772`) — 7-arm switch (no `Rusty` arm).
- **Teardown**: `teardownEngine` (`EngineRig.cpp:774+`) detaches the freeze watcher / proc listener, raises the project-load audio shield, settles a block, then unregisters and destroys. `removeTab` (`EngineRig.cpp:~100-140`) retracts frozen sources first when frozen, then `teardownEngine`, and erases per-page plugin restore descriptions.
- **Resolve APVTS generically**: `EngineRig::apvtsOf` (`EngineRig.cpp:47-61`) — **seven `dynamic_cast`s to built-in processor classes**, returning null for hosted plugins and the chain wrapper by design (`:58-60`).
- **Model events**: `onEngineCreated` / `onEngineDestroying` (`EngineRig.h:369-370`). `StandaloneEditor` subscribes `onEngineCreated` once at startup and uses it for automation registration (`StandaloneEditor.cpp:15528 registerModelEngineAutomation`). `onEngineDestroying` has no subscriber today (`EngineRig.h:366-368`).

### 1d. Pages are non-owning views

`LayersPage.h:146-150` — `juce::AudioProcessor* mEngineProcessor` (raw, non-owning) plus a view-owned `mEngineEditor`. Same shape documented for Vox (`Vox Page.md`), Clips (`Clips Page.md`), Plugins (`PluginsPage.h:16-18`). `StandaloneEditor.h:267-273`: **page destruction on window close is off** — closing a window frees only the window; the page component and rig engine survive. `StandaloneEditor.h:277-278` `rebuildPageForTab` / `canRebuildType` exists but returns false for the page types whose construction is entangled with mixer-strip spawning (Vox/Inst — see `MixerPage.h:90-100`, where the page is *downstream* of the strip).

### 1e. The ribbon "+" menu and the per-tab menus

**The "+" Add menu** — `RibbonTabBar::buildAddMenu()` (`RibbonTabBar.cpp:561-652`). Every row is an **engine name**, and the engine decides the tab (`:563-566`). Exact rows in order:

| Row | Effect | Line |
|---|---|---|
| `BaySickVocal` | `(Vox, "BaySickVocal")` | `:586` |
| `BaySickLiveInst` | `(Inst, "BaySickLiveInst")`, gated on Inst cap | `:587` |
| `BaySickGuitars` | fixed id **1** → `onAddBaySickGuitarsRequest` | `:591` |
| `BaySickBasses` | fixed id **2** → `onAddBaySickBassesRequest` | `:592` |
| `VSTPlugin >` | submenu of `PluginManager::getAddedInstruments()`; each → `(Plugins, identifierString)`; empty → disabled `"None added - see Options > Plugins"` | `:601-616` |
| `BaySickSolstice >` | `Layers` / `Bass` | `:618-623` |
| `BaySickSynth` | `(Layers, "BaySickSynth")` flat | `:627` |
| `BaySickPlayer >` | `Layers` / `Bass` / `Audio Clips` | `:629-635` |
| `BaySickBass` | `(Bass, "BaySickBass")` | `:637` |
| `BaySickDrums >` | `BaySickPlayer` / `BaySickSynth` → Drums | `:641-646` |
| `BaySickRustyDrums` | fixed id **3** → `onAddBaySickRustyDrumsRequest`, disabled when live | `:648-649` |

Dispatch: `handleAddMenuResult` (`RibbonTabBar.cpp:667-678`) — ids 1/2/3 are the three sfizz spawn routes; everything else is `kAddEngineBaseId + index` into `mAddMenuChoices`. `isAddMenuId` at `:660-665`. **`Edit > New Tab` embeds the identical menu**, not a copy: `StandaloneEditor.cpp:11793` `m.addSubMenu("New Tab", mRibbon->buildAddMenu())`, dispatched at `:11886-11888`.

Editor side: `mRibbon->onAddEngineRequest` (`StandaloneEditor.cpp:1765-1845`) calls `onAddTabRequest(type)` then `applyEngineToNewestTabOfType(type, engine)` (`:17480-17498`), which is a 4-way `dynamic_cast` to `LayersPage::selectEngine` / `BassPage::selectEngine` / `DrumPage::selectEngine` / `PluginsPage::selectPluginById`.

**The per-type ribbon dropdown** — `showInstanceDropdown` (`RibbonTabBar.cpp:766-1030`): instance list with tick + `[L]` prefix + `(missing)` suffix (`:782-793`); a `Pages:` header (`:802`) whose rows come from the editor's `onListPageWindowRows` (`:803-806`); `Rename...` (-1) and `Delete` (-2) (`:822-823`); then engine-named add rows scoped to the type (`:845-894`):

- Layers → `+ Add BaySickSolstice / BaySickPlayer / BaySickSynth`
- Bass → `+ Add BaySickSolstice / BaySickPlayer / BaySickBass`
- Drums → `+ Add BaySickPlayer / BaySickSynth`, plus `+ Add BaySickRustyDrums` (-4) when the singleton is free (`:922-928`)
- Vox → `+ Add BaySickVocal`, plus `+ Add New Vox From Export >` (`:900-918`)
- Inst → `+ Add BaySickLiveInst`, plus `+ Add BaySickGuitars` (-5) / `+ Add BaySickBasses` (-6) (`:934-939`)
- Clip → `+ Add BaySickPlayer...` (-3), which opens the file picker
- Plugins → `+ Add VSTPlugin >` from the added-instrument list (`:875-893`)

`Delete` refuses when the tab is locked with a `"Cannot Delete"` box (`RibbonTabBar.cpp:961-967`), otherwise routes to `onTabDeleteRequested` → `StandaloneEditor.cpp:1740-1758`, a 8-arm `dynamic_cast` chain to each page's `requestDelete()`.

`showSubPageDropdown` (`RibbonTabBar.cpp:718-763`) serves only Effects (`Rack / Pre EQ / Post EQ`) and Builder (`Patterns / Audio Clips / Automation`); there is also a dead Drums arm at `:736-741` that `showDropdown` never reaches (Drums goes to `showInstanceDropdown`, `:702-706`).

**The `Pages:` rows** are built by `StandaloneEditor::buildPageWindowRows` (`StandaloneEditor.cpp:6567-6679`) — an 8-arm `dynamic_cast` on the page component producing per-family row sets: Layers/Bass/Clips/Plugins `Player, Piano Roll`; Drums/Rusty `Drum Kit, Player, Piano Roll`; Vox `Player, Vocal Chain, BaySickPitch, BaySickAlign, NAM/IR`; Inst LiveInput `Pedals, NAM/IR`, sfizz Inst `Player, Pedals, NAM/IR, Piano Roll`; every one then appends `Pre EQ` / `Post EQ` keyed on the resolved `eqChannelId` (`:6673-6677`).

**The per-window Menu (hamburger)** is `PageMenuBar` (`SharedUI.h:310-523`). The shared tail is `appendStandardItems` (`SharedUI.h:436`, impl `SharedUI.cpp:1624` FX Rack, `:1660-1680` Freeze/Frozen). `setFreezeSlot` is wired at exactly one site, `StandaloneEditor::wireFreezeSlotForVisiblePage` (`:7797-7892`), keyed off `visiblePageTabIdentity` (`:7775-7790`). Per-page items:

| Page | Menu items | Line |
|---|---|---|
| Layers | Lock Layer, Polyphony, Rename…, **Replace Engine >** (kLayerEngines), Duplicate Layer (new tab), Choke Group, Save Current Patch As…, Load Preset >, Save/Load Page Preset, Delete Layer | `LayersPage.cpp:396-512` |
| Bass | same shape, `kBassEngines`, Delete Bass | `BassPage.cpp:381-492` |
| Drums | Lock Drum, Polyphony, Rename…, **Replace Sound…**, Duplicate Drum, Choke Group, MIDI Note >, MIDI Learn/Forget, Save Current Patch As…, Save/Load Page Preset, Delete Drum | `DrumPage.cpp:1160-1291`; the empty-slot **sound menu** (Sample > / Synth Patch > / None (clear)) at `:428-500` |
| Clips | Lock, Rename…, Duplicate Clip, Choke Group, Save/Load Page Preset, Delete Clip | `ClipsPage.cpp:190-223` |
| Vox | Lock, Rename…, Duplicate Vox, Save/Load Page Preset, Delete Vox | `VoxPage.cpp:286-308` |
| Inst | Lock, Rename…, Duplicate Inst, Save/Load Page Preset, Delete Inst (+ pedalboard preset menu at `:1318-1330`) | `InstPage.cpp:600-642` |
| Plugins | Rename…, **Replace Plugin >**, Duplicate Plugin, Save/Load Page Preset, **Automate >** (chunked 30/submenu), Retry Loading Plugin, Delete Plugin — **no Lock** | `PluginsPage.cpp:618-730` |
| Rusty | Save Player Preset As…, Load Player Preset | `BaySickRustyDrumsPage.cpp:351-374` |

---

## 2. Every place the shell reaches into a built-in engine by name string / `dynamic_cast`

### 2a. Processor class references (`dynamic_cast`, `#include`, `make_unique`) — count per file

Files inside the engine's own folder are marked *(own folder — deleted with the engine)*.

| Symbol | Per-file counts |
|---|---|
| **`BaySickSolsticeProcessor`** | `BaySickSolstice/BaySickSolsticeProcessor.cpp` 11*, **`Standalone/StandaloneEditor.cpp` 10**, **`Standalone/LayersPage.cpp` 6**, **`Standalone/BassPage.cpp` 6**, `BaySickSolstice/…Processor.h` 4*, **`EngineRig.cpp` 3**, `BaySickSolstice/…Editor.h` 3*, **`Standalone/BuilderPage.cpp` 2**, `BaySickSolstice/…Synth.h` 2*, `…ModRegistry.h` 2*, then 1 each in `BaySickSynth/…Processor.h/.cpp`, `BaySickSolstice/…Synth.cpp`, `…Editor.cpp`, `BaySickPlayer/…Processor.h/.cpp`, `BaySickBass/…Processor.h/.cpp` |
| **`BaySickSynthProcessor`** | **`StandaloneEditor.cpp` 12**, `BaySickSynth/…Processor.cpp` 10*, **`LayersPage.cpp` 9**, **`DrumPage.cpp` 7**, `…Processor.h` 4*, `…Editor.h` 4*, **`EngineRig.cpp` 3**, `BaySickBass/…Processor.h` 2, `BaySickBass/…Processor.cpp` 2, **`PluginProcessor.cpp` 1**, `…Editor.cpp` 1*, `…DSP.h` 1* |
| **`BaySickBassProcessor`** | `BaySickBass/…Processor.cpp` 10*, **`BassPage.cpp` 9**, **`StandaloneEditor.cpp` 5**, `…Processor.h` 4*, `…Editor.h` 4*, **`EngineRig.cpp` 3**, `…Editor.cpp` 1* |
| **`BaySickPlayerProcessor`** | **`StandaloneEditor.cpp` 19**, `BaySickPlayer/…Processor.cpp` 14*, **`DrumPage.cpp` 11**, **`LayersPage.cpp` 10**, **`BassPage.cpp` 10**, **`Clips/ClipsPage.cpp` 10**, `…Processor.h` 5*, **`PluginProcessor.cpp` 3**, **`EngineRig.cpp` 3**, `…Editor.h` 3*, `…Editor.cpp` 3*, **`Engine/Tasks/CompositeAudioInsertTask.h` 2**, **`.cpp` 2**, **`Clips/ClipsPage.h` 2**, `…DSP.h` 2*, **`Vox/VoxPage.h` 1**, **`PluginProcessor.h` 1**, `…DSP.cpp` 1* |
| **`BaySickVocalProcessor`** | `BaySickVocal/…Processor.cpp` 41*, **`StandaloneEditor.cpp` 13**, **`PluginProcessor.cpp` 11**, **`Vox/VoxPage.cpp` 10**, `…Editor.cpp` 9*, `…Processor.h` 6*, **`EngineRig.cpp` 4**, `…Editor.h` 4*, `BaySickAlignEditor.h` 4*, **`Standalone/ShotHarness.cpp` 3**, **`DSP/BaySickAlignDSP.h` 3**, `BaySickPitchEditor.h` 3*, `BaySickAlignEditor.cpp` 3*, **`BuilderPage.cpp` 2**, **`Engine/Tasks/VoxStripTask.h` 2**, **`.cpp` 2**, `BaySickPitchEditor.cpp` 2*, **`PluginProcessor.h` 1**, **`Inst/InstPage.h` 1**, **`DSP/PitchShifters.h` 1**, **`DSP/BaySickPitchDSP.h` 1**, **`.cpp` 1**, `BaySickPitchSubEditor.cpp` 1* |
| **`BaySickNAMIRProcessor`** | `BaySickNAMIR/…Processor.cpp` 34*, `BaySickVocal/…Processor.h` 8, **`Inst/InstPage.cpp` 4**, `BaySickVocal/…Processor.cpp` 4, **`EngineRig.cpp` 3**, `BaySickVocal/…Editor.cpp` 3, `…NAMIREditor.h` 3*, **`StandaloneEditor.cpp` 2**, **`EngineRig.h` 2**, **`EffectRack.h` 2**, **`ProjectBundler.cpp` 1**, **`Inst/InstPage.h` 1**, **`DSP/PolyphaseOversampler.h` 1**, **`DSP/MicSimDSP.h` 1**, **`.cpp` 1**, **`DSP/MicPlacementDSP.h` 1**, `BaySickVocal/…Editor.h` 1, `MicPlacementView.h/.cpp` 1 each*, `…NAMIREditor.cpp` 1* |
| **`BaySickPedalsProcessor`** | `BaySickPedals/…Editor.cpp` 35*, `…Processor.cpp` 24*, `…Editor.h` 6*, `…Processor.h` 5*, **`StandaloneEditor.cpp` 4**, **`ShotHarness.cpp` 4**, **`Inst/InstPage.cpp` 4**, **`EngineRig.cpp` 3**, **`EngineChainProcessor.cpp` 2**, **`BuilderPage.cpp` 2**, **`EngineRig.h` 2**, **`StandaloneEditor.h` 1**, **`EffectEditorPanels.h` 1**, **`Inst/InstPage.h` 1** |
| **`BaySickGuitarsProcessor`** | `BaySickGuitars/…Processor.cpp` 19*, `…Processor.h` 5*, **`PluginProcessor.cpp` 3**, **`StandaloneEditor.cpp` 2**, **`PluginProcessor.h` 2**, **`Inst/InstPage.cpp` 1**, **`EngineRig.cpp` 1**, **`Engine/Tasks/InstStripTask.cpp` 1**, `BaySickRustyDrums/…Processor.h` 1, `BaySickPedals/…Processor.h` 1, `BaySickBasses/…Processor.h` 1, `.cpp` 1 |
| **`BaySickBassesProcessor`** | `BaySickBasses/…Processor.cpp` 19*, `…Processor.h` 5*, **`PluginProcessor.cpp` 3**, **`StandaloneEditor.cpp` 2**, **`PluginProcessor.h` 2**, **`Inst/InstPage.cpp` 1**, **`EngineRig.cpp` 1**, **`Engine/Tasks/InstStripTask.cpp` 1** |
| **`BaySickRustyDrumsProcessor`** | `BaySickRustyDrums/…Processor.cpp` 24*, `…Processor.h` 5*, **`StandaloneEditor.cpp` 4**, **`RustyDrumsMapWindow.h` 4**, **`.cpp` 4**, `…KitGraphic.h` 4*, **`PluginProcessor.cpp` 3**, **`BaySickRustyDrumsPage.cpp` 2**, **`PluginProcessor.h` 2**, `…KitGraphic.cpp` 2*, **`BuilderPage.cpp` 1**, **`SafeSfzKit.h` 1**, **`EngineRig.cpp` 1**, **`Engine/Tasks/RustyInsertTask.h` 1**, **`.cpp` 1**, **`RustyDrumsProducerTask.h` 1**, **`.cpp` 1** |

Notable clusters inside the shell:
- `StandaloneEditor.cpp:8289-8301, 8333-8353, 8379-8393` — the piano-roll **audition** closures, three near-identical `dynamic_cast` cascades (Layer / Bass / Drum).
- `StandaloneEditor.cpp:11060-11069` — `wireEngineDirtyHook`, a **nine-way** `dynamic_cast` to call `setOnAnyStateChange` on every built-in processor class.
- `LayersPage.cpp:125-127, 158-176` — engine **editor** casts for `onPatchLoaded`, `getEngineTitle`, `getEngineAccent`, `getTitleStripPresetButton`.
- `LayersPage.cpp:245-256, 994-1002` — `getParamPrefix` / `replaceStateKeepingUndoHistory` / APVTS listener attach, three-way casts.
- `EngineRig.cpp:47-61` — `apvtsOf`, seven casts.

### 2b. Engine-type **strings** — count per file

| String | Per-file counts |
|---|---|
| `"BaySickSolstice"` | **`RibbonTabBar.cpp` 5**, **`ShotHarness.cpp` 3**, **`StandaloneEditor.cpp` 2**, **`LayersPage.cpp` 2**, **`BassPage.cpp` 2**, **`PluginProcessor.h` 1**, **`EngineRig.cpp` 1**, `BaySickSolstice/…Processor.h` 1*, `…Editor.h` 1* |
| `"BaySickPlayer"` | **`RibbonTabBar.cpp` 8**, **`DrumPage.cpp` 8**, **`ShotHarness.cpp` 4**, **`StandaloneEditor.cpp` 3**, **`LayersPage.cpp` 3**, **`BassPage.cpp` 3**, **`Clips/ClipsPage.cpp` 2**, **`PluginProcessor.h` 1**, **`EngineRig.cpp` 1**, `BaySickPlayer/…Processor.h` 1*, `…Editor.h` 1* |
| `"BaySickSynth"` | **`DrumPage.cpp` 6**, **`StandaloneEditor.cpp` 4**, **`RibbonTabBar.cpp` 4**, **`ShotHarness.cpp` 2**, **`LayersPage.cpp` 2**, **`PluginProcessor.h` 1**, **`EngineRig.cpp` 1**, `BaySickSynth/…Processor.h` 1*, `…Editor.h` 1* |
| `"BaySickBass"` | **`ShotHarness.cpp` 5**, **`StandaloneEditor.cpp` 2**, **`RibbonTabBar.cpp` 2**, **`BassPage.cpp` 2**, **`PluginProcessor.h` 1**, **`EngineRig.cpp` 1**, `BaySickBass/…Processor.h` 1*, `…Editor.h` 1* |
| `"BaySickVocal"` | **`Vox/VoxPage.cpp` 2**, **`RibbonTabBar.cpp` 2**, **`ShotHarness.cpp` 1**, **`EngineRig.cpp` 1**, `BaySickVocal/…Processor.h` 1* |
| `"BaySickGuitars"` | **`StandaloneEditor.cpp` 11**, **`PagePresetIO.cpp` 3**, **`Inst/InstPage.cpp` 2**, **`ShotHarness.cpp` 1**, **`RibbonTabBar.cpp` 1**, **`PagePresetIO.h` 1**, `BaySickGuitars/…Processor.h` 1* |
| `"BaySickBasses"` | **`StandaloneEditor.cpp` 9**, **`PagePresetIO.cpp` 3**, **`Inst/InstPage.cpp` 2**, **`ShotHarness.cpp` 1**, **`RibbonTabBar.cpp` 1**, **`PagePresetIO.h` 1**, `BaySickBasses/…Processor.h` 1* |
| `"BaySickRustyDrums"` | **`StandaloneEditor.cpp` 16**, **`PagePresetIO.cpp` 2**, **`BaySickRustyDrumsPage.cpp` 2**, **`ShotHarness.cpp` 1**, **`RibbonTabBar.cpp` 1**, **`BaySickRustyDrumsPage.h` 1**, `BaySickRustyDrums/…Processor.h` 1* |
| `"Chain"` (Inst) | **`Inst/InstPage.cpp` 2**, **`EngineRig.cpp` 1** |
| `"BaySickLiveInst"` | `RibbonTabBar.cpp:587, :869` (2) |

### 2c. `EffectType::` enumerators of built-in effects (excluding `None` and `VST3Plugin`)

Enum declared at `Source/EffectRack.h:19-80`: 13 rack effects (`Compressor=1` … `DeEsser=12`), 18 pedal-native (`BluesDriveStyle=100` … `NAMPedalStyle=118`), `Gate=119`, `DeReverb=120`, `VST3Plugin=121`.

| File | Built-in `EffectType::` sites |
|---|---|
| `Standalone/EffectPresetIO.cpp` | 90 |
| `Standalone/SlotComponent.cpp` | 89 |
| `DSP/EffectParamMap.cpp` | 51 |
| `Standalone/EffectEditorPanels.cpp` | 43 |
| `BaySickPedals/BaySickPedalsEditor.cpp` | 37 |
| `Standalone/ShotHarness.cpp` | 33 |
| `EffectRack.cpp` | 33 |
| `EffectRack.h` | 18 |
| `BaySickPedals/BaySickPedalsProcessor.cpp` | 16 |
| `BaySickVocal/BaySickVocalProcessor.cpp` | 12 |
| `Standalone/EffectWindows.cpp` | 10 |
| `DSP/EffectParamMap.h`, `BaySickPedals/BaySickPedalsProcessor.h` | 1 each |

By contrast `EffectType::VST3Plugin` has only **11 sites total**: `EffectsPage.cpp` 4, `SlotComponent.cpp` 2, `EffectRack.cpp` 2, `EffectWindows.cpp` 1, `EffectPresetIO.cpp` 1, `EffectEditorPanels.cpp` 1, `EffectRack.h` 1. `EffectType::None` has 50.

### 2d. Ten files with the most built-in-coupling sites (processor classes + engine strings + built-in `EffectType::`)

| # | File | Sites |
|---|---|---|
| 1 | `Source/Standalone/StandaloneEditor.cpp` | **120** |
| 2 | `Source/Standalone/EffectPresetIO.cpp` | 90 |
| 3 | `Source/Standalone/SlotComponent.cpp` | 89 |
| 4 | `Source/BaySickPedals/BaySickPedalsEditor.cpp` | 72 |
| 5 | `Source/BaySickVocal/BaySickVocalProcessor.cpp` | 59 |
| 6 | `Source/Standalone/ShotHarness.cpp` | 58 |
| 7 | `Source/DSP/EffectParamMap.cpp` | 51 |
| 8 | `Source/Standalone/EffectEditorPanels.cpp` | 43 |
| 9 | `Source/BaySickPedals/BaySickPedalsProcessor.cpp` | 42 |
| 10 | `Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp` | 36 |

Restricting to files that are **shell, not engine folders** (i.e. survive the fork): `StandaloneEditor.cpp` 120, `EffectPresetIO.cpp` 90, `SlotComponent.cpp` 89, `ShotHarness.cpp` 58, `DSP/EffectParamMap.cpp` 51, `EffectEditorPanels.cpp` 43, `EffectRack.cpp` 33, `LayersPage.cpp` 32, `DrumPage.cpp` 32, `BassPage.cpp` 32, then `EngineRig.cpp` 30, `RibbonTabBar.cpp` 29, `PluginProcessor.cpp` 24, `EffectRack.h` 20, `InstPage.cpp` 14, `VoxPage.cpp` 12, `PluginProcessor.h` 12, `ClipsPage.cpp` 12.

### 2e. Per-page-class coupling inside `StandaloneEditor.cpp` (how much editor code is per-page-type)

`dynamic_cast<X*>` sites / total mentions:

`DrumPage` 38/106 · `InstPage` 37/137 · `ClipsPage` 35/56 · `LayersPage` 28/56 · `BassPage` 28/55 · `VoxPage` 28/59 · `BaySickRustyDrumsPage` 23/34 · `PluginsPage` 19/34 — versus the four permanent pages, which are held as cached raw pointers instead: `MixerPage` 1/244, `EffectsPage` 1/65, `BuilderPage` 1/142, `PianoRollPage` 2/196.

---

## 3. What a tab IS as data, and what keys off it

A "tab" is **three records with no single owner**:

**(a) The ribbon record** — `RibbonTabBar::Tab` (`RibbonTabBar.h:26-38`):
```
int id;  TabType type;  juce::String name;  bool locked;  bool kitMissing;
```
That is the whole struct. **No engine pointer, no page index, no colour, no strip binding.** `locked` drives the `[L] ` prefix and the Delete gate; `kitMissing` is display-only and explicitly never serialized (`:34-37`). Colour is not stored — it is a pure function of `TabType` (`RibbonTabBar.cpp:7-35`). Per-type "last visited" is `std::map<TabType,int> mLastUsedByType` (`RibbonTabBar.h:284`).

**(b) The editor record** — `StandaloneEditor::PageEntry` (`StandaloneEditor.h:212-226`):
```
int ribbonTabId;  RibbonTabBar::TabType type;
std::unique_ptr<juce::Component> component;   // the page (owns it)
std::unique_ptr<WorkspaceWindow> window;      // the frame (hosts page non-owningly)
int pageIndexHint;                            // -1 for system pages
```
`mPages` is an append-ordered `juce::OwnedArray<PageEntry>` (`StandaloneEditor.h:963`). "Newest of type" is literally the last matching entry (`StandaloneEditor.cpp:17486-17488`).

**(c) The model record** — `EngineTab` (`EngineRig.h:51-213`): `kind`, `pageIndex`, `engineType`, `engine`, `ownedStages`, typed `pedals`/`namIr` views, and the whole freeze block (`frozen`, `frozenByUser`, `userUnfroze`, `freezeSpan` incl. `contentStamp`, `freezeStale`, `freezeStreams`, `freezePatternStreams`, `stalePatterns`, `patternStamps`, watchers).

The **page component** holds a fourth copy of the name and lock (`LayersPage.h:153-155`: `mLocked`, `mEngineType`, `mTabName`), kept in sync by `setTabName` / `setTabLocked` callbacks (`StandaloneEditor.cpp:5652, :5670-5675`).

**Persisted form** — `StandaloneEditor::serializeTabsInto` (`:15050-15278`): one `<Tab>` per page, dispatched by `dynamic_cast` on the page component, with `type` (string: `"Plugins" | "Layers" | "Bass" | "Drum" | "Clips" | "Vox" | "Inst" | "BaySickRustyDrums"`), `pageIndex`, `name`, `engine`, `engineData` (base64), `locked` (`:15214`, from `pageIsLocked` at `:15038-15047` — note **Plugins is absent from that list, so a Plugins tab always writes `locked=0`**), plus per-kind extras (`clipPath`, `instChainState`, `source`, `kitPath`, `sfizzEngineData`, `<ProgramStateCache>`), then freeze state (`frozen`, `frozenBy`, `freezeStale`, `freezeScope/Beats/Bpm/Stamp`, `<FreezePattern>` children) at `:15209-15275`.

### How the three downstream surfaces key off a tab

**Piano-roll context label.** `PianoRollConnection` carries `displayName` and `engineType`, and the label is `"{tabName} - {engineType}"` with `"(no engine)"` substituted when empty (`PianoRollPage.h:61-68`). The key is `EngineId{ EngineKind, index }` (`PianoRollPage.h:40-47`) — a **different enum** from both `TabKind` and `TabType`. Both halves are pushed by the editor from the tab: `setEngineType({EngineKind::Layer, pageIdx}, p->getEngineType())` on engine pick (`StandaloneEditor.cpp:5661`), `setEngineDisplayName(..., nm)` on `onSoundNameChanged` (`:5684`). Plugins push `getPluginName()` for the engineType half and `getDisplayName()` for the tab half (`:5710-5730`). The same `EngineKind` ordinal is also the **live-MIDI target encoding** (`PluginProcessor.h:634-642`, dispatch at `PluginProcessor.cpp:3581-3595`: 1 Layer / 2 Bass / 3 Drum / 4 Clip / 7 Guitars / 8 Basses / 9 Rusty / 10 hosted plugin; 0/5/6 drop).

**Mixer strip.** The strip is created **lazily on first engine pick**, not at tab open: `onEngineSelected` → `mMixerPage->addLayerChannel(pageIdx, tab->name)` (`StandaloneEditor.cpp:5654-5657`), `addBassChannel` (`:5763`), `addDrumChannel` (`:5801`), `addPluginChannel` (`:5713`). Binding is by `(StripKind, pageIndex)` (`MixerPage.h:76, :163-172, :273`) → channel id from `MixerChannelIds` (`BaySickGraph.h:71-82`: layer 200+i, bass 300+i, audio 400+i, drum 500+i, vox 600+i, inst 700+i, rusty 800+i, plugin 900+i, direct 950+i) → APVTS prefix `mixer_<kind>_<i>`. Rename flows both ways: tab → strip via `renameChannel(kind, pageIdx, nm)` (`:5683`), strip → tab via `MixerPage::onChannelRenamed` (`MixerPage.h:85`). Vox/Inst are **inverted**: the strip is created first and *fires* the page spawn (`MixerPage.h:90-100 onVoxStripAdded/onInstStripAdded` → `spawnVoxTabIfMissing` / `spawnInstTabIfMissing`, `StandaloneEditor.cpp:11216, :11345`), which is why `canRebuildType` excludes them.

**Builder rows.** Builder track rows are **not** tabs. `Builder Page.md:56-63`: 500 numbered rows, "Rows have no fixed type", named `Track 1…`; per-row state is mute/solo/name/group in `<RowState>` (`Builder Page.md:296`). The coupling is one-directional and only for audio: dropping a file on a row runs `createClipStripAndPage(row, path)` where **the page index IS the audio row** (`Clips Page.md`, `StandaloneEditor.cpp:10538`), and the audio-library entry carries `pageOwnerChannelId = audioInsert(row)` so the browser groups files under the owning Clips page. The Builder's third sub-page (Automation) lists lanes whose display names are resolved from parameter ids back through the tab (`StandaloneEditor.cpp:4158-4230 resolveAutomationDisplayName` — `plugtab<N>_vst_<pid>` resolves via `mPages` + `mRibbon->getTabById` to `"{tab name} - {plugin param name}"`; `inst<N>_` / `vox<N>_` prefixes resolve to `"Inst 4 - …"`).

**Ribbon frozen dot** keys off the rig, not the tab record: `onIsTabFrozen` folds `isFrozen`/`isFreezeStale` over every page index of the mapped `TabKind` and returns the strongest state, because the ribbon has one slot per type (`RibbonTabBar.h:82-85`, `StandaloneEditor.cpp:1889-1920`).

---

## 4. What survives if every engine kind except hosted plugins is deleted

Facts, then clearly-labelled inference.

### Structurally unchanged (no built-in coupling found)

- `WorkspaceWindow.cpp/.h` — 3 and 2 `BaySick` mentions respectively, all in comments/`#include`-free prose; the window/frame layer is engine-agnostic (`Workspace and Windows.md`, `WorkspaceWindow.h`).
- `RibbonTabBar`'s **mechanics**: `Tab` struct, `visibleSlotTypes`, `isRequiredTab`, slot geometry, badges, rename, lock, `onListPageWindowRows`/`onPageWindowRowPicked`, `showSubPageDropdown`. Only the **content** of `buildAddMenu` (`:561-652`) and the add-row block of `showInstanceDropdown` (`:845-939`) is engine-named — 29 engine-string sites in the file.
- `PageMenuBar` in `SharedUI.h/.cpp`: `setMenuBuilder`, `setTabSlots`, `setViewMenu`, `setFxRackSlot`, `setFreezeSlot`, `appendStandardItems`, swing knob. Its `BaySick` mentions are effect/pedal LookAndFeel and image assets, not engine dispatch.

### The tab types that survive

Required, always drawn regardless of instance count (`RibbonTabBar.cpp:38-44`): **Builder, Mixer, Effects, PianoRoll**. The only instance type with a hosted-plugin engine is **`TabType::Plugins` / `TabKind::Plugins`**. Every other instance type (`Clip, Vox, Inst, Layers, Bass, Drums`) has a zero-instance state that already removes its slot from the ribbon (`visibleSlotTypes`, `:46-63`) — so the ribbon degrades cleanly to five slots + "+" with no code change.

`TabKind` would keep only `Plugins`; `capacityOf` (`EngineRig.cpp:28-45`), `trackIdFor` (`:64-84`), `apvtsOf` (`:47-61`), `createEngineFor` (`:536-657`), `registerWithProcessor` (`:673-758`), `unregisterFromProcessor` (`:760-772`) each collapse to their one Plugins arm. `apvtsOf` becomes `return nullptr` outright — hosted plugins deliberately have no APVTS (`EngineRig.cpp:58-60`) and their lanes key on the plugin's own parameter ids (`Plugins Page.md:135`).

### Pages that survive

Only **`PluginsPage`** among instance pages, plus the four permanent pages. `PluginsPage` is already described in-source as "deliberately the thinnest page in the app" and "a VIEW, per TS1: it never constructs an engine" (`PluginsPage.h:9-19`). It carries **19 `dynamic_cast` sites and 34 total mentions** in `StandaloneEditor.cpp` — the lightest of the eight page types. Deleted: `LayersPage`, `BassPage`, `DrumPage` (+`DrumKitGrid`), `ClipsPage`, `VoxPage`, `InstPage`, `BaySickRustyDrumsPage` (+`RustyDrumsMapWindow`, `AriaControlPanel`).

### Menus that survive

- **"+" menu**: of the 11 top-level rows in `buildAddMenu`, only the `VSTPlugin >` submenu (`RibbonTabBar.cpp:601-616`) survives. Ids 1/2/3 and their three callbacks (`onAddBaySickGuitarsRequest`, `onAddBaySickBassesRequest`, `onAddBaySickRustyDrumsRequest`) and `onIsBaySickRustyDrumsActive` / `onIsInstCapReached` (`RibbonTabBar.h:64-81`) become dead. `Edit > New Tab` follows automatically because it embeds the same builder (`StandaloneEditor.cpp:11793`).
- **Per-tab dropdown**: instance list, `Pages:` (`Player`, `Piano Roll`, `Pre EQ`, `Post EQ` — the Plugins arm of `buildPageWindowRows`, `StandaloneEditor.cpp:6633-6639, :6673-6677`), `Rename...`, `Delete`, `+ Add VSTPlugin >`. The Vox-export submenu, Rusty/Guitars/Basses add rows and the Clip file-picker route (-3) all go.
- **Page Menu**: the Plugins menu already has **no Lock, no Choke Group, no Polyphony, no Load Preset, no engine picker** (`PluginsPage.cpp:618-730`, confirmed by `Plugins Page.md:110`). It keeps Rename…, Replace Plugin >, Duplicate Plugin, Save/Load Page Preset, Automate >, Retry Loading Plugin, FX Rack, Freeze/Frozen, Delete Plugin. `FX Rack` + `Freeze` come from the shared `appendStandardItems` (`SharedUI.h:432-436`), so they survive unchanged.

### `StandaloneEditor` paths that survive vs. collapse

Survive, in Plugins-only form:
- `onAddTabRequest` — only the `TabType::Plugins` arm (`:5636-5640, :5692-5754`); the Clip file-picker arm (`:5520-5544`), the Vox/Inst strip-cascade arm (`:5549-5603`) and the Layers/Bass/Drums arms all go.
- `applyEngineToNewestTabOfType` → one line, `pp->selectPluginById(engine)` (`:17497`).
- `onTabClosed` → `pluginStripIdx` only (`:6086-6095`, `rig.removeTab(TabKind::Plugins, …)` at `:6447`, `mMixerPage->removePluginChannel` nearby).
- `visiblePageTabIdentity` → one line (`:7783`); `wireFreezeSlotForVisiblePage` (`:7797-7892`) survives whole, minus the `TabKind::Rusty` special-cases at `:7813-7814, :7859-7861, :7882-7883` and the `kind == TabKind::Vox` vocal-warning argument at `:7891`.
- `serializeTabsInto` / restore → the `"Plugins"` branch only (`:15069-15077` / `:18202-18242`).
- `registerPluginTabAutomation` (`:15857`), `registerPluginPianoRoll` (`:10967`), `spawnDuplicatePluginsTab` (`:11603`), `resolveAutomationDisplayName`'s `plugtab` branch (`:4165-4191`).
- `isPlayerTabType` (`:5889-5909`) → one true case; `getMostRecentPlayerTabId`, F7 "Show Player (Most Recent)" keep working.
- `buildDefaultTabs` (`:2180-2204`) unchanged — it only builds the four permanent pages.

Collapse to nothing:
- `wireEngineDirtyHook` (`:11053-11080`) — all nine casts are built-in classes; a hosted plugin has no `setOnAnyStateChange`.
- All three audition cascades (`:8289-8301, :8333-8353, :8379-8393`) — a hosted plugin has no `auditionNote`; the roll drives it through the live-MIDI route instead (`Plugins Page.md:28`, `PluginProcessor.cpp:3594`).
- `registerModelEngineAutomation` (`:15528`), `registerSfizzEngineAutomation` (`:15642`), `registerBaySickSolsticeModAutomation` (`:15704`), `registerPedalAutomation` (`:15793`).
- `addBaySickRustyDrumsTab` (`:10099`), `addBaySickGuitarsTab` (`:10331`), `addBaySickBassesTab` (`:10444`), `createClipStripAndPage` (`:10538`), `addClipPageFromFile` (`:10575`), `spawnClipsTabIfMissing` (`:10702`), `spawnVoxTabIfMissing` (`:11216`), `spawnInstTabIfMissing` (`:11345`), `spawnDuplicate{Layer,Bass,Drum,Clips,Vox,Inst}Tab` (`:2553, :2610, :2667, :10860, :11481, :11536`).
- The whole drum-kit subsystem: `refreshAllKitViews` (`:8010`), `setActiveDrumBank` (`:8034`), `firstFreeDrumIndexInBank` (`:8042`), `showDrumBankFullMessage` (`:8051`), `moveDrumTab` (`:8063`), `wireDrumPagePlayNote` (`:8087`), `wirePianoRollPageKitView` (`:8106`), `showGlobalLockPrompt` (`:8481`), `applyDrumLockStates` (`:8514`), `applyGlobalLockToggle` (`:8530`), `showKitMenu` (`:9002`), `saveKitAs` (`:9066`), `loadKit`/`loadKitWithUndo`/`loadKitImpl` (`:9137, :9205, :9314`), `drumBankLabel` (`:8029`), plus `mUsedDrumIndices` (`StandaloneEditor.h:990`) and `mActiveDrumBank`.
- `openVoxSatelliteWindow` (`:16572`), `openInstPedalsWindow` (`:16648`), `openInstNamIrWindow` (`:16787`), `installInstNavMenu` (`:16850`), `closeVoxSatellites` (`:16898`), `closeInstSatellites` (`:16907`), `showRustyDrumsMapWindow` (`:10035`).
- `RenameFamily` (`StandaloneEditor.h:583`) → `{ Plugins }`.
- `pageIsLocked` (`:15038-15047`) → always false, since no Plugins arm exists today.

### Inferred (flagged as such)

- **Inferred**: with only `TabKind::Plugins` left, the four hand-written `TabKind ↔ TabType ↔ InsertKind ↔ EngineKind` mapping tables (`StandaloneEditor.cpp:1896-1906, :7777-7788, :15228-15236, :18175-18182`; `PluginProcessor.cpp:4405-4422`) each become single-arm and are candidates to disappear entirely — but the **persisted ordinals** would still need pinning, because `TabKind`/`TabType`/`EffectType` are all documented as raw-int-persisted append-only (`EngineRig.h:43-48`, `RibbonTabBar.h:22-23`, `EffectRack.h:21-27`).
- **Inferred**: the FL-style "tab per bus" model would replace `visibleSlotTypes()`'s hard-coded `order[]` array (`RibbonTabBar.cpp:51-56`) and `tabColour`'s `switch(TabType)` (`:7-35`); neither reads any engine, so the change is contained to those two functions plus `getBadgeCount`/`countTabsOfType` (`RibbonTabBar.h:265-266`).
- **Inferred**: `EffectType` would need `None` + `VST3Plugin` only (2 of 34 enumerators), which removes ~430 built-in `EffectType::` sites concentrated in `EffectPresetIO.cpp` (90), `SlotComponent.cpp` (89), `EffectParamMap.cpp` (51), `EffectEditorPanels.cpp` (43), `EffectRack.cpp` (33) and `EffectRack.h` (18) — against only 11 `VST3Plugin` sites to keep.
- **Inferred**: `ShotHarness.cpp` (58 built-in coupling sites) is the `--shot`/`--docs` manual-figure harness (`Manual Pipeline.md`), not shipping UI; its coupling is to figure recipes, not to the tab system.

### Not determined

- Whether `EngineRig::recreateEngine` / `retryDeadPluginTab` (`EngineRig.cpp:417-534`) have any dependency on non-Plugins arms — read as self-contained, but the project-load-shield nesting interacts with `teardownEngine`'s own shield (`:803-810`), which I did not read past line 810.
- Whether `MixerPage`'s bus/routing code contains built-in-engine coupling — `MixerPage.cpp` appears in none of the built-in-symbol grep results, but I did not read it; `Mixer.md` was not read in this pass.

---

# READER 5: BUILD, PACKAGING & EDITION-SENSITIVE SURFACES — facts with evidence

## BUILD, PACKAGING & EDITION-SENSITIVE SURFACES — facts with evidence

Docs read first (`Plans & Specs/System Reference/INDEX.md`, `Manual Pipeline.md`, `Callout Registry.md`), then confirmed in code. Everything below is code-verified unless marked **inferred**.

---

## 1. Target structure, gating, and what the SHELL alone needs

### Targets defined in `CMakeLists.txt` (923 lines, one file, no subdirectory CMake except vendored libs)

| Target | Kind | Where | Notes |
|---|---|---|---|
| `BaySickDAWStandalone` | `juce_add_gui_app`, `PRODUCT_NAME "BaySickDAW"` | `CMakeLists.txt:504-508` | The shipping exe. `BUNDLE_ID "com.knowledgebasestudios.baysickdaw"`, `ICON_BIG Assets/BaySickDAWLogo.png` |
| `BaySickDAW` | `juce_add_plugin` VST3 | `CMakeLists.txt:463-475` | Explicitly labelled *"Legacy plugin target (NOT shipped; BaySickDAW is standalone-only) … Kept as build scaffolding only. Do not distribute."* `CMakeLists.txt:460-462`. `do_build.bat` never builds it (only `BaySickDAWStandalone` + `BaySickPluginHost`, lines 217/223/237/265). It compiles `${VIBESYNTH_DSP_SOURCES}` + `Source/PluginEditor.cpp` only. |
| `BaySickPluginHost` (x64) | `juce_add_gui_app`, `PRODUCT_NAME "BaySickPluginHost64"` | `CMakeLists.txt:862-891` | One source file: `Source/Hosting/Helper/PluginHostMain.cpp`. Links **only** `juce_audio_processors`, `juce_audio_utils`, `juce_gui_basics` and **none** of the vendored libs — stated as load-bearing at `CMakeLists.txt:853-857` |
| `BaySickPluginHost` (x86) | separate CMake **project** | `Source/Hosting/Helper/CMakeLists.txt:23-68` | Own tree (`build32`), `-A Win32`; `do_build.bat:252-273`. Depends on vendored JUCE + one source file, nothing else (`Helper/CMakeLists.txt:13-16`) |
| `BaySickEqTests` | `add_executable`, `EXCLUDE_FROM_ALL` | `CMakeLists.txt:913-923` | `Tools/EqTests/main.cpp` (1,982 lines). Plain console exe over `Source/DSP/Kbs`, no JUCE. Dies with the strip EQ. |
| `BaySickDAWAssets` | `juce_add_binary_data` | `CMakeLists.txt:32-39` | 3 PNGs. **One of the three is engine art**: `big_rusty_drums.png`, consumed only by `Source/BaySickRustyDrums/BaySickRustyDrumsKitGraphic.cpp`. The other two (`BaySickDAWLogo.png`, `control_tab.png`) are used by `StandaloneApp.cpp` (4 sites) |
| `fontaudio` | `juce_add_module` | `CMakeLists.txt:62` | Icon font compiled into `FontAudioData.cpp`; nothing installs beside the exe |

The helper arch suffix is derived from `CMAKE_SIZEOF_VOID_P` (`CMakeLists.txt:47-51`), so one source tree makes both `BaySickPluginHost64.exe` and `BaySickPluginHost32.exe`. `PRODUCT_NAME` (not `OUTPUT_NAME`) is load-bearing — the host finds the helper **by filename** (`CMakeLists.txt:858-861`).

### Static libs per vendored lib

| Lib | Target | Gate variable | Line |
|---|---|---|---|
| NeuralAmpModelerCore | `BaySickNAMCore` STATIC (12 sources, C++20, `/arch:AVX2`, `/WHOLEARCHIVE`) | `BAYSICK_HAS_NAM_CORE` | `CMakeLists.txt:250-298`, link `726-740` |
| sfizz | `sfizz::sfizz` via `add_subdirectory` | `BAYSICK_HAS_SFIZZ` | `CMakeLists.txt:307-327`, link `743-747` |
| WORLD | `BaySickWorld` STATIC (11 sources) | `BAYSICK_HAS_WORLD` | `CMakeLists.txt:348-372`, link `753-756` |
| Rubber Band R3 | `BaySickRubberBand` STATIC (single-file unit) | `BAYSICK_HAS_RUBBERBAND` | `CMakeLists.txt:381-396`, link `757-760` |
| libmp3lame | `BaySickLame` STATIC (glob of `libmp3lame/*.c`, `vector/*.c`, `mpglib/*.c`) | `BAYSICK_HAS_LAME` | `CMakeLists.txt:414-443`, link `761-764` |
| Signalsmith Stretch + Linear | header-only, include paths only | `BAYSICK_HAS_SIGNALSMITH` | `CMakeLists.txt:449-458`, `765-770` |
| concurrentqueue | header-only include dir | none | `CMakeLists.txt:492`, `710` |
| WebView2 | headers + `WebView2Loader.dll` copied post-build (dynamic `LoadLibraryA`, not linked) | none | `CMakeLists.txt:683-695`, `701`, `841-845` |
| ASIO SDK | **presence gate only** (see below) | inline `EXISTS` test | `CMakeLists.txt:135-154`, `711-712` |

### The `BAYSICK_HAS_*` gating is weaker than it looks — two of six are dead

```
grep BAYSICK_HAS_SFIZZ    Source/ → 0 hits
grep BAYSICK_HAS_NAM_CORE Source/ → 0 hits
```
Only `BAYSICK_HAS_LAME`, `BAYSICK_HAS_WORLD`, `BAYSICK_HAS_RUBBERBAND`, `BAYSICK_HAS_SIGNALSMITH` are `#if`-tested in code, and only in four files: `Source/DSP/LibraryPitchShifters.cpp/.h`, `Source/DSP/PitchCorrectorDSP.cpp/.h`, `Source/DSP/Mp3Writer.cpp`, `Source/MpglibAudioFormat.h`. sfizz and NAM headers are included **unconditionally** by `BaySickRustyDrumsProcessor.h`, `BaySickGuitarsProcessor.h`, `BaySickBassesProcessor.h`, `BaySickPedalsProcessor.h`, `BaySickGraph.h`, `EngineRig.h`, `Engine/Tasks/*` and `Source/BaySickNAMIR/`, `Source/DSP/NAMPedalStyleDSP.*`, `Source/SafeNamModel.h` — so removing those libs from the current tree is a **compile error**, not a graceful disable. Removing the engines removes the includes with them.

### What the SHELL alone needs

Confirmed needed by shell code:
- **JUCE 8.0.12** (`juce/modules/juce_core/juce_core.h:47`) — `juce_audio_utils`, `juce_dsp`, `juce_cryptography` (`CMakeLists.txt:235-241`). `juce_cryptography` is there *only* for Core Library SHA-256 (`CMakeLists.txt:238-240`) → drops with the fetcher unless reused.
- **concurrentqueue** — MT render engine (`Source/Engine/`, `CMakeLists.txt:710`).
- **WebView2** — manuals window only (`Source/Standalone/ManualsWindow.cpp:44-50`, `CMakeLists.txt:683-695`).
- **fontaudio** — icon font, linked into the standalone target (`CMakeLists.txt:718`); the shot harness hard-fails without it (`ShotHarness.cpp` font-width check, ~line 2062).
- **VST3 SDK** — via JUCE only. `JUCE_PLUGINHOST_VST3=1` (`CMakeLists.txt:127`) is the single lever; the SDK is at `juce/modules/juce_audio_processors_headless/format_types/VST3_SDK/`. `JUCE_PLUGINHOST_VST` stays OFF deliberately (`CMakeLists.txt:123-126`).
- **ASIO** — **the repo's `libs/asiosdk` is a presence gate, not the compiled headers.** `CMakeLists.txt:137` tests `libs/asiosdk/common/iasiodrv.h` and appends `JUCE_ASIO=1` + `JUCE_ASIO_DEBUGGING=1` + the include dir, but never sets `JUCE_ASIO_USE_EXTERNAL_SDK`, which defaults to 0 (`juce/modules/juce_audio_devices/juce_audio_devices.h:147-148`). JUCE therefore compiles `#include <juce_audio_devices/native/asio/iasiodrv.h>` — its own bundled copy (`juce_audio_devices.cpp:138-145`). JUCE 8 ships `asio.h`, `asiosys.h`, `iasiodrv.h`, `LICENSE.txt` at `juce/modules/juce_audio_devices/native/asio/`.
- **LAME** — needed for **both** MP3 export (`Source/DSP/Mp3Writer.cpp:5`) **and MP3 import** (`Source/MpglibAudioFormat.h:4,57,198`, mpglib decoder). JUCE's own MP3 decoder and WindowsMedia are both switched off (`CMakeLists.txt:85`, `:97`), so dropping LAME removes .mp3 read as well as write.

Not needed by the shell: **rubberband, world, signalsmith-stretch, signalsmith-linear, sfizz, NeuralAmpModelerCore** — all six are reached only from `Source/BaySickVocal/`, `Source/DSP/` pitch + NAM files, and the sampled-instrument engines.

---

## 2. Source-list variables — are engine/DSP sources cleanly separable in CMake terms?

**No. There are exactly two source lists and both are mixed.**

- `VIBESYNTH_DSP_SOURCES` (`CMakeLists.txt:157-233`, 77 lines) is the *shared* list fed to both the VST3 scaffolding target and the standalone. It already contains **BaySickSolstice (12 files), BaySickPlayer (2), BaySickSynth (6), BaySickBass (2)** — lines 179-204 — alongside genuine shell files (`PluginProcessor.cpp`, `BaySickGraph.cpp`, `ProjectManager.cpp`, `PatternManager.cpp`, `Hosting/*`, `Engine/*`).
- The standalone's own sources are a **flat inline `target_sources` block** with no variable at all: `CMakeLists.txt:512-681` (170 lines). Shell pages, every `Source/DSP/*` effect and pedal DSP, `EffectRack.cpp`, every effect panel file, and every remaining engine (`BaySickNAMIR`, `BaySickPedals`, `BaySickRustyDrums`, `BaySickGuitars`, `BaySickBasses`, `SlideSampler`, `BaySickVocal`, `Clips`, `Vox`, `Inst`) are interleaved in one list.
- Include dirs are likewise one flat list naming every engine folder: `CMakeLists.txt:703` (`Source/BaySickSolstice Source/BaySickPlayer Source/BaySickSynth Source/BaySickBass Source/BaySickNAMIR Source/BaySickPedals Source/BaySickRustyDrums Source/BaySickGuitars Source/BaySickBasses Source/BaySickVocal Source/Clips Source/Vox Source/Inst Source/MidiLearn Source/SlideSampler`).

There is no `BAYSICK_ENGINE_SOURCES` / `BAYSICK_SHELL_SOURCES` split, no per-family variable, no `option()`. Separating in the fork is line-deletion in two blocks (`157-233`, `512-681`) plus the include-dir line 703 — mechanically simple, but nothing in the current CMake expresses the boundary.

Post-build staging steps that are also edition-sensitive:
- `Resources/` copied beside the exe (`CMakeLists.txt:814-820`). Contents: `Acoustic IRs/` (2 wavs — effect only), `Tape/` (IRs + Samples — effect only), `Filmstrips/` (9 PNGs — **shared UI**, `Source/Standalone/SharedUI.cpp:10` `namespace Filmstrips`, used by shell + engine LAFs). A shell-only fork keeps `Filmstrips/` and drops the other two (25 MB total today).
- `Manuals/` copied beside the exe (`CMakeLists.txt:828-834`).
- `WebView2Loader.dll` copied (`CMakeLists.txt:841-845`).
- Helper staged into **both** Release and Debug artefact dirs (`CMakeLists.txt:898-907`).

`do_build.bat` gate contract (do not change per its own header, `do_build.bat:12-19`): `build_log.txt` must contain `RELEASE_EXIT_CODE`, `DEBUG_EXIT_CODE`, `HELPER64_EXIT_CODE`, `HELPER32_CONFIG_EXIT_CODE`, `HELPER32_EXIT_CODE` (+ `ARTEFACTS_EXIT_CODE`, `do_build.bat:289`). A fork that renames the app must keep or re-spell these five strings — everything downstream greps them.

---

## 3. Everything path- or name-sensitive

### User-data root
`Source/AppPaths.h:10-14` — the single authority: `Documents\BaySickDAW`. Header notes it was hand-spelled at 40+ sites before consolidation.

| Thing | Value | Evidence |
|---|---|---|
| App root | `<Documents>\BaySickDAW` | `AppPaths.h:12-13` |
| Global prefs / recents | `settings.xml` | `ProjectManager.cpp:28`, `.h:60-63` |
| Machine audio state | `audio_settings.xml` (+ legacy Roaming fallback) | `StandaloneApp.cpp:428-433` |
| Master routing | `master_output.xml` (sibling of audio_settings) | `StandaloneApp.cpp:440-443` |
| Editor UI prefs | `ui_prefs.xml`; `PropertiesFile::Options{applicationName="BaySickDAW", folderName="BaySickDAW", suffix="xml"}` | `StandaloneEditor.cpp:20334-20342`; also `BaySickPitchEditor.cpp:63-73` |
| Keymap | `keymap.xml` | `KeyBindings.cpp:869`, `.h:149` |
| Hosted-plugin list | `plugins.xml` at app root (deliberately **not** inside settings.xml) | `Hosting/PluginManager.cpp:12-20` |
| Project | folder, never a file: `Projects\<name>\project.xml` + `Samples\` | `ProjectManager.h:19-31`, `.cpp:312` |
| Project XML root tag | **`BaySickDAWProject`** | `ProjectManager.cpp:250`, `:333` |
| Autosave | `<project>\Backups\`, or `Backups\Unsaved\` when no project | `ProjectManager.cpp:132-135`, `197-198` |
| Presets | `Presets\<engine>\` + `My Presets` (user); factory reseed stamp `factory_seed_version.txt` vs `kFactorySeedVersion` | `ProjectManager.cpp:435`, NSI `:290-295` |
| Templates | `Templates\Factory` + `My Templates` | `ProjectManager.h:25`, NSI `:297-298` |
| Kits | `Kits\Factory` + `My Kits` | NSI `:304-305` |
| My Samples | `<Documents>\BaySickDAW\My Samples` | `SampleLibrary.cpp:39` |
| Core Library | `%LOCALAPPDATA%\BaySickDAW\CoreLibrary\` (Win) / `userApplicationDataDirectory/BaySickDAW/CoreLibrary` (fallback) | `SampleLibrary.cpp:24-35` |
| Shortcut | `Sample Library.lnk` → `"BaySickDAW Core Library"` | `SampleLibrary.cpp:62`, `ProjectManager.h:30` |
| Sample path tokens | `library:` / `mysamples:` prefixes stored instead of absolute paths | `ProjectManager.h:39-43` |
| Manual at run time | `<exe dir>\Manuals\manual.html` (F1) | `ManualsWindow.cpp:44-50`; staged by `CMakeLists.txt:828-834` |
| Shot-harness output | `<Documents>\BaySickDAW\Manuals\shots-staging` — **AppPaths, not the repo** | `ShotHarness.cpp` `run()`, `gOutDir = AppPaths::appRoot()...` |
| Docs JSON | `<Documents>\BaySickDAW\Manuals\assets\bsd-docs.json` | same function |

**No custom file extension exists.** Grep of all quoted extensions in `Source/` returns only `.xml` (32× `"*.xml"`), `.wav/.aiff/.aif/.flac/.ogg/.mp3`, `.sfz`, `.nam`, `.namir`, `.pedals`, `.zip`, `.html`, `.kbsref`, `.part`, `.active`, `.dll`, `.png`. Projects, presets, templates are all plain `.xml`; the bundle is a `.zip` or plain folder (`ProjectBundler.h:13`).

### Window / product names
- Main window title: `juce::DocumentWindow("BaySickDAW", …)` — `StandaloneApp.cpp:23`.
- Title composition: `"BaySickDAW"` + `" - <project>"` or `" - Untitled"` + `" *"` if dirty + `" [DEBUG]"` in Debug — `StandaloneEditor.cpp:21290-21309`.
- Splash: `SplashScreen ("BaySickDAW", …)` — `StandaloneApp.cpp:709`.
- Shell tab/title labels: `StandaloneEditor.cpp:1486`, `:1969`.
- Audio dialog title: `"Audio & MIDI Settings"` — `StandaloneEditor.cpp:12013`.

### Installer (`Installer/BaySickDAW-Tester.nsi`, 429 lines)
**There is no GUID anywhere** — identity is by registry path and name.

| Item | Value | Line |
|---|---|---|
| `APP_NAME` / `APP_EXE` | `BaySickDAW` / `BaySickDAW.exe` | `:68`, `:70` |
| `APP_PUBLISHER` | `KnowledgeBase Studios` | `:69` |
| `APP_VER` | read from `project(BaySickDAW VERSION …)` in CMakeLists by `make_installer.bat:87-94` | `:64-66` |
| App reg key | `HKCU\Software\KnowledgeBase Studios\BaySickDAW` | `:76` |
| Uninstall reg key | `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\BaySickDAW` | `:77` |
| Install dir | `$LOCALAPPDATA\Programs\BaySickDAW`, `RequestExecutionLevel user` | `:52`, `:127` |
| Source dir | `<repo>\build\BaySickDAWStandalone_artefacts\Release` | `:88` |
| Output name | `BaySickDAW-<ver>-<stamp>-Tester-Setup.exe` | `make_installer.bat:109` |
| Required inputs (build fails without) | app exe, both helpers, `Resources\*`, `WebView2Loader.dll`, `Presets\*`, `Templates\*`, `Kits\Factory\*` | `:104-111` |
| CMake identity | `PLUGIN_MANUFACTURER_CODE Kbst`, `PLUGIN_CODE Bsdw`, `COMPANY_NAME "KnowledgeBase Studios"` | `CMakeLists.txt:468-474` |

The NSI header states outright it is **not the shipping installer**: no manual page, no licence/EULA page, no legal review of third-party notices, no code signature (`Installer/BaySickDAW-Tester.nsi:1-11`). The `File /r <dir>\*` trailing-`\*` requirement is a documented trap (`:258-265`).

### Core Library fetcher endpoints (`Source/CoreLibraryInstaller.h`, 1,104 lines — deleted wholesale in the fork)
- Base URL: `https://github.com/KnowledgeBaseStudios/BaySickDAW-Downloads/releases/download/Content-v1/` — `:62-64`.
- 10 assets, each with exact byte length + lowercase SHA-256 — `:106-127`. Total ≈ 4.04 GB.
- HTTPS-only enforced on the initial URL and on every redirect (`:762`, `:788-792`).
- Staging dir `CoreLibraryDownload`, a **sibling** of `CoreLibrary` (`:187-188`), same-volume rename (`:248`).
- `juce_cryptography` in `VIBESYNTH_LINK_LIBS` exists for this verification (`CMakeLists.txt:238-240`).

### Stale-name findings (edition-sensitive drift already present)
- About box says **"BaySickDAW v1.0"** and **"Built with JUCE 7"** while `project()` says 1.2.0 and the vendored JUCE is 8.0.12 — `StandaloneEditor.cpp:12037-12047`, menu item `"About BaySickDAW v1.0"` at `:11874`, vs `CMakeLists.txt:2` and `juce_core.h:47`.
- CMake's JUCE-missing FATAL_ERROR tells the user to download **JUCE 7.0.12** — `CMakeLists.txt:25`.

---

## 4. Manual pipeline — engine dependencies and what a shell-only manual reuses

### The pieces (`Manual Pipeline.md:9-17`)
`Source/Standalone/ShotHarness.h/.cpp` (2,125 lines) + `ShotMenuHook.h` (13 call sites) + `ShotFactories.h` → figure PNGs and `bsd-docs.json`; `Manuals/assets/generate-manual.py` (1,259 lines) assembles `Manuals/manual.html`; `control-blurbs.py` (526), `marker-coords.py` (493); master data is `Plans & Specs/System Reference/Callout Registry.md`.

### Figure groups per engine — `kFigures[]`, `ShotHarness.cpp:1976-2020`
43 groups. Split by dependency:

**Engine/effect-bound (drop with the engines — 18 groups):** `synth family`, `baysicksolstice`, `pedals`, `rusty keys`, `effects`, `fx panels`, `pedal panels`, `player`, `vox family`, `rusty family`, `drum kit grid`, `guitars`, `basses`, `kit menus`, `fx picker`, `pedals menus`, `fx rack menu`, `fx panel menu`, `engine menus`.

**Shell-only (reusable — 24 groups):** `transport`, `ribbon`, `keybinds`, `file settings`, `export`, `vu meter`, `builder`, `piano roll`, `event editor`, `undo history`, `analyzer`, `plugin search`, `audio settings`, `roll menus`, `record menu`, `builder menus`, `mixer menus`, `ribbon menu`, `metronome`, `analyzer menu`, `editor menus`.

**Two shell groups have a hard engine dependency in their bodies:**
- `mixer` (`shootMixerCluster`, `ShotHarness.cpp:1012-1043`) calls `rig.addTab (TabKind::Bass, 0)` and `rig.setEngineType (TabKind::Bass, 0, "BaySickBass")`, then `page.addBassChannel(0,"Bass 1")`, `addVoxChannelAtIndex`, `addInstChannelAtIndex`. **This is the group the fork's FL-style mixer must be re-seeded for.**
- `eq` (`shootEqCluster`, `ShotHarness.cpp:1392-1425`) drives `ensureStripEqParams` / `preEqForChannelId` — dies with the strip EQ.

`builder` (`ShotHarness.cpp:1063-1114`) is **engine-free** — PatternManager content + an `UndoContext` only. Same for `piano roll` (`:1117`), `transport` (`:686`), `ribbon` (`:708`), `vu meter` (`:1045`).

### Figure order carries state — the contract
`Manual Pipeline.md:50-54` and the inline registry comments at `ShotHarness.cpp:1965-1969`, `:1988-1991`, `:2010-2012`:
- the empty rack shoots **before** effects load;
- Builder seeds the pattern content Piano Roll and the Event Editor read;
- engine menus ride **earlier figures' kit loads** (guitars kit, rusty kit);
- `{ "editor menus", &shootEditorMenus }` **must run last** — its `StandaloneEditor` ctor re-points the processor's PatternManager and installs process-wide static hooks (`ShotHarness.cpp:1960-1965`).

So the engine groups are not merely deletable rows: `engine menus` and `kit menus` consume state that `guitars`/`rusty family` established, and the `editor menus`-last invariant must survive whatever is removed.

### Headless rules that constrain the harness (`Manual Pipeline.md:41-48`)
Timers never fire (`pollNow()` / `pollForShot()` seams; the one exception is the vox family's `Thread::sleep(550)` + `callPendingTimersSynchronously()` — **that exception disappears with BaySickVocal**); no component reaches the desktop; default LookAndFeel installed by hand; fontaudio glyph-width check hard-fails the run (`ShotHarness.cpp` run(), returns 3).

### The docs walk
`--docs` writes `Manuals/assets/bsd-docs.json`: per figure, every stamped control (`componentID` on plain widgets, `VKnob::paramId` on knobs), slider range strings, resolved parameter metadata, percent bounds, plus every captured menu's row rects. `kExtraDocs` in `ShotHarness.cpp` documents controls the tree walk cannot see (`Manual Pipeline.md:35-38`). The walk is generic over the component tree — it is **not** engine-coded; it only sees whatever a figure builds. Blurbs in `control-blurbs.py` are keyed by param id or `"<figure>|<label>"`, so engine params drop cleanly.

### What a shell-only manual reuses — measured
`Callout Registry.md:26-27` declares **90 figures in three groups**. Table rows counted by group: **Shell 35 / Instrument 40 / Mixing & Effects 16** (Shell ordinals run 1-34 then 36; ord 35 is retired). Prose sources mirror the split exactly: `Manuals/src-m2/shell` = 35 files, `src-m2/instrument` = 40, `src-m2/mixing-effects` = 16. `Manuals/src-m3` holds 90 files (`IMP-*.html`; the registry declares 65 implementation topics — **the extra files are inferred to be retired/renumbered leftovers, not verified**).

Shipped figure PNGs: 137 files in `Manuals/figures/`. 88 of the 90 shipped figures are automated; `Main frame.png` and `Hosted Plugin.png` stay hand captures (`Manual Pipeline.md:29-30`).

A shell-only fork reuses the **Shell group (35 figures) verbatim**, plus the parts of **Mixing & Effects** that survive: `MIX`, `MIXMNU`, `MIXADD`, `MIXSTP`, `MIXSM`, `ANLZ`, `ANLZM`, `FXI`, `FXRM`, `VUMTR` (the rack itself and its menus survive as a VST3-only rack); `FXPICK`, `FX`, `FXV`, `FXM`, `EQ`, `EQB` go with the built-in effects and the strip EQ. The whole **Instrument group (40)** goes, minus `BSPLUG` / `BSPLUGM` (Hosted Plugin, Instrument ord 39-40) which are shell-relevant.

Menu-dot generation: 32 of 38 menu figures self-anchor from emitted row rects; six standing exceptions use grouped callouts — `ANLZM, BSPDLP, EQB, FXPICK, MIXADD, PRC` (`Manual Pipeline.md:91-98`). Of those six, `BSPDLP`, `EQB` and `FXPICK` die with the engines/effects; `ANLZM`, `MIXADD` and `PRC` stay. A menu that grows breaks its count **on purpose** — the fork's FL-style Mixer Add menu will trip `DOT MISMATCH` until `MIXADD`'s registry row is updated at its visual position.

---

## 5. Line counts — engines + DSP vs shell (actual `wc -l`, `.cpp` + `.h` only)

**`Source/` total: 411 files, 234,169 lines.**

### Engines + DSP + effect UI — **114,322 lines (48.8%)**

| Bucket | Lines | Detail |
|---|---|---|
| Engine folders (11) | **43,578** | Solstice 9,539 · Vocal 9,459 · Synth 5,513 · Player 4,656 · NAMIR 3,922 · Bass 2,387 · SlideSampler 2,162 · Pedals 2,028 · RustyDrums 1,907 · Guitars 1,012 · Basses 993 |
| `Source/DSP/` (118 files) | **36,347** | incl. `Kbs/ParametricEq.h` 2,256, ReverbDSP 1,853, SaturationDSP 1,444, DelayDSP 1,257, BaySickPitchDSP 1,193, LimiterDSP 1,069, BaySickAlignDSP 975, CompressorDSP 957, EffectParamMap 937 |
| Effect UI in `Standalone/` | **15,440** | EffectEditorPanels 7,424+229 · EffectWindows 2,418+331 · EffectsPage 1,795+268 · SlotComponent 1,371+219 · EffectPresetIO 767+113 · FxRackPresetIO 169+54 · EffectVisual.h 282 |
| `Standalone/EqWindowUI/` (6 headers) | **5,244** | EqGraphView 2,222 · EqRailView 1,501 · EqMatchPanel 777 · EqInstanceBrowser 326 · EqAnalyser 320 · EqPresets 98 |
| Engine pages in `Standalone/` | **11,801** | DrumKitGrid 4,137+634 · DrumPage 1,761+252 · LayersPage 1,161+184 · BassPage 1,122+160 · AriaControlPanel 1,084+169 · BaySickRustyDrumsPage 765+198 · RustyDrumsMapWindow 128+46 |
| `Source/EffectRack.cpp/.h` | **1,401** | 991 + 410 |
| Top-level synth primitives | **511** | WavetableOscillator, SynthFilter, AdsrEnvelope, LFO, SynthSound, BroadcastSynthesiser.h |

### Shell — **119,847 lines (51.2%)**

| Bucket | Lines | Detail |
|---|---|---|
| `Standalone/` minus the above | **78,043** | StandaloneEditor 21,314+1,518 · BuilderPage 10,984+1,756 · PianoRoll 6,379+815 · SharedUI 4,809+1,667 · MixerPage 4,475+623 · ShotHarness 2,125+21 · EventEditor 2,123+364 · WorkspaceWindow 1,522+553 · StandaloneApp 1,429+221 · RibbonTabBar 1,266+298 · GlobalTransportBar 1,135+262 · MasterAnalyzerWindow 995+184 · PagePresetIO 932+163 · KeyBindings 897+161 · MixerTrackStrip 821+440 · PluginsPage 778+188 · ProjectBundler 632+135 · PluginsManagerWindow 513+148 · KeyBindsWindow 495+90 · LoudnessReportWriter 416+79 · ProjectBrowserWindow 321+69 · PianoRollPage 316+206 · VersionCapture 306+215 · HeavyOperationOverlay 359+112 · BaySickTitleBar 240+119 · PatternColorPicker 194+45 · UndoHistoryWindow 171+86 · RustyDrumsMapWindow *(counted engine)* · ManualsWindow 112+45 · WindowChrome 85+71 · EngineChainProcessor 82+79 · UndoActions.h 586 · MetroPanel.h 127 · misc headers |
| Top-level minus engine primitives/EffectRack | **27,266** | PluginProcessor 9,992+2,447 · BaySickGraph 3,288+1,245 · PatternManager 2,026+1,012 · EngineRig 1,107+404 · CoreLibraryInstaller.h 1,104 · ProjectManager 772+322 · SafeSfzKit.h 464 · SampleLibrary 452+195 · UserFileSave.h 295 · MpglibAudioFormat.h 249 · MissingFileReport.h 244 · SafeNamModel.h 207 · TempoMapRead.h 206 · BaySickConstants.h 147 · SafeXml.h 145 · TsMapRead.h 141 · AudioFileRecorder 140+109 · MidiRecorder 131+94 · SafeAudioReader.h 86 · ProjectFileResolver.h 75 · ClipDropDiag.h 59 · SafeAudioFormats.h 38 · PluginEditor 30+25 · AppPaths.h 15 |
| `Source/Hosting/` (13 files) | **5,057** | VST3 hosting + both bridge ends |
| `Source/Engine/` (33 files) | **4,274** | thread pool, dispatcher, 10 task types |
| `Source/Inst/` | **1,768** | InstPage — engine-bound page, **counted as shell here; it goes with the guitar rig** |
| `Source/MidiLearn/` | **1,496** | registry + DrumTriggerMap |
| `Source/Clips/` | **1,022** | ClipsPage (one BaySickPlayer per tab) |
| `Source/Vox/` | **921** | VoxPage — goes with BaySickVocal |

**Boundary files (counted shell above, but engine-coupled — 3,711 lines):** `Clips/` 1,022 + `Vox/` 921 + `Inst/` 1,768. Also `Standalone/PagePresetIO.cpp/.h` (1,095) serves every page type including engine pages.

### Coupling density in shell files (engine class names per file)
```
Source/Standalone/StandaloneEditor.cpp   406 lines mention a built-in engine
Source/PluginProcessor.cpp                76
Source/Standalone/RibbonTabBar.cpp        47
Source/PluginProcessor.h                  36
Source/EngineRig.cpp                      29
Source/BaySickGraph.h                      9
Source/Standalone/MixerPage.cpp            8
Source/EngineRig.h                         8
Source/EffectRack.cpp                      3
Source/BaySickGraph.cpp                    2
```
(`grep -cE "BaySickSynth|BaySickBass|BaySickPlayer|BaySickSolstice|BaySickVocal|BaySickNAMIR|BaySickPedals|BaySickGuitars|BaySickBasses|BaySickRustyDrums|SlideSampler"`.) `BaySickGraph` and `MixerPage` — the two files the FL-style mixer rework lands in — are the *least* engine-coupled of the shell; `StandaloneEditor.cpp` is where the 406-line surface lives.

`EffectRack.h:19-79` is an append-only persisted enum: 12 rack effects (1-12), 18 pedal ordinals (100-118, with 101 dead), Gate/DeReverb (119/120), and `VST3Plugin = 121`. A VST3-only rack keeps ordinal 121 and must not renumber the rest (saved slots persist the raw int, `EffectRack.h:21-27`).

---

## 6. Legal prerequisites for a CLOSED-SOURCE shell

Current state: repo `LICENSE` is **GNU GPL v3** (674 lines, `LICENSE:1-2`). `THIRD_PARTY_LICENSES.md` covers **only** the four QA-Fe pitch engines and says so (`:21-25`): *"a complete third-party manifest is a pre-release `/audit-licenses` deliverable."*

### Blocker 1 — JUCE
`juce/LICENSE.md:4-8`: the JUCE 8 modules are dual-licensed **AGPLv3 or the commercial JUCE 8 licence**. Not GPLv3 — **AGPL**v3. A closed-source binary requires a paid JUCE 8 licence; the AGPL branch would force source disclosure (and, for AGPL, network-use disclosure). This is the single hardest prerequisite and it applies to *all four* binaries (app + both helpers + the legacy VST3 target). `JUCE_DISPLAY_SPLASH_SCREEN=0` is set (`CMakeLists.txt:68`) — that switch is only legitimate under the commercial licence.

### Blocker 2 — ASIO SDK
`libs/asiosdk/common/LICENSE.txt` and the identical `juce/modules/juce_audio_devices/native/asio/LICENSE.txt`: dual **Proprietary Steinberg ASIO License or GPLv3**. The proprietary branch requires *"a copy of the License Agreement signed by Steinberg Media Technologies GmbH"* **before publishing** (`libs/asiosdk/common/LICENSE.txt:24-26`) and forbids redistributing the SDK in whole or part without written agreement (`:15-17`). For closed source the GPLv3 branch is unavailable, so the signed Steinberg ASIO agreement is mandatory. Note also that the *shipped headers* come from JUCE's bundled copy, not `libs/asiosdk` (see §1) — the notice that must ship is JUCE's.

### Blocker 3 — LAME (LGPL), if MP3 stays
`libs/lame/COPYING` is the licence text; version 3.100 (`CMakeLists.txt:398-413`). It is **statically linked** into `BaySickDAWStandalone` (`add_library(BaySickLame STATIC …)`, `CMakeLists.txt:420`; linked `:762`). Static LGPL linkage in a closed-source binary obliges the relink provision: ship the LAME source (or a written offer), plus either object files / a static-lib archive of the rest of the app or a shared-library form, so a user can relink against a modified LAME. The About box already discloses LAME *"because the LGPL wants the use disclosed somewhere the user can see"* (`StandaloneEditor.cpp:12040-12047`) — disclosure alone does not satisfy the relink obligation. Dropping MP3 entirely also drops **MP3 import** (`Source/MpglibAudioFormat.h`), since JUCE's MP3 and WindowsMedia decoders are both off (`CMakeLists.txt:85`, `:97`).

### Must ship with the binary (permissive, notice-only)

| Component | Licence | Text location | Obligation |
|---|---|---|---|
| VST3 SDK (via JUCE) | **MIT**, © 2025 Steinberg | `juce/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt:2-22`; listed at `juce/LICENSE.md:49` | Reproduce copyright + permission notice. **Separately**: `VST3_Usage_Guidelines.pdf` ships in that folder — the VST trademark/logo usage terms are not covered by MIT (**inferred**: a signed Steinberg VST 3 licensing agreement is required to distribute a product using the VST name/logo; not verified from the PDF) |
| WebView2 SDK | Microsoft BSD-3-style | `libs/webview2/LICENSE.txt:1-16` | Binary redistribution must reproduce the copyright notice and disclaimer "in the documentation and/or other materials". `libs/webview2/NOTICE.txt` carries Microsoft's third-party notices and an LGPL reverse-engineering carve-out (`NOTICE.txt:15-18`) — ship it verbatim. The `WebView2Loader.dll` is redistributed beside the exe (`CMakeLists.txt:841-845`, NSI `:256`) |
| fontaudio | **MIT only in-tree** | `libs/fontaudio/LICENSE` | **Gap:** `CMakeLists.txt:56-61` states three licences — *OFL 1.1 (the font), MIT (module code), CC BY 4.0 (source SVGs)* — but `libs/fontaudio/` contains **only** the MIT text (9 files: `LICENSE`, `fontaudio.cpp/.h/.mm`, `data/FontAudioData.cpp/.h`, `data/Icons.h`, `src/FontAudio.cpp/.h`). No OFL 1.1 and no CC BY 4.0 text is bundled. The font binary is compiled into `FontAudioData.cpp`, so the OFL notice obligation ships with the exe |
| moodycamel concurrentqueue | see `libs/concurrentqueue/README.txt` (not read) | `libs/concurrentqueue/` | notice-only (**inferred** — simplified-BSD/Boost dual is upstream's norm; not verified here) |
| JUCE's own bundled deps | zlib, FLAC, Ogg Vorbis, jpeglib, pnglib, HarfBuzz, SheenBidi, CHOC/QuickJS, LV2, Box2D, GLEW/Mesa/Khronos, AAX, ASIO | enumerated at `juce/LICENSE.md:36-56` | Whichever of these link into the standalone need their notices in the shipped attribution file |

### Falls away with the engines (no longer a fork obligation)
sfizz (BSD-2, credits-screen attribution), NeuralAmpModelerCore + Eigen + nlohmann/json, **Rubber Band R3 (GPL v2-or-later — the one copyleft dependency other than JUCE/ASIO that a closed-source shell could not carry)**, WORLD (modified BSD), Signalsmith Stretch/Linear (MIT), LunaSVG. All four pitch engines are documented in `THIRD_PARTY_LICENSES.md:9-19`.

### What the current shipping surface does **not** have
`Installer/BaySickDAW-Tester.nsi:1-11` states plainly: no licence or EULA page, no legal review of third-party notices, no code signature, and *"The real release installer is owned by the QA-Installer batch, which cannot run until QA-Manuals, QA-Templates and QA-LegalReview have run - none of them have."* The About box's "Powered by" list is explicitly marked **INCOMPLETE** in the source comment (`StandaloneEditor.cpp:12034-12037`). There is no `NOTICES.txt` / `THIRD-PARTY-NOTICES` file staged beside the exe anywhere in `CMakeLists.txt` — the only staged trees are `Resources/`, `Manuals/`, and `WebView2Loader.dll`.

---

# READER 6: SEQUENCER + PROJECT PERSISTENCE — grounded facts

# SEQUENCER + PROJECT PERSISTENCE — grounded facts

Docs read first (`Plans & Specs/System Reference/`): `INDEX.md`, `Builder Page.md`, `Patterns and Arrangement.md`, `Automation.md`, `Piano Roll.md`, `Clips Page.md`, `Projects and Saving.md`, `Freeze and Export.md`. Every claim below re-confirmed in code; doc-only claims are labelled.

---

## 1. Builder: rows, tabs, strips, audio clips, automation

### 1a. Row → tab → strip: there is **no** row↔tab↔engine binding

| Fact | Evidence |
|---|---|
| A row is only a name + mute/solo + group + colour. 500 rows, free-form. | `Source/PatternManager.cpp:1289` (`RowState` node: `mute`/`solo` as `kMaxArrangementRows`-char strings, sparse `<Row i name group groupColor>` children at `:1306-1311`) |
| A **pattern** block on any row schedules **every instrument family's** notes for that pattern. The row is irrelevant to which engine plays. | `Source/PluginProcessor.cpp:3308-3404` — one loop over blocks, then `sched()` fanned into `layerPageMidi / bassPageMidi / drumPageMidi / clipPageMidi / voxPageMidi / instPageMidi / pluginPageMidi / mRustyDrumsMidi` |
| The row's only effect on a pattern block is mute/solo. | `Source/PluginProcessor.cpp:3313` `if (!mPatternManager->isRowAudible(blk.trackRow)) continue;` |
| An **audio** clip routes by `ArrangementBlock::routeChannel` (an owning page's mixer channel id), **not** by its grid row. | `Source/PatternManager.h:402`; `Source/PluginProcessor.cpp:1113-1124` (`ownerRow = routeChannel - kAudioBase`, else fall back to `player.trackRow`) |
| Legacy migration stamps `routeChannel = audioInsert(trackRow)` once, at rebuild time. | `Source/PluginProcessor.cpp:5800-5801` |
| Row mute and *strip* mute are separate gates for an audio clip. | `Source/PluginProcessor.cpp:1128-1129` (`mx.audioRowMute[row]` vs `!isRowAudible(player.trackRow)`) |
| Channel-id space for strips. | `Source/BaySickGraph.h:33-93` — buses 1–18, `kAuxBase 100`, `kLayerBase 200`, `kBassBase 300`, `kAudioBase 400`, `kDrumBase 500`, `kVoxBase 600`, `kInstBase 700`, `kRustyBase 800`, `kPluginBase 900`, `kDirectBase 950` |
| The *lane-id / Effects-dropdown* channel space is a **different** numbering (drums 100+, layers 200+, bass 300+, audio 400+, aux 600+, vox 700+, inst 800+, rusty 900+, plugin 1000+, direct 1100+). | `Source/Standalone/EffectsPage.cpp:1483-1525`; walked in `Source/Standalone/BuilderPage.cpp:10589-10603` |

The only "row is a tab" relationship in the whole subsystem is the **Clips page ⇄ audio-insert row identity**: page index == audio row == `audioInsert(row)` == `mixer_audio_<row>` (doc `Clips Page.md` §"The page index IS the audio row"; code `Source/Standalone/StandaloneEditor.cpp:10539-10572`).

### 1b. Audio clips on the grid — is a Clips **tab** required?

**No. The tab is optional; the audio-insert mixer strip is not.**

- Grid clips are decoded by `CompositeAudioInsertTask` per audio row, additively into the row's insert buffer, then one `processInsert` pass: `Source/Engine/Tasks/CompositeAudioInsertTask.cpp:120-167`; helper `BaySickDAWProcessor::renderAudioClipsForRow` at `Source/PluginProcessor.cpp:1070`.
- The Clips page's `BaySickPlayer` engine is an **optional shaping layer** on that decode: `clipCtx.clipPlayer = mClipPlayer.load(...)` (`CompositeAudioInsertTask.cpp:156`), read once per row per block by `readClipCtl` (`PluginProcessor.cpp:995`); `ClipCtl.active == false` when the pointer is null and "the decode stays raw (pre-batch behavior)" (`PluginProcessor.cpp:975-978`).
- `mClipPlayer` is only set through `registerClipEngine` (`PluginProcessor.cpp:8751`, driven from `Source/EngineRig.cpp:736`).
- Project load rebuilds an audio strip for **every** block owner row with no tab involvement: `StandaloneEditor::restoreAudioStripsFromArrangement` at `Source/Standalone/StandaloneEditor.cpp:19828-19846` (`addAudioRowChannel` → `ensureAudioInsert` → `addAudioChannel` → `rebuildAudioClipPlayers`).
- The drop path *does* spawn a tab: `createClipStripAndPage` registers the strip trio then `spawnClipsTabIfMissing` (`StandaloneEditor.cpp:10559-10572`); but a clip whose `routeChannel != 0` (routed to a Vox/Inst page) skips the spawn entirely (`Source/Standalone/BuilderPage.cpp:5745-5753`).
- **Direct to Master** strips are files playing into master with *no page and no engine at all*: `Source/BaySickGraph.h:79-82`, `Source/PluginProcessor.cpp:8391-8431`.

### 1c. AudioClipStreamer + PhaseVocoder

| Fact | Evidence |
|---|---|
| One `AudioClipPlayer` per audio block, holding a `ClipSource` (shared decoded PCM **or** an owned `AudioClipStreamer`) plus an **always-created** `PhaseVocoder` and pre-allocated scratch. | `Source/PluginProcessor.h:902-996` (`source` :935, `vocoder` :939, `pvInBuf/pvOutBuf` :941-942) |
| Files ≤ 100 MB float-PCM are decoded whole and cached by resolved-path+size+mtime; bigger ones stream. | `Source/DSP/AudioClipStreamer.h:48` (`kRamThresholdBytes`); cache `Source/PluginProcessor.h:889-900`; rebuild `Source/PluginProcessor.cpp:5813-5828` |
| Streamer = SPSC 4-second ring + `TimeSliceClient` background prefetch; audio thread calls `readAndMix` with a stateless *double* file position. | `Source/DSP/AudioClipStreamer.h:19-86` |
| The render bypasses the vocoder when the clip is forward + unstretched + unpitched. | `Source/PluginProcessor.h:936-939` |
| Vocoder is a Laroche–Dolson phase vocoder; `kHopSize` is a wire format shared with the offline stretch decode and `BaySickAlignDSP`. | `Source/DSP/PhaseVocoder.h:5-60` |
| Snapshot publication: `rebuildAudioClipPlayers` (message thread, sole mutator) atomic-exchanges an `AudioClipSnapshot` and retires the old to a `RetirementQueue` drainer so `~AudioClipStreamer` never lands on audio. | `Source/PluginProcessor.cpp:5763-5772`; `Source/PluginProcessor.h:1019-1050` |
| Rebuild trigger: `ArrangementGrid::commitEdit` → `onArrangementChanged` → editor → `rebuildAudioClipPlayers`. | `Source/Standalone/BuilderPage.cpp:1425, 2152, 3950, 4375, 5234, 5583, 6085`; wired at `Source/Standalone/StandaloneEditor.cpp:2851` |
| Offline: streamers switch to synchronous blocking reads. | `Source/PluginProcessor.cpp:7996-7998` (`AudioClipStreamer::sOfflineRender`) |

### 1d. Automation lane target resolution

Two maps keyed by `paramId`, both on the editor: `mAutomationApplicators` / `mAutomationValueReaders` (`Source/Standalone/StandaloneEditor.h:1357,1361`). Registration is **model-side**; every applicator re-resolves through the model at apply time.

Resolution classes, in the order `applyOfflineLaneValue` tries them (`Source/Standalone/BuilderPage.cpp:10347-10604`):

| Class | Lane id shape | Resolution | Live registration |
|---|---|---|---|
| Main APVTS (mixer strip level/pan/width/mute/solo/polarity/bypass/arm, sends, both EQ banks, globals) | the raw param id | `apvts.getParameter(pid)` — consumed by `processBlock` before the offline sweep (`BuilderPage.cpp:10336`) | `registerStaticAutomationHandlers`, `StandaloneEditor.cpp:17572-17619` |
| Mixer fader alias | `<prefix>_fader` → `<prefix>_level` | derived alias | `StandaloneEditor.cpp:17608-17617`; offline fallback `BuilderPage.cpp:10528-10534` |
| Hosted VST3 **instrument** tab | `plugtab<N>_vst_<pluginParamId>` | `rig.engineFor(TabKind::Plugins,N)` → `HostedPluginInstance::applyParamNorm` | `registerPluginTabAutomation`, `StandaloneEditor.cpp:15882-15902`; re-armed by `onParamListArrived` (`:15866-15872`) |
| Vox / Inst page engine | `vox<N>_<bareId>` / `inst<N>_<bareId>` | `rig.findTab(kind,N)` → engine APVTS (Vox also its embedded NAM/IR) | `registerModelEngineAutomation`, `StandaloneEditor.cpp:15613-15624` |
| Pedal board | `inst<N>_pedals_<slotUuid>_<suffix>` | slot **by UUID**, `EffectParamMap::applyNorm(type, variantOf(..., PanelContext::Pedal), ...)` | `registerPedalAutomation`, `StandaloneEditor.cpp:15793-15820` |
| Layers/Bass/Drums/Clips engine params | the engine's own globally-unique APVTS id (`tk_<trackId>_<fam>_*`) | `rig.forEachEngine` sweep | `StandaloneEditor.cpp:15603-15611` |
| sfizz trio (`bgg_<i>_`, `bbb_<i>_`, `brd_`) | bare param id | `forEachSfizzApvts` sweep (processor-owned, not rig-owned) | `registerSfizzEngineAutomation`, `StandaloneEditor.cpp:15642-15702` |
| BaySickSolstice mod matrix | `<targetParamId>_mod<N>_depth|length` | `BaySickSolsticeModRegistry` fields, not APVTS | `registerBaySickSolsticeModAutomation`, `StandaloneEditor.cpp:15704-15791` |
| Effect-rack slot (incl. hosted VST3 **effects**) | `<channelPrefix>_<slotUuid>_<suffix>`; `output_vol`; `vst_<id>` | channel by prefix (`EffectsPage::channelPrefixForId`), slot by UUID, DSP via `EffectParamMap` | `EffectsPage::registerRackAutomationForAllChannels` (`StandaloneEditor.cpp:19860`) |
| Tempo | `global_tempo` | `mPlayHead.setLiveTempo` (never rewrites the stored base tempo) | `StandaloneEditor.cpp:17629-17645` |

Replay paths, all sharing `evalAutomationLaneAt` (`Source/PatternManager.h:84`):
- audio thread, main-APVTS lanes only: `Source/PluginProcessor.cpp:3495-3535`
- editor 30 Hz tick, everything else + stopped-seek APVTS writes: `Source/Standalone/StandaloneEditor.cpp:3950-4005`
- offline: `applyOfflineAutomationAt` (`BuilderPage.cpp:10318`) → `applyOfflineLaneValue` (`:10347`)

All four gate on the same eligibility: `clipType==Automation && !muted && isRowAudible(trackRow) && !points.empty()`.

De-registration is only two things: `unregisterAutomationForTab` by prefix (`StandaloneEditor.cpp:17521-17569`; prefixes `tk_<trackId>_`, `inst<N>_`, `vox<N>_`, `plugtab<N>_`; `Rusty` deliberately returns), and `unregisterAutomationForSlotUuid` by `_<uuid>_` fragment (`:17500-17519`). Both maps are cleared and re-seeded at every project boundary (`:15506-15514`).

Lane list for "New Automation Clip" / Event Editor browser is literally the applicator map's key set (`StandaloneEditor.cpp:3806-3812`); the stale-lane detector is "not in APVTS and not in the applicator map" (`:3830-3851`).

---

## 2. Piano roll → engine MIDI

### EngineKind
`Source/Standalone/PianoRollPage.h:38`
```
enum class EngineKind { DrumKit, Layer, Bass, Drum, Clip, Vox, Inst,
                        BaySickGuitars, BaySickBasses, BaySickRustyDrums, Plugin };
```
`DrumKit=0 … Plugin=10`. `EngineId {kind,index}` at `:40-47`.

### The connection (there is no type named `EngineConnection`)
`PianoRollConnection` — `Source/Standalone/PianoRollPage.h:53-96`. Closure-based, **no engine pointer**: `dataAccessor` returns a `PianoRollData*` out of the current pattern; plus `patternTimeSigProvider`, `noteColor`, `displayName`, `engineType`, three audition closures, `rollMode`, `noteLabelProvider`, `keyswitchLabelProvider`, `noteEditContextProvider`, `defaultTopNote`, `allKeysWhite`. Registered/unregistered via `PianoRollPage::registerEngine/unregisterEngine` (`:113-114`), stored in `mConns` (`:189`).

### Note → engine dispatch (song + pattern mode)
`Source/PluginProcessor.cpp:3350-3404` — per family, `sched(sPat.<fam>Notes[i], <fam>PageMidi[i], k<Fam>PRTarget + i, swingMix, swingTrunc)`. Per-family activity atomics gate the loops; the Plugins family gates on the engine pointer itself: `if (mPluginEngines[pi] != nullptr)` (`:3396-3399`). PR-target bases are **derived by summing the page caps in order** and must stay append-only: `Source/BaySickConstants.h:26-39` (`kPluginsPRTarget = kRustyPRTarget + 1`). Buffers reach the tasks through `BlockContext` (`Source/Engine/BlockContext.h:54-57`), consumed at `Source/Engine/Tasks/EngineInsertTask.cpp:47`.

### Audition
Two mechanisms.
- **Built-in engines**: `conn.auditionMomentary/On/Off` call the engine's own `auditionNote()`.
- **Hosted VST3**: no such API, so the closures feed the **live-MIDI collector**, which routes by `setLiveMidiTarget` — `StandaloneEditor::registerPluginPianoRoll` at `Source/Standalone/StandaloneEditor.cpp:10967-11040`. The live target is pushed from `mPianoRollPage->onEngineSelected` (`:2378-2396`, `mProcessor.setLiveMidiTarget((int) id.kind, id.index)`), so roll-active tab and live target agree by construction.
- Live-MIDI routing switch, keyed on the **EngineKind ordinal**: `Source/PluginProcessor.cpp:3585-3594` — `1 Layer / 2 Bass / 3 Drum / 4 Clip / 7 Guitars / 8 Basses / 9 Rusty / 10 Plugin`. Kinds 0 (DrumKit grid), 5 (Vox), 6 (Inst live-input) are deliberately dropped. Kind 8 gets a hardcoded `-12` live transpose (`:3605`).
- The roll dropdown enumerator skips tabs with no MIDI-driven engine: `StandaloneEditor.cpp:2344-2366` (`PluginsPage` with `getEngineProcessor()==nullptr` is skipped; `InstPage::Source::LiveInput` skipped).

### Riff Machine / generators — **no engine dependency**
`toolRiffMachine` and the whole Tools set live inside `PianoRollContainer` (`Source/Standalone/PianoRoll.h:202`). The only engine-ish string in `PianoRoll.cpp` / `PianoRollPage.cpp` / `EventEditor.cpp` is a comment about `allKeysWhite` for Rusty Drums (`Source/Standalone/PianoRoll.cpp:226`). Confirmed by grep for every built-in engine class name across those three files — one comment hit, zero code hits.

---

## 3. Project XML

### Top-level shape
`Source/PluginProcessor.cpp:6992-7006` (documented) / `:7011-7060` (written); written to `<project>/project.xml` by `Source/ProjectManager.cpp:250-251`.

```
<BaySickDAWProject version="1">
  <Processor>            writeProcessorState        PluginProcessor.cpp:7070
    <APVTSState> … <BaySickRackStates> …
  <PatternManager version="1" currentPattern globalTempo>   PatternManager.cpp:1246
    <Mixer …>                                               :1254
    <RowState mute solo> <Row i name group groupColor>       :1289 / :1306
    <Patterns><Pattern name bars stepsPerBar color tsNum tsDen tsLocked tsBoundUid>
        <Rolls>  LayerRoll BassRoll DrumRoll DrumPageRoll ClipPageRoll
                 VoxPageRoll InstPageRoll PluginPageRoll BaySickRustyDrumsRoll   :1334-1397
    <Arrangement><Block …><AutomationLane><Point …>          :1406 / :1409 / :1452
    <TimeMarkers><Marker bar label>                          :1459
    <TimeSigChanges currentUid nextUid><TS bar num den uid linkedPattern>  :1469
    <TempoChanges><Tempo bar bpm>                            :1485
    <AudioLibrary><Entry path alias chokeGroup pageOwnerChannelId
                          libPitch libBPM libStretch group>  :1498
    <AudioGroups><Group cat name>                            :1517
    <AutomationTemplates><AutomationLane …>                  :1528
  <DenoiseProfiles>  (only when non-empty)   PluginProcessor.cpp:7028
  <MidiCCMappings>                                           :7049
  <DrumTriggers>                                             :7052
  <DirectToMaster><Strip idx name path>                      :8391
  <UIState version="1">                       StandaloneEditor.cpp:14842
</BaySickDAWProject>
```

`<Block>` attributes: `trackRow, patternIndex, startBar (derived, display/legacy), lengthBars, lengthTicks, layerTrack, clipType, audioFilePath, displayAlias, pitchSemitones, originalBPM, stretchMode, muted, routeChannel, alignBake, isOverride, contentStartSamples, startTicks (authoritative), contentOffsetTicks` — `PatternManager.cpp:1410-1450`. `clipType` ordinals are pinned append-only (`PatternManager.h:342-350`), as is `CurveType` (`:21-28`).

`<AutomationLane>`: `paramId, userDisplayName, lastKnownName, isLFO, lfoShape, lfoRate, lfoMin, lfoMax` + `<Point time value curve tension>` — `PatternManager.cpp:1150-1171`.

### `<UIState>` contents
`StandaloneEditor.cpp:14842-15027`: `<Tabs>` (below) + strip names/orders (`serializeStripNamesAndOrders`, `:15285`) + `<Windows>` (`<W key x y w h>`, `<Open key view lock>`, `<VisClosed key>`) + `<ControlLane h visible>` + `activeTab` + `mixerScrollX` + `drumKitBank` + `<Metronome>` + `<VUCalibration>` + `<MeterLatencyComp>` + `<SongLoop>` + `<Arrangement ppBar barOff selStart selEnd>` + `<PianoRollSelection kind index>` (kind stored as a **name**, exhaustive switch at `:15008-15021`).

### Tab records and engine-type strings
`StandaloneEditor::serializeTabsInto`, `Source/Standalone/StandaloneEditor.cpp:15050-15277`:

| `type=` | `engine=` | extras |
|---|---|---|
| `"Plugins"` (:15072) | plugin **identifier string** (`pp->getEngineType()`) | `engineData` = opaque `HostedPluginInstance` blob (description + bridge preference + plugin state) |
| `"Layers"` (:15081), `"Bass"` (:15090), `"Drum"` (:15101) | engine **name string** | `engineData` base64 XML |
| `"Clips"` (:15115) | `(int) ClipsPage::EngineType` — `0 None, 1 BaySickPlayer` (`Source/Clips/ClipsPage.h:39`) | `clipPath` = `SampleLibrary::refForPersist` |
| `"Vox"` (:15125), `"Inst"` (:15134) | `(int) EngineType` | Inst adds `instChainState`, `source` (`"BaySickGuitars"`/`"BaySickBasses"`), `kitPath`, `sfizzEngineData`, `<ProgramStateCache>` |
| `"BaySickRustyDrums"` (:15188) | — | `engineData` only, `pageIndex` always 0 |

Engine name strings: `{"BaySickSolstice","BaySickSynth","BaySickPlayer"}` (`Source/Standalone/LayersPage.cpp:391`); tag→name map `bso/bsp/bss/bsb` at `StandaloneEditor.cpp:4381-4385`.

Every record also gets `locked` (:15214) and, project-shape only, `frozen / frozenBy / freezeStale / freezeScope / freezeBeats / freezeBpm / freezeStamp` + one `<FreezePattern index stamp>` per cached pattern (:15238-15273). The type→`TabKind` map is duplicated on the load side at `:18175-18183` — the match strings are the **record** types (`"Drum"`, `"BaySickRustyDrums"`), not TabKind names, and both comments record that a mismatch here silently dropped freezes.

### A project with only hosted plugins
- `<Tab type="Plugins" pageIndex name engine engineData locked>` per instrument tab.
- No `<Tab>` for effects — hosted VST3 **effects** live inside `<BaySickRackStates>` under `<Processor>` as `EffectType::VST3Plugin` rack slots (restore/retry at `Source/Standalone/EffectsPage.cpp:900-1003`).
- Notes in `<Rolls><PluginPageRoll page=N>`.
- Automation lanes as `plugtab<N>_vst_<id>` for the instrument, `<channelPrefix>_<uuid>_vst_<id>` for a rack effect.
- Mixer/routing/sends/EQ entirely inside `<Processor>` APVTS; freeze state per tab.
- Restore order (`StandaloneEditor.cpp:18202-18305`): page → ribbon tab → hooks → roll registration → **stash the description from the blob** (`stashPluginRestoreDescription`, :18234) → `selectPluginById(engine)` → `applyEngineState(..., xmlBlob=false)` (:18252) → automation. A plugin that cannot be produced by either the added list or the stashed description files `MissingFileReport::add("VST3 instrument", engine)` (:18249).

### Missing files + `ProjectFileResolver`
- `ProjectFileResolver` is a header-only global slot holding one `std::function<juce::File(const juce::String&)>`, installed by the processor on project-folder change, lock-guarded, callable copied out before invoke; falls back to `SampleLibrary::resolvePersistedRef` when uninstalled. `Source/ProjectFileResolver.h:34-75`. It exists because engine/effect `setStateInformation` is reached through racks that no processor reference passes through (`:20-25`).
- `BaySickDAWProcessor::resolveProjectFile` understands `Samples/<name>`, absolute paths, and `library:`/`mysamples:` refs (`ProjectFileResolver.h:11-18`; used at `PluginProcessor.cpp:5806`).
- `MissingFileReport` — thread-safe global `{what, path}` list with a single queued dialog; a drain landing while one is queued merges into it. `Source/MissingFileReport.h:24-60`. Drained at `PluginProcessor.cpp:7369` (`reportMissingFilesIfAny()`), and with a `"template"` noun at `StandaloneEditor.cpp:8805-8809`.
- Also used for corrupt engine blobs: `MissingFileReport::add("Engine settings (corrupt data)", tabLabel)` (`StandaloneEditor.cpp:18126`).
- Builder-side Locate: `BuilderPage.cpp:1178` (Exports), `:1231-1235` (library entry), grey+`+` painting at `:3217-3230` and `:613`.

### Load ordering (project-load shield)
`deserializeProject` at `PluginProcessor.cpp:7293`; `onDeserializeUIState` at `:7359`; `deserializeDirectStrips` at `:7364`; shield lowered `:7367`. Then `restoreAudioStripsFromArrangement` (`StandaloneEditor.cpp:19786`): audio strips → `rebuildAudioClipPlayers` (:19846) → `applyPendingRackStates` (:19854) → `EffectsPage::registerRackAutomationForAllChannels` (:19860) → `restorePendingFreezes` (:19865) → tempo sync (:19870).

### Undo
`Source/Standalone/UndoActions.h`: `ArrangementEditAction` (:137, snapshot = blocks + rowNames/groups/colors/muted/soloed), `AutomationLaneEditAction` (:296), `StructuralOpAction` (:334, owns snapshot files), `PatternListSnapshot`/`PatternListAction` (:381/:392, patterns + currentPattern + blocks + row state), `MarkerSetAction` (:484), `AudioLibrarySnapshot`/`AudioLibraryAction` (:513/:521), `AutomationTemplateAction` (:560). Dirty is a transaction-count comparison, not a flag (doc `Projects and Saving.md` §"The unsaved marker"; `TransactionTracker` in `Source/PluginProcessor.h`).

---

## 4. Recording

### Audio
- Arm is a **per-strip APVTS bool, created only on Vox and Inst inserts**: `Source/PluginProcessor.cpp:8979-8983` — `if (kind == Insert && (prefix.startsWith("mixer_vox_") || prefix.startsWith("mixer_inst_"))) addB(prefix + "_arm", ...)`. Layer/Bass/Drum/Audio/Aux/Plugin strips have no arm param at all.
- `startRecording` scans exactly those two families: `PluginProcessor.cpp:6628-6696` (`scan("mixer_vox_", kMaxVoxStrips, kVoxBase, "Vox", isVox=true)` and `scan("mixer_inst_", ...)`); Vox additionally spins a **wet** recorder.
- **No strips armed → master output capture** (post-PDC, leading samples trimmed): `PluginProcessor.cpp:6707-6726`.
- Where files land — `StandaloneEditor::commitRecordingResult`, `Source/Standalone/StandaloneEditor.cpp:20760`:
  - **Master take → `<project>/Exports/`**, *not* auto-dropped on the grid (`:20898-20918`); a failed move falls back to `dropWavAsClip(...,routeChannel=0)`.
  - **Per-strip takes → `<project>/Samples/`**, added to the audio library and placed as a `ClipType::Audio` block with `routeChannel` = the originating Vox/Inst insert id (`:20784-20887`). For Vox, dry+wet both enter the browser but only the picked variant goes on the grid (`:20936-21010`, four take types + de-noise variants).
  - `routeChannel == 0` is the only case that creates a **new** audio row + `InsertNode` + mixer strip (`:20876-20887`).
  - Pre-roll: block gets `contentStartSamples = res.preRollSamples`, and the visible clip length is file length minus pre-roll (`:20813-20862`).
  - The whole take is **one** undo transaction (library slice + pattern slice), captured at `:20768-20773`.
- `_inputChannelIdx` (Int, −1..127, default −1) is the per-strip live-input pick: `PluginProcessor.cpp:9921-9946`.

### MIDI
- Target is `mLastRollKind` / `mLastRollIndex`, and the enum only has **Layer, Bass, Drums**: `Source/Standalone/StandaloneEditor.h:1081`. Set in `updateActiveTabState` by `dynamic_cast` to `LayersPage`/`BassPage`/`DrumPage` only (`StandaloneEditor.cpp:5858-5885`).
- Record is **refused** when `mLastRollKind == None`, with a dialog naming Layers/Bass/Drums (`StandaloneEditor.cpp:1134-1148`).
- Commit writes into `pat.layerRoll[i] / pat.bassRoll[i]`, or demuxes to `pat.drumRolls[]` by drum-trigger note binding (`:21110-21215`), applying pre-roll shift, noodling discard, early-strike clamp and `Unified_RecordQuantizeDiv` snap; then `mPM->notifyContentChanged()` (:21221).
- **There is no MIDI-record path to Clips, Vox, Inst, Plugins or Rusty rolls.**

---

## 5. Export / render / freeze

### One offline loop
`BuilderPage::runOfflineLoop` — `Source/Standalone/BuilderPage.cpp:9278`. Three consumers, none of which copy it:
- `renderToFile` (:9597) — writers + stem arena taps
- `measureRender` (:10157) — meters, writes nothing
- `renderFreezeFile` (:9858) and `renderKitFreezeFiles` (:10029) — tap one strip

Session bracket is `enterOfflineRender`/`leaveOfflineRender` via `ScopedOfflineSession` (`:9218-9222`, `:9239`), opened **on the message thread by the caller** because entering/leaving activates and deactivates hosted VST3s (`:9251`, `PluginProcessor.cpp:8000-8012`).

Inside the loop, per block: the tempo-aware `OfflineHead` clock, then `applyOfflineAutomationAt(head.mBeatPos)` (`BuilderPage.cpp:9502`), then `processBlock`, then `consumeBlock`. Scope maths uses `mPM.getSongEndBeats()` (`PatternManager.cpp:541`) for Song, ruler selection for Section, `getPatternContentBeats` for Pattern — with an explicit warning that `Pattern.bars` is dead data (`BuilderPage.cpp:9328-9342`). Pattern scope also has to overwrite and restore `mLoopStartBeats` / `mCachedPatternLoopBeats` because those atomics are normally written only by the live transport (`:9404-9414`).

### What `beginOfflineRender` needs
`Source/PluginProcessor.cpp:7939-8039`:
- CAS: one render at a time (`:7947-7949`)
- `suspendProcessing(true)` + 30 ms settle (`:7954-7955`)
- saves prev SR / block / song-mode / playhead / **load shield** (`:7957-7969`)
- `sweepNonRealtime(true)`: `setNonRealtime` on the processor, on **every rig engine**, on the vocal's embedded NAM/IR, on the 30 Guitars + 30 Basses engines, on Rusty, and on **every rack slot** via `mVibeGraph.setAllRackSlotsNonRealtime` (`:7971-7990`) — the last one is what reaches hosted VST3 effects
- `AudioClipStreamer::sOfflineRender = true` (`:7998`)
- `republishTempoMapAtRate(renderSampleRate)` (`:8025`), `mVibeGraph.reset()` (`:8028`), full `prepareToPlay` at render config (`:8036`)

So the offline drive needs **the graph and the engine registry**, not any particular engine type — except the `dynamic_cast<BaySickVocalProcessor*>` at `:7978` and the two hardcoded sfizz arrays at `:7981-7986`.

### Freeze
- Tap is graph-level, **pre-rack**: `graph.armFreezeTap(kind, index)` (`BuilderPage.cpp:9894`), buffer read via `getFreezeTapBuffer()` with a **sequence-number stale guard** (`:9950-9962`); `opts.trimLeadingLatency = false` because the tap is upstream of PDC (`:9868`).
- Which insert and which task: `insertKindForTab` (`PluginProcessor.cpp:4405-4422`, exhaustive, no `default`) and `renderTaskForTab` (`:4372-4398`) — Layers/Bass/Drums/Plugins/Vox/Inst/**Clips** (Clips maps to `mAudioRenderTasks[pageIndex]`, i.e. the composite audio-insert task, so freezing a Clips tab freezes both the engine trigger *and* the grid clip decode). Rusty is 13 tasks and has no single-index entry (`:4394` comment, `EngineRig.cpp:276`).
- Files: `freezeFileFor(kind, pageIndex, patternIndex)` → `<project>/Freeze/tab_<kindName>_<idx>_song.wav` or `_pat<N>.wav`; **the kind is spelled as a name, never the TabKind ordinal** (`PluginProcessor.cpp:4428-4452`).
- `setFreezePrune(target)` skips run() on everything the target does not depend on (`BuilderPage.cpp:9934`).
- Freeze **state** is model-side on `EngineTab`: `frozen, frozenByUser, userUnfroze, freezeSpan{patternIndex,lengthBeats,bpm,contentStamp}, freezeStale, freezeStreams, freezePatternStreams, freezeWatcher, freezeProcListener` — `Source/EngineRig.h:83-212`. The **engine is never destroyed** by a freeze (`:79-82`).
- Staleness axes: `markEngineContentChanged(tab)` (engine/player axis), `markPatternContentChanged(pattern)`, `markAllFreezesStale()` for tempo (`EngineRig.h:248-256`; tempo call site `StandaloneEditor.cpp:1202`). Rack/EQ/fader/send changes do **not** invalidate — they are downstream of the tap (doc `Freeze and Export.md`).
- Substitution: `Source/Engine/FrozenSourceRead.h` (doc-sourced; the header exists and is named there as the one substitution point).

---

## 6. Couplings that break if built-in engines are gone and tabs derive from buses

Ranked roughly by blast radius. Each is a concrete site.

### Hard blockers (compile-breaking references to removed classes)

| Coupling | file:line |
|---|---|
| `registerModelEngineAutomation` switches on `TabKind::{Layers,Bass,Drums,Clips,Vox,Inst}` and `dynamic_cast<BaySickVocalProcessor*>`; there is **no `Plugins` case at all** — hosted-instrument lanes come from a separate function. | `Source/Standalone/StandaloneEditor.cpp:15528-15640` (Vox cast :15543, :15617) |
| `registerSfizzEngineAutomation` — Guitars/Basses/RustyDrums only. | `StandaloneEditor.cpp:15642-15702` |
| `registerBaySickSolsticeModAutomation` — `BaySickSolsticeProcessor`, `ModSource`, `BaySickSolsticeModLength`. | `StandaloneEditor.cpp:15704-15791` |
| `registerPedalAutomation` — `BaySickPedalsProcessor::kNumSlots`, `EffectParamMap::PanelContext::Pedal`. | `StandaloneEditor.cpp:15793-15840` |
| `applyOfflineLaneValue` — pedal branch, sfizz sweep, Solstice mod branch, vox NAM/IR branch; `BuilderPage.cpp` includes `BaySickRustyDrumsProcessor.h`, `BaySickPedalsProcessor.h`, `BaySickSolsticeProcessor.h`, `BaySickVocalProcessor.h` **solely** for these. | `Source/Standalone/BuilderPage.cpp:5, 21, 22, 23`; branches at `:10383-10441` (vox/inst/pedal), `:10472-10481` (sfizz), `:10484-10528` (Solstice) |
| `readClipCtl(BaySickPlayerProcessor*)` + `ClipCtl` — the whole grid-clip shaping chain (volume/pan/filter/drive/reduct/treble/width/ADSR/tune/detune/reverse/sample-start/stretch/vibrato) reads a `BaySickPlayer` APVTS. | `Source/PluginProcessor.cpp:977-1058`, called `:1085` |
| `CompositeAudioInsertTask::mClipPlayer` caches a `BaySickPlayerProcessor*`. | `Source/Engine/Tasks/CompositeAudioInsertTask.h:82`, `.cpp:29, 156` |
| `beginOfflineRender::sweepNonRealtime` casts to `BaySickVocalProcessor` and iterates `mGuitarsEngine` / `mBassesEngine` / `mRustyDrumsEngine`. | `Source/PluginProcessor.cpp:7971-7990` |
| Freeze kit path: `renderKitFreezeFiles`, `getBaySickRustyDrums()`, 13-strip special case. | `BuilderPage.cpp:10029-10040`; `PluginProcessor.cpp:4676, 4745, 5138`; `EngineRig.cpp:276` |
| `Pattern` carries a fixed array per built-in family (`layerRoll, bassRoll, drumRoll, drumRolls, clipRoll, voxRoll, instRoll, pluginRoll, baySickRustyDrumsRoll`) — the roll table is family-indexed, not bus-indexed. | `Source/PatternManager.h:269-303` |
| `SchedulerRollSnapshot`/`PatternRollsSnapshot` mirror the same family arrays. | `Source/PatternManager.h:552-573` |
| `BlockContext` has one MIDI buffer array per family. | `Source/Engine/BlockContext.h:54-57` |
| Roll serialization tag names are per family (`LayerRoll` … `BaySickRustyDrumsRoll`). | `Source/PatternManager.cpp:1337-1397` |

### Tab-derivation couplings (tabs currently come from `mPages` / `EngineRig`, not from buses)

| Coupling | file:line |
|---|---|
| Piano-roll dropdown is built by walking `mPages` and `dynamic_cast`-ing to each page class; the Plugin case is one branch among seven. | `StandaloneEditor.cpp:2320-2376` |
| `PianoRollSelection` persists an `EngineKind` **name** with an exhaustive 11-way switch. | `StandaloneEditor.cpp:15008-15021`; load side reads `kind`/`index` |
| Live-MIDI routing switches on the EngineKind ordinal (1/2/3/4/7/8/9/10). | `PluginProcessor.cpp:3585-3594` |
| `serializeTabsInto` is a `dynamic_cast` ladder over eight page classes producing eight `type=` strings; `deserializeUIState` is the mirror ladder. | `StandaloneEditor.cpp:15060-15191`; `:18202-18400+` |
| Freeze save/load maps `type=` strings → `TabKind` in **two** places that must agree. | `StandaloneEditor.cpp:15228-15236` and `:18175-18183` |
| `unregisterAutomationForTab` hardcodes one lane-id prefix per `TabKind`. | `StandaloneEditor.cpp:17530-17554` |
| `renderTaskForTab` / `insertKindForTab` / `freezeFileFor` are per-`TabKind`. | `PluginProcessor.cpp:4372, 4405, 4428` |
| `onEnumerateRoutablePages` (audio-clip "Routes to:") walks `mPages` for Clip/Vox/Inst tab types only. | `StandaloneEditor.cpp:2893-2925`; consumed at `BuilderPage.cpp:5310-5392` |
| `PagePresetIO::PageKind` is the same eight-way enum. | `Source/Standalone/PagePresetIO.h:29` |
| Per-tab swing params are family-named (`swing_layer_<N>_mix`, `swing_plugin_<N>_mix`, `swing_rusty_mix`, …); Clips has none, Vox is excluded. | doc `Piano Roll.md` §Parameters; consumed `PluginProcessor.cpp:3350-3404` |
| PR-target bases are derived by summing **page caps in order** — changing any cap invalidates saved projects' pending note-offs. | `Source/BaySickConstants.h:26-39` |

### Sequencer-side couplings that survive intact (no change needed)

- `PatternManager` block/arrangement/marker/tempo/library model, `evalAutomationLaneAt`, the roll-snapshot RCU + `ScopedAudioShield` — no engine types (`Source/PatternManager.h` / `.cpp`).
- `PianoRollPage` / `PianoRollContainer` / `ControlLane` / Riff Machine / Event Editor — closure-based, zero engine references (`Source/Standalone/PianoRoll.*`, `PianoRollPage.*`, `EventEditor.*`).
- `AudioClipStreamer`, `PhaseVocoder`, the decoded-clip cache and the clip snapshot/retirement machinery (`Source/DSP/AudioClipStreamer.*`, `PhaseVocoder.*`, `PluginProcessor.h:889-1050`).
- `runOfflineLoop` itself, `renderToFile`, `measureRender`, the freeze tap + stale-tap guard (`BuilderPage.cpp:9278, 9597, 10157, 9858`).
- `ProjectFileResolver`, `MissingFileReport`, `ProjectManager`, `UndoActions.h`.
- `registerStaticAutomationHandlers` (main-APVTS lanes: every mixer strip param, both EQ banks, sends, `global_tempo`, the `_fader` alias) — `StandaloneEditor.cpp:17572-17645`.
- `registerPluginTabAutomation` + the `plugtab<N>_vst_` offline branch — already the shape the fork wants (`StandaloneEditor.cpp:15862-15902`; `BuilderPage.cpp:10368-10381`).
- Rack-slot automation by channel-prefix + slot UUID, including the `vst_` fork for hosted effects (`BuilderPage.cpp:10545-10585`).

### Two behaviours that would be *lost* rather than ported (inferred)

- **MIDI recording has no destination** once Layers/Bass/Drums are gone: `LastRollKind` has only those three values and record is hard-refused otherwise (`StandaloneEditor.h:1081`; `StandaloneEditor.cpp:1134-1148, 5868-5884`). A Lite fork needs a Plugins/bus branch in `updateActiveTabState` and in the commit switch at `:21112-21126`.
- **Grid audio-clip shaping** (`ClipCtl`) disappears with `BaySickPlayer`; clips would decode raw. That is already the documented behaviour when the engine is absent (`PluginProcessor.cpp:975-978`), so the clip still plays — it just loses volume/pan/filter/drive/vibrato/tune/reverse/stretch shaping.

---

# SECOND-ROUND READER 1: Settings stores — settings.xml, ui_prefs.xml, File Settings, Options menu, DrumTriggerVelo

# Settings stores — settings.xml, ui_prefs.xml, File Settings, Options menu, DrumTriggerVelocity

All paths resolve through `AppPaths::appRoot()` = `Documents\BaySickDAW\` (`Source/AppPaths.h:10-15`). `ProjectManager::getSettingsFile()` = `<appRoot>/settings.xml` (`Source/ProjectManager.cpp:18-29`).

## 1. `settings.xml` — full key inventory and who writes each

Root element tag is `BaySickDAWSettings` (`ProjectManager.cpp:720`). Every writer follows the same read-modify-write "sibling-preserving" idiom: parse existing root, remove only its own child, re-add, `writeTo`. There are **six** independent writers, not three.

### 1a. `ProjectManager` — top-level attributes + `<RecentProjects>`
`loadSettings` `ProjectManager.cpp:685-708`, `saveSettings` `:710-751`.

| Key | Kind | Line (save) | Fork verdict |
|---|---|---|---|
| `<RecentProjects><Project path=.../></RecentProjects>` (max 10, `:681`) | child | `:722-728` | KEEP |
| `shortcutCreated` | attr | `:729` | DIES — one-shot flag for `Sample Library.lnk` → `SampleLibrary::getCoreLibraryDir()` (`:451-462`) |
| `migratedFromRoaming` | attr | `:730` | KEEP-or-drop; the migration it guards moves `audio_settings.xml` (`:427-430`, keep) **and** `Presets/` for the five named built-in engines (`:432-443`, dies) |
| `skipGlobalLockPromptBank0` / `Bank1` | attr | `:731-732` | DIES — drum-kit bank lock prompt, only consumer `StandaloneEditor.cpp:8485, :8509` |
| `skipKitReplacePrompt` | attr | `:733` | DIES — kit replace prompt, only consumer `StandaloneEditor.cpp:9158, :9189` |
| `skipCoreContentPrompt` | attr | `:734` | DIES — Core Library opt-out, `ProjectManager.cpp:469-474, :496, :510` |
| `defaultTemplate` (removed when empty) | attr | `:735-738` | KEEP — consumed by `StandaloneEditor.cpp:11703-11713` (menu label), `:11913-11920` (id 111/530/531) |

Write failure raises one alert per session, gated by `mSettingsWarnShown` (`:742-750`). Constructor calls `loadSettings()` at `ProjectManager.cpp:84`; `runFirstLaunchHousekeeping()` is called from `StandaloneEditor.cpp:816` and unconditionally ends in `offerCoreContentDownload(false)` (`ProjectManager.cpp:466`).

### 1b. `StandaloneApp` — `<MultiCoreRendering on>` and `<MidiTriggerVelocity fixed>`
- `loadMultiCoreRenderingPref` `StandaloneApp.cpp:476-495` → `RenderEngine::gMultiThreadedEngineEnabled` (release store, `:493`); `save` `:497-520`. Declared `StandaloneApp.h:216-217`. Toggled from the Mixer hamburger: `StandaloneEditor.cpp:7738-7739`.
- `loadMidiTriggerVelocityPref` `:526-537` → `DrumTriggerVelocity::gUseFixed` (`:535`); `save` `:539-558`. Declared `StandaloneApp.h:218-220`.
- Both are loaded at startup **before** `mDeviceManager->initialise` — `StandaloneApp.cpp:803` and `:807` — so the first audio block sees the persisted values.

### 1c. `PatternColorPicker` — `<RecentPatternColors><Color argb>`
File-local `getSettingsFile()` duplicate at `PatternColorPicker.cpp:10`; `loadRecents` `:115-135`, `saveRecents` `:137-162` (`removeChildElement` + rebuild, `:149-150`), `pushRecent` `:164+`. KEEP (pattern colors are shell).

### 1d. `WorkspaceWindow` — `<WorkspaceWindows><W .../></WorkspaceWindows>`  *(missing from the prompt's list)*
Tags: `kRootTag = "WorkspaceWindows"`, `kWindowTag = "W"` (`WorkspaceWindow.cpp:18-19`). Attributes `key,w,h[,x,y][,rx,ry,rw,rh]` written at `:1345-1372`. Read paths: `loadSavedBounds` `:1249-1261`, restore-rect seed `:888-900`. Written **once at app exit** by `WorkspaceWindow::writeSessionToSettings()` (`:1310-1381`), called from `BaySickDAWStandaloneApp::shutdown` `StandaloneApp.cpp:1321` after `flushWindowBoundsNow()` `:1296-1298`. Position (`x`,`y`) is written only for keys in `placementKeys()` — seeded with exactly four, `"<TabType>:-1"` for Mixer / Builder / Effects / PianoRoll (`StandaloneEditor.cpp:2410-2417`). Note `:1316-1319` explicitly requires the root tag to match every other writer.

### 1e. `SharedUI` / `LufsReadoutBox` — `<MasterLufsMode mode>`  *(missing from the prompt's list)*
Read in the constructor `SharedUI.cpp:3080-3085`, written in `applyMode(..., persist=true)` `:3109-3128`. KEEP (transport/master meter, no engine dependency).

### 1f. `BaySickNAMIREditor` — `<RecentNAMFiles>` / `<RecentIRFiles>`
`BaySickNAMIREditor.cpp:1388-1426`, path `:1385`, documented `BaySickNAMIREditor.h:64`. DIES with BaySickNAMIR. Note a latent inconsistency worth not inheriting: when `settings.xml` does not yet exist this writer creates root `"Settings"` (`:1412`), not `"BaySickDAWSettings"` — a file it creates first is unreadable by every other reader (which all use `getChildByName` off the expected root).

### 1g. `<TransportDisplay showTime>` — `StandaloneEditor`  *(missing from the prompt's list)*
`loadTransportDisplayPref` `StandaloneEditor.cpp:12169-12177` (called `:1385`), `saveTransportDisplayPref` `:12179-12196`, wired to `mPosReadout->onDisplayModeChanged` `:1384`. Documented in `Plans & Specs/System Reference/Transport and Playback.md:204`. KEEP.

**Doc drift to note:** `Projects and Saving.md:198` lists settings.xml's contents but omits `MasterLufsMode`, `TransportDisplay`, `shortcutCreated`, `migratedFromRoaming`, `skipCoreContentPrompt`, and the NAM/IR recents.

## 2. Sibling per-machine files (same folder, not inside settings.xml)

| File | Owner | Evidence |
|---|---|---|
| `audio_settings.xml` | `BaySickDAWStandaloneApp::getAudioSettingsFile` (`StandaloneApp.h:198`), written `saveAudioSettings` `StandaloneApp.cpp:560+` with an empty-name/fallback guard `:567-589` | KEEP |
| `audio_settings_pending.xml` | written by the Audio Settings dialog `StandaloneEditor.cpp:490-493`; promoted by rename at startup `StandaloneApp.cpp:767-770`; suppresses the exit auto-save `:1277-1279` | KEEP |
| `master_output.xml` | `getMasterOutputFile` / `loadMasterOutputRouting` (`StandaloneApp.h:200-205`, `StandaloneApp.cpp:445-467`, called `:1127`) | KEEP (mixer routing) |
| `keymap.xml` | `getKeymapFile` `KeyBindings.cpp:864-870`, `saveMappings` `:872+` | KEEP |
| `plugins.xml` + `plugins_scan_crashes.txt` | `PluginManager::dataFile` / `deadMansPedalFile` `Hosting/PluginManager.cpp:19-32`; the comment `:10-13` explains *why* it is not inside settings.xml (whole-file rewrite churn) | KEEP |
| `ui_prefs.xml` | below | partially keep |

## 3. `ui_prefs.xml` — key inventory, and the duplicated opener

Opened via a **file-local, duplicated** `openUiPrefs()`:
- `StandaloneEditor.cpp:20333-20342` (anonymous namespace; comment `:20326-20332` states the duplication is deliberate)
- `BaySickVocal/BaySickPitchEditor.cpp:65-74` (identical `PropertiesFile::Options`, same path)

These are the only two (`grep ui_prefs` → `BaySickPitchEditor.cpp:73`, `StandaloneEditor.cpp:20341`, plus the explanatory comment `PluginManager.cpp:13`).

| Key | Type / default | Written | Read | Fork verdict |
|---|---|---|---|---|
| `fsWriteDry` / `fsWriteDryCleaned` / `fsWriteWet` / `fsWriteWetCleaned` | bool; Dry+Wet on | `:20495-20498` | `readFileTakeSettings` `:20540-20553`, consumed only in the Vox record-stop path `:20938-20960` (`isVoxCh`) | DIES with BaySickVocal |
| `fsDenoiseStrength` | int, `Denoise::Strong` | `:20499-20500` | `:20551` | DIES (de-noise is Vocal) |
| `fsAutoFreezeCpu` | int 0..101, default 80 (101 = Off, `:20385`) | `:20503` | auto-freeze poll `:19537`, `:19663` | KEEP |
| `fsCaptureRetain` | int, default 2 = project Reports | `:20504` | startup `:2029`, live `:20574` | KEEP (version capture) |
| `fsCaptureAudio` | bool, default false | `:20505` | startup `:2026`, live `:20573` | KEEP |
| `fsInstrumentFreeze` | bool, default false | `:20506` | read **live** in the player-menu gate `:7878-7881` (see the `:7870-7877` comment on why) | KEEP |
| `exSpecId` | int, default `LoudnessSpec::Id::Streaming14` | `:13942` | export dialog `:13684`, capture spec `:19646` | KEEP (export) |
| `exSpecCustom` | double, default -14.0 | `:13943` | `:19647` | KEEP |
| `pitchMultiResetNoPrompt` | bool | `BaySickPitchEditor.cpp:2821` | `:2793` | DIES with the Pitch Editor |
| `pitchWorldOfflineNoPrompt` | bool | `:2857` | `:2832` | DIES |

**Fork consequence (inferred):** with BaySickVocal deleted, the *only* remaining `openUiPrefs()` is the editor's, so the duplication problem disappears by itself — but the four `fsWrite*` checkboxes and `fsDenoiseStrength` are the majority of the File Settings dialog's UI and go with it.

## 4. The File Settings dialog

`FileSettingsComp` is a struct inside the same anonymous namespace, `StandaloneEditor.cpp:20346-20537`; hoisted out of `showFileSettingsDialog` specifically so the screenshot harness can photograph the same component (`:20344-20345`, factory `:20555-20558`, declared `ShotFactories.h:17`, used `ShotHarness.cpp:754`). Dialog launcher: `showFileSettingsDialog` `:20560-20584` (reached from Options id 502, `:12003-12005`).

Widgets, in layout order (`resized` `:20510-20536`):
1. Four take-type toggles (`:20399-20406`) with a ≥1 interlock (`onBoxToggled` `:20485-20491`) — **Vocal-only, dies**.
2. De-noise strength combo (`:20409-20414`) — **dies**.
3. `FreezeSlider` "Auto-freeze above" (`:20356-20364` snap-to-100, `:20424-20441`) + readout (`freezeText` `:20387-20391`) — keep.
4. "Keep captured takes" combo (`:20444-20451`) — keep.
5. "Also keep the audio of each take" (`:20457-20464`) — keep.
6. "Enable Instrument Level Freeze" (`:20471-20480`) — keep.
7. Explanatory `note` label, whose text is entirely about take types (`:20415-20417`) — **dies with the checkboxes**.

`save()` `:20492-20509` writes all nine keys on every change and then fires `onCaptureSettingsChanged`, which the dialog wires to push capture settings live and call `wireFreezeSlotForVisiblePage()` (`:20570-20579`). Fixed size `setSize(400, 416)` `:20482` — the shot harness hard-codes "self-sizes 400x416" (`ShotHarness.cpp:754`), so removing rows changes the documented screenshot geometry.

## 5. The Options menu

Built in `getMenuForIndex` case 4, `StandaloneEditor.cpp:11827-11860`; menu bar names `:11658-11661`.

| Item | Id | Handler | Fork verdict |
|---|---|---|---|
| General ▸ Set / Clear Default Template | 530 / 531 | `:11917-11920` | KEEP |
| File Settings... | 502 | `:12003-12005` | KEEP (shrunk) |
| Audio Settings... | 503 | `:12008-12020` (`AudioSettingsDialog`) | KEEP |
| Plugins... | 504 | `:12023-12025` | KEEP |
| **Get Sound Content...** | **505** | `:12031-12034` → `mProjectManager->offerCoreContentDownload(true)` | **DIES** with the Core Library fetcher |
| Undo History Size ▸ 100/250/500/1000 | 510-513 | `:11993-12000` | KEEP (not persisted — matches `Projects and Saving.md:206-208`) |
| "MIDI is Omni (all devices) - Read Only" | 520 | disabled informational row (`:11859`, no case) | KEEP |

Removing 505 leaves `offerCoreContentDownload` with only the `false` caller in `runFirstLaunchHousekeeping` (`ProjectManager.cpp:466`), i.e. both callers and the `skipCoreContentPrompt` key are removed together. Also note `ProjectManager.h:127-131` still carries a stale "NOT YET REACHABLE with userAsked == true" comment that id 505 already falsified.

## 6. `DrumTriggerVelocity` — the Audio Settings ↔ DrumTriggerMap weld (confirmed)

Declaration is inside the header of a deleted class: `namespace DrumTriggerVelocity { extern std::atomic<bool> gUseFixed; inline constexpr float kFixedVelocity = 0.8f; }` at `Source/MidiLearn/DrumTriggerMap.h:46-55`; definition `MidiLearn/DrumTriggerMap.cpp:3-5` (`gUseFixed { false }`). The header comment `:41-45` records the reason it is a namespaced global: the pref loads before the processor exists, so it cannot live on the processor.

All five consumers:

| Site | Role | Survives the fork? |
|---|---|---|
| `StandaloneEditor.cpp:158` | Audio Settings dialog "Trigger Velocity:" combo initial value | **YES — shell** |
| `StandaloneEditor.cpp:162-164` | combo `onChange` → store + `BaySickDAWStandaloneApp::saveMidiTriggerVelocityPref()` | **YES — shell** |
| `StandaloneApp.cpp:535` | `loadMidiTriggerVelocityPref` at startup (called `:807`) | **YES — shell** |
| `StandaloneApp.cpp:555` | `saveMidiTriggerVelocityPref` | **YES — shell** |
| `PluginProcessor.cpp:8577`, `:8608`, `:8636` | drum-trigger dispatch, `kitFocused` gate `:8566-8569`, `mDrumTriggers.getBindingRT` `:8581` | NO — drum kit |

Include chain that makes it compile today: `DrumTriggerMap.h` is included in exactly two places — `MidiLearn/DrumTriggerMap.cpp:1` and `PluginProcessor.h:22`. `StandaloneEditor.cpp` and `StandaloneApp.cpp` never include it directly; they reach `DrumTriggerVelocity` **transitively through `PluginProcessor.h`**. So deleting `MidiLearn/DrumTriggerMap.{h,cpp}` breaks:
- the Audio Settings dialog combo (`StandaloneEditor.cpp:149-167`), and
- the app-wide pref loader/saver pair (`StandaloneApp.cpp:526-558`) and its startup call `:807`,

unless the namespace is relocated. The three-line namespace has **no dependency on `DrumTriggerMap` the class** — it is a free atomic plus a constant. The MidiLearn folder otherwise holds `MidiLearnRegistry.{cpp,h}`, `MidiLearnUI.h`, `MidiMapView.h`, all kept.

The Lite-fork question this raises (facts only): the combo's *label and semantics* are kit-specific ("Trigger Velocity: From controller / Fixed", documented as "Trigger velocity source → `<MidiTriggerVelocity fixed>` in settings.xml" in `System Reference/MIDI Learn.md:119`), and its only audio-thread reader (`PluginProcessor.cpp:8577`) is the drum-trigger loop that dies. So after the fork the pref, its settings.xml key, its two `StandaloneApp` functions and its dialog row would persist a value nothing reads — unless a hosted-instrument equivalent is given to it. Whichever way that is resolved, the `DrumTriggerVelocity` namespace must be either relocated (out of `DrumTriggerMap.h`) or removed **together with** `StandaloneEditor.cpp:149-167`, `StandaloneApp.cpp:522-558`, `StandaloneApp.h:218-220`, and the `:807` call site.

## 7. Other coupling the map does not carry

- **`AudioSettingsDialog` is defined inside `StandaloneEditor.cpp`** (`:111-...`), not its own file; it holds the ASIO control-panel button (`:184-189`), MIDI input toggles (`rebuildMidiToggles`, `:144-147`), and the velocity combo. Splitting it out is the only way to keep the Audio Settings dialog while shrinking `StandaloneEditor.cpp`. (inferred)
- **`SafeXml::parse`** is the mandatory reader for every settings file (`ProjectManager.cpp:2` comment, used at `:689`, `:718`; `StandaloneApp.cpp:481`, `:531`, `:545`, `:822`; `PatternColorPicker.cpp:121`, `:145`; `WorkspaceWindow.cpp:888`, `:1249`, `:1315`; `SharedUI.cpp:3082`, `:3119`). KEEP.
- **`UserFileSave::showWriteFailure`** is the shared write-failure surface for keymap (`KeyBindings.cpp:878+`); `ProjectManager::saveSettings` uses its own inline alert instead (`:742-750`). Inconsistent but both shell.
- **Load ordering constraint the fork must preserve:** `loadMultiCoreRenderingPref()` / `loadMidiTriggerVelocityPref()` at `StandaloneApp.cpp:803/807` must stay before `mDeviceManager->initialise` (`:798-807` comment); `loadMasterOutputRouting()` at `:1127`; `WorkspaceWindow::writeSessionToSettings()` at `:1321` must stay after `clearContentComponent()` (`:1313`, reason at `:1316-1320`).

---

# SECOND-ROUND READER 2: Vendored JUCE patches — facts for the Lite fork

# Vendored JUCE patches — facts for the Lite fork

## 0. What the docs say (checked first)

The System Reference set is **effectively silent** on the vendored patches. `Plans & Specs/System Reference/INDEX.md` has no vendored-JUCE entry. Only three doc lines touch patched API, and none of them says "vendored":

- `Plans & Specs/System Reference/Engine Tabs (Layers, Bass, Drums).md:30` — "Each engine's APVTS also carries an `undoOwnerTag` of `rig:<kind>:<pageIndex>` so undo entries can find the engine again after it has been destroyed and rebuilt." (`undoOwnerTag` is not a stock JUCE member.)
- `Plans & Specs/System Reference/BaySickBass.md:369` and `BaySickSynth.md:404` — reference `replaceStateKeepingUndoHistory` as if it were JUCE API.
- `Plans & Specs/System Reference/Undo History.md:18-19` — "flush any parameter gesture that is still pending" — that is `AudioProcessorValueTreeState::flushAllLiveInstancesToValueTrees()`, a patched static. The doc never names it or flags it as non-stock.

`CLAUDE.md:105` is the only place the vendoring is stated: "vendored JUCE 8.0.x". **No patch inventory anywhere in `CLAUDE.md`** (grep for `juce/modules` in CLAUDE.md returns nothing). The historical trail exists only in `Plans & Specs/Implemented Work Log.md:127,131,139,149,155` ("Modified vendored JUCE: 7 files in juce/modules/") and in commit messages `bd67fdf8`, `2e2df50a`, `5c43cfa0`, `ade5a10b`.

`juce/` is a **plain vendored directory, not a git submodule** (`.gitmodules` absent; `git log -- juce/modules` shows 4 patch commits on top of the initial import). `CMakeLists.txt:20-28` does `add_subdirectory("${JUCE_DIR}")` with a comment "Vendored JUCE 8.0.12"; note the FATAL_ERROR text at `CMakeLists.txt:25` still says **"Download JUCE 7.0.12"** — a stale string that would mislead a fork re-vendoring by hand.

---

## 1. Inventory: **14 patched files**, not nine

The brief lists nine. Grep for the two marker strings (`BaySickDAW` / `BAYSICKDAW VENDORED CHANGE` / `QA-0a` in `juce/modules`) finds five more.

| # | File | Site | Class of change | Fork verdict |
|---|---|---|---|---|
| 1 | `juce/modules/juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.cpp` | `:39-135, :227-231, :274-279, :331-333, :398, :405, :446-453, :470-488, :558-628, :643-646` | **Behavioural — new API + new undo action** | KEEP verbatim |
| 2 | `juce/modules/juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.h` | `:406-426, :445-466, :468-478, :480-488` | **Behavioural — new public API** | KEEP verbatim |
| 3 | `juce/modules/juce_audio_processors/utilities/juce_ParameterAttachments.cpp` | `:71-84` | **Behavioural — gesture naming + boundary flush** | KEEP verbatim |
| 4 | `juce/modules/juce_gui_basics/native/juce_Windowing_windows.cpp` | `:3371-3385` | **Behavioural — double-undo fix** | KEEP verbatim |
| 5 | `juce/modules/juce_gui_basics/menus/juce_PopupMenu.cpp` | `:1492-1499` | **Behavioural — left-button-only trigger** | KEEP verbatim |
| 6 | `juce/modules/juce_events/interprocess/juce_InterprocessConnection.h` | `:135-156` (`setPipeMessageTimeout`) | **Behavioural — new API** | KEEP verbatim |
| 7 | `juce/modules/juce_events/interprocess/juce_ConnectedChildProcess.h` | `:229-247` (`setWorkerPipeTimeout` decl) | **Behavioural — new API** | KEEP verbatim |
| 8 | `juce/modules/juce_events/interprocess/juce_ConnectedChildProcess.cpp` | `:166-174` | **Behavioural — new API impl** | KEEP verbatim |
| 9 | `juce/modules/juce_core/files/juce_File.cpp` | `:176-187` (`jassertfalse` commented out) | Debug-assert suppression | KEEP |
| 10 | `juce/modules/juce_data_structures/values/juce_ValueTree.cpp` | `:777-783` (`jassert (object != nullptr)` commented out) | Debug-assert suppression | KEEP |
| 11 | `juce/modules/juce_audio_devices/native/juce_ASIO_windows.cpp` | `:1523-1530` (in/out-name `jassert` commented out) | Debug-assert suppression | KEEP |
| 12 | `juce/modules/juce_gui_basics/menus/juce_MenuBarModel.cpp` | `:78-85` (`jassert (listeners.contains …)` commented out) | Debug-assert suppression | KEEP (or drop, see §6) |
| 13 | `juce/modules/juce_audio_basics/utilities/juce_ADSR.h` | `:117-121` (`jassert (newSampleRate > 0.0)` commented out) | Debug-assert suppression, **built-in-engine-caused** | DROPPABLE |
| 14 | `juce/modules/juce_dsp/processors/juce_StateVariableTPTFilter.cpp` | `:73-78` (two `jassert`s commented out) | Debug-assert suppression, **built-in-engine-caused** | DROPPABLE |

Two of the fourteen (#13, #14) name `VibePlayerProcessor` in their comments — a class that **no longer exists in `Source/`** (grep across `Source` returns zero hits). They are stale even in the parent app.

---

## 2. Patch group A — undo correctness (files 1–4, plus 5)

### A1. Programmatic-write phase (`.h:445-466`, `.cpp:41`, `:227-231`, `:274-279`, `:331-333`)

`static thread_local bool programmaticWritePhase` + RAII `ScopedProgrammaticParamWrites`. The mechanism: `ParameterAdapter::parameterValueChanged` records the phase flag into `std::atomic<bool> pendingIsProgrammatic` on the **writer's** thread (`.cpp:277-278`), and `flushToTree` consumes it on the message thread, nulling the UndoManager for that flush (`.cpp:230-231`). Rationale, verbatim from the header (`.h:446-455`): parameter writes notify synchronously but reach the ValueTree later on the timer flush, so a caller-scoped "suppress undo" cannot work.

**Consumers: 29 call sites in `Source/`.** Split by fork relevance:

- **Shell (fork keeps):** `Source/MidiLearn/MidiLearnRegistry.cpp:193` (hardware CC streams excluded from undo), `Source/Standalone/MixerPage.cpp:3645` (ctor-time model→APVTS mirror), `Source/Standalone/BuilderPage.cpp:9291`, `Source/Standalone/FxRackPresetIO.cpp:152`, `Source/Standalone/PagePresetIO.cpp:242,396`, `Source/Standalone/StandaloneEditor.cpp:1237,3940,12924,18677`, `Source/PluginProcessor.cpp:3498,7220,7734,7764,9228,9382`.
- **Built-in engines (fork deletes):** `BaySickBassesProcessor.cpp:664`, `BaySickGuitarsProcessor.cpp:675`, `BaySickNAMIRProcessor.cpp:933,1076`, `BaySickRustyDrumsProcessor.cpp:708`, `BaySickVocalProcessor.cpp:1876,1950,2198`, `Source/Inst/InstPage.cpp:1131`, `Source/Standalone/BaySickRustyDrumsPage.cpp:526`, `Source/PluginProcessor.cpp:9843,9864` (Rusty bus/strip resets).

Net: **the phase mechanism is shell-load-bearing independently of the engines.** MIDI Learn (a stated keep) is the clearest case — without the patch every incoming CC contaminates the open transaction.

### A2. `undoOwnerTag` + `findByUndoOwnerTag` + `ApvtsParamValueUndoAction` (`.h:468-478`, `.cpp:43-135`, `:398`, `:405`)

Stock JUCE's flush performs a tree-bound `SetPropertyAction`. The patch replaces it with `ApvtsParamValueUndoAction` (`.cpp:81-135`), which stores denormalised values plus the owner **tag** and re-resolves the live APVTS at apply time (`.cpp:96-110`). Liveness comes from a static `liveApvtsInstances()` array maintained in the APVTS ctor/dtor (`.cpp:47-51`, `:398`, `:405`).

**This is the API `PluginProcessor.cpp:563` uses:** `apvts.undoOwnerTag = "main";` — with the comment at `:560-562` "tagged so its undo entries use the same tag-resolving path as every engine's".

Tag stamping sites: `Source/PluginProcessor.cpp:563` (`"main"` — **shell**), `Source/EngineRig.cpp:545` (`rigTag = "rig:" + kind + …`), `:562-565`, `:577-578`, `:638`, `:641`, plus `BaySickBassesProcessor.cpp:29`, `BaySickGuitarsProcessor.cpp:30`, `BaySickRustyDrumsProcessor.cpp:27`. `Source/EngineRig.h:220` and `Source/PluginProcessor.h:285` document the contract in comments.

**Fork impact:** the *tag-resolution machinery* is only strictly needed when an APVTS can be destroyed and re-created mid-session. In the fork that still happens — hosted VST3 instances added/removed by tab add/delete/undo — but hosted plugins do not own an `AudioProcessorValueTreeState` (their params are the plugin's own). If the fork's per-insert mixer params stay on the **one main APVTS** (which never dies), the resurrection case narrows to zero for parameters. However `.cpp:99-100` / `:582-583` also use `liveApvtsInstances()` as a **dead-owner guard** whose no-op path must return `true` — the comment at `.cpp:577-580` is explicit: "a false return makes UndoManager wipe the entire history." Dropping the patch reinstates that hazard.

### A3. `replaceStateKeepingUndoHistory` (`.h:406-426`, `.cpp:558-628`)

`replaceState` minus `clearUndoHistory()` (compare stock at `.cpp:548-556`, which calls `undoManager->clearUndoHistory()` at `:555`). With every APVTS sharing the ONE global UndoManager, a mid-session preset load through stock `replaceState` **wipes the whole app history** (`.h:407-409`). With a non-empty `undoTransactionName` the swap itself becomes one undoable `StateSwapAction` (`.cpp:566-607`, "Jeff ruling 3a", `.h:418-424`).

**29 call sites, and they are overwhelmingly built-in-engine preset/state loads:** `BaySickBassEditor.cpp:1106`, `BaySickBassProcessor.cpp:577`, `BaySickBassesProcessor.cpp:740`, `BaySickGuitarsProcessor.cpp:751`, `BaySickNAMIRProcessor.cpp:1251`, `BaySickPedalsProcessor.cpp:414`, `BaySickPlayerEditor.cpp:888`, `BaySickPlayerProcessor.cpp:468`, `BaySickRustyDrumsProcessor.cpp:1077`, `BaySickSolsticeEditor.cpp:1405,1425`, `BaySickSolsticeProcessor.cpp:1122`, `BaySickSynthEditor.cpp:1217`, `BaySickSynthProcessor.cpp:595`, `BaySickVocalProcessor.cpp:2193`, `Source/Inst/InstPage.cpp:1132`, `Source/Standalone/BassPage.cpp:251-253`, `DrumPage.cpp:758,1057`, `LayersPage.cpp:254-256`.

**Only three survive the engine cull:** `Source/Standalone/PagePresetIO.cpp:265` (`slot.engineApvts->replaceStateKeepingUndoHistory`) and `Source/Standalone/StandaloneEditor.cpp:12925,12996,18678,18851` — and all of those are reached *through* engine slots. **Inferred:** in a shell-only fork with no engine APVTS at all, `replaceStateKeepingUndoHistory` may end up with **zero live callers**; the only remaining `replaceState` user would be the main-APVTS project/template load, which *wants* the clearing variant (`.h:410-411`). It is still cheap to keep, and dropping it means editing the vendored file (a merge liability) rather than just not calling it.

### A4. Lazy APVTS registration (`.cpp:446-453`, `:470-488`; plus `juce_ValueTree.cpp:777-783`)

Two halves:
- `.cpp:453` — the `jassert (! state.isValid())` in `createAndAddParameter` is **commented out** because BaySickDAW registers params after the state tree is built.
- `.cpp:470-488` — the *functional* half: `addParameterAdapter` now binds a child tree immediately when `state.isValid()`, with the value property **pre-set to the param's live value**. The comment names the two failure modes precisely: without it a late adapter "kept an INVALID tree forever: its flush could never transact, the begun `param:<id>` gesture stayed empty, and the edit never reached the undo history"; and an id-only node "would make setNewState read the missing value as the DEFAULT and reset the live param (the QA-Ef save-path failure mode)".

**This is the single most fork-critical patch, because the FL-style mixer is built entirely on lazy registration.** Evidence: `Source/PluginProcessor.cpp:8913` `addParamsForMixerStrip(prefix, kind, defaultSendTo)` — §-header at `:8912` "5F-4a: Mixer-strip lazy APVTS registration"; raw `apvts.createAndAddParameter` at `:8925, :8930, :8989`. Also `ensureEqParamsForId` (`:7801`), `ensureDirectStripInfra` (`:8251`), `ensureMixerBusAndMasterParams` (`:9065`), `ensureSwingParams` (`:9110`), `addLiveInputParams` (`:9928-9973`, registering `_inputChannelIdx` / `_listen` / `_inputChannelStereo` — the *live audio input as a per-insert property* the fork wants), and the per-kind insert factories `ensureAudioInsert` (`:9240`), `ensureAuxInsert` (`:9265`), `ensureVoxInsert` (`:9313`), `ensureInstInsert` (`:9325`), `ensureRustyInsert` (`:9341`).

A fork on stock JUCE would still *work in Release* (the assert is Debug-only) but **would lose the `:470-488` binding**, i.e. mixer-strip and routing parameter edits would silently never enter the undo history. There is also a CMake-side companion: `CMakeLists.txt:105` `JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING=1`, added because "our long / similar-prefix mixer-strip paramIDs trip the AAX trim check" (`CMakeLists.txt:98-104`).

### A5. Gesture naming `param:<id>` + boundary flush (`juce_ParameterAttachments.cpp:71-84`; `.h:480-488`, `.cpp:62-67`)

```cpp
// juce/modules/juce_audio_processors/utilities/juce_ParameterAttachments.cpp:80-84
if (undoManager != nullptr)
{
    AudioProcessorValueTreeState::flushAllLiveInstancesToValueTrees();
    undoManager->beginNewTransaction ("param:" + parameter.paramID);
}
```

Stock JUCE begins the gesture transaction **unnamed**. Two app-side systems depend on the naming and on the flush:

- `Source/Standalone/UndoBracket.h:17-34` — both `beginParamUndoGesture` overloads call `flushAllLiveInstancesToValueTrees()` then `beginNewTransaction ("param:" + paramId)`; header comment at `:7-9` says it "is the same marker the vendored ParameterAttachment gesture naming uses".
- `Source/Standalone/StandaloneEditor.cpp:12418-12435` `historyDisplayFor` — `if (transactionName.startsWith ("param:"))` at `:12422`, comment at `:12420-12421` "an attachment gesture named by the **vendored** ParameterAttachment::beginGesture".
- `flushAllLiveInstancesToValueTrees()` also at `Source/Standalone/StandaloneEditor.cpp:12297` (`doUndoAction`), `:12322` (`globalUndo`), `:12340` (`globalRedo`), and `Source/Clips/ClipsPage.cpp:119`.

Without the flush: "two quick gestures could land in ONE transaction" (`.h:481-486`) — one Ctrl+Z reverts both knobs.

**Engine coupling to fix in the fork:** `StandaloneEditor::ownerKeyForParamId` (`:12437-12483`) resolves the owner label and is **hard-wired to the built-in engines** — id-prefix branches `"brd_"` → `"rusty"` (`:12440`), `"bgg_"` → BaySickGuitars (`:12441-12443`), `"bbb_"` → BaySickBasses (`:12444-12446`); then a `kKinds[]` table of `TabKind::Layers/Bass/Drums/Clips/Vox/Inst/Plugins` (`:12453-12458`) walked against `EngineRig::apvtsOf`, `t->namIr` (`:12468`), `t->pedals` (`:12472`), and a `BaySickVocalProcessor` `dynamic_cast` (`:12477`). **This is the direct intersection with the fork's bus-derived tab model** — the function must be rewritten to resolve a param id to a bus/insert key instead of to an engine tab. The patched JUCE file itself needs no change; only this app-side resolver does.

### A6. Double-undo fix (`juce_Windowing_windows.cpp:3371-3385`)

`forwardMessageToParent` now guards `if (getOwnerOfWindow (parentH) == nullptr)` before `PostMessage`. Reason (`:3372-3381`): the forward exists for plugin-in-a-host; when the parent is a JUCE peer **in this process** — "the contained WorkspaceWindows" — the same press dispatches twice ("undo_diag tracer: two src=cmd invocations ~12 ms apart"), and because Ctrl+Alt arrives as `WM_SYSKEYDOWN`/`WM_SYSCHAR` which this switch never forwards, "redo fired once while undo fired twice."

**This is pure shell.** It is caused by the contained-window architecture (`Source/Standalone/WorkspaceWindow.h`, referenced from `CMakeLists.txt:119-121` re: native child peers for hosted VST3 editors) and has zero engine coupling. The fork keeps contained windows and hosted VST3 editors, so it keeps this bug and needs this patch verbatim.

---

## 3. Patch group B — out-of-process plugin helper (files 6, 7, 8)

`InterprocessConnection::setPipeMessageTimeout` (`juce_InterprocessConnection.h:156`, one-liner setter, doc block `:135-155`) and `ChildProcessCoordinator::setWorkerPipeTimeout` (`juce_ConnectedChildProcess.h:247`, impl `juce_ConnectedChildProcess.cpp:170-174`). Rationale: `launchWorkerProcess` takes one `timeoutMs` serving two incompatible jobs — a generous **startup** budget vs. a short **per-write** budget; a wedged peer otherwise blocks the message thread for the full startup budget. The header explicitly says "Not upstream API. Kept as a named method … so a merge conflict here is loud rather than silent" (`juce_InterprocessConnection.h:151-154`).

**Sole consumer:** `Source/Hosting/SandboxedPluginClient.cpp:134` `setWorkerPipeTimeout (kConnectedWriteTimeoutMs);` with rationale at `:60-70` and `:130-133`.

**This is squarely inside the fork's keep list** (out-of-process VST3 helpers). Without it the fork gets a 15-second UI freeze on a wedged plugin bridge. Tagged security MEDIUM-7 (QA-Cleanup 2026-08-11).

---

## 4. Patch group C — Debug-assert suppressions with shell causes (files 9, 10, 11, 12)

| File:line | What is suppressed | Shell cause | Fork |
|---|---|---|---|
| `juce_core/files/juce_File.cpp:185` | `jassertfalse` on a relative path in the `File` ctor | "BaySickDAW stores RELATIVE `audioFilePath` on ArrangementBlocks (Samples/foo.wav etc) for project portability. `BuilderPage::ArrangementGrid::drawAudioClip`'s `existsAsFile` probe hits this path during paint" (`:176-184`). Names the real fix as resolving through `mProcessor.resolveProjectFile`, "Deferred to a dedicated batch." | **Builder + audio clips are keeps** → keep the patch, or land the deferred real fix. |
| `juce_data_structures/values/juce_ValueTree.cpp:783` | `jassert (object != nullptr)` in `setPropertyExcludingListener` | "BaySickDAW's lazy APVTS registration creates params whose ValueTree object isn't bound at flush time" (`:777-780`). The empty-name assert at `:776` is deliberately left active. | Keep (lazy registration is a keep). |
| `juce_audio_devices/native/juce_ASIO_windows.cpp:1530` | `jassert (inputDeviceName == outputDeviceName …)` in `createDevice` | "BaySickDAW's saved audio_settings.xml routinely has only an output device name … `VibesynthStandaloneApp::initialise` has a safety net that detects this, copies output→input, strips channel masks, and restarts the device" (`:1523-1529`). The safety net is live at **`Source/Standalone/StandaloneApp.cpp:929-930`** (`if (setup.inputDeviceName.isEmpty() && setup.outputDeviceName.isNotEmpty()) setup.inputDeviceName = setup.outputDeviceName;`). Note the `hasScanned` assert at `:1531` is intentionally left active. | Keep (ASIO + audio device are keeps). Related non-module ASIO config: `CMakeLists.txt:134-148` (`JUCE_ASIO=1`, `JUCE_ASIO_DEBUGGING=1`, SDK auto-detect at `libs/asiosdk/common/iasiodrv.h`). |
| `juce_gui_basics/menus/juce_MenuBarModel.cpp:85` | `jassert (listeners.contains (listenerToRemove))` | "a shared MenuBarModel can outlive (or pre-deceast) its MenuBarComponents during `closeAllDynamicTabs` cascades … Debug paused once per destroyed PianoRollPage" (`:78-84`). | The **root-cause fix has since shipped** — `Plans & Specs/Implemented Work Log.md:543` records declaration-order swaps in `Source/Standalone/PianoRoll.h:645-646`, `BuilderPage.h:750-751`, `DrumKitGrid.h:494-495` plus explicit destructors. `DrumKitGrid` is engine-side; PianoRoll and Builder are keeps. **Inferred:** this suppression is now redundant for the keep surfaces, but the fork has no evidence it is safe to drop. |

---

## 5. Patch group D — engine-only suppressions (files 13, 14)

`juce_ADSR.h:121` and `juce_StateVariableTPTFilter.cpp:73-78` suppress `sampleRate > 0` / `numChannels > 0` asserts caused by synth voices constructed before `prepareToPlay`. `Plans & Specs/Implemented Work Log.md:131` records the **source-side fix already landed** in all four engine constructors (`setCurrentPlaybackSampleRate(44100)` before the `addVoice` loop) and that these were "kept defensive belts in vendored JUCE in case a future engine misses this."

The named cause `VibePlayerProcessor` no longer exists in `Source/`. A fork with **no built-in synths** has no `juce::Synthesiser`/`juce::ADSR`/`juce::dsp::StateVariableTPTFilter` voice construction at all — hosted VST3s use none of these. **These two are the only patches the fork can drop cleanly**, reducing the delta from 14 files to 12.

---

## 6. Fork-relevant summary of coupling

**Depends on built-in engines (goes away or shrinks):** the 12 engine-side `ScopedProgrammaticParamWrites` sites; ~26 of the 29 `replaceStateKeepingUndoHistory` call sites; the `rigTag` stamping in `EngineRig.cpp:545-641`; `juce_ADSR.h` / `juce_StateVariableTPTFilter.cpp`.

**Depends on the tab model (must be rewritten app-side, not in JUCE):** `StandaloneEditor::ownerKeyForParamId` (`Source/Standalone/StandaloneEditor.cpp:12437-12483`) — engine id prefixes + `TabKind` table + `namIr`/`pedals`/`BaySickVocalProcessor` probes. Under a bus-derived tab model this becomes a bus/insert lookup. The `param:<id>` marker itself (`historyDisplayFor:12422`) is model-agnostic and survives.

**Depends on the mixer (hard keep):** lazy APVTS registration (`juce_AudioProcessorValueTreeState.cpp:446-453` + `:470-488`) plus `juce_ValueTree.cpp:783`. The FL-style insert bank, per-insert routing/level/sidechain and per-insert live audio input are all built on `addParamsForMixerStrip` (`Source/PluginProcessor.cpp:8913`), `ensureMixerBusAndMasterParams` (`:9065`), `addLiveInputParams` (`:9928`) and the `ensure*Insert` family (`:9240-9341`), all of which call `apvts.createAndAddParameter` after the state tree exists.

**Depends on nothing being culled (verbatim keeps):** `juce_Windowing_windows.cpp:3371` (contained windows), `juce_PopupMenu.cpp:1492` (all menus), `juce_File.cpp:176` (Builder audio clips), `juce_ASIO_windows.cpp:1523` + `StandaloneApp.cpp:929`, the three interprocess files (out-of-process VST3 helper), `programmaticWritePhase` (MIDI Learn at `MidiLearnRegistry.cpp:193`, mixer sync at `MixerPage.cpp:3645`, automation replay, project load), `flushAllLiveInstancesToValueTrees` + `param:<id>` naming (undo history window).

**Also non-module but part of the same "don't pull stock" surface:** `CMakeLists.txt:85` `JUCE_USE_MP3AUDIOFORMAT=0`, `:93` `JUCE_USE_WINDOWS_MEDIA_FORMAT=0`, `:105` `JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING=1`, `:128` `JUCE_PLUGINHOST_VST3=1` (with the explicit "JUCE_PLUGINHOST_VST is OFF deliberately" note at `:130-133`). None of these are module edits, but all four are required for the fork's keep list to build and behave the same.

---

# SECOND-ROUND READER 3: Transport Model — facts with evidence

# Transport Model — facts with evidence

Docs read: `Plans & Specs/System Reference/Transport and Playback.md` (whole file). It documents the *behavior* accurately but names only 3 classes (`GlobalTransportBar`, `StandalonePlayHead`, `TransportPositionReadout`); the state actually lives in 6 places. Everything below confirmed in code.

---

## 1. What the transport model is (5 distinct pieces, not 1)

| Piece | Location | Role |
|---|---|---|
| `StandalonePlayHead` | `C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneApp.h:9-129`, impl `StandaloneApp.cpp:171-417` | The clock. Implements `juce::AudioPlayHead`. |
| `TempoMap::` globals | `C:/Users/jeffm/Documents/BaySickDAW/Source/TempoMapRead.h:30-206` | App-wide sample→beat tempo timeline (seqlock). |
| `TsMap::` globals | `C:/Users/jeffm/Documents/BaySickDAW/Source/TsMapRead.h:23-141` | App-wide beat→bar/time-signature timeline (seqlock). |
| `PlayHeadAdvancer` | `C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneApp.cpp:60-154` | The real `AudioIODeviceCallback`; also does physical master-output channel routing. |
| Transport state + metronome DSP on the processor | `Source/PluginProcessor.h:416-437`, `:1809-1819`, `:1887-1923`; DSP at `Source/PluginProcessor.cpp:3868-4260` | Song mode, loop bounds, song end, seek/loop flags, metronome + count-in. |

`GlobalTransportBar` / `MetroPanel` / `TransportPositionReadout` are **views only** — the bar's own comment and the 10 Hz timer at `Source/Standalone/GlobalTransportBar.cpp:691-733` push loop length + TS *down* into the playhead and sync button visuals *up* from processor atomics.

---

## 2. `StandalonePlayHead` — the clock

**Source of truth is an int64 absolute sample counter**, `std::atomic<int64_t> mSamplePos` (`StandaloneApp.h:95`). Beats are derived, never accumulated (`StandaloneApp.h:88-116`).

- `advanceBlock` — `StandaloneApp.cpp:298-340`. Sole audio-thread writer. Exact integer advance (`:304`), loop wrap by integer modulo with **overshoot preserved** (`:329-337`), sets `mLoopWrapped` on wrap (`:336`).
- `deriveBeat` — `StandaloneApp.cpp:171-178`. Goes through `TempoMap::beatAtSample`; falls back to linear base-BPM math before the first publish (`:175-177`).
- `rebuildTimeline` — `StandaloneApp.cpp:194-296`. Message thread only, the timeline's **single writer**. Two modes: truncate-and-append while playing (`:218-257`, history never re-maps) vs pure rebuild from origin (`:258-274`). Stopped ⇒ relocate sample clock to preserve the musical bar (`:278-283`).
- `setBPM` = **base** tempo edit, rebuilds pure from origin even mid-play (`:357-367`).
- `setLiveTempo` = automation override, appends one coalesced tail segment (`:368-381`; pivot coalescing at `:241-247`, `mLastLivePivot` at `StandaloneApp.h:123`).
- `setTempoMarkers` — `:382-388`. `seekTo` — `:389-401` (sets `mSeekDiscontinuity` on backward seek, `:400`).
- `getPosition()` — `:403-417`. Publishes bpm/ppq/**timeInSamples**/isPlaying/isRecording/timeSignature. This is what every hosted VST3 sees.
- Loop state setters: `setLoopBeats` `StandaloneApp.h:70`, `setLoopStart` `:74`, `setTimeSignature` `:79-83`.
- Discontinuity flags exposed as raw pointers: `getSeekDiscontinuityFlag` `StandaloneApp.h:60`, `getLoopWrappedFlag` `:66`.

**Zero references to any built-in engine.** `grep` for BaySickSolstice/Synth/Bass/Player/Vocal/NAMIR/Pedals/Guitars/RustyDrums/SlideSampler across `StandaloneApp.cpp`, `StandaloneApp.h`, `TempoMapRead.h`, `TsMapRead.h` returns only two comment mentions of `BaySickNAMIREditor` in unrelated settings-file notes (`StandaloneApp.cpp:504`, `StandaloneApp.h:211`).

---

## 3. The two seqlock timelines (namespace-global, app-wide)

**`TempoMap`** (`TempoMapRead.h`): `kMaxSegs = 512` (`:32`); atomics `gSeq/gCount/gSampleRate/gSegSample/gSegBeat/gSegBpm` (`:34-39`); `publish` (`:45-62`) is message-thread-only two-fence seqlock; readers are **audio thread + render workers + message thread** (`:18-21`), bounded to `kMaxSeqRetries = 4` with a `thread_local` last-good fallback (`:73-84`). API: `beatAtSample` `:125`, `sampleAtBeat` `:142`, `bpmAtSample` `:160`, `nextBoundaryAfter` `:188` (used by the metronome to split blocks).

Reason it is a namespace global rather than a member is documented at `TempoMapRead.h:13-16`: `PluginProcessor.h` cannot include `StandaloneApp.h` because it is included *by* it (`StandaloneApp.h:3`).

**`TsMap`** (`TsMapRead.h`): `kMaxSegs = 256` (`:24`); same protocol but the retry loops are **unbounded** (`for(;;)` at `:75` and `:126`) — unlike TempoMap. Writer is `PatternManager::publishTimeSigMap` (`Source/PatternManager.cpp:904-931`). `barBeatAt` `:96-118` returns `{bar, beatInBar, barStartBeat, bpb, num, den}`, falling back to 4/4 when inactive (`:99-106`).

Usage counts (`grep TempoMap::|TsMap::` over `Source/`): `PluginProcessor.cpp` 51, `StandaloneApp.cpp` 27, `StandaloneEditor.cpp` 11, **`BaySickVocal/BaySickPitchEditor.cpp` 11**, `PatternManager.cpp` 5, `BuilderPage.cpp` 2, `GlobalTransportBar.cpp` 2, `BaySickVocalProcessor.cpp` 2. The 13 BaySickVocal hits are *consumers* of the timeline (`BaySickPitchEditor.cpp:167,176,1941,2143,2227`), so they disappear with that engine — no reverse dependency.

---

## 4. Timeline writers (who calls `rebuildTimeline`)

- `StandaloneEditor::pushTempoMarkersToPlayHead` — `Source/Standalone/StandaloneEditor.cpp:12153-12167`. **Markers are song-domain only**: pattern mode pushes an empty set (`:12156-12165`).
- Re-pushed on every song/pattern switch — `StandaloneEditor.cpp:1247-1251`.
- Project load — `StandaloneEditor.cpp:19188-19203` (markers then `ph.setBPM(pm->getGlobalTempo())`).
- BPM field commit — `StandaloneEditor.cpp:1190-1198` (`setBPM` + `mPM->setGlobalTempo` + `markDirty`).
- `global_tempo` automation applicator (30 Hz) — `StandaloneEditor.cpp:17629-17645`, calls `mPlayHead.setLiveTempo`, explicitly **not** the base tempo.
- Device sample-rate watch (5 Hz) — `StandaloneEditor.cpp:20600-20612`: segment sample positions are baked at the build rate, so an ASIO panel rate change behind the app's back republishes. Rationale at `StandaloneApp.cpp:200-204`.

Tempo/TS storage is in `PatternManager`: `mGlobalTempo` `Source/PatternManager.h:990`, `mTempoChanges` `:988`, accessors `:765-767` / `:958-959`; `TempoChange` struct `:336-340`; `TimeSigChange` `:317-329` (has `uid` + `linkedPattern` for pattern-follower ownership); `TimeSignature` `:474-478`; `getEffectiveTimeSigAtBar` `PatternManager.cpp:711-720`; `beatToBarAndBeatInBar` `:722-772`.

---

## 5. `PlayHeadAdvancer` — device callback + master output routing

`Source/Standalone/StandaloneApp.cpp:60-154`. It wraps `juce::AudioProcessorPlayer` and does three jobs in one class:

1. Zeroes every device output channel each block (`:96-98`).
2. **Physical master-output routing** — reads `MasterOutputRouting::gFirstOutputChannel` / `gMasterIsMono` (`:100-101`, declared `StandaloneApp.h:134-138`, defined `StandaloneApp.cpp:157-161`), clamps for the live device's channel count (`:109-112`), then either sums L+R into one channel via a pre-sized scratch buffer (`:114-129`) or maps L/R onto the chosen pair (`:130-141`). Persisted per-machine in `master_output.xml` (`StandaloneApp.h:202-205`).
3. Advances the playhead **only when not in an offline render** (`:150-152`) — otherwise a 40 s freeze moved the transport 40 s.

Wiring at startup, `StandaloneApp.cpp:756-760`: processor and playhead are created together, then `setPlayHead`, `setSeekDiscontinuityFlag`, `setLoopWrappedFlag`. Ownership is `BaySickDAWStandaloneApp` (`StandaloneApp.h:170-174`), so the playhead outlives processor and editor.

The same three-call wiring exists headless in the shot harness with **no engines and no editor** — `Source/Standalone/ShotHarness.cpp:2078-2090`. That is direct evidence the transport model is separable from the tab/engine layer.

---

## 6. Metronome + count-in DSP

State: `struct MetroDSP mMetro` — `Source/PluginProcessor.h:1888-1923`. Atomics `enabled/volume/soundType` (`:1891-1893`), count-in `countInActive/countInBpm/countInNum/countInDen` (`:1895-1901`), audio-thread-only click state (`:1902-1913`), PDC deferral `countInDelaySamp` + `transportWasPlaying` (`:1915-1922`).

DSP: `applyPostMixRecordAndMetro` — declared `PluginProcessor.h:1512-1515`, defined `Source/PluginProcessor.cpp:3868`, called once per block **after** `dispatchBlock` at `PluginProcessor.cpp:3840`.

- Whole function is a no-op under `isNonRealtime()` (`:3884-3885`) — click never reaches export/freeze.
- Ordering invariant D-5: MIDI recorder (`:3890-3911`) → master recorder pre-metronome (`:3939-3942`) → capture recorder (`:3949-3952`) → metronome (`:3960+`).
- MIDI recorder clock uses the **TempoMap block-average** beats-per-sample so a tempo-boundary block cannot drift the take (`:3904-3909`).
- Clicks deferred by `mVibeGraph.totalLatencySamples` PDC (`:3969-3970`).
- Count-in loop `:4045-4089` — runs on its own phase clock, independent of the transport (the transport is *frozen* during count-in because `advanceBlock` gates on `mPlaying`, `StandaloneApp.cpp:301`); clicks in denominator units with accent every `ciNum` (`:4055`, `:4078`).
- Transport-locked metro `:4091-4226` — reads `getPlayHead()->getPosition()` (`:4095`), splits the block at `TempoMap::nextBoundaryAfter` (`:4138-4152`) and, in song mode, again at `TsMap::nextBoundaryAfterBeat` (`:4160-4201`); pattern mode uses `mPatternManager->currentPattern().tsNum/tsDen` (`:4114-4119`, `:4202-4210`).

Count-in *scheduling* is UI-side: `StandaloneEditor::startPlayback` `Source/Standalone/StandaloneEditor.cpp:7933-7969` picks the signature (song = `TsMap::barBeatAt`, pattern = current pattern TS, `:7944-7953`), computes a one-bar `delayMs` (`:7954-7955`), sets the atomics and starts `mCountInTimer` (`:7957-7962`). The timer is a nested struct in `Source/Standalone/StandaloneEditor.h:1161-1172` — clears `countInActive` then calls `mPlayHead.start()`.

---

## 7. Song / pattern + loop state (split across processor and editor)

Processor atomics (`Source/PluginProcessor.h`):
- `mSongEndBeats` `:416`, `mRequestStop` `:417`, `mSongLoopMode` `:422` (default true)
- `mLoopStartBeats` `:427`, `mSeekDiscontinuity*` `:431-432`, `mLoopWrapped*` `:436-437`
- `mCachedPatternLoopBeats` `:1809`
- `mSongMode` `:1817`, `setSongMode` `:1818` — implemented at `Source/PluginProcessor.cpp:9196-9234`; **song entry/exit snapshots and restores every automation-targeted param's value**.
- `mPlayStartEdges` / `mLoopWrapEdges` `:464-465`.

Loop resolution is one editor lambda: `onGetLoopBeats` — `Source/Standalone/StandaloneEditor.cpp:1280-1368`. Time-selection wins in both modes (song = Builder ruler `:1286-1298`, pattern = active piano roll `:1302-1317`); otherwise song end from `mPM->getSongEndBeats()` (`:1339`) or `getEffectivePatternLoopBeats()` (`:1364`). It writes `mLoopStartBeats`, `mCachedPatternLoopBeats`, `mSongEndBeats`, `mSongLoopMode` and returns the wrap beat, which `GlobalTransportBar::timerCallback` feeds to `setLoopBeats` (`GlobalTransportBar.cpp:694-695`).

Song-end auto-stop: audio thread raises `mRequestStop` at `Source/PluginProcessor.cpp:3296-3301`; the editor consumes it at `StandaloneEditor.cpp:3918-3925` and runs `stopTransportAndFinalizeRecording` (`:9581-9603`) → `stopPlayback` (`:9605-9635`, which cancels the count-in timer, stops, seeks to the loop start, and broadcasts `mFlushAllNotes`).

The scheduler's sample-exact **loop-seam two-window model** is at `Source/PluginProcessor.cpp:2946-2999` — it re-derives the same wrap sample the playhead uses, from `mLoopStartBeats` / `mCachedPatternLoopBeats` through `TempoMap::sampleAtBeat`. Documented degenerate case: a loop shorter than one block falls back to one window (`:2984-2988`).

Persistence (matches the doc's table): save `StandaloneEditor.cpp:14953-14977` (`<Metronome …precountEnabled>`, `<SongLoop on>`), load `:18926-18955`. `mSongMode` is deliberately not saved.

---

## 8. What the transport depends on

**Built-in engines: no.** The playhead, both timelines, and `PlayHeadAdvancer` contain no engine references (§2). The only engine coupling is *outbound*:
- `mFlushAllNotes` broadcast on Stop/Pause (`StandaloneEditor.cpp:9633`, `:1184`).
- `enginePlayHead()` (`PluginProcessor.h:1352`, `mLastEnginePlayHead` `:459`) is handed to every engine at creation (`Source/EngineRig.cpp:685`); the comment at `PluginProcessor.h:455-457` states this is *the whole of a hosted VST3's transport*.
- `endOfflineRender` restores it and re-pushes it to built-in engine arrays by name — `Source/PluginProcessor.cpp:8102-8117` (`mGuitarsEngine`, `mBassesEngine`, `mRustyDrumsEngine`). **This is the one transport-adjacent site that hardcodes removed engines.**

**Tab model: no.** Nothing in the playhead or timelines is per-tab. Coupling is one-way, from tabs to transport: `BuilderPage::setPlayHead` (`Source/Standalone/BuilderPage.cpp:8802`, cursor at `:8841-8856`), `PianoRollPage::setPlayHead` (`Source/Standalone/PianoRollPage.cpp:131`, cursor `:88-93`), plus `onGetLoopBeats` asking the Builder/piano roll for a time selection (`StandaloneEditor.cpp:1286`, `:1302`). *Inferred*: a bus-derived tab model changes none of this as long as something still answers "current time selection" and "current pattern TS".

**Mixer: only through recording.** `startRecording` allocates one WAV per **armed Vox/Inst strip** (`PluginProcessor.h:1386`, `:1447-1466`, `mStripRecorders` `:1567`); `isRecording()` ORs midi/master/strip recorders (`:1517-1521`). The metronome/count-in reads `mVibeGraph.totalLatencySamples` for PDC (`PluginProcessor.cpp:3969-3970`) and adds into the final master buffer. `PlayHeadAdvancer`'s output routing is configured from the Mixer hamburger (`StandaloneApp.h:132-138`).

**Offline export/freeze: a separate playhead.** `struct OfflineHead` — `Source/Standalone/BuilderPage.cpp:9102-9199`, installed at `:9445`. It reads `TempoMap` through **seconds** because render rate ≠ device rate (`:9090-9093`), integrates beats blockwise (`:9163-9169`), and layers the `global_tempo` automation lane song-scope-only (`:9112-9122`, `:9125-9141`). `beginOfflineRender`/`endOfflineRender` swap the playhead and song mode (`PluginProcessor.cpp:7959-7960`, `:8102-8103`).

**Hosted VST3 transport (kept in Lite):** per-block `DSPBase::HostTransport` published from the same `pos` at `Source/PluginProcessor.cpp:3814-3832`; rack effects hold a member `RackPlayHead` (`Source/Hosting/HostedPluginEffect.h:115-122`) because `setPlayHead` stores the pointer; the out-of-process helper has its own `BridgePlayHead` (`Source/Hosting/Helper/PluginHostMain.cpp:578`).

---

## 9. Lite fork: keep / rename / change

**Keep verbatim (no edits needed):**
- `StandalonePlayHead` entire class — `StandaloneApp.h:9-129` + `StandaloneApp.cpp:171-417`.
- `TempoMapRead.h` (whole file) and `TsMapRead.h` (whole file). Both are self-contained headers with zero engine includes.
- `PlayHeadAdvancer` + `MasterOutputRouting` — `StandaloneApp.cpp:60-161`, `StandaloneApp.h:132-138`. Needed for ASIO multi-out routing, which the fork keeps.
- Startup wiring `StandaloneApp.cpp:756-760` (playhead before processor, both flag pointers).
- Loop-seam window math `PluginProcessor.cpp:2946-2999`; song-end detect `:3288-3301`; `mRequestStop` poll `StandaloneEditor.cpp:3918-3925`.
- Metronome/count-in: `MetroDSP` `PluginProcessor.h:1888-1923` + `applyPostMixRecordAndMetro` `PluginProcessor.cpp:3868-4260` + `startPlayback` `StandaloneEditor.cpp:7933-7969` + `CountInTimer` `StandaloneEditor.h:1161-1172`. Their only couplings are the master buffer, `mVibeGraph.totalLatencySamples`, `PatternManager::currentPattern()`, and `TsMap` — all kept.
- `TimeSigChange` / `TempoChange` / `TimeSignature` and the tempo/TS storage in `PatternManager` (`PatternManager.h:317-340`, `:474-478`, `:765-767`, `:958-959`, `:988-990`; `publishTimeSigMap` `PatternManager.cpp:904-931`).
- `OfflineHead` `BuilderPage.cpp:9102-9199` (offline export is in scope).

**Rename only:** nothing in the transport model carries a product-specific name. `StandalonePlayHead`, `TempoMap`, `TsMap`, `PlayHeadAdvancer`, `MetroDSP` are all generic. *Inferred*: the only rename pressure is the app name in `getApplicationName()` `StandaloneApp.h:146` / `getApplicationVersion()` `:147` and the settings paths (`audio_settings.xml`, `master_output.xml`, `settings.xml`) at `StandaloneApp.h:198-220`.

**Must change:**
1. `endOfflineRender` — `PluginProcessor.cpp:8113-8115` restores the playhead onto `mGuitarsEngine` / `mBassesEngine` / `mRustyDrumsEngine` by name. Those arrays are removed; the loop over hosted plugins at `:8112` and `mLastEnginePlayHead = mOfflinePrevHead` (`:8109`) are what survives.
2. `enginePlayHead()` propagation must reach **every insert's hosted instrument/effect** in the new bus model, not per-tab engines. `PluginProcessor.h:449-458` documents exactly why a one-time pointer change is not enough — every creation path must set it, or a hosted VST3 gets a zeroed `ProcessContext`. Under FL-style inserts the creation path is "insert gains a plugin" instead of "tab is added" (`EngineRig.cpp:685` is the current single choke point).
3. `onGetLoopBeats` — `StandaloneEditor.cpp:1280-1368` — asks `mBuilderPage->hasTimeSelection()` and `getActivePianoRollForLoop()`. Keep the shape; re-source the pattern-mode branch once tabs are bus-derived (*inferred*: "the piano roll the user was last editing" has to be re-defined when tabs are buses).
4. Record arm — `PluginProcessor.h:1386`, `:1447-1466`: "any Vox/Inst strip armed" becomes "any insert with live audio input armed" under the per-insert live-input property.
5. `setSongMode`'s automation baseline snapshot — `PluginProcessor.cpp:9207-9223` — iterates `PatternManager` Automation blocks and resolves `apvts.getParameter(paramId)`. Survives as-is only if insert/bus params keep stable APVTS ids.
6. Transport-metro pattern-mode branch reads `mPatternManager->currentPattern().tsNum/tsDen` (`PluginProcessor.cpp:4114-4119`) and `onGetTimeSig` reads the same (`StandaloneEditor.cpp:1267-1279`) — unchanged so long as PatternManager stays.

**Latent issue worth noting (not a recommendation, an observed asymmetry):** `TsMap::readSegAtBeat` and `TsMap::nextBoundaryAfterBeat` spin unbounded (`TsMapRead.h:75`, `:126`) while the equivalent `TempoMap` readers are explicitly bounded to 4 retries with the comment that an unbounded audio-thread spin "is a dropout" (`TempoMapRead.h:64-73`). Both are read from the audio thread (`PluginProcessor.cpp:4170`, `:4179-4192`).

---

# SECOND-ROUND READER 4: Keybindings / Command Catalog / Typing Keyboard — facts for the Lite fork

# Keybindings / Command Catalog / Typing Keyboard — facts for the Lite fork

Docs read first: `Plans & Specs/System Reference/Keyboard Shortcuts.md` (authoritative; confirmed against code below). Cross-refs it names: `Workspace and Windows.md`, `Transport and Playback.md`, `Mixer.md`.

## 1. What the subsystem is

Four pieces, all in `Source/Standalone/`:

| Piece | File | Role |
|---|---|---|
| Catalog | `KeyBindings.h` / `.cpp` (161 / 897 lines) | Static catalog: 37 rebindable `CommandInfo` + 170 display-only `MouseRefRow`s, 7 `Category` values, keymap.xml persistence |
| Target | `StandaloneEditor.cpp` | The app's single `ApplicationCommandTarget`; `getAllCommands`/`getCommandInfo`/`perform` at `:9645-9791` |
| Editor UI | `KeyBindsWindow.h` / `.cpp` (90 / 495) | Help > Key Binds…, one `KeyBindsTab` per `Category`, six tabs hard-coded at `KeyBindsWindow.cpp:440-458` |
| Typing keyboard | `TypingKeyboardMap.h` (64) + `StandaloneEditor.cpp:12198-12284` | Ctrl+T computer-keys-as-MIDI |

Catalog shape, verified: `KeyBindings.cpp:26-243` builds 37 commands (36 `Category::General`, 1 `Category::Builder` = `cmdToggleSlipStretchMode`, `KeyBindings.cpp:233-236`). `KeyBindings.cpp:246-807` builds the reference rows: General 2, Builder 43, PianoRoll 54, DrumKit 36, VocalEditors 24, EventEditor 11.

`Category::MouseReference = 4` (`KeyBindings.h:99`) is **dead**: it has a display name (`KeyBindings.cpp:16`) but no row and no tab ever uses it (grep: only those two hits).

Wiring facts:
- Registration + target pinning: `StandaloneEditor.cpp:1980-1996` — `registerAllCommandsForTarget(this)`, `setFirstCommandTarget(this)` (needed because pages live in parentless desktop windows), `resetToDefaultMappings()` then `BSCommands::loadMappings`, then `addKeyListener(set)`.
- Typing-note gate registered **after** the mapping set (`:1998-2005`), because `ComponentPeer::handleKeyPress` runs key listeners in reverse registration order. Same order repeated per contained window (`:16084-16087`), per aux window (`:16150-16153`), Undo History (`:12502-12503`), and inverted-by-hand for the Event Editor (`:3795-3801`).
- Persistence: `keymap.xml` in `AppPaths::appRoot()` (`KeyBindings.cpp:864-895`), written with `createXml(true)` = **diffs from defaults only** (`:878`).
- Key Binds window opened from Help id 603 (`StandaloneEditor.cpp:11866`, `:12055-12056`, `showKeyBindsWindow` at `:10061-10068`), 880x680, self-deleting (`KeyBindsWindow.cpp:476-495`).
- Screenshot harness builds its **own** `ApplicationCommandManager` straight from the catalog and renders `KeyBindsContent` at 880x1027 (`ShotHarness.cpp:727-749`). Manual expects six tabs by name (`MANUAL-1 Screenshot List.md:634`); Help menu literal `Key Binds...` id 603 is a verbatim string (`Verbatim Strings.md:259`).

## 2. Engine-bound entries in the catalog (what the fork must delete or re-target)

### 2a. `cmdShowPlayer` = `0x10017`, default F7
- Declared `KeyBindings.h:35`; catalog row + tooltip naming the built-ins: "Layers, Bass, Drums, Clip, Vox, Inst or Plugins" (`KeyBindings.cpp:58-61`).
- Dispatch `StandaloneEditor.cpp:9686-9688` → `showMostRecentPlayerTab()` (`:9827-9837`) → `getMostRecentPlayerTabId()` (`:5911-5920`) → `isPlayerTabType()` (`:5889-5909`), which whitelists `Layers, Bass, Drums, Clip, Vox, Inst, Plugins` and excludes `Mixer, Effects, Builder, PianoRoll`.
- `mLastPlayerTabId` is set in `updateActiveTabState` (`:5866`), which in the same block also tracks `mLastRollKind/mLastRollIndex` via `dynamic_cast<LayersPage/BassPage/DrumPage*>` (`:5870-5884`) — all three classes are built-in engines and disappear in the fork.
- Mirrored as View-menu item **408** `"Show Player (Most Recent)  (F7)"` (`:11819`), handler `:11984`.

Fork impact: of the seven "player" tab types only `Plugins` (hosted VST3 instrument) and, subject to the fork's own decisions, `Clip` survive; `Layers/Bass/Drums/Vox/Inst` go with the built-ins. So F7's *whole* meaning is engine-derived, and in a bus-tab model there is no "player tab" list to be most-recent in — `isPlayerTabType`, `mLastPlayerTabId` and the `LayersPage/BassPage/DrumPage` casts all need replacing, not just the command row (inferred from the code above plus the fork brief).

### 2b. `cmdShowDrumKit` = `0x10019`, default F11
- Declared `KeyBindings.h:37`; catalog `KeyBindings.cpp:78-81` ("show the 16-pad Drum Kit grid").
- Dispatch `:9704-9705` → `showDrumKitGrid()` (`:9816-9825`), which selects ribbon tab 4 then `prp->selectEngine({ EngineKind::DrumKit, 0 })` — `EngineKind::DrumKit` is the first enumerator of `PianoRollPage.h:38`.
- View-menu item **410** `"Show Drum Kit (Most Recent)  (F11)"` (`:11823`), handler `:11986`. Header comments naming F7/F11 at `StandaloneEditor.h:1315-1319`.

### 2c. `Category::DrumKit` (= 3)
- `KeyBindings.h:98`, name "Drum Kit Key Binds" (`KeyBindings.cpp:15`), **36 reference rows** `KeyBindings.cpp:578-694` (incl. five "(Piano Roll only)" explainer rows at `:680-694`), tab added at `KeyBindsWindow.cpp:448-449`.
- `DrumKitGrid.cpp` is the surface those rows document; it is also one of the three typing-keyboard bubblers (`DrumKitGrid.cpp:1264`).

### 2d. `Category::VocalEditors` (= 5)
- `KeyBindings.h:104` (comment names BaySickPitch / BaySickAlign), name `KeyBindings.cpp:17`, **24 rows** `KeyBindings.cpp:698-770` (BaySickPitch gestures `:698-759`, BaySickAlign `:762-770`), tab at `KeyBindsWindow.cpp:453-454`.

### 2e. Engine-flavoured text inside surviving rows
- `cmdShowEffectsRack` tooltip is engine-neutral (`KeyBindings.cpp:63-66`) — the Effects page remembers its own channel; dispatch `:9689-9695`.
- `cmdShowPianoRoll` tooltip says "on whichever engine its own dropdown is set to" (`:73-76`) — the dropdown is the engine model (see §3).
- Builder browser rows mention Vox/Inst recording groups (`KeyBindings.cpp:385-399`) and the Vox strip Arm LED (`:397-399`).
- Piano Roll `S` = "Cycle Note Type … RP Slide / RT Slide / Portamento" (`:423-425`) is a built-in-synth note model, not a VST3 concept.

### 2f. Not engine-bound (survive unchanged)
`cmdShowBuilder` F5 (`:9680-9682`), `cmdShowMixer` F6 (`:9683-9685`), `cmdShowEffectPanel` F9 (`:9696-9700`, `getMostRecentEffectPanel`), `cmdShowEventEditor` F12 (`:9707-9710`), `cmdShowManuals` F1 (`:9711-9713`), all file ops `:9716-9720`, pattern nav/list `:9723-9733`, transport `:9736-9762`, undo/redo `:9770-9771`, precount `:9774-9776`, Slip/Stretch `:9782-9786`. View items 404/405/406/407/409/411 (`:11817-11824`, handlers `:11980-11987`).

## 3. Typing keyboard — the only keyboard audition route into a hosted plugin

`TypingKeyboardMap.h`
- `gActive` atomic (`:16`), two-octave map `semitoneForKey` (`:21-45`), `isOctaveShiftKey` PgUp/PgDn (`:47-51`), `shouldBypassLocalKeys` — bare keys only, any Ctrl/Alt/Shift = normal shortcut (`:56-63`).
- Bubblers that must decline mapped keys: `PianoRoll.cpp:1178`, `DrumKitGrid.cpp:1264`, `BuilderPage.cpp:7706`. Nothing else calls it.

`StandaloneEditor`
- State: `mTypingKeyboardOn`, `mTypingOctaveOffset` (clamped −5..+3), `mTypingHeldNotes` (keyCode→note), `mPluginAuditionHeldNote` (`StandaloneEditor.h:895-908`).
- `toggleTypingKeyboard()` `:12198-12204` — flips mode, releases held notes, stores `gActive`, pushes the transport-bar LED via `setTypingKeyboardOn`.
- `sendTypingNote()` `:12206-12215` — builds `noteOn(ch1, note, 0.8f)`/`noteOff`, self-timestamps (MidiMessageCollector asserts on zero), pushes to `mProcessor.getLiveMidiCollector()`.
- `releaseAllTypingNotes()` `:12217-12234` — also releases the plugin-audition note.
- `keyPressed` `:12236-12266`, `keyStateChanged` `:12268-12284` (diffs held set against `KeyPress::isKeyCurrentlyDown`; returns false so key-up dispatch survives).
- Two entry points only: Ctrl+T (`cmdToggleTypingKeyboard` = `0x10071`, `KeyBindings.h:88`, catalog `KeyBindings.cpp:239-242`, dispatch `:9765-9767`) and the transport-bar `KeyboardMidiButton` (`GlobalTransportBar.cpp:341-353`, click `:431`, LED push `:970-977`, wired `StandaloneEditor.cpp:1392`).
- Alt-Tab backstop: `StandaloneApp.cpp:46` calls `editor->releaseAllTypingNotes()` (public for exactly that reason, `StandaloneEditor.h:929-933`); also called on stop paths at `:6755`.

**Where the notes actually land (the fork-critical chain):**
1. `getLiveMidiCollector()` (`PluginProcessor.h:643`) — same queue hardware MIDI uses (`StandaloneApp.cpp:1379`).
2. `PluginProcessor::processBlock` `:3574-3594` drains it and picks a destination buffer from `mLiveMidiTargetKind/Index`: 1 Layer, 2 Bass, 3 Drum, 4 Clip, 7/8 sfizz Inst, 9 Rusty Drums, **10 = `pluginPageMidi[idx]`, the hosted VST3 instrument tab** (`:3593-3594`). Anything else drops (`PluginProcessor.h:634-642`).
3. The target is pushed by `PianoRollPage::onEngineSelected` → `setLiveMidiTarget((int) id.kind, id.index)` (`StandaloneEditor.cpp:2378-2396`, initial push `:2400-2403`). `EngineKind::Plugin` is enumerator 10 of `PianoRollPage.h:38`.
4. The roll's dropdown is built from the **ribbon tab list** by `dynamic_cast` per page type (`StandaloneEditor.cpp:2300-2376`); the plugin case at `:2344-2349` is gated on `pp->getEngineProcessor() != nullptr`, and Vox / live-input Inst are excluded (`:2350-2366`).
5. Extra per-kind fixups on that path: `liveTranspose = (kind == 8) ? -12 : 0` for BaySickBasses (`PluginProcessor.cpp:3604`), live-note monitor lighting (`:3617-3622`), and a transport-sync/clock filter added **specifically because hosted plugins honour MIDI clock** (`:3624-3640`).
6. Same `sendTypingNote` path is the plugin piano-roll audition (`StandaloneEditor.cpp:10995-11044`): `auditionMomentary/On/Off` on `registerEngine({EngineKind::Plugin, idx}, conn)` (`:11044`), relying on the roll's engine pick and the live target being the same tab "BY CONSTRUCTION" (`:10996-11001`).

So: the typing keyboard depends on **built-in engines only for the dead branches** (kinds 1-4, 7-9), but depends **hard on the tab model** — the ribbon tab list → roll dropdown → `setLiveMidiTarget` chain is the only thing that names a destination. It does **not** depend on the mixer at all (no mixer/bus code appears anywhere on this path).

## 4. What the Lite fork keeps / renames / changes

**Keep as-is (no engine coupling):** the whole `ApplicationCommandManager` + `KeyPressMappingSet` framework and its registration order (`StandaloneEditor.cpp:1980-2005`, `:16084-16087`, `:16150-16153`, `:3795-3801`); `keymap.xml` persistence (`KeyBindings.cpp:864-895`); `KeyBindsWindow`'s table, Set/Reset, capture modal and both conflict prompts (`KeyBindsWindow.cpp:19-429`); `findHardcodedConflicts` (`KeyBindings.cpp:846-861`); `TypingKeyboardMap.h` in full; `sendTypingNote`/`releaseAllTypingNotes`/`keyPressed`/`keyStateChanged` (`:12206-12284`); Ctrl+T + the transport button; the 34 non-engine commands listed in §2f; Categories General, Builder, PianoRoll, EventEditor and their 2/43/54/11 rows.

**Delete:** `cmdShowPlayer` (`KeyBindings.h:35`, `KeyBindings.cpp:58-61`, `StandaloneEditor.cpp:9686-9688`, `:9827-9837`) or re-target it; `cmdShowDrumKit` (`KeyBindings.h:37`, `KeyBindings.cpp:78-81`, `:9704-9705`, `:9816-9825`); `Category::DrumKit` + its 36 rows + its tab (`KeyBindings.h:98`, `KeyBindings.cpp:15`, `:578-694`, `KeyBindsWindow.cpp:448-449`); `Category::VocalEditors` + its 24 rows + its tab (`KeyBindings.h:104`, `KeyBindings.cpp:17`, `:698-770`, `KeyBindsWindow.cpp:453-454`); View items 408/410 (`:11819`, `:11823`, `:11984`, `:11986`); the `DrumKitGrid.cpp:1264` bubbler dies with its file; `liveTranspose` kind-8 branch (`PluginProcessor.cpp:3604`) and live-target kinds 1-4/7-9 (`:3587-3592`) become unreachable.

**Rename / re-target (fork's new bus-tab model):** F7's handler chain — `isPlayerTabType` (`:5889-5909`), `mLastPlayerTabId` (`:5866`), `getMostRecentPlayerTabId` (`:5911-5920`) — currently enumerates engine page types; a bus-derived tab model has no such list. Same for the `LayersPage/BassPage/DrumPage` casts at `:5870-5884`. The typing keyboard's destination resolution (`:2300-2403` → `setLiveMidiTarget` → `PluginProcessor.cpp:3581-3594`) is the one place a fork must re-point if instrument selection stops coming from the Piano Roll's engine dropdown; today `EngineKind::Plugin`/kind 10 already works and is the surviving branch.

**Watch (facts, not advice):**
- Deleting a command id does not corrupt an existing `keymap.xml`: it stores diffs only (`KeyBindings.cpp:878`) and `KeyPressMappingSet::restoreFromXml` (`JUCE/modules/juce_gui_basics/commands/juce_KeyPressMappingSet.cpp:222-245`) hands each hex id to `addKeyPress`, which only creates a mapping when `commandManager.getCommandForID(commandID) != nullptr` — an unknown id is silently dropped. A user's F7/F11 *rebind* would vanish, not crash.
- Removing rows shrinks `findHardcodedConflicts`' warning surface (`KeyBindings.cpp:846-861`) — DrumKit/Vocal-only keys stop warning.
- `KeyBindsContent` adds all six tabs unconditionally (`KeyBindsWindow.cpp:440-458`); `ShotHarness::shootKeybinds` hard-codes 880x1027 for the General tab (`ShotHarness.cpp:746`), and the manual asserts "Six tabs: General, Builder, Piano Roll, Drum Kit, Vocal Editors, Event Editor" (`MANUAL-1 Screenshot List.md:634`, tab-count note `:4124`).
- `Category::MouseReference` is already dead (`KeyBindings.h:99` / `KeyBindings.cpp:16` only).
- Doc drift the fork inherits: `Keyboard Shortcuts.md:110` (F7 engine list), `:114` (F11), `:64-66` (Drum Kit / Vocal Editor tabs), `:326-420` (Drum Kit + vocal-editor sections), `:175-186` (typing keyboard "plays the active tab's instrument").

---

# SECOND-ROUND READER 5: Undo System — facts for the Lite fork

# Undo System — facts for the Lite fork

Doc read first: `C:/Users/jeffm/Documents/BaySickDAW/Plans & Specs/System Reference/Undo History.md` (authoritative; its action table lists 13 rows and omits `PatternListAction`'s siblings correctly but the code has **14 classes**). Everything below is confirmed in code.

---

## 1. Shape of the machinery (shell-level, no engine deps)

| Piece | Evidence |
|---|---|
| One `juce::UndoManager`, **owned by the processor**, editor holds a reference | `Source/PluginProcessor.h:287-289` ("StandaloneEditor::mUndoManager is a REFERENCE to this one -- there is no [second]"), `Source/Standalone/StandaloneEditor.h:846`, ctor init `StandaloneEditor.cpp:747` |
| Depth cap `setMaxNumberOfStoredUnits(1, N)` — 1 unit per action so N == steps | init `StandaloneEditor.cpp:766`; menu 100/250/500/1000 `StandaloneEditor.cpp:11993-12000`, submenu built at `:11845-11857` |
| Editor is a `ChangeListener` on the manager | `StandaloneEditor.cpp:773`, callback `:12351-12403` |
| Central perform: `doUndoAction(action,label,ownerKey)` — flush, `beginNewTransaction("owner|label")`, perform, rebuild labels | `StandaloneEditor.cpp:12289-12310` |
| `globalUndo` / `globalRedo` — flush pending gestures, open `MissingFileReport::ScopedGesture("restored tab")`, then manager call + label rebuild | `:12312-12349` |
| Label list + cursor derived from manager descriptions | `rebuildHistoryLabels` `:12405-12416`; `historyDisplayFor` `:12418-12435` |
| History window (list, `">>  Current"` marker, click-to-time-travel) | `Source/Standalone/UndoHistoryWindow.h:15-86`, `.cpp:24-47` (rows), `.cpp:88-105` (click = N × globalUndo/Redo), created lazily `StandaloneEditor.cpp:12485-12507` |
| Dirty flag = transaction-pointer mismatch, not a markDirty call | `TransactionTracker` `PluginProcessor.h:304-321` (`isDirty()`, `onUndo/onRedo/onNewTransactions/onSave/onLoadReset`), fed only from `changeListenerCallback` `:12364-12391`; `ProjectManager::isDirty` `Source/ProjectManager.cpp:96-98` |
| Load boundary: clear history + sweep snapshots + reset tracker | `ProjectManager.cpp:123-125`; startup sweep `StandaloneEditor.cpp:777`; teardown `:2126-2128` |
| Keyboard | `BSCommands::cmdGlobalUndo/cmdGlobalRedo = 0x10050/0x10051` `Source/Standalone/KeyBindings.h:73-74`, registered `KeyBindings.cpp:212-217`, dispatched `StandaloneEditor.cpp:9770-9771`; menu items 201/202/203 `:11780-11783` |

None of this block references a built-in engine. It survives the fork unchanged.

---

## 2. Action inventory — 14 classes in `Source/Standalone/UndoActions.h`

| # | Class | Decl | Call sites | Engine coupling |
|---|---|---|---|---|
| 1 | `PianoRollEditAction` | `:51-88` | `Source/Standalone/PianoRoll.cpp:703` | none (uses `PianoNote`) |
| 2 | **`PitchEditAction`** | `:95-132` | `Source/BaySickVocal/BaySickPitchEditor.cpp:2107` — **only site** | **Vocal-only**; also forces `#include "../DSP/BaySickPitchDSP.h"` at `UndoActions.h:9` for `PitchNoteRegion` |
| 3 | `ArrangementEditAction` | `:137-171` | `BuilderPage.cpp:3912` | none |
| 4 | **`MixerStateAction`** | `:176-199` | `MixerPage.cpp:1849, 1862, 3521, 3534, 3553, 3565, 3575, 3587` (8 sites, all `applyMixerSnapshot`) | snapshots `MixerState` — see §3 |
| 5 | `FloatParamAction` | `:205-226` | `EffectEditorPanels.cpp:165` — **only site** (built-in effect editor panels) | dies with the built-in effect panels unless re-used |
| 6 | `EffectRackAction` | `:236-290` | `EffectsPage.cpp:1335, 1382, 1403` | `SlotSnapshot::type` is `EffectType` (`Source/EffectRack.h:19`) and array width is `EffectRack::kNumSlots = 6` (`EffectRack.h:142`) — the built-in effect enum |
| 7 | `AutomationLaneEditAction` | `:296-323` | `EventEditor.cpp:151, 1731, 1960, 2060` | none |
| 8 | **`StructuralOpAction`** | `:334-370` | 20+ sites, see §4 | owns snapshot `juce::File`s, deletes them in dtor `:345-349` |
| 9 | `PatternListAction` | `:392-417` (+ `PatternListSnapshot` `:381-390`) | `BuilderPage.cpp:1798, 4386, 4452, 4718`; `StandaloneEditor.cpp:21241` | none |
| 10 | `PatternRenameAction` | `:423-444` | `BuilderPage.cpp:2113`, `StandaloneEditor.cpp:1592` | none |
| 11 | `PatternColorAction` | `:446-467` | `BuilderPage.cpp:1997`, `StandaloneEditor.cpp:1620` | none |
| 12 | `MarkerSetAction` | `:484-505` | `BuilderPage.cpp:3943, 4425, 4455, 4723` | none |
| 13 | `AudioLibraryAction` | `:521-546` | `BuilderPage.cpp:1848, 5727`; `StandaloneEditor.cpp:3059, 3365, 10692, 21247, 21252` | none |
| 14 | `AutomationTemplateAction` | `:560-586` | `BuilderPage.cpp:1889, 9050`; `StandaloneEditor.cpp:4107` | none |

`UndoContext` itself (`:17-46`) is the token: `manager`, `perform`, `undo`, `redo`, `showHistory`, `resolveOwnerPage`. Header-level includes are `../PatternManager.h`, `../EffectRack.h`, `../DSP/BaySickPitchDSP.h` (`:7-9`) — two of the three are engine/effect-side headers.

Fork consequence (facts): removing Vocal breaks `UndoActions.h:9` and orphans class #2; removing the built-in effect DSP breaks `EffectType`/`kNumSlots` in class #6 unless `EffectRack` keeps a VST3-only slot type; class #5 loses its only caller.

---

## 3. `MixerStateAction` and the family-keyed `MixerState`

**The struct** `Source/PatternManager.h:487-532`, explicitly flagged as a legacy cache: "MixerState is being migrated to lazy APVTS ... stay for now as a snapshot cache for undo/preset I/O and backward-compat" (`:481-486`).

Fields are hard-keyed to the built-in families:
- `layersLevel/bassLevel/drumsLevel` + matching mute/solo/pan `:490-504`
- `drumSlotLevel[kMaxDrumPages]`, `drumSlotPan[kMaxDrumPages]` `:510-511`
- `audioRowLevel[100]`, `audioRowMute[100]` (`kMaxAudioRows` `:515-517`)
- `audioClipsBusLevel/Pan/Mute/Solo` `:520-523`

**How undo uses it:** `MixerPage` snapshots the whole struct on drag-start and again on drag-end and hands both to `MixerStateAction`; the apply lambda is `applyMixerSnapshot`, which is a wholesale assignment plus a UI resync — `MixerPage.cpp:3699-3703` (`mPM.getMixer() = state; syncFromModel();`). Fader/pan/mute/solo wiring: `MixerPage.cpp:3516-3593` (buses + master), drums `:1844-1871`. Bus strips are bound by **reference into named MixerState fields**: `wireBusCallbacks(mLayersBusStrip.get(), mx.layersLevel, mx.layersPan, mx.layersMute, mx.layersSolo)` `MixerPage.cpp:1522`, clips bus `:1525`.

**The second, APVTS-based model exists in parallel:** per-strip params are addressed by `MixerChannelIds` prefixes and channel ids (`MixerPage.cpp:1627-1637` bus ids; `:1680, 1713, 1746, 1798, 1836` insert ids), their undo goes through `beginParamUndoGesture` (`MixerTrackStrip.cpp:415, 442`; routing/sends `MixerPage.cpp:720, 734, 760, 807, 834, 861, 1200, 1270, 1330`), and `syncApvtsFromMixerState()` mirrors the struct into APVTS under `ScopedProgrammaticParamWrites` at construction `MixerPage.cpp:3642-3696`. So there are two mixer state models with two different undo paths.

**MixerState is still live outside the UI:** persisted to the project ValueTree `PatternManager.cpp:1256-1282` (save) / `:1559-1592` (load), and read on the audio side at `PluginProcessor.cpp:6556-6572`, `Source/Engine/Tasks/CompositeAudioInsertTask.cpp:145`, `InstStripTask.cpp:129`, `VoxStripTask.cpp:122`.

Fork consequence (facts): an FL-style "bank of inserts" has no `layers*/bass*/drums*` fields, no `drumSlotLevel[kMaxDrumPages]`, and `audioRow*` is arrangement-row-keyed, not insert-keyed. Every one of the 8 `MixerStateAction` sites is a bus/master/drum-slot strip. The APVTS + `beginParamUndoGesture` path is already insert-keyed and family-neutral (inferred: it is the one that generalises).

---

## 4. Structural undo + the snapshot store

`StructuralOpAction` (`UndoActions.h:334-370`): `undoFn`/`redoFn` closures, skip-first perform `:351-356`, and a `juce::Array<juce::File>` of owned snapshots deleted in the destructor `:345-349` — that destructor is what bounds the store by history depth.

`UndoSnapshotStore` (`Source/Standalone/UndoSnapshotStore.h`): `dir()` = `AppPaths::appRoot()/"UndoSnapshots"` `:15-18`; `writeNew` writes `snap_<millis>_<counter>.xml` and **returns an empty `File` on write failure** `:27-39`; `sweepAll()` `:41-47`. The empty-file contract is enforced at the destructive callers: `StandaloneEditor.cpp:3436-3440` and `:13076-13082` / `:13119-13124` abort the delete and show `UserFileSave::kTabNotDeleted` rather than leave an un-undoable "Delete" row.

Call sites (all `StructuralOpAction`), with what they snapshot:

| Site | Gesture | Engine coupling |
|---|---|---|
| `StandaloneEditor.cpp:1818-1838` | Add tab ("Add \<engine\>") via page-preset XML | L/B/D page-preset spine |
| `:3433-3456` | Delete tab (record path) | `captureTabRecord` |
| `:5084-5089` | Rename tab (`applyPageRename`, no snapshot) | `RenameFamily` |
| `:8564-8567` | Lock/Unlock Kit N (`applyDrumLockStates`) | **Drums-only** |
| `:9245-9309` | **Load Kit** — snapshots every drum tab in a bank, replays via `spawnDuplicateDrumTab` | **Drums/kit-only** |
| `:9902-9905` | Rename / Color Pattern | none |
| `:10181-10207` | Rusty chain swap (`captureTabRecord` + `PatternListSnapshot`) | **BaySickRustyDrums-only** |
| `:12561-12580` | Duplicate tab | L/B/D dynamic_casts `:12553-12558` |
| `:13070-13112` | Delete tab, record path with `AudioLibrarySnapshot` (Clips/Vox/Inst) + `PatternListSnapshot` (Rusty) `:13060-13066` | Clips/Vox/Inst/Rusty |
| `:13117-13159` | Delete tab, page-preset path (Layers/Bass/Drums switch `:13140-13148`) | L/B/D |
| `:13194-13225` | `wrapTabAddUndo` — generic add via `captureTabRecord`/`resurrectTabFromRecord` | kind-agnostic wrapper |
| `LayersPage.cpp:962, 1090-1105`; `BassPage.cpp:1004, 1082`; `DrumPage.cpp:837, 864`; `InstPage.cpp:1180, 1203`; `ClipsPage.cpp:123` | page-preset load, lock/unlock, sfizz program switch | per-engine pages |
| `PluginsPage.cpp:504-543` | **`performChainSwapGesture`** — encodes/restores hosted state with `getStateInformation`/`setStateInformation`, resolves the live page through `mUndoCtx.resolveOwnerPage` | **VST3-generic; no built-in engine types** |

The capture/restore spine underneath: `captureTabRecord` `StandaloneEditor.cpp:12584-12663` is a chain of `dynamic_cast`s over `PluginsPage / ClipsPage / VoxPage / InstPage / BaySickRustyDrumsPage` (returns `nullptr` for L/B/D, which use the `PagePresetIO` path `:12662`), with BaySickGuitars/BaySickBasses-specific `kitPath` + `sfizzEngineData` branches `:12632-12649`. `resurrectTabFromRecordImpl` `:12677+` switches on the same `type` strings.

`ClipsPage.cpp:121-125` bypasses `UndoContext` and calls `mFullProcessor->mUndoManager` directly (`beginNewTransaction` + `perform`).

---

## 5. Owner-key resolution — the two engine-keyed tables

**`ownerKeyForParamId`** `StandaloneEditor.cpp:12437-12483`:
- hard-coded sfizz prefixes: `brd_` → `"rusty"` `:12440`; `bgg_` → `"inst<N>"` `:12441-12443`; `bbb_` → `"inst<N>"` `:12444-12446`
- then a `TabKind` table `{Layers "lay", Bass "bass", Drums "drm", Clips "clip", Vox "vox", Inst "inst", Plugins "plug"}` `:12453-12458` walked over `EngineRig::capacityOf(kind)` × `rig.findTab` `:12459-12464`, probing three APVTSs per tab: `EngineRig::apvtsOf(t->engine)` `:12465`, `t->namIr` `:12468-12471`, `t->pedals` `:12472-12475`, plus a `BaySickVocalProcessor::getNamIrProcessor()` special case `:12476-12480`
- returns `"app"` otherwise `:12482` — which is what every main-APVTS mixer param gets.

Consumed only for **history-row labels**: `historyDisplayFor` `:12418-12435` turns `"param:<id>"` into `owner + "|" + displayNameFor(lane)`; `displayNameFor` `:4813-4824`.

**`resolveOwnerPage`** inside `makeUndoContext` `:13227-13268`: prefix table `{"lay" Layers, "bass" Bass, "drm" Drums, "vox" Vox, "inst" Inst, "plug" Plugins}` `:13246-13253`, matched against `ownerKey` + a numeric suffix, then resolved by `e->type == m.type && pageIndexOfEntry(*e) == idx` `:13260-13262`. Note the table has **no `"clip"` entry** even though `ownerKeyForParamId` emits one.

Owner keys are minted at page creation: `makeUndoContext("lay"+idx)` `:2471, 2514`, `"plug"+idx` `:2491, 2504`, `"bass"+idx` `:2525, 2542`, `"drm"+idx` `:2743`, `"vox"+idx` `:11235`, `"inst"+idx` `:11362`; default `"app"` for PianoRoll `:2294`, EventEditor `:3775`, MixerPage `:5094`, `:5328`, `:16215` (`StandaloneEditor.h:135`).

`TabKind` itself is `{Layers, Bass, Drums, Clips, Vox, Inst, Plugins, Rusty}` `Source/EngineRig.h:49`.

---

## 6. Patched JUCE the undo system depends on

All under `C:/Users/jeffm/Documents/BaySickDAW/juce/modules/juce_audio_processors/utilities/`, marked `BaySickDAW QA-UndoCoverage`:

| Patch | Evidence | Why it exists |
|---|---|---|
| Named gesture transactions | `juce_ParameterAttachments.cpp:69-87` — `beginGesture()` calls `flushAllLiveInstancesToValueTrees()` then `undoManager->beginNewTransaction("param:" + parameter.paramID)` | stock JUCE begins gestures unnamed; `"param:"` is the marker `historyDisplayFor` parses |
| `flushAllLiveInstancesToValueTrees()` + live-instance registry | decl `juce_AudioProcessorValueTreeState.h:480-488`, impl `.cpp:43-66` | flush timer is slower than quick gestures; every boundary (`UndoBracket.h:22, 31`, `StandaloneEditor.cpp:12297, 12322, 12340`) calls it first |
| `programmaticWritePhase` (thread_local) + `ScopedProgrammaticParamWrites` | `.h:445-467`; write-time mark `.cpp:274-281`; flush-time consume `.cpp:227-231` | preset/automation/baseline writes never enter the history |
| `ApvtsParamValueUndoAction` (tag-resolving, replaces tree-bound `SetPropertyAction`) | `.cpp:69-113` and `um->perform(new ApvtsParamValueUndoAction(...))` `.cpp:246-250` | redo must survive engine destroy/re-create |
| `undoOwnerTag` + `findByUndoOwnerTag` | `.h:468-479`, `.cpp:52-60` | stable identity across resurrection |
| `replaceStateKeepingUndoHistory(newState, undoTransactionName)` (+ `StateSwapAction`) | `.h:406-426`, `.cpp:558-575` | plain `replaceState` calls `clearUndoHistory()` `.cpp:550-556`; with one global manager an engine preset load would wipe the whole history |
| Lazy-registration bind fix in `addParameterAdapter` | `.cpp:470-486` | BaySickDAW registers params lazily (QA-0a), so late adapters never transacted |

Tag values in app code: `"main"` `Source/PluginProcessor.cpp:563`; `"rusty"` `BaySickRustyDrumsProcessor.cpp:27`; `"sfizz:"+prefix` `BaySickGuitarsProcessor.cpp:30`, `BaySickBassesProcessor.cpp:29`; `"rig:"+kind+…` `EngineRig.cpp:545, 562-565, 577-578, 638, 641`.

`replaceStateKeepingUndoHistory` has ~25 call sites, **all in built-in engines/pages** (`BaySickBassEditor.cpp:1106`, `BaySickSynthEditor.cpp:1217`, `BaySickSolsticeEditor.cpp:1405, 1425`, `BaySickPlayerEditor.cpp:888`, `BaySickNAMIRProcessor.cpp:1251`, `BaySickPedalsProcessor.cpp:414`, `BaySickVocalProcessor.cpp:2193`, `LayersPage.cpp:254-256`, `BassPage.cpp:251-253`, `DrumPage.cpp:758, 1057`, `InstPage.cpp:1132`, `PagePresetIO.cpp:265`, `StandaloneEditor.cpp:12925, 12996, 18678, 18851`). No hosted-VST3 caller.

---

## 7. `ApvtsDirtyTracker` and the dirty hook

`Source/Standalone/ApvtsDirtyTracker.h:31-81`. Two jobs, only one of them undo-adjacent:
1. `onAny` → project dirty marker `:45, 60`
2. `hasChangedSinceLastBlock()` — a lock-free **audio-thread per-block gate** `:51-54`, armed at ctor `:78` and re-armed on `valueTreeRedirected` `:72-75`

Its header names the ten built-in processors `:9-12`. Each declares the tracker + `setOnAnyStateChange`: `BaySickSolsticeProcessor.h:64`, `BaySickSynthProcessor.h:54`, `BaySickBassProcessor.h:54`, `BaySickPlayerProcessor.h:56`, `BaySickGuitarsProcessor.h:157`, `BaySickBassesProcessor.h:147`, `BaySickRustyDrumsProcessor.h:193`, `BaySickPedalsProcessor.h:92`, `BaySickNAMIRProcessor.h:98`, `BaySickVocalProcessor.h:91`.

`StandaloneEditor::wireEngineDirtyHook` `:11053-11076` is a **ten-way `dynamic_cast` chain** (nine returns + a Vocal branch that recurses into `getNamIrProcessor()` `:11069-11075`), called from 20 sites (`:2572, 2629, 2691, 5658, 5765, 5804, 8615, 8672, 9459, 10127, 10392, 10494, 10781, 11294, 11383, 11384, 12898, 18318, 18370, 18640, 18731`). Declared `StandaloneEditor.h:721`.

**`PluginsPage` has no dirty wiring at all** — `grep markDirty|DirtyTracker|onAnyStateChange` over `PluginsPage.cpp/.h` returns nothing. So today a hosted-VST3 parameter tweak does not reach `ProjectManager::markDirty` through this hook (it can still reach the transaction tracker only if it goes through the main APVTS, which hosted plugin params do not).

---

## 8. What the fork keeps / re-keys / drops — grounded

**Keeps unchanged (no engine or tab-family reference):**
- `UndoBracket.h` (entire file, 34 lines)
- `UndoSnapshotStore.h` (entire file)
- `UndoHistoryWindow.h/.cpp` (both files; only `SharedUI.h` + `WindowChrome.h` deps)
- `UndoContext` `UndoActions.h:17-46`; actions #1, #3, #7, #8, #9–#14
- The manager/dispatch/dirty spine: `PluginProcessor.h:287-324`, `StandaloneEditor.cpp:766-773, 12289-12416, 12485-12507`, `ProjectManager.cpp:123-125`, `KeyBindings.h:73-74`
- All seven JUCE patches in §6 — they are keyed by *tags and param ids*, not engine types; the fork's VST3 wrappers and the main APVTS both need `flushAllLiveInstancesToValueTrees`, `programmaticWritePhase`, and `ApvtsParamValueUndoAction`

**Deleted outright:**
- `PitchEditAction` `UndoActions.h:95-132` + the `#include "../DSP/BaySickPitchDSP.h"` at `:9` (only consumer `BaySickPitchEditor.cpp:2107`)
- `FloatParamAction` `:205-226` if `EffectEditorPanels.cpp:165` goes (its only caller)
- `applyDrumLockStates` / `Load Kit` structural ops `StandaloneEditor.cpp:8564, 9245-9309`; the Rusty chain swap `:10181-10207`
- The `brd_`/`bgg_`/`bbb_` prefix branches `:12440-12446`
- `wireEngineDirtyHook`'s ten casts `:11060-11075` and all ten `setOnAnyStateChange` declarations; `ApvtsDirtyTracker` survives only if the fork's VST3 wrapper keeps an APVTS (inferred — no such wrapper exists today, `PluginsPage` has none)

**Must be re-keyed, not deleted:**
- `MixerStateAction` `:176-199` survives as a class but its payload `MixerState` `PatternManager.h:487-532` is family-keyed at every field; the 8 call sites `MixerPage.cpp:1849-3587` all bind bus/master/drum-slot refs (`:1522, 1525`), and the wholesale `mPM.getMixer() = state` apply `:3701` would clobber an insert-bank model. The already-APVTS path (`beginParamUndoGesture` at `MixerTrackStrip.cpp:415, 442` and the routing sites `MixerPage.cpp:720-1330`) is insert-keyed and needs no re-keying (inferred: this is the surviving model; the struct's own comment `PatternManager.h:481-486` says the migration was already planned).
- `ownerKeyForParamId` `:12453-12481` and `resolveOwnerPage` `:13246-13253`: both tables are tab-family lists. A bus-derived tab model needs a bus/insert key in both, and the mismatch already present (`"clip"` emitted at `:12456` but absent from the `resolveOwnerPage` table `:13246-13253`) shows the two tables are maintained independently.
- `captureTabRecord` `:12584-12663` / `resurrectTabFromRecordImpl` `:12677+`: the structural-undo restore spine. Only the `PluginsPage` branch `:12596-12603` is VST3-generic; the Clips/Vox/Inst/Rusty branches and the BaySickGuitars/BaySickBasses `kitPath`+`sfizzEngineData` block `:12632-12649` are built-in-specific.
- `EffectRackAction` `:236-290`: `EffectType` (`EffectRack.h:19`) and `kNumSlots = 6` (`EffectRack.h:142`) must become a VST3-only slot descriptor for the action to compile.

**The pattern the fork already has for VST3:** `PluginsPage::performChainSwapGesture` `PluginsPage.cpp:504-543` — `getStateInformation`/`setStateInformation` base64 into `UndoSnapshotStore::writeNew`, live page via `mUndoCtx.resolveOwnerPage`, one `StructuralOpAction` owning both files. No engine type appears in it. (Inferred: this is the shape every fork structural op would take.)

---

# CRITIC

```json
{
  "missing": [
    {
      "area": "Transport model: StandalonePlayHead, tempo/time-sig timeline, PlayHeadAdvancer, metronome + count-in DSP, song/pattern + loop state",
      "why": "The map cites only GlobalTransportBar.cpp/.h + MetroPanel.h (the widgets) and a global_tempo lane. The transport MODEL the Lite shell must keep is elsewhere and uncited: StandalonePlayHead (tempo markers, seek, loop, BPM, rebuildTimeline) lives in StandaloneApp.h:9 / StandaloneApp.cpp:171-420; the app-wide seqlock tempo timeline is namespace-global state in TempoMapRead.h (read by the audio thread AND the render workers) plus TsMapRead.h; PlayHeadAdvancer (StandaloneApp.cpp:50-160) is the real AudioIODeviceCallback and also does physical master-output channel routing; metronome/count-in DSP is inside PluginProcessor.h:1887-1921 with the post-mix recorder/metronome pipeline at :1500-1520, mSongMode at :1817, loop-seam/seek-discontinuity flags at :427-437; TimeSigChange/TimeSignature at PatternManager.h:317/:474. A fork plan cannot sequence 'keep transport' without these.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneApp.h (:9-133), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneApp.cpp (:50-160, :171-420), C:/Users/jeffm/Documents/BaySickDAW/Source/TempoMapRead.h, C:/Users/jeffm/Documents/BaySickDAW/Source/TsMapRead.h, C:/Users/jeffm/Documents/BaySickDAW/Source/PluginProcessor.h (:427-460, :1500-1520, :1817-1819, :1887-1955), C:/Users/jeffm/Documents/BaySickDAW/Source/PatternManager.h (:317, :474), C:/Users/jeffm/Documents/BaySickDAW/Plans & Specs/System Reference/Transport and Playback.md"
    },
    {
      "area": "Settings stores: settings.xml key inventory (incl. sibling sections written by three subsystems), ui_prefs.xml keys, File Settings dialog, Options menu, and the DrumTriggerVelocity pref that welds Audio Settings to DrumTriggerMap",
      "why": "The map names the files (settings.xml, ui_prefs.xml, audio_settings.xml, master_output.xml, keymap.xml, plugins.xml) but never what is in them or who writes them. settings.xml is written by ProjectManager::saveSettings (RecentProjects, shortcutCreated, migratedFromRoaming, skipGlobalLockPromptBank0/1, skipKitReplacePrompt, skipCoreContentPrompt, defaultTemplate - three of those are kit/Core-Library keys that die) AND by sibling-preserving writers in StandaloneApp (MultiCoreRendering, MidiTriggerVelocity) and PatternColorPicker (RecentPatternColors). ui_prefs.xml carries fsAutoFreezeCpu / fsInstrumentFreeze / fsCaptureRetain / fsCaptureAudio / exSpecId / exSpecCustom via a file-local openUiPrefs() duplicated in BaySickPitchEditor (Vocal, deleted). The Options menu (StandaloneEditor.cpp:11663 case 4) has 'Get Sound Content...' (id 505 -> offerCoreContentDownload) and the Audio Settings dialog's MIDI velocity combo (StandaloneEditor.cpp:158-166) plus StandaloneApp.cpp:535/555 and PluginProcessor.cpp:8577 all read DrumTriggerVelocity::gUseFixed, which is declared in DrumTriggerMap.h:46 - so deleting DrumTriggerMap (listed as deleted) breaks the Audio Settings dialog and the app's pref loader unless that namespace is relocated. None of this coupling is in the map.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/ProjectManager.cpp (:18-28, :411-470, :476-520, :685-745), C:/Users/jeffm/Documents/BaySickDAW/Source/ProjectManager.h (:113-135, :236-252, :286-314), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneApp.cpp (:480-560), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneApp.h (:58-80), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/PatternColorPicker.cpp (:124-160), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:111-170 AudioSettingsDialog, :11663-11880 getMenuForIndex, :11881-12060 menuItemSelected, :20325-20480 openUiPrefs + FileSettingsComp, :20560 showFileSettingsDialog), C:/Users/jeffm/Documents/BaySickDAW/Source/MidiLearn/DrumTriggerMap.h (:46), C:/Users/jeffm/Documents/BaySickDAW/Source/PluginProcessor.cpp (:8577)"
    },
    {
      "area": "Keybindings / command catalog / typing keyboard: engine-bound commands and categories in the shell's own command table",
      "why": "The map cites KeyBindings.cpp/.h and the keymap path only. The command catalog itself carries engine-bound entries the fork must delete or re-target: cmdShowPlayer (F7), cmdShowDrumKit (F11), Category::DrumKit, Category::VocalEditors, plus mouse-reference rows for the drum kit and pitch/align editors in KeyBindings.cpp, mirrored as View-menu items 408/410 and the F-key 'Most Recent' handlers. cmdToggleTypingKeyboard + TypingKeyboardMap.h (grids must bubble mapped keys) and toggleTypingKeyboard/sendTypingNote/releaseAllTypingNotes (StandaloneEditor.cpp:12198-12230) are the ONLY keyboard audition route into a hosted plugin once the built-in engines go, and KeyBindsWindow.cpp (495 lines) tabs by Category. Not mapped at all.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/KeyBindings.h (:22-100), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/KeyBindings.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/KeyBindsWindow.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/TypingKeyboardMap.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:9700-9760 command dispatch, :11817-11825 View menu, :12198-12230), C:/Users/jeffm/Documents/BaySickDAW/Plans & Specs/System Reference/Keyboard Shortcuts.md"
    },
    {
      "area": "Undo system: action inventory, family-keyed MixerState, structural undo snapshots, owner-key resolution, and the patched-JUCE undo machinery it depends on",
      "why": "The map says 'UndoActions.h (all seven action types) survive unchanged'. UndoActions.h defines FOURTEEN action classes (:51-560): PitchEditAction (:95) is Vocal-only and goes; MixerStateAction (:176) snapshots MixerState (PatternManager.h:487-520: layers/bass/drums level/mute/solo/pan, drumSlotLevel[kMaxDrumPages], audioRow arrays) - a family-keyed second mixer model the fork must retire or re-key; StructuralOpAction (:334) + UndoSnapshotStore.h back tab add/delete/engine swap/kit load with page-preset XML snapshot files under Documents/BaySickDAW/UndoSnapshots. ownerKeyForParamId (StandaloneEditor.cpp:12437-12475) hard-codes sfizz prefixes brd_/bgg_/bbb_ and a TabKind table that walks engine/namIr/pedals APVTSs; resolveOwnerPage (:13246-13253) maps lay/bass/drm/vox/inst/plug to TabType; rebuildHistoryLabels :12405; UndoHistoryWindow; ApvtsDirtyTracker.h is the per-engine dirty hook (wired by the nine-way wireEngineDirtyHook cast). Every one of these is shell code with engine keys and none is cited.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/UndoActions.h (:51-560), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/UndoSnapshotStore.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/UndoBracket.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/ApvtsDirtyTracker.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/UndoHistoryWindow.h/.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/PatternManager.h (:487-575), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:11990-12000, :12291-12475, :13227-13270), C:/Users/jeffm/Documents/BaySickDAW/Plans & Specs/System Reference/Undo History.md"
    },
    {
      "area": "Vendored JUCE is locally patched in nine module files (undo, lazy APVTS registration, relative file paths, ASIO, menus, windowing)",
      "why": "The map lists 'JUCE 8.0.12' as a keep and covers only its licence. The tree under juce/modules carries BaySickDAW patches that the shell depends on: juce_AudioProcessorValueTreeState.cpp/.h (undoOwnerTag, thread-local programmatic-write mark, redo-across-resurrection, lazy param registration), juce_ParameterAttachments.cpp:71 (gesture naming 'param:<id>' that UndoBracket.h and rebuildHistoryLabels rely on), juce_Windowing_windows.cpp:3371 (double-undo fix), juce_ValueTree.cpp:777, juce_File.cpp:176 (relative audioFilePath), juce_MenuBarModel.cpp:78, juce_PopupMenu.cpp:1492, juce_ASIO_windows.cpp:1523. A fork that pulls stock JUCE silently loses undo correctness, lazy APVTS, relative sample paths and the ASIO safety net. PluginProcessor.cpp:562 (apvts.undoOwnerTag = \"main\") is a patched-JUCE API. CLAUDE.md does not document the patches either.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/juce/modules/juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.cpp (:39-70, :227, :274, :331, :446, :470, :558, :643), C:/Users/jeffm/Documents/BaySickDAW/juce/modules/juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.h (:406-470), C:/Users/jeffm/Documents/BaySickDAW/juce/modules/juce_audio_processors/utilities/juce_ParameterAttachments.cpp (:71), C:/Users/jeffm/Documents/BaySickDAW/juce/modules/juce_gui_basics/native/juce_Windowing_windows.cpp (:3371), C:/Users/jeffm/Documents/BaySickDAW/juce/modules/juce_data_structures/values/juce_ValueTree.cpp (:777), C:/Users/jeffm/Documents/BaySickDAW/juce/modules/juce_core/files/juce_File.cpp (:176), C:/Users/jeffm/Documents/BaySickDAW/juce/modules/juce_gui_basics/menus/juce_MenuBarModel.cpp (:78), C:/Users/jeffm/Documents/BaySickDAW/juce/modules/juce_gui_basics/menus/juce_PopupMenu.cpp (:1492), C:/Users/jeffm/Documents/BaySickDAW/juce/modules/juce_audio_devices/native/juce_ASIO_windows.cpp (:1523)"
    },
    {
      "area": "Multithreaded render engine internals: flags/watchdog, thread pool lifetime + worker count, retirement GC, arena, idle-suspend, and the Mixer 'Multi-core Rendering' toggle",
      "why": "The map lists the dispatcher and task classes but not the engine's control surface: RenderEngineFlags.h (gMultiThreadedEngineEnabled hot-swap, kMaxWorkers=8, watchdogTimeoutMillis, kMaxStripChannels), BaySickThreadPool (constructed in the processor ctor, never recreated in prepareToPlay, hybrid spin/sleep), computeRenderWorkerCount (PluginProcessor.cpp:540-545), RetirementQueue.h (274 lines of deferred-destruction GC for engine/clip snapshots), ChannelBufferArena.h, SidechainPullHelper.h, Tasks/IdleSuspendFade.h (idle-suspend, tied to the sfizz isAuditionPending predicates the map says go), SendSourceRead.h, AlignedFloatArray.h. The user-facing toggle lives in the Mixer hamburger (StandaloneEditor.cpp:7737-7738, :17223-17230) and persists to settings.xml (StandaloneApp.cpp:480-520). A shell plan needs to know which of these are engine-agnostic (all but IdleSuspendFade's predicates) and where the switch is.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Engine/RenderEngineFlags.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Engine/BaySickThreadPool.h/.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/Engine/RetirementQueue.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Engine/ChannelBufferArena.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Engine/SidechainPullHelper.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Engine/RenderTask.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Engine/Tasks/IdleSuspendFade.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Engine/Tasks/SendSourceRead.h, C:/Users/jeffm/Documents/BaySickDAW/Source/PluginProcessor.cpp (:540-575), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:7737, :17223-17230), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneApp.cpp (:480-520)"
    },
    {
      "area": "Audio device init / ASIO failure path / app ownership and shutdown order (StandaloneApp)",
      "why": "Cited as StandaloneApp.cpp:762 and :839-953 plus 'AudioSettingsDialog :137-564'. Thin: the app object owns processor, playhead, device manager, AudioProcessorPlayer, advancer, window and a FileLogger (StandaloneApp.h:170-185), and the initialise() path has the ASIO-open failure diagnostic + no-device fallback (:813-1010, where buildFixedTopology only runs from prepareToPlay so a failed open leaves no buses), changeListenerCallback :608, MIDI device enable/omni merge :1165-1199, shutdown() teardown order, and the --shot harness branch at :665-667 that bypasses the whole device path. The AudioSettingsDialog class actually starts at StandaloneEditor.cpp:111, not :137, and contains the DrumTriggerVelocity combo (see settings entry).",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneApp.h (:141-235), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneApp.cpp (:608-660, :658-1010, :1165-1199, :1231-1290, :1392-1420), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:111-564)"
    },
    {
      "area": "Templates: save/apply code path (engine-welded), template directories, File > New from Template, default template setting",
      "why": "The map treats templates as a deletable content tree (Templates\\Factory) and a path rename. The shell CODE is untouched by the map: loadTemplate/applyTemplate (StandaloneEditor.cpp:8718-8900) parse <BaySickTemplate> with a v1 branch that calls closeDynamicTabs(LayersBassDrumsOnly), loadKitImpl on a <Kit path> relative to Kits/Factory, and spawnLayerTabFromTemplate / spawnBassTabFromTemplate per <Layer engine=...>/<Bass> entry; saveAsTemplate (:8940-8980 via UserFileSave); templatesDir/userTemplatesDir/factoryTemplatesDir (:8590-8600); the File > New from Template submenu walk (:11670-11730); ProjectManager default-template setting (ProjectManager.h:236-238, Options > General). Templates.md says a template carries every tab+engine, every rack AND both EQ banks - so 'what is a template' is an open design question for a shell with no engines, and the plan has no file to start from.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:8590-8600, :8718-8980, :11670-11730, :13502), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.h (:779-790), C:/Users/jeffm/Documents/BaySickDAW/Source/ProjectManager.h (:236-238), C:/Users/jeffm/Documents/BaySickDAW/Source/UserFileSave.h, C:/Users/jeffm/Documents/BaySickDAW/Plans & Specs/System Reference/Templates.md (:105-135)"
    },
    {
      "area": "Strip EQ removal depth in the graph/processor (not just EqWindowUI)",
      "why": "The map says the strip EQ dies, citing Standalone/EqWindowUI (5,244) and the slot table at PluginProcessor.h:2083-2089. StripEq is a node in the routing graph, not a UI folder: BaySickGraph.h has 46 StripEq/eq references (per-bus accessors :517-560, the post-rack chain position), BaySickGraph.cpp 52, PluginProcessor.cpp 29, PluginProcessor.h 11 (StripEq.h is included by both), EffectWindows.cpp 33, StandaloneEditor.cpp 23 (eqChannelId in the 'Pages:' rows, cited only as 'dies with the strip EQ'), EffectsPage.cpp/.h 5; DSP/StripEq.h pulls DSP/Kbs/ParametricEq.h + Kbs/EqMatch.h; Templates.md says templates carry both EQ banks. There is no file:line in the map for the graph-side surgery, so 'delete EQ' cannot be planned as a task.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/BaySickGraph.h (:8, :409-460, :517-560), C:/Users/jeffm/Documents/BaySickDAW/Source/BaySickGraph.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/PluginProcessor.h (:2083-2089 and the StripEq include), C:/Users/jeffm/Documents/BaySickDAW/Source/DSP/StripEq.h, C:/Users/jeffm/Documents/BaySickDAW/Source/DSP/Kbs/ParametricEq.h, C:/Users/jeffm/Documents/BaySickDAW/Source/DSP/Kbs/EqMatch.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/EffectWindows.cpp, C:/Users/jeffm/Documents/BaySickDAW/Plans & Specs/System Reference/EQ.md"
    },
    {
      "area": "Offline export dialog, loudness measurement, Master Analyzer, VersionCapture, and the DSP-folder survivors they need (incl. EffectParamMap for surviving rack automation)",
      "why": "The map names LoudnessReportWriter and MasterAnalyzerWindow with no lines and lists only AudioClipStreamer/PhaseVocoder/Mp3Writer/DSPBase as DSP survivors. Reality: ExportAudioDialog is a class inside StandaloneEditor.cpp:13585-14230 (not BuilderPage); BuilderPage.h:1431-1520 and .cpp:10146-10180 use DSP/LoudnessSpec.h, LufsMeterDSP, TruePeakMeter; BaySickGraph.cpp includes LufsMeterDSP, TruePeakMeter and PanLaw; BaySickGraph.h includes SpectrumFeed.h; BuilderPage.cpp includes BpmDetect.h; MasterAnalyzerWindow (995 lines) + VersionCapture (306, automatic per-playback capture gated by fsCaptureAudio/fsCaptureRetain at StandaloneEditor.cpp:2026-2029) + report re-read (:3632-3676) all survive. Also DSP/EffectParamMap (listed in the deleted bucket, 937 lines) is called by the SURVIVING rack-slot automation (EffectsPage.cpp:803-841, BuilderPage.cpp:10583-10584, StandaloneEditor.cpp:15812-15846) - it needs a VST3-only stub or the vst_ branch split before it can go. None of these survivors is in the map's DSP survivor list.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:2026-2029, :3632-3676, :13585-14230, :15812-15846), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/BuilderPage.h (:1431-1520), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/BuilderPage.cpp (:10146-10180, :10583-10584), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/EffectsPage.cpp (:795-845), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/MasterAnalyzerWindow.h/.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/LoudnessReportWriter.h/.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/VersionCapture.h/.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/DSP/LoudnessSpec.h, C:/Users/jeffm/Documents/BaySickDAW/Source/DSP/LufsMeterDSP.h, C:/Users/jeffm/Documents/BaySickDAW/Source/DSP/TruePeakMeter.h, C:/Users/jeffm/Documents/BaySickDAW/Source/DSP/SpectrumFeed.h, C:/Users/jeffm/Documents/BaySickDAW/Source/DSP/PanLaw.h, C:/Users/jeffm/Documents/BaySickDAW/Source/DSP/BpmDetect.h, C:/Users/jeffm/Documents/BaySickDAW/Source/DSP/EffectParamMap.h/.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/BaySickGraph.cpp (includes at top)"
    },
    {
      "area": "Theming / LookAndFeel and the shared widget set (which SharedUI pieces are effect-panel-only was never determined)",
      "why": "The map says 'LAF, VKnob' in SharedUI. SharedUI.h holds the VC palette (:7), BaySickLAF (:118, installed as the default LAF at StandaloneEditor.cpp:1031 and by the shot harness), five more LAFs (ModulationLAF :526, TimeLAF :755, DynamicsLAF :809, HarmonicLAF :848, ColoredSectionLAF :1493) and the widget set (:544-1600: LED buttons, VUMeter, GRMeter, GateGRMeter, DBFSMeter, LufsReadoutBox, ChickenHeadSelector, DualLabelToggle, SnapSlider, VKnobAutomation, GlobalAutoRightClick). Four engine-folder LAFs (BaySickSolstice/Synth/Bass/PlayerLAF.h) go. Which of the SharedUI LAFs/meters exist only for the deleted effect panels vs. the surviving mixer/master analyzer (DBFSMeter is reused by MasterAnalyzerWindow) is undetermined, so SharedUI.cpp/.h (4,809+1,667) cannot be trimmed from the map.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/SharedUI.h (:7-70, :118-180, :526-560, :755-870, :1061-1600), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/SharedUI.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:1031, :2154), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/MasterAnalyzerWindow.h (:6), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/STANDALONE_UI_CHANGES.md"
    },
    {
      "area": "Manuals window + F1 + manual folder layout + shot-harness entry point",
      "why": "Cited as ManualsWindow.cpp:44-50 for the path (the resolver is actually :46-53) and generate-manual.py. Not mapped: Help menu items (StandaloneEditor.cpp:11855-11875 - Help Index 601, Key Binds 603, View Projects MidiMap 605 -> MidiMapView.h, About 602), showManualsWindow :10049, cmdShowManuals :9711; the Manuals/ folder actually shipped (index.html, manual.html, manual-1/2/3.html, three PDFs, assets/ with generate-manual.py + generate-manual-1.py + generate-manual-2.py, search.js, atlas.css, manual.css, nav.css, bsd-docs.json, two rename/renumber maps, src-m2, src-m3, shots-staging, 137 figures); the --shot command-line branch (StandaloneApp.cpp:665-667 -> shots::run, ShotHarness.h) that runs a headless processor world; WebView2Loader.dll staging. A fork must decide whether the manual pipeline ships at all, and the map has no inventory to cut from.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/ManualsWindow.h/.cpp (:46-53), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:9711, :10049-10070, :11855-11875, :12030-12060), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/ShotHarness.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneApp.cpp (:665-667), C:/Users/jeffm/Documents/BaySickDAW/Source/MidiLearn/MidiMapView.h, C:/Users/jeffm/Documents/BaySickDAW/Manuals/ (top level + assets/), C:/Users/jeffm/Documents/BaySickDAW/Plans & Specs/System Reference/Manual Pipeline.md"
    },
    {
      "area": "Project lifecycle shell: ProjectBrowserWindow, autosave/backups, first-launch housekeeping, sample import, UserFileSave, HeavyOperationOverlay",
      "why": "Named only (ProjectBrowserWindow, VersionCapture, AppPaths.h in one row with no lines). The fork keeps all of it and needs the entry points: ProjectBrowserWindow (TableListBox over ProjectManager::listProjects, rename/duplicate/delete/show-in-explorer, launched from doFileOpen); ProjectManager autosave timer (mAutosaveSec 900, writeBackup :205, restoreBackup :149, listBackups :128 -> Backups\\ and Backups\\Unsaved), runFirstLaunchHousekeeping :411 (Roaming->Documents migration at :426, shortcut creation, and the Core Library prompt at :466), importSample/duplicateSample :275-282; UserFileSave.h (the single user-named-write path used by presets, templates, rack presets); HeavyOperationOverlay (progress + cancel during load/freeze, must use the software renderer per CLAUDE.md); ClipDropDiag.h (Debug-only trace, stays); PluginEditor.cpp/.h (30+25, the unshipped VST stub the legacy target compiles).",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/ProjectBrowserWindow.h/.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/ProjectManager.h (:49-320), C:/Users/jeffm/Documents/BaySickDAW/Source/ProjectManager.cpp (:128-230, :411-520), C:/Users/jeffm/Documents/BaySickDAW/Source/UserFileSave.h, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/HeavyOperationOverlay.h/.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/ClipDropDiag.h, C:/Users/jeffm/Documents/BaySickDAW/Source/PluginEditor.h/.cpp, C:/Users/jeffm/Documents/BaySickDAW/Plans & Specs/System Reference/Projects and Saving.md"
    },
    {
      "area": "Core Library fetcher: the call sites and settings it leaves behind (the header is cited, its consumers are not)",
      "why": "Deleting CoreLibraryInstaller.h (cited) also has to cut: ProjectManager.cpp:8 include, offerCoreContentDownload (:476-520) and its two callers (runFirstLaunchHousekeeping :466 on first launch; Options > 'Get Sound Content...' id 505 -> StandaloneEditor.cpp:12033), the skipCoreContentPrompt setting (ProjectManager.h:131-135, .cpp:705/735), SampleLibrary.h:41-72 (getCoreLibraryDir under %LOCALAPPDATA%, the 'Sample Library.lnk' shortcut creation, the library:/mysamples: path tokens at :137-138 - the map cites those tokens at ProjectManager.h:39-43 instead), BuilderPage.cpp:10's SampleLibrary include for the import start dir, the NSI lines that describe the library as surviving uninstall (.nsi:41-44, :165, :421), and the INDEX.md lead paragraph that says the Core Library is installed separately. juce_cryptography (CMakeLists.txt:238-240) is the only cited consumer.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/CoreLibraryInstaller.h, C:/Users/jeffm/Documents/BaySickDAW/Source/ProjectManager.cpp (:8, :411-520), C:/Users/jeffm/Documents/BaySickDAW/Source/ProjectManager.h (:131-135), C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:12030-12035), C:/Users/jeffm/Documents/BaySickDAW/Source/SampleLibrary.h (:25-72, :129-170), C:/Users/jeffm/Documents/BaySickDAW/Source/SampleLibrary.cpp, C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/BuilderPage.cpp (:10), C:/Users/jeffm/Documents/BaySickDAW/Installer/BaySickDAW-Tester.nsi (:41-44, :165, :290-305, :421), C:/Users/jeffm/Documents/BaySickDAW/Plans & Specs/System Reference/Sample Library.md"
    },
    {
      "area": "Freeze: automatic CPU-threshold freeze path and its kind table",
      "why": "Freeze is otherwise well mapped, but the AUTOMATIC freeze (5 Hz poll against fsAutoFreezeCpu, arms a pending freeze after a hold, picks 'the first eligible tab' from a hard-coded kFreezable[] table of all eight TabKinds incl. Rusty, reportAutomaticFreezeFailure, and the fsInstrumentFreeze gate at :7878) is not cited anywhere. It is shell code the fork keeps and its kind table must collapse to Plugins.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Source/Standalone/StandaloneEditor.cpp (:7878, :15383, :19407-19420, :19530-19600, :19663), C:/Users/jeffm/Documents/BaySickDAW/Plans & Specs/System Reference/Freeze and Export.md"
    },
    {
      "area": "Installer: no updater exists; built installer exes checked into the tree",
      "why": "The task list asks about an updater. A repo-wide grep finds none (no Updater/checkForUpdate anywhere in Source, Installer, make_installer.bat or CMake) - the map should say so explicitly so the fork plan does not assume one. Installer/ also holds 17 built BaySickDAW-1.2.0-*-Tester-Setup.exe files that a fork would inherit as repo weight. Otherwise the NSI is mapped adequately; ScopedPluginDllDirectory.h (sibling-DLL search fix, 91 lines) is missing from the 13-file Hosting list.",
      "files": "C:/Users/jeffm/Documents/BaySickDAW/Installer/ (directory listing), C:/Users/jeffm/Documents/BaySickDAW/Installer/BaySickDAW-Tester.nsi (:52, :108, :221-338, :379-428), C:/Users/jeffm/Documents/BaySickDAW/make_installer.bat, C:/Users/jeffm/Documents/BaySickDAW/Source/Hosting/ScopedPluginDllDirectory.h"
    }
  ],
  "unverified_claims": [
    "\u00a78/\u00a71: 'UndoActions.h (all seven action types) survive unchanged' - wrong count and no line evidence; the file defines 14 UndoableAction classes (UndoActions.h:51-560), one of which (PitchEditAction :95) is Vocal-only and one (MixerStateAction :176) wraps the family-keyed MixerState.",
    "\u00a71: 'AudioSettingsDialog in StandaloneEditor.cpp:137-564' - the class starts at :111; :137 is a comment inside it.",
    "\u00a79: 'Manual at run time <exe>\\Manuals\\manual.html ... ManualsWindow.cpp:44-50' - :44 is a closing brace; the resolver manualsIndexFile() is :46-53.",
    "\u00a71 shell table rows with no file:line at all: LoudnessReportWriter, MasterAnalyzerWindow, ProjectBrowserWindow, VersionCapture, UndoHistoryWindow, KeyBindsWindow, AudioFileRecorder, MidiRecorder, SafeAudioReader.h, SafeAudioFormats.h, MpglibAudioFormat.h, TempoMapRead.h, TsMapRead.h, WindowChrome, BaySickTitleBar, HeavyOperationOverlay, PatternColorPicker, DSPBase.h ('the rack slot base type'), 'one app-wide UndoManager on StandaloneEditor', 'ProjectManager (settings.xml)' (path cited at ProjectManager.cpp:28 only; the key set is never enumerated).",
    "\u00a72/\u00a710.4: the DSP survivor list (AudioClipStreamer, PhaseVocoder, Mp3Writer, DSPBase, maybe LibraryPitchShifters) has no include-level evidence and is incomplete - shell files also include DSP/LoudnessSpec.h, LufsMeterDSP.h, TruePeakMeter.h, SpectrumFeed.h, PanLaw.h, BpmDetect.h, StripEq.h, EffectParamMap.h and EngineSidechainHelper.h (verified via grep of #include lines in BaySickGraph.*, PluginProcessor.*, BuilderPage.*, EffectsPage.*, EffectWindows.h, MasterAnalyzerWindow.h, VersionCapture.h).",
    "\u00a72: 'DSP/EffectParamMap (937)' placed in the deleted bucket - but it is called by the surviving rack automation path (EffectsPage.cpp:803-841, BuilderPage.cpp:10583-10584, StandaloneEditor.cpp:15812-15846); no evidence was given that those call sites are built-in-only.",
    "\u00a79: 'JUCE 8.0.12' as a keep - no file:line, and it omits that juce/modules is locally patched in nine files (see missing list); the licence discussion assumes a stock tree.",
    "\u00a79: 'fontaudio - CMake claims OFL 1.1 + MIT + CC BY 4.0 (:56-61) but only the MIT text is in-tree' - the in-tree-text half has no path evidence.",
    "\u00a79: 'no GUID anywhere' (installer) and 'no NOTICES file is staged beside the exe anywhere in CMake' - negative grep claims with no cited search scope.",
    "\u00a79: 'Manual pipeline - 24 of 43 figure groups are engine-free ... 88 of 90 automated, Main frame.png + Hosted Plugin.png hand captures' - only the mixer/eq/editor-menus groups carry ShotHarness.cpp lines; the 43-group list and the 88/90 split are uncited (137 PNGs in Manuals/figures verified).",
    "\u00a72: 'Vendored libs ... LunaSVG' - no line and no statement of which engine consumes it.",
    "\u00a72: 'Tests 1,982 Tools/EqTests/main.cpp (BaySickEqTests target)' - target cited as CMakeLists.txt:913-923 but the 1,982 count and 'strip-EQ only' scope have no evidence.",
    "\u00a74: 'Input gain / trim: none (repo grep: only tape_inputGain inside SaturationDSP)' - a grep result, no file:line for the search.",
    "\u00a76: 'refinement changes createIdentifierString()' and \u00a712 'VST3_Usage_Guidelines.pdf requires a signed agreement', \u00a77 'input-channel index vs picker list may diverge (StandaloneApp.cpp:932-953)' - all self-flagged as inferred; still unverified.",
    "\u00a71: 'Shared UI assets: Resources/Filmstrips/ (9 PNGs, SharedUI.cpp:10), libs/fontaudio, Assets/BaySickDAWLogo.png, control_tab.png' - only the Filmstrips half has a line.",
    "\u00a710.12 'EngineRig::recreateEngine / retryDeadPluginTab (EngineRig.cpp:417-534) ... unread' - confirmed recreateEngine starts at :417; the dependency question remains open.",
    "Adjacent stale name (same class of error as \u00a710.3's EngineConnection): CLAUDE.md's 'the app's ONE VibeTooltip' - the only tooltip class is BaySickTooltip (SharedUI.h:179); the map repeats neither name but a fork plan copying CLAUDE.md would.",
    "\u00a71/\u00a78: 'MIDI Learn ... MidiLearnUI.h' - MidiMapView.h (the Help > View Projects MidiMap window) is in the MidiLearn folder line count (1,496) but never named, so its survival is unstated."
  ]
}
```
