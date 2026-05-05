#include "AriaControlPanel.h"
#include "BaySickRustyDrumsProcessor.h"
#include "../Standalone/SharedUI.h"   // VKnobAutomation popup glue

namespace
{
// ── Beginner-friendly tooltip descriptions ──────────────────────────────────
// Big Rusty Drums (and most ARIA drum kits) label each knob with a terse 2–8
// character abbreviation: "Close", "OH", "Punch", "Tune", "Dirt", "Btm", etc.
// For users who have never made music before, those words are jargon.  This
// table decodes the common ones into one-sentence "what it does + when to
// reach for it" descriptions, which we append to the tooltip below the
// kit's label and live value.
//
// Lookups are case-insensitive substring matches so multi-word labels like
// "Kick mic" or "Snare Btm" pick up the relevant token (e.g. "mic", "btm").
// Order: more-specific phrases BEFORE single tokens (e.g. "low cut" before
// "low") so the longer match wins.
struct DrumTermHelp { const char* phrase; const char* explanation; };
constexpr DrumTermHelp kDrumTermHelp[] = {
    // Hi-hat-specific
    { "hi-hat position", "How open or closed the hi-hat is.  Lower = tightly closed (sharp 'tick'), "
                          "higher = wide open (loose 'sizzle').  Drives CC4." },
    { "hi-hat close",    "Close mic for the hi-hat - the bright, articulate tick of the stick on metal." },
    { "hi-hat oh",       "Overhead mic for the hi-hat - bigger, airier, blends with the rest of the kit." },
    { "hi-hat pan",      "Stereo position of the hi-hat in the mix.  Left = -, right = +." },
    { "hi-hat tune",     "Pitches the hi-hat up or down.  Lower = darker, heavier; higher = brighter, snappier." },

    // Tom-specific
    { "lo tom punch",    "Boosts the attack of the low tom - adds aggression and impact." },
    { "hi tom punch",    "Boosts the attack of the high tom - adds aggression and impact." },
    { "tom dirt",        "Adds saturation/distortion to all toms.  Subtle = warmth; heavy = aggressive grit." },
    { "tom deaden",      "Cuts the toms' sustain so they sound shorter and more controlled." },

    // Kick-specific
    { "kick mic",        "The main kick-drum mic.  This is the body of the kick - punch, weight, low end." },
    { "kick oh",         "Overhead/room blend for the kick.  Adds air and ambiance around the punch." },
    { "kick buzz",       "Sympathetic snare buzz triggered by the kick.  Realistic but can muddy a mix at high values." },
    { "kick punch",      "Boosts the kick's attack click - useful when the kick needs to cut through a busy mix." },

    // Snare-specific
    { "snare btm",       "Bottom-snare mic - captures the snare wires, gives the 'crack' and 'sizzle'." },
    { "snare top",       "Top-snare mic - captures the head being hit, gives the 'thud' and tone." },
    { "snare oh",        "Overhead blend for the snare - air and ambiance around the snare body." },
    { "snare snap",      "Boosts the high-frequency snap of the wires - makes the snare cut through." },
    { "snare punch",     "Boosts the snare's attack - emphasizes the hit, useful for rock and pop." },
    { "snare epic",      "Big ambient room blend - turns a tight snare into a wide, cinematic snare." },

    // Generic mic positions
    { "close",           "Close mic - positioned right next to the drum head.  Detailed and dry." },
    { "oh",              "Overhead mic - captures the drum from above with surrounding air." },
    { "btm",             "Bottom mic - captures the underside, including snare wires or shell resonance." },
    { "top",             "Top mic - captures the head being struck." },
    { "mic",             "Microphone level for this section." },

    // Generic shaping
    { "low cut",         "High-pass filter - cuts low rumble.  Higher value = more low end removed." },
    { "high cut",        "Low-pass filter - cuts high frizz.  Higher value = duller / warmer." },
    { "punch",           "Transient enhancer - makes the attack sharper and more aggressive." },
    { "tune",            "Pitches the drum up or down without changing its character." },
    { "dirt",            "Adds saturation/distortion.  A little warms; a lot grunges it up." },
    { "crush",           "Bit-crusher / lo-fi processor.  Adds a digital, gritty edge." },
    { "deaden",          "Reduces the drum's ring-out, making it sound shorter and tighter." },
    { "dead",            "Reduces the drum's ring-out, making it sound shorter and tighter." },
    { "snap",            "Boosts the high-frequency snap or crack of the drum." },
    { "buzz",            "Adds sympathetic resonance - the rattling of nearby drum wires." },
    { "epic",            "Adds a big ambient/cinematic blend." },
    { "pan",             "Stereo position in the mix.  Left = -, center = 0, right = +." },

    // Articulation menus
    { "snare type",      "Which set of snare articulations to play (Sticks / Brushes / Mallets / Tom-on-snare)." },
    { "stir type",       "Brush stir style - Smooth, Reg(ular), or Peaky for different brush textures." },
    { "tom type",        "Which set of tom articulations (Sticks / Brushes / Mallets)." },
};

static juce::String beginnerExplanation (const juce::String& kitLabel)
{
    if (kitLabel.isEmpty()) return {};
    const auto needle = kitLabel.toLowerCase();
    for (const auto& e : kDrumTermHelp)
        if (needle.contains (juce::String (e.phrase).toLowerCase()))
            return juce::String (e.explanation);
    return {};
}

// Right-click menu mirroring VKnob's: Automate / Type Value / MIDI Learn.
// Fires VKnobAutomation static callbacks if registered.  The menu items
// themselves are rendered manually since juce::Slider's default popup is just
// the value tooltip.
static void showAriaParamPopup (juce::Slider& slider, const juce::String& paramId)
{
    juce::Component::SafePointer<juce::Slider> safeSlider (&slider);
    juce::String menuLabel;
    if (VKnobAutomation::sResolveMenuLabel)
        menuLabel = VKnobAutomation::sResolveMenuLabel (paramId);
    if (menuLabel.isEmpty()) menuLabel = paramId;

    juce::PopupMenu m;
    m.addItem (1, "Automate: " + menuLabel);
    m.addItem (2, "Type in value...");
    constexpr int kMidiFirstId = 100;
    VKnobAutomation::appendMidiLearnMenuItems (m, paramId, kMidiFirstId);

    m.showMenuAsync (juce::PopupMenu::Options{}, [paramId, safeSlider] (int result)
    {
        if (result == 1 && VKnobAutomation::sOnAutomate)
            VKnobAutomation::sOnAutomate (paramId);
        else if (result == 2 && safeSlider.getComponent() != nullptr)
            VKnobAutomation::promptSliderValueEntry (*safeSlider.getComponent(), paramId);
        else
            VKnobAutomation::handleMidiLearnMenuResult (result, kMidiFirstId, paramId);
    });
}

// ── AriaKnob ─────────────────────────────────────────────────────────────────
// Slider subclass with vertical-filmstrip paint.  APVTS-bound via
// SliderParameterAttachment so right-click → Type Value, automation, MIDI
// Learn, and undo/redo all work natively.  Double-click resets to the kit's
// set_cc<N> default rather than the slider midpoint.
class AriaKnob : public juce::Slider
{
public:
    AriaKnob (BaySickRustyDrumsProcessor* engine,
              int cc, juce::Image strip, int frames,
              juce::Rectangle<float> nativeRect)
        : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag,
                         juce::Slider::NoTextBox),
          mEngine (engine), mCc (cc),
          mStrip (std::move (strip)), mFrames (juce::jmax (1, frames)),
          mNativeRect (nativeRect)
    {
        setRange (0.0, 127.0, 1.0);
        setMouseDragSensitivity (200);
        setVelocityBasedMode (false);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        // Value popup during drag (current CC value bubbles above the knob).
        setPopupDisplayEnabled (true, true, nullptr);

        if (mEngine != nullptr)
        {
            const auto pid = "brd_cc" + juce::String (mCc);
            if (auto* p = mEngine->apvts.getParameter (pid))
                mAttach = std::make_unique<juce::SliderParameterAttachment> (*p, *this);
        }
    }

    // Hover tooltip: kit's `label_cc<N>` + live value, plus a beginner-friendly
    // sentence about what this knob actually does, decoded from common drum-mic
    // / sound-shaping abbreviations in the label.
    juce::String getTooltip() override
    {
        juce::String label = (mEngine != nullptr) ? mEngine->getCcLabel (mCc) : juce::String();
        if (label.isEmpty()) label = "CC " + juce::String (mCc);
        juce::String tip = label + " -" + juce::String ((int) getValue());
        const auto help = beginnerExplanation (label);
        if (help.isNotEmpty()) tip += "\n\n" + help;
        return tip;
    }

    // Integer formatting for the drag-popup bubble (whole CC values, no decimals).
    juce::String getTextFromValue (double value) override
    {
        return juce::String ((int) std::round (value));
    }

    juce::Rectangle<float> getNativeRect() const { return mNativeRect; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds();
        if (mStrip.isNull() || mFrames <= 0 || b.isEmpty())
        {
            g.setColour (juce::Colours::grey);
            g.fillEllipse (b.toFloat().reduced (1.0f));
            return;
        }
        const double normalized = (getValue() - getMinimum())
                                 / juce::jmax (1.0, (getMaximum() - getMinimum()));
        const int idx = juce::jlimit (0, mFrames - 1,
                                       (int) std::round (normalized * (mFrames - 1)));
        const int sw = mStrip.getWidth();
        const int frameH = juce::jmax (1, mStrip.getHeight() / mFrames);
        g.drawImage (mStrip,
                     b.getX(), b.getY(), b.getWidth(), b.getHeight(),
                     0, idx * frameH, sw, frameH);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            showAriaParamPopup (*this, "brd_cc" + juce::String (mCc));
            return;
        }
        juce::Slider::mouseDown (e);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;
        // Reset to the kit author's set_cc<N> default (parsed at loadKit).
        // Falls back to slider midpoint when no kit default is recorded.
        const int kitDef = (mEngine != nullptr) ? mEngine->getKitDefaultCc (mCc) : 64;
        setValue ((double) kitDef, juce::sendNotificationSync);
    }

private:
    BaySickRustyDrumsProcessor*                       mEngine;
    int                                                mCc;
    juce::Image                                        mStrip;
    int                                                mFrames;
    juce::Rectangle<float>                             mNativeRect;
    std::unique_ptr<juce::SliderParameterAttachment>   mAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AriaKnob)
};

// ── AriaOptionMenu ───────────────────────────────────────────────────────────
// ARIA's <OptionMenu> + <OptionItem name value>.  Each item has a normalized
// value01; CC = round(value01 * 127).  Built as an invisible Slider 0..127
// (so APVTS attachment, automation, undo, MIDI Learn all work) wrapped in a
// custom Component that paints the dropdown look + handles click-to-open.
class AriaOptionMenu : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    struct Item { juce::String name; float value01 { 0.5f }; };

    AriaOptionMenu (BaySickRustyDrumsProcessor* engine,
                    int cc, juce::Rectangle<float> nativeRect,
                    juce::Colour textCol, bool transparent)
        : mEngine (engine), mCc (cc), mNativeRect (nativeRect),
          mTextColor (textCol), mTransparent (transparent)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        // Hidden slider drives APVTS - never painted, never receives mouse.
        // addChildComponent (not addAndMakeVisible) keeps it parented without
        // setting visible=true; the slider's draw call therefore never fires.
        mHiddenSlider.setRange (0.0, 127.0, 1.0);
        mHiddenSlider.setSliderStyle (juce::Slider::LinearBar);
        mHiddenSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        mHiddenSlider.setInterceptsMouseClicks (false, false);
        addChildComponent (mHiddenSlider);
        mHiddenSlider.onValueChange = [this] { repaint(); };

        if (mEngine != nullptr)
        {
            const auto pid = "brd_cc" + juce::String (mCc);
            if (auto* p = mEngine->apvts.getParameter (pid))
                mAttach = std::make_unique<juce::SliderParameterAttachment> (*p, mHiddenSlider);
        }
    }

    void addItem (const juce::String& name, float value01)
    {
        mItems.push_back ({ name, value01 });
    }

    juce::Rectangle<float> getNativeRect() const { return mNativeRect; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        if (! mTransparent)
        {
            g.setColour (juce::Colour (0xff202020));
            g.fillRoundedRectangle (b, 2.0f);
        }
        g.setColour (mTextColor.withAlpha (0.6f));
        g.drawRoundedRectangle (b.reduced (0.5f), 2.0f, 1.0f);
        g.setColour (mTextColor);
        const float fontH = juce::jmax (8.0f, b.getHeight() * 0.6f);
        g.setFont (juce::Font (juce::FontOptions (fontH)));
        g.drawText (currentItemName(), b.reduced (4.0f, 0.0f),
                    juce::Justification::centred, true);
        const float cy = b.getCentreY();
        const float cx = b.getRight() - 7.0f;
        juce::Path chev;
        chev.addTriangle (cx - 3.0f, cy - 1.5f,
                          cx + 3.0f, cy - 1.5f,
                          cx,        cy + 2.0f);
        g.fillPath (chev);
    }

    void resized() override
    {
        mHiddenSlider.setBounds (getLocalBounds());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            showAriaParamPopup (mHiddenSlider, "brd_cc" + juce::String (mCc));
            return;
        }
        if (mItems.empty()) return;
        juce::PopupMenu menu;
        const int curIdx = currentIndex();
        for (size_t i = 0; i < mItems.size(); ++i)
            menu.addItem ((int) (i + 1), mItems[i].name, true,
                          /*ticked=*/(int) i == curIdx);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
            [this] (int r)
            {
                if (r <= 0 || r > (int) mItems.size()) return;
                const float v01 = mItems[(size_t) (r - 1)].value01;
                const int ccVal = juce::jlimit (0, 127, (int) std::round (v01 * 127.0f));
                mHiddenSlider.setValue ((double) ccVal, juce::sendNotificationSync);
                repaint();
            });
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        // Match knob behavior: double-click resets to kit default.
        const int kitDef = (mEngine != nullptr) ? mEngine->getKitDefaultCc (mCc) : 64;
        mHiddenSlider.setValue ((double) kitDef, juce::sendNotificationSync);
    }

    // Hover tooltip: kit's `label_cc<N>` + currently-selected option, plus a
    // beginner-friendly sentence about what this dropdown actually picks.
    juce::String getTooltip() override
    {
        juce::String label = (mEngine != nullptr) ? mEngine->getCcLabel (mCc) : juce::String();
        if (label.isEmpty()) label = "CC " + juce::String (mCc);
        const auto cur = currentItemName();
        juce::String tip = cur.isNotEmpty() ? (label + " -" + cur) : label;
        const auto help = beginnerExplanation (label);
        if (help.isNotEmpty()) tip += "\n\n" + help;
        return tip;
    }

private:
    int currentIndex() const
    {
        if (mItems.empty()) return -1;
        const float cur = (float) mHiddenSlider.getValue() / 127.0f;
        int best = 0;
        float bestDelta = std::abs (mItems[0].value01 - cur);
        for (size_t i = 1; i < mItems.size(); ++i)
        {
            const float d = std::abs (mItems[i].value01 - cur);
            if (d < bestDelta) { bestDelta = d; best = (int) i; }
        }
        return best;
    }

    juce::String currentItemName() const
    {
        const int idx = currentIndex();
        if (idx < 0) return {};
        return mItems[(size_t) idx].name;
    }

    BaySickRustyDrumsProcessor*                       mEngine;
    int                                                mCc;
    juce::Rectangle<float>                             mNativeRect;
    juce::Colour                                       mTextColor;
    bool                                               mTransparent;
    std::vector<Item>                                  mItems;
    juce::Slider                                       mHiddenSlider;
    std::unique_ptr<juce::SliderParameterAttachment>   mAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AriaOptionMenu)
};
} // namespace

// ── AriaControlPanel ─────────────────────────────────────────────────────────
AriaControlPanel::AriaControlPanel (BaySickRustyDrumsProcessor* engine)
    : mEngine (engine)
{
}

AriaControlPanel::~AriaControlPanel() = default;

void AriaControlPanel::clear()
{
    mWidgets.clear();
    mStaticTexts.clear();
    mStaticImages.clear();
    mBackgroundImage = juce::Image();
    mTabs.clear();
    mTabButtons.clear();
    mActiveTab = 0;
    repaint();
}

juce::Image AriaControlPanel::loadImage (const juce::String& filename)
{
    if (filename.isEmpty()) return {};
    if (auto it = mImageCache.find (filename); it != mImageCache.end())
        return it->second;

    const auto file = mGuiDir.getChildFile (filename);
    if (! file.existsAsFile()) return {};
    juce::Image img = juce::ImageFileFormat::loadFrom (file);
    if (img.isValid()) mImageCache[filename] = img;
    return img;
}

juce::Colour AriaControlPanel::parseColor (const juce::String& s, juce::Colour fallback)
{
    auto t = s.trim();
    if (! t.startsWithChar ('#') || t.length() < 7) return fallback;
    t = t.substring (1);
    // Big Rusty Drums uses #RRGGBBAA (the trailing FF is alpha).
    // Many ARIA kits do; if any kit ever uses #AARRGGBB we'd add a switch.
    if (t.length() == 8)
    {
        const auto rr = t.substring (0, 2).getHexValue32();
        const auto gg = t.substring (2, 4).getHexValue32();
        const auto bb = t.substring (4, 6).getHexValue32();
        const auto aa = t.substring (6, 8).getHexValue32();
        return juce::Colour ((juce::uint8) rr, (juce::uint8) gg,
                             (juce::uint8) bb, (juce::uint8) aa);
    }
    if (t.length() == 6)
    {
        const auto rr = t.substring (0, 2).getHexValue32();
        const auto gg = t.substring (2, 4).getHexValue32();
        const auto bb = t.substring (4, 6).getHexValue32();
        return juce::Colour ((juce::uint8) rr, (juce::uint8) gg, (juce::uint8) bb);
    }
    return fallback;
}

void AriaControlPanel::loadFromKit (const juce::File& kitRoot, const juce::File& programXml)
{
    clear();
    if (! programXml.existsAsFile()) return;

    mGuiDir = kitRoot.getChildFile ("GUI");

    // Build the tab list.  "Main" = the active program XML; subsequent tabs
    // are the 03-08 zoom-in XMLs (Kick / Snare / Toms / Hi-hat / Cymbals /
    // Noises) auto-discovered by filename in the same folder.
    mTabs.push_back ({ "Main", programXml });
    struct ZoomTab { const char* prefix; const char* label; };
    constexpr ZoomTab zoomTabs[] = {
        { "03-kick",    "Kick"    },
        { "04-snare",   "Snare"   },
        { "05-toms",    "Toms"    },
        { "06-hihat",   "Hi-hat"  },
        { "07-cymbals", "Cymbals" },
        { "08-noises",  "Noises"  },
    };
    for (const auto& zt : zoomTabs)
    {
        const auto f = mGuiDir.getChildFile (juce::String (zt.prefix) + ".xml");
        if (f.existsAsFile()) mTabs.push_back ({ zt.label, f });
    }

    rebuildTabBar();
    selectTab (0);
}

void AriaControlPanel::rebuildTabBar()
{
    mTabButtons.clear();
    for (size_t i = 0; i < mTabs.size(); ++i)
    {
        auto btn = std::make_unique<juce::TextButton> (mTabs[i].label);
        const int idx = (int) i;
        // No radio group / no auto-toggle: JUCE's turnOffOtherButtonsInGroup
        // walks parent children AFTER onClick, but our onClick rebuilds child
        // widgets via parseGuiXml - that mid-walk mutation crashes.  Manage
        // toggle state manually inside selectTab via setToggleState only.
        btn->setClickingTogglesState (false);
        btn->setToggleState (idx == mActiveTab, juce::dontSendNotification);
        // Defer the heavy work to the next message cycle so the click handler
        // returns cleanly before we tear down + rebuild this panel's children.
        juce::Component::SafePointer<AriaControlPanel> safe (this);
        btn->onClick = [safe, idx]
        {
            juce::MessageManager::callAsync ([safe, idx]
            {
                if (safe) safe->selectTab (idx);
            });
        };
        addAndMakeVisible (*btn);
        mTabButtons.push_back (std::move (btn));
    }
}

void AriaControlPanel::selectTab (int tabIdx)
{
    if (tabIdx < 0 || tabIdx >= (int) mTabs.size()) return;
    mActiveTab = tabIdx;
    for (size_t i = 0; i < mTabButtons.size(); ++i)
        if (mTabButtons[i])
            mTabButtons[i]->setToggleState ((int) i == mActiveTab, juce::dontSendNotification);
    parseGuiXml (mTabs[(size_t) tabIdx].xmlFile);
    resized();
    repaint();
}

void AriaControlPanel::parseGuiXml (const juce::File& xmlFile)
{
    // Reset only the rendered content; tab strip + tab list stay intact.
    mWidgets.clear();
    mStaticTexts.clear();
    mStaticImages.clear();
    mBackgroundImage = juce::Image();

    if (! xmlFile.existsAsFile()) return;
    auto xml = juce::XmlDocument::parse (xmlFile);
    if (xml == nullptr || ! xml->hasTagName ("GUI")) return;

    mNativeW = xml->getIntAttribute ("w", 775);
    mNativeH = xml->getIntAttribute ("h", 335);

    for (auto* el : xml->getChildIterator())
    {
        if (el == nullptr) continue;

        const juce::String tag = el->getTagName();
        const float x = (float) el->getDoubleAttribute ("x");
        const float y = (float) el->getDoubleAttribute ("y");
        const float w = (float) el->getDoubleAttribute ("w");
        const float h = (float) el->getDoubleAttribute ("h");
        const juce::Rectangle<float> nativeRect (x, y, w, h);

        if (tag == "StaticImage")
        {
            const auto imageName = el->getStringAttribute ("image");
            const bool transparent = el->getBoolAttribute ("transparent", false);

            if (x <= 0.5f && y <= 0.5f
                && w >= (float) mNativeW - 4.0f && h >= (float) mNativeH - 4.0f)
            {
                mBackgroundImage = loadImage (imageName);
            }
            else
            {
                StaticImagePiece p;
                p.nativeRect  = nativeRect;
                p.imageName   = imageName;
                p.transparent = transparent;
                mStaticImages.push_back (std::move (p));
            }
        }
        else if (tag == "StaticText")
        {
            StaticText st;
            st.nativeRect    = nativeRect;
            st.text          = el->getStringAttribute ("text");
            st.color         = parseColor (el->getStringAttribute ("color_text"),
                                           juce::Colours::white);
            st.transparent   = el->getBoolAttribute ("transparent", true);
            st.smallFont     = el->getStringAttribute ("font_size").equalsIgnoreCase ("Small");
            st.justification = juce::Justification::centred;
            mStaticTexts.push_back (std::move (st));
        }
        else if (tag == "Knob")
        {
            const int cc       = el->getIntAttribute ("param", -1);
            const auto imgName = el->getStringAttribute ("image");
            const int frames   = el->getIntAttribute ("frames", 128);
            if (cc < 0) continue;

            const auto strip = loadImage (imgName);

            float knobW = w, knobH = h;
            if (knobW <= 0.5f && strip.isValid())
                knobW = (float) strip.getWidth();
            if (knobH <= 0.5f && strip.isValid())
                knobH = (float) (strip.getHeight() / juce::jmax (1, frames));
            const juce::Rectangle<float> kr (x, y, knobW, knobH);

            auto knob = std::make_unique<AriaKnob> (mEngine, cc, strip, frames, kr);
            addAndMakeVisible (*knob);
            mWidgets.push_back (std::move (knob));
        }
        else if (tag == "OptionMenu")
        {
            const int  cc = el->getIntAttribute ("param", -1);
            const auto col = parseColor (el->getStringAttribute ("color_text"),
                                         juce::Colours::white);
            const bool transparent = el->getBoolAttribute ("transparent", true);
            if (cc < 0) continue;

            auto menu = std::make_unique<AriaOptionMenu> (mEngine, cc, nativeRect,
                                                          col, transparent);
            for (auto* it : el->getChildIterator())
            {
                if (it == nullptr || ! it->hasTagName ("OptionItem")) continue;
                menu->addItem (it->getStringAttribute ("name"),
                               (float) it->getDoubleAttribute ("value", 0.5));
            }
            addAndMakeVisible (*menu);
            mWidgets.push_back (std::move (menu));
        }
        // Unknown elements are silently ignored.
    }
}

// J-8 stage 2 (2026-05-04): aspect-locked fit to the entire available area
// below the tab strip.  No cap - the kit grows to whatever the page allows.
static juce::Rectangle<float> computePanelDrawArea (juce::Rectangle<float> outer,
                                                     int nativeW, int nativeH,
                                                     int tabBarH)
{
    auto available = outer.withTrimmedTop ((float) tabBarH);
    if (available.isEmpty() || nativeW <= 0 || nativeH <= 0) return {};

    const float aspect = (float) nativeW / (float) nativeH;
    const float fitW = juce::jmin (available.getWidth(),  available.getHeight() * aspect);
    const float fitH = juce::jmin (available.getHeight(), available.getWidth()  / aspect);
    const float originX = available.getCentreX() - fitW * 0.5f;
    const float originY = available.getCentreY() - fitH * 0.5f;
    return { originX, originY, fitW, fitH };
}

void AriaControlPanel::resized()
{
    const auto b = getLocalBounds().toFloat();
    if (b.isEmpty()) return;

    // Lay out the tab bar at top - equal-width buttons spanning the panel width.
    if (! mTabButtons.empty())
    {
        const int n = (int) mTabButtons.size();
        const int totalW = (int) b.getWidth();
        // Cap each button to a reasonable width so the strip doesn't stretch
        // edge-to-edge on huge windows; left-align if the total would be less.
        const int maxBtnW = 110;
        const int btnW = juce::jmin (maxBtnW, juce::jmax (40, totalW / juce::jmax (1, n)));
        const int stripW = btnW * n;
        const int stripX = juce::jmax (0, (totalW - stripW) / 2);
        for (int i = 0; i < n; ++i)
            if (mTabButtons[(size_t) i])
                mTabButtons[(size_t) i]->setBounds (stripX + i * btnW, 0, btnW - 2, kTabBarHeight);
    }

    if (mNativeW <= 0 || mNativeH <= 0) return;
    const auto drawArea = computePanelDrawArea (b, mNativeW, mNativeH, kTabBarHeight);
    const float sx = drawArea.getWidth()  / (float) mNativeW;
    const float sy = drawArea.getHeight() / (float) mNativeH;

    auto toComp = [&] (juce::Rectangle<float> nr) -> juce::Rectangle<int>
    {
        return juce::Rectangle<float> (drawArea.getX() + nr.getX() * sx,
                                       drawArea.getY() + nr.getY() * sy,
                                       nr.getWidth() * sx,
                                       nr.getHeight() * sy).toNearestInt();
    };

    for (auto& w : mWidgets)
    {
        if (auto* k = dynamic_cast<AriaKnob*>(w.get()))
            k->setBounds (toComp (k->getNativeRect()));
        else if (auto* m = dynamic_cast<AriaOptionMenu*>(w.get()))
            m->setBounds (toComp (m->getNativeRect()));
    }
}

void AriaControlPanel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    if (b.isEmpty() || mNativeW <= 0 || mNativeH <= 0) return;

    const auto drawArea = computePanelDrawArea (b, mNativeW, mNativeH, kTabBarHeight);
    const float fitW = drawArea.getWidth();
    const float fitH = drawArea.getHeight();
    const float originX = drawArea.getX();
    const float originY = drawArea.getY();

    if (mBackgroundImage.isValid())
        g.drawImage (mBackgroundImage, drawArea, juce::RectanglePlacement::stretchToFit);
    else
    {
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillRect (drawArea);
        g.setColour (juce::Colours::grey);
        g.setFont (juce::Font (juce::FontOptions (16.0f)));
        g.drawText ("Loading control surface...", drawArea, juce::Justification::centred);
        return;
    }

    const float sx = fitW / (float) mNativeW;
    const float sy = fitH / (float) mNativeH;
    auto toComp = [&] (juce::Rectangle<float> nr) -> juce::Rectangle<float>
    {
        return { originX + nr.getX() * sx,
                 originY + nr.getY() * sy,
                 nr.getWidth()  * sx,
                 nr.getHeight() * sy };
    };

    for (const auto& sip : mStaticImages)
    {
        const auto img = loadImage (sip.imageName);
        if (img.isValid())
            g.drawImage (img, toComp (sip.nativeRect),
                         juce::RectanglePlacement::stretchToFit);
    }

    // ARIA's font_size="Small" maps to roughly 9pt at native, "default" to ~11pt.
    // Multiply by the current scale factor so downscaled rendering stays
    // proportional.  Clamped at 7pt to keep labels legible.
    const float fontScale = sy;   // sx == sy because we kept aspect locked
    const float smallFont   = juce::jmax (7.0f, 9.0f  * fontScale);
    const float defaultFont = juce::jmax (8.0f, 11.0f * fontScale);

    for (const auto& st : mStaticTexts)
    {
        const auto rect = toComp (st.nativeRect);
        if (! st.transparent)
        {
            g.setColour (juce::Colour (0xff181818));
            g.fillRect (rect);
        }
        g.setColour (st.color);
        g.setFont (juce::Font (juce::FontOptions (st.smallFont ? smallFont : defaultFont)));
        g.drawText (st.text, rect, st.justification, true);
    }
}
