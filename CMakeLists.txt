cmake_minimum_required(VERSION 3.22)
project(BaySickDAW VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── MSVC runtime (must match across JUCE + vendored libs) ────────────────────
# JUCE compiles with /MD (dynamic CRT) by default.  sfizz's CMakeLists picks
# /MT (static CRT) by default, which causes LNK2038 mismatches at link time.
# Force /MD across every target (including vendored sub-projects) so the
# runtime is consistent.  CMP0091 NEW makes CMAKE_MSVC_RUNTIME_LIBRARY the
# canonical control instead of the legacy per-target compile flags.  Must be
# set BEFORE any add_subdirectory or add_library call.
if(MSVC)
    cmake_policy(SET CMP0091 NEW)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
endif()

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
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/BaySickDAWLogo.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/big_rusty_drums.svg"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/big_rusty_drums.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/control_tab.png"
)

# ── Plugin-sandbox helper architecture suffix (QA-ModelShell TS6, BLU-302) ────
# An MSVC generator is single-platform per build tree, so the 32-bit helper is a
# SEPARATE configure into its own build dir with -A Win32.  This derives the
# name suffix from whichever platform the current tree is configured for, so one
# source tree produces BaySickPluginHost64.exe and BaySickPluginHost32.exe
# without either build knowing about the other.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(BAYSICK_HELPER_ARCH_SUFFIX "32")
else()
    set(BAYSICK_HELPER_ARCH_SUFFIX "64")
endif()
message(STATUS "Plugin sandbox helper will build as BaySickPluginHost${BAYSICK_HELPER_ARCH_SUFFIX}.exe")

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
    # QA-0a (2026-05-07): silence two AAX-format paramID asserts in JUCE's
    # validateParameter (paramID-too-long, paramID-trimmed-duplicate).
    # BaySickDAW is standalone-only -- no AAX target -- so these checks are
    # irrelevant.  JUCE's own comment recommends this define as the silence
    # mechanism (juce_AudioProcessor.cpp lines 469 + 480).  Without this,
    # Debug builds break hundreds of times during cold start as our long /
    # similar-prefix mixer-strip paramIDs trip the AAX trim check.
    JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING=1
    # QA-ModelShell TS6 (2026-07-29): VST3 plugin hosting.  This flag is the
    # ONLY lever -- juce_PluginFormatDefs.h derives JUCE_INTERNAL_HAS_VST3 from
    # it and #errors if the internal name is set directly.  Turning it on
    # compiles bodies that are already in our include list but currently expand
    # to nothing: juce_VST3PluginFormat.cpp, juce_KnownPluginList.cpp and
    # juce_PluginDirectoryScanner.cpp, plus the vendored VST3 SDK under
    # juce_audio_processors_headless/format_types/VST3_SDK (MIT).
    #
    # No module change is needed and none was ever needed: juce_audio_utils
    # depends on juce_audio_processors, which depends on
    # juce_audio_processors_headless -- the headless module is the DEPENDENCY
    # of the full one, not an alternative to it.  We have always built the full
    # module (AudioProcessorValueTreeState lives in it), so hosted-plugin
    # EDITORS -- VST3PluginWindow wrapping the plugin's IPlugView -- come with
    # it.  That foreign HWND is why our workspace windows are native child
    # peers (see WorkspaceWindow.h).
    #
    # JUCE_PLUGINHOST_VST stays OFF deliberately: JUCE's VST2 path #includes
    # Steinberg SDK headers that JUCE does not ship and that are absent here,
    # and the VST2 SDK has been unlicensable since Oct 2018.  Future State
    # CL-303 carries the full finding -- do not flip it without reading that.
    JUCE_PLUGINHOST_VST3=1
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
    Source/EngineRig.cpp
    # QA-ModelShell TS6 (2026-07-29): hosted-plugin scanning + the added list.
    # Model-side alongside EngineRig -- project load re-instantiates saved
    # plugin slots with no editor guaranteed to exist.
    Source/Hosting/PluginManager.cpp
    Source/Hosting/HostedPlugin.cpp
    Source/Hosting/HostedPluginEffect.cpp
    # QA-ModelShell TS6 (BLU-302): host end of the plugin sandbox bridge.
    Source/Hosting/SandboxedPluginClient.cpp
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
    # Multi-threaded render engine (Phase 1 scaffolding, 2026-05-06) — flag is
    # constexpr false; these compile but the dispatcher branch is elided in
    # processBlock until per-strip integration ships in Batches 3-8.
    Source/Engine/VibeThreadPool.cpp
    Source/Engine/RenderGraphDispatcher.cpp
    # Batch 3 (2026-05-06): EngineInsertTask wraps Layer/Bass/Drum engine
    # dispatch + insert chain in one RenderTask subclass.
    Source/Engine/Tasks/EngineInsertTask.cpp
    # Batch 4 (2026-05-06): Vox + Inst live-input strip wrappers.
    Source/Engine/Tasks/VoxStripTask.cpp
    Source/Engine/Tasks/InstStripTask.cpp
    # 2026-05-07 (QA-0): per-row composite -- arrangement-clip decode +
    # clip-engine MIDI trigger -- in one task per audio row.  Replaces
    # the old ClipPageTask + AudioInsertTask which conflicted at
    # audioInsert(N) under MT (most-recent-wins silenced one of them;
    # DSP-12 regression).
    Source/Engine/Tasks/CompositeAudioInsertTask.cpp
    # Batch 6 (2026-05-06): BaySickRustyDrums producer + 13 inserts.
    Source/Engine/Tasks/RustyDrumsProducerTask.cpp
    Source/Engine/Tasks/RustyInsertTask.cpp
    # Batch 7 (2026-05-06): passive accumulator strips (Aux + 11 buses).
    Source/Engine/Tasks/PassiveStripTask.cpp
    # Batch 8 (2026-05-06): MasterTask (graph sink) + dispatcher pump.
    Source/Engine/Tasks/MasterTask.cpp
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
        # 2026-05-06: enable AVX2 codegen for NAM core.  Eigen's matrix-vector
        # products + WaveNet inference inner loops auto-vectorize to 256-bit
        # YMM registers (~1.5-2x throughput vs SSE2 baseline).  Min CPU =
        # Haswell (2013+) Intel / Excavator (2015+) AMD; pre-AVX2 CPUs are
        # outside the supported range for BaySickDAW.
        target_compile_options(BaySickNAMCore PRIVATE /arch:AVX2)
    endif()
    set(BAYSICK_HAS_NAM_CORE TRUE)
else()
    message(STATUS "NeuralAmpModelerCore not found at ${NAM_CORE_DIR}/NAM/dsp.cpp"
                   " — BaySickNAM/IR engine will be disabled.")
    set(BAYSICK_HAS_NAM_CORE FALSE)
endif()

# ── sfizz (SFZ player engine) static library ─────────────────────────────────
# Vendored at libs/sfizz/.  Powers Phase J BaySickRustyDrums.  BSD 2-Clause —
# static-link only requires credits-screen attribution (no relink token, no
# source disclosure, no dynamic-link mandate).  The pruned tree at libs/sfizz/
# has dev tooling (.git, .github, benchmarks, tests, demos, devtools, scripts,
# non-CMake build files, dev linting configs, docs) stripped — what remains is
# just the library + its 11 vendored deps in external/.
set(SFIZZ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/sfizz")
if(EXISTS "${SFIZZ_DIR}/src/sfizz.h")
    # sfizz options must be set BEFORE add_subdirectory.  CACHE FORCE so they
    # override sfizz's own option_ex defaults.
    set(SFIZZ_RENDER              OFF CACHE BOOL "" FORCE) # skip clients/sfizz_render exe
    set(SFIZZ_JACK                OFF CACHE BOOL "" FORCE) # n/a on Windows
    set(SFIZZ_TESTS               OFF CACHE BOOL "" FORCE)
    set(SFIZZ_DEMOS               OFF CACHE BOOL "" FORCE)
    set(SFIZZ_DEVTOOLS            OFF CACHE BOOL "" FORCE)
    set(SFIZZ_BENCHMARKS          OFF CACHE BOOL "" FORCE)
    set(SFIZZ_SHARED              OFF CACHE BOOL "" FORCE) # static link only
    set(SFIZZ_USE_SNDFILE         OFF CACHE BOOL "" FORCE) # uses bundled st_audiofile
    set(SFIZZ_GIT_SUBMODULE_CHECK OFF CACHE BOOL "" FORCE) # .git pruned; submodules pre-populated
    set(BUILD_TESTING             OFF CACHE BOOL "" FORCE) # gates Abseil's tests
    add_subdirectory("${SFIZZ_DIR}" sfizz_build)
    set(BAYSICK_HAS_SFIZZ TRUE)
else()
    message(STATUS "sfizz not found at ${SFIZZ_DIR}/src/sfizz.h"
                   " — Phase J BaySickRustyDrums engine will be disabled.")
    set(BAYSICK_HAS_SFIZZ FALSE)
endif()

# ── J-8 (2026-05-04): vendored LunaSVG for high-fidelity SVG rendering ────────
# JUCE's built-in SVG renderer is incomplete (no <linearGradient>/<radialGradient>
# resolution, no `fill="url(#id)"` support).  Big Rusty Drums' kit graphic relies
# on radial gradients for shells/heads/cymbals; without LunaSVG every piece would
# render as a black ellipse.  LunaSVG (MIT) is single-deps (PlutoVG, vendored as
# submodule) and renders to a pixel bitmap we wrap as juce::Image once at load.
set(LUNASVG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/lunasvg")
if(EXISTS "${LUNASVG_DIR}/include/lunasvg.h")
    set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${LUNASVG_DIR}" lunasvg_build)
    set(BAYSICK_HAS_LUNASVG TRUE)
else()
    message(STATUS "LunaSVG not found at ${LUNASVG_DIR}/include/lunasvg.h"
                   " — kit graphic will fall back to JUCE SVG (black blobs).")
    set(BAYSICK_HAS_LUNASVG FALSE)
endif()

# ── QA-Fe (2026-07-13): vocal pitch-shift engines (WORLD / Rubber Band / Signalsmith) ──
# The vocal pitch editor, BaySickAlign, and real-time correction moved off the
# in-house PSOLA engine onto three vendored library engines.  All are standalone-
# only (the pitch DSP is not in the VST scaffolding target).  Full license text
# is bundled per lib under libs/<lib>/ (repo-root LICENSE is GPLv3;
# THIRD_PARTY_LICENSES.md summarizes):
#   * WORLD               -- modified-BSD (permissive)
#   * Rubber Band R3      -- GPL v2-or-later (GPLv3-compatible; the app ships GPLv3)
#   * Signalsmith Stretch -- MIT (header-only; depends on Signalsmith Linear)
#   * Signalsmith Linear  -- MIT (header-only FFT/STFT backend)
# The global CMAKE_MSVC_RUNTIME_LIBRARY (/MD) set at the top of this file applies
# to these add_library() targets too (reference_msvc_runtime_md_md_match), so no
# per-lib runtime patch is needed.

# WORLD (modified-BSD) -- high-quality vocal analysis/synthesis vocoder.  Upstream
# CMakeLists also builds an unconditional world_tool (audio-IO helpers) + examples
# we don't want, so -- like BaySickNAMCore -- collect just the 11 core src/*.cpp
# into our own static lib.  Headers live in src/world/ and sources include them as
# "world/<x>.h", so the PUBLIC include root is src/.
set(WORLD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/world")
if(EXISTS "${WORLD_DIR}/src/harvest.cpp")
    add_library(BaySickWorld STATIC
        "${WORLD_DIR}/src/cheaptrick.cpp"
        "${WORLD_DIR}/src/codec.cpp"
        "${WORLD_DIR}/src/common.cpp"
        "${WORLD_DIR}/src/d4c.cpp"
        "${WORLD_DIR}/src/dio.cpp"
        "${WORLD_DIR}/src/fft.cpp"
        "${WORLD_DIR}/src/harvest.cpp"
        "${WORLD_DIR}/src/matlabfunctions.cpp"
        "${WORLD_DIR}/src/stonemask.cpp"
        "${WORLD_DIR}/src/synthesis.cpp"
        "${WORLD_DIR}/src/synthesisrealtime.cpp"
    )
    target_include_directories(BaySickWorld PUBLIC "${WORLD_DIR}/src")
    if(MSVC)
        target_compile_options(BaySickWorld PRIVATE /W0)
    endif()
    set(BAYSICK_HAS_WORLD TRUE)
else()
    message(STATUS "WORLD not found at ${WORLD_DIR}/src/harvest.cpp"
                   " -- WORLD pitch engine will be disabled.")
    set(BAYSICK_HAS_WORLD FALSE)
endif()

# Rubber Band R3 (GPL v2+) -- general-purpose stretch/shift; R3 offline + the
# R3LiveShifter real-time variant.  Upstream builds with meson (no CMakeLists);
# it ships a single-file compilation unit (single/RubberBandSingle.cpp) that
# #includes every needed src/*.cpp with the built-in FFT + BQ resampler and
# NO_THREADING -- exactly the portable static build we want.  The file must stay
# at single/ for its "../src" relative includes to resolve; the only extra include
# it needs is libs/rubberband (for the public rubberband/*.h it references).
set(RUBBERBAND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/rubberband")
if(EXISTS "${RUBBERBAND_DIR}/single/RubberBandSingle.cpp")
    add_library(BaySickRubberBand STATIC "${RUBBERBAND_DIR}/single/RubberBandSingle.cpp")
    target_include_directories(BaySickRubberBand PUBLIC "${RUBBERBAND_DIR}")
    if(MSVC)
        # Windows.h is pulled in by src/common/sysutils.cpp; NOMINMAX keeps its
        # min/max macros from clashing with the library's std::min/max use.
        target_compile_definitions(BaySickRubberBand PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_options(BaySickRubberBand PRIVATE /W0)
    endif()
    set(BAYSICK_HAS_RUBBERBAND TRUE)
else()
    message(STATUS "Rubber Band single-file unit not found at ${RUBBERBAND_DIR}/single/RubberBandSingle.cpp"
                   " -- Rubber Band pitch engine will be disabled.")
    set(BAYSICK_HAS_RUBBERBAND FALSE)
endif()

# ── libmp3lame (LGPL) -- MP3 export encoder ──────────────────────────────────
# QA-Export docket 7=A: vendored source, statically linked (JUCE ships no MP3
# encoder and its LAME wrapper shells out to a CLI binary we would have to
# redistribute).  LGPL surface is routed to QA-LegalReview; libs/lame/COPYING is
# the license text, version is 3.100 release (libs/lame/libmp3lame/version.h).
#
# Three source dirs, all required:
#   libmp3lame/ - the encoder itself.
#   mpglib/     - the decoder.  lame.c references its entry points even in an
#                 encode-only build, so omitting it is a link error, not a saving.
#   libmp3lame/vector/ - SSE via <xmmintrin.h> INTRINSICS (not assembly).  On x64
#                 SSE is guaranteed, so quantize.c binds init_xrpow_core_sse
#                 unconditionally (quantize.c:101-104) and dropping this dir is an
#                 unresolved-external at link.
# Only libmp3lame/i386/*.nas is real assembly; that needs NASM as a new build
# dependency and stays out -- the intrinsics path already covers x64.
set(LAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/lame")
if(EXISTS "${LAME_DIR}/include/lame.h")
    file(GLOB LAME_SOURCES
        "${LAME_DIR}/libmp3lame/*.c"
        "${LAME_DIR}/libmp3lame/vector/*.c"
        "${LAME_DIR}/mpglib/*.c")
    add_library(BaySickLame STATIC ${LAME_SOURCES})
    target_include_directories(BaySickLame PUBLIC  "${LAME_DIR}/include")
    target_include_directories(BaySickLame PRIVATE
        "${LAME_DIR}" "${LAME_DIR}/libmp3lame" "${LAME_DIR}/mpglib")
    # We do not run autotools, so there is no generated config.h.  Upstream ships
    # a pre-made MSVC equivalent (configMS.h) and libs/lame/config.h -- the only
    # file added to the vendored tree -- forwards to it.  Using upstream's own
    # config beats hand-rolling a define list: it already supplies STDC_HEADERS,
    # LAME_LIBRARY_BUILD, the size/limits macros and the MSVC intN_t typedefs.
    # HAVE_MPGLIB is passed in because configMS.h keys DECODE_ON_THE_FLY off it.
    target_compile_definitions(BaySickLame PRIVATE HAVE_CONFIG_H HAVE_MPGLIB)
    if(MSVC)
        # Upstream C from 1998-2017: signed/unsigned mismatches, unused locals and
        # POSIX-name deprecations throughout.  Not our code to clean, and warnings
        # here would drown the project's own.
        target_compile_options(BaySickLame PRIVATE /W0)
        target_compile_definitions(BaySickLame PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    set(BAYSICK_HAS_LAME TRUE)
else()
    message(STATUS "libmp3lame not found at ${LAME_DIR}/include/lame.h"
                   " -- MP3 export will be disabled.")
    set(BAYSICK_HAS_LAME FALSE)
endif()

# Signalsmith Stretch + Linear (MIT) -- header-only, no compiled target.  Stretch's
# public header (signalsmith-stretch/signalsmith-stretch.h) #includes
# "signalsmith-linear/stft.h", so BOTH packaged include/ roots go on the consumer
# include path; stft.h then resolves its own "./fft.h" locally.
set(SIGSMITH_STRETCH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/signalsmith-stretch")
set(SIGSMITH_LINEAR_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/libs/signalsmith-linear")
if(EXISTS "${SIGSMITH_STRETCH_DIR}/include/signalsmith-stretch/signalsmith-stretch.h"
   AND EXISTS "${SIGSMITH_LINEAR_DIR}/include/signalsmith-linear/stft.h")
    set(BAYSICK_HAS_SIGNALSMITH TRUE)
else()
    message(STATUS "Signalsmith Stretch/Linear headers not found under libs/signalsmith-*/include"
                   " -- Signalsmith pitch engine will be disabled.")
    set(BAYSICK_HAS_SIGNALSMITH FALSE)
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

target_include_directories(BaySickDAW PRIVATE Source Source/Engine Source/Engine/Tasks Source/Harmless Source/VibePlayer Source/BaySickSynth Source/BaySickBass
    # moodycamel::ConcurrentQueue — vendored single-header MPMC queue for the
    # multi-threaded render engine. Drop concurrentqueue.h into this folder.
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/concurrentqueue"
)

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
    Source/Standalone/WorkspaceWindow.cpp
    Source/Standalone/EngineChainProcessor.cpp
    Source/Standalone/BaySickTitleBar.cpp   # QA-A (2026-05-09)
    Source/Standalone/HeavyOperationOverlay.cpp   # QA-I (2026-07-18)
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
    Source/DSP/Mp3Writer.cpp                 # QA-Export (2026-07-25) - libmp3lame front end
    Source/Standalone/ProjectBundler.cpp     # QA-Export (2026-07-25) - shared with QA-ProjectSave's Pack
    Source/DSP/PhaseVocoder.cpp
    Source/DSP/CompressorDSP.cpp
    Source/DSP/DeEsserDSP.cpp
    Source/DSP/DenoiseDSP.cpp                # QA-Fe2 De-noise (2026-07-16)
    Source/DSP/GateDSP.cpp                   # QA-Fe2 vocal-chain Gate
    Source/DSP/DeReverbDSP.cpp               # QA-Fe2 vocal-chain De-reverb
    Source/DSP/SibilanceSpectralProcessor.cpp
    Source/DSP/PitchTrackerYIN.cpp
    Source/DSP/PitchCorrectorDSP.cpp
    Source/DSP/BaySickAlignDSP.cpp
    Source/DSP/BaySickPitchDSP.cpp
    # QA-Fe (2026-07-13) - library pitch-engine bake seam (Rubber Band / Signalsmith / WORLD)
    Source/DSP/LibraryPitchShifters.cpp
    Source/DSP/ReverbDSP.cpp
    Source/DSP/ChorusDSP.cpp
    Source/DSP/DelayDSP.cpp
    Source/DSP/EQ8DSP.cpp
    Source/DSP/EffectParamMap.cpp
    Source/DSP/EQ8MsDSP.cpp
    Source/DSP/EqLinearPhaseProcessor.cpp
    Source/DSP/LufsMeterDSP.cpp
    Source/DSP/TruePeakMeter.cpp
    Source/DSP/SaturationDSP.cpp
    Source/DSP/FlangerDSP.cpp
    Source/DSP/OverdriveDSP.cpp
    Source/DSP/PhaserDSP.cpp
    Source/DSP/TransientShaperDSP.cpp
    Source/DSP/LimiterDSP.cpp
    Source/DSP/MicSimDSP.cpp
    Source/DSP/MicPlacementDSP.cpp
    # Phase I-5 — Harmonics drive pedals batch (4 new DSPs; OD Style is folded into OverdriveDSP)
    Source/DSP/BluesDriveStyleDSP.cpp
    Source/DSP/DistortionStyleDSP.cpp
    Source/DSP/FuzzStyleDSP.cpp
    Source/DSP/HighGainStyleDSP.cpp
    # Phase I-6 — Harmonics bass pedals batch (2 multi-band DSPs)
    Source/DSP/BassDriverStyleDSP.cpp
    Source/DSP/BassOverdriveStyleDSP.cpp
    # Phase I-7 — OC Style Octave (Polyphonic granular + Vintage divider)
    Source/DSP/OctaveStyleDSP.cpp
    # Phase I-8 — Dynamics pedals batch (Noise Gate + multi-band Bass Comp)
    Source/DSP/NoiseGateStyleDSP.cpp
    Source/DSP/BassCompressorStyleDSP.cpp
    # Phase I-9 — SY Style Polyphonic Synth (YIN pitch tracker + oscillator voice)
    Source/DSP/SynthStyleDSP.cpp
    # QA-EffectsReview Task 4 — SY-1 polyphonic pitch tracker (FFT harmonic-sum)
    Source/DSP/PolyPitchTracker.cpp
    # Phase I-10 — PW Style Wah (TPT resonant bandpass + parallel bass LP)
    Source/DSP/WahStyleDSP.cpp
    # Phase I-11 — AD Style Acoustic Preamp (body convolution + Schroeder + notch)
    Source/DSP/AcousticPreampStyleDSP.cpp
    # Phase I-11 — AC Style Acoustic Simulator (corrective EQ + transient + Schroeder)
    Source/DSP/AcousticSimulatorStyleDSP.cpp
    # Phase I-12 — EQ trio batch (GE / GEB / EQFH)
    Source/DSP/GraphicEQStyleDSP.cpp
    Source/DSP/BassGraphicEQStyleDSP.cpp
    Source/DSP/FurmanEQStyleDSP.cpp
    # Phase I-13 — TU Style Tuner (YIN-driven strobe + LED bar)
    Source/DSP/TunerStyleDSP.cpp
    # Phase I-15c — User NAM Pedal (.nam capture loader + EQ + blend)
    Source/DSP/NAMPedalStyleDSP.cpp
    # Phase 2 — EffectRack + Effects Page
    Source/EffectRack.cpp
    Source/Standalone/SlotComponent.cpp
    Source/Standalone/EffectsPage.cpp
    Source/Standalone/EffectEditorPanels.cpp
    Source/Standalone/EffectPresetIO.cpp
    Source/Standalone/EffectWindows.cpp
    Source/Standalone/FxRackPresetIO.cpp
    # QA-ModelShell TS6 (2026-07-29): Options > Plugins manager (BLU-299).
    Source/Standalone/PluginsManagerWindow.cpp
    # QA-ModelShell TS6 (BLU-447): the hosted-VST3-instrument tab view.
    Source/Standalone/PluginsPage.cpp
    # QA-ModelShell TS7 (CL-044): the floating master spectrum analyzer.
    Source/Standalone/MasterAnalyzerWindow.cpp
    # QA-ModelShell TS7 (CL-227): the HTML + CSV loudness conformance report.
    Source/Standalone/LoudnessReportWriter.cpp
    # QA-ModelShell TS7 (§3): automatic per-playback-pass version capture.
    Source/Standalone/VersionCapture.cpp
    # QA-ModelShell TS7 (§9.1): the one title-strip look, shell + desktop windows.
    Source/Standalone/WindowChrome.cpp
    # Pages
    Source/Standalone/LayersPage.cpp
    Source/Standalone/BassPage.cpp
    Source/Standalone/DrumPage.cpp
    Source/Standalone/BaySickRustyDrumsPage.cpp   # J-6 (2026-05-03)
    # G-7 — unified Page Preset save / load
    # 2026-05-05 consolidation: AriaPagePresetIO + RustyDrumsPagePresetIO
    # folded into PagePresetIO; both shim files deleted.  Every page-type
    # (Layers / Bass / Drum / Clip / Vox / Inst / RustyDrums) now uses the
    # one PagePresetIO::PageChainConfig API.
    Source/Standalone/PagePresetIO.cpp
    Source/Standalone/DrumKitGrid.cpp
    Source/Standalone/PianoRoll.cpp
    Source/Standalone/PianoRollPage.cpp
    Source/Standalone/BuilderPage.cpp
    # Phase G-1 — BaySickNAM/IR engine (Neural Amp Modeler + IR cabinet)
    Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp
    Source/BaySickNAMIR/BaySickNAMIREditor.cpp
    # Phase I-1 — BaySickPedals (8-slot guitar/bass pedal rack; editor lands in I-15)
    Source/BaySickPedals/BaySickPedalsProcessor.cpp
    Source/BaySickPedals/BaySickPedalsEditor.cpp
    # Phase J-2 — BaySickRustyDrums (sfizz-driven drum-kit engine; editor lands in J-8)
    Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp
    Source/BaySickRustyDrums/BaySickRustyDrumsKitGraphic.cpp
    # K-5 (2026-05-05): AriaControlPanel moved to Source/Standalone/ (shared by Rusty + Guitars + Basses)
    Source/Standalone/AriaControlPanel.cpp
    # Phase K-1 — BaySickGuitars (sfizz-driven melodic guitar engine; one per Inst page; editor lands in K-5)
    Source/BaySickGuitars/BaySickGuitarsProcessor.cpp
    # Phase L-1 — BaySickBasses (sfizz-driven melodic bass engine; mirrors Guitars; one per Inst page)
    Source/BaySickBasses/BaySickBassesProcessor.cpp
    # QA-SlideSampler — blended multi-sample slide for Guitars/Basses (region extraction + slide DSP + shared decode cache)
    Source/SlideSampler/SlideRegionMap.cpp
    Source/SlideSampler/SlideSampler.cpp
    Source/SlideSampler/SlideSampleCache.cpp
    # Phase I-3b — App-wide MIDI Learn registry (right-click "MIDI Learn" UI lands in I-3c)
    Source/MidiLearn/MidiLearnRegistry.cpp
    Source/MidiLearn/DrumTriggerMap.cpp
    # Phase H-1+ — BaySickVocal engine (vocal chain processor + editor)
    Source/BaySickVocal/BaySickVocalProcessor.cpp
    Source/BaySickVocal/BaySickVocalEditor.cpp
    Source/BaySickVocal/BaySickPitchEditor.cpp
    Source/BaySickVocal/BaySickPitchSubEditor.cpp
    Source/BaySickVocal/BaySickAlignEditor.cpp
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
    Source/Standalone/RustyDrumsMapWindow.cpp
    # Pattern colour picker (Phase F-1 — 2026-04-26)
    Source/Standalone/PatternColorPicker.cpp
    # Phase 2B — VibePlayer Editor (standalone UI only — includes SharedUI.h)
    Source/VibePlayer/VibePlayerEditor.cpp
    # 2026-04-25: legacy DrumsPage.cpp + BaySickDrumsEditor.cpp removed.
)

target_compile_definitions(BaySickDAWStandalone
    PUBLIC ${VIBESYNTH_DEFS} VIBESYNTH_STANDALONE=1
)

target_include_directories(BaySickDAWStandalone PRIVATE Source Source/Engine Source/Engine/Tasks Source/Standalone Source/Harmless Source/VibePlayer Source/BaySickSynth Source/BaySickBass Source/BaySickNAMIR Source/BaySickPedals Source/BaySickRustyDrums Source/BaySickGuitars Source/BaySickBasses Source/BaySickVocal Source/Clips Source/Vox Source/Inst Source/MidiLearn Source/SlideSampler
    # Rubber Band Library -- public headers (rubberband/*.h) so consumer code can
    # #include <rubberband/RubberBandLiveShifter.h>.  The compiled library links
    # via BaySickRubberBand (QA-Fe); this include is also propagated by that lib.
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/rubberband"
    # moodycamel::ConcurrentQueue — vendored single-header MPMC queue for the
    # multi-threaded render engine. Drop concurrentqueue.h into this folder.
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/concurrentqueue"
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
    # K-6 follow-up (2026-05-05): force whole-archive linking so MSVC keeps
    # every .obj from BaySickNAMCore — including wavenet/model.cpp's
    # `static nam::ConfigParserHelper _register_WaveNet(...)` static
    # initializer.  Without this, MSVC's linker dead-strips the .obj because
    # nothing in the consumer code references it directly (the registry-based
    # architecture system reaches it only via indirection), and the runtime
    # ConfigParserRegistry stays empty → "No config parser registered for
    # architecture: WaveNet" when the user tries to load a NAM model.
    if(MSVC)
        target_link_options(BaySickDAWStandalone PRIVATE /WHOLEARCHIVE:BaySickNAMCore)
    endif()
endif()

# Phase J — link sfizz (SFZ player engine) when vendored.
if(BAYSICK_HAS_SFIZZ)
    target_link_libraries(BaySickDAWStandalone PRIVATE sfizz::sfizz)
    target_include_directories(BaySickDAWStandalone PRIVATE "${SFIZZ_DIR}/src")
    target_compile_definitions(BaySickDAWStandalone PRIVATE BAYSICK_HAS_SFIZZ=1)
endif()

# J-8 — link LunaSVG (high-fidelity SVG → pixel bitmap rendering) when vendored.
if(BAYSICK_HAS_LUNASVG)
    target_link_libraries(BaySickDAWStandalone PRIVATE lunasvg)
    target_include_directories(BaySickDAWStandalone PRIVATE "${LUNASVG_DIR}/include")
    target_compile_definitions(BaySickDAWStandalone PRIVATE BAYSICK_HAS_LUNASVG=1)
endif()

# QA-Fe -- link the vocal pitch-shift engines (standalone-only) when vendored.
# WORLD + Rubber Band are compiled static libs; Signalsmith is header-only (just
# include paths).  Each gets a BAYSICK_HAS_<engine>=1 define so the IPitchShifter
# seam can #if out an engine that isn't present.
if(BAYSICK_HAS_WORLD)
    target_link_libraries(BaySickDAWStandalone PRIVATE BaySickWorld)
    target_compile_definitions(BaySickDAWStandalone PRIVATE BAYSICK_HAS_WORLD=1)
endif()
if(BAYSICK_HAS_RUBBERBAND)
    target_link_libraries(BaySickDAWStandalone PRIVATE BaySickRubberBand)
    target_compile_definitions(BaySickDAWStandalone PRIVATE BAYSICK_HAS_RUBBERBAND=1)
endif()
if(BAYSICK_HAS_LAME)
    target_link_libraries(BaySickDAWStandalone PRIVATE BaySickLame)
    target_compile_definitions(BaySickDAWStandalone PRIVATE BAYSICK_HAS_LAME=1)
endif()
if(BAYSICK_HAS_SIGNALSMITH)
    target_include_directories(BaySickDAWStandalone PRIVATE
        "${SIGSMITH_STRETCH_DIR}/include"
        "${SIGSMITH_LINEAR_DIR}/include")
    target_compile_definitions(BaySickDAWStandalone PRIVATE BAYSICK_HAS_SIGNALSMITH=1)
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

    # 2026-05-06: enable AVX2 codegen for the whole standalone target.  MSVC
    # x64 default is /arch:SSE2 (128-bit XMM); AVX2 lets the compiler auto-
    # vectorize hot DSP loops with 256-bit YMM registers — typical 15-25%
    # DSP-load reduction on active sessions across our EQ / compressor /
    # reverb / chorus / etc.  Min CPU = Haswell (2013+) Intel / Excavator
    # (2015+) AMD; pre-AVX2 CPUs will hard-crash on first AVX2 instruction
    # (no graceful fallback).  This matches our supported user base.
    target_compile_options(BaySickDAWStandalone PRIVATE /arch:AVX2)
endif()

# ── Stage runtime resources next to the exe (QA-EffectsReview Task 7) ─────────
# The app loads optional overrides (cassette IRs + hiss samples, acoustic IRs)
# from <exe dir>/Resources/.  Copy the repo Resources/ tree there post-build so
# the running binary finds them.  copy_directory works on the 3.22 minimum;
# re-copies each build (~16 MB, sub-second on SSD).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
    add_custom_command(TARGET BaySickDAWStandalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
                "$<TARGET_FILE_DIR:BaySickDAWStandalone>/Resources"
        VERBATIM)
endif()

# ── BaySickPluginHost — the plugin sandbox helper (QA-ModelShell TS6, BLU-302) ─
# Hosts ONE plugin in its own process, so a crashing plugin takes only itself
# down.  FL's measured shape: one process per bridged plugin.
#
# Links ONLY the three JUCE modules it needs and NONE of our vendored libraries
# (sfizz / NAM / RubberBand / LAME / WORLD / lunasvg).  That is deliberate and
# load-bearing: those are all x64 builds in this tree, and the 32-bit helper
# (BLU-302 7b, built from this same source via a separate Win32 configure)
# would otherwise require x86 builds of every one of them.  Keeping this binary
# tiny is what makes the 32-bit half cheap.
# PRODUCT_NAME (not OUTPUT_NAME) is what juce_add_gui_app uses to name the
# artefact -- OUTPUT_NAME is silently ignored here, which produced a plain
# BaySickPluginHost.exe that SandboxedPluginClient::helperExecutable would never
# have found.  Both builds ship side by side and the host picks between them
# from the plugin's own PE machine type, so the suffix has to be in the filename.
juce_add_gui_app(BaySickPluginHost
    PRODUCT_NAME "BaySickPluginHost${BAYSICK_HELPER_ARCH_SUFFIX}"
)

juce_generate_juce_header(BaySickPluginHost)

target_sources(BaySickPluginHost PRIVATE
    Source/Hosting/Helper/PluginHostMain.cpp
)

target_compile_definitions(BaySickPluginHost
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # The helper's entire reason to exist.
        JUCE_PLUGINHOST_VST3=1
)

target_include_directories(BaySickPluginHost PRIVATE Source)

target_link_libraries(BaySickPluginHost
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Stage the helper beside the app so SandboxedPluginClient::helperExecutable
# finds it next to the running binary.
add_custom_command(TARGET BaySickPluginHost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:BaySickPluginHost>"
            "$<TARGET_FILE_DIR:BaySickDAWStandalone>/$<TARGET_FILE_NAME:BaySickPluginHost>"
    VERBATIM)
