#pragma once
#include <JuceHeader.h>
#include <atomic>

// ── BssLedRadio ───────────────────────────────────────────────────────────────
// A vertical stack of radio buttons with LED indicators.
// Writes the selected index to an AudioParameterChoice via APVTS.
// Shared between BaySickSynthEditor and BaySickBassEditor.
//
// Threading: APVTS dispatches parameterChanged synchronously on whatever thread
// wrote the parameter, which for an automation lane is the offline render thread
// -- never the message thread.  So the listener only mirrors state into atomics
// and raises a flag; the repaint is issued by the poll timer instead.  Calling
// repaint() from the writer's thread trips JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED
// in Debug and races the peer's unsynchronized deferredRepaints list in Release.
// ─────────────────────────────────────────────────────────────────────────────
class BssLedRadio : public juce::Component,
                    private juce::AudioProcessorValueTreeState::Listener,
                    private juce::Timer
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

    int  getSelected() const { return mSelected.load(); }

private:
    void parameterChanged (const juce::String&, float newValue) override;
    void timerCallback() override;
    void parentHierarchyChanged() override;

    juce::AudioProcessorValueTreeState& mAvts;
    juce::String      mParamID;
    juce::StringArray mLabels;
    juce::Colour      mLedColour;
    std::atomic<int>  mSelected     { 0 };
    std::atomic<bool> mNeedsRepaint { false };
    int               mRows     { 0 };   // 0 = legacy 1-col LED-list mode
    int               mCols     { 1 };
};

// ── BssFilterXYPad ────────────────────────────────────────────────────────────
// 2D interactive pad: X = filter cutoff (log-scale 20–20000 Hz),
//                     Y = resonance (0–1, top=1).
// Reads/writes directly to APVTS using the RangedAudioParameter pattern.
// Shared between BaySickSynthEditor and BaySickBassEditor.
//
// Same cross-thread parameterChanged contract as BssLedRadio above.
// ─────────────────────────────────────────────────────────────────────────────
class BssFilterXYPad : public juce::Component,
                       private juce::AudioProcessorValueTreeState::Listener,
                       private juce::Timer
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
    void timerCallback() override;
    void parentHierarchyChanged() override;
    void updateDotFromApvts();
    void writeParamsFromDot();

    float hzToNorm (float hz) const;
    float normToHz (float n)  const;

    juce::AudioProcessorValueTreeState& mAvts;
    juce::String mCutoffID, mResID;
    juce::Colour mDotColour;

    std::atomic<float> mDotX { 0.9f };  // normalized, 0=20Hz 1=20kHz
    std::atomic<float> mDotY { 0.0f };  // normalized, 0=res=0 (bottom), 1=res=1 (top)
    std::atomic<bool>  mNeedsRepaint { false };
};
