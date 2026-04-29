#pragma once
#include <JuceHeader.h>
#include "SharedUI.h"

class VibeSynthProcessor;
class PatternManager;

// ── InstrumentPage ────────────────────────────────────────────────────────────
// Base class for Layers, Bass, and Drums page tabs.
//
// Layers / Bass:
//   - Opens with a full-screen engine-selector ComboBox.
//   - Once an engine is chosen the dropdown locks and 3 sub-tabs appear:
//       [Sound]  [Piano Roll]  [EQ]
//   - The sub-tab content is provided by the concrete subclass.
//
// Drums:
//   - Skips the selector; always shows 14-row piano roll layout.
// ─────────────────────────────────────────────────────────────────────────────

class InstrumentPage : public juce::Component
{
public:
    enum class EngineType { None, Harmless, VibePlayer, Legacy808909, BaySickSynth };

    explicit InstrumentPage(VibeSynthProcessor& proc, PatternManager& pm);
    ~InstrumentPage() override = default;

    // ── Sub-tab indices ───────────────────────────────────────────────────────
    static constexpr int SUB_SOUND    = 0;
    static constexpr int SUB_PIANOROLL = 1;
    static constexpr int SUB_EQ       = 2;

    // ── State ─────────────────────────────────────────────────────────────────
    EngineType getEngine()         const { return mEngine; }
    bool       hasEngineSelected() const { return mEngine != EngineType::None; }

    // Mixer graph node ID — set by StandaloneEditor in Phase 5
    int  getMixerTrackId()   const { return mMixerTrackId; }
    void setMixerTrackId(int id)   { mMixerTrackId = id; }

    // ── Layout ────────────────────────────────────────────────────────────────
    void paint(juce::Graphics&) override;
    void resized() override;

protected:
    VibeSynthProcessor& mProc;
    PatternManager&     mPM;

    // ── Engine selector (shown before an engine is picked) ───────────────────
    // Subclass must call setupEngineDropdown() with its allowed options.
    void setupEngineDropdown(const juce::StringArray& options,
                             const juce::Array<EngineType>& types);

    // Called after user picks an engine and the selector is committed.
    // Override to build Sound / Piano Roll / EQ content components.
    virtual void onEngineChosen(EngineType engine) = 0;

    // Helper: add a component to a sub-tab. Call from onEngineChosen().
    // idx = SUB_SOUND / SUB_PIANOROLL / SUB_EQ
    void setSubTabContent(int idx, juce::Component* content);

    juce::TabbedComponent mSubTabs { juce::TabbedButtonBar::TabsAtTop };

private:
    EngineType mEngine       { EngineType::None };
    int        mMixerTrackId { -1 };

    // Selector panel (shown before engine chosen)
    std::unique_ptr<juce::Component> mSelectorPanel;
    std::unique_ptr<juce::Label>     mSelectorLabel;
    std::unique_ptr<juce::ComboBox>  mEngineBox;
    juce::Array<EngineType>          mEngineTypes;

    void buildSelectorPanel(const juce::StringArray& options,
                            const juce::Array<EngineType>& types);
    void commitEngine(EngineType e);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentPage)
};
