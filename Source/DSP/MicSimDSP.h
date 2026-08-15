#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// MicSimDSP - H-6d (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// Three-way exclusive mic-fingerprint stage that lives on
// BaySickNAMIRProcessor between the IR cabinet stage and the master output.
//
//   Mode 0 = None      (passthrough)
//   Mode 1 = Built-in  (4-band parametric EQ fingerprint, 10 generic mic
//                       archetypes -- "Live Vocal Dynamic" / "Broadcast
//                       Dynamic" / etc.  Generic descriptive names only --
//                       no trademarked product names ship in the binary.)
//   Mode 2 = User IR   (juce::dsp::Convolution of a user-supplied .wav
//                       file.  We DO NOT bundle any third-party IRs.)
//
// `Mix` knob is a wet/dry blend over whichever active path is producing
// output; mix = 100% means full processed signal, mix = 0% means full
// dry passthrough (regardless of Mode).
//
// Threading: setModel + loadUserIr run off the audio thread.  The EQ-coeff
// swap (Built-in mode) is wait-free via per-band coefficient pointer copy;
// user-IR convolution swap uses juce::dsp::Convolution's own wait-free
// pattern.  process() gates the user-IR path on the per-slot atomic flag and
// never reads the per-slot path strings, which the loader reassigns.
// ─────────────────────────────────────────────────────────────────────────────

class MicSimDSP
{
public:
    // NO LONGER 1:1 WITH `nam_micsim_mode` (Jeff, 2026-08-11).  That param lists
    // Built-in / User IR only -- the mic being off is the Mic A / Mic B Active
    // switch now -- so the processor passes index + 1.  None is still the DSP's
    // passthrough and is what the switch's ramp fades toward.
    enum class Mode : int
    {
        None     = 0,
        BuiltIn  = 1,
        UserIR   = 2
    };

    // Built-in mic model indices.  Display names + tooltips live on the
    // editor side; this enum is just an ordinal that maps into the
    // coefficient table inside the .cpp.
    enum class Model : int
    {
        LiveVocalDynamic   = 0,
        BroadcastDynamic   = 1,
        WorkhorseCardioid  = 2,
        VintageLDC87       = 3,
        ModernLDC          = 4,
        MultiPatternLDC    = 5,
        TubeLDC            = 6,
        PencilSDC          = 7,
        Ribbon             = 8,
        KickDrum           = 9,
        kNumModels         = 10
    };

    MicSimDSP();
    ~MicSimDSP();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    void setMode      (Mode m)            noexcept { mMode = m; }
    void setModel     (int  modelIndex);              // updates the 4-band EQ coeffs
    void setMix       (float mix01)       noexcept { mMix = juce::jlimit (0.0f, 1.0f, mix01); }

    // Per-slot Mic IR (matches BaySickNAMIR's A/B slot model).  Both slots'
    // IRs stay resident in RAM; setActiveSlot() switches which is heard
    // instantly with no reload.  -1 in any slot arg means "active slot".
    //
    // Takes a RESOLVED file (callers resolve the stored reference through
    // ProjectFileResolver first) and records the persisted reference for it.
    // Returns false with outErr filled when the file cannot be read as audio;
    // the slot is left exactly as it was in that case, so a failed pick never
    // strips a working IR, and the caller reports the failure.
    bool loadUserIr   (const juce::File& f, juce::String& outErr, int slot = -1);
    void clearUserIr  (int slot = -1);
    void setActiveSlot (int slot) noexcept { mActiveSlot.store (juce::jlimit (0, 1, slot),
                                                                 std::memory_order_release); }

    Mode getMode()           const noexcept { return mMode; }
    int  getModel()          const noexcept { return mModel; }
    int  getActiveSlot()     const noexcept { return mActiveSlot.load (std::memory_order_acquire); }
    bool hasUserIr (int slot = -1) const noexcept
    {
        const int s = (slot < 0) ? getActiveSlot() : juce::jlimit (0, 1, slot);
        return mUserIrLoaded[(size_t) s].load (std::memory_order_acquire);
    }
    // Returns the PERSISTED reference ("library:<rel>" / "mysamples:<rel>" or an
    // absolute path), which is what gets saved -- never build a bare juce::File
    // from it to open the file; resolve it through ProjectFileResolver.
    juce::String getUserIrPath (int slot = -1) const noexcept
    {
        const int s = (slot < 0) ? getActiveSlot() : juce::jlimit (0, 1, slot);
        return mUserIrPaths[(size_t) s];
    }

    // Display names + tooltips for the 10 built-in models.  Editor calls
    // these to populate the dropdown + tooltip strings.
    static const char* modelDisplayName (int modelIndex);
    static const char* modelTypicalUse  (int modelIndex);

private:
    using StereoIIR = juce::dsp::ProcessorDuplicator<
                          juce::dsp::IIR::Filter<float>,
                          juce::dsp::IIR::Coefficients<float>>;

    static constexpr int kNumBands = 4;

    void rebuildEqForModel (int modelIndex);

    double mSampleRate { 44100.0 };
    int    mMaxBlock   { 512 };
    int    mNumCh      { 2 };
    bool   mPrepared   { false };

    std::atomic<Mode> mMode { Mode::None };
    int   mModel { (int) Model::LiveVocalDynamic };
    float mMix   { 1.0f };

    std::array<StereoIIR, kNumBands> mEqBands;

    // Per-slot user IR -- both stay resident.  Convolution doesn't have a
    // brace-init constructor that takes args, so std::array of plain
    // Convolutions default-constructs each.
    std::array<juce::dsp::Convolution, 2> mUserIrs;
    std::array<std::atomic<bool>, 2>       mUserIrLoaded;
    std::array<juce::String, 2>            mUserIrPaths;
    std::atomic<int>                       mActiveSlot { 0 };

    juce::AudioBuffer<float> mDryScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicSimDSP)
};
