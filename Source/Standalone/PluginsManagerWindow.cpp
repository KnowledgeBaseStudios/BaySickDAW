#include "PluginsManagerWindow.h"
#include "SharedUI.h"   // VC palette

namespace
{
    // TableListBox column ids must be > 0.
    constexpr int kAddedColName = 1;
    constexpr int kAddedColKind = 2;
    constexpr int kAddedColMaker = 3;
    constexpr int kAddedColFile = 4;

    constexpr int kResColCheck = 1;
    constexpr int kResColName  = 2;
    constexpr int kResColKind  = 3;
    constexpr int kResColInfo  = 4;

    constexpr int kRowH   = 24;
    constexpr int kTitleH = 22;
    constexpr int kBtnH   = 26;

    juce::String kindOf (const juce::PluginDescription& d)
    {
        return d.isInstrument ? "Instrument" : "Effect";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Section 1 — folders
// ─────────────────────────────────────────────────────────────────────────────
int PluginsManagerContent::FoldersModel::getNumRows()
{
    return owner.mFolderRows.size();
}

void PluginsManagerContent::FoldersModel::paintListBoxItem (int row, juce::Graphics& g,
                                                            int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, owner.mFolderRows.size()))
        return;

    if (selected)
        g.fillAll (VC::Surface);

    g.setColour (VC::Text);
    g.setFont (juce::Font (13.0f));
    g.drawText (owner.mFolderRows[row], 6, 0, w - 12, h, juce::Justification::centredLeft, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Section 2 — added
// ─────────────────────────────────────────────────────────────────────────────
int PluginsManagerContent::AddedModel::getNumRows()
{
    return owner.mAddedRows.size();
}

void PluginsManagerContent::AddedModel::paintRowBackground (juce::Graphics& g, int row,
                                                            int, int, bool selected)
{
    g.fillAll (selected ? VC::Surface : (row % 2 ? VC::Panel : VC::Bg));
}

void PluginsManagerContent::AddedModel::paintCell (juce::Graphics& g, int row, int col,
                                                   int w, int h, bool)
{
    if (! juce::isPositiveAndBelow (row, owner.mAddedRows.size()))
        return;

    const auto& d = owner.mAddedRows.getReference (row);

    g.setColour (col == kAddedColFile ? VC::TextDim : VC::Text);
    g.setFont (juce::Font (13.0f));

    juce::String text;

    switch (col)
    {
        case kAddedColName:  text = d.name;                                break;
        case kAddedColKind:  text = kindOf (d);                            break;
        case kAddedColMaker: text = d.manufacturerName;                    break;
        case kAddedColFile:  text = juce::File (d.fileOrIdentifier).getFullPathName(); break;
        default: break;
    }

    g.drawText (text, 4, 0, w - 8, h, juce::Justification::centredLeft, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Section 3 — scan results (+ what the scan could not use)
// ─────────────────────────────────────────────────────────────────────────────
int PluginsManagerContent::ResultsModel::getNumRows()
{
    return (int) owner.mResultRows.size();
}

void PluginsManagerContent::ResultsModel::paintRowBackground (juce::Graphics& g, int row,
                                                              int, int, bool selected)
{
    g.fillAll (selected ? VC::Surface : (row % 2 ? VC::Panel : VC::Bg));
}

void PluginsManagerContent::ResultsModel::paintCell (juce::Graphics& g, int row, int col,
                                                     int w, int h, bool)
{
    if (! juce::isPositiveAndBelow (row, (int) owner.mResultRows.size()))
        return;

    const auto& r = owner.mResultRows[(size_t) row];

    // Skipped rows are dimmed and carry no checkbox -- they are an explanation,
    // not something the user can act on from here.
    g.setColour (r.skipped ? VC::TextDim : VC::Text);
    g.setFont (juce::Font (13.0f));

    if (col == kResColCheck)
    {
        if (r.skipped)
            return;

        auto box = juce::Rectangle<int> (0, 0, w, h).withSizeKeepingCentre (14, 14).toFloat();
        g.setColour (VC::Accent);
        g.drawRoundedRectangle (box, 3.0f, 1.0f);

        if (r.checked)
        {
            g.setColour (VC::Green);
            g.fillRoundedRectangle (box.reduced (3.0f), 2.0f);
        }

        return;
    }

    juce::String text;

    switch (col)
    {
        case kResColName:
            text = r.skipped ? juce::File (r.path).getFileName() : r.desc.name;
            break;
        case kResColKind:
            text = r.skipped ? juce::String() : kindOf (r.desc);
            break;
        case kResColInfo:
            text = r.skipped ? r.reason : r.path;
            break;
        default: break;
    }

    g.drawText (text, 4, 0, w - 8, h, juce::Justification::centredLeft, true);
}

void PluginsManagerContent::ResultsModel::cellClicked (int row, int col, const juce::MouseEvent&)
{
    if (col != kResColCheck || ! juce::isPositiveAndBelow (row, (int) owner.mResultRows.size()))
        return;

    auto& r = owner.mResultRows[(size_t) row];

    if (r.skipped)
        return;

    r.checked = ! r.checked;
    owner.mResults.repaintRow (row);
    owner.updateScanUi();
}

// ─────────────────────────────────────────────────────────────────────────────
// Content
// ─────────────────────────────────────────────────────────────────────────────
PluginsManagerContent::PluginsManagerContent (Hosting::PluginManager& pm)
    : mPlugins (pm)
{
    auto title = [this] (juce::Label& l)
    {
        l.setFont (juce::Font (14.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, VC::Text);
        addAndMakeVisible (l);
    };

    title (mFoldersTitle);
    title (mAddedTitle);
    title (mResultsTitle);

    mFolders.setRowHeight (kRowH);
    mFolders.setColour (juce::ListBox::backgroundColourId, VC::Bg);
    addAndMakeVisible (mFolders);

    mAdded.setRowHeight (kRowH);
    mAdded.setColour (juce::ListBox::backgroundColourId, VC::Bg);
    mAdded.getHeader().addColumn ("Name",         kAddedColName,  220);
    mAdded.getHeader().addColumn ("Kind",         kAddedColKind,   90);
    mAdded.getHeader().addColumn ("Manufacturer", kAddedColMaker, 160);
    mAdded.getHeader().addColumn ("File",         kAddedColFile,  360);
    addAndMakeVisible (mAdded);

    mResults.setRowHeight (kRowH);
    mResults.setColour (juce::ListBox::backgroundColourId, VC::Bg);
    mResults.getHeader().addColumn ("",     kResColCheck,  30);
    mResults.getHeader().addColumn ("Name", kResColName,  220);
    mResults.getHeader().addColumn ("Kind", kResColKind,   90);
    mResults.getHeader().addColumn ("File / reason skipped", kResColInfo, 520);
    addAndMakeVisible (mResults);

    for (auto* b : { &mAddFolderBtn, &mRemoveFolderBtn, &mResetFoldersBtn,
                     &mRemoveAddedBtn, &mScanBtn, &mAddCheckedBtn })
        addAndMakeVisible (*b);

    mAddFolderBtn   .onClick = [this] { doAddFolder(); };
    mRemoveFolderBtn.onClick = [this] { doRemoveFolder(); };
    mResetFoldersBtn.onClick = [this] { doResetFolders(); };
    mRemoveAddedBtn .onClick = [this] { doRemoveAdded(); };
    mScanBtn        .onClick = [this] { doScan(); };
    mAddCheckedBtn  .onClick = [this] { doAddChecked(); };

    mStatus.setFont (juce::Font (12.0f));
    mStatus.setColour (juce::Label::textColourId, VC::TextDim);
    addAndMakeVisible (mStatus);

    mProgressBar.setVisible (false);
    addChildComponent (mProgressBar);

    // The manager outlives this window, so its callbacks are cleared in the
    // destructor rather than lifetime-guarded here.
    mPlugins.onScanProgress   = [this] { updateScanUi(); };
    mPlugins.onScanFinished   = [this] { refreshResults(); updateScanUi(); };
    mPlugins.onAddedListChanged = [this] { refreshAdded(); };

    refreshFolders();
    refreshAdded();
    refreshResults();
    updateScanUi();
}

PluginsManagerContent::~PluginsManagerContent()
{
    mPlugins.onScanProgress     = nullptr;
    mPlugins.onScanFinished     = nullptr;
    mPlugins.onAddedListChanged = nullptr;
}

void PluginsManagerContent::paint (juce::Graphics& g)
{
    g.fillAll (VC::Bg);
}

void PluginsManagerContent::resized()
{
    auto b = getLocalBounds().reduced (10);

    auto section = [&] (juce::Label& t, juce::Component& list, int listH,
                        std::initializer_list<juce::Component*> buttons)
    {
        t.setBounds (b.removeFromTop (kTitleH));
        list.setBounds (b.removeFromTop (listH));

        auto row = b.removeFromTop (kBtnH + 4).withTrimmedTop (4);

        for (auto* c : buttons)
        {
            if (c == nullptr)
                continue;

            c->setBounds (row.removeFromLeft (130));
            row.removeFromLeft (6);
        }

        b.removeFromTop (10);
    };

    section (mFoldersTitle, mFolders, 96, { &mAddFolderBtn, &mRemoveFolderBtn, &mResetFoldersBtn });
    section (mAddedTitle,   mAdded,   150, { &mRemoveAddedBtn });

    // Section 3 takes whatever height is left, minus its own button row.
    mResultsTitle.setBounds (b.removeFromTop (kTitleH));
    auto btnRow = b.removeFromBottom (kBtnH + 4).withTrimmedTop (4);
    mResults.setBounds (b);

    mScanBtn      .setBounds (btnRow.removeFromLeft (130));
    btnRow.removeFromLeft (6);
    mAddCheckedBtn.setBounds (btnRow.removeFromLeft (130));
    btnRow.removeFromLeft (12);
    mProgressBar  .setBounds (btnRow.removeFromLeft (180).reduced (0, 4));
    btnRow.removeFromLeft (10);
    mStatus       .setBounds (btnRow);
}

// ── Refresh ──────────────────────────────────────────────────────────────────

void PluginsManagerContent::refreshFolders()
{
    mFolderRows.clear();

    const auto path = mPlugins.getScanFolders();

    for (int i = 0; i < path.getNumPaths(); ++i)
        mFolderRows.add (path[i].getFullPathName());

    mFolders.updateContent();
    mFolders.repaint();
}

void PluginsManagerContent::refreshAdded()
{
    mAddedRows = mPlugins.getAddedPlugins();
    mAdded.updateContent();
    mAdded.repaint();
}

void PluginsManagerContent::refreshResults()
{
    mResultRows.clear();

    for (const auto& d : mPlugins.getScanResults())
    {
        ResultRow r;
        r.desc = d;
        r.path = juce::File (d.fileOrIdentifier).getFullPathName();
        mResultRows.push_back (r);
    }

    for (const auto& s : mPlugins.getSkipped())
    {
        ResultRow r;
        r.skipped = true;
        r.path    = s.path;
        r.reason  = s.reason;
        mResultRows.push_back (r);
    }

    mResults.updateContent();
    mResults.repaint();
}

void PluginsManagerContent::updateScanUi()
{
    const bool scanning = mPlugins.isScanning();

    mProgress = (double) mPlugins.getScanProgress();
    mProgressBar.setVisible (scanning);

    mScanBtn.setButtonText (scanning ? "Cancel Scan" : "Scan");
    mAddFolderBtn   .setEnabled (! scanning);
    mRemoveFolderBtn.setEnabled (! scanning);
    mResetFoldersBtn.setEnabled (! scanning);

    int checked = 0;

    for (const auto& r : mResultRows)
        if (! r.skipped && r.checked)
            ++checked;

    mAddCheckedBtn.setEnabled (! scanning && checked > 0);
    mAddCheckedBtn.setButtonText (checked > 0 ? "Add Checked (" + juce::String (checked) + ")"
                                              : "Add Checked");

    if (scanning)
        mStatus.setText (mPlugins.getScanStatusText(), juce::dontSendNotification);
    else if (mPlugins.hasScanned())
        mStatus.setText (mPlugins.getScanStatusText(), juce::dontSendNotification);
    else
        mStatus.setText ("Press Scan to search the folders above.", juce::dontSendNotification);
}

// ── Actions ──────────────────────────────────────────────────────────────────

void PluginsManagerContent::doAddFolder()
{
    mChooser = std::make_unique<juce::FileChooser> ("Choose a folder to scan for plugins",
                                                    juce::File(), juce::String());

    mChooser->launchAsync (juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
                           [this] (const juce::FileChooser& fc)
                           {
                               const auto dir = fc.getResult();

                               if (dir.isDirectory() && mPlugins.addScanFolder (dir))
                                   refreshFolders();
                           });
}

void PluginsManagerContent::doRemoveFolder()
{
    const int row = mFolders.getSelectedRow();

    if (row < 0)
        return;

    mPlugins.removeScanFolder (row);
    refreshFolders();
}

void PluginsManagerContent::doResetFolders()
{
    mPlugins.resetScanFoldersToDefaults();
    refreshFolders();
}

void PluginsManagerContent::doRemoveAdded()
{
    const int row = mAdded.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, mAddedRows.size()))
        return;

    mPlugins.removeFromAddedList (mAddedRows.getReference (row));
    refreshAdded();
}

void PluginsManagerContent::doScan()
{
    if (mPlugins.isScanning())
    {
        mPlugins.cancelScan();
        return;
    }

    mResultRows.clear();
    mResults.updateContent();
    mResults.repaint();

    mPlugins.startScan();
    updateScanUi();
}

void PluginsManagerContent::doAddChecked()
{
    juce::Array<juce::PluginDescription> toAdd;

    for (const auto& r : mResultRows)
        if (! r.skipped && r.checked)
            toAdd.add (r.desc);

    if (toAdd.isEmpty())
        return;

    mPlugins.addToAddedList (toAdd);
    refreshAdded();
    refreshResults();
    updateScanUi();
}

// ─────────────────────────────────────────────────────────────────────────────
// Window
// ─────────────────────────────────────────────────────────────────────────────
PluginsManagerWindow::PluginsManagerWindow (Hosting::PluginManager& pm)
    : juce::DocumentWindow ("Plugins", VC::Bg, juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setContentOwned (new PluginsManagerContent (pm), true);
    centreWithSize (920, 700);
    setVisible (true);
    toFront (true);
}

void PluginsManagerWindow::closeButtonPressed()
{
    delete this;
}
