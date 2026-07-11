#include "BaySickPitchEditor.h"
#include "BaySickVocalProcessor.h"
#include "../Standalone/BaySickTitleBar.h"
#include "../Standalone/SharedUI.h"
#include "../Standalone/StandaloneEditor.h"
#include "../TempoMapRead.h"   // LENGTH readout: seconds -> bars through the timeline
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchEditor - QA-Fa Task 2 (2026-07-10).  See header; section 14 of
// phantom-recording-mongoose is the spec.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    constexpr int kToolbarH  = 70;
    constexpr int kRulerH    = 16;
    constexpr int kKeyboardW = 40;
    constexpr int kInfoBarH  = 22;
    constexpr int kSubCurveH = 46;   // 3 stacked mini-lanes under the selected pill

    const juce::Colour kBg        = juce::Colour (0xff0e0f12);
    const juce::Colour kPanelBg   = juce::Colour (0xff16191e);
    const juce::Colour kToolbarBg = juce::Colour (0xff0d0f12);
    const juce::Colour kText      = juce::Colour (0xffd0d6dc);
    const juce::Colour kTextDim   = juce::Colour (0xff8a929c);
    const juce::Colour kDirtyDot  = juce::Colour (0xff58c067);
    const juce::Colour kStale     = juce::Colour (0xffe8a13c);

    // Section 14c / section 15 palette: pills Effects purple, waveform
    // interior Vox teal, pitch curve Bass green (the same green as the Align
    // Leader lane -- both editors use Bass green for the pitch signal).
    const juce::Colour kPillFill   = juce::Colour (0xff8a2be2);
    const juce::Colour kPillWave   = juce::Colour (0xff0fafa5);
    const juce::Colour kPitchCurve = juce::Colour (0xff33ff88);
    const juce::Colour kPlayhead   = juce::Colour (0xffe8e8e8);

    struct FactoryPitchPreset { const char* name; float focus, mod, speed; };
    constexpr FactoryPitchPreset kPitchPresets[3] = {
        { "Loose",   0.0f, 50.0f, 30.0f },
        { "Close",  50.0f, 50.0f, 50.0f },
        { "Tight", 100.0f, 20.0f, 80.0f },
    };

    const char* const kPitchPresetParamIds[] = { "bsp_focus", "bsp_mod", "bsp_speed" };

    juce::File pitchPresetsDir()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("BaySickDAW").getChildFile ("Presets")
                 .getChildFile ("BaySickPitch").getChildFile ("My Presets");
    }

    juce::String midiNoteName (int midi)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PitchKeyboard - vertical MIDI keyboard strip (section 14c KEEP)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchEditor::PitchKeyboard : public juce::Component
{
public:
    explicit PitchKeyboard (BaySickPitchEditor& o) : mOwner (o) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff14171c));
        const double laneH = mOwner.noteLaneH();
        const int top = mOwner.topNote();
        for (int note = top; note > 0; --note)
        {
            const float y = (float) ((top - note) * laneH);
            if (y > (float) getHeight()) break;
            const int pc = ((note % 12) + 12) % 12;
            const bool black = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
            g.setColour (black ? juce::Colour (0xff1a1d22) : juce::Colour (0xffd8dce0));
            g.fillRect (0.0f, y, (float) getWidth() - 1.0f, (float) laneH - 0.5f);
            if (pc == 0 && laneH >= 7.0)
            {
                g.setColour (juce::Colours::black);
                g.setFont (9.0f);
                g.drawText (midiNoteName (note), 2, (int) y, getWidth() - 6, (int) laneH,
                            juce::Justification::centredRight);
            }
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& w) override
    {
        juce::ignoreUnused (e);
        mOwner.setTopNote (mOwner.topNote() + (w.deltaY > 0 ? 2 : -2));
    }

private:
    BaySickPitchEditor& mOwner;
};

// ─────────────────────────────────────────────────────────────────────────────
// PitchCanvas - ruler + grid + pitch curve + pills + sub-curves + playhead
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchEditor::PitchCanvas : public juce::Component
{
public:
    explicit PitchCanvas (BaySickPitchEditor& o) : mOwner (o) {}

    double secForX (int x) const
    {
        return mOwner.scrollSeconds() + (double) x / mOwner.pixelsPerSecond();
    }
    int xForSec (double sec) const
    {
        return (int) std::round ((sec - mOwner.scrollSeconds()) * mOwner.pixelsPerSecond());
    }
    float yForMidi (double midi) const
    {
        return (float) (kRulerH + (mOwner.topNote() - midi) * mOwner.noteLaneH());
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBg);
        auto& pitch = mOwner.mProc.mPitch;
        const double laneH = mOwner.noteLaneH();
        const int top = mOwner.topNote();

        // Note lanes + octave lines
        for (int note = top; note > 0; --note)
        {
            const float y = yForMidi (note);
            if (y > (float) getHeight()) break;
            if (y < kRulerH) continue;
            const int pc = ((note % 12) + 12) % 12;
            const bool black = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
            if (black)
            {
                g.setColour (juce::Colours::black.withAlpha (0.25f));
                g.fillRect (0.0f, y, (float) getWidth(), (float) laneH);
            }
            if (pc == 0)
            {
                g.setColour (kTextDim.withAlpha (0.25f));
                g.drawHorizontalLine ((int) (y + laneH), 0.0f, (float) getWidth());
            }
        }

        // Ruler (time ticks)
        g.setColour (juce::Colour (0xff14171c));
        g.fillRect (0, 0, getWidth(), kRulerH);
        {
            double step = 1.0;
            while (step * mOwner.pixelsPerSecond() < 50.0)  step *= 2.0;
            while (step * mOwner.pixelsPerSecond() > 160.0) step /= 2.0;
            g.setFont (9.0f);
            double t = std::floor (mOwner.scrollSeconds() / step) * step;
            for (;; t += step)
            {
                const int x = xForSec (t);
                if (x > getWidth()) break;
                if (x < 0) continue;
                g.setColour (kTextDim.withAlpha (0.5f));
                g.drawVerticalLine (x, 2.0f, (float) kRulerH);
                g.setColour (kTextDim);
                const int mins = (int) (t / 60.0);
                g.drawText (juce::String (mins) + ":"
                              + juce::String (t - mins * 60.0, (step < 1.0) ? 1 : 0)
                                    .paddedLeft ('0', (step < 1.0) ? 4 : 2),
                            x + 2, 1, 48, kRulerH - 2, juce::Justification::centredLeft);
                g.setColour (kTextDim.withAlpha (0.12f));
                g.drawVerticalLine (x, (float) kRulerH, (float) getHeight());
            }
        }

        if (! pitch.isAnalyzed() || pitch.regions().empty())
        {
            g.setColour (kTextDim);
            g.setFont (13.0f);
            g.drawText (pitch.isAnalyzed()
                            ? "No notes detected on this channel yet"
                            : "Put audio clips on this channel - notes appear here automatically",
                        getLocalBounds(), juce::Justification::centred);
            return;
        }

        // Detected pitch curve (Bass green) from the analysis F0 track.
        {
            const auto& f0 = pitch.f0Track();
            const double hopSec = BaySickPitchDSP::kF0Hop / pitch.analysisSampleRate();
            juce::Path p;
            bool started = false;
            for (size_t f = 0; f < f0.size(); ++f)
            {
                const double t = (double) f * hopSec;
                const int x = xForSec (t);
                if (x < -4) continue;
                if (x > getWidth() + 4) break;
                if (f0[f] <= 0.0f) { started = false; continue; }
                const double midi = 12.0 * std::log2 (f0[f] / 440.0) + 69.0;
                const float y = yForMidi (midi) + (float) (laneH * 0.5);
                if (! started) { p.startNewSubPath ((float) x, y); started = true; }
                else             p.lineTo ((float) x, y);
            }
            g.setColour (kPitchCurve.withAlpha (0.75f));
            g.strokePath (p, juce::PathStrokeType (1.4f));
        }

        // Pills (rounded 5 px, purple fill, teal waveform interior) at the
        // EDITED pitch (midi + shiftSemis) so drags read immediately.
        const auto& regions = pitch.regions();
        for (int i = 0; i < (int) regions.size(); ++i)
        {
            const auto& r = regions[(size_t) i];
            const int x0 = xForSec (r.startSec);
            const int x1 = xForSec (r.endSec);
            if (x1 < 0 || x0 > getWidth()) continue;
            const float y = yForMidi ((double) r.midi + r.shiftSemis + 0.5);
            auto pill = juce::Rectangle<float> ((float) x0, y - (float) laneH * 0.5f,
                                                (float) juce::jmax (6, x1 - x0),
                                                (float) laneH * 2.0f);
            const bool sel = (i == mOwner.mSelectedRegion);
            g.setColour (kPillFill.withAlpha (sel ? 0.95f : 0.7f));
            g.fillRoundedRectangle (pill, 5.0f);
            if (sel)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.drawRoundedRectangle (pill, 5.0f, 1.4f);
            }
            drawPillWaveform (g, pill);
            if (r.hasEdits())
            {
                g.setColour (kDirtyDot);
                g.fillEllipse (pill.getX() + 3.0f, pill.getY() + 3.0f, 5.0f, 5.0f);
            }
        }

        // Sub-curves under the selected pill (always visible when selected).
        if (mOwner.mSelectedRegion >= 0
            && mOwner.mSelectedRegion < (int) regions.size())
            drawSubCurves (g, regions[(size_t) mOwner.mSelectedRegion]);

        // Playhead (FilePlay position; owner hides it when it stops moving).
        if (mOwner.mLastPlaySample >= 0)
        {
            const double tSec = (double) (mOwner.mLastPlaySample - pitch.startSample())
                                / pitch.analysisSampleRate();
            const int x = xForSec (tSec);
            if (x >= 0 && x <= getWidth())
            {
                g.setColour (kPlayhead.withAlpha (0.8f));
                g.drawVerticalLine (x, (float) kRulerH, (float) getHeight());
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& pitch = mOwner.mProc.mPitch;
        auto& regions = pitch.regions();
        const double tSec = secForX (e.x);

        // Sub-curve hit first (they float below the selected pill).
        if (mOwner.mSelectedRegion >= 0
            && mOwner.mSelectedRegion < (int) regions.size()
            && subCurveArea (regions[(size_t) mOwner.mSelectedRegion])
                   .contains (e.getPosition()))
        {
            mOwner.pushUndo();
            mDragKind = DragKind::SubCurve;
            applySubCurveDrag (e.getPosition(),
                               regions[(size_t) mOwner.mSelectedRegion]);
            repaint();
            return;
        }

        int hit = -1;
        for (int i = 0; i < (int) regions.size(); ++i)
        {
            const auto& r = regions[(size_t) i];
            const float y = yForMidi ((double) r.midi + r.shiftSemis + 0.5);
            if (tSec >= r.startSec && tSec < r.endSec
                && e.y >= y - mOwner.noteLaneH() && e.y <= y + mOwner.noteLaneH() * 2.0)
                { hit = i; break; }
        }

        if (hit < 0)
        {
            mOwner.mSelectedRegion = -1;
            mDragKind = DragKind::None;
            repaint();
            return;
        }

        mOwner.mSelectedRegion = hit;
        mOwner.updateInfoBarFor (hit);

        const bool sliceMode = ((int) mOwner.paramValue ("bsp_mode", 1.0f)) == 0;
        if (sliceMode)
        {
            // Slice: split the region at the click (both halves inherit the
            // detected fields + edits).
            auto& r = regions[(size_t) hit];
            if (tSec > r.startSec + 0.03 && tSec < r.endSec - 0.03)
            {
                mOwner.pushUndo();
                PitchNoteRegion right = r;
                right.startSec = tSec;
                r.endSec       = tSec;
                regions.insert (regions.begin() + hit + 1, right);
                pitch.publishEdits();
            }
            mDragKind = DragKind::None;
            repaint();
            return;
        }

        // Edit mode: edges resize, body = vertical pitch drag.
        mOwner.pushUndo();
        const int x0 = xForSec (regions[(size_t) hit].startSec);
        const int x1 = xForSec (regions[(size_t) hit].endSec);
        if (e.x - x0 < 6)       mDragKind = DragKind::LeftEdge;
        else if (x1 - e.x < 6)  mDragKind = DragKind::RightEdge;
        else                    mDragKind = DragKind::PitchDrag;
        mDragStartShift = regions[(size_t) hit].shiftSemis;
        mDragStartY     = e.y;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto& pitch = mOwner.mProc.mPitch;
        auto& regions = pitch.regions();
        const int sel = mOwner.mSelectedRegion;
        if (sel < 0 || sel >= (int) regions.size() || mDragKind == DragKind::None)
            return;
        auto& r = regions[(size_t) sel];
        const double tSec = secForX (e.x);

        switch (mDragKind)
        {
            case DragKind::PitchDrag:
            {
                // 1 lane = 1 semitone; 0.1 st steps.
                const double semis = mDragStartShift
                    + (mDragStartY - e.y) / mOwner.noteLaneH();
                r.shiftSemis = (float) juce::jlimit (-24.0, 24.0,
                    std::round (semis * 10.0) / 10.0);
                break;
            }
            case DragKind::LeftEdge:
                r.startSec = juce::jlimit (
                    (sel > 0) ? regions[(size_t) sel - 1].endSec : 0.0,
                    r.endSec - 0.02, tSec);
                break;
            case DragKind::RightEdge:
                r.endSec = juce::jlimit (r.startSec + 0.02,
                    (sel + 1 < (int) regions.size()) ? regions[(size_t) sel + 1].startSec
                                                     : pitch.compositeSec(),
                    tSec);
                break;
            case DragKind::SubCurve:
                applySubCurveDrag (e.getPosition(), r);
                break;
            case DragKind::None:
                break;
        }
        mOwner.updateInfoBarFor (sel);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (mDragKind != DragKind::None)
            mOwner.mProc.mPitch.publishEdits();
        mDragKind = DragKind::None;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto& regions = mOwner.mProc.mPitch.regions();
        const double tSec = secForX (e.x);
        for (int i = 0; i < (int) regions.size(); ++i)
        {
            const auto& r = regions[(size_t) i];
            const float y = yForMidi ((double) r.midi + r.shiftSemis + 0.5);
            if (tSec >= r.startSec && tSec < r.endSec
                && e.y >= y - mOwner.noteLaneH() && e.y <= y + mOwner.noteLaneH() * 2.0)
            {
                mOwner.updateInfoBarFor (i);
                return;
            }
        }
        mOwner.updateInfoBarFor (mOwner.mSelectedRegion);
    }

    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown())
        {
            const double f = (w.deltaY > 0) ? 1.2 : 1.0 / 1.2;
            mOwner.setView (mOwner.pixelsPerSecond() * f, mOwner.scrollSeconds());
        }
        else if (e.mods.isShiftDown())
        {
            mOwner.setView (mOwner.pixelsPerSecond(),
                            mOwner.scrollSeconds() - w.deltaY * 60.0 / mOwner.pixelsPerSecond());
        }
        else
            mOwner.setTopNote (mOwner.topNote() + (w.deltaY > 0 ? 2 : -2));
    }

private:
    enum class DragKind { None, PitchDrag, LeftEdge, RightEdge, SubCurve };

    void drawPillWaveform (juce::Graphics& g, juce::Rectangle<float> pill)
    {
        if (! mOwner.mCompValid) return;
        const auto& buf = mOwner.mCompCache;
        const float* src = buf.getReadPointer (0);
        const int n = buf.getNumSamples();
        const double sr = mOwner.mCompSr;
        g.setColour (kPillWave.withAlpha (0.85f));
        const float midY = pill.getCentreY();
        const float halfH = pill.getHeight() * 0.42f;
        const int px0 = juce::jmax ((int) pill.getX(), 0);
        const int px1 = juce::jmin ((int) pill.getRight(), getWidth());
        for (int x = px0; x < px1; ++x)
        {
            const juce::int64 s0 = (juce::int64) (secForX (x) * sr);
            const juce::int64 s1 = (juce::int64) (secForX (x + 1) * sr);
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
            g.drawVerticalLine (x, midY - hi * halfH, midY - lo * halfH + 1.0f);
        }
    }

    juce::Rectangle<int> subCurveArea (const PitchNoteRegion& r) const
    {
        const int x0 = xForSec (r.startSec);
        const int x1 = xForSec (r.endSec);
        const float y = yForMidi ((double) r.midi + r.shiftSemis + 0.5);
        return { x0, (int) (y + mOwner.noteLaneH() * 2.0) + 2,
                 juce::jmax (72, x1 - x0), kSubCurveH };
    }

    void drawSubCurves (juce::Graphics& g, const PitchNoteRegion& r)
    {
        auto area = subCurveArea (r);
        g.setColour (kPanelBg.withAlpha (0.92f));
        g.fillRoundedRectangle (area.toFloat(), 3.0f);

        const int laneH = kSubCurveH / 3;
        auto lane = [&] (int idx)
        {
            return area.withHeight (laneH).withY (area.getY() + idx * laneH);
        };
        g.setFont (8.0f);

        // Vibrato: filled bar = depth mult (0..2, half = natural).
        {
            auto L = lane (0).reduced (2, 1);
            g.setColour (kTextDim);
            g.drawText ("VIB", L.removeFromLeft (26), juce::Justification::centredLeft);
            g.setColour (kPitchCurve.withAlpha (0.25f));
            g.fillRect (L);
            g.setColour (kPitchCurve);
            const float w = (float) L.getWidth()
                          * juce::jlimit (0.0f, 2.0f, r.vibDepthMult) * 0.5f;
            g.fillRect ((float) L.getX(), (float) L.getY(), w, (float) L.getHeight());
        }
        // Formant: bipolar bar from center (-6..+6 st).
        {
            auto L = lane (1).reduced (2, 1);
            g.setColour (kTextDim);
            g.drawText ("FRM", L.removeFromLeft (26), juce::Justification::centredLeft);
            g.setColour (kPillWave.withAlpha (0.25f));
            g.fillRect (L);
            const float cx = (float) L.getCentreX();
            const float dx = (float) L.getWidth() * 0.5f
                           * juce::jlimit (-1.0f, 1.0f, r.formantSemis / 6.0f);
            g.setColour (kPillWave);
            g.fillRect (juce::Rectangle<float> (juce::jmin (cx, cx + dx), (float) L.getY(),
                                                std::abs (dx), (float) L.getHeight()));
        }
        // Volume: shape polyline (unity midline when empty).
        {
            auto L = lane (2).reduced (2, 1);
            g.setColour (kTextDim);
            g.drawText ("VOL", L.removeFromLeft (26), juce::Justification::centredLeft);
            g.setColour (kDirtyDot.withAlpha (0.25f));
            g.fillRect (L);
            g.setColour (kDirtyDot);
            juce::Path p;
            const int steps = juce::jmax (2, L.getWidth() / 4);
            for (int s = 0; s <= steps; ++s)
            {
                const float t01 = (float) s / (float) steps;
                const float gval = juce::jlimit (0.0f, 2.0f, r.volGainAt (t01));
                const float x = (float) L.getX() + t01 * (float) L.getWidth();
                const float y = (float) L.getBottom() - gval * 0.5f * (float) L.getHeight();
                if (s == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }
    }

    void applySubCurveDrag (juce::Point<int> pos, PitchNoteRegion& r)
    {
        auto area = subCurveArea (r);
        const int laneH = kSubCurveH / 3;
        const int lane = juce::jlimit (0, 2, (pos.y - area.getY()) / laneH);
        auto L = area.withHeight (laneH).withY (area.getY() + lane * laneH).reduced (2, 1);
        L.removeFromLeft (26);
        const float t01 = juce::jlimit (0.0f, 1.0f,
            (float) (pos.x - L.getX()) / (float) juce::jmax (1, L.getWidth()));

        if (lane == 0)
            r.vibDepthMult = juce::jlimit (0.0f, 2.0f, t01 * 2.0f);
        else if (lane == 1)
            r.formantSemis = juce::jlimit (-6.0f, 6.0f, (t01 - 0.5f) * 12.0f);
        else
        {
            // Volume: set/update a point at this t01 (merge within 5%).
            const float gval = juce::jlimit (0.0f, 2.0f,
                2.0f * (float) (L.getBottom() - pos.y) / (float) juce::jmax (1, L.getHeight()));
            bool merged = false;
            for (auto& p : r.volShape)
                if (std::abs (p.x - t01) < 0.05f) { p.y = gval; merged = true; break; }
            if (! merged)
            {
                r.volShape.push_back ({ t01, gval });
                std::sort (r.volShape.begin(), r.volShape.end(),
                           [] (const juce::Point<float>& a, const juce::Point<float>& b)
                           { return a.x < b.x; });
            }
        }
    }

    BaySickPitchEditor& mOwner;
    DragKind mDragKind       { DragKind::None };
    float    mDragStartShift { 0.0f };
    int      mDragStartY     { 0 };
};

// ─────────────────────────────────────────────────────────────────────────────
// InfoBar (section 14d): monospace Pitch / Cents / Length
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchEditor::InfoBar : public juce::Component
{
public:
    void setText (const juce::String& t)
    {
        if (t != mText) { mText = t; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (kToolbarBg);
        g.setColour (kText);
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 12.0f,
                               juce::Font::plain));
        g.drawText (mText, getLocalBounds().reduced (8, 0),
                    juce::Justification::centredLeft);
    }
private:
    juce::String mText;
};

// ─────────────────────────────────────────────────────────────────────────────
// Toolbar (section 14b)
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchEditor::Toolbar : public juce::Component
{
public:
    Toolbar (BaySickPitchEditor& o, juce::AudioProcessorValueTreeState& apvts)
        : mOwner (o), mApvts (apvts)
    {
        addAndMakeVisible (mTitleLbl);

        addAndMakeVisible (mPresetCombo);
        for (int i = 0; i < 3; ++i)
            mPresetCombo.addItem (kPitchPresets[i].name, i + 1);
        mPresetCombo.addItem ("(User)", 4);
        mPresetCombo.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
        mPresetCombo.setColour (juce::ComboBox::textColourId, kText);
        mPresetCombo.onChange = [this]
        {
            if (mMirroring) return;
            const int id = mPresetCombo.getSelectedId();
            if (id >= 1 && id <= 3)
                mOwner.applyFactoryPreset (id - 1);
        };

        auto plain = [this] (juce::TextButton& b, const juce::String& t,
                             const juce::String& tt)
        {
            b.setButtonText (t);
            b.setTooltip (tt);
            b.setColour (juce::TextButton::buttonColourId, kPanelBg);
            b.setColour (juce::TextButton::textColourOnId,  kText);
            b.setColour (juce::TextButton::textColourOffId, kText);
            addAndMakeVisible (b);
        };
        plain (mSaveBtn,   "Save",   "Save the current Focus/Mod/Speed as a user preset");
        plain (mLoadBtn,   "Load",   "Load a saved user preset");
        plain (mSliceBtn,  "Slice",  "Slice mode: click a note to split it");
        plain (mEditBtn,   "Edit",   "Edit mode: drag notes vertically (pitch) / edges (length); drag the sub-curves");
        plain (mResetBtn,  "Reset",  "Clear every pitch edit on this channel");
        plain (mRenderBtn, "Render",
               "Export the edited channel to Pitched/{name}_pitch_v{N}.wav (file only - playback is already live)");
        plain (mSendBtn,   "Send Notes to...", "Send the detected notes as MIDI to a Layers / Bass / Drums / Clips tab");
        plain (mSnapshotBtn, "Snapshot",
               "Save the current edits as a restore point in the version history");
        plain (mVersionsBtn, "Versions",
               "Revert to an earlier snapshot (every analyze also creates one)");
        plain (mUndoBtn,   "Undo",   "Undo the last note edit");
        plain (mRedoBtn,   "Redo",   "Redo");
        plain (mScrollBtn, "A",      "Auto-scroll the canvas to follow playback (A)");

        // QA-Fa recovery: bsp_on chain switch -- edits play live while ON;
        // OFF glides the applicator to neutral (never a hard switch).
        addAndMakeVisible (mOnToggle);
        mOnToggle.setButtonText ("ON");
        mOnToggle.setTooltip ("Play the pitch edits live through the chain (off glides back to the untouched take)");
        mOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            mApvts, "bsp_on", mOnToggle);
        mScrollBtn.setClickingTogglesState (true);
        mScrollBtn.setToggleState (true, juce::dontSendNotification);
        mScrollBtn.onClick = [this] { mOwner.mAutoScroll = mScrollBtn.getToggleState(); };

        mSliceBtn.setClickingTogglesState (true);
        mEditBtn .setClickingTogglesState (true);
        mSliceBtn.setRadioGroupId (0x50544D44);
        mEditBtn .setRadioGroupId (0x50544D44);
        mEditBtn .setToggleState (true, juce::dontSendNotification);
        mSliceBtn.onClick = [this] { mOwner.setParamValue ("bsp_mode", 0.0f); };
        mEditBtn .onClick = [this] { mOwner.setParamValue ("bsp_mode", 1.0f); };

        mSaveBtn    .onClick = [this] { mOwner.saveUserPreset(); };
        mLoadBtn    .onClick = [this] { mOwner.loadUserPreset(); };
        mResetBtn   .onClick = [this] { mOwner.runReset(); };
        mRenderBtn  .onClick = [this] { mOwner.runRender(); };
        mSendBtn    .onClick = [this] { mOwner.showSendNotesMenu(); };
        mSnapshotBtn.onClick = [this] { mOwner.runSnapshotVersion(); };
        mVersionsBtn.onClick = [this] { mOwner.showVersionsMenu(); };
        mUndoBtn    .onClick = [this] { mOwner.doUndo(); };
        mRedoBtn    .onClick = [this] { mOwner.doRedo(); };

        // Focus / Mod / Speed (section 14b renames), APVTS-attached.
        auto knob = [this] (juce::Slider& s, const char* id, const juce::String& tt,
                            std::unique_ptr<TaggedSliderAttachment>& att)
        {
            addAndMakeVisible (s);
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 14);
            s.setTooltip (tt);
            att = std::make_unique<TaggedSliderAttachment> (mApvts, id, s);
        };
        knob (mFocus, "bsp_focus", "How strongly note centers pull to the nearest semitone (0 = leave the take alone)", mFocusAtt);
        knob (mMod,   "bsp_mod",   "Vibrato movement: 50 = natural, above adds synthesized vibrato per note", mModAtt);
        knob (mSpeed, "bsp_speed", "How fast pitch moves between notes: low = smooth glide, high = instant", mSpeedAtt);
    }

    void setDirty (bool d) { if (mDirty != d) { mDirty = d; repaint(); } }
    void setStale (bool s, bool pending)
    {
        if (mStale != s || mStalePending != pending)
            { mStale = s; mStalePending = pending; repaint(); }
    }
    // Revert is stop-gated (owner call 2026-07-10, same rule as Align):
    // a mid-play version swap is an unbounded law change.  Snapshot stays
    // live -- it only records, never swaps the map.
    void setPlaybackGate (bool playing)
    {
        if (mPlayGated == playing) return;
        mPlayGated = playing;
        mVersionsBtn.setEnabled (! playing);
        mVersionsBtn.setTooltip (playing
            ? "Stop playback to revert"
            : "Revert to an earlier snapshot (every analyze also creates one)");
    }
    void mirrorPreset (int presetParam)
    {
        mMirroring = true;
        mPresetCombo.setSelectedId (juce::jlimit (0, 3, presetParam) + 1,
                                    juce::dontSendNotification);
        mMirroring = false;
    }
    void mirrorMode (int mode)
    {
        mSliceBtn.setToggleState (mode == 0, juce::dontSendNotification);
        mEditBtn .setToggleState (mode != 0, juce::dontSendNotification);
    }
    void setLengthText (const juce::String& t)
    {
        if (t != mLengthText) { mLengthText = t; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kToolbarBg);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        if (mDirty)
        {
            g.setColour (kDirtyDot);
            g.fillEllipse ((float) mPresetCombo.getRight() + 5.0f, 13.0f, 8.0f, 8.0f);
        }
        if (mStale)
        {
            // Pending = grid changed while the transport runs; the stop-
            // gated auto re-analyze fires at the next stop.
            g.setColour (kStale);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText (mStalePending ? "RE-ANALYZE ON STOP" : "RE-ANALYZE",
                        getWidth() - 3 * 56 - 8 - 152, 2, 148, 30,
                        juce::Justification::centredRight);
        }
        // LENGTH readout (section 14b RETAIN), monospace.
        g.setColour (kTextDim);
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 11.0f,
                               juce::Font::plain));
        g.drawText (mLengthText, 8, kToolbarH - 26, 250, 20,
                    juce::Justification::centredLeft);

        g.setFont (10.0f);
        g.drawText ("Focus", mFocus.getX(), 1, mFocus.getWidth(), 11, juce::Justification::centred);
        g.drawText ("Mod",   mMod  .getX(), 1, mMod  .getWidth(), 11, juce::Justification::centred);
        g.drawText ("Speed", mSpeed.getX(), 1, mSpeed.getWidth(), 11, juce::Justification::centred);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6, 3);

        auto knobs = b.removeFromRight (3 * 56 + 6);
        knobs.removeFromTop (11);
        mFocus.setBounds (knobs.removeFromLeft (56).reduced (2, 0));
        mMod  .setBounds (knobs.removeFromLeft (56).reduced (2, 0));
        mSpeed.setBounds (knobs.removeFromLeft (56).reduced (2, 0));

        auto row1 = b.removeFromTop (26);
        mTitleLbl.setBounds (row1.removeFromLeft (104));
        row1.removeFromLeft (6);
        mPresetCombo.setBounds (row1.removeFromLeft (100).reduced (0, 2));
        row1.removeFromLeft (16);   // dirty-dot slot
        mSaveBtn.setBounds (row1.removeFromLeft (44));
        row1.removeFromLeft (3);
        mLoadBtn.setBounds (row1.removeFromLeft (44));
        row1.removeFromLeft (10);
        mSliceBtn.setBounds (row1.removeFromLeft (48));
        row1.removeFromLeft (2);
        mEditBtn.setBounds (row1.removeFromLeft (48));
        row1.removeFromLeft (10);
        mSendBtn.setBounds (row1.removeFromLeft (108));

        auto row2 = b.withTrimmedTop (4).removeFromTop (26);
        row2.removeFromLeft (260);   // LENGTH readout slot
        mOnToggle.setBounds (row2.removeFromLeft (48));
        mScrollBtn.setBounds (row2.removeFromRight (28));
        row2.removeFromRight (4);
        mRedoBtn.setBounds (row2.removeFromRight (46));
        row2.removeFromRight (3);
        mUndoBtn.setBounds (row2.removeFromRight (46));
        row2.removeFromRight (10);
        mRenderBtn.setBounds (row2.removeFromRight (58));
        row2.removeFromRight (3);
        mResetBtn.setBounds (row2.removeFromRight (50));
        row2.removeFromRight (10);
        mVersionsBtn.setBounds (row2.removeFromRight (64));
        row2.removeFromRight (3);
        mSnapshotBtn.setBounds (row2.removeFromRight (68));
    }

private:
    BaySickPitchEditor& mOwner;
    juce::AudioProcessorValueTreeState& mApvts;

    BaySickEngineLabel mTitleLbl { "BaySickPitch", juce::Colour (0xFF0FAFA5) };
    juce::ComboBox   mPresetCombo;
    juce::TextButton mSaveBtn, mLoadBtn, mSliceBtn, mEditBtn, mResetBtn,
                     mRenderBtn, mSendBtn, mSnapshotBtn, mVersionsBtn,
                     mUndoBtn, mRedoBtn, mScrollBtn;
    juce::ToggleButton mOnToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mOnAtt;
    juce::Slider     mFocus, mMod, mSpeed;
    std::unique_ptr<TaggedSliderAttachment> mFocusAtt, mModAtt, mSpeedAtt;
    juce::String mLengthText;
    bool mDirty { false }, mStale { false }, mStalePending { false },
         mMirroring { false }, mPlayGated { false };

    friend class BaySickPitchEditor;
};

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchEditor
// ─────────────────────────────────────────────────────────────────────────────
BaySickPitchEditor::BaySickPitchEditor (BaySickVocalProcessor& p)
    : mProc (p)
{
    mToolbar  = std::make_unique<Toolbar> (*this, mProc.apvts);
    mKeyboard = std::make_unique<PitchKeyboard> (*this);
    mCanvas   = std::make_unique<PitchCanvas> (*this);
    mInfoBar  = std::make_unique<InfoBar>();

    addAndMakeVisible (*mToolbar);
    addAndMakeVisible (*mKeyboard);
    addAndMakeVisible (*mCanvas);
    addAndMakeVisible (*mInfoBar);

    setWantsKeyboardFocus (true);
    snapshotPresetValues();
    startTimer (400);
}

BaySickPitchEditor::~BaySickPitchEditor() = default;

void BaySickPitchEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
}

void BaySickPitchEditor::resized()
{
    auto b = getLocalBounds();
    mToolbar->setBounds (b.removeFromTop (kToolbarH));
    mInfoBar->setBounds (b.removeFromBottom (kInfoBarH));
    mKeyboard->setBounds (b.removeFromLeft (kKeyboardW).withTrimmedTop (kRulerH));
    mCanvas->setBounds (b);
}

void BaySickPitchEditor::visibilityChanged()
{
    // Composite auto-resolve (section 14b, Call 4a: no manual load): first
    // show, or a stale grid, re-runs the analysis.
    if (isVisible())
        runAnalyzeIfNeeded (false);
}

bool BaySickPitchEditor::keyPressed (const juce::KeyPress& k)
{
    if (k.getTextCharacter() == 'a' || k.getTextCharacter() == 'A')
    {
        mAutoScroll = ! mAutoScroll;
        mToolbar->mScrollBtn.setToggleState (mAutoScroll, juce::dontSendNotification);
        return true;
    }
    return false;
}

void BaySickPitchEditor::setView (double pps, double scrollSec)
{
    mPps    = juce::jlimit (20.0, 2000.0, pps);
    mScroll = juce::jmax (0.0, scrollSec);
    repaint();
}

void BaySickPitchEditor::setTopNote (int note)
{
    mTopNote = juce::jlimit (24, 120, note);
    mKeyboard->repaint();
    mCanvas->repaint();
}

void BaySickPitchEditor::timerCallback()
{
    const bool playing = DSPBase::isTransportPlaying();
    const bool stale = mProc.isPitchStale();
    mToolbar->setStale (stale, stale && playing);
    mToolbar->setPlaybackGate (playing);

    const bool dirty = paramsDivergeFromSnapshot();
    mToolbar->setDirty (dirty);
    if (auto* pd = mProc.apvts.getRawParameterValue ("bsp_preset_dirty"))
        if ((pd->load() > 0.5f) != dirty)
            setParamValue ("bsp_preset_dirty", dirty ? 1.0f : 0.0f);
    mToolbar->mirrorPreset ((int) paramValue ("bsp_preset", 0.0f));
    mToolbar->mirrorMode   ((int) paramValue ("bsp_mode",   1.0f));

    // LENGTH readout (section 14b): composite (or SEL) length as
    // `X bars / M:SS.f`.  Bars resolve through the tempo timeline from the
    // composite's anchor; linear 120 fallback when no map is published.
    {
        auto& pitch = mProc.mPitch;
        juce::String t;
        if (pitch.isAnalyzed() && pitch.compositeSec() > 0.0)
        {
            double secA = 0.0, secB = pitch.compositeSec();
            juce::String prefix;
            if (mSelectedRegion >= 0 && mSelectedRegion < (int) pitch.regions().size())
            {
                const auto& r = pitch.regions()[(size_t) mSelectedRegion];
                secA = r.startSec;
                secB = r.endSec;
                prefix = "SEL ";
            }
            const double sec = secB - secA;
            const double sr  = pitch.analysisSampleRate();
            double beats;
            if (TempoMap::isActive())
                beats = TempoMap::beatAtSample (pitch.startSample()
                                                + (juce::int64) std::llround (secB * sr))
                      - TempoMap::beatAtSample (pitch.startSample()
                                                + (juce::int64) std::llround (secA * sr));
            else
                beats = sec * 2.0;   // 120 BPM legacy fallback (no timeline)
            const int bars = juce::jmax (1, (int) std::ceil (beats / 4.0));
            const int mins = (int) (sec / 60.0);
            t = prefix + juce::String (bars) + " bars / "
              + juce::String (mins) + ":"
              + juce::String (sec - mins * 60.0, 1).paddedLeft ('0', 4);
        }
        mToolbar->setLengthText (t);
    }

    // Playhead from the FilePlay stamp; hide 300 ms after it stops moving.
    const juce::int64 s = mProc.getFilePlayTimelineSample();
    const juce::uint32 now = juce::Time::getMillisecondCounter();
    if (s != mLastPlaySample && mProc.mPitch.isAnalyzed())
    {
        mLastPlaySample = s;
        mLastPlayMoveMs = now;
        if (mAutoScroll)
        {
            const double tSec = (double) (s - mProc.mPitch.startSample())
                                / mProc.mPitch.analysisSampleRate();
            const double viewSec = juce::jmax (1, mCanvas->getWidth()) / mPps;
            if (tSec < mScroll || tSec > mScroll + viewSec * 0.9)
                setView (mPps, tSec - viewSec * 0.2);
        }
        mCanvas->repaint();
    }
    else if (mLastPlaySample >= 0 && now - mLastPlayMoveMs > 300)
    {
        mLastPlaySample = -1;
        mCanvas->repaint();
    }

    // [PITCH DIAG] G2 boundary (Rule 4, Remove at close): while the flag file
    // exists, the InfoBar shows the applicator gate counters so the owner can
    // read which gate eats the signal.
    if (++mDiagTick >= 5)   // flag-file poll ~every 2 s at the 400 ms timer
    {
        mDiagTick  = 0;
        const auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                             .getChildFile ("BaySickDAW");
        // Tolerate the hidden-extensions double-suffix trap.
        mDiagArmed = dir.getChildFile ("enable_pitch_diag.txt").existsAsFile()
                  || dir.getChildFile ("enable_pitch_diag.txt.txt").existsAsFile()
                  || dir.getChildFile ("enable_pitch_diag").existsAsFile();
    }
    if (mDiagArmed)
    {
        auto& d = mProc.mPitch.mDiag;
        mInfoBar->setText (
            "DIAG blk:"   + juce::String (d.blocks.load())
            + " null:"    + juce::String (d.snapNull.load())
            + " off:"     + juce::String (d.bailOff.load())
            + " neut:"    + juce::String (d.bailNeutral.load())
            + " app:"     + juce::String (d.applied.load())
            + " inReg:"   + juce::String (d.inRegion.load())
            + " regs:"    + juce::String (d.regionCount.load())
            + " tSec:"    + juce::String (d.lastTSec.load(), 2)
            + " maxSemi:" + juce::String (d.maxSemis.load(), 2)
            + " in:"      + juce::String (d.peakIn.load(), 3)
            + " out:"     + juce::String (d.peakOut.load(), 3)
            + " chg:"     + juce::String (d.changed.load()));
    }
}

void BaySickPitchEditor::updateInfoBarFor (int regionIdx)
{
    auto& regions = mProc.mPitch.regions();
    if (regionIdx < 0 || regionIdx >= (int) regions.size())
    {
        mInfoBar->setText ({});
        return;
    }
    const auto& r = regions[(size_t) regionIdx];
    const float edited = r.midi + r.shiftSemis;
    const int   nearest = (int) std::round (edited);
    const int   cents   = (int) std::round ((edited - (float) nearest) * 100.0f);
    mInfoBar->setText ("Pitch: " + midiNoteName (nearest)
                       + "  Cents: " + (cents >= 0 ? "+" : "") + juce::String (cents)
                       + "  Length: " + juce::String (r.endSec - r.startSec, 2) + "s"
                       + (r.hasEdits() ? "  [edited]" : ""));
}

// ─── Actions ──────────────────────────────────────────────────────────────────
void BaySickPitchEditor::runAnalyzeIfNeeded (bool force)
{
    if (! force && mProc.mPitch.isAnalyzed() && ! mProc.isPitchStale())
    {
        if (! mCompValid) refreshComposite();
        return;
    }
    // Analyze is stop-gated (owner call 2026-07-10): opening on a stale
    // channel mid-play lights the badge and the VoxPage poller re-runs at
    // the next transport stop instead.
    if (DSPBase::isTransportPlaying())
    {
        if (! mCompValid) refreshComposite();
        return;
    }
    juce::String err;
    if (mProc.analyzePitch (err))
    {
        refreshComposite();
        mSelectedRegion = -1;
        mCanvas->repaint();
    }
    // Silent on failure: an empty channel legitimately has nothing to
    // analyze -- the canvas empty-state carries the message.
}

void BaySickPitchEditor::refreshComposite()
{
    mCompValid = false;
    if (! mProc.onRenderComposite) return;
    double beat = 0.0, sr = 44100.0;
    juce::int64 samp = 0;
    auto buf = mProc.onRenderComposite (mProc.getOwnChannelId(), beat, samp, sr);
    if (buf.getNumSamples() <= 0) return;
    mCompCache = std::move (buf);
    mCompSr    = sr;
    mCompValid = true;
}

void BaySickPitchEditor::runRender()
{
    juce::String err;
    const auto file = mProc.renderPitchedTake (err);
    if (file == juce::File())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "Render", err);
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
            "Render", "Baked to " + file.getFileName());
}

void BaySickPitchEditor::runReset()
{
    pushUndo();
    mProc.mPitch.clearAllEdits();
    mCanvas->repaint();
}

// QA-Fa recovery (bundle item 3): versions are restore points, never an
// edit gate -- edits stay instant; analyze + this explicit action snapshot.
void BaySickPitchEditor::runSnapshotVersion()
{
    if (! mProc.mPitch.isAnalyzed()) return;
    mProc.appendPitchVersion();
}

void BaySickPitchEditor::showVersionsMenu()
{
    const auto& versions = mProc.mPitchVersions;
    if (versions.empty())
    {
        juce::PopupMenu m;
        m.addItem (1, "No versions yet - analyze or Snapshot creates one", false);
        m.showMenuAsync (juce::PopupMenu::Options());
        return;
    }

    juce::int64 curSig = 0;
    if (mProc.onChannelClipSignature && mProc.getOwnChannelId() >= 0)
        curSig = mProc.onChannelClipSignature (mProc.getOwnChannelId());

    juce::PopupMenu m;
    for (int i = (int) versions.size() - 1; i >= 0; --i)
    {
        const auto& v = versions[(size_t) i];
        juce::String label = "v" + juce::String (i + 1) + "  "
            + v.dateIso.substring (0, 16).replace ("T", " ");
        if (v.sigA != curSig)
            label += "  (grid changed)";
        m.addItem (i + 1, "Revert to " + label);
    }
    juce::Component::SafePointer<BaySickPitchEditor> self (this);
    m.showMenuAsync (juce::PopupMenu::Options(),
        [self] (int result)
        {
            if (! self || result <= 0) return;
            // Menu is modal-async: play could have started since it opened.
            if (DSPBase::isTransportPlaying()) return;
            self->pushUndo();
            if (! self->mProc.revertPitchToVersion (result - 1))
                { self->mUndoStack.pop_back(); return; }
            self->mSelectedRegion = -1;
            self->mCanvas->repaint();
        });
}

void BaySickPitchEditor::showSendNotesMenu()
{
    auto* se = findParentComponentOfClass<StandaloneEditor>();
    if (se == nullptr) return;

    const auto targets = se->listPitchNoteTargets();
    juce::PopupMenu m;
    if (targets.empty())
        m.addItem (-1, "(no Layers / Bass / Drums / Clips tabs open)", false, false);
    for (int i = 0; i < (int) targets.size(); ++i)
        m.addItem (i + 1, targets[(size_t) i].label);

    juce::Component::SafePointer<BaySickPitchEditor> self (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&mToolbar->mSendBtn),
        [self, targets] (int r)
        {
            if (! self || r <= 0 || r > (int) targets.size()) return;
            auto* se2 = self->findParentComponentOfClass<StandaloneEditor>();
            if (se2 == nullptr) return;

            // MIDI only (section 14b): the detected contour quantized to
            // notes, normalized so the first note starts the riff.
            std::vector<StandaloneEditor::ContourNote> notes;
            const auto& regions = self->mProc.mPitch.regions();
            double t0 = -1.0;
            for (const auto& reg : regions)
            {
                if (t0 < 0.0) t0 = reg.startSec;
                StandaloneEditor::ContourNote n;
                n.startSec = reg.startSec - t0;
                n.endSec   = reg.endSec   - t0;
                n.midiNote = juce::jlimit (0, 127,
                    (int) std::round (reg.midi + reg.shiftSemis));
                notes.push_back (n);
            }
            if (notes.empty()) return;
            const auto& tgt = targets[(size_t) (r - 1)];
            se2->sendPitchNotesToTab (tgt.kind, tgt.pageIndex, notes);
        });
}

// ─── Presets (section 14f) ────────────────────────────────────────────────────
void BaySickPitchEditor::applyFactoryPreset (int idx)
{
    idx = juce::jlimit (0, 2, idx);
    setParamValue ("bsp_preset", (float) idx);
    setParamValue ("bsp_focus",  kPitchPresets[idx].focus);
    setParamValue ("bsp_mod",    kPitchPresets[idx].mod);
    setParamValue ("bsp_speed",  kPitchPresets[idx].speed);
    snapshotPresetValues();
}

void BaySickPitchEditor::snapshotPresetValues()
{
    mPresetSnapshot.clear();
    for (auto* id : kPitchPresetParamIds)
        mPresetSnapshot[id] = paramValue (id);
}

bool BaySickPitchEditor::paramsDivergeFromSnapshot() const
{
    for (auto* id : kPitchPresetParamIds)
    {
        const auto it = mPresetSnapshot.find (id);
        if (it == mPresetSnapshot.end()) return true;
        if (std::abs (paramValue (id) - it->second) > 0.001f) return true;
    }
    return false;
}

void BaySickPitchEditor::saveUserPreset()
{
    auto* aw = new juce::AlertWindow ("Save Pitch Preset",
                                      "Enter a name for this Pitch preset:",
                                      juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "My Pitch");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<BaySickPitchEditor> self (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [self, aw] (int r)
        {
            if (r != 1 || ! self) return;
            const juce::String name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            juce::XmlElement el ("BaySickPitchPreset");
            for (auto* id : kPitchPresetParamIds)
                el.setAttribute (id, (double) self->paramValue (id));

            auto dir = pitchPresetsDir();
            dir.createDirectory();
            auto target = dir.getChildFile (name + ".xml");
            int n = 2;
            while (target.exists())
                target = dir.getChildFile (name + " (" + juce::String (n++) + ").xml");
            target.replaceWithText (el.toString());

            self->setParamValue ("bsp_preset", 3.0f);   // "(User)"
            self->snapshotPresetValues();
        }), true);
}

void BaySickPitchEditor::loadUserPreset()
{
    juce::PopupMenu m;
    juce::Array<juce::File> files;
    auto dir = pitchPresetsDir();
    if (dir.isDirectory())
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
            files.add (f);
    if (files.isEmpty())
        m.addItem (-1, "(no presets saved)", false, false);
    else
        for (int i = 0; i < files.size(); ++i)
            m.addItem (i + 1, files[i].getFileNameWithoutExtension());

    juce::Component::SafePointer<BaySickPitchEditor> self (this);
    m.showMenuAsync (juce::PopupMenu::Options(),
        [self, files] (int r)
        {
            if (! self || r <= 0 || r > files.size()) return;
            auto xml = juce::parseXML (files[r - 1]);
            if (xml == nullptr || ! xml->hasTagName ("BaySickPitchPreset")) return;
            for (auto* id : kPitchPresetParamIds)
                if (xml->hasAttribute (id))
                    self->setParamValue (id, (float) xml->getDoubleAttribute (id));
            self->setParamValue ("bsp_preset", 3.0f);
            self->snapshotPresetValues();
        });
}

// ─── Local edit undo ──────────────────────────────────────────────────────────
void BaySickPitchEditor::pushUndo()
{
    mUndoStack.push_back (mProc.mPitch.regions());
    if (mUndoStack.size() > 50)
        mUndoStack.erase (mUndoStack.begin());
    mRedoStack.clear();
}

void BaySickPitchEditor::doUndo()
{
    if (mUndoStack.empty()) return;
    mRedoStack.push_back (mProc.mPitch.regions());
    mProc.mPitch.regions() = mUndoStack.back();
    mUndoStack.pop_back();
    mProc.mPitch.publishEdits();
    mSelectedRegion = -1;
    mCanvas->repaint();
}

void BaySickPitchEditor::doRedo()
{
    if (mRedoStack.empty()) return;
    mUndoStack.push_back (mProc.mPitch.regions());
    mProc.mPitch.regions() = mRedoStack.back();
    mRedoStack.pop_back();
    mProc.mPitch.publishEdits();
    mSelectedRegion = -1;
    mCanvas->repaint();
}

// ─── Param helpers ────────────────────────────────────────────────────────────
float BaySickPitchEditor::paramValue (const char* id, float fallback) const
{
    if (auto* p = mProc.apvts.getRawParameterValue (id))
        return p->load();
    return fallback;
}

void BaySickPitchEditor::setParamValue (const char* id, float v)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
            mProc.apvts.getParameter (id)))
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (v));
}
