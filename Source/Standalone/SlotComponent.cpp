#include "SlotComponent.h"
#include "ShotMenuHook.h"
#include "EffectEditorPanels.h"
#include "../DSP/CompressorDSP.h"
#include "../DSP/SaturationDSP.h"
#include "../DSP/DelayDSP.h"
#include "../DSP/ReverbDSP.h"
#include "../DSP/OverdriveDSP.h"   // I-4: Mode dropdown for Overdrive (Rack vs Pedal)
#include "../DSP/LimiterDSP.h"     // TS7: Mode dropdown for Limiter vs Maximizer
#include "EffectPresetIO.h"
#include "../MissingFileReport.h"
#include "../Hosting/HostedPluginEffect.h"   // QA-ModelShell TS6: added-effects list + slot naming

// Base id for hosted-plugin picker rows: kVst3PickerItemId + index into the
// added-EFFECTS list.  Deliberately far outside the EffectType range so a
// plugin row can never be cast to one -- the two are dispatched to different
// callbacks and a collision would load the wrong thing silently.
// kVst3PickerItemId - 1 is the disabled "None added" row.
static constexpr int kVst3PickerItemId = 9001;

// ── HeaderSubMenuItem — a GROUP heading that is itself a dropdown ─────────────
// Jeff 2026-07-29: "Pedals" (and, at TS6, "VST Plugins") must read as a group
// heading -- the bold, taller section font -- with the submenu hanging off that
// same line, not as an ordinary entry tucked under another group.
//
// JUCE cannot do this with a real section header: ItemComponent swaps a header
// item's component for its own HeaderItemComponent and calls setEnabled(false)
// (juce_PopupMenu.cpp:128), and canBeTriggered/hasActiveSubMenu both refuse a
// disabled item -- so a header can never open a submenu.  A custom item
// component CAN, and by calling the same two LookAndFeel entry points the real
// header uses, it renders identically to the headers above it whatever LAF is
// in force.
struct HeaderSubMenuItem final : public juce::PopupMenu::CustomComponent
{
    explicit HeaderSubMenuItem (juce::String text)
        // false = not "triggered automatically": clicking the row must open the
        // submenu, never dismiss the menu as a chosen item would.
        : juce::PopupMenu::CustomComponent (false), mText (std::move (text))
    {
        setName (mText);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        const bool hot = isItemHighlighted();

        // The hover highlight is drawn by the LAF INSIDE drawPopupMenuItem, and
        // a custom component replaces that call entirely -- so without this the
        // row is the one entry in the menu that never lights up under the
        // pointer (Jeff, 2026-07-29).  Same colour and the same 1 px inset
        // LookAndFeel_V4 uses, so it lines up with the rows above and below.
        if (hot)
        {
            g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect (b.reduced (1));
        }

        // Header text stays the LAF's job -- duplicating its font/colour choice
        // here would drift the moment a LAF overrides the header draw.
        getLookAndFeel().drawPopupMenuSectionHeader (g, b, mText);

        // The submenu arrow, drawn here because a custom component replaces the
        // LAF's own item rendering -- without it the row claims to be a heading
        // and gives no sign that it opens.
        auto arrow = b.removeFromRight (18).reduced (5, 0).toFloat();
        const float cx = arrow.getX(), cy = arrow.getCentreY(), h = 4.0f;
        juce::Path p;
        p.startNewSubPath (cx,       cy - h);
        p.lineTo          (cx + 6.f, cy);
        p.lineTo          (cx,       cy + h);
        g.setColour (findColour (hot ? juce::PopupMenu::highlightedTextColourId
                                     : juce::PopupMenu::headerTextColourId));
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }

    void getIdealSize (int& idealWidth, int& idealHeight) override
    {
        getLookAndFeel().getIdealPopupMenuItemSize (mText, false, -1, idealWidth, idealHeight);
        idealHeight += idealHeight / 2;   // the LAF's own header-height rule
        idealWidth  += 24;                // room for the arrow
    }

    juce::String mText;
};

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
    if (mSlotIndex == 3)   // M-4c: one slot carries the callout
        mScBtn->getProperties().set (kDotAnchor, "BSVC-7");
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
    if (mSlotIndex == 3)   // M-4c: one slot carries the callout
        mModeBtn->getProperties().set (kDotAnchor, "BSVC-6");
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
    if (mSlotIndex == 0)   // M-4c: one slot carries the callout
        mPresetBtn->getProperties().set (kDotAnchor, "BSVC-5");
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
    if (mSlotIndex == 2)   // M-4c: one slot carries the callout
        mBasicBtn->getProperties().set (kDotAnchor, "BSVC-4");
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
// (no editor re-mount, so slider SafePointers / automation stay valid); the
// project dirty bit flips through EffectRack::setSlotBasicMode -> onSlotsChanged.
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
            mEffectName = slotDisplayName (mRack, mSlotIndex);
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

        // Wire output vol knob → rack slot gain.  mSlotIndex is read LIVE in
        // the lambda: the panel window's uuid-follow re-points this component
        // at the slot's new index when slots repack, and a mount-time copy
        // kept writing the OLD index's gain.
        if (auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get()))
        {
            const int slot = mSlotIndex;
            base->onOutputGainChanged = [this](float db) {
                if (mRack) mRack->setSlotOutputGain(mSlotIndex, db);
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
    // TS7: Limiter: Limiter (Reproduction) / Maximizer (Loudness).  Omitting it
    // here left the whole Maximizer mode unreachable from the panel.
    if (mModeBtn)
    {
        bool show = false;
        if (mRack)
        {
            const auto t = mRack->getSlot(mSlotIndex).type;
            show = (t == EffectType::Compressor || t == EffectType::Saturation
                 || t == EffectType::Delay      || t == EffectType::Reverb
                 || t == EffectType::Overdrive  || t == EffectType::Limiter);
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

    // Runs LAST: the blocks above decide chrome for a loaded effect, and in a
    // panel window there is no header for that chrome to live in.
    applyPresentationToChrome();

    resized();
    repaint();

    if (onEditorMounted) onEditorMounted();
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

    if (mPresentation == Presentation::PanelOnly)
    {
        // Just the recessed panel bed the editor sits on.  No header strip and
        // no empty-state prompt: a panel window only exists for a loaded slot,
        // and it closes itself when that slot is cleared.
        g.setColour (VC::Panel);
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (VC::Accent.withAlpha (0.4f));
        g.drawRoundedRectangle (b, 3.0f, 1.0f);
        return;
    }

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


        EffectBypassLed::paint (g, mBypassRect, mBypassed);

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

        // Navigation glyphs.  A locked slot (the Vocal Chain) can't reorder or
        // close, and nothing hit-tests these rects - painting them there was
        // dead decoration promising gestures that don't exist (Jeff, 2026-08-13).
        if (! mLocked)
        {
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
}

void SlotComponent::setSlotIndex (int idx)
{
    if (idx == mSlotIndex || idx < 0 || idx >= EffectRack::kNumSlots) return;
    mSlotIndex = idx;
    refresh();
}

void SlotComponent::setPresentation (Presentation p)
{
    if (mPresentation == p) return;
    mPresentation = p;
    applyPresentationToChrome();
    resized();
    repaint();
}

// PanelOnly hides every header button unconditionally.  setEditor() decides
// which buttons a LOADED effect deserves, so this runs after it -- otherwise a
// slot rebuild would put the chrome back in a window that has no header to
// hold it.
void SlotComponent::applyPresentationToChrome()
{
    if (mPresentation != Presentation::PanelOnly) return;
    if (mScBtn)     mScBtn    ->setVisible (false);
    if (mModeBtn)   mModeBtn  ->setVisible (false);
    if (mPresetBtn) mPresetBtn->setVisible (false);
    if (mBasicBtn)  mBasicBtn ->setVisible (false);
}

bool SlotComponent::hasModeMenu() const
{
    if (! mRack || ! mRack->getSlotEffect (mSlotIndex)) return false;
    const auto t = mRack->getSlot (mSlotIndex).type;
    return (t == EffectType::Compressor || t == EffectType::Saturation
         || t == EffectType::Delay      || t == EffectType::Reverb
         || t == EffectType::Overdrive
         || t == EffectType::Limiter);   // TS7: Limiter / Maximizer
}

bool SlotComponent::hasScMenu() const
{
    if (! mRack) return false;
    auto* eff = mRack->getSlotEffect (mSlotIndex);
    return eff != nullptr && eff->usesSidechain();
}

bool SlotComponent::hasBasicMode() const
{
    if (auto* base = dynamic_cast<const EditorPanelBase*> (mEditor.get()))
        return base->hasAdvancedControls();
    return false;
}

bool SlotComponent::isBasicMode() const
{
    return mRack != nullptr && mRack->getSlotBasicMode (mSlotIndex);
}

juce::String SlotComponent::scLabel() const
{
    if (! mRack) return "SC: Off";
    const int pick = mRack->getSlotSidechainPick (mSlotIndex);
    if (pick < 0 || pick >= 4 || mApvts == nullptr || mChannelMixerPrefix.isEmpty())
        return "SC: Off";

    const juce::String pid = mChannelMixerPrefix + "_sc_recv" + juce::String (pick) + "_from";
    if (auto* p = mApvts->getRawParameterValue (pid))
    {
        const int srcId = (int) p->load();
        if (srcId >= 0)
        {
            juce::String n = mResolveSourceName ? mResolveSourceName (srcId) : juce::String();
            if (n.isEmpty()) n = juce::String ("Ch ") + juce::String (srcId);
            return "SC: " + n;
        }
    }
    return "SC: Off";
}

// ── Layout ────────────────────────────────────────────────────────────────────
void SlotComponent::resized()
{
    if (mPresentation == Presentation::PanelOnly)
    {
        if (mEditor) mEditor->setBounds (getLocalBounds().reduced (2));
        return;
    }

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
    // QA-ManualPress M-4c: the bypass dot and the slot name are PAINTED, so
    // they anchor as sub-rects of the header the paint pass uses.  Gated to
    // one slot: the Vocal Chain figure stacks six of these.
    if (mSlotIndex == 0)
        getProperties().set (kDotAnchor,
            "BSVC-2@" + juce::String (mBypassRect.getX()) + ","
                      + juce::String (mBypassRect.getY()) + ","
                      + juce::String (mBypassRect.getWidth()) + ","
                      + juce::String (mBypassRect.getHeight())
          + ";BSVC-3@" + juce::String (header.getX()) + ","
                       + juce::String (header.getY()) + ",90,"
                       + juce::String (header.getHeight()));
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
    // PanelOnly has no header, so there are no hit regions to dispatch on --
    // and the stale mBypassRect etc. from a previous Inline layout would
    // otherwise fire actions on clicks in the panel's own dead space.
    if (mPresentation == Presentation::PanelOnly) return;

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
}

// ── Popup menu (Change D: appears at cursor, alphabetical, no EQ) ─────────────
void SlotComponent::showAddMenu()
{
    showEffectPickerMenu (mLastMousePosScreen, {});
}

void SlotComponent::showEffectPickerMenu (juce::Point<int> screenPos,
                                          std::function<void(EffectType)> onPick,
                                          std::function<void(const juce::PluginDescription&)> onPickPlugin)
{
    // Grouped by effect family, alpha-sorted within each group.
    //
    // QA-ModelShell TS5 (2026-07-29, Jeff): the RACK effects are the top level
    // and the pedal-native types moved into a "Pedals" submenu.  Phase I had
    // alpha-merged the pedals into these groups (I-5 through I-11), which left
    // this menu 13-of-24 pedals and reading as a copy of the pedals board's own
    // picker.  Nothing is removed -- every pedal is still loadable into a rack
    // slot, one hop away -- and no saved project is affected, since slots load
    // by EffectType and never consult this menu.
    juce::PopupMenu m;
    m.addSectionHeader ("Dynamic");
    m.addItem ((int)EffectType::Compressor,          "Compressor");
    m.addItem ((int)EffectType::DeEsser,             "De-esser");
    m.addItem ((int)EffectType::Gate,                "Gate");
    m.addItem ((int)EffectType::Limiter,             "Limiter");
    m.addItem ((int)EffectType::TransientShaper,     "Transient Shaper");

    m.addSectionHeader ("Harmonics");
    m.addItem ((int)EffectType::Overdrive,          "Overdrive");
    m.addItem ((int)EffectType::Saturation,         "Saturation");
    // H-10 cutover (2026-05-02): Tape was folded into Saturation as a 3rd
    // type (Tube/Console/Tape).  Users now pick Saturation and switch the
    // Mode dropdown to Tape; the standalone "Tape" picker entry is gone.
    // EffectType::Tape stays in the enum + EffectRack as an alias so old
    // projects load correctly, but it's no longer in the picker.

    m.addSectionHeader ("Modulation");
    m.addItem ((int)EffectType::Chorus,          "Chorus");
    m.addItem ((int)EffectType::Flanger,         "Flanger");
    m.addItem ((int)EffectType::Phaser,          "Phaser");

    m.addSectionHeader ("Time");
    m.addItem ((int)EffectType::DeReverb,        "De-reverb");
    m.addItem ((int)EffectType::Delay,           "Delay");
    m.addItem ((int)EffectType::Reverb,          "Reverb");

    // Gate + De-reverb (QA-Fe2 types 119/120) were built as locked vocal-chain
    // stages and appeared in NO picker, so a rack slot could never hold either
    // one despite both having full DSP, a panel and TS3 automation tables.
    // Added to the rack picker 2026-07-29 (Jeff).  The vocal chain still pins
    // its own copies -- those slots are locked and never open this menu.

    // ── Pedals ───────────────────────────────────────────────────────────────
    // The BaySickPedals-native types.  I-15's exclusions still hold: Graphic EQ,
    // Bass Graphic EQ, Pro Parametric EQ and Tuner are board-only (fixed slots
    // there), and the User NAM Pedal loader is board-only too.
    {
        juce::PopupMenu pedals;
        pedals.addSectionHeader ("Dynamics");
        pedals.addItem ((int)EffectType::BassCompressorStyle, "Bass Compressor");
        pedals.addItem ((int)EffectType::NoiseGateStyle,      "Noise Gate");

        pedals.addSectionHeader ("Harmonics");
        pedals.addItem ((int)EffectType::BassDriverStyle,    "Bass Driver");
        pedals.addItem ((int)EffectType::BassOverdriveStyle, "Bass Overdrive");
        pedals.addItem ((int)EffectType::BluesDriveStyle,    "Blues Drive");
        pedals.addItem ((int)EffectType::DistortionStyle,    "Distortion");
        pedals.addItem ((int)EffectType::FuzzStyle,          "Fuzz");
        pedals.addItem ((int)EffectType::HighGainStyle,      "High-Gain");
        pedals.addItem ((int)EffectType::OctaveStyle,        "Octave");

        pedals.addSectionHeader ("Modulation");
        pedals.addItem ((int)EffectType::AcousticSimulatorStyle, "Acoustic Simulator");
        pedals.addItem ((int)EffectType::SynthStyle,             "Polyphonic Synth");
        pedals.addItem ((int)EffectType::WahStyle,               "Wah");

        pedals.addSectionHeader ("Time");
        pedals.addItem ((int)EffectType::AcousticPreampStyle, "Acoustic Preamp");

        // A GROUP whose heading is the dropdown, not an entry inside the group
        // above it (Jeff 2026-07-29).  TS6's "VST Plugins" group gets the same
        // treatment -- see the BLU-300 note in the batch plan.
        juce::PopupMenu::Item pedalsGroup;
        pedalsGroup.text    = "Pedals";
        pedalsGroup.subMenu = std::make_unique<juce::PopupMenu> (pedals);
        pedalsGroup.setCustomComponent (new HeaderSubMenuItem ("Pedals"));
        m.addItem (std::move (pedalsGroup));
    }

    // QA-ModelShell TS6 (BLU-300): the added EFFECT plugins, as a group whose
    // heading is itself the dropdown -- the same HeaderSubMenuItem the Pedals
    // group uses, per Jeff's spec.  Replaces TS5's disabled "VST3 Plugin..."
    // placeholder.  Instruments are deliberately absent: they belong to the
    // Plugins tab's "+" menu, and PluginDescription::isInstrument splits them
    // without loading anything.
    //
    // The whole section is gated on a plugin callback being supplied: the vocal
    // chain reaches this menu too, and its stages are locked, so offering rows
    // there that silently do nothing would be worse than not offering them.
    juce::Array<juce::PluginDescription> pluginEffects;

    if (onPickPlugin != nullptr)
        if (auto* pm = Hosting::PluginManager::getInstance())
            pluginEffects = pm->getAddedEffects();   // already alphabetical

    if (! pluginEffects.isEmpty())
    {
        juce::PopupMenu plugins;

        for (int i = 0; i < pluginEffects.size(); ++i)
            plugins.addItem (kVst3PickerItemId + i, pluginEffects.getReference (i).name);

        juce::PopupMenu::Item pluginsGroup;
        pluginsGroup.text    = "VST Plugins";
        pluginsGroup.subMenu = std::make_unique<juce::PopupMenu> (plugins);
        pluginsGroup.setCustomComponent (new HeaderSubMenuItem ("VST Plugins"));
        m.addItem (std::move (pluginsGroup));
    }
    else if (onPickPlugin != nullptr)
    {
        // Shown-but-disabled rather than hidden: with no row at all, a user who
        // has not added anything yet cannot tell plugin hosting exists.
        m.addSectionHeader ("VST Plugins");
        m.addItem (kVst3PickerItemId - 1, "None added - see Options > Plugins", false, false);
    }

    auto opts = juce::PopupMenu::Options()
        .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 });

    if (shots::maybeCapture (m, opts)) return;
    m.showMenuAsync (opts,
        [pick = std::move (onPick), pickPlugin = std::move (onPickPlugin), pluginEffects] (int result)
        {
            if (result < 1) return;

            if (result >= kVst3PickerItemId)
            {
                const int idx = result - kVst3PickerItemId;

                if (pickPlugin && juce::isPositiveAndBelow (idx, pluginEffects.size()))
                    pickPlugin (pluginEffects.getReference (idx));

                return;
            }

            if (pick) pick ((EffectType) result);
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// H-7 (2026-05-01): Mode dropdown -- character-mode picker per effect type.
// Compressor:  Modern (0) / FET (1) / Opto (2)
// Saturation:  Tube (0)   / Console (1)
// ─────────────────────────────────────────────────────────────────────────────
void SlotComponent::refreshModeBtnLabel()
{
    if (! mModeBtn) return;
    mModeBtn->setButtonText (modeLabel());
}

// QA-ModelShell TS5: the label computation moved out of the button refresh so
// the panel window's title-bar menu can show the same current-mode text.
juce::String SlotComponent::modeLabel() const
{
    if (! mRack) return "Mode";
    const auto& slot = mRack->getSlot(mSlotIndex);
    if (! mRack->getSlotEffect(mSlotIndex)) return "Mode";

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
                case CompressorDSP::Type::CS:     label = "Pedal";    break;
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
    else if (slot.type == EffectType::Limiter)
    {
        // TS7: Limiter is the FL reproduction; Maximizer is the loudness suite.
        if (auto* l = dynamic_cast<LimiterDSP*>(mRack->getSlotEffect(mSlotIndex)))
            label = LimiterDSP::modeName (l->getModeIndex());
    }
    return label;
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
        // descriptors per locked spec (option B).  Pedal is the 4th Type
        // alongside Modern/FET/Opto.
        m.addItem(1 + (int) CompressorDSP::Type::Modern, "Modern",            true,
                  currentPick == (int) CompressorDSP::Type::Modern);
        m.addItem(1 + (int) CompressorDSP::Type::FET,    "FET (Punchy)",      true,
                  currentPick == (int) CompressorDSP::Type::FET);
        m.addItem(1 + (int) CompressorDSP::Type::Opto,   "Opto (Smooth)",     true,
                  currentPick == (int) CompressorDSP::Type::Opto);
        // Vocal Chain does not get Pedal (Jeff, 2026-08-11).  A pedal sustainer
        // is not a vocal-chain compressor, and the chain's bsv_comp_type spans
        // Modern/FET/Opto only -- offering it here mounted its panel and let
        // applyChainParams override the DSP back on the next audio block.
        if (! mVocalChainSlot)
            m.addItem(1 + (int) CompressorDSP::Type::CS, "Pedal (Sustain)",   true,
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
    else if (slot.type == EffectType::Limiter)
    {
        // TS7: Limiter is the reproduction, Maximizer is the loudness suite.  The
        // two expose different control sets, which is why mode is a real
        // EffectParamMap variant rather than a display-only selector.
        if (auto* l = dynamic_cast<LimiterDSP*>(mRack->getSlotEffect(mSlotIndex)))
            currentPick = l->getModeIndex();
        m.addItem(1 + (int) LimiterDSP::Mode::Limiter,   "Limiter (Reproduction)", true,
                  currentPick == (int) LimiterDSP::Mode::Limiter);
        m.addItem(1 + (int) LimiterDSP::Mode::Maximizer, "Maximizer (Loudness)",   true,
                  currentPick == (int) LimiterDSP::Mode::Maximizer);
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
                    else if (slotType == EffectType::Limiter)
                    {
                        // TS7: setMode also drops the live servo/trim state so a
                        // hidden maximizer offset cannot keep driving the sound
                        // in reproduction mode.
                        if (auto* l = dynamic_cast<LimiterDSP*>(eff))
                            l->setMode ((LimiterDSP::Mode) newType);
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
    {
        // Outgoing panel dies BEFORE its replacement is built -- see the same
        // note in EffectSlotWindow::buildPanel.  A hosted plugin has exactly
        // one editor instance, so overlapping the two would hand the new panel
        // an editor the old one still owns.
        setEditor (nullptr);
        setEditor (createEffectEditor (eff, slotNow.type));
    }
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
                    if (result != 1) return;
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
                // Anything the preset's DSP could not find (a NAM capture, a
                // user IR) records itself instead of failing loudly, so this
                // gesture has to drain -- an undrained entry otherwise
                // surfaces later attached to an unrelated load.  The scope
                // rather than a tail call to reportIfAny: it covers the early
                // return too, and a bare drain inside an outer gesture would
                // steal that gesture's entries and post them under this noun.
                MissingFileReport::ScopedGesture gesture ("preset");

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
        // QA-Fe2 vocal-chain stages.  QA-ModelShell TS5 also added both to the
        // rack picker after Jeff found them unreachable there.
        case EffectType::Gate:            return "Gate";
        case EffectType::DeReverb:        return "De-reverb";
        // QA-ModelShell TS6: fallback only.  Every surface that has the rack
        // and the slot index should call slotDisplayName instead, which names
        // the actual plugin.
        case EffectType::VST3Plugin:      return "VST3 Plugin";

        // Only the pedalboard's compact slot dropdown asks for this one - the
        // pedal tile names a NAM slot after the loaded capture instead, so the
        // missing case left the dropdown rendering a loaded NAM pedal as the
        // default "-" while the tile below it read correctly.
        case EffectType::NAMPedalStyle:   return "NAM Pedal";

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

// QA-ModelShell TS6: the display name for a LOADED slot.  Everything except a
// hosted plugin is named by its type; a plugin slot has to ask the DSP, because
// one EffectType ordinal covers every plugin.  One home for that rule so the
// row, the window title and the rack preset cannot disagree.
juce::String SlotComponent::slotDisplayName (const EffectRack* rack, int slot)
{
    if (rack == nullptr)
        return "-";

    const auto type = rack->getSlotType (slot);

    if (type == EffectType::VST3Plugin)
        if (auto* hosted = dynamic_cast<const Hosting::HostedPluginEffect*>(rack->getSlotEffect (slot)))
        {
            const auto name = hosted->getPluginName();

            if (name.isNotEmpty())
            {
                // The STORED description names the slot, so a plugin whose DLL
                // moved, whose bridge helper never came up, or that died
                // mid-session kept presenting as a working effect while it was
                // in fact passing audio through untouched (in-process) or
                // clearing the bus (bridged).  Asked LIVE rather than at build
                // time: a bridged load result and a crash both land after the
                // name is first rendered, and every surface polls this.
                auto* inst = hosted->getHosted();

                if (inst != nullptr && ! inst->isAlive())
                    return name + " (missing)";

                return name;
            }
        }

    return effectTypeName (type);
}
