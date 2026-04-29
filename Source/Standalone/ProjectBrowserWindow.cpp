#include "ProjectBrowserWindow.h"

namespace
{
    juce::String formatSize (juce::int64 bytes)
    {
        if (bytes < 1024) return juce::String (bytes) + " B";
        if (bytes < 1024 * 1024) return juce::String (bytes / 1024) + " KB";
        if (bytes < 1024 * 1024 * 1024) return juce::String ((double) bytes / (1024.0 * 1024.0), 1) + " MB";
        return juce::String ((double) bytes / (1024.0 * 1024.0 * 1024.0), 2) + " GB";
    }

    juce::String formatTime (const juce::Time& t)
    {
        // Short local representation: YYYY-MM-DD HH:MM
        return t.formatted ("%Y-%m-%d %H:%M");
    }

    // Async one-text-field prompt reused from StandaloneEditor pattern.
    void askForName (const juce::String& title,
                      const juce::String& message,
                      const juce::String& defaultText,
                      std::function<void (juce::String)> onAccept,
                      std::function<void()> onCancel = {})
    {
        auto* aw = new juce::AlertWindow (title, message,
                                           juce::MessageBoxIconType::QuestionIcon);
        aw->addTextEditor ("name", defaultText, {});
        aw->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true,
            juce::ModalCallbackFunction::create (
                [aw, onAccept = std::move (onAccept),
                     onCancel = std::move (onCancel)] (int result)
                {
                    if (result == 1)
                    {
                        const auto raw = aw->getTextEditorContents ("name");
                        const auto sanitized = ProjectManager::sanitizeProjectName (raw);
                        if (onAccept) onAccept (sanitized);
                    }
                    else
                    {
                        if (onCancel) onCancel();
                    }
                    delete aw;
                }),
            false);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
ProjectBrowserWindow::ProjectBrowserWindow()
{
    setSize (560, 400);

    auto& header = mTable.getHeader();
    header.addColumn ("Name",          ColName,     240, 80, -1,
                      juce::TableHeaderComponent::defaultFlags);
    header.addColumn ("Last Modified", ColModified, 170, 80, -1,
                      juce::TableHeaderComponent::defaultFlags);
    header.addColumn ("Size",          ColSize,     100, 60, -1,
                      juce::TableHeaderComponent::defaultFlags);
    header.setSortColumnId (mSortColumn, mSortForward);
    mTable.setModel (this);
    mTable.setMultipleSelectionEnabled (false);
    addAndMakeVisible (mTable);

    mOpenBtn.onClick   = [this] { doOpenSelected(); };
    mNewBtn.onClick    = [this] { if (onNewProject) onNewProject(); };
    mCancelBtn.onClick = [this]
    {
        if (auto* w = findParentComponentOfClass<juce::DialogWindow>())
            w->exitModalState (0);
    };
    addAndMakeVisible (mOpenBtn);
    addAndMakeVisible (mNewBtn);
    addAndMakeVisible (mCancelBtn);

    refreshList();
}

ProjectBrowserWindow::~ProjectBrowserWindow() = default;

void ProjectBrowserWindow::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.92f));
}

void ProjectBrowserWindow::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto footer = r.removeFromBottom (34);
    mTable.setBounds (r);

    auto btnRow = footer;
    mCancelBtn.setBounds (btnRow.removeFromRight (90));
    btnRow.removeFromRight (6);
    mOpenBtn  .setBounds (btnRow.removeFromRight (90));
    btnRow.removeFromRight (6);
    mNewBtn   .setBounds (btnRow.removeFromLeft (130));
}

// ── TableListBoxModel ────────────────────────────────────────────────────────
int ProjectBrowserWindow::getNumRows() { return (int) mItems.size(); }

void ProjectBrowserWindow::paintRowBackground (juce::Graphics& g, int row,
                                                int w, int h, bool rowIsSelected)
{
    juce::ignoreUnused (row, w);
    if (rowIsSelected)
        g.fillAll (juce::Colour (0xFF3A6EA5));
    else if (row % 2 == 0)
        g.fillAll (juce::Colours::black.withAlpha (0.25f));

    juce::ignoreUnused (h);
}

void ProjectBrowserWindow::paintCell (juce::Graphics& g, int row, int columnId,
                                       int w, int h, bool rowIsSelected)
{
    if (row < 0 || row >= (int) mItems.size()) return;
    const auto& e = mItems[(size_t) row];

    g.setColour (rowIsSelected ? juce::Colours::white : juce::Colours::lightgrey);
    g.setFont (juce::Font (14.0f));

    juce::String text;
    switch (columnId)
    {
        case ColName:     text = e.name;                       break;
        case ColModified: text = formatTime (e.lastModified);  break;
        case ColSize:     text = formatSize (e.sizeBytes);     break;
        default: return;
    }
    g.drawText (text, 8, 0, w - 16, h, juce::Justification::centredLeft, true);
}

void ProjectBrowserWindow::cellDoubleClicked (int row, int, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) mItems.size())
    {
        mTable.selectRow (row);
        doOpenSelected();
    }
}

void ProjectBrowserWindow::cellClicked (int row, int, const juce::MouseEvent& ev)
{
    if (row < 0 || row >= (int) mItems.size()) return;
    mTable.selectRow (row);
    if (! ev.mods.isPopupMenu()) return;

    const auto& e = mItems[(size_t) row];
    const bool isCurrent = isCurrentProject && isCurrentProject (e.folder);

    juce::PopupMenu m;
    m.addItem (1, "Open",               true);
    m.addSeparator();
    m.addItem (2, "Rename...",          ! isCurrent);
    m.addItem (3, "Duplicate...",       true);
    m.addItem (4, "Delete",             ! isCurrent);
    m.addSeparator();
    m.addItem (5, "Show in Explorer",   true);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&mTable),
        [this] (int id)
        {
            switch (id)
            {
                case 1: doOpenSelected();     break;
                case 2: doRenameSelected();   break;
                case 3: doDuplicateSelected(); break;
                case 4: doDeleteSelected();   break;
                case 5: doShowInExplorer();   break;
                default: break;
            }
        });
}

void ProjectBrowserWindow::sortOrderChanged (int newSortColumnId, bool isForwards)
{
    mSortColumn  = newSortColumnId;
    mSortForward = isForwards;

    auto cmp = [&](const ProjectManager::Listing& a,
                    const ProjectManager::Listing& b)
    {
        switch (mSortColumn)
        {
            case ColName:     return mSortForward ? a.name.compareIgnoreCase (b.name) < 0
                                                   : a.name.compareIgnoreCase (b.name) > 0;
            case ColSize:     return mSortForward ? a.sizeBytes < b.sizeBytes
                                                   : a.sizeBytes > b.sizeBytes;
            case ColModified:
            default:          return mSortForward ? a.lastModified < b.lastModified
                                                   : a.lastModified > b.lastModified;
        }
    };
    std::sort (mItems.begin(), mItems.end(), cmp);
    mTable.updateContent();
    mTable.repaint();
}

// ── Action helpers ───────────────────────────────────────────────────────────
void ProjectBrowserWindow::refreshList()
{
    mItems = ProjectManager::listProjects();
    // Re-apply current sort.
    sortOrderChanged (mSortColumn, mSortForward);
}

void ProjectBrowserWindow::doOpenSelected()
{
    const int row = mTable.getSelectedRow();
    if (row < 0 || row >= (int) mItems.size()) return;
    const auto folder = mItems[(size_t) row].folder;
    if (onOpenSelected) onOpenSelected (folder);
}

void ProjectBrowserWindow::doRenameSelected()
{
    const int row = mTable.getSelectedRow();
    if (row < 0 || row >= (int) mItems.size()) return;
    const auto folder = mItems[(size_t) row].folder;

    askForName ("Rename Project",
                 "Enter a new name for " + folder.getFileName() + ":",
                 folder.getFileName(),
                 [this, folder] (juce::String newName)
                 {
                     if (! ProjectManager::isValidProjectName (newName))
                     {
                         juce::AlertWindow::showMessageBoxAsync (
                             juce::MessageBoxIconType::WarningIcon,
                             "Invalid name",
                             "Project names can't contain < > : \" / \\ | ? * or be\n"
                             "reserved device names (CON, PRN, AUX, NUL, COM1-9, LPT1-9).");
                         return;
                     }
                     if (! ProjectManager::renameProject (folder, newName))
                     {
                         juce::AlertWindow::showMessageBoxAsync (
                             juce::MessageBoxIconType::WarningIcon,
                             "Rename failed",
                             "A project with that name already exists, or the folder\n"
                             "couldn't be renamed (is it currently open?).");
                         return;
                     }
                     refreshList();
                 });
}

void ProjectBrowserWindow::doDuplicateSelected()
{
    const int row = mTable.getSelectedRow();
    if (row < 0 || row >= (int) mItems.size()) return;
    const auto folder = mItems[(size_t) row].folder;

    askForName ("Duplicate Project",
                 "Enter a name for the copy:",
                 folder.getFileName() + " Copy",
                 [this, folder] (juce::String newName)
                 {
                     if (! ProjectManager::isValidProjectName (newName))
                     {
                         juce::AlertWindow::showMessageBoxAsync (
                             juce::MessageBoxIconType::WarningIcon,
                             "Invalid name",
                             "Try another name.");
                         return;
                     }
                     auto copied = ProjectManager::duplicateProject (folder, newName);
                     if (copied == juce::File())
                     {
                         juce::AlertWindow::showMessageBoxAsync (
                             juce::MessageBoxIconType::WarningIcon,
                             "Duplicate failed",
                             "Check that the Projects folder is writable and try again.");
                         return;
                     }
                     refreshList();
                 });
}

void ProjectBrowserWindow::doDeleteSelected()
{
    const int row = mTable.getSelectedRow();
    if (row < 0 || row >= (int) mItems.size()) return;
    const auto folder = mItems[(size_t) row].folder;

    juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::WarningIcon,
        "Delete Project",
        "Move '" + folder.getFileName() + "' to the Recycle Bin?\n"
        "You can restore it from there later if needed.",
        "Delete",
        "Cancel",
        this,
        juce::ModalCallbackFunction::create (
            [this, folder] (int result)
            {
                if (result != 1) return;
                if (! ProjectManager::deleteProject (folder))
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "Delete failed",
                        "Couldn't move the folder to the Recycle Bin.");
                    return;
                }
                refreshList();
            }));
}

void ProjectBrowserWindow::doShowInExplorer()
{
    const int row = mTable.getSelectedRow();
    if (row < 0 || row >= (int) mItems.size()) return;
    mItems[(size_t) row].folder.revealToUser();
}
