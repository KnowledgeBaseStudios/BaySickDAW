# PDC Coverage Audit — 2026-07-16 (QA-Fe2, pre-close)

Commissioned by Jeff mid-QA-Fe2 close after the /review-batch finding that the
vocal chain's latency was reported but never consumed.  Two-agent sweep:
(1) every latency SOURCE in Source/ with conditions + magnitudes;
(2) every rack/EQ instance vs what VibeGraph::updateBusLatencies actually
measures and compensates.  File:line evidence throughout.

**Jeff's directive (2026-07-16): EVERY gap below gets fixed IN-BATCH before
the QA-Fe2 close.**  That includes the realtime pitch-corrector monitor
latency, which the session mislabeled "by design / none needed" -- WRONG per
Jeff: the dry-monitor default was a mitigation, not a fix; corrected live
monitoring at ~48 ms is not acceptable as final design.  Options for a real
fix (e.g. dedicated low-latency monitor shifter while the WET recording keeps
R3 quality) are a spec call for Jeff.

**Already built this session (build on, do not redo):**
- `BaySickVocalProcessor::getChainLatencySamples()` (bypass-aware vocal rack total).
- `VibeGraph::onGetVoxStripChainLatency` hook (wired in VibeSynthProcessor
  prepareToPlay before the initial updateBusLatencies call).
- `updateBusLatencies` extended: compensation target = max(L/B/D bus lat,
  max Vox chain lat); source-fed insert alignment for Audio/Inst/Rusty/Vox
  inserts via the live InsertNode::compDelay; Layer/Bass/Drum inserts skipped
  (bus comp covers); Aux skipped (post-comp send inputs); no-op setDelay guard.
- 5 Hz chain-latency watch in StandaloneEditor::pollDenoiseState ->
  setLatencySamples(updateBusLatencies()) on change.

---

## Report 1 — Latency source inventory

# BaySickDAW Latency Source Inventory

## Base contract
`DSPBase::getLatencySamples()` defaults to 0 (`Source\DSP\DSPBase.h:32`). Every rack effect not listed below (Reverb, Delay, Chorus, Flanger, Phaser, GateDSP, NoiseGateStyle, Wah, GraphicEQs, Octave, NAMPedal, Tuner, AcousticSim, etc.) reports 0.

## Source table (magnitudes at 48 kHz)

| DSP | Condition for nonzero | Magnitude | Reported via | Consumed? |
|---|---|---|---|---|
| **CompressorDSP** | Look-ahead knob > 0 ms (0..5 ms, default 0) | 0..240 smp | `CompressorDSP.h:56` returns `mLASamples` (set `CompressorDSP.cpp:763-777`, max `h:251`, default `h:152`) | Yes — rack sum; panel pokes `onLatencyChanged` (`EffectEditorPanels.cpp:900`) |
| **LimiterDSP** | Always when active: Ahead default **2 ms** + 4x TP-detector OS | ~96 (default) up to ~480 + ~8 OS smp | `LimiterDSP.h:46`; `mLatencySamples = mAheadSamples + mOsLatencySamples` (`LimiterDSP.cpp:91,596,679`) | Yes — rack sum |
| **DeEsserDSP (TimeDomain)** | Lookahead knob > 0 ms (0..12 ms, default 0) | 0..576 smp | `DeEsserDSP.h:81-85` (`mLASamples`) | Yes — rack sum + `onLatencyChanged` (`EffectEditorPanels.cpp:3867`) |
| **DeEsserDSP (Spectral)** | Engine=Spectral (lookahead ignored) | **2048** (HQ, default) / **1024** (LL) = one FFT frame | forwards `SibilanceSpectralProcessor::getLatencySamples()` (`SibilanceSpectralProcessor.cpp:93-99`; configs `cpp:26-27`) | Yes — rack sum; engine/quality switches locked while transport runs (`EffectEditorPanels.cpp:3877`) |
| **DeReverbDSP** | Always when active (fixed quality) | **2048** (kFFT, ~43 ms) | `DeReverbDSP.h:28` (`kFFT` at `h:44`) | Yes — vocal chain rack sum |
| **GateDSP** | Never (no lookahead) | 0 | default | n/a (`GateDSP.h` — no override) |
| **TransientShaperDSP** | Always (OS "always runs for constant latency", `TransientShaperDSP.cpp:68`) | OS group delay ~6-12 smp (2x-16x, default 4x, `h:101`) | `TransientShaperDSP.h:55`, set `cpp:76` | Yes — rack sum |
| **SaturationDSP** | Always (OS allocated in prepare at current factor) | ~6-12 smp (2x-16x, default 4x, `SaturationDSP.h:172`) | `SaturationDSP.h:92`, set `cpp:96` | Yes — rack sum |
| **TapeDSP** | Always | same as Saturation (`TapeDSP.h:99`) | `TapeDSP.h:63`, set `cpp:136` | Yes — rack sum |
| **OverdriveDSP** | Always | ~6-12 smp (2x-16x, default 4x, `OverdriveDSP.h:101`) | `OverdriveDSP.h:60`, set `cpp:43` | Yes — rack sum |
| **6 drive pedals** (BassDriver `h:41`, BassOverdrive `h:39`, BluesDrive `h:34`, Distortion `h:33`, Fuzz `h:40`, HighGain `h:36`) | Always (fixed 4x `PolyphaseOversampler4x`, `PolyphaseOversampler.h:50-54`) | ~6-12 smp | forward `mOs.getLatencySamples()` (`PolyphaseOversampler.h:70-74`) | Yes — rack sum |
| **EQ8DSP anti-cramping (2x OS)** | AC on (user opt-in, or forced by HQ+/HQ Linear modes, `EQ8DSP.cpp:1160-1179`) | ~5-8 smp (comment `EQ8DSP.cpp:1356`) | `EQ8DSP::getLatencySamples()` `EQ8DSP.cpp:1367-1375` | Yes — via EQ8MsDSP → bus PDC |
| **EQ8DSP linear-phase modes** | Mode = Linear / HQ Linear / HQ Extended | FFT/2: Linear **1024**, HQL **2048** (+AC ~8 → "~2050", `cpp:1364-1366`), HQE **256** (sizes `cpp:1148-1151`) | same accessor; adds `mLinearProc.getLatencySamples()` | Yes — bus PDC; UI mirror at `SharedUI.cpp:5549-5565` |
| **EqLinearPhaseProcessor** | Only instantiated inside EQ8DSP (`EQ8DSP.h:340` — sole user per grep) | mFftSize/2 (`EqLinearPhaseProcessor.h:77`) | reports THROUGH `EQ8DSP::getLatencySamples()` — yes, it's summed | Yes (indirect) |
| **EQ8MsDSP** | Either sub-EQ nonzero | **Mid + Side summed** (serial, `EQ8MsDSP.h:70-73`) — worst case ~2x2050 | own override | Yes — `VibeGraph.cpp:1962-1968` (4 bus EQs) |
| **PitchCorrectorDSP** (R3 LiveShifter) | Whenever built with RubberBand (realtime corrector active) | `getStartDelay() + blockSize` ≈ **~48 ms ≈ ~2300 smp** (`PitchCorrectorDSP.h:21,40-42`, set `cpp:140`) | own accessor `h:119` — **NOT a DSPBase, not in any rack** | **Partially** — only the WET-recorder skip (`BaySickVocalProcessor.cpp:687`); **never enters PDC** (`getChainLatencySamples` `BaySickVocalProcessor.h:155-158` reads only the rack) |
| **BaySickNAMIRProcessor** | OS param = 2x/4x (default **1x** → 0, `BaySickNAMIRProcessor.cpp:106-107`) | ~4-8 smp | `setLatencySamples(oversamplingLatencySamples(f))` (`cpp:238,1016,254-260`) on its own AudioProcessor | **No** — nothing reads the sub-processor's latency; VibeGraph treats Inst/Rusty/NAM sources as zero-latency (`VibeGraph.cpp:2002`) |
| **NAMIR IR convolution** | Never — default-constructed `juce::dsp::Convolution` = zero-latency uniform-partition mode (`BaySickNAMIRProcessor.h:236-237`); `getLatency()` never called | 0 | not reported | n/a |
| **DenoiseDSP / DenoiseLearner** | Offline file cleaner + background learner only (`DenoiseDSP.h:5-12`), no realtime path | 0 | none | n/a |

## Aggregation map
- **EffectRack::getTotalLatencySamples** (`EffectRack.cpp:656-664`): sums active, non-slot-bypassed slots; whole-rack bypass → 0.
- **VibeGraph::updateBusLatencies** (`VibeGraph.cpp:1951-2025`): sums rack+busEq for the **4 bus nodes only** (Layers/Bass/Drums/Master, `1961-1968`) + per-Vox engine-chain latency via `onGetVoxStripChainLatency` hook (`1975-1982`, wired `PluginProcessor.cpp:367-372` → `BaySickVocalProcessor::getChainLatencySamples`). Compensates via CompDelayLines (bus-level + Vox/Audio/Inst/Rusty insert-level, `1995-2019`); total = maxBusLat + masterLat → `totalLatencySamples`.
- **VibeSynthProcessor::setLatencySamples** refresh points: prepareToPlay (`PluginProcessor.cpp:375`), EffectsPage rack/EQ/panel changes (`EffectsPage.cpp:123,143,765,769,787,793,811,849`), StandaloneEditor vox bypass/engine switch (`StandaloneEditor.cpp:11798`).
- **End consumers**: transport LAT readout (`StandaloneEditor.cpp:917` → `GlobalTransportBar.h:118`), Builder playhead visual offset via `getTotalOutputLatency` = PDC + device latency (`PluginProcessor.h:366-368`, `BuilderPage.cpp:6383-6395`).
- **Vocal chain** (engine-side, compensated): Gate→DeReverb→DeEsser→Compressor→Saturation→Limiter (`BaySickVocalProcessor.cpp:344-355`) — so the vocal chain's Limiter default 2 ms lookahead + Sat OS also ride the PDC sum.

## Gaps found (uncompensated / unreported)
1. **Per-insert mixer racks are NOT in the PDC sum** — `rack.getTotalLatencySamples()` is only called for the 4 bus nodes + vocal engine chain. A Limiter (default ~96 smp) or spectral DeEsser (2048 smp) on a Layer/Bass/Drum/Audio/Aux/Vox-mixer-side/Inst/Rusty **insert** rack shifts that strip late with no compensation and no LAT-readout contribution.
2. **Effects bus / Clips bus / Vox bus node racks+EQs** also absent from `updateBusLatencies` (only `mLayersNode/mBassNode/mDrumsNode/mMasterNode` at `VibeGraph.cpp:1961-1968`).
3. **PitchCorrectorDSP ~48 ms** never enters PDC — only aligns the WET recording; the live-monitor corrected voice runs ~48 ms late by design.
4. **BaySickNAMIRProcessor** `setLatencySamples` (OS 2x/4x, ~4-8 smp) is written but never read by any aggregator.

---

## Report 2 — PDC consumption coverage map

# PDC Consumption Coverage Map — BaySickDAW

Files: `C:\Users\jeffm\Documents\BaySickDAW\Source\VibeGraph.cpp` (VG), `VibeGraph.h` (VGh), `PluginProcessor.cpp` (PP), plus Engine task layer under `Source\Engine\` (the old `routeInsertOutput` lambda **no longer exists** — replaced by the MT pull-model tasks).

## 0. The one compensation computer

`VibeGraph::updateBusLatencies` — VG:1951-2025. Callers: PP:375 (prepareToPlay), EffectsPage.cpp:123, 143, 765, 769, 787, 793, 811, 849 (slot change / bypass / onLatencyChanged), StandaloneEditor.cpp:11798. Message-thread only (setDelay allocates, VGh:420).

**Measured inputs (the ONLY latencies ever read):**
- `mLayersNode->rack.getTotalLatencySamples() + busEq.getLatencySamples()` VG:1961-1962; Bass VG:1963-1964; Drums VG:1965-1966; Master VG:1967-1968. **preEq omitted on all four** (preEq is processed at VG:408/581/739/877 and is a full EQ8MsDSP that can report linear-phase/AC latency per EQ8MsDSP.h:70-72 — never summed).
- Per-Vox-strip ENGINE chain latency via `onGetVoxStripChainLatency` VG:1975-1982, wired PP:367-372 → `BaySickVocalProcessor::getChainLatencySamples` (BaySickVocalProcessor.h:155-158 = engine-side `mVocalChainRack.getTotalLatencySamples()` only; the realtime pitch corrector's latency is NOT in it — that is compensated only at the wet recorder via `mWetLatencySkip`, BaySickVocalProcessor.cpp:684-687).

**Delays written:** `maxBusLat = max(layers, bass, drums, voxChainMax)` VG:1987-1988 (masterLat excluded — terminal). L/B/D bus compDelay = maxBusLat − ownLat VG:1995-1997. Insert compDelays VG:2007-2019: Vox insert = maxBusLat − voxChainLat[i]; Audio/Inst/Rusty insert = maxBusLat; Layer/Bass/Drum insert deliberately skipped (bus delay covers the path); Aux deliberately skipped ("sends tapped post-compensation" VG:2003-2005). Host report: total = maxBusLat + masterLat → `totalLatencySamples` VG:2022-2023 → `setLatencySamples` PP:375. Cap: CompDelayLine kMaxSamples = 8192 (~186 ms @44.1k), silently clamped VG:242, 246.

## 1-2. Coverage matrix

Every rack/EQ instance in the graph. "Measured" = its getLatencySamples enters updateBusLatencies' math. "Delay line" = a CompDelayLine exists on that node. "Compensated" = a nonzero-capable delay is actually programmed for it.

| Instance (DSP it owns) | Latency potential | Measured? | Delay line? | Compensated? | Notes / where latency would need to enter |
|---|---|---|---|---|---|
| LayersBusNode preEq+rack+busEq (VG:283-286) | yes | rack+busEq yes (VG:1961); **preEq NO** | yes (VG:286, runs VG:461) | yes (VG:1995) | add preEq.getLatencySamples() to layersLat |
| BassBusNode (VG:480-483) | yes | rack+busEq yes; preEq NO | yes (VG:625) | yes (VG:1996) | same |
| DrumsBusNode (VG:642-645) | yes | rack+busEq yes; preEq NO | yes (VG:783) | yes (VG:1997) | same |
| MasterBusNode preEq+rack+busEq (VG:802-804) | yes | rack+busEq yes (VG:1967); preEq NO | **no** (terminal — none needed) | n/a; reported to host only (VG:2022) | preEq missing understates the reported total |
| EffectsBusNode / FX bus preEq+rack+busEq (VG:956-958) — **yes it has all three** | yes | **NO** | **NO** (struct VG:954-1090 has no compDelay) | **NO** | a latent FX-bus rack delays every return re-entering Master (MasterTask.cpp:43-56) vs the dry paths → wet/dry misalignment on parallel sends; needs its lat folded into master-side alignment (delay all other Master predecessors) |
| AudioClipsBus InstrChannelNode preEq+rack+eq (VG:1312-1314, processed VG:1737-1754) | yes | NO | NO (InstrChannelNode VG:1309-1376 has no compDelay) | NO | bus input IS aligned (audio inserts delayed to maxBusLat), but the bus's own chain latency shifts Clips vs L/B/D at the master sum |
| VoxBus, InstBus, VoxBus2, InstBus2, InstBus3, RustyDrumsBus (same InstrChannelNode shape, VG:1409-1431) | yes | NO | NO | NO | same as ClipsBus |
| Layer inserts 0-15 preEq+rack+eq (InsertNode VG:1121-1124) | yes | **NO** (never queried) | yes (VG:1124, runs VG:1290) | delay exists but set to 0 (VG:2007-2019 skips L/B/D) | own rack lat must join the bus sum + per-insert differential (maxInsertLat − ownLat) |
| Bass inserts 0-15 | yes | NO | yes | 0 by design | same |
| Drum inserts 0-15 | yes | NO | yes | 0 by design | same |
| Audio inserts 0-49 | yes | NO (own chain) | yes | cross-path only (= maxBusLat, VG:2014-2017) | own lat should become want = maxBusLat − ownLat and feed maxBusLat |
| Aux inserts 0-17 | yes | NO | yes | 0 by design (VG:2003-2005 — input aligned, correct) | aux's own rack lat then delays its FX-bus contribution — uncovered |
| Vox inserts 0-5 (strip-side mixer rack/EQ, distinct from engine chain) | yes | engine chain yes (hook); **strip rack/preEq/eq NO** | yes | partial (= maxBusLat − voxChainLat, VG:2012-2013) | strip-side rack lat should join voxChainLat |
| Inst inserts 0-19 | yes | NO; Inst ENGINE internals (NAMIR etc.) also never queried — assumed zero (VG:2001-2002) | yes | cross-path only (= maxBusLat) | needs an Inst analog of onGetVoxStripChainLatency if engine ever reports latency |
| Rusty inserts 0-12 | yes | NO | yes | cross-path only | same as Audio |
| Legacy mLayerPageRacks/mBassPageRacks (VGh:768-769) | n/a | NO | NO | n/a | UI-only fallback pre-InsertNode; not processed by the MT render path |
| Engine-side BaySickVocal mVocalChainRack | yes | **YES** (the QA-Fe2 hook) | via Vox insert compDelay subtract | yes | pitch-corrector latency excluded from the hook |

Bottom line: of ~200 potential rack/EQ instances, exactly **4 rack+postEq pairs (L/B/D/Master) + 6 Vox engine chains** are measured; everything else is invisible to the math, and the FX bus + 7 InstrChannelNode buses can't be compensated at all (no delay line).

## 3. Send timing

- `routeInsertOutput` is gone. RoutingGraph edges (built VG:2895-2914) become UpstreamLinks (RenderGraphDispatcher.cpp:154-172); consumers PULL the source task's `mOutputBuffer`: PassiveStripTask.cpp:36-49 (buses+aux), MasterTask.cpp:43-56. That buffer is written in-place by processInsert/processBus, so **every tap (main-out, send, SC) is post-fader, post-pan, AND post-compDelay** — InsertNode chain order: preEq VG:1230 → polarity 1233 → width 1238 → rack 1259 → eq 1262 → fader 1268-1281 → pan 1287 → **compDelay 1290** → meters.
- **Pre-fader sends are a stored no-op**: `_send{N}_prepost` registered PP:5294, read into Edge.prePost VG:2911, copied to link.prePost RenderGraphDispatcher.cpp:164 — no consumer ever reads it (only 3 hits, all assignments; UpstreamLink.h:28). All sends are post-fader in audio.
- FX-bus returns re-enter Master as an ordinary main-out edge (defaultSendTo kFxBus→kMaster, VGh:174-181), summed in MasterTask alongside the dry buses. **Yes — a latent FX-bus rack misaligns returns vs dry** (see matrix row); nothing measures or delays around it. Aux inputs are aligned (post-comp taps), so only aux/FX-bus own-chain latency breaks the return path.

## 4. Sidechain taps

- SC receive buffers filled from the source task's `mOutputBuffer` (SidechainPullHelper.h:30-51; engine-level push EngineInsertTask.cpp:73-84, VoxStripTask.cpp:203-214, CompositeAudioInsertTask.cpp:81-92) → SC keys are **post-compDelay on the source**.
- The consumer reads SC inside preEq/rack/postEq (pushScArrayToStrip VG:3071-3115), which runs **before the consumer's own compDelay** (rack VG:1259/419 vs compDelay VG:1290/461). So compensation DOES skew keying whenever maxBusLat > 0: a delayed source (Audio/Inst/Rusty/Vox insert at maxBusLat) keying an un-delayed chain point makes the key LAG by up to maxBusLat; an un-delayed source (Layer/Bass/Drum insert) keying Master/bus post-comp content makes the key LEAD by up to maxBusLat. With no latent FX loaded (maxBusLat = 0) there is no skew. Cross-order SC additionally gets a deliberate +1-block latency (clearScRecvBuffers policy VG:3034-3062).

## 5. Metronome and master recorder

- Dispatch copies Master's arena slot to the host buffer (RenderGraphDispatcher.cpp:317-327), then `applyPostMixRecordAndMetro` (called PP:2644, defined PP:2669): MIDI recorder PP:2677-2698 (transport clock) → master recorder `writeBlock` PP:2719-2720 (pre-metronome) → metronome synthesized PP:2722-2895 **added directly into the final buffer at transport-clock positions with zero delay**. The compensated mix arrives maxBusLat + masterLat late relative to transport, so the click LEADS the music by the full PDC amount whenever any latent FX is loaded — the metronome path consumes no compensation.
- Master recorder captures the delayed (post-master-chain) audio with no PDC trim (the PP:2711-2712 pre-roll shift is count-in only) → recorded WAV content is late vs its own MIDI/beat grid by total PDC.
- Strip dry recorders tap RAW pre-chain input (tapDryRecorder PP:4190, called VoxStripTask.cpp:161-164 / InstStripTask.cpp:206) — pre-everything, PDC-independent. The Vox wet recorder compensates only pitch-corrector latency engine-side (BaySickVocalProcessor.cpp:684-687).
