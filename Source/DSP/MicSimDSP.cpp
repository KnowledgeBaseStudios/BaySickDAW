#include "MicSimDSP.h"
#include "../ProjectFileResolver.h"
#include "../SampleLibrary.h"

// ─────────────────────────────────────────────────────────────────────────────
// MicSimDSP - H-6d (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    struct BandSpec { float freq; float q; float gainDb; };
    struct ModelSpec
    {
        const char* displayName;
        const char* typicalUse;
        BandSpec    bands[4];
    };

    // 10 built-in mic-archetype fingerprints.  Generic descriptive names
    // only -- no trademarked product names ship in the binary.  Curve
    // values are 4-band parametric EQ approximations of typical published
    // FR characteristics.  These are "in the style of" the named archetype.
    static const ModelSpec kModels[(int) MicSimDSP::Model::kNumModels] =
    {
        // LiveVocalDynamic (SM58-style cardioid dynamic)
        { "Live Vocal Dynamic",
          "Live vocals, sturdy stage mic",
          {{   80.0f, 0.7f, -6.0f },
           {  400.0f, 1.0f, -1.5f },
           { 5000.0f, 1.4f, +5.0f },
           {10000.0f, 0.7f, -3.0f }} },

        // BroadcastDynamic (SM7B-style)
        { "Broadcast Dynamic",
          "Podcasting, voiceover, smooth vocals",
          {{  100.0f, 0.7f, -1.0f },
           { 1000.0f, 1.0f,  0.0f },
           { 4000.0f, 1.4f, +2.0f },
           {10000.0f, 0.7f, -2.0f }} },

        // WorkhorseCardioid (Beta 58 / e835-style dynamic)
        { "Workhorse Cardioid",
          "Backing vocals, brighter lead vox",
          {{  100.0f, 0.7f, -4.0f },
           {  250.0f, 1.0f, -1.0f },
           { 6000.0f, 1.4f, +6.0f },
           {12000.0f, 0.7f, +2.0f }} },

        // VintageLDC87 (U87-style large diaphragm condenser)
        { "Vintage LDC '87",
          "Studio lead vocal, classic LDC sound",
          {{   80.0f, 0.7f, +1.0f },
           { 1000.0f, 1.0f,  0.0f },
           { 8000.0f, 1.0f, +3.0f },
           {12000.0f, 1.4f, +4.0f }} },

        // ModernLDC (TLM103-style)
        { "Modern LDC",
          "Modern vocals, voiceover",
          {{  100.0f, 0.7f, -1.0f },
           {  800.0f, 1.0f,  0.0f },
           { 6000.0f, 1.0f, +4.0f },
           {12000.0f, 1.4f, +5.0f }} },

        // MultiPatternLDC (C414-style)
        { "Multi-Pattern LDC",
          "Versatile studio LDC, pop vocals",
          {{   80.0f, 0.7f,  0.0f },
           {  250.0f, 1.0f, -1.0f },
           { 5000.0f, 1.4f, +3.0f },
           {14000.0f, 1.4f, +5.0f }} },

        // TubeLDC (ELA M 251 / C12-style vintage tube)
        { "Tube LDC",
          "Character vocals, jazz, soul",
          {{   80.0f, 0.7f, +2.0f },
           {  350.0f, 1.0f, -1.5f },
           { 6000.0f, 1.0f, +2.5f },
           {10000.0f, 0.7f, +3.0f }} },

        // PencilSDC (KM184-style small diaphragm)
        { "Pencil SDC",
          "Acoustic guitar, drum overheads, hi-hat",
          {{  100.0f, 0.7f, -1.0f },
           { 1000.0f, 1.0f,  0.0f },
           { 8000.0f, 1.0f, +1.0f },
           {14000.0f, 1.0f, +2.0f }} },

        // Ribbon (R-121-style figure-8)
        { "Ribbon",
          "Guitar amps, brass, character vocal",
          {{   80.0f, 0.7f, +2.0f },
           {  500.0f, 1.0f, +1.0f },
           { 5000.0f, 1.0f, -3.0f },
           {12000.0f, 0.7f, -6.0f }} },

        // KickDrum (D112 / Beta 52-style)
        { "Kick Drum",
          "Kick drum, bass cabinets",
          {{   60.0f, 1.0f, +6.0f },
           {  400.0f, 1.4f, -6.0f },
           { 4000.0f, 1.4f, +6.0f },
           {10000.0f, 0.7f, +2.0f }} },
    };
}

const char* MicSimDSP::modelDisplayName (int idx)
{
    if (idx < 0 || idx >= (int) Model::kNumModels) return "(unknown)";
    return kModels[idx].displayName;
}

const char* MicSimDSP::modelTypicalUse (int idx)
{
    if (idx < 0 || idx >= (int) Model::kNumModels) return "";
    return kModels[idx].typicalUse;
}

MicSimDSP::MicSimDSP()
{
    for (auto& a : mUserIrLoaded) a.store (false);
}

MicSimDSP::~MicSimDSP() = default;

void MicSimDSP::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    mSampleRate = sampleRate;
    mMaxBlock   = juce::jmax (1, maxBlockSize);
    mNumCh      = juce::jmax (1, numChannels);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) mMaxBlock,
                                   (juce::uint32) mNumCh };
    for (auto& band : mEqBands)
    {
        band.prepare (spec);
        band.reset();
    }

    for (auto& ir : mUserIrs)
    {
        ir.prepare (spec);
        ir.reset();
    }

    mDryScratch.setSize (mNumCh, mMaxBlock, false, true, true);

    rebuildEqForModel (mModel);

    mPrepared = true;
}

void MicSimDSP::reset()
{
    for (auto& b : mEqBands) b.reset();
    for (auto& ir : mUserIrs) ir.reset();
    mDryScratch.clear();
}

void MicSimDSP::setModel (int idx)
{
    idx = juce::jlimit (0, (int) Model::kNumModels - 1, idx);
    if (idx == mModel) return;
    mModel = idx;
    if (mPrepared) rebuildEqForModel (mModel);
}

void MicSimDSP::rebuildEqForModel (int idx)
{
    const auto& spec = kModels[idx];
    for (int i = 0; i < kNumBands; ++i)
    {
        const auto& b = spec.bands[i];
        auto coefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                          mSampleRate, b.freq, b.q,
                          juce::Decibels::decibelsToGain (b.gainDb));
        *mEqBands[(size_t) i].state = *coefs;
    }
}

bool MicSimDSP::loadUserIr (const juce::File& f, juce::String& outErr, int slot)
{
    const int s = (slot < 0) ? getActiveSlot() : juce::jlimit (0, 1, slot);

    if (! f.existsAsFile())
    {
        outErr = "User IR file does not exist: " + f.getFullPathName();
        return false;
    }

    // The slot remembers a persisted REFERENCE, so "is this one already
    // loaded?" has to resolve before it compares.  Callers that hold only the
    // stored reference (BaySickNAMIRProcessor's A/B slot restore) re-ask for a
    // load whenever their own resolved file does not string-match what the slot
    // reports, which a reference never does -- without this early-out that
    // rebuilds the convolution on every slot switch for the life of the project.
    if (mUserIrLoaded[(size_t) s].load (std::memory_order_acquire)
        && ProjectFileResolver::resolve (mUserIrPaths[(size_t) s]) == f)
        return true;

    // juce::dsp::Convolution::loadImpulseResponse hands the file to its own
    // background loader and reports nothing back, so an unreadable or non-audio
    // pick used to land as "the slot names an IR and renders passthrough".
    // Probe with a reader before committing -- same guard, same format set, as
    // AcousticSimulatorStyleDSP::loadUserIR.
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples <= 0)
    {
        outErr = "This file could not be read as audio:\n" + f.getFullPathName();
        return false;
    }

    mUserIrs[(size_t) s].loadImpulseResponse (f,
                                   juce::dsp::Convolution::Stereo::yes,
                                   juce::dsp::Convolution::Trim::yes,
                                   0,  // 0 = use file size
                                   juce::dsp::Convolution::Normalise::yes);

    // Persisted form, not the absolute path: an IR under Core Library or My
    // Samples has to come back on another install or another Windows account,
    // and an absolute path embeds this machine's user name.  refForPersist
    // returns the absolute path for anything outside those roots, which is what
    // the bundler then rewrites when a project is made self-contained.
    mUserIrPaths[(size_t) s] = SampleLibrary::refForPersist (f);
    mUserIrLoaded[(size_t) s].store (true, std::memory_order_release);
    return true;
}

void MicSimDSP::clearUserIr (int slot)
{
    const int s = (slot < 0) ? getActiveSlot() : juce::jlimit (0, 1, slot);
    mUserIrLoaded[(size_t) s].store (false, std::memory_order_release);
    mUserIrPaths[(size_t) s].clear();
    mUserIrs[(size_t) s].reset();
}

void MicSimDSP::process (juce::AudioBuffer<float>& buffer)
{
    if (! mPrepared) return;

    const Mode mode = mMode.load (std::memory_order_acquire);
    if (mode == Mode::None) return;     // passthrough

    const int n  = buffer.getNumSamples();
    const int ch = juce::jmin (buffer.getNumChannels(), mNumCh);
    if (n == 0 || ch == 0) return;

    // Stash dry for the wet/dry blend
    if (mDryScratch.getNumSamples() < n || mDryScratch.getNumChannels() < ch)
        mDryScratch.setSize (ch, juce::jmax (n, mMaxBlock), false, true, true);
    for (int c = 0; c < ch; ++c)
        mDryScratch.copyFrom (c, 0, buffer, c, 0, n);

    // Process the active path in-place
    if (mode == Mode::BuiltIn)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        for (auto& b : mEqBands) b.process (ctx);
    }
    else if (mode == Mode::UserIR)
    {
        const int s = mActiveSlot.load (std::memory_order_acquire);
        if (! mUserIrLoaded[(size_t) s].load (std::memory_order_acquire))
            return;   // no IR loaded for active slot -- treat as passthrough

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        mUserIrs[(size_t) s].process (ctx);
    }

    // Wet/dry blend
    const float wet = mMix;
    const float dry = 1.0f - mMix;
    if (juce::approximatelyEqual (wet, 1.0f)) return;   // full wet, skip blend
    for (int c = 0; c < ch; ++c)
    {
        float* w = buffer.getWritePointer (c);
        const float* d = mDryScratch.getReadPointer (c);
        for (int i = 0; i < n; ++i)
            w[i] = w[i] * wet + d[i] * dry;
    }
}
