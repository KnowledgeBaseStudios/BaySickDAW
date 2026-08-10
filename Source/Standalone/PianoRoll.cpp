#include "PianoRoll.h"
#include "TypingKeyboardMap.h"   // D-4: bypass tool keys while typing-keyboard mode is on
#include "../G3PlayheadDiag.h"   // [G3 PLAYHEAD] G-9 reading (QA-G3Smoke Task 1); Debug-only
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
// QA-Chords (2026-07-08): each chord carries a literal semitone shape (Mode 2
// input - snapped into the scale when Snap-to-Scale is ON) AND a scale-degree
// template (Mode 1 input - stacked on the roll's Root+Scale when Snap is OFF,
// so the clicked degree's natural quality emerges from the scale; that is why
// Major/Minor/Dim/Aug share the {0,2,4} triad template).
struct ChordDef { const char* name; std::vector<int> intervals; std::vector<int> degrees; };

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
    { "Major",       {0, 4, 7},          {0, 2, 4}       },
    { "Minor",       {0, 3, 7},          {0, 2, 4}       },
    { "Dim",         {0, 3, 6},          {0, 2, 4}       },
    { "Aug",         {0, 4, 8},          {0, 2, 4}       },
    { "Sus2",        {0, 2, 7},          {0, 1, 4}       },
    { "Sus4",        {0, 5, 7},          {0, 3, 4}       },
    { "Major 7",     {0, 4, 7, 11},      {0, 2, 4, 6}    },
    { "Minor 7",     {0, 3, 7, 10},      {0, 2, 4, 6}    },
    { "Dom 7",       {0, 4, 7, 10},      {0, 2, 4, 6}    },
    { "Half-Dim 7",  {0, 3, 6, 10},      {0, 2, 4, 6}    },
    { "Dim 7",       {0, 3, 6,  9},      {0, 2, 4, 6}    },
    { "Major 9",     {0, 4, 7, 11, 14},  {0, 2, 4, 6, 8} },
    { "Minor 9",     {0, 3, 7, 10, 14},  {0, 2, 4, 6, 8} },
    { "Add 9",       {0, 4, 7, 14},      {0, 2, 4, 8}    },
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

void PianoKeyboard::setKeyswitchLabelProvider(std::function<juce::String(int)> provider)
{
    mKeyswitchLabelProvider = std::move(provider);
    repaint();
}

void PianoKeyboard::setAllKeysWhiteMode(bool enabled)
{
    if (mAllKeysWhite == enabled) return;
    mAllKeysWhite = enabled;
    repaint();
}

void PianoKeyboard::setLiveHeldNotes(uint64_t lo, uint64_t hi)
{
    if (lo == mLiveHeldLo && hi == mLiveHeldHi) return;   // no change -> no repaint
    mLiveHeldLo = lo;
    mLiveHeldHi = hi;
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
    // QA-SfzGroup Sub-Q: keyswitch label takes priority over the regular note
    // label, so hovering over a keyswitch key shows "C6 Sustain" etc.
    if (mKeyswitchLabelProvider)
    {
        const auto kl = mKeyswitchLabelProvider(mHoverNote);
        if (kl.isNotEmpty())
            return kl;
    }
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
            bool highlighted = (note == mPreviewNote) || isLiveHeld(note);

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
        // Live-note monitor + mouse preview share the same "hot" highlight so a
        // note played on a hardware keyboard lights up like a clicked key.
        const bool hot = (note == mPreviewNote) || isLiveHeld (note);

        // QA-SfzGroup Sub-Q (2026-05-27): keyswitch key takes priority - render
        // with amber-highlighted full-width background + label, regardless of
        // black/white pitch class.  Allows users to visually discover which
        // notes are keyswitches (rather than playable notes) on an SFZ piano
        // roll loaded with sw_lokey/sw_hikey + sw_label opcodes (e.g. Tuba-KS
        // shows "C6 Sustain" on C7 and "C#6 Staccato" on C#7 - file's IPN
        // c6/c#6 -> MIDI 84/85 -> app-C7/C#7 in FL display convention).
        juce::String keyswitchLabel;
        if (mKeyswitchLabelProvider)
            keyswitchLabel = mKeyswitchLabelProvider(note);
        if (keyswitchLabel.isNotEmpty())
        {
            const juce::Colour amber = hot
                ? juce::Colour(0xfff5d690)    // brighter when previewed / held
                : juce::Colour(0xffe8c060);   // amber for keyswitch resting
            g.setColour(amber);
            g.fillRect(0, y, bw, mNoteH - 1);
            g.setColour(juce::Colour(0xff5a4010));
            g.drawHorizontalLine(y + mNoteH - 1, 0, (float)bw);
            if (mNoteH >= 8)
            {
                g.setColour(juce::Colour(0xff3a2810));
                g.setFont(juce::Font(juce::jmin(10.f, (float)(mNoteH - 2)), juce::Font::bold));
                g.drawText(keyswitchLabel, 4, y, bw - 6, mNoteH - 1,
                           juce::Justification::centredLeft, true);
            }
            continue;
        }

        // J-7b: in all-keys-white mode (BaySickRustyDrums), paint every row
        // as a full-width white key so engine labels are legible regardless
        // of pitch class.  Skip the black-key strip rendering entirely.
        const bool paintAsBlack = ! mAllKeysWhite && isBlackKey(note);

        if (paintAsBlack)
        {
            g.setColour(hot ? VC::Highlight.brighter() : Colour(0xff181820));
            g.fillRect(0, y, bw * 7 / 10, mNoteH);
            g.setColour(VC::Accent);
            g.drawRect(0, y, bw * 7 / 10, mNoteH, 1);
        }
        else
        {
            g.setColour(hot ? VC::Highlight.brighter() : Colour(0xffe8e8f0));
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
    // QA-H Task 7 (MIDI-01): Ctrl+click = select the pitch row, no audition.
    if (e.mods.isCtrlDown())
    {
        if (onCtrlClickPitch) onCtrlClickPitch (yToNote (e.y));
        return;
    }
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
                                   int noteH, int numBars)
{
    mPPB = ppb; mBeatOff = beatOff; mTopNote = topNote;
    mNoteH = noteH; mNumBars = numBars;
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

void PianoRollGrid::setPlayheadBeat(double beat)
{
    if (beat == mPlayhead) return;
    // Residual (b) fix (Jeff, 2026-07-24): repaint only the old + new marker
    // columns (1-px mast + 8-px right-hanging flag, padded to 14 px) instead
    // of the full grid -- the 30 Hz full-canvas repaint per tick WAS the
    // playhead's entire paint cost, and dirty-rects make 60 Hz free.
    const int oldX = mPlayhead >= 0.0 ? beatToX(mPlayhead) : INT_MIN;
    mPlayhead = beat;
    const int newX = beat >= 0.0 ? beatToX(beat) : INT_MIN;
    if (oldX != INT_MIN) repaint(oldX - 2, 0, 14, getHeight());
    if (newX != INT_MIN) repaint(newX - 2, 0, 14, getHeight());
}

// ─────────────────────────────────────────────────────────────────────────────
// Scale snap
// ─────────────────────────────────────────────────────────────────────────────
void PianoRollGrid::setScale(int root, const std::array<bool,12>& relIntervals, bool active)
{
    mScaleRoot   = root;
    mScaleActive = active;
    mScaleInKey.fill(false);
    // QA-Chords: the table fills regardless of `active` - Mode 1 stamping
    // consumes Root+Scale with Snap-to-Scale OFF.  Every snapping BEHAVIOR
    // (snapPitchToScale / nextScalePitch / move-snap / transpose / row tint)
    // stays gated on mScaleActive.
    for (int i = 0; i < 12; ++i)
        if (relIntervals[i]) mScaleInKey[(root + i) % 12] = true;
    repaint();
}

void PianoRollGrid::setStampChord(const std::vector<int>& intervals,
                                  const std::vector<int>& degrees)
{
    mStampIntervals = intervals;
    mStampDegrees   = degrees;
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
std::vector<int> PianoRollGrid::resolveStampNotes(int clickedNote) const
{
    std::vector<int> out;
    if (mStampIntervals.empty()) return out;

    auto contains = [&out](int n) { return std::find(out.begin(), out.end(), n) != out.end(); };

    if (mScaleActive)
    {
        // Mode 2 (Snap-to-Scale ON): literal shape -> strict per-note scale
        // compliance -> octave-collision resolver.  A duplicate produced by
        // snapping jumps an octave up (then re-snaps); if a dense stack still
        // collides it walks up degree-by-degree.  Thickness preserved - notes
        // are never merged or dropped (except off the top of MIDI range).
        const int rootNote = snapPitchToScale(clickedNote);
        for (int interval : mStampIntervals)
        {
            int mn = snapPitchToScale(jlimit(0, 127, rootNote + interval));
            if (contains(mn)) mn = snapPitchToScale(jlimit(0, 127, mn + 12));
            while (contains(mn) && mn < 127) mn = nextScalePitch(mn, +1);
            if (!contains(mn)) out.push_back(mn);
        }
        return out;
    }

    // Mode 1 (Snap-to-Scale OFF): stack the chord's DEGREE template through
    // the roll's Root+Scale at the clicked note - the degree's natural
    // quality comes from the scale.  Chromatic (all 12 in key) would turn
    // degree offsets into a tone cluster, so it - and any missing table -
    // falls back to the literal shape (pre-QA-Chords behavior).
    std::vector<int> scalePCs;
    for (int pc = 0; pc < 12; ++pc)
        if (mScaleInKey[pc]) scalePCs.push_back(pc);

    if (scalePCs.empty() || (int) scalePCs.size() >= 12 || mStampDegrees.empty())
    {
        for (int interval : mStampIntervals)
        {
            const int mn = jlimit(0, 127, clickedNote + interval);
            if (!contains(mn)) out.push_back(mn);
        }
        return out;
    }

    // Nearest in-scale root - same outward search as snapPitchToScale but
    // ungated (mScaleActive is false in this branch by definition).
    int root = clickedNote;
    if (! mScaleInKey[((root % 12) + 12) % 12])
    {
        for (int dist = 1; dist <= 6; ++dist)
        {
            const int up = jlimit(0, 127, clickedNote + dist);
            const int dn = jlimit(0, 127, clickedNote - dist);
            if (mScaleInKey[up % 12]) { root = up; break; }
            if (mScaleInKey[dn % 12]) { root = dn; break; }
        }
    }

    const int scaleLen = (int) scalePCs.size();
    const int rootPc   = ((root % 12) + 12) % 12;
    int degIdx = 0;
    for (int i = 0; i < scaleLen; ++i)
        if (scalePCs[(size_t) i] == rootPc) { degIdx = i; break; }

    int prev = root - 1;
    for (int off : mStampDegrees)
    {
        const int d  = degIdx + off;
        const int pc = scalePCs[(size_t)(d % scaleLen)];
        int note = (root / 12) * 12 + pc + 12 * (d / scaleLen);
        while (note <= prev) note += 12;    // enforce a strictly ascending stack
        if (note > 127) break;
        out.push_back(note);
        prev = note;
    }
    return out;
}

void PianoRollGrid::stampChordAt(int x, int y)
{
    if (!mData) return;
    const auto chord = resolveStampNotes(yToNote(y));
    if (chord.empty()) return;
    beginEdit("Stamp Chord");
    double beat = snapBeat(xToBeat(x));
    double dur  = snapUnitBeats();
    std::vector<std::pair<double,int>> newKeys;
    for (int mn : chord)
    {
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

// Mouse-mapping accuracy (LDT-394): xToBeat samples the CENTER of pixel
// column x (the old left-edge form biased every click a half-pixel early);
// beatToX rounds instead of truncating (truncation also rounds toward zero
// for off-view-left geometry -- 1 px the wrong way); yToNote floors instead
// of int-dividing (int division truncates toward zero, so clicks in a
// partial top row mapped one row low).
double PianoRollGrid::xToBeat(int x)      const { return mBeatOff + ((double)x + 0.5) / mPPB; }
int    PianoRollGrid::beatToX(double beat) const { return (int) std::llround((beat - mBeatOff) * mPPB); }
int    PianoRollGrid::noteToY(int note)    const { return mNoteYOffset + (mTopNote - note) * mNoteH; }
int    PianoRollGrid::yToNote(int y)       const
{
    const int n = mTopNote
                - (int) std::floor ((double)(y - mNoteYOffset) / jmax(1, mNoteH));
    if (mIsFixedRange) return jlimit(mFixedRangeBottom, mFixedRangeTop, n);
    return jlimit(0, 127, n);
}

double PianoRollGrid::snapBeat(double beat) const
{
    // QA-Ee Stage 3: tick-based snap on the global Unified_PianoRollSnapDiv.
    // div 0 = Off, 1 = Line (finest visible rung at this zoom), 2..10 = fixed
    // divisions.  Tick-exact -> triplets land precisely (no 1/3 float drift).
    const int div = onGetSnapDiv ? onGetSnapDiv() : 1;
    if (div <= 0) return beat;   // div 0 = Off (the single GLOBAL on/off)
    const int divTicks = (div == 1) ? dynamicSnapTicks ((double) mPPB * 4.0, kMinGridLinePx)
                                    : snapDivToTicks (div);
    if (divTicks <= 0) return beat;
    const double t = beat * (double) kTicksPerBeat;
    return std::round (t / (double) divTicks) * (double) divTicks / (double) kTicksPerBeat;
}

// QA-Ee Stage 3: current snap unit in beats (new-note length + tool spacing).
// Off -> 1/16; Line -> the finest visible rung.
double PianoRollGrid::snapUnitBeats() const
{
    const int div = onGetSnapDiv ? onGetSnapDiv() : 1;
    int divTicks = (div == 1) ? dynamicSnapTicks ((double) mPPB * 4.0, kMinGridLinePx)
                 : (div >= 2) ? snapDivToTicks (div)
                 : kTicksPerBeat / 4;                       // Off -> 1/16 note (24 ticks)
    if (divTicks <= 0) divTicks = kTicksPerBeat / 4;
    return (double) divTicks / (double) kTicksPerBeat;
}

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
void PianoRollGrid::cycleNewNoteType()
{
    switch (mNewNoteType)
    {
        case NoteType::Standard:    mNewNoteType = NoteType::RampSlide;   break;
        case NoteType::RampSlide:   mNewNoteType = NoteType::RetrigSlide; break;
        // #10 (QA-G3Smoke): the S-cycle enters Bend where Bend exists as a UI
        // concept (engine-aware rolls = Guitars/Basses); elsewhere it wraps to
        // Standard as before.
        case NoteType::RetrigSlide: mNewNoteType = NoteType::Portamento;  break;
        case NoteType::Portamento:
            mNewNoteType = (mNoteEditContextProvider && mNoteEditContextProvider().engineAware)
                               ? NoteType::Bend : NoteType::Standard;
            break;
        case NoteType::Bend:        mNewNoteType = NoteType::Standard;    break;
    }
    if (onNoteTypeArmChanged) onNoteTypeArmChanged();
}

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

void PianoRollGrid::selectAllAtPitch (int midiNote)
{
    if (!mData) return;
    for (int i = 0; i < (int) mData->notes.size(); ++i)
        if (mData->notes[(size_t) i].midiNote == midiNote
            && ! isNoteIndexSelected (i))
            mSelection.push_back (i);
    repaint();
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
    double snap = snapUnitBeats();
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

    // B-2: the slice is the DRAWN SEGMENT, not an infinite line.  Only notes whose
    // vertical center lies within the segment's y-span [loY, hiY] are candidates
    // (t in [0,1]); the X is then interpolated, never extrapolated past an endpoint.
    const int loY = juce::jmin (start.y, end.y);
    const int hiY = juce::jmax (start.y, end.y);

    for (int i = 0; i < (int)mData->notes.size(); ++i)
    {
        const auto& n = mData->notes[i];
        int x1 = beatToX(n.startBeat);
        int x2 = beatToX(n.startBeat + n.durationBeats);
        int cy = noteToY(n.midiNote) + mNoteH / 2;   // vertical centre of note row

        if (cy < loY || cy > hiY) continue;          // outside the drawn segment

        // Interpolate line X at this note's Y (cy is within [loY,hiY] so t in [0,1]).
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
            double minDur    = 1.0 / (double) kTicksPerBeat;  // QA-Ee Stage 3: min slice fragment = 1 tick
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

// QA-Chords (2026-07-08): shared resize-gesture setup for the Draw + Select
// entry points.  Grabbing a selected note's edge resizes the WHOLE selection
// (group-expanded, matching Move); grabbing an unselected note resizes just
// it - byte-identical to the old single-note behavior.
void PianoRollGrid::beginResizeGesture(int grabbedIdx)
{
    beginEdit("Resize");
    mResizing         = true;
    mResizingFromLeft = mResizeFromLeftEnabled;
    mResizeNoteIdx    = grabbedIdx;
    mResizeOrigDur    = mData->notes[grabbedIdx].durationBeats;
    mResizeOrigStart  = mData->notes[grabbedIdx].startBeat;

    mResizeIndices.clear();
    if (isSelected(grabbedIdx) && mSelection.size() > 1)
        mResizeIndices = mSelection;
    else
        mResizeIndices.push_back(grabbedIdx);
    expandForGroups(mResizeIndices);

    mResizeOrigDurs.clear();
    mResizeOrigStarts.clear();
    for (int idx : mResizeIndices)
    {
        mResizeOrigDurs  .push_back(mData->notes[idx].durationBeats);
        mResizeOrigStarts.push_back(mData->notes[idx].startBeat);
    }
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
    // D-4: while typing-keyboard mode is on, mapped note keys (and the PgUp/
    // PgDn octave shift) must bubble up to StandaloneEditor's converter
    // instead of firing the single-letter tool shortcuts below.
    if (TypingKeyboardMap::shouldBypassLocalKeys (key)) return false;

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

        if (key.getKeyCode() == 'S' || key.getKeyCode() == 's')
        {
            cycleNewNoteType();
            if (!mSelection.empty() && mData)
            {
                bool anyChange = false;
                for (int idx : mSelection)
                    if (mData->notes[idx].type != mNewNoteType) { anyChange = true; break; }
                if (anyChange)
                {
                    beginEdit("Change Type");
                    for (int idx : mSelection) mData->notes[idx].type = mNewNoteType;
                    commitEdit();
                    if (onNotesChanged) onNotesChanged();
                    repaint();
                }
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
        else if (kc == 'e') { toolRiffMachine();   return true; }   // D-6
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
// ── QA-H Task 2: Note Properties popup (double-left-click a note) ────────────
// S-10: a BPM-box-style numeric type-in field with a double-click-to-default
// hook (a plain TextEditor's double-click does word-select, useless for a single
// number; we repurpose it as the S-8 reset gesture, matching the sliders).
struct NoteNumberBox : public juce::TextEditor
{
    std::function<void()> onDoubleClickReset;
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (onDoubleClickReset && isEnabled()) onDoubleClickReset();
        else juce::TextEditor::mouseDoubleClick (e);
    }
};

// CallOutBox content editing the per-note fields (type + velocity, release,
// fine pitch, panning, filter cutoff, resonance, + Porta length).  Edits apply
// live to every target note; the FIRST change lazily opens one grid undo edit and
// the panel destructor (box dismissal) commits it, so a popup session is a single
// undo step - and an untouched popup registers no undo entry at all.
class NotePropsPanel : public juce::Component
{
public:
    using NoteEditContext = PianoRollGrid::NoteEditContext;

    NotePropsPanel (PianoRollGrid& grid, PianoRollData* data,
                    std::vector<int> targets, int anchorIdx, NoteEditContext ctx)
        : mGrid (&grid), mData (data), mTargets (std::move (targets)), mCtx (ctx)
    {
        const PianoNote& src = mData->notes[(size_t) anchorIdx];

        auto addTypeBtn = [this] (std::unique_ptr<TextButton>& b,
                                  const char* name, NoteType t)
        {
            b = std::make_unique<TextButton> (name);
            b->onClick = [this, t] { applyType (t); };
            addAndMakeVisible (*b);
        };

        // Velocity is the one per-note expression control usable on every engine;
        // the other 5 sliders + Porta box are in-house-engine only (QA-SlideSampler
        // Task 4: the sfizz Guitars/Basses patches don't map those CCs).
        auto addRow = [this] (int row, const char* name, double lo, double hi,
                              double init, double dflt, const char* suffix,
                              std::function<void (PianoNote&, float)> apply)
        {
            auto& lbl = mLabels[row];
            lbl = std::make_unique<Label> (String(), name);
            lbl->setFont (Font (12.0f));
            addAndMakeVisible (*lbl);

            auto& sl = mSliders[row];
            sl = std::make_unique<Slider> (Slider::LinearHorizontal, Slider::TextBoxRight);
            sl->setRange (lo, hi, 1.0);
            sl->setValue (init, dontSendNotification);
            sl->setDoubleClickReturnValue (true, dflt);
            sl->setTextValueSuffix (suffix);
            sl->setTextBoxStyle (Slider::TextBoxRight, false, 52, 18);
            sl->onValueChange = [this, apply, s = sl.get()] {
                const float v = (float) s->getValue();
                applyToTargets ([&] (PianoNote& n) { apply (n, v); });
            };
            addAndMakeVisible (*sl);
        };
        addRow (0, "Velocity", 0, 100, src.velocity * 100.0, 80, " %",
                [] (PianoNote& n, float v) { n.velocity = v / 100.0f; });

        if (mCtx.engineAware)
        {
            // ── Guitars/Basses: Flat / RP Slide / Bend + the gated Bend dropdowns.
            addTypeBtn (mTypeNormal,  "Flat",     NoteType::Standard);
            addTypeBtn (mTypeRpSlide, "RP Slide", NoteType::RampSlide);
            addTypeBtn (mTypeBend,    "Bend",     NoteType::Bend);

            mBendAmtLabel = std::make_unique<Label> (String(), "Bend");
            mBendAmtLabel->setFont (Font (12.0f));
            addAndMakeVisible (*mBendAmtLabel);
            mBendAmtCombo = std::make_unique<ComboBox>();
            // Item id = semitones + 100 (so a signed value survives id 1..).  Range
            // gated to the patch: guitar +3/0 (up-only), bass +2/+2.  No 0 entry.
            for (int s = mCtx.bendUpSemis; s >= 1; --s)
                mBendAmtCombo->addItem ("+" + String (s) + " st",  s + 100);
            for (int s = 1; s <= mCtx.bendDownSemis; ++s)
                mBendAmtCombo->addItem ("-" + String (s) + " st", -s + 100);
            mBendAmtCombo->onChange = [this] {
                const int id = mBendAmtCombo->getSelectedId();
                if (id == 0) return;
                const double semis = (double) (id - 100);
                applyToTargets ([semis] (PianoNote& n) { n.bendSemitones = semis; });
            };
            addAndMakeVisible (*mBendAmtCombo);
            int amtId = (int) std::lround (src.bendSemitones) + 100;
            if (mBendAmtCombo->indexOfItemId (amtId) < 0)                 // default to the smallest up-bend
                amtId = (mCtx.bendUpSemis >= 1 ? 1 + 100 : -1 + 100);
            mBendAmtCombo->setSelectedId (amtId, dontSendNotification);

            mBendShapeLabel = std::make_unique<Label> (String(), "Shape");
            mBendShapeLabel->setFont (Font (12.0f));
            addAndMakeVisible (*mBendShapeLabel);
            mBendShapeCombo = std::make_unique<ComboBox>();
            mBendShapeCombo->addItem ("Ramp + Hold",  1);   // BendShape::RampHold
            mBendShapeCombo->addItem ("Ramp (whole)", 2);   // RampWhole
            mBendShapeCombo->addItem ("Up + Back",    3);   // UpBack
            mBendShapeCombo->addItem ("Instant",      4);   // InstantHold
            mBendShapeCombo->onChange = [this] {
                const int sh = mBendShapeCombo->getSelectedId() - 1;
                if (sh < 0) return;
                applyToTargets ([sh] (PianoNote& n) { n.bendShape = (BendShape) sh; });
            };
            addAndMakeVisible (*mBendShapeCombo);
            mBendShapeCombo->setSelectedId ((int) src.bendShape + 1, dontSendNotification);

            mNotice = std::make_unique<Label> (String(),
                "Note: RP Slide and Bend move every playing note together, not just "
                "one - great for solos, not for chord bends.");
            mNotice->setFont (Font (11.0f));
            mNotice->setJustificationType (Justification::topLeft);
            mNotice->setColour (Label::textColourId, Colour (0xFFF0C060));
            addAndMakeVisible (*mNotice);
        }
        else
        {
            // ── In-house engines: the full existing panel.
            addTypeBtn (mTypeNormal,  "Flat",     NoteType::Standard);
            addTypeBtn (mTypeRpSlide, "RP Slide", NoteType::RampSlide);
            addTypeBtn (mTypeRtSlide, "RT Slide", NoteType::RetrigSlide);
            addTypeBtn (mTypePorta,   "Porta",    NoteType::Portamento);

            addRow (1, "Release",       0, 100, src.releaseAmt   * 100.0, 50, " %",
                    [] (PianoNote& n, float v) { n.releaseAmt   = v / 100.0f; });
            addRow (2, "Fine Pitch", -100, 100, src.finePitch    * 100.0,  0, " ct",
                    [] (PianoNote& n, float v) { n.finePitch    = v / 100.0f; });
            addRow (3, "Panning",    -100, 100, src.panning      * 100.0,  0, " %",
                    [] (PianoNote& n, float v) { n.panning      = v / 100.0f; });
            addRow (4, "Filter Cutoff", 0, 100, src.filterCutoff * 100.0, 50, " %",
                    [] (PianoNote& n, float v) { n.filterCutoff = v / 100.0f; });
            addRow (5, "Resonance",     0, 100, src.resonance    * 100.0, 50, " %",
                    [] (PianoNote& n, float v) { n.resonance    = v / 100.0f; });

            mPortaLabel = std::make_unique<Label> (String(), "Porta Length");
            mPortaLabel->setFont (Font (12.0f));
            addAndMakeVisible (*mPortaLabel);
            mPortaBox = std::make_unique<NoteNumberBox>();
            mPortaBox->setInputRestrictions (6, "0123456789.");
            mPortaBox->setJustification (Justification::centred);
            mPortaBox->setText (formatBeats (src.portaLengthBeats), false);
            mPortaBox->setTooltip ("Portamento glide length in beats (Porta notes only)");
            auto applyPorta = [this] {
                if (mGrid == nullptr || mData == nullptr) return;
                const double v = jlimit (0.0, 64.0, mPortaBox->getText().getDoubleValue());
                beginIfNeeded();
                for (int idx : mTargets)
                    if (idx >= 0 && idx < (int) mData->notes.size())
                        mData->notes[(size_t) idx].portaLengthBeats = v;
                mPortaBox->setText (formatBeats (v), false);
                mGrid->repaint();
            };
            mPortaBox->onReturnKey        = applyPorta;
            mPortaBox->onFocusLost        = applyPorta;
            mPortaBox->onDoubleClickReset = [this, applyPorta] {
                mPortaBox->setText ("1", false);
                applyPorta();
            };
            addAndMakeVisible (*mPortaBox);
        }

        mCloseBtn = std::make_unique<TextButton> ("Close");
        mCloseBtn->onClick = [this] {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        };
        addAndMakeVisible (*mCloseBtn);

        reflectType (src.type);

        if (mCtx.engineAware)
            setSize (300, kPad * 2 + kRowH * 5 + 46);   // type + vel + bend amt + shape + close + notice
        else
            setSize (300, kPad * 2 + kRowH * 9);
    }

    ~NotePropsPanel() override
    {
        if (mDirty && mGrid != nullptr)
        {
            mGrid->commitEdit();
            if (mGrid->onNotesChanged) mGrid->onNotesChanged();
            mGrid->repaint();
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (kPad);

        auto typeRow = b.removeFromTop (kRowH).reduced (0, 2);
        const int nTypes = mCtx.engineAware ? 3 : 4;
        const int tw = typeRow.getWidth() / nTypes;
        mTypeNormal ->setBounds (typeRow.removeFromLeft (tw).reduced (2, 0));
        mTypeRpSlide->setBounds (typeRow.removeFromLeft (tw).reduced (2, 0));
        if (mCtx.engineAware)
            mTypeBend ->setBounds (typeRow.reduced (2, 0));
        else
        {
            mTypeRtSlide->setBounds (typeRow.removeFromLeft (tw).reduced (2, 0));
            mTypePorta  ->setBounds (typeRow.reduced (2, 0));
        }

        auto velRow = b.removeFromTop (kRowH);
        mLabels[0] ->setBounds (velRow.removeFromLeft (86));
        mSliders[0]->setBounds (velRow.reduced (0, 2));

        if (mCtx.engineAware)
        {
            { auto row = b.removeFromTop (kRowH);
              mBendAmtLabel  ->setBounds (row.removeFromLeft (86));
              mBendAmtCombo  ->setBounds (row.removeFromLeft (110).reduced (0, 3)); }
            { auto row = b.removeFromTop (kRowH);
              mBendShapeLabel->setBounds (row.removeFromLeft (86));
              mBendShapeCombo->setBounds (row.removeFromLeft (110).reduced (0, 3)); }
            mCloseBtn->setBounds (b.removeFromTop (kRowH).reduced (60, 3));
            mNotice->setBounds (b.reduced (2, 2));
        }
        else
        {
            for (int i = 1; i < 6; ++i)
            {
                auto row = b.removeFromTop (kRowH);
                mLabels[i] ->setBounds (row.removeFromLeft (86));
                mSliders[i]->setBounds (row.reduced (0, 2));
            }
            { auto row = b.removeFromTop (kRowH);
              mPortaLabel->setBounds (row.removeFromLeft (86));
              mPortaBox  ->setBounds (row.removeFromLeft (72).reduced (0, 3)); }
            mCloseBtn->setBounds (b.removeFromTop (kRowH).reduced (60, 3));
        }
    }

private:
    static constexpr int kRowH = 26, kPad = 8;

    void beginIfNeeded()
    {
        if (mDirty || mGrid == nullptr) return;
        mGrid->beginEdit ("Note Properties");
        mDirty = true;
    }

    void applyToTargets (std::function<void (PianoNote&)> fn)
    {
        if (mGrid == nullptr || mData == nullptr) return;
        beginIfNeeded();
        for (int idx : mTargets)
            if (idx >= 0 && idx < (int) mData->notes.size())
                fn (mData->notes[(size_t) idx]);
        mGrid->repaint();
    }

    void applyType (NoteType t)
    {
        applyToTargets ([t] (PianoNote& n) { n.type = t; });
        // Switching to Bend must SEED the amount + shape from what the dropdowns
        // show (the combo's initial setSelectedId is silent, and re-picking the
        // shown value fires no onChange), else the note keeps bendSemitones=0 and
        // emits a silent bend.
        if (t == NoteType::Bend)
        {
            if (mBendAmtCombo != nullptr && mBendAmtCombo->getSelectedId() != 0)
            {
                const double semis = (double) (mBendAmtCombo->getSelectedId() - 100);
                applyToTargets ([semis] (PianoNote& n) { n.bendSemitones = semis; });
            }
            if (mBendShapeCombo != nullptr && mBendShapeCombo->getSelectedId() > 0)
            {
                const int sh = mBendShapeCombo->getSelectedId() - 1;
                applyToTargets ([sh] (PianoNote& n) { n.bendShape = (BendShape) sh; });
            }
        }
        reflectType (t);
    }

    void reflectType (NoteType t)
    {
        if (mTypeNormal)  mTypeNormal ->setToggleState (t == NoteType::Standard,    dontSendNotification);
        if (mTypeRpSlide) mTypeRpSlide->setToggleState (t == NoteType::RampSlide,   dontSendNotification);
        if (mTypeRtSlide) mTypeRtSlide->setToggleState (t == NoteType::RetrigSlide, dontSendNotification);
        if (mTypePorta)   mTypePorta  ->setToggleState (t == NoteType::Portamento,  dontSendNotification);
        if (mTypeBend)    mTypeBend   ->setToggleState (t == NoteType::Bend,        dontSendNotification);

        // Bend dropdowns live only for Bend notes; Porta box only for Porta notes.
        const bool bendOn = (t == NoteType::Bend);
        auto setLive = [] (juce::Component* c, bool on)
        { if (c) { c->setEnabled (on); c->setAlpha (on ? 1.0f : 0.5f); } };
        setLive (mBendAmtCombo.get(),   bendOn);
        setLive (mBendAmtLabel.get(),   bendOn);
        setLive (mBendShapeCombo.get(), bendOn);
        setLive (mBendShapeLabel.get(), bendOn);

        const bool portaOn = (t == NoteType::Portamento);
        setLive (mPortaBox.get(),   portaOn);
        setLive (mPortaLabel.get(), portaOn);
    }

    static juce::String formatBeats (double b)
    {
        return juce::String (b, 3).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    }

    Component::SafePointer<PianoRollGrid> mGrid;
    PianoRollData*   mData;
    std::vector<int> mTargets;
    NoteEditContext  mCtx;
    bool             mDirty { false };
    std::unique_ptr<TextButton> mTypeNormal, mTypeRpSlide, mTypeRtSlide, mTypePorta, mTypeBend;
    std::unique_ptr<Label>  mLabels[6];
    std::unique_ptr<Slider> mSliders[6];
    std::unique_ptr<Label>         mPortaLabel;
    std::unique_ptr<NoteNumberBox> mPortaBox;
    std::unique_ptr<Label>         mBendAmtLabel, mBendShapeLabel, mNotice;   // Task 4
    std::unique_ptr<ComboBox>      mBendAmtCombo, mBendShapeCombo;            // Task 4
    std::unique_ptr<TextButton>    mCloseBtn;
};

void PianoRollGrid::openNoteProperties (int noteIdx, juce::Point<int> clickPos)
{
    if (!mData || noteIdx < 0 || noteIdx >= (int) mData->notes.size()) return;

    // Double-click on a selected note edits the whole (group-expanded)
    // selection; on an unselected note just that note.
    std::vector<int> targets;
    if (isNoteIndexSelected (noteIdx))
    {
        targets = mSelection;
        expandForGroups (targets);
        std::sort (targets.begin(), targets.end());
        targets.erase (std::unique (targets.begin(), targets.end()), targets.end());
    }
    else
        targets.push_back (noteIdx);

    NoteEditContext ctx;
    if (mNoteEditContextProvider) ctx = mNoteEditContextProvider();
    auto panel = std::make_unique<NotePropsPanel> (*this, mData,
                                                   std::move (targets), noteIdx, ctx);
    const auto scr = localAreaToGlobal (
        juce::Rectangle<int> (clickPos.x - 4, clickPos.y - 4, 8, 8));
    juce::CallOutBox::launchAsynchronously (std::move (panel), scr, nullptr);
}

void PianoRollGrid::mouseDoubleClick (const MouseEvent& e)
{
    if (mLastClickCreated) { mLastClickCreated = false; return; }
    if (e.mods.isRightButtonDown() || e.mods.isCtrlDown()) return;
    if (mActiveTool != PRTool::Draw && mActiveTool != PRTool::Select) return;
    if (!mData) return;
    const int idx = noteIndexAtPos (e.x, e.y);
    if (idx < 0) return;
    openNoteProperties (idx, e.getPosition());
}

void PianoRollGrid::mouseDown(const MouseEvent& e)
{
#if JUCE_DEBUG
    G3PlayheadDiag::log ("click(roll) x=" + juce::String (e.x) + " y=" + juce::String (e.y)
                         + " rawBeat=" + juce::String (xToBeat (e.x), 4)
                         + " snapBeat=" + juce::String (snapBeat (xToBeat (e.x)), 4)
                         + " snapDiv=" + juce::String (onGetSnapDiv ? onGetSnapDiv() : -1)
                         + " playheadBeat=" + juce::String (mPlayhead, 4));
#endif
    if (!mData) return;
    grabKeyboardFocus();
    if (e.getNumberOfClicks() == 1) mLastClickCreated = false;

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
            // 1A (QA-G3Smoke, Jeff 2026-07-23): seek obeys snap; Alt = free.
            if (onSeek) onSeek (e.mods.isAltDown() ? xToBeat (e.x)
                                                   : snapBeat (xToBeat (e.x)));
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
                beginResizeGesture(ri);
                break;
            }

            // Move check (click on existing note)
            int ni = noteIndexAtPos(e.x, e.y);
            if (ni >= 0)
            {
                // 2026-04-26 (D-7): FL-style click memory - clicking on an
                // existing note remembers its duration + type (+ velocity,
                // #12 QA-G3Smoke; + the full property set, smoke round 2).
                mClickMemoryDur   = mData->notes[ni].durationBeats;
                mClickMemoryType  = mData->notes[ni].type;
                mClickMemoryVel   = mData->notes[ni].velocity;
                mClickMemoryProto = mData->notes[ni];

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
                beginResizeGesture(ri);
                break;
            }

            int idx = noteIndexAtPos(e.x, e.y);
            if (idx >= 0)
            {
                // 2026-04-26 (D-7): click memory carries from the Select tool
                // too - clicking a note here also primes the next Draw-tool
                // click-place with that note's length + type (+ velocity, #12;
                // + the full property set, smoke round 2).
                mClickMemoryDur   = mData->notes[idx].durationBeats;
                mClickMemoryType  = mData->notes[idx].type;
                mClickMemoryVel   = mData->notes[idx].velocity;
                mClickMemoryProto = mData->notes[idx];

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
            mData->notes.push_back({ curNote, curBeat, snapUnitBeats(), 0.8f, 0.f, 0.f, mNewNoteType });
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
        // QA-Ee Stage 3: snap ON -> min one snap step; free (Alt / Off) -> 1 tick.
        const int    rSnapDiv = onGetSnapDiv ? onGetSnapDiv() : 1;
        const double minDur   = (e.mods.isAltDown() || rSnapDiv <= 0)
                                  ? (1.0 / (double) kTicksPerBeat) : snapUnitBeats();

        // QA-Chords: the delta is measured on the GRABBED note, then applied
        // to every gesture member - same amount each, so relative lengths are
        // preserved (D2) and each note floors at minDur independently.
        double deltaStart = 0.0, deltaDur = 0.0;
        if (mResizingFromLeft)
        {
            // 2026-04-26 (D-7): drag the LEFT edge - keep the original right
            // edge fixed and adjust startBeat + durationBeats together.
            const double origEnd  = mResizeOrigStart + mResizeOrigDur;
            const double rawStart = e.mods.isAltDown() ? xToBeat(e.x) : snapBeat(xToBeat(e.x));
            const double newStart = jmax(0.0, jmin(rawStart, origEnd - minDur));
            deltaStart = newStart - mResizeOrigStart;
        }
        else
        {
            const double newEnd = e.mods.isAltDown() ? xToBeat(e.x) : snapBeat(xToBeat(e.x));
            deltaDur = jmax(minDur, newEnd - mResizeOrigStart) - mResizeOrigDur;
        }

        for (size_t k = 0; k < mResizeIndices.size(); ++k)
        {
            const int idx = mResizeIndices[k];
            if (idx < 0 || idx >= (int)mData->notes.size()) continue;
            auto& n = mData->notes[(size_t) idx];
            if (mResizingFromLeft)
            {
                const double origEndK  = mResizeOrigStarts[k] + mResizeOrigDurs[k];
                const double newStartK = jlimit(0.0, origEndK - minDur,
                                                mResizeOrigStarts[k] + deltaStart);
                n.startBeat     = newStartK;
                n.durationBeats = origEndK - newStartK;
            }
            else
            {
                n.durationBeats = jmax(minDur, mResizeOrigDurs[k] + deltaDur);
            }
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
        if (e.mods.isShiftDown())
        {
            // B-5: Shift forces a VERTICAL cut at the snap-div-snapped X under the
            // cursor (both endpoints share that X); the y extent is the raw drag.
            mSliceStart.x = snappedX;
            mSliceEnd     = { snappedX, p.y };
        }
        else
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
        // QA-Ee Stage 3: snap ON -> min one snap step; Off -> 1 tick.
        const double drawMin = (onGetSnapDiv && onGetSnapDiv() <= 0)
                                 ? (1.0 / (double) kTicksPerBeat) : snapUnitBeats();
        const double dur = mDrawHasDragged
            ? jmax(drawMin, mDrawEnd - mDrawStart)
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

        // #12 (QA-G3Smoke) + smoke round 2 (Jeff): placements carry the
        // last-clicked note's WHOLE property set (pan / fine pitch / cutoff /
        // resonance / release / porta length / bend), not just vel+type+dur.
        // groupId / muted / slotIndex are per-note intent -- never carried.
        {
            PianoNote nn = mClickMemoryProto;
            nn.midiNote      = mDrawNote;
            nn.startBeat     = mDrawStart;
            nn.durationBeats = dur;
            nn.velocity      = mClickMemoryVel;
            nn.type          = nt;
            nn.muted         = false;
            nn.groupId       = -1;
            nn.slotIndex     = -1;
            mData->notes.push_back (nn);
        }
        tagLastCreatedNote (mDrawNote);   // Phase C §P4.2: slotIndex tagging
        mLastClickCreated = true;
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
        // QA-Chords: a MULTI-note gesture can move startBeats (left edge), so
        // the selection must survive the re-sort the same way Move's does -
        // capture keys first, rebuild after.  Single-note resize keeps the
        // pre-existing behavior exactly (no selection change).
        const bool multi = mResizeIndices.size() > 1;
        std::vector<std::pair<double,int>> keys;
        if (multi)
            for (int idx : mResizeIndices)
                if (idx >= 0 && idx < (int)mData->notes.size())
                    keys.push_back({mData->notes[(size_t) idx].startBeat,
                                    mData->notes[(size_t) idx].midiNote});
        sortNotes(mData->notes);
        if (multi && !keys.empty()) rebuildSelectionFromKeys(keys);
        commitEdit();
        if (onNotesChanged) onNotesChanged();
        mResizeNoteIdx = -1;
        mResizeIndices.clear();
        mResizeOrigDurs.clear();
        mResizeOrigStarts.clear();
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
        if (onVZoomAnchored) onVZoomAnchored(factor, e.y);   // anchor vertical zoom to cursor
        else if (onVZoom)    onVZoom(factor);
        return;
    }

    if (e.mods.isCtrlDown())
    {
        float factor = (wheel.deltaY > 0.f) ? 1.15f : (1.f / 1.15f);
        if (onZoomAnchored) onZoomAnchored(factor, e.x);   // anchor zoom to cursor
        else if (onZoom)    onZoom(mPPB * factor - mPPB);
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

    int visNotes = b.getHeight() / mNoteH + 2;

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

    // ── Vertical time grid - shared straight/triplet ladder, zoom-adaptive ────
    // QA-Ee Stage 3: lines come from the single shared ladder (kDynamicSnapLadder /
    // kTripletGridLadder) so Piano Roll, Drum Kit, and Builder draw the same set.
    // The snap TYPE picks the ladder (straight vs triplet); the snap DIVISION never
    // caps depth -- every rung that clears the 5px spacing is drawn, so zooming in
    // reveals down to 1/64 (straight) or 1/6 Step (triplet) regardless of the snap.
    // Bar lines are a separate time-signature-aware pass below, so the 384t bar rung
    // is skipped here.  Iterated fine -> coarse so coarser lines overdraw at shared x.
    static constexpr float kMinLineSpacing = (float) kMinGridLinePx;
    {
        const int  snapDiv = onGetSnapDiv ? onGetSnapDiv() : 1;
        int        nLad    = 0;
        const int* ladder  = gridLadderForSnap(snapDiv, nLad);
        for (int i = nLad - 1; i >= 0; --i)
        {
            const int gt = ladder[i];
            if (gt >= 384) continue;                                  // bar => separate TS-aware pass
            const double stepBeats = (double) gt / (double) kTicksPerBeat;
            const float  spacing   = mPPB * (float) stepBeats;
            if (spacing < kMinLineSpacing) continue;
            const float alpha = (gt >= 96) ? 0.50f
                              : (gt >= 32) ? 0.30f
                              : (gt >= 24) ? 0.18f
                              : (gt >= 8)  ? 0.10f
                              :              0.07f;
            g.setColour(VC::Accent.withAlpha(alpha));
            const double startBeat = std::floor(mBeatOff / stepBeats) * stepBeats;
            for (double beat = startBeat; beat <= mBeatOff + b.getWidth() / mPPB + stepBeats; beat += stepBeats)
            {
                int x = beatToX(beat);
                if (x < 0 || x > b.getWidth()) continue;
                g.drawVerticalLine(x, (float)mNoteYOffset, (float)b.getHeight());
            }
        }
    }
    // C.5b: Bar lines at multiples of (tsNum * 4 / tsDen) PPQ beats, drawn on
    // top.  4/4 = 4-beat bars, 3/4 = 3, 6/8 = 3, 5/4 = 5, 7/8 = 3.5.
    // QA-Ee Stage 2: declutter -- stop drawing bar lines once they fall below
    // kMinLinePx (Line snap stays active; only rendering is culled).
    const double barBpb = (double) juce::jmax (1, mTsNum) * 4.0 / (double) juce::jmax (1, mTsDen);
    if ((double) mPPB * barBpb >= (double) kMinLinePx)
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
            // 1-based bar labels (owner 2026-07-16): song downbeat = "1".
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

            // ── Note-type indicators (G-10, QA-G3Smoke #9/#10) ────────────
            // ONE right-edge arrow family for every non-Standard type:
            // RampSlide = filled white (takeover), RetrigSlide = white
            // outline (own attack), Portamento + Bend = white border + black
            // fill.  The old orange left arc collided with the note-name
            // text, and Bend had no marker at all.
            if (n.type != NoteType::Standard && w > 8)
            {
                float tx  = (float)(x + w - 2);
                float ty  = (float)(y + 2);
                float th  = (float)(mNoteH - 4);
                float tw2 = jmin(th * 0.7f, 7.f);
                Path tri;
                tri.addTriangle(tx - tw2, ty, tx - tw2, ty + th, tx, ty + th * 0.5f);
                if (n.type == NoteType::RampSlide)
                {
                    g.setColour(Colours::white.withAlpha(0.85f));
                    g.fillPath(tri);
                }
                else if (n.type == NoteType::RetrigSlide)
                {
                    g.setColour(Colours::white.withAlpha(0.85f));
                    g.strokePath(tri, PathStrokeType(1.2f));
                }
                else   // Portamento + Bend: white border, black fill
                {
                    g.setColour(Colours::black.withAlpha(0.85f));
                    g.fillPath(tri);
                    g.setColour(Colours::white.withAlpha(0.85f));
                    g.strokePath(tri, PathStrokeType(1.2f));
                }
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
        // QA-Chords: preview mirrors resolveStampNotes exactly (both modes).
        double beat     = snapBeat(xToBeat(mStampPos.x));
        int    px       = beatToX(beat);
        double dur      = snapUnitBeats();
        int    pw       = jmax(3, (int)(dur * mPPB) - 1);
        for (int mn : resolveStampNotes(yToNote(mStampPos.y)))
        {
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
            // 1-based bar labels (owner 2026-07-16): song downbeat = "1".
            g.drawText(String((int) std::round (barFrac) + 1), rx + 2, 1, 20, kRulerH - 2,
                       Justification::centredLeft, false);
        }
        else if (isBeat)
        {
            g.setColour(VC::Accent.withAlpha(0.6f));
            g.drawVerticalLine(rx, kRulerH / 2, (float)kRulerH);
        }
    }
    // Playhead marker (#30, QA-G3Smoke, final form per Jeff): ASYMMETRIC,
    // FL-style -- a left-anchored mast (line + cap share the same left edge =
    // the position) with the cap hanging RIGHT off it.  Nothing ever draws
    // left of the position, so parking at beat 0 clips nothing and the marker
    // reads whole at every position; on a bar line the mast's left edge sits
    // exactly on the 1-px grid-line column.  (The earlier centered-triangle +
    // centered-body pair guaranteed a half-clipped look at the left edge.)
    if (mPlayhead >= 0.0)
    {
        int px = beatToX(mPlayhead);
        if (px >= 0 && px <= b.getWidth())
        {
            g.setColour(VC::Green.withAlpha(0.9f));
            Path flag;
            flag.addTriangle((float) px, 0.f,
                             (float) px + 8.f, 0.f,
                             (float) px, (float) kRulerH);
            g.fillPath(flag);
        }
    }

    // Playhead
    if (mPlayhead >= 0.0)
    {
        int px = beatToX(mPlayhead);
        if (px >= 0 && px <= b.getWidth())
        {
            g.setColour(VC::Green.withAlpha(0.8f));
            g.fillRect(px, kRulerH, 1, b.getHeight() - kRulerH);   // 1-px mast: exact overlay on a grid-line column
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ControlLane
// ─────────────────────────────────────────────────────────────────────────────
ControlLane::ControlLane() {}

// QA-Layout T9 (L29): app-wide shared lane prefs -- one height for every lane
// (DrumKitControlLane included; containers lockstep-sync off these on their
// existing timers) and the last settled visibility as the new-container default.
// Message-thread only, like all lane state.
static int  gLaneUserHeight     = ControlLane::kHeight;
static bool gLaneDefaultVisible = true;

int  ControlLane::getUserHeight()             { return gLaneUserHeight; }
void ControlLane::setUserHeight (int h)       { gLaneUserHeight = juce::jlimit (kHeaderH, kHeight, h); }
bool ControlLane::getDefaultVisible()         { return gLaneDefaultVisible; }
void ControlLane::setDefaultVisible (bool v)  { gLaneDefaultVisible = v; }

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
        "Control > Pitch Bend",
        "Control > Filter Cutoff"
    };
    int modeIdx = (mMode == Velocity)  ? 0
                : (mMode == Panning)   ? 1
                : (mMode == PitchBend) ? 2 : 3;
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
            int gx = (int) std::llround((beat - mBeatOff) * mPPB);   // QA-L: match PianoRollGrid::beatToX rounding
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
    else
    {
        // QA-H Task 6: horizontal reference guides + labels in the unipolar
        // modes (Velocity / Filter Cutoff); the bipolar modes keep their
        // single centre line above.
        g.setFont (Font (8.0f));
        for (int pct : { 25, 50, 75 })
        {
            const int gy = b.getHeight() - 4
                         - (int) ((float) pct / 100.0f * (float) (contentH - 6));
            g.setColour (VC::Accent.withAlpha (pct == 50 ? 0.30f : 0.18f));
            g.drawHorizontalLine (gy, 4.f, (float) b.getWidth() - 30);
            g.setColour (VC::Text.withAlpha (0.45f));
            g.drawText (String (pct) + "%", b.getWidth() - 28, gy - 5, 26, 10,
                        Justification::centredLeft, false);
        }
    }

    // ── Stem + node + tail rendering ─────────────────────────────────────
    Colour nodeColor = (mMode == Velocity) ? VC::Green
                     : (mMode == Panning)  ? VC::Blue
                     :                       Colour(0xff44ffcc); // teal for pitch bend

    for (const auto& n : mData->notes)
    {
        int x     = (int) std::llround((n.startBeat - mBeatOff) * mPPB);   // QA-L: match beatToX rounding so the lane stem tracks the note head
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
    // ── Header: press starts either a resize drag or a mode-menu click; the
    // drag threshold decides which, so the menu waits for mouseUp (T9/L29).
    if (e.y < kHeaderH)
    {
        mHeaderPressed    = true;
        mHeaderDragged    = false;
        mHeaderDragStartY = e.getScreenY();
        mHeaderDragStartH = getHeight();
        return;
    }

    if (!mData) return;
    // Map y to normalised value accounting for the header offset
    auto yToValWithHeader = [&](int py) {
        int contentH = jmax(1, getHeight() - kHeaderH);
        int relY     = py - kHeaderH;
        return jlimit(0.f, 1.f, 1.f - (float)relY / (float)contentH);
    };

    // QA-H Task 6 (#5/MIDI-02): Ctrl+drag = scrub.  Sweeps across the lane
    // setting each SELECTED note's dot from the cursor's Y path as the
    // cursor passes its X (locked: selected notes only - no selection means
    // no targets).  Plain drag keeps the single-dot behavior below.
    if (e.mods.isCtrlDown())
    {
        if (! (hasAnySelection && hasAnySelection())) return;   // no targets, no edit
        mScrubbing    = true;
        mLastScrubX   = e.x;
        mLastScrubVal = yToValWithHeader (e.y);
        if (onBeginEdit) onBeginEdit ("Scrub Lane Values");
        scrubApply (e.x - 2, e.x + 2, mLastScrubVal, mLastScrubVal);
        return;
    }

    mDragNote = noteNearX(e.x, e.y);
    if (mDragNote)
    {
        if (onBeginEdit) onBeginEdit ("Adjust Lane Value");
        setVal(*mDragNote, yToValWithHeader(e.y));
        if (onChanged) onChanged();
        repaint();
    }
}

// Sweep [x0,x1] (unordered): every SELECTED note whose dot X lies inside
// gets the value interpolated along the cursor's path for that X.
void ControlLane::scrubApply (int x0, int x1, float v0, float v1)
{
    if (!mData || mPPB <= 0) return;
    const int lo = jmin (x0, x1), hi = jmax (x0, x1);
    bool touched = false;
    for (auto& n : mData->notes)
    {
        if (! (isNoteSelected && isNoteSelected (&n))) continue;
        const int nx = (int) std::llround((n.startBeat - mBeatOff) * mPPB);   // QA-L: match beatToX rounding
        if (nx < lo || nx > hi) continue;
        const float t = (x1 == x0) ? 1.0f
                      : jlimit (0.0f, 1.0f, (float) (nx - x0) / (float) (x1 - x0));
        setVal (n, v0 + (v1 - v0) * t);
        touched = true;
    }
    if (touched)
    {
        if (onChanged) onChanged();
        repaint();
    }
}

void ControlLane::mouseDrag(const MouseEvent& e)
{
    if (mHeaderPressed)
    {
        if (! mHeaderDragged
            && std::abs (e.getScreenY() - mHeaderDragStartY) < 3) return;
        mHeaderDragged = true;
        // Header is the lane's TOP edge: dragging up grows the lane.
        if (onHeightDragged)
            onHeightDragged (mHeaderDragStartH + (mHeaderDragStartY - e.getScreenY()));
        return;
    }

    if (!mData || e.y < kHeaderH) return;
    int contentH = jmax(1, getHeight() - kHeaderH);
    int relY     = e.y - kHeaderH;
    float val    = jlimit(0.f, 1.f, 1.f - (float)relY / (float)contentH);

    if (mScrubbing)
    {
        scrubApply (mLastScrubX, e.x, mLastScrubVal, val);
        mLastScrubX   = e.x;
        mLastScrubVal = val;
        return;
    }

    if (!mDragNote) return;
    setVal(*mDragNote, val); if (onChanged) onChanged(); repaint();
}

void ControlLane::mouseUp(const MouseEvent&)
{
    if (mHeaderPressed)
    {
        const bool wasClick = ! mHeaderDragged;
        mHeaderPressed = false;
        mHeaderDragged = false;
        if (wasClick)
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
        }
        return;
    }

    mDragNote  = nullptr;
    mScrubbing = false;
    if (onCommitEdit) onCommitEdit();
}

void ControlLane::mouseMove(const MouseEvent& e)
{
    setMouseCursor (e.y < kHeaderH ? MouseCursor::UpDownResizeCursor
                                   : MouseCursor::NormalCursor);
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

    // T9 (L29): open with the app-wide lane defaults; header drags write the
    // shared height so every other lane locksteps off its container's timer.
    mLaneVisible = ControlLane::getDefaultVisible();
    mLane->onHeightDragged = [this] (int h)
    {
        ControlLane::setUserHeight (h);
        resized();
    };

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

    // QA-H Task 7 (MIDI-01): Ctrl+click a key = select that pitch's notes.
    mKeyboard->onCtrlClickPitch = [this] (int note) {
        if (mGrid) mGrid->selectAllAtPitch (note);
    };

    // ── Toolbar row 1: Magnet | tool buttons | armed note type | Undo | Redo | H ─
    // QA-UICleanup Task 4: Tools wrench button removed; its popup folded into the
    // menu-bar Tools menu.
    mMagnetBtn = std::make_unique<RightClickTextButton>();
    mMagnetBtn->setButtonText("Snap");
    mMagnetBtn->setToggleState(true, dontSendNotification); // highlight; re-synced to the live div in setSnapAccessors
    mMagnetBtn->setTooltip("Snap resolution - click to choose");
    // QA-UICleanup Task 3 (SC8): click opens the resolution dropdown (was an on/off
    // toggle + right-click picker).  Highlight still reflects Off (dim) vs any active
    // division; "Off" is the first dropdown entry (default Line = highlighted).
    mMagnetBtn->onClick = [this] {
        PopupMenu m;
        const int cur = mOnGetSnapDiv ? mOnGetSnapDiv() : 1;
        for (int i = 0; i < kNumUnifiedSnapDivs; ++i)
            m.addItem(i + 1, kUnifiedSnapLabels[i], true, i == cur);
        m.showMenuAsync(PopupMenu::Options().withTargetComponent(mMagnetBtn.get()),
            [this](int r) {
                if (r > 0 && mOnSetSnapDiv) {
                    const int d = r - 1;
                    mOnSetSnapDiv(d);
                    if (mMagnetBtn) mMagnetBtn->setToggleState(d != 0, juce::dontSendNotification);
                }
                if (mGrid) { mGrid->repaint(); mGrid->grabKeyboardFocus(); }
            });
        if (mGrid) mGrid->grabKeyboardFocus();
    };
    addAndMakeVisible(*mMagnetBtn);

    // QA-UICleanup Task 3 (SC8): keep the Snap highlight synced with the shared
    // global div even when it's changed from another editor (low-freq idle poll).
    startTimer (200);

    static const char* toolLabels[] = {
        "Draw","Paint","Del","Mute","Slice","Select","Zoom","Stamp" };
    static const char* toolTips[] = {
        "Draw (P) - LMB draw | click note to move | near right edge to resize | Ctrl+click select",
        "Paint (B) - drag to paint notes continuously",
        "Delete (D) - click/drag to erase notes",
        "Mute (T) - toggle note mute",
        "Slice (C) - drag to draw cut line",
        "Select (E) - marquee select | drag notes to move",
        "Zoom (Z) - click to zoom in | drag region | RMB to zoom out",
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

    mNoteTypeBtn = std::make_unique<TextButton>("Flat");
    mNoteTypeBtn->setTooltip("Armed note type for new notes - click or S cycles "
                             "Flat/RP Slide/RT Slide/Porta; S with notes selected "
                             "also converts them");
    mNoteTypeBtn->onClick = [this] {
        if (mGrid) { mGrid->cycleNewNoteType(); mGrid->grabKeyboardFocus(); }
    };
    addAndMakeVisible(*mNoteTypeBtn);
    mGrid->onNoteTypeArmChanged = [this] { refreshNoteTypeButton(); };

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
    mGrid->setStampChord(kChordDefs[mChordIdx].intervals, kChordDefs[mChordIdx].degrees);
    updateScaleFromUI();

    // ── Menu bar ──────────────────────────────────────────────────────────
    mMenuBarModel = std::make_unique<PianoRollMenuBar>(*this);
    mMenuBar      = std::make_unique<juce::MenuBarComponent>(mMenuBarModel.get());
    addAndMakeVisible(*mMenuBar);

    // ── Wire grid callbacks ───────────────────────────────────────────────
    mGrid->onZoom    = [this](float delta) { applyZoom((mPPB + delta) / mPPB); };
    mGrid->onZoomAnchored = [this](float f, int x) { applyZoomAnchored(f, x); };
    mGrid->onVZoom   = [this](float factor) { applyVZoom(factor); };
    mGrid->onVZoomAnchored = [this](float f, int y) { applyVZoomAnchored(f, y); };
    mGrid->onHScroll = [this](double dB)   { mBeatOff = jmax(0.0, mBeatOff + dB); syncScrollState(); };
    mGrid->onVScroll = [this](int dN) {
        if (mFixedRange) return;
        int visRows = mGrid ? (mGrid->getHeight() / PianoRollGrid::kNoteH) : 8;
        // 2026-04-21: FL-convention range - bottom C0 (MIDI 0), top G10 (MIDI 127).
        int minTop  = visRows;
        mTopNote = jlimit(minTop, 127, mTopNote + dN);
        syncScrollState();
    };
    mGrid->onNotesChanged = [this]
    {
        mLane->repaint();
        // #30b regression fix: every note mutation republishes the scheduler
        // snapshot (via the editor-wired hook) -- see onContentEdited decl.
        if (onContentEdited) onContentEdited();
    };

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
        // QA-Ee Stage 2: match applyZoom's content-bound dynamic limits.
        const float vpW    = (float)jmax(1, mGrid->getWidth());
        const float minPPB = minZoomPPB (vpW);
        const float maxPPB = maxZoomPPB (vpW);
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

void PianoRollContainer::setLiveHeldNotes(uint64_t lo, uint64_t hi)
{
    if (mKeyboard) mKeyboard->setLiveHeldNotes(lo, hi);
}

void PianoRollContainer::setNoteLabelProvider(std::function<juce::String(int)> provider)
{
    if (mKeyboard) mKeyboard->setNoteLabelProvider(std::move(provider));
}

void PianoRollContainer::setKeyswitchLabelProvider(std::function<juce::String(int)> provider)
{
    if (mKeyboard) mKeyboard->setKeyswitchLabelProvider(std::move(provider));
}

void PianoRollContainer::setNoteEditContextProvider(std::function<PianoRollGrid::NoteEditContext()> provider)
{
    if (mGrid) mGrid->setNoteEditContextProvider(std::move(provider));
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
    if (mData) { mNumBars = mData->numBars; }
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

// QA-Ee Stage 2 (content-bound dynamic zoom): furthest-right note edge in the
// active pattern, in bars (4 beats/bar).  Drives the zoom-OUT minimum.
float PianoRollContainer::contentMaxBars() const
{
    double maxBeats = 0.0;
    if (mData)
        for (const auto& n : mData->notes)
            maxBeats = jmax(maxBeats, n.startBeat + n.durationBeats);
    return (float) jmax(0.0, maxBeats / 4.0);
}

// Zoom-OUT minimum (px/beat).  Empty baseline = vpW / kDefaultPianoRollEmptyPx
// (monitor-dependent); grows with notes + a 1-bar pad.  4 beats/bar.
float PianoRollContainer::minZoomPPB (float vpW) const
{
    const float defaultBars = vpW / kDefaultPianoRollEmptyPx;
    const float maxBars     = jmax (defaultBars, contentMaxBars() + kPianoRollZoomPadBars);
    return vpW / (4.f * jmax (1.f, maxBars));
}

// Zoom-IN maximum (px/beat) -- tick-level micro-editing (kMaxZoomInBeatsAcross
// beats fill the viewport at deepest zoom).
float PianoRollContainer::maxZoomPPB (float vpW) const
{
    return vpW / jmax (0.01f, kMaxZoomInBeatsAcross);
}

void PianoRollContainer::applyZoom(float factor)
{
    // QA-Ee Stage 2: content-bound dynamic limits.  Zoom-out expands with the
    // furthest note (+1-bar pad), floored at a monitor-dependent empty baseline;
    // zoom-in reaches tick level.  mPPB is pixels per beat; 4 beats per bar.
    float vpW = mGrid ? (float)jmax(1, mGrid->getWidth()) : 800.f;
    float minPPB = minZoomPPB (vpW);
    float maxPPB = maxZoomPPB (vpW);
    mPPB = jlimit(minPPB, maxPPB, mPPB * factor);
    syncScrollState();
}

// QA-Ee: cursor-anchored zoom -- keeps the beat under the mouse fixed (FL Ctrl+scroll feel).
void PianoRollContainer::applyZoomAnchored(float factor, int anchorX)
{
    const float vpW = mGrid ? (float)jmax(1, mGrid->getWidth()) : 800.f;
    const double anchorBeat = mBeatOff + (double) anchorX / jmax(1.f, mPPB);
    mPPB = jlimit(minZoomPPB(vpW), maxZoomPPB(vpW), mPPB * factor);
    mBeatOff = jmax(0.0, anchorBeat - (double) anchorX / jmax(1.f, mPPB));
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

// QA-Ee: vertical zoom anchored to cursor -- the pitch under the mouse stays fixed.
void PianoRollContainer::applyVZoomAnchored(float factor, int anchorY)
{
    if (mFixedRange || mGrid == nullptr) { applyVZoom(factor); return; }
    // Pitch under the cursor (mirrors PianoRollGrid::yToNote; mNoteYOffset = kRulerH).
    const int dy         = anchorY - PianoRollGrid::kRulerH;
    const int oldNoteH   = jmax (4, (int) (PianoRollGrid::kNoteH * mNoteHScale));
    const int noteBefore = mTopNote - dy / jmax (1, oldNoteH);
    applyVZoom (factor);                        // clamps mNoteHScale + syncs (note height updated)
    const int newNoteH   = jmax (4, (int) (PianoRollGrid::kNoteH * mNoteHScale));
    const int noteAfter  = mTopNote - dy / jmax (1, newNoteH);
    if (noteBefore != noteAfter)
        onVScroll (noteBefore - noteAfter);     // shift mTopNote so the pitch returns to the cursor
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
    mGrid->setScrollState(mPPB, mBeatOff, mTopNote, noteH, bars);
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
    // QA-Ee: include the current scroll offset so a cursor-anchored zoom that lands
    // past the last note stays representable -- otherwise the H-scrollbar clamps the
    // thumb to 0 and its (async) scrollBarMoved snaps mBeatOff back to bar 0.
    const double totalBeats   = jmax(lastNoteEnd + kBeatsPerBar, mBeatOff + visibleBeats0);
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
    // T9: the last settled toggle anywhere becomes the app-wide default new
    // containers open with (and what the project serializer stores).
    ControlLane::setDefaultVisible (v);
    resized();
    repaint();
}

void PianoRollContainer::setGhostsVisible(bool v)
{
    if (mGhostsVisible == v) return;
    mGhostsVisible = v;
    if (mGrid) mGrid->setGhostData(v ? mGhostStore : decltype(mGhostStore){});
}

// QA-Ee Stage 3: PianoRollPage wires these so the grid reads the global snap
// param live (snapBeat) + the magnet menu writes it.
void PianoRollContainer::setSnapAccessors (std::function<int()> getter,
                                           std::function<void(int)> setter)
{
    mOnGetSnapDiv = std::move (getter);
    mOnSetSnapDiv = std::move (setter);
    if (mGrid) mGrid->onGetSnapDiv = mOnGetSnapDiv;
    if (mOnGetSnapDiv)
    {
        const int cur = mOnGetSnapDiv();
        if (mMagnetBtn) mMagnetBtn->setToggleState (cur != 0, juce::dontSendNotification);
    }
}

// QA-UICleanup Task 4: PianoRollPage wires these so the grid reads the global
// quantize param live (toolQuantize) + the Tools>Quantize Settings menu writes it.
void PianoRollContainer::setQuantizeAccessors (std::function<int()> getter,
                                               std::function<void(int)> setter)
{
    mOnGetQuantizeDiv = std::move (getter);
    mOnSetQuantizeDiv = std::move (setter);
    if (mGrid) mGrid->onGetQuantizeDiv = mOnGetQuantizeDiv;
}

void PianoRollContainer::timerCallback()
{
    // QA-UICleanup Task 3: live-sync the Snap highlight to the shared global div
    // (may change from another editor's dropdown / the Builder).  Only the visible
    // roll needs it; setToggleState repaints only on an actual state change.
    if (isShowing() && mMagnetBtn && mOnGetSnapDiv)
        mMagnetBtn->setToggleState (mOnGetSnapDiv() != 0, juce::dontSendNotification);

    // T9 (L29): lockstep the shared lane height -- another container's header
    // drag (or a project load) changed it; only the visible roll re-lays.
    if (isShowing() && mLaneVisible && mLane
        && mLane->getHeight() != ControlLane::getUserHeight())
        resized();
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
    if (mGrid) mGrid->setStampChord(kChordDefs[chordIdx].intervals, kChordDefs[chordIdx].degrees);
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

void PianoRollContainer::refreshNoteTypeButton()
{
    if (!mNoteTypeBtn) return;
    const NoteType t = mGrid ? mGrid->getNewNoteType() : NoteType::Standard;
    mNoteTypeBtn->setButtonText(t == NoteType::RampSlide   ? "RP Slide"
                              : t == NoteType::RetrigSlide ? "RT Slide"
                              : t == NoteType::Portamento  ? "Porta"
                              : t == NoteType::Bend        ? "Bend"      // #10: labeled "Flat" before
                                                           : "Flat");
    mNoteTypeBtn->setToggleState(t != NoteType::Standard, dontSendNotification);
}

void PianoRollContainer::resized()
{
    auto b = getLocalBounds();

    // Menu bar (20 px)
    if (mMenuBar) mMenuBar->setBounds(b.removeFromTop(kMenuBarH));

    // Single toolbar row (28 px): Magnet | tools | note type | Undo | Redo | H
    auto row1 = b.removeFromTop(kToolbarH);
    row1.removeFromLeft(4);
    mMagnetBtn->setBounds(row1.removeFromLeft(38).reduced(2, 3));
    row1.removeFromLeft(4);
    for (int i = 0; i < 6; ++i)   // Draw(0)..Select(5); Stamp(7) always hidden
        mToolBtns[i]->setBounds(row1.removeFromLeft(62).reduced(2, 3));   // 2026-04-26: 36→62 to match Builder
    if (mNoteTypeBtn)
        mNoteTypeBtn->setBounds(row1.removeFromLeft(62).reduced(2, 3));
    mToolBtns[6]->setBounds(row1.removeFromLeft(62).reduced(2, 3));       // Zoom
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
    int actualLaneH = mLaneVisible ? ControlLane::getUserHeight() : 0;
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

// ── QA-H Task 3: Humanize (FL-replica dialog, docket #1) ─────────────────────
// CallOutBox content.  Start Time / Duration / Velocity each carry a Range +
// Offset knob pair; randomization is seeded (reproducible), quasi-normal
// distributed (mean of three uniforms), and time values scale by the Start
// Time Max Interval division.  Start-time randomness is late-biased (0..range
// delay, FL's human-lag model) - a negative Offset re-centers it; duration +
// velocity randomness is bipolar around the original.  Preview applies live
// to the roll; Accept commits ONE undo edit; any other dismissal restores
// the original notes untouched.
class HumanizePanel : public juce::Component
{
public:
    HumanizePanel (PianoRollGrid& grid, PianoRollData* data, std::vector<int> targets)
        : mGrid (&grid), mData (data), mTargets (std::move (targets))
    {
        std::sort (mTargets.begin(), mTargets.end());
        mOriginal.reserve (mTargets.size());
        for (int idx : mTargets)
            mOriginal.push_back (mData->notes[(size_t) idx]);

        mTitle = std::make_unique<Label> (String(), "Humanize");
        mTitle->setFont (Font (13.0f, Font::bold));
        addAndMakeVisible (*mTitle);
        mRangeHdr  = std::make_unique<Label> (String(), "Range");
        mOffsetHdr = std::make_unique<Label> (String(), "Offset");
        for (auto* l : { mRangeHdr.get(), mOffsetHdr.get() })
        {
            l->setFont (Font (11.0f));
            l->setJustificationType (Justification::centred);
            addAndMakeVisible (*l);
        }

        auto addKnob = [this] (std::unique_ptr<Slider>& sl,
                               double lo, double hi, double init)
        {
            sl = std::make_unique<Slider> (Slider::RotaryHorizontalVerticalDrag,
                                           Slider::TextBoxRight);
            sl->setRange (lo, hi, 1.0);
            sl->setValue (init, dontSendNotification);
            sl->setDoubleClickReturnValue (true, init);   // #19 (QA-G3Smoke)
            sl->setTextValueSuffix (" %");
            sl->setTextBoxStyle (Slider::TextBoxRight, false, 46, 16);
            sl->onValueChange = [this] { paramsChanged(); };
            addAndMakeVisible (*sl);
        };
        addKnob (mStartRange, 0, 100, kDefStartRange);
        addKnob (mStartOffset, -100, 100, 0);
        addKnob (mDurRange,   0, 100, kDefDurRange);
        addKnob (mDurOffset,  -100, 100, 0);
        addKnob (mVelRange,   0, 100, kDefVelRange);
        addKnob (mVelOffset,  -100, 100, 0);

        static const char* kSectionNames[] = { "Start Time", "Duration", "Velocity" };
        for (int i = 0; i < 3; ++i)
        {
            mSectionLabels[i] = std::make_unique<Label> (String(), kSectionNames[i]);
            mSectionLabels[i]->setFont (Font (12.0f));
            addAndMakeVisible (*mSectionLabels[i]);
        }

        auto addRowLabel = [this] (std::unique_ptr<Label>& l, const char* text)
        {
            l = std::make_unique<Label> (String(), text);
            l->setFont (Font (12.0f));
            addAndMakeVisible (*l);
        };
        addRowLabel (mDistLabel,     "Distribution");
        addRowLabel (mIntervalLabel, "Start Time Max Interval");
        addRowLabel (mSeedLabel,     "Seed");

        // #14 (G-3): three distributions, Quasi-Normal default.
        mDistCombo = std::make_unique<ComboBox>();
        mDistCombo->addItem ("Quasi-Normal", 1);
        mDistCombo->addItem ("Triangular",   2);
        mDistCombo->addItem ("Uniform",      3);
        mDistCombo->setSelectedId (1, dontSendNotification);
        mDistCombo->onChange = [this] { paramsChanged(); };
        addAndMakeVisible (*mDistCombo);

        // #13 (G-3): standalone interval list in beats (1/32, 1/64, 1/128),
        // default 1/64 -- decoupled from the app snap table.
        mIntervalCombo = std::make_unique<ComboBox>();
        mIntervalCombo->addItem ("1/32",  1);
        mIntervalCombo->addItem ("1/64",  2);
        mIntervalCombo->addItem ("1/128", 3);
        mIntervalCombo->setSelectedId (2, dontSendNotification);
        mIntervalCombo->onChange = [this] { paramsChanged(); };
        addAndMakeVisible (*mIntervalCombo);

        // #15 (G-3): seed = a friendly 1-10 dropdown, default 1, no "None".
        mSeedCombo = std::make_unique<ComboBox>();
        for (int sd = 1; sd <= 10; ++sd)
            mSeedCombo->addItem (String (sd), sd);
        mSeedCombo->setSelectedId (1, dontSendNotification);
        mSeedCombo->onChange = [this] { paramsChanged(); };
        addAndMakeVisible (*mSeedCombo);

        mPreviewToggle = std::make_unique<ToggleButton> ("Preview");
        mPreviewToggle->setToggleState (true, dontSendNotification);
        mPreviewToggle->onClick = [this] {
            if (mPreviewToggle->getToggleState()) applyPreview();
            else                                  { restoreOriginal(); repaintGrid(); }
        };
        addAndMakeVisible (*mPreviewToggle);

        auto addBtn = [this] (std::unique_ptr<TextButton>& b, const char* name,
                              std::function<void()> fn)
        {
            b = std::make_unique<TextButton> (name);
            b->onClick = std::move (fn);
            addAndMakeVisible (*b);
        };
        addBtn (mResetBtn, "Reset", [this] {
            mStartRange ->setValue (kDefStartRange, dontSendNotification);
            mStartOffset->setValue (0,   dontSendNotification);
            mDurRange   ->setValue (kDefDurRange, dontSendNotification);
            mDurOffset  ->setValue (0,   dontSendNotification);
            mVelRange   ->setValue (kDefVelRange, dontSendNotification);
            mVelOffset  ->setValue (0,   dontSendNotification);
            mIntervalCombo->setSelectedId (2, dontSendNotification);   // #13: 1/64
            paramsChanged();
        });
        addBtn (mRegenBtn, "Regenerate", [this] {
            // #15: rolls the 1-10 seed dropdown.
            mSeedCombo->setSelectedId (1 + juce::Random::getSystemRandom().nextInt (10),
                                       juce::sendNotification);
        });
        addBtn (mAcceptBtn, "Accept", [this] { accept(); });

        // First preview with the starting parameters.
        applyPreview();
        setSize (390, 300);
    }

    ~HumanizePanel() override
    {
        // QA-H: mGrid is a SafePointer; if it nulled out the PianoRollData mData
        // points at is gone too, so guard the restore against a use-after-free.
        if (! mAccepted && mGrid != nullptr)
        {
            restoreOriginal();
            repaintGrid();
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8);
        mTitle->setBounds (b.removeFromTop (18));
        auto hdr = b.removeFromTop (14);
        hdr.removeFromLeft (92);
        mRangeHdr ->setBounds (hdr.removeFromLeft (110));
        mOffsetHdr->setBounds (hdr.removeFromLeft (110));

        Slider* knobs[3][2] = { { mStartRange.get(), mStartOffset.get() },
                                { mDurRange.get(),   mDurOffset.get()   },
                                { mVelRange.get(),   mVelOffset.get()   } };
        for (int i = 0; i < 3; ++i)
        {
            auto row = b.removeFromTop (36);
            mSectionLabels[i]->setBounds (row.removeFromLeft (92));
            knobs[i][0]->setBounds (row.removeFromLeft (110).reduced (0, 1));
            knobs[i][1]->setBounds (row.removeFromLeft (110).reduced (0, 1));
        }

        auto distRow = b.removeFromTop (26);
        mDistLabel->setBounds (distRow.removeFromLeft (150));
        mDistCombo->setBounds (distRow.reduced (0, 2));
        auto intRow = b.removeFromTop (26);
        mIntervalLabel->setBounds (intRow.removeFromLeft (150));
        mIntervalCombo->setBounds (intRow.reduced (0, 2));
        auto seedRow = b.removeFromTop (26);
        mSeedLabel->setBounds (seedRow.removeFromLeft (150));
        mSeedCombo->setBounds (seedRow.removeFromLeft (70).reduced (0, 2));

        auto btns = b.removeFromBottom (26);
        mPreviewToggle->setBounds (btns.removeFromLeft (84));
        mAcceptBtn->setBounds (btns.removeFromRight (70));
        btns.removeFromRight (6);
        mRegenBtn->setBounds (btns.removeFromRight (92));
        btns.removeFromRight (6);
        mResetBtn->setBounds (btns.removeFromRight (64));
    }

private:
    // #16 (G-3): FL-reference defaults -- Start 10% / Duration 10% / Velocity
    // 20%, offsets 0 (offsets already default 0 at knob creation).
    static constexpr double kDefStartRange = 10, kDefDurRange = 10, kDefVelRange = 20;

    void paramsChanged()
    {
        if (mPreviewToggle && mPreviewToggle->getToggleState()) applyPreview();
    }

    void applyPreview()
    {
        restoreOriginal();
        applyToNotes();
        repaintGrid();
    }

    void restoreOriginal()
    {
        if (mData == nullptr) return;
        for (size_t k = 0; k < mTargets.size(); ++k)
        {
            const int idx = mTargets[k];
            if (idx >= 0 && idx < (int) mData->notes.size())
                mData->notes[(size_t) idx] = mOriginal[k];
        }
    }

    void applyToNotes()
    {
        if (mData == nullptr) return;
        juce::Random rng ((juce::int64) (mSeedCombo ? mSeedCombo->getSelectedId() : 1));
        // #13: standalone beats list (1/32 = 0.125, 1/64 = 0.0625, 1/128 =
        // 0.03125) -- no snapDivToTicks walk.
        const int    intId = mIntervalCombo->getSelectedId();
        const double intervalBeats = (intId == 1) ? 0.125
                                   : (intId == 3) ? 0.03125
                                                  : 0.0625;
        const float startR = (float) mStartRange ->getValue() / 100.0f;
        const float startO = (float) mStartOffset->getValue() / 100.0f;
        const float durR   = (float) mDurRange   ->getValue() / 100.0f;
        const float durO   = (float) mDurOffset  ->getValue() / 100.0f;
        const float velR   = (float) mVelRange   ->getValue() / 100.0f;
        const float velO   = (float) mVelOffset  ->getValue() / 100.0f;

        // #14 (G-3): one noise funnel, three shapes -- every consumer below
        // (start / duration / velocity) already draws through qn().
        const int dist = mDistCombo ? mDistCombo->getSelectedId() : 1;
        auto qn = [&rng, dist] {
            if (dist == 3) return rng.nextFloat();                                   // Uniform
            if (dist == 2) return (rng.nextFloat() + rng.nextFloat()) * 0.5f;        // Triangular
            return (rng.nextFloat() + rng.nextFloat() + rng.nextFloat()) * (1.0f / 3.0f);   // Quasi-Normal
        };

        for (size_t k = 0; k < mTargets.size(); ++k)
        {
            const int idx = mTargets[k];
            if (idx < 0 || idx >= (int) mData->notes.size()) continue;
            auto& n = mData->notes[(size_t) idx];
            const PianoNote& o = mOriginal[k];
            const float uStart = qn();                // late-biased 0..1
            const float bDur   = qn() * 2.0f - 1.0f;  // bell around 0
            const float bVel   = qn() * 2.0f - 1.0f;
            n.startBeat = juce::jmax (0.0,
                o.startBeat + (double) (startO + uStart * startR) * intervalBeats);
            n.durationBeats = juce::jmax (1.0 / (double) kTicksPerBeat,
                o.durationBeats + (double) (durO + bDur * durR) * intervalBeats);
            n.velocity = juce::jlimit (0.01f, 1.0f,
                o.velocity + velO + bVel * velR);
        }
    }

    void repaintGrid() { if (mGrid != nullptr) mGrid->repaint(); }

    void accept()
    {
        restoreOriginal();
        if (mGrid != nullptr && mData != nullptr)
        {
            mGrid->beginEdit ("Humanize");
            applyToNotes();
            mGrid->commitEdit();
            if (mGrid->onNotesChanged) mGrid->onNotesChanged();
            mGrid->repaint();
        }
        mAccepted = true;
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }

    Component::SafePointer<PianoRollGrid> mGrid;
    PianoRollData*         mData;
    std::vector<int>       mTargets;
    std::vector<PianoNote> mOriginal;
    bool                   mAccepted { false };

    std::unique_ptr<Label>  mTitle, mRangeHdr, mOffsetHdr;
    std::unique_ptr<Label>  mSectionLabels[3];
    std::unique_ptr<Label>  mDistLabel, mIntervalLabel, mSeedLabel;
    std::unique_ptr<Slider> mStartRange, mStartOffset, mDurRange, mDurOffset,
                            mVelRange, mVelOffset;
    std::unique_ptr<ComboBox>     mDistCombo, mIntervalCombo, mSeedCombo;   // #15: seed is a 1-10 dropdown
    std::unique_ptr<ToggleButton> mPreviewToggle;
    std::unique_ptr<TextButton>   mResetBtn, mRegenBtn, mAcceptBtn;
};

void PianoRollGrid::toolHumanize()
{
    if (!mData || mData->notes.empty()) return;

    // Selection-or-all (group-expanded, deduped).
    std::vector<int> targets;
    if (! mSelection.empty())
    {
        targets = mSelection;
        expandForGroups (targets);
        std::sort (targets.begin(), targets.end());
        targets.erase (std::unique (targets.begin(), targets.end()), targets.end());
    }
    else
    {
        targets.reserve (mData->notes.size());
        for (int i = 0; i < (int) mData->notes.size(); ++i)
            targets.push_back (i);
    }

    auto panel = std::make_unique<HumanizePanel> (*this, mData, std::move (targets));
    const auto scr = localAreaToGlobal (
        juce::Rectangle<int> (getWidth() / 2 - 4, kRulerH + 8, 8, 8));
    juce::CallOutBox::launchAsynchronously (std::move (panel), scr, nullptr);
}

void PianoRollGrid::toolQuantize()
{
    if (!mData) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Quantize");
    // QA-UICleanup Task 4: round to the decoupled Tools>Quantize resolution
    // (Unified_QuantizeDiv 0..3 -> 1/4,1/8,1/16,1/32 note = 1,0.5,0.25,0.125 beats),
    // NOT the snap grid.
    const int qdiv = onGetQuantizeDiv ? onGetQuantizeDiv() : 0;
    const double q = 1.0 / (double) (1 << qdiv);
    for (int i : targets)
        mData->notes[i].startBeat = std::round(mData->notes[i].startBeat / q) * q;
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
    VibeSlider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
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
    double snap = snapUnitBeats();
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

// ── QA-H Task 4: Randomize (FL-replica dialog, docket #1=B) ──────────────────
// Replaces the old instant velocity/start jitter.  Two sections per the FL
// manual: Pattern GENERATES notes (octave/range/key/scale pitch pool over the
// roll's span at the snap grid; length in fixed 1/16 steps + variation;
// population density; stack = extra chord notes; Random Portamento tags
// generated notes NoteType::Portamento; Merge Same Notes joins adjacent
// same-pitch notes) and Levels randomizes the six per-note properties
// (bipolar wheels; MODX = Filter Cutoff, MODY = Resonance per docket D=B).
// Each section is independently seeded.  Live preview (no toggle - FL
// behavior); Accept commits ONE undo edit; other dismissal restores.
class RandomizePanel : public juce::Component
{
public:
    RandomizePanel (PianoRollGrid& grid, PianoRollData* data,
                    std::vector<int> levelsTargets,
                    double spanBeats, double stepBeats)
        : mGrid (&grid), mData (data), mLevelsTargets (std::move (levelsTargets)),
          mSpanBeats (spanBeats), mStepBeats (stepBeats),
          mOriginalAll (data->notes)
    {
        mTitle = std::make_unique<Label> (String(), "Randomize");
        mTitle->setFont (Font (13.0f, Font::bold));
        addAndMakeVisible (*mTitle);

        auto addLabel = [this] (std::unique_ptr<Label>& l, const char* text,
                                float size = 12.0f, bool bold = false)
        {
            l = std::make_unique<Label> (String(), text);
            l->setFont (Font (size, bold ? Font::bold : Font::plain));
            addAndMakeVisible (*l);
        };
        auto addKnob = [this] (std::unique_ptr<Slider>& sl, double lo, double hi,
                               double init, double step, const char* suffix)
        {
            sl = std::make_unique<Slider> (Slider::RotaryHorizontalVerticalDrag,
                                           Slider::TextBoxRight);
            sl->setRange (lo, hi, step);
            sl->setValue (init, dontSendNotification);
            sl->setTextValueSuffix (suffix);
            sl->setTextBoxStyle (Slider::TextBoxRight, false, 46, 16);
            sl->onValueChange = [this] { applyPreview(); };
            addAndMakeVisible (*sl);
        };
        auto addIncDec = [this] (std::unique_ptr<Slider>& sl, double lo, double hi,
                                 double init)
        {
            sl = std::make_unique<Slider> (Slider::IncDecButtons, Slider::TextBoxLeft);
            sl->setRange (lo, hi, 1.0);
            sl->setValue (init, dontSendNotification);
            sl->setTextBoxStyle (Slider::TextBoxLeft, false, 52, 18);
            sl->onValueChange = [this] { applyPreview(); };
            addAndMakeVisible (*sl);
        };
        auto addToggle = [this] (std::unique_ptr<ToggleButton>& t, const char* name,
                                 bool on)
        {
            t = std::make_unique<ToggleButton> (name);
            t->setToggleState (on, dontSendNotification);
            t->onClick = [this] { applyPreview(); };
            addAndMakeVisible (*t);
        };

        // ── Pattern section ──────────────────────────────────────────────
        addToggle (mPatternOn, "Pattern (generate)", true);
        addLabel  (mPatSeedLabel, "Seed");
        addIncDec (mPatSeed, 0, 99999, 1234);

        addLabel  (mOctaveLabel, "Octave");   addIncDec (mOctave,   1, 7, 4);
        addLabel  (mRangeLabel,  "Range");    addIncDec (mRangeOct, 1, 4, 2);

        static const char* kKeyNames[] = {
            "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        addLabel (mKeyLabel, "Key");
        mKeyCombo = std::make_unique<ComboBox>();
        for (int i = 0; i < 12; ++i) mKeyCombo->addItem (kKeyNames[i], i + 1);
        mKeyCombo->setSelectedId (1, dontSendNotification);
        mKeyCombo->onChange = [this] { applyPreview(); };
        addAndMakeVisible (*mKeyCombo);

        addLabel (mScaleLabel, "Scale");
        mScaleCombo = std::make_unique<ComboBox>();
        for (int i = 0; i < kNumScales; ++i)
            mScaleCombo->addItem (kScaleDefs[i].name, i + 1);
        mScaleCombo->setSelectedId (1, dontSendNotification);
        mScaleCombo->onChange = [this] { applyPreview(); };
        addAndMakeVisible (*mScaleCombo);

        addLabel (mLengthLabel, "Length");     addKnob (mLength,    1, 16, 1, 1, " st");
        addLabel (mVarLabel,    "Variation");  addKnob (mVariation, 0, 100, 0, 1, " %");
        addLabel (mPopLabel,    "Population"); addKnob (mPopulation, 0, 100, 50, 1, " %");
        addLabel (mStackLabel,  "Stack");      addKnob (mStack,     0, 100, 0, 1, " %");
        addLabel (mPortaLabel,  "Random Portamento");
        addKnob  (mPorta, 0, 100, 0, 1, " %");
        addToggle (mMergeToggle, "Merge Same Notes", false);

        // ── Levels section ───────────────────────────────────────────────
        addLabel  (mLevelsHdr, "Levels", 12.0f, true);
        addLabel  (mLvlSeedLabel, "Seed");
        addIncDec (mLvlSeed, 0, 99999, 5678);

        static const char* kWheelNames[6] = {
            "Velocity", "Pan", "Fine Pitch", "Release", "Cutoff", "Resonance" };
        for (int i = 0; i < 6; ++i)
        {
            addLabel (mWheelLabels[i], kWheelNames[i]);
            addKnob  (mWheels[i], -100, 100, 0, 1, " %");
        }
        addToggle (mResetBefore, "Reset Before Processing", false);
        addToggle (mBipolar,     "Bipolar",                 true);

        auto addBtn = [this] (std::unique_ptr<TextButton>& b, const char* name,
                              std::function<void()> fn)
        {
            b = std::make_unique<TextButton> (name);
            b->onClick = std::move (fn);
            addAndMakeVisible (*b);
        };
        addBtn (mResetBtn, "Reset", [this] {
            mOctave->setValue (4, dontSendNotification);
            mRangeOct->setValue (2, dontSendNotification);
            mKeyCombo->setSelectedId (1, dontSendNotification);
            mScaleCombo->setSelectedId (1, dontSendNotification);
            mLength->setValue (1, dontSendNotification);
            mVariation->setValue (0, dontSendNotification);
            mPopulation->setValue (50, dontSendNotification);
            mStack->setValue (0, dontSendNotification);
            mPorta->setValue (0, dontSendNotification);
            mMergeToggle->setToggleState (false, dontSendNotification);
            for (auto& w : mWheels) w->setValue (0, dontSendNotification);
            mResetBefore->setToggleState (false, dontSendNotification);
            mBipolar->setToggleState (true, dontSendNotification);
            applyPreview();
        });
        addBtn (mAcceptBtn, "Accept", [this] { accept(); });

        applyPreview();
        setSize (470, 420);
    }

    ~RandomizePanel() override
    {
        // QA-H: guard the mData restore on mGrid validity (SafePointer) to avoid
        // a use-after-free if the roll was destroyed while this callout was open.
        if (! mAccepted && mGrid != nullptr)
        {
            restoreOriginal();
            mGrid->repaint();
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8);
        mTitle->setBounds (b.removeFromTop (18));

        auto patHdr = b.removeFromTop (24);
        mPatternOn->setBounds (patHdr.removeFromLeft (150));
        mPatSeed->setBounds (patHdr.removeFromRight (110).reduced (0, 2));
        mPatSeedLabel->setBounds (patHdr.removeFromRight (40));

        auto half = [] (juce::Rectangle<int>& row) {
            return row.removeFromLeft (row.getWidth() / 2);
        };
        auto pair = [] (juce::Rectangle<int> area, Label* l, Component* c) {
            l->setBounds (area.removeFromLeft (78));
            c->setBounds (area.reduced (2));
        };

        auto r1 = b.removeFromTop (26); auto r1l = half (r1);
        pair (r1l, mOctaveLabel.get(), mOctave.get());
        pair (r1,  mRangeLabel.get(),  mRangeOct.get());
        auto r2 = b.removeFromTop (26); auto r2l = half (r2);
        pair (r2l, mKeyLabel.get(),   mKeyCombo.get());
        pair (r2,  mScaleLabel.get(), mScaleCombo.get());
        auto r3 = b.removeFromTop (32); auto r3l = half (r3);
        pair (r3l, mLengthLabel.get(), mLength.get());
        pair (r3,  mVarLabel.get(),    mVariation.get());
        auto r4 = b.removeFromTop (32); auto r4l = half (r4);
        pair (r4l, mPopLabel.get(),   mPopulation.get());
        pair (r4,  mStackLabel.get(), mStack.get());
        auto r5 = b.removeFromTop (32); auto r5l = half (r5);
        mPortaLabel->setBounds (r5l.removeFromLeft (130));
        mPorta->setBounds (r5l.reduced (2));
        mMergeToggle->setBounds (r5.reduced (2));

        auto lvlHdr = b.removeFromTop (24);
        mLevelsHdr->setBounds (lvlHdr.removeFromLeft (100));
        mLvlSeed->setBounds (lvlHdr.removeFromRight (110).reduced (0, 2));
        mLvlSeedLabel->setBounds (lvlHdr.removeFromRight (40));

        for (int rowI = 0; rowI < 2; ++rowI)
        {
            auto row = b.removeFromTop (32);
            const int w3 = row.getWidth() / 3;
            for (int k = 0; k < 3; ++k)
            {
                auto cell = row.removeFromLeft (w3);
                mWheelLabels[rowI * 3 + k]->setBounds (cell.removeFromLeft (64));
                mWheels[rowI * 3 + k]->setBounds (cell.reduced (2));
            }
        }
        auto togRow = b.removeFromTop (24);
        mResetBefore->setBounds (togRow.removeFromLeft (200));
        mBipolar->setBounds (togRow.removeFromLeft (110));

        auto btns = b.removeFromBottom (26);
        mAcceptBtn->setBounds (btns.removeFromRight (70));
        btns.removeFromRight (6);
        mResetBtn->setBounds (btns.removeFromRight (64));
    }

private:
    void restoreOriginal()
    {
        if (mData != nullptr) mData->notes = mOriginalAll;
    }

    void generate (std::vector<PianoNote>& out, juce::Random& rng) const
    {
        const int  octave  = (int) mOctave->getValue();
        const int  rangeOc = (int) mRangeOct->getValue();
        const int  key     = mKeyCombo->getSelectedId() - 1;
        const auto& inKey  = kScaleDefs[(size_t) (mScaleCombo->getSelectedId() - 1)].inKey;
        const double lenSteps = mLength->getValue();
        const float  lenVar   = (float) mVariation->getValue();
        const float  pop      = (float) mPopulation->getValue();
        const float  stack    = (float) mStack->getValue();
        const float  porta    = (float) mPorta->getValue();

        std::vector<int> pool;
        for (int oc = 0; oc < rangeOc; ++oc)
            for (int pc = 0; pc < 12; ++pc)
                if (inKey[(size_t) pc])
                {
                    const int midi = 12 * (octave + oc) + 12 + key + pc;
                    if (midi >= 0 && midi <= 127) pool.push_back (midi);
                }
        if (pool.empty()) return;

        const double step = mStepBeats > 1.0e-6 ? mStepBeats : 0.25;
        for (double t = 0.0; t < mSpanBeats - 1.0e-9; t += step)
        {
            if (rng.nextFloat() * 100.0f >= pop) { rng.nextFloat(); continue; }
            double dur = 0.25 * lenSteps
                * (1.0 + (rng.nextFloat() * 2.0 - 1.0) * lenVar / 100.0);
            dur = juce::jlimit (1.0 / (double) kTicksPerBeat,
                                juce::jmax (1.0 / (double) kTicksPerBeat, mSpanBeats - t),
                                dur);
            PianoNote n;
            n.midiNote      = pool[(size_t) rng.nextInt ((int) pool.size())];
            n.startBeat     = t;
            n.durationBeats = dur;
            if (rng.nextFloat() * 100.0f < porta) n.type = NoteType::Portamento;
            out.push_back (n);
            for (int extra = 0; extra < 2; ++extra)
            {
                if (rng.nextFloat() * 100.0f >= stack) break;
                PianoNote s = n;
                s.type     = NoteType::Standard;
                s.midiNote = pool[(size_t) rng.nextInt ((int) pool.size())];
                bool dup = false;
                for (const auto& e : out)
                    if (e.midiNote == s.midiNote
                        && std::abs (e.startBeat - t) < 1.0e-9) { dup = true; break; }
                if (! dup) out.push_back (s);
            }
        }

        if (mMergeToggle->getToggleState())
        {
            std::sort (out.begin(), out.end(), [] (const PianoNote& a, const PianoNote& b)
                       { return a.midiNote != b.midiNote ? a.midiNote < b.midiNote
                                                         : a.startBeat < b.startBeat; });
            for (size_t i = 1; i < out.size();)
            {
                auto& prev = out[i - 1];
                auto& cur  = out[i];
                if (prev.midiNote == cur.midiNote
                    && cur.startBeat <= prev.startBeat + prev.durationBeats + 1.0e-9)
                {
                    prev.durationBeats = juce::jmax (prev.durationBeats,
                        cur.startBeat + cur.durationBeats - prev.startBeat);
                    out.erase (out.begin() + (long) i);
                }
                else ++i;
            }
        }
    }

    void applyLevelsToNote (PianoNote& n, juce::Random& rng) const
    {
        const bool bip = mBipolar->getToggleState();
        if (mResetBefore->getToggleState())
        {
            n.velocity = 0.8f; n.panning = 0.0f; n.finePitch = 0.0f;
            n.releaseAmt = 0.5f; n.filterCutoff = 0.5f; n.resonance = 0.5f;
        }
        float r[6];
        for (auto& v : r)
        {
            const float u = rng.nextFloat();
            v = bip ? u * 2.0f - 1.0f : u;
        }
        auto apply = [] (float& field, float lo, float hi, double wheelPct, float rnd)
        {
            if (wheelPct == 0.0) return;
            field = juce::jlimit (lo, hi,
                field + rnd * (float) (wheelPct / 100.0) * (hi - lo));
        };
        apply (n.velocity,     0.01f, 1.f, mWheels[0]->getValue(), r[0]);
        apply (n.panning,     -1.f,   1.f, mWheels[1]->getValue(), r[1]);
        apply (n.finePitch,   -1.f,   1.f, mWheels[2]->getValue(), r[2]);
        apply (n.releaseAmt,   0.f,   1.f, mWheels[3]->getValue(), r[3]);
        apply (n.filterCutoff, 0.f,   1.f, mWheels[4]->getValue(), r[4]);
        apply (n.resonance,    0.f,   1.f, mWheels[5]->getValue(), r[5]);
    }

    void computeInto()
    {
        if (mData == nullptr) return;
        if (mPatternOn->getToggleState())
        {
            std::vector<PianoNote> gen;
            juce::Random prng ((juce::int64) (int) mPatSeed->getValue());
            generate (gen, prng);
            mData->notes = std::move (gen);
            juce::Random lrng ((juce::int64) (int) mLvlSeed->getValue());
            for (auto& n : mData->notes) applyLevelsToNote (n, lrng);
        }
        else
        {
            juce::Random lrng ((juce::int64) (int) mLvlSeed->getValue());
            for (int idx : mLevelsTargets)
                if (idx >= 0 && idx < (int) mData->notes.size())
                    applyLevelsToNote (mData->notes[(size_t) idx], lrng);
        }
        sortNotes (mData->notes);
    }

    void applyPreview()
    {
        restoreOriginal();
        computeInto();
        if (mGrid != nullptr) mGrid->repaint();
    }

    void accept()
    {
        restoreOriginal();
        if (mGrid != nullptr && mData != nullptr)
        {
            mGrid->beginEdit ("Randomize");
            computeInto();
            mGrid->commitEdit();
            mGrid->clearSelection();
            if (mGrid->onNotesChanged) mGrid->onNotesChanged();
            mGrid->repaint();
        }
        mAccepted = true;
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }

    Component::SafePointer<PianoRollGrid> mGrid;
    PianoRollData*         mData;
    std::vector<int>       mLevelsTargets;
    double                 mSpanBeats, mStepBeats;
    std::vector<PianoNote> mOriginalAll;
    bool                   mAccepted { false };

    std::unique_ptr<Label> mTitle, mPatSeedLabel, mOctaveLabel, mRangeLabel,
                           mKeyLabel, mScaleLabel, mLengthLabel, mVarLabel,
                           mPopLabel, mStackLabel, mPortaLabel,
                           mLevelsHdr, mLvlSeedLabel;
    std::unique_ptr<Label>  mWheelLabels[6];
    std::unique_ptr<Slider> mPatSeed, mOctave, mRangeOct, mLength, mVariation,
                            mPopulation, mStack, mPorta, mLvlSeed;
    std::unique_ptr<Slider> mWheels[6];
    std::unique_ptr<ComboBox>     mKeyCombo, mScaleCombo;
    std::unique_ptr<ToggleButton> mPatternOn, mMergeToggle, mResetBefore, mBipolar;
    std::unique_ptr<TextButton>   mResetBtn, mAcceptBtn;
};

void PianoRollGrid::toolRandomize()
{
    if (!mData) return;

    // Levels-only targets (Pattern OFF): selection-or-all, group-expanded.
    std::vector<int> targets;
    if (! mSelection.empty())
    {
        targets = mSelection;
        expandForGroups (targets);
        std::sort (targets.begin(), targets.end());
        targets.erase (std::unique (targets.begin(), targets.end()), targets.end());
    }
    else
    {
        targets.reserve (mData->notes.size());
        for (int i = 0; i < (int) mData->notes.size(); ++i)
            targets.push_back (i);
    }

    const double spanBeats = (double) juce::jmax (1, mData->numBars)
                           * (double) juce::jmax (1, mTsNum)
                           * 4.0 / (double) juce::jmax (1, mTsDen);
    const int div = onGetSnapDiv ? onGetSnapDiv() : 6;
    const double stepBeats = (div >= 2 && div < kNumUnifiedSnapDivs)
        ? snapDivToTicks (div) / (double) kTicksPerBeat
        : 0.25;

    auto panel = std::make_unique<RandomizePanel> (*this, mData, std::move (targets),
                                                   spanBeats, stepBeats);
    const auto scr = localAreaToGlobal (
        juce::Rectangle<int> (getWidth() / 2 - 4, kRulerH + 8, 8, 8));
    juce::CallOutBox::launchAsynchronously (std::move (panel), scr, nullptr);
}

// ── QA-H Task 5: Riff Machine (FL-replica 8-step generator, docket #2=A) ─────
// Eight pipeline steps per the FL manual - Progression, Chords, Arpeggiation,
// Mirror, Levels, Articulation, Groove, Fit - each with an enable + Reset +
// Random, shown one page at a time behind a step-tab row.  Globals: Preview
// to step (stages past it are skipped), Work on existing score (stages 4-8
// transform the roll's notes instead of generating), Length (bars), Start
// Over, Dice (randomize everything), Accept.  Starter preset sets for the
// Progression / Chord / Arp / Articulation / Groove selectors are authored
// here (plan-sanctioned).  Live preview; Accept = ONE undo edit; any other
// dismissal restores the roll.
class RiffMachinePanel : public juce::Component
{
public:
    RiffMachinePanel (PianoRollGrid& grid, PianoRollData* data,
                      double barBeats, double stepBeats, int rollBars)
        : mGrid (&grid), mData (data), mBarBeats (barBeats), mStepBeats (stepBeats),
          mOriginalAll (data->notes)
    {
        mTitle = std::make_unique<Label> (String(), "Riff Machine");
        mTitle->setFont (Font (13.0f, Font::bold));
        addAndMakeVisible (*mTitle);

        static const char* kStepNames[8] = {
            "Prog", "Chords", "Arp", "Mirror", "Levels", "Artic", "Groove", "Fit" };
        for (int i = 0; i < 8; ++i)
        {
            mStepTabs[i] = std::make_unique<TextButton> (String (i + 1) + " " + kStepNames[i]);
            mStepTabs[i]->onClick = [this, i] { setStep (i); };
            addAndMakeVisible (*mStepTabs[i]);

            mStepEnable[i] = std::make_unique<ToggleButton> ("Step enabled");
            // #17 (QA-G3Smoke): ALL steps enabled by default (steps 4-7 were
            // default-off, so half the machine silently did nothing).
            mStepEnable[i]->setToggleState (true, dontSendNotification);
            mStepEnable[i]->onClick = [this] { applyPreview(); };
            addChildComponent (*mStepEnable[i]);
        }

        auto addPageLabel = [this] (int page, std::unique_ptr<Label>& l, const char* text)
        {
            l = std::make_unique<Label> (String(), text);
            l->setFont (Font (12.0f));
            addChildComponent (*l);
            mPageRows[page].push_back ({ l.get(), nullptr });
        };
        auto regComp = [this] (int page, Component* c)
        {
            addChildComponent (*c);
            mPageRows[page].back().comp = c;
        };
        auto addCombo = [&] (int page, std::unique_ptr<Label>& l, const char* name,
                             std::unique_ptr<ComboBox>& cb)
        {
            addPageLabel (page, l, name);
            cb = std::make_unique<ComboBox>();
            cb->onChange = [this] { applyPreview(); };
            regComp (page, cb.get());
        };
        auto addKnob = [&] (int page, std::unique_ptr<Label>& l, const char* name,
                            std::unique_ptr<Slider>& sl, double lo, double hi,
                            double init, const char* suffix)
        {
            addPageLabel (page, l, name);
            sl = std::make_unique<Slider> (Slider::RotaryHorizontalVerticalDrag,
                                           Slider::TextBoxRight);
            sl->setRange (lo, hi, 1.0);
            sl->setValue (init, dontSendNotification);
            sl->setDoubleClickReturnValue (true, init);   // #19 (QA-G3Smoke)
            sl->setTextValueSuffix (suffix);
            sl->setTextBoxStyle (Slider::TextBoxRight, false, 46, 16);
            sl->onValueChange = [this] { applyPreview(); };
            regComp (page, sl.get());
        };
        auto addIncDec = [&] (int page, std::unique_ptr<Label>& l, const char* name,
                              std::unique_ptr<Slider>& sl, double lo, double hi, double init)
        {
            addPageLabel (page, l, name);
            sl = std::make_unique<Slider> (Slider::IncDecButtons, Slider::TextBoxLeft);
            sl->setRange (lo, hi, 1.0);
            sl->setValue (init, dontSendNotification);
            sl->setDoubleClickReturnValue (true, init);   // #19 (QA-G3Smoke)
            sl->setTextBoxStyle (Slider::TextBoxLeft, false, 52, 18);
            sl->onValueChange = [this] { applyPreview(); };
            regComp (page, sl.get());
        };

        // ── Page 0: Progression ──────────────────────────────────────────
        addCombo (0, mProgLabel, "Progression", mProgCombo);
        for (int i = 0; i < kNumProgs; ++i) mProgCombo->addItem (kProgs[i].name, i + 1);
        mProgCombo->setSelectedId (2, dontSendNotification);   // Pop I-V-vi-IV
        addCombo (0, mRateLabel, "Chord rate", mRateCombo);
        mRateCombo->addItem ("Bar", 1); mRateCombo->addItem ("Half bar", 2);
        mRateCombo->addItem ("Beat", 3);
        mRateCombo->setSelectedId (1, dontSendNotification);

        // ── Page 1: Chords ───────────────────────────────────────────────
        addCombo (1, mChordLabel, "Chord", mChordCombo);
        for (int i = 0; i < kNumChordSets; ++i) mChordCombo->addItem (kChordSets[i].name, i + 1);
        mChordCombo->setSelectedId (4, dontSendNotification);  // Triad

        // ── Page 2: Arpeggiation ─────────────────────────────────────────
        addCombo (2, mArpPatLabel, "Pattern", mArpPatCombo);
        {
            static const char* kArpNames[8] = { "Up", "Down", "Up-Down", "Down-Up",
                                                "Converge", "Diverge", "Random", "Off (hold chord)" };
            for (int i = 0; i < 8; ++i) mArpPatCombo->addItem (kArpNames[i], i + 1);
            mArpPatCombo->setSelectedId (1, dontSendNotification);
        }
        addCombo (2, mArpModeLabel, "Mode", mArpModeCombo);
        mArpModeCombo->addItem ("Normal", 1); mArpModeCombo->addItem ("Flip", 2);
        mArpModeCombo->addItem ("Alternate", 3);
        mArpModeCombo->setSelectedId (1, dontSendNotification);
        addCombo (2, mArpSyncLabel, "Sync", mArpSyncCombo);
        mArpSyncCombo->addItem ("Time", 1); mArpSyncCombo->addItem ("Block", 2);
        mArpSyncCombo->addItem ("Chord", 3);
        mArpSyncCombo->setSelectedId (1, dontSendNotification);
        addKnob (2, mGateLabel, "Gate", mGate, 5, 100, 80, " %");

        // ── Page 3: Mirror ───────────────────────────────────────────────
        addKnob (3, mMirrorLabel, "Flip chance", mMirrorChance, 0, 100, 30, " %");

        // ── Page 4: Levels (PAN/VEL/REL/MODX/MODY/PITCH per the manual) ──
        {
            static const char* kWheelNames[6] = {
                "Pan", "Velocity", "Release", "Cutoff", "Resonance", "Fine Pitch" };
            // Smoke #39 (Jeff): ALL wheels default 0 -- every interim
            // non-neutral default pick is revoked (see the Artic/Groove
            // combos + resetStep).
            for (int i = 0; i < 6; ++i)
                addKnob (4, mWheelLabels[i], kWheelNames[i], mWheels[i], -100, 100,
                         0, " %");
        }
        addPageLabel (4, mBipolarLabel, "");
        mBipolar = std::make_unique<ToggleButton> ("Bipolar");
        mBipolar->setToggleState (true, dontSendNotification);
        mBipolar->onClick = [this] { applyPreview(); };
        regComp (4, mBipolar.get());
        addIncDec (4, mSeedLabel, "Seed", mSeed, 0, 99999, 4242);

        // ── Page 5: Articulation ─────────────────────────────────────────
        addCombo (5, mArticLabel, "Preset", mArticCombo);
        {
            static const char* kArticNames[6] = { "None", "Staccato 50%", "Staccato 25%",
                                                  "Legato", "Accent downbeats", "Soft offbeats" };
            for (int i = 0; i < 6; ++i) mArticCombo->addItem (kArticNames[i], i + 1);
            // Smoke #39 (Jeff): neutral default -- the interim non-neutral
            // picks are revoked across Levels/Artic/Groove.
            mArticCombo->setSelectedId (1, dontSendNotification);
        }

        // ── Page 6: Groove ───────────────────────────────────────────────
        addCombo (6, mGrooveLabel, "Preset", mGrooveCombo);
        {
            static const char* kGrooveNames[6] = { "Straight", "Swing light", "Swing",
                                                   "Swing hard", "Push", "Laid back" };
            for (int i = 0; i < 6; ++i) mGrooveCombo->addItem (kGrooveNames[i], i + 1);
            // Smoke #39 (Jeff): neutral default (see Artic note above).
            mGrooveCombo->setSelectedId (1, dontSendNotification);
        }

        // ── Page 7: Fit ──────────────────────────────────────────────────
        addCombo (7, mKeyLabel, "Key", mKeyCombo);
        {
            static const char* kKeyNames[12] = {
                "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
            for (int i = 0; i < 12; ++i) mKeyCombo->addItem (kKeyNames[i], i + 1);
            mKeyCombo->setSelectedId (1, dontSendNotification);
        }
        addCombo (7, mScaleLabel, "Scale", mScaleCombo);
        for (int i = 0; i < kNumScales; ++i) mScaleCombo->addItem (kScaleDefs[i].name, i + 1);
        mScaleCombo->setSelectedId (1, dontSendNotification);
        addIncDec (7, mMinLabel, "Min note", mMinNote, 0, 115, 36);
        addIncDec (7, mMaxLabel, "Max note", mMaxNote, 12, 127, 84);
        addPageLabel (7, mSnapLabel, "");
        mSnapScale = std::make_unique<ToggleButton> ("Snap to scale");
        mSnapScale->setToggleState (true, dontSendNotification);
        mSnapScale->onClick = [this] { applyPreview(); };
        regComp (7, mSnapScale.get());

        // ── Per-step Reset / Random ──────────────────────────────────────
        mStepResetBtn = std::make_unique<TextButton> ("Reset");
        mStepResetBtn->onClick = [this] { resetStep (mCurStep); applyPreview(); };
        addAndMakeVisible (*mStepResetBtn);
        mStepRandomBtn = std::make_unique<TextButton> ("Random");
        mStepRandomBtn->onClick = [this]
        {
            randomizeStep (mCurStep);
            mStepEnable[mCurStep]->setToggleState (true, dontSendNotification);   // #17
            applyPreview();
        };
        addAndMakeVisible (*mStepRandomBtn);

        // ── Globals ──────────────────────────────────────────────────────
        mPreviewLabel = std::make_unique<Label> (String(), "Preview to step");
        mPreviewLabel->setFont (Font (12.0f));
        addAndMakeVisible (*mPreviewLabel);
        mPreviewStep = std::make_unique<Slider> (Slider::IncDecButtons, Slider::TextBoxLeft);
        mPreviewStep->setRange (1, 8, 1);
        mPreviewStep->setValue (8, dontSendNotification);
        mPreviewStep->setTextBoxStyle (Slider::TextBoxLeft, false, 40, 18);
        mPreviewStep->onValueChange = [this] { applyPreview(); };
        addAndMakeVisible (*mPreviewStep);

        mWorkExisting = std::make_unique<ToggleButton> ("Work on existing score");
        // #20 (QA-G3Smoke): a roll the Riff Machine already wrote pre-checks
        // Work-on-existing, so re-opening refines instead of replacing.
        mWorkExisting->setToggleState (mData != nullptr && mData->riffMachineUsed,
                                       dontSendNotification);
        mWorkExisting->onClick = [this] { applyPreview(); };
        addAndMakeVisible (*mWorkExisting);

        mLengthLabel = std::make_unique<Label> (String(), "Length (bars)");
        mLengthLabel->setFont (Font (12.0f));
        addAndMakeVisible (*mLengthLabel);
        mLengthBars = std::make_unique<Slider> (Slider::IncDecButtons, Slider::TextBoxLeft);
        mLengthBars->setRange (1, 16, 1);
        mLengthBars->setValue (juce::jlimit (1, 16, rollBars), dontSendNotification);
        mLengthBars->setTextBoxStyle (Slider::TextBoxLeft, false, 40, 18);
        mLengthBars->onValueChange = [this] { applyPreview(); };
        addAndMakeVisible (*mLengthBars);

        auto addBtn = [this] (std::unique_ptr<TextButton>& b, const char* name,
                              std::function<void()> fn)
        {
            b = std::make_unique<TextButton> (name);
            b->onClick = std::move (fn);
            addAndMakeVisible (*b);
        };
        addBtn (mStartOverBtn, "Start over", [this] {
            for (int i = 0; i < 8; ++i)
            {
                resetStep (i);
                mStepEnable[i]->setToggleState (true, dontSendNotification);   // #17: all-on default
            }
            mPreviewStep->setValue (8, dontSendNotification);
            mWorkExisting->setToggleState (false, dontSendNotification);
            mSeed->setValue (4242, dontSendNotification);
            applyPreview();
        });
        addBtn (mDiceBtn, "Dice", [this] {
            // #17: Dice randomizes AND enables every step -- a diced step
            // that stayed disabled was inert.
            for (int i = 0; i < 8; ++i)
            {
                randomizeStep (i);
                mStepEnable[i]->setToggleState (true, dontSendNotification);
            }
            mSeed->setValue (juce::Random::getSystemRandom().nextInt (100000),
                             dontSendNotification);
            applyPreview();
        });
        addBtn (mAcceptBtn, "Accept", [this] { accept(); });

        setStep (0);
        applyPreview();
        setSize (540, 400);
    }

    ~RiffMachinePanel() override
    {
        // QA-H: guard the mData write on mGrid validity (SafePointer) -- if the
        // grid nulled out, the PianoRollData is gone too (avoid use-after-free).
        if (! mAccepted && mGrid != nullptr)
        {
            if (mData != nullptr) mData->notes = mOriginalAll;
            mGrid->repaint();
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8);
        mTitle->setBounds (b.removeFromTop (18));

        auto tabs = b.removeFromTop (24);
        const int tw = tabs.getWidth() / 8;
        for (int i = 0; i < 8; ++i)
            mStepTabs[i]->setBounds (tabs.removeFromLeft (tw).reduced (1, 0));

        auto enRow = b.removeFromTop (24);
        for (int i = 0; i < 8; ++i)
            mStepEnable[i]->setBounds (enRow.withWidth (130));
        mStepRandomBtn->setBounds (enRow.removeFromRight (70).reduced (0, 1));
        enRow.removeFromRight (6);
        mStepResetBtn->setBounds (enRow.removeFromRight (60).reduced (0, 1));

        auto page = b.removeFromTop (150);
        const auto& rows = mPageRows[mCurStep];
        const int colW = page.getWidth() / 2;
        for (size_t k = 0; k < rows.size(); ++k)
        {
            const int col = (int) k % 2, rowI = (int) k / 2;
            juce::Rectangle<int> cell (page.getX() + col * colW,
                                       page.getY() + rowI * 34, colW, 32);
            if (rows[k].label != nullptr)
                rows[k].label->setBounds (cell.removeFromLeft (78));
            if (rows[k].comp != nullptr)
                rows[k].comp->setBounds (cell.reduced (2));
        }

        auto g1 = b.removeFromTop (24);
        mPreviewLabel->setBounds (g1.removeFromLeft (100));
        mPreviewStep->setBounds (g1.removeFromLeft (86).reduced (0, 2));
        g1.removeFromLeft (10);
        mWorkExisting->setBounds (g1);
        auto g2 = b.removeFromTop (24);
        mLengthLabel->setBounds (g2.removeFromLeft (100));
        mLengthBars->setBounds (g2.removeFromLeft (86).reduced (0, 2));

        auto btns = b.removeFromBottom (26);
        mAcceptBtn->setBounds (btns.removeFromRight (70));
        btns.removeFromRight (6);
        mDiceBtn->setBounds (btns.removeFromRight (60));
        btns.removeFromRight (6);
        mStartOverBtn->setBounds (btns.removeFromRight (84));
    }

private:
    struct PageRow { Label* label; Component* comp; };

    struct ProgDef { const char* name; int len; int deg[8]; };
    static constexpr ProgDef kProgs[8] = {
        { "Static I",             1, { 0 } },
        { "Pop I-V-vi-IV",        4, { 0, 4, 5, 3 } },
        { "Rising I-IV-V",        3, { 0, 3, 4 } },
        { "Cadence ii-V-I",       3, { 1, 4, 0 } },
        { "Minor drift i-VI-VII", 3, { 0, 5, 6 } },
        { "Pendulum I-V",         2, { 0, 4 } },
        { "Descending walk",      8, { 7, 6, 5, 4, 3, 2, 1, 0 } },
        { "Random walk",          0, { 0 } },
    };
    static constexpr int kNumProgs = 8;
    struct ChordSetDef { const char* name; int n; int add[3]; }; // degree offsets; 100+k = octave+k degrees
    static constexpr ChordSetDef kChordSets[8] = {
        { "Single note",     0, { 0 } },
        { "Octave",          1, { 100 } },
        { "Fifth (power)",   1, { 4 } },
        { "Triad",           2, { 2, 4 } },
        { "Triad + Octave",  3, { 2, 4, 100 } },
        { "Seventh",         3, { 2, 4, 6 } },
        { "Sus4",            2, { 3, 4 } },
        { "Wide (5th+10th)", 2, { 4, 102 } },
    };
    static constexpr int kNumChordSets = 8;

    void setStep (int step)
    {
        mCurStep = juce::jlimit (0, 7, step);
        for (int i = 0; i < 8; ++i)
        {
            mStepTabs[i]->setToggleState (i == mCurStep, dontSendNotification);
            mStepEnable[i]->setVisible (i == mCurStep);
            for (auto& r : mPageRows[i])
            {
                if (r.label != nullptr) r.label->setVisible (i == mCurStep);
                if (r.comp  != nullptr) r.comp ->setVisible (i == mCurStep);
            }
        }
        resized();
    }

    void resetStep (int i)
    {
        switch (i)
        {
            case 0: mProgCombo->setSelectedId (2, dontSendNotification);
                    mRateCombo->setSelectedId (1, dontSendNotification); break;
            case 1: mChordCombo->setSelectedId (4, dontSendNotification); break;
            case 2: mArpPatCombo->setSelectedId (1, dontSendNotification);
                    mArpModeCombo->setSelectedId (1, dontSendNotification);
                    mArpSyncCombo->setSelectedId (1, dontSendNotification);
                    mGate->setValue (80, dontSendNotification); break;
            case 3: mMirrorChance->setValue (30, dontSendNotification); break;
            // Smoke #39 (Jeff): Levels/Artic/Groove reset NEUTRAL -- the
            // interim non-neutral picks (vel 20% / Staccato 50% / Swing
            // light) are revoked; #17's shipped substance is the step-enable
            // fix, not baked-in values.
            case 4: for (auto& w : mWheels) w->setValue (0, dontSendNotification);
                    mBipolar->setToggleState (true, dontSendNotification); break;
            case 5: mArticCombo->setSelectedId (1, dontSendNotification); break;
            case 6: mGrooveCombo->setSelectedId (1, dontSendNotification); break;
            case 7: mKeyCombo->setSelectedId (1, dontSendNotification);
                    mScaleCombo->setSelectedId (1, dontSendNotification);
                    mMinNote->setValue (36, dontSendNotification);
                    mMaxNote->setValue (84, dontSendNotification);
                    mSnapScale->setToggleState (true, dontSendNotification); break;
        }
    }

    void randomizeStep (int i)
    {
        auto& sr = juce::Random::getSystemRandom();
        switch (i)
        {
            case 0: mProgCombo->setSelectedId (1 + sr.nextInt (kNumProgs), dontSendNotification);
                    mRateCombo->setSelectedId (1 + sr.nextInt (3), dontSendNotification); break;
            case 1: mChordCombo->setSelectedId (1 + sr.nextInt (kNumChordSets), dontSendNotification); break;
            case 2: mArpPatCombo->setSelectedId (1 + sr.nextInt (7), dontSendNotification);
                    mArpModeCombo->setSelectedId (1 + sr.nextInt (3), dontSendNotification);
                    mArpSyncCombo->setSelectedId (1 + sr.nextInt (3), dontSendNotification);
                    mGate->setValue (30 + sr.nextInt (71), dontSendNotification); break;
            case 3: mMirrorChance->setValue (sr.nextInt (61), dontSendNotification); break;
            case 4: for (auto& w : mWheels) w->setValue (sr.nextInt (81) - 40, dontSendNotification); break;
            case 5: mArticCombo->setSelectedId (1 + sr.nextInt (6), dontSendNotification); break;
            case 6: mGrooveCombo->setSelectedId (1 + sr.nextInt (6), dontSendNotification); break;
            case 7: break;   // key/scale/range stay the user's musical frame
        }
    }

    // Scale-degree resolution basis: Fit's key/scale drives degree math even
    // when Fit's fold/snap is disabled (degrees are meaningless without one).
    void scaleBasis (std::vector<int>& degSemis, int& key) const
    {
        key = mKeyCombo->getSelectedId() - 1;
        const auto& inKey = kScaleDefs[(size_t) (mScaleCombo->getSelectedId() - 1)].inKey;
        degSemis.clear();
        for (int pc = 0; pc < 12; ++pc)
            if (inKey[(size_t) pc]) degSemis.push_back (pc);
        if (degSemis.empty()) degSemis.push_back (0);
    }
    static int degreeToSemis (const std::vector<int>& degSemis, int d)
    {
        const int n   = (int) degSemis.size();
        const int oct = (d >= 0 ? d / n : (d - n + 1) / n);
        const int idx = ((d % n) + n) % n;
        return 12 * oct + degSemis[(size_t) idx];
    }

    void computeInto()
    {
        if (mData == nullptr) return;
        const int preview = (int) mPreviewStep->getValue();
        auto stageOn = [&] (int oneBased) {
            return mStepEnable[oneBased - 1]->getToggleState() && preview >= oneBased;
        };
        const juce::int64 seed = (juce::int64) (int) mSeed->getValue();

        std::vector<int> degSemis; int key = 0;
        scaleBasis (degSemis, key);
        const int nDeg = (int) degSemis.size();

        std::vector<PianoNote> notes;

        if (mWorkExisting->getToggleState())
        {
            notes = mOriginalAll;
        }
        else
        {
            const double span = mLengthBars->getValue() * mBarBeats;
            const int rateId  = mRateCombo->getSelectedId();
            const double rate = rateId == 1 ? mBarBeats
                              : rateId == 2 ? mBarBeats * 0.5 : 1.0;
            juce::Random prng (seed * 8 + 1);
            const auto& prog = kProgs[(size_t) (mProgCombo->getSelectedId() - 1)];

            const int segCount = (int) std::ceil (span / rate - 1.0e-9);
            for (int s = 0; s < segCount; ++s)
            {
                const double segT   = s * rate;
                const double segEnd = juce::jmin (span, segT + rate);

                int rootDeg = 0;
                if (stageOn (1))
                    rootDeg = (prog.len == 0) ? prng.nextInt (2 * nDeg)
                                              : prog.deg[s % prog.len];

                std::vector<int> midis;
                midis.push_back (60 + key + degreeToSemis (degSemis, rootDeg));
                if (stageOn (2))
                {
                    const auto& cs = kChordSets[(size_t) (mChordCombo->getSelectedId() - 1)];
                    for (int a = 0; a < cs.n; ++a)
                    {
                        const int off = cs.add[a] >= 100 ? nDeg + (cs.add[a] - 100)
                                                         : cs.add[a];
                        midis.push_back (60 + key
                            + degreeToSemis (degSemis, rootDeg + off));
                    }
                }
                std::sort (midis.begin(), midis.end());
                midis.erase (std::unique (midis.begin(), midis.end()), midis.end());
                const int m = (int) midis.size();

                const int arpPat = mArpPatCombo->getSelectedId();   // 8 = Off
                if (! stageOn (3) || m <= 1 || arpPat == 8)
                {
                    for (int mi : midis)
                    {
                        PianoNote n;
                        n.midiNote = mi; n.startBeat = segT;
                        n.durationBeats = segEnd - segT;
                        notes.push_back (n);
                    }
                }
                else
                {
                    const int syncId = mArpSyncCombo->getSelectedId();
                    double stepDur = syncId == 1 ? (mStepBeats > 1.0e-6 ? mStepBeats : 0.25)
                                   : syncId == 2 ? 1.0
                                                 : (segEnd - segT) / m;
                    stepDur = juce::jmax (1.0 / (double) kTicksPerBeat, stepDur);

                    std::vector<int> order (static_cast<size_t> (m));
                    for (int k = 0; k < m; ++k) order[(size_t) k] = k;
                    switch (arpPat)
                    {
                        case 2: std::reverse (order.begin(), order.end()); break;
                        case 3: for (int k = m - 2; k >= 1; --k) order.push_back (k); break; // Up-Down
                        case 4: std::reverse (order.begin(), order.end());
                                for (int k = 1; k <= m - 2; ++k) order.push_back (k); break; // Down-Up
                        case 5: { std::vector<int> o; int lo = 0, hi = m - 1;               // Converge
                                  while (lo <= hi) { o.push_back (lo++); if (lo <= hi) o.push_back (hi--); }
                                  order = std::move (o); } break;
                        case 6: { std::vector<int> o; int lo = 0, hi = m - 1;               // Diverge
                                  while (lo <= hi) { o.push_back (lo++); if (lo <= hi) o.push_back (hi--); }
                                  std::reverse (o.begin(), o.end()); order = std::move (o); } break;
                        default: break;   // 1 Up / 7 Random (shuffled per cycle below)
                    }
                    const int  modeId  = mArpModeCombo->getSelectedId();
                    const bool altFlip = (modeId == 3) && (s % 2 == 1);
                    const double gate  = mGate->getValue() / 100.0;
                    const int cycleLen = (int) order.size();
                    int k = 0;
                    for (double t = segT; t < segEnd - 1.0e-9; t += stepDur, ++k)
                    {
                        const int cyc = k / cycleLen;
                        if (arpPat == 7 && k % cycleLen == 0)
                        {
                            juce::Random srng (seed * 977 + s * 131 + cyc);
                            for (int j = cycleLen - 1; j > 0; --j)
                                std::swap (order[(size_t) j],
                                           order[(size_t) srng.nextInt (j + 1)]);
                        }
                        bool rev = altFlip;
                        if (modeId == 2 && (cyc % 2 == 1)) rev = ! rev;   // Flip per cycle
                        const int oi = k % cycleLen;
                        const int idx = order[(size_t) (rev ? cycleLen - 1 - oi : oi)];
                        PianoNote n;
                        n.midiNote  = midis[(size_t) idx];
                        n.startBeat = t;
                        n.durationBeats = juce::jlimit (1.0 / (double) kTicksPerBeat,
                                                        segEnd - t, stepDur * gate);
                        notes.push_back (n);
                    }
                }
            }
        }

        // ── 4 Mirror ─────────────────────────────────────────────────────
        if (stageOn (4) && ! notes.empty())
        {
            double sum = 0.0;
            for (const auto& n : notes) sum += n.midiNote;
            const int center = (int) std::lround (sum / (double) notes.size());
            juce::Random mrng (seed * 8 + 4);
            const float chance = (float) mMirrorChance->getValue();
            for (auto& n : notes)
                if (mrng.nextFloat() * 100.0f < chance)
                    n.midiNote = juce::jlimit (0, 127, 2 * center - n.midiNote);
        }

        // ── 5 Levels (PAN/VEL/REL/MODX/MODY/PITCH) ───────────────────────
        if (stageOn (5))
        {
            juce::Random lrng (seed * 8 + 5);
            const bool bip = mBipolar->getToggleState();
            for (auto& n : notes)
            {
                float r[6];
                for (auto& v : r)
                {
                    const float u = lrng.nextFloat();
                    v = bip ? u * 2.0f - 1.0f : u;
                }
                auto apply = [] (float& field, float lo, float hi, double wheelPct, float rnd)
                {
                    if (wheelPct == 0.0) return;
                    field = juce::jlimit (lo, hi,
                        field + rnd * (float) (wheelPct / 100.0) * (hi - lo));
                };
                apply (n.panning,     -1.f,   1.f, mWheels[0]->getValue(), r[0]);
                apply (n.velocity,     0.01f, 1.f, mWheels[1]->getValue(), r[1]);
                apply (n.releaseAmt,   0.f,   1.f, mWheels[2]->getValue(), r[2]);
                apply (n.filterCutoff, 0.f,   1.f, mWheels[3]->getValue(), r[3]);
                apply (n.resonance,    0.f,   1.f, mWheels[4]->getValue(), r[4]);
                apply (n.finePitch,   -1.f,   1.f, mWheels[5]->getValue(), r[5]);
            }
        }

        // ── 6 Articulation ───────────────────────────────────────────────
        if (stageOn (6) && ! notes.empty())
        {
            const int a = mArticCombo->getSelectedId();
            if (a == 2 || a == 3)
            {
                const double f = (a == 2) ? 0.5 : 0.25;
                for (auto& n : notes)
                    n.durationBeats = juce::jmax (1.0 / (double) kTicksPerBeat,
                                                  n.durationBeats * f);
            }
            else if (a == 4)   // Legato: extend to the next later start
            {
                for (auto& n : notes)
                {
                    double next = 1.0e18;
                    for (const auto& o : notes)
                        if (o.startBeat > n.startBeat + 1.0e-9)
                            next = juce::jmin (next, o.startBeat);
                    if (next < 1.0e17)
                        n.durationBeats = juce::jmax (1.0 / (double) kTicksPerBeat,
                                                      next - n.startBeat);
                }
            }
            else if (a == 5)   // Accent downbeats
            {
                for (auto& n : notes)
                {
                    const double inBar = std::fmod (n.startBeat, mBarBeats);
                    if (inBar < 1.0e-6 || mBarBeats - inBar < 1.0e-6)
                        n.velocity = juce::jmin (1.0f, n.velocity * 1.25f);
                }
            }
            else if (a == 6)   // Soft offbeats
            {
                for (auto& n : notes)
                {
                    const double inBeat = std::fmod (n.startBeat, 1.0);
                    if (inBeat > 1.0e-6 && 1.0 - inBeat > 1.0e-6)
                        n.velocity = juce::jmax (0.05f, n.velocity * 0.75f);
                }
            }
        }

        // ── 7 Groove ─────────────────────────────────────────────────────
        if (stageOn (7))
        {
            const int gId = mGrooveCombo->getSelectedId();
            const double step = mStepBeats > 1.0e-6 ? mStepBeats : 0.25;
            double oddShift = 0.0, allShift = 0.0;
            switch (gId)
            {
                case 2: oddShift = 0.10; break;
                case 3: oddShift = 0.33; break;
                case 4: oddShift = 0.50; break;
                case 5: allShift = -0.05; break;
                case 6: allShift =  0.05; break;
                default: break;
            }
            for (auto& n : notes)
            {
                double s2 = n.startBeat + allShift * step;
                const double k = n.startBeat / step;
                const long   ki = std::lround (k);
                if (std::abs (k - (double) ki) < 1.0e-6 && (ki % 2 != 0))
                    s2 += oddShift * step;
                n.startBeat = juce::jmax (0.0, s2);
            }
        }

        // ── 8 Fit ────────────────────────────────────────────────────────
        if (stageOn (8))
        {
            int minN = (int) mMinNote->getValue();
            int maxN = (int) mMaxNote->getValue();
            if (maxN < minN + 11) maxN = juce::jmin (127, minN + 11);
            std::array<bool, 12> inKeyAbs {};
            const auto& inKey = kScaleDefs[(size_t) (mScaleCombo->getSelectedId() - 1)].inKey;
            for (int pc = 0; pc < 12; ++pc)
                inKeyAbs[(size_t) pc] = inKey[(size_t) (((pc - key) % 12 + 12) % 12)];
            for (auto& n : notes)
            {
                int mi = n.midiNote;
                while (mi > maxN) mi -= 12;
                while (mi < minN) mi += 12;
                if (mSnapScale->getToggleState())
                {
                    for (int off = 0; off <= 6; ++off)
                    {
                        if (inKeyAbs[(size_t) ((mi - off) % 12 + 12) % 12] && mi - off >= 0)
                            { mi -= off; break; }
                        if (inKeyAbs[(size_t) ((mi + off) % 12) % 12] && mi + off <= 127)
                            { mi += off; break; }
                    }
                }
                n.midiNote = juce::jlimit (0, 127, mi);
            }
        }

        sortNotes (notes);
        mData->notes = std::move (notes);
    }

    void applyPreview()
    {
        if (mData == nullptr) return;
        mData->notes = mOriginalAll;
        computeInto();
        if (mGrid != nullptr) mGrid->repaint();
    }

    void accept()
    {
        if (mGrid != nullptr && mData != nullptr)
        {
            mData->notes = mOriginalAll;
            mGrid->beginEdit ("Riff Machine");
            computeInto();
            mData->riffMachineUsed = true;   // #20: next open pre-checks Work-on-existing
            mGrid->commitEdit();
            mGrid->clearSelection();
            if (mGrid->onNotesChanged) mGrid->onNotesChanged();
            mGrid->repaint();
        }
        mAccepted = true;
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }

    Component::SafePointer<PianoRollGrid> mGrid;
    PianoRollData*         mData;
    double                 mBarBeats, mStepBeats;
    std::vector<PianoNote> mOriginalAll;
    bool                   mAccepted { false };
    int                    mCurStep  { 0 };

    std::unique_ptr<Label>        mTitle;
    std::unique_ptr<TextButton>   mStepTabs[8];
    std::unique_ptr<ToggleButton> mStepEnable[8];
    std::vector<PageRow>          mPageRows[8];

    std::unique_ptr<Label>    mProgLabel, mRateLabel, mChordLabel, mArpPatLabel,
                              mArpModeLabel, mArpSyncLabel, mGateLabel, mMirrorLabel,
                              mWheelLabels[6], mBipolarLabel, mSeedLabel, mArticLabel,
                              mGrooveLabel, mKeyLabel, mScaleLabel, mMinLabel, mMaxLabel,
                              mSnapLabel, mPreviewLabel, mLengthLabel;
    std::unique_ptr<ComboBox> mProgCombo, mRateCombo, mChordCombo, mArpPatCombo,
                              mArpModeCombo, mArpSyncCombo, mArticCombo, mGrooveCombo,
                              mKeyCombo, mScaleCombo;
    std::unique_ptr<Slider>   mGate, mMirrorChance, mWheels[6], mSeed, mMinNote,
                              mMaxNote, mPreviewStep, mLengthBars;
    std::unique_ptr<ToggleButton> mBipolar, mSnapScale, mWorkExisting;
    std::unique_ptr<TextButton>   mStepResetBtn, mStepRandomBtn, mStartOverBtn,
                                  mDiceBtn, mAcceptBtn;
};

void PianoRollGrid::toolRiffMachine()
{
    if (!mData) return;

    const double barBeats = (double) juce::jmax (1, mTsNum) * 4.0
                          / (double) juce::jmax (1, mTsDen);
    const int div = onGetSnapDiv ? onGetSnapDiv() : 6;
    const double stepBeats = (div >= 2 && div < kNumUnifiedSnapDivs)
        ? snapDivToTicks (div) / (double) kTicksPerBeat
        : 0.25;

    auto panel = std::make_unique<RiffMachinePanel> (*this, mData, barBeats, stepBeats,
                                                     juce::jmax (1, mData->numBars));
    const auto scr = localAreaToGlobal (
        juce::Rectangle<int> (getWidth() / 2 - 4, kRulerH + 8, 8, 8));
    juce::CallOutBox::launchAsynchronously (std::move (panel), scr, nullptr);
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
        // QA-UICleanup Task 4: Quantize submenu + Transpose items moved to the
        // Tools menu (Quantize action honors the decoupled Quantize resolution).
    }
    else if (idx == 1) // ── Tools ─────────────────────────────────────────────
    {
        // QA-UICleanup Task 4 (SC13): the old Tools-button popup folded in, in its
        // original order; the 7 tool-selectors dropped (SC5 - they duplicate the
        // toolbar tool buttons + the P/B/D/T/C/E/Z keys).
        menu.addItem(60, "Quantize\tAlt+Q");
        menu.addItem(61, "Strum\tAlt+S");
        menu.addItem(62, "Arpeggiate\tAlt+A");

        juce::PopupMenu chopSub;
        chopSub.addItem(70, "Into 2  (halves)");
        chopSub.addItem(71, "Into 3  (thirds)");
        chopSub.addItem(72, "Into 4  (quarters)");
        chopSub.addItem(73, "Into 6");
        chopSub.addItem(74, "Into 8");
        menu.addSubMenu("Chop...", chopSub);

        menu.addItem(63, "Glue\tCtrl+G");
        menu.addItem(64, "Articulate\tAlt+L");
        menu.addItem(65, "Randomize\tAlt+R");
        menu.addItem(67, "Humanize...");
        menu.addItem(68, "Riff Machine...\tAlt+E");
        menu.addItem(66, "Generate Chords\tAlt+P");

        menu.addSeparator();
        juce::PopupMenu quantSub;
        const int qd = mOwner.getQuantizeDiv();
        quantSub.addItem(110, "1/4",  true, qd == 0);
        quantSub.addItem(111, "1/8",  true, qd == 1);
        quantSub.addItem(112, "1/16", true, qd == 2);
        quantSub.addItem(113, "1/32", true, qd == 3);
        menu.addSubMenu("Quantize Settings", quantSub);

        menu.addSeparator();
        menu.addItem(7,  "Transpose Up\tShift+Up");
        menu.addItem(8,  "Transpose Down\tShift+Down");
        menu.addItem(9,  "Transpose Up Octave\tCtrl+Up");
        menu.addItem(10, "Transpose Down Octave\tCtrl+Down");
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
    // ── Tools (folded from the removed Tools-button popup) ──────────────────
    else if (id == 60) { if (auto* g = o.mGrid.get()) g->toolQuantize(); }
    else if (id == 61) { if (auto* g = o.mGrid.get()) g->toolStrum(); }
    else if (id == 62) { if (auto* g = o.mGrid.get()) g->toolArpeggiate(); }
    else if (id == 63) { if (auto* g = o.mGrid.get()) g->toolGlue(); }
    else if (id == 64) { if (auto* g = o.mGrid.get()) g->toolArticulate(); }
    else if (id == 65) { if (auto* g = o.mGrid.get()) g->toolRandomize(); }
    else if (id == 66) { if (auto* g = o.mGrid.get()) g->toolGenerateChords(); }
    else if (id == 67) { if (auto* g = o.mGrid.get()) g->toolHumanize(); }
    else if (id == 68) { if (auto* g = o.mGrid.get()) g->toolRiffMachine(); }
    else if (id >= 70 && id <= 74)
    {
        constexpr int kDivs[] = { 2, 3, 4, 6, 8 };
        if (auto* g = o.mGrid.get()) g->toolChop(kDivs[id - 70]);
    }
    // Quantize Settings: set the decoupled resolution only (no immediate quantize).
    else if (id >= 110 && id <= 113) { o.setQuantizeDiv(id - 110); }
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
