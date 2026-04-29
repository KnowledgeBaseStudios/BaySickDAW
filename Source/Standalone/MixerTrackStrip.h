#pragma once
#include <JuceHeader.h>
#include "SharedUI.h"   // MixerLedButton defined here

// ── PolarityButton (5F-4a) ───────────────────────────────────────────────────
// Single toggle button that shows "Standard" (off) or "Reverse" (on).
// Subclasses juce::Button directly to bypass VibeLAF's filmstrip toggle.
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
        Aux,           // 64px wide — receive-only, no Arm LED (5F-4b B2)
        Vox,           // 64px wide — R1 live-input vocal strip
        Inst,          // 64px wide — R1 live-input instrument strip
    };

    // accentColor: used for fader trailing line and solo button glow.
    // Caller passes VC::Warm for Master, VC::LayerCol[i] for Layer channels, etc.
    MixerTrackStrip(const juce::String& trackName, StripType type, juce::Colour accentColor);
    ~MixerTrackStrip() override;

    // ── Automation prefix (sets componentID on fader and pan knob) ───────────
    void setAutomationPrefix (const juce::String& prefix);

    // ── Level feed (safe to call from UI timer) ───────────────────────────────
    void setLevel(float dBFS);

    // ── Model sync (does NOT fire callbacks) ─────────────────────────────────
    // Use these to push state from PatternManager into the UI.
    void setFaderDb (float db,   bool notify = false);
    void setPan     (float pan,  bool notify = false);
    void setMuted   (bool muted, bool notify = false);
    void setSoloed  (bool soloed,bool notify = false);

    // ── State queries ─────────────────────────────────────────────────────────
    float getFaderDb() const;
    float getPan()     const { return (float)mPanKnob.getValue(); }
    bool  isMuted()    const { return mMuteBtn.getToggleState(); }
    bool  isSoloed()   const { return mSoloBtn.getToggleState(); }
    juce::String getName() const { return mNameLabel.getText(); }

    // Programmatically update displayed name without firing onNameChanged
    void setTrackName(const juce::String& name)
    {
        mNameLabel.setText(name, juce::dontSendNotification);
    }

    // 5F-4b B3: channel ID for cable routing (MixerChannelIds value)
    void setChannelId(int id) { mChannelId = id; }
    int  getChannelId() const { return mChannelId; }

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
    std::function<void(const juce::String&)> onNameChanged;  // fired when label edited (Layer/Bass only)

    // ── Drag start/end for undo gesture capture ───────────────────────────────
    std::function<void()> onFaderDragStarted;
    std::function<void()> onFaderDragEnded;
    std::function<void()> onPanDragStarted;
    std::function<void()> onPanDragEnded;

    // 5F-4b B5: "+" add-send callback (MixerPage wires to CableOverlay)
    std::function<void(int channelId)> onAddSendRequested;

    // R2 (2026-04-23): Vox / Inst Arm-LED click - opens ASIO input picker.
    // Fires the channel id of THIS strip; MixerPage handler reads the
    // current device manager's input channels, shows a popup menu, and
    // writes the selected index to APVTS `_inputChannelIdx` + sets `_arm`
    // accordingly.  Other strip types ignore this callback.
    std::function<void(int channelId)> onArmRequested;

    // R2: tooltip text shown when hovering the Arm LED ("Mic 1" / "no input").
    // MixerPage updates this whenever the input channel selection changes.
    void setInputChannelLabel (const juce::String& label);

    // ── Layout ───────────────────────────────────────────────────────────────
    static int widthForType(StripType t) noexcept
    {
        switch (t)
        {
            case StripType::Master:                  return 96;
            case StripType::Bus:                     return 72;
            case StripType::DrumChannel:
            case StripType::LayerChannel:
            case StripType::BassChannel:
            case StripType::Aux:
            case StripType::Vox:
            case StripType::Inst:
            default:                                 return 64;
        }
    }

    void paint  (juce::Graphics&) override;
    void resized() override;

    // ── 5F-4a: APVTS binding for new controls (polarity/width/arm/bypass) ────
    // Call after setAutomationPrefix() AND after the APVTS params for this
    // prefix have been registered (see VibeSynthProcessor::ensureMixerStripParams).
    // Creates SliderAttachment / ButtonAttachment for each new control so
    // changes flow both ways with APVTS. Safe to call repeatedly (tears down
    // previous attachments first).
    void setApvts(juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& paramPrefix);

private:
    StripType    mType;
    juce::Colour mAccent;
    int          mChannelId { -1 };  // MixerChannelIds value — set by MixerPage
    juce::Point<int> mSocketCentre;   // cable socket centre in local coords
    juce::String mAutomationPrefix;   // e.g. "mixer_layer_0"; source of truth for FX routing

    // ── Existing sub-components ──────────────────────────────────────────────
    juce::Label      mNameLabel;
    DBFSMeter        mMeter;
    MixerLedButton   mMuteBtn;
    MixerLedButton   mSoloBtn;
    juce::TextButton mFXBtn   { "FX Rack" };
    // 2026-04-19: VibeSlider swallows right-click so right-click jogs the knob
    // neither in value nor in rotary angle. Left-click drag works as before.
    VibeSlider       mPanKnob;    // Rotary, -1..+1
    SnapSlider       mFader;      // LinearVertical, -60..+6 dB
    juce::Label      mDbLabel;

    // 5F-4b B5: "+" add-send button
    juce::TextButton mAddSendBtn { "+" };

    // ── 5F-4a: New sub-components ────────────────────────────────────────────
    // Polarity — single toggle; text reflects state ("Standard" / "Reverse").
    // Bus + insert strips only.
    PolarityButton   mPolarityBtn;

    // Width knob: all strip types. Rotary, 0..2, default 1.0 (bipolar around 1).
    VibeSlider       mWidthKnob;   // 2026-04-19: VibeSlider for right-click guard

    // Arm LED (red when armed). Insert strips only.
    MixerLedButton   mArmBtn;

    // R4 (2026-04-23): Listen LED (headphones glyph; green when monitoring).
    // Vox / Inst strips only.
    HeadphonesLedButton mListenBtn;

    // FX Bypass LED (blue). Insert strips only. Syncs with EffectRack.setRackBypassed
    // via the APVTS `_bypass` param (see InsertNode::processBlock in VibeGraph.cpp).
    MixerLedButton   mBypassBtn;

    // Master FX Bypass LED (purple). Master strip ONLY. Global kill-all:
    // when toggled, every effects rack in the app is bypassed regardless of
    // its per-strip bypass. Attaches to APVTS `master_fx_bypass`.
    MixerLedButton   mMasterFXBypassBtn;

    // Attachments (constructed in setApvts, torn down on destroy / rebind)
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    // 5F-4a Batch 6: level + pan + mute + solo — feed InsertNode audio path
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

    // Visibility helpers — driven by mType
    bool hasPolarityRow() const noexcept
        { return mType == StripType::Bus
              || mType == StripType::LayerChannel
              || mType == StripType::BassChannel
              || mType == StripType::DrumChannel
              || mType == StripType::Aux
              || mType == StripType::Vox
              || mType == StripType::Inst; }
    // Arm LED — Vox / Inst live-input strips (R2 2026-04-23) only.
    // Layer / Bass / Drum kept their arm param in APVTS for backward compat
    // but the UI is hidden (R5 will scrub them visually).  Aux strips are
    // receive-only and never had an arm.
    bool hasArm() const noexcept
        { return mType == StripType::Vox
              || mType == StripType::Inst; }
    // Utility row — always present now (FX bypass LED is on every strip type)
    bool hasUtilityRow() const noexcept { return true; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerTrackStrip)
};
