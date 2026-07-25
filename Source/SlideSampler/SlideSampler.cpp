#include "SlideSampler.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    // SS-Q5: nudge a hop voice's start offset to the nearest upward zero-crossing
    // so the crossfade-in begins on a clean sample boundary (reduces zone-hop
    // clicks/comb).  Bounded search (~3 ms), audio-thread safe.
    int snapZeroCrossing (const float* src, int numFrames, int wantIdx) noexcept
    {
        if (src == nullptr || numFrames <= 2) return wantIdx;
        wantIdx = juce::jlimit (0, numFrames - 2, wantIdx);
        const int span = juce::jmin (numFrames - 2 - wantIdx, 132);   // ~3 ms @ 44.1k
        for (int d = 0; d < span; ++d)
        {
            const int i = wantIdx + d;
            if (src[i] <= 0.0f && src[i + 1] > 0.0f) return i + 1;
        }
        return wantIdx;
    }

    // Parse "lfoNN" prefix -> NN (1-based), or -1.
    int lfoIndexOf (const juce::String& k) noexcept
    {
        if (! k.startsWith ("lfo") || k.length() < 5) return -1;
        const int n = k.substring (3, 5).getIntValue();
        return n >= 1 ? n : -1;
    }

    // Parse a trailing "..onccK" / "..ccK" cc number, or -1.
    int trailingCc (const juce::String& k) noexcept
    {
        int p = k.lastIndexOf ("oncc");
        int skip = 4;
        if (p < 0) { p = k.lastIndexOf ("cc"); skip = 2; }
        if (p < 0) return -1;
        const int cc = k.substring (p + skip).getIntValue();
        return (cc >= 0 && cc <= 139) ? cc : -1;   // 128+ = sfizz extended inputs
    }
}

void SlideSampler::prepare (double sampleRate, int maxBlockSize)
{
    mSampleRate = sampleRate > 1000.0 ? sampleRate : 48000.0;
    mBlockSize  = juce::jmax (16, maxBlockSize);
    mScratch.assign ((size_t) mBlockSize + 8, 0.0f);

    juce::dsp::ProcessSpec spec { mSampleRate, (juce::uint32) mBlockSize, 1 };
    for (auto& v : mVoices)
    {
        v.lpf.prepare (spec);
        v.lpf.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        v.lpf.setCutoffFrequency (20000.0f);
        v.lpf.setResonance (0.7071f);   // Butterworth = transparent default
    }
}

float SlideSampler::ccValue (int cc) const noexcept
{
    // sfizz-internal pseudo-CC inputs (close review + Jeff): no panel writer
    // exists for these, so synthesize them from the gesture -- otherwise the
    // kits' velocity-scaled depths + random LFO phases read 0 on slides.
    switch (cc)
    {
        case 131: return (float) mSlideVelocity;    // note velocity
        case 133: return mGestureNoteCc;            // note number (gesture anchor)
        case 135: return mGestureRand1;             // unipolar random, per gesture
        case 136: return mGestureRand2;
        default:  break;
    }
    // Block-rate read through the Task-12 provider; 0 with none (sfizz's
    // no-CC-input behavior).  Provider must be lock-free (APVTS atomics).
    return (mCcProvider != nullptr && cc >= 0) ? juce::jlimit (0.0f, 127.0f, mCcProvider (cc))
                                               : 0.0f;
}

float SlideSampler::ccNorm (const LfoDef::CcDepth& d) const noexcept
{
    float x = ccValue (d.cc) / 127.0f;
    if (d.curve < 0) return x;
    for (const auto& c : mCurves)
    {
        if (c.index != d.curve) continue;
        if (c.points.empty()) break;
        if (x <= c.points.front().first) return c.points.front().second;
        if (x >= c.points.back().first)  return c.points.back().second;
        for (size_t i = 1; i < c.points.size(); ++i)
            if (x <= c.points[i].first)
            {
                const auto& p0 = c.points[i - 1];
                const auto& p1 = c.points[i];
                return (p1.first > p0.first)
                         ? p0.second + (p1.second - p0.second) * (x - p0.first) / (p1.first - p0.first)
                         : p1.second;
            }
        break;
    }
    return x;
}

float SlideSampler::velGainFor (const Zone& z, int velocity) const noexcept
{
    const float v = (float) juce::jlimit (1, 127, velocity);
    float curve;
    if (! z.velcurve.empty())
    {
        // Piecewise-linear through the captured amp_velcurve points; implicit
        // endpoints (1,~0) and (127,1) per SFZ semantics.
        float x0 = 1.0f, y0 = 0.0f, x1 = 127.0f, y1 = 1.0f;
        for (const auto& p : z.velcurve)
        {
            if ((float) p.first <= v && (float) p.first >= x0) { x0 = (float) p.first; y0 = p.second; }
            if ((float) p.first >= v && (float) p.first <  x1) { x1 = (float) p.first; y1 = p.second; }
        }
        curve = (x1 > x0) ? y0 + (y1 - y0) * (v - x0) / (x1 - x0) : y1;
    }
    else
    {
        curve = (v / 127.0f) * (v / 127.0f);   // SFZ default power curve
    }
    // amp_veltrack scales how much the curve applies (100 = fully).
    const float t = juce::jlimit (0.0f, 100.0f, z.ampVeltrack) / 100.0f;
    return juce::jlimit (0.0f, 1.5f, (1.0f - t) + t * curve);
}

void SlideSampler::parseZoneMods (Zone& z, const SlideSample& s)
{
    auto lfoFor = [&z] (int idx) -> LfoDef&
    {
        for (auto& l : z.lfos) if (l.index == idx) return l;
        LfoDef d; d.index = idx;
        z.lfos.push_back (d);
        return z.lfos.back();
    };

    // Task 12b: `_curvecc` opcodes name a <curve> table for their sibling
    // `_oncc` route; routes may parse in any order, so collect and attach at
    // the end.
    struct CurveTag { juce::String base; int cc; int curve; };
    std::vector<CurveTag> curveTags;

    for (const auto& kv : s.modOps)
    {
        const auto& k = kv.first;
        const float val = kv.second.getFloatValue();

        if (k == "cutoff")            { z.hasLpf = true; z.cutoffHz = juce::jmax (20.0f, val); continue; }
        if (k == "fil_keytrack")      { z.filKeytrack = val; continue; }
        // Task 12b: static filter CC routes (bass cutoff_cc92 / resonance_cc91).
        if (k.startsWith ("cutoff_cc") || k.startsWith ("cutoff_oncc"))
        { z.cutoffCc = { trailingCc (k), val, -1 }; continue; }
        if (k.startsWith ("resonance_cc") || k.startsWith ("resonance_oncc"))
        { z.resoCc = { trailingCc (k), val, -1 }; continue; }
        // Task 12b: ANY amplitude_oncc<K> (unison cc100, feedback/noise cc29).
        if (k.startsWith ("amplitude_oncc")) { z.gainCc = { trailingCc (k), val, -1 }; continue; }
        if (k == "pan_oncc101")       { z.uniPan      = { 101, val, -1 }; continue; }
        if (k == "tune_cc102" || k == "tune_oncc102") { z.uniTuneCents = { 102, val, -1 }; continue; }

        if (k.contains ("_curvecc"))
        {
            curveTags.push_back ({ k.upToFirstOccurrenceOf ("_curvecc", false, false),
                                   trailingCc (k), (int) val });
            continue;
        }

        // Task 12b: ampeg `_oncc` mods (guitar cc70; bass cc24 swell / cc107 release).
        if (k.startsWith ("ampeg_") && k.contains ("_oncc"))
        {
            const LfoDef::CcDepth d { trailingCc (k), val, -1 };
            if      (k.startsWith ("ampeg_attack_"))  z.envAttackCc  = d;
            else if (k.startsWith ("ampeg_hold_"))    z.envHoldCc    = d;
            else if (k.startsWith ("ampeg_decay_"))   z.envDecayCc   = d;
            else if (k.startsWith ("ampeg_sustain_")) z.envSustainCc = d;
            else if (k.startsWith ("ampeg_release_")) z.envReleaseCc = d;
            continue;
        }

        // Task 12b: pitcheg (linear A/D toward sustain; depth in cents).
        if (k.startsWith ("pitcheg_"))
        {
            if      (k.startsWith ("pitcheg_depth_oncc")) { z.pegDepthCc = { trailingCc (k), val, -1 }; z.hasPeg = true; }
            else if (k == "pitcheg_attack")  { z.pegAttack     = juce::jmax (0.0f, val); z.hasPeg = true; }
            else if (k == "pitcheg_decay")   { z.pegDecay      = juce::jmax (0.0f, val); z.hasPeg = true; }
            else if (k == "pitcheg_sustain") { z.pegSustainPct = val; z.hasPeg = true; }
            else if (k == "pitcheg_depth")   { z.pegDepthCents = val; z.hasPeg = true; }
            continue;
        }

        // Task 12c: fileg (bass filter EG, cents onto cutoff -- pitcheg shape).
        if (k.startsWith ("fileg_"))
        {
            if      (k.startsWith ("fileg_depth_oncc")) { z.filegDepthCc = { trailingCc (k), val, -1 }; z.hasFileg = true; }
            else if (k == "fileg_attack")  { z.filegAttack     = juce::jmax (0.0f, val); z.hasFileg = true; }
            else if (k == "fileg_decay")   { z.filegDecay      = juce::jmax (0.0f, val); z.hasFileg = true; }
            else if (k == "fileg_sustain") { z.filegSustainPct = val; z.hasFileg = true; }
            else if (k == "fileg_depth")   { z.filegDepthCents = val; z.hasFileg = true; }
            continue;
        }

        // Task 12c: gain_cc alias (volume dB rides the CC).
        if (k.startsWith ("gain_cc") || k.startsWith ("gain_oncc"))
        { z.volDbCc = { trailingCc (k), val, -1 }; continue; }

        // Task 12c: ARIA varNN mult kludge (bass filter).
        if (k.startsWith ("var0"))
        {
            const int vi = k.substring (3, 5).getIntValue();
            Zone::VarDef* vr = nullptr;
            for (auto& x : z.vars) if (x.index == vi) vr = &x;
            if (vr == nullptr)
            {
                Zone::VarDef d; d.index = vi;
                z.vars.push_back (d);
                vr = &z.vars.back();
            }
            if      (k.endsWith ("_mod"))    vr->mult = kv.second.trim().equalsIgnoreCase ("mult");
            else if (k.contains ("_oncc"))   { if (vr->numInputs < 4) vr->inputs[(size_t) vr->numInputs++] = { trailingCc (k), val, -1 }; }
            else if (k.endsWith ("_cutoff")) vr->cutoffCents = val;
            continue;
        }

        const int li = lfoIndexOf (k);
        if (li < 0) continue;
        auto& l = lfoFor (li);
        const juce::String rest = k.substring (5);   // after "lfoNN"

        if      (rest == "_freq")  l.freqHz   = val;
        else if (rest == "_delay") l.delaySec = val;
        else if (rest == "_fade")  l.fadeSec  = val;
        else if (rest == "_wave")  l.wave     = (int) val;
        else if (rest.startsWith ("_pitch_oncc"))  l.pitchCents  = { trailingCc (k), val, -1 };
        else if (rest.startsWith ("_volume_oncc")) l.volumeDb    = { trailingCc (k), val, -1 };
        else if (rest.startsWith ("_cutoff_oncc")) l.cutoffCents = { trailingCc (k), val, -1 };
        else if (rest.startsWith ("_delay_oncc"))  l.delayCc     = { trailingCc (k), val, -1 };
        else if (rest.startsWith ("_fade_oncc"))   l.fadeCc      = { trailingCc (k), val, -1 };
        else if (rest.startsWith ("_freq_lfo"))
        {
            // Cross-LFO rate mod: lfoNN_freq_lfoMM_onccK -- THIS lfo's rate is
            // modulated by lfo MM, depth cc-scaled (the bass wobble trick).
            l.freqModTarget = rest.substring (9, 11).getIntValue();
            l.freqModHz     = { trailingCc (k), val, -1 };
        }
    }

    // Task 12b end-pass: attach <curve> indices to their matching routes
    // (a route matches by base opcode + cc number).
    for (const auto& t : curveTags)
    {
        auto tag = [&t] (LfoDef::CcDepth& d) { if (d.cc == t.cc) d.curve = t.curve; };
        if      (t.base == "amplitude")          tag (z.gainCc);
        else if (t.base == "ampeg_attack")       tag (z.envAttackCc);
        else if (t.base == "ampeg_hold")         tag (z.envHoldCc);
        else if (t.base == "ampeg_decay")        tag (z.envDecayCc);
        else if (t.base == "ampeg_sustain")      tag (z.envSustainCc);
        else if (t.base == "ampeg_release")      tag (z.envReleaseCc);
        else if (t.base == "pitcheg_depth")      tag (z.pegDepthCc);
        else if (t.base == "fileg_depth")        tag (z.filegDepthCc);
        else if (t.base == "gain")               tag (z.volDbCc);
        else if (t.base == "cutoff")             tag (z.cutoffCc);
        else if (t.base == "resonance")          tag (z.resoCc);
        else if (t.base == "pan")                tag (z.uniPan);
        else if (t.base.startsWith ("tune"))     tag (z.uniTuneCents);
        else if (t.base.startsWith ("var0"))
        {
            const int vi = t.base.substring (3, 5).getIntValue();
            for (auto& vr : z.vars)
                if (vr.index == vi)
                    for (int i = 0; i < vr.numInputs; ++i)
                        tag (vr.inputs[(size_t) i]);
        }
        else if (t.base.startsWith ("lfo"))
        {
            const int li = lfoIndexOf (t.base);
            for (auto& l : z.lfos)
                if (l.index == li)
                {
                    tag (l.pitchCents); tag (l.volumeDb); tag (l.cutoffCents);
                    tag (l.delayCc);    tag (l.fadeCc);   tag (l.freqModHz);
                }
        }
    }
}

void SlideSampler::latchVoiceEnv (Voice& v, const Zone& z) noexcept
{
    // sfizz evaluates EG cc-mods at trigger time -- attack/hold/decay/sustain
    // latch here; release re-latches at release() (bass cc107 semantics).
    auto mod = [this] (float base, const LfoDef::CcDepth& d) -> float
    { return d.cc >= 0 ? base + d.depth * ccNorm (d) : base; };
    v.envAttackSec  = juce::jmax (0.0f, mod (z.ampegAttack,  z.envAttackCc));
    v.envHoldSec    = juce::jmax (0.0f, mod (z.ampegHold,    z.envHoldCc));
    v.envDecaySec   = juce::jmax (0.0f, mod (z.ampegDecay,   z.envDecayCc));
    v.envSustainLvl = juce::jlimit (0.0f, 1.0f, mod (z.ampegSustain, z.envSustainCc) / 100.0f);
    v.envReleaseSec = juce::jmax (0.01f, mod (z.ampegRelease, z.envReleaseCc));
    v.pegTimeSec    = 0.0;
    v.filegTimeSec  = 0.0;
}

void SlideSampler::buildLayer (LayerTables& t, const std::vector<SlideSample>& set,
                               SlideSampleCache& cache, bool unisonHpf)
{
    t.bands.clear();
    for (const auto& s : set)
    {
        int bi = -1;
        for (int i = 0; i < (int) t.bands.size(); ++i)
            if (t.bands[(size_t) i].loVel == s.loVel && t.bands[(size_t) i].hiVel == s.hiVel) { bi = i; break; }
        if (bi < 0)
        {
            Band nb; nb.loVel = s.loVel; nb.hiVel = s.hiVel;
            t.bands.push_back (std::move (nb));
            bi = (int) t.bands.size() - 1;
        }
        Zone z;
        z.rootKey      = s.rootKey;
        z.wav          = s.wav;
        z.sample       = cache.getHandle (s.wav);
        z.offsetFrames = s.offsetFrames;              // Task 11: `offset` now carried
        z.loopStart    = s.loopStart;
        z.loopEnd      = s.loopEnd;
        z.looped       = (s.loopMode != 0);
        z.tuneCents    = (float) s.tuneCents;
        z.volumeDb     = s.volumeDb;
        z.amplitudePct = s.amplitudePct;
        z.ampVeltrack  = s.ampVeltrack;
        z.velcurve     = s.velcurve;
        z.ampegDelay   = s.ampegDelay;
        z.ampegAttack  = s.ampegAttack;
        z.ampegHold    = s.ampegHold;
        z.ampegDecay   = s.ampegDecay;
        z.ampegSustain = s.ampegSustain;
        z.ampegRelease = juce::jmax (0.01f, s.ampegRelease);
        z.rtDecay      = s.rtDecay;
        z.unisonHpf    = unisonHpf;
        parseZoneMods (z, s);
        cache.decodeNow (z.sample, z.wav);
        t.bands[(size_t) bi].zones.push_back (std::move (z));
    }
    std::sort (t.bands.begin(), t.bands.end(),
               [] (const Band& a, const Band& b) { return a.loVel < b.loVel; });
    for (auto& b : t.bands)
        std::sort (b.zones.begin(), b.zones.end(),
                   [] (const Zone& a, const Zone& c) { return a.rootKey < c.rootKey; });
}

void SlideSampler::setProgram (const SlideRegionMap& map)
{
    // Runs inside loadKit with the engine's processing gate OFF -> the audio
    // thread is not reading the tables, so a full rebuild is safe here.
    for (auto& v : mVoices) { v.active = false; v.killed = false; v.sample = nullptr; v.zone = nullptr; }
    mAnyActive.store (false, std::memory_order_release);
    mActiveVoice = nullptr;
    mGestureArt  = nullptr;
    mGestureTables = nullptr;
    mActiveBand = -1;
    mActiveZoneRoot = -1000;
    mArts.clear();
    mActiveArt.store (-1, std::memory_order_release);
    mCurves = map.curves;
    mResidentHandles.clear();

    if (map.articulations.empty() || map.defaultArticulation < 0) return;

    auto buildFlat = [this] (std::vector<Zone>& dst, const std::vector<SlideSample>& set)
    {
        LayerTables tmp;
        buildLayer (tmp, set, *mCache, false);
        for (auto& b : tmp.bands)
            for (auto& z : b.zones)
                dst.push_back (std::move (z));
        std::sort (dst.begin(), dst.end(),
                   [] (const Zone& a, const Zone& c) { return a.rootKey < c.rootKey; });
    };

    // Task 12: voiced tables for EVERY articulation.  Keyswitch tracking swaps
    // mActiveArt over these immutable sets; with all samples resident (G-11)
    // an articulation swap costs nothing.
    mArts.reserve (map.articulations.size());
    for (const auto& srcArt : map.articulations)
    {
        ArtSet a;
        a.swLast      = srcArt.swLast;
        a.hasMonoSet  = srcArt.hasMonoSet;
        a.monoOffTime = srcArt.monoOffTime > 0.001f ? srcArt.monoOffTime : 0.2f;
        buildLayer (a.center,    srcArt.center,    *mCache, false);
        buildLayer (a.tUp,       srcArt.tUp,       *mCache, true);
        buildLayer (a.tDown,     srcArt.tDown,     *mCache, true);
        buildLayer (a.tailpiece, srcArt.tailpiece, *mCache, false);
        buildFlat (a.releases, srcArt.releases);
        buildFlat (a.noise,    srcArt.noise);
        buildFlat (a.feedback, srcArt.feedback);
        mArts.push_back (std::move (a));
    }
    mActiveArt.store (map.defaultArticulation, std::memory_order_release);

    // G-11 (option A): decode EVERY articulation's EVERY layer set now -- zero
    // keyswitch latency ever; handles held keep-alive (path-keyed dedupe).
    {
        auto decodeSet = [this] (const std::vector<SlideSample>& set)
        {
            for (const auto& s : set)
            {
                auto h = mCache->getHandle (s.wav);
                mCache->decodeNow (h, s.wav);
                mResidentHandles.push_back (std::move (h));
            }
        };
        for (const auto& a : map.articulations)
        {
            decodeSet (a.center);    decodeSet (a.tUp);      decodeSet (a.tDown);
            decodeSet (a.tailpiece); decodeSet (a.feedback); decodeSet (a.noise);
            decodeSet (a.releases);
        }
    }

   #if JUCE_DEBUG
    {
        std::vector<const DecodedSlideSample*> seen;
        juce::int64 bytes = 0;
        auto tally = [&seen, &bytes] (const std::shared_ptr<DecodedSlideSample>& h)
        {
            if (h == nullptr) return;
            if (std::find (seen.begin(), seen.end(), h.get()) != seen.end()) return;
            seen.push_back (h.get());
            bytes += (juce::int64) h->buffer.getNumSamples() * (juce::int64) sizeof (float);
        };
        for (auto& h : mResidentHandles) tally (h);
        DBG ("[SlideSampler] G-11 residency: " << (int) seen.size()
             << " unique decoded sample(s), " << juce::String (bytes / (1024.0 * 1024.0), 1)
             << " MB float32 resident");
    }
   #endif
}

int SlideSampler::pickBand (const LayerTables& t, int velocity) const noexcept
{
    if (t.bands.empty()) return -1;
    for (int i = 0; i < (int) t.bands.size(); ++i)
        if (velocity >= t.bands[(size_t) i].loVel && velocity <= t.bands[(size_t) i].hiVel)
            return i;
    int best = 0, bestDist = std::numeric_limits<int>::max();
    for (int i = 0; i < (int) t.bands.size(); ++i)
    {
        const auto& b = t.bands[(size_t) i];
        const int d = velocity < b.loVel ? b.loVel - velocity : velocity - b.hiVel;
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

int SlideSampler::pickZone (const LayerTables& t, int bandIdx, float pitchSemis) const noexcept
{
    if (bandIdx < 0 || bandIdx >= (int) t.bands.size()) return -1;
    const auto& z = t.bands[(size_t) bandIdx].zones;
    if (z.empty()) return -1;
    if (pitchSemis < (float) z[0].rootKey) return 0;

    int lo = 0, hi = (int) z.size() - 1, res = 0;
    while (lo <= hi)
    {
        const int mid = (lo + hi) / 2;
        if ((float) z[(size_t) mid].rootKey <= pitchSemis) { res = mid; lo = mid + 1; }
        else                                                hi = mid - 1;
    }
    return res;
}

SlideSampler::Voice* SlideSampler::allocVoice() noexcept
{
    for (auto& v : mVoices) if (! v.active) return &v;
    // #6/Task 11 steal = oldest-quietest: lowest audible contribution first,
    // oldest breaks ties.  (The old lowest-fade pick stole the INCOMING
    // crossfade voice -- the single worst choice.)
    Voice* pick = &mVoices[0];
    float  pickScore = std::numeric_limits<float>::max();
    for (auto& v : mVoices)
    {
        const float audible = std::sin (v.fade * juce::MathConstants<float>::halfPi)
                              * v.envLevel * v.baseGain;
        const float score = audible - (float) v.age * 1.0e-6f;   // age tiebreak
        if (&v == mActiveVoice) continue;                        // never steal the steered voice
        if (score < pickScore) { pickScore = score; pick = &v; }
    }
    return pick;
}

void SlideSampler::updateVoiceRatio (Voice& v, float pitchSemis) noexcept
{
    if (v.sample == nullptr || v.zone == nullptr) return;
    const double srcRate = v.sample->ready.load (std::memory_order_acquire)
                           ? v.sample->sourceRate : 44100.0;
    // At-or-below pick keeps semis in [0,1) -> a <=1-semitone bend UP (SL-2).
    // Zone `tune` + (Task 12) unison detune ride on top in cents.
    double cents = ((double) pitchSemis - (double) v.rootKey) * 100.0
                 + (double) v.zone->tuneCents;
    if (v.zone->uniTuneCents.cc >= 0)
        cents += (double) v.zone->uniTuneCents.depth * (double) ccNorm (v.zone->uniTuneCents);
    // Review fix: remember the full sum so block-rate pitch modulation
    // (LFO/pitcheg) rides ON TOP of it instead of discarding the
    // root-relative bend + unison detune for non-steered voices.
    v.baseCents = cents;
    v.ratio = (srcRate / mSampleRate) * std::pow (2.0, cents / 1200.0);
}

void SlideSampler::triggerZone (const LayerTables& t, int bandIdx, int zoneIdx,
                                float pitchSemis, bool firstNote, float panSign) noexcept
{
    if (bandIdx < 0 || zoneIdx < 0) return;
    auto& zone = t.bands[(size_t) bandIdx].zones[(size_t) zoneIdx];
    if (zone.sample == nullptr) return;

    // #6: time-align the incoming hop voice to the OUTGOING voice's elapsed
    // source position, so the hop continues the note's life instead of
    // restarting the sustain.  Smoke #24/#28: align by FRACTION, not raw
    // frames -- higher-fret samples are shorter, so a verbatim transplant
    // lands at/past the new sample's end late in long UP slides (the old
    // len/4 fallback then restarted the body on every hop = stutter, and the
    // landing exhausted with no ring-out).
    double alignFrac = -1.0;
    if (! firstNote && mActiveVoice != nullptr && mActiveVoice->active
        && mActiveVoice->sample != nullptr && mActiveVoice->sample->numFrames > 0)
        alignFrac = juce::jlimit (0.0, 1.0,
            (double) mActiveVoice->readIdx / (double) mActiveVoice->sample->numFrames);

    Voice* v = allocVoice();

    v->active   = true;
    v->killed   = false;
    v->gestureSerial = mGestureSerial;
    v->lastCutoffHz  = -1.0f;
    v->lastResQ      = -1.0f;
    v->zone     = &zone;
    v->sample   = zone.sample.get();
    v->rootKey  = zone.rootKey;
    // Task 12b: the `amplitude` (+ amplitude_oncc route) term moved to BLOCK
    // rate in renderVoice so cc-gained layers (unison cc100, feedback cc29)
    // follow the live CC instead of latching (or staying silent) at trigger.
    v->baseGain = mBaseGain * velGainFor (zone, mSlideVelocity)
                  * juce::Decibels::decibelsToGain (zone.volumeDb);
    v->age      = 0;
    v->pan      = panSign * (zone.uniPan.cc >= 0
                             ? (zone.uniPan.depth / 100.0f) * ccNorm (zone.uniPan)
                             : 0.0f);
    v->interp.reset();
    v->lpfOn  = zone.hasLpf;
    v->hpfOn  = zone.unisonHpf;
    v->hpfState = 0.0f;
    if (v->lpfOn) v->lpf.reset();
    v->lfoPhase.fill (0.0f);
    v->lfoElapsed.fill (0.0f);

    int offs = zone.offsetFrames;   // Task 11: captured `offset` honored
    if (! firstNote)
    {
        const int len = v->sample->numFrames;
        const double srcRate = v->sample->ready.load (std::memory_order_acquire)
                               ? v->sample->sourceRate : 44100.0;
        const int floorOffs = (int) (mAttackOffsetMs * 0.001 * srcRate);   // SS-Q5 TUNE floor
        // Fraction -> this sample's frames, capped at 80% so a hop/landing
        // ALWAYS keeps enough body to sustain + ring out (smoke #28).
        const int cap = (int) (0.8 * (double) len);   // SS-Q5 TUNE landing-tail floor
        int alignIdx = alignFrac >= 0.0 ? (int) (alignFrac * (double) len) : -1;
        if (alignIdx > cap) alignIdx = cap;
        offs = juce::jmax (offs, alignIdx >= 0 ? juce::jmax (alignIdx, floorOffs)
                                               : floorOffs);
        if (offs >= len) offs = juce::jmax (0, cap);
        if (v->sample->ready.load (std::memory_order_acquire))
            offs = snapZeroCrossing (v->sample->buffer.getReadPointer (0), len, offs);
    }
    v->readIdx = offs;

    const float xf = juce::jmax (1.0f, mCrossfadeMs * 0.001f * (float) mSampleRate);
    v->fade     = firstNote ? 1.0f : 0.0f;
    v->fadeStep = firstNote ? 0.0f : (1.0f / xf);

    // AHDSR: a first note runs the full envelope; a hop enters at the sustain
    // level (mid-body -- no re-attack, matching the takeover semantics).
    latchVoiceEnv (*v, zone);
    if (firstNote)
    {
        v->envStage   = zone.ampegDelay > 0.0f ? Voice::EnvStage::Delay : Voice::EnvStage::Attack;
        v->envLevel   = v->envAttackSec <= 0.0005f ? 1.0f : 0.0f;
        if (v->envLevel >= 1.0f) v->envStage = Voice::EnvStage::Hold;
    }
    else
    {
        v->envStage = Voice::EnvStage::Sustain;
        v->envLevel = v->envSustainLvl;
    }
    v->envTimeSec = 0.0;

    mActiveVoice = v;
    updateVoiceRatio (*v, pitchSemis);
}

void SlideSampler::triggerFlatZone (const Zone& z, float pitchSemis) noexcept
{
    if (z.sample == nullptr || ! z.sample->ready.load (std::memory_order_acquire)) return;

    Voice* v = allocVoice();
    v->active = true; v->killed = false;
    v->gestureSerial = mGestureSerial;
    v->lastCutoffHz  = -1.0f;
    v->lastResQ      = -1.0f;
    v->zone = &z; v->sample = z.sample.get();
    v->rootKey  = z.rootKey;
    v->baseGain = velGainFor (z, mSlideVelocity) * juce::Decibels::decibelsToGain (z.volumeDb);
    v->age = 0; v->pan = 0.0f;
    v->interp.reset();
    v->lpfOn = z.hasLpf; if (v->lpfOn) v->lpf.reset();
    v->hpfOn = false; v->hpfState = 0.0f;
    v->lfoPhase.fill (0.0f); v->lfoElapsed.fill (0.0f);
    v->readIdx = z.offsetFrames;
    // Its own ampeg shapes the entry (feedback swells have real attacks);
    // the amplitude_oncc29 route gains it at block rate -- silent at cc 0.
    v->fade = 1.0f; v->fadeStep = 0.0f;
    latchVoiceEnv (*v, z);
    v->envStage = z.ampegDelay > 0.0f ? Voice::EnvStage::Delay : Voice::EnvStage::Attack;
    v->envLevel = v->envAttackSec <= 0.0005f ? 1.0f : 0.0f;
    if (v->envLevel >= 1.0f) v->envStage = Voice::EnvStage::Hold;
    v->envTimeSec = 0.0;
    updateVoiceRatio (*v, pitchSemis);   // pitched feedback tracks the gesture pitch
}

void SlideSampler::spawnCc29Layers (const ArtSet& a, float pitchSemis) noexcept
{
    // Trigger-time gate, matching sfizz's locc29 evaluation at noteOn: any
    // nonzero cc29 spawns; the layers' amplitude_oncc29 gain curves then
    // follow the CC continuously at block rate.
    if (ccValue (29) < 1.0f) return;

    auto nearest = [pitchSemis] (const std::vector<Zone>& set) -> const Zone*
    {
        const Zone* best = nullptr;
        for (const auto& z : set)
            if (best == nullptr
                || std::abs ((float) z.rootKey - pitchSemis) < std::abs ((float) best->rootKey - pitchSemis))
                best = &z;
        return best;
    };
    if (const Zone* f = nearest (a.feedback)) triggerFlatZone (*f, pitchSemis);
    if (const Zone* n = nearest (a.noise))    triggerFlatZone (*n, pitchSemis);
}

void SlideSampler::startSlide (float startPitchSemis, int velocity, bool reAttack)
{
    const ArtSet* a = art();
    if (a == nullptr) return;

    // Bass cc105 Mono: a new gesture chokes whatever is sounding through the
    // patch's off_time (the mono-set choke feel), independent of cut-self.
    if (a->hasMonoSet && ccValue (105) >= 64.0f)
        chokeAll (a->monoOffTime);

    // G-12: no blanket voice reset -- a prior gesture's tail rings through its
    // release (cut-self OFF) or was already cut by the caller (ON).  The new
    // serial scopes every hop fade to THIS gesture's voices only.
    ++mGestureSerial;
    mActiveVoice = nullptr;
    mGestureArt  = a;

    mSlideVelocity = juce::jlimit (1, 127, velocity);
    mCurrentPitch  = startPitchSemis;
    mHeldSec       = 0.0;
    // Loudness now rides the zone's real velcurve/veltrack (velGainFor) --
    // the old jlimit(0.05,1,vel/100) stopgap is gone (#2 loudness half).
    mBaseGain = 1.0f;

    // sfizz-internal pseudo-CC inputs latch per gesture (see ccValue).
    mGestureNoteCc = juce::jlimit (0.0f, 127.0f, startPitchSemis);
    mGestureRand1  = mRand.nextFloat() * 127.0f;
    mGestureRand2  = mRand.nextFloat() * 127.0f;

    // Task 12b: tailpiece hi/lo switch -- cc118 high at gesture start selects
    // the tailpiece table set.  Trigger-time evaluation (sfizz locc semantics;
    // the kit panel switch sends 0/127); pinned for the gesture's lifetime.
    const LayerTables* tbl = (! a->tailpiece.bands.empty() && ccValue (118) >= 64.0f)
                               ? &a->tailpiece : &a->center;
    mGestureTables = tbl;

    mActiveBand = pickBand (*tbl, mSlideVelocity);
    if (mActiveBand < 0) return;

    const int zi = pickZone (*tbl, mActiveBand, startPitchSemis);
    if (zi < 0) return;

    mActiveZoneRoot = tbl->bands[(size_t) mActiveBand].zones[(size_t) zi].rootKey;

    triggerZone (*tbl, mActiveBand, zi, startPitchSemis, /*firstNote*/ reAttack, 0.0f);

    // Unison t-layers: gain rides amplitude_oncc100 (block-rate through the
    // Task-12 CC provider), pan oncc101 signed by layer, detune tune_cc102.
    const int upBand = pickBand (a->tUp,   mSlideVelocity);
    const int upZi   = pickZone (a->tUp,   upBand, startPitchSemis);
    const int dnBand = pickBand (a->tDown, mSlideVelocity);
    const int dnZi   = pickZone (a->tDown, dnBand, startPitchSemis);
    Voice* center = mActiveVoice;
    if (upBand >= 0 && upZi >= 0) triggerZone (a->tUp,   upBand, upZi, startPitchSemis, reAttack, +1.0f);
    if (dnBand >= 0 && dnZi >= 0) triggerZone (a->tDown, dnBand, dnZi, startPitchSemis, reAttack, -1.0f);
    mActiveVoice = center;   // the steered voice stays the center layer

    // Task 12b: cc29-gated feedback + looped-noise layers.
    spawnCc29Layers (*a, startPitchSemis);

    mAnyActive.store (true, std::memory_order_release);
}

void SlideSampler::moveTo (float currentPitchSemis) noexcept
{
    const ArtSet* a = mGestureArt;   // gesture keeps its trigger-time tables
    const LayerTables* tbl = mGestureTables;
    if (a == nullptr || tbl == nullptr
        || ! mAnyActive.load (std::memory_order_acquire) || mActiveBand < 0) return;
    mCurrentPitch = currentPitchSemis;

    const int zi = pickZone (*tbl, mActiveBand, currentPitchSemis);
    if (zi < 0) return;

    const int newRoot = tbl->bands[(size_t) mActiveBand].zones[(size_t) zi].rootKey;
    if (newRoot != mActiveZoneRoot)
    {
        const float xf = juce::jmax (1.0f, mCrossfadeMs * 0.001f * (float) mSampleRate);
        // Fade out THIS gesture's sounding layer voices (center + unison move
        // together).  Serial-scoped (review fix): a PRIOR gesture's ring-out
        // tails and release-layer voices must survive the hop per G-12 OFF --
        // the old all-voice sweep choked them in 28 ms on the first hop.
        for (auto& v : mVoices)
            if (v.active && v.gestureSerial == mGestureSerial && v.fadeStep >= 0.0f)
                v.fadeStep = -1.0f / xf;
        mActiveZoneRoot = newRoot;

        triggerZone (*tbl, mActiveBand, zi, currentPitchSemis, false, 0.0f);
        Voice* center = mActiveVoice;
        const int upBand = pickBand (a->tUp,   mSlideVelocity);
        const int upZi   = pickZone (a->tUp,   upBand, currentPitchSemis);
        const int dnBand = pickBand (a->tDown, mSlideVelocity);
        const int dnZi   = pickZone (a->tDown, dnBand, currentPitchSemis);
        if (upBand >= 0 && upZi >= 0) triggerZone (a->tUp,   upBand, upZi, currentPitchSemis, false, +1.0f);
        if (dnBand >= 0 && dnZi >= 0) triggerZone (a->tDown, dnBand, dnZi, currentPitchSemis, false, -1.0f);
        mActiveVoice = center;

        // Task 12b: the cc29 layers hop with the gesture (old ones just faded
        // with the all-voice fade above; trigger-time gate re-evaluates).
        spawnCc29Layers (*a, currentPitchSemis);
    }
    else
    {
        for (auto& v : mVoices)
            if (v.active && v.fadeStep >= 0.0f)
                updateVoiceRatio (v, currentPitchSemis);
    }
}

void SlideSampler::release() noexcept
{
    // #4 mechanism: every sounding voice enters its AHDSR Release stage (the
    // patch ampeg_release, guitar 0.25 s) instead of ringing the full one-shot.
    // Task 12b: release time re-latches HERE so a release `_oncc` route (bass
    // cc107) reads the CC at the moment of release, per sfizz semantics.
    for (auto& v : mVoices)
        if (v.active)
        {
            if (v.zone != nullptr && v.zone->envReleaseCc.cc >= 0)
                v.envReleaseSec = juce::jmax (0.01f, v.zone->ampegRelease
                    + v.zone->envReleaseCc.depth * ccNorm (v.zone->envReleaseCc));
            v.envStage = Voice::EnvStage::Release;
        }

    // Release-triggered layer zones (rt_decay attenuates by the held time).
    if (mGestureArt != nullptr && ! mGestureArt->releases.empty())
    {
        const Zone* best = nullptr;
        for (const auto& z : mGestureArt->releases)
            if (best == nullptr
                || std::abs (z.rootKey - mCurrentPitch) < std::abs (best->rootKey - mCurrentPitch))
                best = &z;
        if (best != nullptr && best->sample != nullptr
            && best->sample->ready.load (std::memory_order_acquire))
        {
            Voice* v = allocVoice();
            v->active = true; v->killed = false;
            v->gestureSerial = mGestureSerial;   // the ENDING gesture's serial:
            v->lastCutoffHz  = -1.0f;            // the next startSlide increments,
            v->lastResQ      = -1.0f;            // so hops never fade this tail
            v->zone = best; v->sample = best->sample.get();
            v->rootKey = best->rootKey;
            v->baseGain = velGainFor (*best, mSlideVelocity)
                          * juce::Decibels::decibelsToGain (best->volumeDb
                                - best->rtDecay * (float) mHeldSec);
            v->age = 0; v->pan = 0.0f;
            v->interp.reset();
            v->lpfOn = best->hasLpf; if (v->lpfOn) v->lpf.reset();
            v->hpfOn = false; v->hpfState = 0.0f;
            v->readIdx = best->offsetFrames;
            v->fade = 1.0f; v->fadeStep = 0.0f;
            latchVoiceEnv (*v, *best);
            v->envStage = Voice::EnvStage::Hold; v->envLevel = 1.0f; v->envTimeSec = 0.0;
            updateVoiceRatio (*v, (float) best->rootKey);
        }
    }

    mActiveVoice = nullptr;
    mActiveZoneRoot = -1000;
}

bool SlideSampler::trySelectArticulation (int note) noexcept
{
    for (int i = 0; i < (int) mArts.size(); ++i)
        if (mArts[(size_t) i].swLast == note)
        {
            mActiveArt.store (i, std::memory_order_release);
            return true;
        }
    return false;
}

void SlideSampler::chokeAll (float seconds) noexcept
{
    // jmin keeps the FASTER of an in-flight fade-out and this choke.
    const float step = -1.0f / juce::jmax (1.0f, juce::jmax (0.003f, seconds) * (float) mSampleRate);
    for (auto& v : mVoices)
        if (v.active)
        {
            // Review fix: a voice that never faded in is inaudible -- kill it
            // outright (the old fade=1 flip stepped its gain to full for one
            // ramp = a click window).
            if (v.fade <= 0.0f) { v.active = false; v.killed = false; continue; }
            v.killed   = true;
            v.fadeStep = juce::jmin (v.fadeStep, step);
        }
    mActiveVoice = nullptr;
    mActiveZoneRoot = -1000;
}

void SlideSampler::stopAllNow() noexcept
{
    chokeAll (0.007f);   // G-12 cut-self / #5: declick-length hard stop
}

void SlideSampler::renderVoice (Voice& v, float* L, float* R, int startSample, int numSamples) noexcept
{
    if (v.sample == nullptr || v.zone == nullptr) { v.active = false; return; }
    if (! v.sample->ready.load (std::memory_order_acquire)) return;

    const auto& z = *v.zone;
    const int len = v.sample->numFrames;
    if (len <= 0 || (! z.looped && v.readIdx >= len)) { v.active = false; return; }

    const float* src = v.sample->buffer.getReadPointer (0);

    // ── Block-rate modulation (LFO bank + env stage rates) ──────────────────
    const double blockSec = (double) numSamples / mSampleRate;
    float pitchModCents = 0.0f, volModDb = 0.0f, cutoffModCents = 0.0f;
    if (! v.killed)
    {
        for (size_t li = 0; li < z.lfos.size() && li < v.lfoPhase.size(); ++li)
        {
            const auto& l = z.lfos[li];
            v.lfoElapsed[li] += (float) blockSec;
            float delay = l.delaySec;
            if (l.delayCc.cc >= 0) delay += l.delayCc.depth * ccNorm (l.delayCc);
            if (v.lfoElapsed[li] < delay) continue;
            float freq = l.freqHz;
            if (l.freqModTarget >= 1 && l.freqModHz.cc >= 0)
            {
                // Cross-LFO rate mod (bass wobble): scale by the target LFO's
                // current value when it runs in this voice too.
                for (size_t mj = 0; mj < z.lfos.size() && mj < v.lfoPhase.size(); ++mj)
                    if (z.lfos[mj].index == l.freqModTarget)
                    {
                        const float mv = std::sin (v.lfoPhase[mj] * juce::MathConstants<float>::twoPi);
                        freq += l.freqModHz.depth * ccNorm (l.freqModHz) * mv;
                        break;
                    }
            }
            v.lfoPhase[li] += (float) (juce::jmax (0.0f, freq) * blockSec);
            v.lfoPhase[li] -= std::floor (v.lfoPhase[li]);
            float amp = std::sin (v.lfoPhase[li] * juce::MathConstants<float>::twoPi);
            float fadeIn = 1.0f;
            float fadeSec = l.fadeSec;
            if (l.fadeCc.cc >= 0) fadeSec += l.fadeCc.depth * ccNorm (l.fadeCc);
            if (fadeSec > 0.001f)
                fadeIn = juce::jlimit (0.0f, 1.0f, (v.lfoElapsed[li] - delay) / fadeSec);
            amp *= fadeIn;
            if (l.pitchCents.cc >= 0)  pitchModCents  += amp * l.pitchCents.depth  * ccNorm (l.pitchCents);
            if (l.volumeDb.cc >= 0)    volModDb       += amp * l.volumeDb.depth    * ccNorm (l.volumeDb);
            if (l.cutoffCents.cc >= 0) cutoffModCents += amp * l.cutoffCents.depth * ccNorm (l.cutoffCents);
        }

        // Task 12b: pitcheg -- linear attack to full depth, decay toward the
        // sustain level; cents ride pitchModCents like the LFO pitch routes.
        if (z.hasPeg)
        {
            v.pegTimeSec += blockSec;
            float lvl;
            if (z.pegAttack > 0.0005f && v.pegTimeSec < (double) z.pegAttack)
                lvl = (float) (v.pegTimeSec / (double) z.pegAttack);
            else
            {
                const float sus01 = juce::jlimit (0.0f, 1.0f, z.pegSustainPct / 100.0f);
                const double t2 = v.pegTimeSec - (double) z.pegAttack;
                lvl = (z.pegDecay > 0.0005f)
                        ? 1.0f - (1.0f - sus01) * (float) juce::jmin (1.0, t2 / (double) z.pegDecay)
                        : sus01;
            }
            float depth = z.pegDepthCents;
            if (z.pegDepthCc.cc >= 0) depth += z.pegDepthCc.depth * ccNorm (z.pegDepthCc);
            pitchModCents += depth * lvl;
        }

        // Task 12c: fileg -- same linear A/D shape, cents onto the cutoff.
        if (z.hasFileg)
        {
            v.filegTimeSec += blockSec;
            float lvl;
            if (z.filegAttack > 0.0005f && v.filegTimeSec < (double) z.filegAttack)
                lvl = (float) (v.filegTimeSec / (double) z.filegAttack);
            else
            {
                const float sus01 = juce::jlimit (0.0f, 1.0f, z.filegSustainPct / 100.0f);
                const double t2 = v.filegTimeSec - (double) z.filegAttack;
                lvl = (z.filegDecay > 0.0005f)
                        ? 1.0f - (1.0f - sus01) * (float) juce::jmin (1.0, t2 / (double) z.filegDecay)
                        : sus01;
            }
            float depth = z.filegDepthCents;
            if (z.filegDepthCc.cc >= 0) depth += z.filegDepthCc.depth * ccNorm (z.filegDepthCc);
            cutoffModCents += depth * lvl;
        }

        // Task 12c: gain_cc alias -- volume dB rides the CC directly.
        if (z.volDbCc.cc >= 0)
            volModDb += z.volDbCc.depth * ccNorm (z.volDbCc);
        if (pitchModCents != 0.0f)
        {
            // Review fix: every voice modulates around its stored baseCents
            // (root-relative bend + tune + unison detune from the last
            // updateVoiceRatio) -- the old non-steered branch dropped that
            // sum, snapping unison neighbor-key layers ~a semitone the
            // moment any pitch modulation engaged.
            const double srcRate = v.sample->sourceRate;
            v.ratio = (srcRate / mSampleRate)
                      * std::pow (2.0, (v.baseCents + (double) pitchModCents) / 1200.0);
        }
    }

    // Filter cutoff (bass): keytracked static + cc92 static route + LFO
    // wobble, set per block; resonance rides its cc91 route.  Review fix:
    // the TPT setters recompute coefficients, so they fire on CHANGE only
    // (CPU-safeguard rule) -- static-cutoff voices with idle CCs skip both.
    if (v.lpfOn)
    {
        float hz = z.cutoffHz;
        if (z.filKeytrack != 0.0f)
            hz *= std::pow (2.0f, z.filKeytrack * ((float) v.rootKey - 60.0f) / 1200.0f);
        if (z.cutoffCc.cc >= 0)
            hz *= std::pow (2.0f, (z.cutoffCc.depth * ccNorm (z.cutoffCc)) / 1200.0f);
        // Task 12c: varNN kludge -- product (mult) or sum of the var's cc
        // inputs scales its cutoff-cents depth.
        for (const auto& vr : z.vars)
        {
            if (vr.cutoffCents == 0.0f || vr.numInputs <= 0) continue;
            float x = vr.mult ? 1.0f : 0.0f;
            for (int i = 0; i < vr.numInputs; ++i)
            {
                const float term = vr.inputs[(size_t) i].depth * ccNorm (vr.inputs[(size_t) i]);
                x = vr.mult ? x * term : x + term;
            }
            hz *= std::pow (2.0f, (vr.cutoffCents * x) / 1200.0f);
        }
        if (cutoffModCents != 0.0f)
            hz *= std::pow (2.0f, cutoffModCents / 1200.0f);
        const float hzClamped = juce::jlimit (20.0f, (float) (mSampleRate * 0.45), hz);
        if (hzClamped != v.lastCutoffHz)
        {
            v.lpf.setCutoffFrequency (hzClamped);
            v.lastCutoffHz = hzClamped;
        }
        float q = 0.7071f;
        if (z.resoCc.cc >= 0)
            q = juce::jlimit (0.1f, 10.0f, 0.7071f + z.resoCc.depth * ccNorm (z.resoCc));
        if (q != v.lastResQ)
        {
            v.lpf.setResonance (q);
            v.lastResQ = q;
        }
    }

    // Envelope per-block linear step toward the stage target.
    auto stageRate = [&] (float seconds, float from, float to) -> float
    {
        if (seconds <= 0.0005f) { v.envLevel = to; return 0.0f; }
        return (to - from) / (seconds * (float) mSampleRate);
    };
    float envStep = 0.0f;
    const float sus = v.envSustainLvl;
    switch (v.envStage)
    {
        case Voice::EnvStage::Delay:
            if ((v.envTimeSec += blockSec) >= z.ampegDelay) v.envStage = Voice::EnvStage::Attack;
            break;
        case Voice::EnvStage::Attack:
            envStep = stageRate (v.envAttackSec, 0.0f, 1.0f);
            if (v.envLevel >= 1.0f) { v.envStage = Voice::EnvStage::Hold; v.envTimeSec = 0.0; }
            break;
        case Voice::EnvStage::Hold:
            if ((v.envTimeSec += blockSec) >= v.envHoldSec) v.envStage = Voice::EnvStage::Decay;
            break;
        case Voice::EnvStage::Decay:
            envStep = stageRate (v.envDecaySec, 1.0f, sus);
            if (v.envLevel <= sus + 0.0005f) v.envStage = Voice::EnvStage::Sustain;
            break;
        case Voice::EnvStage::Sustain:
            v.envLevel = juce::jmax (v.envLevel, 0.0f);   // hold
            break;
        case Voice::EnvStage::Release:
            envStep = stageRate (v.envReleaseSec, 1.0f, 0.0f);   // slope from full scale
            break;
        case Voice::EnvStage::Done:
            v.active = false; return;
    }

    // Task 12b: the `amplitude` term evaluates at BLOCK rate so its `_oncc`
    // route (unison cc100, feedback/noise cc29 gain curves) follows the live
    // CC -- an amplitude=0 + oncc zone is silent at cc 0, audible as it rises.
    float ampPct = z.amplitudePct;
    if (z.gainCc.cc >= 0) ampPct += z.gainCc.depth * ccNorm (z.gainCc);
    const float volGain = juce::Decibels::decibelsToGain (volModDb)
                          * (juce::jmax (0.0f, ampPct) / 100.0f);
    const float panL = std::cos ((v.pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi);
    const float panR = std::sin ((v.pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi);
    // 1-pole HPF 250 Hz coefficient (unison layers -- bass fil2 block).
    const float hpA = v.hpfOn
        ? std::exp (-juce::MathConstants<float>::twoPi * 250.0f / (float) mSampleRate) : 0.0f;

    int produced = 0;
    while (produced < numSamples)
    {
        int want = numSamples - produced;
        if (! z.looped)
        {
            const double avail  = (double) (len - v.readIdx);
            const int    maxOut = (int) std::floor ((avail - 2.0) / juce::jmax (1.0e-6, v.ratio));
            want = juce::jlimit (0, want, maxOut);
            if (want <= 0) { v.active = false; break; }
        }
        else
        {
            // Loop wrap (noise layer): rewind before the read runs out.
            const int lend = (v.zone->loopEnd > 0 && v.zone->loopEnd < len) ? v.zone->loopEnd : len;
            if (v.readIdx >= lend - 2)
            {
                v.readIdx = juce::jmax (0, v.zone->loopStart);
                v.interp.reset();
            }
            const double avail = (double) (lend - v.readIdx);
            want = juce::jlimit (0, want, (int) std::floor ((avail - 2.0) / juce::jmax (1.0e-6, v.ratio)));
            if (want <= 0) break;
        }

        const int used = v.interp.process (v.ratio, src + v.readIdx, mScratch.data(), want);
        v.readIdx += used;

        const float halfPi = juce::MathConstants<float>::halfPi;
        for (int k = 0; k < want; ++k)
        {
            v.fade = juce::jlimit (0.0f, 1.0f, v.fade + v.fadeStep);
            v.envLevel = juce::jlimit (0.0f, 1.0f, v.envLevel + envStep);
            float s = mScratch[(size_t) k];
            if (v.hpfOn)
            {
                const float lp = v.hpfState + (1.0f - hpA) * (s - v.hpfState);
                v.hpfState = lp;
                s -= lp;
            }
            if (v.lpfOn) s = v.lpf.processSample (0, s);
            const float g = std::sin (v.fade * halfPi) * v.baseGain * v.envLevel * volGain;
            L[startSample + produced + k] += s * g * panL;
            R[startSample + produced + k] += s * g * panR;
        }
        produced += want;
        if (! z.looped && v.readIdx >= len) { v.active = false; break; }
    }

    if (v.fadeStep < 0.0f && v.fade <= 0.0f) { v.active = false; v.killed = false; }
    if (v.envStage == Voice::EnvStage::Release && v.envLevel <= 0.0005f) v.active = false;
    ++v.age;
}

void SlideSampler::renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    if (out.getNumChannels() < 2 || numSamples <= 0) return;
    if ((int) mScratch.size() < numSamples + 8) return;   // prepare() not sized for this block

    float* L = out.getWritePointer (0);
    float* R = out.getWritePointer (1);

    bool any = false;
    for (auto& v : mVoices)
    {
        if (! v.active) continue;
        renderVoice (v, L, R, startSample, numSamples);
        any = any || v.active;
    }
    mAnyActive.store (any, std::memory_order_release);
    mHeldSec  += (double) numSamples / mSampleRate;
}
