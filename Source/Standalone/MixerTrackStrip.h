#pragma once
#include <JuceHeader.h>
#include "SharedUI.h"   // MixerLedButton defined here

// ── PolarityButton (5F-4a) ───────────────────────────────────────────────────
// Single toggle button that shows "Standard" (off) or "Reverse" (on).
// Subclasses juce::Button directly to bypass BaySickLAF's filmstrip toggle.
// Blue tint on text when in "Reverse" state.
// ─────────────────────────────────────────────────────────────────────────────
class PolarityButton : public juce::Button
{
public:
    PolarityButton() : juce::Button("polarity") { setClickingTogglesState(true); }

    void paintButton(juce::Graphics& g, bool isOver, bool /*isDown*/) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);

        // Recessed body (same style as MixerLedButton for consistency)
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(juce::Colour(isOver ? 0xff555555 : 0xff3a3a3a));
        g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);

        const bool reversed = getToggleState();
        g.setColour(reversed ? juce::Colour(0xff4488ff) : juce::Colour(0xffaaaaaa));
        g.setFont(juce::Font(8.5f, juce::Font::bold));
        g.drawText(reversed ? "Reverse" : "Standard",
                   b.toNearestInt(), juce::Justification::centred);
    }
};

// ── MixerCollapseArrow (QA-RustyMeter Task 5) ────────────────────────────────
// Tiny triangle button on a Bus strip's name row: points DOWN when the bus's
// channel strips are expanded (shown), UP when collapsed (hidden) -- the
// RibbonTabBar tab-dropdown idiom.  Greyed + non-interactive (disabled) when the
// bus has no member strips to collapse.
// ─────────────────────────────────────────────────────────────────────────────
class MixerCollapseArrow : public juce::Button
{
public:
    MixerCollapseArrow() : juce::Button("collapse") {}
    bool collapsed = false;   // false = expanded (arrow down) / true = collapsed (arrow up)

    void paintButton(juce::Graphics& g, bool isOver, bool /*isDown*/) override
    {
        auto b = getLocalBounds().toFloat();
        const bool en = isEnabled();
        if (isOver && en)
        {
            g.setColour(juce::Colour(0x22ffffff));
            g.fillRoundedRectangle(b.reduced(0.5f), 2.0f);
        }
        auto tri = b.reduced(b.getWidth() * 0.24f, b.getHeight() * 0.34f);
        juce::Path p;
        if (collapsed)   // up-pointing = collapsed
            p.addTriangle(tri.getX(), tri.getBottom(),
                          tri.getRight(), tri.getBottom(),
                          tri.getCentreX(), tri.getY());
        else             // down-pointing = expanded
            p.addTriangle(tri.getX(), tri.getY(),
                          tri.getRight(), tri.getY(),
                          tri.getCentreX(), tri.getBottom());
        g.setColour(en ? juce::Colour(0xffBFC4C6) : juce::Colour(0xff44484A));
        g.fillPath(p);
    }
};

// ── MixerTrackStrip ────────────────────────────────────────────────────────────
// One vertical channel strip in the mixer console.
//
// Layout (top → bottom):
//   ┌───────────────┐
//   │  [Track Name] │   editable label
//   │  [DBFSMeter]  │   peak + hold, 60fps
//   │   [M]   [S]   │   Mute / Solo toggles
//   │    [Pan Knob] │   rotary, center-snap
//   │  ╔═════════╗  │
//   │  ║ Level   ║  │   LinearVertical fader (SnapSlider, 0 dB snap)
//   │  ║ Fader   ║  │
//   │  ╚═════════╝  │
//   │   [-6.0 dB]   │   dB readout label
//   └───────────────┘
//
// StripType controls width and accent color used for the fader trailing line
// and mute/solo button highlights.
// ─────────────────────────────────────────────────────────────────────────────
class MixerTrackStrip : public juce::Component,
                        private juce::Slider::Listener
{
public:
    enum class StripType
    {
        Master,        // 96px wide, gold accent
        Bus,           // 72px wide, tinted accent (Layers/Bass/Drums/FX)
        DrumChannel,   // 64px wide
        LayerChannel,  // 64px wide
        BassChannel,   // 64px wide
        Aux,           // 64px wide - receive-only, no Arm LED (5F-4b B2)
        Vox,           // 64px wide - R1 live-input vocal strip
        Inst,          // 64px wide - R1 live-input instrument strip
    };

    // accentColor: used for fader trailing line and solo button glow.
    // Caller passes VC::Warm for Master, VC::LayerCol[i] for Layer channels, etc.
    MixerTrackStrip(const juce::String& trackName, StripType type, juce::Colour accentColor);
    ~MixerTrackStrip() override;

    // ── Automation prefix (sets componentID on fader and pan knob) ───────────
    void setAutomationPrefix (const juce::String& prefix);

    // QA-ModelShell TS3: reRegisterAutomation() is gone.  It existed because
    // this strip registered its own view-scoped "_fader" applicator, which had
    // no APVTS twin for resetProjectState()'s re-seed to find.  Registration is
    // now model-side at param materialization and the "_fader" alias is derived
    // from the "_level" param, so there is nothing left to put back.

    // ── Level feed (safe to call from UI timer) ───────────────────────────────
    void setLevel(float dBFS);
    // 2026-04-30: stereo entry point - independent L/R peak feeds.  When the
    // audio thread doesn't yet have stereo peakDb atomics wired, callers stay
    // on setLevel() (mono fans to L=R inside DBFSMeter).  As bus / insert
    // nodes get split into peakDbL+peakDbR, callers switch to this method.
    void setStereoLevel(float dBFS_L, float dBFS_R);
    // QA-RustyMeter (2026-05-30): windowed-RMS feed for the split meter top half.
    void setRmsStereo(float dBFS_L, float dBFS_R);
    // QA-RustyMeter Task 3 (2026-05-30): master-strip LUFS feed (Momentary /
    // Short-Term / Integrated).  MixerPage pushes all three each vblank; the box
    // displays the selected one.  No-op on non-master strips (box hidden).
    void setMasterLufs(float momentary, float shortTerm, float integrated)
    {
        mLufsBox.setValues(momentary, shortTerm, integrated);
    }

    // QA-Eg: cable-overlay telemetry feed.  Returns the DBFSMeter's currently-
    // displayed smoothed value (FL-style ballistic-filtered max of L/R).
    // Routing through DBFSMeter gives cables the same visual smoothness as
    // the meter LEDs - no separate smoothing layer, no double-smoothing
    // artifacts, cables and meters stay in perfect visual sync.
    float getCurrentPeakDb() const noexcept { return mMeter.getCurrentDisplayedDb(); }

    // ── Model sync (does NOT fire callbacks) ─────────────────────────────────
    // Use these to push state from PatternManager into the UI.
    void setFaderDb (float db,   bool notify = false);
    void setPan     (float pan,  bool notify = false);
    void setMuted   (bool muted, bool notify = false);
    void setSoloed  (bool soloed,bool notify = false);

    // ── State queries ─────────────────────────────────────────────────────────
    bool  isMuted()    const { return mMuteBtn.getToggleState(); }
    bool  isSoloed()   const { return mSoloBtn.getToggleState(); }
    juce::String getName() const { return mNameLabel.getText(); }

    // Programmatically update displayed name without firing onNameChanged
    void setTrackName(const juce::String& name)
    {
        mNameLabel.setText(name, juce::dontSendNotification);
        refreshNameTooltip();   // QA-RustyMeter Task 5: keep the hover tooltip current
    }

    // G-6 (2026-04-29): opt Bus-type strips into rename.  By default Bus
    // strips are non-renameable (system buses have fixed identity); Vox/Inst
    // BUS strips opt in via this setter.
    void setRenameable (bool canRename)
    {
        mNameLabel.setEditable (canRename, canRename, false);
        refreshNameTooltip();   // QA-RustyMeter Task 5: name + rename hint where editable
    }

    // ── QA-RustyMeter Task 5: bus collapse/expand ────────────────────────────
    // Fired when the bus collapse arrow is clicked; passes this strip's channel
    // id.  MixerPage flips the persisted state + relayouts.  (Bus strips only.)
    std::function<void(int channelId)> onCollapseToggled;
    // Drive the arrow direction (false = expanded / down, true = collapsed / up).
    void setCollapsed (bool c) { mCollapseBtn.collapsed = c; mCollapseBtn.repaint(); }
    // Grey out + disable the arrow when the bus has no member strips to collapse.
    void setCollapseEnabled (bool e) { mCollapseBtn.setEnabled (e); }

    // 5F-4b B3: channel ID for cable routing (MixerChannelIds value)
    void setChannelId(int id) { mChannelId = id; }
    int  getChannelId() const { return mChannelId; }

    // K-2 (2026-05-05): suppress arm + listen LEDs for sfizz-driven Inst strips
    // (BaySickGuitars / BaySickBasses) which have no live input - the engine
    // IS the source.  Strip stays type Inst (so it categorizes correctly on
    // mixer / FX / sends), but hasArm() returns false and the buttons are
    // hidden.  Safe to call before or after setApvts; the parameter listener
    // for `_arm` is only registered when noLiveInput is false.
    void setNoLiveInput (bool b);
    bool isNoLiveInput() const noexcept { return mNoLiveInput; }

    // Display-only marker for a sfizz Inst strip whose saved kit was
    // substituted at restore.  Renders as an error-red NAME COLOR + a tooltip
    // line -- NEVER as label text: getName() is mNameLabel.getText(), which is
    // what <InstNames> persists AND what the in-place rename editor commits,
    // and the 74 px name slot tail-ellipsizes any suffix anyway.
    void setKitMissing (bool b)
    {
        if (b == mKitMissing) return;
        mKitMissing = b;
        // 0xffff5555 is the app's missing-file red (BaySickNAMIREditor kErrARGB).
        mNameLabel.setColour (juce::Label::textColourId,
                              mKitMissing ? juce::Colour (0xffff5555) : VC::Text);
        refreshNameTooltip();
        repaint();
    }

    // Unique APVTS prefix for this strip (e.g. "mixer_layer_0", "mixer_master").
    // Stable across renames, used by the Effects Page dropdown mapping.
    const juce::String& getAutomationPrefix() const { return mAutomationPrefix; }

    // Update the strip's top-bar accent color (e.g. when rerouted between buses).
    void setAccentColor(juce::Colour c);

    // 5F-4b B5: cable socket centre (in strip-local coords), set by resized().
    // MixerPage translates to page coords for cable rendering.
    juce::Point<int> getSocketCentre() const { return mSocketCentre; }

    // ── Callbacks (set by MixerPage) ─────────────────────────────────────────
    std::function<void(float dB)>  onFaderChanged;
    std::function<void(bool)>      onMuteChanged;
    std::function<void(bool)>      onSoloChanged;
    std::function<void(float pan)>           onPanChanged;   // pan in -1..+1
    std::function<void(const juce::String&)> onFXClicked;    // route to Effects page, passes channel id
    std::function<void(const juce::String&)> onNameChanged;  // fired when the label is edited, on every renameable strip type

    // ── Drag start/end for undo gesture capture ───────────────────────────────
    std::function<void()> onFaderDragStarted;
    std::function<void()> onFaderDragEnded;
    std::function<void()> onPanDragStarted;
    std::function<void()> onPanDragEnded;

    // 5F-4b B5: "+" add-send callback (MixerPage wires to CableOverlay)
    std::function<void(int channelId)> onAddSendRequested;
    // QA-ModelShell TS7 (CL-044): fired instead of onAddSendRequested when this
    // is the MASTER strip, whose "+" is repurposed as the Analyzer button (a send
    // from the terminal node has nowhere to go).
    std::function<void()>              onAnalyzerRequested;

    // R2 (2026-04-23): Vox / Inst Arm-LED click - opens ASIO input picker.
    // Fires the channel id of THIS strip; MixerPage handler reads the
    // current device manager's input channels, shows a popup menu, and
    // writes the selected index to APVTS `_inputChannelIdx` + sets `_arm`
    // accordingly.  Other strip types ignore this callback.
    std::function<void(int channelId)> onArmRequested;

    // G-7 (2026-04-29): right-click context menu hook.  MixerPage installs a
    // handler on Aux strips + secondary Vox/Inst bus strips that pops a
    // Delete prompt.  Other strips ignore right-click (left as-is).
    std::function<void(juce::Point<int> screenPos)> onContextMenuRequested;

    // R2: tooltip text shown when hovering the Arm LED ("Mic 1" / "no input").
    // MixerPage updates this whenever the input channel selection changes.
    void setInputChannelLabel (const juce::String& label);

    // ── Layout ───────────────────────────────────────────────────────────────
    static int widthForType(StripType t) noexcept
    {
        switch (t)
        {
            // 2026-04-30 v2: every strip type is now 80 px wide.  Earlier 64
            // for inserts left the Pan/Width knobs cramped after the meter
            // moved into a 28 px right column.  72 for Master (matching
            // Bus) was still slightly tight.  Unified at 80 - controls
            // column gets ~52 px usable width on every strip, regardless
            // of type.
            case StripType::Master:
            case StripType::Bus:
            case StripType::DrumChannel:
            case StripType::LayerChannel:
            case StripType::BassChannel:
            case StripType::Aux:
            case StripType::Vox:
            case StripType::Inst:
            default:                                 return 80;
        }
    }

    void paint     (juce::Graphics&)         override;
    void resized   ()                        override;
    void mouseDown (const juce::MouseEvent&) override;   // G-7: right-click → onContextMenuRequested

    // QA-TrueLevel SC-12: a strip whose file is gone (Direct to Master after
    // "Proceed missing") greys out under a "+" overlay; a left click on the
    // strip then fires onMissingFileClicked, which the editor answers with the
    // native chooser at the file's last-known folder.
    void setMissingFile (bool missing)
    {
        if (mMissingFile == missing) return;
        mMissingFile = missing;
        repaint();
    }
    bool isMissingFile() const noexcept { return mMissingFile; }
    std::function<void()> onMissingFileClicked;

    // ── 5F-4a: APVTS binding for new controls (polarity/width/arm/bypass) ────
    // Call after setAutomationPrefix() AND after the APVTS params for this
    // prefix have been registered (see BaySickDAWProcessor::ensureMixerStripParams).
    // Creates SliderAttachment / ButtonAttachment for each new control so
    // changes flow both ways with APVTS. Safe to call repeatedly (tears down
    // previous attachments first).
    void setApvts(juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& paramPrefix);

private:
    bool mMissingFile { false };

    // QA-E Task 5 (2026-05-15): the C3 (2026-05-04) parameterChanged + manual
    // listener-install workaround was removed when arm-button wiring switched
    // to a unified ButtonAttachment + onRightClick path.  ButtonAttachment
    // handles the APVTS -> button sync automatically; no manual listener needed.

    StripType    mType;
    juce::Colour mAccent;
    int          mChannelId { -1 };  // MixerChannelIds value - set by MixerPage
    bool         mNoLiveInput { false }; // K-2: suppress arm/listen for sfizz-source Inst strips
    bool         mKitMissing  { false };
    juce::Point<int> mSocketCentre;   // cable socket centre in local coords
    juce::String mAutomationPrefix;   // e.g. "mixer_layer_0"; source of truth for FX routing

    // ── Existing sub-components ──────────────────────────────────────────────
    juce::Label      mNameLabel;
    DBFSMeter        mMeter;
    MixerLedButton   mMuteBtn;
    MixerLedButton   mSoloBtn;
    juce::TextButton mFXBtn   { "FX Rack" };
    // 2026-04-19: BaySickSlider swallows right-click so right-click jogs the knob
    // neither in value nor in rotary angle. Left-click drag works as before.
    BaySickSlider       mPanKnob;    // Rotary, -1..+1
    SnapSlider       mFader;      // LinearVertical, -60..+5.6 dB (matches the
                                  // _level param range AND the drawn dB marks)
    juce::Label      mDbLabel;

    // 5F-4b B5: "+" add-send button
    juce::TextButton mAddSendBtn { "+" };

    // ── 5F-4a: New sub-components ────────────────────────────────────────────
    // Polarity - single toggle; text reflects state ("Standard" / "Reverse").
    // Bus + insert strips only.
    PolarityButton   mPolarityBtn;

    // Width knob: all strip types. Rotary, 0..2, default 1.0 (bipolar around 1).
    BaySickSlider       mWidthKnob;   // 2026-04-19: BaySickSlider for right-click guard

    // Arm LED (red when armed). Insert strips only.
    MixerLedButton   mArmBtn;

    // R4 (2026-04-23): Listen LED (headphones glyph; green when monitoring).
    // Vox / Inst strips only.
    HeadphonesLedButton mListenBtn;

    // FX Bypass LED (blue). Every strip type (each owns a rack). Syncs with
    // EffectRack.setRackBypassed via the APVTS `_bypass` param (see
    // InsertNode::processBlock in BaySickGraph.cpp).
    MixerLedButton   mBypassBtn;

    // Master FX Bypass LED (purple). Master strip ONLY. Global kill-all:
    // when toggled, every effects rack in the app is bypassed regardless of
    // its per-strip bypass. Attaches to APVTS `master_fx_bypass`.
    MixerLedButton   mMasterFXBypassBtn;

    // QA-RustyMeter Task 3 (2026-05-30): master LUFS readout. Master strip ONLY
    // (added to the component + positioned in resized() only when mType==Master).
    // Sits between the width knob and the fader; fed by MixerPage::onVBlank.
    LufsReadoutBox   mLufsBox;

    // QA-RustyMeter Task 5 (2026-05-30): bus collapse arrow.  Bus strips ONLY
    // (added + positioned only when mType==Bus); fires onCollapseToggled.
    MixerCollapseArrow mCollapseBtn;

    // Attachments (constructed in setApvts, torn down on destroy / rebind)
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    // 5F-4a Batch 6: level + pan + mute + solo - feed InsertNode audio path
    std::unique_ptr<SliderAtt> mLevelAtt;
    std::unique_ptr<SliderAtt> mPanAtt;
    std::unique_ptr<ButtonAtt> mMuteAtt;
    std::unique_ptr<ButtonAtt> mSoloAtt;
    std::unique_ptr<SliderAtt> mWidthAtt;
    std::unique_ptr<ButtonAtt> mPolarityAtt;
    std::unique_ptr<ButtonAtt> mArmAtt;
    std::unique_ptr<ButtonAtt> mListenAtt;   // R4: Vox / Inst Listen toggle
    std::unique_ptr<ButtonAtt> mBypassAtt;
    std::unique_ptr<ButtonAtt> mMasterFXBypassAtt;  // Master strip only

    bool mUpdating { false };   // re-entrancy guard for setFaderDb / setMuted / setSoloed

    // Slider::Listener overrides (for drag start/end)
    void sliderValueChanged  (juce::Slider*) override {}  // handled via onFaderChanged / onPanChanged
    void sliderDragStarted   (juce::Slider* s) override;
    void sliderDragEnded     (juce::Slider* s) override;

    void updateDbLabel();

    // QA-RustyMeter Task 5 (2026-05-30): hover tooltip = the full (possibly
    // truncated) name, plus the substituted-kit line and a "Double-click to
    // rename" line where each applies.  Refreshed on construct / rename /
    // setTrackName.  All strip types.  The kit line is RE-DERIVED here rather
    // than written once by setKitMissing because project restore runs
    // restoreStripNames -> setTrackName AFTER the tab loop that sets the flag;
    // a directly-set tooltip would be wiped by that later pass.
    void refreshNameTooltip()
    {
        juce::String tip = mNameLabel.getText();
        if (mKitMissing)
            tip += "\nSaved kit is missing - playing the default kit instead";
        if (mNameLabel.isEditableOnDoubleClick())
            tip += "\nDouble-click to rename";
        mNameLabel.setTooltip (tip);
    }

    // Visibility helpers - driven by mType
    bool hasPolarityRow() const noexcept
        { return mType == StripType::Bus
              || mType == StripType::LayerChannel
              || mType == StripType::BassChannel
              || mType == StripType::DrumChannel
              || mType == StripType::Aux
              || mType == StripType::Vox
              || mType == StripType::Inst; }
    // Arm LED - Vox / Inst live-input strips (R2 2026-04-23) only.
    // Layer / Bass / Drum kept their arm param in APVTS for backward compat
    // but the UI is hidden (R5 will scrub them visually).  Aux strips are
    // receive-only and never had an arm.
    // K-2 (2026-05-05): sfizz-source Inst strips (BaySickGuitars / BaySickBasses)
    // also lose the LED via mNoLiveInput - the engine IS the source.
    bool hasArm() const noexcept
        { return ! mNoLiveInput
              && (mType == StripType::Vox
              ||  mType == StripType::Inst); }
    // Utility row - always present now (FX bypass LED is on every strip type)
    bool hasUtilityRow() const noexcept { return true; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerTrackStrip)
};
