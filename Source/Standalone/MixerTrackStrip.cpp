#include "MixerTrackStrip.h"

namespace
{
    // dB range for the level fader.
    // 2026-04-30: max dropped +10 → +5.6 dB (FL-parity ish - FL caps at +5.6).
    constexpr float kFaderMin = -60.0f;
    constexpr float kFaderMax =   5.6f;
    constexpr float kFaderDef =   0.0f;

    // Row heights.
    // 2026-04-30: Pan row tightened 36→28, Width row tightened 28→24
    // (saves 12 px total for the meter to grow into).
    constexpr int kNameH    = 20;
    constexpr int kMeterH   = 80;   // unused now - meter is right-column flex
    constexpr int kMSH      = 20;   // Mute / Solo row
    constexpr int kPanH     = 28;   // was 36
    constexpr int kFaderH   = 100;
    constexpr int kDbH      = 16;
    constexpr int kPadV     = 4;    // vertical padding between rows

    // 2026-04-30: meter is a 28 px wide vertical L/R bar in the strip's
    // right column (Jeff's spec).  Bar spans from below the name label down
    // to above the dB-readout label, so the strip top + bottom regions show
    // the strip's own colour, framing the meter as one piece.
    constexpr int kMeterColW = 28;
    constexpr int kMeterColGap = 2;   // 2 px between controls column and meter

    // 2026-04-30: fader cap shrunk 57 → 35 (kGuard becomes 17.5 each side).
    // Used for C-overlap: dB label can overlap the fader's bottom kGuard
    // (dB label's top y = fader bottom y - kFaderBottomGuard).
    constexpr int kFaderBottomGuard = 18;   // round(35/2) = 17.5 → 18 px
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
    // QA-RustyMeter Task 5 (2026-05-30): tooltip on ALL strips shows the full
    // (possibly truncated) name, plus the rename hint where editable.
    refreshNameTooltip();
    mNameLabel.onTextChange = [this] {
        refreshNameTooltip();
        if (onNameChanged) onNameChanged(mNameLabel.getText());
    };
    addAndMakeVisible(mNameLabel);

    // QA-RustyMeter Task 5 (2026-05-30): bus collapse/expand arrow (Bus strips
    // only; positioned in resized() right of the shrunk name label).  Fires
    // onCollapseToggled with this strip's channel id (set later via setChannelId).
    if (type == StripType::Bus)
    {
        mCollapseBtn.onClick = [this] { if (onCollapseToggled) onCollapseToggled(mChannelId); };
        mCollapseBtn.setTooltip("Collapse / expand this bus's channel strips");
        addAndMakeVisible(mCollapseBtn);
    }

    // ── DBFSMeter ─────────────────────────────────────────────────────────────
    mMeter.setCompact(true);
    // QA-RustyMeter (2026-05-30): every non-master strip gets the Split layout
    // (peak bar + scrolling RMS waveform).  Part 1 = insert/channel strips; part 2
    // (2026-05-30) flips the Bus strips too (their per-bus RMS is now published).
    // Only Master keeps a full-height peak bar -- it carries the LUFS box instead.
    mMeter.setMeterLayout (type == StripType::Master
                           ? DBFSMeter::Layout::Full : DBFSMeter::Layout::Split);
    addAndMakeVisible(mMeter);

    // QA-RustyMeter Task 3 (2026-05-30): master strip carries the LUFS box
    // (between width knob + fader, positioned in resized()).  Master only.
    if (type == StripType::Master)
        addAndMakeVisible(mLufsBox);

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
    // 2026-04-30: percent-style popup display on hover/drag (FL parity).
    // setPopupDisplayEnabled enables the floating popup, textFromValueFunction
    // formats it as "L 42%", "Center", "R 17%".  Without this, the strip's
    // pan knob silently spun with no value shown anywhere on screen.
    mPanKnob.setPopupDisplayEnabled(true, true, nullptr);
    mPanKnob.textFromValueFunction = [](double v)
    {
        if (std::abs(v) < 0.005) return juce::String("Center");
        return (v < 0 ? juce::String("L ") : juce::String("R "))
             + juce::String((int) std::round(std::abs(v) * 100.0)) + "%";
    };
    mPanKnob.updateText();
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
    // 2026-04-30: percent popup display.  Width range 0..2 maps to 0%..200%
    // (1.0 = unchanged stereo).  Same rationale as Pan above.
    mWidthKnob.setPopupDisplayEnabled(true, true, nullptr);
    mWidthKnob.textFromValueFunction = [](double v)
    {
        return juce::String((int) std::round(v * 100.0)) + "%";
    };
    mWidthKnob.updateText();
    addAndMakeVisible(mWidthKnob);

    // ── 5F-4a: Arm LED (insert strips only) ─────────────────────────────────
    mArmBtn.setButtonText("A");
    mArmBtn.setClickingTogglesState(true);
    mArmBtn.setOnColour(juce::Colour(0xffff4444));   // red
    mArmBtn.setTooltip("Arm for recording");
    if (hasArm()) addAndMakeVisible(mArmBtn);
    else          addChildComponent(mArmBtn);

    // R4 (2026-04-23): Listen LED - Vox / Inst only.  Headphones glyph,
    // ButtonAttachment-driven (no custom click handler).
    mListenBtn.setClickingTogglesState(true);
    mListenBtn.setColours(juce::Colour(0xff707070), juce::Colour(0xff33ff88));
    mListenBtn.setTooltip("Listen: hear this input through the bus + master");
    if (hasArm()) addAndMakeVisible(mListenBtn);
    else          addChildComponent(mListenBtn);

    // ── 5F-4a: FX Bypass LED (all strip types - master/bus/insert) ──────────
    mBypassBtn.setButtonText("FX Bypass");
    mBypassBtn.setClickingTogglesState(true);
    mBypassBtn.setOnColour(juce::Colour(0xff4488ff));   // blue
    mBypassBtn.setTooltip("Bypass entire effects rack (preserves slot settings)");
    addAndMakeVisible(mBypassBtn);

    // Master FX Bypass LED - visible only on the Master strip. Purple to match
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
    // QA-E Task 5 (2026-05-15): manual parameter listener removed -- arm
    // visual sync now comes free from ButtonAttachment.
}

// ─────────────────────────────────────────────────────────────────────────────
// 5F-4a: bind new controls (polarity/width/arm/bypass) to APVTS params.
// Safe to call repeatedly - tears down previous attachments first.
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

    // 5F-4a Batch 6: level/pan/mute/solo - all strip types (InsertNode reads these)
    if (apvts.getParameter(paramPrefix + "_level") != nullptr)
        mLevelAtt = std::make_unique<SliderAtt>(apvts, paramPrefix + "_level", mFader);
    if (apvts.getParameter(paramPrefix + "_pan") != nullptr)
        mPanAtt   = std::make_unique<SliderAtt>(apvts, paramPrefix + "_pan",   mPanKnob);
    if (apvts.getParameter(paramPrefix + "_mute") != nullptr)
        mMuteAtt  = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_mute", mMuteBtn);
    if (apvts.getParameter(paramPrefix + "_solo") != nullptr)
        mSoloAtt  = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_solo", mSoloBtn);

    // Width - always present
    if (apvts.getParameter(paramPrefix + "_width") != nullptr)
        mWidthAtt = std::make_unique<SliderAtt>(apvts, paramPrefix + "_width", mWidthKnob);

    // Polarity - bus + insert only. Single-button attachment.
    if (hasPolarityRow() && apvts.getParameter(paramPrefix + "_polarity") != nullptr)
    {
        mPolarityAtt = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_polarity", mPolarityBtn);
    }

    // Arm.  QA-E Task 5 (2026-05-15): left-click is direct _arm toggle via
    // ButtonAttachment for ALL strip types.  Vox/Inst additionally get a
    // right-click hook that opens the input-channel picker WITHOUT arming
    // -- lets users pick + monitor a channel without committing to record
    // (the prior R2/C3 flow tied channel selection to arming, which blocked
    // monitoring inputs while no strip was armed; surfaced by QA-E Task 5
    // master-mix-fallback verify).  New flow:
    //   * Left-click  -> toggle _arm (uses whichever channel is in _inputChannelIdx)
    //   * Right-click -> open picker -> writes _inputChannelIdx only (no _arm)
    if (hasUtilityRow() && apvts.getParameter(paramPrefix + "_arm") != nullptr
        && ! mNoLiveInput)   // K-2: skip arm wiring entirely on sfizz-source strips
    {
        mArmAtt = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_arm", mArmBtn);

        if (mType == StripType::Vox || mType == StripType::Inst)
        {
            // Read mChannelId at click time - setChannelId runs AFTER setApvts
            // so a captured value would be stale (-1).
            mArmBtn.onRightClick = [this]
            {
                if (onArmRequested) onArmRequested (mChannelId);
            };
        }
    }

    // Bypass - insert only (canonical store; EffectsPage button reads/writes the same param)
    if (hasUtilityRow() && apvts.getParameter(paramPrefix + "_bypass") != nullptr)
        mBypassAtt = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_bypass", mBypassBtn);

    // Master FX Bypass - Master strip only. Global app-wide kill-all flag.
    if (mType == StripType::Master && apvts.getParameter("master_fx_bypass") != nullptr)
        mMasterFXBypassAtt = std::make_unique<ButtonAtt>(apvts, "master_fx_bypass", mMasterFXBypassBtn);

    // R4: Listen - Vox / Inst only.  K-2 (2026-05-05): skip on sfizz-source
    // strips (mNoLiveInput) - listen has no meaning when there's no live input.
    if ((mType == StripType::Vox || mType == StripType::Inst)
        && ! mNoLiveInput
        && apvts.getParameter(paramPrefix + "_listen") != nullptr)
    {
        mListenAtt = std::make_unique<ButtonAtt>(apvts, paramPrefix + "_listen", mListenBtn);
    }
}

// K-2 (2026-05-05): toggle the noLiveInput suppression.  When true, hides the
// arm + listen LEDs (sfizz-source Inst strips don't have a live input - the
// engine IS the source) and tears down the `_arm` parameter listener so APVTS
// changes don't drive a hidden button.  When false (default), the strip
// behaves exactly as before - Vox/Inst type strips show arm + listen LEDs.
void MixerTrackStrip::setNoLiveInput (bool b)
{
    if (b == mNoLiveInput) return;
    mNoLiveInput = b;

    // Visibility of arm + listen LEDs follows hasArm().
    const bool show = hasArm();
    mArmBtn.setVisible (show);
    mListenBtn.setVisible (show);

    // QA-E Task 5 (2026-05-15): the C3 (2026-05-04) parameter-listener
    // teardown is no longer needed -- ButtonAttachment is the only path now
    // and setApvts() rebuilds it.  Hidden LEDs simply ignore APVTS changes.

    if (getWidth() > 0) resized();
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
    // QA-Eg: smoothing now lives in DBFSMeter (delta-time attack + release).
    // Cable overlay reads the smoothed value via getCurrentPeakDb -> mMeter.
    mMeter.setLevel(dBFS);
}

void MixerTrackStrip::setStereoLevel(float dBFS_L, float dBFS_R)
{
    mMeter.setStereoLevel(dBFS_L, dBFS_R);
}

void MixerTrackStrip::setRmsStereo(float dBFS_L, float dBFS_R)
{
    mMeter.setRmsStereo(dBFS_L, dBFS_R);   // QA-RustyMeter split-meter RMS top half
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

    // Accent top bar in the strip's color.
    // 2026-04-30: alpha 0.85 → 1.0 + bar height 2 → 3 px so the top stripe
    // matches the brightness of the bus → first-member neon divider.  The
    // divider draws at full alpha with a glow halo (line.bright path in
    // ScrollContent::paintOverChildren), so the strip top used to look
    // dim by comparison.  Same alpha now reads as one piece.
    g.setColour(mAccent);
    g.fillRect(b.getX(), b.getY(), b.getWidth(), 3);

    // Right border separator
    g.setColour(VC::Bg);
    g.fillRect(b.getRight() - 1, b.getY(), 1, b.getHeight());

    // 5F-4b B5: cable socket - neon green ring with black interior
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

    // 2026-04-30: tick marks are now drawn INSIDE DBFSMeter::paint (tick
    // labels overlay unlit segments and get covered by lit segments - FL
    // parity).  The strip used to draw them in an 18 px column to the
    // left of the meter, but with the meter now in the strip's right
    // column there's no room for an external tick column anyway.
}

// 2026-04-30: TWO-COLUMN layout per Jeff's spec.
//   Top:    name label (full strip width)
//   Middle: LEFT controls column + RIGHT 28 px meter column
//   Bottom: dB label (full width, overlapping fader's bottom kGuard) + socket
// The meter spans the full middle height (top to dB-label-top), and shows
// strip background above (under the name) and below (around dB label +
// socket) so it reads as one piece per Jeff's spec.
void MixerTrackStrip::resized()
{
    auto outer = getLocalBounds();

    // Row heights specific to controls column.
    constexpr int kPolH   = 18;   // polarity row
    constexpr int kWidthH = 24;   // 2026-04-30 tightened from 28
    constexpr int kUtilH  = 20;   // arm + bypass row
    constexpr int kSocketRowH = 18;
    constexpr int kSocketDiam = 12;

    const bool polRow    = hasPolarityRow();
    const bool utilRow   = hasUtilityRow();
    const bool masterRow = (mType == StripType::Master);

    // ── Top: name label (+ bus collapse arrow on the right) ─────────────────
    {
        auto top = outer.removeFromTop(kNameH).reduced(3, 0);
        // QA-RustyMeter Task 5 (2026-05-30): Bus strips reserve a small arrow on
        // the right of the name (tab-dropdown style); the name shrinks to fit.
        if (mType == StripType::Bus)
            mCollapseBtn.setBounds(top.removeFromRight(14).reduced(0, 2));
        mNameLabel.setBounds(top);
    }
    outer.removeFromTop(kPadV);

    // ── Bottom: socket row + "+" send button, full strip width ──────────────
    {
        auto socketRow = outer.removeFromBottom(kSocketRowH);
        const int socketCx = socketRow.getX() + 3 + kSocketDiam / 2;
        const int socketCy = socketRow.getY() + kSocketRowH / 2;
        mSocketCentre = { socketCx, socketCy };
        mAddSendBtn.setBounds(socketRow.getX() + kSocketDiam + 8, socketRow.getY(),
                              socketRow.getWidth() - kSocketDiam - 11, kSocketRowH);
    }
    outer.removeFromBottom(kPadV);

    // `outer` is now the MIDDLE area between name+pad on top and socket+pad
    // on bottom.  Reserve dB-label height at the bottom of the middle area
    // and split the remaining middle into LEFT controls + RIGHT meter cols.
    const int middleBottom = outer.getBottom();
    const int dbLabelTopY  = middleBottom - kDbH;

    // Right column: 28 px wide meter, spanning from middle-top to
    // dB-label-top (so the meter's bottom edge lines up with the dB label's
    // top edge).
    auto meterCol = outer.removeFromRight(kMeterColW);
    mMeter.setBounds(meterCol.getX(), meterCol.getY(),
                     meterCol.getWidth(), dbLabelTopY - meterCol.getY());

    // Tiny gap between controls column and meter.
    outer.removeFromRight(kMeterColGap);

    // Controls column lives in `outer` minus 3 px side padding.
    auto controls = outer.reduced(3, 0);
    int y = controls.getY();
    int w = controls.getWidth();
    int x = controls.getX();

    // ── Mute / Solo LED row ─────────────────────────────────────────────────
    {
        const int btnW = (w - 2) / 2;
        mMuteBtn.setBounds(x,            y, btnW,         kMSH);
        mSoloBtn.setBounds(x + btnW + 2, y, w - btnW - 2, kMSH);
    }
    y += kMSH + kPadV;

    // ── FX (Effects-page jump) button ───────────────────────────────────────
    mFXBtn.setBounds(x, y, w, 18);
    y += 18 + kPadV;

    // ── Utility row: Arm + Listen + Bypass (Vox/Inst), or Bypass full ───────
    if (utilRow)
    {
        if (hasArm())
        {
            const int third = (w - 4) / 3;
            mArmBtn   .setBounds(x,                   y, third,                kUtilH);
            mListenBtn.setBounds(x + third + 2,       y, third,                kUtilH);
            mBypassBtn.setBounds(x + (third + 2) * 2, y, w - (third + 2) * 2,  kUtilH);
        }
        else
        {
            mBypassBtn.setBounds(x, y, w, kUtilH);
        }
        y += kUtilH + kPadV;
    }

    // ── Master FX Bypass row (Master strip only) ────────────────────────────
    if (masterRow)
    {
        mMasterFXBypassBtn.setBounds(x, y, w, kUtilH);
        y += kUtilH + kPadV;
    }

    // ── Pan knob ────────────────────────────────────────────────────────────
    {
        const int panSize = juce::jmin(w, kPanH);
        mPanKnob.setBounds(x + (w - panSize) / 2, y, panSize, kPanH);
    }
    y += kPanH + kPadV;

    // ── Polarity row (Bus + Insert only) ────────────────────────────────────
    if (polRow)
    {
        mPolarityBtn.setBounds(x, y, w, kPolH);
        y += kPolH + kPadV;
    }

    // ── Width knob ──────────────────────────────────────────────────────────
    {
        const int widthSize = juce::jmin(w, kWidthH);
        mWidthKnob.setBounds(x + (w - widthSize) / 2, y, widthSize, kWidthH);
    }
    y += kWidthH + kPadV;

    // ── Master LUFS box (Master strip only) — between width knob and fader ───
    // Stacked value-over-title layout (spec #1, confirmed by Jeff 2026-05-30 over
    // #9's ~18-20 px single-line note): taller box for the LUFS value on top +
    // the full mode title underneath.  Per Jeff, the fader thumb may overlap it
    // above unity (>0 dB); acceptable.
    if (masterRow)
    {
        constexpr int kLufsH = 30;
        mLufsBox.setBounds(x, y, w, kLufsH);
        y += kLufsH + kPadV;
    }

    // ── Fader ───────────────────────────────────────────────────────────────
    // C-overlap: fader's BOUNDS extend kFaderBottomGuard (~18 px) below the
    // dB label's TOP y so the dB label visually sits inside the fader's
    // bottom kGuard zone (which is empty unless the cap is at -60 dB).  The
    // visible track still ends at dB-label-top because the LookAndFeel only
    // paints the track between the kGuard zones.
    {
        const int faderBottomY = dbLabelTopY + kFaderBottomGuard;
        const int faderH       = juce::jmax(40, faderBottomY - y);
        mFader.setBounds(x, y, w, faderH);
    }

    // ── dB label (full strip width, overlaps fader's bottom kGuard) ─────────
    {
        const int stripX = getLocalBounds().getX();
        const int stripW = getLocalBounds().getWidth();
        mDbLabel.setBounds(stripX + 3, dbLabelTopY, stripW - 6, kDbH);
    }
}

// G-7 (2026-04-29): right-click on a strip's empty area pops a context menu.
// Only fires when onContextMenuRequested is wired (MixerPage wires it on Aux
// strips and secondary Vox/Inst bus strips).  Left-clicks fall through to the
// strip's child components (faders / knobs / buttons / etc.) untouched.
void MixerTrackStrip::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onContextMenuRequested)
    {
        onContextMenuRequested (e.getScreenPosition());
        return;
    }
    juce::Component::mouseDown (e);
}
