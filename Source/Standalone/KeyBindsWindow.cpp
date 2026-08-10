#include "KeyBindsWindow.h"
#include "SharedUI.h"   // VC palette + BaySickLAF
#include "WindowChrome.h"   // TS7 §9.3

namespace
{
    // Column IDs (must be > 0 for TableListBox).
    constexpr int kColAction   = 1;
    constexpr int kColShortcut = 2;
    constexpr int kColSet      = 3;
    constexpr int kColReset    = 4;

    constexpr int kRowH = 26;
}

// ─────────────────────────────────────────────────────────────────────────────
// KeyBindsTab
// ─────────────────────────────────────────────────────────────────────────────
KeyBindsTab::KeyBindsTab (BSCommands::Category category,
                          juce::ApplicationCommandManager& mgr)
    : mCategory (category), mMgr (mgr)
{
    addAndMakeVisible (mTable);
    mTable.setModel (this);
    mTable.setRowHeight (kRowH);
    mTable.setHeaderHeight (24);
    mTable.getViewport()->setScrollBarsShown (true, false);

    auto& h = mTable.getHeader();
    h.addColumn ("Action",    kColAction,   240, 120);
    h.addColumn ("Shortcut",  kColShortcut, 180, 100);
    h.addColumn ("Set",       kColSet,       60,  60, 60);
    h.addColumn ("Reset",     kColReset,     60,  60, 60);
    h.setStretchToFitActive (true);

    if (auto* set = mMgr.getKeyMappings())
        set->addChangeListener (this);

    rebuildRows();
}

KeyBindsTab::~KeyBindsTab()
{
    mTable.setModel (nullptr);   // detach before mRows destructs
    if (auto* set = mMgr.getKeyMappings())
        set->removeChangeListener (this);
}

void KeyBindsTab::resized()
{
    mTable.setBounds (getLocalBounds().reduced (4));
}

void KeyBindsTab::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildRows();
}

void KeyBindsTab::rebuildRows()
{
    mRows.clear();

    auto* set = mMgr.getKeyMappings();

    // Editable command rows for this category.
    for (auto* ci : BSCommands::getCommandsInCategory (mCategory))
    {
        Row r;
        r.editable  = true;
        r.commandId = ci->id;
        r.name      = ci->name;
        r.tooltip   = ci->tooltip;

        if (set != nullptr)
        {
            // G1 smoke (the F/H ghost-bind hunt): show EVERY binding, not
            // just the first - hidden extras were live but invisible here.
            auto keys = set->getKeyPressesAssignedToCommand (ci->id);
            if (keys.size() > 0)
            {
                juce::StringArray descs;
                for (const auto& k : keys)
                    descs.add (k.getTextDescription());
                r.shortcut = descs.joinIntoString (", ");
            }
            else
                r.shortcut = "(none)";
        }
        else
        {
            r.shortcut = ci->defaultKey.getTextDescription();
        }
        mRows.push_back (r);
    }

    // Mouse reference rows for this category - appended after the editable list.
    for (auto* mr : BSCommands::getMouseRefsInCategory (mCategory))
    {
        Row r;
        r.editable  = false;
        r.commandId = 0;
        r.shortcut  = mr->shortcut;
        r.name      = mr->name;
        r.tooltip   = mr->tooltip;
        mRows.push_back (r);
    }

    mTable.updateContent();
    mTable.repaint();
}

int KeyBindsTab::getNumRows() { return (int) mRows.size(); }

void KeyBindsTab::paintRowBackground (juce::Graphics& g, int row, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) mRows.size()) return;

    if (selected)
        g.fillAll (VC::Accent.withAlpha (0.20f));
    else if ((row % 2) == 0)
        g.fillAll (VC::Bg.brighter (0.05f));
    else
        g.fillAll (VC::Bg);

    // Subtle separator before the first non-editable (mouse-ref) row.
    if (row > 0 && mRows[(size_t) row].editable != mRows[(size_t) row - 1].editable)
    {
        g.setColour (VC::Accent.withAlpha (0.40f));
        g.fillRect (0, 0, w, 1);
    }
}

void KeyBindsTab::paintCell (juce::Graphics& g, int row, int col, int w, int h, bool /*selected*/)
{
    if (row < 0 || row >= (int) mRows.size()) return;
    const auto& r = mRows[(size_t) row];

    juce::Colour textCol = r.editable ? VC::Text : VC::Text.withAlpha (0.55f);
    g.setColour (textCol);
    g.setFont (juce::Font (13.0f));

    juce::String s;
    if      (col == kColAction)   s = r.name;
    else if (col == kColShortcut) s = r.shortcut;
    else                          s = {};   // Set / Reset cells use child components

    g.drawFittedText (s, 8, 0, w - 16, h, juce::Justification::centredLeft, 1);
}

juce::String KeyBindsTab::getCellTooltip (int row, int /*col*/)
{
    if (row < 0 || row >= (int) mRows.size()) return {};
    return mRows[(size_t) row].tooltip;
}

// Custom button cell - owned by the TableListBox via refreshComponentForCell.
namespace
{
    class CellButton : public juce::TextButton
    {
    public:
        explicit CellButton (const juce::String& label) : juce::TextButton (label)
        {
            setLookAndFeel (&BaySickLAF::get());
        }

        int  rowNumber  { -1 };
        int  commandId  {  0 };
        bool isReset    { false };
        std::function<void(int)> onActivate;     // payload = commandId

    private:
        void clicked() override
        {
            if (onActivate) onActivate (commandId);
        }
    };
}

juce::Component* KeyBindsTab::refreshComponentForCell (int row, int col, bool /*selected*/,
                                                       juce::Component* existing)
{
    if (row < 0 || row >= (int) mRows.size()) { delete existing; return nullptr; }
    const auto& r = mRows[(size_t) row];

    if (col != kColSet && col != kColReset) { delete existing; return nullptr; }

    // Mouse-reference rows: no buttons.
    if (! r.editable) { delete existing; return nullptr; }

    auto* btn = dynamic_cast<CellButton*> (existing);
    if (btn == nullptr)
    {
        btn = new CellButton (col == kColSet ? "Set" : "Reset");
        btn->isReset = (col == kColReset);
    }

    btn->rowNumber = row;
    btn->commandId = r.commandId;
    btn->onActivate = (col == kColSet)
        ? std::function<void(int)> ([this](int cmdId){ promptKeyChange (cmdId); })
        : std::function<void(int)> ([this](int cmdId){ resetCommand   (cmdId); });

    return btn;
}

// Modal capture content - a plain Component that owns the keyPressed override.
// Hosted inside a DialogWindow via LaunchOptions::launchAsync so focus + key
// routing land directly on us instead of an AlertWindow's button child.
namespace
{
    class CaptureContent : public juce::Component
    {
    public:
        CaptureContent (juce::ApplicationCommandManager& mgr, int cmdId,
                        const juce::String& cmdName)
            : mMgr (mgr), mCmdId (cmdId)
        {
            setSize (420, 170);
            setWantsKeyboardFocus (true);
            setOpaque (true);

            mLabel.setText ("Press a key combination for:\n\n  " + cmdName
                              + "\n\nPress Escape to cancel.",
                            juce::dontSendNotification);
            mLabel.setJustificationType (juce::Justification::centred);
            mLabel.setColour (juce::Label::textColourId, VC::Text);
            addAndMakeVisible (mLabel);

            // No focusable button - we want every keypress on this component.
            // Cancel handled via Esc inside keyPressed.
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (VC::Bg);
        }

        void resized() override
        {
            mLabel.setBounds (getLocalBounds().reduced (16));
        }

        void parentHierarchyChanged() override
        {
            // grabKeyboardFocus() during parentHierarchyChanged fires before
            // the DialogWindow is on-screen - JUCE silently drops the request.
            // Defer to the next message-loop tick so the focus lands cleanly.
            if (getParentComponent() == nullptr) return;

            juce::Component::SafePointer<CaptureContent> safe (this);
            juce::MessageManager::callAsync ([safe]
            {
                if (safe != nullptr) safe->grabKeyboardFocus();
            });
        }

        bool keyPressed (const juce::KeyPress& kp) override
        {
            if (kp == juce::KeyPress (juce::KeyPress::escapeKey))
            {
                dismiss (0);
                return true;
            }

            if (! kp.isValid()) return false;

            auto* set = mMgr.getKeyMappings();
            if (set == nullptr) { dismiss (0); return true; }

            // Check for an existing owner of this keypress.
            const juce::CommandID otherCmd = set->findCommandForKeyPress (kp);

            // Check page-local hardcoded conflicts (P / B / C tool letters,
            // Ctrl+G glue, etc).  These aren't editable; they always win on
            // the page that owns them.  We warn the user the new binding will
            // only fire when that page isn't focused.
            const auto hardcoded = BSCommands::findHardcodedConflicts (kp);

            // No editable conflict, or rebinding to same command's same key.
            if (otherCmd == 0 || otherCmd == (juce::CommandID) mCmdId)
            {
                if (hardcoded.empty())
                {
                    set->removeKeyPress (kp);
                    // G1 smoke (the F/H ghost-bind hunt): Set = REPLACE, not
                    // accumulate.  addKeyPress alone STACKED a new key onto
                    // the command's old one - and the row only displays the
                    // first, so the extra stayed invisible but live (Jeff's
                    // keymap.xml carried five such strays: 2/H->play,
                    // F/G->record, D->stop).
                    set->clearAllKeyPresses (mCmdId);
                    set->addKeyPress (mCmdId, kp);
                    BSCommands::saveMappings (*set);
                    dismiss (1);
                    return true;
                }

                // Hardcoded-only conflict path - warn and confirm.
                juce::String list;
                for (auto* r : hardcoded)
                    list += "    \"" + r->name + "\" ("
                          + BSCommands::categoryName (r->category) + ")\n";

                auto& mgrRef = mMgr;
                const int        newCmd = mCmdId;
                const juce::KeyPress kpCopy = kp;

                dismiss (2);

                juce::AlertWindow::showOkCancelBox (
                    juce::MessageBoxIconType::QuestionIcon,
                    "Key Hardcoded On Another Page",
                    kpCopy.getTextDescription()
                        + " is hardcoded for:\n\n" + list
                        + "\nThe binding will only fire when those pages are NOT in focus.\n\n"
                          "Use the binding anyway?",
                    "Use Anyway",
                    "Cancel",
                    nullptr,
                    juce::ModalCallbackFunction::create (
                        [&mgrRef, newCmd, kpCopy] (int result)
                        {
                            if (result != 1) return;
                            if (auto* s = mgrRef.getKeyMappings())
                            {
                                s->removeKeyPress (kpCopy);
                                s->clearAllKeyPresses ((juce::CommandID) newCmd);   // replace, don't stack
                                s->addKeyPress ((juce::CommandID) newCmd, kpCopy);
                                BSCommands::saveMappings (*s);
                            }
                        }));
                return true;
            }

            // Editable conflict - close this prompt and ask whether to steal.
            // Hardcoded conflicts are listed inline in the same prompt so the
            // user has the full picture in one click.
            const juce::String otherName = [otherCmd]
            {
                if (auto* ci = BSCommands::findCommand ((int) otherCmd))
                    return ci->name;
                return juce::String ("Command ") + juce::String ((int) otherCmd);
            }();

            juce::String hardNote;
            if (! hardcoded.empty())
            {
                hardNote = "\n\nIt's also hardcoded for:\n";
                for (auto* r : hardcoded)
                    hardNote += "    \"" + r->name + "\" ("
                              + BSCommands::categoryName (r->category) + ")\n";
                hardNote += "(Page-local hardcoded bindings always win when "
                            "that page is in focus.)";
            }

            auto& mgrRef = mMgr;
            const int        newCmd = mCmdId;
            const juce::KeyPress kpCopy = kp;

            dismiss (2);

            juce::AlertWindow::showOkCancelBox (
                juce::MessageBoxIconType::QuestionIcon,
                "Key Already In Use",
                kpCopy.getTextDescription() + " is already assigned to:\n\n    "
                    + otherName + hardNote + "\n\nReplace it with the new binding?",
                "Replace",
                "Cancel",
                nullptr,
                juce::ModalCallbackFunction::create (
                    [&mgrRef, newCmd, kpCopy] (int result)
                    {
                        if (result != 1) return;
                        if (auto* s = mgrRef.getKeyMappings())
                        {
                            s->removeKeyPress (kpCopy);
                            s->clearAllKeyPresses ((juce::CommandID) newCmd);   // replace, don't stack
                            s->addKeyPress ((juce::CommandID) newCmd, kpCopy);
                            BSCommands::saveMappings (*s);
                        }
                    }));
            return true;
        }

    private:
        void dismiss (int rc)
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (rc);
        }

        juce::Label                      mLabel;
        juce::ApplicationCommandManager& mMgr;
        int                              mCmdId;
    };
}

void KeyBindsTab::promptKeyChange (int commandId)
{
    if (mMgr.getKeyMappings() == nullptr) return;

    auto* ci = BSCommands::findCommand (commandId);
    const juce::String name = (ci != nullptr ? ci->name : juce::String ("?"));

    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle                  = "Set Shortcut";
    o.dialogBackgroundColour       = VC::Bg;
    o.content.setOwned             (new CaptureContent (mMgr, commandId, name));
    o.componentToCentreAround      = this;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar            = false;   // TS7 §9.3
    o.resizable                    = false;
    // TS7 §9.4: the always-on-top on this one STAYS.  It is not the same case as
    // the Key Binds / Undo History windows -- this is the modal shortcut-capture
    // prompt, which must sit above the very window that launched it while the
    // user presses a key combination, and it lives only for that gesture.
    if (auto* dw = o.launchAsync())
        dw->setAlwaysOnTop (true);
}

void KeyBindsTab::resetCommand (int commandId)
{
    auto* set = mMgr.getKeyMappings();
    if (set == nullptr) return;

    set->resetToDefaultMapping (commandId);
    BSCommands::saveMappings (*set);
}

// ─────────────────────────────────────────────────────────────────────────────
// KeyBindsContent
// ─────────────────────────────────────────────────────────────────────────────
KeyBindsContent::KeyBindsContent (juce::ApplicationCommandManager& mgr)
{
    mTabs.setColour (juce::TabbedComponent::backgroundColourId, VC::Bg);
    mTabs.setOutline (0);
    mTabs.setLookAndFeel (&BaySickLAF::get());

    mTabs.addTab (BSCommands::categoryName (BSCommands::Category::General),
                  VC::Bg, new KeyBindsTab (BSCommands::Category::General,   mgr), true);
    mTabs.addTab (BSCommands::categoryName (BSCommands::Category::Builder),
                  VC::Bg, new KeyBindsTab (BSCommands::Category::Builder,   mgr), true);
    mTabs.addTab (BSCommands::categoryName (BSCommands::Category::PianoRoll),
                  VC::Bg, new KeyBindsTab (BSCommands::Category::PianoRoll, mgr), true);
    // 2026-04-26 (D-7 sub-2): Drum-kit-specific keybind reference.  Mirrors
    // Piano Roll except for pitch-based ops; documented row-by-row.
    mTabs.addTab (BSCommands::categoryName (BSCommands::Category::DrumKit),
                  VC::Bg, new KeyBindsTab (BSCommands::Category::DrumKit,   mgr), true);
    // 2026-07-11 (QA-Fd): vocal-editor (BaySickPitch / BaySickAlign)
    // reference.  Page-local hardcoded keys + mouse gestures - documented,
    // not rebindable (same deal as the Piano Roll's local keys).
    mTabs.addTab (BSCommands::categoryName (BSCommands::Category::VocalEditors),
                  VC::Bg, new KeyBindsTab (BSCommands::Category::VocalEditors, mgr), true);
    // 2026-08-06 (QA-UndoCoverage Task 5, docket 14=a): Event Editor local
    // keys - display-only reference rows, none rebindable.
    mTabs.addTab (BSCommands::categoryName (BSCommands::Category::EventEditor),
                  VC::Bg, new KeyBindsTab (BSCommands::Category::EventEditor, mgr), true);

    addAndMakeVisible (mTabs);
}

void KeyBindsContent::resized()
{
    mTabs.setBounds (getLocalBounds().reduced (6));
}

void KeyBindsContent::paint (juce::Graphics& g)
{
    g.fillAll (VC::Bg);
}

// ─────────────────────────────────────────────────────────────────────────────
// KeyBindsWindow
// ─────────────────────────────────────────────────────────────────────────────
KeyBindsWindow::KeyBindsWindow (juce::ApplicationCommandManager& mgr)
    : juce::DocumentWindow ("Key Binds",
                            VC::Bg,
                            juce::DocumentWindow::closeButton)
{
    WindowChrome::applyToDesktopWindow (*this);   // TS7 §9.3
    setResizable (true, true);
    setContentOwned (new KeyBindsContent (mgr), true);
    // 2026-04-26 (B-6): bumped from 640x500 - Builder + Piano Roll tabs are
    // now densely populated, the larger size keeps tooltips + Set/Reset
    // buttons reachable without horizontal scroll.
    centreWithSize (880, 680);
    setVisible (true);
    toFront (true);
}

void KeyBindsWindow::closeButtonPressed()
{
    delete this;
}
