#include "ClipsPage.h"
#include "../VibePlayer/VibePlayerProcessor.h"
#include "../BaySickNAMIR/BaySickNAMIRProcessor.h"

namespace
{
    constexpr int kHeaderRowH = 36;     // engine-picker row at top of page
    constexpr int kPad        = 12;
    constexpr int kPickerW    = 220;
    constexpr int kFilenameW  = 320;
}

// ─────────────────────────────────────────────────────────────────────────────
// ClipsPage
// ─────────────────────────────────────────────────────────────────────────────

ClipsPage::ClipsPage (int pageIndex)
    : mPageIndex (pageIndex),
      mTabName   ("Clip " + juce::String (pageIndex + 1))
{
    buildEnginePicker();
    buildEqStub();

    addAndMakeVisible (mClipFileLabel);
    mClipFileLabel.setJustificationType (juce::Justification::centredLeft);
    mClipFileLabel.setColour (juce::Label::textColourId,        juce::Colour (0xff66ff88));
    mClipFileLabel.setColour (juce::Label::backgroundColourId,  juce::Colour (0xff1a1a1a));
    mClipFileLabel.setColour (juce::Label::outlineColourId,     juce::Colour (0xff333333));
    mClipFileLabel.setBorderSize ({ 2, 6, 2, 6 });
    mClipFileLabel.setText ("(no clip)", juce::dontSendNotification);
    mClipFileLabel.setTooltip ("The audio file this Clips tab is bound to.  "
                                "Set by drag/drop onto Builder or onto the Clips empty-state page.");

    switchTab (0);
}

ClipsPage::~ClipsPage() = default;

void ClipsPage::buildEnginePicker()
{
    addAndMakeVisible (mEnginePicker);
    mEnginePicker.setTextWhenNothingSelected ("Pick an engine");
    mEnginePicker.addItem ("BaySickPlayer", 1);
    mEnginePicker.addItem ("BaySickNAM/IR", 2);
    mEnginePicker.setTooltip (
        "Pick how the clip plays back.  BaySickPlayer = sampler (piano-roll "
        "notes pitch-shift the clip).  BaySickNAM/IR = re-amp through a "
        "neural amp model + IR cabinet.");
    mEnginePicker.onChange = [this]()
    {
        const int id = mEnginePicker.getSelectedId();
        if (id == 1)      selectEngine (EngineType::BaySickPlayer);
        else if (id == 2) selectEngine (EngineType::BaySickNAMIR);
    };
}

void ClipsPage::buildEqStub()
{
    mEqStub = std::make_unique<juce::Label>();
    mEqStub->setText ("Pre-clip EQ - coming in a later polish pass.\n"
                       "For now, use the post-rack EQ on the mixer audio strip.",
                      juce::dontSendNotification);
    mEqStub->setJustificationType (juce::Justification::centred);
    mEqStub->setColour (juce::Label::textColourId, juce::Colour (0xff909090));
    addChildComponent (*mEqStub);
}

// ─────────────────────────────────────────────────────────────────────────────
// Engine swap (G-3 2026-04-28: dual-engine A/B-style — both processors are
// lazy-created on first selection and KEPT ALIVE across swaps so each
// retains its APVTS state and the audio thread never sees a dangling ptr).
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessor* ClipsPage::getEngineProcessor() const noexcept
{
    return mEngineType == EngineType::BaySickPlayer ? mPlayerProc.get()
         : mEngineType == EngineType::BaySickNAMIR  ? mNamIrProc .get()
         :                                            nullptr;
}

void ClipsPage::selectEngine (EngineType e)
{
    if (e == mEngineType) return;

    // Phase 1: notify editor BEFORE we change anything so it can call
    // unregisterClipEngine while the OLD processor pointer is still in
    // mClipEngines[].  No race with the audio thread.
    if (onEngineDestroying) onEngineDestroying();

    // Phase 2: lazy-create the new engine if this is its first selection.
    // Existing engine instances stay untouched; their APVTS state persists.
    if (e == EngineType::BaySickPlayer && ! mPlayerProc)
    {
        const juce::String prefix = "clip_" + juce::String (mPageIndex) + "_";
        auto vp = std::make_unique<VibePlayerProcessor> (prefix);
        vp->prepareToPlay (44100.0, 512);   // re-prepared at host SR by editor
        if (mClipPath.isNotEmpty())
            vp->loadSampleFile (juce::File (mClipPath));
        mPlayerProc = std::move (vp);
        mPlayerEditor.reset (mPlayerProc->createEditor());
        if (mPlayerEditor) addChildComponent (*mPlayerEditor);
    }
    else if (e == EngineType::BaySickNAMIR && ! mNamIrProc)
    {
        auto nam = std::make_unique<BaySickNAMIRProcessor>();
        nam->prepareToPlay (44100.0, 512);
        mNamIrProc = std::move (nam);
        mNamIrEditor.reset (mNamIrProc->createEditor());
        if (mNamIrEditor) addChildComponent (*mNamIrEditor);
    }

    // Phase 3: swap the active type + visible editor.
    mEngineType = e;
    if (mPlayerEditor) mPlayerEditor->setVisible (e == EngineType::BaySickPlayer && mActiveTab == 0);
    if (mNamIrEditor)  mNamIrEditor ->setVisible (e == EngineType::BaySickNAMIR  && mActiveTab == 0);
    resized();

    // Reflect selection in the picker UI without re-firing onChange.
    const int id = (e == EngineType::BaySickPlayer) ? 1
                 : (e == EngineType::BaySickNAMIR)  ? 2 : 0;
    mEnginePicker.setSelectedId (id, juce::dontSendNotification);

    repaint();

    // Phase 4: notify editor AFTER the active engine has been updated so it
    // can call registerClipEngine with the new active processor.  Audio
    // thread starts processing the new engine on the next block.
    if (onEngineChanged) onEngineChanged();
}

juce::AudioProcessorEditor* ClipsPage::activeEditor() const
{
    return mEngineType == EngineType::BaySickPlayer ? mPlayerEditor.get()
         : mEngineType == EngineType::BaySickNAMIR  ? mNamIrEditor .get()
         :                                            nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sub-tab + clip path.
// ─────────────────────────────────────────────────────────────────────────────
void ClipsPage::switchTab (int idx)
{
    mActiveTab = juce::jlimit (0, 2, idx);

    // Show only the active engine's editor on the Player sub-tab.  Inactive
    // engines stay alive (settings preserved) but their editors stay hidden.
    if (mPlayerEditor) mPlayerEditor->setVisible (mEngineType == EngineType::BaySickPlayer && mActiveTab == 0);
    if (mNamIrEditor)  mNamIrEditor ->setVisible (mEngineType == EngineType::BaySickNAMIR  && mActiveTab == 0);
    if (mEqStub)       mEqStub      ->setVisible (mActiveTab == 2);
    // mActiveTab == 1 = Piano Roll redirect handled by StandaloneEditor before
    // it ever reaches this page.

    resized();
    repaint();
}

void ClipsPage::setClipFilePath (const juce::String& p)
{
    mClipPath = p;
    mClipFileLabel.setText (p.isNotEmpty()
                                ? juce::File (p).getFileName()
                                : juce::String ("(no clip)"),
                            juce::dontSendNotification);

    // If the BaySickPlayer instance exists (it's been picked at least once),
    // push the new file in so it plays the latest sample.  NAM/IR doesn't
    // load clips directly (re-amp routing pulls audio from upstream).
    if (auto* vp = dynamic_cast<VibePlayerProcessor*> (mPlayerProc.get()))
        if (p.isNotEmpty())
            vp->loadSampleFile (juce::File (p));
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint + layout.
// ─────────────────────────────────────────────────────────────────────────────
void ClipsPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));

    // Header strip behind the picker + filename.
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRect (0, 0, getWidth(), kHeaderRowH);
    g.setColour (juce::Colour (0xff333333));
    g.fillRect (0, kHeaderRowH, getWidth(), 1);
}

void ClipsPage::resized()
{
    auto r = getLocalBounds();

    // Header row: [Engine ▾] [Clip filename ───────────]
    auto header = r.removeFromTop (kHeaderRowH).reduced (kPad, 6);
    mEnginePicker.setBounds (header.removeFromLeft (kPickerW));
    header.removeFromLeft (kPad);
    mClipFileLabel.setBounds (header.removeFromLeft (juce::jmin (kFilenameW, header.getWidth())));

    // Body: engine editor (Player tab) or EQ stub.
    layoutEditor (r);
}

void ClipsPage::layoutEditor (juce::Rectangle<int> r)
{
    // 2026-04-28 (G-2): give the active engine editor the full content area.
    // Mirrors LayersPage's pattern — VibePlayerEditor stretches via resized();
    // BaySickNAMIREditor's fixed-coord widgets keep position with the unused
    // space below as deliberate room for future controls.
    if (auto* ed = activeEditor(); ed && ed->isVisible())
        ed->setBounds (r);

    if (mEqStub && mEqStub->isVisible())
        mEqStub->setBounds (r);
}

// ─────────────────────────────────────────────────────────────────────────────
// ClipsEmptyState — drop-zone placeholder for the empty Clips ribbon slot.
// ─────────────────────────────────────────────────────────────────────────────
ClipsEmptyState::ClipsEmptyState()
{
    setInterceptsMouseClicks (true, false);
}

void ClipsEmptyState::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181818));

    auto bounds = getLocalBounds().toFloat().reduced (32.0f);
    juce::Path p;
    p.addRoundedRectangle (bounds, 12.0f);
    juce::Path dashed;
    const float dashes[] = { 8.0f, 6.0f };
    juce::PathStrokeType (mFileHover ? 3.0f : 2.0f).createDashedStroke (
        dashed, p, dashes, 2);
    g.setColour (mFileHover ? juce::Colour (0xff2bb6c8) : juce::Colour (0xff404040));
    g.fillPath (dashed);

    g.setColour (mFileHover ? juce::Colour (0xff2bb6c8) : juce::Colour (0xffc0c0c0));
    g.setFont (juce::Font (18.0f, juce::Font::plain));
    g.drawText (
        "Drop and drag a clip here or on the Builder page to start a Clips page",
        bounds.toNearestInt(),
        juce::Justification::centred,
        true);

    g.setColour (juce::Colour (0xff707070));
    g.setFont (juce::Font (12.0f, juce::Font::plain));
    auto sub = bounds.withHeight (40.0f).withY (bounds.getCentreY() + 16.0f);
    g.drawText (
        "Accepted formats: .wav  .mp3  .aiff  .flac  .ogg  .aif",
        sub.toNearestInt(),
        juce::Justification::centred,
        true);
}

bool ClipsEmptyState::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase (".wav")  || f.endsWithIgnoreCase (".mp3")
         || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac")
         || f.endsWithIgnoreCase (".ogg")  || f.endsWithIgnoreCase (".aif"))
        {
            mFileHover = true;
            repaint();
            return true;
        }
    }
    return false;
}

void ClipsEmptyState::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    mFileHover = false;
    repaint();
    if (! onClipDropped) return;

    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase (".wav")  || f.endsWithIgnoreCase (".mp3")
         || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac")
         || f.endsWithIgnoreCase (".ogg")  || f.endsWithIgnoreCase (".aif"))
        {
            onClipDropped (f);
        }
    }
}
