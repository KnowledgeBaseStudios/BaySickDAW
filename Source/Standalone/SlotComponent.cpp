#include "SlotComponent.h"
#include "EffectEditorPanels.h"
#include "../DSP/CompressorDSP.h"
#include "../DSP/SaturationDSP.h"
#include "../DSP/DelayDSP.h"
#include "../DSP/ReverbDSP.h"
#include "../DSP/OverdriveDSP.h"   // I-4: Mode dropdown for Overdrive (Rack vs Pedal)
#include "EffectPresetIO.h"

SlotComponent::SlotComponent(int slotIndex) : mSlotIndex(slotIndex)
{
    setInterceptsMouseClicks(true, true);
    // 2026-05-02: vblank attachment is created lazily in parentHierarchyChanged
    // once the component has a peer.  Old 30 Hz Timer dropped -- ballistics
    // now run on monitor-refresh cadence.

    // C.4 Phase 1 (2026-04-30): SC source dropdown.  Hidden by default;
    // setEditor() shows it only when the loaded effect declares
    // usesSidechain().  Click pops a menu of currently-routed SC lines.
    mScBtn = std::make_unique<juce::TextButton>("SC: Off");
    mScBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b3b3b));
    mScBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd6d6d6));
    mScBtn->setTooltip("Sidechain source");
    mScBtn->setVisible(false);
    mScBtn->onClick = [this] { showScMenu(); };
    addChildComponent(*mScBtn);

    // H-7 (2026-05-01): Mode dropdown for effects with character-mode
    // umbrellas (Compressor: Modern/FET/Opto; Saturation: Tube/Console).
    // Hidden by default; setEditor() shows it for the relevant effect types.
    mModeBtn = std::make_unique<juce::TextButton>("Mode");
    mModeBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b3b3b));
    mModeBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd6d6d6));
    mModeBtn->setTooltip("Character mode");
    mModeBtn->setVisible(false);
    mModeBtn->onClick = [this] { showModeMenu(); };
    addChildComponent(*mModeBtn);

    // H-9 prep (2026-05-02): Preset menu button on the LEFT side of the slot
    // header (next to the bypass LED).  Always visible when a non-empty
    // effect is loaded.  Click pops Save / Load (Factory + My Presets) /
    // Restore / Save as Default / Manage Presets.
    mPresetBtn = std::make_unique<juce::TextButton>("Preset");
    mPresetBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b3b3b));
    mPresetBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd6d6d6));
    mPresetBtn->setTooltip("Effect presets -- save / load / restore");
    mPresetBtn->setVisible(false);
    mPresetBtn->onClick = [this] { showPresetMenu(); };
    addChildComponent(*mPresetBtn);

    // QA-EffectsReview Task 1: Basic/Advanced disclosure toggle.  Same chrome
    // styling as mPresetBtn.  Hidden until setEditor() shows it for a panel that
    // reports hasAdvancedControls().  Click flips Basic<->Advanced + re-lays-out
    // the panel in place.
    mBasicBtn = std::make_unique<juce::TextButton>("Basic");
    mBasicBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b3b3b));
    mBasicBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd6d6d6));
    mBasicBtn->setTooltip("Basic / Advanced controls -- Advanced reveals power-user knobs");
    mBasicBtn->setVisible(false);
    mBasicBtn->onClick = [this] { toggleBasicMode(); };
    addChildComponent(*mBasicBtn);
}

// QA-EffectsReview Task 1: Basic/Advanced toggle helpers.  The flag is owned by
// the EffectRack slot (persisted with the project); the inline panel mirrors it
// via EditorPanelBase::mBasicMode.  Toggling re-applies the panel layout IN PLACE
// (no editor re-mount, so slider SafePointers / automation stay valid) and
// notifies the host so the project dirty bit picks it up.
void SlotComponent::refreshBasicBtnLabel()
{
    if (!mBasicBtn || !mRack) return;
    mBasicBtn->setButtonText(mRack->getSlotBasicMode(mSlotIndex) ? "Basic" : "Advanced");
}

void SlotComponent::toggleBasicMode()
{
    if (!mRack) return;
    const bool nb = ! mRack->getSlotBasicMode(mSlotIndex);
    mRack->setSlotBasicMode(mSlotIndex, nb);
    if (onBasicModeChanged) onBasicModeChanged(mSlotIndex, nb);
    if (auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get()))
    {
        base->mBasicMode = nb;
        base->applyBasicMode();
    }
    refreshBasicBtnLabel();
}

SlotComponent::~SlotComponent()
{
    mVBlank.reset();
    if (mEditor)
        removeChildComponent(mEditor.get());
}

// ── Rack / editor wiring ──────────────────────────────────────────────────────
void SlotComponent::setRack(EffectRack* rack)
{
    mRack = rack;
    refresh();
}

void SlotComponent::refresh()
{
    if (!mRack)
    {
        mLoaded      = false;
        mBypassed    = false;
        mEffectName  = {};
    }
    else
    {
        const auto& slot = mRack->getSlot(mSlotIndex);
        mLoaded   = (slot.type != EffectType::None);
        mBypassed = slot.bypassed.load (std::memory_order_relaxed);

        if (mLoaded)
            mEffectName = effectTypeName(slot.type);
        else
            mEffectName = {};
    }

    resized();
    repaint();
}

void SlotComponent::setEditor(std::unique_ptr<juce::Component> editor)
{
    if (mEditor)
        removeChildComponent(mEditor.get());

    mEditor = std::move(editor);
    mLoaded = (mEditor != nullptr);

    if (mEditor)
    {
        // QA-EffectsReview Task 9: stamp the slot's persisted Basic/Advanced
        // state onto EVERY incoming panel before its first layout.  This is
        // the single authority -- internal remounts (preset load, Mode-menu
        // switch, remountEditor) construct fresh panels that would otherwise
        // fall back to the ctor default (Basic) while the rack + header
        // button still say Advanced.
        if (auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get()))
            if (mRack)
                base->mBasicMode = mRack->getSlotBasicMode(mSlotIndex);

        addAndMakeVisible(*mEditor);

        // Wire output vol knob → rack slot gain
        if (auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get()))
        {
            const int slot = mSlotIndex;
            base->onOutputGainChanged = [this, slot](float db) {
                if (mRack) mRack->setSlotOutputGain(slot, db);
            };
            // I-4 (2026-05-02): pedal-style panels (CSStyleCompressorPanel +
            // every I-5+ pedal) call disableOutputVolKnob() in their ctor and
            // own their own Level knob, so base->outputVolKnob may be null.
            // Sync only when present.
            if (mRack && base->outputVolKnob)
                base->outputVolKnob->slider.setValue(mRack->getSlotOutputGain(slot),
                                                     juce::dontSendNotification);
        }
    }

    // C.4 Phase 1: show SC dropdown only for effects that consume SC.
    if (mScBtn)
    {
        bool show = false;
        if (mRack)
        {
            if (auto* eff = mRack->getSlotEffect(mSlotIndex))
                if (eff->usesSidechain())
                    show = true;
        }
        mScBtn->setVisible(show);
        if (show) refreshScBtnLabel();
    }

    // H-7 (2026-05-01): show Mode dropdown only for effects with character-
    // mode umbrellas.  Compressor: Modern/FET/Opto/CS Style (I-4).
    // Saturation: Tube/Console/Tape.
    // H-8 (2026-05-02): Delay: Echo / VocalDoubler.
    // H-9 (2026-05-02): Reverb: Plate / Hall / Chamber / Room / VocalBooth.
    // I-4 (2026-05-02): Overdrive: Rack / Pedal.
    if (mModeBtn)
    {
        bool show = false;
        if (mRack)
        {
            const auto t = mRack->getSlot(mSlotIndex).type;
            show = (t == EffectType::Compressor || t == EffectType::Saturation
                 || t == EffectType::Delay      || t == EffectType::Reverb
                 || t == EffectType::Overdrive);
        }
        mModeBtn->setVisible(show);
        if (show) refreshModeBtnLabel();
    }

    // H-9 prep (2026-05-02): Preset menu visible whenever a non-empty
    // effect is loaded (every effect gets factory + user presets).
    if (mPresetBtn)
    {
        bool show = false;
        if (mRack)
        {
            const auto& s = mRack->getSlot(mSlotIndex);
            show = (mRack->getSlotEffect(mSlotIndex) != nullptr && s.type != EffectType::None);
        }
        mPresetBtn->setVisible(show);
    }

    // QA-EffectsReview Task 1: Basic/Advanced toggle shown ONLY when the loaded
    // panel actually has advanced (non-reference) controls to hide -- a panel
    // with only reference controls (e.g. a Wah in the rack) reports false and
    // gets no button.  Label reflects the slot's current Basic/Advanced state.
    if (mBasicBtn)
    {
        bool show = false;
        if (auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get()))
            show = base->hasAdvancedControls();
        mBasicBtn->setVisible(show);
        if (show) refreshBasicBtnLabel();
    }

    resized();
    repaint();
}

// C.4 Phase 1 (2026-04-30): channel context wiring -- EffectsPage::rebuildSlotEditor
// pushes the strip's mixer APVTS prefix and a source-name resolver every time
// it rebuilds a slot editor.  Read by showScMenu / refreshScBtnLabel.
void SlotComponent::setChannelContext (juce::AudioProcessorValueTreeState* apvts,
                                         const juce::String& channelMixerPrefix,
                                         std::function<juce::String(int)> resolveSourceName)
{
    mApvts              = apvts;
    mChannelMixerPrefix = channelMixerPrefix;
    mResolveSourceName  = std::move(resolveSourceName);
    refreshScBtnLabel();
}

void SlotComponent::refreshScBtnLabel()
{
    if (!mScBtn || !mRack) return;
    const int pick = mRack->getSlotSidechainPick(mSlotIndex);

    auto label = juce::String("SC: Off");
    if (pick >= 0 && pick < 4 && mApvts != nullptr && mChannelMixerPrefix.isNotEmpty())
    {
        const juce::String pid = mChannelMixerPrefix
            + "_sc_recv" + juce::String(pick) + "_from";
        if (auto* p = mApvts->getRawParameterValue(pid))
        {
            const int srcId = (int) p->load();
            if (srcId >= 0)
            {
                juce::String srcName = mResolveSourceName ? mResolveSourceName(srcId)
                                                          : juce::String("Ch ") + juce::String(srcId);
                if (srcName.isEmpty()) srcName = juce::String("Ch ") + juce::String(srcId);
                label = "SC: " + srcName;
            }
        }
    }
    mScBtn->setButtonText(label);
}

void SlotComponent::showScMenu()
{
    if (!mRack || !mApvts || mChannelMixerPrefix.isEmpty()) return;

    juce::PopupMenu m;
    m.addItem(1, "Off", true, mRack->getSlotSidechainPick(mSlotIndex) < 0);

    // Enumerate active SC receive lines on this strip.  Inactive lines
    // (-1) are skipped so the user only sees real, drag-routed sources.
    bool anyActive = false;
    for (int s = 0; s < 4; ++s)
    {
        const juce::String pid = mChannelMixerPrefix
            + "_sc_recv" + juce::String(s) + "_from";
        if (auto* p = mApvts->getRawParameterValue(pid))
        {
            const int srcId = (int) p->load();
            if (srcId < 0) continue;
            anyActive = true;
            juce::String srcName = mResolveSourceName ? mResolveSourceName(srcId)
                                                      : juce::String("Ch ") + juce::String(srcId);
            if (srcName.isEmpty()) srcName = juce::String("Ch ") + juce::String(srcId);
            m.addItem(10 + s, srcName, true,
                      mRack->getSlotSidechainPick(mSlotIndex) == s);
        }
    }

    if (! anyActive)
    {
        m.addSeparator();
        m.addItem(99, "(no sidechain cables routed to this strip)", false, false);
    }

    m.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(mScBtn.get()),
        [this](int r)
        {
            if (r <= 0 || !mRack) return;
            const int pick = (r == 1) ? -1 : (r - 10);
            mRack->setSlotSidechainPick(mSlotIndex, pick);
            refreshScBtnLabel();
        });
}

void SlotComponent::setEditorUndoContext(const UndoContext& ctx)
{
    if (auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get()))
        base->setUndoContext(ctx);
}

// 2026-05-02: vblank-locked level feed (replaces 30 Hz timer).  Each monitor
// refresh, drain the slot's running-max input/output atomics via exchange-
// and-reset and push the values to the editor panel's VU + DBFS meters,
// which run their own UI-thread ballistics on top.
void SlotComponent::onVBlank()
{
    if (!mRack || !mEditor) return;
    auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get());
    if (!base) return;
    base->setInputLevel (mRack->drainSlotInputLevel (mSlotIndex));
    base->setOutputLevel(mRack->drainSlotOutputLevel(mSlotIndex));
}

void SlotComponent::parentHierarchyChanged()
{
    // Create / destroy the vblank attachment based on whether we have a peer.
    if (getPeer() != nullptr && mVBlank == nullptr)
    {
        mVBlank = std::make_unique<juce::VBlankAttachment> (
            this, [this] { onVBlank(); });
    }
    else if (getPeer() == nullptr && mVBlank != nullptr)
    {
        mVBlank.reset();
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void SlotComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);

    if (!mLoaded)
    {
        // Empty state: dark recessed panel with dashed border
        g.setColour(VC::Panel);
        g.fillRoundedRectangle(b, 3.0f);

        // Dashed border
        juce::Path border;
        border.addRoundedRectangle(b, 3.0f);
        float dashes[] = { 6.0f, 4.0f };
        juce::Path dashedBorder;
        juce::PathStrokeType(1.0f).createDashedStroke(dashedBorder, border, dashes, 2);
        g.setColour(VC::Accent);
        g.fillPath(dashedBorder);

        // Slot number (top-left, small)
        g.setColour(VC::Accent);
        g.setFont(juce::Font(9.0f));
        g.drawText(juce::String(mSlotIndex + 1), 4, 2, 14, 14,
                   juce::Justification::centredLeft);

        // "+" centered
        g.setColour(VC::TextDim);
        g.setFont(juce::Font(22.0f));
        g.drawText("+", getLocalBounds(), juce::Justification::centred);
    }
    else
    {
        // Loaded: header strip background
        auto headerR = getLocalBounds().removeFromTop(28).toFloat().reduced(1.0f, 1.0f);
        g.setColour(VC::Surface);
        g.fillRoundedRectangle(headerR, 3.0f);
        g.setColour(VC::Accent);
        g.drawRoundedRectangle(headerR, 3.0f, 1.0f);

        // Editor area background
        auto editorR = getLocalBounds().withTrimmedTop(28).toFloat().reduced(1.0f, 0.0f);
        g.setColour(VC::Panel);
        g.fillRoundedRectangle(editorR, 3.0f);
        g.setColour(VC::Accent.withAlpha(0.4f));
        g.drawRoundedRectangle(editorR, 3.0f, 1.0f);


        // Bypass dot: green when effect active, red when bypassed
        g.setFont(juce::Font(16.0f));
        g.setColour(mBypassed ? juce::Colour(0xffcc2222) : juce::Colour(0xff22cc44));
        g.drawText(juce::String::fromUTF8("\xe2\x97\x8f"),  // UTF-8 for ●
                   mBypassRect, juce::Justification::centred);

        // Effect name (between bypass and the SC dropdown / up-arrow on the right).
        // C.4 Phase 1: when SC dropdown is visible, name area shrinks to leave
        // room for it.  When hidden, name extends to ▲ glyph as before.
        // H-7 (2026-05-01): Mode dropdown also takes header space; name shrinks
        // to fit when it is visible.
        int nameX = mBypassRect.getRight() + 4;
        int nameRight = mUpRect.getX() - 4;
        if (mScBtn != nullptr && mScBtn->isVisible())
            nameRight = mScBtn->getX() - 4;
        if (mModeBtn != nullptr && mModeBtn->isVisible())
            nameRight = juce::jmin (nameRight, mModeBtn->getX() - 4);
        if (mPresetBtn != nullptr && mPresetBtn->isVisible())
            nameRight = juce::jmin (nameRight, mPresetBtn->getX() - 4);
        if (mBasicBtn != nullptr && mBasicBtn->isVisible())
            nameRight = juce::jmin (nameRight, mBasicBtn->getX() - 4);
        int nameW = juce::jmax(0, nameRight - nameX);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.setColour(VC::Text);
        g.drawText(mEffectName,
                   juce::Rectangle<int>(nameX, mBypassRect.getY(),
                                        nameW, mBypassRect.getHeight()),
                   juce::Justification::centredLeft);

        // Navigation glyphs
        g.setFont(juce::Font(14.0f));
        g.setColour(VC::TextDim);
        g.drawText(juce::String::fromUTF8("\xe2\x96\xb2"),  // UTF-8 for ▲
                   mUpRect,   juce::Justification::centred);
        g.drawText(juce::String::fromUTF8("\xe2\x96\xbc"),  // UTF-8 for ▼
                   mDownRect, juce::Justification::centred);

        // Close glyph (reddish)
        g.setColour(juce::Colour(0xffcc4444));
        g.drawText(juce::String::fromUTF8("\xc3\x97"),      // UTF-8 for ×
                   mCloseRect, juce::Justification::centred);
    }
}

// ── Layout ────────────────────────────────────────────────────────────────────
void SlotComponent::resized()
{
    if (!mLoaded)
    {
        if (mEditor)
            mEditor->setBounds(getLocalBounds().reduced(2));
        return;
    }

    auto b = getLocalBounds();
    auto header = b.removeFromTop(28).reduced(2, 2);

    // Bypass dot on the left
    mBypassRect = header.removeFromLeft(24).withSizeKeepingCentre(20, 20);
    header.removeFromLeft(2);

    // Action glyphs on the right (close, then down, then up - right-to-left)
    mCloseRect = header.removeFromRight(24).withSizeKeepingCentre(20, 20);
    mDownRect  = header.removeFromRight(24).withSizeKeepingCentre(20, 20);
    mUpRect    = header.removeFromRight(24).withSizeKeepingCentre(20, 20);
    header.removeFromRight(2);

    // C.4 Phase 1: SC dropdown sits between the effect name area and the
    // ▲▼× glyph cluster.  ~110 px wide so "SC: Layer 2" fits comfortably.
    // H-7 (2026-05-01): Mode dropdown sits immediately left of SC dropdown
    // for Compressor (Modern/FET/Opto) and takes the SC slot's footprint
    // for Saturation (Tube/Console - Saturation has no SC).
    if (mScBtn && mScBtn->isVisible())
    {
        mScBtn->setBounds(header.removeFromRight(110).withSizeKeepingCentre(108, 20));
        header.removeFromRight(4);
    }
    if (mModeBtn && mModeBtn->isVisible())
    {
        mModeBtn->setBounds(header.removeFromRight(90).withSizeKeepingCentre(88, 20));
        header.removeFromRight(4);
    }

    // H-9 prep (2026-05-02): Preset menu sits to the RIGHT of the effect
    // name.  In the right-side header cluster it's the leftmost item --
    // taken from removeFromRight after Mode + SC are placed (which sit
    // further right).  Visible only when a non-empty effect is loaded.
    if (mPresetBtn && mPresetBtn->isVisible())
    {
        mPresetBtn->setBounds(header.removeFromRight(60).withSizeKeepingCentre(58, 20));
        header.removeFromRight(4);
    }

    // QA-EffectsReview Task 1: Basic/Advanced toggle sits immediately LEFT of the
    // Preset button (next removeFromRight after Preset).  ~72 px so "Advanced" fits.
    if (mBasicBtn && mBasicBtn->isVisible())
    {
        mBasicBtn->setBounds(header.removeFromRight(72).withSizeKeepingCentre(70, 20));
        header.removeFromRight(4);
    }

    // Editor fills the rest (fader column already removed from b)
    if (mEditor)
        mEditor->setBounds(b.reduced(2, 2));
}

// ── Mouse ─────────────────────────────────────────────────────────────────────
void SlotComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!mLoaded)
    {
        // H-6c (2026-05-01): locked + empty is a no-op (vocal chain slots
        // are pre-loaded so this state shouldn't occur, but defensive).
        if (mLocked) return;
        mLastMousePosScreen = e.getScreenPosition();
        showAddMenu();
        return;
    }

    // Loaded: dispatch based on hit region
    auto pos = e.getPosition();

    if (mBypassRect.contains(pos))
    {
        mBypassed = !mBypassed;
        if (mRack) mRack->setSlotBypassed(mSlotIndex, mBypassed);
        repaint();
    }
    else if (! mLocked && mUpRect.contains(pos))
    {
        if (onMoveRequested) onMoveRequested(mSlotIndex, true);
    }
    else if (! mLocked && mDownRect.contains(pos))
    {
        if (onMoveRequested) onMoveRequested(mSlotIndex, false);
    }
    else if (! mLocked && mCloseRect.contains(pos))
    {
        if (onEffectRemoved) onEffectRemoved(mSlotIndex);
    }
}

// ── Popup menu (Change D: appears at cursor, alphabetical, no EQ) ─────────────
void SlotComponent::showAddMenu()
{
    // 2026-05-02: grouped by effect family.  Section headers are
    // non-clickable JUCE separators so the list stays flat (no submenu
    // hover step) but reads as 4 categories.
    juce::PopupMenu m;
    m.addSectionHeader ("Dynamic");
    // I-8 (2026-05-03): Dynamics pedals (Bass Compressor + Noise Gate)
    // alpha-sorted alongside the existing rack dynamics.
    m.addItem ((int)EffectType::BassCompressorStyle, "Bass Compressor");
    m.addItem ((int)EffectType::Compressor,          "Compressor");
    m.addItem ((int)EffectType::DeEsser,             "De-esser");
    m.addItem ((int)EffectType::Limiter,             "Limiter");
    m.addItem ((int)EffectType::NoiseGateStyle,      "Noise Gate");
    m.addItem ((int)EffectType::TransientShaper,     "Transient Shaper");

    m.addSectionHeader ("Harmonics");
    // I-5 + I-6 + I-7 (2026-05-02): Harmonics drive + bass + octave pedals.
    // Sorted alpha within the group per H-cross-cutting effect-picker
    // convention.
    m.addItem ((int)EffectType::BassDriverStyle,    "Bass Driver");
    m.addItem ((int)EffectType::BassOverdriveStyle, "Bass Overdrive");
    m.addItem ((int)EffectType::BluesDriveStyle,    "Blues Drive");
    m.addItem ((int)EffectType::DistortionStyle,    "Distortion");
    m.addItem ((int)EffectType::FuzzStyle,          "Fuzz");
    m.addItem ((int)EffectType::HighGainStyle,      "High-Gain");
    m.addItem ((int)EffectType::OctaveStyle,        "Octave");
    m.addItem ((int)EffectType::Overdrive,          "Overdrive");
    m.addItem ((int)EffectType::Saturation,         "Saturation");
    // H-10 cutover (2026-05-02): Tape was folded into Saturation as a 3rd
    // type (Tube/Console/Tape).  Users now pick Saturation and switch the
    // Mode dropdown to Tape; the standalone "Tape" picker entry is gone.
    // EffectType::Tape stays in the enum + EffectRack as an alias so old
    // projects load correctly, but it's no longer in the picker.

    m.addSectionHeader ("Modulation");
    // I-9 (2026-05-03): added SY Style Polyphonic Synth (Modulation LAF group
    // per locked spec).
    // I-10 (2026-05-03): added PW Style Wah (filter pedal -- Modulation group).
    // I-11 (2026-05-03): added AC Style Acoustic Simulator (Modulation group
    // per Jeff's call -- corrective EQ + transient + Schroeder reverb, no IR
    // unless User mode).
    m.addItem ((int)EffectType::AcousticSimulatorStyle, "Acoustic Simulator");
    m.addItem ((int)EffectType::Chorus,          "Chorus");
    m.addItem ((int)EffectType::Flanger,         "Flanger");
    m.addItem ((int)EffectType::Phaser,          "Phaser");
    m.addItem ((int)EffectType::SynthStyle,      "Polyphonic Synth");
    m.addItem ((int)EffectType::WahStyle,        "Wah");

    m.addSectionHeader ("Time");
    // I-11 (2026-05-03): added AD Style Acoustic Preamp (Time LAF group per
    // locked spec table).
    // I-15 (2026-05-03): GE / GEB / EQFH / TU are intentionally NOT in the FX
    // rack picker -- per locked spec they are BaySickPedals-only effects.
    // DSP + panels live in the codebase, factory wires them up, and the
    // BaySickPedalsEditor (Inst page sub-tab) is the only place users can
    // reach them.
    m.addItem ((int)EffectType::AcousticPreampStyle, "Acoustic Preamp");
    m.addItem ((int)EffectType::Delay,           "Delay");
    m.addItem ((int)EffectType::Reverb,          "Reverb");

    auto opts = juce::PopupMenu::Options()
        .withTargetScreenArea({ mLastMousePosScreen.x, mLastMousePosScreen.y, 1, 1 });

    m.showMenuAsync(opts,
        [this](int result)
        {
            if (result < 1) return;
            if (onEffectChosen) onEffectChosen(mSlotIndex, (EffectType)result);
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// H-7 (2026-05-01): Mode dropdown -- character-mode picker per effect type.
// Compressor:  Modern (0) / FET (1) / Opto (2)
// Saturation:  Tube (0)   / Console (1)
// ─────────────────────────────────────────────────────────────────────────────
void SlotComponent::refreshModeBtnLabel()
{
    if (! mModeBtn || ! mRack) return;
    const auto& slot = mRack->getSlot(mSlotIndex);
    if (! mRack->getSlotEffect(mSlotIndex)) { mModeBtn->setButtonText("Mode"); return; }

    juce::String label = "Mode";
    if (slot.type == EffectType::Compressor)
    {
        // I-4 (2026-05-02): friendly Mode-dropdown labels per locked spec
        // (option B parenthetical descriptors).  Dropdown button shows the
        // short label; full menu adds the descriptor in parens.
        if (auto* c = dynamic_cast<CompressorDSP*>(mRack->getSlotEffect(mSlotIndex)))
        {
            switch (c->mType)
            {
                case CompressorDSP::Type::Modern: label = "Modern";   break;
                case CompressorDSP::Type::FET:    label = "FET";      break;
                case CompressorDSP::Type::Opto:   label = "Opto";     break;
                case CompressorDSP::Type::CS:     label = "CS Style"; break;
            }
        }
    }
    else if (slot.type == EffectType::Saturation)
    {
        if (auto* s = dynamic_cast<SaturationDSP*>(mRack->getSlotEffect(mSlotIndex)))
        {
            // H-10 (2026-05-02): Tape joins Tube/Console as a 3rd type.
            switch (s->mSatType)
            {
                case SaturationDSP::Type::Tube:    label = "Tube";    break;
                case SaturationDSP::Type::Console: label = "Console"; break;
                case SaturationDSP::Type::Tape:    label = "Tape";    break;
            }
        }
    }
    else if (slot.type == EffectType::Delay)
    {
        if (auto* d = dynamic_cast<DelayDSP*>(mRack->getSlotEffect(mSlotIndex)))
        {
            label = (d->getType() == (int) DelayDSP::Type::VocalDoubler)
                        ? "Doubler" : "Echo";
        }
    }
    else if (slot.type == EffectType::Reverb)
    {
        if (auto* r = dynamic_cast<ReverbDSP*>(mRack->getSlotEffect(mSlotIndex)))
        {
            switch ((ReverbDSP::Algorithm) r->getAlgorithm())
            {
                case ReverbDSP::Algorithm::Plate:      label = "Plate"; break;
                case ReverbDSP::Algorithm::Hall:       label = "Hall";  break;
                case ReverbDSP::Algorithm::Chamber:    label = "Chamber"; break;
                case ReverbDSP::Algorithm::Room:       label = "Room";  break;
                case ReverbDSP::Algorithm::VocalBooth: label = "Booth"; break;
            }
        }
    }
    else if (slot.type == EffectType::Overdrive)
    {
        // I-4 (2026-05-02): Overdrive folds the OD Style pedal in as a Type
        // (Rack vs Pedal), same pattern as Compressor's Modern/FET/Opto/CS
        // Style.  Single picker entry "Overdrive"; this Mode dropdown is
        // where the user picks the algorithm character.
        if (auto* o = dynamic_cast<OverdriveDSP*>(mRack->getSlotEffect(mSlotIndex)))
        {
            switch (o->mType)
            {
                case OverdriveDSP::Type::Rack:  label = "Rack";  break;
                case OverdriveDSP::Type::Pedal: label = "Pedal"; break;
            }
        }
    }
    mModeBtn->setButtonText(label);
}

void SlotComponent::showModeMenu()
{
    if (! mRack) return;
    const auto& slot = mRack->getSlot(mSlotIndex);
    if (! mRack->getSlotEffect(mSlotIndex)) return;

    juce::PopupMenu m;
    int currentPick = -1;
    if (slot.type == EffectType::Compressor)
    {
        if (auto* c = dynamic_cast<CompressorDSP*>(mRack->getSlotEffect(mSlotIndex)))
            currentPick = (int) c->mType;
        // I-4 (2026-05-02): friendly Mode-menu labels with parenthetical
        // descriptors per locked spec (option B).  CS Style is the new
        // 4th Type alongside Modern/FET/Opto.
        m.addItem(1 + (int) CompressorDSP::Type::Modern, "Modern",            true,
                  currentPick == (int) CompressorDSP::Type::Modern);
        m.addItem(1 + (int) CompressorDSP::Type::FET,    "FET (Punchy)",      true,
                  currentPick == (int) CompressorDSP::Type::FET);
        m.addItem(1 + (int) CompressorDSP::Type::Opto,   "Opto (Smooth)",     true,
                  currentPick == (int) CompressorDSP::Type::Opto);
        m.addItem(1 + (int) CompressorDSP::Type::CS,     "CS Style (Sustain)", true,
                  currentPick == (int) CompressorDSP::Type::CS);
    }
    else if (slot.type == EffectType::Saturation)
    {
        if (auto* s = dynamic_cast<SaturationDSP*>(mRack->getSlotEffect(mSlotIndex)))
            currentPick = (int) s->mSatType;
        m.addItem(1 + (int) SaturationDSP::Type::Tube,    "Tube",    true,
                  currentPick == (int) SaturationDSP::Type::Tube);
        m.addItem(1 + (int) SaturationDSP::Type::Console, "Console", true,
                  currentPick == (int) SaturationDSP::Type::Console);
        // H-10 (2026-05-02): Tape now lives under the Saturation umbrella as
        // a 3rd type; picking it remounts the slot panel to TapeSatPanel.
        m.addItem(1 + (int) SaturationDSP::Type::Tape,    "Tape",    true,
                  currentPick == (int) SaturationDSP::Type::Tape);
    }
    else if (slot.type == EffectType::Delay)
    {
        if (auto* d = dynamic_cast<DelayDSP*>(mRack->getSlotEffect(mSlotIndex)))
            currentPick = d->getType();
        m.addItem(1 + (int) DelayDSP::Type::Echo,         "Echo",         true,
                  currentPick == (int) DelayDSP::Type::Echo);
        m.addItem(1 + (int) DelayDSP::Type::VocalDoubler, "Vocal Doubler", true,
                  currentPick == (int) DelayDSP::Type::VocalDoubler);
    }
    else if (slot.type == EffectType::Reverb)
    {
        if (auto* r = dynamic_cast<ReverbDSP*>(mRack->getSlotEffect(mSlotIndex)))
            currentPick = r->getAlgorithm();
        m.addItem(1 + (int) ReverbDSP::Algorithm::Plate,      "Plate",      true,
                  currentPick == (int) ReverbDSP::Algorithm::Plate);
        m.addItem(1 + (int) ReverbDSP::Algorithm::Hall,       "Hall",       true,
                  currentPick == (int) ReverbDSP::Algorithm::Hall);
        m.addItem(1 + (int) ReverbDSP::Algorithm::Chamber,    "Chamber",    true,
                  currentPick == (int) ReverbDSP::Algorithm::Chamber);
        m.addItem(1 + (int) ReverbDSP::Algorithm::Room,       "Room",       true,
                  currentPick == (int) ReverbDSP::Algorithm::Room);
        m.addItem(1 + (int) ReverbDSP::Algorithm::VocalBooth, "VocalBooth", true,
                  currentPick == (int) ReverbDSP::Algorithm::VocalBooth);
    }
    else if (slot.type == EffectType::Overdrive)
    {
        // I-4 (2026-05-02): Overdrive Mode dropdown -- Rack vs Pedal.  Pedal
        // body is a placeholder until I-5 ships the OD Style algorithm; for
        // now both modes use the existing Rack chain so the slot processes
        // audio normally.
        if (auto* o = dynamic_cast<OverdriveDSP*>(mRack->getSlotEffect(mSlotIndex)))
            currentPick = (int) o->mType;
        m.addItem(1 + (int) OverdriveDSP::Type::Rack,  "Overdrive (Rack)",  true,
                  currentPick == (int) OverdriveDSP::Type::Rack);
        m.addItem(1 + (int) OverdriveDSP::Type::Pedal, "Overdrive (Pedal)", true,
                  currentPick == (int) OverdriveDSP::Type::Pedal);
    }
    else
    {
        return;
    }

    m.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(mModeBtn.get()),
        [this, slotType = slot.type](int r)
        {
            if (r < 1) return;
            const int newType = r - 1;
            // Push directly to the DSP so the rack-state save/load picks it up
            // on the next save.  Hosts that drive via APVTS (BaySickVocal) ALSO
            // get notified through onModeChanged so they can mirror the value
            // into APVTS, keeping the per-block APVTS push consistent.
            if (mRack)
            {
                if (auto* eff = mRack->getSlotEffect(mSlotIndex))
                {
                    if (slotType == EffectType::Compressor)
                    {
                        if (auto* c = dynamic_cast<CompressorDSP*>(eff))
                            c->setType(newType);
                    }
                    else if (slotType == EffectType::Saturation)
                    {
                        if (auto* s = dynamic_cast<SaturationDSP*>(eff))
                            s->setSatType(newType);
                    }
                    else if (slotType == EffectType::Delay)
                    {
                        if (auto* d = dynamic_cast<DelayDSP*>(eff))
                            d->setType(newType);
                    }
                    else if (slotType == EffectType::Reverb)
                    {
                        if (auto* r = dynamic_cast<ReverbDSP*>(eff))
                            r->setAlgorithm(newType);
                    }
                    else if (slotType == EffectType::Overdrive)
                    {
                        if (auto* o = dynamic_cast<OverdriveDSP*>(eff))
                            o->setType(newType);
                    }
                }
            }
            if (onModeChanged) onModeChanged(mSlotIndex, newType);
            // H-7 (2026-05-01): re-mount the inline editor with the dedicated
            // panel for the new mode (FET / Opto / Modern -- Console / Tube).
            // createEffectEditor dispatches by the DSP's mType field, which
            // we just updated above.
            if (mRack)
            {
                const auto& slotNow = mRack->getSlot (mSlotIndex);
                if (auto* eff2 = mRack->getSlotEffect(mSlotIndex))
                    setEditor (createEffectEditor (eff2, slotNow.type));
            }
            refreshModeBtnLabel();
        });
}

// Public re-mount helper.  Used after a preset load / Restore-Defaults (see
// showPresetMenu) so an inline panel picks up a Type change (e.g. Delay
// Echo <-> VocalDoubler) and shows the right layout.  Mirrors the re-mount
// logic in showModeMenu's callback.
void SlotComponent::remountEditor()
{
    if (! mRack) return;
    const auto& slotNow = mRack->getSlot (mSlotIndex);
    if (auto* eff = mRack->getSlotEffect(mSlotIndex))
        setEditor (createEffectEditor (eff, slotNow.type));
    refreshModeBtnLabel();
}

// H-9 prep (2026-05-02): preset menu.  Save / Load (Factory + My Presets) /
// Restore Default / Save as Default / Manage Presets.  Save + Load both go
// through EffectPresetIO::savePreset / loadPreset which use the DSP's
// getStateInformation / setStateInformation -- so Type-umbrella state
// (Compressor / Saturation / Delay) round-trips faithfully.  After a load,
// the inline panel is re-mounted so a Type change in the loaded preset
// shows the right layout.
void SlotComponent::showPresetMenu()
{
    if (! mRack) return;
    const auto& slot = mRack->getSlot (mSlotIndex);
    DSPBase* preDsp = mRack->getSlotEffect(mSlotIndex);
    if (! preDsp || slot.type == EffectType::None) return;

    juce::PopupMenu menu;
    const EffectType type = slot.type;
    DSPBase* dsp          = mRack->getSlotEffect(mSlotIndex);

    // ── Save Current ─────────────────────────────────────────────────────
    menu.addItem ("Save Current Preset...", [this, type, dsp]()
    {
        auto* aw = new juce::AlertWindow ("Save Preset",
            "Name this preset:", juce::AlertWindow::QuestionIcon);
        aw->addTextEditor ("name", "", "Preset name");
        aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true,
            juce::ModalCallbackFunction::create (
                [aw, type, dsp] (int result)
                {
                    const juce::String name = aw->getTextEditorContents ("name").trim();
                    delete aw;
                    if (result != 1 || name.isEmpty()) return;
                    juce::String err;
                    if (! EffectPresetIO::savePreset (*dsp, type, name, err))
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::AlertWindow::WarningIcon,
                            "Could not save preset", err);
                }),
            true);
    });

    // ── Load Preset (Factory + My Presets sub-menus) ─────────────────────
    auto buildLoadSubmenu = [this, type, dsp] (const juce::Array<juce::File>& files,
                                                  juce::PopupMenu& sub,
                                                  const juce::String& emptyHint)
    {
        if (files.isEmpty())
        {
            sub.addItem (emptyHint, false, false, [](){});
            return;
        }
        for (auto& f : files)
        {
            const juce::String label = f.getFileNameWithoutExtension();
            sub.addItem (label, [this, dsp, f]()
            {
                juce::String err;
                if (! EffectPresetIO::loadPreset (*dsp, f, err))
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::WarningIcon,
                        "Could not load preset", err);
                    return;
                }
                // Re-mount the panel so Type changes inside the preset
                // show the right layout (e.g. Compressor switching to FET).
                remountEditor();
            });
        }
    };

    juce::PopupMenu loadFactory;
    buildLoadSubmenu (EffectPresetIO::enumerateFactory (type), loadFactory,
                       "(no factory presets)");
    menu.addSubMenu ("Load: Factory", loadFactory);

    juce::PopupMenu loadUser;
    buildLoadSubmenu (EffectPresetIO::enumerateMyPresets (type), loadUser,
                       "(no user presets yet)");
    menu.addSubMenu ("Load: My Presets", loadUser);

    menu.addSeparator();

    // ── Restore Default ──────────────────────────────────────────────────
    menu.addItem ("Restore Defaults", [this, type, dsp]()
    {
        EffectPresetIO::restoreDefaults (*dsp, type);
        remountEditor();
    });

    // ── Save as Default ──────────────────────────────────────────────────
    menu.addItem ("Save Current as Default", [type, dsp]()
    {
        juce::String err;
        if (! EffectPresetIO::saveAsDefault (*dsp, type, err))
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "Could not save default", err);
    });

    menu.addSeparator();

    // ── Manage Presets ───────────────────────────────────────────────────
    menu.addItem ("Manage Presets... (open folder)", [type]()
    {
        const auto root = EffectPresetIO::typeRoot (type);
        EffectPresetIO::ensureFolderTree (type);
        if (root.isDirectory()) root.startAsProcess();
    });

    menu.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent (mPresetBtn.get()));
}

juce::String SlotComponent::effectTypeName(EffectType type)
{
    switch (type)
    {
        case EffectType::Compressor:      return "Compressor";
        case EffectType::Reverb:          return "Reverb";
        case EffectType::Chorus:          return "Chorus";
        case EffectType::Delay:           return "Delay";
        case EffectType::Saturation:      return "Saturation";
        case EffectType::Flanger:         return "Flanger";
        case EffectType::Overdrive:       return "Overdrive";
        case EffectType::Phaser:          return "Phaser";
        case EffectType::TransientShaper: return "Transient Shaper";
        case EffectType::Tape:            return "Tape";
        case EffectType::Limiter:         return "Limiter";
        case EffectType::DeEsser:         return "De-esser";
        // QA-Fe2: vocal-chain-only stages (locked slots; deliberately absent
        // from the rack picker menu above).
        case EffectType::Gate:            return "Gate";
        case EffectType::DeReverb:        return "De-reverb";

        // I-5 (2026-05-02): BaySickPedals Harmonics drive pedals batch.
        case EffectType::BluesDriveStyle: return "Blues Drive";
        case EffectType::DistortionStyle: return "Distortion";
        case EffectType::FuzzStyle:       return "Fuzz";
        case EffectType::HighGainStyle:   return "High-Gain";

        // I-1 enum entries with no DSP yet (I-6..I-13 ship them).  Names
        // match the FX-rack picker labels so when those land, the slot
        // header doesn't show "-" while the picker shows the friendly name.
        case EffectType::NoiseGateStyle:      return "Noise Gate";
        case EffectType::TunerStyle:          return "Tuner";
        case EffectType::AcousticPreampStyle:    return "Acoustic Preamp";
        case EffectType::AcousticSimulatorStyle: return "Acoustic Simulator";
        case EffectType::GraphicEQStyle:      return "Graphic EQ";
        case EffectType::SynthStyle:          return "Polyphonic Synth";
        case EffectType::OctaveStyle:         return "Octave";
        case EffectType::WahStyle:            return "Wah";
        case EffectType::BassGraphicEQStyle:  return "Bass Graphic EQ";
        case EffectType::BassCompressorStyle: return "Bass Compressor";
        case EffectType::BassDriverStyle:     return "Bass Driver";
        case EffectType::BassOverdriveStyle:  return "Bass Overdrive";
        case EffectType::FurmanEQStyle:       return "Pro Parametric EQ";

        default:                          return "-";
    }
}
