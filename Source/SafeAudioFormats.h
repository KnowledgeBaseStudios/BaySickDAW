#pragma once
#include <JuceHeader.h>
#include "MpglibAudioFormat.h"

// ─────────────────────────────────────────────────────────────────────────────
// SafeAudioFormats - the ONE place audio input formats are registered.
//
// QA-Cleanup 2026-08-11.  `registerBasicFormats()` was called at 19 sites across
// 11 files, which is exactly the resolver-island shape AppPaths was created to
// kill: swapping a decoder meant finding all 19, and missing one leaves a
// manager that silently disagrees with the rest of the app about what it can
// open.
//
// What differs from JUCE's own set: MP3 DECODING comes from the vendored LAME
// tree (MpglibAudioFormat) instead of JUCE's unaudited hand-ported decoder,
// which CMakeLists switches off via JUCE_USE_MP3AUDIOFORMAT=0.  Everything else
// - WAV, AIFF, FLAC, Ogg - is JUCE's.
//
// Any NEW format goes here, not at a call site.
// ─────────────────────────────────────────────────────────────────────────────
namespace SafeAudioFormats
{
    inline void registerAll (juce::AudioFormatManager& fm)
    {
        fm.registerBasicFormats();

        // Not the default format: JUCE picks readers by extension, and this is
        // the only one claiming ".mp3" now, so ordering does not matter.
        fm.registerFormat (new MpglibAudioFormat(), false);
    }
}
