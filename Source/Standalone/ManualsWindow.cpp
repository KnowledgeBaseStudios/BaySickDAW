#include "ManualsWindow.h"
#include "WindowChrome.h"
#include "SharedUI.h"   // namespace VC (the app palette)

namespace
{
    // Shown instead of a blank browser when <exe dir>/Manuals/index.html is not
    // there.  A clean install before the manuals are staged is a real state and
    // an empty frame reads as a broken window, so it is named plainly.
    class ManualsMissingView : public juce::Component
    {
    public:
        explicit ManualsMissingView (juce::String reason) : mReason (std::move (reason))
        {
            mOpenFolder.setButtonText ("Open Manuals Folder");
            mOpenFolder.onClick = []
            {
                auto dir = ManualsWindow::manualsIndexFile().getParentDirectory();
                dir.createDirectory();
                dir.revealToUser();
            };
            addAndMakeVisible (mOpenFolder);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (VC::Bg);
            g.setColour (VC::Text);
            g.setFont (juce::FontOptions (16.0f));
            g.drawFittedText (mReason, getLocalBounds().reduced (28).removeFromTop (140),
                              juce::Justification::centredTop, 8);
        }

        void resized() override
        {
            mOpenFolder.setBounds (getLocalBounds().withSizeKeepingCentre (200, 30)
                                                   .withY (getHeight() / 2 + 30));
        }

    private:
        juce::String   mReason;
        juce::TextButton mOpenFolder;
    };
}

juce::File ManualsWindow::manualsIndexFile()
{
    // The manual is ONE document since the 2026-08-13 unification; F1 lands
    // straight in it (Jeff: the landing page was a pointless extra step).
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile)
               .getParentDirectory()
               .getChildFile ("Manuals")
               .getChildFile ("manual.html");
}

ManualsWindow::ManualsWindow()
    : juce::DocumentWindow ("BaySickDAW Manuals",
                            VC::Bg,
                            juce::DocumentWindow::allButtons)
{
    WindowChrome::applyToDesktopWindow (*this);   // TS7 section 9.3
    setResizable (true, true);

    const auto index = manualsIndexFile();

    if (! index.existsAsFile())
    {
        setContentOwned (new ManualsMissingView (
            "The manuals are not installed.\n\n"
            "They belong at:\n" + index.getFullPathName() + "\n\n"
            "A full install places them there. If you are running a development "
            "build, the manuals are built separately and staged into that folder."),
            true);
    }
    else
    {
       #if JUCE_USE_WIN_WEBVIEW2
        // Backend::webview2 is explicit: JUCE defaults to the IE engine on
        // Windows even when the macro is on, and IE cannot lay out the
        // callout overlays the atlas depends on.
        auto opts = juce::WebBrowserComponent::Options{}
                        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2);
        auto* browser = new juce::WebBrowserComponent (opts);
       #else
        auto* browser = new juce::WebBrowserComponent();
       #endif

        // A bare Windows path is not a URL -- WebView2 wants file:///C:/...,
        // which juce::URL builds from the File.
        juce::String url = juce::URL (index).toString (false);
       #if JUCE_DEBUG
        // Authoring-only: the page renders its nudge bar solely when ASKED
        // (?nudge=1), and only the Debug build asks -- Release and every
        // installed copy can never surface it (Jeff, 2026-08-16).
        url << "?nudge=1";
       #endif
        browser->goToURL (url);
        setContentOwned (browser, true);
    }

    // Maximized by default (Jeff, 2026-08-14); the centreWithSize is the
    // restore size the maximize button toggles back to.
    centreWithSize (1100, 800);
    setFullScreen (true);
    setVisible (true);
    toFront (true);
}

void ManualsWindow::closeButtonPressed()
{
    delete this;
}
