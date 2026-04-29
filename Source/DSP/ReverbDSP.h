#pragma once
#include <JuceHeader.h>
#include "DSPBase.h"
#include <array>
#include <vector>

//──────────────────────────────────────────────────────────────────────────────
// ReverbDSP — 8-line FDN reverb + 5F-9 §8 quality pass
//
//  Base architecture:
//   • 8 delay lines, prime lengths × RoomSize scale (@ current SR)
//   • Normalised Hadamard H8 feedback mixing matrix
//   • Per-line feedback gain: exp(−6.91 × len_s / Decay) (RT60 target)
//   • Per-line high-damping 1-pole LP in feedback path
//   • Per-line bass shelf (1-pole LP blend) in feedback path
//   • 4-stage Schroeder allpass pre-diffusion
//   • Pre-delay ring buffer (up to 500 ms, optional BPM snap)
//   • Input HP (LowCut) + LP (HighCut) filters
//   • Separate L/R delay buffers for natural stereo decorrelation
//   • M/S-width stereo separation control on wet output
//   • Processing modes: Stereo / MidOnly / SideOnly
//
//  5F-9 §8 additions:
//   • §8a per-line tail modulation (sine LFO, ±depth ms, cubic-interp reads;
//         depth==0 is an exact bypass — zero-cost)
//   • §8b FDN buffers allocated at MAX room size once in prepare(); room-size
//         changes update mFDNLen only — no re-allocation, no state zeroing
//   • §8c Freeze toggle — smooth on/off ramp; when engaged, feedback gains go
//         to 1.0, input inject → 0, HF damp + bass shelf → off (prevents drift)
//   • §8d Multi-tap early reflections (3 taps per line at prime-ratio positions
//         len/5, len/3, len/2 with natural-decay gain envelope — 24 taps total)
//   • §8e HF Decay Ratio — post-LP tilt on feedback path so highs decay faster
//         (ratio<1) or slower (ratio>1) than the overall Decay time
//   • §8f Tail-mod shape selector (sine / triangle / random S&H)
//   • §8g Wet Tone tilt — ±12 dB around 1 kHz pivot, wet path only
//   • §8h BassCrossover knob (already in DSP, now exposed to UI)
//
//  Audit fixes: CPU guards, R2 ER clamp, R3 host-BPM sync refresh, R5 state-
//  load clear, R7 mode clamp.
//──────────────────────────────────────────────────────────────────────────────
class ReverbDSP : public DSPBase
{
public:
    ReverbDSP();
    ~ReverbDSP() override = default;

    // ── DSPBase interface ────────────────────────────────────────────────────
    void prepare (double sampleRate, int maxBlockSize) override;
    void process (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sz) override;
    void setHostBPM (double bpm) override;

    // ── FDN API ──────────────────────────────────────────────────────────────
    void setProcessingMode  (int   m);    // 0=Stereo, 1=MidOnly, 2=SideOnly
    void setLowCut          (float hz);   // input HPF cutoff
    void setHighCut         (float hz);   // input LPF cutoff
    void setPreDelay        (float ms);   // 0–500 ms
    void setTempoSync       (bool  on);   // BPM-locked pre-delay
    void setRoomSize        (float v);    // room scale 0–2
    void setDiffusion       (float v);    // 0–1  allpass coefficient
    void setDecay           (float sec);  // RT60 in seconds
    void setBassMult        (float v);    // bass tail multiplier (1.0 = flat)
    void setBassCrossover   (float hz);   // bass shelf corner frequency
    void setHighDamp        (float hz);   // per-line LP cutoff in feedback
    void setHighDampBypass  (bool bypass);
    void setER              (float dB);   // early reflections level dB
    void setStereoSep       (float pct);  // stereo separation 0–200%
    void setDry             (float v);    // dry mix 0–1
    void setWet             (float v);    // wet mix 0–1

    // ── 5F-9 §8 extended setters ─────────────────────────────────────────────
    void setTailModDepth    (float ms);   // §8a 0–1.5 ms (default 0.3)
    void setTailModRate     (float hz);   // §8a 0.05–2 Hz (default 0.35)
    void setTailModShape    (int   shape);// §8f 0=sine, 1=triangle, 2=random S&H
    void setFreeze          (bool  on);   // §8c infinite-hold
    void setHFRatio         (float ratio);// §8e 0.3–2.0 (default 0.7)
    void setWetTone         (float dB);   // §8g -12..+12 dB tilt @ 1 kHz

    // ── Legacy setters (keep old editor panel compiling until Change G) ──────
    void setSize    (float v);   // old 0-1 → setRoomSize
    void setDamp    (float v);   // old 0-1 → setHighDamp (bright→dark)
    void setMsMode  (bool  e) { setProcessingMode(e ? 2 : 0); }
    void setErLevel (float v);   // old 0-1 → setER dB

    // ── Public getters for panel construct-time state-sync (A9 extended to sliders) ─────
    float getRoomSize()       const noexcept { return mRoomSize;       }
    float getDecay()          const noexcept { return mDecay;          }
    float getHFRatio()        const noexcept { return mHFRatio;        }
    float getDiffusion()      const noexcept { return mDiffusion;      }
    float getPreDelayMs()     const noexcept { return mPreDelayMs;     }
    float getWetTiltDb()      const noexcept { return mWetTiltDb;      }
    float getWet()            const noexcept { return mWet;            }
    float getDry()            const noexcept { return mDry;            }
    float getLowCutHz()       const noexcept { return mLowCutHz;       }
    float getHighCutHz()      const noexcept { return mHighCutHz;      }
    float getBassMult()       const noexcept { return mBassMult;       }
    float getBassCrossHz()    const noexcept { return mBassCrossHz;    }
    float getTailModDepthMs() const noexcept { return mTailModDepthMs; }
    float getTailModRateHz()  const noexcept { return mTailModRateHz;  }
    float getStereoSep()      const noexcept { return mStereoSep;      }
    float getHighDampHz()     const noexcept { return mHighDampHz;     }

private:
    // ── Constants ────────────────────────────────────────────────────────────
    static constexpr int   kN           = 8;     // FDN lines
    static constexpr int   kDiff        = 4;     // allpass pre-diffusion stages
    static constexpr int   kERTaps      = 3;     // §8d — 3 taps per line
    static constexpr float kMaxRoomSize = 2.0f;  // §8b max-alloc ceiling for FDN lines
    static constexpr float kMaxSR       = 192000.0f; // §8b max-alloc ceiling for SR scaling

    // Base prime delay lengths at 44100 Hz
    static const int kPrimes[kN];

    // Allpass pre-diffusion delay times (ms)
    static constexpr float kAPMs[kDiff] = { 5.1f, 8.4f, 12.7f, 17.3f };

    // §8d ER tap position fractions and natural-decay gain envelope (24 taps total)
    static constexpr float kERTapFrac [kERTaps] = { 0.20f, 0.3333f, 0.50f };
    static constexpr float kERTapGain [kERTaps] = { 1.00f, 0.70f,   0.45f };

    // ── Parameters ───────────────────────────────────────────────────────────
    int   mMode          {  0      };
    float mLowCutHz      { 80.0f   };
    float mHighCutHz     { 18000.0f};
    float mPreDelayMs    { 10.0f   };
    bool  mTempoSync     { false   };
    float mRoomSize      {  0.6f   };
    float mDiffusion     {  0.5f   };
    float mDecay         {  2.0f   };
    float mBassMult      {  1.2f   };
    float mBassCrossHz   { 250.0f  };
    float mHighDampHz    { 8000.0f };
    bool  mHighDampBypass{ false   };
    float mERdB          { -6.0f   };
    float mStereoSep     { 100.0f  };
    float mDry           {  0.7f   };
    float mWet           {  0.3f   };
    double mHostBPM      { 120.0   };

    // §8a tail modulation
    float mTailModDepthMs { 0.3f  };
    float mTailModRateHz  { 0.35f };
    int   mTailModShape   { 0     };   // §8f 0=sine, 1=triangle, 2=random S&H
    std::array<float, kN> mTailModPhase {};
    std::array<float, kN> mTailModSH    {};  // random S&H held value per line

    // §8c freeze
    bool mFreeze { false };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mFreezeSmooth;

    // §8e HF decay ratio
    float mHFRatio { 0.7f };

    // §8g wet-tone tilt — cascaded low-shelf (250 Hz) + high-shelf (4 kHz)
    // biquad pair, Pultec-style: highShelf gain = +dB/2, lowShelf gain = -dB/2
    // so positive WetTone = bright (cuts lows, boosts highs), negative = warm.
    // Cookbook RBJ biquads — provably stable for valid coefs.
    float mWetTiltDb { 0.0f };

public:
    // TiltBiquad must be public so the free-function computeLoShelf /
    // computeHiShelf helpers in ReverbDSP.cpp can take it by reference.
    struct TiltBiquad
    {
        float b0{1.f}, b1{0.f}, b2{0.f}, a1{0.f}, a2{0.f};
        float x1L{0.f}, x2L{0.f}, y1L{0.f}, y2L{0.f};
        float x1R{0.f}, x2R{0.f}, y1R{0.f}, y2R{0.f};
        float processL (float x) noexcept
        {
            const float y = b0*x + b1*x1L + b2*x2L - a1*y1L - a2*y2L;
            x2L=x1L; x1L=x; y2L=y1L; y1L=y;
            return y;
        }
        float processR (float x) noexcept
        {
            const float y = b0*x + b1*x1R + b2*x2R - a1*y1R - a2*y2R;
            x2R=x1R; x1R=x; y2R=y1R; y1R=y;
            return y;
        }
        void reset() noexcept { x1L=x2L=y1L=y2L=x1R=x2R=y1R=y2R=0.f; }
    };

private:
    TiltBiquad mWetLoShelf;   // 250 Hz
    TiltBiquad mWetHiShelf;   // 4 kHz

    // ── Pre-delay ring buffers ────────────────────────────────────────────────
    static constexpr float kMaxPreDelayMs = 500.0f;
    std::vector<float> mPreBufL, mPreBufR;
    int mPreWrite        { 0 };
    int mPreDelaySamples { 0 };

    // ── Input filters (1-pole HPF + 1-pole LPF) ──────────────────────────────
    float mInHPCoef  { 0.0f };
    float mInHPStL   { 0.0f }, mInHPStR   { 0.0f };
    float mInHPPrevL { 0.0f }, mInHPPrevR { 0.0f };
    float mInLPAlpha { 1.0f };
    float mInLPStL   { 0.0f }, mInLPStR   { 0.0f };

    // ── Pre-diffusion allpass stages (L and R separate) ───────────────────────
    std::array<std::vector<float>, kDiff> mAPBufL, mAPBufR;
    std::array<int, kDiff> mAPLen  {}, mAPWrL  {}, mAPWrR  {};
    float mAPCoef { 0.35f };

    // ── FDN delay lines (separate L/R buffers, same lengths) ─────────────────
    //  §8b: buffers are MAX-allocated in prepare() once; mFDNLen[i] is the
    //  effective length used by reads/writes/wraparound. Unused buffer tail
    //  stays allocated. No realloc on room-size changes.
    std::array<std::vector<float>, kN> mFDNL, mFDNR;
    std::array<int, kN> mFDNLen {}, mFDNWr {};

    // ── Feedback processing coefficients and state ────────────────────────────
    std::array<float, kN> mFeedGain {};

    // High-damping LP (1-pole, shared coefficient, per-line state)
    float mLPAlpha { 1.0f };
    std::array<float, kN> mLPStL {}, mLPStR {};

    // Bass shelf: 1-pole LP + blend (shared coefficients, per-line state)
    float mBassLPCoef  { 0.0f };
    float mBassGainAdd { 0.2f };   // = mBassMult - 1.0f
    std::array<float, kN> mBassStL {}, mBassStR {};

    // ── Update helpers ───────────────────────────────────────────────────────
    void updateInputFilters();
    void updateFeedback();
    void updateDelayLines();
    void updateDiffusion();
    void updatePreDelay();
    void updateWetTone();    // §8g

    // ── DSP helpers ──────────────────────────────────────────────────────────
    static float processAP (float x, std::vector<float>& buf, int len, int& wp, float g);
    static void  applyH8   (std::array<float, kN>& v);
    static float cubicInterpCircular (const std::vector<float>& buf, float pos, int size);  // §8a
    static float tailLFOVal (int shape, float phaseRad, float& sh);                          // §8a/§8f
};
