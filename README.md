# Vibesynth 🎛️

A polyphonic wavetable synthesiser VST3/AU plugin built with JUCE.

## Features

### Oscillators (per voice)
| Mode | Description |
|---|---|
| **Wavetable** | 8-frame morphable wavetable (like Serum). Saw frames by default — replace `mFrames` with custom tables. |
| **Classic** | Sine / Saw / Square / Triangle with pure phase accumulator |
| **FM** | 2-operator FM — carrier + sine modulator, adjustable ratio (0.5–8×) and index (0–10) |
| **Noise** | White noise generator, blendable with any other osc mode |

Wavetable mode also features **unison** (1–7 voices) with adjustable spread and **fine detune**.

### Filter
State-variable filter (Zavalishin topology) with four modes: **LP / HP / BP / Notch**. Cutoff, resonance (Q), and envelope modulation amount are all automatable.

### Envelopes
Two independent exponential ADSR envelopes:
- **Amp** — controls voice amplitude
- **Filter** — modulates cutoff by a configurable semitone amount

### LFO
Synth-level LFO routed to pitch. Shapes: Sine, Triangle, Square, Sample & Hold. Rate 0.01–20 Hz, depth controls pitch deviation in semitones.

### Sequencer
- 8 / 16 / 32 steps, synced to DAW transport BPM
- Per-step: **note** (drag up/down in grid), **active toggle** (click)
- Per-step probability and gate length in code (extendable via right-click context menu)
- Note range: C3–C6 (48–84)
- Fires real MIDI into the synth engine — works with external MIDI too

### Effects chain (serial, in order)
| Effect | Parameters |
|---|---|
| **Distortion** | Drive (1–20), Tone (500Hz–18kHz LP), Wet |
| **Chorus** | Rate (0.01–10 Hz), Depth (0.1–20 ms), Wet — stereo quadrature LFO |
| **Delay** | Time (10–2000 ms), Feedback, Wet — stereo ping-pong |
| **Reverb** | Room size, Damping, Wet — JUCE Schroeder network |

Each effect has an on/off toggle and is automatable from the DAW.

### Polyphony
8 voices with JUCE's built-in voice stealing. All DSP runs on the real-time audio thread with no allocations.

---

## Building

### Prerequisites
- **CMake ≥ 3.22**
- **C++17 compiler** — Clang/MSVC/GCC
- **Git** (CMake fetches JUCE automatically)
- **macOS**: Xcode command-line tools (`xcode-select --install`)
- **Windows**: Visual Studio 2022 with "Desktop development with C++"
- **Linux**: `sudo apt install build-essential libasound2-dev libfreetype6-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev`

### Build (all platforms)
```bash
git clone <this-repo>
cd Vibesynth
chmod +x build.sh
./build.sh
```

Or manually:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j8
```

### Install
**macOS VST3**: Copy `build/Vibesynth_artefacts/VST3/Vibesynth.vst3` to `~/Library/Audio/Plug-Ins/VST3/`

**Windows VST3**: Copy to `C:\Program Files\Common Files\VST3\`

**Linux VST3**: Copy to `~/.vst3/`

---

## Project structure
```
Vibesynth/
├── CMakeLists.txt
├── build.sh
└── Source/
    ├── PluginProcessor.h/cpp   — AudioProcessor, APVTS, FX routing
    ├── PluginEditor.h/cpp      — Full GUI (LookAndFeel, knobs, sequencer grid, FX strips)
    ├── SynthVoice.h/cpp        — Polyphonic voice (osc + filter + env + LFO)
    ├── SynthSound.h/cpp        — JUCE SynthesiserSound stub
    ├── WavetableOscillator.h/cpp — Morphable wavetable osc with unison
    ├── FMOscillator.h/cpp      — 2-op FM osc
    ├── SynthFilter.h/cpp       — SVF (LP/HP/BP/Notch)
    ├── AdsrEnvelope.h/cpp      — Exponential ADSR
    ├── LFO.h/cpp               — Sine/Tri/Sq/S&H LFO
    ├── Sequencer.h/cpp         — 32-step sequencer, DAW-synced
    ├── FXChain.h               — FX base class + all FX declarations
    ├── ReverbFX.cpp
    ├── DelayFX.cpp
    ├── ChorusFX.cpp
    └── DistortionFX.cpp
```

---

## Extending

**Add a wavetable**: Fill `mFrames[n]` in `WavetableOscillator` with your single-cycle waveform data (2048 samples, normalised ±1). Load from a `.wav` using JUCE's `AudioFormatManager`.

**Add an FM operator**: Extend `FMOscillator` to 4-op by chaining `FMOscillator` instances — operator B's output becomes operator A's modulation source.

**Add sequencer per-step controls**: Right-click in `SequencerGrid::mouseDown` and show a popup with velocity, gate, and probability sliders for that step.

**Preset browser**: Serialize `apvts.copyState()` to XML, write to `File::getSpecialLocation(userApplicationDataDirectory)`. Load via `apvts.replaceState()`.
