#include "BssEditorComponents.h"
#include "BaySickSynthLAF.h"
#include "../Standalone/UndoBracket.h"
#include <cmath>

//==============================================================================
// ── BssLedRadio ───────────────────────────────────────────────────────────────

BssLedRadio::BssLedRadio (juce::AudioProcessorValueTreeState& avts,
                           const juce::String& paramID,
                           const juce::StringArray& labels,
                           juce::Colour ledColour)
    : mAvts (avts), mParamID (paramID), mLabels (labels), mLedColour (ledColour)
{
    if (auto* p = mAvts.getRawParameterValue (mParamID))
        mSelected.store (juce::jlimit (0, mLabels.size() - 1, (int) p->load()));

    mAvts.addParameterListener (mParamID, this);
}

BssLedRadio::BssLedRadio (juce::AudioProcessorValueTreeState& avts,
                           const juce::String& paramID,
                           const juce::StringArray& labels,
                           int rows, int cols,
                           juce::Colour ledColour)
    : mAvts (avts), mParamID (paramID), mLabels (labels), mLedColour (ledColour),
      mRows (juce::jmax (1, rows)), mCols (juce::jmax (1, cols))
{
    if (auto* p = mAvts.getRawParameterValue (mParamID))
        mSelected.store (juce::jlimit (0, mLabels.size() - 1, (int) p->load()));

    mAvts.addParameterListener (mParamID, this);
}

BssLedRadio::~BssLedRadio()
{
    mAvts.removeParameterListener (mParamID, this);
    stopTimer();
}

void BssLedRadio::parentHierarchyChanged()
{
    if (getPeer() != nullptr) { if (! isTimerRunning()) startTimerHz (30); }
    else if (isTimerRunning()) stopTimer();
}

void BssLedRadio::parameterChanged (const juce::String&, float newValue)
{
    mSelected.store (juce::jlimit (0, mLabels.size() - 1, (int) newValue));
    mNeedsRepaint.store (true);
}

void BssLedRadio::timerCallback()
{
    if (mNeedsRepaint.exchange (false))
        repaint();
}

void BssLedRadio::paint (juce::Graphics& g)
{
    const int n = mLabels.size();
    if (n == 0) return;

    const int selectedIdx = mSelected.load();

    // Grid mode (2026-04-22): Harmless-VEL-style buttons spread across the top
    // of the component in a single row (or rows for multi-row grids), with
    // space between each button. Height fixed (~20 px), width fills evenly.
    if (mRows > 0)
    {
        const int cellH  = 20;
        const int hGap   = 6;   // horizontal space between buttons
        const int vGap   = 4;   // between rows (multi-row only)
        const int topY   = 2;   // top padding inside component
        const int totalW = getWidth();
        const int cellW  = juce::jmax (10, (totalW - hGap * (mCols + 1)) / mCols);

        const juce::Colour offFill   (0xFF2A2C30);
        const juce::Colour onFill   = mLedColour.withAlpha (0.18f).overlaidWith (offFill);
        const juce::Colour offBorder (0xFF404040);

        for (int i = 0; i < n; ++i)
        {
            const int col = i % mCols;
            const int row = i / mCols;
            const juce::Rectangle<float> r (
                (float) (hGap + col * (cellW + hGap)),
                (float) (topY + row * (cellH + vGap)),
                (float) cellW,
                (float) cellH);

            const bool sel = (i == selectedIdx);

            g.setColour (sel ? onFill : offFill);
            g.fillRoundedRectangle (r, 2.0f);

            g.setColour (sel ? mLedColour.withAlpha (0.4f) : offBorder);
            g.drawRoundedRectangle (r, 2.0f, 1.0f);

            if (sel)
            {
                g.setColour (mLedColour);
                g.fillRoundedRectangle (r.getX() + 2.0f, r.getY(),
                                         r.getWidth() - 4.0f, 2.0f, 1.0f);
            }

            g.setColour (sel ? mLedColour : juce::Colour (0xFF909090));
            g.setFont   (juce::Font (10.0f, juce::Font::bold));
            g.drawFittedText (mLabels[i], r.toNearestInt().reduced (2, 0),
                              juce::Justification::centred, 1);
        }
        return;
    }

    // Legacy vertical LED-list mode
    const float itemH = (float) getHeight() / (float) n;
    const float ledR  = 5.0f;
    const float ledCX = ledR + 4.0f;

    for (int i = 0; i < n; ++i)
    {
        const float y  = (float) i * itemH;
        const float cy = y + itemH * 0.5f;
        const bool  sel = (i == selectedIdx);

        const juce::Colour ledFill = sel ? mLedColour : juce::Colour (0xFF333537);
        const juce::Colour ledRim  = sel ? mLedColour.brighter (0.3f) : juce::Colour (0xFF4A4C50);

        g.setColour (ledFill.withAlpha (0.25f));
        g.fillEllipse (ledCX - ledR - 2.0f, cy - ledR - 2.0f,
                       (ledR + 2.0f) * 2.0f, (ledR + 2.0f) * 2.0f);
        g.setColour (ledFill);
        g.fillEllipse (ledCX - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f);
        g.setColour (ledRim);
        g.drawEllipse (ledCX - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f, 1.0f);

        g.setColour (sel ? juce::Colour (0xFFE8E8E8) : juce::Colour (0xFF888A90));
        g.setFont   (juce::Font (11.0f, sel ? juce::Font::bold : juce::Font::plain));
        g.drawText  (mLabels[i],
                     (int) (ledCX + ledR + 5.0f), (int) y,
                     getWidth() - (int) (ledCX + ledR + 5.0f), (int) itemH,
                     juce::Justification::centredLeft);
    }
}

void BssLedRadio::mouseDown (const juce::MouseEvent& e)
{
    const int n = mLabels.size();
    if (n == 0) return;

    int clicked = 0;
    if (mRows > 0)
    {
        // Hit-test matches paint() geometry (hGap left/right, row cells across top).
        const int cellH  = 20;
        const int hGap   = 6;
        const int vGap   = 4;
        const int topY   = 2;
        const int cellW  = juce::jmax (10, (getWidth() - hGap * (mCols + 1)) / mCols);

        const int col = juce::jlimit (0, mCols - 1, (e.x - hGap) / juce::jmax (1, (cellW + hGap)));
        const int row = juce::jlimit (0, mRows - 1, (e.y - topY) / juce::jmax (1, (cellH + vGap)));
        clicked = juce::jlimit (0, n - 1, row * mCols + col);
    }
    else
    {
        clicked = juce::jlimit (0, n - 1, (int) ((float) e.y / (float) getHeight() * (float) n));
    }

    mSelected.store (clicked);
    beginParamUndoGesture (mAvts, mParamID); // Task 6 (12-iv)
    if (auto* raw = dynamic_cast<juce::RangedAudioParameter*> (mAvts.getParameter (mParamID)))
        raw->setValueNotifyingHost (raw->getNormalisableRange().convertTo0to1 ((float) clicked));

    repaint();
}

//==============================================================================
// ── BssFilterXYPad ────────────────────────────────────────────────────────────

BssFilterXYPad::BssFilterXYPad (juce::AudioProcessorValueTreeState& avts,
                                  const juce::String& cutoffID,
                                  const juce::String& resID,
                                  juce::Colour dotColour)
    : mAvts (avts), mCutoffID (cutoffID), mResID (resID), mDotColour (dotColour)
{
    mAvts.addParameterListener (cutoffID, this);
    mAvts.addParameterListener (resID,    this);
    updateDotFromApvts();
}

BssFilterXYPad::~BssFilterXYPad()
{
    mAvts.removeParameterListener (mCutoffID, this);
    mAvts.removeParameterListener (mResID,    this);
    stopTimer();
}

void BssFilterXYPad::parentHierarchyChanged()
{
    if (getPeer() != nullptr) { if (! isTimerRunning()) startTimerHz (30); }
    else if (isTimerRunning()) stopTimer();
}

void BssFilterXYPad::parameterChanged (const juce::String&, float)
{
    updateDotFromApvts();
    mNeedsRepaint.store (true);
}

void BssFilterXYPad::timerCallback()
{
    if (mNeedsRepaint.exchange (false))
        repaint();
}

void BssFilterXYPad::updateDotFromApvts()
{
    if (auto* pc = mAvts.getRawParameterValue (mCutoffID))
        mDotX.store (hzToNorm (pc->load()));
    if (auto* pr = mAvts.getRawParameterValue (mResID))
        mDotY.store (1.0f - juce::jlimit (0.0f, 1.0f, pr->load()));
}

float BssFilterXYPad::hzToNorm (float hz) const
{
    return std::log (hz / 20.0f) / std::log (20000.0f / 20.0f);
}

float BssFilterXYPad::normToHz (float n) const
{
    return 20.0f * std::pow (20000.0f / 20.0f, juce::jlimit (0.0f, 1.0f, n));
}

void BssFilterXYPad::writeParamsFromDot()
{
    const float hz  = normToHz (mDotX.load());
    const float res = 1.0f - mDotY.load();

    if (auto* pc = dynamic_cast<juce::RangedAudioParameter*> (mAvts.getParameter (mCutoffID)))
        pc->setValueNotifyingHost (pc->getNormalisableRange().convertTo0to1 (hz));

    if (auto* pr = dynamic_cast<juce::RangedAudioParameter*> (mAvts.getParameter (mResID)))
        pr->setValueNotifyingHost (pr->getNormalisableRange().convertTo0to1 (res));
}

void BssFilterXYPad::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (juce::Colour (0xFF0F1012));
    g.fillRoundedRectangle (b, 4.0f);

    g.setColour (juce::Colour (0xFF1E2026));
    for (int i = 1; i < 4; ++i)
    {
        const float y = b.getY() + b.getHeight() * (float) i / 4.0f;
        const float x = b.getX() + b.getWidth()  * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
        g.drawVerticalLine   ((int) x, b.getY(), b.getBottom());
    }

    // Reserve space for axis labels
    auto inner = b;
    auto bottomLabel = inner.removeFromBottom (11.0f);
    auto leftLabel   = inner.removeFromLeft   (13.0f);

    g.setColour (juce::Colour (0xFF555760));
    g.setFont   (juce::Font (9.0f));
    g.drawText  ("CUTOFF", bottomLabel, juce::Justification::centred);
    g.drawText  ("RES",    leftLabel,   juce::Justification::centredTop);

    // Dot position
    const float dotX = inner.getX() + mDotX.load() * inner.getWidth();
    const float dotY = inner.getY() + mDotY.load() * inner.getHeight();

    // Crosshairs
    g.setColour (mDotColour.withAlpha (0.20f));
    g.drawVerticalLine   ((int) dotX, inner.getY(), inner.getBottom());
    g.drawHorizontalLine ((int) dotY, inner.getX(), inner.getRight());

    // Dot glow + core
    const float dotR = 7.0f;
    g.setColour (mDotColour.withAlpha (0.18f));
    g.fillEllipse (dotX - dotR * 2.0f, dotY - dotR * 2.0f, dotR * 4.0f, dotR * 4.0f);
    g.setColour (mDotColour.withAlpha (0.5f));
    g.fillEllipse (dotX - dotR,        dotY - dotR,        dotR * 2.0f, dotR * 2.0f);
    g.setColour (mDotColour);
    g.fillEllipse (dotX - dotR * 0.5f, dotY - dotR * 0.5f, dotR, dotR);

    g.setColour (juce::Colour (0xFF2A2C30));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
}

void BssFilterXYPad::mouseDown (const juce::MouseEvent& e)
{
    // Account for axis labels on left (13px) and bottom (11px)
    const float lm = 13.0f;
    const float bm = 11.0f;
    const auto  b  = getLocalBounds().toFloat().reduced (0.5f);
    const float iW = b.getWidth()  - lm;
    const float iH = b.getHeight() - bm;
    const float iX = b.getX() + lm;
    const float iY = b.getY();

    mDotX.store (juce::jlimit (0.0f, 1.0f, ((float) e.x - iX) / iW));
    mDotY.store (juce::jlimit (0.0f, 1.0f, ((float) e.y - iY) / iH));
    if (! e.mouseWasDraggedSinceMouseDown())
        beginParamUndoGesture (mAvts, mCutoffID); // Task 6 (12-iv)
    writeParamsFromDot();
    repaint();
}

void BssFilterXYPad::mouseDrag (const juce::MouseEvent& e)
{
    mouseDown (e);
}
