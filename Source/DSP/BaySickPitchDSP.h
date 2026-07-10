#pragma once
#include <JuceHeader.h>
#include "PitchShifters.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchDSP - QA-Fa Task 1 (2026-07-10)
// ─────────────────────────────────────────────────────────────────────────────
// Composite-driven note-level pitch editor engine (the BaySickAlign sibling,
// section 14 of phantom-recording-mongoose).  Channel-level: the editor feeds
// it the channel composite (VibeSynthProcessor::renderChannelComposite);
// analysis runs YIN once over it and segments note regions with ABSOLUTE
// timeline positions.  User edits (pitch shift / formant / vibrato / volume
// shape) attach to regions and flow to playback through the REALTIME
// APPLICATOR (Mode C): during FilePlay the running strip audio is mapped to
// the overlapping region by timeline position and the stored edits are
// applied per block via the shared PitchShifters.  Render/Freeze is the
// opt-in bake (writes <project>/Pitched/{name}_pitch_v{N}.wav via the
// owning BaySickVocalProcessor).
//
// THREADING: analysis / edits / render are MESSAGE THREAD ONLY.  The
// applicator runs on the AUDIO THREAD and reads an immutable snapshot
// published via atomic pointer swap; retired snapshots are retained on the
// message thread (ring of 8) so the audio thread can never observe a freed
// snapshot -- it holds the pointer only within one block, and eight edit
// round-trips outlive any in-flight block by orders of magnitude.
// ─────────────────────────────────────────────────────────────────────────────

// One detected note (composite/timeline domain) + its user edits.
struct PitchNoteRegion
{
    // ── Detected at analysis ─────────────────────────────────────────────────
    double startSec       { 0.0 };    // seconds from composite sample 0
    double endSec         { 0.0 };
    float  midi           { 60.0f };  // median detected pitch (float MIDI)
    float  f0Hz           { 261.6f }; // median F0 (drives the PSOLA period)
    float  vibDepthCents  { 0.0f };   // detected natural vibrato
    float  vibRateHz      { 5.0f };

    // ── User edits (section 14c sub-curves + Edit-mode drags) ────────────────
    float  shiftSemis     { 0.0f };   // vertical pill drag
    float  formantSemis   { 0.0f };   // formant sub-curve amount
    float  vibDepthMult   { 1.0f };   // vibrato sub-curve depth (0 = flatten)
    float  vibRateMult    { 1.0f };   // vibrato sub-curve rate
    // Volume sub-curve: (t01, gain) points sorted by t01; empty = unity.
    std::vector<juce::Point<float>> volShape;

    bool hasEdits() const noexcept
    {
        return std::abs (shiftSemis) > 0.01f || std::abs (formantSemis) > 0.01f
            || std::abs (vibDepthMult - 1.0f) > 0.01f
            || std::abs (vibRateMult - 1.0f) > 0.01f
            || ! volShape.empty();
    }

    float volGainAt (float t01) const noexcept
    {
        if (volShape.empty()) return 1.0f;
        if (t01 <= volShape.front().x) return volShape.front().y;
        if (t01 >= volShape.back().x)  return volShape.back().y;
        for (size_t i = 0; i + 1 < volShape.size(); ++i)
            if (t01 >= volShape[i].x && t01 < volShape[i + 1].x)
            {
                const float d = volShape[i + 1].x - volShape[i].x;
                const float f = (d > 1.0e-6f) ? (t01 - volShape[i].x) / d : 0.0f;
                return volShape[i].y + f * (volShape[i + 1].y - volShape[i].y);
            }
        return volShape.back().y;
    }

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree v ("N");
        v.setProperty ("s",  startSec,      nullptr);
        v.setProperty ("e",  endSec,        nullptr);
        v.setProperty ("m",  midi,          nullptr);
        v.setProperty ("f0", f0Hz,          nullptr);
        v.setProperty ("vd", vibDepthCents, nullptr);
        v.setProperty ("vr", vibRateHz,     nullptr);
        v.setProperty ("sh", shiftSemis,    nullptr);
        v.setProperty ("fo", formantSemis,  nullptr);
        v.setProperty ("dm", vibDepthMult,  nullptr);
        v.setProperty ("rm", vibRateMult,   nullptr);
        juce::String pts;
        for (const auto& p : volShape)
            pts << p.x << "," << p.y << ";";
        v.setProperty ("vol", pts, nullptr);
        return v;
    }

    static PitchNoteRegion fromValueTree (const juce::ValueTree& v)
    {
        PitchNoteRegion r;
        r.startSec      = (double) v.getProperty ("s",  0.0);
        r.endSec        = (double) v.getProperty ("e",  0.0);
        r.midi          = (float)(double) v.getProperty ("m",  60.0);
        r.f0Hz          = (float)(double) v.getProperty ("f0", 261.6);
        r.vibDepthCents = (float)(double) v.getProperty ("vd", 0.0);
        r.vibRateHz     = (float)(double) v.getProperty ("vr", 5.0);
        r.shiftSemis    = (float)(double) v.getProperty ("sh", 0.0);
        r.formantSemis  = (float)(double) v.getProperty ("fo", 0.0);
        r.vibDepthMult  = (float)(double) v.getProperty ("dm", 1.0);
        r.vibRateMult   = (float)(double) v.getProperty ("rm", 1.0);
        juce::StringArray pts;
        pts.addTokens (v.getProperty ("vol", juce::String()).toString(), ";", "");
        for (const auto& tok : pts)
        {
            if (! tok.containsChar (',')) continue;
            r.volShape.push_back ({ tok.upToFirstOccurrenceOf (",", false, false).getFloatValue(),
                                    tok.fromFirstOccurrenceOf (",", false, false).getFloatValue() });
        }
        return r;
    }
};

class BaySickPitchDSP
{
public:
    BaySickPitchDSP();
    ~BaySickPitchDSP();

    void prepare (double sampleRate, int maxBlockSize);

    // ── Analysis (message thread) ────────────────────────────────────────────
    // Segments the composite into note regions (frame-YIN track -> voiced
    // grouping -> pitch-jump splits) + detects per-note vibrato.  Edits on
    // regions whose start still matches (+/-50 ms) are carried over.
    void analyzeComposite (const float* mono, int numSamples, double sampleRate,
                           double startBeat, juce::int64 startSample);

    bool   isAnalyzed()      const noexcept { return mAnalyzed; }
    double compositeSec()    const noexcept { return mCompositeSec; }
    double startBeat()       const noexcept { return mStartBeat; }
    juce::int64 startSample() const noexcept { return mStartSample; }
    double analysisSampleRate() const noexcept { return mAnalysisSr; }

    // Analyzed F0 track (hop = kF0Hop samples at the analysis rate) -- the
    // editor's Bass-green pitch curve.
    static constexpr int kF0Hop = 2048;
    const std::vector<float>& f0Track() const noexcept { return mF0Track; }

    // Message-thread edit surface (the editor mutates, then publishEdits()).
    std::vector<PitchNoteRegion>&       regions()       noexcept { return mRegions; }
    const std::vector<PitchNoteRegion>& regions() const noexcept { return mRegions; }

    // Rebuild + atomically publish the audio-thread snapshot.  Call after
    // every edit gesture completes (mouse-up), every analyze, and on load.
    void publishEdits();

    void clearAllEdits();   // Reset button: keeps regions, zeroes edits

    // ── Global knobs (CPU-guarded; pushed per block from bsp_ params) ────────
    void setFocus01   (float v);   // 0..1: pull note centers to the nearest semitone
    void setModAmount (float v);   // 0..2: vibrato preservation scale (1 = natural)
    void setSpeedMs   (float ms);  // 5..300: note-transition glide
    // QA-Fa recovery: bsp_on chain switch.  OFF glides every target to
    // neutral through the Speed smoothing (no hard switch), then the fast
    // path disengages once settled.
    void setChainOn   (bool on);

    // ── Realtime applicator (AUDIO THREAD, FilePlay only -- Mode C) ──────────
    // timelineStartSample = the block's first timeline sample (stamped by
    // finalizeFilePlayStrip next to setForcePitchBypass).  Fast-path: a
    // single atomic load bails when no edits exist (zero-edit channels cost
    // one load per block).
    void processFilePlay (juce::AudioBuffer<float>& buffer,
                          juce::int64 timelineStartSample) noexcept;

    // ── Offline render (message thread; the Render/Freeze bake) ─────────────
    // Applies focus/vibrato/edits to the composite exactly like the realtime
    // path, but offline over the whole buffer.  Returns the processed mono.
    juce::AudioBuffer<float> renderOffline (const float* mono, int numSamples,
                                            double sampleRate) const;

    // ── Persistence (message thread) ─────────────────────────────────────────
    juce::ValueTree stateToValueTree() const;
    void stateFromValueTree (const juce::ValueTree& v);

private:
    // Immutable audio-thread view.  Published via atomic swap; retired
    // snapshots retained in mRetired (audio holds the pointer only within a
    // block -- see the class comment).  Carries ALL regions -- the Focus /
    // Mod knobs need every region's midi/f0 even when unedited; the
    // per-block fast path gates on (anyEdits || focus > 0 || mod != 1).
    struct Snapshot
    {
        std::vector<PitchNoteRegion> regions;
        double      sampleRate    { 44100.0 };
        juce::int64 startSample   { 0 };
        bool        anyEdits      { false };
        bool        anyFormant    { false };
    };

    // Streaming state shared by the realtime members and the offline
    // render's locals.
    struct ApplicatorState
    {
        float  smoothedSemis   { 0.0f };
        float  smoothedFormant { 0.0f };
        // Volume-shape gain gets its own fast (~5 ms) smoother: the drawn
        // envelope must track (Speed-rate smoothing would smear a fade), but
        // region-boundary and chain-toggle steps still need de-clicking.
        float  smoothedGain    { 1.0f };
        double vibPhase        { 0.0 };
        int    cursor          { 0 };
        int    lastRegion      { -1 };
    };

    void applyEditsToBuffer (float* const* chans, int numCh, int numSamples,
                             juce::int64 timelineStartSample, double sr,
                             const Snapshot& snap,
                             float focus01, float modAmt, float speedMs,
                             bool chainOn,
                             PsolaShifter* shifters,
                             CepstralFormantEngine* formants,
                             ApplicatorState& st) const noexcept;

    // Message-thread state
    std::vector<PitchNoteRegion> mRegions;
    std::vector<float> mF0Track;
    bool        mAnalyzed     { false };
    double      mCompositeSec { 0.0 };
    double      mStartBeat    { 0.0 };
    juce::int64 mStartSample  { 0 };
    double      mAnalysisSr   { 44100.0 };

    // Audio-thread snapshot machinery
    std::atomic<Snapshot*> mActive { nullptr };
    std::vector<std::unique_ptr<Snapshot>> mRetired;   // message thread only

    // Knob targets (audio thread reads; message thread writes via setters).
    // Focus DEFAULTS 0 -- an analyzed-but-untouched channel must play
    // bit-identical (the fast path stays engaged) until the user asks for
    // correction.
    std::atomic<float> mFocus01  { 0.0f };
    std::atomic<float> mModAmt   { 1.0f };
    std::atomic<float> mSpeedMs  { 60.0f };
    std::atomic<bool>  mChainOn  { true };

    // Audio-thread applicator state (owned by the audio thread)
    std::array<PsolaShifter, 2>          mShifters;
    std::array<CepstralFormantEngine, 2> mFormant;
    ApplicatorState mAppState;
    double mSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPitchDSP)
};
