#include "BaySickAlignEditor.h"
#include "BaySickVocalProcessor.h"
#include "../Standalone/StandaloneEditor.h"
#include "../Standalone/BaySickTitleBar.h"   // QA-A (2026-05-09)

// ─────────────────────────────────────────────────────────────────────────────
// BaySickAlignEditor - H-6c (2026-05-02)
//
// VocAlign-clone visual + interaction model.  See header for the full layout
// summary.  H-6c ships the empty-state shell: every UI element paints
// correctly, every interaction handler is stubbed, all data is empty.
// G-9 wires the recording loaders + sidechain picker enumeration.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // ── Layout constants ────────────────────────────────────────────────────
    constexpr int kToolbarH        = 36;
    constexpr int kHeaderW         = 96;     // lane header column
    constexpr int kSyncStripH      = 16;
    constexpr int kProtectedStripH = 16;
    constexpr int kViewModeH       = 22;
    constexpr int kHistoryH        = 60;
    constexpr int kTabBarW         = 28;
    constexpr int kSidePanelW      = 168;

    // ── VocAlign-matched palette ────────────────────────────────────────────
    const juce::Colour kBg          = juce::Colour (0xff0e0f12);
    const juce::Colour kPanelBg     = juce::Colour (0xff16191e);
    const juce::Colour kStripBg     = juce::Colour (0xff1a1d22);
    const juce::Colour kHeaderBg    = juce::Colour (0xff14171c);
    const juce::Colour kToolbarBg   = juce::Colour (0xff0d0f12);
    const juce::Colour kText        = juce::Colour (0xffd0d6dc);
    const juce::Colour kTextDim     = juce::Colour (0xff8a929c);
    const juce::Colour kTextSection = juce::Colour (0xff6a727c);
    const juce::Colour kBlueAccent  = juce::Colour (0xff5680ff);
    const juce::Colour kCheckGreen  = juce::Colour (0xff58c067);
    const juce::Colour kRedActive   = juce::Colour (0xfff0503c);
    const juce::Colour kProtectRed  = juce::Colour (0x66f04030);

    const juce::Colour kGuideYellow  = juce::Colour (0xffe8c147);
    const juce::Colour kDubOrange    = juce::Colour (0xffe0822a);
    const juce::Colour kOutputPurple = juce::Colour (0xff7950d0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Toolbar - VocAlign Pro logo + preset selector + modified indicator +
// undo / redo + settings + help
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::Toolbar : public juce::Component
{
public:
    Toolbar()
    {
        // QA-A (2026-05-09): mTitleLbl swapped to BaySickEngineLabel for
        // matching BaySickVocals title styling (16pt bold, teal #0FAFA5,
        // bloom halo).  Engine name + accent are set via the member
        // initializer below; no setText/setFont/setColour needed here.
        addAndMakeVisible (mTitleLbl);

        addAndMakeVisible (mPresetCombo);
        mPresetCombo.setTextWhenNothingSelected ("(no preset)");
        mPresetCombo.addItem ("Vocal -- Harmony -- Tight Timing",  1);
        mPresetCombo.addItem ("Vocal -- Harmony -- Loose Timing",  2);
        mPresetCombo.addItem ("Vocal -- Double -- Tight Timing",   3);
        mPresetCombo.addItem ("Vocal -- Double -- Loose Timing",   4);
        mPresetCombo.addItem ("Vocal -- Lead Match",                5);
        mPresetCombo.addItem ("Dialogue -- ADR Sync",               6);
        mPresetCombo.addItem ("Instrument -- Double",               7);
        mPresetCombo.setSelectedId (1, juce::dontSendNotification);
        mPresetCombo.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
        mPresetCombo.setColour (juce::ComboBox::textColourId,        kText);

        auto plain = [this](juce::TextButton& b, const juce::String& txt,
                             const juce::String& tt)
        {
            b.setButtonText (txt);
            b.setTooltip (tt);
            b.setColour (juce::TextButton::buttonColourId, kPanelBg);
            b.setColour (juce::TextButton::textColourOnId,  kText);
            b.setColour (juce::TextButton::textColourOffId, kText);
            addAndMakeVisible (b);
        };

        plain (mUndoBtn, "Undo", "Undo (Ctrl+Z)");
        mUndoBtn.onClick = [this]
        {
            if (auto* se = findParentComponentOfClass<StandaloneEditor>())
                se->globalUndo();
        };

        plain (mRedoBtn, "Redo", "Redo (Ctrl+Alt+Z)");
        mRedoBtn.onClick = [this]
        {
            if (auto* se = findParentComponentOfClass<StandaloneEditor>())
                se->globalRedo();
        };

        plain (mAutoPreviewBtn, "Auto", "Auto-Preview Output (off = manual Render only)");
        mAutoPreviewBtn.setClickingTogglesState (true);
        mAutoPreviewBtn.setToggleState (true, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kToolbarBg);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        // Modified-preset indicator (asterisk-style green dot)
        const int x = mPresetCombo.getRight() + 4;
        const int y = getHeight() / 2;
        g.setColour (kCheckGreen);
        g.fillEllipse ((float) x, (float) y - 4.0f, 8.0f, 8.0f);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6, 4);
        mTitleLbl.setBounds (b.removeFromLeft (110));
        b.removeFromLeft (8);
        mPresetCombo.setBounds (b.removeFromLeft (260).reduced (0, 2));
        b.removeFromLeft (16);   // leave room for the modified-indicator dot

        auto right = b;
        const int btnW = 64;
        const int gap  = 4;
        mAutoPreviewBtn.setBounds (right.removeFromRight (btnW));
        right.removeFromRight (gap);
        mRedoBtn       .setBounds (right.removeFromRight (btnW));
        right.removeFromRight (gap);
        mUndoBtn       .setBounds (right.removeFromRight (btnW));
    }

private:
    BaySickEngineLabel mTitleLbl { "BaySickAlign", juce::Colour (0xFF0FAFA5) };
    juce::ComboBox   mPresetCombo;
    juce::TextButton mUndoBtn, mRedoBtn, mAutoPreviewBtn;
};

// ─────────────────────────────────────────────────────────────────────────────
// LaneHeader - left-side label + Capture/Render button + per-lane controls.
// Used by all 3 lanes; role determines colour + button text + extra controls.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::LaneHeader : public juce::Component
{
public:
    enum class Role { Guide, Dub, Output };

    explicit LaneHeader (Role r) : mRole (r)
    {
        addAndMakeVisible (mLabel);
        mLabel.setColour (juce::Label::textColourId, accent());
        mLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        mLabel.setJustificationType (juce::Justification::centredLeft);
        mLabel.setText (roleName(), juce::dontSendNotification);

        addAndMakeVisible (mActionBtn);
        mActionBtn.setButtonText (mRole == Role::Output ? "Render" : "Capture");
        mActionBtn.setColour (juce::TextButton::buttonColourId, accent());
        mActionBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        mActionBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        mActionBtn.setEnabled (false);   // H-6c: empty state, action stubbed

        if (mRole == Role::Guide)
        {
            // Sidechain-source picker -- populated at G-9 from the active
            // sidechain inputs feeding this Vox channel.  Empty until then.
            addAndMakeVisible (mGuidePicker);
            mGuidePicker.setTextWhenNothingSelected ("(no sidechain)");
            mGuidePicker.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
            mGuidePicker.setColour (juce::ComboBox::textColourId,        kText);
        }

        if (mRole == Role::Dub)
        {
            // Process-group selector (Q=2: link multiple Vox channels'
            // BaySickAlign settings).  Empty until G-9.
            addAndMakeVisible (mGroupCombo);
            mGroupCombo.addItem ("NO GROUP", 1);
            mGroupCombo.setSelectedId (1, juce::dontSendNotification);
            mGroupCombo.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
            mGroupCombo.setColour (juce::ComboBox::textColourId,        kText);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kHeaderBg);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8, 6);
        mLabel.setBounds (b.removeFromTop (16));
        b.removeFromTop (4);

        if (mRole == Role::Dub)
        {
            mGroupCombo.setBounds (b.removeFromTop (22));
            b.removeFromTop (4);
        }

        if (mRole == Role::Guide)
        {
            mGuidePicker.setBounds (b.removeFromTop (22));
            b.removeFromTop (4);
        }

        mActionBtn.setBounds (b.removeFromTop (24));
    }

    void setActionEnabled (bool e) { mActionBtn.setEnabled (e); }

    juce::Colour accent() const noexcept
    {
        switch (mRole)
        {
            case Role::Guide:  return kGuideYellow;
            case Role::Dub:    return kDubOrange;
            case Role::Output: return kOutputPurple;
        }
        return kText;
    }

    juce::String roleName() const noexcept
    {
        switch (mRole)
        {
            case Role::Guide:  return "GUIDE";
            case Role::Dub:    return "DUB";
            case Role::Output: return "OUTPUT";
        }
        return {};
    }

private:
    Role             mRole;
    juce::Label      mLabel;
    juce::TextButton mActionBtn;
    juce::ComboBox   mGuidePicker;
    juce::ComboBox   mGroupCombo;
};

// ─────────────────────────────────────────────────────────────────────────────
// WaveformLane - main waveform display per Guide / Dub / Output role.
// Renders three view modes (Waveform / Pitch / Energy).  Empty state =
// just the dark canvas.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::WaveformLane : public juce::Component
{
public:
    using Role = LaneHeader::Role;

    explicit WaveformLane (BaySickAlignEditor& owner, Role r)
        : mOwner (owner), mRole (r) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBg);

        // Time ruler at the top of the Guide lane only -- single shared ruler
        // for all 3 lanes lives at the top of the Guide lane (matches
        // VocAlign's layout).
        if (mRole == Role::Guide)
        {
            auto ruler = getLocalBounds().removeFromTop (14);
            g.setColour (kStripBg);
            g.fillRect (ruler);
            g.setColour (kTextDim);
            g.setFont (juce::Font (10.0f));

            const double pps  = mOwner.pixelsPerSecond();
            const double secL = mOwner.scrollSeconds();
            const double secR = secL + getWidth() / juce::jmax (1.0, pps);
            for (int sec = (int) std::floor (secL); (double) sec <= secR + 1.0; ++sec)
            {
                const int x = (int) ((sec - secL) * pps);
                if (x < 0 || x >= getWidth()) continue;
                g.drawVerticalLine (x, 0.0f, (float) ruler.getHeight());
                g.drawText (juce::String (sec) + "s",
                            x + 2, 0, 36, ruler.getHeight(),
                            juce::Justification::centredLeft);
            }
        }

        // Subtle horizontal centerline (zero-amplitude reference)
        g.setColour (kTextDim.withAlpha (0.20f));
        g.drawHorizontalLine (getHeight() / 2, 0.0f, (float) getWidth());

        // Bottom: greyscale heat-map strip on the Output lane only
        if (mRole == Role::Output)
        {
            auto heat = getLocalBounds().removeFromBottom (8);
            g.setColour (kStripBg);
            g.fillRect (heat);
            // G-9: paint per-time-slot processing intensity here once
            // analyzeOffline produces a heat map.
        }

        // G-9: render the actual waveform in role-coloured fill once
        // audio is loaded.  Empty state = just the centerline.
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu())
            return;

        // Right-click context menu (matches VocAlign)
        juce::PopupMenu menu;
        if (mRole == Role::Dub)
        {
            juce::PopupMenu addProtected;
            addProtected.addItem ("Protect Pitch and Timing", []{ /* G-9 */ });
            addProtected.addItem ("Protect Timing",            []{ /* G-9 */ });
            addProtected.addItem ("Protect Pitch",             []{ /* G-9 */ });
            menu.addSubMenu ("Add Protected Area", addProtected);
        }
        menu.addItem ("Automatic Sync Points", []{ /* G-9 */ });
        menu.addItem ("Add Sync Point",        []{ /* G-9 */ });

        juce::PopupMenu selectAudio;
        selectAudio.addItem ("(no audio captured yet)", false, false, []{});
        menu.addSubMenu ("Select Audio", selectAudio);

        menu.showMenuAsync (juce::PopupMenu::Options{});
    }

private:
    BaySickAlignEditor& mOwner;
    Role                mRole;
};

// ─────────────────────────────────────────────────────────────────────────────
// SyncPointsStrip - between Guide + Dub.  Hosts user-placed and
// algorithm-placed sync-point markers.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::SyncPointsStrip : public juce::Component
{
public:
    explicit SyncPointsStrip (BaySickAlignEditor& owner) : mOwner (owner) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kStripBg);
        g.setColour (kTextSection);
        g.setFont (juce::Font (9.0f, juce::Font::bold));
        g.drawText ("SYNC POINTS", getLocalBounds().reduced (8, 0),
                    juce::Justification::centredLeft);
        // G-9: paint sync-point vertical lines + linking arcs to the Dub.
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        // G-9: drag-create or drag-existing sync points here.
    }

private:
    BaySickAlignEditor& mOwner;
};

// ─────────────────────────────────────────────────────────────────────────────
// ProtectedAreasStrip - between Dub + Output.  Drag-to-create areas that
// exempt parts of the Dub from time / pitch / both alignment.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::ProtectedAreasStrip : public juce::Component
{
public:
    explicit ProtectedAreasStrip (BaySickAlignEditor& owner) : mOwner (owner) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kStripBg);
        g.setColour (kTextSection);
        g.setFont (juce::Font (9.0f, juce::Font::bold));
        g.drawText ("PROTECTED AREAS", getLocalBounds().reduced (8, 0),
                    juce::Justification::centredLeft);
        // G-9: paint red-overlay protected areas + shield handles + the
        // pitch-protect / time-protect indicator icons.
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        // G-9: drag-create protected areas; right-click on existing area
        // pops up the Modify (Pitch+Timing / Timing / Pitch) submenu.
    }

private:
    BaySickAlignEditor& mOwner;
};

// ─────────────────────────────────────────────────────────────────────────────
// ViewModeBar - 3 buttons (Waveform / Pitch / Energy) at the bottom of the
// lane area, just above the history scrubber.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::ViewModeBar : public juce::Component
{
public:
    explicit ViewModeBar (BaySickAlignEditor& owner) : mOwner (owner)
    {
        auto setup = [this](juce::TextButton& b, const juce::String& t,
                              const juce::String& tt, BaySickAlignEditor::ViewMode m)
        {
            b.setButtonText (t);
            b.setTooltip (tt);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (1);
            b.setColour (juce::TextButton::buttonColourId, kPanelBg);
            b.setColour (juce::TextButton::textColourOnId,  kText);
            b.setColour (juce::TextButton::textColourOffId, kTextDim);
            b.onClick = [this, m] { mOwner.setViewMode (m); };
            addAndMakeVisible (b);
        };
        setup (mWaveBtn,   "Wave",   "Waveform display",      BaySickAlignEditor::ViewMode::Waveform);
        setup (mPitchBtn,  "Pitch",  "Pitch profile display", BaySickAlignEditor::ViewMode::PitchProfile);
        setup (mEnergyBtn, "Energy", "Energy profile display",BaySickAlignEditor::ViewMode::EnergyProfile);
        mWaveBtn.setToggleState (true, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kPanelBg);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 2);
        const int w = 50;
        mWaveBtn  .setBounds (b.removeFromLeft (w)); b.removeFromLeft (3);
        mPitchBtn .setBounds (b.removeFromLeft (w)); b.removeFromLeft (3);
        mEnergyBtn.setBounds (b.removeFromLeft (w));
    }

    void setViewModeUI (BaySickAlignEditor::ViewMode m)
    {
        mWaveBtn  .setToggleState (m == BaySickAlignEditor::ViewMode::Waveform,      juce::dontSendNotification);
        mPitchBtn .setToggleState (m == BaySickAlignEditor::ViewMode::PitchProfile,  juce::dontSendNotification);
        mEnergyBtn.setToggleState (m == BaySickAlignEditor::ViewMode::EnergyProfile, juce::dontSendNotification);
    }

private:
    BaySickAlignEditor& mOwner;
    juce::TextButton mWaveBtn, mPitchBtn, mEnergyBtn;
};

// ─────────────────────────────────────────────────────────────────────────────
// HistoryScrubber - bottom navigator.  K=2 (locked): repurposed from
// VocAlign's multi-region overview into a render-history scrubber.  Each
// prior render is a mini lane the user can switch back to for A/B compare.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::HistoryScrubber : public juce::Component
{
public:
    explicit HistoryScrubber (BaySickAlignEditor& owner) : mOwner (owner)
    {
        addAndMakeVisible (mTrashBtn);
        mTrashBtn.setButtonText ("Del");
        mTrashBtn.setTooltip ("Delete the selected render from history");
        mTrashBtn.setColour (juce::TextButton::buttonColourId, kPanelBg);
        mTrashBtn.setEnabled (false);

        addAndMakeVisible (mZoomInBtn);
        mZoomInBtn.setButtonText ("+");
        mZoomInBtn.setTooltip ("Zoom in horizontally");

        addAndMakeVisible (mZoomOutBtn);
        mZoomOutBtn.setButtonText ("-");
        mZoomOutBtn.setTooltip ("Zoom out horizontally");
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kStripBg);
        g.setColour (kTextSection);
        g.setFont (juce::Font (9.0f, juce::Font::bold));
        g.drawText ("RENDER HISTORY",
                    getLocalBounds().reduced (8, 0).withHeight (12),
                    juce::Justification::centredLeft);
        // G-9: paint each render's mini Guide+Dub waveform overview here.
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4);
        mTrashBtn.setBounds (b.removeFromLeft (40).reduced (0, 12));
        b.removeFromRight (4);
        mZoomInBtn .setBounds (b.removeFromRight (24).reduced (0, 12));
        mZoomOutBtn.setBounds (b.removeFromRight (24).reduced (0, 12));
    }

private:
    BaySickAlignEditor& mOwner;
    juce::TextButton mTrashBtn, mZoomInBtn, mZoomOutBtn;
};

// ─────────────────────────────────────────────────────────────────────────────
// MatchTimingPanel - right-side collapsible panel
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::MatchTimingPanel : public juce::Component
{
public:
    MatchTimingPanel()
    {
        addAndMakeVisible (mTitle);
        mTitle.setText ("MATCH TIMING", juce::dontSendNotification);
        mTitle.setColour (juce::Label::textColourId, kText);
        mTitle.setFont (juce::Font (11.0f, juce::Font::bold));

        addAndMakeVisible (mMaster);
        mMaster.setClickingTogglesState (true);
        mMaster.setToggleState (true, juce::dontSendNotification);
        mMaster.setButtonText ("ON");
        mMaster.setColour (juce::TextButton::buttonOnColourId,  kBlueAccent);
        mMaster.setColour (juce::TextButton::buttonColourId,    kPanelBg);

        addAndMakeVisible (mMaxDiffLbl);
        mMaxDiffLbl.setText ("MAX DIFFERENCE", juce::dontSendNotification);
        mMaxDiffLbl.setColour (juce::Label::textColourId, kTextSection);
        mMaxDiffLbl.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mMaxDiff);
        mMaxDiff.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        mMaxDiff.setRange (0.0, 100.0, 0.5);
        mMaxDiff.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        mMaxDiff.setValue (0.0, juce::dontSendNotification);
        mMaxDiff.setTextValueSuffix (" MS");
        mMaxDiff.setTooltip ("Match Timing tolerance -- LOOSE allows looser timing, TIGHT snaps Dub to Guide");

        addAndMakeVisible (mAlignRuleLbl);
        mAlignRuleLbl.setText ("ALIGNMENT RULE", juce::dontSendNotification);
        mAlignRuleLbl.setColour (juce::Label::textColourId, kTextSection);
        mAlignRuleLbl.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mAlignRule);
        mAlignRule.addItem ("Strict",             1);
        mAlignRule.addItem ("Normal Flexibility", 2);
        mAlignRule.addItem ("Loose Flexibility",  3);
        mAlignRule.setSelectedId (2, juce::dontSendNotification);

        addAndMakeVisible (mSmartAlign);
        mSmartAlign.setButtonText ("Smart Align");
        mSmartAlign.setColour (juce::ToggleButton::tickColourId, kBlueAccent);

        addAndMakeVisible (mMaxShiftLbl);
        mMaxShiftLbl.setText ("MAXIMUM SHIFT", juce::dontSendNotification);
        mMaxShiftLbl.setColour (juce::Label::textColourId, kTextSection);
        mMaxShiftLbl.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mMaxShift);
        mMaxShift.addItem ("No Limit",  1);
        mMaxShift.addItem ("Small",     2);
        mMaxShift.addItem ("Medium",    3);
        mMaxShift.addItem ("Large",     4);
        mMaxShift.setSelectedId (1, juce::dontSendNotification);

        addAndMakeVisible (mHighRes);
        mHighRes.setButtonText ("High Resolution");
        mHighRes.setColour (juce::ToggleButton::tickColourId, kBlueAccent);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kPanelBg);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawVerticalLine (0, 0.0f, (float) getHeight());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10, 8);
        mTitle .setBounds (b.removeFromTop (16));
        mMaster.setBounds (juce::Rectangle<int> (getWidth() - 38, 8, 28, 16));

        b.removeFromTop (8);
        mMaxDiffLbl.setBounds (b.removeFromTop (12));
        mMaxDiff   .setBounds (b.removeFromTop (78).reduced (8, 0));

        b.removeFromTop (10);
        mAlignRuleLbl.setBounds (b.removeFromTop (12));
        mAlignRule   .setBounds (b.removeFromTop (22));

        b.removeFromTop (6);
        mSmartAlign.setBounds (b.removeFromTop (22));

        b.removeFromTop (10);
        mMaxShiftLbl.setBounds (b.removeFromTop (12));
        mMaxShift   .setBounds (b.removeFromTop (22));

        b.removeFromTop (6);
        mHighRes.setBounds (b.removeFromTop (22));
    }

private:
    juce::Label        mTitle, mMaxDiffLbl, mAlignRuleLbl, mMaxShiftLbl;
    juce::TextButton   mMaster;
    juce::Slider       mMaxDiff;
    juce::ComboBox     mAlignRule, mMaxShift;
    juce::ToggleButton mSmartAlign, mHighRes;
};

// ─────────────────────────────────────────────────────────────────────────────
// MatchPitchPanel - right-side collapsible panel
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::MatchPitchPanel : public juce::Component
{
public:
    MatchPitchPanel()
    {
        addAndMakeVisible (mTitle);
        mTitle.setText ("MATCH PITCH", juce::dontSendNotification);
        mTitle.setColour (juce::Label::textColourId, kText);
        mTitle.setFont (juce::Font (11.0f, juce::Font::bold));

        addAndMakeVisible (mMaster);
        mMaster.setClickingTogglesState (true);
        mMaster.setToggleState (true, juce::dontSendNotification);
        mMaster.setButtonText ("ON");
        mMaster.setColour (juce::TextButton::buttonOnColourId, kBlueAccent);
        mMaster.setColour (juce::TextButton::buttonColourId,   kPanelBg);

        addAndMakeVisible (mMaxDiffLbl);
        mMaxDiffLbl.setText ("MAX DIFFERENCE", juce::dontSendNotification);
        mMaxDiffLbl.setColour (juce::Label::textColourId, kTextSection);
        mMaxDiffLbl.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mMaxDiff);
        mMaxDiff.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        mMaxDiff.setRange (0.0, 100.0, 0.1);
        mMaxDiff.setValue (2.0, juce::dontSendNotification);
        mMaxDiff.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        mMaxDiff.setTextValueSuffix (" %");
        mMaxDiff.setTooltip ("Match Pitch tolerance");

        addAndMakeVisible (mTargetModeLbl);
        mTargetModeLbl.setText ("TARGET MODE", juce::dontSendNotification);
        mTargetModeLbl.setColour (juce::Label::textColourId, kTextSection);
        mTargetModeLbl.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mTargetMode);
        mTargetMode.addItem ("Absolute",   1);
        mTargetMode.addItem ("Relative",   2);
        mTargetMode.setSelectedId (1, juce::dontSendNotification);

        addAndMakeVisible (mPitchTargetLbl);
        mPitchTargetLbl.setText ("PITCH TARGET", juce::dontSendNotification);
        mPitchTargetLbl.setColour (juce::Label::textColourId, kTextSection);
        mPitchTargetLbl.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mPitchTarget);
        mPitchTarget.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        mPitchTarget.setRange (0.0, 100.0, 0.1);
        mPitchTarget.setValue (100.0, juce::dontSendNotification);
        mPitchTarget.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        mPitchTarget.setTextValueSuffix (" %");
        mPitchTarget.setTooltip ("Dub-to-Guide pitch interpolation");

        addAndMakeVisible (mSmartPitchLbl);
        mSmartPitchLbl.setText ("SMART PITCH", juce::dontSendNotification);
        mSmartPitchLbl.setColour (juce::Label::textColourId, kTextSection);
        mSmartPitchLbl.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mSmartPitch);
        mSmartPitch.addItem ("Match All To Guide",          1);
        mSmartPitch.addItem ("Match Unison Only",            2);
        mSmartPitch.addItem ("Match Unison & Tune Non-Unison",3);
        mSmartPitch.setSelectedId (1, juce::dontSendNotification);

        addAndMakeVisible (mAlgoLbl);
        mAlgoLbl.setText ("ALGORITHM", juce::dontSendNotification);
        mAlgoLbl.setColour (juce::Label::textColourId, kTextSection);
        mAlgoLbl.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mAlgo);
        mAlgo.addItem ("Mode 1", 1);
        mAlgo.addItem ("Mode 2", 2);
        mAlgo.addItem ("Mode 3", 3);
        mAlgo.setSelectedId (1, juce::dontSendNotification);

        addAndMakeVisible (mTransposeLbl);
        mTransposeLbl.setText ("TRANSPOSE", juce::dontSendNotification);
        mTransposeLbl.setColour (juce::Label::textColourId, kTextSection);
        mTransposeLbl.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mTranspose);
        mTranspose.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        mTranspose.setRange (-12.0, 12.0, 0.1);
        mTranspose.setValue (0.0, juce::dontSendNotification);
        mTranspose.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        mTranspose.setTextValueSuffix (" st");
        mTranspose.setTooltip ("Semitone transposition applied to the Output");
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kPanelBg);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawVerticalLine (0, 0.0f, (float) getHeight());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10, 8);
        mTitle .setBounds (b.removeFromTop (16));
        mMaster.setBounds (juce::Rectangle<int> (getWidth() - 38, 8, 28, 16));

        auto knobRow = [&] (juce::Label& lbl, juce::Slider& s)
        {
            b.removeFromTop (8);
            lbl.setBounds (b.removeFromTop (12));
            s  .setBounds (b.removeFromTop (78).reduced (8, 0));
        };
        auto comboRow = [&] (juce::Label& lbl, juce::ComboBox& c)
        {
            b.removeFromTop (8);
            lbl.setBounds (b.removeFromTop (12));
            c  .setBounds (b.removeFromTop (22));
        };

        knobRow  (mMaxDiffLbl,     mMaxDiff);
        comboRow (mTargetModeLbl,  mTargetMode);
        knobRow  (mPitchTargetLbl, mPitchTarget);
        comboRow (mSmartPitchLbl,  mSmartPitch);
        comboRow (mAlgoLbl,        mAlgo);
        knobRow  (mTransposeLbl,   mTranspose);
    }

private:
    juce::Label      mTitle, mMaxDiffLbl, mTargetModeLbl, mPitchTargetLbl,
                     mSmartPitchLbl, mAlgoLbl, mTransposeLbl;
    juce::TextButton mMaster;
    juce::Slider     mMaxDiff, mPitchTarget, mTranspose;
    juce::ComboBox   mTargetMode, mSmartPitch, mAlgo;
};

// ─────────────────────────────────────────────────────────────────────────────
// OtherPanel - right-side collapsible panel (Pitch Ranges + Formant Shift)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::OtherPanel : public juce::Component
{
public:
    OtherPanel()
    {
        addAndMakeVisible (mTitle);
        mTitle.setText ("OTHER", juce::dontSendNotification);
        mTitle.setColour (juce::Label::textColourId, kText);
        mTitle.setFont (juce::Font (11.0f, juce::Font::bold));

        addAndMakeVisible (mPitchRangesLbl);
        mPitchRangesLbl.setText ("PITCH RANGES", juce::dontSendNotification);
        mPitchRangesLbl.setColour (juce::Label::textColourId, kTextSection);
        mPitchRangesLbl.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mGuideLbl);
        mGuideLbl.setText ("GUIDE", juce::dontSendNotification);
        mGuideLbl.setColour (juce::Label::textColourId, kTextDim);
        mGuideLbl.setFont (juce::Font (9.0f));

        addAndMakeVisible (mGuideRange);
        for (auto* nm : { "Normal", "Low Pitched Vocal", "High Pitched Vocal",
                           "Bass Instrument", "Treble Instrument" })
            mGuideRange.addItem (nm, mGuideRange.getNumItems() + 1);
        mGuideRange.setSelectedId (1, juce::dontSendNotification);

        addAndMakeVisible (mDubLbl);
        mDubLbl.setText ("DUB", juce::dontSendNotification);
        mDubLbl.setColour (juce::Label::textColourId, kTextDim);
        mDubLbl.setFont (juce::Font (9.0f));

        addAndMakeVisible (mDubRange);
        for (auto* nm : { "Normal", "Low Pitched Vocal", "High Pitched Vocal",
                           "Bass Instrument", "Treble Instrument" })
            mDubRange.addItem (nm, mDubRange.getNumItems() + 1);
        mDubRange.setSelectedId (1, juce::dontSendNotification);

        addAndMakeVisible (mFormantSection);
        mFormantSection.setText ("FORMANT SHIFT", juce::dontSendNotification);
        mFormantSection.setColour (juce::Label::textColourId, kTextSection);
        mFormantSection.setFont (juce::Font (9.0f, juce::Font::bold));

        addAndMakeVisible (mFormantMaster);
        mFormantMaster.setClickingTogglesState (true);
        mFormantMaster.setButtonText ("ON");
        mFormantMaster.setColour (juce::TextButton::buttonOnColourId, kBlueAccent);
        mFormantMaster.setColour (juce::TextButton::buttonColourId,   kPanelBg);

        addAndMakeVisible (mFormant);
        mFormant.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        mFormant.setRange (-100.0, 100.0, 0.1);
        mFormant.setValue (0.0, juce::dontSendNotification);
        mFormant.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        mFormant.setTextValueSuffix (" %");
        mFormant.setTooltip ("Formant shift -- LOWER makes Dub voice deeper, HIGHER makes it brighter");
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kPanelBg);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawVerticalLine (0, 0.0f, (float) getHeight());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10, 8);
        mTitle.setBounds (b.removeFromTop (16));
        b.removeFromTop (10);

        mPitchRangesLbl.setBounds (b.removeFromTop (12));
        b.removeFromTop (4);
        mGuideLbl  .setBounds (b.removeFromTop (12));
        mGuideRange.setBounds (b.removeFromTop (22));
        b.removeFromTop (4);
        mDubLbl    .setBounds (b.removeFromTop (12));
        mDubRange  .setBounds (b.removeFromTop (22));

        b.removeFromTop (12);
        mFormantSection.setBounds (b.removeFromTop (12));
        mFormantMaster .setBounds (juce::Rectangle<int> (getWidth() - 38,
                                                          mFormantSection.getY() - 2,
                                                          28, 16));
        mFormant.setBounds (b.removeFromTop (78).reduced (8, 0));
    }

private:
    juce::Label      mTitle, mPitchRangesLbl, mGuideLbl, mDubLbl, mFormantSection;
    juce::ComboBox   mGuideRange, mDubRange;
    juce::TextButton mFormantMaster;
    juce::Slider     mFormant;
};

// ─────────────────────────────────────────────────────────────────────────────
// SidePanelTabs - 3 vertical icon buttons on the far right edge
// ─────────────────────────────────────────────────────────────────────────────
class BaySickAlignEditor::SidePanelTabs : public juce::Component
{
public:
    explicit SidePanelTabs (BaySickAlignEditor& owner) : mOwner (owner)
    {
        auto setup = [this](juce::TextButton& b, const juce::String& glyph,
                              const juce::String& tt, BaySickAlignEditor::SidePanel p)
        {
            b.setButtonText (glyph);
            b.setTooltip (tt);
            b.setClickingTogglesState (true);
            b.setColour (juce::TextButton::buttonColourId,   kPanelBg);
            b.setColour (juce::TextButton::buttonOnColourId, kBlueAccent.withAlpha (0.55f));
            b.setColour (juce::TextButton::textColourOnId,   kText);
            b.setColour (juce::TextButton::textColourOffId,  kTextDim);
            b.onClick = [this, p, &b]
            {
                mOwner.toggleSidePanel (p);
                b.setToggleState (mOwner.isSidePanelOpen (p), juce::dontSendNotification);
            };
            addAndMakeVisible (b);
        };
        // ASCII glyph proxies (no icon resources yet).  T = Timing,
        // P = Pitch, O = Other.
        setup (mTimingTab, "T", "Match Timing panel",
                BaySickAlignEditor::SidePanel::Timing);
        setup (mPitchTab,  "P", "Match Pitch panel",
                BaySickAlignEditor::SidePanel::Pitch);
        setup (mOtherTab,  "O", "Other panel (Pitch Ranges + Formant Shift)",
                BaySickAlignEditor::SidePanel::Other);

        mTimingTab.setToggleState (true, juce::dontSendNotification);
        mPitchTab .setToggleState (true, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kPanelBg);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawVerticalLine (0, 0.0f, (float) getHeight());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (2, 8);
        const int btnH = 26;
        mTimingTab.setBounds (b.removeFromTop (btnH)); b.removeFromTop (4);
        mPitchTab .setBounds (b.removeFromTop (btnH)); b.removeFromTop (4);
        mOtherTab .setBounds (b.removeFromTop (btnH));
    }

private:
    BaySickAlignEditor& mOwner;
    juce::TextButton mTimingTab, mPitchTab, mOtherTab;
};

// ─────────────────────────────────────────────────────────────────────────────
// BaySickAlignEditor
// ─────────────────────────────────────────────────────────────────────────────
BaySickAlignEditor::BaySickAlignEditor (BaySickVocalProcessor& p) : mProc (p)
{
    using LH = LaneHeader;

    mToolbar         = std::make_unique<Toolbar>             ();
    mGuideHeader     = std::make_unique<LH>                  (LH::Role::Guide);
    mGuideLane       = std::make_unique<WaveformLane>        (*this, LH::Role::Guide);
    mSyncStrip       = std::make_unique<SyncPointsStrip>     (*this);
    mDubHeader       = std::make_unique<LH>                  (LH::Role::Dub);
    mDubLane         = std::make_unique<WaveformLane>        (*this, LH::Role::Dub);
    mProtectedStrip  = std::make_unique<ProtectedAreasStrip> (*this);
    mOutputHeader    = std::make_unique<LH>                  (LH::Role::Output);
    mOutputLane      = std::make_unique<WaveformLane>        (*this, LH::Role::Output);
    mViewModeBar     = std::make_unique<ViewModeBar>         (*this);
    mHistoryScrubber = std::make_unique<HistoryScrubber>     (*this);
    mTimingPanel     = std::make_unique<MatchTimingPanel>    ();
    mPitchPanel      = std::make_unique<MatchPitchPanel>     ();
    mOtherPanel      = std::make_unique<OtherPanel>          ();
    mSidePanelTabs   = std::make_unique<SidePanelTabs>       (*this);

    addAndMakeVisible (*mToolbar);
    addAndMakeVisible (*mGuideHeader);
    addAndMakeVisible (*mGuideLane);
    addAndMakeVisible (*mSyncStrip);
    addAndMakeVisible (*mDubHeader);
    addAndMakeVisible (*mDubLane);
    addAndMakeVisible (*mProtectedStrip);
    addAndMakeVisible (*mOutputHeader);
    addAndMakeVisible (*mOutputLane);
    addAndMakeVisible (*mViewModeBar);
    addAndMakeVisible (*mHistoryScrubber);
    addAndMakeVisible (*mTimingPanel);
    addAndMakeVisible (*mPitchPanel);
    addAndMakeVisible (*mOtherPanel);
    addAndMakeVisible (*mSidePanelTabs);
}

BaySickAlignEditor::~BaySickAlignEditor() = default;

void BaySickAlignEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
}

bool BaySickAlignEditor::isSidePanelOpen (SidePanel p) const noexcept
{
    return mSidePanelOpen[(int) p];
}

void BaySickAlignEditor::toggleSidePanel (SidePanel p)
{
    mSidePanelOpen[(int) p] = ! mSidePanelOpen[(int) p];
    resized();
}

void BaySickAlignEditor::setViewMode (ViewMode m)
{
    if (m == mViewMode) return;
    mViewMode = m;
    if (mViewModeBar) mViewModeBar->setViewModeUI (m);
    if (mGuideLane)   mGuideLane ->repaint();
    if (mDubLane)     mDubLane   ->repaint();
    if (mOutputLane)  mOutputLane->repaint();
}

void BaySickAlignEditor::resized()
{
    auto bounds = getLocalBounds();

    // Toolbar at top
    mToolbar->setBounds (bounds.removeFromTop (kToolbarH));

    // Side-panel tab column on the far right (always visible)
    auto tabCol = bounds.removeFromRight (kTabBarW);
    mSidePanelTabs->setBounds (tabCol);

    // Open side panels grow leftward from the tab column.  Order from
    // right-to-left: Other, Pitch, Timing.  Each panel is placed only when
    // its open flag is set; otherwise hidden.
    auto place = [&] (juce::Component& c, bool open)
    {
        if (open)
        {
            c.setBounds (bounds.removeFromRight (kSidePanelW));
            c.setVisible (true);
        }
        else
        {
            c.setVisible (false);
        }
    };
    place (*mOtherPanel,  mSidePanelOpen[(int) SidePanel::Other]);
    place (*mPitchPanel,  mSidePanelOpen[(int) SidePanel::Pitch]);
    place (*mTimingPanel, mSidePanelOpen[(int) SidePanel::Timing]);

    // History scrubber at the very bottom
    mHistoryScrubber->setBounds (bounds.removeFromBottom (kHistoryH));

    // View-mode bar above the scrubber
    mViewModeBar->setBounds (bounds.removeFromBottom (kViewModeH));

    // Lane area (split into Guide / sync / Dub / protected / Output rows).
    // Each lane-row has a header column on the left + waveform on the right.
    const int rowH = (bounds.getHeight() - kSyncStripH - kProtectedStripH) / 3;

    auto guideRow = bounds.removeFromTop (rowH);
    mGuideHeader->setBounds (guideRow.removeFromLeft (kHeaderW));
    mGuideLane  ->setBounds (guideRow);

    auto syncRow = bounds.removeFromTop (kSyncStripH);
    syncRow.removeFromLeft (kHeaderW);
    mSyncStrip->setBounds (syncRow);

    auto dubRow = bounds.removeFromTop (rowH);
    mDubHeader->setBounds (dubRow.removeFromLeft (kHeaderW));
    mDubLane  ->setBounds (dubRow);

    auto protRow = bounds.removeFromTop (kProtectedStripH);
    protRow.removeFromLeft (kHeaderW);
    mProtectedStrip->setBounds (protRow);

    auto outRow = bounds;
    mOutputHeader->setBounds (outRow.removeFromLeft (kHeaderW));
    mOutputLane  ->setBounds (outRow);
}
