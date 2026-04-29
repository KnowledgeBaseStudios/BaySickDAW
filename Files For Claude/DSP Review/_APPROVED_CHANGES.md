# DSP Review — Approved Changes

Running list of changes Jeff approved during DSP review walkthrough.
Nothing in this file has been implemented yet — action happens after all files are reviewed.

---

## 1. ChorusDSP — Per-Voice Base Delay Offset
**File:** `Source/DSP/ChorusDSP.cpp` (and .h if state needs it)

**Change:** Add prime-number base-delay offsets per voice so the 3 (or 6) voices no longer all share the same `baseSamples`. Currently voices are separated only by LFO phase and rate, which can leave residual resonant peaks when LFOs happen to align.

**Spec:**
- Voice 0: `baseSamples + 0ms`
- Voice 1: `baseSamples + 2ms`
- Voice 2: `baseSamples + 5ms`
- Voices 3–5 (6-voice mode): same pattern, staggered (e.g. +1ms, +3ms, +7ms, or reuse 0/2/5 with the existing π phase offset)

**Location in code:** inside the voice loop in `ChorusDSP::process()` (around line 151), where `rposL`/`rposR` are computed — add a per-voice offset constant to `baseSamples` before the subtract.

**Note:** must ensure `kMaxDelayMs` still covers `base + max offset + depth`. Current value is 55ms; delay max is 30ms + depth max 20ms = 50ms. Adding 7ms offset → 57ms. **Bump `kMaxDelayMs` to 64ms** for safety.

---

## 2. CompressorDSP — Four Additions
**File:** `Source/DSP/CompressorDSP.h` + `.cpp`

### 2a. Look-ahead delay
Add a small delay line (2–5 ms, user-selectable or fixed at 3 ms) applied to the **main audio path only** — the detector reads from the undelayed signal so the compressor reacts *before* the transient hits the output.

**Implementation:**
- Add `std::vector<float> mLookaheadL, mLookaheadR;` + `int mLAWritePos, mLASamples;`
- In `prepare()`: size to `ceil(5ms * sr) + 1`
- In `process()`: write current sample to delay line, read sample `mLASamples` behind for the main gain application. Detector/envelope path still uses the live input (or sidechain).
- Add `setLookaheadMs(float ms)` clamped 0..5 ms. Store `lookaheadMs` (default 0 = off to preserve existing behavior).
- Serialize `lookaheadMs` in state.

### 2b. True stereo linking
Currently each channel has its own envelope (`mEnvL`, `mEnvR`) and its own GR is applied — image can shift on asymmetric transients.

**Fix:** compute a single detection value as `max(|L|, |R|)` (or `max(rmsL, rmsR)`), run one envelope, apply the same GR to both channels. Keep `mEnvL`/`mEnvR` as the shared envelope (pick one, retire the other).

**Optional user control:** add `bool stereoLink` (default true) so advanced users can opt back into dual-mono.

### 2c. Auto-makeup gain toggle
Add `bool autoMakeup { false }` + `setAutoMakeup(bool)`.

When `autoMakeup == true`, compute the makeup amount as `-computeGainDb(0.0f)` (the gain reduction the compressor would apply at 0 dBFS input) and use *that* in place of `makeupDb` for the final multiply. Manual `makeupDb` is ignored when auto is on.

Serialize `autoMakeup`.

### 2d. Per-sample envelope detection (replaces per-block RMS)
Current `process()` computes one RMS value for the entire block, so transients shorter than the block length are smeared. Replace with per-sample detection:

**Per-sample loop structure:**
```cpp
for (int i = 0; i < numSamples; ++i) {
    // 1. detection source sample (stereo-linked: max(|L|,|R|) or from sidechain)
    float det = std::max(std::abs(detL[i]), std::abs(detR[i]));

    // 2. one-pole running RMS (or peak) — short window, e.g. 10 ms
    mRunningRms = rmsCoef * mRunningRms + (1.0f - rmsCoef) * det * det;
    float levelDb = 10.0f * std::log10(std::max(mRunningRms, 1e-12f));
    // (note: 10*log10 on mean-square = 20*log10 on RMS)

    // 3. gain computer → target GR in dB
    float targetDb = computeGainDb(levelDb);

    // 4. attack/release smoothing (existing logic)
    mEnv = (targetDb < mEnv ? mAttackCoef : mReleaseCoef) * mEnv
           + (1.0f - ...) * targetDb;

    // 5. apply to lookahead-delayed audio + makeup
    float g = dbToGain(mEnv) * makeupLin;
    outL[i] = mix * (delayedL * g) + (1-mix) * delayedL;
    outR[i] = mix * (delayedR * g) + (1-mix) * delayedR;
}
```

**New members:**
- `float mRunningRms { 0.0f };`
- `float mRmsCoef { 0.0f };` — computed in `calcCoefs()` from a ~10 ms time constant

Retire the existing `rms()` lambda in `process()`.

**Meter:** `mGainReductionDb` still stores the smoothed envelope each sample (throttle the atomic store to every N samples if profiling shows it matters).

---

## 3. DelayDSP — Three Additions
**File:** `Source/DSP/DelayDSP.h` + `.cpp`

### 3a. 5 Hz DC-blocker in feedback path
Saturation with `mFBDistSymmetry > 0` injects DC that compounds per repeat and eats headroom.

**Implementation:**
- Add state: `float mDcBlockXL{0}, mDcBlockYL{0}, mDcBlockXR{0}, mDcBlockYR{0};` + `float mDcBlockCoef{0.0f};`
- In `prepare()`: `mDcBlockCoef = 1.0f - exp(-2π · 5.0 / sampleRate);` (or use the classic `R` form: `R = 1 - 2π·5/sr`, `y[n] = x[n] - x[n-1] + R·y[n-1]`)
- In `process()`: place DC blocker **after** the feedback distortion stage and **before** the feedback-level multiply (i.e. right before "5. Feedback level"):
  ```cpp
  float dcOutL = feedL - mDcBlockXL + 0.9995f * mDcBlockYL;
  mDcBlockXL = feedL; mDcBlockYL = dcOutL; feedL = dcOutL;
  // same for R
  ```
  Using the `R = 0.9995` form is simpler and doesn't need `prepare()`. Pick one approach and be consistent. (The `R = 1 - 2π·fc/sr` form is marginally more accurate at non-48k rates — prefer that for correctness at 96k/192k.)
- In `reset()`: zero the four DC-blocker state variables.

### 3b. Cubic interpolation for delay reads
Replace `linInterp()` with cubic (Catmull-Rom), matching the approach already in `ChorusDSP::cubicInterp()`.

**Implementation:**
- Add `static float cubicInterp(const std::vector<float>& buf, float rpos, int size);` to `DelayDSP.h` private section (or copy the Chorus implementation verbatim — Catmull-Rom 4-point).
- Remove or keep `linInterp` (keep for now — harmless dead code, or delete for cleanliness).
- In `process()` change the two call sites:
  ```cpp
  rawReadL = cubicInterp(mLineL, rposL, lineSize);
  rawReadR = cubicInterp(mLineR, rposR, lineSize);
  ```

### 3c. Wire `mModCutoffMod` + upgrade feedback filter to TPT
Currently `setModCutoffMod()` exists and `mModCutoffMod` is stored/serialized, but it is **never read in `process()`** — dead parameter.

Upgrading the feedback filter to TPT (Topology-Preserving Transform) is needed so that per-sample cutoff modulation doesn't thump.

**Implementation:**
- Replace `Biquad2P mFBFilter` with `juce::dsp::StateVariableTPTFilter<float> mFBTPT;` (or implement a hand-rolled TPT SVF for full control — JUCE's is fine).
- In `prepare()`: `mFBTPT.prepare({sampleRate, (juce::uint32)mMaxBlock, 2});` and set type/cutoff/resonance initial values.
- Replace `updateFBFilter()` body: map `mFBFilterType` {0=LP, 1=HP, 2=BP} to JUCE's `Type::lowpass/highpass/bandpass`, set cutoff and resonance.
- In `process()` sample loop, compute modulated cutoff **per sample**:
  ```cpp
  // mModCutoffMod scales cutoff by +/- (mModCutoffMod * octaves), e.g. ±1 octave at full depth
  const float cutoffMod = std::exp2(mModCutoffMod * lfoVal);   // lfoVal is already -1..1
  const float modulatedCutoff = juce::jlimit(20.0f, 20000.0f, mFBCutoff * cutoffMod);
  mFBTPT.setCutoffFrequency(modulatedCutoff);
  feedL = mFBTPT.processSample(0, feedL);
  feedR = mFBTPT.processSample(1, feedR);
  ```
- In `reset()`: `mFBTPT.reset();`
- **Note on StateVariableTPTFilter gotcha (from CLAUDE.md):** if user sets resonance to 0, with highpass at 20kHz → blocks all audio. Our feedback cutoff range is 20–20000 and type is selectable, so this is fine in normal operation, but make sure `mFBResonance` default (0.707) stays sane.

### 3d. Remove `Biquad2P mFBFilter` and its `updateFBFilter()`
Only after 3c is in. `mToneFilter` (also a Biquad2P) stays — it's on the **output** path, not in a feedback loop, so biquad is fine there.

---

## 4. FlangerDSP — Three Additions
**File:** `Source/DSP/FlangerDSP.h` + `.cpp`

### 4a. Move damp filter into the feedback loop
Currently `mDampStateL/R` filters `delL/delR` **after** `mFbL = delL` has been captured for feedback — so the signal looping back into the buffer is undamped. That defeats the stated purpose of taming feedback screech.

**Fix (in `process()`):**
1. Compute `delL`, `delR` from `readAt(...)` as today.
2. Apply the damp LP filter to `delL`/`delR` **before** storing feedback:
   ```cpp
   if (mDamp > 0.0f) {
       mDampStateL = mDamp * mDampStateL + (1.0f - mDamp) * delL;
       mDampStateR = mDamp * mDampStateR + (1.0f - mDamp) * delR;
       delL = mDampStateL; delR = mDampStateR;
   }
   mFbL = delL;  mFbR = delR;   // feedback tap is now post-damp
   ```
3. Remove the second damp filter application (lines 138–144) — it's now already applied.
4. `mInvertWet` still operates on `delL`/`delR` for the **output** only; do NOT let it affect `mFbL`/`mFbR` (current code already captures feedback before invertWet — preserve that ordering).

**Order in the sample loop becomes:**
read → damp → capture feedback (`mFbL = delL`) → invertWet (output only) → mix to output.

### 4b. Cubic interpolation for delay reads
Replace the `readAt` lambda's linear interpolation with 4-point Catmull-Rom cubic, matching Chorus/Delay.

**Implementation:**
- Either hoist to a static `cubicInterp(buf, dSamp, writePos, bufSize)` helper (preferred — reusable), or keep it as a lambda.
- 4 sample points: `r(id-1), r(id), r(id+1), r(id+2)` measured back from `writePos`:
  ```cpp
  auto readCubic = [&](const std::vector<float>& buf, float dSamp) -> float {
      int   id   = (int)dSamp;
      float f    = dSamp - (float)id;
      auto idx = [&](int off) { return (mWritePos - id - off + bufSize * 2) % bufSize; };
      float y0 = buf[idx(-1)];  // one sample newer
      float y1 = buf[idx( 0)];
      float y2 = buf[idx( 1)];
      float y3 = buf[idx( 2)];
      const float a = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
      const float b =       y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
      const float c = -0.5f*y0 + 0.5f*y2;
      return ((a*f + b)*f + c)*f + y1;
  };
  ```
  **Wrap safety:** the `+ bufSize * 2` before `% bufSize` keeps the index positive for all offsets (-1..+2). Verify this is correct for the flanger's read direction — flanger reads *back* from writePos, so `idx(-1)` refers to the most recent sample (one step closer to writePos) and `idx(2)` is two steps older. That matches the Catmull-Rom coefficient layout (y0=newest, y3=oldest).

### 4c. LinearSmoothedValue on mRate, mDepth, mFeedback
Low-priority zipper-noise guard. Wrap the three parameters in `juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>` with a ~20 ms ramp.

**Implementation sketch:**
- Add members: `juce::SmoothedValue<float> mRateSmooth, mDepthSmooth, mFeedbackSmooth;`
- In `prepare()`: `mRateSmooth.reset(sampleRate, 0.02); mRateSmooth.setCurrentAndTargetValue(mRate);` (and same for Depth/Feedback)
- In `setRate/setDepth/setFeedback`: also call `mRateSmooth.setTargetValue(...)`
- In `process()` sample loop: pull `float rate = mRateSmooth.getNextValue();` etc. and use locally (replace the outer-scope `mRate`, `mDepth`, `mFeedback` reads)
- Be careful: `phaseInc` is currently computed **outside** the loop from `mRate`. Move that inside the loop, or keep it outside if you're okay with per-block granularity on rate changes (acceptable — rate changes are usually slow).
- `mDepth` → `sweepSamp` is also currently outside the loop — move inside, cheap.
- `mFeedback` → `fb` is outside — move inside so smoothing is visible sample-by-sample.
- Serialization: no change (smoothed values are transient, not stored).

---

## 5. LimiterDSP — Net-New Build
**Files to create:** `Source/DSP/LimiterDSP.h` + `Source/DSP/LimiterDSP.cpp`
**No existing code to compare against** — this is additive. Derived from combined spec in `Limiter.txt` Section 1 (peak-limiter fundamentals) + Section 2 (Fruity-Limiter-style architecture).

### Signal chain (chronological)
```
input → InputGain → tanh SoftSat(Thresh,Curve) → DelayLine(0–10 ms)
                                                      ↓
                         (pre-delay signal) → 4× oversampled TP detector
                                                      ↓
                                              Peak envelope follower
                                                      ↓
                                       GainComputer: min(1, ceiling/peak)
                                                      ↓
                                         apply GR to delayed audio
                                                      ↓
                                         hard ceiling clamp (safety)
                                                      ↓
                                                   output
```

### Parameters (all with sensible defaults + JUCE SmoothedValue where noted)
| Param          | Range        | Smoothed | Default |
|----------------|--------------|----------|---------|
| InputGain      | -12..+24 dB  | ✅ 15ms  | 0 dB    |
| Ceiling        | -24..0 dB    | ✅ 15ms  | -0.3 dB |
| SatThresh      | 0..1 (lin)   | ✅ 15ms  | 1.0 (off) |
| SatCurve       | 0..1         | no       | 0.5     |
| Attack         | 0.1..20 ms   | no (coef recalc) | 1 ms |
| Release        | 10..1000 ms  | no (coef recalc) | 100 ms |
| Ahead          | 0..10 ms     | no (delay line size) | 2 ms |
| ReleaseCurve   | 0..1 (lin↔exp) | no     | 0.5     |
| AutoRelease    | bool         | no       | false   |

### Implementation checklist (a–o from confirmed spec)

- **(a) Input Gain** — `juce::SmoothedValue<float>` applied to input samples before anything else.
- **(b) Soft Saturator** — `tanh(drive * x) / tanh(drive)` with drive derived from `1 - SatThresh` and `SatCurve` shaping the knee. Applied per-channel after InputGain.
- **(c) Look-ahead DelayLine** — `std::vector<float> mDelayL, mDelayR` circular buffer sized to `max_ahead_samples + 4`. Write position, read position = write - mAheadSamples. Do NOT use `juce::dsp::DelayLine` (fractional interp overkill for integer-sample lookahead) — hand-rolled is fine.
- **(d) Pre-delay detector tap** — detector reads directly from the input sample (before it's written to the delay line). This is the key look-ahead property.
- **(e) 4× oversampled True Peak detection** — use `juce::dsp::Oversampling<float>` with `factor = 2, stages = 2` (2 stages of 2× = 4× total) in IIR mode (low-latency) OR polyphase mode (better accuracy, higher latency). Start with **IIR for minimal latency**; evaluate later if polyphase is preferred. The oversampler takes the detector-path block each processBlock, produces 4× samples, we take `max(|sample|)` across all 4× samples as the true-peak estimate for that output sample.
  - Alternative cheaper path: instead of JUCE's Oversampling, implement a 4-tap FIR interpolator (common ITU-R BS.1770-style) — this is what most pro limiters use and adds no reported latency beyond the filter group delay. Prefer this if CPU shows up.
- **(f) Peak envelope** — instantaneous attack (`env = max(env, peak)`), exponential release (`env = peak + releaseCoef * (env - peak)` when peak < env).
- **(g) Two-stage adaptive release** — maintain two parallel envelopes (fast ~20 ms, slow ~300 ms). Output envelope = blended based on current GR amount: more GR → more weight on slow envelope. Formula: `blendSlow = jlimit(0, 1, (currentGrDb > 6 ? 1.0 : currentGrDb/6.0))`. When `AutoRelease == false`, use only the user Release param (no blending).
- **(h) Release curve shape** — `ReleaseCurve` param morphs the envelope release equation between linear decay (`env -= step`) and exponential (`env *= releaseCoef`). Implement as `env = mix(linearRelease, expRelease, ReleaseCurve)`.
- **(i) Gain computer** — `float peakLin = envelope; float ceilingLin = dbToGain(Ceiling); float targetGain = min(1.0f, ceilingLin / max(peakLin, 1e-9f));` Stereo linking: detector uses `max(|L|, |R|)` → single envelope → same gain applied to both channels.
- **(j) Hard ceiling clamp** — after `outL *= targetGain`, `outL = jlimit(-ceilingLin, ceilingLin, outL)`. Protects against any residual overshoot from interpolation / envelope slop.
- **(k) SmoothedValue** — as marked in param table above. `reset(sampleRate, 0.015)`.
- **(l) Atomic meters** — `std::atomic<float> mInputDb{-96}, mOutputDb{-96}, mGrDb{0};`. Update once per process block (peak of block) to keep overhead low. Also add `float getCurrentGainReduction() const { return mGrDb.load(); }` (and override `getGainReductionDb()` from DSPBase). Add `getInputLevelDb()` / `getOutputLevelDb()` accessors for the future scrolling visualizer.
- **(m) `getLatencySamples()`** — override returning `mAheadSamples + oversamplerLatency`. EffectRack already accumulates this for PDC.
- **(n) `juce::ValueTree` state** — tag `"LimiterDSP"`, serialize all 9 params + `bypassed`. Match pattern used in `CompressorDSP::getStateInformation`.
- **(o) EffectRack registration** — add new effect type enum value (e.g. `EffectType::Limiter`), factory case in `EffectRack::makeEffect()` (or wherever slot instantiation lives), include header. **Check `Source/EffectRack.cpp` for the exact pattern before wiring.**

### Class skeleton
```cpp
class LimiterDSP : public DSPBase {
public:
    LimiterDSP();
    ~LimiterDSP() override = default;

    void prepare(double sr, int maxBlock) override;
    void process(juce::AudioBuffer<float>& buf) override;
    void reset() override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    float getGainReductionDb() const override { return mGrDb.load(); }
    int   getLatencySamples() const override;

    // setters
    void setInputGain(float dB);
    void setCeiling(float dB);
    void setSatThresh(float lin);
    void setSatCurve(float v01);
    void setAttack(float ms);
    void setRelease(float ms);
    void setAhead(float ms);
    void setReleaseCurve(float v01);
    void setAutoRelease(bool on);

    // meters
    float getCurrentGainReduction() const { return mGrDb.load(); }
    float getInputLevelDb() const  { return mInputDb.load(); }
    float getOutputLevelDb() const { return mOutputDb.load(); }

private:
    // params
    juce::SmoothedValue<float> mInputGain, mCeilingDb, mSatThresh;
    float mAttackMs{1}, mReleaseMs{100}, mAheadMs{2}, mReleaseCurve{0.5f}, mSatCurve{0.5f};
    bool  mAutoRelease{false};

    // state
    std::vector<float> mDelayL, mDelayR;
    int   mDelaySize{0}, mWritePos{0}, mAheadSamples{0};
    float mEnvFast{0}, mEnvSlow{0};
    float mAttackCoef{0}, mRelFastCoef{0}, mRelSlowCoef{0};

    // true-peak detector
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;
    // (or hand-rolled 4-tap FIR — decide during impl)

    // atomic meters
    std::atomic<float> mInputDb{-96}, mOutputDb{-96}, mGrDb{0};

    // helpers
    void recalcCoefs();
    void allocateDelay();
};
```

### Deferred (separate UI task — NOT part of this action item)
**⚠️ When building the Limiter editor panel, refer to the full UI/layout spec in `Files For Claude/DSP Review/Limiter.txt` (Sections 1 & 2).** That file has the complete three-zone layout, skeuomorphic look-and-feel specs, color palette, font notes, and exact knob groupings.

Key deferred UI items per Limiter.txt:
- **Three-zone layout:** Zone A (scrolling waveform visualizer, left) • Zone B (big knobs Gain/Ceiling/Sat, center) • Zone C (smaller knobs Attack/Release/Ahead/Curve, right)
- **Scrolling waveform display** — input signal (white/gray trace) + GR curve (cyan/blue) dipping from top + horizontal red Ceiling line, moving right→left
- **Skeuomorphic knob LAF** — brushed aluminum gradients, glow ring brightening with value (FabFilter/Valhalla quality target)
- **Glass panel effect** over visualizer (CRT/glass reflection overlay)
- **Color palette:** `#00FFF2` electric cyan for GR, `#FF9100` safety orange for saturation indicator, monospace digital-readout font for dB values
- **Class to create:** `EffectEditorPanels::LimiterPanel` — follows the pattern of other per-effect editor panels

---

## 6. OverdriveDSP — Four Additions
**File:** `Source/DSP/OverdriveDSP.h` + `.cpp`

### 6a. 4× oversampling around the shaper stage
`atan(100 × x)` at base sample rate aliases heavily; x100 mode makes it worse. Oversample only the nonlinearity (steps 2–3), not the filters.

**Implementation:**
- Add member: `std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;`
- In `prepare()`:
  ```cpp
  mOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
      2 /* numChannels */,
      2 /* factor: 2 stages × 2× each = 4× */,
      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR /* low latency */,
      true /* maxQuality */);
  mOversampler->initProcessing((size_t)maxBlockSize);
  mOversampler->reset();
  ```
- In `process()`: restructure so that the **BPF band** is written into a small temp buffer, that buffer is processed upsampled → shaper → downsampled, then recombined with residual and passed into the LPF. Rough shape:
  ```cpp
  // Per block, per channel:
  //   1. BPF per-sample → write band[n] to tempBand; residual[n] = x[n]-band[n]
  //   2. AudioBlock wrapper around tempBand → oversampler upsamples
  //   3. per upsampled sample: driven = atan(driveScale * s) / halfPi
  //   4. oversampler downsamples back → shaped[]
  //   5. per sample: out = shaped[n] + residual[n]; LPF(out); postGain; clip; wet/dry
  ```
- Note: `filterHalfBandPolyphaseIIR` adds a small **latency** (roughly 5–6 samples total at 2 stages). Report it:
  ```cpp
  int getLatencySamples() const override {
      return mOversampler ? (int)mOversampler->getLatencyInSamples() : 0;
  }
  ```
- In `reset()`: `if (mOversampler) mOversampler->reset();`
- **Stereo:** allocate the oversampler with 2 channels; process a stereo AudioBlock in one call (avoid per-channel processing if possible — the oversampler prefers block-based processing).

### 6b. 5 Hz DC-blocker before final output
Asymmetric `atan` drive at high gain accumulates DC; blocker is cheap and essential.

**Implementation:**
- Add state: `float mDcX_L{0}, mDcY_L{0}, mDcX_R{0}, mDcY_R{0};`
- Use the `R = 1 - 2π·5/sr` form (so it tracks sample rate at 96k/192k):
  ```cpp
  const float R = 1.0f - juce::MathConstants<float>::twoPi * 5.0f / (float)mSampleRate;
  // per sample (after hard clip, before wet/dry mix):
  float y = out - mDcX_L + R * mDcY_L;
  mDcX_L = out; mDcY_L = y; out = y;
  ```
- Compute `R` once per `prepare()` and cache as `mDcCoef`.
- Place **after** the hard safety clip, **before** the legacy wet/dry mix. (Hard clip shouldn't re-introduce DC; DC blocker after clip is safer.)
- Reset the four state vars in `reset()`.

### 6c. SmoothedValue on PreAmp / Color / PostFilter / PostGain
All four params can be user-automated. Current code recomputes biquad coefs on setter call → instant jump → click potential.

**Implementation:**
- Add: `juce::SmoothedValue<float> mPreAmpSmooth, mColorSmooth, mPostFilterSmooth, mPostGainSmooth;`
- In `prepare()`:
  ```cpp
  mPreAmpSmooth.reset(sampleRate, 0.015);    // 15 ms ramp
  mColorSmooth.reset(sampleRate, 0.015);
  mPostFilterSmooth.reset(sampleRate, 0.030); // filter cutoff needs slower ramp to avoid popping
  mPostGainSmooth.reset(sampleRate, 0.015);
  mPreAmpSmooth.setCurrentAndTargetValue(mPreAmp);
  // ... same for others
  ```
- In setters: also call `.setTargetValue(...)`. **Do NOT call `updateBPF()` / `updateLPF()` from the setters anymore** — move coefficient recomputation **into the process loop**, once per block (or once per N samples if CPU matters), driven by the smoothed values:
  ```cpp
  // in process(), before sample loop:
  float color     = mColorSmooth.getNextValue();      // peek, don't consume here
  float postFilt  = mPostFilterSmooth.getNextValue();
  // ... only recompute coefs if changed by >0.1 Hz to avoid wasted work
  ```
  Simpler: recompute coefs once at top of each `process()` using current smoothed-value target (not per-sample). That's the standard pattern and is sufficient for user automation.
- **PreAmp / PostGain** are scalar multiplies — consume per-sample from the smoother directly inside the loop. That's where SmoothedValue shines.
- Serialization: unchanged (smoothed values are transient).

### 6d. Swap biquads → `juce::dsp::StateVariableTPTFilter`
Only after 6c is in (so smoothing is already handled).

**Implementation:**
- Replace `Biquad mBPF_L/R, mLPF_L/R` with:
  ```cpp
  juce::dsp::StateVariableTPTFilter<float> mBPF;   // stereo (2ch)
  juce::dsp::StateVariableTPTFilter<float> mLPF;
  ```
- In `prepare()`: `mBPF.prepare({sampleRate, (juce::uint32)maxBlockSize, 2}); mBPF.setType(juce::dsp::StateVariableTPTFilterType::bandpass);` and same for `mLPF` with `lowpass`.
- Remove `updateBPF()` / `updateLPF()`. In each `process()` block:
  ```cpp
  const float bpfQ = 0.5f + (1.0f - mPreBand) * 9.5f;
  mBPF.setCutoffFrequency(mColorSmooth.getTargetValue());
  mBPF.setResonance(bpfQ);
  mLPF.setCutoffFrequency(mPostFilterSmooth.getTargetValue());
  mLPF.setResonance(0.7071f); // Butterworth
  ```
- Per-sample calls become `band = mBPF.processSample(ch, x)` and `out = mLPF.processSample(ch, out)`.
- Remove the `Biquad` struct, `setBiquadCoeffs()` helper, and coefficient math entirely.
- **Gotcha check (from CLAUDE.md):** verify `setResonance` takes Q directly (it does — JUCE docs confirm). Butterworth Q=0.7071 is correct for "transparent" LPF.
- In `reset()`: `mBPF.reset(); mLPF.reset();`

### Order of implementation
Do 6b (DC-blocker, trivial) + 6c (SmoothedValue, refactor setters) first. Then 6d (TPT swap — depends on 6c's move of coef recalc into process). Then 6a (oversampling — biggest change, isolated to the shaper stage).

---

## 7. PhaserDSP — Four Additions
**File:** `Source/DSP/PhaserDSP.h` + `.cpp`

### 7a. Logarithmic LFO → frequency mapping
Currently: `centerHz = minHz + lfo * (maxHz - minHz)` (linear). Human hearing is log-scaled; linear mapping makes the sweep spend too much perceived time in the high range.

**Fix (in `process()` sample loop):**
```cpp
// Replace:
float centerHzL = minHz + lfoL * (maxHz - minHz);
float centerHzR = minHz + lfoR * (maxHz - minHz);

// With:
const float logMinHz = std::log(minHz);
const float logMaxHz = std::log(maxHz);
float centerHzL = std::exp(logMinHz + lfoL * (logMaxHz - logMinHz));
float centerHzR = std::exp(logMinHz + lfoR * (logMaxHz - logMinHz));
// Equivalent: minHz * std::pow(maxHz/minHz, lfoL);
```
Hoist `logMinHz` and `logMaxHz` outside the sample loop (they only depend on per-block values). Keep the `jlimit` clamp.

### 7b. Always-allocated 24-stage buffer
Currently `setStages()` calls `assign(mNumStages, 0.0f)` which resizes AND zeroes all filter state → audible click whenever the user adjusts stage count. Review spec: allocate 24 stages once, iterate only `mNumStages` in the process loop.

**Implementation:**
- Change state vector allocation: in `prepare()` and `reset()`, size to **24** unconditionally:
  ```cpp
  mXL.assign(24, 0.0f);  mYL.assign(24, 0.0f);
  mXR.assign(24, 0.0f);  mYR.assign(24, 0.0f);
  ```
- Rewrite `setStages()`:
  ```cpp
  void PhaserDSP::setStages(int numStages) {
      mNumStages = juce::jlimit(1, 24, numStages);
      // Do NOT touch state vectors — they stay allocated at 24.
      // Do NOT reset feedback — changing count should be seamless.
  }
  ```
- In `process()`, the existing `for (int s = 0; s < mNumStages; ++s)` loop already does the right thing once vectors are size-24.
- **Edge case:** when stage count increases (say from 4 → 12), stages 4–11 will have stale state from whenever they were last active. Initial state is zero (from prepare/reset) → first activation is clean. After that, they persist. That's the review's intended behavior and matches hardware.
- Remove the stale-size check `if ((int)mXL.size() != mNumStages) reset();` from `process()` — no longer needed, and `reset()` inside `process()` was already a latent click bug.
- In `setStateInformation()`: also don't call `setStages(mNumStages)` if it would have resized — with this change, the setter is a pure int assignment, so the existing call is fine (no-op side effects).

### 7c. SmoothedValue on Rate, Feedback, MinDepth, MaxDepth
Prevent zipper/clicks on parameter automation.

**Implementation:**
- Add members:
  ```cpp
  juce::SmoothedValue<float> mRateSmooth, mFeedbackSmooth, mMinDepthSmooth, mMaxDepthSmooth;
  ```
- In `prepare()`:
  ```cpp
  mRateSmooth.reset(sampleRate, 0.020);
  mFeedbackSmooth.reset(sampleRate, 0.020);
  mMinDepthSmooth.reset(sampleRate, 0.030);   // filter freqs want a slightly slower ramp
  mMaxDepthSmooth.reset(sampleRate, 0.030);
  mRateSmooth.setCurrentAndTargetValue(mRate);
  mFeedbackSmooth.setCurrentAndTargetValue(mFeedback);
  mMinDepthSmooth.setCurrentAndTargetValue(mMinDepthHz);
  mMaxDepthSmooth.setCurrentAndTargetValue(mMaxDepthHz);
  ```
- In setters, also call `.setTargetValue(...)` (keep the stored field for serialization + `getCurrentTabIndex`-style queries):
  ```cpp
  void PhaserDSP::setRate(float hz) {
      mRate = juce::jlimit(0.05f, 10.0f, hz); mSweepHz = mRate;
      mRateSmooth.setTargetValue(mRate);
  }
  // ... same pattern for setFeedback, setMinDepth, setMaxDepth
  ```
- In `process()` sample loop, consume smoothed values per sample:
  ```cpp
  const float rate    = mRateSmooth.getNextValue();
  const float fb      = juce::jlimit(-1.2f, 1.2f, mFeedbackSmooth.getNextValue());
  const float minHzS  = juce::jlimit(10.0f, sr * 0.499f, mMinDepthSmooth.getNextValue());
  const float maxHzS  = juce::jlimit(minHzS, sr * 0.499f, mMaxDepthSmooth.getNextValue());
  // phaseInc becomes per-sample: phase += rate / sr
  ```
  Note `phaseInc` was hoisted outside the loop — now it's per-sample (negligible cost). Recompute `logMinHz`/`logMaxHz` per sample too (cheap — `std::log` is fast).

### 7d. Explicit InvertFeedback toggle
Aesthetics/discoverability improvement. Currently user must dial negative feedback manually.

**Implementation:**
- Add member: `bool mInvertFeedback { false };`
- Add setter: `void setInvertFeedback(bool on) { mInvertFeedback = on; }`
- In `process()`:
  ```cpp
  const float fbSigned = mInvertFeedback ? -fb : fb;
  // use fbSigned instead of fb in the "procL = inL + fb * mFbL" lines
  ```
- Serialize `mInvertFeedback` in get/setStateInformation (as int 0/1).
- **Interaction:** user can still dial negative `mFeedback` — combining both flips twice back to positive. That's fine and expected.

### Order of implementation
Do 7a (log mapping — 3 lines, sonically the biggest win) + 7b (always-allocated vectors — trivial refactor) first. Then 7c (SmoothedValue — requires moving per-block constants into the loop). Then 7d (invert toggle — trivial).

---

## 8. ReverbDSP — Two Enhancements
**File:** `Source/DSP/ReverbDSP.h` + `.cpp`

**Note:** the review spec was weaker than the existing 8×8 FDN. These are additive enhancements, not review-driven fixes.

### 8a. Subtle tail modulation (Valhalla-style liveliness)
Prevents the reverb tail from sounding "static" on sustained tones. Add a slow LFO that modulates each FDN delay-line read offset by ±0.5 ms.

**Implementation:**
- Add parameter: `float mTailModDepthMs { 0.3f };` (range 0–1.5 ms, default 0.3)
- Add parameter: `float mTailModRateHz  { 0.35f };` (range 0.05–2.0 Hz, default 0.35 — very slow)
- Add setter: `void setTailModDepth(float ms); void setTailModRate(float hz);`
- Add state: `std::array<float, kN> mTailModPhase {};` (per-line phase so lines don't modulate in lockstep)
- In `prepare()`: initialize each line's phase evenly distributed across 0..2π (so 8 lines sweep independently):
  ```cpp
  for (int i = 0; i < kN; ++i)
      mTailModPhase[i] = juce::MathConstants<float>::twoPi * (float)i / (float)kN;
  ```
- In `process()` sample loop, **replace the current FDN read** with a modulated-offset read using cubic interpolation (Catmull-Rom, same as Chorus/Delay/Flanger):
  ```cpp
  // Precompute per block:
  const float modPhaseInc = juce::MathConstants<float>::twoPi * mTailModRateHz / (float)mSampleRate;
  const float modDepthSmp = mTailModDepthMs * 0.001f * (float)mSampleRate;

  // In sample loop, per line:
  for (int i = 0; i < kN; ++i) {
      // Compute modulated read offset in samples
      const float modOffset = modDepthSmp * std::sin(mTailModPhase[i]);
      mTailModPhase[i] += modPhaseInc;
      if (mTailModPhase[i] >= juce::MathConstants<float>::twoPi)
          mTailModPhase[i] -= juce::MathConstants<float>::twoPi;

      // Read at (writePos - lineLen + modOffset) — i.e. the "oldest" sample, slightly modulated
      // Current code reads mFDNL[i][mFDNWr[i]] (the oldest sample = write position pre-increment).
      // New code: interpolate around that position with modOffset.
      const float readPos = (float)mFDNWr[i] + modOffset;  // small perturbation around oldest tap
      zL[i] = cubicInterpCircular(mFDNL[i], readPos, mFDNLen[i]);
      zR[i] = cubicInterpCircular(mFDNR[i], readPos, mFDNLen[i]);
  }
  ```
- Add static helper `cubicInterpCircular(const std::vector<float>& buf, float pos, int size)` — Catmull-Rom 4-point lift from `ChorusDSP::cubicInterp`. Shared utility would be nice but not required.
- **Mod depth of 0 must be exactly bypassed** — skip interp, read directly (preserves current zero-cost behavior when user doesn't want modulation).
- **Early-reflection taps (len/3) do NOT get modulated** — keep them as integer reads to preserve their crisp pre-echo character.
- Serialize `mTailModDepthMs` and `mTailModRateHz` in state (both versions: `ReverbDSP2` tag).
- `reset()`: zero `mTailModPhase`.

### 8b. Size-change click reduction
Currently `updateDelayLines()` reallocates `mFDNL[i]`/`mFDNR[i]` and zeros all state when `mRoomSize` changes. If user automates room-size, audible click.

**Implementation (simplest viable option):**
- **Option A (recommended):** allocate FDN buffers at MAX size (e.g., `RoomSize=2.0`, 2× max) once in `prepare()`. Never resize after. When size changes, only update `mFDNLen[i]` (the effective length used for wraparound). Read position uses `mFDNLen[i]`; writes wrap at `mFDNLen[i]`; unused buffer tail stays allocated but unused.
  ```cpp
  // In prepare():
  const float maxScale = 2.0f / 0.6f * (float)(sampleRate / 44100.0);
  for (int i = 0; i < kN; ++i) {
      const int maxLen = (int)((float)kPrimes[i] * maxScale) + 4;
      mFDNL[i].assign(maxLen, 0.0f);
      mFDNR[i].assign(maxLen, 0.0f);
  }
  // In setRoomSize() / updateDelayLines():
  // Compute newLen; if newLen > buffer size, clamp. Do NOT resize the buffer.
  // Do NOT zero state — running tail keeps decaying naturally.
  // If newLen decreases, write pointer may now be past the end — wrap it:
  //     mFDNWr[i] = mFDNWr[i] % mFDNLen[i];
  ```
- Remove the `mLPStL[i] = 0.0f;` / `mBassStL[i] = 0.0f;` lines in `updateDelayLines()` — filter state should persist through size changes.
- **Gotcha:** when shrinking size, samples in positions `[newLen..oldLen)` are now unreachable stale data. That's fine — they won't be read from. They'll naturally get overwritten when the write pointer cycles.
- **Gotcha:** when growing size (from previously smaller), samples in `[oldLen..newLen)` contain **zeros** (from initial allocation) — that's fine, reads from those positions will inject silence for one cycle, which is imperceptible compared to a full-state click.
- Update `updateFeedback()` to recompute `mFeedGain[i]` using the **new** `mFDNLen[i]` — already does this, so no change there.

**Apply 8a first** (genuine sonic upgrade), then 8b (click reduction — less user-facing but cleaner).

---

## 9. SaturationDSP — Four Additions
**File:** `Source/DSP/SaturationDSP.h` + `.cpp`

### 9a. 4× oversampling around the tube engine
Same pattern as Overdrive 6a. The tanh/atan/cubic shapers in `processTube()` alias heavily at 44.1/48k when drive is high.

**Implementation:**
- Add member: `std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;`
- In `prepare()`:
  ```cpp
  mOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
      2 /* numChannels */,
      2 /* factor: 2 stages × 2× each = 4× total */,
      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,  // low-latency
      true /* maxQuality */);
  mOversampler->initProcessing((size_t)maxBlockSize);
  mOversampler->reset();
  ```
- **Scope of oversampling:** wrap **only the tube engine and DC-block stage** — steps 4 (tube) + 5 (DC block after tube). Keep tone shelves, bass split, wet/dry, and output gain at base rate (they're linear filters, don't alias).
- Restructure `process()`:
  ```cpp
  // Per block, per channel:
  //  Steps 1–3 (sensitivity → tone pre → bass split) run at base rate per sample,
  //  producing a `high` band. Collect `high[]` into a small AudioBlock across the block.
  //  Oversample `high[]` 4× → upsampled.
  //  For each upsampled sample: h = processTube(h); DC-block h.
  //  Downsample back → processedHigh[].
  //  Continue at base rate: recombine with low band, tone post, wet/dry, out gain.
  ```
  This means restructuring the current per-sample single-pass loop into a two-pass pattern: base-rate pre-loop collects `high[n]`, oversampler processes the block, base-rate post-loop consumes `processedHigh[n]` and finishes. Requires a scratch `juce::AudioBuffer<float>` or two `std::vector<float>` scratches of length `maxBlockSize`.
- **Low band path:** the low band is also run through `processTube()` (when BassRelief < 100%). This creates asymmetric latency between bands → comb-filter issue if we oversample only one. **Solution:** oversample the tube engine and run BOTH low and high bands through it during the oversampled phase. Adjust scratch buffers accordingly (two channels per band, or interleave).
  - Simpler alternative: skip BassRelief's tube-on-lows feature when oversampling is active (treat relief=100% always for lows). Loses a feature.
  - Best: oversample the full tube path (low + high both go through the oversampled block). The pre-oversampling stage splits into low/high; post-oversampling stage recombines and applies relief blend.
- Override `getLatencySamples()` to return `mOversampler->getLatencyInSamples()` for PDC.
- In `reset()`: `if (mOversampler) mOversampler->reset();`

### 9b. Auto-gain compensation toggle
Decouple drive amount from output volume.

**Implementation:**
- Add member: `bool mAutoGain { false };`
- Add setter: `void setAutoGain(bool on) { mAutoGain = on; }`
- In `process()`, compute compensation gain once per block:
  ```cpp
  const float sensGain = juce::Decibels::decibelsToGain(mSensitivity);
  const float autoComp = mAutoGain ? (1.0f / std::max(0.001f, sensGain)) : 1.0f;
  const float outGain  = juce::Decibels::decibelsToGain(mOut) * autoComp;
  ```
  (Multiply `autoComp` into `outGain` so only one multiply per sample in the loop.)
- Serialize `mAutoGain` in ValueTree state.
- Editor panel (existing `SaturationPanel`) gets a new toggle — **that's editor work, not DSP-only**, so just expose the setter; editor update is out of scope for this action item.

### 9c. Sample-rate-aware DC blocker
Replace hardcoded `kDCR = 0.9975f` with a computed coefficient targeting 5 Hz cutoff at any sample rate.

**Implementation:**
- Remove `static constexpr float kDCR = 0.9975f;` from header.
- Add member: `float mDCCoef { 0.9995f };`
- In `updateFilters()` (or `prepare()`): `mDCCoef = 1.0f - juce::MathConstants<float>::twoPi * 5.0f / (float)mSampleRate;`
- In `process()`: use `mDCCoef` in place of `kDCR`:
  ```cpp
  tube_out = dc_in - dcX + mDCCoef * dcY;
  ```
- `prepare()` already calls `updateFilters()`, so this picks up new sample rate automatically.

### 9d. SmoothedValue on Flowers, Dabs, Sensitivity, Out
Zipper-noise prevention on automation.

**Implementation:**
- Add members: `juce::SmoothedValue<float> mFlowersSmooth, mDabsSmooth, mSensitivitySmooth, mOutSmooth;`
- In `prepare()`:
  ```cpp
  mFlowersSmooth.reset(sampleRate, 0.015);
  mDabsSmooth.reset(sampleRate, 0.015);
  mSensitivitySmooth.reset(sampleRate, 0.020);
  mOutSmooth.reset(sampleRate, 0.015);
  mFlowersSmooth.setCurrentAndTargetValue(mFlowers);
  // ... same pattern for others
  ```
- In setters: also call `.setTargetValue(...)` alongside the existing member assignment.
- In `process()` sample loop (or per-block if N samples with minor zipper is acceptable):
  - Per-sample consume: `const float flowersNow = mFlowersSmooth.getNextValue();` etc.
  - `processTube()` currently reads `mFlowers` and `mDabs` directly as member access. Refactor to take them as parameters: `float processTube(float x, float flowers, float dabs) const noexcept;` — then pass the smoothed per-sample values in.
  - Update the single call site to pass the new args.
- **Sensitivity:** per-sample smoothed value goes into `sensGain = dbToGain(smoothedSens)` — but computing `dbToGain` per sample is expensive. Alternative: smooth `sensGain` in linear domain directly (smooth over `dbToGain(mSensitivity)`). Same pattern for `mOut`.
- Serialization: unchanged.

### Order of implementation
Do 9c (trivial) + 9d (smoothing) first. Then 9b (auto-gain — 1-line logic change). Then 9a (oversampling — biggest refactor, needs scratch buffers and restructure of the sample loop). After 9a, verify `getLatencySamples()` properly propagates through `EffectRack` PDC.

---

## 10. TapeDSP — Full Rework (Biggest Single Change in Review Set)
**File:** `Source/DSP/TapeDSP.h` + `.cpp`

**Scope:** the current TapeDSP is the weakest module of the set — it's effectively `tanh + LP + wobble + white noise`. Real tape needs hysteresis memory, flutter, pre/de-emphasis, and pink hiss. This is a near-total rewrite.

### 10a. Hysteresis state variable
Replace stateless `tanh(sat·x)` with a 1-pole memory accumulator that drifts toward the shaped value:
```cpp
// Per channel:
float shaped = asymShaper(x);
mHystL = mHystL + mHystAlpha * (shaped - mHystL);   // or _R for right
float satOut = mHystL;
```
- Add members: `float mHystL{0}, mHystR{0}; float mHystAlpha{0.5f};`
- Add parameter: `float mTapeSpeed { 0.5f };` (0 = slow/squishy, 1 = fast/clean)
- `mHystAlpha = 0.2f + 0.8f * mTapeSpeed` — at speed 0, α=0.2 (strong memory, 5-sample time constant); at speed 1, α=1.0 (no memory, instantaneous).
- Expose `setTapeSpeed(float)` setter, serialize, add to SmoothedValue group (10i).

### 10b. Asymmetric sigmoid shaper
Replace current `tanh(satGain·x)` with the review's formula:
```cpp
static float asymShaper(float x, float k) {
    const float sgn = (x >= 0.0f) ? 1.0f : -1.0f;
    return (x + k * x * x * sgn) / (1.0f + std::abs(x));
}
```
- `k` driven by `mVibe` or a dedicated `mAsymmetry` param (expose both? simplest: `k = 0.3f * mVibe`). Keep it tied to `mVibe` for single-knob simplicity.
- Replace `std::tanh(satGain * inL)` with `asymShaper(satGain * inL, 0.3f * mVibe)`.

### 10c. 4× oversampling around hysteresis + shaper
Same pattern as Overdrive 6a / Saturation 9a.
- Add `std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;`
- `filterHalfBandPolyphaseIIR`, 2 stages × 2× = 4×.
- Oversample **only steps 1 + 2** (input gain → shaper → hysteresis). LP, wow/flutter, hiss, out gain stay at base rate.
- **Hysteresis state inside oversampling:** `mHystL/R` runs at the oversampled rate (not base rate). Adjust `mHystAlpha` computation: `α_os = α_base / 4` (approximately — the time constant should be equivalent to 4× the sample count at base rate). Actually — simpler: compute `α` based on the effective sample rate inside the oversampled block. Cache `α_effective = f(mTapeSpeed, sampleRate * 4)` in `prepare()` / when speed changes.
- Override `getLatencySamples()` to return `mOversampler->getLatencyInSamples()`.
- `reset()`: reset oversampler + zero hysteresis state.

### 10d. Separate Flutter LFO + smoothed noise
Current code has one wow LFO. Add flutter as a second modulation source summed with wow.
- Add parameters: `float mFlutterRate { 15.0f };` (10–20 Hz), `float mFlutterDepth { 0.2f };` (0–1, typically much shallower than wow)
- Add members: `double mFlutterPhase { 0.0 };  float mFlutterNoiseState { 0.0f };`
- In the sample loop:
  ```cpp
  // Wow (existing):
  float wowLfo = (float)std::sin(twoPi * mWowPhase);
  mWowPhase = std::fmod(mWowPhase + mWowRate/mSampleRate, 1.0);

  // Flutter: sine + smoothed white noise
  float flutterSine = (float)std::sin(twoPi * mFlutterPhase);
  mFlutterPhase = std::fmod(mFlutterPhase + mFlutterRate/mSampleRate, 1.0);

  float rawNoise = mRandom.nextFloat() * 2.0f - 1.0f;
  mFlutterNoiseState = 0.98f * mFlutterNoiseState + 0.02f * rawNoise;  // ~300 Hz LP smoothing
  float flutter = 0.8f * flutterSine + 0.2f * mFlutterNoiseState;

  // Total modulation:
  float totalModSmp = maxWowSamples * (mWowDepth * wowLfo + mFlutterDepth * flutter * 0.3f);
  ```
  (Flutter amplitude is scaled down 0.3× since flutter in real tape is usually much shallower than wow.)
- Setters: `setFlutterRate(float hz)`, `setFlutterDepth(float d)`.
- Serialize both.
- Note: flutter at 15 Hz with any meaningful depth (±0.3 ms) is audible as a "shimmer" — that's correct tape behavior. Keep default `mFlutterDepth` low (0.1–0.2).

### 10e. Cubic interpolation on wow/flutter delay reads
Replace the current linear interp (lines 100–104 in `process()`) with Catmull-Rom cubic — same formula as Chorus/Delay/Flanger cubic interp helpers.
- Add static `cubicInterpCircular(buf, writePos, intOffset, frac, size)` helper (or copy from ChorusDSP).
- Call site change: replace the linear `buf[r0] + frac*(buf[r1]-buf[r0])` with 4-point cubic read.

### 10f. Pre/de-emphasis filter pair (replaces current LP-only warmth)
Current `updateLPCoef()` rolls off everything above 4–8 kHz → dull. Review wants the distortion to happen *in* the highs, then roll them back afterward.
- Remove the current single LP (`mLPCoef`, `mLPStateL/R`, and its per-sample filtering after saturation).
- Add two 1-pole high shelves using the "LP blend" formula already used in `SaturationDSP::updateFilters`:
  ```cpp
  // Pre-emphasis: +6 dB high shelf @ 5 kHz, BEFORE saturation (inside oversampled block? or before? — simpler: before)
  // De-emphasis: -8 dB high shelf @ 4 kHz, AFTER saturation
  ```
- Add members: `float mPreShelfCoef{0}, mDeShelfCoef{0};` + per-channel LP states for each shelf.
- Add: `float mPreShelfGainLin{1.0f}`, `float mDeShelfGainLin{1.0f};`
- In `updateFilters()` (rename from `updateLPCoef()`):
  ```cpp
  mPreShelfCoef = std::exp(-twoPi * 5000.0f / sr);
  mDeShelfCoef  = std::exp(-twoPi * 4000.0f / sr);
  mPreShelfGainLin = juce::Decibels::decibelsToGain(+6.0f);
  mDeShelfGainLin  = juce::Decibels::decibelsToGain(-8.0f);
  ```
- Per-sample shelf processing:
  ```cpp
  // Pre-emphasis: shelf_out = G·x + (1−G)·LP(x)
  preLP = (1 - mPreShelfCoef) * x + mPreShelfCoef * preLP;
  x = mPreShelfGainLin * x + (1 - mPreShelfGainLin) * preLP;
  // ... saturation ...
  // De-emphasis (same structure, different coef/gain):
  deLP = (1 - mDeShelfCoef) * x + mDeShelfCoef * deLP;
  x = mDeShelfGainLin * x + (1 - mDeShelfGainLin) * deLP;
  ```
- The emphasis pair replaces the bandwidth-limiting role of the old LP. User's `mVibe` knob can still modulate shelf gain if desired — default is fine.

### 10g. Pink-filtered hiss with 200 Hz HPF
Current hiss is raw white noise. Replace with pink+HPF200 for "ssshhh" character.
- Add pink-noise filter state. The classic Paul Kellet pink filter:
  ```cpp
  // Per channel, 7-stage running pink filter:
  struct PinkState {
      float b0{0}, b1{0}, b2{0}, b3{0}, b4{0}, b5{0}, b6{0};
      float process(float white) {
          b0 = 0.99886f * b0 + white * 0.0555179f;
          b1 = 0.99332f * b1 + white * 0.0750759f;
          b2 = 0.96900f * b2 + white * 0.1538520f;
          b3 = 0.86650f * b3 + white * 0.3104856f;
          b4 = 0.55000f * b4 + white * 0.5329522f;
          b5 = -0.7616f * b5 - white * 0.0168980f;
          float pink = b0+b1+b2+b3+b4+b5+b6 + white*0.5362f;
          b6 = white * 0.115926f;
          return pink * 0.11f;  // normalize
      }
  };
  PinkState mPinkL, mPinkR;
  ```
- Follow with 1-pole HPF @ 200 Hz: `float hpHz = 200.0f; mHissHpCoef = std::exp(-twoPi * hpHz / sr);`
- Add HPF state members: `float mHissHPx_L{0}, mHissHPy_L{0}, mHissHPx_R{0}, mHissHPy_R{0};`
- Per sample:
  ```cpp
  float whiteL = mRandom.nextFloat() * 2.0f - 1.0f;
  float pinkL = mPinkL.process(whiteL);
  // 1-pole HPF @ 200 Hz:  y = a*(y_prev + x - x_prev)
  float hpL = mHissHpCoef * (mHissHPy_L + pinkL - mHissHPx_L);
  mHissHPx_L = pinkL; mHissHPy_L = hpL;
  wowL += mHiss * hpL * 0.1f;  // scale down — pink+HP output is already normalized-ish
  ```
- Do the same independently for R (two separate `PinkState` → natural stereo decorrelation).

### 10h. 5 Hz DC blocker before final output
Same as Saturation 9c.
- Add members: `float mDcX_L{0}, mDcY_L{0}, mDcX_R{0}, mDcY_R{0}; float mDcCoef{0};`
- `prepare()`: `mDcCoef = 1.0f - twoPi * 5.0f / sr;`
- Per sample, immediately before `L[n] = ... * mOutputGain`:
  ```cpp
  float dcInL = wowL;
  wowL = dcInL - mDcX_L + mDcCoef * mDcY_L;
  mDcX_L = dcInL; mDcY_L = wowL;
  ```

### 10i. SmoothedValue on all user params
Zipper-noise prevention.
- Members: `juce::SmoothedValue<float> mVibeSmooth, mTapeSpeedSmooth, mWowDepthSmooth, mFlutterDepthSmooth, mInputGainSmooth, mOutputGainSmooth, mHissSmooth;`
- `prepare()`: reset each with 15–20 ms ramp, `setCurrentAndTargetValue(...)`.
- Setters: call `.setTargetValue(...)` alongside existing field assignment.
- In sample loop, consume per-sample (or at minimum per-block — per-sample for gain parameters, per-block for `vibe` and `tapeSpeed` is fine).

### 10j. (Deferred — skip for now) IR-based cassette frequency profile
Review lists `juce::dsp::Convolution` with a cassette IR as the "Pro" alternative to the emphasis pair. **Skip this** — the pre/de-emphasis pair (10f) gets ~90% of the tonal character with zero IR curation work. Revisit only if Jeff later wants specific cassette-brand profiles (Type I / Type II / specific deck models).

### Order of implementation
1. **10h** (DC blocker — trivial, applies to any future rewrite)
2. **10g** (pink hiss + HPF 200 — self-contained, replaces existing noise line)
3. **10f** (pre/de-emphasis pair — replaces the current LP; biggest tonal shift)
4. **10b** (asymmetric shaper — 10-line function swap)
5. **10a** (hysteresis state — adds memory to shaper)
6. **10d** (flutter LFO — adds to existing wow modulation)
7. **10e** (cubic interp — replaces linear interp)
8. **10i** (SmoothedValue on all params — mechanical refactor)
9. **10c** (oversampling — biggest refactor, needs scratch buffers and loop restructure)

Each step is independently testable — after each, the tape should sound incrementally more tape-like. 10c is the last and biggest step, worth doing after the rest is stable.

---

## 11. TransientShaperDSP — Five Additions
**File:** `Source/DSP/TransientShaperDSP.h` + `.cpp`

### 11a. Quadratic attack + sustain curves
Current linear `gain = 1 + attack * transient + sustain * sustainClamp` feels mushy. Quadratic feels snappier and is the review's specific recommendation.

**Implementation:**
Change the gain computation in `process()`:
```cpp
// Existing:
float gain = 1.0f + attack * transientClamp + sustain * sustainClamp;

// New:
const float tSq = transientClamp * transientClamp;   // quadratic
const float sSq = sustainClamp   * sustainClamp;
float gain = 1.0f + attack * tSq + sustain * sSq;
```
- **Bipolar preserved:** `attack` and `sustain` are -1..1; when negative, the squared envelope still produces the right sign in the product. No additional logic needed.
- **Alternative:** for even more "snap" feel, use `std::pow(transientClamp, 2.0f)` — same as squared but cleaner to read. Stick with `*` for performance.

### 11b. Linkwitz-Riley 4th-order crossover
Replace the current 1-pole LP split with JUCE's LR filter for phase-perfect recombination.

**Implementation:**
- Replace `float mLPStateL, mLPStateR;` with:
  ```cpp
  juce::dsp::LinkwitzRileyFilter<float> mLR_LP, mLR_HP;
  ```
  (LR4 = 2nd-order Butterworth cascaded with itself = 24 dB/oct)
- In `prepare()`:
  ```cpp
  juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)maxBlockSize, 2 };
  mLR_LP.prepare(spec);  mLR_LP.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
  mLR_HP.prepare(spec);  mLR_HP.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
  mLR_LP.setCutoffFrequency(mSplitFreq);
  mLR_HP.setCutoffFrequency(mSplitFreq);
  ```
- In `setSplitFreq()`: also call `mLR_LP.setCutoffFrequency(mSplitFreq); mLR_HP.setCutoffFrequency(mSplitFreq);`
- In `process()` sample loop, replace the manual 1-pole LP:
  ```cpp
  // Old:  lpState += lpAlpha * (samp - lpState); lowBand = lpState; highBand = samp - lowBand;
  // New (per channel):
  float lowBand  = mLR_LP.processSample(ch, samp);
  float highBand = mLR_HP.processSample(ch, samp);
  // Note: LR4 design ensures low + high ≈ samp at all freqs (no magnitude hole at crossover)
  ```
- **Gotcha:** JUCE's `LinkwitzRileyFilter` introduces **latency** = filter group delay. For a 4th-order at 260 Hz this is ~1–2 ms at 48k. Report via `getLatencySamples()` — but LR4's group delay varies with frequency, so treat as 0 for PDC purposes (not worth the complexity for a drum tool where a few ms is imperceptible).
- In `reset()`: `mLR_LP.reset(); mLR_HP.reset();`
- **Remove:** the `mLPStateL/R` members and the `lpAlpha` computation.

### 11c. 4× oversampling around the drive stage
Current `tanh(d·x)/d` aliases when `drive > 0`. Oversample only the drive stage.

**Implementation:**
- Add `std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;`
- In `prepare()`: same setup as other oversampling additions (factor=2, stages=2, polyphase IIR).
- In `process()`, restructure so the drive stage is a separate pass:
  - Option 1 (simplest): accumulate the per-sample post-gain, post-split output into a scratch `AudioBuffer<float>` across the block, then run the oversampler→drive→downsampler on the full scratch in one shot, then apply `* outLin`.
  - Option 2 (in-place): process current per-sample logic up through band-recombine, write to the buffer, then in a second pass run the oversampler+drive over the written buffer, then a third pass applies output gain. Fewer scratch buffers, two loops.
- Either way, drive must be **inside** the oversampled block; the band-split and gain computation stay at base rate.
- Override `getLatencySamples()` to return `mOversampler->getLatencyInSamples()`.
- In `reset()`: `if (mOversampler) mOversampler->reset();`
- **Skip oversampling when `mDrive == 0`** — pass audio through unchanged, report 0 latency. Bypass is crucial to avoid paying latency cost when the user hasn't dialed in drive. **Important:** changing `mDrive` from 0 → nonzero changes latency mid-session. Either (i) always report the oversampler's latency regardless of mDrive (accept constant latency when drive==0), or (ii) don't wire up the oversampler at all when drive is 0 and live with a small glitch on drive crossing 0. Option (i) is cleaner for the host.

### 11d. SmoothedValue on Attack, Sustain, Sensitivity, SplitFreq, Drive, Gain
Zipper-noise prevention.

**Implementation:**
- Members:
  ```cpp
  juce::SmoothedValue<float> mAttackSmooth, mSustainSmooth, mSensitivitySmooth,
                             mSplitFreqSmooth, mDriveSmooth, mGainSmooth;
  ```
- In `prepare()`: `.reset(sampleRate, 0.015)` on each; `setCurrentAndTargetValue(...)` to initial values.
- In setters: call `.setTargetValue(...)` alongside existing field assignments.
- In `process()` sample loop, replace direct reads with `.getNextValue()`:
  ```cpp
  const float attack    = mAttackSmooth.getNextValue();
  const float sustain   = mSustainSmooth.getNextValue();
  const float sensScale = 1.0f + mSensitivitySmooth.getNextValue() * 9.0f;
  // ...
  ```
- **SplitFreq:** the smoothed value feeds `mLR_LP.setCutoffFrequency(...)` once per block (or per sample if you want it truly smooth — per-block is fine since filter setter is cheap).
- **Gain (outLin):** smooth the linear gain directly (don't smooth dB and convert per-sample — too expensive). Compute `outLin` in setter: `mOutLinTarget = dbToGain(mOutGainDb); mOutLinSmooth.setTargetValue(mOutLinTarget);`.
- Serialization: unchanged (smoothed values are transient).

### 11e. Slow envelope uses RMS detector (review's exact spec)
Current dual-envelope uses peak detection for both. Review specifies fast=peak, slow=**RMS**. RMS on the slow follower discriminates program level better than peak on complex material.

**Implementation:**
- Current level line: `level = avg(|samples|)` → one mono peak level per sample.
- **Fast envelope** keeps using `level` (peak-like).
- **Slow envelope** now tracks `level²` (mean-square), converted back to RMS for comparison:
  ```cpp
  // Per sample:
  float level     = /* existing: avg of |samples| across channels */;
  float levelSq   = level * level;

  // Fast envelope — peak follower (existing logic)
  float fastCoef = (level > mFastL) ? fastAtt : fastRel;
  mFastL = fastCoef * mFastL + (1.0f - fastCoef) * level;

  // Slow envelope — RMS follower (new)
  mSlowMs = slowCoef * mSlowMs + (1.0f - slowCoef) * levelSq;  // running mean-square
  //  (where slowCoef uses the existing slowAtt/slowRel selection)
  float slowRms = std::sqrt(std::max(mSlowMs, 1e-12f));

  // Transient (existing — keep as difference, not ratio)
  float transient = mFastL - slowRms;
  ```
- Replace `mSlowL` (peak envelope of level) with `mSlowMs` (running mean-square).
- Replace `slowCoef = (level > mSlowL)` attack-vs-release selector with `slowCoef = (levelSq > mSlowMs)`.
- Replace `sustainClamp   = juce::jlimit (0.0f, 1.0f, mSlowL * sensScale);` with `sustainClamp = juce::jlimit(0.0f, 1.0f, slowRms * sensScale);`.
- **Stick with difference (`fast - slowRms`), not ratio.** Review prefers ratio but difference is easier to tune and matches current behavior — keep the stylistic choice we already made.
- Serialization: no change (envelope states are transient).
- **Remove:** `mSlowR = mSlowL;` at end of process — replace with `mSlowMs` state (single mono value, no R counterpart needed).

### Order of implementation
1. **11a** (quadratic curves — 1-line change)
2. **11e** (RMS slow envelope — member rename + sqrt addition)
3. **11b** (LR4 crossover — filter swap, contained)
4. **11d** (SmoothedValue — mechanical)
5. **11c** (oversampling — biggest, last)

---

## 12. EQ8DSP — Ten Additions (incl. Full Dynamic EQ)
**Files:** `Source/DSP/EQ8DSP.h` + `.cpp` (DSP) • `Source/Standalone/SharedUI.h/.cpp` (ParametricEQDisplay UI) • possibly new `Source/DSP/EQ8DynamicDSP.h/.cpp` helper

**⚠️ Band count confirmed: keeping 8 bands (`kNumBands = 8`).** The review spec called for 7 bands; we already have 8, which is more capable. All EQ enhancements below — including Dynamic EQ (§12j) with its 7 new per-band params — apply to all 8 bands. The `kNumBands` constant does not change.

### 12a. `getMagnitudeForFrequency(float freq) const` method
Essential for UI curve drawing. Returns combined linear magnitude (or dB) at a given frequency, computed by multiplying magnitudes of every active section of every enabled band.

**Implementation:**
- Add public method: `float getMagnitudeForFrequency(float freq) const noexcept;`
- Also add convenience: `float getMagnitudeForFrequencyDb(float freq) const noexcept { return juce::Decibels::gainToDecibels(getMagnitudeForFrequency(freq)); }`
- Iterate over `mBands`: for each band that passes `bandShouldProcess(i)`, for each active section, multiply in `filter.coefficients->getMagnitudeForFrequency(freq, mSampleRate)`.
- Multiply final result by `dbToGain(mMainLevelDb)` so the main-level trim is included.
- Thread safety: method is `const` and reads shared coefficient pointers — guard against `coefficients == nullptr` (OFF type, or mid-recompute).
- **Cost:** O(numBands × numSections) per query. Called from editor timer at ~60 Hz for ~200 X-pixels → 200 calls/frame. Fine on CPU.

### 12b. Proportional Q on Peaking bands
Q narrows as gain rises (SSL/Neve "hardware" feel). Formula:
```cpp
// Only for type 0 (Peaking) — NOT shelves or filters
float propQ = userQ;
if (type == 0) {
    float gainFactor = 1.0f + std::abs(gainDb) / 18.0f;  // ±18 dB → up to 2× narrower
    propQ = userQ * gainFactor;
}
```
- Add `bool mProportionalQ { true };` member + setter `setProportionalQ(bool)` — expose as user toggle (some users prefer fixed Q).
- Apply in `makeOneSection()` (or in `updateBand()` before the section loop). Pass `propQ` in place of `qv` for Peaking sections.
- Shelves/filters/notch/bandpass get `userQ` unchanged — only Peaking is affected.
- Serialize `mProportionalQ`.

### 12c. `SmoothedValue` on freq / gain / Q per band
Currently setters call `updateBand()` → instant coefficient rebuild → click on automation.

**Implementation:**
- Per band, add three `juce::SmoothedValue<float> freqSmooth, gainSmooth, qSmooth;`
- In `prepare()`: `.reset(sampleRate, rampSec); .setCurrentAndTargetValue(...)`. Ramp time driven by `mIIRModSpeed` (see 12d).
- In setters (`setBandFreq/Gain/Q`), call `.setTargetValue(...)` and mark `mBandDirty[i] = true;` — but **do NOT call `updateBand()` immediately.** Remove the immediate rebuild from setters.
- In `process()` at the top of each block, for each dirty band:
  - Consume one step of each smoother (block-rate, not per-sample — biquad coefs can't change per sample without popping anyway)
  - If any of freq/gain/Q changed since last update OR `mBandDirty[i]`: call `updateBand(i)` with the current smoothed values, clear dirty flag
- Shelves and notches also benefit from this.
- **Gotcha:** slopes (multi-section bands) rebuild all sections — ensure block-rate (not per-sample) recompute to keep CPU sane.

### 12d. Wire `mIIRModSpeed` as smoothing-time parameter
Currently `mIIRModSpeed` is a setter with stored value but **never read anywhere** — dead parameter (same latent bug as Delay's `mModCutoffMod`).

**Wire it as the smoothing ramp length:**
- `mIIRModSpeed ∈ [0, 1]`: 0 = instant (no smoothing, ~1 ms), 1 = slow (~50 ms ramp)
- In `prepare()` and when `setIIRModSpeed()` is called:
  ```cpp
  float rampSec = 0.001f + mIIRModSpeed * 0.049f;   // 1..50 ms
  for (auto& bs : mBands) {
      bs.freqSmooth.reset(sampleRate, rampSec);
      bs.gainSmooth.reset(sampleRate, rampSec);
      bs.qSmooth.reset(sampleRate, rampSec);
  }
  ```
- Keeps backward compatibility — param was already serialized.
- Default 1.0 (current value) → 50 ms ramp, conservative "silent automation" feel.

### 12e. TPT filter swap (StateVariableTPT)
Replace `juce::dsp::IIR::Filter<float>` with `juce::dsp::StateVariableTPTFilter<float>` for all **single-section** filter types (LP, HP, Bell/Peak can map to TPT's bandpass+eq form, shelves are tricky).

**Scope limits — TPT doesn't cleanly support all filter types:**
- **Works natively in TPT:** LP, HP, BP, Notch, AllPass
- **Doesn't map cleanly to TPT:** Bell/Peaking (with gain), LowShelf, HighShelf — these need biquad or a TPT-based Zavalishin EQ topology (separate implementation)
- **Practical approach:** use **hybrid** — TPT for LP/HP/BP/Notch, keep biquad for Peaking/Shelf/Tilt. Most EQs are used for shelves/bells, so biquad stays the workhorse; TPT is a drop-in for the filter types it handles well.
- **Alternative:** implement Zavalishin's "The Art of VA Filter Design" Ch.5 TPT shelf/bell topology by hand. More work, fully TPT. Defer unless we hit popping with biquad.

**Implementation (hybrid approach):**
- Add to `BandState`: `juce::dsp::StateVariableTPTFilter<float> tptL, tptR;` (prepared per band)
- In `updateBand()`, for types 1/2/5/7 (LP/HP/Notch/BP): configure TPT (`setType`, `setCutoffFrequency`, `setResonance`), mark `useTPT[i] = true`
- For types 0/3/4/6/8 (Peak/Shelves/OFF/Tilt): keep biquad, `useTPT[i] = false`
- In `process()` sample loop, per band: branch on `useTPT[i]` — call `tptL.processSample(0, x)` or the existing biquad chain.

### 12f. Anti-cramping (2× oversampling for high-frequency bands)
Bands whose center freq is above ~8 kHz get asymmetric bell curves because of bilinear-transform warping near Nyquist. Orfanidis is the textbook fix but complex to implement; 2× oversampling the whole EQ is simpler and industry-standard (FabFilter Pro-Q 3 "Natural Phase" effectively does this).

**Implementation (2× oversampling path):**
- Add `std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler;`
- Add `bool mAntiCramping { false };` — off by default (opt-in to avoid latency surprise for existing users)
- Add setter `setAntiCramping(bool)`
- In `prepare()`:
  ```cpp
  mOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
      2, 1 /* 2× single stage */,
      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
      true);
  mOversampler->initProcessing((size_t)maxBlockSize);
  ```
- In `process()`: when `mAntiCramping == true`, wrap the full band-processing loop in `mOversampler->processSamplesUp(...)` / `processSamplesDown(...)`. Filter coefficient computations must use **`2 × sampleRate`** when anti-cramping is on (so the filter is designed for the oversampled rate).
- Override `getLatencySamples()` returning `mAntiCramping ? mOversampler->getLatencyInSamples() : 0`.
- **CPU cost:** 2× when enabled. User-toggleable.
- **Alternative for later:** hand-rolled Orfanidis coefficient design — corrects the warping analytically with zero latency. Deferred as future work.

### 12g. Linear-phase / HQ mode implementation (fill in the stubs)
Currently `mPhaseMode` enum exists with Standard / Linear / HQPlus / HQL / HQE values; `setPhaseMode()` stores it but `process()` always uses IIR.

**Linear-phase implementation:**
- Linear phase = FFT convolution with the EQ's impulse response. JUCE provides `juce::dsp::Convolution` but rebuilding the IR whenever a band changes is expensive.
- **Standard approach:** compute the EQ's impulse response offline (IFFT of the frequency response obtained by sampling `getMagnitudeForFrequency()` across the spectrum), reload into the convolution engine each time a band changes. Use `mLinearPrec` (already exists, 0..4) to pick FFT size 256/512/1024/2048/4096.
- Adds significant latency: FFT_size / 2 samples. At 2048 = 23 ms at 48k. Pro-Q 3 and FabFilter EQs do exactly this. Report via `getLatencySamples()`.
- **HQ modes** (HQPlus, HQL, HQE): these are FabFilter's labels for "higher quality at higher CPU" — in practice they map to different oversampling factors. Implement:
  - `HQPlus` = 2× oversampling (same as `mAntiCramping == true` but forced on)
  - `HQL` = Linear phase with large FFT (4096)
  - `HQE` = Linear phase with economical FFT (512)
- **Scope:** this is **substantial work** — linear phase alone is a week of focused DSP. Proceeding per user request.
- Add `juce::dsp::Convolution mConvolution;` + `juce::AudioBuffer<float> mIRBuffer;`
- Add `void rebuildLinearPhaseIR();` — called when any band changes AND `mPhaseMode != Standard`.
- The IR rebuild: compute frequency response `H(k)` at FFT/2 bins via `getMagnitudeForFrequency + getPhaseForFrequency` (need to add a phase method too), IFFT to get IR, window it, load into convolution.
- Swap between IIR path and Convolution path in `process()` based on `mPhaseMode`.

### 12h. Per-band M/S routing (replace whole-EQ `EQ8MsDSP` approach)
Current `EQ8MsDSP` is two full EQ8DSP instances (one Mid, one Side). Per-band M/S is the Pro-Q 3 approach: each individual band routes to L+R, M only, S only, or L only / R only.

**Implementation:**
- Extend `Band` struct with `enum Channel { Stereo, Mid, Side, LOnly, ROnly }; Channel channel { Stereo };`
- Add `setBandChannel(int i, Band::Channel)` setter
- In `process()`, restructure per-sample loop:
  - For each sample: compute mid = (L+R)/2, side = (L-R)/2, keep originals L/R
  - For each band, based on `channel`:
    - Stereo: process L and R independently (current behavior)
    - Mid: process mid only, leave side untouched
    - Side: process side only, leave mid untouched
    - LOnly / ROnly: process one channel only
  - Recombine: L = mid + side, R = mid - side (if any band touched mid or side)
- **CPU impact:** same as current; you still run 8 bands' filters, just on different signals.
- **Deprecate `EQ8MsDSP`** — per-band M/S subsumes whole-EQ M/S. Keep `EQ8MsDSP` class as a thin wrapper that just sets all bands to Mid (for one spare) and all to Side (for the other) for backward compat with existing editor panels. Flag it for removal in a later pass.
- Serialize `channel` per band.

### 12i. Spectrum analyzer overlay
Real-time FFT of the input signal (pre-EQ) and output signal (post-EQ), drawn behind the EQ curve in the editor.

**DSP side (EQ8DSP):**
- Add two `juce::dsp::FFT` instances (size 2048, overlap 50% via `juce::dsp::WindowingFunction<float>`)
- Add two ring buffers (input-sampled, output-sampled) with seqlock protection (pattern already in `SpectrumFeed` per CLAUDE.md)
- In `process()`: at block start, feed input block into input-FFT ring; at block end, feed output block into output-FFT ring
- Expose `getInputSpectrum(float* bins, int numBins)` and `getOutputSpectrum(...)` methods using the `SpectrumFeed` seqlock pattern
- **Cost:** 1 FFT per ~1024 samples per channel = roughly 0.5% CPU at 48k. Negligible.

**UI side (ParametricEQDisplay):**
- Add timer @ 30 Hz
- Each tick: pull spectrum, convert bin magnitudes to dB, interpolate to pixel X-positions
- Draw pre-EQ spectrum in translucent gray, post-EQ spectrum in translucent white/yellow
- Draw on top: the EQ curve from `getMagnitudeForFrequencyDb()` (12a) at solid color

**Editor integration:**
- `ParametricEQDisplay::bindToDSP(EQ8DSP*)` wires up both curve and spectrum pulls
- User toggles: "Show Input Spectrum", "Show Output Spectrum" — stored as UI state, not DSP state

### 12j. Dynamic EQ — Full feature (DSP + UI)

**Per-band DSP extensions (add to `Band` struct):**
```cpp
bool  dynamic   { false };   // enable dynamic processing
float threshold {  -18.0f };  // dB, -60..0
float ratio     {    4.0f };  // 1..20 (or <1 for upward)
float attack    {   10.0f };  // ms, 0.1..100
float release   {  100.0f };  // ms, 10..2000
float range     {    6.0f };  // dB, 0..24 (max dynamic deviation)
bool  upward    { false };    // true = expander, false = compressor
```

**Per-band runtime state (new `DynBandState` struct):**
```cpp
struct DynBandState {
    std::array<IIRFilter, kMaxSections> detectorL, detectorR;  // parallel detector
    float envL { 0.0f }, envR { 0.0f };        // envelope follower
    float attackCoef { 0.0f }, releaseCoef { 0.0f };
    std::atomic<float> currentGrDb { 0.0f };   // for UI meter
};
std::array<DynBandState, kNumBands> mDyn;
```

**Detector filter:**
- Parallel to the main band filter — same type / freq / Q / slope
- Built in `updateBand()` alongside the main filter
- Runs on the input signal (NOT the post-EQ output) to avoid feedback-loop complications
- Stereo: `detectorL` and `detectorR` mirror the main filter's `filtersL/R` configuration

**Per-block processing in `process()` for each dynamic band:**
```cpp
for (int i = 0; i < kNumBands; ++i) {
    const auto& p = mBands[i].params;
    if (!p.dynamic || !bandShouldProcess(i)) continue;

    float peak = 0.0f;
    for (int n = 0; n < numSamples; ++n) {
        // 1. Run detector filter on input sample
        float detL = detector_eval(i, 0, inputL[n]);
        float detR = detector_eval(i, 1, inputR[n]);
        float det  = std::max(std::abs(detL), std::abs(detR));  // stereo linked

        // 2. One-pole envelope follower
        float coef = (det > mDyn[i].envL) ? mDyn[i].attackCoef : mDyn[i].releaseCoef;
        mDyn[i].envL = coef * mDyn[i].envL + (1.0f - coef) * det;

        peak = std::max(peak, mDyn[i].envL);
    }

    // 3. Block-rate gain computation
    float levelDb = 20.0f * std::log10(std::max(peak, 1e-6f));
    float overshoot = levelDb - p.threshold;
    float grDb = 0.0f;
    if (overshoot > 0.0f) {
        grDb = overshoot * (1.0f/p.ratio - 1.0f);
        if (p.upward) grDb = -grDb;   // upward = add gain
    }
    grDb = juce::jlimit(-p.range, p.range, grDb);
    mDyn[i].currentGrDb.store(grDb);

    // 4. Effective band gain = static + dynamic delta
    float effectiveGainDb = p.gainDb + grDb;

    // 5. Rebuild main band coefs with new gain (block-rate)
    if (std::abs(effectiveGainDb - mBands[i].lastAppliedGainDb) > 0.01f) {
        rebuildBandWithGain(i, effectiveGainDb);
        mBands[i].lastAppliedGainDb = effectiveGainDb;
    }
}
```

**New helper method `rebuildBandWithGain(int i, float gainDb)`:** variant of `updateBand()` that takes an override gain, used by dynamic processing. Keeps other params (freq/Q/type/slope) from `params`.

**Coefficient recompute: block rate, not per-sample.** Per-sample biquad rebuild is too expensive for 8 × 4 sections. Block rate (e.g. every 64 samples via JUCE's internal block size, or explicit inner-block subdivide) is standard and sounds fine.

**Attack/Release coef recompute:**
- In `updateBand()`: `mDyn[i].attackCoef = std::exp(-1.0 / (p.attack * 0.001 * sr));` (same for release)
- Triggered when attack/release params change.

**UI extensions (large — this is the UI-heavy part of the feature):**

1. **Per-band parameter panel** — when a band is selected, show:
   - Dynamic ON/OFF toggle
   - When ON: Threshold, Ratio, Attack, Release, Range knobs/sliders
   - Direction selector (compressor / expander)
   - Uses existing `SharedUI.h` knob components (VKnob, SnapSlider)

2. **Animated EQ curve** — `ParametricEQDisplay` extensions:
   - Timer at 30 Hz polls each dynamic band's `currentGrDb.load()`
   - Redraws the curve with effective gain instead of static gain for dynamic bands
   - Adds translucent "ghost band" showing the full ±range envelope so user sees the dynamic movement potential
   - Color coding: static bands in band-indexed color, dynamic bands pulse slightly when active

3. **Right-click band menu** — add "Make Dynamic" / "Make Static" quick action

4. **Per-band GR meter** — small vertical meter next to each band's handle showing live dynamic gain reduction

**Serialization additions:**
- Each `Band` serializes 7 new fields: `dynamic, threshold, ratio, attack, release, range, upward`
- Loading old presets: all fields default to safe values, `dynamic = false` → identical behavior to pre-upgrade

**CPU budget:**
- Each dynamic band: ~2× a static band (main + detector filters + envelope + gain computer)
- All 8 bands dynamic worst case: ~1.8× current EQ CPU. Acceptable.
- All 8 static (default): zero overhead (`if (!dynamic) skip`)

**Thread safety:**
- `mDyn[i].currentGrDb` is `std::atomic<float>` — audio thread stores, UI thread loads
- `mBands[i].lastAppliedGainDb` is audio-thread-only → no sync needed

### Order of implementation (Section 12)
1. **12a** (magnitude method — standalone, easy, unblocks UI curve drawing)
2. **12b** (proportional Q — 1-line change)
3. **12d** (wire `mIIRModSpeed`)
4. **12c** (SmoothedValue per band — uses 12d's ramp time)
5. **12h** (per-band M/S routing — refactors process() to mid/side split)
6. **12i** (spectrum analyzer — DSP FFT feeds + UI overlay)
7. **12j** (dynamic EQ full — DSP first, then UI integration)
8. **12e** (TPT hybrid — lower priority, applies only to specific filter types)
9. **12f** (2× oversampling anti-cramping — opt-in)
10. **12g** (Linear-phase / HQ modes — biggest, last — substantial DSP week)

---

# ✅ End of Review — Summary of Approved Changes

12 modules total. Implementation order recommendation:
- **Quick wins first:** Chorus (§1), Compressor (§2a,c), Phaser (§7), Transient Shaper (§11a,b,d,e), DC-blockers (§3a, §6b, §9c, §10h), log mapping (§7a) — all small isolated changes.
- **Filter upgrades:** Delay §3c (TPT in FB), Overdrive §6d (TPT filters), Transient §11b (LR4).
- **Oversampling wave:** Overdrive §6a, Saturation §9a, Tape §10c, Transient Shaper §11c — do these consecutively since they share the same `juce::dsp::Oversampling` pattern and test method.
- **Rewrites:** Limiter §5 (net-new), Tape §10 (biggest rework).
- **Stereo/detection pivots:** Compressor §2b+d (per-sample envelope + stereo linking).
- **Reverb enhancements:** §8a (tail mod) + §8b (click reduction).
- **EQ enhancements (§12):** do 12a-d first (easy DSP wins), then 12h, then 12i+12j (UI-heavy), then 12e-g (biggest items — TPT hybrid, anti-cramping, linear-phase/HQ modes).

The full per-module spec is above. Ready to begin implementation when Jeff approves.

