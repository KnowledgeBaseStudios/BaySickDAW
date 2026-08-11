#include "BaySickPitchEditor.h"
#include "SafeXml.h"   // XXE + depth-guarded XML parse (QA-Cleanup)
#include "../AppPaths.h"
#include "../UserFileSave.h"
#include "BaySickPitchSubEditor.h"
#include "BaySickVocalProcessor.h"
#include "../Standalone/UndoBracket.h"
#include "../DSP/PitchCorrectorDSP.h"   // shared 13-scale table
#include "../Standalone/BaySickTitleBar.h"
#include "../Standalone/SharedUI.h"
#include "../Standalone/StandaloneEditor.h"
#include "../TempoMapRead.h"   // LENGTH readout + playhead beat->sample
#include "../BaySickConstants.h"   // shared piano-roll zoom baselines (single source of truth)
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchEditor - QA-Fd rework (2026-07-11).  See header; the
// snug-orbiting-catmull plan (dockets 1-20 + parity 1-14) is the spec.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    constexpr int kToolbarH   = 70;
    constexpr int kRulerH     = 16;
    constexpr int kKeyboardW  = 40;
    constexpr int kInfoBarH   = 22;
    constexpr int kBoxH       = 64;   // display-only sub-edit box (4 bars)
    constexpr int kBoxBtnH    = 14;
    constexpr int kMinNote    = 12;   // C0 floor (view + lanes never go below)
    constexpr int kHandleSz   = 8;    // on-pill corner handles
    // Design E: a move/stretch may not squash or stretch a gap (or a note) past
    // this ratio -- beyond it the time-warp vocoder rails (Hs -> kFFTSize, overlap
    // collapses) into spikes + level drop.  Detach bypasses it (free hard-cut).
    constexpr double kMaxWarpRatio = 3.0;

    const juce::Colour kBg        = juce::Colour (0xff0e0f12);
    const juce::Colour kPanelBg   = juce::Colour (0xff16191e);
    const juce::Colour kToolbarBg = juce::Colour (0xff0d0f12);
    const juce::Colour kText      = juce::Colour (0xffd0d6dc);
    const juce::Colour kTextDim   = juce::Colour (0xff8a929c);
    const juce::Colour kDirtyDot  = juce::Colour (0xff58c067);
    const juce::Colour kStale     = juce::Colour (0xffe8a13c);

    // Pills Effects purple, waveform interior Vox teal, pitch curve Bass
    // green (as-sung reference -- stays put under every edit); slice pieces
    // neutral gray; handles green=in / red=out (Newtone figure convention).
    const juce::Colour kPillFill   = juce::Colour (0xff8a2be2);
    const juce::Colour kPillWave   = juce::Colour (0xff0fafa5);
    const juce::Colour kPitchCurve = juce::Colour (0xff33ff88);
    const juce::Colour kPlayhead   = juce::Colour (0xffe8e8e8);
    const juce::Colour kSliceFill  = juce::Colour (0xff5a6068);
    const juce::Colour kHandleIn   = juce::Colour (0xff58c067);
    const juce::Colour kHandleOut  = juce::Colour (0xffe05555);
    const juce::Colour kDetachCol  = juce::Colour (0xffe8a13c);

    juce::File pitchPresetsDir()
    {
        return AppPaths::appRoot().getChildFile ("Presets")
                 .getChildFile ("BaySickPitch").getChildFile ("My Presets");
    }

    // App-wide UI prefs (the multi-select reset never-show-again flag).
    // Lives beside settings.xml in Documents/BaySickDAW per the standing
    // artifact-location convention.
    std::unique_ptr<juce::PropertiesFile> openUiPrefs()
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "BaySickDAW";
        o.filenameSuffix      = "xml";
        o.folderName          = "BaySickDAW";
        juce::File dir = AppPaths::appRoot();
        return std::make_unique<juce::PropertiesFile> (
            dir.getChildFile ("ui_prefs.xml"), o);
    }

    juce::String midiNoteName (int midi)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PitchKeyboard - vertical MIDI keyboard strip.  QA-Fd: clicking a key with a
// selection batch re-pitches the selected pills onto that note (the Newtone
// ruler-click parity item); holding previews the focused pill at its new
// pitch while the transport is stopped.
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
        for (int note = top; note >= kMinNote; --note)
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

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int note = juce::jlimit (kMinNote, 120,
            mOwner.topNote() - (int) std::floor (e.y / mOwner.noteLaneH()));
        if (! mOwner.mSelection.empty())
        {
            mOwner.batchRepitchTo (note);
            if (! DSPBase::isTransportPlaying() && mOwner.mFocusRegion >= 0)
                mOwner.startRegionPreview (mOwner.mFocusRegion, false);
        }
    }

    void mouseUp (const juce::MouseEvent&) override { mOwner.stopPreview(); }

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
// PitchCanvas - ruler + grid + pitch curve + pills + display box + playhead.
// Owns every canvas gesture (QA-Fd motion model).
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
    // Composite-seconds <-> project beats (bars/beats grid + Slice snap).  Mirrors
    // the playhead/length mapping: the composite sits at startSample (analysis
    // rate) on the project timeline.  120 BPM fallback when no tempo map.
    double beatForSec (double s) const
    {
        auto& pitch = mOwner.mProc.mPitch;
        const double sr = juce::jmax (1.0, pitch.analysisSampleRate());
        return TempoMap::isActive()
            ? TempoMap::beatAtSample (pitch.startSample()
                                      + (juce::int64) std::llround (s * sr))
            : s * 2.0;
    }
    double secForBeat (double b) const
    {
        auto& pitch = mOwner.mProc.mPitch;
        const double sr = juce::jmax (1.0, pitch.analysisSampleRate());
        return TempoMap::isActive()
            ? (double) (TempoMap::sampleAtBeat (b) - pitch.startSample()) / sr
            : b * 0.5;
    }
    // Visible grid resolution in beats -- coarsens when zoomed out, subdivides
    // (1/2, 1/4 beat) when zoomed in.  Shared by the ruler draw and Slice snap.
    double gridBeatStep() const
    {
        const double pxPerBeat = juce::jmax (1.0e-6,
            (double) (xForSec (secForBeat (1.0)) - xForSec (secForBeat (0.0))));
        double step = 1.0;
        if (pxPerBeat >= 24.0) { while (step * pxPerBeat > 24.0) step *= 0.5; }
        else                   { while (step * pxPerBeat < 12.0) step *= 2.0; }
        return step;
    }
    float yForMidi (double midi) const
    {
        return (float) (kRulerH + (mOwner.topNote() - midi) * mOwner.noteLaneH());
    }
    double midiForY (int y) const
    {
        return mOwner.topNote() - (double) (y - kRulerH) / mOwner.noteLaneH();
    }

    // Snap a composite-second to the 96-tick grid at the visible resolution --
    // matches the piano roll's snapBeat (kTicksPerBeat = 96; div = the visible grid
    // step in ticks; tick-exact rounding so triplet grids don't drift).  Used for
    // the time SELECTION only; the bare-click seek stays free (piano-roll parity).
    double snapSecToGrid (double sec) const
    {
        const double beat     = beatForSec (sec);
        const int    divTicks = juce::jmax (1, (int) std::llround (gridBeatStep() * 96.0));
        const double snapped  = std::round (beat * 96.0 / (double) divTicks)
                                 * (double) divTicks / 96.0;
        return secForBeat (snapped);
    }

    // Ruler range <-> builder-grid SONG time selection.  Push: composite seconds
    // -> project beats -> bars (4 beats/bar, matching onGetLoopBeats); endBar <=
    // startBar clears.  Sync: read the builder selection back into the local draw
    // range (skipped mid-drag so the live gesture stays responsive).
    void pushRulerSel()
    {
        if (! mOwner.mProc.onSetSongTimeSel) return;
        if (mRulerSelStart >= 0.0 && mRulerSelEnd > mRulerSelStart)
            mOwner.mProc.onSetSongTimeSel ((float) (beatForSec (mRulerSelStart) / 4.0),
                                           (float) (beatForSec (mRulerSelEnd)   / 4.0));
        else
            mOwner.mProc.onSetSongTimeSel (-1.0f, -1.0f);
    }
    void syncRulerSelFromBuilder()
    {
        if (mRulerDragging || ! mOwner.mProc.onGetSongTimeSel) return;
        float sBar = 0.0f, eBar = 0.0f;
        if (mOwner.mProc.onGetSongTimeSel (sBar, eBar) && eBar > sBar)
        { mRulerSelStart = secForBeat (sBar * 4.0); mRulerSelEnd = secForBeat (eBar * 4.0); }
        else
        { mRulerSelStart = mRulerSelEnd = -1.0; }
        repaint();
    }

    // Rendered pitch (locked 8): detected + drag + Center/Focus pull toward the
    // nearest semitone center.  The green curve stays as-sung; the PILL shows
    // what you will hear.  Must mirror the DSP Center pull exactly (nearest
    // semitone, scale-independent) or the pill would display a different target
    // than it sounds.
    float drawMidiFor (const PitchNoteRegion& r) const
    {
        if (r.isSlice) return r.midi;   // unpitched block: fixed lane, no Focus pull
        const float base   = r.midi + r.shiftSemis;
        const float focus  = mOwner.paramValue ("bsp_focus") / 100.0f;
        const float target = std::round (base);
        return base + (target - base) * juce::jlimit (0.0f, 1.0f, focus);
    }

    juce::Rectangle<float> pillRect (const PitchNoteRegion& r) const
    {
        const int x0 = xForSec (r.dstStart());
        const int x1 = xForSec (r.dstEnd());
        const float w = (float) juce::jmax (6, x1 - x0);
        // Slice pieces render inline at their sibling's pitch row -- a slice
        // inherits the note's midi, so consonant+vowel read as one unit.
        const double laneH = mOwner.noteLaneH();
        const float y = yForMidi ((double) drawMidiFor (r) + 0.5);
        return { (float) x0, y - (float) laneH * 0.5f, w, (float) laneH * 2.0f };
    }

    juce::Rectangle<int> displayBoxArea (const PitchNoteRegion& r) const
    {
        auto pill = pillRect (r);
        const int x0 = (int) pill.getX();
        return { x0, (int) pill.getBottom() + 4,
                 juce::jmax (150, (int) pill.getWidth()), kBoxH };
    }
    juce::Rectangle<int> boxEditBtn (const juce::Rectangle<int>& box) const
    {
        return { box.getX() + 2, box.getY() + 2, 34, kBoxBtnH };
    }
    juce::Rectangle<int> boxResetBtn (const juce::Rectangle<int>& box) const
    {
        return { box.getRight() - 44, box.getY() + 2, 42, kBoxBtnH };
    }

    // On-pill corner handles (locked 19a; selected pill only, hidden below
    // the zoom threshold).  0=fade-in 1=fade-out 2=approach 3=release.
    bool handlesVisible (const PitchNoteRegion& r) const
    {
        return ! r.isSlice && mOwner.noteLaneH() >= 8.0
            && pillRect (r).getWidth() >= 36.0f;
    }
    juce::Rectangle<float> handleRect (const PitchNoteRegion& r, int which) const
    {
        auto p = pillRect (r);
        const float s = (float) kHandleSz;
        switch (which)
        {
            case 0:  return { p.getX(),         p.getY(),            s, s };
            case 1:  return { p.getRight() - s, p.getY(),            s, s };
            case 2:  return { p.getX(),         p.getBottom() - s,   s, s };
            default: return { p.getRight() - s, p.getBottom() - s,   s, s };
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBg);
        auto& pitch = mOwner.mProc.mPitch;
        const double laneH = mOwner.noteLaneH();
        const int top = mOwner.topNote();

        // Note lanes + octave lines (floored at C0 -- #6 bottom clamp).
        for (int note = top; note >= kMinNote; --note)
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

        // Ruler + grid (bars/beats -- NewTone / FL piano-roll style).  Bar lines
        // major + numbered, beat lines minor, sub-beats when zoomed in.
        g.setColour (juce::Colour (0xff14171c));
        g.fillRect (0, 0, getWidth(), kRulerH);
        {
            constexpr int beatsPerBar = 4;   // 4/4 (app default; TODO time-sig read)
            const double beatStep = gridBeatStep();

            g.setFont (9.0f);
            double b = std::floor (beatForSec (mOwner.scrollSeconds()) / beatStep) * beatStep;
            for (;; b += beatStep)
            {
                const int x = xForSec (secForBeat (b));
                if (x > getWidth()) break;
                if (x < 0) continue;
                const bool onBeat = std::abs (b - std::round (b)) < 1.0e-6;
                const bool onBar  = onBeat && (((int) std::llround (b)) % beatsPerBar == 0);
                g.setColour (kTextDim.withAlpha (onBar ? 0.28f : onBeat ? 0.13f : 0.06f));
                g.drawVerticalLine (x, (float) kRulerH, (float) getHeight());
                g.setColour (kTextDim.withAlpha (onBar ? 0.8f : 0.4f));
                g.drawVerticalLine (x, onBar ? 1.0f : 6.0f, (float) kRulerH);
                if (onBar)
                {
                    const int bar = (int) std::llround (b) / beatsPerBar + 1;   // 1-based
                    if (bar >= 1)
                    {
                        g.setColour (kTextDim);
                        g.drawText (juce::String (bar), x + 2, 1, 40, kRulerH - 2,
                                    juce::Justification::centredLeft);
                    }
                }
            }
        }

        if (! pitch.isAnalyzed() || pitch.regions().empty())
        {
            // QA-Fd 4a: the empty canvas names its state instead of sitting
            // silent (Analyzing / Deferred / Failed / genuinely empty).
            juce::String msg;
            switch (mOwner.mAnalysisState)
            {
                case AnalysisState::Analyzing:
                    msg = "Analyzing..."; break;
                case AnalysisState::Deferred:
                    msg = "Analysis deferred until stop - stop playback to refresh the notes"; break;
                case AnalysisState::Failed:
                    msg = mOwner.mAnalysisError.isNotEmpty()
                            ? mOwner.mAnalysisError
                            : "Analysis failed - check the channel has audio clips"; break;
                case AnalysisState::Idle:
                default:
                    msg = pitch.isAnalyzed()
                            ? "No notes detected on this channel yet"
                            : "Put audio clips on this channel - notes appear here automatically";
                    break;
            }
            g.setColour (kTextDim);
            g.setFont (13.0f);
            g.drawText (msg, getLocalBounds(), juce::Justification::centred);
            return;
        }

        // Detected pitch curve (Bass green, SOURCE positions -- the as-sung
        // reference stays put under every edit, locked 8).
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

        // Pills at their EDITED positions (dst spans + corrected pitch).
        const auto& regions = pitch.regions();
        for (int i = 0; i < (int) regions.size(); ++i)
        {
            const auto& r = regions[(size_t) i];
            auto pill = pillRect (r);
            if (pill.getRight() < 0.0f || pill.getX() > (float) getWidth()) continue;
            const bool sel = mOwner.isSelected (i);

            if (r.isSlice)
            {
                g.setColour (kSliceFill.withAlpha (sel ? 0.95f : 0.6f));
                g.fillRoundedRectangle (pill, 3.0f);
                // Always-on outline: abutting same-pitch pills would render
                // as one seamless bar without it (splits invisible).
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.drawRoundedRectangle (pill, 3.0f, 1.0f);
                if (sel)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.8f));
                    g.drawRoundedRectangle (pill, 3.0f, 1.2f);
                }
            }
            else
            {
                g.setColour (kPillFill.withAlpha (sel ? 0.95f : 0.7f));
                g.fillRoundedRectangle (pill, 5.0f);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.drawRoundedRectangle (pill, 5.0f, 1.0f);
                if (sel)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.8f));
                    g.drawRoundedRectangle (pill, 5.0f, 1.4f);
                }
                if (r.detached)
                {
                    g.setColour (kDetachCol.withAlpha (0.9f));
                    g.drawRoundedRectangle (pill.reduced (1.0f), 5.0f, 1.0f);
                }
                drawPillWaveform (g, r, pill);

                // In-block pitch readout at sufficient zoom (parity item 13).
                if (laneH >= 12.0 && pill.getWidth() >= 44.0f)
                {
                    const float edited = drawMidiFor (r);
                    const int   nearest = (int) std::round (edited);
                    const int   cents = (int) std::round ((edited - (float) nearest) * 100.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.85f));
                    g.setFont (9.5f);
                    g.drawText (midiNoteName (nearest)
                                  + (cents != 0 ? juce::String (" ")
                                        + (cents > 0 ? "+" : "") + juce::String (cents)
                                    : juce::String()),
                                (int) pill.getX() + 10, (int) pill.getY(),
                                (int) pill.getWidth() - 14, (int) pill.getHeight(),
                                juce::Justification::centredLeft);
                }
            }
            if (r.hasEdits() || r.hasTimeEdit())
            {
                g.setColour (kDirtyDot);
                g.fillEllipse (pill.getX() + 3.0f, pill.getY() + 3.0f, 5.0f, 5.0f);
            }

            // On-pill handles (19a): selected pill only.
            if (sel && (int) mOwner.mSelection.size() == 1 && handlesVisible (r))
                drawHandles (g, r);
        }

        // Display-only sub-edit box under a single-selected pill.
        if ((int) mOwner.mSelection.size() == 1
            && mOwner.mFocusRegion >= 0
            && mOwner.mFocusRegion < (int) regions.size())
            drawDisplayBox (g, regions[(size_t) mOwner.mFocusRegion]);

        // Marquee
        if (mDragKind == DragKind::Marquee)
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRect (mMarquee);
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawRect (mMarquee);
        }
        // Zoom rect (family Ctrl+RMB idiom)
        if (mDragKind == DragKind::ZoomRect)
        {
            g.setColour (kPillWave.withAlpha (0.15f));
            g.fillRect (mMarquee);
            g.setColour (kPillWave.withAlpha (0.7f));
            g.drawRect (mMarquee);
        }

        // Ruler range selection (drives song-mode looping via the builder grid) --
        // piano-roll parity: VC::Highlight (red), full-height column + denser ruler
        // band + bright edge lines in the ruler band.
        if (mRulerSelStart >= 0.0 && mRulerSelEnd > mRulerSelStart)
        {
            const int sx = xForSec (mRulerSelStart);
            const int ex = xForSec (mRulerSelEnd);
            if (ex > 0 && sx < getWidth())
            {
                g.setColour (VC::Highlight.withAlpha (0.08f));
                g.fillRect (sx, 0, ex - sx, getHeight());              // full-height column
                g.setColour (VC::Highlight.withAlpha (0.30f));
                g.fillRect (sx, 0, ex - sx, kRulerH);                  // ruler band, denser
                g.setColour (VC::Highlight.withAlpha (0.85f));
                g.drawVerticalLine (sx, 0.0f, (float) kRulerH);        // edge lines (ruler band)
                g.drawVerticalLine (ex, 0.0f, (float) kRulerH);
            }
        }

        // Playhead -- piano-roll / builder parity: VC::Green triangle in the ruler
        // band + a 2px body line below it (was a plain grey line).
        if (mOwner.mPlayheadSec >= 0.0)
        {
            const int x = xForSec (mOwner.mPlayheadSec);
            if (x >= 0 && x <= getWidth())
            {
                g.setColour (VC::Green.withAlpha (0.9f));
                juce::Path tri;
                tri.addTriangle ((float) x, 0.0f, (float) (x - 5), (float) kRulerH,
                                 (float) (x + 5), (float) kRulerH);
                g.fillPath (tri);
                g.setColour (VC::Green.withAlpha (0.8f));
                g.fillRect (x, kRulerH, 2, getHeight() - kRulerH);
            }
        }
    }

    // ── Mouse ────────────────────────────────────────────────────────────────
    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& pitch = mOwner.mProc.mPitch;
        auto& regions = pitch.regions();
        mDragKind = DragKind::None;
        mMouseDownPos = e.getPosition();

        // Middle-drag pan (parity item 2).
        if (e.mods.isMiddleButtonDown())
        {
            mDragKind = DragKind::Pan;
            mPanStartScroll = mOwner.scrollSeconds();
            mPanStartTop    = mOwner.topNote();
            return;
        }

        // Ruler band: bare left-click seeks the shared transport (moves the
        // builder-grid song playhead too); Ctrl+drag selects a time range that
        // drives song-mode looping via the builder selection (family idiom).
        // Right-click falls through to the existing menu path.
        if (e.y < kRulerH && ! e.mods.isPopupMenu())
        {
            if (e.mods.isCtrlDown())
            {
                mRulerDragging  = true;
                mRulerSelAnchor = snapSecToGrid (secForX (e.x));
                mRulerSelStart  = mRulerSelEnd = mRulerSelAnchor;
                repaint();
            }
            else if (mOwner.mProc.onTransportSeek)
                mOwner.mProc.onTransportSeek (beatForSec (secForX (e.x)));
            return;
        }

        const int hit = hitTest (e.getPosition());

        if (e.mods.isPopupMenu())
        {
            if (e.mods.isCtrlDown())
            {
                // Ctrl+RMB drag = zoom-to-rect (family idiom; works over
                // pills too).
                mDragKind = DragKind::ZoomRect;
                mMarquee = { e.x, e.y, 0, 0 };
                return;
            }
            if (hit >= 0)
            {
                if (! mOwner.isSelected (hit))
                    mOwner.selectOnly (hit);
                mOwner.mFocusRegion = hit;
                mOwner.showPillMenu (hit);
            }
            else
            {
                // #3 gesture: right-click the empty GRID with a live selection
                // (all or partial) + a real scale forces it to scale (Select All
                // -> right-click grid = Force All to Scale).  No selection ->
                // restore the zoom-to-selection view.
                if (! mOwner.mSelection.empty() && (int) mOwner.paramValue ("bsp_scale") > 0)
                    mOwner.forceSelectionToScale (mOwner.selectionOrFocus());
                else
                    mOwner.restoreSavedView();
            }
            return;
        }

        // Slice mode is a blade, not a selector (owner find 2026-07-11: the
        // old order selected first, so clicks read as selection): no marquee,
        // no handles, no popup -- a pill click splits at the click point.
        if (((int) mOwner.paramValue ("bsp_mode", 1.0f)) == 0)
        {
            if (hit < 0) return;
            auto& r = regions[(size_t) hit];
            // Slice snaps to the beat grid; hold Alt to slice anywhere (NewTone).
            double cutSec = secForX (e.x);
            if (! e.mods.isAltDown())
            {
                const double step = gridBeatStep();
                cutSec = secForBeat (std::round (beatForSec (cutSec) / step) * step);
            }
            const double tSrc = srcTimeForDstSec (r, cutSec);
            if (tSrc > r.startSec + 0.03 && tSrc < r.endSec - 0.03)
            {
                mOwner.beginEdit();
                PitchNoteRegion right = r;
                const double f = (tSrc - r.startSec) / (r.endSec - r.startSec);
                if (r.dstStartSec >= 0.0)
                {
                    const double dstMid = r.dstStart() + f * (r.dstEnd() - r.dstStart());
                    right.dstStartSec = dstMid;
                    right.dstEndSec   = r.dstEndSec;
                    r.dstEndSec       = dstMid;
                }
                right.startSec = tSrc;
                r.endSec       = tSrc;
                // Auto-mark the more-unvoiced half (the consonant) as a slice --
                // excluded from pitch correction so tuning the vowel doesn't shift
                // the noise.  Only when it is genuinely mostly unvoiced; slicing
                // mid-vowel leaves two plain notes.  Right-click toggles it.
                auto& pitch = mOwner.mProc.mPitch;
                const double vL = pitch.voicedFraction (r.startSec, r.endSec);
                const double vR = pitch.voicedFraction (right.startSec, right.endSec);
                if (vL <= vR) { if (vL < 0.5) r.isSlice = true; }
                else          { if (vR < 0.5) right.isSlice = true; }
                regions.insert (regions.begin() + hit + 1, right);
                mOwner.commitEdit ("Pitch: Slice");
            }
            repaint();
            return;
        }

        // Display-box buttons (single selection).
        if ((int) mOwner.mSelection.size() == 1 && mOwner.mFocusRegion >= 0
            && mOwner.mFocusRegion < (int) regions.size())
        {
            const auto box = displayBoxArea (regions[(size_t) mOwner.mFocusRegion]);
            if (boxEditBtn (box).contains (e.getPosition()))
                { mOwner.openSubEditor (mOwner.mFocusRegion); return; }
            if (boxResetBtn (box).contains (e.getPosition()))
                { mOwner.resetBoxedParams (mOwner.selectionOrFocus()); return; }
        }

        // On-pill handle grab (single selection, 19a).
        if ((int) mOwner.mSelection.size() == 1 && mOwner.mFocusRegion >= 0
            && mOwner.mFocusRegion < (int) regions.size())
        {
            const auto& r = regions[(size_t) mOwner.mFocusRegion];
            if (handlesVisible (r))
                for (int h = 0; h < 4; ++h)
                    if (handleRect (r, h).expanded (2.0f).contains (e.position))
                    {
                        mOwner.beginEdit();
                        mDragKind   = DragKind::Handle;
                        mHandleIdx  = h;
                        mHandlePill = mOwner.mFocusRegion;
                        applyHandleDrag (e.getPosition());
                        return;
                    }
        }

        if (hit < 0)
        {
            // Empty space: marquee select (Edit mode) / deselect.
            if (! e.mods.isShiftDown())
                mOwner.clearSelection();
            mDragKind = DragKind::Marquee;
            mMarquee = { e.x, e.y, 0, 0 };
            repaint();
            return;
        }

        // Double-click a pill = open the sub-editor popup.
        if (e.getNumberOfClicks() >= 2)
        {
            mOwner.selectOnly (hit);
            mOwner.openSubEditor (hit);
            return;
        }

        // Selection update (family convention: plain click selects; click on
        // an already-selected pill keeps the group; Shift+click toggles).
        // QA-Fe2: Shift-toggle only WITHOUT Ctrl (Ctrl+Shift = detach move).
        if (e.mods.isShiftDown() && ! e.mods.isCtrlDown())
            mOwner.toggleSelect (hit);
        else if (! mOwner.isSelected (hit))
            mOwner.selectOnly (hit);
        mOwner.mFocusRegion = hit;
        mOwner.updateInfoBarFor (hit);

        // QA-Fe2 gesture map (Jeff, 2026-07-16): body plain drag = vertical
        // pitch ONLY (dragging up can never knock a note out of line);
        // Ctrl = fine pitch; Ctrl+Alt = elastic move (neighbor-walled);
        // Ctrl+Shift = detach move (hard cut, no walls).  Edges keep:
        // drag = stretch, Ctrl = detach stretch.
        mOwner.beginEdit();
        const auto pill = pillRect (regions[(size_t) hit]);
        mFineDrag      = e.mods.isCtrlDown();
        mDetachArm     = e.mods.isCtrlDown();                          // stretch-path detach
        mMoveArm       = e.mods.isCtrlDown() && e.mods.isAltDown();
        mDetachMoveArm = e.mods.isCtrlDown() && e.mods.isShiftDown();
        mDidDetach  = false;
        if (e.position.x - pill.getX() < 6.0f)
            mDragKind = DragKind::StretchLeft;
        else if (pill.getRight() - e.position.x < 6.0f)
            mDragKind = DragKind::StretchRight;
        else
            mDragKind = DragKind::PitchMove;

        captureDragBases();
        mScrubbing = ! DSPBase::isTransportPlaying();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (mRulerDragging)
        {
            const double s = snapSecToGrid (secForX (e.x));
            mRulerSelStart = juce::jmin (mRulerSelAnchor, s);
            mRulerSelEnd   = juce::jmax (mRulerSelAnchor, s);
            pushRulerSel();
            repaint();
            return;
        }
        auto& pitch = mOwner.mProc.mPitch;
        auto& regions = pitch.regions();

        switch (mDragKind)
        {
            case DragKind::Pan:
            {
                mOwner.setView (mOwner.pixelsPerSecond(),
                                mPanStartScroll
                                    - (e.x - mMouseDownPos.x) / mOwner.pixelsPerSecond());
                mOwner.setTopNote (mPanStartTop
                    + (int) std::round ((e.y - mMouseDownPos.y) / mOwner.noteLaneH()));
                return;
            }
            case DragKind::Marquee:
            case DragKind::ZoomRect:
            {
                mMarquee = juce::Rectangle<int>::leftTopRightBottom (
                    juce::jmin (mMouseDownPos.x, e.x), juce::jmin (mMouseDownPos.y, e.y),
                    juce::jmax (mMouseDownPos.x, e.x), juce::jmax (mMouseDownPos.y, e.y));
                if (mDragKind == DragKind::Marquee)
                    applyMarqueeSelection (e.mods.isShiftDown());
                repaint();
                return;
            }
            case DragKind::Handle:
                applyHandleDrag (e.getPosition());
                repaint();
                return;
            case DragKind::PitchMove:
            {
                // Vertical: pitch (0.1 st; Ctrl fine 0.01; Snap ON lands on
                // in-scale lanes).  Horizontal (QA-Fe2 gesture map): only
                // when armed -- Ctrl+Alt elastic / Ctrl+Shift detach.
                const double dySemisRaw = (mMouseDownPos.y - e.y) / mOwner.noteLaneH();
                const bool  snapOn = mOwner.paramValue ("bsp_snap") > 0.5f
                                     && ! mFineDrag;
                const double quant = mFineDrag ? 0.01 : 0.1;
                const double dxSec = (e.x - mMouseDownPos.x) / mOwner.pixelsPerSecond();
                const bool wantDetach = mDetachMoveArm
                    && std::abs (e.x - mMouseDownPos.x) > 3;

                const double timeDelta = ! (mMoveArm || wantDetach) ? 0.0
                    : wantDetach ? juce::jmax (dxSec, minDetachDelta())
                                 : clampElasticDelta (dxSec);

                for (const auto& b : mDragBases)
                {
                    if (b.idx < 0 || b.idx >= (int) regions.size()) continue;
                    auto& r = regions[(size_t) b.idx];
                    if (! r.isSlice)
                    {
                        if (snapOn)
                        {
                            // Wall: the note's home semitone leaps only to VALID
                            // scale lanes; the shift is whole-semitone so r.midi's
                            // natural cents ride along (Center flattens them, not
                            // Snap).  NewTone "Snap Only" = land on lane, keep the
                            // cents.
                            const int   root      = (int) mOwner.paramValue ("bsp_root");
                            const int   scale     = (int) mOwner.paramValue ("bsp_scale");
                            const float homeInt   = std::round (r.midi);
                            const float wantedInt = homeInt + b.shift + (float) dySemisRaw;
                            const float laneInt   = PitchCorrectorDSP::snapMidiToScaleStatic (wantedInt, root, scale);
                            r.shiftSemis = juce::jlimit (-24.0f, 24.0f, laneInt - homeInt);
                        }
                        else
                        {
                            const double q = std::round (dySemisRaw / quant) * quant;
                            r.shiftSemis = (float) juce::jlimit (-24.0, 24.0,
                                (double) b.shift + q);
                        }
                    }
                    if (std::abs (timeDelta) > 1.0e-9)
                    {
                        r.dstStartSec = juce::jmax (0.0, b.dstS + timeDelta);
                        r.dstEndSec   = r.dstStartSec + (b.dstE - b.dstS);
                        if (wantDetach && ! r.detached)
                            { r.detached = true; mDidDetach = true; }
                    }
                }
                mOwner.updateInfoBarFor (mOwner.mFocusRegion);
                maybeScrub();
                repaint();
                return;
            }
            case DragKind::StretchLeft:
            case DragKind::StretchRight:
            {
                if (mOwner.mFocusRegion < 0
                    || mOwner.mFocusRegion >= (int) regions.size()) return;
                auto& r = regions[(size_t) mOwner.mFocusRegion];
                const DragBase* b = baseFor (mOwner.mFocusRegion);
                if (b == nullptr) return;
                const double dxSec = (e.x - mMouseDownPos.x) / mOwner.pixelsPerSecond();
                const bool detach = mDetachArm && std::abs (e.x - mMouseDownPos.x) > 3;
                const double srcLen = juce::jmax (1.0e-6, r.endSec - r.startSec);
                double s = b->dstS, en = b->dstE;
                if (mDragKind == DragKind::StretchLeft)
                {
                    double loS = detach ? 0.0 : prevBound (mOwner.mFocusRegion);
                    double hiS = en - 0.02;
                    if (! detach)   // keep the note's own dst/src ratio off the rails
                    {
                        loS = juce::jmax (loS, en - srcLen * kMaxWarpRatio);   // max stretch
                        hiS = juce::jmin (hiS, en - srcLen / kMaxWarpRatio);   // max compress
                    }
                    s = juce::jlimit (loS, juce::jmax (loS, hiS), b->dstS + dxSec);
                    s = juce::jmax (0.0, s);
                }
                else
                {
                    double loE = s + 0.02;
                    double hiE = detach ? 1.0e9 : nextBound (mOwner.mFocusRegion);
                    if (! detach)
                    {
                        loE = juce::jmax (loE, s + srcLen / kMaxWarpRatio);    // max compress
                        hiE = juce::jmin (hiE, s + srcLen * kMaxWarpRatio);    // max stretch
                    }
                    en = juce::jlimit (loE, juce::jmax (loE, hiE), b->dstE + dxSec);
                }
                r.dstStartSec = s;
                r.dstEndSec   = en;
                if (detach && ! r.detached) { r.detached = true; mDidDetach = true; }
                mOwner.updateInfoBarFor (mOwner.mFocusRegion);
                repaint();
                return;
            }
            case DragKind::None:
                return;
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (mRulerDragging)
        {
            mRulerDragging = false;
            const double secPerBeat = secForBeat (1.0) - secForBeat (0.0);
            if (mRulerSelEnd - mRulerSelStart < 0.25 * juce::jmax (0.05, secPerBeat))
                mRulerSelStart = mRulerSelEnd = -1.0;   // tiny drag == click -> clear
            pushRulerSel();
            repaint();
            return;
        }
        switch (mDragKind)
        {
            case DragKind::ZoomRect:
                if (mMarquee.getWidth() > 8 && mMarquee.getHeight() > 8)
                    zoomToRect (mMarquee);
                break;
            case DragKind::Marquee:
                break;
            case DragKind::Handle:
                mOwner.commitEdit ("Pitch: Handle Ramp");
                break;
            case DragKind::PitchMove:
                if (e.getPosition() != mMouseDownPos)
                    mOwner.commitEdit (mDidDetach ? "Pitch: Detach Move"
                                                  : "Pitch: Move");
                else
                    mOwner.cancelEdit();
                break;
            case DragKind::StretchLeft:
            case DragKind::StretchRight:
                if (e.getPosition() != mMouseDownPos)
                    mOwner.commitEdit (mDidDetach ? "Pitch: Detach Stretch"
                                                  : "Pitch: Stretch");
                else
                    mOwner.cancelEdit();
                break;
            case DragKind::Pan:
            case DragKind::None:
                break;
        }
        if (mScrubbing) mOwner.stopPreview();
        mScrubbing = false;
        mDragKind = DragKind::None;
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        // Hover feedback: handles advertise their drag axis (19a).
        auto& regions = mOwner.mProc.mPitch.regions();
        if ((int) mOwner.mSelection.size() == 1 && mOwner.mFocusRegion >= 0
            && mOwner.mFocusRegion < (int) regions.size())
        {
            const auto& r = regions[(size_t) mOwner.mFocusRegion];
            if (handlesVisible (r))
            {
                int hov = -1;
                for (int h = 0; h < 4; ++h)
                    if (handleRect (r, h).expanded (2.0f).contains (e.position))
                        { hov = h; break; }
                if (hov != mHoverHandle) { mHoverHandle = hov; repaint(); }
                if (hov >= 0)
                {
                    setMouseCursor (hov <= 1 ? juce::MouseCursor::LeftRightResizeCursor
                                             : juce::MouseCursor::UpDownLeftRightResizeCursor);
                    mOwner.updateInfoBarFor (mOwner.mFocusRegion);
                    return;
                }
            }
        }
        else if (mHoverHandle != -1) { mHoverHandle = -1; repaint(); }

        const int hit = hitTest (e.getPosition());
        const bool sliceMode = ((int) mOwner.paramValue ("bsp_mode", 1.0f)) == 0;
        if (hit >= 0)
        {
            if (sliceMode)
            {
                // Blade affordance: the I-beam marks the cut point.
                setMouseCursor (juce::MouseCursor::IBeamCursor);
                mOwner.updateInfoBarFor (hit);
                return;
            }
            const auto pill = pillRect (regions[(size_t) hit]);
            const bool edge = (e.position.x - pill.getX() < 6.0f)
                           || (pill.getRight() - e.position.x < 6.0f);
            setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor
                                 : juce::MouseCursor::NormalCursor);
            mOwner.updateInfoBarFor (hit);
        }
        else
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
            mOwner.updateInfoBarFor (mOwner.mFocusRegion);
        }
    }

    // Wheel map per family idiom: Ctrl h-zoom, Alt v-zoom, Shift h-scroll,
    // bare v-scroll.
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown())
        {
            const double f = (w.deltaY > 0) ? 1.15 : 1.0 / 1.15;   // piano-roll step
            const double anchorSec = secForX (e.x);
            // Clamp THEN anchor (piano-roll applyZoomAnchored parity): let setView
            // apply the zoom-out floor, then re-anchor with the actual clamped pps so
            // the second under the cursor stays put even at the zoom limit.
            mOwner.setView (mOwner.pixelsPerSecond() * f, mOwner.scrollSeconds());
            const double clamped = mOwner.pixelsPerSecond();
            mOwner.setView (clamped, anchorSec - (double) e.x / clamped);
        }
        else if (e.mods.isAltDown())
        {
            // Alt+wheel vertical zoom (parity item 2; PianoRoll idiom).
            const double f = (w.deltaY > 0) ? 1.15 : 1.0 / 1.15;
            mOwner.setLaneHeight (mOwner.noteLaneH() * f, midiForY (e.y), e.y);
        }
        else if (e.mods.isShiftDown())
        {
            mOwner.setView (mOwner.pixelsPerSecond(),
                            mOwner.scrollSeconds() - w.deltaY * 60.0 / mOwner.pixelsPerSecond());
        }
        else
            mOwner.setTopNote (mOwner.topNote() + (w.deltaY > 0 ? 2 : -2));
    }

    void zoomToRegions (const std::vector<int>& idxs)
    {
        auto& regions = mOwner.mProc.mPitch.regions();
        if (idxs.empty()) return;
        double t0 = 1.0e18, t1 = -1.0e18;
        float mLo = 200.0f, mHi = -200.0f;
        for (int i : idxs)
        {
            if (i < 0 || i >= (int) regions.size()) continue;
            const auto& r = regions[(size_t) i];
            t0 = juce::jmin (t0, r.dstStart());
            t1 = juce::jmax (t1, r.dstEnd());
            if (! r.isSlice)
            {
                mLo = juce::jmin (mLo, drawMidiFor (r) - 4.0f);
                mHi = juce::jmax (mHi, drawMidiFor (r) + 4.0f);
            }
        }
        if (t1 <= t0) return;
        mOwner.mHasSavedView = true;
        mOwner.mSavedPps = mOwner.pixelsPerSecond();
        mOwner.mSavedScroll = mOwner.scrollSeconds();
        mOwner.mSavedTopNote = mOwner.topNote();
        mOwner.mSavedLaneH = mOwner.noteLaneH();

        const double pps = juce::jlimit (20.0, 2000.0,
            (double) juce::jmax (60, getWidth() - 40) / (t1 - t0));
        mOwner.setView (pps, t0 - 20.0 / pps);
        if (mHi > mLo)
        {
            const double laneH = juce::jlimit (4.0, 24.0,
                (double) juce::jmax (60, getHeight() - kRulerH)
                    / juce::jmax (8.0, (double) (mHi - mLo)));
            mOwner.setLaneHeight (laneH, (mLo + mHi) * 0.5,
                                  (getHeight() + kRulerH) / 2);
        }
    }

private:
    enum class DragKind { None, PitchMove, StretchLeft, StretchRight,
                          Marquee, ZoomRect, Pan, Handle };
    struct DragBase { int idx; float shift; double dstS, dstE; };

    int hitTest (juce::Point<int> pos) const
    {
        const auto& regions = mOwner.mProc.mPitch.regions();
        for (int i = 0; i < (int) regions.size(); ++i)
            if (pillRect (regions[(size_t) i]).contains (pos.toFloat()))
                return i;
        return -1;
    }

    // Map a canvas dst-time back into a region's SOURCE time (slice tool).
    static double srcTimeForDstSec (const PitchNoteRegion& r, double dstSec)
    {
        const double dLen = r.dstEnd() - r.dstStart();
        if (dLen <= 1.0e-9) return r.startSec;
        const double f = juce::jlimit (0.0, 1.0, (dstSec - r.dstStart()) / dLen);
        return r.startSec + f * (r.endSec - r.startSec);
    }

    void captureDragBases()
    {
        mDragBases.clear();
        const auto& regions = mOwner.mProc.mPitch.regions();
        for (int i : mOwner.selectionOrFocus())
            if (i >= 0 && i < (int) regions.size())
            {
                const auto& r = regions[(size_t) i];
                mDragBases.push_back ({ i, r.shiftSemis, r.dstStart(), r.dstEnd() });
            }
    }

    const DragBase* baseFor (int idx) const
    {
        for (const auto& b : mDragBases)
            if (b.idx == idx) return &b;
        return nullptr;
    }

    // Non-selected neighbor bound in DST time + the SOURCE position of that same
    // neighbor (for the gap warp-ratio clamp).  prev -> {0,0} = composite start;
    // next -> {big,big} = no neighbor.
    struct Bound { double dst; double src; };
    Bound prevBoundEx (int idx) const
    {
        const auto& regions = mOwner.mProc.mPitch.regions();
        Bound b { 0.0, 0.0 };
        for (int j = 0; j < (int) regions.size(); ++j)
        {
            if (j == idx || mOwner.isSelected (j)) continue;
            const auto& o = regions[(size_t) j];
            if (o.dstEnd() <= regions[(size_t) idx].dstStart() + 1.0e-9 && o.dstEnd() >= b.dst)
                b = { o.dstEnd(), o.endSec };
        }
        return b;
    }
    Bound nextBoundEx (int idx) const
    {
        const auto& regions = mOwner.mProc.mPitch.regions();
        Bound b { 1.0e18, 1.0e18 };
        for (int j = 0; j < (int) regions.size(); ++j)
        {
            if (j == idx || mOwner.isSelected (j)) continue;
            const auto& o = regions[(size_t) j];
            if (o.dstStart() >= regions[(size_t) idx].dstEnd() - 1.0e-9 && o.dstStart() <= b.dst)
                b = { o.dstStart(), o.startSec };
        }
        return b;
    }
    double prevBound (int idx) const { return prevBoundEx (idx).dst; }
    double nextBound (int idx) const { return nextBoundEx (idx).dst; }

    // Move-delta clamp (design E): pills never cross a non-selected neighbor AND
    // no adjacent GAP is squashed/stretched past kMaxWarpRatio (that ratio is what
    // rails the vocoder into garble).  Also caps the leading gap so a move to the
    // origin can't cram the anacrusis into ~0.
    double clampElasticDelta (double wanted) const
    {
        const auto& regions = mOwner.mProc.mPitch.regions();
        double lo = -1.0e18, hi = 1.0e18;
        for (const auto& b : mDragBases)
        {
            if (b.idx < 0 || b.idx >= (int) regions.size()) continue;
            const auto& r = regions[(size_t) b.idx];

            const Bound pv = prevBoundEx (b.idx);
            const double gapSrcBefore = juce::jmax (0.0, r.startSec - pv.src);
            if (gapSrcBefore > 1.0e-6)   // gap-before dst len = (b.dstS + d) - pv.dst
            {
                lo = juce::jmax (lo, pv.dst + gapSrcBefore / kMaxWarpRatio - b.dstS);
                hi = juce::jmin (hi, pv.dst + gapSrcBefore * kMaxWarpRatio - b.dstS);
            }
            else
                lo = juce::jmax (lo, pv.dst - b.dstS);

            const Bound nx = nextBoundEx (b.idx);
            if (nx.dst < 1.0e17)
            {
                const double gapSrcAfter = juce::jmax (0.0, nx.src - r.endSec);
                if (gapSrcAfter > 1.0e-6)   // gap-after dst len = nx.dst - (b.dstE + d)
                {
                    hi = juce::jmin (hi, nx.dst - gapSrcAfter / kMaxWarpRatio - b.dstE);
                    lo = juce::jmax (lo, nx.dst - gapSrcAfter * kMaxWarpRatio - b.dstE);
                }
                else
                    hi = juce::jmin (hi, nx.dst - b.dstE);
            }

            lo = juce::jmax (lo, -b.dstS);                 // composite start
        }
        return juce::jlimit (juce::jmin (0.0, lo), juce::jmax (0.0, hi), wanted);
    }
    double minDetachDelta() const
    {
        double lo = -1.0e18;
        for (const auto& b : mDragBases)
            lo = juce::jmax (lo, -b.dstS);
        return lo;   // detach: free placement, floored at composite start
    }

    void applyMarqueeSelection (bool additive)
    {
        auto& regions = mOwner.mProc.mPitch.regions();
        if (! additive) mOwner.mSelection.clear();
        for (int i = 0; i < (int) regions.size(); ++i)
            if (pillRect (regions[(size_t) i]).toNearestInt()
                    .intersects (mMarquee))
            {
                mOwner.mSelection.insert (i);
                mOwner.mFocusRegion = i;
            }
    }

    void zoomToRect (juce::Rectangle<int> rect)
    {
        mOwner.mHasSavedView = true;
        mOwner.mSavedPps = mOwner.pixelsPerSecond();
        mOwner.mSavedScroll = mOwner.scrollSeconds();
        mOwner.mSavedTopNote = mOwner.topNote();
        mOwner.mSavedLaneH = mOwner.noteLaneH();

        const double t0 = secForX (rect.getX());
        const double t1 = secForX (rect.getRight());
        const double midiHi = midiForY (rect.getY());
        const double midiLo = midiForY (rect.getBottom());
        if (t1 - t0 < 0.01) return;
        const double pps = juce::jlimit (20.0, 2000.0, (double) getWidth() / (t1 - t0));
        mOwner.setView (pps, t0);
        const double span = juce::jmax (6.0, midiHi - midiLo);
        const double laneH = juce::jlimit (4.0, 24.0,
            (double) juce::jmax (60, getHeight() - kRulerH) / span);
        mOwner.setLaneHeight (laneH, (midiHi + midiLo) * 0.5,
                              (getHeight() + kRulerH) / 2);
    }

    // 150 ms throttle keeps the per-step preview re-render from hitching
    // long pills.
    void maybeScrub()
    {
        if (! mScrubbing || mOwner.mFocusRegion < 0) return;
        const juce::uint32 now = juce::Time::getMillisecondCounter();
        if (now - mLastScrubMs < 150) return;
        mLastScrubMs = now;
        mOwner.mProc.mPitch.publishEdits();
        mOwner.startRegionPreview (mOwner.mFocusRegion, false);
    }

    // ── On-pill handles (19a single-storage contract) ────────────────────────
    // Handles write ORDINARY points into the SAME volShape / pitchShape the
    // popup edits; re-dragging over a complex boundary span replaces that
    // span with the clean ramp (deliberate, one undo step).
    void applyHandleDrag (juce::Point<int> pos)
    {
        auto& regions = mOwner.mProc.mPitch.regions();
        if (mHandlePill < 0 || mHandlePill >= (int) regions.size()) return;
        auto& r = regions[(size_t) mHandlePill];
        const auto pill = pillRect (r);
        const float t01 = juce::jlimit (0.0f, 1.0f,
            (float) (pos.x - pill.getX()) / juce::jmax (1.0f, pill.getWidth()));

        auto replaceSpanLE = [] (std::vector<juce::Point<float>>& pts, float x)
        {
            pts.erase (std::remove_if (pts.begin(), pts.end(),
                        [x] (const juce::Point<float>& p) { return p.x <= x + 1.0e-4f; }),
                       pts.end());
        };
        auto replaceSpanGE = [] (std::vector<juce::Point<float>>& pts, float x)
        {
            pts.erase (std::remove_if (pts.begin(), pts.end(),
                        [x] (const juce::Point<float>& p) { return p.x >= x - 1.0e-4f; }),
                       pts.end());
        };
        auto sortPts = [] (std::vector<juce::Point<float>>& pts)
        {
            std::sort (pts.begin(), pts.end(),
                       [] (const juce::Point<float>& a, const juce::Point<float>& b)
                       { return a.x < b.x; });
        };

        if (mHandleIdx == 0)          // volume fade-in: horizontal = length
        {
            const float len = juce::jlimit (0.0f, 0.9f, t01);
            replaceSpanLE (r.volShape, len);
            if (len > 0.005f)
            {
                r.volShape.push_back ({ 0.0f, 0.0f });
                r.volShape.push_back ({ len, 1.0f });
            }
            sortPts (r.volShape);
            mHandleReadout = "Fade in: " + juce::String (len * 100.0f, 0) + "%";
        }
        else if (mHandleIdx == 1)     // volume fade-out
        {
            const float start = juce::jlimit (0.1f, 1.0f, t01);
            replaceSpanGE (r.volShape, start);
            if (start < 0.995f)
            {
                r.volShape.push_back ({ start, 1.0f });
                r.volShape.push_back ({ 1.0f, 0.0f });
            }
            sortPts (r.volShape);
            mHandleReadout = "Fade out: " + juce::String ((1.0f - start) * 100.0f, 0) + "%";
        }
        else
        {
            // Pitch approach (2) / release (3): 2-axis -- horizontal =
            // length, vertical = semitone offset scooped from / released to.
            const float semis = juce::jlimit (-12.0f, 12.0f,
                (float) ((pill.getCentreY() - pos.y) / mOwner.noteLaneH()));
            if (mHandleIdx == 2)
            {
                const float len = juce::jlimit (0.02f, 0.9f, t01);
                replaceSpanLE (r.pitchShape, len);
                if (std::abs (semis) > 0.05f)
                {
                    r.pitchShape.push_back ({ 0.0f, semis });
                    r.pitchShape.push_back ({ len, 0.0f });
                }
                sortPts (r.pitchShape);
                mHandleReadout = "Approach: " + juce::String (semis, 1) + " st over "
                               + juce::String (len * 100.0f, 0) + "%";
            }
            else
            {
                const float start = juce::jlimit (0.1f, 0.98f, t01);
                replaceSpanGE (r.pitchShape, start);
                if (std::abs (semis) > 0.05f)
                {
                    r.pitchShape.push_back ({ start, 0.0f });
                    r.pitchShape.push_back ({ 1.0f, semis });
                }
                sortPts (r.pitchShape);
                mHandleReadout = "Release: " + juce::String (semis, 1) + " st from "
                               + juce::String ((1.0f - start) * 100.0f, 0) + "%";
            }
        }
        mOwner.updateInfoBarFor (mHandlePill);
    }

    void drawHandles (juce::Graphics& g, const PitchNoteRegion& r)
    {
        for (int h = 0; h < 4; ++h)
        {
            auto hr = handleRect (r, h);
            g.setColour ((h == 0 || h == 2) ? kHandleIn : kHandleOut);
            g.fillRect (hr);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawRect (hr, 1.0f);
            if (h == mHoverHandle)
            {
                // Hover arrows advertise the drag axis (19a).
                g.setColour (juce::Colours::white);
                const float cx = hr.getCentreX(), cy = hr.getCentreY();
                g.drawArrow ({ cx - 10.0f, cy, cx - 4.0f, cy }, 1.4f, 5.0f, 4.0f);
                g.drawArrow ({ cx + 10.0f, cy, cx + 4.0f, cy }, 1.4f, 5.0f, 4.0f);
                if (h >= 2)
                {
                    g.drawArrow ({ cx, cy - 10.0f, cx, cy - 4.0f }, 1.4f, 5.0f, 4.0f);
                    g.drawArrow ({ cx, cy + 10.0f, cx, cy + 4.0f }, 1.4f, 5.0f, 4.0f);
                }
            }
        }
    }

    // Display-only sub-edit box (Jeff's design): 4 bars VIB / FRM / VOL /
    // PITCH + Edit button over the titles + Reset button over the bars.
    void drawDisplayBox (juce::Graphics& g, const PitchNoteRegion& r)
    {
        const auto box = displayBoxArea (r);
        if (box.getBottom() > getHeight()) return;   // off-canvas
        g.setColour (kPanelBg.withAlpha (0.94f));
        g.fillRoundedRectangle (box.toFloat(), 3.0f);

        auto edit = boxEditBtn (box);
        auto rst  = boxResetBtn (box);
        g.setColour (kPillWave.withAlpha (0.85f));
        g.fillRoundedRectangle (edit.toFloat(), 2.0f);
        g.setColour (juce::Colours::black);
        g.setFont (9.0f);
        g.drawText ("EDIT", edit, juce::Justification::centred);
        g.setColour (kStale.withAlpha (0.85f));
        g.fillRoundedRectangle (rst.toFloat(), 2.0f);
        g.setColour (juce::Colours::black);
        g.drawText ("RESET", rst, juce::Justification::centred);

        auto bars = box.withTrimmedTop (kBoxBtnH + 4).reduced (2, 1);
        const int laneH = bars.getHeight() / 4;
        auto lane = [&] (int idx)
        {
            return bars.withHeight (laneH).withY (bars.getY() + idx * laneH);
        };
        g.setFont (8.0f);

        {   // VIB: filled bar = depth mult (0..2, half = natural)
            auto L = lane (0).reduced (2, 1);
            g.setColour (kTextDim);
            g.drawText ("VIB", L.removeFromLeft (26), juce::Justification::centredLeft);
            g.setColour (kPitchCurve.withAlpha (0.25f));
            g.fillRect (L);
            g.setColour (kPitchCurve);
            g.fillRect ((float) L.getX(), (float) L.getY(),
                        (float) L.getWidth() * juce::jlimit (0.0f, 2.0f, r.vibDepthMult) * 0.5f,
                        (float) L.getHeight());
        }
        {   // FRM: bipolar bar (-6..+6 st)
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
        {   // VOL: shape polyline (unity midline when empty)
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
        {   // PITCH: additive curve (+-12 st) around the midline
            auto L = lane (3).reduced (2, 1);
            g.setColour (kTextDim);
            g.drawText ("PIT", L.removeFromLeft (26), juce::Justification::centredLeft);
            g.setColour (kPillFill.withAlpha (0.25f));
            g.fillRect (L);
            g.setColour (kPillFill.brighter (0.4f));
            juce::Path p;
            const int steps = juce::jmax (2, L.getWidth() / 4);
            for (int s = 0; s <= steps; ++s)
            {
                const float t01 = (float) s / (float) steps;
                const float semis = juce::jlimit (-12.0f, 12.0f, r.pitchSemisAt (t01));
                const float x = (float) L.getX() + t01 * (float) L.getWidth();
                const float y = (float) L.getCentreY() - semis / 12.0f * 0.5f * (float) L.getHeight();
                if (s == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }
    }

    // Pill waveform, drawn POST-GAIN so handle fades show on-canvas (19a).
    void drawPillWaveform (juce::Graphics& g, const PitchNoteRegion& r,
                           juce::Rectangle<float> pill)
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
        const double srcLen = r.endSec - r.startSec;
        for (int x = px0; x < px1; ++x)
        {
            // Canvas dst position -> region t01 -> SOURCE audio position.
            const float t01a = juce::jlimit (0.0f, 1.0f,
                (float) ((secForX (x) - r.dstStart())
                         / juce::jmax (1.0e-9, r.dstEnd() - r.dstStart())));
            const float t01b = juce::jlimit (0.0f, 1.0f,
                (float) ((secForX (x + 1) - r.dstStart())
                         / juce::jmax (1.0e-9, r.dstEnd() - r.dstStart())));
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

    BaySickPitchEditor& mOwner;
    DragKind mDragKind { DragKind::None };
    juce::Point<int> mMouseDownPos;
    juce::Rectangle<int> mMarquee;
    // Ruler range selection (Ctrl+drag) in composite seconds; drives + mirrors
    // the builder-grid SONG time selection (loops in song mode).
    double mRulerSelStart { -1.0 }, mRulerSelEnd { -1.0 }, mRulerSelAnchor { 0.0 };
    bool   mRulerDragging { false };
    std::vector<DragBase> mDragBases;
    bool mFineDrag { false }, mDetachArm { false }, mDidDetach { false };
    // QA-Fe2 gesture map: Ctrl+Alt = elastic move, Ctrl+Shift = detach move.
    bool mMoveArm { false }, mDetachMoveArm { false };
    bool mScrubbing { false };
    juce::uint32 mLastScrubMs { 0 };
    double mPanStartScroll { 0.0 };
    int    mPanStartTop { 84 };
    int mHandleIdx { -1 }, mHandlePill { -1 }, mHoverHandle { -1 };
    juce::String mHandleReadout;

    friend class BaySickPitchEditor;
};

// ─────────────────────────────────────────────────────────────────────────────
// InfoBar: monospace Pitch / Cents / Length at hover/selection
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
// Toolbar - QA-Fd: preset combo retired (7a); Root/Scale/Snap added (8/14a-c);
// Undo/Redo drive the GLOBAL stack (9a).
// ─────────────────────────────────────────────────────────────────────────────
class BaySickPitchEditor::Toolbar : public juce::Component,
                                    private juce::ComboBox::Listener
{
public:
    Toolbar (BaySickPitchEditor& o, juce::AudioProcessorValueTreeState& apvts)
        : mOwner (o), mApvts (apvts)
    {
        // Jeff, 2026-08-04: the logo moved to the hosting window's title strip.
        // Added-but-hidden (not deleted) so the toolbar's setBounds still
        // reserves its slot -- every other widget stays exactly where it was.
        addChildComponent (mTitleLbl);

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
        plain (mSliceBtn,  "Slice",  "Slice mode: click a note to split it -- e.g. chop a consonant off its vowel (snaps to grid; Alt = free)");
        plain (mEditBtn,   "Edit",   "Edit mode: drag pills = pitch (Ctrl = fine), edges stretch (Ctrl = detach stretch), Ctrl+Alt+drag = move, Ctrl+Shift+drag = detach move");
        plain (mResetBtn,  "Reset",  "Clear every pitch edit on this channel");
        plain (mRenderBtn, "Render",
               "Export the edited channel to Pitched/{name}_pitch_v{N}.wav (file only - playback is already live)");
        plain (mSendBtn,   "Send Notes to...", "Send the detected notes as MIDI to a Layers / Bass / Drums / Clips tab");
        plain (mSnapshotBtn, "Snapshot",
               "Save the current edits as a restore point in the version history");
        plain (mVersionsBtn, "Versions",
               "Revert to an earlier snapshot (every analyze also creates one)");
        plain (mUndoBtn,   "Undo",   "Undo (app-wide history - Ctrl+Z)");
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

        // QA-Fd 8/14a: Root + Scale + Snap (shared 13-scale table; the
        // editor's pick is deliberately independent from the realtime board).
        addAndMakeVisible (mRootCombo);
        const char* keyNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        for (int i = 0; i < 12; ++i) mRootCombo.addItem (keyNames[i], i + 1);
        mRootCombo.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
        mRootCombo.setColour (juce::ComboBox::textColourId, kText);
        mRootCombo.setTooltip ("Root note for Snap and the Focus pull target");
        mRootAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            mApvts, "bsp_root", mRootCombo);

        addAndMakeVisible (mScaleCombo);
        for (int i = 0; i < PitchCorrectorDSP::kNumScales; ++i)
            mScaleCombo.addItem (PitchCorrectorDSP::scaleName (i), i + 1);
        mScaleCombo.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
        mScaleCombo.setColour (juce::ComboBox::textColourId, kText);
        mScaleCombo.setTooltip ("Scale for Snap and the Focus pull target (same list as the piano roll)");
        mScaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            mApvts, "bsp_scale", mScaleCombo);

        // QA-Fe B2: pitch engine dropdown (bsp_engine value order 0/1/2).  A
        // ComboBox::Listener (separate from the attachment's onChange) fires the
        // WORLD-offline notice on a user pick; mEnginePopupArmed suppresses the
        // attachment's construction-time sync.
        addAndMakeVisible (mEngineCombo);
        mEngineCombo.addItem ("Rubber Band - Balanced",             1);
        mEngineCombo.addItem ("Signalsmith - Lightest (Low CPU)",   2);
        mEngineCombo.addItem ("WORLD - Highest Quality (High CPU)", 3);
        mEngineCombo.setColour (juce::ComboBox::backgroundColourId, kPanelBg);
        mEngineCombo.setColour (juce::ComboBox::textColourId, kText);
        mEngineCombo.setTooltip ("Pitch-shift engine.  Rubber Band + Signalsmith edit live; "
                                 "WORLD is highest quality but works offline (edits apply a moment later).");
        mEngineAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            mApvts, "bsp_engine", mEngineCombo);
        mEngineCombo.addListener (this);

        addAndMakeVisible (mSnapBtn);
        mSnapBtn.setButtonText ("Snap");
        mSnapBtn.setClickingTogglesState (true);
        mSnapBtn.setTooltip ("Snap ON: Focus pulls toward the Root/Scale notes and "
                             "vertical drags land on in-scale lanes (Ctrl+drag = free fine "
                             "movement).  OFF: nearest semitone.");
        mSnapAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            mApvts, "bsp_snap", mSnapBtn);

        mSaveBtn    .onClick = [this] { mOwner.saveUserPreset(); };
        mLoadBtn    .onClick = [this] { mOwner.loadUserPreset(); };
        mResetBtn   .onClick = [this] { mOwner.runReset(); };
        mRenderBtn  .onClick = [this] { mOwner.runRender(); };
        mSendBtn    .onClick = [this] { mOwner.showSendNotesMenu(); };
        mSnapshotBtn.onClick = [this] { mOwner.runSnapshotVersion(); };
        mVersionsBtn.onClick = [this] { mOwner.showVersionsMenu(); };
        mUndoBtn    .onClick = [this] { if (mOwner.mUndoCtx.undo) mOwner.mUndoCtx.undo(); };
        mRedoBtn    .onClick = [this] { if (mOwner.mUndoCtx.redo) mOwner.mUndoCtx.redo(); };

        // Focus / Mod / Speed, APVTS-attached, FL knob conventions (P1-14).
        auto knob = [this] (juce::Slider& s, const char* id, const juce::String& tt,
                            std::unique_ptr<TaggedSliderAttachment>& att,
                            double defaultValue)
        {
            addAndMakeVisible (s);
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 14);
            s.setTooltip (tt);
            att = std::make_unique<TaggedSliderAttachment> (mApvts, id, s);
            applyFLKnobFeel (s, defaultValue);
        };
        knob (mFocus, "bsp_focus", "How strongly note centers pull to the snap target (semitone, or the Root/Scale when Snap is on).  0 = leave the take alone.", mFocusAtt, 0.0);
        knob (mMod,   "bsp_mod",   "Vibrato movement: 50 = natural, above adds synthesized vibrato per note", mModAtt, 50.0);
        knob (mSpeed, "bsp_speed", "How fast pitch moves between notes: low = smooth glide, high = instant", mSpeedAtt, 50.0);
        knob (mThroat, "bsp_throat", "Throat / character: 50 = natural; left = bigger/darker, right = smaller/brighter (formant only, pitch stays put)", mThroatAtt, 50.0);
        mEnginePopupArmed = true;   // now user picks fire the WORLD notice, not the ctor sync
    }

    void comboBoxChanged (juce::ComboBox* cb) override
    {
        if (mEnginePopupArmed && cb == &mEngineCombo
            && cb->getSelectedItemIndex() == 2)   // WORLD (offline)
            mOwner.showWorldOfflineNotice();
    }

    void setStale (bool s, bool pending)
    {
        if (mStale != s || mStalePending != pending)
            { mStale = s; mStalePending = pending; repaint(); }
    }
    // QA-Fd 4a: transient analysis state -- wins over the stale text.
    void setAnalysisBadge (const juce::String& t)
    {
        if (t != mAnalysisBadge) { mAnalysisBadge = t; repaint(); }
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
    void setUndoRedoEnabled (bool canUndo, bool canRedo)
    {
        mUndoBtn.setEnabled (canUndo);
        mRedoBtn.setEnabled (canRedo);
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

        if (mAnalysisBadge.isNotEmpty())
        {
            g.setColour (kStale);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText (mAnalysisBadge,
                        mRootCombo.getX() - 196, 2, 188, 30,
                        juce::Justification::centredRight);
        }
        else if (mStale)
        {
            // Pending = grid changed while the transport runs; the stop-
            // gated auto re-analyze fires at the next stop.
            g.setColour (kStale);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText (mStalePending ? "RE-ANALYZE ON STOP" : "RE-ANALYZE",
                        mRootCombo.getX() - 156, 2, 148, 30,
                        juce::Justification::centredRight);
        }
        // LENGTH readout, monospace.
        g.setColour (kTextDim);
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 11.0f,
                               juce::Font::plain));
        g.drawText (mLengthText, 8, kToolbarH - 26, 250, 20,
                    juce::Justification::centredLeft);

        g.setFont (10.0f);
        g.drawText ("Focus",  mFocus .getX(), 1, mFocus .getWidth(), 11, juce::Justification::centred);
        g.drawText ("Mod",    mMod   .getX(), 1, mMod   .getWidth(), 11, juce::Justification::centred);
        g.drawText ("Speed",  mSpeed .getX(), 1, mSpeed .getWidth(), 11, juce::Justification::centred);
        g.drawText ("Throat", mThroat.getX(), 1, mThroat.getWidth(), 11, juce::Justification::centred);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6, 3);

        auto knobs = b.removeFromRight (4 * 56 + 6);
        knobs.removeFromTop (11);
        mFocus .setBounds (knobs.removeFromLeft (56).reduced (2, 0));
        mMod   .setBounds (knobs.removeFromLeft (56).reduced (2, 0));
        mSpeed .setBounds (knobs.removeFromLeft (56).reduced (2, 0));
        mThroat.setBounds (knobs.removeFromLeft (56).reduced (2, 0));

        auto row1 = b.removeFromTop (26);
        mTitleLbl.setBounds (row1.removeFromLeft (104));
        row1.removeFromLeft (6);
        mSaveBtn.setBounds (row1.removeFromLeft (44));
        row1.removeFromLeft (3);
        mLoadBtn.setBounds (row1.removeFromLeft (44));
        row1.removeFromLeft (10);
        mSliceBtn.setBounds (row1.removeFromLeft (46));
        row1.removeFromLeft (2);
        mEditBtn.setBounds (row1.removeFromLeft (42));
        row1.removeFromLeft (10);
        mSendBtn.setBounds (row1.removeFromLeft (104));
        // Root / Scale / Snap sit right-aligned, directly above row 2's
        // Undo / Redo stack (owner placement call, 2026-07-11).
        row1.removeFromRight (32);
        mSnapBtn.setBounds (row1.removeFromRight (44));
        row1.removeFromRight (3);
        mScaleCombo.setBounds (row1.removeFromRight (102).reduced (0, 2));
        row1.removeFromRight (2);
        mRootCombo.setBounds (row1.removeFromRight (52).reduced (0, 2));

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
        row2.removeFromRight (10);
        mEngineCombo.setBounds (row2.removeFromRight (210).reduced (0, 2));   // QA-Fe engine dropdown, right of Snapshot
    }

private:
    BaySickPitchEditor& mOwner;
    juce::AudioProcessorValueTreeState& mApvts;

    BaySickEngineLabel mTitleLbl { "BaySickPitch", juce::Colour (0xFF0FAFA5) };
    juce::TextButton mSaveBtn, mLoadBtn, mSliceBtn, mEditBtn, mResetBtn,
                     mRenderBtn, mSendBtn, mSnapshotBtn, mVersionsBtn,
                     mUndoBtn, mRedoBtn, mScrollBtn, mSnapBtn;
    juce::ComboBox   mRootCombo, mScaleCombo, mEngineCombo;
    bool             mEnginePopupArmed { false };
    juce::ToggleButton mOnToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mOnAtt, mSnapAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mRootAtt, mScaleAtt, mEngineAtt;
    BaySickSlider       mFocus, mMod, mSpeed, mThroat;
    std::unique_ptr<TaggedSliderAttachment> mFocusAtt, mModAtt, mSpeedAtt, mThroatAtt;
    juce::String mLengthText, mAnalysisBadge;
    bool mStale { false }, mStalePending { false }, mPlayGated { false };

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

    mHScroll = std::make_unique<juce::ScrollBar> (false);   // horizontal (seconds)
    mHScroll->setAutoHide (false);
    mHScroll->addListener (this);
    addAndMakeVisible (*mHScroll);
    mVScroll = std::make_unique<juce::ScrollBar> (true);    // vertical (MIDI note)
    mVScroll->setAutoHide (false);
    mVScroll->addListener (this);
    addAndMakeVisible (*mVScroll);

    setWantsKeyboardFocus (true);
    // 30 Hz: playhead + auto-scroll follow the main transport smoothly; the
    // poll-ish work (stale badges, length readout, diag) runs on a divided
    // slow path (#7 rate note -- the old 400 ms timer was too coarse).
    startTimerHz (30);
}

BaySickPitchEditor::~BaySickPitchEditor()
{
    stopPreview();
}

void BaySickPitchEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
}

void BaySickPitchEditor::resized()
{
    auto b = getLocalBounds();
    mToolbar->setBounds (b.removeFromTop (kToolbarH));
    mInfoBar->setBounds (b.removeFromBottom (kInfoBarH));

    // Reserve a gutter on the right (V bar) + bottom (H bar); the keyboard is
    // trimmed to the shortened canvas height so its lanes stay aligned.
    const int cx = b.getX() + kKeyboardW;
    const int cy = b.getY();
    const int canvasW = juce::jmax (1, b.getWidth()  - kKeyboardW - kScrollBarSz);
    const int canvasH = juce::jmax (1, b.getHeight() - kScrollBarSz);

    mKeyboard->setBounds (b.getX(), cy + kRulerH, kKeyboardW, juce::jmax (1, canvasH - kRulerH));
    mCanvas  ->setBounds (cx, cy, canvasW, canvasH);
    if (mVScroll) mVScroll->setBounds (cx + canvasW, cy, kScrollBarSz, canvasH);
    if (mHScroll) mHScroll->setBounds (cx, cy + canvasH, canvasW, kScrollBarSz);

    pushScrollStateToBars();
}

void BaySickPitchEditor::visibilityChanged()
{
    // Composite auto-resolve (no manual load): first show, or a stale grid,
    // re-runs the analysis (QA-Fd 4a: first analysis even mid-play).
    if (isVisible())
        runAnalyzeIfNeeded (false);
}

bool BaySickPitchEditor::keyPressed (const juce::KeyPress& k)
{
    const bool ctrl  = k.getModifiers().isCtrlDown();
    const bool shift = k.getModifiers().isShiftDown();
    const int  kc    = k.getKeyCode();

    if (! ctrl && ! shift && (k.getTextCharacter() == 'a' || k.getTextCharacter() == 'A'))
    {
        mAutoScroll = ! mAutoScroll;
        mToolbar->mScrollBtn.setToggleState (mAutoScroll, juce::dontSendNotification);
        return true;
    }
    if (ctrl && ! shift)
    {
        if (kc == 'A' || kc == 'a')
        {
            mSelection.clear();
            for (int i = 0; i < (int) mProc.mPitch.regions().size(); ++i)
                mSelection.insert (i);
            if (! mSelection.empty()) mFocusRegion = *mSelection.begin();
            mCanvas->repaint();
            return true;
        }
        if (kc == 'C' || kc == 'c') { copySelection();    return true; }
        if (kc == 'V' || kc == 'v') { pasteToSelection(); return true; }
        if (kc == 'B' || kc == 'b')
        {
            // Family "duplicate" analog for analysis pills: duplicate the
            // FOCUSED pill's treatment across the selection (one undo step).
            copySelection();
            pasteToSelection();
            return true;
        }
    }
    if (shift && ! ctrl)
    {
        if (k.isKeyCode (juce::KeyPress::upKey))    { nudgeSelection (+0.1f); return true; }
        if (k.isKeyCode (juce::KeyPress::downKey))  { nudgeSelection (-0.1f); return true; }
        if (k.isKeyCode (juce::KeyPress::leftKey))  { nudgeSelectionTime (-0.01); return true; }
        if (k.isKeyCode (juce::KeyPress::rightKey)) { nudgeSelectionTime (+0.01); return true; }
    }
    if (! ctrl && (k.isKeyCode (juce::KeyPress::deleteKey)
                   || k.isKeyCode (juce::KeyPress::backspaceKey)))
    {
        // Pills are analysis segments, not deletable objects: Delete =
        // Restore to Original State (locked Task 6 semantics).
        deleteSelection();
        return true;
    }
    if (k.isKeyCode (juce::KeyPress::escapeKey))
    {
        if (mSubWin != nullptr) { closeSubEditor(); return true; }
        clearSelection();
        mCanvas->repaint();
        return true;
    }
    return false;
}

void BaySickPitchEditor::setView (double pps, double scrollSec)
{
    // Zoom spans EXACTLY match the piano roll: full zoom-IN shows 0.5 beat across
    // the canvas; full zoom-OUT shows max(canvasW/160 baseline, content + 1 bar) bars.
    const double canvasW = (double) juce::jmax (1, mCanvas != nullptr ? mCanvas->getWidth() : 800);
    const double compSec = mProc.mPitch.compositeSec();
    double secPerBeat = 0.5;   // 120 BPM fallback
    if (TempoMap::isActive())
    {
        const double sr = juce::jmax (1.0, mProc.mPitch.analysisSampleRate());
        secPerBeat = juce::jmax (1.0e-3,
            (double) (TempoMap::sampleAtBeat (1.0) - TempoMap::sampleAtBeat (0.0)) / sr);
    }
    const double secPerBar   = 4.0 * secPerBeat;
    const double contentBars = compSec / juce::jmax (1.0e-6, secPerBar);
    // Zoom-OUT bar count = the piano roll's exact minZoomPPB formula: the empty
    // baseline (canvasW / kDefaultPianoRollEmptyPx px-per-bar, monitor-dependent) OR
    // content + kPianoRollZoomPadBars, whichever is larger.  For a short composite the
    // baseline wins (~7 bars) -- an earlier version dropped it and zoomed out too far.
    const double maxBars = juce::jmax (canvasW / (double) kDefaultPianoRollEmptyPx,
                                       contentBars + (double) kPianoRollZoomPadBars);
    const double maxPps  = canvasW / ((double) kMaxZoomInBeatsAcross * secPerBeat);  // 0.5 beat across (in)
    const double minPps  = canvasW / juce::jmax (1.0e-3, maxBars * secPerBar);       // maxBars bars across (out)
    mPps    = juce::jlimit (minPps, maxPps, pps);
    mScroll = juce::jmax (0.0, scrollSec);
    pushScrollStateToBars();
    repaint();
}

void BaySickPitchEditor::setTopNote (int note)
{
    // Floor: the view bottom never dips below C0 (#6).
    // Lane area = canvas MINUS the ruler (lanes draw from y=kRulerH), matching
    // pushScrollStateToBars so the clamp + V-scrollbar thumb agree -- otherwise a
    // ruler-height sliver of grey shows below C0.
    const int visLanes = (int) std::ceil (
        (double) juce::jmax (60, (mCanvas != nullptr ? mCanvas->getHeight() : 400) - kRulerH)
        / juce::jmax (2.0, mLaneH));
    const int minTop = juce::jmin (120, kMinNote + visLanes);
    mTopNote = juce::jlimit (minTop, 120, note);
    pushScrollStateToBars();
    if (mKeyboard) mKeyboard->repaint();
    if (mCanvas)   mCanvas->repaint();
}

void BaySickPitchEditor::setLaneHeight (double laneH, double anchorMidi, int anchorY)
{
    // Zoom spans EXACTLY match the piano roll's applyVZoom: it divides the FULL grid
    // height (gridHf, ruler included) by 48 (out) / 12 (in), so mirror that with the
    // full canvas height -- NOT canvasH-kRulerH, which fit ~2 extra lanes below the
    // ruler and read as slightly more zoomed-out than the roll.  (48 < the 108-lane
    // C0..120 range, so the range always over-fills the height -> no grey below C0.)
    const double laneArea = (double) juce::jmax (60, mCanvas != nullptr ? mCanvas->getHeight() : 400);
    const double minLaneH = laneArea / 48.0;   // 48 notes (out)
    const double maxLaneH = laneArea / 12.0;   // 12 notes (in)
    mLaneH = juce::jlimit (minLaneH, maxLaneH, laneH);
    // Keep the note under the cursor stationary (cursor-anchored v-zoom).
    setTopNote ((int) std::round (anchorMidi + (double) (anchorY - kRulerH) / mLaneH));
}

// Push the view state to the scroll bars (piano-roll parity).  Every view
// mutation funnels through setView/setTopNote, so this keeps the thumbs live.
// Writes use dontSendNotification (+ the mPushingToBars guard) so it never
// re-enters scrollBarMoved.
void BaySickPitchEditor::pushScrollStateToBars()
{
    if (mCanvas == nullptr || mHScroll == nullptr || mVScroll == nullptr) return;
    mPushingToBars = true;

    // Horizontal (seconds).  Extend past the composite end by the current view so
    // a cursor-zoom past the end stays representable + can't snap back to 0.
    const double viewSec  = (double) juce::jmax (1, mCanvas->getWidth()) / juce::jmax (1.0, mPps);
    const double compSec  = mProc.mPitch.compositeSec();
    const double totalSec = juce::jmax (compSec, mScroll + viewSec);
    const double visSec   = juce::jmin (totalSec, viewSec);
    mHScroll->setRangeLimits (0.0, totalSec);
    mHScroll->setCurrentRange (juce::jlimit (0.0, juce::jmax (0.0, totalSec - visSec), mScroll),
                               visSec, juce::dontSendNotification);

    // Vertical (MIDI note, inverted: high MIDI at the top; range kMinNote..120).
    const double lanes      = (double) juce::jmax (1, mCanvas->getHeight() - kRulerH)
                                / juce::jmax (2.0, mLaneH);
    const double totalNotes = 120.0 - (double) kMinNote;
    const double sv         = 120.0 - (double) mTopNote;
    mVScroll->setRangeLimits (0.0, totalNotes);
    mVScroll->setCurrentRange (juce::jlimit (0.0, juce::jmax (0.0, totalNotes - lanes), sv),
                               lanes, juce::dontSendNotification);

    mPushingToBars = false;
}

void BaySickPitchEditor::scrollBarMoved (juce::ScrollBar* sb, double newStart)
{
    if (mPushingToBars || sb == nullptr) return;
    if (sb == mHScroll.get())
        setView (mPps, juce::jmax (0.0, newStart));
    else if (sb == mVScroll.get())
        setTopNote (120 - (int) std::round (newStart));
}

void BaySickPitchEditor::zoomToSelection()
{
    if (mSelection.empty()) return;
    mCanvas->zoomToRegions (selectionOrFocus());
}

void BaySickPitchEditor::restoreSavedView()
{
    if (! mHasSavedView) return;
    mHasSavedView = false;
    mLaneH = mSavedLaneH;
    setView (mSavedPps, mSavedScroll);
    setTopNote (mSavedTopNote);
}

// ─── Selection ────────────────────────────────────────────────────────────────
void BaySickPitchEditor::selectOnly (int idx)
{
    mSelection.clear();
    if (idx >= 0) mSelection.insert (idx);
    mFocusRegion = idx;
    mCanvas->repaint();
}

void BaySickPitchEditor::toggleSelect (int idx)
{
    if (idx < 0) return;
    if (! mSelection.insert (idx).second)
        mSelection.erase (idx);
    mFocusRegion = idx;
    mCanvas->repaint();
}

void BaySickPitchEditor::clearSelection()
{
    mSelection.clear();
    mFocusRegion = -1;
}

std::vector<int> BaySickPitchEditor::selectionOrFocus() const
{
    if (! mSelection.empty())
        return { mSelection.begin(), mSelection.end() };
    if (mFocusRegion >= 0) return { mFocusRegion };
    return {};
}

// ─── Global undo (9a) ─────────────────────────────────────────────────────────
void BaySickPitchEditor::beginEdit()
{
    mPendingBefore = mProc.mPitch.regions();
    mEditPending   = true;
}

void BaySickPitchEditor::cancelEdit()
{
    mEditPending = false;
    mPendingBefore.clear();
}

void BaySickPitchEditor::commitEdit (const juce::String& label)
{
    if (! mEditPending) return;
    mEditPending = false;

    mProc.mPitch.publishEdits();
    updateInfoBarFor (mFocusRegion);

    if (! mUndoCtx.isValid())
        { mPendingBefore.clear(); return; }

    juce::Component::SafePointer<BaySickPitchEditor> safe (this);
    mUndoCtx.perform (
        new PitchEditAction (label, std::move (mPendingBefore),
                             mProc.mPitch.regions(),
                             [safe] (const std::vector<PitchNoteRegion>& regions)
                             {
                                 // Tab closed = editor gone: the SafePointer
                                 // degrades the action to a safe no-op (the
                                 // processor died with its page too).
                                 if (safe != nullptr)
                                     safe->applyRegionsFromUndo (regions);
                             }),
        label);
    mPendingBefore.clear();
}

void BaySickPitchEditor::applyRegionsFromUndo (const std::vector<PitchNoteRegion>& regions)
{
    mProc.mPitch.regions() = regions;
    mProc.mPitch.publishEdits();
    clearSelection();
    // #9: the InfoBar refreshes on undo/redo so the [edited] tag is truthful.
    updateInfoBarFor (-1);
    mCanvas->repaint();
}

// ─── Timer (30 Hz) ────────────────────────────────────────────────────────────
void BaySickPitchEditor::timerCallback()
{
    const bool playing = DSPBase::isTransportPlaying();

    // ── Playhead: main transport beat -> composite seconds (#7) ────────────
    double newPlayhead = -1.0;
    if (mProc.onTransportBeat && mProc.mPitch.isAnalyzed())
    {
        const double beat = mProc.onTransportBeat();
        const double devSr = (mProc.getSampleRate() > 0.0)
                               ? mProc.getSampleRate() : 44100.0;
        const double tlSample = TempoMap::isActive()
            ? (double) TempoMap::sampleAtBeat (beat)
            : beat * 0.5 * devSr;   // 120 BPM legacy fallback
        newPlayhead = tlSample / devSr
                    - (double) mProc.mPitch.startSample()
                        / mProc.mPitch.analysisSampleRate();
        if (newPlayhead < 0.0 || newPlayhead > mProc.mPitch.compositeSec())
            newPlayhead = -1.0;
    }
    if (std::abs (newPlayhead - mPlayheadSec) > 1.0e-4)
    {
        const double prev = mPlayheadSec;
        mPlayheadSec = newPlayhead;

        // Auto-scroll (#8 fix): edge-triggered page-flip ONLY while the
        // playhead ADVANCES across the right edge during playback -- never
        // recenters on a stale/frozen position, never fights manual scroll.
        if (mAutoScroll && playing && mPlayheadSec >= 0.0 && prev >= 0.0
            && mPlayheadSec > prev)
        {
            const double viewSec = juce::jmax (1, mCanvas->getWidth()) / mPps;
            if (mPlayheadSec > mScroll + viewSec * 0.98)
                setView (mPps, mPlayheadSec - viewSec * 0.1);
        }
        mCanvas->syncRulerSelFromBuilder();   // reflect builder-set range on the pitch ruler
        mCanvas->repaint();
    }

    // Focus / Snap / Root / Scale move the RENDERED pill positions (the
    // Focus pull is paint-time math), and the Slice / Edit buttons mirror
    // bsp_mode -- without this fast-path watch, knob turns while stopped
    // leave the canvas visually frozen (2026-07-11 owner find).
    {
        const float sig = paramValue ("bsp_focus")
                        + paramValue ("bsp_snap")  * 1000.0f
                        + paramValue ("bsp_root")  * 4000.0f
                        + paramValue ("bsp_scale") * 100000.0f
                        + paramValue ("bsp_mode", 1.0f) * 2000000.0f;
        if (sig != mRenderSig)
        {
            mRenderSig = sig;
            mToolbar->mirrorMode ((int) paramValue ("bsp_mode", 1.0f));
            mCanvas->repaint();
        }
    }

    // ── Slow path (~2 Hz) ───────────────────────────────────────────────────
    if (++mSlowTick < 15) return;
    mSlowTick = 0;

    const bool stale = mProc.isPitchStale();
    mToolbar->setStale (stale, stale && playing);
    mToolbar->setPlaybackGate (playing);
    mToolbar->setUndoRedoEnabled (
        mUndoCtx.isValid() && mUndoCtx.manager->canUndo(),
        mUndoCtx.isValid() && mUndoCtx.manager->canRedo());

    // QA-Fd 4a state upkeep: a deferred analysis fires as soon as the
    // transport stops; Failed clears once something analyzes cleanly.
    if (mAnalysisState == AnalysisState::Deferred && ! playing && isVisible())
        runAnalyzeIfNeeded (false);
    else if (mAnalysisState == AnalysisState::Failed
             && mProc.mPitch.isAnalyzed() && ! stale)
        setAnalysisState (AnalysisState::Idle, {});

    // LENGTH readout: composite (or SEL) length as `X bars / M:SS.f`.
    {
        auto& pitch = mProc.mPitch;
        juce::String t;
        if (pitch.isAnalyzed() && pitch.compositeSec() > 0.0)
        {
            double secA = 0.0, secB = pitch.compositeSec();
            juce::String prefix;
            if (mFocusRegion >= 0 && mFocusRegion < (int) pitch.regions().size()
                && ! mSelection.empty())
            {
                const auto& r = pitch.regions()[(size_t) mFocusRegion];
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
    if (mCanvas != nullptr && mCanvas->mHandleReadout.isNotEmpty()
        && mCanvas->mDragKind == PitchCanvas::DragKind::Handle)
    {
        mInfoBar->setText (mCanvas->mHandleReadout);
        return;
    }
    if (r.isSlice)
    {
        mInfoBar->setText (juce::String ("Slice")
                           + "  Length: " + juce::String (r.endSec - r.startSec, 2) + "s"
                           + (r.hasTimeEdit() ? "  [moved]" : "")
                           + (r.hasEdits() ? "  [edited]" : ""));
        return;
    }
    const float edited = r.midi + r.shiftSemis;
    const int   nearest = (int) std::round (edited);
    const int   cents   = (int) std::round ((edited - (float) nearest) * 100.0f);
    mInfoBar->setText ("Pitch: " + midiNoteName (nearest)
                       + "  Cents: " + (cents >= 0 ? "+" : "") + juce::String (cents)
                       + "  Length: " + juce::String (r.endSec - r.startSec, 2) + "s"
                       + (r.detached ? "  [detached]" : "")
                       + (r.hasTimeEdit() ? "  [moved]" : "")
                       + (r.hasEdits() ? "  [edited]" : ""));
}

// ─── Analysis flow (QA-Fd 4a) ─────────────────────────────────────────────────
void BaySickPitchEditor::setAnalysisState (AnalysisState s, const juce::String& err)
{
    if (mAnalysisState == s && mAnalysisError == err) return;
    mAnalysisState = s;
    mAnalysisError = err;
    mToolbar->setAnalysisBadge (s == AnalysisState::Analyzing ? "ANALYZING..."
                              : s == AnalysisState::Deferred  ? "DEFERRED UNTIL STOP"
                              : s == AnalysisState::Failed    ? "ANALYSIS FAILED"
                                                              : juce::String());
    mCanvas->repaint();
}

void BaySickPitchEditor::runAnalyzeIfNeeded (bool force)
{
    const bool analyzed = mProc.mPitch.isAnalyzed();
    if (! force && analyzed && ! mProc.isPitchStale())
    {
        setAnalysisState (AnalysisState::Idle, {});
        if (! mCompValid) refreshComposite();
        return;
    }
    // QA-Fd 4a: RE-analysis stays stop-gated (it swaps an applied law
    // mid-play); a NEVER-analyzed channel analyzes immediately even during
    // playback -- no edits exist, so the fresh snapshot is a provable no-op,
    // and the felt "pitch needs Align first" coupling dies with the silent
    // empty canvas.
    if (DSPBase::isTransportPlaying() && analyzed)
    {
        setAnalysisState (AnalysisState::Deferred, {});
        if (! mCompValid) refreshComposite();
        return;
    }
    // Review fix: visibility + timer (or a double retrigger) could queue two
    // deferred analyses -- one in flight at a time.
    if (mAnalyzeQueued) return;
    mAnalyzeQueued = true;

    setAnalysisState (AnalysisState::Analyzing, {});
    // One paint pass before the synchronous analysis so the ANALYZING state
    // is actually visible (analysis can take seconds on long channels -- the
    // NIT-4 hitch rides the owner's judgment at the boundary listen).
    juce::Component::SafePointer<BaySickPitchEditor> self (this);
    juce::Timer::callAfterDelay (30, [self]
    {
        if (! self) return;
        self->mAnalyzeQueued = false;
        juce::String err;
        if (self->mProc.analyzePitch (err))
        {
            self->setAnalysisState (AnalysisState::Idle, {});
            self->refreshComposite();
            self->clearSelection();
        }
        else
            self->setAnalysisState (AnalysisState::Failed, err);
        self->mCanvas->repaint();
    });
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

// ─── Actions ──────────────────────────────────────────────────────────────────
void BaySickPitchEditor::runRender()
{
    // QA-Fe2 item 2 (Jeff 2a): the High Resolution choice is RETIRED here --
    // it oversampled the applyWarp phase-vocoder pass, which QA-Fe Option A
    // removed from the pitch path (engines warp natively at full quality), so
    // the choice had become a no-op.  BaySickAlign's High-Res render is real
    // (oversampled PV warp) and keeps its dialog.
    auto* aw = new juce::AlertWindow ("Render",
        "Bake the edited channel to Pitched/{name}_pitch_v{N}.wav "
        "(file only - playback is already live).",
        juce::AlertWindow::NoIcon);
    aw->addButton ("Render", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<BaySickPitchEditor> self (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [self] (int r)
        {
            if (! self || r == 0) return;
            juce::String err;
            const auto file = self->mProc.renderPitchedTake (err);
            if (file == juce::File())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                    "Render", err);
            else
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                    "Render", "Baked to " + file.getFileName());
        }), true);
}

void BaySickPitchEditor::runReset()
{
    beginEdit();
    mProc.mPitch.clearAllEdits();
    commitEdit ("Pitch: Clear All Edits");
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
            self->beginEdit();
            if (! self->mProc.revertPitchToVersion (result - 1))
                { self->cancelEdit(); return; }
            self->commitEdit ("Pitch: Revert Version");
            self->clearSelection();
            self->mCanvas->repaint();
        });
}

void BaySickPitchEditor::showSendNotesMenu()
{
    // QA-Layout T4: injected providers replace the findParentComponentOfClass
    // escape -- inside a contained window the parent chain never reaches the
    // app editor.
    if (! onListNoteTargets || ! onSendNotes) return;

    const auto targets = onListNoteTargets();
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
            if (! self->onSendNotes) return;

            // MIDI only: the detected contour quantized to notes, normalized
            // so the first note starts the riff.  Slice pills are skipped
            // (no pitch identity).
            std::vector<SentNote> notes;
            const auto& regions = self->mProc.mPitch.regions();
            double t0 = -1.0;
            for (const auto& reg : regions)
            {
                if (reg.isSlice) continue;
                if (t0 < 0.0) t0 = reg.dstStart();
                SentNote n;
                n.startSec = reg.dstStart() - t0;
                n.endSec   = reg.dstEnd()   - t0;
                n.midiNote = juce::jlimit (0, 127,
                    (int) std::round (reg.midi + reg.shiftSemis));
                notes.push_back (n);
            }
            if (notes.empty()) return;
            const auto& tgt = targets[(size_t) (r - 1)];
            self->onSendNotes (tgt.kind, tgt.pageIndex, notes);
        });
}

// ─── Pill menu (18a) + pill operations ───────────────────────────────────────
void BaySickPitchEditor::showPillMenu (int idx)
{
    auto& regions = mProc.mPitch.regions();
    if (idx < 0 || idx >= (int) regions.size()) return;
    const bool slice = regions[(size_t) idx].isSlice;
    const bool multi = (int) mSelection.size() > 1;

    juce::PopupMenu m;
    m.addItem (1, multi ? "Restore Selected to Original State"
                        : "Restore to Original State");
    if (! slice)
        m.addItem (2, multi ? "Snap Selected to Semitone" : "Snap to Semitone");
    if (! slice)
        m.addItem (7, multi ? "Force Selected to Scale" : "Force to Scale",
                   (int) paramValue ("bsp_scale") > 0);
    m.addSeparator();
    if (multi)
        m.addItem (3, "Merge Selected Pills");
    else if (idx + 1 < (int) regions.size())
        m.addItem (4, "Merge With Next Pill");
    if (! slice)
        m.addItem (5, "Open Sub-Editor...");
    if (! mSelection.empty())
        m.addItem (6, "Zoom to Selection");
    m.addSeparator();
    m.addItem (8, slice ? "Include in Pitch Correction"
                        : "Exclude from Pitch Correction");

    juce::Component::SafePointer<BaySickPitchEditor> self (this);
    m.showMenuAsync (juce::PopupMenu::Options(),
        [self, idx] (int r)
        {
            if (! self || r <= 0) return;
            switch (r)
            {
                case 1: self->restoreToOriginal (self->selectionOrFocus()); break;
                case 2: self->snapToSemitone (self->selectionOrFocus());    break;
                case 3: self->mergeSelection();                              break;
                case 4:
                    self->mSelection = { idx, idx + 1 };
                    self->mergeSelection();
                    break;
                case 5: self->openSubEditor (idx);                           break;
                case 6: self->zoomToSelection();                             break;
                case 7: self->forceSelectionToScale (self->selectionOrFocus()); break;
                case 8: self->toggleSliceExcluded (idx);                     break;
                default: break;
            }
        });
}

void BaySickPitchEditor::restoreToOriginal (const std::vector<int>& idxs)
{
    if (idxs.empty()) return;
    auto& regions = mProc.mPitch.regions();
    beginEdit();
    for (int i : idxs)
    {
        if (i < 0 || i >= (int) regions.size()) continue;
        auto& r = regions[(size_t) i];
        r.shiftSemis = 0.0f; r.formantSemis = 0.0f;
        r.vibDepthMult = 1.0f; r.vibRateMult = 1.0f; r.variation = 1.0f;
        r.volShape.clear(); r.pitchShape.clear();
        r.dstStartSec = -1.0; r.dstEndSec = -1.0; r.detached = false;
    }
    commitEdit ("Pitch: Restore to Original");
    mCanvas->repaint();
}

void BaySickPitchEditor::toggleSliceExcluded (int idx)
{
    auto& regions = mProc.mPitch.regions();
    if (idx < 0 || idx >= (int) regions.size()) return;
    beginEdit();
    regions[(size_t) idx].isSlice = ! regions[(size_t) idx].isSlice;
    commitEdit ("Pitch: Toggle Slice");
    mCanvas->repaint();
}

void BaySickPitchEditor::snapToSemitone (const std::vector<int>& idxs)
{
    if (idxs.empty()) return;
    auto& regions = mProc.mPitch.regions();
    beginEdit();
    for (int i : idxs)
    {
        if (i < 0 || i >= (int) regions.size()) continue;
        auto& r = regions[(size_t) i];
        if (r.isSlice) continue;
        r.shiftSemis = std::round (r.midi + r.shiftSemis) - r.midi;
    }
    commitEdit ("Pitch: Snap to Semitone");
    mCanvas->repaint();
}

void BaySickPitchEditor::forceSelectionToScale (const std::vector<int>& idxs)
{
    const int scale = (int) paramValue ("bsp_scale");
    if (scale <= 0) return;                        // Chromatic: nothing is out of scale
    const int root  = (int) paramValue ("bsp_root");
    const auto& mask = PitchCorrectorDSP::scaleMask (scale);
    auto& regions = mProc.mPitch.regions();
    bool any = false;
    beginEdit();
    for (int i : idxs)
    {
        if (i < 0 || i >= (int) regions.size()) continue;
        auto& r = regions[(size_t) i];
        if (r.isSlice) continue;
        const float center    = r.midi + r.shiftSemis;
        const int   centerInt = (int) std::round (center);
        const int   rel       = ((centerInt - root) % 12 + 12) % 12;
        if (mask[(size_t) rel]) continue;          // already in scale -> keep its cents
        const float lane = PitchCorrectorDSP::snapMidiToScaleStatic (center, root, scale);
        r.shiftSemis = juce::jlimit (-24.0f, 24.0f, lane - r.midi);
        any = true;
    }
    if (any) commitEdit ("Pitch: Force to Scale");  // publishEdits() re-bakes
    else     cancelEdit();
    if (mCanvas) mCanvas->repaint();
}

void BaySickPitchEditor::mergeSelection()
{
    auto idxs = selectionOrFocus();
    if ((int) idxs.size() < 2) return;
    std::sort (idxs.begin(), idxs.end());
    auto& regions = mProc.mPitch.regions();
    // Review fix: the pill menu is async -- a deferred re-analysis can
    // shrink the region list between menu-open and click.
    if (idxs.front() < 0 || idxs.back() >= (int) regions.size()) return;
    for (size_t k = 0; k + 1 < idxs.size(); ++k)
        if (idxs[k + 1] != idxs[k] + 1) return;   // adjacent runs only

    beginEdit();
    // Keep the LONGEST member's pitch identity + edits; absorb src + dst
    // spans (gaps between members fold in).
    int longest = idxs.front();
    for (int i : idxs)
        if (regions[(size_t) i].endSec - regions[(size_t) i].startSec
            > regions[(size_t) longest].endSec - regions[(size_t) longest].startSec)
            longest = i;

    PitchNoteRegion merged = regions[(size_t) longest];
    const auto& first = regions[(size_t) idxs.front()];
    const auto& last  = regions[(size_t) idxs.back()];
    merged.startSec = first.startSec;
    merged.endSec   = last.endSec;
    if (first.dstStartSec >= 0.0 || last.dstStartSec >= 0.0
        || merged.dstStartSec >= 0.0)
    {
        merged.dstStartSec = first.dstStart();
        merged.dstEndSec   = last.dstEnd();
    }
    merged.isSlice = std::all_of (idxs.begin(), idxs.end(),
        [&] (int i) { return regions[(size_t) i].isSlice; });

    regions.erase (regions.begin() + idxs.front(),
                   regions.begin() + idxs.back() + 1);
    regions.insert (regions.begin() + idxs.front(), merged);
    commitEdit ("Pitch: Merge Pills");
    selectOnly (idxs.front());
}

void BaySickPitchEditor::batchRepitchTo (int midiNote)
{
    auto idxs = selectionOrFocus();
    if (idxs.empty()) return;
    auto& regions = mProc.mPitch.regions();
    beginEdit();
    for (int i : idxs)
    {
        if (i < 0 || i >= (int) regions.size()) continue;
        auto& r = regions[(size_t) i];
        if (r.isSlice) continue;
        r.shiftSemis = juce::jlimit (-24.0f, 24.0f, (float) midiNote - r.midi);
    }
    commitEdit ("Pitch: Re-pitch to " + midiNoteName (midiNote));
    mCanvas->repaint();
}

void BaySickPitchEditor::copySelection()
{
    const auto& regions = mProc.mPitch.regions();
    if (mFocusRegion < 0 || mFocusRegion >= (int) regions.size()) return;
    const auto& r = regions[(size_t) mFocusRegion];
    mClipboard.valid        = true;
    mClipboard.shiftSemis   = r.shiftSemis;
    mClipboard.formantSemis = r.formantSemis;
    mClipboard.vibDepthMult = r.vibDepthMult;
    mClipboard.vibRateMult  = r.vibRateMult;
    mClipboard.variation    = r.variation;
    mClipboard.volShape     = r.volShape;
    mClipboard.pitchShape   = r.pitchShape;
}

void BaySickPitchEditor::pasteToSelection()
{
    if (! mClipboard.valid) return;
    auto idxs = selectionOrFocus();
    if (idxs.empty()) return;
    auto& regions = mProc.mPitch.regions();
    beginEdit();
    for (int i : idxs)
    {
        if (i < 0 || i >= (int) regions.size()) continue;
        auto& r = regions[(size_t) i];
        if (! r.isSlice)
        {
            r.shiftSemis   = mClipboard.shiftSemis;
            r.formantSemis = mClipboard.formantSemis;
            r.vibDepthMult = mClipboard.vibDepthMult;
            r.vibRateMult  = mClipboard.vibRateMult;
            r.variation    = mClipboard.variation;
            r.pitchShape   = mClipboard.pitchShape;
        }
        r.volShape = mClipboard.volShape;
    }
    commitEdit ("Pitch: Paste Edits");
    mCanvas->repaint();
}

void BaySickPitchEditor::nudgeSelection (float semis)
{
    auto idxs = selectionOrFocus();
    if (idxs.empty()) return;
    auto& regions = mProc.mPitch.regions();
    beginEdit();
    for (int i : idxs)
        if (i >= 0 && i < (int) regions.size() && ! regions[(size_t) i].isSlice)
            regions[(size_t) i].shiftSemis = juce::jlimit (-24.0f, 24.0f,
                regions[(size_t) i].shiftSemis + semis);
    commitEdit ("Pitch: Nudge");
    mCanvas->repaint();
}

void BaySickPitchEditor::nudgeSelectionTime (double dSec)
{
    auto idxs = selectionOrFocus();
    if (idxs.empty()) return;
    auto& regions = mProc.mPitch.regions();

    // Collective elastic clamp: pills never cross non-selected neighbors
    // (same rule as the drag; detached pills keep their freedom implicitly
    // since a nudge is elastic by definition).
    double lo = -1.0e18, hi = 1.0e18;
    for (int i : idxs)
    {
        if (i < 0 || i >= (int) regions.size()) continue;
        const auto& r = regions[(size_t) i];
        double prevB = 0.0, nextB = 1.0e18;
        for (int j = 0; j < (int) regions.size(); ++j)
        {
            if (j == i || isSelected (j)) continue;
            const auto& o = regions[(size_t) j];
            if (o.dstEnd() <= r.dstStart() + 1.0e-9)
                prevB = juce::jmax (prevB, o.dstEnd());
            if (o.dstStart() >= r.dstEnd() - 1.0e-9)
                nextB = juce::jmin (nextB, o.dstStart());
        }
        lo = juce::jmax (lo, juce::jmax (prevB - r.dstStart(), -r.dstStart()));
        hi = juce::jmin (hi, nextB - r.dstEnd());
    }
    dSec = juce::jlimit (juce::jmin (0.0, lo), juce::jmax (0.0, hi), dSec);
    if (std::abs (dSec) < 1.0e-9) return;

    beginEdit();
    for (int i : idxs)
    {
        if (i < 0 || i >= (int) regions.size()) continue;
        auto& r = regions[(size_t) i];
        const double len = r.dstEnd() - r.dstStart();
        const double s = juce::jmax (0.0, r.dstStart() + dSec);
        r.dstStartSec = s;
        r.dstEndSec   = s + len;
    }
    commitEdit ("Pitch: Nudge Time");
    mCanvas->repaint();
}

void BaySickPitchEditor::deleteSelection()
{
    restoreToOriginal (selectionOrFocus());
}

void BaySickPitchEditor::resetBoxedParams (const std::vector<int>& idxs)
{
    if (idxs.empty()) return;

    auto doReset = [this, idxs]
    {
        auto& regions = mProc.mPitch.regions();
        beginEdit();
        for (int i : idxs)
        {
            if (i < 0 || i >= (int) regions.size()) continue;
            auto& r = regions[(size_t) i];
            // 15b: boxed params only -- pitch offset + time edits KEPT.
            r.formantSemis = 0.0f;
            r.vibDepthMult = 1.0f; r.vibRateMult = 1.0f; r.variation = 1.0f;
            r.volShape.clear(); r.pitchShape.clear();
        }
        commitEdit ("Pitch: Reset Sub-Edits");
        mCanvas->repaint();
    };

    if ((int) idxs.size() <= 1) { doReset(); return; }

    auto prefs = openUiPrefs();
    if (prefs->getBoolValue ("pitchMultiResetNoPrompt", false))
        { doReset(); return; }

    // Multi-select reset warning + never-show-again (settings-persisted).
    auto* aw = new juce::AlertWindow ("Reset Sub-Edits",
        "This clears the vibrato / formant / variation values and the volume "
        "and pitch curves of " + juce::String ((int) idxs.size())
        + " selected pills.",
        juce::AlertWindow::WarningIcon);
    if (mResetPromptCheck == nullptr)
    {
        mResetPromptCheck = std::make_unique<juce::ToggleButton> (
            "Do not show this again");
        mResetPromptCheck->setSize (220, 22);
    }
    mResetPromptCheck->setToggleState (false, juce::dontSendNotification);
    aw->addCustomComponent (mResetPromptCheck.get());
    aw->addButton ("Reset",  1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<BaySickPitchEditor> self (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [self, doReset, aw] (int r)
        {
            if (! self) return;
            if (self->mResetPromptCheck != nullptr
                && self->mResetPromptCheck->getToggleState())
            {
                auto prefs2 = openUiPrefs();
                prefs2->setValue ("pitchMultiResetNoPrompt", true);
                prefs2->saveIfNeeded();
            }
            juce::ignoreUnused (aw);
            if (r == 1) doReset();
        }), true);
}

// ─── QA-Fe: WORLD-is-offline notice ──────────────────────────────────────────
void BaySickPitchEditor::showWorldOfflineNotice()
{
    if (openUiPrefs()->getBoolValue ("pitchWorldOfflineNoPrompt", false))
        return;

    auto* aw = new juce::AlertWindow ("WORLD works offline",
        "WORLD is the highest-quality pitch engine, but it processes offline: "
        "edits you make while audio is playing apply a moment later, not live.  "
        "For instant edits during playback, use Rubber Band or Signalsmith.",
        juce::AlertWindow::InfoIcon);
    if (mWorldPromptCheck == nullptr)
    {
        mWorldPromptCheck = std::make_unique<juce::ToggleButton> ("Do not show this again");
        mWorldPromptCheck->setSize (220, 22);
    }
    mWorldPromptCheck->setToggleState (false, juce::dontSendNotification);
    aw->addCustomComponent (mWorldPromptCheck.get());
    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    juce::Component::SafePointer<BaySickPitchEditor> self (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [self, aw] (int)
        {
            if (! self) return;
            if (self->mWorldPromptCheck != nullptr
                && self->mWorldPromptCheck->getToggleState())
            {
                auto prefs = openUiPrefs();
                prefs->setValue ("pitchWorldOfflineNoPrompt", true);
                prefs->saveIfNeeded();
            }
            juce::ignoreUnused (aw);
        }), true);
}

// ─── Sub-editor popup (Task 7) ───────────────────────────────────────────────
void BaySickPitchEditor::openSubEditor (int idx)
{
    auto& regions = mProc.mPitch.regions();
    if (idx < 0 || idx >= (int) regions.size()) return;
    if (regions[(size_t) idx].isSlice) return;

    if (mSubWin == nullptr)
        mSubWin = std::make_unique<BaySickPitchSubEditorWindow> (*this);
    if (auto* content = mSubWin->content())
        content->openFor (idx, selectionOrFocus());
    mSubWin->setVisible (true);
    mSubWin->toFront (true);
}

void BaySickPitchEditor::closeSubEditor()
{
    stopPreview();
    // Reset deletes the floating window; safe when called from its own
    // closeButtonPressed because nothing touches members after the call.
    mSubWin.reset();
}

// ─── Preview play (shared scrub / popup Play) ────────────────────────────────
void BaySickPitchEditor::startRegionPreview (int regionIdx, bool loop)
{
    const auto& regions = mProc.mPitch.regions();
    if (regionIdx < 0 || regionIdx >= (int) regions.size()) return;
    if (! mCompValid) refreshComposite();
    if (! mCompValid) return;
    const auto& r = regions[(size_t) regionIdx];
    // QA-Fe: cache read uses the EDITED span (cache is edited-timeline); the dry
    // fallback uses the raw SOURCE span (== edited when untimed).
    mProc.startPitchPreview (mCompCache, mCompSr,
                             r.startSec, r.endSec, r.dstStart(), r.dstEnd(), loop);
}

void BaySickPitchEditor::stopPreview()
{
    mProc.stopPitchPreview();
}

// ─── User presets (Focus/Mod/Speed) ──────────────────────────────────────────
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
            const juce::String name = aw->getTextEditorContents ("name");

            juce::XmlElement el ("BaySickPitchPreset");
            for (auto* id : { "bsp_focus", "bsp_mod", "bsp_speed" })
                el.setAttribute (id, (double) self->paramValue (id));

            UserFileSave::writeXmlAsync (pitchPresetsDir(), name, el, {});
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
            auto xml = SafeXml::parse (files[r - 1]);
            if (xml == nullptr || ! xml->hasTagName ("BaySickPitchPreset")) return;
            beginParamUndoGesture (self->mProc.apvts, "bsp_focus"); // Task 6 (12-iv)
            for (auto* id : { "bsp_focus", "bsp_mod", "bsp_speed" })
                if (xml->hasAttribute (id))
                    self->setParamValue (id, (float) xml->getDoubleAttribute (id));
        });
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
