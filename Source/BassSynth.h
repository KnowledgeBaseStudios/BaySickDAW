#pragma once
#include <JuceHeader.h>
#include "VibesynthConstants.h"

struct BassSoundParams
{
    float pitch     { 36.f };
    float decay     { 0.4f };
    float tone      { 0.5f };
    float drive     { 0.0f };
    float level     { 0.8f };
    float fmRatio   { 0.5f };
    float fmIndex   { 3.0f };
    float cutoff    { 0.5f };
    float resonance { 0.5f };
    float attack    { 0.01f };
    float release_  { 0.3f };
    float punch     { 0.7f };
};

struct BassParamDef { const char* name; float BassSoundParams::* member; };
const BassParamDef* getBassParamDefs(BassSoundType type, int& outCount);

class BassSynthVoice
{
public:
    BassSoundType   type   { BassSoundType::Sub808 };
    BassSoundParams params;
    bool            active     { false };
    bool            mReleasing { false };  // public so BassSynth::noteOff can set it
    int             mSamplePos { 0 };      // public so BassSynth::reset can clear it

    void trigger  (double sampleRate);
    void release_v();
    void resetVoice();
    float process ();

private:
    double mSampleRate   { 44100.0 };
    double mPhase        { 0.0 };
    double mPhase2       { 0.0 };
    double mEnvAmp       { 0.0 };
    double mEnvPitch     { 0.0 };
    // Filter states are float to match onePoleLP signature
    float  mFilterState  { 0.f };
    float  mFilterState2 { 0.f };
    juce::Random mRand;

    float processSub808();
    float processKickBass909();
    float processTrapSub();
    float processJazzWalking();
    float processIndustrialGrind();
    float processAnalogSine();
    float processMoogStyle();
    float processAcidBass();
    float processGrowlBass();
    float processRubberBass();
    float processHipHopThump();
    float processPopSustain();
    float processRnBSmooth();
    float processBoomBap();
    float processFunkSlap();

    float softClip(float x) const { return std::tanh(x); }
    // state is float& -- no double/float mismatch
    float onePoleLP(float in, float& state, float cutoff01);
    float midiToHz(float note) const;
};

class BassSynth
{
public:
    static constexpr int MAX_POLY = 4;

    BassSynth();
    void prepare         (double sampleRate, int maxBlockSize);
    void noteOn          (int midiNote, float velocity);
    void noteOff         ();
    void renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void reset           ();

    void           setSoundType (BassSoundType type);
    BassSoundType  getSoundType () const { return mCurrentType; }
    BassSoundParams& getParams  ()       { return mParams; }

private:
    BassSoundType   mCurrentType { BassSoundType::Sub808 };
    BassSoundParams mParams;
    double          mSampleRate  { 44100.0 };
    std::array<BassSynthVoice, MAX_POLY> mVoices;

    int  findFreeVoice() const;
    void applyParamsToVoice(BassSynthVoice& v);
};
