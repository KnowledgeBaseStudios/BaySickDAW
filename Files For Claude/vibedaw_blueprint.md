# VibeDAW Blueprint

> **Purpose:** this is VibeDAW's living reference document — a map of everything that IS (current architecture, shipped features per module, system-page state, cross-cutting infrastructure) and everything that could be (deferred features, post-v1.0 ideas, future effect modules, architectural refactors).
>
> **Complements `CLAUDE.md`**: CLAUDE.md carries hot session-loaded rules and gotchas (auto-loaded every conversation, kept lean). This Blueprint is the deep reference, browsed on demand for planning and architectural review.
>
> **Current release target: v1.0.** Items labelled post-v1.0 are candidates for v1.1 (first maintenance release) and beyond.
>
> **Rule for adding new items:** If a new entry doesn't cleanly fit an existing category (Effect Modules / Future Effect Modules / Player Engines / System Pages / Cross-cutting / Architecture), **stop and ask Jeff which category it belongs in** before writing it. Jeff uses category placement to navigate the Blueprint when planning release scope, so miscategorised items get lost.

Sections capture per-module shipped features + deferred / future work. Each item tagged:

- **PRESET-SAFE** — purely additive (new params default to neutral on missing). v1 presets load fine in v1.1+.
- **PRESET-BREAK** — modifies/removes/renames existing params or changes semantics. Would require preset migration logic. ⚠️ **Consider promoting to Tier 2 if the deferral window is long.**

Structure per module:
- **Shipped in v1** — spec items + audit fixes + Tier 1 always-adds + any Tier 2 chosen at audit time
- **Tier 2 deferred** — items presented in audit but passed over this time
- **Tier 3 (post-v1.0)** — bigger features requiring new subsystems (v1.1 maintenance releases, v2.0 major, etc.)

As items ship in future versions, tick them off with the version they landed in.

---

## Effect Modules

### §1 ChorusDSP
**Shipped in v1:**
- §1 Per-voice prime-number delay offsets (0/2/5/1/3/7 ms)
- 3-LFO architecture (independent freq + wave per LFO)
- Catmull-Rom cubic interpolation on delay reads
- `kMaxDelayMs = 64 ms`
- State-serialisation with old-tag compat
- **Phase A retrospective (2026-04-17):**
  - A1 CPU guards on all setters (value-change comparisons)
  - A2 `juce::LinearSmoothedValue` on delayMs / depth / stereo / wet (20 ms ramp; snapped on state-load)
  - A3 `juce::ScopedNoDenormals` in process()
  - A4 `setCrossCutoff` upper clamp to `min(20 kHz, 0.45·SR)`
  - A5 LFO phase wrap uses `while` instead of single subtract
  - A6 `setRate()` legacy method deleted
  - **C1** LR4 crossover — 1-pole LP/HP replaced with `juce::dsp::LinkwitzRileyFilter` LP + HP pair (24 dB/oct)
  - **C2** "Organic" LFO wave added as 4th option (`sin(φ) + sin(0.37φ)`) — PRESET-SAFE (existing Multi unchanged)
  - **C3** `wetOnly` toggle + serialization (kills dry + pass-band return; for send routing)
  - **C4** CrossHz UI range widened to 20 Hz – 10 kHz

**Tier 2 deferred:** (none — all Tier 2 offers accepted this round)

**Tier 3 (post-v1.0):**
- **Algorithm voicing presets** — Chorus/Ensemble/Dimension-D preset chooser with per-voicing parameter constellations. Optional topology swap (voice count, cutoff defaults, LFO wave preset). **PRESET-SAFE** — default "Custom" mode matches current v1 behaviour; user's saved param values stay intact.
- **Per-LFO phase tap offsets → UI** — currently the `extraPh = π` offset for voices 3–5 is hardcoded; expose per-LFO tap-phase knobs for user tuning. **PRESET-SAFE** — new params default to current hidden constants.
- **Tempo-sync on LFO freqs** — per-LFO bool + division selector like Delay/Flanger already have. **PRESET-SAFE** — default off preserves current free-rate behaviour.
- **Feedback-into-delay** (C5 offered this round, skipped for v1) — float 0..0.95 fractional feed of chorus output back to the delay write position. Low values add thickness/regeneration; mid values give flanger-tinged resonance; high values approach full flanger. **PRESET-SAFE** — default 0 = no regen (current behaviour).

### §2 CompressorDSP
**Shipped in v1:**
- §2a Look-ahead (0-5 ms, serialized, reports latency to EffectRack PDC)
- §2b Stereo link with `max(|L|,|R|)` detection, single shared envelope
- §2c Auto-makeup (uses `threshold+6 dB` reference - more musical than 0 dBFS)
- §2d Per-sample RMS detection (now user-controlled time constant - see C4)
- 8 knee types (Hard/Med/Vintage/Soft + /R TCR variants) with vintage optical ratio rollback
- Parallel mix knob, sidechain buffer plumbing, full state serialization
- **Phase A retrospective (2026-04-17):**
  - A1 CPU guards on all setters (value-change comparisons)
  - A2 `juce::LinearSmoothedValue` on threshold/ratio/mix/makeupDb (20 ms ramp; snapped on state-load)
  - A3 `juce::ScopedNoDenormals` in process()
  - A4 GR meter hold+decay (30 dB/sec, SR-aware) - was overwrite-only
  - A5 `setMakeup` now clamps -30..+30 dB (previously unclamped; matched `setGain`)
  - A6 TCR attack/release coefficients + new peak-detector coefficients moved from per-block recompute to `calcCoefs()`
  - Safety clamp: `levelDb` limited to `[-120, +60]` before gain computer (defensive, prevents denormal or numerical glitch from producing inf/nan in the envelope)
  - **C1** `setUseSidechain(bool)` public setter added (flag was serialized but unreachable)
  - **C2** Sidechain HPF knob (20-2000 Hz, default 20 = effectively off) using `juce::dsp::StateVariableTPTFilter` highpass on the detection source; prevents bass transients triggering GR across the mix
  - **C3** Peak vs RMS detection toggle (PRESET-SAFE; RMS default = v1 behavior). Peak detector has fast attack (~0.1 ms) / slow release (= detectionMs). Enough safety layers (envelope smoother + ratio clamp + safety `levelDb` clamp + denormal guard) that the old "peak detection could spike and crash" history no longer applies.
  - **C4** Detection window ("Det") knob (1-100 ms, default 10) — previously hardcoded; exposed as `setDetectionMs`
  - **Scaffolding:** `sidechainSourceId` int param added to state (default -1 = internal). No DSP behavior today; future-proofs preset format against Tier-3 external SC routing UI work.

**Tier 2 deferred:** (none — C1/C2/C3/C4 + scaffolding all accepted this round)

**Tier 3 (post-v1.0):**
- **Multi-stage program-dependent release** (LA-2A opto-style chain: fast→slow transition for musical release curve). Complements existing TCR variants. **PRESET-SAFE** — new mode enum, default matches current behavior.
- **Feedforward vs feedback topology toggle.** Feedback-style compressors (1176) have a distinct "smoother" character because GR is fed back into detection. Would add a new bool param. **PRESET-SAFE** — default feedforward = current behavior.
- **External sidechain source routing UI.** Adds a mixer-channel dropdown on the panel that sets `sidechainSourceId`; VibeGraph reads the id to wire an edge from that channel's output into the compressor's `setSidechainBuffer`. Compressor preset format is already future-proofed by the `scSourceId` scaffolding shipped in v1. **PRESET-BREAK ⚠️** at the cross-cutting graph level (tracked under `Sidechaining infrastructure` below) — but the compressor's own state serialization stays stable.
- **LUFS-referenced detection for mastering use.** K-weighting filter + integrated LUFS reading drives the detector. Useful for mastering-bus comps. **PRESET-SAFE** new mode enum.

### §3 DelayDSP
**Shipped in v1:**
- §3a 5 Hz DC-blocker in feedback path (post-distortion, pre-feedback-level)
- §3b Catmull-Rom cubic interpolation on delay reads (upgraded from linear)
- §3c Per-sample `mModCutoffMod` + `juce::dsp::StateVariableTPTFilter` in feedback chain (LP/HP/BP)
- §3d Biquad retirement from feedback path (replaced by TPT SVF)
- Sat-mode gain-bug fix (Option A): tanh normalized so small-signal gain = 1.0 (prevents feedback runaway)
- ModFB knob (LFO cutoff modulation depth)
- B3 `mToneFilter.reset()` on state load (prevents pop on preset recall)
- Initial CPU guards on many setters
- **Phase A retrospective (2026-04-17):**
  - A1 `juce::ScopedNoDenormals` in process()
  - A2 `juce::LinearSmoothedValue` on `delayMs` with spec-mandated 100 ms ramp (tape-style smooth knob drag). Snapped on state-load.
  - A3 Defensive re-clamp on `mCurDelayL/R` after keep-pitch slew (belt-and-braces against slewed drift below 1 sample)
  - A4 `while`-wrap on LFO phase (was single `if`-subtract)
  - A5 `setDelayMs` upper clamp to 2000 ms (matched documented spec range)
  - A8/A11 Deleted dead `mLegacy*` filter state members (zero readers)
  - A9 State-load legacy-to-new mirror only runs when the new-API key is absent (fixes mixed-preset case where legacy keys overwrote newer API values)
  - **A6 (retrofit during §4 Flanger pass)** Time-knob lockout when BPM sync is engaged. Uses new `VKnob::setLocked(bool)` and `ChickenHeadSelector::setLocked(bool)` methods that grey the widget (alpha 0.45) and swallow value-changing clicks/drags while KEEPING hover tooltips reachable (VKnob via transparent `LockoutOverlay` that is itself a TooltipClient; ChickenHeadSelector via short-circuit in mouseDown/Drag). Toggle state initialized from DSP on panel construct so preset-loaded sync state reflects correctly on reopen. **Inverse lockout on sync-division chicken-head:** greyed out when BPM sync is OFF since the division only takes effect with sync engaged.
  - **C1** Diffusion Spread knob added to panel (unlocks `setDiffusionSpread` that was serialized but never UI-exposed)
  - **C2** Feedback Resonance (FBReso) knob added to panel (unlocks `setFeedbackResonance`)
  - **C3** FBDist Knee + Symmetry knobs added to panel (mode-dependent character: Knee for Limit mode, Sym for Sat mode)
  - **C4** Sync-division chicken-head selector (8 positions: 1/1, 1/2, 1/4, 1/8, 1/8D, 1/4T, 1/16, 1/8T) driving `setSyncNote(num, den)`
  - **C5** WetIn knob (input gain into delay, pre-feedback) — enables "freeze feedback while muting new input" gesture that Dry=0 cannot achieve
  - **Scaffolding:** `mDelayMode` int + 8 reserved `mBandDelayMs[i]` floats serialized in state (DSP ignores; future-proofs preset format against Tier-3 Spectral Delay)

**Tier 2 deferred:** (none - all Tier 2 offers accepted)

**Tier 3 (post-v1.0):**
- **Dual stereo independent delay times** (L/R different base delays, each with own sync division). **PRESET-SAFE** — new params default off.
- **Tape-mode emulation preset bundle.** Bundles keep-pitch off + wow/flutter LFO + lo-fi partial + soft-limit high-drive. **PRESET-SAFE** (preset only, no new params).
- **Reverse delay mode.** New `mDelayModel` enum value that reads the line backward. **PRESET-SAFE** — new enum value.
- **Dedicated wow/flutter LFO** separate from `mModRate`, mimicking tape speed drift. **PRESET-SAFE** new params default off.
- **Spectral delay (per-band delay times via FFT).** New block-processing subsystem, separate UI. **PRESET-BREAK ⚠️** — APVTS scaffolding (`delayMode` + 8 reserved `bandDelayMs` floats) shipped in v1 to future-proof the preset format, so v1→v1.x migration remains unnecessary when the DSP lands.

### §4 FlangerDSP
**Shipped in v1:**
- §4a Damp LPF moved INSIDE feedback loop (was post-feedback-capture, defeating purpose)
- §4b 4-point Catmull-Rom cubic interpolation on delay reads (upgraded from linear)
- §4c `juce::SmoothedValue<Linear>` on Rate / Depth / Feedback (20 ms ramp)
- F2 Sine-only short-circuit skips `asin` branch when Shape == 0 (CPU)
- F3 CPU guards on setters
- F4 `setPhase` clamp 0..360°
- F5 Sync-toggle snap handling (tempo-driven Rate change snaps smoothers, no glide across the switch)
- Serialized: rate, depth, delay, feedback, wet, stereoPhase, shape, invertFeedback/Wet, dryLevelDb, wetLevelDb, crossLevelDb
- **Phase A retrospective (2026-04-17):**
  - A1 `juce::ScopedNoDenormals` in process()
  - A2 Smoothed `mWet` (20 ms ramp; snapped on state-load)
  - A3 Smoothed `mDelay` (20 ms ramp; kills click on base-delay drag at short times)
  - A4 Smoothed `mShape` (20 ms ramp; kills LFO "step" on sine<->triangle morph)
  - A5 LFO phase uses `while`-subtract instead of `std::fmod` (cheaper, no per-sample division)
  - **A6** Rate-knob lockout when BPM sync is engaged. Uses new `VKnob::setLocked(bool)` and `ChickenHeadSelector::setLocked(bool)` methods (greys widget + swallows clicks/drags but keeps hover tooltips reachable so users can read WHY the control is locked). Toggle state initialized from DSP on panel construct so preset-loaded sync state is reflected. **Inverse lockout on sync-division chicken-head:** greyed out when BPM sync is OFF (division selector is sync-only).
  - **A6 cross-apply to §3 Delay panel:** same Time-knob lockout when Delay's BPM toggle is on. Fixed the same pre-existing UX gap there (Delay's Time knob was also silently ignored when sync was engaged).
  - **C1** Cross knob on panel (unlocks existing `setCrossLevel(dB)`, default -96 = off, pro stereo 'spinning' width)
  - **C2** Shape knob on panel (unlocks existing `setShape(0..1)` morph)
  - **C3** `setDampHz(200..20000)` replaces `setDamp(0..1)` -- frequency-cutoff knob instead of raw IIR coefficient. 20 kHz default = transparent (preserves v1 "damp=0" off behavior). Internally converts Hz to 1-pole alpha via `exp(-2*pi*fc/sr)`. **PRESET-BREAK ⚠️** — shipped pre-v1-release, no in-the-wild presets to migrate. State format: `dampHz` key replaces old `damp` key.
  - **C4** Sync-division chicken-head selector (8 positions: 1/1, 1/2, 1/4, 1/8, 1/8D, 1/4T, 1/16, 1/8T). Default index 3 = 1/8 preserves v1 hardcoded behavior. Stored as `mSyncDivIdx`; serialized.

**Tier 2 deferred:**
- **Dry / Wet / CrossLevel dB knobs** (C5 offered this round, skipped for v1). DSP already has `setDryLevel/WetLevel/CrossLevel(dB)` but only Cross is UI-exposed (C1 above). Adding Dry+Wet as extra dB trims would stack on the 0..1 Wet blend; redundant without a full "pro mix section" redesign. Logged to Tier 3 as the redesign path.

**Tier 3 (post-v1.0):**
- **"Pro mix section" redesign** — delete the 0..1 Wet blend knob, replace with independent `WetDb` + `DryDb` + `CrossDb` trims (unity default). More pro UX but inconsistent with Chorus/Delay/Reverb panels. **PRESET-SAFE** — on-load migration `mWet` → `mWetDb` equivalent is trivial. Would need to extend the pattern across other effect panels for consistency.
- **Through-zero (TZ) flanging dedicated mode.** Currently `InvertFeedback` enables negative feedback. A proper TZ mode uses dual delay lines (static 0-delay + swept delay summed) for classic jet-flanger character. **PRESET-SAFE** new bool.
- **Secondary modulator (envelope follower → Rate/Depth).** Per pro flangers like MXR 117. **PRESET-SAFE** new routing.
- **Independent L/R base-delay ("stereo offset split").** **PRESET-SAFE** new field.
- **Resonant feedback-filter** (BP or peaking EQ in feedback loop instead of just LP damp). **PRESET-SAFE** new mode enum.

### §5 LimiterDSP (NET-NEW)
**Shipped in v1:**
- Full spec: input gain + tanh soft-sat + look-ahead delay + 4× oversampled TP detection + peak envelope + two-stage adaptive release + release-curve morph + gain computer + hard-clip + atomic meters + PDC + ValueTree + EffectRack registration
- Basic LimiterPanel with GR meter + Auto Release toggle
- CPU guards + SmoothedValue on InputGain/Ceiling/SatThresh (15 ms)
- **Phase A retrospective (2026-04-17):**
  - A1 `juce::ScopedNoDenormals` in process()
  - A2 GR meter hold+decay (30 dB/sec, SR-aware; was overwrite-only — transient GR between 30 Hz UI polls was being missed)
  - A3 Input + Output dBFS meter hold+decay (same pattern; fall toward -96 dB)
  - **C1** `mSatCurveSmooth` 15 ms ramp (was unsmoothed; knob drag produced harmonic step)
  - **C2** Sidechain HPF knob (20–2000 Hz, default 20 = off) via `juce::dsp::StateVariableTPTFilter` highpass on the detector path only (main audio path untouched; preserves bass in output)
  - **C4** Auto-makeup gain toggle (default off). When on, post-limit boost of `-ceilingDb` keeps output hot as ceiling drops — maximizer workflow. Ceiling still strictly enforced via hard clamp AFTER the boost.
  - **C5** Stereo-link toggle (default on = single envelope driven by max(|L|,|R|)). When off, per-channel TP peaks drive independent envelopes (`mEnvR` / `mEnvFastR` / `mEnvSlowR`) — dual-mono limiting. New `mTpPeaksL`/`mTpPeaksR` arrays separate from the linked `mTpPeaks`.
  - **Panel layout fix:** three toggles (Auto Release / Auto Makeup / Stereo Link) now split into two columns — Auto MU in its own left column (centered), Auto Release + Link stacked on right. All same size, no overlap.
  - **Cross-apply to §2 Compressor:** same layout pattern (Auto MU left, Link + Peak/RMS stacked right) replacing the 3-tall stack that was clipping labels.

**Tier 2 deferred:**
- Polished Fruity-Limiter-style UI per `Files For Claude/DSP Review/Limiter.txt` (3-zone layout, scrolling waveform, skeuomorphic knobs, #00FFF2 cyan GR / #FF9100 orange sat). **PRESET-SAFE** (visual only). Already listed under 5F-9 "Deferred UI work" in master plan.

**Tier 3 (post-v1.0):**
- **Spectral / multi-band limiting** — separate limiter per band for mastering use. Large scope. **PRESET-SAFE** (mode enum).
- **Auto-ceiling ("true-peak aware ceiling")** — ceiling self-adjusts so final output is guaranteed under 0 dBTP after codec transcoding (Spotify/YouTube margin). **PRESET-SAFE**.
- **Release-character voicing presets** (Transparent / Punchy / Vintage) — preset bundles of attack+release+curve+autoRelease. **PRESET-SAFE**.
- **LUFS / dBFS side-by-side output meter** — meter addition, no new params. **PRESET-SAFE**.
- **Oversampling factor UI** (2× / 4× / 8×). Currently hardcoded 4×. Trade CPU for TP accuracy. **PRESET-SAFE** new enum.

### §6 OverdriveDSP
**Shipped in v1:**
- §6a 4x oversampling around the waveshaper stage only (not filters)
- §6b 5 Hz DC-blocker post-clip, SR-tracking R-form
- §6c SmoothedValue on PreAmp / Color / PostFilter / PostGain (15 / 15 / 30 / 15 ms)
- §6d TPT SVF BPF (pre-shaper) + TPT SVF LPF (post-shaper)
- Wet knob (O2), x100 toggle, O5 state-load BPF refresh, O7 state-load reset, O1 CPU guards, legacy setDrive/setTone wrappers
- **Phase A retrospective (2026-04-17):**
  - A1 `juce::ScopedNoDenormals` in process()
  - A3 Smoothed PreBand (BPF Q) via new `mPreBandSmooth` -- knob drags no longer step Q at block boundaries
  - A4 Smoothed Wet via new `mWetSmooth`
  - A5 Smoothed x100 transition via new `mX100ScalarSmooth` (1 <-> 100 ramped over 20 ms) so toggling on/off is click-free
  - A8 Mono-input safety: pad mBandBuf/mResidualBuf channel 1 from channel 0 before running the 2-ch oversampler; read back only numCh on output
  - A9 Per-sample BPF + LPF coef refresh (was once-per-block via `refreshFilterCoefs`/`.skip(numSamples)`). Fast Color / PostFilter sweeps now track smoothly; refreshFilterCoefs() retired as a no-op stub
  - **C1** Waveshaper swapped from `atan(drive*x)/halfPi` to spec `x/(1+|x|)` soft-clip sigmoid (PRESET-BREAK in character -- harmonic profile slightly different; clean slate pre-v1-release so no migration)
  - **C2** New `Bias` knob (-1..+1, default 0): pre-shaper DC offset injects even harmonics (tube-like warmth). Static DC floor subtracted post-shaper; 5 Hz blocker catches any residual drift.
  - **C4** New `Parallel` named toggle (Blend / Parallel): Blend = `dry*in + wet*processed` (default, current behavior). Parallel = `in + wet*processed` (dry stays full, wet adds on top) -- classic "drive-up mix" workflow.
  - **C5** New OS-factor chicken-head (2x / 4x / 8x / 16x, default 4x). Changing the factor reallocates `mOversampler` via `prepare()`; plugin latency updates via `getLatencySamples()`.

**Tier 2 deferred:** (none -- all Tier 2 offers accepted)

**Tier 3 (post-v1.0):**
- **Multi-stage cascade shaper** - 2 or 3 atan/sigmoid stages in series for richer harmonic content. **PRESET-SAFE** new mode enum.
- **Pre-emphasis + de-emphasis EQ curves** (guitar-amp-style frequency shaping before/after distortion). **PRESET-SAFE** new params.
- **Cab simulator impulse-response post-stage.** **PRESET-SAFE** new param.
- **Character voicing presets** (Tube / Fuzz / Distortion / BitCrush) - preset bundles of PreBand + Color + Bias + OS + shape. **PRESET-SAFE**.

### §7 PhaserDSP
**Shipped in v1:**
- §7a log-scaled LFO -> freq mapping, §7b always-allocated 24-stage buffers (no click on count change), §7c SmoothedValue on Rate/Feedback/MinDepth/MaxDepth, §7d InvertFeedback toggle
- P-series audit items: P2 state-load rate/sweep reconciliation, P4 state-load snap+clear, P5 Rate clamp dropped (knob is truth), P6 stereo/outGain clamps, P7 CPU guards, P8 sync-toggle immediate recompute
- **Phase A retrospective (2026-04-18):**
  - **A1** `juce::ScopedNoDenormals` in process() (24-stage IIR state is denormal-prone)
  - **A2** SmoothedValue on Wet (15 ms) - kills zipper on Wet drags
  - **A3** SmoothedValue on Stereo phase (20 ms; stored as cycle fraction) - kills audible step on degrees drag at short wavelengths
  - **A4** while-wrap on LFO phase (was `std::fmod`) - CPU win
  - **A5** SmoothedValue on OutGain converted to linear (15 ms) - kills zipper on Gain drags
  - **A6** single-branch wrap on stereo-offset LFO phase (was `std::fmod`) - matches A4 pattern
  - **A7** BPM-sync Rate-knob soft-lockout. When `mSyncBPM == true` the DSP silently overwrites Rate from host BPM / sync-division; Rate knob now greyed + click-swallowed via `VKnob::setLocked` (transparent `LockoutOverlay` keeps tooltip reachable). **Inverse lockout on sync-division chicken-head:** greyed when sync OFF. Direct cross-apply from §4 Flanger A6 + §3 Delay A6 pattern. Phaser was the 3rd and last module with this gap.
  - **A9 (new cross-apply)** Panel toggles/selectors now sync from DSP state on construct. Pre-fix: `bpmTog`, `invFbTog`, `rangeTog`, `stagesSel` (new: `waveSel`, `syncDivSel`) all defaulted on open regardless of preset. Now all four read `dsp->mSyncBPM` / `dsp->mInvertFeedback` / `dsp->mFreqRange` / `dsp->mNumStages` / `dsp->mLFOWaveIdx` / `dsp->mSyncDivIdx` at construct. `stagesSel` uses a reverse-lookup of `kStageValues[8]`.
  - **C1** `Slow` / `Fast` Range toggle **REWIRED** (was dead UI control — P5 had stripped its audio effect). Slow now clamps Rate to 0.05-2 Hz; Fast to 0.05-10 Hz. DSP setter clamps `mRate`/`mSweepHz` down if the current value exceeds the new max; panel mirrors by calling `slider.setRange(0.05, maxHz, 0.01)` + `slider.setValue(dsp->mRate, dontSendNotification)`. BPM sync re-applies against the new Range so a 1/4-note @ 240 BPM (= 2 Hz) is achievable under Slow but anything shorter would clamp. **PRESET-BREAK ⚠️** in semantics (stored `freqRange` flag now has audio effect) - pre-v1 clean slate, no in-the-wild presets.
  - **C2** Sync-division chicken-head (8 positions: 1/1, 1/2, 1/4, 1/8, 1/8D, 1/4T, 1/16, 1/8T). Default index 2 = 1/4 preserves v1 hardcoded `BPM/60/4` behavior. Serialized as `syncDivIdx`. BPM toggle now uses this division via new `mSyncDivIdx` + `setSyncDiv`. Mirrors §3 Delay + §4 Flanger patterns (reuses same division table shape). PRESET-SAFE.
  - **C3** LFO wave chicken-head (Sine / Triangle / Saw / S&H). Default 0 = Sine preserves v1. Sample-and-hold uses per-channel phase-wrap detection (`mLastPhaseL/R`) + `juce::Random` to draw a new held value on each cycle wrap (stereo-independent). PRESET-SAFE.
  - **C4** Rate knob log-skew (`setSkewFactor(0.35)`) - UI-only, no DSP change. 0.05-0.5 Hz range now gets most of the knob travel. PRESET-SAFE.
  - **C5** Cross-channel feedback knob (0-1, default 0 = current behavior). New `mCrossFB` + `setCrossFB` + `mCrossFBSmooth` (20 ms). Linear blend: `mainFb = 1 - 0.5·xfb`, `crossFb = 0.5·xfb` conserves feedback energy. At xfb=1 the L and R feedback states cross-feed equally. PRESET-SAFE.

**Tier 2 deferred:**
- **C6 SmoothedValue crossfade on Stages count** - spec asked for it; §7b state-preserve already kills clicks in practice, so skipped for v1. Belt-and-braces add if we ever hear an artifact. PRESET-SAFE.
- **Dry/Wet/CrossLevel dB knobs** (consistency follow-on to Flanger Tier 2 deferral) - not in Phaser's Tier 2 this round. Would stack redundantly with the 0..1 Wet blend; see §4 Flanger Tier 3 "Pro mix section redesign" for cross-module path.

**Tier 3 (post-v1.0):**
- **T1 Per-stage resonance LFO offset** (Mu-Tron warble) - offset each stage's notch freq by a small multiplier, gives hardware-style warble. **PRESET-SAFE** new mode enum.
- **T2 Secondary modulator (envelope follower -> Rate/Depth)** - mirror of §4 Flanger T3. Dynamic sweep driven by input amplitude. **PRESET-SAFE** new routing.
- **T3 Feedback filter (LP/HP/BP in FB path)** - shape resonance character per-frequency, mirrors §3 Delay's FB filter topology. **PRESET-SAFE** new mode enum.
- **T4 Barberpole phaser mode** - ever-rising or ever-falling sweep instead of oscillating LFO (Shepard-tone effect). **PRESET-SAFE** new mode enum.

### §8 ReverbDSP
**Shipped in v1:** §8a tail modulation (per-line LFO + cubic-interp reads, mod-depth-zero exact bypass, ER taps stay integer), §8b size-change click reduction (max-allocated FDN buffers, no realloc on room-size change), §8c Freeze/Infinite toggle (smooth on/off, disables input inject + HF damp + bass shelf when engaged to prevent spectral drift and bass pump; HFRatio auto-lerps to 1.0 during freeze so held tail preserves full spectrum), §8d multi-tap ER (3 taps per line at prime-ratio positions `len/5, len/3, len/2` with natural decay envelope), §8e HF Decay Ratio (tilt on per-line feedback LP, runtime-clamped per-line to keep HF loop gain ≤ 0.9995 — prevents runaway regardless of decay/ratio combo), §8f tail-mod shape selector (sine/triangle/random S&H), §8g Wet Tone tilt — **RBJ cookbook low-shelf (250 Hz) + high-shelf (4 kHz) biquad pair** (replaced original DIY split/recombine filter after it caused CPU denormal stalls + gain-transient explosions), §8h BassCross knob UI, `juce::ScopedNoDenormals` guard in process() (prevents subnormal-arithmetic CPU stalls across the 8×FDN + per-line IIR state), R1 CPU guards, R2 setER clamp, R3 setHostBPM refresh on sync, R5 state-load clear, R7 setProcessingMode clamp

**Tier 2 deferred:** (none — all Tier 2 adds shipped this round per Jeff direction)

**Tier 3 (post-v1.0):**
- **Shimmer / pitch-shift feedback** — octave/user-selectable pitch shift in feedback path. Requires pitch shifter (phase vocoder or granular). **PRESET-SAFE** — new params default to "off" when loading v1 presets. Valhalla Supermassive / ShimmerVerb territory.
- **Ducking reverb (dynamic feedback)** — env follower on input sidechains feedback gain, so wet ducks on loud transients. Common in vocal busses. **PRESET-SAFE** — new params (threshold, amount, attack, release) default to off.
- **Algorithm presets (Hall / Chamber / Plate / Room / etc.)** — preset-chooser combo that nudges existing FDN params into algorithm-specific voicings. Could optionally also swap FDN topology (Householder matrix instead of Hadamard). **PRESET-SAFE** — default "Custom" mode matches current v1 behavior; user's saved param values stay intact.
- **Per-tap ER editor UI** — right-click the ER knob to open a visual editor for individual tap times + gains. **PRESET-SAFE** — adds new optional serialization of custom tap config; missing → uses default prime-ratio pattern.
- **Dedicated Reverse / Gated modes** — different output topology (reverse-envelope the decay, or noise-gate the tail). **PRESET-SAFE** — new mode enum value.

### §9 SaturationDSP
**Shipped in v1:**
- F1 full rewrite (prior session): Flowers (tanh even harms) + Dabs (3 tube types A/B/C) + Sensitivity + BassRelief 350 Hz split + Transformer + Tone Pre/Post (10 kHz shelves) + Wet/Dry + Out.
- **Phase A retrospective (2026-04-18):**
  - **A1** `juce::ScopedNoDenormals` in process() - multiple IIR states + DC blocker
  - **A2** CPU guards on all 10 setters (value-change comparison; previously all wrote unconditionally)
  - **A5** Panel A9 construct-time state sync: `tubeTypeSel` reads `dsp->mTubeType`, `transformerTog` reads `dsp->mTransformer`, new `autoGainTog` reads `dsp->getAutoGain()`, new `osSel` reads `dsp->getOversamplingLog2()`. Cross-apply from last session's A9 pattern.
  - **A7** `processTube` Type B: `std::pow(abs(t), 2/3)` replaced with `std::cbrt(t*t)` - mathematically identical (within float precision), ~3x faster. PRESET-SAFE.
  - **9a** 4x oversampling around the tube engine. IIR half-band polyphase (low-latency); `mOversampler` holds 2-channel state; reports `getLatencySamples()` = `oversampler->getLatencyInSamples()` for PDC. Process restructured into Phase 0 (consume smoothers into scratch arrays) + Phase 1 (base-rate Sens + TonePre -> mBandBuf) + Phase 2 (oversampled: Bass split + tube + relief blend + recombine, all at OS rate via `processSamplesUp`/`processSamplesDown`) + Phase 3 (base-rate DC block + TonePost + Wet/Dry + OutGain). Both low and high bands processed inside OS domain to avoid latency-mismatch comb filtering. Bass-split `mBassLPCoef` computed at OS rate (re-derived on `setOversamplingFactor`).
  - **9b** Auto-Gain compensation toggle. New `mAutoGain` bool + `setAutoGain(bool)` + `getAutoGain()`. When on, final out-gain multiplier = `outGainLin / sensGain` so Sens drive is decoupled from output volume. Default off = v1 behavior. Exposed via new `DualLabelToggle` "Auto MU" on panel. PRESET-SAFE.
  - **9c** Sample-rate-aware DC blocker replaces hardcoded `kDCR = 0.9975f`. `mDCCoef = 1 - 2*pi*5/sr` computed in `updateFilters()`; targets 5 Hz cutoff at any sample rate. Old 44.1-kHz-only behavior (~17 Hz at 48k) eliminated. PRESET-SAFE.
  - **9d + C1** `juce::SmoothedValue<Linear>` on all 8 continuous knobs: Flowers (15 ms), Dabs (15 ms), Sensitivity (20 ms, linear-domain), BassRelief (20 ms, 0..1 fraction), TonePre (20 ms, linear gain), TonePost (20 ms, linear gain), Wet (15 ms, 0..1 fraction), OutGain (15 ms, linear). Sens/TonePre/TonePost/OutGain smooth linear-domain gains (not dB) to avoid per-sample `dbToGain` cost. `processTube` refactored to `static float processTube(x, flowers, dabs, tubeType, transformer)` - takes per-sample smoothed params instead of class members. All smoothers snapped to targets on state-load. Spec called for 4 (Flowers/Dabs/Sens/Out); C1 extended to all 8 for consistency with Chorus/Delay/Flanger/etc.
  - **C2** Oversampling factor chicken-head (2x/4x/8x/16x, default 4x). `mOsLog2` stored 1..4. New `setOversamplingFactor(int)` reallocates the oversampler via `prepare()`. UI `osSel` on panel. `mBassLPCoef` re-derived since it uses OS rate. Direct mirror of §6 Overdrive C5. PRESET-SAFE (default 4x matches spec 9a).
  - **C4** Auto-Gain compensation dB readout. Small label next to Auto-Gain toggle shows `-sensDb` (e.g. "-6.0 dB") when toggle is on, blank when off. `getAutoGainCompDb()` returns the computed value; panel `juce::Timer` at 10 Hz keeps label synced against automation-driven Sens changes too.

**Tier 2 deferred:** (none this round - all Tier 2 items shipped)

**Tier 3 (post-v1.0):**
- **T1 Asymmetric sigmoid as Tube Type D** - spec's original proposal `f(x) = (x + k*x^2*sgn(x))/(1+|x|)` added as 4th tube type. Predominantly 2nd/4th even harmonics, asymmetric clipping character (Fruity-saturator-style). **PRESET-SAFE** new enum value (TubeType int range extends 0..3).
- **T2 Pre/de-emphasis EQ curves** - guitar-amp style frequency-shaped distortion. Pre-EQ before the shaper, inverse post-EQ after. Shapes WHICH frequencies distort rather than how hard. **PRESET-SAFE** new params default to flat.
- **T3 Multi-stage cascade shaper** - 2-3 shapers in series (same type or different). Creates richer, denser harmonic content. **PRESET-SAFE** new mode enum.
- **T4 Multi-band saturation** - split signal into 3 bands (low/mid/high), saturate each independently with its own Flowers/Dabs/Type. Standard on high-end saturators. **PRESET-SAFE** new mode enum.
- **T5 Character voicing presets** (Tube / Console / Tape / Fuzz) - preset BUNDLES (not new params) that set Flowers/Dabs/TubeType/Transformer/BassRelief/Tone Pre/Post to classic voicings. **PRESET-SAFE** (just preset files). **Depends on: effect-panel preset loader UI** (see Cross-cutting section below).

### §10 TapeDSP
**Shipped in v1:**
- Prior state before this session: minimal `tanh(satGain*x) + LP + linear-interp wow + white-noise hiss`. 6 params. Spec-tagged as "biggest single rework in review set."
- **Full rewrite 2026-04-18 (Phase A retrospective + §10 spec bundle):**
  - **A1** `juce::ScopedNoDenormals` in process() - 4 shelf LP states + hysteresis accumulators + 7-stage pink filter + HPF-200 state + DC-block state; denormal-heavy.
  - **A2** CPU guards on all 14 setters (value-change comparison).
  - **A3** `while`-wrap on wow + flutter LFO phases (replaces `std::fmod`).
  - **A5** removed dead `maxWowSamples;` statement (stale "suppress unused warning" that did nothing).
  - **A6** hiss formula made symmetric between L and R (previously L was unconditional, R had `mHiss > 0` check). Now both use pink-filter + HPF 200 path with independent `juce::Random` streams per channel for natural stereo decorrelation.
  - **A4/A9** Panel construct-time DSP-state sync for `tapeSpeedSel` (reads via `getTapeSpeed()` -> nearest of 3 positions) and `osSel` (reads `getOsLog2()`).
  - **10a Hysteresis state variable** - `mHystL/R` accumulates toward shaper output at OS rate: `y += alpha * (shaped - y)`. Gives tape its magnetic-memory character. alpha computed from time-constant model (SR-aware): `tauMs = 5 * exp(-5.5 * tapeSpeed)` so TapeSpeed=0 -> tau=5ms (slow/squishy), TapeSpeed=1 -> tau=0.02ms (fast/nearly instantaneous).
  - **10b Asymmetric sigmoid shaper** replaces `tanh`: `(x + k*x^2*sgn(x)) / (1 + |x|)` with `k = 0.3 * mVibe`. Even-harmonic-heavy. **PRESET-BREAK** ⚠️ (sonic character shift; pre-v1 clean slate, no in-the-wild presets).
  - **10c 4x oversampling** around shaper + hysteresis (steps 3 + 4 of chain). IIR half-band polyphase. PDC via `getLatencySamples()`. Process restructured into Phase 0 (consume smoothers into scratch) + Phase 1 (base-rate InGain + pre-emphasis -> mBandBuf) + Phase 2 (OS: bias injection + asymmetric shaper + hysteresis accumulator, per-sample at OS rate) + Phase 3 (base-rate de-emphasis + wow/flutter with cubic interp + pink hiss + DC block + out gain). Oversampler is 2-channel (stereo; mono input padded in Phase 1).
  - **10d Separate Flutter LFO + smoothed noise** - new `mFlutterRate` (5-25 Hz, default 15) + `mFlutterDepth` (0-1, default 0.15) params. Per-sample: `flutter = 0.8 * flutterSin + 0.2 * flutterNoiseState` where flutterNoiseState is a 1-pole LP smoother on white noise (~300 Hz cutoff). Total modulation sum: `wow + 0.3 * flutter` so flutter is naturally shallower than wow.
  - **10e Cubic (Catmull-Rom) interpolation** on wow/flutter delay reads via new static `cubicCircular` helper. Replaces the previous linear interp. Smooths out zipper artifacts at short delay times under heavy modulation.
  - **10f Pre/de-emphasis shelf pair** replaces the old single LP. Pre-emphasis: +6 dB @ 5 kHz before shaper. De-emphasis: -8 dB @ 4 kHz after shaper. Shelves use the "LP blend" formula `shelf = G*x + (1-G)*LP(x)` (same pattern as §9 Saturation tone shelves). **PRESET-BREAK** ⚠️ (removes `mLPCoef` field from state; sonic character changes; pre-v1 clean slate).
  - **10g Pink-filtered hiss + 200 Hz HPF** replaces raw white noise. Per-channel 7-stage Paul Kellet pink filter (members `mPinkL/R` each holding `b0..b6` biquad-chain-like state) + 1-pole HPF @ 200 Hz per channel. Independent `juce::Random` streams per channel give natural stereo decorrelation. **PRESET-BREAK** ⚠️ (hiss character changes; pre-v1 clean slate).
  - **10h 5 Hz SR-aware DC blocker** before final output. `mDcCoef = 1 - 2*pi*5/sr` computed in `updateFilters()`. Per-channel `dcX/dcY` states. Same pattern as §9c Saturation.
  - **10i SmoothedValue on all 10 continuous params** (Vibe, HystAmount, Bias, PreShelfGain, DeShelfGain, InGain, OutGain, WowDepth, FlutterDepth, Hiss). Linear-domain for gains, 0..1 fractions where applicable. Consumed at base rate in Phase 0 into scratch arrays; held constant across the 4x OS inner loop (so 15-20 ms time-constants stay user-intuitive regardless of OS factor).
  - **C1 Hysteresis Amount knob** (0..2, default 1) - multiplier on the computed alpha. Exposes hysteresis as its own control separate from TapeSpeed. 0 = no memory (shaper output passes through), 1 = spec default, 2 = strong memory (slow tracking).
  - **C2 Oversampling factor chicken-head** (2x/4x/8x/16x, default 4x). New `osLog2` int param. `setOsLog2` reallocates the oversampler via `prepare()`. Mirror of §6 Overdrive C5 / §9 Saturation C2.
  - **C3 Pre/de-emphasis gain knobs** (user-adjustable ±12 dB). Defaults +6 dB (pre) / -8 dB (de) match the spec's hardcoded values. Lets users dial tape brightness independently of Vibe.
  - **C4 Tape speed chicken-head** (7.5 ips / 15 ips / 30 ips, default 15). Wraps existing `mTapeSpeed` 0..1 param at values 0.0 / 0.5 / 1.0. Matches pro tape emulation UX (Slate VTM, Waves J37). Genre-authentic metaphor for the speed-vs-hysteresis relationship.
  - **C5 Bias knob** (0..10, default 5). Pre-shaper DC-offset injection (`biasedX = x + (bias - 5) * 0.02`). At 5 = neutral (pure asymmetric shape driven by Vibe/k). Extremes inject even-harmonic-heavy DC bias before the shaper. 5 Hz DC blocker catches any residual at the end.
  - **Signal chain:** Input gain -> Pre-emphasis shelf -> [OS: Bias inject -> Asymmetric shaper -> Hysteresis accumulator] -> De-emphasis shelf -> Wow+Flutter delay (cubic interp) -> Pink-filtered hiss + HPF 200 -> DC blocker -> Output gain.
  - **Panel**: 11 knobs over 2 rows (Vibe / Hyst / Bias / InGain / Hiss on row 1; WowHz / WowDp / FlutHz / FlutDp / PreShf / DeShf on row 2) + 2 chicken-heads on row 1 right (TapeSpeed 7.5/15/30 + OS 2x/4x/8x/16x). Output-gain knob stays deleted per L2B (per-slot fader).

**Tier 2 deferred:** (none this round - all Tier 2 items shipped)

**Tier 3 (post-v1.0):**
- **T1 IR-based cassette frequency profile** (spec 10j, explicitly deferred there) - `juce::dsp::Convolution` with Type I / Type II deck impulse responses. Replacement or supplement to the emphasis shelf pair. PRESET-SAFE.
- **T2 Type II / IV cassette variant presets** - preset bundles with different emphasis curves + hiss characteristics (Type I = Fe2O3, Type II = CrO2/high-bias, Type IV = metal). Depends on **Effect-panel preset loader UI** (see Cross-cutting section). PRESET-SAFE.
- **T3 Isolation mode** - mode enum to bypass individual stages: "hear just the hysteresis" / "hear just the emphasis" / "hear just wow+flutter" / etc. A/B each DSP stage for learning and tweaking. PRESET-SAFE new enum.
- **T4/T5/T6 bundle - advanced tape realism:**
  - **T4 Multi-head simulation** - 3-head tape machines have record + playback head offset; adds a short parallel delay with its own gain for the "live monitoring through tape" character. PRESET-SAFE new mode.
  - **T5 Drop-outs** - brief random amplitude dips simulating aged/damaged tape. PRESET-SAFE new param.
  - **T6 Print-through** - ghost pre-echoes from adjacent tape layers (signal leaks between wraps). PRESET-SAFE new param with very short lookahead delay.

### §11 TransientShaperDSP
**Shipped in v1:**
- Prior state: dual-envelope detection (fast+slow peak followers), linear gain formula, 1-pole LP band-split, base-rate tanh drive, mono-sum envelope detection. 9 params.
- **Phase A retrospective + full spec bundle + C adds (2026-04-18):**
  - **A1** `juce::ScopedNoDenormals` in process() - envelope integrators + LR4 filters + drive saturation are denormal-prone.
  - **A2** CPU guards on all setters (value-change comparison; was unconditional writes everywhere).
  - **A4** `setAttack` cleaned up - removed dual-range kludge that accepted both -1..1 and -100..100. Panel always sends -100..100; setter now uniformly `a * 0.01`.
  - **A5** Panel A9 for chicken-heads. `attackShapeSel` + `releaseShapeSel` now read `dsp->mAttackShape` / `dsp->mReleaseShape` on construct via `setSelectedIndex(dsp->mField, dontSendNotification)` (was hardcoded to index 1 regardless of DSP state). New `osSel` + `stereoDetectTog` also sync from DSP. Cross-apply from last session's A9 pattern.
  - **A7** Dead `mFastR` + `mSlowR` (peak) state removed. Replaced with `mFastR` (only used when StereoDetect on) + `mSlowMs` / `mSlowMsR` (mean-square RMS accumulators, per 11e).
  - **11a Quadratic attack + sustain curves.** Gain formula changed from `1 + attack*t + sustain*s` to `1 + attack*t^2 + sustain*s^2`. Snappier feel per spec. Bipolar sign preserved through the square (negative attack/sustain still produces correct sign in the product). **PRESET-BREAK** ⚠️ (sonic character shift; pre-v1 clean slate).
  - **11b Linkwitz-Riley 4th-order crossover** replaces the 1-pole LP band-split. `juce::dsp::LinkwitzRileyFilter<float>` x2 (LP + HP). Phase-perfect recombination (no magnitude hole at crossover). Split frequency applied via `setCutoffFrequency` on both filters (cheap per-block). Minor group delay (~1-2 ms at 48k); treated as 0 for PDC per spec (imperceptible on a drum tool). `mLPStateL/R` members removed. **PRESET-BREAK** ⚠️ (band-split frequency response shifts from 6 dB/oct to 24 dB/oct; pre-v1 clean slate).
  - **11c 4x oversampling around the drive stage.** `juce::dsp::Oversampling<float>` wraps Phase 2. Oversampler ALWAYS runs regardless of drive value, giving constant latency via `getLatencySamples()`. When `drive < 0.001`, the OS loop inner math is skipped (identity pass-through), preserving latency but saving the `tanh` calc. Default 4x (C1 makes it user-selectable 2/4/8/16x).
  - **11d SmoothedValue on 8 continuous params** (Attack, Sustain, Sensitivity, SplitFreq, Balance, Drive, OutGain as linear, Wet). 15-20 ms ramps. SplitFreq smoother drives LR4 `setCutoffFrequency` per-block (cheap). Balance smoother computes `(bal + 100) / 200 -> highWeight 0..1`. OutGain smoother holds LINEAR gain (not dB) to avoid per-sample `dbToGain` cost.
  - **11e Slow envelope uses RMS detector** (fast stays peak). `mSlowL` peak follower replaced with `mSlowMs` mean-square accumulator: `slowMs += coef * (level^2 - slowMs)`; RMS extracted as `sqrt(max(slowMs, 1e-12))` at use. Transient signal = `fast - slowRms` (difference, per spec's "keep as difference, not ratio" guidance). Per-channel when StereoDetect on (`mSlowMsR`). **PRESET-BREAK** ⚠️ (envelope detection behavior shifts; pre-v1 clean slate).
  - **C1 OS factor chicken-head** (2x/4x/8x/16x, default 4x). `setOsLog2` reallocates the oversampler via `prepare()`. Mirror of §6 C5 / §9 C2 / §10 C2. PRESET-SAFE.
  - **C2 Stereo envelope detection toggle** (default off = current mono-sum behavior). When on, fast + slow envelopes run per-channel, and gain is computed per-channel. On = better for stereo drum buses, overheads, room mics with asymmetric transients. Off = classic mono-sidechain feel (v1 default). PRESET-SAFE.
  - **C3 Dry/Wet mix knob** (0-1, default 1.0 = current 100%-wet). Enables parallel transient shaping workflow. Smoothed (15 ms). PRESET-SAFE.
  - **C4 FastRel + SlowAtt exposed as user knobs** (1-50 ms each, default 10 ms each matching prior hardcoded values). Previously the shape presets (Sharp/Med/Soft) only controlled ONE end of each envelope; the OTHER end was fixed. Now both ends are user-settable. Fine-tuning power for drum-specific envelope shaping beyond the 3 preset shapes. PRESET-SAFE.
  - **Signal chain:** Phase 0 (consume smoothers -> scratch for Wet/OutGain) -> Phase 1 (per-sample detection + quadratic gain + LR4 split + high-band shape + balance crossfade + recombine -> mBandBuf) -> Phase 2 (oversampled drive saturation; always constant latency) -> Phase 3 (wet/dry mix against original dry + output gain).
  - **Panel:** 7 knobs row 1 (Attack/Release/Sens/Split/Balance/Drive/Gain) + AttackShape + ReleaseShape chicken-heads on right. 3 knobs row 2 (Wet/FastRel/SlowAtt) + OS chicken-head + StereoDetect toggle on right.

**Tier 2 deferred:** (none this round - all Tier 2 items shipped)

**Tier 3 (post-v1.0):**
- **T1 Multiband transient shaper** - 3+ bands with per-band attack/sustain shaping. Standard on high-end transient tools (FabFilter Pro-Q with transient enhancer, SPL Transient Designer Plus). PRESET-SAFE new mode.
- **T2 Look-ahead detector** - preemptive attack boost without distorting peaks. Small latency cost. Works similarly to Limiter look-ahead. PRESET-SAFE.
- **T3 Sidechain input** - external-trigger shaping (e.g. sidechain a kick into a bass to emphasize its transient). Shares infrastructure with §2 Compressor's `sidechainSourceId` scaffolding. PRESET-SAFE.
- **T4 Character voicing presets** (Punchy / Tight / Soft / Retro) - preset bundles of Attack/Release/Sens/Shapes/Wet/Drive combinations tuned for common drum-shaping goals. PRESET-SAFE. Depends on effect-panel preset-loader UI (see Cross-cutting section).

### §12 EQ8DSP
§12 is the largest spec in the DSP review (10 sub-items including Linear-phase mode and full Dynamic EQ). Split across phases per scope. Phase 1 shipped this session.

**Shipped in v1 (Phase 1 - 2026-04-18):**
- **A1** `juce::ScopedNoDenormals` in process() — up to 64 IIR filter states per EQ instance (8 bands x 4 sections x L+R), denormal-prone.
- **A2** CPU guards on all band setters (`setBand*` now short-circuit on no-change; 12c smoothers + dirty-flag pattern delays coef rebuilds to block rate).
- **A3** CPU guards on misc setters (`setMainLevel`, `setPhaseMode`, `setLinearPhasePrecision`, `setIIRModSpeed`, new `setProportionalQ`).
- **A6** `mSpareLocked` made private; added `isSpareLocked()` const getter. Encapsulation cleanup; no callers were touching the field directly.
- **A8** `anySoloed()` result cached per-block in `mAnySoloedCached` — computed once in process() instead of re-scanning 8 bands inside `bandShouldProcess` for every band. Was O(n^2) per block; now O(n).
- **12a** `getMagnitudeForFrequency(float freq) const noexcept` + `getMagnitudeForFrequencyDb(float freq) const noexcept` methods. Iterates enabled bands, multiplies each active section's `Coefficients::getMagnitudeForFrequency`, applies main-level trim. Null-guards unused section coefs. Thread-safe const read for UI-thread curve drawing. Returns 1.0 / 0 dB when sample rate not set.
- **12b** Proportional Q on Peaking bands (SSL/Neve "hardware" feel). Formula: `propQ = userQ * (1 + |gainDb|/18)` so ±18 dB gives up to 2x narrower Q. Only Peaking (type 0); shelves/filters/notch/BP use user Q unchanged. Toggle via `setProportionalQ(bool)` default true. **PRESET-BREAK ⚠️** pre-v1 (sonic character of Peaking bands shifts when toggle is on; default-on). Serialised as `proportionalQ`.
- **12c** `juce::SmoothedValue<float>` per band on `freq` / `gainDb` / `q`. Setters now call `setTargetValue` instead of synchronous `updateBand()`. In process(), smoothers advance by `numSamples` per block (`.skip(numSamples)` + `.getCurrentValue()` — block rate, not per-sample, because biquad coef rebuilds-per-sample would be wildly expensive). If smoother delta > tolerance OR `dirty` flag set (from type/slope/propQ changes), band rebuilds coefs at block start. `lastAppliedFreq/GainDb/Q` track what coefs were last built for to avoid redundant rebuilds. Eliminates automation click-artifacts on freq/gain/Q sweeps. Mechanical refactor; PRESET-SAFE.
- **12d** `mIIRModSpeed` wired as smoothing ramp length. Formerly stored-but-never-read (dead param, like §3 Delay's `mModCutoffMod` was); now maps 0..1 to 1..50 ms ramp via `0.001 + speed * 0.049`. `refreshSmootherRamps()` helper recomputes on change or at prepare(). Default 1.0 = 50 ms (conservative silent-automation feel). Serialised as `iirModSpeed`. PRESET-SAFE.
- **Misc cleanup:** `setStateInformation` now snaps smoothers to restored values (no ramp across potentially-large deltas on preset load); `refreshSmootherRamps` called on state-load to reflect any restored `iirModSpeed`. `swapWithSpare` snaps smoothers to the swapped-in band's params so compare-bank A/B is instant.

**Blast radius confirmed safe:**
- `EQ8MsDSP` wrapper unchanged — calls `mid().prepare/process/getState/setState` etc., all my changes preserve those entry points.
- All `EQ8MsDSP` consumers (`VibeGraph::getLayersBusEQ/getBassBusEQ/getDrumsBusEQ/getMasterEQ/getEffectsBusEQ/getAudioClipsBusEQ`, `getLayerPageEQ`, `getBassPageEQ`, `getAudioRowEQ`, `getInstrChannelEQ`, new `getInsertEQ`) unchanged.
- `ParametricEQDisplay` in SharedUI.cpp has its own `evalBandDb` path (per-band magnitude). 12a's new combined-magnitude method is available but not yet wired to the widget; Phase 2 will consolidate.
- `PluginProcessor.cpp` APVTS-driven EQ updates (~30 setBand*() call sites across LayersBus, DrumsBus, BassBus, LayerPages, BassPages EQs) already guard with `!=` checks; my new setters also internally guard. No behavior change.

**Shipped in v1 (Phase 2 Session A - 2026-04-19):**
- **12h** Per-band M/S routing. New `EQ8DSP::Channel` enum (`Stereo=0, Mid=1, Side=2, LOnly=3, ROnly=4`) + `Band::channel` field. `EQ8DSP::process()` rewritten with per-band channel dispatch: L/R-domain pass for Stereo/LOnly/ROnly bands, then on-demand M/S-domain pass using a pre-allocated `mMsScratch` scratch buffer when any enabled band uses Mid or Side. Biquad chains are LTI so grouping bands by channel domain preserves the transfer function. New `setBandChannel(int, int)` setter with CPU guard + filter state reset on channel change (avoids transient click when the filter's input domain flips L -> M, etc). Serialised as `channel` int per band in EQ8DSP state; fallback to current default on missing field. Defaults seeded by EQ8MsDSP ctor (mMid bands -> Mid, mSide bands -> Side) so today's behaviour is reproduced exactly when no band is re-routed. **PRESET-SAFE** (additive, all new defaults match prior behaviour; 12h itself intentionally did not break presets).
- **EQ8MsDSP wrapper kept (Option A)**. Inner M/S encode/decode dropped from the wrapper's `process()`; both inner EQ8DSPs now receive the FULL stereo buffer and handle per-band routing internally. Wrapper continues to be the natural 2x8 = 16-band container for storage + serialisation + UI binding. All ~120 `.mid()` / `.side()` call sites in `PluginProcessor.cpp`'s `updateLayersEQ` / `updateDrumsEQ` / `updateBassEQ` + per-layer / per-bass page update loops stay exactly as they were. Zero churn outside the wrapper internals.
- **APVTS `_Channel` params** added additively. `layers_mid_eqN_Channel` / `layers_side_eqN_Channel` / `drums_mid_eqN_Channel` / `drums_side_eqN_Channel` / `bass_mid_eqN_Channel` / `bass_side_eqN_Channel` (Int 0..4, defaults Mid=1 on mid bands, Side=2 on side bands) in the bus layouts. `tk_<id>_<ch>_eqN_Channel` / `tk_bass_<id>_<ch>_eqN_Channel` added to the per-page lazy-register path in `addParamsForTrackEQ`. Read path added to `updateLayersEQ` / `updateBassEQ` / `updateDrumsEQ` / `updateLayerPageEQsFromApvts` / `updateBassPageEQsFromApvts` with the standard `!=` CPU guard. PRESET-SAFE (additive; defaults reproduce today's behaviour).
- **Internal MID/SIDE pill deleted** from `ParametricEQDisplay`. `mMidSideBtn` creation removed; `showMidSideToggle(bool)` is now a no-op; `bindDSP` / `bindMsDSP` / `setShowMid` null-guard the removed widget. External page-header MID/SIDE buttons still drive `setShowMid()` from outside (EffectsPage / LayersPage / BassPage / DrumsPage -> `mEQDisplay->setShowMid()`), which is the intended UX per CLAUDE.md's "External MID/SIDE Button Pattern". Layout reserves toolbar space only when `mMidSideBtn != nullptr` (always false now), so the toolbar tightens up at the top-right.
- **12i** Spectrum analyser overlay. Each `EQ8MsDSP` instance now owns its own `preFeed` + `postFeed` `SpectrumFeed` members, populated at the stereo-I/O boundary of its `process()` (mono-mixdown pushed before the EQ runs, and again after). Extracted `SpectrumFeed` into new `Source/DSP/SpectrumFeed.h` header so `EQ8MsDSP` can embed it without a circular include on `VibeGraph.h`; `VibeGraph::SpectrumFeed` kept as a `using` alias so every existing `VibeGraph::SpectrumFeed` reference still compiles. Standalone `mLayersEQFeed` / `mDrumsEQFeed` / `mBassEQFeed` fields removed from `PluginProcessor`; `VibeGraph::buildFixedTopology` signature dropped the three `SpectrumFeed&` params; bus-node `feed.push()` calls between pageEQ and rack removed (feeds now live inside their EQ). Widget side: new `pushSamplesPre` method + `mFifoBufferPre` / `mSpectrumDbPre` / `mFifoIndexPre` / `mSpectrumPreReady` state; `drawSpectrum` refactored to draw pre (translucent white alpha 0.06 / 0.22) behind the existing post (VC::Green alpha 0.09 / 0.28); `syncFromDSP` polls `mBoundMsDsp->preFeed` and `->postFeed` every timer tick when bound, routing them to `pushSamplesPre` / `pushSamples`. `EffectsPage::timerCallback` now calls `mEQDisplay->syncFromDSP()` (pre-existing gap - the Effects Page EQ was never polling its feed before). Coverage: every EQ instance in VibeDAW now has pre+post feeds via the shared `EQ8MsDSP` infrastructure. PRESET-SAFE.

**Session A polish pass (2026-04-19, same session):**
- **Spectrum axis fill.** `ParametricEQDisplay::drawSpectrum` extends the curve path horizontally to the graph's left edge (20 Hz) using the first in-range FFT bin's dB, and to the right edge (20 kHz) using the last. At 48 kHz / 1024-pt FFT the first usable bin is ~47 Hz so low-end content is flat-extended; if a higher-res mode is wanted later, bump `kFFTOrder` 10 -> 12 (tradeoff: ~43 ms -> ~85 ms refresh interval). Common helper `buildSpectrumPaths` shared between pre + post curves.
- **Spectrum poll runs during drag.** Moved the pre/post `SpectrumFeed` poll ABOVE the `mSyncing || mUserDragging` early-return in `syncFromDSP`. Previously the guard blocked the feed poll too so the analyser froze while holding a band handle; guard still skips the DSP-to-UI band-value sync (which we don't want during drag).
- **DSP-to-UI band sync skipped when MsDSP has no APVTS write-back** (`mBindMode == MsDSP && mMsDSPApvts == nullptr`). Defends against a race on EffectsPage's simple `bindMsDSP` path for Layers/Bass/Drums bus EQs: without write-back, `setAPVTSFromBand` can't propagate user edits to APVTS, `processBlock::updateXxxEQ` resets the DSP to APVTS defaults every block, and the timer would pull those defaults back into `mBands` on every tick. Now the widget holds `mBands` as the user-authoritative source and the DSP-reset loop is ignored visually. **Pre-existing audio bug logged for Session B**: on EffectsPage with bus EQs selected, user drags still don't change audio because the simple bind never writes APVTS; fix = use the full `bindMsDSP` overload with the correct prefix per selected channel (naturally part of Session B automation-wiring scope).
- **`syncBandFromControl` now calls `pushBandToDSP`.** Root cause of the slider-and-knob reset-to-default bug: sliders' `onValueChange` called `setAPVTSFromBand` (APVTS write) but NOT `pushBandToDSP` (direct DSP write). Graph-handle drag called both. So between a slider change and the next `processBlock` (~0-11 ms), the 30 Hz `syncFromDSP` could poll the stale DSP value and snap the slider back. Graph-handle drag was protected by `mUserDragging` + the direct DSP write; sliders had neither. Fix: sliders now match the graph-handle contract - both APVTS and DSP are updated on every `onValueChange`.
- **EQ gain faders switch to mixer-style metallic cap.** New `eqFader` property on `juce::Slider::getProperties()` triggers a copy of the mixer-fader render path in `VibeLAF::drawLinearSlider`, parameterised for the EQ's bipolar -18..+18 dB range. Tick labels at -18/-12/-6/0/6/12/18 (no "+" prefix - position on the symmetric scale implies sign, and dropping it solved a narrow-column clipping problem that hid the positive labels entirely). Labels render at 9.5 pt via `drawFittedText` with `minimumHorizontalScale=0.5` so JUCE horizontally squeezes glyphs when the box is tight rather than clipping them. Zero-dB tick highlighted red.
- **Live fader position pointer.** Short horizontal amber/red line at the cap's current Y, pointing from the dB-tick zone to the cap's left edge. Amber when off-centre, red when within 0.05 dB of unity. Makes the exact current value visually pegged onto the scale.
- **Per-band readouts below each control.** Three new 10 px strips in the right-panel column layout (`mGainReadoutR` / `mFreqReadoutR` / `mQReadoutR`) populated by `resized()` and rendered by `paint()`. Gain: `+3.5` / `-12.0` / `0.0` (amber / red at unity, 9.5 pt bold). Freq: `440` / `1.0k` / `12k` (scale-adaptive format, 9 pt). Q: `0.71` / `2.00` / `10.00` (9 pt). Gain readout strip is translated up 5 px via `.translated(0, -5)` to breathe away from the freq knob below it - moves into the visually-empty margin between the fader's drawn ticks and the readout position (confirmed not to collide with the cap even at min gain). Readout in `drawLinearSlider` removed; only the pointer remains in the slider paint.
- **Reset Band right-click now restores freq + gain + Q + slope** (was gain + Q + slope only). Uses per-band default freq from `kEQDefaultFreqs[band]` = 40/250/500/1k/2k/4k/8k/12k Hz. Type / on / mute / solo / channel deliberately left untouched - only the frequency-response shape resets, not routing config.

**Shipped in v1 (Phase 2 Session B - 2026-04-19):**
- **Universal lazy-register of EQ band params on every mixer strip.** `ensureMixerStripParams(prefix, kind, ...)` now also calls `addParamsForTrackEQ(prefix)`, so every Master / Bus / Insert / Aux strip automatically gets `<prefix>_mid_eq{0..7}_{Freq,Gain,Q,Type,On,Slope,Mute,Solo,Channel}` + `<prefix>_side_eq{0..7}_*` registered the first time the strip is ensured. `addParamsForTrackEQ` also rounded out with the previously-missing `Mute` + `Solo` + `Channel` (Session A added Channel; Mute / Solo were never there, so `updateLayerPageEQsFromApvts` / `updateBassPageEQsFromApvts` were silently reading them as 0.f / off). Coverage: 6 buses (layers / bass / drums / master / fx / clipsbus) + up to 94 inserts (8 layer + 4 bass + 16 drum + 50 audio + 16 aux) = up to 100 EQ instances × 144 APVTS params each. All PRESET-SAFE (additive; defaults match prior hard-coded DSP defaults).
- **Generic `updateEQFromApvts(EQ8MsDSP*, midPrefix, sidePrefix)` helper in `PluginProcessor.cpp`** replaces ~400 lines of hand-rolled `updateLayersEQ` / `updateBassEQ` / `updateDrumsEQ` etc. (existing functions kept since their call sites are already tested; new helper is used for post-rack coverage). Reads 9 params x 8 bands x 2 sides with the standard `!=` CPU guard; skips bands whose `_Freq` param isn't registered (unregistered insert strip). Guards against mid-prefix + side-prefix both being empty.
- **`updateAllPostRackEQsFromApvts()` iterator** walks all 6 bus getters + all 5 InsertKind x up-to-max-index slots. Called once per `processBlock` after the existing update-functions. `getInsertEQ` returns nullptr for unregistered indices so inactive strips contribute zero overhead. Estimated cost: ~2 % CPU for ~10 active strips, scaling linearly with strip count.
- **Widget `Band::channel` field** in `ParametricEQDisplay::Band` struct (plumbed through `syncFromAPVTS` / `setAPVTSFromBand` / `pushBandToDSP` / `syncFromDSP` so the widget's local mirror of a band's state now carries the routing decision).
- **Right-click band-handle menu gains two submenus:** `Channel` (Stereo / Mid / Side / L Only / R Only - check-marks the current routing, clicks write via APVTS) and `Automate` (9 items for Freq / Gain / Q / Type / On / Slope / Mute / Solo / Channel - grayed-out when the widget has no APVTS write-back; click fires `VKnobAutomation::sOnAutomate(paramId)` with the correct `<prefix><b><suffix>` id).
- **`registerAutomationForBoundEQ()`** called from `bindMsDSP(eq, apvts, midPrefix, sidePrefix)`. Registers `VKnobAutomation::sOnRegisterApplicator` + `sOnRegisterReader` for every paramId under the prefix pair (2 sides x 8 bands x 9 suffixes = 144 paramIds). Applicator does `setValueNotifyingHost` with a clamped 0..1; reader does `RangedAudioParameter::getValue()`. Idempotent (re-registering overwrites). Wired so Event Editor can drive every EQ band param just like any other automated knob.
- **EffectsPage now uses the full `bindMsDSP` overload with per-channel APVTS prefix.** `onChannelChanged` computes the prefix via existing `getMixerApvtsPrefixForChannel(id)` and passes `<prefix>_mid_eq` / `<prefix>_side_eq` to the widget. Fixes the pre-existing audio bug logged in Session A: drags on Effects Page bus EQs now actually affect audio (they write APVTS -> `updateAllPostRackEQsFromApvts` reads them back onto the DSP). Falls back to the simple bind only when `eq == nullptr` or the prefix is empty (stub/unrecognised channel id).
- **Option C channel badge on band handles.** Rendered in `drawHandles` when `b.channel != defaultForSide` (mid-view default = Mid, side-view default = Side, bare-DSP default = Stereo). Small amber 12x10 px rounded chip at upper-right of the dot with a single letter (`St` / `M` / `S` / `L` / `R`). Unchanged bands show no badge (clean graph); re-routed bands flag themselves visually. 7.5 pt bold letter on amber background.
- **Dynamic hover tooltip via `juce::TooltipClient`.** `ParametricEQDisplay` now inherits `TooltipClient`; `getTooltip()` composes a multi-line readout from `mHoveredBand`:
  ```
  Band 3 - Peaking
  Freq:    1.20 kHz
  Gain:    +4.5 dB
  Q:       0.71
  Channel: L Only
  (SOLO)
  ```
  Empty string when no band is hovered (tooltip hides). The existing in-graph mini hover-tip (Q + slope name) is retained - it pops immediately on hover without the 700 ms tooltip-window delay, complementary to the full tooltip. PRESET-SAFE (pure UI).

**Phase 2 ORIGINAL (now history for this section):**
- **12h** Per-band M/S routing - SHIPPED above.
- **12i** Spectrum analyzer overlay - SHIPPED above.
- **EQ band automation wiring** - Session B (deferred).

**Shipped in v1 (Phase 3 - 2026-04-19):**
- **12e** TPT (State-Variable, zero-delay-feedback) filter engine for LP (type 1), HP (type 2), BP (type 7) bands. Peaking / Shelf / OFF / Tilt keep biquad (TPT has no native gain control or shelf mode). **Notch stays biquad** (`juce::dsp::StateVariableTPTFilter` has no native notch mode; BP-subtract would cost 2x state for a filter type rarely used at extreme Q). `BandState` gains parallel `tptL` / `tptR` arrays + `useTPT` derived flag + `tptCutoffHz` / `tptResonance` per-section cache (used by the magnitude-query UI path). `process()` dispatches per-band based on `useTPT`: TPT calls `processSample(0, x)` (each instance prepared `numChannels=1` to mirror biquad structure for 12h channel-routing state semantics); biquad path unchanged. `updateBand()` LP/HP/BP branch sets SVF mode + cutoff + resonance using the same Butterworth cascade Q table (`kSteepQ`) as biquad - JUCE's SVF `setResonance()` matches biquad Q convention directly. `setBandChannel` + `setBandType` reset both biquad and TPT state (type swap between engines is the new case; channel swap was already handled for biquad). `getMagnitudeForFrequency` gets a closed-form SVF magnitude branch (analog prototype |H(jw)| per LP/HP/BP, cascaded across sections) since `StateVariableTPTFilter` exposes no magnitude query. `prepare()` seeds unity defaults (`lowpass @ 20 kHz, Q=0.7071`) only when `sampleRate > 0` (guards against host `prepare(0)` initialization calls NaN'ing the TPT internal `g` coefficient). **PRESET-BREAK ⚠️ pre-v1** (slight sonic shift on existing LP/HP/BP bands at high Q near Nyquist: biquad coefficient warping was the old behaviour; TPT keeps the analog shape there). Tier 2 C1 (user-selectable engine override) and C2 (Notch via TPT BP-subtract) both deferred to Tier 3.

**Shipped in v1 (Phase 3 - 2026-04-19):**
- **12j** Full Dynamic EQ. 7 new per-band fields (`dynamic`, `threshold`, `ratio`, `attack`, `release`, `rangeDb`, `upward`) + Option B sidechain scaffolding field (`scSourceId`, default -1 = internal). Parallel-detector architecture (all dynamic bands see pristine original input, not in-series-filtered signal, matching FabFilter Pro-Q Dynamic style). Detector = bandpass at band.freq / band.q (updates with band freq/Q at block rate). Envelope follower with asymmetric attack/release time constants (1-pole smoother per channel, stereo-linked peak detection). Gain computer: downward compression above threshold OR upward expansion below threshold when `upward=true`, with ratio scaling + range clamp. Per-band `std::atomic<float> currentGrDb` for UI polling. Block-rate coef rebuild via `updateBand(i, effectiveGain)` override (no smoother state disturbance). Supported only on gain-bearing types (Peaking=0, LowShelf=3, HighShelf=4, Tilt=8); LP/HP/Notch/BP ignore dynamic flag. `getMagnitudeForFrequency` branches for dynamic bands: builds coefs on-the-fly with effective gain for the animated curve. Full state serialisation (all 8 new fields in the XML tree, additive PRESET-SAFE on missing). APVTS: 8 new per-band suffixes (`Dynamic`, `Threshold`, `Ratio`, `Attack`, `Release`, `Range`, `Upward`, `ScSource`) registered via `addParamsForTrackEQ` so every ~100 EQ instances gets them lazily. 7 of 8 automatable via Session B's right-click "Automate: ..." menu (ScSource is a routing id, not a continuous control). Widget side: Band struct gains matching fields + `currentGrDb` live-polled from DSP; sync/push/APVTS paths carry them; right-click menu gains "Make Dynamic" (toggle) + "Dynamic Params..." (opens CallOutBox popout); `DynamicParamsPopout` file-scope class with 5 rotary sliders (Threshold / Ratio / Attack / Release / Range) + Upward toggle + live GR meter (30 Hz timer), all APVTS-attached and componentID-tagged so automation + type-in-value work on them; `drawHandles` adds a small vertical GR bar next to each dynamic handle (orange below = downward, green above = upward); `drawCurve` adds a faint dashed orange ghost outline showing the range-endpoint effective gain alongside the live animated curve; `evalBandDb` uses effective gain (design + current GR) for dynamic bands by default, with optional override for the ghost-path render. **Bundled UX polish:** gain fader greyed out (via `setEnabled`) when the band type is LP / HP / Notch / BP / Off - types without a gain parameter. Prevents the previous drag-and-snap-to-0 UX where the DSP would silently reject the change. PRESET-SAFE throughout (`dynamic=false` default preserves v1 behaviour exactly; sidechain scaffolding always reads internal).

**Shipped in v1 (Phase 3 - 2026-04-19):**
- **12f** 2x oversampling anti-cramping (opt-in per EQ instance, default off). `std::unique_ptr<juce::dsp::Oversampling<float>> mOversampler` allocated in `prepare()` (2 ch, 1 stage, IIR half-band polyphase, useIntegerLatency=true). `bool mAntiCramping` + `setAntiCramping(bool)` setter; toggle re-runs `prepare()` so TPT instances (which store sample rate internally) and oversampler scratch resize for the new design rate. New `designSr()` helper returns `mAntiCramping ? 2*sr : sr`; threaded through `updateBand` / `makeOneSection` for the main filter coefs. Detector path stays at HOST rate (mDetInput snapshot + envelope follower + GR computer all run pre-bracket; detector biquap built with host sr in `updateBand`'s dynamic block, and detector unityBp seed in `prepare()` uses host sr). `process()` refactored: extracted band dispatch into new `processBands(juce::dsp::AudioBlock<float>&)` so the same code runs on either the host-rate buffer (AC off) or the upsampled block (AC on). Bracket pattern: `mOversampler->processSamplesUp(hostBlock)` -> `processBands(upBlock)` -> `processSamplesDown(hostBlock)`. M/S encode/decode runs INSIDE the bracket on upsampled samples (per Jeff's T2b). `mMsScratch` sized for `2*maxBlockSize` so AC toggling never reallocates on the audio thread. New `EQ8DSP::getLatencySamples()` override returns `ceil(mOversampler->getLatencyInSamples())` when AC on, 0 otherwise. New `EQ8MsDSP::getLatencySamples()` returns mid+side latency (sequential processing on same buffer). New `EQ8MsDSP::setAntiCramping(bool)` pass-through to both inner EQs (per-instance scope per Jeff's T2c). `getMagnitudeForFrequency` biquad branch + dynamic-band local rebuild branch both pass `designRate` (= 2*sr when AC on) so the displayed curve matches the audible response; TPT branch unchanged (analog-prototype magnitude is sample-rate-agnostic). UI: new "Anti-cramping (2x OS)" item in `ParametricEQDisplay::showEQOptionsMenu` (case 6), shown only when a DSP is bound (DSP or MsDSP mode); APVTS-only mode hides the toggle. Click flips `setAntiCramping` on the bound DSP/wrapper then fires new `onLatencyChanged` callback wired by every page (EffectsPage / DrumsPage / LayersPage / BassPage) to `mProcessor.setLatencySamples(mProcessor.mVibeGraph.updateBusLatencies())`. **Cross-system PDC fix bundled in (T2d audit):** `VibeGraph::updateBusLatencies()` now sums `rack + busEq` per bus instead of just rack - pre-existing bug that was benign while every EQ reported 0 latency, would have drifted the moment AC went on. Per-insert / per-instr-channel PDC remains a separate gap (out of scope this session). State serialisation: `antiCramping` bool added to EQ8DSP's XML state tree (PRESET-SAFE additive; defaults false on missing). EQ8MsDSP roundtrips it via the existing base64-wrapped inner EQ state (no wrapper-level change needed). `setStateInformation` calls `setAntiCramping(acSaved)` when the loaded value differs from the current state so TPT/oversampler re-prepare runs. CPU: 2x when enabled, hence opt-in. PRESET-SAFE additive throughout.

**Shipped in v1 (Phase 3 - 2026-04-19):**
- **12g** Linear-phase / HQ modes. PhaseMode (existing UI-only enum) now actively drives DSP behaviour: 0 Standard (today's IIR path), 1 Linear (linear-phase FFT @ 2048-pt, no oversampling), 2 HQ Plus (AC=on forced; no FFT path), 3 HQ Linear (AC=on forced + linear-phase FFT @ 4096-pt), 4 HQ Extended (linear-phase FFT @ 512-pt, low-latency variant). New `EqLinearPhaseProcessor` class (`Source/DSP/EqLinearPhaseProcessor.h/.cpp`): 50%-OLA Hann-windowed FFT processor, per-channel input ring + output OLA ring, IR stored as N/2+1 real magnitude bins (zero phase = linear phase), forward FFT -> per-bin magnitude multiply -> inverse FFT -> synthesis-window + OLA. Hand-rolled (NOT `juce::dsp::Convolution`) so IR rebuild is cheap (just a magnitude callback walking N/2+1 bins; no time-domain IFFT/load step). Latency = FFT/2 reported via `getLatencySamples()` and summed with AC oversampler latency in `EQ8DSP::getLatencySamples()`. EQ8MsDSP wrapper sums mid+side latency unchanged - doubles per-instance. `EQ8DSP::setPhaseMode` now actually acts: forces AC on/off for HQ Plus / HQ Linear, calls `prepare()` to resize TPT + oversampler + linear-phase processor for the new design rate, marks IR dirty, arms a mode-aware output fade scaled to `min(latencyDelta, 4096)` with floor 256 (T2c) so the seam is masked even on Standard->HQL flips that shift latency by ~2050 samples. New `reconfigureLinearProc()` lazy-allocates / frees the linear processor based on the current phase mode + design rate. New `magnitudeForFrequencyStatic()` helper builds the IR from band design gain only (no GR injection) so dynamic-EQ behaviour is excluded from the linear IR per T2b option C. Per-band M/S routing restricted to Stereo in linear modes per T2a option B - the linear processor convolves combined L+R independently. UI: `ParametricEQDisplay::showEQOptionsMenu` Processing Mode submenu now (a) appends a live `[+N sp]` latency readout to each mode label so the user sees the cost before picking (T2e), (b) writes the chosen mode to `mBoundDSP->setPhaseMode` / `mBoundMsDsp->setPhaseMode` then fires `onLatencyChanged` so the host PDC refreshes via the per-page wiring already established by 12f. Right-click band-handle Channel submenu greys all 5 routing items + relabels "Channel  (disabled in Linear modes)" when the EQ is in any linear mode. Same treatment for Make Dynamic + Dynamic Params... menu items (relabeled "Make Dynamic  (disabled in Linear modes)"). `syncFromDSP` now pulls phaseMode from the bound DSP each tick so the popup checkmark is correct after preset load / external mode change. State serialisation: `phaseMode` int added to EQ8DSP's XML state tree (PRESET-SAFE additive; defaults Standard on missing); `setStateInformation` calls `setPhaseMode(saved)` when loaded value differs so the linear processor reconfigures + AC re-forces. EQ8MsDSP roundtrips via existing base64-wrapped inner state. Spectrum analyser pre/post taps documented as having `kFFTSize/2` offset in linear modes per T2f option C (no DSP change; visible offset is small and not gameplay-critical). CPU per linear EQ instance: ~1% (HQE 512), ~3-5% (Linear 2048), ~5-8% (HQL 4096); doubled by M/S wrapper. Cross-system PDC fix from 12f session continues to apply (`updateBusLatencies` sums rack + busEq per bus). Tier 3 added: T15 multi-IR linear-phase per-band M/S, T16 per-block dynamic IR rebuild in linear-phase, T17 user-selectable FFT size, T18 IR crossfade transitions. **§12 EQ8 now complete: 10/10 spec items shipped.**

**Phase 3 remaining:** *(none - all 4 deferred items shipped 2026-04-19)*

**Tier 2 deferred:** (none this round for Phase 1; Phase 2/3 items above are the deferrals by spec ordering)

**Tier 3 (post-v1.0):**
- **T1 Orfanidis analytical anti-cramping** - alternative to 12f oversampling. Corrects bilinear-transform warping via coefficient design, zero latency. Already tracked under Cross-cutting (section below); cross-referenced here. PRESET-SAFE.
- **T2 Match-EQ** - analyze reference track spectrum, auto-suggest EQ curve to match source to reference. Pro-mixing feature (FabFilter Pro-Q Match, iZotope Ozone). PRESET-SAFE new mode.
- **T3 Auto-suggest cuts** - detect frequency masking between tracks, suggest cuts to improve separation. Modern AI-assisted mixing feature. PRESET-SAFE new mode.
- **T4 EQ preset bank** (vocal / guitar / bass / drums tonal presets) - depends on effect-panel preset-loader UI. PRESET-SAFE.
- **T5 M/S spectrum analyzer overlay** - per-band M/S spectrum display. Depends on 12h (per-band M/S routing) + 12i (spectrum analyzer). PRESET-SAFE.
- **T6 User-selectable filter engine per band** (12e Tier 2 C1, deferred 2026-04-19). Right-click band handle -> "Filter engine -> Biquad / TPT / Auto". Default "Auto" maps to type-driven choice (current behavior). New per-band serialised field `useTPTOverride`. Lets power users force biquad on LP/HP/BP if they prefer that flavor. PRESET-SAFE (additive with default-Auto). ~80 lines incl. right-click menu wiring.
- **T7 Notch via TPT BP-subtract** (12e Tier 2 C2, deferred 2026-04-19). Puts Notch (type 5) into the TPT engine using `y = x - BP(x)` with 2x filter state per notch band. **PRESET-BREAK ⚠️** if enacted (sonic shift on existing notch bands - biquad notch and SVF-derived notch have slightly different bandwidth/asymmetry at extreme Q). Consistency-of-engine argument; real-world benefit small since notches usually sit at moderate Q on specific problem frequencies where biquad is perfectly stable.
- **T8 TPT shelves** (12e Tier 3 T1 from scoping, 2026-04-19). Research-grade: SVF can approximate shelves via mode-mixing (LP+HP blend with gain scaling), but it's not a drop-in replacement for biquad `makeLowShelf` / `makeHighShelf`. PRESET-SAFE (new mode option; biquad shelves stay default).
- **T9 Ladder / Moog-style filter engine** (12e Tier 3 T2 from scoping, 2026-04-19). Third per-band filter topology option alongside biquad + TPT. Warm analog character, self-oscillates at high resonance. PRESET-SAFE (additive engine choice).
- **T10 Bandpass Q remapping** (12e Tier 3 T3 from scoping, 2026-04-19). Biquad BP Q and SVF BP "resonance" don't have the same physical meaning - user who types Q=5 gets different bandwidth depending on engine. Add a compensation curve so "Q" reads the same across engines. PRESET-BREAK if retro-applied to existing TPT-routed bands.
- **T11 External sidechain detector source for Dynamic EQ** (12j Option B, 2026-04-19). Per-band `scSourceId` APVTS scaffolding is shipped in 12j (default -1 = internal; serialised; preserved in presets). DSP today always reads internal band input. Needs the cross-cutting Sidechaining Infrastructure session (see `Sidechaining infrastructure` entry below) to land first - that session wires channel-output-taps into VibeGraph, adds the UI channel-picker dropdown in the Dynamic Params popout, and compensates for source PDC latency. When it ships, existing presets with `scSourceId=-1` stay on internal exactly as before. PRESET-SAFE transition.
- **T12 Variable oversampling factor for anti-cramping** (12f Tier 3, 2026-04-19). Today AC is fixed at 2x (1 stage IIR halfband). Promote to a chicken-head selector (2x / 4x / 8x) in the EQ options popup with matching `mOversampler` reallocation in `prepare()` + PDC re-report. 4x/8x give measurably better Nyquist behaviour on Peaking / Shelf / Notch at extreme freqs but cost proportionally more CPU + latency. PRESET-SAFE additive (new `osFactor` int field; defaults to 1 = 2x for old presets).
- **T13 FIR-equiripple oversampler option for AC** (12f Tier 3, 2026-04-19). Alternative to today's IIR half-band polyphase: linear-phase FIR equiripple at the cost of ~10x latency. Useful when phase coherence between sources matters more than latency (mastering chains). Add as a second popup item ("Anti-cramping (linear-phase)") that swaps the oversampler filter type. PRESET-SAFE additive (new `acFilterType` int; defaults 0 = IIR for old presets).
- **T14 Auto-enable AC heuristic** (12f Tier 3, 2026-04-19). When any band's freq exceeds `sr/4` AND its type is gain-bearing (Peaking / Shelf / Notch where biquad cramping is most visible), automatically enable AC and surface a small badge in the EQ options popup explaining why. Toggle the heuristic via a third popup item ("Auto anti-cramping"). PRESET-SAFE additive.
- **T15 Multi-IR linear-phase per-band M/S** (12g Tier 3, 2026-04-19). Today's 12g restricts per-band M/S routing to Stereo when the EQ is in any linear-phase mode (Linear / HQ Linear / HQ Extended). Channel field is preserved in state but mode-gated at process time + greyed in UI. T15 = build up to 4 parallel linear-phase IRs (combined Stereo, Mid, Side, L-only with R-only folded in), convolve in 4 paths inside the M/S encode/decode bracket, and re-enable per-band channel routing in linear modes. CPU ~4x today's single-IR linear path. PRESET-SAFE additive (existing channel state already preserved).
- **T16 Per-block dynamic IR rebuild in linear-phase** (12g Tier 3, 2026-04-19). Today's 12g disables dynamic EQ when the instance is in any linear-phase mode (UI greys Make Dynamic; DSP no-ops the dynamic flag for linear modes). T16 = rebuild the linear-phase IR every block to incorporate current GR per dynamic band, so the animated curve stays correct in linear modes. CPU ~5-10% per dynamic band per linear-phase EQ instance. Two implementation sub-options: (a) full per-block rebuild (FabFilter Pro-Q "linear phase + dynamic" model — heavy but correct), (b) snapshot at block start with smoothed update (cheaper, slight visual lag). PRESET-SAFE (dynamic-band state already preserved).
- **T17 User-selectable FFT size for linear-phase modes** (12g Tier 3, 2026-04-19). Replace the per-mode default FFT sizes (Linear=2048, HQ Linear=4096, HQ Extended=512) with a chicken-head selector in the EQ options popup: 256 / 512 / 1024 / 2048 / 4096 / 8192. Override per EQ instance, defaulting to the per-mode value. Power-user latency vs precision tradeoff knob. PRESET-SAFE additive (new int field defaults to per-mode default - old presets unchanged).
- **T18 Linear-phase IR crossfade transitions** (12g Tier 3, 2026-04-19). Today's 12g rebuilds the IR on band changes (drag, type swap, dynamic GR) and the new IR replaces the old one whole. Audible artefacts during fast drags. T18 = double-buffer the IR + crossfade old to new over a few hundred samples so band drags sound smooth in linear-phase. PRESET-SAFE (pure runtime behaviour change).
- ~~**T19 Reclaim ParametricEQDisplay's 22-px toolbar row**~~ (SHIPPED 2026-04-19 with the A/B compare overhaul - 22-px row removed; `[SPARE]` migrated into the new BankIndicator pill in the PageMenuBar's extra-right slot).
- **T20 Extract shared `EQ8Defaults` header** (2026-04-19). The 8-band default-frequency array currently lives in TWO places that must stay in sync: `kDefaultFreqs8` in `Source/DSP/EQ8DSP.cpp` (DSP-side seed for `mBands` and `mSpare`) and `kEQDefaultFreqs` in `Source/Standalone/SharedUI.cpp` (widget-side defaults for `setDoubleClickReturnValue`, Reset Band right-click, band creation). 2026-04-19 they drifted (DSP had 80/200/500/2k/6k/16k/8k/4k vs widget 40/250/500/1k/2k/4k/8k/12k) and caused the A/B compare bug where B bank loaded with DSP-side defaults different from A's widget-side defaults. Manually realigned + added memory rule to prevent future drift, but the proper fix is a shared header (e.g. `Source/DSP/EQ8Defaults.h` exposing `kDefaultBandFreqs[8]`) included by both EQ8DSP and ParametricEQDisplay so the array exists in exactly one place. Also a natural home for default Q (0.707), default type (0/Bell), default slope (0), etc. PRESET-SAFE pure refactor.

---

## Future Effect Modules (post-v1.0)

All items in this section are new effects that do not currently exist in VibeDAW. Every one is **PRESET-SAFE by construction** (no existing presets to migrate - they're net-new modules). Ordered by category; pairings with existing VibeDAW LAFs noted where natural. Implementation dependencies noted where existing codebase support (e.g. `PhaseVocoder`, `juce::dsp::LinkwitzRileyFilter`, `juce::dsp::Oversampling`) reduces the lift.

### 1. Dynamics
*Focuses on amplitude logic, envelope following, and threshold-based gain manipulation. When designing skeuomorphic rack gear, these pair perfectly with classic VU meters and metallic indicator LEDs — natural fit for the existing `DynamicsLAF` cream/LA-2A style.*

- **Gate / Downward Expander** - essential utility for silencing noise floors or tightening drum bleed. Uses an inverted compression envelope where signal is attenuated *below* a threshold. Reuses existing Compressor envelope/detector infrastructure.
- **Upward Compressor** - raises the volume of the quietest parts of the signal without squashing the peaks (crucial for modern, aggressive "OTT" style sound design). Same detector as Compressor but inverted gain-computer.
- **Multi-band Compressor** - splits the signal via `juce::dsp::LinkwitzRileyFilter` crossovers before hitting independent compression algorithms per band. JUCE LR4 already used in `ChorusDSP` crossover - pattern is established.
- **De-Esser** - a specialized frequency-conscious compressor sidechained to trigger only when harsh sibilance (typically 5 kHz - 8 kHz) crosses the threshold. Reuses existing Compressor + adds a tilt/bandpass sidechain tap.
- **Auto-Gain / Vocal Rider** - smoothly levels an audio track over time using JUCE's `BallisticsFilter` with very slow attack and release times, mimicking a mixing engineer riding a fader.
- **Maximizer** - a mastering-grade limiter variation that heavily relies on a look-ahead buffer to smoothly clamp down on transient peaks before they occur, allowing for extreme loudness. Extends the existing §5 Limiter with denser oversampling + LUFS-referenced mastering workflow.

### 2. Harmonics
*Focuses on generating new frequencies, digital destruction, and waveform manipulation. Many of these require `juce::dsp::Oversampling` to prevent nasty aliasing artifacts. Natural fit for `HarmonicLAF` Hammerite / Bakelite aesthetic.*

- **Bitcrusher** - achieved by quantizing the floating-point audio to a lower bit-depth and using a sample-and-hold algorithm to simulate a reduced sample rate. §3 Delay already has a `LoFiBits` knob - could be promoted into its own dedicated module.
- **Wavefolder** - instead of clipping a signal when it hits the threshold (like §6 Overdrive), a wavefolder folds the waveform back in on itself, creating complex, metallic synthesizer tones. Requires oversampling; reuse the §6/§9/§10 pattern.
- **Exciter / Harmonic Enhancer** - uses a high-pass filter fed into a subtle distortion circuit, blending newly generated high-order harmonics back into the dry signal to add "air" without boosting EQ peaks. Similar topology to §9 Saturation's BassRelief split, inverted (highs get excited, not lows).
- **Ring Modulator** - multiplies the incoming audio signal by a carrier oscillator (usually a sine wave), yielding sum and difference frequencies that sound robotic or bell-like. Trivial DSP; needs a dedicated LFO.
- **Subharmonic Generator (Octaver)** - tracks the pitch of the incoming audio and synthesizes a sine or square wave exactly one or two octaves below it to beef up low-end. Pitch tracker is non-trivial (FFT peak or autocorrelation).
- **Comb Filter** - a very short delay (under 20 ms) mixed heavily with the dry signal, creating a series of deep frequency notches that resemble a comb. Could be added as a mode to §4 Flanger or stand-alone.

### 3. Time-Based
*Focuses on manipulating the audio buffer over time. These effects inherently introduce latency or require complex memory management via `juce::AudioBuffer`. Natural fit for `TimeLAF` Pultec-style gear aesthetic.*

- **Convolution (IR Loader)** - uses `juce::dsp::Convolution` to load `.wav` impulse responses. This turns your rack into an authentic guitar cabinet simulator or a hardware-sampled environment. JUCE ships the full implementation; mainly a file-browser + UI task.
- **Pitch Shifter** - alters the pitch of a signal without changing its duration. **VibeDAW already has `Source/DSP/PhaseVocoder.h/.cpp`** (Laroche-Dolson, FFT 2048, 4x overlap) built for the audio clip time-stretch engine. Could be reused directly as the core of a pitch shifter effect.
- **Granular Processor** - chops the incoming audio buffer into tiny, overlapping "grains" (10 ms - 100 ms) and scrambles, stretches, or repitches them for ambient textures. New subsystem.
- **Stereo Widener (Haas Effect)** - delays one side of the stereo signal by a tiny amount (e.g. 10 ms - 30 ms) to trick the human ear into perceiving a massive, wide stereo image. Trivial DSP; could also add M/S width control (already have M/S infrastructure).
- **Reverse Buffer** - continuously records the input signal into a circular buffer and reads it backwards at specified intervals. Similar to §3 Delay's line + a reverse read pointer.

### 4. Modulation
*Focuses on using Low Frequency Oscillators (LFOs) to automate parameters automatically. These require smooth interpolation to prevent clicking. Natural fit for `ModulationLAF` glossy-black/brushed-silver aesthetic (Chorus / Flanger / Phaser panels).*

- **Tremolo** - uses an LFO to rhythmically modulate the amplitude (gain) of the signal. Can be customized with sine, triangle, or square wave LFOs. Simple module; LFO infrastructure from §1 Chorus / §4 Flanger / §7 Phaser is reusable.
- **Auto-Pan** - similar to Tremolo, but uses the LFO to modulate a `juce::dsp::Panner`, bouncing the audio left and right across the stereo field. Same LFO code as Tremolo with a pan rotation instead of amplitude.
- **Vibrato** - modulates the pitch of the signal using a very short, modulating delay line (like a Chorus, but with the dry signal completely muted). Drop-in from §1 Chorus with Dry forced to 0.
- **Envelope Filter (Auto-Wah)** - uses an envelope follower (borrowing logic from a compressor) to modulate the cutoff frequency of a `juce::dsp::LadderFilter` based on how loud the input signal is. Reuses §2 Compressor envelope detector + new LadderFilter.
- **Frequency Shifter** - adds or subtracts a fixed number of Hertz to every frequency in the signal (e.g. +100 Hz). Because it breaks mathematical harmonic relationships, it creates a wildly dissonant, phasing sound. Requires Hilbert transform (analytic signal).
- **Rotary Speaker (Leslie Sim)** - a complex modulation effect that combines Tremolo, Auto-Pan, and Vibrato to simulate a speaker physically spinning in a wooden cabinet. Two independent LFOs (horn + drum), each with their own rate.

---

## Player Engines

### §P1 Harmless (additive)

**Audit baseline (2026-04-19):** Harmless was originally built as a Harmor clone per the design docs in `Files For Claude/Harmless/*.txt` and `Files For Claude/Mega Update Source Files/System/Harmless.txt`. ~30+ APVTS params were registered as ghosts (no DSP wiring) creating a "I turn this knob and nothing happens" problem at scale. UI also had visual stubs (the routing matrix had no APVTS attach call; the Harmonizer panel was missing entirely; the central spectrogram VisualizerScreen.h was an unused stub; multiple knobs visible but unbound; modules with sub-controls layout-compacted because the docs called for elements that weren't built). Session-by-session ship plan adopted to wire it all out without one mega-session: S1 (bugs + small wires + removals + Tier 3 demote), S2 (filter envelopes + LFO routing + Mod XYZ destinations), S3 (full unison engine), S4 (mod-editor 4 tabs + right-click-to-modulate + persistence), S5 (central 516-partial spectrogram + background wavetable + routing-matrix LED-vs-fader final question).

**Shipped in v1 - Session 1 (2026-04-19):**
- **T1a Output EQ Mix wired.** `oeq_mix` param registered (was missing); editor attachment uncommented. New tilt-EQ DSP (low shelf 250 Hz + high shelf 4 kHz with opposite-sign gain, scaled by mix) lives on HarmlessSynth post-voice; bypassed when `mOutputEqMix < 0.001`.
- **T1b flt2_kb_track param registered.** Editor's `mFilter2Row.attachToApvts` would have jasserted on missing param; now exists (DSP wiring deferred to S2 with the rest of filter env work).
- **T1c Filter type wiring (LP/HP/BP/Notch).** `mFilter1Row` + `mFilter2Row` filter type combos previously had no DSP effect (AdditiveVoice always instantiated `lowpass`). New `setFilterType(int)` + `setFilter2Type(int)` on AdditiveVoice maps 0/1/2 directly to JUCE `StateVariableTPTFilterType` (lowpass/highpass/bandpass). Notch (type 3) uses BP-subtract via parallel `mFilterNotchHelper` + `mFilter2NotchHelper` (same cutoff/Q as main; output = input - bandpass). Helpers prepared in ctor + setCurrentPlaybackSampleRate, reset on note start, kept in sync by setFilterParams. `setFilterType` retains user's current cutoff/Q across mode changes.
- **T1d setComponentID on every Harmless slider.** Was zero across the entire `Source/Harmless/` folder - GlobalAutoRightClick filtered every knob out of the right-click "Automate: ..." + "Type in value..." menus. Bulk wired via new `wireMeta(slider, paramSuffix, tooltip)` helper in HarmlessEditor + per-knob calls inside HarmlessFilterRow::attachToApvts.
- **T1e Tooltips on every Harmless slider.** Hover-reveals param name + units in plain ASCII. Knobs that wire later sessions (S2/S3) carry "WIRES IN Sx" suffix in the tooltip so users (and future dev) see the deferred status at a glance.
- **T1f CPU guards on previously-unguarded setters.** `setTremoloParams`, `setVibratoParams`, `setPhaseInit`, `setPitchOffset` on AdditiveVoice now early-return on no-change before assignment + propagation. Per CLAUDE.md standing rule. Cuts unnecessary work in the per-block × 16-voice broadcast path.
- **T1g Removed `juce::Time::getHighResolutionTicks()` syscall from AdditiveVoice::startNote.** Was constructing a fresh `juce::Random rng (Time::getHighResolutionTicks())` on the audio thread for every voice trigger. Replaced with per-voice `mRng` member seeded once in the ctor on the message thread.
- **T1h Mono playback safety.** Existing `if (outR)` already guarded the right-channel write; comment added flagging the pattern. Filter is hardcoded 2-channel in prepare; mono playback wastes the spread but no longer risks UB.
- **T2-D Prism Mode wired.** New `PrismModule::setMode(int)` selects between three inharmonic-spread shapes: 0 stretched (default i^2 piano-like), 1 bunched (sqrt(i) softer spread), 2 scattered (alternating sign per partial - bell/metallic). Called from `HarmlessSynth::setPrismMode` with wavetable rebuild. Editor's existing prism_mode slider now actually affects the spectrum.
- **T2-H Phaser Mask Rate UI knob.** New `mPhaserMaskRate` rotary added to FX row (right of Phaser Rate). Param + DSP already existed - just had no UI affordance. Wired via APVTS attachment.
- **T2-N misc ghost params wired (6 of 8):**
  - `timbre_blend` (A<->B crossfade): new `setTimbreBlend(float)` on AdditiveVoice; multiplies on top of existing partA/B level smoothers (blend=0 leaves levels unchanged, blend=1 pushes everything to Part B).
  - `prism_mode`: covered by T2-D.
  - `pluck_blur` (toggle softens pluck transient): new `PluckModule::setBlur(bool)` halves the effective decay rate when ON.
  - `strum_tns` (-1..+1 tension on strum delay distribution curve): applied in `HarmlessSynth::applyStrum` via `pow(t, exp(-tns*1.5))` - +1 bunches strum at start, -1 bunches at end, 0 linear.
  - `vel_link` (velocity-link bool): scaffolded on HarmlessSynth (`mVelLink`) for S3 per-part velocity routing. Param flows + cached but DSP no-op until S3.
  - `legato_limit` (max glide cap in seconds): scaffolded on HarmlessSynth (`mLegatoLimit`) for S2 filter envelope work. Param flows + cached but DSP no-op until S2.
  - Skipped this session: `part_sel` (editor-side selector - deferred to S2 with per-part work), `flt_env_amt`/`flt2_env_amt` (require filter ADSR DSP - S2).
- **T2-O Layout audit.** Routing matrix gains a layout slot adjacent to the rest of the routing UI per the design doc proportions (40% top-left panel). Phaser Mask Rate knob fitted into FX row between Rate and EQ Mix.
- **T2-I Reverb removed.** `reverb_amount` param dropped from `createParameterLayout` - VibeDAW's effects rack handles reverb on the Layers bus. PRESET-SAFE (param had no DSP).
- **T2-J Multi-band compressor confirmed not present.** Spec called for it but never built; no removal needed. Effects rack handles compression.
- **T2-G Harmonizer demoted to Tier 3.** All 6 `harm_*` params removed from `createParameterLayout` and the design-doc Harmonizer panel deferred to T3 wishlist (when it ships, re-add the 6 params + the panel + DSP).
- **T2-F Routing Matrix wired (faders only - LEDs dropped per Jeff).** Six new APVTS params: `rm_sub`, `rm_prot`, `rm_clip`, `rm_fx`, `rm_vol`, `rm_env` with Harmor-equivalent semantics (sub = octave-down sine partial, prot = high-partial nyquist rolloff, clip = output tanh saturation, fx = wavetable-build FX scale on prism/pluck/blur/filterMask/phaserMask, vol = output gain trim, env = amp-envelope depth on the voice). Defaults preserve current behaviour (sub/prot/clip = 0; fx/vol/env = 1). Sub osc + env depth live in AdditiveVoice; vol + clip live in HarmlessSynth post-voice; fx + prot live in HarmonicEngine wavetable-build path. The 6 LED toggles in the design doc were intentionally dropped per Jeff's "no LED needed" direction; toggles still allocated as field storage but never `addAndMakeVisible`d. `HarmlessRoutingMatrix.attachToApvts` now actually called from `HarmlessEditor` ctor (was a stub-API on a stub-component).
- **HarmonicEngine extensions:** `setSpectralFxScale(float)` snapshot+scale+restore wrapper around each spectral module's amount during `buildWavetable` (preserves user knob values while honouring rm_fx); `setNyquistProtect(float)` adds soft exponential rolloff above partial kNumPartials/4.

**Tier 1 - shipped in S1 above. Future sessions:**

**SLA Audit COMPLETE (2026-04-19, no code changes - planning only):** Per-element review of all 66 documented controls in `Files For Claude/Player Layouts/Harmless.txt` against current code, with Jeff's per-item decisions locked. Outcome:
- **WIRE: 16 elements** spread across SLA-Impl + S2 + S3 + S4
- **DROP: 11 elements** (all PRESET-SAFE; removed entirely)
- **TIER 3: 7 elements** (Harmonizer module + Legato curve toggles + Filter osc knob + Filter secondary shape dropdown + Pluck curve-shape toggle + Phaser shape dropdown + EQ shape dropdown)

**SLA-Impl session (next - small, ~1 sitting):** Pitch group (freq number input + detune number input + fraction selector chicken-head with `1/1 / 1/2 / 1/4 / 1/8 / x2 / x4 / x8` ratios + oct toggle + Hz toggle), Phaser WIDTH knob, Phaser OFS knob, Pluck blur UI button (DSP shipped S1, just needs a TextButton with ButtonAttachment). ~8 new APVTS params + UI, light DSP. ALL PRESET-SAFE additive. Info Bar DROPPED per Jeff's "remove as confusing" - live-value popups cover the use case.

**Updated S2 scope (filter envelopes + LFO routing + XYZ destinations + SLA-bundled items):** T2-A filter envelopes, T2-B LFO routing, T2-E Mod XYZ destinations, **+ Blur time knob + Blur harm knob + Filter ofs knob (per filter, x2) + Prism bipolar range change** (PRESET-BREAK ⚠️ pre-v1 - prism_amount range goes from 0..1 to -1..+1, sign encodes polarity, +/- toggles dropped) + flt2_kb_track DSP wiring. T2-N items (vel_link, legato_limit, part_sel) DSP wiring lands here too.

**S3 (unchanged):** Full Unison engine (T2-C with up to 9 detuned siblings per note + pan/phase/pitch spread) + Unison Type chicken-head DSP behaviour (4 modes) + vel_link DSP + part_sel editor-side selector + mTimbreWavB interactivity.

**S4 (unchanged):** Mod-editor 4 tabs become real (T2-K, PRESET-BREAK ⚠️ pre-v1) + right-click-to-modulate paradigm (T2-L, PRESET-BREAK ⚠️ pre-v1) + target dropdown + tool buttons (snap/step/curve) + modifier knobs (SPD/TNS/SKEW/PW) + TEMPO/GLOBAL radios + viewport tools.

**S5 (unchanged):** Central 516-partial spectrogram (T2-M) + background wavetable rebuild (T2-P).

**DROPPED elements (PRESET-SAFE, removed entirely from v1 scope):**
1. Timbre Fade horizontal slider (#4) - right-click-modulate of timbre_blend in S4 covers
2. Routing Matrix 6 LED toggles (#6) - already done in S1 per Jeff's "no LED needed"
3. Blur 2 curve-routing toggles (#10) - right-click-modulate covers
4. Prism "from vol" toggle (#15) - right-click-modulate covers
5. Filter width knob x2 per filter (#32) - redundant with RES (= Q in SVF)
6. Filter 6 tiny toggles per filter (#37) - Harmor-specific modeling not applicable
7. Phaser 4 toggles oct/Hz/harm/kb.t (#46) - same family, not applicable
8. Global porta knob (#51) - redundant with glide_time
9. Global link/chain toggle (#53) - A/B coupling implicit via timbre_blend
10. LFO pre/post fx labels (#56) - no pre/post distinction in simplified impl
11. Info Bar text area (#59) - "remove as confusing" per Jeff; popups cover

**TIER 3 entries promoted from DROP (post-v1.0 wishlist, all PRESET-SAFE):**
- Legato curve toggles (#24) - 2 buttons cycling glide curve shape (linear / exp / log / S-curve). Default exp is musical; this is a power-user nicety.
- Filter `osc` tiny knob (#35) - oscillator-sync / filter-FM rate per filter. Complex DSP for v1; great character knob for v1.1+.
- Filter secondary shape dropdown (#36) - alternative filter topology (2-pole / 4-pole / ladder / state-variable variants). v1 ships one good filter mode.
- Pluck curve-shape toggle (#40) - alternative pluck-decay envelope shapes.
- Phaser shape dropdown (#41) - LFO waveform pick (Triangle / Sine / Saw / Square). Requires custom phaser since juce::dsp::Phaser is fixed-sine.
- EQ shape dropdown (#47) - alternative output EQ curves beyond tilt (peak / shelf / parametric).

**Tier 1 - shipped in S1 above. Future sessions:**

**S2 (after SLA-Impl):**
- **T2-A Filter envelopes** - `flt_a/d/s/r` + `flt_env_amt` + `flt2_*` ADSRs + `flt2_kb_track` DSP. AdditiveVoice gets per-voice `mFltADSR` + `mFltADSR2`; cutoff modulated by env*amt. Resolves the long-standing TODO comment in AdditiveVoice.h.
- **T2-B LFO routing** - one shared LFO with rate + waveform; depths `lfo_vel`/`lfo_vol`/`lfo_pitch` route to per-voice destinations.
- **T2-E Mod XYZ destinations** - 3 destination dropdowns picking what x/y/z modulate, with per-block apply.
- **T2-N follow-on** - `legato_limit` actually applied in glide-time clamp; `part_sel` editor-side selector swaps which Part the timbre knobs route to; `flt_env_amt` consumed by the new filter envelopes; `flt2_kb_track` gains DSP.

**S3:**
- **T2-C Unison engine** - per-spec up to 9 detuned siblings per note with pan+phase+pitch spread. `setUnisonType` chooses spread shape; `setUnisonAlt` flips alternating-sign detune; `setUnisonPhase` controls per-slot phase offset.

**Shipped in v1 - Session 4 (2026-04-20):** The mod matrix. Per-target articulator envelope + LFO + velocity/keyboard/Mod X/Y/Z sources wired across 16 targets (Volume, Pan, Pitch, TimbreBlend, PluckDecay, PrismAmount, BlurHarm, Filter 1+2 Cutoff/Res, Phaser Mix/Width, UnisonPitch, PartA/B Level). Serialised in `apvts.state` under `<harmlessMod>` ValueTree. Shipped across 6 batches:

- **Batch 1 — Foundation.** New `HarmlessModRegistry` class (enums `ModSource`/`ModTab`/`ModTargetIndex`; `ModCurveState`/`ModSourceState`/`ModTarget` structs). ValueTree serialize/deserialize round-trip. Target registration at processor startup (16 fixed targets; paramIds baked in). APVTS rip: removed `lfo_rate/shape/vel/vol/pitch` + `mod_x/y/z_dest` (8 params ripped - PRESET-BREAK ⚠️ pre-v1). Added `auto_gain_mode` Int param. HarmonicEngine auto-gain DSP pass (Rel/Abs mode) between spectral FX and Brownian weighting. AdditiveVoice lost its old `setLfoParams/Depths/ModDestinations` methods.
- **Batch 2a — Per-voice engines (Option 2 architecture).** Each AdditiveVoice now owns its own `HarmonicEngine` pair (mVoiceEngineA/B), pre-allocated in `setCurrentPlaybackSampleRate`. `activeEngineA/B()` accessors pick between shared template + voice-private copy based on `mUseVoiceEngines`. New `HarmonicEngine::copyStateFrom` + atomic generation counter. Enables true Harmor-faithful per-voice spectral modulation (Option 3 real-time additive deferred to `T3-RealTimeAdditive`). Memory cost: ~1 MB upfront (16 voices × 2 engines × ~32 KB).
- **Batch 2b — Articulator DSP consumption.** `HarmlessModCurve` utility (sample + SKEW/PW/TNS warps + LFO LUT + toBipolar). Per-voice `TargetVoiceState` array (envPhase/lfoPhase/capturedVel/capturedKey/contribution/uniMult). Note-on snapshots registry generation, captures velocity+note, resets phases, computes active flags. Per-block `updateModContributions` sums 7 sources into bipolar `contribution` + unipolar `uniMult` (via product-rule blend across active sources). Voice-local dispatch for 11 targets (Volume, Pan, Pitch, TimbreBlend, Filter 1+2 Cutoff+Res, UnisonPitch, PartA/B Level). VoiceEngine dispatch for 3 targets (PluckDecay/PrismAmount/BlurHarm) via `applyVoiceEngineMods` — rebuilds voice wavetable when contribution moves > 0.01 threshold. SynthLevel dispatch for 2 targets (PhaserMix/PhaserWidth) in HarmlessSynth via "loudest voice wins" aggregation. Volume + Part A/B Level use the **unipolar** mapping so the drawn curve directly scales gain; other targets bipolar (centered on 0 = no change).
- **Batch 3 — Mod editor UI.** Full rewrite of `HarmlessModEditor` around the registry. Articulations dropdown (16 targets) + Modulations dropdown (7 sources). Bottom strip: DEPTH (bipolar center-detent, inverts on negative), LENGTH (13-step musical selector 1/8..32 beats, skew-midpoint at 1 beat), TEMPO toggle, SHAPE rotary (LFO-only), SPD/TNS/SKEW/PW warp knobs (per-target-per-source-per-tab). Grid drawing routes through `HarmlessModCurve::sample` so warps are visually reflected. LFO source with default 2-point curve renders the selected waveform; with 3+ points renders the user-drawn curve. IMG tab moved to T3-HarmlessImgTab. GLOBAL toggle moved to T3-PerPartModRouting. All edits lock `mEditLock` + `publishSnapshot`.
- **Batch 4 — Right-click + orphan cleanup + global LFO.** `VKnobAutomation::sShouldOfferModulate` + `sOnModulateEnvelope` added to SharedUI; GlobalAutoRightClick offers a 3rd "Modulate envelope..." item when the paramId is registered in the mod registry. HarmlessEditor installs+clears hooks in ctor/dtor. Re-added `lfo_rate`/`lfo_shape`/`lfo_tempo` as GLOBAL APVTS params (Harmor-authentic global LFO). Main editor's LFO section restored with RATE/SHAPE/TEMPO knobs; processor change on any of them fires `HarmlessSynth::applyGlobalLfoToAllTargets` which writes to every target's LFO source in the registry (macro). Per-target override still works via mod editor. AG-1 REL/ABS auto-gain toggle in Timbre panel. Orphan XYZ dest dropdowns + old LFO per-dest knobs removed from layout. Cleaned stale "WIRES IN S2/S3" tooltips. Fixed `HarmlessModCurve::applySkew` clamp bug (upper-clamped at 1.0, killed all skew > 0.5). WYSIWYG envelope model with amp-ADSR-release-time release advance (not fixed 100ms); per `feedback_no_preset_system_yet.md`, envelope duration stays a user-set LENGTH until `T3-NoteDurationAwareEnvelopes` lands.
- **Batch 5 — Mod editor tools + viewport + undo/redo.** CURVE/STEP radio (new-point curveType), SNAP toggle (snap to current division). Shift-axis-snap (anchored at drag-start position, not current). Double-click point cycles curveType (linear→smooth→step). Ctrl+Z/Y undo + redo with 100-frame UndoFrame history per target. FREEZE toggle locks edits. +/- zoom (1x..8x) with zoom-aware pointToPixel/pixelToPoint + segment-aware drawCurve that emits each point as an exact path vertex. Division dropdown (1 / 1/2 / 1/4 / 1/8 / 1/16 / 1/32) sets snap target; grid always shows 32 divisions regardless so every snap position is visible. Horizontal ScrollBar below the grid — thumb size = visible phase window, drag to pan. Endpoint anchor protection — right-click delete locks the leftmost + rightmost time points so the curve always has ≥ 2 anchors for the sampler to bracket. Sanitize pass on every mouseUp / target focus / dropdown change (sorts + nudges duplicate-time points apart by 0.002 to prevent "ghost dots").

**Deferred from S4 (T3 entries listed below):** IMG tab (image resynthesis), per-part envelope routing (restores GLOBAL toggle), per-point explicit sustain markers, pattern-scheduled-note-duration-aware envelope timing, mod matrix automation, Harmor ADV tab controls, undo/redo beyond mod editor scope.

**Shipped in v1 - Session 5 / Layout Review (2026-04-20):** Layout review pulled forward from post-S5 because T2-M spectrogram placement required a layout decision. Full top-to-bottom restructure of HarmlessEditor. All knobs + controls migrated to new homes per Jeff's locked map. **Top-left column:** Row A (Output: Volume + Pan + VelLink | Routing Matrix), Row B (Tremolo | Vibrato + Legato), Row C merged (left blank for future | right column: Strum top-1/4, XYZ pad + 3 knobs bottom-3/4). **Top-middle column:** Unison top (knob+button row at current size, 3 faders shortened to fit remainder) / Pitch middle (FREQ + DETUNE + fraction chicken-head + OCT + Hz toggles) / LFO Mod bottom (global RATE + SHAPE + TEMPO macro). **Top-right column:** 5x2 grid (R1 Filter 1 | Filter 2, R2 Timbre + A/B + waveform buttons | Blur/Prism, R3 Amp Env + Phase | FX - Pluck/Phaser/EQ, R4 + R5 blank for future). **Bottom-left panel:** left column two blank stacked tiles (future space), right column single spectrogram visualiser spanning both tiles as one unit. **Bottom-right panel:** mod editor unchanged. New `layoutRow` helper in `HarmlessEditor::resized()` distributes each row's controls evenly across the cell width with equal gap at start / between / end - matches the effect-panel style (was: fixed pixel gaps that left-clustered). `HarmlessFilterRow::resized()` also rewritten for even distribution. New section-rect members: `mUnisonSec`, `mFutureR4LSec / R4RSec / R5LSec / R5RSec` (future-space placeholders in the 5x2 grid), `mFutureBL_TopSec / BotSec`, `mSpectroTopSec / BotSec`. Paint drawSection labels updated. Empty future-space boxes intentionally label-less so they read as "available" rather than "broken".

**T2-M Central spectrogram (S5 shipped):** 516-partial real-time visualiser in the bot-left right column. `VisualizerScreen` component with a 30 Hz timer that polls `HarmlessSynth::getAggregatedPartialAmplitudes(buf, N)`. Aggregator iterates voices, calls each active `AdditiveVoice::accumulatePartialAmplitudes(out, N)` which reads that voice's active Engine A + B `amplitudes[]` arrays weighted by envelope level + per-part A/B level. Harmor-faithful "what you hear = what you see" aggregated across all playing voices. Rendering uses **dB-scaled bar height** (floor -60 dB, full at 0 dB) so quieter upper partials are visible instead of collapsing into the baseline, plus a smoothing decay (max-merge with 0.85x previous tick) for natural peak-fall visual. Thread safety: GUI-thread polling reads audio-thread-written `amplitudes[]` without locks - torn reads at 30 Hz are visually imperceptible. New file `Source/Harmless/VisualizerScreen.cpp` (out-of-line timer callback to keep `HarmlessSynth.h` out of every includer). CMakeLists updated.

**T2-P Background wavetable rebuild (S5 shipped):** UI-initiated wavetable rebuilds now run off the audio thread. `HarmonicEngine` gains `requestRebuild()` (atomic flag) + `consumeRebuildRequest()`. `HarmlessSynth` owns a `juce::TimeSliceThread` + two `WavetableRebuildClient` TimeSliceClients registered for `mPartA` + `mPartB`; background thread polls their request flags every 25 ms and calls `buildWavetable()` off-thread when set. All external-setter call sites in HarmlessSynth.cpp (setPrism/Pluck/Blur/FilterMask/PhaserMask/Brownian/TimeScale/HarmAxis/Mode/... on mPartA + mPartB) switched from direct `buildWavetable()` to `requestRebuild()`. Voice-engine rebuilds (inside `AdditiveVoice::applyVoiceEngineMods` + note-on sync) stay synchronous on the audio thread per Option A — they're sparse (>0.01 contribution threshold) and need immediate audibility to feel "Harmor-live". Thread lifetime: started at synth ctor with `Thread::Priority::low`, stopped with 500 ms timeout in synth dtor. Thread safety around `amplitudes[]` is accepted as-is: a one-frame torn read during a UI-driven setShape is imperceptible, and formal lock is deferred to T3 if it ever surfaces.

**S5 closed (2026-04-20).** §P1 Harmless layout review + S5 both complete.

**Tier 3 (post-v1.0):**
- **T3-Harm Harmonizer module** (demoted from T2 in S1, 2026-04-19). Re-add 6 `harm_*` APVTS params + the 6-control Harmonizer panel (AMT/WIDTH/STR + ofs-step/shift/gap value boxes per the design doc) + spectral-DSP that adds shifted partial copies. PRESET-SAFE additive.
- **T3-Img Image resynthesis** (drag PNG → 516 partials). Stretch goal in spec.
- **T3-Aud Audio resynthesis** (drag WAV → harmonic analysis → 516 partials).
- **T3-Reorder Dynamic spectral unit-order reordering** (Advanced tab — drag to reorder Pluck/Prism/Blur/Filter/Phaser).
- **T3-9Voice Up to 9-voice unison expansion** if T2-C ships at lower count first.
- **T3-Curve Curve editor enhancements** - bezier tension, looping segments, multi-target assignment per envelope.
- **T3-LegatoCurves Legato curve-shape toggles** (SLA #24, 2026-04-19). 2 buttons cycling glide curve shape (linear / exp / log / S-curve). Default exp is musical; power-user character pick. PRESET-SAFE additive.
- **T3-FltOsc Filter osc / oscillator-sync rate knob** per filter (SLA #35, 2026-04-19). Filter-FM modulation rate. Complex DSP for v1; iconic character knob for v1.1+. PRESET-SAFE additive.
- **T3-FltShape Filter secondary topology dropdown** per filter (SLA #36, 2026-04-19). Alternative topologies (2-pole / 4-pole / ladder / state-variable variants). v1 ships one good filter mode. PRESET-SAFE additive (defaults to current SVF when missing).
- **T3-PluckCurve Pluck curve-shape toggle** (SLA #40, 2026-04-19). Alternative pluck-decay envelope shapes (linear / exp / log / soft-exp). PRESET-SAFE additive.
- **T3-PhaserShape Phaser LFO waveform dropdown** (SLA #41, 2026-04-19). Triangle / Sine / Saw / Square LFO pick - requires custom phaser implementation since juce::dsp::Phaser is fixed-sine. PRESET-SAFE additive.
- **T3-EQShape EQ output-curve dropdown** (SLA #47, 2026-04-19). Alternative output EQ curves beyond tilt (peak / shelf / parametric). v1 ships tilt only. PRESET-SAFE additive.
- **T3-PerPartModRouting Per-source A/B/Both routing on the mod matrix** (S4, 2026-04-20). Re-adds the GLOBAL toggle next to TEMPO in the mod editor bottom strip. Purpose: route a single envelope to Part A only, Part B only, or both parts simultaneously — the third axis of Harmor-faithful A/B modulation beyond explicit PartALevel / PartBLevel targets. Adds `int partRouting { 2 }` field on `ModSourceState` (0=A / 1=B / 2=Both). PRESET-SAFE additive (default Both preserves v1 behavior on load).
- **T3-HarmlessADV Harmor-style Advanced envelope tab** (S4, 2026-04-20). The ADV tab from Harmor's mod editor (unit order, poly-rel, ramp, declick, partial count, safety, grain, precision, monophonic mode, randomness, threading, HQ render). Originally §P1 scope, deferred after scoping pass — requires dedicated per-control learn-friendly UI so beginners aren't lost. PRESET-SAFE additive.
- **T3-ModMatrixAutomation Pattern-time automation of mod editor knobs** (S4, 2026-04-20). DEPTH / LENGTH / TEMPO / SHAPE / SPD / TNS / SKEW / PW aren't APVTS-backed (they live in the ValueTree registry) so the existing pattern-automation pipeline can't touch them. Fix: extend the automation system to write ValueTree paths, not just APVTS paramIds. Enumerate mod matrix knobs as virtual automation targets. ~672 new pseudo-params across 16 targets × 7 sources × 6 knobs would bloat APVTS, so the pseudo-target path is the right approach. PRESET-SAFE additive.
- **T3-NoteDurationAwareEnvelopes Pattern-scheduled note duration plumbing** (S4, 2026-04-20). For pattern playback, the scheduler knows each note's duration at schedule time but doesn't pass it through MIDI. Adding this (e.g. custom MIDI CC pair sent alongside note-on) lets envelopes auto-scale to match each note's duration, removing the need for the user to tune LENGTH per-note. Live MIDI still uses the LENGTH knob. Cross-cuts pattern scheduler → MIDI buffer format → processor preamble → voice state → envelope rate. PRESET-SAFE additive.
- **T3-PerPointSustain Explicit sustain markers on curve points** (S4, 2026-04-20). Right-click a point + "Mark as sustain" so the envelope phase pauses at that point while the note is held (ADSR-style). Currently v1 uses LENGTH-only envelope timing (WYSIWYG over the phase range). PRESET-SAFE additive — new `bool isSustain` field on HarmlessCurvePoint.
- **T3-HarmlessImgTab Harmor IMG tab (spectral image resynthesis)** (S4, 2026-04-20). Originally §P1 scope, deferred after scoping pass (13-21 days of work, ~3-5k lines). Full feature set: STFT analyzer for audio-to-spectrogram conversion; 516-partial × time image storage; heatmap renderer; edit tools (paint/invert/flip/degrade/random cloud); dual planes (Gain + Freq); loop markers + ping-pong; Coarse/Fine Speed + Time performance controls + articulator routing; audio/image file drop handling; core DSP rework to continuously update partial amplitudes per-voice from the image (wavetable model becomes optional). PRESET-SAFE additive — new `image` field on `ModSourceState` + source-mode flag defaults to ENV on load. Adds a 2nd tab back to the mod editor.
- **T3-RealTimeAdditive Real-time per-voice additive synthesis** (Option 3 from S4 Batch 2 scoping). Replaces the current per-voice wavetable model (built via IFFT from partial amplitudes) with direct per-sample additive resynthesis (sum of sin(partial.freq × t + phase) × amplitude across 516 partials per voice). True Harmor architecture. Enables per-sample spectral modulation (vs current block-rate wavetable rebuilds). CPU cost: 16 voices × 516 partials × sin LUT + interpolation per sample. Expensive but unlocks finer modulation fidelity. PRESET-SAFE (pure rendering backend swap; preset schema unchanged).

**Notes for future sessions:** the 6 LED toggles on the routing matrix + the Unison Alt button + the Vel Link button were all bound but with no DSP. Per Jeff's "no LED needed" direction in S1, the 6 LEDs were dropped from the matrix; Unison Alt + Vel Link are scaffolded for S3 per-part work. Don't re-add them until the corresponding DSP ships.

### §P2 VibePlayer (+ BaySickDrums inherits)
**Shipped in v1 — §P2 closed 2026-04-21:**

SLA audit + 3 implementation sessions:

- **S1 (DSP, 3 increments):** `cutSelf` bool (MIDI preprocess voice kill), `detuneMode` int (simple/random/pair), `attack`/`sustain` ADSR exposure (were hardcoded), `lfo_rate` wired (was hardcoded 5.5 Hz), `reverse` bool (ReversedMemoryAudioSource file-local class), `sampleStart` float, `velToVolume` float, `unisonVoices` int + `unisonSpread` float, `voiceCap` int, `tune` + `detune` wired into VibeVoice pitch calc. DROPPED `tremolo` orphan param ⚠️ PRESET-BREAK. All new setters CPU-guarded. Per-pitch preemption + stale-note-off strip fix rapid-retrigger voice cascading. Treble range-mapping bug found + fixed (APVTS -12..+12 was hitting a setter expecting 0..1).
- **S2 (UI rewrite):** 6-box grid layout (Sample Engine / Pitch & Voicing / Dynamics / Amp Envelope / LFO / Output). All knobs use ModulationLAF filmstrip; Master Volume uses VibeLAF white-volume filmstrip. DualLabelToggle (OnOff mode) for Cut-Self + Reverse with label colours matched to other VP labels. ChickenHeadSelector for Detune Mode. 24 knobs + 2 toggles + 1 chickenhead wired to APVTS. Popup value display on hover/drag. Section-title labels only (no box borders — dark matte aesthetic preserved, avoids CPU-renderer AA banding). "Open Sample..." added to the preset menu. All file pickers default to `SampleLibrary::getCoreLibraryDir()`.
- **S3 (componentID + automation plumbing + trackId collision fix):** engine ctors refactored from `int trackId` → `const juce::String& trackId` across all 4 engines (Harmless/VibePlayer/BaySickSynth/BaySickBass) + BaySickDrums wrapper. Unique 3-char page prefixes: `lay_{N}`, `bas_{N}`, `drm_{N}` (drums slots propagate as `drm_0_s{N}`). **This fixes a real collision bug** where Harmless on Layers 0 and Harmless on Bass 0 shared `tk_0_harm_*` APVTS params, causing automation to cross-fire between pages. Resolver extended with engine-tag branches (`harm`/`bsp`/`bss`/`bkb` → friendly engine names), page-tab-name lookup via `lookupPageTabName(pagePrefix, pageIndex)` (supports user ribbon-tab renames). Duplicate tab-name auto-suffix applied in 4 rename paths: ribbon tabs + Builder browser (patterns / audio clips / automation lanes — automation uses effective-display-name compare against resolver output, not raw userDisplayName). `setComponentID` on all 24 BaySickPlayer knobs.

**Cross-cutting work shipped during §P2:**
- **Range-mapping audit** (retro) — swept VibePlayer (28 params) + Harmless (60+1 orphan), zero additional mismatches beyond the Treble fix. New standing audit rule: every SLA must verify APVTS range matches DSP setter expectations at both ends.
- **Branding rename** — app renamed VibeDAW → BaySickDAW, VibePlayer → BaySickPlayer in all user-facing strings, CMake PRODUCT_NAME, COMPANY_NAME "KnowledgeBase Studios", BUNDLE_ID. Preset folder paths updated. Windows exe / title bar / About dialog / page title all consistent.
- **Splash screen** — `juce::SplashScreen` with 512×512 half-scaled logo from `Assets/BaySickDAWLogo.png` (embedded via `juce_add_binary_data` + HEADER_NAME/NAMESPACE to avoid BinaryData collision with VibeSynthSamples). 4s auto-dismiss + click-to-skip. Window title-bar icon wired to same logo via `DocumentWindow::setIcon`. Exe ICON_BIG wired so Explorer / taskbar / pinned shortcut show the logo.
- **LRX-5 global vignette disabled** — CPU renderer banding made the radial gradient produce visible ghost-rings. T3 re-enable when GL renderer lands.
- **"No VST" standing rule** — saved as `feedback_standalone_only_no_vst.md`. BaySickDAW is standalone-only; legacy `juce_add_plugin` target remains as build scaffolding but is not shipped.

**Tier 3 logged during §P2:**
- **T3-StretchToNoteLength** (note-duration-aware sample stretch via Rubber Band or PhaseVocoder — needs pattern-scheduler note-duration plumbing, cross-applies to all 4 players)
- **T3-VPFilterUI / T3-VPLoFiUI / T3-VPArticUI** — expose existing READ-ONLY params (`cutoff`, `res`, `reduct`, `artic_group`) as UI knobs
- **T3-VPKnobStyle** — knob-styling audit (Jeff's note post-layout)
- **T3-VPPanRing** — PAN outer-ring indicator from design doc (dropped for v1)
- **T3-VPLoopUI** — SFZ loop-opcode UI
- **T3-VPKbTrack** — keyboard-tracking filter amount (blocked on T3-VPFilterUI)
- **T3-MonoMode** — full Mono Mode (cut-self + legato + portamento + glide-time)
- **T3-DrumsComponentID** — BaySickDrums componentID pass (blocked on §621 drum restructure)
- **T3-LRX5Vignette** (logged under Cross-cutting → LRX realism pass)

**Remaining post-§P2 work (absorbed into §P3 or separate):**
- Cut-self cross-apply retro-patch to Harmless + BaySickSynth + BaySickBass (add `cutSelf` bool + same MIDI-preprocess DSP). Each engine gets its own copy; Harmless is a small retro session, BaySickSynth + Bass land during §P3.

---

**Pre-v1 scope notes (pre-booked, pre-audit):**


---

**SLA Audit lock — 2026-04-20 (scope locked; implementation starts session-S1)**

Audit covered `Source/VibePlayer/*` + `Source/BaySickDrums/*` (inherits VibePlayerEditor) vs `Files For Claude/Player Layouts/VibePlayer.txt`. Per-element table + ghost-param sub-audit + SharedUI automation plumbing check (cross-apply read of `VKnobAutomation::sResolveMenuLabel` + `setComponentID` conventions across Harmless / Mixer strips / EQ widget).

**Findings summary:**
- Design-doc controls: 15/17 fully wired. 2 UI-only ghosts: `tune` + `detune` attached in editor but never read by DSP.
- Orphan APVTS params: `tremolo` (no DSP read, no UI, redundant with LFO amt) and `lfo_rate` (DSP hardcodes 5.5 Hz, param ignored).
- READ-ONLY params (DSP uses, no UI): `cutoff`, `res`, `reduct`, `volume`, `artic_group` — filter/lo-fi/articulation UI is not in the design doc; deferred to T3.
- componentID gap: 0/15 VP knobs call `setComponentID` → no right-click Automate menu, no Type-in-value dialog. Same gap in BaySickSynth / BaySickBass / BaySickDrums (only Harmless + Mixer strips + EQ widget participate today).
- BaySickDrums-specific: drums componentID fix deferred until post-§621 dual-piano-roll + 16-slot restructure lands. No VP→Drums inheritance assumption.

**Scope locked for §P2 implementation sessions:**

*Session S1 — DSP wiring + param hygiene (no new UI):*
- WIRE `tune` → VibeVoice pitch offset at note-on (semitones).
- WIRE `detune` + new `detuneMode` int param (0=simple cents offset / 1=random per-voice spread / 2=pair split). Mode-3 chickenhead UI lands in S2.
- WIRE `lfo_rate` → `VibeVoice::setLfoRate()`.
- Add `cutSelf` bool APVTS param (default false) + MIDI-preprocess voice-kill in `VibeSynth::renderNextBlock` — on every note-on, if cutSelf, inject all-notes-off at same timestamp before the note-on.
- DROP `tremolo` param ⚠️ PRESET-BREAK (approved — redundant with LFO amt).
- CPU guards on every new setter (memory rule).

*Session S1 also adds new APVTS params for S2 UI widgets (wired to DSP now so UI in S2 is pure attachment work):*
- Q4b `volume` already registered+read; needs UI in S2 only.
- #1 `attack` + `sustain` (currently hardcoded 0.001s / 1.0 in updateFromApvts). Extend ADSR exposure.
- #2 `voiceCap` int 1..16 (default 16) — limits `juce::Synthesiser` active-voice count.
- #3 `reverse` bool (default false) — reverses sample playback.
- #4 `sampleStart` float 0..1 (default 0) — skip-into offset on note-on.
- #9 `velToVolume` float 0..1 (default 1.0) — matches existing VEL→MUFFLE / VEL→HARDNESS amount pattern; multiplies `adjustedVelocity`'s contribution to `mVelocityScale`.
- #6 `unisonVoices` int 1..8 (default 1) + `unisonSpread` float 0..100c (default 0).

*Session S2 — New UI controls + grid layout rework:*
10 new widgets added to VibePlayerEditor (volume, LFO rate, cut-self toggle, detune-mode chickenhead, Attack, Sustain, voice-cap, reverse, sample-start, VEL→VOL, unison voices + spread). 4-column grid needs rework — layout decision deferred until all widgets are in the editor so we can see them all at once.

*Session S3 — componentID pass + automation plumbing (standalone VP only):*
- `setComponentID(<full APVTS paramID>)` on every attached slider (prefix already baked in by `VibePlayerProcessor::pid()`).
- Register applicators + readers via `VKnobAutomation::sOnRegisterApplicator` / `sOnRegisterReader`.
- Verify `resolveAutomationDisplayName` handles `tk_{N}_vp_` prefix; extend resolver branch if missing.
- BaySickDrums componentID propagation **explicitly deferred** until §621 drum restructure (no free ride from VP fix).

*Session S4 — Cut-self cross-apply:*
Same `cutSelf` bool pattern retro-fitted to Harmless (§P1 reopened for this one additive change; preset-safe), BaySickSynth, BaySickBass. Drums get it automatically per-slot via VP instance.

**Tier 1 (locked — ship this in v1):**
- All S1–S4 items above.
- Items already pre-booked below (componentID fix for §P2 original pre-booking — superseded by S3 above; per-slot EQ, dual piano roll, dual-engine slots — all preserved as §621 drum restructure items).

**Tier 2 (locked deferred — not in §P2):**
- Choke groups (pre-existing; still T2).

**Tier 3 (logged 2026-04-20 from this audit):**
- **T3-VPFilterUI Filter section UI** — expose existing `cutoff` + `res` params with a filter strip. DSP already wired; purely additive UI work. PRESET-SAFE.
- **T3-VPLoFiUI Sample-rate reduction UI (lo-fi knob)** — exposes existing `reduct` param. DSP already wired. PRESET-SAFE.
- **T3-VPArticUI Articulation A/B/C/D switcher** — 4-way button row for existing `artic_group` param. PRESET-SAFE.
- **T3-VPKnobStyle Knob-styling audit** — Jeff wants to revisit VP knob visuals once everything is wired. Non-functional cosmetic pass.
- **T3-VPPanRing PAN outer-ring indicator** — design doc calls this out; dropped from S2 scope, logged for cosmetic pass.
- **T3-VPLoopUI Loop on/off + loop points** — SFZ regions carry loop points; expose as a global override. Needs SFZ loop-opcode audit first. PRESET-SAFE.
- **T3-VPKbTrack Keyboard-tracking filter amount** — blocked by T3-VPFilterUI. PRESET-SAFE.
- **T3-MonoMode Full Mono Mode (cut-self + legato + portamento + glide-time)** — S1 ships cut-self only. Full mono adds legato (no retrigger on overlap) + portamento (pitch glide). Per-player, parallels cut-self bool. PRESET-SAFE additive.
- **T3-DrumsComponentID BaySickDrums componentID pass** — blocked on §621 dual-piano-roll + 16-slot restructure. Post-§621 only.
- **T3-StretchToNoteLength "Stretch to note length" playback mode (2026-04-21)** — new per-player opt-in toggle (`stretchToNote` bool, default off). When on, sample playback is time-stretched to match the pattern-scheduled note duration: a 1-beat note plays the whole sample in 1 beat, a 4-beat note plays it over 4 beats. Prerequisite: note-duration plumbing from pattern scheduler -> processor (same infrastructure as `T3-NoteDurationAwareEnvelopes`). Stretch engine: Rubber Band (already bundled in `libs/rubberband`) for pitch-preserving stretch, or PhaseVocoder for lighter CPU. Rationale: solves the "I played a 4-beat note but only heard the first 0.5s of audio" pedagogical confusion for newcomers, and makes reverse-playback match visual note length. Counter to sampler convention (Kontakt/HALion do not stretch to note length by default) — ships as opt-in. Cross-applies to all 4 players (VibePlayer/Harmless/BaySick pair) since pattern scheduler is shared. PRESET-SAFE additive.

**Cross-apply notes (for §P3 BaySick family):**
- componentID gap applies identically to BaySickSynth + BaySickBass (0 `setComponentID` calls in either editor). Same S3-style fix pattern.
- Orphan/ghost-param audit recipe applies — scan for APVTS params with no `getRawParameterValue` read + no attachment (Harmless precedent confirmed the pattern exists in player engines generally).
- Cut-self bool lands here in §P2 S4, not §P3.

**Range-mapping audit sweep (2026-04-21, retro):** VibePlayer Treble bug found (APVTS range -12..+12 but setter expected 0..1, making 0-position a full high-cut instead of flat). Fixed to `treble / 12.f` linear map. Follow-up sweep of all VibePlayer (28 params) + Harmless (60+1 orphan) confirmed no other range-mapping mismatches. **New standing audit rule:** every SLA audit must verify the APVTS-declared range matches what the DSP setter expects at both ends (not just "param is registered + read"). Cross-apply to upcoming §P3 BaySick family audits.

---

**Tier 1 — to do at review time (pre-booked from discussion 2026-04-17, + 2026-04-18 additions):**

- **Right-click "Automate" menu gap (2026-04-18 addition).** VibePlayerEditor knobs use `SliderAttachment` but `componentID` isn't set on the sliders -> `GlobalAutoRightClick` can't expose the "Automate: X" menu. All VibePlayer params are registered in APVTS with paramIds - fully automatable from the backend, just no UI affordance. **BaySickDrums:** editor uses ZERO attachments and the drum-slot panel embeds a `VibePlayerEditor` for the selected slot; same componentID gap applies to the embedded VibePlayer sliders. **Fix pattern:** `slider.setComponentID(apvtsParamId)` per attached slider. Confirm `VKnobAutomation::sOnRegisterApplicator` + `sOnRegisterReader` are wired for these paramIds. PRESET-SAFE.
- **A9 slider-sync issue: assumed NOT applicable.** VibePlayer uses `SliderAttachment` (JUCE handles bidirectional sync). BaySickDrums does NOT use attachments itself - its controls are mostly slot selectors + embedded VibePlayer panel. **Verify at review time:** for BaySickDrums specifically, check whether any non-attached slider could diverge from DSP state (unlikely but worth confirming - the drum-engine fields are accessed via the embedded VibePlayer's APVTS which does attach).


- **Cut-self / mono-mode voice management (system-wide).** Default-ON per slot on DrumsPage Sound tab; default-OFF on Layers/Bass Sound pages (per-instance toggle); exposed as right-click option on arrangement audio clips (BuilderPage). Before `noteOn`, kills the same slot's previous voices so repeated triggers don't phase-interfere (fixes DBFS "wobble" artifact on repeated drum hits). PRESET-SAFE — new APVTS param per VibePlayer instance + per audio clip, default preserves current polyphony unless explicitly set. 4 contexts: DrumsPage, LayersPage, BassPage, ArrangementBlock menu.
- **Per-slot EQ on DrumsPage (replace bus EQ).** DrumsPage EQ tab gets a slot-selector master dropdown (same as Sound tab), rebinds `ParametricEQDisplay` to the selected slot's InsertNode EQ. Bus EQ removed (not hidden — removed). Superset case subsumed by the pre-rack EQ addition (see Cross-cutting / Architecture → **Pre-rack EQ on all InsertNodes** below). PRESET-SAFE.
- **Dual piano roll mode on DrumsPage.** Unified per-slot note storage (each slot owns a full-range note list); drum-grid view shows lit cells for ANY note in a slot's lane (pitch-agnostic); full-roll view per-selected-slot via master dropdown. **Click behavior (option D):** drum grid clicks add/remove C5-only; lit-but-no-C5 cells are no-ops (pitched notes only removable via full-roll). Playback: notes play at their stored pitch — C5 = native drum sample, non-C5 = retuned sample. **PRESET-BREAK ⚠️** — drum note storage changes from `pitch = 51-slot` encoding to `{slot, pitch}` pairs. **Must ship in v1** to avoid preset-migration code for v1.1. Tier 1 priority.
- **Ghost notes bug fix (shared PianoRoll component).** Ghost-note feature existed but got broken. Must work across drum-grid + full-roll views in the new dual-mode DrumsPage AND on Layers/Bass/Builder piano rolls. Cross-referenced under System Pages → Piano Roll.
- **Dual-engine drum slots (2026-04-18 addition).** Each `BaySickDrumsProcessor` slot can hold EITHER a `VibePlayerProcessor` (sampler) OR a `BaySickSynthProcessor` (synth). User toggles engine type per slot via a small "Sample / Synth" control in the slot-bar UI. Editor dispatch: DrumsPage embeds `VibePlayerEditor` for sample slots, `BaySickSynthEditor` (full 5-tab) for synth slots. Default = Sample (preserves all current behaviour for fresh kits). Dispatch in the processor: per-slot variant / tagged union holds one or the other engine; `processBlock` walks slots and routes MIDI trigger → appropriate engine. Voice management + PDC + per-slot EQ routing all become engine-type-aware. **PRESET-BREAK** ⚠️ — serialised drum state changes from `16 x VibePlayerState` to `16 x {engineType, engineState}`. Ships in v1 alongside the dual piano roll change (same preset-break window; single migration event). Scope depends on **§P3 Drum Synthesis Expansion Bundle** below — the 6 DSP adds land in BaySickSynth so synth-drum slots have the capability to authentically produce TR-series drums, Simmons drums, FM drums, analog-machine drums, tuned percussion, and modern designer percussion (full capability table under §P3). Ship order suggestion: §P3 DSP adds first, then §P2 dual-engine refactor, then editor swap. Creative payoff for non-musicians: "design your own kicks/snares/hats from scratch with a few knobs" + "load classic sample packs" = both workflows in one kit.

**Tier 2 deferred:**
- **Choke groups** for drums (HH open/closed pairs etc.). Per-slot "choke group" enum — slots in the same group kill each other on trigger. Extends cut-self. PRESET-SAFE.

**Tier 3 (post-v1.0):** TBD

### §P3 BaySick family (Synth + Bass, share DSP)
**Shipped in v1:** TBD (review pending)

**Tier 1 - to do at review time (pre-booked):**
- **Right-click "Automate" menu gap.** BaySickSynthEditor + BaySickBassEditor use `SliderAttachment` (3 attachment usages each in the .h files) but don't set `componentID` on their sliders -> `GlobalAutoRightClick` can't expose the "Automate: X" menu. All BaySick params are registered in APVTS with prefixes `bss_` (synth) and `bkb_` (bass) - fully automatable from the backend, no UI affordance. **Fix pattern:** `slider.setComponentID(apvtsParamId)` per attached slider. Confirm `VKnobAutomation::sOnRegisterApplicator` + `sOnRegisterReader` are wired for these paramIds. PRESET-SAFE.
- **A9 slider-sync issue: assumed NOT applicable.** Both BaySickSynth and BaySickBass use `SliderAttachment` for knobs. JUCE handles bidirectional sync. **Verify at review time** that every user-facing knob is attached (not just a subset) and that the shared `BaySickSynthDSP` (used by both Synth + Bass wrappers) reads APVTS correctly.

#### Drum Synthesis Expansion Bundle (2026-04-18 pre-booking)
**Context:** Required for the §P2 dual-engine drum slot feature. When drum slots can hold `BaySickSynthProcessor`, the synth needs enough capability to authentically produce TR-series analog drums, Simmons electronic drums, Yamaha FM drums, Korg analog machines, tuned percussion, and modern designer synth percussion. Bell FM mode alone (current capability) covers tonal percussion well but can't do classic analog drum machine sounds without these 6 additions.

**6 DSP additions to BaySickSynthDSP / BaySickSynthVoice (all PRESET-SAFE):**

1. **Pitch envelope (ADSR on pitch)** — new envelope generator that modulates oscillator pitch independently of amp/filter envelopes. Per-voice state. Default amount = 0 (no pitch mod, v1 behaviour preserved). One-shot, not cyclic (distinct from existing LFO-on-pitch). APVTS params: `bss_pEnvAtk`, `bss_pEnvDec`, `bss_pEnvSus`, `bss_pEnvRel`, `bss_pEnvAmt` (semitones of swing, typically -24..+24).
2. **Sine waveform primitive** — new `BssWaveform::Sine` enum value. Pure sine generator, not approximated via filter sweep on pulse. Critical for 808/909 kick which is pure sine + pitch env.
3. **Noise-only mode** — toggle that routes the internal noise generator directly to the filter/amp chain, bypassing the oscillator. Current `mNoiseLevel` is a mix-in amount; this adds a "noise-primary" mode where the oscillator is muted and noise IS the sound source. Default off (v1 behaviour). Essential for snares, hats, claps.
4. **Free Hz tuning on dual-osc** — current DualOsc is musical-interval only (octaves, fifths, SawSaw, SpreadOct, SpreadFifth). Add a mode flag that reinterprets the interval parameter as absolute Hz offset (or absolute Hz for the 2nd oscillator, ignoring the base note). Enables classic 808 cowbell (540 + 800 Hz) and 909 hi-hat (6 inharmonic frequencies) voicings.
5. **Transient injector** — optional short noise/click burst at note-on, before the main ADSR takes over. Parameters: transient amount (dB), transient duration (0-20 ms), transient colour (noise BW / HPF cutoff). Default amount = 0 (off). Critical for 909 kick's signature "click" punch and for snare attack crispness.
6. **Multi-burst envelope mode** — alternate envelope shape for the amp stage: instead of standard ADSR, the attack phase fires N short bursts with settable gap (e.g. 4 bursts ~20 ms apart for 808 handclap). Default = standard ADSR (v1 behaviour). New mode enum + burst count + burst spacing params.

**PRESET-SAFE** for all 6 — each one adds new params defaulting to a value that preserves v1 behaviour, or adds a new enum value / mode that isn't selected by default. Existing BaySickSynth/BaySickBass presets load unchanged.

**What the 6 DSP adds unlock — full capability map:**

**Sine + pitch envelope unlocks:**
- TR-808 kick (pure sine, pitch drop 200 -> 50 Hz)
- TR-909 kick (sine + pitch drop + click transient — needs both)
- TR-606 kick / bass drum
- Simmons SDS-V / SDS-7 / SDS-8 kicks + toms (pitched sine sweep was their signature)
- 808/909/Simmons toms at any pitch
- Modern EDM "designer kicks" (sine + shaped pitch envelope)
- Sub-bass hits / 808-style sub
- Sci-fi "laser zap" synth FX

**Noise-only mode unlocks:**
- TR-808 snare (noise + BPF + short env)
- TR-909 snare (noise + tuned oscillators + BPF)
- TR-606 / TR-707-style noise-driven snares
- All TR-series hi-hats (noise + HPF + short envelope)
- 808/909/606 cymbals (noise + resonant filter)
- Whooshes, wind, transient FX
- Paper / brush textures

**Free Hz oscillator tuning unlocks:**
- TR-808 cowbell (two squares at 540 + 800 Hz — the classic ratio)
- TR-808 handclap (combined with multi-burst envelope)
- 909 hi-hat (6 square oscillators at specific inharmonic ratios)
- Cymbals + gongs (inharmonic partials)
- Bell-like ringing tones at specific frequencies
- Any metallic / clangy synthesis

**Transient injector unlocks:**
- TR-909 kick's signature "click" punch
- Snare attack crispness
- Stick-hit starts
- Any attack transient on any drum

**Multi-burst envelope unlocks:**
- TR-808 handclap (4 short bursts ~20 ms apart)
- Hand percussion textures (cabasa, shaker patterns)
- Flams, rolls
- Complex percussive attack shapes

**Combined with existing Bell FM unlocks:**
- Yamaha RX-11 / RX-5 / RX-21 FM drums
- DX7 percussive patches (wood, glass, metal)
- Bells, marimbas, glockenspiels, chimes
- Woodblocks, tabla, congas, bongos, agogo, triangle, tambourine, rimshots

**What it DOESN'T unlock (for completeness):**
- PCM-sampled drum machines (LinnDrum, TR-707, DMX, SP-1200, Alesis, MPC) — those are sample-based by design. VibePlayer slots cover this already.
- Granular / glitch drums — needs granular engine (separate module, logged as Future Effect Module).
- Physical-modeled acoustic drums — would need PM DSP (not currently in scope).
- Modern layered kicks (808 sub + attack layer + sample tail) — users can layer sample + synth slots, which solves this organically via the dual-engine slot feature.

**The "big door" summary:**

| Era / style | Current VibeDAW | After the 6 adds |
|---|---|---|
| TR-analog drums (78, 808, 909, 606) | ❌ synth-only, can only approximate | ✅ all authentic |
| Simmons electronic drums | ❌ | ✅ |
| Yamaha FM drums (RX / DX percussion) | 🟡 Bell FM tuned only | ✅ full FM drums |
| Korg analog machines (DDM, KPR) | ❌ | ✅ |
| Tuned percussion (bells / marimbas / chimes) | 🟡 Bell FM only | ✅ broader |
| Sampled drum machines (LinnDrum, 707, MPC) | ✅ via VibePlayer | ✅ via VibePlayer (unchanged) |
| Modern designer percussion | ❌ partial | ✅ any synth-percussion technique |

**Cross-benefits (non-drum-specific):** these adds also enrich BaySickSynth as a general synth and BaySickBass as a bass voice. Pitch envelope, for example, is useful for synth leads (bell-like attack sweep), classic analog lead envelopes, and "PEW" zap synthesis. Noise-only mode doubles as a hiss/texture layer for non-drum contexts. All 6 adds are useful outside of drums too.

**Ship order suggestion:**
1. §P3 DSP adds first (6 items, land on BaySickSynthDSP / BaySickSynthVoice).
2. §P3 Editor adds (expose the 6 new params as UI controls on BaySickSynthEditor; PRESET-SAFE).
3. ~~§P2 Dual-engine drum slot architecture (processor variant + editor dispatch).~~ **[moved 2026-04-21 to §P4.1 — §P2 closed before this work started]**
4. ~~§P2 Drum preset bank (factory kits using both engines — 808, 909, Simmons, FM, modern-designer kits).~~ **[moved 2026-04-21 to §P4.4 — §P2 closed before this work started]**

**Tier 2 deferred:** TBD
**Tier 3 (post-v1.0):** TBD

---

### §P3 BaySickSynth SLA Audit (2026-04-21, planning-only)

**Scope:** BaySickSynth engine (prefix `bss_`). BaySickBass shares `BaySickSynthDSP` + `BaySickSynthVoice`, so every DSP-side finding here is 2-for-1 (Synth + Bass). Editor wiring is separate (bkb_ prefix, green LAF).

**Wiring state:**
- 26 APVTS params. 24 fully wired (APVTS ↔ Editor ↔ DSP).
- Range-mapping audit: all 24 clean (no Treble-style mismatch).
- CPU guards: consistent cache-compare pattern across every setter in `updateFromApvts`.
- componentID for right-click Automate: ❌ zero calls in BaySickSynthEditor (same gap §P2 S3 fixed for VibePlayer).
- `cutSelf` bool: not present — retro pre-booked in §P2-closeout S4 (Harmless + Synth + Bass).

**2 broken params found:**
- **`bss_flt_type`** — APVTS param + DSP setter (`setFilterType`) both exist, but `updateFromApvts` never reads the param → setter never called → filter always stuck on LowPass. No UI control visible on Filter tab either.
- **`bss_lfo_sync`** — APVTS param registered + button attached in editor, but never read from APVTS and no DSP setter exists → broken UX (button does nothing).

**§P3-CORE — 11 DSP additions (expanded 2026-04-21 from 6 → 11 for full classic-synth authenticity beyond just drums; all PRESET-SAFE per blueprint guarantee):**

Drum-focused (blueprint lines 742-751):

| # | Addition | PRESET | Lock-before-code |
|---|---|---|---|
| P3.1 | Pitch Envelope (ADSR on pitch, new params `bss_pEnvAtk/Dec/Sus/Rel/Amt`, default Amt=0) | PRESET-SAFE | One-shot confirmed? Bipolar ±24 default range OK? |
| P3.2 | Sine waveform (new `BssWaveform::Sine` enum) | PRESET-SAFE *only if appended at index 10; inserting at 0 shifts every existing preset's waveform choice* | Append at index 10 — confirm? |
| P3.3 | Noise-only mode (new `bss_noiseOnlyMode` bool, default false) | PRESET-SAFE | When mode on: does `noise` param control noise level, or always max? |
| P3.4 | Free-Hz dual-osc tuning (new mode flag reinterpreting interval param as absolute Hz) | PRESET-SAFE | Reuse `modifier` param (mode-switched meaning) or add dedicated `dualOscFreqA/B` params (cleaner, +2 params)? |
| P3.5 | Transient injector (amount dB / duration ms / colour HPF, default off) | PRESET-SAFE | Colour = HPF cutoff (simple) or white/pink toggle + BW? |
| P3.6 | Multi-burst envelope mode (new `envMode` / `burstCount` / `burstSpacing`, default ADSR) | PRESET-SAFE | Impl approach: subclass AdsrEnvelope, internal mode flag, or parallel env? |

Classic-synth authenticity additions (expanded scope 2026-04-21):

| # | Addition | PRESET | Unlocks | Lock-before-code |
|---|---|---|---|---|
| P3.7 | Hard sync — osc2 phase restarts when osc1 completes | PRESET-SAFE (new `bss_oscSync` bool, default false) | 80s sync leads (Jump / Final Countdown), CS-80 brass, Prince leads, Moog/ARP sync sweeps | UI placement on OSC tab? |
| P3.8 | Ring modulation — multiply osc1 × osc2 | PRESET-SAFE (new `bss_ringMod` bool, default false) | CS-80 metallic patches, sci-fi/horror FX, bell/gong tones beyond Bell FM, 909 cowbell extension | UI placement on OSC tab? |
| P3.9 | Pink + brown noise types (new `bss_noiseColor` choice: White/Pink/Brown, default White) | PRESET-SAFE | Authentic 808 snare (pink), pad air layer (pink), sub rumbles (brown), vintage drum warmth | Replace existing LCG noise or add as selector? |
| P3.10 | Per-voice analog drift — small random pitch wander per voice (new `bss_drift` 0-1, default 0) | PRESET-SAFE (default 0 = identical to current behaviour) | Juno/Prophet/CS-80 warmth, chord "aliveness," prevents sterile digital feel | Drift depth range (±cents at max)? |
| P3.11 | Unison mode — per-voice detune stack with spread (promoted from T3.2; new `bss_unisonVoices` 1-7 + `bss_unisonDetune` + `bss_unisonSpread`) | PRESET-SAFE (default 1 voice = current behaviour) | Supersaw trance leads, classic Juno chorus, fat pads, Roland JP-8000 | Max voice count (5 or 7)? |

**Tier 1 — auto-done (no ask):**

| # | Item | Cross-apply | PRESET |
|---|---|---|---|
| T1.1 | Add `flt_type` read in `updateFromApvts` | Bass: `bkb_flt_type` identical bug — 2-for-1 | PRESET-SAFE (default 0=LowPass matches current broken behavior; fixing lets saved presets honor their choice) |
| T1.2 | `setComponentID(apvtsParamId)` on every attached slider (right-click Automate) | Bass editor identical fix | PRESET-SAFE (UI wiring only) |
| T1.3 | Verify every `bss_*` param has an editor attachment (stranded-param sweep) | Bass parallel | PRESET-SAFE (audit-only) |
| T1.4 | Commit this audit to blueprint | — | PRESET-SAFE (docs only) |

**Tier 2 — LOCKED 2026-04-21:**

| # | Item | Decision | PRESET |
|---|---|---|---|
| T2.1 | `bss_lfo_sync` | ✅ **Ship DSP** (host BPM + division combobox, ~2 hr). Tempo-sync is a real musical feature; button exists but does nothing today. | PRESET-SAFE (default off = current free-running behavior) |
| T2.2 | `bss_flt_type` UI | ✅ **Add selector** (LP/HP/BP/Notch). Standard synth control; DSP supports all 4 already. | PRESET-SAFE (param exists, just adds UI) |
| T2.3 | Velocity → Amp (`bss_velAmpTrack`, default 0) | ✅ **Approved** | PRESET-SAFE |
| T2.4 | Legato mode (add 4th voiceMode slot vs. repurpose Lead) | ✅ **Approved** — add 4th LED (preset-safe path) | PRESET-SAFE |
| T2.5 | Ship all 6 §P3-CORE DSP adds in v1.0 | ✅ **Approved for v1.0** | PRESET-SAFE whenever they land |
| T2.6 | UI layout for 6 new params | ✅ **Approved** — layout TBD at UI session (merge into existing tabs vs. new tab decided then) | PRESET-SAFE |

**Tier 3 — future (post-v1.0, documented only):**

| # | Item | PRESET when shipped |
|---|---|---|
| T3.1 | Oversampling on DeafSaw / FM / Bell nonlinear stages (part of Phase 5F-9 DSP quality pass) | PRESET-SAFE |
| ~~T3.2~~ | ~~Unison mode (detune + spread, per-voice count)~~ **PROMOTED 2026-04-21 to P3.11 under the expanded §P3-CORE scope** | — |
| T3.3 | Anti-click fades on param jumps | PRESET-SAFE |
| T3.4 | Per-note portamento via CC / pattern metadata (requires pattern-infra plumbing) | PRESET-SAFE |
| T3.5 | Aftertouch → cutoff / volume | PRESET-SAFE |
| T3.6 | Waveform morph/blend knob (crossfade between two waveforms) | PRESET-SAFE |
| ~~T3.7~~ | ~~Filter KB-track as cents/oct slider~~ **DROPPED 2026-04-21** — synth-nerd feature, 0-1 amount is beginner-friendlier, keep as-is | — |
| T3.8 | LFO sync-to-host *if T2.1 picks "remove"* — re-promoted if reinstated later | PRESET-SAFE |

**Cross-apply confirmed:** all 11 §P3-CORE adds land in shared DSP → BaySickBass gets them simultaneously. Editor UI must be added to both BaySickSynthEditor + BaySickBassEditor separately.

**UI layout plan — Option C (locked 2026-04-21):** Single new tab called MOD (or PERFORMANCE / CHARACTER — final name TBD at UI session). Existing OSC / OSC ENV / FILTER / FLT ENV / LFO tabs absorb 1-3 new controls each. Final layout:
- **OSC** — gains hard-sync LED (P3.7), ring-mod LED (P3.8), noise-color selector (P3.9), free-Hz mode switch (P3.4). Sine (P3.2) adds one entry to the waveform dropdown (no new control).
- **OSC ENV** — gains pitch-env ADSR row (P3.1: 5 vertical sliders A/D/S/R + Amt) alongside existing amp ADSR + VEL knob.
- **FILTER / FLT ENV / LFO** — unchanged.
- **New MOD tab** — transient injector (P3.5: 3 knobs), multi-burst envelope (P3.6: 3 controls), drift (P3.10: 1 knob), unison (P3.11: 3 controls), noise-only toggle (P3.3: 1 switch).

Layout polish deferred: Jeff flagged 2026-04-21 that "a number of pieces that are knobs are WAY oversized" — full editor layout review scheduled once all 11 adds ship, before §P3 close.

**Session estimate (expanded):** ~11-16 implementation sessions to close §P3 at Tier 1 + Tier 2 + all 11 DSP adds + editor UI. Was 6-12 for 6 adds.

**Open questions locked-pending-Jeff:** ~~T2.1, T2.2, T2.5, T2.6~~ (all locked 2026-04-21); **P3.1-P3.6 design locks still open** — must be answered before each respective DSP add's implementation session. T3.7 dropped 2026-04-21.

### §P3 Preset Recipe Catalogue (accumulating — every §P3-CORE session must append)

All recipes surfaced during D1–D11 sessions are captured here for later preset-bank construction (§P4.4 and the v1 factory kits). Per-recipe: waveform, params knob-by-knob, voiceMode, effect settings. Cross-apply: any synth recipe also becomes a Bass preset candidate if the frequency range makes sense for low register.

**Basses / Kicks / Subs:**
- **Sub bass** — Sine waveform, Transpose -12 or -24, amp env: slow release.
- **808 kick** — Sine + Pitch Env Amt +12 + Pitch Env Decay ~60 ms + Pitch Env Sustain 0 + Amp Decay ~300-400 ms + Amp Sustain 0. Transient AMT 0.2 / DUR 3 ms / COLOUR 200 Hz for low-frequency click body. (Pitch-env amt was originally +24 but produced a too-pitched "woofy" tail at full-duration playback — uniform kick re-tune 2026-04-25 dropped pEnv_amt by 12 and moved trans_colour from 5 kHz to 200 Hz across all 6 factory kicks.)
- **909 kick** — Same shape as 808 kick post-tune. Differs only in amp_decay being identical (0.35 s) but the uniform 2026-04-25 pass collapsed the formerly distinctive 0.6 transient amount to 0.2 and the 6 kHz transient colour to 200 Hz. Re-tune if you want a brighter 909 character — pre-pass values were AMT 0.6 / COLOUR 6 kHz.
- **303 acid bass** — SAW, Mono voiceMode, glide up, filter resonance high, filter env amount high.
- **Picked bass** — SAW, low transpose, Transient AMT 0.4 / DUR 2 ms / COLOUR 8 kHz.

**Drums (synth-based):**
- **TR-808 snare** — Noise Only ON, Noise Color **Pink**, Filter BP ~1.5 kHz resonance moderate, Amp fast attack / ~150 ms decay / sustain 0.
- **TR-808 hi-hat / cymbal** — Noise Only ON, Noise Color **White**, Filter HP ~8 kHz, Amp very fast decay (~50 ms).
- **TR-808 cowbell** — SQUARE+SQUARE waveform, Dual Tuning Absolute Hz, Modifier ~0.60 (~800 Hz), short Amp decay, Noise Only OFF.
- **808 handclap** — Noise Only ON, Noise Color **Pink**, Filter BP ~1.5 kHz res mod, Amp fast attack / ~300 ms decay / sustain 0, **Burst Mode ON**, COUNT 4, SPACING 20 ms.
- **Stick-hit drum** — Noise Only ON, Noise Color White, Filter BP, Transient AMT 0.8 / DUR 2 ms / COLOUR 10 kHz.

**Classic synth leads / pads:**
- **Moog-style lead "woop"** — SAW, Mono, Pitch Env small amount + fast decay (brief blip at onset), glide moderate.
- **Detuned supersaw / trance lead** — SAW+SAW Musical, Modifier ~0.5 detune, Amp env: fast attack slight decay full sustain. Will layer with unison when P3.11 ships.
- **Classic 80s sync lead (Jump / Final Countdown)** — SAW+SAW or SAW+SQUARE, Dual Tuning Absolute Hz, **SYNC ON**, modulate Modifier (LFO on modifier destination or pitch envelope).
- **Brass scoop** — SAW+SAW (or any saw-ish), Pitch Env Amount -7 semitones, fast attack, ~200 ms decay, sustain 0.
- **Moog-style Hz-interval (no key tracking)** — SAW+SQUARE, Dual Tuning Hz Offset, Modifier to taste.

**Bells / metallic / sci-fi:**
- **CS-80 metallic / bell** — SQUARE+SQUARE, Dual Tuning Absolute Hz, **RING MOD ON**, Modifier ~0.5, low transpose for bell character.
- **Sci-fi zap / R2-D2 bleep** — SAW+SAW, Dual Tuning Absolute Hz, **RING MOD ON**, Modifier high (2-10 kHz range), short Amp env.
- **Horror / sync'd ring** — SAW+SAW, both **SYNC + RING ON**, Dual Tuning Hz Offset, sweep Modifier with LFO.
- **Doctor Who Theremin-adjacent** — SAW+SAW, RING ON, Dual Tuning Absolute Hz, LFO on modifier destination.

**Keys / organ / electric piano:**
- **Hammond-ish organ** — Sine waveform, no glide, fast amp attack/release, optionally layer multiple oscs via unison (P3.11).
- **Rhodes / Wurli hammer-strike** — BELL or SAW+SQUARE for E.Piano base, Transient AMT 0.3 / DUR 5 ms / COLOUR ~3 kHz.
- **Theremin** — Sine, Legato voiceMode, glide high, LFO on pitch (subtle amount).
- **Vibraphone / marimba roll** — BELL waveform, gentle Pitch Env, **Burst Mode ON**, COUNT 8, SPACING 40 ms, Amp slow attack full sustain.

**Percussion / rhythmic:**
- **Flam (double-hit percussion)** — Any waveform, short Amp decay, **Burst Mode ON**, COUNT 2, SPACING 30 ms.
- **Tabla-like roll** — BELL waveform high transpose, **Burst ON**, COUNT 6, SPACING 15 ms, short Amp decay.

**Ambient / textural:**
- **Ocean ambient pad** — Noise Only ON, Noise Color **Brown**, Filter LP ~500 Hz with slow LFO, long attack/release.
- **Wind pad** — Noise Only ON, Noise Color **Pink**, Filter LP ~2 kHz with LFO.
- **Dark sub-rumble texture** — Noise Only ON, Noise Color **Brown**, Filter LP ~200 Hz, long release.

**Additional recipes spanning the full §P3-CORE capability map** (reconstructed from the earlier conversation + blueprint lines 753-817). All recipes below are PRESET-SAFE by construction (they use params that default to neutral). Any recipe can also be built as a Bass preset if the note range is dropped an octave.

**TR-series analog drums (beyond the ones above):**
- **TR-808 tom (high)** — Sine waveform, Transpose +2, Pitch Env Amt +12 / Decay 80 ms / Sustain 0, Amp Decay ~250 ms Sustain 0.
- **TR-808 tom (mid)** — Sine, Transpose 0, Pitch Env Amt +10 / Decay 100 ms, Amp Decay ~300 ms.
- **TR-808 tom (low)** — Sine, Transpose -5, Pitch Env Amt +8 / Decay 120 ms, Amp Decay ~400 ms.
- **TR-808 rimshot** — SQUARE+SQUARE, Dual Tuning Absolute Hz (Modifier ~0.55 = ~600 Hz), Amp very short decay ~40 ms, slight pitch env drop.
- **TR-808 open hi-hat** — Noise Only ON, Noise Color White, Filter HP ~10 kHz, Amp Decay ~400 ms.
- **TR-808 closed hi-hat** — Noise Only ON, Noise Color White, Filter HP ~9 kHz, Amp Decay ~80 ms.
- **TR-808 maraca / shaker** — Noise Only ON, Noise Color Pink, Filter BP ~5 kHz moderate res, Amp Decay ~50 ms.
- **TR-808 conga (high)** — Sine, Transpose +5, Pitch Env Amt +6 / Decay 50 ms, Amp Decay ~200 ms.
- **TR-808 conga (mid)** — Sine, Transpose 0, Pitch Env Amt +5 / Decay 60 ms, Amp Decay ~250 ms.
- **TR-808 conga (low)** — Sine, Transpose -4, Pitch Env Amt +4 / Decay 80 ms, Amp Decay ~300 ms.
- **TR-808 claves** — SQUARE+SQUARE, Absolute Hz Modifier ~0.72 (~2.5 kHz), Amp very short ~15 ms, Transient AMT 0.5 / DUR 2 ms / COLOUR 8 kHz.
- **TR-909 snare** — Noise Only ON, Pink noise + add in osc at low level (SAW+SAW Modifier 0 for tuned element), Filter BP ~2 kHz res mid, Amp Decay ~180 ms Sustain 0.
- **TR-909 open hi-hat** — Noise Only ON, Noise Color White, Filter HP ~6 kHz, Amp Decay ~350 ms.
- **TR-909 closed hi-hat** — Noise Only ON, Noise Color White, Filter HP ~7 kHz, Amp Decay ~60 ms.
- **TR-909 ride / crash** — Noise Only ON, Noise Color White, Filter BP ~8 kHz, Amp Decay ~800-1200 ms.
- **TR-909 tom (high / mid / low)** — Sine with Pitch Env Amt +10 / Decay 70 ms + Transient AMT 0.3 / DUR 2 ms (gives 909's signature click-on-tom character).
- **TR-606 kick** — Sine + Pitch Env Amt +18 / Decay 40 ms (faster/tighter than 808), Amp Decay ~200 ms.
- **TR-606 snare** — Noise Only ON, Pink, Filter BP ~1.8 kHz, Amp Decay ~100 ms (drier than 808/909).

**Simmons electronic drums (pitched sine sweep — their signature):**
- **Simmons SDS-V kick** — Sine, Pitch Env Amt +24 / Decay 100 ms / Sustain 0, Amp Decay ~300 ms, Transient AMT 0.2 / DUR 5 ms (the stick hit).
- **Simmons SDS-V snare** — Sine + Noise Only OFF with Noise mix ~0.4 (Pink), Filter slight LP, Pitch Env Amt +12 / Decay 80 ms, Amp Decay ~200 ms.
- **Simmons SDS-V tom (any pitch)** — Sine, Transpose per-note, Pitch Env Amt +12 / Decay 120 ms, Amp Decay ~350 ms. The iconic 80s tom-fill sound.
- **Simmons SDS-7 / SDS-8 kick** — same pattern as SDS-V but longer decay and deeper pitch env (+18 amount).

**Yamaha FM drums (RX-series / DX7 percussive):**
- **RX-11 kick** — BELL waveform, Modifier ~0.4, Pitch Env Amt +18 / Decay 60 ms, Amp Decay ~250 ms, Transient AMT 0.3.
- **RX-11 snare** — BELL Modifier ~0.7 (high FM index), noise mix ~0.3 Pink, Filter BP ~2 kHz, Amp Decay ~180 ms.
- **DX7 wood block** — BELL Modifier ~0.8 (metallic FM), Transpose +12, Pitch Env Amt +8 / Decay 30 ms, Amp very short Decay ~80 ms.
- **DX7 glass** — BELL Modifier ~0.6, Transpose +12 or higher, long Amp Decay ~1-2 s, very low sustain.
- **DX7 metal (anvil / pipe hit)** — BELL Modifier ~0.9, Transient AMT 0.4 / DUR 2 ms / COLOUR 10 kHz, Amp Decay ~500 ms.

**Tuned percussion:**
- **Glockenspiel** — BELL Modifier ~0.4, Transpose +12, Amp fast attack, Decay ~1 s, Sustain low.
- **Marimba** — BELL Modifier ~0.3, natural transpose, Amp Decay ~600 ms, Pitch Env small downward amt -3.
- **Xylophone** — BELL Modifier ~0.5, Transpose +7, Amp Decay ~400 ms shorter than marimba.
- **Tubular bells** — BELL Modifier ~0.55, long Amp Release ~2 s full sustain, noticeable chorus/delay.
- **Celesta** — BELL Modifier ~0.2 (more sine-like), Transpose +12, Amp moderate decay.
- **Triangle** — SQUARE+SQUARE Abs Hz Modifier ~0.8 (~3 kHz), RING MOD ON for shimmer, Amp Decay ~2 s.
- **Tambourine** — Noise Only ON, Noise Color White, Filter HP ~6 kHz, **BURST ON** COUNT 2 SPACING 15 ms, Amp Decay ~200 ms.
- **Cowbell (generic)** — see TR-808 cowbell.
- **Agogo** — SQUARE+SQUARE Abs Hz Modifier ~0.65 (~1.5 kHz), Amp short Decay ~80 ms.
- **Rimshot (acoustic-ish)** — Noise Only OFF with small noise mix, BELL Modifier ~0.5, Transient AMT 0.7 / DUR 2 ms.
- **Woodblock** — BELL Modifier ~0.85, Transpose +5, Amp very short Decay ~40 ms.

**Hand percussion:**
- **Tabla (high / "tin")** — BELL Modifier ~0.7, Transpose +7, Amp Decay ~200 ms, slight Pitch Env Amt -4.
- **Tabla (low / "dun dun")** — Sine + small noise mix, Transpose -5, Pitch Env Amt +5 / Decay 100 ms, Amp Decay ~400 ms.
- **Bongo (high)** — Sine, Transpose +10, Pitch Env Amt +5 / Decay 60 ms, Amp Decay ~150 ms.
- **Bongo (low)** — Sine, Transpose +5, Pitch Env Amt +4 / Decay 80 ms, Amp Decay ~200 ms.
- **Cabasa / shaker** — Noise Only ON, Pink, Filter BP ~4 kHz, **BURST ON** COUNT 3 SPACING 25 ms.

**Basses:**
- **Moog Minimoog bass** — SAW+SAW Musical slight detune, Mono, Filter LP with moderate res, Filter Env Amt +0.5 / Decay 200 ms, Amp fast attack full sustain.
- **Moog sub (fundamental-only)** — Sine, Transpose -12, Amp slow release, no filter modulation.
- **303 acid bass** — SAW, Mono, glide 0.05-0.1 s, Filter LP cutoff low + resonance high, Filter Env Amt high, Filter Decay ~200 ms.
- **DX bass (FM)** — BELL Modifier ~0.4, Transpose -12, Pitch Env Amt -2 / Decay 30 ms (subtle FM-attack shift), Amp full sustain.
- **Picked bass (P-bass-ish)** — SAW, Transpose -12, Mono, Transient AMT 0.4 / DUR 2 ms / COLOUR 8 kHz, Amp moderate decay short sustain.
- **Reggae / dub bass** — Sine + SAW low blend (layer via two engines) OR just Sine, Transpose -12, Amp long release with short decay.
- **Synthwave bass (wide detune)** — SAW+SAW Musical max detune, Poly, long slight Pitch Env upward.
- **FM growl bass** — BELL Modifier ~0.75 (aggressive FM), Filter LP slight, Mono, glide.

**Leads:**
- **Moog lead** — SAW, Mono/Legato, glide, Filter LP with res, Filter Env Amt moderate.
- **CS-80 brass lead** — SAW+SAW small detune, Polymode, Filter BP or LP moderate cutoff, slow Amp attack ~50 ms, Pitch Env tiny +2.
- **OB-8 brass** — similar to CS-80 but with slower filter attack + Unison (will use P3.11).
- **Sync lead** — see "Classic 80s sync lead" above.
- **Supersaw trance lead** — SAW+SAW + Unison (P3.11 when shipped), Filter env sweeping in, slow attack.
- **DX lead (FM)** — BELL Modifier ~0.6, bright envelope, slight vibrato via LFO on pitch.
- **Theremin** — see above (Sine + Legato + glide + LFO pitch).
- **Whistle** — Sine, Transpose +12 or +24, LFO pitch vibrato subtle, slow Amp attack.
- **Square lead (8-bit game)** — PULSE Modifier ~0.5, Mono, fast amp env.
- **Pulse-width-modulated lead** — PULSE + LFO modifying modifier destination (OscModifier LFO dest).

**Pads:**
- **Juno warm pad** — SAW+SAW Musical slight detune, slow attack ~300 ms, long release ~1.5 s, Filter LP moderate cutoff + slow LFO.
- **Jupiter brass pad** — SAW+SAW + BELL layer (two voices), medium attack, high sustain.
- **OB-8 string pad** — SAW+SQUARE, slow attack, long release, subtle Pitch Env (Unison once P3.11 ships).
- **JP-8000 supersaw pad** — SUPERSAW waveform, slow attack, long release.
- **Solina string ensemble** — SAW+SAW slight detune, full polyphony, slow attack, steady sustain.
- **Glass pad** — BELL Modifier ~0.3, slow attack + release, very clean, no filter mod.
- **Ambient drone** — Sine or Noise-Only Brown, very slow attack and release, LFO on filter.

**Keys / Organs:**
- **Rhodes EP** — SAW+SQUARE base or BELL Modifier ~0.3, Transient AMT 0.3 / DUR 4 ms / COLOUR 3 kHz (hammer strike), Amp Decay ~400 ms Sustain ~0.3.
- **Wurlitzer EP** — BELL Modifier ~0.5 (brighter than Rhodes), Transient AMT 0.35, Amp Decay ~350 ms Sustain ~0.2.
- **DX E.Piano (DX-EP1)** — BELL Modifier ~0.55, Transient AMT 0.4 / DUR 3 ms, Amp Decay ~500 ms Sustain low, fast attack.
- **Clavinet** — SAW+SQUARE or PULSE Modifier ~0.3, Transient AMT 0.6 / DUR 2 ms / COLOUR 5 kHz, Filter LP moderate, Amp very short Decay ~150 ms.
- **Harpsichord** — SAW+SQUARE, Transient AMT 0.7 / DUR 1 ms / COLOUR 6 kHz (pluck), Amp Decay ~600 ms Sustain 0.
- **Hammond drawbar organ** — Sine (fundamental) + BELL at octave (layer via two voices, or simulate with Sine + natural harmonics via filter), no glide, fast attack/release.
- **Farfisa organ** — PULSE Modifier ~0.5 + SAW layer, fast attack/release, no filter mod.
- **Vox Continental organ** — SQUARE+SQUARE, fast attack/release.

**Sound FX:**
- **Sci-fi zap / laser** — Sine + Pitch Env Amt +24 / Decay 200 ms (pitch falls), Filter LP sweep via Filter Env.
- **Sub drop / downlifter** — Sine, Pitch Env Amt -24 / long Decay 1-2 s, long Amp Release.
- **Riser** — Noise Only ON, Filter LP automation rising, long slow Amp attack, accel with LFO rate rising.
- **Impact / hit** — Sine very low + Noise-Only Brown layer, Transient AMT 1.0 / DUR 10 ms, short Amp Decay.
- **Robot voice / R2-D2** — SAW+SAW Abs Hz, RING MOD ON, Modifier modulated by random/LFO.
- **Horror pad / tension** — SAW+SAW + SYNC + RING both ON, Hz Offset, slow LFO on Modifier destination.
- **Wind howl** — Noise Only ON, Brown or Pink, Filter LP with slow LFO, long Amp env.

**Convention**: every new §P3-CORE D-session appends new recipes here before closing. Recipes are the input set for §P4.4 factory preset bank + demo content.

---

### §P4 DrumsPage Dual-Engine Restructure (created 2026-04-21)

**Context:** Absorbs drum-page work originally pencilled into §P2 and §P3 ship orders. §P4 depends on §P3 closing (synth must be drum-capable first) and supplies the user-facing delivery of the whole drum-synthesis story. Ship order logically: §P3 → §P4.

**§P4.1 Dual-engine drum slot dropdown** — each of the 16 drum slots (BaySickDrumsProcessor::kNumSlots) gains an engine selector: "VibePlayer sample" (current behavior) OR "BaySickSynth patch." Processor variant per-slot + editor dispatch. PRESET-BREAK ⚠️ — ship in v1 (existing single-engine slot state needs migration path: all existing slots map to VibePlayer engine at load).

**§P4.2 Dual piano roll mode for DrumsPage** — originally logged at line 838 under Piano Roll. Full spec lives there. PRESET-BREAK ⚠️ — ship in v1. Switches DrumsPage between drum-grid mode (current 16-row step grid) and full piano-roll mode (all 127 notes, any synth-slot becomes playable across the keyboard).

**§P4.3 Per-slot EQ rebind** — originally at line 967 (pre-EQ hook). The pre-EQ addition provides the binding point the slot-dropdown uses. PRESET-SAFE (pre-EQ defaults to flat/bypass).

**§P4.4 Drum preset bank** — factory kits using both engines: 808 kit (BaySickSynth kick + snare + clap + cowbell / VibePlayer hats + perc), 909 kit, Simmons kit, FM kit (Yamaha RX-style), modern-designer kit. PRESET-SAFE (additive factory content).

**§P4 ship order within phase:**
1. §P4.1 dropdown infrastructure (foundation)
2. §P4.3 per-slot EQ rebind (straightforward)
3. §P4.2 dual piano roll mode (UI work)
4. §P4.4 factory preset bank (ships last — needs 1+2+3 complete)

**Cross-dependencies:**
- §P3-CORE 6 DSP adds must ship before §P4.4 (factory kits rely on them).
- §P4.1 must precede §P4.2 (dual piano roll needs slot-engine knowledge to know what to display).
- §P4.3 is independent and can land in parallel with §P4.1.

**Tier 2 / Tier 3 scope:** TBD — audit session to run after §P3 closes, same SLA pattern.

---

#### §P4 DrumsPage SLA Audit (committed 2026-04-22)

**Terminology correction adopted this audit:** PRESET-BREAK was being used loosely. Precise taxonomy going forward:
- **PRESET** = individual patch (one engine's state — a VibePlayer patch, a BaySickSynth patch)
- **KIT** = 16-slot bundle (engine selection + preset per slot, slot names, per-slot mixer state)
- **PATTERN** = musical content (notes, automation lanes)
- **PROJECT** = everything together

Re-classification of pre-booked items under correct labels:
- §P4.1 was "PRESET-BREAK" → actually **KIT-BREAK** (kit format `16 × VibePlayerState` → `16 × {engineType, engineState}`)
- §P4.2 was "PRESET-BREAK" → actually **PATTERN-BREAK** (drumRoll encoding `pitch = 51-slot` → `{slot, pitch}` pairs)
- §P4.3 stays PRESET-SAFE
- §P4.4 stays PRESET-SAFE

**Discovery items locked for v1 (in addition to §P4.1-§P4.4):**
- D1 Slot copy/paste/duplicate (right-click slot bar)
- D2 Drag-and-drop sample onto slot bar
- D3 Per-slot polyphony cap (Mono/Poly toggle, Bool APVTS per slot per engine)
- D4 Per-step velocity + probability in slot-grid mode (wire ControlLane to drum mode)
- D5 Kit randomizer + A/B kit compare (in Kit menu)
- D6 Slot lock toggle (prevents accidental overwrite on kit-load / drag-drop)
- T2 Multi-sample slot UI (PRESET-SAFE: VibePlayer DSP already handles round-robin / velocity layers / articulation groups via SFZ opcodes per VibePlayerDSP.cpp:196-232; T2 is purely a GUI affordance for users who don't hand-write SFZ)
- T3 Choke groups (per-slot Int APVTS, slots in same non-zero group kill each other on trigger)
- T6 Fill generator (toolbar button — procedurally generates a drum fill for current/selected bar)
- T12 Per-slot MIDI learn (per-slot Int APVTS for MIDI note remap, default = current 36-51 mapping)

**Discovery items deferred to dedicated later phases:**
- T1 Harmless as 3rd engine option — DROPPED (Harmless not drum-tuned; BaySickSynth + VibePlayer cover the drum sound-design space)
- T4 Per-slot automation lanes — PATTERN-BREAK, requires PatternManager refactor
- T5 Per-slot sidechain routing — extends §P3 routing; needs new dropdown in EQ dynamic params popout
- T7 Kit morph (A/B + morph%)
- T8 Per-slot freeze-to-audio — needs offline-render scaffolding
- T9 External MIDI controller pad mapping — settings.json scope, drums-adjacent
- T10 TR-808 step sequencer view — alt editor for same drumRoll data
- T11 Custom slot count — KIT-BREAK MAJOR, every per-slot prefix scheme + mixer strip provisioning + drum grid range anchored to 16

**UI conventions locked across §P4:**

*Player page tab strips (Layers / Bass / Drums):*
- Layers/Bass: `[Player | Piano Roll | Pre EQ8 M/S]` ("Sound" → "Player", "EQ" → "Pre EQ8 M/S")
- Drums: `[Player | <Drum Grid ▾ or Full Piano Roll ▾> | Pre EQ8 M/S]`
  - Middle button is split-button: plain click activates Piano Roll tab (normal); ▾ arrow opens mode picker menu
  - Button label reflects current mode
  - Default mode = Drum Grid (beginner-first)

*Mixer strip Effects pages:*
- Layer / Bass / Drum-slot inserts: `[Rack | Post EQ8 M/S]` (single EQ on the strip = post-rack)
- Aux / Audio / Bus inserts: `[Pre EQ8 M/S | Rack | Post EQ8 M/S]` (full pre-rack EQ added universally)

*Slot dropdown placement (Drums page):*
- The piano-roll-page slot-selector dropdown AND the EQ-page slot-selector dropdown both live in the PageMenuBar header row — same row as the existing Kit button + slot Nav combo
- Single shared dropdown source (the existing `mDrumNavCbo`) used by all three Drums tabs

*Slot-source picker (P4.1):*
- Folder-style two-section combo per slot:
  - Section "Sample" (existing VibePlayer source picker — folder browser of samples)
  - Section "Synth Patch" (drum-oriented BaySickSynth presets only — filtered subset of §P3 recipe catalogue)
- Drum patch bank ≠ synth patch bank: when BaySickSynth is the primary instrument (Layer page), it sees its full melodic patch bank; in a drum slot, it sees only the drum-oriented filter

*Cross-page ghost notes (C7 in step list):*
- Layer roll: sees Bass 0..3 + Drums (current pattern) + other Layers (0..7 except self)
- Bass roll: sees Layers 0..7 + Drums (current pattern) + other Bass (0..3 except self)
- Drums DRUM-GRID mode: NO ghost notes (the grid already displays all 16 slots)
- Drums FULL-ROLL mode: sees Layers 0..7 + Bass 0..3 + all other drum slots (15 except active)
- Per-source color: VC::LayerCol[N], VC::BassCol[N], kDrumRollColor (or per-slot color in drums full-roll)
- Builder ArrangementGrid + EventEditor NOT in scope (no piano rolls there — blueprint's earlier mention of "Builder arrangement piano rolls" was imprecise; Builder has block-based ArrangementGrid with its own drag-preview ghost concept, separate machinery)

*APVTS naming for new pre-rack EQ:*
- Convention: `mixer_<kind>_<N>_preeq_mid_eq<b><suffix>` and `..._preeq_side_eq<b><suffix>`
- Mirrors existing post-EQ `mixer_<kind>_<N>_mid_eq<b><suffix>` with `_preeq_` token inserted
- Mx/Pg display tags are added by the resolver (StandaloneEditor::resolveAutomationDisplayName) at lookup time; never appear in stored param IDs
- `formatMixerSuffix` gains a `_preeq_` branch → renders as "Pre EQ Mid B4 Freq" (vs existing "EQ Mid B4 Freq" for post-EQ)
- Display example: `mixer_drum_5_preeq_mid_eq3Freq` → `Mx Drum 5 - Pre EQ Mid B4 Freq`

**Drums bus EQ resolution:**
- Investigation flagged: there appear to be TWO drums-bus EQs running today — `mDrumsEQDSP` + APVTS `drums_mid_eq*`/`drums_side_eq*` (page-tab bound) AND the 5F-4a `mixer_drums_*_eq*` post-rack EQ on the Drums Bus mixer strip
- `mDrumsEQDSP` + its params retire (deleted, not migrated). The 5F-4a Drums Bus post-rack EQ is THE drums bus EQ going forward, accessed via Mixer → Drums Bus → Effects page
- Verification step happens at Phase B kickoff before deletion (per "always check" rule)

**Cross-cutting items pre-booked under §P4 (must land in this phase):**
- Cut-self per slot (default-ON for drum slots, default-OFF for Layer/Bass; right-click option on arrangement audio clips)
- Right-click "Automate" componentID gap on VibePlayerEditor (BaySickDrums embeds VibePlayerEditor → inherits gap)
- Ghost notes bug fix + cross-page wiring (C7 above)
- Universal pre-rack EQ on every InsertNode (Layer/Bass/Drum/Audio/Aux) AND on every bus (Master + 5 buses)

**Step breakdown locked (full version in session log; ship order matches blueprint Cross-dependencies):**

Phase A — §P4.1 Dual-engine drum slot dropdown (FOUNDATION)
- A1 Slot ownership refactor (BaySickDrumsProcessor: 16× hard-coded VibePlayer → array<DrumSlot> with engineKind + lazy proc + swapLock)
- A2 Engine swap mechanism (UI-thread setSlotEngine, audio-thread try-lock with silent-block fallback)
- A3 processSlotsSeparately polymorphism
- A4 Slot-source dropdown UI (folder-style two-section combo)
- A5 Polymorphic editor dispatch (VibePlayerEditor for sample slots, BaySickSynthEditor for synth slots)
- D-CC1 Cut-self per slot (default-ON for drums)
- D-CC2 Right-click Automate componentID gap
- D-CC3 T3 Choke groups
- D1, D2, D3, D6, T12 (slot-domain discovery items)

Phase B — §P4.3 Per-slot EQ rebind + universal pre-rack EQ (PARALLEL with A)
- B1 Drums bus EQ retirement (verify `mDrumsEQDSP` is the duplicate, retire it)
- B2 Add `EQ8MsDSP mPreEQ` to every InsertNode + every bus node in VibeGraph
- B3 Pre-rack EQ APVTS registration (`_preeq_` convention)
- B4 Pre-rack EQ APVTS sync in processBlock
- B5 Player-page EQ tabs rebind to InsertNode pre-EQ + DrumsPage gets slot-selector dropdown
- B6 Mixer effects-page tab renames per UI conventions above
- B7 Migration cleanup of legacy `mLayerPageEQs[]` / `mBassPageEQs[]`
- B8 Resolver + automation label `_preeq_` branch

Phase C — §P4.2 Dual piano roll mode (AFTER A — needs slot-engine knowledge)
- C1 Pattern data refactor (drumRoll: `pitch = 51-slot` → `{slot, pitch}` pairs) — PATTERN-BREAK
- C2 PianoRollContainer dual-mode plumbing (setRollMode 0=DrumGrid 1=FullRoll)
- C3 Drum-grid click semantics (option D — C5-only edit, lit-but-no-C5 cells are no-ops)
- C4 Playback routing (notes play at stored pitch through slot's active engine)
- C5 Mode toggle UI (split-button per E.2 Interpretation B + label reflects mode + default Drum Grid)
- C6 Slot selector dropdown for full-roll mode (PageMenuBar row with Kit + Nav)
- C7 Ghost notes bug fix + cross-page wiring (per policy above)
- D4 Per-step velocity + probability in slot-grid mode
- T6 Fill generator

Phase E — Multi-sample slot UI (T2; promoted from Tier-3 to v1 after DSP was found pre-built)
- E1 Slot-bar multi-sample drop (extends D2)
- E2 Multi-sample assembly UI (RR / velocity-split / key-zoned modes)
- E3 Internal SFZ region generation (DSP unchanged — uses existing parser at VibePlayerDSP.cpp:196-232)

Phase F — §P4.4 Factory drum preset bank (LAST — needs A+B+C+E)
- F1 `.bsd` serialization audit/fix (verify per-slot active engine APVTS subtree round-trips, plus per-slot pre-EQ from B3, plus T12/T3/D6/D3 state)
- F2 Build factory kit content (808 / 909 / Simmons / FM / modern-designer)
- F3 Factory bank installation path (read-only factory dir scanned alongside user dir)
- F4 D5 Kit randomizer + A/B kit compare additions to Kit menu

Cross-cutting throughout all phases:
- All new APVTS params follow Mx/Pg display-naming conventions
- Every new control in player editors gets `setComponentID(apvtsParamId)` for right-click Automate
- Every new APVTS param gets range-mapping verified at register time vs DSP setter expectation
- Continuous-blueprint rule: blueprint gets updated when each step ships, not at end of session

**Scope-locked open questions resolved during this audit:**
- Q-A APVTS pre-EQ naming → `mixer_<kind>_<N>_preeq_mid_eq<b><suffix>`
- Q-B Drums bus EQ → retire `mDrumsEQDSP` + `drums_mid_eq*`/`drums_side_eq*`; keep 5F-4a `mixer_drums_*_eq*` (verify at B1 kickoff)
- Q-C Player-page EQ tab labelling → "Pre EQ8 M/S"; mixer-strip insert-effects EQ tab → "Post EQ8 M/S"
- Q-D Discovery list inclusion → D1-D6 + T2 + T3 + T6 + T12; T1 dropped; rest deferred
- Q-E.1 Slot dropdown placement → PageMenuBar header row, same row as Kit button
- Q-E.2 Piano Roll tab button → Interpretation B (split button: click activates tab, ▾ opens mode menu) + label reflects current mode
- Drum-grid ghost notes → none (grid already shows all slots); full-roll mode shows Layer + Bass + other drum slots

**Deferred follow-ups discovered during Phase A implementation:**
- **Lazy per-slot processor construction (BaySickDrums)** — currently 16 VibePlayerProcessor instances are constructed in `BaySickDrumsProcessor::ctor` regardless of whether the user has assigned sounds.  Each carries a full APVTS tree (~30 params) so startup pays for ~480 unused params + 16x VibeSynth voice infrastructure.  Audio-thread cost is already near-zero (empty slots have an early-out skipping `processBlock`), but memory + startup time would benefit from lazy allocation: `DrumSlot::proc` starts null, only gets constructed on first `setSlotEngine` call (or first sample load).  All audio paths (`processBlock`, `processSlotsSeparately`) already null-guard via try-lock + null check (added in A1).  Empty-slot UI placeholder already implemented (Phase A); this is the audio-side complement.  PRESET-SAFE.  Defer to a dedicated session post-Phase-A.
- **Multi-file drop spreading** — D2 drag-and-drop currently uses only the first dropped file.  Future polish: drop N files on slot K → load files[0..N-1] across slots K..K+N-1, skipping locked targets, asking confirmation before overwriting non-empty slots.  Maschine-style workflow.  PRESET-SAFE.

**Pre-existing bugs found + fixed during Phase A (worth noting because the patterns may recur):**
- **Layers/Bass per-page EQ prefix mismatch** (fixed 2026-04-22 early in session) — `updateLayerPageEQsFromApvts` / `updateBassPageEQsFromApvts` in PluginProcessor.cpp used hardcoded `tk_{i}_*` / `tk_bass_{i}_*` while registration used `tk_lay_{i}_*` / `tk_bas_{i}_*` (the trackId convention had changed but the audio-side reader was missed).  Result: all per-page EQ bands silently forced to On=false + Channel=Stereo every block.  Lesson: APVTS prefix registration and audio-thread reader must be tested in lockstep when trackId conventions change.
- **kDPRTop = 49 in song-mode pattern playback** (fixed 2026-04-22) — leftover from the 14-slot drum era; pattern-mode at line ~492 used 51 (16 slots).  Slots 14-15 (FX 1, FX 2) were unreachable from song-mode playback for an unknown amount of time.  Bumped to 51 to match.  Lesson: when bumping fixed counts (14→16), grep for ALL constants referencing the old number.
- **trackId-portability hole in player loadPreset** (fixed 2026-04-22) — BaySickSynth/Bass/VibePlayer's `loadPreset` called `apvts.replaceState` directly with the loaded XML; loaded params with the saving instance's trackId-prefix never matched the loading instance's APVTS, so `replaceState` silently failed across instances.  Same-instance load worked (the failure was invisible until factory presets or cross-page preset sharing tried to load).  Fix: substitute the loaded trackId with the local instance's trackId on every PARAM id before `replaceState`.  Cross-applies to all 3 sample/synth player editors (VibePlayer used `_bsp_`, BaySickSynth `_bss_`, BaySickBass `_bsb_`).
- **Use-after-free in BaySickDrumsEditor mSlotEditor teardown during engine swap** (fixed 2026-04-22) — `setSlotEngine` destroyed the old processor BEFORE the editor (mSlotEditor) holding ParameterAttachments to its APVTS was destroyed.  The async editor-rebuild path then ran `~ParameterAttachment` → `removeListener` on freed parameter memory → CriticalSection deadlock.  Fix: added `onSlotEngineWillChange` callback fired SYNCHRONOUSLY before the swap so the editor can tear down attachments while the old proc is still alive.  Pattern lesson: any time a processor swap is paired with editor teardown, the editor must be torn down BEFORE the processor (sync, not async) — otherwise ParameterAttachments dangle.
- **DrumVoice mislabeled as "legacy" + slot picker hid 46 first-class voices** (caught 2026-04-22 during D-CC2/D-CC3 menu work) — DrumVoice is a fully-implemented 808/909-style synth with 46 voices and 794 lines of DSP; my earlier audit incorrectly labeled it deprecated.  Replaced with proper architectural read: DrumVoice was a placeholder for what BaySickSynth (after §P3's 11 DSP additions) now does directly.  Per Jeff's direction, all 46 voicings were rebuilt as BaySickSynth-format presets and DrumVoice DSP was UI-orphaned (kept alive for back-compat but unreachable from the slot picker).  Factory bank scaled up — see §P4.4 below.

**§P4 ship-status updates from Phase A implementation:**
- **§P4.1 Dual-engine drum slot dropdown: SHIPPED** in Phase A.  Slot picker is the folder-style two-section combo per spec (Sample / Synth Patch).  DrumSlot struct holds polymorphic engine (VibePlayer or BaySickSynth) + lazy proc + per-slot swapLock.  Editor dispatches polymorphically.  KIT-BREAK absorbed via state version 3.
- **§P4.4 Factory drum preset bank: PARTIAL SHIP** during Phase A as a scope-pull-in (Jeff needed drum sounds for testing once the legacy DrumVoice menu was removed).  107 BaySickSynth-format presets generated by `Tools/gen_factory_presets.py`: 56 drums + 42 synth + 9 bass.  All recipes from the §P3 catalogue translated to ValueTree XMLs in the appropriate `%APPDATA%/BaySickDAW/Presets/<player>/` folder.  Drum presets get `cutSelf=1` baked in (D-CC1).  Generator script is reusable + source-controlled.
  - **2026-04-26 update:** §P4.4 now FULLY shipped — see Phase E1 below for the expanded 790 presets / 87 kits / 29 templates ship.

**Phase A discovery items SHIPPED in Phase A:**
- D1 Slot copy/paste/duplicate (right-click context menu — Copy / Paste / Duplicate to ▸; locked targets refused; per-slot clipboard via file-scope juce::String survives editor rebuilds)
- D2 Drag-and-drop sample/folder/SFZ on slot bar (SlotBarButton inherits FileDragAndDropTarget; green-tinted hover feedback; locked targets silently refused; multi-file drop takes only first file — spreading deferred)
- D3 Per-slot polyphony cap (Mono/Poly toggle in context menu; sets BSS voiceMode or VP voiceCap to mono/poly equivalent on the slot's active engine)
- D6 Slot lock toggle (context menu; consumed by D1 paste, D1 duplicate, D2 drop, future D5 randomizer)
- T2 Multi-sample slot UI: NOT YET — VibePlayer DSP already supports round-robin / velocity layers / articulation groups via SFZ opcodes (verified at `VibePlayerDSP.cpp:196-232`); the missing piece is a GUI assembly surface.  PRESET-SAFE deferred to a follow-up session.
- T3 Choke groups (per-slot Int 0-8 in context menu; pre-pass in both render paths; noteOff injected into other-grouped slots' MIDI streams; persists in kit state)
- T6 Fill generator: NOT YET — deferred to Phase C piano-roll work where the toolbar lives
- T12 Per-slot MIDI Map (which incoming note triggers the slot — context menu submenu; 2-pass priority logic so remaps win over default mappings; pattern playback in song + pattern modes both updated to use slot's effective mapped note via `bsd->getSlotMidiNote(slot)`)
- T12b Per-slot MIDI Note (engine play pitch — emergent feature added during D3+T12 testing per Jeff's request; default 60 = C5 = current behavior; all `noteOn(1, 60, ...)` callsites in BaySickDrums switched to `noteOn(1, mSlots[slot].playPitch, ...)`; lets users drop kicks lower or push snares higher without leaving drum-grid mode)
- D-CC1 Cut-self default-ON for drum slots (slot ctor + setSlotEngine call setSlotCutSelfDefault to push cutSelf=1 onto the engine's APVTS; factory drum presets bake cutSelf=1 too)
- D-CC2 Right-click Automate componentID gap (existing wireID helper handled all 24 sliders; added `setComponentID` to the 3 missing non-slider attached components: reverse + cutSelf buttons, detuneMode selector.  BaySickDrums embeds VibePlayerEditor → inherits the fix automatically.  BSS/BSB/Harmless flagged as separate audit cycles per blueprint line 736)

**Bonus UX changes during Phase A (not in original audit):**
- Empty-slot placeholder rendering — drum-slot editor now shows "Pick a drum sound from the slot dropdown" instead of an empty VibePlayerEditor on freshly-launched slots.  Matches Layers/Bass blank-state UX.  Sub-editor only constructed when slot has actual content (sample loaded, built-in voice picked, or BaySickSynth preset loaded).  CPU savings are UI-side only (every slot still has its VibePlayerProcessor instance allocated; see lazy-proc deferred follow-up above).
- Removed transport-mode auto-switch — Builder→Song / piano-roll-tab→Pattern auto-switching deleted (was at StandaloneEditor.cpp tab-changed handler + 3 page `onSubTabChanged` lambdas).  PATTERN/SONG button in the transport bar is now the single source of truth.  Default Pattern (already was the UI default; processor's `mSongMode { false }` matches).  Reason: silent mode flips were confusing; the explicit toggle is clearer.

**Standing pattern established during Phase A:** When swapping a per-slot processor whose APVTS is bound to UI parameter attachments (ParameterAttachment, SliderAttachment, ButtonAtt), **the editor must be torn down BEFORE the processor is destroyed** — synchronously, not via a queued/async path.  Async editor teardown after processor destruction = use-after-free in `~ParameterAttachment` → `removeListener` on freed CriticalSection.  Implementation pattern: emit a "willChange" callback synchronously before the swap (callers tear down editors), then perform the swap, then emit a "changed" callback (callers rebuild editors).  Used by `BaySickDrumsProcessor::setSlotEngine` (`onSlotEngineWillChange` + `onSlotEngineChanged`).  Apply this same pattern to any future processor-swap surface.

**State save/load schema bump for `.bsd` kit format (v2 → v3):** New per-slot XML attributes added during Phase A (all default to old behavior on missing-attr load):
- `engineKind` (int 0 = VibePlayer, 1 = BaySickSynth) — P4.1
- `chokeGroup` (int 0..8, 0 = no group) — D-CC3
- `monoMode` (0/1) — D3
- `locked` (0/1) — D6
- `midiNote` (int -1..127, -1 = default `kBaseMidiNote+slot` mapping) — T12 input map
- `playPitch` (int 0..127, default 60 = C5) — T12b engine play pitch

Old v2 kits load correctly into v3 — missing attributes default to values that preserve pre-Phase-A behavior.

**Phase B kickoff finding (B1 verification, 2026-04-22):**
The Phase A audit speculated `mDrumsEQDSP` was a duplicate of the 5F-4a `mixer_drums_*_eq*` post-rack bus EQ.  Verification at `VibeGraph.cpp:343-367` (DrumsBusNode::processBlock) shows the two EQs run at DIFFERENT chain positions:
```
drums.renderNextBlock(buf)
  → pageEq.process(buf)    ← mDrumsEQDSP (passed-in reference) — PRE-rack
  → rack.process(buf)
  → busEq.process(buf)     ← owned member, mixer_drums_*_eq* — POST-rack
```
**Important historical context (per Jeff):** the drums bus having both pre+post EQ is NOT intentional architecture — it's an accident.  `mDrumsEQDSP` was originally the drums bus EQ (pre-5F-4a era, originally a 6-band EQ with the legacy `drums_mid_eq*` / `drums_side_eq*` naming).  A prior session accidentally also bound it to DrumsPage's EQ tab without Jeff asking.  When 5F-4a later added the standard per-strip post-rack `mixer_drums_*_eq*`, the side-by-side coexistence emerged — not by design.

Implications for §P4.3 (now corrected):
- **DO NOT recycle `mDrumsEQDSP` as the new drums-bus pre-rack EQ.**  It carries legacy naming + may behave differently from the standard EQ8MsDSP wiring.  Build the new pre-rack EQs FRESH using the standard `addParamsForTrackEQ`-style helper extended for `_preeq_` so every bus + insert pre-EQ behaves identically.
- Same caution for `mLayerPageEQs[]` / `mBassPageEQs[]` arrays — don't recycle.  They're per-page DSPs with their own registration; the new universal pre-rack EQs live on InsertNode/BusNode and use the standard machinery.
- B2 adds a universal `mPreEQ` to every bus + insert node (fresh EQ8MsDSP instances).
- B5 rebinds DrumsPage EQ tab from `mDrumsEQDSP` to per-slot Drum InsertNode pre-EQs (16 instances).  Layers/Bass page EQ tabs rebind to their respective insert pre-EQs.
- B7 deletes `mDrumsEQDSP` + `drums_mid_eq*`/`drums_side_eq*` APVTS + the legacy `mLayerPageEQs[]`/`mBassPageEQs[]` arrays + their `tk_lay_*` / `tk_bas_*` per-page EQ params.

**B2 SHIPPED (2026-04-22):** Fresh `EQ8MsDSP preEq` member added to all 7 audio nodes that own a rack — InsertNode (covers Layer/Bass/Drum/Audio/Aux uniformly) + LayersBusNode + BassBusNode + DrumsBusNode + MasterBusNode + EffectsBusNode + InstrChannelNode (used by Audio Clips Bus).  Each node's `prepare()` and `reset()` updated to include the new member.  Each node's processBlock inserts `preEq.process(buf)` at the very start of its chain, before polarity/width/rack/post-EQ.  DrumsBusNode runs the new preEq alongside the legacy `pageEq` (mDrumsEQDSP) until B7 retires the legacy path — both default to flat so audio is unchanged.  6 new accessor methods exposed on VibeGraph: `getLayersBusPreEQ` / `getBassBusPreEQ` / `getDrumsBusPreEQ` / `getMasterPreEQ` / `getEffectsBusPreEQ` / `getAudioClipsBusPreEQ` plus `getInsertPreEQ(InsertKind, int)`.  PRESET-SAFE — defaults don't alter signal until B5 wires UI binding + B3 registers APVTS.

**B2 drive-by fix (logged separately for visibility):** Audio Clips Bus post-rack EQ was registered + APVTS-synced via `updateAllPostRackEQsFromApvts` but `eq.process(clipsBus)` was never called in PluginProcessor's clips-bus inline render path.  The bus had a non-functional EQ accessor for an unknown duration.  Fixed by adding `eq->process(clipsBus)` after the rack stage, symmetric with other bus nodes' chain order.  PRESET-SAFE (the EQ defaults to flat so silent until user touches it).

**Files touched in B2:**
- `Source/VibeGraph.cpp` — preEq member + prepare/reset/processBlock additions in 7 node structs; 7 new accessor implementations
- `Source/VibeGraph.h` — 7 new accessor declarations
- `Source/PluginProcessor.cpp` — clips-bus inline path: pre-EQ added + post-EQ drive-by fix

**B3 SHIPPED (2026-04-22):** Pre-rack EQ APVTS registration.  Refactored the existing `addParamsForTrackEQ(prefix)` to call a shared internal `addParamsForEQBank(prefix, subPrefix)` helper.  New public `addParamsForTrackPreEQ(prefix)` calls the helper with `subPrefix = "preeq_"`.  Param IDs use the convention `prefix + "_preeq_mid_eq{b}{Suffix}"` (mirror of post-EQ's `prefix + "_mid_eq{b}{Suffix}"` with `_preeq_` token inserted).  Human-readable param names use "Pre EQ" (vs post's "EQ") so automation menus disambiguate.  `ensureMixerStripParams` now calls BOTH `addParamsForTrackEQ` (post) AND `addParamsForTrackPreEQ` (pre) for every mixer strip — master + 5 buses + every Layer/Bass/Drum/Audio/Aux insert all gain `_preeq_*` params at registration time.  Each pre-EQ bank is the same 25 params per band × 8 bands × 2 sides = 400 params per strip.  PRESET-SAFE — defaults match post-EQ defaults (Bell type, 0 dB, On=true, channel=Mid/Side per side).  Old kits load without these params; defaults flat = identity.

**Files touched in B3:**
- `Source/PluginProcessor.cpp` — `addParamsForTrackEQ` refactored, new `addParamsForEQBank` helper + `addParamsForTrackPreEQ`; `ensureMixerStripParams` calls both
- `Source/PluginProcessor.h` — 2 new method declarations

**B4 SHIPPED (2026-04-22):** Pre-rack EQ APVTS sync.  New `updateAllPreRackEQsFromApvts()` mirrors the existing `updateAllPostRackEQsFromApvts()` — walks every bus + insert pre-EQ instance, calls `updateEQFromApvts(eq, "_preeq_mid_eq", "_preeq_side_eq")` to push APVTS values into the DSP each block.  Reuses the existing `updateEQFromApvts` helper (no new sync logic — just feeds it the new prefix family).  Wired into `processBlock` immediately after the existing post-rack sync call.  PRESET-SAFE — DSP defaults to flat, APVTS defaults to flat, no behavior change until B5 wires UI binding.

**Files touched in B4:**
- `Source/PluginProcessor.cpp` — new `updateAllPreRackEQsFromApvts` + processBlock call
- `Source/PluginProcessor.h` — declaration

**B2-B3-B4 form a complete inert subsystem:** every bus/insert has a fresh pre-EQ DSP, has its `_preeq_*` APVTS params registered at strip-creation time, and the per-block sync pushes those params into the DSP every audio block.  Nothing is audibly changed yet because no UI binds to the new params.  The user-facing surface lands in B5 (page tabs rebind to insert pre-EQs; DrumsPage gets slot dropdown) and B6 (mixer effects-page tab renames + the new Pre EQ8 M/S tab on aux/audio/bus).

**B2-B4 perf fix shipped immediately (2026-04-22):** Initial B2-B4 build doubled DSP CPU (≈30% → 60%) because every bus + insert was now processing TWO EQ8MsDSP instances per block (preEq + eq) regardless of whether either had been touched.  Mitigated by adding `EQ8DSP::isIdentity()` (and matching `EQ8MsDSP::isIdentity()` that ANDs both inner EQs).  An EQ band is identity when: `!on`, OR `muted`, OR type 6 (explicit OFF), OR (gain-bearing type — Bell/LowShelf/HighShelf/Tilt — with `|gainDb| < 0.001 dB` AND not dynamic).  Filter types (LP/HP/BP/Notch) are conservatively considered non-identity even at extreme cutoffs.  All 14 EQ `process(buf)` callsites in `VibeGraph.cpp` (5 bus pre-EQs + 5 bus post-EQs + InsertNode pre + InsertNode post + DrumsBusNode legacy pageEq + InstrChannelNode pre/post via PluginProcessor inline) now wrap with `if (! eq.isIdentity()) eq.process(buf);`.  Cost of `isIdentity()` is 8 cheap comparisons per inner EQ (16 per EQ8MsDSP); negligible compared to skipped IIR work.  Benefits POST-rack EQ too (same skip applies) — total DSP can now drop BELOW the original ~30% baseline if many post-EQs were also untouched.

**Files touched in B2-B4 perf fix:**
- `Source/DSP/EQ8DSP.h` — `isIdentity()` declaration
- `Source/DSP/EQ8DSP.cpp` — `isIdentity()` implementation
- `Source/DSP/EQ8MsDSP.h` — `isIdentity()` inline (mid AND side)
- `Source/VibeGraph.cpp` — 12 EQ-process callsites wrapped with isIdentity check
- `Source/PluginProcessor.cpp` — 2 clips-bus EQ-process callsites wrapped

**B2-B4 perf fix Option A only got 60% → 53%, so Option E shipped 2026-04-22:** APVTS dirty-flag short-circuit on EQ sync.  `VibeSynthProcessor` now privately inherits `juce::ValueTree::Listener`; constructor calls `apvts.state.addListener(this)` (destructor removes).  Override `valueTreePropertyChanged` simply sets `mEQsDirty.store(true)`.  In `processBlock`, the `updateAllPost+PreRackEQsFromApvts` calls are now wrapped with `if (mEQsDirty.exchange(false)) { ... }`.  Default `mEQsDirty=true` so the first block always syncs (catches initial state load).  Subsequent blocks pay only 1 atomic load + skip — eliminates the ~1.4M string-concat hash lookups/sec the EQ syncs were doing on the audio thread when nothing had changed.  Catches every change source uniformly because APVTS routes UI/automation/host setValue calls through ValueTree::setProperty under the hood — listener fires in all cases.

**Files touched in B2-B4 perf fix Option E:**
- `Source/PluginProcessor.h` — class inherits `juce::ValueTree::Listener`; `valueTreePropertyChanged` override + `mEQsDirty` atomic flag
- `Source/PluginProcessor.cpp` — listener register/unregister in ctor/dtor; processBlock wraps sync calls in dirty-flag exchange

**Result (verified 2026-04-22):** DSP dropped from a ~30% baseline (pre-Phase-B) to ~1% idle, with usage scaling correctly (a couple % per actively-tweaked EQ).  Audio path verified working: sounds play, EQ tweaks audibly affect signal.  The "30% baseline" was actually being eaten by EQ sync waste running on the audio thread — `updateAllPostRackEQsFromApvts` did ~1.4M string-concat hash lookups per second every block whether anything had changed or not, plus every flat post-EQ ran its full 8-band IIR chain.  The Phase B perf fixes (isIdentity + dirty flag) accidentally repaired this pre-existing waste while also eliminating the new pre-EQ overhead.  Net change vs pre-Phase-B: ~30% CPU reduction, not just a perf "neutral" addition.

**Standing pattern (apply to all future APVTS-synced DSP):**
1. **Per-DSP `isIdentity()` check** — every DSP module that runs a non-trivial process loop should expose a cheap method that returns true when its current parameter state produces a pass-through (identity) result.  Callers wrap process() with `if (! dsp.isIdentity()) dsp.process(buf);`.
2. **APVTS state listener + dirty flag** — for any sync function that walks APVTS to push values into DSP, gate the entire walk behind a `std::atomic<bool> dirty` flag.  Subscribe to `apvts.state` via `juce::ValueTree::Listener` once at construction; mark dirty on `valueTreePropertyChanged`; clear via `exchange(false)` at sync time.  Default `dirty=true` so first block always runs.
3. **Listener catches all change paths uniformly** — UI sliders, automation, host setValue, preset loads all route through `ValueTree::setProperty` under the hood.  No need for per-param listeners or change-tracking infrastructure.

These two patterns together eliminate ALL idle-block overhead from APVTS-driven DSP modules.  Future B-phase work on rebinding pre-EQ to UI tabs, post-§P4 cross-apply sessions, and any new effect-rack module additions should default to this pattern.  Existing modules that may benefit from retroactive application: BaySickSynth/Bass/VibePlayer per-block `updateFromApvts` (each runs hash lookups every block); per-effect-rack-slot DSPs that sync APVTS every block.

**B5 SHIPPED (2026-04-22):** Player-page EQ tabs rebound to InsertNode pre-rack EQs.

LayersPage::selectEngine now binds `mEQDisplay` to `vg.getInsertPreEQ(InsertKind::Layer, mPageIndex)` with `mixer_layer_<i>_preeq_mid_eq` / `_preeq_side_eq` prefixes (replacing the legacy `&mEQDsp` + `tk_lay_<i>_*` path).  Same for BassPage with `InsertKind::Bass` + `mixer_bass_<i>_*` prefixes.  Both fall back to the legacy `mEQDsp` for display-only if the InsertNode isn't yet present (defensive — registerXxxEngine creates it just before the bind).

DrumsPage gets per-slot rebinding: `buildFXEQTab` no longer hardcodes a bind to `mProcessor.mDrumsEQDSP`.  Instead it binds to the active slot's pre-EQ via new `rebindEQDisplayToSlot(int)` helper.  BaySickDrumsEditor exposes `getActiveSlot()` accessor + new `onActiveSlotChanged` callback fired from `setActiveSlot`.  DrumsPage subscribes in its ctor and rebinds the EQ display whenever the user changes slots.  Initial bind in `buildFXEQTab` reads the editor's current active slot.

Page tab labels renamed in StandaloneEditor's `setTabSlots` calls: Layers/Bass/Drums all now show `[Player | Piano Roll | Pre EQ8 M/S]`.  (Drums had "Sound" → "Player" pulled in here too — was promised in P4.1 audit lock and slipped this iteration.)

Legacy DSP audio path UNCHANGED in B5: `mLayerPageEQs[i]`, `mBassPageEQs[i]`, and `mProcessor.mDrumsEQDSP` still get processed in their old positions in PluginProcessor.cpp.  All default to flat post-rebind so they have no audible effect (the user's tweaks now go through the new InsertNode pre-EQs via the new APVTS prefixes).  B7 removes the legacy DSPs + their audio-thread wiring + their old-prefix params.

**Files touched in B5:**
- `Source/Standalone/LayersPage.cpp` — selectEngine EQ bind switched to InsertNode pre-EQ
- `Source/Standalone/BassPage.cpp` — same pattern
- `Source/Standalone/DrumsPage.h` + `.cpp` — `rebindEQDisplayToSlot(int)` helper, ctor wiring
- `Source/BaySickDrums/BaySickDrumsEditor.h` + `.cpp` — `getActiveSlot()` + `onActiveSlotChanged` callback
- `Source/Standalone/StandaloneEditor.cpp` — page tab labels (3 sites: Layers/Bass/Drums)

**B5 follow-up bug fix (2026-04-22):** Spectrum analyser display went blank on EQs that hadn't been touched.  Root cause: the Option-A `isIdentity` short-circuit at the call sites skipped `eq.process(buf)` entirely when the EQ was flat — but the spectrum feeds (`preFeed` / `postFeed`) are populated INSIDE that method.  No process call → no feed pushes → empty waveform display.  Fix: move the identity short-circuit INSIDE `EQ8MsDSP::process` (treat it like the existing `bypassed` early-out which already pushes the same input samples to both pre+post feeds).  Removed the now-redundant `&& ! eq.isIdentity()` wraps at all 14 call sites — process() self-short-circuits.  Architectural improvement: callers don't need to know about the perf optimization; the EQ class owns both its perf contract AND its UI-feed contract.

**Files touched in B5 follow-up:**
- `Source/DSP/EQ8MsDSP.cpp` — `if (bypassed)` extended to `if (bypassed || isIdentity())`, comment updated
- `Source/VibeGraph.cpp` — 12 callsites: removed `&& ! eq.isIdentity()` wraps
- `Source/PluginProcessor.cpp` — 2 clips-bus callsites: removed `&& ! eq->isIdentity()` wraps

**B5 follow-up #2 (2026-04-22):** Drums Kit + Nav (slot-selector) combo only appeared on the Player tab (was gated behind `if (i == 0)` in the tab-change callback + initial state).  Per E.1 audit lock the slot dropdown is supposed to be visible on ALL three Drums tabs (Player, Piano Roll, Pre EQ8 M/S) so the EQ tab can use it to pick which slot's pre-EQ to view (and Piano Roll will use it for keyboard-mode slot selection in P4.2).  Removed the `if (i == 0)` gate — Kit + Nav now always added after `clearExtraRightComponents`.

**Files touched in B5 follow-up #2:**
- `Source/Standalone/StandaloneEditor.cpp` — DrumsPage tab callback + initial state: Kit/Nav added unconditionally

**B6.1 SHIPPED (2026-04-22):** EffectsPage tab strip rename "EQ8 M/S" → "Post EQ8 M/S" (single label change in StandaloneEditor's `setTabSlots` call for EffectsPage).  Disambiguates from the new "Pre EQ8 M/S" page tab on player pages.  Layer/Bass/Drum-slot Effects pages now show `[Rack | Post EQ8 M/S]` (final state per spec).  Aux/Audio/Bus Effects pages still show `[Rack | Post EQ8 M/S]` only — the third Pre EQ8 M/S tab for those will land in B6.2 (requires adding a new ParametricEQDisplay component + dynamic 2-vs-3 tab logic based on selected channel's kind).

**Files touched in B6.1:**
- `Source/Standalone/StandaloneEditor.cpp` — single label change

**B6.2 SHIPPED (2026-04-22):** Pre EQ8 M/S tab added to EffectsPage with dynamic 2-vs-3 tab visibility based on selected channel kind.

EffectsPage gains a `TabKind` enum (PreEQ/Rack/PostEQ), a new `mPreEQTab` Component + `mPreEQDisplay` ParametricEQDisplay member + `mPreEffectsEQDsp` (display fallback before channel binding lands), and `buildPreEQTab()` that mirrors `buildEQTab()`.  `currentChannelHasPagePreEQ()` returns true for IDs 100-115 (Drum) / 200-207 (Layer) / 300-303 (Bass) — those channels' pre-EQ lives on the player page, so the mixer Effects page hides the Pre tab.  Aux/Audio/Bus channels (no page) get the full 3-tab layout.

`switchTab` now has two overloads: the legacy `switchTab(int)` interprets the visible-tab index via `tabKindForVisibleIndex(int)` and dispatches to the new `switchTab(TabKind)`, which sets visibility on all 3 tab Components.  Index↔TabKind mapping is dynamic per current channel kind.

`onChannelChanged` extended: also resolves the new pre-rack EQ via `vg.getXxxBusPreEQ()` / `vg.getInsertPreEQ(kind, idx)` and binds `mPreEQDisplay` with the `mixer_<kind>_<N>_preeq_*` APVTS prefix.  Then fires the new `onTabsNeedRefresh` callback so StandaloneEditor can re-call `setTabSlots` with the right 2-vs-3 label list.

StandaloneEditor's EffectsPage tab setup refactored into a `setupEffectsTabs` lambda that's called initially AND wired into `ep->onTabsNeedRefresh`.  Tab callback uses `tabKindForVisibleIndex` to decide whether the clicked tab is an EQ tab (→ show MID/SIDE buttons + sync the EQ hamburger menu) and which EQ display to point the hamburger at (Pre or Post).  `setEQMid` propagates to BOTH EQ displays so the MID/SIDE state is unified across pre and post views.

PRESET-SAFE.  The new pre-rack EQ params on Aux/Audio/Bus were registered in B3 (`ensureMixerStripParams` already calls `addParamsForTrackPreEQ` for every strip kind); B6.2 just exposes the UI for them.

**Files touched in B6.2:**
- `Source/Standalone/EffectsPage.h` — TabKind enum, public `currentChannelHasPagePreEQ` / `tabKindForVisibleIndex` / `visibleIndexForTabKind` / `getPreEQDisplay` / `switchTab(TabKind)` / `onTabsNeedRefresh`; new `mPreEQTab` / `mPreEQDisplay` / `mPreEffectsEQDsp` members + `buildPreEQTab()` decl
- `Source/Standalone/EffectsPage.cpp` — `buildPreEQTab` impl, `currentChannelHasPagePreEQ` + `tabKindForVisibleIndex` + `visibleIndexForTabKind` impls, `switchTab(int)` delegates via TabKind, `switchTab(TabKind)` shows/hides 3 tabs, `setEQMid` propagates to pre-display, `onChannelChanged` binds pre-EQ + fires `onTabsNeedRefresh`, `resized` lays out pre-tab
- `Source/Standalone/StandaloneEditor.cpp` — EffectsPage tab setup refactored into `setupEffectsTabs` lambda; wired into `ep->onTabsNeedRefresh`; tab callback uses TabKind-aware logic for MID/SIDE visibility + EQ hamburger sync

**B6.2 follow-up bug fixes (2026-04-22):** Two bugs surfaced during verification.
- **Bug 1: Player-page tab buttons stomped with EffectsPage's labels.** `setupEffectsTabs` (wired to `ep->onTabsNeedRefresh`) called `setTabSlots` unconditionally — when channel selection changed on EffectsPage while the user was viewing a player page, the player page's tab slots got overwritten.  Fix: gate `setupEffectsTabs` with `if (mVisiblePage != ep) return;` at the top.
- **Bug 2: MID/SIDE button visibility inconsistent across Rack vs EQ tabs.** My refactor moved the authoritative `setMidSideVisible(...)` call INSIDE `setupEffectsTabs` (which fires first), then `setMidSideSlots` ran after, with side-effects that could re-show the buttons.  Fix: reordered to call `setMidSideSlots` BEFORE `setupEffectsTabs()` so setupEffectsTabs has the final word on visibility.

**Files touched in B6.2 follow-up:**
- `Source/Standalone/StandaloneEditor.cpp` — guard added; setMidSideSlots reordered before setupEffectsTabs

**B7 SHIPPED (2026-04-22):** Legacy per-page EQ cleanup — every DSP instance, APVTS block, audio-path wiring, register API, page-owned member, and helper tied to the pre-B2 page-EQ model is deleted.

Removed DSPs + members:
- `VibeSynthProcessor::mDrumsEQDSP` (page-owned EQ8MsDSP that DrumsPage used to bind)
- `mLayerPageEQs[8]` + `mLayerEQLock` / `mBassPageEQs[4]` + `mBassEQLock` / `mDrumsPageEQ` + `mDrumsEQLock` (non-owning ptr arrays into LayersPage/BassPage's local DSPs)
- `LayersPage::mEQDsp`, `BassPage::mEQDsp` (page-owned pre-rack EQ DSPs)
- `VibeGraph::DrumsBusNode::pageEq` reference + ctor arg (external EQ ref no longer needed — the node owns its own `preEq`)

Removed APVTS blocks:
- `drums_mid_eq{0..7}*` / `drums_side_eq{0..7}*` (18 params × 8 bands × 2 sides — the Drums bus pre-rack EQ, which fed `mDrumsEQDSP`)
- `tk_lay_<N>_mid_eq*` / `tk_lay_<N>_side_eq*` / `tk_bas_<N>_mid_eq*` / `tk_bas_<N>_side_eq*` — removed by dropping `addParamsForTrackEQ(prefix)` from `registerParamsForTrack` (the per-track lazy EQ that fed `mLayerPageEQs[i]` / `mBassPageEQs[i]`).  `addParamsForTrackEQ` itself is retained because `ensureMixerStripParams` still uses it for the post-rack EQ on every mixer strip.

Removed APIs + update fns:
- `registerLayerPageEQ` / `unregisterLayerPageEQ` / `registerBassPageEQ` / `unregisterBassPageEQ` / `registerDrumsPageEQ` / `unregisterDrumsPageEQ`
- `updateDrumsEQ` / `updateLayerPageEQsFromApvts` / `updateBassPageEQsFromApvts` (all 3 bodies — pre-rack EQ sync is now entirely inside `updateAllPreRackEQsFromApvts` which iterates every registered mixer strip prefix)
- `LayersPage::midEQPrefix` / `sideEQPrefix` / `syncEQFromApvts` (EQ display binds directly to `getInsertPreEQ` + mixer-strip prefix)
- `BassPage::midEQPrefix` / `sideEQPrefix` / `syncEQFromApvts`
- `EffectsPage` / `DrumsPage` legacy fallback `bindMsDSP(&mProcessor.mDrumsEQDSP)` — InsertNode preEq is guaranteed to exist for registered slots

Removed audio-path wiring:
- PluginProcessor::processBlock — the two `{ SpinLock::ScopedTryLockType eqLk(mXxxEQLock); if (... mXxxPageEQs[i] ...) mXxxPageEQs[i]->process(...); }` blocks in the Layer + Bass render loops
- VibeGraph::DrumsBusNode::processBlock — the `pageEq.process(buf)` call that ran after `preEq.process(buf)` (legacy was doubled during the B2-B6 transition so audio stayed unchanged; now only the node's own preEq runs)
- VibeSynthProcessor::prepareToPlay — `mDrumsEQDSP.prepare(...)` + the `drumsEQ` ref threaded through `buildFixedTopology`

Signature changes:
- `VibeGraph::buildFixedTopology(synth, bass, drums, drumsEQ, apvts)` → `buildFixedTopology(synth, bass, drums, apvts)`
- `DrumsBusNode::DrumsBusNode(d, e, m)` → `DrumsBusNode(d, m)`

Behavior invariants after B7: every mixer strip (master / 6 buses / up to 94 inserts including 16 drum slots + 16 aux) runs exactly one pre-rack EQ (`preEq`) and one post-rack EQ (`busEq`), sync'd from `mixer_{prefix}_preeq_*` + `mixer_{prefix}_*` APVTS blocks respectively, dirty-flag gated, with identity short-circuit + spectrum-feed-preserving semantics intact.  No audible change expected — prior legacy DSPs were defaulting flat and running in parallel with the new preEq chain since B2.  PRESET-SAFE (no new params added; the removed `drums_*_eq*` block has no factory preset consumers — the user's project files use `mixer_drumsbus_preeq_*` since B2).

**Files touched in B7:**
- `Source/PluginProcessor.h` — dropped register/unregister PageEQ APIs, `mDrumsEQDSP`, 3 SpinLocks + 3 arrays, 3 update-fn decls
- `Source/PluginProcessor.cpp` — dropped `drums_*_eq*` param block from `createParameterLayout`, `mDrumsEQDSP.prepare` + the `drumsEQ` ref in `buildFixedTopology`, 3 update-fn bodies + their `processBlock` call sites, 2 per-page EQ blocks in the Layer/Bass render loops, 6 register/unregister bodies, `addParamsForTrackEQ(prefix)` call in `registerParamsForTrack`
- `Source/VibeGraph.h` — updated `buildFixedTopology` signature + doc; dropped stale doc about page EQs
- `Source/VibeGraph.cpp` — dropped `pageEq` member + ctor arg from `DrumsBusNode`; dropped `pageEq.process` call in its `processBlock`
- `Source/Standalone/LayersPage.h` — dropped `mEQDsp`, `syncEQFromApvts`, `midEQPrefix`, `sideEQPrefix`
- `Source/Standalone/LayersPage.cpp` — dropped `registerLayerPageEQ` call, the legacy `mEQDsp` bind + fallback branch, `syncEQFromApvts` call in timer
- `Source/Standalone/BassPage.h` / `.cpp` — same set of changes as LayersPage
- `Source/Standalone/DrumsPage.cpp` — removed legacy `mDrumsEQDSP` fallback in `rebindEQDisplayToSlot`

**B8 SHIPPED (2026-04-22):** Automation display resolver now recognises the `preeq_` token on mixer-strip prefixes.

- `StandaloneEditor::resolveAutomationDisplayName` → `formatMixerSuffix` inner lambda: if the suffix starts with `preeq_`, strip the token and emit `"Pre EQ Mid/Side B{n} {Param}"` instead of `"EQ Mid/Side B{n} {Param}"`.  Example — `mixer_layer_0_preeq_mid_eq3Freq` now renders as `"Mx Layer 1 - Pre EQ Mid B4 Freq"` (was `"Mx Layer 1 - Preeq Mid Eq3Freq"` prettify-fallback garbage pre-B8).
- Dead `tryPageEQTab` lambda (matched `tk_{pagePrefix}_{N}_mid_eq*` / `_side_eq*` — all gone in B7) deleted.  Page + mixer pre-rack EQ write to the same `mixer_{kind}_{N}_preeq_*` params now, so both UIs produce identical `"Mx ..."` automation labels.
- Header comment about the drum-slot / drums-bus EQ surface updated to match the new reality (drums bus EQ no longer lives at `drums_{mid,side}_eq*` — resolves through `mixer_drum_{N}_preeq_*` via `tryMixerNonSlot`).

**Files touched in B8:**
- `Source/Standalone/StandaloneEditor.cpp` — `formatMixerSuffix` preeq_ branch + `tryPageEQTab` deletion + resolver header comment refresh

---

## Project Persistence (new phase, inserted between Phase B and Phase C of §P4)

**Context (locked 2026-04-23):** Audit during Phase C1 kickoff discovered patterns don't persist to disk at all — `PatternManager::toValueTree`/`fromValueTree` exist but are never called, and even if they were, they don't serialize `layerRoll`/`bassRoll`/`drumRoll` (the PianoNote vectors where all the musical content lives).  `VibeSynthProcessor::getStateInformation` saves only APVTS + rack states.  This makes the app effectively unable to save user work — an unacceptable gap for v1.  Phase C paused; persistence inserted as a blocker phase.

**Scope locks (2026-04-23, layout finalized after a unified-folder revision):**
- **Unified user folder:** `Documents\BaySickDAW\` holds every user-visible artifact — projects, recordings, presets, templates, settings.  The CoreLibrary sample bundle stays in `%LOCALAPPDATA%\BaySickDAW\CoreLibrary\` (heavy, installed, not user-edited), with a `Sample Library.lnk` shortcut in the Documents folder for browse access.
- **Projects folder model:** `Documents\BaySickDAW\Projects\<ProjectName>\` — a folder per project containing `project.xml` + `Samples/` subfolder for copy-on-drop audio.  Matches the existing `Documents\BaySickDAW\Recordings\` convention.  Beginner-first.
- **Bundled-by-default:** samples dropped on the Builder are copied into `<ProjectFolder>/Samples/` on drop.  No external-reference mode in v1.
- **Name-only prompt:** New Project takes a text name, app picks the location — user never navigates the file system.  Collision handling: auto-append ` (2)` / ` (3)`.
- **Save As:** GarageBand model — duplicate-to-new-name-in-same-root, no file dialog.
- **Open Project:** custom Project Browser window (not a system file picker) listing the Projects folder's subfolders.
- **Delete:** in-app right-click → `moveToTrash` (Recycle Bin) with confirmation.
- **Rename:** only on projects NOT currently open.
- **Autosave:** 15-minute interval, writes `project.xml.bak` sidecar.
- **Dirty tracking:** APVTS + PatternManager + VibeGraph rack changes dirty the project; audio device settings (sample rate / device) are global, don't dirty.
- **Startup:** empty default; later Settings will allow designating a "default template" project that seeds new projects.
- **Recent Projects:** last 10, persisted in `%APPDATA%/BaySickDAW/settings.xml`.  Missing-folder entries grey out.
- **Project name validation:** Windows filename rules — reject `<>:"/\|?*` + reserved device names (CON, PRN, AUX, NUL, COM1..9, LPT1..9) + trailing dots/spaces.
- **Legacy `getStateInformation`/`setStateInformation` blob format kept** untouched for backward compat; new `serializeProject`/`deserializeProject` XML surface lives alongside it.
- **Plugin (VST) mode is not shipped in v1** — the legacy `juce_add_plugin` CMake target exists but isn't built.  File menu lives in standalone only.

**Ship order (sub-batches P1-P6):**
- **P1** — Schema + serialization (no UI).  Extend `PatternManager::toValueTree/fromValueTree` to cover everything.  New `ProjectManager` skeleton with lifecycle methods.  `VibeSynthProcessor::serializeProject/deserializeProject`.
- **P2** — File menu + New Project dialog + Save / Save As (basic flow).
- **P3** — Project Browser window (Open / Recent submenu / Rename / Duplicate / Delete).
- **P4** — Copy-on-drop integration with Builder + `Samples/` folder management.
- **P5** — Autosave timer + dirty tracking + close-with-unsaved-changes prompt.
- **P6** — Welcome screen + default-template support (hook exists in `newProject` from P1 — P6 adds the UI for setting a default).

**P1 SHIPPED (2026-04-23):** Schema + serialization plumbing.  No UI yet.

- **`PatternManager::toValueTree` rewritten** — full round-trip for every field on `Pattern`, `PatternManager`, `PianoNote`, `PianoRollData`, `ArrangementBlock`, `AutomationLane`, `ControlPoint`, `MixerState`, `PageSequenceData`, `BasicStep`, `ComplexStep`.  New coverage:
  - Piano-roll notes (`layerRoll[0..7]`, `bassRoll[0..kMaxBassPages-1]`, `drumRoll`) including every field on `PianoNote` (midi, beat, duration, velocity, panning, finePitch, type, muted, groupId, filterCutoff, **slotIndex** for C1 forward-compat).  Compact per-note XML: short attribute names, default-value-omission keeps XML small for large patterns.
  - Full `MixerState` — previously only bus levels + mutes; now includes solos, all pans, per-drum-row level/pan arrays (CSV-packed), per-audio-row level + mute arrays (CSV-packed), audio clips bus level/pan/mute/solo.
  - Full `PageSequenceData` — previously only the basic envelope + routing/bars/spb; now includes basic grid steps, complex grid steps, complex envelope (attack/decay/sustain/hold/release + swing + triplet).  Row-level + step-level default-omission keeps empty rows out of the output.
  - Full `ArrangementBlock` — previously only patternIndex/startBar/lengthBars/layerTrack; now includes trackRow, clipType, audioFilePath, displayAlias, pitchSemitones, originalBPM, stretchMode, muted, and nested AutomationLane for automation clips.
  - `drumRowToSlot` per pattern (CSV values).
  - `mDrumEnabled` (packed bits).
  - Per-track row mute/solo (`mRowMuted` / `mRowSoloed`, packed bits) + `mAnyRowSoloed` derived on load.
  - `mAudioLibrary` entries (path + alias).
  - `mAutomationTemplates` library.
- **`PatternManager::fromValueTree` rewritten** — matching loader.  Missing-attr reads fall back to struct-default values so older state tree formats load forward.  Arrangement + libraries are cleared at entry (full replace, not merge).
- **Schema version tag** — `<PatternManager version="1">` root attribute.  Future format bumps will branch on this.
- **New `Source/ProjectManager.h` + `.cpp`** — skeleton lifecycle class.  API surface: `newProject(name, templatePath)`, `openProject(folderOrXml)`, `saveProject()`, `saveProjectAs(newName)`, static `deleteProject(folder)` + `renameProject(folder, newName)`, `importSample(externalFile)` (P4 helper).  Helpers: `getDefaultProjectsRoot()`, `getSettingsFile()`, `sanitizeProjectName()`, `isValidProjectName()`.  Recent-projects list persisted to `%APPDATA%/BaySickDAW/settings.xml` under `<RecentProjects>` — survives across runs.  Collision-safe (appends `" (N)"`), template-aware (copies template's `project.xml` + `Samples/` on new-project creation).
- **`VibeSynthProcessor::serializeProject(XmlElement&)` + `deserializeProject(XmlElement&)`** — compose APVTS state (with VibeRackStates child) + PatternManager's ValueTree into a single `<BaySickDAWProject version="1">` XML document.  Legacy `getStateInformation`/`setStateInformation` blob format untouched — it still wraps the same processor-state tree for backward compat with any on-disk data from pre-persistence runs.
- **`VibeSynthProcessor::getPatternManager()` getter** added (it was set-only before).

**Known-limitation / deferred to later P-batches:**
- No UI wiring yet (P2 adds File menu + Save/SaveAs/New dialogs).
- No Builder copy-on-drop yet (P4 wires `importSample` into the arrangement drop path).
- No dirty tracking or autosave (P5).
- No Project Browser / Open window (P3; for now `openProject` can be called programmatically only).
- No welcome screen / default-template UI (P6; the hook exists in `newProject(name, templatePath)` since P1).

**Files touched in P1:**
- `Source/PatternManager.cpp` — extended `toValueTree`/`fromValueTree` + anonymous-namespace helpers (`noteToValueTree`, `noteFromValueTree`, `rollToValueTree`, `rollFromValueTree`, `automationLaneToValueTree`, `automationLaneFromValueTree`)
- `Source/ProjectManager.h` + `.cpp` (NEW)
- `Source/PluginProcessor.h` + `.cpp` — `serializeProject` / `deserializeProject` methods, `getPatternManager` getter
- `CMakeLists.txt` — `Source/ProjectManager.cpp` added to `VIBESYNTH_DSP_SOURCES`

**P2 SHIPPED (2026-04-23):** File menu wiring + New/Open/Save/Save As basic flow.

- `StandaloneEditor` gains a `std::unique_ptr<ProjectManager> mProjectManager`, constructed after PatternManager is wired to the processor.
- File menu (previously placeholder items 101-107) now live-wired:
  - **New Project...** → prompts for project name, creates the folder, saves an initial empty `project.xml`.  Validation via `ProjectManager::isValidProjectName` — rejects Windows-reserved characters and device names with a retry-on-fail alert.
  - **New from Template...** → disabled; P6 will wire to the default-template system.
  - **Open Project...** → interim native file picker rooted at the projects folder (`canSelectDirectories | canSelectFiles`).  Accepts either the folder itself or the `project.xml` inside.  P3 replaces this with a custom Project Browser window.
  - **Save** → `mProjectManager->saveProject()`.  If no project is open, transparently falls through to Save As.
  - **Save As...** → prompts for name, duplicates the current project folder (or creates a fresh one if nothing's open), switches to it as the current project.
  - **Save as Preset...** disabled (not a v1 feature; projects supplant the old per-track preset model).
  - **Import Audio...** → unchanged (existing Builder audio-import path).
- Title bar live-updates — `refreshWindowTitle()` pushes `BaySickDAW — <ProjectName>` to the top-level `DocumentWindow` after every project lifecycle change.  P5 will add the unsaved-dirty asterisk.
- Async prompt helper `promptForProjectName()` — anonymous-namespace function in StandaloneEditor.cpp.  Returns the entered name already passed through `sanitizeProjectName`; caller checks `isValidProjectName` and re-prompts on failure.  All paths async-safe (audio thread never blocks).
- `promptCreateProject(reasonExplanation)` public method on StandaloneEditor — exposed so P4's Builder audio-drop code can reuse the same prompt when a drop arrives with no project open.

**Known deferrals into later P-batches:**
- P3: Custom Project Browser window replacing the native file picker; right-click Rename / Duplicate / Delete; `File → Open Recent ▸ (10)` submenu.
- P4: `importSample` wired into Builder audio-drop path.
- P5: Dirty tracking + autosave + close-with-unsaved prompt + "reset to defaults" on New (for now, New keeps whatever state was in memory at the moment New was invoked).
- P6: Welcome screen on first launch + default-template designation UI in Settings.

**Files touched in P2:**
- `Source/Standalone/StandaloneEditor.h` — forward-declare `ProjectManager`, add `mProjectManager` member, declare five handlers (`doFileNew` / `doFileOpen` / `doFileSave` / `doFileSaveAs` / `promptCreateProject`) + `refreshWindowTitle`
- `Source/Standalone/StandaloneEditor.cpp` — include `ProjectManager.h`, instantiate in ctor, wire menu items 101/103/104/105, implement all five handlers + the `promptForProjectName` helper

**Verification plan:** build runs, File menu reachable, New Project creates a folder with a valid `project.xml` under `Documents/BaySickDAW/Projects/<name>/`, Open Project loads it back with all state restored, Save overwrites, Save As duplicates to a new folder + switches current project to it.

**P3 SHIPPED (2026-04-23):** Custom Project Browser + Open Recent submenu + Rename / Duplicate / Delete / Show in Explorer.

- New component `ProjectBrowserWindow` (`Source/Standalone/ProjectBrowserWindow.h` + `.cpp`) — juce::TableListBox-backed project list; columns Name / Last Modified / Size, all sortable; row background alternation; right-click context menu with Open / Rename / Duplicate / Delete / Show in Explorer; footer buttons New Project / Open / Cancel.  Double-click a row to open.  Launched from `StandaloneEditor::doFileOpen` via `juce::DialogWindow::LaunchOptions` (async, non-blocking).
- **Rename** disabled when target folder is the currently-open project (guarded via `ProjectBrowserWindow::isCurrentProject` callback -> `ProjectManager::isCurrentProject`); caller is offered a retry alert if the new name is invalid or collides.
- **Duplicate** calls the new static `ProjectManager::duplicateProject(folder, newName)` which collision-appends " (N)" and returns the new folder.  Does NOT switch the current project.
- **Delete** uses `juce::AlertWindow::showOkCancelBox` for confirmation, then `ProjectManager::deleteProject` (moveToTrash = Recycle Bin).
- **Show in Explorer** uses `juce::File::revealToUser` (standard OS reveal path).
- `ProjectManager` gains `listProjects()` which scans the projects root, filters to folders that contain a valid `project.xml`, and returns sorted `{folder, lastModified, sizeBytes, name}` entries.  Size = sum of all files recursively (so Samples\ counts toward the displayed size).
- **File menu** gains a new "Open Recent" submenu between Open Project and Save.  Entries (IDs 130-139) are the last 10 opened project folders; missing folders render as "(missing)" and greyed out; "Clear Recent Projects" (ID 140) at the bottom.  Implementation uses explicit `case 130..139:` rather than a mid-switch `default:` (the switch already has a terminal `default:`).
- Open Project menu (Ctrl+O) now launches the new browser instead of the native file picker.

**Files touched in P3:**
- `Source/ProjectManager.h` + `.cpp` — added `duplicateProject` + `listProjects` + `Listing` struct
- `Source/Standalone/ProjectBrowserWindow.h` + `.cpp` (NEW)
- `Source/Standalone/StandaloneEditor.cpp` — include `ProjectBrowserWindow.h`, replace `doFileOpen` with the new launcher, add "Open Recent" submenu + recent-project IDs 130-139 + "Clear Recent" (140) handlers
- `CMakeLists.txt` — register `Source/Standalone/ProjectBrowserWindow.cpp`

**P4 SHIPPED (2026-04-23):** Copy-on-drop in Builder + Sample Library shortcut + project-folder awareness on the processor.

- **Copy-on-drop:** `ArrangementGrid::importAudioFile` gains a new `onImportSampleRequest(juce::File)` callback.  When wired by StandaloneEditor, audio drops route through `ProjectManager::importSample` which copies the file into `<project>/Samples/` (dedupes by filename + mtime + size, falls back to `" (N)"` suffix on hash-mismatched collision) and returns `"Samples/<filename>"` as the relative string to store on the arrangement block.  Empty return = rejected (no project open); editor shows "No project open" info dialog and the drop is silently discarded.  No callback = legacy absolute-path behavior preserved.
- **Path resolution:** `VibeSynthProcessor` gains `setCurrentProjectFolder` / `getCurrentProjectFolder` (guarded by `mProjectFolderLock` since both message + audio threads read it) + `resolveProjectFile(storedPath)` which returns absolute paths unchanged (legacy compat) and resolves relative paths against the current project folder.  Audio-clip player creation in `rebuildAudioClipPlayers` now uses `resolveProjectFile` so playback works for both fresh relative-path drops and old absolute-path projects.  Thumbnail cache (`ArrangementGrid::getOrCreateThumbnail`) keys by the STORED path (so paint calls from project reload hit the same cache entry) but creates the `FileInputSource` from the resolved absolute file.
- **ProjectManager integration:** `newProject` / `openProject` / `saveProjectAs` now all call `mProcessor.setCurrentProjectFolder(folder)` so path resolution is always live for the current project.  `openProject` sets the folder BEFORE `deserializeProject` runs so any path resolution during state restore has the folder ready.
- **Sample Library shortcut:** new `ProjectManager::runFirstLaunchHousekeeping()`.  Creates `Documents\BaySickDAW\` if missing.  Creates `Documents\BaySickDAW\Sample Library.lnk` pointing at `%LOCALAPPDATA%\BaySickDAW\CoreLibrary\` (creates CoreLibrary preemptively so the shortcut resolves).  Guarded by a `shortcutCreated` bool persisted in `settings.xml` — users who delete the shortcut don't see it come back on next launch.  Called from `StandaloneEditor` ctor.

**P4b SHIPPED (2026-04-23):** Roaming-to-Documents migration for Presets/ + audio_settings.xml.
- `ProjectManager::runFirstLaunchHousekeeping` gains a one-shot migration step (guarded by `mMigratedFromRoaming` bool persisted in settings.xml as `migratedFromRoaming` attr).  On first launch with this build:
  - `%APPDATA%\BaySickDAW\Presets\` -> `Documents\BaySickDAW\Presets\` (`copyDirectoryTo` then `deleteRecursively`).
  - `%APPDATA%\BaySickDAW\audio_settings.xml` -> `Documents\BaySickDAW\audio_settings.xml` (`moveFileTo`).
- All preset-reading sites flipped to `userDocumentsDirectory`:
  - `BaySickBassEditor::presetsDir`
  - `BaySickSynthEditor::presetsDir`
  - `BaySickDrumsEditor::presetsDir`
  - `VibePlayerEditor::presetsDir`
  - `HarmlessEditor::presetsDir`
- `VibesynthStandaloneApp::getAudioSettingsFile` now returns the Documents path.  Reads transparently fall back to the legacy Roaming path when the new file doesn't exist yet (covers the FIRST launch when StandaloneApp loads device state BEFORE the editor + ProjectManager + migration ctor runs).  After migration moves the file, all subsequent runs read/write the Documents path.
- Result: every user-facing artifact now lives under `Documents\BaySickDAW\` exactly as the unified-folder layout prescribed.  The Roaming `%APPDATA%\BaySickDAW\` folder is left empty after migration (could be deleted by a future cleanup pass; harmless to leave).

**Files touched in P4b:**
- `Source/ProjectManager.h` + `.cpp` - `mMigratedFromRoaming` bool + persistence + migration in housekeeping
- `Source/Standalone/StandaloneApp.cpp` - `getAudioSettingsFile` returns Documents with Roaming fallback
- `Source/BaySickBass/BaySickBassEditor.cpp` - `presetsDir` flip
- `Source/BaySickSynth/BaySickSynthEditor.cpp` - same
- `Source/BaySickDrums/BaySickDrumsEditor.cpp` - same
- `Source/VibePlayer/VibePlayerEditor.cpp` - same
- `Source/Harmless/HarmlessEditor.cpp` - same

**Preset Folder Reorg (2026-04-23):** preset XMLs grouped into family subfolders so the in-app picker can render section headers per source synth.

- **On disk:**
  - `Documents\BaySickDAW\Presets\BaySickDrums\` - 7 family subfolders (TR-808 16, TR-909 8, TR-606 2, Simmons 5, Yamaha FM 5, Tuned Percussion 14, Hand Percussion 5).  `BaySick Kit 1.bsd` stays at the root since it's a kit, not a preset.
  - `Documents\BaySickDAW\Presets\BaySickSynth\` - 5 subfolders (Leads 12, Pads 11, Keys & Organs 10, Bells & Metallic 3, Sound FX 8). Sound FX absorbs `Impact Hit` and `Sub Drop FX` that were previously misplaced under BaySickDrums.
  - `Documents\BaySickDAW\Presets\BaySickBass\` - 3 subfolders (Analog Bass 5, FM Bass 2, Sub & Dub 2).
- **Picker code:** `BaySickSynthEditor::showPresetMenu`, `BaySickBassEditor::showPresetMenu`, and `BaySickDrumsEditor::showSoundPicker`'s Synth Patch submenu now enumerate `presetsDir()` subfolders alphabetically + emit each as `addSectionHeader(folderName)` followed by the XMLs inside.  Root-level XMLs render under a "My Presets" section so user-saved presets coexist with the categorized factory ones.  Empty subfolders are skipped.
- **Generator script:** `Tools/gen_factory_presets.py` rewritten to emit into category subfolders.  Three new dicts (`DRUM_CATEGORIES` / `SYNTH_CATEGORIES` / `BASS_CATEGORIES`) map each recipe name -> family folder; new helper `categorized_dir(base, name, map)` resolves the destination at write time.  Recipes lists themselves remain `(name, overrides)` tuples - mapping is name-driven so future recipes only need a category-map entry.  Path root also updated from Roaming `%APPDATA%` to `Documents\BaySickDAW\` (matches the P4b migration).
- **Files touched:**
  - `Source/BaySickSynth/BaySickSynthEditor.cpp` - subfolder enumeration in `showPresetMenu`
  - `Source/BaySickBass/BaySickBassEditor.cpp` - same
  - `Source/BaySickDrums/BaySickDrumsEditor.cpp` - subfolder enumeration in Synth Patch submenu of `showSoundPicker`; flat list of all preset XMLs collected into `presetXmls` so existing index-based result handling (`kPresetBase + i`) works unchanged
  - `Tools/gen_factory_presets.py` - category maps + categorized_dir helper + updated write loops + Documents path + Impact Hit / Sub Drop FX moved from drums to synth recipe list

**Core Library Page-Context Filter (2026-04-23):** the in-app pack browser now filters which top-level packs are visible based on which page invoked it.

- **Drum slot Sample submenu** (BaySickDrumsEditor) -> drum packs only.  Folder enumeration in `showSoundPicker` filters via `SampleLibrary::isDrumPack(name)`; only matching packs become submenus.  Hides Brass / Keys / Strings / Woodwinds / etc. so users can't accidentally pick a piano sample for a kick.
- **VibePlayer File menu Core Library** (Layers / Bass) -> melodic packs only.  Same filter inverted (`! isDrumPack`).  This is also a NEW addition - VibePlayer's File menu previously had only file pickers; now it also has an in-app Core Library submenu that lists melodic packs with recursive sub-pack browsing (folders containing audio = clickable load-folder, otherwise recurse; .sfz = clickable load-sfz; loose audio = clickable load-single).
- **Embedded VibePlayer (drum-slot context)** -> Core Library submenu hidden entirely.  `VibePlayerEditor::setDrumContext(true)` is called from BaySickDrumsEditor when constructing the embedded VP editor.  Avoids showing melodic packs from inside a drum slot, since the outer drum-slot picker already exposes the drum-filtered library.
- **Classification rule** (single source of truth in SampleLibrary): pack name contains "Drums" OR contains "Percussion" (both case-insensitive SUBSTRING matches, not equality) -> drum.  Folders install as "Hip Hop Drums Package", "EDM Drums Package", "Percussion Package" so substring (not equals) is required to catch the " Package" suffix.  Drum-page packs: Hip Hop Drums Package, EDM Drums Package, Percussion Package.  Melodic packs (Layer + Bass): Brass Package, Keys Package, Strings Package, Woodwinds Package.  Future user-added drum packs (e.g. "Trap Drums") auto-categorize correctly.
- **API rename:** `getPercussionPacks` / `getNonPercussionPacks` -> `getDrumPacks` / `getMelodicPacks` (zero existing callers, so the rename is invisible).

**Files touched in Core Library filter:**
- `Source/SampleLibrary.h` + `.cpp` - `isDrumPack` static helper, member rename, classification swap
- `Source/BaySickDrums/BaySickDrumsEditor.cpp` - `showSoundPicker` filters Core Library to drum packs; `setDrumContext(true)` on embedded VP editor
- `Source/VibePlayer/VibePlayerEditor.h` + `.cpp` - `setDrumContext` setter + `mIsDrumContext` flag; `showFileMenu` adds melodic-filtered Core Library submenu (skipped when `mIsDrumContext`)

**Live-Input Recording Plan (R1-R5, 2026-04-23):** scoped after auditing the existing arm button's misalignment with DAW-internal strips.  Jeff locked: new live-input strip class routes from ASIO input channels instead of retrofitting arm onto existing strips.  Architecture + spec:
- Two new bus channels: **VoxBus** (`kVoxBus=7`, `mixer_voxbus`) + **InstBus** (`kInstBus=8`, `mixer_instbus`).  Both carry full pre-EQ / rack / post-EQ / fader / meter like the other buses.
- Mixer page order (display + signal): `Clips > Vox > Inst > Layers > Bass > Drums > FX > Master`.
- New InsertKind `Vox` and `Inst`.  Up to `kMaxVoxStrips` (6) and `kMaxInstStrips` (6) respectively.  Channel IDs at `kVoxBase=600..605` and `kInstBase=700..705`.
- Mixer page header buttons: `"Add Mixer Strip"` renamed to `"Add Aux Strip"`, plus new `"Add Vox Strip"` + `"Add Inst Strip"`.  Button order right-to-left in the PageMenuBar extra-right slot: `[Add Aux] [Add Vox] [Add Inst]`.
- Each Vox/Inst strip will eventually get: Arm button (R2), ASIO input channel picker (R2), Listen toggle with headphones icon (R4), and recording routing (R5).
- Existing `_arm` APVTS params on Layer/Bass/Drum/Audio/Aux strips: stay registered (backward compat, zero-cost orphan params); UI buttons get removed in R5.
- Record button gets split-button mode picker (R5): **MIDI** default (records external MIDI into the active piano roll) vs **ASIO** (records armed Vox/Inst strips to WAV files).
- Recording destination: `<project>/Samples/<ProjectName>-<StripName>-<Timestamp>.wav`.  Also creates Builder audio clip on stop.
- No-project-open recording triggers New Project prompt (same flow as copy-on-drop).
- Channel assignment remembers both name AND index; re-resolves by name first when device reconnects.

**R1 SHIPPED (2026-04-23) — partial:** bus infrastructure + insert-registration APIs + mixer-page buttons.
- `MixerChannelIds`: added `kVoxBus=7`, `kInstBus=8`, `kVoxBase=600`, `kInstBase=700`, `kMaxVoxStrips=6`, `kMaxInstStrips=6`, plus `voxInsert(idx)` / `instInsert(idx)` helpers and full updates to `prefixFromChannelId` / `isBus` / `defaultSendTo`.
- `VibeGraph::InsertKind` gains `Vox` + `Inst`.  New storage maps `mVoxInserts` / `mInstInserts`.  Bus-node pointers `mVoxBusNode` / `mInstBusNode` declared (not yet constructed — R3 wires them when the audio-routing side ships).  `selectInsertMap` + `getInsertPeakDb` switches updated.
- `VibeSynthProcessor::ensureVoxInsert(idx, name)` + `ensureInstInsert(idx, name)` public APIs — register APVTS params + InsertNode, default route to their respective bus.
- `MixerTrackStrip::StripType` gains `Vox` + `Inst`.  Width matches the other 64px strip types.
- `MixerPage`: `addVoxChannel` / `addInstChannel` creators, new button members (`mAddVoxBtn` / `mAddInstBtn`), new strip maps, new `getAddVoxBtn` / `getAddInstBtn` accessors.  Existing `"Add Mixer Strip"` button text renamed to `"Add Aux Strip"`.  Layout code in `layoutScrollContent` places Vox/Inst strips in the bucket pass (currently grouped with Aux for layout purposes; proper Vox/Inst bus-group visual separation lands in R3).
- `StandaloneEditor::setVisiblePage` reparents all 3 Mixer buttons into the PageMenuBar's extra-right slot when the Mixer page becomes active.
- EQ-sync tables updated to include Vox/Inst so their EQ params sync through the existing dirty-flag pass.

**R1 known incompleteness (fills in later batches):**
- Vox/Inst bus nodes not constructed in `buildFixedTopology` — R3 will add them when audio routing goes live.
- No dedicated Vox/Inst bus **strips** on the Mixer page — strips aggregate into the Aux group for layout; proper bus-separator + bus-strip rendering lands with the bus-node work.
- Arm button + channel picker: R2.
- Audio input routing from ASIO: R3.
- Listen toggle: R4.
- Recording engine + transport split button + hide orphan arms: R5.

**R1 follow-up fixes (2026-04-23, same day):** during initial test the Vox/Inst Add buttons created strips but they were invisible + cable routing was broken.  Three sequential fixes landed:
- **Strips invisible.** `MixerPage::layoutScrollContent` only iterated buckets keyed by Master / FxBus / Aux* / Clips / Layers / Bass / Drums.  Strips bucketed under `kVoxBus` / `kInstBus` were positioned nowhere.  Added explicit iteration over both buckets between Clips Bus and Layers Bus, rendered as floating groups (no bus strip yet — that lands with R3).  Colors: Vox `#0FAFA5` (teal), Inst `#1C3A8A` (navy).
- **Cable anchor.** `MixerPage::findStripByChannelId` didn't recognize Vox/Inst channel IDs (600..705), so `getSocketPosition` returned `(-1,-1)` and the cable's start point was off-screen.  Added Vox/Inst lookup branches.  Same fix to `CableOverlay::findSocketNear` and `findStripUnder`.
- **Cable doesn't attach to aux destination.** `VibeGraph::rebuildActiveChannels` (the source list passed to `RoutingGraph::rebuildFromApvts`) didn't include Vox/Inst InsertNodes.  When user clicked the aux to commit a send, the `_sendN_to` APVTS write succeeded but the routing-graph rebuild never read it back — so the edge never entered `edges()` and `paint()` had nothing to draw.  Added Vox/Inst iteration + bumped the reserve count.
- **Routing-rule scope correction.** Initial `isRouteAllowed` for Vox/Inst over-permitted (allowed routing to Layers/Bass/Drums/Clips).  Jeff scoped down to Master + Clips + their own bus only.  Audio insert main-out gained Vox + Inst as new allowed destinations.  Final rule table:
  | Source | Main-out destinations |
  |---|---|
  | Layer | Master, Layers Bus, Bass Bus |
  | Bass | Master, Bass Bus, Layers Bus |
  | Drum | Master, Drums Bus |
  | Audio | Master, Layers Bus, Bass Bus, Drums Bus, Clips Bus, Vox Bus, Inst Bus |
  | Aux | Master, FX Bus, other Aux |
  | Vox | Master, Clips Bus, Vox Bus |
  | Inst | Master, Clips Bus, Inst Bus |
  Sends (separate from main-out) still only land on Aux strips for every source — universal rule unchanged.

**R2 IN PROGRESS (2026-04-23):** Vox/Inst Arm LED + ASIO input picker + APVTS persistence.
- `MixerTrackStrip::hasArm()` flipped: was `LayerChannel || BassChannel || DrumChannel`, now `Vox || Inst`.  Layer/Bass/Drum's `_arm` APVTS param stays registered for backward compat (until R5 hides the LED scrub-pass), but their LED no longer renders.
- New `MixerTrackStrip::onArmRequested` callback + `setInputChannelLabel` for hover tooltip.
- For Vox/Inst strips, `setApvts` skips installing the standard `ButtonAttachment` on the Arm LED (which would auto-toggle `_arm`) and instead installs a click handler firing `onArmRequested(channelId)`.  MixerPage handler enumerates ASIO inputs from the AudioDeviceManager + writes the selected index to APVTS.  `_arm` LED visual state stays driven by APVTS so it stays in sync regardless of how `_arm` was set.
- **APVTS `_inputChannelIdx`** registered lazily on Vox / Inst inserts via new `VibeSynthProcessor::addLiveInputParams(prefix)` called inside `ensureVoxInsert` / `ensureInstInsert`.  Range -1..127, default -1 (no input).
- **Channel NAME** stored as a non-APVTS attribute on `apvts.state` (since APVTS only handles numeric ranged params).  New helpers `setInputChannelName` / `getInputChannelName` on the processor; thread-safe via `mInputChannelNamesLock`.  Round-trips with the project state because `apvts.state.setProperty` writes onto the same ValueTree that `apvts.copyState()` returns.
- **MixerPage::showInputChannelPicker(channelId)** — shared popup-menu handler.  Reads channel names via `getInputChannelNames` callback (wired by StandaloneEditor to `mDeviceManager.getCurrentAudioDevice()->getInputChannelNames()`).  Shows "No channels available" disabled item when device has zero inputs.  Shows tick on the currently-selected channel (if armed).  Adds "Disarm" entry when `_arm` is currently true.  Selection writes `_inputChannelIdx` + `_inputChannelName` + `_arm` to APVTS.
- **MixerPage::refreshLiveInputStrip(channelId)** — updates the strip's Arm-LED tooltip via `MixerTrackStrip::setInputChannelLabel` after the picker writes new state.  Will also be called from R3 on project load.
- **Reconnect-by-name** deferred to R3 (where audio actually starts flowing through the strip).  Stored name + index round-trip with the project today via the APVTS state so the data is ready for R3 to consume.

**R3 SHIPPED (2026-04-23):** Audio device input wiring through Vox / Inst strips into their buses.

- **Audio device init bumped 0 -> 16 input channels** in `StandaloneApp::initialise` (both the saved-settings path and the default-init path).  16 covers most desktop interfaces; JUCE clamps to actual device max so larger / smaller devices both work.
- **Processor input bus declared:** `BusesProperties().withInput("Input", AudioChannelSet::discreteChannels(16), true)`.  `isBusesLayoutSupported` accepts any input layout so VST / standalone hosts can negotiate down.
- **Input snapshot before buffer.clear():** new `mLiveInputSnapshot` AudioBuffer, sized in `prepareToPlay` (`mLiveInputSlotBuf` for the per-strip stereo scratch; `mLiveInputSnapshot` for the full input snapshot).  `processBlock` copies `getTotalNumInputChannels()` channels into the snapshot before the buffer-clear, so input data isn't lost.
- **Vox / Inst processing pass** added in `processBlock` after the aux-insert pass.  Iterates each Vox / Inst InsertNode, reads its `_arm` + `_inputChannelIdx` from APVTS, mono->stereo duplicates the chosen input channel into `mLiveInputSlotBuf`, runs `processInsert` (preEQ -> rack -> postEQ -> fader / mute / solo / meter), and `routeInsertOutput` fans to the strip's bus accumulator (kVoxBus / kInstBus by default).
- **Bus accumulator drain to Master:** after all Vox / Inst inserts processed, `routeInsertOutput(kVoxBus, *voxAccum, n)` and the same for kInstBus pumps the bus accumulators through the routing graph - default edge `kVoxBus -> kMaster` (and `kInstBus -> kMaster`) sums them into the master accumulator that VibeGraph::processBlock then runs through the master rack to output.
- **VoxBus / InstBus nodes constructed** in `VibeGraph::buildFixedTopology` as `InstrChannelNode` instances (same shape as Audio Clips Bus - rack + pre/post EQ + fader + meter).  Their DSP isn't engaged in R3 (audio passes through accumulator -> master without touching the bus rack/EQ); R3.5 will wire the bus DSP for group-level control over Vox + Inst content.
- **Insert prepare loop** updated to include `mVoxInserts` + `mInstInserts` so newly-created strips get `prepare()` called on their internal DSP at the right sample rate / block size.

**R3 known limitations (R3.5 / R5 follow-ups):**
- ~~Bus rack/EQ/fader DSP not yet applied on VoxBus/InstBus accumulators~~ -- DONE in R3.5.
- ~~No visible Vox/Inst BUS strips on the Mixer page yet~~ -- DONE in R3.5.
- Reconnect-by-name still uses index only - if the device's channel order changes, the strip pulls from a different physical input.  R5 to add the name-resolve fallback.
- Recording itself (writing WAVs, creating Builder clips) lands in R5 along with the transport split-button.

**R3.5 (2026-04-23):** Vox + Inst BUS strips now visible on Mixer page (Teal `0xFF0FAFA5` + Navy `0xFF1C3A8A`) alongside the Clips Bus.  Each bus has full DSP applied to its accumulator: pre-EQ -> rack -> post-EQ -> polarity/M-S width -> fader x mute x peak meter, then `routeInsertOutput` to the destination (Master by default).  New APVTS prefixes `mixer_voxbus` / `mixer_instbus` registered as `MixerStripKind::Bus`; new accessors `getVox/InstBusRack/EQ/PreEQ` + `applyVox/InstBusPolarityWidth` mirror the Audio Clips Bus pattern.  New atomics `mVoxBusPeakDb` / `mInstBusPeakDb` drive the strip meters.  `mActiveChannels` extended so the routing graph picks up the new bus channels.

**Cable overlay flicker fix (2026-04-23):** the `MixerPage::CableOverlay` now sets `setBufferedToImage(true)` so sibling strip-meter repaints don't force the bezier render to re-rasterize 30x/sec.  Timer-driven `repaint()` only fires when the viewport scrolled; layout / route changes invalidate the cache explicitly.  Drag / flash / send-placement modes self-trigger via mouse handlers as before.

**Mixer page review backlog (deferred):** (a) cables visually pass over the Master strip body when the FX Bus scrolls close to it (overlay is on top by design; fix candidates are clip-out-Master, dedicated patcher lane, or steeper sag); (b) on fast horizontal scroll the buffered overlay can briefly look "unconnected" until the next 30 Hz tick invalidates it.

**R4 (2026-04-23):** Listen toggle on every Vox / Inst strip.  New `_listen` Bool param (default false) lazy-registered alongside `_inputChannelIdx` in `addLiveInputParams`.  New `HeadphonesLedButton` in `SharedUI.h` paints a vector headphones glyph (path-based, no font/PNG dependency so it can't fall back to a box character).  Strip layout switched to a 3-column utility row (Arm | Listen | Bypass) on Vox / Inst types only; other strip types still use the 2-column layout.  In `processBlock`, the `routeInsertOutput` call after each Vox / Inst `processInsert` is now gated on the strip's `_listen` param: when off, the InsertNode still runs (peak meter still updates so the user can see signal level) but the audio is silenced before it reaches the bus.  This is the safe default for beginners arming a mic with desk speakers - no surprise feedback - and matches FL Studio / Logic monitoring semantics.

**R5a (2026-04-24):** Play + Record transport buttons repainted.  New `PlayButton` + `RecordButton` TextButton subclasses in `GlobalTransportBar.cpp`.  Inactive state routes through `VibeLAF::drawButtonBackground` so the base body matches Pause / Stop / Metro / Tap byte-for-byte (same color, shape, corner radius, hover treatment).  Active state paints a tinted raised-slab backdrop (green for Play-when-playing, red for Record-when-armed) mirroring the LAF's LRX-12 action-button shape.  White play-triangle / white dot glyphs paint on top in both states so the glyph is always visible.  RecordButton has a right-edge chevron zone (14 px) that fires a separate `onArrowClicked` callback for the mode dropdown.  Button type in `GlobalTransportBar.h` widened from `std::unique_ptr<juce::TextButton>` to `std::unique_ptr<juce::Button>` for the two buttons that are now custom subclasses.

**R5b (2026-04-24):** Record button now arms-only - no more auto-start-playback on Record click.  `StandaloneEditor::mRecordArmed` + `mRecordingActive` flags split the state: Record toggles `mRecordArmed`; `onPlay` checks it + starts the capture engine before `startPlayback`.  Pause + Stop both commit any active recording (per Jeff's explicit spec: "Pause ALWAYS stops recording") and disarm Record so the next Play is plain playback unless re-armed.  New `GlobalTransportBar::setRecordArmed(bool)` lets the editor flip the button's red body without firing a click.

**R5c (2026-04-24):** ASIO / MIDI mode picker on the Record chevron.  New `GlobalTransportBar::RecordMode { Audio, Midi }` enum + `getRecordMode` / `setRecordMode` + `onRecordModeChanged` callback.  Chevron click opens a PopupMenu with two items: "ASIO" + "MIDI (piano roll tabs only)".  Tooltip on the Record button updates to reflect the active mode.  Drives behavior in R5d.

**R5d (2026-04-24):** Recording engine rewrite.  `VibeSynthProcessor::startRecording` now takes a mode + project-name + samples-folder + start-beat; `stopRecording` returns a `RecordResult { masterFile, stripFiles[{chId, file}], midiNotes, startBeat }`.  AudioFileRecorder split into `mMasterRecorder` (master-output fallback) + `mStripRecorders` (per-armed-Vox/Inst-strip mono WAVs capturing RAW pre-rack input).

  - **ASIO mode**, Vox / Inst strips armed -> each gets its own WAV at `{ProjectFolder}/Samples/{ProjectName} - {StripName} - {YYYY-MM-DD HH-MM-SS}.wav`; no strips armed -> master output captured instead with strip name "Master".  Files are written mono for strips (one input channel each) and stereo for the master.
  - **Project-required guard**: attempting Record + Play with no open project now pops the `File > New Project` dialog directly (via `doFileNew()`) instead of a dead-end "you need to save" notice - beginner-friendly, no intermediate step before creating a project.  Disarms Record so the user finishes naming before trying again.
  - **Auto-drop on arrangement**: new `StandaloneEditor::commitRecordingResult` runs on Pause / Stop / disarm.  Each captured WAV gets dropped as a `ClipType::Audio` `ArrangementBlock` on the next free track row (scans `mPM->getBlock(i).trackRow + 1`) at `startBar = floor(startBeat / 4)`, length in bars derived from the recorded file's duration + current BPM.  `rebuildAudioClipPlayers()` fires so clips are playable immediately; project is marked dirty.
  - **MIDI mode**: last-accessed piano roll tracking via new `StandaloneEditor::mLastRollKind` (`None` / `Layer` / `Bass` / `Drums`) + `mLastRollIndex`.  Updated inside `onTabSelected` whenever the user switches to a LayersPage / BassPage / DrumsPage tab.  `onPlay` guards MIDI-mode records with `mLastRollKind == None` -> AlertWindow "No Active Piano Roll Selected"; otherwise `commitRecordingResult` appends captured notes to the matching roll in `mPM->currentPattern()` (`layerRoll[idx]` / `bassRoll[idx]` / `drumRoll`).
  - **Legacy Recordings folder deprecated**: recordings no longer land at `Documents/BaySickDAW/Recordings/`.  That folder stays on disk for back-compat but nothing new writes to it.  All R5d captures live inside the project bundle.
  - **`_arm` param preserved but hidden on non-live strips**: MixerTrackStrip's `hasArm()` continues to return true only for Vox / Inst types; Layer / Bass / Drum / Audio strips keep their `_arm` APVTS param for back-compat (saved projects) but the LED is hidden.

**R3 verifiable on Jeff's studio machine** (Tascam connected): arm a Vox / Inst strip, pick an input channel, sing / play - signal should flow through the strip's signal chain to the master output.  The strip's own fader / mute / EQ / rack work.  Bus-level grouping and recording are next.

**ASIO Build Support (2026-04-23):** restored after discovering the build had been compiling without `JUCE_ASIO=1`, leaving the audio device dialog showing only Windows Audio (WASAPI / DirectSound) instead of the ASIO type expected by the plan + audio-interface workflow.

- `CMakeLists.txt` adds a conditional ASIO block: looks for the Steinberg SDK at `libs/asiosdk/common/iasiodrv.h`; when present, defines `JUCE_ASIO=1` and adds the include path; when absent, prints a status message and gracefully falls back to WASAPI.  Build is portable - users without the SDK still get a working DAW, just without ASIO.
- The Steinberg ASIO SDK is license-restricted (free download from Steinberg developer portal but non-redistributable), so it's not committed to the repo.  Jeff installs once locally + rebuilds.

**Required setup (one-time, per machine):**
1. Download `asiosdk_*.zip` from https://www.steinberg.net/developers/
2. Extract its `common/` subfolder so the layout becomes `libs/asiosdk/common/iasiodrv.h` (plus the other ASIO headers in that folder)
3. Re-run `do_build.bat` - CMake auto-detects + enables ASIO
4. CMake status line confirms: "ASIO SDK found at ... -> ASIO support ENABLED"

**Audio recording input wiring** stays in POST-P4 #2 territory: even with ASIO enabled, the current `StandaloneApp::initialise` requests `0, 2` channels (no input) and `AudioSettingsDialog::applySettings` hardcodes `audioInputDeviceName=""`.  Both need lifting to expose ASIO inputs for the arm-then-record flow.

**Files touched in P4:**
- `Source/PluginProcessor.h` + `.cpp` — `setCurrentProjectFolder` / `getCurrentProjectFolder` / `resolveProjectFile` + `mCurrentProjectFolder` member + lock; `rebuildAudioClipPlayers` now routes through resolver
- `Source/ProjectManager.h` + `.cpp` — `runFirstLaunchHousekeeping` + `mShortcutCreated` bool persisted in settings.xml; `newProject`/`openProject`/`saveProjectAs` call `setCurrentProjectFolder`
- `Source/Standalone/BuilderPage.h` — new `onImportSampleRequest` + `onResolveStoredPath` + `onDropWithoutProject` callbacks on `ArrangementGrid`
- `Source/Standalone/BuilderPage.cpp` — `importAudioFile` routes through callback + uses stored path for thumbnail cache key; `getOrCreateThumbnail` resolves stored -> absolute via resolver callback
- `Source/Standalone/StandaloneEditor.cpp` — wires `grid->onImportSampleRequest` to `mProjectManager->importSample` + `onResolveStoredPath` to `mProcessor.resolveProjectFile` + `onDropWithoutProject` -> async New Project prompt + retry; calls `runFirstLaunchHousekeeping` at editor construction

**P5 SHIPPED (2026-04-23):** Dirty tracking + autosave + close-with-unsaved-changes prompt.

- **Dirty state:** `ProjectManager` gains `mDirty` atomic + `markDirty()` / `clearDirty()` / `isDirty()` / `onDirtyChanged` callback.  `mIgnoreDirty` transitional bool set during `openProject` so the deserialize replay doesn't self-mark the project dirty.  `saveProject` / `newProject` / `openProject` / `saveProjectAs` all `clearDirty` on success.
- **Dirty sources wired:**
  - **APVTS changes** — `VibeSynthProcessor::valueTreePropertyChanged` (already subscribed for EQ sync) now also calls `onAnyStateChange`, a new `std::function<void()>` wired by StandaloneEditor to `mProjectManager->markDirty`.  Covers engine knobs, mixer faders, EQ bands, every APVTS-backed control.
  - **Undoable edits** — `StandaloneEditor::doUndoAction` + `globalUndo` + `globalRedo` call `markDirty`.  Covers piano-roll note edits, arrangement block moves/resizes/deletes, pattern metadata edits — anything that routes through the undo manager.
- **Title-bar asterisk:** `refreshWindowTitle` appends ` *` when `isDirty()`.  Wired to fire via `onDirtyChanged` so the transition is instant.
- **Autosave:** `ProjectManager` now inherits from `juce::Timer`.  Default 15-minute interval (per the lock-in), tunable via `setAutosaveIntervalSeconds`.  `timerCallback` writes a backup XML every tick **regardless of dirty state**.  Two locations:
  - **Project open:** `<project>/Backups/<ProjectName>_backup_YYYY-MM-DD_HH-MM.xml`.  Each tick adds a new file; rolling retention keeps the newest 10 (oldest deletes on the 11th write).  Same-minute name collisions get `" (N)"` suffix.
  - **No project open:** `Documents\BaySickDAW\Backups\Unsaved\Untitled_backup_YYYY-MM-DD_HH-MM.xml`.  Same rolling retention.  Allows recovery from a crash during an unsaved tinkering session.
  Each backup is the same `<BaySickDAWProject>` XML the main `project.xml` uses, so it's directly openable / restorable.  Audio paths inside backups remain relative to the parent project folder (when the project exists) or absolute (legacy unsaved-session imports).
  Autosave DOES NOT clear the dirty flag - backups are a safety net; the user's real Save is still pending.
- **Restore from Backup:** new `File -> Restore from Backup...` menu item shows a popup of the current project's (or unsaved-session's) backups, newest first, formatted as `"YYYY-MM-DD HH:MM (X min/hr/days ago)"`.  Selecting one shows a confirmation with a warning about missing-content edge cases ("any audio samples or other content that was deleted from the original project folder since this backup was made will be missing from the restored project"), then `ProjectManager::restoreBackup` copies the backup over `project.xml` (preserving the live one as `project.xml.before-restore`) and reloads in-memory.
- **Close-with-unsaved-changes prompt:**
  - New `StandaloneEditor::confirmDiscardChanges(continuation)` shows Save / Don't Save / Cancel if dirty; calls continuation on Save (after successful save) or Don't Save; aborts on Cancel.  When not dirty, calls continuation immediately.
  - Wired at all 4 risk points: **File -> New Project**, **File -> Open Project**, **File -> Open Recent -> (any)**, **window close button** (via `StandaloneEditor::requestAppQuit` -> VibeSynthWindow::closeButtonPressed override).
- **Compile quirk:** `promptForProjectName` lives in an anonymous namespace further down in StandaloneEditor.cpp but is now called from `createBuilderPage` and `onDropWithoutProject` (earlier in source order).  Solved by adding a forward declaration at the top of the TU; default arg lives on the forward decl only (C++ rule: default args appear in exactly one place per parameter).

**Files touched in P5:**
- `Source/ProjectManager.h` + `.cpp` — dirty flag API + autosave timer + `onDirtyChanged` + `mIgnoreDirty` during load; inherits `juce::Timer`; `~ProjectManager()` stops timer
- `Source/PluginProcessor.h` — `onAnyStateChange` callback + fires inside `valueTreePropertyChanged`
- `Source/Standalone/StandaloneEditor.h` — `confirmDiscardChanges` + `requestAppQuit` declarations
- `Source/Standalone/StandaloneEditor.cpp` — wires dirty-source callbacks + title-bar asterisk + routes New/Open/OpenRecent through `confirmDiscardChanges` + implements `requestAppQuit` + marks dirty in `doUndoAction` / `globalUndo` / `globalRedo`
- `Source/Standalone/StandaloneApp.cpp` — `VibeSynthWindow::closeButtonPressed` override routes through `editor->requestAppQuit()` first

**Known gaps (flagged for future polish, not blockers):**
- **Backup-on-launch detection.**  If `project.xml.bak` is newer than `project.xml` at open time, we could offer to restore.  Not implemented in P5.
- **Non-undoable edits bypass dirty.**  Pattern renames, clip alias changes, a few other direct-mutation paths don't go through the undo manager and won't mark dirty unless they also touch APVTS.  Gaps likely small; find-and-patch as we notice.
- **Audio-device settings don't dirty** (per the spec lock) - correct behavior, not a gap.

**Verified dirty-tracked (no gap):** rack state changes.  `EffectsPage::onMoveRequested` routes slot swaps through `mUndoCtx.perform` (undo manager), and every other rack mutation (`loadEffect`, `clearSlot`, slot bypass, output, rack bypass, knob tweaks) goes through either the undo manager or APVTS - both of which mark the project dirty in P5.

**P6 SHIPPED (2026-04-23):** Default-template support + File menu wiring.

- **`ProjectManager` template API:**
  - `mDefaultTemplate` (juce::File) persisted in `settings.xml` as `defaultTemplate` attribute (full path).
  - `getDefaultTemplate()` / `setDefaultTemplate(folder)` / `clearDefaultTemplate()`.
  - `setDefaultTemplate` calls `saveSettings()` so the choice survives across runs.
  - `newProject(name, templatePath)` already accepted a template arg from P1; P6 wires it to `mDefaultTemplate` by default in `doFileNew`.
- **File menu (P6 additions):**
  - `New Project...` (existing) now seeds from the default template if set.  After folder is created, the seed XML is also deserialized into in-memory state so the user actually sees the template content (otherwise the seed file sits on disk but the live state is whatever was loaded before).
  - `New from Template...` (was disabled, now enabled) - launches the Project Browser; on selection prompts for the new project's name, then creates + seeds + loads.  Independent of the persistent default.
- **Options menu (P6 additions, moved 2026-04-23 per Jeff request from File menu):**
  - `Options > General > Set Default Template...` (ID 530) - launches the Project Browser; selection becomes the persistent default.  Menu label shows the current default's folder name when set: "Set Default Template... (current: <name>)".
  - `Options > General > Clear Default Template` (ID 531) - greyed when no default is set; clears `mDefaultTemplate` + persists.
  - `Options > General` is a submenu under the Options menu; replaces the prior placeholder `General Settings...` item (ID 501).  Future general-settings entries land here too.
- **Welcome screen DEFERRED:** the original P6 spec called for a startup welcome screen.  Per Jeff's lock at the project-persistence audit ("Empty default, but we will be adding a place for them to set the default template"), the startup-welcome UI is dropped from v1.  The default-template feature ships in its place - users designate a template once, then File > New Project always seeds from it.

**Files touched in P6:**
- `Source/ProjectManager.h` + `.cpp` - default-template field + accessors + settings.xml round-trip
- `Source/Standalone/StandaloneEditor.h` - `doFileNewFromTemplate` + `doFileSetDefaultTemplate` declarations
- `Source/Standalone/StandaloneEditor.cpp` - wires File menu items 102 / 110 / 111 + handlers; `doFileNew` passes default template + deserializes seed XML

**Autosave restored to 15-min default.**  The 2-min testing override that lived briefly in `mAutosaveSec` is gone; production interval = 900s.

---

### Audio recording findings (flagged 2026-04-23, to address POST-P4)

Discovered while Jeff was reviewing a recording from the test session.  Current state of the audio-recording subsystem:

- `VibeSynthProcessor::processBlock` at line 1208 calls `mAudioRecorder.writeBlock(buffer)` where `buffer` is the final master-mix output (layers + bass + drums + audio clips, post-FX, post-master-fader).  Press record -> captures exactly that: the full mix.
- The `_arm` APVTS param is registered on every mixer strip (PluginProcessor.cpp:2135) and the mixer-UI button toggles it, but **no code reads the arm state for audio recording**.  Grep confirms `mAudioRecorder.writeBlock` is unconditional and `MidiRecorder` has no arm-awareness either.  The arm buttons are effectively dead UI for recording today.
- Jeff flagged "occasional pops in the recording that aren't from any of the instruments playing."  Root cause TBD — suspects: voice-steal clicks, engine/slot swap DC jumps, buffer underruns from DSP load spikes, denormal runaway in filter state.  Needs a reproducible trigger to pin down.

**Three POST-P4 follow-ups (landing as one grouped batch after P4 completes):**

1. **Document + expose current "record = master mix" behavior.**  Short recording-info tooltip or modal explaining "pressing Record captures the full mix" so users don't expect arm-based isolation until the feature ships.  Zero DSP cost.
2. **Wire per-track arm -> selective recording.**  When any strip is armed, `mAudioRecorder` taps that strip's post-insert buffer instead of the master; multiple armed strips sum together; none armed = current master-mix behavior.  Needs a per-strip peak-point accessor in VibeGraph (already exposed for metering — reuse).
3. **Debug occasional pops.**  Reproduction step needed first.  Instrument `mAudioRecorder::writeBlock` with a ring-buffer of the last few blocks + a "dump on pop detection" mode (threshold + abs-diff between consecutive samples).  Start with the most common suspects listed above.

**Files touched in Phase A (for reference):**
- `Source/BaySickDrums/BaySickDrumsProcessor.h` + `.cpp` — full slot ownership refactor + all per-slot fields
- `Source/BaySickDrums/BaySickDrumsEditor.h` + `.cpp` — polymorphic editor + context menu + drag-drop
- `Source/BaySickSynth/BaySickSynthEditor.cpp` — trackId-substitution loadPreset
- `Source/BaySickBass/BaySickBassEditor.cpp` — trackId-substitution loadPreset
- `Source/VibePlayer/VibePlayerEditor.cpp` — trackId-substitution loadPreset + 3 componentID adds
- `Source/PluginProcessor.cpp` + `.h` — Layers/Bass EQ prefix fix + T12 pattern playback in 2 sites + kDPRTop 49→51 + comment cleanups
- `Source/Standalone/StandaloneEditor.cpp` — Mx/Pg automation display naming + removed transport auto-switch (4 sites)
- `Tools/gen_factory_presets.py` (NEW) — reusable recipe-to-XML generator
- `%APPDATA%/BaySickDAW/Presets/BaySickDrums/*.xml` — 56 factory drum presets
- `%APPDATA%/BaySickDAW/Presets/BaySickSynth/*.xml` — 42 factory synth presets
- `%APPDATA%/BaySickDAW/Presets/BaySickBass/*.xml` — 9 factory bass presets

---

## System Pages

### Piano Roll (shared component)
Used across Layers / Bass / Drums / Event Editor. Items below naturally land during §P2 VibePlayer / BaySickDrums review since the most impactful changes are DrumsPage-driven.

**Shipped:**
- **Press-and-hold audition (2026-04-25)** — both the on-screen piano keyboard and the grid (Draw + Move tools) now hold the audition note for as long as the trigger is held, instead of the previous fixed 1-block (~10 ms) one-shot.  Implementation: each engine processor (BaySickSynth / BaySickBass / Harmless / VibePlayer) gained `auditionNoteOn(int)` + `auditionNoteOff(int)` + paired atomics; `processBlock` consumes them as noteOn-only / noteOff-only events at sample 0 (existing one-shot `auditionNote` path kept untouched for backward compat).  PianoRoll keyboard's `onNotePreview` now routes press → `onNoteAuditionOn` and release → `onNoteAuditionOff` (one-shot `onNoteAudition` no longer fires from the keyboard).  PianoRollGrid added `mAuditionHeldNote` + `triggerAudition` / `releaseAudition` helpers wired into mouseDown (Draw new + Draw-Move + Select-Move), mouseDrag pitch transitions (releases prior, starts new), and mouseUp.  PianoRollContainer forwards the new callbacks; DrumPage / BassPage / LayersPage subscribe and dispatch to the active engine via the same engine cascade as the existing one-shot path.  The redundant one-shot fire at note-create commit (PianoRoll.cpp:1370) was removed since the held-audition's noteOff at mouseUp already releases the voice.  Closes the audition-vs-playback character mismatch that exposed envelope tail behavior only at long durations (Phase D 2026-04-25 wide+woofy bug — confirmed not engine fault, just one-shot was hiding the real preset character).  PRESET-SAFE.

**Tier 1 — to do at review time:**
- **Ghost notes bug fix.** Feature existed and got broken somewhere along the way. Must work across ALL PianoRoll instances — Layer, Bass, DrumsPage drum-grid, DrumsPage full-roll (once dual mode lands), Builder arrangement patterns. PRESET-SAFE.
- **Dual piano roll mode for DrumsPage** — see §P2 entry for full spec. PRESET-BREAK ⚠️ — ship in v1.

### Mixer Page
**Future enhancements:** TBD (survey pending — still future-only for v1 individually)

### Builder Page
**Future enhancements:** TBD (survey pending — still future-only for v1 individually)

### Transport Bar (GlobalTransportBar)
**Current state (v1):** Combined bar at the top of the standalone window. Houses transport buttons (Play/Pause/Stop/Rec), BPM field + Tap, Song/Pattern + loop mode toggles, Metronome dropdown, and the perf cell on the far right.

**Perf cell layout (2x2 grid, shipped 2026-04-19 with §12 EQ8 12f):** Single right-justified `juce::Label` rendering 2 text rows in 9 pt monospaced. Top row `SYS X%  DSP X%`, bottom row `MEM X  LAT X`. Unit suffixes (MB on MEM, samples on LAT) live in the tooltip only so both row widths stay balanced - first round had `MEM XMB / LAT Xsp/Y.Yms` and the bottom row overflowed the 160 px cell width and clipped the M of MEM. Width currently 160 px, same vertical footprint as the rest of the bar (label centres vertically and uses both text rows). LAT polls `mProcessor.getLatencySamples()` via `onGetLatencySamples` callback wired in StandaloneEditor; sums every effect that overrides `getLatencySamples()` (EQ8 anti-cramping today, plus the §5 Limiter / §6 Overdrive / §9 Saturation / §10 Tape / §11 Transient Shaper oversampled or look-ahead modules already shipped). `onGetSampleRate` callback also wired but currently unused (see Tier 3).

**Tier 3 (post-v1.0):**
- **TB-T1 LAT readout in ms.** Restore the millisecond conversion that was in the first 12f follow-on and dropped to fix the bottom-row overflow. `onGetSampleRate` is already wired and returns the live sample rate; format candidates: `LAT 16/0.3` (samples / ms slash) requires layout rework so the bottom row no longer overflows, OR move ms into the tooltip only (cheap), OR add a 5th cell. Useful when the user is wondering whether their reported PDC actually translates to audible delay (samples don't communicate that without mental math). PRESET-SAFE pure UI tweak.

### Effects Page
**Current state (v1):** 6 horizontal effect slots shown side-by-side. Each slot renders its own full `EditorPanelBase` subclass (ChorusPanel / CompressorPanel / etc.). All 6 panels paint + run timers + run meters simultaneously. Slot chrome: effect-type dropdown + bypass LED + add/remove buttons. Slot reordering via ribbon tabs.

#### Sub-tab default + per-channel persistence (2026-04-26)

Two related Effects-page bugs fixed in one batch:

1. **Default sub-tab "says Rack but shows EQ" on first open.** Constructor's `switchTab(0)` resolved index 0 against the dynamic 2/3-tab layout — for non-player (3-tab) channels, index 0 = PreEQ, not Rack. The PageMenuBar tab indicator said "Rack" while the visible content was Pre EQ8 M/S. **Fix:** constructor calls `switchTab(TabKind::Rack)` directly so the kind drives the visible-index, regardless of layout.

2. **Per-channel sub-tab persistence.** User wants returning to a channel to restore its last-used sub-tab (e.g. Layers Bus → EQ stays on EQ when navigated away and back). Implemented via `std::map<int, TabKind> mLastTabPerChannel` keyed by dropdown channel id (1–6 buses, 100+ inserts, 200+ layers, 300+ basses, 400+ audio, 600+ aux). `switchTab(TabKind)` writes the current value (single source of truth — every tab change funnels through here). `onChannelChanged` reads the saved value and clamps `PreEQ → Rack` when arriving on a player channel that has no Pre tab. `setupEffectsTabs` lambda (StandaloneEditor) trusts `ep->getActiveTab()` — was hardcoded `TabKind::Rack`, which stomped the just-restored value.

3. **Ribbon-Effects-tab re-entry mLastFXChannel reset.** Old code reset `mLastFXChannel = "mixer_master"` after every consumed click, forcing Master selection on every subsequent ribbon click and stomping the user's manual channel choice. **Fix:** reset to empty string + only re-select when non-empty. FX-button → page link still works (sets `mLastFXChannel` immediately before `onTabSelected(2)` fires). `onSubPageSelected` no longer re-selects the channel either — `onTabSelected` already handled it.

4. **Ribbon Effects ▾ ▸ Rack/EQ uses TabKind not raw index.** `switchTab(jlimit(0,1,subPageIndex))` resolved "Rack" → PreEQ on non-player 3-tab layouts. Fixed to map index 0 → `TabKind::Rack` and index 1 → `TabKind::PostEQ` directly, layout-agnostic.

**Future enhancements (post-v1.0):**

**FX-1 Rack UI refactor: sidebar picker + single detail pane (Tier 3).**
Refactor the 6-slot horizontal layout into a left-side slot picker (similar to current ribbon sorting rows) + a single large panel editor on the right, pattern similar to the Event Editor. Clicking a slot on the left swaps the right-side detail pane to that slot's editor panel. Motivated by:
- Makes generous space available for the **effect-panel preset loader UI** (cross-referenced in the preset-UI Cross-cutting item). A detail pane with full width + full height easily fits a proper preset browser column (list of presets + Load / Save / Save As / Prev / Next buttons), OR a top-bar preset picker (`[preset name v] [< >] [Save]`). Even better: the sidebar slot row can show the loaded preset name inline (e.g. `FX 1: Reverb - Large Hall`) for at-a-glance visibility without opening the panel.
- Easier slot reordering via up/down arrows on sidebar rows (currently horizontal drag is awkward).
- Room for per-slot metadata (bypass + preset + label + gain meter) in the sidebar row.

**Impact on presets: zero.** Preset data lives entirely in DSP `ValueTree` state (`getStateInformation`/`setStateInformation`); UI layer is fully decoupled. Swapping from 6-simultaneous-panels to 1-panel-at-a-time doesn't touch any DSP state, APVTS params, or ValueTree structure. Round-trip preset compatibility preserved.

**Impact on CPU:**
- **Audio DSP thread:** unchanged. `EffectRack::process()` runs all 6 slots regardless of UI visibility.
- **UI thread:** meaningful win. Currently 6 panels each paint at ~60 Hz (expensive LAF paints like HarmonicLAF hammerite, TimeLAF Pultec, DynamicsLAF cream), each runs VU + DBFS meter timers at 30 Hz, each has knob rotation + filmstrip repaints. Collapsing to 1 visible panel is 6x less paint work + 5x fewer timer ticks firing. On a typical user's system that's ~2-5% CPU recovered, more on slower GPUs or 4K displays.
- **Extra CPU squeeze available:** JUCE's `setVisible(false)` does NOT automatically pause `juce::Timer` callbacks. Hidden panels keep firing their repaint timers in the background (wasted CPU). Override `Component::visibilityChanged()` on each panel to call `stopTimer()` when hidden and `startTimer(...)` when shown again. Combines cleanly with the hide-all-but-one pattern. Turns the savings from "small" to "real."

**UX tradeoff:** the current 6-panel view lets you see all 6 DBFS meters simultaneously, useful for debugging "which effect is squashing my signal". To preserve that with the detail-pane refactor, the sidebar slot rows should carry tiny VU/DBFS strips inline (similar to mixer strip meters). Same idea for bypass LED + preset name + maybe a mini level readout per row.

**Implementation notes:**
- Keep all 6 panel instances constructed (switching is instant); just setVisible(false) on 5 of them.
- `EffectRack` ownership model unchanged (rack still owns the 6 DSPs; panels are views on those DSPs).
- Sidebar row width: ~180 px suggested (effect name + preset inline + VU + bypass + up/down arrows). Detail pane gets the rest (typically 600-700 px wide).
- Preset-UI approaches #1/#2/#3 from the cross-cutting preset-UI item all become trivial — detail pane has ample room for any of them, or a dedicated preset section in the sidebar row.

**PRESET-SAFE.** Pure UI refactor, no DSP or APVTS changes.

---

## Cross-cutting / Architecture

### Player + Page Audit Workflow (the "SLA pattern")

**Established 2026-04-19 during §P1 Harmless Session 1 + Layout Audit. Apply this pattern to every remaining player engine (BaySickSynth / BaySickBass / VibePlayer / BaySickDrums) AND every system page (Mixer / Builder / Effects / DrumsPage / LayersPage / BassPage / etc.) when we get to them.**

The audit is the **first session** for any module that has a design doc (`Files For Claude/<ModuleName>/*.txt` or `Files For Claude/Player Layouts/*.txt` or `Files For Claude/Mega Update Source Files/*`). It's a **planning-only** session — zero code changes, just decisions logged into the Blueprint.

**Step-by-step:**

1. **Read the design doc + the related .png screenshot** if one exists.
2. **Read the current code** for the module: APVTS layout, processor `updateFromApvts`, editor / sub-component attachments.
3. **Enumerate** every documented control from the design doc, numbered.
4. For each control, classify current state: **HAVE** (visible + wired), **PARTIAL** (UI exists but DSP ghost / opposite), or **MISSING**.
5. **Per-element table** in this exact format:

   | # | Element (per design doc) | Current | Proposal | PRESET | Session |
   |---|---|---|---|---|---|

   - **Proposal** must be one of: **WIRE** (add UI + APVTS + DSP), **DROP** (remove from v1 scope entirely), **TIER 3** (defer to post-v1.0 wishlist with a one-line entry under the module's T3 list), or — for already-correct items — `✅ HAVE` / `—`.
   - **PRESET column** must be filled on every row: `PS` (PRESET-SAFE) or `PB ⚠️` (PRESET-BREAK) with pre-v1 / post-v1 note.
   - **Session column** assigns to a session: `SLA-Impl` (small follow-up), `S2` / `S3` / `S4` / etc (existing planned sessions for that module), `T3` (wishlist), or `—` if no action needed.

6. **Tally at the end**: count of WIRE / DROP / TIER 3 + which sessions absorb the work.
7. **Open questions table**: any element where my proposal isn't obvious (PRESET-BREAK trade-offs, ambiguous design intent, scope tradeoffs). Number them and ask explicitly.
8. **Wait for user confirmation** of the open questions + any DROP→TIER 3 reclassifications.
9. **Commit** the locked plan into the module's Blueprint section as: SLA outcome paragraph + DROPPED list + TIER 3 entries (added to the module's T3 list with `(SLA #N, YYYY-MM-DD)` provenance) + Session-N scope updates absorbing the WIRE items + new Last-updated entry.

**The audit-only session never writes code.** Subsequent SLA-Impl / S2 / S3 / etc sessions do the implementation, each shipping its share of the WIRE items per the locked plan.

**Why the format matters:** the per-element table makes scope crystal-clear, prevents elements from slipping through cracks, locks PRESET implications BEFORE code is written, and gives Jeff a single document where every documented spec element has a defensible decision attached.

**Lineage:** Harmless §P1 SLA was the first - 66 elements audited, 16 WIRE / 11 DROP / 7 promoted to T3 / 7 already planned across S2-S5 / 25 already shipped. Memory rule `feedback_player_page_audit_workflow.md` saved alongside.

### LRX realism pass (deferred — needs GL renderer)

**Context:** Several LRX visual-polish passes were spec'd for the app. They rely on smooth gradient / shader rendering that JUCE's default CPU (software) renderer does not handle well — large radial gradients produce visible banding (ghost rings) rather than a smooth fall-off, especially against dark backgrounds. All LRX items below are gated on adding an OpenGL renderer to the app so the passes render smoothly.

**Tier 3 items:**

- **T3-LRX5Vignette Global lens vignette** (disabled 2026-04-21). Implemented in `StandaloneEditor::paintOverChildren` as a radial gradient (center clear, corners darkened). Originally at alpha 0.38 it produced obvious banding rings visible on every page on dark VibePlayer / mixer backgrounds. Reducing to alpha 0.10 did not resolve the banding. Disabled the overlay entirely (the paintOverChildren body is a no-op comment block) until the app has a GL renderer available. To re-enable: restore the radial-gradient draw in paintOverChildren at the original 0.38 alpha once GL is running. PRESET-SAFE (pure cosmetic, no state).

- **T3-LRX8GLSL GLSL shader realism pass** (pre-existing deferral from v1.0 scope per CLAUDE.md). Full GLSL-shader-based rendering for knob skirts, panel textures, and spec highlights. Post-v1.0. Natural carrier for re-enabling the other LRX items (LRX-5 vignette here, any future LRX-N additions) once GL is in. PRESET-SAFE (rendering only).

### Sidechaining infrastructure
Compressor has plumbing (`setSidechainBuffer`, `useSidechain`). Needs:
- APVTS send type definition
- Effect-slot sidechain input routing
- Compressor panel sidechain-source selector
- Routing-graph sidechain edge in VibeGraph

**PRESET-BREAK** ⚠️ — adds new APVTS params to EffectRack slot serialization. If we defer and later add sidechain APVTS params, v1 preset rack layouts won't have them; default-value-on-missing should handle this, but the send-destination graph edges would require a migration pass. **Suggest:** include at least the APVTS scaffolding in v1 even if UI/routing comes later — future-proofs the preset format.

### Combo-box automation infrastructure
Site-wide task: automate combo-box selections via APVTS int params. Currently combos store selection but don't expose to automation.
**PRESET-BREAK** ⚠️ — combo selections become APVTS params → preset format changes. Similar suggestion to sidechaining: add the APVTS scaffolding in v1.

### App-wide juce::Slider -> VibeSlider refactor (2026-04-19, scheduled)
Problem: JUCE's default `juce::Slider::mouseDown` doesn't early-return on right-click unless `setPopupMenuEnabled(true)` is enabled (which would install JUCE's built-in Default/Set-value menu and compete with our `GlobalAutoRightClick`-driven "Automate..." menu). Result: right-click on ANY slider in the app also jogs its value / angle while opening the Automate popup. Jeff confirmed this affects every fader/knob in the app.

Targeted fix already shipped for §12 EQ widget + DynamicParamsPopout + MixerTrackStrip pan / width / fader:
- New `VibeSlider` subclass in `SharedUI.h` whose `mouseDown` / `mouseDrag` / `mouseUp` return early when `isRightButtonDown()` is true. Left-click + drag unchanged. Right-click still propagates up to `GlobalAutoRightClick`'s mouse listener so the Automate + Type-in-value menu still fires.
- `SnapSlider` rebased on `VibeSlider`.
- `ChickenHeadSelector::mouseDown` + `mouseDrag` gained right-click early-return guards.

**Remaining scope (future session):** find-and-replace `juce::Slider` -> `VibeSlider` across every other widget (probably 150-300 sites):
- All 11 effect panels (`Source/Standalone/EffectEditorPanels.cpp`).
- All 5 player editors (`Source/Harmless/`, `BaySickSynth/`, `BaySickBass/`, `VibePlayer/`, `BaySickDrums/`).
- Piano roll control lane, arrangement grid selectors, builder panel.
- `ParametricEQDisplay` any stray uses (EQ polish pass caught the main ones already).
- Event Editor sliders.
- Any other `std::make_unique<juce::Slider>` / `juce::Slider mX` sites not touched by this pass.

PRESET-SAFE (pure UI behaviour; no DSP or APVTS change). Can ship any time; no dependency on other work. Scope estimate: ~2 hours.

### Orfanidis analytical anti-cramping (EQ8)
Alternative to §12f oversampling path. Replaces high-frequency cramping via analytical coefficient correction rather than oversampling. Potential future replacement. **PRESET-SAFE.**

### Pre-rack EQ on all InsertNodes (B2 architecture)
Add a second `EQ8MsDSP mPreEQ` to every `InsertNode` (Layer / Bass / Drum / Audio / Aux), creating pre+post rack EQ per insert. Signal: `input → preEQ → rack → postEQ → fader`. New APVTS params `mixer_<kind>_<N>_preeq_*` mirroring existing `_eq_*`. UI split:
- **DrumsPage / LayersPage / BassPage EQ tabs** → pre-rack EQ (new)
- **EffectsPage EQ tab** → post-rack EQ (unchanged)

PRESET-SAFE — pre-EQ defaults to flat/bypass so v1-without-preEQ presets load fine. To do at **§P2 player review** (natural touchpoint since DrumsPage EQ rebind is part of that pass). The pre-EQ addition is what actually delivers "per-slot EQ on DrumsPage" since it provides the hook the drum slot dropdown binds to.

### Mixer strip meter fix (DONE 2026-04-17)
Was: every Layer/Bass/Drum strip showed its bus peak instead of its own insert peak. Fixed by switching `MixerPage::timerCallback` to use `getInsertPeakDb(kind, index)` per strip — matches the Aux pattern. 3-line fix.

### Preset-as-double-click-default for engine knobs (DONE 2026-04-25)
Double-clicking any knob in an engine editor now resets that knob to the currently-loaded preset's value, not the engine's hardcoded factory default.  Standard DAW behavior — matches FL Studio.

Implementation:
- `TaggedSliderAttachment` (in `SharedUI.h`) — drop-in replacement for `juce::AudioProcessorValueTreeState::SliderAttachment` that ALSO tags the slider with `getProperties().set("apvtsId", paramId)` at construction.
- `setSliderDoubleClickDefaultsFromApvts(Component&, APVTS&)` (in `SharedUI.cpp`) — recursively walks all child components, finds tagged sliders, calls `setDoubleClickReturnValue(true, currentApvtsValue)` on each.
- `using SliderAtt = TaggedSliderAttachment` in 7 editor headers (`BaySickSynthEditor.h`, `BaySickBassEditor.h`, `HarmlessEditor.h` + 3 sub-component headers `HarmlessFilterRow.h` / `HarmlessRoutingMatrix.h` / `HarmlessXYZPad.h`, `VibePlayerEditor.h`).  The typedef change auto-tags every slider already attached via SliderAtt — no per-attachment-site edits.
- Each main engine editor (Synth / Bass / Harmless / Vibe) inherits `juce::ValueTree::Listener` and overrides `valueTreeRedirected` to re-walk on every `apvts.replaceState` (preset load / project load / paste).  Initial walk fires at end of editor constructor so opening a tab with a preset already loaded gets the right home values.  Listener registered/unregistered in editor ctor / dtor.
- Sub-components (HarmlessFilterRow / RoutingMatrix / XYZPad) only need the typedef change — the parent editor's walker recurses into them.

Out of scope: ComboBox / Button "reset to preset value" (JUCE has no native double-click reset for those — would need custom mouseDoubleClick override per widget type).  Mixer strips + effect panels still use raw `SliderAttachment` (not engine presets, so factory-default reset is correct there).

PRESET-SAFE.

---

### Automation display-name system + paramId coverage fixes
Shipped 2026-04-17.

**Bug fixes:**
1. **Fix 1 — `setSlotContext` coverage.** `EditorPanelBase::setSlotContext()` only walked the base-class `knobs` vector; Chorus / Delay / Reverb / Limiter / Saturation keep their knobs in own `r1knobs`/`r2knobs` vectors and were never stamped with paramIds. Right-click on those panels showed bare labels like "Wet" instead of `layers_bus_s0_wet`. Added `virtual std::vector<VKnob*> getExtraKnobs()` hook on the base; the 5 panels override it; `setSlotContext` + `setUndoContext` + automation registrar now walk both the base `knobs` and `getExtraKnobs()`.

2. **Fix 2 — right-click-created automation blocks didn't populate the Browser.** `StandaloneEditor::createAutomationBlock()` added an `ArrangementBlock` but never called `mPM->addAutomationTemplate()`. Compare with BuilderPage's "New Automation Clip" flow which did both. Added the one-line `addAutomationTemplate` call to `createAutomationBlock`.

**Display-name system:** `AutomationLane` got a new `userDisplayName` field (default empty). Three-layer naming:
- `paramId` — stable backend key (e.g. `layers_bus_s0_mix`). Never changes after creation. Used for APVTS writeback + applicator lookup + preset key.
- Auto-generated display name — resolved on demand via `StandaloneEditor::resolveAutomationDisplayName(paramId)`. Parses the paramId prefix to infer channel (Layers Bus / Layer 1 / Drum 3 / Aux 0 / Master / etc.), looks up the current effect in the slot via the rack, and returns `"Channel - Effect - Param"`. Effect swaps inside a slot update the name immediately (not cached). Falls back to channel-only or raw paramId when parsing fails.
- `userDisplayName` — user rename. When non-empty, takes precedence. Written via `PatternManager::setAutomationTemplateUserName()` and propagated to any already-placed blocks sharing the paramId. Empty = revert to auto.

**UI wiring:** `StandaloneEditor::displayNameFor(lane)` returns `userDisplayName` when set, else auto-resolved. Threaded through:
- Event Editor title label + window caption
- AutomationBrowserPane (side pane in the Event Editor)
- BuilderPage's BrowserPanel (main left browser, includes snapshot-hash update so effect swaps / renames trigger a refresh)
- `ArrangementGrid::drawAutomationClip` on-grid block labels

**Rename UX:**
- Browser right-click "Rename..." (existing) now writes `userDisplayName` only, leaving `paramId` stable. Previously it overwrote `paramId`, breaking applicator lookups.
- New "Revert to auto name" right-click item appears only when `userDisplayName` is non-empty. Clears the override.

Preset-safe: new field defaults to empty; project serialization (Phase 5A, not yet implemented) will pick it up automatically.

### Effect-panel preset loader UI (Tier-3 architecture)
Logged 2026-04-18 during §9 Saturation retrospective. Player editors (Harmless, BaySickSynth, BaySickBass, VibePlayer, BaySickDrums) all have preset buttons. Effect panels (all 12) do NOT have any preset load/save mechanism. §9's T5 character voicings (Tube/Console/Tape/Fuzz) depends on this. So does every other Tier-3 "character voicing preset" item on other modules (§1 Chorus algorithm voicings, §6 Overdrive character presets, §8 Reverb algorithm presets, §5 Limiter release voicings, etc.).

**Three approach options identified (pick at implementation time):**

1. **Right-click menu approach** - right-click panel background -> "Presets >" submenu with Load/Save/Save As/Delete/preset list. **Zero panel-space cost.** Matches existing right-click-to-automate + right-click-type-value patterns. Users may not discover it without a small hint label.

2. **Small `Preset v` button** top-right of each panel, ~60x18 px. Pops menu when clicked. **~60x18 px space cost per panel.** More discoverable. Matches the existing player-LAF preset button pattern.

3. **Slot-chrome preset picker** - lives in the EffectRack slot header (next to effect-type dropdown and bypass LED), not inside the panel. Same UI for all effects. **Panel layout untouched.** Most invasive to the slot chrome code.

**Cross-module impact when implemented:**
- All 12 effect panels get the preset UI (Chorus/Compressor/Delay/Flanger/Limiter/Overdrive/Phaser/Reverb/Saturation/Tape/TransientShaper/EQ8).
- Preset files live at `Presets/<effect>/<name>.json` or similar (format TBD).
- First character-voicing preset bundle (§9 T5 Tube/Console/Tape/Fuzz) ships alongside.

**Preset file format:** stored DSP state equivalent of `getStateInformation()` output, serialised as JSON/XML. Each effect's state is already fully captured by its ValueTree - just needs to be dumped to file instead of MemoryBlock.

PRESET-SAFE (new feature, doesn't touch existing preset semantics).

### Automation applicator dangling-pointer crash fix (cross-apply)
Shipped 2026-04-18 alongside §7 Phaser retrospective after Jeff hit a crash during testing. `StandaloneEditor::applyAutomationAtCurrentPosition` stores applicator lambdas keyed by paramId in `mAutomationApplicators`. Those lambdas were capturing raw `juce::Slider*` (in `EffectEditorPanels::setSlotContext`) or raw `this` (in `MixerTrackStrip::setAutomationPrefix`) by value; when the underlying VKnob slider / MixerTrackStrip was destroyed (effect swap in slot, aux strip removed, panel rebuild), the stale applicator remained in the map. Next automation tick dereferenced freed memory and crashed inside `juce::NormalisableRange::snapToLegalValue` — the std::function member there was being called in its empty state because the containing `Slider::Pimpl` was zombie memory. Stack trace confirmed: `applyAutomationAtCurrentPosition() -> lambda -> Slider::Pimpl::setValue -> constrainedValue -> snapToLegalValue -> _Func_class::_Empty()`.

**Fix:** both call sites now capture `juce::Component::SafePointer<juce::Slider>` instead of a raw pointer. Applicator + reader lambdas null-check `safeSl.getComponent()` before use. When the underlying component is gone, applicator becomes a no-op + reader returns 0.5 (neutral normalized value) — safe even if the applicator map hasn't been cleaned up yet.

Files touched:
- `Source/Standalone/EffectEditorPanels.cpp` - `EditorPanelBase::setSlotContext` `regKnob` lambda.
- `Source/Standalone/MixerTrackStrip.cpp` - `setAutomationPrefix` fader + pan blocks.

PRESET-SAFE, no behavior change for the non-crash path.

**Future work (not fixed here):** `StandaloneEditor` should also prune stale applicators from the map when a slot's effect is swapped or a strip is removed, rather than leaking entries across swaps. That's a memory cleanliness concern distinct from the crash; the SafePointer fix makes it non-crashing even when leaks occur.

### EQ band automation wiring (pre-booked Tier 1 for §12 Phase 2)
Pre-existing gap, surfaced during §12 Phase 1 testing. Jeff noted he'd expected this to already work. Pre-booked as Tier 1 at §12 Phase 2 review time.

**Current state:** `ParametricEQDisplay` handles EQ band drag input directly via `mouseDown`/`mouseDrag` and does NOT expose per-band-parameter sliders with `componentID` tags, so:
- `GlobalAutoRightClick` never sees EQ band controls → no right-click "Automate: X" menu
- Band params (freq/gain/Q + future type/slope/channel) are drag-editable but NOT automatable
- No APVTS bridge — widget writes `EQ8DSP::setBandFreq/Gain/Q` directly; no paramIds, no applicators, no listeners

**Fix pattern (Tier 1 at §12 Phase 2):**
1. **APVTS param registration** per EQ instance, lazily per strip (matches the 5F-4a mixer-strip lazy pattern). Scope per Jeff's confirmation: **Freq + Gain + Q per band** in v1 initial ship; **type + slope + channel** added when 12h per-band M/S ships (since `channel` is a new field there).
2. Systematic naming: `<prefix>_eqmid_b{0..7}_{freq|gain|q}` and `<prefix>_eqside_b{0..7}_...`. `<prefix>` matches existing channel prefixes (`layers_bus`, `bass_bus`, `drums_bus`, `master`, `fx_bus`, `clips_bus`, `mixer_layer_{N}`, `mixer_bass_{N}`, `mixer_drum_{N}`, `mixer_audio_{N}`, `mixer_aux_{N}`).
3. **Widget input bridge:** replace direct `EQ8DSP::setBandFreq/etc` calls in `ParametricEQDisplay` drag handlers with `setValueNotifyingHost` on the matching APVTS param. APVTS change → parameter listener on audio thread → `EQ8DSP::setBandFreq/etc` (with smoothing from 12c).
4. **Automation hookup:** register `VKnobAutomation::sOnRegisterApplicator` + `sOnRegisterReader` per paramId when the strip is selected on the EffectsPage (mirrors `EditorPanelBase::setSlotContext`). Applicator writes to the APVTS param; reader reads it back.
5. **Right-click "Automate: ..." menu** in `ParametricEQDisplay::mouseDown`: hit-test the clicked band handle, show menu with items per automatable param (Automate Freq / Automate Gain / Automate Q + type/slope/channel once they're in scope).

**Scope notes:**
- Lazy-register: an EQ instance's params are only registered with APVTS when the user first selects that strip on the EffectsPage (matches mixer strip lazy pattern). Avoids registering thousands of params upfront for strips that never get edited.
- 12h dependency: `channel` param comes from 12h per-band M/S. If automation wiring lands after 12h, channel joins the automatable set naturally.
- **PRESET-SAFE.** APVTS adds are additive; existing DSP state unchanged; existing EQ drag UX unchanged (just goes through APVTS now instead of direct DSP writes).

**Ship order within Phase 2:**
Recommend 12h (per-band M/S) → 12i (spectrum analyzer) → EQ automation wiring last. EQ automation depends on the final Band struct shape which 12h modifies.

### Mixer-strip EQ binding fixes — Layer / Bass / Aux / Audio (2026-04-18)
Pre-existing bug cluster, same pattern as the drum-strip dropdown fix earlier in the same day. Symptom: on the Effects Page, when a layer-page / bass-page / aux-strip / audio-row channel was selected, the EQ tab/display bound to a null EQ pointer (or an EQ from a non-active legacy registry) so changes made to the mixer-strip EQ did NOT affect sound. Drum mixer strips were already fixed. Bus-level EQs (layers/bass/drums/master/fx/clips) always worked. Player-page EQs (LayersPage / BassPage / DrumsPage own EQ tabs bound to the bus EQ) always worked.

Root causes (four parallel call sites in `EffectsPage::onChannelChanged`):
- **Layer IDs 200..207** — `eq = nullptr` hardcoded with comment `"per-channel post-rack EQ not yet wired (Phase 3)"`. Stub never filled in after 5F-4a migration.
- **Bass IDs 300..303** — same hardcoded `eq = nullptr`.
- **Aux IDs 600..615** — same hardcoded `eq = nullptr` (stub placeholder).
- **Audio IDs 400..499** — called `getAudioRowEQ(row)`, which internally ONLY looked in the legacy `mInstrChannelNodes` registry, not the post-5F-4a `mAudioInserts` InsertNode map. When audio rows live in the InsertNode registry (the normal case after 5F-4a), `getAudioRowEQ` returned nullptr.

Meanwhile the audio path DOES route through the InsertNode's EQ via `PluginProcessor::processBlock` calling `vg.processInsert(InsertKind::Layer|Bass|Aux|Audio, i, ...)`. So UI binding was the gap — audio was already flowing through each EQ, but with no UI to drive coefficients, each sat at flat defaults and the visible EQ tab appeared non-responsive.

**Fix (two files):**
- `Source/Standalone/EffectsPage.cpp` — `onChannelChanged`: Layer (200..207), Bass (300..303), Aux (600..615), Audio (400..499) branches now resolve rack + EQ via `VibeGraph::getInsertRack/getInsertEQ(InsertKind::*, idx)`. Legacy `getLayerPageRack` / `getBassPageRack` / `getAuxRack` / `getAudioRowRack+getAudioRowEQ` kept as fallbacks for state-restore edge cases (belt-and-suspenders, mirrors the drum fix pattern).
- `Source/VibeGraph.cpp` — `getAudioRowEQ` itself patched to use the InsertNode-first pattern (checks `mAudioInserts` first, falls back to `mInstrChannelNodes`). Mirrors `getAudioRowRack`'s existing dual-path. Fixes any future callers beyond EffectsPage.

PRESET-SAFE (UI binding fix only; no data format change). After rebuild, mixer-strip EQs may suddenly start affecting sound based on whatever EQ state was previously in session auto-save — if the InsertNode's EQ had non-default state sitting unused, the newly-connected UI will now reflect and control it.

**Pattern lesson (logged for future-me):** the 5F-4a insert-node migration introduced a dual-path (legacy arrays/maps + new InsertNode registry) in the AUDIO path but left UI-binding code at various call sites still reading from the legacy-only paths. FIVE call sites were broken by the same asymmetry: drum-strip dropdown (fixed earlier today), plus Layer / Bass / Aux / Audio mixer-strip EQ bindings (this fix). Every `getLayerPageRack`, `getBassPageRack`, `getAuxRack`, `getAudioRowRack/EQ`, and `getInstrChannelRack/EQ` call site should be re-audited for the same asymmetry. In particular, `EffectsPage::getChannelPrefix` still returns `instr_{id}` for drum IDs 100..115 — paramId stamping for drum-strip knobs may still use the legacy prefix which doesn't match the `mixer_drum_{N}` convention used by other systems. Automation display-name resolver (`StandaloneEditor.cpp` line 1163) still calls `getInstrChannelRack` for `instr_{id}` prefixes — returns nullptr for drums now since they migrated. Logged as a follow-on audit item.

### Drum strip effects-rack dropdown fix (2026-04-18)
Pre-existing bug discovered during §11 Transient testing. Symptom: on the Effects Page, when a drum strip (ID 100..115) was selected in the channel dropdown, clicking an effect-type option in the add-effect dropdown did nothing. Layer / Bass / Master / FX / Bus / Aux / Audio strips all worked correctly.

Root cause: `EffectsPage::onChannelChanged` routed `id >= 100` to `VibeGraph::getInstrChannelRack(id)`, which is the LEGACY InstrChannelNode path. During 5F-3, drums were migrated to the new `InsertNode` architecture (`InsertKind::Drum`) and are no longer registered as InstrChannelNodes. `getInstrChannelRack(100+slot)` returns nullptr for drums -> `mRack` set to nullptr -> `onEffectChosen` early-returns at `if (!mRack) return` -> nothing happens on option click.

The dropdown enumeration (`StandaloneEditor::onGetActiveChannels`) already used the correct InsertNode-based path (via `MixerPage::getDrumStripIndices`) and populated drums with ID=100+slot. Just the resolve-to-rack step was missed during 5F-3 migration.

**Fix:** route drum IDs (100..115) through `VibeGraph::getInsertRack(InsertKind::Drum, id-100)` + `getInsertEQ(InsertKind::Drum, id-100)`. Added `getInsertEQ` to VibeGraph public API (parallel to existing `getInsertRack`). Kept a fallback to `getInstrChannelRack` for stray state-restore paths (belt-and-suspenders, mirrors the same pattern in the dropdown enumeration).

Files changed:
- `Source/VibeGraph.h` - added `EQ8MsDSP* getInsertEQ(InsertKind, int)`
- `Source/VibeGraph.cpp` - implementation (2 lines, mirrors getInsertRack)
- `Source/Standalone/EffectsPage.cpp` - `onChannelChanged` now routes 100..115 via InsertNode first, legacy fallback second

PRESET-SAFE (behavior fix only; no data format change).

### A9 Panel-construct DSP-state sync (cross-apply)
Shipped 2026-04-18 alongside §7 Phaser retrospective. Many effect panels were defaulting their `DualLabelToggle` and `ChickenHeadSelector` controls on construct instead of reading the DSP's current state. Preset reload showed stale toggle/selector positions until the user clicked each one. Fix pattern: every non-attachment UI control (toggles + selectors that don't ride APVTS attachments) reads `dsp->mField` at construct and calls `setToggleState(value, dontSendNotification)` or `setSelectedIndex(value, dontSendNotification)`. 18 controls across 4 panels:

- **ChorusPanel (6):** `lfoWaveSel[0..2]` (read `dsp->lfoParams[i].wave`), `voicesTog` (read `dsp->voices == 6`), `crossTypeTog` (read `dsp->crossType == 1`), `wetOnlyTog` (read `dsp->wetOnly`).
- **DelayPanel (5):** `modelSel` (read `dsp->mDelayModel`), `fbFilterTypeSel` (read `dsp->mFBFilterType` - previously hardcoded to index 1 regardless of preset), `keepPitchTog` (read `dsp->mKeepPitch`), `fbDistTypeTog` (read `dsp->mFBDistType == 1`), `syncDivSel` (reverse-lookup `kSyncDivNumer/Denom` tables against `dsp->syncNumerator/syncDenominator`).
- **FlangerPanel (3):** `invFbTog` (read `dsp->mInvertFeedback`), `invWetTog` (read `dsp->mInvertWet`), `syncDivSel` (read `dsp->mSyncDivIdx`).
- **PhaserPanel (4 new + reset for existing):** `bpmTog` (read `dsp->mSyncBPM`), `invFbTog` (read `dsp->mInvertFeedback`), `rangeTog` (read `dsp->mFreqRange == 1`), `stagesSel` (reverse-lookup `kStageValues[8]` against `dsp->mNumStages`), plus new `waveSel` (read `dsp->mLFOWaveIdx`) and `syncDivSel` (read `dsp->mSyncDivIdx`).

`LimiterPanel` and `OverdrivePanel` already did this correctly (`autoRelTog`/`autoMuTog`/`linkTog` and `x100Tog`/`parallelTog`/`osSel` respectively) - those were the reference implementations. All DSP state fields are public so no getters needed; same pattern as `DelayPanel::tempoTog` line 893 which was the original correct implementation.

**Extended to sliders (2026-04-18 during §10 Tape debugging):** the A9 pattern originally only applied to toggles and chicken-head selectors. Sliders were assumed to match DSP state because `buildKnobs` sets the slider value to the same default baked into the KnobDef list, and that usually matched the DSP header default. But when a DSP default CHANGES in code (as it did during §10 Tape iteration: `mFlutterRate` default flipped 15 -> 5), the `buildKnobs` default updates alongside, while any stale in-memory DSP state from before the rebuild still holds the old default. Result: slider displays 5, DSP plays 15 -> apparent 3x rate mismatch that Jeff observed.

**Rolled out to ALL effect panels 2026-04-18:** pattern applied is a one-liner per knob using `sendNotificationSync`:
```cpp
knob->slider.setValue (dsp->mField, juce::sendNotificationSync);
```
This:
1. `setValue` clamps the incoming DSP value to the slider's range if out-of-range (e.g., when a knob range gets shrunk in code and DSP still holds a value above the new max).
2. `sendNotificationSync` fires the existing `onValueChange` lambda, which calls `dsp->setField((float)slider.getValue())` with the clamped value.
3. The DSP setter's own range clamp + CPU guard (`if (n != mField) return`) either reconciles DSP to the slider-clamped value, or no-ops if they already agree.

Result: on panel construct, slider accurately reflects whatever the DSP authoritative state is, and the DSP is reconciled to that value re-clamped by both the slider range and the setter range. If DSP had an out-of-range value (from a changed range or preset migration), it gets pulled back into range. If DSP and slider already matched, the whole block is a no-op.

Panels updated: TapePanel, CompressorPanel, ReverbPanel, SaturationPanel, ChorusPanel, DelayPanel, FlangerPanel (with `mFeedback * 100` for the Feed pct knob), OverdrivePanel, PhaserPanel, LimiterPanel, TransientShaperPanel (with `mAttack * 100` + `mSustain * 100` for the -100..100 knobs vs -1..1 DSP storage).

**What this fixes (audible):** nothing by itself — the DSP was always authoritative. What was broken was the *display*, which could mislead the user about what value they were actually hearing. After this rollout, the slider value always agrees with the DSP value on every panel open.

### §P3-CORE Cross-Apply to other engines + effects (post-§P4, scheduled phase)

**Scheduled:** after §P4 DrumsPage ships. Purpose: maximise the return on the 11 DSP additions we built for BaySickSynth/Bass during §P3-CORE by porting the portable ones to other engines (Harmless, BaySickPlayer) and either integrating into existing effects OR spinning up new rack modules. This turns §P3-CORE from "BaySickSynth/Bass features" into "whole-DAW ingredients."

#### Engine cross-apply matrix

| DSP add | BaySickBass | Harmless | BaySickPlayer |
|---|---|---|---|
| P3.1 Pitch Env | ✅ auto (shared DSP) | Partial (mod-matrix has pitch target already) | 🔥 Useful — authentic 808-bass-sample pitch sweep, brass-sample scoops |
| P3.2 Sine waveform | ✅ auto | N/A (additive engine architecture) | N/A (sample-based) |
| P3.3 Noise-only mode | ✅ auto | Different approach (routing matrix rm_sub) | N/A |
| P3.4 Free-Hz dual-osc | ✅ auto | N/A | N/A |
| P3.5 Transient Injector | ✅ auto | 🔥 BIG WIN — instant realistic hammer strike on Rhodes/Wurli/Clav Harmless patches | 🔥 BIG WIN — #1 missing feature for sample realism; inject a click onto ANY sample |
| P3.6 Multi-burst Env | ✅ auto | Useful — alternative to standard ADSR | 🔥 Useful — retrigger a sample 4× for sample-based handclap / roll |
| P3.7 Hard Sync | ✅ auto | N/A | N/A |
| P3.8 Ring Mod | ✅ auto | N/A (works in harmonic space) | Partial (sample × osc ring mod — fancy) |
| P3.9 Pink/Brown noise | ✅ auto | Feeds Harmless's sub-osc | N/A |
| P3.10 Analog Drift | ✅ auto | 🔥 BIG WIN — Harmless patches go from "clean digital" to "analog alive" | 🔥 BIG WIN — ends "sterile sample" feel on every preset |
| P3.11 Unison | ✅ auto | Already has its own unison engine (T2-C S3 — different architecture, keep separate) | Useful — fat sample stacks |

**Three "🔥 BIG WIN" cross-apply candidates that would massively improve the WHOLE DAW:**
1. **Transient Injector → Harmless + BaySickPlayer** (biggest impact, smallest effort — same 3 knobs on each engine's MOD tab)
2. **Analog Drift → Harmless + BaySickPlayer** (universal warmth, every preset benefits)
3. **Pitch Envelope → BaySickPlayer** (unlocks pitch-sweep sample playback — 808 sub drops, brass scoops)

#### Effect integrations (existing rack modules)

| Priority | Add | Integration | Why |
|---|---|---|---|
| **High** | P3.10 Drift | Tape effect | Tape's wow/flutter character IS pitch drift. Our DSP ports straight onto a new Drift knob. Clear Prophet-tape / CS-80-tape realism win. |
| **Medium** | P3.10 Drift | Delay (tape-echo mode) | Classic tape delays (EP-3, Space Echo, RE-201) wobble pitch in feedback loop. |
| **Medium** | P3.9 Pink/Brown noise | Saturation / Tape / Overdrive | Optional "noise-floor" knob for tape-hiss / tube-hiss realism. Niche but authentic. |
| **Low** | P3.10 Drift | Chorus / Flanger | Add random pitch variation on top of LFO. Marginal — LFO already covers main modulation. |

**Why most P3.* adds DON'T fit existing effects:**
- P3.1 Pitch Env, P3.5 Transient Injector, P3.6 Multi-burst → all triggered by **noteOn**. Effects don't know about notes.
- P3.2 Sine, P3.3 Noise-only, P3.4 Free-Hz, P3.7 Hard Sync, P3.11 Unison → all require **oscillators**. Effects process incoming audio, don't generate.

#### New effect modules (candidates — separate scope from Round 1/2)

| Candidate | Source | Why it's worth it |
|---|---|---|
| **Ring Modulator effect** | P3.8 math | Classic effect pedal (Fairfield Circuitry, Moogerfooger). Metallic/bell character on ANY audio source (drums, vocals, guitar). Math is already written. |
| **Transient Injector effect** | P3.5 math | Distinct from existing Transient Shaper. Shaper reshapes existing transients; Injector ADDS a click. Usable on kick/snare tracks. |
| **Gate / Rhythmic Tremolo effect** | P3.6 Multi-burst engine | We don't have a Gate effect. Multi-burst drives rhythmic volume patterns cleanly — tremolo to rhythmic stutter-gate. |
| **Analog Drift / Tape Pitch Wander** | P3.10 math | Could bolt into existing Tape module OR become its own subtle pitch-drift effect (different from detuned chorus). |

#### Recommended phase breakdown (all post-§P4)

**Round 1 — Engine cross-apply, biggest wins (smallest effort):**
- Port **Transient Injector** (P3.5) to Harmless + BaySickPlayer — same 3 knobs (AMT / DUR / COLOUR) on each engine's MOD-equivalent panel.
- Port **Analog Drift** (P3.10) to Harmless + BaySickPlayer — same single knob on each.
- Est: 2-3 sessions.

**Round 2 — Engine cross-apply, medium:**
- Port **Pitch Envelope** (P3.1) to BaySickPlayer — sample-based pitch sweep.
- Est: 1-2 sessions.

**Round 3 — Effect integrations (lightweight):**
- Integrate **Drift → Tape effect** (new "Drift" knob on the Tape panel using existing DSP).
- Integrate **Drift → Delay effect** (tape-echo-style drift on the feedback loop).
- Optional: **Pink/Brown noise-floor → Saturation / Tape / Overdrive** (noise-floor knob + color selector).
- Est: 2-3 sessions.

**Round 4 — New effect modules (larger scope, separate phase):**
- **Ring Mod effect** — new rack module.
- **Gate / Rhythmic Tremolo effect** — new rack module driven by Multi-burst engine.
- **Transient Injector effect** — new rack module distinct from Transient Shaper.
- Est: 3-5 sessions (each module is a full DSP + editor panel + slot integration).

**Total Rounds 1-3 estimate:** ~5-8 sessions. Round 4 is its own phase.

**Payoff:** every engine in the DAW gains "analog warmth" (Drift) + "click-on-attack" (Transient). Effect rack gains 3 net-new tools (Ring Mod, Gate, Transient Injector effect). The 11 DSP adds stop being "BaySickSynth/Bass features" and become "whole-DAW ingredients."

**Already-different implementations** (deliberately NOT cross-applied):
- Harmless Unison (T2-C S3) uses a different per-voice slot/detune/phase system in its additive architecture. Our unison is saw-stack based. Keep separate.
- Harmless Filter/Mod envelopes live in the mod-matrix registry. Different paradigm than our ADSR. Keep separate.

**Trigger**: do not begin this phase until §P4 DrumsPage (all 4 sub-items) has shipped. Drums is the highest user-visible priority and pulls §P3-CORE recipes into user-facing factory kits first.

---

### Right-click "Type in value" on all value controls
Shipped 2026-04-17. Every VKnob rotary and every plain `juce::Slider` with a non-empty `componentID` now shows a second right-click menu item: **Type in value...**  Opens a modal `juce::AlertWindow` pre-filled with the slider's current display text (matching the drag popup's units via `Slider::getTextFromValue`). The prompt text also shows the valid range in the slider's own units (e.g. "Range: 1.0 ms - 2000.0 ms") so users know what values are accepted before typing. User types a value; `Slider::getValueFromText` parses it through the slider's inverse-unit logic, and `setValue(..., sendNotificationSync)` auto-clamps to range + drives `onValueChange` so DSP picks it up. Skipped for non-Slider Components (buttons, labels, etc.). Implementation: `VKnobAutomation::promptSliderValueEntry()` in `SharedUI.cpp`, wired from both `VKnob::mouseDown` and `GlobalAutoRightClick::mouseDown`.

---

**Last updated:** 2026-04-22 (§P3 FULLY CLOSED across BaySickSynth + BaySickBass + §P1 Harmless cut-self retro. Sessions A/B/C + D1-D11 + E all shipped in this sitting; §P3-CORE = 11 DSP adds live in shared BaySickSynthDSP + BaySickSynthVoice (Pitch Env / Sine waveform / Noise-only mode / Free-Hz dual-osc / Transient Injector / Multi-burst envelope / Hard Sync / Ring Mod / Pink+Brown noise / Analog Drift / Unison). Every 🔥 BIG WIN recipe from TR-808/909/606 drums through Simmons SDS / Yamaha RX FM drums / DX7 percussion / tuned percussion / hand percussion / classic basses/leads/pads / keys/organs/EP / sound FX is now achievable in BaySickSynth+Bass — ~90+ recipes captured in the §P3 Preset Recipe Catalogue. Session A landed T1.1 false-positive-corrected flt_type audit, T1.2 componentID on every attached slider (both editors), T2.2 filter-type LED selector (LP/HP/BP/Notch), and the bkb_ → bsb_ APVTS prefix rename across source + CMakeLists + CLAUDE.md + StandaloneEditor engine-tag resolver (pre-v1 cleanup, no user presets existed yet). Session B shipped T2.1 LFO tempo-sync via new lfo_division choice param + host BPM read from AudioPlayHead + std::atomic<float> mEffectiveLfoRate wired into BaySickVisualizerScreen so the scrolling-LFO visualizer tracks tempo-sync correctly + editor DIV combobox with grey/enable state listener on lfo_sync. Session C added T2.3 velAmpTrack (default 1.0 = PRESET-SAFE) + T2.4 Legato as 4th voiceMode with manual MIDI dispatch in handleLegatoMidi (last-note-priority held-note stack, retargetLegato method re-pitches without env/LFO/filter/phase reset, mLegatoSynthNote bookkeeping so final noteOff matches synth's view, voice-0 forced reuse via stopNote(0,false) before synth.noteOn fixing the 2-and-2 ping-pong across pattern loops, 1-ms declick fade-in via mDeclickGain on every startNote suppressing click from hard legato termination, !mInRelease guard in startNote glide condition preventing stale-pitch glides on re-allocated release-tail voices). Sessions D1-D11 = all 11 §P3-CORE DSP adds with both Synth + Bass editor UI (MOD tab introduced as 6th tab housing NOISE/TRANSIENT/BURST/DRIFT/UNISON groups in 8-column grid). Session E cutSelf retrofit: BaySickSynth + BaySickBass share a setCutSelf bool in BaySickSynthDSP (Poly-only preprocess injecting noteOff before each noteOn), Harmless gained its own mCutSelf + applyCutSelf preprocess in HarmlessSynth::renderNextBlock, APVTS param in processor, and a CUT SELF toggle in the Global OUTPUT row right of VEL (widened to 56 px to fit label). All §P3 additions PRESET-SAFE by design (new params default to neutral values or modes not selected by default). **Layout polish pass shipped same session:** BssLedRadio refactored to grid mode with Harmless-VEL-style button rendering (dark fill + BaySick-coloured LED bar on top edge when active + green text on / grey text off + 10pt bold), 5 LED radios per editor x 2 editors migrated to grid (VoiceMode 1x4 Poly/Mono/Lead/Legato, ModWheel 1x2 Filter/LFO, FilterType 1x4 LP/HP/BP/Notch, LFO Shape 1x3 Sine/Saw/Square, LFO Dest 1x3 Filter/Pitch/OscMod). Buttons spread across top of their containing group with consistent spacing between cells (6 px hGap, 20 px cell height). kKnobSz/kBassKnobSz = 80 applied across every placeKnob lambda (6 per editor) + Glide + ModWheelAmt + LFO Rate. OSC ENV tab switched from stacked top/bottom rows to side-by-side GroupComponent boxes (AMP ENV | PITCH ENV). MOD BURST button moved from vertical-center to top of the BURST ENV group, knobs below. Filter tab re-balanced from 3-column horizontal (50/22/28) to XY pad left + TYPE-top / TRACKING-below on the right so the 1x4 FILTER TYPE button strip has enough horizontal room. BaySickSynthLAF + BaySickBassLAF drawButtonBackground + drawButtonText split to VEL-style-for-toggle-buttons vs push-style-for-menu-buttons so CUT SELF + SYNC + RING + NOISE ONLY + BURST + LFO SYNC all render with the same green-LED-bar visual language as the grid buttons (tabs + preset dropdown keep the push-button look). Harmless CUT SELF button text displays "CUT SELF" not "CUT" (button widened 34 -> 56 px). Hover value popups enabled on all sliders (setPopupDisplayEnabled) across Synth + Bass editors. Modifier-knob value display is now mode-aware via textFromValueFunction/valueFromTextFunction: Musical shows 0.00-1.00, Hz Offset shows "0 Hz"-"2000 Hz", Absolute Hz shows "20 Hz"-"20.0 kHz" with log scaling. Tooltip box widened from 300 -> 460 px in VibeLAF::getTooltipBounds with proper TextLayout-based measurement so long multi-line tooltips (SYNC, RING, etc.) render in full. Build-system recovery completed: do_build.bat + CLAUDE.md build paths Vibesynth -> BaySickDAW corrected; new configure.bat added (PowerShell doesn't have MSVC env loaded, needs vcvars64 for cmake configure). §P3 CLOSED. Next phase: §P4 DrumsPage Dual-Engine Restructure (SLA audit first per SLA workflow, planning-only). Post-§P4, the logged §P3-CORE Cross-Apply Phase kicks off (Transient Injector + Analog Drift port to Harmless + BaySickPlayer; Pitch Env to BaySickPlayer; then Drift/noise-floor integrations into Tape/Delay/Saturation effects; then new Ring Mod + Gate + Transient Injector effect modules). Memory: `feedback_capture_preset_recipes.md`, `feedback_cross_apply_phase_post_drums.md`, `feedback_oversized_knobs.md` saved alongside. Prior entry below.)

**Prior update:** 2026-04-21 (§P3-CORE Cross-Apply Phase LOGGED to Cross-cutting / Architecture section — scheduled to run after §P4 DrumsPage ships. Comprehensive 4-round plan: Round 1 ports Transient Injector + Analog Drift to Harmless + BaySickPlayer (biggest wins, smallest effort); Round 2 ports Pitch Envelope to BaySickPlayer; Round 3 integrates Drift into Tape + Delay effects (tape-realism wow/flutter + tape-echo feedback wobble) and optional Pink/Brown noise-floor into Saturation/Tape/Overdrive; Round 4 is net-new effect modules (Ring Mod, Gate/Rhythmic Tremolo, Transient Injector effect distinct from Transient Shaper). Engine cross-apply matrix captured with all 11 DSP adds rated 🔥 BIG WIN / ✅ auto / Useful / Partial / N/A per-engine. Total Rounds 1-3 estimate ~5-8 sessions; Round 4 separate phase 3-5 more. Already-different Harmless Unison (T2-C S3 additive architecture) and Harmless mod-matrix envelope registry deliberately NOT cross-applied. Memory rule `feedback_cross_apply_phase_post_drums.md` saved alongside so future sessions auto-load the trigger (don't start until §P4 closes). Session D1–D11 §P3-CORE implementation SHIPPED earlier this date (all 11 DSP adds: Pitch Env / Sine / Noise-only / Free-Hz dual-osc / Transient Injector / Multi-burst / Hard Sync / Ring Mod / Pink+Brown noise / Analog Drift / Unison) across BaySickSynth + BaySickBass both engines, with 5-tab editor expanded to 6-tab layout (new MOD tab with 5-group grid: NOISE / TRANSIENT / BURST / DRIFT / UNISON). Wide tooltip fix landed (VibeLAF::getTooltipBounds 300→460px + TextLayout-based measurement). ~90+ authentic instrument recipes catalogued in §P3 Preset Recipe Catalogue spanning TR-analog drums, Simmons, Yamaha FM drums, tuned percussion, hand percussion, classic basses / leads / pads, keys/organ/EP, and SFX — catalogue becomes input for §P4.4 factory preset bank. Prior entry below.)

**Prior update:** 2026-04-21 (§P3-CORE EXPANDED from 6 → 11 DSP adds for full classic-synth authenticity beyond just drums. Sessions A/B/C of the §P3 implementation plan SHIPPED earlier this date: Session A (T1.2 setComponentID on all 18 attached sliders in both Synth + Bass editors; T2.2 new filter-type LED selector LP/HP/BP/Notch on Filter tab; T1.1 audit false-positive — flt_type read was already correct; bkb_→bsb_ APVTS prefix rename across source + build config + CLAUDE.md since it was stale inconsistent naming that needed aligning), Session B (T2.1 LFO tempo-sync shipped — new lfo_division choice param with 1/1..1/32 divisions, host BPM read via AudioPlayHead in processBlock, sync-aware effective-rate computed in updateFromApvts and stored in std::atomic<float> mEffectiveLfoRate so the BaySickVisualizerScreen animation tracks tempo-sync correctly; editor gained DIV combobox with attachment + grey/enable state toggle on APVTS listener for lfo_sync so rate knob greys out when sync on and DIV greys when sync off), Session C (T2.3 velAmpTrack float 0-1 default 1.0 - PRESET-SAFE matches current full-velocity behavior; T2.4 Legato voiceMode added as 4th option - manual MIDI dispatch in BaySickSynthDSP::handleLegatoMidi with last-note-priority held-note stack, new Voice::retargetLegato method re-pitches without env/LFO/filter/phase reset, mLegatoSynthNote tracks synth's view of which note voice plays so final noteOff matches, forces voice 0 reuse via stopNote(0,false) before synth.noteOn for deterministic legato voice allocation (fixed 2-and-2 ping-pong observed during pattern loops), 1ms declick fade-in via mDeclickGain ramp in every startNote to suppress click from hard legato termination; added !mInRelease guard to startNote glide condition so re-allocated release-tail voices don't glide from stale pitch; new VEL knob on OSC ENV tab + new SLIDE-awareness notes). Build-system recovery: do_build.bat path Vibesynth → BaySickDAW corrected (folder was renamed previously, CMakeCache regeneration required + new configure.bat created since PowerShell doesn't have MSVC env loaded; CLAUDE.md build-system section updated). **§P3-CORE expansion locked (2026-04-21):** P3.7 Hard Sync, P3.8 Ring Modulation, P3.9 Pink + Brown noise types (replaces white-only LCG), P3.10 Per-voice analog drift (small random pitch wander, Juno/Prophet warmth), P3.11 Unison mode (PROMOTED from T3.2). All 5 additional adds PRESET-SAFE with neutral defaults. Full capability map: Moog leads/bass ✅, Yamaha DX7 ✅ (Bell FM + pitch env + sine + transient), Roland Juno/Jupiter pads ✅, ARP 2600/Odyssey ✅, CS-80 brass ✅, 80s sync leads (Jump/Final Countdown) ✅ (needs P3.7), 303 acid ✅, supersaw trance ✅ (needs P3.11), 808/909/Simmons drums ✅, Rhodes/Wurli/Clavinet ✅, Hammond organ ✅, analog drift/warmth ✅ (needs P3.10). Deferred v1.1+: 6-op FM (DX7 authentic), Karplus-Strong plucked strings, wavetable morphing, formant vocoder filter, acoustic-piano PM. **UI layout plan Option C locked:** one new MOD tab absorbs transient + multi-burst + drift + unison + noise-only toggle. Existing tabs gain 1-3 new controls: OSC gets hard-sync LED + ring-mod LED + noise-color selector + free-Hz mode switch; OSC ENV gains pitch-env ADSR row alongside amp ADSR; FILTER/FLT ENV/LFO unchanged. Jeff flagged "number of knobs WAY oversized" for full editor-polish review after all 11 adds ship. Prior entry below.)

**Prior update:** 2026-04-21 (§P3 BaySickSynth Tier 2 SCOPE LOCKED + T3.7 dropped. All 6 Tier 2 items decided: T2.1 Ship tempo-sync DSP (PRESET-SAFE, default off = free-running), T2.2 Add filter-type selector LP/HP/BP/Notch (PRESET-SAFE, param already exists), T2.3 velocity→amp approved (PRESET-SAFE), T2.4 legato mode via new 4th voiceMode LED slot (PRESET-SAFE, not repurposing Lead), T2.5 all 6 §P3-CORE DSP adds approved for v1.0, T2.6 UI layout TBD at UI session. T3.7 filter-KB-track-as-cents/oct dropped — 0-1 amount is beginner-friendlier, no real user need, would confuse musicians. All other Tier 3 items (T3.1-T3.6, T3.8) remain parked post-v1.0. **Still open:** P3.1-P3.6 design locks (per-DSP-add questions like "pitch env bipolar vs unipolar," "sine waveform insert vs append," "noise-only semantics," etc.) — to be answered immediately before each respective implementation session, one at a time. Next session = begin implementation, starting with all 4 Tier 1 items (flt_type read + componentID + attachment sweep + audit-commit already done) then T2.2 filter selector UI then T2.1 tempo-sync DSP. Prior entry below.)

**Prior update:** 2026-04-21 (§P3 BaySickSynth SLA Audit COMPLETE (planning-only, no code) + §P4 DrumsPage Dual-Engine Restructure created. Per-element audit against `Files For Claude/Player Layouts/BaySickSynth & BaySickBass.txt` + Source/BaySickSynth/*: 26 APVTS params, 24 fully wired (APVTS ↔ Editor ↔ DSP), range-mapping audit clean across all 24 (no Treble-style bug), CPU guards consistent via cache-compare pattern in updateFromApvts. **2 broken params identified:** `bss_flt_type` (APVTS registered + setter exists in DSP, but updateFromApvts never reads → filter stuck on LowPass; no UI control visible either — T1.1 to fix read, T2.2 to decide UI) and `bss_lfo_sync` (param + button attached, but never read from APVTS and no DSP setter — T2.1 decides remove-vs-ship-tempo-sync). **componentID gap** (zero setComponentID calls on editor sliders — same gap §P2 S3 fixed for VibePlayer, now T1.2 for Synth + mirror for Bass). **cross-apply:** BaySickSynth + BaySickBass share `BaySickSynthDSP` + `BaySickSynthVoice`, so all DSP findings 2-for-1; editor UI is separate (bkb_ prefix, green LAF). **§P3-CORE 6 DSP additions** (blueprint lines 742-751, all PRESET-SAFE per blueprint guarantee): Pitch Envelope (P3.1), Sine waveform (P3.2), Noise-only mode (P3.3), Free-Hz dual-osc tuning (P3.4), Transient injector (P3.5), Multi-burst envelope (P3.6). None implemented; design-spec ready. These are the critical drum-engine payload — unlock authentic TR-808/909, Simmons, Yamaha FM, Korg analog drum synthesis inside DrumsPage slots (see lines 806-817 "big door" capability map). **Tier 1 auto-done** (4 items): flt_type read, componentID pass, attachment sweep, audit commit. **Tier 2 ask** (6 items, T2.1-T2.6): lfo_sync decision, flt_type UI decision, velocity→amp improvement, legato mode question, v1.0-vs-v1.1 scope for 6 adds, UI layout question. **Tier 3 future** (8 items, T3.1-T3.8): oversampling, unison, anti-click, per-note portamento, aftertouch, waveform morph, KB-track cents/oct, LFO sync re-promotion-if-removed. **§P4 DrumsPage created** — absorbs drum-page work originally mislabeled as §P2 in ship-order lines 823-824 (lines struck-through with redirect note, pure append rule honored). §P4.1 dual-engine slot dropdown (PRESET-BREAK, v1), §P4.2 dual piano roll mode from line 838 (PRESET-BREAK, v1), §P4.3 per-slot EQ rebind from line 967 (PRESET-SAFE, pre-EQ hook), §P4.4 factory drum preset bank (PRESET-SAFE, additive). §P4 depends on §P3 closing first — synth must be drum-capable before drum page hosts synth slots. Session carried forward to Jeff's tier confirmation before any code. Prior entry below.)

**Prior update:** 2026-04-20 (§P1 Harmless S4 SHIPPED - mod matrix + global LFO + mod editor tools. 6 implementation batches over 2 sessions. **Batch 1 foundation:** new HarmlessModRegistry class with ModSource/ModTab/ModTargetIndex enums + ModCurveState/ModSourceState/ModTarget structs; ValueTree serialize/deserialize under <harmlessMod> child; 16 articulation targets registered at processor startup with baked-in paramIds. APVTS rip: 8 old params removed (lfo_rate, lfo_shape, lfo_vel, lfo_vol, lfo_pitch, mod_x_dest, mod_y_dest, mod_z_dest) - PRESET-BREAK ⚠ pre-v1. AG-1 auto-gain added as auto_gain_mode Int APVTS with REL/ABS toggle in Timbre panel + DSP pass in HarmonicEngine::buildWavetable. **Batch 2a Option 2 architecture:** each AdditiveVoice gets its own private HarmonicEngine pair pre-allocated in setCurrentPlaybackSampleRate; activeEngineA/B() picks between shared template + voice copy. ~1 MB memory bump. **Batch 2b articulator DSP:** HarmlessModCurve utility with SKEW/PW/TNS warps + LFO LUT + toBipolar/unipolar helpers; per-voice TargetVoiceState (envPhase/lfoPhase/capturedVel/capturedKey/contribution/uniMult); note-on snapshot + per-block updateModContributions summing 7 sources. VoiceLocal dispatch for 11 targets (sample-rate apply); VoiceEngine dispatch for 3 wavetable-rebuild targets (Pluck/Prism/Blur) via applyVoiceEngineMods; SynthLevel dispatch for 2 output-phaser targets via loudest-voice-wins aggregation. Volume + Part A/B Level use UNIPOLAR mapping; other targets bipolar. **Batch 3 UI rewrite:** HarmlessModEditor rebuilt around the registry. Articulations + Modulations dropdowns; DEPTH (bipolar center-detent); LENGTH (13-step musical 1/8..32 beats, skew-midpoint at 1 beat); TEMPO toggle; SHAPE rotary (LFO-only); SPD/TNS/SKEW/PW warp knobs; grid drawing routes through HarmlessModCurve::sample so warps visually reflect; LFO default 2-point curve renders selected waveform, 3+ points renders user-drawn curve. IMG tab moved to T3. GLOBAL toggle moved to T3. **Batch 4 right-click + global LFO + orphan cleanup:** VKnobAutomation::sShouldOfferModulate + sOnModulateEnvelope hooks in SharedUI; GlobalAutoRightClick offers "Modulate envelope..." when paramId is registered. Global LFO re-added: lfo_rate/lfo_shape/lfo_tempo APVTS -> RATE/SHAPE/TEMPO knobs in main editor; any change triggers HarmlessSynth::applyGlobalLfoToAllTargets which macro-writes to every target's LFO source. Per-target override still works via mod editor. Orphan XYZ dropdowns + old per-destination LFO knobs removed. Stale WIRES IN tooltips cleaned. HarmlessModCurve::applySkew upper-clamp bug fixed (was clamped at 1.0 killing all skew > 0.5). WYSIWYG envelope with amp-ADSR-release-time release phase advance. **Batch 5 mod editor tools:** CURVE/STEP radio (new-point curveType); SNAP toggle; shift-axis-snap anchored at drag-start position; double-click-point curveType cycle (linear->smooth->step); Ctrl+Z/Y undo+redo with 100-frame UndoFrame history; FREEZE toggle locks edits; +/- zoom (1..8x) with zoom-aware pointToPixel/pixelToPoint + segment-aware drawCurve that emits each point as exact path vertex. Division dropdown (1/1, 1/2, 1/4, 1/8, 1/16, 1/32) sets snap target; grid always 32 divisions. Horizontal ScrollBar below grid. Endpoint anchor protection prevents deletion of leftmost/rightmost points. Sanitize pass on mouseUp / target focus / dropdown change sorts + nudges duplicate-time points apart by 0.002. **Tally:** 4 APVTS added (auto_gain_mode, lfo_rate, lfo_shape, lfo_tempo); 8 removed; 1 new class + 2 new files; ~2500 lines new + ~1000 lines rewrites. 7 new T3 entries: T3-PerPartModRouting, T3-HarmlessADV, T3-ModMatrixAutomation, T3-NoteDurationAwareEnvelopes, T3-PerPointSustain, T3-HarmlessImgTab, T3-RealTimeAdditive. After S4, §P1 progress: ~80 elements wired. Next session = S5 (central 516-partial spectrogram + background wavetable rebuild). Then §P1 layout review pass. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a7P1 Harmless S3.5 SHIPPED - per-part wavetable-domain split + functional A/B switch. After S3 left the A/B buttons as a non-functional visual toggle, Jeff reframed the design call: "while the target is beginner friendly I am trying to encourage learning how things like this are actually done so wouldn't adding it in make more sense if the point is to give them the tools to learn these things". Per-part architecture now genuinely teaches the Harmor-style two-engines-in-one concept. **Architecture:** the wavetable-domain spectral chain (Brownian / Blur size+time+harm / Prism amount+mode / Pluck decay+blur / Filter mask / Phaser mask) splits per-part because mPartA + mPartB are already independent HarmonicEngine instances - just driven by the same APVTS reads pre-S3.5. The signal-domain effects (Filter 1/2 + envs, Amp ADSR, Tremolo, Vibrato, LFO, Mod XYZ, Pitch, Strum, Glide, output phaser/EQ, routing matrix) stay shared because there's only one filter chain / ADSR / etc per voice. This split tells a real story: spectral chains can run per-engine; signal-domain effects run per-voice on the summed output. **APVTS:** 10 new partB_* params added with defaults matching the existing param defaults so first-load behaviour is unchanged: partB_brownian_amount, partB_blur_size, partB_blur_time, partB_blur_harm, partB_prism_amount (-1..+1 per S2 bipolar), partB_prism_mode, partB_pluck_decay, partB_pluck_blur (bool), partB_filter_mask_cutoff, partB_phaser_mask_rate. All PRESET-SAFE additive. **DSP split:** 6 existing combined setters on HarmlessSynth (setBrownianAmount / setPluckDecay / setBlurSize / setFilterMaskAmount / setPhaserMaskRate / setPrismAmount) + 3 newer ones (setPrismMode / setPluckBlur / setBlurTime / setBlurHarm) gain matching A and B suffixed variants. Old combined setters kept as backward-compat noop wrappers (still used by no-one outside this file now). HarmlessProcessor::updateFromApvts switched its existing reads from setXxx to setXxxA, then 10 new reads at the end drive setXxxB. **Editor attachment swap:** new `DualSliderPart` and `DualButtonPart` structs hold (slider/button + paramA + paramB + current attachment unique_ptr). Constructor builds 8 dual-slider entries + 1 dual-button entry covering the per-part-able controls; existing direct attachments (mBrownianAtt / mBlurSizeAtt / mPrismAmtAtt / mPrismModeAtt / mPluckDecayAtt / mPhaserMaskRateAtt / mPluckBlurAtt / mBlurTimeAtt / mBlurHarmAtt) replaced with juce::ignoreUnused stubs - the dual-attachment system owns the live attachment now. New `rebindToPart(int)` method destroys + recreates each attachment pointing at the chosen part's paramId; slider values auto-sync via JUCE's bidirectional attachment behaviour. Part A/B button onClicks now call rebindToPart(0/1) in addition to writing part_sel APVTS state. Initial bind on editor construction reads part_sel and seeds mActivePart accordingly. **What it does for users:** dial in a Brownian + Blur + Prism setting on Part A; click Part B button; the same visible knobs now show + edit Part B's independent values; click Part A again; original A values come back. With both Part A Level + Part B Level non-zero (set via the dedicated A/B level knobs which stay bound directly), the user hears both engines summed - and can A/B between two different timbre setups in real-time without losing either. **Tally:** 10 new APVTS params, 18 new HarmlessSynth setter methods, ~30 new editor lines for the dual-attachment system. PRESET-SAFE additive throughout. After S3.5, §P1 progress: 25 (S1) + 8 (SLA-Impl) + 18 (S2) + 6 (S3) + 10 (S3.5) = 67 elements wired. Remaining: S4 (mod-editor 4 tabs real + right-click-to-modulate paradigm + target dropdown + tool buttons + modifier knobs + TEMPO/GLOBAL + viewport tools - PRESET-BREAK pre-v1) + S5 (central 516-partial spectrogram + background wavetable rebuild). Prior entry below.)

**Prior update:** 2026-04-19 (\u00a7P1 Harmless S3 SHIPPED - full Unison engine + Part B shape interactivity + vel_link DSP + part_sel A/B button toggle. **T2-C Unison Type / Alt / Phase:** AdditiveVoice's recalcUnisonSlots refactored to honour mUnisonType (4 modes: 0 Pure linear symmetric detune, 1 Random per-slot bias via mRng, 2 Drifting linear + small static sine offset for "alive" stack feel, 3 Alt-only alternating signs without centre voice). mUnisonAlt superimposes alternating sign on top of any base mode. New setUnisonType / setUnisonAlt / setUnisonPhase setters on AdditiveVoice; mUnisonPhaseAmount drives per-slot phase stagger applied at startNote (amount=0 -> all slots use base random; amount=1 -> slots evenly spread across full wavetable). Wired through HarmlessSynth via existing forEachVoice + new APVTS reads in updateFromApvts. Existing unison_type/unison_alt/unison_phase ghost params from S1 finally do something. **T2-N vel_link DSP:** mVolSmooth refactored - no longer pre-multiplies velocity. New mNoteVelocity field tracks per-voice velocity. Per-sample render multiplies Part A by velocity always; Part B by velocity only when mVelLink is true (default), otherwise B uses 1.0 (handy for layered tones with constant-amplitude underneath). HarmlessSynth::setVelLink now actually broadcasts to voices (was a stored-but-never-propagated stub from S1). **mTimbreWavB interactivity:** new partB_timbre_shape APVTS Choice (Sine/Saw/Square/Triangle, default Square). Was previously hardcoded in HarmlessSynth ctor. New mPartBShapeSlider hidden child component with SliderAttachment; mTimbreWavB.onChange writes to it (mirrors the existing Part A wiring); HarmlessSynth::setPartBShape converts the int to HarmonicEngine::Shape and rebuilds Part B's wavetable. **part_sel button toggle:** mPartABtn / mPartBBtn now act as a 2-way radio group writing to part_sel APVTS int param. onClick handlers push 0/1 + mutually-exclusive setToggleState; initial state synced from APVTS. Knob-rerouting (which Part the timbre knobs control) deferred to a future polish session - this is just the visual + state-tracking foundation. **Tally:** 1 new APVTS param (partB_timbre_shape), ~120 lines DSP across AdditiveVoice + HarmlessSynth, ~50 lines editor wiring. All PRESET-SAFE (additive on partB_shape; refactor on velocity is internally-equivalent). Build warning fixes: float-cast literals in HarmlessSynth tilt-EQ rebuild + cleaner "step" comment in applyStrum. After S3, §P1 progress: 25 (S1) + 8 (SLA-Impl) + 18 (S2) + 6 (S3) = 57 elements wired. Remaining sessions: S4 (mod-editor 4 tabs real + right-click-to-modulate paradigm + target dropdown + tool buttons + modifier knobs + TEMPO/GLOBAL + viewport tools - PRESET-BREAK pre-v1) + S5 (central 516-partial spectrogram + background wavetable rebuild). Prior entry below.)

**Prior update:** 2026-04-19 (\u00a7P1 Harmless S2 SHIPPED - filter envelopes + LFO routing + Mod XYZ destinations + bundled SLA wires (Blur time/harm + Filter ofs per filter + Prism bipolar). Big DSP session. **T2-A Filter envelopes:** per-voice juce::ADSR mFltADSR1 + mFltADSR2 + setFilter1Env/setFilter2Env (a,d,s,r) + setFilter1EnvAmt/setFilter2EnvAmt (-1..+1 scaler). ADSRs trigger noteOn/noteOff alongside amp ADSR. Per-block effective cutoff = baseCutoff * 2^((envAmt*envValue + cutoffOfs/12 + kbTrack*noteSemisAbove60 + modCutoffSemis)*octaveScale). 4-octave swing on full env amount. Resolves the long-standing TODO comment in AdditiveVoice.h. Also wires the existing flt_a/d/s/r + flt_env_amt + flt2_a/d/s/r + flt2_env_amt APVTS params (S1 ghosts). **SLA #34 Filter ofs:** new flt1_cutoff_ofs + flt2_cutoff_ofs APVTS params (-24..+24 semitones each). Combines with kb_track and env to produce the effective per-voice cutoff. **T2-N flt2_kb_track DSP:** keyboard tracking depth (0..1) wires alongside the new ofs - 0 = cutoff fixed, 1 = full keyboard tracking using noteSemisAbove60. Same for flt1_kb_track (was registered in S1 but DSP-pending). **T2-B LFO:** new lfo_rate (Hz) + lfo_shape (int 0-3) APVTS params + per-voice mLfoPhase. setLfoParams + setLfoDepths broadcast through HarmlessSynth. Per-sample LFO value adds to pitch (lfoVal * lfoDepthPitch * 12 semis = +/- 1 octave at full depth) and multiplies the gain stage (1 + lfoVal * lfoDepthVol * 0.5 = +/- 0.5 swing). lfoDepthVel scaffolded but unused per-sample (velocity is one-shot). UI: new mLfoRate rotary + mLfoShape chicken-head selector added to the LFO panel; existing 3 vertical depth sliders kept. **T2-E Mod XYZ destinations:** new mod_x_dest / mod_y_dest / mod_z_dest int APVTS params (0..6) selecting modulation target: 0=Off, 1=FilterCutoff, 2=Pitch, 3=Volume, 4=Pan, 5=PartBlend, 6=Prism. Per-block accumulator for FilterCutoff (+/- 2 octaves at full mod), Pitch (+/- 1 octave), Volume (+/- 0.5x). Pan/PartBlend/Prism scaffolded but not yet applied per-sample (S3+ will close those). UI: 3 ComboBox dropdowns added under the XYZ pad (Off, Cutoff, Pitch, Volume, Pan, Part A/B, Prism) with ComboBoxAttachment. **SLA #14 Prism bipolar:** prism_amount range changed from 0..1 to -1..+1 with default 0 (PRESET-BREAK pre-v1; existing presets would only cover the +0..+1 half). Sign encodes polarity for the spectral spread shape - PrismModule's getFrequencyShift respects the sign through the existing math. **SLA #8 Blur time:** new blur_time APVTS (0..2, default 1) + BlurModule::setTimeScale - multiplies the kernel half-width. **SLA #9 Blur harm:** new blur_harm APVTS (0..1, default 0) + BlurModule::setHarmAxis - at >0.5 the box-blur kernel steps by 2 instead of 1, smearing only harmonic-related partials. UI: new mBlurTime + mBlurHarm rotaries added to BLUR/PRISM section. **T2-N legato_limit DSP:** caps effective glide_time. setGlide stores the user value; setLegatoLimit re-broadcasts the clamped effective glide to all voices on change. **T2-N vel_link:** scaffolded only - in our current architecture both Part A + B share the same velocity-scaled vol so vel_link has no audible effect today; flag for S3 per-part velocity routing. **T2-N part_sel:** editor-side selector deferred to S3 (UI swap of which part the timbre knobs route to). All 18 new APVTS params (PRESET-BREAK on prism_amount only, all others PRESET-SAFE additive) + 8 new UI controls + 5 new chicken-head/rotary attachments + Mod XYZ ComboBoxes + bulk wireMeta tooltips + setPopupDisplayEnabled inherited via setupRotary helper. Section labels + knob labels added for new controls. **Tally:** 18 new APVTS params (1 PRESET-BREAK, 17 PRESET-SAFE), 7 new UI knobs + 1 chicken-head + 3 ComboBoxes, ~150 new lines DSP, ~60 new lines UI. Filter ADSR knob UI deferred to S4 (lives in mod editor per design). Prior entry below.)

**Prior update:** 2026-04-19 (\u00a7P1 Harmless SLA-Impl SHIPPED - Pitch group + Phaser WIDTH/OFS + Pluck blur button. Smallest of the wire sessions per the locked SLA plan. **DSP additions:** new `pitch_freq_frac` Int 0..6 APVTS param + `setPitchFraction(int)` on HarmlessSynth + AdditiveVoice; static lookup table `kFracTable[7] = {1, 0.5, 0.25, 0.125, 2, 4, 8}` maps the 7 chicken-head positions; `mPitchFracMult` applied in renderNextBlock as `mNoteHz * mPitchFracMult * pow(2, semitones/12)` so the fraction multiplies the fundamental BEFORE pitch_semitones+cents stack on top. New `ophaser_width` Float (0..0.95, default 0.5) + `ophaser_ofs` Float (200..2000 Hz log-skew, default 1000) APVTS params + `setOutputPhaserExtras(width, ofs)` on HarmlessSynth wiring to JUCE phaser's `setFeedback` + `setCentreFrequency`. **UI additions:** existing `pitch_semitones` + `pitch_cents` ghost params (DSP wired since S1 but never had UI) now get visible knobs labeled FREQ + DETUNE in a new PITCH section between BLUR/PRISM and TREMOLO in the top-left panel. Section heights redistributed (0.20/0.18/0.14/0.13/0.14/0.21). New chicken-head `mPitchFreqFrac` selector + UI-only `mPitchOctBtn` + `mPitchHzBtn` toggles (display-mode togglers per spec, no DSP). Phaser WIDTH + OFS knobs added to FX row in top-right (between Rate and MaskRate). Pluck `BLUR` TextButton added next to Pluck Decay knob with ButtonAttachment to existing `pluck_blur` bool param (DSP shipped S1; UI was missing). All 5 new attachments + 4 chicken-head variant flag set + 8 new wireMeta tooltip+componentID entries + setPopupDisplayEnabled inherited via setupRotary helper. Section header "PITCH" added to drawSection list; FREQ/DETUNE/FRAC knob labels + WIDTH/OFS knob labels added to the paint label loop. **Tally:** 1 new APVTS int + 2 new APVTS floats = 3 new params (all PRESET-SAFE additive) + 8 new UI controls + 0 deferred. SLA-Impl session complete; next session = S2 (filter envelopes + LFO routing + Mod XYZ destinations + bundled SLA items: Blur time/harm + Filter ofs per filter + Prism bipolar conversion). Prior entry below.)

**Prior update:** 2026-04-19 (\u00a7P1 Harmless SLA Audit COMPLETE + Player/Page Audit Workflow established as cross-cutting convention. Per-element review of all 66 documented controls in Files For Claude/Player Layouts/Harmless.txt against current code, with Jeff's per-item decisions locked. Outcome: 16 WIRE (spread across SLA-Impl + S2 + S3 + S4) + 11 DROP (PRESET-SAFE; removed entirely from v1 scope) + 7 promoted to TIER 3 (Harmonizer module already there + Legato curve toggles + Filter osc knob + Filter secondary shape dropdown + Pluck curve-shape toggle + Phaser shape dropdown + EQ shape dropdown - all PRESET-SAFE additive for v1.1+) + 25 already shipped (S1) + 7 already planned across existing S2/S3/S4/S5. Three open questions resolved: (1) #14 Prism amount range goes bipolar -1..+1 (PRESET-BREAK pre-v1, ships in S2; sign encodes polarity, +/- toggles dropped), (2) #19 Pitch fraction selector wires as chicken-head with `1/1 / 1/2 / 1/4 / 1/8 / x2 / x4 / x8` ratios (SLA-Impl), (3) #59 Info Bar DROPPED per Jeff "remove as confusing" - live-value popups cover the use case. Updated session scopes: SLA-Impl (next, ~1 sitting) = Pitch group full + Phaser WIDTH + OFS + Pluck blur UI button + 8 new APVTS params + light DSP, all PRESET-SAFE; S2 absorbs Blur time/harm + Filter ofs per filter + Prism bipolar conversion alongside the existing T2-A/B/E filter envelope + LFO + XYZ work; S3/S4/S5 unchanged.

**Convention established (Cross-cutting / Architecture section):** new `Player + Page Audit Workflow (the SLA pattern)` subsection documents the per-element audit format for reviewing every remaining player engine (BaySickSynth / BaySickBass / VibePlayer / BaySickDrums) AND every system page (Mixer / Builder / Effects / DrumsPage / LayersPage / BassPage / etc.) against their design docs. 9-step recipe: read design doc + .png screenshot, read current code (delegate to subagent if scope large), enumerate every documented control numbered, classify HAVE / PARTIAL / MISSING, build per-element table with `# / Element / Current / Proposal / PRESET / Session` columns, tally counts, ask explicit open questions, wait for user confirmation, commit to Blueprint with per-row provenance. Audit session is planning-only - zero code changes. Implementation lands in subsequent sessions. Memory rule `feedback_player_page_audit_workflow.md` saved alongside so the pattern auto-loads in future sessions. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a7P1 Harmless visual pass v2 - Time-style continuous knobs + standard fader-bar with under-glow + chicken-heads ONLY on multi-selectors. Per Jeff: "you made every knob a chicken head which is not what I want as I want multi selectors to be chicken heads but I still want knobs else where and those knobs should be the knob from the time effects LAF but with our orange color filing in the white lines on the knob and underneath it. Second the faders now work as faders but there is no fader bar like on every other fader in the app and we have no under glow to help show where the fader is." **Knob fix:** HarmlessLAF::drawRotarySlider refactored to dispatch on a new `kKnobVariant` property. Default (continuous knob) renders the Time-effects filmstrip (Filmstrips::timeBased() = 64x64, 101 frames, dark matte cylinder + 8-fluted star grip + white indicator) with two orange overlays: (a) an indicator line in `kAccent` (#FF6600) drawn at the value angle on top of the white filmstrip indicator so the white pixels are dominated by orange + soft amber halo, and (b) an arc-glow ring at radius 1.05x the knob drawn behind the knob showing value position bipolar-aware. "chickenHead" variant routes to the existing chicken-head pointer (extracted into drawChickenHeadKnob). Editor marks mPrismMode + mUnisonType + mStrumDirSlider as chickenHead per their discrete-mode-picker semantics. **Fader fix:** HarmlessLAF::drawLinearSlider switched from custom rectangular cap to the standard Filmstrips::fader() filmstrip (128x128, 31 frames - same fader bar visual VibeLAF uses for non-mixer/non-EQ vertical faders) plus an amber under-glow gradient from cap-bottom to track-bottom (same kAccent gradient pattern). Fallback path (filmstrip unavailable) draws a slim track + amber-glowing thumb-cap so the visual still works. Both LinearVertical and LinearBarVertical handled. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a7P1 Harmless Bug-L1 fix + live-value popups. **Bug-L1: Legato ON silenced new noteOns.** Symptom: clicking LEGATO on, the held note finished naturally, then every subsequent noteOn was silent until LEGATO was clicked off. Root cause: AdditiveVoice startNote skipped `mAmpADSR.noteOn()` whenever the voice was active, but `isVoiceActive()` returns true throughout the release tail too (until clearCurrentNote fires). So a fresh noteOn arriving while the previous note was still in release re-used the same voice, skipped the ADSR retrigger, and the envelope stayed stuck in release -> instant fade-out -> silence. Fix: new `mInRelease` bool flag set true in stopNote(allowTailOff=true), false in startNote (on actual retrigger) + stopNote(noTailOff). Legato re-trigger gate now reads `wasSustained = wasActive && !mInRelease` instead of just `wasActive` - so legato only suppresses ADSR retrigger when the previous note is still HELD (sustaining), and a release-tail re-trigger does normal noteOn(). Same gate also applied to the glide-vs-snap decision so portamento only kicks in on truly held re-triggers. **Live-value popups.** Per Jeff: "tooltips don't show current amount when hovered like the effects so I have to type in value to find out". Effects-panel VKnobs use `setPopupDisplayEnabled(true, true, nullptr)` to show the current numeric value as a small popup on hover + drag. Added the same call to setupRotary + setupVertical (HarmlessEditor) + makeRotary (HarmlessFilterRow) + the routing-matrix slider init - all Harmless sliders now show their live value on hover, matching the effects-panel pattern. Static tooltip text from S1 still wires the param-name + units description; the popup adds the live value on top. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a7P1 Harmless S1 visual followup - chicken-head knobs + thumbed faders + LinearVertical sweep. Two visual issues from S1: (1) routing-matrix sliders rendered as "colored line with no fader" because LinearBarVertical (the previous style) draws only a fill and no thumb cap - against the dark Harmless chassis the fill barely shows. (2) Rotary knobs across the entire editor "appear to be chicken heads but they are normal knobs" - the original ring-glow design only drew a thin pointer tick on a dark cap that was nearly invisible against the dark background. Per Jeff's "should follow the same display setup on the chicken heads in the effects panels" - both fixes target visual parity with the existing effects-panel chicken heads. **Knob fix:** HarmlessLAF::drawRotarySlider replaced with a composite render - vector-drawn chicken-head pointer (hex base + rotating beak with counterweight arc, ported from DynamicsLAF::drawChickenHead but adapted for the dark Harmless palette: dark grey body + bright orange tip line + amber edge highlight) layered on top of the original orange ring-glow value arc. Smooth rotation (vector, not 10-frame filmstrip), unmistakable amber tip indicator visible against the kChassis black, matches the chicken-head visual character users expect. Background track tinted brighter (0xFF3A2A18 vs the old kInactive 0xFF332211) so the inactive arc itself reads on the chassis. **Fader fix:** HarmlessLAF::drawLinearSlider now handles juce::Slider::LinearVertical (in addition to LinearBarVertical for back-compat) - thin recessed track + amber fill from bottom to thumb position + wide metallic thumb cap with amber accent line across the centre. Routing matrix slider style switched from LinearBarVertical to LinearVertical; the editor's setupVertical helper made the same switch so the Unison Pan/Pitch/Phase + LFO Vel/Vol/Pitch sliders all render with the new thumb cap. HarmlessLAF.h now includes ../Standalone/SharedUI.h for the Filmstrips namespace (forward-prep for future filmstrip-based variants). **Layout-gap session deferred:** Jeff also flagged "knobs pushed to the side with huge open space as though we are missing things" - per the Harmor reference + Player Layouts/Harmless.txt design doc the current build is missing roughly 30+ documented UI elements (Fade slider in Timbre group, Blur time/harm knobs, Prism +/- toggles, Pitch freq/detune number inputs + oct/Hz toggles, Legato curve toggles, Filter row extensions: width x2 + ofs + osc + 6 toggles per filter, Pluck toggles, Phaser dropdown + WIDTH + OFS + 4 toggles, EQ dropdown, Info Bar tooltip surface, Mod editor tool buttons + SPD/TNS/SKEW/PW knobs + TEMPO/GLOBAL radios + viewport tools). New dedicated **Session SLA (Layout Audit)** added to §P1 plan - per-element wire-or-remove audit comparing design doc to current state with PRESET-SAFE flags throughout. SLA scheduled to run BEFORE S2 (filter envelopes / LFO routing / Mod XYZ destinations) since several of the missing controls would affect what S2 wires into. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a7P1 Harmless Session 1 shipped - bugs + small wires + removals + Tier 3 demote. Comprehensive ghost-param sweep: ~30+ APVTS params that were registered but had zero DSP wiring caused widespread "I turn this knob and nothing happens" UX bugs - design intent from `Files For Claude/Harmless/*.txt` mostly unimplemented. S1 closed the smallest gaps + removed deferred features so no ghost UI/params remain. **Tier 1 bugs:** T1a oeq_mix wired (param registered + new tilt-EQ DSP); T1b flt2_kb_track param registered (was missing - editor attachment would have jasserted); T1c filter type combo wired to DSP (LP/HP/BP/Notch all functional - notch via BP-subtract through new mFilterNotchHelper instances); T1d setComponentID on every Harmless slider (was zero across the folder - GlobalAutoRightClick filtered every knob out); T1e tooltips on every Harmless slider with ASCII-only descriptions + WIRES IN Sx flags for deferred items; T1f CPU guards added to setTremoloParams/setVibratoParams/setPhaseInit/setPitchOffset on AdditiveVoice; T1g removed Time::getHighResolutionTicks() syscall from AdditiveVoice::startNote (replaced with per-voice mRng seeded once on message thread); T1h mono playback safety confirmed. **Tier 2 wires:** T2-D prism_mode (3 inharmonic-spread shapes via new PrismModule::setMode); T2-H phaser_mask_rate UI knob added to FX row; T2-N misc ghost params (timbre_blend A<->B crossfade, pluck_blur via PluckModule::setBlur, strum_tns via tension curve in applyStrum, vel_link + legato_limit scaffolded for S2/S3); T2-O layout audit; T2-F Routing Matrix wired with 6 new APVTS params (rm_sub octave-down sine, rm_prot nyquist rolloff, rm_clip output tanh, rm_fx wavetable-FX scale, rm_vol output gain trim, rm_env amp-env depth) + Harmor-equivalent DSP semantics; sub osc + env depth in AdditiveVoice; vol + clip in HarmlessSynth post-voice; fx + prot in HarmonicEngine wavetable-build via setSpectralFxScale snapshot+restore + setNyquistProtect; the 6 LED toggles dropped per Jeff's "no LED needed" direction. **Tier 2 removals:** T2-I reverb_amount param (rack handles reverb); T2-J multi-band comp confirmed not present; T2-G Harmonizer (6 harm_* params dropped + module demoted to Tier 3 - design-doc panel was never built). **Tier 3 entries logged:** T3-Harm Harmonizer module, T3-Img image resynthesis, T3-Aud audio resynthesis, T3-Reorder dynamic spectral unit-order, T3-9Voice expansion if T2-C ships smaller, T3-Curve editor enhancements. New memory rule `feedback_harmless_ghost_params.md` saved to capture the ghost-param antipattern + remediation pattern. Sessions S2-S5 outlined for the remaining bigger wires (filter envelopes, LFO routing, Mod XYZ destinations, full unison engine, mod-editor 4-tab + right-click-to-modulate paradigm, central spectrogram, background wavetable rebuild). Prior entry below.)

**Prior update:** 2026-04-19 (EQ default-frequency arrays unified across DSP and widget. Two arrays had drifted apart: EQ8DSP.cpp's kDefaultFreqs8 = 80/200/500/2k/6k/16k/8k/4k vs SharedUI.cpp's kEQDefaultFreqs = 40/250/500/1k/2k/4k/8k/12k (used for widget reset, double-click return values, and Reset Band). After the prior round's saveToSpare() seed in EQ8MsDSP ctor, spare captured the DSP-side defaults at construction time (before APVTS write-back populated DSP main with widget defaults), so swapping to B showed DSP's old freqs (80/200/...) which differed from A's freqs (40/250/...) - and double-clicking any knob on B reset to A's defaults via the widget's setDoubleClickReturnValue. Fix: aligned EQ8DSP.cpp's kDefaultFreqs8 to match SharedUI.cpp's kEQDefaultFreqs (40/250/500/1k/2k/4k/8k/12k). Both arrays now must stay in sync; comment in EQ8DSP.cpp flags this dependency. PRESET-SAFE (existing presets serialise their own band freqs - only fresh-state defaults change). Prior entry below.)

**Prior update:** 2026-04-19 (Spare bank channel routing seed fix. Bug: when swapping to B on the mid tab, all 8 bands defaulted to Channel=Stereo instead of Mid - because EQ8MsDSP's constructor only seeded the MAIN bank's per-band channels (mMid bands -> Channel=Mid, mSide bands -> Channel=Side) but never seeded the spare. Spare bank inherited the raw Band struct default (Stereo), so first compare flip on a mid tab silently rerouted all bands away from M/S. Fix: EQ8MsDSP constructor now calls saveToSpare() on both inner EQs after seeding the main channels - spare = main = correct channel routing for that side. PRESET-SAFE (initial spare state only; existing presets serialise their own spare state via getStateInformation/setStateInformation). Prior entry below.)

**Prior update:** 2026-04-19 (A/B compare APVTS write-back fix - architectural mismatch was overwriting swaps. Bug: APVTS holds a single set of band params (no spare-bank concept), but the compare bank lives DSP-side only. For full-bindMsDSP EQs (all bus EQs), processBlock constantly reads APVTS -> DSP via updateXxxEQ. After a swap, DSP held the new bank but APVTS still held the OLD bank's values, so the next processBlock pass overwrote the just-swapped DSP state with the OLD APVTS values - exactly the symptom Jeff reported ("swap shows briefly then immediately rewrites"). Fix: triggerCompare's MsDSP branch now pushes the just-swapped-in DSP state of BOTH inner EQs (mid + side) back into BOTH APVTS prefixes after every swap. Existing setAPVTSFromBand only writes the side currently being VIEWED (mid OR side based on mShowMid), so a new helper pushInnerDSPBandsToAPVTS reads each inner EQ's bands directly via getBand and writes all 18 per-band suffixes (Freq/Gain/Q/Type/On/Slope/Mute/Solo/Channel + 12j dynamic Threshold/Ratio/Attack/Release/Range/Upward + ScSource) to the right prefix. Both inner EQs (mid + side) swap together as part of the same compare flip, so APVTS sync covers both prefixes. APVTS-only mode (no DSP) gets the same treatment for consistency. Pure DSP-bind mode (no APVTS) doesn't need the fix - no APVTS write-back loop. Copy A -> B unaffected: it doesn't change which bank is viewed, so APVTS stays consistent. Prior entry below.)

**Prior update:** 2026-04-19 (Bank indicator positioning + duplicate-stacking fix. Two bugs from the prior round: (1) bank indicator landed in PageMenuBar's right-extras cluster which on EffectsPage put it FAR right of the mixer-strip dropdown rather than adjacent to MID/SIDE as Jeff requested. (2) Repeated EQ-tab clicks called `addExtraRightComponent` again without removal so the indicator stacked, expanding the bar leftward and eventually pushing the right-side widgets behind tabs/menu making them unusable. Fix: new dedicated `setBankIndicator(juce::Component*)` slot in PageMenuBar with a single non-owning pointer field. setBankIndicator is idempotent (same-pointer no-op) so repeat tab clicks can't stack. Layout in PageMenuBar::resized() now slots the indicator IMMEDIATELY after the MID/SIDE buttons (and before the right-extras cluster) which is exactly where Jeff wanted it. syncEQHamburger in StandaloneEditor swapped from addExtraRightComponent/removeExtraRightComponent to setBankIndicator(eq ? eq->getBankIndicator() : nullptr). Page-activation cleanup at the top of the if-mPageMenuBar block also calls setBankIndicator(nullptr) so it doesn't leak across page switches alongside the existing setMenuBuilder + clearTabSlots + clearExtraRightComponents calls. Prior entry below.)

**Prior update:** 2026-04-19 (A/B compare overhaul + bank indicator + EQ display toolbar row reclaimed. Existing compare logic had a destructive bug: triggerCompare auto-saved current bank to spare BEFORE swapping, which (a) made the first flip imperceptible since spare was just cloned from main, and (b) destroyed the user's B bank on every other flip thereafter. Fix per Jeff's option A: pure swap, no auto-save. New explicit "Copy A -> B" menu item (case 7) seeds the spare from current bank when the user wants a starting point for B. Compare menu label now flips to show what the click WILL DO ("swap to A bank" / "swap to B bank") rather than which bank you're on (the bank indicator below shows that). Lock bands semantically extended: now means "freeze BOTH banks for safe A/B comparison" not just "lock spare from overwrite". Widget refuses user-initiated edits while locked - mouseDown/mouseDrag short-circuit on band handles, per-band gain fader / freq knob / Q knob / type combo all setEnabled(false) for visible greying. Right-click menu still opens (user may want to invoke Compare / Copy A->B etc). APVTS-driven changes (preset load, automation playback) bypass the lock - it's a UI gesture, not a DSP bypass. New `BankIndicator` inner component on ParametricEQDisplay: green pill "A Bank" / red pill "B Bank", click swaps banks (alias for triggerCompare). Lives unparented in the EQ display; pages inject it into the PageMenuBar's extra-right slot via `getBankIndicator()` on EQ-tab activation, removed via new `removeExtraRightComponent(juce::Component*)` API on switching away. New `refreshBankIndicator()` helper called after every swap so the colour + label track DSP state. EQ display 22-px toolbar row removed (T19 from prior session) - the row was empty after the "..." button moved to the hamburger and `[SPARE]` migrated to BankIndicator. mToolbarArea kept as empty Rectangle for any leftover paint references. Net result: EQ canvas gains 22 px of usable height app-wide. Bonus cross-system fix bundled in: page activation in StandaloneEditor now also calls `clearExtraRightComponents()` at the top of the if-mPageMenuBar block - pre-existing leak of per-page extras (Effects meters/bypass/trackBox/trackLabel, etc) across page switches that became visible with the new bank indicator. Each page branch then re-adds its own extras as before. Prior entry below.)

**Prior update:** 2026-04-19 (Universal PageMenuBar hamburger convention established + EQ menu migrated. Per Jeff's direction the ≡ hamburger is now the universal "page actions" pivot across the entire app: per-component options menus install into the shared PageMenuBar's hamburger via a new `setMenuBuilder(MenuBuilder)` callback rather than dedicated `...`/gear buttons on component toolbars. PageMenuBar gets a new `MenuBuilder = std::function<void(juce::Component* anchor)>` field; when set, hamburger click invokes it instead of building from the simple flat mMenuItems list. ParametricEQDisplay gets `installPageMenu(PageMenuBar&)` + `uninstallPageMenu(PageMenuBar&)` plus `showEQOptionsMenu(juce::Component* anchor)` reworked to take an explicit anchor (defaults to the legacy in-display ... button which is now hidden). The four pages with EQ tabs (EffectsPage / LayersPage / BassPage / DrumsPage) all gain `getEQDisplay()` accessors. StandaloneEditor's per-page tab-click lambdas now also call a new `syncEQHamburger(eq, onEqTab)` helper that installs the EQ menu into the page's hamburger when the EQ sub-tab is active and clears it on switching to other sub-tabs - both on initial page activation AND on tab clicks. The in-display ... button (`mOptionsBtn`) is hidden via setVisible(false) but kept around so the existing 22-px toolbar row still paints the [SPARE] A/B-compare indicator. Memory rule `feedback_universal_page_menu.md` saved to capture the convention going forward (apply to Mixer / Builder / Effects rack tab / future pages as we touch each one). The empty 22-px EQ display toolbar row is a Tier 3 cleanup target - can be removed once [SPARE] indicator migrates somewhere else (e.g. into the hamburger menu title or as a small badge in the EQ display's own paint area). Prior entry below.)

**Prior update:** 2026-04-19 (\u00a712 EQ8 Phase 3 item 12g shipped - Linear-phase / HQ modes via FFT convolution. PhaseMode enum (Standard / Linear / HQ Plus / HQ Linear / HQ Extended) now actively drives DSP - was UI-only stub. New `EqLinearPhaseProcessor` class (header + cpp): 50%-OLA Hann-windowed FFT processor, per-channel input ring + output OLA ring, IR stored as N/2+1 real magnitude bins (zero phase = linear phase). Hand-rolled (NOT juce::dsp::Convolution) so IR rebuild is cheap on every band drag. Per-mode FFT size: HQE=512 (T2d locked), Linear=2048, HQ Linear=4096. Latency = N/2 reported via getLatencySamples and summed with AC oversampler latency in EQ8DSP::getLatencySamples. EQ8MsDSP wrapper sums mid+side latency unchanged - doubles per-instance. EQ8DSP::setPhaseMode now actually acts: forces AC on/off for HQ Plus / HQ Linear, calls prepare() to resize TPT + oversampler + linear processor for the new design rate, marks IR dirty, arms a mode-aware output fade scaled to min(latencyDelta, 4096) with floor 256 (T2c) so the seam is masked even on Standard->HQL flips that shift latency by ~2050 samples. New `reconfigureLinearProc()` lazy-allocates / frees the linear processor based on phase mode + design rate. New `magnitudeForFrequencyStatic()` builds the IR from band design gain only (no GR injection) so dynamic-EQ behaviour is excluded from the linear IR per T2b option C. Per-band M/S routing restricted to Stereo in linear modes per T2a option B - linear processor convolves combined L+R independently. UI: ParametricEQDisplay popup Processing Mode submenu now (a) appends a live [+N sp] latency readout to each mode label so the user sees the cost before picking (T2e), (b) writes chosen mode to mBoundDSP->setPhaseMode / mBoundMsDsp->setPhaseMode then fires onLatencyChanged so host PDC refreshes via the per-page wiring established by 12f. Right-click band-handle Channel submenu greys all 5 routing items + relabels "Channel  (disabled in Linear modes)" when EQ is in any linear mode. Same treatment for Make Dynamic + Dynamic Params items. syncFromDSP pulls phaseMode from bound DSP each tick so popup checkmark is correct after preset load / external change. State serialisation: phaseMode int added to EQ8DSP XML (PRESET-SAFE additive; defaults Standard on missing); setStateInformation calls setPhaseMode(saved) when loaded value differs so the linear processor reconfigures + AC re-forces. EQ8MsDSP::setPhaseMode + isLinearPhaseMode pass-throughs added (header-only). Spectrum analyser pre/post taps documented as having FFT/2 offset in linear modes per T2f option C (no DSP change). CPU: ~1% HQE 512, ~3-5% Linear 2048, ~5-8% HQL 4096 per instance; doubled by M/S wrapper. Tier 3: T15-T18 added (multi-IR M/S, per-block dynamic, user-selectable FFT size, IR crossfade). CMakeLists updated with EqLinearPhaseProcessor.cpp under VibesynthStandalone target. **§12 EQ8 now complete: 10/10 spec items shipped (Phase 1: 12a/b/c/d, Phase 2: 12h/i + EQ automation wiring, Phase 3: 12e/j/f/g).** Prior entry below.)

**Prior update:** 2026-04-19 (Transport Bar perf-cell follow-on + new System Pages -> Transport Bar subsection. After 12f shipped, perf cell got a 2x2 grid (top row SYS / DSP %, bottom row MEM / LAT) so LAT could ride alongside the existing CPU + memory readouts without widening the bar. First round used unit suffixes (MB / sp / ms) in the bottom row text; second row overflowed the 160 px cell and clipped the M of MEM at the left edge. Fix: dropped unit suffixes from the rendered text (now `MEM 234  LAT 16`) and moved them into the tooltip only - both rows now have balanced widths. `onGetSampleRate` callback kept wired in StandaloneEditor for forward-flexibility (currently unused; ms readout deferred to TB-T1). New Tier 3: TB-T1 LAT readout in ms once the layout supports it. New Blueprint subsection created under `## System Pages` -> `### Transport Bar` per Jeff's living-document convention ("anything we touch that isn't already on here gets its own subsection"). Memory rule `feedback_blueprint_living_doc.md` saved to capture that workflow going forward. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a712 EQ8 Phase 3 item 12f shipped - 2x oversampling anti-cramping (opt-in per EQ instance, default off). EQ8DSP gains `mOversampler` (1 stage IIR half-band polyphase, useIntegerLatency=true, 2 ch) allocated in `prepare()`, `mAntiCramping` flag, `setAntiCramping(bool)` setter that re-runs `prepare()` so TPT instances + oversampler scratch resize for the new design rate. New `designSr()` helper threaded through `updateBand` + `makeOneSection` so main filter coefs are designed at 2*sr when AC on. Detector path stays at HOST rate (mDetInput snapshot + envelope + GR computer all run pre-bracket; detector biquap built with host sr). `process()` refactored: extracted band dispatch into new `processBands(juce::dsp::AudioBlock<float>&)` so the same code runs on either host-rate buffer (AC off) or upsampled block (AC on). Bracket pattern: processSamplesUp -> processBands -> processSamplesDown. M/S encode/decode runs INSIDE the bracket on upsampled samples (T2b confirm). mMsScratch sized for 2*maxBlockSize so AC toggling never reallocates audio-thread. New `EQ8DSP::getLatencySamples()` override returns ceil(mOversampler->getLatencyInSamples()) when AC on, 0 otherwise. New `EQ8MsDSP::getLatencySamples()` returns mid+side latency (sequential processing on same buffer). New `EQ8MsDSP::setAntiCramping(bool)` pass-through (per-instance scope, T2c). `getMagnitudeForFrequency` biquad branch + dynamic-band local rebuild branch both pass `designRate` so the displayed curve matches the audible response; TPT branch unchanged (analog-prototype magnitude is sample-rate-agnostic). UI: new "Anti-cramping (2x OS)" item in showEQOptionsMenu (case 6), shown only when DSP/MsDSP-bound. Click flips setAntiCramping then fires new `onLatencyChanged` callback wired by all 4 pages (EffectsPage / DrumsPage / LayersPage / BassPage) to refresh host PDC. **Cross-system PDC fix bundled in (T2d audit):** `VibeGraph::updateBusLatencies()` now sums rack + busEq per bus instead of just rack - pre-existing bug benign while every EQ reported 0 latency, would have drifted the moment AC went on. State serialisation: `antiCramping` bool added to EQ8DSP's XML state tree (PRESET-SAFE additive; defaults false on missing). EQ8MsDSP roundtrips it via existing base64-wrapped inner EQ state. setStateInformation calls setAntiCramping(acSaved) when loaded value differs so TPT/oversampler re-prepare runs. CPU 2x when enabled. **Toggle fade follow-up:** added a 256-sample linear output ramp on every AC toggle (both directions) to mask the latency-shift discontinuity that was audible as a one-shot click on sustained notes. New `kAcFadeSamples` constant + `mAcFadeRemaining` counter on EQ8DSP; `setAntiCramping` arms it; `process()` consumes it post-bracket and applies a per-sample gain ramp from 0 to 1 to L and R (same ramp for stereo coherence). Cost negligible (256 multiplies one-shot per toggle). **Global LAT readout follow-up:** GlobalTransportBar's perf cell reworked from 1-line to a 2x2 grid (top row SYS / DSP %, bottom row MEM / LAT). LAT polls `mProcessor.getLatencySamples()` via new `onGetLatencySamples` transport-bar callback wired in StandaloneEditor (plus an unused `onGetSampleRate` kept around for future ms-readout uses); format is bare integer sample count. Bottom-row unit suffixes (MB / sp) dropped after first round caused the bottom row to overflow the cell width and clip the M of MEM at the left edge - units now live in the tooltip only, both rows now have predictable balanced widths. Same column position as before; cell footprint widened from 110 to 160 px and the second text row replaces what would have been horizontal growth. 9pt monospaced, centredRight, multiline juce::Label with `\n` separator. Tooltip updated to document all four metrics. Surfaces PDC for every effect that overrides `getLatencySamples()` (EQ8 anti-cramping, plus the §5 / §6 / §9 / §10 / §11 oversampled / look-ahead modules already shipped). Tier 3 added: T12 (variable OS factor 2x/4x/8x), T13 (FIR-equiripple alternative oversampler), T14 (auto-enable AC heuristic). Per-insert / per-instr-channel PDC remains a separate gap (logged but out of scope this session). Phase 3 remaining: 12g Linear-phase / HQ mode. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a712 EQ8 Phase 3 12j polish round 4 - expansion noise floor + app-wide right-click-doesn't-change-value fix (targeted). Issue 1 - upward expansion was semantically-correct but visually wrong. When the band's detector saw near-silence in its frequency range, envDb sat at my floor (-120 dB), so 'under threshold' was huge (102 dB with default thr=-18), grDb pegged at rangeMag, ghost outline overlapped live curve, GR meter pegged 'expansion always happening'. Audibly silent (no signal to boost) but display completely misleading. Fix: added a noise-floor gate. When envDb < threshold - 2 x |rangeDb|, grDb stays at 0. Creates a sensitive window [thr - 2*rangeMag, thr] where expansion fires; beyond that, no fake expansion on silence. With default thr=-18 range=+12: expansion fires for envelope between -42 and -18 dB; below -42 no GR. Matches user-intuitive 'boost present-but-quiet content' semantics. Issue 2 - right-click on any slider / knob / chicken-head was JOGGING the param value in addition to showing the Automate popup (JUCE's default juce::Slider::mouseDown doesn't short-circuit on popup-menu right-click unless setPopupMenuEnabled(true) is on, which would install JUCE's default Default/Set menu and collide with our GlobalAutoRightClick-driven Automate menu). Targeted fix: added new VibeSlider class in SharedUI.h - a juce::Slider subclass whose mouseDown / mouseDrag / mouseUp return early on right-click so the slider's value never changes on right-click. SnapSlider rebased on VibeSlider so mixer strip's main fader inherits the guard. Swapped juce::Slider -> VibeSlider in: EQ widget BandControl (gainFader / freqKnob / qKnob x 8 bands), DynamicParamsPopout's 5 knobs (Thr/Ratio/Atk/Rel/Range), MixerTrackStrip's mPanKnob + mWidthKnob. ChickenHeadSelector::mouseDown gained right-click handling that opens a popup menu listing all options (current one checkmarked, click to pick directly) + "Automate: ..." item when the selector has a componentID; mouseDrag gained right-click early-return. Right-click no longer changes the selection by accident AND gives users a faster way to jump to any option without having to drag the wheel to it. Also this session: dynamic Range param magnitude matched to Gain param magnitude - -18..+18 instead of -24..+24 - so Range can't exceed what the gain knob could reach on its own. PRESET-BREAK pre-v1 (stored values outside the new bounds clamp on load); default still -12 inside the new range. App-wide juce::Slider -> VibeSlider refactor logged as future session (pattern: every effect panel, every instrument page, every other place juce::Slider is used - probably a couple hundred find/replace sites). PRESET-SAFE (pure UI behaviour fix). Prior entry below.)

**Prior update:** 2026-04-19 (\u00a712 EQ8 Phase 3 12j polish round 3 - four items shipped together. (1) Inline TextEditor readout editing: double-click any of the 24 per-band Gain/Freq/Q readouts in the right panel -> shared juce::TextEditor child overlays at exactly the readout rect size (no layout impact), pre-filled with current value formatted as shown, select-all'd, grab-focus. Enter commits + parses via the existing slider-range + clamp logic; Escape cancels; focus-loss commits. Accepts shorthand (e.g. '1.5k' / '12khz' / '+3.5' / '-12'). Zero external layout change. (2) Range knob is now BIPOLAR (PRESET-BREAK pre-v1): APVTS Range param range changed from 0..24 to -24..+24, default -12 (matches today's downward default). Sign encodes direction, magnitude encodes amount: negative = max compression amount (cut above threshold), positive = max expansion amount (boost below threshold), zero = no modulation. Upward APVTS bool kept as unused scaffolding for preset stability. DSP gain computer derives direction from sign(rangeDb) instead of reading the Upward flag. Popout UI drops the Up toggle entirely - one knob controls both direction + amount. Range knob has double-click-to-zero + new tooltip explaining bipolar behaviour. getBandEffectiveGainDbAtRangeLimit = params.gainDb + params.rangeDb (signed), matches ghost range outline rendering. Hover tooltip header shows 'Dynamic (Compress)' / 'Dynamic (Expand)' / 'Dynamic (off)' based on range sign, with colour-coded header (orange / green / dim) and signed Range display. GR meter uses |rangeDb| for fill scaling. (3) Right-panel per-band controls gain automation consistency: stampRightPanelComponentIds() called from bindMsDSP + setShowMid stamps paramIds on freqKnob / gainFader / qKnob under the currently-viewed (mid or side) prefix. Right-click any right-panel control -> GlobalAutoRightClick catches it and offers the 'Automate: ...' menu with the SAME paramId as the dot's Automate submenu (one automation per paramId - no duplicates, single source of truth). Re-stamped on MID/SIDE flip so paramIds track the view side. (4) DynamicParamsPopout right-click automation fix: popout is a CallOutBox peer top-level window so the app-wide GlobalAutoRightClick never saw its child sliders. Added a self-MouseListener on the popout (addMouseListener(this, true)) that duplicates GlobalAutoRightClick's right-click -> Automate menu logic locally. All 5 dynamic knobs in the popout (Thr / Ratio / Atk / Rel / Range) now respond to right-click with the standard Automate + Type-in-value menu. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a712 EQ8 Phase 3 12j follow-on fixes + polish. Five issues addressed from user testing: (1) Shelf curves in ParametricEQDisplay::evalBandDb replaced with proper RBJ biquad magnitude math (both low + high shelf now render smooth S-curves and respond to Q factor - previously were linear-interpolation between two log-frequency points, Q-blind and straight-line-segment visual). Matches the biquad path JUCE uses for the actual audio. (2) DynamicParamsPopout widened 360 -> 380 px so Range knob no longer touches the GR meter. (3) JUCE system tooltip replaced with custom 3-column in-paint hover tooltip via new drawHoverTooltip() method: column 1 = band info (type, freq, gain, Q, slope, channel, state); column 2 = dynamic knob values when dynamic; column 3 = graphical GR meter (vertical bar with centreline, +/- range ticks, numeric readout, orange = downward compression, green = upward expansion). getTooltip() now returns empty string to suppress JUCE's built-in tooltip. Small handle-adjacent GR bar + old in-graph Q+slope tip both removed (info migrated into tooltip). Panel positioned near hovered handle with clamp-to-graph-bounds so it never clips off-screen. (4) Gain fader visual greying: VibeLAF::drawLinearSlider's eqFader (+ mixerFader) branch now overlays a 60%-alpha dark rectangle at the slider bounds when s.isEnabled() == false. Matches the VKnob::setLocked pattern. Previously setEnabled(false) blocked input but rendered full-color; now visibly greyed. (5) Type-swap cleanup: EQ8DSP::setBandType now zeros params.gainDb (bypassing setBandGain's type-guard early-return) when the new type is non-gain-bearing (LP/HP/Notch/BP/Off). Widget side: both typeCombo onChange AND right-click Type submenu path also zero mBands[i].gainDb on non-gain-type swap + call syncControlsFromBands() so the gain fader's setEnabled state refreshes on the way back to a gain-bearing type. Previously: fader locked at stale +6 dB on swap to LP, and stayed locked on swap back to Peaking because syncControlsFromBands was only triggered by graph-dot drag, not combo change. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a712 EQ8 Phase 3 item 12j Full Dynamic EQ shipped. Per-band dynamic / threshold / ratio / attack / release / rangeDb / upward fields + Option B sidechainSourceId scaffolding (all 8 serialised, PRESET-SAFE additive). Parallel-detector architecture (all dynamic bands see pristine original input, not in-series-filtered) matches FabFilter Pro-Q Dynamic style and prevents band self-interaction. Detector = bandpass at band.freq / band.q auto-updated at block rate. 1-pole smoother with asymmetric attack/release time constants, stereo-linked peak detection. Gain computer: downward compression above threshold OR upward expansion below threshold when upward=true, with ratio + range clamp. std::atomic<float> currentGrDb per band for UI polling. Block-rate coef rebuild uses updateBand(i, effectiveGain) override with the gain smoother state intact. Supported only on gain-bearing types (Peaking / LowShelf / HighShelf / Tilt); other types ignore the dynamic flag. getMagnitudeForFrequency branches for dynamic bands to build coefs on-the-fly with effective gain for the animated curve. APVTS: 8 new per-band suffixes added to addParamsForTrackEQ so every ~100 EQ instances gets them lazily (7 automatable via Session B's right-click Automate menu; ScSource is a routing id). Widget Band struct gains matching fields; syncFromDSP / setAPVTSFromBand / pushBandToDSP carry them; right-click adds Make Dynamic toggle + Dynamic Params popout; DynamicParamsPopout is a file-scope CallOutBox component with 5 rotary sliders + Upward toggle + live GR meter (30 Hz timer; APVTS-attached + componentID-tagged so automation + type-in-value work). drawHandles renders a small GR bar next to each dynamic handle (orange = downward, green = upward); drawCurve renders a faint dashed orange ghost outline at the range-endpoint alongside the live animated curve. evalBandDb uses effective gain (design + currentGrDb) for dynamic bands by default, with optional override for ghost-path render. Bundled UX polish: gain fader greyed via setEnabled when the band type is LP/HP/Notch/BP/Off - prevents the previous drag-and-snap-to-0 UX where the DSP would silently reject the change. External sidechain detector source logged as Tier 3 T11; needs the cross-cutting Sidechaining Infrastructure session to ship first (references existing sidechaining cross-cutting entry). Phase 3 remaining: 12f 2x oversampling anti-cramping, 12g Linear-phase / HQ mode. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a712 EQ8 Phase 3 item 12e TPT filter hybrid shipped. LP/HP/BP band types now use juce::dsp::StateVariableTPTFilter (zero-delay feedback topology) instead of cascaded biquads. Peaking / Shelf / OFF / Tilt keep biquad (TPT has no gain/shelf mode). Notch stays biquad too (SVF has no notch mode, BP-subtract workaround would cost 2x state for a filter type rarely used at extreme Q - deferred to Tier 3 T7). Implementation: BandState gained parallel tptL/tptR arrays + useTPT derived flag + tptCutoffHz/tptResonance per-section cache for the UI magnitude-query path. prepare() seeds TPT defaults only when sampleRate>0 (guards against host prepare(0) NaN'ing internal g coefficient). updateBand() LP/HP/BP branch uses the same Butterworth cascade Q table as biquad (kSteepQ - SVF setResonance matches biquad Q convention directly). process() dispatches per-band via useTPT flag - separate branches in both the L/R-domain pass (Stereo/LOnly/ROnly bands) and the M/S-domain pass (Mid/Side bands). setBandChannel + setBandType reset both biquad and TPT state (type swap between engines is the new case requiring state reset). getMagnitudeForFrequency added a closed-form SVF analog-prototype magnitude branch (|H(jw)|^2 formulas per LP/HP/BP cascaded across sections) since StateVariableTPTFilter exposes no magnitude query; curve accuracy within ~1% of actual filter response across audible range - plenty for UI. PRESET-BREAK pre-v1 (slight sonic shift on existing LP/HP/BP bands at high Q near Nyquist where biquad coefficient warping was the old behaviour; TPT keeps the analog shape). Tier 2 C1 (user-selectable engine override) + C2 (Notch via TPT BP-subtract) both deferred to Tier 3 (T6, T7). §12 Tier 3 also gained T8 TPT shelves / T9 Ladder-Moog filter engine / T10 BP Q remapping between engines. Phase 3 remaining after this: 12j Dynamic EQ (substantial DSP week), 12f 2x oversampling anti-cramping, 12g Linear-phase / HQ mode. Prior entry below.)

**Prior update:** 2026-04-19 (Bus pre-rack EQ cruft removed for Layers + Bass. mLayersEQDSP + mBassEQDSP fields deleted from PluginProcessor; layers_mid_eq* / layers_side_eq* / bass_mid_eq* / bass_side_eq* APVTS param blocks removed from createParameterLayout (240 params gone); updateLayersEQ + updateBassEQ functions deleted and their processBlock calls removed; buildFixedTopology signature dropped layersEQ + bassEQ refs; LayersBusNode + BassBusNode lost their pageEq reference member and constructor arg and the pageEq.process(buf) call in their processBlock. Drums NOT touched - DrumsPage still binds its EQ tab to mDrumsEQDSP as its page EQ (DrumsPage has no separate per-page EQ by design; when §P2 per-slot-drum EQ dropdown ships, mDrumsEQDSP becomes obsolete and gets the same cleanup). Net savings: ~200 lines of dead code + 240 APVTS params + 2 redundant audio passes per block. PRESET-BREAK pre-v1 for the deleted param IDs but zero user impact since no UI ever wrote to them. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a712 EQ8 Phase 2 Session B shipped - EQ band automation wiring + per-band channel UI + handle badges + dynamic hover tooltip + universal APVTS lazy-register + EffectsPage full-bind fix. Scope locked as one comprehensive session per Jeff's direction (option a: ship it all together). Infrastructure: ensureMixerStripParams now also calls addParamsForTrackEQ for every Master/Bus/Insert/Aux strip so 100 possible EQ instances get 144 APVTS params each (freq/gain/Q/type/on/slope/mute/solo/channel x 8 bands x 2 sides). addParamsForTrackEQ rounded out with previously-missing Mute + Solo. Generic updateEQFromApvts helper + updateAllPostRackEQsFromApvts iterator called once per processBlock after existing update-functions - inactive strips contribute zero cost via getInsertEQ nullptr short-circuit. Widget Band::channel field plumbed through all sync paths. Right-click band-handle menu gains Channel submenu (5 options, writes via APVTS) + Automate submenu (9 params, fires VKnobAutomation::sOnAutomate). registerAutomationForBoundEQ wires applicators + readers for every paramId under the bound prefix pair on every full-bindMsDSP call (idempotent). EffectsPage::onChannelChanged now uses full bindMsDSP overload with per-channel APVTS prefix - fixes Session A pre-existing audio bug where bus-EQ drags were silently overwritten every processBlock. Option C channel badge (amber letter chip upper-right of handle) shown only when routing != side default - clean graph for unchanged bands, visual flag for re-routed ones. Dynamic hover tooltip via juce::TooltipClient on the display component composes a multi-line band info readout (type / freq / gain / Q / channel / mute-solo-off state) from mHoveredBand. Complementary to the existing in-graph mini-tip (Q + slope, immediate) since system tooltip has ~700 ms delay. PRESET-SAFE everywhere (additive APVTS, defaults preserve behavior, widget UI overlays are pure paint). Prior entry below.)

**Prior update:** 2026-04-19 (\u00a712 EQ8 Phase 2 Session A polish pass shipped - follow-on fixes + UX additions bundled into the same session. Spectrum curve now extends to grid edges via flat-extension from first/last in-range FFT bin (20 Hz / 20 kHz); spectrum feed poll moved above the mSyncing/mUserDragging guard so the analyser stays live while dragging a band. DSP-to-UI band sync now skipped when MsDSP has no APVTS write-back - defends against an EffectsPage simple-bind race where processBlock's updateXxxEQ was resetting the DSP every block and the widget would pull the defaults back (logged separate pre-existing audio bug: EffectsPage's simple bindMsDSP means bus-EQ drags don't affect audio - fix naturally belongs with Session B automation scope). Reset-to-default slider/knob bug fixed: syncBandFromControl now calls pushBandToDSP in addition to setAPVTSFromBand, matching the graph-handle-drag contract that already did both; the race between setAPVTSFromBand and the next processBlock was letting the 30 Hz syncFromDSP poll stale DSP values. EQ gain faders now use the mixer-style metallic-cap skin via new 'eqFader' Slider property with EQ-appropriate -18..+18 tick labels (no + prefix, drawFittedText with 0.5 minimumHorizontalScale); live amber/red position-pointer line at cap midY; per-band readout strips below each fader (gain dB), freq knob (scale-adaptive Hz/kHz), and Q knob (2 decimal). Gain readout translated up 5 px into the visually-empty fader margin to breathe away from the freq knob. Reset Band right-click now restores freq to per-band default in addition to gain/Q/slope; type/on/mute/solo/channel left untouched. Prior entry below.)

**Prior update:** 2026-04-19 (\u00a712 EQ8 Phase 2 Session A shipped - 12h per-band M/S routing + 12i spectrum analyser overlay. 2x8=16-band structure preserved per Jeff's clarification (was NOT a collapse to 8 bands with routing field; each Band gains a channel field Stereo/Mid/Side/LOnly/ROnly, default Mid on mMid's 8 bands and Side on mSide's 8). EQ8MsDSP wrapper kept via Option A - inner encode/decode dropped, both inner EQ8DSPs receive full stereo buffer, per-band channel dispatch happens inside EQ8DSP::process() using LTI-commute grouping (L/R-domain pass + on-demand M/S-domain pass with pre-allocated scratch). Zero churn on the ~120 .mid()/.side() call sites in PluginProcessor updateXxxEQ functions - they just added one extra Channel read line each. Additive `_Channel` APVTS params on bus + per-page EQs with defaults matching existing behaviour - PRESET-SAFE. Filter state reset on channel change (via setBandChannel + setBand) avoids transient click when the filter's input domain flips L->M etc. 12i: each EQ8MsDSP owns preFeed + postFeed SpectrumFeed members populated at process() I/O boundary; extracted SpectrumFeed to new DSP/SpectrumFeed.h header to resolve circular-include concern (VibeGraph::SpectrumFeed kept as using alias). Removed standalone mLayersEQFeed/mDrumsEQFeed/mBassEQFeed fields + old single-tap feed.push() call in bus nodes; buildFixedTopology signature simplified. Widget gets pushSamplesPre + mSpectrumDbPre + dual-spectrum drawSpectrum (translucent-grey pre behind green-tinted post) + syncFromDSP poll of both feeds every tick when bound. Pre-existing EffectsPage gap fixed in passing - timerCallback now drives mEQDisplay->syncFromDSP() (the Effects EQ was never polling its feed before). Internal MID/SIDE pill deleted; external page-header MID/SIDE buttons unchanged. Per-band-channel UI (right-click menu + handle badge) deferred to Session B since without automation wiring there's no way for UI changes to survive processBlock override. Session B remaining: EQ band automation wiring (freq/gain/Q/type/slope/channel) + per-band-channel right-click UI + handle badges. Prior entry below.)

**Prior update:** 2026-04-18 (EQ band automation promoted to Tier 1 for \u00a712 Phase 2. Jeff flagged during \u00a712 Phase 1 testing that he expected EQ band automation to already exist. It doesn't - ParametricEQDisplay handles band drag directly via mouseDown/mouseDrag with no componentID-tagged sliders underneath and no APVTS bridge. Originally logged as Tier 3 gap; promoted to Tier 1 per Jeff's scope direction. Full scope: Freq + Gain + Q per band in v1 initial ship; type + slope + channel added once 12h per-band M/S lands (since channel is new there). Lazy-register per strip (matches 5F-4a mixer-strip lazy pattern) to avoid thousands of APVTS params upfront. Widget drag-behaviour unchanged - just goes through APVTS now so applicators + readers work. Right-click 'Automate: freq/gain/Q' menu on band handles. Ship order within Phase 2: 12h -> 12i -> EQ automation (depends on 12h for channel). PRESET-SAFE. Blueprint updated in both the \u00a712 Phase 2 section and the Cross-cutting section. Also saved memory rule 'feedback_no_eq_automation.md' so future sessions don't suggest EQ-band automation tests until the wiring ships. Prior entry below.)

**Prior update:** 2026-04-18 (Mixer-strip EQ binding fix sweep - 4 additional call sites beyond the drum-strip dropdown fix earlier today. Jeff noticed the symptom on Layer/Bass mixer strips (EQ tab changes had no effect on sound), then flagged aux + audio as likely same pattern. Confirmed. **Four parallel call sites** all had the 5F-4a migration asymmetry: Layer (IDs 200..207), Bass (IDs 300..303), Aux (IDs 600..615), Audio (IDs 400..499). All had UI EQ binding wired to nullptr or to a legacy-only getter that returned nullptr after the InsertNode migration. Meanwhile audio path already routed through each InsertNode's EQ (audio was flowing through, just not controllable from UI). **Fix:** EffectsPage::onChannelChanged now uses getInsertRack + getInsertEQ InsertNode-first for all four, with legacy fallbacks. Also patched VibeGraph::getAudioRowEQ itself to use InsertNode-first (matching the existing getAudioRowRack dual-path) for any future callers. PRESET-SAFE. **Cross-system audit follow-up logged:** getChannelPrefix still returns instr_{id} for drums, and the automation display-name resolver in StandaloneEditor.cpp still routes `instr_{id}` via legacy getInstrChannelRack — both need a follow-up sweep since drums now live in InsertNode. Prior entry below.)

**Prior update:** 2026-04-18 (§12 EQ8 Phase 1 shipped - infrastructure + quick wins. §12 is the biggest spec in the DSP review (10 sub-items) so scope is split: Phase 1 = audit fixes + 12a/12b/12c/12d. Phase 2 = 12h per-band M/S + 12i spectrum analyzer. Phase 3 = 12j Dynamic EQ, 12g linear-phase, 12e TPT hybrid, 12f anti-cramping (each dedicated session). **Phase 1 this session:** A1 denormal guard (64 IIR filter states, high-risk), A2 CPU guards on band setters, A3 CPU guards on misc setters, A6 mSpareLocked encapsulation cleanup, A8 anySoloed() cached per-block (was O(n^2)). Spec: 12a getMagnitudeForFrequency + dB variants for UI curve drawing (null-guarded, thread-safe const), 12b proportional Q on Peaking bands with setProportionalQ(bool) toggle default-on (PRESET-BREAK pre-v1), 12c SmoothedValue per band on freq/gain/Q with block-rate deferred coef rebuild via dirty flag + lastApplied tracking (eliminates automation clicks), 12d mIIRModSpeed wired as smoothing ramp time (was dead param; maps 0..1 -> 1..50 ms). Cross-system blast radius verified safe: EQ8MsDSP wrapper entry points unchanged, all VibeGraph EQ accessors unchanged, ParametricEQDisplay unaffected (has own evalBandDb path), PluginProcessor APVTS-driven setBand* call sites (~30) already guard with != so new internal CPU guards are neutral. Prior entry below.)

**Prior update:** 2026-04-18 (Drum strip effects-rack dropdown fix. Pre-existing bug from 5F-3 migration: EffectsPage routed drum channels (ID 100..115) through legacy `getInstrChannelRack` which returns nullptr since drums moved to InsertKind::Drum architecture. Result: clicking effect-type options in the dropdown did nothing on drum strips. Only drum strips affected — layer/bass/bus/aux/audio all worked. **Fix:** route 100..115 through new `VibeGraph::getInsertRack(InsertKind::Drum, id-100)` + `getInsertEQ(InsertKind::Drum, id-100)` (new getter parallel to existing getInsertRack). Legacy `getInstrChannelRack` kept as fallback for state-restore paths. PRESET-SAFE. Prior entry below.)

**Prior update:** 2026-04-18 (§11 Transient Shaper shipped - full spec bundle + all Tier 2 adds. Tier 1: A1 denormal guard, A2 CPU guards on 10 setters, A4 setAttack dual-range kludge removed, A5 Panel A9 extended to chicken-heads (attackShapeSel + releaseShapeSel now sync from DSP), A7 dead mFastR/mSlowR removed. Full §11 spec bundle: 11a quadratic attack+sustain curves (PRESET-BREAK pre-v1), 11b Linkwitz-Riley 4th-order crossover replaces 1-pole LP band-split (PRESET-BREAK pre-v1), 11c 4x oversampling around drive with CONSTANT latency via always-on oversampler (identity pass-through when drive < 0.001), 11d SmoothedValue on 8 continuous params, 11e slow envelope uses RMS detector (PRESET-BREAK pre-v1). Tier 2 additions: C1 OS factor chicken-head (2x/4x/8x/16x, default 4x), C2 Stereo envelope detection toggle (default off = v1 mono-sum; on = per-channel for asymmetric stereo transients), C3 Dry/Wet mix knob (default 1.0 = v1 100%-wet), C4 FastRel + SlowAtt exposed as knobs (1-50 ms each, default 10 ms match prior hardcoded). Panel restructured to 2 rows: Row 1 (7 existing knobs + Attack/Release shape chicken-heads), Row 2 (Wet/FastRel/SlowAtt + OS chicken-head + StereoDetect toggle). Tier 3 logged: T1 multiband transient shaper, T2 look-ahead detector, T3 sidechain input, T4 character voicing presets. Forward work next: \u00a712 EQ8 (biggest; full Dynamic EQ + spectrum). Prior entry below.)

**Prior update:** 2026-04-18 (Drum-engine capability expansion pre-booked as Tier 1 for §P2 + §P3 reviews. Two linked pieces: (1) **§P2: Dual-engine drum slots** — each BaySickDrums slot can hold either a VibePlayerProcessor (sampler) or BaySickSynthProcessor (synth); DrumsPage editor dispatches to `VibePlayerEditor` or full `BaySickSynthEditor` based on slot type; default = Sample preserves v1 kit behaviour. PRESET-BREAK ⚠️ (drum state format changes from `16 x VibePlayerState` to `16 x {engineType, engineState}`), ships in v1 alongside the dual piano roll change (same preset-break window). (2) **§P3: Drum Synthesis Expansion Bundle** — 6 DSP adds to BaySickSynth: pitch envelope, sine waveform primitive, noise-only mode, free-Hz dual-osc tuning, transient injector, multi-burst envelope mode. All PRESET-SAFE (additive; neutral defaults). Together the 6 adds unlock authentic TR-analog drums (78, 808, 909, 606), Simmons electronic drums, Yamaha FM drums (RX/DX percussion), Korg analog machines (DDM, KPR), broader tuned percussion (bells/marimbas/chimes/woodblocks/tabla/congas etc.), and any modern designer synth percussion. Full capability map with era/style vs before/after table appended under §P3. Cross-benefit: the 6 adds also enrich BaySickSynth as a general synth (pitch env for leads, noise-only for textures, etc.). Ship order: §P3 DSP adds -> §P3 editor controls -> §P2 dual-engine refactor -> §P2 factory drum-preset bank. Prior entry below.)

**Prior update:** 2026-04-18 (Player Blueprint pre-booking additions. Audit found: (1) all 5 player editors (Harmless / BaySickSynth / BaySickBass / VibePlayer / BaySickDrums) use `juce::AudioProcessorValueTreeState::SliderAttachment` for their knobs (except BaySickDrums which uses 0 attachments because it embeds a VibePlayerEditor for the selected slot). JUCE's SliderAttachment handles bidirectional slider<->APVTS sync, so **the A9 slider-sync bug that hit effect panels does NOT apply to players** - Jeff confirmed "I registered all params in APVTS for that reason." Logged as "assumed not applicable" with a verify-at-review-time note. (2) **Right-click "Automate" menu gap applies to ALL 5 players.** None of the player sliders set `componentID`, so `GlobalAutoRightClick` (the app-wide mouse listener that fires the "Automate: X" menu on right-click) can't hook them - but the params ARE all registered in APVTS and fully automatable from the backend. Fix pattern logged per-player: `slider.setComponentID(apvtsParamId)` per attached slider + confirm `VKnobAutomation::sOnRegisterApplicator`/`sOnRegisterReader` wiring. Pre-booked as Tier 1 at each player's review time. Prior entry below.)

**Prior update:** 2026-04-18 (A9 slider-sync cross-apply sweep to ALL 11 effect panels. Pattern: `knob->slider.setValue(dsp->mField, juce::sendNotificationSync)` at end of panel ctor, after onValueChange bindings are wired. The sync fires the existing onValueChange lambda, which calls the DSP setter with the slider's (potentially clamped) value. Setter CPU guards no-op when values match. Handles out-of-range DSP state (e.g. when a knob range shrinks in code), stale session state, preset restoration mismatches. Panels covered: Tape (tightened from earlier dontSendNotification pattern), Compressor, Reverb, Saturation, Chorus, Delay, Flanger (Feed knob uses `mFeedback * 100` for pct mapping), Overdrive, Phaser, Limiter, Transient Shaper (Attack/Release use `* 100` for -100..100 UI vs -1..1 DSP storage). PRESET-SAFE (pure UI-side reconciliation; no DSP behavior change). Prior entry below.)

**Prior update:** 2026-04-18 (§10 Tape follow-on fixes 3 - FlutHz range change + A9 slider sync. (1) FlutHz knob range 5-25 Hz default 15 -> 1-15 Hz default 5 (per Jeff request; more musical range for mechanical tape flutter). (2) After the range change Jeff reported rates sounding 3x faster than displayed ("slider at 5 sounds like 15"). Root cause: panel's `buildKnobs` sets sliders to their build-time defaults via `setValue(d.def, dontSendNotification)`, but the running DSP instance held its OLD default value (15 Hz) from before the rebuild (stale in-memory state or persisted JUCE standalone session state). Slider displayed 5, DSP played 15. A9 pattern previously only covered toggles and chicken-head selectors; now extended to ALL sliders in TapePanel via post-buildKnobs `setValue(dsp->mField, dontSendNotification)` sync. **This is a latent bug in the other 11 effect panels too** — same pattern applies whenever a DSP default differs from buildKnobs defaults. Logged to A9 cross-cutting section for future cross-apply pass; not urgent since defaults typically match, but safer long-term. Prior entry below.)

**Prior update:** 2026-04-18 (§10 Tape follow-on fixes 2. (1) Defaults: `PreShelfDb` and `DeShelfDb` 6/-8 -> 0/0, `Hiss` 0.1 -> 0. Fresh Tape slot now opens fully flat (no emphasis shaping, no hiss) - users opt into each sonic element. (2) Flutter noise cutoff bug: spec wrote `mFlutterNoiseState = 0.98 * state + 0.02 * raw` claiming "~300 Hz LP smoothing" - actual cutoff was ~154 Hz at 48k. Since flutterNoiseState is used as a DELAY-LINE MODULATOR, that 154 Hz of noise content creates audible pitch jitter when the delay read position jumps around. Jeff reported "background static" appearing above FlutDp ~0.13, growing into "distorted static" at max. Changed to 0.999/0.001 (~7.6 Hz cutoff - sub-audible mechanical wobble, matches real tape capstan flutter). Also reduced noise mix share 0.8 sin + 0.2 noise -> 0.9 sin + 0.1 noise for cleaner flutter character at full depth. Prior entry below.)

**Prior update:** 2026-04-18 (§10 Tape cubic interpolator bug fix. My initial `cubicCircular` helper used a coefficient formulation I wrote myself that did NOT reduce to `y1` at `frac=1` (verified: on a ramp y=n, ym1=-1, y0=0, y1=1, y2=2, my formula returned 6 instead of 1 at frac=1). Every sample-boundary crossing produced a wrong output -> audible chopping/static on both wow and flutter modulation. Replaced with standard 4-point 3rd-order Hermite (c0..c3 form) that is verified to return y0 at frac=0, y1 at frac=1, with C1 continuity across boundaries. Jeff reported "chopping static" on wow and "way too fast with static" on flutter; both symptoms trace back to this bug since both modulators share the same interp helper. Prior entry below.)

**Prior update:** 2026-04-18 (§10 Tape follow-on fixes. (1) `WowDp` and `FlutDp` defaults changed 0.3/0.15 -> 0/0 so a fresh Tape slot opens as a clean mastering effect; users opt into wow/flutter as a creative choice. (2) Hysteresis alpha calculation bug fixed. My initial implementation used a time-constant model with `tau = 5ms * exp(-5.5 * tapeSpeed)` which gave tau=5ms at 7.5 ips -> ~32 Hz cutoff at OS rate, killing almost all audible content (Jeff reported DBFS showing nothing at 7.5 ips but muffled sub-bass audible). Replaced with spec formula: `alphaBase = 0.2 + 0.8 * tapeSpeed`, then `alphaOS = alphaBase / osFactor` to preserve cutoff frequency regardless of OS factor. At 7.5 ips / OS=4x now gives ~1.5 kHz cutoff (tape-dark but musical), at 30 ips / OS=4x gives ~7.6 kHz cutoff (tape-bright). `HystAmount` still scales: 0 = bypass (alpha=1), 1 = spec default, 2 = half alpha (more memory). Prior entry below.)

**Prior update:** 2026-04-18 (§10 Tape shipped - full rewrite per spec. Tier 1: A1 denormal guard, A2 CPU guards on 14 setters, A3 while-wrap LFOs, A5 dead statement removed, A6 symmetric hiss formula (independent L/R RNG), A9 Panel state sync on tapeSpeedSel + osSel. Full §10 spec bundle: 10a hysteresis (SR-aware alpha via time-constant model), 10b asymmetric sigmoid shaper (PRESET-BREAK pre-v1), 10c 4x oversampling around shaper + hysteresis with Phase 0/1/2/3 restructure, 10d separate flutter LFO (5-25 Hz + smoothed noise), 10e cubic Catmull-Rom interpolation on wow/flutter delay reads, 10f pre/de-emphasis shelf pair replaces LP (PRESET-BREAK pre-v1), 10g pink-filtered hiss + 200 Hz HPF with independent L/R streams (PRESET-BREAK pre-v1), 10h 5 Hz SR-aware DC blocker, 10i SmoothedValue on all 10 continuous params. Tier 2 additions: C1 Hyst Amount knob (0..2 multiplier on alpha), C2 OS factor chicken-head (2x/4x/8x/16x default 4x), C3 Pre/de-emphasis gain knobs (+-12 dB, default +6/-8), C4 Tape speed chicken-head (7.5/15/30 ips wraps mTapeSpeed 0..1), C5 Bias knob (0..10 pre-shaper DC offset; 5=neutral). Panel reworked to 11 knobs over 2 rows + 2 chicken-heads. Tier 3 logged: T1 IR cassette profile, T2 Type II/IV variant presets, T3 isolation mode, T4-T6 bundle (multi-head / drop-outs / print-through). Forward work next: \u00a711 Transient Shaper. Prior entry below.)

**Prior update:** 2026-04-18 (rename sweep + re-framing as living reference, no code shipped. File renamed `v2_wishlist.md` -> `vibedaw_blueprint.md` to reflect evolving role: no longer just a deferred-items wishlist, but VibeDAW's living project reference covering everything that IS + everything that could be. Top-of-file intro rewritten to frame the Blueprint vs CLAUDE.md relationship (CLAUDE.md = hot session-loaded rules; Blueprint = deep on-demand reference). "System Pages (in-scope for v1 polish)" heading dropped its qualifier -> just "System Pages" (the FX-1 Effects Page item there is post-v1.0, so the v1-polish qualifier had become inaccurate). Memory files renamed for consistency: `feedback_plan_vs_wishlist.md` -> `feedback_plan_vs_blueprint.md`, `feedback_wishlist_category.md` -> `feedback_blueprint_category.md`. All references to old filenames updated across `lucky-discovering-tiger.md`, `MEMORY.md`, and the 2 renamed memory files. Category-placement rule kept as-is (preserved from earlier today): if a new entry doesn't cleanly fit a category, stop and ask Jeff. Prior entry below.)

**Prior update:** 2026-04-18 (version-label sweep, no code shipped. Release target clarified as **v1.0** (first public release; v1.1+ = maintenance releases; v2.0 = next major). All `v2.0` / `Post-v2.0` / `v1.9` references that meant APP version changed to `v1.0` / `Post-v1.0` / `v0.9` across plan, wishlist, CLAUDE.md, and memory files. Wishlist header renamed to "Post-v1.0 Wishlist" (filename `v2_wishlist.md` kept for continuity with existing references). All `**Tier 3 (v2+):**` labels per-module -> `**Tier 3 (post-v1.0):**`. "Future Effect Modules (v2+)" section renamed to "(post-v1.0)". Note: plan doc header "v3.0" means plan-document revision 3 (consolidated from 2 prior plans), not app version — left unchanged. Also added **top-of-wishlist rule:** if a new entry doesn't cleanly fit an existing category, I must stop and ask Jeff which category it belongs in before writing. Saved as memory rule `feedback_wishlist_category.md`. Prior entry below.)

**Prior update:** 2026-04-18 (wishlist inventory expansion - no code shipped. Added **System Pages / Effects Page** subsection with FX-1 entry: future rack UI refactor (sidebar picker + single detail pane, like Event Editor) including preset-impact analysis (zero - DSP state is fully decoupled from UI), CPU analysis (audio unchanged; ~2-5% UI thread recovered from 6x less panel painting + 5x fewer meter timer ticks; extra squeeze available via visibilityChanged stopTimer pattern), UX tradeoffs (sidebar inline VU/DBFS per row preserves at-a-glance debugging), and implementation notes. PRESET-SAFE. Also added **Future Effect Modules (v2+)** top-level section with 23 effects across 4 categories: Dynamics (Gate/UpComp/MultiComp/DeEsser/VocalRider/Maximizer), Harmonics (Bitcrusher/Wavefolder/Exciter/RingMod/Octaver/Comb), Time-Based (Convolution/PitchShifter/Granular/Widener/Reverse), Modulation (Tremolo/AutoPan/Vibrato/EnvFilter/FreqShifter/RotarySim). Existing codebase reuse noted per effect (PhaseVocoder for PitchShifter, juce::dsp::LinkwitzRileyFilter for MultiComp, existing LFO code for Tremolo/AutoPan/Vibrato). All PRESET-SAFE by construction (net-new modules). Prior entry below.)

**Prior update:** 2026-04-18 (§9 Saturation shipped - full Tier 1 + all Tier 2. Tier 1: A1 denormal guard, A2 CPU guards on 10 setters, A5 Panel A9 (tubeTypeSel/transformerTog + new autoGainTog/osSel read from DSP), A7 cbrt optimization (Type B shaper, ~3x faster). Tier 2: 9a 4x oversampling around tube (2-channel polyphase IIR, reports getLatencySamples(), Phase 0/1/2/3 restructure with both low and high bands oversampled to avoid comb filtering) + 9b Auto-Gain toggle (decouples Sens drive from output volume) + 9c SR-aware DC blocker (5 Hz target, replaces hardcoded 44.1-kHz-only coefficient) + 9d/C1 smoothing on ALL 8 continuous knobs (Flowers/Dabs/Sens/Out + Wet/BassRelief/TonePre/TonePost; processTube refactored static taking per-sample params) + C2 OS chicken-head (2x/4x/8x/16x default 4x; mirror of \u00a76 C5) + C4 Auto-Gain dB compensation label (10 Hz timer keeps synced against automation-driven Sens changes). Tier 3 logged: T1 asymmetric sigmoid as Type D, T2 pre/de-emphasis EQ curves, T3 multi-stage cascade, T4 multi-band saturation, T5 character voicing presets (Tube/Console/Tape/Fuzz - DEPENDS ON new cross-cutting Tier-3 item: **Effect-panel preset loader UI** with 3 approach options identified). Forward work next: \u00a710 Tape (biggest single rework in the spec set). Prior entry below.)

**Prior update:** 2026-04-18 (post-ship follow-on — **automation applicator dangling-pointer crash fix shipped** after Jeff hit it testing §7 Phaser. `SafePointer<juce::Slider>` capture in `EditorPanelBase::setSlotContext` regKnob lambda + `MixerTrackStrip::setAutomationPrefix` fader/pan blocks. Prevents crash inside `NormalisableRange::snapToLegalValue` when an effect-swap / panel-rebuild leaves stale applicators in `StandaloneEditor::mAutomationApplicators`. PRESET-SAFE. **Investigation note:** Jeff's reported "Rate knob reverts to default on loop restart" turned out NOT to be a bug - it was an accidentally-created Rate automation block running its curve. Expected behavior. Prior entry below.)

**Prior update:** 2026-04-18 (§7 Phaser Phase A retrospective shipped -- Tier 1 A1 denormal, A2 Wet smooth, A3 Stereo smooth, A4 while-wrap LFO, A5 OutGain smooth, A6 single-branch stereo-offset wrap, A7 BPM-sync Rate-knob soft-lockout (cross-apply from §4 + §3), A9 panel-construct DSP-state sync (cross-applied to Chorus/Delay/Flanger/Phaser — 18 controls total). Tier 2 C1 Slow/Fast Range REWIRED (was dead UI — now actually clamps Rate 0-2 Hz vs 0-10 Hz; PRESET-BREAK ⚠️ pre-v1 clean slate) + C2 Sync-division chicken-head (default 1/4 = v1) + C3 LFO wave chicken-head (Sine/Tri/Saw/S&H, default Sine = v1) + C4 Rate knob log-skew (UI-only) + C5 Cross-channel feedback knob (default 0 = v1); C6 Stages crossfade skipped. Tier 3 logged: T1 per-stage resonance LFO (Mu-Tron warble), T2 env-follower modulator, T3 FB filter (LP/HP/BP), T4 Barberpole mode. **All Phase A retrospectives complete — §1-§8 now fully reviewed under the Tier 1/2/3 framework.** Forward work resumes with §9 Saturation. Previous entry below.)

**Project Persistence audit + P1/P2 shipped (2026-04-24):** full audit of save/reload coverage revealed that while APVTS + rack states + PatternManager (notes / arrangement / mixer snapshot / drum flags / row mute-solo) were persisting correctly, four categories were silently lost on close + reopen:

1. Ribbon tab configuration (how many Layers/Bass/Drums tabs, tab names, page indices) - default tabs got recreated on every launch.
2. Engine selection per tab (Harmless / BaySickPlayer / BaySickSynth / BaySickBass) - pages always came up blank.
3. Engine processor internal state - each engine has its OWN `apvts` instance separate from the main `VibeSynthProcessor::apvts`; nothing bridged that state into the project XML.
4. VibePlayer sample-folder / SFZ / single-file paths (non-APVTS state inside `VibeSampleManager`) - sample assignments vanished even when everything else survived.

**P1+P2 shipped:** tab + engine-state serialization via new `<UIState>` child under the project root.
- `VibeSynthProcessor::onSerializeUIState` / `onDeserializeUIState` callbacks wired to StandaloneEditor, fired inside `serializeProject` / `deserializeProject`.  Lets the audio processor delegate UI state to the editor without a hard dependency.
- Editor's `serializeUIState` walks every LayersPage / BassPage / DrumsPage in `mPages` and writes a `<Tab type=... pageIndex=... name=... engine=... engineData=base64...>` record per tab.  `engineData` is each engine processor's full `getStateInformation` byte-for-byte - Harmless partials + modulations, VibePlayer APVTS, BaySickSynth `bss_*` params, BaySickDrums per-slot state all ride inside.
- `deserializeUIState` calls new `closeAllDynamicTabs` helper (which routes through `mRibbon->closeTab` + `onTabClosed` so slot tracking is correct), then rebuilds each saved tab via new `createLayersPageAtIndex` / `createBassPageAtIndex` (deterministic index so `layerRoll[N]` notes land on the right tab), calls `selectEngine(savedType)`, and pushes `setStateInformation` with the decoded engineData.  Post-rebuild selects the first dynamic tab (or Builder) so the user lands on a live page.
- New public getters on each page: `LayersPage::getEngineType() / getEngineProcessor()`, `BassPage` same, `DrumsPage::getDrumsProcessor()`.

**P3 shipped (2026-04-24):** VibePlayer sample-path persistence.  New `VibePlayerProcessor::loadSampleFolder / loadSampleSFZ / loadSampleFile` wrappers stash the loaded path as three non-APVTS properties on `apvts.state`: `bsp_loadKind` ("folder" / "sfz" / "file"), `bsp_loadPath` (absolute), `bsp_loadNormalize` (MIDI root for drum normalization, -1 = none).  `setStateInformation` replays whichever API was last used so the samples come back on project load.  All six VibePlayerEditor call sites (drag-drop menu + Core Library browser) + all four BaySickDrumsEditor call sites (library item menu + Load Folder + Load SFZ + drag-drop onto slot) now route through the new wrappers instead of reaching into `mSynth.getManager()` directly.  Single machine: full fidelity; cross-machine portability still requires the Bundle & Export action from §5D-BUNDLE.

**File > Open / Restore Backup state-bleed fix (2026-04-24).**  Only File > New was wiping in-memory state before loading.  File > Open and Restore-Backup paths left residual rack effects / dynamic state from the prior session showing through.  Fixes: (a) new `VibeGraph::clearAllRackStates()` iterates every rack (buses + InstrChannelNodes + every insert map) and calls `clearSlot(0..5)` — `loadRackStates(emptyTree)` never did this, it only RESTORED from tree entries so racks without matching entries kept their prior state; (b) `resetToBlankState` now calls `clearAllRackStates` instead of the no-op `loadRackStates(empty)`; (c) all four load entry points (Open, Open Recent, Restore Backup, File > New) now run `closeAllDynamicTabs() + MixerPage::clearDynamicStrips() + resetToBlankState()` before calling into ProjectManager; (d) new `MixerPage::clearDynamicStrips()` wipes all Layer / Bass / Drum / Audio / Aux / Vox / Inst strip maps (mixer strips persist independently of tabs; the tab wipe wasn't enough).  File > New already had (a)+(b)+(c); it now benefits from (d) too.

**Phase C §P4.2 Batch 1+2 shipped (2026-04-24).** Foundation layers for Drums dual piano roll mode.
- *Batch 1 (C1 Pattern data refactor)*: `PianoNote::slotIndex` field already existed + serialized.  Added migration in `PatternManager::fromValueTree` for old drumRoll notes (slotIndex=-1 + midiNote in [36..51]) → `slotIndex = 51 - oldMidiNote`, `midiNote = 60`.  Both Song-mode + Pattern-mode drum playback paths in `PluginProcessor` now prefer `note.slotIndex` (legacy fallback retained).  Full-roll semantics: stored midiNote != 60 triggers the slot's engine at that pitch; midiNote == 60 OR legacy notes use the slot's T12-remapped native MIDI note.
- *Batch 2 (C2 dual-mode plumbing + creation tagging)*: New `RollMode { Standard, DrumGrid, FullRoll }` enum on both `PianoRollContainer` and `PianoRollGrid`.  Container default = Standard (Layer/Bass keep current behaviour); DrumsPage explicitly calls `setRollMode(DrumGrid)` in its piano-roll tab build.  `PianoRollGrid::tagLastCreatedNote(rowMidi)` runs after every user-driven `notes.push_back` (Draw / Paint / Stamp) and stamps `slotIndex` based on row (DrumGrid) or active slot (FullRoll).  `setActiveSlot(int)` sets which slot's notes are visible in FullRoll mode.  Main paint loop skips notes whose `effectiveSlot` doesn't match `mActiveSlot` when in FullRoll mode.

- *Batch 3 (C5 mode toggle + C6 slot selector + DrumGrid render by slot + C3 click-non-C5 no-op)*: New `SplitTabButton` (in `SharedUI.cpp`, anonymous namespace) is a `TextButton` subclass with a right-edge arrow zone — body click hits the inherited onClick, arrow click fires a separate callback.  New `PageMenuBar::setTabSlotArrow(idx, onArrow, getDynamicLabel)` upgrades an existing tab slot button into a SplitTabButton; the dynamic-label callback polls each paint so external state changes refresh the label.  `StandaloneEditor` calls `setTabSlotArrow(1, ...)` on the Drums Piano Roll tab — body click activates the tab as usual, ▾ click opens a Drum Grid / Full Piano Roll PopupMenu that calls `dp->setDrumRollMode(...)`.  Button label reflects current mode ("Drum Grid" / "Full Piano Roll").  Slot selector: Drums' nav combo `onChange` now also sets `dp->setDrumActiveSlot()` which propagates to both `mDrumRoll->setActiveSlot()` (piano roll filter) and `mDrumsEditor->setActiveSlot()` (Sound + EQ tabs share the slot).  New `displayMidiForNote(note)` helper on `PianoRollGrid`: in DrumGrid mode, returns `(51 - effectiveSlot(note))` regardless of stored midiNote — so non-C5 notes (created in FullRoll) render as lit cells on the slot's row.  Main note render loop + ghost render loop both use it.  `eraseAt` handles C3 click-non-C5 no-op: in DrumGrid, only notes with `midiNote == 60` are erasable (non-C5 notes left alone, must remove from FullRoll mode); in FullRoll, only notes whose `effectiveSlot == mActiveSlot` are erasable.

**Arrangement block fractional length (2026-04-24).**  Added `float lengthBeats` to `ArrangementBlock` (default -1 = "use `lengthBars * 4`" for back-compat).  `effectiveLengthBeats(block)` / `effectiveLengthBars(block)` free helpers in `PatternManager.h` handle the "prefer the precise length" selection.  `commitRecordingResult::dropWavAsClip` now sets `lengthBeats = fileBeats` (exact recording duration).  `lengthBars` stays rounded-up for bar-aligned arrangement UX.  Song-end calc (`onGetLoopBeats`) / audio-clip playback bounds (`rebuildAudioClipPlayers`) / arrangement grid visuals (block width, playhead-over-block check, rightmost-edge calc, per-block progress) all switched to `effectiveLengthBars / Beats` so recordings end at their real last sample.  Manual edge-resize of an audio block in the grid clears `lengthBeats = -1` so the user's new bar count becomes authoritative.  Song-end calc also lost the earlier `ClipType != Pattern continue;` filter (wrongly copied from `getEffectivePatternLoopBeats`), so Audio + Automation blocks now properly contribute to Song-mode end-of-song and loop wrap.

**Persistence Cycle 1+2 (2026-04-24) — rack + lazy APVTS + Cycle-2 gaps.**

- *saveRackStates / applyRackStates extended*.  Before: only 5 fixed-bus racks + dynamic InstrChannelNode racks were written.  After: every per-insert rack (Layer / Bass / Drum / Audio / Aux / Vox / Inst InsertNodes) + the three special-bus racks (ClipsBus / VoxBus / InstBus InstrChannelNodes) are serialized under `<VibeRackStates>` via new `<InsertRack kind="..." index="..." rack="..." eq="..."/>` and `<BusRack id="ClipsBus|VoxBus|InstBus"/>` entries.  `saveRackStates` now covers all 100+ racks; before this, most of them silently dropped on save.
- *Deferred rack-state replay*.  `deserializeProject` runs `loadRackStates` once for fixed buses (they already exist at load time) AND stashes the rack-state tree in `mPendingProjectRackState`.  After the editor finishes `deserializeUIState` + `restoreAudioStripsFromArrangement` (which create every Layer/Bass/Drum/Audio InsertNode), the editor calls `mProcessor.applyPendingRackStates()` which re-applies the stash — the per-insert racks now have their targets and restore cleanly.
- *Drum mixer strip restore on load*.  `BaySickDrumsEditor::onSlotChanged` only fires on user interaction, so on project load the drum mixer strips never got created even though the processor's per-slot state was restored correctly.  `deserializeUIState` now walks `BaySickDrumsProcessor::getSlotName(slot)` for the restored Drums tab and calls `MixerPage::addDrumChannel` for every non-empty slot.
- *APVTS lazy-param binding fix*.  `AudioProcessorValueTreeState::replaceState` binds each parameter adapter to its matching tree child via `valueTreeRedirected` → `updateParameterConnectionsToChildTrees` — but only once, synchronously.  Mixer-strip params are lazily registered via `ensureMixerStripParams` from inside the editor's load callbacks, AFTER replaceState ran, so their adapters existed but were never tree-bound and stayed at constructor defaults (fader, pan, mute, polarity, width, sendTo, sends, EQ bands all lost).  Fixed in `VibeSynthProcessor::applyPendingRackStates`: calls `apvts.replaceState(apvts.copyState())` before restoring rack state, which re-triggers the redirect callback after every lazy param is in place.
- *Cycle 2 gap fills*.  `<UIState>` now also carries `<Metronome>` (volume / soundType / enabled / countInBars — the last pulled from `StandaloneEditor::mCountInBars`), `<VUCalibration dbfs="...">` (was only a session-global static), `<SongLoop on="...">` (project-scoped play-through-vs-loop toggle), and `<AuxNames>` / `<VoxNames>` / `<InstNames>` (per-strip custom name for user-renamed strips; default-named strips are skipped).  Restoration fires inside `deserializeUIState` — new `MixerPage::setAuxStripName / setVoxStripName / setInstStripName` push saved names into created strips.  `MixerTrackStrip::canRename` extended to include Aux / Vox / Inst (was just Layer / Bass channel strips) so the double-click-to-rename affordance works on user-creatable strips.

**File>New param-reset double-normalise fix (2026-04-24):** `VibeSynthProcessor::resetToBlankState` was wrapping `getDefaultValue()` in a second `convertTo0to1` call.  JUCE's `RangedAudioParameter::getDefaultValue` already returns a normalised 0..1 value (it calls `convertTo0to1` internally on the raw default), so the extra wrap double-normalised the result - Int params with large ranges collapsed toward zero.  Most visibly, every mixer strip's `_sendTo` (Int 0..999) became 0 after File > New, routing all audio to kOutput (terminal sink that only Master is supposed to write to) instead of its natural bus.  Drums were silent on File > New because their routing edges were broken.  Layers / Bass had the same bug but it was less noticeable - still silent on the bus-routed path, just not under a simple sanity check.  Fixed by passing `getDefaultValue()` straight through `setValueNotifyingHost`.

**Global tempo + recorded-clip library + tempo automation (2026-04-24):** four related fixes.
- **Audio library registration for recorded clips**: `commitRecordingResult::dropWavAsClip` now calls `mPM->addAudioToLibrary(relativePath)` alongside `addBlock`, matching what `BuilderPage::importAudioFile` does on user drop.  Without this, recorded clips showed as arrangement blocks but weren't in the Browser's Audio tab and their sample reference didn't survive save/reload.
- **Per-pattern `Pattern::tempo` removed**: vestigial field, never read for playback, misleading the save/load round-trip.  FL-style: tempo is a single project-level value.  Old projects with `<Pattern tempo="..."/>` silently drop the attribute on load.
- **Global tempo**: new `PatternManager::mGlobalTempo` (default 120.0) + `getGlobalTempo` / `setGlobalTempo` accessors + `globalTempo` attribute in the top-level `toValueTree` / `fromValueTree`.  `GlobalTransportBar::onTempoChanged` now writes both to `mPlayHead` AND to `mPM->setGlobalTempo`.  Post-load helper `syncTempoFromPatternManager` pushes the saved tempo back into the playhead inside `restoreAudioStripsFromArrangement` (which runs at the end of every load path).  `PatternManager::reset()` also resets it to 120.
- **Tempo automation (FL-style)**: new `global_tempo` paramId with applicator + reader registered in `StandaloneEditor` ctor.  Linear 0..1 <-> 20..300 BPM map; applicator updates playhead + `mPM->setGlobalTempo`; reader normalises current playhead BPM.  New `GlobalTransportBar::onAutomateTempo` callback fired on right-click of the BPM `TextEditor` (via persistent `BpmMouseWatcher` MouseListener + `setPopupMenuEnabled(false)`).  Editor wires that to `openEventEditorForParam("global_tempo")` which drops an Automation clip on the arrangement and opens the Event Editor - same flow every other automatable param uses.  Tempo-render-to-WAV path in `BuilderPage` switched from `pat2.tempo` to `mPM.getGlobalTempo()`.

**P4 shipped (2026-04-24):** UI-state persistence.  `<UIState>` now carries `activeTabId` + `mixerScrollX` attributes and an `<Arrangement ppBar barOff selStart selEnd>` child.  On load, the saved active tab is selected (falls back to first dynamic tab / Builder if the id no longer resolves); Mixer restores its horizontal scroll via new `MixerPage::getScrollX / setScrollX`; the Builder arrangement grid restores zoom, horizontal scroll, and time selection via new `ArrangementGrid::setTimeSelection` + direct read/write of the already-public `mPPBar` / `mBarOff` members.  Piano-roll per-tab scroll/zoom/tool deferred - the PianoRollContainer state (mPPB/mBeatOff/mTopNote) is internal and would need a wider setScrollState API; low priority for v1.

**P1+P2 follow-ups (2026-04-24, same day):**

- **Bug: phantom default tabs after load.**  `RibbonTabBar::closeTab` refuses to remove Drums tabs (hardcoded) AND refuses to remove the last instance of any Layers/Bass type - correct for user-initiated closes, wrong for project load.  Symptom: after reopening a saved project, the ribbon showed extra empty "Layers"/"Bass"/"Drums" tabs alongside the restored ones; Drums specifically was blank because its default ribbon tab never got wiped but its `mPages` entry was.  Fixed by new `RibbonTabBar::clearAllDynamicTabs()` that bypasses those guards; `StandaloneEditor::closeAllDynamicTabs` now drives the ribbon wipe through that path instead of repeated `closeTab` calls.
- **Bug: File > New kept prior project's state.**  `newProject` created the folder + empty project.xml but never cleared in-memory APVTS / rack / PatternManager / tab state.  Fixed by new `VibeSynthProcessor::resetToBlankState()` (iterates every registered param and resets to default, loads an empty `<VibeRackStates>` into VibeGraph, calls `PatternManager::reset()` which is also new - single empty default pattern + cleared arrangement + cleared mixer + cleared row/drum flags + cleared automation templates).  `doFileNew` now calls `closeAllDynamicTabs()` + `resetToBlankState()` BEFORE optionally applying a template, then either lets the template's deserializeUIState rebuild tabs or calls new `addDefaultDynamicTabs()` helper to put the three blank-engine defaults back.  `addDefaultDynamicTabs` is split out of `buildDefaultTabs` (which also creates the system Mixer/Effects/Builder entries and can only run once at startup).
- **Bug: F4 master-bus declick silenced audition.**  `mMasterFadeGain` defaulted to 0 and only ramped to 1 when `pos.getIsPlaying()` was true.  Audition fires voices with the transport stopped, so the final buffer was multiplied by 0 and silenced.  Reverted F4 entirely - Play/Stop click is small enough to live with for now; engine voice envelopes already handle most of it.  The `mMasterFadeGain` member stays in the header in case a smarter "any-voice-active" guard lands later.

**Project Bundle & Export (planned; added 2026-04-24 to plan §5D-BUNDLE):** explicit File menu action to copy every referenced sample (BaySickPlayer / BaySickDrums SFZ + WAV roots, Builder clips, recordings) into a target folder or .zip, with `project.xml` rewritten to relative `Samples/{filename}` paths so the bundle is portable across machines. Decision locked with Jeff 2026-04-24: no auto-bundle on load (disk bloat); bundle is opt-in only. Load-path resolution changes to try absolute first, then `{Project}/Samples/`, then Core Library, then fail loudly — preserves back-compat for existing projects while making bundles "just work" on any machine. Full spec in `C:/Users/jeffm/.claude/plans/lucky-discovering-tiger.md` §5D-BUNDLE. v1 target-folder only; .zip deferred.

---

### Phase C §P4.2 — Drum Dual Piano Roll (2026-04-24, partially shipped, SUPERSEDED by Phase D1)

Initial attempt at giving drums two piano-roll modes:
- **Drum Grid mode**: 16 fixed rows = 16 drum slots, native pitch (C5) per row, slot-name labels in keyboard column.
- **Full Piano Roll mode**: arbitrary pitches per slot, single active slot at a time, full chromatic keyboard.

**Encoding refactor (PATTERN-BREAK):** added `PianoNote::slotIndex { -1 }` so a note's slot identity (which drum) is independent of its `midiNote` (which pitch the engine plays).  Migration path on load: legacy notes with `slotIndex < 0` and `midiNote ∈ [36..51]` get `slotIndex = 51 - midiNote, midiNote = 60`.  Saves carry both fields so the migration detection stays correct on round-trip.

**B1 + B2 + B3 shipped:** RollMode enum (Standard / DrumGrid / FullRoll), `setRollMode` / `setActiveSlot` plumbed through `PianoRollContainer` + `PianoRollGrid`, `tagLastCreatedNote` stamps `slotIndex` from row-or-active-slot, `effectiveSlot()` + `displayMidiForNote()` helpers, FullRoll filter in main paint loop (only show notes for active slot), DrumGrid keyboard column shows slot labels, dual-mode picker on the Piano Roll tab's split-button arrow ("Drum Grid" / "Full Piano Roll" with dynamic label).  `BaySickDrumsProcessor::scheduleSlotTrigger(slot, pitch, vel, smp)` API for FullRoll-pitch override on the legacy 16-slot kit.  PluginProcessor's drum dispatch for FullRoll notes → `scheduleSlotTrigger` (engine retunes its sample / synth to the user-chosen pitch); drum-grid + legacy notes use the standard MIDI route at slot's playPitch.

**Bugs caught + open issues at the time of pivot:**
- Audition pitch retune via new `BaySickDrumsProcessor::auditionSlotAtPitch(slot, pitch)` + `mAuditionPitch` atomic — needed because regular `auditionSlot` always used slot's playPitch.
- Notes placed in DrumGrid mode appeared at low rows (C2..D#3 = grid encoding) when viewed in FullRoll mode — confirmed the grid encoding leaked into the piano-roll view because `midiNote` stored the row, not C5.  Encoding is the bug: should stamp `midiNote = 60` so the slot identifies the drum and midiNote is the played pitch (60 = native).
- Edit ops (Ctrl+A, marquee, arrow nudge, drag) ignored the `effectiveSlot` filter — selected/moved notes from invisible (other-slot) drums.

**Why superseded by Phase D1:** Jeff's reading of the architecture identified the root cause — the "one player, 16 slots" model was the source of the bug class.  The fix was to restructure rather than patch: each drum becomes its own engine instance (BaySickPlayer / BaySickSynth) with its own piano roll, eliminating the slotIndex hack entirely.  Phase D1 below replaces all of §P4.2's slot-tagging machinery; the slotIndex field stays in PianoNote for legacy migration but is no longer authored by the new DrumPage.

### Phase D — Dynamic-Drum Architecture (2026-04-24 → 2026-04-25)

Each drum tab is now a fully independent engine instance, exactly like Layers / Bass.  The legacy `BaySickDrumsProcessor` (one processor wrapping 16 internal slots), `DrumsPage` (one page with sub-tab Sound/PianoRoll/EQ), `BaySickDrumsEditor` (16-slot kit grid UI), `BaySickDrumsLAF`, `DrumSynth` (46-voice procedural drum library), and `DrumVoice` (single-voice helper struct) are all gone.  Drums use the same dynamic-engine machinery as Layers/Bass: each tab carries one `BaySickPlayer` (sample) or `BaySickSynth` (synth preset) engine.

#### D1.1 — Data model foundation

- `kMaxDrumPages = 16` constant in `VibesynthConstants.h`.
- `Pattern::drumRolls[16]` per-drum `PianoRollData` array (alongside legacy single `drumRoll` for migration).
- New persistence tag `<DrumPageRoll page="N">` saved per non-empty drum roll.
- `kDrumPRTarget = kMaxLayerPages + kMaxBassPages` constant for `mPRPendingOffs` target IDs (drum dispatch routes through this range).
- One-time migration in `PatternManager::fromValueTree`: legacy `drumRoll.notes` get folded into `drumRolls[slotIndex]` when `drumRolls[*]` is empty, with `slotIndex` cleared on the migrated copy.  Legacy `drumRoll` stays populated for back-compat detection.
- `currentPatternLoopBeats` scan extended to include `drumRolls[]` so loop-end calculation covers per-drum-tab notes.

#### D1.2 — PluginProcessor scaffolding

- `mDrumEngines[kMaxDrumPages]` + `mDrumEngineLock` + `mDrumEngineBuf` + `mDrumEngineScratch` buffers in `VibeSynthProcessor`.
- `registerDrumEngine(idx, eng)` / `unregisterDrumEngine(idx)` mirror `registerLayerEngine` — also creates the Drum InsertNode + mixer strip params.
- `drumPageMidi[kMaxDrumPages]` per-drum MIDI buffers in `processBlock`, included in flush + AllNotesOff.
- Per-drum-page note scheduling in both Song mode and Pattern mode (parallel to layer/bass loops).  All four `mPRPendingOffs` dispatch sites handle `kDrumPRTarget` range.
- New per-drum render loop runs each `mDrumEngines[i]->processBlock` through its Drum InsertNode + `routeInsertOutput`.
- **Fast-path bypass:** `mAnyDrumPageActive` atomic set by `register/unregisterDrumEngine`.  When false, the entire D1.2 audio-thread path skips with a single atomic load.  Without this guard, the loop iterating 16 empty MidiBuffer constructions per block + per-drum dispatch caused noticeable CPU jitter during the transition before any DrumPage was created.

#### D1.3 — DrumPage class (no ribbon yet)

`Source/Standalone/DrumPage.h/.cpp` mirrors `LayersPage` exactly.  Per-drum APVTS prefix `drm_{N}_*`; drum InsertNode at `mixer_drum_<N>_*` (reusing existing `InsertKind::Drum` range 500..515 → see `MixerChannelIds`).  3 sub-tabs: Player / Piano Roll / Pre EQ8 M/S.  PianoRollContainer bound to `drumRolls[mPageIndex]`.  `VC::DrumCol[16]` palette added in `SharedUI.h` (16 sequenced shades of red, no orange/pink — matches Phase C Batch 5 spec).  At this point the class compiled standalone but wasn't instantiated by the ribbon.

#### D1.4 — Cutover (ribbon wiring + DrumPage default)

- `addDefaultDynamicTabs` creates one `DrumPage` (idx 0) instead of one `DrumsPage`.
- New `createDrumPage` / `createDrumPageAtIndex` factories + `mUsedDrumIndices` member in `StandaloneEditor`.
- `onAddTabRequest` extended to accept `TabType::Drums` (previously rejected for anything but Layers/Bass).
- Ribbon's Drums dropdown switched from `showSubPageDropdown` (legacy "Sound | Piano Roll | EQ" navigator) to `showInstanceDropdown` (Layers/Bass-style: instances + Pages + Rename / Delete / + Add New Drum).
- Badge count uses `countTabsOfType(Drums)` instead of hardcoded 2.
- `closeTab` allows Drums type (was `if (type != Layers && type != Bass) return;`).
- Sub-page dispatch (`onSubPageSelected`) walks `mPages` to find the active DrumPage of the chosen type; legacy `mLegacyDrumsPage` stale-pointer dispatch removed.
- Spawn-duplicate helpers: `spawnDuplicateDrumTab(xml)` (mirrors legacy DrumsPage's slot duplicate, but spawns a brand-new drum tab cloned from the source state).
- Tab name auto-rename: when a sound is loaded on a drum, `onSoundNameChanged` callback renames the ribbon tab + mixer strip + tab's own context label.  `MixerPage::renameChannel` extended to handle `mDrumStrips`.

#### D1.4-fix(a) — Picker popup (sound browser)

Replaced the 4-engine combo on the drum's Player tab with a single `[Pick a sound ▾]` button that opens a hierarchical popup matching the legacy `BaySickDrumsEditor::showSoundPicker`:
- **Sample submenu**: Browse sample folder... / Load SFZ file... / Core Library walker (`SampleLibrary::isDrumPack` filtered).  Picks → `selectEngine("BaySickPlayer")` + `loadSampleFile/Folder/SFZ` + `normalizeRootNotes(60)` (drums always trigger at C5).
- **Synth Patch submenu**: `+ New Patch (Blank)` (loads BaySickSynth at APVTS defaults, names tab "User Patch") + factory presets organized by subfolder section headers (TR-808, TR-909, Simmons, etc.) + `Save Current Patch As...` at bottom.  Picks → `selectEngine("BaySickSynth")` + apply XML state with `tk_lay_0_bss_*` → `tk_drm_N_bss_*` prefix substitution.
- Picker dispatches synth vs sample preset by XML root tag (`BaySickSynthState` vs `BaySickPlayerState`).
- `selectEngine` made swap-aware (no longer one-way locked) so re-picks tear down old engine + create new properly.
- Folders sort BEFORE files at every level of the Core Library tree.
- `addLibDirToMenuDP` mirrors legacy `addLibDirToMenu` exactly: subfolder → submenu, .wav/.aiff/.aif/.flac → individual clickable item, .sfz → clickable with "  [SFZ]" suffix.  No "load folder" group items.

#### D1.4-fix(b) — Save Patch As infrastructure (BaySickPlayer + BaySickSynth)

- `DrumPage::savePatchAs` writes XML preset to `Documents/BaySickDAW/Presets/BaySickDrums/My Presets/{name}.xml`.
- For BaySickSynth: just the engine's `apvts.copyState().createXml()`.
- For BaySickPlayer: wraps the engine's apvts state + a `<Sample kind="file|folder|sfz" path="..."/>` reference.  Sample path is stored relative to Core Library when the file lives there (`library:relative/path.wav`), absolute otherwise — same reference convention as `ProjectManager::importSample`'s `Samples/<filename>` model.  Bundle & Export will follow the `library:` prefix at bundle time to copy the referenced sample into the bundle's `Samples/` folder + rewrite the path.
- `loadPlayerPreset` mirror: detects engine from XML root tag, applies apvts (with prefix substitution), restores sample via the right `loadX` method.  Missing sample → notice + load with empty sample slot.
- `mLoadedSampleKind` + `mLoadedSamplePath` member fields on `DrumPage` track current sample for save-time reference.  Cleared on engine swap + clearSound + new patch.

#### D1.4-fix(c) — Right-click context menu + lock-after-pick (Drums / Layers / Bass)

After first sound pick, the picker button transforms — label = current sound name (with `[L]` prefix when locked); both left-click AND right-click open the per-drum context menu.  No accidental re-pick possible.  To get a different sound: delete the drum (via context menu) and add a new one.

**Per-drum menu**: Lock Drum (toggle) / Polyphony toggle / — / Copy Drum / Paste Drum / Duplicate Drum (new tab) / — / Choke Group ▸ (D3 placeholder) / MIDI Map ▸ (D1.5 placeholder) / MIDI Note ▸ (D1.5 placeholder) / — / Save Current Patch As... (BaySickSynth + BaySickPlayer only) / — / Delete Drum (with confirmation prompts).

**Per-layer menu** (Layers + Bass parity): Lock / Polyphony / Copy / Paste / Duplicate / Choke Group ▸ (D3 placeholder) / Save Current Patch As... / Delete.  Engine combo replaced with `LockableCombo` subclass that intercepts clicks once locked → routes to context menu instead of opening the engine dropdown.  After first engine pick, `LockableCombo::locked = true` and click → `onLockedClick` → `showContextMenu`.  Per-engine Save Patch As writes to `Documents/BaySickDAW/Presets/{EngineName}/My Presets/{name}.xml` (works for Harmless / BaySickPlayer / BaySickSynth / BaySickBass — uses the engine's own `getStateInformation` blob).

**Polyphony toggle is engine-aware**:
- BaySickSynth → `_bss_voiceMode` (4-choice Poly/Mono/Lead/Legato; menu toggles Poly↔Mono only).
- BaySickBass → `_bsb_voiceMode` (same family).
- BaySickPlayer → `_bsp_voiceCap` (Int 1-16; menu toggles 1↔8).
- Harmless → polyphonic-only, shows "Polyphony: (n/a)" disabled.

**Tab name auto-rename** with **User Patch save prompt**: Sound load auto-renames tab to file name (e.g. "808 Kick").  `+ New Patch (Blank)` names tab "User Patch".  Manual rename via the dropdown still works (doesn't touch the source preset/sample).  When the current name is "User Patch" and the user clicks Rename, ribbon's `onRenameInterceptRequested` callback fires → DrumPage's `savePatchAs` opens with prompt "Renaming this drum saves it as a preset.\nEnter a name:" → on save, the patch is written + tab + mixer + context label all rename to the saved name.

**Delete confirmation prompts**:
- For saveable engines (BaySickSynth or BaySickPlayer): "Save this drum as a preset before deleting?\n(Engine settings will otherwise be lost.)" → Save & Delete / Delete Anyway / Cancel.
- For non-saveable: "This action cannot be undone. Delete this drum?" → Delete / Cancel.
- Refuses to delete the only Drums/Layers/Bass tab (matches legacy behaviour for Layers/Bass): "Cannot Delete\nThis is the only Drum tab. Add another first."  Driven by new `RibbonTabBar::isLastOfType(type)` helper.

**onDeleteRequested simplified**: previously double-fired cleanup (`onTabClosed` + `closeTab` which itself fires `onTabClosed` via callback).  Now just calls `mRibbon->closeTab(tabId)` and lets the ribbon fire `onTabClosed`.  Fixed an orphaned-tab crash where the page got deleted but the ribbon entry stayed.

#### Legacy sweep (2026-04-25)

Files DELETED:
- `Source/DrumSynth.h` + `.cpp` (DrumSynth class + DrumVoice struct + 46 procedural drum voice processKickThump/processSnareCrack/etc.)
- `Source/Standalone/DrumsPage.h` + `.cpp`
- `Source/BaySickDrums/BaySickDrumsProcessor.h` + `.cpp`
- `Source/BaySickDrums/BaySickDrumsEditor.h` + `.cpp`
- `Source/BaySickDrums/BaySickDrumsLAF.h`
- (`Source/BaySickDrums/` directory now empty — kept for now since `Presets/BaySickDrums/` still uses the brand name.)

Code stripped:
- `CMakeLists.txt`: removed DrumSynth.cpp + BaySickDrumsProcessor.cpp + DrumsPage.cpp + BaySickDrumsEditor.cpp from sources; removed BaySickDrums from include dirs (both shared + standalone targets).
- `PluginProcessor.h/.cpp`: removed `mDrumSynth` field, `getDrumSynth()` accessor, `mDrumsEngine` + `mDrumsEngineLock` + `mDrumsEngineBuf`, `kNumDrumSlots` + `mDrumSlotBufs`, `addParamsForBaySickDrums`, `register/unregisterDrumsEngine`, basic-step-sequencer drum trigger, drum-page render loop's legacy fallback, `drumsPageMidi`, "BaySickDrums" branch in `registerParamsForTrack`, all references in `prepareToPlay` + `processBlock`.
- `VibeGraph.h/.cpp`: removed `DrumSynth&` parameter from `buildFixedTopology`, removed `drums` field + `DrumSynth&` ctor param from `DrumsBusNode`, removed legacy `drums.renderNextBlock` fallback (drum bus is silent when `preRendered` is null — per-drum-tab InsertNode outputs are the only source now), removed `#include "DrumSynth.h"` + forward-decl.
- `StandaloneEditor.h/.cpp`: removed `DrumsPage` forward decl, `createDrumsPage` decl/def, `mLegacyDrumsPage` field + all clears, all `dynamic_cast<DrumsPage*>` branches (sub-tab UI / serialize / deserialize / lookupPageTabName / getActivePianoRollForLoop / onTabSelected LastRollKind / showPageForTab).  Legacy "Drums" type tab in `deserializeUIState` now silently skips (with comment that PatternManager already migrated the notes into drumRolls[slot]).
- VibePlayer/PianoRoll/SharedUI: only stale comments referencing the deleted classes — left as historical breadcrumbs, no code touches.

#### D1.4 ancillary fixes (2026-04-25)

- **Audio settings dialog path fix**: `AudioSettingsDialog::applySettings` was hard-coded to write `audio_settings_pending.xml` to `userApplicationDataDirectory` (Roaming).  After the P4b migration moved `audio_settings.xml` to `Documents/BaySickDAW/`, the startup loader looked for the pending file as a sibling of the live file in Documents — never saw the Roaming pending file.  Result: device changes appeared "stuck" — Apply + Restart loop didn't switch the device.  Fix: dialog now writes the pending file as a sibling of `VibesynthStandaloneApp::getAudioSettingsFile()` (the single source of truth).  Made `getAudioSettingsFile()` public for cross-file access.
- **TR- prefix removed from drum preset names**: `Tools/gen_factory_presets.py` regex-substituted `"TR-NNN "` → `"NNN "` in DRUM_RECIPES + DRUM_CATEGORIES (folder names like `TR-808/` stay as section labels in the picker; only individual preset names changed).  Cleanup pass at script start removes any leftover `TR-*.xml` files from factory folders (skips `My Presets/`).
- **Master output volume parity (BaySickSynth + BaySickBass)**: added `_bss_outVol` (Float 0-1, default 0.8 = ~ -1.94 dB) param.  Cached in `ParamCache::outVol`, read in `updateFromApvts`, applied via `buffer.applyGain()` after `mSynth.renderNextBlock()`.  Same default as `BaySickPlayer`'s `volume` param.  Closes the parity gap that made BaySickSynth-based drums + Layers/Bass with synth/bass engines audibly hotter than equivalent BaySickPlayer paths.  No editor knob yet — defer to follow-up batch.

#### D2 — Drum Kit tab (2026-04-25)

New first sub-tab on every DrumPage.  Composited 16-row grid view that shows every drum tab in the project at once — beginner-friendly "drum machine" entry point that complements (does not replace) the per-drum Piano Roll sub-tab.  All DrumPage instances render the same DrumKitView content (provider callback walks `mPages` filtered to TabType::Drums, ribbon order).

##### Batches 1-4 — Foundation, audition, mute/solo, drag-reorder rows
- `DrumKitView` component owns the tab, sits as sub-tab index 0 (default landing for new drums).  `DrumPage::switchTab(0)` = Drum Kit; index shifted Player→1, PianoRoll→2, EQ→3 across all DrumPages.  Ribbon dropdown sub-pages list updated to match (`-10` Drum Kit / `-11` Player / `-12` Piano Roll / `-13` EQ in `RibbonTabBar::showInstanceDropdown`).
- Per-row layout: drag-reorder handle (3 horizontal bars, 14 px) + sound picker button (120 px, opens the same picker popup as the Player tab) + Mute (`MixerLedButton`, red LED, 20 px) + Solo (`MixerLedButton`, yellow LED, 20 px) + audition piano key (28 px white rectangle, darkens while held) + grid area filling the rest of the row.
- Mute / Solo bind lazily to `mixer_drum_{N}_mute` / `_solo` via `ButtonAttachment` so the kit-tab toggles drive the same APVTS params the per-drum mixer strip uses.  Re-bound on every `refresh()` (cheap, handles row reorder + tab swap).
- Audition keys use the press-and-hold pattern: `mAuditionOn(rowIdx)` on mouseDown, `mAuditionOff(rowIdx)` on mouseUp / mouseExit.  `StandaloneEditor::wireDrumPageKitView` walks the per-drum Player engine to dispatch noteOn/noteOff to the right BaySickPlayer / BaySickSynth / Harmless.
- Drag-reorder:  `mReorderHandler(srcRow, dstRow)` callback fires `StandaloneEditor::moveDrumTab(src, dst)` which reorders both `mPages` and the ribbon's tab list (via new `RibbonTabBar::moveTabOfType`), then refreshes every kit view.  Active-row border (drum's accent colour) makes the active drum tab visually obvious in the kit list.
- Active drum is whichever DrumPage tab is currently selected in the ribbon — shown via the row border, not via grid contents.  Provider callback fires every kit-view refresh so all open DrumPage instances stay in sync after add / remove / rename / select.

##### Batch 4.5 — Selection + clipboard + Option A piano-roll-style notes
The kit grid was first prototyped as a step-sequencer (cell click toggles a fixed-duration note at C5).  Pivoted mid-batch to **Option A — full piano-roll behavior** so the Drum Kit tab is a real piano roll for all 16 drums simultaneously, not a separate sequencer paradigm.

**Follow-up rewrite (2026-04-25, same session)** — Option A's first pass still divided the grid by `totalCells` (cell count derived from `drumRolls[0].numBars * 16`) instead of the pixels-per-beat (`mPPB`) + beat-offset (`mBeatOff`) model the rest of the project's piano rolls use.  Result was a stretched-cell sequencer look that auto-fit to width and was hard-locked at 2 bars.  Rewrite ports the kit grid to PianoRollGrid's coordinate model:
- New state: `mPPB` (default 80, range 4-400) + `mBeatOff` (beats) + `juce::ScrollBar mHScroll` member.  Ctor wires `addListener(this)` and the class implements `juce::ScrollBar::Listener::scrollBarMoved` with a `mPushingToBars` re-entry guard so `pushScrollState()` never re-triggers the listener.
- New helpers: `beatToX(beat)` / `xToBeat(x)` map screen pixels through `mPPB` + `mBeatOff`; `getTotalBeats()` returns `max(pattern.numBars * 4, lastNoteEnd + 4)` over all `drumRolls[0..15]` so the canvas auto-extends past the pattern length when notes are placed late, mirroring `PianoRollContainer::pushScrollStateToBars`.
- Multi-level zoom-adaptive grid lines (1/32 / 1/16 / 1/8 / 1/4 — only drawn when pixel spacing >= 5 px) plus full-height bar lines, copied from PianoRollGrid.  Bar number labels live in `paintOverChildren` so they sit above scrolled note overflow.
- Notes render with the same 3D bevel + top-highlight + bottom-shadow + rounded-rectangle outline pattern as PianoRollGrid.  Selection = white double-outline (subtle outer glow + inner stroke).  Kit-specific: white retune dot (top-right) for non-C5 notes is preserved.  Mute = desaturated/dim fill, again matching PianoRollGrid.
- Wheel: Ctrl+wheel zooms anchored on the cursor (re-anchors `mBeatOff` so the beat under the cursor stays put); plain wheel pans horizontally ~2 beats per notch.  Vertical scroll is irrelevant (rows are fixed).
- Layout: 14 px ruler at top, 12 px scrollbar at bottom, row area = `(height - rulerH - scrollbarH) / 16` per row.  `rowHeight()` clamps to >=24 px so 16 tall blocks always render at usable size.  Sidebar columns (handle / picker / mute / solo / audition key, 14+120+20+20+28 = 202 px) unchanged.
- Snap honors `pattern.drumRolls[0].snapDenominator` so the kit grid's snap matches whatever the per-drum Piano Roll sub-tab is set to.
- All 4-arg `noteRectFor(row, note, gridRow, totalBeats)` callers updated to the new 2-arg `noteRectFor(row, note)` (uses `mPPB` / `mBeatOff` internally).  `cellAt` 3-arg helper removed; `hitTest` now takes a single `outRow` since column index is no longer meaningful.
- `pushScrollState()` is called from `resized()`, `refresh()`, and `mouseWheelMove` so the scrollbar's range and visible window track the live pattern + zoom state.

**Second follow-up rewrite (2026-04-25, same session) — match PianoRollGrid behavior, not just data model.**  First follow-up ported coordinates (mPPB / mBeatOff / scrollbar) but missed several visible PianoRollGrid behaviors.  User caught these on test and asked me to confirm the plan before another rebuild.  Surgical fixes:
1. **Viewport-relative zoom limits**: replaced static `kMinPPB = 4 / kMaxPPB = 400` with `minPPB() = vpW / (4 * 8)` and `maxPPB() = vpW / (4 * 1)` helpers (8 bars at full-out, 1 bar at full-in — matches `PianoRollContainer::applyZoom`).  Re-clamp `mPPB` in `resized()` because limits are viewport-relative and change with width.
2. **Playhead style**: replaced 1 px white line with `VC::Green` 2 px line in body + `VC::Green.withAlpha(0.9f)` triangle arrow at the top of the ruler.  Arrow lives in `paintOverChildren` so it sits above the ruler.  Direct copy from `PianoRollGrid::paint` lines 1832-1854.
3. **Active-row indicator on picker only**: dropped the 2 px `g.drawRect (gridRowBounds, 2)` from grid paint.  Now the indicator is drawn in `paintOverChildren` as a 2 px accent border around the picker `TextButton`'s bounds (after children paint, so it overlays the button outline).  No marking on the grid row itself.
4. **Grid lines fill the full viewport**: removed both `if (beat > totalBeats) break;` clips from the multi-level grid loop and the bar-line loop.  Lines now extend across the entire visible width regardless of pattern length, matching PianoRollGrid's behavior.
5. **Default placement reverted to 1/16 note (0.25 beat)**: the brief `1.0`-beat default introduced in the first Option A pass was wrong for the kit grid (and for FL-style placement in general).  Restored to `0.25` beat in `PianoRoll.cpp` Draw + Paint tools and in DrumPage's empty-area new-note path (`durationBeats`, `mNoteDragOriginalDuration`, `mNoteDragGrabOffsetBeats`).

**Third rewrite (2026-04-25, same session) — structural copy from PianoRoll.cpp.**  Earlier passes still missed multiple piano-roll behaviors (timeline drag-select, multi-note move, full Tools menu + keybinds, full menu bar above toolbar, slice tool, group toggle, time-selection on ruler, etc.).  User pushed back: "I feel like you're trying to apply elements of the piano roll piece by piece... why can't we take that entire shell and just remove the midi mapping part."

So `Source/Standalone/DrumKitGrid.h` + `.cpp` were created as a **structural copy** of PianoRoll.h + PianoRoll.cpp (~1500 + ~270 lines respectively).  Same machinery — same paint, same mouse handlers, same tools, same keybinds, same menu bar, same scrollbar, same zoom limits, same playhead, same time-selection, same marquee, same ghost-paint patterns — with two surgical changes:

1. **Row mapping replaces pitch mapping.**  `noteToY(midiNote)` becomes `rowToY(rowIdx)`; `yToNote(y)` becomes `yToRow(y)`.  Note Y position is determined by the row index (which corresponds to a drum tab in ribbon order), not by `note.midiNote`.  All hit-testing, paint, and mouse code uses `(row, idx)` tuples instead of single-vector indices.

2. **Per-row data instead of single mData.**  Notes pull from `mPM->currentPattern().drumRolls[rowIdx]` (a separate `PianoRollData` per drum), not from one combined `mData`.  Selection is `std::vector<NoteRef>` where `NoteRef = { row, idx }`.  Vertical drag transfers a note from one row's drumRoll to the destination row's drumRoll (cross-drum transfer).  Undo snapshot covers all 16 drumRolls atomically (`DrumKitSnapshot = std::array<vector<PianoNote>, 16>`).

**Components**:
- `DrumKitGrid` — replaces PianoRollGrid; same tools (Draw/Paint/Delete/Mute/Slice/Select/Zoom), same keybinds (P/B/D/T/C/E/Shift+Z + Ctrl+A/C/V/X/B + Shift+arrows nudge + Alt+arrows fine-nudge + Shift+I invert + Ctrl+G glue + Alt+Q/S/U/L/R tools).
- `DrumKitSidebar` — replaces PianoKeyboard.  202 px wide.  16 rows of (drag-handle 14 + picker button 120 + Mute LED 20 + Solo LED 20 + audition piano-key 28).  Active-row indicator = 2 px accent border around the picker only (matches user's spec).
- `DrumKitControlLane` — replaces ControlLane.  Velocity + Panning modes only (PitchBend / FilterCutoff dropped — pitch-bend has no meaning for drum hits).
- `DrumKitContainer` — replaces PianoRollContainer.  Same menu bar (Edit / Tools / View) + same toolbar (Tools wrench / Snap magnet w/ right-click denom / 7 tool buttons / Undo / Redo / History / Zoom +/-).
- `DrumKitMenuBar` — replaces PianoRollMenuBar.  Same Edit + Tools + View menu items minus scale/chord (drum-irrelevant).

**Dropped from the copy** (drum-irrelevant): 88-key keyboard widget, scale-snap, vertical scrollbar, pitch-bend control-lane mode, slide/portamento note types, row scale-tinting/black-key shading, Stamp/chord tool, FullRoll/DrumGrid roll-mode logic.

**DrumPage rewiring**: the old custom `DrumKitView` class (~1340 lines, defined inline in DrumPage.cpp) deleted entirely.  `mDrumKitTab` member type changes to `std::unique_ptr<DrumKitContainer>`.  Existing public API on DrumPage (`setKitListProvider`, `setKitRowClickHandler`, `setKitAuditionHandlers`, `setKitReorderHandler`, `refreshKitView`) preserved — internally they wrap the user's `KitDrumInfo` callback into a `DrumKitRowInfo` callback that the container expects (identical fields, structural conversion).  `setPlayHead` no longer pushes to the kit (the container takes `setPlayheadBeat(double)` from the timer poll like PianoRollContainer does).  `timerCallback` pumps the playhead beat every frame.

**CMake**: `Source/Standalone/DrumKitGrid.cpp` added to the standalone target's source list.

**Trade-off**: code duplication.  Future PianoRollGrid improvements (e.g. §5F-6 deferred work) need cross-applying to DrumKitGrid.  Acceptable trade-off — three previous attempts at "minimal-fix" approaches failed to capture the full PianoRollGrid feature set, and each was met with another round of "still missing X / Y / Z" feedback from the user.  Copy + adapt is the only model that guarantees parity.

**Cache-refresh fix (same session, post-build)**: the kit grid's `mRowsCache` only refreshed at `setKitRowProvider` time — once at startup, before any drums were picked.  After a user picked a sound, `rowToPageIndex` returned -1 on mouseUp and notes weren't committed (preview block + audition fired but the note vanished on release).  Fixed via `refreshRowsCache()` helper called from `mouseDown`, `paint`, and `DrumKitContainer::refreshKitView`.

#### D1.5 — Per-note pitch editing via double-click (2026-04-25, simplified scope)

Original D1.5 spec was per-drum `mInputNote` for external pad-controller mapping.  User redirected: "we now have advanced the setup and no longer will have the midi map ... I would say it makes more sense that the notes that you change on the drum kit to a different pitch display as a different pitch on that drums actual piano roll."  In other words: drop the per-drum input-note mapping entirely; per-note pitch editing handles the same use case more flexibly.

- **Double-click a note in the Drum Kit grid → context menu**: `Velocity...` / `MIDI Note...` / `Delete`.  Implemented as `DrumKitGrid::mouseDoubleClick` + `showNoteContextMenu` + `promptVelocity` + `promptMidiNote`.  Double-click on empty area is a no-op (only fires when over a note rect via `noteAtPos`).
- **MIDI Note prompt accepts both formats**: type `C5` or `60`, both → MIDI 60.  `C#5` / `Db5` → 61.  `C6` / `72` → 72.  Static `parseNoteOrMidi(String)` helper: detects pure-numeric input and treats as MIDI; otherwise parses note name with letter (case-insensitive) + optional accidental (`#` for sharp, lowercase `b` for flat — uppercase `B` is the note B) + octave digits.  FL convention preserved: `C5 = MIDI 60` (octave * 12 + pitch class).  Static `midiToName(int)` helper formats back as `C5` etc.  Dialog pre-fills with the current note name and shows "Current: C5 = 60" so the user can see both representations.  Invalid input shows an alert; valid input commits via `beginEdit("MIDI Note")` + `commitEdit()`.
- **Per-drum Piano Roll automatically renders retuned notes at their actual pitch**: notes live in `drumRolls[N].notes` with their stored `midiNote`.  Per-drum Piano Roll uses `PianoRollContainer` in Standard mode (full pitch range, no fixed range).  No code change needed — retuning a kit note to D5 makes it appear at D5 on that drum's Piano Roll sub-tab automatically.
- **Per-drum context menu cleanup**: dropped `MIDI Map ▸` and `MIDI Note ▸` placeholder comments since they're now superseded.  The menu was never wired with those items in code (only documented as future placeholders); only the doc comment changed.

#### D3 — Global choke groups (2026-04-25, 4 batches shipped together)

Cross-engine choke bus: when any insert (synth or audio clip) on group `G > 0` fires, every other peer on the same group is silenced.  16 groups (1..16), `0 = None` is the default.  Configuration:
- **Synth strips** (Layers / Bass / Drums) — `Choke Group ▸` submenu in the per-tab right-click context menu (replaces the old `D3 placeholder` line).  ID 200 = None, 201..216 = groups 1..16.  Writes to APVTS `mixer_<kind>_<idx>_chokeGroup`.
- **Audio clips** — `Choke Group ▸` submenu on the BrowserPanel's right-click for audio items.  Per-source-clip (not per-instance) — every ArrangementBlock referencing the source clip inherits the group.  Stored on `AudioLibraryEntry.chokeGroup`, persisted in the project's `<AudioLibrary>` XML.  Audio strips' own `mixer_audio_<N>_chokeGroup` APVTS param is registered (since they're `MixerStripKind::Insert`) but ignored — clip-level metadata is the source of truth for audio.

##### Batch 1 — Data + persistence
- New APVTS param `_chokeGroup` (Int 0-16, default 0) added to every insert prefix in `addParamsForMixerStrip` (only for `MixerStripKind::Insert`; buses/master have no concept of voices to choke).  Lazy-registered alongside existing per-strip params via `ensureMixerStripParams`.
- `InsertNode::pChokeGroup` cached pointer, populated by `rebindApvts`.  Audio thread reads via wait-free atomic load.
- Public accessor `VibeGraph::getInsertChokeGroup(kind, idx)` for the dispatch path.
- `AudioLibraryEntry { path, alias, chokeGroup }` (was 2 fields, now 3).  `getAudioLibraryChokeGroup(idx)` + `setAudioLibraryChokeGroup(idx, g)` accessors.  Save/load via `<AudioLibrary><Entry chokeGroup="N"/></AudioLibrary>`; defaults to 0 on legacy projects without the field.

##### Batch 2 — Audio-thread dispatch (`applyChokeGroupDispatch`)
- Single function in `VibeSynthProcessor` called once per block, between MIDI scheduling and engine rendering, that handles BOTH directions of choke (synth↔synth, synth↔audio, audio↔synth, audio↔audio).
- Builds a `ChokeFire` list from synth note-ons (scanned out of each per-engine `MidiBuffer`) AND audio clip starts (clips whose absolute sample falls inside the current block's `[projectStart, projectEnd)`).
- For each fire: injects `allNotesOff` (channel 1) into peer synth inserts' MIDI buffers at the fire's sample position; sets `mutedByChoke = true` on peer audio clips.
- Pre-pass: for each `AudioClipPlayer` that is NOT currently in playback range, resets `mutedByChoke = false` so a fresh playthrough starts un-choked.
- Audio render loop respects `mutedByChoke` — silenced clips skip render and drop their row peak meter to -60 dB.
- Wait-free: cached atomic param pointers; `mAudioClipLock` is a `ScopedTryLockType` (skip dispatch this block if contended, no audio glitch — choke just lands a block later).

##### Batch 3 — Synth context menus
DrumPage / LayersPage / BassPage `showContextMenu` each get a new `Choke Group ▸` submenu after the Copy / Paste / Duplicate group.  Tick mark on current selection.  Click writes via `setValueNotifyingHost` to `mixer_<kind>_<idx>_chokeGroup`.  All three menus use the same ID range (`200..216`) and the same selection logic.

##### Batch 4 — Browser audio-clip menu + audio dispatch
- `BrowserPanel::showItemContextMenu` adds the `Choke Group ▸` submenu when `kind == BrowserItem::Kind::Audio`.  Click calls `mPM.setAudioLibraryChokeGroup(idx, group)`.
- `AudioClipPlayer` gets two new fields: `int chokeGroup` (copied from the source library entry at `rebuildAudioClipPlayers()` time) and `bool mutedByChoke` (audio-thread state).
- Audio render loop in `processBlock` checks `player.mutedByChoke` before playback and skips the clip if set.

#### Layers/Bass — Load Preset submenu (2026-04-25)

`LayersPage` and `BassPage` already had `Save Patch As` (writes wrapped `<BaySickEnginePreset engine="X" data="base64(getStateInformation)"/>` XML to `Documents/BaySickDAW/Presets/{EngineName}/My Presets/`).  In-app reload was missing — closes that gap.

- New `LayersPage::loadPreset(const juce::File&)` + `BassPage::loadPreset(const juce::File&)` parse the XML, detect format (wrapped vs raw apvts), and apply with prefix substitution so a preset saved on tab N loads correctly on tab M.
- Format auto-detect:
  - Wrapped (`<BaySickEnginePreset>`): base64-decode the `data` attribute, run `juce::AudioProcessor::getXmlFromBinary` to reach the inner apvts XML.
  - Raw apvts (factory presets, root tag e.g. `<BaySickBassState>`): use directly.
- Prefix substitution: extract the engine tag (`_bsb_` / `_bss_` / `_bsp_` / etc.) from the local prefix; find any PARAM child whose id contains the same tag; rewrite every PARAM id whose loaded prefix matches.  No-op when prefixes match (saved on the same tab).
- Apply via the matching processor's `apvts.replaceState` (via dynamic_cast dispatch on `mEngineProcessor`).
- New `Load Preset ▸` submenu in each page's right-click context menu.  Walks `Documents/BaySickDAW/Presets/{currentEngineName}/`:
  - **My Presets** section (user-saved) first.
  - **Factory subfolders** (alphabetical) next.
  - **(root)** loose XMLs last.
- IDs: 500..500+N where N is the number of presets discovered.  The presets array is captured by-value into the menu callback and indexed on click.
- Submenu is greyed when no engine is loaded.  Engine type filter via directory: only `Presets/{engineName}/` is walked, so a Layers tab with Harmless never sees BaySickPlayer presets.

- Notes render as variable-width rounded rects spanning `note.durationBeats`, with an internal velocity bar (height + alpha track velocity) and a white retune dot on the top-right corner when `midiNote != 60`.  Resize-edge hint (subtle vertical strip on the right edge) telegraphs the resize affordance.
- Bar / beat ruler (14 px) sits above all rows.  Click anywhere on the ruler to seek the transport.  Bar boundaries draw bright + numbered (1, 2, 3, ...); intra-bar beat ticks draw dim half-height.
- White playhead vertical line (alpha 0.7) sweeps across all rows during pattern playback, fed by `StandalonePlayHead::getCurrentBeat()` polled at 24 Hz.
- Mouse interaction (`mouseDown` / `mouseDrag` / `mouseUp` / `mouseDoubleClick`):
  - **Empty grid area, left-click** → places a new C5 note with 1.0-beat default duration, immediately enters Resize mode (drag-right extends).  Alt bypasses snap-to-grid.
  - **Click an existing note (left)** → starts Move on the body, Resize on the rightmost 6 px.  Selection: if not already selected, replaces selection with just this note.  Move supports cross-row drag — releasing over a different drum's row transfers the note to that drum's `drumRolls[pageIndex]`, updating selection in-place.
  - **Click an existing note (right)** → hard-deletes (matches per-drum Piano Roll convention).
  - **Double-click an existing note (left)** → opens context menu: Velocity... / MIDI Note... / Delete.  Velocity prompt accepts 0-127; MIDI Note prompt accepts 0-127 (default 60 = C5).  This avoids overloading right-click with both delete + menu.
  - **Shift+drag** → marquee selection (rect-vs-note-rect intersection test).  Ctrl+Shift extends an existing selection.
  - **Ctrl+click on a note** → toggle that note in the selection.
  - **Drag-handle** → row reorder (unchanged from Batch 4).
  - **Audition piano key** → press-and-hold audition (unchanged from Batch 4).
  - All beat math snaps to the kit's 1/16 cell resolution by default; Alt held during a drag bypasses snap (free placement / free resize).
- Default placement duration unified across **all** piano rolls: PianoRoll.cpp `mDrawEnd = mDrawStart + 1.0` (1/4 note) replaces the old `min(4/snapDenom, 4/32)` formula in both Draw-tool empty-space and Paint-tool branches.  Affects Layers + Bass + per-drum DrumPage piano rolls + the kit grid.  Smaller default (1/16 etc.) felt cramped on the Drum Kit tab; user prefers the FL Studio convention where every empty-area click drops a quarter note.
- Selection model: `std::set<std::pair<int,int>>` keyed by (pageIndex, noteIdx).  When `notes` is reordered (e.g. erase shifts indices), the affected entries are dropped.  `KitGridCell` clipboard struct stores `relRow` + `relStartBeat` (double, replacing prior `relCol` int) so paste can land at arbitrary microtiming, not just 1/16 cell starts.
- Keyboard:  Ctrl+A select-all / Ctrl+C copy / Ctrl+X cut / Ctrl+V paste-at-cursor / Ctrl+D duplicate-after-end / Delete erase / Esc clear selection.  All operate on the (pageIndex, noteIdx) set; clipboard is file-static so notes copy across DrumPage instances.  `deleteSelection` groups by pageIndex and erases descending so per-page indices stay stable across the batch erase.

##### Wiring callbacks added on DrumPage
- `setKitListProvider(fn)` — provides the live drum list (called every refresh).
- `setKitRowClickHandler(fn)` — opens the picker popup for an empty row.
- `setKitAuditionHandlers(onOn, onOff)` — press-and-hold routing.
- `setKitReorderHandler(fn)` — drag-reorder commit.
- `setApvts(...)` + `setPatternManager(...)` + `setPlayHead(...)` — needed for mute/solo APVTS attachment, drum-roll access, playhead read.
- `refreshKitView()` — repaint forwarder.
- StandaloneEditor methods: `getKitDrumList()` / `refreshAllKitViews()` / `wireDrumPageKitView(dp)` / `moveDrumTab(src, dst)`.

##### Open issues
- BaySickSynth-based drum playback wide+woofy bug from D1 still open — diagnosis ongoing.

### Phase E — Factory content expansion + UX polish (2026-04-25 → 2026-04-26)

Single sitting that closed out factory content generation, added templates as a first-class concept, normalized the per-engine preset/menu pattern, and shipped a chain of UX fixes.

#### E1 — Factory preset / kit / template generation (`Tools/gen_factory_presets.py` rewrite)

Generator now emits **790 presets, 87 kits, 29 templates** in one run.  Replaces the 107-preset partial ship logged at §P4.4.

**Counts by engine:**
- BaySickSynth: ~280 (drums + synth + bass families, organized by subfolder section header — TR-808 / TR-909 / Simmons / Yamaha FM / Tuned Percussion / etc.)
- BaySickBass: ~80 (analog / FM / sub variants)
- BaySickPlayer: ~290 (sample + SFZ references; drum + melodic split)
- Harmless: ~140
- Total: **790**

**Kits (87):** Each kit is a `<BaySickKit>` XML with one `<Drum slot=N engine= presetPath= locked=>` per non-empty slot.  Empty slots are omitted.  Two sources: (a) curated full-kit recipes (TR-808 / TR-909 / Hip Hop / Trap / EDM / Cinematic / etc.) and (b) drum-pack-derived kits walking each Core Library drum-pack folder and assigning one slot per file.

**Templates (29):** One per Full kit style; each template references a kit + 8 layer presets + 4 bass presets via `<Kit path/>`, `<Layer engine= presetPath= />`, `<Bass engine= presetPath= />` entries.  Factory templates reference presets by path (lazy-loaded on apply).

**Generator script changes:**
- `KIT_STYLES`, `TEMPLATES`, `HARMLESS_RECIPES`, `HARMLESS_CATEGORIES`, `BSP_RECIPES`, `BSP_SAMPLE_RECIPES` dicts added.
- `_xml_attr_escape()` helper — folders like `Cinematic, Industrial & FX` had raw `&` in attributes that broke XML parsing.  Escapes `& < > " '` consistently in every Kit / Template attribute write.
- `generate_factory_templates()` writes `<BaySickTemplate>` XMLs to `Templates/Factory/{name}.xml`.
- TR-prefix cleanup pass at script start removes any leftover `TR-*.xml` files from prior runs (skips `My Presets/`).

#### E2 — Templates infrastructure (StandaloneEditor)

User-facing template = a Save-as-Template / Load-Template bundle that captures kit + 8 layers + 4 basses with embedded engine state for user templates (factory templates reference presets by path).

**File menu additions:**
- Item 106 — `Save as Template...`
- Item 109 — `Load Template...`

**Folder layout:**
- `templatesDir()` → `Documents/BaySickDAW/Templates/`
- `factoryTemplatesDir()` → `<templatesDir>/Factory/`
- `userTemplatesDir()` → `<templatesDir>/My Templates/`

**Save flow (`saveTemplateAs`):** prompt for name → walk `mPages` collecting Layer/Bass/Drum tabs → for each, embed engine state (`getStateInformation` blob) so loading the template into a fresh project doesn't depend on the original presets still existing on disk.

**Load flow (`loadTemplate`):** confirm-discard guard → tear down all dynamic tabs → parse template XML → for each `<Kit>` reference dispatch `loadKitImpl` → for each `<Layer>` / `<Bass>` entry call new `spawnLayerTabFromTemplate(engine, presetFile, locked)` / `spawnBassTabFromTemplate(...)` helpers.  Helpers use the existing per-engine `loadPreset` machinery so factory + user templates take the same path.

**Default-template picker (Options > General > Set Default Template):** now uses `Templates/` folder with a `.xml` file picker (`mTemplateChooser`).  Replaces the prior P6 default-template picker that pointed at project folders.

**Split internal API:** `loadKit` (entry: confirm-discard prompt + dispatch) → `loadKitImpl` (actual tear-down + rebuild) so `loadTemplate` can call `loadKitImpl` directly without re-prompting per kit.

#### E3 — VibeLAF property-gated rendering

Two opt-in `Component::Properties` flags for differentiating render paths in `VibeLAF::drawButtonBackground` / `setTabSlots`:

- **`switchToggle` (bool, opt-in)** — `juce::ToggleButton` with VibeLAF defaults to checkbox style; setting `getProperties().set("switchToggle", true)` opts the button into the switch-filmstrip render.  Reserved for the FX rack slot, player switch panels, and mixer pre/post-send toggles.  Established 2026-04-26 after a confirm-prompt's "Don't show again" rendered as a switch by mistake.  See `feedback_switch_toggle_opt_in.md` memory.

- **`outlineGlowOnly` (bool, opt-in)** — page sub-tab styling.  Replaces the prior dark-text-on-accent-bg approach (illegible at default contrast) with chrome body + accent ring only.  Used inside `setTabSlots` (PageMenuBar's tab buttons).  Hover/active states still show the accent color, but as a 2 px outer-ring glow rather than a fill.

#### E4 — Per-engine preset menu pattern (recursive walker)

Cascading-submenu pattern shared across **DrumPage / LayersPage / BassPage / BaySickSynthEditor / BaySickBassEditor / HarmlessEditor / VibePlayerEditor**.  Each engine's preset menu now walks `Documents/BaySickDAW/Presets/{EngineName}/` with the same conventions:

- **My Presets section first** (user-saved patches).
- **Factory subfolders** alphabetical, each as a top-level submenu (TR-808 / TR-909 / Simmons / etc.).
- **(root) loose XMLs** last.
- IDs assigned 500..500+N at menu-build time; preset list captured by-value into the click callback and indexed.
- Greyed when no engine is loaded.

**My Presets folder pathing fix:** BaySickSynth/Bass/Drums Save Patch As was writing to the engine's root `Presets/{EngineName}/` folder.  Fixed to write into `Presets/{EngineName}/My Presets/{name}.xml` so saves coexist with the factory-folder hierarchy without polluting it.  Already-correct path on Layers/Bass.

**Drum vs melodic preset filter on Layer/Bass tabs:** new `addLayerPresetDirToMenu` / `addBassPresetDirToMenu` helpers accept a `skipDrumFolders` flag.  Layer/Bass tabs walking the BaySickPlayer presets folder pass `skipDrumFolders=true` so factory drum-pack presets don't appear in melodic context.  `DrumPage` walking the same tree passes `skipDrumFolders=false`.  `SampleLibrary::isDrumPack(name)` is the underlying classifier (already shipped at §P4 line 1609).

**Drum picker — sample-based BaySickPlayer presets in Sample submenu (not Synth Patch):** prior layout grouped all BaySickPlayer presets under "Synth Patch" alongside BaySickSynth patches, which felt wrong (they're sample-based, not synth-based).  Reorganized so the Sample submenu shows: Browse / SFZ / Core Library walker + per-engine BaySickPlayer drum presets organized by category (Hip Hop Drums / EDM Drums / etc. — each a section header).  Synth Patch submenu is BaySickSynth patches only.

#### E5 — AudioSettingsDialog Documents-path fix

`AudioSettingsDialog::applySettings` was writing `audio_settings_pending.xml` to a hardcoded Roaming path.  After the P4b migration moved audio settings to `Documents/BaySickDAW/audio_settings.xml`, the Apply + Restart loop silently no-op'd because the dialog wrote pending changes to a file the loader never read.

**Fix:** the dialog now writes the pending file as a sibling of `VibesynthStandaloneApp::getAudioSettingsFile()` — single source of truth for the path.  Reinforces the memory rule `reference_single_source_of_truth_for_paths.md`: every reader + writer of files at a given path must call the central resolver, never hardcode parallel path strings.

#### E6 — Effects page sub-tab default + per-channel persistence (2026-04-26)

(Documented in the **Effects Page** section above — sub-tab default fix, per-channel TabKind persistence, ribbon `mLastFXChannel` reset, and ribbon-▾ TabKind mapping.)

### Phase B — Keymap expansion (2026-04-26, single sitting after Phase A)

Sub-batched into 6 chunks; each is independently verifiable.

#### B-1 — Page switches + file operations
- Added 11 commands to `BSCommands::CommandIDs` enum + `KeyBindings` catalog:
  - `cmdShowMixer` (F5), `cmdShowEffects` (F6), `cmdShowBuilder` (F7), `cmdShowLayers` (F8), `cmdShowBass` (F9), `cmdShowDrums` (F10), `cmdShowPianoRoll` (F11)
  - `cmdFileNew` (Ctrl+N), `cmdFileOpen` (Ctrl+O), `cmdFileSave` (Ctrl+S), `cmdFileSaveAs` (Ctrl+Shift+S)
- `StandaloneEditor::perform` routes to existing helpers (`handleCommandMessage`, `selectFirstTabOfType`) + new `showLastUsedPianoRoll` for F11 (uses `mLastRollKind`/`mLastRollIndex`, switches to that tab + its Piano Roll sub-tab; falls back to first Layers tab + Piano Roll when no piano roll has been visited yet).
- View menu labels reordered to F5=Mixer, F6=Effects, F7=Builder, F8=Layers, F9=Bass, F10=Drums, F11=Piano Roll (was F5=Layers, F6=Bass, F7=Drums, F8=Builder).
- File menu's New Project label updated to show `(Ctrl+N)`.

#### B-2 — Pattern navigation
- 5 new commands: `cmdRenameActivePattern` (F2), `cmdNextEmptyPattern` (F3), `cmdNewPattern` (F4), `cmdNextPattern` (`+`), `cmdPrevPattern` (`-`).
- New `StandaloneEditor` helpers: `showRenamePatternDialog`, `jumpToNextEmptyPattern`, `createNewPattern`, `cyclePattern(int delta)` with wrap, `isPatternEmpty(int idx)` (checks all per-page rolls — layerRoll[]/bassRoll[]/drumRolls[] + legacy drumRoll).

#### B-3 — Transport extensions
- 6 new commands: `cmdToggleSongMode` (L), `cmdSeekHome` (Home), `cmdFastForward` (NumPad 0 default; switched to plain `0` after user reported numpad-divide / numpad-multiply not firing on his keyboard), `cmdPrevBarSong` (`/`, Song-mode-only), `cmdNextBarSong` (`*`, Song-mode-only), `cmdToggleMetronome` (Ctrl+M).
- `GlobalTransportBar` gets two new public methods: `toggleMetronome()` (uses `dontSendNotification` — the button's own onClick already fires onMetronomeToggle, so `sendNotification` would double-fire and desync visual + state) and `toggleSongMode()` (delegates to existing `setSongMode`, which already fires onSongModeChanged).
- **Song-mode infinite-playback fix (PluginProcessor.cpp:307):** the audio-thread stop check required `songEnd > 0.0`, so empty arrangement + play-through mode played indefinitely.  Extended condition to also fire `mRequestStop` when `songEnd <= 0.0` AND not in loop mode.  User caught this when L key made testing Song mode trivial.
- **Song loop button defaults to ON** (was play-through).  `mSongLoopBtn->setToggleState(true)` at construction + `mSongLoopMode { true }` in PluginProcessor.h matched.

#### B-4 — Conflict rebinds + scroll fixes + Ctrl+drag marquee
- **Bare Z = Zoom tool** in Builder (`ArrangementGrid::keyPressed`) and Piano Roll (`PianoRollGrid::keyPressed`) tool-letter blocks.  Old `Shift+Z` paths removed.
- **PgUp/PgDn = Zoom in/out when Zoom tool active**, otherwise vertical scroll.  Added in both Builder + Piano Roll.
- **Alt+G = Ungroup** in Piano Roll (was Shift+Alt+G).  Shift+G stays for group.
- **Builder mouse wheel direction fix** — bare wheel was horizontal scroll (wrong per spec), Shift+wheel was unhandled.  Flipped: bare wheel = vertical viewport scroll (~1 row per click, standard Windows direction = wheel up → view-Y decreases), Shift+wheel = horizontal scroll with FL convention (wheel up = scroll RIGHT toward later bars; sign flipped vs vertical because timeline scrolling reads as "advance through" not "scroll the surface").  Ctrl+wheel + Alt+wheel zoom branches unchanged.
- **Slice tool snap fix in Piano Roll** — `sliceNotesOnLine` always called `snapBeat()` on the cut, but the visual drag line used raw mouse pixels — user couldn't see where the cut would land.  Fixed by snapping `mSliceStart.x` + `mSliceEnd.x` to grid in `mouseDown`/`mouseDrag` (Alt held bypasses snap, matching the global piano-roll convention).  The cut math itself was already correct.
- **Ctrl+drag marquee select** added to Builder, Piano Roll, AND Drum Kit grid (caught after F-1; was missing on Drum Kit because that grid's keyPressed/mouseDown is independent from PianoRollGrid).  Click on a block/note still falls through so existing tool-specific Ctrl+click semantics work; Ctrl+drag from empty area starts marquee regardless of tool.
- S-split into 3 mutually-exclusive toggles + Standard/Slide/Portamento symbols deferred to **D-8** (paired with the slide/porta DSP fix).

#### B-5 — Ctrl+Z / Ctrl+Alt+Z global migration
- 2 new commands: `cmdGlobalUndo` (Ctrl+Z), `cmdGlobalRedo` (Ctrl+Alt+Z).  `perform` calls `globalUndo` / `globalRedo` (existing).
- Removed per-page Ctrl+Z handlers:
  - **EffectsPage**: dropped entire `keyPressed` + `visibilityChanged` + `KeyListener` inheritance + the `addKeyListener`/`removeKeyListener(this)` dance.  Header simplified.
  - **BuilderPage::ArrangementGrid::keyPressed**: dropped Ctrl+Z + Ctrl+Alt+Z lines from the Ctrl block; remaining Ctrl shortcuts (A/B/C/V/P, Ctrl+Shift+1..6 zoom presets) stay local.
  - **PianoRollGrid::keyPressed**: dropped Ctrl+Z + Ctrl+Alt+Z lines; remaining Ctrl shortcuts stay local.
- Removed Ctrl+Z/Ctrl+Alt+Z from Builder + Piano Roll documentation rows in the popup (they're now editable globals in the General tab — listing them as page-local would confuse the hardcoded-conflict check).

#### B-6 — Path A documentation rows + popup expansion
- Path A chosen over Path B for tool letters / mouse modifiers — full migration to commands-with-page-context-guards is high-risk for low value (FL doesn't let you rebind tool letters either).  Surface them as non-editable rows instead.
- Added ~80 reference rows to `KeyBindings::buildMouseRefs()`:
  - **Builder Page (~30 rows):** 9 tool letters, 8 edit ops (Ctrl+A/B/C/V + zoom presets + Delete + Escape + Shift+Arrows), PgUp/PgDn behaviors, 14 mouse modifiers (wheel directions + Ctrl/Shift/Alt variants + ruler interactions + Ctrl+drag marquee + Right-Alt mute/quantize + Ctrl+Shift / Ctrl+Right zoom rect / fit-to-viewport + Shift/Middle pan).
  - **Piano Roll (~36 rows):** 8 tool letters incl. `S` note-type cycle, ~14 edit ops, Shift+G group / Alt+G ungroup, 7 Alt+letter tool dialogs (Q/S/A/U/L/R/P), PgUp/PgDn zoom, ~11 mouse modifiers (wheel directions + Right-click+wheel cycle tools + Ctrl+drag marquee + ruler interactions).
- **Popup window resized 640×500 → 880×680** for browsable density.
- **Hardcoded-conflict warning** (`BSCommands::findHardcodedConflicts`): when the Set capture grabs a key already bound page-locally (parsed from each documentation row's shortcut string via `juce::KeyPress::createFromDescription`), the user gets a different prompt explaining "this is hardcoded as X on Y page; your binding fires only when that page isn't focused".  Combined with the editable-conflict prompt when both apply.
- **Side-fix:** removed Builder F2 / F4 local handlers (they were intercepting before the global commands could fire).  F2's track-row-rename functionality stays accessible via right-click on the row label; F4's find-next-empty-bar was redundant with the new `cmdNextEmptyPattern` (F3).

### Phase F — Pattern colours + template loading bugfix (2026-04-26)

#### F-1 — Per-pattern user colour
- Added `juce::Colour color { 0xffb0b0b0 }` field to `Pattern` struct (default light grey).  Persisted in `toValueTree`/`fromValueTree` as `color="argb-int"` attribute; missing attribute on legacy projects falls back to default.
- `BuilderPage::ArrangementGrid::blockColour` and the `BrowserItem::setAccentColour` call swap from `kBlockCols[idx % 8]` to `mPM.getPattern(idx).color`.  Legacy palette table no longer consulted for new content.
- New `Source/Standalone/PatternColorPicker.h/.cpp` helper with `juce::ColourSelector` subclass `ColourSelectorWithRecents` that overrides `getNumSwatches`/`getSwatchColour`/`setSwatchColour` to surface a persistent 10-slot "Recently Used" row.  Picker uses `juce::DialogWindow::LaunchOptions` + `setAlwaysOnTop(true)`.
- **Live preview**: `ChangeListener` on the selector fires every time the user adjusts the colour — Builder grid blocks + browser pattern items repaint immediately.  When the picker closes (`PickerWindow::~PickerWindow`) the LAST colour is pushed onto the recents list (intermediate hue drags don't spam).
- **Recent-colour persistence**: `Documents/BaySickDAW/settings.xml` `<RecentPatternColors>` child with `<Color argb="…"/>` entries, capped at `kMaxRecents = 10`.  Survives across runs.  Settings file is the same one `ProjectManager` already manages — `pushRecent` reads + writes preserving every other section (`<RecentProjects>`, `defaultTemplate`, etc).
- "Change Color..." entries added to two invocation points:
  - `BrowserPanel::showItemContextMenu` (Builder browser, right-click on a pattern item) → result==6.
  - Transport-bar pattern dropdown's left-click popup (`mPatternBtn->onClick`) → result==-4.

#### F-2 — Template Layer/Bass preset prefix bug + DrumKit Ctrl+drag
- **Bug:** factory templates loaded the engine type + tab name but every Layer/Bass tab sounded default (no preset params applied).  Drums worked fine because they load via `loadKitImpl` with embedded per-drum state.
- **Root cause:** `LayersPage::loadPreset` + `BassPage::loadPreset` had a prefix-extraction routine assuming format `tk_<row>_<engineTag>_` (3 segments).  Actual format is `tk_<row>_<idx>_<engineTag>_` (4 segments — page index inserted between row and tag).  The 3-segment extraction returned the *index* segment ("_1_") instead of the *engine tag* segment ("_bss_").  When the loaded preset (saved at slot 0, prefix "_0_") was searched for "_1_", `idx<0` for every PARAM, `loadedPrefix` stayed empty, the substitution block was skipped, and `apvts.replaceState` applied params with the original "_0_" prefix that didn't match the local APVTS at slot N — params silently dropped.
- **Fix:** compute `engineTagWithUnders` from the trailing-underscore: `tagStart = localPrefix.substring(0, trailingUnder).lastIndexOfChar('_')` → `engineTagWithUnders = substring(tagStart, trailingUnder+1)`.  For "tk_lay_1_bss_" returns "_bss_".  Identical fix applied to LayersPage + BassPage.  DrumPage uses a different path (template embeds full state per drum slot) and is unaffected.
- **DrumKit Ctrl+drag marquee** added — was missing from B-4's marquee additions because `DrumKitGrid::mouseDown` is independent from `PianoRollGrid::mouseDown` (separate file, separate hit-test via `noteAtPos` returning `NoteRef`).  Same pattern as the other two: Ctrl+drag from empty area starts marquee regardless of active tool; click on a note still falls through to tool-specific handling.

---

#### E7 — Phase A keymap framework (2026-04-26)

First slice of the keybinds-editor work documented in the standalone plan (`.claude/plans/keybinds.md` if/when drafted).  Foundation only — populated initially with the three transport commands that GlobalTransportBar's old `KeyListener::keyPressed` handled.

**New files:**
- `Source/Standalone/KeyBindings.h/.cpp` — central command catalog + persistence.  `BSCommands::CommandIDs` enum starts at `0x10001` (above JUCE's reserved range).  `Category { General, Builder, PianoRoll, MouseReference }`.  Per-command record: id / category / name / tooltip / default `KeyPress`.  Keymap XML at `Documents/BaySickDAW/keymap.xml`.
- `Source/Standalone/KeyBindsWindow.h/.cpp` — Help > Key Binds popup.  3-tab `juce::TabbedComponent` (General / Builder Page / Piano Roll); each tab is a custom `TableListBox`-based `KeyBindsTab` (JUCE's stock `KeyMappingEditorComponent` has no per-row tooltip API, so this rolls its own).  Columns: Action / Shortcut / Set / Reset.  Per-row tooltips via `getCellTooltip`.  Set button opens a `DialogWindow`-hosted `CaptureContent` whose `keyPressed` override grabs the next keypress; deferred `grabKeyboardFocus` via `MessageManager::callAsync` from `parentHierarchyChanged` so focus lands cleanly after the window is on screen.  Conflict-check: `KeyPressMappingSet::findCommandForKeyPress` query before assignment; if another command owns the key, dismisses the capture dialog and pops a `showOkCancelBox` async confirm — Replace strips + reassigns, Cancel leaves both bindings alone.  Reset button calls `KeyPressMappingSet::resetToDefaultMapping(id)`.

**StandaloneEditor wiring:**
- New base class `juce::ApplicationCommandTarget`.
- New member `juce::ApplicationCommandManager mCmdMgr` + `Component::SafePointer<juce::Component> mKeyBindsWin`.
- `getAllCommands` / `getCommandInfo` / `perform` / `getNextCommandTarget` overrides.
- Constructor: `mCmdMgr.registerAllCommandsForTarget(this)` → `set->resetToDefaultMappings()` → `BSCommands::loadMappings(*set)` → `addKeyListener(set)`.  This registers the `KeyPressMappingSet` as a top-level `KeyListener` so commands fire from any focus location after existing per-page handlers fall through.
- Destructor: `removeKeyListener(set)` (replaces the prior `removeKeyListener(mTransport.get())` — `GlobalTransportBar` is no longer a `KeyListener`).
- Help menu item 603 — `Key Binds...` opens / re-fronts the popup.

**GlobalTransportBar refactor:** dropped the `KeyListener` base + `keyPressed(KeyPress, Component*)` override.  Three new public methods carry the same logic:
- `togglePlayPause()` — Space default
- `stopAndDisarm()` — Shift+Space default
- `toggleRecord()` — R default

`StandaloneEditor::perform` switches on the command ID and calls these.

**Phase A catalog (3 commands shipped — the rest land in Phase B+):**
- `cmdPlayPause` (Space)
- `cmdStopAndDisarm` (Shift+Space)
- `cmdToggleRecord` (R)

Plus 2 mouse-reference rows in the General tab (vertical/horizontal scroll) — non-editable documentation rows surfaced by `BSCommands::getMouseRefRows()`.

The Builder Page and Piano Roll tabs exist but render empty pending Phase B migration.  The popup's TooltipWindow is local (`KeyBindsContent` member) since the editor's main `VibeTooltip` only monitors the editor's component tree.

### Phase D-1 — Builder per-block mute + right-click zoom helpers (2026-04-26)

Pure-additive UX layer on top of the existing arrangement grid.  No data-model changes.

- **Alt+M / Alt+Shift+M** — mute / unmute the current selection.  Routes through the existing `ArrangementGrid::muteSelected(bool)` so the action goes through `beginEdit/commitEdit` and is undoable.
- **Right-Alt+Left-click** on a clip — already toggles mute on the hit block via the existing handler at `BuilderPage.cpp:2262`.  No change required for D-1; documented for completeness.
- **Right-Alt+Right-click** — opens a "Quantize selection to nearest" popup with 4 units: Bar / 1/2 Bar / Beat / Step.  Each selected block's `startBar` is rounded to the nearest unit via `std::round(startBars / unit) * unit`.  Wrapped in `beginEdit("Quantize") / commitEdit()` for undo.  **Note:** `ArrangementBlock::startBar` is `int`, so sub-bar units are no-ops on the current data model — the math is written for fractional units so it Just Works once startBar gets promoted alongside per-bar TS support (D-2).
- **Ctrl+Shift+Right-click** on a clip — fits that block to the viewport horizontally.  New `fitBlockToViewport(int)` helper sets `mPPBar = vpW / lenBars` (clamped to existing minPP/maxPP), and `mBarOff = block.startBar`.
- **Ctrl+Right-click + drag** — drag-rect zoom-fit.  New state on ArrangementGrid: `mZoomRectActive` / `mZoomRectStart` / `mZoomRect` (mirrors PianoRoll's pattern).  mouseDown sets the start, mouseDrag updates the rect, mouseUp computes `mPPBar = vpW / (endBar - startBar)` + `mBarOff = startBar` and clears the rect.  Empty rect on bare click is a no-op.  Visual: white-fill 18% + outline 50% during the drag, drawn from `drawMarquee` which now also paints the zoom rect.
- **Visual mute upgrade:** muted blocks now render as 30% black wash + diagonal white-stripe hatch (8 px stride, 2 px stroke, clipped to the block's rounded rect via `Graphics::ScopedSaveState` + `Path::addRoundedRectangle` + `reduceClipRegion`).  Replaces the prior solid-40%-black overlay across all 3 paint paths.
- All four right-click branches dispatch BEFORE `showClipContextMenu` so they take priority over the default flow.  Bare right-click (no modifier) falls through to the unchanged context menu.

---

#### Open issues (carry into next session)

- **Wide+woofy drum playback bug (open):** BaySickSynth-based drum tabs play correctly via audition (mono mid-summed kick thud) but during playback the same engine produces a wider stereo "woofy" tone overlaid on the kick.  Verified NOT caused by: voice stacking (mono-mode doesn't help), velocity (audition vel 100 vs playback vel 101 — imperceptible diff), preset transpose (compensated then reverted), Layers comparison (Layer playback is louder than audition but NOT wide+woofy).  Engine instance is shared between paths.  Drum strip mute silences both.  Reducing outVol made the effect MORE pronounced relative to the kick, suggesting a separate signal at constant level mixes in somewhere downstream of the engine's `applyGain`.  Phase C drum dual-roll work and the legacy DrumSynth removal both ruled out as causes.  Carrying forward into next session for fresh look.
- **Audition vs playback level mismatch (general):** affects Layers/Bass too — playback voice plays its full envelope (attack + decay + sustain + release) where audition's quick noteOff cuts the voice short.  Different envelope integral ⇒ perceived loudness diff.  Likely a UX choice rather than a bug, but flagging for design decision: should audition mimic full-duration playback?

---

**Last updated:** 2026-04-26 (Phase B + Phase F shipped on top of Phase A keymap framework — see the per-phase sections above.  Summary: Phase B-1 page-switches (F5-F11) + file ops (Ctrl+N/O/S/Shift+S, 11 commands); B-2 pattern navigation (F2/F3/F4 + +/- cycle, 5 commands); B-3 transport extensions (L Pattern↔Song, Home seek, fast-forward, ±bar Song mode, Ctrl+M metronome, 6 commands) + Song-mode-empty infinite-playback fix in PluginProcessor + Song loop button defaults to ON; B-4 conflict rebinds (bare Z = Zoom, Alt+G ungroup, PgUp/PgDn = zoom-when-tool-active) + Builder mouse wheel direction fix (FL convention) + Slice tool grid-snap visualization fix + Ctrl+drag marquee added to Builder/PianoRoll/DrumKit; B-5 Ctrl+Z / Ctrl+Alt+Z migrated to global commands (per-page Ctrl+Z handlers stripped from EffectsPage/BuilderPage/PianoRoll); B-6 Path A documentation rows for ~80 page-local key + mouse-modifier entries + popup window resized 640×500 → 880×680 + hardcoded-conflict warning when binding to a page-local key; F-1 per-pattern user colour (default light grey, persisted on Pattern struct, live-preview ColourSelector picker with 10-slot persistent recents row in settings.xml); F-2 template Layer/Bass preset-loading prefix-extraction bug fix (was computing index "_1_" instead of engine tag "_bss_") — drum templates were unaffected as they load via embedded kit state.  Prior entry below.)

**Prior update:** 2026-04-26 (Phase E shipped — factory content expansion (790 presets / 87 kits / 29 templates via `Tools/gen_factory_presets.py` rewrite); Templates infrastructure in StandaloneEditor (Save as Template / Load Template menu items, `templatesDir` + `factoryTemplatesDir` + `userTemplatesDir`, `spawnLayerTabFromTemplate` + `spawnBassTabFromTemplate` helpers, default-template picker now points at Templates folder); VibeLAF property-gated rendering (`switchToggle` opt-in for switch-style ToggleButtons reserved for FX rack / player switch panels / mixer pre-post send; `outlineGlowOnly` opt-in for page sub-tab styling — chrome body + accent ring only); per-engine preset menu walker pattern unified across DrumPage / LayersPage / BassPage / BaySickSynthEditor / BaySickBassEditor / HarmlessEditor / VibePlayerEditor (My Presets first → factory subfolders alphabetical → loose root XMLs last); My Presets folder pathing fix for BaySickSynth/Bass/Drums saves; drum-vs-melodic preset filter via `skipDrumFolders` + `SampleLibrary::isDrumPack` on Layer/Bass tabs; drum picker reorganized so sample-based BaySickPlayer presets live in Sample submenu (not Synth Patch); AudioSettingsDialog Documents-path fix (was hardcoded Roaming, now uses `VibesynthStandaloneApp::getAudioSettingsFile` parent — closes the Apply+Restart silent-no-op bug); Effects page sub-tab fixes (default Rack via `switchTab(TabKind::Rack)` instead of indexed `switchTab(0)` which resolved to PreEQ on 3-tab bus layouts; per-channel TabKind persistence via `mLastTabPerChannel` map keyed by dropdown channel id; `mLastFXChannel` reset to empty so subsequent ribbon-Effects clicks leave the channel alone; ribbon ▾ sub-page picks now map TabKind directly so "Rack" doesn't redirect to PreEQ on bus channels); **Phase A keymap framework shipped** — `juce::ApplicationCommandManager` + `KeyPressMappingSet` registered at editor startup, persisted to `Documents/BaySickDAW/keymap.xml`; new `KeyBindings` module (catalog: id / category / name / tooltip / default KeyPress) + `KeyBindsWindow` 3-tab popup with custom TableListBox per-row tooltips, Set/Reset buttons, conflict-check + confirm dialog; GlobalTransportBar dropped its KeyListener role (3 new public methods `togglePlayPause` / `stopAndDisarm` / `toggleRecord` invoked from `StandaloneEditor::perform`); Help menu adds "Key Binds..."; Phase A registers 3 transport commands as proof-of-concept, Phase B-onwards extends the catalog. Prior entry below.)

**Prior update:** 2026-04-17 (§6 Overdrive retrospective shipped -- A1 denormal, A3/A4/A5 smoothed PreBand/Wet/x100, A8 mono safety, A9 per-sample filter coef refresh, C1 sigmoid shaper swap, C2 Bias knob, C4 Parallel toggle, C5 OS-factor chicken-head; previous entry below)

**Prior update:** 2026-04-17 (§5 Limiter retrospective shipped -- A1 denormal, A2/A3 meter hold+decay, C1 smoothed SatCurve, C2 SC HPF, C4 auto-makeup, C5 stereo-link; panel layout fix cross-applied to §2 Compressor)

**Prior update:** 2026-04-17 (Reverb §8 shipped; all-bus meter hold+decay fix; Player-review pre-bookings added for cut-self / per-slot EQ / dual piano roll / ghost notes / pre-rack EQ; **§1 Chorus Phase A retrospective shipped** — Tier 1 A1–A6 + Tier 2 C1 LR4 / C2 Organic wave / C3 WetOnly / C4 widen CrossHz; **Effect-panel widget overhaul shipped** — new `ChickenHeadSelector` for all >2-option combos with per-letter tooltips + drag/click interaction; new `DualLabelToggle` replaces `LabeledToggle` for all switches with Named mode or OnOff mode; `VKnob` label now carries the slider tooltip; ASCII-only sweep across all UI strings (tofu-box fix); **§2 Compressor Phase A retrospective shipped** — Tier 1 A1–A6 + safety level clamp + GR meter hold+decay + Tier 2 C1 setUseSidechain / C2 SC HPF knob / C3 Peak-RMS toggle / C4 Det knob + `sidechainSourceId` scaffolding; **EQ8 startup crash fix** — null-coef guard in prepare()+reset(); **§3 Delay Phase A retrospective shipped** — Tier 1 A1-A11 (denormal guard, 100 ms smoothed delayMs, defensive clamp, while-wrap, upper clamp, dead-member cleanup, preset mirror fix) + Tier 2 C1-C5 (DiffSpread/FBReso/FBKnee/FBSym/WetIn knobs + Sync-division chicken-head) + Spectral-Delay scaffolding; **§4 Flanger Phase A retrospective shipped** — Tier 1 A1-A5 (denormal guard, smoothed Wet/Delay/Shape, while-wrap LFO) + A6 Rate-knob lockout when BPM sync is engaged + Tier 2 C1 Cross knob / C2 Shape knob / C3 Damp-as-Hz cutoff replaces 0..1 coefficient (PRESET-BREAK pre-v1, clean slate) / C4 Sync-division chicken-head; **A6 cross-apply to §3 Delay**: Time-knob lockout when Delay's BPM toggle is on, fixing same pre-existing UX gap; **A6 refinement**: soft-lockout via new `VKnob::setLocked()` + `ChickenHeadSelector::setLocked()` keeps hover tooltips reachable while swallowing clicks (transparent `LockoutOverlay` for VKnob, mouseDown short-circuit for chicken-head); **Right-click "Type in value..."** added to every VKnob rotary + plain Slider with componentID, opens modal AlertWindow pre-filled with current display text, parses via `Slider::getValueFromText` and auto-clamps via `setValue`; **Automation display-name system + paramId coverage fixes** — Chorus/Delay/Reverb/Limiter/Saturation knobs now get full `channel_sN_param` paramIds via new `getExtraKnobs()` base hook; right-click-created automations now appear in Browser panel; 3-layer name resolution (`paramId` stable / auto-resolved "Channel - Effect - Param" / `userDisplayName` rename); rename writes display-only so backend key stays stable; "Revert to auto name" right-click option)

**Subsequent update:** 2026-04-26 (Phase D-7 sub-1 shipped — Smaller Piano Roll bundle, batch 1 of 4: 7 of 13 keybinds wired in `PianoRollGrid::keyPressed`. New ctrl shortcuts: **Ctrl+Q** quickQuantizeQuarter (snap startBeat to nearest 1.0 beat ignoring snap-denominator setting), **Ctrl+U** alias for `toolChop(4)`, **Ctrl+L** quickLegato (extend each note's durationBeats up to next note's startBeat via `std::upper_bound` over a sorted startBeats vector), **Ctrl+Up / Ctrl+Down** transposeSelection(±12), **Ctrl+Delete** deleteTimeRegion (wipe time span [t0,t1) defined by the selection's earliest start / latest end, then slide later notes left by `removedLen`). New alt shortcut: **Alt+F** flamSelected (1/32-beat grace note before each selected, velocity × 0.6, groupId reset to -1, skip if graceStart < 0). New global mode: **Ctrl+Alt+Home** toggleResizeFromLeftMode flips `mResizeFromLeftEnabled` on the grid; `noteIndexNearRightEdge()` extended to detect either edge based on the flag, and `mResizingFromLeft` locked at mouseDown so direction stays stable through the gesture even if the user toggles mid-drag — mouseDrag's resize branch picks startBeat-and-shrink-toward-fixed-end-instead-of-extending-end when the flag is set. All five new helpers (`quickQuantizeQuarter`, `quickLegato`, `flamSelected`, `deleteTimeRegion`, `toggleResizeFromLeftMode`) are undoable via `beginEdit`/`commitEdit` (toggleResizeFromLeftMode itself is UI-state, not undoable). No conflicts: bare-`L` cmdToggleSongMode and bare-`Home` cmdSeekHome have different modifier masks. Sub-2 (M toggle keyboard, Alt+X scale levels), sub-3 (Ctrl+Left/Right shift-time-selection), sub-4 (Alt+Wheel + Shift+Ctrl+V cross-tab clipboard) still pending.)

**Subsequent update:** 2026-04-26 (D-7 sub-1 follow-up — chop minimum-piece guard.  `PianoRollGrid::toolChop(int divisions)` now refuses to subdivide a note when the resulting sub-piece would be smaller than a 1/16 note (`kMinSubDur = 0.25` beat, with `1e-9` float tolerance).  Affects every chop entry point: Alt+U / Ctrl+U keybinds, the right-click "Chop into N" submenu, and the toolbar Chop button.  Per-note filter applied before `beginEdit` — notes already at or below 1/16 are silently skipped, and if the entire selection is below the threshold the call is a complete no-op (no undo entry created).  Rationale: pieces smaller than 1/16 visually overlap into a single blob in the piano roll and are too narrow to grab/move.  Selections that mix coarser + finer notes still chop the coarser ones.)

**Subsequent update:** 2026-04-26 (D-7 sub-1 follow-up #2 — selection-required scope tightened on the new shortcut helpers.  Previous batch fell back to "all notes on the page" when nothing was highlighted via shared `getWorkingSet()`; user feedback: that's surprising for the new keybinds.  Now: **Ctrl+Q quickQuantizeQuarter**, **Ctrl+L quickLegato**, **Alt+F flamSelected** all use `mSelection` directly (with `expandForGroups`) and early-return when empty.  **Ctrl+U** keybind now guards `if (!mSelection.empty()) toolChop(4)` at the call site — leaves legacy `toolChop` untouched so Alt+U / right-click "Chop into N" / toolbar Chop still operate on `getWorkingSet()` as before.  **Ctrl+Up / Ctrl+Down** already selection-only via `transposeSelection` → `nudgeSelection`.  **Ctrl+Delete deleteTimeRegion** retargeted at the ruler time-range (`mTimeSelBeatStart` / `mTimeSelBeatEnd` — same drag-on-ruler range that drives Ctrl+B Duplicate Timeline) instead of the note selection: erases notes whose entire span lies inside [t0, t1) and slides every later note left by `removedLen`, then clears the ruler range.  **Ctrl+Alt+Home toggleResizeFromLeftMode** unchanged (UI mode toggle, not a note edit).)

**Subsequent update:** 2026-04-26 (D-7 sub-1 follow-up #3 — selection-only sweep + ruler time-range auto-select.  Two user-reported issues addressed.  **Issue 1**: a freshly-drawn note (not added to `mSelection` by the Draw tool's mouseUp) was being mutated by tool shortcuts because `PianoRollGrid::getWorkingSet()` fell back to "all notes on the page" when nothing was selected — so on a fresh page with one note, every Alt-letter tool acted on that one note as if it were selected (no highlight, but operations ran).  Fix: `getWorkingSet()` now returns empty when `mSelection` is empty, making all tool ops require explicit selection.  Same fallback removed from `duplicateSelected()`'s legacy non-timeline path.  **Issue 2**: Ctrl+drag on the ruler created a time range but did NOT visually mark the contained notes as selected, which (a) made plain Delete useless against the range and (b) left users guessing whether the right span was captured.  Fix: `PianoRollGrid::mouseUp` time-range release path now auto-populates `mSelection` with every note whose start lies in `[t0, t1)`, while keeping `mTimeSelBeatStart/End` set so timeline-aware ops (Ctrl+B Duplicate Timeline, Ctrl+Delete deleteTimeRegion close-gap) still see the range.  **Builder parity**: `ArrangementGrid::mouseUp` time-range release now auto-populates `mSelection` with every block that overlaps the ruler range (`block_start < t1 && block_end > t0` — covers blocks that start before, end after, or fully contain the range, not just blocks whose start lies inside it).  Net effect: drag a time range on the ruler in either piano roll or builder, blocks/notes inside light up as selected, plain Delete + Ctrl+Delete + Ctrl+B + every other selection-aware op now work without an extra marquee step.)

**Subsequent update:** 2026-04-26 (D-7 sub-1 cross-applied to DrumKitGrid + click-memory feature shipped.  **Click memory (FL-style)**: clicking on an existing note in Draw or Select tools captures its `durationBeats` (and `type` for piano roll — Slide / Portamento included) into `mClickMemoryDur` / `mClickMemoryType`; the next click-place uses those.  Drag-to-place still wins via `mDrawHasDragged` flag set in `mouseDrag` only when the new end beat differs from the click-memory preview length (so a pure click leaves the flag false and mouseUp uses the memory).  `mClickMemoryDur` defaults to 0.25 (1/16 note) so first-ever placement matches old behavior.  Drum kit variant tracks length only (drum hits don't carry slide / portamento).  Implementation files: `PianoRoll.h`, `PianoRoll.cpp`, `DrumKitGrid.h`, `DrumKitGrid.cpp`.  **DrumKitGrid cross-apply (5 areas)**: (1) `getWorkingSet()` no longer falls back to "every drum hit on every row" when `mSelection` is empty; tool ops require explicit selection.  (2) `toolChop` 1/16 minimum guard with per-note skip + no-op when entire selection is below threshold.  (3) `mouseUp` time-range release auto-populates `mSelection` with every drum hit (any row) whose start lies inside `[t0, t1)`.  (4) `noteNearRightEdge` extended to detect LEFT edge when `mResizeFromLeftEnabled` is on; `mResizingFromLeft` locked at mouseDown so direction stays stable through the gesture.  (5) `duplicateSelected()` no longer falls back to all hits when nothing is selected.  **DrumKitGrid keybinds**: Ctrl+Q (quickQuantizeQuarter), Ctrl+U (toolChop(4) selection-only), Ctrl+Delete (deleteTimeRegion uses ruler range), Alt+F (flamSelected — drum flam!), Ctrl+Alt+Home (toggleResizeFromLeftMode).  **Skipped on drums (don't apply)**: Ctrl+L (legato — drum hits don't sustain), Ctrl+Up/Down (transpose octave — drum rows are slot-based, not pitch).  Bare S note-type cycle is going away in D-8 anyway.)

**Subsequent update:** 2026-04-26 (D-7 sub-1 follow-up #4 — Ctrl+Delete selection fallback.  User reported Ctrl+Delete doesn't fire when there's a marquee/note selection but no ruler time-range set.  Cause: `deleteTimeRegion` strictly required `mTimeSelBeatStart/End` and no-op'd otherwise — the UX expectation is that Ctrl+Delete should "do something" whenever there's any kind of time-bounded selection.  Fix: `PianoRollGrid::deleteTimeRegion` and `DrumKitGrid::deleteTimeRegion` now derive `[t0, t1]` from the ruler range if set, otherwise fall back to the bounding span of the current note selection (`t0` = earliest startBeat, `t1` = latest endBeat across selected notes).  Both paths still erase notes ENTIRELY inside the span and slide notes that start at or after `t1` left by `removedLen`, then clear ruler range + selection.  Net: Ctrl+Delete now works in three flows — (a) Ctrl+drag ruler then Ctrl+Delete, (b) marquee-select notes then Ctrl+Delete, (c) auto-selection from ruler range release then Ctrl+Delete.)

**Subsequent update:** 2026-04-26 (D-7 sub-1 follow-up #5 — Ctrl+Delete added to Builder.  Mirrors PianoRollGrid + DrumKitGrid: ruler time-range first (`mTimeSelStart` / `mTimeSelEnd`), with bounding-span-of-`mSelection` fallback so the shortcut still fires after a marquee.  `ArrangementGrid::deleteTimeRegion()` walks every block in reverse, removes those whose entire span lies in `[t0, t1)`, then slides every block whose `startBar >= t1` left by `(int) std::round(t1 - t0)` (matching the existing timeline-Duplicate convention at BuilderPage.cpp:1764).  Float epsilon `1.0e-4f` in the comparisons so blocks that happen to land exactly on the range edge still get caught.  Wired in `ArrangementGrid::keyPressed` Ctrl block alongside Ctrl+A/B/C/V.)

**Subsequent update:** 2026-04-26 (D-7 sub-1 follow-up #6 — Ctrl+Delete logic rewrite + Path A doc rows.  Two fixes addressed Jeff's "Ctrl+Delete still doesn't work, and it's not even listed in the keybinds" report.  **Logic fix**: previous "ENTIRELY inside [t0, t1)" erase rule meant a long note that began inside the ruler range but extended past it would be auto-selected (start-in-range = selected) but NOT erased (end > t1).  Hence the visible-but-not-deleted bug when ruler range was shorter than a note's duration.  Rewrite: erase rule is now "STARTS in [t0, t1)" which matches the auto-select rule exactly.  Selection bounds also win over the ruler range now (previously the ruler won, with selection as fallback) so when the user does ruler-Ctrl+drag-then-Ctrl+Delete the bounds derive from the AUTO-SELECTED notes / blocks instead — preventing edge cases where a note straddling the ruler edge was missed.  Float epsilons (1e-6 for beats, 1e-4f for bars) added to all comparisons to catch boundary notes after snap rounding.  Same logic across `PianoRollGrid::deleteTimeRegion`, `DrumKitGrid::deleteTimeRegion`, and `ArrangementGrid::deleteTimeRegion`.  **Doc rows added** to Help > Key Binds: Ctrl+Delete entries in Builder + PianoRoll categories, plus retroactive entries for the rest of D-7 sub-1 that were missing - Ctrl+Q (Quick Quantize 1/4), Ctrl+U (Quick Chop into 4), Ctrl+L (Quick Legato), Ctrl+Up/Down (Transpose Octave), Ctrl+Alt+Home (Flip Resize Edge), Alt+F (Flam), Alt+M (Mute Selected).  All rows are PianoRoll category for now; DrumKit category split lands in D-7 sub-2.)

**Subsequent update:** 2026-04-26 (D-7 sub-1 follow-up #7 — Ctrl+Delete root cause FOUND.  All my prior follow-ups were patching the wrong layer; the real bug was the C++ overload resolution on `key == KeyPress::deleteKey`.  JUCE's `KeyPress` declares both `bool operator==(int) const` and `bool operator==(const KeyPress&) const` PLUS an implicit `KeyPress(int)` constructor.  The compiler picks `operator==(KeyPress&)` for `key == KeyPress::deleteKey` (treats the int constant as a KeyPress with NO modifiers via the implicit ctor), and that overload compares MODIFIERS too.  So `Ctrl+Delete == KeyPress::deleteKey` was returning false because `KeyPress::deleteKey` has no Ctrl bit.  The plain Delete handler still worked because plain-Delete press has no modifiers either — both sides matched.  **Fix**: replaced `key == KeyPress::deleteKey || key == KeyPress::backspaceKey` with `key.isKeyCode(KeyPress::deleteKey) || key.isKeyCode(KeyPress::backspaceKey)` in all three Ctrl+Delete handlers (`PianoRollGrid::keyPressed`, `ArrangementGrid::keyPressed`, `DrumKitGrid::keyPressed`).  `isKeyCode(int)` is the unambiguous int-only comparison.  Diagnosis path: added a state-dump AlertWindow inside each `deleteTimeRegion`, plus a "Ctrl-block reached unhandled" AlertWindow in PianoRollGrid + ArrangementGrid — diagnostics revealed `key.getKeyCode()=65582`, `KeyPress::deleteKey=65582`, `key == deleteKey ? false`, `key.isKeyCode(deleteKey)? TRUE`.  Diagnostics removed after fix verified by user.  Other `key == KeyPress::xxxKey` sites in the codebase are inside no-modifier blocks where both overloads happen to agree, so those stay untouched.)

**Subsequent update:** 2026-04-26 (D-7 sub-2 shipped — three pieces of the smaller-piano-roll bundle: **M toggle keyboard column** (PianoRollContainer-side `mKeyboardVisible` bool flipped via new `PianoRollGrid::onToggleKeyboard` callback fired from bare-M in the no-modifier block of `keyPressed`; `resized()` now uses a per-call `kbW = mKeyboardVisible ? PianoKeyboard::kWidth : 0` so the grid + scrollbars + control lane all expand into the freed column when the keyboard hides; per-tab state - each Piano Roll tab remembers its own visibility); **Alt+X Scale Levels** (modal `juce::AlertWindow` with a custom `ScaleLevelsHost` component containing a 0-200 % LinearHorizontal slider with built-in TextBoxRight numeric box; OK applies `velocity = jlimit(0, 1, velocity * scale)` to every selected note, undoable via beginEdit/commitEdit; selection-only, no-op when nothing highlighted; mirror added to `DrumKitGrid::scaleSelectionLevels` with parallel `DrumScaleLevelsHost`); **DrumKit Keybinds tab in Help > Key Binds** (new `Category::DrumKit = 3` enum value with `MouseReference` bumped to 4, `categoryName` mapping for the new category, ~14 documentation rows covering tools / Ctrl ops / Alt ops / arrows / Skipped-from-Piano-Roll caveats, and a fourth `mTabs.addTab` call in `KeyBindsWindow.cpp` so the tab actually renders).  M is documented as "Piano Roll only" - drums use a sidebar instead of a piano keyboard so M is unbound on `DrumKitGrid::keyPressed`.  Alt+X applies in both surfaces.  Shift+Ctrl+V was DROPPED from the D-7 spec per Jeff: Option A picked, Ctrl+V will be promoted to a static cross-tab clipboard in sub-4 instead of having two clipboards.)

**Subsequent update:** 2026-04-26 (D-7 sub-2 follow-up - DrumKit doc rows expanded.  Beginners shouldn't have to cross-reference the Piano Roll tab to learn what shortcuts do; original DrumKit rows had compact "same as Piano Roll" entries that defeated the purpose of a separate tab.  Each drum-applicable shortcut now has its full self-contained description (separate rows for P / B / D / T / C / E tools instead of one combined row; same for Ctrl+A/B/C/V/G and Alt+Q/S/U/L/R).  Also expanded the "(Piano Roll only)" rejection rows: M, S, Ctrl+L, Ctrl+Up/Down, Alt+A, Alt+P each get their own row explaining why they're unbound on drum kit (slot-based rows, no slide/porta, no chords/arp by pitch, drum sidebar instead of keyboard).  Total ~30 self-documenting drum-kit rows.)

**Subsequent update:** 2026-04-26 (D-7 sub-3 shipped — Ctrl+Left / Ctrl+Right shifts the ruler time-selection box by its own length without moving the contents.  Implemented across all three time-aware grids: `PianoRollGrid::shiftTimeSelectionLeft/Right` (delegate to private `shiftTimeSelectionByLength(int direction)` which mutates `mTimeSelBeatStart/End/Anchor` then re-runs the auto-select rule from `mouseUp` to repopulate `mSelection` from the new range), same pattern for `DrumKitGrid` (walks every drum row to repopulate `mSelection` of `NoteRef`), and `ArrangementGrid` (uses `mTimeSelStart/End` floats with bar-overlap auto-select).  Wired in each `keyPressed` Ctrl block via `key.isKeyCode(KeyPress::leftKey)` / `KeyPress::rightKey`.  Clamps `t0` to 0 on left shift (no negative beat / bar positions).  No-op when no ruler range is set (this shortcut is specifically for moving the ruler box, not for shifting note selections).  Doc rows added to PianoRoll, DrumKit, and Builder categories of the Help > Key Binds window.)

**Subsequent update:** 2026-04-26 (D-7 sub-4 shipped — five things: **(1) cross-tab Ctrl+V** promoted `PianoRollGrid::mClipboard` from per-instance to `static sClipboard` so all Piano-Roll tabs (Layer / Bass / Drum-tab piano-rolls) share one clipboard.  Last copy anywhere wins.  `DrumKitGrid::mClipboard` stays per-instance (different format).  `Shift+Ctrl+V` dropped from spec entirely.  **(2) Alt+Wheel over Control Lane** = adjust the lane's currently-displayed property (velocity / pan / pitch-bend) for the note whose bar is under the cursor x.  Default delta ±0.05; **Shift+Alt+Wheel** = ±0.01 fine.  Spatially separate from grid-Alt+Wheel (zoom) so no conflict.  Implemented on `ControlLane::mouseWheelMove` and `DrumKitControlLane::mouseWheelMove`.  Undoable via lane callbacks.  **(3) Selected notes paint RED in the Control Lane** — `ControlLane`/`DrumKitControlLane` query the grid via new `isNoteSelected` / `isRefSelected` callbacks and color the bar/dot/stem with `Colour(0xffff3344)` when the note's index is in `mSelection`.  Solves "I can't tell what the lane is going to edit" UX issue.  **(4) Selection-locked lane edits** — `ControlLane::noteNearX` and `DrumKitControlLane::noteNearX` honour the selection: when the grid has any selection, only selected notes' bars are targetable.  Fixes the long-standing chord-overlap bug where dragging a selected note's bar would target an unselected note next to it.  Applies to drag, click, AND Alt+Wheel.  Applies whether the selection came from a marquee or from the ruler-time-range auto-select.  **(5) Click-outside-time-range clears state** — added prefix block to `PianoRollGrid::mouseDown`, `DrumKitGrid::mouseDown`, and `ArrangementGrid::mouseDown`: when a ruler time-range is set AND click-x falls outside `[t0, t1)` (and not a right-button), clear both `mTimeSelBeatStart/End` AND `mSelection` BEFORE the normal mouseDown logic runs.  Right-clicks skipped so context menus don't wipe state.  **EC-1**: drum-hit / piano-roll-note drawn INSIDE an existing ruler range adds itself to the selection (sortNotes-aware index re-find post-push).  **EC-2**: clicking a non-selected note replaces selection (current behaviour, no change needed).  Doc rows added for Alt+Wheel-over-grid (kept) AND Alt+Wheel-over-Control-Lane (new) in PianoRoll category, plus matching Alt+Wheel-over-Control-Lane in DrumKit.  Builder skipped for Alt+Wheel-over-lane (Builder has no control lane).  Public accessors added: `PianoRollGrid::isNoteIndexSelected`, `getSelection`, `getData`, `hasTimeSelection`, `getTimeSelStart/End`, `clearTimeSelection`; `DrumKitGrid::isRefSelected` wrapper.  Lane↔grid undo bridging via new `onBeginEdit(label)` / `onCommitEdit` callbacks on each lane class, fired by mouseDown / mouseUp / mouseWheelMove paths.)

**Subsequent update:** 2026-04-26 (D-7 sub-4 EC-1 reversed by user.  Original spec: drawing a new note inside an active ruler time-range adds it to mSelection.  Jeff testing: "It doesn't work right" + "I don't want it to work like that".  New rule: drawing a new note INSIDE an active ruler time-range clears `mSelection` but PRESERVES the ruler time-range itself; the new note is NOT added to the selection.  Click outside the range still clears both (unchanged).  Applied to PianoRollGrid AND DrumKitGrid.  Removes the sortNotes-aware index re-find logic since we no longer need to track the new note for selection membership.)

**Subsequent update:** 2026-04-26 (D-7 sub-4 follow-up #2 - range-aware Ctrl+A.  `selectAll()` on `PianoRollGrid`, `DrumKitGrid`, and `ArrangementGrid` now checks for an active ruler time-range first.  When set: re-grab only the notes / drum hits / blocks that fall inside `[t0, t1)` (same rule used by ruler-release auto-select - PR/DrumKit use `start in range`, Builder uses overlap).  When NOT set: classic select-everything-on-page behaviour.  Useful workflow: drag a range, mass-edit it, hit Ctrl+A to re-grab the same span if you accidentally clicked away and lost the auto-populated selection.)

**Subsequent update:** 2026-04-26 (D-7 sub-4 follow-up #3 - Ctrl+Delete priority reversed back to ruler-first.  Earlier in sub-4 I'd flipped it to selection-bounds-first; that broke the case where the ruler range was wider than the selected notes' content (e.g. range [0, 8] covering one 4-beat note and 4 beats of empty space - shifted later notes by 4 instead of 8).  Jeff: "now instead of deleting the block and moving everything forward to the start of that block its now going to the end of the last note".  Fix: restore ruler-range priority on `PianoRollGrid::deleteTimeRegion`, `DrumKitGrid::deleteTimeRegion`, and `ArrangementGrid::deleteTimeRegion`.  When a ruler range is set, both the erase span AND the shift amount come from `[mTimeSelBeatStart, mTimeSelBeatEnd]`.  Selection bounds are now the fallback only when no ruler range is set (e.g. user marquee-selected notes without ever dragging on the ruler).  Erase rule stays "starts in [t0, t1)".)

**Subsequent update:** 2026-04-26 (Unified Piano Roll Page consolidation - step 1a shipped.  Adds the new top-level ribbon slot + a stub `PianoRollPage` component without disturbing any existing piano-roll wiring.  Changes: `RibbonTabBar` enum gains `TabType::PianoRoll`; `kNumSlots` 6 -> 7 (existing slots auto-compact via `totalW / kNumSlots`, no ribbon expansion); `slotType()` order array and ctor's fixed-tab list both extended to include the new slot at index 6; `tabColour()` returns black (active `0xff1a1a1a`, inactive `0xff060606`) for the piano-key palette; `hasDropdown` returns false for PianoRoll in 1a (engine picker lands in 1b/2).  New files `PianoRollPage.h/.cpp` (placeholder body, fills with dark `0xff0d0d0d`); registered as PageEntry id=4 alongside Mixer/Effects/Builder; added to CMakeLists.  `cmdShowPianoRoll` (F11) re-routed from `showLastUsedPianoRoll()` to `selectTab(4) + onTabSelected(4)`; KeyBindings doc row updated.  Dynamic Layers/Bass/Drums tab IDs shift from 4+ to 5+ (no hardcoded references found that depend on the old range).  Step 1b (next) adds a `DrumKitContainer` instance to PianoRollPage; step 2 migrates per-engine PianoRollContainers + sub-tab nav-shortcuts + dropdown.)

**Subsequent update:** 2026-04-26 (Unified Piano Roll Page step 1b shipped - DrumKit relocated.  `PianoRollPage` now owns a `DrumKitContainer` instance bound to the same `Pattern::drumRolls[]` data + `mProcessor.apvts` as DrumPage's per-tab kit instances.  Both views edit the same data so changes propagate either way; UI state (selection / scroll / zoom) is per-instance until step 2 collapses the duplication.  Header above the kit shows "Drum Kit  v" as a non-interactive label placeholder for the engine-picker dropdown landing in step 2.  Editor wiring: new `wirePianoRollPageKitView()` mirrors `wireDrumPageKitView()` exactly - kit-row provider, audition handlers (BaySickSynth + VibePlayer drum engines), reorder handler, row-click handler (existing-row -> select drum tab + open picker / context menu; empty-row -> add new drum + open sound picker), kit-menu (Save Kit As / Load Kit), global lock prompt.  `setPlayHead` + `setUndoContext` + `isSongMode` callback on PianoRollPage; 30 Hz `juce::Timer` pumps `kit->setPlayheadBeat(songMode ? -1.0 : currentBeat)` matching DrumPage's `timerCallback`.  `refreshAllKitViews()` extended to include the unified page's kit so ribbon / drum reordering keeps it in sync.  Step 2 (next): migrate per-engine `PianoRollContainer`s, remove DrumPage's 16 redundant kit instances, intercept page sub-tabs as nav shortcuts, expand the dropdown, and ship D-7a ghost-notes.)

**Subsequent update:** 2026-04-26 (Unified Piano Roll Page step 1b polish - dropdown moved to PageMenuBar.  Original 1b had a 28px header bar on the page itself with a "Drum Kit  v" label.  Jeff: that's not how the regular Drum Kit on Drums page looks (no extra bar above it) and the dropdown should live on the editor's PageMenuBar next to the hamburger - same position other pages use for their sub-tabs.  Fix: removed `mDropdownLabel` + `kHeaderH` from `PianoRollPage`; resized() now lets `DrumKitContainer` fill the full content area.  In `StandaloneEditor::showPageForTab` added a `PianoRollPage` branch that calls `mPageMenuBar->setTabSlots({"Drum Kit  v"}, [](int){}, 0, Colour(0xff1a1a1a))` - single black-accent pill right after the hamburger, no-op click handler in 1b (engine-picker popup lands in step 2).)

**Subsequent update:** 2026-04-26 (Unified Piano Roll Page step 2 commit 1 - PianoRollPage gains engine registry.  Adds `EngineKind` / `EngineId` / `PianoRollConnection` types.  `PianoRollConnection::dataAccessor` is a closure (NOT a raw pointer) so pattern switches survive: PianoRollPage's 30 Hz timer re-runs the closure each tick to keep the active container bound to the right slice of `mPM->currentPattern()`.  New API: `registerEngine(id, conn)` builds a fresh `PianoRollContainer` wired with audition + undo + seek + roll-mode + display name; `unregisterEngine(id)` removes it (and falls back to `{DrumKit,0}` if it was the active selection); `selectEngine(id)` swaps visibility; `setEngineDisplayName(id, name)` updates the dropdown label live for engine renames.  `buildEngineDropdown()` returns a `juce::PopupMenu` with Drum Kit always at the top and the remainder pulled from an editor-supplied `dropdownEnumerator` callback so PianoRollPage stays decoupled from ribbon order.  `applyActiveVisibility()` shows the active container at full bounds, hides the rest.  `setUndoContext` propagates to every container.  Existing 1b API surface (`getDrumKitContainer`, `setPlayHead`, `setUndoContext`, `isSongMode`) preserved unchanged so commit 2 / 3 wiring lands incrementally.  No callers wired yet - commit 1 should build green standalone.)

**Subsequent update:** 2026-04-26 (Unified Piano Roll Page step 2 commit 2 - editor wires the registry.  Three new editor helpers (`registerLayerPianoRoll`, `registerBassPianoRoll`, `registerDrumPianoRoll`) build a closure-based `PianoRollConnection` for each engine page and call `mPianoRollPage->registerEngine`.  `dataAccessor` closure captures `this + lp/bp/dp` and re-resolves `&mPM->currentPattern().<row>[idx]` per tick so pattern switches stay live.  Audition closures capture the page raw ptr and read `getEngineProcessor()` per call - engine swaps survive without re-registering.  Wired into every spawn path: `addDefaultDynamicTabs` (default Layers/Bass/Drums), `onAddTabRequest` (ribbon + button), `spawnDuplicateLayerTab` / `spawnDuplicateBassTab` / `spawnDuplicateDrumTab` (right-click duplicate), `spawnLayerTabFromTemplate` / `spawnBassTabFromTemplate` (template loader), and project-load `<Tab type="Layers/Bass/Drum">` branches.  `onSoundNameChanged` lambdas in every spawn path now also call `mPianoRollPage->setEngineDisplayName(id, nm)` so the dropdown pill label tracks renames.  `onTabClosed` calls `unregisterEngine` BEFORE `mPages.remove` so the connection's lambdas don't dangle.  `cmdShowPianoRoll` (F11) now routes to the unified page AND restores the last-used engine via `mLastRollKind/Index` mapping (None/Layer/Bass/Drums -> EngineKind, falls back to Drum Kit when no engine page was last active).  `dropdownEnumerator` walks `mPages` in ribbon order to populate the popup; `onEngineSelected` triggers a `showPageForTab(4)` rebuild so the pill label refreshes after the user picks.  Engine pages still own their original `PianoRollContainer`s in this intermediate state - duplicate containers display the same Pattern data via shared `mPM` so edits sync.  Commit 3 strips engine-page ownership and turns sub-tab clicks into nav shortcuts.  Project save/load `<PianoRollSelection>` deferred to commit 3.)

**Subsequent update:** 2026-04-26 (Unified Piano Roll Page step 2 commit 3 - engine pages release piano-roll ownership.  LayersPage / BassPage / DrumPage no longer call `buildPianoRollTab()` in their constructors (DrumPage also drops `buildDrumKitTab()`); `mPianoRoll` and `mDrumKitTab` stay null `unique_ptr`s, and every existing `if (mPianoRoll) ...` / `if (mDrumKitTab) ...` guard becomes a no-op so the rest of the engine page code paths (audition wiring, playhead pump, undo context, resized) compile clean without behavior change.  DrumPage default landing switches from `switchTab(0)` (Drum Kit, now a nav shortcut) to `switchTab(1)` (Player) since Drum Kit is no longer a local sub-page.  Editor's `showPageForTab` setTabSlots click handlers updated: Layers/Bass `i==1` (Piano Roll) redirects to PianoRollPage + selects this engine; Drums `i==0` (Drum Kit) redirects to PianoRollPage + selects DrumKit; Drums `i==2` (Piano Roll) redirects with the per-drum engine selected.  Player + EQ sub-tabs continue to show locally on each engine page as before.  `getActivePianoRollForLoop()` rewritten to query `mPianoRollPage->getActivePianoRollForLoop()` directly instead of dynamic_cast on the visible engine page.  Net result: single source of truth - PianoRollPage owns every piano roll + the Drum Kit; engine pages keep only Player + EQ; nav shortcuts on the menu-bar pills give the user one-click access from any engine page to its piano roll on the unified view.  Project save/load `<PianoRollSelection>` round-trip is the last remaining piece; deferred to a polish pass since the F11 in-session restore via `mLastRollKind/Index` already covers most of the UX.)

**Subsequent update:** 2026-04-26 (Step 2 polish - Load Kit stays on Piano Roll page.  Reported by Jeff: loading a kit from the Drum Kit menu while on the unified Piano Roll page (Drum Kit view) was navigating away to the Drums page's Player tab.  Cause: `loadKitImpl` always called `selectTab(firstNewTabId) + onTabSelected(firstNewTabId)` after rebuilding drum tabs, jumping to the freshly-spawned drum tab.  Fix: skip that navigation when the user is currently viewing PianoRollPage with `EngineKind::DrumKit` active.  The kit view repaints in place with the new contents.  Other entry points (e.g. loading a kit from a Drums-page tab) still get the default navigation behavior so they don't end up on a destroyed page.)

**Subsequent update:** 2026-04-26 (Step 2 polish - project save / load round-trips PianoRollPage's active engine.  `serializeUIState` writes `<PianoRollSelection kind="..." index="N"/>` inside the `<UIState>` block.  `deserializeUIState` parses it AFTER all engine pages have been restored (so `registerEngine` has populated the registry) and calls `mPianoRollPage->selectEngine({kind, idx})`.  Falls back silently to Drum Kit when the saved engine isn't valid (deleted before save, or out-of-range index): `PianoRollPage::selectEngine` returns early without mutating `mActive` for unknown EngineIds.  Per-project persistence only - NOT in app-wide settings.xml.  Closes step 2.)

**Subsequent update:** 2026-04-26 (Phase G-1.1 shipped — BaySickNAM/IR engine skeleton + NAM core CMake integration.  CMakeLists.txt collects every NAM source file (`libs/NeuralAmpModelerCore/NAM/*.cpp` + `wavenet/*.cpp`) into a new `BaySickNAMCore` static library target with C++20, NAM_ENABLE_A2_FAST defined, Eigen + nlohmann include paths from `Dependencies/`, MSVC warnings tamed (W3 + suppressed 4127/4244/4267/4305) since Eigen trips W4 noise.  Conditional gating via `BAYSICK_HAS_NAM_CORE` so missing-vendor case still builds (graceful degrade).  `BaySickDAWStandalone` links the static lib + sets `BAYSICK_HAS_NAM_CORE=1` compile def.  New `BaySickNAMIRProcessor` skeleton (`Source/BaySickNAMIR/.h/.cpp`) with the 11-param APVTS layout (input_gain, gate_threshold, gate_release, nam_bypass, cab_bypass, low_cut, high_cut, cab_mix, oversampling 1x/2x/4x choice, ab_slot A/B choice, output) + 4 string properties (`nam_filepath`, `ir_filepath`, `nam_filepath_b`, `ir_filepath_b`) round-tripped via getStateInformation/setStateInformation.  processBlock is empty pass-through; full chain wires up in G-1.2.  Stereo IO bus (mono inference internally per spec).  No NAM headers included from our code yet — sub-batch only validates that the static lib builds and the processor compiles standalone.)

**Subsequent update:** 2026-04-27 (Phase G plan locked — eight sub-batches between Builder and Layers in the ribbon.  Pre-flight decisions confirmed by Jeff (see `Files For Claude/Pre-Flight Decisions (Confirmed).txt` if/when written down explicitly): files are NEVER deleted on disk regardless of how the user clears clips / closes tabs / removes engines — the app only forgets references; voice mode (poly / mono) on Clips reuses the same 4-choice picker as BaySickSynth so users learn one widget; Audio browser unifies into a single tab on the Builder browser pane (Clips + Vox + Inst recordings + imported audio files all surface from the same tree); Copy operation (right-click any clip / preset / sound) prompts for a destination + name and lands a fresh duplicate that's safe to mutate without backreferencing the source.  **Sub-batches:** **G-1 BaySickNAM/IR engine** (G-1.1 done = skeleton + CMake; G-1.2 done = file loaders + chain + full-rig auto-detect + latency; G-1.3 = noise gate + oversampling + A/B compare slots; G-1.4 = editor UI in the existing widget style — VKnob / DualLabelToggle / ChickenHeadSelector / drag-drop / recent-files menu / file-format alerts / VU + clip-LED; G-1.5 = standalone test exposure as a temporary Layers picker entry until the engine becomes a top-level ribbon tab post-G); **G-2 Clips ribbon tab** (page generation flow with the borrowed BaySickSynth voice-mode picker); **G-3 Clip piano-roll trigger DSP** (the audio engine that fires loaded clips from piano-roll notes); **G-4 Vox + Inst ribbon tabs** (recording flow + re-amp model so Vox tracks can be sent through Inst-style processing); **G-5 Audio browser unified view** (single tree showing Clips + Vox + Inst + imported, all paths file-system-anchored); **G-6 Right-click Copy operation** (cross-cutting on every clip / preset / engine — prompts for destination + name, duplicates the underlying file when needed, leaves the source untouched); **G-7 Tab-close prompts + no-file-delete contract** (consistent "remove from project but keep on disk?" flow across every closeable element); **G-8 Builder ribbon recolor** (8-color tie-dye / fallback bars matching the 8 mixer-strip colors so visual identity stays consistent between Builder clips and their Mixer destinations).  After Phase G ships, **D-7a (Ghost notes overhaul + Alt+V toggle)** is the immediate follow-up so Clips / Vox / Inst tabs get ghost-note support from day one rather than retrofitted.)

**Subsequent update:** 2026-04-27 (Phase G-1.2 shipped — BaySickNAM/IR full DSP chain, file loaders, full-rig auto-detect, latency reporting.  `BaySickNAMIRProcessor.h` forward-declares `nam::DSP` (header stays free of Eigen so consumers don't get dragged into Eigen-heavy compilation), declares the dtor out-of-line so `std::unique_ptr<nam::DSP>` can hold a forward-declared type, adds the audio-thread-safe swap-pending state (`mNamActive` + `mNamPending` + `std::atomic<bool> mNamSwapPending`), zero-latency `juce::dsp::Convolution mIr`, stereo HPF/LPF via `juce::dsp::ProcessorDuplicator<IIR::Filter, IIR::Coefficients>`, and the public load/clear/state APIs (`loadNamModel(path, &errOut)`, `loadImpulseResponse(path, &errOut)`, `clearNamModel()`, `clearImpulseResponse()`, `hasNamModel()`, `hasImpulseResponse()`, `isFullRig()`, `getNamErrorMessage()`, `getIrErrorMessage()`).  **Threading model:** loaders run on the message thread (UI thread of the caller); NAM swap is wait-free — message thread parks the new model in `mNamPending` and sets the flag; audio thread `std::swap()`s on the next `processBlock` so the OLD active model goes back into `mNamPending` and gets dealloc'd on the NEXT message-thread load (never on audio).  `mLoadLock` (`juce::CriticalSection`) serializes message-thread loads against each other; the wait-for-flag-clear spin (1 ms tick, capped at 1000 iterations) guarantees the audio thread has consumed any prior pending before the new load overwrites it.  `juce::dsp::Convolution` provides its own wait-free IR swap internally.  **prepareToPlay:** stores sr / maxBlock, builds `juce::dsp::ProcessSpec`, preps the convolution + both filters, sizes the mono double scratch buffers + `mDryBuf`, re-warms the active NAM at the new rate (`ResetAndPrewarm`), calls `setLatencySamples(0)` (zero-latency convolution + causal NAM = 0 PDC).  **processBlock chain:** (1) adopt pending NAM swap if flag set; (2) snapshot APVTS params via `getRawParameterValue` once per block (input_gain dB, output dB, nam_bypass, cab_bypass, low_cut Hz, high_cut Hz, cab_mix as 0-1); (3) update filter coeffs only when Hz changed (per-block dirty flag, matches `feedback_apvts_dirty_flag_pattern.md`); (4) input gain via `buffer.applyGain(decibelsToGain(...))`, skipped when ≈ 1.0; (5) NAM mono-sum L+R into `mNamMonoIn` (double), `mNamActive->process(double**, double**, n)`, broadcast result to both channels — try/catch around the inference fills the mono-out with zeros on exception so a bad model mutes rather than emitting garbage; (6) HPF then LPF in-place via `juce::dsp::AudioBlock` + `ProcessContextReplacing<float>`; (7) cab fork — when IR loaded AND not bypassed, snapshot dry into `mDryBuf`, run convolution in-place, then crossfade `wet = cabMix01` / `dry = 1 - cabMix01`; (8) master output gain.  **Full-rig auto-detect:** `loadNamModel` calls `nam::get_dsp(path, dspData)` (the variant that fills the metadata struct), peeks `data.metadata["gear_type"]` for the strings `"amp_cab"` / `"amp_pedal_cab"` (matches the iPlug2 NAM plugin convention), stores the result on `mNamIsFullRig` atomic for the editor to read.  When set, the editor (G-1.4) will suggest auto-bypassing the IR convolution since the capture already includes a cabinet.  **Filter coeff updates:** `updateLowCutCoeffs` / `updateHighCutCoeffs` use `*mLowCut.state = *Coefficients::makeHighPass(sr, hz)` (mutate the shared ref-counted coefficients in place — the `=` on the `Ptr` would only update the duplicator's outer pointer, not the per-channel filter instances which each hold their own Ptr to the same shared object).  **CMake refinements:** `BaySickNAMCore` include dirs flipped to `SYSTEM PUBLIC` (MSVC treats them as external headers, suppresses most template-noise warnings), and the `/wd4127 /wd4244 /wd4267 /wd4305` flags promoted from `PRIVATE` to `PUBLIC` so any consumer that pulls in Eigen via `<NAM/dsp.h>` inherits the suppressions and keeps a clean `/W4`.  **State recall:** `setStateInformation` triggers best-effort `loadNamModel` / `loadImpulseResponse` of saved paths after restoring APVTS; the IR loader defers gracefully if `prepareToPlay` hasn't run yet (records the path so a later prepare can retry).  **Build hiccup along the way:** Eigen submodule wasn't initialized in the vendored NAM checkout — `Dependencies/eigen/` was empty, breaking `<Eigen/Dense>`.  Resolved by `cd libs/NeuralAmpModelerCore && git submodule update --init --recursive`.  **Side fix during the same build:** pre-existing latent C2594 ambiguity in `Source/Standalone/SharedUI.cpp` `DynamicParamsPopout` class — was inheriting from `juce::MouseListener` directly while also being a `juce::Component` (which already inherits MouseListener), creating a diamond that made `addMouseListener(this, true)` / `removeMouseListener(this)` ambiguous.  Fix: dropped the redundant `public juce::MouseListener` base.  Compiler had been warning about this with C4584 for a while; the fresh full rebuild surfaced the actual error.  G-1.3 (noise gate / oversampling / A/B) is next.)

**Subsequent update:** 2026-04-28 (Phase G-1.3 shipped — noise gate + oversampling + A/B compare slots.  **Noise gate** sits between input gain and NAM in `processBlock`: stereo peak detection vs `gate_threshold` (lin), per-sample one-pole envelope follower with fixed 1 ms attack + user-controlled `gate_release` ms, gain ∈ [0, 1] applied to both channels.  Coefs computed via `updateGateCoeffs` only when threshold or release changes (per-block dirty-flag pattern, NaN-init forces first-block compute).  **Oversampling 1x/2x/4x** wraps just the NAM block (mono): `juce::dsp::Oversampling<float>` with `filterHalfBandPolyphaseIIR` + `isMaximumQuality = true`.  Two stage chains pre-allocated at `prepareToPlay` (`mOversampling[0]` = 1 stage = 2x, `mOversampling[1]` = 2 stages = 4x); 1x bypasses both and runs NAM at host SR.  Audio path: mono-sum to `mMonoFloatBuf`, `processSamplesUp` → write upsampled view → NAM at OS rate → `processSamplesDown` writes back to original block.  Filters + IR convolution stay at host rate.  OS factor changes go through `parameterChanged` listener → `MessageManager::callAsync` → `reResetNamForOversampling(factor)` on message thread under `mLoadLock`: re-`ResetAndPrewarm`s every loaded NAM model at the new effective rate (`mSampleRate * 2^factor`) and updates `setLatencySamples` to the OS instance's reported latency.  Wait-for-flag-clear spin (1 ms tick capped at 1000 iterations) on the message side gates writes against the audio-side swap pattern; audio thread stays wait-free.  **A/B compare slots** — every NAM (`mNamActive` / `mNamPending` / `mNamSwapPending` / `mNamLoaded` / `mNamIsFullRig`), every IR (`mIr` / `mIrLoaded`), and every path (`mNamPaths` / `mIrPaths`) is now a `std::array<…, 2>`.  `processBlock` reads `ab_slot` from APVTS once per block and indexes everything by slot.  Loaders + clears + queries gain `int slot = -1` (resolves to active via `getActiveSlot()` reading APVTS); explicit 0 / 1 forces a specific slot.  `setStateInformation` recalls A and B independently using the existing four string property keys (`nam_filepath` / `ir_filepath` / `nam_filepath_b` / `ir_filepath_b`) — backward-compatible with G-1.1 / G-1.2 single-slot saves.  `parameterChanged` listener is registered/unregistered in ctor/dtor; `std::atomic<bool>` flags explicitly `store(false)` in the constructor body (atomic's value ctor is explicit, so brace-init inside `std::array` is fragile across MSVC/GCC).  `BaySickNAMIRProcessor` now inherits `juce::AudioProcessorValueTreeState::Listener` privately for the OS-change hook.  Open question deferred: should A/B also include knob state (per-slot input gain / EQ / gate / etc.) for full preset A/B compare semantics?  Current implementation has shared knobs — only the captured amp + cab combo swaps, matching iPlug2 NAM's behavior — but the doubling refactor is on the table if Jeff wants fuller "preset A vs B" semantics later.)

**Subsequent update:** 2026-04-28 (Phase G-1.4 shipped — `BaySickNAMIREditor` 760×340 panel using existing widget vocabulary (VKnob / DualLabelToggle / ChickenHeadSelector + VibeLAF; the spec's skeuomorphic filmstrip pass at `Files For Claude/NAM & IR Loader.txt` §3.2 is overridden by Jeff's house style per CLAUDE.md "editor UI (existing widget style)").  Layout: header strip with title (left) + A/B `juce::TextButton` radio pair (right, ConnectedEdges); two file rows — AMP / CAB section labels, custom `FilePickerButton` subclass that swallows the right-button mouseDown to fire a recent-files popup while leaving left-click → onClick semantics intact, LCD-styled filename labels (amber for NAM, green for IR), bypass `DualLabelToggle`s on the right; knobs row of 7 VKnobs (Input Gain / Gate Thresh / Gate Rel / Low Cut / High Cut / Cab Mix / Output) + 1 OS `ChickenHeadSelector` (1x / 2x / 4x); status row with full-rig hint (left, amber) + error label (right, red).  **APVTS attachments**: `SliderAttachment` for every knob (auto-bidirectional), `ButtonAttachment` for both bypass toggle buttons.  Choice params (`oversampling` + `ab_slot`) drive their widgets manually — APVTS listener calls `MessageManager::callAsync` → `setSelectedIndex(idx, dontSendNotification)` / `setToggleState(slot, dontSendNotification)` to avoid feedback loops.  **File ops**: `juce::FileChooser` async pattern with `shared_ptr` capture so the chooser stays alive through the callback; drag-drop via `FileDragAndDropTarget` (`.nam` and `.wav` routed by extension); error popup via `AlertWindow::showMessageBoxAsync(WarningIcon, ...)` plus an inline error label that clears on the next successful load.  **Recent files**: 10-deep per kind in `Documents/BaySickDAW/settings.xml` `<RecentNAMFiles>` / `<RecentIRFiles>` sections; right-click on Browse fires the popup with case-insensitive dedupe + LRU promote.  **Full-rig auto-detect hint**: when active slot's NAM is loaded and `processor.isFullRig()` is true, status row prompts "Full-rig model - cabinet already included.  Consider bypassing the IR."  **Processor wiring**: `createEditor()` returns the editor, `hasEditor() = true`.  CMake adds `Source/BaySickNAMIR/BaySickNAMIREditor.cpp` to the standalone source list.  **Layout polish round (same day)**: per Jeff's review feedback the two-tone amp/cab faceplate collapsed to a single dark `kCabBgARGB` background with no divider; bypass toggles switched from `setupOnOff(featureName, tip)` to `setupNamed("OFF", offTip, "ON", onTip)` with `setLabelColour(white)` so the active state is visually unambiguous (OFF on top, ON on bottom, `onClick` swallowed by the labels themselves); broader tooltip coverage added to title, both A/B buttons, AMP / CAB section labels, both filename LCD labels, both bypass toggles, the OS chickenhead body + per-letter labels, and the OS text label; ASCII-only sweep across all user-facing strings (em-dashes in tooltips replaced with ` - ` per `feedback_ascii_only_ui_strings.md`).)

**Subsequent update:** 2026-04-28 (Phase G-1.5 shipped — temporary BaySickNAM/IR layout test window.  Help menu gains item id 604 "BaySickNAM/IR Test Layout..." that opens a non-resizable `juce::DocumentWindow` (close-button-only, native title bar) holding a private `BaySickNAMIRProcessor` warmed via `prepareToPlay(44100, 512)` + the editor as `setContentOwned` payload.  `closeButtonPressed` self-deletes; a `Component::SafePointer<juce::Component> mNamIrTestWin` on `StandaloneEditor` re-fronts the existing window on a second click instead of double-spawning (same pattern as `KeyBindsWindow`).  **Tooltip wiring fix** (caught during review): the floating window had no `juce::TooltipWindow` in its hierarchy — the StandaloneEditor's main `VibeTooltip` only monitors the editor's subtree, so all the per-widget `setTooltip()` calls in `BaySickNAMIREditor` were silently no-op'ing.  Added `juce::TooltipWindow mTooltips { this, 600 }` as a member of the inline `NamIrTestWindow` class (anonymous namespace inside `StandaloneEditor.cpp`); same fix `KeyBindsWindow` uses for the same reason.  Implemented as a single anonymous-namespace class block inside `StandaloneEditor.cpp` rather than a separate `.h/.cpp` pair — the whole window is throwaway scaffolding that gets removed when the engine gets a real home (Inst tab / FX-rack effect type / wherever Phase G-2/G-3/etc lands it).  Audio is NOT routed: the engine's processBlock runs in isolation, output goes nowhere.  This is fine for layout review (knobs drag, toggles flip, A/B + OS selectors switch, file pickers + drag-drop + recent menu + error alerts all work, full-rig hint surfaces when an `amp_cab` model is loaded) but cannot validate audio behavior — that waits until either an ASIO-connected dev session or the proper graph integration in a later G sub-batch.)
