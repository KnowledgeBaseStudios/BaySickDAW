#pragma once
#include <JuceHeader.h>
#include <unordered_map>

class BaySickRustyDrumsProcessor;

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
// Knob / OptionMenu changes route to BaySickRustyDrumsProcessor::sendCc(N, val)
// which dispatches to the underlying sfizz instance and stamps the value into
// the processor's persistence map (so project save/load round-trips cleanly).
// ─────────────────────────────────────────────────────────────────────────────
class AriaControlPanel : public juce::Component
{
public:
    explicit AriaControlPanel (BaySickRustyDrumsProcessor* engine);
    ~AriaControlPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Re-parses + rebuilds the panel for the given program.  Pass an empty
    // file to clear (returns to placeholder text).  J-8 stage 2: also discovers
    // the kit's 03-kick / 04-snare / ... zoom-in XMLs in the same folder and
    // wires them to a sub-tab strip across the top of the panel.
    void loadFromKit (const juce::File& kitRoot, const juce::File& programXml);
    void clear();

    // Re-bind to a different engine instance (program switch creates a new
    // BaySickRustyDrumsProcessor; the panel must follow).
    void setEngine (BaySickRustyDrumsProcessor* engine) { mEngine = engine; }

private:
    // Re-parses the rendering for one specific XML — drops existing widgets
    // and rebuilds.  Background image + native size + child widgets are all
    // owned by these data members and cleared first.
    void parseGuiXml (const juce::File& xml);
    void rebuildTabBar();
    void selectTab (int tabIdx);

    BaySickRustyDrumsProcessor* mEngine { nullptr };

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
