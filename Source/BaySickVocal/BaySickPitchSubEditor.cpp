#include "BaySickPitchSubEditor.h"
#include "BaySickPitchEditor.h"
#include "BaySickVocalProcessor.h"
#include "../Standalone/SharedUI.h"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchSubEditor - QA-Fd Task 7.  See header.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    const juce::Colour kBg       = juce::Colour (0xf2101318);   // slight translucency
    const juce::Colour kPanelBg  = juce::Colour (0xff16191e);
    const juce::Colour kText     = juce::Colour (0xffd0d6dc);
    const juce::Colour kTextDim  = juce::Colour (0xff8a929c);
    const juce::Colour kWave     = juce::Colour (0xff0fafa5);
    const juce::Colour kVolCol   = juce::Colour (0xff58c067);
    const juce::Colour kPitchCol = juce::Colour (0xffb06ae8);

    juce::String midiNoteName (int midi)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Lane - one editable shape (volume or pitch) over the pill waveform ghost.
// EventEditor gesture set: click empty = add, drag = move, right-click = del.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchSubEditor::Lane : public juce::Component
{
public:
    explicit Lane (BaySickPitchSubEditor& o) : mOwner (o) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kPanelBg);
        auto* r = mOwner.region();
        if (r == nullptr)
        {
            g.setColour (kTextDim);
            g.drawText ("(pill gone - re-analyze?)", getLocalBounds(),
                        juce::Justification::centred);
            return;
        }

        // Pill waveform ghost (source span, post-gain so the volume shape
        // previews its own effect).
        drawWaveGhost (g, *r);

        const bool pitchLane = mOwner.mShowPitchLane;
        auto& pts = pitchLane ? r->pitchShape : r->volShape;

        // Midline (unity gain / 0 st)
        g.setColour (kTextDim.withAlpha (0.4f));
        const float midY = yForVal (pitchLane ? 0.0f : 1.0f);
        g.drawHorizontalLine ((int) midY, 0.0f, (float) getWidth());

        // Shape polyline sampled through the same lerp the applicator uses.
        g.setColour (pitchLane ? kPitchCol : kVolCol);
        juce::Path path;
        const int steps = juce::jmax (2, getWidth() / 3);
        for (int s = 0; s <= steps; ++s)
        {
            const float t01 = (float) s / (float) steps;
            const float v = pitchLane ? r->pitchSemisAt (t01) : r->volGainAt (t01);
            const float x = t01 * (float) getWidth();
            const float y = yForVal (v);
            if (s == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        g.strokePath (path, juce::PathStrokeType (1.6f));

        // Points
        for (const auto& p : pts)
        {
            const float x = p.x * (float) getWidth();
            const float y = yForVal (p.y);
            g.setColour (juce::Colours::black);
            g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
            g.setColour (pitchLane ? kPitchCol : kVolCol);
            g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
        }

        // Scale captions
        g.setColour (kTextDim);
        g.setFont (9.0f);
        if (pitchLane)
        {
            g.drawText ("+12 st", 4, 2, 60, 12, juce::Justification::centredLeft);
            g.drawText ("-12 st", 4, getHeight() - 14, 60, 12, juce::Justification::centredLeft);
        }
        else
        {
            g.drawText ("x2", 4, 2, 40, 12, juce::Justification::centredLeft);
            g.drawText ("x0", 4, getHeight() - 14, 40, 12, juce::Justification::centredLeft);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto* r = mOwner.region();
        if (r == nullptr) return;
        auto& pts = mOwner.mShowPitchLane ? r->pitchShape : r->volShape;
        const int hit = hitTest (pts, e.getPosition());

        if (e.mods.isPopupMenu())
        {
            if (hit >= 0)
            {
                mOwner.beginLaneEdit();
                pts.erase (pts.begin() + hit);
                mOwner.commitLaneEdit ("Pitch: Delete Curve Point");
                repaint();
            }
            return;
        }

        mOwner.beginLaneEdit();
        if (hit >= 0)
            mDragIdx = hit;
        else
        {
            pts.push_back (pointForPos (e.getPosition()));
            sortPts (pts);
            mDragIdx = hitTest (pts, e.getPosition());
        }
        mDragged = false;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* r = mOwner.region();
        if (r == nullptr || mDragIdx < 0) return;
        auto& pts = mOwner.mShowPitchLane ? r->pitchShape : r->volShape;
        if (mDragIdx >= (int) pts.size()) return;
        pts[(size_t) mDragIdx] = pointForPos (e.getPosition());
        sortPts (pts);
        mDragIdx = hitTest (pts, e.getPosition());
        mDragged = true;
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (mDragIdx >= 0)
            mOwner.commitLaneEdit (mDragged ? "Pitch: Move Curve Point"
                                            : "Pitch: Add Curve Point");
        mDragIdx = -1;
    }

private:
    float yForVal (float v) const
    {
        if (mOwner.mShowPitchLane)
            return juce::jmap (juce::jlimit (-12.0f, 12.0f, v),
                               -12.0f, 12.0f, (float) getHeight() - 4.0f, 4.0f);
        return juce::jmap (juce::jlimit (0.0f, 2.0f, v),
                           0.0f, 2.0f, (float) getHeight() - 4.0f, 4.0f);
    }
    float valForY (int y) const
    {
        if (mOwner.mShowPitchLane)
            return juce::jmap ((float) juce::jlimit (4, getHeight() - 4, y),
                               (float) getHeight() - 4.0f, 4.0f, -12.0f, 12.0f);
        return juce::jmap ((float) juce::jlimit (4, getHeight() - 4, y),
                           (float) getHeight() - 4.0f, 4.0f, 0.0f, 2.0f);
    }
    juce::Point<float> pointForPos (juce::Point<int> pos) const
    {
        return { juce::jlimit (0.0f, 1.0f,
                               (float) pos.x / juce::jmax (1.0f, (float) getWidth())),
                 valForY (pos.y) };
    }
    int hitTest (const std::vector<juce::Point<float>>& pts,
                 juce::Point<int> pos) const
    {
        for (int i = 0; i < (int) pts.size(); ++i)
        {
            const float x = pts[(size_t) i].x * (float) getWidth();
            const float y = yForVal (pts[(size_t) i].y);
            if (std::abs (x - (float) pos.x) < 6.0f
                && std::abs (y - (float) pos.y) < 6.0f)
                return i;
        }
        return -1;
    }
    static void sortPts (std::vector<juce::Point<float>>& pts)
    {
        std::sort (pts.begin(), pts.end(),
                   [] (const juce::Point<float>& a, const juce::Point<float>& b)
                   { return a.x < b.x; });
    }

    void drawWaveGhost (juce::Graphics& g, const PitchNoteRegion& r)
    {
        const auto* bufPtr = mOwner.compCache();
        if (bufPtr == nullptr) return;
        const auto& buf = *bufPtr;
        const float* src = buf.getReadPointer (0);
        const int n = buf.getNumSamples();
        const double sr = mOwner.compSr();
        const double srcLen = r.endSec - r.startSec;
        if (srcLen <= 0.0) return;
        g.setColour (kWave.withAlpha (0.22f));
        const float midY = (float) getHeight() * 0.5f;
        const float halfH = (float) getHeight() * 0.42f;
        for (int x = 0; x < getWidth(); ++x)
        {
            const float t01a = (float) x / (float) getWidth();
            const float t01b = (float) (x + 1) / (float) getWidth();
            const juce::int64 s0 = (juce::int64) ((r.startSec + t01a * srcLen) * sr);
            const juce::int64 s1 = (juce::int64) ((r.startSec + t01b * srcLen) * sr);
            if (s0 >= n || s1 <= 0) continue;
            const juce::int64 a = juce::jmax ((juce::int64) 0, s0);
            const juce::int64 z = juce::jmin ((juce::int64) n, juce::jmax (s0 + 1, s1));
            const juce::int64 stride = juce::jmax ((juce::int64) 1, (z - a) / 32);
            float lo = 0.0f, hi = 0.0f;
            for (juce::int64 s = a; s < z; s += stride)
            {
                lo = juce::jmin (lo, src[s]);
                hi = juce::jmax (hi, src[s]);
            }
            const float gain = juce::jlimit (0.0f, 2.0f, r.volGainAt (t01a));
            g.drawVerticalLine (x, midY - hi * gain * halfH,
                                   midY - lo * gain * halfH + 1.0f);
        }
    }

    BaySickPitchSubEditor& mOwner;
    int  mDragIdx { -1 };
    bool mDragged { false };
};

// ─────────────────────────────────────────────────────────────────────────────
// Browser - pills in order (selection-filtered); click switches the popup.
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchSubEditor::Browser : public juce::Component
{
public:
    explicit Browser (BaySickPitchSubEditor& o) : mOwner (o) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kPanelBg.darker (0.2f));
        g.setColour (kTextDim);
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText ("PILLS", 6, 2, getWidth() - 12, 14, juce::Justification::centredLeft);

        const auto& regions = mOwner.regionsRef();
        int y = 18;
        for (int idx : mOwner.mBrowserIdxs)
        {
            if (idx < 0 || idx >= (int) regions.size()) continue;
            const auto& r = regions[(size_t) idx];
            auto row = juce::Rectangle<int> (2, y, getWidth() - 4, kRowH - 2);
            const bool active = (idx == mOwner.mRegionIdx);
            g.setColour (active ? kWave.withAlpha (0.3f) : kPanelBg);
            g.fillRoundedRectangle (row.toFloat(), 3.0f);
            g.setColour (active ? kText : kTextDim);
            g.setFont (10.0f);
            const juce::String label = r.isSlice
                ? "Slice " + juce::String (r.startSec, 2) + "s"
                : midiNoteName ((int) std::round (r.midi + r.shiftSemis))
                    + "  " + juce::String (r.startSec, 2) + "s";
            g.drawText (label + (r.hasEdits() ? " *" : ""),
                        row.reduced (6, 0), juce::Justification::centredLeft);
            y += kRowH;
            if (y > getHeight()) break;
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int slot = (e.y - 18) / kRowH;
        if (slot < 0 || slot >= (int) mOwner.mBrowserIdxs.size()) return;
        mOwner.switchToPill (mOwner.mBrowserIdxs[(size_t) slot]);
    }

private:
    static constexpr int kRowH = 20;
    BaySickPitchSubEditor& mOwner;
};

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchSubEditor
// ─────────────────────────────────────────────────────────────────────────────
BaySickPitchSubEditor::BaySickPitchSubEditor (BaySickPitchEditor& owner)
    : mOwner (owner)
{
    mLane    = std::make_unique<Lane> (*this);
    mBrowser = std::make_unique<Browser> (*this);
    addAndMakeVisible (*mLane);
    addAndMakeVisible (*mBrowser);

    auto lane = [this] (juce::TextButton& b, const juce::String& t, bool pitch)
    {
        b.setButtonText (t);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (0x50495453);
        b.setColour (juce::TextButton::buttonColourId,   kPanelBg);
        b.setColour (juce::TextButton::buttonOnColourId, kPanelBg.brighter (0.3f));
        b.setColour (juce::TextButton::textColourOnId,   kText);
        b.setColour (juce::TextButton::textColourOffId,  kTextDim);
        b.onClick = [this, pitch]
        {
            mShowPitchLane = pitch;
            mLane->repaint();
        };
        addAndMakeVisible (b);
    };
    lane (mVolBtn,   "Volume", false);
    lane (mPitchBtn, "Pitch",  true);
    mVolBtn.setToggleState (true, juce::dontSendNotification);
    mVolBtn.setTooltip ("Edit the pill's volume curve (gain around unity)");
    mPitchBtn.setTooltip ("Edit the pill's pitch curve (additive semitones - "
                          "audibly bends pitch inside the note)");

    addAndMakeVisible (mPlayBtn);
    mPlayBtn.setButtonText ("Play");
    mPlayBtn.setTooltip ("Preview this pill through the current edits (SPACE)");
    mPlayBtn.onClick = [this] { togglePlay(); };

    // Per-pill knobs: bipolar Vib / Frm + the Variation knob (15a).  Direct
    // region writes (no APVTS -- per-pill state); each drag = one undo step.
    auto applyKnobs = [this]
    {
        if (mSyncingKnobs) return;
        if (region() == nullptr) return;
        // Typed entries / detent snaps arrive outside a drag gesture -- wrap
        // them in their own undo step so every path lands on the stack (9a).
        const bool wrap = ! mOwner.mEditPending;
        if (wrap) beginLaneEdit();
        if (auto* r = region())
        {
            r->vibDepthMult = (float) mVibKnob.getValue();
            r->formantSemis = (float) mFrmKnob.getValue();
            r->variation    = (float) mVarKnob.getValue();
        }
        if (wrap) commitLaneEdit ("Pitch: Sub-Edit Knob");
        else
        {
            // Mid-gesture: publish live so main-transport playback tracks
            // the knob during the drag (undo still lands once at drag end).
            mOwner.mProc.mPitch.publishEdits();
            mLane->repaint();
        }
    };
    auto knob = [this, &applyKnobs] (juce::Slider& s, double lo, double hi,
                                     double def, const juce::String& tt)
    {
        addAndMakeVisible (s);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 14);
        s.setRange (lo, hi, 0.01);
        s.setValue (def, juce::dontSendNotification);
        s.setTooltip (tt);
        s.onDragStart = [this] { beginLaneEdit(); };
        s.onDragEnd   = [this] { commitLaneEdit ("Pitch: Sub-Edit Knob"); };
        s.onValueChange = applyKnobs;
        // Wraps the handler above (detent/fine/type-in) -- call order matters.
        applyFLKnobFeel (s, def);
    };
    knob (mVibKnob, 0.0, 2.0, 1.0,
          "Vibrato depth for this pill: 1 = natural, below flattens the added "
          "vibrato, above deepens it");
    knob (mFrmKnob, -6.0, 6.0, 0.0,
          "Formant shift for this pill, in semitones (bipolar)");
    knob (mVarKnob, 0.0, 2.0, 1.0,
          "Variation: scales this note's own pitch wiggle around its center - "
          "0 flattens the natural movement, 2 exaggerates it");

    setWantsKeyboardFocus (true);
}

BaySickPitchSubEditor::~BaySickPitchSubEditor() = default;

PitchNoteRegion* BaySickPitchSubEditor::region() const
{
    auto& regions = mOwner.mProc.mPitch.regions();
    if (mRegionIdx < 0 || mRegionIdx >= (int) regions.size()) return nullptr;
    return &regions[(size_t) mRegionIdx];
}

std::vector<PitchNoteRegion>& BaySickPitchSubEditor::regionsRef() const
{
    return mOwner.mProc.mPitch.regions();
}

const juce::AudioBuffer<float>* BaySickPitchSubEditor::compCache() const
{
    return mOwner.mCompValid ? &mOwner.mCompCache : nullptr;
}

double BaySickPitchSubEditor::compSr() const { return mOwner.mCompSr; }

void BaySickPitchSubEditor::switchToPill (int idx)
{
    const auto& regions = mOwner.mProc.mPitch.regions();
    if (idx < 0 || idx >= (int) regions.size() || regions[(size_t) idx].isSlice)
        return;
    mRegionIdx = idx;
    mOwner.selectOnly (idx);
    refreshFromRegion();
    repaint();
}

void BaySickPitchSubEditor::openFor (int regionIdx, std::vector<int> browserIdxs)
{
    mRegionIdx   = regionIdx;
    mBrowserIdxs = std::move (browserIdxs);
    if ((int) mBrowserIdxs.size() <= 1)
    {
        // Single entry: list every pill (the browser filters only when the
        // popup was entered from a multi-select).
        mBrowserIdxs.clear();
        for (int i = 0; i < (int) mOwner.mProc.mPitch.regions().size(); ++i)
            mBrowserIdxs.push_back (i);
    }
    setVisible (true);
    refreshFromRegion();
    grabKeyboardFocus();
}

void BaySickPitchSubEditor::refreshFromRegion()
{
    auto* r = region();
    mSyncingKnobs = true;
    if (r != nullptr)
    {
        mVibKnob.setValue (r->vibDepthMult, juce::dontSendNotification);
        mFrmKnob.setValue (r->formantSemis, juce::dontSendNotification);
        mVarKnob.setValue (r->variation,    juce::dontSendNotification);
        // QA-Layout T4: injected callback replaces the
        // findParentComponentOfClass<DocumentWindow> escape (hosting-agnostic).
        if (onTitleChanged)
            onTitleChanged ("Pitch Sub-Editor - "
                + midiNoteName ((int) std::round (r->midi + r->shiftSemis))
                + "  (" + juce::String (r->endSec - r->startSec, 2) + "s)");
    }
    mSyncingKnobs = false;
    mLane->repaint();
    mBrowser->repaint();
}

void BaySickPitchSubEditor::togglePlay()
{
    if (mOwner.mProc.isPitchPreviewPlaying())
        mOwner.stopPreview();
    else if (mRegionIdx >= 0 && ! DSPBase::isTransportPlaying())
    {
        // Preview is a stopped-transport feature (it would sum over the mix).
        mOwner.mProc.mPitch.publishEdits();
        mOwner.startRegionPreview (mRegionIdx, false);
    }
}

bool BaySickPitchSubEditor::keyPressed (const juce::KeyPress& k)
{
    if (k.isKeyCode (juce::KeyPress::spaceKey))
    {
        togglePlay();
        return true;
    }
    if (k.isKeyCode (juce::KeyPress::escapeKey))
    {
        mOwner.closeSubEditor();
        return true;
    }
    return false;
}

void BaySickPitchSubEditor::visibilityChanged()
{
    if (! isVisible())
        mOwner.stopPreview();
}

void BaySickPitchSubEditor::paint (juce::Graphics& g)
{
    // Chrome (title bar / frame) is the host DocumentWindow's job now --
    // the content just fills its panel color, matching EventEditorContent.
    g.fillAll (kBg);

    g.setColour (kTextDim);
    g.setFont (10.0f);
    g.drawText ("Vib",       mVibKnob.getX(), mVibKnob.getY() - 12, mVibKnob.getWidth(), 12, juce::Justification::centred);
    g.drawText ("Frm",       mFrmKnob.getX(), mFrmKnob.getY() - 12, mFrmKnob.getWidth(), 12, juce::Justification::centred);
    g.drawText ("Variation", mVarKnob.getX() - 8, mVarKnob.getY() - 12, mVarKnob.getWidth() + 16, 12, juce::Justification::centred);
}

void BaySickPitchSubEditor::resized()
{
    auto b = getLocalBounds().reduced (10);

    auto top = b.removeFromTop (26);
    mPlayBtn.setBounds (top.removeFromRight (56));
    top.removeFromRight (10);
    mPitchBtn.setBounds (top.removeFromRight (56));
    top.removeFromRight (2);
    mVolBtn.setBounds (top.removeFromRight (60));

    auto knobRow = b.removeFromTop (76);
    knobRow.removeFromTop (12);   // captions
    mVibKnob.setBounds (knobRow.removeFromLeft (60));
    knobRow.removeFromLeft (8);
    mFrmKnob.setBounds (knobRow.removeFromLeft (60));
    knobRow.removeFromLeft (8);
    mVarKnob.setBounds (knobRow.removeFromLeft (60));

    b.removeFromTop (6);
    mBrowser->setBounds (b.removeFromRight (130));
    b.removeFromRight (6);
    mLane->setBounds (b);
}

// The lane + knobs route undo through the owner (one action per gesture).
void BaySickPitchSubEditor::beginLaneEdit()  { mOwner.beginEdit(); }
void BaySickPitchSubEditor::commitLaneEdit (const juce::String& label)
{
    mOwner.commitEdit (label);
    mLane->repaint();
    mBrowser->repaint();
    mOwner.repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchSubEditorWindow -- Event Editor's floating-window conventions
// verbatim (same title-bar color, resizable, screen-centered) so every popup
// editor in the DAW shares one box style (owner call, 2026-07-11).
// ─────────────────────────────────────────────────────────────────────────────
BaySickPitchSubEditorWindow::BaySickPitchSubEditorWindow (BaySickPitchEditor& owner)
    : juce::DocumentWindow ("Pitch Sub-Editor", juce::Colour (0xff1a1c1e),
                            juce::DocumentWindow::allButtons),
      mOwner (owner)
{
    auto content = std::make_unique<BaySickPitchSubEditor> (owner);
    content->setSize (760, 430);
    content->onTitleChanged = [this] (const juce::String& t) { setName (t); };
    setContentOwned (content.release(), true);
    setResizable (true, true);
    setResizeLimits (560, 320, 1600, 900);

    centreWithSize (760, 430);
    setVisible (true);
    setAlwaysOnTop (false);
    toFront (true);
}

BaySickPitchSubEditor* BaySickPitchSubEditorWindow::content()
{
    return dynamic_cast<BaySickPitchSubEditor*> (getContentComponent());
}

void BaySickPitchSubEditorWindow::closeButtonPressed()
{
    // The owner resets its unique_ptr -> deletes this window; no member
    // access after the call (EventEditor's close pattern).
    mOwner.closeSubEditor();
}
