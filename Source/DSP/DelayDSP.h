#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <array>
#include <vector>

// ── DelayDSP ──────────────────────────────────────────────────────────────────
// Full-featured stereo delay with:
//   • Delay models: Stereo / Mono / PingPong / Off (Off = reference-delay
//     behavior: no echoes -- lo-fi / FB filter / FB distortion / tone run
//     instantly on the input, i.e. a pure distortion/filter unit)
//   • Keep-pitch smooth delay time changes (exponential slew)
//   • Feedback chain: diffusion (4 allpass) → Lo-Fi → SVF filter (LP/HP/BP/Off)
//     → distortion
//   • LFO modulation of delay time and/or feedback filter cutoff
//   • Tone filter (bipolar LP↔HP) on the output path
//   • Output limiter (protects against feedback > 1)
//   • Full legacy API retained for backward compat until Change G
// ─────────────────────────────────────────────────────────────────────────────
class DelayDSP : public DSPBase
{
public:
    // H-8 (2026-05-02): Type umbrella matching the Compressor / Saturation
    // pattern.  Echo = the existing full-featured delay (Stereo/Mono/
    // PingPong/Off models, feedback chain, diffusion, lo-fi, mod LFO,
    // tone filter, FB distortion) -- bit-exact unchanged.  VocalDoubler =
    // new Eventide-H910-style doubler: two short detuned taps, no
    // feedback, fixed dual-tap stereo image.
    enum class Type : int
    {
        Echo         = 0,   // existing algorithm bit-exact
        VocalDoubler = 1
    };

    DelayDSP();
    ~DelayDSP() override = default;

    // DSPBase interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer)    override;
    void reset()                                        override;

    // H-8 (2026-05-02): always advertises sidechain support so the SlotComponent
    // SC picker appears -- the Duck Amount knob uses the picked SC source as the
    // envelope-follower trigger.  Falls back to self-sidechain (dry input) when
    // no source is connected so duck still works on a single track.
    bool usesSidechain() const noexcept override { return true; }

    void getStateInformation (juce::MemoryBlock& dest)  override;
    void setStateInformation (const void* data, int sz) override;
    void setHostBPM          (double bpm)               override;

    // ── New API ───────────────────────────────────────────────────────────────
    void setWetIn            (float v);   // 0..1       input wet level into delay
    void setDelayMs          (float ms);  // 1..2000 ms
    void setTempoSync        (bool en);
    void setKeepPitch        (bool en);   // smooth delay time changes (no pitch shift)
    void setSmoothing        (float v);   // 0..1 slew rate (higher = slower transition)
    void setOffsetPan        (float v);   // -1..1 L/R delay time offset for width
    void setDelayModel       (int m);     // 0=Stereo 1=Mono 2=PingPong 3=Off(instant FX chain)
    void setStereoSpread     (float v);   // 0..1 extra L/R delay time difference
    void setFeedbackLevel    (float v);   // 0..1.2 (>1 builds up; limiter protects)
    void setFeedbackFilterType(int t);    // 0=LP  1=HP  2=BP  3=Off(bypass)
    void setFeedbackCutoff   (float hz);
    void setFeedbackResonance(float q);
    void setLoFiSampleRate   (float hz);  // 100..48000 (48000 = full quality)
    void setLoFiBits         (float bits);// 1..24      (24    = full quality)
    void setModRate          (float hz);  // 0..20 Hz LFO rate
    void setModTimeMod       (float v);   // 0..1 depth of time modulation
    void setModCutoffMod     (float v);   // 0..1 depth of cutoff modulation
    void setDiffusionLevel   (float v);   // 0..1 allpass coefficient strength
    void setDiffusionSpread  (float v);   // 0..1 allpass delay time spread
    void setFBDistType       (int t);     // 0=Limit  1=Saturation
    void setFBDistKnee       (float v);   // 0..1  (Limit mode: 0=hard 1=smooth)
    void setFBDistSymmetry   (float v);   // 0..1  (Saturation mode: 0=sym 1=asym DC)
    void setFBDistLevel      (float v);   // 0..10 drive
    void setTone             (float v);   // -1..1 (negative=HP, positive=LP)
    void setWetOut           (float v);   // 0..1
    void setDryOut           (float v);   // 0..1

    // ── H-8 (2026-05-02): Type umbrella + VocalDoubler params ──────────────
    void setType            (int t);          // 0=Echo (default), 1=VocalDoubler
    void setDoubleTimeLMs   (float ms);       // 5..50  (VocalDoubler only)
    void setDoubleTimeRMs   (float ms);       // 5..50
    void setDoubleDetune    (float pct);      // 0..100  drift LFO depth
    void setDoubleWidth     (float pct);      // 0..100  L/R tap pan spread
    void setDoubleRate      (float hz);       // 0.1..2  drift LFO rate

    // ── H-8: sidechain ducking (applies to both Types, post-effect output) ──
    // Uses the dry input level as the sidechain trigger (self-ducking).  When
    // input > threshold, the wet output is attenuated by Amount.  Recreates
    // the classic "delay gets out of the way of the lead vocal" technique.
    void setDuckAmount     (float pct);   // 0..100  0=disabled
    void setDuckThresholdDb(float db);    // -60..0
    void setDuckAttackMs   (float ms);    // 1..200
    void setDuckReleaseMs  (float ms);    // 10..1000

    // H-8: Slapback preset -- writes specific Echo-Type values into the DSP
    // (single short delay ~110 ms, ~12% feedback, low wet, no diffusion).
    // Doesn't change Type; if user is on VocalDoubler, presetSlapback first
    // switches Type back to Echo.
    void presetSlapback();

    // ── Legacy API ────────────────────────────────────────────────────────────
    void setFeedback  (float fb);
    void setWet       (float w);
    void setHpHz      (float hz);
    void setLpHz      (float hz);
    void setPingPong  (bool en);
    void setSyncBPM   (bool en);
    void setSyncNote  (int numerator, int denominator);

    // ── Public state (matches legacy field names for panel compat) ─────────
    float delayMs         { 250.0f };
    float feedback        {   0.4f };
    float wet             {   0.3f };
    float hpHz            {  80.0f };
    float lpHz            { 8000.0f };
    bool  pingPong        { false };
    bool  syncBPM         { false };
    int   syncNumerator   { 1 };
    int   syncDenominator { 4 };

    // ── Public getters for panel construct-time state-sync (A9) ─────────────
    int   getDelayModel      () const noexcept { return mDelayModel;     }
    int   getFBFilterType    () const noexcept { return mFBFilterType;   }
    int   getFBDistType      () const noexcept { return mFBDistType;     }
    bool  getKeepPitch       () const noexcept { return mKeepPitch;      }

    // ── H-8: getters for new umbrella + VocalDoubler + ducking params ──────
    int   getType                () const noexcept { return (int) mType; }
    float getDoubleTimeLMs       () const noexcept { return mDoubleTimeLMs; }
    float getDoubleTimeRMs       () const noexcept { return mDoubleTimeRMs; }
    float getDoubleDetune        () const noexcept { return mDoubleDetune; }
    float getDoubleWidth         () const noexcept { return mDoubleWidth; }
    float getDoubleRate          () const noexcept { return mDoubleRate; }
    float getDuckAmount          () const noexcept { return mDuckAmount; }
    float getDuckThresholdDb     () const noexcept { return mDuckThresholdDb; }
    float getDuckAttackMs        () const noexcept { return mDuckAttackMs; }
    float getDuckReleaseMs       () const noexcept { return mDuckReleaseMs; }

    // ── A9 slider sync (extended 2026-04-18) ────────────────────────────────
    float getFeedbackLevel   () const noexcept { return mFeedbackLevel;  }
    float getWetIn           () const noexcept { return mWetIn;          }
    float getWetOut          () const noexcept { return mWetOut;         }
    float getDryOut          () const noexcept { return mDryOut;         }
    float getFBCutoff        () const noexcept { return mFBCutoff;       }
    float getFBResonance     () const noexcept { return mFBResonance;    }
    float getTone            () const noexcept { return mTone;           }
    float getModRate         () const noexcept { return mModRate;        }
    float getModCutoffMod    () const noexcept { return mModCutoffMod;   }
    float getDiffLevel       () const noexcept { return mDiffLevel;      }
    float getDiffSpread      () const noexcept { return mDiffSpread;     }
    float getLoFiBits        () const noexcept { return mLoFiBits;       }
    float getFBDistLevel     () const noexcept { return mFBDistLevel;    }
    float getFBDistKnee      () const noexcept { return mFBDistKnee;     }
    float getFBDistSymmetry  () const noexcept { return mFBDistSymmetry; }
    float getStereoSpread    () const noexcept { return mStereoSpread;   }
    float getOffsetPan       () const noexcept { return mOffsetPan;      }
    // D.4-Q5 (2026-05-01): getters for previously-hidden params now wired to UI knobs.
    float getSmoothing       () const noexcept { return mSmoothing;      }
    float getLoFiSampleRate  () const noexcept { return mLoFiRate;       }
    float getModTimeMod      () const noexcept { return mModTimeMod;     }

private:
    // ── Internal types ────────────────────────────────────────────────────────

    // 2-pole Direct Form I biquad, independent L/R state
    struct Biquad2P
    {
        float a0{1.f}, a1{0.f}, a2{0.f}, b1{0.f}, b2{0.f};
        float x1L{0.f}, x2L{0.f}, y1L{0.f}, y2L{0.f};
        float x1R{0.f}, x2R{0.f}, y1R{0.f}, y2R{0.f};

        void  reset() { x1L=x2L=y1L=y2L=x1R=x2R=y1R=y2R=0.f; }

        float processL (float x)
        {
            float y = a0*x + a1*x1L + a2*x2L - b1*y1L - b2*y2L;
            x2L=x1L; x1L=x; y2L=y1L; y1L=y;
            return y;
        }
        float processR (float x)
        {
            float y = a0*x + a1*x1R + a2*x2R - b1*y1R - b2*y2R;
            x2R=x1R; x1R=x; y2R=y1R; y1R=y;
            return y;
        }
    };

    // Schroeder allpass filter with stereo state (shared write position)
    struct AllpassStereo
    {
        std::vector<float> stateL, stateR;
        int   writePos    { 0 };
        int   delaySamples{ 1 };
        float coef        { 0.5f };

        void reset()
        {
            std::fill(stateL.begin(), stateL.end(), 0.f);
            std::fill(stateR.begin(), stateR.end(), 0.f);
            writePos = 0;
        }

        void processLR (float& l, float& r)
        {
            const int sz = static_cast<int>(stateL.size());
            if (sz == 0 || delaySamples <= 0) return;

            const int rp = (writePos - delaySamples + sz) % sz;

            // L
            const float wdL = stateL[rp];
            const float wL  = l - coef * wdL;
            stateL[writePos] = wL;
            l = wdL + coef * wL;

            // R
            const float wdR = stateR[rp];
            const float wR  = r - coef * wdR;
            stateR[writePos] = wR;
            r = wdR + coef * wR;

            writePos = (writePos + 1) % sz;
        }
    };

    // ── Constants ─────────────────────────────────────────────────────────────
    static constexpr float kMaxDelaySeconds = 3.0f;
    static constexpr int   kDiffStages      = 4;

    // ── Main delay lines ──────────────────────────────────────────────────────
    std::vector<float> mLineL, mLineR;
    int   mWritePos   { 0 };

    // Smoothed (current) and target delay times in samples, per channel
    float mCurDelayL  { 250.0f };
    float mCurDelayR  { 250.0f };
    float mTargDelayL { 250.0f };
    float mTargDelayR { 250.0f };

    // A2 -- Per-sample smoothed delayMs (spec requires 100 ms ramp for tape-style
    // smoothness). Re-ranges delay on knob-drag without clicking.
    juce::LinearSmoothedValue<float> mDelayMsSmoothed;

    // BPM state
    double mHostBPM { 120.0 };

    // ── New parameters ────────────────────────────────────────────────────────
    float mWetIn          { 1.0f };
    bool  mKeepPitch      { false };
    float mSmoothing      { 0.5f };
    float mOffsetPan      { 0.0f };
    int   mDelayModel     { 0 };        // 0=Stereo 1=Mono 2=PingPong 3=Off
    float mStereoSpread   { 0.0f };
    float mFeedbackLevel  { 0.4f };
    int   mFBFilterType   { 1 };        // 0=LP 1=HP 2=BP 3=Off  (HP default like legacy)
    float mFBCutoff       { 80.0f };
    float mFBResonance    { 0.707f };
    float mLoFiRate       { 48000.0f };
    float mLoFiBits       { 24.0f };
    float mModRate        { 0.0f };
    float mModTimeMod     { 0.0f };
    float mModCutoffMod   { 0.0f };
    float mDiffLevel      { 0.0f };
    float mDiffSpread     { 0.5f };
    // 0=Limit 1=Saturation.  Fresh instances default Sat -- matches the
    // reference delay's fresh-load state (owner-verified on the real unit,
    // 2026-07-02).  Legacy saves without the key load as Limit (the old
    // default) via the explicit fallback in setStateInformation.
    int   mFBDistType     { 1 };
    float mFBDistKnee     { 0.5f };
    float mFBDistSymmetry { 0.0f };
    float mFBDistLevel    { 1.0f };
    float mTone           { 0.0f };
    float mWetOut         { 0.3f };
    float mDryOut         { 1.0f };

    // ── H-8: Type umbrella + VocalDoubler params + ducking ─────────────────
    Type  mType            { Type::Echo };
    float mDoubleTimeLMs   { 13.0f };
    float mDoubleTimeRMs   { 22.0f };
    float mDoubleDetune    { 50.0f };   // 0..100  drift LFO depth
    float mDoubleWidth     { 80.0f };   // 0..100  L/R tap pan spread
    float mDoubleRate      { 0.7f };    // 0.1..2  drift LFO rate (Hz)

    float mDuckAmount      { 0.0f };    // 0..100  0 = disabled
    float mDuckThresholdDb { -24.0f };
    float mDuckAttackMs    { 10.0f };
    float mDuckReleaseMs   { 200.0f };

    // VocalDoubler internal state -- per-tap LFO phase + envelope follower for
    // ducking.  Both paths share mLineL/mLineR delay buffers.
    float mDoubleLfoPhaseL { 0.0f };
    float mDoubleLfoPhaseR { 0.27f };   // offset to decorrelate L/R drift
    float mDuckEnv         { 0.0f };    // 0..1 follower output

    // ── DSP state ─────────────────────────────────────────────────────────────
    juce::dsp::StateVariableTPTFilter<float> mFBTPT;
    Biquad2P    mToneFilter;

    static constexpr float kDiffBaseMs[kDiffStages] = { 1.0f, 3.0f, 7.0f, 15.0f };
    AllpassStereo mDiffusion[kDiffStages];

    float mLFOPhase   { 0.0f };

    // Lo-Fi state
    float mLoFiPhase  { 1.0f };  // init to 1 so first sample captures hold
    float mLoFiHoldL  { 0.0f };
    float mLoFiHoldR  { 0.0f };

    // 5 Hz DC-blocker state (feedback-path, post-distortion / pre-level)
    float mDcBlockXL  { 0.0f }, mDcBlockYL { 0.0f };
    float mDcBlockXR  { 0.0f }, mDcBlockYR { 0.0f };

    // Scaffolding for future Spectral Delay subsystem. Reserved in state
    // serialization so v1 presets survive the eventual addition without
    // requiring migration code. DSP reads neither today; mode remains 0
    // (=standard delay) and band entries default to 0 ms each.
    int                    mDelayMode { 0 };           // 0 = standard (only supported)
    std::array<float, 8>   mBandDelayMs { { 0,0,0,0,0,0,0,0 } };

    // ── Helpers ───────────────────────────────────────────────────────────────
    void recalcTargetDelay();
    void updateToneFilter();
    void updateDiffusion();
    void applyFBFilterType();   // push mFBFilterType → mFBTPT.setType(...)

    static void computeBiquadCoefs (Biquad2P& bq, double sr, float fc, float q, int type);
    static float softLimit  (float x, float knee);
    static float linInterp  (const std::vector<float>& buf, float rpos, int size);
    static float cubicInterp(const std::vector<float>& buf, float rpos, int size);
    static float bitcrush   (float x, float bits);
};
