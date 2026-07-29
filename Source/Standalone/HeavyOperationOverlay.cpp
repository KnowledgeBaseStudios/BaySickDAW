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
        mTicker.clear();
        mProgress = indeterminate ? -1.0f : 0.0f;
        mPulse    = 0.0f;

        promoteToDesktop();

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

void HeavyOperationOverlay::pushTicker (const juce::String& label)
{
    // Skip blanks and consecutive repeats -- several call sites re-set the same
    // label, and a ticker that repeats itself reads as a stuck load.
    if (label.isEmpty() || (! mTicker.isEmpty() && mTicker.strings.getLast() == label))
        return;

    mTicker.add (label);
    while (mTicker.size() > kTickerKeep)
        mTicker.remove (0);
}

void HeavyOperationOverlay::setStep (int stepIndex, int stepCount, const juce::String& label)
{
    mStepLabel = label;
    mProgress  = stepCount > 0
               ? juce::jlimit (0.0f, 1.0f, (float) (stepIndex - 1) / (float) stepCount)
               : -1.0f;
    pushTicker (label);
    pumpPaint();
}

void HeavyOperationOverlay::setStepLabel (const juce::String& label)
{
    mStepLabel = label;
    pushTicker (label);
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
    returnToHost();

    if (mCursorShown)
    {
        juce::MouseCursor::hideWaitCursor();
        mCursorShown = false;
    }
}

void HeavyOperationOverlay::promoteToDesktop()
{
    // Remember who owns us before addToDesktop detaches us from them.
    if (mHost == nullptr)
        if (auto* p = getParentComponent())
            mHost = p;

    if (mHost == nullptr)
        return;

    // Cover the editor exactly.  Screen space, because a desktop window's
    // bounds are screen-relative -- unlike the contained windows, which are
    // child peers measured in parent-client space.
    const auto area = mHost->getScreenBounds();

    if (! isOnDesktop())
    {
        setAlwaysOnTop (true);
        // windowIsTemporary: no taskbar button, and it does not become the
        // app's main window while a load is running.
        addToDesktop (juce::ComponentPeer::windowIsTemporary);

        // FRAMEWORK QUIRK, verified in the vendored JUCE 8.0.12
        // (juce_Windowing_windows.cpp): the Direct2D render context implements
        // performAnyPendingRepaintsNow() as an EMPTY function (:5214), while the
        // Software Renderer context implements it properly (:4897, invalidates
        // then pumps WM_PAINT).  JUCE 8 defaults to Direct2D, which silently
        // voids this overlay's entire reason for existing -- it holds the
        // message thread, so without a working synchronous pump it paints once
        // and then sits frozen at 0% for the whole load, which is exactly what
        // Jeff saw.  This panel is flat 2D with no images or effects, so
        // software rendering costs nothing, and it is the ONLY surface in the
        // app that has to paint while the message thread is blocked.
        //
        // Matched by NAME rather than by index: the order of
        // contextDescriptorList is an implementation detail that a JUCE update
        // could reorder, and picking the wrong one here fails silently.
        if (auto* peer = getPeer())
        {
            const auto engines = peer->getAvailableRenderingEngines();
            const int  idx     = engines.indexOf ("Software Renderer");
            if (idx >= 0)
                peer->setCurrentRenderingEngine (idx);
        }
    }

    setBounds (area);
}

void HeavyOperationOverlay::returnToHost()
{
    if (! isOnDesktop())
        return;

    removeFromDesktop();

    // Back to being an ordinary hidden child, so the editor's teardown owns it
    // the same way it always did and nothing is left parented to the desktop.
    if (mHost != nullptr)
    {
        mHost->addChildComponent (this);
        setBounds (mHost->getLocalBounds());
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
    // Only meaningful while we are an ordinary child; on the desktop our bounds
    // are screen-relative and are set by promoteToDesktop.
    if (! isOnDesktop())
        if (auto* p = getParentComponent())
            setBounds (p->getLocalBounds());
}

void HeavyOperationOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.55f));

    auto full = getLocalBounds();
    const int pw = juce::jmin (520, juce::jmax (240, full.getWidth() - 40));

    // Height grows with however much ticker there is to show, so a short op
    // still gets a compact panel instead of a mostly-empty box.
    const int tickerRows = juce::jmin (kTickerDraw, mTicker.size());
    const int panelH     = juce::jmin (juce::jmax (110, full.getHeight() - 40),
                                       110 + tickerRows * 17);
    auto panel = juce::Rectangle<int> (pw, panelH).withCentre (full.getCentre());

    g.setColour (VC::Panel);
    g.fillRoundedRectangle (panel.toFloat(), 8.0f);
    g.setColour (VC::Accent);
    g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 8.0f, 1.0f);

    auto inner = panel.reduced (20, 16);

    g.setColour (VC::Text);
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    {
        // Percentage sits on the title row, right-aligned (Jeff 2026-07-28: a
        // long load needs to say how far along it is, not just that it is
        // busy).  Only shown when the op is determinate -- an indeterminate
        // op has no honest number to print, and inventing one is worse than
        // showing none.
        auto titleRow = inner.removeFromTop (26);
        if (mProgress >= 0.0f)
        {
            const int pct = juce::jlimit (0, 100, juce::roundToInt (mProgress * 100.0f));
            g.drawText (juce::String (pct) + "%", titleRow.removeFromRight (56),
                        juce::Justification::centredRight, false);
            titleRow.removeFromRight (8);
        }
        g.drawText (mTitle, titleRow, juce::Justification::centredLeft, true);
    }

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

    // Running ticker: the tail of everything loaded so far, oldest at the top,
    // fading back so the eye lands on the most recent line.  The current step
    // is already drawn above it, so the tail EXCLUDES the last entry to avoid
    // printing the same string twice.
    if (mTicker.size() > 1)
    {
        inner.removeFromTop (6);
        g.setFont (juce::Font (12.0f));

        const int avail = inner.getHeight() / 17;
        const int count = juce::jmin (juce::jmin (kTickerDraw, avail), mTicker.size() - 1);

        for (int i = count; i >= 1; --i)
        {
            const auto& line = mTicker.strings.getReference (mTicker.size() - 1 - i);
            // Oldest visible line is faintest; nearest to current is clearest.
            const float a = juce::jmap ((float) (count - i), 0.0f, (float) juce::jmax (1, count - 1),
                                        0.30f, 0.75f);
            g.setColour (VC::TextDim.withMultipliedAlpha (a));
            g.drawText (line, inner.removeFromTop (17), juce::Justification::centredLeft, true);
        }
    }
}
