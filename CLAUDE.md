# BaySickDAW — Claude Working Guide

## Plans & Specs Location (READ FIRST)

All planning documents live in `Plans & Specs/` at the repo root:
- `Plans & Specs/Main Plan.md` — master sequencing of work, every batch and phase.
- `Plans & Specs/Carry-Forward Reference.md` — frozen architectural snapshot from 2026-05-07.
- `Plans & Specs/Implemented Work Log.md` — running ledger of QA-era execution (2026-05-07 onward).
- `Plans & Specs/Previously Implemented.md` — historical record of pre-QA build work.
- `Plans & Specs/Future State.md` — post-V1 roadmap.
- `Plans & Specs/Batch Plans/<silly-name>.md` — per-batch implementation plans.

Per the three-doc system in Main Plan §0, every per-batch session starts by
reading Main Plan + Carry-Forward + Implemented Work Log.  See Main Plan §0
Rule 3 for the convention on how findings discovered during execution get
routed at batch close.

---

## Project Overview
JUCE 7 C++ music production app (formerly Vibesynth, then VibeDAW). **Standalone Windows app only** — no VST/plugin version planned; a legacy `juce_add_plugin` target still exists in CMake but is not shipped. Future platform plan: tablet "DJ Party" variant, still not a VST.
**App name:** BaySickDAW by KnowledgeBase Studios
**Target audience:** people who have never made music before.
**User-facing engine names:** Harmless, BaySickPlayer (sample player — internal source is still `VibePlayer*`; class / file renames deferred), BaySickSynth, BaySickBass. Drums are now per-tab engine instances (Phase D dynamic-drum architecture, 2026-04-25) — each Drums tab owns one BaySickPlayer or BaySickSynth instance; the legacy monolithic `BaySickDrums` engine was deleted.

**Owner:** Jeff — professional FL Studio user. Technically capable. **Jeff runs builds himself** — never try to run do_build.bat in bash (MSVC env not available). Just tell him to run it.

---

## Build System

- **Build command:** Run `do_build.bat` from `C:\Users\jeffm\Documents\BaySickDAW\`. Builds BOTH Release and Debug per QA-0a (2026-05-07).
- **Release exe:** `build\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe` - the shipping binary, used for music production.
- **Debug exe:** `build\BaySickDAWStandalone_artefacts\Debug\BaySickDAW.exe` - the diagnostic binary, used for verifying fixes. Window title shows `[DEBUG]` suffix. Same embedded icon (JUCE+VS multi-config gotcha; differentiation via window title only).
- **Build dir:** `C:\Users\jeffm\Documents\BaySickDAW\build\`
- **Build log:** `build_log.txt` at repo root. Two exit codes - `RELEASE_EXIT_CODE` and `DEBUG_EXIT_CODE`. Either non-zero = that config failed.
- **Header dependencies:** handled automatically by MSBuild. No manual `.obj` deletion after header edits - just re-run `do_build.bat`.
- **Standing rule (verifying Claude fixes):** run the Debug exe FIRST. Any `jassert` that fires shows a Windows dialog with file path + line + condition - screenshot to share. Then re-run in Release as the actual user test. Debug runs slower; audio that glitches in Debug under heavy load may be fine in Release. Always confirm in Release before declaring a real performance regression.
- **Don't run both simultaneously.** ASIO opens audio devices exclusively (second instance gets no audio). Both exes share `Documents\BaySickDAW\settings.xml` + `audio_settings.xml` - changes in one are seen by the other on next start.
- **MT engine in Debug:** the multi-threaded render path is a no-op under Debug (real Debug-only bug; investigation owned by **QA-Md** in `Plans & Specs/Main Plan.md` §5). DSP meter readings in Debug always reflect single-thread cost. Use Release for any MT vs serial verification.

---

## Source Layout

```
Source/
  VibesynthConstants.h      — NUM_OSC_LAYERS=4, MAX_DRUM_ROWS=16, MAX_DRUM_SOUNDS=46, etc.
  PluginProcessor.h/.cpp    — VibeSynthProcessor, APVTS layout, processBlock
  PatternManager.h/.cpp     — PatternManager, Pattern, ArrangementBlock, MixerState
  EffectRack.h/.cpp         — EffectRack (6 slots, hot-swap DSP)
  VibeGraph.h/.cpp          — AudioProcessorGraph (bus nodes, instrument nodes, full mix routing)
  AdsrEnvelope.h / FMOscillator.h / SynthFilter.h / LFO.h / WavetableOscillator.h — shared DSP primitives
  // 2026-04-25: DrumSynth.h/.cpp DELETED with the legacy drum sweep.
  PluginEditor.h            — VST3 plugin editor stub
  Harmless/
    HarmlessProcessor.h/.cpp      — APVTS layout (~100 params), updateFromApvts, auditionNote
    HarmlessSynth.h/.cpp          — Voice dispatcher + output phaser + strum direction
    AdditiveVoice.h/.cpp          — Voice DSP (dual SVF filter, tremolo/vibrato LFO, phase init)
    HarmonicEngine.h/.cpp         — 2048-pt IFFT wavetable (double-buffered atomic publish)
    HarmlessLAF.h                 — Ring-glow knobs, 3-pass bloom, micro-toggles, palette constants
    HarmlessEditor.h/.cpp         — 960×620 5-panel proportional layout
    HarmlessWaveformButton.h      — Cyclable waveform icon
    HarmlessFilterRow.h/.cpp      — Type/ENV/FREQ/RES/KB filter strip
    HarmlessRoutingMatrix.h/.cpp  — 6 vertical sliders + 6 LED toggles
    HarmlessXYZPad.h/.cpp         — 2D mod pad + X/Y/Z knobs
    HarmlessModEditor.h/.cpp      — Curve editor (internal HarmlessCurvePoint, 4 tabs)
  BaySickSynth/
    BaySickSynthProcessor.h/.cpp  — APVTS (bss_ prefix), mono MIDI preprocess, auditionNote
    BaySickSynthVoice.h/.cpp      — 10 waveforms incl. Bell FM, inline phases
    BaySickSynthDSP.h/.cpp        — Shared DSP used by synth + bass
    BaySickSynthEditor.h/.cpp     — 5-tab editor with null-guarded layouts
    BaySickVisualizerScreen.h/.cpp — Parameterized LED color constructor
    BssEditorComponents.h/.cpp    — Shared BssLedRadio + BssFilterXYPad
  BaySickBass/
    BaySickBassProcessor.h/.cpp   — Wraps BaySickSynthDSP with bsb_ prefix + bass-tuned defaults
    BaySickBassEditor.h/.cpp      — Mirrors synth editor with green LEDs
    BaySickBassVisualizerScreen.h/.cpp — Header-only subclass (0xFF33FF88)
    BaySickBassLAF.h              — kGreen accent palette
  VibePlayer/
    VibePlayerProcessor.h/.cpp    — APVTS, updateFromApvts, auditionNote
    VibePlayerDSP.h/.cpp          — VibeSampleManager (loadFolder/loadSFZ/loadSingleFile/
                                    normalizeRootNotes), voice rendering with M/S width + treble shelf
    VibePlayerEditor.h/.cpp       — 4-column FL Keys layout (480×400, kKnobSz=55)
    VibePlayerLAF.h               — Knob palette
  // 2026-04-25: BaySickDrums/ DELETED.  Drums now use the dynamic-drum model
  // (DrumPage.h/.cpp below) — each drum tab owns one BaySickPlayer or
  // BaySickSynth engine instance.  Folder remains empty.
  DSP/
    EQ8DSP.h/.cpp           — 8-band parametric EQ DSP (8 filter types, solo/mute, compare banks)
    EQ8MsDSP.h/.cpp         — M/S wrapper around two EQ8DSP instances
    SaturationDSP.h/.cpp    — Saturation / tape
    ChorusDSP.h/.cpp        — Chorus
    CompressorDSP.h/.cpp    — Compressor (with GR meter output)
    DelayDSP.h/.cpp         — Delay (ping-pong, sync)
    FlangerDSP.h/.cpp       — Flanger
    OverdriveDSP.h/.cpp     — Overdrive
    PhaserDSP.h/.cpp        — Phaser
    ReverbDSP.h/.cpp        — Reverb (8-line FDN)
    TransientShaperDSP.h/.cpp — Transient shaper
    PhaseVocoder.h/.cpp     — Laroche-Dolson, FFT 2048, 4× overlap (hop 512), Hann window
    AudioClipStreamer.h/.cpp — SPSC ring buffer (4s), background TimeSliceThread
  Standalone/
    StandaloneApp.h/.cpp    — VibesynthStandaloneApp, VibeSynthWindow, StandalonePlayHead
    StandaloneEditor.h/.cpp — StandaloneEditor (main window, menu bar, tab management)
    SharedUI.h/.cpp         — VibeLAF, VKnob, DBFSMeter, VUMeter, ParametricEQDisplay,
                               SnapSlider, PianoRollContainer, BasicSequenceGrid, etc.
    GlobalTransportBar.h/.cpp — Combined transport + pattern dropdown + tab ribbon bar
    RibbonTabBar.h/.cpp     — Chrome-style tab bar with dropdown arrows + badges
    LayersPage.h/.cpp       — LayersPage (one Layers tab; engine combo locks after first pick;
                              right-click → per-layer context menu via LockableCombo subclass)
    BassPage.h/.cpp         — BassPage (mirrors LayersPage structure; LockableCombo + per-bass menu)
    DrumPage.h/.cpp         — DrumPage (D1.4 dynamic-drum model — one drum per tab; sound picker
                              popup + lock-after-pick + per-drum context menu; per-drum APVTS prefix
                              `drm_{N}_*`; drum InsertNode + mixer strip auto-created on engine pick)
    // 2026-04-25: legacy DrumsPage.h/.cpp DELETED.
    BuilderPage.h/.cpp      — BuilderPage: BrowserPanel + ArrangementGrid (piano-roll style)
    MixerPage.h/.cpp        — MixerPage (Master + Bus + instrument channel strips)
    MixerTrackStrip.h/.cpp  — MixerTrackStrip component
    EffectsPage.h/.cpp      — EffectsPage (Rack + EQ8 M/S sub-tabs)
    EffectEditorPanels.h/.cpp — Per-effect editor panels (one per DSP type)
    PianoRoll.h/.cpp        — PianoRollContainer, PianoRollGrid, PianoKeyboard, ControlLane
    InstrumentPage.h/.cpp   — InstrumentPage base (Phase 2)
    TrackSelectionManager.h — Track selection state shared across pages
    SlotComponent.h/.cpp    — EffectRack slot UI component
    SequencerPage.h         — SequencerPage stub
    EventEditor.h/.cpp      — Floating automation editor window (multi-instance)
    BuilderMenuBar.h/.cpp   — juce::MenuBarModel for Builder page
    PianoRollMenuBar.h/.cpp — Shared menu bar for all piano rolls (Layers/Bass/Drums)
    MetroPanel.h            — Metronome floating CallOutBox panel
    PageMenuBar.h/.cpp      — ≡ menu bar with tab slots + MID/SIDE + extra-right APIs
```

---

## APVTS Parameter IDs

### Global
- `masterGain` (Float 0-1)

### Drums M/S EQ (prefix `drums_mid_eq{b}_` / `drums_side_eq{b}_` where b=0..7)
- `freq` (Float 20-20000), `gain` (Float -18-18), `q` (Float 0.1-10), `type` (Int 0-7), `on` (Bool)

### Bass M/S EQ (prefix `bass_mid_eq{b}_` / `bass_side_eq{b}_`)
- Same as drums EQ above

### Layers M/S EQ (prefix `layers_mid_eq{b}_` / `layers_side_eq{b}_`)
- Same as drums EQ above

### EQ type codes: 0=Bell, 1=LP, 2=HP, 3=LowShelf, 4=HiShelf, 5=Notch, 6=BandPass, 7=AllPass
### EQ defaults: all 8 bands Bell type, 0 dB gain — flat curve with 8 moveable handles

### Mixer — per-strip lazy APVTS (5F-4a)
Every mixer strip has its own set of params, registered lazily the first time the strip is created. Prefix format varies by strip type:

| Strip type     | Prefix format                    | Params (suffixes)                                           |
|----------------|----------------------------------|-------------------------------------------------------------|
| Master         | `mixer_master`                   | `_level`, `_pan`, `_width`                                  |
| Bus (5 total)  | `mixer_{layers,bass,drums,fx,clipsbus}` | `_level`, `_pan`, `_mute`, `_solo`, `_polarity`, `_width` |
| Layer insert   | `mixer_layer_{0..7}`             | `_level`, `_pan`, `_mute`, `_solo`, `_polarity`, `_width`, `_bypass`, `_arm` |
| Bass insert    | `mixer_bass_{0..3}`              | same as Layer insert                                        |
| Drum insert    | `mixer_drum_{0..15}`             | same as Layer insert                                        |
| Audio insert   | `mixer_audio_{0..49}`            | same as Layer insert                                        |

Param ranges: `_level` Float `-60..+10` dB (default 0), `_pan` Float `-1..+1` (default 0), `_mute`/`_solo`/`_polarity`/`_bypass`/`_arm` Bool (default false), `_width` Float `0..2` (default 1.0 — M/S encode → scale side → decode).

Registration entry points (PluginProcessor):
- `ensureMixerBusAndMasterParams()` — bulk-registers master + 5 buses (called once at startup)
- `ensureMixerStripParams(prefix, kind)` — lazy per-insert; `kind ∈ {Master, Bus, Insert}`

### Per-Insert Audio Nodes (5F-4a)
Per-insert audio processing moved from `PluginProcessor`'s render loop into new `InsertNode` type in `VibeGraph`. Each insert owns: `EffectRack`, post-rack `EQ8MsDSP`, peakDb atomic, CompDelayLine. Process order: polarity flip → M/S width → rack (bypassable) → EQ → fader × mute × solo → peak push. Registered via `VibeGraph::ensureInsertNode(kind, index, displayName, apvtsPrefix)`. Kinds: `Layer`, `Bass`, `Drum`, `Audio`.

### Routing (5F-4b B1a)
Every mixer strip gains routing params registered lazily alongside the rest:

| Suffix                | Type  | Range        | Default            | Purpose |
|-----------------------|-------|--------------|--------------------|---------|
| `_sendTo`             | Int   | `0..999`     | natural parent bus | Main-out destination channel id |
| `_send{0..3}_to`      | Int   | `-1..999`    | `-1` (inactive)    | Additional send destination (or inactive) |
| `_send{0..3}_amount`  | Float | `-60..+6` dB | `0` dB             | Send level |
| `_send{0..3}_prepost` | Bool  | —            | `false` (post)     | Pre-fader vs post-fader tap |

Channel ids (see `MixerChannelIds` namespace in `Source/VibeGraph.h`):
- `0` = Output (terminal — only Master routes here)
- `1`–`6` = Layers / Bass / Drums / Master / FX / Clips buses
- `100..115` = Aux 0..15
- `200..215` = Layer insert 0..15
- `300..315` = Bass insert 0..15
- `400..449` = Audio insert 0..49
- `500..515` = Drum insert 0..15

Defaults preserve current behavior (Layer insert → Layers Bus, etc.). `RoutingGraph` (in `VibeGraph.h/.cpp`) provides `wouldCreateCycle(src, dst)` for UI pre-flight + `rebuildFromApvts()` for block-rate graph resolution with Kahn's topo sort + cycle drop. Audio path wiring lands in B1b; these params are registered and cycle-checked now but are no-ops in the audio domain until then.

---

## Completed Work (chronological)

### Sessions 1–5 (April 3–4) — Foundation
Basic playback, APVTS, VibeLAF, VUMeter, LayerSynthPanel, DualEnvelope, LFO display.

### Session 6 (April 5) — Piano Roll Full Overhaul
PianoNote, PianoKeyboard, PianoRollGrid (tools/selection/scale snap/ghost notes/ControlLane),
playback integration, 100-step undo + history panel, scale-aware chord generation.

### Session 7 (April 7) — DSP Primitives & Effects Rack
9 effect DSPs rewritten, EQ8DSP (8 bands, 8 types), EQ8MsDSP, EffectRack (6 slots, hot-swap).
All 9 EffectEditorPanels rebuilt.

### L&F Sprint (April 8) — Visual Overhaul L1-L7
- L1: EQ color scheme (violet→cyan gradient, dark grid)
- L2: DBFSMeter (gradient bar, red 0dB, peak hold)
- L3: VUMeter hardware recreation (cream plate, needle, spring physics, arc scale)
- L4: ModulationLAF (Chorus/Flanger/Phaser — glossy black/silver 1176 style)
- L5: DynamicsLAF (Compressor/Transient Shaper — cream LA-2A style)
- L6: TimeLAF (Delay/Reverb — Pultec Radio Gray style, jewel indicators)
- L7: HarmonicLAF (Saturation/Overdrive/Tape — Fairchild olive style)
- VU calibration menu (-18 to -14 dBFS), global, persisted to XML
- Visual fixes: compressor resize, TimeLAF text color, chicken head rendering

### L&F Visual Fixes + Combined Toolbar (April 8)
- Effect panel toggle widths fixed (labels fully visible)
- Output VKnob added to all effect panels (beside DBFS meter)
- Combined toolbar: Header + Transport + Ribbon = single 40px bar
- PlayModeCombo removed (SONG toggle is sufficient)
- Window title = "VibeDAW"
- RibbonTabBar rewritten: 6 permanent slots with dropdown arrows + badges
  - Mixer (no arrow), Effects▾② (Rack/EQ), Builder▾③ (Patterns/Audio Clips/Automation)
  - Layers▾[n], Bass▾[n], Drums▾② (Sounds/EQ)
- BuilderPage mixer strip removed (moved PlayMode to GlobalTransportBar)

### Phase 0 — Cleanup & Migration (April 10)
- Deleted: FXChain.h, AllFX.cpp, MasteringEngine, MasteringPage, SamplerEngine, Sequencer,
  old EQ6DSP, EQ6MsDSP
- Removed all L{i}_* APVTS params (~152) + layersComp_* (6)
- LayersPage + BassPage replaced with stubs ("Select engine to begin")
- processBlock cleaned: only piano roll scheduling + M/S EQ processing remains

### Phase 1 — Graph, Mixer, EQ/M/S Polish (April 10–11)
- **1A:** VibeGraph AudioProcessorGraph shim (infrastructure ready, not yet routing audio)
- **1D:** MixerPage + MixerTrackStrip (Master/Bus/Layer/Bass/Drum strips, faders, VU, M/S)
- **EQ polish:** ParametricEQDisplay — removed toggle buttons, added Reset All, bindMsDSP,
  fixed APVTS write-back bug that caused M/S bands to reset on each processBlock
- **External MID/SIDE pattern:** buttons as children of page, positioned in tab bar row,
  timer-based tab detection (mLastTabIndex) since TabbedComponent has no addChangeListener
- **EQ defaults:** all 8 bands Bell type, 0 dB (flat line) for all three buses
- **DrumsPage:** EQ tab renamed, EQ display fills full content area
- **LayersPage + BassPage:** EQ tab added with M/S, bound to mLayersEQDSP / mBassEQDSP
- **PluginProcessor:** mLayersEQDSP + mBassEQDSP + spectrum feeds added
- **Pattern Dropdown:** replaced ComboBox + Add button + hidden TextEditor with single
  [Pattern▾] TextButton → PopupMenu (pattern list with tick, New Pattern, Rename, Delete)
- **ArrangementGrid rewrite:** full piano-roll-style grid (same visual, same controls):
  - Draw/Select/Delete tools (P/E/D keys), marquee selection, drag/resize blocks
  - Ctrl+Z/Y undo/redo (full block snapshot), Ctrl+A/C/V/B, Delete, Shift+Arrows, Escape
  - 32 free-form rows (no pattern-to-row assignment), zoom via Ctrl+Scroll + toolbar buttons
  - Blocks colored by pattern index, 3D bevel gradient fill, ruler with bar numbers
  - Row labels editable via double-click
  - Right-click "Render to WAV" moved exclusively to BrowserPanel pattern right-click
  - PatternRowButton subclass (TextButton + onRightClick callback)
  - Toggle removed from BrowserPanel pattern buttons
  - trackRow field added to ArrangementBlock struct

---

### 5F-4a + 5F-4b — Mixer Strip Overhaul + Dynamic Routing (April 15–16, 2026)
- **5F-4a (6 batches):** Full mixer strip feature update + audio-path refactor. Per-insert audio moved from `PluginProcessor` render loop into new `InsertNode` type in `VibeGraph`. Lazy APVTS per strip (level/pan/mute/solo/polarity/width/bypass/arm). New per-strip controls: MixerLedButton (red mute / yellow solo / red arm / blue FX bypass), PolarityButton (Standard/Reverse text toggle), width rotary, per-strip dBFS tick marks beside the meter. FX Bypass LED ↔ Effects page button two-way synced via APVTS + `parameterChanged` listener driving `rack.setRackBypassed()`. Drum engine bumped 14 → 16 slots (BaySickDrumsProcessor::kNumSlots) with `processSlotsSeparately` for per-slot InsertNode routing.
- **5F-4b (7 batches):** Dynamic routing + cables + aux strips. `MixerChannelIds` namespace (ids 0..999: master/buses/aux/inserts). `RoutingGraph` with Kahn's topo sort + DFS cycle check. Per-strip `_sendTo` + 4 sends (`_sendN_to`/`_amount`/`_prepost`) in APVTS. `VibeGraph::mChannelAccum` per-channel input accumulators; `routeInsertOutput` lambda in PluginProcessor fans each InsertNode output via graph edges. Aux strips (`InsertKind::Aux`, `mAuxInserts`, "Add Mixer Strip" button in PageMenuBar) default to FX Bus. Green bezier CableOverlay with neon-green socket circles at strip bottoms. Main-out cable drag with cycle check + red flash. Per-strip "+" button spawns send cables (crosshair click-to-place). Right-click cable → CallOutBox with amount slider + delete button (info-only popup for main-out). Aux strip persistence via `restoreAuxStripsFromState` scanning APVTS tree. Pre-fader sends deferred (UI toggle omitted; audio treats all sends as post-fader).

### 5F-5 — Event Editor Layout Alignment (April 16, 2026)
Functional-only scope (existing layout preserved). Title label (top-left, shows param id, updates on block switch), tool button strip (D/P/E/I/S/Z single-letter buttons with shared radio group, 22px below grid), Delete button (red-tinted, clears lane to single 0.5 point, undoable), footer status bar (20px, live "Beat X.XX Value Y.YYY" via new `EEAutomationGrid::onHoverChanged` callback). Skipped ambiguous spec items (LED "ON", RANGE box, Link icon, 5-of-6 target-link icons, knob relocations, Viewport wrapping).

### 5F-6 — Piano Roll Layout Alignment (April 16, 2026)
Functional-only + visible scrollbars. `ControlLane::paint` header now reads `Control > Velocity` / `Control > Panning` / `Control > Pitch Bend`; lane also draws vertical bar/beat grid lines that exactly match the main canvas timing. New `PianoRollContainer::setContextLabel(juce::String)` + right-aligned `mContextLabel` on toolbar row showing `"{tabName} - {engineType}"` (e.g. `Layer 0 - Harmless`, `Bass 0 - (no engine)`, `Drums - VibePlayer`). Default `tabName` is synthesized per-page (`Layer {N}` / `Bass {N}` / `Drums`); ribbon rename propagates via StandaloneEditor's `onTabRenamed` to `LayersPage::setTabName` / `BassPage::setTabName` / `DrumsPage::setTabName` → `refreshPianoRollContextLabel()`. Visible H + V scrollbars wired via `juce::ScrollBar::Listener` — `mHScroll` drives `mBeatOff` (shared by grid + lane), `mVScroll` drives `mTopNote` (auto-hidden in fixed-range/drum mode). `pushScrollStateToBars()` called from `syncScrollState()` with `mPushingToBars` guard against feedback. `kScrollBarSz = 12 px`. Spec's "linked Viewport" requirement is satisfied functionally (both panels receive the same `mPPB`+`mBeatOff` on every change); no Viewport rewrite needed. Skipped: draggable lane divider, speaker-icon transport, window controls (N/A for tab-embedded panel).

### Phase D — Dynamic-Drum Architecture (April 24-25, 2026)

Restructured the entire drum subsystem.  Each drum tab is now a fully independent engine instance (BaySickPlayer or BaySickSynth) instead of one monolithic 16-slot processor.  Mirrors the LayersPage / BassPage pattern.  See blueprint `Files For Claude/vibedaw_blueprint.md` for the detailed batch-by-batch log; here's the architectural summary:

- **Data model (D1.1)**: `kMaxDrumPages = 16`, `Pattern::drumRolls[16]` per-drum piano-roll arrays, `<DrumPageRoll>` save tag, legacy `drumRoll` → `drumRolls[slotIndex]` migration on load.
- **Audio thread (D1.2)**: `mDrumEngines[16]` parallel to `mLayerEngines` / `mBassEngines`, per-drum MIDI dispatch in song + pattern modes, fast-path bypass via `mAnyDrumPageActive` atomic.
- **DrumPage class (D1.3)**: Mirror of LayersPage — Player / Piano Roll / Pre EQ8 M/S sub-tabs, per-drum APVTS prefix `drm_{N}_*`, drum InsertNode created on engine pick.
- **Cutover (D1.4)**: Default Drums tab is DrumPage (singular).  Drums dropdown ▾ uses `showInstanceDropdown` like Layers/Bass — instances + Pages + Rename / Delete / + Add New Drum.  Badge counts instances.  `closeTab` allows Drums.  All `dynamic_cast<DrumsPage*>` legacy branches removed.
- **Sound picker (D1.4-fix(a))**: Single `[Pick a sound ▾]` button on Player tab.  Popup mirrors legacy showSoundPicker — Sample (Browse / SFZ / Core Library walker filtered by `isDrumPack`) + Synth Patch (`+ New Patch (Blank)` + factory presets + `Save Current Patch As...`).  Engine swap-aware: subsequent picks tear down + recreate.
- **Save Patch As (D1.4-fix(b))**: BaySickSynth → apvts XML.  BaySickPlayer → apvts XML + `<Sample kind path>` reference (path uses `library:rel/path` for Core Library samples, absolute otherwise).  Layers / Bass have parallel save flow → `Documents/BaySickDAW/Presets/{EngineName}/My Presets/{name}.xml`.
- **Lock-after-pick + context menus (D1.4-fix(c))**: Picker button transforms to show current sound name with `[L]` prefix when locked; both clicks open the per-drum context menu.  Items: Lock / Polyphony / Copy / Paste / Duplicate / Choke Group ▸ (D3 placeholder) / MIDI Map ▸ (D1.5 placeholder) / MIDI Note ▸ (D1.5 placeholder) / Save Current Patch As / Delete (with prompts).  Layers / Bass parity via new `LockableCombo` subclass that intercepts post-lock clicks → routes to per-page context menu.  Polyphony toggle is engine-aware (BaySickSynth/Bass voiceMode 4-choice Poly↔Mono / BaySickPlayer voiceCap 1↔8 / Harmless n/a).  Tab name auto-rename on sound load + User Patch save prompt via `RibbonTabBar::onRenameInterceptRequested` callback.  Refuses to delete only-of-type via new `RibbonTabBar::isLastOfType(type)` helper.
- **Master output parity**: BaySickSynth + BaySickBass got new `_bss_outVol` / `_bsb_outVol` Float 0-1 default 0.8 (matches BaySickPlayer's `volume`).  Applied via `buffer.applyGain()` after `mSynth.renderNextBlock()`.  No editor knob yet.
- **Audio settings dialog path fix**: `AudioSettingsDialog::applySettings` now writes `audio_settings_pending.xml` as a sibling of `VibesynthStandaloneApp::getAudioSettingsFile()` (Documents path post-P4b migration), not hardcoded Roaming.  Closes the bug where Apply + Restart loop didn't switch the device.
- **Factory drum preset names**: TR-808/909/606 prefix removed from individual preset names (folder names like `TR-808/` stay as section labels in the picker).  `Tools/gen_factory_presets.py` updated; cleanup pass at script start removes leftover `TR-*.xml` files.

**Legacy DELETED (April 25)**: `Source/DrumSynth.h/.cpp`, `Source/Standalone/DrumsPage.h/.cpp`, `Source/BaySickDrums/BaySickDrumsProcessor.h/.cpp`, `Source/BaySickDrums/BaySickDrumsEditor.h/.cpp`, `Source/BaySickDrums/BaySickDrumsLAF.h`.  Plus all PluginProcessor / VibeGraph / StandaloneEditor / CMakeLists references stripped.  Old "Drums" type tabs in saved projects silently skip on load (notes already migrated to `drumRolls[slot]` by D1.1 path).

## Next Steps

**Current position (2026-05-08):** Post-Batch-10 QA cycle is the active program. Sequencing, scope, and per-batch routing are all owned by the new docs in `Plans & Specs/` — read those FIRST for any planning question (per the directive at the top of this file). Quick orientation:

- **Active phase:** Phase 1 of `Plans & Specs/Main Plan.md` after QA-0/QA-0a/QA-Inventory close. Next batch: **QA-Md** (MT Engine Debug-Build Investigation).
- **Full sequence + scope:** `Plans & Specs/Main Plan.md` §5 (per-batch entries) and §6 (sequencing arrow).
- **What's already shipped:** `Plans & Specs/Previously Implemented.md` (1089 entries, source-verified 2026-05-08) and `Plans & Specs/Implemented Work Log.md` (post-QA-0 work).
- **What's deferred / dropped / post-v1.0:** `Plans & Specs/Future State.md`.
- **Per-batch plan files:** `Plans & Specs/Batch Plans/<silly-name>.md` when a batch starts.

Pre-QA reference docs (`Files For Claude/Final Stretch Work.txt`, `Files For Claude/vibedaw_blueprint.md`, `C:/Users/jeffm/.claude/plans/lucky-discovering-tiger.md`) are now historical — every still-needed item from those docs has been triaged and routed (see `Plans & Specs/Main Plan.md` §9 fifth Forks entry, dated 2026-05-08).

---

## Key Technical Notes

### JUCE Gotchas
- `TabbedComponent` has NO `addChangeListener` — use timer polling `mLastTabIndex` for tab detection
- `juce::Path::addArc()` starts a NEW subpath — breaks filled paths. Use `lineTo` loop with cos/sin.
- `juce::LookAndFeel_V4::drawLabel()` override required to force text color when VKnob calls `label.setColour()`
- `juce::dsp::IIR::Filter<float>::coefficients` starts NULL — pointer copy only (`mFilter.coefficients = c`), NEVER deref
- `juce::dsp::FFT` not default-constructible — must be in constructor init list: `: mFFT(kFFTOrder)`
- Notch filter: `Coeffs::makeNotch(sr, freq, q)` — NOT `makeNotchFilter`
- `juce::Font::Font(size, style)` generates C4996 warnings — harmless, ignore
- `CharPointer_UTF8` + `String` concatenation: wrap in `juce::String(juce::CharPointer_UTF8("..."))` before `+`
- `juce::dsp::StateVariableTPTFilter<float>`: `resonance` parameter = Q factor directly. Q=0.001 with fc=20kHz gives -34dB at 1kHz — effectively silent. Butterworth Q=0.7071 is the correct "transparent" default.
- `juce::dsp::StateVariableTPTFilter` highpass at 20kHz BLOCKS all audible audio. If using filter type as "bypass", initialize as **lowpass** at 20kHz instead (transparent when fully open).
- `setSize()` triggers `resized()` BEFORE unique_ptr members are constructed when called in an editor constructor. Always null-guard: `if (mFoo) mFoo->setBounds(...)`.
- `constexpr const char*[]` out-of-class definition causes C++17 MSVC compile error. Define inline in the header only; do NOT repeat the definition in the .cpp.
- `addChildComponent()` adds a child WITHOUT making it visible (preserves setVisible(false)). `addAndMakeVisible()` unconditionally sets visible=true.
- `juce::PopupMenu` has no `isEmpty()` method — use `getNumItems() > 0`.
- LCG noise generator `x * 1103515245.0 + 12345.0` overflows double in ~35 samples. Wrap with `std::fmod(..., 4294967296.0)` every iteration.

### Harmless-specific
- Internal `HarmlessCurvePoint { float time; float value; int curveType; }` is defined inside `HarmlessModEditor` — deliberately NOT shared with `PatternManager::ControlPoint`. Harmless modulation is per-note 0-1 phase; PatternManager automation is tick-based song position. Different domains.
- `HarmlessLAF::kBipolar` property on a Slider makes the ring-glow knob render from 12 o'clock outward (left or right). Set via `slider.getProperties().set(HarmlessLAF::kBipolar, "true")`.
- `HarmlessProcessor` has `oeq_mix` referenced in editor but not in `createLayout` — attachment is commented out; add the param when wiring output EQ for real.

### Drum sample root note
- `VibeSampleManager::detectRootNote()` parses filename numbers as MIDI notes. `Kick_01.wav` → rootNote=1 → 30× pitch. Drum engines must call `normalizeRootNotes(60)` after every `loadFolder()`/`loadSFZ()` since BaySickDrums always sends MIDI note 60.
- `loadSingleFile()` already initializes rootNote=60.

### Engine audition pattern
- All 4 engine processors (BaySickSynth, BaySickBass, Harmless, VibePlayer) have `auditionNote(int midiNote)` + `std::atomic<int> mAuditionNote { -1 }`.
- `processBlock` opens with `int n = mAuditionNote.exchange(-1); if (n >= 0) { ...noteOff-any, noteOn n... }`.
- Page audition callbacks must cascade through all engine types: try BaySickSynth first, then Harmless, then VibePlayer (order doesn't matter, all silent on cast failure).

### Constructor Order / resized() Safety
- Never call `resized()` during construction before all members it touches are initialized
- Guard with null checks: `if (mFoo) mFoo->setBounds(...)`

### APVTS Binding Pattern
```cpp
// Push UI → APVTS:
auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id));
if (p) p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(value));
// Read from APVTS:
if (auto* ap = apvts.getRawParameterValue(id)) return ap->load();
```

### CPU Safeguarding (standing rule)
Every DSP update function MUST guard numeric setters with value-change comparisons.
Only call the setter if the new value differs from the current DSP state.
processBlock fires hundreds of times/sec — unconditional recalculation wastes CPU.

### SpectrumFeed (seqlock audio→UI)
- Audio thread: `feed.push(src, n)` — increments seq to odd, copies, increments to even
- UI timer: `feed.poll(buf, count)` — reads seq before/after copy; returns false if write was in-progress (frame dropped, fine for visualizer)
- Seqlock is wait-free on audio thread, correct under C++ memory model

### ArrangementGrid Layout Constants
- kRowH=40, kRulerH=18, kLabelW=120, kNumRows=32, kResizeZone=8
- mPPBar (public): pixels per bar (zoom state), default 80
- Undo: full snapshot of blocks + row names per entry, max 100 entries

### External MID/SIDE Button Pattern
```cpp
// Position after tab bar (in resized()):
auto& bar = mTabs->getTabbedButtonBar();
int afterEQ = mTabs->getX() + 4;
if (auto* btn = bar.getTabButton(eqTabIndex))
    afterEQ = mTabs->getX() + btn->getRight() + 6;
int barY = mTabs->getY();
mEQMidBtn ->setBounds(afterEQ,      barY + 2, 46, 24);
mEQSideBtn->setBounds(afterEQ + 50, barY + 2, 50, 24);
// Timer-based tab detection:
int cur = mTabs->getCurrentTabIndex();
if (cur != mLastTabIndex) { mLastTabIndex = cur; resized(); }
```

### UI Changes Reference
All deliberate standalone UI changes are documented in:
**`Source/Standalone/STANDALONE_UI_CHANGES.md`**
Read this file before modifying any of the listed components.

### DSP Quality Pass Reference (Phase 5F-9)
All approved DSP upgrades for the 12 effect modules are spec'd in:
**`Files For Claude/DSP Review/_APPROVED_CHANGES.md`**
Covers 12 modules (Chorus, Compressor, Delay, Flanger, Limiter (net-new), Overdrive, Phaser, Reverb, Saturation, Tape, Transient Shaper, EQ8). Includes per-module change list with implementation notes, CPU budget, ordering, and deferred UI tasks. The source prompts Jeff provided are at `Files For Claude/DSP Review/*.txt` (one per module). Read `_APPROVED_CHANGES.md` before touching any DSP file listed there.

Notes:
- **Limiter UI** — `Limiter.txt` has the full 3-zone layout spec (scrolling waveform, skeuomorphic knobs, #00FFF2 cyan GR / #FF9100 orange sat, monospace digital font). Reference this when building `EffectEditorPanels::LimiterPanel`.
- **EQ band count** — stays at 8 (`kNumBands = 8`). Review suggested 7; we keep our existing 8.
- **Dynamic EQ** is full feature (DSP + UI), not DSP-only.
