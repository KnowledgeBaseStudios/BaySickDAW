#pragma once
#include <JuceHeader.h>
#include "../PatternManager.h"
#include "SharedUI.h"
#include "UndoActions.h"

// ── Piano keyboard strip (left side of piano roll) ────────────────────────────
class PianoKeyboard : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    PianoKeyboard();
    void paint         (juce::Graphics&)                                          override;
    void mouseDown     (const juce::MouseEvent&)                                  override;
    void mouseUp       (const juce::MouseEvent&)                                  override;
    void mouseMove     (const juce::MouseEvent&)                                  override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&)  override;
    juce::String getTooltip() override;

    void setScrollState(int topNote, int noteH);
    // Drum mode: show row labels instead of piano key graphics.
    // Labels are in top-to-bottom order: labels[0] = highest visible note.
    void setDrumRowLabels(const std::vector<juce::String>& labels);
    // J-7b: per-MIDI-note label provider - when set, each visible white key
    // shows the engine-supplied label (e.g. "Snare Center", "Hi-hat Tip")
    // instead of just the C-octave name.  Returning empty falls through to
    // the default name.  Used by BaySickRustyDrums to label the full kit
    // keymap; ignored in drum-row-label mode.
    void setNoteLabelProvider(std::function<juce::String(int)> provider);
    // QA-SfzGroup Sub-Q (2026-05-27): keyswitch label provider.  When non-null
    // AND returns non-empty for a given MIDI note, that key is rendered with
    // a distinct visual (amber highlight + label) to signal it's a keyswitch
    // (not a playable note).  Takes priority over setNoteLabelProvider in
    // PianoKeyboard::paint - keyswitch keys get the special render even if
    // the engine also sets a regular note-label provider.
    void setKeyswitchLabelProvider(std::function<juce::String(int)> provider);
    // J-7b: render every key as white (no black-key strips) so labels are
    // legible across the full range.  Used by BaySickRustyDrums where the
    // sharp/flat distinction has no musical meaning (it's a drum kit).
    void setAllKeysWhiteMode(bool enabled);
    // Live-note monitor: light the keys the user is holding on a hardware MIDI
    // keyboard.  The Piano Roll timer pushes a 128-bit held-note mask each tick
    // (note n -> lo>>n for n<64, hi>>(n-64) otherwise); lit keys render like a
    // mouse-preview.  Repaints only when the mask changes.
    void setLiveHeldNotes(uint64_t lo, uint64_t hi);
    std::function<void(int midiNote, bool noteOn)> onNotePreview;
    // 2026-04-21: forward wheel events to the grid so scrolling works over keys too.
    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;
    // QA-H Task 7 (MIDI-01): Ctrl+click a key selects every note at that
    // pitch in the grid (additive - no preview fires for the click).
    std::function<void(int midiNote)> onCtrlClickPitch;

    static constexpr int kWidth = 64;

private:
    int mTopNote { 96 };
    int mNoteH   { 12 };
    int mPreviewNote { -1 };
    int mHoverNote { -1 };  // J-7b: tracks key under mouse for tooltip
    bool mDrumLabelMode { false };
    bool mAllKeysWhite { false };
    std::vector<juce::String> mDrumRowLabels;  // top-to-bottom row labels
    std::function<juce::String(int)> mNoteLabelProvider;
    std::function<juce::String(int)> mKeyswitchLabelProvider;

    // Live-note monitor mask (message thread only; pushed by setLiveHeldNotes).
    uint64_t mLiveHeldLo { 0 };
    uint64_t mLiveHeldHi { 0 };
    bool isLiveHeld (int note) const
    {
        if (note < 0 || note > 127) return false;
        return note < 64 ? ((mLiveHeldLo >> note)        & 1ull) != 0
                         : ((mLiveHeldHi >> (note - 64)) & 1ull) != 0;
    }

    int noteToY  (int note) const;
    int yToNote  (int y)    const;
    static bool isBlackKey(int note);
};

// ── Piano roll grid (main note-editing area) ──────────────────────────────────
class PianoRollGrid : public juce::Component
{
public:
    enum class PRTool { Draw, Paint, Delete, Mute, Slice, Select, Zoom, Stamp };

    PianoRollGrid();
    void paint         (juce::Graphics&)                                          override;
    void mouseMove     (const juce::MouseEvent&)                                  override;
    void mouseDown     (const juce::MouseEvent&)                                  override;
    void mouseDrag     (const juce::MouseEvent&)                                  override;
    void mouseUp       (const juce::MouseEvent&)                                  override;
    void mouseDoubleClick(const juce::MouseEvent&)                                override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&)  override;
    bool keyPressed    (const juce::KeyPress&)                                    override;

    void setScrollState(float ppb, double beatOff, int topNote, int noteH,
                        int numBars);
    // C.5b: pattern's intrinsic time signature drives bar-line spacing +
    // sub-grid count.  Bar PPQ width = num * (4/den).  Sub-grid count = num.
    void setTimeSignature(int num, int den);
    void setData         (PianoRollData* data);
    void setPlayheadBeat (double beat);
    void setTool         (PRTool t);
    PRTool getActiveTool () const { return mActiveTool; }

    // Armed new-note type.  The grid owns the state; the container's toolbar
    // button mirrors it via onNoteTypeArmChanged (button click and the S key
    // both cycle it; S additionally converts a selection).
    NoteType getNewNoteType  () const { return mNewNoteType; }
    void     cycleNewNoteType();
    std::function<void()> onNoteTypeArmChanged;

    // QA-SlideSampler Task 4: engine-aware note-props context.  On Guitars/Basses
    // rolls the note-props panel shows Flat / RP Slide / Bend + the Bend dropdowns
    // gated to the patch's real bend range; the provider supplies that range.  Null
    // on other engines -> the normal (in-house) note-props panel.
    struct NoteEditContext { bool engineAware = false; int bendUpSemis = 2; int bendDownSemis = 2; };
    void setNoteEditContextProvider (std::function<NoteEditContext()> p) { mNoteEditContextProvider = std::move (p); }
    std::function<NoteEditContext()> mNoteEditContextProvider;

    // ── Selection ─────────────────────────────────────────────────────────
    void selectAll      ();
    void clearSelection ();
    void invertSelection();
    void selectAllAtPitch(int midiNote);   // additive (MIDI-01 keyboard Ctrl+click)
    bool hasSelection   () const { return !mSelection.empty(); }
    // 2026-04-26 (D-7 sub-4): public accessors so the ControlLane can honour
    // selection (red rendering + selection-locked edits + Alt+Wheel target).
    // hasTimeSelection / getTimeSelBeatStart / End / clearTimeSelection are
    // defined further below already - kept here only as documentation pointer.
    const std::vector<int>& getSelection () const { return mSelection; }
    bool isNoteIndexSelected (int idx) const;
    PianoRollData* getData() const { return mData; }

    // ── Scale snap ────────────────────────────────────────────────────────
    // relIntervals: which of the 12 pitch classes (relative to root=0) are in key
    void setScale      (int root, const std::array<bool,12>& relIntervals, bool active);
    // QA-Chords (2026-07-08): the stamp carries BOTH shapes - literal semitone
    // offsets (Mode 2 snaps them into the scale) and scale-degree offsets
    // (Mode 1 stacks degrees of the roll's Root+Scale at the clicked note).
    void setStampChord (const std::vector<int>& semitoneOffsets,
                        const std::vector<int>& degreeOffsets);

    // ── Ghost notes (other rolls shown behind this one, with per-source color) ──
    void setGhostData  (const std::vector<std::pair<const PianoRollData*, juce::Colour>>& ghosts);

    // ── Per-instrument note color ─────────────────────────────────────────
    void setNoteColor  (juce::Colour c);

    // Phase C §P4.2 (2026-04-24): dual-roll mode + active slot for Drums.
    // See PianoRollContainer for full doc.
    enum class RollMode { Standard = 0, DrumGrid = 1, FullRoll = 2 };
    void setRollMode   (RollMode m) { mRollMode = m; repaint(); }
    void setActiveSlot (int slot)   { mActiveSlot = juce::jlimit (0, 15, slot); repaint(); }

    // Phase C §P4.2 (2026-04-24): tag a freshly-created PianoNote based on
    // current RollMode.  Called immediately after `notes.push_back(...)` in
    // every note-creation path so DrumGrid notes get slotIndex from row +
    // midiNote=60, and FullRoll notes get slotIndex from active + midiNote
    // from row.  Standard mode = no-op (Layer/Bass).
    void tagLastCreatedNote (int rowMidi);

    // ── Note grouping ─────────────────────────────────────────────────────
    void groupSelected  ();
    void ungroupSelected();
    void muteSelectedNotes(bool mute);   // 2026-04-26 (D-1): Alt+M / Alt+Shift+M

    // 2026-04-26 (D-7): Smaller Piano Roll bundle helpers.
    void quickQuantizeQuarter();   // Ctrl+Q - snap to nearest 1/4 note
    void quickLegato();            // Ctrl+L - extend each note to start of next
    void flamSelected();           // Alt+F - grace note before each selected
    void deleteTimeRegion();       // Ctrl+Delete - remove time, shift later left
    void toggleResizeFromLeftMode();   // Ctrl+Alt+Home
    void scaleSelectionLevels();   // Alt+X - modal dialog scales velocity %
    // 2026-04-26 (D-7 sub-3): shift the ruler time-selection box left/right
    // by its own length.  Contents don't move; mSelection re-populates with
    // notes inside the new range so the visual highlight stays consistent.
    void shiftTimeSelectionLeft();   // Ctrl+Left
    void shiftTimeSelectionRight();  // Ctrl+Right
    // Bare M fires this callback so the container can hide / show the
    // keyboard column.  PianoRollContainer wires it in its ctor.
    std::function<void()> onToggleKeyboard;

    // ── Edit operations (callable from menu bar / container) ─────────────
    void copySelected      ();
    void pasteClipboard    ();
    void deleteSelected    ();
    void duplicateSelected ();
    void toolQuantize      ();
    void transposeSelection(int semitones);  // nudgeSelection(0, n)
    // QA-UICleanup Task 4: the rest of the tool algorithms are menu-bar-invoked
    // now (folded from the removed Tools popup), same as toolQuantize above.
    void toolChop       (int divisions);
    void toolGlue       ();
    void toolArpeggiate ();
    void toolStrum      ();
    void toolRandomize  ();
    void toolArticulate ();
    void toolGenerateChords();
    void toolHumanize   ();
    void toolRiffMachine();

    // ── Undo / Redo ───────────────────────────────────────────────────────
    // Set the global undo context (called by PianoRollContainer after it
    // receives one from StandaloneEditor via setUndoContext).
    void setUndoContext(const UndoContext& ctx) { mUndoCtx = ctx; }

    // Two-phase commit: call beginEdit() BEFORE mutation, commitEdit() AFTER.
    void beginEdit (const juce::String& label = "Edit");
    void commitEdit();

    // Apply a note snapshot (used by PianoRollEditAction callbacks).
    void applySnapshot(const std::vector<PianoNote>& notes);

    bool canUndo() const { return mUndoCtx.isValid() && mUndoCtx.manager->canUndo(); }
    bool canRedo() const { return mUndoCtx.isValid() && mUndoCtx.manager->canRedo(); }

    // ── Callbacks back up to container ────────────────────────────────────
    std::function<void(float deltaPPB)>          onZoom;
    std::function<void(float factor, int anchorX)> onZoomAnchored; // QA-Ee: cursor-anchored wheel zoom
    std::function<void(double dBeats)>            onHScroll;
    std::function<void(int dNotes)>               onVScroll;
    std::function<void(float factor)>             onVZoom;      // Alt+scroll → vertical zoom
    std::function<void(float factor, int anchorY)> onVZoomAnchored; // QA-Ee: cursor-anchored vertical zoom
    std::function<void()>                         onNotesChanged;
    std::function<void(PRTool)>                   onToolChanged;
    std::function<void()>                         onUndoRedoStateChanged;
    std::function<void(double beatStart,
                       double beatEnd)>           onZoomTo;     // zoom-to-rect
    std::function<void()>                         onZoomToggle; // RMB in Zoom tool
    std::function<int()>                          onGetSnapDiv; // QA-Ee Stage 3: live read of the global snap div
    std::function<int()>                          onGetQuantizeDiv; // QA-UICleanup Task 4: live read of the global quantize div
    std::function<void(double beat)>              onSeek;       // ruler click → seek playhead
    std::function<void(int midiNote)>             onNoteAudition; // legacy one-shot (kept for callers)

    // Mouse-button-held audition (fires on mouseDown of Draw / Move, releases
    // on mouseUp). Pitch changes during a Move drag fire AuditionOff(prev)
    // followed by AuditionOn(new) so the engine plays a continuous sustained
    // note that follows the dragged pitch - matches FL's piano-roll feel.
    std::function<void(int midiNote)>             onNoteAuditionOn;
    std::function<void(int midiNote)>             onNoteAuditionOff;

    // ── Time selection (Ctrl+drag on ruler) ───────────────────────────────
    bool   hasTimeSelection()     const { return mTimeSelBeatStart >= 0.0; }
    double getTimeSelBeatStart()  const { return mTimeSelBeatStart; }
    double getTimeSelBeatEnd()    const { return mTimeSelBeatEnd; }
    void   clearTimeSelection()         { mTimeSelBeatStart = mTimeSelBeatEnd = -1.0; repaint(); }

    static constexpr int kNoteH      = 12;
    static constexpr int kRulerH     = 14; // px at top of grid = click-to-seek ruler zone
    static constexpr int kResizeZone =  6; // px from right edge = resize cursor

    // Fixed-range (drum) mode: note rows start below the ruler strip.
    // Call before setScrollState so noteToY/yToNote use the correct offset.
    void setNoteYOffset    (int yOff)                     { mNoteYOffset = yOff; repaint(); }
    void setFixedNoteRange (bool en, int loNote, int hiNote)
    {
        mIsFixedRange     = en;
        mFixedRangeBottom = loNote;
        mFixedRangeTop    = hiNote;
    }

private:
    // ── Scroll / zoom state ───────────────────────────────────────────────
    float  mPPB       { 80.f };
    double mBeatOff   { 0.0 };
    int    mTopNote   { 96 };
    int    mNoteH     { kNoteH };
    int    mNumBars   { 2 };
    double mPlayhead  { -1.0 };
    // C.5b: pattern intrinsic TS used for bar-line spacing.
    int    mTsNum     { 4 };
    int    mTsDen     { 4 };

    // ── Ruler offset (2026-04-21: default to kRulerH so note rows never
    //    draw under the ruler). Setters can override for special modes.
    int  mNoteYOffset     { kRulerH };
    bool mIsFixedRange    { false };
    int  mFixedRangeBottom { 0 };
    int  mFixedRangeTop    { 127 };
    // Phase C §P4.2 (2026-04-24): drum dual-roll state.  Standard = no slot
    // tagging (Layer/Bass).  DrumGrid uses existing fixed-range layout +
    // slot-from-row.  FullRoll uses pitched layout filtered to active slot.
    RollMode mRollMode    { RollMode::Standard };
    int      mActiveSlot  { 0 };

    // Returns the slot a note belongs to:
    //   note.slotIndex when set (>= 0)
    //   else (51 - note.midiNote) for legacy drum notes in [36..51]
    //   else -1 (non-drum context)
    int effectiveSlot (const PianoNote& n) const noexcept;

    // Phase C §P4.2: row at which a note should render.  In DrumGrid mode,
    // notes with a valid slot render at (51 - slot) regardless of pitch -
    // so non-C5 notes created in FullRoll show as lit cells in DrumGrid.
    // In Standard / FullRoll, display row == midiNote.
    int displayMidiForNote (const PianoNote& n) const noexcept;

    PianoRollData* mData       { nullptr };
    PRTool         mActiveTool { PRTool::Draw };

    // ── Draw interaction ──────────────────────────────────────────────────
    bool   mDrawing   { false };
    bool   mErasing   { false };
    int    mDrawNote  { -1 };
    double mDrawStart { 0.0 };
    double mDrawEnd   { 0.25 };
    // 2026-04-26 (D-7): FL-style click memory.  When the user clicks an
    // existing note, its duration + type are remembered here and used as the
    // length / type of the next click-placed note.  Drag-to-place still wins
    // (mDrawHasDragged tells mouseUp to use the dragged length instead).
    double   mClickMemoryDur  { 0.25 };
    NoteType mClickMemoryType { NoteType::Standard };
    float    mClickMemoryVel  { 0.8f };   // #12 (QA-G3Smoke): last-clicked note's velocity rides placements
    // Smoke round 2 (Jeff): the WHOLE clicked note is the placement template
    // (pan / fine pitch / cutoff / resonance / release / porta length / bend
    // amount + shape) -- not just dur+type+vel.  groupId / muted / slotIndex
    // are per-note intent and never carry.
    PianoNote mClickMemoryProto {};
    bool     mDrawHasDragged  { false };
    // Double-click-created suppression: the first click of a double-click on
    // empty grid CREATES a note, so the pair would instantly open the Note
    // Properties popup on it.  Set by the Draw create paths, reset by the next
    // single-click mouseDown, checked+consumed by mouseDoubleClick.
    bool     mLastClickCreated { false };
    void openNoteProperties (int noteIdx, juce::Point<int> clickPos);

    // ── Move interaction ──────────────────────────────────────────────────
    bool                mMoving { false };
    std::vector<int>    mMoveIndices;       // which notes are being moved
    juce::Point<int>    mMoveDragOrigin;
    std::vector<double> mMoveOrigBeats;
    std::vector<int>    mMoveOrigNotes;

    // ── Mouse-held audition state ─────────────────────────────────────────
    // Tracks the currently-held audition note across mouseDown / mouseDrag /
    // mouseUp.  triggerAudition releases any prior held note before starting
    // a new one - drag pitch changes use this for off-then-on transitions.
    int  mAuditionHeldNote { -1 };
    void triggerAudition (int midiNote);
    void releaseAudition ();

    // ── Resize interaction ────────────────────────────────────────────────
    bool   mResizing        { false };
    int    mResizeNoteIdx   { -1 };  // the grabbed note (the drag delta is measured on it)
    double mResizeOrigDur   { 0.0 };
    double mResizeOrigStart { 0.0 }; // grabbed note's startBeat (immutable during resize)
    // QA-Chords (2026-07-08): selection-based multi-note resize.  When the
    // grabbed edge belongs to a selected note, every selected note (group-
    // expanded, matching Move) resizes by the SAME delta - relative lengths
    // preserved, per-note min-duration clamp (Jeff's D/D2 picks).
    std::vector<int>    mResizeIndices;
    std::vector<double> mResizeOrigDurs;    // parallel to mResizeIndices
    std::vector<double> mResizeOrigStarts;
    void beginResizeGesture (int grabbedIdx);
    // 2026-04-26 (D-7): when on, the right-edge-grab area moves to the LEFT
    // edge so dragging extends a note's start backward (instead of length).
    // Toggled via Ctrl+Alt+Home.
    bool   mResizeFromLeftEnabled { false };
    // Locked at mouseDown to keep the direction stable through the gesture
    // even if the user toggles the global flag mid-drag.
    bool   mResizingFromLeft      { false };

    // ── Marquee selection ─────────────────────────────────────────────────
    bool                 mMarqueeActive { false };
    juce::Point<int>     mMarqueeStart;
    juce::Rectangle<int> mMarqueeRect;

    // ── Slice interaction ─────────────────────────────────────────────────
    bool             mSlicing    { false };
    juce::Point<int> mSliceStart, mSliceEnd;

    // ── Zoom-to-rect interaction ──────────────────────────────────────────
    bool                 mZoomRectActive { false };
    juce::Point<int>     mZoomRectStart;
    juce::Rectangle<int> mZoomRect;

    // ── Time selection (Ctrl+drag on ruler) ──────────────────────────────
    double mTimeSelBeatStart  { -1.0 };   // -1 = no selection
    double mTimeSelBeatEnd    { -1.0 };
    double mTimeSelBeatAnchor {  0.0 };
    bool   mTimeSelDragging   { false };

    // ── Selection ─────────────────────────────────────────────────────────
    std::vector<int> mSelection;

    // ── Undo / Redo ───────────────────────────────────────────────────────
    // ── Undo context + pending edit state ────────────────────────────────
    UndoContext            mUndoCtx;
    std::vector<PianoNote> mPendingBefore;
    juce::String           mPendingLabel;

    // ── Clipboard ─────────────────────────────────────────────────────────
    // 2026-04-26 (D-7 sub-4): static so all Piano-Roll tabs share one
    // clipboard.  Last copy anywhere wins.  Beats are relative to first note.
    static std::vector<PianoNote> sClipboard;

    // ── Marquee ctrl-state ────────────────────────────────────────────────
    bool                 mMarqueeWasCtrl { false };

    // ── Scale snap ────────────────────────────────────────────────────────
    bool                 mScaleActive { false };
    int                  mScaleRoot   { 0 };        // 0=C … 11=B
    std::array<bool, 12> mScaleInKey  {};            // absolute pitch classes in key

    // ── Stamp (chord stamp) tool ──────────────────────────────────────────
    std::vector<int>     mStampIntervals;             // literal semitone offsets (Mode 2 input)
    std::vector<int>     mStampDegrees;               // scale-degree offsets (Mode 1 input)
    juce::Point<int>     mStampPos;

    // ── Ghost notes ───────────────────────────────────────────────────────
    std::vector<std::pair<const PianoRollData*, juce::Colour>> mGhosts;

    // ── Per-instrument color ──────────────────────────────────────────────
    juce::Colour mNoteColor { juce::Colour(0xff44bbff) };  // default: light blue

    // ── New-note type (armed via S key / container toolbar button) ────────
    NoteType             mNewNoteType { NoteType::Standard };

    // ── Coordinate helpers ────────────────────────────────────────────────
    double      xToBeat              (int x)        const;
    int         beatToX              (double beat)  const;
    int         noteToY              (int note)     const;
    int         yToNote              (int y)        const;
    double      snapBeat             (double beat)  const;
    double      snapUnitBeats        ()             const;   // QA-Ee Stage 3: snap unit in beats
    int         noteIndexAtPos       (int x, int y) const;
    PianoNote*  noteAtPos            (int x, int y) const;
    int         noteIndexNearRightEdge(int x, int y) const;
    void        eraseAt              (int x, int y);
    void        updateCursor         ();
    // 2026-04-26 (D-7 sub-3): direction = -1 (left) or +1 (right).
    void        shiftTimeSelectionByLength (int direction);

    // ── Selection helpers ─────────────────────────────────────────────────
    void finaliseMarquee ();
    bool isSelected      (int idx) const;
    void toggleSelection (int idx);

    // ── Operation helpers ─────────────────────────────────────────────────
    void nudgeSelection    (int snapUnits, int semitones); // Shift+arrows
    void nudgeSelectionFine(int pixels,   int semitones); // Alt+arrows
    void sliceNotesOnLine  (juce::Point<int> start, juce::Point<int> end);
    void stampChordAt      (int x, int y);

    // Scale helpers
    int  snapPitchToScale (int midiNote)          const; // nearest in-scale note
    int  nextScalePitch   (int midiNote, int dir) const; // next in-scale in direction (+1/-1)
    // QA-Chords: final MIDI notes a stamp at `clickedNote` produces (Mode 1
    // degree-stacking with Snap OFF / Mode 2 strict-snap + octave-collision
    // resolver with Snap ON).  Shared by stampChordAt + the ghost preview.
    std::vector<int> resolveStampNotes (int clickedNote) const;

    // Group helpers
    int  nextGroupId      ()                           const;
    void expandForGroups  (std::vector<int>& indices)  const;

    // Rebuild mSelection after a sort by matching stored (beat, note) pairs
    void rebuildSelectionFromKeys(const std::vector<std::pair<double,int>>& keys);

    // ── Tools menu (private helper) ──────────────────────────────────────
    std::vector<int> getWorkingSet() const;   // selection (group-expanded) or all notes
};

// ── Control lane (velocity / panning / fine pitch) ────────────────────────────
class ControlLane : public juce::Component
{
public:
    enum Mode { Velocity, Panning, PitchBend, FilterCutoff };

    ControlLane();
    void paint        (juce::Graphics&)                                    override;
    void mouseDown    (const juce::MouseEvent&)                            override;
    void mouseDrag    (const juce::MouseEvent&)                            override;
    void mouseUp      (const juce::MouseEvent&)                            override;
    void mouseMove    (const juce::MouseEvent&)                            override;
    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails&)                   override;

    void setScrollState(float ppb, double beatOff);
    void setData(PianoRollData* data);
    void setMode(Mode m);

    std::function<void()>        onChanged;
    std::function<void(Mode)>    onModeChange; // fired when user changes mode via header
    // 2026-04-26 (D-7 sub-4): selection-aware rendering / editing.  When
    // hasAnySelection returns true the lane only edits notes that
    // isNoteSelected accepts; selected notes paint red.
    std::function<bool(const PianoNote*)> isNoteSelected;
    std::function<bool()>                 hasAnySelection;
    // Fired before any edit so the grid can begin an undo gesture.
    std::function<void(const juce::String&)> onBeginEdit;
    std::function<void()>                    onCommitEdit;
    static constexpr int kHeight  = 240;
    static constexpr int kHeaderH = 16; // dropdown header at top of lane

    // QA-Layout T9 (L29): the header doubles as a resize handle -- a vertical
    // drag sets the lane height (kHeaderH = collapsed min, kHeight = max), a
    // clean click still opens the mode dropdown.  ONE height is shared by
    // every lane in the app (DrumKitControlLane mirrors in lockstep), so it
    // lives behind these statics rather than per-instance.
    static int  getUserHeight();
    static void setUserHeight (int h);
    // Last settled lane visibility -- the default new containers open with and
    // what the project serializer stores.  In-session toggles stay per-container.
    static bool getDefaultVisible();
    static void setDefaultVisible (bool v);
    // Desired height during a header drag; the owning container clamps + applies.
    std::function<void(int)> onHeightDragged;

private:
    float  mPPB     { 80.f };
    double mBeatOff { 0.0 };
    Mode   mMode    { Velocity };
    PianoRollData* mData { nullptr };

    PianoNote* noteNearX  (int x, int y = -1)  const;
    float      getVal     (const PianoNote& n)  const;
    void       setVal     (PianoNote& n, float v);
    float      yToNormVal (int y)              const;
    void       scrubApply (int x0, int x1, float v0, float v1);

    PianoNote* mDragNote { nullptr };  // locked on mouseDown, cleared on mouseUp
    // QA-H Task 6 (#5/MIDI-02): Ctrl+drag scrub - sweeps the SELECTED notes'
    // dots, setting each from the cursor's Y path as it passes their X.
    bool  mScrubbing    { false };
    int   mLastScrubX   { 0 };
    float mLastScrubVal { 0.0f };

    // Header press bookkeeping: mode menu on a clean click (fires on mouseUp),
    // resize once the drag threshold is crossed.
    bool mHeaderPressed    { false };
    bool mHeaderDragged    { false };
    int  mHeaderDragStartY { 0 };
    int  mHeaderDragStartH { 0 };
};

// ── Helper: TextButton with right-click callback ───────────────────────────
class RightClickTextButton : public juce::TextButton
{
public:
    std::function<void(const juce::MouseEvent&)> onRightMouseDown;
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            if (onRightMouseDown) onRightMouseDown(e);
            return;
        }
        TextButton::mouseDown(e);
    }
};


class PianoRollMenuBar;   // defined after PianoRollContainer

// ── PianoRollContainer: menu bar + toolbar + keyboard + grid + lane ───────────
class PianoRollContainer : public juce::Component,
                           private juce::ScrollBar::Listener,
                           private juce::Timer
{
public:
    PianoRollContainer();
    ~PianoRollContainer() override;   // QA-D Task 4: defensive MenuBarComponent teardown
    void resized()    override;
    void paint   (juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(juce::Component::FocusChangeType) override;
    void timerCallback() override;   // QA-UICleanup Task 3: live snap-highlight sync

    void setData         (PianoRollData* data);
    // C.5b: forward pattern's intrinsic TS to the grid.  Caller is the
    // editor's pattern-change handler, so the piano roll updates whenever
    // the active pattern changes or the user manually overrides via the
    // pattern dropdown's "Set Time Signature" submenu.
    void setTimeSignature (int num, int den);
    void setGhostData    (const std::vector<std::pair<const PianoRollData*, juce::Colour>>& ghosts);
    void setNoteColor    (juce::Colour c);
    void setFixedNoteRange(int bottomMidi, int topMidi);
    void setDrumMode     ();

    // Phase C §P4.2 (2026-04-24): dual-roll mode for the Drums page.
    //   DrumGrid (0) = current 16-row fixed-range step grid.  Click writes
    //     slotIndex from row + midiNote=60 (C5 native pitch).  Lit cells
    //     light when ANY note for that slot exists at the beat (pitch-
    //     agnostic).  Click-on-lit removes if the stored note is C5; no-op
    //     if the stored note is at a different pitch (must be removed in
    //     full-roll mode).
    //   FullRoll (1) = standard pitched roll bound to ONE selected slot's
    //     note list.  Click writes midiNote from row + slotIndex from the
    //     active slot setter.  Other slots' notes show as ghosts (Batch 4).
    // Standard = Layer/Bass behaviour (no slot tagging).  DrumGrid + FullRoll
    // are Drums-specific.  DrumsPage explicitly sets one of the latter two;
    // every other piano roll stays Standard so its note creation paths
    // remain unchanged.
    // Mirror of PianoRollGrid::RollMode (kept separate enum so the container
    // header doesn't need PianoRollGrid's full type).
    enum class RollMode { Standard = 0, DrumGrid = 1, FullRoll = 2 };
    void setRollMode    (RollMode m);
    RollMode getRollMode() const { return mRollMode; }
    void setActiveSlot  (int slot);
    int  getActiveSlot  () const { return mActiveSlot; }
    // Drum row labels - forwarded to PianoKeyboard. Top-to-bottom order.
    void setDrumRowLabels(const std::vector<juce::String>& labels);
    // J-7b: forward a per-MIDI-note label provider to the keyboard widget.
    void setNoteLabelProvider(std::function<juce::String(int)> provider);
    // QA-SfzGroup Sub-Q: forward keyswitch label provider to keyboard.
    void setKeyswitchLabelProvider(std::function<juce::String(int)> provider);
    // QA-SlideSampler Task 4: forward the engine-aware note-props context to the grid.
    void setNoteEditContextProvider(std::function<PianoRollGrid::NoteEditContext()> provider);
    // J-7b: paint every keyboard row as a white key (BaySickRustyDrums).
    void setAllKeysWhiteMode(bool enabled);
    // J-7b: scroll the view so `topNote` is the highest visible MIDI note.
    void setTopNote(int topNote);
    // Live-note monitor: forward a held-note mask (from PianoRollPage's timer)
    // to the keyboard strip so hardware-MIDI notes light up on the keys.
    void setLiveHeldNotes(uint64_t lo, uint64_t hi);
    void setPlayheadBeat (double beat);

    // ── Toolbar context label (e.g. "Layer 0 - Harmless") ────────────────
    // Caller pushes a combined "{tab name} - {engine type}" string.
    // Displayed right-aligned in the toolbar row between the zoom buttons
    // and the right edge of the container.
    void setContextLabel (const juce::String& text);

    // Fired when the user clicks a keyboard key (audition).
    // In drum mode: note = 36+slotIndex → auditionSlot(note-36).
    std::function<void(int midiNote, bool noteOn)> onNotePreview;

    // Connect this container and its grid to the global undo system.
    void setUndoContext(const UndoContext& ctx);

    // Global undo/redo - delegates to ctx.manager
    void undoRoll        ();
    void redoRoll        ();
    // Show the global history window via callback (wired by StandaloneEditor)
    std::function<void()> onShowHistoryWindow;

    // Fired when the user clicks the ruler to seek the playhead
    std::function<void(double beat)> onSeek;
    // #30b regression fix (QA-G3Smoke): fired after EVERY grid note mutation
    // (the grid's onNotesChanged tail).  StandaloneEditor routes it to
    // PatternManager::notifyContentChanged so the scheduler's roll snapshot
    // republishes -- without this, freshly edited notes exist on screen but
    // never reach the audio thread.
    std::function<void()> onContentEdited;

    // Fired when a note is created or its pitch changes (for engine audition).
    // One-shot - engine receives noteOn + auto-noteOff at end of buffer.
    std::function<void(int midiNote)> onNoteAudition;

    // Fired by the on-screen piano keyboard on key press / release respectively.
    // Used for press-and-hold audition (engine receives noteOn on press, noteOff
    // on release). Lets the user hear a sustained note for the full press
    // duration, which the one-shot path cannot.
    std::function<void(int midiNote)> onNoteAuditionOn;
    std::function<void(int midiNote)> onNoteAuditionOff;

    // ── Tool / zoom (also invoked by menu bar) ────────────────────────────
    void setActiveTool (PianoRollGrid::PRTool t);
    void applyZoom     (float factor);
    void applyZoomAnchored (float factor, int anchorX);   // QA-Ee: cursor-anchored wheel zoom
    void applyVZoom    (float factor);
    void applyVZoomAnchored (float factor, int anchorY);  // QA-Ee: cursor-anchored vertical zoom

    // QA-Ee Stage 3: wire the GLOBAL Piano Roll snap accessors (provided by
    // PianoRollPage).  getter -> live div for the grid's snapBeat; setter -> menu writes.
    void setSnapAccessors (std::function<int()> getter, std::function<void(int)> setter);
    // QA-UICleanup Task 4: GLOBAL quantize accessors (provided by PianoRollPage).
    // getter -> live div for toolQuantize; setter -> Tools>Quantize Settings writes.
    void setQuantizeAccessors (std::function<int()> getter, std::function<void(int)> setter);
    int  getQuantizeDiv() const { return mOnGetQuantizeDiv ? mOnGetQuantizeDiv() : 0; }
    void setQuantizeDiv (int div) { if (mOnSetQuantizeDiv) mOnSetQuantizeDiv (div); }
    // QA-Ee Stage 2: content-bound dynamic zoom limits (active pattern's notes).
    float contentMaxBars() const;
    float minZoomPPB (float vpW) const;
    float maxZoomPPB (float vpW) const;

    // ── Edit actions (invoked by menu bar) ────────────────────────────────
    void transposeSelection     (int semitones);
    void scrollToPlayhead       ();
    void setLaneVisible         (bool v);
    void setGhostsVisible       (bool v);

    // ── Scale / chord state (set by menu bar, read back for checkmarks) ───
    void setScaleActive(bool active);
    void setScaleRoot  (int rootIdx);     // 0=C … 11=B
    void setScaleName  (int nameIdx);     // index into kScaleDefs
    void selectChord   (int chordIdx);    // index into kChordDefs; arms Stamp tool

    bool isScaleActive()   const { return mScaleActive;   }
    int  getScaleRootIdx() const { return mScaleRootIdx;  }
    int  getScaleNameIdx() const { return mScaleNameIdx;  }
    int  getChordIdx()     const { return mChordIdx;      }
    bool isLaneVisible()   const { return mLaneVisible;   }
    bool isGhostsVisible() const { return mGhostsVisible; }
    PianoRollGrid::PRTool getActiveTool() const { return mActiveTool; }

    // Forwarded time-selection accessors (for loop/stop-seek integration)
    bool   hasTimeSelection()    const { return mGrid && mGrid->hasTimeSelection(); }
    double getTimeSelBeatStart() const { return mGrid ? mGrid->getTimeSelBeatStart() : 0.0; }
    double getTimeSelBeatEnd()   const { return mGrid ? mGrid->getTimeSelBeatEnd()   : 0.0; }

    static constexpr int kMenuBarH  = 20;  // menu bar above toolbar
    // Toolbar is always a single 28 px row (row 2 has been folded in).
    static constexpr int kToolbarH  = 28;
    static constexpr int kScrollH   = 12;
    static constexpr int kScrollBarSz = 12; // width of V scrollbar / height of H scrollbar

private:
    std::unique_ptr<PianoKeyboard>  mKeyboard;
    std::unique_ptr<PianoRollGrid>  mGrid;
    std::unique_ptr<ControlLane>    mLane;
    // 2026-04-26 (D-7 sub-2): bare-M toggles this - when false the keyboard
    // column collapses to 0 width and the grid expands across the freed space.
    // Per-instance: each Piano Roll tab has its own visibility state.
    bool mKeyboardVisible { true };

    // ── Toolbar context label ────────────────────────────────────────────
    std::unique_ptr<juce::Label>    mContextLabel;

    // ── Visible scrollbars ───────────────────────────────────────────────
    // mHScroll drives mBeatOff (shared by grid + lane).
    // mVScroll drives mTopNote (grid only; hidden in fixed-range/drum mode).
    std::unique_ptr<juce::ScrollBar> mHScroll;
    std::unique_ptr<juce::ScrollBar> mVScroll;
    bool mPushingToBars { false }; // guard against scrollbar → syncScrollState → bar feedback

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* sb, double newRangeStart) override;

    // Toolbar row 1: Magnet | tool buttons | armed note type | Undo | Redo | H
    std::array<std::unique_ptr<juce::TextButton>, 8> mToolBtns; // [7] = Stamp, always hidden
    std::unique_ptr<juce::TextButton>        mNoteTypeBtn;   // mirrors grid's armed type
    void refreshNoteTypeButton();
    std::unique_ptr<juce::TextButton>        mUndoBtn, mRedoBtn, mHistoryBtn;
    std::unique_ptr<RightClickTextButton>    mMagnetBtn;     // snap resolution dropdown

    // Toolbar row 2
    std::unique_ptr<juce::TextButton> mZoomInBtn, mZoomOutBtn;

    PianoRollData* mData { nullptr };
    UndoContext    mUndoCtx;

    float  mPPB        { 80.f };
    float  mNoteHScale {  1.f };   // vertical zoom multiplier
    double mBeatOff    { 0.0 };
    int    mTopNote    { 96 };
    int    mNumBars    { 2 };
    std::function<int()>     mOnGetSnapDiv;   // QA-Ee Stage 3: global snap read (PianoRollPage)
    std::function<void(int)> mOnSetSnapDiv;   // QA-Ee Stage 3: global snap write
    std::function<int()>     mOnGetQuantizeDiv;   // QA-UICleanup Task 4: global quantize read
    std::function<void(int)> mOnSetQuantizeDiv;   // QA-UICleanup Task 4: global quantize write
    PianoRollGrid::PRTool mActiveTool { PianoRollGrid::PRTool::Draw };

    // Zoom-to-rect undo state
    float  mPreZoomPPB     { 80.f };
    double mPreZoomBeatOff { 0.0 };
    bool   mZoomedIn       { false };

    // Fixed note range (drums, locked view)
    bool   mFixedRange        { false };
    int    mFixedRangeBottom  { 0 };
    int    mFixedRangeTop     { 127 };
    bool   mDrumMode          { false };
    // Phase C §P4.2 (2026-04-24): roll-mode + active-slot for drum dual mode.
    // Container default Standard - DrumsPage flips to DrumGrid in its ctor.
    RollMode mRollMode    { RollMode::Standard };
    int      mActiveSlot  { 0 };   // 0..15, used in FullRoll mode


    friend class PianoRollMenuBar;

    // Menu bar.  QA-D Task 4 / QA-0a finding #8: model declared FIRST so it
    // outlives the MenuBarComponent (member destruction order is reverse of
    // declaration order; mMenuBar dies first, then mMenuBarModel).  Required
    // because juce::MenuBarComponent's destructor calls back into its model.
    std::unique_ptr<PianoRollMenuBar>       mMenuBarModel;
    std::unique_ptr<juce::MenuBarComponent> mMenuBar;

    // Scale / chord state (was in toolbar combos, now menu-driven)
    bool  mScaleActive  { false };
    int   mScaleRootIdx { 0 };      // 0=C … 11=B
    int   mScaleNameIdx { 1 };      // 1 = Major
    int   mChordIdx     { 0 };      // 0 = Major chord

    // Visibility toggles
    bool  mLaneVisible   { true };
    bool  mGhostsVisible { true };

    // Stored ghost data (re-forwarded when visibility changes)
    std::vector<std::pair<const PianoRollData*, juce::Colour>> mGhostStore;

    // Playhead beat (cached so scrollToPlayhead can use it)
    double mPlayheadBeat { -1.0 };

    void updateUndoRedoBtns();
    void syncScrollState   ();
    void onHScroll         (double dBeats);
    void onVScroll         (int dNotes);
    void updateScaleFromUI ();
    void pushScrollStateToBars(); // one-shot helper used by syncScrollState
};

// ── Piano roll menu bar model ─────────────────────────────────────────────────
class PianoRollMenuBar : public juce::MenuBarModel
{
public:
    explicit PianoRollMenuBar(PianoRollContainer& owner) : mOwner(owner) {}
    ~PianoRollMenuBar() override { setApplicationCommandManagerToWatch(nullptr); }

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu   getMenuForIndex (int index, const juce::String& name) override;
    void              menuItemSelected(int itemId, int topLevelIndex) override;

private:
    PianoRollContainer& mOwner;
};
