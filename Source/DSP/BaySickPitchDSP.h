#pragma once
#include <JuceHeader.h>
#include "PitchShifters.h"
#include "LibraryPitchShifters.h"   // QA-Fe: PitchEngine + IPitchShifter bake seam
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// BaySickPitchDSP - QA-Fa Task 1 (2026-07-10)
// ─────────────────────────────────────────────────────────────────────────────
// Composite-driven note-level pitch editor engine (the BaySickAlign sibling,
// section 14 of phantom-recording-mongoose).  Channel-level: the editor feeds
// it the channel composite (BaySickDAWProcessor::renderChannelComposite);
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

// QA-Fd time-edit engine: the published channel time map reuses the align
// snapshot type (same monotone-cubic anchor math; guideSec = EDITED time,
// dubSec = SOURCE time, pitchSemis unused).
struct AlignPlaySnapshot;

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
    // QA-Fe (NewTone model): slice flag -- a piece excluded from pitch
    // correction.  Analysis no longer sets it (consonants/breaths fold into the
    // neighboring note); it is set only by the Slice tool, which chops a
    // consonant off its vowel so the vowel tunes without shifting the noise.  No
    // pitch identity: the applicator skips pitch/focus/vibrato and applies the
    // volume shape only; the editor draws it inline.  The "sl" save key is
    // unchanged, so old auto-slices still load (migrated on restore).
    bool   isSlice        { false };

    // ── QA-Fd time edits (locked 5/13a: pitch tab is UPSTREAM of align) ─────
    // Destination span on the EDITED performance timeline.  dstStartSec < 0
    // = no time edit (the pill plays at its source position) -- the value
    // pre-QA-Fd saves implicitly carry.  Elastic moves keep the channel map
    // monotone (gap counter-warp, pills never cross); `detached` marks a
    // Ctrl-detached pill whose boundaries may jump (rendered as steep ramp
    // segments in the published map; the decode layer cuts, not glides).
    double dstStartSec    { -1.0 };
    double dstEndSec      { -1.0 };
    bool   detached       { false };

    double dstStart() const noexcept { return dstStartSec >= 0.0 ? dstStartSec : startSec; }
    double dstEnd()   const noexcept { return dstStartSec >= 0.0 ? dstEndSec   : endSec;   }
    bool hasTimeEdit() const noexcept
    {
        return dstStartSec >= 0.0
            && (std::abs (dstStartSec - startSec) > 1.0e-6
                || std::abs (dstEndSec - endSec) > 1.0e-6);
    }

    // ── User edits (QA-Fd sub-edit system + Edit-mode drags) ─────────────────
    float  shiftSemis     { 0.0f };   // vertical pill drag
    float  formantSemis   { 0.0f };   // formant amount (popup knob)
    float  vibDepthMult   { 1.0f };   // vibrato depth (popup knob; 0 = flatten)
    float  vibRateMult    { 1.0f };   // vibrato rate
    // QA-Fd 15a: scales the note's own pitch wiggle around its center
    // (deviation sourced from the analysis F0 track): 1 = natural, 0 =
    // flattened, 2 = exaggerated.
    float  variation      { 1.0f };
    // Volume sub-curve: (t01, gain) points sorted by t01; empty = unity.
    // Single-storage contract (locked 19a): the popup lanes AND the on-pill
    // corner handles read/write THESE SAME points.
    std::vector<juce::Point<float>> volShape;
    // QA-Fd (Jeff's sub-edit design): additive pitch curve over the pill,
    // (t01, semis) points; empty = flat.  Rendered by the popup Pitch lane +
    // the bottom corner handles (approach/release ramps).
    std::vector<juce::Point<float>> pitchShape;

    // #9 (QA-Fd): shapes count as edits only when they DO something -- a
    // handle dragged back to zero leaves neutral points behind, and those
    // must not latch the [edited] tag or keep anyEdits disengaging the
    // audio fast path.
    static bool shapeIsMeaningful (const std::vector<juce::Point<float>>& pts,
                                   float neutral) noexcept
    {
        for (const auto& p : pts)
            if (std::abs (p.y - neutral) > 0.01f)
                return true;
        return false;
    }

    bool hasEdits() const noexcept
    {
        return std::abs (shiftSemis) > 0.01f || std::abs (formantSemis) > 0.01f
            || std::abs (vibDepthMult - 1.0f) > 0.01f
            || std::abs (vibRateMult - 1.0f) > 0.01f
            || std::abs (variation - 1.0f) > 0.01f
            || shapeIsMeaningful (volShape, 1.0f)
            || shapeIsMeaningful (pitchShape, 0.0f);
    }

    static float shapeValueAt (const std::vector<juce::Point<float>>& pts,
                               float t01, float fallback) noexcept
    {
        if (pts.empty()) return fallback;
        if (t01 <= pts.front().x) return pts.front().y;
        if (t01 >= pts.back().x)  return pts.back().y;
        for (size_t i = 0; i + 1 < pts.size(); ++i)
            if (t01 >= pts[i].x && t01 < pts[i + 1].x)
            {
                const float d = pts[i + 1].x - pts[i].x;
                const float f = (d > 1.0e-6f) ? (t01 - pts[i].x) / d : 0.0f;
                return pts[i].y + f * (pts[i + 1].y - pts[i].y);
            }
        return pts.back().y;
    }

    float volGainAt     (float t01) const noexcept { return shapeValueAt (volShape,   t01, 1.0f); }
    float pitchSemisAt  (float t01) const noexcept { return shapeValueAt (pitchShape, t01, 0.0f); }

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
        if (std::abs (variation - 1.0f) > 1.0e-4f)
            v.setProperty ("va", variation, nullptr);
        if (isSlice)
            v.setProperty ("sl", 1, nullptr);
        if (dstStartSec >= 0.0)
        {
            v.setProperty ("ds", dstStartSec, nullptr);
            v.setProperty ("de", dstEndSec,   nullptr);
        }
        if (detached)
            v.setProperty ("dt", 1, nullptr);
        auto packShape = [] (const std::vector<juce::Point<float>>& shape)
        {
            juce::String s;
            for (const auto& p : shape)
                s << p.x << "," << p.y << ";";
            return s;
        };
        v.setProperty ("vol", packShape (volShape), nullptr);
        if (! pitchShape.empty())
            v.setProperty ("pit", packShape (pitchShape), nullptr);
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
        r.variation     = (float)(double) v.getProperty ("va", 1.0);
        r.isSlice       = ((int) v.getProperty ("sl", 0)) != 0;
        r.dstStartSec   = (double) v.getProperty ("ds", -1.0);
        r.dstEndSec     = (double) v.getProperty ("de", -1.0);
        r.detached      = ((int) v.getProperty ("dt", 0)) != 0;
        auto unpackShape = [&v] (const char* prop,
                                 std::vector<juce::Point<float>>& shape)
        {
            juce::StringArray pts;
            pts.addTokens (v.getProperty (prop, juce::String()).toString(), ";", "");
            for (const auto& tok : pts)
            {
                if (! tok.containsChar (',')) continue;
                shape.push_back ({ tok.upToFirstOccurrenceOf (",", false, false).getFloatValue(),
                                   tok.fromFirstOccurrenceOf (",", false, false).getFloatValue() });
            }
        };
        unpackShape ("vol", r.volShape);
        unpackShape ("pit", r.pitchShape);
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
    // Hop 512 (2026-07-12): a 4x-finer F0 track so the shifter's live grain
    // period follows vibrato/drift (Angle-1 fix).  Segmentation's frame-count
    // gates (kGapFrames/kSplitHoldFrames) were scaled x4 to keep the SAME
    // sample thresholds -- note detection is unchanged.  YIN window stays 2048
    // (kYinWin), so windows now overlap 75%.
    static constexpr int kF0Hop = 512;
    const std::vector<float>& f0Track() const noexcept { return mF0Track; }

    // Message-thread edit surface (the editor mutates, then publishEdits()).
    std::vector<PitchNoteRegion>&       regions()       noexcept { return mRegions; }
    const std::vector<PitchNoteRegion>& regions() const noexcept { return mRegions; }

    // QA-Fe: fraction of the [startSec,endSec) composite span that is voiced
    // (F0 detected).  The Slice tool uses it to auto-mark the more-unvoiced
    // half of a cut (the consonant) as excluded from pitch correction.
    double voicedFraction (double startSec, double endSec) const noexcept
    {
        if (mF0Track.empty() || endSec <= startSec || mAnalysisSr <= 0.0) return 0.0;
        const double hopSec = (double) kF0Hop / mAnalysisSr;
        const int n = (int) mF0Track.size();
        const int a = juce::jlimit (0, n, (int) std::floor (startSec / hopSec));
        const int z = juce::jlimit (0, n, (int) std::ceil  (endSec   / hopSec));
        if (z <= a) return 0.0;
        int voiced = 0;
        for (int f = a; f < z; ++f)
            if (mF0Track[(size_t) f] > 0.0f) ++voiced;
        return (double) voiced / (double) (z - a);
    }

    // Rebuild + atomically publish the audio-thread snapshot.  Call after
    // every edit gesture completes (mouse-up), every analyze, and on load.
    // QA-Fd: also rebuilds + publishes the channel TIME MAP (nullptr when no
    // region has a time edit -- the decode fast path stays untouched).
    void publishEdits();

    void clearAllEdits();   // Reset button: keeps regions, zeroes edits

    // ── QA-Fd time-edit engine (locked 5/13a) ────────────────────────────────
    // AUDIO/MT: the published EDITED->SOURCE map (composite-relative seconds
    // both sides; AlignPlaySnapshot anchor/Hermite reuse).  nullptr = no
    // time edits.  Same retire-ring liveness contract as the edit snapshot.
    const AlignPlaySnapshot* loadTimeMapSnapshot() const noexcept
        { return mTimeMapActive.load (std::memory_order_acquire); }

    // AUDIO/MT: true when a non-empty pitched cache is published.  The decode
    // layer reads this so a baked channel stamps the ALIGN-ONLY edited read
    // position instead of the composed source position (the pitch time-warp now
    // lives in the edited-timeline cache).  Immutable published snapshot;
    // retire-ring-safe.
    bool hasBakedCache() const noexcept
    { auto* c = mCacheActive.load (std::memory_order_acquire); return c != nullptr && ! c->audio.empty(); }

    // MESSAGE THREAD: hash over the regions' time-edit state (0 = none).
    // The align staleness signature folds this in so a time edit re-arms
    // the stop-gated align re-analysis (align consumes the EDITED timing).
    juce::int64 timeMapHash() const;

    // MESSAGE THREAD: source-composite seconds -> edited-performance seconds
    // (the inverse view the align analysis needs to transform raw onsets).
    double srcToEditedSec (double srcSec) const;

    // ── Global knobs (CPU-guarded; pushed per block from bsp_ params) ────────
    void setFocus01   (float v);   // 0..1: pull note centers toward the snap target
    void setModAmount (float v);   // 0..2: vibrato preservation scale (1 = natural)
    void setSpeedMs   (float ms);  // 5..300: note-transition glide
    // QA-Fe Task 6: global throat/character -- semitones of formant shift added
    // on top of each region's own formant before the bake's 2^(x/12) formant
    // scale.  0 = neutral; + raises formants (smaller/brighter throat), - lowers
    // them (bigger/darker).  Maps to the engine's native formant control via the
    // bake seam; a change re-bakes.
    void setThroat    (float semis);
    // QA-Fa recovery: bsp_on chain switch.  OFF glides every target to
    // neutral through the Speed smoothing (no hard switch), then the fast
    // path disengages once settled.
    void setChainOn   (bool on);
    // QA-Fd 14b: Snap OFF -> Focus pulls to the nearest semitone (legacy);
    // Snap ON -> the target set is the Root/Scale pick (shared 13-scale
    // table).  Pushed per block like the knobs above.
    void setRoot      (int pc);        // 0..11
    void setScaleIdx  (int idx);       // 0..12 (piano-roll order)
    void setSnapOn    (bool on);
    // QA-Fe: selected bake engine (Task 3 wires bsp_engine to this).  Read on
    // the bake/render (message/worker) thread; an engine change re-bakes.
    void setEngine    (PitchEngine e)
    {
        if ((int) e != mEngine.load (std::memory_order_relaxed))
        {
            mEngine.store ((int) e, std::memory_order_relaxed);
            requestBake();
        }
    }

    // ── Realtime applicator (AUDIO THREAD, FilePlay only -- Mode C) ──────────
    // timelineStartSample = the block's first timeline sample (stamped by
    // finalizeFilePlayStrip next to setForcePitchBypass).  Fast-path: a
    // single atomic load bails when no edits exist (zero-edit channels cost
    // one load per block).
    //
    // QA-Fd source-domain stamps (closes the wrong-syllable hole): when the
    // decode layer warps this channel (align and/or time map), srcValid=true
    // and srcX0/srcRatePerSample describe the SOURCE-composite position the
    // audible audio actually came from (timeline-equivalent samples at the
    // device rate) -- the applicator resolves the active pill there instead
    // of at the linear timeline position.
    void processFilePlay (juce::AudioBuffer<float>& buffer,
                          juce::int64 timelineStartSample,
                          double srcX0 = 0.0, double srcRatePerSample = 1.0,
                          bool srcValid = false) noexcept;

    // QA-Fb (A1 monitor merge): same applicator, SECOND stream state.  While
    // a live take runs over prior takes, the realtime corrector occupies the
    // main pitch stage, so the takes' monitor path applies the channel's note
    // edits through this parallel state set (shared snapshot, own PSOLA /
    // formant / glide cursors -- the two streams advance independently).
    void processFilePlayMonitor (juce::AudioBuffer<float>& buffer,
                                 juce::int64 timelineStartSample,
                                 double srcX0 = 0.0, double srcRatePerSample = 1.0,
                                 bool srcValid = false) noexcept;

    // ── Offline render (message thread; the Render/Freeze bake) ─────────────
    // Applies focus/vibrato/edits to the composite exactly like the realtime
    // path, but offline over the whole buffer.  Returns the processed mono.
    // QA-Fd: spanStart/spanLen render just a composite portion (the shared
    // preview-play / scrub-audition path); defaults = the whole composite.
    juce::AudioBuffer<float> renderOffline (const float* mono, int numSamples,
                                            double sampleRate,
                                            juce::int64 spanStart = 0,
                                            int spanLen = -1) const;

    // QA-Fe (threading fix): message-thread snapshot of the background-baked cache
    // over an EDITED-timeline span [startEditedSec, endEditedSec).  Lock-free load
    // of the immutable published snapshot + a plain span copy (no engine bake) --
    // the scrub preview reads THIS instead of running the heavy bake (WORLD froze
    // the UI on every drag scrub).  Empty when no cache is published yet.
    juce::AudioBuffer<float> copyCacheSpan (double startEditedSec, double endEditedSec) const;

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
        // QA-Fd 15a: the analysis F0 track rides the snapshot so the
        // Variation knob can scale the note's own contour deviation
        // per-sample (composite-relative, hop = kF0Hop at sampleRate).
        std::vector<float> f0Track;
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


    // QA-Fe: resolve the per-sample pitch ratio / formant-semitones / gain the
    // edits produce (outRatio/outFormant/outGain, each length numSamples) for the
    // library-engine bake -- no audio touched.  Snap + knob state is passed in
    // (immutable) so the background bake thread can run it off a captured
    // BakeInput without racing the message thread's live members.
    void computeEnvelopes (int numSamples, juce::int64 timelineStartSample, double sr,
                           const Snapshot& snap,
                           float focus01, float modAmt, float speedMs, bool chainOn,
                           bool snapScale, int rootPc, int scaleIdx,
                           ApplicatorState& st,
                           float* outRatio, float* outFormant, float* outGain,
                           double srcX0 = 0.0, double srcRatePerSample = 1.0,
                           bool srcValid = false) const noexcept;

    // Shared bake core (message + worker thread): resolve envelopes over
    // [spanStart, spanStart+spanLen), bake through `engine` (length-preserved,
    // formant-preserving), apply the gain.
    juce::AudioBuffer<float> bakeSpan (const float* mono, int numSamples, double sr,
                                       const Snapshot& snap, PitchEngine engine,
                                       float focus, float mod, float speed, float throat,
                                       bool snapOn, int rootPc, int scaleIdx,
                                       juce::int64 spanStart, int spanLen,
                                       const AlignPlaySnapshot* timeMap = nullptr) const;

    // publishEdits helper: rebuild + swap the time-map snapshot.
    void publishTimeMap();
    // #1/#2 bake path: build the EDITED->SOURCE map (guide=edited, dub=source)
    // from the regions' time edits; nullptr when no region has one.
    std::unique_ptr<AlignPlaySnapshot> buildTimeMapSnapshot() const;

    // Message-thread state
    std::vector<PitchNoteRegion> mRegions;
    std::vector<float> mF0Track;
    bool        mAnalyzed     { false };
    double      mCompositeSec { 0.0 };
    double      mStartBeat    { 0.0 };
    juce::int64 mStartSample  { 0 };
    double      mAnalysisSr   { 44100.0 };
    // QA-Fe: the analyzed composite (mono, analysis rate).  shared_ptr so the
    // background bake thread reads it via a cheap refcount copy in the BakeInput
    // without racing a re-analyze on the message thread.
    std::shared_ptr<const std::vector<float>> mComposite;

    // QA-Fe: the background-baked pitched composite the audio thread reads.
    // Published lock-free (atomic ptr + retire ring, same liveness contract as
    // the time map); the audio thread holds the pointer only within one block.
    struct CacheSnapshot
    {
        std::vector<float> audio;          // pitched composite, mono, analysis rate
        double             sampleRate  { 44100.0 };
        juce::int64        startSample { 0 };
    };
    std::atomic<CacheSnapshot*> mCacheActive { nullptr };
    std::vector<std::unique_ptr<CacheSnapshot>> mCacheRetired;   // worker thread only

    // Cache-handoff crossfade (AUDIO THREAD state only, RT-safe/lock-free): a fresh
    // bake is a new pointer, so the read loop crossfades old->new over ~30 ms
    // instead of hard-swapping (which steps the waveform = a click after an edit
    // during playback).  The retiring cache stays alive through the short fade
    // (retire ring is 8 deep vs one 30 ms fade).
    CacheSnapshot* mCachePlaying    { nullptr };   // last cache the read loop adopted
    CacheSnapshot* mCacheFading     { nullptr };   // previous cache, fading out
    int            mCacheFadeRemain { 0 };
    int            mCacheFadeLen    { 0 };
    // Hazard pointers: the two caches the audio thread holds as raw pointers
    // ACROSS blocks -- the last-adopted cache (mCachePlaying: pointer-compared at
    // the swap check, dereferenced there, and latched as the fade source) and the
    // fading-from cache (mCacheFading).  The worker's retire-ring erase skips BOTH,
    // so neither can be freed out from under processFilePlay -- including when a
    // transport pause freezes the audio thread while the user keeps editing (many
    // bakes).  The plain 8-deep ring only covers a within-block hold.  Audio thread
    // writes (release); worker reads (acquire).
    std::atomic<CacheSnapshot*> mCacheHazardPlaying { nullptr };
    std::atomic<CacheSnapshot*> mCacheHazardFading  { nullptr };

    // QA-Fd: published time map (edited -> source), same liveness contract.
    std::atomic<AlignPlaySnapshot*> mTimeMapActive { nullptr };
    std::vector<std::unique_ptr<AlignPlaySnapshot>> mTimeMapRetired;

    // Knob targets (audio thread reads; message thread writes via setters).
    // Focus DEFAULTS 0 -- an analyzed-but-untouched channel must play
    // bit-identical (the fast path stays engaged) until the user asks for
    // correction.
    std::atomic<float> mFocus01  { 0.0f };
    std::atomic<float> mModAmt   { 1.0f };
    std::atomic<float> mSpeedMs  { 60.0f };
    std::atomic<float> mThroat   { 0.0f };   // QA-Fe Task 6: global formant/throat (semis)
    std::atomic<bool>  mChainOn  { true };
    std::atomic<int>   mRootPc   { 0 };
    std::atomic<int>   mScaleIdx { 0 };
    std::atomic<bool>  mSnapOn   { false };
    std::atomic<int>   mEngine   { (int) PitchEngine::RubberBand };  // QA-Fe (B1 default)

    double mSampleRate { 44100.0 };

    // QA-Fe: background bake worker.  requestBake() (message thread) captures a
    // BakeInput under mBakeMutex + wakes the worker; the worker bakes it into a
    // fresh CacheSnapshot off-thread and publishes it -- so a WORLD bake never
    // freezes the UI, and RB/Signalsmith finish fast enough to feel live.
    struct BakeInput
    {
        Snapshot    snap;                 // startSample = 0 (composite-relative bake)
        std::shared_ptr<const std::vector<float>> composite;
        // #1/#2: EDITED->SOURCE map (guide=edited, dub=source); nullptr = no
        // time edit -> the bake warps onto the edited timeline so playback reads
        // it directly (no varispeed).
        std::shared_ptr<const AlignPlaySnapshot> timeMap;
        juce::int64 timelineStart { 0 };  // composite's timeline origin (for the cache read)
        int         engine   { (int) PitchEngine::RubberBand };
        float       focus    { 0.0f };
        float       mod      { 1.0f };
        float       speed    { 60.0f };
        float       throat   { 0.0f };    // QA-Fe Task 6
        bool        snapOn   { false };
        int         rootPc   { 0 };
        int         scaleIdx { 0 };
    };
    struct BakeWorker : juce::Thread
    {
        explicit BakeWorker (BaySickPitchDSP& o) : juce::Thread ("BsPitchBake"), owner (o) {}
        void run() override
        {
            while (! threadShouldExit())
            {
                wait (-1);   // sleep until requestBake() notifies (or shutdown)
                while (owner.mBakeDirty.exchange (false, std::memory_order_acquire))
                {
                    if (threadShouldExit()) return;
                    BakeInput in;
                    {
                        std::lock_guard<std::mutex> lk (owner.mBakeMutex);
                        in = owner.mBakeInput;   // cheap: shared_ptr composite + small vectors
                    }
                    owner.bakeToCache (in);
                }
            }
        }
        BaySickPitchDSP& owner;
    };
    std::unique_ptr<BakeWorker> mBakeWorker;
    std::mutex        mBakeMutex;
    BakeInput         mBakeInput;                 // guarded by mBakeMutex
    std::atomic<bool> mBakeDirty { false };

    void requestBake();     // message thread: snapshot inputs + wake the worker
    void bakeToCache (const BakeInput& in);       // worker: bake whole composite -> cache

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BaySickPitchDSP)
};
