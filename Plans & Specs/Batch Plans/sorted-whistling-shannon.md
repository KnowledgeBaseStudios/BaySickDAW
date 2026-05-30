# QA-RustyMeter — Metering architecture upgrade: split Peak/RMS meters + Master LUFS readout — Plan (sorted-whistling-shannon)

> **Canonical path** (mirrored over the existing file after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/sorted-whistling-shannon.md`
> Paired running notes: `Plans & Specs/Running Notes/sorted-whistling-shannon.md`
> LUFS research: `Plans & Specs/Research Reports/daw-architecture-lufs-momentary-shortterm-metering-2026-05-29.md`

> **For execution:** `superpowers:executing-plans` inline. `- [ ]` checkbox steps. Builds run by Jeff (`do_build.bat`) — never by Claude. Verify Debug FIRST, then Release. **RE-SCOPE of an open batch** (Task 0 + Task 1 already landed). The original per-layer-volume "bug" diagnosed as **not a bug** (expected peak-meter behavior); Jeff pivoted to a metering-architecture upgrade. Tasks continue: **Task 2 (Split meter) → Task 3 (Master LUFS) → Task 4 (dedup fix) → Task 5 (bus collapse UI) → Task 6 (Close)**.
>
> **Code blocks below are implementation sketches** — codebase-consistent and concrete enough to build from, but exact JUCE sign conventions (IIR `a1/a2`), field names (`BlockContext`/`posInfo`), and the full per-site plumbing enumeration are verified/refined at build. The K-weighting coeffs have a hard acceptance test (the 48 kHz sanity table).

---

## Context

**Origin + pivot.** QA-RustyMeter opened (Task 0, `8e27a31`) to investigate the BaySickRustyDrums per-layer-volume CC sliders that audibly change output but don't move the per-strip dBFS meter. **Task 1 (no source change) settled it as NOT a bug:** the kit SFZ + `buildOutputRoutedSfzWrapper` route the `amplitude_cc` correctly to each piece's strip (verified kick + snare); every meter is a **PEAK** meter (`bufferPeakDbStereo`→`getMagnitude`), and Rusty's per-layer faders are **mic-mix** controls (overhead/room/body mics + decorrelated summing raise loudness/RMS without raising the peak transient), so the peak meter correctly shows ~no change.

**The new work (Jeff pivot, 2026-05-30):** a dense, FL-style metering upgrade:
1. **Split Peak/RMS meter (all non-master strips):** 50/50 — bottom = existing dBFS peak bar; top = a centered scrolling RMS "waveform" (L left / R right, color-graded green-center → red-edge by the dBFS palette), scrolling down ~3.5 s.
2. **Master-strip LUFS readout:** a box between the width knob and master fader showing one of Momentary / Short-Term / Integrated (all 3 computed, one shown, `▾` selector). Master keeps a **full-height peak bar**.

**Risk:** medium-high (shared `DBFSMeter`, broad meter-publish plumbing, net-new master DSP + transport hook + new UI). **Effort:** large (~12-20 h, Tasks 2-3). **Dependencies:** QA-DispatcherAffinity closed (`5e830e2`). **Bucket:** Mixer / Routing (primary), UI / L&F / Theming, Cross-cutting Infrastructure.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Source |
|----|----------|--------|
| S1 | Task structure = **Option A, 3 tasks** (Split meter / Master LUFS / Close). | Jeff 2026-05-30 (context economy). |
| S2 | Diagnosis settled: meter-vs-knob disconnect is **not a bug** (peak-vs-loudness + mic-mix). Original wrapper/sfizz Sub-A superseded. | Task 1. |
| S3 | `sorted-whistling-shannon`; **re-scope in place**. | Jeff 2026-05-30 (#11). |
| #1 | LUFS box: **M+S+I all compute; ONE displayed**; [value / mode-title] + `▾` selector. Integrated gated + **resets on play-from-top / loop**. | Jeff (#1). |
| #2 | **Master keeps a FULL peak bar** → `DBFSMeter::Layout {Full, Split}` (master=Full, others=Split). | Jeff (#2=b). |
| #3 | **All non-master strips split this batch** (full per-strip RMS publish across insert kinds + buses). | Jeff (#3=a). |
| #4/#7 | **Centered scope trace; L deflects left, R deflects right** from the centerline. | Jeff. |
| #5 | RMS = **windowed ~150-300 ms** (EMA). | Jeff (#5=b). |
| #6 | History **~3-4 s**; scroll derived. | Jeff (#6=b). |
| #8 | Color = **smooth gradient, dBFS palette keyed to deflection**: green center → `#FFCC00` → `#FF6020` → `#FF2020` edge. | Jeff ("exactly what I want"). |
| #9 | LUFS box **~18-20 px** (single mode). | Jeff. |
| #10 | Split **50/50**. | Jeff. |

## Sub-spec calls surfaced for ExitPlanMode (minor tunables — proposed defaults; confirm at review or Task 2/3 verify)

| ID | Tunable | Default (within Jeff's ranges) |
|----|---------|--------------------------------|
| T-a | RMS EMA time constant | **~200 ms** |
| T-b | History depth / ring size | **~3.5 s**, `kRmsHist = 256` |
| T-c | LUFS bins/sec | **20** |
| T-d | Selected-mode persistence | **persist to `settings.xml`**; default Momentary |
| T-e | RMS stroke | two `juce::Path`s, ~1.6 px (optional 3-layer neon) |
| T-f | True-peak (dBTP) + Integrated LRA + per-strip LUFS | **out of scope → Future State** |

> §0 Rule 5: S1-S3 + #1-#10 were each surfaced + Jeff-answered in chat before landing. T-a..T-f are points within already-chosen ranges, flagged not pre-locked.

---

## Class outline + implementation sketches

### A. `LufsMeterDSP` — NEW `Source/DSP/LufsMeterDSP.{h,cpp}` (the meaty net-new DSP)

```cpp
// LufsMeterDSP.h  — EBU R128 / BS.1770 on a stereo bus.
// M (400 ms) + S (3 s) = ungated sliding windows; I = gated (-70 abs, -10 rel),
// resets on transport play-from-top / loop.  Recipe: Research Reports/daw-architecture-lufs-*.
class LufsMeterDSP
{
public:
    void prepareToPlay (double sr, int /*blk*/)
    {
        mSr = sr; designKWeighting (sr);
        mSamplesPerBin = juce::jmax (1, (int) std::lround (sr / kBinsPerSec));
        mBinEL.assign (kShortTermBins, 0.0); mBinER.assign (kShortTermBins, 0.0);
        mBinHead = mSampleInBin = 0; mCurL = mCurR = 0.0;
        resetIntegrated();
        mShelfL.reset(); mShelfR.reset(); mHpL.reset(); mHpR.reset();
    }

    void process (const juce::AudioBuffer<float>& buf)   // audio thread; post-fader/width stereo
    {
        const int n = buf.getNumSamples(); const int nc = buf.getNumChannels();
        const float* L = buf.getReadPointer (0);
        const float* R = nc > 1 ? buf.getReadPointer (1) : L;
        for (int s = 0; s < n; ++s)
        {
            const float kl = mHpL.processSample (mShelfL.processSample (L[s]));
            const float kr = mHpR.processSample (mShelfR.processSample (R[s]));
            mCurL += (double) kl * kl; mCurR += (double) kr * kr;
            if (++mSampleInBin >= mSamplesPerBin) closeBin();
        }
    }

    void resetIntegrated() noexcept
    { mIntegBlocks.clear(); mIntegratedLufs.store (-120.f, std::memory_order_relaxed); }

    float momentary()  const noexcept { return mM.load (std::memory_order_relaxed); }
    float shortTerm()  const noexcept { return mS.load (std::memory_order_relaxed); }
    float integrated() const noexcept { return mIntegratedLufs.load (std::memory_order_relaxed); }

private:
    static constexpr int    kBinsPerSec = 20, kMomentaryBins = 8, kShortTermBins = 60;
    static constexpr double  kOffset = -0.691;

    void closeBin()
    {
        mBinEL[mBinHead] = mCurL; mBinER[mBinHead] = mCurR;
        mCurL = mCurR = 0.0; mSampleInBin = 0;
        mBinHead = (mBinHead + 1) % kShortTermBins;
        mM.store (windowLufs (kMomentaryBins), std::memory_order_relaxed);
        mS.store (windowLufs (kShortTermBins), std::memory_order_relaxed);
        accumulateIntegrated();   // 400 ms blocks @ 75% overlap -> gated histogram
    }
    float windowLufs (int bins) const
    {
        double eL = 0, eR = 0;
        for (int i = 0; i < bins; ++i)
        { const int j = (mBinHead - 1 - i + 2*kShortTermBins) % kShortTermBins;
          eL += mBinEL[j]; eR += mBinER[j]; }
        const double ms = (eL + eR) / ((double) mSamplesPerBin * bins);  // G_L=G_R=1
        return ms > 1e-12 ? (float) (kOffset + 10.0 * std::log10 (ms)) : -120.f;
    }
    void accumulateIntegrated();   // .cpp: -70 abs gate + -10 LU relative gate over stored blocks
    void designKWeighting (double fs);

    juce::dsp::IIR::Filter<float> mShelfL, mShelfR, mHpL, mHpR;
    double mSr = 48000; int mSamplesPerBin = 2400, mSampleInBin = 0, mBinHead = 0;
    double mCurL = 0, mCurR = 0; std::vector<double> mBinEL, mBinER, mIntegBlocks;
    std::atomic<float> mM { -120.f }, mS { -120.f }, mIntegratedLufs { -120.f };
};
```

```cpp
// LufsMeterDSP.cpp — K-weighting, bilinear-from-constants (exact at any fs).
// ACCEPTANCE TEST @48k: shelf ~ {1.53512,-2.69170,1.19839, a1=-1.69066,a2=0.73248};
//                       RLB  ~ {1,-2,1, a1=-1.99005,a2=0.99007}.  Verify JUCE a-sign at build.
void LufsMeterDSP::designKWeighting (double fs)
{
    auto mk = [] (double b0,double b1,double b2,double a1,double a2)
    { return new juce::dsp::IIR::Coefficients<float> ((float)b0,(float)b1,(float)b2,1.0f,(float)a1,(float)a2); };
    {   const double f0=1681.9744509555319, G=3.99984385397, Q=0.7071752369554193;
        const double K=std::tan(juce::MathConstants<double>::pi*f0/fs);
        const double Vh=std::pow(10.0,G/20.0), Vb=std::pow(Vh,0.4996667741545416), a0=1+K/Q+K*K;
        auto* c = mk((Vh+Vb*K/Q+K*K)/a0, 2*(K*K-Vh)/a0, (Vh-Vb*K/Q+K*K)/a0, 2*(K*K-1)/a0, (1-K/Q+K*K)/a0);
        mShelfL.coefficients = c; mShelfR.coefficients = c; }
    {   const double f0=38.13547087613982, Q=0.5003270373253953;
        const double K=std::tan(juce::MathConstants<double>::pi*f0/fs), a0=1+K/Q+K*K;
        auto* c = mk(1/a0,-2/a0,1/a0, 2*(K*K-1)/a0, (1-K/Q+K*K)/a0);
        mHpL.coefficients = c; mHpR.coefficients = c; }
}
```

**Owner/site:** `MasterBusNode` member; `process()` in `MasterBusNode::processBlock` after the M/S width stage (`VibeGraph.cpp:~887`), before peak publish (`~:897`). **Transport reset** (self-contained, `BlockContext::posInfo`):
```cpp
const double ppq = mCtx->posInfo.ppqPosition; const bool playing = mCtx->posInfo.isPlaying;
if ((playing && ! mWasPlaying) || (playing && ppq + 1e-6 < mLastPpq)) mLufs.resetIntegrated();
mWasPlaying = playing; mLastPpq = ppq;
mLufs.process (buf);
```
**Accessor:** `float VibeSynthProcessor::getMasterLufs (int mode)` → reads the master node's M/S/I atom by mode.

### B. `DBFSMeter` split + scrolling RMS (`Source/Standalone/SharedUI.h:1623`, `.cpp:6405-6663`)

```cpp
// SharedUI.h additions
enum class Layout { Full, Split };
void setMeterLayout (Layout l) { mLayout = l; }
void setRmsStereo (float dbL, float dbR) { mRmsInL = dbL; mRmsInR = dbR; }  // UI thread (MixerPage drain)
private:
    Layout mLayout { Layout::Split };
    float  mRmsInL { kFloor }, mRmsInR { kFloor };
    static constexpr int kRmsHist = 256;                       // ~3.5 s @ vblank (T-b)
    std::array<float, kRmsHist> mRmsHistL {}, mRmsHistR {};
    int    mRmsHead { 0 };
    void paintRmsWaveform (juce::Graphics&, juce::Rectangle<float>) const;
    void paintBars        (juce::Graphics&, juce::Rectangle<float>) const;  // refactor of current L/R bar block
```
```cpp
// onVBlank() — after the existing ballistics step(), before repaint():
if (mLayout == Layout::Split) {
    mRmsHistL[mRmsHead] = mRmsInL; mRmsHistR[mRmsHead] = mRmsInR;
    mRmsHead = (mRmsHead + 1) % kRmsHist;
}
```
```cpp
// paint() — replace the single bar block:
if (mLayout == Layout::Split) {
    const float splitY = b.getY() + b.getHeight() * 0.5f;
    paintRmsWaveform (g, b.withBottom (splitY));   // top half
    paintBars        (g, b.withTop   (splitY));    // bottom half = existing LED bars
} else
    paintBars (g, b);                              // master: full-height bar
```
```cpp
void DBFSMeter::paintRmsWaveform (juce::Graphics& g, juce::Rectangle<float> r) const
{
    g.setColour (juce::Colour (0xff0A0C0E)); g.fillRect (r);     // recessed housing
    const float cx = r.getCentreX(), half = r.getWidth() * 0.5f - 1.f;
    auto buildPath = [&] (const std::array<float,kRmsHist>& h, bool leftSide) {
        juce::Path p; const int rows = juce::jmax (1, (int) r.getHeight());
        for (int row = 0; row < rows; ++row) {
            const int j = (mRmsHead - 1 - row + 2*kRmsHist) % kRmsHist;   // newest at top
            const float nrm = dbToNorm (h[j]);                            // 0..1 deflection
            const float x = leftSide ? cx - nrm * half : cx + nrm * half;
            const float y = r.getY() + (float) row;
            row == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
        }
        return p;
    };
    for (bool leftSide : { true, false }) {
        // smooth dBFS-palette gradient: green at centre -> red at the outer edge (#8)
        juce::ColourGradient grad (juce::Colour (0xff22EE44), cx, r.getY(),
                                   juce::Colour (0xffFF2020), leftSide ? r.getX() : r.getRight(), r.getY(), false);
        grad.addColour (0.72, juce::Colour (0xffFFCC00));
        grad.addColour (0.85, juce::Colour (0xffFF6020));
        g.setGradientFill (grad);
        g.strokePath (buildPath (leftSide ? mRmsHistL : mRmsHistR, leftSide), juce::PathStrokeType (1.6f));
    }
    g.setColour (juce::Colour (0xff2A2E30)); g.drawRect (r, 1.f);
}
```

### C. Per-strip windowed-RMS publish (mirror the peak path — pattern once, all insert kinds + buses)

```cpp
// Each node (InsertNode :1168 / *BusNode :330) gains:
std::atomic<float> rmsDbL { -60.f }, rmsDbR { -60.f };
float msEmaL { 0.f }, msEmaR { 0.f };   // audio-thread running mean-square
// ...and calls, right after publishPeakReading(...):
publishRms (buf, msEmaL, msEmaR, rmsDbL, rmsDbR, mSampleRate);
```
```cpp
// VibeGraph.cpp — sibling of publishPeakReading (block sum-of-squares -> ~200 ms EMA -> dB)
inline void publishRms (const juce::AudioBuffer<float>& buf, float& emaL, float& emaR,
                        std::atomic<float>& outL, std::atomic<float>& outR, double sr) noexcept
{
    const int n = buf.getNumSamples(); if (n <= 0) return;
    const float* L = buf.getReadPointer (0);
    const float* R = buf.getNumChannels() > 1 ? buf.getReadPointer (1) : L;
    double sL = 0, sR = 0; for (int s = 0; s < n; ++s) { sL += (double)L[s]*L[s]; sR += (double)R[s]*R[s]; }
    const float a = 1.f - std::exp (-(float) n / (0.200f * (float) sr));     // ~200 ms (T-a)
    emaL += ((float)(sL / n) - emaL) * a;  emaR += ((float)(sR / n) - emaR) * a;
    outL.store (juce::Decibels::gainToDecibels (std::sqrt (emaL), -60.f), std::memory_order_relaxed);
    outR.store (juce::Decibels::gainToDecibels (std::sqrt (emaR), -60.f), std::memory_order_relaxed);
}
```
Then mirror the **existing peak handoff** at the same sites with **store/load** (current value, not CAS-max): per-kind RMS arrays in `VibeGraph` (beside `<Kind>InsertPeakDb*`) via `processInsert` (`:2391-2473`) + `processBus`; PluginProcessor mirrors + `drainInsertRmsDbStereo` (beside `drainInsertPeakDbStereo` `:2042`) + RMS loop in `drainMeterAtomicsForUI` (`:2112`); `MixerPage` drain (`:3251-3284`):
```cpp
auto [rL, rR] = mProcessor.drainInsertRmsDbStereo (kind, idx);   // mirror of the peak drain
strip->setRmsStereo (rL, rR);                                    // -> DBFSMeter::setRmsStereo
```
Master is **excluded** (Full layout, no RMS top).

### D. `LufsReadoutBox` — NEW UI (in `Source/Standalone/SharedUI.{h,cpp}`)

```cpp
class LufsReadoutBox : public juce::Component, private juce::Timer {
public:
    explicit LufsReadoutBox (VibeSynthProcessor& p) : mProc (p) { mMode = loadModeFromSettings(); startTimerHz (30); }
    void paint (juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0A0C0E)); g.fillRoundedRectangle (b, 2.f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.f, juce::Font::bold));
        g.drawText (juce::String (mProc.getMasterLufs (mMode), 1), valueArea(), juce::Justification::centred);
        g.setColour (juce::Colour (0xff7A7E80)); g.setFont (8.f);
        g.drawText (modeName (mMode), titleArea(), juce::Justification::centred);  // "Momentary"/"Short Term"/"Integrated"
        paintCaret (g, caretArea());                                              // the dropdown triangle (right)
    }
    void mouseDown (const juce::MouseEvent&) override {
        juce::PopupMenu m; m.addItem (1,"Momentary"); m.addItem (2,"Short Term"); m.addItem (3,"Integrated");
        m.showMenuAsync ({}, [this] (int r) { if (r) { mMode = r - 1; saveModeToSettings (mMode); repaint(); } });
    }
    void timerCallback() override { repaint(); }
private:
    VibeSynthProcessor& mProc; int mMode { 0 };   // 0=M 1=S 2=I
};
```

### E. Master strip layout (`Source/Standalone/MixerTrackStrip.{h,cpp}`)

```cpp
// member: LufsReadoutBox mLufsBox { mProcessor };   // master only
// construction:
mMeter.setMeterLayout (mType == StripType::Master ? DBFSMeter::Layout::Full : DBFSMeter::Layout::Split);
if (mType == StripType::Master) addAndMakeVisible (mLufsBox);

// resized() — after the width-knob row (y ~ 178), before the fader:
y += kWidthH + kPadV;
if (masterRow) { mLufsBox.setBounds (x, y, w, kLufsH); y += kLufsH + kPadV; }   // kLufsH ~18-20 (#9)
// existing fader setBounds now starts at the (lower) y; thumb may overlap above unity — fine per Jeff.
```

---

## Files to modify
- **Task 2:** `SharedUI.h/.cpp` (DBFSMeter split + RMS ring + `paintRmsWaveform`/`paintBars` + `setRmsStereo`/`setMeterLayout`); `VibeGraph.h/.cpp` (`publishRms`, per-node `rmsDbL/R`, per-kind RMS arrays, `processInsert`/`processBus` store, master→Full); `PluginProcessor.h/.cpp` (RMS mirrors + `drainInsertRmsDbStereo` + drain loop); `MixerPage.cpp` (RMS drain → `setRmsStereo`); `MixerTrackStrip.h/.cpp` (`setRmsStereo` passthrough; Full/Split at construction). *Pattern repeats across insert kinds + buses — representative sites cited in §C; full enumeration at execution.*
- **Task 3:** NEW `Source/DSP/LufsMeterDSP.h/.cpp`; `VibeGraph.h/.cpp` (MasterBusNode owns + calls + transport-reset detect + 3 atomics); `PluginProcessor.h/.cpp` (`getMasterLufs`); `SharedUI.h/.cpp` (`LufsReadoutBox`); `MixerTrackStrip.h/.cpp` (master LUFS row); `CMakeLists.txt` (+`LufsMeterDSP.cpp`); settings path (persist mode, T-d).

---

## Tasks

> Tasks 0 (open, `8e27a31`) + 1 (investigate — diagnosis closed) COMPLETE.

### Task 2 — Split Peak/RMS meter (all non-master strips)
- [ ] **Re-scope docs commit (first, own commit):** mirror this plan over `Batch Plans/sorted-whistling-shannon.md` (+ delete home copy); rewrite Main Plan §5 QA-RustyMeter to the metering-upgrade scope (note original "bug" → not-a-bug); add §9 Forks pivot entry; update §6 footnote. Surface git status + `/draft-commit` → approve → commit (docs only).
- [ ] DBFSMeter §B: `Layout` + `setMeterLayout`; RMS ring + `setRmsStereo`; `paintRmsWaveform`; refactor bar block into `paintBars`; split `paint()` 50/50.
- [ ] RMS publish §C: `publishRms` (~200 ms EMA) on each node; mirror the peak handoff (store/load) across insert kinds + buses → mirrors → `drainInsertRmsDbStereo` → MixerPage drain → `strip->setRmsStereo`.
- [ ] Construction: master = `Full`, others = `Split`.
- [ ] **Tell Jeff (verify):** "Run `do_build.bat`. Debug: (1) Mixer — every non-master strip's meter is split: bottom LED peak bar + a top scrolling waveform; play audio → the wave scrolls downward + reacts. (2) Centered: L fills the left half, R the right; quiet = thin green near center, loud = blooms outward to red tips. (3) **Master** keeps one full-height peak bar. (4) No regression: peak bars read correctly everywhere; CPU steady. Repeat Release."
- [ ] On pass: `/draft-commit` → surface + commit. `/draft-doc running-notes` → apply.

### Task 3 — Master LUFS readout (M/S/I + selector + transport reset)
- [ ] `LufsMeterDSP` §A: K-weighting (verify the 48 k acceptance table + JUCE a-sign); M/S/I windows; `accumulateIntegrated` gating; `resetIntegrated`; 3 atomics.
- [ ] Wire into `MasterBusNode`: own + `prepareToPlay` + `process()` after width; ppq-backward / stopped→playing → `resetIntegrated()`. `getMasterLufs` accessor.
- [ ] `LufsReadoutBox` §D + master row §E (between width knob + fader; fader shifts down). Persist selected mode (T-d).
- [ ] `CMakeLists.txt` += `LufsMeterDSP.cpp`.
- [ ] **Tell Jeff (verify):** "Run `do_build.bat`. Debug: (1) Master shows a LUFS box between width knob + fader: value, mode label under, `▾`. (2) Play a loud mix → sane LUFS (−20..−6); `▾` switch M/S/I — Momentary lively, Short-Term steadier, Integrated climbs toward an overall value. (3) Stop + play-from-top (or loop) → **Integrated resets**; M/S keep tracking. (4) Selected mode persists across restart. Repeat Release."
- [ ] On pass: `/draft-commit` → surface + commit. `/draft-doc running-notes` → apply.

### Task 4 — Project-lifecycle dedup fix (end-batch cleanup; folded in 2026-05-30)
> Out-of-scope bug surfaced during Task 2 testing (Jeff): on File > New (fresh empty project), dropping a previously-used audio file falsely prompts "File Already in Library — already in your library on 'an existing page'". The dedup is consulting stale prior-project page/library state not reset on New Project (or a global index that should be per-project). Same family as the QA-D STATE-* resets. Jeff: fix in THIS batch as end-batch cleanup, after the metering.
- [ ] Diagnose: find the "already in your library" dedup check + what index it consults; confirm File > New / project-create doesn't reset it (or the check is global, not per-project).
- [ ] Fix: reset the library/page dedup index on New Project (or scope the check to the current project's samples).
- [ ] Tell Jeff (verify): (1) open a project with samples → File > New → drop a previously-used file → NO false "already in library" prompt. (2) Drop a true duplicate WITHIN one project → the prompt still fires correctly. Debug then Release.
- [ ] On pass: `/draft-commit` → surface + commit. `/draft-doc running-notes` → apply.

### Task 5 — Bus collapse/expand UI (end-batch cleanup; folded in 2026-05-30)
> Out-of-scope UI add requested by Jeff right after Task 2 part-2 verify (2026-05-30); folded into this batch's cleanup per §0 Rule 3 + the QA-batches-fix convention. A per-bus collapse toggle so a busy mixer can hide a bus's grouped strips without removing them. NOT audio — pure view state. Spec confirmed by Jeff (4 answers below).

**Confirmed spec (Jeff 2026-05-30):**
- Small arrow button (sized like the RibbonTabBar tab dropdown arrows) on each BUS strip's top row, to the right of the name label. **Buses only — NOT Master** (#1).
- Default = arrow points DOWN, group expanded (looks exactly as today). Click → arrow flips UP → that bus's grouped member strips collapse/hide + the layout closes their gap. Click again → expand. The bus strip itself stays.
- Each bus collapses ONLY its own group (Vox collapsing doesn't touch Vox Bus 2).
- **Persists through save** (#3) — collapsed/expanded state saved in project state, restored on load.
- Name label shrinks slightly to make room for the arrow → **tooltip change on ALL strips** (#2): the full displayed name on the top line + "Double-click to rename" below (names truncate when narrow).
- Arrow **greyed out / disabled** when the bus has no member strips to collapse (#4).

**Implementation pointers (grounded in the layout read 2026-05-30):**
- Layout: `MixerPage` `laidOutBus` (`:3529`) + `layoutGroup` (`:3494`) lay each bus strip then its members = `buckets[busChId]` flush to its right, then `kGroupSep`. Collapse = when a bus's flag is set, skip `layoutGroup` for its members (`setVisible(false)`) and DON'T advance `x` for them — just the bus + the gap. Members stay constructed + audio-live (only hidden). `mScrollContent` width recomputed from the new `x`.
- Toggle plumbing: `MixerTrackStrip` gets a collapse arrow button (buses only) + an `onCollapseToggled` callback; `MixerPage` owns the per-bus flag, flips it in the callback, re-runs the strip layout (`:3490-3592`) + `syncHScrollBar`. Arrow up/down driven by the flag; disabled when `buckets[busChId]` is empty.
- Persistence: per-bus `_collapsed` bool — APVTS per-bus param (mirrors the lazily-registered `_mute`/`_solo`, auto-persists with project) OR a MixerState field; invisible-to-user impl detail, settled at build.
- Tooltip (ALL strips): `MixerTrackStrip` ctor (`:52`) sets `mNameLabel` tooltip to `name + "\nDouble-click to rename"` (renamable) / `name` (non-renamable); refresh in `onTextChange` (`:53`) + on programmatic rename so it tracks the current name.

- [ ] Arrow button on bus strips (RibbonTabBar arrow style), right of the shrunk name label; down=expanded / up=collapsed; disabled when no members.
- [ ] `MixerPage` per-bus collapsed flag + `onCollapseToggled` → skip-members-in-layout + relayout + scrollbar sync.
- [ ] Persist the collapsed flag per-project (restore on load).
- [ ] Tooltip on ALL strips: full name (top) + "Double-click to rename" (below); tracks renames.
- [ ] **Tell Jeff (verify):** "Run `do_build.bat`. Debug: (1) every bus strip has a small down-arrow right of its name; click → it flips up + that bus's strips collapse/hide + the row closes the gap; click again → they return. (2) Master has NO arrow. (3) a bus with no strips shows the arrow greyed/disabled. (4) hover any strip's name → tooltip shows the full name + 'Double-click to rename'; long truncated names still show full in the tooltip. (5) collapse some buses, save + reload the project → collapsed state restored. (6) no audio change: a collapsed bus's hidden strips still play + meter. Repeat Release."
- [ ] On pass: `/draft-commit` → surface + commit. `/draft-doc running-notes` → apply.

### Task 6 — Close
- [ ] `/draft-doc batch-close` → apply to `Implemented Work Log.md` (`**Bucket:** Mixer / Routing, UI / L&F / Theming, Cross-cutting Infrastructure`).
- [ ] `/review-batch QA-RustyMeter` → address BLOCKER/NEEDS-FIX; defer NITs into the entry.
- [ ] Strip temp diagnostics (Rule 4) — none expected (static investigation); surface list if any.
- [ ] Route side findings (Rule 3): in-scope → close table; out-of-scope (T-f true-peak / Integrated LRA / per-strip LUFS) → §9 + Future State (surface slot options).
- [ ] Full git status. `/draft-commit` close → surface + approve → commit (separate).

---

## Verification (end-to-end smoke)
1. Build clean (Release + Debug).
2. Split meters on all non-master strips: peak bar + scrolling RMS top; centered L-left/R-right; green-center→red-edge; ~3.5 s scroll; reacts.
3. Master = full peak bar + LUFS box between width knob and fader.
4. LUFS M/S/I selectable; sane values; Momentary lively / Short-Term steady / Integrated accumulates + resets on play/loop; mode persists.
5. No regression: peak readings unchanged; non-Rusty engines + the QA-DispatcherAffinity 6-cymbal MT test clean; CPU steady (RMS = 1 EMA/block/node; LUFS = 2 biquads on master only).

## Routing notes (Rule 3)
- True-peak (dBTP) + Integrated LRA + per-strip LUFS → Future State (T-f), §9 at close.
- If a peak+RMS single-publish unify refactor surfaces → fold if low-risk, else follow-up.
- Rule 4: any temp trace → running-notes catalog row same edit pass; strip at close.

## Carry-Forward Reference touch points
- Task 2: none binding — the meter publish path + `DBFSMeter` are post-2026-05-07-freeze (not in Carry-Forward; confirmed Task 1).
- Task 3: `juce::dsp::IIR` precedent `EQ8DSP.cpp:501-520`; LUFS recipe in `Research Reports/daw-architecture-lufs-momentary-shortterm-metering-2026-05-29.md`.
