#pragma once
#include <JuceHeader.h>
#include <unordered_map>
#include <functional>

// ── AriaControlPanel ─────────────────────────────────────────────────────────
// J-8 stage 2 (2026-05-04): renders the kit's prebuilt ARIA control surface
// (the same XML-driven GUI that shows in Sforzando, Falcon, and other ARIA
// hosts).  Parses `GUI/<program>.xml` from the kit root and instantiates
// child components for every `<StaticImage>`, `<StaticText>`, `<Knob>`, and
// `<OptionMenu>` declared.  Knobs use the kit's vertical filmstrip PNGs
// (Perc_knob.png 128 frames, Perc_stumpy.png 81 frames) for native ARIA look.
//
// XML coordinate space is the kit's native (e.g. 775x335 for Big Rusty Drums).
// We render at native size scaled to fit our component bounds (aspect locked).
//
// K-5 (2026-05-05): moved to Source/Standalone/ and refactored to take an
// engine-agnostic Binding struct so BaySickRustyDrumsPage AND sfizz-source
// InstPages (BaySickGuitars / BaySickBasses) can share the same panel
// implementation.  The binding carries:
//   * the engine's APVTS reference (so SliderParameterAttachment binds
//     each ARIA widget directly to the engine's `<prefix>cc<N>` param);
//   * a closure that builds the param-id string for a given CC index
//     (e.g. `"brd_cc<N>"` for Rusty, `"bgg_<instIdx>_cc<N>"` for Guitars);
//   * closures returning the kit-author default and label strings for
//     double-click reset + tooltips.
// ─────────────────────────────────────────────────────────────────────────────
class AriaControlPanel : public juce::Component
{
public:
    // Engine-agnostic binding.  Every field except apvts may be left empty —
    // missing closures degrade gracefully (kit-default reset → 64; label →
    // empty string; param attachment → no-op).
    struct Binding
    {
        juce::AudioProcessorValueTreeState* apvts { nullptr };
        std::function<juce::String(int cc)>  ccParamId;
        std::function<int(int cc)>           kitDefaultCc;
        std::function<juce::String(int cc)>  ccLabel;
    };

    explicit AriaControlPanel (Binding binding);
    ~AriaControlPanel() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // Re-parses + rebuilds the panel for the given program.  Pass an empty
    // file to clear (returns to placeholder text).  J-8 stage 2: also discovers
    // the kit's 03-kick / 04-snare / ... zoom-in XMLs in the same folder and
    // wires them to a sub-tab strip across the top of the panel.
    void loadFromKit (const juce::File& kitRoot, const juce::File& programXml);
    void clear();

    // K-5: re-bind to a different engine instance (program switch creates a
    // new processor, or InstPage rebuilds its source-mode chain).  Updates
    // every widget's APVTS attachment via parseGuiXml on the next selectTab.
    void setEngine (Binding binding);

private:
    void parseGuiXml (const juce::File& xml);
    void rebuildTabBar();
    void selectTab (int tabIdx);

    Binding mBinding;

    // Native coordinate space declared by the loaded GUI XML's `<GUI w h>`.
    // Defaults to 775x335 (Big Rusty Drums) until first load.
    int mNativeW { 775 };
    int mNativeH { 335 };

    // Image cache — every PNG from the kit's GUI/ folder loaded once + reused
    // across knobs that share the same filmstrip.
    std::unordered_map<juce::String, juce::Image> mImageCache;

    // Background — drawn in paint() rather than via a child Component so child
    // widgets can sit on top without re-parenting.
    juce::Image mBackgroundImage;

    // Static text labels — drawn in paint().  Position + style stored verbatim.
    struct StaticText
    {
        juce::Rectangle<float> nativeRect;     // x, y, w, h in XML's native coords
        juce::String           text;
        juce::Colour           color;
        bool                   transparent;
        bool                   smallFont;      // font_size="Small" mapped to 11pt
        juce::Justification    justification { juce::Justification::centred };
    };
    std::vector<StaticText> mStaticTexts;

    // Static images (other than the background, which is drawn first).
    struct StaticImagePiece
    {
        juce::Rectangle<float> nativeRect;
        juce::String           imageName;
        bool                   transparent;
    };
    std::vector<StaticImagePiece> mStaticImages;

    // Interactive widgets (own their own juce::Component subclass).
    std::vector<std::unique_ptr<juce::Component>> mWidgets;

    // Base directory used to resolve `image="<name>"` paths.  Set on every
    // load to `<kitRoot>/GUI`.
    juce::File mGuiDir;

    // J-8 stage 2: sub-tab strip.  "Main" is always tab 0 and renders the
    // active program (01-full or 02-basic).  Subsequent tabs are 03-08 zoom-in
    // pages discovered alongside the main XML.
    struct TabEntry { juce::String label; juce::File xmlFile; };
    std::vector<TabEntry>                       mTabs;
    std::vector<std::unique_ptr<juce::TextButton>> mTabButtons;
    int                                         mActiveTab { 0 };
    static constexpr int kTabBarHeight = 26;


    // Parses a `#AARRGGBB` or `#RRGGBBAA` color string.  ARIA emits AARRGGBB
    // (alpha-first) in some kits and RRGGBBAA in others; we autodetect by
    // inspecting trailing alpha vs leading alpha for the all-opaque kits.
    static juce::Colour parseColor (const juce::String& s, juce::Colour fallback = juce::Colours::white);

    // Resolves an image filename against `mGuiDir`, loads + caches.  Returns
    // a null Image on failure (caller paints fallback).
    juce::Image loadImage (const juce::String& filename);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AriaControlPanel)
};
