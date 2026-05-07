#pragma once
#include <JuceHeader.h>
#include "../ProjectManager.h"

// ──────────────────────────────────────────────────────────────────────────────
// ProjectBrowserWindow (project-persistence Phase P3, 2026-04-23)
//
// Custom "Open Project" browser - replaces the native file picker.  Shows one
// row per project folder under Documents\BaySickDAW\Projects\, sortable by
// name / last modified / size.  Double-click opens; right-click gives the
// Rename / Duplicate / Delete / Show in Explorer menu.  "New Project..." and
// "Open" buttons at the bottom mirror the File menu entries for convenience.
//
// Launched via juce::DialogWindow::LaunchOptions from StandaloneEditor::doFileOpen.
// ──────────────────────────────────────────────────────────────────────────────
class ProjectBrowserWindow : public juce::Component,
                              public juce::TableListBoxModel
{
public:
    // onOpenSelected fires when user double-clicks a row or hits Open.  The
    // caller is responsible for closing the enclosing DialogWindow.  onNewProject
    // is called when the "New Project..." button is pressed (also caller-owned
    // close + prompt flow).
    std::function<void (const juce::File& projectFolder)> onOpenSelected;
    std::function<void()>                                 onNewProject;

    // Whether a given project folder is the one currently open - used to
    // refuse destructive operations (Rename / Delete).  Caller plugs in a
    // query against ProjectManager::isCurrentProject.
    std::function<bool (const juce::File&)>               isCurrentProject;

    ProjectBrowserWindow();
    ~ProjectBrowserWindow() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // ── TableListBoxModel ───────────────────────────────────────────────────
    int  getNumRows() override;
    void paintRowBackground (juce::Graphics&, int rowNumber,
                              int w, int h, bool rowIsSelected) override;
    void paintCell (juce::Graphics&, int rowNumber, int columnId,
                     int w, int h, bool rowIsSelected) override;
    void cellDoubleClicked (int rowNumber, int columnId,
                             const juce::MouseEvent&) override;
    void cellClicked (int rowNumber, int columnId,
                       const juce::MouseEvent&) override;
    void sortOrderChanged (int newSortColumnId, bool isForwards) override;

private:
    void refreshList();
    void doOpenSelected();
    void doRenameSelected();
    void doDuplicateSelected();
    void doDeleteSelected();
    void doShowInExplorer();

    std::vector<ProjectManager::Listing> mItems;
    juce::TableListBox                   mTable { "Projects", this };
    juce::TextButton                     mOpenBtn   { "Open" };
    juce::TextButton                     mNewBtn    { "New Project..." };
    juce::TextButton                     mCancelBtn { "Cancel" };

    enum Column { ColName = 1, ColModified, ColSize };
    int  mSortColumn { ColModified };
    bool mSortForward { false };   // newest-first default

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectBrowserWindow)
};
