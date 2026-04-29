#pragma once
#include <JuceHeader.h>

// ── BssLedRadio ───────────────────────────────────────────────────────────────
// A vertical stack of radio buttons with LED indicators.
// Writes the selected index to an AudioParameterChoice via APVTS.
// Shared between BaySickSynthEditor and BaySickBassEditor.
// ─────────────────────────────────────────────────────────────────────────────
class BssLedRadio : public juce::Component,
                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    // Backward-compatible ctor: vertical 1-column stack.
    BssLedRadio (juce::AudioProcessorValueTreeState& avts,
                 const juce::String& paramID,
                 const juce::StringArray& labels,
                 juce::Colour ledColour = juce::Colour (0xFFA0DB2B));

    // 2026-04-22: VEL-style button grid. rows × cols = labels.size() (or padded).
    BssLedRadio (juce::AudioProcessorValueTreeState& avts,
                 const juce::String& paramID,
                 const juce::StringArray& labels,
                 int rows, int cols,
                 juce::Colour ledColour = juce::Colour (0xFFA0DB2B));

    ~BssLedRadio() override;

    void paint     (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

    int  getSelected() const { return mSelected; }

private:
    void parameterChanged (const juce::String&, float newValue) override;

    juce::AudioProcessorValueTreeState& mAvts;
    juce::String      mParamID;
    juce::StringArray mLabels;
    juce::Colour      mLedColour;
    int               mSelected { 0 };
    int               mRows     { 0 };   // 0 = legacy 1-col LED-list mode
    int               mCols     { 1 };
};

// ── BssFilterXYPad ────────────────────────────────────────────────────────────
// 2D interactive pad: X = filter cutoff (log-scale 20–20000 Hz),
//                     Y = resonance (0–1, top=1).
// Reads/writes directly to APVTS using the RangedAudioParameter pattern.
// Shared between BaySickSynthEditor and BaySickBassEditor.
// ─────────────────────────────────────────────────────────────────────────────
class BssFilterXYPad : public juce::Component,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    BssFilterXYPad (juce::AudioProcessorValueTreeState& avts,
                    const juce::String& cutoffID,
                    const juce::String& resID,
                    juce::Colour dotColour = juce::Colour (0xFFA0DB2B));
    ~BssFilterXYPad() override;

    void paint     (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    void parameterChanged (const juce::String&, float) override;
    void updateDotFromApvts();
    void writeParamsFromDot();

    float hzToNorm (float hz) const;
    float normToHz (float n)  const;

    juce::AudioProcessorValueTreeState& mAvts;
    juce::String mCutoffID, mResID;
    juce::Colour mDotColour;

    float mDotX { 0.9f };  // normalised, 0=20Hz 1=20kHz
    float mDotY { 0.0f };  // normalised, 0=res=0 (bottom), 1=res=1 (top)
};
