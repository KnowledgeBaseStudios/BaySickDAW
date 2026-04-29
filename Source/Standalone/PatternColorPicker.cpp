#include "PatternColorPicker.h"
#include "SharedUI.h"   // VC palette

namespace PatternColorPicker
{
namespace
{
    juce::File getSettingsFile()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("BaySickDAW");
        if (! dir.exists()) dir.createDirectory();
        return dir.getChildFile ("settings.xml");
    }

    // ── Subclass that exposes the persisted recent list as the swatch row. ─
    class ColourSelectorWithRecents : public juce::ColourSelector
    {
    public:
        ColourSelectorWithRecents()
            : juce::ColourSelector (juce::ColourSelector::showColourAtTop
                                    | juce::ColourSelector::showColourspace
                                    | juce::ColourSelector::showSliders
                                    | juce::ColourSelector::editableColour),
              mRecents (loadRecents())
        {
            // Pad the list out to kMaxRecents so the swatch row always shows
            // 10 cells, with empty/transparent slots until they fill in.
            mRecents.resize (kMaxRecents, juce::Colour(0));
        }

        int  getNumSwatches() const override { return kMaxRecents; }

        juce::Colour getSwatchColour (int index) const override
        {
            if (index < 0 || index >= (int) mRecents.size()) return juce::Colour (0);
            return mRecents[(size_t) index];
        }

        // Called when the user right-clicks a swatch + picks "Set this colour".
        // Default JUCE behaviour: write the current colour into the slot.  We
        // honour that — slot becomes a manually-pinned colour — and persist.
        void setSwatchColour (int index, const juce::Colour& newColour) override
        {
            if (index < 0 || index >= (int) mRecents.size()) return;
            mRecents[(size_t) index] = newColour;
            // Persist the FULL list (including empty slots) so the slot order
            // matches what the user sees.
            saveRecents (mRecents);
        }

    private:
        std::vector<juce::Colour> mRecents;
    };

    // ── Modal window — owns the selector + drives the live-update callback. ─
    class PickerWindow : public juce::DialogWindow,
                         public juce::ChangeListener
    {
    public:
        PickerWindow (juce::Colour current,
                      std::function<void(juce::Colour)> onColourChanged)
            : juce::DialogWindow ("Pattern Color", VC::Bg,
                                  juce::DocumentWindow::closeButton),
              mOnChanged (std::move (onColourChanged))
        {
            setUsingNativeTitleBar (true);
            setResizable (false, false);

            auto* sel = new ColourSelectorWithRecents();
            sel->setSize (380, 520);
            sel->setCurrentColour (current, juce::dontSendNotification);
            sel->addChangeListener (this);
            mSelector = sel;

            setContentOwned (sel, true);
            centreWithSize (380, 520);
            setVisible (true);
            setAlwaysOnTop (true);
            toFront (true);

            mLastColour = current;
        }

        ~PickerWindow() override
        {
            // Only commit the closing colour to the recent list — every
            // intermediate hue drag would otherwise spam the slots.
            if (mLastColour.getARGB() != 0)
                pushRecent (mLastColour);
        }

        void changeListenerCallback (juce::ChangeBroadcaster* src) override
        {
            if (src == mSelector && mSelector != nullptr)
            {
                mLastColour = mSelector->getCurrentColour();
                if (mOnChanged) mOnChanged (mLastColour);
            }
        }

        void closeButtonPressed() override { delete this; }

    private:
        ColourSelectorWithRecents* mSelector { nullptr };
        std::function<void(juce::Colour)> mOnChanged;
        juce::Colour mLastColour;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PickerWindow)
    };
} // anonymous

// ─────────────────────────────────────────────────────────────────────────────
std::vector<juce::Colour> loadRecents()
{
    std::vector<juce::Colour> out;
    auto f = getSettingsFile();
    if (! f.existsAsFile()) return out;

    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return out;

    auto* node = xml->getChildByName ("RecentPatternColors");
    if (node == nullptr) return out;

    for (auto* e : node->getChildIterator())
    {
        if (! e->hasTagName ("Color")) continue;
        const int argb = e->getIntAttribute ("argb", 0);
        out.push_back (juce::Colour ((juce::uint32) argb));
        if ((int) out.size() >= kMaxRecents) break;
    }
    return out;
}

void saveRecents (const std::vector<juce::Colour>& recents)
{
    auto f = getSettingsFile();
    f.getParentDirectory().createDirectory();

    // Read existing root (preserve other sections written by ProjectManager).
    std::unique_ptr<juce::XmlElement> root;
    if (f.existsAsFile())
        root = juce::XmlDocument::parse (f);
    if (root == nullptr)
        root = std::make_unique<juce::XmlElement> ("BaySickDAWSettings");

    root->removeChildElement (root->getChildByName ("RecentPatternColors"), true);
    auto* node = root->createNewChildElement ("RecentPatternColors");
    int written = 0;
    for (auto& c : recents)
    {
        if (written >= kMaxRecents) break;
        // Skip empty/transparent slots — keeps the file tidy.
        if (c.getARGB() == 0) continue;
        auto* e = node->createNewChildElement ("Color");
        e->setAttribute ("argb", (int) c.getARGB());
        ++written;
    }
    root->writeTo (f);
}

void pushRecent (juce::Colour c)
{
    if (c.getARGB() == 0) return;
    auto recents = loadRecents();

    // Dedup — drop any prior occurrence so the new color rises to the front.
    recents.erase (std::remove_if (recents.begin(), recents.end(),
                                   [&] (const juce::Colour& x) { return x == c; }),
                   recents.end());
    recents.insert (recents.begin(), c);
    if ((int) recents.size() > kMaxRecents)
        recents.resize (kMaxRecents);
    saveRecents (recents);
}

void showAsync (juce::Component* /*anchor*/,
                juce::Colour current,
                std::function<void(juce::Colour)> onColourChanged)
{
    new PickerWindow (current, std::move (onColourChanged));
    // Window self-deletes on close.
}

} // namespace PatternColorPicker
