#include "SlideRegionMap.h"
#include <map>
#include <algorithm>
#include <utility>

int SlideRegionMap::bendMaxUpSemis() const noexcept
{
    const int cents = (bendUpCents == kBendUnset) ? 200 : bendUpCents;   // sfizz default +200
    return juce::jmax (1, (cents + 50) / 100);
}

int SlideRegionMap::bendMaxDownSemis() const noexcept
{
    if (bendDownCents == kBendUnset) return 2;       // sfizz default -200 -> 2 semis down
    if (bendDownCents >= 0)          return 0;       // positive bend_down = up-only (guitar)
    return juce::jmax (0, (-bendDownCents + 50) / 100);
}

namespace
{
    // SFZ `//` runs to end of line.
    juce::String stripComment (const juce::String& raw)
    {
        const int c = raw.indexOf ("//");
        return (c >= 0 ? raw.substring (0, c) : raw).trim();
    }

    // Task 10 parser hardening: one line may carry a <header> AND opcodes, or
    // several opcodes.  An opcode's VALUE runs until the next `key=` anchor or
    // EOL (SFZ semantics -- `sample=My File.wav lokey=40` keeps the space).
    void tokenizeSfzLine (const juce::String& line,
                          juce::StringArray& headersOut,
                          std::vector<std::pair<juce::String, juce::String>>& opsOut)
    {
        juce::String s = line;
        for (;;)
        {
            const int lt = s.indexOfChar ('<');
            if (lt < 0) break;
            const int gt = s.indexOfChar (lt, '>');
            if (gt < 0) break;
            headersOut.add (s.substring (lt + 1, gt).trim().toLowerCase());
            s = s.substring (0, lt) + " " + s.substring (gt + 1);
        }
        struct Anchor { int keyStart, eq; };
        std::vector<Anchor> anchors;
        for (int i = 0; i < s.length(); ++i)
        {
            if (s[i] != '=') continue;
            int k = i - 1;
            while (k >= 0 && (juce::CharacterFunctions::isLetterOrDigit (s[k]) || s[k] == '_'))
                --k;
            if (k == i - 1) continue;   // '=' with no key token
            anchors.push_back ({ k + 1, i });
        }
        for (size_t a = 0; a < anchors.size(); ++a)
        {
            const auto key  = s.substring (anchors[a].keyStart, anchors[a].eq).trim().toLowerCase();
            const int  vEnd = (a + 1 < anchors.size()) ? anchors[a + 1].keyStart : s.length();
            const auto val  = s.substring (anchors[a].eq + 1, vEnd).trim();
            if (key.isNotEmpty())
                opsOut.push_back ({ key, val });
        }
    }

    // Splice the #include chain into one line list.  Include paths resolve
    // against the INCLUDING file's dir; opcode lines are copied verbatim.
    void expandIncludes (const juce::File& f, int depth, juce::StringArray& out)
    {
        if (depth > 6 || ! f.existsAsFile()) return;
        juce::StringArray ls;
        ls.addLines (f.loadFileAsString());
        for (const auto& raw : ls)
        {
            const auto t = raw.trim();
            if (t.startsWithIgnoreCase ("#include"))
            {
                const int q1 = t.indexOfChar ('"');
                const int q2 = (q1 >= 0) ? t.indexOfChar (q1 + 1, '"') : -1;
                if (q1 >= 0 && q2 > q1)
                {
                    const auto rel = t.substring (q1 + 1, q2).replaceCharacter ('\\', '/');
                    expandIncludes (f.getParentDirectory().getChildFile (rel), depth + 1, out);
                }
            }
            else
            {
                out.add (raw);
            }
        }
    }

    // Is this opcode part of a modulation family Task 11's router consumes?
    bool isModFamilyOpcode (const juce::String& k)
    {
        return k.startsWith ("lfo")
            || k.startsWith ("pitcheg_")
            || k.startsWith ("fileg_")
            || k.startsWith ("cutoff")          // cutoff, cutoff2, cutoff_cc92, cutoff_oncc...
            || k.startsWith ("fil_")
            || k.startsWith ("fil2_")
            || k.startsWith ("resonance")
            || k.startsWith ("var0")            // var01_* / var02_* mult kludge
            || k.startsWith ("offset_oncc")
            || k.startsWith ("amplitude_oncc")
            || k.startsWith ("pan_oncc")
            || k.startsWith ("tune_cc")
            || k.startsWith ("tune_oncc")
            || k.startsWith ("gain_cc")
            || k.startsWith ("gain_oncc")
            || (k.startsWith ("ampeg_") && (k.contains ("_oncc") || k.contains ("_curvecc")
                                            || k.contains ("_cc")));
    }
}

SlideRegionMap extractSlideRegions (const juce::File& programSfz)
{
    SlideRegionMap map;
    map.sourceProgram = programSfz.getFileName();
    if (! programSfz.existsAsFile()) return map;

    // sfizz resolves `sample=` relative to the top-level program's directory.
    const juce::File sampleBase = programSfz.getParentDirectory();

    juce::StringArray lines;
    expandIncludes (programSfz, 0, lines);

    // SFZ opcode inheritance: region > group > master > global > control.
    using OpMap = std::map<juce::String, juce::String>;
    OpMap cOp, gOp, mOp, grpOp, rOp;

    enum class Level { Control, Global, Master, Group, Region, Curve, Other };
    Level level = Level::Control;
    bool  inRegion = false;

    // Custom <curve> capture (curve_index + vNNN=val breakpoints).
    SlideCurve curCurve;
    auto finalizeCurve = [&] ()
    {
        if (level == Level::Curve && curCurve.index >= 0 && ! curCurve.points.empty())
        {
            std::sort (curCurve.points.begin(), curCurve.points.end());
            map.curves.push_back (curCurve);
        }
        curCurve = SlideCurve();
    };

    auto effGet = [&] (const char* k) -> juce::String
    {
        const juce::String key (k);
        if (rOp.count (key))   return rOp.at (key);
        if (grpOp.count (key)) return grpOp.at (key);
        if (mOp.count (key))   return mOp.at (key);
        if (gOp.count (key))   return gOp.at (key);
        if (cOp.count (key))   return cOp.at (key);
        return {};
    };
    auto effHas = [&] (const char* k) -> bool
    {
        const juce::String key (k);
        return rOp.count (key) || grpOp.count (key) || mOp.count (key)
            || gOp.count (key) || cOp.count (key);
    };

    // Keyswitch-less programs land in the swLast == -1 articulation (Task 10:
    // staccato/twang-only programs now extract instead of going silent).
    // sw_default is tracked as it streams by -- a later <global> header CLEARS
    // gOp, so a post-parse scope read would lose it.
    juce::String lastSwDefault;
    int missing = 0;

    auto artFor = [&] (int swLast) -> SlideArticulation&
    {
        for (auto& a : map.articulations)
            if (a.swLast == swLast) return a;
        SlideArticulation a;
        a.swLast = swLast;
        map.articulations.push_back (std::move (a));
        return map.articulations.back();
    };

    auto finalizeRegion = [&] ()
    {
        if (! inRegion) return;
        inRegion = false;

        if (! rOp.count ("sample")) return;

        // Articulation key: sw_last when present, else the -1 bucket.
        const juce::String swl = effGet ("sw_last");
        const int swLast = swl.isNotEmpty() ? swl.getIntValue() : -1;

        // Layer classification (before any drop): the Task-10 sets.
        const bool isUnison    = effHas ("locc100") && effGet ("locc100").getIntValue() >= 1;
        const bool isTailpiece = effHas ("locc118") && effGet ("locc118").getIntValue() >= 1;
        const bool isFbGated   = effHas ("locc29")  && effGet ("locc29").getIntValue()  >= 1;
        const bool isLooped    = effHas ("loop_mode")
                                 && ! effGet ("loop_mode").equalsIgnoreCase ("no_loop");
        const bool isRelease   = effGet ("trigger").equalsIgnoreCase ("release");
        const bool isMonoSet   = effHas ("locc105") && effGet ("locc105").getIntValue() >= 1;

        auto& art = artFor (swLast);

        // Bass cc105 Mono set: capture the choke policy once, zones come from
        // the poly set (identical samples; only group policy differs).
        if (isMonoSet)
        {
            art.hasMonoSet  = true;
            art.monoGroup   = effGet ("group").getIntValue();
            art.monoOffBy   = effGet ("off_by").getIntValue();
            if (effHas ("off_time")) art.monoOffTime = effGet ("off_time").getFloatValue();
            return;
        }

        // Bend range: center sustain regions only (articulation-level; the
        // feedback/noise layers carry bend_up=0 by design).
        if (! isUnison && ! isTailpiece && ! isFbGated && ! isRelease && ! isLooped)
        {
            const juce::String bu = effGet ("bend_up");
            if (bu.isNotEmpty())
            {
                const int c = bu.getIntValue();
                if (c > 0 && (art.bendUpCents == SlideArticulation::kBendUnset || c > art.bendUpCents))
                    art.bendUpCents = c;
            }
            const juce::String bd = effGet ("bend_down");
            if (bd.isNotEmpty())
            {
                const int c = bd.getIntValue();
                if (art.bendDownCents == SlideArticulation::kBendUnset || c < art.bendDownCents)
                    art.bendDownCents = c;
            }
        }

        // Default round-robin only (RR variation is inaudible across a slide).
        const juce::String seqPos = effGet ("seq_position");
        if (seqPos.isNotEmpty() && seqPos.getIntValue() != 1) return;

        const auto samp = rOp.at ("sample").replaceCharacter ('\\', '/').trim();
        const juce::File wav = sampleBase.getChildFile (samp);
        if (! wav.existsAsFile()) { ++missing; return; }

        SlideSample s;
        s.wav = wav;
        const juce::String kc = effGet ("pitch_keycenter");
        s.rootKey = kc.isNotEmpty() ? kc.getIntValue()
                  : effHas ("key")  ? effGet ("key").getIntValue()
                                    : effGet ("lokey").getIntValue();
        s.loKey = effHas ("lokey") ? effGet ("lokey").getIntValue() : s.rootKey;
        s.hiKey = effHas ("hikey") ? effGet ("hikey").getIntValue() : s.rootKey;
        s.loVel = effHas ("lovel") ? effGet ("lovel").getIntValue() : 1;
        s.hiVel = effHas ("hivel") ? effGet ("hivel").getIntValue() : 127;
        s.offsetFrames = effGet ("offset").getIntValue();
        s.loopStart = effHas ("loop_start") ? effGet ("loop_start").getIntValue() : -1;
        s.loopEnd   = effHas ("loop_end")   ? effGet ("loop_end").getIntValue()   : -1;
        s.loopMode  = isLooped ? 1 : 0;
        s.tuneCents = effGet ("tune").getIntValue();

        // Merge the scope maps ascending (higher scopes override) ONCE, then
        // read every remaining field from the merged view -- one pass instead
        // of per-key 5-map probes (matters at load: thousands of regions).
        OpMap eff = cOp;
        for (const auto& kv : gOp)   eff[kv.first] = kv.second;
        for (const auto& kv : mOp)   eff[kv.first] = kv.second;
        for (const auto& kv : grpOp) eff[kv.first] = kv.second;
        for (const auto& kv : rOp)   eff[kv.first] = kv.second;

        auto effF = [&eff] (const char* k, float dflt) -> float
        {
            const auto it = eff.find (juce::String (k));
            return it != eff.end() ? it->second.getFloatValue() : dflt;
        };
        auto effI = [&eff] (const char* k, int dflt) -> int
        {
            const auto it = eff.find (juce::String (k));
            return it != eff.end() ? it->second.getIntValue() : dflt;
        };

        // Gain staging + envelope statics (effective, scope-inherited).
        s.volumeDb      = effF ("volume",        0.0f);
        s.amplitudePct  = effF ("amplitude",   100.0f);
        s.ampVeltrack   = effF ("amp_veltrack",100.0f);
        s.ampegDelay    = effF ("ampeg_delay",   0.0f);
        s.ampegAttack   = effF ("ampeg_attack",  0.0f);
        s.ampegHold     = effF ("ampeg_hold",    0.0f);
        s.ampegDecay    = effF ("ampeg_decay",   0.0f);
        s.ampegSustain  = effF ("ampeg_sustain",100.0f);
        s.ampegRelease  = effF ("ampeg_release", 0.0f);
        s.group         = effI ("group", 0);
        s.offBy         = effI ("off_by", 0);
        s.notePolyphony = effI ("note_polyphony", 0);
        s.offTime       = effF ("off_time", 0.0f);
        s.rtDecay       = effF ("rt_decay", 0.0f);

        // amp_velcurve_N points + the modulation families: one scan of the
        // merged view.
        for (const auto& kv : eff)
        {
            if (kv.first.startsWith ("amp_velcurve_"))
            {
                const int v = kv.first.substring (13).getIntValue();
                if (v >= 1 && v <= 127)
                    s.velcurve.push_back ({ v, kv.second.getFloatValue() });
            }
            else if (isModFamilyOpcode (kv.first))
                s.modOps.push_back ({ kv.first, kv.second });
        }
        std::sort (s.velcurve.begin(), s.velcurve.end());

        // Route to the layer set.
        std::vector<SlideSample>* dest = nullptr;
        if      (isRelease)   dest = &art.releases;
        else if (isLooped)    dest = &art.noise;
        else if (isFbGated)   dest = &art.feedback;
        else if (isTailpiece) dest = &art.tailpiece;
        else if (isUnison)    dest = (s.rootKey >= s.loKey + 1) ? &art.tUp
                                   : (s.rootKey <= s.loKey - 1) ? &art.tDown
                                   : &art.tUp;   // same-key unison (rare): treat as tUp
        else                  dest = &art.center;

        // Dedup per (layer, rootKey, band) -- a map included under two
        // qualifying masters must not double an entry.
        for (const auto& e : *dest)
            if (e.rootKey == s.rootKey && e.loVel == s.loVel && e.hiVel == s.hiVel
                && e.loKey == s.loKey)
                return;

        dest->push_back (std::move (s));
    };

    for (const auto& raw : lines)
    {
        const auto t = stripComment (raw);
        if (t.isEmpty()) continue;

        juce::StringArray headers;
        std::vector<std::pair<juce::String, juce::String>> ops;
        tokenizeSfzLine (t, headers, ops);

        for (const auto& hdr : headers)
        {
            finalizeRegion();
            finalizeCurve();
            if      (hdr == "control") { level = Level::Control; }
            else if (hdr == "global")  { gOp.clear(); mOp.clear(); grpOp.clear(); rOp.clear(); level = Level::Global; }
            else if (hdr == "master")  { mOp.clear(); grpOp.clear(); rOp.clear(); level = Level::Master; }
            else if (hdr == "group")   { grpOp.clear(); rOp.clear(); level = Level::Group; }
            else if (hdr == "region")  { rOp.clear(); inRegion = true; level = Level::Region; }
            else if (hdr == "curve")   { level = Level::Curve; }
            else                       { level = Level::Other; }   // <effect>/<sample>...
        }

        for (const auto& [key, val] : ops)
        {
            if (key == "sw_default")
                lastSwDefault = val;
            switch (level)
            {
                case Level::Region:  rOp[key]   = val; break;
                case Level::Group:   grpOp[key] = val; break;
                case Level::Master:  mOp[key]   = val; break;
                case Level::Global:  gOp[key]   = val; break;
                case Level::Control: cOp[key]   = val; break;
                case Level::Curve:
                    if (key == "curve_index")
                        curCurve.index = val.getIntValue();
                    else if (key.startsWithChar ('v') && key.length() >= 2)
                    {
                        const int pos = key.substring (1).getIntValue();
                        if (pos >= 0 && pos <= 127)
                            curCurve.points.push_back ({ (float) pos / 127.0f,
                                                         val.getFloatValue() });
                    }
                    break;
                case Level::Other:
                default:                               break;
            }
        }
    }
    finalizeRegion();
    finalizeCurve();

    // Order articulations (the -1 single-articulation bucket first, then by
    // keyswitch note) and resolve the default: sw_default's articulation when
    // present, else the first with any center content.
    std::sort (map.articulations.begin(), map.articulations.end(),
               [] (const SlideArticulation& a, const SlideArticulation& b)
               { return a.swLast < b.swLast; });
    {
        map.defaultArticulation = -1;
        if (lastSwDefault.isNotEmpty())
        {
            const int want = lastSwDefault.getIntValue();
            for (int i = 0; i < (int) map.articulations.size(); ++i)
                if (map.articulations[(size_t) i].swLast == want
                    && ! map.articulations[(size_t) i].center.empty())
                    map.defaultArticulation = i;
        }
        if (map.defaultArticulation < 0)
            for (int i = 0; i < (int) map.articulations.size(); ++i)
                if (! map.articulations[(size_t) i].center.empty())
                { map.defaultArticulation = i; break; }
    }

    // Compat mirror: pre-Task-11 consumers (SlideSampler zone build, the Bend
    // dropdown range) read the DEFAULT articulation's center set.
    if (map.defaultArticulation >= 0)
    {
        const auto& d = map.articulations[(size_t) map.defaultArticulation];
        map.samples       = d.center;
        map.bendUpCents   = d.bendUpCents;
        map.bendDownCents = d.bendDownCents;
    }

   #if JUCE_DEBUG
    {
        juce::String msg;
        msg << "[SlideSampler] " << map.sourceProgram << ": "
            << (int) map.articulations.size() << " articulation(s), default #"
            << map.defaultArticulation << ", " << (int) map.curves.size()
            << " curve table(s), " << missing << " path(s) missing/skipped";
        for (const auto& a : map.articulations)
        {
            msg << "\n[SlideSampler]   sw " << a.swLast
                << ": center " << (int) a.center.size()
                << " / tUp "   << (int) a.tUp.size()
                << " / tDn "   << (int) a.tDown.size()
                << " / tp "    << (int) a.tailpiece.size()
                << " / fb "    << (int) a.feedback.size()
                << " / noise " << (int) a.noise.size()
                << " / rel "   << (int) a.releases.size()
                << (a.hasMonoSet ? " (mono set)" : "");
        }
        DBG (msg);
    }
   #else
    juce::ignoreUnused (missing);
   #endif

    return map;
}
