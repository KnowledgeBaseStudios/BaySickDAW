#include "UndoHistoryWindow.h"
#include "SharedUI.h"

// ─────────────────────────────────────────────────────────────────────────────
// Content
// ─────────────────────────────────────────────────────────────────────────────
UndoHistoryWindow::Content::Content(const std::deque<juce::String>& labels,
                                     const int& cursor,
                                     std::function<void()> globalUndo,
                                     std::function<void()> globalRedo)
    : mLabels(labels), mCursor(cursor)
    , mGlobalUndo(std::move(globalUndo))
    , mGlobalRedo(std::move(globalRedo))
{
    mList.setModel(this);
    mList.setRowHeight(20);
    mList.setColour(juce::ListBox::backgroundColourId, VC::Panel);
    mList.setColour(juce::ListBox::outlineColourId,    VC::Accent);
    addAndMakeVisible(mList);
    refresh();
}

void UndoHistoryWindow::Content::refresh()
{
    mItems.clear();

    // 2026-04-26: ASCII-only marker glyphs (was UTF-8 ↓ / ● / ↑ which rendered
    // as tofu boxes on the default font).  Now uses pure ASCII so labels read
    // cleanly on any system.
    // Past items (oldest first - index 0 is oldest action still in history)
    for (int i = 0; i < mCursor; ++i)
        mItems.push_back({ "    " + mLabels[i], false, false });

    // Current state marker
    mCurrentRow = (int)mItems.size();
    mItems.push_back({ ">>  Current", true, false });

    // Future items (redo available, below the marker)
    for (int i = mCursor; i < (int)mLabels.size(); ++i)
        mItems.push_back({ "    " + mLabels[i], false, true });

    mList.updateContent();
    mList.repaint();   // 2026-04-26: force redraw - updateContent may not repaint cells reliably
    mList.selectRow (mCurrentRow, /*dontScroll*/ false, /*deselectOthersFirst*/ true);
    mList.scrollToEnsureRowIsOnscreen(mCurrentRow);
}

void UndoHistoryWindow::Content::resized()
{
    mList.setBounds(getLocalBounds());
}

int UndoHistoryWindow::Content::getNumRows()
{
    return (int)mItems.size();
}

void UndoHistoryWindow::Content::paintListBoxItem(int row, juce::Graphics& g,
                                                    int w, int h, bool selected)
{
    if (row < 0 || row >= (int)mItems.size()) return;
    const auto& item = mItems[row];

    if (item.isCurrent)
        g.setColour(VC::Blue.withAlpha(0.25f));
    else if (selected)
        g.setColour(VC::Accent);
    else
        g.setColour(VC::Panel);
    g.fillRect(0, 0, w, h);

    // Divider line above the "Current" marker
    if (item.isCurrent)
    {
        g.setColour(VC::Blue.withAlpha(0.6f));
        g.drawLine(0.f, 0.f, (float)w, 0.f, 1.f);
    }

    juce::Colour textCol = item.isFuture   ? VC::TextDim
                         : item.isCurrent  ? VC::Blue
                                           : VC::Text;
    g.setColour(textCol);
    g.setFont(juce::Font(11.0f));
    g.drawText(item.text, 4, 0, w - 4, h, juce::Justification::centredLeft, true);
}

void UndoHistoryWindow::Content::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int)mItems.size()) return;
    if (mItems[row].isCurrent) return;

    if (row < mCurrentRow)
    {
        int steps = mCurrentRow - row;
        for (int i = 0; i < steps; ++i) mGlobalUndo();
    }
    else
    {
        int steps = row - mCurrentRow;
        for (int i = 0; i < steps; ++i) mGlobalRedo();
    }

    refresh();
}

// ─────────────────────────────────────────────────────────────────────────────
// UndoHistoryWindow
// ─────────────────────────────────────────────────────────────────────────────
UndoHistoryWindow::UndoHistoryWindow(juce::UndoManager&               manager,
                                     const std::deque<juce::String>&  labels,
                                     const int&                        cursor,
                                     std::function<void()>             globalUndo,
                                     std::function<void()>             globalRedo)
    : juce::DocumentWindow("Undo History",
                            VC::Panel,
                            juce::DocumentWindow::closeButton,
                            true)
    , mManager(manager)
    , mLabels(labels)
    , mCursor(cursor)
    , mGlobalUndo(std::move(globalUndo))
    , mGlobalRedo(std::move(globalRedo))
{
    setUsingNativeTitleBar(true);
    setResizable(true, false);

    auto* content = new Content(mLabels, mCursor, mGlobalUndo, mGlobalRedo);
    mContent = content;
    setContentOwned(content, false);
    setSize(220, 400);
    centreWithSize(220, 400);
    setVisible(true);
    // 2026-04-26: was `setAlwaysOnTop(false)` → main app window stole focus
    // back immediately, hiding the history panel behind it.  Always-on-top
    // keeps it visible without going modal so the user can still click into
    // the editor while the panel is open.
    setAlwaysOnTop(true);
    toFront(true);

    mManager.addChangeListener(this);
}

UndoHistoryWindow::~UndoHistoryWindow()
{
    mManager.removeChangeListener(this);
}

void UndoHistoryWindow::refresh()
{
    if (mContent) mContent->refresh();
}

void UndoHistoryWindow::closeButtonPressed()
{
    setVisible(false);
}

void UndoHistoryWindow::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refresh();
}

// 2026-04-26: Ctrl+Z / Ctrl+Alt+Z dispatch handled by registering the
// editor's KeyPressMappingSet as a KeyListener on this window in
// `StandaloneEditor::showHistoryWindow` - same command path as the main
// editor uses, so cursor moves whether the history window has focus or not.
