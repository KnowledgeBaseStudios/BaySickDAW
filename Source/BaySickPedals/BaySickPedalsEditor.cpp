#include "BaySickPedalsEditor.h"
#include "../Standalone/EffectEditorPanels.h"
#include "../Standalone/SlotComponent.h"   // SlotComponent::effectTypeName
#include "../Standalone/EffectPresetIO.h"
#include "../DSP/DSPBase.h"
#include "../DSP/NAMPedalStyleDSP.h"

// ─────────────────────────────────────────────────────────────────────────────
// PedalArrowButton - tiny custom button that paints a filled triangle.  Used for
// the per-tile Up/Down reorder controls.  No text labels (Unicode arrows
// would render as box glyphs per the project's ASCII-only UI rule).
// ─────────────────────────────────────────────────────────────────────────────
class PedalArrowButton : public juce::Button
{
public:
    enum Direction { Up, Down };
    explicit PedalArrowButton (Direction d) : juce::Button ({}), mDir (d) {}

    void paintButton (juce::Graphics& g, bool isOver, bool isDown) override
    {
        const auto b = getLocalBounds().toFloat();
        if (isDown)      g.setColour (juce::Colour (0xff444444));
        else if (isOver) g.setColour (juce::Colour (0xff333333));
        else             g.setColour (juce::Colour (0xff222222));
        g.fillRoundedRectangle (b.reduced (0.5f), 2.0f);

        const float pad = 4.0f;
        juce::Path p;
        if (mDir == Up)
        {
            p.addTriangle (b.getCentreX(),         b.getY() + pad,
                           b.getX() + pad,         b.getBottom() - pad,
                           b.getRight() - pad,     b.getBottom() - pad);
        }
        else
        {
            p.addTriangle (b.getX() + pad,         b.getY() + pad,
                           b.getRight() - pad,     b.getY() + pad,
                           b.getCentreX(),         b.getBottom() - pad);
        }
        g.setColour (isEnabled() ? juce::Colour (0xffe0e0e0) : juce::Colour (0xff555555));
        g.fillPath (p);
    }
private:
    Direction mDir;
};

namespace
{
    constexpr int kTitleH    = 24;
    constexpr int kBypassH   = 28;
    constexpr int kGridGap   = 10;
    constexpr int kPagePad   = 12;
    constexpr int kCols      = 4;
    constexpr int kRows      = 2;

    juce::String slotBypassParamId (int slot)
    {
        return "bsp_slot" + juce::String (slot) + "_bypass";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PedalSlotComponent - one pedal tile.
// ─────────────────────────────────────────────────────────────────────────────
class PedalSlotComponent : public juce::Component
{
public:
    PedalSlotComponent (BaySickPedalsProcessor& proc, int slot, BaySickPedalsEditor& parentEditor)
        : mProc (proc), mSlot (slot), mParent (parentEditor)
    {
        // Title label.
        mTitle.setJustificationType (juce::Justification::centredLeft);
        mTitle.setFont (juce::Font (12.0f, juce::Font::bold));
        mTitle.setColour (juce::Label::textColourId, juce::Colour (0xffe0e0e0));
        mTitle.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (mTitle);

        // Per-pedal preset menu button (3-dot).
        mPresetBtn = std::make_unique<juce::TextButton>("...");
        mPresetBtn->setTooltip ("Pedal preset menu (Save / Load / Restore Default)");
        mPresetBtn->onClick = [this] { showPresetMenu(); };
        addAndMakeVisible (*mPresetBtn);

        // I-15 polish: explicit "X" remove button on slots 1-6 when populated.
        // (Slot 0 / 7 are locked-position and never removed.)
        if (mSlot != BaySickPedalsProcessor::kSlotTuner
            && mSlot != BaySickPedalsProcessor::kSlotEQ)
        {
            mRemoveBtn = std::make_unique<juce::TextButton>("X");
            mRemoveBtn->setTooltip ("Remove this pedal (revert slot to empty)");
            mRemoveBtn->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff332020));
            mRemoveBtn->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff8080));
            mRemoveBtn->onClick = [this]
            {
                mProc.clearSlot (mSlot);
                mParent.onSlotTypeChanged (mSlot);
            };
            addAndMakeVisible (*mRemoveBtn);

            // I-15 polish (2026-05-03): up/down reorder buttons (mirror the FX
            // rack pattern).  Drag-to-reorder still in place but unreliable, so
            // these buttons give an explicit reorder path.  Filled triangle
            // glyphs painted by PedalArrowButton (no text -- ASCII-only rule).
            mUpBtn = std::make_unique<PedalArrowButton>(PedalArrowButton::Up);
            mUpBtn->setTooltip ("Move this pedal one slot earlier in the chain");
            mUpBtn->onClick = [this]
            {
                mParent.performMove (mSlot, mSlot - 1);
            };
            addAndMakeVisible (*mUpBtn);

            mDownBtn = std::make_unique<PedalArrowButton>(PedalArrowButton::Down);
            mDownBtn->setTooltip ("Move this pedal one slot later in the chain");
            mDownBtn->onClick = [this]
            {
                mParent.performMove (mSlot, mSlot + 1);
            };
            addAndMakeVisible (*mDownBtn);
        }

        // Slot 7 (EQ) gets a header dropdown for the 3 EQ types.
        if (mSlot == BaySickPedalsProcessor::kSlotEQ)
        {
            mEqPicker = std::make_unique<juce::ComboBox>();
            mEqPicker->addItem ("Graphic EQ",        1);
            mEqPicker->addItem ("Bass Graphic EQ",   2);
            mEqPicker->addItem ("Pro Parametric EQ", 3);
            mEqPicker->onChange = [this] { onEqPickerChanged(); };
            addAndMakeVisible (*mEqPicker);
        }

        // Bypass LED footswitch.
        mBypassBtn = std::make_unique<juce::TextButton>();
        mBypassBtn->setClickingTogglesState (true);
        // Bypass = true means OFF (audio passes through unchanged).  LED is lit
        // (green) when the pedal is engaged (NOT bypassed).
        mBypassBtn->setButtonText ("ON");
        mBypassBtn->setColour (juce::TextButton::buttonColourId,    juce::Colour (0xff222222));
        mBypassBtn->setColour (juce::TextButton::buttonOnColourId,  juce::Colour (0xff333333));
        mBypassBtn->setColour (juce::TextButton::textColourOffId,   juce::Colour (0xff44ff88));
        mBypassBtn->setColour (juce::TextButton::textColourOnId,    juce::Colour (0xff666666));
        mBypassBtn->setTooltip ("Bypass this pedal");
        addAndMakeVisible (*mBypassBtn);
        mBypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            mProc.apvts, slotBypassParamId (mSlot), *mBypassBtn);

        rebuild();
    }

    ~PedalSlotComponent() override
    {
        mBypassAttach.reset();
    }

    void rebuild()
    {
        const auto type = mProc.getSlotType (mSlot);

        // Title text.  Special case: NAM Pedal shows the loaded .nam filename
        // (without extension) so the user can tell at a glance which capture
        // is in this slot.  Falls back to the type name when no model is
        // loaded yet.
        if (type == EffectType::None)
        {
            mTitle.setText ("Empty", juce::dontSendNotification);
        }
        else if (type == EffectType::NAMPedalStyle)
        {
            juce::String name;
            if (auto* dsp = dynamic_cast<NAMPedalStyleDSP*> (mProc.getSlotEffect (mSlot)))
                name = dsp->getModelName();
            mTitle.setText (name.isNotEmpty() ? name : juce::String ("NAM Pedal (no file)"),
                            juce::dontSendNotification);
        }
        else
        {
            mTitle.setText (SlotComponent::effectTypeName (type), juce::dontSendNotification);
        }

        // EQ picker selection (slot 7 only).
        if (mEqPicker)
        {
            const int id = type == EffectType::GraphicEQStyle     ? 1
                         : type == EffectType::BassGraphicEQStyle ? 2
                         : type == EffectType::FurmanEQStyle      ? 3
                         : 1;
            mEqPicker->setSelectedId (id, juce::dontSendNotification);
        }

        // Hide preset / remove / up / down / bypass on empty slots.  Up/down
        // are also disabled when the pedal is already at the chain extremes
        // (slot 1 = first user-movable -> Up disabled; slot 6 = last -> Down).
        const bool populated = (type != EffectType::None);
        if (mPresetBtn) mPresetBtn->setVisible (populated);
        if (mRemoveBtn) mRemoveBtn->setVisible (populated);
        if (mBypassBtn) mBypassBtn->setVisible (populated);
        if (mUpBtn)
        {
            mUpBtn  ->setVisible (populated);
            mUpBtn  ->setEnabled (populated && mSlot > BaySickPedalsProcessor::kSlotTuner + 1);
        }
        if (mDownBtn)
        {
            mDownBtn->setVisible (populated);
            mDownBtn->setEnabled (populated && mSlot < BaySickPedalsProcessor::kSlotEQ - 1);
        }

        // (Re)build the inner panel for the new type, in pedal mode.
        if (mPanel)
        {
            removeChildComponent (mPanel.get());
            mPanel.reset();
        }
        if (type != EffectType::None)
        {
            if (auto* dsp = mProc.getSlotEffect (mSlot))
            {
                mPanel = createEffectEditor (dsp, type, EditorPanelBase::PanelMode::Pedal);
                if (mPanel)
                    addAndMakeVisible (*mPanel);
            }
        }

        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto type   = mProc.getSlotType (mSlot);
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const bool locked = (mSlot == BaySickPedalsProcessor::kSlotTuner
                          || mSlot == BaySickPedalsProcessor::kSlotEQ);

        if (type == EffectType::None && ! locked)
        {
            // Empty slot: dashed outline + giant + sign (mirrors FX rack empty slot).
            juce::Path p;
            p.addRoundedRectangle (bounds, 6.0f);
            juce::Path dashed;
            const float dashes[] = { 6.0f, 6.0f };
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, p, dashes, 2);
            g.setColour (juce::Colour (0xff444444));
            g.fillPath (dashed);

            // + sign
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            const float arm = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.18f;
            const float w   = 4.0f;
            g.setColour (juce::Colour (0xff666666));
            g.fillRoundedRectangle (cx - arm, cy - w * 0.5f, arm * 2.0f, w, 1.0f);
            g.fillRoundedRectangle (cx - w * 0.5f, cy - arm, w, arm * 2.0f, 1.0f);
            return;
        }

        // Loaded / locked slot: dark panel with optional chrome bezel.
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillRoundedRectangle (bounds, 6.0f);

        if (locked)
        {
            // Chrome bezel for the locked-position slots.
            juce::ColourGradient grad (juce::Colour (0xff909098), bounds.getX(), bounds.getY(),
                                        juce::Colour (0xff404048), bounds.getX(), bounds.getBottom(),
                                        false);
            g.setGradientFill (grad);
            g.drawRoundedRectangle (bounds, 6.0f, 2.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff333333));
            g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
        }

        // I-15 polish: drop-target highlight during drag-to-reorder.
        if (mIsDropTarget)
        {
            g.setColour (juce::Colour (0xff44ff88).withAlpha (0.7f));
            g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, 3.0f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4);

        // Title row (top).
        auto title = b.removeFromTop (kTitleH);

        // Right side of title row (read right-to-left order in code = visual
        // right-to-left positions): X (remove) | "..." (preset) | Dn | Up | title text...
        if (mRemoveBtn && mRemoveBtn->isVisible())
            mRemoveBtn->setBounds (title.removeFromRight (22).reduced (2));
        if (mPresetBtn && mPresetBtn->isVisible())
            mPresetBtn->setBounds (title.removeFromRight (24).reduced (2));
        if (mDownBtn && mDownBtn->isVisible())
            mDownBtn->setBounds (title.removeFromRight (24).reduced (2));
        if (mUpBtn && mUpBtn->isVisible())
            mUpBtn  ->setBounds (title.removeFromRight (24).reduced (2));

        // Slot 7: EQ-picker dropdown takes the left half of the title row.
        if (mEqPicker)
        {
            mEqPicker->setBounds (title.reduced (2, 1));
        }
        else
        {
            mTitle.setBounds (title.reduced (4, 0));
        }

        // Bottom row: bypass LED.
        auto bypassRow = b.removeFromBottom (kBypassH);
        if (mBypassBtn && mBypassBtn->isVisible())
            mBypassBtn->setBounds (bypassRow.withSizeKeepingCentre (60, 22));

        // Middle: the effect panel.
        if (mPanel) mPanel->setBounds (b);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Right-click anywhere on the tile -> Change Pedal menu (per locked spec).
        if (e.mods.isPopupMenu())
        {
            showChangePedalMenu();
            return;
        }

        // Left-click on an empty (unlocked) slot opens the Change Pedal menu.
        if (mProc.getSlotType (mSlot) == EffectType::None
            && mSlot != BaySickPedalsProcessor::kSlotTuner
            && mSlot != BaySickPedalsProcessor::kSlotEQ)
        {
            showChangePedalMenu();
            return;
        }

        // Drag-to-reorder is initiated only from the title bar of slots 1-6
        // (locked slots 0 / 7 cannot be moved).
        if (canDrag()
            && e.y < kTitleH
            && mProc.getSlotType (mSlot) != EffectType::None)
        {
            mDragArmed = true;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! mDragArmed) return;

        // Translate the event's local position into the editor's coordinate
        // space, then ask the editor which slot is under the cursor.
        const auto editorPos = mParent.getLocalPoint (this, e.position).roundToInt();
        const int dropSlot = mParent.findSlotIndexAt (editorPos);
        mParent.setDropTargetSlot (dropSlot);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! mDragArmed) return;
        mDragArmed = false;

        const auto editorPos = mParent.getLocalPoint (this, e.position).roundToInt();
        const int dropSlot = mParent.findSlotIndexAt (editorPos);
        mParent.setDropTargetSlot (-1);   // clear highlight

        if (dropSlot >= 0 && dropSlot != mSlot)
            mParent.performMove (mSlot, dropSlot);
    }

    bool canDrag() const noexcept
    {
        return mSlot != BaySickPedalsProcessor::kSlotTuner
            && mSlot != BaySickPedalsProcessor::kSlotEQ;
    }

    void setDropTarget (bool on)
    {
        if (mIsDropTarget != on) { mIsDropTarget = on; repaint(); }
    }

private:
    void showChangePedalMenu()
    {
        // Slot 0 (Tuner) is fully locked -- no menu.
        if (mSlot == BaySickPedalsProcessor::kSlotTuner) return;

        juce::PopupMenu m;
        // Menu IDs use the EffectType enum value directly; reserved high IDs
        // (>=10000) carry slot-management actions that don't collide with the
        // enum.  Earlier code hardcoded 101..103 which collided with the
        // enum values for Distortion/Fuzz/HighGain -- fixed 2026-05-03.
        constexpr int kClearId = 10000;

        if (mSlot == BaySickPedalsProcessor::kSlotEQ)
        {
            m.addSectionHeader ("Change EQ");
            const auto cur = mProc.getSlotType (mSlot);
            m.addItem ((int) EffectType::GraphicEQStyle,     "Graphic EQ",        true, cur == EffectType::GraphicEQStyle);
            m.addItem ((int) EffectType::BassGraphicEQStyle, "Bass Graphic EQ",   true, cur == EffectType::BassGraphicEQStyle);
            m.addItem ((int) EffectType::FurmanEQStyle,      "Pro Parametric EQ", true, cur == EffectType::FurmanEQStyle);
        }
        else
        {
            // Slots 1-6: 14 swappable pedals (alpha-sorted within their LAF group).
            buildSwappableMenu (m, mProc.getSlotType (mSlot));
            m.addSeparator();
            m.addItem (kClearId, "Clear", mProc.getSlotType (mSlot) != EffectType::None);
        }

        m.showMenuAsync (juce::PopupMenu::Options(),
            [this] (int r)
            {
                if (r == 0) return;
                if (r == kClearId)
                {
                    mProc.clearSlot (mSlot);
                    mParent.onSlotTypeChanged (mSlot);
                    return;
                }

                const auto picked = (EffectType) r;
                if (! mProc.isEffectAllowedInSlot (mSlot, picked)) return;

                mProc.loadEffect (mSlot, picked);
                mParent.onSlotTypeChanged (mSlot);

                // I-15c (2026-05-03): if NAM Pedal was just loaded, immediately
                // prompt for a .nam file -- the panel is useless without one.
                if (picked == EffectType::NAMPedalStyle)
                    promptForNAMFile();
            });
    }

    void promptForNAMFile()
    {
        auto* dsp = dynamic_cast<NAMPedalStyleDSP*> (mProc.getSlotEffect (mSlot));
        if (dsp == nullptr) return;

        auto userPedals = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                              .getChildFile ("BaySickDAW")
                              .getChildFile ("Presets")
                              .getChildFile ("Effects")
                              .getChildFile ("Pedals")
                              .getChildFile ("User NAM Pedals");
        userPedals.createDirectory();

        mNamChooser = std::make_unique<juce::FileChooser>("Load NAM Pedal", userPedals, "*.nam");
        mNamChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                auto* dsp = dynamic_cast<NAMPedalStyleDSP*> (mProc.getSlotEffect (mSlot));
                if (dsp == nullptr) return;
                juce::String err;
                if (! dsp->loadModel (f, err))
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::WarningIcon, "NAM Load Failed", err);
                    return;
                }
                mParent.onSlotTypeChanged (mSlot);   // forces title/panel refresh
            });
    }

    static void buildSwappableMenu (juce::PopupMenu& m, EffectType current)
    {
        auto add = [&] (EffectType t, const char* label)
        {
            m.addItem ((int) t, label, true, current == t);
        };

        // I-15c (2026-05-03): User NAM Pedal section sits at the top -- own
        // section per locked spec.
        m.addSectionHeader ("User NAM Pedal");
        add (EffectType::NAMPedalStyle, "Load NAM Pedal");

        m.addSectionHeader ("Dynamics");
        add (EffectType::BassCompressorStyle, "Bass Compressor");
        add (EffectType::Compressor,          "Compressor");
        add (EffectType::NoiseGateStyle,      "Noise Gate");

        m.addSectionHeader ("Harmonics");
        add (EffectType::BassDriverStyle,    "Bass Driver");
        add (EffectType::BassOverdriveStyle, "Bass Overdrive");
        add (EffectType::BluesDriveStyle,    "Blues Drive");
        add (EffectType::DistortionStyle,    "Distortion");
        add (EffectType::FuzzStyle,          "Fuzz");
        add (EffectType::HighGainStyle,      "High-Gain");
        add (EffectType::OctaveStyle,        "Octave");
        add (EffectType::Overdrive,          "Overdrive");
        add (EffectType::Saturation,         "Saturation");

        m.addSectionHeader ("Modulation");
        add (EffectType::AcousticSimulatorStyle, "Acoustic Simulator");
        add (EffectType::Chorus,                 "Chorus");
        add (EffectType::Flanger,                "Flanger");
        add (EffectType::Phaser,                 "Phaser");
        add (EffectType::SynthStyle,             "Polyphonic Synth");
        add (EffectType::WahStyle,               "Wah");

        m.addSectionHeader ("Time");
        add (EffectType::AcousticPreampStyle, "Acoustic Preamp");
        add (EffectType::Delay,               "Delay");
        add (EffectType::Reverb,              "Reverb");
    }

    void onEqPickerChanged()
    {
        if (! mEqPicker) return;
        const int id = mEqPicker->getSelectedId();
        const EffectType target = id == 1 ? EffectType::GraphicEQStyle
                                : id == 2 ? EffectType::BassGraphicEQStyle
                                : id == 3 ? EffectType::FurmanEQStyle
                                          : EffectType::GraphicEQStyle;
        if (mProc.getSlotType (mSlot) == target) return;
        mProc.loadEffect (mSlot, target);
        mParent.onSlotTypeChanged (mSlot);
    }

    void showPresetMenu()
    {
        const auto type = mProc.getSlotType (mSlot);
        if (type == EffectType::None) return;

        auto* dsp = mProc.getSlotEffect (mSlot);
        if (! dsp) return;

        EffectPresetIO::ensureFolderTree (type);

        juce::PopupMenu m;
        m.addItem (1, "Save Preset As...");
        m.addSeparator();

        // Factory presets submenu.
        auto factoryFiles = EffectPresetIO::enumerateFactory (type);
        juce::PopupMenu factoryMenu;
        if (factoryFiles.isEmpty())
            factoryMenu.addItem (-1, "(none)", false);
        else
            for (int i = 0; i < factoryFiles.size(); ++i)
                factoryMenu.addItem (1000 + i, factoryFiles[i].getFileNameWithoutExtension());
        m.addSubMenu ("Factory Presets", factoryMenu);

        auto myFiles = EffectPresetIO::enumerateMyPresets (type);
        juce::PopupMenu myMenu;
        if (myFiles.isEmpty())
            myMenu.addItem (-1, "(none)", false);
        else
            for (int i = 0; i < myFiles.size(); ++i)
                myMenu.addItem (2000 + i, myFiles[i].getFileNameWithoutExtension());
        m.addSubMenu ("My Presets", myMenu);

        m.addSeparator();
        m.addItem (10, "Restore Defaults");
        m.addItem (11, "Save as Default");
        m.addItem (12, "Reveal Folder...");

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mPresetBtn.get()),
            [this, dsp, type, factoryFiles, myFiles] (int r)
            {
                if (r == 0) return;
                if (r == 1)
                {
                    auto* aw = new juce::AlertWindow ("Save Preset", "Preset name:", juce::AlertWindow::NoIcon);
                    aw->addTextEditor ("name", "");
                    aw->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
                    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                    aw->enterModalState (true,
                        juce::ModalCallbackFunction::create (
                            [aw, dsp, type] (int result)
                            {
                                if (result == 1)
                                {
                                    const auto name = aw->getTextEditorContents ("name").trim();
                                    if (name.isNotEmpty())
                                    {
                                        juce::String err;
                                        EffectPresetIO::savePreset (*dsp, type, name, err);
                                    }
                                }
                                delete aw;
                            }),
                        false);
                    return;
                }
                if (r == 10) { EffectPresetIO::restoreDefaults (*dsp, type); return; }
                if (r == 11) { juce::String err; EffectPresetIO::saveAsDefault (*dsp, type, err); return; }
                if (r == 12) { EffectPresetIO::typeRoot (type).revealToUser(); return; }

                if (r >= 1000 && r < 2000)
                {
                    const int idx = r - 1000;
                    if (idx >= 0 && idx < factoryFiles.size())
                    {
                        juce::String err;
                        EffectPresetIO::loadPreset (*dsp, factoryFiles[idx], err);
                    }
                    return;
                }
                if (r >= 2000)
                {
                    const int idx = r - 2000;
                    if (idx >= 0 && idx < myFiles.size())
                    {
                        juce::String err;
                        EffectPresetIO::loadPreset (*dsp, myFiles[idx], err);
                    }
                }
            });
    }

    BaySickPedalsProcessor& mProc;
    int                      mSlot;
    BaySickPedalsEditor&     mParent;

    juce::Label                              mTitle;
    std::unique_ptr<juce::TextButton>        mPresetBtn;
    std::unique_ptr<juce::TextButton>        mRemoveBtn;
    std::unique_ptr<PedalArrowButton>             mUpBtn;
    std::unique_ptr<PedalArrowButton>             mDownBtn;
    std::unique_ptr<juce::ComboBox>          mEqPicker;
    std::unique_ptr<juce::Component>         mPanel;
    std::unique_ptr<juce::TextButton>        mBypassBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mBypassAttach;

    bool mDragArmed     { false };
    bool mIsDropTarget  { false };

    std::unique_ptr<juce::FileChooser> mNamChooser;   // I-15c: NAM Pedal file picker

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalSlotComponent)
};

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPedalsEditor
// ─────────────────────────────────────────────────────────────────────────────
BaySickPedalsEditor::BaySickPedalsEditor (BaySickPedalsProcessor& proc)
    : juce::AudioProcessorEditor (&proc),
      mProc (proc)
{
    setSize (880, 480);
    for (int s = 0; s < BaySickPedalsProcessor::kNumSlots; ++s)
    {
        mTiles[s] = std::make_unique<PedalSlotComponent>(proc, s, *this);
        addAndMakeVisible (*mTiles[s]);
        mLastTypes[s] = proc.getSlotType (s);
    }
    startTimerHz (10);

    // 2026-05-05 (Bug B fix): subscribe to bulk-restore notifications so we
    // rebuild every tile when the processor swaps DSP pointers behind our
    // back (page preset load, project load, pedalboard preset load).  The
    // timer-based rebuild only catches type-CHANGES - same-type DSP swaps
    // need this explicit signal.
    juce::Component::SafePointer<BaySickPedalsEditor> safeThis (this);
    mProc.onSlotsExternallyChanged = [safeThis]
    {
        if (auto* e = safeThis.getComponent())
        {
            for (int s = 0; s < BaySickPedalsProcessor::kNumSlots; ++s)
            {
                e->mLastTypes[s] = e->mProc.getSlotType (s);
                if (e->mTiles[s]) e->mTiles[s]->rebuild();
            }
        }
    };
}

BaySickPedalsEditor::~BaySickPedalsEditor()
{
    // Defensive cleanup: stop the rebuild-detector timer FIRST so it can't
    // fire on a half-destroyed tile array, then drop the external-change
    // callback (the lambda's SafePointer would also catch this, but explicit
    // is cheaper than waiting for the next message-thread pump), then
    // explicitly tear down each tile (which cleans the APVTS button
    // attachments + child panels) before the base destructor runs.
    stopTimer();
    mProc.onSlotsExternallyChanged = nullptr;
    for (auto& tile : mTiles) tile.reset();
}

void BaySickPedalsEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121212));
}

void BaySickPedalsEditor::resized()
{
    auto b = getLocalBounds().reduced (kPagePad);
    if (b.isEmpty()) return;

    const int tileW = (b.getWidth()  - kGridGap * (kCols - 1)) / kCols;
    const int tileH = (b.getHeight() - kGridGap * (kRows - 1)) / kRows;

    for (int s = 0; s < BaySickPedalsProcessor::kNumSlots; ++s)
    {
        const int row = s / kCols;
        const int col = s % kCols;
        const auto rect = juce::Rectangle<int> (
            b.getX() + col * (tileW + kGridGap),
            b.getY() + row * (tileH + kGridGap),
            tileW, tileH);
        if (mTiles[s]) mTiles[s]->setBounds (rect);
    }
}

void BaySickPedalsEditor::onSlotTypeChanged (int slot)
{
    if (slot < 0 || slot >= BaySickPedalsProcessor::kNumSlots) return;
    if (mTiles[slot]) mTiles[slot]->rebuild();
    mLastTypes[slot] = mProc.getSlotType (slot);
}

int BaySickPedalsEditor::findSlotIndexAt (juce::Point<int> editorPos) const noexcept
{
    for (int s = 0; s < BaySickPedalsProcessor::kNumSlots; ++s)
    {
        if (mTiles[s] && mTiles[s]->getBounds().contains (editorPos))
            return s;
    }
    return -1;
}

void BaySickPedalsEditor::setDropTargetSlot (int slot)
{
    if (slot == mDropTargetSlot) return;

    // Clear old.
    if (mDropTargetSlot >= 0 && mDropTargetSlot < BaySickPedalsProcessor::kNumSlots)
        if (mTiles[mDropTargetSlot]) mTiles[mDropTargetSlot]->setDropTarget (false);

    mDropTargetSlot = slot;

    // Set new (only valid for movable slots 1-6).
    if (mDropTargetSlot >= 0 && mDropTargetSlot < BaySickPedalsProcessor::kNumSlots
        && mDropTargetSlot != BaySickPedalsProcessor::kSlotTuner
        && mDropTargetSlot != BaySickPedalsProcessor::kSlotEQ)
    {
        if (mTiles[mDropTargetSlot]) mTiles[mDropTargetSlot]->setDropTarget (true);
    }
}

void BaySickPedalsEditor::performMove (int from, int to)
{
    if (! mProc.moveSlot (from, to)) return;

    // Both involved tiles need a rebuild (the underlying types swapped).
    onSlotTypeChanged (from);
    onSlotTypeChanged (to);
}

void BaySickPedalsEditor::timerCallback()
{
    // External mutations (state-load, undo, pedalboard preset load) can change
    // slot types behind our back.  Detect and rebuild.
    for (int s = 0; s < BaySickPedalsProcessor::kNumSlots; ++s)
    {
        const auto t = mProc.getSlotType (s);
        if (t != mLastTypes[s])
        {
            mLastTypes[s] = t;
            if (mTiles[s]) mTiles[s]->rebuild();
        }
    }
}
