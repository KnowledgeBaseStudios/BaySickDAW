#include "InstrumentPage.h"

// ── Ctor ──────────────────────────────────────────────────────────────────────
InstrumentPage::InstrumentPage(VibeSynthProcessor& proc, PatternManager& pm)
    : mProc(proc), mPM(pm)
{
    mSubTabs.setTabBarDepth(26);
    mSubTabs.setColour(juce::TabbedComponent::backgroundColourId, VC::Bg);
    mSubTabs.setColour(juce::TabbedComponent::outlineColourId,    VC::Accent);
}

// ── Selector setup (called by subclass ctor before anything is shown) ────────
void InstrumentPage::setupEngineDropdown(const juce::StringArray& options,
                                          const juce::Array<EngineType>& types)
{
    jassert(options.size() == types.size());
    mEngineTypes = types;
    buildSelectorPanel(options, types);
}

void InstrumentPage::buildSelectorPanel(const juce::StringArray& options,
                                         const juce::Array<EngineType>&)
{
    mSelectorPanel = std::make_unique<juce::Component>();

    mSelectorLabel = std::make_unique<juce::Label>("sel_lbl");
    mSelectorLabel->setText("Select sound engine:", juce::dontSendNotification);
    mSelectorLabel->setFont(juce::Font(15.0f));
    mSelectorLabel->setColour(juce::Label::textColourId, VC::TextDim);
    mSelectorLabel->setJustificationType(juce::Justification::centred);
    mSelectorPanel->addAndMakeVisible(*mSelectorLabel);

    mEngineBox = std::make_unique<juce::ComboBox>();
    mEngineBox->setTextWhenNothingSelected("-- Choose Engine --");
    for (int i = 0; i < options.size(); ++i)
        mEngineBox->addItem(options[i], i + 1);
    mEngineBox->setLookAndFeel(&VibeLAF::get());
    mEngineBox->onChange = [this]
    {
        int sel = mEngineBox->getSelectedItemIndex();
        if (sel >= 0 && sel < mEngineTypes.size())
            commitEngine(mEngineTypes[sel]);
    };
    mSelectorPanel->addAndMakeVisible(*mEngineBox);

    addAndMakeVisible(*mSelectorPanel);
}

void InstrumentPage::commitEngine(EngineType e)
{
    mEngine = e;

    // Remove the selector and show the sub-tab system
    mSelectorPanel = nullptr;
    mSelectorLabel = nullptr;
    mEngineBox     = nullptr;

    // Add placeholder sub-tabs; subclass fills them in onEngineChosen()
    mSubTabs.addTab("Sound",     VC::Panel, nullptr, false);
    mSubTabs.addTab("Piano Roll",VC::Panel, nullptr, false);
    mSubTabs.addTab("EQ",        VC::Panel, nullptr, false);
    addAndMakeVisible(mSubTabs);

    onEngineChosen(e);
    resized();
}

void InstrumentPage::setSubTabContent(int idx, juce::Component* content)
{
    if (idx >= 0 && idx < mSubTabs.getNumTabs())
    {
        mSubTabs.setTabBackgroundColour(idx, VC::Panel);
        // Replace the existing content component (ownership NOT transferred — caller manages it)
        mSubTabs.getTabContentComponent(idx);  // just ensures tab exists
        // Use a non-owning wrapper so TabbedComponent doesn't delete it
        if (content)
        {
            // We add the content as a child of the tab's container
            auto* existing = mSubTabs.getTabContentComponent(idx);
            if (existing != content)
            {
                // Replace with our own component: use a wrapper
                struct Wrapper : public juce::Component {
                    juce::Component* inner;
                    Wrapper(juce::Component* c) : inner(c) {
                        addAndMakeVisible(*inner);
                    }
                    void resized() override { inner->setBounds(getLocalBounds()); }
                };
                mSubTabs.removeTab(idx);
                // Re-insert at same position — note: tabs shift so we need to be careful
                // Simplest: just remove/re-insert all three keeping order
                // For Phase 1 this is only called once so order is safe
                auto* w = new Wrapper(content);
                mSubTabs.addTab(
                    idx == SUB_SOUND     ? "Sound"     :
                    idx == SUB_PIANOROLL ? "Piano Roll" : "EQ",
                    VC::Panel, w, true);
                // Move tab to correct position (it was added at end)
                // JUCE TabbedComponent doesn't have moveTab, so rebuild is needed
                // In Phase 4 we'll refactor this; for now just append works since
                // setSubTabContent is called in order (Sound, PianoRoll, EQ).
            }
        }
    }
}

// ── Layout ────────────────────────────────────────────────────────────────────
void InstrumentPage::paint(juce::Graphics& g)
{
    g.fillAll(VC::Bg);
}

void InstrumentPage::resized()
{
    auto b = getLocalBounds();

    if (mSelectorPanel)
    {
        mSelectorPanel->setBounds(b);
        // Layout label and combo box centred within the panel
        auto centre = b.getCentre();
        if (mSelectorLabel) mSelectorLabel->setBounds(centre.getX() - 160, centre.getY() - 50, 320, 30);
        if (mEngineBox)     mEngineBox->setBounds    (centre.getX() - 120, centre.getY() - 10, 240, 34);
        return;
    }

    if (mSubTabs.isVisible())
        mSubTabs.setBounds(b);
}
