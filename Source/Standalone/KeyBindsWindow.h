#pragma once
#include <JuceHeader.h>
#include "KeyBindings.h"

// ── KeyBindsWindow ───────────────────────────────────────────────────────────
// "Key Binds" popup, opened from Help menu.  Three-tab layout, one tab per
// editable category (General / Builder / Piano Roll), plus a Mouse Reference
// section embedded at the bottom of each tab.
//
// Per-row tooltips are surfaced via TableListBoxModel::getCellTooltip - JUCE's
// stock KeyMappingEditorComponent has no equivalent, which is why the popup
// rolls its own table.  Mouse-reference rows are visually identical except
// the "Set" / "Reset" buttons are absent and the row text is dimmed.
// ─────────────────────────────────────────────────────────────────────────────

// One tab's worth of rows - editable command rows + non-editable mouse rows.
class KeyBindsTab : public juce::Component,
                    public juce::TableListBoxModel,
                    public juce::ChangeListener
{
public:
    KeyBindsTab (BSCommands::Category category,
                 juce::ApplicationCommandManager& mgr);
    ~KeyBindsTab() override;

    void resized() override;

    // ── TableListBoxModel ──────────────────────────────────────────────────
    int  getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int col, int w, int h, bool selected) override;
    juce::Component* refreshComponentForCell (int row, int col, bool selected,
                                              juce::Component* existing) override;
    juce::String getCellTooltip (int row, int col) override;

    // ── ChangeListener (KeyPressMappingSet) ────────────────────────────────
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    struct Row
    {
        bool         editable;     // true = command row, false = mouse-ref row
        int          commandId;    // valid when editable
        juce::String shortcut;     // displayed in shortcut column
        juce::String name;
        juce::String tooltip;
    };

    BSCommands::Category             mCategory;
    juce::ApplicationCommandManager& mMgr;
    juce::TableListBox               mTable;
    std::vector<Row>                 mRows;

    void rebuildRows();
    void promptKeyChange (int commandId);
    void resetCommand    (int commandId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBindsTab)
};

// Container component: hosts the TabbedComponent.  Owned by the DocumentWindow.
class KeyBindsContent : public juce::Component
{
public:
    explicit KeyBindsContent (juce::ApplicationCommandManager& mgr);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::TabbedComponent mTabs { juce::TabbedButtonBar::TabsAtTop };
    // Local tooltip window - the editor's main TooltipWindow only monitors
    // its own component tree, so the popup needs its own to show per-row hover
    // tips on the keymap rows.
    juce::TooltipWindow   mTooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBindsContent)
};

// The popup itself.  Self-deleting on close - owners hold a SafePointer.
class KeyBindsWindow : public juce::DocumentWindow
{
public:
    explicit KeyBindsWindow (juce::ApplicationCommandManager& mgr);

    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBindsWindow)
};
