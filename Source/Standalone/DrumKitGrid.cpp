#include "DrumKitGrid.h"
#include <numeric>
#include <algorithm>
#include <map>
#include <set>
using namespace juce;

// ─────────────────────────────────────────────────────────────────────────────
// Static helpers
// ─────────────────────────────────────────────────────────────────────────────
static void sortNotes(std::vector<PianoNote>& notes)
{
    std::sort(notes.begin(), notes.end(),
        [](const PianoNote& a, const PianoNote& b){ return a.startBeat < b.startBeat; });
}

// ── MIDI note <-> name parsing (FL convention: C5 = MIDI 60, C6 = 72) ────────
// Accepts pure numbers (e.g. "60") or note names (e.g. "C5", "c5", "C#5", "Db5").
// Lowercase 'b' = flat (uppercase 'B' is the note B).  '#' = sharp.
// Returns -1 on parse failure.
static int parseNoteOrMidi(const String& input)
{
    String s = input.trim();
    if (s.isEmpty()) return -1;

    // Pure number → MIDI value.
    if (s.containsOnly("0123456789-"))
    {
        const int mn = s.getIntValue();
        return (mn >= 0 && mn <= 127) ? mn : -1;
    }

    // Note name: letter (case-insensitive) + optional accidental + octave.
    const char letter = (char) std::toupper(s[0]);
    int pc;
    switch (letter)
    {
        case 'C': pc = 0;  break;
        case 'D': pc = 2;  break;
        case 'E': pc = 4;  break;
        case 'F': pc = 5;  break;
        case 'G': pc = 7;  break;
        case 'A': pc = 9;  break;
        case 'B': pc = 11; break;
        default: return -1;
    }

    int idx = 1;
    if (idx < s.length())
    {
        const juce_wchar c = s[idx];
        if (c == '#')      { pc++; idx++; }
        else if (c == 'b') { pc--; idx++; }   // lowercase 'b' only = flat
    }

    if (idx >= s.length()) return -1;
    const int oct = s.substring(idx).getIntValue();
    const int midi = oct * 12 + pc;
    return (midi >= 0 && midi <= 127) ? midi : -1;
}

static String midiToName(int midi)
{
    static const char* names[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int pc  = ((midi % 12) + 12) % 12;
    const int oct = midi / 12;
    return String(names[pc]) + String(oct);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitSidebar - left strip (16 picker rows + audition keys)
// ─────────────────────────────────────────────────────────────────────────────
DrumKitSidebar::DrumKitSidebar()
{
    setMouseCursor(MouseCursor::PointingHandCursor);
    for (int i = 0; i < kNumRows; ++i)
    {
        auto btn = std::make_unique<TextButton>("Pick a sound  v");
        const int rowIdx = i;
        btn->onClick = [this, rowIdx]
        {
            if (mRowClickHandler) mRowClickHandler(rowIdx, mPickers[rowIdx].get());
        };
        addAndMakeVisible(*btn);
        mPickers[i] = std::move(btn);

        auto m = std::make_unique<MixerLedButton>("M");
        m->setClickingTogglesState(true);
        m->setOnColour(Colour(0xffff4444));
        m->setTooltip("Mute");
        addAndMakeVisible(*m);
        mMuteBtns[i] = std::move(m);

        auto s = std::make_unique<MixerLedButton>("S");
        s->setClickingTogglesState(true);
        s->setOnColour(VC::Yellow);
        s->setTooltip("Solo");
        addAndMakeVisible(*s);
        mSoloBtns[i] = std::move(s);
    }

    // 2026-04-26: thin Global Lock/Unlock button in the ruler row above the
    // picker dropdowns.  Toggles every drum slot's locked state in one click
    // (with a confirm-prompt the user can opt out of via "Don't show again").
    mGlobalLockBtn = std::make_unique<TextButton>("Global Lock/Unlock");
    mGlobalLockBtn->setTooltip("Lock or unlock every drum slot at once");
    mGlobalLockBtn->onClick = [this] { if (onGlobalLockRequested) onGlobalLockRequested(); };
    addAndMakeVisible(*mGlobalLockBtn);
}

void DrumKitSidebar::setRowMetrics(int rulerH, int rowH)
{
    if (mRulerH != rulerH || mRowH != rowH)
    {
        mRulerH = rulerH;
        mRowH   = jmax(1, rowH);
        resized();
        repaint();
    }
}

void DrumKitSidebar::setApvts(juce::AudioProcessorValueTreeState* a)
{
    mApvts = a;
}

void DrumKitSidebar::setKitRowProvider(std::function<std::vector<DrumKitRowInfo>()> fn)
{
    mProvider = std::move(fn);
    refresh();
}

void DrumKitSidebar::setRowClickHandler(std::function<void(int, juce::Component*)> fn)
{
    mRowClickHandler = std::move(fn);
}

void DrumKitSidebar::setAuditionHandlers(std::function<void(int)> onOn,
                                          std::function<void(int)> onOff)
{
    mAuditionOn  = std::move(onOn);
    mAuditionOff = std::move(onOff);
}

void DrumKitSidebar::setReorderHandler(std::function<void(int, int)> fn)
{
    mReorderHandler = std::move(fn);
}

void DrumKitSidebar::refresh()
{
    mRows = mProvider ? mProvider() : std::vector<DrumKitRowInfo>{};

    for (int i = 0; i < kNumRows; ++i)
    {
        auto& btn = *mPickers[i];
        if (i < (int) mRows.size() && mRows[i].hasEngine)
        {
            const auto& d = mRows[i];
            btn.setButtonText(d.locked ? "[L] " + d.displayName : d.displayName);
        }
        else
        {
            btn.setButtonText("Pick a sound  v");
        }

        mMuteAtt[i].reset();
        mSoloAtt[i].reset();
        const bool hasParams = mApvts != nullptr
                               && i < (int) mRows.size()
                               && mRows[i].hasEngine
                               && mRows[i].pageIndex >= 0;
        mMuteBtns[i]->setEnabled(hasParams);
        mSoloBtns[i]->setEnabled(hasParams);
        if (hasParams)
        {
            const String prefix = "mixer_drum_" + String(mRows[i].pageIndex);
            mMuteAtt[i] = std::make_unique<BtnAtt>(*mApvts, prefix + "_mute", *mMuteBtns[i]);
            mSoloAtt[i] = std::make_unique<BtnAtt>(*mApvts, prefix + "_solo", *mSoloBtns[i]);
        }
        else
        {
            mMuteBtns[i]->setToggleState(false, dontSendNotification);
            mSoloBtns[i]->setToggleState(false, dontSendNotification);
        }
    }
    repaint();
}

void DrumKitSidebar::resized()
{
    const auto b = getLocalBounds();

    // Global Lock/Unlock button sits in the ruler band, spanning the same
    // horizontal range as the picker column directly below it.
    if (mGlobalLockBtn)
    {
        const int btnX = b.getX() + kHandleW;
        const int btnW = kPickerW + kMuteW + kSoloW;   // span pickers + M + S
        const int btnH = jmax(12, mRulerH - 2);
        mGlobalLockBtn->setBounds(btnX, b.getY() + 1, btnW, btnH);
    }

    for (int r = 0; r < kNumRows; ++r)
    {
        const int y = b.getY() + mRulerH + r * mRowH;
        auto row = Rectangle<int>(b.getX(), y, kWidth, mRowH).reduced(2);
        row.removeFromLeft(kHandleW);
        if (mPickers[r])  mPickers[r]->setBounds(row.removeFromLeft(kPickerW));
        if (mMuteBtns[r]) mMuteBtns[r]->setBounds(row.removeFromLeft(kMuteW).reduced(1));
        if (mSoloBtns[r]) mSoloBtns[r]->setBounds(row.removeFromLeft(kSoloW).reduced(1));
        // Audition piano key area is painted (no child).
    }
}

void DrumKitSidebar::paint(Graphics& g)
{
    const auto b = getLocalBounds();
    g.fillAll(VC::Panel);

    // Ruler band - match the grid's ruler band so the area lines up.
    g.setColour(VC::Panel.brighter(0.12f));
    g.fillRect(0, 0, kWidth, mRulerH);
    g.setColour(VC::Accent.withAlpha(0.5f));
    g.drawHorizontalLine(mRulerH - 1, 0.f, (float) kWidth);

    for (int r = 0; r < kNumRows; ++r)
    {
        const int y = mRulerH + r * mRowH;
        auto rowRect = Rectangle<int>(0, y, kWidth, mRowH);
        const bool isAlt = (r % 2) == 0;
        g.setColour(isAlt ? VC::Panel : VC::Bg.brighter(0.05f));
        g.fillRect(rowRect);
        g.setColour(VC::Accent.withAlpha(0.3f));
        g.drawHorizontalLine(y + mRowH - 1, 0.f, (float) kWidth);

        // Drag handle.
        if (r < (int) mRows.size())
        {
            g.setColour(VC::TextDim);
            const int barH = 2, gap = 3;
            const int barW = kHandleW - 6;
            const int totalH = barH * 3 + gap * 2;
            int by = y + mRowH / 2 - totalH / 2;
            for (int i = 0; i < 3; ++i)
            {
                g.fillRect(3, by, barW, barH);
                by += barH + gap;
            }
        }

        // Audition piano key (rightmost, painted not a child).
        const int keyX = kHandleW + kPickerW + kMuteW + kSoloW;
        auto key = Rectangle<int>(keyX, y, kKeyW, mRowH).reduced(2);
        const bool keyDown = (mActiveAuditionRow == r);
        g.setColour(keyDown ? Colour(0xffcccccc) : Colours::white);
        g.fillRect(key);
        g.setColour(Colours::black);
        g.drawRect(key, 1);
    }

    // Active-row indicator - 2 px accent border around the picker only.
    for (int r = 0; r < (int) mRows.size() && r < kNumRows; ++r)
    {
        if (! mRows[r].isActive) continue;
        if (! mPickers[r]) continue;
        const auto pb = mPickers[r]->getBounds().expanded(1);
        g.setColour(mRows[r].color);
        g.drawRect(pb, 2);
    }

    // Drop indicator for drag-reorder.
    if (mReorderActive && mDragTargetRow >= 0)
    {
        const int y = mRulerH + mDragTargetRow * mRowH;
        g.setColour(Colours::white);
        g.fillRect(0, y - 1, kWidth, 2);
    }

    // Right edge separator (matches PianoKeyboard's).
    g.setColour(VC::Accent);
    g.fillRect(kWidth - 1, 0, 1, b.getHeight());
}

int DrumKitSidebar::rowFromY(int y) const
{
    if (y < mRulerH) return -1;
    int r = (y - mRulerH) / jmax(1, mRowH);
    return jlimit(0, kNumRows - 1, r);
}

DrumKitSidebar::HitArea DrumKitSidebar::hitArea(juce::Point<int> p, int& outRow) const
{
    outRow = rowFromY(p.y);
    if (outRow < 0) return HitArea::None;
    if (p.x < kHandleW) return HitArea::DragHandle;
    const int keyX = kHandleW + kPickerW + kMuteW + kSoloW;
    if (p.x >= keyX && p.x < keyX + kKeyW) return HitArea::AuditionKey;
    return HitArea::None;
}

void DrumKitSidebar::mouseDown(const MouseEvent& e)
{
    int row;
    const auto area = hitArea(e.position.toInt(), row);
    if (area == HitArea::None) return;

    if (area == HitArea::DragHandle)
    {
        if (row < (int) mRows.size())
        {
            mDragSourceRow = row;
            mDragTargetRow = row;
            mReorderActive = false;
        }
        return;
    }

    if (area == HitArea::AuditionKey)
    {
        if (row >= (int) mRows.size() || ! mRows[row].hasEngine) return;
        mActiveAuditionRow = row;
        if (mAuditionOn) mAuditionOn(row);
        repaint();
        return;
    }
}

void DrumKitSidebar::mouseDrag(const MouseEvent& e)
{
    // Drag-reorder.
    if (mDragSourceRow >= 0)
    {
        const int ey = e.getPosition().y;
        const int dy = std::abs(ey - (mRulerH + mDragSourceRow * mRowH + mRowH / 2));
        if (! mReorderActive && dy > 4)
            mReorderActive = true;
        if (mReorderActive)
        {
            const int newTarget = jlimit(0, (int) mRows.size(),
                                         (ey - mRulerH) / jmax(1, mRowH));
            if (newTarget != mDragTargetRow)
            {
                mDragTargetRow = newTarget;
                repaint();
            }
        }
    }
}

void DrumKitSidebar::mouseUp(const MouseEvent&)
{
    // Drag-reorder commit.
    if (mDragSourceRow >= 0)
    {
        if (mReorderActive
            && mDragTargetRow >= 0
            && mDragTargetRow != mDragSourceRow
            && mReorderHandler)
        {
            int dst = mDragTargetRow;
            if (dst > mDragSourceRow) dst = jmax(0, dst - 1);
            mReorderHandler(mDragSourceRow, dst);
        }
        mDragSourceRow = -1;
        mDragTargetRow = -1;
        mReorderActive = false;
        repaint();
        return;
    }

    // Audition release.
    if (mActiveAuditionRow >= 0)
    {
        if (mAuditionOff) mAuditionOff(mActiveAuditionRow);
        mActiveAuditionRow = -1;
        repaint();
    }
}

void DrumKitSidebar::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (onWheel) onWheel(e, wheel);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitGrid - constructor / coordinate helpers / accessors
// ─────────────────────────────────────────────────────────────────────────────
DrumKitGrid::DrumKitGrid()
{
    setMouseCursor(MouseCursor::CrosshairCursor);
    setWantsKeyboardFocus(true);
}

void DrumKitGrid::setScrollState(float ppb, double beatOff, int rowH)
{
    mPPB = ppb;
    mBeatOff = beatOff;
    mRowH = jmax(1, rowH);
    repaint();
}

void DrumKitGrid::setPatternManager(PatternManager* pm)
{
    if (mPM != pm)
    {
        mPM = pm;
        mSelection.clear();
        mPendingLabel = {};
        for (auto& v : mPendingBefore) v.clear();
        if (onUndoRedoStateChanged) onUndoRedoStateChanged();
    }
    repaint();
}

void DrumKitGrid::setKitRowProvider(std::function<std::vector<DrumKitRowInfo>()> fn)
{
    mRowProvider = std::move(fn);
    refreshRowsCache();
    repaint();
}

void DrumKitGrid::refreshRowsCache()
{
    if (mRowProvider) mRowsCache = mRowProvider();
}

void DrumKitGrid::setPlayheadBeat(double beat) { mPlayhead = beat; repaint(); }

double DrumKitGrid::xToBeat(int x)      const { return mBeatOff + (double) x / jmax(1.f, mPPB); }
int    DrumKitGrid::beatToX(double beat) const { return (int) ((beat - mBeatOff) * mPPB); }
int    DrumKitGrid::rowToY (int row)     const { return mRowYOffset + row * mRowH; }
int    DrumKitGrid::yToRow (int y)       const
{
    int r = (y - mRowYOffset) / jmax(1, mRowH);
    return jlimit(0, DrumKitSidebar::kNumRows - 1, r);
}

double DrumKitGrid::snapBeat(double beat) const
{
    // QA-Ee Stage 3: tick-based snap on the global Unified_PianoRollSnapDiv.
    // div 0 = Off, 1 = Line (finest visible rung at this zoom), 2..10 = fixed.
    const int div = onGetSnapDiv ? onGetSnapDiv() : 1;
    if (div <= 0) return beat;   // div 0 = Off (the single GLOBAL on/off)
    const int divTicks = (div == 1) ? dynamicSnapTicks ((double) mPPB * 4.0, kMinGridLinePx)
                                    : snapDivToTicks (div);
    if (divTicks <= 0) return beat;
    const double t = beat * (double) kTicksPerBeat;
    return std::round (t / (double) divTicks) * (double) divTicks / (double) kTicksPerBeat;
}

// QA-Ee Stage 3: current snap unit in beats (new-hit length + tool spacing).
double DrumKitGrid::snapUnitBeats() const
{
    const int div = onGetSnapDiv ? onGetSnapDiv() : 1;
    int divTicks = (div == 1) ? dynamicSnapTicks ((double) mPPB * 4.0, kMinGridLinePx)
                 : (div >= 2) ? snapDivToTicks (div)
                 : kTicksPerBeat / 4;                       // Off -> 1/16 note
    if (divTicks <= 0) divTicks = kTicksPerBeat / 4;
    return (double) divTicks / (double) kTicksPerBeat;
}

double DrumKitGrid::totalBeats() const
{
    // C.5b: pattern's intrinsic TS drives bar-length so a 3/4 pattern's
    // total beats reflect the actual playback length.
    int patternBars = 4;
    double bpb = 4.0;
    double last = 0.0;
    if (mPM != nullptr)
    {
        patternBars = jmax(1, mPM->currentPattern().drumRolls[0].numBars);
        bpb = jmax (1.0, mPM->getPatternBeatsPerBar (mPM->getCurrentPatternIndex()));
        for (int p = 0; p < (int) kMaxDrumPages; ++p)
            for (const auto& n : mPM->currentPattern().drumRolls[p].notes)
                last = jmax(last, n.startBeat + jmax(0.0625, n.durationBeats));
    }
    return jmax((double) patternBars * bpb, last + bpb);
}

int DrumKitGrid::rowToPageIndex(int rowIdx) const
{
    if (rowIdx < 0 || rowIdx >= (int) mRowsCache.size()) return -1;
    return mRowsCache[rowIdx].pageIndex;
}

int DrumKitGrid::pageIndexToRow(int pageIdx) const
{
    for (int r = 0; r < (int) mRowsCache.size(); ++r)
        if (mRowsCache[r].pageIndex == pageIdx) return r;
    return -1;
}

PianoRollData* DrumKitGrid::rollForRow(int rowIdx) const
{
    const int pageIdx = rowToPageIndex(rowIdx);
    if (pageIdx < 0 || pageIdx >= (int) kMaxDrumPages || mPM == nullptr) return nullptr;
    return &mPM->currentPattern().drumRolls[pageIdx];
}

DrumKitGrid::NoteRef DrumKitGrid::noteAtPos(int x, int y) const
{
    if (mPM == nullptr) return {};
    const int row = yToRow(y);
    if (row < 0 || row >= DrumKitSidebar::kNumRows) return {};
    const int pageIdx = rowToPageIndex(row);
    if (pageIdx < 0) return {};
    const auto& roll = mPM->currentPattern().drumRolls[pageIdx];
    const double beat = xToBeat(x);
    for (int i = (int) roll.notes.size() - 1; i >= 0; --i)
    {
        const auto& n = roll.notes[i];
        if (beat >= n.startBeat && beat < n.startBeat + jmax(0.0625, n.durationBeats))
            return { row, i };
    }
    return {};
}

DrumKitGrid::NoteRef DrumKitGrid::noteNearRightEdge(int x, int y) const
{
    // 2026-04-26 (D-7): when mResizeFromLeftEnabled (Ctrl+Alt+Home toggle),
    // detect the LEFT edge instead so drag-resize extends the note backward.
    if (mPM == nullptr) return {};
    const int row = yToRow(y);
    if (row < 0 || row >= DrumKitSidebar::kNumRows) return {};
    const int pageIdx = rowToPageIndex(row);
    if (pageIdx < 0) return {};
    const auto& roll = mPM->currentPattern().drumRolls[pageIdx];
    for (int i = (int) roll.notes.size() - 1; i >= 0; --i)
    {
        const auto& n = roll.notes[i];
        const int leftX  = beatToX(n.startBeat);
        const int rightX = beatToX(n.startBeat + jmax(0.0625, n.durationBeats));
        if (mResizeFromLeftEnabled)
        {
            if (x >= leftX - kResizeZone && x <= leftX + kResizeZone && x <= rightX)
                return { row, i };
        }
        else
        {
            if (x >= rightX - kResizeZone && x <= rightX + kResizeZone && x >= leftX)
                return { row, i };
        }
    }
    return {};
}

void DrumKitGrid::eraseAt(int x, int y)
{
    if (mPM == nullptr) return;
    const int row = yToRow(y);
    const int pageIdx = rowToPageIndex(row);
    if (pageIdx < 0) return;
    auto& notes = mPM->currentPattern().drumRolls[pageIdx].notes;
    const double beat = xToBeat(x);
    notes.erase(
        std::remove_if(notes.begin(), notes.end(),
            [beat](const PianoNote& n){
                return beat >= n.startBeat
                    && beat <  n.startBeat + jmax(0.0625, n.durationBeats);
            }),
        notes.end());
    mSelection.clear();
}

void DrumKitGrid::updateCursor()
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

void DrumKitGrid::setTool(PRTool t) { mActiveTool = t; updateCursor(); }

// ─────────────────────────────────────────────────────────────────────────────
// Undo / Redo
// ─────────────────────────────────────────────────────────────────────────────
DrumKitGrid::DrumKitSnapshot DrumKitGrid::takeSnapshot() const
{
    DrumKitSnapshot snap;
    if (mPM != nullptr)
    {
        for (int p = 0; p < (int) kMaxDrumPages; ++p)
            snap[(size_t) p] = mPM->currentPattern().drumRolls[p].notes;
    }
    return snap;
}

void DrumKitGrid::applySnapshot(const DrumKitSnapshot& snap)
{
    if (mPM == nullptr) return;
    for (int p = 0; p < (int) kMaxDrumPages; ++p)
        mPM->currentPattern().drumRolls[p].notes = snap[(size_t) p];
    mSelection.clear();
    repaint();
    if (onNotesChanged)         onNotesChanged();
    if (onUndoRedoStateChanged) onUndoRedoStateChanged();
}

// Undoable action wrapper for drum-kit edits.  Stores a snapshot of all 16
// drumRolls; redo restores `after`, undo restores `before`.  mFirstPerform
// matches PianoRollEditAction so the initial perform() (called from
// commitEdit() right after the live edit was applied) is a no-op.
class DrumKitEditAction : public juce::UndoableAction
{
public:
    DrumKitEditAction(juce::String label,
                       DrumKitGrid::DrumKitSnapshot before,
                       DrumKitGrid::DrumKitSnapshot after,
                       std::function<void(const DrumKitGrid::DrumKitSnapshot&)> apply)
        : mLabel(std::move(label)),
          mBefore(std::move(before)),
          mAfter(std::move(after)),
          mApply(std::move(apply)) {}

    bool perform() override
    {
        if (mFirstPerform) { mFirstPerform = false; return true; }
        if (mApply) mApply(mAfter);
        return true;
    }
    bool undo() override { if (mApply) mApply(mBefore); return true; }

private:
    juce::String mLabel;
    DrumKitGrid::DrumKitSnapshot mBefore, mAfter;
    std::function<void(const DrumKitGrid::DrumKitSnapshot&)> mApply;
    bool mFirstPerform { true };
};

void DrumKitGrid::beginEdit(const juce::String& label)
{
    if (mPM == nullptr) return;
    mPendingLabel  = label;
    mPendingBefore = takeSnapshot();
}

void DrumKitGrid::commitEdit()
{
    if (mPM == nullptr || mPendingLabel.isEmpty() || ! mUndoCtx.isValid()) return;
    auto label  = mPendingLabel;
    auto before = mPendingBefore;
    auto after  = takeSnapshot();
    mPendingLabel = {};

    mUndoCtx.perform(
        new DrumKitEditAction(
            label,
            std::move(before), std::move(after),
            [this](const DrumKitSnapshot& s) { applySnapshot(s); }),
        label);
}

// ─────────────────────────────────────────────────────────────────────────────
// Selection helpers
// ─────────────────────────────────────────────────────────────────────────────
bool DrumKitGrid::isSelected(NoteRef ref) const
{
    return std::find(mSelection.begin(), mSelection.end(), ref) != mSelection.end();
}

void DrumKitGrid::toggleSelection(NoteRef ref)
{
    auto it = std::find(mSelection.begin(), mSelection.end(), ref);
    if (it != mSelection.end()) mSelection.erase(it);
    else                         mSelection.push_back(ref);
}

void DrumKitGrid::selectAll()
{
    mSelection.clear();
    if (mPM == nullptr) return;
    // 2026-04-26 (D-7 sub-4 follow-up): range-aware Ctrl+A.  When a ruler
    // range is set, grab only the hits whose start lies inside [t0, t1)
    // across every drum row.
    const bool rangeActive = (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart);
    const double t0 = rangeActive ? jmin (mTimeSelBeatStart, mTimeSelBeatEnd) : 0.0;
    const double t1 = rangeActive ? jmax (mTimeSelBeatStart, mTimeSelBeatEnd) : 0.0;
    for (int row = 0; row < (int) mRowsCache.size(); ++row)
    {
        const int pageIdx = rowToPageIndex(row);
        if (pageIdx < 0) continue;
        const auto& roll = mPM->currentPattern().drumRolls[pageIdx];
        for (int i = 0; i < (int) roll.notes.size(); ++i)
        {
            if (rangeActive)
            {
                const double s = roll.notes[i].startBeat;
                if (s < t0 || s >= t1) continue;
            }
            mSelection.push_back({ row, i });
        }
    }
    repaint();
}

void DrumKitGrid::clearSelection() { mSelection.clear(); repaint(); }

void DrumKitGrid::invertSelection()
{
    if (mPM == nullptr) return;
    std::vector<NoteRef> newSel;
    for (int row = 0; row < (int) mRowsCache.size(); ++row)
    {
        const int pageIdx = rowToPageIndex(row);
        if (pageIdx < 0) continue;
        const auto& roll = mPM->currentPattern().drumRolls[pageIdx];
        for (int i = 0; i < (int) roll.notes.size(); ++i)
            if (! isSelected({ row, i }))
                newSel.push_back({ row, i });
    }
    mSelection = std::move(newSel);
    repaint();
}

void DrumKitGrid::finaliseMarquee()
{
    if (mPM == nullptr) return;
    if (! mMarqueeWasCtrl) mSelection.clear();
    auto rect = mMarqueeRect.toFloat();
    for (int row = 0; row < (int) mRowsCache.size(); ++row)
    {
        const int pageIdx = rowToPageIndex(row);
        if (pageIdx < 0) continue;
        const auto& roll = mPM->currentPattern().drumRolls[pageIdx];
        for (int i = 0; i < (int) roll.notes.size(); ++i)
        {
            const auto& n = roll.notes[i];
            const int x = beatToX(n.startBeat);
            const int w = jmax(3, (int) (jmax(0.0625, n.durationBeats) * mPPB) - 1);
            const int y = rowToY(row);
            const Rectangle<int> noteRect(x, y, w, mRowH);
            const NoteRef ref { row, i };
            if (rect.intersects(noteRect.toFloat()) && ! isSelected(ref))
                mSelection.push_back(ref);
        }
    }
}

void DrumKitGrid::rebuildSelectionFromKeys(const std::vector<std::pair<int,double>>& keys)
{
    mSelection.clear();
    if (mPM == nullptr) return;
    for (int row = 0; row < (int) mRowsCache.size(); ++row)
    {
        const int pageIdx = rowToPageIndex(row);
        if (pageIdx < 0) continue;
        const auto& roll = mPM->currentPattern().drumRolls[pageIdx];
        for (int i = 0; i < (int) roll.notes.size(); ++i)
        {
            for (const auto& k : keys)
                if (k.first == row && std::abs(roll.notes[i].startBeat - k.second) < 1e-9)
                    { mSelection.push_back({ row, i }); break; }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Clipboard
// ─────────────────────────────────────────────────────────────────────────────
void DrumKitGrid::copySelected()
{
    mClipboard.clear();
    if (mPM == nullptr || mSelection.empty()) return;

    int    minRow  = INT_MAX;
    double minBeat = std::numeric_limits<double>::max();
    for (auto ref : mSelection)
    {
        const int pageIdx = rowToPageIndex(ref.row);
        if (pageIdx < 0) continue;
        const auto& roll = mPM->currentPattern().drumRolls[pageIdx];
        if (ref.idx < 0 || ref.idx >= (int) roll.notes.size()) continue;
        minRow  = jmin(minRow,  ref.row);
        minBeat = jmin(minBeat, roll.notes[ref.idx].startBeat);
    }
    if (minRow == INT_MAX) return;

    for (auto ref : mSelection)
    {
        const int pageIdx = rowToPageIndex(ref.row);
        if (pageIdx < 0) continue;
        const auto& roll = mPM->currentPattern().drumRolls[pageIdx];
        if (ref.idx < 0 || ref.idx >= (int) roll.notes.size()) continue;
        ClipboardCell c;
        c.relRow = ref.row - minRow;
        c.note   = roll.notes[ref.idx];
        c.note.startBeat -= minBeat;
        mClipboard.push_back(c);
    }
}

void DrumKitGrid::pasteClipboard()
{
    if (mPM == nullptr || mClipboard.empty()) return;
    beginEdit("Paste");
    const double pasteAt = snapBeat(mPlayhead >= 0.0 ? mPlayhead : mBeatOff);

    std::vector<std::pair<int,double>> newKeys;
    for (const auto& c : mClipboard)
    {
        const int row = c.relRow;
        if (row < 0 || row >= (int) mRowsCache.size()) continue;
        const int pageIdx = rowToPageIndex(row);
        if (pageIdx < 0) continue;
        PianoNote n = c.note;
        n.startBeat += pasteAt;
        newKeys.push_back({ row, n.startBeat });
        mPM->currentPattern().drumRolls[pageIdx].notes.push_back(n);
    }
    for (int p = 0; p < (int) kMaxDrumPages; ++p)
        sortNotes(mPM->currentPattern().drumRolls[p].notes);
    rebuildSelectionFromKeys(newKeys);
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

void DrumKitGrid::duplicateSelected()
{
    if (mPM == nullptr) return;

    // Timeline-based duplicate.
    if (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart)
    {
        const double selStart = jmin(mTimeSelBeatStart, mTimeSelBeatEnd);
        const double selEnd   = jmax(mTimeSelBeatStart, mTimeSelBeatEnd);
        const double selLen   = selEnd - selStart;
        if (selLen <= 0.0) return;

        std::vector<std::pair<int, PianoNote>> newNotes;
        for (int row = 0; row < (int) mRowsCache.size(); ++row)
        {
            const int pageIdx = rowToPageIndex(row);
            if (pageIdx < 0) continue;
            const auto& roll = mPM->currentPattern().drumRolls[pageIdx];
            for (const auto& n : roll.notes)
                if (n.startBeat >= selStart && n.startBeat < selEnd)
                {
                    PianoNote nn = n;
                    nn.startBeat += selLen;
                    newNotes.push_back({ row, nn });
                }
        }
        if (newNotes.empty()) return;

        beginEdit("Duplicate (Timeline)");
        std::vector<std::pair<int,double>> newKeys;
        for (auto& [row, nn] : newNotes)
        {
            const int pageIdx = rowToPageIndex(row);
            if (pageIdx < 0) continue;
            mPM->currentPattern().drumRolls[pageIdx].notes.push_back(nn);
            newKeys.push_back({ row, nn.startBeat });
        }
        for (int p = 0; p < (int) kMaxDrumPages; ++p)
            sortNotes(mPM->currentPattern().drumRolls[p].notes);
        rebuildSelectionFromKeys(newKeys);
        mTimeSelBeatStart += selLen;
        mTimeSelBeatEnd   += selLen;
        commitEdit();
        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    // Selection-based duplicate.  2026-04-26 (D-7): no longer falls back to
    // "every drum hit on every row" when nothing is selected - duplicate now
    // requires either an explicit selection or a ruler time-range (handled
    // above).
    if (mSelection.empty()) return;
    std::vector<NoteRef> src = mSelection;
    beginEdit("Duplicate");

    double minBeat = std::numeric_limits<double>::max();
    double maxEnd  = 0.0;
    for (auto ref : src)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        const auto& n = mPM->currentPattern().drumRolls[pi].notes[ref.idx];
        minBeat = jmin(minBeat, n.startBeat);
        maxEnd  = jmax(maxEnd,  n.startBeat + n.durationBeats);
    }
    const double offset = maxEnd - minBeat;

    std::vector<std::pair<int,double>> newKeys;
    for (auto ref : src)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        PianoNote n = mPM->currentPattern().drumRolls[pi].notes[ref.idx];
        n.startBeat += offset;
        mPM->currentPattern().drumRolls[pi].notes.push_back(n);
        newKeys.push_back({ ref.row, n.startBeat });
    }
    for (int p = 0; p < (int) kMaxDrumPages; ++p)
        sortNotes(mPM->currentPattern().drumRolls[p].notes);
    rebuildSelectionFromKeys(newKeys);
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Nudging
// ─────────────────────────────────────────────────────────────────────────────
void DrumKitGrid::nudgeSelection(int snapUnits, int rowDelta)
{
    if (mPM == nullptr || mSelection.empty()) return;
    beginEdit("Nudge");
    const double snap = snapUnitBeats();

    // Snapshot original (row, beat, midi, vel, dur, ...) so we can rebuild.
    struct Tmp { int oldRow, newRow; PianoNote n; };
    std::vector<Tmp> tmp;
    for (auto ref : mSelection)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        const auto& roll = mPM->currentPattern().drumRolls[pi];
        if (ref.idx < 0 || ref.idx >= (int) roll.notes.size()) continue;
        Tmp t;
        t.oldRow = ref.row;
        t.newRow = jlimit(0, jmax(0, (int) mRowsCache.size() - 1), ref.row + rowDelta);
        t.n = roll.notes[ref.idx];
        t.n.startBeat = jmax(0.0, t.n.startBeat + snap * snapUnits);
        tmp.push_back(t);
    }

    // Erase originals (by index, descending per page).
    std::map<int, std::vector<int>> byPage;
    for (auto ref : mSelection)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi >= 0) byPage[pi].push_back(ref.idx);
    }
    for (auto& [pi, idxs] : byPage)
    {
        std::sort(idxs.begin(), idxs.end(), std::greater<int>());
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        for (int i : idxs) if (i >= 0 && i < (int) notes.size()) notes.erase(notes.begin() + i);
    }

    // Reinsert into (possibly new) rows.
    std::vector<std::pair<int,double>> keys;
    for (auto& t : tmp)
    {
        const int pi = rowToPageIndex(t.newRow);
        if (pi < 0) continue;
        mPM->currentPattern().drumRolls[pi].notes.push_back(t.n);
        keys.push_back({ t.newRow, t.n.startBeat });
    }
    for (int p = 0; p < (int) kMaxDrumPages; ++p)
        sortNotes(mPM->currentPattern().drumRolls[p].notes);
    rebuildSelectionFromKeys(keys);
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

void DrumKitGrid::nudgeSelectionFine(int pixels, int rowDelta)
{
    if (mPM == nullptr || mSelection.empty()) return;
    beginEdit("Nudge Fine");
    const double beatPerPixel = (mPPB > 0) ? 1.0 / mPPB : 0.0;

    struct Tmp { int oldRow, newRow; PianoNote n; };
    std::vector<Tmp> tmp;
    for (auto ref : mSelection)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        const auto& roll = mPM->currentPattern().drumRolls[pi];
        if (ref.idx < 0 || ref.idx >= (int) roll.notes.size()) continue;
        Tmp t;
        t.oldRow = ref.row;
        t.newRow = jlimit(0, jmax(0, (int) mRowsCache.size() - 1), ref.row + rowDelta);
        t.n = roll.notes[ref.idx];
        t.n.startBeat = jmax(0.0, t.n.startBeat + beatPerPixel * pixels);
        tmp.push_back(t);
    }
    std::map<int, std::vector<int>> byPage;
    for (auto ref : mSelection)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi >= 0) byPage[pi].push_back(ref.idx);
    }
    for (auto& [pi, idxs] : byPage)
    {
        std::sort(idxs.begin(), idxs.end(), std::greater<int>());
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        for (int i : idxs) if (i >= 0 && i < (int) notes.size()) notes.erase(notes.begin() + i);
    }
    std::vector<std::pair<int,double>> keys;
    for (auto& t : tmp)
    {
        const int pi = rowToPageIndex(t.newRow);
        if (pi < 0) continue;
        mPM->currentPattern().drumRolls[pi].notes.push_back(t.n);
        keys.push_back({ t.newRow, t.n.startBeat });
    }
    for (int p = 0; p < (int) kMaxDrumPages; ++p)
        sortNotes(mPM->currentPattern().drumRolls[p].notes);
    rebuildSelectionFromKeys(keys);
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slice tool
// ─────────────────────────────────────────────────────────────────────────────
void DrumKitGrid::sliceNotesOnLine(Point<int> start, Point<int> end)
{
    if (mPM == nullptr) return;
    beginEdit("Slice");
    for (int row = 0; row < (int) mRowsCache.size(); ++row)
    {
        const int pi = rowToPageIndex(row);
        if (pi < 0) continue;
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        std::vector<PianoNote> added;
        std::vector<int> toRemove;
        for (int i = 0; i < (int) notes.size(); ++i)
        {
            const auto& n = notes[i];
            const int x1 = beatToX(n.startBeat);
            const int x2 = beatToX(n.startBeat + n.durationBeats);
            const int cy = rowToY(row) + mRowH / 2;
            int lineX;
            if (end.y == start.y) lineX = (start.x + end.x) / 2;
            else
            {
                float t = (float) (cy - start.y) / (float) (end.y - start.y);
                lineX = (int) (start.x + t * (end.x - start.x));
            }
            if (lineX > x1 + 2 && lineX < x2 - 2)
            {
                const double sliceBeat = snapBeat(xToBeat(lineX));
                const double minDur    = 1.0 / (double) kTicksPerBeat;  // QA-Ee Stage 3: min slice fragment = 1 tick
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
        std::sort(toRemove.rbegin(), toRemove.rend());
        for (int i : toRemove) notes.erase(notes.begin() + i);
        for (const auto& a : added) notes.push_back(a);
        sortNotes(notes);
    }
    mSelection.clear();
    commitEdit();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Audition helpers
// ─────────────────────────────────────────────────────────────────────────────
void DrumKitGrid::triggerAudition(int rowIdx)
{
    if (rowIdx < 0 || rowIdx >= DrumKitSidebar::kNumRows) return;
    if (mAuditionHeldRow >= 0 && onRowAuditionOff) onRowAuditionOff(mAuditionHeldRow);
    mAuditionHeldRow = rowIdx;
    if (onRowAuditionOn) onRowAuditionOn(rowIdx);
}

void DrumKitGrid::releaseAudition()
{
    if (mAuditionHeldRow >= 0 && onRowAuditionOff) onRowAuditionOff(mAuditionHeldRow);
    mAuditionHeldRow = -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// keyPressed
// ─────────────────────────────────────────────────────────────────────────────
bool DrumKitGrid::keyPressed(const KeyPress& key)
{
    const bool ctrl  = key.getModifiers().isCtrlDown();
    const bool shift = key.getModifiers().isShiftDown();
    const bool alt   = key.getModifiers().isAltDown();

    // Tool shortcuts (no modifiers).
    if (! ctrl && ! alt && ! shift)
    {
        struct { int code; PRTool tool; } toolKeys[] = {
            { 'P', PRTool::Draw   }, { 'B', PRTool::Paint  },
            { 'D', PRTool::Delete }, { 'T', PRTool::Mute   },
            { 'C', PRTool::Slice  }, { 'E', PRTool::Select },
        };
        for (auto& tk : toolKeys)
            if (key.getKeyCode() == tk.code || key.getKeyCode() == (tk.code + 32))
            {
                mActiveTool = tk.tool; updateCursor();
                if (onToolChanged) onToolChanged(mActiveTool);
                return true;
            }
    }

    // Ctrl shortcuts.
    if (ctrl && alt && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
    {
        if (mUndoCtx.redo) mUndoCtx.redo();
        return true;
    }
    if (ctrl && ! alt)
    {
        if (! shift && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
        {
            if (mUndoCtx.undo) mUndoCtx.undo();
            return true;
        }
        if (! shift && (key.getKeyCode() == 'A' || key.getKeyCode() == 'a'))
            { selectAll(); return true; }
        if (! shift && (key.getKeyCode() == 'C' || key.getKeyCode() == 'c'))
            { copySelected(); return true; }
        if (! shift && (key.getKeyCode() == 'V' || key.getKeyCode() == 'v'))
            { pasteClipboard(); return true; }
        if (! shift && (key.getKeyCode() == 'B' || key.getKeyCode() == 'b'))
            { duplicateSelected(); return true; }
        if (! shift && (key.getKeyCode() == 'G' || key.getKeyCode() == 'g'))
            { toolGlue(); return true; }
        // 2026-04-26 (D-7): drum-applicable bundle.  Quick-legato + Ctrl+Up/Down
        // transpose are skipped - drum hits don't legato and rows are
        // slot-based, not pitch-based.
        if (! shift && (key.getKeyCode() == 'Q' || key.getKeyCode() == 'q'))
            { quickQuantizeQuarter(); return true; }
        if (! shift && (key.getKeyCode() == 'U' || key.getKeyCode() == 'u'))
            { if (! mSelection.empty()) toolChop(4); return true; }
        // 2026-04-26 (D-7): isKeyCode forces int comparison so the modifier
        // mask doesn't matter.  `key == KeyPress::deleteKey` resolves to
        // operator==(KeyPress&) and would compare modifiers, missing every
        // Ctrl-held press.
        if (! shift && (key.isKeyCode(KeyPress::deleteKey) || key.isKeyCode(KeyPress::backspaceKey)))
            { deleteTimeRegion(); return true; }
        // 2026-04-26 (D-7 sub-3): Ctrl+Left/Right shifts ruler box by length.
        if (! shift && key.isKeyCode(KeyPress::leftKey))
            { shiftTimeSelectionLeft();  return true; }
        if (! shift && key.isKeyCode(KeyPress::rightKey))
            { shiftTimeSelectionRight(); return true; }
    }

    // Ctrl+Alt+Home = flip resize-edge (right -> left).
    if (ctrl && alt && ! shift && key.isKeyCode(KeyPress::homeKey))
    {
        toggleResizeFromLeftMode();
        return true;
    }

    // Alt+letter = tools menu shortcuts.
    if (alt && ! ctrl && ! shift)
    {
        int kc = key.getKeyCode() | 32;
        if      (kc == 'q') { toolQuantize();  return true; }
        else if (kc == 's') { toolStrum();     return true; }
        else if (kc == 'u') { toolChop(4);     return true; }
        else if (kc == 'l') { toolArticulate();return true; }
        else if (kc == 'r') { toolRandomize(); return true; }
        else if (kc == 'f') { flamSelected();  return true; }   // D-7
        else if (kc == 'x') { scaleSelectionLevels(); return true; }   // D-7
    }

    // Shift+I = invert selection.
    if (shift && ! ctrl && ! alt && (key.getKeyCode() == 'I' || key.getKeyCode() == 'i'))
        { invertSelection(); return true; }

    // Delete / Backspace.
    if (! ctrl && (key == KeyPress::deleteKey || key == KeyPress::backspaceKey))
    {
        if (! mSelection.empty() && mPM != nullptr)
        {
            beginEdit("Delete");
            std::map<int, std::vector<int>> byPage;
            for (auto ref : mSelection)
            {
                const int pi = rowToPageIndex(ref.row);
                if (pi >= 0) byPage[pi].push_back(ref.idx);
            }
            for (auto& [pi, idxs] : byPage)
            {
                std::sort(idxs.begin(), idxs.end(), std::greater<int>());
                auto& notes = mPM->currentPattern().drumRolls[pi].notes;
                for (int i : idxs)
                    if (i >= 0 && i < (int) notes.size()) notes.erase(notes.begin() + i);
            }
            mSelection.clear();
            commitEdit();
            if (onNotesChanged) onNotesChanged();
            repaint();
        }
        return true;
    }

    // Shift+Z = Zoom tool; Shift+Arrows = nudge by snap unit.
    if (shift && ! ctrl && ! alt)
    {
        if (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z')
        {
            mActiveTool = PRTool::Zoom; updateCursor();
            if (onToolChanged) onToolChanged(mActiveTool);
            return true;
        }
        if (key.isKeyCode(KeyPress::leftKey))  { nudgeSelection(-1,  0); return true; }
        if (key.isKeyCode(KeyPress::rightKey)) { nudgeSelection(+1,  0); return true; }
        if (key.isKeyCode(KeyPress::upKey))    { nudgeSelection( 0, -1); return true; }
        if (key.isKeyCode(KeyPress::downKey))  { nudgeSelection( 0, +1); return true; }
    }

    // Alt+Arrows = nudge by one pixel (fine).
    if (alt && ! ctrl && ! shift)
    {
        if (key.isKeyCode(KeyPress::leftKey))  { nudgeSelectionFine(-1,  0); return true; }
        if (key.isKeyCode(KeyPress::rightKey)) { nudgeSelectionFine(+1,  0); return true; }
        if (key.isKeyCode(KeyPress::upKey))    { nudgeSelectionFine( 0, -1); return true; }
        if (key.isKeyCode(KeyPress::downKey))  { nudgeSelectionFine( 0, +1); return true; }
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse - hover cursor
// ─────────────────────────────────────────────────────────────────────────────
void DrumKitGrid::mouseMove(const MouseEvent& e)
{
    if (mActiveTool == PRTool::Draw || mActiveTool == PRTool::Select)
    {
        if (noteNearRightEdge(e.x, e.y).isValid())
            setMouseCursor(MouseCursor::LeftRightResizeCursor);
        else if (mActiveTool == PRTool::Select && noteAtPos(e.x, e.y).isValid())
            setMouseCursor(MouseCursor::DraggingHandCursor);
        else
            updateCursor();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse - button down
// ─────────────────────────────────────────────────────────────────────────────
void DrumKitGrid::mouseDown(const MouseEvent& e)
{
    if (mPM == nullptr) return;
    refreshRowsCache();   // ensure rowToPageIndex sees current drum state
    grabKeyboardFocus();

    // ── 2026-04-26 (D-7 sub-4): click-outside-time-range clears state ────
    if (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart
        && ! e.mods.isRightButtonDown())
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

    // Ruler zone: Ctrl+drag = time selection; bare click = seek.
    if (e.y < kRulerH)
    {
        if (e.mods.isCtrlDown() && ! e.mods.isRightButtonDown())
        {
            const double beat    = snapBeat(xToBeat(e.x));
            mTimeSelBeatAnchor   = beat;
            mTimeSelBeatStart    = beat;
            mTimeSelBeatEnd      = beat;
            mTimeSelDragging     = true;
            repaint();
        }
        else if (! e.mods.isRightButtonDown())
        {
            if (onSeek) onSeek(xToBeat(e.x));
        }
        return;
    }

    // Right-button: erase in draw/paint/delete tools.
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

    // 2026-04-26 (F-2): Ctrl+drag from empty area = marquee selection
    // regardless of active tool.  Click on a note still falls through so
    // tool-specific Ctrl+click semantics (Draw tool's toggle-selection) work.
    if (e.mods.isCtrlDown() && ! noteAtPos(e.x, e.y).isValid())
    {
        mMarqueeWasCtrl = true;       // Ctrl held = additive (Shift-style)
        mMarqueeActive  = true;
        mMarqueeStart   = e.getPosition();
        mMarqueeRect    = {};
        repaint();
        return;
    }

    // Left-button: dispatch to active tool.
    switch (mActiveTool)
    {
        // ── DRAW ──────────────────────────────────────────────────────────
        case PRTool::Draw:
        {
            if (e.mods.isCtrlDown())
            {
                const auto ref = noteAtPos(e.x, e.y);
                if (ref.isValid()) toggleSelection(ref);
                else               clearSelection();
                repaint();
                break;
            }

            // Resize check (right edge - or left edge if Ctrl+Alt+Home toggle on).
            const auto ri = noteNearRightEdge(e.x, e.y);
            if (ri.isValid())
            {
                beginEdit("Resize");
                mResizing         = true;
                mResizingFromLeft = mResizeFromLeftEnabled;
                mResizeRef        = ri;
                const auto* roll = rollForRow(ri.row);
                if (roll != nullptr && ri.idx < (int) roll->notes.size())
                {
                    mResizeOrigDur   = roll->notes[ri.idx].durationBeats;
                    mResizeOrigStart = roll->notes[ri.idx].startBeat;
                }
                break;
            }

            // Move check (click on existing note).
            const auto ni = noteAtPos(e.x, e.y);
            if (ni.isValid())
            {
                // 2026-04-26 (D-7): click memory - clicking an existing hit
                // remembers its duration so the next click-place uses it.
                if (auto* roll = rollForRow(ni.row);
                    roll != nullptr && ni.idx < (int) roll->notes.size())
                {
                    mClickMemoryDur = roll->notes[ni.idx].durationBeats;
                }

                beginEdit("Move");
                mMoving = true;
                mMoveDragOrigin = e.getPosition();
                if (isSelected(ni))
                    mMoveRefs = mSelection;
                else
                {
                    clearSelection();
                    mSelection.push_back(ni);
                    mMoveRefs = { ni };
                }
                mMoveOrigBeats.clear(); mMoveOrigRow.clear();
                for (auto ref : mMoveRefs)
                {
                    const auto* roll = rollForRow(ref.row);
                    if (roll == nullptr || ref.idx >= (int) roll->notes.size()) continue;
                    mMoveOrigBeats.push_back(roll->notes[ref.idx].startBeat);
                    mMoveOrigRow.push_back(ref.row);
                }
                if (! mMoveRefs.empty())
                    triggerAudition(mMoveRefs[0].row);
                repaint();
                break;
            }

            // Empty space: draw new note.
            beginEdit("Draw");
            mDrawing        = true;
            mDrawHasDragged = false;
            mDrawRow        = yToRow(e.y);
            mDrawStart      = snapBeat(xToBeat(e.x));
            // 2026-04-26 (D-7): preview at the click-memory length.
            mDrawEnd        = mDrawStart + mClickMemoryDur;
            triggerAudition(mDrawRow);
            repaint();
            break;
        }

        // ── PAINT ─────────────────────────────────────────────────────────
        case PRTool::Paint:
        {
            beginEdit("Paint");
            mDrawing   = true;
            mDrawRow   = yToRow(e.y);
            mDrawStart = snapBeat(xToBeat(e.x));
            mDrawEnd   = mDrawStart + 0.25;
            const int pi = rowToPageIndex(mDrawRow);
            if (pi >= 0)
            {
                PianoNote n;
                n.midiNote      = kKitMidiNote;
                n.startBeat     = mDrawStart;
                n.durationBeats = 0.25;
                n.velocity      = 0.8f;
                n.type          = mNewNoteType;
                mPM->currentPattern().drumRolls[pi].notes.push_back(n);
                sortNotes(mPM->currentPattern().drumRolls[pi].notes);
            }
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
            const auto ref = noteAtPos(e.x, e.y);
            if (ref.isValid())
            {
                beginEdit("Mute");
                auto* roll = rollForRow(ref.row);
                if (roll != nullptr && ref.idx < (int) roll->notes.size())
                    roll->notes[ref.idx].muted = ! roll->notes[ref.idx].muted;
                commitEdit();
                if (onNotesChanged) onNotesChanged();
                repaint();
            }
            break;
        }

        // ── SLICE ─────────────────────────────────────────────────────────
        case PRTool::Slice:
        {
            mSlicing    = true;
            mSliceStart = e.getPosition();
            mSliceEnd   = e.getPosition();
            repaint();
            break;
        }

        // ── SELECT ────────────────────────────────────────────────────────
        case PRTool::Select:
        {
            const auto ri = noteNearRightEdge(e.x, e.y);
            if (ri.isValid())
            {
                beginEdit("Resize");
                mResizing         = true;
                mResizingFromLeft = mResizeFromLeftEnabled;
                mResizeRef        = ri;
                const auto* roll = rollForRow(ri.row);
                if (roll != nullptr && ri.idx < (int) roll->notes.size())
                {
                    mResizeOrigDur   = roll->notes[ri.idx].durationBeats;
                    mResizeOrigStart = roll->notes[ri.idx].startBeat;
                }
                break;
            }

            const auto ni = noteAtPos(e.x, e.y);
            if (ni.isValid())
            {
                // 2026-04-26 (D-7): Select-tool click also primes click memory.
                if (auto* roll = rollForRow(ni.row);
                    roll != nullptr && ni.idx < (int) roll->notes.size())
                {
                    mClickMemoryDur = roll->notes[ni.idx].durationBeats;
                }

                if (! e.mods.isCtrlDown() && ! isSelected(ni))
                    clearSelection();
                if (! isSelected(ni)) mSelection.push_back(ni);

                beginEdit("Move");
                mMoving = true;
                mMoveDragOrigin = e.getPosition();
                mMoveRefs       = mSelection;
                mMoveOrigBeats.clear(); mMoveOrigRow.clear();
                for (auto ref : mMoveRefs)
                {
                    const auto* roll = rollForRow(ref.row);
                    if (roll == nullptr || ref.idx >= (int) roll->notes.size()) continue;
                    mMoveOrigBeats.push_back(roll->notes[ref.idx].startBeat);
                    mMoveOrigRow.push_back(ref.row);
                }
                if (! mMoveRefs.empty())
                    triggerAudition(mMoveRefs[0].row);
            }
            else
            {
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
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse - drag
// ─────────────────────────────────────────────────────────────────────────────
void DrumKitGrid::mouseDrag(const MouseEvent& e)
{
    // Ruler time selection drag.
    if (mTimeSelDragging)
    {
        const double beat = snapBeat(xToBeat(e.x));
        mTimeSelBeatStart = jmin(mTimeSelBeatAnchor, beat);
        mTimeSelBeatEnd   = jmax(mTimeSelBeatAnchor, beat);
        repaint();
        return;
    }

    if (mPM == nullptr) return;

    if (mErasing)
    {
        eraseAt(e.x, e.y);
        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    // Paint drag: add notes at each new snap position.
    if (mDrawing && mActiveTool == PRTool::Paint)
    {
        const double curBeat = snapBeat(xToBeat(e.x));
        const int    curRow  = yToRow(e.y);
        const int    pi      = rowToPageIndex(curRow);
        if (pi < 0) return;
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        bool exists = false;
        for (const auto& n : notes)
            if (std::abs(n.startBeat - curBeat) < 1e-9)
                { exists = true; break; }
        if (! exists)
        {
            PianoNote n;
            n.midiNote      = kKitMidiNote;
            n.startBeat     = curBeat;
            n.durationBeats = snapUnitBeats();
            n.velocity      = 0.8f;
            n.type          = mNewNoteType;
            notes.push_back(n);
            sortNotes(notes);
            if (onNotesChanged) onNotesChanged();
            repaint();
        }
        return;
    }

    // Draw drag: extend note duration preview.
    if (mDrawing)
    {
        double endBeat = snapBeat(xToBeat(e.x));
        if (e.mods.isAltDown()) endBeat = xToBeat(e.x);
        if (endBeat > mDrawStart)
        {
            // 2026-04-26 (D-7): only flip the dragged flag when the new end
            // beat differs from the click-memory preview length.  A pure
            // click leaves mDrawHasDragged false so mouseUp uses the
            // click-memory length on commit.
            if (std::abs(endBeat - (mDrawStart + mClickMemoryDur)) > 1.0e-6)
                mDrawHasDragged = true;
            mDrawEnd = endBeat;
        }
        repaint();
        return;
    }

    // Move drag (cross-row supported).
    if (mMoving && ! mMoveRefs.empty())
    {
        const bool shiftHeld = e.mods.isShiftDown();
        const bool ctrlHeld  = e.mods.isCtrlDown();
        const bool altHeld   = e.mods.isAltDown();

        double beatDelta = xToBeat(e.x) - xToBeat(mMoveDragOrigin.x);
        int    rowDelta  = (e.y - mMoveDragOrigin.y) / jmax(1, mRowH);

        if (shiftHeld) rowDelta  = 0;
        if (ctrlHeld)  beatDelta = 0.0;

        // Snap delta using the first reference.
        if (! altHeld && ! ctrlHeld && ! mMoveOrigBeats.empty())
        {
            const double snappedFirst = snapBeat(mMoveOrigBeats[0] + beatDelta);
            beatDelta = snappedFirst - mMoveOrigBeats[0];
        }

        // Build a list of (newRow, newBeat, originalNote) by reading orig
        // values and applying delta.  Then erase originals + reinsert at new
        // locations.  We rebuild mMoveRefs at the end so subsequent drag
        // events see consistent indices.
        struct Tmp { int newRow; PianoNote n; };
        std::vector<Tmp> tmps;
        tmps.reserve(mMoveRefs.size());
        for (size_t i = 0; i < mMoveRefs.size(); ++i)
        {
            const int oldRow = mMoveOrigRow[i];
            const int pi = rowToPageIndex(oldRow);
            if (pi < 0) continue;
            const auto& roll = mPM->currentPattern().drumRolls[pi];
            if (mMoveRefs[i].idx < 0 || mMoveRefs[i].idx >= (int) roll.notes.size()) continue;
            Tmp t;
            t.n = roll.notes[mMoveRefs[i].idx];
            t.n.startBeat = jmax(0.0, mMoveOrigBeats[i] + beatDelta);
            t.newRow = jlimit(0, jmax(0, (int) mRowsCache.size() - 1), oldRow + rowDelta);
            tmps.push_back(t);
        }
        // Erase originals (descending per page).
        std::map<int, std::vector<int>> byPage;
        for (auto ref : mMoveRefs)
        {
            const int pi = rowToPageIndex(ref.row);
            if (pi >= 0) byPage[pi].push_back(ref.idx);
        }
        for (auto& [pi, idxs] : byPage)
        {
            std::sort(idxs.begin(), idxs.end(), std::greater<int>());
            auto& notes = mPM->currentPattern().drumRolls[pi].notes;
            for (int i : idxs) if (i >= 0 && i < (int) notes.size()) notes.erase(notes.begin() + i);
        }
        // Reinsert into (possibly new) rows.
        std::vector<NoteRef> newRefs;
        std::vector<double>  newOrigBeats;
        std::vector<int>     newOrigRow;
        for (auto& t : tmps)
        {
            const int pi = rowToPageIndex(t.newRow);
            if (pi < 0) continue;
            mPM->currentPattern().drumRolls[pi].notes.push_back(t.n);
            const int newIdx = (int) mPM->currentPattern().drumRolls[pi].notes.size() - 1;
            newRefs.push_back({ t.newRow, newIdx });
            newOrigBeats.push_back(t.n.startBeat - beatDelta);
            newOrigRow.push_back(t.newRow - rowDelta);
        }
        mMoveRefs      = std::move(newRefs);
        mMoveOrigBeats = std::move(newOrigBeats);
        mMoveOrigRow   = std::move(newOrigRow);
        mSelection     = mMoveRefs;

        // Audition the primary row when it changes during drag.
        if (! mMoveRefs.empty() && mMoveRefs[0].row != mAuditionHeldRow)
            triggerAudition(mMoveRefs[0].row);

        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    // Resize drag.
    if (mResizing && mResizeRef.isValid())
    {
        auto* roll = rollForRow(mResizeRef.row);
        if (roll != nullptr && mResizeRef.idx < (int) roll->notes.size())
        {
            auto& n = roll->notes[mResizeRef.idx];
            // QA-Ee Stage 3: snap ON -> min one snap step; free (Alt / Off) -> 1 tick.
            const int    rSnapDiv = onGetSnapDiv ? onGetSnapDiv() : 1;
            const double minDur   = (e.mods.isAltDown() || rSnapDiv <= 0)
                                      ? (1.0 / (double) kTicksPerBeat) : snapUnitBeats();
            if (mResizingFromLeft)
            {
                // 2026-04-26 (D-7): drag the LEFT edge - origEnd stays put.
                const double origEnd  = mResizeOrigStart + mResizeOrigDur;
                const double rawStart = e.mods.isAltDown() ? xToBeat(e.x) : snapBeat(xToBeat(e.x));
                const double newStart = jmax(0.0, jmin(rawStart, origEnd - minDur));
                n.startBeat     = newStart;
                n.durationBeats = origEnd - newStart;
            }
            else
            {
                const double newEnd = e.mods.isAltDown() ? xToBeat(e.x) : snapBeat(xToBeat(e.x));
                n.durationBeats = jmax(minDur, newEnd - n.startBeat);
            }
            if (onNotesChanged) onNotesChanged();
            repaint();
        }
        return;
    }

    // Marquee drag.
    if (mMarqueeActive)
    {
        mMarqueeRect = Rectangle<int>::leftTopRightBottom(
            jmin(mMarqueeStart.x, e.x), jmin(mMarqueeStart.y, e.y),
            jmax(mMarqueeStart.x, e.x), jmax(mMarqueeStart.y, e.y));
        repaint();
        return;
    }

    // Slice drag.
    if (mSlicing)
    {
        mSliceEnd = e.getPosition();
        repaint();
        return;
    }

    // Zoom-rect drag.
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
void DrumKitGrid::mouseUp(const MouseEvent&)
{
    if (mTimeSelDragging)
    {
        mTimeSelDragging = false;
        if (mTimeSelBeatEnd - mTimeSelBeatStart < 0.01)
        {
            mTimeSelBeatStart = mTimeSelBeatEnd = -1.0;
        }
        else if (mPM != nullptr)
        {
            // 2026-04-26 (D-7): mirror PianoRollGrid auto-select - every drum
            // hit (any row) whose start lies inside the ruler range becomes
            // selected so subsequent ops act on it without an extra marquee.
            const double t0 = jmin(mTimeSelBeatStart, mTimeSelBeatEnd);
            const double t1 = jmax(mTimeSelBeatStart, mTimeSelBeatEnd);
            mSelection.clear();
            for (int row = 0; row < (int) mRowsCache.size(); ++row)
            {
                const int pi = rowToPageIndex(row);
                if (pi < 0) continue;
                const auto& notes = mPM->currentPattern().drumRolls[pi].notes;
                for (int i = 0; i < (int) notes.size(); ++i)
                    if (notes[i].startBeat >= t0 && notes[i].startBeat < t1)
                        mSelection.push_back({ row, i });
            }
        }
        repaint();
        return;
    }

    if (mPM == nullptr) { mDrawing = mErasing = mMoving = mResizing = false; return; }

    // Commit drawn note (Draw tool).
    if (mDrawing && mDrawRow >= 0 && mActiveTool == PRTool::Draw)
    {
        const int pi = rowToPageIndex(mDrawRow);
        if (pi >= 0)
        {
            // 2026-04-26 (D-7): click memory - click-only placement uses the
            // remembered duration; drag-to-place uses the dragged length.
            // Drum hits don't carry slide / portamento so type isn't tracked.
            // QA-Ee Stage 3: snap ON -> min one snap step; Off -> 1 tick.
            const double drawMin = (onGetSnapDiv && onGetSnapDiv() <= 0)
                                     ? (1.0 / (double) kTicksPerBeat) : snapUnitBeats();
            const double dur = mDrawHasDragged
                ? jmax(drawMin, mDrawEnd - mDrawStart)
                : mClickMemoryDur;
            // 2026-04-26 (D-7 sub-4 EC-1, revised): drawing a new hit INSIDE
            // an active ruler time-range clears the existing selection but
            // preserves the ruler range itself.  The new hit is NOT added
            // to the selection.
            const bool drawnInsideRange =
                (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart
                 && mDrawStart >= mTimeSelBeatStart
                 && mDrawStart <  mTimeSelBeatEnd);
            if (drawnInsideRange) mSelection.clear();

            PianoNote n;
            n.midiNote      = kKitMidiNote;
            n.startBeat     = mDrawStart;
            n.durationBeats = dur;
            n.velocity      = 0.8f;
            n.type          = mNewNoteType;
            mPM->currentPattern().drumRolls[pi].notes.push_back(n);
            sortNotes(mPM->currentPattern().drumRolls[pi].notes);
        }
        commitEdit();
        if (onNotesChanged) onNotesChanged();
        mDrawHasDragged = false;
    }

    if (mDrawing && mActiveTool == PRTool::Paint) { commitEdit(); }
    if (mErasing) { commitEdit(); }

    if (mMoving)
    {
        std::vector<std::pair<int,double>> keys;
        for (auto ref : mMoveRefs)
        {
            const int pi = rowToPageIndex(ref.row);
            if (pi < 0) continue;
            const auto& roll = mPM->currentPattern().drumRolls[pi];
            if (ref.idx >= 0 && ref.idx < (int) roll.notes.size())
                keys.push_back({ ref.row, roll.notes[ref.idx].startBeat });
        }
        for (int p = 0; p < (int) kMaxDrumPages; ++p)
            sortNotes(mPM->currentPattern().drumRolls[p].notes);
        rebuildSelectionFromKeys(keys);
        commitEdit();
        if (onNotesChanged) onNotesChanged();
        mMoveRefs.clear(); mMoveOrigBeats.clear(); mMoveOrigRow.clear();
    }

    if (mResizing)
    {
        for (int p = 0; p < (int) kMaxDrumPages; ++p)
            sortNotes(mPM->currentPattern().drumRolls[p].notes);
        commitEdit();
        if (onNotesChanged) onNotesChanged();
        mResizeRef = {};
    }

    if (mMarqueeActive)
    {
        finaliseMarquee();
        mMarqueeActive = false; mMarqueeRect = {};
    }

    if (mSlicing)
    {
        if (mSliceStart != mSliceEnd) sliceNotesOnLine(mSliceStart, mSliceEnd);
        mSlicing = false;
    }

    if (mZoomRectActive)
    {
        if (mZoomRect.getWidth() > 8 && mZoomRect.getHeight() > 8 && mPPB > 0)
        {
            const double beatStart = xToBeat(mZoomRect.getX());
            const double beatEnd   = xToBeat(mZoomRect.getRight());
            if (beatEnd > beatStart && onZoomTo) onZoomTo(beatStart, beatEnd);
        }
        else
        {
            if (onZoom) onZoom(mPPB * 0.3f);
        }
        mZoomRectActive = false; mZoomRect = {};
    }

    mDrawing  = false;
    mErasing  = false;
    mMoving   = false;
    mResizing = false;
    mDrawRow  = -1;
    releaseAudition();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse - double-click on note → context menu (Velocity / MIDI Note / Delete)
// ─────────────────────────────────────────────────────────────────────────────
void DrumKitGrid::mouseDoubleClick(const MouseEvent& e)
{
    if (mPM == nullptr) return;
    refreshRowsCache();
    const auto ref = noteAtPos(e.x, e.y);
    if (! ref.isValid()) return;
    showNoteContextMenu(ref);
}

void DrumKitGrid::showNoteContextMenu(NoteRef ref)
{
    if (mPM == nullptr || ! ref.isValid()) return;
    const int pi = rowToPageIndex(ref.row);
    if (pi < 0) return;
    auto& roll = mPM->currentPattern().drumRolls[pi];
    if (ref.idx < 0 || ref.idx >= (int) roll.notes.size()) return;

    // Anchor the popup to the note's screen rect so it pops next to the
    // block the user just double-clicked.
    const auto& n   = roll.notes[ref.idx];
    const int    x  = beatToX(n.startBeat);
    const int    w  = jmax(3, (int)(jmax(0.0625, n.durationBeats) * mPPB) - 1);
    const int    y  = rowToY(ref.row);
    const auto noteScreen = localAreaToGlobal(Rectangle<int>(x, y, w, jmax(1, mRowH)));

    PopupMenu m;
    m.addItem(1, "Velocity...");
    m.addItem(2, "MIDI Note...");
    m.addSeparator();
    m.addItem(3, "Delete");

    m.showMenuAsync(PopupMenu::Options().withTargetScreenArea(noteScreen),
        [this, ref](int r)
        {
            if (mPM == nullptr) return;
            const int pi2 = rowToPageIndex(ref.row);
            if (pi2 < 0) return;
            auto& roll2 = mPM->currentPattern().drumRolls[pi2];
            if (ref.idx < 0 || ref.idx >= (int) roll2.notes.size()) return;

            if      (r == 1) promptVelocity(ref);
            else if (r == 2) promptMidiNote(ref);
            else if (r == 3)
            {
                beginEdit("Delete");
                roll2.notes.erase(roll2.notes.begin() + ref.idx);
                mSelection.clear();
                commitEdit();
                if (onNotesChanged) onNotesChanged();
                repaint();
            }
        });
}

void DrumKitGrid::promptVelocity(NoteRef ref)
{
    if (mPM == nullptr || ! ref.isValid()) return;
    const int pi = rowToPageIndex(ref.row);
    if (pi < 0) return;
    auto& notes = mPM->currentPattern().drumRolls[pi].notes;
    if (ref.idx < 0 || ref.idx >= (int) notes.size()) return;

    const int curMidi = jlimit(0, 127, juce::roundToInt(notes[ref.idx].velocity * 127.f));
    auto* aw = new AlertWindow("Velocity",
                                "Enter velocity (0..127):",
                                MessageBoxIconType::NoIcon);
    aw->addTextEditor("v", String(curMidi));
    aw->addButton("OK", 1);
    aw->addButton("Cancel", 0);
    aw->enterModalState(true,
        ModalCallbackFunction::create(
            [this, aw, ref](int result)
            {
                if (result != 1 || mPM == nullptr) return;
                const int pi2 = rowToPageIndex(ref.row);
                if (pi2 < 0) return;
                auto& notes2 = mPM->currentPattern().drumRolls[pi2].notes;
                if (ref.idx < 0 || ref.idx >= (int) notes2.size()) return;
                const int v = jlimit(0, 127, aw->getTextEditorContents("v").getIntValue());
                beginEdit("Velocity");
                notes2[ref.idx].velocity = (float) v / 127.f;
                commitEdit();
                if (onNotesChanged) onNotesChanged();
                repaint();
            }),
        true);
}

void DrumKitGrid::promptMidiNote(NoteRef ref)
{
    if (mPM == nullptr || ! ref.isValid()) return;
    const int pi = rowToPageIndex(ref.row);
    if (pi < 0) return;
    auto& notes = mPM->currentPattern().drumRolls[pi].notes;
    if (ref.idx < 0 || ref.idx >= (int) notes.size()) return;

    const int curMidi = jlimit(0, 127, notes[ref.idx].midiNote);
    const String curName = midiToName(curMidi);
    auto* aw = new AlertWindow("MIDI Note",
                                "Enter note name or MIDI number\n"
                                "(e.g. C5 / c5 / C#5 / Db5 / 60).\n"
                                "Current: " + curName + " = " + String(curMidi),
                                MessageBoxIconType::NoIcon);
    aw->addTextEditor("n", curName);
    aw->addButton("OK", 1);
    aw->addButton("Cancel", 0);
    aw->enterModalState(true,
        ModalCallbackFunction::create(
            [this, aw, ref](int result)
            {
                if (result != 1 || mPM == nullptr) return;
                const int pi2 = rowToPageIndex(ref.row);
                if (pi2 < 0) return;
                auto& notes2 = mPM->currentPattern().drumRolls[pi2].notes;
                if (ref.idx < 0 || ref.idx >= (int) notes2.size()) return;
                const int newMidi = parseNoteOrMidi(aw->getTextEditorContents("n"));
                if (newMidi < 0)
                {
                    AlertWindow::showMessageBoxAsync(
                        MessageBoxIconType::WarningIcon,
                        "Invalid Note",
                        "Couldn't parse that as a note name or MIDI number.\n"
                        "Examples: C5, c5, C#5, Db5, 60.");
                    return;
                }
                beginEdit("MIDI Note");
                notes2[ref.idx].midiNote = newMidi;
                commitEdit();
                if (onNotesChanged) onNotesChanged();
                repaint();
            }),
        true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse - wheel
// ─────────────────────────────────────────────────────────────────────────────
void DrumKitGrid::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (e.mods.isRightButtonDown() && ! mErasing)
    {
        static const PRTool tools[] = {
            PRTool::Draw, PRTool::Paint, PRTool::Delete,
            PRTool::Mute, PRTool::Slice, PRTool::Select, PRTool::Zoom };
        int cur = 0;
        for (int i = 0; i < 7; ++i) if (tools[i] == mActiveTool) { cur = i; break; }
        cur = (cur + (wheel.deltaY > 0.f ? 6 : 1)) % 7;
        mActiveTool = tools[cur]; updateCursor();
        if (onToolChanged) onToolChanged(mActiveTool);
        return;
    }

    // QA-Ee: no vertical zoom on the drum kit -- the 16 rows are fixed by design.
    // Alt+scroll falls through to horizontal scroll like the bare wheel.
    if (e.mods.isCtrlDown())
    {
        const float factor = (wheel.deltaY > 0.f) ? 1.15f : (1.f / 1.15f);
        if (onZoomAnchored) onZoomAnchored(factor, e.x);   // anchor zoom to cursor
        else if (onZoom)    onZoom(mPPB * factor - mPPB);
    }
    else if (e.mods.isShiftDown())
    {
        if (onHScroll) onHScroll(-wheel.deltaY * 2.0);
    }
    else
    {
        if (onHScroll) onHScroll(-wheel.deltaY * 2.0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────
void DrumKitGrid::paint(Graphics& g)
{
    refreshRowsCache();   // keep cache in sync with drum tabs
    auto b = getLocalBounds();
    g.fillAll(VC::Bg);
    if (mPPB <= 0) return;

    const int rowH = jmax(1, mRowH);
    const int rowsTop = mRowYOffset;

    // Row backgrounds - alternate brightness so adjacent drums are distinct.
    for (int r = 0; r < DrumKitSidebar::kNumRows; ++r)
    {
        const int y = rowsTop + r * rowH;
        const bool isAlt = (r % 2) == 0;
        g.setColour(isAlt ? VC::Panel : VC::Bg.brighter(0.05f));
        g.fillRect(0, y, b.getWidth(), rowH);
        g.setColour(VC::Accent.withAlpha(0.3f));
        g.drawHorizontalLine(y + rowH - 1, 0.f, (float) b.getWidth());
    }

    // QA-Ee Stage 3: shared straight/triplet ladder, zoom-adaptive depth (same rule
    // as Piano Roll + Builder -- one source of truth).  Snap TYPE picks the ladder;
    // the snap DIVISION never caps depth (every rung clearing 5px is drawn, down to
    // 1/64 straight / 1/6 Step triplet).  Bar lines are the separate TS-aware pass
    // below, so the 384t bar rung is skipped.  Fine -> coarse so coarser overdraw.
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
                const int x = beatToX(beat);
                if (x < 0 || x > b.getWidth()) continue;
                g.drawVerticalLine(x, (float) rowsTop, (float) b.getHeight());
            }
        }
    }
    // Bar lines (every patternBpb PPQ beats - pattern's intrinsic TS).
    // C.5b: 4/4 → 4-beat bars; 3/4 → 3-beat bars; 6/8 → 3-beat bars.
    {
        const double barBpb = (mPM != nullptr)
            ? mPM->getPatternBeatsPerBar (mPM->getCurrentPatternIndex())
            : 4.0;
        const double safeBpb = juce::jmax (1.0, barBpb);
        // QA-Ee Stage 2: declutter -- cull bar lines once they fall below kMinLinePx.
        if ((double) mPPB * safeBpb >= (double) kMinLinePx)
        {
            const double startBeat = std::floor(mBeatOff / safeBpb) * safeBpb;
            for (double beat = startBeat;
                 beat <= mBeatOff + b.getWidth() / mPPB + safeBpb;
                 beat += safeBpb)
            {
                const int x = beatToX(beat);
                if (x < 0 || x > b.getWidth()) continue;
                g.setColour(VC::Accent.brighter(0.3f));
                g.drawVerticalLine(x, 0, (float) b.getHeight());
                g.setColour(VC::TextDim); g.setFont(Font(9));
                // QA-Ea Task 0c (2026-05-20): 0-indexed bar labels (song downbeat = "0").
                g.drawText(String((int) std::round (beat / safeBpb)),
                           x + 2, 2, 24, 10, Justification::centredLeft);
            }
        }
    }

    // Time selection highlight.
    if (mTimeSelBeatStart >= 0.0 && mTimeSelBeatEnd > mTimeSelBeatStart)
    {
        const int sx = beatToX(mTimeSelBeatStart);
        const int ex = beatToX(mTimeSelBeatEnd);
        g.setColour(VC::Highlight.withAlpha(0.08f));
        g.fillRect(sx, 0, ex - sx, b.getHeight());
        g.setColour(VC::Highlight.withAlpha(0.30f));
        g.fillRect(sx, 0, ex - sx, kRulerH);
        g.setColour(VC::Highlight.withAlpha(0.85f));
        g.drawVerticalLine(sx, 0.f, (float) kRulerH);
        g.drawVerticalLine(ex, 0.f, (float) kRulerH);
    }

    // Notes.
    if (mPM != nullptr)
    {
        for (int row = 0; row < (int) mRowsCache.size(); ++row)
        {
            const auto& info = mRowsCache[row];
            if (info.pageIndex < 0) continue;
            const auto& roll = mPM->currentPattern().drumRolls[info.pageIndex];
            for (int ni = 0; ni < (int) roll.notes.size(); ++ni)
            {
                const auto& n = roll.notes[ni];
                const int x = beatToX(n.startBeat);
                const int w = jmax(3, (int) (jmax(0.0625, n.durationBeats) * mPPB) - 1);
                const int y = rowToY(row);
                if (x + w < 0 || x > b.getWidth())   continue;
                if (y + rowH < 0 || y > b.getHeight()) continue;

                Colour noteCol = info.color.withMultipliedLightness(0.6f + n.velocity * 0.4f);
                if (n.muted) noteCol = info.color.withSaturation(0.12f).withMultipliedLightness(0.4f).withAlpha(0.45f);

                const float nx = (float) x;
                const float ny = (float) (y + 1);
                const float nw = (float) w;
                const float nh = (float) (rowH - 2);

                g.setGradientFill(ColourGradient(
                    noteCol.brighter(0.30f), nx, ny,
                    noteCol.darker (0.20f), nx, ny + nh, false));
                g.fillRoundedRectangle(nx, ny, nw, nh, 2.f);

                g.setColour(noteCol.brighter(0.65f).withAlpha(0.7f));
                g.drawLine(nx + 2.5f, ny + 1.f, nx + nw - 2.5f, ny + 1.f, 1.f);
                g.setColour(Colour(0xff000000).withAlpha(0.35f));
                g.drawLine(nx + 2.5f, ny + nh - 1.f, nx + nw - 2.5f, ny + nh - 1.f, 1.f);

                const NoteRef ref { row, ni };
                if (isSelected(ref))
                {
                    g.setColour(Colours::white.withAlpha(0.2f));
                    g.drawRoundedRectangle(nx - 1.f, ny - 1.f, nw + 2.f, nh + 2.f, 3.f, 1.5f);
                    g.setColour(Colours::white.withAlpha(0.95f));
                    g.drawRoundedRectangle(nx + 0.5f, ny + 0.5f, nw - 1.f, nh - 1.f, 2.f, 1.5f);
                }
                else
                {
                    g.setColour(noteCol.brighter(0.3f).withAlpha(0.55f));
                    g.drawRoundedRectangle(nx + 0.5f, ny + 0.5f, nw - 1.f, nh - 1.f, 2.f, 1.f);
                }

                // Retune dot - top-right when not C5 (kit-grid placement note).
                if (n.midiNote != kKitMidiNote && w >= 12 && rowH >= 10)
                {
                    const float dotR = 4.f;
                    const auto dot = Rectangle<float>(nx + nw - dotR - 2.f, ny + 2.f, dotR, dotR);
                    g.setColour(Colours::white);
                    g.fillEllipse(dot);
                    g.setColour(Colours::black);
                    g.drawEllipse(dot, 1.f);
                }
            }
        }
    }

    // Preview note being drawn (Draw tool).
    if (mDrawing && mDrawRow >= 0 && mActiveTool == PRTool::Draw)
    {
        const int x = beatToX(mDrawStart);
        const int w = jmax(3, (int) ((mDrawEnd - mDrawStart) * mPPB) - 1);
        const int y = rowToY(mDrawRow);
        g.setColour(VC::Highlight.withAlpha(0.7f));
        g.fillRoundedRectangle((float) x, (float) (y + 1), (float) w, (float) (rowH - 2), 2.f);
    }

    // Marquee.
    if (mMarqueeActive && ! mMarqueeRect.isEmpty())
    {
        g.setColour(VC::Blue.withAlpha(0.15f));
        g.fillRect(mMarqueeRect);
        g.setColour(VC::Blue.withAlpha(0.8f));
        g.drawRect(mMarqueeRect, 1);
    }

    // Slice line.
    if (mSlicing && mSliceStart != mSliceEnd)
    {
        g.setColour(VC::Highlight.withAlpha(0.85f));
        g.drawLine((float) mSliceStart.x, (float) mSliceStart.y,
                   (float) mSliceEnd.x,   (float) mSliceEnd.y, 2.f);
        g.fillEllipse((float) mSliceStart.x - 3, (float) mSliceStart.y - 3, 6, 6);
        g.fillEllipse((float) mSliceEnd.x   - 3, (float) mSliceEnd.y   - 3, 6, 6);
    }

    // Zoom-to-rect overlay.
    if (mZoomRectActive && ! mZoomRect.isEmpty())
    {
        g.setColour(VC::Yellow.withAlpha(0.1f));
        g.fillRect(mZoomRect);
        g.setColour(VC::Yellow.withAlpha(0.7f));
        g.drawRect(mZoomRect, 1);
    }

    // Ruler strip at top.
    g.setColour(VC::Panel.brighter(0.12f));
    g.fillRect(0, 0, b.getWidth(), kRulerH);
    g.setColour(VC::Accent.withAlpha(0.5f));
    g.drawHorizontalLine(kRulerH - 1, 0.f, (float) b.getWidth());
    // C.5b: ruler bar boundaries follow pattern's intrinsic TS.
    const double rulerBarBpb = (mPM != nullptr)
        ? juce::jmax (1.0, mPM->getPatternBeatsPerBar (mPM->getCurrentPatternIndex()))
        : 4.0;
    for (double beat = std::floor(mBeatOff); beat <= mBeatOff + b.getWidth() / mPPB + 1.0; beat += 0.5)
    {
        const int rx = beatToX(beat);
        if (rx < 0 || rx > b.getWidth()) continue;
        const double barFrac = beat / rulerBarBpb;
        const bool isBar  = (std::abs (barFrac - std::round (barFrac)) < 1e-6);
        const bool isBeat = (std::fmod(beat, 1.0) < 1e-9);
        if (isBar)
        {
            g.setColour(VC::Accent.brighter(0.5f));
            g.drawVerticalLine(rx, 0, (float) kRulerH);
            g.setColour(VC::Text); g.setFont(Font(9));
            // QA-Ea Task 0c (2026-05-20): 0-indexed bar labels (song downbeat = "0").
            g.drawText(String((int) std::round (barFrac)), rx + 2, 1, 20, kRulerH - 2,
                       Justification::centredLeft, false);
        }
        else if (isBeat)
        {
            g.setColour(VC::Accent.withAlpha(0.6f));
            g.drawVerticalLine(rx, kRulerH / 2, (float) kRulerH);
        }
    }
    // Playhead arrow in ruler.
    if (mPlayhead >= 0.0)
    {
        const int px = beatToX(mPlayhead);
        if (px >= 0 && px <= b.getWidth())
        {
            g.setColour(VC::Green.withAlpha(0.9f));
            Path tri;
            tri.addTriangle((float) px, 0.f, (float)(px - 5), (float) kRulerH,
                            (float)(px + 5), (float) kRulerH);
            g.fillPath(tri);
        }
    }
    // Playhead body line.
    if (mPlayhead >= 0.0)
    {
        const int px = beatToX(mPlayhead);
        if (px >= 0 && px <= b.getWidth())
        {
            g.setColour(VC::Green.withAlpha(0.8f));
            g.fillRect(px, kRulerH, 2, b.getHeight() - kRulerH);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tools menu + algorithms
// ─────────────────────────────────────────────────────────────────────────────
std::vector<DrumKitGrid::NoteRef> DrumKitGrid::getWorkingSet() const
{
    // 2026-04-26 (D-7): selection-only - mirrors PianoRollGrid::getWorkingSet
    // change.  Previously fell back to "every drum note across every row" when
    // mSelection was empty, which made tool shortcuts act on freshly-drawn
    // hits without an explicit highlight.
    if (mPM == nullptr || mSelection.empty()) return {};
    return mSelection;
}

void DrumKitGrid::showToolsMenu()
{
    PopupMenu menu;
    menu.addItem(1, "Quantize          Alt+Q");
    menu.addItem(2, "Strum             Alt+S");

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

    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(this), [this](int r)
    {
        if      (r == 1) toolQuantize();
        else if (r == 2) toolStrum();
        else if (r == 4) toolGlue();
        else if (r == 5) toolArticulate();
        else if (r == 6) toolRandomize();
        else if (r >= 10 && r <= 14)
        {
            constexpr int kDivs[] = { 2, 3, 4, 6, 8 };
            toolChop(kDivs[r - 10]);
        }
    });
}

void DrumKitGrid::toolQuantize()
{
    if (mPM == nullptr) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Quantize");
    const double snap = snapUnitBeats();
    for (auto ref : targets)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        if (ref.idx >= 0 && ref.idx < (int) notes.size())
            notes[ref.idx].startBeat = std::round(notes[ref.idx].startBeat / snap) * snap;
    }
    for (int p = 0; p < (int) kMaxDrumPages; ++p)
        sortNotes(mPM->currentPattern().drumRolls[p].notes);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// D-7 (2026-04-26): Smaller piano roll bundle helpers - drum kit variants
// ─────────────────────────────────────────────────────────────────────────────

void DrumKitGrid::quickQuantizeQuarter()
{
    if (mPM == nullptr || mSelection.empty()) return;
    beginEdit("Quick Quantize 1/4");
    constexpr double snap = 1.0;   // 1 beat
    for (auto ref : mSelection)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        if (ref.idx >= 0 && ref.idx < (int) notes.size())
            notes[ref.idx].startBeat = std::round(notes[ref.idx].startBeat / snap) * snap;
    }
    for (int p = 0; p < (int) kMaxDrumPages; ++p)
        sortNotes(mPM->currentPattern().drumRolls[p].notes);
    commitEdit();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void DrumKitGrid::flamSelected()
{
    if (mPM == nullptr || mSelection.empty()) return;
    beginEdit("Flam");

    constexpr double kGraceLen = 4.0 / 32.0;   // 1/32 note in beats
    std::map<int, std::vector<PianoNote>> toAdd;
    for (auto ref : mSelection)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        const auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        if (ref.idx < 0 || ref.idx >= (int) notes.size()) continue;
        const PianoNote& src = notes[ref.idx];
        const double graceStart = src.startBeat - kGraceLen;
        if (graceStart < 0.0) continue;
        PianoNote g = src;
        g.startBeat     = graceStart;
        g.durationBeats = kGraceLen;
        g.velocity      = juce::jlimit(0.05f, 1.0f, src.velocity * 0.6f);
        g.groupId       = -1;
        toAdd[pi].push_back(g);
    }
    for (auto& [pi, list] : toAdd)
    {
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        for (auto& g : list) notes.push_back(g);
        sortNotes(notes);
    }
    commitEdit();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void DrumKitGrid::deleteTimeRegion()
{
    if (mPM == nullptr) return;

    // Ruler range wins (the highlighted box's width is exactly what the
    // user expects to disappear).  Selection bounds are the fallback when
    // no ruler range is set.  Erase rule is "starts in [t0, t1)".
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
        for (auto ref : mSelection)
        {
            const int pi = rowToPageIndex(ref.row);
            if (pi < 0) continue;
            const auto& notes = mPM->currentPattern().drumRolls[pi].notes;
            if (ref.idx < 0 || ref.idx >= (int) notes.size()) continue;
            const auto& n = notes[ref.idx];
            t0 = jmin(t0, n.startBeat);
            t1 = jmax(t1, n.startBeat + n.durationBeats);
        }
        haveRange = (t1 > t0);
    }
    if (!haveRange) return;

    const double removedLen = t1 - t0;
    if (removedLen <= 0.0) return;

    constexpr double kEps = 1.0e-6;
    beginEdit("Delete Time");
    for (int p = 0; p < (int) kMaxDrumPages; ++p)
    {
        auto& notes = mPM->currentPattern().drumRolls[p].notes;
        // 1) erase notes whose START lies in [t0, t1)
        for (int i = (int) notes.size() - 1; i >= 0; --i)
        {
            const double s = notes[i].startBeat;
            if (s >= t0 - kEps && s < t1 - kEps)
                notes.erase(notes.begin() + i);
        }
        // 2) shift every note that starts at or after t1 left by removedLen
        for (auto& n : notes)
            if (n.startBeat >= t1 - kEps) n.startBeat -= removedLen;
        sortNotes(notes);
    }
    clearSelection();
    mTimeSelBeatStart = mTimeSelBeatEnd = -1.0;
    commitEdit();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void DrumKitGrid::toggleResizeFromLeftMode()
{
    mResizeFromLeftEnabled = !mResizeFromLeftEnabled;
    if (auto* mainWin = getTopLevelComponent())
        mainWin->repaint();
}

namespace { struct DrumScaleLevelsHost : public juce::Component
{
    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    DrumScaleLevelsHost()
    {
        slider.setRange (0.0, 200.0, 1.0);
        slider.setValue (100.0);
        slider.setTextValueSuffix (" %");
        addAndMakeVisible (slider);
        setSize (360, 32);
    }
    void resized() override { slider.setBounds (getLocalBounds()); }
}; }

bool DrumKitGrid::isRefSelected (NoteRef ref) const { return isSelected (ref); }

// Ctrl+Left / Ctrl+Right - mirror of PianoRollGrid version.  Walk every
// drum row and re-populate mSelection from the new range so the visual
// highlight stays in sync.
void DrumKitGrid::shiftTimeSelectionLeft()  { /* uses internal helper */
    if (mPM == nullptr) return;
    if (mTimeSelBeatStart < 0.0 || mTimeSelBeatEnd <= mTimeSelBeatStart) return;
    const double t0 = jmin (mTimeSelBeatStart, mTimeSelBeatEnd);
    const double t1 = jmax (mTimeSelBeatStart, mTimeSelBeatEnd);
    const double len = t1 - t0;
    if (len <= 0.0) return;
    double newStart = t0 - len;
    if (newStart < 0.0) newStart = 0.0;
    const double newEnd = newStart + len;
    mTimeSelBeatStart = newStart; mTimeSelBeatEnd = newEnd; mTimeSelBeatAnchor = newStart;
    mSelection.clear();
    for (int row = 0; row < (int) mRowsCache.size(); ++row)
    {
        const int pi = rowToPageIndex (row);
        if (pi < 0) continue;
        const auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        for (int i = 0; i < (int) notes.size(); ++i)
            if (notes[i].startBeat >= newStart && notes[i].startBeat < newEnd)
                mSelection.push_back ({ row, i });
    }
    repaint();
}

void DrumKitGrid::shiftTimeSelectionRight()
{
    if (mPM == nullptr) return;
    if (mTimeSelBeatStart < 0.0 || mTimeSelBeatEnd <= mTimeSelBeatStart) return;
    const double t0 = jmin (mTimeSelBeatStart, mTimeSelBeatEnd);
    const double t1 = jmax (mTimeSelBeatStart, mTimeSelBeatEnd);
    const double len = t1 - t0;
    if (len <= 0.0) return;
    const double newStart = t0 + len;
    const double newEnd   = newStart + len;
    mTimeSelBeatStart = newStart; mTimeSelBeatEnd = newEnd; mTimeSelBeatAnchor = newStart;
    mSelection.clear();
    for (int row = 0; row < (int) mRowsCache.size(); ++row)
    {
        const int pi = rowToPageIndex (row);
        if (pi < 0) continue;
        const auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        for (int i = 0; i < (int) notes.size(); ++i)
            if (notes[i].startBeat >= newStart && notes[i].startBeat < newEnd)
                mSelection.push_back ({ row, i });
    }
    repaint();
}

void DrumKitGrid::scaleSelectionLevels()
{
    if (mPM == nullptr || mSelection.empty()) return;

    auto* host = new DrumScaleLevelsHost();
    auto* aw = new juce::AlertWindow ("Scale Levels",
        "Scale velocities for the selection (100 % = no change).",
        juce::AlertWindow::NoIcon);
    aw->addCustomComponent (host);
    aw->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, host](int result)
        {
            if (result == 1 && mPM != nullptr)
            {
                const float pct = (float) host->slider.getValue();
                if (pct >= 0.0f && std::abs (pct - 100.0f) > 0.01f)
                {
                    const float scale = pct / 100.0f;
                    beginEdit ("Scale Levels");
                    for (auto ref : mSelection)
                    {
                        const int pi = rowToPageIndex (ref.row);
                        if (pi < 0) continue;
                        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
                        if (ref.idx < 0 || ref.idx >= (int) notes.size()) continue;
                        notes[ref.idx].velocity = juce::jlimit (0.0f, 1.0f,
                                                                notes[ref.idx].velocity * scale);
                    }
                    commitEdit();
                    if (onNotesChanged) onNotesChanged();
                    repaint();
                }
            }
            delete host;
        }), true);
}

void DrumKitGrid::deleteSelected()
{
    if (mPM == nullptr) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Delete");
    std::map<int, std::vector<int>> byPage;
    for (auto ref : targets)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi >= 0) byPage[pi].push_back(ref.idx);
    }
    for (auto& [pi, idxs] : byPage)
    {
        std::sort(idxs.begin(), idxs.end(), std::greater<int>());
        idxs.erase(std::unique(idxs.begin(), idxs.end()), idxs.end());
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        for (int i : idxs)
            if (i >= 0 && i < (int) notes.size()) notes.erase(notes.begin() + i);
    }
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void DrumKitGrid::toolChop(int divisions)
{
    if (mPM == nullptr || divisions < 2) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;

    // 2026-04-26 (D-7): mirror PianoRollGrid's chop guard - refuse to
    // subdivide drum hits below 1/16 (0.25 beat).  Skip per-note; if every
    // target falls under the threshold the call is a no-op.
    constexpr double kMinSubDur = 0.25;
    std::vector<NoteRef> chopTargets;
    chopTargets.reserve (targets.size());
    for (auto ref : targets)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        const auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        if (ref.idx < 0 || ref.idx >= (int) notes.size()) continue;
        const double subDur = notes[ref.idx].durationBeats / divisions;
        if (subDur >= kMinSubDur - 1.0e-9)
            chopTargets.push_back (ref);
    }
    if (chopTargets.empty()) return;

    beginEdit("Chop");
    std::map<int, std::vector<int>> byPage;
    std::map<int, std::vector<PianoNote>> toAdd;
    for (auto ref : chopTargets)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi >= 0) byPage[pi].push_back(ref.idx);
    }
    for (auto& [pi, idxs] : byPage)
    {
        std::sort(idxs.begin(), idxs.end(), std::greater<int>());
        idxs.erase(std::unique(idxs.begin(), idxs.end()), idxs.end());
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        for (int i : idxs)
        {
            if (i < 0 || i >= (int) notes.size()) continue;
            PianoNote orig = notes[i];
            notes.erase(notes.begin() + i);
            const double subDur = orig.durationBeats / divisions;
            for (int k = 0; k < divisions; ++k)
            {
                PianoNote sub = orig;
                sub.startBeat     = orig.startBeat + k * subDur;
                sub.durationBeats = subDur;
                toAdd[pi].push_back(sub);
            }
        }
    }
    for (auto& [pi, list] : toAdd)
    {
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        for (auto& n : list) notes.push_back(n);
        sortNotes(notes);
    }
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void DrumKitGrid::toolGlue()
{
    if (mPM == nullptr) return;
    auto targets = getWorkingSet();
    if (targets.size() < 2) return;
    beginEdit("Glue");
    // Glue per-row only (drum glue across rows wouldn't make sense).
    std::map<int, std::vector<int>> byRow;
    for (auto ref : targets) byRow[ref.row].push_back(ref.idx);
    for (auto& [row, idxs] : byRow)
    {
        if (idxs.size() < 2) continue;
        const int pi = rowToPageIndex(row);
        if (pi < 0) continue;
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        std::sort(idxs.begin(), idxs.end(), [&notes](int a, int b) {
            return notes[a].startBeat < notes[b].startBeat;
        });
        std::set<int> toErase;
        for (int i = 0; i < (int) idxs.size() - 1; ++i)
        {
            int ia = idxs[i], ib = idxs[i + 1];
            if (toErase.count(ia)) continue;
            auto& na = notes[ia]; const auto& nb = notes[ib];
            const double newEnd = std::max(na.startBeat + na.durationBeats,
                                           nb.startBeat + nb.durationBeats);
            na.durationBeats = newEnd - na.startBeat;
            toErase.insert(ib);
        }
        std::vector<int> eraseVec(toErase.rbegin(), toErase.rend());
        for (int i : eraseVec) notes.erase(notes.begin() + i);
        sortNotes(notes);
    }
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void DrumKitGrid::toolStrum()
{
    if (mPM == nullptr) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Strum");
    constexpr double kStrumStep = 1.0 / 32.0;
    // Per-beat, group across rows; offset notes by row order.
    std::map<double, std::vector<NoteRef>> byBeat;
    for (auto ref : targets)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        const auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        if (ref.idx < 0 || ref.idx >= (int) notes.size()) continue;
        byBeat[notes[ref.idx].startBeat].push_back(ref);
    }
    for (auto& [beat, refs] : byBeat)
    {
        if ((int) refs.size() < 2) continue;
        std::sort(refs.begin(), refs.end(), [](NoteRef a, NoteRef b){ return a.row < b.row; });
        double offset = 0.0;
        for (auto ref : refs)
        {
            const int pi = rowToPageIndex(ref.row);
            if (pi < 0) continue;
            mPM->currentPattern().drumRolls[pi].notes[ref.idx].startBeat = beat + offset;
            offset += kStrumStep;
        }
    }
    for (int p = 0; p < (int) kMaxDrumPages; ++p)
        sortNotes(mPM->currentPattern().drumRolls[p].notes);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void DrumKitGrid::toolRandomize()
{
    if (mPM == nullptr) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Randomize");
    Random rng;
    const double snap = snapUnitBeats();
    for (auto ref : targets)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        if (ref.idx < 0 || ref.idx >= (int) notes.size()) continue;
        auto& n = notes[ref.idx];
        n.velocity  = jlimit(0.f, 1.f, n.velocity + (rng.nextFloat() - 0.5f) * 0.4f);
        n.startBeat = jmax(0.0, n.startBeat + (rng.nextDouble() - 0.5) * snap);
    }
    for (int p = 0; p < (int) kMaxDrumPages; ++p)
        sortNotes(mPM->currentPattern().drumRolls[p].notes);
    commitEdit();
    clearSelection();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

void DrumKitGrid::toolArticulate()
{
    if (mPM == nullptr) return;
    auto targets = getWorkingSet();
    if (targets.empty()) return;
    beginEdit("Articulate");
    for (auto ref : targets)
    {
        const int pi = rowToPageIndex(ref.row);
        if (pi < 0) continue;
        auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        if (ref.idx < 0 || ref.idx >= (int) notes.size()) continue;
        notes[ref.idx].durationBeats = jmax(1.0 / 64.0, notes[ref.idx].durationBeats * 0.8);
    }
    commitEdit();
    repaint();
    if (onNotesChanged) onNotesChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitControlLane
// ─────────────────────────────────────────────────────────────────────────────
DrumKitControlLane::DrumKitControlLane() {}

void DrumKitControlLane::setScrollState(float ppb, double beatOff) { mPPB = ppb; mBeatOff = beatOff; repaint(); }
void DrumKitControlLane::setPatternManager(PatternManager* pm) { mPM = pm; repaint(); }
void DrumKitControlLane::setKitRowProvider(std::function<std::vector<DrumKitRowInfo>()> fn) { mRowProvider = std::move(fn); repaint(); }
void DrumKitControlLane::setMode(Mode m) { mMode = m; repaint(); }

float DrumKitControlLane::getVal(const PianoNote& n) const
{
    return (mMode == Velocity) ? n.velocity : (n.panning + 1.f) * 0.5f;
}

void DrumKitControlLane::setVal(PianoNote& n, float v)
{
    v = jlimit(0.f, 1.f, v);
    if (mMode == Velocity) n.velocity = v;
    else                    n.panning  = v * 2.f - 1.f;
}

DrumKitGrid::NoteRef DrumKitControlLane::noteNearX(int x, int y) const
{
    if (mPM == nullptr || mPPB <= 0 || ! mRowProvider) return {};
    const auto rows = mRowProvider();
    const double beat = mBeatOff + (double) x / mPPB;
    static constexpr double kPxTol = 10.0;
    const double kBeatTol = kPxTol / mPPB;

    // 2026-04-26 (D-7 sub-4): selection-locked editing - restrict candidates
    // to selected refs when the grid has any selection.
    const bool selectionLocked = (hasAnySelection && hasAnySelection());

    DrumKitGrid::NoteRef best { -1, -1 };
    double bestDist = kBeatTol;
    for (int row = 0; row < (int) rows.size(); ++row)
    {
        const int pi = rows[row].pageIndex;
        if (pi < 0) continue;
        const auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        for (int i = 0; i < (int) notes.size(); ++i)
        {
            const DrumKitGrid::NoteRef ref { row, i };
            if (selectionLocked && isRefSelected && ! isRefSelected (ref))
                continue;
            const double d = std::abs(notes[i].startBeat - beat);
            if (d < bestDist) { bestDist = d; best = ref; }
        }
    }
    juce::ignoreUnused(y);
    return best;
}

void DrumKitControlLane::paint(Graphics& g)
{
    auto b = getLocalBounds();
    g.setColour(VC::Panel.darker(0.15f)); g.fillRect(b);
    g.setColour(VC::Accent.withAlpha(0.5f)); g.drawRect(b, 1);

    g.setColour(VC::Panel.brighter(0.1f));
    g.fillRect(0, 0, b.getWidth(), kHeaderH);
    g.setColour(VC::Accent.withAlpha(0.7f));
    g.drawHorizontalLine(kHeaderH - 1, 0.f, (float) b.getWidth());

    static const char* kModeNames[] = { "Control > Velocity", "Control > Panning" };
    const int modeIdx = (mMode == Velocity) ? 0 : 1;
    g.setColour(VC::Text); g.setFont(Font(10, Font::bold));
    g.drawText(juce::String(kModeNames[modeIdx]) + "  \xe2\x96\xbe",
               6, 1, b.getWidth() - 12, kHeaderH - 2, Justification::centredLeft);

    if (mPM == nullptr || mPPB <= 0 || ! mRowProvider) return;
    const auto rows = mRowProvider();

    // Beat grid lines.
    {
        const int contentTop = kHeaderH;
        for (double beat = std::floor(mBeatOff); beat <= mBeatOff + b.getWidth() / mPPB + 1.0; beat += 0.5)
        {
            const int gx = (int) ((beat - mBeatOff) * mPPB);
            if (gx < 0 || gx > b.getWidth()) continue;
            const bool isBar  = (std::fmod(beat, 4.0) < 1e-9);
            const bool isBeat = (std::fmod(beat, 1.0) < 1e-9);
            if (isBar)
                g.setColour(VC::Accent.withAlpha(0.35f));
            else if (isBeat)
                g.setColour(VC::Accent.withAlpha(0.18f));
            else
                g.setColour(VC::Accent.withAlpha(0.08f));
            g.drawVerticalLine(gx, (float) contentTop, (float) b.getHeight());
        }
    }

    const bool bipolar = (mMode == Panning);
    const int contentH = b.getHeight() - kHeaderH;
    const int contentY = kHeaderH;
    if (bipolar)
    {
        const int cy = contentY + contentH / 2;
        g.setColour(VC::Accent.withAlpha(0.45f));
        g.drawHorizontalLine(cy, 4.f, (float) b.getWidth() - 4);
    }

    Colour nodeColor = (mMode == Velocity) ? VC::Green : VC::Blue;

    for (int row = 0; row < (int) rows.size(); ++row)
    {
        const int pi = rows[row].pageIndex;
        if (pi < 0) continue;
        const auto& notes = mPM->currentPattern().drumRolls[pi].notes;
        for (int i = 0; i < (int) notes.size(); ++i)
        {
            const auto& n = notes[i];
            const int x     = (int) ((n.startBeat - mBeatOff) * mPPB);
            const int tailW = jmax(2, (int) (jmax(0.0625, n.durationBeats) * mPPB) - 1);
            if (x + tailW < 0 || x > b.getWidth()) continue;

            const float val = getVal(n);
            // 2026-04-26 (D-7 sub-4): selected drum hits paint RED.
            const DrumKitGrid::NoteRef ref { row, i };
            const bool selected = (isRefSelected && isRefSelected (ref));
            Colour col = selected
                ? Colour (0xffff3344).withMultipliedLightness (0.65f + val * 0.35f)
                : nodeColor.withMultipliedLightness(0.55f + val * 0.45f);
            if (n.muted) col = col.withSaturation(0.1f).withAlpha(0.4f);

            int nodeY, stemTop, stemBot;
            if (bipolar)
            {
                const int cy = contentY + contentH / 2;
                const int offset = (int) ((val - 0.5f) * (contentH - 8));
                nodeY  = cy - offset;
                stemTop = jmin(nodeY, cy);
                stemBot = jmax(nodeY, cy);
            }
            else
            {
                const int stemH = jmax(2, (int) (val * (contentH - 6)));
                stemTop  = b.getHeight() - 4 - stemH;
                stemBot  = b.getHeight() - 4;
                nodeY    = stemTop;
            }

            g.setColour(col);
            g.drawLine((float) x, (float) stemTop, (float) x, (float) stemBot, 1.f);
            g.fillEllipse((float) (x - 2), (float) (nodeY - 2), 4.f, 4.f);
            g.drawLine((float) x, (float) nodeY, (float) (x + tailW), (float) nodeY, 1.f);
        }
    }
}

void DrumKitControlLane::mouseDown(const MouseEvent& e)
{
    if (e.y < kHeaderH)
    {
        PopupMenu m;
        m.addItem(1, "Velocity");
        m.addItem(2, "Panning");
        m.showMenuAsync(PopupMenu::Options().withTargetComponent(this),
            [this](int r) {
                if (r < 1 || r > 2) return;
                mMode = static_cast<Mode>(r - 1);
                if (onModeChange) onModeChange(mMode);
                repaint();
            });
        return;
    }
    if (mPM == nullptr) return;
    auto yToVal = [&](int py) {
        const int contentH = jmax(1, getHeight() - kHeaderH);
        const int relY     = py - kHeaderH;
        return jlimit(0.f, 1.f, 1.f - (float) relY / (float) contentH);
    };
    mDragRef = noteNearX(e.x, e.y);
    if (mDragRef.isValid() && mRowProvider)
    {
        if (onBeginEdit) onBeginEdit ("Adjust Lane Value");
        const auto rows = mRowProvider();
        if (mDragRef.row < (int) rows.size())
        {
            const int pi = rows[mDragRef.row].pageIndex;
            if (pi >= 0)
            {
                auto& notes = mPM->currentPattern().drumRolls[pi].notes;
                if (mDragRef.idx < (int) notes.size())
                {
                    setVal(notes[mDragRef.idx], yToVal(e.y));
                    if (onChanged) onChanged();
                    repaint();
                }
            }
        }
    }
}

void DrumKitControlLane::mouseDrag(const MouseEvent& e)
{
    if (mPM == nullptr || ! mDragRef.isValid() || e.y < kHeaderH || ! mRowProvider) return;
    const int contentH = jmax(1, getHeight() - kHeaderH);
    const int relY     = e.y - kHeaderH;
    const float val    = jlimit(0.f, 1.f, 1.f - (float) relY / (float) contentH);
    const auto rows = mRowProvider();
    if (mDragRef.row < (int) rows.size())
    {
        const int pi = rows[mDragRef.row].pageIndex;
        if (pi >= 0)
        {
            auto& notes = mPM->currentPattern().drumRolls[pi].notes;
            if (mDragRef.idx < (int) notes.size())
            {
                setVal(notes[mDragRef.idx], val);
                if (onChanged) onChanged();
                repaint();
            }
        }
    }
}

void DrumKitControlLane::mouseUp(const MouseEvent&)
{
    mDragRef = { -1, -1 };
    if (onCommitEdit) onCommitEdit();
}

// 2026-04-26 (D-7 sub-4): Alt+Wheel over the drum-kit lane mirrors the
// Piano-Roll behaviour - adjust the currently-displayed property
// (velocity / pan) for the hit whose bar is under the cursor x.  Default
// delta ±0.05; Shift+Alt+Wheel = ±0.01 for fine adjustment.
void DrumKitControlLane::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (mPM == nullptr || ! mRowProvider) { Component::mouseWheelMove (e, wheel); return; }
    if (! e.mods.isAltDown())              { Component::mouseWheelMove (e, wheel); return; }
    if (e.y < kHeaderH)                    { return; }

    const auto ref = noteNearX (e.x, e.y);
    if (! ref.isValid()) return;
    const auto rows = mRowProvider();
    if (ref.row >= (int) rows.size()) return;
    const int pi = rows[ref.row].pageIndex;
    if (pi < 0) return;
    auto& notes = mPM->currentPattern().drumRolls[pi].notes;
    if (ref.idx < 0 || ref.idx >= (int) notes.size()) return;

    const float delta = (e.mods.isShiftDown() ? 0.01f : 0.05f)
                      * (wheel.deltaY >= 0.f ? +1.f : -1.f);

    if (onBeginEdit) onBeginEdit ("Adjust Lane Value");
    const float newVal = juce::jlimit (0.0f, 1.0f, getVal (notes[ref.idx]) + delta);
    setVal (notes[ref.idx], newVal);
    if (onCommitEdit) onCommitEdit();
    if (onChanged)    onChanged();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitContainer
// ─────────────────────────────────────────────────────────────────────────────
bool DrumKitContainer::keyPressed(const juce::KeyPress& key)
{
    if (mGrid) return mGrid->keyPressed(key);
    return false;
}

void DrumKitContainer::focusGained(juce::Component::FocusChangeType)
{
    if (mGrid) mGrid->grabKeyboardFocus();
}

DrumKitContainer::DrumKitContainer()
{
    setWantsKeyboardFocus(true);

    mSidebar = std::make_unique<DrumKitSidebar>(); addAndMakeVisible(*mSidebar);
    mGrid    = std::make_unique<DrumKitGrid>();    addAndMakeVisible(*mGrid);
    mLane    = std::make_unique<DrumKitControlLane>(); addAndMakeVisible(*mLane);

    // Forward sidebar wheel events to the grid.
    mSidebar->onWheel = [this](const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
        if (mGrid) mGrid->mouseWheelMove(e, wheel);
    };

    // 2026-04-26: forward sidebar's Global Lock/Unlock button click up to
    // StandaloneEditor (via DrumPage → setGlobalLockHandler).
    mSidebar->onGlobalLockRequested = [this] {
        if (onGlobalLockRequested) onGlobalLockRequested();
    };

    // Toolbar.
    mWrenchBtn = std::make_unique<TextButton>("Tools");
    mWrenchBtn->setTooltip("Tools - Quantize, Strum, Glue, Chop, Randomize, Articulate");
    mWrenchBtn->onClick = [this] {
        if (mGrid) mGrid->showToolsMenu();
        if (mGrid) mGrid->grabKeyboardFocus();
    };
    addAndMakeVisible(*mWrenchBtn);

    mMagnetBtn = std::make_unique<DrumKitRightClickButton>();
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

    static const char* toolLabels[] = { "Draw","Paint","Del","Mute","Slice","Sel","Zoom" };
    static const char* toolTips[] = {
        "Draw (P) - LMB draw | click note to move | near right edge to resize | Ctrl+click select",
        "Paint (B) - drag to paint notes continuously",
        "Delete (D) - click/drag to erase notes",
        "Mute (T) - toggle note mute",
        "Slice (C) - drag to draw cut line",
        "Select (E) - marquee select | drag notes to move",
        "Zoom (Shift+Z) - click to zoom in | drag region | RMB to zoom out"
    };
    for (int i = 0; i < 7; ++i)
    {
        mToolBtns[i] = std::make_unique<TextButton>(toolLabels[i]);
        mToolBtns[i]->setTooltip(toolTips[i]);
        const auto tool = static_cast<DrumKitGrid::PRTool>(i);
        mToolBtns[i]->onClick = [this, tool] { setActiveTool(tool); };
        addAndMakeVisible(*mToolBtns[i]);
    }
    mToolBtns[0]->setToggleState(true, dontSendNotification);

    mUndoBtn = std::make_unique<TextButton>("Undo");
    mUndoBtn->setTooltip("Undo (Ctrl+Z)");
    mUndoBtn->onClick = [this] { undoRoll(); if (mGrid) mGrid->grabKeyboardFocus(); };
    mUndoBtn->setEnabled(false);
    addAndMakeVisible(*mUndoBtn);

    mRedoBtn = std::make_unique<TextButton>("Redo");
    mRedoBtn->setTooltip("Redo (Ctrl+Alt+Z)");
    mRedoBtn->onClick = [this] { redoRoll(); if (mGrid) mGrid->grabKeyboardFocus(); };
    mRedoBtn->setEnabled(false);
    addAndMakeVisible(*mRedoBtn);

    mHistoryBtn = std::make_unique<TextButton>("H");
    mHistoryBtn->setTooltip("Show undo history");
    mHistoryBtn->onClick = [this] {
        if (onShowHistoryWindow) onShowHistoryWindow();
        if (mGrid) mGrid->grabKeyboardFocus();
    };
    addAndMakeVisible(*mHistoryBtn);

    mZoomInBtn = std::make_unique<TextButton>("+");
    mZoomInBtn->setTooltip("Zoom in (Ctrl+scroll)");
    mZoomInBtn->onClick = [this] { applyZoom(1.3f); if (mGrid) mGrid->grabKeyboardFocus(); };
    addAndMakeVisible(*mZoomInBtn);

    mZoomOutBtn = std::make_unique<TextButton>("-");
    mZoomOutBtn->setTooltip("Zoom out (Ctrl+scroll)");
    mZoomOutBtn->onClick = [this] { applyZoom(1.f / 1.3f); if (mGrid) mGrid->grabKeyboardFocus(); };
    addAndMakeVisible(*mZoomOutBtn);

    // Batch 5: Kit button.  Click defers to StandaloneEditor (wired via
    // onKitMenuRequested) which builds + shows the Save / Load Kit menu.
    mKitBtn = std::make_unique<TextButton>("Kit  v");
    mKitBtn->setTooltip("Save / Load Kit (16-drum bundle)");
    mKitBtn->onClick = [this] {
        if (onKitMenuRequested) onKitMenuRequested(mKitBtn.get());
        if (mGrid) mGrid->grabKeyboardFocus();
    };
    addAndMakeVisible(*mKitBtn);

    // Menu bar.
    mMenuBarModel = std::make_unique<DrumKitMenuBar>(*this);
    mMenuBar      = std::make_unique<juce::MenuBarComponent>(mMenuBarModel.get());
    addAndMakeVisible(*mMenuBar);

    // Wire grid callbacks.
    mGrid->onZoom    = [this](float delta) { applyZoom((mPPB + delta) / mPPB); };
    mGrid->onZoomAnchored = [this](float f, int x) { applyZoomAnchored(f, x); };
    mGrid->onHScroll = [this](double dB)   { mBeatOff = jmax(0.0, mBeatOff + dB); syncScrollState(); };
    mGrid->onNotesChanged = [this] { if (mLane) mLane->repaint(); };
    mGrid->onToolChanged = [this](DrumKitGrid::PRTool t) {
        mActiveTool = t;
        const int idx = static_cast<int>(t);
        for (int i = 0; i < 7; ++i)
            if (mToolBtns[i]) mToolBtns[i]->setToggleState(i == idx, dontSendNotification);
    };
    mGrid->onUndoRedoStateChanged = [this] { updateUndoRedoBtns(); };
    mGrid->onSeek = [this](double beat) { if (onSeek) onSeek(beat); };

    mGrid->onZoomTo = [this](double beatStart, double beatEnd) {
        mPreZoomPPB     = mPPB;
        mPreZoomBeatOff = mBeatOff;
        mZoomedIn       = true;
        const double range = jmax(0.01, beatEnd - beatStart);
        const float vpW    = (float) jmax(1, mGrid->getWidth());
        const float minPPB = minZoomPPB (vpW);
        const float maxPPB = maxZoomPPB (vpW);
        mPPB     = jlimit(minPPB, maxPPB, vpW / (float) range);
        mBeatOff = beatStart;
        syncScrollState();
    };
    mGrid->onZoomToggle = [this] {
        if (mZoomedIn)
        {
            mPPB     = mPreZoomPPB;
            mBeatOff = mPreZoomBeatOff;
            mZoomedIn = false;
        }
        else
        {
            applyZoom(1.f / 1.3f);
        }
        syncScrollState();
    };

    mLane->onModeChange = [this](DrumKitControlLane::Mode) {};

    // 2026-04-26 (D-7 sub-4): wire selection-aware lane callbacks.
    mLane->isRefSelected = [this] (DrumKitGrid::NoteRef ref) {
        return mGrid ? mGrid->isRefSelected (ref) : false;
    };
    mLane->hasAnySelection = [this] {
        return mGrid ? mGrid->hasSelection() : false;
    };
    mLane->onBeginEdit  = [this] (const juce::String& label) {
        if (mGrid) mGrid->beginEdit (label);
    };
    mLane->onCommitEdit = [this] {
        if (mGrid) mGrid->commitEdit();
    };

    // Context label.
    mContextLabel = std::make_unique<juce::Label>();
    mContextLabel->setColour(juce::Label::textColourId, VC::TextDim);
    mContextLabel->setFont(juce::Font(11.f, juce::Font::bold));
    mContextLabel->setJustificationType(juce::Justification::centredRight);
    mContextLabel->setInterceptsMouseClicks(false, false);
    addAndMakeVisible(*mContextLabel);

    // Horizontal scrollbar (no V scrollbar - 16 rows are fixed).
    mHScroll = std::make_unique<juce::ScrollBar>(false);
    mHScroll->setAutoHide(false);
    mHScroll->addListener(this);
    addAndMakeVisible(*mHScroll);

    syncScrollState();
}

void DrumKitContainer::setPatternManager(PatternManager* pm)
{
    mPM = pm;
    if (mGrid) mGrid->setPatternManager(pm);
    if (mLane) mLane->setPatternManager(pm);
    syncScrollState();
}

void DrumKitContainer::setApvts(juce::AudioProcessorValueTreeState* a)
{
    if (mSidebar) mSidebar->setApvts(a);
}

void DrumKitContainer::setKitRowProvider(std::function<std::vector<DrumKitRowInfo>()> fn)
{
    if (mSidebar) mSidebar->setKitRowProvider(fn);
    if (mGrid)    mGrid->setKitRowProvider(fn);
    if (mLane)    mLane->setKitRowProvider(fn);
}

void DrumKitContainer::setRowClickHandler(std::function<void(int, juce::Component*)> fn)
{
    if (mSidebar) mSidebar->setRowClickHandler(std::move(fn));
}

void DrumKitContainer::setAuditionHandlers(std::function<void(int)> onOn,
                                           std::function<void(int)> onOff)
{
    if (mSidebar) mSidebar->setAuditionHandlers(onOn, onOff);
    if (mGrid) {
        mGrid->onRowAuditionOn  = onOn;
        mGrid->onRowAuditionOff = onOff;
    }
}

void DrumKitContainer::setReorderHandler(std::function<void(int, int)> fn)
{
    if (mSidebar) mSidebar->setReorderHandler(std::move(fn));
}

void DrumKitContainer::refreshKitView()
{
    if (mSidebar) mSidebar->refresh();
    if (mGrid)    { mGrid->refreshRowsCache(); mGrid->repaint(); }
    syncScrollState();
}

void DrumKitContainer::setActiveTool(DrumKitGrid::PRTool t)
{
    mActiveTool = t;
    const int idx = static_cast<int>(t);
    for (int i = 0; i < 7; ++i)
        if (mToolBtns[i]) mToolBtns[i]->setToggleState(i == idx, dontSendNotification);
    if (mGrid) mGrid->setTool(t);
    if (mGrid) mGrid->grabKeyboardFocus();
}

void DrumKitContainer::updateUndoRedoBtns()
{
    const bool canUndo = mUndoCtx.isValid() && mUndoCtx.manager->canUndo();
    const bool canRedo = mUndoCtx.isValid() && mUndoCtx.manager->canRedo();
    if (mUndoBtn) mUndoBtn->setEnabled(canUndo);
    if (mRedoBtn) mRedoBtn->setEnabled(canRedo);
}

void DrumKitContainer::undoRoll() { if (mUndoCtx.undo) mUndoCtx.undo(); }
void DrumKitContainer::redoRoll() { if (mUndoCtx.redo) mUndoCtx.redo(); }

void DrumKitContainer::setUndoContext(const UndoContext& ctx)
{
    mUndoCtx = ctx;
    if (mGrid) mGrid->setUndoContext(ctx);
    if (ctx.showHistory) onShowHistoryWindow = ctx.showHistory;
    updateUndoRedoBtns();
}

void DrumKitContainer::setPlayheadBeat(double beat)
{
    mPlayheadBeat = beat;
    if (mGrid) mGrid->setPlayheadBeat(beat);
}

// QA-Ee Stage 2 (content-bound dynamic zoom): furthest-right note edge across
// ALL drum rows in the current pattern, in bars (4 beats/bar).
float DrumKitContainer::contentMaxBars() const
{
    double maxBeats = 0.0;
    if (mPM)
    {
        const auto& pat = mPM->currentPattern();
        for (int pi = 0; pi < kMaxDrumPages; ++pi)
            for (const auto& n : pat.drumRolls[pi].notes)
                maxBeats = jmax(maxBeats, n.startBeat + n.durationBeats);
    }
    return (float) jmax(0.0, maxBeats / 4.0);
}

// Zoom-OUT minimum (px/beat).  Empty baseline = vpW / kDefaultPianoRollEmptyPx
// (monitor-dependent); grows with notes + a 1-bar pad.  4 beats/bar.
float DrumKitContainer::minZoomPPB (float vpW) const
{
    const float defaultBars = vpW / kDefaultPianoRollEmptyPx;
    const float maxBars     = jmax (defaultBars, contentMaxBars() + kPianoRollZoomPadBars);
    return vpW / (4.f * jmax (1.f, maxBars));
}

// Zoom-IN maximum (px/beat) -- tick-level micro-editing.
float DrumKitContainer::maxZoomPPB (float vpW) const
{
    return vpW / jmax (0.01f, kMaxZoomInBeatsAcross);
}

void DrumKitContainer::applyZoom(float factor)
{
    // QA-Ee Stage 2: content-bound dynamic limits (matches the Piano Roll).
    const float vpW    = mGrid ? (float) jmax(1, mGrid->getWidth()) : 800.f;
    const float minPPB = minZoomPPB (vpW);
    const float maxPPB = maxZoomPPB (vpW);
    mPPB = jlimit(minPPB, maxPPB, mPPB * factor);
    syncScrollState();
}

// QA-Ee: cursor-anchored zoom -- keeps the beat under the mouse fixed (FL Ctrl+scroll feel).
void DrumKitContainer::applyZoomAnchored(float factor, int anchorX)
{
    const float vpW = mGrid ? (float) jmax(1, mGrid->getWidth()) : 800.f;
    const double anchorBeat = mBeatOff + (double) anchorX / jmax(1.f, mPPB);
    mPPB = jlimit(minZoomPPB(vpW), maxZoomPPB(vpW), mPPB * factor);
    mBeatOff = jmax(0.0, anchorBeat - (double) anchorX / jmax(1.f, mPPB));
    syncScrollState();
}

void DrumKitContainer::onHScrollDelta(double dBeats)
{
    mBeatOff = jmax(0.0, mBeatOff + dBeats);
    syncScrollState();
}

void DrumKitContainer::syncScrollState()
{
    if (! mGrid || ! mSidebar || ! mLane) return;

    // Auto-fit row height: rowH = (gridH - rulerH) / 16, clamped to [min, max].
    const int gridH = mGrid->getHeight();
    int rowH = jmax(DrumKitGrid::kMinRowH,
                    (gridH - DrumKitGrid::kRulerH) / DrumKitSidebar::kNumRows);
    rowH = jlimit(DrumKitGrid::kMinRowH, DrumKitGrid::kMaxRowH, rowH);

    mGrid->setScrollState(mPPB, mBeatOff, rowH);
    mGrid->setRowYOffset(DrumKitGrid::kRulerH);
    mLane->setScrollState(mPPB, mBeatOff);
    mSidebar->setRowMetrics(DrumKitGrid::kRulerH, rowH);
    pushScrollStateToBars();
    repaint();
}

void DrumKitContainer::pushScrollStateToBars()
{
    if (! mGrid || ! mHScroll) return;
    mPushingToBars = true;
    const int    gridW         = jmax(1, mGrid->getWidth());
    const double visibleBeats0 = (double) gridW / jmax(1.f, mPPB);
    // C.5b: pattern's intrinsic TS drives bar length.
    double bpb = 4.0;
    double last = 0.0;
    int patternBars = 4;
    if (mPM)
    {
        patternBars = jmax(1, mPM->currentPattern().drumRolls[0].numBars);
        bpb = jmax (1.0, mPM->getPatternBeatsPerBar (mPM->getCurrentPatternIndex()));
        for (int p = 0; p < (int) kMaxDrumPages; ++p)
            for (const auto& n : mPM->currentPattern().drumRolls[p].notes)
            {
                const double end = n.startBeat + jmax(0.0625, n.durationBeats);
                if (end > last) last = end;
            }
    }
    // QA-Ee: include the current scroll offset so a cursor-anchored zoom that lands
    // past the content stays representable -- otherwise the H-scrollbar clamps the
    // thumb to 0 and its (async) scrollBarMoved snaps mBeatOff back to bar 0.
    const double totalBeats   = jmax((double) patternBars * bpb,
                                     jmax(last + bpb, mBeatOff + visibleBeats0));
    const double visibleBeats = jmin(totalBeats, visibleBeats0);
    mHScroll->setRangeLimits(0.0, totalBeats);
    mHScroll->setCurrentRange(jlimit(0.0, jmax(0.0, totalBeats - visibleBeats), mBeatOff),
                              visibleBeats, juce::dontSendNotification);
    mPushingToBars = false;
}

void DrumKitContainer::scrollBarMoved(juce::ScrollBar* sb, double newStart)
{
    if (mPushingToBars || ! sb) return;
    if (sb == mHScroll.get())
    {
        mBeatOff = jmax(0.0, newStart);
        syncScrollState();
    }
}

void DrumKitContainer::setContextLabel(const juce::String& text)
{
    if (mContextLabel) mContextLabel->setText(text, juce::dontSendNotification);
}

void DrumKitContainer::scrollToPlayhead()
{
    if (mPlayheadBeat < 0.0 || ! mGrid) return;
    mBeatOff = jmax(0.0, mPlayheadBeat - 1.0);
    syncScrollState();
}

void DrumKitContainer::setLaneVisible(bool v)
{
    if (mLaneVisible == v) return;
    mLaneVisible = v;
    resized();
    repaint();
}

void DrumKitContainer::setSnapDenomAndQuantize(int denom)
{
    // QA-Ee Stage 3: legacy quantize-submenu denom (4/8/16/32) -> unified div,
    // written to the GLOBAL snap; then quantize the selection to it.
    const int div = (denom <= 4) ? 3 : (denom <= 8) ? 4 : (denom <= 16) ? 6 : 7;
    if (mOnSetSnapDiv) mOnSetSnapDiv(div);
    if (mGrid) mGrid->toolQuantize();
}

// QA-Ee Stage 3: PianoRollPage wires these so the grid reads the global snap
// param live (snapBeat) + the magnet menu writes it.
void DrumKitContainer::setSnapAccessors (std::function<int()> getter,
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

void DrumKitContainer::timerCallback()
{
    // QA-UICleanup Task 3: live-sync the Snap highlight to the shared global div
    // (only the visible grid needs it; setToggleState repaints only on a change).
    if (isShowing() && mMagnetBtn && mOnGetSnapDiv)
        mMagnetBtn->setToggleState (mOnGetSnapDiv() != 0, juce::dontSendNotification);
}

void DrumKitContainer::paint(Graphics& g)
{
    g.fillAll(VC::Bg);
    g.setColour(VC::Panel);
    g.fillRect(0, 0, getWidth(), kMenuBarH + kToolbarH);
    g.setColour(VC::Accent);
    g.drawHorizontalLine(kMenuBarH + kToolbarH - 1, 0.f, (float) getWidth());
}

DrumKitContainer::~DrumKitContainer()
{
    // QA-D Task 4 (QA-0a finding #8): defensive teardown of the MenuBarComponent
    // before its model is destroyed.  See PianoRollContainer::~PianoRollContainer
    // for the rationale -- same shape, same justification.
    if (mMenuBar)
    {
        mMenuBar->setModel (nullptr);
        mMenuBar.reset();
    }
}

void DrumKitContainer::resized()
{
    auto b = getLocalBounds();

    if (mMenuBar) mMenuBar->setBounds(b.removeFromTop(kMenuBarH));

    auto row1 = b.removeFromTop(kToolbarH);
    row1.removeFromLeft(4);
    mWrenchBtn->setBounds(row1.removeFromLeft(38).reduced(2, 3));
    row1.removeFromLeft(2);
    mMagnetBtn->setBounds(row1.removeFromLeft(38).reduced(2, 3));
    row1.removeFromLeft(4);
    for (int i = 0; i < 7; ++i)
        mToolBtns[i]->setBounds(row1.removeFromLeft(62).reduced(2, 3));   // 2026-04-26: 36→62 to match Builder
    row1.removeFromLeft(4);
    mUndoBtn   ->setBounds(row1.removeFromLeft(48).reduced(2, 3));        // 40→48
    mRedoBtn   ->setBounds(row1.removeFromLeft(48).reduced(2, 3));        // 40→48
    row1.removeFromLeft(2);
    mHistoryBtn->setBounds(row1.removeFromLeft(22).reduced(2, 3));
    row1.removeFromLeft(6);
    // 2026-04-26: 22→28 so the +/- glyphs fit.
    mZoomOutBtn->setBounds(row1.removeFromLeft(28).reduced(2, 3));
    mZoomInBtn ->setBounds(row1.removeFromLeft(28).reduced(2, 3));
    // QA-UICleanup Task 3 (SC9): Kit button pinned to the far-right end (like the
    // other players' preset buttons); context label fills the space to its left.
    if (mKitBtn) mKitBtn->setBounds(row1.removeFromRight(46).reduced(2, 3));
    if (mContextLabel)
    {
        row1.removeFromLeft(8);
        mContextLabel->setBounds(row1.reduced(4, 4));
    }

    static constexpr int kMinGridH = 120;
    const int totalH      = b.getHeight();
    const int actualLaneH = mLaneVisible ? kLaneH : 0;
    const int gridH       = jmax(kMinGridH, totalH - actualLaneH - kScrollBarSz);

    const int bx = b.getX();
    const int by = b.getY();
    const int bw = b.getWidth();

    mGrid->setBounds(bx + DrumKitSidebar::kWidth, by,
                     bw - DrumKitSidebar::kWidth, gridH);

    if (mHScroll)
        mHScroll->setBounds(bx + DrumKitSidebar::kWidth, by + gridH,
                            bw - DrumKitSidebar::kWidth, kScrollBarSz);

    // Sidebar takes the full grid height (top-aligned with grid; ruler offset
    // matched in paint via mRulerH).
    mSidebar->setBounds(bx, by, DrumKitSidebar::kWidth, gridH);

    if (mLane)
    {
        mLane->setVisible(mLaneVisible);
        if (mLaneVisible)
            mLane->setBounds(bx + DrumKitSidebar::kWidth, by + gridH + kScrollBarSz,
                             bw - DrumKitSidebar::kWidth, actualLaneH);
    }

    syncScrollState();
}

// ─────────────────────────────────────────────────────────────────────────────
// DrumKitMenuBar
// ─────────────────────────────────────────────────────────────────────────────
juce::StringArray DrumKitMenuBar::getMenuBarNames()
{
    return { "Edit", "Tools", "View" };
}

juce::PopupMenu DrumKitMenuBar::getMenuForIndex(int idx, const juce::String&)
{
    juce::PopupMenu menu;

    if (idx == 0)
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
    }
    else if (idx == 1)
    {
        using T = DrumKitGrid::PRTool;
        auto addTool = [&](int id, const juce::String& label, T tool) {
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
    else if (idx == 2)
    {
        menu.addItem(51, "Zoom In");
        menu.addItem(52, "Zoom Out");
        menu.addSeparator();
        menu.addItem(55, "Scroll to Playhead");
        menu.addSeparator();
        menu.addItem(57, "Velocity Lane", true, mOwner.isLaneVisible());
    }
    return menu;
}

void DrumKitMenuBar::menuItemSelected(int id, int)
{
    auto& o = mOwner;
    if      (id == 1) { if (auto* g = o.mGrid.get()) g->selectAll(); }
    else if (id == 2) { if (auto* g = o.mGrid.get()) g->clearSelection(); }
    else if (id == 3) { if (auto* g = o.mGrid.get()) g->copySelected(); }
    else if (id == 4) { if (auto* g = o.mGrid.get()) g->pasteClipboard(); }
    else if (id == 5) { if (auto* g = o.mGrid.get()) g->deleteSelected(); }
    else if (id == 6) { if (auto* g = o.mGrid.get()) g->duplicateSelected(); }
    else if (id >= 101 && id <= 104)
    {
        constexpr int kDenoms[] = { 4, 8, 16, 32 };
        o.setSnapDenomAndQuantize(kDenoms[id - 101]);
    }
    else if (id >= 21 && id <= 27)
    {
        using T = DrumKitGrid::PRTool;
        constexpr T kTools[] = {
            T::Draw, T::Paint, T::Delete, T::Mute, T::Slice, T::Select, T::Zoom };
        o.setActiveTool(kTools[id - 21]);
    }
    else if (id == 51) { o.applyZoom(1.3f); }
    else if (id == 52) { o.applyZoom(1.f / 1.3f); }
    else if (id == 55) { o.scrollToPlayhead(); }
    else if (id == 57) { o.setLaneVisible(! o.isLaneVisible()); }

    if (auto* g = o.mGrid.get()) g->grabKeyboardFocus();
}
