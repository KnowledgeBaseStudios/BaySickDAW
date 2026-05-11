#include "PianoRoll.h"
#include <numeric>
#include <algorithm>
#include <set>
using namespace juce;

// ─────────────────────────────────────────────────────────────────────────────
// Static helpers
// ─────────────────────────────────────────────────────────────────────────────
static bool isBlackKeyStatic(int note)
{
    int pc = note % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

static void sortNotes(std::vector<PianoNote>& notes)
{
    std::sort(notes.begin(), notes.end(),
        [](const PianoNote& a, const PianoNote& b){ return a.startBeat < b.startBeat; });
}

// ─────────────────────────────────────────────────────────────────────────────
// Scale & chord tables
// ─────────────────────────────────────────────────────────────────────────────
struct ScaleDef { const char* name; std::array<bool,12> inKey; };
struct ChordDef { const char* name; std::vector<int>   intervals; };

static const ScaleDef kScaleDefs[] = {
    { "Chromatic",       {1,1,1,1,1,1,1,1,1,1,1,1} },
    { "Major",           {1,0,1,0,1,1,0,1,0,1,0,1} },
    { "Minor",           {1,0,1,1,0,1,0,1,1,0,1,0} },
    { "Dorian",          {1,0,1,1,0,1,0,1,0,1,1,0} },
    { "Phrygian",        {1,1,0,1,0,1,0,1,1,0,1,0} },
    { "Lydian",          {1,0,1,0,1,0,1,1,0,1,0,1} },
    { "Mixolydian",      {1,0,1,0,1,1,0,1,0,1,1,0} },
    { "Locrian",         {1,1,0,1,0,1,1,0,1,0,1,0} },
    { "Harm. Minor",     {1,0,1,1,0,1,0,1,1,0,0,1} },
    { "Mel. Minor",      {1,0,1,1,0,1,0,1,0,1,0,1} },
    { "Pentatonic Maj",  {1,0,1,0,1,0,0,1,0,1,0,0} },
    { "Pentatonic Min",  {1,0,0,1,0,1,0,1,0,0,1,0} },
    { "Blues",           {1,0,0,1,0,1,1,1,0,0,1,0} },
};
static constexpr int kNumScales = (int)(sizeof(kScaleDefs) / sizeof(kScaleDefs[0]));

static const ChordDef kChordDefs[] = {
    { "Major",       {0, 4, 7}          },
    { "Minor",       {0, 3, 7}          },
    { "Dim",         {0, 3, 6}          },
    { "Aug",         {0, 4, 8}          },
    { "Sus2",        {0, 2, 7}          },
    { "Sus4",        {0, 5, 7}          },
    { "Major 7",     {0, 4, 7, 11}      },
    { "Minor 7",     {0, 3, 7, 10}      },
    { "Dom 7",       {0, 4, 7, 10}      },
    { "Half-Dim 7",  {0, 3, 6, 10}      },
    { "Dim 7",       {0, 3, 6,  9}      },
    { "Major 9",     {0, 4, 7, 11, 14}  },
    { "Minor 9",     {0, 3, 7, 10, 14}  },
    { "Add 9",       {0, 4, 7, 14}      },
};
static constexpr int kNumChords = (int)(sizeof(kChordDefs) / sizeof(kChordDefs[0]));

// ─────────────────────────────────────────────────────────────────────────────
// PianoKeyboard
// ─────────────────────────────────────────────────────────────────────────────
PianoKeyboard::PianoKeyboard() { setMouseCursor(MouseCursor::PointingHandCursor); }

bool PianoKeyboard::isBlackKey(int note) { return isBlackKeyStatic(note); }
int  PianoKeyboard::noteToY(int note) const { return (mTopNote - note) * mNoteH; }
int  PianoKeyboard::yToNote(int y)    const { return jlimit(0, 127, mTopNote - (y / mNoteH)); }

void PianoKeyboard::setScrollState(int topNote, int noteH)
{
    mTopNote = topNote; mNoteH = noteH; repaint();
}

void PianoKeyboard::setNoteLabelProvider(std::function<juce::String(int)> provider)
{
    mNoteLabelProvider = std::move(provider);
    repaint();
}

void PianoKeyboard::setAllKeysWhiteMode(bool enabled)
{
    if (mAllKeysWhite == enabled) return;
    mAllKeysWhite = enabled;
    repaint();
}

void PianoKeyboard::mouseMove(const juce::MouseEvent& e)
{
    const int n = yToNote(e.y);
    if (n != mHoverNote)
    {
        mHoverNote = n;
        // Toggling tooltip text forces JUCE's TooltipWindow to refresh.
        setTooltip(getTooltip());
    }
}

juce::String PianoKeyboard::getTooltip()
{
    if (mDrumLabelMode || mHoverNote < 0)
        return {};
    if (mNoteLabelProvider)
    {
        const auto label = mNoteLabelProvider(mHoverNote);
        if (label.isNotEmpty())
            return label;
    }
    return {};
}

void PianoKeyboard::setDrumRowLabels(const std::vector<juce::String>& labels)
{
    mDrumLabelMode = !labels.empty();
    mDrumRowLabels = labels;
    repaint();
}

void PianoKeyboard::paint(Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(VC::Panel);

    int visibleNotes = bounds.getHeight() / mNoteH + 2;
    int loNote = jmax(0, mTopNote - visibleNotes);
    int hiNote = jmin(127, mTopNote + 1);
    int bw = kWidth - 1;

    if (mDrumLabelMode)
    {
        // Drum mode: flat colored rows with slot name labels
        static const Colour kDrumRowBg   (0xff252530);
        static const Colour kDrumRowAlt  (0xff1e1e28);
        static const Colour kDrumRowHl   (0xffee3333);

        for (int note = loNote; note <= hiNote; ++note)
        {
            int y = noteToY(note);
            int labelIdx = mTopNote - note;
            bool highlighted = (note == mPreviewNote);

            g.setColour(highlighted ? kDrumRowHl : (labelIdx % 2 == 0 ? kDrumRowBg : kDrumRowAlt));
            g.fillRect(0, y, bw, mNoteH - 1);
            g.setColour(Colour(0xff3a3a48));
            g.drawHorizontalLine(y + mNoteH - 1, 0, (float)bw);

            if (labelIdx >= 0 && labelIdx < (int)mDrumRowLabels.size() && mNoteH >= 8)
            {
                g.setColour(highlighted ? Colours::white : VC::TextDim);
                g.setFont(Font(jmin(11.f, (float)(mNoteH - 2))));
                g.drawText(mDrumRowLabels[labelIdx], 4, y, bw - 6, mNoteH - 1,
                           Justification::centredLeft, true);
            }
        }
        g.setColour(VC::Accent);
        g.fillRect(bw, 0, 1, bounds.getHeight());
        return;
    }

    for (int note = loNote; note <= hiNote; ++note)
    {
        int y  = noteToY(note);

        // J-7b: in all-keys-white mode (BaySickRustyDrums), paint every row
        // as a full-width white key so engine labels are legible regardless
        // of pitch class.  Skip the black-key strip rendering entirely.
        const bool paintAsBlack = ! mAllKeysWhite && isBlackKey(note);

        if (paintAsBlack)
        {
            g.setColour(note == mPreviewNote ? VC::Highlight.brighter() : Colour(0xff181820));
            g.fillRect(0, y, bw * 7 / 10, mNoteH);
            g.setColour(VC::Accent);
            g.drawRect(0, y, bw * 7 / 10, mNoteH, 1);
        }
        else
        {
            g.setColour(note == mPreviewNote ? VC::Highlight.brighter() : Colour(0xffe8e8f0));
            g.fillRect(0, y, bw, mNoteH - 1);
            g.setColour(Colour(0xffaaaaaa));
            g.drawHorizontalLine(y + mNoteH - 1, 0, (float)bw);

            // J-7b: engine-provided label takes priority over the C-octave default.
            juce::String engineLabel;
            if (mNoteLabelProvider)
                engineLabel = mNoteLabelProvider(note);

            if (engineLabel.isNotEmpty() && mNoteH >= 8)
            {
                g.setColour(Colour(0xff404048));
                g.setFont(Font(jmin(10.f, (float)(mNoteH - 2))));
                g.drawText(engineLabel, 4, y, bw - 6, mNoteH - 1,
                           Justification::centredLeft, true);
            }
            else if (note % 12 == 0)
            {
                g.setColour(VC::TextDim);
                g.setFont(Font(jmin(9, mNoteH - 2)));
                // FL Studio convention: MIDI 60 = C5 (middle C). Drop the -1.
                g.drawText("C" + String(note / 12),
                           bw * 7 / 10 + 1, y, bw * 3 / 10, mNoteH,
                           Justification::centredLeft);
            }
        }
    }
    g.setColour(VC::Accent);
    g.fillRect(kWidth - 1, 0, 1, bounds.getHeight());
}

void PianoKeyboard::mouseDown(const MouseEvent& e)
{
    mPreviewNote = yToNote(e.y);
    if (onNotePreview) onNotePreview(mPreviewNote, true);
    repaint();
}

void PianoKeyboard::mouseUp(const MouseEvent&)
{
    if (mPreviewNote >= 0 && onNotePreview) onNotePreview(mPreviewNote, false);
    mPreviewNote = -1;
    repaint();
}

// 2026-04-21: forward wheel events to the grid so scrolling works over keys too.
void PianoKeyboard::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (onWheel) onWheel(e, wheel);
}

// ─────────────────────────────────────────────────────────────────────────────
// PianoRollGrid - constructor & coordinate helpers
// ─────────────────────────────────────────────────────────────────────────────

// 2026-04-26 (D-7 sub-4): cross-tab clipboard.  Shared by every Piano-Roll
// tab (Layer / Bass / Drum-tab piano roll).  Last copy anywhere wins; survives
// switching tabs, but not app restart.  Drum Kit grid keeps its own
// per-instance clipboard with a different note-ref format.
std::vector<PianoNote> PianoRollGrid::sClipboard;

PianoRollGrid::PianoRollGrid()
{
    setMouseCursor(MouseCursor::CrosshairCursor);
    setWantsKeyboardFocus(true);
}

void PianoRollGrid::setScrollState(float ppb, double beatOff, int topNote,
                                   int noteH, int numBars, int snapDenom)
{
    mPPB = ppb; mBeatOff = beatOff; mTopNote = topNote;
    mNoteH = noteH; mNumBars = numBars; mSnapDenom = snapDenom;
    repaint();
}

void PianoRollGrid::setTimeSignature(int num, int den)
{
    const int n = juce::jlimit (1, 32, num);
    const int d = (den > 0) ? den : 4;
    if (n != mTsNum || d != mTsDen)
    {
        mTsNum = n;
        mTsDen = d;
        repaint();
    }
}

void PianoRollGrid::setData(PianoRollData* data)
{
    if (mData != data)
    {
        mData = data;
        mSelection.clear();
        mPendingLabel = {};
        mPendingBefore.clear();
        if (onUndoRedoStateChanged) onUndoRedoStateChanged();
    }
    repaint();
}

void PianoRollGrid::setPlayheadBeat(double beat) { mPlayhead = beat; repaint(); }

// ─────────────────────────────────────────────────────────────────────────────
// Scale snap
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::setScale(int root, const std::array<bool,12>& relIntervals, bool active)
{
    mScaleRoot   = root;
    mScaleActive = active;
    mScaleInKey.fill(false);
    if (active)
        for (int i = 0; i < 12; ++i)
            if (relIntervals[i]) mScaleInKey[(root + i) % 12] = true;
    repaint();
}

void PianoRollGrid::setStampChord(const std::vector<int>& intervals)
{
    mStampIntervals = intervals;
}

int PianoRollGrid::snapPitchToScale(int midiNote) const
{
    if (!mScaleActive) return midiNote;
    int pc = ((midiNote % 12) + 12) % 12;
    if (mScaleInKey[pc]) return midiNote;   // already in scale
    for (int dist = 1; dist <= 6; ++dist)
    {
        int up   = jlimit(0, 127, midiNote + dist);
        int down = jlimit(0, 127, midiNote - dist);
        if (mScaleInKey[((up   % 12) + 12) % 12]) return up;
        if (mScaleInKey[((down % 12) + 12) % 12]) return down;
    }
    return midiNote;
}

int PianoRollGrid::nextScalePitch(int midiNote, int dir) const
{
    // Step in direction until we land on an in-scale note
    if (!mScaleActive) return jlimit(0, 127, midiNote + dir);
    int step = (dir > 0) ? 1 : -1;
    int n = midiNote + step;
    while (n >= 0 && n <= 127)
    {
        if (mScaleInKey[((n % 12) + 12) % 12]) return n;
        n += step;
    }
    return jlimit(0, 127, midiNote + step);
}

// ─────────────────────────────────────────────────────────────────────────────
// Stamp chord
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::stampChordAt(int x, int y)
{
    if (!mData || mStampIntervals.empty()) return;
    beginEdit("Stamp Chord");
    int    rootNote = snapPitchToScale(yToNote(y));
    double beat     = snapBeat(xToBeat(x));
    double dur      = 4.0 / mSnapDenom;
    std::vector<std::pair<double,int>> newKeys;
    for (int interval : mStampIntervals)
    {
        int mn = jlimit(0, 127, rootNote + interval);
        mData->notes.push_back({ mn, beat, dur, 0.8f, 0.f, 0.f });
        tagLastCreatedNote (mn);   // Phase C §P4.2
        newKeys.push_back({beat, mn});
    }
    sortNotes(mData->notes);
    rebuildSelectionFromKeys(newKeys);
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

double PianoRollGrid::xToBeat(int x)      const { return mBeatOff + (double)x / mPPB; }
int    PianoRollGrid::beatToX(double beat) const { return (int)((beat - mBeatOff) * mPPB); }
int    PianoRollGrid::noteToY(int note)    const { return mNoteYOffset + (mTopNote - note) * mNoteH; }
int    PianoRollGrid::yToNote(int y)       const
{
    int n = mTopNote - (y - mNoteYOffset) / jmax(1, mNoteH);
    if (mIsFixedRange) return jlimit(mFixedRangeBottom, mFixedRangeTop, n);
    return jlimit(0, 127, n);
}

double PianoRollGrid::snapBeat(double beat) const
{
    if (!mSnapEnabled) return beat;
    double snap = 4.0 / mSnapDenom;
    return std::round(beat / snap) * snap;
}

void PianoRollGrid::setSnapEnabled(bool b) { mSnapEnabled = b; }

int PianoRollGrid::noteIndexAtPos(int x, int y) const
{
    if (!mData) return -1;
    int    note = yToNote(y);
    double beat = xToBeat(x);
    // Search in reverse so top-drawn (last) notes take priority
    for (int i = (int)mData->notes.size() - 1; i >= 0; --i)
    {
        const auto& n = mData->notes[i];
        if (n.midiNote == note
            && beat >= n.startBeat
            && beat <  n.startBeat + n.durationBeats)
            return i;
    }
    return -1;
}

PianoNote* PianoRollGrid::noteAtPos(int x, int y) const
{
    int idx = noteIndexAtPos(x, y);
    return (idx >= 0) ? const_cast<PianoNote*>(&mData->notes[idx]) : nullptr;
}

// Returns index of a note whose resize edge (RIGHT by default, LEFT when the
// Ctrl+Alt+Home toggle is on) is within kResizeZone pixels of x.
int PianoRollGrid::noteIndexNearRightEdge(int x, int y) const
{
    if (!mData) return -1;
    int note = yToNote(y);
    for (int i = (int)mData->notes.size() - 1; i >= 0; --i)
    {
        const auto& n = mData->notes[i];
        if (n.midiNote != note) continue;
        const int leftX  = beatToX(n.startBeat);
        const int rightX = beatToX(n.startBeat + n.durationBeats);
        if (mResizeFromLeftEnabled)
        {
            if (x >= leftX - kResizeZone && x <= leftX + kResizeZone
                && x <= rightX)               // don't trigger past the right edge
                return i;
        }
        else
        {
            if (x >= rightX - kResizeZone && x <= rightX + kResizeZone
                && x >= leftX)                // don't trigger on left side
                return i;
        }
    }
    return -1;
}

void PianoRollGrid::eraseAt(int x, int y)
{
    if (!mData) return;
    int    note = yToNote(y);
    double beat = xToBeat(x);
    mData->notes.erase(
        std::remove_if(mData->notes.begin(), mData->notes.end(),
            [&](const PianoNote& n){
                // Phase C §P4.2 (2026-04-24): in DrumGrid mode, a click at
                // row R targets notes whose effective slot maps back to that
                // row (51 - slot == R) - regardless of stored midiNote.  But
                // only notes whose stored pitch is C5 (60) get removed; non-C5
                // notes (created in FullRoll) stay - user must switch to
                // FullRoll to remove them.  In Standard / FullRoll modes the
                // click matches by stored midiNote as before.
                if (mRollMode == RollMode::DrumGrid)
                {
                    const int s = effectiveSlot (n);
                    if (s < 0) return false;
                    if ((51 - s) != note) return false;
                    if (n.midiNote != 60) return false;   // non-C5: leave alone
                    return beat >= n.startBeat
                        && beat <  n.startBeat + n.durationBeats;
                }
                if (mRollMode == RollMode::FullRoll
                    && effectiveSlot (n) != mActiveSlot)
                    return false;   // other-slot notes invisible -> not erasable
                return n.midiNote == note
                    && beat >= n.startBeat
                    && beat <  n.startBeat + n.durationBeats;
            }),
        mData->notes.end());
    mSelection.clear();
}

void PianoRollGrid::updateCursor()
{
    switch (mActiveTool)
    {
        case PRTool::Draw:
        case PRTool::Paint:
        case PRTool::Slice:  setMouseCursor(MouseCursor::CrosshairCursor); break;
        case PRTool::Delete:
        case PRTool::Mute:
        case PRTool::Select:
        case PRTool::Zoom:   setMouseCursor(MouseCursor::NormalCursor);    break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool control
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::setTool(PRTool t) { mActiveTool = t; updateCursor(); }

// ─────────────────────────────────────────────────────────────────────────────
// Undo / Redo
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::beginEdit(const juce::String& label)
{
    if (!mData) return;
    mPendingLabel  = label;
    mPendingBefore = mData->notes;
}

void PianoRollGrid::commitEdit()
{
    if (!mData || mPendingLabel.isEmpty() || !mUndoCtx.isValid()) return;
    auto label  = mPendingLabel;
    auto before = std::move(mPendingBefore);
    auto after  = mData->notes;
    mPendingLabel = {};

    mUndoCtx.perform(
        new PianoRollEditAction(
            label,
            std::move(before), std::move(after),
            [this](const std::vector<PianoNote>& notes) { applySnapshot(notes); }),
        label);
}

void PianoRollGrid::applySnapshot(const std::vector<PianoNote>& notes)
{
    if (!mData) return;
    mData->notes = notes;
    mSelection.clear();
    repaint();
    if (onNotesChanged)         onNotesChanged();
    if (onUndoRedoStateChanged) onUndoRedoStateChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Selection helpers
// ─────────────────────────────────────────────────────────────────────────────
bool PianoRollGrid::isSelected(int idx) const
{
    return std::find(mSelection.begin(), mSelection.end(), idx) != mSelection.end();
}

bool PianoRollGrid::isNoteIndexSelected(int idx) const { return isSelected(idx); }

void PianoRollGrid::toggleSelection(int idx)
{
    auto it = std::find(mSelection.begin(), mSelection.end(), idx);
    if (it != mSelection.end()) mSelection.erase(it);
    else                         mSelection.push_back(idx);
}

void PianoRollGrid::selectAll()
{
    if (!mData) return;
    // 2026-04-26 (D-7 sub-4 follow-up): when a ruler time-range is set,
    // Ctrl+A re-grabs only the notes whose start lies inside [t0, t1).
    // No range -> classic select-everything.
    if (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart)
    {
        const double t0 = jmin (mTimeSelBeatStart, mTimeSelBeatEnd);
        const double t1 = jmax (mTimeSelBeatStart, mTimeSelBeatEnd);
        mSelection.clear();
        for (int i = 0; i < (int) mData->notes.size(); ++i)
        {
            const auto& n = mData->notes[i];
            if (n.startBeat >= t0 && n.startBeat < t1)
                mSelection.push_back (i);
        }
    }
    else
    {
        mSelection.resize(mData->notes.size());
        std::iota(mSelection.begin(), mSelection.end(), 0);
    }
    repaint();
}

void PianoRollGrid::clearSelection() { mSelection.clear(); repaint(); }

void PianoRollGrid::invertSelection()
{
    if (!mData) return;
    std::vector<int> newSel;
    for (int i = 0; i < (int)mData->notes.size(); ++i)
        if (!isSelected(i)) newSel.push_back(i);
    mSelection = std::move(newSel);
    repaint();
}

void PianoRollGrid::finaliseMarquee()
{
    if (!mData) return;
    // Clear selection before applying marquee (unless Ctrl was held to add to existing selection)
    if (!mMarqueeWasCtrl) mSelection.clear();
    auto rect = mMarqueeRect.toFloat();
    for (int i = 0; i < (int)mData->notes.size(); ++i)
    {
        const auto& n = mData->notes[i];
        int x = beatToX(n.startBeat);
        int w = jmax(3, (int)(n.durationBeats * mPPB) - 1);
        int y = noteToY(n.midiNote);
        if (rect.intersects(Rectangle<int>(x, y, w, mNoteH).toFloat()) && !isSelected(i))
            mSelection.push_back(i);
    }
}

void PianoRollGrid::rebuildSelectionFromKeys(const std::vector<std::pair<double,int>>& keys)
{
    mSelection.clear();
    if (!mData) return;
    for (int i = 0; i < (int)mData->notes.size(); ++i)
    {
        const auto& n = mData->notes[i];
        for (const auto& k : keys)
            if (std::abs(n.startBeat - k.first) < 1e-9 && n.midiNote == k.second)
                { mSelection.push_back(i); break; }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Clipboard operations
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::copySelected()
{
    if (!mData || mSelection.empty()) return;
    double minBeat = std::numeric_limits<double>::max();
    for (int idx : mSelection)
        minBeat = jmin(minBeat, mData->notes[idx].startBeat);

    sClipboard.clear();
    for (int idx : mSelection)
    {
        PianoNote n   = mData->notes[idx];
        n.startBeat  -= minBeat;  // make relative to first note
        sClipboard.push_back(n);
    }
}

void PianoRollGrid::pasteClipboard()
{
    if (!mData || sClipboard.empty()) return;
    beginEdit("Paste");
    double pasteAt = snapBeat(mPlayhead >= 0.0 ? mPlayhead : mBeatOff);

    std::vector<std::pair<double,int>> newKeys;
    for (const auto& cn : sClipboard)
    {
        PianoNote n   = cn;
        n.startBeat  += pasteAt;
        newKeys.push_back({n.startBeat, n.midiNote});
        mData->notes.push_back(n);
    }
    sortNotes(mData->notes);
    rebuildSelectionFromKeys(newKeys);
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRollGrid::duplicateSelected()
{
    if (!mData) return;

    // ── Timeline-based duplicate (when a time-selection exists) ───────────
    // The user marked a range on the ruler - copy every note whose start
    // lies inside that range and paste it at +selLen, then advance the
    // time-sel so repeat-Ctrl+B chains forward. Wins over note selection.
    if (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart)
    {
        const double selStart = jmin(mTimeSelBeatStart, mTimeSelBeatEnd);
        const double selEnd   = jmax(mTimeSelBeatStart, mTimeSelBeatEnd);
        const double selLen   = selEnd - selStart;
        if (selLen <= 0.0) return;

        std::vector<PianoNote> newNotes;
        for (const auto& n : mData->notes)
            if (n.startBeat >= selStart && n.startBeat < selEnd)
            {
                PianoNote nn = n;
                nn.startBeat += selLen;
                newNotes.push_back(nn);
            }
        if (newNotes.empty()) return;

        beginEdit("Duplicate (Timeline)");
        std::vector<std::pair<double,int>> newKeys;
        for (auto& nn : newNotes)
        {
            newKeys.push_back({nn.startBeat, nn.midiNote});
            mData->notes.push_back(nn);
        }
        sortNotes(mData->notes);
        rebuildSelectionFromKeys(newKeys);
        // Advance the time-sel so a second Ctrl+B duplicates the new copy.
        mTimeSelBeatStart += selLen;
        mTimeSelBeatEnd   += selLen;
        commitEdit();
        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    // ── Selection-based duplicate (no time-selection) ─────────────────────
    // 2026-04-26 (D-7): no longer falls back to "all notes" when nothing
    // is selected - duplicate now requires either an explicit note
    // selection or a ruler time-selection (handled above).
    if (mSelection.empty()) return;
    std::vector<int> src = mSelection;
    expandForGroups(src);
    beginEdit("Duplicate");

    double minBeat = std::numeric_limits<double>::max();
    double maxEnd  = 0.0;
    for (int idx : src)
    {
        minBeat = jmin(minBeat, mData->notes[idx].startBeat);
        maxEnd  = jmax(maxEnd,  mData->notes[idx].startBeat + mData->notes[idx].durationBeats);
    }
    double offset = maxEnd - minBeat;

    std::vector<std::pair<double,int>> newKeys;
    for (int idx : src)
    {
        PianoNote n   = mData->notes[idx];
        n.startBeat  += offset;
        newKeys.push_back({n.startBeat, n.midiNote});
        mData->notes.push_back(n);
    }
    sortNotes(mData->notes);
    rebuildSelectionFromKeys(newKeys);
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Nudging
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::nudgeSelection(int snapUnits, int semitones)
{
    if (!mData || mSelection.empty()) return;
    beginEdit("Nudge");
    double snap = 4.0 / mSnapDenom;
    std::vector<std::pair<double,int>> keys;
    for (int idx : mSelection)
    {
        auto& n = mData->notes[idx];
        n.startBeat = jmax(0.0, n.startBeat + snap * snapUnits);
        if (semitones != 0)
            n.midiNote = (mScaleActive) ? nextScalePitch(n.midiNote, semitones)
                                        : jlimit(0, 127, n.midiNote + semitones);
        keys.push_back({n.startBeat, n.midiNote});
    }
    sortNotes(mData->notes);
    rebuildSelectionFromKeys(keys);
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRollGrid::nudgeSelectionFine(int pixels, int semitones)
{
    if (!mData || mSelection.empty()) return;
    beginEdit("Nudge Fine");
    double beatPerPixel = (mPPB > 0) ? 1.0 / mPPB : 0.0;
    std::vector<std::pair<double,int>> keys;
    for (int idx : mSelection)
    {
        auto& n = mData->notes[idx];
        n.startBeat = jmax(0.0, n.startBeat + beatPerPixel * pixels);
        n.midiNote  = jlimit(0, 127, n.midiNote + semitones);
        keys.push_back({n.startBeat, n.midiNote});
    }
    sortNotes(mData->notes);
    rebuildSelectionFromKeys(keys);
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slice tool
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::sliceNotesOnLine(Point<int> start, Point<int> end)
{
    if (!mData) return;
    beginEdit("Slice");

    std::vector<PianoNote> added;
    std::vector<int>       toRemove;

    for (int i = 0; i < (int)mData->notes.size(); ++i)
    {
        const auto& n = mData->notes[i];
        int x1 = beatToX(n.startBeat);
        int x2 = beatToX(n.startBeat + n.durationBeats);
        int cy = noteToY(n.midiNote) + mNoteH / 2;   // vertical centre of note row

        // Interpolate line X at this note's Y
        int lineX;
        if (end.y == start.y)
            lineX = (start.x + end.x) / 2;
        else
        {
            float t = (float)(cy - start.y) / (float)(end.y - start.y);
            lineX = (int)(start.x + t * (end.x - start.x));
        }

        // Does the cut land inside the note?
        if (lineX > x1 + 2 && lineX < x2 - 2)
        {
            double sliceBeat = snapBeat(xToBeat(lineX));
            double minDur    = 4.0 / 32.0;  // minimum slice fragment = 1/32 note
            if (sliceBeat > n.startBeat + minDur
                && sliceBeat < n.startBeat + n.durationBeats - minDur)
            {
                PianoNote first  = n;
                first.durationBeats = sliceBeat - n.startBeat;
                PianoNote second = n;
                second.startBeat    = sliceBeat;
                second.durationBeats = (n.startBeat + n.durationBeats) - sliceBeat;
                added.push_back(first);
                added.push_back(second);
                toRemove.push_back(i);
            }
        }
    }

    // Remove originals (reverse to preserve indices)
    std::sort(toRemove.rbegin(), toRemove.rend());
    for (int idx : toRemove)
        mData->notes.erase(mData->notes.begin() + idx);

    for (const auto& n : added)
        mData->notes.push_back(n);

    sortNotes(mData->notes);
    mSelection.clear();
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Ghost notes
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::setGhostData(const std::vector<std::pair<const PianoRollData*, Colour>>& ghosts)
{
    mGhosts = ghosts;
    repaint();
}

void PianoRollGrid::setNoteColor(Colour c)
{
    mNoteColor = c;
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Note grouping
// ─────────────────────────────────────────────────────────────────────────────
int PianoRollGrid::nextGroupId() const
{
    int maxId = -1;
    if (mData)
        for (const auto& n : mData->notes) maxId = jmax(maxId, n.groupId);
    return maxId + 1;
}

void PianoRollGrid::expandForGroups(std::vector<int>& indices) const
{
    if (!mData || indices.empty()) return;
    std::set<int> groupIds;
    for (int idx : indices)
        if (idx >= 0 && idx < (int)mData->notes.size())
        {
            int gid = mData->notes[idx].groupId;
            if (gid >= 0) groupIds.insert(gid);
        }
    if (groupIds.empty()) return;
    std::set<int> idxSet(indices.begin(), indices.end());
    for (int i = 0; i < (int)mData->notes.size(); ++i)
    {
        int gid = mData->notes[i].groupId;
        if (gid >= 0 && groupIds.count(gid) > 0 && idxSet.count(i) == 0)
        {
            indices.push_back(i);
            idxSet.insert(i);
        }
    }
}

void PianoRollGrid::groupSelected()
{
    if (!mData || mSelection.empty()) return;
    beginEdit("Group");
    int newId = nextGroupId();
    for (int idx : mSelection) mData->notes[idx].groupId = newId;
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRollGrid::ungroupSelected()
{
    if (!mData || mSelection.empty()) return;
    beginEdit("Ungroup");
    std::set<int> toUngroup;
    for (int idx : mSelection)
        if (mData->notes[idx].groupId >= 0)
            toUngroup.insert(mData->notes[idx].groupId);
    for (auto& n : mData->notes)
        if (toUngroup.count(n.groupId) > 0) n.groupId = -1;
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

// 2026-04-26 (D-1): Alt+M / Alt+Shift+M for selection-wide mute / unmute.
// Toggles `note.muted` on every selected note (group-expanded so muting one
// member of a group also mutes its mates).  Undoable.
void PianoRollGrid::muteSelectedNotes(bool mute)
{
    if (!mData || mSelection.empty()) return;
    beginEdit(mute ? "Mute" : "Unmute");
    std::vector<int> targets = mSelection;
    expandForGroups(targets);
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    for (int idx : targets)
        if (idx >= 0 && idx < (int) mData->notes.size())
            mData->notes[idx].muted = mute;
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Keyboard shortcuts
// ─────────────────────────────────────────────────────────────────────────────
bool PianoRollGrid::keyPressed(const KeyPress& key)
{
    const bool ctrl  = key.getModifiers().isCtrlDown();
    const bool shift = key.getModifiers().isShiftDown();
    const bool alt   = key.getModifiers().isAltDown();

    // ── Tool shortcuts (no modifiers) ─────────────────────────────────────
    if (!ctrl && !alt && !shift)
    {
        // 2026-04-26 (D-7): bare M = toggle keyboard column visibility.
        // Container owns the layout state; we just fire a callback.
        if (key.getKeyCode() == 'M' || key.getKeyCode() == 'm')
        {
            if (onToggleKeyboard) onToggleKeyboard();
            return true;
        }

        struct { int code; PRTool tool; } toolKeys[] = {
            { 'P', PRTool::Draw   }, { 'B', PRTool::Paint  },
            { 'D', PRTool::Delete }, { 'T', PRTool::Mute   },
            { 'C', PRTool::Slice  }, { 'E', PRTool::Select },
            { 'Z', PRTool::Zoom   },   // 2026-04-26 (B-4): bare Z (was Shift+Z).
        };
        for (auto& tk : toolKeys)
        {
            if (key.getKeyCode() == tk.code || key.getKeyCode() == (tk.code + 32))
            {
                mActiveTool = tk.tool; updateCursor();
                if (onToolChanged) onToolChanged(mActiveTool);
                return true;
            }
        }

        // S = cycle note type (Standard→Slide→Portamento) on selection, or for new notes
        if (key.getKeyCode() == 'S' || key.getKeyCode() == 's')
        {
            auto cycleType = [](NoteType t) {
                switch (t) {
                    case NoteType::Standard:   return NoteType::Slide;
                    case NoteType::Slide:      return NoteType::Portamento;
                    case NoteType::Portamento: return NoteType::Standard;
                }
                return NoteType::Standard;
            };
            if (!mSelection.empty() && mData)
            {
                beginEdit("Change Type");
                for (int idx : mSelection) mData->notes[idx].type = cycleType(mData->notes[idx].type);
                commitEdit();
                if (onNotesChanged) onNotesChanged();
                repaint();
            }
            else
            {
                mNewNoteType = cycleType(mNewNoteType);
            }
            return true;
        }
    }

    // ── Shift+G = group, Alt+G = ungroup (2026-04-26 B-4 rebind) ─────────
    {
        const int kcG = key.getKeyCode();
        const bool isG = (kcG == 'G' || kcG == 'g');
        if (isG && !ctrl)
        {
            if (shift && !alt) { groupSelected();   return true; }
            if (alt && !shift) { ungroupSelected(); return true; }
        }
    }

    // ── Ctrl shortcuts ────────────────────────────────────────────────────
    // 2026-04-26 (B-5): Ctrl+Z / Ctrl+Alt+Z migrated to global BSCommands -
    // page-local Z handlers removed.  Remaining Ctrl shortcuts stay local.
    if (ctrl && !alt)
    {
        if (!shift && (key.getKeyCode() == 'A' || key.getKeyCode() == 'a'))
            { selectAll(); return true; }
        if (!shift && (key.getKeyCode() == 'C' || key.getKeyCode() == 'c'))
            { copySelected(); return true; }
        if (!shift && (key.getKeyCode() == 'V' || key.getKeyCode() == 'v'))
            { pasteClipboard(); return true; }
        if (!shift && (key.getKeyCode() == 'B' || key.getKeyCode() == 'b'))
            { duplicateSelected(); return true; }
        if (!shift && (key.getKeyCode() == 'G' || key.getKeyCode() == 'g'))
            { toolGlue(); return true; }
        // 2026-04-26 (D-7): Ctrl shortcut bundle.
        if (!shift && (key.getKeyCode() == 'Q' || key.getKeyCode() == 'q'))
            { quickQuantizeQuarter(); return true; }
        if (!shift && (key.getKeyCode() == 'U' || key.getKeyCode() == 'u'))
            { if (!mSelection.empty()) toolChop(4); return true; }   // quick chop into 4 - selection only
        if (!shift && (key.getKeyCode() == 'L' || key.getKeyCode() == 'l'))
            { quickLegato(); return true; }
        if (!shift && key.isKeyCode(KeyPress::upKey))
            { transposeSelection(+12); return true; }   // Ctrl+Up = +octave
        if (!shift && key.isKeyCode(KeyPress::downKey))
            { transposeSelection(-12); return true; }   // Ctrl+Down = -octave
        // 2026-04-26 (D-7 sub-3): Ctrl+Left/Right shifts the ruler time-
        // selection box (not contents) by its own length.
        if (!shift && key.isKeyCode(KeyPress::leftKey))
            { shiftTimeSelectionLeft();  return true; }
        if (!shift && key.isKeyCode(KeyPress::rightKey))
            { shiftTimeSelectionRight(); return true; }
        // 2026-04-26 (D-7): use isKeyCode(...) - `key == KeyPress::deleteKey`
        // resolves to the operator==(KeyPress&) overload via implicit int→
        // KeyPress conversion, which compares MODIFIERS too and never matches
        // a press with Ctrl held.  isKeyCode is int-only and correct.
        if (!shift && (key.isKeyCode(KeyPress::deleteKey) || key.isKeyCode(KeyPress::backspaceKey)))
            { deleteTimeRegion(); return true; }
    }

    // ── Ctrl+Alt+Home = flip note resize-edge (right ↔ left) ─────────────
    if (ctrl && alt && !shift && key.isKeyCode(KeyPress::homeKey))
    {
        toggleResizeFromLeftMode();
        return true;
    }

    // ── Alt+letter = tools menu shortcuts ────────────────────────────────
    if (alt && !ctrl && !shift)
    {
        int kc = key.getKeyCode() | 32; // to lowercase
        if      (kc == 'q') { toolQuantize();      return true; }
        else if (kc == 's') { toolStrum();         return true; }
        else if (kc == 'a') { toolArpeggiate();    return true; }
        else if (kc == 'u') { toolChop(4);         return true; }  // default: chop into 4
        else if (kc == 'l') { toolArticulate();    return true; }
        else if (kc == 'r') { toolRandomize();     return true; }
        else if (kc == 'p') { toolGenerateChords();return true; }
        else if (kc == 'm') { muteSelectedNotes(true);  return true; }   // D-1
        else if (kc == 'f') { flamSelected();      return true; }        // D-7
        else if (kc == 'x') { scaleSelectionLevels(); return true; }     // D-7
    }
    // Alt+Shift+M = unmute selection (D-1).
    if (alt && shift && !ctrl && (key.getKeyCode() | 32) == 'm')
    {
        muteSelectedNotes(false);
        return true;
    }

    // ── Shift+I = invert selection ────────────────────────────────────────
    if (shift && !ctrl && !alt && (key.getKeyCode() == 'I' || key.getKeyCode() == 'i'))
        { invertSelection(); return true; }

    // ── Delete / Backspace = erase selected notes ─────────────────────────
    if (!ctrl && (key == KeyPress::deleteKey || key == KeyPress::backspaceKey))
    {
        if (!mSelection.empty() && mData)
        {
            beginEdit("Delete");
            std::vector<int> toDelete = mSelection;
            expandForGroups(toDelete);
            std::sort(toDelete.rbegin(), toDelete.rend());
            toDelete.erase(std::unique(toDelete.begin(), toDelete.end()), toDelete.end());
            for (int idx : toDelete)
                mData->notes.erase(mData->notes.begin() + idx);
            mSelection.clear();
            commitEdit();
            if (onNotesChanged) onNotesChanged();
            repaint();
        }
        return true;
    }

    // ── Shift+Arrows = nudge by snap unit (Shift+Z dropped in B-4 - bare Z) ──
    if (shift && !ctrl && !alt)
    {
        if (key.isKeyCode(KeyPress::leftKey))  { nudgeSelection(-1,  0); return true; }
        if (key.isKeyCode(KeyPress::rightKey)) { nudgeSelection(+1,  0); return true; }
        if (key.isKeyCode(KeyPress::upKey))    { nudgeSelection( 0, +1); return true; }
        if (key.isKeyCode(KeyPress::downKey))  { nudgeSelection( 0, -1); return true; }
    }

    // ── Alt+Arrows = nudge by one pixel (fine) ────────────────────────────
    if (alt && !ctrl && !shift)
    {
        if (key.isKeyCode(KeyPress::leftKey))  { nudgeSelectionFine(-1,  0); return true; }
        if (key.isKeyCode(KeyPress::rightKey)) { nudgeSelectionFine(+1,  0); return true; }
        if (key.isKeyCode(KeyPress::upKey))    { nudgeSelectionFine( 0, +1); return true; }
        if (key.isKeyCode(KeyPress::downKey))  { nudgeSelectionFine( 0, -1); return true; }
    }

    // ── PgUp / PgDn = zoom in / out when Zoom tool is active ─────────────
    // 2026-04-26 (B-4): pairs with the bare-Z Zoom tool shortcut.  Outside
    // Zoom mode the keys are ignored (host viewport handles scroll if any).
    if (!ctrl && !shift && !alt
        && (key == KeyPress::pageUpKey || key == KeyPress::pageDownKey)
        && mActiveTool == PRTool::Zoom)
    {
        const float factor = (key == KeyPress::pageUpKey) ? 1.15f : (1.f / 1.15f);
        if (onZoom) onZoom(mPPB * factor - mPPB);
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse - hover cursor
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::mouseMove(const MouseEvent& e)
{
    // Stamp tool: track position for ghost-chord preview
    if (mActiveTool == PRTool::Stamp)
    {
        mStampPos = e.getPosition();
        repaint();
        return;
    }
    if (mActiveTool == PRTool::Draw || mActiveTool == PRTool::Select)
    {
        if (noteIndexNearRightEdge(e.x, e.y) >= 0)
            setMouseCursor(MouseCursor::LeftRightResizeCursor);
        else if (mActiveTool == PRTool::Select && noteIndexAtPos(e.x, e.y) >= 0)
            setMouseCursor(MouseCursor::DraggingHandCursor);
        else
            updateCursor();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse - button down
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::mouseDown(const MouseEvent& e)
{
    if (!mData) return;
    grabKeyboardFocus();

    // ── Below MIDI 0 guard (2026-04-21) ───────────────────────────────────
    // Reject clicks in the sliver below the last visible note row so a
    // user can't "place a note below C0" where none can exist.
    {
        const int bottomY = mNoteYOffset + (mTopNote + 1) * mNoteH;
        if (!mIsFixedRange && e.y >= bottomY) return;
    }

    // ── 2026-04-26 (D-7 sub-4): click-outside-time-range clears state ────
    // When a ruler time-range is set, a click whose x falls OUTSIDE [t0, t1)
    // clears both the range and the auto-populated mSelection BEFORE the
    // rest of mouseDown runs.  Inside the range = preserve.  This applies
    // to grid + ruler clicks alike - including a fresh Ctrl+drag-on-ruler
    // outside the old range, which then starts a new range from scratch.
    // Right-button presses are skipped (right-click context menus shouldn't
    // wipe state).
    if (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart
        && !e.mods.isRightButtonDown())
    {
        const double t0 = jmin (mTimeSelBeatStart, mTimeSelBeatEnd);
        const double t1 = jmax (mTimeSelBeatStart, mTimeSelBeatEnd);
        const double clickBeat = xToBeat (e.x);
        if (clickBeat < t0 || clickBeat >= t1)
        {
            mTimeSelBeatStart = mTimeSelBeatEnd = -1.0;
            mTimeSelDragging  = false;
            mSelection.clear();
        }
    }

    // ── Ruler zone: Ctrl+drag = time selection; bare click = seek ────────
    if (e.y < kRulerH)
    {
        if (e.mods.isCtrlDown() && !e.mods.isRightButtonDown())
        {
            double beat          = snapBeat(xToBeat(e.x));
            mTimeSelBeatAnchor   = beat;
            mTimeSelBeatStart    = beat;
            mTimeSelBeatEnd      = beat;
            mTimeSelDragging     = true;
            repaint();
        }
        else if (!e.mods.isRightButtonDown())
        {
            if (onSeek) onSeek(xToBeat(e.x));
        }
        return;
    }

    // ── Right-button: erase in draw/paint/delete tools ────────────────────
    if (e.mods.isRightButtonDown())
    {
        if (mActiveTool == PRTool::Zoom)
        {
            if (onZoomToggle) onZoomToggle();
            return;
        }
        if (mActiveTool == PRTool::Draw || mActiveTool == PRTool::Paint
         || mActiveTool == PRTool::Delete)
        {
            beginEdit("Erase");
            mErasing = true;
            eraseAt(e.x, e.y);
            if (onNotesChanged) onNotesChanged();
            repaint();
        }
        return;
    }

    // 2026-04-26 (B-4): Ctrl+drag from empty area = marquee selection
    // regardless of active tool.  Click on a note still falls through so
    // tool-specific Ctrl+click semantics (Draw tool's toggle-selection) work.
    if (e.mods.isCtrlDown() && noteIndexAtPos(e.x, e.y) < 0)
    {
        mMarqueeWasCtrl = true;       // Ctrl held = additive (Shift-style)
        mMarqueeActive  = true;
        mMarqueeStart   = e.getPosition();
        mMarqueeRect    = {};
        repaint();
        return;
    }

    // ── Left-button: dispatch to active tool ─────────────────────────────
    switch (mActiveTool)
    {
        // ── DRAW ──────────────────────────────────────────────────────────
        case PRTool::Draw:
        {
            if (e.mods.isCtrlDown())
            {
                int idx = noteIndexAtPos(e.x, e.y);
                if (idx >= 0) toggleSelection(idx);
                else          clearSelection();
                repaint();
                break;
            }

            // Resize check (right edge - or left edge if Ctrl+Alt+Home toggle on)
            int ri = noteIndexNearRightEdge(e.x, e.y);
            if (ri >= 0)
            {
                beginEdit("Resize");
                mResizing        = true;
                mResizingFromLeft = mResizeFromLeftEnabled;
                mResizeNoteIdx   = ri;
                mResizeOrigDur   = mData->notes[ri].durationBeats;
                mResizeOrigStart = mData->notes[ri].startBeat;
                break;
            }

            // Move check (click on existing note)
            int ni = noteIndexAtPos(e.x, e.y);
            if (ni >= 0)
            {
                // 2026-04-26 (D-7): FL-style click memory - clicking on an
                // existing note remembers its duration + type so the next
                // click-place uses them.  Drag-to-place still wins.
                mClickMemoryDur  = mData->notes[ni].durationBeats;
                mClickMemoryType = mData->notes[ni].type;

                beginEdit("Move");
                mMoving = true;
                mMoveDragOrigin = e.getPosition();
                if (isSelected(ni))
                    mMoveIndices = mSelection;
                else
                {
                    clearSelection();
                    mSelection.push_back(ni);
                    mMoveIndices = { ni };
                }
                expandForGroups(mMoveIndices);   // pull in grouped mates
                mMoveOrigBeats.clear(); mMoveOrigNotes.clear();
                for (int idx : mMoveIndices)
                {
                    mMoveOrigBeats.push_back(mData->notes[idx].startBeat);
                    mMoveOrigNotes.push_back(mData->notes[idx].midiNote);
                }
                if (! mMoveOrigNotes.empty()) triggerAudition(mMoveOrigNotes[0]);
                repaint();
                break;
            }

            // Empty space: draw new note
            beginEdit("Draw");
            mDrawing         = true;
            mDrawHasDragged  = false;
            mDrawNote        = snapPitchToScale(yToNote(e.y));
            mDrawStart       = snapBeat(xToBeat(e.x));
            // 2026-04-26 (D-7): initial preview length is the click memory
            // (defaults to 1/16 = 0.25 beat).  If the user drags, mouseDrag
            // overrides mDrawEnd and sets mDrawHasDragged so mouseUp uses the
            // dragged length instead.
            mDrawEnd         = mDrawStart + mClickMemoryDur;
            triggerAudition(mDrawNote);
            repaint();
            break;
        }

        // ── PAINT ─────────────────────────────────────────────────────────
        case PRTool::Paint:
        {
            beginEdit("Paint");
            mDrawing   = true;
            mDrawNote  = snapPitchToScale(yToNote(e.y));
            mDrawStart = snapBeat(xToBeat(e.x));
            // Default placement duration: 1/16 note (0.25 beat).
            mDrawEnd   = mDrawStart + 0.25;
            // Immediately commit one note
            double dur = 0.25;
            mData->notes.push_back({ mDrawNote, mDrawStart, dur, 0.8f, 0.f, 0.f, mNewNoteType });
            tagLastCreatedNote (mDrawNote);   // Phase C §P4.2
            sortNotes(mData->notes);
            if (onNotesChanged) onNotesChanged();
            repaint();
            break;
        }

        // ── DELETE ────────────────────────────────────────────────────────
        case PRTool::Delete:
        {
            beginEdit("Delete");
            mErasing = true;
            eraseAt(e.x, e.y);
            if (onNotesChanged) onNotesChanged();
            repaint();
            break;
        }

        // ── MUTE ──────────────────────────────────────────────────────────
        case PRTool::Mute:
        {
            int ni2 = noteIndexAtPos(e.x, e.y);
            if (ni2 >= 0)
            {
                beginEdit("Mute");
                std::vector<int> targets = { ni2 };
                expandForGroups(targets);
                bool newMuted = !mData->notes[ni2].muted;
                for (int idx : targets) mData->notes[idx].muted = newMuted;
                commitEdit();
                if (onNotesChanged) onNotesChanged();
                repaint();
            }
            break;
        }

        // ── SLICE ─────────────────────────────────────────────────────────
        case PRTool::Slice:
        {
            mSlicing = true;
            // 2026-04-26: snap the slice line's X to the nearest grid line so
            // the user can SEE where the cut will land.  Alt bypasses snap
            // (matches the rest of the piano roll's snap-bypass convention).
            const auto p = e.getPosition();
            const bool noSnap = e.mods.isAltDown();
            const int snappedX = noSnap ? p.x : beatToX(snapBeat(xToBeat(p.x)));
            mSliceStart = { snappedX, p.y };
            mSliceEnd   = mSliceStart;
            repaint();
            break;
        }

        // ── SELECT ────────────────────────────────────────────────────────
        case PRTool::Select:
        {
            // Resize check
            int ri = noteIndexNearRightEdge(e.x, e.y);
            if (ri >= 0)
            {
                beginEdit("Resize");
                mResizing        = true;
                mResizingFromLeft = mResizeFromLeftEnabled;
                mResizeNoteIdx   = ri;
                mResizeOrigDur   = mData->notes[ri].durationBeats;
                mResizeOrigStart = mData->notes[ri].startBeat;
                break;
            }

            int idx = noteIndexAtPos(e.x, e.y);
            if (idx >= 0)
            {
                // 2026-04-26 (D-7): click memory carries from the Select tool
                // too - clicking a note here also primes the next Draw-tool
                // click-place with that note's length + type.
                mClickMemoryDur  = mData->notes[idx].durationBeats;
                mClickMemoryType = mData->notes[idx].type;

                if (!e.mods.isCtrlDown() && !isSelected(idx))
                    clearSelection();
                if (!isSelected(idx)) mSelection.push_back(idx);

                beginEdit("Move");
                mMoving = true;
                mMoveDragOrigin = e.getPosition();
                mMoveIndices    = mSelection;
                expandForGroups(mMoveIndices);   // pull in grouped mates
                mMoveOrigBeats.clear(); mMoveOrigNotes.clear();
                for (int mi : mMoveIndices)
                {
                    mMoveOrigBeats.push_back(mData->notes[mi].startBeat);
                    mMoveOrigNotes.push_back(mData->notes[mi].midiNote);
                }
                if (! mMoveOrigNotes.empty()) triggerAudition(mMoveOrigNotes[0]);
            }
            else
            {
                // Don't clearSelection here - do it in finaliseMarquee so that
                // a spurious extra mouseDown can't wipe out the completed selection.
                mMarqueeWasCtrl = e.mods.isCtrlDown();
                mMarqueeActive  = true;
                mMarqueeStart   = e.getPosition();
                mMarqueeRect    = {};
            }
            repaint();
            break;
        }

        // ── ZOOM ──────────────────────────────────────────────────────────
        case PRTool::Zoom:
        {
            mZoomRectActive = true;
            mZoomRectStart  = e.getPosition();
            mZoomRect       = {};
            repaint();
            break;
        }

        // ── STAMP ─────────────────────────────────────────────────────────
        case PRTool::Stamp:
        {
            stampChordAt(e.x, e.y);
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse - drag
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::mouseDrag(const MouseEvent& e)
{
    // ── Ruler time selection drag ─────────────────────────────────────────
    if (mTimeSelDragging)
    {
        double beat          = snapBeat(xToBeat(e.x));
        mTimeSelBeatStart    = jmin(mTimeSelBeatAnchor, beat);
        mTimeSelBeatEnd      = jmax(mTimeSelBeatAnchor, beat);
        repaint();
        return;
    }

    if (!mData) return;

    // Erase drag
    if (mErasing)
    {
        eraseAt(e.x, e.y);
        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    // Paint drag: add a note at each new snap position
    if (mDrawing && mActiveTool == PRTool::Paint)
    {
        double curBeat = snapBeat(xToBeat(e.x));
        int    curNote = snapPitchToScale(yToNote(e.y));
        bool   exists  = false;
        for (const auto& n : mData->notes)
            if (n.midiNote == curNote && std::abs(n.startBeat - curBeat) < 1e-9)
                { exists = true; break; }
        if (!exists)
        {
            mData->notes.push_back({ curNote, curBeat, 4.0 / mSnapDenom, 0.8f, 0.f, 0.f, mNewNoteType });
            tagLastCreatedNote (curNote);   // Phase C §P4.2
            sortNotes(mData->notes);
            if (onNotesChanged) onNotesChanged();
            repaint();
        }
        return;
    }

    // Draw drag: extend note duration preview
    if (mDrawing)
    {
        double endBeat = snapBeat(xToBeat(e.x));
        if (e.mods.isAltDown()) endBeat = xToBeat(e.x);   // Alt = free (no snap)
        if (endBeat > mDrawStart)
        {
            // 2026-04-26 (D-7): only flip the dragged flag when the new end
            // beat differs from the click-memory preview length.  A pure
            // click (no horizontal travel) leaves mDrawHasDragged false so
            // mouseUp uses the click-memory length on commit.
            if (std::abs(endBeat - (mDrawStart + mClickMemoryDur)) > 1.0e-6)
                mDrawHasDragged = true;
            mDrawEnd = endBeat;
        }
        repaint();
        return;
    }

    // Move drag
    if (mMoving && !mMoveIndices.empty())
    {
        const bool shiftHeld = e.mods.isShiftDown();  // lock pitch
        const bool ctrlHeld  = e.mods.isCtrlDown();   // lock timing
        const bool altHeld   = e.mods.isAltDown();    // bypass snap

        double beatDelta = xToBeat(e.x) - xToBeat(mMoveDragOrigin.x);
        int    noteDelta = (mMoveDragOrigin.y - e.y) / mNoteH;

        if (shiftHeld) noteDelta = 0;
        if (ctrlHeld)  beatDelta = 0.0;

        // Snap the delta using the first note as reference
        if (!altHeld && !ctrlHeld)
        {
            double snappedFirst = snapBeat(mMoveOrigBeats[0] + beatDelta);
            beatDelta = snappedFirst - mMoveOrigBeats[0];
        }

        for (int i = 0; i < (int)mMoveIndices.size(); ++i)
        {
            auto& n        = mData->notes[mMoveIndices[i]];
            n.startBeat    = jmax(0.0, mMoveOrigBeats[i] + beatDelta);
            int rawNote    = jlimit(0, 127, mMoveOrigNotes[i] + noteDelta);
            int newNote    = (mScaleActive && !shiftHeld) ? snapPitchToScale(rawNote) : rawNote;
            // Audition the primary note when its pitch changes during drag
            // (mouse-button-held: triggerAudition releases prior held first).
            if (i == 0 && newNote != n.midiNote)
                triggerAudition(newNote);
            n.midiNote = newNote;
        }
        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    // Resize drag
    if (mResizing && mResizeNoteIdx >= 0 && mResizeNoteIdx < (int)mData->notes.size())
    {
        auto& n = mData->notes[mResizeNoteIdx];
        const double minDur = 4.0 / 32.0;  // always allow resize back to 1/32 regardless of snap
        if (mResizingFromLeft)
        {
            // 2026-04-26 (D-7): drag the LEFT edge - keep the original right
            // edge fixed and adjust startBeat + durationBeats together.
            const double origEnd  = mResizeOrigStart + mResizeOrigDur;
            const double rawStart = e.mods.isAltDown() ? xToBeat(e.x) : snapBeat(xToBeat(e.x));
            const double newStart = jmax(0.0, jmin(rawStart, origEnd - minDur));
            n.startBeat     = newStart;
            n.durationBeats = origEnd - newStart;
        }
        else
        {
            double newEnd = e.mods.isAltDown() ? xToBeat(e.x) : snapBeat(xToBeat(e.x));
            n.durationBeats = jmax(minDur, newEnd - n.startBeat);
        }
        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    // Marquee drag
    if (mMarqueeActive)
    {
        mMarqueeRect = Rectangle<int>::leftTopRightBottom(
            jmin(mMarqueeStart.x, e.x), jmin(mMarqueeStart.y, e.y),
            jmax(mMarqueeStart.x, e.x), jmax(mMarqueeStart.y, e.y));
        repaint();
        return;
    }

    // Slice drag - snap the moving endpoint's X to grid (Alt bypasses snap).
    if (mSlicing)
    {
        const auto p = e.getPosition();
        const bool noSnap = e.mods.isAltDown();
        const int snappedX = noSnap ? p.x : beatToX(snapBeat(xToBeat(p.x)));
        mSliceEnd = { snappedX, p.y };
        repaint();
        return;
    }

    // Zoom-rect drag
    if (mZoomRectActive)
    {
        mZoomRect = Rectangle<int>::leftTopRightBottom(
            jmin(mZoomRectStart.x, e.x), jmin(mZoomRectStart.y, e.y),
            jmax(mZoomRectStart.x, e.x), jmax(mZoomRectStart.y, e.y));
        repaint();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse - button up
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::mouseUp(const MouseEvent&)
{
    // ── Ruler time selection release ──────────────────────────────────────
    if (mTimeSelDragging)
    {
        mTimeSelDragging = false;
        // Collapse tiny selections
        if (mTimeSelBeatEnd - mTimeSelBeatStart < 0.01)
        {
            mTimeSelBeatStart = mTimeSelBeatEnd = -1.0;
        }
        else if (mData)
        {
            // 2026-04-26 (D-7): auto-select every note whose start lies inside
            // the ruler range so subsequent ops (Delete, Ctrl+Q, Ctrl+L, etc.)
            // act on those notes immediately - matches user expectation that
            // "what's inside the time selection IS the selection".  The range
            // itself stays set so Ctrl+Delete (delete time + close gap) and
            // Ctrl+B Duplicate Timeline still target the time span.
            const double t0 = jmin(mTimeSelBeatStart, mTimeSelBeatEnd);
            const double t1 = jmax(mTimeSelBeatStart, mTimeSelBeatEnd);
            mSelection.clear();
            for (int i = 0; i < (int)mData->notes.size(); ++i)
            {
                const auto& n = mData->notes[i];
                if (n.startBeat >= t0 && n.startBeat < t1)
                    mSelection.push_back(i);
            }
        }
        repaint();
        return;
    }

    if (!mData) { mDrawing = mErasing = mMoving = mResizing = false; return; }

    // Commit drawn note (Draw tool)
    // Note: audition is mouse-held and ends below in releaseAudition() - no
    // one-shot fire here, otherwise the held noteOff would chase a brief
    // re-trigger and re-cut the voice mid-note.
    if (mDrawing && mDrawNote >= 0 && mActiveTool == PRTool::Draw)
    {
        // 2026-04-26 (D-7): click-memory length / type win for click-only
        // placement; drag-to-place uses the dragged length and the current
        // mNewNoteType (S-cycled) - drag-to-place doesn't override the
        // memory because the user explicitly chose a custom length.
        const double dur = mDrawHasDragged
            ? jmax(4.0 / 32.0, mDrawEnd - mDrawStart)
            : mClickMemoryDur;
        const NoteType nt = mDrawHasDragged ? mNewNoteType : mClickMemoryType;

        // 2026-04-26 (D-7 sub-4 EC-1, revised): drawing a new note INSIDE
        // an active ruler time-range clears the existing note selection
        // (user is starting fresh inside the range) but PRESERVES the
        // ruler range itself.  The new note is NOT added to the selection.
        const bool drawnInsideRange =
            (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart
             && mDrawStart >= mTimeSelBeatStart
             && mDrawStart <  mTimeSelBeatEnd);
        if (drawnInsideRange) mSelection.clear();

        mData->notes.push_back({ mDrawNote, mDrawStart, dur, 0.8f, 0.f, 0.f, nt });
        tagLastCreatedNote (mDrawNote);   // Phase C §P4.2: slotIndex tagging
        sortNotes(mData->notes);
        commitEdit();
        if (onNotesChanged) onNotesChanged();
        mDrawHasDragged = false;
    }

    // Finalise paint drag
    if (mDrawing && mActiveTool == PRTool::Paint)
    {
        commitEdit();
    }

    // Finalise erase drag
    if (mErasing)
    {
        commitEdit();
    }

    // Finalise move: sort + rebuild selection
    if (mMoving && mData)
    {
        std::vector<std::pair<double,int>> keys;
        for (int idx : mMoveIndices)
            keys.push_back({mData->notes[idx].startBeat, mData->notes[idx].midiNote});
        sortNotes(mData->notes);
        rebuildSelectionFromKeys(keys);
        commitEdit();
        if (onNotesChanged) onNotesChanged();
        mMoveIndices.clear(); mMoveOrigBeats.clear(); mMoveOrigNotes.clear();
    }

    // Finalise resize: sort
    if (mResizing && mData)
    {
        sortNotes(mData->notes);
        commitEdit();
        if (onNotesChanged) onNotesChanged();
        mResizeNoteIdx = -1;
    }

    // Finalise marquee
    if (mMarqueeActive)
    {
        finaliseMarquee();
        mMarqueeActive = false; mMarqueeRect = {};
    }

    // Finalise slice
    if (mSlicing)
    {
        if (mSliceStart != mSliceEnd)
            sliceNotesOnLine(mSliceStart, mSliceEnd);
        mSlicing = false;
    }

    // Finalise zoom-rect
    if (mZoomRectActive)
    {
        // Only do rect-zoom when the drag was large enough; otherwise treat as click=zoom-in
        if (mZoomRect.getWidth() > 8 && mZoomRect.getHeight() > 8 && mPPB > 0)
        {
            double beatStart = xToBeat(mZoomRect.getX());
            double beatEnd   = xToBeat(mZoomRect.getRight());
            if (beatEnd > beatStart && onZoomTo)
                onZoomTo(beatStart, beatEnd);
        }
        else
        {
            if (onZoom) onZoom(mPPB * 0.3f); // click = zoom in ~30%
        }
        mZoomRectActive = false; mZoomRect = {};
    }

    mDrawing  = false;
    mErasing  = false;
    mMoving   = false;
    mResizing = false;
    mDrawNote = -1;
    releaseAudition();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Audition helpers - mouse-button-held
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::triggerAudition (int midiNote)
{
    if (midiNote < 0 || midiNote > 127) return;
    if (mAuditionHeldNote >= 0 && onNoteAuditionOff)
        onNoteAuditionOff (mAuditionHeldNote);
    mAuditionHeldNote = midiNote;
    if (onNoteAuditionOn) onNoteAuditionOn (midiNote);
}

void PianoRollGrid::releaseAudition()
{
    if (mAuditionHeldNote >= 0 && onNoteAuditionOff)
        onNoteAuditionOff (mAuditionHeldNote);
    mAuditionHeldNote = -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse - wheel
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
    // RMB + wheel = cycle tools (only when not mid-erase)
    if (e.mods.isRightButtonDown() && !mErasing)
    {
        static const PRTool tools[] = {
            PRTool::Draw, PRTool::Paint, PRTool::Delete,
            PRTool::Mute, PRTool::Slice, PRTool::Select, PRTool::Zoom, PRTool::Stamp };
        int cur = 0;
        for (int i = 0; i < 8; ++i) if (tools[i] == mActiveTool) { cur = i; break; }
        cur = (cur + (wheel.deltaY > 0.f ? 7 : 1)) % 8;
        mActiveTool = tools[cur]; updateCursor();
        if (onToolChanged) onToolChanged(mActiveTool);
        return;
    }

    // Alt + wheel = vertical zoom (note height)
    if (e.mods.isAltDown() && !e.mods.isCtrlDown())
    {
        float factor = (wheel.deltaY > 0.f) ? 1.15f : (1.f / 1.15f);
        if (onVZoom) onVZoom(factor);
        return;
    }

    if (e.mods.isCtrlDown())
    {
        float factor = (wheel.deltaY > 0.f) ? 1.15f : (1.f / 1.15f);
        if (onZoom) onZoom(mPPB * factor - mPPB);
    }
    else if (e.mods.isShiftDown())
    {
        if (onHScroll) onHScroll(-wheel.deltaY * 2.0);
    }
    else
    {
        if (onVScroll) onVScroll(wheel.deltaY > 0 ? 3 : -3);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::paint(Graphics& g)
{
    auto b = getLocalBounds();
    g.fillAll(VC::Bg);
    if (mPPB <= 0) return;

    double totalBeats = mNumBars * 4.0;
    int    visNotes   = b.getHeight() / mNoteH + 2;

    // ── Row backgrounds (scale-highlighted rows shown with teal tint) ────
    // mNoteYOffset accounts for the ruler strip at the top (kRulerH in drum/fixed mode).
    // Must match noteToY() which returns mNoteYOffset + (mTopNote - note) * mNoteH.
    for (int dn = 0; dn <= visNotes; ++dn)
    {
        int  note    = mTopNote - dn;
        if (note < 0) break;
        int  y       = mNoteYOffset + dn * mNoteH;
        bool inScale = mScaleActive && mScaleInKey[((note % 12) + 12) % 12];
        bool isBlack = isBlackKeyStatic(note);

        if (inScale)
            g.setColour(isBlack ? Colour(0xff0c2820) : Colour(0xff133530));
        else if (isBlack)
            g.setColour(VC::Bg.brighter(0.05f));
        else if (note % 12 == 0)
            g.setColour(VC::Panel.brighter(0.08f));
        else
            g.setColour(VC::Panel);

        g.fillRect(0, y, b.getWidth(), mNoteH);
        g.setColour(VC::Accent.withAlpha(0.3f));
        g.drawHorizontalLine(y + mNoteH - 1, 0, (float)b.getWidth());
        if (note % 12 == 0)
        {
            g.setColour(inScale ? Colour(0x5000ff80) : VC::Accent.withAlpha(0.6f));
            g.drawHorizontalLine(y, 0, (float)b.getWidth());
        }
    }

    // ── Vertical time grid - zoom-adaptive subdivisions ──────────────────
    // Levels: subdivisions per beat (8 = 1/32 note, 4 = 1/16, 2 = 1/8, 1 = 1/4 beat).
    // A level is drawn only when its pixel spacing >= 5 px.
    // Finest lines drawn first; coarser lines overdraw at the same position.
    static constexpr float kMinLineSpacing = 5.f;
    struct GridLevel { int perBeat; Colour col; };
    const GridLevel gridLevels[] = {
        { 8, VC::Accent.withAlpha(0.10f) },   // 1/32 note
        { 4, VC::Accent.withAlpha(0.18f) },   // 1/16 note
        { 2, VC::Accent.withAlpha(0.30f) },   // 1/8 note
        { 1, VC::Accent.withAlpha(0.50f) },   // 1/4 note (beat)
    };
    for (const auto& lv : gridLevels)
    {
        float spacing = mPPB / (float)lv.perBeat;
        if (spacing < kMinLineSpacing) continue;
        double step      = 1.0 / lv.perBeat;
        double startBeat = std::floor(mBeatOff * lv.perBeat) / lv.perBeat;
        g.setColour(lv.col);
        for (double beat = startBeat; beat <= mBeatOff + b.getWidth() / mPPB + step; beat += step)
        {
            int x = beatToX(beat);
            if (x < 0 || x > b.getWidth()) continue;
            g.drawVerticalLine(x, (float)mNoteYOffset, (float)b.getHeight());
        }
    }
    // C.5b: Bar lines at multiples of (tsNum * 4 / tsDen) PPQ beats - always
    // shown, drawn on top.  4/4 = 4-beat bars, 3/4 = 3, 6/8 = 3, 5/4 = 5, 7/8 = 3.5.
    const double barBpb = (double) juce::jmax (1, mTsNum) * 4.0 / (double) juce::jmax (1, mTsDen);
    {
        const double startBar = std::floor (mBeatOff / barBpb);
        double startBeat = startBar * barBpb;
        for (double beat = startBeat;
             beat <= mBeatOff + b.getWidth() / mPPB + barBpb;
             beat += barBpb)
        {
            int x = beatToX(beat);
            if (x < 0 || x > b.getWidth()) continue;
            g.setColour(VC::Accent.brighter(0.3f));
            g.drawVerticalLine(x, 0, (float)b.getHeight());
            g.setColour(VC::TextDim); g.setFont(Font(9));
            g.drawText(String((int) std::round (beat / barBpb) + 1),
                       x + 2, 2, 24, 10, Justification::centredLeft);
        }
    }

    // End-of-pattern marker - no special colour; falls through as a regular bar line

    // ── Time selection highlight ──────────────────────────────────────────
    if (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart)
    {
        int sx = beatToX(mTimeSelBeatStart);
        int ex = beatToX(mTimeSelBeatEnd);
        g.setColour(VC::Highlight.withAlpha(0.08f));
        g.fillRect(sx, 0, ex - sx, b.getHeight());
        g.setColour(VC::Highlight.withAlpha(0.30f));
        g.fillRect(sx, 0, ex - sx, kRulerH);
        g.setColour(VC::Highlight.withAlpha(0.85f));
        g.drawVerticalLine(sx, 0.f, (float)kRulerH);
        g.drawVerticalLine(ex, 0.f, (float)kRulerH);
    }

    // ── Ghost notes (other rolls, drawn with their instrument color tinted) ──
    for (const auto& [ghostData, ghostCol] : mGhosts)
    {
        if (!ghostData) continue;
        Colour fillCol    = ghostCol.withSaturation(0.35f).withAlpha(0.22f);
        Colour outlineCol = ghostCol.withSaturation(0.55f).withAlpha(0.45f);
        for (const auto& n : ghostData->notes)
        {
            int x = beatToX(n.startBeat);
            int w = jmax(2, (int)(n.durationBeats * mPPB) - 1);
            int y = noteToY(displayMidiForNote(n));   // Phase C §P4.2
            if (x + w < 0 || x > b.getWidth())       continue;
            if (y + mNoteH < 0 || y > b.getHeight()) continue;
            g.setColour(fillCol);
            g.fillRoundedRectangle((float)x, (float)(y + 1),
                                   (float)w, (float)(mNoteH - 2), 2.f);
            g.setColour(outlineCol);
            g.drawRoundedRectangle((float)x + 0.5f, (float)(y + 1.5f),
                                   (float)w - 1, (float)(mNoteH - 3), 2.f, 0.75f);
        }
    }

    // ── Notes ─────────────────────────────────────────────────────────────
    if (mData)
    {
        for (int ni = 0; ni < (int)mData->notes.size(); ++ni)
        {
            const auto& n = mData->notes[ni];
            // Phase C §P4.2: in FullRoll mode, only show notes belonging to
            // the active slot (others render as ghosts in Batch 4).
            if (mRollMode == RollMode::FullRoll
                && effectiveSlot (n) != mActiveSlot)
                continue;
            int x = beatToX(n.startBeat);
            int w = jmax(3, (int)(n.durationBeats * mPPB) - 1);
            // Phase C §P4.2: DrumGrid renders notes at slot row regardless of
            // pitch so non-C5 notes from FullRoll edits show as lit cells.
            int y = noteToY(displayMidiForNote(n));
            if (x + w < 0 || x > b.getWidth())      continue;
            if (y + mNoteH < 0 || y > b.getHeight()) continue;

            Colour noteCol = mNoteColor.withMultipliedLightness(0.6f + n.velocity * 0.4f);
            if (n.muted) noteCol = mNoteColor.withSaturation(0.12f).withMultipliedLightness(0.4f).withAlpha(0.45f);

            float nx = (float)x, ny = (float)(y + 1), nw = (float)w, nh = (float)(mNoteH - 2);

            // ── 3D bevel fill ────────────────────────────────────────────────
            // Gradient top→bottom (lighter at top = specular)
            g.setGradientFill(ColourGradient(
                noteCol.brighter(0.30f), nx, ny,
                noteCol.darker (0.20f), nx, ny + nh, false));
            g.fillRoundedRectangle(nx, ny, nw, nh, 2.f);

            // Top highlight line
            g.setColour(noteCol.brighter(0.65f).withAlpha(0.7f));
            g.drawLine(nx + 2.5f, ny + 1.f, nx + nw - 2.5f, ny + 1.f, 1.f);

            // Bottom shadow line
            g.setColour(Colour(0xff000000).withAlpha(0.35f));
            g.drawLine(nx + 2.5f, ny + nh - 1.f, nx + nw - 2.5f, ny + nh - 1.f, 1.f);

            // ── Outline / selection ──────────────────────────────────────────
            if (isSelected(ni))
            {
                // White border with subtle glow
                g.setColour(Colours::white.withAlpha(0.2f));
                g.drawRoundedRectangle(nx - 1.f, ny - 1.f, nw + 2.f, nh + 2.f, 3.f, 1.5f);
                g.setColour(Colours::white.withAlpha(0.95f));
                g.drawRoundedRectangle(nx + 0.5f, ny + 0.5f, nw - 1.f, nh - 1.f, 2.f, 1.5f);
            }
            else if (n.groupId >= 0)
            {
                g.setColour(Colour(0xffffaa00).withAlpha(0.75f));
                g.drawRoundedRectangle(nx + 0.5f, ny + 0.5f, nw - 1.f, nh - 1.f, 2.f, 1.5f);
            }
            else
            {
                g.setColour(noteCol.brighter(0.3f).withAlpha(0.55f));
                g.drawRoundedRectangle(nx + 0.5f, ny + 0.5f, nw - 1.f, nh - 1.f, 2.f, 1.f);
            }

            // ── Note name (C5, D#4, etc.) - only when wide enough ──────────
            // FL Studio convention: MIDI 60 = C5 (middle C). Drop the -1.
            if (w >= 18 && mNoteH >= 9)
            {
                static const char* kNoteNames[] = {
                    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                int pitch  = n.midiNote % 12;
                int octave = n.midiNote / 12;
                juce::String noteName = juce::String(kNoteNames[pitch]) + juce::String(octave);
                int fontSize = juce::jlimit(7, 10, mNoteH - 3);
                g.setFont(Font(fontSize, Font::bold));
                g.setColour(Colours::white.withAlpha(n.muted ? 0.35f : 0.80f));
                g.drawText(noteName, x + 3, y + 1, w - 5, mNoteH - 2,
                           Justification::centredLeft, true);
            }

            // ── Slide / Portamento type indicators ────────────────────────
            if (n.type == NoteType::Slide && w > 8)
            {
                // Right-pointing triangle at note's right end
                float tx  = (float)(x + w - 2);
                float ty  = (float)(y + 2);
                float th  = (float)(mNoteH - 4);
                float tw2 = jmin(th * 0.7f, 7.f);
                Path tri;
                tri.addTriangle(tx - tw2, ty, tx - tw2, ty + th, tx, ty + th * 0.5f);
                g.setColour(Colours::white.withAlpha(0.85f));
                g.fillPath(tri);
            }
            else if (n.type == NoteType::Portamento && w > 6)
            {
                // Small curved arc on the left end indicating pitch slide-in
                float px2 = (float)(x + 2);
                float py2 = (float)(y + 1);
                float ph2 = (float)(mNoteH - 2);
                float pw2 = jmin(ph2 * 0.55f, 6.f);
                Path arc;
                arc.addArc(px2, py2, pw2, ph2,
                           juce::MathConstants<float>::pi * 0.3f,
                           juce::MathConstants<float>::pi * 1.1f, true);
                g.setColour(Colours::orange.withAlpha(0.9f));
                g.strokePath(arc, PathStrokeType(1.5f));
            }
        }
    }

    // Preview note being drawn (Draw tool)
    if (mDrawing && mDrawNote >= 0 && mActiveTool == PRTool::Draw)
    {
        int x = beatToX(mDrawStart);
        int w = jmax(3, (int)((mDrawEnd - mDrawStart) * mPPB) - 1);
        int y = noteToY(mDrawNote);
        g.setColour(VC::Highlight.withAlpha(0.7f));
        g.fillRoundedRectangle((float)x, (float)(y + 1), (float)w, (float)(mNoteH - 2), 2.f);
    }

    // Marquee selection rectangle
    if (mMarqueeActive && !mMarqueeRect.isEmpty())
    {
        g.setColour(VC::Blue.withAlpha(0.15f));
        g.fillRect(mMarqueeRect);
        g.setColour(VC::Blue.withAlpha(0.8f));
        g.drawRect(mMarqueeRect, 1);
    }

    // Slice line
    if (mSlicing && mSliceStart != mSliceEnd)
    {
        g.setColour(VC::Highlight.withAlpha(0.85f));
        g.drawLine((float)mSliceStart.x, (float)mSliceStart.y,
                   (float)mSliceEnd.x,   (float)mSliceEnd.y, 2.f);
        // Tick marks at start and end
        g.fillEllipse((float)mSliceStart.x - 3, (float)mSliceStart.y - 3, 6, 6);
        g.fillEllipse((float)mSliceEnd.x   - 3, (float)mSliceEnd.y   - 3, 6, 6);
    }

    // Zoom-to-rect overlay
    if (mZoomRectActive && !mZoomRect.isEmpty())
    {
        g.setColour(VC::Yellow.withAlpha(0.1f));
        g.fillRect(mZoomRect);
        g.setColour(VC::Yellow.withAlpha(0.7f));
        g.drawRect(mZoomRect, 1);
    }

    // Stamp chord ghost preview
    if (mActiveTool == PRTool::Stamp && !mStampIntervals.empty())
    {
        int    rootNote = snapPitchToScale(yToNote(mStampPos.y));
        double beat     = snapBeat(xToBeat(mStampPos.x));
        int    px       = beatToX(beat);
        double dur      = 4.0 / mSnapDenom;
        int    pw       = jmax(3, (int)(dur * mPPB) - 1);
        for (int interval : mStampIntervals)
        {
            int mn = jlimit(0, 127, rootNote + interval);
            int py = noteToY(mn);
            g.setColour(VC::Highlight.withAlpha(0.45f));
            g.fillRoundedRectangle((float)px, (float)(py + 1),
                                   (float)pw, (float)(mNoteH - 2), 2.f);
            g.setColour(VC::Highlight.withAlpha(0.85f));
            g.drawRoundedRectangle((float)px + 0.5f, (float)(py + 1.5f),
                                   (float)pw - 1, (float)(mNoteH - 3), 2.f, 1.f);
        }
    }

    // ── Ruler strip (click-to-seek zone at top) ───────────────────────────
    g.setColour(VC::Panel.brighter(0.12f));
    g.fillRect(0, 0, b.getWidth(), kRulerH);
    g.setColour(VC::Accent.withAlpha(0.5f));
    g.drawHorizontalLine(kRulerH - 1, 0, (float)b.getWidth());
    // C.5b: ruler bar boundaries follow the pattern's intrinsic TS so the
    // grid bars + ruler bar numbers stay aligned.  4/4 → every 4 beats,
    // 3/4 → every 3, 6/8 → every 3 (PPQ-beat basis), 7/8 → every 3.5.
    const double rulerBarBpb = (double) juce::jmax (1, mTsNum) * 4.0
                              / (double) juce::jmax (1, mTsDen);
    for (double beat = std::floor(mBeatOff); beat <= mBeatOff + b.getWidth() / mPPB + 1.0; beat += 0.5)
    {
        int rx = beatToX(beat);
        if (rx < 0 || rx > b.getWidth()) continue;
        const double barFrac = beat / rulerBarBpb;
        bool isBar  = (std::abs (barFrac - std::round (barFrac)) < 1e-6);
        bool isBeat = (std::fmod(beat, 1.0) < 1e-9);
        if (isBar)
        {
            g.setColour(VC::Accent.brighter(0.5f));
            g.drawVerticalLine(rx, 0, (float)kRulerH);
            g.setColour(VC::Text); g.setFont(Font(9));
            g.drawText(String((int) std::round (barFrac) + 1), rx + 2, 1, 20, kRulerH - 2,
                       Justification::centredLeft, false);
        }
        else if (isBeat)
        {
            g.setColour(VC::Accent.withAlpha(0.6f));
            g.drawVerticalLine(rx, kRulerH / 2, (float)kRulerH);
        }
    }
    // Playhead tick in ruler
    if (mPlayhead >= 0.0)
    {
        int px = beatToX(mPlayhead);
        if (px >= 0 && px <= b.getWidth())
        {
            g.setColour(VC::Green.withAlpha(0.9f));
            Path tri;
            tri.addTriangle((float)px, 0.f, (float)(px - 5), (float)kRulerH,
                            (float)(px + 5), (float)kRulerH);
            g.fillPath(tri);
        }
    }

    // Playhead
    if (mPlayhead >= 0.0)
    {
        int px = beatToX(mPlayhead);
        if (px >= 0 && px <= b.getWidth())
        {
            g.setColour(VC::Green.withAlpha(0.8f));
            g.fillRect(px, kRulerH, 2, b.getHeight() - kRulerH);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ControlLane
// ─────────────────────────────────────────────────────────────────────────────
ControlLane::ControlLane() {}

void ControlLane::setScrollState(float ppb, double beatOff) { mPPB = ppb; mBeatOff = beatOff; repaint(); }
void ControlLane::setData(PianoRollData* data) { mData = data; repaint(); }
void ControlLane::setMode(Mode m)              { mMode = m;    repaint(); }

float ControlLane::getVal(const PianoNote& n) const
{
    switch (mMode) {
        case Velocity:     return n.velocity;
        case Panning:      return (n.panning + 1.f) * 0.5f;
        case PitchBend:    return (n.finePitch + 100.f) / 200.f;
        case FilterCutoff: return n.filterCutoff;
    }
    return 0.f;
}

void ControlLane::setVal(PianoNote& n, float v)
{
    v = jlimit(0.f, 1.f, v);
    switch (mMode) {
        case Velocity:     n.velocity  = v; break;
        case Panning:      n.panning   = v * 2.f - 1.f; break;
        case PitchBend:    n.finePitch = v * 200.f - 100.f; break;
        case FilterCutoff: n.filterCutoff = v; break;
    }
}

float ControlLane::yToNormVal(int y) const
{
    return jlimit(0.f, 1.f, 1.f - (float)y / (float)jmax(1, getHeight()));
}

PianoNote* ControlLane::noteNearX(int x, int y) const
{
    if (!mData || mPPB <= 0) return nullptr;
    double beat = mBeatOff + (double)x / mPPB;

    // Collect all notes within horizontal tolerance.
    // Tolerance is in PIXELS (converted to beats via mPPB) so the hit zone
    // is consistent regardless of zoom level - 0.3 beats at default zoom
    // was huge at max zoom and tight at min zoom.
    struct Candidate { PianoNote* note; double dist; };
    std::vector<Candidate> candidates;
    static constexpr double kPxTol = 10.0;
    const double kBeatTol = kPxTol / mPPB;
    // 2026-04-26 (D-7 sub-4): when the grid has a selection, restrict edits
    // to the selected notes - fixes the chord-overlap bug where dragging
    // the bar of a selected note in a chord would actually edit the
    // unselected note next to it.
    const bool selectionLocked = (hasAnySelection && hasAnySelection());
    for (auto& n : mData->notes)
    {
        if (selectionLocked && isNoteSelected && !isNoteSelected (&n))
            continue;
        double dist = std::abs(n.startBeat - beat);
        if (dist < kBeatTol)
            candidates.push_back({ const_cast<PianoNote*>(&n), dist });
    }

    if (candidates.empty()) return nullptr;

    // Single hit or no Y provided - return closest by beat
    if (candidates.size() == 1 || y < 0)
    {
        PianoNote* best = nullptr; double minD = kBeatTol;
        for (auto& c : candidates) if (c.dist < minD) { minD = c.dist; best = c.note; }
        return best;
    }

    // Multiple notes at same beat - disambiguate by closest node Y to cursor
    const bool bipolar  = (mMode == Panning || mMode == PitchBend);
    const int  contentH = jmax(1, getHeight() - kHeaderH);

    PianoNote* best  = nullptr;
    int        bestDY = INT_MAX;
    for (auto& c : candidates)
    {
        float val = getVal(*c.note);
        int nodeY;
        if (bipolar)
        {
            int cy     = kHeaderH + contentH / 2;
            int offset = (int)((val - 0.5f) * (contentH - 8));
            nodeY = cy - offset;
        }
        else
        {
            int stemH = jmax(2, (int)(val * (contentH - 6)));
            nodeY = getHeight() - 4 - stemH;
        }
        int dy = std::abs(nodeY - y);
        if (dy < bestDY) { bestDY = dy; best = c.note; }
    }
    return best;
}

void ControlLane::paint(Graphics& g)
{
    auto b = getLocalBounds();
    g.setColour(VC::Panel.darker(0.15f)); g.fillRect(b);
    g.setColour(VC::Accent.withAlpha(0.5f)); g.drawRect(b, 1);

    // ── Header (dropdown) ────────────────────────────────────────────────
    g.setColour(VC::Panel.brighter(0.1f));
    g.fillRect(0, 0, b.getWidth(), kHeaderH);
    g.setColour(VC::Accent.withAlpha(0.7f));
    g.drawHorizontalLine(kHeaderH - 1, 0.f, (float)b.getWidth());

    static const char* kModeNames[] = {
        "Control > Velocity",
        "Control > Panning",
        "Control > Pitch Bend"
    };
    int modeIdx = (mMode == Velocity) ? 0 : (mMode == Panning) ? 1 : 2;
    g.setColour(VC::Text); g.setFont(Font(10, Font::bold));
    g.drawText(juce::String(kModeNames[modeIdx]) + "  \xe2\x96\xbe", // ▾
               6, 1, b.getWidth() - 12, kHeaderH - 2, Justification::centredLeft);

    if (!mData || mPPB <= 0) return;

    // ── Beat grid lines (matches main canvas vertical timing) ─────────────
    // Draw bar/beat ticks inside the content area only (below header).
    {
        const int contentTop = kHeaderH;
        for (double beat = std::floor(mBeatOff);
             beat <= mBeatOff + b.getWidth() / mPPB + 1.0;
             beat += 0.5)
        {
            int gx = (int)((beat - mBeatOff) * mPPB);
            if (gx < 0 || gx > b.getWidth()) continue;
            const bool isBar  = (std::fmod(beat, 4.0) < 1e-9);
            const bool isBeat = (std::fmod(beat, 1.0) < 1e-9);
            if (isBar)
                g.setColour(VC::Accent.withAlpha(0.35f));
            else if (isBeat)
                g.setColour(VC::Accent.withAlpha(0.18f));
            else
                g.setColour(VC::Accent.withAlpha(0.08f));
            g.drawVerticalLine(gx, (float)contentTop, (float)b.getHeight());
        }
    }

    // ── Bipolar centre line for Panning / Pitch Bend ─────────────────────
    const bool bipolar = (mMode == Panning || mMode == PitchBend);
    const int  contentH = b.getHeight() - kHeaderH;
    const int  contentY = kHeaderH;
    if (bipolar)
    {
        int cy = contentY + contentH / 2;
        g.setColour(VC::Accent.withAlpha(0.45f));
        g.drawHorizontalLine(cy, 4.f, (float)b.getWidth() - 4);
    }

    // ── Stem + node + tail rendering ─────────────────────────────────────
    Colour nodeColor = (mMode == Velocity) ? VC::Green
                     : (mMode == Panning)  ? VC::Blue
                     :                       Colour(0xff44ffcc); // teal for pitch bend

    for (const auto& n : mData->notes)
    {
        int x     = (int)((n.startBeat - mBeatOff) * mPPB);
        int tailW = jmax(2, (int)(n.durationBeats * mPPB) - 1);
        if (x + tailW < 0 || x > b.getWidth()) continue;

        float val    = getVal(n);
        // 2026-04-26 (D-7 sub-4): selected notes paint RED so the user can
        // see at-a-glance which bars the lane interactions will affect.
        const bool selected = (isNoteSelected && isNoteSelected (&n));
        Colour col   = selected
            ? Colour (0xffff3344).withMultipliedLightness (0.65f + val * 0.35f)
            : nodeColor.withMultipliedLightness(0.55f + val * 0.45f);
        if (n.muted) col = col.withSaturation(0.1f).withAlpha(0.4f);

        int nodeY, stemTop, stemBot;
        if (bipolar)
        {
            int cy     = contentY + contentH / 2;
            int offset = (int)((val - 0.5f) * (contentH - 8));
            nodeY  = cy - offset;
            stemTop = jmin(nodeY, cy);
            stemBot = jmax(nodeY, cy);
        }
        else
        {
            int stemH = jmax(2, (int)(val * (contentH - 6)));
            stemTop  = b.getHeight() - 4 - stemH;
            stemBot  = b.getHeight() - 4;
            nodeY    = stemTop;
        }

        g.setColour(col);
        // 1px vertical stem
        g.drawLine((float)x, (float)stemTop, (float)x, (float)stemBot, 1.f);
        // 4px filled circle node at value end
        g.fillEllipse((float)(x - 2), (float)(nodeY - 2), 4.f, 4.f);
        // Horizontal tail from node center, length = note pixel width
        g.drawLine((float)x, (float)nodeY, (float)(x + tailW), (float)nodeY, 1.f);
    }
}

void ControlLane::mouseDown(const MouseEvent& e)
{
    // ── Header: open mode dropdown ────────────────────────────────────────
    if (e.y < kHeaderH)
    {
        PopupMenu m;
        m.addItem(1, "Velocity");
        m.addItem(2, "Panning");
        m.addItem(3, "Pitch Bend");
        // Batch E #2 (2026-05-01): Filter Cutoff exposed.  Engines read
        // PianoNote::filterCutoff (0..1) on note-on and apply as a per-note
        // cutoff offset (octaves around master cutoff knob).
        m.addItem(4, "Filter Cutoff");
        m.showMenuAsync(PopupMenu::Options().withTargetComponent(this),
            [this](int r) {
                if (r < 1 || r > 4) return;
                mMode = static_cast<Mode>(r - 1);
                if (onModeChange) onModeChange(mMode);
                repaint();
            });
        return;
    }

    if (!mData) return;
    // Map y to normalised value accounting for the header offset
    auto yToValWithHeader = [&](int py) {
        int contentH = jmax(1, getHeight() - kHeaderH);
        int relY     = py - kHeaderH;
        return jlimit(0.f, 1.f, 1.f - (float)relY / (float)contentH);
    };
    mDragNote = noteNearX(e.x, e.y);
    if (mDragNote)
    {
        if (onBeginEdit) onBeginEdit ("Adjust Lane Value");
        setVal(*mDragNote, yToValWithHeader(e.y));
        if (onChanged) onChanged();
        repaint();
    }
}

void ControlLane::mouseDrag(const MouseEvent& e)
{
    if (!mData || !mDragNote || e.y < kHeaderH) return;
    int contentH = jmax(1, getHeight() - kHeaderH);
    int relY     = e.y - kHeaderH;
    float val    = jlimit(0.f, 1.f, 1.f - (float)relY / (float)contentH);
    setVal(*mDragNote, val); if (onChanged) onChanged(); repaint();
}

void ControlLane::mouseUp(const MouseEvent&)
{
    mDragNote = nullptr;
    if (onCommitEdit) onCommitEdit();
}

// 2026-04-26 (D-7 sub-4): Alt+Wheel over the lane adjusts the currently-
// displayed property (velocity / pan / pitch-bend) for the note whose bar
// is under the cursor x.  Default delta ±0.05; Shift+Alt+Wheel = ±0.01 for
// fine adjustment.  Honours the selection lock - when notes are selected,
// only those notes' bars are targetable (noteNearX filters internally).
void ControlLane::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (!mData)                          { Component::mouseWheelMove (e, wheel); return; }
    if (! e.mods.isAltDown())            { Component::mouseWheelMove (e, wheel); return; }
    if (e.y < kHeaderH)                  { return; }    // ignore wheel over the header

    PianoNote* target = noteNearX (e.x, e.y);
    if (target == nullptr) return;

    const float delta = (e.mods.isShiftDown() ? 0.01f : 0.05f)
                      * (wheel.deltaY >= 0.f ? +1.f : -1.f);

    if (onBeginEdit) onBeginEdit ("Adjust Lane Value");
    const float newVal = juce::jlimit (0.0f, 1.0f, getVal (*target) + delta);
    setVal (*target, newVal);
    if (onCommitEdit) onCommitEdit();
    if (onChanged)    onChanged();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// PianoRollContainer
// ─────────────────────────────────────────────────────────────────────────────
bool PianoRollContainer::keyPressed(const juce::KeyPress& key)
{
    if (mGrid) return mGrid->keyPressed(key);
    return false;
}

void PianoRollContainer::focusGained(juce::Component::FocusChangeType)
{
    if (mGrid) mGrid->grabKeyboardFocus();
}

PianoRollContainer::PianoRollContainer()
{
    setWantsKeyboardFocus(true);  // container catches focus if grid misses it

    mKeyboard = std::make_unique<PianoKeyboard>(); addAndMakeVisible(*mKeyboard);
    mGrid     = std::make_unique<PianoRollGrid>();  addAndMakeVisible(*mGrid);
    mLane     = std::make_unique<ControlLane>();   addAndMakeVisible(*mLane);

    // 2026-04-26 (D-7): bare-M from the grid toggles keyboard column.
    mGrid->onToggleKeyboard = [this] {
        mKeyboardVisible = !mKeyboardVisible;
        resized();
        repaint();
    };

    // 2026-04-26 (D-7 sub-4): Control lane queries the grid for selection
    // membership so it can paint selected bars red AND restrict edits to
    // the selected notes (fixes chord-overlap drag-targets-wrong-note bug).
    mLane->isNoteSelected = [this] (const PianoNote* note) -> bool {
        if (! mGrid || note == nullptr) return false;
        auto* data = mGrid->getData();
        if (data == nullptr || data->notes.empty()) return false;
        const PianoNote* base = data->notes.data();
        const int idx = (int) (note - base);
        if (idx < 0 || idx >= (int) data->notes.size()) return false;
        return mGrid->isNoteIndexSelected (idx);
    };
    mLane->hasAnySelection = [this] {
        return mGrid ? mGrid->hasSelection() : false;
    };
    // Lane edits (drag / wheel) hit the grid's undo stack the same way other
    // PianoRollGrid edits do.  beginEdit takes a snapshot; commitEdit pushes
    // it as a single undo entry.
    mLane->onBeginEdit  = [this] (const juce::String& label) {
        if (mGrid) mGrid->beginEdit (label);
    };
    mLane->onCommitEdit = [this] {
        if (mGrid) mGrid->commitEdit();
    };

    // Forward keyboard preview events. Press-and-hold audition: fire On on
    // key-down and Off on key-up so the engine plays a sustained note for the
    // full press duration (the one-shot onNoteAudition is reserved for grid
    // create / drag, where a brief preview is what we want).
    mKeyboard->onNotePreview = [this](int note, bool on) {
        if (on)  { if (onNoteAuditionOn)  onNoteAuditionOn (note); }
        else     { if (onNoteAuditionOff) onNoteAuditionOff(note); }
        if (onNotePreview) onNotePreview(note, on);
    };

    // 2026-04-21: forward wheel events from the keyboard strip to the grid so
    //   scrolling/zoom modifiers work identically over either surface.
    mKeyboard->onWheel = [this](const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
        if (mGrid) mGrid->mouseWheelMove(e, wheel);
    };

    // ── Toolbar row 1: Wrench | Magnet | 7 tool buttons | Undo | Redo | H ─
    mWrenchBtn = std::make_unique<TextButton>("Tools");
    mWrenchBtn->setTooltip("Tools - Quantize, Strum, Glue, Chop, Randomize, Articulate...");
    mWrenchBtn->onClick = [this] {
        if (mGrid) mGrid->showToolsMenu();
        if (mGrid) mGrid->grabKeyboardFocus();
    };
    addAndMakeVisible(*mWrenchBtn);

    mMagnetBtn = std::make_unique<RightClickTextButton>();
    mMagnetBtn->setButtonText("Snap");
    mMagnetBtn->setClickingTogglesState(true);
    mMagnetBtn->setToggleState(true, dontSendNotification); // snap on by default
    mMagnetBtn->setTooltip("Snap on/off - right-click to set resolution");
    mMagnetBtn->onClick = [this] {
        mSnapEnabled = mMagnetBtn->getToggleState();
        mGrid->setSnapEnabled(mSnapEnabled);
        mGrid->grabKeyboardFocus();
    };
    mMagnetBtn->onRightMouseDown = [this](const MouseEvent&) {
        PopupMenu m;
        m.addItem(4,  "1/4");
        m.addItem(8,  "1/8");
        m.addItem(16, "1/16");
        m.addItem(32, "1/32");
        m.showMenuAsync(PopupMenu::Options().withTargetComponent(mMagnetBtn.get()),
            [this](int r) {
                if (r > 0) { mSnapDenom = r; syncScrollState(); }
                if (mGrid) mGrid->grabKeyboardFocus();
            });
    };
    addAndMakeVisible(*mMagnetBtn);

    static const char* toolLabels[] = {
        "Draw","Paint","Del","Mute","Slice","Sel","Zoom","Stamp" };
    static const char* toolTips[] = {
        "Draw (P) - LMB draw | click note to move | near right edge to resize | Ctrl+click select",
        "Paint (B) - drag to paint notes continuously",
        "Delete (D) - click/drag to erase notes",
        "Mute (T) - toggle note mute",
        "Slice (C) - drag to draw cut line",
        "Select (E) - marquee select | drag notes to move",
        "Zoom (Shift+Z) - click to zoom in | drag region | RMB to zoom out",
        "Stamp - click to place selected chord"
    };

    for (int i = 0; i < 8; ++i)
    {
        mToolBtns[i] = std::make_unique<TextButton>(toolLabels[i]);
        mToolBtns[i]->setTooltip(toolTips[i]);
        const auto tool = static_cast<PianoRollGrid::PRTool>(i);
        mToolBtns[i]->onClick = [this, tool] { setActiveTool(tool); };
        if (i < 7) addAndMakeVisible(*mToolBtns[i]); // Stamp (7) hidden from toolbar
    }
    mToolBtns[0]->setToggleState(true, dontSendNotification);

    mUndoBtn = std::make_unique<TextButton>("Undo");
    mUndoBtn->setTooltip("Undo (Ctrl+Z)");
    mUndoBtn->onClick = [this] {
        undoRoll();
        if (mGrid) mGrid->grabKeyboardFocus();
    };
    mUndoBtn->setEnabled(false);
    addAndMakeVisible(*mUndoBtn);

    mRedoBtn = std::make_unique<TextButton>("Redo");
    mRedoBtn->setTooltip("Redo (Ctrl+Alt+Z)");
    mRedoBtn->onClick = [this] {
        redoRoll();
        if (mGrid) mGrid->grabKeyboardFocus();
    };
    mRedoBtn->setEnabled(false);
    addAndMakeVisible(*mRedoBtn);

    mHistoryBtn = std::make_unique<TextButton>("H");
    mHistoryBtn->setTooltip("Show undo history");
    mHistoryBtn->onClick = [this] {
        if (onShowHistoryWindow) onShowHistoryWindow();
        if (mGrid) mGrid->grabKeyboardFocus();
    };
    addAndMakeVisible(*mHistoryBtn);

    // ── Zoom buttons + scale controls (all on row 1 now) ─────────────────
    mZoomInBtn = std::make_unique<TextButton>("+");
    mZoomInBtn->setTooltip("Zoom in (Ctrl+scroll)");
    mZoomInBtn->onClick = [this] {
        applyZoom(1.3f);
        if (mGrid) mGrid->grabKeyboardFocus();
    };
    addAndMakeVisible(*mZoomInBtn);

    mZoomOutBtn = std::make_unique<TextButton>("-");
    mZoomOutBtn->setTooltip("Zoom out (Ctrl+scroll)");
    mZoomOutBtn->onClick = [this] {
        applyZoom(1.f / 1.3f);
        if (mGrid) mGrid->grabKeyboardFocus();
    };
    addAndMakeVisible(*mZoomOutBtn);

    // ── Initialise scale / chord state and seed the grid ─────────────────
    mGrid->setStampChord(kChordDefs[mChordIdx].intervals);
    updateScaleFromUI();

    // ── Menu bar ──────────────────────────────────────────────────────────
    mMenuBarModel = std::make_unique<PianoRollMenuBar>(*this);
    mMenuBar      = std::make_unique<juce::MenuBarComponent>(mMenuBarModel.get());
    addAndMakeVisible(*mMenuBar);

    // ── Wire grid callbacks ───────────────────────────────────────────────
    mGrid->onZoom    = [this](float delta) { applyZoom((mPPB + delta) / mPPB); };
    mGrid->onVZoom   = [this](float factor) { applyVZoom(factor); };
    mGrid->onHScroll = [this](double dB)   { mBeatOff = jmax(0.0, mBeatOff + dB); syncScrollState(); };
    mGrid->onVScroll = [this](int dN) {
        if (mFixedRange) return;
        int visRows = mGrid ? (mGrid->getHeight() / PianoRollGrid::kNoteH) : 8;
        // 2026-04-21: FL-convention range - bottom C0 (MIDI 0), top G10 (MIDI 127).
        int minTop  = visRows;
        mTopNote = jlimit(minTop, 127, mTopNote + dN);
        syncScrollState();
    };
    mGrid->onNotesChanged = [this] { mLane->repaint(); };

    mGrid->onToolChanged = [this](PianoRollGrid::PRTool t) {
        mActiveTool = t;
        int idx = static_cast<int>(t);
        for (int i = 0; i < 8; ++i)
            if (mToolBtns[i]) mToolBtns[i]->setToggleState(i == idx, dontSendNotification);
    };
    mGrid->onUndoRedoStateChanged = [this] { updateUndoRedoBtns(); };
    mGrid->onSeek = [this](double beat) { if (onSeek) onSeek(beat); };

    mGrid->onNoteAudition = [this](int note) {
        if (onNoteAudition) onNoteAudition(note);
    };
    mGrid->onNoteAuditionOn  = [this](int note) {
        if (onNoteAuditionOn)  onNoteAuditionOn (note);
    };
    mGrid->onNoteAuditionOff = [this](int note) {
        if (onNoteAuditionOff) onNoteAuditionOff(note);
    };

    mGrid->onZoomTo = [this](double beatStart, double beatEnd) {
        mPreZoomPPB     = mPPB;
        mPreZoomBeatOff = mBeatOff;
        mZoomedIn       = true;
        double range = jmax(0.01, beatEnd - beatStart);
        // Match applyZoom limits: 8 bars full-out, 1 bar full-in.
        const float vpW    = (float)jmax(1, mGrid->getWidth());
        const float minPPB = vpW / (4.f * 8.f);
        const float maxPPB = vpW / (4.f * 1.f);
        mPPB     = jlimit(minPPB, maxPPB, vpW / (float)range);
        mBeatOff = beatStart;
        syncScrollState();
    };
    mGrid->onZoomToggle = [this] {
        if (mZoomedIn)
        {
            mPPB      = mPreZoomPPB;
            mBeatOff  = mPreZoomBeatOff;
            mZoomedIn = false;
        }
        else
        {
            applyZoom(1.f / 1.3f); // right-click in Zoom tool = zoom out
        }
        syncScrollState();
    };

    // ── Lane mode changes from the lane's own header dropdown ────────────
    mLane->onModeChange = [this](ControlLane::Mode) { /* mode is managed inside lane */ };

    // ── Context label (toolbar row, right of zoom buttons) ───────────────
    mContextLabel = std::make_unique<juce::Label>();
    mContextLabel->setColour(juce::Label::textColourId, VC::TextDim);
    mContextLabel->setFont(juce::Font(11.f, juce::Font::bold));
    mContextLabel->setJustificationType(juce::Justification::centredRight);
    mContextLabel->setInterceptsMouseClicks(false, false);
    addAndMakeVisible(*mContextLabel);

    // ── Scrollbars ───────────────────────────────────────────────────────
    mHScroll = std::make_unique<juce::ScrollBar>(false); // horizontal
    mHScroll->setAutoHide(false);
    mHScroll->addListener(this);
    addAndMakeVisible(*mHScroll);

    mVScroll = std::make_unique<juce::ScrollBar>(true);  // vertical
    mVScroll->setAutoHide(false);
    mVScroll->addListener(this);
    addAndMakeVisible(*mVScroll);

    syncScrollState();
}

void PianoRollContainer::setActiveTool(PianoRollGrid::PRTool t)
{
    mActiveTool = t;
    int idx = static_cast<int>(t);
    for (int i = 0; i < 8; ++i)
        if (mToolBtns[i]) mToolBtns[i]->setToggleState(i == idx, dontSendNotification);
    mGrid->setTool(t);
    mGrid->grabKeyboardFocus(); // return focus to grid after toolbar click
}

void PianoRollContainer::updateUndoRedoBtns()
{
    bool canUndo = mUndoCtx.isValid() && mUndoCtx.manager->canUndo();
    bool canRedo = mUndoCtx.isValid() && mUndoCtx.manager->canRedo();
    if (mUndoBtn) mUndoBtn->setEnabled(canUndo);
    if (mRedoBtn) mRedoBtn->setEnabled(canRedo);
}

void PianoRollContainer::undoRoll()
{
    if (mUndoCtx.undo) mUndoCtx.undo();
}

void PianoRollContainer::redoRoll()
{
    if (mUndoCtx.redo) mUndoCtx.redo();
}

void PianoRollContainer::setUndoContext(const UndoContext& ctx)
{
    mUndoCtx = ctx;
    if (mGrid) mGrid->setUndoContext(ctx);
    if (ctx.showHistory) onShowHistoryWindow = ctx.showHistory;
    updateUndoRedoBtns();
}

void PianoRollContainer::setGhostData(const std::vector<std::pair<const PianoRollData*, Colour>>& ghosts)
{
    mGhostStore = ghosts;
    if (mGrid) mGrid->setGhostData(mGhostsVisible ? ghosts : decltype(ghosts){});
}

void PianoRollContainer::setNoteColor(Colour c)
{
    if (mGrid) mGrid->setNoteColor(c);
}

void PianoRollContainer::setFixedNoteRange(int bottomMidi, int topMidi)
{
    mFixedRange       = true;
    mFixedRangeBottom = bottomMidi;
    mFixedRangeTop    = topMidi;
    mTopNote          = topMidi;
    mGrid->setFixedNoteRange(true, bottomMidi, topMidi);
    mGrid->setNoteYOffset(PianoRollGrid::kRulerH);
    syncScrollState();
}

void PianoRollContainer::setDrumMode()
{
    mDrumMode = true;
    // Scale / chord state is now menu-driven; no toolbar controls to hide.
    // Stamp tool (index 7) is never shown in the toolbar.
}

// Phase C §P4.2 (2026-04-24): dual-roll mode + active-slot for the Drums page.
int PianoRollGrid::effectiveSlot (const PianoNote& n) const noexcept
{
    if (n.slotIndex >= 0) return n.slotIndex;
    if (n.midiNote >= 36 && n.midiNote <= 51) return 51 - n.midiNote;
    return -1;
}

int PianoRollGrid::displayMidiForNote (const PianoNote& n) const noexcept
{
    if (mRollMode == RollMode::DrumGrid)
    {
        const int s = effectiveSlot (n);
        if (s >= 0) return 51 - s;
    }
    return n.midiNote;
}

void PianoRollGrid::tagLastCreatedNote (int rowMidi)
{
    if (mData == nullptr || mData->notes.empty()) return;
    auto& n = mData->notes.back();
    if (mRollMode == RollMode::DrumGrid)
    {
        // Tag the slot from row but keep midiNote = rowMidi (in [36..51]) so
        // the existing display path still positions the note at the right
        // row.  Playback engine prefers slotIndex - tagging it makes the
        // note round-trip safely through full-roll mode later.
        n.slotIndex = juce::jlimit (0, 15, 51 - rowMidi);
    }
    else if (mRollMode == RollMode::FullRoll)
    {
        // Full-roll: slot from active dropdown, midiNote already = rowMidi.
        n.slotIndex = juce::jlimit (0, 15, mActiveSlot);
    }
    // Standard mode: no-op (Layer/Bass keep midiNote=row, slotIndex=-1).
}

void PianoRollContainer::setRollMode (RollMode m)
{
    mRollMode = m;
    if (mGrid)
    {
        const auto gm = (m == RollMode::DrumGrid) ? PianoRollGrid::RollMode::DrumGrid
                      : (m == RollMode::FullRoll) ? PianoRollGrid::RollMode::FullRoll
                                                   : PianoRollGrid::RollMode::Standard;
        mGrid->setRollMode (gm);
    }
    // FullRoll mode disables the fixed 36..51 range; user can scroll the
    // full keyboard.  DrumGrid restores the 16-row drum layout.
    if (m == RollMode::FullRoll)
    {
        mFixedRange = false;
        if (mGrid) mGrid->setFixedNoteRange (false, 0, 127);
        if (mGrid) mGrid->setNoteYOffset (PianoRollGrid::kRulerH);
    }
    else
    {
        mFixedRange       = true;
        mFixedRangeBottom = 36;
        mFixedRangeTop    = 51;
        mTopNote          = 51;
        if (mGrid) mGrid->setFixedNoteRange (true, 36, 51);
        if (mGrid) mGrid->setNoteYOffset (PianoRollGrid::kRulerH);
    }
    syncScrollState();
    repaint();
}

void PianoRollContainer::setActiveSlot (int slot)
{
    mActiveSlot = juce::jlimit (0, 15, slot);
    if (mGrid) mGrid->setActiveSlot (mActiveSlot);
    repaint();
}

void PianoRollContainer::setDrumRowLabels(const std::vector<juce::String>& labels)
{
    if (mKeyboard) mKeyboard->setDrumRowLabels(labels);
}

void PianoRollContainer::setNoteLabelProvider(std::function<juce::String(int)> provider)
{
    if (mKeyboard) mKeyboard->setNoteLabelProvider(std::move(provider));
}

void PianoRollContainer::setAllKeysWhiteMode(bool enabled)
{
    if (mKeyboard) mKeyboard->setAllKeysWhiteMode(enabled);
}

void PianoRollContainer::setTopNote(int topNote)
{
    mTopNote = juce::jlimit(0, 127, topNote);
    syncScrollState();
    repaint();
}

void PianoRollContainer::setData(PianoRollData* data)
{
    mData = data;
    if (mData) { mNumBars = mData->numBars; mSnapDenom = mData->snapDenominator; }
    mGrid->setData(data);
    mLane->setData(data);
    syncScrollState();
}

void PianoRollContainer::setTimeSignature(int num, int den)
{
    if (mGrid) mGrid->setTimeSignature (num, den);
}

void PianoRollContainer::setPlayheadBeat(double beat)
{
    mPlayheadBeat = beat;
    mGrid->setPlayheadBeat(beat);
}

void PianoRollContainer::applyZoom(float factor)
{
    // Viewport-relative limits: full out = 8 bars, full in = 1 bar.
    // mPPB is pixels per beat; 4 beats per bar in 4/4.
    float vpW = mGrid ? (float)jmax(1, mGrid->getWidth()) : 800.f;
    float minPPB = vpW / (4.f * 8.f);   // 8 bars fill the viewport
    float maxPPB = vpW / (4.f * 1.f);   // 1 bar fills the viewport
    mPPB = jlimit(minPPB, maxPPB, mPPB * factor);
    syncScrollState();
}

void PianoRollContainer::applyVZoom(float factor)
{
    if (mFixedRange) return;  // fixed-range mode auto-sizes, don't interfere
    // Viewport-relative limits: full out = 4 octaves (48 notes), full in = 1 octave (12 notes).
    float gridHf = mGrid ? (float)jmax(1, mGrid->getHeight()) : 600.f;
    float minScale = gridHf / (48.f * (float)PianoRollGrid::kNoteH);
    float maxScale = gridHf / (12.f * (float)PianoRollGrid::kNoteH);
    // Guard against degenerate (extremely short grid)
    if (minScale > maxScale) minScale = maxScale * 0.5f;
    mNoteHScale = jlimit(minScale, maxScale, mNoteHScale * factor);
    syncScrollState();
}

void PianoRollContainer::onHScroll(double dBeats)
{
    mBeatOff = jmax(0.0, mBeatOff + dBeats); syncScrollState();
}

void PianoRollContainer::onVScroll(int dNotes)
{
    if (mFixedRange) return;
    int noteH    = jmax(4, (int)(PianoRollGrid::kNoteH * mNoteHScale));
    int visNotes = mGrid ? (mGrid->getHeight() / noteH) : 8;
    // 2026-04-21: FL-convention range - bottom C0 (MIDI 0), top G10 (MIDI 127).
    int minTop   = visNotes;
    mTopNote = jlimit(minTop, 127, mTopNote + dNotes);
    syncScrollState();
}

void PianoRollContainer::syncScrollState()
{
    int noteH = jmax(4, (int)(PianoRollGrid::kNoteH * mNoteHScale));
    if (mFixedRange && mGrid)
    {
        // Note rows fill the space below the ruler (kRulerH px at top of grid).
        // noteToY adds kRulerH offset so row 0 starts at y=kRulerH, not y=0.
        int gridH = mGrid->getHeight();
        int range = mFixedRangeTop - mFixedRangeBottom + 1;
        if (gridH > 0 && range > 0)
            noteH = jmax(4, (gridH - PianoRollGrid::kRulerH) / range);
        mTopNote = mFixedRangeTop;
    }
    int bars = mData ? mData->numBars : mNumBars;
    mKeyboard->setScrollState(mTopNote, noteH);
    mGrid->setScrollState(mPPB, mBeatOff, mTopNote, noteH, bars, mSnapDenom);
    mLane->setScrollState(mPPB, mBeatOff);
    pushScrollStateToBars();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Scrollbar sync (helper + listener)
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollContainer::pushScrollStateToBars()
{
    if (!mGrid || !mHScroll || !mVScroll) return;
    mPushingToBars = true;

    // ── Horizontal ────────────────────────────────────────────────────────
    // Extent rule: last-note end + 1 bar (4 beats), but always at least one
    // viewport's worth of beats so the thumb has somewhere to live even when
    // the pattern is empty. Notes past pattern end still extend the scroll.
    const int    gridW         = jmax(1, mGrid->getWidth());
    const double visibleBeats0 = (double)gridW / jmax(1.f, mPPB);
    double lastNoteEnd = 0.0;
    if (mData)
    {
        for (const auto& n : mData->notes)
        {
            const double end = n.startBeat + n.durationBeats;
            if (end > lastNoteEnd) lastNoteEnd = end;
        }
    }
    constexpr double kBeatsPerBar = 4.0;
    const double totalBeats   = jmax(lastNoteEnd + kBeatsPerBar, visibleBeats0);
    const double visibleBeats = jmin(totalBeats, visibleBeats0);
    mHScroll->setRangeLimits(0.0, totalBeats);
    mHScroll->setCurrentRange(jlimit(0.0, jmax(0.0, totalBeats - visibleBeats), mBeatOff),
                              visibleBeats, juce::dontSendNotification);

    // ── Vertical (hidden in fixed-range / drum mode) ──────────────────────
    const bool vVisible = !mFixedRange;
    mVScroll->setVisible(vVisible);
    if (vVisible)
    {
        const int    noteH      = jmax(4, (int)(PianoRollGrid::kNoteH * mNoteHScale));
        const int    gridHRem   = jmax(1, mGrid->getHeight() - PianoRollGrid::kRulerH);
        const double visNotes   = (double)gridHRem / (double)noteH;
        const double totalNotes = 128.0;
        // scrollValue = 127 - mTopNote (top of grid = high MIDI = thumb near top)
        const double sv = (double)(127 - mTopNote);
        mVScroll->setRangeLimits(0.0, totalNotes);
        mVScroll->setCurrentRange(jlimit(0.0, jmax(0.0, totalNotes - visNotes), sv),
                                  visNotes, juce::dontSendNotification);
    }

    mPushingToBars = false;
}

void PianoRollContainer::scrollBarMoved(juce::ScrollBar* sb, double newStart)
{
    if (mPushingToBars || !sb) return;
    if (sb == mHScroll.get())
    {
        mBeatOff = jmax(0.0, newStart);
        syncScrollState();
    }
    else if (sb == mVScroll.get())
    {
        const int noteH    = jmax(4, (int)(PianoRollGrid::kNoteH * mNoteHScale));
        const int visNotes = mGrid ? jmax(1, (mGrid->getHeight() - PianoRollGrid::kRulerH) / noteH) : 8;
        // 2026-04-21: FL-convention range - bottom C0 (MIDI 0), top G10 (MIDI 127).
        const int minTop   = visNotes;
        mTopNote = jlimit(minTop, 127, 127 - (int)std::round(newStart));
        syncScrollState();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Context label
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollContainer::setContextLabel(const juce::String& text)
{
    if (mContextLabel) mContextLabel->setText(text, juce::dontSendNotification);
}

void PianoRollContainer::updateScaleFromUI()
{
    if (mScaleNameIdx >= 0 && mScaleNameIdx < kNumScales &&
        mScaleRootIdx >= 0 && mScaleRootIdx < 12)
        mGrid->setScale(mScaleRootIdx, kScaleDefs[mScaleNameIdx].inKey, mScaleActive);
}

// ── New public methods wired to menu bar actions ──────────────────────────────

void PianoRollContainer::transposeSelection(int semitones)
{
    if (mGrid) mGrid->transposeSelection(semitones);
}

void PianoRollContainer::scrollToPlayhead()
{
    if (mPlayheadBeat < 0.0 || !mGrid) return;
    mBeatOff = jmax(0.0, mPlayheadBeat - 1.0);
    syncScrollState();
}

void PianoRollContainer::setLaneVisible(bool v)
{
    if (mLaneVisible == v) return;
    mLaneVisible = v;
    resized();
    repaint();
}

void PianoRollContainer::setGhostsVisible(bool v)
{
    if (mGhostsVisible == v) return;
    mGhostsVisible = v;
    if (mGrid) mGrid->setGhostData(v ? mGhostStore : decltype(mGhostStore){});
}

void PianoRollContainer::setSnapDenomAndQuantize(int denom)
{
    mSnapDenom = denom;
    syncScrollState();
    if (mGrid) mGrid->toolQuantize();
}

void PianoRollContainer::setScaleActive(bool active)
{
    mScaleActive = active;
    updateScaleFromUI();
}

void PianoRollContainer::setScaleRoot(int rootIdx)
{
    mScaleRootIdx = rootIdx;
    updateScaleFromUI();
}

void PianoRollContainer::setScaleName(int nameIdx)
{
    mScaleNameIdx = nameIdx;
    updateScaleFromUI();
}

void PianoRollContainer::selectChord(int chordIdx)
{
    if (chordIdx < 0 || chordIdx >= kNumChords) return;
    mChordIdx = chordIdx;
    if (mGrid) mGrid->setStampChord(kChordDefs[chordIdx].intervals);
    setActiveTool(PianoRollGrid::PRTool::Stamp);
}

void PianoRollContainer::paint(Graphics& g)
{
    g.fillAll(VC::Bg);
    g.setColour(VC::Panel);
    g.fillRect(0, 0, getWidth(), kMenuBarH + kToolbarH);
    g.setColour(VC::Accent);
    g.drawHorizontalLine(kMenuBarH + kToolbarH - 1, 0.f, (float)getWidth());
    // (No line at mGrid->getBottom() - the H scrollbar now sits there and
    //  provides its own visual separation between grid and control lane.)
}

PianoRollContainer::~PianoRollContainer()
{
    // QA-D Task 4 (QA-0a finding #8): defensive teardown of the MenuBarComponent
    // before its model is destroyed.  Header reorders the unique_ptrs so the
    // model outlives the component naturally (reverse-declaration destruction
    // order), but the explicit setModel(nullptr) + reset() here is the
    // belt-and-suspenders that survives any future reordering or JUCE-side
    // destructor change that would otherwise re-enable the assertion-firing
    // removeListener call from juce::MenuBarComponent's dtor.
    if (mMenuBar)
    {
        mMenuBar->setModel (nullptr);
        mMenuBar.reset();
    }
}

void PianoRollContainer::resized()
{
    auto b = getLocalBounds();

    // Menu bar (20 px)
    if (mMenuBar) mMenuBar->setBounds(b.removeFromTop(kMenuBarH));

    // Single toolbar row (28 px): Wrench | Magnet | tool buttons | Undo | Redo | H
    auto row1 = b.removeFromTop(kToolbarH);
    row1.removeFromLeft(4);
    mWrenchBtn->setBounds(row1.removeFromLeft(38).reduced(2, 3));
    row1.removeFromLeft(2);
    mMagnetBtn->setBounds(row1.removeFromLeft(38).reduced(2, 3));
    row1.removeFromLeft(4);
    for (int i = 0; i < 7; ++i)   // Draw(0)..Zoom(6); Stamp(7) always hidden
        mToolBtns[i]->setBounds(row1.removeFromLeft(62).reduced(2, 3));   // 2026-04-26: 36→62 to match Builder
    row1.removeFromLeft(4);
    mUndoBtn   ->setBounds(row1.removeFromLeft(48).reduced(2, 3));        // 40→48
    mRedoBtn   ->setBounds(row1.removeFromLeft(48).reduced(2, 3));        // 40→48
    row1.removeFromLeft(2);
    mHistoryBtn->setBounds(row1.removeFromLeft(22).reduced(2, 3));
    row1.removeFromLeft(6);
    // Zoom buttons (all modes) - 2026-04-26: bumped 22→28 so the +/- glyphs fit.
    mZoomOutBtn->setBounds(row1.removeFromLeft(28).reduced(2, 3));
    mZoomInBtn ->setBounds(row1.removeFromLeft(28).reduced(2, 3));
    // Context label fills remaining row1 space (right-justified).
    if (mContextLabel)
    {
        row1.removeFromLeft(8);
        mContextLabel->setBounds(row1.reduced(4, 4));
    }

    // Compute explicit bounds so keyboard/grid/lane can never overlap.
    static constexpr int kMinGridH = 120;
    int totalH      = b.getHeight();
    int actualLaneH = mLaneVisible ? kLaneH : 0;
    int gridH       = jmax(kMinGridH, totalH - actualLaneH - kScrollBarSz);

    int bx = b.getX();
    int by = b.getY();
    int bw = b.getWidth();

    // 2026-04-26 (D-7): when M-toggled hidden, the keyboard collapses to 0
    // width and the grid + scrollbars + lane all use the freed space.
    const int kbW = mKeyboardVisible ? PianoKeyboard::kWidth : 0;

    // Grid (right of keyboard, left of V scrollbar, above H scrollbar + lane)
    const bool vVis = !mFixedRange;
    const int  vSbW = vVis ? kScrollBarSz : 0;
    mGrid->setBounds(bx + kbW, by,
                     bw - kbW - vSbW, gridH);

    // V scrollbar on right edge of grid (only outside fixed/drum mode)
    if (mVScroll)
    {
        mVScroll->setVisible(vVis);
        if (vVis)
            mVScroll->setBounds(bx + bw - kScrollBarSz, by, kScrollBarSz, gridH);
    }

    // H scrollbar immediately below grid (spans grid width)
    if (mHScroll)
        mHScroll->setBounds(bx + kbW, by + gridH,
                            bw - kbW, kScrollBarSz);

    // Keyboard (left column, below the grid's ruler so rows align with grid).
    // 2026-04-21: always offset by kRulerH (previously only in fixed-range mode)
    //   so the top MIDI row is usable - not covered by the ruler click-zone.
    mKeyboard->setVisible(mKeyboardVisible);
    if (mKeyboardVisible)
        mKeyboard->setBounds(bx, by + PianoRollGrid::kRulerH,
                             PianoKeyboard::kWidth,
                             jmax(1, gridH - PianoRollGrid::kRulerH));

    // Control lane (hidden when velocity lane is toggled off)
    // Lane sits BELOW the H scrollbar and spans full grid-area width.
    mLane->setVisible(mLaneVisible);
    if (mLaneVisible)
        mLane->setBounds(bx + kbW, by + gridH + kScrollBarSz,
                         bw - kbW, actualLaneH);
}

// ─────────────────────────────────────────────────────────────────────────────
// PianoRollGrid - Tools menu + algorithms
// ─────────────────────────────────────────────────────────────────────────────

std::vector<int> PianoRollGrid::getWorkingSet() const
{
    // 2026-04-26 (D-7): selection-only - previously fell back to "all notes
    // on the page" when nothing was highlighted, which made tools (Quantize /
    // Strum / Chop / etc.) act on freshly-drawn notes without the user ever
    // selecting them.  Now: no selection -> tools no-op.  The user must
    // explicitly select notes (drag-marquee, click, or drag a ruler time
    // range - see PianoRollGrid::mouseUp time-range release path) first.
    if (!mData || mSelection.empty()) return {};
    std::vector<int> expanded = mSelection;
    expandForGroups(expanded);
    return expanded;
}

void PianoRollGrid::showToolsMenu()
{
    PopupMenu menu;
    menu.addItem(1, "Quantize          Alt+Q");
    menu.addItem(2, "Strum             Alt+S");
    menu.addItem(3, "Arpeggiate        Alt+A");

    PopupMenu chopSub;
    chopSub.addItem(10, "Into 2  (halves)");
    chopSub.addItem(11, "Into 3  (thirds)");
    chopSub.addItem(12, "Into 4  (quarters)");
    chopSub.addItem(13, "Into 6");
    chopSub.addItem(14, "Into 8");
    menu.addSubMenu("Chop...", chopSub);

    menu.addItem(4, "Glue              Ctrl+G");
    menu.addItem(5, "Articulate        Alt+L");
    menu.addItem(6, "Randomize         Alt+R");
    menu.addSeparator();
    menu.addItem(7, "Generate Chords   Alt+P");

    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(this), [this](int r)
    {
        if      (r == 1)  toolQuantize();
        else if (r == 2)  toolStrum();
        else if (r == 3)  toolArpeggiate();
        else if (r == 4)  toolGlue();
        else if (r == 5)  toolArticulate();
        else if (r == 6)  toolRandomize();
        else if (r == 7)  toolGenerateChords();
        else if (r >= 10 && r <= 14)
        {
            constexpr int kDivs[] = { 2, 3, 4, 6, 8 };
            toolChop(kDivs[r - 10]);
        }
    });
}

void PianoRollGrid::toolQuantize()
{
    if (!mData) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Quantize");
    double snap = 4.0 / mSnapDenom;
    for (int i : targets)
        mData->notes[i].startBeat = std::round(mData->notes[i].startBeat / snap) * snap;
    auto& notes = mData->notes;
    sortNotes(notes);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void PianoRollGrid::deleteSelected()
{
    if (!mData) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Delete");
    std::sort(targets.rbegin(), targets.rend());
    for (int i : targets)
        mData->notes.erase(mData->notes.begin() + i);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void PianoRollGrid::transposeSelection(int semitones)
{
    nudgeSelection(0, semitones);
}

// ─────────────────────────────────────────────────────────────────────────────
// D-7 (2026-04-26): Smaller piano roll bundle helpers
// ─────────────────────────────────────────────────────────────────────────────

// Ctrl+Q - snap each selected note's startBeat to the nearest 1/4 note
// boundary.  Ignores the snap setting (the whole point: a fast "tighten to
// quarters" pass regardless of how fine the user is currently working).
// Selection-only: no-op when nothing is highlighted.
void PianoRollGrid::quickQuantizeQuarter()
{
    if (!mData || mSelection.empty()) return;
    std::vector<int> targets = mSelection;
    expandForGroups(targets);
    beginEdit("Quick Quantize 1/4");
    constexpr double snap = 1.0;   // 1 beat = quarter note
    for (int i : targets)
        mData->notes[i].startBeat = std::round(mData->notes[i].startBeat / snap) * snap;
    sortNotes(mData->notes);
    commitEdit();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

// Ctrl+L - extend each selected note's duration so it ends right when the
// next note (on any pitch) begins.  Notes with no follower keep their length.
// Selection-only: no-op when nothing is highlighted.
void PianoRollGrid::quickLegato()
{
    if (!mData || mSelection.empty()) return;
    std::vector<int> targets = mSelection;
    expandForGroups(targets);
    beginEdit("Legato");

    // Sort by startBeat (asc) so we can find each note's follower in O(n log n).
    std::vector<double> startBeats;
    startBeats.reserve(mData->notes.size());
    for (const auto& n : mData->notes) startBeats.push_back(n.startBeat);
    std::sort(startBeats.begin(), startBeats.end());

    for (int i : targets)
    {
        auto& n = mData->notes[i];
        // Find smallest startBeat strictly greater than this note's start.
        auto it = std::upper_bound(startBeats.begin(), startBeats.end(), n.startBeat);
        if (it == startBeats.end()) continue;
        const double nextStart = *it;
        const double newDur = nextStart - n.startBeat;
        if (newDur > 0.0) n.durationBeats = newDur;
    }
    commitEdit();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

// Alt+F - add a quick 1/32-note grace note one slot before each selected note,
// at the same pitch with reduced velocity.  Selection-only.
void PianoRollGrid::flamSelected()
{
    if (!mData || mSelection.empty()) return;
    std::vector<int> targets = mSelection;
    expandForGroups(targets);
    beginEdit("Flam");

    constexpr double kGraceLen = 4.0 / 32.0;   // 1/32 note in beats
    std::vector<PianoNote> toAdd;
    toAdd.reserve(targets.size());
    for (int i : targets)
    {
        const PianoNote& src = mData->notes[i];
        const double graceStart = src.startBeat - kGraceLen;
        if (graceStart < 0.0) continue;        // can't place before bar 0
        PianoNote g = src;
        g.startBeat     = graceStart;
        g.durationBeats = kGraceLen;
        g.velocity      = juce::jlimit(0.05f, 1.0f, src.velocity * 0.6f);
        g.groupId       = -1;                  // grace is independent
        toAdd.push_back(g);
    }
    for (auto& g : toAdd) mData->notes.push_back(g);
    sortNotes(mData->notes);
    commitEdit();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

// Ctrl+Delete - delete a time span and slide every later note left by the
// removed length.  Source for [t0, t1):
//   1. RULER time-range first (the highlighted box's width is exactly what
//      the user sees and expects to disappear - including any empty space
//      beyond the last selected note inside the range).
//   2. Note-selection fallback when no ruler range is set (e.g. user
//      marquee-selected without ever dragging on the ruler).
// Erase rule is "starts in [t0, t1)" - matches what the ruler-release
// auto-select highlights.  Distinct from plain Delete: this closes the gap
// by sliding everything past t1 to the LEFT by removedLen.
void PianoRollGrid::deleteTimeRegion()
{
    if (!mData) return;

    double t0 = 0.0, t1 = 0.0;
    bool   haveRange = false;
    if (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart)
    {
        t0 = jmin(mTimeSelBeatStart, mTimeSelBeatEnd);
        t1 = jmax(mTimeSelBeatStart, mTimeSelBeatEnd);
        haveRange = true;
    }
    else if (!mSelection.empty())
    {
        t0 =  std::numeric_limits<double>::max();
        t1 = -std::numeric_limits<double>::max();
        for (int idx : mSelection)
        {
            if (idx < 0 || idx >= (int) mData->notes.size()) continue;
            const auto& n = mData->notes[idx];
            t0 = jmin(t0, n.startBeat);
            t1 = jmax(t1, n.startBeat + n.durationBeats);
        }
        haveRange = (t1 > t0);
    }
    if (!haveRange) return;

    const double removedLen = t1 - t0;
    if (removedLen <= 0.0) return;

    beginEdit("Delete Time");

    // 1) erase notes whose START lies in [t0, t1).  Float epsilons prevent
    //    boundary notes from being missed due to snap rounding.
    constexpr double kEps = 1.0e-6;
    for (int i = (int) mData->notes.size() - 1; i >= 0; --i)
    {
        const double s = mData->notes[i].startBeat;
        if (s >= t0 - kEps && s < t1 - kEps)
            mData->notes.erase(mData->notes.begin() + i);
    }
    // 2) shift every note that starts at or after t1 left by removedLen
    for (auto& n : mData->notes)
        if (n.startBeat >= t1 - kEps) n.startBeat -= removedLen;

    sortNotes(mData->notes);
    clearSelection();
    mTimeSelBeatStart = mTimeSelBeatEnd = -1.0;
    commitEdit();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

// Ctrl+Alt+Home - flip whether note resize grabs the LEFT or RIGHT edge.
// State is kept on the grid so subsequent drags pick up the change.  No undo
// entry: this is a UI mode toggle, not an edit.
void PianoRollGrid::toggleResizeFromLeftMode()
{
    mResizeFromLeftEnabled = !mResizeFromLeftEnabled;
    if (auto* mainWin = getTopLevelComponent())
        mainWin->repaint();
}

// Ctrl+Left / Ctrl+Right helpers - shift the ruler time-selection by its
// own length without moving the notes underneath.  After the shift we re-
// populate mSelection with the notes inside the new range (mirroring the
// mouseUp time-range release auto-select rule) so the visual highlight
// stays in sync.  No-op when no ruler range is set; clamps t0 to 0.
void PianoRollGrid::shiftTimeSelectionLeft()  { shiftTimeSelectionByLength(-1); }
void PianoRollGrid::shiftTimeSelectionRight() { shiftTimeSelectionByLength(+1); }

void PianoRollGrid::shiftTimeSelectionByLength(int direction)
{
    if (!mData) return;
    if (mTimeSelBeatStart < 0.0 || mTimeSelBeatEnd <= mTimeSelBeatStart) return;

    const double t0  = jmin (mTimeSelBeatStart, mTimeSelBeatEnd);
    const double t1  = jmax (mTimeSelBeatStart, mTimeSelBeatEnd);
    const double len = t1 - t0;
    if (len <= 0.0) return;

    double newStart = t0 + (double) direction * len;
    if (newStart < 0.0) newStart = 0.0;
    const double newEnd = newStart + len;

    mTimeSelBeatStart  = newStart;
    mTimeSelBeatEnd    = newEnd;
    mTimeSelBeatAnchor = newStart;

    // Re-populate selection from the new range.
    mSelection.clear();
    for (int i = 0; i < (int) mData->notes.size(); ++i)
    {
        const auto& n = mData->notes[i];
        if (n.startBeat >= newStart && n.startBeat < newEnd)
            mSelection.push_back (i);
    }
    repaint();
}

// Alt+X - modal popup with a velocity slider + linked numeric box that
// scales the selection's velocities by a percentage (0-200 %).  Selection-
// only.  juce::Slider's built-in TextBoxRight gives both controls in one
// widget; the AlertWindow auto-deletes via enterModalState's deleteOnDismiss
// flag.  Undoable.
namespace { struct ScaleLevelsHost : public juce::Component
{
    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    ScaleLevelsHost()
    {
        slider.setRange (0.0, 200.0, 1.0);
        slider.setValue (100.0);
        slider.setTextValueSuffix (" %");
        addAndMakeVisible (slider);
        setSize (360, 32);
    }
    void resized() override { slider.setBounds (getLocalBounds()); }
}; }

void PianoRollGrid::scaleSelectionLevels()
{
    if (!mData || mSelection.empty()) return;

    auto* host = new ScaleLevelsHost();
    auto* aw = new juce::AlertWindow ("Scale Levels",
        "Scale velocities for the selection (100 % = no change).",
        juce::AlertWindow::NoIcon);
    aw->addCustomComponent (host);
    aw->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, host](int result)
        {
            if (result == 1)
            {
                const float pct = (float) host->slider.getValue();
                if (pct >= 0.0f && std::abs (pct - 100.0f) > 0.01f && mData)
                {
                    const float scale = pct / 100.0f;
                    std::vector<int> targets = mSelection;
                    expandForGroups (targets);
                    beginEdit ("Scale Levels");
                    for (int idx : targets)
                    {
                        if (idx < 0 || idx >= (int) mData->notes.size()) continue;
                        auto& n = mData->notes[idx];
                        n.velocity = juce::jlimit (0.0f, 1.0f, n.velocity * scale);
                    }
                    commitEdit();
                    if (onNotesChanged) onNotesChanged();
                    repaint();
                }
            }
            delete host;
        }), true);   // deleteOnDismiss = true → AlertWindow auto-deletes
}

void PianoRollGrid::toolChop(int divisions)
{
    if (!mData || divisions < 2) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;

    // 2026-04-26 (D-7): refuse to chop pieces smaller than a 1/16 note
    // (0.25 beat).  Past that the slices overlap into a visual blob and
    // are too small to grab.  Notes that would produce sub-1/16 pieces
    // are silently skipped; if every selected note falls under that rule
    // (e.g. trying to chop notes already AT 1/16), the call is a no-op.
    constexpr double kMinSubDur = 0.25;
    std::vector<int> chopTargets;
    chopTargets.reserve (targets.size());
    for (int idx : targets)
    {
        const double subDur = mData->notes[idx].durationBeats / divisions;
        if (subDur >= kMinSubDur - 1.0e-9)
            chopTargets.push_back (idx);
    }
    if (chopTargets.empty()) return;

    beginEdit("Chop");
    std::vector<PianoNote> toAdd;
    // Erase targets in reverse order so indices stay valid
    std::sort(chopTargets.rbegin(), chopTargets.rend());
    chopTargets.erase(std::unique(chopTargets.begin(), chopTargets.end()), chopTargets.end());
    for (int idx : chopTargets)
    {
        PianoNote orig = mData->notes[idx];
        mData->notes.erase(mData->notes.begin() + idx);
        double subDur = orig.durationBeats / divisions;
        for (int k = 0; k < divisions; ++k)
        {
            PianoNote sub = orig;
            sub.startBeat     = orig.startBeat + k * subDur;
            sub.durationBeats = subDur;
            toAdd.push_back(sub);
        }
    }
    for (auto& n : toAdd) mData->notes.push_back(n);
    sortNotes(mData->notes);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void PianoRollGrid::toolGlue()
{
    if (!mData) return;
    auto targets = getWorkingSet();
    if (targets.size() < 2) return;
    beginEdit("Glue");
    // Sort targets by pitch (desc) then startBeat
    std::sort(targets.begin(), targets.end(), [this](int a, int b) {
        const auto& na = mData->notes[a]; const auto& nb = mData->notes[b];
        if (na.midiNote != nb.midiNote) return na.midiNote > nb.midiNote;
        return na.startBeat < nb.startBeat;
    });
    std::set<int> toErase;
    for (int i = 0; i < (int)targets.size() - 1; ++i)
    {
        int ia = targets[i], ib = targets[i + 1];
        if (toErase.count(ia)) continue;
        auto& na = mData->notes[ia]; const auto& nb = mData->notes[ib];
        if (na.midiNote == nb.midiNote)
        {
            double newEnd = std::max(na.startBeat + na.durationBeats,
                                     nb.startBeat + nb.durationBeats);
            na.durationBeats = newEnd - na.startBeat;
            toErase.insert(ib);
        }
    }
    std::vector<int> eraseVec(toErase.rbegin(), toErase.rend());
    for (int idx : eraseVec) mData->notes.erase(mData->notes.begin() + idx);
    sortNotes(mData->notes);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void PianoRollGrid::toolArpeggiate()
{
    if (!mData) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Arpeggiate");
    double snap = 4.0 / mSnapDenom;
    // Group notes that share the same startBeat
    std::map<double, std::vector<int>> byBeat;
    for (int i : targets) byBeat[mData->notes[i].startBeat].push_back(i);
    for (auto& [beat, indices] : byBeat)
    {
        if ((int)indices.size() < 2) continue;
        std::sort(indices.begin(), indices.end(), [this](int a, int b) {
            return mData->notes[a].midiNote < mData->notes[b].midiNote;
        });
        double offset = 0.0;
        for (int idx : indices) { mData->notes[idx].startBeat = beat + offset; offset += snap; }
    }
    sortNotes(mData->notes);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void PianoRollGrid::toolStrum()
{
    if (!mData) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Strum");
    constexpr double kStrumStep = 1.0 / 32.0;  // 1/32 beat between strummed notes
    std::map<double, std::vector<int>> byBeat;
    for (int i : targets) byBeat[mData->notes[i].startBeat].push_back(i);
    for (auto& [beat, indices] : byBeat)
    {
        if ((int)indices.size() < 2) continue;
        std::sort(indices.begin(), indices.end(), [this](int a, int b) {
            return mData->notes[a].midiNote < mData->notes[b].midiNote;
        });
        double offset = 0.0;
        for (int idx : indices) { mData->notes[idx].startBeat = beat + offset; offset += kStrumStep; }
    }
    sortNotes(mData->notes);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void PianoRollGrid::toolRandomize()
{
    if (!mData) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Randomize");
    juce::Random rng;
    double snap = 4.0 / mSnapDenom;
    for (int i : targets)
    {
        auto& n = mData->notes[i];
        n.velocity  = jlimit(0.f, 1.f, n.velocity + (rng.nextFloat() - 0.5f) * 0.4f);
        n.startBeat = jmax(0.0, n.startBeat + (rng.nextDouble() - 0.5) * snap);
    }
    sortNotes(mData->notes);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void PianoRollGrid::toolArticulate()
{
    if (!mData) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Articulate");
    for (int i : targets)
        mData->notes[i].durationBeats = jmax(1.0 / 64.0, mData->notes[i].durationBeats * 0.8);
    commitEdit();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void PianoRollGrid::toolGenerateChords()
{
    if (!mData) return;
    beginEdit("Generate Chords");

    // Determine time region from selection, or default to 4 beats
    double startBeat = 0.0, endBeat = 4.0;
    if (!mSelection.empty())
    {
        startBeat = mData->notes[mSelection[0]].startBeat;
        endBeat   = startBeat;
        for (int i : mSelection)
        {
            startBeat = std::min(startBeat, mData->notes[i].startBeat);
            endBeat   = std::max(endBeat,   mData->notes[i].startBeat + mData->notes[i].durationBeats);
        }
    }

    double segLen  = (endBeat - startBeat) / 4.0;
    double noteDur = jmax(0.125, segLen * 0.9);

    // ── Scale-aware harmonization ─────────────────────────────────────────
    // Build sorted list of in-scale pitch classes (absolute, 0-11)
    std::vector<int> scaleNotes;
    if (mScaleActive)
    {
        for (int pc = 0; pc < 12; ++pc)
            if (mScaleInKey[pc]) scaleNotes.push_back(pc);
    }

    // Chord degree roots: I–IV–V–I (scale degrees 0, 3, 4, 0)
    static const int kDegrees[4] = { 0, 3, 4, 0 };

    for (int c = 0; c < 4; ++c)
    {
        double beatStart = startBeat + c * segLen;

        if (mScaleActive && scaleNotes.size() >= 3)
        {
            // Scale-aware: harmonize in thirds from the chosen degree
            int scaleLen  = (int)scaleNotes.size();
            int degRoot   = kDegrees[c] % scaleLen;
            int deg3rd    = (degRoot + 2) % scaleLen;
            int deg5th    = (degRoot + 4) % scaleLen;

            // Convert scale pitch classes to MIDI notes in a sensible octave
            auto pcToMidi = [&](int degIdx) -> int {
                int pc = scaleNotes[degIdx];
                // Put the chord root in the C4-B4 range, thirds/fifths may go above
                int midi = 60 + ((pc - mScaleRoot + 12) % 12);
                return midi;
            };

            int rootMidi = pcToMidi(degRoot);
            int thrdMidi = pcToMidi(deg3rd);
            int fifthMidi = pcToMidi(deg5th);

            // Ensure third and fifth are above root
            while (thrdMidi  <= rootMidi)  thrdMidi  += 12;
            while (fifthMidi <= thrdMidi)   fifthMidi += 12;

            // Keep in playable range (C3-C6)
            while (rootMidi  < 48)  { rootMidi += 12; thrdMidi += 12; fifthMidi += 12; }
            while (fifthMidi > 84)  { rootMidi -= 12; thrdMidi -= 12; fifthMidi -= 12; }

            for (int midi : { rootMidi, thrdMidi, fifthMidi })
            {
                PianoNote note;
                note.midiNote      = jlimit(0, 127, midi);
                note.startBeat     = beatStart;
                note.durationBeats = noteDur;
                note.velocity      = 0.75f;
                mData->notes.push_back(note);
            }
        }
        else
        {
            // No scale active - fall back to I–V–vi–IV in C major
            static const int kRoots[4]     = { 0, 7, 9, 5 };
            static const int kShapes[4][3] = { {0,4,7},{0,4,7},{0,3,7},{0,4,7} };
            int root      = mScaleActive ? mScaleRoot : 0;
            int chordRoot = root + kRoots[c];
            for (int n = 0; n < 3; ++n)
            {
                PianoNote note;
                note.midiNote      = 60 + chordRoot + kShapes[c][n];
                while (note.midiNote > 76) note.midiNote -= 12;
                while (note.midiNote < 48) note.midiNote += 12;
                note.startBeat     = beatStart;
                note.durationBeats = noteDur;
                note.velocity      = 0.75f;
                mData->notes.push_back(note);
            }
        }
    }

    sortNotes(mData->notes);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

// =============================================================================
// PianoRollMenuBar
// =============================================================================

juce::StringArray PianoRollMenuBar::getMenuBarNames()
{
    return { "Edit", "Tools", "Scale", "Chords", "View" };
}

juce::PopupMenu PianoRollMenuBar::getMenuForIndex(int idx, const juce::String&)
{
    juce::PopupMenu menu;

    if (idx == 0) // ── Edit ──────────────────────────────────────────────────
    {
        menu.addItem(1, "Select All\tCtrl+A");
        menu.addItem(2, "Deselect");
        menu.addSeparator();
        menu.addItem(3, "Copy\tCtrl+C");
        menu.addItem(4, "Paste\tCtrl+V");
        menu.addItem(5, "Delete");
        menu.addItem(6, "Duplicate\tCtrl+B");
        menu.addSeparator();

        juce::PopupMenu quantSub;
        quantSub.addItem(101, "1/4");
        quantSub.addItem(102, "1/8");
        quantSub.addItem(103, "1/16");
        quantSub.addItem(104, "1/32");
        menu.addSubMenu("Quantize", quantSub);

        menu.addSeparator();
        menu.addItem(7,  "Transpose Up\tShift+\xe2\x86\x91");
        menu.addItem(8,  "Transpose Down\tShift+\xe2\x86\x93");
        menu.addItem(9,  "Transpose Up Octave\tShift+Ctrl+\xe2\x86\x91");
        menu.addItem(10, "Transpose Down Octave\tShift+Ctrl+\xe2\x86\x93");
    }
    else if (idx == 1) // ── Tools ─────────────────────────────────────────────
    {
        using T = PianoRollGrid::PRTool;
        auto addTool = [&](int id, const juce::String& label, T tool)
        {
            menu.addItem(id, label, true, mOwner.getActiveTool() == tool);
        };
        addTool(21, "Draw\tP",       T::Draw);
        addTool(22, "Paint\tB",      T::Paint);
        addTool(23, "Delete\tD",     T::Delete);
        addTool(24, "Mute\tT",       T::Mute);
        addTool(25, "Slice\tC",      T::Slice);
        addTool(26, "Select\tE",     T::Select);
        addTool(27, "Zoom\tShift+Z", T::Zoom);
    }
    else if (idx == 2) // ── Scale ─────────────────────────────────────────────
    {
        menu.addItem(28, "Snap to Scale", true, mOwner.isScaleActive());
        menu.addSeparator();

        static const char* kNoteNames[] = {
            "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        juce::PopupMenu rootSub;
        for (int i = 0; i < 12; ++i)
            rootSub.addItem(201 + i, kNoteNames[i], true,
                            mOwner.getScaleRootIdx() == i);
        menu.addSubMenu("Root", rootSub);

        juce::PopupMenu scaleSub;
        for (int i = 0; i < kNumScales; ++i)
            scaleSub.addItem(301 + i, kScaleDefs[i].name, true,
                             mOwner.getScaleNameIdx() == i);
        menu.addSubMenu("Scale", scaleSub);
    }
    else if (idx == 3) // ── Chords ────────────────────────────────────────────
    {
        for (int i = 0; i < kNumChords; ++i)
            menu.addItem(401 + i, kChordDefs[i].name, true,
                         mOwner.getChordIdx() == i);
    }
    else if (idx == 4) // ── View ──────────────────────────────────────────────
    {
        menu.addItem(51, "Zoom In");
        menu.addItem(52, "Zoom Out");
        menu.addItem(53, "Zoom In Vertical");
        menu.addItem(54, "Zoom Out Vertical");
        menu.addSeparator();
        menu.addItem(55, "Scroll to Playhead");
        menu.addSeparator();
        menu.addItem(56, "Ghost Notes",   true, mOwner.isGhostsVisible());
        menu.addItem(57, "Velocity Lane", true, mOwner.isLaneVisible());
    }

    return menu;
}

void PianoRollMenuBar::menuItemSelected(int id, int)
{
    auto& o = mOwner;

    // ── Edit ──────────────────────────────────────────────────────────────
    if      (id == 1) { if (auto* g = o.mGrid.get()) g->selectAll(); }
    else if (id == 2) { if (auto* g = o.mGrid.get()) g->clearSelection(); }
    else if (id == 3) { if (auto* g = o.mGrid.get()) g->copySelected(); }
    else if (id == 4) { if (auto* g = o.mGrid.get()) g->pasteClipboard(); }
    else if (id == 5) { if (auto* g = o.mGrid.get()) g->deleteSelected(); }
    else if (id == 6) { if (auto* g = o.mGrid.get()) g->duplicateSelected(); }
    else if (id == 7) { o.transposeSelection(+1); }
    else if (id == 8) { o.transposeSelection(-1); }
    else if (id == 9) { o.transposeSelection(+12); }
    else if (id == 10){ o.transposeSelection(-12); }
    // Quantize submenu
    else if (id >= 101 && id <= 104)
    {
        constexpr int kDenoms[] = { 4, 8, 16, 32 };
        o.setSnapDenomAndQuantize(kDenoms[id - 101]);
    }
    // ── Tools ─────────────────────────────────────────────────────────────
    else if (id >= 21 && id <= 27)
    {
        using T = PianoRollGrid::PRTool;
        constexpr T kTools[] = {
            T::Draw, T::Paint, T::Delete, T::Mute, T::Slice, T::Select, T::Zoom };
        o.setActiveTool(kTools[id - 21]);
    }
    // ── Scale root ────────────────────────────────────────────────────────
    else if (id >= 201 && id <= 212)  { o.setScaleRoot(id - 201); }
    // ── Scale name ────────────────────────────────────────────────────────
    else if (id >= 301 && id < 301 + kNumScales) { o.setScaleName(id - 301); }
    // ── Scale toggle ──────────────────────────────────────────────────────
    else if (id == 28) { o.setScaleActive(!o.isScaleActive()); }
    // ── Chords ────────────────────────────────────────────────────────────
    else if (id >= 401 && id < 401 + kNumChords) { o.selectChord(id - 401); }
    // ── View ──────────────────────────────────────────────────────────────
    else if (id == 51) { o.applyZoom(1.3f); }
    else if (id == 52) { o.applyZoom(1.f / 1.3f); }
    else if (id == 53) { o.applyVZoom(1.3f); }
    else if (id == 54) { o.applyVZoom(1.f / 1.3f); }
    else if (id == 55) { o.scrollToPlayhead(); }
    else if (id == 56) { o.setGhostsVisible(!o.isGhostsVisible()); }
    else if (id == 57) { o.setLaneVisible(!o.isLaneVisible()); }

    // Return focus to grid after any menu action
    if (auto* g = o.mGrid.get()) g->grabKeyboardFocus();
}
