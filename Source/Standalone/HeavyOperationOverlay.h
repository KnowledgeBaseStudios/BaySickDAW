#pragma once
#include <JuceHeader.h>

// Full-parent overlay for heavy synchronous operations (project load, shutdown
// teardown, engine swaps / big preset loads).  Those ops hold the MESSAGE
// thread, so a plain repaint() would never reach the screen until the op
// finished -- every state change here pumps the peer's pending paint
// synchronously (ComponentPeer::performAnyPendingRepaintsNow) so progress is
// visible mid-freeze.  Display-only: the work itself stays synchronous on the
// message thread by design (no off-thread load re-architecture).
class HeavyOperationOverlay : public juce::Component
{
public:
    HeavyOperationOverlay();
    ~HeavyOperationOverlay() override;

    // Nested begin/end pairs are legal (an engine swap inside a project load):
    // an inner beginOp shows as the outer op's step label; only the outermost
    // endOp hides the overlay.
    void beginOp (const juce::String& title, bool indeterminate = false);

    // stepIndex is 1-based ("now starting step i of stepCount"); the bar shows
    // the completed fraction (i-1)/n.  stepCount <= 0 = indeterminate.
    void setStep (int stepIndex, int stepCount, const juce::String& label);
    void setStepLabel (const juce::String& label);
    void endOp();

    bool isActive() const noexcept { return mDepth > 0; }

    void pumpPaint();

    // RAII begin/end pair for the synchronous load-type entry points -- endOp
    // runs on every exit path (early returns included), so the overlay can
    // never stick after a failed load.
    struct ScopedOp
    {
        ScopedOp (HeavyOperationOverlay& o, const juce::String& title,
                  bool indeterminate = false)
            : ov (&o)
        {
            ov->beginOp (title, indeterminate);
        }
        // Null-tolerant: pages pass StandaloneEditor::busyOverlayFor(this),
        // which is null while unparented (project restore runs selectEngine
        // before addChildComponent) -- the whole op is then a no-op.
        ScopedOp (HeavyOperationOverlay* o, const juce::String& title,
                  bool indeterminate = false)
            : ov (o)
        {
            if (ov) ov->beginOp (title, indeterminate);
        }
        ~ScopedOp() { if (ov) ov->endOp(); }
        HeavyOperationOverlay* ov;
        JUCE_DECLARE_NON_COPYABLE (ScopedOp)
    };

    void paint (juce::Graphics& g) override;
    void parentSizeChanged() override;

private:
    juce::String mTitle, mStepLabel;
    float mProgress    = -1.0f;   // < 0 = indeterminate
    float mPulse       = 0.0f;
    int   mDepth       = 0;
    bool  mCursorShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeavyOperationOverlay)
};
