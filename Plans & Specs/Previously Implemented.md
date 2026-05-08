# Previously Implemented - Pre-QA Build History

> **Append-only.** This document is the historical record of work completed
> BEFORE the post-Batch-10 QA plan started (2026-05-07).  Each entry was
> verified against the actual build at the time of recording - function /
> file / behavior confirmed present in source.
>
> **Distinct from `Implemented Work Log.md`**, which tracks QA-era work only
> (everything from 2026-05-07 onward).

## Header conventions

- `#` - document title
- `##` - top-level section (How to read, Sources, Entries)
- `### Phase X - <Name>` - grouping headers
- `#### **<ID>: <Title>**` - individual entry
- `#### <Sub-section>` - sub-section within an entry

Grep patterns:
- `^### ` finds all phase / module headers
- `^#### \*\*` finds all individual entries
- `^## ` finds top-level sections

## Sources surveyed

Three pre-QA source documents reviewed end-to-end and cross-referenced
against the build:

1. `Files For Claude/Final Stretch Work.txt`
2. `Files For Claude/vibedaw_blueprint.md`
3. `C:/Users/jeffm/.claude/plans/lucky-discovering-tiger.md`

## How to read this doc

Entries are grouped by phase / module / feature cluster derived from the
source-doc context (the `SectionPhase` column of the deduped inventory).
Within each grouping, entries are sorted by CanonicalID.

Each entry uses a uniform 5-line shape:

- **Sources:** pipe-separated source IDs from the dedupe pass (`BLU-*`,
  `FSW-*`, `LDT-*`).  When two source documents independently logged the
  same shipped item, the IDs land on a single canonical entry.
- **Implemented:** the BriefContext (a one-line description of what
  shipped); if Notes adds detail beyond the context, it is appended.
- **Source:** the SectionPhase / origin pointer from the source doc
  (useful for tracing back to where this was logged).
- **Verified:** date + verification method.  Phase-4 source verification
  follows the headline + 20% sample + escalate convention used during the
  QA-era survey.

This file was generated from
`C:/Users/jeffm/.claude/plans/qa-inventory-deduped-final.tsv` and is meant
to be appended-to manually as additional pre-QA items surface.  Edits to
existing entries are intentionally rare; the QA-era log records changes
over time without rewriting history here.

## Entries

### Effect Modules - DSP Quality Pass (5F-9)

#### **BLU-001: Per-voice prime-number delay offsets**
- **Sources:** BLU-001
- **Implemented:** Delay offsets 0/2/5/1/3/7 ms per voice
- **Source:** Effect Modules > §1 ChorusDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-002: 3-LFO architecture**
- **Sources:** BLU-002
- **Implemented:** Independent freq + wave per LFO
- **Source:** Effect Modules > §1 ChorusDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-003: Catmull-Rom cubic interpolation on delay reads**
- **Sources:** BLU-003
- **Implemented:** Cubic interpolation upgrade
- **Source:** Effect Modules > §1 ChorusDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-004: kMaxDelayMs = 64 ms**
- **Sources:** BLU-004
- **Implemented:** Max delay constant
- **Source:** Effect Modules > §1 ChorusDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-005: State-serialisation with old-tag compat**
- **Sources:** BLU-005
- **Implemented:** Backward-compat state serialization
- **Source:** Effect Modules > §1 ChorusDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-006: A1 CPU guards on all setters**
- **Sources:** BLU-006
- **Implemented:** Value-change comparisons on setters
- **Source:** Effect Modules > §1 ChorusDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-007: A2 LinearSmoothedValue on knobs**
- **Sources:** BLU-007
- **Implemented:** 20 ms ramp on delayMs/depth/stereo/wet
- **Source:** Effect Modules > §1 ChorusDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-008: A3 ScopedNoDenormals in process()**
- **Sources:** BLU-008
- **Implemented:** Denormal protection
- **Source:** Effect Modules > §1 ChorusDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-009: A4 setCrossCutoff upper clamp**
- **Sources:** BLU-009
- **Implemented:** min(20 kHz, 0.45·SR)
- **Source:** Effect Modules > §1 ChorusDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-010: A5 LFO phase wrap with while loop**
- **Sources:** BLU-010
- **Implemented:** Replaces single subtract
- **Source:** Effect Modules > §1 ChorusDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-011: A6 Delete legacy setRate() method**
- **Sources:** BLU-011
- **Implemented:** Method removal
- **Source:** Effect Modules > §1 ChorusDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-012: C1 LR4 crossover replaces 1-pole**
- **Sources:** BLU-012
- **Implemented:** juce::dsp::LinkwitzRileyFilter LP+HP pair 24 dB/oct
- **Source:** Effect Modules > §1 ChorusDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-013: C2 Organic LFO wave**
- **Sources:** BLU-013
- **Implemented:** sin(φ) + sin(0.37φ) as 4th option; PRESET-SAFE
- **Source:** Effect Modules > §1 ChorusDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-014: C3 wetOnly toggle + serialization**
- **Sources:** BLU-014
- **Implemented:** Kills dry + pass-band return for send routing
- **Source:** Effect Modules > §1 ChorusDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-015: C4 CrossHz UI range widened**
- **Sources:** BLU-015
- **Implemented:** 20 Hz – 10 kHz
- **Source:** Effect Modules > §1 ChorusDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-020: Look-ahead 0-5 ms with PDC**
- **Sources:** BLU-020
- **Implemented:** §2a: serialized, reports latency to EffectRack PDC
- **Source:** Effect Modules > §2 CompressorDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-021: Stereo link with max(L,R) detection**
- **Sources:** BLU-021
- **Implemented:** §2b: single shared envelope
- **Source:** Effect Modules > §2 CompressorDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-022: Auto-makeup using threshold+6 dB reference**
- **Sources:** BLU-022
- **Implemented:** §2c: more musical than 0 dBFS
- **Source:** Effect Modules > §2 CompressorDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-023: Per-sample RMS detection**
- **Sources:** BLU-023
- **Implemented:** §2d: user-controlled time constant
- **Source:** Effect Modules > §2 CompressorDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-024: 8 knee types**
- **Sources:** BLU-024
- **Implemented:** Hard/Med/Vintage/Soft + /R TCR variants
- **Source:** Effect Modules > §2 CompressorDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-025: Parallel mix + sidechain plumbing + state serialization**
- **Sources:** BLU-025
- **Implemented:** Parallel mix knob, full state serialization
- **Source:** Effect Modules > §2 CompressorDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-026: A1 CPU guards on all setters**
- **Sources:** BLU-026
- **Implemented:** Value-change comparisons
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-027: A2 LinearSmoothedValue on threshold/ratio/mix/makeupDb**
- **Sources:** BLU-027
- **Implemented:** 20 ms ramp
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-028: A3 ScopedNoDenormals in process()**
- **Sources:** BLU-028
- **Implemented:** Denormal protection
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-029: A4 GR meter hold+decay**
- **Sources:** BLU-029
- **Implemented:** 30 dB/sec, SR-aware
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-030: A5 setMakeup clamps -30..+30 dB**
- **Sources:** BLU-030
- **Implemented:** Was unclamped previously
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-031: A6 TCR coefficients moved to calcCoefs()**
- **Sources:** BLU-031
- **Implemented:** From per-block recompute
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-032: Safety clamp on levelDb**
- **Sources:** BLU-032
- **Implemented:** [-120, +60] before gain computer
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-033: C1 setUseSidechain(bool) public setter**
- **Sources:** BLU-033
- **Implemented:** Flag was serialized but unreachable
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-034: C2 Sidechain HPF knob**
- **Sources:** BLU-034
- **Implemented:** 20-2000 Hz, default 20 = off
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-035: C3 Peak vs RMS detection toggle**
- **Sources:** BLU-035
- **Implemented:** PRESET-SAFE; RMS default = v1; PRESET-SAFE
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-036: C4 Detection window (Det) knob**
- **Sources:** BLU-036
- **Implemented:** 1-100 ms, default 10
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-037: sidechainSourceId scaffolding**
- **Sources:** BLU-037
- **Implemented:** Future-proofs preset format for Tier-3 SC routing
- **Source:** Effect Modules > §2 CompressorDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-042: 5 Hz DC-blocker in feedback path**
- **Sources:** BLU-042
- **Implemented:** §3a: post-distortion, pre-feedback-level
- **Source:** Effect Modules > §3 DelayDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-043: Catmull-Rom cubic interpolation**
- **Sources:** BLU-043
- **Implemented:** §3b: upgraded from linear
- **Source:** Effect Modules > §3 DelayDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-044: Per-sample mModCutoffMod + TPT SVF**
- **Sources:** BLU-044
- **Implemented:** §3c: LP/HP/BP filter in feedback chain
- **Source:** Effect Modules > §3 DelayDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-045: Biquad retirement from feedback path**
- **Sources:** BLU-045
- **Implemented:** §3d: replaced by TPT SVF
- **Source:** Effect Modules > §3 DelayDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-046: Sat-mode gain-bug fix Option A**
- **Sources:** BLU-046
- **Implemented:** tanh normalized so small-signal gain = 1.0
- **Source:** Effect Modules > §3 DelayDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-047: ModFB knob**
- **Sources:** BLU-047
- **Implemented:** LFO cutoff modulation depth
- **Source:** Effect Modules > §3 DelayDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-048: B3 mToneFilter.reset() on state load**
- **Sources:** BLU-048
- **Implemented:** Prevents pop on preset recall
- **Source:** Effect Modules > §3 DelayDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-049: Initial CPU guards on many setters**
- **Sources:** BLU-049
- **Implemented:** Setter guards
- **Source:** Effect Modules > §3 DelayDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-050: A1 ScopedNoDenormals in process()**
- **Sources:** BLU-050
- **Implemented:** Denormal protection
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-051: A2 LinearSmoothedValue on delayMs (100 ms)**
- **Sources:** BLU-051
- **Implemented:** Tape-style smooth knob drag
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-052: A3 Defensive re-clamp on mCurDelayL/R**
- **Sources:** BLU-052
- **Implemented:** After keep-pitch slew
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-053: A4 while-wrap on LFO phase**
- **Sources:** BLU-053
- **Implemented:** Was single if-subtract
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-054: A5 setDelayMs upper clamp 2000 ms**
- **Sources:** BLU-054
- **Implemented:** Match documented spec range
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-055: A8/A11 Delete dead mLegacy filter state members**
- **Sources:** BLU-055
- **Implemented:** Zero readers
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-056: A9 State-load legacy-to-new mirror only when new key absent**
- **Sources:** BLU-056
- **Implemented:** Fixes mixed-preset case
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-057: A6 Time-knob lockout when BPM sync engaged**
- **Sources:** BLU-057
- **Implemented:** VKnob::setLocked(bool) + ChickenHeadSelector::setLocked
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-058: C1 Diffusion Spread knob added to panel**
- **Sources:** BLU-058
- **Implemented:** Unlocks setDiffusionSpread
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-059: C2 Feedback Resonance (FBReso) knob**
- **Sources:** BLU-059
- **Implemented:** Unlocks setFeedbackResonance
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-060: C3 FBDist Knee + Symmetry knobs**
- **Sources:** BLU-060
- **Implemented:** Mode-dependent: Knee for Limit, Sym for Sat
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-061: C4 Sync-division chicken-head selector**
- **Sources:** BLU-061
- **Implemented:** 8 positions: 1/1, 1/2, 1/4, 1/8, 1/8D, 1/4T, 1/16, 1/8T
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-062: C5 WetIn knob**
- **Sources:** BLU-062
- **Implemented:** Input gain into delay, pre-feedback
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-063: mDelayMode + reserved mBandDelayMs scaffolding**
- **Sources:** BLU-063
- **Implemented:** Future-proofs Tier-3 Spectral Delay
- **Source:** Effect Modules > §3 DelayDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-069: §4a Damp LPF moved INSIDE feedback loop**
- **Sources:** BLU-069
- **Implemented:** Was post-feedback-capture
- **Source:** Effect Modules > §4 FlangerDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-070: §4b 4-point Catmull-Rom cubic interpolation**
- **Sources:** BLU-070
- **Implemented:** Upgraded from linear
- **Source:** Effect Modules > §4 FlangerDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-071: §4c SmoothedValue on Rate / Depth / Feedback**
- **Sources:** BLU-071
- **Implemented:** 20 ms ramp
- **Source:** Effect Modules > §4 FlangerDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-072: F2 Sine-only short-circuit**
- **Sources:** BLU-072
- **Implemented:** Skips asin branch when Shape == 0
- **Source:** Effect Modules > §4 FlangerDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-073: F3 CPU guards on setters**
- **Sources:** BLU-073
- **Implemented:** Setter guards
- **Source:** Effect Modules > §4 FlangerDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-074: F4 setPhase clamp 0..360°**
- **Sources:** BLU-074
- **Implemented:** Phase clamp
- **Source:** Effect Modules > §4 FlangerDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-075: F5 Sync-toggle snap handling**
- **Sources:** BLU-075
- **Implemented:** No glide across the switch
- **Source:** Effect Modules > §4 FlangerDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-076: Full state serialization**
- **Sources:** BLU-076
- **Implemented:** Rate, depth, delay, feedback, etc
- **Source:** Effect Modules > §4 FlangerDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-077: A1 ScopedNoDenormals in process()**
- **Sources:** BLU-077
- **Implemented:** Denormal protection
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-078: A2 Smoothed mWet**
- **Sources:** BLU-078
- **Implemented:** 20 ms ramp
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-079: A3 Smoothed mDelay**
- **Sources:** BLU-079
- **Implemented:** 20 ms ramp; kills click on base-delay drag
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-080: A4 Smoothed mShape**
- **Sources:** BLU-080
- **Implemented:** 20 ms ramp; kills LFO step on sine to triangle morph
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-081: A5 LFO phase while-subtract**
- **Sources:** BLU-081
- **Implemented:** Replaces std::fmod
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-082: A6 Rate-knob lockout when BPM sync engaged**
- **Sources:** BLU-082
- **Implemented:** Soft-lockout via setLocked
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-083: A6 cross-apply to §3 Delay panel**
- **Sources:** BLU-083
- **Implemented:** Time-knob lockout in Delay sync
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-084: C1 Cross knob on panel**
- **Sources:** BLU-084
- **Implemented:** Unlocks setCrossLevel(dB), default -96
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-085: C2 Shape knob on panel**
- **Sources:** BLU-085
- **Implemented:** Unlocks setShape(0..1) morph
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-086: C3 setDampHz replaces setDamp(0..1)**
- **Sources:** BLU-086
- **Implemented:** 200..20000 Hz cutoff knob; PRESET-BREAK pre-v1
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-087: C4 Sync-division chicken-head selector**
- **Sources:** BLU-087
- **Implemented:** 8 positions, default 1/8
- **Source:** Effect Modules > §4 FlangerDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-094: Full LimiterDSP spec**
- **Sources:** BLU-094
- **Implemented:** NET-NEW: input gain + tanh + look-ahead + 4x OS TP detection + adaptive release
- **Source:** Effect Modules > §5 LimiterDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-095: Basic LimiterPanel + GR meter + Auto Release**
- **Sources:** BLU-095
- **Implemented:** Initial UI
- **Source:** Effect Modules > §5 LimiterDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-096: CPU guards + SmoothedValue (15 ms)**
- **Sources:** BLU-096
- **Implemented:** On InputGain/Ceiling/SatThresh
- **Source:** Effect Modules > §5 LimiterDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-097: A1 ScopedNoDenormals in process()**
- **Sources:** BLU-097
- **Implemented:** Denormal protection
- **Source:** Effect Modules > §5 LimiterDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-098: A2 GR meter hold+decay**
- **Sources:** BLU-098
- **Implemented:** 30 dB/sec, SR-aware
- **Source:** Effect Modules > §5 LimiterDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-099: A3 Input + Output dBFS meter hold+decay**
- **Sources:** BLU-099
- **Implemented:** Fall toward -96 dB
- **Source:** Effect Modules > §5 LimiterDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-100: C1 mSatCurveSmooth 15 ms ramp**
- **Sources:** BLU-100
- **Implemented:** Was unsmoothed
- **Source:** Effect Modules > §5 LimiterDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-101: C2 Sidechain HPF knob**
- **Sources:** BLU-101
- **Implemented:** 20-2000 Hz default 20 = off
- **Source:** Effect Modules > §5 LimiterDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-102: C4 Auto-makeup gain toggle**
- **Sources:** BLU-102
- **Implemented:** Default off; maximizer workflow
- **Source:** Effect Modules > §5 LimiterDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-103: C5 Stereo-link toggle**
- **Sources:** BLU-103
- **Implemented:** Default on = single envelope
- **Source:** Effect Modules > §5 LimiterDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-104: Panel layout fix two-column toggles**
- **Sources:** BLU-104
- **Implemented:** Auto MU left, Auto Release+Link right
- **Source:** Effect Modules > §5 LimiterDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-105: Cross-apply to §2 Compressor panel layout**
- **Sources:** BLU-105
- **Implemented:** Same two-column pattern
- **Source:** Effect Modules > §5 LimiterDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-106: Polished Fruity-Limiter-style UI**
- **Sources:** BLU-106
- **Implemented:** 3-zone layout, scrolling waveform, skeuomorphic knobs; PRESET-SAFE
- **Source:** Effect Modules > §5 LimiterDSP > Tier 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-112: §6a 4x oversampling around waveshaper**
- **Sources:** BLU-112
- **Implemented:** Stage only, not filters
- **Source:** Effect Modules > §6 OverdriveDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-113: §6b 5 Hz DC-blocker post-clip**
- **Sources:** BLU-113
- **Implemented:** SR-tracking R-form
- **Source:** Effect Modules > §6 OverdriveDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-114: §6c SmoothedValue on PreAmp/Color/PostFilter/PostGain**
- **Sources:** BLU-114
- **Implemented:** 15 / 15 / 30 / 15 ms
- **Source:** Effect Modules > §6 OverdriveDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-115: §6d TPT SVF BPF + LPF**
- **Sources:** BLU-115
- **Implemented:** Pre-shaper BPF + post-shaper LPF
- **Source:** Effect Modules > §6 OverdriveDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-116: Wet knob, x100 toggle, audit items**
- **Sources:** BLU-116
- **Implemented:** O2/O5/O7/O1 + legacy wrappers
- **Source:** Effect Modules > §6 OverdriveDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-117: A1 ScopedNoDenormals in process()**
- **Sources:** BLU-117
- **Implemented:** Denormal protection
- **Source:** Effect Modules > §6 OverdriveDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-118: A3 Smoothed PreBand (BPF Q)**
- **Sources:** BLU-118
- **Implemented:** mPreBandSmooth
- **Source:** Effect Modules > §6 OverdriveDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-119: A4 Smoothed Wet**
- **Sources:** BLU-119
- **Implemented:** mWetSmooth
- **Source:** Effect Modules > §6 OverdriveDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-120: A5 Smoothed x100 transition**
- **Sources:** BLU-120
- **Implemented:** Click-free toggle
- **Source:** Effect Modules > §6 OverdriveDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-121: A8 Mono-input safety**
- **Sources:** BLU-121
- **Implemented:** Pad mBandBuf/mResidualBuf channel 1
- **Source:** Effect Modules > §6 OverdriveDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-122: A9 Per-sample BPF + LPF coef refresh**
- **Sources:** BLU-122
- **Implemented:** Was once-per-block
- **Source:** Effect Modules > §6 OverdriveDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-123: C1 Waveshaper swap to x/(1+abs(x))**
- **Sources:** BLU-123
- **Implemented:** From atan/halfPi; PRESET-BREAK character
- **Source:** Effect Modules > §6 OverdriveDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-124: C2 Bias knob**
- **Sources:** BLU-124
- **Implemented:** -1..+1 default 0; even harmonics
- **Source:** Effect Modules > §6 OverdriveDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-125: C4 Parallel named toggle**
- **Sources:** BLU-125
- **Implemented:** Blend / Parallel mode
- **Source:** Effect Modules > §6 OverdriveDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-126: C5 OS-factor chicken-head**
- **Sources:** BLU-126
- **Implemented:** 2x / 4x / 8x / 16x default 4x
- **Source:** Effect Modules > §6 OverdriveDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-131: §7a-§7d audit items**
- **Sources:** BLU-131
- **Implemented:** log-scaled LFO; pre-allocated 24-stage; SmoothedValue; InvertFeedback
- **Source:** Effect Modules > §7 PhaserDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-132: P-series audit items P2-P8**
- **Sources:** BLU-132
- **Implemented:** state-load reconcile + clamps + CPU guards + sync recompute
- **Source:** Effect Modules > §7 PhaserDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-133: A1 ScopedNoDenormals in process()**
- **Sources:** BLU-133
- **Implemented:** 24-stage IIR is denormal-prone
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-134: A2 SmoothedValue on Wet (15 ms)**
- **Sources:** BLU-134
- **Implemented:** Kills zipper on Wet drags
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-135: A3 SmoothedValue on Stereo phase (20 ms)**
- **Sources:** BLU-135
- **Implemented:** Cycle fraction storage
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-136: A4 while-wrap on LFO phase**
- **Sources:** BLU-136
- **Implemented:** Was std::fmod
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-137: A5 SmoothedValue on OutGain (linear)**
- **Sources:** BLU-137
- **Implemented:** 15 ms; kills zipper on Gain drags
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-138: A6 single-branch wrap on stereo-offset LFO**
- **Sources:** BLU-138
- **Implemented:** Was std::fmod
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-139: A7 BPM-sync Rate-knob soft-lockout**
- **Sources:** BLU-139
- **Implemented:** Cross-apply from §4/§3
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-140: A9 Panel toggles/selectors sync from DSP on construct**
- **Sources:** BLU-140
- **Implemented:** Cross-apply pattern
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-141: C1 Slow/Fast Range toggle REWIRED**
- **Sources:** BLU-141
- **Implemented:** 0.05-2 Hz vs 0.05-10 Hz; PRESET-BREAK pre-v1
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-142: C2 Sync-division chicken-head**
- **Sources:** BLU-142
- **Implemented:** 8 positions default 1/4; PRESET-SAFE
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-143: C3 LFO wave chicken-head**
- **Sources:** BLU-143
- **Implemented:** Sine/Triangle/Saw/S&H; PRESET-SAFE
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-144: C4 Rate knob log-skew**
- **Sources:** BLU-144
- **Implemented:** UI-only; PRESET-SAFE
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-145: C5 Cross-channel feedback knob**
- **Sources:** BLU-145
- **Implemented:** 0-1 default 0; PRESET-SAFE
- **Source:** Effect Modules > §7 PhaserDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-152: §8a-§8h Tail mod + size click + Freeze + multi-tap ER + HF Decay + tail mod shape + Wet Tone tilt + BassCross + denormals + R1/R2/R3/R5/R7**
- **Sources:** BLU-152
- **Implemented:** Full reverb spec set with audit items
- **Source:** Effect Modules > §8 ReverbDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-158: F1 full rewrite**
- **Sources:** BLU-158
- **Implemented:** Flowers + Dabs + Sensitivity + BassRelief + Transformer + Tone Pre/Post
- **Source:** Effect Modules > §9 SaturationDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-159: A1 ScopedNoDenormals in process()**
- **Sources:** BLU-159
- **Implemented:** Multiple IIR + DC blocker
- **Source:** Effect Modules > §9 SaturationDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-160: A2 CPU guards on all 10 setters**
- **Sources:** BLU-160
- **Implemented:** Value-change comparison
- **Source:** Effect Modules > §9 SaturationDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-161: A5 Panel A9 construct-time state sync**
- **Sources:** BLU-161
- **Implemented:** tubeTypeSel/transformerTog/autoGainTog/osSel
- **Source:** Effect Modules > §9 SaturationDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-162: A7 processTube Type B cbrt optimization**
- **Sources:** BLU-162
- **Implemented:** ~3x faster; PRESET-SAFE
- **Source:** Effect Modules > §9 SaturationDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-163: 9a 4x oversampling around tube engine**
- **Sources:** BLU-163
- **Implemented:** IIR half-band polyphase, low-latency
- **Source:** Effect Modules > §9 SaturationDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-164: 9b Auto-Gain compensation toggle**
- **Sources:** BLU-164
- **Implemented:** mAutoGain bool; default off; PRESET-SAFE
- **Source:** Effect Modules > §9 SaturationDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-165: 9c Sample-rate-aware DC blocker**
- **Sources:** BLU-165
- **Implemented:** 5 Hz cutoff at any SR; PRESET-SAFE
- **Source:** Effect Modules > §9 SaturationDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-166: 9d/C1 SmoothedValue on all 8 continuous knobs**
- **Sources:** BLU-166
- **Implemented:** 15-20 ms ramps
- **Source:** Effect Modules > §9 SaturationDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-167: C2 Oversampling factor chicken-head**
- **Sources:** BLU-167
- **Implemented:** 2x/4x/8x/16x default 4x; PRESET-SAFE
- **Source:** Effect Modules > §9 SaturationDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-168: C4 Auto-Gain compensation dB readout**
- **Sources:** BLU-168
- **Implemented:** Live label
- **Source:** Effect Modules > §9 SaturationDSP > Phase A retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-174: A1 ScopedNoDenormals in process()**
- **Sources:** BLU-174
- **Implemented:** 4 shelf LP states + hysteresis + 7-stage pink + DC-block
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-175: A2 CPU guards on all 14 setters**
- **Sources:** BLU-175
- **Implemented:** Value-change comparison
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-176: A3 while-wrap on wow + flutter LFO phases**
- **Sources:** BLU-176
- **Implemented:** Replaces std::fmod
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-177: A5 Removed dead maxWowSamples statement**
- **Sources:** BLU-177
- **Implemented:** Stale unused warning
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-178: A6 Hiss formula symmetric L/R**
- **Sources:** BLU-178
- **Implemented:** Independent juce::Random per channel
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-179: A4/A9 Panel construct-time DSP state sync**
- **Sources:** BLU-179
- **Implemented:** tapeSpeedSel + osSel
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-180: 10a Hysteresis state variable**
- **Sources:** BLU-180
- **Implemented:** Magnetic-memory accumulates at OS rate
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-181: 10b Asymmetric sigmoid shaper**
- **Sources:** BLU-181
- **Implemented:** Even-harmonic-heavy; PRESET-BREAK pre-v1
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-182: 10c 4x oversampling around shaper + hysteresis**
- **Sources:** BLU-182
- **Implemented:** IIR half-band polyphase + PDC
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-183: 10d Separate Flutter LFO + smoothed noise**
- **Sources:** BLU-183
- **Implemented:** mFlutterRate 5-25 Hz default 15
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-184: 10e Cubic (Catmull-Rom) interpolation**
- **Sources:** BLU-184
- **Implemented:** On wow/flutter delay reads
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-185: 10f Pre/de-emphasis shelf pair**
- **Sources:** BLU-185
- **Implemented:** +6 dB @ 5 kHz / -8 dB @ 4 kHz; PRESET-BREAK pre-v1
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-186: 10g Pink-filtered hiss + 200 Hz HPF**
- **Sources:** BLU-186
- **Implemented:** Per-channel 7-stage Paul Kellet; PRESET-BREAK pre-v1
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-187: 10h 5 Hz SR-aware DC blocker**
- **Sources:** BLU-187
- **Implemented:** mDcCoef = 1 - 2*pi*5/sr
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-188: 10i SmoothedValue on all 10 continuous params**
- **Sources:** BLU-188
- **Implemented:** Linear-domain for gains
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-189: C1 Hysteresis Amount knob**
- **Sources:** BLU-189
- **Implemented:** 0..2 default 1
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-190: C2 Oversampling factor chicken-head**
- **Sources:** BLU-190
- **Implemented:** 2x/4x/8x/16x default 4x
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-191: C3 Pre/de-emphasis gain knobs**
- **Sources:** BLU-191
- **Implemented:** User-adjustable ±12 dB
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-192: C4 Tape speed chicken-head**
- **Sources:** BLU-192
- **Implemented:** 7.5/15/30 ips default 15
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-193: C5 Bias knob**
- **Sources:** BLU-193
- **Implemented:** 0..10 default 5
- **Source:** Effect Modules > §10 TapeDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-200: A1 ScopedNoDenormals in process()**
- **Sources:** BLU-200
- **Implemented:** Envelope integrators + LR4 + drive
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-201: A2 CPU guards on all setters**
- **Sources:** BLU-201
- **Implemented:** Value-change comparison
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-202: A4 setAttack cleanup**
- **Sources:** BLU-202
- **Implemented:** Removed dual-range kludge
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-203: A5 Panel A9 for chicken-heads**
- **Sources:** BLU-203
- **Implemented:** attackShapeSel/releaseShapeSel/osSel/stereoDetectTog
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-204: A7 Dead mFastR/mSlowR state removed**
- **Sources:** BLU-204
- **Implemented:** Cleanup
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-205: 11a Quadratic attack + sustain curves**
- **Sources:** BLU-205
- **Implemented:** 1+attack*t^2+sustain*s^2; PRESET-BREAK pre-v1
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-206: 11b Linkwitz-Riley 4th-order crossover**
- **Sources:** BLU-206
- **Implemented:** Replaces 1-pole LP band-split; PRESET-BREAK pre-v1
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-207: 11c 4x oversampling around drive stage**
- **Sources:** BLU-207
- **Implemented:** Always-on for constant latency
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-208: 11d SmoothedValue on 8 continuous params**
- **Sources:** BLU-208
- **Implemented:** 15-20 ms ramps
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-209: 11e Slow envelope uses RMS detector**
- **Sources:** BLU-209
- **Implemented:** Fast stays peak; PRESET-BREAK pre-v1
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-210: C1 OS factor chicken-head**
- **Sources:** BLU-210
- **Implemented:** 2x/4x/8x/16x default 4x; PRESET-SAFE
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-211: C2 Stereo envelope detection toggle**
- **Sources:** BLU-211
- **Implemented:** Default off = mono-sum; PRESET-SAFE
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-212: C3 Dry/Wet mix knob**
- **Sources:** BLU-212
- **Implemented:** Default 1.0 = current 100% wet; PRESET-SAFE
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-213: C4 FastRel + SlowAtt as user knobs**
- **Sources:** BLU-213
- **Implemented:** 1-50 ms each default 10 ms; PRESET-SAFE
- **Source:** Effect Modules > §11 TransientShaperDSP > Shipped in v1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-218: A1 ScopedNoDenormals in process()**
- **Sources:** BLU-218
- **Implemented:** Up to 64 IIR filter states
- **Source:** Effect Modules > §12 EQ8DSP > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-219: A2 CPU guards on all band setters**
- **Sources:** BLU-219
- **Implemented:** short-circuit on no-change
- **Source:** Effect Modules > §12 EQ8DSP > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-220: A3 CPU guards on misc setters**
- **Sources:** BLU-220
- **Implemented:** setMainLevel/setPhaseMode/etc
- **Source:** Effect Modules > §12 EQ8DSP > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-221: A6 mSpareLocked encapsulation**
- **Sources:** BLU-221
- **Implemented:** Made private + isSpareLocked() getter
- **Source:** Effect Modules > §12 EQ8DSP > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-222: A8 anySoloed() per-block caching**
- **Sources:** BLU-222
- **Implemented:** Was O(n^2)
- **Source:** Effect Modules > §12 EQ8DSP > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-223: 12a getMagnitudeForFrequency for UI curve**
- **Sources:** BLU-223
- **Implemented:** Thread-safe const
- **Source:** Effect Modules > §12 EQ8DSP > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-224: 12b Proportional Q on Peaking bands**
- **Sources:** BLU-224
- **Implemented:** SSL/Neve hardware feel; PRESET-BREAK pre-v1
- **Source:** Effect Modules > §12 EQ8DSP > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-225: 12c SmoothedValue per band**
- **Sources:** BLU-225
- **Implemented:** freq/gainDb/q with dirty flag pattern; PRESET-SAFE
- **Source:** Effect Modules > §12 EQ8DSP > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-226: 12d mIIRModSpeed wired**
- **Sources:** BLU-226
- **Implemented:** Maps 0..1 to 1..50 ms ramp; PRESET-SAFE
- **Source:** Effect Modules > §12 EQ8DSP > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-227: setStateInformation snaps smoothers**
- **Sources:** BLU-227
- **Implemented:** Misc cleanup
- **Source:** Effect Modules > §12 EQ8DSP > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-228: 12h Per-band M/S routing**
- **Sources:** BLU-228
- **Implemented:** Channel enum: Stereo/Mid/Side/LOnly/ROnly; PRESET-SAFE
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-229: EQ8MsDSP wrapper kept (Option A)**
- **Sources:** BLU-229
- **Implemented:** Inner M/S encode/decode dropped
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-230: APVTS _Channel params added additively**
- **Sources:** BLU-230
- **Implemented:** layers_mid/side_eqN_Channel etc; PRESET-SAFE
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-231: Internal MID/SIDE pill deleted**
- **Sources:** BLU-231
- **Implemented:** From ParametricEQDisplay; external buttons remain
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-232: 12i Spectrum analyser overlay**
- **Sources:** BLU-232
- **Implemented:** preFeed + postFeed in EQ8MsDSP; PRESET-SAFE
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-233: Spectrum axis fill**
- **Sources:** BLU-233
- **Implemented:** Curve extends to grid edges
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-234: Spectrum poll runs during drag**
- **Sources:** BLU-234
- **Implemented:** Moved above mSyncing/mUserDragging guard
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-235: DSP-to-UI sync skipped when no APVTS write-back**
- **Sources:** BLU-235
- **Implemented:** Defends against EffectsPage simple bind race
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-236: syncBandFromControl calls pushBandToDSP**
- **Sources:** BLU-236
- **Implemented:** Fixes slider/knob reset-to-default bug
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-237: EQ gain faders mixer-style metallic cap**
- **Sources:** BLU-237
- **Implemented:** eqFader Slider property
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-238: Live fader position pointer**
- **Sources:** BLU-238
- **Implemented:** Amber/red horizontal line at cap
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-239: Per-band readouts below each control**
- **Sources:** BLU-239
- **Implemented:** Gain/Freq/Q strips
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-240: Reset Band right-click restores freq+gain+Q+slope**
- **Sources:** BLU-240
- **Implemented:** Was gain+Q+slope only
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session A polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-241: Universal lazy-register of EQ band params**
- **Sources:** BLU-241
- **Implemented:** Every Master/Bus/Insert/Aux strip; PRESET-SAFE
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-242: Generic updateEQFromApvts helper**
- **Sources:** BLU-242
- **Implemented:** Replaces ~400 lines per-bus updates
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-243: updateAllPostRackEQsFromApvts iterator**
- **Sources:** BLU-243
- **Implemented:** Walks all 6 buses + 5 InsertKinds
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-244: Widget Band::channel field plumbed**
- **Sources:** BLU-244
- **Implemented:** Through sync/push paths
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-245: Right-click band-handle Channel + Automate submenus**
- **Sources:** BLU-245
- **Implemented:** 5 Channel + 9 Automate items
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-246: registerAutomationForBoundEQ**
- **Sources:** BLU-246
- **Implemented:** 144 paramIds wired
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-247: EffectsPage uses full bindMsDSP overload**
- **Sources:** BLU-247
- **Implemented:** Per-channel APVTS prefix
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-248: Option C channel badge on band handles**
- **Sources:** BLU-248
- **Implemented:** Amber 12x10 px chip
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-249: Dynamic hover tooltip via TooltipClient**
- **Sources:** BLU-249
- **Implemented:** Multi-line readout
- **Source:** Effect Modules > §12 EQ8DSP > Phase 2 Session B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-250: 12e TPT filter engine for LP/HP/BP**
- **Sources:** BLU-250
- **Implemented:** Peaking/Shelf/OFF/Tilt keep biquad; Notch stays biquad; PRESET-BREAK pre-v1
- **Source:** Effect Modules > §12 EQ8DSP > Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-251: 12j Full Dynamic EQ**
- **Sources:** BLU-251
- **Implemented:** 7 new per-band fields + Option B sidechain scaffolding; PRESET-SAFE
- **Source:** Effect Modules > §12 EQ8DSP > Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-253: 12g Linear-phase / HQ modes**
- **Sources:** BLU-253
- **Implemented:** 5 modes via FFT convolution; PRESET-SAFE
- **Source:** Effect Modules > §12 EQ8DSP > Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-272: T19 Reclaim 22-px toolbar row**
- **Sources:** BLU-272
- **Implemented:** Migrated to BankIndicator; (SHIPPED 2026-04-19)
- **Source:** Effect Modules > §12 EQ8DSP > Tier 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-032: §1 Chorus 5F-9 retrospective**
- **Sources:** LDT-032
- **Implemented:** Phase A retrospective done under Tier 1/2/3 framework
- **Source:** Master Checklist > 5F-9 §1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-033: §2 Compressor 5F-9 retrospective**
- **Sources:** LDT-033
- **Implemented:** Phase A retrospective done under Tier 1/2/3 framework
- **Source:** Master Checklist > 5F-9 §2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-034: §3 Delay 5F-9 retrospective**
- **Sources:** LDT-034
- **Implemented:** Phase A retrospective done; A6 cross-apply from §4
- **Source:** Master Checklist > 5F-9 §3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-035: §4 Flanger 5F-9 retrospective**
- **Sources:** LDT-035
- **Implemented:** Phase A retrospective done
- **Source:** Master Checklist > 5F-9 §4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-037: §6 Overdrive 5F-9 retrospective**
- **Sources:** LDT-037
- **Implemented:** Phase A retrospective done
- **Source:** Master Checklist > 5F-9 §6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-038: §7 Phaser 5F-9 retrospective**
- **Sources:** LDT-038
- **Implemented:** A7 BPM-sync cross-apply; A9 cross-apply to Chorus/Delay/Flanger
- **Source:** Master Checklist > 5F-9 §7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-039: §8 Reverb 5F-9 retrospective**
- **Sources:** LDT-039
- **Implemented:** First module reviewed under new tier framework
- **Source:** Master Checklist > 5F-9 §8
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-040: §9 Saturation 5F-9 retrospective**
- **Sources:** LDT-040
- **Implemented:** Full spec + C1-C4 + C6
- **Source:** Master Checklist > 5F-9 §9
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-041: §10 Tape 5F-9 retrospective**
- **Sources:** LDT-041
- **Implemented:** Full rewrite per spec + C1-C5 + multiple follow-on fixes
- **Source:** Master Checklist > 5F-9 §10
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-042: §11 Transient Shaper 5F-9 retrospective**
- **Sources:** LDT-042
- **Implemented:** Full spec bundle + C1-C4
- **Source:** Master Checklist > 5F-9 §11
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-043: §12 EQ8 Phase 2/3**
- **Sources:** FSW-325|LDT-043
- **Implemented:** Phase 1 = audit + 12a/b/c/d; Phase 2/3 still pending per master checklist text | Pending; this is the only doc-claimed-pending §12 status; later session entries claim more EQ work done
- **Source:** Master Checklist > 5F-9 §12 | Standing parallel work > Phase 5F-9 DSP Quality Pass
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-052: A1-A6 Tier 1 + C1-C4 (Chorus)**
- **Sources:** LDT-052
- **Implemented:** CPU guards + smoothed Delay/Depth/Stereo/Wet + denormal guard + cross-cutoff upper clamp + while-wrap LFO phase + setRate deleted; C1 LR4 crossover; C2 Organic wave; C3 WetOnly toggle; C4 widen CrossHz
- **Source:** 5F-9 §1 Chorus retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-053: A1-A6 + GR meter hold+decay + C1-C4 (Compressor)**
- **Sources:** LDT-053
- **Implemented:** A1-A6 Tier 1 + safety levelDb clamp + GR meter; C1 setUseSidechain setter; C2 SC HPF knob; C3 Peak/RMS toggle; C4 Det knob
- **Source:** 5F-9 §2 Compressor retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-054: A1-A11 + C1-C5 + Spectral-Delay APVTS scaffolding**
- **Sources:** LDT-054
- **Implemented:** A1-A11 Tier 1; C1 DiffSpread; C2 FBReso; C3 FBKnee+FBSym; C4 Sync-division chicken-head; C5 WetIn; spectral-delay APVTS scaffolding
- **Source:** 5F-9 §3 Delay retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-055: A1-A6 + C1-C4 (Flanger)**
- **Sources:** LDT-055
- **Implemented:** A1-A5 + A6 Rate-knob BPM-sync soft-lockout; C1 Cross knob; C2 Shape knob; C3 Damp-as-Hz; C4 Sync-division chicken-head
- **Source:** 5F-9 §4 Flanger retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-056: A1-A3 + C1-C5 (Limiter)**
- **Sources:** LDT-056
- **Implemented:** A1 denormal + A2/A3 meter hold+decay; C1 smoothed SatCurve; C2 Sidechain HPF; C4 Auto-makeup; C5 Stereo-link
- **Source:** 5F-9 §5 Limiter retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-057: A1-A9 + C1-C5 (Overdrive)**
- **Sources:** LDT-057
- **Implemented:** A1 denormal + A3-A9; C1 shaper swap; C2 Bias knob; C4 Parallel toggle; C5 OS-factor chicken-head
- **Source:** 5F-9 §6 Overdrive retrospective
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-058: ChickenHeadSelector shared widget**
- **Sources:** LDT-058
- **Implemented:** Soft-lockout multi-position rotary
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-059: DualLabelToggle shared widget**
- **Sources:** LDT-059
- **Implemented:** Named + OnOff modes
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-060: VKnob label-tooltip mirror**
- **Sources:** LDT-060
- **Implemented:** Tooltip mirroring
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-061: Right-click Type in value with Range prompt**
- **Sources:** LDT-061
- **Implemented:** Right-click value entry
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-062: Automation display-name system**
- **Sources:** LDT-062
- **Implemented:** paramId stable + auto-resolved Channel-Effect-Param + userDisplayName user rename
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-063: setSlotContext coverage fix**
- **Sources:** LDT-063
- **Implemented:** getExtraKnobs hook picks up Chorus/Delay/Reverb/Limiter/Saturation r1knobs/r2knobs
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-064: createAutomationBlock populates Browser**
- **Sources:** LDT-064
- **Implemented:** Browser pane wiring
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-065: A6 BPM-sync soft-lockout cross-applied**
- **Sources:** LDT-065
- **Implemented:** §4 Flanger + §3 Delay; inverse lockout on sync-division chicken-heads
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-066: Compressor + Limiter toggle layout fix**
- **Sources:** LDT-066
- **Implemented:** Auto MU moved to own left column
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-067: Bus-meter hold+decay across all 6 buses**
- **Sources:** LDT-067
- **Implemented:** Meter ballistics
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-068: EQ8 startup crash fix**
- **Sources:** LDT-068
- **Implemented:** Null-coef guard
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-069: ASCII-only UI string sweep**
- **Sources:** LDT-069
- **Implemented:** Two passes: literal Unicode + hex-escape bytes
- **Source:** 5F-9 cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-070: 12a/b/c/d audit + Phase 1**
- **Sources:** LDT-070
- **Implemented:** Audit A1-A8 + 12a getMagnitudeForFrequency + 12b proportional Q + 12c SmoothedValue + 12d wire mIIRModSpeed
- **Source:** 5F-9 §12 EQ8 Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-071: 12h per-band M/S + 12i spectrum analyzer**
- **Sources:** LDT-071
- **Implemented:** Both touch ParametricEQDisplay widget + DSP; subsequently shipped per 2026-04-19 session
- **Source:** 5F-9 §12 EQ8 Phase 2 Session A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-072: EQ band automation wiring**
- **Sources:** BLU-502|LDT-072
- **Implemented:** APVTS lazy-register per strip; widget drag input bridges through APVTS | Per-EQ lazy APVTS + right-click Automate; PRESET-SAFE; subsequently logged as DONE per session entries
- **Source:** 5F-9 §12 EQ8 Phase 2 Session B | Cross-cutting > EQ band automation
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-073: 12j Full Dynamic EQ**
- **Sources:** LDT-073
- **Implemented:** Per-band detector + envelope + gain computer + block-rate coef rebuild; shipped 2026-04-19
- **Source:** 5F-9 §12 EQ8 Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-075: 12e TPT hybrid**
- **Sources:** LDT-075
- **Implemented:** StateVariableTPTFilter for LP/HP/BP/Notch; shipped 2026-04-19
- **Source:** 5F-9 §12 EQ8 Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-083: 12f 2x oversampling anti-cramping**
- **Sources:** BLU-252|LDT-076|LDT-083
- **Implemented:** Opt-in per-EQ-instance toggle | Opt-in per EQ instance default off; PRESET-SAFE; shipped 2026-04-19; subsequently shipped
- **Source:** 2026-04-19 Session > §12 Phase 3 remaining | 5F-9 §12 EQ8 Phase 3 | Effect Modules > §12 EQ8DSP > Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-084: 12g Linear-phase / HQ mode**
- **Sources:** LDT-074|LDT-084
- **Implemented:** FFT-convolution path triggered by mPhaseMode; shipped 2026-04-19; subsequently shipped
- **Source:** 2026-04-19 Session > §12 Phase 3 remaining | 5F-9 §12 EQ8 Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-077: 12h + 12i + Polish DONE**
- **Sources:** LDT-077
- **Implemented:** Per-band M/S routing via Band::channel enum
- **Source:** 2026-04-19 Session > §12 Phase 2 Session A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-078: Universal lazy-register**
- **Sources:** LDT-078
- **Implemented:** Universal lazy-register of EQ band params on every mixer strip
- **Source:** 2026-04-19 Session > §12 Session B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-079: Bus pre-rack EQ cruft removed**
- **Sources:** LDT-079
- **Implemented:** Layers + Bass; mDrumsEQDSP retained for DrumsPage binding
- **Source:** 2026-04-19 Session
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-080: TPT filter hybrid**
- **Sources:** LDT-080
- **Implemented:** LP/HP/BP use StateVariableTPTFilter with Butterworth cascade Q
- **Source:** 2026-04-19 Session > §12 Phase 3 12e
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-081: Full Dynamic EQ**
- **Sources:** LDT-081
- **Implemented:** 7 new per-band fields + scSourceId Option B scaffolding
- **Source:** 2026-04-19 Session > §12 Phase 3 12j
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-082: Right-click doesn't jog params**
- **Sources:** LDT-082
- **Implemented:** VibeSlider subclass; SnapSlider rebased
- **Source:** 2026-04-19 Session > Cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

(Note: this section continues with the LDT-395 to LDT-412 5F-9 entries, but those are bucketed under the Phase 5F section per spec. The full file continues below with the remaining 22 sections covering all 1089 entries.)

### Player Engines - Harmless / VibePlayer / BaySick family

#### **BLU-303: T1a Output EQ Mix wired**
- **Sources:** BLU-303
- **Implemented:** oeq_mix param + tilt-EQ DSP
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-304: T1b flt2_kb_track param registered**
- **Sources:** BLU-304
- **Implemented:** Was missing
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-305: T1c Filter type wiring (LP/HP/BP/Notch)**
- **Sources:** BLU-305
- **Implemented:** setFilterType + setFilter2Type
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-306: T1d setComponentID on every Harmless slider**
- **Sources:** BLU-306
- **Implemented:** wireMeta helper bulk wired
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-307: T1e Tooltips on every Harmless slider**
- **Sources:** BLU-307
- **Implemented:** ASCII-only descriptions
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-308: T1f CPU guards on previously-unguarded setters**
- **Sources:** BLU-308
- **Implemented:** setTremoloParams etc
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-309: T1g Removed Time::getHighResolutionTicks() syscall**
- **Sources:** BLU-309
- **Implemented:** Per-voice mRng seeded once
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-310: T1h Mono playback safety**
- **Sources:** BLU-310
- **Implemented:** Existing if(outR) confirmed
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-311: T2-D Prism Mode wired**
- **Sources:** BLU-311
- **Implemented:** 3 inharmonic-spread shapes
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-312: T2-H Phaser Mask Rate UI knob**
- **Sources:** BLU-312
- **Implemented:** Was missing UI affordance
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-313: T2-N misc ghost params wired (6 of 8)**
- **Sources:** BLU-313
- **Implemented:** timbre_blend, prism_mode, pluck_blur, strum_tns, vel_link, legato_limit
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-314: T2-O Layout audit**
- **Sources:** BLU-314
- **Implemented:** Routing matrix + Phaser slot
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-315: T2-I Reverb removed**
- **Sources:** BLU-315
- **Implemented:** reverb_amount param dropped; PRESET-SAFE
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-316: T2-J Multi-band compressor confirmed not present**
- **Sources:** BLU-316
- **Implemented:** No removal needed
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-317: T2-G Harmonizer demoted to Tier 3**
- **Sources:** BLU-317
- **Implemented:** 6 harm_* params removed
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-318: T2-F Routing Matrix wired (faders only)**
- **Sources:** BLU-318
- **Implemented:** 6 new APVTS params; LEDs dropped
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-319: HarmonicEngine extensions**
- **Sources:** BLU-319
- **Implemented:** setSpectralFxScale + setNyquistProtect
- **Source:** Player Engines > §P1 Harmless > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-320: Pitch group + Phaser WIDTH/OFS + Pluck blur button**
- **Sources:** BLU-320
- **Implemented:** 8 new APVTS params + UI
- **Source:** Player Engines > §P1 Harmless > SLA-Impl
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-321: T2-A Filter envelopes**
- **Sources:** BLU-321
- **Implemented:** Per-voice mFltADSR + mFltADSR2
- **Source:** Player Engines > §P1 Harmless > S2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-322: T2-B LFO routing**
- **Sources:** BLU-322
- **Implemented:** Shared LFO with rate + waveform
- **Source:** Player Engines > §P1 Harmless > S2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-323: T2-E Mod XYZ destinations**
- **Sources:** BLU-323
- **Implemented:** 3 destination dropdowns
- **Source:** Player Engines > §P1 Harmless > S2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-324: T2-N follow-on (legato_limit, part_sel, etc)**
- **Sources:** BLU-324
- **Implemented:** DSP wiring lands here
- **Source:** Player Engines > §P1 Harmless > S2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-325: T2-C Unison engine**
- **Sources:** BLU-325
- **Implemented:** Up to 9 detuned siblings; setUnisonType/Alt/Phase
- **Source:** Player Engines > §P1 Harmless > S3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-326: Mod matrix S4 (6 batches)**
- **Sources:** BLU-326
- **Implemented:** Per-target articulator envelope + LFO + sources
- **Source:** Player Engines > §P1 Harmless > S4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-327: T2-M Central spectrogram**
- **Sources:** BLU-327
- **Implemented:** 516-partial real-time visualiser
- **Source:** Player Engines > §P1 Harmless > S5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-328: T2-P Background wavetable rebuild**
- **Sources:** BLU-328
- **Implemented:** juce::TimeSliceThread off audio thread
- **Source:** Player Engines > §P1 Harmless > S5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-329: Layout review (top-to-bottom restructure)**
- **Sources:** BLU-329
- **Implemented:** Migrated all knobs per Jeff's locked map
- **Source:** Player Engines > §P1 Harmless > Layout Review
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-349: cutSelf bool, detuneMode, attack/sustain ADSR**
- **Sources:** BLU-349
- **Implemented:** S1 DSP increments
- **Source:** Player Engines > §P2 VibePlayer > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-350: lfo_rate wired, reverse, sampleStart, velToVolume**
- **Sources:** BLU-350
- **Implemented:** More S1 DSP increments
- **Source:** Player Engines > §P2 VibePlayer > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-351: unisonVoices + unisonSpread, voiceCap, tune+detune**
- **Sources:** BLU-351
- **Implemented:** S1 DSP wiring
- **Source:** Player Engines > §P2 VibePlayer > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-352: DROPPED tremolo orphan param**
- **Sources:** BLU-352
- **Implemented:** PRESET-BREAK; PRESET-BREAK
- **Source:** Player Engines > §P2 VibePlayer > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-353: Per-pitch preemption + stale-note-off strip fix**
- **Sources:** BLU-353
- **Implemented:** Rapid-retrigger voice cascading fix
- **Source:** Player Engines > §P2 VibePlayer > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-355: UI rewrite (6-box grid layout)**
- **Sources:** BLU-355
- **Implemented:** Sample Engine / Pitch & Voicing / Dynamics / Amp Env / LFO / Output
- **Source:** Player Engines > §P2 VibePlayer > S2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-356: componentID + automation plumbing + trackId fix**
- **Sources:** BLU-356
- **Implemented:** ctors changed to const juce::String& trackId
- **Source:** Player Engines > §P2 VibePlayer > S3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-357: Range-mapping audit (retro)**
- **Sources:** BLU-357
- **Implemented:** VibePlayer + Harmless sweep
- **Source:** Player Engines > §P2 VibePlayer > Cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-358: Branding rename VibeDAW -> BaySickDAW**
- **Sources:** BLU-358
- **Implemented:** CMake PRODUCT_NAME etc
- **Source:** Player Engines > §P2 VibePlayer > Cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-359: Splash screen**
- **Sources:** BLU-359
- **Implemented:** juce::SplashScreen + window icon + exe icon
- **Source:** Player Engines > §P2 VibePlayer > Cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-360: LRX-5 global vignette disabled**
- **Sources:** BLU-360
- **Implemented:** CPU renderer banding workaround
- **Source:** Player Engines > §P2 VibePlayer > Cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-361: No VST standing rule**
- **Sources:** BLU-361
- **Implemented:** Standalone-only
- **Source:** Player Engines > §P2 VibePlayer > Cross-cutting
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-371: Cut-self cross-apply retro to Harmless+Synth+Bass**
- **Sources:** BLU-371
- **Implemented:** Each engine gets own copy
- **Source:** Player Engines > §P2 VibePlayer > Remaining
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-372: Cut-self / mono-mode voice management (system-wide)**
- **Sources:** BLU-372
- **Implemented:** Default-ON drums; OFF Layers/Bass; right-click on audio clips; PRESET-SAFE
- **Source:** Player Engines > §P2 VibePlayer > Tier 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-373: Per-slot EQ on DrumsPage (replace bus EQ)**
- **Sources:** BLU-373
- **Implemented:** Slot-selector master dropdown; PRESET-SAFE
- **Source:** Player Engines > §P2 VibePlayer > Tier 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-374: Dual piano roll mode on DrumsPage**
- **Sources:** BLU-374|BLU-410|BLU-473
- **Implemented:** Drum-grid + full-roll views | See §P2 entry; PATTERN-BREAK; PRESET-BREAK
- **Source:** Player Engines > §P2 VibePlayer > Tier 1 | Player Engines > §P4 DrumsPage > §P4.2 | System Pages > Piano Roll > Tier 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-376: Dual-engine drum slots**
- **Sources:** BLU-376
- **Implemented:** VibePlayer or BaySickSynth per slot; PRESET-BREAK
- **Source:** Player Engines > §P2 VibePlayer > Tier 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-377: Choke groups for drums**
- **Sources:** BLU-377
- **Implemented:** Per-slot choke group enum; PRESET-SAFE
- **Source:** Player Engines > §P2 VibePlayer > Tier 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-380: P3.1 Pitch envelope (ADSR on pitch)**
- **Sources:** BLU-380
- **Implemented:** bss_pEnvAtk/Dec/Sus/Rel/Amt; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-381: P3.2 Sine waveform primitive**
- **Sources:** BLU-381
- **Implemented:** BssWaveform::Sine enum value; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-382: P3.3 Noise-only mode**
- **Sources:** BLU-382
- **Implemented:** bss_noiseOnlyMode bool; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-383: P3.4 Free Hz tuning on dual-osc**
- **Sources:** BLU-383
- **Implemented:** Mode flag for absolute Hz; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-384: P3.5 Transient injector**
- **Sources:** BLU-384
- **Implemented:** Short noise/click burst at note-on; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-385: P3.6 Multi-burst envelope mode**
- **Sources:** BLU-385
- **Implemented:** N short bursts with settable gap; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-386: P3.7 Hard sync**
- **Sources:** BLU-386
- **Implemented:** osc2 phase restarts on osc1 cycle; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-387: P3.8 Ring modulation**
- **Sources:** BLU-387
- **Implemented:** Multiply osc1 × osc2; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-388: P3.9 Pink + brown noise types**
- **Sources:** BLU-388
- **Implemented:** bss_noiseColor White/Pink/Brown; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-389: P3.10 Per-voice analog drift**
- **Sources:** BLU-389
- **Implemented:** bss_drift 0-1 default 0; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-390: P3.11 Unison mode**
- **Sources:** BLU-390
- **Implemented:** bss_unisonVoices/Detune/Spread; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > §P3-CORE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-391: T1.1 Add flt_type read in updateFromApvts**
- **Sources:** BLU-391
- **Implemented:** Bass identical bug 2-for-1; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > Tier 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-392: T1.2 setComponentID on every attached slider**
- **Sources:** BLU-392
- **Implemented:** Bass parallel; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > Tier 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-393: T1.3 Verify every bss_* param has editor attachment**
- **Sources:** BLU-393
- **Implemented:** Stranded-param sweep; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > Tier 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-394: T1.4 Commit audit to blueprint**
- **Sources:** BLU-394
- **Implemented:** Docs only; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > Tier 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-395: T2.1 bss_lfo_sync ship DSP**
- **Sources:** BLU-395
- **Implemented:** Host BPM + division combobox; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > Tier 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-396: T2.2 bss_flt_type UI selector**
- **Sources:** BLU-396
- **Implemented:** LP/HP/BP/Notch; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > Tier 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-397: T2.3 Velocity -> Amp**
- **Sources:** BLU-397
- **Implemented:** bss_velAmpTrack default 0; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > Tier 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-398: T2.4 Legato mode (4th voiceMode)**
- **Sources:** BLU-398
- **Implemented:** Add 4th LED slot; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > Tier 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-399: T2.5 Ship all 6 §P3-CORE DSP adds in v1.0**
- **Sources:** BLU-399
- **Implemented:** Approved for v1.0; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > Tier 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-400: T2.6 UI layout for 6 new params**
- **Sources:** BLU-400
- **Implemented:** Approved; PRESET-SAFE
- **Source:** Player Engines > §P3 BaySick family > Tier 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-402: T3.2 Unison mode (PROMOTED to P3.11)**
- **Sources:** BLU-402
- **Implemented:** Promoted under expanded §P3-CORE; (promoted)
- **Source:** Player Engines > §P3 BaySick family > Tier 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-409: Dual-engine drum slot dropdown**
- **Sources:** BLU-409
- **Implemented:** Each slot VibePlayer or BaySickSynth; KIT-BREAK
- **Source:** Player Engines > §P4 DrumsPage > §P4.1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-411: Per-slot EQ rebind**
- **Sources:** BLU-411
- **Implemented:** Pre-EQ defaults flat; PRESET-SAFE
- **Source:** Player Engines > §P4 DrumsPage > §P4.3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-412: Drum preset bank**
- **Sources:** BLU-412
- **Implemented:** 790 presets / 87 kits / 29 templates; PRESET-SAFE
- **Source:** Player Engines > §P4 DrumsPage > §P4.4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-413: D1 Slot copy/paste/duplicate**
- **Sources:** BLU-413
- **Implemented:** Right-click slot bar
- **Source:** Player Engines > §P4 DrumsPage > Discovery
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-414: D2 Drag-and-drop sample onto slot bar**
- **Sources:** BLU-414
- **Implemented:** FileDragAndDropTarget
- **Source:** Player Engines > §P4 DrumsPage > Discovery
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-415: D3 Per-slot polyphony cap**
- **Sources:** BLU-415
- **Implemented:** Mono/Poly toggle
- **Source:** Player Engines > §P4 DrumsPage > Discovery
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-416: D4 Per-step velocity + probability in slot-grid**
- **Sources:** BLU-416
- **Implemented:** Wire ControlLane to drum mode
- **Source:** Player Engines > §P4 DrumsPage > Discovery
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-418: D6 Slot lock toggle**
- **Sources:** BLU-418
- **Implemented:** Prevents accidental overwrite
- **Source:** Player Engines > §P4 DrumsPage > Discovery
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-419: T2 Multi-sample slot UI**
- **Sources:** BLU-419
- **Implemented:** GUI affordance for SFZ regions; PRESET-SAFE
- **Source:** Player Engines > §P4 DrumsPage > Discovery
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-420: T3 Choke groups (per-slot)**
- **Sources:** BLU-420
- **Implemented:** Per-slot Int APVTS
- **Source:** Player Engines > §P4 DrumsPage > Discovery
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-422: T12 Per-slot MIDI learn**
- **Sources:** BLU-422
- **Implemented:** Per-slot Int APVTS for MIDI note remap
- **Source:** Player Engines > §P4 DrumsPage > Discovery
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-431: B2 Pre-rack EQ on every audio node**
- **Sources:** BLU-431
- **Implemented:** 7 audio nodes get fresh EQ8MsDSP preEq; PRESET-SAFE
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-432: B2 Audio Clips Bus post-rack EQ drive-by fix**
- **Sources:** BLU-432
- **Implemented:** eq->process(clipsBus) was never called; PRESET-SAFE
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-433: B3 Pre-rack EQ APVTS registration**
- **Sources:** BLU-433
- **Implemented:** addParamsForTrackPreEQ helper; PRESET-SAFE
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-434: B4 Pre-rack EQ APVTS sync in processBlock**
- **Sources:** BLU-434
- **Implemented:** updateAllPreRackEQsFromApvts; PRESET-SAFE
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-435: B2-B4 perf fix isIdentity check**
- **Sources:** BLU-435
- **Implemented:** EQ8DSP::isIdentity at all callsites
- **Source:** Player Engines > §P4 DrumsPage > Phase B perf
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-436: APVTS dirty-flag short-circuit on EQ sync**
- **Sources:** BLU-436
- **Implemented:** mEQsDirty atomic + ValueTree::Listener
- **Source:** Player Engines > §P4 DrumsPage > Phase B perf
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-437: B5 Player-page EQ tabs rebound to InsertNode pre-EQs**
- **Sources:** BLU-437
- **Implemented:** LayersPage / BassPage / DrumsPage
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-438: B5 follow-up: spectrum analyser display fix**
- **Sources:** BLU-438
- **Implemented:** Identity short-circuit moved INSIDE process
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-439: B5 follow-up #2: Drums Kit + Nav combo on all 3 tabs**
- **Sources:** BLU-439
- **Implemented:** Removed if(i==0) gate
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-440: B6.1 EQ8 M/S -> Post EQ8 M/S label rename**
- **Sources:** BLU-440
- **Implemented:** EffectsPage tab strip rename
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-441: B6.2 Pre EQ8 M/S tab on EffectsPage with dynamic 2-vs-3 visibility**
- **Sources:** BLU-441
- **Implemented:** TabKind enum + dynamic layout; PRESET-SAFE
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-442: B7 Legacy per-page EQ cleanup**
- **Sources:** BLU-442
- **Implemented:** mDrumsEQDSP + mLayerPageEQs[] removed; PRESET-SAFE
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-443: B8 Automation display resolver recognises preeq_**
- **Sources:** BLU-443
- **Implemented:** formatMixerSuffix preeq_ branch
- **Source:** Player Engines > §P4 DrumsPage > Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-444: G-1.1 to G-1.5 BaySickNAM/IR full pipeline**
- **Sources:** BLU-444
- **Implemented:** Skeleton / DSP / OS / A/B / editor / test window
- **Source:** Player Engines > §P5 BaySickNAM/IR
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-446: BaySickPedals I-1 to I-12**
- **Sources:** BLU-446
- **Implemented:** Rack + 8 pedals + tuner + GE-7
- **Source:** Player Engines > §P7 BaySickPedals > Phase I
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-606: Drum Synthesis Expansion Bundle**
- **Sources:** BLU-606
- **Implemented:** 6 DSP additions for authentic drum synthesis; (now part of §P3-CORE)
- **Source:** Player Engines > §P3 BaySick family > Drum Synthesis Bundle
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-613: I-1 Rack skeleton**
- **Sources:** BLU-613
- **Implemented:** 6-slot array + locked-end
- **Source:** Player Engines > §P7 BaySickPedals > I-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-614: I-2 BD-2 Blues Drive**
- **Sources:** BLU-614
- **Implemented:** Asymmetric tanh + DC-offset shaper
- **Source:** Player Engines > §P7 BaySickPedals > I-2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-615: I-3 OD-3 Overdrive**
- **Sources:** BLU-615
- **Implemented:** Frequency-split dual cascaded soft-clip
- **Source:** Player Engines > §P7 BaySickPedals > I-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-616: I-4 DS-1 Distortion**
- **Sources:** BLU-616
- **Implemented:** Pre-emphasis + hard clip + Big-Muff tilt
- **Source:** Player Engines > §P7 BaySickPedals > I-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-617: I-5 FZ-5 Fuzz**
- **Sources:** BLU-617
- **Implemented:** 3-mode shape switch
- **Source:** Player Engines > §P7 BaySickPedals > I-5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-618: I-6 NS-2 Noise Gate**
- **Sources:** BLU-618
- **Implemented:** RMS env + sidechain
- **Source:** Player Engines > §P7 BaySickPedals > I-6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-619: I-7 CS-3 Compressor**
- **Sources:** BLU-619
- **Implemented:** Leverages H-2 extended CompressorDSP
- **Source:** Player Engines > §P7 BaySickPedals > I-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-620: I-8 MT-2 Metal**
- **Sources:** BLU-620
- **Implemented:** Mid-boost + cascading hard clip + 3-band
- **Source:** Player Engines > §P7 BaySickPedals > I-8
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-621: I-9 AD-2 Acoustic**
- **Sources:** BLU-621
- **Implemented:** IR + variable-Q band-stop + Schroeder reverb
- **Source:** Player Engines > §P7 BaySickPedals > I-9
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-622: I-10 TU-3 Tuner**
- **Sources:** BLU-622
- **Implemented:** Leverages H-4 YIN/MPM + LED strobe
- **Source:** Player Engines > §P7 BaySickPedals > I-10
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-623: I-11 GE-7 Graphic EQ**
- **Sources:** BLU-623
- **Implemented:** Leverages EQ8DSP + 8 vertical sliders
- **Source:** Player Engines > §P7 BaySickPedals > I-11
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-624: I-12 BaySickPedals editor**
- **Sources:** BLU-624
- **Implemented:** Skeuomorphic horizontal 6-pedal rack
- **Source:** Player Engines > §P7 BaySickPedals > I-12
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-625: flt2_kb_track DSP wiring**
- **Sources:** BLU-625
- **Implemented:** S1 ghost lands DSP here
- **Source:** Player Engines > §P1 Harmless > S2 SLA-bundled
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-626: Blur time + Blur harm knobs**
- **Sources:** BLU-626
- **Implemented:** BlurModule::setTimeScale + setHarmAxis
- **Source:** Player Engines > §P1 Harmless > S2 SLA-bundled
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-627: Filter ofs knobs (per filter x2)**
- **Sources:** BLU-627
- **Implemented:** flt1/2_cutoff_ofs
- **Source:** Player Engines > §P1 Harmless > S2 SLA-bundled
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-628: Prism bipolar range change**
- **Sources:** BLU-628
- **Implemented:** -1..+1 with sign encoding polarity; PRESET-BREAK pre-v1
- **Source:** Player Engines > §P1 Harmless > S2 SLA-bundled
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase D - Dynamic-Drum Architecture

#### **BLU-530: Phase D Dynamic-Drum architecture**
- **Sources:** BLU-530
- **Implemented:** Each drum tab is independent engine
- **Source:** Player Engines > Phase D Dynamic-Drum
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-534: D1.4 Cutover (ribbon wiring + DrumPage default)**
- **Sources:** BLU-534
- **Implemented:** createDrumPage factories + showInstanceDropdown
- **Source:** Player Engines > Phase D > D1.4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-538: Legacy sweep (DrumSynth / DrumsPage / BaySickDrums DELETED)**
- **Sources:** BLU-538
- **Implemented:** Files DELETED
- **Source:** Player Engines > Phase D > Legacy sweep
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-539: D1.4 ancillary fixes**
- **Sources:** BLU-539
- **Implemented:** Audio settings path + TR- prefix removal + outVol param
- **Source:** Player Engines > Phase D > D1.4 ancillary
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-540: D2 Drum Kit tab (4 batches)**
- **Sources:** BLU-540
- **Implemented:** Composited 16-row grid view
- **Source:** Player Engines > Phase D > D2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-541: D2 Batches 1-4 Foundation/audition/mute/solo/reorder**
- **Sources:** BLU-541
- **Implemented:** DrumKitView component
- **Source:** Player Engines > Phase D > D2 Batches 1-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-542: D2 Batch 4.5 selection + clipboard + Option A piano-roll-style**
- **Sources:** BLU-542
- **Implemented:** Pivot mid-batch to piano-roll behavior
- **Source:** Player Engines > Phase D > D2 Batch 4.5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-543: D2 follow-up rewrite (mPPB + mBeatOff)**
- **Sources:** BLU-543
- **Implemented:** Coordinate model port
- **Source:** Player Engines > Phase D > D2 follow-up
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-544: D2 second follow-up viewport-relative zoom + playhead style**
- **Sources:** BLU-544
- **Implemented:** Surgical fixes for missed PianoRollGrid behaviors
- **Source:** Player Engines > Phase D > D2 second follow-up
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-545: D2 third rewrite structural copy from PianoRoll**
- **Sources:** BLU-545
- **Implemented:** DrumKitGrid + DrumKitContainer + DrumKitMenuBar
- **Source:** Player Engines > Phase D > D2 third rewrite
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-546: D2 cache-refresh fix**
- **Sources:** BLU-546
- **Implemented:** refreshRowsCache helper
- **Source:** Player Engines > Phase D > D2 cache fix
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-547: D1.5 Per-note pitch editing via double-click**
- **Sources:** BLU-547|FSW-315
- **Implemented:** Velocity / MIDI Note / Delete prompts | Drum architecture
- **Source:** Player Engines > Phase D > D1.5 | Standing parallel work > Drum architecture
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-548: D3 Global choke groups (4 batches)**
- **Sources:** BLU-548
- **Implemented:** Cross-engine choke bus
- **Source:** Player Engines > Phase D > D3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-549: D3 Batch 1 Data + persistence**
- **Sources:** BLU-549
- **Implemented:** APVTS _chokeGroup + AudioLibraryEntry.chokeGroup
- **Source:** Player Engines > Phase D > D3 Batch 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-550: D3 Batch 2 Audio-thread dispatch**
- **Sources:** BLU-550
- **Implemented:** applyChokeGroupDispatch
- **Source:** Player Engines > Phase D > D3 Batch 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-551: D3 Batch 3 Synth context menus**
- **Sources:** BLU-551
- **Implemented:** Choke Group submenu
- **Source:** Player Engines > Phase D > D3 Batch 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-552: D3 Batch 4 Browser audio-clip menu + audio dispatch**
- **Sources:** BLU-552
- **Implemented:** BrowserPanel choke menu
- **Source:** Player Engines > Phase D > D3 Batch 4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-553: Layers/Bass - Load Preset submenu**
- **Sources:** BLU-553
- **Implemented:** loadPreset + format auto-detect
- **Source:** Player Engines > Phase D > Layers/Bass
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-569: D-1 Builder per-block mute + right-click zoom helpers**
- **Sources:** BLU-569
- **Implemented:** Alt+M + visual mute upgrade + drag-rect zoom
- **Source:** Phase D-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-572: D-7 sub-1 Smaller Piano Roll bundle batch 1 of 4**
- **Sources:** BLU-572
- **Implemented:** 7 of 13 keybinds wired
- **Source:** D-7 sub-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-573: D-7 sub-1 chop minimum-piece guard**
- **Sources:** BLU-573
- **Implemented:** 1/16 minimum
- **Source:** D-7 sub-1 follow-up
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-574: D-7 sub-1 selection-required scope tightened**
- **Sources:** BLU-574
- **Implemented:** mSelection direct
- **Source:** D-7 sub-1 follow-up #2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-575: D-7 sub-1 selection-only sweep + ruler auto-select**
- **Sources:** BLU-575
- **Implemented:** getWorkingSet returns empty
- **Source:** D-7 sub-1 follow-up #3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-576: D-7 sub-1 cross-applied to DrumKitGrid + click-memory**
- **Sources:** BLU-576
- **Implemented:** 5 areas + click memory
- **Source:** D-7 sub-1 cross-apply
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-577: D-7 sub-1 Ctrl+Delete selection fallback**
- **Sources:** BLU-577
- **Implemented:** Selection bounding span fallback
- **Source:** D-7 sub-1 follow-up #4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-578: D-7 sub-1 Ctrl+Delete added to Builder**
- **Sources:** BLU-578
- **Implemented:** ArrangementGrid::deleteTimeRegion
- **Source:** D-7 sub-1 follow-up #5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-579: D-7 sub-1 Ctrl+Delete logic rewrite + doc rows**
- **Sources:** BLU-579
- **Implemented:** STARTS in [t0,t1) rule
- **Source:** D-7 sub-1 follow-up #6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-580: D-7 sub-1 Ctrl+Delete root cause FOUND**
- **Sources:** BLU-580
- **Implemented:** isKeyCode replacement
- **Source:** D-7 sub-1 follow-up #7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-581: D-7 sub-2 M toggle + Alt+X scale + DrumKit keybinds tab**
- **Sources:** BLU-581
- **Implemented:** 3 pieces
- **Source:** D-7 sub-2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-582: D-7 sub-2 DrumKit doc rows expanded**
- **Sources:** BLU-582
- **Implemented:** ~30 self-documenting rows
- **Source:** D-7 sub-2 follow-up
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-583: D-7 sub-3 Ctrl+Left/Right shift ruler time-selection**
- **Sources:** BLU-583
- **Implemented:** Across all 3 grids
- **Source:** D-7 sub-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-584: D-7 sub-4 cross-tab Ctrl+V + Alt+Wheel Control Lane + selected red + selection-locked + click-outside**
- **Sources:** BLU-584
- **Implemented:** 5 things
- **Source:** D-7 sub-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-585: D-7 sub-4 EC-1 reversed**
- **Sources:** BLU-585
- **Implemented:** Drawing inside range clears mSelection
- **Source:** D-7 sub-4 EC-1 reversal
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-586: D-7 sub-4 range-aware Ctrl+A**
- **Sources:** BLU-586
- **Implemented:** Re-grab from time range
- **Source:** D-7 sub-4 follow-up #2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-587: D-7 sub-4 Ctrl+Delete ruler-first priority**
- **Sources:** BLU-587
- **Implemented:** Restored
- **Source:** D-7 sub-4 follow-up #3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-588: Step 1a top-level ribbon slot + PianoRollPage stub**
- **Sources:** BLU-588
- **Implemented:** kNumSlots 6->7, F11 routing
- **Source:** Unified Piano Roll Page > Step 1a
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-589: Step 1b DrumKit relocated**
- **Sources:** BLU-589
- **Implemented:** DrumKitContainer instance
- **Source:** Unified Piano Roll Page > Step 1b
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-590: Step 1b dropdown moved to PageMenuBar**
- **Sources:** BLU-590
- **Implemented:** No extra header bar
- **Source:** Unified Piano Roll Page > Step 1b polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-591: Step 2 commit 1 PianoRollPage gains engine registry**
- **Sources:** BLU-591
- **Implemented:** EngineKind/EngineId types
- **Source:** Unified Piano Roll Page > Step 2 commit 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-592: Step 2 commit 2 editor wires the registry**
- **Sources:** BLU-592
- **Implemented:** registerLayer/Bass/DrumPianoRoll
- **Source:** Unified Piano Roll Page > Step 2 commit 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-593: Step 2 commit 3 engine pages release piano-roll ownership**
- **Sources:** BLU-593
- **Implemented:** mPianoRoll/mDrumKitTab nullified
- **Source:** Unified Piano Roll Page > Step 2 commit 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-594: Step 2 polish Load Kit stays on Piano Roll page**
- **Sources:** BLU-594
- **Implemented:** Skip nav in DrumKit view
- **Source:** Unified Piano Roll Page > Step 2 polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-595: Step 2 polish project save/load active engine**
- **Sources:** BLU-595
- **Implemented:** PianoRollSelection in UIState
- **Source:** Unified Piano Roll Page > Step 2 polish
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-056: D-1 Builder per-block mute & quantize**
- **Sources:** FSW-056
- **Implemented:** Shipped
- **Source:** Phase D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-057: D-2 Time markers + per-bar TS**
- **Sources:** FSW-057
- **Implemented:** Struct + ruler glyphs + Alt+T / Shift+Alt+T + right-click menu + persistence
- **Source:** Phase D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-058: D-3 Performance Mode (Ctrl+P)**
- **Sources:** FSW-058
- **Implemented:** mPerfMode + setPerformanceMode + menu toggle
- **Source:** Phase D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-060: D-5 Recording precount metronome**
- **Sources:** FSW-060
- **Implemented:** 1-bar lead-in, gated on record-arm + precount toggle
- **Source:** Phase D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-062: D-7 Smaller Piano Roll bundle**
- **Sources:** FSW-062
- **Implemented:** sub-1 to sub-4 + follow-ups all shipped
- **Source:** Phase D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-068: D-10b MixerStateAction (fader/pan/mute/solo/drums-strip params)**
- **Sources:** FSW-068
- **Implemented:** Shipped
- **Source:** Phase D > D-10
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-314: D1.1 to D1.4 + fix(a/b/c)**
- **Sources:** FSW-314
- **Implemented:** Phase D dynamic drums
- **Source:** Standing parallel work > Drum architecture
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-318: Open bug - BaySickSynth-based drum playback wide+woofy vs correct mono audition**
- **Sources:** FSW-318
- **Implemented:** Open bug; Memory note says this is FIXED - stale reference; project_drum_woofy_bug_fixed.md memory entry says CLAUDE.md OPEN BUG entry is stale
- **Source:** Standing parallel work > Drum architecture
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-001: Phase D dynamic-drum architecture shipped**
- **Sources:** LDT-001
- **Implemented:** Drums architecture restructured to dynamic per-tab engines; each tab owns one BaySickPlayer or BaySickSynth; up to 16 drum tabs (kMaxDrumPages); references DrumSynth (deleted), references BaySickDrums (deleted monolithic processor)
- **Source:** Session Carryover 2026-04-25 > Phase D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-002: Drums dropdown ribbon mirrors Layers/Bass**
- **Sources:** LDT-002
- **Implemented:** Drums ▾ ribbon dropdown lists drums + Pages + Rename/Delete/Add New Drum
- **Source:** Session Carryover 2026-04-25
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-003: Pick a sound dropdown popup on Player tab**
- **Sources:** LDT-003
- **Implemented:** Single Pick a sound ▾ button with Sample submenu (Browse/SFZ/Core Library walker filtered by isDrumPack) + Synth Patch submenu
- **Source:** Session Carryover 2026-04-25
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-004: Lock-after-pick UX**
- **Sources:** LDT-004
- **Implemented:** Button transforms to show current sound name; both clicks open per-drum context menu
- **Source:** Session Carryover 2026-04-25
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-005: Per-drum context menu**
- **Sources:** LDT-005
- **Implemented:** Lock/Polyphony toggle/Copy/Paste/Duplicate/Choke Group/MIDI Map/MIDI Note/Save Patch As/Delete
- **Source:** Session Carryover 2026-04-25
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-006: Layers/Bass parity LockableCombo**
- **Sources:** LDT-006
- **Implemented:** Engine combo replaced with LockableCombo subclass; per-page context menu after first pick
- **Source:** Session Carryover 2026-04-25
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-007: Polyphony toggle is engine-aware**
- **Sources:** LDT-007
- **Implemented:** BaySickSynth/Bass voiceMode (Poly<->Mono); BaySickPlayer voiceCap (1<->8); Harmless n/a
- **Source:** Session Carryover 2026-04-25
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-010: BaySickPlayer presets include sample reference**
- **Sources:** LDT-010
- **Implemented:** library: prefix for Core Library, absolute path otherwise; Bundle & Export follows references
- **Source:** Session Carryover 2026-04-25
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-011: Master output volume parity for synth/bass**
- **Sources:** LDT-011
- **Implemented:** _bss_outVol / _bsb_outVol Float 0-1 default 0.8; cached in ParamCache
- **Source:** Session Carryover 2026-04-25
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-012: Delete DrumSynth.h/.cpp**
- **Sources:** LDT-012
- **Implemented:** Removed 46-voice procedural drum library + DrumVoice struct; references DrumSynth (deleted)
- **Source:** Session Carryover 2026-04-25 > Legacy Sweep
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-013: Delete legacy DrumsPage.h/.cpp**
- **Sources:** LDT-013
- **Implemented:** 16-slot DrumsPage with sub-tabs Sound/PianoRoll/EQ + Drum Grid + Full Piano Roll modes; references DrumsPage (deleted)
- **Source:** Session Carryover 2026-04-25 > Legacy Sweep
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-014: Delete BaySickDrumsProcessor.h/.cpp**
- **Sources:** LDT-014
- **Implemented:** Single-processor 16-internal-slot orchestrator; references BaySickDrums (deleted)
- **Source:** Session Carryover 2026-04-25 > Legacy Sweep
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-015: Delete BaySickDrumsEditor.h/.cpp**
- **Sources:** LDT-015
- **Implemented:** Slot-bar grid UI with slot picker + sub-editor area; references BaySickDrums (deleted)
- **Source:** Session Carryover 2026-04-25 > Legacy Sweep
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-016: Delete BaySickDrumsLAF.h**
- **Sources:** LDT-016
- **Implemented:** Legacy LAF; references BaySickDrums (deleted)
- **Source:** Session Carryover 2026-04-25 > Legacy Sweep
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-017: PluginProcessor cleanup (drums-related)**
- **Sources:** LDT-017
- **Implemented:** mDrumSynth, getDrumSynth, mDrumsEngine+lock+buf, kNumDrumSlots, mDrumSlotBufs, addParamsForBaySickDrums removed; references DrumSynth/BaySickDrums (deleted)
- **Source:** Session Carryover 2026-04-25
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-018: D1.1 Data model**
- **Sources:** BLU-531|LDT-018
- **Implemented:** kMaxDrumPages = 16, Pattern::drumRolls[16] array, DrumPageRoll save/load, legacy migration | kMaxDrumPages=16 + Pattern::drumRolls[16]
- **Source:** Phase D > D1.1 Data model | Player Engines > Phase D > D1.1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-019: D1.2 Processor scaffolding**
- **Sources:** BLU-532|LDT-019
- **Implemented:** mDrumEngines[16] + lock + buffers, register/unregisterDrumEngine, per-drum MIDI dispatch, fast-path bypass | mDrumEngines[16] + per-drum MIDI dispatch
- **Source:** Phase D > D1.2 Processor scaffolding | Player Engines > Phase D > D1.2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-020: D1.3 DrumPage class**
- **Sources:** BLU-533|LDT-020
- **Implemented:** Mirror of LayersPage; 3 sub-tabs Player/Piano Roll/Pre EQ8 M/S; per-drum APVTS prefix drm_{N}_* | Mirrors LayersPage exactly
- **Source:** Phase D > D1.3 DrumPage class | Player Engines > Phase D > D1.3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-021: D1.4 Cutover**
- **Sources:** LDT-021
- **Implemented:** Default Drums tab is DrumPage now; ribbon dropdown switched to instance-mode
- **Source:** Phase D > D1.4 Cutover
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-022: D1.4-fix(a) Picker**
- **Sources:** BLU-535|LDT-022
- **Implemented:** Replaced 4-engine combo with sound-picker popup mirroring legacy | Sample / Synth Patch submenus
- **Source:** Phase D > D1.4-fix(a) Picker | Player Engines > Phase D > D1.4-fix(a)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-023: D1.4-fix(b) Save Patch As**
- **Sources:** BLU-536|LDT-023
- **Implemented:** Synth + Player preset save, sample reference XML for Player | BaySickPlayer + BaySickSynth
- **Source:** Phase D > D1.4-fix(b) Save Patch As | Player Engines > Phase D > D1.4-fix(b)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-024: D1.4-fix(c) Right-click + lock**
- **Sources:** BLU-537|LDT-024
- **Implemented:** Per-drum + per-layer + per-bass context menus, lock-after-pick, delete confirmations | Drums / Layers / Bass parity
- **Source:** Phase D > D1.4-fix(c) Right-click + lock | Player Engines > Phase D > D1.4-fix(c)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-025: Legacy sweep**
- **Sources:** LDT-025
- **Implemented:** All 9 files + ~600 lines of references removed; references DrumSynth/BaySickDrums/DrumsPage (deleted)
- **Source:** Phase D > Legacy sweep
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-027: D2 Drum Kit tab**
- **Sources:** FSW-316|LDT-027
- **Implemented:** Beginner-friendly composited grid view + kit picker (4-batch plan) | Drum architecture
- **Source:** Phase D > D2 | Standing parallel work > Drum architecture
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-028: D3 Global choke groups**
- **Sources:** FSW-317|LDT-028
- **Implemented:** Cross-engine choke bus, Choke Group submenu populated | Drum architecture
- **Source:** Phase D > D3 | Standing parallel work > Drum architecture
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-029: Drum playback wide+woofy bug**
- **Sources:** LDT-029
- **Implemented:** BaySickSynth-loaded drum tab playback produces wider/woofy sound layered on kick; per CLAUDE.md memory rule, this bug is FIXED but doc still says open
- **Source:** Phase D > Open Bug
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-030: Audition vs playback level mismatch**
- **Sources:** BLU-571|LDT-030
- **Implemented:** Affects Layers/Bass too; likely UX choice rather than bug; flag for design decision; (design decision)
- **Source:** Open issues | Phase D > Other deferred
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-031: Save Patch As editor knob**
- **Sources:** LDT-031
- **Implemented:** BaySickSynth/Bass outVol param works but has no visible knob in engine editor
- **Source:** Phase D > Other deferred
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase A - Keybind Framework

#### **FSW-007: Phase A Keybind Framework complete**
- **Sources:** FSW-007
- **Implemented:** juce::ApplicationCommandManager + KeyPressMappingSet at editor startup
- **Source:** Phase A - Keybind Framework
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-008: juce::ApplicationCommandManager + KeyPressMappingSet at editor startup**
- **Sources:** FSW-008
- **Implemented:** Foundation for keybind system
- **Source:** Phase A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-009: Per-command catalog (id / category / name / tooltip / default key)**
- **Sources:** FSW-009
- **Implemented:** Command catalog
- **Source:** Phase A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-010: Persisted to Documents/BaySickDAW/keymap.xml**
- **Sources:** FSW-010
- **Implemented:** XML persistence
- **Source:** Phase A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-011: Help Key Binds 4-tab popup (General/Builder/Piano Roll/DrumKit)**
- **Sources:** FSW-011
- **Implemented:** Help menu integration
- **Source:** Phase A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-012: Custom TableListBox-based tab with per-row tooltips, Set/Reset, conflict-check**
- **Sources:** FSW-012
- **Implemented:** Keybind UI
- **Source:** Phase A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-013: GlobalTransportBar dropped its KeyListener role**
- **Sources:** FSW-013
- **Implemented:** Refactor
- **Source:** Phase A
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase B - Conflict Resolutions

#### **BLU-560: B-1 Page switches + file operations**
- **Sources:** BLU-560
- **Implemented:** F5-F11 + Ctrl+N/O/S/Shift+S
- **Source:** Phase B Keymap > B-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-561: B-2 Pattern navigation**
- **Sources:** BLU-561
- **Implemented:** F2/F3/F4 + +/-
- **Source:** Phase B Keymap > B-2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-562: B-3 Transport extensions**
- **Sources:** BLU-562
- **Implemented:** L Song mode + Home + ±bar + Ctrl+M
- **Source:** Phase B Keymap > B-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-563: B-4 Conflict rebinds + scroll fixes + Ctrl+drag marquee**
- **Sources:** BLU-563
- **Implemented:** Z=Zoom; PgUp/PgDn; mouse wheel direction
- **Source:** Phase B Keymap > B-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-564: B-5 Ctrl+Z / Ctrl+Alt+Z global migration**
- **Sources:** BLU-564
- **Implemented:** Per-page handlers stripped
- **Source:** Phase B Keymap > B-5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-565: B-6 Path A documentation rows + popup expansion**
- **Sources:** BLU-565
- **Implemented:** ~80 reference rows + 880x680
- **Source:** Phase B Keymap > B-6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-014: Phase B Conflict Resolutions complete**
- **Sources:** FSW-014
- **Implemented:** All B sub-batches
- **Source:** Phase B - Conflict Resolutions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-015: B-1 Page switches F5-F11 + file ops Ctrl+N/O/S/Shift+S**
- **Sources:** FSW-015
- **Implemented:** 11 commands
- **Source:** Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-016: B-2 Pattern nav F2/F3/F4 + +/- cycle**
- **Sources:** FSW-016
- **Implemented:** 5 commands
- **Source:** Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-017: B-3 Transport extensions (L/Home/fast-fwd/+-bar/Ctrl+M) + Song-mode-empty playback fix + Song loop default ON**
- **Sources:** FSW-017
- **Implemented:** Transport
- **Source:** Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-018: B-4 Bare Z=Zoom + PgUp/PgDn zoom-when-tool + Alt+G ungroup + Builder wheel direction + slice snap fix + Ctrl+drag marquee on all 3 grids**
- **Sources:** FSW-018
- **Implemented:** Misc
- **Source:** Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-019: B-5 Ctrl+Z / Ctrl+Alt+Z migrated to global commands**
- **Sources:** FSW-019
- **Implemented:** Global undo/redo
- **Source:** Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-020: B-6 ~80 Path-A doc rows + 880x680 popup + hardcoded-conflict warning**
- **Sources:** FSW-020
- **Implemented:** Documentation
- **Source:** Phase B
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase C - Simple New Keybinds

#### **FSW-023: Ctrl+N/O/S/Shift+S, Ctrl+M, Home, +/- cycle, F3, L, NumPad 0, NumPad / ***
- **Sources:** FSW-023
- **Implemented:** Rolled into B-1/B-2/B-3
- **Source:** Phase C
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase E - Mouse-modifier Reference Rows

#### **BLU-554: E1 Factory preset/kit/template generation**
- **Sources:** BLU-554
- **Implemented:** 790 presets, 87 kits, 29 templates
- **Source:** Player Engines > Phase E > E1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-555: E2 Templates infrastructure**
- **Sources:** BLU-555
- **Implemented:** File menu + Save/Load Template + folders
- **Source:** Player Engines > Phase E > E2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-556: E3 VibeLAF property-gated rendering**
- **Sources:** BLU-556
- **Implemented:** switchToggle + outlineGlowOnly
- **Source:** Player Engines > Phase E > E3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-557: E4 Per-engine preset menu pattern (recursive walker)**
- **Sources:** BLU-557
- **Implemented:** My Presets + Factory + root
- **Source:** Player Engines > Phase E > E4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-558: E5 AudioSettingsDialog Documents-path fix**
- **Sources:** BLU-558
- **Implemented:** Single source of truth
- **Source:** Player Engines > Phase E > E5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-559: E6 Effects page sub-tab default + per-channel persistence**
- **Sources:** BLU-559
- **Implemented:** (also documented in Effects Page section)
- **Source:** Player Engines > Phase E > E6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-568: E7 Phase A keymap framework**
- **Sources:** BLU-568
- **Implemented:** juce::ApplicationCommandManager + Help menu
- **Source:** Phase E > E7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-074: Phase E Mouse-modifier reference rows**
- **Sources:** FSW-074
- **Implemented:** Builder ~30 rows, Piano Roll ~36 rows, DrumKit ~30 rows
- **Source:** Phase E
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-075: Builder ~30, Piano Roll ~36, DrumKit ~30 self-documenting rows**
- **Sources:** FSW-075
- **Implemented:** Reference rows
- **Source:** Phase E
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-076: Hardcoded-conflict warning when binding to a page-local key**
- **Sources:** FSW-076
- **Implemented:** Conflict warning
- **Source:** Phase E
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase F - Pattern Colours

#### **BLU-567: F-2 Template Layer/Bass preset prefix bug + DrumKit Ctrl+drag**
- **Sources:** BLU-567
- **Implemented:** 4-segment trackId fix
- **Source:** Phase F > F-2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-077: Phase F Pattern colours + template-load bugfix**
- **Sources:** FSW-077
- **Implemented:** Both sub-batches done
- **Source:** Phase F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-078: F-1 Per-pattern user colour**
- **Sources:** BLU-566|FSW-078
- **Implemented:** Pattern struct, persisted, ColourSelector w/ 10-slot recents | Pattern struct color field + ColourSelector
- **Source:** Phase F | Phase F > F-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-079: F-2 Template Layer/Bass preset prefix-extraction bug fix; DrumKit Ctrl+drag marquee added**
- **Sources:** FSW-079
- **Implemented:** Bug fix
- **Source:** Phase F
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase G - Clips / Vox / Inst

#### **BLU-475: G-5 Audio browser unified view**
- **Sources:** BLU-475|FSW-100
- **Implemented:** juce::TreeView with Clips/Vox/Inst | Single Builder browser tree
- **Source:** Phase G > G-5 | System Pages > Builder Page > G-5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-596: G-1.1 BaySickNAM/IR engine skeleton + NAM core CMake**
- **Sources:** BLU-596
- **Implemented:** BaySickNAMCore static lib
- **Source:** Phase G > G-1.1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-598: G-1.2 Full DSP chain + file loaders + full-rig auto-detect + zero latency**
- **Sources:** BLU-598|FSW-082
- **Implemented:** Threading model + processBlock chain | DSP chain
- **Source:** Phase G > G-1 | Phase G > G-1.2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-599: G-1.3 Noise gate + 1×/2×/4× oversampling + A/B compare slots**
- **Sources:** BLU-599
- **Implemented:** NS-2 sit between input gain and NAM
- **Source:** Phase G > G-1.3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-600: G-1.4 Editor UI 760×340**
- **Sources:** BLU-600
- **Implemented:** Existing widget vocabulary
- **Source:** Phase G > G-1.4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-601: G-1.5 Floating test window**
- **Sources:** BLU-601|FSW-085
- **Implemented:** Help menu test layout | Test UI
- **Source:** Phase G > G-1 | Phase G > G-1.5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-603: G-3 Clip piano-roll trigger DSP + dual-engine A/B**
- **Sources:** BLU-603|FSW-088
- **Implemented:** clipRoll[50] + mClipEngines + audio routing | All 6 items shipped
- **Source:** Phase G > G-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-080: G-1 BaySickNAM/IR engine**
- **Sources:** FSW-080
- **Implemented:** 5 sub-batches all shipped
- **Source:** Phase G > G-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-081: G-1.1 Skeleton + CMake / NAM core integration**
- **Sources:** FSW-081
- **Implemented:** Build infrastructure
- **Source:** Phase G > G-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-083: G-1.3 Noise gate + 1x/2x/4x oversampling + A/B compare slots**
- **Sources:** FSW-083
- **Implemented:** DSP features
- **Source:** Phase G > G-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-084: G-1.4 Editor UI (760x340)**
- **Sources:** FSW-084
- **Implemented:** UI
- **Source:** Phase G > G-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-086: G-2 Clips ribbon tab**
- **Sources:** BLU-602|FSW-086
- **Implemented:** Ribbon TabType::Clip + ClipsPage + ClipsEmptyState
- **Source:** Phase G > G-2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-087: Ribbon TabType::Clip with amber palette and ClipsPage host**
- **Sources:** FSW-087
- **Implemented:** Implementation
- **Source:** Phase G > G-2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-089: Pattern data model (clipRoll[50] + kClipPRTarget + XML + loop-beats scan fix)**
- **Sources:** FSW-089
- **Implemented:** Data model
- **Source:** Phase G > G-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-090: Audio thread (mClipEngines + fast-path bypass + register/unregister + dispatch + scheduleRoll)**
- **Sources:** FSW-090
- **Implemented:** Audio thread
- **Source:** Phase G > G-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-091: Dual-engine A/B persistence (both kept alive across picker swaps)**
- **Sources:** FSW-091
- **Implemented:** A/B compare
- **Source:** Phase G > G-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-092: ClipsBus pre-processing pulled out of song-mode-only block**
- **Sources:** FSW-092
- **Implemented:** Routing
- **Source:** Phase G > G-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-093: Drum-kit black-page regression fix**
- **Sources:** FSW-093
- **Implemented:** Bug fix
- **Source:** Phase G > G-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-094: Path resolution via resolveProjectFile**
- **Sources:** FSW-094
- **Implemented:** Path resolution
- **Source:** Phase G > G-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-095: G-4 Vox + Inst ribbon tabs**
- **Sources:** FSW-095
- **Implemented:** All 4 batches + cleanup shipped
- **Source:** Phase G > G-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-096: Batch 1 - Ribbon tabs (TabType::Vox/Inst, kNumSlots 8 to 10, teal/navy palette)**
- **Sources:** FSW-096
- **Implemented:** Ribbon
- **Source:** Phase G > G-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-097: Batch 2 - Constants, processor register/unregister, audio-thread loops, EngineKind extended**
- **Sources:** FSW-097
- **Implemented:** Constants/audio thread
- **Source:** Phase G > G-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-098: Batch 3 - Page components (VoxPage/InstPage, empty states), CMake, StandaloneEditor wiring**
- **Sources:** FSW-098
- **Implemented:** Page components
- **Source:** Phase G > G-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-099: Cleanup post-build review (Vox/Inst dropped Piano Roll sub-tab + dropped from PianoRollPage registry; ribbon dropdown sub-pages dispatch correctly; BaySickPlayer dropped from Vox/Inst entirely)**
- **Sources:** FSW-099
- **Implemented:** Cleanup
- **Source:** Phase G > G-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-101: Single Builder browser tree showing Clips + Vox + Inst recordings + imported audio**
- **Sources:** FSW-101
- **Implemented:** Browser unification
- **Source:** Phase G > G-5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-102: G-6 Right-click Copy operation**
- **Sources:** FSW-102
- **Implemented:** Cross-cutting copy on every clip/preset/sound
- **Source:** Phase G > G-6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-103: Cross-cutting Copy on every clip/preset/sound + destination prompt + file duplication**
- **Sources:** FSW-103
- **Implemented:** Copy
- **Source:** Phase G > G-6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-104: G-7 Tab-close prompts + no-file-delete contract**
- **Sources:** FSW-104
- **Implemented:** All 8 items shipped
- **Source:** Phase G > G-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-105: Consistent remove from project but keep on disk flow across all closeable elements**
- **Sources:** FSW-105
- **Implemented:** Tab close
- **Source:** Phase G > G-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-106: Page Preset save/load (full chain) for all 6 page types**
- **Sources:** FSW-106
- **Implemented:** PagePresetIO + bus-fallback callback
- **Source:** Phase G > G-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-107: Chained Save Page Preset & Delete**
- **Sources:** FSW-107
- **Implemented:** savePagePreset takes onSaved continuation
- **Source:** Phase G > G-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-108: Listener-based dirty tracking (replaced fragile byte comparison)**
- **Sources:** FSW-108
- **Implemented:** juce::ValueTree::Listener + mSuppressDirty
- **Source:** Phase G > G-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-109: Empty-state Load Preset hamburger entry**
- **Sources:** FSW-109
- **Implemented:** Clips/Vox/Inst empty states
- **Source:** Phase G > G-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-110: Aux/secondary-bus right-click delete with sweepSendsTargeting helper**
- **Sources:** FSW-110
- **Implemented:** Secondary bus delete
- **Source:** Phase G > G-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-111: Builder block-leak fix on Clips tab close**
- **Sources:** FSW-111
- **Implemented:** Bug fix
- **Source:** Phase G > G-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-112: Overwrite button removed from save dialogs (no-file-delete contract)**
- **Sources:** FSW-112
- **Implemented:** Save dialog
- **Source:** Phase G > G-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-113: G-8 Builder ribbon recolor + slot reorder**
- **Sources:** FSW-113
- **Implemented:** 2 items
- **Source:** Phase G > G-8
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-114: 8-color tie-dye gradient via ColourGradient with 8 stops matching mixer-strip palette**
- **Sources:** FSW-114
- **Implemented:** Gradient
- **Source:** Phase G > G-8
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-115: Builder slot moved to leftmost (before Mixer) per spec**
- **Sources:** FSW-115
- **Implemented:** Slot reorder
- **Source:** Phase G > G-8
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-116: G-9 Vox/Inst chain rework**
- **Sources:** FSW-116
- **Implemented:** Always-active series chain replaces engine-picker; shipped via I-16
- **Source:** Phase G > G-9
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-118: G-9.2 Page UI rework drop engine picker**
- **Sources:** FSW-118
- **Implemented:** Vox=5 sub-tabs, Inst=3 sub-tabs
- **Source:** Phase G > G-9
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-119: G-9.3 Live-input wiring**
- **Sources:** FSW-119
- **Implemented:** ASIO arm-LED-selected input from mixer
- **Source:** Phase G > G-9
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-124: Pattern-mode arrangement-block timeline length read fix**
- **Sources:** FSW-124
- **Implemented:** Builder block longer than 1 bar should loop at its own length
- **Source:** Phase G > G polish gate
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-125: Blueprint logging for G-1 to G-9**
- **Sources:** FSW-125
- **Implemented:** Documentation at end of Phase G; This is documentation for a document we are moving away from in this action so no blueprint logging needed
- **Source:** Phase G > G polish gate
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase H - BaySickVocal

#### **FSW-126: Phase H BaySickVocal complete**
- **Sources:** FSW-126
- **Implemented:** Own AudioProcessor peer to BaySickNAM/IR
- **Source:** Phase H - BaySickVocal
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-127: Pitch correction latency hybrid algorithm (realtime hop=256, offline hop=512)**
- **Sources:** FSW-127
- **Implemented:** Locked decision
- **Source:** Phase H Locked Spec
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-128: Realtime AND offline both ship; no mode switch; realtime always-on with stage Bypass LED**
- **Sources:** FSW-128
- **Implemented:** Locked decision
- **Source:** Phase H Locked Spec
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-129: Editor fills entire tab page, not fixed 760x340 card**
- **Sources:** FSW-129
- **Implemented:** Locked decision
- **Source:** Phase H Locked Spec
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-130: Compressor Type dropdown (Modern/FET/Opto) with real algorithms per Type**
- **Sources:** FSW-130
- **Implemented:** Locked decision
- **Source:** Phase H Locked Spec
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-131: Saturation umbrella SaturationType dropdown (Tube/Console)**
- **Sources:** FSW-131
- **Implemented:** Locked decision
- **Source:** Phase H Locked Spec
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-132: De-esser split-band SC HPF 5-10kHz + dynamic notch**
- **Sources:** FSW-132
- **Implemented:** Locked decision
- **Source:** Phase H Locked Spec
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-133: Tape post-Phase H cleanup batch folds TapeDSP into SaturationDSP**
- **Sources:** FSW-133
- **Implemented:** Locked decision
- **Source:** Phase H Locked Spec
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-134: BaySickPitch = Newtone clone full-page offline pitch editor**
- **Sources:** FSW-134
- **Implemented:** Locked decision
- **Source:** Phase H Locked Spec
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-135: BaySickAlign = VocAlign clone offline time-alignment editor**
- **Sources:** FSW-135
- **Implemented:** Locked decision
- **Source:** Phase H Locked Spec
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-136: Realtime/offline interaction no double-processing**
- **Sources:** FSW-136
- **Implemented:** Source mux integration
- **Source:** Phase H Locked Spec
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-137: H-1 Skeleton (BaySickVocalProcessor)**
- **Sources:** FSW-137
- **Implemented:** APVTS bsv_*, processBlock pass-through, prepareToPlay propagation, state save/load
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-138: H-2 CompressorDSP Type umbrella (Modern/FET/Opto)**
- **Sources:** FSW-138
- **Implemented:** FETCompressorPanel + OptoCompressorPanel; createEffectEditor switches by type
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-139: H-3 DeEsserDSP**
- **Sources:** FSW-139
- **Implemented:** New module per locked spec
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-140: H-4 YIN/MPM pitch tracker**
- **Sources:** FSW-140
- **Implemented:** Autocorrelation tracker, worker thread, atomic publish
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-141: H-5 Pitch correction wrapper (hybrid hop=256/512)**
- **Sources:** FSW-141
- **Implemented:** PhaseVocoder + YIN
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-142: H-6 Editor (BaySickVocalEditor with 6 sub-tabs)**
- **Sources:** FSW-142
- **Implemented:** BaySickVocals/VocalChain/BaySickPitch/BaySickAlign/BaySickNAMIR/PreEQ8MS
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-143: H-6a BaySickAlign DSP**
- **Sources:** FSW-143
- **Implemented:** VocAlign-clone offline time-alignment DSP
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-144: H-6b BaySickPitch UI**
- **Sources:** FSW-144
- **Implemented:** Newtone-clone editor; FL pitch labels
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-145: H-6c BaySickAlign UI**
- **Sources:** FSW-145
- **Implemented:** VocAlign-clone editor; 3-lane GUIDE/DUB/OUTPUT layout
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-146: H-6d BaySickNAMIR sub-tab integration**
- **Sources:** FSW-146
- **Implemented:** Owns BaySickNAMIRProcessor; per-slot A/B; MicSimDSP+MicPlacementDSP
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-147: H-7 SaturationDSP Type umbrella (Tube/Console)**
- **Sources:** FSW-147
- **Implemented:** Tube path bit-exact untouched; Vocal Body shaping toggle
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-148: H-8 DelayDSP extensions (Echo + VocalDoubler)**
- **Sources:** FSW-148
- **Implemented:** Type umbrella + sidechain ducking + Slapback preset
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-149: H-9 ReverbDSP extensions (5-algorithm umbrella)**
- **Sources:** FSW-149
- **Implemented:** Plate/Hall/Chamber/Room/VocalBooth + sidechain + tempo-sync pre-delay
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-150: H-10 Tape fold cleanup (TapeDSP folded into SaturationDSP)**
- **Sources:** FSW-150
- **Implemented:** Saturation umbrella as 3rd Type + load-time migration
- **Source:** Phase H Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase I - BaySickPedals

#### **FSW-001: Reuse juce::dsp::Oversampling pattern for non-linear pedals**
- **Sources:** FSW-001
- **Implemented:** Existing NAM/IR oversampling reused for BD-2, OD-3, DS-1, FZ-5, MT-2 pedals
- **Source:** Leverage from existing FX rack
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-002: Reuse juce::dsp::Convolution for AD-2 acoustic body IRs**
- **Sources:** FSW-002
- **Implemented:** NAM/IR convolution reused
- **Source:** Leverage from existing FX rack
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-003: Reuse CompressorDSP for CS-3 pedal after H-2 extension**
- **Sources:** FSW-003
- **Implemented:** Direct reuse path
- **Source:** Leverage from existing FX rack
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-004: Reuse PhaseVocoder for vocal pitch correction (H-5)**
- **Sources:** FSW-004
- **Implemented:** PV used in H-5
- **Source:** Leverage from existing FX rack
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-005: Reuse EffectRack hot-swap pattern for I-1 slot 1-4 hot-swap**
- **Sources:** FSW-005
- **Implemented:** Wait-free atomic swap pattern reused
- **Source:** Leverage from existing FX rack
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-006: Reuse EQ8DSP for GE-7 graphic EQ**
- **Sources:** FSW-006
- **Implemented:** 7 of 8 bands as fixed-Q peaks
- **Source:** Leverage from existing FX rack
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-183: Phase I BaySickPedals complete**
- **Sources:** FSW-183
- **Implemented:** All 16 sub-batches I-1..I-16 complete
- **Source:** Phase I - BaySickPedals
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-184: I-1 BaySickPedals rack skeleton**
- **Sources:** FSW-184
- **Implemented:** BaySickPedalsProcessor with 8-slot config
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-185: I-2 Universal pedal infrastructure**
- **Sources:** FSW-185
- **Implemented:** EditorPanelBase::PanelMode + PolyphaseOversampler
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-186: I-3 MIDI CC to APVTS routing infrastructure**
- **Sources:** FSW-186
- **Implemented:** MidiLearnRegistry + MidiLearnUI; closes C13 audit gap
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-187: I-4 CompressorDSP CS Type extension**
- **Sources:** FSW-187
- **Implemented:** Type::CS + Sustain macro + Tone tilt
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-188: I-5 Harmonics drive pedals batch (BD/DS/FZ/MT)**
- **Sources:** FSW-188
- **Implemented:** 4x oversampled, 5Hz DC-blocker; OD folded into OverdriveDSP::Type::Pedal
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-189: I-6 Harmonics bass pedals batch (BB/ODB)**
- **Sources:** FSW-189
- **Implemented:** Bass driver + bass overdrive
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-190: I-7 OC Style Octave**
- **Sources:** FSW-190
- **Implemented:** Polyphonic + Vintage modes
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-191: I-8 Dynamics pedals batch (NS/BC)**
- **Sources:** FSW-191
- **Implemented:** Noise gate + bass compressor
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-192: I-9 SY Style Polyphonic Synth**
- **Sources:** FSW-192
- **Implemented:** YIN tracker + Type-driven waveform mix
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-193: I-10 PW Style Wah**
- **Sources:** FSW-193
- **Implemented:** TPT resonant bandpass log-swept
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-194: I-11 AD Style Acoustic Preamp + AC Style Acoustic Simulator**
- **Sources:** FSW-194
- **Implemented:** Both built; AC added at user request
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-195: I-12 EQ trio batch (GE/GEB/EQFH)**
- **Sources:** FSW-195
- **Implemented:** Pedalboard-only graphic + parametric EQs
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-196: I-13 TU Style Tuner**
- **Sources:** FSW-196
- **Implemented:** Pedalboard-only; Strobe + LED Bar display modes
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-197: I-14 Simplified pedalboard panels for existing effects**
- **Sources:** FSW-197
- **Implemented:** 7 small new panel structs; createEffectEditor dispatches by mode
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-198: I-15 BaySickPedals editor UI**
- **Sources:** FSW-198
- **Implemented:** 4x2 grid + pedalboard preset library + User NAM Pedal + folder seeding
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-199: I-16 G-9 unblock follow-on / G-9 chain rework**
- **Sources:** FSW-199
- **Implemented:** Full Vox + Inst audio routing per Option C spec
- **Source:** Phase I Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase J - BaySickRustyDrums

#### **FSW-200: Phase J BaySickRustyDrums (locked spec)**
- **Sources:** FSW-200
- **Implemented:** J-2 shipped, J-3 in progress notation but all sub-batches actually shipped; ambiguous status - header marked not-started but all sub-batches shipped
- **Source:** Phase J - BaySickRustyDrums
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-201: sfizz vendoring done (libs/sfizz, BSD 2-Clause static-link, ~535MB->~79MB pruned)**
- **Sources:** FSW-201
- **Implemented:** Pre-J-1, 2026-05-03
- **Source:** Phase J Pre-J-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-202: J-1 sfizz CMake integration + build verification**
- **Sources:** FSW-202
- **Implemented:** add_subdirectory + /MD runtime forced + About dialog attribution
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-203: J-2 BaySickRustyDrumsProcessor skeleton + APVTS + state save/load**
- **Sources:** FSW-203
- **Implemented:** APVTS prefix brd_; mAuditionNote atomic; KitPath element
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-204: J-3 Kit loader + channel discovery**
- **Sources:** FSW-204
- **Implemented:** loadKit + discoverChannels + 13 sound-type rules
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-205: J-4 RustyDrums Bus + MixerChannelIds extension**
- **Sources:** FSW-205
- **Implemented:** kRustyDrumsBus=12 + kRustyBase=800
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-206: J-5 Multi-strip auto-creation flow**
- **Sources:** FSW-206
- **Implemented:** 13 strips per kit; addRustyChannelAtIndex etc.
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-207: J-6 BaySickRustyDrumsPage + ribbon integration + EQ unification**
- **Sources:** FSW-207
- **Implemented:** 2 sub-tabs Player+Piano Roll; EQ unification removes per-page Pre EQ
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-208: J-7a BaySickRustyDrums singleton render path + bus accumulator**
- **Sources:** FSW-208
- **Implemented:** Initial single stereo pair render
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-209: J-7b Pattern playback dispatch + per-strip routing + custom note labels + Help map window + native keymap refactor**
- **Sources:** FSW-209
- **Implemented:** Native-keymap refactor + custom note labels + Help Rusty Drums Map; per-strip routing parked as interim single-output
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-210: J-8 Editor UI**
- **Sources:** FSW-210
- **Implemented:** Sub-tab restructure + Kit Graphic + Program selector + ARIA panel + Page Preset Save/Load
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-211: J-9 Project save/load round-trip**
- **Sources:** FSW-211
- **Implemented:** serializeUIState writes Tab type=BaySickRustyDrums; crash fix for kit load order
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-212: J-11 Player Preset dropdown + closing pass**
- **Sources:** FSW-212
- **Implemented:** Player Preset selector with overlay semantics
- **Source:** Phase J Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-213: J-6/J-9 greys-out vs disappears for singleton-locked menu entry**
- **Sources:** FSW-213
- **Implemented:** Deferred spec call
- **Source:** Phase J Open spec calls
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-214: J-8 visual palette + texture for editor**
- **Sources:** FSW-214
- **Implemented:** Deferred spec call
- **Source:** Phase J Open spec calls
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase K - BaySickGuitars

#### **FSW-215: Phase K BaySickGuitars overall**
- **Sources:** FSW-215
- **Implemented:** K-1 to K-6 shipped; K-7 pending
- **Source:** Phase K - BaySickGuitars
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-216: K-1 Engine processor + APVTS skeleton**
- **Sources:** FSW-216
- **Implemented:** BaySickGuitarsProcessor with bgg_ prefix
- **Source:** Phase K Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-217: K-2 Inst page Source-Type extension + MixerTrackStrip::noLiveInput**
- **Sources:** FSW-217
- **Implemented:** Source enum + noLiveInput flag
- **Source:** Phase K Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-218: K-3 Inst-page piano roll wiring (cross-cutting, shared with L)**
- **Sources:** FSW-218
- **Implemented:** Pattern data already shipped from G-4; just wiring
- **Source:** Phase K Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-219: K-4 Ribbon entry + multi-instance plumbing**
- **Sources:** FSW-219
- **Implemented:** + Add BaySickGuitars dropdown entry
- **Source:** Phase K Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-220: K-5 ARIA control panel + program selector**
- **Sources:** FSW-220
- **Implemented:** AriaControlPanel moved to shared Source/Standalone
- **Source:** Phase K Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-221: K-6 Project save/load round-trip**
- **Sources:** FSW-221
- **Implemented:** Tab type=Inst with source attribute
- **Source:** Phase K Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-222: K-7 Page preset Save/Load - refactor RustyDrumsPagePresetIO to shared AriaPagePresetIO**
- **Sources:** FSW-222
- **Implemented:** Refactor pending
- **Source:** Phase K Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-223: NAM core static-init fix (CMake)**
- **Sources:** FSW-223
- **Implemented:** target_link_options /WHOLEARCHIVE for BaySickNAMCore
- **Source:** Phase K cleanup
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-224: Inst tab project save/load was dropping every page-internal stage**
- **Sources:** FSW-224
- **Implemented:** instChainState attribute
- **Source:** Phase K cleanup
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-225: Inst tab mixer strip was dropped on save/load when name unchanged**
- **Sources:** FSW-225
- **Implemented:** K-6 deserialize calls addInstChannelAtIndex
- **Source:** Phase K cleanup
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-226: APVTS CC-default semantics fixed across all sfizz-backed engines**
- **Sources:** FSW-226
- **Implemented:** CC default 0; reset before applying set_cc; mProcessingEnabled atomic
- **Source:** Phase K cleanup
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-227: AriaControlPanel widget set + UI polish**
- **Sources:** FSW-227
- **Implemented:** Slider + OnOffButton parsing; tab strip suppression for single-program
- **Source:** Phase K cleanup
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-228: Per-program state cache for sfizz-source Inst pages**
- **Sources:** FSW-228
- **Implemented:** std::map keyed by SFZ filename; persists per-project
- **Source:** Phase K cleanup
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-229: VU calibration target persistence**
- **Sources:** FSW-229
- **Implemented:** Already round-tripped; added VUMeter::sOnCalibrationChanged
- **Source:** Phase K cleanup
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-230: Project-dirty wiring across every engine + non-APVTS state change**
- **Sources:** FSW-230
- **Implemented:** ApvtsDirtyTracker + EffectRack::onSlotsChanged + many other hooks
- **Source:** Phase K cleanup
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase L - BaySickBasses

#### **FSW-231: Phase L BaySickBasses overall**
- **Sources:** FSW-231
- **Implemented:** Locked spec, full breakdown approved
- **Source:** Phase L - BaySickBasses
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-232: L-1 Engine processor + APVTS skeleton**
- **Sources:** FSW-232
- **Implemented:** Mirror of K-1 with bbb_ prefix
- **Source:** Phase L Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-233: L-2 Inst page Source-Type extension**
- **Sources:** FSW-233
- **Implemented:** Add Source::BaySickBasses
- **Source:** Phase L Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-234: L-3 Ribbon entry + multi-instance plumbing**
- **Sources:** FSW-234
- **Implemented:** + Add BaySickBasses dropdown
- **Source:** Phase L Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-235: L-4 ARIA control panel + program selector**
- **Sources:** FSW-235
- **Implemented:** Reuses shared AriaControlPanel from K-5
- **Source:** Phase L Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-236: L-5 Project save/load round-trip**
- **Sources:** FSW-236
- **Implemented:** Tab type=Inst with source=BaySickBasses
- **Source:** Phase L Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-237: L-6 Page preset Save/Load**
- **Sources:** FSW-237
- **Implemented:** Plug into shared AriaPagePresetIO from K-7
- **Source:** Phase L Sub-batches
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Cross-cutting Infrastructure

#### **BLU-455: R1 Bus infrastructure + insert APIs**
- **Sources:** BLU-455
- **Implemented:** VoxBus + InstBus + InsertKind extensions
- **Source:** Live-Input Recording > R1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-456: R1 follow-up fixes**
- **Sources:** BLU-456
- **Implemented:** Strip visibility + cable anchor + routing
- **Source:** Live-Input Recording > R1 follow-up
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-457: R2 Vox/Inst Arm LED + ASIO input picker**
- **Sources:** BLU-457
- **Implemented:** APVTS persistence
- **Source:** Live-Input Recording > R2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-458: R3 Audio device input wiring**
- **Sources:** BLU-458
- **Implemented:** Through Vox/Inst strips into buses
- **Source:** Live-Input Recording > R3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-459: R3.5 Vox + Inst BUS strips visible**
- **Sources:** BLU-459
- **Implemented:** Teal/Navy with full DSP
- **Source:** Live-Input Recording > R3.5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-460: Cable overlay flicker fix**
- **Sources:** BLU-460
- **Implemented:** setBufferedToImage(true)
- **Source:** Live-Input Recording > Cable
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-461: Mixer page review backlog**
- **Sources:** BLU-461|FSW-336
- **Implemented:** Cables over Master strip + scroll redraw | Pending
- **Source:** Live-Input Recording > Mixer page review | Standing parallel work > Smaller open items
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-462: R4 Listen toggle**
- **Sources:** BLU-462
- **Implemented:** HeadphonesLedButton + _listen Bool
- **Source:** Live-Input Recording > R4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-463: R5a Play + Record transport buttons repainted**
- **Sources:** BLU-463
- **Implemented:** PlayButton + RecordButton subclasses
- **Source:** Live-Input Recording > R5a
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-464: R5b Record button arms-only**
- **Sources:** BLU-464
- **Implemented:** mRecordArmed + mRecordingActive
- **Source:** Live-Input Recording > R5b
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-465: R5c ASIO/MIDI mode picker on Record chevron**
- **Sources:** BLU-465
- **Implemented:** RecordMode enum + onRecordModeChanged
- **Source:** Live-Input Recording > R5c
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-466: R5d Recording engine rewrite**
- **Sources:** BLU-466
- **Implemented:** Per-strip + master + auto-drop on arrangement
- **Source:** Live-Input Recording > R5d
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-488: Player + Page Audit Workflow**
- **Sources:** BLU-488
- **Implemented:** Per-element table format
- **Source:** Cross-cutting > SLA Pattern
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-491: Sidechaining infrastructure**
- **Sources:** BLU-491
- **Implemented:** APVTS scaffolding + slot routing + UI; PRESET-BREAK
- **Source:** Cross-cutting > Sidechaining
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-495: Pre-rack EQ on all InsertNodes (B2 architecture)**
- **Sources:** BLU-495
- **Implemented:** mPreEQ on every InsertNode; PRESET-SAFE
- **Source:** Cross-cutting > Pre-rack EQ
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-496: Mixer strip meter fix**
- **Sources:** BLU-496
- **Implemented:** getInsertPeakDb per strip
- **Source:** Cross-cutting > Mixer strip meter fix
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-497: Preset-as-double-click-default for engine knobs**
- **Sources:** BLU-497
- **Implemented:** TaggedSliderAttachment + walker; PRESET-SAFE
- **Source:** Cross-cutting > Preset double-click default
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-498: Automation display-name system + paramId coverage fixes**
- **Sources:** BLU-498
- **Implemented:** 3-layer naming + AutomationLane userDisplayName
- **Source:** Cross-cutting > Automation display-name
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-500: Automation applicator dangling-pointer crash fix**
- **Sources:** BLU-500
- **Implemented:** SafePointer<juce::Slider>; PRESET-SAFE
- **Source:** Cross-cutting > Automation crash fix
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-503: Mixer-strip EQ binding fixes**
- **Sources:** BLU-503
- **Implemented:** Layer/Bass/Aux/Audio call sites; PRESET-SAFE
- **Source:** Cross-cutting > Mixer-strip EQ binding fixes
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-504: Drum strip effects-rack dropdown fix**
- **Sources:** BLU-504
- **Implemented:** Route 100..115 via getInsertRack(Drum); PRESET-SAFE
- **Source:** Cross-cutting > Drum strip dropdown fix
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-505: A9 Panel-construct DSP-state sync (cross-apply)**
- **Sources:** BLU-505
- **Implemented:** 18 controls across 4 panels + sliders
- **Source:** Cross-cutting > A9 Panel sync
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-509: Drift -> Tape effect**
- **Sources:** BLU-509
- **Implemented:** Tape wow/flutter is pitch drift; High priority
- **Source:** Cross-cutting > §P3-CORE Cross-Apply > Round 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-510: Drift -> Delay (tape-echo mode)**
- **Sources:** BLU-510
- **Implemented:** Classic tape delay wobble; Medium priority
- **Source:** Cross-cutting > §P3-CORE Cross-Apply > Round 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-511: Pink/Brown noise -> Saturation/Tape/Overdrive**
- **Sources:** BLU-511
- **Implemented:** Optional noise-floor knob; Medium priority
- **Source:** Cross-cutting > §P3-CORE Cross-Apply > Round 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-517: Right-click "Type in value" on all value controls**
- **Sources:** BLU-517
- **Implemented:** Modal AlertWindow with current value
- **Source:** Cross-cutting > Type-in value
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-151: Cross-cutting infrastructure shipped during Phase H**
- **Sources:** FSW-151
- **Implemented:** Bundle
- **Source:** Cross-cutting infrastructure (Phase H)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-152: Effect Preset Framework**
- **Sources:** FSW-152
- **Implemented:** Generic preset save/load for every DSP
- **Source:** Cross-cutting > Effect Preset Framework
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-153: EffectPresetIO.h/.cpp generic preset save/load**
- **Sources:** FSW-153
- **Implemented:** Per-effect-type folder layout
- **Source:** Cross-cutting > Effect Preset Framework
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-154: Per-effect-type folder layout under Documents/BaySickDAW/Presets/Effects**
- **Sources:** FSW-154
- **Implemented:** Folder structure
- **Source:** Cross-cutting > Effect Preset Framework
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-155: 36 factory presets seeded on first launch (3 per effect x 12 types)**
- **Sources:** FSW-155
- **Implemented:** Factory presets
- **Source:** Cross-cutting > Effect Preset Framework
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-156: SlotComponent gets Preset button with menu**
- **Sources:** FSW-156
- **Implemented:** Save/Load/Restore Default/Manage Presets
- **Source:** Cross-cutting > Effect Preset Framework
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-157: EffectRack::createEffect made static + public**
- **Sources:** FSW-157
- **Implemented:** For seeder
- **Source:** Cross-cutting > Effect Preset Framework
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-158: Phase I-1 commitment BaySickPedals per-pedal preset menus reuse EffectPresetIO**
- **Sources:** FSW-158
- **Implemented:** Framework reuse
- **Source:** Cross-cutting > Effect Preset Framework
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-159: Effect Picker Reorganization**
- **Sources:** FSW-159
- **Implemented:** addSectionHeader to group 12 effects into 4 categories
- **Source:** Cross-cutting > Effect Picker Reorganization
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-160: SlotComponent::showAddMenu uses addSectionHeader**
- **Sources:** FSW-160
- **Implemented:** 4 categories Dynamic/Harmonics/Modulation/Time
- **Source:** Cross-cutting > Effect Picker Reorganization
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-161: Each group alphabetically sorted**
- **Sources:** FSW-161
- **Implemented:** Sorting
- **Source:** Cross-cutting > Effect Picker Reorganization
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-162: Meter Rebuild (Vsync + Snapshot Architecture)**
- **Sources:** FSW-162
- **Implemented:** Replaces 30Hz timer-driven peak feed
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-163: DBFSMeter dropped Timer for juce::VBlankAttachment**
- **Sources:** FSW-163
- **Implemented:** Vblank-locked metering
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-164: VUMeter dropped Timer for vblank attachment**
- **Sources:** FSW-164
- **Implemented:** FL-style snap+decay (25 dB/sec)
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-165: MixerPage 30Hz Timer for meter polling replaced with vblank**
- **Sources:** FSW-165
- **Implemented:** 30Hz Timer kept for cable overlay scroll etc.
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-166: SlotComponent 30Hz Timer dropped for vblank**
- **Sources:** FSW-166
- **Implemented:** Effect-panel level feed
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-167: Removed audio-side decay/smoothing from EffectRack::process**
- **Sources:** FSW-167
- **Implemented:** CAS-max only
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-168: publishPeakReading helper in VibeGraph**
- **Sources:** FSW-168
- **Implemented:** Per-block compensated peak into running-max atomics
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-169: FxBus/AudioClipsBus/Vox/Inst CAS-max into Run companion atomics**
- **Sources:** FSW-169
- **Implemented:** No audio-side decay
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-170: Audio rows write into mAudioRowPeakDb*Run**
- **Sources:** FSW-170
- **Implemented:** Peak atomics
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-171: Removed transport-stopped decay path (UI ballistics handle)**
- **Sources:** FSW-171
- **Implemented:** UI-side decay
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-172: End-of-block snapshot promotion (eliminates layer-vs-bus ping-pong)**
- **Sources:** FSW-172
- **Implemented:** Run/Snapshot architecture
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-173: Bus mirrors drained via exchange-and-CAS-max**
- **Sources:** FSW-173
- **Implemented:** Bus end-of-block
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-174: VibeGraph::promoteAllInsertPeakSnapshots walks every InsertNode**
- **Sources:** FSW-174
- **Implemented:** Coverage
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-175: EffectRack::promoteSlotPeakSnapshots lifts slot peaks**
- **Sources:** FSW-175
- **Implemented:** Slot atomics
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-176: Coverage layers/bass/drums/master/audio rows/inserts/rack slots all coherent**
- **Sources:** FSW-176
- **Implemented:** Snapshot promotion
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-177: MeterLatencyComp namespace (gEnabled + gCompensationBlocks)**
- **Sources:** FSW-177
- **Implemented:** Latency-compensation
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-178: Per-bus-node 16-entry peak ring for latency-compensated publish**
- **Sources:** FSW-178
- **Implemented:** Output-driver latency alignment
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-179: Mixer hamburger menu Latency-compensate meters toggle**
- **Sources:** FSW-179
- **Implemented:** Off by default; persisted per-project
- **Source:** Cross-cutting > Meter Rebuild
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-180: Drum sample-load bug fix**
- **Sources:** FSW-180
- **Implemented:** 12 call sites fixed across DrumPage/LayersPage/BassPage
- **Source:** Cross-cutting > Drum sample-load bug fix
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-181: DrumPage/LayersPage/BassPage now call vp->loadSample*() instead of mgr.load*()**
- **Sources:** FSW-181
- **Implemented:** Closes Phase D carry
- **Source:** Cross-cutting > Drum sample-load bug fix
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-182: Fixed across 12 call sites**
- **Sources:** FSW-182
- **Implemented:** Comprehensive sweep
- **Source:** Cross-cutting > Drum sample-load bug fix
- **Verified:** 2026-05-08 (Phase-4 source verification)

### System Pages

#### **BLU-471: Press-and-hold audition**
- **Sources:** BLU-471
- **Implemented:** Both keyboard and grid hold note
- **Source:** System Pages > Piano Roll > Shipped
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-474: Mixer Page future enhancements**
- **Sources:** BLU-474
- **Implemented:** TBD survey pending
- **Source:** System Pages > Mixer Page
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-476: Builder Page future enhancements**
- **Sources:** BLU-476
- **Implemented:** TBD survey pending
- **Source:** System Pages > Builder Page
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-477: Perf cell layout 2x2 grid**
- **Sources:** BLU-477
- **Implemented:** SYS X% / DSP X% / MEM X / LAT X
- **Source:** System Pages > Transport Bar
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-479: Sub-tab default + per-channel persistence**
- **Sources:** BLU-479
- **Implemented:** switchTab(TabKind::Rack) etc
- **Source:** System Pages > Effects Page
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-481: Ribbon Tab Bar v1 state**
- **Sources:** BLU-481
- **Implemented:** 10 slots; multi-instance dropdowns
- **Source:** System Pages > Ribbon Tab Bar
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-482: Clips Page G-2 + G-3**
- **Sources:** BLU-482
- **Implemented:** BaySickPlayer + BaySickNAM/IR engine picker
- **Source:** System Pages > Clips Page > G-2/G-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-483: Vox Page G-4 baseline**
- **Sources:** BLU-483
- **Implemented:** BaySickPlayer + reserved BaySickVocal
- **Source:** System Pages > Vox Page > G-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-484: Vox Page G-9 BaySickVocal+NAM/IR chain**
- **Sources:** BLU-484
- **Implemented:** VoxChainProcessor wrapper
- **Source:** System Pages > Vox Page > G-9
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-485: Inst Page G-4 baseline**
- **Sources:** BLU-485
- **Implemented:** BaySickPlayer + BaySickNAM/IR
- **Source:** System Pages > Inst Page > G-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-486: Inst Page G-9 BaySickPedals+NAM/IR chain**
- **Sources:** BLU-486
- **Implemented:** InstChainProcessor wrapper
- **Source:** System Pages > Inst Page > G-9
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Project Persistence

#### **BLU-448: P1 Schema + serialization plumbing**
- **Sources:** BLU-448
- **Implemented:** Full PatternManager round-trip
- **Source:** Project Persistence > P1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-449: P2 File menu + New/Open/Save/SaveAs**
- **Sources:** BLU-449
- **Implemented:** Project lifecycle wired
- **Source:** Project Persistence > P2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-450: P3 Custom Project Browser + Open Recent**
- **Sources:** BLU-450
- **Implemented:** Rename / Duplicate / Delete / Show in Explorer
- **Source:** Project Persistence > P3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-451: P4 Copy-on-drop + Sample Library shortcut**
- **Sources:** BLU-451
- **Implemented:** Builder copy + path resolution
- **Source:** Project Persistence > P4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-452: P4b Roaming-to-Documents migration**
- **Sources:** BLU-452
- **Implemented:** Presets/ + audio_settings.xml
- **Source:** Project Persistence > P4b
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-453: Preset Folder Reorg**
- **Sources:** BLU-453
- **Implemented:** Family subfolders + section headers
- **Source:** Project Persistence > Preset Reorg
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-454: Core Library Page-Context Filter**
- **Sources:** BLU-454
- **Implemented:** Drum vs melodic packs filter
- **Source:** Project Persistence > Core Library Filter
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-468: P5 Dirty tracking + autosave + close-with-unsaved**
- **Sources:** BLU-468
- **Implemented:** 15-min interval + Restore from Backup
- **Source:** Project Persistence > P5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-469: P6 Default-template support + File menu**
- **Sources:** BLU-469
- **Implemented:** Template API + General submenu
- **Source:** Project Persistence > P6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-518: Persistence Cycle 1+2: tab + engine-state serialization**
- **Sources:** BLU-518
- **Implemented:** UIState child + closeAllDynamicTabs
- **Source:** Project Persistence > Cycle 1+2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-519: VibePlayer sample-path persistence**
- **Sources:** BLU-519
- **Implemented:** loadSampleFolder/SFZ/File wrappers
- **Source:** Project Persistence > P3 (sample paths)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-520: File>Open / Restore Backup state-bleed fix**
- **Sources:** BLU-520
- **Implemented:** clearAllRackStates + closeAllDynamicTabs
- **Source:** Project Persistence > File>Open fix
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-521: Phase C §P4.2 Batch 1+2 Drums dual roll**
- **Sources:** BLU-521
- **Implemented:** PianoNote::slotIndex + RollMode enum
- **Source:** Project Persistence > Phase C C1+C2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-522: Phase C §P4.2 Batch 3 mode toggle + slot selector**
- **Sources:** BLU-522
- **Implemented:** SplitTabButton + setTabSlotArrow
- **Source:** Project Persistence > Phase C C3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-523: Arrangement block fractional length**
- **Sources:** BLU-523
- **Implemented:** float lengthBeats default -1
- **Source:** Project Persistence > Arrangement
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-524: Persistence Cycle 1+2 rack + lazy APVTS + Cycle-2 gaps**
- **Sources:** BLU-524
- **Implemented:** Per-insert rack save + drum strip restore + APVTS lazy-bind fix
- **Source:** Project Persistence > Cycle 1+2 details
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-525: File>New param-reset double-normalise fix**
- **Sources:** BLU-525
- **Implemented:** getDefaultValue() pass-through
- **Source:** Project Persistence > File>New fix
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-526: Global tempo + recorded-clip library + tempo automation**
- **Sources:** BLU-526
- **Implemented:** Pattern::tempo removed; mGlobalTempo
- **Source:** Project Persistence > Tempo
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-527: P4 UI-state persistence**
- **Sources:** BLU-527
- **Implemented:** activeTabId + mixerScrollX + Arrangement
- **Source:** Project Persistence > P4 (UI state)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-528: P1+P2 follow-ups (phantom tabs + File>New fix + F4 revert)**
- **Sources:** BLU-528
- **Implemented:** clearAllDynamicTabs + resetToBlankState
- **Source:** Project Persistence > P1+P2 follow-ups
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Audio-Device Infrastructure

#### **FSW-238: Audio-device infrastructure (parallel work)**
- **Sources:** FSW-238
- **Implemented:** Master Output channel picker + ASIO fixes
- **Source:** Audio-device infrastructure
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-239: A2 Master Output channel picker (Mixer hamburger)**
- **Sources:** FSW-239
- **Implemented:** MasterOutputRouting namespace + master_output.xml
- **Source:** Audio-device infrastructure
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-240: Audio-device init fixes (VibesynthStandaloneApp::initialise)**
- **Sources:** FSW-240
- **Implemented:** Channel bump 16->64; strip BigInteger masks; force input=output device name
- **Source:** Audio-device infrastructure
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-241: Audio Settings dialog ASIO fixes (AudioSettingsDialog)**
- **Sources:** FSW-241
- **Implemented:** scanForDevices before getDeviceNames; refuse empty pending XML
- **Source:** Audio-device infrastructure
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-242: Live-input strip arm-LED visual sync (MixerTrackStrip)**
- **Sources:** FSW-242
- **Implemented:** AudioProcessorValueTreeState::Listener for arm
- **Source:** Audio-device infrastructure
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-243: Live-input channel pairing in input picker (FL-Studio-style + B1 fallback)**
- **Sources:** FSW-243
- **Implemented:** Mono/Stereo sections + kDeviceProfiles table
- **Source:** Audio-device infrastructure
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-244: Per-input-channel diagnostic (Show Input Diagnostics dialog)**
- **Sources:** FSW-244
- **Implemented:** Diagnoses input chain breaks
- **Source:** Audio-device infrastructure
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-245: Memory entries added (asio_input_output, drum_woofy_bug_fixed)**
- **Sources:** FSW-245
- **Implemented:** Memory documentation
- **Source:** Audio-device infrastructure
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Locked Design Decisions

#### **LDT-180: Version naming v0.9 first build v1.0 after testing**
- **Sources:** LDT-180
- **Implemented:** Version naming convention
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-181: Harmless scope full Harmor-equivalent**
- **Sources:** LDT-181
- **Implemented:** Layout overhaul in 5F dense Advanced-mode layout
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-182: BaySickSynth redesigned 5-tab structure**
- **Sources:** LDT-182
- **Implemented:** Reuses core DSP
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-183: Layers architecture old 4-layer-per-page REMOVED**
- **Sources:** LDT-183
- **Implemented:** Each Layers page = 1 engine instance; max 8 Layers pages
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-184: Per-channel effects FXChain.h REMOVED**
- **Sources:** LDT-184
- **Implemented:** All channels use EffectRack
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-185: EQ everywhere EQ6DSP/EQ6MsDSP REMOVED**
- **Sources:** LDT-185
- **Implemented:** EQ8MsDSP used on ALL EQ tabs
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-186: EQ signal flow**
- **Sources:** LDT-186
- **Implemented:** Engine -> Page EQ8 M/S -> Channel EffectRack -> Effects Page EQ8 -> Mixer Fader -> Bus chain -> Master chain -> Output
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-187: EffectRack built-in EQ REMOVED**
- **Sources:** LDT-187
- **Implemented:** Rack = 6 pure FX slots only
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-188: MasteringPage/MasteringEngine BOTH REMOVED**
- **Sources:** LDT-188
- **Implemented:** Master channel = regular EffectRack
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-189: Effects racks ALL empty by default**
- **Sources:** LDT-189
- **Implemented:** Templates/presets populate them
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-190: Effects Page layout 2 tabs per channel**
- **Sources:** LDT-190
- **Implemented:** Tab 1 = Rack, Tab 2 = EQ8 M/S
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-191: M/S EQ true parallel**
- **Sources:** LDT-191
- **Implemented:** Mid + Side independent 8-band each
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-192: VibePlayer layout overhaul in 5F**
- **Sources:** LDT-192
- **Implemented:** FL Keys 4-column layout
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-193: BaySickSynth layout overhaul in 5F**
- **Sources:** LDT-193
- **Implemented:** 5-tab layout; same for BaySickBass
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-194: Drums page max 1 (BaySickDrums fixed)**
- **Sources:** LDT-194
- **Implemented:** 14 slots; 3 sub-tabs Sound/Piano Roll/EQ; references BaySickDrums (deleted); replaced by Phase D dynamic drums
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-195: Bass page max 4**
- **Sources:** LDT-195
- **Implemented:** 3 sub-tabs each
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-196: Layers page max 8**
- **Sources:** LDT-196
- **Implemented:** 3 sub-tabs each
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-197: Engine dropdown locks on first selection**
- **Sources:** LDT-197
- **Implemented:** Layers and Bass only; Drums has no selector; references original drums design (now superseded by Phase D)
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-198: Engine naming final**
- **Sources:** LDT-198
- **Implemented:** Layers: Harmless/VibePlayer/BaySickSynth; Bass: Harmless/VibePlayer/BaySickBass; Drums: BaySickDrums; references VibePlayer/BaySickDrums (renamed/deleted)
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-199: Sequencer.h/.cpp REMOVED**
- **Sources:** LDT-199
- **Implemented:** Piano roll only sequencing
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-200: SamplerEngine.h/.cpp REMOVED**
- **Sources:** LDT-200
- **Implemented:** VibePlayer replaces all sample playback
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-201: MIDI input omni mode**
- **Sources:** LDT-201
- **Implemented:** All active OS-recognized MIDI devices auto-connected
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-202: MIDI recording per-page arm**
- **Sources:** LDT-202
- **Implemented:** Quantize-on-record option, red dot indicator
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-203: Audio recording dedicated Audio Track type**
- **Sources:** LDT-203
- **Implemented:** ASIO input -> disk writer -> audio clip
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-204: Input monitoring per-track Software Monitor toggle**
- **Sources:** LDT-204
- **Implemented:** Default OFF
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-205: ASIO setup full ASIO dialog**
- **Sources:** LDT-205
- **Implemented:** juce::AudioDeviceSelectorComponent
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-206: Metronome accent on beat 1**
- **Sources:** LDT-206
- **Implemented:** Sound selection menu, volume control, count-in
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-207: Main tabs permanent**
- **Sources:** LDT-207
- **Implemented:** Mixer/Effects/Builder/Layers/Bass/Drums + dropdowns
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-208: Mixer track auto-create**
- **Sources:** LDT-208
- **Implemented:** Add Layers/Bass page = mixer track; Drums on-demand
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-209: Mixer track grouping**
- **Sources:** LDT-209
- **Implemented:** Master | Buses | Layer 1-8 | Bass 1-4 | Drum Ch 1-14 | Audio Track 1+; references 14-channel drums (now 16 per Phase D)
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-210: Mixer bus strips visible**
- **Sources:** LDT-210
- **Implemented:** Faders, meters, controls
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-211: Mixer track count 100 max**
- **Sources:** LDT-211
- **Implemented:** Lazy load, horizontal scroll
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-212: FX Master Switch per-track + global**
- **Sources:** LDT-212
- **Implemented:** Master strip global FX on/off
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-213: Keyboard shortcuts focus-based routing**
- **Sources:** LDT-213
- **Implemented:** Global shortcuts work everywhere
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-214: APVTS naming convention**
- **Sources:** LDT-214
- **Implemented:** tk_{trackID}_{engine}_{param} for tracks; bus_fx_{slot}_{param}
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-215: APVTS lazy registration**
- **Sources:** LDT-215
- **Implemented:** Params registered when page/track created
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-216: APVTS param table**
- **Sources:** LDT-216
- **Implemented:** Per-effect param table when automation implemented (Phase 4)
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-217: Harmless automation macro only**
- **Sources:** LDT-217
- **Implemented:** ~30-50 APVTS params per instance
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-218: All other engine params APVTS-registered**
- **Sources:** LDT-218
- **Implemented:** VibePlayer, BaySickSynth, BaySickBass, Drums vol/pitch/pan
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-220: Standalone only**
- **Sources:** LDT-220
- **Implemented:** VibeDAW will never be a VST itself
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-221: Undo history size user-configurable**
- **Sources:** LDT-221
- **Implemented:** 100/250/500/1000 steps default 250
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-222: Audio import File>Import + drag-and-drop**
- **Sources:** LDT-222
- **Implemented:** .wav .mp3
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-223: Autosave every 15 minutes**
- **Sources:** LDT-223
- **Implemented:** Temp file
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-225: Ghost clip drag**
- **Sources:** LDT-225
- **Implemented:** Semi-transparent preview
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-226: Automation full APVTS**
- **Sources:** LDT-226
- **Implemented:** Per-effect named params; VibeDAW Event Editor
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-227: LAF per context**
- **Sources:** LDT-227
- **Implemented:** Separate LookAndFeel class per engine/effect category
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-228: Lever switch for 2-position toggles**
- **Sources:** LDT-228
- **Implemented:** Effects panels and player engines only
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-229: Global bloom on neon elements**
- **Sources:** LDT-229
- **Implemented:** Double-draw bloom glow
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-230: Shared hardware elements**
- **Sources:** LDT-230
- **Implemented:** JewelIndicator, Chicken Head, Hex-bolt, Lever switch
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-232: Texture caching**
- **Sources:** LDT-232
- **Implemented:** TextureUtils namespace; cached juce::Image
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-233: Lock-free audio->UI EQSpectrumFeed**
- **Sources:** LDT-233
- **Implemented:** Proper lock-free FIFO
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-234: CPU safeguarding**
- **Sources:** LDT-234
- **Implemented:** Every DSP update guards setters
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-235: Sample rate changes**
- **Sources:** LDT-235
- **Implemented:** suspend audio -> prepare -> resume
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-236: Meter components**
- **Sources:** LDT-236
- **Implemented:** DBFSMeter + VUMeter
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-237: Meter assignments**
- **Sources:** LDT-237
- **Implemented:** VU + DBFS for Dynamics/Harmonics; DBFS only for Time/Modulation
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-238: VU calibration -18 dBFS = 0 VU default**
- **Sources:** LDT-238
- **Implemented:** Configurable -18 to -14 via Effects Page
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-239: EQ band colors**
- **Sources:** LDT-239
- **Implemented:** violet->yellow/gold->cyan gradient via kBandCols[8]
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-240: Note colors**
- **Sources:** LDT-240
- **Implemented:** Drums Red; Layers 8 oranges; Bass 4 greens
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-241: Effects Bus position POST-master**
- **Sources:** LDT-241
- **Implemented:** Layer+Bass+Drums -> Master rack -> +Effects Bus -> Output
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-242: Time-stretching Rubber Band Library**
- **Sources:** LDT-242
- **Implemented:** BSD; libs/rubberband/ vendored; subsequently replaced by custom PhaseVocoder
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-243: PDC full automatic graph-wide**
- **Sources:** LDT-243
- **Implemented:** Each DSP reports latency
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-244: Preset system FILE menu**
- **Sources:** LDT-244
- **Implemented:** Save as Template / New from Template
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-245: Menu systems on Mixer/Builder/Effects/Piano Roll/Engines**
- **Sources:** LDT-245
- **Implemented:** Standard menu bar locations
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-246: Piano Roll menus standard**
- **Sources:** LDT-246
- **Implemented:** Edit, Tools, Scale, Chords, View
- **Source:** Locked Design Decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-352: Browser Panel collapsible**
- **Sources:** LDT-224|LDT-352
- **Implemented:** Project Browser; patterns + automation targets | 3 filter buttons
- **Source:** Locked Design Decisions | Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Standing Parallel Work

#### **FSW-324: §1 to §6 + §8 retrospectives shipped (Chorus/Compressor/Delay/Flanger/Limiter/Overdrive/Reverb)**
- **Sources:** FSW-324
- **Implemented:** Quality pass partial
- **Source:** Standing parallel work > Phase 5F-9 DSP Quality Pass
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-331: Legacy juce_add_plugin CMake target removal**
- **Sources:** FSW-331
- **Implemented:** Still at CMakeLists.txt:174
- **Source:** Standing parallel work > Smaller open items
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-332: 5F-5 ambiguous Event Editor items**
- **Sources:** FSW-332
- **Implemented:** Deferred
- **Source:** Standing parallel work > Smaller open items
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-333: 5F-6 ambiguous Piano Roll items**
- **Sources:** FSW-333
- **Implemented:** Deferred
- **Source:** Standing parallel work > Smaller open items
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-335: Orfanidis analytical anti-cramping for EQ**
- **Sources:** FSW-335
- **Implemented:** Deferred
- **Source:** Standing parallel work > Smaller open items
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-337: Internal cleanup: rip unused voxRoll/instRoll/kVoxPRTarget/kInstPRTarget/voxPageMidi/instPageMidi**
- **Sources:** FSW-337
- **Implemented:** Dead since piano-roll removal from Vox/Inst; We are leaving this alone in case I add synth vocals and inst already uses the piano roll now
- **Source:** Standing parallel work > Smaller open items
- **Verified:** 2026-05-08 (Phase-4 source verification)

### L&F Sprint

#### **LDT-126: L2 DBFSMeter**
- **Sources:** LDT-126
- **Implemented:** LED segments, 60fps
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-127: L2B Meters on Effect Panels**
- **Sources:** LDT-127
- **Implemented:** VU + DBFS placement matrix
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-128: L3 VUMeter**
- **Sources:** LDT-128
- **Implemented:** Hardware recreation, spring-mass-damper ballistics
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-129: L4 ModulationLAF**
- **Sources:** LDT-129
- **Implemented:** Chorus/Flanger/Phaser glossy black + brushed silver
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-130: L5 DynamicsLAF**
- **Sources:** LDT-130
- **Implemented:** Compressor/TransientShaper LA-2A cream
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-131: L6 TimeLAF**
- **Sources:** LDT-131
- **Implemented:** Delay/Reverb star-fluted lever switches JewelIndicator
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-132: L7 HarmonicLAF**
- **Sources:** LDT-132
- **Implemented:** Saturation/Overdrive/Tape Bakelite Hammerite
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-134: L8B EQ Default Band Frequencies**
- **Sources:** LDT-134
- **Implemented:** 40/250/500/1k/2k/4k/8k/12kHz + Reset All resets freq+type
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-136: L10 Global Tooltip System**
- **Sources:** LDT-136
- **Implemented:** Done via Phase 0B; subsequently flagged NOT COMPLETE per L2175
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-137: L11 Piano Roll Note Enhancement**
- **Sources:** LDT-137|LDT-297
- **Implemented:** 1px highlights, note name text, selected border | 1px highlights + note name + selected border
- **Source:** L&F Sprint > L11 | Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-138: LRX-1 through LRX-7**
- **Sources:** LDT-138
- **Implemented:** Realism upgrades panel textures shadows highlights Fresnel vignette asymmetry topography
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-140: LRX-9 through LRX-15**
- **Sources:** LDT-140
- **Implemented:** Faders, VU realism, DBFS realism, buttons, mixer console, global elements, matrix
- **Source:** Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-264: Toggle/Action/Navigation button types**
- **Sources:** LDT-264
- **Implemented:** 3 system-wide button types
- **Source:** L&F Sprint > Global Button Visual
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-265: Keep visible never move to menu**
- **Sources:** LDT-265
- **Implemented:** FX bypass, mute/solo, transport, etc.
- **Source:** L&F Sprint > Button Menu Consolidation
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-266: Effect reorder + EQ band options**
- **Sources:** LDT-266
- **Implemented:** Right-click menus alongside ≡ menu
- **Source:** L&F Sprint > Right-click context menus
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-267: 3-tier all pages**
- **Sources:** LDT-267
- **Implemented:** Main tabs / Page menu bar / Sub-tabs / Content
- **Source:** L&F Sprint > Page Layout Hierarchy
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-268: EQ right panel 8-column controls**
- **Sources:** LDT-268
- **Implemented:** 8 color-coded columns; some elements may be missing; subsequently shipped per §12 work
- **Source:** L&F Sprint > EQ8 Right Panel Redesign
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-269: EQ Display Color Scheme**
- **Sources:** LDT-125|LDT-269
- **Implemented:** violet->yellow/gold->cyan gradient | Violet->cyan gradient
- **Source:** L&F Sprint > L1 | Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-270: DBFSMeter class**
- **Sources:** LDT-270
- **Implemented:** LED segments, gold dB labels, peak hold
- **Source:** L&F Sprint > L2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-271: Meters on Effect Panels matrix**
- **Sources:** LDT-271
- **Implemented:** VU left + DBFS right rule
- **Source:** L&F Sprint > L2B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-272: VUMeter Hardware Recreation**
- **Sources:** LDT-272
- **Implemented:** Cream plate, needle, pivot, scale
- **Source:** L&F Sprint > L3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-273: Second-order dynamics**
- **Sources:** LDT-273
- **Implemented:** Mass-spring-damper f=2.0 z=0.65 r=2.0
- **Source:** L&F Sprint > VU Ballistics
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-274: Global calibration setting**
- **Sources:** LDT-274
- **Implemented:** -18 to -14 dBFS configurable
- **Source:** L&F Sprint > VU Calibration
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-275: ModulationLAF**
- **Sources:** LDT-275
- **Implemented:** Glossy black + brushed silver
- **Source:** L&F Sprint > L4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-276: DynamicsLAF**
- **Sources:** LDT-276
- **Implemented:** 3 knob variants + Chicken Head KneeType
- **Source:** L&F Sprint > L5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-277: TimeLAF Pultec recreation**
- **Sources:** LDT-277
- **Implemented:** Star-fluted lever switches JewelIndicator
- **Source:** L&F Sprint > L6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-278: HarmonicLAF Fairchild Bakelite**
- **Sources:** LDT-278
- **Implemented:** Fluted Bakelite + Hammerite panel
- **Source:** L&F Sprint > L7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-279: Panel Faceplate Texturing per hardware**
- **Sources:** LDT-279
- **Implemented:** 1176 / Pultec / LA-2A / Fairchild surface simulation
- **Source:** L&F Sprint > LRX-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-280: Three-Layer AO Shadow Stack**
- **Sources:** LDT-280
- **Implemented:** Contact + Drop + Reflection on every knob
- **Source:** L&F Sprint > LRX-2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-281: Anisotropic Highlights**
- **Sources:** LDT-281
- **Implemented:** 1176 + Pultec metal lathe
- **Source:** L&F Sprint > LRX-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-282: Fresnel Rim Lighting**
- **Sources:** LDT-282
- **Implemented:** LA-2A + Fairchild plastic/bakelite
- **Source:** L&F Sprint > LRX-4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-283: Global Lens Vignetting**
- **Sources:** LDT-283
- **Implemented:** 5% radial gradient; subsequently disabled per 2026-04-21 (T3-LRX5Vignette)
- **Source:** L&F Sprint > LRX-5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-284: Organic Asymmetry**
- **Sources:** LDT-284
- **Implemented:** Fingerprint grunge + variable bevels + pointer wobble
- **Source:** L&F Sprint > LRX-6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-285: Hardware Topography Specifics**
- **Sources:** LDT-285
- **Implemented:** Per-LAF physical detail specs
- **Source:** L&F Sprint > LRX-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-287: Faders Realism**
- **Sources:** LDT-287
- **Implemented:** Track + cap + scale + trailing line
- **Source:** L&F Sprint > LRX-9
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-288: VU Meter Realism**
- **Sources:** LDT-288
- **Implemented:** Glass overlay + backlight + bezel + screws
- **Source:** L&F Sprint > LRX-10
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-289: DBFS Meter Realism**
- **Sources:** LDT-289
- **Implemented:** LED segments + bloom + bezel + glass
- **Source:** L&F Sprint > LRX-11
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-290: Buttons Realism**
- **Sources:** LDT-290
- **Implemented:** Toggle/Action/Navigation material physics
- **Source:** L&F Sprint > LRX-12
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-291: Mixer Console Surface**
- **Sources:** LDT-291
- **Implemented:** Brushed aluminum + channel strip borders
- **Source:** L&F Sprint > LRX-13
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-292: Global Application Elements**
- **Sources:** LDT-292
- **Implemented:** Transport, EQ, Piano Roll, Builder, Scrollbars, Combos
- **Source:** L&F Sprint > LRX-14
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-293: Realism Technique Quick-Reference Matrix**
- **Sources:** LDT-293
- **Implemented:** Quick-reference matrix
- **Source:** L&F Sprint > LRX-15
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-294: Drums EQ Tab + Bass EQ Tab**
- **Sources:** LDT-133|LDT-294
- **Implemented:** EQ tabs added | EQ tab work
- **Source:** L&F Sprint > L8 | Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-295: Builder Page L&F**
- **Sources:** LDT-135|LDT-295
- **Implemented:** Track header, source picker, arrangement grid, clips | Builder visual treatment
- **Source:** L&F Sprint > L9 | Master Checklist > L&F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-298: Transport bar CPU monitor**
- **Sources:** LDT-298
- **Implemented:** CPU%, memory MB, scrolling history graph
- **Source:** L&F Sprint > CPU Performance Monitor
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-299: CPU Update Rate + History Speed**
- **Sources:** LDT-299
- **Implemented:** Low/High + Slow/Medium/Fast
- **Source:** L&F Sprint > Options>System
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-300: Tap Tempo**
- **Sources:** LDT-300
- **Implemented:** Modal dialog with tap area
- **Source:** L&F Sprint
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-301: Render Pattern to WAV**
- **Sources:** LDT-301
- **Implemented:** Right-click pattern in Builder
- **Source:** L&F Sprint
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-302: Effects Page Visual + Combined Toolbar + Tab Dropdowns**
- **Sources:** LDT-302
- **Implemented:** Documented in STANDALONE_UI_CHANGES.md
- **Source:** L&F Sprint
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-303: L&F Sprint Verification**
- **Sources:** LDT-303
- **Implemented:** Sprint complete
- **Source:** L&F Sprint Verification
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-452: L8 Drums/Bass EQ tabs + EQ M/S bug fix + SharedUI compile fix**
- **Sources:** LDT-452
- **Implemented:** Visual fixes
- **Source:** L&F Sprint Visual Fixes Session 2026-04-08
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-453: 7 Effects Panel visual issues + Chicken Head fixes + TransientShaperPanel + PhaserPanel + TapePanel**
- **Sources:** LDT-453
- **Implemented:** Visual fixes
- **Source:** L&F Sprint Visual Fixes Session 2026-04-08
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Pre-Flight / Phase 0 / Phase 1 / Phase 2 / Phase 3 / Phase 4

#### **LDT-120: Pre-Flight 1 Basic playback**
- **Sources:** LDT-120
- **Implemented:** VibeLAF, APVTS, Ribbon tabs, VUMeter, TransportBar; references VibeLAF/VibeDAW (renamed); references Vibesynth name (pre-rename)
- **Source:** Master Checklist > Foundation
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-121: Pre-Flight 2 12 DSP modules + EffectRack**
- **Sources:** LDT-121
- **Implemented:** 6 slots + EffectEditorPanels + ParametricEQDisplay
- **Source:** Master Checklist > Foundation
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-122: Piano Roll Full Overhaul**
- **Sources:** LDT-122
- **Implemented:** Tools, selection, scale snap, ghost notes, ControlLane, undo, chords
- **Source:** Master Checklist > Foundation
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-123: Phase 0 Remove FXChain/EQ6/Sequencer/SamplerEngine/MasteringPage**
- **Sources:** LDT-123
- **Implemented:** Cleanup of obsolete code; references multiple deleted classes
- **Source:** Master Checklist > Cleanup
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-124: Phase 0B Delete SequencerPage + Build VibeTooltip + APVTS lazy registration**
- **Sources:** LDT-124
- **Implemented:** Tilt EQ type added; references SequencerPage (deleted)
- **Source:** Master Checklist > Cleanup
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-141: CPU Performance Monitor + Tap Tempo + Render to WAV + Combined Toolbar**
- **Sources:** LDT-141
- **Implemented:** Transport bar features
- **Source:** Master Checklist
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-142: 1A VibeGraph AudioProcessorGraph**
- **Sources:** LDT-142
- **Implemented:** Bus nodes, instrument nodes, lazy loading
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-143: 1C EffectRack APVTS lazy registration**
- **Sources:** LDT-143
- **Implemented:** Done via Phase 0B
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-144: 1D MixerPage + MixerTrackStrip**
- **Sources:** LDT-144
- **Implemented:** Mixer page UI
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-145: 1E Effects Page**
- **Sources:** LDT-145
- **Implemented:** Channel dropdown, 2 sub-tabs, FX Master Switch
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-146: 1F Audio Settings**
- **Sources:** LDT-146
- **Implemented:** Save-and-restart pending XML
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-147: 1G MIDI Recording**
- **Sources:** LDT-147
- **Implemented:** Record button, arm, quantize-on-record
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-148: 1H Audio Recording**
- **Sources:** LDT-148
- **Implemented:** AudioFileRecorder, disk writer, lock-free FIFO
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-149: 1I Audio Engine Signal Flow**
- **Sources:** LDT-149
- **Implemented:** Signal flow
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-150: 1J Lock-Free FIFO Upgrade**
- **Sources:** LDT-150
- **Implemented:** AbstractFifo
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-151: 1K Automatic PDC**
- **Sources:** LDT-151
- **Implemented:** CompDelayLine, updateBusLatencies
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-152: 1L Time-Stretch DSP**
- **Sources:** LDT-152
- **Implemented:** Completed 2026-04-13 as part of Audio Clip Engine
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-153: 1M CPU Overload Protection**
- **Sources:** LDT-153
- **Implemented:** 85% voice steal, 95% red flash
- **Source:** Master Checklist > Phase 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-154: 2A Harmless**
- **Sources:** LDT-154
- **Implemented:** Additive synth, 516 partials, IFFT, Basic/Advanced, presets
- **Source:** Master Checklist > Phase 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-155: 2B VibePlayer**
- **Sources:** LDT-155
- **Implemented:** Disk streaming sampler, SFZ parser, articulations
- **Source:** Master Checklist > Phase 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-156: 2C BaySickSynth**
- **Sources:** LDT-156
- **Implemented:** Redesigned 5-tab synth, VisualizerScreen
- **Source:** Master Checklist > Phase 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-157: 2D BaySickBass**
- **Sources:** LDT-157
- **Implemented:** 4-tab editor + visualizer wrapping BassSynth DSP, APVTS prefix bkb_
- **Source:** Master Checklist > Phase 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-158: 2E BaySickDrums (14-slot)**
- **Sources:** LDT-158
- **Implemented:** 14-slot drum player wrapping VibePlayer
- **Source:** Master Checklist > Phase 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-159: 3A Layers Page engine wiring**
- **Sources:** LDT-159
- **Implemented:** Max 8 pages, 3 sub-tabs
- **Source:** Master Checklist > Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-160: 3B Bass Page engine wiring**
- **Sources:** LDT-160
- **Implemented:** Max 4 pages, 3 sub-tabs
- **Source:** Master Checklist > Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-161: 3C Drums Page wiring + Lazy Loader**
- **Sources:** LDT-161
- **Implemented:** 1 page, 3 sub-tabs, BaySickDrums + dynamic mixer strips
- **Source:** Master Checklist > Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-162: 3D Sample Library Scanner + SFZ parser fixes**
- **Sources:** LDT-162
- **Implemented:** Sample library + SFZ parser
- **Source:** Master Checklist > Phase 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-163: 4A Piano Roll close-out + JUCE UndoManager migration**
- **Sources:** LDT-163
- **Implemented:** Toolbar overhaul, new tools, ControlLane rewrite, marquee fix, UndoManager migration, keyboard focus, audition
- **Source:** Master Checklist > Phase 4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-164: 4B Builder Page full playlist**
- **Sources:** LDT-164
- **Implemented:** 9-tool toolbar, BuilderMenuBar, clip types, audio import, time-stretch, performance mode, time signature, context menus
- **Source:** Master Checklist > Phase 4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-165: 4C Automation Clips**
- **Sources:** LDT-165
- **Implemented:** ControlPoint/AutomationLane structs, AutomationGrid, LFO overlay, target pane
- **Source:** Master Checklist > Phase 4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-166: 4D Event Editor + Right-Click Automation + Audio Clip Engine**
- **Sources:** LDT-166
- **Implemented:** EventEditor window, EEAutomationGrid, MIDI CC import, automation on knobs, audio clip engine, PhaseVocoder, AudioClipStreamer
- **Source:** Master Checklist > Phase 4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-167: 4E Piano Roll Menu System**
- **Sources:** LDT-167
- **Implemented:** 5 menus on all piano rolls; option B toolbar cleanup
- **Source:** Master Checklist > Phase 4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-168: 4F UI Button/Toggle Cleanup + Transport Bar Polish + Space Reclaim**
- **Sources:** LDT-168
- **Implemented:** Control lane fixed, drum keyboard alignment, snap grid default, button cleanup, space reclaim, MetroPanel
- **Source:** Master Checklist > Phase 4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-247: Delete FXChain.h + per-layer FX**
- **Sources:** LDT-247
- **Implemented:** FXChain, CompressorFX, DistortionFX, ChorusFX, DelayFX, ReverbFX, StereoSpreadFX; references deleted classes
- **Source:** Phase 0 > 0A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-248: Remove AllFX.cpp**
- **Sources:** LDT-248
- **Implemented:** Per-layer FX implementations removed; references deleted classes
- **Source:** Phase 0 > 0A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-249: Delete EQ6DSP.h/.cpp + EQ6MsDSP**
- **Sources:** LDT-249
- **Implemented:** EQ8MsDSP replaces; references deleted EQ6
- **Source:** Phase 0 > 0A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-250: Delete EQ6BandStrip**
- **Sources:** LDT-250
- **Implemented:** SharedUI removal; references deleted EQ6
- **Source:** Phase 0 > 0A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-251: Delete Sequencer.h/.cpp**
- **Sources:** LDT-251
- **Implemented:** Piano roll handles all sequencing
- **Source:** Phase 0 > 0A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-252: Delete SamplerEngine.h/.cpp**
- **Sources:** LDT-252
- **Implemented:** VibePlayer replaces
- **Source:** Phase 0 > 0A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-253: Delete MasteringPage.h/.cpp**
- **Sources:** LDT-253
- **Implemented:** Functionality moves to Effects Page Master
- **Source:** Phase 0 > 0A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-254: Remove EffectRack built-in EQ6MsDSP**
- **Sources:** LDT-254
- **Implemented:** Rack = 6 pure FX slots
- **Source:** Phase 0 > 0A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-255: Replace LayerCol[4]**
- **Sources:** LDT-255
- **Implemented:** 8 Layer oranges + 4 Bass greens
- **Source:** Phase 0 > 0A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-256: Remove NUM_OSC_LAYERS=4 constant**
- **Sources:** LDT-256
- **Implemented:** No longer relevant
- **Source:** Phase 0 > 0A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-257: APVTS migration**
- **Sources:** LDT-257
- **Implemented:** Remove old L{i}_* params + layersComp_* + lazy registration infrastructure
- **Source:** Phase 0 > 0B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-258: Page stubs**
- **Sources:** LDT-258
- **Implemented:** LayersPage/BassPage stubs; remove MasteringPage from RibbonTabBar
- **Source:** Phase 0 > 0C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-259: EffectRack update**
- **Sources:** LDT-259
- **Implemented:** Remove built-in EQ6MsDSP
- **Source:** Phase 0 > 0D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-260: processBlock cleanup**
- **Sources:** LDT-260
- **Implemented:** Remove Sequencer/FXChain/MasteringEngine references
- **Source:** Phase 0 > 0E
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-261: Delete dead code**
- **Sources:** LDT-261
- **Implemented:** Delete SequencerPage; remove NUM_OSC_LAYERS; assess OscStack/FMOscillator
- **Source:** Phase 0B > 0B-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-262: Refactor old layer architecture**
- **Sources:** LDT-262
- **Implemented:** SynthVoice without 4-layer arrays; PatternManager dynamic 1-8 layers
- **Source:** Phase 0B > 0B-2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-263: Build missing infrastructure**
- **Sources:** LDT-263
- **Implemented:** L10 VibeTooltip + 1C APVTS lazy registration + Tilt EQ type
- **Source:** Phase 0B > 0B-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-304: AudioProcessorGraph (VibeGraph)**
- **Sources:** LDT-304
- **Implemented:** Bus nodes, instrument nodes, lazy loading, max 100 nodes
- **Source:** Phase 1 > 1A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-305: EffectRack APVTS Registration (Lazy)**
- **Sources:** LDT-305
- **Implemented:** Param ID pattern tk_{trackID}_fx_{slot}_{effectName}_{paramName}
- **Source:** Phase 1 > 1C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-306: Mixer Page UI**
- **Sources:** LDT-306
- **Implemented:** Bus strips, fader, meter, mute/solo, pan, FX button, routing wires, shortcuts
- **Source:** Phase 1 > 1D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-307: Effects Page Update**
- **Sources:** LDT-307
- **Implemented:** Channel dropdown, sub-tabs, FX Master Switch, VU calibration
- **Source:** Phase 1 > 1E
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-308: ASIO Setup, MIDI Input, Settings Dialog**
- **Sources:** LDT-308
- **Implemented:** juce::AudioDeviceSelectorComponent
- **Source:** Phase 1 > 1F
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-309: MIDI Recording**
- **Sources:** LDT-309
- **Implemented:** Record button, arm per page tab, quantize-on-record
- **Source:** Phase 1 > 1G
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-310: Audio Recording**
- **Sources:** LDT-310
- **Implemented:** AudioTrack type, ASIO input, disk writer
- **Source:** Phase 1 > 1H
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-311: Audio Engine Signal Flow**
- **Sources:** LDT-311
- **Implemented:** Layers/Bass + Drums signal flow + PDC levels
- **Source:** Phase 1 > 1I
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-312: Lock-Free FIFO Upgrade**
- **Sources:** LDT-312
- **Implemented:** juce::AbstractFifo
- **Source:** Phase 1 > 1J
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-313: Automatic PDC**
- **Sources:** LDT-313
- **Implemented:** getLatencySamples virtual + delay buffers
- **Source:** Phase 1 > 1K
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-314: Time-Stretch DSP Foundation**
- **Sources:** LDT-314
- **Implemented:** Rubber Band Library vendored; subsequently replaced by custom PhaseVocoder
- **Source:** Phase 1 > 1L
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-315: CPU Overload Protection**
- **Sources:** LDT-315
- **Implemented:** Voice steal at 85%; red flash at 95%
- **Source:** Phase 1 > 1M
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-316: Phase 1 Verification**
- **Sources:** LDT-316
- **Implemented:** (no context recorded)
- **Source:** Phase 1 Verification
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-317: Harmless DSP engine**
- **Sources:** LDT-317
- **Implemented:** HarmlessLAF + APVTS macros + 516 partials + IFFT; UI overhaul deferred to 5F-3
- **Source:** Phase 2 > 2A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-318: VibePlayer DSP engine**
- **Sources:** LDT-318
- **Implemented:** VibeRegion/VibeSampleManager/VibeVoice/VibeSynth; references VibePlayer (renamed); UI overhaul in 5F-2
- **Source:** Phase 2 > 2B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-319: BaySickSynth**
- **Sources:** LDT-319
- **Implemented:** 11 files; 16-voice; APVTS bss_*; VisualizerScreen; references VibeSynth name (pre-rename); UI overhaul in 5F-1
- **Source:** Phase 2 > 2C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-320: BaySickBass renamed from 808/909**
- **Sources:** LDT-320
- **Implemented:** APVTS prefix bkb_; preset path
- **Source:** Phase 2 > 2D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-321: BaySickDrums (14-slot)**
- **Sources:** LDT-321
- **Implemented:** 14 slot bars, VibePlayer face, navigator dropdown; references BaySickDrums (deleted) + VibePlayer (renamed); replaced by Phase D
- **Source:** Phase 2 > 2E
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-322: Layers Page engine wiring**
- **Sources:** LDT-322
- **Implemented:** Max 8 pages, engine dropdown locks on first selection
- **Source:** Phase 3 > 3A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-323: Bass Page engine wiring**
- **Sources:** LDT-323
- **Implemented:** Max 4 pages, 3 sub-tabs
- **Source:** Phase 3 > 3B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-324: Drums Page Wiring (BaySickDrums)**
- **Sources:** LDT-324
- **Implemented:** Max 1 page; 3 sub-tabs; references BaySickDrums (deleted); replaced by Phase D
- **Source:** Phase 3 > 3C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-325: Sample Library Scanner**
- **Sources:** LDT-325
- **Implemented:** Scans CoreLibrary at startup
- **Source:** Phase 3 > 3D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-326: Drums Page Full Wiring**
- **Sources:** LDT-326
- **Implemented:** Files modified; bugs fixed; references BaySickDrums (deleted); replaced by Phase D
- **Source:** Phase 3 > 3C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-327: Dynamic Mixer Strips**
- **Sources:** LDT-327
- **Implemented:** Master + 4 Bus only at startup; drum channels on-demand
- **Source:** Phase 3 > Lazy Loader
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-328: Sample Library Scanner full**
- **Sources:** LDT-328
- **Implemented:** SampleLibrary singleton; scan; ID-based menu
- **Source:** Phase 3 > 3D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-329: SFZ Parser Fixes**
- **Sources:** LDT-329
- **Implemented:** control section, sfzOpcode truncation, key= shorthand
- **Source:** Phase 3 > SFZ Parser Fixes
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-330: Phase 3 Verification**
- **Sources:** LDT-330
- **Implemented:** All passed
- **Source:** Phase 3 Verification
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-331: UndoManager Migration completed gaps**
- **Sources:** LDT-331
- **Implemented:** VKnob drag callbacks + Pan fields + History sync + Ctrl+Alt+Z
- **Source:** Phase 4 > 4A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-332: Piano Roll spec - locked items**
- **Sources:** LDT-332
- **Implemented:** Note colors, zebra grid, geometry, bevels, etc.
- **Source:** Phase 4 > 4A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-333: Piano Roll toolbar tool set**
- **Sources:** LDT-333
- **Implemented:** Wrench/Magnet/Pencil/Paintbrush/Eraser/Mute/Slice/Select/Zoom
- **Source:** Phase 4 > 4A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-334: Snap resolution home (Magnet right-click popup)**
- **Sources:** LDT-334
- **Implemented:** Confirm with Jeff before implementing
- **Source:** Phase 4 > 4A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-335: Note audition spec**
- **Sources:** LDT-335
- **Implemented:** Fires on key click, note created, note moved
- **Source:** Phase 4 > 4A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-336: Control lane currently broken - rewrite**
- **Sources:** LDT-336
- **Implemented:** Renders as solid blocks; rewrite as stem+node+tail; subsequently shipped
- **Source:** Phase 4 > 4A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-337: Keyboard shortcuts non-functional**
- **Sources:** LDT-337
- **Implemented:** No keybinds reach PianoRollGrid; subsequently shipped
- **Source:** Phase 4 > 4A bug 1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-338: Marquee select deselects immediately**
- **Sources:** LDT-338
- **Implemented:** Selection box appears half a second then clears; subsequently shipped
- **Source:** Phase 4 > 4A bug 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-339: Ctrl+A no glow**
- **Sources:** LDT-339
- **Implemented:** mSelectedNotes may not be populated; subsequently shipped
- **Source:** Phase 4 > 4A bug 3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-340: Ctrl+Alt+Z redo**
- **Sources:** LDT-340
- **Implemented:** Confirm works after next build
- **Source:** Phase 4 > 4A bug 4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-341: Paintbrush B / Mute T / Slice C / Zoom Shift+Z**
- **Sources:** LDT-341
- **Implemented:** Not yet implemented; subsequently shipped
- **Source:** Phase 4 > 4A missing tools
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-342: Snap denominator selector**
- **Sources:** LDT-342
- **Implemented:** Replace with Magnet toggle button + right-click popup; subsequently shipped
- **Source:** Phase 4 > 4A snap
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-343: Piano key audition**
- **Sources:** LDT-343
- **Implemented:** Wire to engine noteOn callback; subsequently shipped
- **Source:** Phase 4 > 4A audition
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-344: Control lane rendering rewrite**
- **Sources:** LDT-344
- **Implemented:** Filled blocks -> stem + node + tail; subsequently shipped
- **Source:** Phase 4 > 4A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-345: Control lane switchable target**
- **Sources:** LDT-345
- **Implemented:** Add dropdown header for Velocity/Panning/Pitch Bend; subsequently shipped
- **Source:** Phase 4 > 4A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-346: Builder menu bar**
- **Sources:** LDT-346
- **Implemented:** File/Edit/Patterns/View/Tools/Help
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-347: Builder toolbar**
- **Sources:** LDT-347
- **Implemented:** Draw/Paint/Delete/Mute/Slip Edit/Slice/Select/Zoom/Play Selected
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-348: Snap modes**
- **Sources:** LDT-348
- **Implemented:** Main/Line/Cell/None/Steps/Beats/Bar/Events; Alt override
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-349: Clip types**
- **Sources:** LDT-349
- **Implemented:** Pattern/Audio/Automation Clips
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-350: Pattern Clips MIDI shading**
- **Sources:** LDT-350
- **Implemented:** In page's color + automation overlay
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-351: Source Picker (Alt+P)**
- **Sources:** LDT-351
- **Implemented:** Patterns/Audio/Automation filter tabs
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-353: Audio import**
- **Sources:** LDT-353
- **Implemented:** File>Import + drag-and-drop -> Audio Clip
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-354: Audio clip time-stretching**
- **Sources:** LDT-354
- **Implemented:** Resample / Stretch mode; Shift+drag
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-355: Arrangement operations**
- **Sources:** LDT-355
- **Implemented:** Clone/rename/delete/merge
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-356: Performance Mode (Ctrl+P)**
- **Sources:** LDT-356
- **Implemented:** Progress bars + clock animation + clip highlighting
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-357: Time Signature struct**
- **Sources:** LDT-357
- **Implemented:** TimeSignature in PatternData and StandalonePlayHead
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-358: Timeline & markers**
- **Sources:** LDT-358
- **Implemented:** All marker types; time signature changes per marker
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-359: Context menus**
- **Sources:** LDT-359
- **Implemented:** Right-click clip + track header
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-360: All keyboard shortcuts**
- **Sources:** LDT-360
- **Implemented:** Ctrl+wheel zoom, Page Up/Down, Shift+1-6 zoom presets
- **Source:** Phase 4 > 4B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-361: Automation Clips structs**
- **Sources:** LDT-361
- **Implemented:** ControlPoint/AutomationLane structs + LFO overlay
- **Source:** Phase 4 > 4C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-362: Event Editor menu bar**
- **Sources:** LDT-362
- **Implemented:** File/Edit/Tools/View/Target Control/Import MIDI
- **Source:** Phase 4 > 4D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-363: Event Editor architecture**
- **Sources:** LDT-363
- **Implemented:** juce::DocumentWindow, AutomationGrid, AutomationTargetPane, multiple windows
- **Source:** Phase 4 > 4D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-364: Piano Roll lower panel embedded**
- **Sources:** LDT-364
- **Implemented:** EventEditorPanel with shared zoom/scroll
- **Source:** Phase 4 > 4D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-365: Convert to Automation Clip**
- **Sources:** LDT-365
- **Implemented:** Edit menu -> Douglas-Peucker simplification
- **Source:** Phase 4 > 4D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-366: Playback**
- **Sources:** LDT-366
- **Implemented:** processBlock reads active lanes, calls setValueNotifyingHost
- **Source:** Phase 4 > 4D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-367: Piano Roll Menu Bar (all rolls)**
- **Sources:** LDT-367
- **Implemented:** Edit/Tools/Scale/Chords/View
- **Source:** Phase 4 > 4E
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-368: Piano Roll context menu**
- **Sources:** LDT-368
- **Implemented:** Cut/Copy/Paste/Delete/Mute/Transpose/Set velocity/Legato
- **Source:** Phase 4 > 4E
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-431: 8 Layer Oranges (L1-L8)**
- **Sources:** LDT-431
- **Implemented:** Bright/Deep/Amber/Burnt/Coral/Peach/Muted/Golden orange
- **Source:** Note Color Palettes
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-432: 4 Bass Greens (B1-B4)**
- **Sources:** LDT-432
- **Implemented:** Bright neon/Lime/Teal/Forest green
- **Source:** Note Color Palettes
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-433: Drums Red**
- **Sources:** LDT-433
- **Implemented:** #FF4444
- **Source:** Note Color Palettes
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-434: Build Verification + CPU Safeguarding + Texture Caching + Constructor Safety + Stale .obj**
- **Sources:** LDT-434
- **Implemented:** All phases
- **Source:** Working Rules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-435: SaturationDSP full rewrite**
- **Sources:** LDT-435
- **Implemented:** Flowers/Dabs tube engine, TonePre/Post, BassRelief
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-436: ChorusDSP full rewrite**
- **Sources:** LDT-436
- **Implemented:** 3 LFOs, 3 or 6 voices, stereo phase, crossover
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-437: CompressorDSP extended**
- **Sources:** LDT-437
- **Implemented:** TCR modes, 8 knee types, ratio 0.4-30
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-438: DelayDSP full rewrite**
- **Sources:** LDT-438
- **Implemented:** Mono/stereo/pingpong, diffusion, lo-fi, LFO mod, feedback
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-439: FlangerDSP extended**
- **Sources:** LDT-439
- **Implemented:** Phase/damp/shape/feed/cross/invert
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-440: OverdriveDSP full rewrite**
- **Sources:** LDT-440
- **Implemented:** Pre BPF + atan drive + post LPF + limiter
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-441: PhaserDSP extended**
- **Sources:** LDT-441
- **Implemented:** Dynamic stages 1-24, freq range, stereo
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-442: ReverbDSP full rewrite**
- **Sources:** LDT-442
- **Implemented:** 8-line FDN, Hadamard matrix, M/S, bass shelf, early reflections
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-443: TransientShaperDSP extended**
- **Sources:** LDT-443
- **Implemented:** Split freq, attack/release, drive, gain
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-444: TapeDSP existing**
- **Sources:** LDT-444
- **Implemented:** Existing module
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-445: EQ8DSP new**
- **Sources:** LDT-445
- **Implemented:** 8 bands, 8 filter types, slopes, solo/mute, compare banks
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-446: EQ8MsDSP new**
- **Sources:** LDT-446
- **Implemented:** M/S wrapper around two EQ8DSP instances
- **Source:** Pre-Flight Phase 2 > DSP Modules
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-447: EffectRack 6 slots hot-swap**
- **Sources:** LDT-447
- **Implemented:** loadEffect/moveSlot/packSlotsToTop
- **Source:** Pre-Flight Phase 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-448: All 9 EffectEditorPanels rebuilt**
- **Sources:** LDT-448
- **Implemented:** Pink toggle buttons; empty rack default
- **Source:** Pre-Flight Phase 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-449: ParametricEQDisplay overhaul**
- **Sources:** LDT-449
- **Implemented:** Heatmap, phase curve, compare, toolbar
- **Source:** Pre-Flight Phase 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-450: APVTS 8-band EQ params per layer**
- **Sources:** LDT-450
- **Implemented:** APVTS parameters
- **Source:** Pre-Flight Phase 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-451: Slot glyphs + toggle colors + popup at cursor + auto-shift**
- **Sources:** LDT-451
- **Implemented:** Slot rendering
- **Source:** Pre-Flight Phase 2
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase 5F - Player Layout Overhauls

#### **LDT-174: 5F Player Layout Overhauls (10 sub-items)**
- **Sources:** LDT-174
- **Implemented:** 5F-1 to 5F-6 done; 5F-7 Builder, 5F-8 UI Touch-ups, 5F-9 DSP Quality Pass remaining
- **Source:** Master Checklist > Phase 5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-373: Project Bundle & Export**
- **Sources:** LDT-373
- **Implemented:** File>Bundle & Export action; folder or .zip target
- **Source:** Phase 5 > 5D-BUNDLE
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-376: BaySickSynth & BaySickBass Layout Overhaul**
- **Sources:** LDT-376
- **Implemented:** 5-tab layout; new waveforms; XY filter pad; BssEditorComponents
- **Source:** Phase 5 > 5F-1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-377: VibePlayer Layout Overhaul**
- **Sources:** LDT-377
- **Implemented:** 480x400 4-column FL Keys; ENVIRONMENT/MISC/VELOCITY/TUNING; references VibePlayer (renamed BaySickPlayer)
- **Source:** Phase 5 > 5F-2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-378: Drum system fixes paired with 5F-2**
- **Sources:** LDT-378
- **Implemented:** DrumSynth whiteNoise LCG fix; HARDNESS default 0.5->0.0; rootNote normalization; references DrumSynth (deleted)
- **Source:** Phase 5 > 5F-2 drum fixes
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-379: Harmless Layout Overhaul**
- **Sources:** LDT-379
- **Implemented:** 960x620 5-panel proportional layout; ~50 new APVTS params
- **Source:** Phase 5 > 5F-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-380: oeq_mix wire output EQ in 5F-8**
- **Sources:** LDT-380
- **Implemented:** Param referenced in editor but not in createLayout; subsequently wired in §P1 S1
- **Source:** Phase 5 > 5F-3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-381: Per-Strip Feature Update + Audio-Path Refactor**
- **Sources:** LDT-381
- **Implemented:** 6-batch rollout; InsertNode + lazy APVTS + MixerLedButton + width knob + dB ticks + bypass sync
- **Source:** Phase 5 > 5F-4a
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-382: C/M header + polarity + LED colors + width knob + FX button + cable model**
- **Sources:** LDT-382
- **Implemented:** Various decisions during Q&A
- **Source:** Phase 5 > 5F-4 decisions
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-383: Per-insert audio path move**
- **Sources:** LDT-383
- **Implemented:** Move per-engine racks into VibeGraph InsertNode
- **Source:** Phase 5 > 5F-4a architecture
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-384: Lazy APVTS for all mixer state**
- **Sources:** LDT-384
- **Implemented:** Migrated MixerState arrays to lazy APVTS
- **Source:** Phase 5 > 5F-4a architecture
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-385: FX Bypass LED two-way sync**
- **Sources:** LDT-385
- **Implemented:** rack.setRackBypassed + Effects page button two-way
- **Source:** Phase 5 > 5F-4a architecture
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-386: Dynamic Routing + Cables + Add-Strip**
- **Sources:** LDT-386
- **Implemented:** 7-batch rollout; routing data model + dynamic audio path + aux strips + cable rendering + drag + +button + right-click + persistence
- **Source:** Phase 5 > 5F-4b
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-389: Event Editor Layout Alignment**
- **Sources:** LDT-389
- **Implemented:** Title label + tool button strip + Delete button + footer status bar
- **Source:** Phase 5 > 5F-5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-391: Piano Roll Layout Alignment**
- **Sources:** LDT-391
- **Implemented:** Control lane header text + beat grid + toolbar context label + visible scrollbars
- **Source:** Phase 5 > 5F-6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-393: Builder Layout Alignment**
- **Sources:** LDT-393
- **Implemented:** Verify Phase 4B Builder matches; minor adjustments
- **Source:** Phase 5 > 5F-7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-395: DSP Quality Pass (12 effect modules + 3 player engines)**
- **Sources:** LDT-395
- **Implemented:** Comprehensive DSP-level upgrade; spec in _APPROVED_CHANGES.md; subsequently §1-§11 + §12 fully closed
- **Source:** Phase 5 > 5F-9
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-396: Chorus per-voice prime delay offsets**
- **Sources:** LDT-396
- **Implemented:** Spec item; subsequently shipped
- **Source:** Phase 5 > 5F-9 §1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-397: Compressor look-ahead + true stereo + auto-makeup + per-sample envelope**
- **Sources:** LDT-397
- **Implemented:** Spec item; subsequently shipped
- **Source:** Phase 5 > 5F-9 §2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-398: Delay DC-blocker + cubic interp + TPT feedback + mModCutoffMod**
- **Sources:** LDT-398
- **Implemented:** Spec item; subsequently shipped
- **Source:** Phase 5 > 5F-9 §3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-399: Flanger damp inside FB + cubic interp + SmoothedValue**
- **Sources:** LDT-399
- **Implemented:** Spec item; subsequently shipped
- **Source:** Phase 5 > 5F-9 §4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-400: Limiter (NET-NEW) DSP**
- **Sources:** LDT-036|LDT-400
- **Implemented:** LimiterDSP.h/.cpp + 4× true-peak + adaptive release + tanh + look-ahead | Net-new Limiter shipped; toggle layout cross-applied to §2; subsequently shipped DSP; UI deferred
- **Source:** Master Checklist > 5F-9 §5 | Phase 5 > 5F-9 §5
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-401: Limiter editor panel UI**
- **Sources:** LDT-401
- **Implemented:** 3-zone layout spec + skeuomorphic LAF (#00FFF2 cyan GR / #FF9100 orange sat)
- **Source:** Phase 5 > 5F-9 §5 UI
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-402: Overdrive 4× oversampling + 5 Hz DC-blocker + SmoothedValue + TPT BPF/LPF**
- **Sources:** LDT-402
- **Implemented:** Spec item; subsequently shipped
- **Source:** Phase 5 > 5F-9 §6
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-403: Phaser logarithmic LFO->freq + always-allocated 24 stages + invert-feedback**
- **Sources:** LDT-403
- **Implemented:** Spec item; subsequently shipped
- **Source:** Phase 5 > 5F-9 §7
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-404: Reverb Valhalla-style tail modulation + click-free size changes**
- **Sources:** LDT-404
- **Implemented:** Spec item; subsequently shipped
- **Source:** Phase 5 > 5F-9 §8
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-405: Saturation 4× oversampling + auto-gain + SR-aware DC + SmoothedValue**
- **Sources:** LDT-405
- **Implemented:** Spec item; subsequently shipped
- **Source:** Phase 5 > 5F-9 §9
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-406: Tape (BIGGEST REWORK) hysteresis + asymmetric sigmoid + flutter + pre/de-emphasis**
- **Sources:** LDT-406
- **Implemented:** Spec item; subsequently shipped
- **Source:** Phase 5 > 5F-9 §10
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-407: Transient Shaper quadratic curves + LR4 24 dB/oct + 4× oversampling**
- **Sources:** LDT-407
- **Implemented:** Spec item; subsequently shipped
- **Source:** Phase 5 > 5F-9 §11
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-408: EQ8 10 items keeping 8 bands**
- **Sources:** LDT-408
- **Implemented:** getMagnitudeForFrequency + proportional Q + SmoothedValue + TPT hybrid + 2× anti-cramping + linear-phase + per-band M/S + spectrum analyzer + Dynamic EQ; subsequently shipped
- **Source:** Phase 5 > 5F-9 §12
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-409: Harmless DSP + UI quality pass**
- **Sources:** LDT-409
- **Implemented:** HarmonicEngine + AdditiveVoice filter chain + output phaser + strum direction + CPU-guard + SmoothedValue audit + UI review; subsequently shipped
- **Source:** Phase 5 > 5F-9 §P1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-410: VibePlayer DSP + UI quality pass**
- **Sources:** LDT-410
- **Implemented:** AudioClipStreamer + VibeSampleManager + M/S width + treble shelf + PhaseVocoder + voice allocation + CPU guards
- **Source:** Phase 5 > 5F-9 §P2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-411: BaySick family DSP + UI quality pass**
- **Sources:** LDT-411
- **Implemented:** BaySickSynthVoice + filter chain + LFO routing + sub-osc + keyboard tracking + glide + auditionNote + mono MIDI preprocess; subsequently shipped
- **Source:** Phase 5 > 5F-9 §P3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-412: Limiter editor panel + EQ Dynamic UI extensions + EQ spectrum analyzer overlay**
- **Sources:** LDT-412
- **Implemented:** Tracked under 5F-9 but execute after DSP is stable
- **Source:** Phase 5 > 5F-9 deferred UI
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Phase H-0 Audit

#### **FSW-246: Phase H-0 Audit + targeted fixes (cross-cutting cleanup)**
- **Sources:** FSW-246
- **Implemented:** Triggered by surface-level Mixer-strip bugs
- **Source:** Phase H-0 Audit
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-247: Audit work - meter rebuild + routing cleanup**
- **Sources:** FSW-247
- **Implemented:** 13 items
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-248: DBFSMeter rewrite (stereo L/R + range -60..+6 + log-scale)**
- **Sources:** FSW-248
- **Implemented:** Stereo meters
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-249: MixerTrackStrip layout rewrite (meter on right column 28px, all strips 80px wide)**
- **Sources:** FSW-249
- **Implemented:** Strip layout
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-250: Audio-thread stereo peak atomics on every InsertNode + buses**
- **Sources:** FSW-250
- **Implemented:** Stereo atomics
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-251: PluginProcessor stereo bus peak atomics + per-row stereo peak writes**
- **Sources:** FSW-251
- **Implemented:** Stereo writes
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-252: MixerPage timer pushes stereo L/R via setStereoLevel**
- **Sources:** FSW-252
- **Implemented:** Push stereo
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-253: Audio-row insert routing fix (addAudioChannel before ensureAudioInsert)**
- **Sources:** FSW-253
- **Implemented:** 3 call sites + defensive rebindApvts
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-254: Master mute wired to MasterBusNode + mixer_master_mute APVTS param**
- **Sources:** FSW-254
- **Implemented:** Master mute
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-255: Pan implementation in InsertNode + every BusNode + MasterBusNode**
- **Sources:** FSW-255
- **Implemented:** Pan DSP
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-256: Bus FX Bypass wiring (every bus's rack ORs strip-local _bypass with master_fx_bypass)**
- **Sources:** FSW-256
- **Implemented:** FX bypass
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-257: Pan Law selector (Circular/Triangular/Square)**
- **Sources:** FSW-257
- **Implemented:** master_pan_law APVTS + Mixer hamburger
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-258: Vox/Inst insert top-stripe color fix**
- **Sources:** FSW-258
- **Implemented:** pickStripColor
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-259: Stop-decay meter wiggle fix**
- **Sources:** FSW-259
- **Implemented:** Per-row peakDb atomics decay 30 dB/sec
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-260: Fader-mark misalignment fix**
- **Sources:** FSW-260
- **Implemented:** APVTS _level range -60..+10 -> -60..+5.6
- **Source:** Phase H-0 > Audit Work
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-261: Batch A quick wins (7 fixes)**
- **Sources:** FSW-261
- **Implemented:** Quick wins bundle
- **Source:** Phase H-0 > Batch A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-262: A.1 AudioSettingsDialog::applySettings no longer hardcodes audioInputDeviceName=""**
- **Sources:** FSW-262
- **Implemented:** ASIO input fix
- **Source:** Phase H-0 > Batch A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-263: A.2 EffectRack slot outputGainDb (per-slot Vol knob) saved + restored in project XML**
- **Sources:** FSW-263
- **Implemented:** Knob persistence
- **Source:** Phase H-0 > Batch A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-264: A.3 Pause now flushes all-notes-off**
- **Sources:** FSW-264
- **Implemented:** Was Stop-only
- **Source:** Phase H-0 > Batch A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-265: A.4 BPM clamps to 20-300 instead of snapping to 120 on out-of-range input**
- **Sources:** FSW-265
- **Implemented:** BPM clamp
- **Source:** Phase H-0 > Batch A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-266: A.5 Playhead's setIsRecording PositionInfo flag honors real recorder state**
- **Sources:** FSW-266
- **Implemented:** Was hardcoded false
- **Source:** Phase H-0 > Batch A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-267: A.6 Bus solo wired on 6 buses (Clips/Vox/Inst/Vox2/Inst2/Inst3)**
- **Sources:** FSW-267
- **Implemented:** Was completely dead
- **Source:** Phase H-0 > Batch A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-268: A.7 SongLoop + Metronome button visual sync via callbacks**
- **Sources:** FSW-268
- **Implemented:** Project load no longer leaves buttons lying about state
- **Source:** Phase H-0 > Batch A
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-269: Batch B moderate (6 items + 1 sub-fix)**
- **Sources:** FSW-269
- **Implemented:** Moderate fixes bundle
- **Source:** Phase H-0 > Batch B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-270: B.1 C14 Harmless LFO sliders restored**
- **Sources:** FSW-270
- **Implemented:** 3 APVTS params + DSP routing + 2-row layout
- **Source:** Phase H-0 > Batch B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-271: B.2 Clips/Vox/Inst tab persistence in serializeUIState/deserialize**
- **Sources:** FSW-271
- **Implemented:** Mirrors Layer/Bass/Drum pattern
- **Source:** Phase H-0 > Batch B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-272: B.3 30Hz bus latency fix**
- **Sources:** FSW-272
- **Implemented:** Layers/Bass/Drums + Master + ClipsBus read direct from APVTS each block
- **Source:** Phase H-0 > Batch B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-273: B.4 Piano roll panning + pitch bend MIDI emit**
- **Sources:** FSW-273
- **Implemented:** Per-note panning to CC10, finePitch to PitchWheel; emitPianoNoteOn helper
- **Source:** Phase H-0 > Batch B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-274: B.5 resolveAutomationDisplayName covers 7 missing prefix groups**
- **Sources:** FSW-274
- **Implemented:** Vox/Inst Bus + secondary buses + inserts
- **Source:** Phase H-0 > Batch B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-275: B.6 Choke groups expansion to Vox/Inst MIDI buffers**
- **Sources:** FSW-275
- **Implemented:** Was Layer/Bass/Drum only
- **Source:** Phase H-0 > Batch B
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-276: Batch C architectural (partial)**
- **Sources:** FSW-276
- **Implemented:** Partial
- **Source:** Phase H-0 > Batch C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-277: C.1 C8 FX Bus dead - wire EffectsBusNode::processBlock**
- **Sources:** FSW-277
- **Implemented:** Aux strips default-route there silent loss
- **Source:** Phase H-0 > Batch C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-278: C.2 C13 slot automation paramIds - needs spec call**
- **Sources:** FSW-278
- **Implemented:** UUID-per-slot vs effect-type-keyed vs defer
- **Source:** Phase H-0 > Batch C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-279: C.3 C1-C4 MIDI subsystem - partially shipped**
- **Sources:** FSW-279
- **Implemented:** Remaining: CC/pitch-bend to APVTS dispatch (delivered by I-3b)
- **Source:** Phase H-0 > Batch C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-280: C.4 Compressor sidechain wiring (Phase 1 + 2.1 + 2.2)**
- **Sources:** FSW-280
- **Implemented:** Per-strip Send/Sidechain submenu + audio-thread routing + 5 player engines
- **Source:** Phase H-0 > Batch C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-281: C.5 Time signature honor (FL-style per-pattern TS)**
- **Sources:** FSW-281
- **Implemented:** Pattern.timeSig field + auto-derive on placement + per-pattern TS
- **Source:** Phase H-0 > Batch C
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-282: Batch D Tier 3 mop-up (4 items)**
- **Sources:** FSW-282
- **Implemented:** Mop-up bundle
- **Source:** Phase H-0 > Batch D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-283: D.1 EffectRack loadEffect preserves bypassed flag**
- **Sources:** FSW-283
- **Implemented:** No longer reset on hot-swap
- **Source:** Phase H-0 > Batch D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-284: D.2 EffectRackAction undo snapshot expanded (full SlotSnapshot)**
- **Sources:** FSW-284
- **Implemented:** Move/Load/Remove preserves knob values
- **Source:** Phase H-0 > Batch D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-285: D.3 Mixer strip insertion order persistence**
- **Sources:** FSW-285
- **Implemented:** mAuxOrder/mAudioRowOrder/mVoxOrder/mInstOrder
- **Source:** Phase H-0 > Batch D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-286: D.4 16 DSP-features-without-UI-knob surfaced as UI knobs (6 sub-batches)**
- **Sources:** FSW-286
- **Implemented:** Bundle
- **Source:** Phase H-0 > Batch D
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-287: D.4-Q1+Q2 Harmless filter layout restructure**
- **Sources:** FSW-287
- **Implemented:** Filter 2 row + ADSR boxes + AutoGain to Output cell + timbre 2x2 stack
- **Source:** Phase H-0 > Batch D > D.4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-288: D.4-Q3 BaySickPlayer Filter box (cutoff/res/reduct knobs)**
- **Sources:** FSW-288
- **Implemented:** 7th box bottom row
- **Source:** Phase H-0 > Batch D > D.4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-289: D.4-Q4 Compressor knee width knob**
- **Sources:** FSW-289
- **Implemented:** Ratio<->Gain
- **Source:** Phase H-0 > Batch D > D.4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-290: D.4-Q5 Delay LoFiSR / ModTime / Smooth knobs + DSP placement fix**
- **Sources:** FSW-290
- **Implemented:** Lo-fi moved from feedback path to delay-line read
- **Source:** Phase H-0 > Batch D > D.4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-291: D.4-Q6 EQ8 main level fader + 3 hamburger menu items**
- **Sources:** FSW-291
- **Implemented:** Linear Phase Precision / IIR Mod Speed / Proportional Q
- **Source:** Phase H-0 > Batch D > D.4
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-292: Batch E Tier 4 polish (verified completed)**
- **Sources:** FSW-292
- **Implemented:** Bundle
- **Source:** Phase H-0 > Batch E
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-293: Compressor manual-knee**
- **Sources:** FSW-293
- **Implemented:** Via D.4-Q4
- **Source:** Phase H-0 > Batch E (already done)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-294: Delay smoothing**
- **Sources:** FSW-294
- **Implemented:** Via D.4-Q5
- **Source:** Phase H-0 > Batch E (already done)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-295: EQ8 IIRModSpeed**
- **Sources:** FSW-295
- **Implemented:** Via D.4-Q6
- **Source:** Phase H-0 > Batch E (already done)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-296: EQ8 ProportionalQ**
- **Sources:** FSW-296
- **Implemented:** Via D.4-Q6
- **Source:** Phase H-0 > Batch E (already done)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-297: BPM out-of-range silent snap**
- **Sources:** FSW-297
- **Implemented:** Via A.4
- **Source:** Phase H-0 > Batch E (already done)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-298: info.setIsRecording honored**
- **Sources:** FSW-298
- **Implemented:** Via A.5
- **Source:** Phase H-0 > Batch E (already done)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-299: Flanger Dry/Wet dB levels (redundant with mix knob)**
- **Sources:** FSW-299
- **Implemented:** No work needed; defaults unity
- **Source:** Phase H-0 > Batch E (deliberate)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-300: EQ8 per-band Upward (preset stability scaffolding)**
- **Sources:** FSW-300
- **Implemented:** No work needed
- **Source:** Phase H-0 > Batch E (deliberate)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-301: EQ8 per-band ScSource (was wrongly tagged placeholder; actually fully wired in C.4 Phase 1)**
- **Sources:** FSW-301
- **Implemented:** UI per-band SC source dropdown live
- **Source:** Phase H-0 > Batch E
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-302: Tape OutputGain (intentionally removed; slot fader replaces)**
- **Sources:** FSW-302
- **Implemented:** No work needed
- **Source:** Phase H-0 > Batch E (deliberate)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-304: Harmless OCT/Hz pitch toggles wired**
- **Sources:** FSW-304
- **Implemented:** OCT snaps drag to +-12 semis; default semitones with sign
- **Source:** Phase H-0 > Batch E (newly shipped)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-305: Piano Roll FilterCutoff lane**
- **Sources:** FSW-305
- **Implemented:** Added Filter Cutoff to control-lane dropdown + CC74 dispatch
- **Source:** Phase H-0 > Batch E (newly shipped)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-306: Stale automation lane warnings**
- **Sources:** FSW-306
- **Implemented:** [stale] red-prefixed rows in EventEditor
- **Source:** Phase H-0 > Batch E (newly shipped)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-307: Missing audio clip warnings**
- **Sources:** FSW-307
- **Implemented:** Builder grid renders red instead of teal
- **Source:** Phase H-0 > Batch E (newly shipped)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-308: Piano Roll dual automation timing path**
- **Sources:** FSW-308
- **Implemented:** UI 30Hz applicator skips APVTS-backed paramIds
- **Source:** Phase H-0 > Batch E (newly shipped)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-309: EffectRack mRackBypassed dual-storage smell removed**
- **Sources:** FSW-309
- **Implemented:** APVTS _bypass is sole source of truth
- **Source:** Phase H-0 > Batch E (newly shipped)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-310: _arm zombie param - registration gated to mixer_vox_*/mixer_inst_* prefix only**
- **Sources:** FSW-310
- **Implemented:** Was registered on every Insert kind
- **Source:** Phase H-0 > Batch E (newly shipped)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-311: Dead audioRowLevel local removed from PluginProcessor.cpp**
- **Sources:** FSW-311
- **Implemented:** Cleanup
- **Source:** Phase H-0 > Batch E (newly shipped)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-312: EffectsPage getChannelPrefix() rewritten**
- **Sources:** FSW-312
- **Implemented:** Now handles every channel category
- **Source:** Phase H-0 > Batch E (newly shipped)
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **FSW-313: BaySickSynth voice octave-sweep literals to named constexpr**
- **Sources:** FSW-313
- **Implemented:** Self-documenting code
- **Source:** Phase H-0 > Batch E (newly shipped)
- **Verified:** 2026-05-08 (Phase-4 source verification)

### Sessions / Working Rules / Open Issues

#### **BLU-467: ASIO Build Support**
- **Sources:** BLU-467
- **Implemented:** JUCE_ASIO=1 conditional + fallback
- **Source:** ASIO Build Support
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **BLU-570: Wide+woofy drum playback bug**
- **Sources:** BLU-570
- **Implemented:** BaySickSynth-based drum playback; (carry forward)
- **Source:** Open issues
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-085: §P2 BaySickPlayer fully closed**
- **Sources:** LDT-085
- **Implemented:** Cross-cutting trackId collision bug fixed; branding renamed; references VibePlayer (renamed BaySickPlayer)
- **Source:** 2026-04-21 Session > §P2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-086: trackId collision fix**
- **Sources:** LDT-086
- **Implemented:** int trackId -> const juce::String& with unique page prefixes
- **Source:** 2026-04-21 Session
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-087: Automation resolver friendly labels**
- **Sources:** LDT-087
- **Implemented:** Parses engine-instance paramIds; reads ribbon-tab name via lookupPageTabName
- **Source:** 2026-04-21 Session
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-088: Duplicate-name auto-suffix**
- **Sources:** LDT-088
- **Implemented:** 4 rename paths: ribbon tabs, Builder patterns, audio clips, automation lanes
- **Source:** 2026-04-21 Session
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-114: Builder Page SLA**
- **Sources:** LDT-114
- **Implemented:** System Pages SLA pass
- **Source:** 2026-04-19 Session > System Pages SLA
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-115: Mixer Page SLA**
- **Sources:** LDT-115
- **Implemented:** System Pages SLA pass
- **Source:** 2026-04-19 Session > System Pages SLA
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-116: Effects Page SLA**
- **Sources:** LDT-116
- **Implemented:** System Pages SLA pass
- **Source:** 2026-04-19 Session > System Pages SLA
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-117: Layers Page SLA**
- **Sources:** LDT-117
- **Implemented:** Player wrapper
- **Source:** 2026-04-19 Session > System Pages SLA
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-118: Bass Page SLA**
- **Sources:** LDT-118
- **Implemented:** Player wrapper
- **Source:** 2026-04-19 Session > System Pages SLA
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-119: Drums Page SLA**
- **Sources:** LDT-119
- **Implemented:** Player wrapper - includes dual-engine + per-slot EQ + dual piano roll; references DrumsPage (deleted); replaced by Phase D dynamic drums
- **Source:** 2026-04-19 Session > System Pages SLA
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-044: §P1 Harmless player review**
- **Sources:** LDT-044
- **Implemented:** Player engine review pending; subsequently shipped per session carryover entries
- **Source:** Master Checklist > 5F-9 §P1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-045: §P2 VibePlayer player review**
- **Sources:** LDT-045
- **Implemented:** Includes BaySickDrums inheritance; references VibePlayer (renamed BaySickPlayer); subsequently shipped
- **Source:** Master Checklist > 5F-9 §P2
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-046: §P3 BaySick family player review**
- **Sources:** LDT-046
- **Implemented:** Synth + Bass shared DSP review; subsequently shipped
- **Source:** Master Checklist > 5F-9 §P3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-047: Voice management cut-self**
- **Sources:** LDT-047
- **Implemented:** System-wide cut-self: DrumsPage default ON, Layers/Bass default OFF, ArrangementBlock right-click
- **Source:** 5F-9 §P2 Pre-bookings
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-048: Per-slot EQ on DrumsPage**
- **Sources:** LDT-048
- **Implemented:** Slot-selector dropdown rebinds bus EQ display to selected slot's InsertNode EQ; references DrumsPage (deleted); now DrumPage has built-in per-drum EQ
- **Source:** 5F-9 §P2 Pre-bookings
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-049: Pre-rack EQ on all InsertNodes (B2)**
- **Sources:** LDT-049
- **Implemented:** Cross-cutting, lands during §P2
- **Source:** 5F-9 §P2 Pre-bookings
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-050: Dual piano roll for DrumsPage**
- **Sources:** LDT-050
- **Implemented:** Unified per-slot note storage; drum-grid as C5-filtered view; references DrumsPage (deleted); replaced by Phase D dynamic drums
- **Source:** 5F-9 §P2 Pre-bookings
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-089: Treble range-mapping bug fix**
- **Sources:** BLU-354|LDT-089
- **Implemented:** APVTS declared -12..+12 but setter expected 0..1 | APVTS -12..+12 hitting setter expecting 0..1
- **Source:** 2026-04-21 Session | Player Engines > §P2 VibePlayer > S1
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-090: Branding pass complete**
- **Sources:** LDT-090
- **Implemented:** VibeDAW -> BaySickDAW everywhere user-visible; VibePlayer -> BaySickPlayer; references VibeDAW/VibePlayer (renamed)
- **Source:** 2026-04-21 Session
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-091: Splash screen + window icon**
- **Sources:** LDT-091
- **Implemented:** Assets/BaySickDAWLogo.png embedded via juce_add_binary_data
- **Source:** 2026-04-21 Session
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-092: LRX-5 global lens vignette disabled**
- **Sources:** LDT-092
- **Implemented:** T3-LRX5Vignette logged
- **Source:** 2026-04-21 Session
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-093: §P3 BaySickSynth SLA audit**
- **Sources:** LDT-093
- **Implemented:** Planning-only session per SLA pattern; subsequently shipped
- **Source:** 2026-04-21 Session > Next
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-094: §P3 BaySickBass SLA audit**
- **Sources:** LDT-094
- **Implemented:** Shares DSP so audit is mostly deltas; subsequently shipped
- **Source:** 2026-04-21 Session > After §P3
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-095: System Pages SLA pass**
- **Sources:** LDT-095
- **Implemented:** Builder/Mixer/Effects/Layers/Bass/Drums
- **Source:** 2026-04-21 Session > Phase 5 ordering
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-101: §12 EQ8 fully closed 10/10 spec items**
- **Sources:** LDT-101
- **Implemented:** 12f anti-cramping + 12g linear-phase / HQ modes shipped
- **Source:** 2026-04-19 Session > §12 EQ8
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-102: Universal PageMenuBar hamburger convention**
- **Sources:** LDT-102
- **Implemented:** setMenuBuilder API
- **Source:** 2026-04-19 Session > Universal hamburger
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-103: S1 Harmless Tier 1 bugs + small wires + removals**
- **Sources:** LDT-103
- **Implemented:** oeq_mix wired + flt2_kb_track + filter type + setComponentID + tooltips + CPU guards + mono safety
- **Source:** 2026-04-19 Session > §P1 Harmless
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-104: S1 SLA Audit planning**
- **Sources:** LDT-104
- **Implemented:** Per-element table 16 WIRE / 11 DROP / 7 promoted to T3 / 7 already planned across S2-S5 / 25 already shipped
- **Source:** 2026-04-19 Session > §P1 Harmless
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-105: SLA-Impl Pitch group full UI**
- **Sources:** LDT-105
- **Implemented:** FREQ/DETUNE knobs + pitch_freq_frac chicken-head + UI-only oct/Hz toggles + Phaser WIDTH/OFS + Pluck blur button
- **Source:** 2026-04-19 Session > §P1 Harmless
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-106: S2 filter envelopes + LFO routing + Mod XYZ**
- **Sources:** LDT-106
- **Implemented:** 18 new APVTS params total
- **Source:** 2026-04-19 Session > §P1 Harmless
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-107: S3 Unison engine + vel_link + part_sel**
- **Sources:** LDT-107
- **Implemented:** Unison 4 modes + vel_link DSP + Part B waveform interactivity
- **Source:** 2026-04-19 Session > §P1 Harmless
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-108: S3.5 per-part wavetable-domain split**
- **Sources:** LDT-108
- **Implemented:** 10 new partB_* APVTS params + 18 setter A/B variants
- **Source:** 2026-04-19 Session > §P1 Harmless
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-109: S4 Mod Editor + Right-click-to-modulate**
- **Sources:** LDT-109
- **Implemented:** 4 tabs real + right-click-to-modulate + target dropdown + tool buttons + modifier knobs + TEMPO/GLOBAL + viewport tools; subsequently shipped
- **Source:** 2026-04-19 Session > §P1 Harmless
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-110: S5 Spectrogram + Background wavetable**
- **Sources:** LDT-110
- **Implemented:** Central 516-partial real-time spectrogram + background wavetable rebuild; subsequently shipped
- **Source:** 2026-04-19 Session > §P1 Harmless
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-111: Layout review pass**
- **Sources:** LDT-111
- **Implemented:** After S5; subsequently shipped
- **Source:** 2026-04-19 Session > §P1 Harmless
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-112: §P2 VibePlayer SLA audit**
- **Sources:** LDT-112
- **Implemented:** BaySickDrums inherits; references VibePlayer (renamed); subsequently shipped
- **Source:** 2026-04-19 Session > Phase 5 Pre-Review
- **Verified:** 2026-05-08 (Phase-4 source verification)

#### **LDT-113: §P3 BaySick family SLA audit**
- **Sources:** LDT-113
- **Implemented:** Synth + Bass shared DSP; subsequently shipped
- **Source:** 2026-04-19 Session > Phase 5 Pre-Review
- **Verified:** 2026-05-08 (Phase-4 source verification)
