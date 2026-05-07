#include "PianoRollPage.h"
#include "SharedUI.h"
#include "StandaloneApp.h"   // StandalonePlayHead

PianoRollPage::PianoRollPage()
{
    mDrumKit = std::make_unique<DrumKitContainer>();
    addAndMakeVisible (*mDrumKit);

    startTimerHz (30);
}

PianoRollPage::~PianoRollPage()
{
    stopTimer();
}

void PianoRollPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d0d));
}

void PianoRollPage::resized()
{
    applyActiveVisibility();
}

void PianoRollPage::timerCallback()
{
    if (mPlayHead == nullptr) return;
    const bool song = (isSongMode && isSongMode());
    const double beat = song ? -1.0 : mPlayHead->getCurrentBeat();

    // Drum Kit pump (always alive - even when not the active view, the kit
    // keeps its playhead in sync so switching back is seamless).
    if (mDrumKit) mDrumKit->setPlayheadBeat (beat);

    // Refresh the active piano-roll container's data binding (pattern switch
    // moves the underlying slice in PatternManager) and pump the playhead.
    if (mActive.kind != EngineKind::DrumKit)
    {
        auto rollIt = mRolls.find (mActive);
        auto connIt = mConns.find (mActive);
        if (rollIt != mRolls.end() && connIt != mConns.end()
            && rollIt->second != nullptr)
        {
            if (connIt->second.dataAccessor)
                rollIt->second->setData (connIt->second.dataAccessor());
            // C.5b: refresh pattern TS so bar lines re-space on pattern change.
            if (connIt->second.patternTimeSigProvider)
            {
                int n = 4, d = 4;
                connIt->second.patternTimeSigProvider (n, d);
                rollIt->second->setTimeSignature (n, d);
            }
            rollIt->second->setPlayheadBeat (beat);
        }
    }
}

void PianoRollPage::setPlayHead (StandalonePlayHead* ph)
{
    mPlayHead = ph;
}

void PianoRollPage::setUndoContext (const UndoContext& ctx)
{
    mUndoCtx = ctx;
    if (mDrumKit) mDrumKit->setUndoContext (ctx);
    for (auto& kv : mRolls)
        if (kv.second) kv.second->setUndoContext (ctx);
}

void PianoRollPage::registerEngine (EngineId id, PianoRollConnection conn)
{
    // Replace any prior registration (defensive: caller may unregister-then-
    // register on engine swap).
    unregisterEngine (id);

    auto roll = std::make_unique<PianoRollContainer>();
    if (conn.dataAccessor) roll->setData (conn.dataAccessor());
    // C.5b: prime pattern TS at registration so the grid is correct from the
    // very first paint, not deferred to the first timer tick.
    if (conn.patternTimeSigProvider)
    {
        int n = 4, d = 4;
        conn.patternTimeSigProvider (n, d);
        roll->setTimeSignature (n, d);
    }
    roll->setNoteColor (conn.noteColor);
    roll->setContextLabel (conn.displayName);
    roll->setUndoContext (mUndoCtx);
    if (conn.rollMode != PianoRollContainer::RollMode::Standard)
        roll->setRollMode (conn.rollMode);
    if (conn.auditionMomentary) roll->onNoteAudition    = conn.auditionMomentary;
    if (conn.auditionOn)        roll->onNoteAuditionOn  = conn.auditionOn;
    if (conn.auditionOff)       roll->onNoteAuditionOff = conn.auditionOff;
    if (conn.noteLabelProvider) roll->setNoteLabelProvider (conn.noteLabelProvider);
    if (conn.defaultTopNote >= 0) roll->setTopNote (conn.defaultTopNote);
    if (conn.allKeysWhite) roll->setAllKeysWhiteMode (true);
    if (mPlayHead)
        roll->onSeek = [ph = mPlayHead](double b) { if (ph) ph->seekTo (b); };
    if (mUndoCtx.showHistory)
        roll->onShowHistoryWindow = mUndoCtx.showHistory;

    addChildComponent (*roll);   // not visible until selected
    mRolls[id] = std::move (roll);
    mConns[id] = std::move (conn);

    applyActiveVisibility();
}

void PianoRollPage::unregisterEngine (EngineId id)
{
    auto it = mRolls.find (id);
    if (it != mRolls.end())
    {
        if (it->second) removeChildComponent (it->second.get());
        mRolls.erase (it);
    }
    mConns.erase (id);

    // If the active engine was the one we just removed, fall back to Drum Kit.
    if (mActive == id)
    {
        mActive = { EngineKind::DrumKit, 0 };
        applyActiveVisibility();
        if (onEngineSelected) onEngineSelected (mActive);
    }
}

void PianoRollPage::selectEngine (EngineId id)
{
    if (id.kind != EngineKind::DrumKit && mRolls.find (id) == mRolls.end())
        return;   // unknown engine - ignore
    mActive = id;
    applyActiveVisibility();
    if (onEngineSelected) onEngineSelected (mActive);
}

void PianoRollPage::setEngineDisplayName (EngineId id, const juce::String& name)
{
    auto cIt = mConns.find (id);
    if (cIt != mConns.end()) cIt->second.displayName = name;
    auto rIt = mRolls.find (id);
    if (rIt != mRolls.end() && rIt->second)
        rIt->second->setContextLabel (name);
    if (mActive == id && onEngineSelected)
        onEngineSelected (mActive);   // refresh menu-bar pill label
}

PianoRollContainer* PianoRollPage::getActivePianoRoll() const
{
    if (mActive.kind == EngineKind::DrumKit) return nullptr;
    auto it = mRolls.find (mActive);
    return it != mRolls.end() ? it->second.get() : nullptr;
}

void PianoRollPage::applyActiveVisibility()
{
    auto bounds = getLocalBounds();

    if (mDrumKit)
    {
        const bool active = (mActive.kind == EngineKind::DrumKit);
        mDrumKit->setVisible (active);
        if (active) mDrumKit->setBounds (bounds);
    }

    for (auto& kv : mRolls)
    {
        if (! kv.second) continue;
        const bool active = (kv.first == mActive);
        kv.second->setVisible (active);
        if (active) kv.second->setBounds (bounds);
    }
}

juce::PopupMenu PianoRollPage::buildEngineDropdown()
{
    juce::PopupMenu m;
    // Drum Kit always at the top.
    const EngineId kitId { EngineKind::DrumKit, 0 };
    m.addItem (1, "Drum Kit", true, mActive == kitId);
    if (dropdownEnumerator)
    {
        const auto entries = dropdownEnumerator();
        if (! entries.empty()) m.addSeparator();
        int nextItemId = 100;
        for (const auto& e : entries)
            m.addItem (nextItemId++, e.label, true, mActive == e.id);
    }
    return m;
}
