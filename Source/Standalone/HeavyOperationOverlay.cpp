#include "HeavyOperationOverlay.h"
#include "SharedUI.h"

HeavyOperationOverlay::HeavyOperationOverlay()
{
    setVisible (false);
    setAlwaysOnTop (true);
    setInterceptsMouseClicks (true, true);
    setMouseCursor (juce::MouseCursor (juce::MouseCursor::WaitCursor));
}

HeavyOperationOverlay::~HeavyOperationOverlay()
{
    if (mCursorShown)
        juce::MouseCursor::hideWaitCursor();
}

void HeavyOperationOverlay::beginOp (const juce::String& title, bool indeterminate)
{
    if (++mDepth == 1)
    {
        mTitle = title;
        mStepLabel.clear();
        mProgress = indeterminate ? -1.0f : 0.0f;
        mPulse    = 0.0f;

        if (auto* p = getParentComponent())
            setBounds (p->getLocalBounds());

        setVisible (true);
        toFront (false);

        if (! mCursorShown)
        {
            juce::MouseCursor::showWaitCursor();
            mCursorShown = true;
        }
    }
    else
    {
        mStepLabel = title;
    }

    pumpPaint();
}

void HeavyOperationOverlay::setStep (int stepIndex, int stepCount, const juce::String& label)
{
    mStepLabel = label;
    mProgress  = stepCount > 0
               ? juce::jlimit (0.0f, 1.0f, (float) (stepIndex - 1) / (float) stepCount)
               : -1.0f;
    pumpPaint();
}

void HeavyOperationOverlay::setStepLabel (const juce::String& label)
{
    mStepLabel = label;
    pumpPaint();
}

void HeavyOperationOverlay::endOp()
{
    if (mDepth == 0)
        return;

    if (--mDepth > 0)
    {
        pumpPaint();
        return;
    }

    setVisible (false);

    if (mCursorShown)
    {
        juce::MouseCursor::hideWaitCursor();
        mCursorShown = false;
    }
}

void HeavyOperationOverlay::pumpPaint()
{
    if (! isVisible())
        return;

    mPulse += 0.08f;
    repaint();

    // The op holding the message thread means the normal paint dispatch never
    // runs until it finishes; force the pending WM_PAINT through the peer now.
    if (auto* peer = getPeer())
        peer->performAnyPendingRepaintsNow();
}

void HeavyOperationOverlay::parentSizeChanged()
{
    if (auto* p = getParentComponent())
        setBounds (p->getLocalBounds());
}

void HeavyOperationOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.55f));

    auto full = getLocalBounds();
    const int pw = juce::jmin (440, juce::jmax (240, full.getWidth() - 40));
    auto panel = juce::Rectangle<int> (pw, 130).withCentre (full.getCentre());

    g.setColour (VC::Panel);
    g.fillRoundedRectangle (panel.toFloat(), 8.0f);
    g.setColour (VC::Accent);
    g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 8.0f, 1.0f);

    auto inner = panel.reduced (20, 16);

    g.setColour (VC::Text);
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.drawText (mTitle, inner.removeFromTop (26), juce::Justification::centredLeft, true);

    inner.removeFromTop (8);
    auto track = inner.removeFromTop (14);

    g.setColour (VC::Surface);
    g.fillRoundedRectangle (track.toFloat(), 7.0f);

    g.setColour (VC::Blue);

    if (mProgress >= 0.0f)
    {
        auto fill = track.toFloat().withWidth (juce::jmax (14.0f, track.getWidth() * mProgress));
        g.fillRoundedRectangle (fill, 7.0f);
    }
    else
    {
        const float segW  = (float) track.getWidth() * 0.28f;
        const float sweep = std::fmod (mPulse, 1.0f) * ((float) track.getWidth() + segW) - segW;
        auto seg = juce::Rectangle<float> ((float) track.getX() + sweep, (float) track.getY(),
                                           segW, (float) track.getHeight())
                       .getIntersection (track.toFloat());
        if (! seg.isEmpty())
            g.fillRoundedRectangle (seg, 7.0f);
    }

    inner.removeFromTop (8);
    g.setColour (VC::TextDim);
    g.setFont (juce::Font (14.0f));
    g.drawText (mStepLabel, inner.removeFromTop (20), juce::Justification::centredLeft, true);
}
