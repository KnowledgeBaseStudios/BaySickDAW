#pragma once
#include <JuceHeader.h>
#include "../BaySickConstants.h"
#include "../PatternManager.h"

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
    // QA-TrueLevel SC-10: Direct to Master strips belong to no engine family.
    inline const juce::Colour DirectGrey = juce::Colour(0xff8a8f96);
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
class BaySickLAF : public juce::LookAndFeel_V4
{
public:
    BaySickLAF();
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
    // ── TS7 §9.1/§9.3: desktop-window chrome ─────────────────────────────────
    // BaySickLAF is the app-wide default LookAndFeel (StandaloneEditor sets it), so
    // overriding here is what makes every juce::DocumentWindow / DialogWindow
    // with a NON-native title bar paint the shell's strip -- no per-window
    // look-and-feel plumbing, and no second place for the look to drift to.
    // Windows that keep setUsingNativeTitleBar(true) are unaffected.  The main
    // app frame does NOT opt into native chrome -- it paints through this
    // override too, which is where the centred app title + icon land (L26).
    void drawDocumentWindowTitleBar (juce::DocumentWindow&, juce::Graphics&,
                                     int w, int h, int titleSpaceX, int titleSpaceW,
                                     const juce::Image* icon,
                                     bool drawTitleTextOnLeft) override;
    juce::Button* createDocumentWindowButton (int buttonType) override;
    void positionDocumentWindowButtons (juce::DocumentWindow&,
                                        int titleBarX, int titleBarY,
                                        int titleBarW, int titleBarH,
                                        juce::Button* minimise, juce::Button* maximise,
                                        juce::Button* close,
                                        bool positionTitleBarButtonsOnLeft) override;

    static BaySickLAF& get() { static BaySickLAF laf; return laf; }

    // Helper: mark a tooltip string as automatable (appends tag)
    static juce::String automatable(const juce::String& tip)
    {
        return tip + "\n*Automatable*";
    }
};

// ── BaySickTooltip ───────────────────────────────────────────────────────────────
// Global tooltip window. Renders via BaySickLAF::drawTooltip / getTooltipBounds.
// One instance owned by StandaloneEditor - applies to all child components.
// APVTS-bound controls append "\n*Automatable*" via BaySickLAF::automatable().
class BaySickTooltip : public juce::TooltipWindow
{
public:
    explicit BaySickTooltip(juce::Component* parent, int delayMs = 500)
        : juce::TooltipWindow(parent, delayMs)
    {
        setLookAndFeel(&BaySickLAF::get());
    }

    ~BaySickTooltip() override
    {
        setLookAndFeel(nullptr);
    }

    // NEVER while something is modal -- a popup menu, in practice.
    //
    // juce_TooltipWindow.cpp:219 gates on
    //   newComp == nullptr || getParentComponent() == nullptr
    //                      || newComp->getPeer() == getPeer()
    // While this window was PARENTED that middle term was false, so a menu item
    // (which lives in the menu's own desktop peer, never the editor's) failed
    // the gate and no tooltip was ever evaluated over an open menu.  Making the
    // window parentless -- required so tooltips draw above the contained
    // windows -- makes that term TRUE, so tooltips began evaluating over popup
    // menus for the first time and displayTipInternal put an always-on-top
    // temporary desktop window up while the menu was modal.
    //
    // A tooltip over a modal menu is wrong on its own terms, so this guard is
    // right regardless of what it fixes.
    //
    // Hooked on getTipFor rather than timerCallback: TooltipWindow inherits
    // Timer PRIVATELY, so timerCallback cannot be overridden from outside.
    // Returning an empty tip is equivalent and lands earlier -- the base timer
    // treats "no tip" as hide-and-do-nothing (juce_TooltipWindow.cpp:246).
    juce::String getTipFor (juce::Component& c) override
    {
        if (juce::Component::getCurrentlyModalComponent() != nullptr)
            return {};
        return juce::TooltipWindow::getTipFor (c);
    }
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
    // HOLD-FOR-GL-RENDERER: disabled 2026-04-21 (CPU-renderer banding); re-enable
    // plan T3-LRX5Vignette, Future State BLU-370/BLU-489. Call site was
    // StandaloneEditor::paintOverChildren.
    static void drawVignette(juce::Graphics& g, juce::Rectangle<int> bounds,
                             float strength = 0.45f);
};

// ── Filmstrip rendering helpers ───────────────────────────────────────────────
// All filmstrips are vertical PNG strips (frames stacked top-to-bottom).
// Images are lazy-loaded at first use from "Resources/Filmstrips/" next to the
// exe (staged there by CMake post-build; packaged by the installer).
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

// ── TooltipMenuItem ──────────────────────────────────────────────────────────
// JUCE PopupMenu items carry no tooltip.  A custom item component is the only
// hook JUCE gives us; TooltipClient on it is what the tooltip window queries
// (the app's single parentless VibeTooltip finds components inside menu
// windows).  Born for the locked-Freeze row (Jeff, 2026-08-04), shared since
// the Pan Law rows needed the same thing (QA-TrueLevel SC-2).  `ticked` paints
// the radio-style check the stock items show for the current choice.
class TooltipMenuItem : public juce::PopupMenu::CustomComponent,
                        public juce::TooltipClient
{
public:
    TooltipMenuItem (juce::String text, juce::String tip, bool enabled,
                     juce::Colour textColour, bool ticked = false);

    juce::String getTooltip() override { return mTip; }
    void getIdealSize (int& w, int& h) override;
    void paint (juce::Graphics& g) override;

private:
    juce::String mText, mTip;
    bool         mEnabled, mTicked;
    juce::Colour mColour;
};

// ── PageMenuBar ───────────────────────────────────────────────────────────────
// Tier 2: sits below the ribbon, above sub-tabs / content.
// Shows ≡ hamburger (opens popup), optional page title, optional action items.
class PageMenuBar : public juce::Component
{
public:
    PageMenuBar();

    void setPageTitle(const juce::String& t);

    // QA-Layout T3 (Window-4/L2): the engine's colored name, centered on the
    // strip in BaySickTitleBar's bloom style -- the dissolved engine title
    // bars' identity moved up here.  Independent of setPageTitle (the small
    // grey tab title, suppressed when tab slots exist); D7 reviews
    // narrow-width collisions.  Empty name = nothing drawn.
    void setCenterTitle(const juce::String& name, juce::Colour accent);
    const juce::String& getPageTitle() const noexcept { return mTitle; }

    // Universal page-actions menu (2026-04-19): components install a menu
    // builder that the hamburger invokes. Used by the EQ window (and
    // future per-tab actions across all pages) to surface their options
    // through the hamburger instead of an in-component ... button. The builder
    // is given the hamburger as the popup anchor; it is responsible for both
    // populating + showing the menu, so it can include submenus, checkmarks
    // and disabled items. Pass nullptr to clear -- the hamburger then opens
    // nothing.
    using MenuBuilder = std::function<void(juce::Component* anchor)>;
    void setMenuBuilder(MenuBuilder builder);
    // QA-Layout T10 (L13): second flat titled heading right of "Menu" -- the
    // strip reads "Menu  Add".  Hidden when no builder is installed; pass
    // nullptr to clear (the branch-top clear in showPageForTab does).
    void setAddMenuBuilder(MenuBuilder builder);

    // QA-Layout T16 (Jeff, 2026-08-04): further flat headings right of "Menu"
    // (and "Add"), same native-menu-bar styling.  Builder uses them for Edit
    // and View so its own 20px menu row could be deleted and the grid moved up.
    // onOpen receives the heading index and the button to anchor the popup on.
    // The branch-top clear in showPageForTab drops them per page-show.
    void setExtraHeadings (const juce::StringArray& labels,
                           std::function<void(int, juce::Component*)> onOpen);
    void clearExtraHeadings();

    // ── View-mode menu (Jeff, 2026-08-05) ─────────────────────────────────────
    // Installs a "View" heading listing the given mode names with a tick on the
    // active one.  It lands right of "Menu" and LEFT of the tab slots, so on the
    // pedals window the strip reads "Menu  View  NAM/IR".
    //
    // This is the REUSABLE half of view swapping, and is meant to stay that way:
    // any window that grows a second view calls this and supplies two closures.
    // The heading knows nothing about what the modes mean; the editor owns its
    // layout and the host owns the window resize.  Rolling a bespoke switcher
    // per player later is the thing this exists to prevent (see CL-307).
    void setViewMenu (const juce::StringArray& modeNames,
                      std::function<int()>     getMode,
                      std::function<void(int)> setMode);

    // --shot only: run the stored builders through their real click paths so
    // the harness's capture hook (ShotMenuHook.h) can take the menu headless.
    void triggerMenuForShot()                { showHamburgerMenu(); }
    void triggerExtraHeadingForShot (int i)
    {
        if (i >= 0 && i < (int) mExtraHeadings.size()
            && mExtraHeadings[(size_t) i]->onClick)
            mExtraHeadings[(size_t) i]->onClick();
    }

    // QA-Layout T17: the Menu dropdown's "Visual" entry -- opens this effect's
    // Visual sub-page window, and is the way back once the user has closed it.
    //
    // T20 (Jeff, 2026-08-05): `available` is a PRESENCE gate -- false and the
    // entry is not built at all.  Reverses T17's show-it-greyed treatment for
    // this one item; unlike locked Freeze, which is a capability the user can
    // unlock and therefore needs to be told about, an effect with no visual has
    // nothing to offer and no path to acquiring one, so a permanently dead row
    // in every other effect's menu is noise rather than discoverability.
    void setVisualSlot (std::function<void()> openVisual,
                        std::function<bool()> available);

    // ── Non-owning extra components on the right (e.g. Kit ▾, Nav combo) ────────
    // Components are reparented into PageMenuBar. Call clear before the page hides.
    // QA-Layout T3: entries are SafePointers -- a mounted component owned by an
    // engine editor can die on an engine swap before the next page-show clears
    // the strip, and a raw pointer here was a guaranteed dangle.
    void addExtraRightComponent(juce::Component* c, int width);
    // 2026-04-19: targeted removal so per-tab extras (e.g. EQ bank indicator)
    // can be added/removed without disturbing page-level extras that should
    // persist across tab switches. No-op if c isn't currently in the list.
    void removeExtraRightComponent(juce::Component* c);
    void clearExtraRightComponents();

    // ── Tab slot buttons (owned, laid out after ≡) ────────────────────────────
    // QA-Layout T15: player pages no longer mount slot clusters (their nav
    // entries live in the Menu dropdown).  Remaining users: the EQ windows'
    // Pre/Post pair, PianoRollPage's jump cluster, the pedals window's NAM/IR
    // launcher.  Call clearTabSlots when reconfiguring a bar without slots.
    void setTabSlots(const juce::StringArray& labels,
                     std::function<void(int)> onTabClick,
                     int activeIdx = 0,
                     juce::Colour accent = juce::Colour());
    void updateTabActive(int idx);
    void clearTabSlots();
    // Per-slot width, default 74.  A narrow strip (the pedals Compact view)
    // sets this small and leans on the slot's tooltip for the full name.
    void setTabSlotWidth (int px);
    void setTabSlotTooltip (int idx, const juce::String& tip);

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

    // FX Rack jump.  Jeff, 2026-08-04: this is a MENU ITEM, not a bar button --
    // registration is unchanged (set per page-show, empty fn removes it,
    // clearTabSlots() clears it), but it surfaces through appendStandardItems.
    void setFxRackSlot(std::function<void()> onClick);

    // Appends the entries every player window shares -- FX Rack, then Freeze --
    // to a menu a page is building.  Called from each page's nav-menu hook so
    // the items land in that window's own Menu dropdown.  No-op when neither
    // slot is registered (Effects / Mixer / Builder / Piano Roll).
    void appendStandardItems (juce::PopupMenu& m);

    // Smoke round 2 (Jeff): per-player Swing Mix knob.  Jeff, 2026-08-04: it
    // now sits at the far LEFT, immediately right of the Menu heading, so its
    // position never shifts with whatever else a page mounts.  Visible on
    // EVERY sub-tab of a player page (the engine title-bar hosting only showed
    // on the Player sub-tab).  Set per page-show with that page's swing
    // binding; empty getMix hides it; clearTabSlots() clears it too.
    void setSwingKnobSlot (std::function<float()>     getMix,
                           std::function<void(float)> setMix,
                           std::function<bool()>      getTruncate,
                           std::function<void(bool)>  setTruncate);

    // TS7 §6 freeze toggle (Jeff, 2026-07-30).  Jeff, 2026-08-04: a MENU ITEM
    // under this window's Menu, directly after FX Rack, so the placement is
    // identical on every player AND it sits where the user is already looking
    // at the player that is misbehaving.
    //
    // The tab right-click menu was the obvious alternative and is WRONG: that
    // menu acts on a tab TYPE, so it could not target one player.
    //
    // getState: 0 = not frozen, 1 = frozen, 2 = frozen but stale (playing live
    // while its file re-renders).  An empty getState hides the slot entirely.
    // getDisabledReason returning a non-empty string shows the button DISABLED
    // with that reason as its tooltip rather than hiding it -- a capability the
    // user cannot see is a capability they cannot ask for.
    // isVocal appends the §6.9 warning: a vocal freeze prints the whole chain
    // including pitch and alignment, and is for reclaiming CPU once a sound is
    // settled rather than something to leave on while setting one up.
    void setFreezeSlot (std::function<int()> getState,
                        std::function<void(bool /*wantFrozen*/)> onToggle,
                        std::function<juce::String()> getDisabledReason = {},
                        bool isVocal = false);
    // Repaints the freeze button from getState without rebuilding the slot.
    void refreshFreezeState();

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kHeight = 26;
    // QA-Layout L31: width of the "Menu" entry -- a flat native-menu-bar-style
    // text heading, not a chrome button (was a 22px "=" glyph).  Shared by
    // resized() and the paint() title x-offset.
    static constexpr int kMenuBtnW = 46;
    // T10 (L13): width of the "Add" heading, same flat style as "Menu".
    static constexpr int kAddBtnW  = 40;

private:
    juce::String mTitle;
    MenuBuilder           mMenuBuilder;
    MenuBuilder           mAddMenuBuilder;   // T10 (L13)

    std::unique_ptr<juce::TextButton> mHamburgerBtn;
    std::unique_ptr<juce::TextButton> mAddBtn;   // T10 (L13)
    std::vector<std::unique_ptr<juce::TextButton>> mExtraHeadings;   // T16

    // Non-owning extra right components (e.g. Kit button, Nav combo).
    // SafePointer: see addExtraRightComponent.
    struct ExtraComp { juce::Component::SafePointer<juce::Component> comp; int width; };
    std::vector<ExtraComp> mExtraRight;

    juce::String mCenterName;
    juce::Colour mCenterAccent;
    int          mTabSlotW { 74 };
    // Free span left over between the left cluster and the right extras, filled
    // by resized() and used by paint() to place mCenterName.  See paint().
    int          mCenterFreeL { 0 };
    int          mCenterFreeR { 0 };

    // Tab slot buttons (owned)
    std::vector<std::unique_ptr<juce::TextButton>> mTabSlotBtns;
    std::unique_ptr<juce::TextButton>              mMidBtn;
    std::unique_ptr<juce::TextButton>              mSideBtn;
    std::function<void()>                          mFxRackAction;
    std::function<void(bool)>                      mFreezeToggle;   // TS7 §6
    std::function<int()>                           mFreezeState;
    std::function<juce::String()>                  mFreezeDisabledReason;
    bool                                           mFreezeIsVocal { false };
    std::function<void()>                          mVisualAction;            // T17
    std::function<bool()>                          mVisualAvailable;         // T20 presence gate
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

// ── Effect bypass LED (QA-ModelShell TS5) ────────────────────────────────────
// The green/red dot that has always sat at the left of an FX slot header.  TS5
// shows the SAME LED in three places -- the rack window's slot row, the
// per-effect panel window's title strip, and the classic inline slot header the
// vocal chain still uses -- so the drawing lives in one function and every site
// calls it.  Green = effect active, red = bypassed.
namespace EffectBypassLed
{
    void paint (juce::Graphics& g, juce::Rectangle<int> area, bool bypassed);
}

// Clickable widget form of the LED above, for the sites that need it as a real
// component rather than a painted region of a bigger strip.
class BypassLedButton : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;

    void setBypassed (bool b) { if (b != mBypassed) { mBypassed = b; repaint(); } }
    bool isBypassed() const noexcept { return mBypassed; }

    void paint (juce::Graphics& g) override
    { EffectBypassLed::paint (g, getLocalBounds(), mBypassed); }

    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    bool mBypassed { false };
};

// ── MixerLedButton (5F-4a) ───────────────────────────────────────────────────
// LED-style toggle button with a glowing colored dot and optional small label.
// Subclasses juce::Button directly so BaySickLAF's filmstrip toggle path is
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

    // CL-299 (1): slider property that opts a knob into the additive-feedback
    // warning ring.  Its VALUE is the normalized position where the warning
    // starts, so the LAF never needs to know a knob's units.
    //   slider.getProperties().set (TimeLAF::kWarnRingFrom, 1.0 / 1.2);
    static constexpr const char* kWarnRingFrom = "warnRingFrom";
    // CL-299 (1) second half (Jeff, 2026-08-05): the LIVE feedback level,
    // normalized onto the same arc scale, refreshed by the owning panel's
    // timer.  The ring is a METER of the feedback occurring -- where its lit
    // head sits IS the current level, and it only reads red when the loop is
    // genuinely in the clipping zone.  Absent/0 = silent, ring shows only the
    // setting track.
    static constexpr const char* kWarnRingLive = "warnRingLive";

    // Ring ellipse calibration -- the Time-Based knob art is drawn in
    // perspective, so a flat circle cannot sit on its face; the ring is a
    // squashed, tilted ellipse instead.  Values are Jeff's, fitted by eye with
    // the T20 placement box (2026-08-06) and hardcoded from his settled
    // numbers; his offsets came out 0/0, so only stretch + tilt survive.
    static constexpr float kWarnRingScaleX = 1.417f;
    static constexpr float kWarnRingScaleY = 0.889f;
    static constexpr float kWarnRingRotDeg = -1.5f;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float startAngle, float endAngle, juce::Slider& s) override;

    static void drawWarnRing (juce::Graphics& g, juce::Rectangle<float> area,
                              float sliderPos, float warnFrom, float liveNorm,
                              float startAngle, float endAngle);

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

    // Registers a playback applicator for a paramId: given a 0..1 value, drive
    // whatever that lane targets.
    //
    // QA-ModelShell TS3 (2026-07-27): the `owner` Component these took is gone.
    // It existed so the registry could drop an entry when the control that
    // registered it died -- necessary while views registered lanes, and
    // meaningless now that none do.  Every caller is a model-side registration
    // (engine creation, rack slot, pedal slot, param materialization) whose
    // closures resolve their target through the model at APPLY time, so a
    // registration is valid exactly as long as the app is.
    extern std::function<void(const juce::String& paramId,
                              std::function<void(float)>)> sOnRegisterApplicator;

    // Called alongside sOnRegisterApplicator to register a value reader:
    // returns the current normalized 0..1 value of the lane's target.
    extern std::function<void(const juce::String& paramId,
                              std::function<float()>)> sOnRegisterReader;

    // The counterpart to the two above, and the only removal path they have
    // besides the wholesale clear at a project boundary.  Rack and pedal slot
    // paramIds are "<channelPrefix>_<slotUuid>_<param>", and a slot mints a
    // FRESH uuid on every user-facing load -- so without this, auditioning
    // effects in one slot leaves a block of closures and a block of dead
    // "Automate" menu targets behind on every swap, for the life of the
    // session.  Fired with the RETIRING uuid; a uuid is unique, so matching on
    // it needs no channel context.  MESSAGE THREAD ONLY -- it destroys
    // std::function closures, so it must never be called under a lock the audio
    // thread can want.
    extern std::function<void(const juce::String& slotUuid)> sOnUnregisterSlotUuid;

    // QA-ModelShell TS3 (2026-07-27): the five register*Automation helpers that
    // lived here -- Slider / Button / Combo / Parameter / Selector -- are gone
    // with their last callers.  Every one of them tied a lane's lifetime to a
    // VIEW: four drove the control itself, and the Parameter variant still took
    // a Component lifetimeGuard.  Under destroy-on-close windows that is a
    // guaranteed dead lane, so engine-parameter registration moved to the model
    // (StandaloneEditor::registerModelEngineAutomation, off the rig's
    // engine-created event) and rack/pedal registration to the rack and board.
    // Views now only stamp componentIDs for the right-click Automate menu.

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
    // sOnMidiLearn / sOnMidiForget: action triggers.
    extern std::function<bool(const juce::String& paramId)>          sIsMidiMapped;
    extern std::function<bool(const juce::String& paramId)>          sIsMidiLearningTarget;
    extern std::function<juce::String(const juce::String& paramId)>  sDescribeMidiMapping;
    extern std::function<void(const juce::String& paramId)>          sOnMidiLearn;
    extern std::function<void(const juce::String& paramId)>          sOnMidiForget;

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
    // I-3c (2026-05-02): MIDI Learn items shared between VKnob's own
    // mouseDown and this global handler.  IDs start at 100 to keep the
    // 1-99 range free for automation items above.
    static constexpr int kMidiFirstId = 100;

    static juce::PopupMenu buildControlMenu (const juce::String& id, bool isSlider)
    {
        // Resolve paramId -> friendly label for the menu text only; the backend
        // callback still receives the stable `id`.
        juce::String menuLabel;
        if (VKnobAutomation::sResolveMenuLabel)
            menuLabel = VKnobAutomation::sResolveMenuLabel(id);
        if (menuLabel.isEmpty()) menuLabel = id;

        juce::PopupMenu m;
        m.addItem(1, "Automate: " + menuLabel);
        // Only offer "Type in value" for components that have a value (Sliders).
        if (isSlider)
            m.addItem(2, "Type in value...");

        if (VKnobAutomation::sShouldOfferModulate
            && VKnobAutomation::sShouldOfferModulate(id))
            m.addItem(3, "Modulate envelope...");

        VKnobAutomation::appendMidiLearnMenuItems (m, id, kMidiFirstId);
        return m;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!e.mods.isRightButtonDown()) return;

        auto* comp = e.eventComponent;
        if (!comp) return;

        // Skip VKnob's internal slider - identified by a property tag set in VKnob ctor.
        if ((bool)comp->getProperties()["vknob_slider"]) return;

        juce::String id = comp->getComponentID();
        if (id.isEmpty()) return;

        auto* asSlider = dynamic_cast<juce::Slider*>(comp);
        juce::Component::SafePointer<juce::Slider> safeSlider(asSlider);

        auto m = buildControlMenu (id, asSlider != nullptr);

        m.showMenuAsync(juce::PopupMenu::Options{}, [id, safeSlider](int result)
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

// ── BaySickSlider ───────────────────────────────────────────────────────────────
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
class BaySickSlider : public juce::Slider
{
public:
    BaySickSlider() = default;
    BaySickSlider(SliderStyle style, TextEntryBoxPosition textPos = NoTextBox)
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

// QA-ManualPress M-4 option C: the manual's callout dots anchor themselves.
// A component declares the callout it is the target of -
//     comp.getProperties().set (kDotAnchor, "BSSBOSC-4");
// - and `BaySickDAW.exe --shot --docs` emits its live bounds, so the dot
// tracks the real layout instead of a hand-measured percentage.  Costs one
// property on a component that is built once; nothing reads it at runtime.
static const juce::Identifier kDotAnchor ("dotAnchor");

// ── Compact knob with label ───────────────────────────────────────────────────
class VKnob : public juce::Component,
              private juce::Slider::Listener
{
public:
    // BaySickSlider so right-click never jogs the knob; the Automate menu still
    // fires because VKnob registers as a mouseListener here and
    // Component::internalMouseDown notifies listeners after the swallowed mouseDown.
    BaySickSlider   slider;
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

    // Fired on drag end - use to build undo actions.
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
// BaySickLAF switch_toggle filmstrip regardless of the panel's current LAF.
//
// Right-click never flips the switch (button + label paths both swallow it):
// it is reserved for the GlobalAutoRightClick "Automate" menu, which fires
// via the app-wide mouse listener regardless of the swallow -- same
// rationale as BaySickSlider.  The menu appears only on toggles a panel has
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
// SnapSlider inherits BaySickSlider so the mixer fader gets the right-click
// swallow behaviour for free.
class SnapSlider : public BaySickSlider
{
public:
    SnapSlider() : BaySickSlider(juce::Slider::LinearVertical, juce::Slider::NoTextBox) {}
    double snapValue(double v, DragMode) override
    {
        return (std::abs(v) < 1.5) ? 0.0 : v;
    }
};

// QA-EqPro T6: ParametricEQDisplay deleted - the EQ window is the
// EqWindowUI stack (EqGraphView + EqRailView) drawing from the kbs
// engine's own queries, which retires the display's whole
// second-formula defect class (D1-D5) and both dead bind modes.

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

    // ONE definition of the calibration submenu.  It is offered from the
    // Effects rack menu AND from the VU window's own menu (Jeff, 2026-08-12);
    // two hand-built copies of a five-item radio group would drift the moment
    // the range or the wording changed.
    static void addCalibrationSubMenu (juce::PopupMenu& parent);

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
// three are fed each vblank by MixerPage from BaySickDAWProcessor::getMasterLufs.
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

