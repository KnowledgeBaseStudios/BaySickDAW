cmake_minimum_required(VERSION 3.22)

# ── BaySickPluginHost — STANDALONE project for the plugin-sandbox helper ──────
# QA-ModelShell TS6 (BLU-302 step 7b, 2026-07-29).
#
# WHY THIS IS ITS OWN PROJECT rather than a target in the root CMakeLists:
# an MSVC generator is single-platform per build tree, so the 32-bit helper
# cannot be another target alongside the x64 app -- it needs its own configure
# with -A Win32 into its own build directory.  Configuring the ROOT project that
# way would drag in BaySickDAWStandalone and therefore x86 builds of sfizz, NAM,
# RubberBand, LAME and WORLD, none of which we want or need.
#
# This project depends on NOTHING but vendored JUCE and one source file, which
# is exactly what keeps the 32-bit half cheap.  The root CMakeLists still builds
# the x64 helper as a normal target; this file exists solely so the same source
# can also be built x86.
#
# Configure (from the repo root):
#   cmake -S Source/Hosting/Helper -B build32 -A Win32
#   cmake --build build32 --config Release
# do_build.bat does both automatically.

project(BaySickPluginHostStandalone VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vendored JUCE, four directories up from here.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../juce" "${CMAKE_BINARY_DIR}/juce_build")

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(BAYSICK_HELPER_ARCH_SUFFIX "32")
else()
    set(BAYSICK_HELPER_ARCH_SUFFIX "64")
endif()
message(STATUS "Standalone helper project -> BaySickPluginHost${BAYSICK_HELPER_ARCH_SUFFIX}.exe")

juce_add_gui_app(BaySickPluginHost
    PRODUCT_NAME "BaySickPluginHost${BAYSICK_HELPER_ARCH_SUFFIX}"
)

juce_generate_juce_header(BaySickPluginHost)

target_sources(BaySickPluginHost PRIVATE PluginHostMain.cpp)

target_compile_definitions(BaySickPluginHost
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # The helper's entire reason to exist.
        JUCE_PLUGINHOST_VST3=1
)

# Reaches ../../ for PluginBridgeProtocol.h -- the ONE header shared with the
# host, and the reason its layout is asserted rather than trusted: this build
# and the x64 one must agree on every byte.
target_include_directories(BaySickPluginHost PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

target_link_libraries(BaySickPluginHost
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
