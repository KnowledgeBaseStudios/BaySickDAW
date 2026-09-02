# BaySickDAW

A free standalone Windows digital audio workstation built around two ideas: making the first steps into music production as approachable as possible, and putting professional-grade tools in the hands of anyone who wants them — no licensing fees, no subscriptions, no gated upgrades. Whether you've never made a track before or you've been priced out of the tools you've always wanted to learn on, BaySickDAW is built for you.

Made by **KnowledgeBase Studios**. Built with JUCE 7 in C++.

> ⚠️ **Pre-release.** This repo is the active source for BaySickDAW. The app is in a multi-batch QA hardening cycle (see `Plans & Specs/Main Plan.md`); the V1 release is not yet shipped.

---

## What it is

BaySickDAW is a single-window music-production app — load engines onto a tab strip, write parts in a unified piano roll, mix on a node-graph mixer with sends + auxes, and lay it all out on a pattern + audio-clip arrangement grid. The UX is designed so a first-time music maker can get from "blank project" to "playing a finished idea" without learning plugin-host conventions, signal-routing graphs, or the rest of the conventions DAWs typically expect their users to already understand.

**Standalone-only.** There is no VST3 / AU build target. A legacy `juce_add_plugin` target still sits in `CMakeLists.txt` for historical reasons but is not shipped.

---

## Engines

Ten engine families ship with the app, each with its own editor page:

| Engine | Type | What it does |
|---|---|---|
| **BaySickSolstice** | Additive synth | Inspired by FL BaySickSolstice — 2048-pt IFFT wavetable, dual SVF filter, X/Y/Z modulation pad |
| **BaySickPlayer** | Sample player | Drag-and-drop folders / SFZ / single files; M/S width, treble shelf, voice modes |
| **BaySickSynth** | Multi-waveform synth | 10 waveforms incl. Bell FM, mono/poly voice modes |
| **BaySickBass** | Bass synth | Bass-tuned variant of BaySickSynth |
| **BaySickNAM/IR** | Amp modeler + IR | Neural Amp Modeler with cabinet impulse responses, A/B slot toggle |
| **BaySickVocals** | Vocal processor | Pitch + formant alignment sub-pages |
| **BaySickPedals** | Guitar pedal rack | 4×2 grid of swappable pedal effects with drag-to-reorder |
| **BaySickGuitars** | Sampled guitar | sfizz-driven, ARIA-format kit artwork + native control surface |
| **BaySickBasses** | Sampled bass | sfizz-driven, ARIA-format kit artwork |
| **BaySickRustyDrums** | Sampled drum kit | sfizz-driven, multi-mic capture (Big Rusty Drums library) |

---

## System pages

- **Mixer** — Master strip + 5 buses (Layers / Bass / Drums / FX / Clips) + per-engine inserts + user-spawned aux strips. Per-strip level / pan / mute / solo / polarity / M-S width / FX bypass / arm. Dynamic routing graph with cycle detection, post-fader sends, color-coded cable overlay.
- **Effects rack** — 6 hot-swappable slots, 12 DSP modules (EQ8 M/S, Saturation, Chorus, Compressor, Delay, Flanger, Overdrive, Phaser, Reverb, Transient Shaper, Tape, plus an upcoming Limiter). Per-slot bypass, drag-to-reorder.
- **EQ8 M/S** — 8-band parametric EQ with split mid/side processing, 8 filter types, solo/mute per band, A/B compare banks.
- **Builder** — Pattern + audio-clip + automation arrangement grid. 32 free-form rows, full undo/redo, click/drag/marquee tools, per-block resize and loop.
- **Piano Roll** — Unified roll for every engine + the drum kit. Scale snap, ghost notes, control lane for velocity / panning / pitch bend, multi-pattern ControlPoint automation.
- **Tab ribbon** — 10 fixed top-level slots (Mixer / Effects / Builder / Clips / Vox / Inst / Layers / Bass / Drums / Piano Roll). Variable-width slot layout that auto-shrinks for long engine names and wraps to two lines for very long custom labels.

---

## Audio engine

- Multi-threaded render graph (production, default ON, fully working under both Release and Debug configurations as of QA-Md).
- Lock-free render-task pipeline with worker pool, watchdog timeouts, and runtime-toggleable diagnostic counters.
- ASIO support via JUCE's audio device manager; per-machine settings persist to `Documents/BaySickDAW/audio_settings.xml`.
- All DSP runs on the audio thread without allocations; APVTS-synced modules pair `isIdentity()` short-circuits with ValueTree-listener-driven dirty-flag updates.

---

## Building

### Prerequisites

- **Windows 10 / 11**
- **Visual Studio 2022** with the *Desktop development with C++* workload (provides MSVC + Windows SDK)
- **CMake ≥ 3.22**
- **Git** with submodule support (vendored libs are vendored; no submodule fetch needed)

### Build

From the repo root, run:

```cmd
do_build.bat
```

This produces both Release and Debug configurations in one pass and writes `build_log.txt` with the per-config exit codes.

- Release exe: `build/BaySickDAWStandalone_artefacts/Release/BaySickDAW.exe`
- Debug exe: `build/BaySickDAWStandalone_artefacts/Debug/BaySickDAW.exe` (window title shows ` [DEBUG]`)

The build is incremental — header changes are picked up automatically by MSBuild.

---

## Project structure

```
BaySickDAW/
├── CMakeLists.txt
├── do_build.bat                      — build entry point (Release + Debug)
├── Source/                           — application source (~150 files)
│   ├── PluginProcessor.h/.cpp        — top-level audio processor + APVTS
│   ├── VibeGraph.h/.cpp              — node-graph audio routing + InsertNode
│   ├── PatternManager.h/.cpp         — pattern + arrangement state
│   ├── EffectRack.h/.cpp             — 6-slot effect rack
│   ├── DSP/                          — 12 effect modules + phase vocoder + audio streamer
│   ├── Engine/                       — render-graph dispatcher + thread pool + tasks
│   ├── BaySickSolstice/                     — BaySickSolstice additive synth
│   ├── VibePlayer/                   — BaySickPlayer (sample player; rename pending QA-PlayerRename)
│   ├── BaySickSynth/                 — multi-waveform synth
│   ├── BaySickBass/                  — bass synth
│   ├── BaySickNAMIR/                 — Neural Amp Modeler + IR cabinet
│   ├── BaySickVocal/                 — vocal processor cluster
│   ├── BaySickPedals/                — guitar pedal rack
│   ├── Inst/                         — sfizz-driven Inst page (Guitars + Basses)
│   ├── Vox/                          — Vox page lifecycle
│   └── Standalone/                   — main window, ribbon tabs, page editors, shared UI
├── juce/                             — vendored JUCE 7
├── libs/                             — vendored third-party libs (sfizz, NAM, eigen, lunasvg, etc.)
├── Assets/                           — icons, fonts, packaged graphics
├── Kits/                             — sample / sfizz instrument libraries
├── Presets/                          — factory + user presets (My Presets/ subfolders are gitignored)
└── Plans & Specs/                    — active development plan, batch records, running notes
```

---

## Status

The app is in pre-release development. The current focus is **Phase 1 of the post-Batch-10 QA cycle** — a sequence of small targeted bug-fix and polish batches (QA-A through QA-N) followed by a Phase 6 cleanup audit, a Phase 7 docs / installer / auto-update build, and a release-candidate verification pass before V1 ships.

The full plan and per-batch breakdown live in `Plans & Specs/Main Plan.md`.

---

## License

All rights reserved — KnowledgeBase Studios.
