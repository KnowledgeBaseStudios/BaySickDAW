// BaySickDAW — the EQ factory presets (QA-EqPro).
//
// Ported from KBS EQ Pro's EqProPresets: pure data, applied through the same
// parameters the user turns so loading is undoable.  A dynamic band in a
// factory preset always carries a static cut as well - the preset audibly
// does its job the moment it loads, and the dynamics dig deeper when the
// signal crosses the threshold (the KBS test-pass-six lesson: a purely
// conditional cut read as "no settings").
#pragma once

#include "../../DSP/Kbs/ParametricEq.h"
#include <vector>

namespace eqview {

struct EqPreset
{
    const char* category;
    const char* name;
    std::vector<kbs::EqBandParams> bands;
};

namespace presetbuild {

inline kbs::EqBandParams bell (float hz, float dB, float q)
{
    kbs::EqBandParams b;
    b.on = true; b.type = kbs::EqType::bell;
    b.freqHz = hz; b.gainDb = dB; b.q = q;
    return b;
}

inline kbs::EqBandParams shelf (float hz, float dB, bool high)
{
    kbs::EqBandParams b;
    b.on = true;
    b.type = high ? kbs::EqType::highShelf : kbs::EqType::lowShelf;
    b.freqHz = hz; b.gainDb = dB; b.q = 0.707f;
    return b;
}

inline kbs::EqBandParams hp (float hz, int slope = 3)
{
    kbs::EqBandParams b;
    b.on = true; b.type = kbs::EqType::highPass;
    b.freqHz = hz; b.slope = slope; b.q = 0.707f;
    return b;
}

inline kbs::EqBandParams dynBell (float hz, float staticDb, float extentDb,
                                  float thrDb, float q)
{
    kbs::EqBandParams b = bell (hz, staticDb, q);
    b.dynamic = true;
    b.thresholdDb = thrDb;
    b.ratio = 4.0f;
    b.rangeDb = -std::abs (extentDb);
    return b;
}

} // namespace presetbuild

inline const std::vector<EqPreset>& eqFactoryPresets()
{
    using namespace presetbuild;
    static const std::vector<EqPreset> list = {
        { "Cleanup", "Rumble Cut",       { hp (40.0f) } },
        { "Cleanup", "Mud Cut",          { dynBell (280.0f, -3.0f, 4.0f, -30.0f, 1.2f) } },
        { "Cleanup", "De-Harsh",         { bell (3400.0f, -1.5f, 1.6f),
                                           dynBell (3400.0f, -1.5f, 5.0f, -28.0f, 2.2f) } },
        { "Vocals",  "Vocal Presence",   { hp (90.0f), bell (240.0f, -2.0f, 1.2f),
                                           bell (3200.0f, 2.5f, 1.0f),
                                           shelf (11000.0f, 2.0f, true) } },
        { "Vocals",  "Vocal Air",        { shelf (12000.0f, 3.5f, true) } },
        { "Drums",   "Kick Punch",       { bell (60.0f, 3.0f, 1.1f),
                                           bell (400.0f, -3.0f, 1.2f),
                                           bell (3500.0f, 2.5f, 1.0f) } },
        { "Drums",   "Snare Body",       { bell (200.0f, 2.5f, 1.1f),
                                           bell (900.0f, -2.0f, 1.4f),
                                           shelf (8000.0f, 2.0f, true) } },
        { "Drums",   "Drum Bus Air",     { bell (500.0f, -1.5f, 1.0f),
                                           dynBell (500.0f, -1.5f, 4.0f, -26.0f, 1.2f),
                                           shelf (10000.0f, 2.5f, true) } },
        { "Bass",    "Bass Tighten",     { hp (30.0f),
                                           bell (180.0f, -1.5f, 1.1f),
                                           dynBell (180.0f, -1.5f, 4.0f, -26.0f, 1.3f),
                                           bell (800.0f, 2.0f, 1.0f) } },
        { "Master",  "Master Smile",     { shelf (100.0f, 1.5f, false),
                                           bell (450.0f, -1.0f, 0.9f),
                                           shelf (10000.0f, 1.5f, true) } },
        { "Master",  "Master Warmth",    { shelf (150.0f, 2.0f, false),
                                           bell (2800.0f, -1.0f, 1.2f) } },
        { "Master",  "Brighten",         { shelf (8000.0f, 2.5f, true) } },
    };
    return list;
}

} // namespace eqview
