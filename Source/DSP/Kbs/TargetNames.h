// KBS Plugins — readable names for DSP parameter ids
//
// The routing tables address parameters as "compressor/threshold". Both the
// host's automation list and advanced mode have to show that to a human, so
// the translation lives here — once, JUCE-free, shared by all four suites.
#pragma once

#include <string>

namespace kbs {

inline std::string deviceName (const std::string& id)
{
    static const std::pair<const char*, const char*> map[] = {
        { "eqLowCut",    "Low Cut"    }, { "eqHighCut",   "High Cut"   },
        { "eqLowPeak",   "Low Bell"   }, { "eqLowShelf",  "Low Shelf"  },
        { "eqMid",       "Mid Bell"   }, { "eqHighShelf", "High Shelf" },
        { "deHarsh",     "De-Harsh"   }, { "chug",        "Chug"       },
        { "compressor",  "Compressor" }, { "limiter",     "Limiter"    },
        { "saturation",  "Saturation" }, { "output",      "Output"     },
        { "input",       "Input"      }, { "amp",     "Amp"     },
        { "clip",        "Clip"       }, { "enhance", "Enhance" },
        { "tight",       "Tight"      }, { "smooth",  "Smooth"  },
        { "lowCtl",      "Low Band"   }, { "lowMidCtl", "Low Mid Band" },
        { "eqLowMid",    "Low Mid"    }, { "eqBright", "Bright" },
        { "deEss",       "De-Ess"     }, { "control", "Control" },
        { "push",        "Push"       }, { "plosive", "Plosive" },
    };
    for (auto& e : map) if (id == e.first) return e.second;
    return id;
}

// What an advanced knob DOES, in a few words. Keyed by the full target
// first for the cases where the same suffix means different things (a
// compressor's threshold is not a de-esser's), then by suffix for the
// many that mean the same thing everywhere. The range is appended by the
// caller from the link itself, so it is never written twice.
inline const char* paramBlurb (const std::string& target)
{
    static const std::pair<const char*, const char*> exact[] = {
        { "deEss/range",        "The most it will duck an S" },
        { "deEss/threshold",    "How easily it triggers" },
        { "plosive/range",      "The most it will duck a pop" },
        { "plosive/threshold",  "How easily pops trigger it" },
        { "control/threshold",  "How much of the voice it catches" },
        { "push/threshold",     "How much it catches" },
        { "limiter/ceiling",    "The absolute top" },
        { "limiter/release",    "How fast the limiter recovers" },
        { "saturation/amount",  "Drive depth" },
        { "saturation/character", "Which flavor of drive" },
        { "input/gain",         "Level into the strip" },
        { "output/gain",        "Level out of the strip" },
        { "deHarsh/range",      "The most it will duck harshness" },
        { "deHarsh/threshold",  "How easily it triggers" },
        { "chug/threshold",     "How easily the chug band triggers" },
        { "glue/threshold",     "How much of the bus it catches" },
        { "compressor/threshold", "How much it catches" },
    };
    for (auto& e : exact) if (target == e.first) return e.second;

    const auto slash = target.find ('/');
    const std::string suffix = slash == std::string::npos ? target
                                                          : target.substr (slash + 1);
    static const std::pair<const char*, const char*> bySuffix[] = {
        { "frequency", "Where it sits" },
        { "gain",      "Boost or cut" },
        { "q",         "How wide it reaches" },
        { "range",     "The most it will move" },
        { "threshold", "How easily it triggers" },
        { "depth",     "How firmly it rides the excess" },
        { "attack",    "How fast it grabs" },
        { "release",   "How fast it lets go" },
        { "ratio",     "How hard it squeezes" },
        { "knee",      "How gradually it engages" },
        { "makeup",    "How much taken level comes back" },
        { "ceiling",   "The absolute top" },
        { "lookahead", "Catches peaks before they land" },
        { "lookaheadMs", "How far ahead it sees" },
        { "amount",    "How much of the effect" },
        { "mix",       "Dry against wet" },
        { "width",     "Stereo width" },
        { "enabled",   "On or off" },
        { "mode",      "Which flavor" },
        { "hold",      "How long it stays open" },
        { "sense",     "How much level it takes to react" },
        { "rate",      "How fast it moves" },
        { "smooth",    "How softly it follows" },
        { "offset",    "How far the sides pull apart" },
        { "feedback",  "How much comes back around" },
    };
    for (auto& e : bySuffix) if (suffix == e.first) return e.second;
    return "";
}

inline std::string paramName (const std::string& id)
{
    static const std::pair<const char*, const char*> map[] = {
        { "frequency",  "Frequency" }, { "enabled",   "Engage"    },
        { "gain",       "Gain"      }, { "q",         "Q"         },
        { "range",      "Range"     }, { "threshold", "Threshold" },
        { "depth",      "Depth"     }, { "attack",    "Attack"    },
        { "release",    "Release"   }, { "ratio",     "Ratio"     },
        { "lookahead",  "Lookahead" }, { "lookaheadMs", "Lookahead Time" },
        { "knee",       "Knee"      }, { "makeup",    "Makeup"    },
        { "amount",     "Amount"    }, { "character", "Character" },
        { "ceiling",    "Ceiling"   }, { "mode",   "Band"   },
        { "mix",        "Mix"       }, { "select", "Select" },
        { "hold",       "Hold"      }, { "kbtrack",  "Key Track" },
        { "veltrack",   "Velocity"  },
    };
    for (auto& e : map) if (id == e.first) return e.second;
    return id;
}

// "compressor/threshold" -> "Compressor Threshold"
inline std::string targetName (const std::string& target)
{
    const auto slash = target.find ('/');
    if (slash == std::string::npos) return target;
    return deviceName (target.substr (0, slash)) + " " + paramName (target.substr (slash + 1));
}

// Discrete selectors — a mode, a character. Advanced mode gives these no
// per-link override: there is nothing to detach from a switch, and a second
// copy of it would only collide with the macro that drives it.
inline bool isSelector (const std::string& target)
{
    const auto s = target.substr (target.find ('/') + 1);
    return s == "mode" || s == "character" || s == "select";
}

// The half after the slash, which is what decides the units.
inline std::string targetSuffix (const std::string& target)
{
    const auto slash = target.find ('/');
    return slash == std::string::npos ? target : target.substr (slash + 1);
}

} // namespace kbs
