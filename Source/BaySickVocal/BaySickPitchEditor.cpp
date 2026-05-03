#include "BaySickPitchEditor.h"
#include "BaySickVocalProcessor.h"

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchEditor — H-6b (2026-05-02)
//
// Newtone-clone visual + interaction model.  See header for layout summary.
// H-6b ships the empty-state shell: layout + paint match Newtone, but no
// recording is loaded yet (G-9 wires the source).
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    // ── Layout constants ────────────────────────────────────────────────────
    constexpr int kToolbarH   = 70;
    constexpr int kRulerH     = 16;
    constexpr int kKeyboardW  = 40;
    constexpr int kInfoBarH   = 22;

    constexpr int kBigKnobSz  = 52;       // CENTER / VARIATION / TRANS

    // ── Palette: matches the existing PianoRoll (VC::Bg / VC::Panel /
    //    VC::Accent) so BaySickPitch reads as part of the same family.
    const juce::Colour kCanvas      = juce::Colour (0xff252529);   // VC::Panel
    const juce::Colour kGridFaint   = juce::Colour (0xff333339);   // VC::Accent ~50% alpha baked
    const juce::Colour kGridStrong  = juce::Colour (0xff3a3a3e);   // VC::Accent
    const juce::Colour kRulerBg     = juce::Colour (0xff2e2e33);   // VC::Surface
    const juce::Colour kToolbarBg   = juce::Colour (0xff1c1c1e);   // VC::Bg
    const juce::Colour kToolbarText = juce::Colour (0xffe0e0e8);   // VC::Text
    const juce::Colour kKeyWhite    = juce::Colour (0xff8a939d);
    const juce::Colour kKeyBlack    = juce::Colour (0xff2e3742);
    const juce::Colour kKeyBorder   = juce::Colour (0xff202830);
    const juce::Colour kPitchCurve  = juce::Colour (0xffe89c5a);
    const juce::Colour kPlayhead    = juce::Colour (0xfff05030);
    const juce::Colour kInfoBarBg   = juce::Colour (0xff252529);   // VC::Panel
}

// ─────────────────────────────────────────────────────────────────────────────
// Toolbar
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchEditor::Toolbar : public juce::Component
{
public:
    explicit Toolbar (BaySickPitchEditor& owner) : mOwner (owner)
    {
        auto plain = [this](juce::TextButton& b, const juce::String& txt,
                             const juce::String& tooltip)
        {
            b.setButtonText (txt);
            b.setTooltip (tooltip);
            b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff404a55));
            b.setColour (juce::TextButton::textColourOnId,  kToolbarText);
            b.setColour (juce::TextButton::textColourOffId, kToolbarText);
            addAndMakeVisible (b);
        };

        // Transport row (top-left)
        plain (mLoopBtn, "Loop", "Loop Mode (L)");
        mLoopBtn.setClickingTogglesState (true);

        plain (mPlayBtn, "Play", "Play / Stop (Spacebar)");
        plain (mStopBtn, "Stop", "Stop");

        // Edit-mode row (right of transport)
        plain (mCutBtn,      "Cut",  "Cut Mode (C)");
        mCutBtn.setClickingTogglesState (true);
        mCutBtn.setRadioGroupId (1);
        mCutBtn.onClick = [this] { mOwner.setEditMode (BaySickPitchEditor::EditMode::Cut); };

        plain (mAdvBtn,      "Adv",  "Advanced Edit Mode (Ctrl+Shift+A)");
        mAdvBtn.setClickingTogglesState (true);
        mAdvBtn.setRadioGroupId (1);
        mAdvBtn.setToggleState (true, juce::dontSendNotification);
        mAdvBtn.onClick = [this] { mOwner.setEditMode (BaySickPitchEditor::EditMode::Advanced); };

        plain (mVibBtn,      "Vib",  "Vibrato Edit Mode");
        mVibBtn.setClickingTogglesState (true);
        mVibBtn.setRadioGroupId (1);
        mVibBtn.onClick = [this] { mOwner.setEditMode (BaySickPitchEditor::EditMode::Vibrato); };

        plain (mSlavedBtn,   "Slv",  "Slaved Playback Mode (H) -- follow global transport");
        mSlavedBtn.setClickingTogglesState (true);

        plain (mAutoBtn,     "Auto", "Auto-Scroll Mode (A)");
        mAutoBtn.setClickingTogglesState (true);

        // File-action row (icons-as-text since we don't ship icon resources yet)
        plain (mLoadBtn,   "Load",   "Load Sample (Ctrl+O) -- only used pre-G-9");
        plain (mSaveBtn,   "Save",   "Render (Ctrl+S)");
        mSaveBtn.setEnabled (false);   // disabled until source loaded
        plain (mResetBtn,  "Reset",  "Reset all edits");
        plain (mCenterPRBtn, "->PR", "Send Selection to Piano Roll");

        // Title + filename labels (top-left text strip)
        addAndMakeVisible (mTitleLbl);
        mTitleLbl.setText ("BaySickPitch", juce::dontSendNotification);
        mTitleLbl.setColour (juce::Label::textColourId, kToolbarText);
        mTitleLbl.setFont (juce::Font (14.0f, juce::Font::bold));

        addAndMakeVisible (mFileLbl);
        mFileLbl.setText ("(no recording loaded)", juce::dontSendNotification);
        mFileLbl.setColour (juce::Label::textColourId,
                             kToolbarText.withAlpha (0.65f));
        mFileLbl.setFont (juce::Font (11.0f));

        addAndMakeVisible (mLengthLbl);
        mLengthLbl.setText ("LENGTH/SEL --", juce::dontSendNotification);
        mLengthLbl.setColour (juce::Label::textColourId,
                               kToolbarText.withAlpha (0.65f));
        mLengthLbl.setFont (juce::Font (10.0f));

        addAndMakeVisible (mTempoLbl);
        mTempoLbl.setText ("TEMPO --", juce::dontSendNotification);
        mTempoLbl.setColour (juce::Label::textColourId,
                              kToolbarText.withAlpha (0.65f));
        mTempoLbl.setFont (juce::Font (10.0f));

        addAndMakeVisible (mSyncLbl);
        mSyncLbl.setText ("SYNC", juce::dontSendNotification);
        mSyncLbl.setColour (juce::Label::textColourId,
                             kToolbarText.withAlpha (0.65f));
        mSyncLbl.setFont (juce::Font (10.0f));

        // 3 big global knobs (top-right) -- CENTER / VARIATION / TRANS
        auto bigKnob = [this](juce::Slider& s, juce::Label& l,
                               const juce::String& nm, const juce::String& tt)
        {
            s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
            s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            s.setRange (0.0, 100.0, 0.5);
            s.setValue (50.0, juce::dontSendNotification);
            s.setTooltip (tt);
            s.setColour (juce::Slider::rotarySliderFillColourId,
                          juce::Colour (0xff7a8898));
            s.setColour (juce::Slider::rotarySliderOutlineColourId,
                          juce::Colour (0xff1d242d));
            addAndMakeVisible (s);

            l.setText (nm, juce::dontSendNotification);
            l.setColour (juce::Label::textColourId, kToolbarText);
            l.setFont (juce::Font (10.0f, juce::Font::bold));
            l.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (l);
        };
        bigKnob (mCenter,    mCenterLbl,    "CENTER",
                  "Global pitch correction. Scales all notes toward 100% nearest pitch.");
        bigKnob (mVariation, mVariationLbl, "VARIATION",
                  "Global pitch variation correction. Controls vibrato + pitch errors.");
        bigKnob (mTrans,     mTransLbl,     "TRANS",
                  "Global pitch transition speed between notes.");
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kToolbarBg);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6, 4);

        // ── Top row (transport + edit-mode buttons + file-action buttons) ──
        const int btnH = 20;
        auto topRow = b.removeFromTop (btnH);
        b.removeFromTop (4);

        auto place = [&topRow] (juce::Component& c, int w) {
            c.setBounds (topRow.removeFromLeft (w));
            topRow.removeFromLeft (3);
        };
        place (mLoopBtn, 36);
        place (mPlayBtn, 36);
        place (mStopBtn, 36);
        topRow.removeFromLeft (8);
        place (mCutBtn,    34);
        place (mAdvBtn,    34);
        place (mVibBtn,    34);
        topRow.removeFromLeft (6);
        place (mSlavedBtn, 30);
        place (mAutoBtn,   34);
        topRow.removeFromLeft (6);
        place (mLoadBtn,    40);
        place (mSaveBtn,    40);
        place (mResetBtn,   40);
        place (mCenterPRBtn, 40);

        // ── Bottom row (title + filename + length/tempo info) + big knobs ──
        const int rightW = (kBigKnobSz + 6) * 3 + 6;
        auto right = b.removeFromRight (rightW);

        // 3 big knobs across the right edge (vertically centered in the
        // remaining toolbar space), each with its label below the dial.
        auto laneW = right.getWidth() / 3;
        auto lane0 = right.removeFromLeft (laneW);
        auto lane1 = right.removeFromLeft (laneW);
        auto lane2 = right;

        auto layoutKnob = [] (juce::Rectangle<int> lane, juce::Slider& s, juce::Label& l)
        {
            const int kw = juce::jmin (kBigKnobSz, lane.getWidth() - 4);
            auto knobR = lane.withSizeKeepingCentre (kw, kw)
                              .translated (0, -4);
            s.setBounds (knobR);
            l.setBounds (lane.withTrimmedTop (knobR.getBottom() - lane.getY() + 1)
                              .withHeight (12));
        };
        layoutKnob (lane0, mCenter,    mCenterLbl);
        layoutKnob (lane1, mVariation, mVariationLbl);
        layoutKnob (lane2, mTrans,     mTransLbl);

        // Remaining left area for title + filename + info strip
        auto left = b;
        const int titleW = 130;
        mTitleLbl.setBounds (left.removeFromLeft (titleW).withHeight (16));
        left.removeFromLeft (6);

        auto info = left;
        const int infoH = 14;
        auto row = info.removeFromTop (infoH);
        mFileLbl.setBounds (row);

        info.removeFromTop (2);
        auto row2 = info.removeFromTop (infoH);
        mLengthLbl.setBounds (row2.removeFromLeft (110));
        row2.removeFromLeft (8);
        mTempoLbl.setBounds (row2.removeFromLeft (90));
        row2.removeFromLeft (8);
        mSyncLbl.setBounds (row2.removeFromLeft (40));
    }

    void setRenderEnabled (bool b) { mSaveBtn.setEnabled (b); }

    void setEditModeUI (BaySickPitchEditor::EditMode m)
    {
        mCutBtn.setToggleState (m == BaySickPitchEditor::EditMode::Cut,      juce::dontSendNotification);
        mAdvBtn.setToggleState (m == BaySickPitchEditor::EditMode::Advanced, juce::dontSendNotification);
        mVibBtn.setToggleState (m == BaySickPitchEditor::EditMode::Vibrato,  juce::dontSendNotification);
    }

private:
    BaySickPitchEditor& mOwner;

    juce::TextButton mLoopBtn, mPlayBtn, mStopBtn;
    juce::TextButton mCutBtn, mAdvBtn, mVibBtn;
    juce::TextButton mSlavedBtn, mAutoBtn;
    juce::TextButton mLoadBtn, mSaveBtn, mResetBtn, mCenterPRBtn;

    juce::Label mTitleLbl, mFileLbl, mLengthLbl, mTempoLbl, mSyncLbl;

    juce::Slider mCenter,    mVariation,    mTrans;
    juce::Label  mCenterLbl, mVariationLbl, mTransLbl;
};

// ─────────────────────────────────────────────────────────────────────────────
// PitchKeyboard
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchEditor::PitchKeyboard : public juce::Component
{
public:
    explicit PitchKeyboard (BaySickPitchEditor& owner) : mOwner (owner) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kKeyBlack);

        const int top    = mOwner.topNote();
        const int bot    = mOwner.bottomNote();
        const int rows   = juce::jmax (1, top - bot + 1);
        const float rowH = (float) getHeight() / (float) rows;

        for (int n = bot; n <= top; ++n)
        {
            const int row = top - n;
            const float y = row * rowH;
            const bool black = isBlackKey (n);

            g.setColour (black ? kKeyBlack : kKeyWhite);
            g.fillRect (0.0f, y, (float) getWidth(), rowH);

            g.setColour (kKeyBorder);
            g.drawHorizontalLine ((int) (y + rowH), 0.0f, (float) getWidth());

            // Label every C with its octave (FL Studio convention --
            // MIDI 0 = C0, middle C = C5 = MIDI 60, MIDI 120 = C10).
            if ((n % 12) == 0)
            {
                g.setColour (juce::Colour (0xff1a2028));
                g.setFont (juce::Font (9.0f, juce::Font::bold));
                g.drawText ("C" + juce::String (n / 12),
                            juce::Rectangle<float> (2.0f, y, (float) getWidth() - 4.0f, rowH),
                            juce::Justification::centredLeft);
            }
        }
    }

private:
    static bool isBlackKey (int midi) noexcept
    {
        const int n = ((midi % 12) + 12) % 12;
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    BaySickPitchEditor& mOwner;
};

// ─────────────────────────────────────────────────────────────────────────────
// PitchGrid -- includes the bar/beat ruler at the top of itself.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchEditor::PitchGrid : public juce::Component
{
public:
    explicit PitchGrid (BaySickPitchEditor& owner) : mOwner (owner) {}

    void paint (juce::Graphics& g) override
    {
        // Bar/beat ruler strip at top
        auto local = getLocalBounds();
        auto ruler = local.removeFromTop (kRulerH);
        g.setColour (kRulerBg);
        g.fillRect (ruler);

        const double pps  = mOwner.pixelsPerSecond();
        const double secL = mOwner.scrollSeconds();
        const double secR = secL + getWidth() / juce::jmax (1.0, pps);

        g.setColour (kToolbarText.withAlpha (0.50f));
        g.setFont (juce::Font (10.0f));
        for (int sec = (int) std::floor (secL); (double) sec <= secR + 1.0; ++sec)
        {
            const int x = (int) ((sec - secL) * pps);
            if (x < 0 || x >= getWidth()) continue;
            g.drawVerticalLine (x, 0.0f, (float) ruler.getHeight());
            g.drawText (juce::String (sec) + "s",
                        x + 2, 0, 36, ruler.getHeight(),
                        juce::Justification::centredLeft);
        }

        // Canvas body
        g.setColour (kCanvas);
        g.fillRect (local);

        const int top  = mOwner.topNote();
        const int bot  = mOwner.bottomNote();
        const int rows = juce::jmax (1, top - bot + 1);
        const float rowH = (float) local.getHeight() / (float) rows;

        // Horizontal semitone lines + bolder octaves
        for (int n = bot; n <= top + 1; ++n)
        {
            const float y = (float) local.getY() + (top - n + 1) * rowH;
            g.setColour ((n % 12) == 0 ? kGridStrong : kGridFaint);
            g.drawHorizontalLine ((int) y, 0.0f, (float) getWidth());
        }

        // Vertical second grid lines
        for (int sec = (int) std::floor (secL); (double) sec <= secR + 1.0; ++sec)
        {
            const int x = (int) ((sec - secL) * pps);
            if (x < 0 || x >= getWidth()) continue;
            g.setColour (kGridFaint);
            g.drawVerticalLine (x, (float) local.getY(), (float) (local.getBottom()));
        }

        // G-9: draw note blobs (waveform-inside-blob) + orange pitch curve
        // overlay + per-note volume / pitch ramp / formant / vibrato curves
        // here once audio is loaded.  Empty grid renders as just the grid
        // -- the toolbar's filename label "(no recording loaded)" is the
        // only empty-state indicator.  Interaction handlers below.
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (! mOwner.hasAudio()) return;
        // G-9: hit-test note blobs / handles per current edit mode.
    }

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (! mOwner.hasAudio()) return;
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! mOwner.hasAudio()) return;
    }

    void mouseWheelMove (const juce::MouseEvent& e,
                          const juce::MouseWheelDetails& w) override
    {
        // Forward to the editor so the same wheel logic fires whether the
        // event happened over the grid or the keyboard.
        mOwner.mouseWheelMove (e, w);
    }

private:
    BaySickPitchEditor& mOwner;
};

// ─────────────────────────────────────────────────────────────────────────────
// InfoBar
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchEditor::InfoBar : public juce::Component
{
public:
    InfoBar()
    {
        addAndMakeVisible (mLbl);
        mLbl.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(),
                                   12.0f, juce::Font::plain));
        mLbl.setColour (juce::Label::textColourId, kToolbarText.withAlpha (0.85f));
        mLbl.setJustificationType (juce::Justification::centredLeft);
        mLbl.setText ("Pitch: --   Cents: --   Length: --   Loop: --",
                       juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kInfoBarBg);
    }

    void resized() override
    {
        mLbl.setBounds (getLocalBounds().reduced (8, 0));
    }

private:
    juce::Label mLbl;
};

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchEditor
// ─────────────────────────────────────────────────────────────────────────────
BaySickPitchEditor::BaySickPitchEditor (BaySickVocalProcessor& p) : mProc (p)
{
    mToolbar  = std::make_unique<Toolbar>       (*this);
    mKeyboard = std::make_unique<PitchKeyboard> (*this);
    mGrid     = std::make_unique<PitchGrid>     (*this);
    mInfoBar  = std::make_unique<InfoBar>       ();

    addAndMakeVisible (*mToolbar);
    addAndMakeVisible (*mKeyboard);
    addAndMakeVisible (*mGrid);
    addAndMakeVisible (*mInfoBar);

    addAndMakeVisible (mHScroll);
    addAndMakeVisible (mVScroll);
    mHScroll.addListener (this);
    mVScroll.addListener (this);
    mHScroll.setColour (juce::ScrollBar::thumbColourId,
                          juce::Colour (0xff7a8898));
    mVScroll.setColour (juce::ScrollBar::thumbColourId,
                          juce::Colour (0xff7a8898));
    mHScroll.setAutoHide (false);
    mVScroll.setAutoHide (false);
}

BaySickPitchEditor::~BaySickPitchEditor()
{
    mHScroll.removeListener (this);
    mVScroll.removeListener (this);
}

void BaySickPitchEditor::setEditMode (EditMode m)
{
    if (m == mEditMode) return;
    mEditMode = m;
    if (mToolbar) mToolbar->setEditModeUI (m);
    if (mGrid)    mGrid->repaint();
}

void BaySickPitchEditor::paint (juce::Graphics& g)
{
    g.fillAll (kCanvas);
}

void BaySickPitchEditor::resized()
{
    auto b = getLocalBounds();
    mToolbar->setBounds (b.removeFromTop    (kToolbarH));
    mInfoBar->setBounds (b.removeFromBottom (kInfoBarH));

    // Carve out scroll-bar tracks from the remaining canvas.  H-scroll runs
    // along the bottom (above the InfoBar but below the grid + keyboard).
    // V-scroll runs along the right edge (right of the grid).
    auto hTrack = b.removeFromBottom (kHScrollH);
    auto vTrack = b.removeFromRight  (kVScrollW);

    mKeyboard->setBounds (b.removeFromLeft (kKeyboardW)
                            .withTrimmedTop (kRulerH));   // align under ruler
    mGrid    ->setBounds (b);

    // H-scroll stops at the keyboard on the left and at the V-scroll column
    // on the right, leaving a small dead corner in the bottom-right.
    mHScroll.setBounds (hTrack.withTrimmedLeft (kKeyboardW)
                                .withTrimmedRight (kVScrollW));
    mVScroll.setBounds (vTrack.withTrimmedTop (kRulerH));

    syncScrollBarsToState();
}

void BaySickPitchEditor::syncScrollBarsToState()
{
    if (mPushingToBars) return;
    mPushingToBars = true;

    // ── Horizontal scroll: range = total length in seconds (placeholder
    //    kEmptyStateLengthSec until G-9 gives a real length); thumb = the
    //    visible window in seconds.
    const int gridW = juce::jmax (1, mGrid ? mGrid->getWidth() : getWidth());
    const double visibleSec = gridW / juce::jmax (1.0, mPpsX);
    const double totalSec   = juce::jmax (visibleSec, kEmptyStateLengthSec);
    mHScroll.setRangeLimits (0.0, totalSec);
    mHScroll.setCurrentRange (mScrollX, visibleSec, juce::dontSendNotification);

    // ── Vertical scroll: range = full MIDI 0..127, thumb = visible
    //    semitones (mTopNote - mBottomNote + 1).  Top of bar = highest
    //    visible note (so dragging up scrolls toward higher pitches).
    const double visibleNotes = juce::jmax (1, mTopNote - mBottomNote + 1);
    mVScroll.setRangeLimits (0.0, 128.0);
    // Bar value = position of mBottomNote from the bottom; we present
    // "MIDI 0 at top of range" so high notes appear at the top of the bar.
    const double barTop = juce::jlimit (0.0, 128.0 - visibleNotes,
                                          (double) (127 - mTopNote));
    mVScroll.setCurrentRange (barTop, visibleNotes, juce::dontSendNotification);

    mPushingToBars = false;
}

void BaySickPitchEditor::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (mPushingToBars) return;

    if (bar == &mHScroll)
    {
        mScrollX = juce::jmax (0.0, newRangeStart);
    }
    else if (bar == &mVScroll)
    {
        const int visibleNotes = juce::jmax (1, mTopNote - mBottomNote + 1);
        const int newTop = juce::jlimit (visibleNotes - 1, 127,
                                           127 - (int) std::round (newRangeStart));
        mTopNote    = newTop;
        mBottomNote = newTop - visibleNotes + 1;
    }

    repaint();
}

void BaySickPitchEditor::zoomHorizontalBy (double factor)
{
    const double newPps = juce::jlimit (20.0, 400.0, mPpsX * factor);
    if (juce::approximatelyEqual (newPps, mPpsX)) return;

    // Keep the visible center fixed: anchor the time at the midpoint of
    // the canvas, recompute scrollX for the new pps.
    const int gridW = juce::jmax (1, mGrid ? mGrid->getWidth() : getWidth());
    const double centerSec = mScrollX + (gridW * 0.5) / juce::jmax (1.0, mPpsX);
    mPpsX    = newPps;
    mScrollX = juce::jmax (0.0, centerSec - (gridW * 0.5) / mPpsX);

    syncScrollBarsToState();
    repaint();
}

void BaySickPitchEditor::zoomVerticalBy (double factor)
{
    const int currentVisible = juce::jmax (1, mTopNote - mBottomNote + 1);
    const int newVisible = juce::jlimit (12, 96,
                                           (int) std::round (currentVisible / factor));
    if (newVisible == currentVisible) return;

    const int center = (mTopNote + mBottomNote) / 2;
    int newTop = center + newVisible / 2;
    int newBot = newTop - newVisible + 1;
    newTop = juce::jlimit (newVisible - 1, 127, newTop);
    newBot = newTop - newVisible + 1;

    mTopNote    = newTop;
    mBottomNote = newBot;

    syncScrollBarsToState();
    repaint();
}

void BaySickPitchEditor::setScrollSeconds (double secs)
{
    mScrollX = juce::jmax (0.0, secs);
    syncScrollBarsToState();
    repaint();
}

void BaySickPitchEditor::setTopNote (int note)
{
    const int visibleNotes = juce::jmax (1, mTopNote - mBottomNote + 1);
    mTopNote    = juce::jlimit (visibleNotes - 1, 127, note);
    mBottomNote = mTopNote - visibleNotes + 1;
    syncScrollBarsToState();
    repaint();
}

void BaySickPitchEditor::mouseWheelMove (const juce::MouseEvent&,
                                           const juce::MouseWheelDetails& w)
{
    const auto& mods = juce::ModifierKeys::currentModifiers;

    if (mods.isCtrlDown())
    {
        // Ctrl+wheel = horizontal zoom
        zoomHorizontalBy (w.deltaY > 0.0f ? 1.15 : 1.0 / 1.15);
    }
    else if (mods.isAltDown())
    {
        // Alt+wheel = vertical zoom
        zoomVerticalBy (w.deltaY > 0.0f ? 1.15 : 1.0 / 1.15);
    }
    else if (mods.isShiftDown())
    {
        // Shift+wheel = horizontal scroll
        const int gridW = juce::jmax (1, mGrid ? mGrid->getWidth() : getWidth());
        const double stepSec = (gridW / juce::jmax (1.0, mPpsX)) * 0.1;
        setScrollSeconds (mScrollX - w.deltaY * stepSec);
    }
    else
    {
        // Plain wheel = vertical scroll (move the visible note range).
        // deltaY > 0 (wheel up) -> view moves up (higher pitches), mTopNote
        // increases.  deltaY < 0 (wheel down) -> view moves down (lower
        // pitches), mTopNote decreases.  Don't clamp the step magnitude
        // via jmax(1, ...) -- that drops the sign for negative deltas and
        // makes both directions scroll up.
        int step = (int) std::round (w.deltaY * 3.0);
        if (step == 0)                   // tiny wheel deltas: still move 1 note
            step = (w.deltaY >= 0.0f) ? 1 : -1;
        setTopNote (mTopNote + step);
    }
}
