#include "LoudnessReportWriter.h"

namespace
{
    // Chart geometry.  CALIBRATION (Rule 6 cat. 5): -40..0 LUFS spans every
    // target in LoudnessSpec (-14 through ATSC's -24) with room to read a quiet
    // passage below EBU's -23, matching the analyzer window's own scale so the
    // saved report and the live view cannot disagree about where the bar sits.
    constexpr float kLufsMin = -40.0f;
    constexpr float kLufsMax =   0.0f;
    constexpr int   kChartW  = 900;
    constexpr int   kChartH  = 260;

    juce::String secsToTimecode (double s)
    {
        if (s < 0.0) s = 0.0;
        const int total = (int) s;
        return juce::String (total / 60).paddedLeft ('0', 2) + ":"
             + juce::String (total % 60).paddedLeft ('0', 2) + "."
             + juce::String ((int) ((s - (double) total) * 10.0));
    }

    juce::String esc (const juce::String& s)
    {
        return s.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;");
    }

    const char* kindName (BuilderPage::MeasureResult::Violation::Kind k)
    {
        return k == BuilderPage::MeasureResult::Violation::Kind::TruePeak
                 ? "True peak" : "Short-term loudness";
    }
}

juce::String LoudnessReportWriter::buildDataBlock (const BuilderPage::MeasureResult& m,
                                                   const Context& ctx)
{
    // A compact line-oriented block rather than a nested format: it has to
    // survive being embedded in an HTML comment and parsed back with no library,
    // and a flat key=value list cannot be broken by a stray character in a
    // project name the way a nested syntax could.
    juce::String d;
    d << dataOpenTag() << "\n";
    d << "project="   << ctx.projectName.replace ("\n", " ")   << "\n";
    d << "scope="     << ctx.scopeLabel.replace ("\n", " ")    << "\n";
    d << "stamp="     << ctx.timestampLabel.replace ("\n", " ")<< "\n";
    d << "spec="      << (int) m.specId          << "\n";
    // §4.2: without this a reloaded Custom report recomputes its verdict against
    // the table's placeholder instead of the target the measurement used.
    d << "customtgt=" << juce::String (m.customTargetLufs, 2) << "\n";
    d << "integrated="<< juce::String (m.integratedLufs, 3) << "\n";
    d << "truepeak="  << juce::String (m.truePeakDb, 3)     << "\n";
    d << "lra="       << juce::String (m.lraLu, 3)          << "\n";
    d << "maxshort="  << juce::String (m.maxShortTermLufs, 3)<< "\n";
    d << "maxmom="    << juce::String (m.maxMomentaryLufs, 3)<< "\n";
    d << "duration="  << juce::String (m.durationSeconds, 3) << "\n";
    d << "truncated=" << (m.violationsTruncated ? 1 : 0)     << "\n";

    d << "curve=";
    for (size_t i = 0; i < m.lufsCurve.size(); ++i)
    {
        if (i > 0) d << ",";
        d << juce::String (m.lufsCurve[i], 2);
    }
    d << "\n";

    for (const auto& v : m.violations)
        d << "viol=" << (int) v.kind << "," << juce::String (v.startSecs, 3) << ","
          << juce::String (v.endSecs, 3) << "," << juce::String (v.worstValue, 3) << "\n";

    d << dataCloseTag() << "\n";
    return d;
}

juce::String LoudnessReportWriter::buildHtml (const BuilderPage::MeasureResult& m,
                                              const Context& ctx)
{
    const auto  spec = m.resolvedSpec();   // §4.2: honours a Custom target
    const bool  pass = m.integratedInSpec && m.truePeakInSpec;

    auto yFor = [] (float lufs)
    {
        const float t = juce::jlimit (0.0f, 1.0f,
                                      (lufs - kLufsMin) / (kLufsMax - kLufsMin));
        return (float) kChartH - t * (float) kChartH;
    };

    // ── The curve, as an SVG polyline ────────────────────────────────────────
    juce::String poly, overBars;
    const int n = (int) m.lufsCurve.size();
    if (n > 0)
    {
        const float dx = (n > 1) ? ((float) kChartW / (float) (n - 1)) : (float) kChartW;
        for (int i = 0; i < n; ++i)
        {
            const float v = m.lufsCurve[(size_t) i];
            if (v <= kLufsMin) continue;   // gated: no line drawn through silence
            poly << juce::String ((float) i * dx, 1) << "," << juce::String (yFor (v), 1) << " ";
        }
        // Moments over the target, drawn as bars so they read without the log.
        const float ty = yFor (spec.checksIntegrated ? spec.integratedLufs : kLufsMin);
        for (int i = 0; i < n; ++i)
        {
            const float v = m.lufsCurve[(size_t) i];
            if (! spec.checksIntegrated || v <= spec.integratedLufs) continue;
            overBars << "<rect x='" << juce::String ((float) i * dx, 1)
                     << "' y='" << juce::String (yFor (v), 1)
                     << "' width='" << juce::String (juce::jmax (1.0f, dx), 1)
                     << "' height='" << juce::String (juce::jmax (0.0f, ty - yFor (v)), 1)
                     << "' fill='#ff9100' opacity='0.55'/>";
        }
    }

    const float targetY = yFor (spec.checksIntegrated ? spec.integratedLufs : kLufsMin);

    juce::String h;
    h << "<!doctype html><html><head><meta charset='utf-8'>"
      << "<title>Loudness Report - " << esc (ctx.projectName) << "</title><style>"
         "body{background:#101214;color:#cfd6da;font-family:Segoe UI,Arial,sans-serif;margin:24px}"
         "h1{font-size:18px;margin:0 0 4px}h2{font-size:13px;margin:22px 0 8px;color:#8f9aa1}"
         ".sub{color:#8f9aa1;font-size:12px;margin-bottom:18px}"
         ".verdict{display:inline-block;padding:5px 12px;border-radius:3px;font-weight:bold;"
         "font-family:monospace;font-size:13px}"
         ".pass{background:#0d3b2e;color:#00fff2}.fail{background:#3b1f0d;color:#ff9100}"
         "table{border-collapse:collapse;font-family:monospace;font-size:12px}"
         "td,th{border:1px solid #23282c;padding:5px 10px;text-align:left}"
         "th{color:#8f9aa1;font-weight:normal}"
         ".big{font-family:monospace;font-size:22px;color:#00fff2}"
         ".muted{color:#8f9aa1;font-size:11px}"
      << "</style></head><body>";

    h << "<h1>Loudness Report - " << esc (ctx.projectName) << "</h1>"
      << "<div class='sub'>" << esc (ctx.scopeLabel) << " &middot; "
      << esc (ctx.timestampLabel) << " &middot; checked against " << esc (spec.name)
      << "</div>";

    h << "<div class='verdict " << (pass ? "pass'>IN SPEC" : "fail'>OUT OF SPEC") << "</div>";

    // ── Summary ─────────────────────────────────────────────────────────────
    h << "<h2>Summary</h2><table>"
      << "<tr><th>Integrated</th><td class='big'>" << juce::String (m.integratedLufs, 1)
      << " LUFS</td><td class='muted'>";
    if (spec.checksIntegrated)
        h << "target " << juce::String (spec.integratedLufs, 1) << " &plusmn; "
          << juce::String (spec.toleranceLu, 1) << " LU &middot; "
          << (m.integratedInSpec ? "in spec" : "out of spec");
    else
        h << "measurement only - this spec defines no target";
    h << "</td></tr>";

    h << "<tr><th>True peak</th><td class='big'>" << juce::String (m.truePeakDb, 1)
      << " dBTP</td><td class='muted'>max " << juce::String (spec.maxTruePeakDb, 1)
      << " dBTP &middot; " << (m.truePeakInSpec ? "in spec" : "OVER") << "</td></tr>";

    h << "<tr><th>Loudness range</th><td class='big'>" << juce::String (m.lraLu, 1)
      << " LU</td><td class='muted'>EBU Tech 3342</td></tr>";
    h << "<tr><th>Max short-term</th><td>" << juce::String (m.maxShortTermLufs, 1)
      << " LUFS</td><td class='muted'>max momentary "
      << juce::String (m.maxMomentaryLufs, 1) << " LUFS</td></tr>";
    h << "<tr><th>Duration</th><td>" << secsToTimecode (m.durationSeconds)
      << "</td><td class='muted'></td></tr></table>";

    // ── Chart ───────────────────────────────────────────────────────────────
    h << "<h2>Loudness over time</h2>"
      << "<svg viewBox='0 0 " << kChartW << " " << kChartH
      << "' width='100%' height='" << kChartH << "' preserveAspectRatio='none'>"
      << "<rect width='" << kChartW << "' height='" << kChartH << "' fill='#0b0d0e'/>";
    for (float lu = 0.0f; lu >= kLufsMin; lu -= 6.0f)
        h << "<line x1='0' y1='" << juce::String (yFor (lu), 1) << "' x2='" << kChartW
          << "' y2='" << juce::String (yFor (lu), 1) << "' stroke='#1d2124'/>";
    h << overBars;
    if (spec.checksIntegrated)
        h << "<line x1='0' y1='" << juce::String (targetY, 1) << "' x2='" << kChartW
          << "' y2='" << juce::String (targetY, 1)
          << "' stroke='#00fff2' stroke-dasharray='6,4'/>";
    if (poly.isNotEmpty())
        h << "<polyline points='" << poly
          << "' fill='none' stroke='#00fff2' stroke-width='1.6'/>";
    h << "</svg><div class='muted'>Dashed line is the target. Orange marks the moments above it. "
         "Scale " << (int) kLufsMax << " to " << (int) kLufsMin << " LUFS.</div>";

    // ── §5.1 spectrum snapshot ──────────────────────────────────────────────
    // Omitted entirely when the measurement produced no bands (a render shorter
    // than one transform) rather than drawn as a flat line, which would read as
    // "this mix has no content" instead of "this was not measured".
    if (! m.spectrumDb.empty())
    {
        constexpr float kSpecTop = 0.0f, kSpecBot = -96.0f;
        constexpr float kSw = 900.0f, kSh = 180.0f;

        h << "<h2>Spectrum</h2>";
        h << "<svg viewBox='0 0 " << (int) kSw << " " << (int) kSh
          << "' preserveAspectRatio='none' style='width:100%;height:180px'>";
        h << "<rect x='0' y='0' width='" << (int) kSw << "' height='" << (int) kSh
          << "' fill='#101214'/>";

        // Decade marks on the same log axis the analyzer window uses, so the two
        // read as the same picture of the same mix.
        static const float kMarks[] = { 50.f, 100.f, 500.f, 1000.f, 5000.f, 10000.f };
        const float lo = std::log (BuilderPage::MeasureResult::kSpectrumMinHz);
        const float span = std::log (BuilderPage::MeasureResult::kSpectrumMaxHz) - lo;
        for (float hz : kMarks)
        {
            const float x = kSw * (std::log (hz) - lo) / span;
            h << "<line x1='" << juce::String (x, 1) << "' y1='0' x2='"
              << juce::String (x, 1) << "' y2='" << (int) kSh
              << "' stroke='#23282c' stroke-width='1'/>";
            h << "<text x='" << juce::String (x + 3.0f, 1) << "' y='" << (int) kSh - 4
              << "' fill='#8f9aa1' font-size='9'>"
              << (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k"
                                : juce::String ((int) hz)) << "</text>";
        }

        juce::String sp;
        const int n = (int) m.spectrumDb.size();
        for (int i = 0; i < n; ++i)
        {
            const float x = kSw * (float) i / (float) juce::jmax (1, n - 1);
            const float t = juce::jlimit (0.0f, 1.0f,
                (m.spectrumDb[(size_t) i] - kSpecBot) / (kSpecTop - kSpecBot));
            const float y = kSh - t * kSh;
            sp << juce::String (x, 1) << "," << juce::String (y, 1) << " ";
        }
        h << "<polyline points='" << sp
          << "' fill='none' stroke='#00fff2' stroke-width='1.4'/>";
        h << "</svg><div class='muted'>Average level per frequency band across the whole "
             "render, " << (int) BuilderPage::MeasureResult::kSpectrumMinHz << " Hz to "
          << (int) (BuilderPage::MeasureResult::kSpectrumMaxHz / 1000) << " kHz, "
             "0 to " << (int) kSpecBot << " dB.</div>";
    }

    // ── Violations ──────────────────────────────────────────────────────────
    h << "<h2>Flagged moments</h2>";
    if (m.violations.empty())
    {
        h << "<div class='muted'>None - nothing crossed the limits for this spec.</div>";
    }
    else
    {
        h << "<table><tr><th>Start</th><th>End</th><th>What</th><th>Worst</th></tr>";
        for (const auto& v : m.violations)
            h << "<tr><td>" << secsToTimecode (v.startSecs) << "</td><td>"
              << secsToTimecode (v.endSecs)   << "</td><td>" << kindName (v.kind)
              << "</td><td>" << juce::String (v.worstValue, 1) << "</td></tr>";
        h << "</table>";
        if (m.violationsTruncated)
            h << "<div class='muted'>List truncated at " << BuilderPage::kMaxViolationRows
              << " spans - there were more.</div>";
    }

    h << buildDataBlock (m, ctx);
    h << "</body></html>";
    return h;
}

juce::String LoudnessReportWriter::buildCsv (const BuilderPage::MeasureResult& m,
                                             const Context& ctx)
{
    const auto spec = m.resolvedSpec();   // §4.2: honours a Custom target
    juce::String c;
    c << "Project," << ctx.projectName.replace (",", " ") << "\n"
      << "Scope,"   << ctx.scopeLabel.replace (",", " ")  << "\n"
      << "When,"    << ctx.timestampLabel.replace (",", " ") << "\n"
      << "Spec,"    << juce::String (spec.name).replace (",", " ") << "\n"
      << "Integrated LUFS," << juce::String (m.integratedLufs, 2) << "\n"
      << "True peak dBTP,"  << juce::String (m.truePeakDb, 2)     << "\n"
      << "LRA LU,"          << juce::String (m.lraLu, 2)          << "\n"
      << "Max short-term LUFS," << juce::String (m.maxShortTermLufs, 2) << "\n"
      << "Max momentary LUFS,"  << juce::String (m.maxMomentaryLufs, 2) << "\n"
      << "Duration seconds,"    << juce::String (m.durationSeconds, 2)  << "\n"
      << "In spec," << ((m.integratedInSpec && m.truePeakInSpec) ? "yes" : "no") << "\n"
      << "\nStart,End,Kind,Worst\n";
    for (const auto& v : m.violations)
        c << juce::String (v.startSecs, 3) << "," << juce::String (v.endSecs, 3) << ","
          << kindName (v.kind) << "," << juce::String (v.worstValue, 2) << "\n";
    if (m.violationsTruncated)
        c << "\nNOTE,list truncated at " << BuilderPage::kMaxViolationRows << " spans\n";
    return c;
}

bool LoudnessReportWriter::write (const juce::File& reportsDir,
                                  const juce::String& base,
                                  const BuilderPage::MeasureResult& m,
                                  const Context& ctx,
                                  bool wantCsv,
                                  juce::String& outErr)
{
    if (! reportsDir.exists() && ! reportsDir.createDirectory())
    {
        outErr = "Could not create the Reports folder.";
        return false;
    }

    const juce::File html = reportsDir.getChildFile (base + " - Loudness Report.html");
    if (! html.replaceWithText (buildHtml (m, ctx)))
    {
        outErr = "Could not write " + html.getFileName();
        return false;
    }

    if (wantCsv)
    {
        const juce::File csv = reportsDir.getChildFile (base + " - Loudness Report.csv");
        if (! csv.replaceWithText (buildCsv (m, ctx)))
        {
            // The HTML already landed, so this is a partial success and saying so
            // is better than reporting total failure for a file the user opted into.
            outErr = "The HTML report was written, but the CSV could not be: "
                   + csv.getFileName();
            return false;
        }
    }
    return true;
}

bool LoudnessReportWriter::readEmbedded (const juce::File& htmlFile,
                                         BuilderPage::MeasureResult& out,
                                         Context& outCtx)
{
    if (! htmlFile.existsAsFile()) return false;
    const juce::String text = htmlFile.loadFileAsString();

    const int open = text.indexOf (dataOpenTag());
    if (open < 0) return false;                       // foreign or hand-edited file
    const int close = text.indexOf (open, dataCloseTag());
    if (close < 0) return false;

    const juce::String block = text.substring (open + (int) juce::String (dataOpenTag()).length(),
                                               close);
    juce::StringArray lines;
    lines.addLines (block);

    out = BuilderPage::MeasureResult{};
    bool sawAny = false;

    for (const auto& raw : lines)
    {
        const juce::String line = raw.trim();
        if (line.isEmpty()) continue;
        const juce::String key = line.upToFirstOccurrenceOf ("=", false, false);
        const juce::String val = line.fromFirstOccurrenceOf ("=", false, false);
        if (key.isEmpty()) continue;
        sawAny = true;

        if      (key == "project")   outCtx.projectName    = val;
        else if (key == "scope")     outCtx.scopeLabel     = val;
        else if (key == "stamp")     outCtx.timestampLabel = val;
        else if (key == "customtgt") out.customTargetLufs = val.getFloatValue();
        else if (key == "spec")      out.specId = (LoudnessSpec::Id) juce::jlimit (
                                         0, LoudnessSpec::numSpecs() - 1, val.getIntValue());
        else if (key == "integrated")out.integratedLufs   = val.getFloatValue();
        else if (key == "truepeak")  out.truePeakDb       = val.getFloatValue();
        else if (key == "lra")       out.lraLu            = val.getFloatValue();
        else if (key == "maxshort")  out.maxShortTermLufs = val.getFloatValue();
        else if (key == "maxmom")    out.maxMomentaryLufs = val.getFloatValue();
        else if (key == "duration")  out.durationSeconds  = val.getDoubleValue();
        else if (key == "truncated") out.violationsTruncated = (val.getIntValue() != 0);
        else if (key == "curve")
        {
            juce::StringArray pts;
            pts.addTokens (val, ",", "");
            for (const auto& p : pts)
                if (p.isNotEmpty()) out.lufsCurve.push_back (p.getFloatValue());
        }
        else if (key == "viol")
        {
            juce::StringArray f;
            f.addTokens (val, ",", "");
            if (f.size() >= 4)
                out.violations.push_back ({
                    (BuilderPage::MeasureResult::Violation::Kind) f[0].getIntValue(),
                    f[1].getDoubleValue(), f[2].getDoubleValue(), f[3].getFloatValue() });
        }
    }

    if (! sawAny) return false;

    // Recompute the verdicts rather than storing them: the spec table is the one
    // authority, so a report reopened after a spec's numbers changed reads against
    // the current definition instead of a stale copy of it.
    const auto spec = out.resolvedSpec();
    out.truePeakInSpec   = (out.truePeakDb <= spec.maxTruePeakDb);
    out.integratedInSpec = (! spec.checksIntegrated)
                         || (std::abs (out.integratedLufs - spec.integratedLufs)
                               <= spec.toleranceLu);
    return true;
}
