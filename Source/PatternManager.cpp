#include "PatternManager.h"

// ── AutomationLane::evaluateAt ────────────────────────────────────────────────
float AutomationLane::evaluateAt(float pos01) const
{
    pos01 = juce::jlimit(0.f, 1.f, pos01);

    // ── LFO mode ──────────────────────────────────────────────────────────────
    if (isLFO)
    {
        if (lfoRate <= 0.f) return (lfoMin + lfoMax) * 0.5f;
        float phase = std::fmod(pos01 / lfoRate, 1.f);
        float raw = 0.f;
        switch (lfoShape)
        {
            case 0: // Sine
                raw = 0.5f + 0.5f * std::sin(phase * juce::MathConstants<float>::twoPi);
                break;
            case 1: // Triangle
                raw = (phase < 0.5f) ? (phase * 2.f) : (2.f - phase * 2.f);
                break;
            case 2: // Sawtooth
                raw = phase;
                break;
            case 3: // Square
                raw = (phase < 0.5f) ? 1.f : 0.f;
                break;
            default: raw = phase; break;
        }
        return juce::jlimit(0.f, 1.f, lfoMin + raw * (lfoMax - lfoMin));
    }

    // ── Point-based curve ─────────────────────────────────────────────────────
    if (points.empty()) return 0.5f;
    if (points.size() == 1) return points[0].value01;

    // Work on a sorted copy (points may be in insertion order during editing)
    auto sorted = points;
    std::sort(sorted.begin(), sorted.end(),
        [](const ControlPoint& a, const ControlPoint& b)
            { return a.timeTicks < b.timeTicks; });

    if (pos01 <= sorted.front().timeTicks) return sorted.front().value01;
    if (pos01 >= sorted.back().timeTicks)  return sorted.back().value01;

    // Find bracketing segment
    int idx = 0;
    for (int i = 0; i < (int)sorted.size() - 1; ++i)
    {
        if (pos01 >= sorted[i].timeTicks && pos01 < sorted[i + 1].timeTicks)
        {
            idx = i;
            break;
        }
    }

    const auto& p0 = sorted[idx];
    const auto& p1 = sorted[idx + 1];
    const float span = p1.timeTicks - p0.timeTicks;
    if (span <= 0.f) return p0.value01;

    const float t = (pos01 - p0.timeTicks) / span;

    switch (p0.curveType)
    {
        case CurveType::Stepped:
            return p0.value01;

        case CurveType::Linear:
        {
            // FL Studio Scaled Exponential Transfer Function:
            //   t_factor = 1 - T / (0.5*T + 0.5)^2
            //   y = (t_factor^x - 1) / (t_factor - 1)
            // Positive tension = ease-out, negative = ease-in.
            float T = juce::jlimit(-0.999f, 0.999f, p0.tension);
            if (std::abs(T) < 0.001f)
                return p0.value01 + t * (p1.value01 - p0.value01);

            float denom    = 0.5f * T + 0.5f;
            float t_factor = 1.0f - T / (denom * denom);

            float y;
            if (std::abs(t_factor - 1.0f) < 0.0001f)
                y = t;
            else
                y = (std::pow(std::abs(t_factor), t) - 1.0f) / (t_factor - 1.0f);

            return p0.value01 + juce::jlimit(0.0f, 1.0f, y) * (p1.value01 - p0.value01);
        }

        case CurveType::Spline:
        {
            // Catmull-Rom using neighbouring points; clamp at ends
            float v0 = (idx > 0)                       ? sorted[idx - 1].value01 : p0.value01;
            float v1 = p0.value01;
            float v2 = p1.value01;
            float v3 = (idx + 2 < (int)sorted.size()) ? sorted[idx + 2].value01 : p1.value01;
            float t2 = t * t, t3 = t2 * t;
            float val = 0.5f * (
                  2.f * v1
                + (-v0 + v2)                         * t
                + (2.f * v0 - 5.f * v1 + 4.f * v2 - v3) * t2
                + (-v0 + 3.f * v1 - 3.f * v2 + v3)  * t3);
            return juce::jlimit(0.f, 1.f, val);
        }

        default:
            return p0.value01 + t * (p1.value01 - p0.value01);
    }
}

const char* PatternManager::kDrumNames[MAX_DRUM_SOUNDS] = {
    // KICK (0-4)
    "Kick Thump", "Kick Snap", "Kick Sub", "808 Kick", "Kick Short",
    // SNARE (5-10)
    "Snare Crack", "Snare Rim", "Snare Brush", "Rimshot", "Cross Stick", "Snare Ghost",
    // HI-HAT (11-15)
    "HH Closed", "HH Open", "HH Pedal", "HH Tight", "HH Loose",
    // CYMBAL (16-20)
    "Ride Bell", "Ride Edge", "Crash", "China", "Splash",
    // TOM (21-24)
    "Tom High", "Tom Mid", "Tom Low", "Floor Tom",
    // PERC (25-30)
    "Clap", "Snap", "Clave", "Cowbell", "Woodblock", "Shaker",
    // ETHNIC (31-35)
    "Tambourine", "Bongo High", "Bongo Low", "Conga", "Djembe",
    // ELECTRONIC (36-41)
    "808 Clap", "808 Tom", "Noise Hit", "Laser", "Glitch", "Vinyl Noise",
    // FX (42-45)
    "Reverse Cymbal", "Pitched Kick", "Sub Boom", "Impact"
};

PatternManager::PatternManager()
{
    mDrumEnabled.fill(false);
    addPattern("Pattern 1");
}

int PatternManager::addPattern(const juce::String& name)
{
    Pattern p;
    p.name = name.isEmpty() ? "Pattern " + juce::String(mPatterns.size()+1) : name;
    mPatterns.push_back(std::move(p));
    notifyContentChanged();
    return (int)mPatterns.size() - 1;
}

int PatternManager::duplicatePattern(int srcIndex)
{
    if (srcIndex < 0 || srcIndex >= (int)mPatterns.size()) return -1;
    Pattern copy = mPatterns[srcIndex];   // deep-copy by value: notes, sequences, rolls
    copy.name = copy.name + " (copy)";
    mPatterns.push_back(std::move(copy));
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
    mAudioLibrary.push_back({ path, alias, 0 /* chokeGroup = none */, pageOwnerChannelId });
}

void PatternManager::removeAudioFromLibrary(const juce::String& path)
{
    for (auto it = mAudioLibrary.begin(); it != mAudioLibrary.end(); ++it)
        if (it->path == path) { mAudioLibrary.erase(it); return; }
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

void PatternManager::renameAutomationTemplate(int idx, const juce::String& newParamId)
{
    if (idx < 0 || idx >= (int)mAutomationTemplates.size()) return;
    mAutomationTemplates[idx].paramId = newParamId;
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
    mPatterns.erase(mPatterns.begin() + index);
    mCurrentPattern = juce::jlimit(0, (int)mPatterns.size()-1, mCurrentPattern);
    notifyContentChanged();
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
}

void PatternManager::addBlock(ArrangementBlock block)
{
    // C.5b (post-revert): auto-derive removed.  Song-level TS markers are
    // decorative-only (visual reference on the Builder ruler), so deriving a
    // pattern's intrinsic TS from a marker would be confusing.  Pattern TS
    // is set explicitly via right-click → "Set Time Signature".
    mArrangement.push_back(block);
    notifyContentChanged();
}

void PatternManager::removeBlock(int index)
{
    if (index >= 0 && index < (int)mArrangement.size())
    {
        mArrangement.erase(mArrangement.begin() + index);
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
    for (auto& b : mArrangement) maxBar = juce::jmax(maxBar, b.startBar + b.lengthBars);
    return juce::jmax(16, maxBar);
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

// ── Time-signature changes (D-2) ────────────────────────────────────────
void PatternManager::addTimeSigChange (int bar, int num, int den)
{
    bar = juce::jmax(0, bar);
    num = juce::jlimit(1, 32, num);
    // Round denominator to the nearest power of 2 in [1, 32].
    static const int kAllowedDen[] = { 1, 2, 4, 8, 16, 32 };
    int closest = 4;
    int bestDiff = 999;
    for (int d : kAllowedDen)
        if (std::abs(d - den) < bestDiff) { closest = d; bestDiff = std::abs(d - den); }
    den = closest;

    // Replace existing change at the same bar (one TS per bar).
    for (auto& ts : mTimeSigChanges)
        if (ts.bar == bar) { ts.num = num; ts.den = den; return; }

    mTimeSigChanges.push_back({ bar, num, den });
    std::sort(mTimeSigChanges.begin(), mTimeSigChanges.end(),
              [](const TimeSigChange& a, const TimeSigChange& b) { return a.bar < b.bar; });
}

void PatternManager::removeTimeSigChange (int idx)
{
    if (idx >= 0 && idx < (int) mTimeSigChanges.size())
        mTimeSigChanges.erase(mTimeSigChanges.begin() + idx);
}

int PatternManager::findTimeSigChangeAtBar (int bar) const
{
    for (int i = 0; i < (int) mTimeSigChanges.size(); ++i)
        if (mTimeSigChanges[(size_t) i].bar == bar) return i;
    return -1;
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

double PatternManager::getBeatsPerBarAtBar (int bar) const
{
    const auto eff = getEffectiveTimeSigAtBar (bar);
    // PPQ beat = quarter note.  Bar length in PPQ = num * (4/den).
    const int den = (eff.den > 0) ? eff.den : 4;
    return (double) eff.num * 4.0 / (double) den;
}

double PatternManager::getBeatsPerBarAtBeat (double beat) const
{
    int bar = 0;
    double bib = 0.0;
    beatToBarAndBeatInBar (beat, bar, bib);
    return getBeatsPerBarAtBar (bar);
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

bool PatternManager::autoDerivePatternTimeSig (int patternIndex, int placementBar)
{
    if (patternIndex < 0 || patternIndex >= (int) mPatterns.size()) return false;
    auto& pat = mPatterns[(size_t) patternIndex];
    if (pat.tsLocked) return false;   // user already set or already auto-derived

    const auto eff = getEffectiveTimeSigAtBar (placementBar);
    pat.tsNum    = eff.num;
    pat.tsDen    = eff.den;
    pat.tsLocked = true;
    return true;
}

void PatternManager::setPatternTimeSig (int patternIndex, int num, int den)
{
    if (patternIndex < 0 || patternIndex >= (int) mPatterns.size()) return;
    auto& pat = mPatterns[(size_t) patternIndex];
    pat.tsNum    = juce::jlimit (1, 32, num);
    // Round denominator to nearest power of 2 in [1, 32].
    static const int kAllowedDen[] = { 1, 2, 4, 8, 16, 32 };
    int closest = 4, bestDiff = 999;
    for (int d : kAllowedDen)
        if (std::abs (d - den) < bestDiff) { closest = d; bestDiff = std::abs (d - den); }
    pat.tsDen    = closest;
    pat.tsLocked = true;
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

void PatternManager::enableDrum(int slot, bool enabled)
{
    if (slot >= 0 && slot < MAX_DRUM_SOUNDS) mDrumEnabled[slot] = enabled;
}

int PatternManager::getNumEnabledDrums() const
{
    int count = 0;
    for (bool e : mDrumEnabled) if (e) ++count;
    return count;
}

double PatternManager::getEffectivePatternLoopBeats() const
{
    // C.5b (post-revert): pattern owns its TS.  Bar length in PPQ = pattern's
    // own bpb (4/4 = 4, 3/4 = 3, 6/8 = 3, 7/8 = 3.5).  Builder grid is
    // uniform 4-beat-per-bar separately (song-level TS markers are decorative
    // only) - but pattern playback length is in pattern-bars, each pat-bpb wide.
    if (mPatterns.empty() || mCurrentPattern < 0 || mCurrentPattern >= (int) mPatterns.size())
        return 4.0;   // safe fallback when no patterns loaded
    const auto&  pat          = mPatterns[(size_t) mCurrentPattern];
    const double patBpb       = juce::jmax (1.0, getPatternBeatsPerBar (mCurrentPattern));
    // Minimum is 1 pattern-bar so a bar-1-only piano roll loops immediately
    // rather than waiting for the full configured bar count.
    const double kMinBeats    = patBpb;

    // Helper: ceil a beat position up to the next bar-start beat using the
    // pattern's intrinsic bpb (uniform within the pattern).
    auto ceilToBarStart = [patBpb](double endBeat) -> double
    {
        if (endBeat <= 0.0) return 0.0;
        const double bars = std::ceil (endBeat / patBpb - 1e-9);
        return bars * patBpb;
    };

    // C.5b: default loop = 1 pattern-bar (kMinBeats).  Note-end + step-end
    // priorities below extend when content exists.  Block-driven priority
    // dropped because Builder bars are uniform 4-beat while patterns play at
    // their intrinsic TS - mixing units broke 7/4 → 3/4 transitions.
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
        // J-7b (2026-05-04): BaySickRustyDrums singleton roll likewise - without
        // this scan, multi-bar drum patterns wrap at the 1-bar minimum instead
        // of the longest note's end.
        scanRoll(pat.baySickRustyDrumsRoll);

        if (latestEnd > 0.0)
            loopBeats = juce::jmax (loopBeats, ceilToBarStart (latestEnd));
    }

    // ── Priority 3: extend to cover any active basic-sequence steps ───────────
    // The step index = (ppqPos / stepLen) % totalSteps, wrapping with mLoopBeats.
    // If active steps exist beyond the current loop length, extend so they fire.
    // C.5b: stepLen still uses 4-beat-per-bar reference for stepsPerBar - the
    // basic step grid is grid-based, not TS-based.  This is consistent with
    // how step grids work in FL-style sequencers.
    {
        auto scanSeq = [&](const PageSequenceData& seq) {
            double stepLen = 4.0 / juce::jmax(1, seq.stepsPerBar);
            int    total   = seq.totalSteps();
            for (int row = 0; row < MAX_DRUM_SOUNDS; ++row)
            {
                for (int s = 0; s < total; ++s)
                {
                    if (seq.basicGrid[row][s].active)
                    {
                        double stepEnd = (s + 1) * stepLen;
                        loopBeats = juce::jmax (loopBeats, ceilToBarStart (stepEnd));
                    }
                }
            }
        };
        scanSeq(pat.layerSeq);
        scanSeq(pat.bassSeq);
        scanSeq(pat.drumSeq);
    }

    return loopBeats;
}

bool PatternManager::isComplexSequenceActive() const
{
    for (auto& p : mPatterns)
    {
        if (p.layerSeq.routing == SeqRouting::ComplexSequence) return true;
        if (p.bassSeq.routing  == SeqRouting::ComplexSequence) return true;
        if (p.drumSeq.routing  == SeqRouting::ComplexSequence) return true;
    }
    return false;
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
        t.setProperty ("m",  n.midiNote,      nullptr);
        t.setProperty ("s",  n.startBeat,     nullptr);
        t.setProperty ("d",  n.durationBeats, nullptr);
        t.setProperty ("v",  n.velocity,      nullptr);
        if (n.panning      != 0.0f)                  t.setProperty ("p",  n.panning,      nullptr);
        if (n.finePitch    != 0.0f)                  t.setProperty ("f",  n.finePitch,    nullptr);
        if (n.type         != NoteType::Standard)    t.setProperty ("t",  (int) n.type,   nullptr);
        if (n.muted)                                 t.setProperty ("u",  true,           nullptr);
        if (n.groupId      != -1)                    t.setProperty ("g",  n.groupId,      nullptr);
        if (n.filterCutoff != 0.5f)                  t.setProperty ("c",  n.filterCutoff, nullptr);
        if (n.slotIndex    != -1)                    t.setProperty ("sl", n.slotIndex,    nullptr);
        return t;
    }

    PianoNote noteFromValueTree (const juce::ValueTree& t)
    {
        PianoNote n;
        n.midiNote      = (int)           t.getProperty ("m",  60);
        n.startBeat     = (double)        t.getProperty ("s",  0.0);
        n.durationBeats = (double)        t.getProperty ("d",  0.25);
        n.velocity      = (float)(double) t.getProperty ("v",  0.8);
        n.panning       = (float)(double) t.getProperty ("p",  0.0);
        n.finePitch     = (float)(double) t.getProperty ("f",  0.0);
        n.type          = (NoteType)(int) t.getProperty ("t",  (int) NoteType::Standard);
        n.muted         = (bool)          t.getProperty ("u",  false);
        n.groupId       = (int)           t.getProperty ("g",  -1);
        n.filterCutoff  = (float)(double) t.getProperty ("c",  0.5);
        n.slotIndex     = (int)           t.getProperty ("sl", -1);
        return n;
    }

    juce::ValueTree rollToValueTree (const juce::String& tag, const PianoRollData& r)
    {
        juce::ValueTree t (tag);
        t.setProperty ("numBars",         r.numBars,         nullptr);
        t.setProperty ("snapDenominator", r.snapDenominator, nullptr);
        for (const auto& n : r.notes) t.addChild (noteToValueTree (n), -1, nullptr);
        return t;
    }

    void rollFromValueTree (const juce::ValueTree& t, PianoRollData& r)
    {
        if (! t.isValid()) return;
        r.notes.clear();
        r.numBars         = (int) t.getProperty ("numBars",         2);
        r.snapDenominator = (int) t.getProperty ("snapDenominator", 32);
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
        return lane;
    }
}

void PatternManager::reset()
{
    mPatterns.clear();
    mPatterns.emplace_back();   // one empty default pattern
    mCurrentPattern = 0;
    mGlobalTempo    = 120.0;
    mArrangement.clear();
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
    mMixer = {};
    mDrumEnabled.fill (true);
    for (auto& m : mRowMuted)  m.store (false, std::memory_order_relaxed);
    for (auto& s : mRowSoloed) s.store (false, std::memory_order_relaxed);
    mAutomationTemplates.clear();
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
        for (int i = 0; i < MAX_DRUM_ROWS;          ++i) { slot.add(juce::String(mMixer.drumSlotLevel[i]));
                                                           span.add(juce::String(mMixer.drumSlotPan[i])); }
        for (int i = 0; i < MixerState::kMaxAudioRows; ++i) { aLv.add(juce::String(mMixer.audioRowLevel[i]));
                                                               aMu.add(mMixer.audioRowMute[i] ? "1" : "0"); }
        mixNode.setProperty("drumSlotLevel", slot.joinIntoString(","), nullptr);
        mixNode.setProperty("drumSlotPan",   span.joinIntoString(","), nullptr);
        mixNode.setProperty("audioRowLevel", aLv .joinIntoString(","), nullptr);
        mixNode.setProperty("audioRowMute",  aMu .joinIntoString(","), nullptr);
    }
    root.addChild(mixNode, -1, nullptr);

    // ── Drum-enabled flags + per-track row mute/solo ─────────────────────────
    {
        juce::ValueTree n("DrumEnabled");
        juce::StringArray bits;
        for (int i = 0; i < MAX_DRUM_SOUNDS; ++i) bits.add(mDrumEnabled[i] ? "1" : "0");
        n.setProperty("bits", bits.joinIntoString(""), nullptr);
        root.addChild(n, -1, nullptr);
    }
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

        // Legacy per-row drum step grid
        for (int d = 0; d < MAX_DRUM_SOUNDS; ++d)
        {
            juce::String bits;
            for (int s = 0; s < p.totalSteps(); ++s)
                bits += p.drumGrid[d][s] ? "1" : "0";
            juce::ValueTree dNode("Drum");
            dNode.setProperty("slot", d, nullptr);
            dNode.setProperty("grid", bits, nullptr);
            pNode.addChild(dNode, -1, nullptr);
        }

        // Per-row drum-sound assignment (drumRowToSlot)
        {
            juce::StringArray vals;
            for (int i = 0; i < MAX_DRUM_ROWS; ++i) vals.add(juce::String(p.drumRowToSlot[i]));
            juce::ValueTree n("DrumRowToSlot");
            n.setProperty("values", vals.joinIntoString(","), nullptr);
            pNode.addChild(n, -1, nullptr);
        }

        // Per-page PageSequenceData - now writes basic+complex grids + full envelopes
        auto savePageSeq = [&](const juce::String& tag, const PageSequenceData& seq)
        {
            juce::ValueTree seqNode(tag);
            seqNode.setProperty("routing", (int)seq.routing,   nullptr);
            seqNode.setProperty("bars",         seq.bars,       nullptr);
            seqNode.setProperty("spb",          seq.stepsPerBar, nullptr);
            seqNode.setProperty("bEnvA",        seq.basicEnv.attack,   nullptr);
            seqNode.setProperty("bEnvH",        seq.basicEnv.hold,     nullptr);
            seqNode.setProperty("bEnvD",        seq.basicEnv.decay,    nullptr);
            seqNode.setProperty("bEnvS",        seq.basicEnv.sustain,  nullptr);
            seqNode.setProperty("bEnvR",        seq.basicEnv.release_, nullptr);
            seqNode.setProperty("cEnvA",        seq.complexEnv.attack,   nullptr);
            seqNode.setProperty("cEnvH",        seq.complexEnv.hold,     nullptr);
            seqNode.setProperty("cEnvD",        seq.complexEnv.decay,    nullptr);
            seqNode.setProperty("cEnvS",        seq.complexEnv.sustain,  nullptr);
            seqNode.setProperty("cEnvR",        seq.complexEnv.release_, nullptr);
            seqNode.setProperty("cSwing",       seq.complexEnv.swing,    nullptr);
            seqNode.setProperty("cTriplet",     seq.complexEnv.triplet,  nullptr);
            // Basic + complex grids - only rows/steps that have non-default content.
            for (int r = 0; r < MAX_DRUM_SOUNDS; ++r)
            {
                bool anyActive = false;
                for (int s = 0; s < seq.totalSteps(); ++s)
                    if (seq.basicGrid[r][s].active) { anyActive = true; break; }
                if (! anyActive) continue;
                juce::ValueTree rowN("BasicRow");
                rowN.setProperty("row", r, nullptr);
                for (int s = 0; s < seq.totalSteps(); ++s)
                {
                    const auto& st = seq.basicGrid[r][s];
                    if (! st.active) continue;
                    juce::ValueTree stN("Step");
                    stN.setProperty("s",  s,           nullptr);
                    stN.setProperty("v",  st.velocity, nullptr);
                    stN.setProperty("l",  st.length,   nullptr);
                    stN.setProperty("fx", st.fxAmount, nullptr);
                    rowN.addChild(stN, -1, nullptr);
                }
                seqNode.addChild(rowN, -1, nullptr);
            }
            for (int r = 0; r < MAX_DRUM_SOUNDS; ++r)
            {
                bool any = false;
                for (int s = 0; s < seq.totalSteps(); ++s)
                    if (seq.complexGrid[r][s].active) { any = true; break; }
                if (! any) continue;
                juce::ValueTree rowN("ComplexRow");
                rowN.setProperty("row", r, nullptr);
                for (int s = 0; s < seq.totalSteps(); ++s)
                {
                    const auto& st = seq.complexGrid[r][s];
                    if (! st.active) continue;
                    juce::ValueTree stN("Step");
                    stN.setProperty("s",  s,              nullptr);
                    stN.setProperty("t",  (int)st.stepType, nullptr);
                    stN.setProperty("v",  st.velocity,    nullptr);
                    stN.setProperty("fx", st.fxAmount,    nullptr);
                    stN.setProperty("n",  st.note,        nullptr);
                    rowN.addChild(stN, -1, nullptr);
                }
                seqNode.addChild(rowN, -1, nullptr);
            }
            pNode.addChild(seqNode, -1, nullptr);
        };
        savePageSeq("LayerSeq", p.layerSeq);
        savePageSeq("BassSeq",  p.bassSeq);
        savePageSeq("DrumSeq",  p.drumSeq);

        // Piano-roll notes - layerRoll[0..7], bassRoll[0..kMaxBassPages-1], drumRoll
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
        bNode.setProperty("startBar",       b.startBar,       nullptr);
        bNode.setProperty("lengthBars",     b.lengthBars,     nullptr);
        bNode.setProperty("lengthBeats",    b.lengthBeats,    nullptr);   // 2026-04-24: sub-bar precision
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
        // QA-E Task 7 (FILE-02): persist the per-copy override flag (see
        // ArrangementBlock::isOverride).  New saves always carry it.
        bNode.setProperty("isOverride",     b.isOverride,      nullptr);
        // QA-Ea Task 0c (FL pre-roll record + non-destructive clip trim):
        // persist the audio-clip content-start offset so reopened recordings
        // + manual slip-edits survive save/load.  Default 0 on read leaves
        // every pre-Task-0c project unchanged (see deserialize below).
        bNode.setProperty("contentStartSamples",
                          (juce::int64) b.contentStartSamples, nullptr);
        // QA-Ea Task 0c (2026-05-20 - Option A slip-edit): persist sub-bar
        // start ONLY when slip-edit has set it (sentinel > -1e5).  Skipping
        // the property on bar-aligned blocks keeps the XML clean + makes
        // every pre-Task-0c project byte-identical on round-trip.
        if (b.startBeats > -1.0e5f)
            bNode.setProperty("startBeats", b.startBeats, nullptr);
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
        for (const auto& s : mTimeSigChanges)
        {
            juce::ValueTree e("TS");
            e.setProperty("bar", s.bar, nullptr);
            e.setProperty("num", s.num, nullptr);
            e.setProperty("den", s.den, nullptr);
            ts.addChild(e, -1, nullptr);
        }
        root.addChild(ts, -1, nullptr);
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
            lib.addChild(en, -1, nullptr);
        }
        root.addChild(lib, -1, nullptr);
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
    mPatterns.clear();
    mArrangement.clear();
    mAudioLibrary.clear();
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
            for (int i = 0; i < MAX_DRUM_ROWS; ++i)
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

    // ── DrumEnabled + row state ─────────────────────────────────────────────
    {
        auto n = root.getChildWithName("DrumEnabled");
        if (n.isValid())
        {
            juce::String bits = n.getProperty("bits", juce::String()).toString();
            for (int i = 0; i < MAX_DRUM_SOUNDS && i < bits.length(); ++i)
                mDrumEnabled[i] = (bits[i] == '1');
        }
    }
    {
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
        // F-1: missing color attribute → fall back to default (light grey).
        if (pNode.hasProperty("color"))
            p.color = juce::Colour ((juce::uint32) (int) pNode.getProperty ("color"));

        for (const auto child : pNode)
        {
            if (child.hasType("Drum"))
            {
                int d = (int)child.getProperty("slot", 0);
                juce::String bits = child.getProperty("grid", "").toString();
                for (int s = 0; s < bits.length() && s < MAX_STEPS_TOTAL; ++s)
                    if (d >= 0 && d < MAX_DRUM_SOUNDS) p.drumGrid[d][s] = (bits[s] == '1');
            }
            else if (child.hasType("DrumRowToSlot"))
            {
                auto vals = juce::StringArray::fromTokens(
                    child.getProperty("values", juce::String()).toString(), ",", "");
                for (int i = 0; i < MAX_DRUM_ROWS && i < vals.size(); ++i)
                    p.drumRowToSlot[i] = vals[i].getIntValue();
            }
        }

        auto loadPageSeq = [&](const juce::String& tag, PageSequenceData& seq)
        {
            auto seqNode = pNode.getChildWithName(tag);
            if (!seqNode.isValid()) return;
            seq.routing              = (SeqRouting)(int) seqNode.getProperty("routing", 0);
            seq.bars                 = (int)             seqNode.getProperty("bars", DEFAULT_BARS);
            seq.stepsPerBar          = (int)             seqNode.getProperty("spb",  DEFAULT_SPB);
            seq.basicEnv.attack      = (float)(double)   seqNode.getProperty("bEnvA", 0.01);
            seq.basicEnv.hold        = (float)(double)   seqNode.getProperty("bEnvH", 0.0);
            seq.basicEnv.decay       = (float)(double)   seqNode.getProperty("bEnvD", 0.2);
            seq.basicEnv.sustain     = (float)(double)   seqNode.getProperty("bEnvS", 0.7);
            seq.basicEnv.release_    = (float)(double)   seqNode.getProperty("bEnvR", 0.3);
            seq.complexEnv.attack    = (float)(double)   seqNode.getProperty("cEnvA", 0.01);
            seq.complexEnv.hold      = (float)(double)   seqNode.getProperty("cEnvH", 0.0);
            seq.complexEnv.decay     = (float)(double)   seqNode.getProperty("cEnvD", 0.2);
            seq.complexEnv.sustain   = (float)(double)   seqNode.getProperty("cEnvS", 0.7);
            seq.complexEnv.release_  = (float)(double)   seqNode.getProperty("cEnvR", 0.3);
            seq.complexEnv.swing     = (float)(double)   seqNode.getProperty("cSwing",   0.0);
            seq.complexEnv.triplet   = (bool)            seqNode.getProperty("cTriplet", false);
            for (int i = 0; i < seqNode.getNumChildren(); ++i)
            {
                auto rn = seqNode.getChild(i);
                const int r = (int) rn.getProperty("row", -1);
                if (r < 0 || r >= MAX_DRUM_SOUNDS) continue;
                if (rn.hasType("BasicRow"))
                {
                    for (int j = 0; j < rn.getNumChildren(); ++j)
                    {
                        auto st = rn.getChild(j);
                        if (! st.hasType("Step")) continue;
                        const int s = (int) st.getProperty("s", -1);
                        if (s < 0 || s >= MAX_STEPS_TOTAL) continue;
                        seq.basicGrid[r][s].active   = true;
                        seq.basicGrid[r][s].velocity = (float)(double) st.getProperty("v", 0.8);
                        seq.basicGrid[r][s].length   = (float)(double) st.getProperty("l", 1.0);
                        seq.basicGrid[r][s].fxAmount = (float)(double) st.getProperty("fx", 1.0);
                    }
                }
                else if (rn.hasType("ComplexRow"))
                {
                    for (int j = 0; j < rn.getNumChildren(); ++j)
                    {
                        auto st = rn.getChild(j);
                        if (! st.hasType("Step")) continue;
                        const int s = (int) st.getProperty("s", -1);
                        if (s < 0 || s >= MAX_STEPS_TOTAL) continue;
                        seq.complexGrid[r][s].active   = true;
                        seq.complexGrid[r][s].stepType = (StepType)(int) st.getProperty("t", 0);
                        seq.complexGrid[r][s].velocity = (float)(double) st.getProperty("v", 0.8);
                        seq.complexGrid[r][s].fxAmount = (float)(double) st.getProperty("fx", 1.0);
                        seq.complexGrid[r][s].note     = (int)           st.getProperty("n", 60);
                    }
                }
            }
        };
        loadPageSeq("LayerSeq", p.layerSeq);
        loadPageSeq("BassSeq",  p.bassSeq);
        loadPageSeq("DrumSeq",  p.drumSeq);

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
        b.startBar       = (int)             bNode.getProperty("startBar",       0);
        b.lengthBars     = (int)             bNode.getProperty("lengthBars",     4);
        b.lengthBeats    = (float)           bNode.getProperty("lengthBeats",    -1.f);   // 2026-04-24
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
        // QA-Ea Task 0c (2026-05-20): restore sub-bar start.  Sentinel -1e6
        // default = "no sub-bar override; fall back to startBar * 4" -- every
        // pre-Task-0c project keeps its exact bar-aligned position on reopen.
        b.startBeats = (float) (double) bNode.getProperty (
            "startBeats", (double) -1.0e6);
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
        for (int i = 0; i < ts.getNumChildren(); ++i)
        {
            auto e = ts.getChild(i);
            if (! e.hasType("TS")) continue;
            TimeSigChange s;
            s.bar = (int) e.getProperty("bar", 0);
            s.num = (int) e.getProperty("num", 4);
            s.den = (int) e.getProperty("den", 4);
            mTimeSigChanges.push_back(s);
        }
        std::sort(mTimeSigChanges.begin(), mTimeSigChanges.end(),
                  [](const TimeSigChange& a, const TimeSigChange& b) { return a.bar < b.bar; });
    }

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
                (bool)  e.getProperty("libStretch", true)
            });
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
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-track mute / solo (arrangement playback gate)
// ─────────────────────────────────────────────────────────────────────────────
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
