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

        // Not the default format.  ORDERING DOES MATTER, and the comment that
        // used to sit here saying it did not was wrong (QA-Manuals MF-4):
        // createReaderFor returns the FIRST format whose canHandleFile passes,
        // and registerBasicFormats() used to register WindowsMediaAudioFormat
        // ahead of this line while also claiming ".mp3".  Ours is last, so it
        // only decodes .mp3 because CMakeLists now sets
        // JUCE_USE_WINDOWS_MEDIA_FORMAT=0 as well as JUCE_USE_MP3AUDIOFORMAT=0.
        // Both defines are load-bearing; dropping either one silently hands
        // .mp3 back to a decoder we did not audit.
        fm.registerFormat (new MpglibAudioFormat(), false);
    }
}
