#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>

// -- LoudnessSpec ---------------------------------------------------------------
// ONE table of delivery loudness targets, shared by every surface that names a
// loudness number: the export dialog's normalize target, the maximizer's target
// line, and the CL-227 conformance report's selectable spec.  Kept in one place
// because three independently-maintained copies of "-14 LUFS, -1 dBTP" is how
// the export ends up disagreeing with the meter that approved it.
//
// DOMAIN REFERENCES (Rule 6 cat. 3):
//   EBU R128        -- programme loudness -23 LUFS, +-0.5 LU tolerance,
//                      max true peak -1 dBTP.
//   ATSC A/85       -- -24 LKFS anchor, +-2 LU practical tolerance,
//                      max true peak -2 dBTP.
//   ITU-R BS.1770-4 -- defines HOW to measure (K-weighting + gating), not what
//                      to hit.  Carried in the list because the report offers it,
//                      with no integrated target: `checksIntegrated == false`.
//   Streaming       -- the normalization points the major services apply
//                      (approximately -14 LUFS; Apple Music / spoken word -16),
//                      with -1 dBTP of margin for lossy transcode overshoot.
// ------------------------------------------------------------------------------
struct LoudnessSpec
{
    enum class Id
    {
        Streaming14 = 0,   // -14 LUFS  (Spotify / YouTube / Tidal normalization)
        Streaming16,       // -16 LUFS  (Apple Music, spoken word)
        EbuR128,           // -23 LUFS
        AtscA85,           // -24 LKFS
        Bs1770Measure,     // measurement only, no integrated target
        Custom,
        Count
    };

    Id          id                { Id::Streaming14 };
    const char* name              { "Streaming (-14 LUFS)" };
    float       integratedLufs    { -14.0f };
    float       toleranceLu       {   1.0f };   // +- around integratedLufs
    float       maxTruePeakDb     {  -1.0f };
    // Short-term ceiling.  Only EBU-style delivery bothers with one; where a
    // spec does not define it this sits at 0 and shortTermChecked is false.
    float       maxShortTermLufs  {   0.0f };
    bool        checksIntegrated  { true  };
    bool        checksShortTerm   { false };

    static const LoudnessSpec& get (Id which) noexcept
    {
        static const std::array<LoudnessSpec, (size_t) Id::Count> table
        {{
            { Id::Streaming14,   "Streaming (-14 LUFS)",   -14.0f, 1.0f, -1.0f,  0.0f, true,  false },
            { Id::Streaming16,   "Streaming (-16 LUFS)",   -16.0f, 1.0f, -1.0f,  0.0f, true,  false },
            { Id::EbuR128,       "EBU R128 (-23 LUFS)",    -23.0f, 0.5f, -1.0f,  0.0f, true,  false },
            { Id::AtscA85,       "ATSC A/85 (-24 LKFS)",   -24.0f, 2.0f, -2.0f,  0.0f, true,  false },
            { Id::Bs1770Measure, "ITU-R BS.1770-4 (measure only)",
                                                             0.0f, 0.0f, -1.0f,  0.0f, false, false },
            { Id::Custom,        "Custom",                 -14.0f, 1.0f, -1.0f,  0.0f, true,  false },
        }};

        const auto i = (size_t) juce::jlimit (0, (int) Id::Count - 1, (int) which);
        return table[i];
    }

    // TS7 §4.2: Custom's target is the USER'S typed number, not the table's
    // placeholder.  The table cannot hold it (one shared static), so a resolved
    // spec is a COPY with integratedLufs replaced.  EVERY verdict and every
    // report line must come through here -- reading get(Id::Custom) directly is
    // how "Custom, -18" silently measured and reported against -14.
    static LoudnessSpec resolved (Id which, float customLufs) noexcept
    {
        LoudnessSpec s = get (which);
        if (which == Id::Custom) s.integratedLufs = customLufs;
        return s;
    }

    // Menu / combo population -- one loop, so no surface can list a subset and
    // then index the table by its own position.
    static int numSpecs() noexcept { return (int) Id::Count; }
    static const char* nameOf (int index) noexcept { return get ((Id) index).name; }
};

// -- LoudnessViolation ----------------------------------------------------------
// A timecoded span where the signal broke one of the spec's limits, plus the ONE
// coalescing rule that builds them.
//
// Lives here rather than inside MeasureResult because TWO producers now need it
// (Jeff, 2026-07-30: flagged moments must work for captured playback takes as
// well as rendered measurements) -- the offline render scan and the live capture
// poll.  Two copies of "consecutive breaches within kGapSeconds are one span"
// would be two rules that drift, and §5.3 is explicit that coalescing happens at
// CAPTURE, not in the report.
struct LoudnessViolation
{
    enum class Kind { TruePeak, ShortTerm };
    Kind   kind       { Kind::TruePeak };
    double startSecs  { 0.0 };
    double endSecs    { 0.0 };
    float  worstValue { 0.0f };   // dBTP for TruePeak, LUFS for ShortTerm

    static constexpr double kGapSeconds = 0.5;
    // A pathological signal must not grow the report unbounded.  Past this the
    // scan stops ADDING rows but keeps measuring, and the report says it was
    // truncated -- a silent cap would read as "only 400 problems".
    static constexpr int    kMaxRows    = 400;

    // Extends the previous span when this breach continues it, else starts a new
    // one.  `truncated` is set (never cleared) when the row budget is reached.
    static void addOrExtend (std::vector<LoudnessViolation>& list, Kind kind,
                             double startSecs, double endSecs, float value,
                             bool& truncated)
    {
        if (! list.empty())
        {
            auto& last = list.back();
            if (last.kind == kind && startSecs - last.endSecs <= kGapSeconds)
            {
                last.endSecs    = endSecs;
                last.worstValue = juce::jmax (last.worstValue, value);
                return;
            }
        }
        if ((int) list.size() >= kMaxRows) { truncated = true; return; }
        list.push_back ({ kind, startSecs, endSecs, value });
    }
};

// -- LoudnessRangeAccumulator ---------------------------------------------------
// EBU Tech 3342 Loudness Range (LRA) from a stream of Short-Term (3 s) loudness
// readings: gate at -70 LUFS absolute, then at (gated mean - 20 LU) relative,
// and report the span between the 10th and 95th percentiles of what survives.
//
// Fixed histogram, so it is allocation-free once constructed and safe to feed
// from a render thread.  0.1 LU bins over -70 .. +5 LUFS matches the resolution
// LufsMeterDSP's own integrated gate uses.
class LoudnessRangeAccumulator
{
public:
    void reset() noexcept
    {
        mCount.fill (0);
        mTotal = 0;
    }

    // Feed one Short-Term reading (LUFS).  Values below the absolute gate are
    // discarded here rather than binned, exactly as the relative gate expects.
    void push (float shortTermLufs) noexcept
    {
        if (shortTermLufs < kAbsGateLufs) return;
        const int bin = juce::jlimit (0, kBins - 1,
                                      (int) std::lround ((shortTermLufs - kMinLufs) / kStepLu));
        ++mCount[(size_t) bin];
        ++mTotal;
    }

    // LRA in LU.  Returns 0 when there is not enough gated material to describe
    // a range (silence, or a clip shorter than one short-term window).
    float lra() const noexcept
    {
        if (mTotal <= 0) return 0.0f;

        // Gated mean of the absolute-gated set, in the energy domain -- averaging
        // in dB would bias the relative gate.
        double sumMs = 0.0;
        int    n     = 0;
        for (int b = 0; b < kBins; ++b)
        {
            if (mCount[(size_t) b] == 0) continue;
            const double lufs = kMinLufs + (double) b * kStepLu;
            sumMs += (double) mCount[(size_t) b] * std::pow (10.0, lufs / 10.0);
            n     += mCount[(size_t) b];
        }
        if (n <= 0) return 0.0f;

        const double meanLufs  = 10.0 * std::log10 (sumMs / (double) n);
        const double relGate   = meanLufs + kRelGateLu;
        const int    gateBin   = juce::jlimit (0, kBins - 1,
                                    (int) std::lround ((relGate - kMinLufs) / kStepLu));

        int gatedTotal = 0;
        for (int b = gateBin; b < kBins; ++b) gatedTotal += mCount[(size_t) b];
        if (gatedTotal <= 0) return 0.0f;

        const auto percentileLufs = [&] (double frac) -> double
        {
            const int wanted = juce::jmax (1, (int) std::lround (frac * (double) gatedTotal));
            int running = 0;
            for (int b = gateBin; b < kBins; ++b)
            {
                running += mCount[(size_t) b];
                if (running >= wanted) return kMinLufs + (double) b * kStepLu;
            }
            return kMinLufs + (double) (kBins - 1) * kStepLu;
        };

        return (float) juce::jmax (0.0, percentileLufs (0.95) - percentileLufs (0.10));
    }

private:
    static constexpr double kMinLufs     = -70.0;
    static constexpr double kStepLu      =   0.1;
    static constexpr int    kBins        = 751;    // -70.0 .. +5.0 inclusive
    static constexpr double kAbsGateLufs = -70.0;
    static constexpr double kRelGateLu   = -20.0;  // Tech 3342 uses -20 LU, not R128's -10

    std::array<int, kBins> mCount { };
    int                    mTotal { 0 };
};
