# Effects Subsystem Fidelity Audit — 2026-06-06

**Batch:** QA-EffectsReview (`composed-foraging-rose`) Step 1.
**Type:** Read-only fidelity audit (no source changes).  The per-fix code designs derived
from this audit live in the batch plan [`Plans & Specs/Batch Plans/composed-foraging-rose.md`](../Batch Plans/composed-foraging-rose.md);
this report is the graded findings + sources.

## Purpose

The entire effects subsystem (rack effects + the BaySickPedals roster) was built early
(Sonnet-era) against real-hardware / FL-Studio references and was never audited as a set.
This audit grades every unit against its locked reference, lists the fidelity gaps + the
non-reference extras, and flags the bugs / doc defects / hidden features found along the way.
It is the input to the QA-EffectsReview max-clone fidelity rework.

## Methodology + confidence

8 parallel read-only research passes (one per effect family) + 4 ambiguity-resolution passes.
Each: confirm the reference → research it (vendor manuals / official pages / Sound on Sound /
KVR, cited per unit) → read our DSP + panel source → grade → gap list → flag extras → severity.

- **Grade:** Faithful+ (faithful superset) · Faithful · Faithful− (faithful, minor miss) · Partial · Divergent.
- **Confidence:** HIGH = vendor manual / official page fetched; MEDIUM = aggregated snippets; flagged where thin.
- **Proprietary refs** (Sibilance ORS, SY-1 COSM, AD-2 adaptive resonance): faithful same-class emulation is the achievable bar, not bit-exact.

## Master fidelity matrix

### Compressors & dynamics
| Unit | Reference | Grade | Top gap | Sev |
|---|---|---|---|---|
| Compressor (Modern) | FL Fruity Compressor | Faithful+ | superset (lookahead/SC-HPF/Det/Peak-RMS/TCR/Mix/auto-makeup) | — |
| Compressor FET | UREI 1176 | Partial | RMS not peak detection; inverted Input (b); all-buttons faked 1000:1; grit gated >6 dB & on control signal | HIGH |
| Compressor Opto | Teletronix LA-2A | Partial | no level-dependent rising-ratio curve; release 500 ms vs 1–15 s; "warmth" advertised, not implemented | HIGH |
| Compressor CS | BOSS CS-3 | Divergent | Sustain=threshold-drop not input-drive; Tone=bipolar tilt not hi-shelf; Attack doesn't set release; ratio 5:1 vs ~8:1 | MED |
| Noise Gate | BOSS NS-2 | Faithful | binary gate, opens as slow as it closes, no hysteresis | MED |
| Bass Compressor | BOSS BC-1X | Partial | knob 1 is a "Comp" macro; real BC-1X has a discrete Threshold | MED |

### Drive / distortion / fuzz / octave
| Unit | Reference | Grade | Top gap | Sev |
|---|---|---|---|---|
| Overdrive (pedal) | BOSS OD-3 | Partial | missing 500 Hz pre-clip notch + 720 Hz rolloff; drive ceiling low | LOW-MED |
| Overdrive (rack) | FL Fruity Blood Overdrive | Partial→Divergent | band-split keeps clean residual (ref drives whole signal in-series); Post Gain bipolar vs attenuate-only; extras Bias/Parallel/Wet/OS | LOW-MED |
| Blues Drive | BOSS BD-2 | Partial | no dynamic dual-stage 2nd→3rd harmonic shift; no ~100 Hz body | MED |
| Distortion | BOSS DS-1 | Faithful− | mid scoop ~800 Hz vs real ~500 Hz | LOW |
| Fuzz | BOSS FZ-5 | Faithful | missing Boost knob | LOW |
| High-Gain | BOSS MT-2 | Partial | pre-boost too gentle (700 Hz/+9 vs 1 kHz/+36); single scoop vs double-notch V | MED |
| Octave | BOSS OC-5 | Faithful | poly granular tracking quality (inherent) | LOW |
| Bass Driver | BOSS BB-1X | Faithful/Partial | static vs MDP-adaptive; header names wrong pedal (SansAmp) | LOW + doc |
| Bass Overdrive | BOSS ODB-3 | Faithful | clean at Gain=0 vs real grit-floor | LOW |

### Saturation
| Unit | Reference | Grade | Top gap | Sev |
|---|---|---|---|---|
| Tube | Waves BB Tubes | Faithful (best in app) | missing saturation meter (cosmetic); extra Type C | LOW |
| Tape | Caelum Tape Cassette 2 | Partial | no Low-Pass control; synthetic (not sampled) hiss; no Cassette IR | MED-HIGH |
| Console | SSL / Neve (was unspecified) | Divergent | generic tanh, faithful to neither; **dead "Color" knob bug** + Tube→Console state leak | MED (bug) |

### Modulation
| Unit | Reference | Grade | Top gap | Sev |
|---|---|---|---|---|
| Chorus | FL Fruity Chorus | Partial+ | superset: Voices 3/6, 2 extra LFO waves | LOW |
| Flanger | FL Fruity Flanger | Partial | Damp re-domained to Hz vs FL 0–1; one-way sync (item c) | LOW + (c) |
| Phaser | FL Fruity Phaser | Partial+ | superset: BPM sync, LFO waves, CrossFB; stages 24 vs 23; one-way sync (item c) | LOW + (c) |
| Wah | BOSS PW-3 | Faithful | — | — |
| Acoustic Simulator | BOSS AC-3 | Faithful | extra User/IR mode | — |
| Polyphonic Synth | BOSS SY-1 | Divergent | 4 Types vs 11; mono vs polyphonic; no Guitar/Bass switch | MED-HIGH |

### Time / EQ / utility
| Unit | Reference | Grade | Top gap | Sev |
|---|---|---|---|---|
| Delay | FL Fruity Delay 3 | Faithful | Time 1–2000 vs 1–1000; missing FB-filter "Off"; doubler/duck extras | LOW |
| Reverb | FL Fruity Reeverb 2 | Partial (big superset) | Ducking built in DSP but no panel control; 5 algorithms + extras | LOW + hidden |
| Acoustic Preamp | BOSS AD-2 | Divergent | Resonance = static IR vs AD-2 adaptive analysis; always-on notch, no defeat | MED-HIGH |
| EQ8 / EQ8 M/S | FL Parametric EQ 2 | Faithful+ | superset (8 vs 7 bands deliberate; Dynamic EQ, sidechain, HQ modes) | — |
| Graphic EQ | BOSS GE-7 | Partial | top band should be high-shelf, not bell; Level −60..+12 vs ±15 | MED |
| Bass Graphic EQ | BOSS GEB-7 | Faithful | header names wrong pedal (MXR M-108); Level range | LOW + doc |
| Pro Parametric EQ | Furman PQ-3 | Partial | +86 dB on preamp alone vs combined; no Hi/Lo switch; no overload LED; Q 0.1–10 vs 0.2–3.8 | MED-HIGH |
| Transient Shaper | FL Transient Processor | Partial | Attack default +50 (FL neutral); detector fast-peak−slow-RMS vs FL slope/IEF; internal `mSustain` misnomer (label correct); 10 extras | MED |
| Limiter | FL Fruity Limiter | Faithful+ | ceiling capped 0 dB vs FL +12 dB; no SUSTAIN/RMS window | MED |
| De-Esser | Waves Sibilance | Divergent (method) | band-split compressor vs Sibilance's Organic ReSynthesis (spectral); Mode/MS/Listen built but unsurfaced | MED |
| Tuner | BOSS TU-3 | Partial | detection range 40–1500 Hz vs TU-3 16–4186 Hz; no flat-tunings | MED |

**Roll-up:** ~13 Faithful/Faithful+ · ~14 Partial · ~5 Divergent (CS, SY-1, AD-2, De-Esser, Console).

## Cross-cutting findings

- **Two real bugs:** (1) Console "Color" knob is dead in the default state — its 2nd-harmonic term is gated behind a Tube `mTransformer` flag the Console panel can't set, and it leaks nondeterministically from Tube state (`SaturationDSP.cpp:296`); (2) Flanger + Phaser one-way un-sync (`setSyncBPM(false)` never restores the manual rate) = the docket's item (c).
- **Three doc-comment defects:** Bass Driver header says "SansAmp" (is BB-1X); Bass Graphic EQ header says "MXR M-108" (is GEB-7); Overdrive header says `atan` (code is `x/(1+|x|)`).
- **Four built-but-hidden DSP features:** Reverb Ducking; De-Esser Mode (Wide/Split) / Mid-Side / Listen; Delay's full duck params; Saturation "Vocal Body" — all implemented + serialized, none wired to a panel.
- **Board basics confirmed intact:** the rack effects that appear on the BaySickPedals board use dedicated simplified panels (`*PedalPanel` + Overdrive Type::Pedal + Compressor forced to Type::CS at `BaySickPedalsProcessor.cpp:158`); none get the Basic/Advanced toggle.
- **Legacy `Tape` EffectType** = correctly aliased to SaturationDSP Type::Tape; dead, no action.

## Reference table (confirmed 2026-06-06)

| Family | Unit → reference |
|---|---|
| Pedals (BOSS) | Blues Drive=BD-2, Distortion=DS-1, Fuzz=FZ-5, High-Gain=MT-2, Noise Gate=NS-2, Octave=OC-5, Acoustic Preamp=AD-2, Acoustic Simulator=AC-3, Graphic EQ=GE-7, Bass Graphic EQ=GEB-7, Bass Compressor=BC-1X, Bass Driver=BB-1X, Bass Overdrive=ODB-3, Wah=PW-3, Synth=SY-1, Tuner=TU-3, Overdrive(pedal)=OD-3 |
| Pedal (non-BOSS) | Pro Parametric EQ = Furman PQ-3 |
| Rack (FL Studio) | Compressor=Fruity Compressor, Reverb=Reeverb 2, Chorus=Fruity Chorus, Delay=Fruity Delay 3, Flanger=Fruity Flanger, Phaser=Fruity Phaser, Overdrive(rack)=Fruity Blood Overdrive, EQ8=Parametric EQ 2, Limiter=Fruity Limiter, Transient Shaper=Transient Processor |
| Rack (other) | Saturation Tube=Waves BB Tubes, Saturation Tape=Caelum Tape Cassette 2, Saturation Console=SSL(Clean)/Neve(Dirty); De-Esser=Waves Sibilance |
| Compressor character modes | FET=1176, Opto=LA-2A, CS=BOSS CS-3, Modern=generic |

## Sources

Per-unit citations are in the session research passes (Image-Line FL manuals; boss.info; Sound on
Sound; KVR / Caelum; mastereffectspedals / Furman PQ-3B; SSL official; Wikipedia 1176; UA tips).
Confidence HIGH where the vendor page was fetched; MEDIUM (flagged) where only aggregated snippets
were available (notably: SSL bus harmonic dB figures; BB Tubes exact ranges; Waves Sibilance ORS
internals; BC-1X band count).
