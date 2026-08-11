#include "TunerStyleDSP.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)

namespace
{
    // Standard guitar open strings (low to high).
    constexpr int kGuitarOpenStrings[] = { 40, 45, 50, 55, 59, 64 };  // E2 A2 D3 G3 B3 E4
    // Standard bass open strings (low to high).
    constexpr int kBassOpenStrings[]   = { 28, 33, 38, 43 };          // E1 A1 D2 G2
}

TunerStyleDSP::TunerStyleDSP() = default;

void TunerStyleDSP::prepare (double sampleRate, int /*maxBlockSize*/)
{
    // Bigger analysis window than the shared default so the tuner reaches bass
    // low notes (~21.5 Hz @ 44.1k).  Must be set before prepare().  Other
    // PitchTrackerYIN consumers (Octave / Synth / PitchCorrector) keep 2048.
    mYin.setAnalysisWindow (4096, 1024);
    mYin.prepare (sampleRate);
}

void TunerStyleDSP::reset()
{
    mYin.reset();
}

void TunerStyleDSP::setMode (int m)
{
    mMode = static_cast<Mode> (juce::jlimit (0, 2, m));
}

void TunerStyleDSP::setMuted (bool yes)
{
    mMuted = yes;
}

void TunerStyleDSP::set432 (bool yes)
{
    if (mIs432 == yes) return;
    mIs432 = yes;

    // Re-clamp current trim into the new mode's range.
    float lo = 0.0f, hi = 0.0f;
    getTrimRange (lo, hi);
    if (mTrimHz < lo || mTrimHz > hi)
        mTrimHz = mIs432 ? 432.0f : 440.0f;
}

void TunerStyleDSP::setTrimHz (float hz)
{
    float lo = 0.0f, hi = 0.0f;
    getTrimRange (lo, hi);
    mTrimHz = juce::jlimit (lo, hi, hz);
}

void TunerStyleDSP::setFlat (int semitones)
{
    mFlatSemitones = juce::jlimit (0, 6, semitones);
}

void TunerStyleDSP::getTrimRange (float& outMin, float& outMax) const
{
    if (mIs432) { outMin = 428.0f; outMax = 437.0f; }
    else        { outMin = 436.0f; outMax = 445.0f; }
}

float TunerStyleDSP::midiToHz (int midi) const noexcept
{
    return mTrimHz * std::pow (2.0f, (midi - 69) / 12.0f);
}

bool TunerStyleDSP::getReading (Reading& out) const noexcept
{
    const float hz = mYin.getFrequencyHz();
    if (hz < PitchTrackerYIN::kMinFreqHz || hz > PitchTrackerYIN::kMaxFreqHz) return false;

    // Convert Hz -> chromatic midi note (with cents).  Flat tuning references N
    // semitones below standard: shift the note up by N so an N-flat string reads
    // as its standard note name at 0 cents.
    const float midiF = 69.0f + 12.0f * std::log2 (hz / mTrimHz) + (float) mFlatSemitones;
    const int   midi  = (int) std::round (midiF);
    const float cents = (midiF - (float) midi) * 100.0f;

    int target = midi;
    float targetCents = cents;

    if (mMode == Mode::Guitar)
    {
        // Snap to nearest guitar open string (compare in midi space).
        int best = kGuitarOpenStrings[0];
        int bestDist = std::abs (midi - best);
        for (int s : kGuitarOpenStrings)
        {
            const int d = std::abs (midi - s);
            if (d < bestDist) { bestDist = d; best = s; }
        }
        target = best;
        targetCents = (midiF - (float) target) * 100.0f;
    }
    else if (mMode == Mode::Bass)
    {
        int best = kBassOpenStrings[0];
        int bestDist = std::abs (midi - best);
        for (int s : kBassOpenStrings)
        {
            const int d = std::abs (midi - s);
            if (d < bestDist) { bestDist = d; best = s; }
        }
        target = best;
        targetCents = (midiF - (float) target) * 100.0f;
    }

    out.midiNote        = midi;
    out.centsFromNote   = cents;
    out.targetNote      = target;
    out.centsFromTarget = targetCents;
    out.frequencyHz     = hz;
    return true;
}

void TunerStyleDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (bypassed) return;
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    // Push mono-summed input into the YIN tracker.
    if (numCh >= 2)
        mYin.pushAudio (buffer.getReadPointer (0), buffer.getReadPointer (1), n);
    else
        mYin.pushAudio (buffer.getReadPointer (0), n);

    // Mute the output if the user has the Mute switch engaged.
    if (mMuted) buffer.clear();
}

void TunerStyleDSP::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("TunerStyleDSP");
    state.setProperty ("mode",     (int) mMode,    nullptr);
    state.setProperty ("muted",    (int) mMuted,   nullptr);
    state.setProperty ("is432",    (int) mIs432,   nullptr);
    state.setProperty ("trimHz",   mTrimHz,        nullptr);
    state.setProperty ("flat",     mFlatSemitones, nullptr);
    state.setProperty ("bypassed", (int) bypassed, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void TunerStyleDSP::setStateInformation (const void* data, int sz)
{
    auto xml = SafeXml::parseBinaryBlob (data, sz);
    if (! xml || ! xml->hasTagName ("TunerStyleDSP")) return;
    auto state = juce::ValueTree::fromXml (*xml);

    setMode  ((int) state.getProperty ("mode",  (int) Mode::Chromatic));
    mMuted = ((int) state.getProperty ("muted", 0)) != 0;
    set432   (((int) state.getProperty ("is432", 0)) != 0);
    setTrimHz ((float)(double) state.getProperty ("trimHz", mIs432 ? 432.0 : 440.0));
    setFlat   ((int) state.getProperty ("flat", 0));
    bypassed = ((int) state.getProperty ("bypassed", 0)) != 0;
}
