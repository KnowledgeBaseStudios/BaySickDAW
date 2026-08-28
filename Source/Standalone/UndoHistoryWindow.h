#pragma once
#include <JuceHeader.h>
#include <deque>
#include <functional>

// ── UndoHistoryWindow ─────────────────────────────────────────────────────────
// Floating window showing the full undo/redo history as a scrollable list.
// Supports time-travel: clicking a past or future item steps undo/redo to
// reach that state.
//
// Owned by StandaloneEditor. References the editor's mHistoryLabels deque and
// mHistoryCursor directly (no copy); refreshed via ChangeListener on the
// UndoManager (which is a ChangeBroadcaster).
// ─────────────────────────────────────────────────────────────────────────────
class UndoHistoryWindow : public juce::DocumentWindow,
                          public juce::ChangeListener
{
public:
    // showOnConstruct=false (QA-ManualPress) builds the window fully laid
    // out but never adds it to the desktop - the shot harness snapshots it.
    UndoHistoryWindow(juce::UndoManager&               manager,
                      const std::deque<juce::String>&  labels,
                      const int&                        cursor,
                      std::function<void()>             globalUndo,
                      std::function<void()>             globalRedo,
                      bool                              showOnConstruct = true);
    ~UndoHistoryWindow() override;

    void refresh();

    // juce::DocumentWindow
    void closeButtonPressed() override;

    // juce::ChangeListener (fires when UndoManager state changes)
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

private:
    juce::UndoManager&               mManager;
    const std::deque<juce::String>&  mLabels;
    const int&                        mCursor;
    std::function<void()>             mGlobalUndo;
    std::function<void()>             mGlobalRedo;

    // ── Content component ─────────────────────────────────────────────────────
    class Content : public juce::Component, public juce::ListBoxModel
    {
    public:
        Content(const std::deque<juce::String>& labels,
                const int& cursor,
                std::function<void()> globalUndo,
                std::function<void()> globalRedo);

        void refresh();
        void resized() override;

        // ListBoxModel
        int  getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& g,
                              int width, int height, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;

    private:
        const std::deque<juce::String>& mLabels;
        const int&                       mCursor;
        std::function<void()>            mGlobalUndo;
        std::function<void()>            mGlobalRedo;

        juce::ListBox mList;

        // Flat list: past entries (0..cursor-1), "* Current" marker, future (cursor..end)
        struct Item
        {
            juce::String text;
            bool         isCurrent { false };
            bool         isFuture  { false };
        };
        std::vector<Item> mItems;
        int               mCurrentRow { 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Content)
    };

    Content* mContent { nullptr };  // owned by the DocumentWindow content component

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoHistoryWindow)
};
