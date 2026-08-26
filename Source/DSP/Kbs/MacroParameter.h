// KBS Plugins — macro parameter layer
//
// One user control drives several internal DSP parameters, each across its own
// range with its own curve. Advanced mode exposes those internal parameters
// directly, clamped to exactly the span the macro would have swept, so the two
// views can never disagree about what states are reachable.
//
// Pure C++: no JUCE, so it can be unit-tested without building a plugin.
#pragma once

#include "TargetNames.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace kbs {

// ── a DSP parameter in natural units (Hz, dB, ms, ratio) ───────────────────
struct Param
{
    float value = 0.0f;
    float min = 0.0f;
    float max = 1.0f;
    const char* unit = "";

    void set (float v) { value = std::clamp (v, std::min (min, max), std::max (min, max)); }
};

// Devices register their parameters here under stable string ids such as
// "eqMid/gain". Macro links address them by that id.
class ParamRegistry
{
public:
    void add (const std::string& id, Param* p) { map[id] = p; }

    Param* find (const std::string& id) const
    {
        auto it = map.find (id);
        return it == map.end() ? nullptr : it->second;
    }

    std::vector<std::string> ids() const
    {
        std::vector<std::string> out;
        out.reserve (map.size());
        for (auto& kv : map) out.push_back (kv.first);
        std::sort (out.begin(), out.end());
        return out;
    }

private:
    std::unordered_map<std::string, Param*> map;
};

// ── curve shaping ──────────────────────────────────────────────────────────
// skew 0 is linear travel. Positive skew reaches the top of the range sooner,
// negative skew later. Chosen for being monotonic and easy to fit — it is our
// curve, not a recovered one.
inline float applySkew (float t, float skew)
{
    t = std::clamp (t, 0.0f, 1.0f);
    if (std::abs (skew) < 1.0e-6f) return t;
    const float exponent = std::exp (-2.0f * std::clamp (skew, -0.99f, 0.99f));
    return std::pow (t, exponent);
}

// ── one macro -> one DSP parameter ─────────────────────────────────────────
//
// Two ways to define the mapping:
//   parametric  — rangeStart/rangeEnd plus a curve (log for frequencies)
//   measured    — breakpoints straight from the measurement rig, interpolated
//
// The measured form is preferred wherever we have data: it reproduces what was
// actually observed instead of assuming a curve shape fits.
struct MacroLink
{
    std::string target;

    float rangeStart = 0.0f;      // value when the macro sits at 0
    float rangeEnd = 1.0f;        // value when the macro sits at 1
    bool logarithmic = false;     // interpolate in log space (use for Hz)
    float skew = 0.0f;

    // breakpoints as {macro position 0..1, natural value}, ascending
    std::vector<std::pair<float, float>> table;

    // advanced-mode override: breaks this one link, leaves the rest driven
    bool overridden = false;
    float overrideValue = 0.0f;

    // ── the span the user has chosen, if any ──────────────────────────────
    //
    // Setup lets somebody move where a macro sweeps, because the factory span
    // was placed against one person's source. A guitar in D standard puts its
    // palm mute somewhere a guitar in E standard does not.
    //
    // It is stored as a replacement span rather than as edits to the curve, so
    // the shape survives: a breakpoint table keeps its contour, a log sweep
    // stays logarithmic, a skewed range keeps its skew. Only the two ends move.
    // That is what makes one uniform control work for all three kinds of link.
    bool customised = false;
    float customLo = 0.0f, customHi = 1.0f;

    // ── a link the macro holds still ──────────────────────────────────────
    //
    // Most links sweep: turn the macro and the value travels its span. A parked
    // link does not. The macro sets it once, to parkValue, and leaves it there.
    // rangeStart and rangeEnd still describe where it may go — that is what the
    // advanced knob travels and what the setup row edits — they just are not a
    // path the macro walks.
    //
    // Makeup amount is what this exists for. How much of the level a compressor
    // took should be handed back is a decision about the mix, and "more
    // compression" has no business dragging it along. Making it a link anyway is
    // what puts it on the setup page and the advanced page beside everything
    // else, rather than inventing a second route into the DSP for one control.
    //
    // For a parked link customLo carries the moved park value and customHi is
    // unused, so it saves, restores and reports as customised through exactly
    // the same machinery as a span.
    bool parked = false;
    float parkValue = 0.0f;

    // What the factory intended, before any of that.
    float factoryValueAt (float macro01) const
    {
        if (parked) return parkValue;
        if (! table.empty()) return interpolateTable (std::clamp (macro01, 0.0f, 1.0f));

        const float s = applySkew (macro01, skew);
        if (logarithmic && rangeStart > 0.0f && rangeEnd > 0.0f)
            return rangeStart * std::pow (rangeEnd / rangeStart, s);
        return rangeStart + s * (rangeEnd - rangeStart);
    }

    std::pair<float, float> factoryBounds() const
    {
        float lo, hi;
        if (! table.empty())
        {
            lo = hi = table.front().second;
            for (auto& p : table) { lo = std::min (lo, p.second); hi = std::max (hi, p.second); }
        }
        else
        {
            lo = std::min (rangeStart, rangeEnd);
            hi = std::max (rangeStart, rangeEnd);
        }
        return { lo, hi };
    }

    float valueAt (float macro01) const
    {
        // A park has no curve to remap - there is one number and the user has
        // either moved it or not.
        if (parked) return customised ? customLo : parkValue;

        const float v = factoryValueAt (macro01);
        if (! customised) return v;

        const auto fb = factoryBounds();
        const float span = fb.second - fb.first;
        if (span < 1.0e-9f) return customLo;

        // Remap in the same space the link travels in, or a frequency sweep
        // would stop being logarithmic the moment somebody moved its ends.
        if (logarithmic && fb.first > 0.0f && v > 0.0f
            && customLo > 0.0f && customHi > 0.0f)
        {
            const float t = std::log (v / fb.first) / std::log (fb.second / fb.first);
            return customLo * std::pow (customHi / customLo, t);
        }

        const float t = (v - fb.first) / span;
        return customLo + t * (customHi - customLo);
    }

    // ── addressing a link by proportion rather than by value ──────────────
    //
    // Advanced mode holds "seven tenths of the way along this link" rather
    // than "3.2 kHz". A host parameter's range is fixed for the life of the
    // plugin, so a parameter carrying Hz would keep the factory ends forever:
    // move the span in Setup and the knob would have travel that does nothing
    // at one end and values it could not reach at the other. A proportion is
    // true to whatever the span currently is.
    //
    // The conversion travels in the link's own space, so a frequency stays
    // logarithmic and the middle of the knob stays the middle of the sweep.
    float normalisedOf (float natural) const
    {
        const auto b = bounds();
        if (b.second - b.first < 1.0e-9f) return 0.0f;

        if (logarithmic && b.first > 0.0f && b.second > 0.0f && natural > 0.0f)
            return std::clamp ((float) (std::log (natural / b.first)
                                        / std::log (b.second / b.first)), 0.0f, 1.0f);

        return std::clamp ((natural - b.first) / (b.second - b.first), 0.0f, 1.0f);
    }

    float naturalOf (float t) const
    {
        const auto b = bounds();
        t = std::clamp (t, 0.0f, 1.0f);
        if (b.second - b.first < 1.0e-9f) return b.first;

        if (logarithmic && b.first > 0.0f && b.second > 0.0f)
            return b.first * std::pow (b.second / b.first, t);

        return b.first + t * (b.second - b.first);
    }

    // The bounds advanced mode is allowed to move within — exactly the span the
    // macro sweeps, so advanced mode can never reach an unreachable state.
    std::pair<float, float> bounds() const
    {
        // A parked link's ends are where the knob may go, and moving the park
        // must not shrink that. Narrowing it here would be the bug this whole
        // proportional scheme exists to avoid, one level down.
        if (parked) return factoryBounds();

        if (customised)
            return { std::min (customLo, customHi), std::max (customLo, customHi) };
        return factoryBounds();
    }

private:
    float interpolateTable (float t) const
    {
        if (t <= table.front().first)  return table.front().second;
        if (t >= table.back().first)   return table.back().second;

        for (size_t i = 1; i < table.size(); ++i)
        {
            if (t <= table[i].first)
            {
                const auto& a = table[i - 1];
                const auto& b = table[i];
                const float span = b.first - a.first;
                const float u = span > 1.0e-9f ? (t - a.first) / span : 0.0f;
                return a.second + u * (b.second - a.second);
            }
        }
        return table.back().second;
    }
};

// A link the macro parks rather than sweeps. It may travel lo..hi, but only
// because somebody moved it: the macro itself just sets it to park and leaves.
inline MacroLink parkedLink (std::string target, float lo, float hi, float park)
{
    MacroLink l;
    l.target     = std::move (target);
    l.rangeStart = lo;
    l.rangeEnd   = hi;
    l.parked     = true;
    l.parkValue  = park;
    return l;
}

// ── which links advanced mode puts a knob on ───────────────────────────────
//
// The processor registers a parameter pair from this and the editor draws a
// knob from it, and the two must agree exactly or a knob appears with nothing
// behind it. They used to ask the question separately with a comment on each
// saying it had to match the other, which is a warning rather than a mechanism.
//
// The rule, in one place:
//
//   A macro driving a single swept parameter *is* that parameter - its own knob
//   edits it directly, so a second knob would just be a copy of it. Two or more
//   and each one needs a control of its own. That is what stopped roughly five
//   hundred entries with no control behind them reaching hosts.
//
//   A parked link is never swept by the macro, so advanced mode is the only
//   place it can be reached and it always gets a knob. It is deliberately not
//   counted towards the pair above: if it were, adding one to a single-link
//   macro would make that link sprout a duplicate of the macro knob.
//
// Returned in link order, so knobs read in the order the product declares them.
inline std::vector<size_t> exposedLinks (const std::vector<MacroLink>& links)
{
    std::vector<size_t> swept, out;

    for (size_t l = 0; l < links.size(); ++l)
    {
        if (isSelector (links[l].target)) continue;      // an index, nothing to detach
        const auto& t = links[l].target;
        if (t.size() >= 10 && t.compare (t.size() - 10, 10, "/lookahead") == 0)
            continue;                                    // the panel toggle owns this switch
        const auto b = links[l].bounds();
        if (b.second - b.first < 1.0e-6f) continue;      // fixed: no travel to give

        if (links[l].parked) out.push_back (l);
        else                 swept.push_back (l);
    }

    if (swept.size() >= 2)
        out.insert (out.end(), swept.begin(), swept.end());

    std::sort (out.begin(), out.end());
    return out;
}

// The link a macro knob *is*, when it sweeps exactly one parameter - or -1 when
// it sweeps several and can only honestly read as a percentage.
//
// Parked links are ignored here on purpose. The macro does not move them, so
// one sitting alongside must not stop its knob reading in real units: adding a
// makeup control to Saturate should not turn its readout from "62 %" into a
// percentage of something unnamed.
inline int soleSweptLink (const std::vector<MacroLink>& links)
{
    int found = -1;

    for (size_t l = 0; l < links.size(); ++l)
    {
        if (isSelector (links[l].target) || links[l].parked) continue;
        const auto b = links[l].bounds();
        if (b.second - b.first < 1.0e-6f) continue;

        if (found >= 0) return -1;      // more than one
        found = (int) l;
    }

    return found;
}

// ── one link's span, as the user has it ────────────────────────────────────
//
// The processor owns the authoritative list of these on the message thread and
// publishes it to the audio thread. Setup edits a copy and hands the whole list
// back, rather than reaching into a link the audio thread is reading.
struct SpanEdit
{
    int   macro = 0, link = 0;
    bool  customised = false;
    float lo = 0.0f, hi = 1.0f;

    // A parked link carries its one value in lo, with hi the same. Recorded
    // here rather than looked up, because everything that reconstructs this
    // list starts from the factory copy and patches it - so a flag set once at
    // the source survives a session, a file and a restore without a second
    // place having to remember.
    bool  parked = false;
};

// ── the user-facing control ────────────────────────────────────────────────
class MacroParameter
{
public:
    MacroParameter (std::string name, float defaultPos = 0.0f)
        : displayName (std::move (name)), position (defaultPos) {}

    MacroParameter& link (MacroLink l) { links.push_back (std::move (l)); return *this; }

    // One line on what the knob DOES, in the owner's voice. This exact text
    // is the knob's tooltip and the manual's description - one source, so
    // the two can never disagree. Ranges are not welcome here: they live on
    // the Setup page and in the control reference, which is their job.
    MacroParameter& describe (std::string text) { blurb = std::move (text); return *this; }
    const std::string& getBlurb() const { return blurb; }

    void setPosition (float p) { position = std::clamp (p, 0.0f, 1.0f); }
    float getPosition() const { return position; }
    const std::string& getName() const { return displayName; }

    // Push this macro's value into every linked parameter that is not overridden.
    void apply (const ParamRegistry& reg) const
    {
        for (const auto& l : links)
            if (auto* p = reg.find (l.target))
                p->set (l.overridden ? l.overrideValue : l.valueAt (position));
    }

    // ── advanced mode ──────────────────────────────────────────────────────
    // Overriding one link detaches only that link. Everything else keeps
    // following the knob, which is what makes the two views coexist.
    void overrideLink (size_t index, float naturalValue)
    {
        if (index >= links.size()) return;
        auto& l = links[index];
        const auto b = l.bounds();
        l.overridden = true;
        l.overrideValue = std::clamp (naturalValue, b.first, b.second);
    }

    void releaseLink (size_t index)
    {
        if (index < links.size()) links[index].overridden = false;
    }

    void releaseAll() { for (auto& l : links) l.overridden = false; }
    bool anyOverridden() const
    {
        for (auto& l : links) if (l.overridden) return true;
        return false;
    }

    // Seed the advanced view so switching modes changes nothing audible.
    void seedOverridesFromPosition()
    {
        for (auto& l : links) l.overrideValue = l.valueAt (position);
    }

    const std::vector<MacroLink>& getLinks() const { return links; }
    std::vector<MacroLink>& getLinks() { return links; }

private:
    std::string displayName;
    std::string blurb;
    float position = 0.0f;
    std::vector<MacroLink> links;
};

} // namespace kbs
