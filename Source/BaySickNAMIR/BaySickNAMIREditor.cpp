#include "BaySickNAMIREditor.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickNAMIREditor — Phase G-1.4 implementation.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    constexpr int kEditorW       = 760;
    constexpr int kEditorH       = 340;
    constexpr int kHeaderH       = 28;
    constexpr int kFileRowH      = 50;
    constexpr int kKnobsRowH     = 130;
    constexpr int kPad           = 12;
    constexpr int kSlotABtnW     = 28;
    constexpr int kSlotABtnY     = 4;
    constexpr int kSlotABtnH     = 20;

    constexpr juce::uint32 kAmpBgARGB    = 0xff222222;
    constexpr juce::uint32 kCabBgARGB    = 0xff181818;
    constexpr juce::uint32 kHeaderBgARGB = 0xff0a0a0a;
    constexpr juce::uint32 kDividerARGB  = 0xff333333;
    constexpr juce::uint32 kAmberARGB    = 0xffffb84d;   // NAM "LCD" amber
    constexpr juce::uint32 kCabGreenARGB = 0xff66ff88;   // IR "LCD" green
    constexpr juce::uint32 kHintARGB     = 0xffffaa33;
    constexpr juce::uint32 kErrARGB      = 0xffff5555;
}

BaySickNAMIREditor::BaySickNAMIREditor (BaySickNAMIRProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor (p),
      mInGainKnob       ("Input Gain",  0.0f,
        "Pre-NAM gain (-24..+24 dB).  Tweak so the model is hit at the level it was captured at."),
      mGateThreshKnob   ("Gate Thr",   -50.0f,
        "Noise gate threshold (-60..0 dB).  Anything below this gets gated to silence."),
      mGateReleaseKnob  ("Gate Rel",   100.0f,
        "Noise gate release (5..500 ms).  How long the gate takes to close after the signal drops below threshold."),
      mLowCutKnob       ("Low Cut",     20.0f,
        "Pre-cab high-pass (20..500 Hz).  Tightens the low end before the IR."),
      mHighCutKnob      ("High Cut", 20000.0f,
        "Pre-cab low-pass (3k..20k Hz).  Tames fizz and brightness before the IR."),
      mCabMixKnob       ("Cab Mix",    100.0f,
        "Wet / dry mix on the cabinet IR (0..100 %).  100 = full cab; 0 = none."),
      mOutputKnob       ("Output",      0.0f,
        "Master output (-24..+12 dB).  Adjust to match levels with other inserts.")
{
    setSize (kEditorW, kEditorH);
    setWantsKeyboardFocus (false);

    // ── Header ───────────────────────────────────────────────────────────────
    addAndMakeVisible (mTitleLabel);
    mTitleLabel.setText ("BaySickNAM/IR", juce::dontSendNotification);
    mTitleLabel.setJustificationType (juce::Justification::centredLeft);
    mTitleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    mTitleLabel.setTooltip (
        "BaySickNAM/IR - Neural Amp Modeler + IR cabinet engine.  "
        "Loads .nam amp captures and .wav cabinet impulse responses.");

    auto wireSlotBtn = [this] (juce::TextButton& btn, int slot,
                               juce::Button::ConnectedEdgeFlags edge,
                               const juce::String& tip)
    {
        addAndMakeVisible (btn);
        btn.setRadioGroupId (1);
        btn.setClickingTogglesState (true);
        btn.setConnectedEdges (edge);
        btn.setTooltip (tip);
        btn.onClick = [this, slot, &btn]()
        {
            if (btn.getToggleState())
                setActiveSlotFromUI (slot);
        };
    };
    wireSlotBtn (mSlotABtn, 0, juce::Button::ConnectedOnRight,
        "Slot A.  Loads A's NAM model + IR.  Click B to A/B compare two amp setups.");
    wireSlotBtn (mSlotBBtn, 1, juce::Button::ConnectedOnLeft,
        "Slot B.  Loads B's NAM model + IR.  Click A to swap back.");

    // ── Section labels ───────────────────────────────────────────────────────
    auto setupSectionLbl = [] (juce::Label& l, const juce::String& txt, const juce::String& tip)
    {
        l.setText (txt, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId, juce::Colour (0xffd0d0d0));
        l.setTooltip (tip);
    };
    setupSectionLbl (mAmpSectionLbl, "AMP",
        "Amp section - loads a .nam neural-network amp capture.  "
        "Run the dry guitar through here to colour it like the captured amp.");
    setupSectionLbl (mCabSectionLbl, "CAB",
        "Cab section - loads a .wav impulse response of a guitar cabinet.  "
        "The convolution sits after the amp.  Bypass when using a Full-Rig NAM.");
    addAndMakeVisible (mAmpSectionLbl);
    addAndMakeVisible (mCabSectionLbl);

    // ── File pickers + filename labels ───────────────────────────────────────
    addAndMakeVisible (mNamBrowseBtn);
    addAndMakeVisible (mIrBrowseBtn);
    mNamBrowseBtn.setButtonText ("Load .nam...");
    mIrBrowseBtn .setButtonText ("Load .wav IR...");
    mNamBrowseBtn.onClick      = [this]() { browseForNamFile(); };
    mIrBrowseBtn .onClick      = [this]() { browseForIrFile();  };
    mNamBrowseBtn.onRightClick = [this]() { showRecentNamMenu(); };
    mIrBrowseBtn .onRightClick = [this]() { showRecentIrMenu();  };
    mNamBrowseBtn.setTooltip ("Left-click: open file browser.  Right-click: recent files.");
    mIrBrowseBtn .setTooltip ("Left-click: open file browser.  Right-click: recent IRs.");

    addAndMakeVisible (mNamFileLabel);
    addAndMakeVisible (mIrFileLabel);
    auto setupLcd = [] (juce::Label& l, juce::Colour textC, const juce::String& tip)
    {
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId,        textC);
        l.setColour (juce::Label::backgroundColourId,  juce::Colour (0xff1a1a1a));
        l.setColour (juce::Label::outlineColourId,     juce::Colour (0xff333333));
        l.setBorderSize ({ 2, 6, 2, 6 });
        l.setTooltip (tip);
    };
    setupLcd (mNamFileLabel, juce::Colour (kAmberARGB),
        "Currently-loaded .nam model for this slot.  Drag a .nam file here, or use the Browse button.");
    setupLcd (mIrFileLabel,  juce::Colour (kCabGreenARGB),
        "Currently-loaded .wav IR for this slot.  Drag a .wav file here, or use the Browse button.");

    // ── Bypass toggles (Named mode: OFF on top, ON on bottom; white labels)
    addAndMakeVisible (mNamBypassToggle);
    mNamBypassToggle.setupNamed (
        "OFF", "Switch up = NAM bypass OFF.  The neural amp model is processing audio normally.",
        "ON",  "Switch down = NAM bypass ON.  The neural amp model is skipped - dry input passes straight to the filters / cab.");
    mNamBypassToggle.setLabelColour (juce::Colours::white);
    mNamBypassToggle.btn().setTooltip (
        "NAM bypass.  Skips the neural amp model entirely.");
    mNamBypassAtt = std::make_unique<ButtonAtt> (
                        processor.apvts, "nam_bypass", mNamBypassToggle.btn());

    addAndMakeVisible (mCabBypassToggle);
    mCabBypassToggle.setupNamed (
        "OFF", "Switch up = Cab bypass OFF.  The IR cabinet convolution is on.",
        "ON",  "Switch down = Cab bypass ON.  The IR cabinet is skipped - useful when running a Full-Rig NAM that already includes a cab.");
    mCabBypassToggle.setLabelColour (juce::Colours::white);
    mCabBypassToggle.btn().setTooltip (
        "Cab bypass.  Skips the IR cabinet convolution.");
    mCabBypassAtt = std::make_unique<ButtonAtt> (
                        processor.apvts, "cab_bypass", mCabBypassToggle.btn());

    // ── Knobs + APVTS attachments ────────────────────────────────────────────
    auto bindKnob = [&] (VKnob& k, const char* paramId, std::unique_ptr<SliderAtt>& att)
    {
        addAndMakeVisible (k);
        k.paramId = paramId;
        att = std::make_unique<SliderAtt> (processor.apvts, paramId, k.slider);
    };
    bindKnob (mInGainKnob,      "input_gain",     mInGainAtt);
    bindKnob (mGateThreshKnob,  "gate_threshold", mGateThreshAtt);
    bindKnob (mGateReleaseKnob, "gate_release",   mGateReleaseAtt);
    bindKnob (mLowCutKnob,      "low_cut",        mLowCutAtt);
    bindKnob (mHighCutKnob,     "high_cut",       mHighCutAtt);
    bindKnob (mCabMixKnob,      "cab_mix",        mCabMixAtt);
    bindKnob (mOutputKnob,      "output",         mOutputAtt);

    // ── OS selector (chicken head + manual APVTS sync) ───────────────────────
    addAndMakeVisible (mOSSelector);
    mOSSelector.setOptions ({
        { "1", "1x", "No oversampling.  Fastest." },
        { "2", "2x", "2x oversampling.  Reduces aliasing on high-gain models." },
        { "4", "4x", "4x oversampling.  Best quality.  CPU-heavy." },
    });
    mOSSelector.setBodyTooltip (
        "Oversampling factor around the NAM block.  Higher = less aliasing on saturated tones.");
    if (auto* p = processor.apvts.getRawParameterValue ("oversampling"))
        mOSSelector.setSelectedIndex ((int) p->load(), juce::dontSendNotification);
    mOSSelector.onChange = [this] (int idx)
    {
        if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (
                            processor.apvts.getParameter ("oversampling")))
            *prm = idx;
    };

    addAndMakeVisible (mOSSelectorLbl);
    mOSSelectorLbl.setText ("OS", juce::dontSendNotification);
    mOSSelectorLbl.setJustificationType (juce::Justification::centredTop);
    mOSSelectorLbl.setColour (juce::Label::textColourId, juce::Colour (0xffd0d0d0));
    mOSSelectorLbl.setTooltip (
        "Oversampling factor.  1x = no OS (cheapest).  2x / 4x reduce aliasing on saturated tones at the cost of CPU.");

    // ── Status row ───────────────────────────────────────────────────────────
    addAndMakeVisible (mFullRigHint);
    mFullRigHint.setText ({}, juce::dontSendNotification);
    mFullRigHint.setColour (juce::Label::textColourId, juce::Colour (kHintARGB));
    mFullRigHint.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (mErrorLabel);
    mErrorLabel.setText ({}, juce::dontSendNotification);
    mErrorLabel.setColour (juce::Label::textColourId, juce::Colour (kErrARGB));
    mErrorLabel.setJustificationType (juce::Justification::centredRight);

    // ── A/B + listener — initial sync ────────────────────────────────────────
    processor.apvts.addParameterListener ("ab_slot",       this);
    processor.apvts.addParameterListener ("oversampling",  this);

    const int slot0 = processor.getActiveSlot();
    mSlotABtn.setToggleState (slot0 == 0, juce::dontSendNotification);
    mSlotBBtn.setToggleState (slot0 == 1, juce::dontSendNotification);

    updateLabels();
}

BaySickNAMIREditor::~BaySickNAMIREditor()
{
    processor.apvts.removeParameterListener ("ab_slot",      this);
    processor.apvts.removeParameterListener ("oversampling", this);
}

// ─────────────────────────────────────────────────────────────────────────────
// Painting + layout.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIREditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kCabBgARGB));   // single dark faceplate

    // Header strip.
    g.setColour (juce::Colour (kHeaderBgARGB));
    g.fillRect (0, 0, getWidth(), kHeaderH);

    // Header bottom divider only.
    g.setColour (juce::Colour (kDividerARGB));
    g.fillRect (0, kHeaderH, getWidth(), 1);
}

void BaySickNAMIREditor::resized()
{
    // ── Header ───────────────────────────────────────────────────────────────
    mTitleLabel.setBounds (8, 0, 240, kHeaderH);
    const int slotsRight = getWidth() - kPad;
    mSlotBBtn.setBounds (slotsRight - kSlotABtnW,         kSlotABtnY, kSlotABtnW, kSlotABtnH);
    mSlotABtn.setBounds (slotsRight - 2 * kSlotABtnW,     kSlotABtnY, kSlotABtnW, kSlotABtnH);

    // ── File rows ────────────────────────────────────────────────────────────
    constexpr int sectionLblW = 36;
    constexpr int browseBtnW  = 110;
    constexpr int bypassW     = 70;
    constexpr int rowGap      = 4;

    auto layoutFileRow = [&] (int y, juce::Label& sectionLbl, juce::Button& browseBtn,
                              juce::Label& fileLbl, DualLabelToggle& bypassTog)
    {
        const int textY = y + 14;
        const int textH = 22;
        sectionLbl.setBounds (kPad, y, sectionLblW, kFileRowH);
        browseBtn .setBounds (kPad + sectionLblW + rowGap, textY, browseBtnW, textH);
        const int fileLblX = kPad + sectionLblW + rowGap + browseBtnW + rowGap;
        const int fileLblW = getWidth() - fileLblX - bypassW - rowGap - kPad;
        fileLbl   .setBounds (fileLblX, textY, fileLblW, textH);
        bypassTog .setBounds (getWidth() - bypassW - kPad, y + 2, bypassW, kFileRowH - 4);
    };
    layoutFileRow (kHeaderH,                 mAmpSectionLbl, mNamBrowseBtn, mNamFileLabel, mNamBypassToggle);
    layoutFileRow (kHeaderH + kFileRowH,     mCabSectionLbl, mIrBrowseBtn,  mIrFileLabel,  mCabBypassToggle);

    // ── Knobs row ────────────────────────────────────────────────────────────
    const int knobsY     = kHeaderH + 2 * kFileRowH + 8;
    const int slotsCount = 8;          // 7 knobs + OS selector
    const int knobsAreaW = getWidth() - 2 * kPad;
    const int slotW      = knobsAreaW / slotsCount;

    int slotX = kPad;
    auto placeKnob = [&] (VKnob& k)
    {
        k.setBounds (slotX, knobsY, slotW, kKnobsRowH);
        slotX += slotW;
    };
    placeKnob (mInGainKnob);
    placeKnob (mGateThreshKnob);
    placeKnob (mGateReleaseKnob);
    placeKnob (mLowCutKnob);
    placeKnob (mHighCutKnob);
    placeKnob (mCabMixKnob);
    placeKnob (mOutputKnob);

    // OS chicken head — center the 66×66 dial inside the last slot, label below.
    const int dialSz   = 66;
    const int dialX    = slotX + (slotW - dialSz) / 2;
    const int dialY    = knobsY + 4;
    mOSSelector  .setBounds (dialX, dialY, dialSz, dialSz);
    mOSSelectorLbl.setBounds (slotX, dialY + dialSz + 4, slotW, 16);

    // ── Status row ───────────────────────────────────────────────────────────
    const int statusY = knobsY + kKnobsRowH + 8;
    mFullRigHint.setBounds (kPad, statusY, getWidth() / 2 - kPad, 22);
    mErrorLabel .setBounds (getWidth() / 2, statusY, getWidth() / 2 - kPad, 22);
}

// ─────────────────────────────────────────────────────────────────────────────
// APVTS listener.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIREditor::parameterChanged (const juce::String& paramID, float newValue)
{
    if (paramID == "ab_slot")
    {
        const int slot = juce::jlimit (0, 1, (int) newValue);
        juce::MessageManager::callAsync ([this, slot]()
        {
            mSlotABtn.setToggleState (slot == 0, juce::dontSendNotification);
            mSlotBBtn.setToggleState (slot == 1, juce::dontSendNotification);
            updateLabels();
        });
    }
    else if (paramID == "oversampling")
    {
        const int idx = juce::jlimit (0, 2, (int) newValue);
        juce::MessageManager::callAsync ([this, idx]()
        {
            mOSSelector.setSelectedIndex (idx, juce::dontSendNotification);
        });
    }
}

void BaySickNAMIREditor::setActiveSlotFromUI (int slot)
{
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (
                        processor.apvts.getParameter ("ab_slot")))
        *prm = juce::jlimit (0, 1, slot);
    updateLabels();
}

void BaySickNAMIREditor::updateLabels()
{
    const int slot = processor.getActiveSlot();

    const juce::String namPath = processor.getNamFilePath (slot);
    const juce::String irPath  = processor.getIrFilePath  (slot);

    mNamFileLabel.setText (namPath.isNotEmpty()
                               ? juce::File (namPath).getFileName()
                               : juce::String ("(no model loaded)"),
                           juce::dontSendNotification);
    mIrFileLabel .setText (irPath .isNotEmpty()
                               ? juce::File (irPath).getFileName()
                               : juce::String ("(no IR loaded)"),
                           juce::dontSendNotification);

    if (processor.hasNamModel (slot) && processor.isFullRig (slot))
        mFullRigHint.setText (
            "Full-rig model - cabinet already included.  Consider bypassing the IR.",
            juce::dontSendNotification);
    else
        mFullRigHint.setText ({}, juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────────────────────────
// File browse + drag-drop.
// ─────────────────────────────────────────────────────────────────────────────
void BaySickNAMIREditor::browseForNamFile()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Choose a .nam model",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.nam");

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        const juce::File f = fc.getResult();
        if (! f.existsAsFile()) return;
        juce::String err;
        if (processor.loadNamModel (f.getFullPathName(), err))
        {
            pushRecent ("nam", f.getFullPathName());
            mErrorLabel.setText ({}, juce::dontSendNotification);
            updateLabels();
        }
        else
        {
            mErrorLabel.setText (err, juce::dontSendNotification);
            showError ("Couldn't load NAM model:\n" + err);
        }
    });
}

void BaySickNAMIREditor::browseForIrFile()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Choose a .wav IR",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.wav");

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        const juce::File f = fc.getResult();
        if (! f.existsAsFile()) return;
        juce::String err;
        if (processor.loadImpulseResponse (f.getFullPathName(), err))
        {
            pushRecent ("ir", f.getFullPathName());
            mErrorLabel.setText ({}, juce::dontSendNotification);
            updateLabels();
        }
        else
        {
            mErrorLabel.setText (err, juce::dontSendNotification);
            showError ("Couldn't load IR:\n" + err);
        }
    });
}

void BaySickNAMIREditor::showError (const juce::String& msg)
{
    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                            "BaySickNAM/IR", msg, "OK");
}

bool BaySickNAMIREditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".nam") || f.endsWithIgnoreCase (".wav"))
            return true;
    return false;
}

void BaySickNAMIREditor::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    juce::String lastErr;
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase (".nam"))
        {
            juce::String err;
            if (processor.loadNamModel (f, err))
                pushRecent ("nam", f);
            else
                lastErr = err;
        }
        else if (f.endsWithIgnoreCase (".wav"))
        {
            juce::String err;
            if (processor.loadImpulseResponse (f, err))
                pushRecent ("ir", f);
            else
                lastErr = err;
        }
    }
    if (lastErr.isNotEmpty())
    {
        mErrorLabel.setText (lastErr, juce::dontSendNotification);
        showError ("Couldn't load dropped file:\n" + lastErr);
    }
    else
    {
        mErrorLabel.setText ({}, juce::dontSendNotification);
    }
    updateLabels();
}

// ─────────────────────────────────────────────────────────────────────────────
// Recent files menu.
// ─────────────────────────────────────────────────────────────────────────────
juce::File BaySickNAMIREditor::settingsFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                  .getChildFile ("BaySickDAW");
    if (! dir.exists()) dir.createDirectory();
    return dir.getChildFile ("settings.xml");
}

juce::StringArray BaySickNAMIREditor::loadRecents (const juce::String& tag)
{
    juce::StringArray out;
    const juce::File f = settingsFile();
    if (! f.existsAsFile()) return out;

    if (auto xml = juce::XmlDocument::parse (f))
    {
        const juce::String childTag = (tag == "nam") ? "RecentNAMFiles" : "RecentIRFiles";
        if (auto* root = xml->getChildByName (childTag))
            for (auto* item = root->getChildByName ("File"); item != nullptr;
                 item = item->getNextElementWithTagName ("File"))
                out.add (item->getStringAttribute ("path"));
    }
    return out;
}

void BaySickNAMIREditor::saveRecents (const juce::String& tag, const juce::StringArray& list)
{
    const juce::File f = settingsFile();
    std::unique_ptr<juce::XmlElement> root;
    if (f.existsAsFile())
        root = juce::XmlDocument::parse (f);
    if (! root)
        root = std::make_unique<juce::XmlElement> ("Settings");

    const juce::String childTag = (tag == "nam") ? "RecentNAMFiles" : "RecentIRFiles";
    if (auto* existing = root->getChildByName (childTag))
        root->removeChildElement (existing, true);

    auto* section = root->createNewChildElement (childTag);
    for (const auto& p : list)
    {
        auto* item = section->createNewChildElement ("File");
        item->setAttribute ("path", p);
    }

    root->writeTo (f);
}

void BaySickNAMIREditor::pushRecent (const juce::String& tag, const juce::String& path)
{
    juce::StringArray list = loadRecents (tag);
    list.removeString (path, true);     // dedupe (case-insensitive on Windows is fine)
    list.insert (0, path);
    while (list.size() > 10) list.remove (list.size() - 1);
    saveRecents (tag, list);
}

void BaySickNAMIREditor::showRecentNamMenu()
{
    juce::PopupMenu m;
    auto recents = loadRecents ("nam");
    if (recents.isEmpty())
    {
        m.addItem (1, "(no recent .nam files)", false);
    }
    else
    {
        for (int i = 0; i < recents.size(); ++i)
            m.addItem (100 + i, juce::File (recents[i]).getFileName());
        m.addSeparator();
        m.addItem (1, "Clear recent");
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mNamBrowseBtn),
        [this, recents] (int result)
        {
            if (result == 1)
            {
                saveRecents ("nam", {});
            }
            else if (result >= 100 && result < 100 + recents.size())
            {
                juce::String err;
                const juce::String path = recents[result - 100];
                if (processor.loadNamModel (path, err))
                {
                    pushRecent ("nam", path);
                    mErrorLabel.setText ({}, juce::dontSendNotification);
                    updateLabels();
                }
                else
                {
                    mErrorLabel.setText (err, juce::dontSendNotification);
                    showError ("Couldn't load NAM model:\n" + err);
                }
            }
        });
}

void BaySickNAMIREditor::showRecentIrMenu()
{
    juce::PopupMenu m;
    auto recents = loadRecents ("ir");
    if (recents.isEmpty())
    {
        m.addItem (1, "(no recent IRs)", false);
    }
    else
    {
        for (int i = 0; i < recents.size(); ++i)
            m.addItem (100 + i, juce::File (recents[i]).getFileName());
        m.addSeparator();
        m.addItem (1, "Clear recent");
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mIrBrowseBtn),
        [this, recents] (int result)
        {
            if (result == 1)
            {
                saveRecents ("ir", {});
            }
            else if (result >= 100 && result < 100 + recents.size())
            {
                juce::String err;
                const juce::String path = recents[result - 100];
                if (processor.loadImpulseResponse (path, err))
                {
                    pushRecent ("ir", path);
                    mErrorLabel.setText ({}, juce::dontSendNotification);
                    updateLabels();
                }
                else
                {
                    mErrorLabel.setText (err, juce::dontSendNotification);
                    showError ("Couldn't load IR:\n" + err);
                }
            }
        });
}
