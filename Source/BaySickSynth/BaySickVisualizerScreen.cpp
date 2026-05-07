#include "BaySickVisualizerScreen.h"
#include "BaySickSynthLAF.h"
#include <cmath>

// Params subscribed for repaint (must match list in ~BaySickVisualizerScreen)
static const char* const kVisualizerParams[] = {
    "waveform","modifier",
    "amp_attack","amp_decay","amp_sustain","amp_release",
    "flt_type","flt_cutoff","flt_res","flt_env_amt",
    "flt_attack","flt_decay","flt_sustain","flt_release",
    "lfo_shape","lfo_rate",
    nullptr
};

BaySickVisualizerScreen::BaySickVisualizerScreen (juce::Colour ledColour)
    : mLedColour (ledColour)
{
    startTimerHz (30);
}

BaySickVisualizerScreen::~BaySickVisualizerScreen()
{
    if (mApvts != nullptr)
        for (int i = 0; kVisualizerParams[i] != nullptr; ++i)
            mApvts->removeParameterListener (mPrefix + kVisualizerParams[i], this);
    stopTimer();
}

void BaySickVisualizerScreen::setup (juce::AudioProcessorValueTreeState& apvts,
                                      const juce::String& prefix)
{
    mApvts  = &apvts;
    mPrefix = prefix;
    for (int i = 0; kVisualizerParams[i] != nullptr; ++i)
        apvts.addParameterListener (prefix + kVisualizerParams[i], this);
}

void BaySickVisualizerScreen::setActiveTab (int tab)
{
    mActiveTab = juce::jlimit (0, 4, tab);
    repaint();
}

void BaySickVisualizerScreen::parameterChanged (const juce::String&, float)
{
    repaint();
}

void BaySickVisualizerScreen::timerCallback()
{
    if (mApvts != nullptr)
    {
        const float rate = [&]() -> float {
            // Prefer the processor's sync-aware effective rate when wired up
            // so the LFO animation tracks tempo-sync.
            if (mEffectiveLfoRate != nullptr)
                return mEffectiveLfoRate->load();
            if (auto* p = mApvts->getRawParameterValue (mPrefix + "lfo_rate"))
                return p->load();
            return 1.0f;
        }();
        mLFOPhase += rate / 30.0f;
        if (mLFOPhase >= 1.0f) mLFOPhase -= 1.0f;
    }
    if (mActiveTab == 4) repaint();
}

//==============================================================================
void BaySickVisualizerScreen::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (BaySickSynthLAF::kBgMain).darker (0.5f));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (juce::Colour (0xFF1A1C1F));
    for (int i = 1; i < 4; ++i)
    {
        const float y = bounds.getY() + bounds.getHeight() * float (i) / 4.0f;
        g.drawHorizontalLine ((int) y, bounds.getX(), bounds.getRight());
    }
    g.drawVerticalLine ((int) bounds.getCentreX(), bounds.getY(), bounds.getBottom());

    const auto content = bounds.reduced (6.0f, 4.0f);

    if (mApvts == nullptr)
    {
        g.setColour (juce::Colour (0xFF333333));
        g.setFont   (juce::Font (11.0f));
        g.drawText  ("No engine", bounds, juce::Justification::centred);
        g.setColour (juce::Colour (0xFF2A2C30));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
        return;
    }

    auto getf = [&] (const char* name) -> float {
        if (auto* p = mApvts->getRawParameterValue (mPrefix + name)) return p->load();
        return 0.f;
    };

    switch (mActiveTab)
    {
        case 0:  paintOscTab (g, content); break;
        case 1:
        {
            const float a = getf ("amp_attack"),  d = getf ("amp_decay"),
                        s = getf ("amp_sustain"), r = getf ("amp_release");
            paintEnvTab (g, content, a, d, s, r);
            break;
        }
        case 2:  paintFilterTab (g, content); break;
        case 3:
        {
            const float a = getf ("flt_attack"),  d = getf ("flt_decay"),
                        s = getf ("flt_sustain"), r = getf ("flt_release");
            paintEnvTab (g, content, a, d, s, r);
            break;
        }
        case 4:  paintLFOTab (g, content); break;
        default: break;
    }

    g.setColour (juce::Colour (0xFF2A2C30));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

//==============================================================================
void BaySickVisualizerScreen::paintOscTab (juce::Graphics& g,
                                            juce::Rectangle<float> bounds)
{
    if (mApvts == nullptr) return;

    auto getf = [&] (const char* name) -> float {
        if (auto* p = mApvts->getRawParameterValue (mPrefix + name)) return p->load();
        return 0.f;
    };
    const int   waveform = juce::jlimit (0, 10, (int) getf ("waveform"));
    const float modifier = getf ("modifier");  // 0..1

    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float hw = bounds.getWidth()  * 0.45f;
    const float hh = bounds.getHeight() * 0.38f;

    const int kPts = 256;
    juce::Path path;

    for (int i = 0; i <= kPts; ++i)
    {
        const float t  = (float) i / kPts;   // 0..1 one cycle
        float y = 0.0f;

        switch (waveform)
        {
            case 0: // SAW
                y = 1.0f - 2.0f * t;
                break;
            case 1: // SAW+SAW - two slightly offset saws summed
            {
                float offset = modifier * 0.04f;
                float t2 = std::fmod (t + offset, 1.0f);
                y = ((1.0f - 2.0f * t) + (1.0f - 2.0f * t2)) * 0.5f;
                break;
            }
            case 2: // PULSE
            {
                float pw = juce::jlimit (0.05f, 0.95f, modifier);
                y = (t < pw) ? 0.85f : -0.85f;
                break;
            }
            case 3: // SAW+SQUARE - average of saw and square
                y = ((1.0f - 2.0f * t) + ((t < 0.5f) ? 1.0f : -1.0f)) * 0.5f;
                break;
            case 4: // SQUARE+SQUARE
                y = (t < 0.5f) ? 0.85f : -0.85f;
                break;
            case 5: // SUPERSAW - show as fat/fuzzy saw
            {
                float offset1 = modifier * 0.015f;
                float offset2 = modifier * 0.025f;
                float t1 = std::fmod (t + offset1, 1.0f);
                float t2 = std::fmod (t + offset2, 1.0f);
                float t3 = std::fmod (t - offset1 + 1.0f, 1.0f);
                y = ((1.0f - 2.0f * t)  + (1.0f - 2.0f * t1) +
                     (1.0f - 2.0f * t2) + (1.0f - 2.0f * t3)) * 0.25f;
                break;
            }
            case 6: // BELL - FM modulated sine
            {
                const float beta = modifier * 6.0f; // display-range FM depth
                y = std::sin (juce::MathConstants<float>::twoPi * t
                              + beta * std::sin (juce::MathConstants<float>::twoPi * t * 2.0f));
                y *= 0.7f;
                break;
            }
            case 7: // DEAF SAW - rounded / filtered saw
            {
                // Simple approximation: Fourier series low-pass (first 3 harmonics)
                float fc = 0.1f + modifier * modifier * 0.8f; // "brightness"
                y = std::sin (juce::MathConstants<float>::twoPi * t) * fc;
                y += std::sin (juce::MathConstants<float>::twoPi * t * 2.0f) * fc * 0.5f;
                y += std::sin (juce::MathConstants<float>::twoPi * t * 3.0f) * fc * 0.33f;
                y *= 0.9f;
                break;
            }
            case 8: // SPREAD OCT - root + oct up + oct down (3 saws)
            {
                float mix = modifier;
                float s1 = 1.0f - 2.0f * t;
                float s2 = 1.0f - 2.0f * std::fmod (t * 2.0f, 1.0f);
                float s3 = 1.0f - 2.0f * std::fmod (t * 0.5f, 1.0f);
                y = s1 * (1.0f - mix * 0.5f) + (s2 + s3) * mix * 0.25f;
                break;
            }
            case 9: // SPREAD 5TH - root + 5th up + 5th down
            {
                float mix = modifier;
                float s1 = 1.0f - 2.0f * t;
                float s2 = 1.0f - 2.0f * std::fmod (t * 1.5f, 1.0f);
                float s3 = 1.0f - 2.0f * std::fmod (t * (2.0f / 3.0f), 1.0f);
                y = s1 * (1.0f - mix * 0.5f) + (s2 + s3) * mix * 0.25f;
                break;
            }
            case 10: // SINE (P3.2) - pure sine
                y = std::sin (juce::MathConstants<float>::twoPi * t);
                break;
            default: y = 0.0f; break;
        }

        const float px = bounds.getX() + t * bounds.getWidth();
        const float py = cy - y * hh;
        if (i == 0) path.startNewSubPath (px, py);
        else        path.lineTo          (px, py);
    }

    g.setColour (mLedColour.withAlpha (0.20f));
    g.strokePath (path, juce::PathStrokeType (5.0f));
    g.setColour (mLedColour.withAlpha (0.45f));
    g.strokePath (path, juce::PathStrokeType (3.0f));
    g.setColour (mLedColour);
    g.strokePath (path, juce::PathStrokeType (1.5f));

    // Waveform name label
    static const char* const kNames[] = {
        "SAW","SAW+SAW","PULSE","SAW+SQUARE","SQUARE+SQUARE",
        "SUPERSAW","BELL","DEAF SAW","SPREAD OCT","SPREAD 5TH","SINE"
    };
    g.setColour (mLedColour.withAlpha (0.5f));
    g.setFont   (juce::Font (9.0f));
    g.drawText  (kNames[waveform], bounds.removeFromBottom (13.0f),
                 juce::Justification::centred);
}

//==============================================================================
void BaySickVisualizerScreen::paintEnvTab (juce::Graphics& g,
                                            juce::Rectangle<float> bounds,
                                            float a, float d, float s, float r)
{
    drawADSR (g, bounds, a, d, s, r, mLedColour);
}

void BaySickVisualizerScreen::drawADSR (juce::Graphics& g,
                                         juce::Rectangle<float> bounds,
                                         float a, float d, float s, float r,
                                         juce::Colour colour)
{
    const float totalTime = a + d + r + 0.05f;
    const float scale     = bounds.getWidth() / totalTime;

    const float bx = bounds.getX();
    const float by = bounds.getY();
    const float bh = bounds.getHeight();

    const float xA = bx + a * scale;
    const float xD = xA + d * scale;
    const float xS = xD + 0.05f * scale;
    const float xR = xS + r * scale;

    const float yTop     = by + 2.0f;
    const float yBottom  = by + bh - 2.0f;
    const float ySustain = yTop + (1.0f - s) * (yBottom - yTop);

    juce::Path env;
    env.startNewSubPath (bx, yBottom);
    env.lineTo (xA, yTop);
    env.lineTo (xD, ySustain);
    env.lineTo (xS, ySustain);
    env.lineTo (xR, yBottom);

    juce::Path fill = env;
    fill.lineTo (xR, yBottom);
    fill.lineTo (bx, yBottom);
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.08f));
    g.fillPath  (fill);

    g.setColour (colour.withAlpha (0.20f));
    g.strokePath (env, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (colour.withAlpha (0.50f));
    g.strokePath (env, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (colour);
    g.strokePath (env, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    g.setColour (colour.withAlpha (0.50f));
    g.setFont   (juce::Font (9.0f));
    g.drawText ("A", juce::Rectangle<float> (bx, yBottom - 14.f, xA - bx, 12.f),
                juce::Justification::centred);
    g.drawText ("D", juce::Rectangle<float> (xA, yBottom - 14.f, xD - xA, 12.f),
                juce::Justification::centred);
    g.drawText ("S", juce::Rectangle<float> (xD, yBottom - 14.f, xS - xD, 12.f),
                juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (xS, yBottom - 14.f, xR - xS, 12.f),
                juce::Justification::centred);
}

//==============================================================================
void BaySickVisualizerScreen::paintFilterTab (juce::Graphics& g,
                                               juce::Rectangle<float> bounds)
{
    if (mApvts == nullptr) return;

    auto getf = [&] (const char* name) -> float {
        if (auto* p = mApvts->getRawParameterValue (mPrefix + name)) return p->load();
        return 0.f;
    };

    const int   filterType = (int) getf ("flt_type");
    const float cutoff     = juce::jlimit (20.f, 20000.f, getf ("flt_cutoff"));
    const float res        = getf ("flt_res");
    const float q          = 0.707f + res * (10.0f - 0.707f);

    const int   kPts  = (int) bounds.getWidth();
    const float kDbLo = -48.0f;
    const float kDbHi =  24.0f;

    juce::Path curve;
    bool first = true;
    for (int i = 0; i < kPts; ++i)
    {
        const float freq = 20.0f * std::pow (1000.0f, (float) i / (float) (kPts - 1));
        const float u    = freq / cutoff;
        const float mag  = svfMagnitude (u, q, filterType);
        const float db   = (mag > 1e-6f) ? 20.0f * std::log10 (mag) : kDbLo;
        const float dbC  = juce::jlimit (kDbLo, kDbHi, db);

        const float px = bounds.getX() + (float) i;
        const float py = bounds.getBottom()
                         - (dbC - kDbLo) / (kDbHi - kDbLo) * bounds.getHeight();

        if (first) { curve.startNewSubPath (px, py); first = false; }
        else        curve.lineTo           (px, py);
    }

    const float yZero = bounds.getBottom() - (0.0f - kDbLo) / (kDbHi - kDbLo) * bounds.getHeight();
    g.setColour (juce::Colour (0xFF303333));
    g.drawHorizontalLine ((int) yZero, bounds.getX(), bounds.getRight());

    g.setColour (mLedColour.withAlpha (0.18f));
    g.strokePath (curve, juce::PathStrokeType (5.0f));
    g.setColour (mLedColour.withAlpha (0.45f));
    g.strokePath (curve, juce::PathStrokeType (2.5f));
    g.setColour (mLedColour);
    g.strokePath (curve, juce::PathStrokeType (1.5f));

    const float cutoffX = bounds.getX()
        + bounds.getWidth() * std::log (cutoff / 20.0f) / std::log (20000.0f / 20.0f);
    g.setColour (mLedColour.withAlpha (0.35f));
    g.drawVerticalLine ((int) cutoffX, bounds.getY(), bounds.getBottom());
}

//==============================================================================
void BaySickVisualizerScreen::paintLFOTab (juce::Graphics& g,
                                            juce::Rectangle<float> bounds)
{
    if (mApvts == nullptr) return;

    auto getf = [&] (const char* name) -> float {
        if (auto* p = mApvts->getRawParameterValue (mPrefix + name)) return p->load();
        return 0.f;
    };
    // Only 3 shapes now: 0=Sine, 1=Saw, 2=Square
    const int lfoShape = juce::jlimit (0, 2, (int) getf ("lfo_shape"));

    const float cy   = bounds.getCentreY();
    const float hh   = bounds.getHeight() * 0.38f;
    const int   kPts = (int) bounds.getWidth();

    juce::Path wave;
    for (int i = 0; i <= kPts; ++i)
    {
        const float t     = (float) i / kPts;
        const float phase = std::fmod (mLFOPhase + t * 2.0f, 1.0f);
        const float y     = lfoSample (lfoShape, phase);

        const float px = bounds.getX() + (float) i;
        const float py = cy - y * hh;
        if (i == 0) wave.startNewSubPath (px, py);
        else        wave.lineTo          (px, py);
    }

    const juce::Colour cyan (BaySickSynthLAF::kCyan);
    g.setColour (cyan.withAlpha (0.20f));
    g.strokePath (wave, juce::PathStrokeType (5.0f));
    g.setColour (cyan.withAlpha (0.45f));
    g.strokePath (wave, juce::PathStrokeType (2.5f));
    g.setColour (cyan);
    g.strokePath (wave, juce::PathStrokeType (1.5f));

    static const char* const kShapeNames[] = { "Sine", "Saw", "Square" };
    g.setColour (cyan.withAlpha (0.6f));
    g.setFont   (juce::Font (10.0f));
    g.drawText  (kShapeNames[lfoShape],
                 bounds.removeFromBottom (14.0f),
                 juce::Justification::centred);
}

//==============================================================================
float BaySickVisualizerScreen::svfMagnitude (float u, float q, int filterType)
{
    const float u2   = u * u;
    const float uQ   = (q > 1e-4f) ? u / q : 0.0f;
    const float den  = (1.0f - u2) * (1.0f - u2) + uQ * uQ;
    if (den < 1e-12f) return 1.0f;

    switch (filterType)
    {
        case 0:  return 1.0f / std::sqrt (den);
        case 1:  return (u2 * u2 > 0.0f) ? u2 / std::sqrt (den) : 0.0f;
        case 2:  return uQ / std::sqrt (den);
        case 3:  return std::abs (1.0f - u2) / std::sqrt (den);
        default: return 1.0f;
    }
}

float BaySickVisualizerScreen::lfoSample (int shape, float phase)
{
    switch (shape)
    {
        case 0:  return std::sin (juce::MathConstants<float>::twoPi * phase);
        case 1:  return 2.0f * phase - 1.0f;   // Saw Up
        case 2:  return (phase < 0.5f) ? 1.0f : -1.0f;
        default: return 0.0f;
    }
}
