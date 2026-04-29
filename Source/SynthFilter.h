#pragma once
#include <JuceHeader.h>

enum class FilterMode { LowPass, HighPass, BandPass, Notch };

class SynthFilter
{
public:
    void prepare (double sampleRate);
    void setCutoff (float hz);
    void setResonance (float q);   // 0.1 – 10
    void setMode (FilterMode mode);
    void reset();

    float processSample (float in);

private:
    double     mSampleRate { 44100.0 };
    FilterMode mMode       { FilterMode::LowPass };
    float      mCutoff     { 5000.0f };
    float      mResonance  { 0.707f };

    // SVF state
    float mIC1eq { 0.0f };
    float mIC2eq { 0.0f };

    // Coefficients
    float mG { 0.0f }, mK { 0.0f }, mA1 { 0.0f }, mA2 { 0.0f }, mA3 { 0.0f };

    void calcCoeffs();
};
