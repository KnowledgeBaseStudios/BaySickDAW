# QA-EffectsReview — Effects Subsystem Fidelity Sweep — Plan (composed-foraging-rose)

**Canonical path on approval:** `Plans & Specs/Batch Plans/composed-foraging-rose.md`
(mirror from `~/.claude/plans/` + delete the home-dir copy per plan-file hygiene).
**For execution:** §0-conformant FIX plan. One large cohesive batch (Jeff's call — splitting
loses things at the seams). The punch-list below is the coverage guarantee. Code blocks show
the changed lines with `file:line` anchors; trivial value changes are stated inline.

---

## Context

The whole effects subsystem was built early (Sonnet-era) against real-hardware / FL-Studio
references and never audited as a set. QA-EffectsReview opened as a 4-bug sweep (a/b/c/d) and
expanded — per Jeff — into a full **max-clone fidelity rework** of every effect, after a
read-only research audit (Step 1, 2026-06-06; full per-cluster reports + the per-fix code
designs are in the session transcript, to be written to
`Plans & Specs/Research Reports/effects-fidelity-audit-2026-06-06.md` as Task 0).

**Goal:** every effect as close to a faithful clone of its reference as achievable (proprietary
refs = faithful same-class, not bit-exact). Beginners get a clean reference-matching panel by
default; advanced users reveal extras via a Basic/Advanced toggle.

**Risk:** mostly bounded DSP/UI fixes; four heavy new builds (⚠). No hot-path/transport risk
(that's (d), split out). **Effort:** large/multi-session — Carry-Over + running-notes carry it.

---

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC-scope | Full effects-subsystem fidelity rework, not just the 4 docket bugs. | Partial pass = doing it again (Jeff). |
| SC-structure | ONE fidelity batch (a+b+c, all per-effect fidelity, 2 bugs, 3 doc fixes, hidden-feature wiring, toggle infra). **(d) → separate `QA-MultiBlockHazard`, directly after.** | (d) is engine/hot-path, not effect fidelity. |
| SC-fidelity | Max-clone everywhere; **big build on all 4 heavy units** (De-Esser, SY-1, AD-2, Tape). | "As close to a clone as possible." |
| SC-console | Console = **Clean/Dirty toggle**: Clean=SSL (drive-scaled 2nd-harmonic), Dirty=Neve (LF-weighted sat + 2nd≈3rd + LF bloom), reusing the Tube band-split + shapers. Folds in the dead-Color fix. | Both, cheaply — Tube engine already has the parts. |
| SC-extras | **Basic/Advanced toggle button in the FULL-mode FX-rack slot header** (next to Preset; panels reuse the `mPanelMode`-style show/hide). Default **Basic** = reference-clone set. **Per-slot, saved with project, default Basic.** | Progressive disclosure serves beginner + advanced. |
| SC-extras-scope | Toggle = full FX-rack panels ONLY. NOT pedals, NOT the simplified board panels (`*PedalPanel` + Overdrive Type::Pedal + Compressor-forced-CS). Board untouched. | Verified: board uses dedicated basic panels. |
| SC-c | (c) c1 manual-rate shadow restored on un-sync (Flanger + Phaser). Delay verified correct, untouched. | Bug lives in the effects' `setSyncBPM`. |
| SC-refs | Pedals=BOSS (except Pro Parametric EQ=Furman PQ-3). Rack=FL Studio, except Sat Tube=Waves BB Tubes, Tape=Caelum Tape Cassette 2, Console=SSL/Neve, Overdrive-rack=FL Fruity Blood Overdrive, Transient=FL Transient Processor, De-Esser=Waves Sibilance. | Confirmed 2026-06-06. |

## Sub-spec calls surfaced for ExitPlanMode (deferred to task-time)

Surfaced to Jeff **as each unit is implemented**, not pre-picked:
1. **Per-unit core-vs-Advanced line** — which knobs stay Basic vs move behind Advanced (lists below are the proposed Advanced set per unit).
2. **Preset-affecting value-picks** — FET default threshold; GraphicEQ/BassGraphicEQ Level ±15 + drop the −∞ kill; Transient Attack default→0; Furman Q clamp; Flanger Damp remap; Tuner range Option A vs B. Each flagged at its task; **preset re-save required** after FET (#b), FET-"All", BassComp threshold, GraphicEQ Level.
3. **Heavy-build depth checkpoints** — confirm achievable target + asset needs at the start of each ⚠ build.

## Files to modify (by area)

DSP: `Source/DSP/{CompressorDSP,NoiseGateStyleDSP,BassCompressorStyleDSP,OverdriveDSP,BluesDriveStyleDSP,DistortionStyleDSP,FuzzStyleDSP,HighGainStyleDSP,SaturationDSP,ChorusDSP,FlangerDSP,PhaserDSP,AcousticSimulatorStyleDSP,GraphicEQStyleDSP,BassGraphicEQStyleDSP,FurmanEQStyleDSP,LimiterDSP,TransientShaperDSP,DeEsserDSP,TunerStyleDSP,PitchTrackerYIN,AcousticPreampStyleDSP,EQ8DSP,SynthStyleDSP}.{h,cpp}` + new `SibilanceSpectralProcessor.{h,cpp}`, `PolyPitchTracker.{h,cpp}`.
UI/infra: `Source/Standalone/{EffectEditorPanels,SlotComponent,EffectsPage}.{h,cpp}`, `Source/EffectRack.{h,cpp}`.

---

## Tasks

**Per-task ritual:** implement → **Tell Jeff** numbered Debug-then-Release verify (incl. extras core/advanced line + value-picks) → on PASS `/draft-doc running-notes` (apply) → `/draft-commit` → surface message + full `git status` → Jeff approves → commit (one focused commit per unit; long msgs via `git commit -F`). Diagnostics → §0 Rule 4 catalog row.

**Sequencing:** 0→1→2 first (open, infra, doc fixes), then per-family tasks 3-8; the four ⚠ builds run last in their family.

## Commit structure

Commits are sequential flag-points; the working tree always equals a verified-working state
(never edit source to shape a commit — stage in the index, or use one combined commit per
`feedback_no_source_edits_to_shape_commits.md`). One focused commit per unit/fix, in the
sequencing order above:

| # | Commit | Contents |
|---|--------|----------|
| C0 | open | plan mirror + §5/§6/§9 edits + running-notes seed + Research Report (Task 0) |
| C1 | toggle infra | Task 1 (EditorPanelBase flag + SlotComponent button + EffectRack persistence + EffectsPage wiring) — land + verify BEFORE any extras tagging, since every later panel task depends on it |
| C2 | doc fixes | Task 2 (the 3 header comments) |
| C3..Cn | one per unit | each effect's fidelity rework + its Advanced-tagging + any in-effect bug, in family order (Tasks 3-8). Trivially-related siblings (e.g. GraphicEQ + BassGraphicEQ Level range) may share a commit ONLY when verified together. |
| (heavy ⚠) | staged | each big build may split across 2 commits at a natural seam — e.g. **De-Esser** (i) surface hidden Mode/MS/Listen, then (ii) spectral build; **SY-1** (i) 11 types + Guitar/Bass, then (ii) polyphony; **Console** (i) dead-Color fix + Clean/SSL, then (ii) Dirty/Neve; **Tape** (i) Low-Pass, then (ii) IR + sampled hiss. |
| Cclose | close | docs-only (Implemented Work Log + §5 CLOSED + §9) — separate commit, clean rollback boundary |

Every commit routes through `/draft-commit`; long multi-paragraph messages via `git commit -F`
(temp file under `.git/`, removed after). Before each commit: surface the FULL `git status`
(every dirty + untracked entry, even outside scope) + the drafted message → Jeff approves → commit.
Preset-re-save items (FET threshold, FET-"All" ratio, BassComp threshold, GraphicEQ Level) are
called out in their unit commit's message.

### Task 0 — Batch open
- [ ] Mirror plan → `Batch Plans/`; delete home copy. §5 `**Plan file:**` pointer + STATUS:OPEN. Seed `Running Notes/composed-foraging-rose.md`. Write Step-1 findings → `Research Reports/effects-fidelity-audit-2026-06-06.md`. §9 Forks: QA-EffectsReview re-scope + QA-MultiBlockHazard insert. `/draft-commit` → commit.

### Task 1 — Basic/Advanced toggle infrastructure (foundational; per-slot, saved, default Basic)
Pattern mirrors `mPanelMode` + the `mModeBtn` chrome button. Five touch points:

**1a `EditorPanelBase` (`EffectEditorPanels.h`, after the `mPanelMode` block):**
```cpp
    bool mBasicMode { true };                 // per-slot, persisted; default Basic
    virtual void applyBasicMode() { resized(); }  // panels w/ extras override to setVisible(false) then relayout
```
**1b `SlotComponent` header button** (`.h` add `std::unique_ptr<juce::TextButton> mBasicBtn;` + `std::function<void(int,bool)> onBasicModeChanged;` + `void toggleBasicMode(); void refreshBasicBtnLabel();`). In `.cpp` ctor (after `mPresetBtn` around :49) create `mBasicBtn` ("Basic"/"Advanced", same chrome colours, `addChildComponent`, onClick→`toggleBasicMode`); show it in `setEditor()` under the same non-empty gate as `mPresetBtn` (around :164); lay out in `resized()` immediately LEFT of `mPresetBtn` (around :416, `header.removeFromRight(72)`); extend the `paint()` name-shrink (around :350) with the `mBasicBtn` clause.
```cpp
void SlotComponent::toggleBasicMode() {
    if (!mRack) return;
    const bool nb = ! mRack->getSlotBasicMode(mSlotIndex);
    mRack->setSlotBasicMode(mSlotIndex, nb);
    if (onBasicModeChanged) onBasicModeChanged(mSlotIndex, nb);
    if (auto* base = dynamic_cast<EditorPanelBase*>(mEditor.get())) { base->mBasicMode = nb; base->applyBasicMode(); }
    refreshBasicBtnLabel();
}   // re-layouts in place — no editor re-mount (keeps slider SafePointers/automation valid)
```
**1c persistence on `EffectRack::Slot`** (`EffectRack.h`): add `bool basicMode { true };` to `struct Slot` (next to `scPick`), carry it in the Slot move-ctor + move-assign, add `set/getSlotBasicMode`. `.cpp`: `setSlotBasicMode` skips no-op then fires `onSlotsChanged`; serialize `slotTree.setProperty("basicMode",(int)..)` in get-state, restore `getProperty("basicMode",1)` (default 1=Basic for old projects) and call `setSlotBasicMode` in the `type != None` branch.
**1d wire + stamp** (`EffectsPage.cpp`): in `buildRackTab` set `mSlots[i]->onBasicModeChanged = [this](int idx,bool b){ if(mRack) mRack->setSlotBasicMode(idx,b); }`; in `rebuildSlotEditor`, BEFORE `setEditor()`, `if (auto* b=dynamic_cast<EditorPanelBase*>(editor.get())) b->mBasicMode = mRack->getSlotBasicMode(slotIndex);` (so the first `resized()` picks the right layout).
**1e confirm excluded:** `mBasicBtn` lives only in `SlotComponent`; pedal tiles + `*PedalPanel` board panels never get it and don't override `applyBasicMode()`.
- [ ] Tell Jeff: (1) toggle on EQ8/Modern-comp shows/hides extras + reflows; (2) save+reload → per-slot state sticks, fresh slot = Basic; (3) BOSS pedal + board Compressor = no toggle, unchanged.

### Task 2 — Documentation-comment fixes (comment-only, one commit)
- [ ] `BassDriverStyleDSP.h:9-10` "SansAmp" → BOSS BB-1X. `BassGraphicEQStyleDSP.h:9` "MXR M-108" → BOSS GEB-7. `OverdriveDSP.h:8-13` `atan` → `x/(1+|x|)`. Build-only verify.

### Task 3 — EQ family
**GraphicEQ (GE-7)** `GraphicEQStyleDSP.cpp:27` — top band (idx 6) peak→high-shelf:
```cpp
    auto coefs = (idx == kNumBands - 1)
        ? juce::dsp::IIR::Coefficients<float>::makeHighShelf (mSampleRate, kFreqs[idx], 0.707f, gain)
        : juce::dsp::IIR::Coefficients<float>::makePeakFilter (mSampleRate, kFreqs[idx], 1.4f, gain);
```
Level range −60..+12 → **±15** (`setLevelDb` clamp `:53`; drop the dead −∞ kill at `:73-77` → `if(!approximatelyEqual(mLevelDb,0)) buffer.applyGain(decibelsToGain(mLevelDb));`); panel fader `EffectEditorPanels.cpp:4298` `setRange(-15,15,0.1)` + tooltip; band-6 fader tooltip `:4280` "peaking"→"high shelf"; header doc `:13-14,:34`.
**BassGraphicEQ (GEB-7)** — all-peaking is correct (no rebuildBand change); Level ±15 mirror (`BassGraphicEQStyleDSP.cpp:53,:73`; panel `:4399`; header `:14`).
**Furman (PQ-3)** — gain structure: cap preamp `setInputVolDb` clamp `0..86`→`0..kInputMaxDb(26)` (`FurmanEQStyleDSP.cpp:91`, add `static constexpr float kInputMaxDb=26.0f;`); the three +20 dB bands stack to ≈86 in the mid (header doc `:23-25` updated). **Hi/Lo switch:** add `enum class GainRange{Lo,Hi}` + `set/getGainRange` + member (default Lo); in `process()` after the bands, `if(mGainRange==Hi) buffer.applyGain(decibelsToGain(20.0f));`; serialize `gainRange` (default 0); panel `hiLoBtn` TextButton in the right cluster. **Overload LED:** `std::atomic<float> mClipLevel{0}` + `getClipLevel()`; in the preamp loop track `blockPeak=jmax(blockPeak,|driven|)` (pre-tanh) then `mClipLevel.store(blockPeak)`; panel becomes a `juce::Timer`, LED lights when `getClipLevel()>1.0f`. **Q clamp** `setQ` `0.1..10`→**0.2..3.8** (`:65`; panel Q range `:4518`; doc `:21,:48`).
**EQ8** — faithful+; tag **Advanced**: Dynamic EQ (per-band threshold/ratio/atk/rel/range/upward + GR readouts), per-band sidechain, Tilt type, HQ phase modes + linear-phase precision + anti-cramping, L/R channel routing, proportional-Q + IIR mod speed, compare banks (save/swap/lock spare). Basic: per-band Freq/Gain/Q/Type/Slope/On/Mute/Solo + Main Level.
- [ ] Tell Jeff: shelf audible on GE-7 top; ±15 ranges; Furman 86 dB now emergent + Hi/Lo + overload LED; EQ8 Advanced line.

### Task 4 — Modulation
**(c) Flanger** (`FlangerDSP`): add `float mManualRate{0.5f};` (`:42`); `setRate` records it + yields to sync; `setSyncBPM(false)` restores it.
```cpp
void FlangerDSP::setRate (float hz){ const float n=juce::jlimit(0.05f,5.0f,hz); mManualRate=n; if(mSyncBPM) return; if(n!=mRate){mRate=n; mRateSmooth.setTargetValue(n);} }
void FlangerDSP::setSyncBPM (bool sync){ if(sync==mSyncBPM) return; mSyncBPM=sync;
    if(mSyncBPM && mHostBPM>0.0){ /* existing derive-from-division */ }
    else if(!mSyncBPM){ mRate=juce::jlimit(0.05f,5.0f,mManualRate); mRateSmooth.setCurrentAndTargetValue(mRate); } }   // <-- restore (was no-op)
```
(optional: `setStateInformation` seed `mManualRate=mRate;`).
**(c) Phaser** (`PhaserDSP`): add `float mManualRate{0.5f};` (`:82`); `setRate` AND `setSweepFreq` record it + yield to sync; `setSyncBPM(false)` restores both aliases:
```cpp
void PhaserDSP::setSyncBPM (bool sync){ if(sync==mSyncBPM) return; mSyncBPM=sync;
    if(mSyncBPM) reapplyBpmSync();
    else { const float n=juce::jlimit(0.05f,getRateMaxHz(),mManualRate); mRate=n; mSweepHz=n; mRateSmooth.setCurrentAndTargetValue(n); } }
```
(`reapplyBpmSync` unchanged.)
**Flanger Damp** (value-pick: remap UI to 0–1, keep Hz DSP): panel knob `EffectEditorPanels.cpp:1987` → `{"Damp",0,1,0,0.01,...}`; onChange `:2046` maps `amt`→`fc=20000*pow(1000/20000,amt)`→`setDampHz(fc)`; init-sync `:2058` inverts. (Preset back-compat preserved — DSP/serialization untouched.)
**Chorus / Acoustic Sim** — faithful; tag Advanced (Chorus: Voices 3/6 + LFO waves Triangle/Organic; AcousticSim: User/IR mode).
**Phaser Advanced:** BPM-sync + division, LFO-wave selector, CrossFB.
- [ ] Tell Jeff: set manual rate → sync ON → OFF restores it, on both Flanger + Phaser; Damp 0–1 feel.

### Task 5 — Drives / distortion / fuzz
**Overdrive RACK (Blood Overdrive)** `OverdriveDSP.cpp` — in-series, no clean residual: in the pre-filter loop (`:319`) drop `mResidualBuf`, write only the filtered band to `mBandBuf`; in recombine (`:406`) `float out = shaped;` (no residual add). Post Gain attenuate-only: `setPostGain` clamp `-18..18`→**`-18..0`** (`:151`) + panel range. Advanced: Bias/Parallel/Wet/OS.
**Overdrive PEDAL (OD-3)** — add `mPedalNotch{L,R}` (≈500 Hz, −4.5 dB peak) + `mPedalDriveHpf{L,R}` (720 Hz 1st-order) (`.h:133`); prepare/reset them; apply notch pre-clip (1x) then the 720 Hz HPF between stage-1 and stage-2 inside the OS loop; raise drive `1 + preAmp*4`→`1 + preAmp*13` (≈+43 dB) (`:260-280`).
**Blues Drive (BD-2)** — add `mBodyPeak` (≈100 Hz +3.5 dB, `.h:54`, fixed coefs in prepare); replace the single tanh (`:88-103`) with an envelope-driven dual stage (stage-1 asym tanh; cross-fade into a cubic soft-clip `1.5a-0.5a^3` as `|s1|` rises → 2nd→3rd harmonic shift); insert `mBodyPeak.process(ctx)` after the tone LPF (`:108`).
**Distortion (DS-1)** `DistortionStyleDSP.cpp:13` — scoop recenter: `kToneLpfHz 400→250`, `kToneHpfHz 2000→1000` (geo-mean ≈500 Hz).
**Fuzz (FZ-5)** — add **Boost** knob: `setBoost`/`mBoost{0}` (`.h:45`, 0..+20 dB), apply `drive=(1+mFuzz*49)*decibelsToGain(mBoost)` (`:50`), serialize `boost` (default 0), panel: insert `{"Boost",0,20,0,0.1,...}` knob between Fuzz/Level + reindex handlers (`:3405,:3422`), header "3 controls"→"4".
**High-Gain (MT-2)** `HighGainStyleDSP` — pre-clip boost `700/+9`→**`1000/+36`** (`:3-15`); replace single post-clip scoop with two-notch V: `mFixedScoopLo`(≈100 Hz −12) + `mFixedScoopHi`(≈5 kHz −12) (`.h:64`, prepare/reset/initFixedEqCoefs/process all updated). **Caution:** +36 dB into `drive=1+mDist*999` is hot — `/test-signal` for stability; narrow boost Q if fizzy.
- [ ] Tell Jeff: per-pedal A/B vs reference; MT-2 stability check.

### Task 6 — Compressor family (incl. a + b)
**(a) Vintage-knee monotonic** `CompressorDSP.cpp:245` — taper toward a 2:1 floor, never 1:1 (keeps slope <1 → GR monotonic):
```cpp
    if (isVintage && overshoot > 0.0f) {
        constexpr float kVintageFloorRatio = 2.0f;
        const float t = juce::jlimit(0.0f,1.0f, overshoot/12.0f);
        effectiveRatio = ratio + t*(kVintageFloorRatio - ratio);
        effectiveRatio = std::max(effectiveRatio, kVintageFloorRatio);
    }   // above-knee return unchanged: threshold + overshoot/effectiveRatio - levelDb
```
**(b) FET Input un-invert** `EffectEditorPanels.cpp:402` — Input up → lower threshold → more comp; init-sync reverse-maps (preset re-save needed):
```cpp
    const float input=(float)knobs[0]->slider.getValue();           // -60..0
    if (dsp) dsp->setThreshold(-(input+60.0f)*0.70f);               // 0..-42 dB
    // init: knobs[0]->slider.setValue(jlimit(-60,0, -dsp->threshold/0.70f - 60.0f), ...);
```
**FET peak + hard knee** `setType` (`:88`, after `mOptoHistory=0`): `if(mType==FET){ peakDetection=true; kneeDb=0.0f; }` (+ FET panel ctor `setPeakDetection(true)`).
**FET all-buttons** `:382` — ratio array `...,1000.f`→`...,14.f`; on idx==4 `setAttack(jmax(attackMs,0.5f))` (lag).
**FET always-on grit on audio path** — delete the GR-signal `satGr` block (`:477-493`); make `wetL/wetR` non-const (`:531`) and after the gain multiply: `if(mType==FET){ float drv=0.15f+0.12f*jmax(0,-grL); wetL = tanh(wetL*(1+drv))/tanh(1+drv)*0.85f + wetL*0.15f; /* R same */ }`.
**Opto rising-ratio + release + warmth** — branch `computeGainDb` top: `if(mType==Opto){ if(overshoot<=0)return 0; float t=jlimit(0,1,overshoot/20); float effR=1.5f+t*(4.0f-1.5f); return (1/effR-1)*overshoot; }`; slow release `mOptoSlowRelCoef` 500ms→**1500ms** (`:221`); Opto warmth on audio path `if(mType==Opto){ wetL += (0.02f+0.03f*jmax(0,-grL))*wetL*std::abs(wetL); /* R */ }`.
**CS-3** — ratio 5→**8** (`:99`); Sustain = input drive into FIXED −24 dB threshold (rewrite `applyCsSustainMacro`: fixed threshold + `csInputDriveDb=24*s01` + `makeup=6*s01`; add member `float csInputDriveDb{0}` + apply CS-only in `process` to detector level + audio); Tone = high-shelf-only (`updateCsToneCoefs` `:112` — pin low shelf flat, high shelf ±9 dB); Attack also sets release inversely in `setAttack` (`:589`, CS-only map).
**Noise Gate (NS-2)** — add `mAttackCoef` (fast ≈1 ms open) + `kHysteresisDb=3` + per-ch `mOpenL/R` latch (`.h`); `recomputeCoefs` adds attack coef; process loop: Schmitt-trigger open/close (open at threshold, close 3 dB below) + asymmetric `coef = open?mAttackCoef:mDecayCoef`.
**Bass Comp (BC-1X)** — drop `setComp`/`mComp` macro → `setThresholdDb`/`mThresholdDb{-24}` discrete + direct `effRatio=mRatio` (remove macro constants); state key `comp`→`threshold`; panel knob[0] "Comp"→"Thresh" −48..0 (preset re-save).
**Modern Advanced:** lookahead, SC-HPF, Det window, Peak/RMS toggle, manual KneeW, Mix, Auto-MU (Basic: Thresh/Ratio/Gain/Attack/Release/KneeType/meter; Stereo-Link = Basic).
> Sequence the FET-grit + Opto-warmth edits as ONE pass (same `around :531` region, both need non-const wet). `csInputDriveDb` is the largest sub-change (member + apply-points + serialize).
- [ ] Tell Jeff: (a) loud transient compressed on Vintage; (b) FET Input up = more comp; Opto rising ratio + long release; CS sustain/tone/attack; NS fast-open; BC discrete threshold.

### Task 7 — Saturation (⚠ Console + ⚠ Tape)
**⚠ Console Clean/Dirty + dead-Color fix** `SaturationDSP` — new state: `enum ConsoleMode{Clean,Dirty} mConsoleMode{Clean}`, `bool mConsoleColor{true}`, LF-bloom shelf coef+state; new `setConsoleMode/setConsoleColor`. Rework `processConsole` signature → `(x, drive, color, colorOn, dirty, isLow)`:
```cpp
    if(!dirty){ float driveAmt=1+drive*0.25f; float y=tanh(x*driveAmt)/driveAmt;
        if(colorOn&&color>0.001f){ float ds=0.5f+0.5f*(drive/10); float a=(0.04f+(color/10)*0.36f)*ds; y+=a*(y*y-0.333f);} return y; }   // Clean=SSL
    float bandDrive=isLow?1.6f:0.7f; float driveAmt=(1+drive*0.30f)*bandDrive;     // Dirty=Neve, LF-weighted
    float y=tanh(x*driveAmt)/driveAmt;                                              // 3rd (odd)
    if(colorOn&&color>0.001f){ float a=0.06f+(color/10)*0.34f; y+=a*(y*y-0.333f);} return y;  // +2nd ≈ 3rd
```
Call sites (`:519`) pass `mConsoleColor, mConsoleMode==Dirty, isLow`; Dirty inverts the relief blend so lows dominate; LF-bloom low-shelf (≈40 Hz) added on the Dirty path post-DC-block. Serialize `consoleMode`/`consoleColor`. Panel: Clean/Dirty `ChickenHeadSelector` + Color-enable `DualLabelToggle` (default ON → Color is now LIVE in Console). **This is the dead-Color fix** (decoupled from the Tube `mTransformer` flag + leak).
**Tube Advanced:** Type C, Auto-MU, OS selector, harmonics-routing.
**⚠ Tape (Caelum TC2)** — (i) **Low-Pass** 5–22 kHz: `mTapeLpHz/Coef` + `setTapeLpHz` + 1-pole in Phase-3 post-de-emphasis; panel knob. (ii) **Cassette IR**: `juce::dsp::Convolution mTapeConv` + `mTapeIrOn` + `loadCassetteIR()` (mirror `AcousticPreampStyleDSP`'s convolution load/prepare; **IR asset** `Resources/IRs/Tape/CassetteCaelum2.wav`, identity fallback if missing); block-based `process` after output gain when on; panel toggle. (iii) **Sampled hiss**: `mTapeHissSampled` + `mTapeHissSample` looped (asset `Resources/Samples/Tape/CassetteHiss.wav`, synthetic-pink fallback) branched in the hiss block; panel toggle. Tape Advanced: Bias/Hyst/Vibe/Speed/pre-emphasis. (Convolution load on prepare/message thread only; all new keys default-preserve old presets.)
- [ ] Tell Jeff: Clean vs Dirty distinct + Color live in both; Tape LP + IR + hiss.

### Task 8 — Utility + ⚠ heavy builds
**Limiter (FL Fruity Limiter)** `LimiterDSP` — ceiling clamp `-24..0`→**`-24..+12`** (`setCeilingDb:496`; panel `:2921`; header `:48`); add **SUSTAIN** RMS window: `setSustainMs(0..1000)` + `mSusCoef` (in `recalcCoefs`) + per-sample mean-square smoother blended `peak=jmax(rawPeak, sqrt(susMs))` (keeps true-peak guarantee); serialize `sustainMs`; panel: insert "Sustain" knob in row2 + reindex. Advanced: SC-HPF, Ahead, RelCv, SatTh, SatCv, AutoRel, AutoMU, Link.
**Transient (FL Transient Processor)** — Attack default +50→**0** (panel `:2456` + DSP `mAttack{0.5}`→`{0.0}` `.h:90`); rename internal `mSustain`→`mRelease` (+ smoother), **keep XML key `"sustain"`** (no migration); panel "Release" label already correct. Advanced: Sens/Wet/FastRel/SlowAtt/OS/StereoDetect/sidechain (Basic: Attack/Release + Attack/Release Shape; Split/Balance/Drive/Gain Basic).
**Tuner (TU-3)** — Option A (bounded): `PitchTrackerYIN.h:44` `kMinFreqHz 40→30` (B0), `kMaxFreqHz 1500→4186` (C8); `TunerStyleDSP.cpp:66` gate uses the constants. (Option B = 4096 window for true 16 Hz — flag, heavier.) `/test-signal` ±1 cent validation (high octave precision is the risk). Advanced: 432 mode + Strobe (keep; LED-Bar default).
**⚠ De-Esser (Waves Sibilance)** — (1a low-effort, do first) surface the built-but-hidden Mode(Wide/Split)/MidSide/Listen in `DeEsserPanel` (3 controls; DSP+setters+serialize already exist; black label colour for the cream panel); relabel Q→**Detection** (label only, `setQ` unchanged). (1c ⚠ big) add `Mode::Spectral` + new `SibilanceSpectralProcessor.{h,cpp}` (STFT de-esser scaffolded on `PhaseVocoder`: 2048/512 75%-overlap, identity OLA, per-bin sibilance mask = in-band-energy-ratio + per-bin floor EMA + selectivity-shaped reduction, magnitude-only scale keeping phase; latency 1536 via `getLatencySamples()`); `DeEsserDSP` owns `mSpectral`, dispatches block-based in `process()` when `Mode::Spectral && !mListen`, widened mode clamp. Advanced: Atk/Rel/Mix/Lookahead (Lookahead N/A in Spectral). `/test-signal`: sibilant-burst-over-vowel, pink-noise selectivity, latency, null test.
**⚠ SY-1 (BOSS SY-1)** — (a) Types 4→**11** (enum + `kProfiles[11]` extended `TypeProfile` w/ lfoTarget/Q/detune/brightness/additivePartials; `variationMix` + triangle term in `waveSample`; clamp bump). (c) Guitar/Bass switch (`mInstrument` + range push to tracker; bass caps poly to 1-2). (b ⚠ big) **Polyphony**: new `PolyPitchTracker.{h,cpp}` (FFT harmonic-sum + iterative spectral subtraction, ≤6 notes, temporal hysteresis, double-buffered wait-free publish — mirrors `PitchTrackerYIN` threading); replace single phase accumulator with `SynthVoice mVoices[8]` pool + `updateVoiceAllocation` (match/assign/steal/release) + per-voice env+VCF; keep `mEnvelope` as global dynamics; Mono fallback retains the current path. Poly/Instrument toggles + state.
**⚠ AD-2 (Acoustic Preamp)** — replace static body-IR resonance with **adaptive**: `DynamicsAnalyzer` (fast/slow env + transient, lift the pattern from `AcousticSimulatorStyleDSP`) drives a `BodyResonanceBank` (3 `ModalResonator` biquads seeded from the existing `kRes[]` air/top/body freqs; depth←level, bloom←transient modulate Q+gain per block); Resonance knob = ceiling of dynamic depth. Replace the convolution block (`:241-259`); keep `mConv` only for `Body::User` (static, no regression). Add **Notch defeat** (`mNotchEnabled` + wrap `:296-312`; panel toggle). Advanced: Level/Body-selector/User-IR.
- [ ] Tell Jeff: per-unit verify; heavy builds get fuller A/B + the achievable-target confirmation + `/test-signal` where noted.

### Task 9 — Batch close (mandatory §0 sequence, in order)
- [ ] `/draft-doc batch-close` → compile the Implemented Work Log entry from the running notes; apply via Edit to `Plans & Specs/Implemented Work Log.md` (with `**Bucket:** Effects`).
- [ ] `/review-batch QA-EffectsReview` → audit the full diff vs this plan + CLAUDE.md rules + memory-tracked gotchas. Address BLOCKER / NEEDS-FIX in-batch; defer NITs into the close entry.
- [ ] Rule 4: walk the running-notes Diagnostic Instrumentation Catalog and strip every `Remove`-disposition site (surface the strip list to Jeff first; Keep/Remove borderline = Jeff's call).
- [ ] Route side findings per Rule 3: resolved-in-batch → close-entry routing table; outside-batch → §9 Forks entry + §5/§6/Future State edits (surface slot/placement to Jeff — don't pick).
- [ ] §5 QA-EffectsReview STATUS:CLOSED banner; finalize the §9 Forks entries (the 4-bug→full-sweep re-scope + the QA-MultiBlockHazard insert). Confirm QA-MultiBlockHazard is teed up as the next batch.
- [ ] `/draft-commit` for the close → surface message + full `git status` → Jeff approves → commit the close (separate docs-only commit, distinct from every source commit).

---

## Verification (end-to-end smoke, after all tasks)
1. **Toggle+persistence:** every reworked rack panel toggles cleanly; per-slot state saves/reloads; fresh=Basic; pedals/board unchanged.
2. **Bugs closed:** Vintage compresses loud transients (a); FET Input up=more comp (b); Flanger+Phaser restore manual rate (c); Console Color live in Clean+Dirty.
3. **Per-unit fidelity A/B** vs each reference for the gap closed; heavy builds get fuller A/B + `/test-signal` (MT-2 stability, De-Esser selectivity/latency, Tuner ±1 cent).
4. **No regressions:** presets load (preset-re-save items flagged at their tasks); board/pedal behavior unchanged; clean Debug+Release build.

## Routing notes (Rule 3)
- **(d) multi-call** → new batch **QA-MultiBlockHazard**, directly after; §5 docket + §6 arrow + §9 Forks at Task 0. **QA-EffectsReview re-scope** (4-bug → full sweep) recorded via §9 Forks at Task 0.
- If a ⚠ build's faithful target proves infeasible in-batch → surface to Jeff; route deferral via §9 Forks, never a silent cap.

## Carry-Forward Reference touch points
- Effects bucket primitives (EffectRack process order, InsertNode chain) — read before Task 1 (slot/panel lifecycle).
- `Files For Claude/DSP Review/_APPROVED_CHANGES.md` — cross-check before any module it covers.
