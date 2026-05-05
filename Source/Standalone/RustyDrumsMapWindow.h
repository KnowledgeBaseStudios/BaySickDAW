#pragma once
#include <JuceHeader.h>

class BaySickRustyDrumsProcessor;

// ── RustyDrumsMapWindow ──────────────────────────────────────────────────────
// J-7b: Help-menu reference popup that lists every kit-native MIDI note
// alongside its sound type and articulation label.  Driven by the engine's
// PianoRollKey list (parsed at kit-load time from `keymap.sfz`); when no kit
// is loaded the table renders empty.  Style mirrors KeyBindsWindow — a single
// scrollable table inside a self-deleting DocumentWindow.
// ─────────────────────────────────────────────────────────────────────────────

class RustyDrumsMapTable : public juce::Component,
                           public juce::TableListBoxModel
{
public:
    explicit RustyDrumsMapTable (BaySickRustyDrumsProcessor* engine);
    ~RustyDrumsMapTable() override;

    void resized() override;
    int  getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int col, int w, int h, bool selected) override;

private:
    BaySickRustyDrumsProcessor* mEngine { nullptr };
    juce::TableListBox          mTable;
    struct Row { int midi; juce::String sound; juce::String articulation; juce::String defineName; };
    std::vector<Row>            mRows;

    void rebuildRows();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RustyDrumsMapTable)
};

class RustyDrumsMapWindow : public juce::DocumentWindow
{
public:
    explicit RustyDrumsMapWindow (BaySickRustyDrumsProcessor* engine);

    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RustyDrumsMapWindow)
};
