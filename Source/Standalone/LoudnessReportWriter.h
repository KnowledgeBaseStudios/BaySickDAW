#pragma once
#include <JuceHeader.h>
#include "BuilderPage.h"

// -- LoudnessReportWriter — CL-227 (QA-ModelShell TS7) -------------------------
// Writes the loudness-conformance report for a measured render.
//
// TWO OUTPUTS, and the split is deliberate (Jeff, 2026-07-29):
//   * HTML  -- ALWAYS written, no control to disable it.  This is the one meant
//              to be READ: inline SVG showing the loudness curve, the target as
//              a bar across it, the moments that went over, true-peak markers and
//              a pass/fail summary.  "Numbers on a spreadsheet" was the thing to
//              avoid.
//   * CSV   -- opt-in, for anyone who wants the raw rows.
// No XML.
//
// SELF-CONTAINED, AND RELOADABLE FROM ITSELF.  The HTML carries no external
// references -- styles and the chart are inline, so it opens anywhere.  It also
// embeds the measurement DATA in a block a browser ignores, which is how a saved
// report reopens INSIDE the app: clicking one in the Builder's Files browser
// parses that block back out and repopulates the analyzer window with the same
// curve it drew live.  One file is therefore both the shareable artifact and our
// reload source -- no sidecar, no third format, and the Reports folder holds
// exactly what the user thinks it holds.
//
// We do NOT render the HTML in-app: JUCE_WEB_BROWSER is 0 and only
// juce_gui_basics is linked, and enabling WebBrowserComponent would add a Windows
// WebView2 runtime dependency that yields a blank report on any machine without
// it.  The analyzer window is the in-app view.
// -----------------------------------------------------------------------------
class LoudnessReportWriter
{
public:
    // Everything the report needs that the measurement itself does not carry.
    struct Context
    {
        juce::String projectName;
        juce::String scopeLabel;      // "Full Arrangement" / "Selected Section" / a pattern
        juce::String timestampLabel;  // human-readable, for the page header
    };

    // Writes "<base> - Loudness Report.html" (+ ".csv" when wantCsv) into
    // `reportsDir`.  `base` should already carry the date-time stamp.  Returns
    // false and fills outErr on a write failure; a report that cannot be written
    // is reported, never swallowed.
    static bool write (const juce::File& reportsDir,
                       const juce::String& base,
                       const BuilderPage::MeasureResult& m,
                       const Context& ctx,
                       bool wantCsv,
                       juce::String& outErr);

    // Parses a report written by write() back into a MeasureResult, for the
    // in-app view.  Returns false if the file carries no data block (e.g. a
    // hand-edited or foreign HTML file) rather than guessing at defaults.
    static bool readEmbedded (const juce::File& htmlFile,
                              BuilderPage::MeasureResult& out,
                              Context& outCtx);

    // The marker delimiting the embedded data block.  Kept here so the writer and
    // the reader cannot disagree about it.
    static const char* dataOpenTag()  { return "<!--BSDAW-REPORT-DATA"; }
    static const char* dataCloseTag() { return "BSDAW-REPORT-DATA-->"; }

private:
    static juce::String buildHtml (const BuilderPage::MeasureResult&, const Context&);
    static juce::String buildCsv  (const BuilderPage::MeasureResult&, const Context&);
    static juce::String buildDataBlock (const BuilderPage::MeasureResult&, const Context&);
};
