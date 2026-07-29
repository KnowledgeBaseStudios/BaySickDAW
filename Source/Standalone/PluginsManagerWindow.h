#pragma once
#include <JuceHeader.h>
#include "../Hosting/PluginManager.h"

// ── PluginsManagerWindow ─────────────────────────────────────────────────────
// QA-ModelShell TS6 (BLU-299): Options > Plugins.  Jeff's spec, 2026-07-29 --
// three sections, in this order:
//
//   1. SCAN FOLDERS  the folders currently chosen, plus a button that opens the
//                    OS folder picker to add another.  Seeded with the standard
//                    VST3 install locations.
//   2. ADDED         the plugins the user has added.  This is the list every
//                    other surface reads (rack picker, Plugins "+" menu).
//   3. SCAN RESULTS  blank until Scan.  Everything found that is not already
//                    added, each with a checkbox, plus an Add button that moves
//                    the checked rows into section 2.
//
// Section 3 also lists what the scan COULD NOT use, with the reason -- never a
// silent omission.  Under VST3-only scope a user's old VST2 freeware is exactly
// what turns up and will not load, and "Skipped: VST2 is not supported" is the
// difference between an explanation and an apparent bug.
//
// A real desktop window rather than a contained WorkspaceWindow: it is an
// Options utility like Audio Settings, not a page.  That also keeps it clear of
// the child-peer z-order trap -- a desktop window sits above the workspace's
// native child peers, where a drawn overlay would be buried behind them.
// ─────────────────────────────────────────────────────────────────────────────

class PluginsManagerContent final : public juce::Component
{
public:
    explicit PluginsManagerContent (Hosting::PluginManager&);
    ~PluginsManagerContent() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    // ── Section 1 ───────────────────────────────────────────────────────────
    struct FoldersModel final : public juce::ListBoxModel
    {
        explicit FoldersModel (PluginsManagerContent& o) : owner (o) {}
        int  getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        PluginsManagerContent& owner;
    };

    // ── Section 2 ───────────────────────────────────────────────────────────
    struct AddedModel final : public juce::TableListBoxModel
    {
        explicit AddedModel (PluginsManagerContent& o) : owner (o) {}
        int  getNumRows() override;
        void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
        void paintCell (juce::Graphics&, int row, int col, int w, int h, bool selected) override;
        PluginsManagerContent& owner;
    };

    // ── Section 3 ───────────────────────────────────────────────────────────
    // One row type for both halves so the two always render in one list and a
    // skipped plugin can never be scrolled past unnoticed.
    struct ResultRow
    {
        bool                    skipped = false;
        bool                    checked = false;
        juce::PluginDescription desc;      // valid when !skipped
        juce::String            path;
        juce::String            reason;    // valid when skipped
    };

    struct ResultsModel final : public juce::TableListBoxModel
    {
        explicit ResultsModel (PluginsManagerContent& o) : owner (o) {}
        int  getNumRows() override;
        void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
        void paintCell (juce::Graphics&, int row, int col, int w, int h, bool selected) override;
        void cellClicked (int row, int col, const juce::MouseEvent&) override;
        PluginsManagerContent& owner;
    };

    void refreshFolders();
    void refreshAdded();
    void refreshResults();
    void updateScanUi();

    void doAddFolder();
    void doRemoveFolder();
    void doResetFolders();
    void doRemoveAdded();
    void doScan();
    void doAddChecked();

    Hosting::PluginManager& mPlugins;

    juce::Label   mFoldersTitle { {}, "1.  Scan folders" };
    juce::Label   mAddedTitle   { {}, "2.  Added plugins" };
    juce::Label   mResultsTitle { {}, "3.  Scan results" };

    FoldersModel  mFoldersModel { *this };
    AddedModel    mAddedModel   { *this };
    ResultsModel  mResultsModel { *this };

    juce::ListBox      mFolders { "folders", &mFoldersModel };
    juce::TableListBox mAdded   { "added",   &mAddedModel };
    juce::TableListBox mResults { "results", &mResultsModel };

    juce::TextButton mAddFolderBtn    { "Add Folder..." };
    juce::TextButton mRemoveFolderBtn { "Remove" };
    juce::TextButton mResetFoldersBtn { "Reset to Defaults" };
    juce::TextButton mRemoveAddedBtn  { "Remove Selected" };
    juce::TextButton mScanBtn         { "Scan" };
    juce::TextButton mAddCheckedBtn   { "Add Checked" };

    // BLU-299 search/filter.  ONE box filtering BOTH the added list and the scan
    // results (Jeff 2026-07-29: "so you can also see what you already have under
    // that title") -- two boxes would let a user search the results, see
    // nothing, and not realise the plugin was already added.
    juce::Label       mFilterLabel { {}, "Filter:" };
    juce::TextEditor  mFilter;
    juce::String      mFilterText;

    bool matchesFilter (const juce::PluginDescription&) const;
    bool matchesFilter (const juce::String& text) const;

    juce::Label       mStatus;
    double            mProgress { 0.0 };
    juce::ProgressBar mProgressBar { mProgress };

    juce::StringArray            mFolderRows;
    juce::Array<juce::PluginDescription> mAddedRows;
    std::vector<ResultRow>       mResultRows;

    std::unique_ptr<juce::FileChooser> mChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginsManagerContent)
};

// Self-deleting on close; owners hold a SafePointer (same shape as
// KeyBindsWindow / RustyDrumsMapWindow).
class PluginsManagerWindow final : public juce::DocumentWindow
{
public:
    explicit PluginsManagerWindow (Hosting::PluginManager&);

    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginsManagerWindow)
};
