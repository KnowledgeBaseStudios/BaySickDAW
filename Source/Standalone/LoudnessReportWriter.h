#pragma once
#include <JuceHeader.h>
#include "BuilderPage.h"

// -- LoudnessReportWriter -- CL-227 (QA-ModelShell TS7), session form QA-TrueLevel
// SC-14 (Jeff, 2026-08-22) ---------------------------------------------------
// ONE report file per session, takes as sections.  Each take carries its
// summary, its Short-Term AND Momentary curves at 10 Hz, its violations and
// (for rendered measurements) a spectrum snapshot.  The page is SELF-CONTAINED
// and INTERACTIVE: plain JavaScript, no external assets, so it opens anywhere
// -- zoom / pan the curves, hover for a readout, shift-drag a region for that
// region's integrated loudness (BS.1770 gating over the momentary series, which
// at 10 Hz IS the 400 ms / 100 ms-hop gating-block series, so the number is
// exact per spec), its LRA (Tech 3342 over short-term), max M / S and length;
// tick takes and "Save selected as report" downloads a new self-contained file.
//
// RELOADABLE FROM ITSELF.  The HTML embeds the measurement DATA in a block a
// browser ignores; the app parses that block back out (readEmbeddedAll) and
// imports every take into the analyzer's Source list.  One file is both the
// shareable artifact and our reload source -- no sidecar, no second format.
// The page's own script reads the SAME block, so the data exists once.
//
// We do NOT render the HTML in-app (no WebBrowserComponent -- see the QA-
// ModelShell TS7 note); the analyzer window is the in-app view.
// -----------------------------------------------------------------------------
class LoudnessReportWriter
{
public:
    struct Context
    {
        juce::String projectName;
        juce::String scopeLabel;      // legacy single-take reports only
        juce::String timestampLabel;  // session start, human-readable
    };

    // One section of a session report.
    struct Take
    {
        juce::String label;           // take label (its timestamp)
        juce::String scopeLabel;      // what was playing
        BuilderPage::MeasureResult m; // lufsCurve (S) + momentaryCurve (M), both 10 Hz
    };

    // Writes "<base>.html" into `dir`.  Returns false and fills outErr on a
    // write failure; a report that cannot be written is reported, never swallowed.
    static bool writeSession (const juce::File& dir,
                              const juce::String& base,
                              const std::vector<Take>& takes,
                              const Context& ctx,
                              juce::String& outErr);

    // Every take in a report written by writeSession (or the older single-take
    // write).  False if the file carries no data block.
    static bool readEmbeddedAll (const juce::File& htmlFile,
                                 std::vector<Take>& out,
                                 Context& outCtx);

    // Legacy single-take forms, kept for the callers that still think in one
    // measurement: write() emits "<base> - Loudness Report.html" with one take,
    // readEmbedded() returns the first take.
    static bool write (const juce::File& reportsDir,
                       const juce::String& base,
                       const BuilderPage::MeasureResult& m,
                       const Context& ctx,
                       bool wantCsv,
                       juce::String& outErr);
    static bool readEmbedded (const juce::File& htmlFile,
                              BuilderPage::MeasureResult& out,
                              Context& outCtx);

    static const char* dataOpenTag()  { return "<!--BSDAW-REPORT-DATA"; }
    static const char* dataCloseTag() { return "BSDAW-REPORT-DATA-->"; }

private:
    static juce::String buildHtml      (const std::vector<Take>&, const Context&);
    static juce::String buildDataBlock (const std::vector<Take>&, const Context&);
    static juce::String takeSection    (const Take&, int index);
    static juce::String esc            (const juce::String&);
};
