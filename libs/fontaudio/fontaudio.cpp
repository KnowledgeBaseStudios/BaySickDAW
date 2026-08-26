#if defined(__FONTAUDIO_HEADER__) && !JUCE_AMALGAMATED_INCLUDE
/* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
#error "Incorrect use of JUCE cpp file"
#endif

// LOCAL CHANGE (KBS): the AppConfig.h include is removed.
//
// This module predates JUCE's CMake support and was written for the Projucer,
// which generated an AppConfig.h per project. JUCE 6 onward does not, so the
// include fails outright on any CMake build. The module declares its own
// dependencies - juce_core and juce_graphics - in fontaudio.h, and CMake
// satisfies those, so nothing here needed it in the first place.
//
// Upstream is otherwise untouched. If fontaudio is ever updated, this is the
// one edit to reapply.

#include "fontaudio.h"

#include "data/FontAudioData.cpp"
#include "src/FontAudio.cpp"
