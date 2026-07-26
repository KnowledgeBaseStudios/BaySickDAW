#pragma once
#include <JuceHeader.h>
#include "../VibesynthConstants.h"
#include "../PatternManager.h"
#include "../DSP/SpectrumFeed.h"   // for SpectrumFeed::kSize used by ParametricEQDisplay

// ── Colour palette ────────────────────────────────────────────────────────────
namespace VC {
    // Chassis / structural
    inline const juce::Colour Bg        = juce::Colour(0xff1c1c1e);   // near-black charcoal
    inline const juce::Colour Panel     = juce::Colour(0xff252529);   // dark module panel
    inline const juce::Colour Surface   = juce::Colour(0xff2e2e33);   // raised surface
    inline const juce::Colour Accent    = juce::Colour(0xff3a3a3e);   // border / groove
    // Text
    inline const juce::Colour Text      = juce::Colour(0xffe0e0e8);
    inline const juce::Colour TextDim   = juce::Colour(0xff808090);
    // Neon accents
    inline const juce::Colour Highlight = juce::Colour(0xffe94560);
    inline const juce::Colour Green     = juce::Colour(0xff00e5a0);
    inline const juce::Colour Blue      = juce::Colour(0xff4cc9f0);
    inline const juce::Colour Yellow    = juce::Colour(0xffffcc00);
    inline const juce::Colour Orange    = juce::Colour(0xffff9f1c);
    inline const juce::Colour Purple    = juce::Colour(0xff7b61ff);
    inline const juce::Colour Red       = juce::Colour(0xffe94560);
    // Hardware-inspired
    inline const juce::Colour Warm      = juce::Colour(0xffd4a017);   // amber / gold
    inline const juce::Colour Chrome    = juce::Colour(0xff9090a0);   // metallic knob face

    inline const juce::Colour EQGridBg    = juce::Colour(0xff1a2229);
    inline const juce::Colour EQGridLine  = juce::Colour(0xff2d3a45);

    // Layer pages L1–L8 (orange family)
    inline const juce::Colour LayerCol[8] = {
        juce::Colour(0xffff8833),   // L1 bright orange
        juce::Colour(0xffff6a1a),   // L2 deep orange
        juce::Colour(0xffff9900),   // L3 amber orange
        juce::Colour(0xffcc5500),   // L4 burnt orange
        juce::Colour(0xffff7755),   // L5 coral orange
        juce::Colour(0xffffaa66),   // L6 peach orange
        juce::Colour(0xffcc7722),   // L7 muted orange
        juce::Colour(0xffffbb44),   // L8 golden orange
    };
    // Bass pages B1–B4 (green family)
    inline const juce::Colour BassCol[4] = {
        juce::Colour(0xff33ff88),   // B1 bright neon green
        juce::Colour(0xff66cc00),   // B2 lime green
        juce::Colour(0xff00aa88),   // B3 teal green
        juce::Colour(0xff229944),   // B4 forest green
    };
    // Drums (single red, legacy)
    inline const juce::Colour DrumsCol = juce::Colour(0xffff4444);

    // Drum pages D1–D16 (red family, sequenced shades, no orange/pink - Phase C
    // Batch 5 spec).  Used by D1.3 DrumPage tabs.
    inline const juce::Colour DrumCol[16] = {
        juce::Colour(0xff4a0000), juce::Colour(0xff5e0000), juce::Colour(0xff720000), juce::Colour(0xff860000),
        juce::Colour(0xff9a0000), juce::Colour(0xffae0000), juce::Colour(0xffc20000), juce::Colour(0xffd60000),
        juce::Colour(0xffea0000), juce::Colour(0xffff0000), juce::Colour(0xffff2222), juce::Colour(0xffff4444),
        juce::Colour(0xffff6666), juce::Colour(0xffff8888), juce::Colour(0xffffaaaa), juce::Colour(0xffffcccc),
    };
}

// ── APVTS-tagged slider attachment ────────────────────────────────────────────
// Drop-in replacement for AudioProcessorValueTreeState::SliderAttachment that
// also tags the slider with its paramId via getProperties().set("apvtsId", id),
// so setSliderDoubleClickDefaultsFromApvts can find it and reset its double-click
// return to the param's FACTORY DEFAULT, without each editor maintaining its own
// (slider, paramId) registry.
class TaggedSliderAttachment : public juce::AudioProcessorValueTreeState::SliderAttachment
{
public:
    TaggedSliderAttachment (juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& paramId,
                            juce::Slider& slider)
        : SliderAttachment (apvts, paramId, slider)
    {
        slider.getProperties().set ("apvtsId", paramId);
    }
};

// Recursively walks all child components of `root`, finds every juce::Slider
// tagged with an "apvtsId" property, and sets that slider's double-click return
// to the param's FACTORY DEFAULT.  QA-ClipPlayback: was the loaded-patch value
// (deliberate, but non-standard) -- double-click now behaves like every other
// DAW knob (returns to the factory default, not whatever was last saved).
void setSliderDoubleClickDefaultsFromApvts (juce::Component& root,
                                            juce::AudioProcessorValueTreeState& apvts);

// ── QA-Fd FL knob conventions (locked P1-14; vocal editors) ───────────────────
// Detent at the default value (Shift bypasses), Ctrl-drag = fine velocity
// mode, editable value box for type-in.  Wraps any existing onValueChange.
inline void applyFLKnobFeel (juce::Slider& s, double defaultValue)
{
    s.setDoubleClickReturnValue (true, defaultValue);
    s.setVelocityModeParameters (0.6, 1, 0.0, true,
                                 juce::ModifierKeys::ctrlModifier);
    s.setTextBoxIsEditable (true);
    const double detent =
        juce::jmax (1.0e-9, s.getRange().getLength()) * 0.015;
    auto prev = s.onValueChange;
    s.onValueChange = [&s, defaultValue, detent, prev]
    {
        if (! juce::ModifierKeys::currentModifiers.isShiftDown()
            && s.getValue() != defaultValue
            && std::abs (s.getValue() - defaultValue) < detent)
        {
            // Re-enters this handler carrying the detented value; prev fires
            // on that pass.
            s.setValue (defaultValue, juce::sendNotificationSync);
            return;
        }
        if (prev) prev();
    };
}

// ── Shared LookAndFeel (forward declared -- defined in SharedUI.cpp) ──────────
class VibeLAF : public juce::LookAndFeel_V4
{
public:
    VibeLAF();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sp, float sa, float ea, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour&, bool isOver, bool isDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool isOver, bool isDown) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h,
                          float sp, float lo, float hi,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawGroupComponentOutline(juce::Graphics&, int w, int h,
                                   const juce::String& text,
                                   const juce::Justification&,
                                   juce::GroupComponent&) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool isDown,
                      int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawTooltip(juce::Graphics&, const juce::String& text,
                     int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& text,
                                          juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;
    juce::Font getLabelFont(juce::Label&) override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h,
                       bool isVertical, int thumbStart, int thumbSize,
                       bool isOver, bool isDown) override;
    static VibeLAF& get() { static VibeLAF laf; return laf; }

    // Helper: mark a tooltip string as automatable (appends tag)
    static juce::String automatable(const juce::String& tip)
    {
        return tip + "\n*Automatable*";
    }
};

// ── VibeTooltip ───────────────────────────────────────────────────────────────
// Global tooltip window. Renders via VibeLAF::drawTooltip / getTooltipBounds.
// One instance owned by StandaloneEditor - applies to all child components.
// APVTS-bound controls append "\n*Automatable*" via VibeLAF::automatable().
class VibeTooltip : public juce::TooltipWindow
{
public:
    explicit VibeTooltip(juce::Component* parent, int delayMs = 500)
        : juce::TooltipWindow(parent, delayMs)
    {
        setLookAndFeel(&VibeLAF::get());
    }

    ~VibeTooltip() override
    {
        setLookAndFeel(nullptr);
    }
};

// ── LRX - Texture cache ───────────────────────────────────────────────────────
// Generates and caches expensive textures (brushed aluminum, Voronoi, grunge).
// All methods are thread-safe once the image has been generated (first call on
// the message thread). Keys are derived from type + dimensions.
struct TextureUtils
{
    static const juce::Image& brushedAluminum(int w, int h);
    static const juce::Image& voronoiCellular(int w, int h);
    static const juce::Image& fingerGrunge   (int w, int h);

private:
    static std::map<juce::String, juce::Image>& cache();
    static juce::Image makeBrushedAluminum(int w, int h);
    static juce::Image makeVoronoi        (int w, int h);
    static juce::Image makeFingerGrunge   (int w, int h);
};

// ── LRX - Realism drawing helpers ────────────────────────────────────────────
// Shared by all LAF classes. Static-only.
struct LRXHelper
{
    // Three-layer AO shadow: contact + drop + optional uplight reflection
    static void drawAO(juce::Graphics& g, const juce::Path& shape,
                       bool withReflection = false,
                       juce::Colour reflectCol = juce::Colours::transparentBlack);

    // Double-draw bloom: wide+faint pass then normal pass
    static void drawWithBloom(juce::Graphics& g, const juce::Path& path,
                              juce::Colour col, float width,
                              float bloomMult = 2.5f, float bloomAlpha = 0.28f);

    // Fresnel rim: bright thin ring at perimeter, strongest at bottom
    static void drawFresnelRim(juce::Graphics& g, juce::Rectangle<float> bounds,
                               juce::Colour rimCol, float thickness = 1.2f);

    // Anisotropic highlights for brushed-metal cap surfaces
    static void drawAnisotropicHL(juce::Graphics& g, juce::Rectangle<float> capBounds,
                                  float lightAngleDeg = 135.f);

    // Phillips-head mounting screws at 4 panel corners
    static void drawMountingScrews(juce::Graphics& g, juce::Rectangle<int> panel,
                                   int inset = 6,
                                   juce::Colour col = juce::Colour(0xff606070));

    // Global lens vignette (call from window paint())
    static void drawVignette(juce::Graphics& g, juce::Rectangle<int> bounds,
                             float strength = 0.45f);

    // Fingerprint grunge overlay
    static void applyGrunge(juce::Graphics& g, juce::Rectangle<int> bounds,
                            float intensity = 0.06f);
};

// ── Filmstrip rendering helpers ───────────────────────────────────────────────
// All filmstrips are vertical PNG strips (frames stacked top-to-bottom).
// Images are lazy-loaded at first use from "Files For Claude/Filmstrips/" relative
// to the project root (4 directories above the standalone .exe).
namespace Filmstrips
{
    // Draw one frame from a filmstrip. normalizedValue is 0.0 (first frame) to
    // 1.0 (last frame). Scales the frame to fill destBounds exactly.
    void drawFrame(juce::Graphics& g, const juce::Image& strip,
                   int frameW, int frameH, int numFrames,
                   float normalizedValue,
                   juce::Rectangle<float> destBounds);

    // Lazy-loaded filmstrip accessors
    const juce::Image& dynamics();    // Dynamics Group Knobs.png  - 96x96,   31 frames
    const juce::Image& harmonics();   // Harmonics Group Knobs.png - 128x128, 200 frames
    const juce::Image& modulation();  // Modulation Group Knobs.png - 70x70,  101 frames
    const juce::Image& timeBased();   // Time Based Group Knobs.png - 64x64,  101 frames
    const juce::Image& chickenHead(); // Chicken Head.png           - 66x66,   10 frames
    const juce::Image& fader();         // Fader Slider.png           - 128x128,  31 frames
    const juce::Image& faderInverted(); // Fader Slider.png (pixel-inverted) - for Dynamics panel
    const juce::Image& vuMeter();      // VU Meter.png               - 128x128, 100 frames
    const juce::Image& switchToggle(); // Switch Toggle.png          -   46x92,   2 frames
    const juce::Image& volumeBlack();  // Volume Black.png           -   70x70, 100 frames
    const juce::Image& volumeWhite();  // Volume White.png           -   70x70, 100 frames
}

// ── PageMenuBar ───────────────────────────────────────────────────────────────
// Tier 2: sits below the ribbon, above sub-tabs / content.
// Shows ≡ hamburger (opens popup), optional page title, optional action items.
class PageMenuBar : public juce::Component
{
public:
    struct MenuItem { juce::String label; std::function<void()> action; };

    PageMenuBar();

    void setPageTitle(const juce::String& t);
    void setMenuItems(std::vector<MenuItem> items);

    // Universal page-actions menu (2026-04-19): components can install a custom
    // menu builder that takes precedence over the simple mMenuItems list. Used
    // by ParametricEQDisplay (and future per-tab actions across all pages) to
    // surface their options through the ≡ hamburger instead of an in-component
    // ... button. The builder is given the hamburger as the popup anchor; it
    // is responsible for both populating + showing the menu (so it can include
    // submenus, checkmarks, disabled items, etc. that the simple MenuItem list
    // can't express). Pass nullptr to clear and revert to mMenuItems behaviour.
    using MenuBuilder = std::function<void(juce::Component* anchor)>;
    void setMenuBuilder(MenuBuilder builder);
    void addActionButton(const juce::String& label, std::function<void()> action);
    void clearActionButtons();

    // ── Non-owning extra components on the right (e.g. Kit ▾, Nav combo) ────────
    // Components are reparented into PageMenuBar. Call clear before the page hides.
    void addExtraRightComponent(juce::Component* c, int width);
    // 2026-04-19: targeted removal so per-tab extras (e.g. EQ bank indicator)
    // can be added/removed without disturbing page-level extras that should
    // persist across tab switches. No-op if c isn't currently in the list.
    void removeExtraRightComponent(juce::Component* c);
    void clearExtraRightComponents();

    // ── Tab slot buttons (owned, laid out after ≡) ────────────────────────────
    // Call setTabSlots when a Layers/Bass/Drums page becomes visible.
    // Call clearTabSlots when switching to a page with no sub-tabs.
    void setTabSlots(const juce::StringArray& labels,
                     std::function<void(int)> onTabClick,
                     int activeIdx = 0,
                     juce::Colour accent = juce::Colour());
    void updateTabActive(int idx);
    void clearTabSlots();

    // Phase C §P4.2 (2026-04-24): convert an existing tab-slot button into a
    // split-button.  Body-click still fires the slot's onTabClick; clicks
    // inside a small right-edge arrow zone fire `onArrow` instead (typically
    // opens a mode-picker popup).  `getDynamicLabel`, if set, is polled on
    // every paint so the button text reflects external state changes (e.g.
    // "Drum Grid" vs "Full Piano Roll" for the Drums Piano Roll slot).
    void setTabSlotArrow (int idx,
                          std::function<void(juce::Component*)> onArrow,
                          std::function<juce::String()> getDynamicLabel = {});

    // MID/SIDE slot (laid out after tab slots, only for EQ tab)
    void setMidSideSlots(std::function<void()> onMid, std::function<void()> onSide,
                         bool midActive = true);
    void setMidSideVisible(bool show);
    void updateMidSideActive(bool midActive);

    // 2026-04-19: dedicated bank-indicator slot, laid out IMMEDIATELY after the
    // MID/SIDE buttons (and before the right-extras cluster). Single non-owning
    // pointer - calling with the same component again is a no-op; calling with
    // a different component replaces; calling with nullptr removes. Avoids the
    // duplicate-stacking bug the previous extras-list approach had where every
    // EQ-tab click appended a new entry.
    void setBankIndicator(juce::Component* indicator);

    // FX Rack jump slot at the right end of the page-tab button cluster
    // (after tabs / MID-SIDE / bank pill).  Set per page-show with that
    // page's jump; empty fn hides it.  clearTabSlots() clears it too --
    // the slot belongs to the tab cluster's lifecycle.
    void setFxRackSlot(std::function<void()> onClick);

    // Smoke round 2 (Jeff): per-player Swing Mix knob, right of the FX Rack
    // slot, so it's visible on EVERY sub-tab of a player page (the engine
    // title-bar hosting only showed on the Player sub-tab).  Set per
    // page-show with that page's swing binding; empty getMix hides it;
    // clearTabSlots() clears it too.
    void setSwingKnobSlot (std::function<float()>     getMix,
                           std::function<void(float)> setMix,
                           std::function<bool()>      getTruncate,
                           std::function<void(bool)>  setTruncate);

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kHeight = 26;

private:
    juce::String mTitle;
    std::vector<MenuItem> mMenuItems;
    MenuBuilder           mMenuBuilder;

    std::unique_ptr<juce::TextButton> mHamburgerBtn;
    std::vector<std::unique_ptr<juce::TextButton>> mActionBtns;

    // Non-owning extra right components (e.g. Kit button, Nav combo)
    struct ExtraComp { juce::Component* comp; int width; };
    std::vector<ExtraComp> mExtraRight;

    // Tab slot buttons (owned)
    std::vector<std::unique_ptr<juce::TextButton>> mTabSlotBtns;
    std::unique_ptr<juce::TextButton>              mMidBtn;
    std::unique_ptr<juce::TextButton>              mSideBtn;
    std::unique_ptr<juce::TextButton>              mFxRackBtn;
    std::unique_ptr<juce::Slider>                  mSwingKnob;   // smoke round 2: per-player Swing Mix
    bool                                           mMidSideVisible { false };

    // 2026-04-19: bank indicator slot (non-owning, single pointer).
    juce::Component*                               mBankIndicator { nullptr };

    void showHamburgerMenu();
};

// ── Modulation effect LookAndFeel (Chorus / Flanger / Phaser) ────────────────
class ModulationLAF : public juce::LookAndFeel_V4
{
public:
    static ModulationLAF& get() { static ModulationLAF instance; return instance; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float startAngle, float endAngle, juce::Slider& s) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& b,
        const juce::Colour&, bool isMouseOver, bool isDown) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

private:
    ModulationLAF() = default;
};

// ── Jewel-style LED indicator ─────────────────────────────────────────────────
class JewelIndicator : public juce::Component {
public:
    bool isActive = false;
    void setActive(bool a) { isActive = a; repaint(); }
    void paint(juce::Graphics& g) override;
    void resized() override {}
};

// ── MixerLedButton (5F-4a) ───────────────────────────────────────────────────
// LED-style toggle button with a glowing colored dot and optional small label.
// Subclasses juce::Button directly so VibeLAF's filmstrip toggle path is
// bypassed - prevents the "stuck toggle switch" rendering on mixer strips AND
// the Effects page FX Bypass button. Works as a drop-in replacement for
// juce::TextButton in toggle mode, supports ButtonAttachment.
// ─────────────────────────────────────────────────────────────────────────────
class MixerLedButton : public juce::Button
{
public:
    explicit MixerLedButton(const juce::String& name = {}) : juce::Button(name) {}

    void setOnColour(juce::Colour c) { mOnColour = c; repaint(); }

    // QA-E Task 5 (2026-05-15): right-click callback so an LED can pair a
    // left-click toggle action with a separate right-click secondary action
    // (e.g. Arm LED: left-click toggles _arm, right-click opens input picker)
    // without subclassing.  Mirrors the pattern used for FilePickerButton in
    // BaySickNAMIREditor + pedal tile preset button in BaySickPedalsEditor.
    std::function<void()> onRightClick;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown() && onRightClick) { onRightClick(); return; }
        juce::Button::mouseDown (e);
    }

    void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
    {
        juce::ignoreUnused(isDown);
        auto b = getLocalBounds().toFloat().reduced(1.0f);

        // Recessed body
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(juce::Colour(isOver ? 0xff555555 : 0xff3a3a3a));
        g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);

        const bool  on      = getToggleState();
        const float dotR    = juce::jmin(b.getHeight(), b.getWidth()) * 0.28f;
        const float cx      = b.getCentreX();
        const bool  hasText = getButtonText().isNotEmpty();
        const float cy      = b.getCentreY() - (hasText ? dotR * 0.35f : 0.0f);

        if (on)
        {
            g.setColour(mOnColour.withAlpha(0.30f));
            g.fillEllipse(cx - dotR * 1.7f, cy - dotR * 1.7f, dotR * 3.4f, dotR * 3.4f);
            g.setColour(mOnColour);
            g.fillEllipse(cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.55f));
            g.fillEllipse(cx - dotR * 0.4f, cy - dotR * 0.55f,
                          dotR * 0.55f, dotR * 0.40f);
        }
        else
        {
            g.setColour(mOnColour.withAlpha(0.18f));
            g.fillEllipse(cx - dotR * 0.8f, cy - dotR * 0.8f,
                          dotR * 1.6f, dotR * 1.6f);
        }

        if (hasText)
        {
            g.setColour(on ? mOnColour.brighter(0.35f) : juce::Colour(0xff808080));
            g.setFont(juce::Font(8.5f, juce::Font::bold));
            auto textArea = juce::Rectangle<float>(b.getX(), cy + dotR + 1.0f,
                                                    b.getWidth(),
                                                    b.getBottom() - (cy + dotR + 1.0f));
            g.drawText(getButtonText(), textArea.toNearestInt(),
                       juce::Justification::centred);
        }
    }

private:
    juce::Colour mOnColour { juce::Colours::limegreen };
};

// ── HeadphonesLedButton (R4, 2026-04-23) ─────────────────────────────────────
// Drop-in MixerLedButton variant that paints a vector headphones glyph instead
// of a glowing dot.  Used as the Listen toggle on Vox / Inst strips so the
// user sees a recognizable icon rather than a generic LED.  Pure Path render -
// no font / PNG dependency.
// ─────────────────────────────────────────────────────────────────────────────
class HeadphonesLedButton : public MixerLedButton
{
public:
    explicit HeadphonesLedButton(const juce::String& name = {}) : MixerLedButton(name) {}

    void paintButton(juce::Graphics& g, bool isOver, bool /*isDown*/) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);

        // Recessed body (matches MixerLedButton)
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(juce::Colour(isOver ? 0xff555555 : 0xff3a3a3a));
        g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);

        const bool  on  = getToggleState();
        const auto  col = on ? mGlyphOn : mGlyphOff;

        // Centre the glyph (text label, if any, sits below)
        const bool  hasText = getButtonText().isNotEmpty();
        const float glyphH  = juce::jmin(b.getHeight() * (hasText ? 0.55f : 0.78f),
                                         b.getWidth()  * 0.78f);
        const float glyphW  = glyphH * 1.2f;
        const float cx      = b.getCentreX();
        const float cy      = b.getCentreY() - (hasText ? glyphH * 0.18f : 0.0f);

        // Headphones path: arched band + two cups.
        const float bandW   = glyphW * 0.95f;
        const float bandH   = glyphH * 0.55f;
        const float cupR    = glyphH * 0.26f;
        const float cupY    = cy + bandH * 0.20f;

        if (on)   // soft outer glow when active
        {
            g.setColour(col.withAlpha(0.25f));
            g.fillEllipse(cx - glyphW * 0.65f, cy - glyphH * 0.55f,
                           glyphW * 1.3f,       glyphH * 1.3f);
        }

        juce::Path band;
        band.addCentredArc(cx, cupY, bandW * 0.5f, bandH,
                           0.0f,
                           juce::MathConstants<float>::pi * -0.5f,
                           juce::MathConstants<float>::pi *  0.5f,
                           true);
        g.setColour(col);
        g.strokePath(band, juce::PathStrokeType(juce::jmax(1.5f, glyphH * 0.10f),
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        const float leftX  = cx - bandW * 0.5f;
        const float rightX = cx + bandW * 0.5f;
        g.fillEllipse(leftX  - cupR, cupY - cupR * 0.35f, cupR * 2.0f, cupR * 2.2f);
        g.fillEllipse(rightX - cupR, cupY - cupR * 0.35f, cupR * 2.0f, cupR * 2.2f);

        if (on)   // bright highlight inside each cup
        {
            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.fillEllipse(leftX  - cupR * 0.45f, cupY - cupR * 0.10f,
                          cupR * 0.55f, cupR * 0.45f);
            g.fillEllipse(rightX - cupR * 0.10f, cupY - cupR * 0.10f,
                          cupR * 0.55f, cupR * 0.45f);
        }

        if (hasText)
        {
            g.setColour(on ? col.brighter(0.35f) : juce::Colour(0xff808080));
            g.setFont(juce::Font(8.5f, juce::Font::bold));
            const float textY = cy + glyphH * 0.55f;
            g.drawText(getButtonText(),
                       juce::Rectangle<float>(b.getX(), textY,
                                              b.getWidth(),
                                              b.getBottom() - textY).toNearestInt(),
                       juce::Justification::centred);
        }
    }

    void setColours(juce::Colour offCol, juce::Colour onCol)
    {
        mGlyphOff = offCol; mGlyphOn = onCol; repaint();
    }

private:
    juce::Colour mGlyphOff { juce::Colour(0xff808080) };
    juce::Colour mGlyphOn  { juce::Colour(0xff33ff88) };   // green when listening
};

// ── Time-domain effect LookAndFeel (Delay / Reverb) ──────────────────────────
// Pultec-inspired: matte black fluted knobs, lever switches, Radio Gray panel.
class TimeLAF : public juce::LookAndFeel_V4 {
public:
    static TimeLAF& get() {
        static TimeLAF instance;
        return instance;
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float startAngle, float endAngle, juce::Slider& s) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float minSliderPos, float maxSliderPos,
        juce::Slider::SliderStyle style, juce::Slider& s) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    // Radio Gray panel background helper
    static void paintPultecPanel(juce::Graphics& g, juce::Rectangle<int> bounds);

private:
    TimeLAF() = default;
};

// ── Dynamics effect LookAndFeel (Compressor / Transient Shaper) ───────────────
// Set the "knobVariant" property on each Slider to select the draw style:
//   "modernAnalog"       - threshold/ratio knobs (dark dome + specular)
//   "dualLayerAluminum"  - gain/makeup knobs (knurled skirt + brushed cap)
//   "chickenHead"        - discrete selectors (beak pointer over hex-bolt base)
class DynamicsLAF : public juce::LookAndFeel_V4
{
public:
    static DynamicsLAF& get()
    {
        static DynamicsLAF instance;
        return instance;
    }

    // Property key set on Slider to select variant
    static const juce::Identifier kKnobVariant;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float startAngle, float endAngle, juce::Slider& s) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    // LA-2A style white/cream panel background - called from CompressorPanel and TransientShaperPanel paint()
    static void paintLA2APanel(juce::Graphics& g, juce::Rectangle<int> bounds);

private:
    DynamicsLAF() = default;

    void drawModernAnalog(juce::Graphics& g, juce::Rectangle<float> bounds,
                          float angle, bool isActive);
    void drawDualLayerAluminum(juce::Graphics& g, juce::Rectangle<float> bounds,
                               float angle, bool isActive);
    void drawChickenHead(juce::Graphics& g, juce::Point<float> centre,
                         float radius, float angle);
};

// ── Harmonic effect LookAndFeel (Saturation / Overdrive / Tape) ──────────────
// Fairchild Bakelite knobs + Olive Hammerite panel background.
class HarmonicLAF : public juce::LookAndFeel_V4
{
public:
    static HarmonicLAF& get() { static HarmonicLAF i; return i; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float startAngle, float endAngle, juce::Slider& s) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    static void paintHammeritePanel(juce::Graphics& g, juce::Rectangle<int> bounds);

private:
    HarmonicLAF() = default;
};

// ── Quadrant-colored page-navigation button ───────────────────────────────────
// Draws 4 colored quadrants (orange/blue/purple/pink) to represent the Layers page.
class QuadrantButton : public juce::TextButton
{
public:
    explicit QuadrantButton(const juce::String& label);
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
private:
    static const juce::Colour kQuads[4];
};

class ChickenHeadSelector;

// ── Global automation callback + right-click listener ────────────────────────
// sOnAutomate is set once by StandaloneEditor. Any component (VKnob slider,
// plain juce::Slider, etc.) with a non-empty componentID gets an
// "Automate: [id]" right-click menu via GlobalAutoRightClick.
namespace VKnobAutomation
{
    // Called when "Automate: X" is chosen from a right-click menu.
    extern std::function<void(const juce::String& paramId)> sOnAutomate;

    // Called when a panel/strip assigns a paramId to a control, to register a
    // playback applicator: given a 0..1 value, apply it to the live control.
    extern std::function<void(const juce::String& paramId, std::function<void(float)>)> sOnRegisterApplicator;

    // Called alongside sOnRegisterApplicator to register a value reader:
    // returns the current normalized 0..1 value of the control right now.
    extern std::function<void(const juce::String& paramId, std::function<float()>)> sOnRegisterReader;

    // Drives both hooks above for a plain slider whose value reaches its engine
    // through an existing attachment.  Maps 0..1 against the slider's range read
    // AT APPLY TIME, not a range captured here: the instrument editors register
    // during their componentID pass, and Harmless rebinds its Part A/B sliders
    // afterwards, so a captured range would freeze the pre-attachment default.
    // Ownership: the registry has no erase-on-destroy path, so these closures
    // outlive a closed tab -- the SafePointer inside makes a dead control a
    // no-op, and a rebuilt tab re-registers over the stale key.
    void registerSliderAutomation (const juce::String& paramId, juce::Slider& slider);

    // Button twin of the above: >= 0.5 is on.  Same SafePointer ownership rule.
    void registerButtonAutomation (const juce::String& paramId, juce::Button& button);

    // juce::ComboBox twin (ChickenHeadSelector has its own overload below).
    // 0..1 maps across item INDEX, so a lane sweep steps through the list.
    void registerComboAutomation (const juce::String& paramId, juce::ComboBox& combo);

    // Writes the PARAMETER instead of a control.  Required wherever one physical
    // control is time-shared between several params (Harmless Part A/B), since a
    // control-driven applicator would write whichever param is bound right now and
    // collapse independent lanes onto one target.  0..1 maps through the param's
    // own NormalisableRange, so skewed params behave like main-apvts lanes.
    // Ownership: `lifetimeGuard` must be a Component owned by the editor that owns
    // `param`'s processor -- engine editors are destroyed BEFORE their processor
    // (LayersPage::setEngine / dtor), so a live guard proves `param` is still valid.
    // `suppressWhen` (optional) vetoes the write while it returns true -- used by
    // the vocal capture lock, where a lane must not flip chain state mid-take for
    // the same reason the UI greys those controls out.  The lane is not consumed:
    // the next tick after the veto clears applies the current value normally.
    void registerParameterAutomation (const juce::String& paramId,
                                      juce::RangedAudioParameter& param,
                                      juce::Component& lifetimeGuard,
                                      std::function<bool()> suppressWhen = {});

    // Selector twin: 0..1 spreads across the option indices.  Option count is
    // read at apply time because panels call setOptions() after registration.
    void registerSelectorAutomation (const juce::String& paramId, ChickenHeadSelector& selector);

    // Translates a raw paramId into a user-facing label for the right-click menu
    // item ("Automate: <label>"). When null or returns empty, falls back to the
    // raw paramId. Wired by StandaloneEditor to use its display-name resolver so
    // the menu label matches the Event Editor title / Browser row / grid block.
    extern std::function<juce::String(const juce::String& paramId)> sResolveMenuLabel;

    // 2026-04-20 (S4 Batch 4): "Modulate envelope..." right-click item for
    // engines that expose a mod matrix (Harmless). When non-null AND returns
    // true for the given paramId, GlobalAutoRightClick adds a third menu
    // option that calls sOnModulateEnvelope(paramId) on click.
    // Multi-instance caveat: last-registered wins. A per-editor registry is
    // deferred to T3-ModMatrixAutomation.
    extern std::function<bool(const juce::String& paramId)> sShouldOfferModulate;
    extern std::function<void(const juce::String& paramId)> sOnModulateEnvelope;

    // I-3c (2026-05-02): MIDI Learn right-click items.  Wired by
    // StandaloneEditor's startup; null on plugin builds (legacy VST target).
    // sIsMidiMapped: returns true if `paramId` already has a mapping (used
    //   to gate the "MIDI Forget" menu item).
    // sIsMidiLearningTarget: returns true if `paramId` is the current learn
    //   target (VKnob uses this to draw the dashed-yellow learn outline).
    // sDescribeMidiMapping: returns short human-readable mapping summary
    //   ("USB Keyboard CC#74 ch1") for the "MIDI Forget" item label.
    // sOnMidiLearn / sOnMidiForget / sOnMidiSaveAsDefault: action triggers.
    // sHasAnyMidiMappings: gates the "Save as global default" item visibility.
    extern std::function<bool(const juce::String& paramId)>          sIsMidiMapped;
    extern std::function<bool(const juce::String& paramId)>          sIsMidiLearningTarget;
    extern std::function<juce::String(const juce::String& paramId)>  sDescribeMidiMapping;
    extern std::function<void(const juce::String& paramId)>          sOnMidiLearn;
    extern std::function<void(const juce::String& paramId)>          sOnMidiForget;
    extern std::function<void()>                                     sOnMidiSaveAsDefault;
    extern std::function<bool()>                                     sHasAnyMidiMappings;

    // Builds the MIDI Learn submenu items into `m`.  Shared between VKnob
    // and GlobalAutoRightClick; keeps the menu structure in one place.
    // Returns the highest reserved menu id (caller picks ids above this).
    int  appendMidiLearnMenuItems (juce::PopupMenu& m, const juce::String& paramId, int firstId);

    // Dispatch the click result for a MIDI Learn submenu item.  Returns
    // true if the result code was a MIDI Learn item; false otherwise so
    // callers can fall through to their own handlers.
    bool handleMidiLearnMenuResult (int result, int firstId, const juce::String& paramId);

    // Shared helper: prompt the user for a text value, parse it back through
    // the slider's `getValueFromText()` so units/scaling match the drag popup,
    // and `setValue(..., sendNotification)` (which auto-clamps to range).
    // Used by both VKnob's right-click menu and the GlobalAutoRightClick path.
    void promptSliderValueEntry(juce::Slider& slider, const juce::String& displayId);
}

// Attach one instance to the top-level window with wantsEventsForAllNestedChildComponents=true.
// Intercepts right-clicks on any component whose componentID is non-empty.
// Skips VKnob sliders (VKnob's own mouseDown handles those - they set "vknob_slider" property).
class GlobalAutoRightClick : public juce::MouseListener
{
public:
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!e.mods.isRightButtonDown()) return;

        auto* comp = e.eventComponent;
        if (!comp) return;

        // Skip VKnob's internal slider - identified by a property tag set in VKnob ctor.
        if ((bool)comp->getProperties()["vknob_slider"]) return;

        juce::String id = comp->getComponentID();
        if (id.isEmpty()) return;

        // Only offer "Type in value" for components that have a value (Sliders).
        auto* asSlider = dynamic_cast<juce::Slider*>(comp);
        juce::Component::SafePointer<juce::Slider> safeSlider(asSlider);

        // Resolve paramId -> friendly label for the menu text only; the backend
        // callback still receives the stable `id`.
        juce::String menuLabel;
        if (VKnobAutomation::sResolveMenuLabel)
            menuLabel = VKnobAutomation::sResolveMenuLabel(id);
        if (menuLabel.isEmpty()) menuLabel = id;

        juce::PopupMenu m;
        m.addItem(1, "Automate: " + menuLabel);
        if (asSlider != nullptr)
            m.addItem(2, "Type in value...");

        const bool offerModulate = (VKnobAutomation::sShouldOfferModulate
                                 && VKnobAutomation::sShouldOfferModulate(id));
        if (offerModulate)
            m.addItem(3, "Modulate envelope...");

        // I-3c (2026-05-02): MIDI Learn items shared between VKnob's own
        // mouseDown and this global handler.  IDs start at 100 to keep the
        // 1-99 range free for automation items above.
        constexpr int kMidiFirstId = 100;
        VKnobAutomation::appendMidiLearnMenuItems (m, id, kMidiFirstId);

        m.showMenuAsync(juce::PopupMenu::Options{}, [id, safeSlider, kMidiFirstId](int result)
        {
            if (result == 1 && VKnobAutomation::sOnAutomate)
                VKnobAutomation::sOnAutomate(id);
            else if (result == 2)
            {
                if (auto* s = safeSlider.getComponent())
                    VKnobAutomation::promptSliderValueEntry(*s, id);
            }
            else if (result == 3 && VKnobAutomation::sOnModulateEnvelope)
            {
                VKnobAutomation::sOnModulateEnvelope(id);
            }
            else
            {
                VKnobAutomation::handleMidiLearnMenuResult (result, kMidiFirstId, id);
            }
        });
    }
};

// ── VibeSlider ───────────────────────────────────────────────────────────────
// A juce::Slider that swallows right-click mouseDown / mouseDrag events so the
// slider value never changes on right-click. Left-click interaction is
// unchanged. Right-click still propagates up to the app-wide
// GlobalAutoRightClick mouse listener via JUCE's normal component event chain,
// so the "Automate..." + "Type in value..." popup still fires.
//
// Rationale: JUCE's default juce::Slider::mouseDown does NOT early-return on
// right-click unless setPopupMenuEnabled(true) is set (which would install
// JUCE's own Default/Set-value menu, competing with our custom Automate menu).
// Without that guard, right-click on a LinearVertical slider with snap-to-mouse
// enabled snaps the value to the Y of the click - unwanted whenever the user
// is trying to right-click to reach the Automate menu.
//
// Defined ahead of VKnob because VKnob holds one by value.
class VibeSlider : public juce::Slider
{
public:
    VibeSlider() = default;
    VibeSlider(SliderStyle style, TextEntryBoxPosition textPos = NoTextBox)
        : juce::Slider(style, textPos) {}

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;   // swallow right-click
        juce::Slider::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;
        juce::Slider::mouseDrag(e);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;
        juce::Slider::mouseUp(e);
    }
};

// ── Compact knob with label ───────────────────────────────────────────────────
class VKnob : public juce::Component,
              private juce::Slider::Listener
{
public:
    // VibeSlider so right-click never jogs the knob; the Automate menu still
    // fires because VKnob registers as a mouseListener here and
    // Component::internalMouseDown notifies listeners after the swallowed mouseDown.
    VibeSlider   slider;
    juce::Label  label;

    // Set this to enable the right-click "Automate" context menu.
    juce::String paramId;

    VKnob(const juce::String& lbl, float def, const juce::String& tip = {});
    ~VKnob() override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    // I-3c (2026-05-02): draw dashed-yellow outline when this knob is the
    // current MIDI Learn target.  Called after children paint so the outline
    // sits on top of the slider rendering.
    void paintOverChildren(juce::Graphics& g) override;

    // Soft lockout: knob visually greys out and ignores value-changing input,
    // but tooltips on hover (both on the body and the label) still work so users
    // can discover WHY the control is locked. Preferred over
    // setInterceptsMouseClicks(false) which also kills hover.
    void setLocked(bool lock);
    bool isLocked() const noexcept { return mLocked; }

    // Fired on drag start/end - use these to build undo actions.
    std::function<void(float beforeVal)>             onDragStarted;
    std::function<void(float beforeVal, float after)> onDragEnded;

private:
    float mValueBeforeDrag { 0.f };
    bool  mLocked { false };

    // Invisible lockout overlay. When lock is engaged it sits on top of the
    // slider, catches all mouse clicks/drags silently, and exposes the knob's
    // tooltip via TooltipClient so hovering still shows the help text.
    struct LockoutOverlay : public juce::Component, public juce::TooltipClient
    {
        juce::String* tip { nullptr };
        LockoutOverlay() { setInterceptsMouseClicks(true, false); }
        void mouseDown(const juce::MouseEvent&) override {}  // swallow
        void mouseDrag(const juce::MouseEvent&) override {}  // swallow
        juce::String getTooltip() override { return tip ? *tip : juce::String(); }
    };
    LockoutOverlay mLockoutOverlay;
    juce::String   mStoredTooltip;   // mirror of slider tip for overlay
    void sliderValueChanged(juce::Slider*)          override {}
    void sliderDragStarted (juce::Slider* s)        override;
    void sliderDragEnded   (juce::Slider* s)        override;
};

// ── ChickenHeadSelector ──────────────────────────────────────────────────────
// Discrete N-option rotary selector rendered with the shared Chicken Head
// filmstrip asset (66×66, 10 frames). Supports 2–10 options.
//
// Interaction:
//   • Rotary drag  - click anywhere on the knob body + drag to cycle options.
//   • Click a letter - snap directly to that option (faster than drag).
//
// Tooltips:
//   • Hovering a letter shows that option's own tooltip.
//   • Hovering the knob body shows the component-wide tooltip (via
//     setBodyTooltip()) - typically the combo's overall purpose.
//
// Caller registers onChange callback; the selected index is authoritative.
// Use for any discrete selector with >2 options on effect panels (LFO wave,
// delay model, phaser stages, reverb mode, etc.).
// ─────────────────────────────────────────────────────────────────────────────
class ChickenHeadSelector : public juce::Component,
                            public juce::TooltipClient
{
public:
    struct Option
    {
        juce::String letter;   // 1–2 char mark shown around the bezel
        juce::String label;    // full name (appears in hover tooltip title)
        juce::String tooltip;  // per-option help text
    };

    ChickenHeadSelector();
    ~ChickenHeadSelector() override = default;

    // Replace the full option list. Clamps current selection if out of range.
    void setOptions(const std::vector<Option>& opts);

    // Set/get current 0-based selection. notify=true fires onChange.
    void setSelectedIndex(int idx, juce::NotificationType notify = juce::sendNotification);
    int  getSelectedIndex() const noexcept { return mSelectedIdx; }
    int  getNumOptions()    const noexcept { return (int) mOptions.size(); }

    // Component-wide tooltip (shown when hovering body, not a letter).
    void setBodyTooltip(const juce::String& t) { mBodyTooltip = t; }

    // Soft lockout: selector visually greys out and ignores click/drag events,
    // but hover (tooltips, letter highlight) still works so users can read
    // WHY the control is locked. Preferred over setInterceptsMouseClicks(false)
    // which also kills hover/tooltip.
    void setLocked(bool lock);
    bool isLocked() const noexcept { return mLocked; }

    // Default colour for un-selected, un-hovered letters. Call from panel with
    // the colour that reads well against the panel's background - typically
    // white on dark/modulation/time/harmonic panels, black on cream dynamics
    // panels. Selected and hover colours are fixed (red palette) and apply
    // on top of this default regardless.
    void setDefaultLabelColour(juce::Colour c) { mDefaultTextColour = c; repaint(); }

    std::function<void(int)> onChange;

    // Component overrides
    void paint      (juce::Graphics&) override;
    void resized    () override;
    void mouseDown  (const juce::MouseEvent&) override;
    void mouseDrag  (const juce::MouseEvent&) override;
    void mouseMove  (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;

    // TooltipClient
    juce::String getTooltip() override;

private:
    std::vector<Option> mOptions;
    int   mSelectedIdx { 0 };
    int   mHoverLetter { -1 };
    float mDragStartAngle { 0.0f };
    int   mDragStartIdx   { 0 };
    bool  mIsDragging     { false };
    juce::String mBodyTooltip;
    // Default letter colour (overridable by panel). Selected + hover are fixed.
    juce::Colour mDefaultTextColour { juce::Colours::white };
    bool  mLocked         { false };

    // Arc the N positions span on the dial (degrees; 0 = 12 o'clock, +CW).
    static constexpr float kStartAngleDeg = -135.0f;  // 7 o'clock
    static constexpr float kEndAngleDeg   =  135.0f;  // 5 o'clock

    juce::Rectangle<float> getKnobBounds() const noexcept;
    float                  angleForIndex(int idx) const noexcept;
    juce::Point<float>     letterCentre(int idx) const noexcept;
    int                    hitLetter(juce::Point<float> p) const noexcept;
    int                    indexFromAngle(float angleRad) const noexcept;
};

// ── DualLabelToggle ──────────────────────────────────────────────────────────
// Composite widget: two or three labels + physical switch. Replaces
// LabeledToggle going forward.
//
// Two setup modes:
//
//   setupNamed(topLbl, topTip, botLbl, botTip)
//     Layout:  topLbl  ← tooltip = topTip
//                sw
//              botLbl  ← tooltip = botTip
//     Switch up = false (top label active), down = true (bottom label active).
//     Clicking a label sets the state to match.
//
//   setupOnOff(featureName, featureTip)
//     Layout:  featureName  ← tooltip = featureTip (only tooltip in on/off mode)
//                 OFF
//                 sw
//                 ON
//     Switch up = false (OFF), down = true (ON). Clicking OFF/ON sets state.
//
// The underlying juce::ToggleButton is accessible via btn() and uses the
// VibeLAF switch_toggle filmstrip regardless of the panel's current LAF.
//
// Right-click never flips the switch (button + label paths both swallow it):
// it is reserved for the GlobalAutoRightClick "Automate" menu, which fires
// via the app-wide mouse listener regardless of the swallow -- same
// rationale as VibeSlider.  The menu appears only on toggles a panel has
// registered via EditorPanelBase::addAutomatableToggle (componentID set).
// ─────────────────────────────────────────────────────────────────────────────
class DualLabelToggle : public juce::Component
{
public:
    enum class Mode { Unset, Named, OnOff };

    DualLabelToggle();
    ~DualLabelToggle() override;

    void setupNamed(const juce::String& topLabel,    const juce::String& topTooltip,
                    const juce::String& bottomLabel, const juce::String& bottomTooltip);
    void setupOnOff(const juce::String& featureName, const juce::String& featureTooltip);

    juce::ToggleButton& btn() { return mBtn; }

    // Optional colour override for label text (e.g. dark panels).
    void setLabelColour(juce::Colour c);

    void resized() override;
    // Clicks on the state labels set the switch directly.
    void mouseUp(const juce::MouseEvent& e) override;

private:
    Mode mMode { Mode::Unset };
    // Labels used per mode:
    //   Named : mTop (top), mBot (bottom)
    //   OnOff : mTop (feature name), mMidTop ("OFF"), mMidBot ("ON")
    juce::Label        mTop, mMidTop, mMidBot, mBot;

    // ToggleButton that ignores right-click (see class comment) -- still a
    // juce::ToggleButton to callers via btn().
    struct SwitchButton : public juce::ToggleButton
    {
        void mouseDown (const juce::MouseEvent& e) override
        { if (! e.mods.isPopupMenu()) juce::ToggleButton::mouseDown (e); }
        void mouseUp   (const juce::MouseEvent& e) override
        { if (! e.mods.isPopupMenu()) juce::ToggleButton::mouseUp   (e); }
    };
    SwitchButton mBtn;
};

// ── Fader with 0 dB snap ─────────────────────────────────────────────────────
// LinearVertical fader that snaps to exactly 0.0 dB when dragged within ±1.5 dB.
// SnapSlider inherits VibeSlider so the mixer fader gets the right-click
// swallow behaviour for free.
class SnapSlider : public VibeSlider
{
public:
    SnapSlider() : VibeSlider(juce::Slider::LinearVertical, juce::Slider::NoTextBox) {}
    double snapValue(double v, DragMode) override
    {
        return (std::abs(v) < 1.5) ? 0.0 : v;
    }
};

// ── Basic Sequence step cell ──────────────────────────────────────────────────
// One cell in the on-page basic sequence grid.
// Click = toggle, vertical drag = velocity, horizontal drag = length.
class BasicStepCell : public juce::Component
{
public:
    int   rowIndex  { 0 };
    int   stepIndex { 0 };
    bool  active    { false };
    float velocity  { 0.8f };  // 0-1
    float length    { 1.0f };  // 1.0 = full step

    std::function<void(int row, int step, bool active, float vel, float len)> onChange;

    BasicStepCell();
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp  (const juce::MouseEvent&) override;

    void setRowColour(juce::Colour c) { mRowColour = c; repaint(); }

private:
    juce::Colour mRowColour { VC::Highlight };
    juce::Point<float> mDragStart;
    float mDragStartVel { 0.8f };
    float mDragStartLen { 1.0f };
    bool  mDragging     { false };
};

// ── Basic Sequence AHDSR envelope display + controls ─────────────────────────
class BasicEnvelopeEditor : public juce::Component
{
public:
    BasicEnvelope env;
    std::function<void(const BasicEnvelope&)> onChange;

    BasicEnvelopeEditor();
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    std::unique_ptr<VKnob> mAttack, mHold, mDecay, mSustain, mRelease;
    void updateFromKnobs();
    void drawCurve(juce::Graphics& g, juce::Rectangle<int> area);
};

// ── Sequence routing bar (Basic / Complex dropdown + Go button) ───────────────
class SeqRoutingBar : public juce::Component
{
public:
    std::function<void(SeqRouting)> onRoutingChanged;
    std::function<void()>           onGoToComplex;

    SeqRoutingBar();
    void setRouting(SeqRouting r);
    SeqRouting getRouting() const;
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    std::unique_ptr<juce::ComboBox>   mRoutingBox;
    std::unique_ptr<juce::TextButton> mGoBtn;
    SeqRouting mRouting { SeqRouting::BasicSequence };
    void updateVisibility();
};

// ── FX chain strip (6 slots: On toggle + type label + 3 VKnobs each) ─────────
class FXChainStrip : public juce::Component
{
public:
    // slotLabels: display names for each slot ("Comp", "Dist", etc.)
    FXChainStrip(const juce::StringArray& slotLabels = {});
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    struct Slot {
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::Label>        label;
        std::unique_ptr<VKnob>              k1, k2, k3;
    };
    std::vector<Slot> mSlots;
};

// ── Waveform display with start/end markers ───────────────────────────────────
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();
    void setWaveform(const std::vector<float>& samples);
    void setColor(juce::Colour c);
    void setCurrentRate(float rate) { mBodyDragStartRate = rate; }  // keep in sync for drag origin

    // Called when user drags start/end markers (values 0.0-1.0)
    std::function<void(float startPos, float endPos)> onMarkersChanged;
    // Called when user drags the waveform body up/down (maps to LFO rate 0.01-20Hz)
    std::function<void(float rate)> onSpeedChanged;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp  (const juce::MouseEvent&) override;

private:
    std::vector<float> mSamples;
    juce::Colour       mColor { VC::Highlight };
    float mStartPos { 0.0f };
    float mEndPos   { 1.0f };
    enum class DragTarget { None, StartMarker, EndMarker, Body } mDrag { DragTarget::None };
    float mBodyDragStartY    { 0.f };
    float mBodyDragStartRate { 1.f };  // LFO rate at drag start

    juce::Path buildCurvePath(juce::Rectangle<float> area) const;
    DragTarget hitTestMarker(juce::Point<float> p) const;
    float markerX(float pos, float w) const { return pos * w; }
};

// Forward declarations for DSP binding modes (defined in DSP/)
class EQ8DSP;
class EQ8MsDSP;

// ── Interactive 8-band parametric EQ display ──────────────────────────────────
// Supports three binding modes:
//   1. APVTS mode (Layers page): bindAPVTS()
//   2. DSP-direct mode (Effects page): bindDSP()
//   3. M/S DSP mode (Effects page): bindMsDSP()  - adds Mid/Side toggle pill
class ParametricEQDisplay : public juce::Component,
                             public juce::TooltipClient
{
public:
    // 0=Bell 1=LP 2=HP 3=LowShelf 4=HiShelf 5=Notch 6=OFF 7=BandPass 8=Tilt
    // slope: 0=Center-2, 1=Steep-4, 2=Steep-6, 3=Steep-8,
    //        4=Gentle-4, 5=Gentle-6, 6=Gentle-8
    // channel (12h): 0=Stereo 1=Mid 2=Side 3=LOnly 4=ROnly
    struct Band {
        float freq    { 1000.f };
        float gainDb  { 0.f };
        float q       { 0.707f };
        int   type    { 0 };
        int   slope   { 0 };
        bool  enabled { true };
        bool  muted   { false };
        bool  soloed  { false };
        int   channel { 0 };
        // 12j dynamic EQ mirror (widget-side setpoints + live GR readout).
        bool  dynamic   { false };
        float threshold { -18.f };
        float ratio     { 2.f };
        float attack    { 10.f };
        float release   { 100.f };
        // C.4 follow-up (2026-04-30): default 0 so the dotted ghost curve sits
        // flat (no modulation) when the user first toggles Dynamic on.  Was
        // +12 (legacy from the pre-bipolar era when 12 = max upward + a
        // separate Upward bool); the bipolar redesign repurposed +12 to mean
        // "12 dB upward expansion" which the user didn't ask for.  All three
        // tiers (this UI Band, EQ8DSP::Band, APVTS Range param) now default
        // to 0 so they don't fight each other on first use.
        float rangeDb   { 0.f };
        bool  upward    { false };
        int   scSourceId{ -1 };       // Option B scaffolding, not user-editable yet
        // Live gain reduction in dB, polled from DSP::getBandGrDb each timer tick.
        // Signed: negative = downward compression, positive = upward expansion.
        float currentGrDb { 0.f };
    };

    ParametricEQDisplay();

    void setBand(int idx, const Band& b);
    Band getBand(int idx) const;

    // ── Spectrum analyser ──────────────────────────────────────────────────
    // 12i: two feeds per EQ instance.
    //   pushSamples    -> POST-EQ spectrum  (existing green/yellow overlay)
    //   pushSamplesPre -> PRE-EQ  spectrum  (new translucent-grey overlay behind)
    // When bound via bindMsDSP, syncFromDSP() polls mBoundMsDsp->preFeed and
    // ->postFeed from the widget's own timer and routes them to these two paths.
    // External callers (pages) may still push directly - both entry points work.
    void pushSamples    (const float* data, int numSamples);
    void pushSamplesPre (const float* data, int numSamples);
    void setSampleRate(double sr) { mSampleRateForFFT = sr; }

    // ── Binding modes ──────────────────────────────────────────────────────
    // Mode 1: APVTS binding (Layers page)
    void bindAPVTS (juce::AudioProcessorValueTreeState& apvts, int layerIdx);
    void syncFromAPVTS();   // poll from LayersPage timerCallback

    // Mode 2: DSP-direct (Effects page single EQ)
    void bindDSP   (EQ8DSP* dsp);

    // Mode 3: M/S DSP (Drums/Bass EQ - Mid/Side toggle pill shown)
    // Basic bind - no APVTS write-back (UI changes won't survive processBlock override)
    void bindMsDSP (EQ8MsDSP* msDsp);
    // Full bind with APVTS write-back so processBlock updateXxxEQ() doesn't revert UI edits
    void bindMsDSP (EQ8MsDSP* msDsp, juce::AudioProcessorValueTreeState* apvts,
                    juce::String midPrefix, juce::String sidePrefix);

    // C.4 Phase 1 (2026-04-30): the strip the EQ is on -- used by the
    // DynamicParamsPopout's SC source dropdown to enumerate routed SC lines.
    // mixerPrefix should be the strip's mixer APVTS prefix (e.g.
    // "mixer_layer_0").  resolveSourceName maps a source channel id to a
    // friendly label ("Layer 1" / "Bass 1" / "Master" / etc.).  Optional --
    // when unset the popout shows "(no sidechain context)" disabled item.
    void setStripContext (juce::String mixerPrefix,
                           std::function<juce::String(int)> resolveSourceName);

    // Periodically called (from timer) to pull values from bound DSP instance
    void syncFromDSP();

    // ── M/S view control ──────────────────────────────────────────────────
    // Called externally (e.g. from EffectsPage/DrumsPage header buttons)
    void setShowMid(bool showMid);
    bool isShowingMid() const { return mShowMid; }

    // ── Compare banks ──────────────────────────────────────────────────────
    void triggerCompare();   // calls EQ8DSP::swapWithSpare() or swaps mBands/mSpare
    void triggerLock();      // calls EQ8DSP::lockSpare() or toggles mSpareLocked

    // ── EQ options popup ───────────────────────────────────────────────────
    // 2026-04-19: now wired into PageMenuBar::setMenuBuilder per the universal
    // page-actions convention. The in-display "..." button is hidden; pages
    // install the EQ menu into the page's hamburger ≡ on EQ-tab activation.
    void showEQOptionsMenu(juce::Component* anchor = nullptr);
    void installPageMenu  (PageMenuBar& bar);   // sets bar's menu builder to ours
    void uninstallPageMenu(PageMenuBar& bar);   // clears bar's menu builder

    // 2026-04-19: A/B compare overhaul. New explicit "Copy A -> B" action
    // (compare button no longer auto-saves; pure swap). Bank indicator
    // component shows current bank ("A Bank" green / "B Bank" red), lives
    // in PageMenuBar's extra-right slot via getBankIndicator() injection.
    // Click on the indicator triggers compare swap (convenience shortcut).
    void triggerCopyAToB();
    juce::Component* getBankIndicator();   // owned by EQ display; non-owning ptr
    void refreshBankIndicator();           // updates label + colour after swap

private:
    // 2026-04-19: A/B compare APVTS sync. After a swap, push the inner EQ's
    // current DSP-side band values into the corresponding APVTS prefix so
    // processBlock::updateXxxEQ doesn't overwrite the swap with the old
    // bank's stale APVTS values. Walks all 8 bands + all per-band suffixes
    // (Freq/Gain/Q/Type/On/Slope/Mute/Solo/Channel + 12j dynamic + ScSource).
    void pushInnerDSPBandsToAPVTS (class EQ8DSP& innerEq,
                                   const juce::String& apvtsPrefix);
public:

    // ── M/S display toggle (called by parent to force visibility of internal pill) ─
    void showMidSideToggle(bool show);   // force MID/SIDE pill to be visible/hidden

    // Callback for band drag changes
    std::function<void(int bandIdx, float newFreq, float newGainDb)> onBandChanged;

    // 12f: invoked after the user toggles anti-cramping in the options popup.
    // Pages bind this to refresh the host's PDC (mProcessor.setLatencySamples
    // (mProcessor.mVibeGraph.updateBusLatencies())). Optional - APVTS-bound
    // displays without a DSP attached leave it empty and the toggle is hidden.
    std::function<void()> onLatencyChanged;

    void paint         (juce::Graphics&) override;
    void resized       () override;
    void mouseDown     (const juce::MouseEvent&) override;
    void mouseDrag     (const juce::MouseEvent&) override;
    void mouseUp       (const juce::MouseEvent&) override;
    void mouseMove     (const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Session B: dynamic hover tooltip showing the hovered band's full settings
    // (type, freq, gain, Q, channel routing, enable/mute/solo state).
    juce::String getTooltip() override;

private:
    static constexpr int kNumBands         = 8;
    static constexpr int kFFTOrder         = 10;   // 1024-point FFT
    static constexpr int kFFTSize          = 1 << kFFTOrder;
    static constexpr int kNumHeatmapFrames = 48;   // circular buffer depth

    std::array<Band, kNumBands> mBands;
    std::array<Band, kNumBands> mSpareBands;
    bool mSpareLocked   { false };
    bool mViewingSpare  { false };
    bool mHasSpare      { false };

    // 2026-04-19: bank indicator component (owned). Lives in PageMenuBar's
    // extra-right slot via getBankIndicator() injection. Shows "A Bank" green
    // or "B Bank" red. Click swaps banks (alias for triggerCompare).
    class BankIndicator : public juce::Component,
                          public juce::SettableTooltipClient
    {
    public:
        BankIndicator (ParametricEQDisplay& owner) : mOwner (owner) {}
        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool mViewingSpare { false };
    private:
        ParametricEQDisplay& mOwner;
    };
    std::unique_ptr<BankIndicator> mBankIndicator;

    int  mDragBand      { -1 };
    bool mUserDragging  { false };
    bool mFineAdjust    { false };   // Ctrl held → 0.1× sensitivity
    mutable int mHoveredBand { -1 };
    juce::Point<float> mDragOrigin;
    float mDragStartFreq { 0.f }, mDragStartGain { 0.f };

    juce::Rectangle<int> mGraphArea;
    juce::Rectangle<int> mRightPanelArea;
    juce::Rectangle<int> mToolbarArea;

    struct BandControl {
        std::unique_ptr<juce::ComboBox>     typeCombo;  // dropdown for band filter type
        // 2026-04-19: VibeSlider swallows right-click so it doesn't steal focus
        // from the Automate menu popup. Polymorphic via juce::Slider base pointer
        // so existing code that takes juce::Slider* signatures still works.
        std::unique_ptr<VibeSlider>         gainFader;  // bipolar vertical -18..+18 dB
        std::unique_ptr<VibeSlider>         freqKnob;   // rotary 20..20000 Hz
        std::unique_ptr<VibeSlider>         qKnob;      // rotary 0.1..10
        // enableBtn removed - click the colored dot at the top of each column to toggle on/off
    };
    std::array<BandControl, kNumBands> mControls;

    // 12i / EQ polish: small text-readout strips placed immediately below each
    // gain fader / freq knob / Q knob so users can see the numeric value at a
    // glance. Rectangles are set in resized(), rendered in paint().
    std::array<juce::Rectangle<int>, kNumBands> mGainReadoutR {};
    std::array<juce::Rectangle<int>, kNumBands> mFreqReadoutR {};
    std::array<juce::Rectangle<int>, kNumBands> mQReadoutR    {};

    // D.4-Q6 (2026-05-01): EQ8 main-level output fader, surfaced as a 9th
    // vertical fader on the right of the band column area.  -18..+18 dB.
    // Bound to EQ8DSP::setMainLevel (or both Mid+Side in MsDSP mode).
    std::unique_ptr<VibeSlider> mMainLevelFader;
    juce::Rectangle<int>        mMainReadoutR {};

    // 12j follow-up Q1: shared inline TextEditor for readout editing. Positioned
    // at whichever readout rect the user double-clicked; pre-filled with current
    // value, select-all'd, grab-focus. Enter / Escape / focus-loss commits or
    // cancels. Single child (not 24) to keep component count low.
    enum class ReadoutEditKind { None, Gain, Freq, Q };
    std::unique_ptr<juce::TextEditor> mReadoutEditor;
    ReadoutEditKind mReadoutEditKind { ReadoutEditKind::None };
    int             mReadoutEditBand { -1 };

    // ── Toolbar UI elements ────────────────────────────────────────────────
    std::unique_ptr<juce::TextButton> mOptionsBtn;   // "..." opens EQ options popup menu
    std::unique_ptr<juce::TextButton> mMidSideBtn;   // MID/SIDE toggle pill (always shown in M/S mode)
    // State tracked for options menu items
    bool mShowPhase     { false };
    bool mHeatmapEnabled{ false };
    bool mSpareLocked_btn { false }; // shadow for lock state (replaces mLockBtn->getToggleState())
    int  mPhaseMode     { 0 };       // 0=STD 1=LIN 2=HQ+ 3=HQL 4=HQE

    // ── FFT spectrum ───────────────────────────────────────────────────────
    // mFFT is shared between pre- and post-EQ paths (UI thread is single-threaded
    // so sequential calls are safe).
    juce::dsp::FFT                         mFFT;
    std::array<float, kFFTSize>            mFifoBuffer {};
    std::array<float, kFFTSize * 2>        mFFTData    {};
    std::array<float, kFFTSize / 2>        mSpectrumDb {};
    int                                    mFifoIndex       { 0 };
    bool                                   mSpectrumReady   { false };
    double                                 mSampleRateForFFT{ 44100.0 };
    // 12i: pre-EQ spectrum storage (parallel to post-EQ above).
    std::array<float, kFFTSize>            mFifoBufferPre {};
    std::array<float, kFFTSize / 2>        mSpectrumDbPre {};
    int                                    mFifoIndexPre       { 0 };
    bool                                   mSpectrumPreReady   { false };
    // Scratch polled from EQ8MsDSP::preFeed / postFeed in syncFromDSP (UI thread only).
    float                                  mFeedPollBuf[SpectrumFeed::kSize] {};

    // ── Heatmap circular buffer ────────────────────────────────────────────
    using HeatmapFrame = std::array<float, kFFTSize / 2>;
    std::array<HeatmapFrame, kNumHeatmapFrames> mHeatmapFrames;
    int  mHeatmapWritePos { 0 };
    bool mHeatmapHasData  { false };

    // ── Binding state ──────────────────────────────────────────────────────
    enum class BindMode { None, APVTS, DSP, MsDSP };
    BindMode mBindMode { BindMode::None };

    juce::AudioProcessorValueTreeState* mAPVTS     { nullptr };
    int                                 mLayerIdx  { -1 };
    EQ8DSP*                             mBoundDSP  { nullptr };
    EQ8MsDSP*                           mBoundMsDsp{ nullptr };
    bool                                mShowMid   { true };   // M/S view toggle

    // APVTS write-back for MsDSP mode (prevents processBlock from reverting UI edits)
    juce::AudioProcessorValueTreeState* mMsDSPApvts     { nullptr };
    juce::String                        mMsDSPMidPrefix;
    juce::String                        mMsDSPSidePrefix;

    // C.4 Phase 1 (2026-04-30): strip context for the SC dropdown in
    // DynamicParamsPopout.  Set externally via setStripContext.
    juce::String                                mStripMixerPrefix;
    std::function<juce::String(int)>            mResolveSourceName;

    void setAPVTSFromBand(int b);
    void pushBandToDSP   (int b);

    // Session B: register VKnobAutomation applicator + reader for every EQ band
    // paramId under the currently-bound mid/side prefixes so right-click
    // "Automate: ..." on a band handle wires the Event Editor to APVTS. Idempotent
    // (re-registering the same paramId just overwrites the lambda).
    void registerAutomationForBoundEQ();

    // 12j: opens a CallOutBox popout with Threshold / Ratio / Attack / Release /
    // Range sliders + Upward toggle + live GR meter for the given band index.
    // Requires full bindMsDSP with APVTS prefix (else the knobs have no APVTS
    // attachment target). Knobs are componentID-tagged so GlobalAutoRightClick's
    // "Automate: ..." / "Type in value..." menus work on them.
    void openDynamicParamsPopout(int bandIdx);

    // 12j follow-up Q1: readout-edit helpers. beginReadoutEdit shows the shared
    // TextEditor positioned at the given rect, pre-filled with the formatted
    // current value. commitReadoutEdit parses the entered text + applies via
    // setBand*. cancelReadoutEdit just hides without applying.
    void beginReadoutEdit(int band, ReadoutEditKind kind);
    void commitReadoutEdit();
    void cancelReadoutEdit();
    // Bonus Q3: stamp componentIDs on per-band right-panel controls so the app-
    // wide GlobalAutoRightClick can catch right-clicks on them and offer the
    // "Automate: ..." menu. Called from bindMsDSP() and when mShowMid flips so
    // the paramIds track the currently-viewed (mid or side) prefix.
    void stampRightPanelComponentIds();

    // ── Drawing helpers ────────────────────────────────────────────────────
    float freqToX(float hz) const;
    float gainToY(float db) const;
    float xToFreq(float x)  const;
    float yToGain(float y)  const;
    // 12j: gainOverride lets drawCurve call evalBandDb with the range-endpoint
    // gain for the ghost outline. Default NaN => use effective gain (design gain
    // + current GR when dynamic=true, else design gain).
    float evalBandDb (int band, float hz,
                      float gainOverride = std::numeric_limits<float>::quiet_NaN()) const;
    float evalPhaseRad(int band, float hz) const;

    void drawGrid      (juce::Graphics&) const;
    void drawSpectrum  (juce::Graphics&) const;
    // 12j Issue 3: rich 3-column hover panel drawn in-paint when a band is
    // hovered. Columns: [band info] [dynamic params] [graphical GR meter].
    // Replaces the JUCE TooltipClient text tooltip (getTooltip() returns "").
    void drawHoverTooltip(juce::Graphics&) const;
    void drawHeatmap   (juce::Graphics&) const;
    void drawPhaseCurve(juce::Graphics&) const;
    void drawCurve     (juce::Graphics&) const;
    void drawHandles   (juce::Graphics&) const;
    void drawToolbar   (juce::Graphics&) const;

    void syncControlsFromBands();
    void syncBandFromControl(int idx);

    bool mSyncing { false };
};

// ── VU Meter (horizontal or vertical level / GR display) ─────────────────────
class VUMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Style { Horizontal, Vertical };

    VUMeter(Style style = Horizontal);
    ~VUMeter() override = default;

    void setLevel(float rms01);          // 0–1 normalised (maps to -60..0 dBFS internally)
    void setGainReduction(float grDb);   // negative dB value for GR overlay

    void paint(juce::Graphics&) override;

    // 2026-05-02: vblank-locked refresh.  Replaces the 60 Hz Timer so the
    // spring-damper ballistics integrate against monitor-refresh deltas
    // instead of message-thread coalesced 60 Hz ticks.
    void parentHierarchyChanged() override;

private:
    Style mStyle;

    // ── Horizontal (legacy bar) state ──────────────────────────────────────
    std::atomic<float> mLevel { 0.f };
    std::atomic<float> mPeak  { 0.f };
    std::atomic<float> mGR    { 0.f };
    int   mPeakHoldFrames { 0 };

    // ── Vertical (hardware VU recreation) state ────────────────────────────
    // mDisplayLevel is in VU dB (-20 to +3), driven by second-order ballistics
    // mPeakDb is the highest instantaneous VU dB seen (for MAX box)
    std::atomic<float> mLevelRms01 { 0.f };   // written by setLevel(), read by timer
    float mDisplayLevel { -20.f };             // smoothed display value in VU dB
    float mPeakDb       { -20.f };             // running peak for MAX display
    int   mPeakMaxHoldFrames { 0 };            // frames to hold MAX indicator

    static constexpr int kPeakHoldMs = 1500;
    static constexpr int kTimerHz    = 60;     // 60fps for smooth needle
    static constexpr float kFallRate = 0.05f;  // per timer tick (horizontal mode)
    static constexpr float kDecayDbPerSec = 20.f; // MAX peak decay rate

    // VU scale constants
    static constexpr float kVuMin = -20.f;     // leftmost arc position
    static constexpr float kVuMax =  +3.f;     // rightmost arc position

    // ── Second-order needle dynamics ───────────────────────────────────────
    float mVelocity   { 0.f };   // needle velocity for second-order dynamics
    float mCurrentPos { 0.f };   // normalized 0..1 position (maps to VU scale)

    // 2026-05-02: wall-clock vblank timestamp for delta-time-based decay --
    // ballistics stay consistent across 60/120/144 Hz monitors.
    double mLastVBlankMs { 0.0 };

    void onVBlank();
    std::unique_ptr<juce::VBlankAttachment> mVBlank;

    // ── Drawing helpers ────────────────────────────────────────────────────
    void paintHorizontal(juce::Graphics&);
    void paintVerticalVU(juce::Graphics&);

public:
    // ── Global VU calibration ─────────────────────────────────────────────
    // dBFS level that maps to 0 VU. Default -18. Range -18 to -14.
    // Stored as negative float (e.g. -18.f = -18 dBFS = 0 VU).
    // 2026-05-05: persists in project files via <VUCalibration> in
    // serializeUIState / deserializeUIState (-18 default for fresh projects).
    static float sCalibrationDb;
    // 2026-05-05 dirty-flag wiring: fired on every successful change
    // (no-fire on no-op when the value already matches).  StandaloneEditor
    // wires this to ProjectManager::markDirty so menu changes flip the
    // project dirty bit - without it, VU calibration edits would only
    // persist if the user explicitly saved before exiting.
    static std::function<void()> sOnCalibrationChanged;
    static float getCalibrationDb()      { return sCalibrationDb; }
    static void  setCalibrationDb(float db)
    {
        const float clamped = juce::jlimit(-18.f, -14.f, db);
        if (sCalibrationDb == clamped) return;
        sCalibrationDb = clamped;
        if (sOnCalibrationChanged) sOnCalibrationChanged();
    }
};

// ── GR Meter (SSL-style analog gain-reduction indicator) ─────────────────────
// Round analog-face meter. Needle at rest points right (0 dB = no reduction).
// Swings LEFT as compression deepens. Scale: 0 → -5 → -10 → -15 → -20 dB.
// Cream face, red zone from -15..-20, small LCD readout below arc.
class GRMeter : public juce::Component,
                public juce::SettableTooltipClient,
                private juce::Timer
{
public:
    GRMeter();
    ~GRMeter() override { stopTimer(); }

    // Push live GR value in dB (0 = no reduction, negative = compressing).
    // Thread-safe; store-only on the writer side.
    void setGainReduction(float grDb);

    void paint(juce::Graphics&) override;

private:
    std::atomic<float> mTargetDb  { 0.0f };
    float              mDisplayDb { 0.0f };   // smoothed needle position (dB)

    void timerCallback() override;

    // Scale constants
    static constexpr float kMinDb       = -20.0f;  // leftmost position
    static constexpr float kMaxDb       =   0.0f;  // rightmost position (rest)
    static constexpr float kRedDb       = -15.0f;  // red zone starts here
    static constexpr int   kTimerHz     = 60;
    // Needle smoothing: first-order lerp coefficient per timer tick
    static constexpr float kSmoothAlpha = 0.35f;
};

// ── GateGRMeter ──────────────────────────────────────────────────────────────
// QA-Fe2 (2026-07-16): gate-specific sibling of GRMeter.  A gate's attenuation
// legitimately swings to -80 dB (closed is its RESTING state, not a warning),
// so the compressor meter's 0..-20 scale pins and its deep-end red zone reads
// backwards.  Same chassis (chrome bezel / cream plate / needle / LCD), but:
// scale 0..-80, and the red zone sits at the 0 end (owner call: red toward 0,
// never at the closed end).
class GateGRMeter : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    GateGRMeter();
    ~GateGRMeter() override { stopTimer(); }

    // Push live attenuation in dB (0 = open / passing, negative = gating).
    // Thread-safe; store-only on the writer side.
    void setGainReduction(float grDb);

    void paint(juce::Graphics&) override;

private:
    std::atomic<float> mTargetDb  { 0.0f };
    float              mDisplayDb { 0.0f };

    void timerCallback() override;

    static constexpr float kMinDb       = -80.0f;  // leftmost (fully closed)
    static constexpr float kMaxDb       =   0.0f;  // rightmost (open, rest)
    static constexpr float kRedDb       =  -6.0f;  // red zone: kRedDb..0 (open end)
    static constexpr int   kTimerHz     = 60;
    static constexpr float kSmoothAlpha = 0.35f;
};

// ── ColoredSectionLAF ────────────────────────────────────────────────────────
// LookAndFeel override for ComboBox popup menus that draws section headings
// as colored horizontal lines. Encode the color in the heading string using
// the prefix "\xc2\xa7#RRGGBB\xc2\xa7" - the LAF strips this prefix, parses the color, and
// draws a 2 px glowing horizontal line in that color with the remaining text
// below in bold.
//
// Usage:
//   combo.addSectionHeading(ColoredSectionLAF::encode(juce::Colour(0xffce3f8e), "FX BUS"));
// Pair with `combo.setLookAndFeel(&ColoredSectionLAF::get())`.
class ColoredSectionLAF : public juce::LookAndFeel_V4
{
public:
    static ColoredSectionLAF& get();

    // Format a section heading string with color prefix.
    static juce::String encode(juce::Colour c, const juce::String& title);

    // Extract the color + title from an encoded heading. Returns false if no prefix.
    static bool decode(const juce::String& s, juce::Colour& outColor, juce::String& outTitle);

    void drawPopupMenuSectionHeader(juce::Graphics& g,
                                    const juce::Rectangle<int>& area,
                                    const juce::String& sectionName) override;

    // Ensure the combo box popup is never clipped to its parent viewport.
    // Targets the full display and permits 1..3 columns so very tall menus
    // wrap instead of showing scroll arrows.
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(
        juce::ComboBox& box, juce::Label& label) override;

private:
    ColoredSectionLAF() = default;
};

// ── Digital peak meter (dBFS scale, green→yellow→red, peak hold) ─────────────
// 2026-04-30: rewritten for stereo L/R + FL-parity range/scale.
//  - Range: -60 dBFS .. +6 dBFS (headroom above 0 visible like FL).
//  - Mapping: piecewise linear log-style - top 30 % covers -18..+6, bottom 70 %
//    covers -60..-18 (compressed) so the dB range that matters takes the most
//    pixels.
//  - Inside tick labels: drawn over unlit segments, naturally covered when lit
//    segments paint on top.  Idle meter shows full scale; loud meter hides
//    labels under the lit fill except above the current level.
//  - Stereo split (single bar, two halves filled separately).  Mono callers
//    use setLevel(); strip callers now use setStereoLevel().
//  - setCompact() kept as a no-op for back-compat (legacy callers may still
//    call it; the FL-parity range above is now the only mode).
// QA-RustyMeter Task 3 (2026-05-30): master-strip LUFS readout.  Shows ONE of
// Momentary / Short-Term / Integrated at a time (small dropdown selector); all
// three are fed each vblank by MixerPage from VibeSynthProcessor::getMasterLufs.
// Sits between the master width knob and fader.  The strip holds no processor
// ref, so values are pushed in (not polled) -- same as the strip's DBFSMeter.
// Selected mode persists to settings.xml.  Layout (spec #1): the LUFS value on
// top (with a down-caret on the right) + the full mode title underneath
// ("Momentary" / "Short Term" / "Integrated").  Click anywhere -> mode popup.
class LufsReadoutBox : public juce::Component, public juce::SettableTooltipClient {
public:
    LufsReadoutBox();
    ~LufsReadoutBox() override = default;

    // UI thread (MixerPage::onVBlank): latest Momentary / Short-Term / Integrated.
    void setValues (float momentary, float shortTerm, float integrated);

    void paint     (juce::Graphics&)         override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    static juce::String modeName   (int mode);   // "Momentary" / "Short Term" / "Integrated"
    void  applyMode      (int mode, bool persist);
    void  refreshTooltip ();

    int   mMode { 0 };                           // 0=Momentary 1=Short-Term 2=Integrated
    float mVals[3] { -120.f, -120.f, -120.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LufsReadoutBox)
};

class DBFSMeter : public juce::Component, public juce::SettableTooltipClient {
public:
    DBFSMeter();
    ~DBFSMeter() override = default;

    // Mono entry point - both L and R get the same value.  Audio thread
    // writes (CAS-loop max) into mLevelDbL/R; UI thread on every vblank
    // exchanges them with -inf to read+reset (lock-free max window pattern).
    void setLevel       (float dBFS);
    // Stereo entry point - independent L and R levels.
    void setStereoLevel (float dBFS_L, float dBFS_R);

    // Split-layout meter (2026-05-30, QA-RustyMeter): non-master strips show a
    // peak bar (bottom half) + a scrolling RMS-history waveform (top half).
    // Master uses Full (peak bar only) since it carries the LUFS box instead.
    enum class Layout { Full, Split };
    void setMeterLayout (Layout l) { mLayout = l; }
    // Latest windowed RMS (dB) per channel, fed from the strip drain on the UI
    // thread (parallel to setStereoLevel's peak feed).  onVBlank pushes these
    // into the scrolling history ring.
    void setRmsStereo (float dBFS_L, float dBFS_R) { mRmsInL = dBFS_L; mRmsInR = dBFS_R; }

    // QA-Eg: smoothed visual value exposed for CableOverlay telemetry.
    // Returns max(mDisplayDbL, mDisplayDbR) - the FL-parity ballistic-smoothed
    // value the meter LEDs are currently rendering.  Cables reading this stay
    // in perfect visual sync with the meter bars (no separate smoothing layer
    // in MixerTrackStrip, no double-smoothing artifacts).
    float getCurrentDisplayedDb() const noexcept
    {
        return juce::jmax(mDisplayDbL, mDisplayDbR);
    }

    // Back-compat no-op.  All strips now use the FL-parity range below.
    void setCompact (bool) {}

    void paint   (juce::Graphics&) override;
    void resized () override {}

    // 2026-04-30: hover tooltip shows live "L: -3.2 dB  |  R: -5.7 dB" while
    // the mouse is over the meter.  TooltipWindow polls getTooltip() on its
    // own timer (~100 ms) so the value tracks audio in real time.  Overrides
    // SettableTooltipClient::getTooltip so any setTooltip() call still
    // gets ignored - the dynamic per-channel string takes precedence.
    juce::String getTooltip() override;

    // 2026-05-02: vblank-locked refresh.  Replaces the old 60 Hz Timer so
    // meter ballistics + repaint stay in lockstep with the monitor refresh
    // (FL-style).  The attachment is created lazily when the meter joins a
    // peer (see parentHierarchyChanged) and torn down when it leaves.
    void parentHierarchyChanged() override;

private:
    void onVBlank();
    void paintBar (juce::Graphics& g, juce::Rectangle<float> r,
                   float displayDb, float peakDb, bool drawLabels) const;
    static float dbToNorm (float dB) noexcept;   // log-style mapping

    // Split-layout helpers (QA-RustyMeter).
    void paintBars        (juce::Graphics& g, juce::Rectangle<float> r) const;   // L/R peak bars into r
    void paintRmsWaveform (juce::Graphics& g, juce::Rectangle<float> r) const;   // centered scrolling RMS

    // Per-channel running-max atomic.  Audio thread CAS-loops the max-since-
    // last-read into here; UI exchanges with -inf on each vblank.
    std::atomic<float> mLevelDbL { -std::numeric_limits<float>::infinity() };
    std::atomic<float> mLevelDbR { -std::numeric_limits<float>::infinity() };

    // Per-channel UI state (decayed display + peak hold).  Updated only on
    // the message thread inside onVBlank(); read in paint().  No atomicity
    // needed because the sole writer (vblank callback) and reader (paint)
    // both run on the message thread.
    float mDisplayDbL { -60.f }, mPeakDbL { -60.f };
    float mDisplayDbR { -60.f }, mPeakDbR { -60.f };
    double mPeakHoldUntilL { 0.0 }, mPeakHoldUntilR { 0.0 };  // ms timestamps

    // Last vblank timestamp (ms hi-res) for delta-time-based decay -- keeps
    // ballistics consistent across 60/120/144 Hz monitors.
    double mLastVBlankMs { 0.0 };

    // FL-parity range - top has +6 dB headroom above 0 dBFS so peaks above
    // the digital ceiling are still visible.
    static constexpr float kFloor             = -60.f;
    static constexpr float kCeiling           =   6.f;
    static constexpr float kDecayDbPerSec     =  20.f;
    static constexpr double kPeakHoldMs       = 1000.0;   // ~1 s hold
    // Piecewise log: top 30 % of bar covers -18..+6 dB (where it matters).
    static constexpr float kBreakDb           = -18.f;
    static constexpr float kBreakNorm         =  0.7f;

    // Split-layout state (QA-RustyMeter).  mLayout = Full (peak bar only, master)
    // vs Split (peak bar + scrolling RMS, all other strips).  The RMS ring holds
    // the most-recent windowed-RMS dB per channel (newest at mRmsHead-1);
    // paintRmsWaveform maps it across the top half, newest at top.
    Layout mLayout { Layout::Split };
    float  mRmsInL   { kFloor }, mRmsInR   { kFloor };   // raw per-frame RMS in (UI thread, strip drain)
    float  mRmsDispL { kFloor }, mRmsDispR { kFloor };   // EMA-smoothed value pushed to the ring
    static constexpr float kRmsTimeConstSec = 0.05f;      // RMS UI smoothing (~50 ms) - short so the wave tracks the music's dynamics
    static constexpr float kRmsTopFrac      = 0.35f;      // top 35% = RMS wave, bottom 65% = dBFS peak bar (Jeff 2026-05-30)
    static constexpr int kRmsHist = 256;          // ~3.5 s @ 60 Hz vblank (tunable)
    std::array<float, (size_t) kRmsHist> mRmsHistL { }, mRmsHistR { };
    int    mRmsHead { 0 };

    // VBlank attachment is constructed in parentHierarchyChanged once the
    // component has a peer.  Optional so we can null it out cleanly when
    // the meter is detached (e.g. tab switch).
    std::unique_ptr<juce::VBlankAttachment> mVBlank;
};

// ── Full basic sequence grid (rows x steps, scrollable) ──────────────────────
class BasicSequenceGrid : public juce::Component
{
public:
    // rowNames: displayed on left side of each row
    // numRows: 4 for layers, 1 for bass, 10 for drums
    BasicSequenceGrid(int numRows, const juce::StringArray& rowNames);

    void setNumSteps(int steps);
    void setRowColour(int row, juce::Colour col);
    void setStepData(int row, int step, bool active, float vel, float len);
    BasicStep getStepData(int row, int step) const;

    std::function<void(int row, int step, bool active, float vel, float len)> onStepChanged;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kRowH    = 36;
    static constexpr int kLabelW  = 120;
    static constexpr int kStepW   = 38;

private:
    int mNumRows  { 1 };
    int mNumSteps { DEFAULT_STEPS };
    juce::StringArray mRowNames;

    // Cells stored as [row][step]
    std::vector<std::vector<std::unique_ptr<BasicStepCell>>> mCells;

    std::unique_ptr<juce::Viewport>   mViewport;
    std::unique_ptr<juce::Component>  mContent;

    // Playhead indicator drawn in paint()
    int mPlayheadStep { -1 };

    void rebuild();
    void layoutCells();
};
