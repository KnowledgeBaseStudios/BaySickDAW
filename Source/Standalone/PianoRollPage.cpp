#include "PianoRollPage.h"
#include "SharedUI.h"
#include "StandaloneApp.h"   // StandalonePlayHead
#include "../G3PlayheadDiag.h"   // [G3 PLAYHEAD] G-9 reading (QA-G3Smoke Task 1); Debug-only

// QA-D STATE-02 follow-on: shared composer for the piano-roll context label.
// Format: "{displayName} - {engineType-or-(no engine)}".  Mirrors the
// per-page LayersPage/BassPage/DrumPage::refreshPianoRollContextLabel format
// so the visible label (owned by PianoRollPage post-D-5 unification) matches
// the design documented in CLAUDE.md 5F-6.
static juce::String composeContextLabel (const PianoRollConnection& conn)
{
    const juce::String engine = conn.engineType.isEmpty()
                                    ? juce::String ("(no engine)")
                                    : conn.engineType;
    return conn.displayName + " - " + engine;
}

PianoRollPage::PianoRollPage()
{
    mDrumKit = std::make_unique<DrumKitContainer>();
    addAndMakeVisible (*mDrumKit);

    // Residual (b) fix: 60 Hz halves the playhead's visual trail; affordable
    // because the grids now dirty-rect their playhead repaints (the old
    // full-canvas repaint per tick would have doubled instead).
    startTimerHz (60);
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
    // Live-note monitor: push the processor's held hardware-MIDI notes to the
    // active roll's keyboard so live-played keys light up.  Runs before the
    // playhead early-return below so it works even with no playhead attached.
    if (liveHeldNotesProvider)
        if (auto* roll = getActivePianoRoll())
        {
            uint64_t lo = 0, hi = 0;
            liveHeldNotesProvider (lo, hi);
            roll->setLiveHeldNotes (lo, hi);
        }

    // QA-H Task 7 (docket 4b): ghost-note producer.  Every OTHER registered
    // instrument roll feeds the active roll, tinted with its source tab's
    // note color.  dataAccessor closures are live (pattern switches track),
    // and the ghost render reads the note vectors at paint time, so only the
    // (pointer, color) SET needs re-pushing - change-guarded because the
    // grid repaints on every setGhostData.  Runs before the playhead
    // early-return so ghosts work with no playhead attached.
    if (auto* active = getActivePianoRoll())
    {
        std::vector<std::pair<const PianoRollData*, juce::Colour>> ghosts;
        for (auto& kv : mConns)
        {
            if (kv.first == mActive || ! kv.second.dataAccessor) continue;
            if (auto* d = kv.second.dataAccessor())
                if (! d->notes.empty())
                    ghosts.push_back ({ d, kv.second.noteColor });
        }
        std::sort (ghosts.begin(), ghosts.end(),
                   [] (const auto& a, const auto& b) { return a.first < b.first; });
        if (ghosts != mLastGhosts)
        {
            mLastGhosts = ghosts;
            active->setGhostData (ghosts);
        }
    }

    if (mPlayHead == nullptr) return;
    // #30 (QA-G3Smoke): while PLAYING, shift the visual playhead back by the
    // output latency (mirrors the Builder) so the line sits where the user
    // HEARS the beat.  Stopped = raw position (a seek parks exactly where
    // clicked).  Covers the roll AND the drum kit -- both consume this pump.
    double cur = mPlayHead->getCurrentBeat();
    if (mPlayHead->isPlaying() && deviceInfoProvider)
    {
        int lat = 0; double sr = 0.0;
        deviceInfoProvider (lat, sr);
        const double bpm = mPlayHead->getBPM();
        if (sr > 0.0 && bpm > 0.0)
            cur -= (double) lat / sr * bpm / 60.0;
    }
    const bool song = (isSongMode && isSongMode());
    // #31 (QA-G3Smoke): song mode shows the playhead IFF the viewed pattern is
    // playing under it (pattern-local beat via the editor's block search; no
    // modulo -- tiling is gone).  Pattern mode: the loop-local beat as before.
    const double beat = ! song ? cur
                       : (songLocalBeatProvider ? songLocalBeatProvider (cur) : -1.0);

#if JUCE_DEBUG
    if (beat >= 0.0 && mPlayHead->isPlaying())
    {
        int lat = -1; double sr = 0.0;
        if (g3DiagDeviceInfo) g3DiagDeviceInfo (lat, sr);
        G3PlayheadDiag::log ("tick beat=" + juce::String (beat, 4)
                             + " latSamples=" + juce::String (lat)
                             + " sr=" + juce::String (sr, 1));
    }
#endif

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

void PianoRollPage::setContentEditedHook (std::function<void()> fn)
{
    mContentEditedHook = std::move (fn);
    if (mDrumKit) mDrumKit->onContentEdited = mContentEditedHook;
    for (auto& kv : mRolls)
        if (kv.second) kv.second->onContentEdited = mContentEditedHook;
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
    roll->setContextLabel (composeContextLabel (conn));   // QA-D STATE-02 follow-on
    roll->setUndoContext (mUndoCtx);
    if (mSnapGetter) roll->setSnapAccessors (mSnapGetter, mSnapSetter);   // QA-Ee Stage 3: global snap
    if (mQuantizeGetter) roll->setQuantizeAccessors (mQuantizeGetter, mQuantizeSetter);   // QA-UICleanup Task 4
    if (conn.rollMode != PianoRollContainer::RollMode::Standard)
        roll->setRollMode (conn.rollMode);
    if (conn.auditionMomentary) roll->onNoteAudition    = conn.auditionMomentary;
    if (conn.auditionOn)        roll->onNoteAuditionOn  = conn.auditionOn;
    if (conn.auditionOff)       roll->onNoteAuditionOff = conn.auditionOff;
    if (conn.noteLabelProvider) roll->setNoteLabelProvider (conn.noteLabelProvider);
    if (conn.keyswitchLabelProvider) roll->setKeyswitchLabelProvider (conn.keyswitchLabelProvider);
    // QA-SlideSampler Task 4: set unconditionally so switching to a non-sfizz engine
    // clears the guitar/bass note-props mode (null provider -> normal panel).
    roll->setNoteEditContextProvider (conn.noteEditContextProvider);
    roll->onContentEdited = mContentEditedHook;   // #30b regression fix
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

// QA-Ee Stage 3: wire the global snap accessors into the drum kit + every roll
// (future registrations pick them up in registerEngine).
void PianoRollPage::setSnapAccessors (std::function<int()> getter, std::function<void(int)> setter)
{
    mSnapGetter = std::move (getter);
    mSnapSetter = std::move (setter);
    if (mDrumKit) mDrumKit->setSnapAccessors (mSnapGetter, mSnapSetter);
    for (auto& kv : mRolls)
        if (kv.second) kv.second->setSnapAccessors (mSnapGetter, mSnapSetter);
}

// QA-UICleanup Task 4: wire the global quantize accessors into the drum kit +
// every roll (future registrations pick them up in registerEngine).
void PianoRollPage::setQuantizeAccessors (std::function<int()> getter, std::function<void(int)> setter)
{
    mQuantizeGetter = std::move (getter);
    mQuantizeSetter = std::move (setter);
    if (mDrumKit) mDrumKit->setQuantizeAccessors (mQuantizeGetter, mQuantizeSetter);
    for (auto& kv : mRolls)
        if (kv.second) kv.second->setQuantizeAccessors (mQuantizeGetter, mQuantizeSetter);
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
    if (rIt != mRolls.end() && rIt->second && cIt != mConns.end())
        rIt->second->setContextLabel (composeContextLabel (cIt->second));   // QA-D STATE-02 follow-on
    if (mActive == id && onEngineSelected)
        onEngineSelected (mActive);   // refresh menu-bar pill label
}

// QA-D STATE-02 follow-on: engineType updates land via this entry point.
// Wired by StandaloneEditor on every onEngineSelected callback for Layer /
// Bass / Drum tabs.  Refreshes the context label to "{displayName} -
// {engineType}".  Empty engineType reverts the label to "{displayName} -
// (no engine)".
void PianoRollPage::setEngineType (EngineId id, const juce::String& engineType)
{
    auto cIt = mConns.find (id);
    if (cIt == mConns.end()) return;
    cIt->second.engineType = engineType;
    auto rIt = mRolls.find (id);
    if (rIt != mRolls.end() && rIt->second)
        rIt->second->setContextLabel (composeContextLabel (cIt->second));
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
