#include "PatternManager.h"
#include "ProjectFileResolver.h"
#include "TsMapRead.h"

// ── AutomationLane::evaluateAt ────────────────────────────────────────────────
float AutomationLane::evaluateAt(float pos01) const
{
    return evalAutomationLaneAt (*this, pos01);
}

PatternManager::PatternManager()
{
    for (int i = 0; i < kMaxArrangementRows; ++i)
        mRowNames[i] = defaultRowName(i);
    addPattern("Pattern 1");   // publishes the first roll-snapshot table via notifyContentChanged
    publishTimeSigMap();   // TsMap live from startup (implicit 4/4)
}

PatternManager::~PatternManager()
{
    // #30b teardown mirrors the AudioClipSnapshot RCU: audio is stopped by the
    // time the owner destroys us, so the ACTIVE table is ours to free here;
    // queue-retired tables are destroyed by mRollRetirement's own teardown
    // (drainer joined in its destructor, which runs after this body).
    if (auto* last = mActiveRollSnapshot.exchange (nullptr, std::memory_order_acq_rel))
        delete last;
}

// ── QA-G3Smoke #30b: roll-snapshot build + publish (message thread only) ────
std::shared_ptr<const PatternRollsSnapshot> PatternManager::buildPatternRollsSnapshot (int patternIndex) const
{
    auto snap = std::make_shared<PatternRollsSnapshot>();
    if (patternIndex < 0 || patternIndex >= (int) mPatterns.size())
        return snap;
    const auto& pat = mPatterns[(size_t) patternIndex];
    for (int i = 0; i < kMaxLayerPages; ++i) snap->layerNotes[(size_t) i] = pat.layerRoll[(size_t) i].notes;
    for (int i = 0; i < kMaxBassPages;  ++i) snap->bassNotes[(size_t) i]  = pat.bassRoll[(size_t) i].notes;
    for (int i = 0; i < kMaxDrumPages;  ++i) snap->drumNotes[(size_t) i]  = pat.drumRolls[(size_t) i].notes;
    for (int i = 0; i < kMaxClipPages;  ++i) snap->clipNotes[(size_t) i]  = pat.clipRoll[(size_t) i].notes;
    for (int i = 0; i < kMaxVoxPages;   ++i) snap->voxNotes[(size_t) i]   = pat.voxRoll[(size_t) i].notes;
    for (int i = 0; i < kMaxInstPages;  ++i) snap->instNotes[(size_t) i]  = pat.instRoll[(size_t) i].notes;
    for (int i = 0; i < kMaxPluginPages; ++i) snap->pluginNotes[(size_t) i] = pat.pluginRoll[(size_t) i].notes;
    snap->rustyNotes   = pat.baySickRustyDrumsRoll.notes;
    snap->contentBeats = getPatternContentBeats (patternIndex);
    return snap;
}

void PatternManager::republishRollTable()
{
    auto next = std::make_unique<SchedulerRollSnapshot>();
    next->patterns            = mPatternSnapCache;
    next->currentPatternIndex = mCurrentPattern;
    next->generation          = ++mRollSnapGeneration;
    const std::uint64_t gen = next->generation;
    if (auto* old = mActiveRollSnapshot.exchange (next.release(), std::memory_order_acq_rel))
        mRollRetirement.retire (std::unique_ptr<SchedulerRollSnapshot> (old), gen);
}

void PatternManager::publishRollSnapshotFor (int patternIndex)
{
    // Structural safety net: every pattern-count-changing CRUD also funnels
    // through notifyContentChanged -- a count mismatch rebuilds everything, so
    // no explicit hook is needed in add / duplicate / insert / remove.  A
    // SAME-count structural change (movePattern, restorePatternList) is
    // invisible here and must publish all itself.
    if ((int) mPatternSnapCache.size() != (int) mPatterns.size())
    {
        publishAllRollSnapshots();
        return;
    }
    if (patternIndex < 0 || patternIndex >= (int) mPatterns.size()) return;
    mPatternSnapCache[(size_t) patternIndex] = buildPatternRollsSnapshot (patternIndex);
    republishRollTable();
}

void PatternManager::publishAllRollSnapshots()
{
    mPatternSnapCache.resize (mPatterns.size());
    for (int i = 0; i < (int) mPatterns.size(); ++i)
        mPatternSnapCache[(size_t) i] = buildPatternRollsSnapshot (i);
    republishRollTable();
}

int PatternManager::addPattern(const juce::String& name)
{
    Pattern p;
    p.name = name.isEmpty() ? "Pattern " + juce::String(mPatterns.size()+1) : name;
    // QA-G Task 6 (docket #4): new patterns bind to the current-TS selection.
    p.tsBoundMarkerUid = mCurrentTsMarkerUid;
    {
        const ScopedAudioShield shield (*this);
        mPatterns.push_back(std::move(p));
    }
    refreshPatternTimeSigs();
    notifyContentChanged();
    return (int)mPatterns.size() - 1;
}

int PatternManager::duplicatePattern(int srcIndex)
{
    if (srcIndex < 0 || srcIndex >= (int)mPatterns.size()) return -1;
    Pattern copy = mPatterns[srcIndex];   // deep-copy by value: notes, sequences, rolls
    copy.name = copy.name + " (copy)";
    {
        const ScopedAudioShield shield (*this);
        mPatterns.push_back(std::move(copy));
    }
    notifyContentChanged();
    return (int)mPatterns.size() - 1;
}

// ── Audio file library ──────────────────────────────────────────────────────
// QA-E Task 4 (2026-05-12): pageOwnerChannelId param tags each entry to a
// Vox / Inst / Clips page.  If the path is already in the library, update
// its ownerChannelId to the caller's value -- this lets re-tagging via
// Properties-dropdown route assignment work without first removing the
// entry.  Default 0 = generic Audio (no tag change on existing entries).
void PatternManager::addAudioToLibrary(const juce::String& path,
                                        const juce::String& alias,
                                        int                 pageOwnerChannelId)
{
    // QA-E Task 5 (2026-05-15): dedup on (path, pageOwnerChannelId).  A single
    // file can now exist in the library under multiple page owners (the
    // "Use existing routing / New page" prompt creates a second entry with
    // identical path but different channelId when user picks New).  Prior
    // dedup-on-path-only forced a 1:1 file:page mapping and silently
    // overwrote the page owner on re-add, blocking multi-page routing.
    for (auto& e : mAudioLibrary)
    {
        if (e.path == path && e.pageOwnerChannelId == pageOwnerChannelId)
            return;   // exact duplicate -- no-op

        // Legacy compat: when re-adding with channelId=0 (generic Audio
        // category), upgrade the existing entry's owner to a real channel
        // if one is provided.  This preserves the pre-Task-5 behavior for
        // the recording-finalize path that always passes a real channelId.
        if (e.path == path && e.pageOwnerChannelId == 0 && pageOwnerChannelId != 0)
        {
            e.pageOwnerChannelId = pageOwnerChannelId;
            return;
        }
    }
    // Same display name twice made the browser's delete/rename ambiguous
    // (FILE-03): auto-number the NEW entry's alias when its display name
    // (alias, else filename) collides with any existing entry's.  Single
    // point -- every add path (drops, Add New Clip, recordings) inherits it.
    auto displayOf = [] (const juce::String& p, const juce::String& a)
    {
        // Resolved: a stored stable ref carries its prefix and relative
        // folder, so two entries for the same file would not collide.
        return a.isNotEmpty() ? a : ProjectFileResolver::resolve (p).getFileName();
    };
    juce::String finalAlias = alias;
    {
        const juce::String want = displayOf (path, alias);
        auto taken = [&] (const juce::String& name)
        {
            for (const auto& e : mAudioLibrary)
                if (displayOf (e.path, e.alias) == name) return true;
            return false;
        };
        if (taken (want))
        {
            int n = 2;
            juce::String alt;
            do { alt = want + " (" + juce::String (n++) + ")"; } while (taken (alt));
            finalAlias = alt;
        }
    }
    mAudioLibrary.push_back({ path, finalAlias, 0 /* chokeGroup = none */, pageOwnerChannelId });
}

// QA-E Task 5 (2026-05-15): library lookup helpers.  See header.
int PatternManager::findAudioLibraryIndexByPath (const juce::String& path) const
{
    for (size_t i = 0; i < mAudioLibrary.size(); ++i)
        if (mAudioLibrary[i].path == path) return (int) i;
    return -1;
}

int PatternManager::countAudioLibraryEntriesForChannel (int channelId) const
{
    int n = 0;
    for (const auto& e : mAudioLibrary)
        if (e.pageOwnerChannelId == channelId) ++n;
    return n;
}

void PatternManager::removeAudioFromLibraryAt (int idx)
{
    if (idx < 0 || idx >= (int) mAudioLibrary.size()) return;
    mAudioLibrary.erase (mAudioLibrary.begin() + idx);
}

void PatternManager::setAudioLibraryAlias(int idx, const juce::String& alias)
{
    if (idx < 0 || idx >= (int)mAudioLibrary.size()) return;
    mAudioLibrary[idx].alias = alias;
}

void PatternManager::setAudioLibraryChokeGroup(int idx, int group)
{
    if (idx < 0 || idx >= (int)mAudioLibrary.size()) return;
    mAudioLibrary[idx].chokeGroup = juce::jlimit(0, 16, group);
}

void PatternManager::setAudioLibraryPageOwner(int idx, int channelId)
{
    if (idx < 0 || idx >= (int)mAudioLibrary.size()) return;
    mAudioLibrary[idx].pageOwnerChannelId = channelId;
}

// QA-E Task 7 (FILE-02): set the source-of-truth pitch / BPM / stretch on the
// library entry (browser "Properties..." applies here; followers inherit).
// ── QA-Fe2 browser groups ────────────────────────────────────────────────────
void PatternManager::addManualAudioGroup(int category, const juce::String& name)
{
    if (name.isEmpty()) return;
    for (const auto& [c, n] : mManualAudioGroups)
        if (c == category && n == name) return;   // dedup
    mManualAudioGroups.emplace_back(category, name);
}

void PatternManager::renameManualAudioGroup(int category, const juce::String& oldName,
                                            const juce::String& newName)
{
    if (newName.isEmpty()) return;
    for (auto& [c, n] : mManualAudioGroups)
        if (c == category && n == oldName) n = newName;
    // Member rewrite is category-filtered too: same-named groups can exist
    // in different categories (the create default is "New Group"), and a
    // rename in one must not re-group the other's members.  Category from
    // the owner-channel ranges.  Literals mirror MixerChannelIds caps BY HAND
    // (this core file doesn't see BaySickGraph.h): Clips 400..499 = 0,
    // Vox 600..609 = 1, Inst 700..729 = 2 (QA-Layout T11 caps).
    auto ownerCategory = [] (int ch) noexcept
    {
        if (ch >= 600 && ch < 610) return 1;
        if (ch >= 700 && ch < 730) return 2;
        if (ch >= 400 && ch < 500) return 0;
        return -1;
    };
    for (auto& e : mAudioLibrary)
        if (e.groupName == oldName && ownerCategory(e.pageOwnerChannelId) == category)
            e.groupName = newName;
}

juce::StringArray PatternManager::getManualAudioGroups(int category) const
{
    juce::StringArray out;
    for (const auto& [c, n] : mManualAudioGroups)
        if (c == category) out.add(n);
    return out;
}

int PatternManager::replaceAudioPath(const juce::String& oldPath,
                                     const juce::String& newPath)
{
    int hits = 0;
    for (auto& e : mAudioLibrary)
        if (e.path == oldPath) { e.path = newPath; ++hits; }
    for (auto& blk : mArrangement)
        if (blk.clipType == ClipType::Audio && blk.audioFilePath == oldPath)
            { blk.audioFilePath = newPath; ++hits; }
    return hits;
}

void PatternManager::setAudioLibraryClipDefaults(int idx, float pitch,
                                                 float bpm, bool stretchMode)
{
    if (idx < 0 || idx >= (int)mAudioLibrary.size()) return;
    mAudioLibrary[idx].pitchSemitones = pitch;
    mAudioLibrary[idx].originalBPM    = juce::jmax(1.f, bpm);
    mAudioLibrary[idx].stretchMode    = stretchMode;
}

// ── Automation template library ─────────────────────────────────────────────
void PatternManager::addAutomationTemplate(const AutomationLane& lane)
{
    // Dedupe by paramId - one template per parameter.
    for (const auto& t : mAutomationTemplates)
        if (t.paramId == lane.paramId && lane.paramId.isNotEmpty()) return;
    mAutomationTemplates.push_back(lane);
}

void PatternManager::removeAutomationTemplate(int idx)
{
    if (idx < 0 || idx >= (int)mAutomationTemplates.size()) return;
    mAutomationTemplates.erase(mAutomationTemplates.begin() + idx);
}

void PatternManager::setAutomationTemplateUserName(int idx, const juce::String& userName)
{
    if (idx < 0 || idx >= (int)mAutomationTemplates.size()) return;
    // Display-only rename; paramId stays put so applicator lookups hold.
    mAutomationTemplates[idx].userDisplayName = userName;
}

void PatternManager::removePattern(int index)
{
    if (index < 0 || index >= (int)mPatterns.size() || mPatterns.size() <= 1) return;
    {
        // The re-index cascade is inside the shield too: a half-remapped
        // arrangement read by the scheduler plays the wrong pattern.
        const ScopedAudioShield shield (*this);
        mPatterns.erase(mPatterns.begin() + index);
        // QA-G (found during Split): the erase never re-indexed the rest of the
        // project -- every block referencing a pattern ABOVE the removed index
        // silently played the wrong (shifted) pattern, blocks OF the removed
        // pattern dangled past the end, and linked TS markers kept stale pattern
        // indices.  Re-index everything; blocks + markers of the dead pattern go.
        for (int i = (int) mArrangement.size() - 1; i >= 0; --i)
        {
            auto& b = mArrangement[(size_t) i];
            if (b.clipType != ClipType::Pattern) continue;
            if (b.patternIndex == index)
                mArrangement.erase(mArrangement.begin() + i);
            else if (b.patternIndex > index)
                --b.patternIndex;
        }
        for (int i = (int) mTimeSigChanges.size() - 1; i >= 0; --i)
        {
            auto& ts = mTimeSigChanges[(size_t) i];
            if (ts.linkedPattern == index)
            {
                const int uid = ts.uid;
                mTimeSigChanges.erase(mTimeSigChanges.begin() + i);
                if (uid == mCurrentTsMarkerUid)
                    mCurrentTsMarkerUid = ((int) mTimeSigChanges.size() == 1)
                                            ? mTimeSigChanges[0].uid : -1;
            }
            else if (ts.linkedPattern > index)
                --ts.linkedPattern;
        }
        if (mCurrentPattern > index) --mCurrentPattern;
        mCurrentPattern = juce::jlimit(0, (int)mPatterns.size()-1, mCurrentPattern);
    }
    refreshPatternTimeSigs();
    publishTimeSigMap();
    notifyContentChanged();
    if (onTimeSigStateChanged) onTimeSigStateChanged();
}

int PatternManager::insertPattern(int atIndex, const juce::String& name)
{
    const int oldCount = (int) mPatterns.size();
    atIndex = juce::jlimit(0, oldCount, atIndex);

    Pattern p;
    p.name = name.isEmpty() ? "Pattern " + juce::String(oldCount + 1) : name;
    // New patterns bind to the current-TS selection (follower lifecycle).
    p.tsBoundMarkerUid = mCurrentTsMarkerUid;
    {
        const ScopedAudioShield shield (*this);
        mPatterns.insert(mPatterns.begin() + atIndex, std::move(p));

        // Same re-index cascade removePattern runs, under the insert's mapping:
        // every stored reference at or after the insertion point shifts up one.
        // Without it the arrangement silently plays the wrong pattern.
        for (auto& b : mArrangement)
            if (b.clipType == ClipType::Pattern && b.patternIndex >= atIndex)
                ++b.patternIndex;
        for (auto& ts : mTimeSigChanges)
            if (ts.linkedPattern >= atIndex)
                ++ts.linkedPattern;
        // The selection follows the pattern it named, not the number it held.
        if (mCurrentPattern >= atIndex) ++mCurrentPattern;
    }

    refreshPatternTimeSigs();
    publishTimeSigMap();
    notifyContentChanged();
    if (onTimeSigStateChanged) onTimeSigStateChanged();
    return atIndex;
}

bool PatternManager::movePattern(int fromIndex, int toIndex)
{
    const int count = (int) mPatterns.size();
    if (fromIndex < 0 || fromIndex >= count) return false;
    if (toIndex   < 0 || toIndex   >= count) return false;
    if (fromIndex == toIndex)                return false;

    // A move is a PERMUTATION, not removePattern's one-way shift: fromIndex
    // becomes toIndex, every index between the two slides one step the
    // opposite way, and anything outside [min,max] keeps its number.  An
    // off-by-one here silently re-points a user's arrangement.
    auto remap = [fromIndex, toIndex] (int idx) -> int
    {
        if (idx == fromIndex) return toIndex;
        if (fromIndex < toIndex)
            return (idx > fromIndex && idx <= toIndex) ? idx - 1 : idx;
        return (idx >= toIndex && idx < fromIndex) ? idx + 1 : idx;
    };

    {
        // The permutation reseats every Pattern between the two indices, so a
        // reference the audio thread is holding cannot survive it.
        const ScopedAudioShield shield (*this);
        Pattern moved = std::move(mPatterns[(size_t) fromIndex]);
        mPatterns.erase (mPatterns.begin() + fromIndex);
        mPatterns.insert(mPatterns.begin() + toIndex, std::move(moved));

        for (auto& b : mArrangement)
            if (b.clipType == ClipType::Pattern)
                b.patternIndex = remap(b.patternIndex);
        for (auto& ts : mTimeSigChanges)
            if (ts.linkedPattern >= 0)
                ts.linkedPattern = remap(ts.linkedPattern);
        mCurrentPattern = remap(mCurrentPattern);
    }

    // The pattern COUNT is unchanged, so the roll-table size check inside
    // publishRollSnapshotFor cannot see that every cache slot moved --
    // publish all (restorePatternList precedent).
    publishAllRollSnapshots();
    refreshPatternTimeSigs();
    publishTimeSigMap();
    notifyContentChanged();
    if (onTimeSigStateChanged) onTimeSigStateChanged();
    return true;
}

void PatternManager::restoreTimeMarkers (const std::vector<TimeMarker>& markers)
{
    mTimeMarkers = markers;
    notifyContentChanged();
}

void PatternManager::restoreTimeSigState (const std::vector<TimeSigChange>& changes,
                                          int currentUid)
{
    mTimeSigChanges     = changes;
    mCurrentTsMarkerUid = currentUid;
    tsStateChanged();
}

void PatternManager::restoreTempoChanges (const std::vector<TempoChange>& changes)
{
    mTempoChanges = changes;
    notifyContentChanged();
}

void PatternManager::restoreAudioLibrary (const std::vector<AudioLibraryEntry>& entries,
                                          const std::vector<std::pair<int, juce::String>>& manualGroups)
{
    mAudioLibrary      = entries;
    mManualAudioGroups = manualGroups;
    notifyContentChanged();
}

void PatternManager::restoreAutomationTemplates (const std::vector<AutomationLane>& templates)
{
    mAutomationTemplates = templates;
    notifyContentChanged();
}

void PatternManager::restorePatternList (const std::vector<Pattern>& patterns, int currentIndex)
{
    if (patterns.empty()) return;
    {
        // Whole-vector assignment destroys and reallocates every Pattern.
        const ScopedAudioShield shield (*this);
        mPatterns = patterns;
        mCurrentPattern = juce::jlimit(0, (int)mPatterns.size()-1, currentIndex);
    }
    // #30b: a same-count list swap changes EVERY pattern's content -- the
    // notifyContentChanged size check below can't see that, so publish all.
    publishAllRollSnapshots();
    refreshPatternTimeSigs();
    publishTimeSigMap();
    notifyContentChanged();
    if (onTimeSigStateChanged) onTimeSigStateChanged();
}

void PatternManager::renamePattern(int index, const juce::String& name)
{
    if (index >= 0 && index < (int)mPatterns.size())
    {
        mPatterns[index].name = name;
        notifyContentChanged();
    }
}

Pattern& PatternManager::getPattern(int index)
{
    return mPatterns[juce::jlimit(0, (int)mPatterns.size()-1, index)];
}

const Pattern& PatternManager::getPattern(int index) const
{
    return mPatterns[juce::jlimit(0, (int)mPatterns.size()-1, index)];
}

void PatternManager::setCurrentPattern(int index)
{
    mCurrentPattern = juce::jlimit(0, (int)mPatterns.size()-1, index);
    // #30b: pattern-mode scheduling reads the snapshot's stamped index, so a
    // switch republishes (refreshes the new current entry + restamps).
    publishRollSnapshotFor (mCurrentPattern);
}

void PatternManager::addBlock(ArrangementBlock block)
{
    // C.5b (post-revert): auto-derive removed.  Song-level TS markers are
    // decorative-only (visual reference on the Builder ruler), so deriving a
    // pattern's intrinsic TS from a marker would be confusing.  Pattern TS
    // is set explicitly via right-click → "Set Time Signature".
    {
        const ScopedAudioShield shield (*this);
        mArrangement.push_back(block);
    }
    notifyContentChanged();
}

void PatternManager::removeBlock(int index)
{
    if (index >= 0 && index < (int)mArrangement.size())
    {
        {
            const ScopedAudioShield shield (*this);
            mArrangement.erase(mArrangement.begin() + index);
        }
        notifyContentChanged();
    }
}

ArrangementBlock& PatternManager::getBlock(int index)
{
    return mArrangement[juce::jlimit(0, (int)mArrangement.size()-1, index)];
}

int PatternManager::getTotalArrangementBars() const
{
    int maxBar = 0;
    for (auto& b : mArrangement)
        maxBar = juce::jmax(maxBar, (int) std::ceil((b.startBeats + effectiveLengthBeats(b)) / 4.0 - 1e-9));
    return juce::jmax(16, maxBar);
}

double PatternManager::getSongEndBeats() const
{
    double songEnd = 0.0;
    for (const auto& b : mArrangement)
        songEnd = juce::jmax (songEnd, effectiveStartBeats (b) + effectiveLengthBeats (b));
    return songEnd;
}

// ── Time markers (D-2) ──────────────────────────────────────────────────
void PatternManager::addTimeMarker (int bar, const juce::String& label)
{
    bar = juce::jmax(0, bar);
    mTimeMarkers.push_back({ bar, label });
    std::sort(mTimeMarkers.begin(), mTimeMarkers.end(),
              [](const TimeMarker& a, const TimeMarker& b) { return a.bar < b.bar; });
    notifyContentChanged();
}

void PatternManager::removeTimeMarker (int idx)
{
    if (idx >= 0 && idx < (int) mTimeMarkers.size())
    {
        mTimeMarkers.erase(mTimeMarkers.begin() + idx);
        notifyContentChanged();
    }
}

void PatternManager::renameTimeMarker (int idx, const juce::String& label)
{
    if (idx >= 0 && idx < (int) mTimeMarkers.size())
    {
        mTimeMarkers[(size_t) idx].label = label;
        notifyContentChanged();
    }
}

int PatternManager::findTimeMarkerNearBar (float bar, float tolerance) const
{
    int  bestIdx = -1;
    float bestDist = tolerance + 1.f;
    for (int i = 0; i < (int) mTimeMarkers.size(); ++i)
    {
        const float d = std::abs((float) mTimeMarkers[(size_t) i].bar - bar);
        if (d <= tolerance && d < bestDist) { bestIdx = i; bestDist = d; }
    }
    return bestIdx;
}

// ── Time-signature changes (D-2; QA-G Task 6: the SOLE played source) ───
static int roundTsDenominator (int den)
{
    static const int kAllowedDen[] = { 1, 2, 4, 8, 16, 32 };
    int closest = 4;
    int bestDiff = 999;
    for (int d : kAllowedDen)
        if (std::abs(d - den) < bestDiff) { closest = d; bestDiff = std::abs(d - den); }
    return closest;
}

void PatternManager::tsStateChanged()
{
    refreshPatternTimeSigs();
    publishTimeSigMap();
    notifyContentChanged();
    if (onTimeSigStateChanged) onTimeSigStateChanged();
}

void PatternManager::addTimeSigChange (int bar, int num, int den, int linkedPattern)
{
    bar = juce::jmax(0, bar);
    num = juce::jlimit(1, 32, num);
    den = roundTsDenominator(den);

    // One TS per bar.  A manual marker owns its bar: a linked spawn onto an
    // occupied bar is a no-op (docket B "same-bar manual wins"); a MANUAL
    // write takes the bar over (and unlinks whatever held it).
    for (auto& ts : mTimeSigChanges)
        if (ts.bar == bar)
        {
            if (linkedPattern >= 0) return;
            ts.num = num; ts.den = den; ts.linkedPattern = -1;
            tsStateChanged();
            return;
        }

    TimeSigChange ts;
    ts.bar = bar; ts.num = num; ts.den = den;
    ts.uid = mNextTsUid++;
    ts.linkedPattern = linkedPattern;
    mTimeSigChanges.push_back(ts);
    std::sort(mTimeSigChanges.begin(), mTimeSigChanges.end(),
              [](const TimeSigChange& a, const TimeSigChange& b) { return a.bar < b.bar; });
    // Current-selection maintenance: a sole marker is automatically current.
    if ((int) mTimeSigChanges.size() == 1)
        mCurrentTsMarkerUid = ts.uid;
    tsStateChanged();
}

void PatternManager::removeTimeSigChange (int idx)
{
    if (idx < 0 || idx >= (int) mTimeSigChanges.size()) return;
    const int removedUid = mTimeSigChanges[(size_t) idx].uid;
    mTimeSigChanges.erase(mTimeSigChanges.begin() + idx);
    if (removedUid == mCurrentTsMarkerUid)
    {
        // Docket 4B: sole survivor auto-becomes current; 2+ left -> unset
        // (the Builder re-prompts); none -> unset.
        mCurrentTsMarkerUid = ((int) mTimeSigChanges.size() == 1)
                                ? mTimeSigChanges[0].uid : -1;
    }
    tsStateChanged();
}

int PatternManager::findTimeSigChangeAtBar (int bar) const
{
    for (int i = 0; i < (int) mTimeSigChanges.size(); ++i)
        if (mTimeSigChanges[(size_t) i].bar == bar) return i;
    return -1;
}

int PatternManager::findTimeSigChangeByUid (int uid) const
{
    if (uid < 0) return -1;
    for (int i = 0; i < (int) mTimeSigChanges.size(); ++i)
        if (mTimeSigChanges[(size_t) i].uid == uid) return i;
    return -1;
}

void PatternManager::setCurrentTsMarkerUid (int uid)
{
    if (mCurrentTsMarkerUid == uid) return;
    mCurrentTsMarkerUid = uid;
    tsStateChanged();
}

// ── Tempo changes (QA-TempoMap, 2026-07-08) ─────────────────────────────
void PatternManager::addTempoChange (int bar, double bpm)
{
    bar = juce::jmax (0, bar);
    bpm = juce::jlimit (20.0, 300.0, bpm);   // the tempo range every other writer clamps to
    for (auto& tc : mTempoChanges)
        if (tc.bar == bar) { tc.bpm = bpm; notifyContentChanged(); return; }   // one change per bar
    mTempoChanges.push_back ({ bar, bpm });
    std::sort (mTempoChanges.begin(), mTempoChanges.end(),
               [] (const TempoChange& a, const TempoChange& b) { return a.bar < b.bar; });
    notifyContentChanged();
}

void PatternManager::removeTempoChange (int idx)
{
    if (idx >= 0 && idx < (int) mTempoChanges.size())
    {
        mTempoChanges.erase (mTempoChanges.begin() + idx);
        notifyContentChanged();
    }
}

int PatternManager::findTempoChangeNearBar (float bar, float tolerance) const
{
    int   bestIdx  = -1;
    float bestDist = tolerance + 1.f;
    for (int i = 0; i < (int) mTempoChanges.size(); ++i)
    {
        const float d = std::abs ((float) mTempoChanges[(size_t) i].bar - bar);
        if (d <= tolerance && d < bestDist) { bestIdx = i; bestDist = d; }
    }
    return bestIdx;
}

// ── C.5: time-signature-aware beat/bar conversion ────────────────────────────
TimeSigChange PatternManager::getEffectiveTimeSigAtBar (int bar) const
{
    TimeSigChange eff { 0, 4, 4 };   // implicit 4/4 default
    for (const auto& ts : mTimeSigChanges)
    {
        if (ts.bar <= bar) eff = ts;
        else break;   // sorted ascending; first ts.bar > bar means we're done
    }
    return eff;
}

void PatternManager::beatToBarAndBeatInBar (double beat, int& outBar, double& outBeatInBar) const
{
    if (beat <= 0.0)
    {
        outBar = 0;
        outBeatInBar = juce::jmax (0.0, beat);
        return;
    }

    // Walk through TS changes accumulating bar-by-bar beat counts.
    int    barCounter  = 0;
    double beatCounter = 0.0;
    int    currentNum  = 4;
    int    currentDen  = 4;

    auto bpbFor = [](int num, int den) -> double {
        return (double) num * 4.0 / (double) (den > 0 ? den : 4);
    };

    const size_t n = mTimeSigChanges.size();
    for (size_t i = 0; i <= n; ++i)
    {
        const int nextSwitchBar = (i < n) ? mTimeSigChanges[i].bar
                                          : std::numeric_limits<int>::max();
        const double bpb = bpbFor (currentNum, currentDen);

        // Walk bars while still inside this TS run AND not past target.
        while (barCounter < nextSwitchBar)
        {
            const double barEnd = beatCounter + bpb;
            if (beat < barEnd)
            {
                outBar = barCounter;
                outBeatInBar = beat - beatCounter;
                return;
            }
            beatCounter = barEnd;
            ++barCounter;
        }

        if (i < n)
        {
            currentNum = mTimeSigChanges[i].num;
            currentDen = mTimeSigChanges[i].den;
        }
    }

    // Should be unreachable (loop above always returns), but be safe.
    outBar = barCounter;
    outBeatInBar = 0.0;
}

// C.5b: per-pattern intrinsic TS.
double PatternManager::getPatternBeatsPerBar (int patternIndex) const
{
    if (patternIndex < 0 || patternIndex >= (int) mPatterns.size()) return 4.0;
    const auto& pat = mPatterns[(size_t) patternIndex];
    const int den = (pat.tsDen > 0) ? pat.tsDen : 4;
    return (double) juce::jmax (1, pat.tsNum) * 4.0 / (double) den;
}

// QA-G Task 6: follower lifecycle.  autoDerivePatternTimeSig (orphaned C.5b
// machinery, zero callers) is replaced by this live re-derivation.
void PatternManager::refreshPatternTimeSigs()
{
    for (int pi = 0; pi < (int) mPatterns.size(); ++pi)
    {
        auto& pat = mPatterns[(size_t) pi];
        if (pat.tsLocked) continue;   // user-set: fields are authoritative

        // Placed follower: the marker in effect at the EARLIEST block start.
        double earliestBeat = std::numeric_limits<double>::max();
        for (const auto& b : mArrangement)
            if (b.clipType == ClipType::Pattern && b.patternIndex == pi)
                earliestBeat = juce::jmin (earliestBeat, effectiveStartBeats (b));
        if (earliestBeat < std::numeric_limits<double>::max())
        {
            int bar = 0; double bib = 0.0;
            beatToBarAndBeatInBar (juce::jmax (0.0, earliestBeat), bar, bib);
            const auto eff = getEffectiveTimeSigAtBar (bar);
            pat.tsNum = eff.num;
            pat.tsDen = eff.den;
            continue;
        }

        // Unplaced follower: binding -> current -> 4/4.  Dead bindings
        // re-bind to the current selection (owner wiring, 2026-07-17).
        if (pat.tsBoundMarkerUid >= 0
            && findTimeSigChangeByUid (pat.tsBoundMarkerUid) < 0)
            pat.tsBoundMarkerUid = mCurrentTsMarkerUid;
        int mi = findTimeSigChangeByUid (pat.tsBoundMarkerUid);
        if (mi < 0) mi = findTimeSigChangeByUid (mCurrentTsMarkerUid);
        if (mi >= 0)
        {
            pat.tsNum = mTimeSigChanges[(size_t) mi].num;
            pat.tsDen = mTimeSigChanges[(size_t) mi].den;
        }
        else
        {
            pat.tsNum = 4;
            pat.tsDen = 4;
        }
    }
}

void PatternManager::resetPatternTimeSig (int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= (int) mPatterns.size()) return;
    auto& pat = mPatterns[(size_t) patternIndex];
    pat.tsLocked = false;
    pat.tsBoundMarkerUid = mCurrentTsMarkerUid;
    // "Acts like it never was changed": its still-linked auto-markers go
    // too (manual + unlinked markers stay -- they are the user's).
    for (int i = (int) mTimeSigChanges.size() - 1; i >= 0; --i)
        if (mTimeSigChanges[(size_t) i].linkedPattern == patternIndex)
        {
            const int uid = mTimeSigChanges[(size_t) i].uid;
            mTimeSigChanges.erase (mTimeSigChanges.begin() + i);
            if (uid == mCurrentTsMarkerUid)
                mCurrentTsMarkerUid = ((int) mTimeSigChanges.size() == 1)
                                        ? mTimeSigChanges[0].uid : -1;
        }
    tsStateChanged();
}

void PatternManager::cleanupLinkedMarkers()
{
    // Two-phase: resolve orphans against the CURRENT map first (erasing
    // mid-loop would shift bar indexing for later conversions), then erase.
    std::vector<int> orphanUids;
    for (const auto& ts : mTimeSigChanges)
    {
        if (ts.linkedPattern < 0) continue;
        bool hasBlock = false;
        if (ts.linkedPattern < (int) mPatterns.size())
            for (const auto& b : mArrangement)
            {
                if (b.clipType != ClipType::Pattern
                    || b.patternIndex != ts.linkedPattern) continue;
                int bar = 0; double bib = 0.0;
                beatToBarAndBeatInBar (juce::jmax (0.0, effectiveStartBeats (b)), bar, bib);
                if (bar == ts.bar) { hasBlock = true; break; }
            }
        if (! hasBlock) orphanUids.push_back (ts.uid);
    }
    for (const int uid : orphanUids)
    {
        const int idx = findTimeSigChangeByUid (uid);
        if (idx < 0) continue;
        mTimeSigChanges.erase (mTimeSigChanges.begin() + idx);
        if (uid == mCurrentTsMarkerUid)
            mCurrentTsMarkerUid = ((int) mTimeSigChanges.size() == 1)
                                    ? mTimeSigChanges[0].uid : -1;
    }
    // Always re-derive followers + refresh the UI -- block edits change
    // placed-follower governance even when no marker was removed.  Only a
    // real removal republishes / dirties.
    refreshPatternTimeSigs();
    if (! orphanUids.empty())
    {
        publishTimeSigMap();
        notifyContentChanged();
    }
    if (onTimeSigStateChanged) onTimeSigStateChanged();
}

bool PatternManager::patternHasNotes (int patternIndex) const
{
    if (patternIndex < 0 || patternIndex >= (int) mPatterns.size()) return false;
    const auto& p = mPatterns[(size_t) patternIndex];
    for (const auto& r : p.layerRoll) if (! r.notes.empty()) return true;
    for (const auto& r : p.bassRoll)  if (! r.notes.empty()) return true;
    for (const auto& r : p.drumRolls) if (! r.notes.empty()) return true;
    for (const auto& r : p.clipRoll)  if (! r.notes.empty()) return true;
    for (const auto& r : p.voxRoll)   if (! r.notes.empty()) return true;
    for (const auto& r : p.instRoll)  if (! r.notes.empty()) return true;
    for (const auto& r : p.pluginRoll) if (! r.notes.empty()) return true;
    if (! p.baySickRustyDrumsRoll.notes.empty()) return true;
    if (! p.drumRoll.notes.empty()) return true;
    return false;
}

void PatternManager::publishTimeSigMap() const
{
    double beats[TsMap::kMaxSegs]; double bpbs[TsMap::kMaxSegs];
    int    nums [TsMap::kMaxSegs]; int    dens[TsMap::kMaxSegs];
    int    bars [TsMap::kMaxSegs];
    // Segment 0 = the opening signature (bar-0 marker or implicit 4/4).
    int i0 = 0, curNum = 4, curDen = 4;
    if (! mTimeSigChanges.empty() && mTimeSigChanges[0].bar <= 0)
    {
        curNum = mTimeSigChanges[0].num;
        curDen = mTimeSigChanges[0].den;
        i0 = 1;
    }
    beats[0] = 0.0;
    bpbs [0] = (double) curNum * 4.0 / (double) juce::jmax (1, curDen);
    nums [0] = curNum; dens[0] = curDen; bars[0] = 0;
    int n = 1;
    for (int i = i0; i < (int) mTimeSigChanges.size() && n < TsMap::kMaxSegs; ++i)
    {
        const auto& ts = mTimeSigChanges[(size_t) i];
        if (ts.bar <= bars[n - 1]) continue;   // sorted; defensive vs dupes
        beats[n] = beats[n - 1] + (double) (ts.bar - bars[n - 1]) * bpbs[n - 1];
        bpbs [n] = (double) ts.num * 4.0 / (double) juce::jmax (1, ts.den);
        nums [n] = ts.num; dens[n] = ts.den; bars[n] = ts.bar;
        ++n;
    }
    TsMap::publish (beats, bpbs, nums, dens, bars, n);
}

void PatternManager::setPatternTimeSig (int patternIndex, int num, int den)
{
    if (patternIndex < 0 || patternIndex >= (int) mPatterns.size()) return;
    auto& pat = mPatterns[(size_t) patternIndex];
    pat.tsNum    = juce::jlimit (1, 32, num);
    pat.tsDen    = roundTsDenominator (den);
    pat.tsLocked = true;
    // Docket B: pattern-TS edits update all still-linked markers.  Spawns
    // nothing -- marker creation is placement-event-driven in the grid.
    for (auto& ts : mTimeSigChanges)
        if (ts.linkedPattern == patternIndex) { ts.num = pat.tsNum; ts.den = pat.tsDen; }
    tsStateChanged();
}

double PatternManager::barStartBeat (int bar) const
{
    if (bar <= 0) return 0.0;

    int    barCounter  = 0;
    double beatCounter = 0.0;
    int    currentNum  = 4;
    int    currentDen  = 4;

    auto bpbFor = [](int num, int den) -> double {
        return (double) num * 4.0 / (double) (den > 0 ? den : 4);
    };

    const size_t n = mTimeSigChanges.size();
    for (size_t i = 0; i <= n; ++i)
    {
        const int nextSwitchBar = (i < n) ? mTimeSigChanges[i].bar
                                          : std::numeric_limits<int>::max();
        const double bpb = bpbFor (currentNum, currentDen);

        const int runEnd = juce::jmin (bar, nextSwitchBar);
        if (runEnd > barCounter)
        {
            beatCounter += (runEnd - barCounter) * bpb;
            barCounter = runEnd;
        }
        if (barCounter >= bar) return beatCounter;

        if (i < n)
        {
            currentNum = mTimeSigChanges[i].num;
            currentDen = mTimeSigChanges[i].den;
        }
    }
    return beatCounter;
}

// B-1: per-patternIndex content length in beats (furthest note/step end across
// that pattern's rolls, ceiled to a bar at the pattern's bpb, min 1 bar).  The
// Builder tiling + the song-mode scheduler feed off THIS so an 8-bar pattern
// loops every 8, a 2-bar every 2 - independent of the stored Pattern.bars field
// (which is no longer the tile length).  getEffectivePatternLoopBeats() is the
// current-pattern shorthand.
double PatternManager::getPatternContentBeats (int patternIndex) const
{
    // C.5b (post-revert): pattern owns its TS.  Bar length in PPQ = pattern's
    // own bpb (4/4 = 4, 3/4 = 3, 6/8 = 3, 7/8 = 3.5).  Builder grid is
    // uniform 4-beat-per-bar separately (song-level TS markers are decorative
    // only) - but pattern playback length is in pattern-bars, each pat-bpb wide.
    if (mPatterns.empty() || patternIndex < 0 || patternIndex >= (int) mPatterns.size())
        return 4.0;   // safe fallback when no patterns loaded
    const auto&  pat          = mPatterns[(size_t) patternIndex];
    const double patBpb       = juce::jmax (1.0, getPatternBeatsPerBar (patternIndex));
    // Minimum is 1 pattern-bar so a bar-1-only piano roll loops immediately
    // rather than waiting for the full configured bar count.
    const double kMinBeats    = patBpb;

    // Helper: ceil a beat position up to the next bar-start beat using the
    // pattern's intrinsic bpb (uniform within the pattern).
    auto ceilToBarStart = [patBpb](double endBeat) -> double
    {
        if (endBeat <= 0.0) return 0.0;
        // QA-Ee (96 PPQ): snap to the tick grid first so a note/step end that
        // float-drifted a hair PAST a bar boundary (invisible on screen) does
        // not ceil up an extra bar -- the bug behind the "note short of the line
        // but the pattern loops a bar late" report.  beatsToTicks rounds to the
        // nearest 1/96 beat; an exact-tick end then ceils deterministically.
        // Stage 3 makes note ends native ticks, at which point this snap is a
        // no-op (already grid-aligned).
        endBeat = ticksToBeats (beatsToTicks (endBeat));
        const double bars = std::ceil (endBeat / patBpb - 1e-9);
        return bars * patBpb;
    };

    // C.5b: default loop = 1 pattern-bar (kMinBeats).  The note-end priority
    // below extends when content exists.  Block-driven priority dropped
    // because Builder bars are uniform 4-beat while patterns play at their
    // intrinsic TS - mixing units broke 7/4 → 3/4 transitions.
    double loopBeats = kMinBeats;

    // ── Priority 2: furthest note end in any roll, ceiled to bar boundary ────
    {
        double latestEnd = 0.0;
        auto scanRoll = [&](const PianoRollData& roll) {
            for (auto& note : roll.notes)
                latestEnd = juce::jmax(latestEnd, note.startBeat + note.durationBeats);
        };
        for (auto& roll : pat.layerRoll) scanRoll(roll);
        for (auto& roll : pat.bassRoll)  scanRoll(roll);
        scanRoll(pat.drumRoll);
        for (auto& roll : pat.drumRolls) scanRoll(roll);
        // G-3 (2026-04-28): Clips rolls also extend the loop length.  Missing
        // this on the initial G-3 ship caused long Clip notes to retrigger
        // every bar (pattern wrap at the 1-bar minimum) instead of the wrap
        // point sitting past the note's end like every other roll type.
        for (auto& roll : pat.clipRoll)  scanRoll(roll);
        // G-4 (2026-04-28): Vox + Inst rolls likewise.
        for (auto& roll : pat.voxRoll)   scanRoll(roll);
        for (auto& roll : pat.instRoll)  scanRoll(roll);
        for (auto& roll : pat.pluginRoll) scanRoll(roll);
        // J-7b (2026-05-04): BaySickRustyDrums singleton roll likewise - without
        // this scan, multi-bar drum patterns wrap at the 1-bar minimum instead
        // of the longest note's end.
        scanRoll(pat.baySickRustyDrumsRoll);

        if (latestEnd > 0.0)
            loopBeats = juce::jmax (loopBeats, ceilToBarStart (latestEnd));
    }

    return loopBeats;
}

double PatternManager::getEffectivePatternLoopBeats() const
{
    return getPatternContentBeats (mCurrentPattern);
}

// ── Serialisation ─────────────────────────────────────────────────────────────
// Project-persistence Phase P1 (2026-04-23): full round-trip for every field on
// Pattern / PatternManager.  Schema is versioned via the "version" property on
// the root PatternManager node - bumps happen when we break backward compat.
// Missing-attr reads always fall back to struct-default values so older files
// load forward without data corruption.
namespace
{
    // Compact per-note serializer: attribute names are single-letter to keep
    // XML small for patterns with thousands of notes.
    juce::ValueTree noteToValueTree (const PianoNote& n)
    {
        juce::ValueTree t ("Note");
        t.setProperty ("m",  n.midiNote,                     nullptr);
        // QA-Ee Stage 3b: tick-authoritative timebase (96 PPQ).  Write ticks only,
        // computed from the tick-aligned editing beats; the legacy `s`/`d` float-beat
        // props are dropped -- new format is tick-only (downgrade unsupported), matching
        // the block serdes.
        t.setProperty ("st", beatsToTicks (n.startBeat),     nullptr);
        t.setProperty ("dt", beatsToTicks (n.durationBeats), nullptr);
        t.setProperty ("v",  n.velocity,      nullptr);
        if (n.panning      != 0.0f)                  t.setProperty ("p",  n.panning,      nullptr);
        if (n.finePitch    != 0.0f)                  t.setProperty ("f",  n.finePitch,    nullptr);
        if (n.type         != NoteType::Standard)    t.setProperty ("t",  (int) n.type,   nullptr);
        if (n.muted)                                 t.setProperty ("u",  true,           nullptr);
        if (n.groupId      != -1)                    t.setProperty ("g",  n.groupId,      nullptr);
        if (n.filterCutoff != 0.5f)                  t.setProperty ("c",  n.filterCutoff, nullptr);
        if (n.releaseAmt   != 0.5f)                  t.setProperty ("r",  n.releaseAmt,   nullptr);
        if (n.resonance    != 0.5f)                  t.setProperty ("q",  n.resonance,    nullptr);
        if (n.slotIndex    != -1)                    t.setProperty ("sl", n.slotIndex,    nullptr);
        if (n.portaLengthBeats != 1.0)               t.setProperty ("pl", n.portaLengthBeats, nullptr);
        if (n.bendSemitones != 0.0)                  t.setProperty ("bs", n.bendSemitones, nullptr);
        if (n.bendShape    != BendShape::RampHold)   t.setProperty ("bsh", (int) n.bendShape, nullptr);
        return t;
    }

    PianoNote noteFromValueTree (const juce::ValueTree& t)
    {
        PianoNote n;
        n.midiNote      = (int)           t.getProperty ("m",  60);
        // QA-Ee Stage 3b: prefer tick props (`st`/`dt`); else migrate legacy float
        // beats (`s`/`d`) to the nearest tick.  Both representations land consistent.
        if (t.hasProperty ("st")) { n.startTicks    = (juce::int64) t.getProperty ("st", (juce::int64) 0);  n.startBeat     = ticksToBeats (n.startTicks); }
        else                      { n.startBeat     = (double)      t.getProperty ("s",  0.0);              n.startTicks    = beatsToTicks (n.startBeat); }
        if (t.hasProperty ("dt")) { n.durationTicks = (juce::int64) t.getProperty ("dt", (juce::int64) 24); n.durationBeats = ticksToBeats (n.durationTicks); }
        else                      { n.durationBeats = (double)      t.getProperty ("d",  0.25);             n.durationTicks = beatsToTicks (n.durationBeats); }
        n.velocity      = (float)(double) t.getProperty ("v",  0.8);
        n.panning       = (float)(double) t.getProperty ("p",  0.0);
        n.finePitch     = (float)(double) t.getProperty ("f",  0.0);
        n.type          = (NoteType)(int) t.getProperty ("t",  (int) NoteType::Standard);
        n.muted         = (bool)          t.getProperty ("u",  false);
        n.groupId       = (int)           t.getProperty ("g",  -1);
        n.filterCutoff  = (float)(double) t.getProperty ("c",  0.5);
        n.releaseAmt    = (float)(double) t.getProperty ("r",  0.5);
        n.resonance     = (float)(double) t.getProperty ("q",  0.5);
        n.slotIndex     = (int)           t.getProperty ("sl", -1);
        n.portaLengthBeats = (double)     t.getProperty ("pl", 1.0);
        n.bendSemitones = (double)        t.getProperty ("bs", 0.0);
        n.bendShape     = (BendShape)(int) t.getProperty ("bsh", (int) BendShape::RampHold);
        return n;
    }

    juce::ValueTree rollToValueTree (const juce::String& tag, const PianoRollData& r)
    {
        juce::ValueTree t (tag);
        t.setProperty ("numBars",         r.numBars,         nullptr);
        if (r.riffMachineUsed)
            t.setProperty ("riffUsed", true, nullptr);   // #20 (QA-G3Smoke)
        for (const auto& n : r.notes) t.addChild (noteToValueTree (n), -1, nullptr);
        return t;
    }

    void rollFromValueTree (const juce::ValueTree& t, PianoRollData& r)
    {
        if (! t.isValid()) return;
        r.notes.clear();
        r.numBars         = (int) t.getProperty ("numBars",         2);
        r.riffMachineUsed = (bool) t.getProperty ("riffUsed", false);   // #20
        for (int i = 0; i < t.getNumChildren(); ++i)
        {
            auto c = t.getChild (i);
            if (c.hasType ("Note"))
                r.notes.push_back (noteFromValueTree (c));
        }
    }

    juce::ValueTree automationLaneToValueTree (const AutomationLane& lane)
    {
        juce::ValueTree t ("AutomationLane");
        t.setProperty ("paramId",         lane.paramId,         nullptr);
        t.setProperty ("userDisplayName", lane.userDisplayName, nullptr);
        t.setProperty ("lastKnownName",   lane.lastKnownName,   nullptr);
        t.setProperty ("isLFO",           lane.isLFO,           nullptr);
        t.setProperty ("lfoShape",        lane.lfoShape,        nullptr);
        t.setProperty ("lfoRate",         lane.lfoRate,         nullptr);
        t.setProperty ("lfoMin",          lane.lfoMin,          nullptr);
        t.setProperty ("lfoMax",          lane.lfoMax,          nullptr);
        for (const auto& cp : lane.points)
        {
            juce::ValueTree p ("Point");
            p.setProperty ("time",    cp.timeTicks, nullptr);
            p.setProperty ("value",   cp.value01,   nullptr);
            p.setProperty ("curve",   (int) cp.curveType, nullptr);
            p.setProperty ("tension", cp.tension,   nullptr);
            t.addChild (p, -1, nullptr);
        }
        return t;
    }

    AutomationLane automationLaneFromValueTree (const juce::ValueTree& t)
    {
        AutomationLane lane;
        if (! t.isValid()) return lane;
        lane.paramId         =                   t.getProperty ("paramId",         juce::String()).toString();
        lane.userDisplayName =                   t.getProperty ("userDisplayName", juce::String()).toString();
        lane.lastKnownName   =                   t.getProperty ("lastKnownName",   juce::String()).toString();
        lane.isLFO           = (bool)            t.getProperty ("isLFO",    false);
        lane.lfoShape        = (int)             t.getProperty ("lfoShape", 0);
        lane.lfoRate         = (float)(double)   t.getProperty ("lfoRate",  1.0);
        lane.lfoMin          = (float)(double)   t.getProperty ("lfoMin",   0.0);
        lane.lfoMax          = (float)(double)   t.getProperty ("lfoMax",   1.0);
        for (int i = 0; i < t.getNumChildren(); ++i)
        {
            auto c = t.getChild (i);
            if (! c.hasType ("Point")) continue;
            ControlPoint cp;
            cp.timeTicks = (float)(double) c.getProperty ("time",    0.0);
            cp.value01   = (float)(double) c.getProperty ("value",   0.5);
            cp.curveType = (CurveType)(int) c.getProperty ("curve",  (int) CurveType::Linear);
            cp.tension   = (float)(double) c.getProperty ("tension", 0.0);
            lane.points.push_back (cp);
        }
        // evalAutomationLaneAt assumes sorted points and cannot sort (it runs on
        // the audio thread), so the load boundary establishes the invariant.
        std::sort (lane.points.begin(), lane.points.end(),
                   [](const ControlPoint& a, const ControlPoint& b)
                       { return a.timeTicks < b.timeTicks; });
        return lane;
    }
}

void PatternManager::reset()
{
    const ScopedAudioShield shield (*this);
    mPatterns.clear();
    mPatterns.emplace_back();   // one empty default pattern
    mCurrentPattern = 0;
    mGlobalTempo    = 120.0;
    mArrangement.clear();
    // QA-G Task 6: File > New previously LEAKED ruler markers into the fresh
    // project (reset never cleared them) -- clear all three lists + TS state.
    mTimeMarkers.clear();
    mTimeSigChanges.clear();
    mTempoChanges.clear();
    mCurrentTsMarkerUid = -1;
    mNextTsUid          = 1;
    publishTimeSigMap();
    // QA-RustyMeter Task 4 (2026-05-30): clear the audio library on blank-reset.
    // reset() is the "wipe to empty project" path (resetToBlankState -> here, hit
    // by all File > New / New-from-template entry points); it cleared patterns /
    // arrangement / mixer / automation but NOT mAudioLibrary, so a previously-used
    // sample dropped onto a fresh File > New project still matched
    // findAudioLibraryIndexByPath and fired a false "File Already in Library"
    // prompt.  fromValueTree (the project-load path) already clears it; reset()
    // was simply missing the same clear (QA-D STATE-* family).  In-project dedup
    // is unaffected -- entries still accumulate via addAudioLibraryEntry within a
    // project, so a true duplicate drop still prompts correctly.
    mAudioLibrary.clear();
    mManualAudioGroups.clear();   // QA-Fe2
    mMixer = {};
    for (auto& m : mRowMuted)  m.store (false, std::memory_order_relaxed);
    for (auto& s : mRowSoloed) s.store (false, std::memory_order_relaxed);
    for (int i = 0; i < kMaxArrangementRows; ++i)
        mRowNames[i] = defaultRowName(i);
    mRowGroupId.fill(0);
    mRowGroupColor.fill(0);
    mAutomationTemplates.clear();
    // #30b: reset never notifies (wipe path); publish the emptied rolls so the
    // audio thread stops scheduling stale content immediately.
    publishAllRollSnapshots();
}

juce::ValueTree PatternManager::toValueTree() const
{
    juce::ValueTree root("PatternManager");
    root.setProperty("version",        1, nullptr);   // P1 schema version
    root.setProperty("currentPattern", mCurrentPattern, nullptr);
    root.setProperty("globalTempo",    mGlobalTempo,    nullptr);   // 2026-04-24

    // ── Mixer (full dump) ────────────────────────────────────────────────────
    juce::ValueTree mixNode("Mixer");
    mixNode.setProperty("masterLevel",        mMixer.masterLevel,        nullptr);
    mixNode.setProperty("layersLevel",        mMixer.layersLevel,        nullptr);
    mixNode.setProperty("bassLevel",          mMixer.bassLevel,          nullptr);
    mixNode.setProperty("drumsLevel",         mMixer.drumsLevel,         nullptr);
    mixNode.setProperty("layersMute",         mMixer.layersMute,         nullptr);
    mixNode.setProperty("bassMute",           mMixer.bassMute,           nullptr);
    mixNode.setProperty("drumsMute",          mMixer.drumsMute,          nullptr);
    mixNode.setProperty("layersSolo",         mMixer.layersSolo,         nullptr);
    mixNode.setProperty("bassSolo",           mMixer.bassSolo,           nullptr);
    mixNode.setProperty("drumsSolo",          mMixer.drumsSolo,          nullptr);
    mixNode.setProperty("masterPan",          mMixer.masterPan,          nullptr);
    mixNode.setProperty("layersPan",          mMixer.layersPan,          nullptr);
    mixNode.setProperty("bassPan",            mMixer.bassPan,            nullptr);
    mixNode.setProperty("drumsPan",           mMixer.drumsPan,           nullptr);
    mixNode.setProperty("audioClipsBusLevel", mMixer.audioClipsBusLevel, nullptr);
    mixNode.setProperty("audioClipsBusPan",   mMixer.audioClipsBusPan,   nullptr);
    mixNode.setProperty("audioClipsBusMute",  mMixer.audioClipsBusMute,  nullptr);
    mixNode.setProperty("audioClipsBusSolo",  mMixer.audioClipsBusSolo,  nullptr);
    // Per-drum-row + per-audio-row arrays - pack as CSV strings for compactness
    {
        juce::StringArray slot, span, aLv, aMu;
        for (int i = 0; i < kMaxDrumPages;          ++i) { slot.add(juce::String(mMixer.drumSlotLevel[i]));
                                                           span.add(juce::String(mMixer.drumSlotPan[i])); }
        for (int i = 0; i < MixerState::kMaxAudioRows; ++i) { aLv.add(juce::String(mMixer.audioRowLevel[i]));
                                                               aMu.add(mMixer.audioRowMute[i] ? "1" : "0"); }
        mixNode.setProperty("drumSlotLevel", slot.joinIntoString(","), nullptr);
        mixNode.setProperty("drumSlotPan",   span.joinIntoString(","), nullptr);
        mixNode.setProperty("audioRowLevel", aLv .joinIntoString(","), nullptr);
        mixNode.setProperty("audioRowMute",  aMu .joinIntoString(","), nullptr);
    }
    root.addChild(mixNode, -1, nullptr);

    // ── Per-track row mute/solo ──────────────────────────────────────────────
    {
        juce::ValueTree n("RowState");
        juce::StringArray mute, solo;
        for (int i = 0; i < kMaxArrangementRows; ++i)
        {
            mute.add(mRowMuted [i].load(std::memory_order_relaxed) ? "1" : "0");
            solo.add(mRowSoloed[i].load(std::memory_order_relaxed) ? "1" : "0");
        }
        n.setProperty("mute", mute.joinIntoString(""), nullptr);
        n.setProperty("solo", solo.joinIntoString(""), nullptr);
        // QA-G: sparse <Row> children carry non-default names + group state
        // (child elements, not CSV, so names may contain any character).
        for (int i = 0; i < kMaxArrangementRows; ++i)
        {
            const bool named   = (mRowNames[i] != defaultRowName(i));
            const bool grouped = (mRowGroupId[i] != 0);
            if (!named && !grouped) continue;
            juce::ValueTree r("Row");
            r.setProperty("i", i, nullptr);
            if (named)   r.setProperty("name", mRowNames[i], nullptr);
            if (grouped)
            {
                r.setProperty("group",      mRowGroupId[i],           nullptr);
                r.setProperty("groupColor", (int) mRowGroupColor[i],  nullptr);
            }
            n.addChild(r, -1, nullptr);
        }
        root.addChild(n, -1, nullptr);
    }

    // ── Patterns ─────────────────────────────────────────────────────────────
    juce::ValueTree patternsNode("Patterns");
    for (auto& p : mPatterns)
    {
        juce::ValueTree pNode("Pattern");
        pNode.setProperty("name",        p.name,        nullptr);
        pNode.setProperty("bars",        p.bars,        nullptr);
        pNode.setProperty("stepsPerBar", p.stepsPerBar, nullptr);
        pNode.setProperty("color",       (int) p.color.getARGB(), nullptr);   // F-1
        // C.5b: per-pattern intrinsic TS
        pNode.setProperty("tsNum",       p.tsNum,       nullptr);
        pNode.setProperty("tsDen",       p.tsDen,       nullptr);
        pNode.setProperty("tsLocked",    p.tsLocked,    nullptr);
        pNode.setProperty("tsBoundUid",  p.tsBoundMarkerUid, nullptr);   // QA-G Task 6

        // Piano-roll notes - layerRoll[0..kMaxLayerPages-1], bassRoll[0..kMaxBassPages-1], drumRoll
        juce::ValueTree rollsNode("Rolls");
        for (int i = 0; i < (int)p.layerRoll.size(); ++i)
        {
            auto rn = rollToValueTree("LayerRoll", p.layerRoll[i]);
            rn.setProperty("page", i, nullptr);
            rollsNode.addChild(rn, -1, nullptr);
        }
        for (int i = 0; i < (int)p.bassRoll.size(); ++i)
        {
            auto rn = rollToValueTree("BassRoll", p.bassRoll[i]);
            rn.setProperty("page", i, nullptr);
            rollsNode.addChild(rn, -1, nullptr);
        }
        rollsNode.addChild(rollToValueTree("DrumRoll", p.drumRoll), -1, nullptr);
        // D1.1 (2026-04-24): per-drum piano rolls for the dynamic-drum model.
        // Saved in addition to legacy DrumRoll until D1.4 cutover.
        for (int i = 0; i < (int)p.drumRolls.size(); ++i)
        {
            if (p.drumRolls[i].notes.empty()) continue;   // skip empty rolls
            auto rn = rollToValueTree("DrumPageRoll", p.drumRolls[i]);
            rn.setProperty("page", i, nullptr);
            rollsNode.addChild(rn, -1, nullptr);
        }
        // G-3 (2026-04-28): per-clip piano rolls.  Mirrors the DrumPageRoll
        // pattern - only non-empty rolls are persisted; on load, missing
        // entries default to empty so old projects without ClipPageRoll tags
        // round-trip cleanly.
        for (int i = 0; i < (int)p.clipRoll.size(); ++i)
        {
            if (p.clipRoll[i].notes.empty()) continue;
            auto rn = rollToValueTree("ClipPageRoll", p.clipRoll[i]);
            rn.setProperty("page", i, nullptr);
            rollsNode.addChild(rn, -1, nullptr);
        }
        // G-4 (2026-04-28): per-Vox / per-Inst piano rolls.  Same idempotent
        // pattern as Clips - only non-empty rolls saved.
        for (int i = 0; i < (int)p.voxRoll.size(); ++i)
        {
            if (p.voxRoll[i].notes.empty()) continue;
            auto rn = rollToValueTree("VoxPageRoll", p.voxRoll[i]);
            rn.setProperty("page", i, nullptr);
            rollsNode.addChild(rn, -1, nullptr);
        }
        for (int i = 0; i < (int)p.instRoll.size(); ++i)
        {
            if (p.instRoll[i].notes.empty()) continue;
            auto rn = rollToValueTree("InstPageRoll", p.instRoll[i]);
            rn.setProperty("page", i, nullptr);
            rollsNode.addChild(rn, -1, nullptr);
        }
        // QA-ModelShell TS6 (BLU-447): per-plugin-tab roll.  Skip-if-empty like
        // its siblings, so a project written before TS6 round-trips unchanged.
        for (int i = 0; i < (int)p.pluginRoll.size(); ++i)
        {
            if (p.pluginRoll[i].notes.empty()) continue;
            auto rn = rollToValueTree("PluginPageRoll", p.pluginRoll[i]);
            rn.setProperty("page", i, nullptr);
            rollsNode.addChild(rn, -1, nullptr);
        }
        // J-7b (2026-05-04): BaySickRustyDrums singleton roll.  No page index
        // (one Rusty engine per project).  Idempotent skip-if-empty so old
        // projects without this tag round-trip cleanly.
        if (! p.baySickRustyDrumsRoll.notes.empty())
            rollsNode.addChild(rollToValueTree("BaySickRustyDrumsRoll",
                                                p.baySickRustyDrumsRoll), -1, nullptr);
        pNode.addChild(rollsNode, -1, nullptr);

        patternsNode.addChild(pNode, -1, nullptr);
    }
    root.addChild(patternsNode, -1, nullptr);

    // ── Arrangement (full block fields: audio clip + automation lanes too) ──
    juce::ValueTree arrNode("Arrangement");
    for (auto& b : mArrangement)
    {
        juce::ValueTree bNode("Block");
        bNode.setProperty("trackRow",       b.trackRow,       nullptr);
        bNode.setProperty("patternIndex",   b.patternIndex,   nullptr);
        bNode.setProperty("startBar",       b.displayStartBar(), nullptr);   // 8A: derived display bar (legacy readers)
        bNode.setProperty("lengthBars",     b.lengthBars,     nullptr);
        bNode.setProperty("lengthTicks",    b.lengthTicks,    nullptr);   // QA-Ee: 96 PPQ tick length (-1 = unset)
        bNode.setProperty("layerTrack",     b.layerTrack,     nullptr);
        bNode.setProperty("clipType",       (int) b.clipType, nullptr);
        bNode.setProperty("audioFilePath",  b.audioFilePath,  nullptr);
        bNode.setProperty("displayAlias",   b.displayAlias,   nullptr);
        bNode.setProperty("pitchSemitones", b.pitchSemitones, nullptr);
        bNode.setProperty("originalBPM",    b.originalBPM,    nullptr);
        bNode.setProperty("stretchMode",    b.stretchMode,    nullptr);
        bNode.setProperty("muted",          b.muted,          nullptr);
        // QA-E Task 3 (2026-05-12): persist routeChannel so Vox/Inst-recorded
        // audio clips remember which engine's chain they should play through
        // across save/load.  Added 2026-05-03 to ArrangementBlock for FilePlay
        // routing but the save/load pair was forgotten -- on reload, blocks
        // defaulted to routeChannel=0 and audio re-spawned a Clips strip
        // instead of routing through the original Vox/Inst page.  Default 0
        // on deserialize preserves backward compatibility with pre-fix saves.
        bNode.setProperty("routeChannel",   b.routeChannel,   nullptr);
        // QA-F (2026-07-09): persist the align-bake marker so the composite /
        // signature exclusion survives save/load (default false on read
        // leaves pre-QA-F saves unchanged).
        bNode.setProperty("alignBake",      b.alignBake,      nullptr);
        // QA-E Task 7 (FILE-02): persist the per-copy override flag (see
        // ArrangementBlock::isOverride).  New saves always carry it.
        bNode.setProperty("isOverride",     b.isOverride,      nullptr);
        // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim):
        // persist the audio-clip content-start offset so reopened recordings
        // + manual slip-edits survive save/load.  Default 0 on read leaves
        // every pre-Task-0c project unchanged (see deserialize below).
        bNode.setProperty("contentStartSamples",
                          (juce::int64) b.contentStartSamples, nullptr);
        // 8A (QA-G3Smoke): startBeats is authoritative in memory; persist as
        // full-precision ticks (96 PPQ round-trips every snapped value
        // exactly).  Same XML property pair as before -- no migration.
        bNode.setProperty("startTicks", beatsToTicks (b.startBeats), nullptr);
        // QA-G Task 5: slice content offset (skip when 0 -- unsliced default).
        if (b.contentOffsetTicks != 0)
            bNode.setProperty("contentOffsetTicks", b.contentOffsetTicks, nullptr);
        if (b.clipType == ClipType::Automation)
            bNode.addChild(automationLaneToValueTree(b.automationLane), -1, nullptr);
        arrNode.addChild(bNode, -1, nullptr);
    }
    root.addChild(arrNode, -1, nullptr);

    // ── D-2 (2026-04-26): time markers + time-signature changes ────────────
    {
        juce::ValueTree tm("TimeMarkers");
        for (const auto& m : mTimeMarkers)
        {
            juce::ValueTree e("Marker");
            e.setProperty("bar",   m.bar,   nullptr);
            e.setProperty("label", m.label, nullptr);
            tm.addChild(e, -1, nullptr);
        }
        root.addChild(tm, -1, nullptr);

        juce::ValueTree ts("TimeSigChanges");
        ts.setProperty("currentUid", mCurrentTsMarkerUid, nullptr);   // QA-G Task 6
        ts.setProperty("nextUid",    mNextTsUid,          nullptr);
        for (const auto& s : mTimeSigChanges)
        {
            juce::ValueTree e("TS");
            e.setProperty("bar", s.bar, nullptr);
            e.setProperty("num", s.num, nullptr);
            e.setProperty("den", s.den, nullptr);
            e.setProperty("uid",           s.uid,           nullptr);
            e.setProperty("linkedPattern", s.linkedPattern, nullptr);
            ts.addChild(e, -1, nullptr);
        }
        root.addChild(ts, -1, nullptr);

        // QA-TempoMap (2026-07-08): ruler tempo flags.
        juce::ValueTree tc("TempoChanges");
        for (const auto& t : mTempoChanges)
        {
            juce::ValueTree e("Tempo");
            e.setProperty("bar", t.bar, nullptr);
            e.setProperty("bpm", t.bpm, nullptr);
            tc.addChild(e, -1, nullptr);
        }
        root.addChild(tc, -1, nullptr);
    }

    // ── Audio library + automation-lane template library ────────────────────
    {
        juce::ValueTree lib("AudioLibrary");
        for (const auto& e : mAudioLibrary)
        {
            juce::ValueTree en("Entry");
            en.setProperty("path",               e.path,               nullptr);
            en.setProperty("alias",              e.alias,              nullptr);
            en.setProperty("chokeGroup",         e.chokeGroup,         nullptr);   // D3
            en.setProperty("pageOwnerChannelId", e.pageOwnerChannelId, nullptr);   // QA-E Task 4
            // QA-E Task 7 (FILE-02): source-of-truth clip props on the entry.
            en.setProperty("libPitch",           e.pitchSemitones,     nullptr);
            en.setProperty("libBPM",             e.originalBPM,        nullptr);
            en.setProperty("libStretch",         e.stretchMode,        nullptr);
            en.setProperty("group",              e.groupName,          nullptr);   // QA-Fe2
            lib.addChild(en, -1, nullptr);
        }
        root.addChild(lib, -1, nullptr);
    }
    {
        // QA-Fe2: user-created browser groups (may be empty of members).
        juce::ValueTree grp("AudioGroups");
        for (const auto& [cat, name] : mManualAudioGroups)
        {
            juce::ValueTree g("Group");
            g.setProperty("cat",  cat,  nullptr);
            g.setProperty("name", name, nullptr);
            grp.addChild(g, -1, nullptr);
        }
        root.addChild(grp, -1, nullptr);
    }
    {
        juce::ValueTree lib("AutomationTemplates");
        for (const auto& lane : mAutomationTemplates)
            lib.addChild(automationLaneToValueTree(lane), -1, nullptr);
        root.addChild(lib, -1, nullptr);
    }

    return root;
}

void PatternManager::fromValueTree(const juce::ValueTree& root)
{
    // Whole-body bracket: both containers are emptied and then refilled one
    // push_back at a time, so there is no instant in here at which the audio
    // thread could safely be walking them.  Reached from deserializeProject
    // with the shield already up, where the guard nests for free.
    const ScopedAudioShield shield (*this);
    mPatterns.clear();
    mArrangement.clear();
    mAudioLibrary.clear();
    mManualAudioGroups.clear();   // QA-Fe2
    mAutomationTemplates.clear();
    mCurrentPattern = (int)root.getProperty("currentPattern", 0);
    mGlobalTempo    = (double) root.getProperty ("globalTempo", 120.0);
    const int fileVersion = (int) root.getProperty("version", 0);   // 0 = pre-P1 legacy files
    juce::ignoreUnused (fileVersion);

    // ── Mixer ───────────────────────────────────────────────────────────────
    auto mixNode = root.getChildWithName("Mixer");
    if (mixNode.isValid())
    {
        mMixer.masterLevel        = (float)(double) mixNode.getProperty("masterLevel",        1.0);
        mMixer.layersLevel        = (float)(double) mixNode.getProperty("layersLevel",        1.0);
        mMixer.bassLevel          = (float)(double) mixNode.getProperty("bassLevel",          1.0);
        mMixer.drumsLevel         = (float)(double) mixNode.getProperty("drumsLevel",         1.0);
        mMixer.layersMute         = (bool)          mixNode.getProperty("layersMute",         false);
        mMixer.bassMute           = (bool)          mixNode.getProperty("bassMute",           false);
        mMixer.drumsMute          = (bool)          mixNode.getProperty("drumsMute",          false);
        mMixer.layersSolo         = (bool)          mixNode.getProperty("layersSolo",         false);
        mMixer.bassSolo           = (bool)          mixNode.getProperty("bassSolo",           false);
        mMixer.drumsSolo          = (bool)          mixNode.getProperty("drumsSolo",          false);
        mMixer.masterPan          = (float)(double) mixNode.getProperty("masterPan",          0.0);
        mMixer.layersPan          = (float)(double) mixNode.getProperty("layersPan",          0.0);
        mMixer.bassPan            = (float)(double) mixNode.getProperty("bassPan",            0.0);
        mMixer.drumsPan           = (float)(double) mixNode.getProperty("drumsPan",           0.0);
        mMixer.audioClipsBusLevel = (float)(double) mixNode.getProperty("audioClipsBusLevel", 1.0);
        mMixer.audioClipsBusPan   = (float)(double) mixNode.getProperty("audioClipsBusPan",   0.0);
        mMixer.audioClipsBusMute  = (bool)          mixNode.getProperty("audioClipsBusMute",  false);
        mMixer.audioClipsBusSolo  = (bool)          mixNode.getProperty("audioClipsBusSolo",  false);
        {
            auto csv = [](const juce::String& s) { return juce::StringArray::fromTokens(s, ",", ""); };
            auto slot = csv(mixNode.getProperty("drumSlotLevel", juce::String()).toString());
            auto span = csv(mixNode.getProperty("drumSlotPan",   juce::String()).toString());
            auto aLv  = csv(mixNode.getProperty("audioRowLevel", juce::String()).toString());
            auto aMu  =     mixNode.getProperty("audioRowMute",  juce::String()).toString();
            // The per-index size test carries the pre-QA-SOUNDNESS 16-value CSV:
            // a project saved before the second drum bank keeps its bank-1
            // values and leaves slots 16-31 at their MixerState defaults.
            for (int i = 0; i < kMaxDrumPages; ++i)
            {
                if (i < slot.size()) mMixer.drumSlotLevel[i] = slot[i].getFloatValue();
                if (i < span.size()) mMixer.drumSlotPan  [i] = span[i].getFloatValue();
            }
            for (int i = 0; i < MixerState::kMaxAudioRows; ++i)
            {
                if (i < aLv.size()) mMixer.audioRowLevel[i] = aLv[i].getFloatValue();
                if (i < aMu.length()) mMixer.audioRowMute[i] = (aMu[i] == '1');
            }
        }
    }

    // ── Row state ───────────────────────────────────────────────────────────
    {
        // Names + groups always reset to defaults first so a project without
        // <Row> children (or no RowState at all) never inherits prior state.
        for (int i = 0; i < kMaxArrangementRows; ++i)
            mRowNames[i] = defaultRowName(i);
        mRowGroupId.fill(0);
        mRowGroupColor.fill(0);

        auto n = root.getChildWithName("RowState");
        if (n.isValid())
        {
            juce::String mute = n.getProperty("mute", juce::String()).toString();
            juce::String solo = n.getProperty("solo", juce::String()).toString();
            bool anySolo = false;
            for (int i = 0; i < kMaxArrangementRows; ++i)
            {
                const bool m = (i < mute.length() && mute[i] == '1');
                const bool s = (i < solo.length() && solo[i] == '1');
                mRowMuted [i].store(m, std::memory_order_relaxed);
                mRowSoloed[i].store(s, std::memory_order_relaxed);
                if (s) anySolo = true;
            }
            mAnyRowSoloed.store(anySolo, std::memory_order_relaxed);

            for (const auto r : n)
            {
                if (!r.hasType("Row")) continue;
                const int i = (int) r.getProperty("i", -1);
                if (i < 0 || i >= kMaxArrangementRows) continue;
                if (r.hasProperty("name")) mRowNames[i] = r.getProperty("name").toString();
                mRowGroupId[i]    = (int) r.getProperty("group", 0);
                mRowGroupColor[i] = (juce::uint32)(int) r.getProperty("groupColor", 0);
            }
        }
    }

    // ── Patterns ─────────────────────────────────────────────────────────────
    auto patternsNode = root.getChildWithName("Patterns");
    for (const auto pNode : patternsNode)
    {
        Pattern p;
        p.name        = pNode.getProperty("name",        "Pattern").toString();
        p.bars        = (int)pNode.getProperty("bars",        DEFAULT_BARS);
        p.stepsPerBar = (int)pNode.getProperty("stepsPerBar", DEFAULT_SPB);
        // C.5b: per-pattern intrinsic TS (defaults 4/4 for legacy projects).
        p.tsNum       = (int) pNode.getProperty("tsNum",    4);
        p.tsDen       = (int) pNode.getProperty("tsDen",    4);
        p.tsLocked    =       pNode.getProperty("tsLocked", false);
        p.tsBoundMarkerUid = (int) pNode.getProperty("tsBoundUid", -1);   // QA-G Task 6
        // F-1: missing color attribute → fall back to default (light grey).
        if (pNode.hasProperty("color"))
            p.color = juce::Colour ((juce::uint32) (int) pNode.getProperty ("color"));

        // Piano-roll notes
        auto rollsNode = pNode.getChildWithName("Rolls");
        if (rollsNode.isValid())
        {
            for (int i = 0; i < rollsNode.getNumChildren(); ++i)
            {
                auto rn = rollsNode.getChild(i);
                if (rn.hasType("LayerRoll"))
                {
                    int page = (int) rn.getProperty("page", 0);
                    if (page >= 0 && page < (int)p.layerRoll.size())
                        rollFromValueTree(rn, p.layerRoll[page]);
                }
                else if (rn.hasType("BassRoll"))
                {
                    int page = (int) rn.getProperty("page", 0);
                    if (page >= 0 && page < (int)p.bassRoll.size())
                        rollFromValueTree(rn, p.bassRoll[page]);
                }
                else if (rn.hasType("DrumPageRoll"))
                {
                    // D1.1 (2026-04-24): per-drum roll for the dynamic-drum
                    // model.  Notes are standard piano-roll notes (no slot
                    // encoding), so no migration needed.
                    int page = (int) rn.getProperty("page", 0);
                    if (page >= 0 && page < (int)p.drumRolls.size())
                        rollFromValueTree(rn, p.drumRolls[page]);
                }
                else if (rn.hasType("ClipPageRoll"))
                {
                    // G-3 (2026-04-28): per-clip roll.  Page index = audio-row
                    // index for the bound clip.  Standard piano-roll notes.
                    int page = (int) rn.getProperty("page", 0);
                    if (page >= 0 && page < (int)p.clipRoll.size())
                        rollFromValueTree(rn, p.clipRoll[page]);
                }
                else if (rn.hasType("VoxPageRoll"))
                {
                    // G-4 (2026-04-28): per-Vox roll.  Page index = Vox insert idx.
                    int page = (int) rn.getProperty("page", 0);
                    if (page >= 0 && page < (int)p.voxRoll.size())
                        rollFromValueTree(rn, p.voxRoll[page]);
                }
                else if (rn.hasType("InstPageRoll"))
                {
                    // G-4 (2026-04-28): per-Inst roll.  Page index = Inst insert idx.
                    int page = (int) rn.getProperty("page", 0);
                    if (page >= 0 && page < (int)p.instRoll.size())
                        rollFromValueTree(rn, p.instRoll[page]);
                }
                else if (rn.hasType("PluginPageRoll"))
                {
                    // QA-ModelShell TS6: per-plugin-tab roll.
                    int page = (int) rn.getProperty("page", 0);
                    if (page >= 0 && page < (int)p.pluginRoll.size())
                        rollFromValueTree(rn, p.pluginRoll[page]);
                }
                else if (rn.hasType("BaySickRustyDrumsRoll"))
                {
                    // J-7b (2026-05-04): singleton Rusty roll.
                    rollFromValueTree(rn, p.baySickRustyDrumsRoll);
                }
                else if (rn.hasType("DrumRoll"))
                {
                    rollFromValueTree(rn, p.drumRoll);
                    // Phase C §P4.2 C1 migration (2026-04-24):
                    // Old drumRoll encoding used `midiNote = 51 - slot` to
                    // identify the slot and lock pitch to the row.  New
                    // encoding uses `slotIndex` + an independent `midiNote`
                    // so slots can play any pitch in full-roll mode.  Any
                    // drumRoll note loaded with slotIndex == -1 AND midiNote
                    // in the legacy range [36..51] is migrated here:
                    //   slotIndex = 51 - oldMidiNote
                    //   midiNote  = 60  (C5, native pitch)
                    // New-format notes carry slotIndex on save so the "legacy
                    // note" check stays correct across multiple load/save
                    // cycles.  Notes OUTSIDE [36..51] with slotIndex == -1
                    // are rare and left untouched (might be corrupt data).
                    for (auto& n : p.drumRoll.notes)
                    {
                        if (n.slotIndex < 0
                            && n.midiNote >= 36 && n.midiNote <= 51)
                        {
                            n.slotIndex = 51 - n.midiNote;
                            n.midiNote  = 60;
                        }
                    }
                }
            }

            // D1.1 (2026-04-24): legacy → dynamic-drum migration.  If we got
            // here from a pre-D1 project, the legacy `drumRoll` is populated
            // and `drumRolls[]` is empty.  Fold each legacy note into
            // drumRolls[slotIndex] so the new per-drum playback path sees it.
            // Skip if drumRolls[] already has data (project saved post-D1).
            bool drumRollsEmpty = true;
            for (auto& dr : p.drumRolls)
                if (! dr.notes.empty()) { drumRollsEmpty = false; break; }
            if (drumRollsEmpty && ! p.drumRoll.notes.empty())
            {
                for (auto& n : p.drumRoll.notes)
                {
                    const int slot = (n.slotIndex >= 0 && n.slotIndex < kMaxDrumPages)
                                       ? n.slotIndex : -1;
                    if (slot < 0) continue;
                    PianoNote copy = n;
                    copy.slotIndex = -1;   // dynamic-drum model has no slot encoding
                    p.drumRolls[slot].notes.push_back (copy);
                }
            }
        }

        mPatterns.push_back(std::move(p));
    }

    if (mPatterns.empty()) addPattern("Pattern 1");
    mCurrentPattern = juce::jlimit(0, (int)mPatterns.size()-1, mCurrentPattern);

    // ── Arrangement ─────────────────────────────────────────────────────────
    auto arrNode = root.getChildWithName("Arrangement");
    for (const auto bNode : arrNode)
    {
        ArrangementBlock b;
        b.trackRow       = (int)             bNode.getProperty("trackRow",       0);
        b.patternIndex   = (int)             bNode.getProperty("patternIndex",   0);
        const int legacyStartBar = (int)     bNode.getProperty("startBar",       0);
        b.lengthBars     = (int)             bNode.getProperty("lengthBars",     4);
        // QA-Ee (96 PPQ): prefer the tick property; else migrate the legacy
        // float "lengthBeats" (beats x 96 -> ticks).  Absent/unset -> bars.
        if (bNode.hasProperty ("lengthTicks"))
            b.lengthTicks = (juce::int64) bNode.getProperty ("lengthTicks", (juce::int64) ArrangementBlock::kLengthTicksUnset);
        else
        {
            const float lb = (float) bNode.getProperty ("lengthBeats", -1.f);
            b.lengthTicks  = (lb > 0.f) ? beatsToTicks ((double) lb) : ArrangementBlock::kLengthTicksUnset;
        }
        b.layerTrack     = (bool)            bNode.getProperty("layerTrack",     true);
        b.clipType       = (ClipType)(int)   bNode.getProperty("clipType",       (int) ClipType::Pattern);
        b.audioFilePath  =                   bNode.getProperty("audioFilePath",  juce::String()).toString();
        b.displayAlias   =                   bNode.getProperty("displayAlias",   juce::String()).toString();
        b.pitchSemitones = (float)(double)   bNode.getProperty("pitchSemitones", 0.0);
        b.originalBPM    = (float)(double)   bNode.getProperty("originalBPM",    120.0);
        b.stretchMode    = (bool)            bNode.getProperty("stretchMode",    true);
        b.muted          = (bool)            bNode.getProperty("muted",          false);
        // QA-E Task 3 (2026-05-12): see toValueTree comment.  Default 0 on
        // pre-fix saves -> legacy "no Vox/Inst routing" behavior.  New saves
        // carry the field so Vox/Inst-recorded clips persist their route.
        b.routeChannel   = (int)             bNode.getProperty("routeChannel",   0);
        b.alignBake      = (bool)            bNode.getProperty("alignBake",      false);
        // QA-E Task 7 (FILE-02): default TRUE on read.  Pre-Task-7 saves have
        // no "isOverride" attribute -> treat every existing block as a fully
        // customized copy so its saved pitch / BPM / mode / route are all
        // preserved exactly and the library original does NOT silently change
        // it on reopen (Jeff's call, 2026-05-15).  New saves carry the flag.
        b.isOverride     = (bool)            bNode.getProperty("isOverride",     true);
        // QA-Ea Task 0c (FL pre-roll record): restore the audio-clip
        // content-start offset.  Default 0 on read = play from file sample 0
        // (every pre-Task-0c project is backwards-compatible; no silent
        // shift on reopen).
        b.contentStartSamples = (juce::int64) bNode.getProperty (
            "contentStartSamples", (juce::int64) 0);
        // 8A (QA-G3Smoke): startBeats is authoritative in memory.  Prefer the
        // tick property (full precision, written by every post-QA-Ee save);
        // else the legacy float "startBeats"; else the bar-precision startBar.
        if (bNode.hasProperty ("startTicks"))
            b.startBeats = ticksToBeats ((juce::int64) bNode.getProperty ("startTicks", (juce::int64) 0));
        else
        {
            const double sb = (double) bNode.getProperty ("startBeats", (double) -1.0e6);
            b.startBeats    = (sb > -1.0e5) ? sb : (double) legacyStartBar * 4.0;
        }
        b.contentOffsetTicks = (juce::int64) bNode.getProperty ("contentOffsetTicks", (juce::int64) 0);
        if (b.clipType == ClipType::Automation)
        {
            auto la = bNode.getChildWithName("AutomationLane");
            if (la.isValid()) b.automationLane = automationLaneFromValueTree(la);
        }
        mArrangement.push_back(b);
    }

    // ── D-2 (2026-04-26): time markers + time-signature changes ────────────
    mTimeMarkers.clear();
    mTimeSigChanges.clear();
    {
        auto tm = root.getChildWithName("TimeMarkers");
        for (int i = 0; i < tm.getNumChildren(); ++i)
        {
            auto e = tm.getChild(i);
            if (! e.hasType("Marker")) continue;
            TimeMarker m;
            m.bar   = (int) e.getProperty("bar", 0);
            m.label = e.getProperty("label", juce::String()).toString();
            mTimeMarkers.push_back(m);
        }
        std::sort(mTimeMarkers.begin(), mTimeMarkers.end(),
                  [](const TimeMarker& a, const TimeMarker& b) { return a.bar < b.bar; });

        auto ts = root.getChildWithName("TimeSigChanges");
        mCurrentTsMarkerUid = (int) ts.getProperty("currentUid", -1);
        mNextTsUid          = (int) ts.getProperty("nextUid",     1);
        for (int i = 0; i < ts.getNumChildren(); ++i)
        {
            auto e = ts.getChild(i);
            if (! e.hasType("TS")) continue;
            TimeSigChange s;
            s.bar = (int) e.getProperty("bar", 0);
            s.num = (int) e.getProperty("num", 4);
            s.den = (int) e.getProperty("den", 4);
            s.uid           = (int) e.getProperty("uid", -1);
            s.linkedPattern = (int) e.getProperty("linkedPattern", -1);
            mTimeSigChanges.push_back(s);
        }
        std::sort(mTimeSigChanges.begin(), mTimeSigChanges.end(),
                  [](const TimeSigChange& a, const TimeSigChange& b) { return a.bar < b.bar; });
        // QA-G Task 6: pre-uid projects -- assign fresh uids; sole marker
        // auto-becomes current when no selection was stored.
        for (auto& s : mTimeSigChanges)
            if (s.uid < 0) s.uid = mNextTsUid++;
        if (mCurrentTsMarkerUid < 0 && (int) mTimeSigChanges.size() == 1)
            mCurrentTsMarkerUid = mTimeSigChanges[0].uid;

        // QA-TempoMap (2026-07-08): ruler tempo flags (absent in old projects).
        mTempoChanges.clear();
        auto tc = root.getChildWithName("TempoChanges");
        for (int i = 0; i < tc.getNumChildren(); ++i)
        {
            auto e = tc.getChild(i);
            if (! e.hasType("Tempo")) continue;
            TempoChange t;
            t.bar = (int)    e.getProperty("bar", 0);
            t.bpm = (double) e.getProperty("bpm", 120.0);
            mTempoChanges.push_back(t);
        }
        std::sort(mTempoChanges.begin(), mTempoChanges.end(),
                  [](const TempoChange& a, const TempoChange& b) { return a.bar < b.bar; });
    }

    // QA-G Task 6: patterns + arrangement + markers are all in -- resolve the
    // follower caches and hand the marker timeline to the audio side.  (No
    // notifyContentChanged here: loading must not dirty the project.)
    refreshPatternTimeSigs();
    publishTimeSigMap();

    // ── Audio library + automation-lane template library ────────────────────
    {
        auto lib = root.getChildWithName("AudioLibrary");
        for (int i = 0; i < lib.getNumChildren(); ++i)
        {
            auto e = lib.getChild(i);
            if (! e.hasType("Entry")) continue;
            mAudioLibrary.push_back({
                e.getProperty("path",               juce::String()).toString(),
                e.getProperty("alias",              juce::String()).toString(),
                (int) e.getProperty("chokeGroup",         0),    // D3 (default 0 for legacy projects)
                (int) e.getProperty("pageOwnerChannelId", 0),    // QA-E Task 4 (default 0 = generic Audio for legacy)
                // QA-E Task 7 (FILE-02): source-of-truth clip props.  Legacy
                // projects have no lib* attrs -> neutral defaults; their grid
                // blocks all deserialize isOverride=true so they don't follow.
                (float) e.getProperty("libPitch",   0.0),
                (float) e.getProperty("libBPM",     120.0),
                (bool)  e.getProperty("libStretch", true),
                e.getProperty("group", juce::String()).toString()   // QA-Fe2
            });
        }
    }
    {
        auto grp = root.getChildWithName("AudioGroups");   // QA-Fe2
        for (int i = 0; i < grp.getNumChildren(); ++i)
        {
            auto g = grp.getChild(i);
            if (g.hasType("Group"))
                mManualAudioGroups.emplace_back((int) g.getProperty("cat", 0),
                                                g.getProperty("name", juce::String()).toString());
        }
    }
    {
        auto lib = root.getChildWithName("AutomationTemplates");
        for (int i = 0; i < lib.getNumChildren(); ++i)
        {
            auto lane = lib.getChild(i);
            if (lane.hasType("AutomationLane"))
                mAutomationTemplates.push_back(automationLaneFromValueTree(lane));
        }
    }

    // #30b: loaded content replaced every pattern wholesale -- publish the roll
    // snapshots directly (no notifyContentChanged: loading must not dirty).
    publishAllRollSnapshots();
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-track mute / solo (arrangement playback gate)
// ─────────────────────────────────────────────────────────────────────────────
void PatternManager::setRowName(int row, const juce::String& name)
{
    if (row < 0 || row >= kMaxArrangementRows) return;
    if (mRowNames[row] == name) return;
    mRowNames[row] = name;
    notifyContentChanged();
}

int PatternManager::getRowGroup(int row) const
{
    if (row < 0 || row >= kMaxArrangementRows) return 0;
    return mRowGroupId[row];
}

void PatternManager::setRowGroup(int row, int groupId)
{
    if (row < 0 || row >= kMaxArrangementRows) return;
    if (mRowGroupId[row] == groupId) return;
    mRowGroupId[row] = groupId;
    notifyContentChanged();
}

juce::uint32 PatternManager::getRowGroupColor(int row) const
{
    if (row < 0 || row >= kMaxArrangementRows) return 0;
    return mRowGroupColor[row];
}

void PatternManager::setRowGroupColor(int row, juce::uint32 argb)
{
    if (row < 0 || row >= kMaxArrangementRows) return;
    if (mRowGroupColor[row] == argb) return;
    mRowGroupColor[row] = argb;
    notifyContentChanged();
}

int PatternManager::allocateRowGroupId() const
{
    int maxId = 0;
    for (const int g : mRowGroupId) maxId = juce::jmax(maxId, g);
    return maxId + 1;
}

void PatternManager::setRowMuted(int row, bool m)
{
    if (row < 0 || row >= kMaxArrangementRows) return;
    mRowMuted[row].store(m, std::memory_order_relaxed);
}

void PatternManager::setRowSoloed(int row, bool s)
{
    if (row < 0 || row >= kMaxArrangementRows) return;
    mRowSoloed[row].store(s, std::memory_order_relaxed);

    // Recompute anyRowSoloed cache so isRowAudible() stays O(1).
    bool any = false;
    for (auto& a : mRowSoloed)
        if (a.load(std::memory_order_relaxed)) { any = true; break; }
    mAnyRowSoloed.store(any, std::memory_order_relaxed);
}

bool PatternManager::isRowMuted(int row) const
{
    if (row < 0 || row >= kMaxArrangementRows) return false;
    return mRowMuted[row].load(std::memory_order_relaxed);
}

bool PatternManager::isRowSoloed(int row) const
{
    if (row < 0 || row >= kMaxArrangementRows) return false;
    return mRowSoloed[row].load(std::memory_order_relaxed);
}

bool PatternManager::isRowAudible(int row) const
{
    if (row < 0 || row >= kMaxArrangementRows) return true;
    if (mRowMuted[row].load(std::memory_order_relaxed)) return false;
    if (mAnyRowSoloed.load(std::memory_order_relaxed)
        && !mRowSoloed[row].load(std::memory_order_relaxed))
        return false;
    return true;
}
