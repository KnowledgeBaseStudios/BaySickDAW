#include "SlotComponent.h"
#include "EffectEditorPanels.h"

SlotComponent::SlotComponent(int slotIndex) : mSlotIndex(slotIndex)
{
    setInterceptsMouseClicks(true, true);
    startTimerHz(30);  // 30fps level feed to meters
}

SlotComponent::~SlotComponent()
{
    stopTimer();
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
        mBypassed = slot.bypassed;

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
        addAndMakeVisible(*mEditor);

        // Wire output vol knob → rack slot gain
        if (auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get()))
        {
            const int slot = mSlotIndex;
            base->onOutputGainChanged = [this, slot](float db) {
                if (mRack) mRack->setSlotOutputGain(slot, db);
            };
            if (mRack)
                base->outputVolKnob->slider.setValue(mRack->getSlotOutputGain(slot),
                                                     juce::dontSendNotification);
        }
    }

    resized();
    repaint();
}

void SlotComponent::setEditorUndoContext(const UndoContext& ctx)
{
    if (auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get()))
        base->setUndoContext(ctx);
}

void SlotComponent::timerCallback()
{
    if (!mRack || !mEditor) return;
    auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get());
    if (!base) return;
    base->setInputLevel (mRack->getSlotInputLevel (mSlotIndex));
    base->setOutputLevel(mRack->getSlotOutputLevel(mSlotIndex));
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

        // Effect name (between bypass and up-arrow)
        int nameX = mBypassRect.getRight() + 4;
        int nameW = mUpRect.getX() - nameX - 4;
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

    // Action glyphs on the right (close, then down, then up — right-to-left)
    mCloseRect = header.removeFromRight(24).withSizeKeepingCentre(20, 20);
    mDownRect  = header.removeFromRight(24).withSizeKeepingCentre(20, 20);
    mUpRect    = header.removeFromRight(24).withSizeKeepingCentre(20, 20);
    header.removeFromRight(2);

    // Editor fills the rest (fader column already removed from b)
    if (mEditor)
        mEditor->setBounds(b.reduced(2, 2));
}

// ── Mouse ─────────────────────────────────────────────────────────────────────
void SlotComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!mLoaded)
    {
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
    else if (mUpRect.contains(pos))
    {
        if (onMoveRequested) onMoveRequested(mSlotIndex, true);
    }
    else if (mDownRect.contains(pos))
    {
        if (onMoveRequested) onMoveRequested(mSlotIndex, false);
    }
    else if (mCloseRect.contains(pos))
    {
        if (onEffectRemoved) onEffectRemoved(mSlotIndex);
    }
}

// ── Popup menu (Change D: appears at cursor, alphabetical, no EQ) ─────────────
void SlotComponent::showAddMenu()
{
    juce::PopupMenu m;
    m.addItem((int)EffectType::Chorus,          "Chorus");
    m.addItem((int)EffectType::Compressor,      "Compressor");
    m.addItem((int)EffectType::Delay,           "Delay");
    m.addItem((int)EffectType::Flanger,         "Flanger");
    m.addItem((int)EffectType::Limiter,         "Limiter");
    m.addItem((int)EffectType::Overdrive,       "Overdrive");
    m.addItem((int)EffectType::Phaser,          "Phaser");
    m.addItem((int)EffectType::Reverb,          "Reverb");
    m.addItem((int)EffectType::Saturation,      "Saturation");
    m.addItem((int)EffectType::Tape,            "Tape");
    m.addItem((int)EffectType::TransientShaper, "Transient Shaper");

    auto opts = juce::PopupMenu::Options()
        .withTargetScreenArea({ mLastMousePosScreen.x, mLastMousePosScreen.y, 1, 1 });

    m.showMenuAsync(opts,
        [this](int result)
        {
            if (result < 1) return;
            if (onEffectChosen) onEffectChosen(mSlotIndex, (EffectType)result);
        });
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
        default:                          return "-";
    }
}
