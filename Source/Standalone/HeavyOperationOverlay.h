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
    // Records every distinct step label for the running ticker (Jeff spec
    // 2026-07-28: bar + percent + a running list of what is loading, so a long
    // load reads as progress rather than as a hang).  Bounded -- a project with
    // many tabs would otherwise grow this without limit, and only the tail is
    // ever drawn.
    // QA-ModelShell TS4 (Jeff, 2026-07-28: "I don't see the popup for loading").
    // This overlay is a DRAWN component inside the editor, and the contained
    // windows are NATIVE CHILD PEERS -- an OS window always renders above
    // anything painted into its parent's client area, which is the very reason
    // WorkspaceWindow uses child peers in the first place.  So once windows
    // covered the workspace the overlay was painting underneath them and was
    // simply invisible.  It therefore has to BE a window while it is showing:
    // promoted to its own always-on-top desktop window on the outermost
    // beginOp, and returned to being a child component on the matching endOp.
    void promoteToDesktop();
    void returnToHost();
    juce::Component::SafePointer<juce::Component> mHost;

    void pushTicker (const juce::String& label);
    juce::StringArray mTicker;
    static constexpr int kTickerKeep = 64;   // retained
    static constexpr int kTickerDraw = 8;    // drawn

    juce::String mTitle, mStepLabel;
    float mProgress    = -1.0f;   // < 0 = indeterminate
    float mPulse       = 0.0f;
    int   mDepth       = 0;
    bool  mCursorShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeavyOperationOverlay)
};
