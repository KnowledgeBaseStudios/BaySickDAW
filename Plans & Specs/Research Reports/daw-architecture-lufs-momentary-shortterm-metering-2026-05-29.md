# DAW Architecture Research — Real-Time EBU R128 / BS.1770 Momentary + Short-Term LUFS on a JUCE Stereo Master Bus — 2026-05-29

> Provenance: produced by the QA-RustyMeter (`sorted-whistling-shannon`) "Understand" workflow (`daw-architecture-research` agent), 2026-05-29. Drafter output applied verbatim. Informs the QA-RustyMeter metering-architecture upgrade (master-strip LUFS readout).

## Problem statement
BaySickDAW wants a real-time loudness readout (Momentary 400 ms and/or Short-Term 3 s LUFS) on the stereo master bus, computed on the audio thread and broadcast to a UI label. This is a metering DSP node, not the full integrated-with-gating standard. The question is the concrete recipe: K-weighting filter coefficients (or derivation), the sliding-window mean-square integration, the LUFS formula, the lock-free audio->UI broadcast, and whether JUCE ships a helper.

## What BaySickDAW currently does
- **No LUFS / loudness / K-weighting anywhere in Source/.** Greenfield — nothing to duplicate.
- **Master bus metering today is peak only.** `MasterBusNode` (`Source/VibeGraph.h:691,698`) feeds `masterPeakDbL` / `masterPeakDbR` atomics (`Source/VibeGraph.h:628-629`) plus a mono `masterPeakDb` (`:619`). These are the broadcast points a LUFS atomic sits beside.
- **The audio->UI broadcast pattern is already established.** `DBFSMeter` (`Source/Standalone/SharedUI.h:1623`) uses `std::atomic<float> mLevelDbL/mLevelDbR` (`:1672-1673`); audio thread CAS-max-writes, UI vblank `exchange()`s with `-inf`. For a *continuously-valued* LUFS readout the pattern is simpler — a plain relaxed store/load (NOT CAS-max).
- **K-weighting biquads are buildable with primitives already in use.** `EQ8DSP::makeOneSection` (`Source/DSP/EQ8DSP.cpp:501-520`) already calls `juce::dsp::IIR::Coefficients<float>::makeHighShelf` / `makeHighPass` (`:512,514`).
- **Seqlock broadcast available if a vector is ever needed.** `SpectrumFeed` (`Source/DSP/SpectrumFeed.h`) is the wait-free audio->UI seqlock for arrays. Not needed for a scalar LUFS value.

## K-weighting filter — two stages (derive per `prepareToPlay(sampleRate)`)

**Stage 1 — high shelf** (analog-prototype constants, cross-checked across libebur128 + loudness.py):
- `f0 = 1681.9744509555319 Hz`, `G = 3.99984385397 dB`, `Q = 0.7071752369554193`

**Stage 2 — RLB high-pass:**
- `f0 = 38.13547087613982 Hz`, `Q = 0.5003270373253953`

**Bilinear derivation per stage:** `K = tan(pi * f0 / fs)`
- High-shelf (libebur128 Vh/Vb form): `Vh = pow(10, G/20); Vb = pow(Vh, 0.4996667741545416); a0 = 1 + K/Q + K*K;`
  - `b0 = (Vh + Vb*K/Q + K*K)/a0;  b1 = 2*(K*K - Vh)/a0;  b2 = (Vh - Vb*K/Q + K*K)/a0;`
  - `a1 = 2*(K*K - 1)/a0;  a2 = (1 - K/Q + K*K)/a0;`
- High-pass (RLB): `a0 = 1 + K/Q + K*K; b0 = 1/a0; b1 = -2/a0; b2 = 1/a0; a1 = 2*(K*K-1)/a0; a2 = (1 - K/Q + K*K)/a0;`
- Feed each stage into `juce::dsp::IIR::Coefficients<float>(b0,b1,b2,a0=1,a1,a2)` → `juce::dsp::IIR::Filter<float>` per channel (2 stages × 2 channels), or `ProcessorDuplicator` × 2 stages.

**Acceptance test (sanity check at fs = 48000):** shelf should land near `{b0=1.53512, b1=-2.69170, b2=1.19839, a1=-1.69066, a2=0.73248}`; RLB near `{1, -2, 1, a1=-1.99005, a2=0.99007}` (klangfreund / narkive canonical). Match to 4-5 places = bilinear correct.

## Mean-square + sliding window (fixed ring of per-bin energies)
- `binsPerSecond = 10..20` (>=10 Hz EBU update; 20 gives smoother window edges — **owner pick**).
- `samplesPerBin = round(fs / binsPerSecond);`
- `momentaryBins = round(0.4 * binsPerSecond);`  (8 bins @ 20/s)
- `shortTermBins = round(3.0 * binsPerSecond);`  (60 bins @ 20/s)
- Ring `binEnergyL/R` sized `shortTermBins` (covers both windows). Per sample after K-filtering: `curBinSumL += kL*kL; curBinSumR += kR*kR;`. At bin boundary store energy, advance ring (mod), recompute window means (full re-sum of <=60 bins is negligible, or O(1) running-sum-with-subtract).
- Window mean-square per channel = `windowEnergySum_ch / (samplesPerBin * binsInWindow)`.

## LUFS formula (UNgated — Momentary + Short-Term)
```
G_L = G_R = 1.0
weightedMS = 1.0 * meanSquareL + 1.0 * meanSquareR;
lufs = (weightedMS > 1e-12) ? float(-0.691 + 10.0 * log10(weightedMS)) : -120.0f;
```
**Momentary + Short-Term are explicitly NOT gated** (Essentia verbatim, EBU Tech 3341, klangfreund). The -70 absolute / -10 LU relative gates apply ONLY to Integrated/LRA — out of scope.

## Lock-free broadcast (single-producer/single-consumer; relaxed)
```
// node: std::atomic<float> mMomentaryLufs { -120.f };  (+ mShortTermLufs)
// audio thread at bin boundary:  mMomentaryLufs.store(lufsMom, std::memory_order_relaxed);
// UI vblank:  float v = node.mMomentaryLufs.load(std::memory_order_relaxed);
//             label.setText(juce::String(v, 1) + " LUFS", juce::dontSendNotification);
```
Relaxed is sufficient — a stale frame is harmless. NO CAS-max (current value, not a peak).

## JUCE built-in?
**No.** JUCE ships no `LoudnessMeter` / BS.1770 helper. Multiple forum threads confirm build-from-scratch. K-weighting must be hand-built from `juce::dsp::IIR::Coefficients`.

## Recommendation
Adopt the **bilinear-from-constants** derivation (exact at any sample rate — we support 44.1/48/96) fed into `juce::dsp::IIR::Coefficients<float>`, with a fixed-size energy ring. **Do Momentary first** (the FL-style "how loud am I right now" number); Short-Term is a near-free second ring over the same per-bin energies. The stock-JUCE `makeHighShelf` shortcut is rejected — same audio-thread cost but a known ~0.07-0.3 dB error (JUCE forum 59111 flags it).

New node `Source/DSP/LufsMeterDSP.{h,cpp}`, owned by `MasterBusNode`, called in `processMasterBus` (`VibeGraph.h:532`) on the post-fader/post-width stereo sum (after `MasterBusNode::processBlock` width stage ~`VibeGraph.cpp:887`, before/parallel to peak publish ~`:897`). Two atomics beside `masterPeakDbL/R`. `prepareToPlay` already plumbs the sample rate to master DSP. No CMake change (`juce::dsp` already in use).

## Sources (all WebFetched unless noted; HIGH confidence)
- libebur128 (jiixyj) `ebur128.c` — https://github.com/jiixyj/libebur128 — derivation constants, energy storage, `-0.691`, channel weights.
- klangfreund LUFSMeter `Ebu128LoudnessMeter.cpp` — https://github.com/klangfreund/LUFSMeter — canonical 48k coeffs, bin-ring window, ungated confirmation.
- loudness.py (BrechtDeMan) — https://github.com/BrechtDeMan/loudness.py — independent confirmation of f0/G/Q.
- Essentia LoudnessEBUR128 — https://essentia.upf.edu/reference/std_LoudnessEBUR128.html — 400 ms / 3 s windows, "not gated", 10 Hz hop.
- JUCE forum 59111 — https://forum.juce.com/t/iir-filter-coefficients-for-lufs-calculation/59111 — makeHighShelf approximation caveat.
- narkive music-dsp ITU 1770 RLB thread — RLB coeff cross-check.
- EBU Tech 3341 / tech.ebu.ch/loudness (WebSearch snippet — PDF image-encoded, did not text-extract); ITU-R BS.1770-4 (PDF did not extract). Numerical facts triangulated from the three mutually-agreeing code references above.

## Deferred / out of scope
- **True-peak (dBTP)** — separate BS.1770 component (4× oversample + peak); own node if ever wanted.
- **Integrated LUFS + LRA** — the gated start-to-stop number; needs -70 absolute + -10 LU relative histogram gating + transport-tied window. Larger follow-on node.
