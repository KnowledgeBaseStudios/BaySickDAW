#include "MixerTrackStrip.h"

namespace
{
    // dB range for the level fader
    constexpr float kFaderMin = -60.0f;
    constexpr float kFaderMax =  10.0f;
    constexpr float kFaderDef =   0.0f;

    // Row heights
    constexpr int kNameH    = 20;
    constexpr int kMeterH   = 80;
    constexpr int kMSH      = 20;   // Mute / Solo row
    constexpr int kPanH     = 36;
    constexpr int kFaderH   = 100;
    constexpr int kDbH      = 16;
    constexpr int kPadV     = 4;    // vertical padding between rows
}

MixerTrackStrip::MixerTrackStrip(const juce::String& trackName,
                                 StripType type, juce::Colour accentColor)
    : mType(type), mAccent(accentColor)
{
    // ── Name label ────────────────────────────────────────────────────────────
    mNameLabel.setText(trackName, juce::dontSendNotification);
    mNameLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    mNameLabel.setColour(juce::Label::textColourId, VC::Text);
    mNameLabel.setJustificationType(juce::Justification::centred);
    // 2026-04-24: every user-creatable strip type is renameable.  Names persist
    // via <AuxNames> / <VoxNames> / <InstNames> in UIState.
    bool canRename = (type == StripType::LayerChannel
                      || type == StripType::BassChannel
                      || type == StripType::Aux
                      || type == StripType::Vox
                      || type == StripType::Inst);
    mNameLabel.setEditable(canRename, canRename, false);
    if (canRename) mNameLabel.setTooltip("Double-click to rename");
    mNameLabel.onTextChange = [this] {
        if (onNameChanged) onNameChanged(mNameLabel.getText());
    };
    addAndMakeVisible(mNameLabel);

    // ── DBFSMeter ─────────────────────────────────────────────────────────────
    mMeter.setCompact(true);
    addAndMakeVisible(mMeter);

    // ── Mute LED ──────────────────────────────────────────────────────────────
    mMuteBtn.setButtonText("M");
    mMuteBtn.setClickingTogglesState(true);
    mMuteBtn.setOnColour(juce::Colour(0xffff4444));   // red
    mMuteBtn.setTooltip("Mute");
    mMuteBtn.onClick = [this]
    {
        if (!mUpdating && onMuteChanged)
            onMuteChanged(mMuteBtn.getToggleState());
    };
    addAndMakeVisible(mMuteBtn);

    // ── Solo LED ──────────────────────────────────────────────────────────────
    mSoloBtn.setButtonText("S");
    mSoloBtn.setClickingTogglesState(true);
    mSoloBtn.setOnColour(VC::Yellow);
    mSoloBtn.setTooltip("Solo");
    mSoloBtn.onClick = [this]
    {
        if (!mUpdating && onSoloChanged)
            onSoloChanged(mSoloBtn.getToggleState());
    };
    addAndMakeVisible(mSoloBtn);

    // ── FX Rack button ────────────────────────────────────────────────────────
    // Navigates to the Effects page and selects this strip's rack in the
    // dropdown. Passes the APVTS prefix (unique, stable across renames) rather
    // than the display name, so rename drift can't break the mapping.
    mFXBtn.setColour(juce::TextButton::buttonColourId,  VC::Surface);
    mFXBtn.setColour(juce::TextButton::textColourOffId, VC::Blue);
    mFXBtn.setTooltip("Open Effects Page for this channel's rack");
    mFXBtn.onClick = [this]
    {
        if (onFXClicked)
            onFXClicked(mAutomationPrefix.isNotEmpty() ? mAutomationPrefix
                                                       : mNameLabel.getText());
    };
    addAndMakeVisible(mFXBtn);

    // ── Pan knob ──────────────────────────────────────────────────────────────
    mPanKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mPanKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mPanKnob.setRange(-1.0, 1.0, 0.01);
    mPanKnob.setValue(0.0, juce::dontSendNotification);
    mPanKnob.setDoubleClickReturnValue(true, 0.0);
    mPanKnob.setTooltip("Pan (double-click to center)");
    mPanKnob.onValueChange = [this]
    {
        if (!mUpdating && onPanChanged)
            onPanChanged((float)mPanKnob.getValue());
    };
    addAndMakeVisible(mPanKnob);

    // ── Level fader ───────────────────────────────────────────────────────────
    // Tag as mixer fader so drawLinearSlider can render at 3x width
    mFader.getProperties().set("mixerFader", true);
    mFader.setRange(kFaderMin, kFaderMax, 0.01);
    mFader.setValue(kFaderDef, juce::dontSendNotification);
    mFader.setDoubleClickReturnValue(true, 0.0);
    mFader.setTooltip("Level fader (double-click for 0 dB)");
    mFader.onValueChange = [this]
    {
        if (!mUpdating)
        {
            updateDbLabel();
            if (onFaderChanged)
                onFaderChanged((float)mFader.getValue());
        }
    };
    mFader.addListener(this);
    addAndMakeVisible(mFader);

    mPanKnob.addListener(this);

    // ── dB readout ────────────────────────────────────────────────────────────
    mDbLabel.setFont(juce::Font(10.0f));
    mDbLabel.setColour(juce::Label::textColourId, VC::TextDim);
    mDbLabel.setJustificationType(juce::Justification::centred);
    updateDbLabel();
    addAndMakeVisible(mDbLabel);

    // ── 5F-4a: Polarity single toggle (bus + insert strips only) ─────────────
    mPolarityBtn.setTooltip("Polarity (click to invert)");
    if (hasPolarityRow()) addAndMakeVisible(mPolarityBtn);
    else                  addChildComponent(mPolarityBtn);

    // ── 5F-4a: Width knob (all strips) ───────────────────────────────────────
    mWidthKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mWidthKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mWidthKnob.setRange(0.0, 2.0, 0.01);
    mWidthKnob.setValue(1.0, juce::dontSendNotification);
    mWidthKnob.setDoubleClickReturnValue(true, 1.0);
    mWidthKnob.setTooltip("Stereo width (double-click to reset)");
    addAndMakeVisible(mWidthKnob);

    // ── 5F-4a: Arm LED (insert strips only) ─────────────────────────────────
    mArmBtn.setButtonText("A");
    mArmBtn.setClickingTogglesState(true);
    mArmBtn.setOnColour(juce::Colour(0xffff4444));   // red
    mArmBtn.setTooltip("Arm for recording");
    if (hasArm()) addAndMakeVisible(mArmBtn);
    else          addChildComponent(mArmBtn);

    // R4 (2026-04-23): Listen LED — Vox / Inst only.  Headphones glyph,
    // ButtonAttachment-driven (no custom click handler).
    mListenBtn.setClickingTogglesState(true);
    mListenBtn.setColours(juce::Colour(0xff707070), juce::Colour(0xff33ff88));
    mListenBtn.setTooltip("Listen: hear this input through the bus + master");
    if (hasArm()) addAndMakeVisible(mListenBtn);
    else          addChildComponent(mListenBtn);

    // ── 5F-4a: FX Bypass LED (all strip types — master/bus/insert) ──────────
    mBypassBtn.setButtonText("FX Bypass");
    mBypassBtn.setClickingTogglesState(true);
    mBypassBtn.setOnColour(juce::Colour(0xff4488ff));   // blue
    mBypassBtn.setTooltip("Bypass entire effects rack (preserves slot settings)");
    addAndMakeVisible(mBypassBtn);

    // Master FX Bypass LED — visible only on the Master strip. Purple to match
    // the Mixer ribbon-tab color. Bypasses EVERY rack in the app when on.
    mMasterFXBypassBtn.setButtonText("Master FX Bypass");
    mMasterFXBypassBtn.setClickingTogglesState(true);
    mMasterFXBypassBtn.setOnColour(juce::Colour(0xff7b2fbe));   // purple (Mixer tab)
    mMasterFXBypassBtn.setTooltip("Kill-all: bypass every effects rack in the project");
    if (mType == StripType::Master)
        addAndMakeVisible(mMasterFXBypassBtn);

    // 5F-4b B5: "+" add-send button (all strip types)
    mAddSendBtn.setButtonText("+");
    mAddSendBtn.setColour(juce::TextButton::buttonColourId, VC::Surface);
    mAddSendBtn.setColour(juce::TextButton::textColourOffId, VC::Text);
    mAddSendBtn.setTooltip("Add send cable from this strip");
    mAddSendBtn.onClick = [this]
    {
        if (onAddSendRequested && mChannelId >= 0)
            onAddSendRequested(mChannelId);
    };
    addAndMakeVisible(mAddSendBtn);
}

MixerTrackStrip::~MixerTrackStrip()
{
    mFader.removeListener(this);
    mPanKnob.removeListener(this);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5F-4a: bind new controls (polarity/width/arm/bypass) to APVTS params.
// Safe to call repeatedly — tears down previous attachments first.
// Caller must have registered the params via ensureMixerStripParams() already.
// ─────────────────────────────────────────────────────────────────────────────
void MixerTrackStrip::setApvts(juce::AudioProcessorValueTreeState& apvts,
                                const juce::String& paramPrefix)
{
    // Tear down any previous attachments first
    mLevelAtt    .reset();
    mPanAtt           .reset();
    mMuteAtt          .reset();
    mSoloAtt          .reset();
    mWidthAtt         .reset();
    mPolarityAtt      .reset();
    mArmAtt           .reset();
    mListenAtt        .reset();
    mBypassAtt        .reset();
    mMasterFXBypassAtt.reset();

    // 5F-4a Batch 6: level/pan/mute/solo — all strip types (InsertNode reads these)
    if (apvts.getParameter(paramPrefix + "_level") != nullptr)
        mLevelAtt = std::make_unique<SliderAtt>(apvts, paramPrefix + "_level", mFader);
    if (apvts.getParameter(paramPrefix + "_pan") != nullptr)
        mPanAtt   = std::make_unique<SliderAtt>(apvts, paramPrefix + "_pan",   mPanKnob);
    if (apvts.getParameter(paramPrefix + "_mute") != nullptr)
        mMuteAtt  = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_mute", mMuteBtn);
    if (apvts.getParameter(paramPrefix + "_solo") != nullptr)
        mSoloAtt  = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_solo", mSoloBtn);

    // Width — always present
    if (apvts.getParameter(paramPrefix + "_width") != nullptr)
        mWidthAtt = std::make_unique<SliderAtt>(apvts, paramPrefix + "_width", mWidthKnob);

    // Polarity — bus + insert only. Single-button attachment.
    if (hasPolarityRow() && apvts.getParameter(paramPrefix + "_polarity") != nullptr)
    {
        mPolarityAtt = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_polarity", mPolarityBtn);
    }

    // Arm.  R2 (2026-04-23): for Vox / Inst strips the LED click opens the
    // ASIO input picker (handled by MixerPage via onArmRequested), so we do
    // NOT install a ButtonAttachment - that would auto-toggle _arm and steal
    // the click.  Visual on/off state is driven by MixerPage writing _arm
    // through APVTS + the timer-driven syncFromApvts already in place.  For
    // any other strip type that still has _arm registered (legacy back-compat
    // until R5 hides the LED), keep the old direct-toggle attachment.
    if (hasUtilityRow() && apvts.getParameter(paramPrefix + "_arm") != nullptr)
    {
        if (mType == StripType::Vox || mType == StripType::Inst)
        {
            mArmBtn.setClickingTogglesState (false);   // we own the click
            // Read mChannelId at click time - setChannelId runs AFTER setApvts
            // so a captured value would be stale (-1).
            mArmBtn.onClick = [this]
            {
                if (onArmRequested) onArmRequested (mChannelId);
            };
        }
        else
        {
            mArmAtt = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_arm", mArmBtn);
        }
    }

    // Bypass — insert only (canonical store; EffectsPage button reads/writes the same param)
    if (hasUtilityRow() && apvts.getParameter(paramPrefix + "_bypass") != nullptr)
        mBypassAtt = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_bypass", mBypassBtn);

    // Master FX Bypass — Master strip only. Global app-wide kill-all flag.
    if (mType == StripType::Master && apvts.getParameter("master_fx_bypass") != nullptr)
        mMasterFXBypassAtt = std::make_unique<ButtonAtt>(apvts, "master_fx_bypass", mMasterFXBypassBtn);

    // R4: Listen — Vox / Inst only.
    if ((mType == StripType::Vox || mType == StripType::Inst)
        && apvts.getParameter(paramPrefix + "_listen") != nullptr)
    {
        mListenAtt = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_listen", mListenBtn);
    }
}

// R2 (2026-04-23): Update the Arm LED's tooltip with the currently-selected
// ASIO input channel name (e.g. "Mic 1") or "no input" when unassigned.
// MixerPage calls this whenever the strip's _inputChannelIdx APVTS changes.
void MixerTrackStrip::setInputChannelLabel (const juce::String& label)
{
    if (! hasArm()) return;
    mArmBtn.setTooltip (label.isNotEmpty()
                          ? ("Recording from: " + label + " (click to change)")
                          : "Click to pick an audio input channel");
}

void MixerTrackStrip::sliderDragStarted(juce::Slider* s)
{
    if (s == &mFader   && onFaderDragStarted) onFaderDragStarted();
    if (s == &mPanKnob && onPanDragStarted)   onPanDragStarted();
}

void MixerTrackStrip::sliderDragEnded(juce::Slider* s)
{
    if (s == &mFader   && onFaderDragEnded) onFaderDragEnded();
    if (s == &mPanKnob && onPanDragEnded)   onPanDragEnded();
}

// ─────────────────────────────────────────────────────────────────────────────
void MixerTrackStrip::setAutomationPrefix (const juce::String& prefix)
{
    mAutomationPrefix = prefix;
    mFader.setComponentID   (prefix + "_fader");
    mPanKnob.setComponentID (prefix + "_pan");

    // SafePointer guards against the slider being destroyed between registration and an
    // automation-tick invocation (aux strip removed, mixer rebuild, etc.). Without it, a
    // stale applicator stored in StandaloneEditor would dereference freed memory on the
    // next automation tick and crash inside NormalisableRange::snapToLegalValue.
    // Fader
    {
        juce::Component::SafePointer<juce::Slider> safeSl(&mFader);
        double lo = mFader.getMinimum(), hi = mFader.getMaximum();
        juce::String id = mFader.getComponentID();
        if (VKnobAutomation::sOnRegisterApplicator)
            VKnobAutomation::sOnRegisterApplicator(id, [safeSl, lo, hi](float v01) {
                if (auto* sl = safeSl.getComponent())
                    sl->setValue(lo + v01 * (hi - lo), juce::sendNotification);
            });
        if (VKnobAutomation::sOnRegisterReader)
            VKnobAutomation::sOnRegisterReader(id, [safeSl, lo, hi]() -> float {
                auto* sl = safeSl.getComponent();
                if (!sl) return 0.5f;
                double range = hi - lo;
                return range > 0.0 ? (float)((sl->getValue() - lo) / range) : 0.5f;
            });
    }
    // Pan knob
    {
        juce::Component::SafePointer<juce::Slider> safeSl(&mPanKnob);
        double lo = mPanKnob.getMinimum(), hi = mPanKnob.getMaximum();
        juce::String id = mPanKnob.getComponentID();
        if (VKnobAutomation::sOnRegisterApplicator)
            VKnobAutomation::sOnRegisterApplicator(id, [safeSl, lo, hi](float v01) {
                if (auto* sl = safeSl.getComponent())
                    sl->setValue(lo + v01 * (hi - lo), juce::sendNotification);
            });
        if (VKnobAutomation::sOnRegisterReader)
            VKnobAutomation::sOnRegisterReader(id, [safeSl, lo, hi]() -> float {
                auto* sl = safeSl.getComponent();
                if (!sl) return 0.5f;
                double range = hi - lo;
                return range > 0.0 ? (float)((sl->getValue() - lo) / range) : 0.5f;
            });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void MixerTrackStrip::setAccentColor(juce::Colour c)
{
    if (mAccent != c) { mAccent = c; repaint(); }
}

void MixerTrackStrip::setLevel(float dBFS)
{
    mMeter.setLevel(dBFS);
}

void MixerTrackStrip::setFaderDb(float db, bool notify)
{
    mUpdating = !notify;
    mFader.setValue(juce::jlimit((double)kFaderMin, (double)kFaderMax, (double)db),
                    notify ? juce::sendNotification : juce::dontSendNotification);
    updateDbLabel();
    mUpdating = false;
}

void MixerTrackStrip::setPan(float pan, bool notify)
{
    mUpdating = !notify;
    mPanKnob.setValue(juce::jlimit(-1.0, 1.0, (double)pan),
                      notify ? juce::sendNotification : juce::dontSendNotification);
    mUpdating = false;
}

void MixerTrackStrip::setMuted(bool muted, bool notify)
{
    mUpdating = !notify;
    mMuteBtn.setToggleState(muted, notify ? juce::sendNotification : juce::dontSendNotification);
    mUpdating = false;
}

void MixerTrackStrip::setSoloed(bool soloed, bool notify)
{
    mUpdating = !notify;
    mSoloBtn.setToggleState(soloed, notify ? juce::sendNotification : juce::dontSendNotification);
    mUpdating = false;
}

float MixerTrackStrip::getFaderDb() const
{
    return (float)mFader.getValue();
}

void MixerTrackStrip::updateDbLabel()
{
    float v = (float)mFader.getValue();
    juce::String text;
    if (v <= kFaderMin + 0.5f)
        text = "-inf";
    else
        text = juce::String(v, 1) + " dB";
    mDbLabel.setText(text, juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────────────────────────
void MixerTrackStrip::paint(juce::Graphics& g)
{
    auto b = getLocalBounds();

    // Strip background
    g.setColour(VC::Panel);
    g.fillRoundedRectangle(b.toFloat(), 3.0f);

    // Accent top bar (2px) in the strip's color
    g.setColour(mAccent.withAlpha(0.85f));
    g.fillRect(b.getX(), b.getY(), b.getWidth(), 2);

    // Right border separator
    g.setColour(VC::Bg);
    g.fillRect(b.getRight() - 1, b.getY(), 1, b.getHeight());

    // 5F-4b B5: cable socket — neon green ring with black interior
    {
        constexpr float kSocketDiam = 12.f;
        const float cx = (float) mSocketCentre.x;
        const float cy = (float) mSocketCentre.y;
        const float r  = kSocketDiam * 0.5f;

        // Black interior (recessed hole)
        g.setColour(juce::Colour(0xff0a0a0a));
        g.fillEllipse(cx - r, cy - r, kSocketDiam, kSocketDiam);

        // Neon green ring
        g.setColour(juce::Colour(0xff33ff88));
        g.drawEllipse(cx - r, cy - r, kSocketDiam, kSocketDiam, 1.5f);

        // Subtle inner glow
        g.setColour(juce::Colour(0xff33ff88).withAlpha(0.15f));
        g.drawEllipse(cx - r + 1.f, cy - r + 1.f,
                      kSocketDiam - 2.f, kSocketDiam - 2.f, 1.f);
    }

    // 5F-4a: dBFS tick marks beside the meter (reserved 18px left margin).
    // DBFSMeter compact mode spans -20..0 dBFS (kFloorCmp = -20, top = 0).
    {
        auto mb = mMeter.getBounds();
        if (mb.getHeight() > 30)
        {
            constexpr float kMeterMin = -20.0f;    // matches DBFSMeter::kFloorCmp
            constexpr float kMeterMax =   0.0f;
            static const int kLabels[] = { 0, -3, -6, -9, -12, -15, -20 };

            const int tickAreaX = mb.getX() - 18;
            const int tickX     = mb.getX() - 4;    // tick line end (touches meter)
            g.setColour(VC::TextDim);
            g.setFont(juce::Font(7.0f));

            for (int db : kLabels)
            {
                const float t = (kMeterMax - (float)db) / (kMeterMax - kMeterMin);
                const int y   = mb.getY() + (int)(t * mb.getHeight());
                // Tick line (5 px wide)
                g.drawLine((float)(tickX - 5), (float)y, (float)tickX, (float)y, 1.0f);
                // Label
                g.drawText(juce::String(db), tickAreaX, y - 5, 12, 10,
                           juce::Justification::centredRight);
            }
        }
    }
}

void MixerTrackStrip::resized()
{
    auto b = getLocalBounds().reduced(3, 4);
    int y = b.getY();
    int w = b.getWidth();
    int x = b.getX();

    // 5F-4a extra row heights
    constexpr int kPolH   = 18;   // polarity arrows row
    constexpr int kWidthH = 28;   // width knob row
    constexpr int kUtilH  = 20;   // arm + bypass row

    const bool polRow    = hasPolarityRow();
    const bool utilRow   = hasUtilityRow();
    const bool masterRow = (mType == StripType::Master);   // extra Master FX Bypass row

    // Fixed content (everything except meter and fader — those flex to fill).
    constexpr int kSocketRowH = 18;  // socket circle + "+" button row at bottom
    const int fixedContentH = kNameH + kPadV
                            + kPadV                               // after meter
                            + kMSH + kPadV                        // Mute + Solo
                            + 18 + kPadV                          // FX button
                            + (utilRow ? (kUtilH + kPadV) : 0)    // Arm + Bypass (moved up)
                            + (masterRow ? (kUtilH + kPadV) : 0)  // Master FX Bypass (Master only)
                            + kPanH + kPadV
                            + (polRow ? (kPolH + kPadV) : 0)
                            + kWidthH + kPadV
                            + kPadV                               // after fader
                            + kDbH + kPadV                        // dB label
                            + kSocketRowH;                        // socket row

    // Flex space split between meter (~40%) and fader (~60%).
    const int flexSpace = juce::jmax(120, b.getHeight() - fixedContentH);
    const int meterH    = juce::jlimit(60, 130, flexSpace * 2 / 5);
    const int faderH    = juce::jmax(40, flexSpace - meterH);

    // Name
    mNameLabel.setBounds(x, y, w, kNameH);
    y += kNameH + kPadV;

    // Meter — leave 18px on the left for dB scale (drawn in paint())
    constexpr int kDbTickW = 18;
    mMeter.setBounds(x + kDbTickW, y, w - kDbTickW, meterH);
    y += meterH + kPadV;

    // Mute / Solo LED row
    int btnW = (w - 2) / 2;
    mMuteBtn.setBounds(x,            y, btnW,         kMSH);
    mSoloBtn.setBounds(x + btnW + 2, y, w - btnW - 2, kMSH);
    y += kMSH + kPadV;

    // FX button (jump to Effects page)
    mFXBtn.setBounds(x, y, w, 18);
    y += 18 + kPadV;

    // Utility row — Bypass on all strips; Arm + Listen on Vox / Inst.
    if (hasArm())
    {
        // R4: three columns - Arm | Listen | Bypass.
        const int third = (w - 4) / 3;
        mArmBtn   .setBounds(x,                       y, third,                       kUtilH);
        mListenBtn.setBounds(x + third + 2,           y, third,                       kUtilH);
        mBypassBtn.setBounds(x + (third + 2) * 2,     y, w - (third + 2) * 2,         kUtilH);
    }
    else
    {
        // Master/bus: Bypass LED full width, centered
        mBypassBtn.setBounds(x, y, w, kUtilH);
    }
    y += kUtilH + kPadV;

    // Master FX Bypass LED — Master strip only, below the regular FX Bypass
    if (masterRow)
    {
        mMasterFXBypassBtn.setBounds(x, y, w, kUtilH);
        y += kUtilH + kPadV;
    }

    // Pan knob
    int panSize = juce::jmin(w, kPanH);
    mPanKnob.setBounds(x + (w - panSize) / 2, y, panSize, kPanH);
    y += kPanH + kPadV;

    // Polarity row (bus + insert) — single button, full width
    if (polRow)
    {
        mPolarityBtn.setBounds(x, y, w, kPolH);
        y += kPolH + kPadV;
    }

    // Width knob (all strip types)
    int widthSize = juce::jmin(w, kWidthH);
    mWidthKnob.setBounds(x + (w - widthSize) / 2, y, widthSize, kWidthH);
    y += kWidthH + kPadV;

    // Fader — full width (dB scale is next to the meter, not the fader)
    mFader.setBounds(x, y, w, faderH);
    y += faderH + kPadV;

    // dB label (full width)
    mDbLabel.setBounds(x, y, w, kDbH);
    y += kDbH + kPadV;

    // Socket row: neon green ring (left) + "+" send button (right).
    // The ring is painted in paint(); here we just position the "+" button
    // and store the socket centre for cable rendering.
    constexpr int kSocketH    = 18;
    constexpr int kSocketDiam = 12;
    const int socketCx = x + kSocketDiam / 2 + 3;
    const int socketCy = y + kSocketH / 2;
    mSocketCentre = { socketCx, socketCy };

    mAddSendBtn.setBounds(x + kSocketDiam + 8, y, w - kSocketDiam - 8, kSocketH);
}
