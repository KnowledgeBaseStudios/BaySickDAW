cmake_minimum_required(VERSION 3.22)
project(BaySickDAW VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Local JUCE (no network / no DNS hit) ─────────────────────────────────────
# JUCE 7.0.12 must be extracted to: C:\Users\jeffm\Documents\Vibesynth\juce\
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/juce")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${JUCE_DIR}\n"
        "Download JUCE 7.0.12 from https://github.com/juce-framework/JUCE/releases/tag/7.0.12\n"
        "and extract it to that folder.")
endif()
add_subdirectory("${JUCE_DIR}" juce_build)

# ── Binary data (instrument samples) ─────────────────────────────────────────
file(GLOB_RECURSE SAMPLE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Samples/*.wav")
if(SAMPLE_FILES)
    juce_add_binary_data(VibeSynthSamples SOURCES ${SAMPLE_FILES})
else()
    message(STATUS "No samples in Assets/Samples/ -- instrument mode silent until samples installed")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/stub_sample.txt" "no samples")
    juce_add_binary_data(VibeSynthSamples SOURCES "${CMAKE_CURRENT_BINARY_DIR}/stub_sample.txt")
endif()

# ── Branding assets (logo / splash) ──────────────────────────────────────────
# Unique HEADER_NAME + NAMESPACE so this doesn't collide with the existing
# VibeSynthSamples default BinaryData.h/namespace.
juce_add_binary_data(BaySickDAWAssets
    HEADER_NAME "BaySickAssets.h"
    NAMESPACE   BaySickAssets
    SOURCES     "${CMAKE_CURRENT_SOURCE_DIR}/Assets/BaySickDAWLogo.png"
)

# ── Shared compile definitions ────────────────────────────────────────────────
set(VIBESYNTH_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_WIN32_USE_DIRECTWRITE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    # 2026-04-28: MP3 patents expired April 2017; safe to enable.  Allows
    # .mp3 drops on Builder + Clips empty-state to actually decode (default
    # JUCE flag was 0 due to pre-2017 patent concerns that no longer apply).
    JUCE_USE_MP3AUDIOFORMAT=1
)

# ── ASIO (Steinberg) ─────────────────────────────────────────────────────────
# License-restricted SDK; not bundled with JUCE or this repo.  Enable by
# downloading from https://www.steinberg.net/developers/ and extracting the
# `common/` folder (containing iasiodrv.h, asio.h, asiosys.h, etc.) into
# libs/asiosdk/ at the repo root.  CMake auto-detects + enables when present.
set(ASIO_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/asiosdk")
set(VIBESYNTH_ASIO_INCLUDES "")
if(EXISTS "${ASIO_SDK_DIR}/common/iasiodrv.h")
    message(STATUS "ASIO SDK found at ${ASIO_SDK_DIR}/common -> ASIO support ENABLED")
    list(APPEND VIBESYNTH_DEFS JUCE_ASIO=1)
    set(VIBESYNTH_ASIO_INCLUDES "${ASIO_SDK_DIR}/common")
else()
    message(STATUS "ASIO SDK not found at ${ASIO_SDK_DIR}/common -> ASIO support DISABLED")
    message(STATUS "  Audio device picker will only show Windows Audio (WASAPI / DirectSound).")
    message(STATUS "  See https://www.steinberg.net/developers/ to download the SDK.")
endif()

# ── Shared DSP sources (used by both VST and Standalone) ─────────────────────
set(VIBESYNTH_DSP_SOURCES
    Source/PluginProcessor.cpp
    Source/SampleLibrary.cpp
    Source/VibeGraph.cpp
    Source/SynthVoice.cpp
    Source/SynthSound.cpp
    Source/WavetableOscillator.cpp
    Source/FMOscillator.cpp
    Source/SynthFilter.cpp
    Source/AdsrEnvelope.cpp
    Source/LFO.cpp
    Source/BassSynth.cpp
    Source/PatternManager.cpp
    Source/ProjectManager.cpp
    Source/MidiRecorder.cpp
    Source/AudioFileRecorder.cpp
    # Phase 2A — Harmless (Additive Synth) — Alpha + Beta + Final sessions
    Source/Harmless/SpectralModules.cpp
    Source/Harmless/HarmonicEngine.cpp
    Source/Harmless/AdditiveVoice.cpp
    Source/Harmless/HarmlessSynth.cpp
    Source/Harmless/HarmlessProcessor.cpp
    Source/Harmless/HarmlessEditor.cpp
    Source/Harmless/HarmlessFilterRow.cpp
    Source/Harmless/HarmlessRoutingMatrix.cpp
    Source/Harmless/HarmlessXYZPad.cpp
    Source/Harmless/HarmlessModEditor.cpp
    Source/Harmless/HarmlessModRegistry.cpp
    Source/Harmless/VisualizerScreen.cpp
    # Phase 2B — VibePlayer (Disk Streaming Sampler) — DSP + Processor only
    Source/VibePlayer/VibePlayerDSP.cpp
    Source/VibePlayer/VibePlayerProcessor.cpp
    # Phase 2C — BaySickSynth (5-tab subtractive synth)
    Source/BaySickSynth/BaySickSynthVoice.cpp
    Source/BaySickSynth/BaySickSynthDSP.cpp
    Source/BaySickSynth/BaySickSynthProcessor.cpp
    Source/BaySickSynth/BaySickVisualizerScreen.cpp
    Source/BaySickSynth/BssEditorComponents.cpp
    Source/BaySickSynth/BaySickSynthEditor.cpp
    # Phase 2D — BaySickBass (bass engine using BaySickSynthDSP with bsb_ prefix)
    Source/BaySickBass/BaySickBassProcessor.cpp
    Source/BaySickBass/BaySickBassEditor.cpp
    # 2026-04-25: BaySickDrumsProcessor removed (legacy 16-slot kit superseded
    # by per-drum-tab dynamic engines).  DrumSynth.cpp also gone.
)

set(VIBESYNTH_LINK_LIBS
    VibeSynthSamples
    juce::juce_audio_utils
    juce::juce_dsp
)

# ── BaySickNAM (Neural Amp Modeler) Core static library ─────────────────────
# Vendored at libs/NeuralAmpModelerCore/.  Upstream CMakeLists doesn't expose
# a library target — it just builds their CLI tools — so we collect the
# source files into a static lib here.  Eigen + nlohmann/json are bundled
# inside the vendored repo's Dependencies/ folder.  C++20 required by NAM
# (their CMakeLists hard-codes it); our consumer targets stay C++17 — only
# this static lib needs C++20 internally.
set(NAM_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/NeuralAmpModelerCore")
if(EXISTS "${NAM_CORE_DIR}/NAM/dsp.cpp")
    set(NAM_CORE_SOURCES
        "${NAM_CORE_DIR}/NAM/activations.cpp"
        "${NAM_CORE_DIR}/NAM/container.cpp"
        "${NAM_CORE_DIR}/NAM/conv1d.cpp"
        "${NAM_CORE_DIR}/NAM/convnet.cpp"
        "${NAM_CORE_DIR}/NAM/dsp.cpp"
        "${NAM_CORE_DIR}/NAM/get_dsp.cpp"
        "${NAM_CORE_DIR}/NAM/lstm.cpp"
        "${NAM_CORE_DIR}/NAM/ring_buffer.cpp"
        "${NAM_CORE_DIR}/NAM/util.cpp"
        "${NAM_CORE_DIR}/NAM/wavenet/model.cpp"
        "${NAM_CORE_DIR}/NAM/wavenet/a2_fast.cpp"
        "${NAM_CORE_DIR}/NAM/wavenet/slimmable.cpp"
    )
    add_library(BaySickNAMCore STATIC ${NAM_CORE_SOURCES})
    # SYSTEM here flips MSVC into "external header" treatment for these dirs,
    # which combined with the PUBLIC /wd suppressions below keeps consumers'
    # /W4 builds from drowning in Eigen template noise when they include the
    # NAM public headers (NAM/get_dsp.h, NAM/dsp.h pull in Eigen/Dense).
    target_include_directories(BaySickNAMCore SYSTEM PUBLIC
        "${NAM_CORE_DIR}"
        "${NAM_CORE_DIR}/NAM"
        "${NAM_CORE_DIR}/Dependencies/eigen"
        "${NAM_CORE_DIR}/Dependencies/nlohmann"
    )
    target_compile_features(BaySickNAMCore PUBLIC cxx_std_20)
    target_compile_definitions(BaySickNAMCore PUBLIC NAM_ENABLE_A2_FAST)
    if(MSVC)
        target_compile_definitions(BaySickNAMCore PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
        # /W3 is internal-only (NAM core compilation).  The /wd suppressions go
        # PUBLIC so anything that includes Eigen via NAM's headers (including
        # BaySickNAMIRProcessor.cpp) inherits them and keeps a clean /W4.
        target_compile_options(BaySickNAMCore PRIVATE /W3)
        target_compile_options(BaySickNAMCore PUBLIC  /wd4127 /wd4244 /wd4267 /wd4305)
    endif()
    set(BAYSICK_HAS_NAM_CORE TRUE)
else()
    message(STATUS "NeuralAmpModelerCore not found at ${NAM_CORE_DIR}/NAM/dsp.cpp"
                   " — BaySickNAM/IR engine will be disabled.")
    set(BAYSICK_HAS_NAM_CORE FALSE)
endif()

# ── Legacy plugin target (NOT shipped; BaySickDAW is standalone-only) ────────
# Kept as build scaffolding only. Do not distribute. See
# memory/feedback_standalone_only_no_vst.md for rationale.
juce_add_plugin(BaySickDAW
    COMPANY_NAME             "KnowledgeBase Studios"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Kbst
    PLUGIN_CODE              Bsdw
    FORMATS                  VST3
    PRODUCT_NAME             "BaySickDAW"
    VST3_CATEGORIES          "Instrument|Synth"
)

juce_generate_juce_header(BaySickDAW)

target_sources(BaySickDAW PRIVATE
    ${VIBESYNTH_DSP_SOURCES}
    Source/PluginEditor.cpp
)

target_compile_definitions(BaySickDAW
    PUBLIC  ${VIBESYNTH_DEFS} VIBESYNTH_VST=1
    PRIVATE JUCE_MODULE_AVAILABLE_juce_audio_plugin_client=1
)

target_include_directories(BaySickDAW PRIVATE Source Source/Harmless Source/VibePlayer Source/BaySickSynth Source/BaySickBass)

target_link_libraries(BaySickDAW
    PRIVATE ${VIBESYNTH_LINK_LIBS}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ── Standalone Application ────────────────────────────────────────────────────
juce_add_gui_app(BaySickDAWStandalone
    PRODUCT_NAME "BaySickDAW"
    BUNDLE_ID    "com.knowledgebasestudios.baysickdaw"
    ICON_BIG     "${CMAKE_CURRENT_SOURCE_DIR}/Assets/BaySickDAWLogo.png"
)

juce_generate_juce_header(BaySickDAWStandalone)

target_sources(BaySickDAWStandalone PRIVATE
    ${VIBESYNTH_DSP_SOURCES}
    # Shared standalone UI
    Source/Standalone/SharedUI.cpp
    # Main shell
    Source/Standalone/StandaloneApp.cpp
    Source/Standalone/StandaloneEditor.cpp
    Source/Standalone/ProjectBrowserWindow.cpp
    Source/Standalone/UndoHistoryWindow.cpp
    # Phase 1 — new shell components
    Source/Standalone/GlobalTransportBar.cpp
    Source/Standalone/RibbonTabBar.cpp
    Source/Standalone/InstrumentPage.cpp
    # Phase 2 — DSP modules
    Source/DSP/AudioClipStreamer.cpp
    Source/DSP/PhaseVocoder.cpp
    Source/DSP/CompressorDSP.cpp
    Source/DSP/ReverbDSP.cpp
    Source/DSP/ChorusDSP.cpp
    Source/DSP/DelayDSP.cpp
    Source/DSP/EQ8DSP.cpp
    Source/DSP/EQ8MsDSP.cpp
    Source/DSP/EqLinearPhaseProcessor.cpp
    Source/DSP/SaturationDSP.cpp
    Source/DSP/FlangerDSP.cpp
    Source/DSP/OverdriveDSP.cpp
    Source/DSP/PhaserDSP.cpp
    Source/DSP/TransientShaperDSP.cpp
    Source/DSP/TapeDSP.cpp
    Source/DSP/LimiterDSP.cpp
    # Phase 2 — EffectRack + Effects Page
    Source/EffectRack.cpp
    Source/Standalone/SlotComponent.cpp
    Source/Standalone/EffectsPage.cpp
    Source/Standalone/EffectEditorPanels.cpp
    # Pages
    Source/Standalone/LayersPage.cpp
    Source/Standalone/BassPage.cpp
    Source/Standalone/DrumPage.cpp
    # G-7 — unified Page Preset save / load
    Source/Standalone/PagePresetIO.cpp
    Source/Standalone/DrumKitGrid.cpp
    Source/Standalone/PianoRoll.cpp
    Source/Standalone/PianoRollPage.cpp
    Source/Standalone/BuilderPage.cpp
    # Phase G-1 — BaySickNAM/IR engine (Neural Amp Modeler + IR cabinet)
    Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp
    Source/BaySickNAMIR/BaySickNAMIREditor.cpp
    # Phase G-2 — Clips page (engine picker hosts BaySickPlayer or
    # BaySickNAM/IR; spawned via Builder drop or empty-state drop, never
    # via the ribbon dropdown)
    Source/Clips/ClipsPage.cpp
    # Phase G-4 — Vox + Inst pages (same dual-engine A/B-style architecture
    # as Clips, spawned by the Mixer page's "Add Vox/Inst Strip" buttons).
    Source/Vox/VoxPage.cpp
    Source/Inst/InstPage.cpp
    # Phase 4D — Event Editor
    Source/Standalone/EventEditor.cpp
    # Phase 1 — Mixer Page
    Source/Standalone/MixerTrackStrip.cpp
    Source/Standalone/MixerPage.cpp
    # Keymap framework (Phase A — 2026-04-26)
    Source/Standalone/KeyBindings.cpp
    Source/Standalone/KeyBindsWindow.cpp
    # Pattern colour picker (Phase F-1 — 2026-04-26)
    Source/Standalone/PatternColorPicker.cpp
    # Phase 2B — VibePlayer Editor (standalone UI only — includes SharedUI.h)
    Source/VibePlayer/VibePlayerEditor.cpp
    # 2026-04-25: legacy DrumsPage.cpp + BaySickDrumsEditor.cpp removed.
)

target_compile_definitions(BaySickDAWStandalone
    PUBLIC ${VIBESYNTH_DEFS} VIBESYNTH_STANDALONE=1
)

target_include_directories(BaySickDAWStandalone PRIVATE Source Source/Standalone Source/Harmless Source/VibePlayer Source/BaySickSynth Source/BaySickBass Source/BaySickNAMIR Source/Clips Source/Vox Source/Inst
    # Rubber Band Library (Phase 4B time-stretch) — headers only; link rubberband.lib to enable DSP
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/rubberband"
    # ASIO SDK include path (empty when SDK absent — harmless to leave in the list).
    ${VIBESYNTH_ASIO_INCLUDES}
)

target_link_libraries(BaySickDAWStandalone
    PRIVATE ${VIBESYNTH_LINK_LIBS}
            BaySickDAWAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Phase G-1 — link BaySickNAMCore (Neural Amp Modeler) when vendored.
if(BAYSICK_HAS_NAM_CORE)
    target_link_libraries(BaySickDAWStandalone PRIVATE BaySickNAMCore)
    target_compile_definitions(BaySickDAWStandalone PRIVATE BAYSICK_HAS_NAM_CORE=1)
endif()

# ── Generate PDBs in Release builds so minidumps symbolicate ─────────────────
# /Zi = full debug info at compile time; /DEBUG = emit PDB during link;
# /OPT:REF /OPT:ICF keep the size/speed benefits of Release (/DEBUG alone
# disables them). PDB lands next to the EXE as BaySickDAW.pdb.
if(MSVC)
    target_compile_options(BaySickDAWStandalone PRIVATE
        $<$<CONFIG:Release>:/Zi>)
    target_link_options(BaySickDAWStandalone PRIVATE
        $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
endif()
