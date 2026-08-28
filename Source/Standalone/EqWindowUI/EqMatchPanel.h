// BaySickDAW - the EQ Match panel (QA-EqPro; mid/side + dynamics 2026-08-26).
//
// CURRENT is the strip as it arrives (the pre-EQ feed, averaged over the
// passage you play); REFERENCE is what it should sound like - the strip's
// picked sidechain receive line averaged the same way, or an audio file read
// straight into a spectrum.  Match fits bells to the difference and writes
// them into the bands, replacing what was there.
//
// Both sides are captured in MID and SIDE, because a mono sum cannot say
// whether a problem lives in the center or at the edges - and that is the
// whole point of a mid/side match.  The capture also keeps how far each point
// SWINGS beside its average, so a problem that only shows up on peaks becomes
// a dynamic band instead of a static cut that dulls the other ninety percent.
//
// The fit is DSP/Kbs/EqMatch.h and is proven in the BaySickEqTests target;
// this panel is only capture, preview numbers, and apply.
#pragma once

#include "EqGraphView.h"
#include "../../DSP/Kbs/EqMatch.h"
#include "../../AppPaths.h"

namespace eqview {

class EqMatchPanel : public juce::Component, private juce::Timer
{
public:
    // ONE home: the window positions the panel from these, so growing the
    // panel cannot leave its bottom row clipped off by a stale number.
    static constexpr int kPanelW = 280;
    static constexpr int kPanelH = 246;

    explicit EqMatchPanel (EqGraphView& graphRef) : graph (graphRef)
    {
        auto initButton = [this] (juce::TextButton& b, const juce::String& text)
        {
            b.setButtonText (text);
            addAndMakeVisible (b);
        };

        initButton (captureCur, "Capture Current");
        initButton (loadCur,    "Load Current Export...");
        initButton (captureRef, "Capture Reference (SC)");
        initButton (loadRef,    "Load Reference File...");
        initButton (apply,      "Match");
        initButton (close,      "Close");

        captureCur.setTooltip ("Averages this strip's own sound while you play "
                               "the passage. Press again to stop.");
        loadCur.setTooltip ("Match a whole track without playing it. Needs an "
                            "exported bounce of the track.");
        captureRef.setTooltip ("Averages the picked sidechain receive line - "
                               "route the track you want to sound like into "
                               "one of this strip's receive slots first.");

        captureCur.onClick = [this] { toggleCapture (true); };
        captureRef.onClick = [this] { toggleCapture (false); };
        loadCur.onClick    = [this] { loadIntoSide (true); };
        loadRef.onClick    = [this] { loadIntoSide (false); };
        apply.onClick      = [this] { doMatch(); };
        close.onClick      = [this] { setVisible (false); };

        // ONE control, answering one question: how closely should the match
        // follow the reference?  The two raw numbers it replaces - a smoothing
        // width and a band cap - were implementation details wearing knobs,
        // and they fought each other (broad smoothing leaves few distinct
        // errors, so the extra bands were never spent).  The cap is gone
        // outright: the fit stops when what is left is inaudible, so a job
        // needing two bands uses two and one needing twenty-four uses all.
        detail.setRange (0.0, 1.0, 0.0);
        detail.setValue (0.7);
        detail.setSliderStyle (juce::Slider::LinearHorizontal);
        detail.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        detail.setTooltip ("How closely to follow the reference. Left moves the "
                           "overall tonal balance and nothing else; right chases "
                           "individual resonances. It never limits how many bands "
                           "are used - the fit takes what the job needs and stops "
                           "when the rest is inaudible.");
        detail.onValueChange = [this] { updateDetailLabel(); };
        addAndMakeVisible (detail);

        detailWhat.setFont (juce::Font (juce::FontOptions (10.0f)));
        detailWhat.setColour (juce::Label::textColourId, VC::TextDim);
        detailWhat.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (detailWhat);
        updateDetailLabel();

        status.setFont (juce::Font (juce::FontOptions (10.5f)));
        status.setColour (juce::Label::textColourId, VC::TextDim);
        status.setText ("Play the strip, capture Current. Feed the reference "
                        "into a receive slot and capture it, or load a file. "
                        "Match places stereo, mid and side bands as needed.",
                        juce::dontSendNotification);
        addAndMakeVisible (status);

        for (auto* a : { &curMid, &curSide, &refMid, &refSide })
            a->tiltDbPerOct = 0.0f;          // captures are raw, tilt is display-only

        startTimerHz (10);
        setSize (kPanelW, kPanelH);
    }

    void setSampleRate (double sr)
    {
        for (auto* a : { &curMid, &curSide, &refMid, &refSide })
            a->setSampleRate (sr);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (VC::Panel.withAlpha (0.97f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (VC::Accent);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);

        g.setColour (VC::Text);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText ("EQ MATCH", 10, 6, getWidth() - 20, 14,
                    juce::Justification::centredLeft);

        g.setColour (VC::TextDim);
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("DETAIL", 10, detailLabelY, 60, 10, juce::Justification::left);

        // Capture states, as lights beside their own buttons.  The y comes
        // from resized() rather than a constant, so a layout change cannot
        // leave a light pointing at the wrong row.
        auto light = [&] (int y, bool have, bool running)
        {
            g.setColour (running ? VC::Yellow
                        : have ? VC::Green : VC::Surface.darker (0.3f));
            g.fillEllipse ((float) getWidth() - 18.0f, (float) y, 8.0f, 8.0f);
        };
        light (lightYCur, haveCur, capturingCur);
        light (lightYRef, haveRef, capturingRef);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 4);
        r.removeFromTop (22);

        // Grouped by what they are FOR, not by what kind of control they are:
        // current and its file loader, then everything about the reference.
        auto row = r.removeFromTop (20);
        lightYCur = row.getY() + 6;
        captureCur.setBounds (row.withTrimmedRight (24));
        r.removeFromTop (2);
        loadCur.setBounds (r.removeFromTop (20));
        r.removeFromTop (10);

        row = r.removeFromTop (20);
        lightYRef = row.getY() + 6;
        captureRef.setBounds (row.withTrimmedRight (24));
        r.removeFromTop (2);
        loadRef.setBounds (r.removeFromTop (20));
        r.removeFromTop (10);

        auto head = r.removeFromTop (12);
        detailLabelY = head.getY();
        detailWhat.setBounds (head);
        detail.setBounds (r.removeFromTop (20));
        r.removeFromTop (4);

        status.setBounds (r.removeFromTop (36));
        auto bottom = r.removeFromTop (22);
        apply.setBounds (bottom.removeFromLeft (getWidth() / 2 - 14));
        bottom.removeFromLeft (6);
        close.setBounds (bottom);
    }

private:
    // What the knob means, in words: "0.7" tells nobody anything.
    void updateDetailLabel()
    {
        const double v = detail.getValue();
        const char* what = v < 0.2  ? "overall tonal balance"
                         : v < 0.45 ? "broad shape"
                         : v < 0.7  ? "shape and wide peaks"
                         : v < 0.9  ? "close match"
                                    : "close match, chases resonances";
        detailWhat.setText (what, juce::dontSendNotification);
    }

    // The fit's smoothness runs the OTHER way from the knob: 0 follows every
    // wrinkle, 1 paints in 1.5-octave strokes.  Inverting here is what makes
    // "chases resonances" on the right actually chase resonances.
    float smoothnessForFit() const { return 1.0f - (float) detail.getValue(); }

    void showArmed (juce::TextButton& b, bool armed, const juce::String& idle)
    {
        b.setButtonText (armed ? "Capturing - click to stop" : idle);
        // Yellow is what the capture light already means here; VC::Accent is
        // our border gray and would read as no change at all.
        b.setColour (juce::TextButton::buttonColourId,
                     armed ? VC::Yellow.withAlpha (0.85f) : VC::Surface);
        b.setColour (juce::TextButton::textColourOffId,
                     armed ? VC::Panel : VC::Text);
    }

    void toggleCapture (bool isCurrent)
    {
        auto& mid     = isCurrent ? curMid       : refMid;
        auto& side    = isCurrent ? curSide      : refSide;
        bool& running = isCurrent ? capturingCur : capturingRef;

        if (! running)
        {
            mid.startAverage();
            side.startAverage();
            running = true;
            showArmed (isCurrent ? captureCur : captureRef, true, {});
            status.setText ("Waiting for audio - play the passage, then click again.",
                            juce::dontSendNotification);
            repaint();
            return;
        }

        mid.stopAverage();
        side.stopAverage();
        running = false;
        showArmed (isCurrent ? captureCur : captureRef, false,
                   isCurrent ? "Capture Current" : "Capture Reference (SC)");

        const int n = kbs::EqMatch::kPoints;
        auto& grid     = isCurrent ? curGrid     : refGrid;
        auto& sideGrid = isCurrent ? curSideGrid : refSideGrid;
        bool& have     = isCurrent ? haveCur     : haveRef;
        bool& haveSide = isCurrent ? haveCurSide : haveRefSide;

        grid.assign (n, -80.0f);
        sideGrid.assign (n, -80.0f);

        have = mid.averagedGrid (grid.data(), n, kbs::EqMatch::kLoHz, kbs::EqMatch::kHiHz);
        haveSide = side.averagedGrid (sideGrid.data(), n,
                                      kbs::EqMatch::kLoHz, kbs::EqMatch::kHiHz);

        // Spread is only meaningful for the CURRENT side: it describes the
        // material being corrected, not the thing being matched to.
        if (isCurrent)
        {
            curSpread.assign (n, 0.0f);
            haveSpread = mid.averagedSpreadGrid (curSpread.data(), n,
                                                 kbs::EqMatch::kLoHz, kbs::EqMatch::kHiHz);
        }

        status.setText (have ? "Captured." : "Too short - nothing captured.",
                        juce::dontSendNotification);
        repaint();
    }

    void timerCallback() override
    {
        if (! isVisible()) return;
        setSampleRate (graph.sessionSampleRate());
        auto* e = graph.eq();
        if (e == nullptr) return;
        if (capturingCur) { curMid.analyse (e->preFeed); curSide.analyse (e->preFeed, true); }
        if (capturingRef) { refMid.analyse (e->scFeed);  refSide.analyse (e->scFeed,  true); }
        if (capturingCur || capturingRef) repaint();
    }

    // Both file browsers open where the references live, rather than wherever
    // the OS was last - which was landing them in an unrelated presets folder.
    static juce::File referencesDir()
    {
        auto d = AppPaths::appRoot().getChildFile ("Presets").getChildFile ("EQ")
                                    .getChildFile ("References");
        if (! d.isDirectory()) d.createDirectory();
        return d;
    }

    void loadIntoSide (bool isCurrent)
    {
        chooser = std::make_unique<juce::FileChooser> (
            isCurrent ? "Current track audio" : "Reference audio",
            referencesDir(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, isCurrent] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (! file.existsAsFile()) return;

                auto& grid     = isCurrent ? curGrid     : refGrid;
                auto& sideGrid = isCurrent ? curSideGrid : refSideGrid;
                bool& have     = isCurrent ? haveCur     : haveRef;
                bool& haveSide = isCurrent ? haveCurSide : haveRefSide;

                if (spectrumOfFile (file, grid, sideGrid, haveSide))
                {
                    have = true;
                    // A file has no moment-to-moment story to tell, so nothing
                    // read from one can drive a dynamic band.
                    if (isCurrent) haveSpread = false;
                    status.setText ((isCurrent ? "Current: " : "Reference: ")
                                      + file.getFileName(),
                                    juce::dontSendNotification);
                }
                else
                    status.setText ("Could not read that file.",
                                    juce::dontSendNotification);
                repaint();
            });
    }

    // A whole file to one averaged spectrum per domain: windowed frames, mean
    // dB per bin, resampled to the match grid.  Offline, so no feed.
    static bool spectrumOfFile (const juce::File& file, std::vector<float>& grid,
                                std::vector<float>& sideGrid, bool& haveSide)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples < 8192) return false;

        // A streamed WAV lies about its length: anything piped to disk without
        // seeking back to patch the header writes 0xFFFFFFFF as the data size,
        // and JUCE reports that faithfully - 1.07 billion samples.  Spreading
        // frames over the claimed length then reads past the end and averages
        // silence, which produces a flat curve that passes every other check.
        // The file's real size wins.
        juce::int64 span = reader->lengthInSamples;
        if (auto in = std::unique_ptr<juce::FileInputStream> (file.createInputStream()))
        {
            const int bytesPerFrame = juce::jmax (1, (int) reader->numChannels
                                                     * (int) (reader->bitsPerSample / 8));
            const juce::int64 byLength = in->getTotalLength() / bytesPerFrame;
            if (byLength > 0) span = juce::jmin (span, byLength);
        }
        if (span < 8192) return false;

        const int n = EqAnalyser::kSize;
        kbs::FFT fft (EqAnalyser::kOrder);
        std::vector<std::complex<float>> td ((size_t) n), ts ((size_t) n);
        std::vector<double> sum ((size_t) (n / 2), 0.0), sumS ((size_t) (n / 2), 0.0);
        std::vector<float> win ((size_t) n);
        for (int i = 0; i < n; ++i)
            win[(size_t) i] = 0.5f - 0.5f * std::cos (kbs::kTwoPi * i / n);

        juce::AudioBuffer<float> buf ((int) reader->numChannels, n);
        const bool stereo = reader->numChannels > 1;
        int frames = 0;

        // Frames spread across the WHOLE file: a fixed 50% hop measured only
        // the opening half-minute and called it the track.
        const juce::int64 step = juce::jmax ((juce::int64) (n / 2), span / 400);
        for (juce::int64 at = 0; at + n <= span && frames < 400; at += step)
        {
            reader->read (&buf, 0, n, at, true, true);
            for (int i = 0; i < n; ++i)
            {
                const float l = buf.getSample (0, i);
                const float rr = stereo ? buf.getSample (1, i) : l;
                td[(size_t) i] = { 0.5f * (l + rr) * win[(size_t) i], 0.0f };
                ts[(size_t) i] = { 0.5f * (l - rr) * win[(size_t) i], 0.0f };
            }
            fft.transform (td.data(), false);
            fft.transform (ts.data(), false);
            const float norm = 4.0f / (float) n;
            for (int k = 0; k < n / 2; ++k)
            {
                sum[(size_t) k] += 20.0 * std::log10 (
                    std::max (1.0e-7f, std::abs (td[(size_t) k]) * norm));
                sumS[(size_t) k] += 20.0 * std::log10 (
                    std::max (1.0e-7f, std::abs (ts[(size_t) k]) * norm));
            }
            ++frames;
        }
        if (frames < 2) return false;

        const int pts = kbs::EqMatch::kPoints;
        grid.assign (pts, -80.0f);
        sideGrid.assign (pts, -80.0f);
        const double sr = reader->sampleRate;
        for (int i = 0; i < pts; ++i)
        {
            const double hz = kbs::EqMatch::hzAt (i);
            const double bin = hz * n / sr;
            const int k = juce::jlimit (1, n / 2 - 2, (int) bin);
            const double t = juce::jlimit (0.0, 1.0, bin - k);
            auto lerp = [&] (const std::vector<double>& s)
            {
                return (float) ((s[(size_t) k] * (1.0 - t)
                               + s[(size_t) (k + 1)] * t) / frames);
            };
            grid[(size_t) i]     = lerp (sum);
            sideGrid[(size_t) i] = lerp (sumS);
        }

        // A mono file's side curve is near-silence, not a description of
        // anything; fitting bands to it would fit them to noise.
        float lo = 1.0e9f, hi = -1.0e9f;
        for (int i = 0; i < pts; ++i)
        {
            lo = std::min (lo, sideGrid[(size_t) i]);
            hi = std::max (hi, sideGrid[(size_t) i]);
        }
        haveSide = stereo && (hi - lo) >= 6.0f;
        return true;
    }

    void doMatch()
    {
        if (! haveCur || ! haveRef)
        {
            status.setText ("Capture both sides first.", juce::dontSendNotification);
            return;
        }

        const float sm = smoothnessForFit();
        const double sr = graph.sessionSampleRate();
        const int budget = EqGraphView::kBands;   // no cap: the fit stops itself
        const float* spread = haveSpread ? curSpread.data() : nullptr;

        // Mid/side whenever BOTH sides carry a side curve; otherwise the
        // stereo-wide fit, which is what a mono source can honestly support.
        const bool ms = haveCurSide && haveRefSide;
        const auto fit = ms
            ? kbs::EqMatch::fitMidSide (refGrid.data(), refSideGrid.data(),
                                        curGrid.data(), curSideGrid.data(),
                                        sm, budget, sr, spread, nullptr)
            : kbs::EqMatch::fit (refGrid.data(), curGrid.data(), sm, budget, sr, spread);

        // Replace, not overlay: matching over an existing curve would fit the
        // difference twice.  One undo step; only REGISTERED bands are touched
        // (removing an unregistered band would materialize its params and
        // defeat the lazy grain - SC-2/SC-9).
        beginParamUndoGesture (graph.processor().apvts, graph.paramId (0, "on"));
        for (int b = 0; b < EqGraphView::kBands; ++b)
            if (graph.isBandRegistered (b))
                graph.removeBand (b, false);

        // A stereo-wide fit lands in the view the user is looking at; a
        // mid/side fit carries its own domain per band and ignores the view.
        const float viewChan = (float) (int) graph.domainOfCurrentView();
        int stereoN = 0, midN = 0, sideN = 0;

        for (size_t i = 0; i < fit.bands.size() && (int) i < EqGraphView::kBands; ++i)
        {
            const auto& fb = fit.bands[i];
            const int b = (int) i;
            const float chan = ms ? (float) (int) fb.channel : viewChan;

            graph.setBandValue (b, "on",   1.0f);
            graph.setBandValue (b, "type", 0.0f);
            graph.setBandValue (b, "chan", chan);
            graph.setBandValue (b, "freq", fb.freqHz);
            graph.setBandValue (b, "gain", fb.gainDb);
            graph.setBandValue (b, "q",    fb.q);

            if (fb.dynamic)
            {
                graph.setBandValue (b, "dyn",   1.0f);
                graph.setBandValue (b, "range", fb.rangeDb);
                graph.setBandValue (b, "thr",   fb.thresholdDb);
                graph.setBandValue (b, "ratio", fb.ratio);
                graph.setBandValue (b, "atk",   fb.attackMs);
                graph.setBandValue (b, "rel",   fb.releaseMs);
            }

            if (chan == (float) (int) kbs::EqChannel::mid)       ++midN;
            else if (chan == (float) (int) kbs::EqChannel::side) ++sideN;
            else                                                 ++stereoN;
        }

        juce::String tally;
        if (stereoN > 0) tally << stereoN << " stereo";
        if (midN > 0)    tally << (tally.isEmpty() ? "" : ", ") << midN << " mid";
        if (sideN > 0)   tally << (tally.isEmpty() ? "" : ", ") << sideN << " side";
        if (tally.isEmpty()) tally = "no bands";
        if (fit.dynamicBands > 0) tally << ", " << fit.dynamicBands << " dynamic";

        status.setText (tally + "  -  residual "
                          + juce::String (fit.residualRmsDb, 1) + " dB RMS (was "
                          + juce::String (fit.targetRmsDb, 1) + ")",
                        juce::dontSendNotification);
    }

    EqGraphView& graph;

    juce::TextButton captureCur, loadCur, captureRef, loadRef, apply, close;
    juce::Slider detail;
    juce::Label detailWhat, status;
    std::unique_ptr<juce::FileChooser> chooser;

    EqAnalyser curMid, curSide, refMid, refSide;
    std::vector<float> curGrid, curSideGrid, refGrid, refSideGrid, curSpread;
    bool capturingCur = false, capturingRef = false;
    bool haveCur = false, haveRef = false;
    bool haveCurSide = false, haveRefSide = false, haveSpread = false;
    int lightYCur = 30, lightYRef = 52, detailLabelY = 96;
};

} // namespace eqview
