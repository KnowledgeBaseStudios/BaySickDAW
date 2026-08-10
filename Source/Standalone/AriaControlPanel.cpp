#include "AriaControlPanel.h"
#include "SharedUI.h"   // VKnobAutomation popup glue

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

// Helpers (local to this TU) for engine-agnostic CC lookups.  Each takes the
// per-panel binding by const ref + the CC index; degrades gracefully when
// closures aren't wired (kit-default → 64; label → empty; param ID → empty).
static juce::String ccParamId (const AriaControlPanel::Binding& b, int cc)
{
    return b.ccParamId ? b.ccParamId (cc) : juce::String();
}
static int ccKitDefault (const AriaControlPanel::Binding& b, int cc)
{
    return b.kitDefaultCc ? b.kitDefaultCc (cc) : 64;
}
static juce::String ccLabel (const AriaControlPanel::Binding& b, int cc)
{
    return b.ccLabel ? b.ccLabel (cc) : juce::String();
}

// ── AriaKnob ─────────────────────────────────────────────────────────────────
// Slider subclass with vertical-filmstrip paint.  APVTS-bound via
// SliderParameterAttachment so right-click → Type Value, automation, MIDI
// Learn, and undo/redo all work natively.  Double-click resets to the kit's
// set_cc<N> default rather than the slider midpoint.
class AriaKnob : public juce::Slider
{
public:
    AriaKnob (AriaControlPanel::Binding binding,
              int cc, juce::Image strip, int frames,
              juce::Rectangle<float> nativeRect)
        : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag,
                         juce::Slider::NoTextBox),
          mBinding (std::move (binding)), mCc (cc),
          mStrip (std::move (strip)), mFrames (juce::jmax (1, frames)),
          mNativeRect (nativeRect)
    {
        setRange (0.0, 127.0, 1.0);
        setMouseDragSensitivity (200);
        setVelocityBasedMode (false);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        // Value popup during drag (current CC value bubbles above the knob).
        setPopupDisplayEnabled (true, true, nullptr);

        if (mBinding.apvts != nullptr)
        {
            const auto pid = ccParamId (mBinding, mCc);
            if (pid.isNotEmpty())
                if (auto* p = mBinding.apvts->getParameter (pid))
                    // Regression fix (2026-08-06): the manager arg was omitted
                    // (defaults nullptr) -- no gesture transaction ever began,
                    // so knob turns bled into the previous history entry.
                    mAttach = std::make_unique<juce::SliderParameterAttachment> (
                                  *p, *this, mBinding.apvts->undoManager);
        }
    }

    juce::String getTooltip() override
    {
        juce::String label = ccLabel (mBinding, mCc);
        if (label.isEmpty()) label = "CC " + juce::String (mCc);
        juce::String tip = label + " -" + juce::String ((int) getValue());
        const auto help = beginnerExplanation (label);
        if (help.isNotEmpty()) tip += "\n\n" + help;
        return tip;
    }

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
            showAriaParamPopup (*this, ccParamId (mBinding, mCc));
            return;
        }
        juce::Slider::mouseDown (e);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;
        const int kitDef = ccKitDefault (mBinding, mCc);
        setValue ((double) kitDef, juce::sendNotificationSync);
    }

private:
    AriaControlPanel::Binding                          mBinding;
    int                                                mCc;
    juce::Image                                        mStrip;
    int                                                mFrames;
    juce::Rectangle<float>                             mNativeRect;
    std::unique_ptr<juce::SliderParameterAttachment>   mAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AriaKnob)
};

// ── AriaSlider (K-5 fix #2, 2026-05-05) ─────────────────────────────────────
// ARIA's <Slider> element - vertical fader with a slot background image and
// a separate handle image.  Same CC mapping as AriaKnob (0..127) but linear-
// vertical instead of rotary.  bg fills the bounds; handle is drawn at a
// position computed from the current value.
class AriaSlider : public juce::Slider
{
public:
    AriaSlider (AriaControlPanel::Binding binding,
                int cc, juce::Image bg, juce::Image handle,
                juce::Rectangle<float> nativeRect)
        : juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox),
          mBinding (std::move (binding)), mCc (cc),
          mBg (std::move (bg)), mHandle (std::move (handle)),
          mNativeRect (nativeRect)
    {
        setRange (0.0, 127.0, 1.0);
        setMouseDragSensitivity (200);
        setVelocityBasedMode (false);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setPopupDisplayEnabled (true, true, nullptr);

        if (mBinding.apvts != nullptr)
        {
            const auto pid = ccParamId (mBinding, mCc);
            if (pid.isNotEmpty())
                if (auto* p = mBinding.apvts->getParameter (pid))
                    // Regression fix (2026-08-06): the manager arg was omitted
                    // (defaults nullptr) -- no gesture transaction ever began,
                    // so knob turns bled into the previous history entry.
                    mAttach = std::make_unique<juce::SliderParameterAttachment> (
                                  *p, *this, mBinding.apvts->undoManager);
        }
    }

    juce::String getTooltip() override
    {
        juce::String label = ccLabel (mBinding, mCc);
        if (label.isEmpty()) label = "CC " + juce::String (mCc);
        juce::String tip = label + " -" + juce::String ((int) getValue());
        const auto help = beginnerExplanation (label);
        if (help.isNotEmpty()) tip += "\n\n" + help;
        return tip;
    }

    juce::String getTextFromValue (double value) override
    {
        return juce::String ((int) std::round (value));
    }

    juce::Rectangle<float> getNativeRect() const { return mNativeRect; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        if (b.isEmpty()) return;

        // Background slot (fills the entire bounds, stretched to fit).
        if (mBg.isValid())
            g.drawImage (mBg, b, juce::RectanglePlacement::stretchToFit);
        else
        {
            g.setColour (juce::Colour (0xff202020));
            g.fillRect (b);
        }

        if (! mHandle.isValid()) return;

        // Handle: drawn at native handle dimensions scaled by the slot's
        // height ratio - preserves the kit author's intended aspect.  Position
        // along Y by normalized value (high = top).
        const double normalized = (getValue() - getMinimum())
                                 / juce::jmax (1.0, (getMaximum() - getMinimum()));
        const float scale     = b.getHeight() / juce::jmax (1.0f, mNativeRect.getHeight());
        const float handleW   = (float) mHandle.getWidth()  * scale;
        const float handleH   = (float) mHandle.getHeight() * scale;
        const float trackTop  = b.getY();
        const float trackBot  = b.getBottom() - handleH;
        const float handleY   = trackTop + (1.0f - (float) normalized) * (trackBot - trackTop);
        const float handleX   = b.getCentreX() - handleW * 0.5f;
        g.drawImage (mHandle,
                     juce::Rectangle<float> (handleX, handleY, handleW, handleH),
                     juce::RectanglePlacement::stretchToFit);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            showAriaParamPopup (*this, ccParamId (mBinding, mCc));
            return;
        }
        juce::Slider::mouseDown (e);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;
        const int kitDef = ccKitDefault (mBinding, mCc);
        setValue ((double) kitDef, juce::sendNotificationSync);
    }

private:
    AriaControlPanel::Binding                          mBinding;
    int                                                mCc;
    juce::Image                                        mBg;
    juce::Image                                        mHandle;
    juce::Rectangle<float>                             mNativeRect;
    std::unique_ptr<juce::SliderParameterAttachment>   mAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AriaSlider)
};

// ── AriaToggle (K-5 fix #2, 2026-05-05) ─────────────────────────────────────
// ARIA's <OnOffButton> element - 2-frame button (off / on) bound to a CC.
// Click toggles between 0 (off) and 127 (on).  Image is treated as a 2-frame
// vertical filmstrip (top half = off, bottom half = on); single-frame images
// degrade gracefully.
class AriaToggle : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    AriaToggle (AriaControlPanel::Binding binding,
                int cc, juce::Image strip,
                juce::Rectangle<float> nativeRect)
        : mBinding (std::move (binding)), mCc (cc),
          mStrip (std::move (strip)), mNativeRect (nativeRect)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        mHiddenSlider.setRange (0.0, 127.0, 1.0);
        mHiddenSlider.setSliderStyle (juce::Slider::LinearBar);
        mHiddenSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        mHiddenSlider.setInterceptsMouseClicks (false, false);
        addChildComponent (mHiddenSlider);
        mHiddenSlider.onValueChange = [this] { repaint(); };

        if (mBinding.apvts != nullptr)
        {
            const auto pid = ccParamId (mBinding, mCc);
            if (pid.isNotEmpty())
                if (auto* p = mBinding.apvts->getParameter (pid))
                    // Regression fix (2026-08-06): same omitted-manager fix as
                    // the AriaKnob attachment above.
                    mAttach = std::make_unique<juce::SliderParameterAttachment> (
                                  *p, mHiddenSlider, mBinding.apvts->undoManager);
        }
    }

    juce::Rectangle<float> getNativeRect() const { return mNativeRect; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        if (b.isEmpty()) return;
        const bool isOn = mHiddenSlider.getValue() > 63.5;

        if (mStrip.isValid())
        {
            // Treat as 2-frame vertical filmstrip when the height is >= twice
            // the width (typical for ARIA toggle PNGs).  Otherwise fall back
            // to single-frame draw.
            const int sw = mStrip.getWidth();
            const int sh = mStrip.getHeight();
            const bool twoFrame = sh >= sw * 2;
            const int frameH = twoFrame ? sh / 2 : sh;
            const int srcY   = (twoFrame && isOn) ? frameH : 0;
            g.drawImage (mStrip,
                         (int) b.getX(), (int) b.getY(),
                         (int) b.getWidth(), (int) b.getHeight(),
                         0, srcY, sw, frameH);
        }
        else
        {
            g.setColour (isOn ? juce::Colour (0xff33ff88) : juce::Colour (0xff333333));
            g.fillRoundedRectangle (b, 3.0f);
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
        }
    }

    void resized() override
    {
        mHiddenSlider.setBounds (getLocalBounds());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            showAriaParamPopup (mHiddenSlider, ccParamId (mBinding, mCc));
            return;
        }
        const bool isOn = mHiddenSlider.getValue() > 63.5;
        mHiddenSlider.setValue (isOn ? 0.0 : 127.0, juce::sendNotificationSync);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        const int kitDef = ccKitDefault (mBinding, mCc);
        mHiddenSlider.setValue ((double) kitDef, juce::sendNotificationSync);
    }

    juce::String getTooltip() override
    {
        juce::String label = ccLabel (mBinding, mCc);
        if (label.isEmpty()) label = "CC " + juce::String (mCc);
        const bool isOn = mHiddenSlider.getValue() > 63.5;
        return label + (isOn ? " -ON" : " -OFF");
    }

private:
    AriaControlPanel::Binding                          mBinding;
    int                                                mCc;
    juce::Image                                        mStrip;
    juce::Rectangle<float>                             mNativeRect;
    BaySickSlider                                         mHiddenSlider;
    std::unique_ptr<juce::SliderParameterAttachment>   mAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AriaToggle)
};

// ── AriaOptionMenu ───────────────────────────────────────────────────────────
class AriaOptionMenu : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    struct Item { juce::String name; float value01 { 0.5f }; };

    AriaOptionMenu (AriaControlPanel::Binding binding,
                    int cc, juce::Rectangle<float> nativeRect,
                    juce::Colour textCol, bool transparent)
        : mBinding (std::move (binding)), mCc (cc), mNativeRect (nativeRect),
          mTextColor (textCol), mTransparent (transparent)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        mHiddenSlider.setRange (0.0, 127.0, 1.0);
        mHiddenSlider.setSliderStyle (juce::Slider::LinearBar);
        mHiddenSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        mHiddenSlider.setInterceptsMouseClicks (false, false);
        addChildComponent (mHiddenSlider);
        mHiddenSlider.onValueChange = [this] { repaint(); };

        if (mBinding.apvts != nullptr)
        {
            const auto pid = ccParamId (mBinding, mCc);
            if (pid.isNotEmpty())
                if (auto* p = mBinding.apvts->getParameter (pid))
                    // Regression fix (2026-08-06): same omitted-manager fix as
                    // the AriaKnob attachment above.
                    mAttach = std::make_unique<juce::SliderParameterAttachment> (
                                  *p, mHiddenSlider, mBinding.apvts->undoManager);
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
            showAriaParamPopup (mHiddenSlider, ccParamId (mBinding, mCc));
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
        const int kitDef = ccKitDefault (mBinding, mCc);
        mHiddenSlider.setValue ((double) kitDef, juce::sendNotificationSync);
    }

    juce::String getTooltip() override
    {
        juce::String label = ccLabel (mBinding, mCc);
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

    AriaControlPanel::Binding                          mBinding;
    int                                                mCc;
    juce::Rectangle<float>                             mNativeRect;
    juce::Colour                                       mTextColor;
    bool                                               mTransparent;
    std::vector<Item>                                  mItems;
    BaySickSlider                                         mHiddenSlider;
    std::unique_ptr<juce::SliderParameterAttachment>   mAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AriaOptionMenu)
};
} // namespace

// ── AriaControlPanel ─────────────────────────────────────────────────────────
AriaControlPanel::AriaControlPanel (Binding binding)
    : mBinding (std::move (binding))
{
    // QA-A (2026-05-09): host an internal BaySickTitleBar when the binding
    // supplies an engine name.  Used by InstPage (Guitars / Basses) and
    // BaySickRustyDrumsPage to give each engine its own title bar through
    // this shared kit-artwork panel.
    if (mBinding.engineName.isNotEmpty() || mBinding.hostTitleBar)
    {
        mTitleBar = std::make_unique<BaySickTitleBar> (mBinding.engineName,
                                                        mBinding.accentColor);
        addAndMakeVisible (*mTitleBar);
    }
}

AriaControlPanel::~AriaControlPanel() = default;

void AriaControlPanel::setEngine (Binding binding)
{
    mBinding = std::move (binding);

    // QA-A (2026-05-09): create / update / drop the internal title bar to
    // match the new binding.
    if (mBinding.engineName.isNotEmpty() || mBinding.hostTitleBar)
    {
        if (! mTitleBar)
        {
            mTitleBar = std::make_unique<BaySickTitleBar> (mBinding.engineName,
                                                            mBinding.accentColor);
            addAndMakeVisible (*mTitleBar);
        }
        else
        {
            mTitleBar->setEngineName  (mBinding.engineName);
            mTitleBar->setAccentColor (mBinding.accentColor);
        }
    }
    else
    {
        mTitleBar.reset();
    }
    resized();
}

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
    // Noises) auto-discovered by filename in the same folder.  Drum kits use
    // these zoom-in pages; melodic guitar / bass kits typically don't ship
    // them, in which case only the "Main" tab appears.
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
    // K-5 fix #3 (2026-05-05): when the kit ships only the main program XML
    // (no 03-08 zoom-in pages), suppress the tab strip entirely - a single
    // "Main" button takes vertical space without offering a choice.  The
    // resized() / paint() codepaths skip the tab-bar reservation when there
    // are no buttons, so the panel reclaims the full area below the toolbar.
    if (mTabs.size() <= 1) return;
    for (size_t i = 0; i < mTabs.size(); ++i)
    {
        auto btn = std::make_unique<juce::TextButton> (mTabs[i].label);
        const int idx = (int) i;
        btn->setClickingTogglesState (false);
        btn->setToggleState (idx == mActiveTab, juce::dontSendNotification);
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

            auto knob = std::make_unique<AriaKnob> (mBinding, cc, strip, frames, kr);
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

            auto menu = std::make_unique<AriaOptionMenu> (mBinding, cc, nativeRect,
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
        else if (tag == "Slider")
        {
            // K-5 fix #2 (2026-05-05): vertical fader.  bg = slot image,
            // handle = thumb image.  Guitar / bass kits use this for vibrato
            // depth, mute, feedback, releases - every CC that the kit author
            // wants represented as a fader instead of a knob.
            const int  cc = el->getIntAttribute ("param", -1);
            if (cc < 0) continue;
            const auto bgName     = el->getStringAttribute ("image_bg");
            const auto handleName = el->getStringAttribute ("image_handle");
            const auto bg     = loadImage (bgName);
            const auto handle = loadImage (handleName);

            // Use the XML-declared rect verbatim - the fader's native size IS
            // its slot dimensions (e.g. w=20 h=128 for vertical guitar faders).
            auto slider = std::make_unique<AriaSlider> (mBinding, cc, bg, handle, nativeRect);
            addAndMakeVisible (*slider);
            mWidgets.push_back (std::move (slider));
        }
        else if (tag == "OnOffButton")
        {
            // K-5 fix #2: 2-frame toggle button (CC=0 off, CC=127 on).
            const int cc = el->getIntAttribute ("param", -1);
            if (cc < 0) continue;
            const auto imgName = el->getStringAttribute ("image");
            const auto strip = loadImage (imgName);

            auto toggle = std::make_unique<AriaToggle> (mBinding, cc, strip, nativeRect);
            addAndMakeVisible (*toggle);
            mWidgets.push_back (std::move (toggle));
        }
        // Unknown elements are silently ignored.
    }
}

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
    // Centre-anchor the ratio-locked artwork in the available area.  Earlier
    // QA-A 4.5 work briefly top-anchored this, but the combination of
    // top-anchor + tabBarH = 0 (overlay model, see resized()) pinned the
    // artwork flush against the title bar with no breathing room, leaving
    // nowhere to drag the tab strip without overlapping the kit's top row
    // (kick / snare / toms labels).  Centre-anchor restores the original
    // visual position; the draggable tab strip rides on top via
    // mTabStripNativeOffset.
    const float originY = available.getCentreY() - fitH * 0.5f;
    return { originX, originY, fitW, fitH };
}

void AriaControlPanel::resized()
{
    // QA-A (2026-05-09): if a title bar is present, anchor it at the top of
    // the panel and trim the remainder for the kit artwork.
    const int titleBarH = mTitleBar ? BaySickTitleBar::kStandardHeight : 0;
    if (mTitleBar)
        mTitleBar->setBounds (0, 0, getWidth(), titleBarH);

    const auto b = getLocalBounds().withTrimmedTop (titleBarH).toFloat();
    if (b.isEmpty()) return;
    if (mNativeW <= 0 || mNativeH <= 0) return;

    // QA-A 4.5 (2026-05-09): kit artwork claims the full available area below
    // the title bar; the tab strip overlays the artwork at a user-positioned
    // anchor (mTabStripNativeOffset, expressed in native artwork coords so
    // the placement survives window resizes).  Pass tabBarH = 0 so the
    // artwork no longer reserves a strip-height band at the top.
    const auto drawArea = computePanelDrawArea (b, mNativeW, mNativeH, 0);
    const float sx = drawArea.getWidth()  / (float) mNativeW;
    const float sy = drawArea.getHeight() / (float) mNativeH;

    if (! mTabButtons.empty())
    {
        const int n = (int) mTabButtons.size();
        if (mTitleBar != nullptr)
        {
            // Jeff 2026-08-06: with a title band present, the section tab row
            // (Main / Kick / Snare / ...) lives IN the band -- the Program +
            // Player Preset controls that used to sit there moved up to the
            // WINDOW title strip, freeing the band's full width.  Centered,
            // vertically middled in the 32px row.
            const int maxBtnW = 110;
            const int btnW = juce::jmin (maxBtnW,
                                          juce::jmax (40, getWidth() / juce::jmax (1, n)));
            const int stripW = btnW * n;
            const int stripX = (getWidth() - stripW) / 2;
            const int stripY = (titleBarH - kTabBarHeight) / 2;
            for (int i = 0; i < n; ++i)
                if (mTabButtons[(size_t) i])
                {
                    mTabButtons[(size_t) i]->setBounds (stripX + i * btnW, stripY,
                                                         btnW - 2, kTabBarHeight);
                    // The band is a sibling child -- keep the buttons above it.
                    mTabButtons[(size_t) i]->toFront (false);
                }
        }
        else
        {
            // Band-less hosts (Guitars/Basses InstPage panels): keep the
            // artwork-overlay placement.
            //   X: artwork center + mTabStripNativeOffset.x (native coords
            //      scaled to pixels) -- centred on the artwork when offset is 0.
            //   Y: artwork top + mTabStripNativeOffset.y (native coords scaled
            //      to pixels) -- flush at the top when offset is 0.
            const int maxBtnW = 110;
            const int btnW = juce::jmin (maxBtnW,
                                          juce::jmax (40,
                                                       (int) drawArea.getWidth()
                                                           / juce::jmax (1, n)));
            const int stripW  = btnW * n;
            const int stripCx = (int) drawArea.getCentreX()
                              + (int) std::lround ((float) mTabStripNativeOffset.x * sx);
            const int stripX  = stripCx - stripW / 2;
            const int stripY  = (int) drawArea.getY()
                              + (int) std::lround ((float) mTabStripNativeOffset.y * sy);
            for (int i = 0; i < n; ++i)
                if (mTabButtons[(size_t) i])
                {
                    mTabButtons[(size_t) i]->setBounds (stripX + i * btnW, stripY,
                                                         btnW - 2, kTabBarHeight);
                    // QA-A 4.5: tab strip overlays kit artwork + widgets, so the
                    // buttons must paint on top of them.  Widgets are added to
                    // the child list AFTER tab buttons (rebuildTabBar() runs
                    // before parseGuiXml() in selectTab()), so by default they'd
                    // sit visually on top.  Move each tab button to the front
                    // here.  Buttons don't overlap each other.
                    mTabButtons[(size_t) i]->toFront (false);
                }
        }
    }

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
        else if (auto* s = dynamic_cast<AriaSlider*>(w.get()))
            s->setBounds (toComp (s->getNativeRect()));
        else if (auto* t = dynamic_cast<AriaToggle*>(w.get()))
            t->setBounds (toComp (t->getNativeRect()));
    }
}

void AriaControlPanel::paint (juce::Graphics& g)
{
    // QA-A (2026-05-09): mirror resized() -- if a title bar is present, trim
    // its space off the top before computing the kit-artwork area.  The
    // title bar itself paints separately as a child component.
    const int titleBarH = mTitleBar ? BaySickTitleBar::kStandardHeight : 0;
    const auto b = getLocalBounds().withTrimmedTop (titleBarH).toFloat();
    if (b.isEmpty() || mNativeW <= 0 || mNativeH <= 0) return;

    // QA-A 4.5 (2026-05-09): tab strip overlays the artwork (see resized()),
    // so the artwork claims the full available area below the title bar.
    const auto drawArea = computePanelDrawArea (b, mNativeW, mNativeH, 0);
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

    const float fontScale = sy;
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
