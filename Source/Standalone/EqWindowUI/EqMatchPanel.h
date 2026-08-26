// BaySickDAW — the EQ Match panel (QA-EqPro).
//
// Ported from KBS EQ Pro: three captures and a fit.  CURRENT is the strip as
// it arrives (the pre-EQ feed, averaged over the passage you play); REFERENCE
// is what it should sound like - the strip's picked sidechain receive line
// averaged the same way, or an audio file read straight into a spectrum.
// Match fits bells to the difference and writes them into the bands,
// replacing what was there.  The fit is DSP/Kbs/EqMatch.h and is proven in
// the BaySickEqTests target; this panel is only capture, preview numbers,
// and apply.
#pragma once

#include "EqGraphView.h"
#include "../../DSP/Kbs/EqMatch.h"

namespace eqview {

class EqMatchPanel : public juce::Component, private juce::Timer
{
public:
    explicit EqMatchPanel (EqGraphView& graphRef) : graph (graphRef)
    {
        auto initButton = [this] (juce::TextButton& b, const juce::String& text)
        {
            b.setButtonText (text);
            addAndMakeVisible (b);
        };

        initButton (captureCur, "Capture Current");
        initButton (captureRef, "Capture Reference (SC)");
        initButton (loadRef, "Load Reference File...");
        initButton (apply, "Match");
        initButton (close, "Close");

        captureCur.setTooltip ("Averages this strip's own sound while you play "
                               "the passage. Press again to stop.");
        captureRef.setTooltip ("Averages the picked sidechain receive line - "
                               "route the track you want to sound like into "
                               "one of this strip's receive slots first.");

        captureCur.onClick = [this] { toggleCapture (curAnalyser, capturingCur, haveCur, curGrid); };
        captureRef.onClick = [this] { toggleCapture (refAnalyser, capturingRef, haveRef, refGrid); };
        loadRef.onClick = [this] { loadReferenceFile(); };
        apply.onClick = [this] { doMatch(); };
        close.onClick = [this] { setVisible (false); };

        smooth.setRange (0.0, 1.0, 0.0);
        smooth.setValue (0.3);
        smooth.setSliderStyle (juce::Slider::LinearHorizontal);
        smooth.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        smooth.setTooltip ("How broad the fitted strokes are. Full smoothness "
                           "matches the tone, not the bumps.");
        addAndMakeVisible (smooth);

        bandsBox.setRange (1.0, 24.0, 1.0);
        bandsBox.setValue (12.0);
        bandsBox.setSliderStyle (juce::Slider::IncDecButtons);
        bandsBox.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 18);
        addAndMakeVisible (bandsBox);

        status.setFont (juce::Font (juce::FontOptions (10.5f)));
        status.setColour (juce::Label::textColourId, VC::TextDim);
        status.setText ("Play the strip, capture Current. Feed the reference "
                        "into a receive slot and capture it, or load a file.",
                        juce::dontSendNotification);
        addAndMakeVisible (status);

        curAnalyser.tiltDbPerOct = 0.0f;     // captures are raw, tilt is display-only
        refAnalyser.tiltDbPerOct = 0.0f;

        startTimerHz (10);
        setSize (280, 190);
    }

    void setSampleRate (double sr)
    {
        curAnalyser.setSampleRate (sr);
        refAnalyser.setSampleRate (sr);
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
        g.drawText ("SMOOTH", 10, 96, 60, 10, juce::Justification::left);
        g.drawText ("BANDS", getWidth() - 96, 96, 50, 10, juce::Justification::left);

        // Capture states, as lights beside their buttons.
        auto light = [&] (float y, bool have, bool running)
        {
            g.setColour (running ? VC::Yellow
                        : have ? VC::Green : VC::Surface.darker (0.3f));
            g.fillEllipse ((float) getWidth() - 18.0f, y, 8.0f, 8.0f);
        };
        light (30.0f, haveCur, capturingCur);
        light (52.0f, haveRef, capturingRef);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 4);
        r.removeFromTop (22);
        captureCur.setBounds (r.removeFromTop (20).withTrimmedRight (24));
        r.removeFromTop (2);
        captureRef.setBounds (r.removeFromTop (20).withTrimmedRight (24));
        r.removeFromTop (2);
        loadRef.setBounds (r.removeFromTop (20));
        r.removeFromTop (14);

        auto row = r.removeFromTop (20);
        smooth.setBounds (row.removeFromLeft (getWidth() / 2 - 14));
        bandsBox.setBounds (row.removeFromRight (92));
        r.removeFromTop (4);

        status.setBounds (r.removeFromTop (30));
        auto bottom = r.removeFromTop (22);
        apply.setBounds (bottom.removeFromLeft (getWidth() / 2 - 14));
        bottom.removeFromLeft (6);
        close.setBounds (bottom);
    }

private:
    void toggleCapture (EqAnalyser& an, bool& running, bool& have, std::vector<float>& grid)
    {
        if (! running)
        {
            an.startAverage();
            running = true;
            status.setText ("Capturing... play the passage, press again to stop.",
                            juce::dontSendNotification);
        }
        else
        {
            an.stopAverage();
            running = false;
            grid.assign (kbs::EqMatch::kPoints, -80.0f);
            have = an.averagedGrid (grid.data(), kbs::EqMatch::kPoints,
                                    kbs::EqMatch::kLoHz, kbs::EqMatch::kHiHz);
            status.setText (have ? "Captured." : "Too short - nothing captured.",
                            juce::dontSendNotification);
        }
        repaint();
    }

    void timerCallback() override
    {
        if (! isVisible()) return;
        setSampleRate (graph.sessionSampleRate());
        auto* e = graph.eq();
        if (e == nullptr) return;
        if (capturingCur) curAnalyser.analyse (e->preFeed);
        if (capturingRef) refAnalyser.analyse (e->scFeed);
        if (capturingCur || capturingRef) repaint();
    }

    void loadReferenceFile()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Reference audio", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (! file.existsAsFile()) return;
                if (spectrumOfFile (file, refGrid))
                {
                    haveRef = true;
                    status.setText ("Reference: " + file.getFileName(),
                                    juce::dontSendNotification);
                }
                else
                    status.setText ("Could not read that file.",
                                    juce::dontSendNotification);
                repaint();
            });
    }

    // A whole file to one averaged spectrum: windowed frames at 50% overlap,
    // mean dB per bin, resampled to the match grid.  Offline, so no feed.
    static bool spectrumOfFile (const juce::File& file, std::vector<float>& grid)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples < 8192) return false;

        const int n = EqAnalyser::kSize;
        kbs::FFT fft (EqAnalyser::kOrder);
        std::vector<std::complex<float>> td ((size_t) n);
        std::vector<double> sum ((size_t) (n / 2), 0.0);
        std::vector<float> win ((size_t) n);
        for (int i = 0; i < n; ++i)
            win[(size_t) i] = 0.5f - 0.5f * std::cos (kbs::kTwoPi * i / n);

        juce::AudioBuffer<float> buf ((int) reader->numChannels, n);
        int frames = 0;

        for (juce::int64 at = 0; at + n <= reader->lengthInSamples && frames < 400;
             at += n / 2)
        {
            reader->read (&buf, 0, n, at, true, true);
            for (int i = 0; i < n; ++i)
            {
                float mono = buf.getSample (0, i);
                if (buf.getNumChannels() > 1)
                    mono = 0.5f * (mono + buf.getSample (1, i));
                td[(size_t) i] = { mono * win[(size_t) i], 0.0f };
            }
            fft.transform (td.data(), false);
            const float norm = 4.0f / (float) n;
            for (int k = 0; k < n / 2; ++k)
                sum[(size_t) k] += 20.0 * std::log10 (
                    std::max (1.0e-7f, std::abs (td[(size_t) k]) * norm));
            ++frames;
        }
        if (frames < 2) return false;

        grid.assign (kbs::EqMatch::kPoints, -80.0f);
        const double sr = reader->sampleRate;
        for (int i = 0; i < kbs::EqMatch::kPoints; ++i)
        {
            const double hz = kbs::EqMatch::hzAt (i);
            const double bin = hz * n / sr;
            const int k = juce::jlimit (1, n / 2 - 2, (int) bin);
            const double t = juce::jlimit (0.0, 1.0, bin - k);
            grid[(size_t) i] = (float) ((sum[(size_t) k] * (1.0 - t)
                                       + sum[(size_t) (k + 1)] * t) / frames);
        }
        return true;
    }

    void doMatch()
    {
        if (! haveCur || ! haveRef)
        {
            status.setText ("Capture both sides first.", juce::dontSendNotification);
            return;
        }

        const auto fit = kbs::EqMatch::fit (refGrid.data(), curGrid.data(),
                                            (float) smooth.getValue(),
                                            (int) bandsBox.getValue(),
                                            graph.sessionSampleRate());

        // Replace, not overlay: matching over an existing curve would fit the
        // difference twice.  The fit lands in the CURRENT view's domain.
        for (int b = 0; b < EqGraphView::kBands; ++b)
            graph.removeBand (b);

        const float chan = (float) (int) graph.domainOfCurrentView();
        for (size_t i = 0; i < fit.bands.size(); ++i)
        {
            const auto& fb = fit.bands[i];
            const int b = (int) i;
            graph.setBandValue (b, "on", 1.0f);
            graph.setBandValue (b, "type", 0.0f);
            graph.setBandValue (b, "chan", chan);
            graph.setBandValue (b, "freq", fb.freqHz);
            graph.setBandValue (b, "gain", fb.gainDb);
            graph.setBandValue (b, "q", fb.q);
        }

        status.setText (juce::String ((int) fit.bands.size()) + " bands  -  residual "
                          + juce::String (fit.residualRmsDb, 1) + " dB RMS (was "
                          + juce::String (fit.targetRmsDb, 1) + ")",
                        juce::dontSendNotification);
    }

    EqGraphView& graph;

    juce::TextButton captureCur, captureRef, loadRef, apply, close;
    juce::Slider smooth, bandsBox;
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;

    EqAnalyser curAnalyser, refAnalyser;
    std::vector<float> curGrid, refGrid;
    bool capturingCur = false, capturingRef = false;
    bool haveCur = false, haveRef = false;
};

} // namespace eqview
