#pragma once
#include <JuceHeader.h>
#include "SharedUI.h"

// ── MetroPanel ────────────────────────────────────────────────────────────────
// Floating settings panel for the metronome. Shown via CallOutBox when the
// ▾ arrow button next to METRO is clicked.
//
// Controls:
//   Sound    : ComboBox — Sine / Click / Wood / Bell
//   Volume   : Slider 0–200 % (0.0–2.0 internally)
//   Precount : Toggle — fires a 1-bar lead-in click before recording starts
//              when both record-arm AND this toggle are on (D-5 2026-04-26).
// ─────────────────────────────────────────────────────────────────────────────
class MetroPanel : public juce::Component
{
public:
    MetroPanel()
    {
        auto makeLabel = [&](const juce::String& text) {
            auto lbl = std::make_unique<juce::Label>("", text);
            lbl->setFont(juce::Font(11.f));
            lbl->setColour(juce::Label::textColourId, VC::TextDim);
            addAndMakeVisible(*lbl);
            return lbl;
        };

        // Title
        mTitleLabel = std::make_unique<juce::Label>("", "Metronome");
        mTitleLabel->setFont(juce::Font(13.f, juce::Font::bold));
        mTitleLabel->setColour(juce::Label::textColourId, VC::Text);
        addAndMakeVisible(*mTitleLabel);

        // Sound row
        mSoundLabel = makeLabel("Sound:");
        mSoundCombo = std::make_unique<juce::ComboBox>();
        mSoundCombo->addItem("Sine",  1);
        mSoundCombo->addItem("Click", 2);
        mSoundCombo->addItem("Wood",  3);
        mSoundCombo->addItem("Bell",  4);
        mSoundCombo->setSelectedId(1, juce::dontSendNotification);
        mSoundCombo->onChange = [this] {
            if (onSoundChanged) onSoundChanged(mSoundCombo->getSelectedId() - 1);
        };
        addAndMakeVisible(*mSoundCombo);

        // Volume row
        mVolLabel  = makeLabel("Volume:");
        mVolSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                    juce::Slider::TextBoxRight);
        mVolSlider->setRange(0.0, 2.0, 0.01);
        mVolSlider->setValue(0.7, juce::dontSendNotification);
        mVolSlider->textFromValueFunction = [](double v) {
            return juce::String((int)(v * 100.0)) + "%";
        };
        mVolSlider->valueFromTextFunction = [](const juce::String& t) {
            return t.getDoubleValue() / 100.0;
        };
        mVolSlider->setColour(juce::Slider::textBoxTextColourId, VC::Text);
        mVolSlider->onValueChange = [this] {
            if (onVolumeChanged) onVolumeChanged((float)mVolSlider->getValue());
        };
        addAndMakeVisible(*mVolSlider);

        // Precount toggle (D-5 2026-04-26): single bool, always 1 bar lead-in.
        mPrecountLabel = makeLabel("Precount:");
        mPrecountToggle = std::make_unique<juce::ToggleButton>("1-bar lead-in when recording (Ctrl+P)");
        mPrecountToggle->setColour(juce::ToggleButton::textColourId, VC::Text);
        mPrecountToggle->onClick = [this] {
            if (onPrecountChanged) onPrecountChanged(mPrecountToggle->getToggleState());
        };
        addAndMakeVisible(*mPrecountToggle);

        setSize(240, 122);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(8);

        mTitleLabel->setBounds(b.removeFromTop(18));
        b.removeFromTop(4);

        auto layoutRow = [&](juce::Label* lbl, juce::Component* ctrl, int ctrlW = 0) {
            auto row = b.removeFromTop(22);
            lbl->setBounds(row.removeFromLeft(58));
            if (ctrlW > 0) ctrl->setBounds(row.removeFromLeft(ctrlW));
            else            ctrl->setBounds(row);
            b.removeFromTop(4);
        };

        layoutRow(mSoundLabel.get(),   mSoundCombo.get(),   130);
        layoutRow(mVolLabel.get(),     mVolSlider.get());
        layoutRow(mPrecountLabel.get(),mPrecountToggle.get());
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(VC::Panel);
        g.setColour(VC::Accent.withAlpha(0.6f));
        g.drawRect(getLocalBounds().toFloat(), 1.f);
    }

    // Initialise with current state
    void setVolume (float v)  { if (mVolSlider)  mVolSlider->setValue(v, juce::dontSendNotification); }
    void setSound  (int t)    { if (mSoundCombo) mSoundCombo->setSelectedId(t + 1, juce::dontSendNotification); }
    void setPrecountEnabled (bool on)
    {
        if (mPrecountToggle) mPrecountToggle->setToggleState(on, juce::dontSendNotification);
    }

    // Callbacks wired by StandaloneEditor
    std::function<void(float)> onVolumeChanged;
    std::function<void(int)>   onSoundChanged;
    std::function<void(bool)>  onPrecountChanged;   // D-5 (2026-04-26)

private:
    std::unique_ptr<juce::Label>        mTitleLabel;
    std::unique_ptr<juce::Label>        mSoundLabel;
    std::unique_ptr<juce::ComboBox>     mSoundCombo;
    std::unique_ptr<juce::Label>        mVolLabel;
    std::unique_ptr<juce::Slider>       mVolSlider;
    std::unique_ptr<juce::Label>        mPrecountLabel;
    std::unique_ptr<juce::ToggleButton> mPrecountToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetroPanel)
};
