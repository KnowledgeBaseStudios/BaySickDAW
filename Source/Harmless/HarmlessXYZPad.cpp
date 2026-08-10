#include "HarmlessXYZPad.h"
#include "HarmlessLAF.h"

static void makeRotary (juce::Slider& s) {
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
}

HarmlessXYZPad::HarmlessXYZPad()
{
    for (auto* s : { &mXKnob, &mYKnob, &mZKnob })
        { makeRotary (*s); s->setRange (-1.0, 1.0); addAndMakeVisible (*s); }
    // Listen to knob drags so the pad dot follows. Z is not displayed on the
    // pad but we still capture changes for consistency.
    mXKnob.addListener (this);
    mYKnob.addListener (this);
}

void HarmlessXYZPad::sliderValueChanged (juce::Slider* s)
{
    if (s == &mXKnob) mX = juce::jlimit (-1.f, 1.f, (float) mXKnob.getValue());
    if (s == &mYKnob) mY = juce::jlimit (-1.f, 1.f, (float) mYKnob.getValue());
    repaint();
}

void HarmlessXYZPad::attachToApvts (juce::AudioProcessorValueTreeState& apvts,
                                     const juce::String& xId, const juce::String& yId,
                                     const juce::String& zId)
{
    // QA-ApvtsAutomation Task 5 follow-up: attached but never stamped, so the
    // three mod knobs had no Automate menu.  QA-ModelShell TS3: stamping is all
    // this does now -- the applicator is registered model-side against the
    // engine APVTS and survives this pad.
    auto wire = [&apvts] (const juce::String& id, VibeSlider& knob,
                          std::unique_ptr<SliderAtt>& att)
    {
        if (id.isEmpty()) return;
        att = std::make_unique<SliderAtt> (apvts, id, knob);
        knob.setComponentID (id);
    };
    wire (xId, mXKnob, mXAtt);
    wire (yId, mYKnob, mYAtt);
    wire (zId, mZKnob, mZAtt);
}

void HarmlessXYZPad::paint (juce::Graphics& g)
{
    // Pad background
    g.setColour (juce::Colour (HarmlessLAF::kPanel));
    g.fillRoundedRectangle (mPadBounds.toFloat(), 3.f);
    g.setColour (juce::Colour (HarmlessLAF::kBorder));
    g.drawRoundedRectangle (mPadBounds.toFloat().reduced (0.5f), 3.f, 1.f);

    // Grid lines
    g.setColour (juce::Colour (HarmlessLAF::kBorder));
    const float cx = mPadBounds.getCentreX(), cy = mPadBounds.getCentreY();
    g.drawLine (float(mPadBounds.getX()), cy, float(mPadBounds.getRight()), cy, 0.5f);
    g.drawLine (cx, float(mPadBounds.getY()), cx, float(mPadBounds.getBottom()), 0.5f);

    // Dot
    const float dotX = cx + mX * (mPadBounds.getWidth()  * 0.5f - 4.f);
    const float dotY = cy - mY * (mPadBounds.getHeight() * 0.5f - 4.f);
    g.setColour (juce::Colour (HarmlessLAF::kAccent).withAlpha (0.3f));
    g.fillEllipse (dotX - 8.f, dotY - 8.f, 16.f, 16.f);
    g.setColour (juce::Colour (HarmlessLAF::kAccent));
    g.fillEllipse (dotX - 4.f, dotY - 4.f, 8.f, 8.f);

    // Labels
    g.setColour (juce::Colour (HarmlessLAF::kTextDim));
    g.setFont (juce::Font (8.f));
    g.drawText ("X", mXKnob.getX(), mXKnob.getBottom() + 2, mXKnob.getWidth(), 10, juce::Justification::centred);
    g.drawText ("Y", mYKnob.getX(), mYKnob.getBottom() + 2, mYKnob.getWidth(), 10, juce::Justification::centred);
    g.drawText ("Z", mZKnob.getX(), mZKnob.getBottom() + 2, mZKnob.getWidth(), 10, juce::Justification::centred);
    g.drawText ("MOD", mPadBounds.getX() + 4, mPadBounds.getY() + 4, 40, 12, juce::Justification::centredLeft);
}

void HarmlessXYZPad::resized()
{
    const int kKnob = 36, kGap = 4;
    const int padH = getHeight() - kKnob - kGap - 14;
    mPadBounds = getLocalBounds().withHeight (padH);

    const int totalKnobW = kKnob * 3 + kGap * 2;
    int kx = (getWidth() - totalKnobW) / 2;
    const int ky = padH + kGap;
    mXKnob.setBounds (kx,             ky, kKnob, kKnob); kx += kKnob + kGap;
    mYKnob.setBounds (kx,             ky, kKnob, kKnob); kx += kKnob + kGap;
    mZKnob.setBounds (kx,             ky, kKnob, kKnob);
}

void HarmlessXYZPad::mouseDown (const juce::MouseEvent& e) { updateFromMouse (e); }
void HarmlessXYZPad::mouseDrag (const juce::MouseEvent& e) { updateFromMouse (e); }

void HarmlessXYZPad::updateFromMouse (const juce::MouseEvent& e)
{
    if (!mPadBounds.contains (e.getPosition())) return;
    const float cx = mPadBounds.getCentreX(), cy = mPadBounds.getCentreY();
    const float hw = mPadBounds.getWidth()  * 0.5f - 4.f;
    const float hh = mPadBounds.getHeight() * 0.5f - 4.f;
    if (hw <= 0.f || hh <= 0.f) return;
    mX = juce::jlimit (-1.f, 1.f, (float(e.x) - cx) / hw);
    mY = juce::jlimit (-1.f, 1.f, -(float(e.y) - cy) / hh);
    mXKnob.setValue (double(mX), juce::sendNotificationAsync);
    mYKnob.setValue (double(mY), juce::sendNotificationAsync);
    repaint();
}
