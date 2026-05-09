# DAW Architecture Research — 2026-05-08

> **Five focused sweeps**, all WebFetch-verified where vendor pages were reachable, auto-demoted to MEDIUM where bot-blocking forced reliance on third-party sources.

> **Distinct from `competitive-research-2026-05-08.md`** (user-facing features for Future State) — this report is **engineering-notes shape**: comparative analysis with implementation sketches, intended for architectural decision-making. Some findings will graduate to Future State as `[CL-XXX / PE]` entries; others are pure engineering reference; one (GPU offload) is a "stop investing in this direction" call.

> **Method:** dispatched 5 `daw-architecture-research` subagent runs in parallel. One stalled on a 10 MB PDF content-length limit and was killed + replaced with a tighter narrow sweep on its uncovered sub-topic (sample streaming).

---

## Index

1. [GPU offload for FFT-based effects](#1-gpu-offload-for-fft-based-effects-strong-negative) — strong negative
2. [FFT plan caching for many EQ8 instances](#2-fft-plan-caching-for-many-eq8-instances) — strong recommendation
3. [Lock-free MIDI dispatch UI → audio](#3-lock-free-midi-dispatch-ui--audio) — current pattern correct + clear upgrade path
4. [Voice management priorities under voice-stealing](#4-voice-management-priorities-under-voice-stealing) — concrete tier-hierarchy upgrade
5. [Sample streaming](#5-sample-streaming) — three real gaps with sketched fixes

## Cross-sweep summary

| Topic | Recommendation | Effort | Strategic |
|-------|----------------|--------|-----------|
| GPU offload | **Drop CL-049** OR move to "Tech not yet feasible" | n/a | Stop investing — evidence says not worth it for our FFT sizes |
| FFT plan cache | Add `FftPlanCache` keyed by order, owned by `VibeGraph`. Concrete 7-file diff. | Small | Forward-compatible with MT engine + future PFFFT/FFTW swap |
| MIDI dispatch | **Keep current single-slot atomic pattern.** Fold three audition atomics into one POD ring (`juce::AbstractFifo` or `farbot::fifo`) only when arpeggio / chord-strum / paste-to-roll needs hit. | Small (when needed) | Defer until use case appears |
| Voice mgmt | Add envelope-stage tier hierarchy at `VibePlayerDSP.cpp:944-960`; mirror to BaySickSynth/Bass; soft-stop instead of hard-kill; expose user-visible mode (Last/Highest/Lowest). | Medium | Phase 5F follow-up; CL-053 holds for cross-engine pool |
| Sample streaming | Add `SamplePool` with refcount + preload-head + decode-once cache for MP3; sampler engines (VibeSampleManager + Phase D drums) consume the pool too. | Large | Unlocks Kontakt-scale libraries |

---

# 1. GPU offload for FFT-based effects — STRONG NEGATIVE

**Headline:** drop `CL-049` from the active roadmap or demote to "Tech not yet feasible." Evidence converges: GPU offload only pays off for FFT sizes ≥ ~16K-point complex (64 KiB), batched with amortizable transfer cost, tolerating 2-4× buffer-size additional latency. **BaySickDAW's FFT use is at 2048** (PhaseVocoder, Harmless wavetable, EQ8 linear-phase). All below the GPU break-even threshold by an order of magnitude.

## Sources fetched

| Source | URL | Confidence |
|--------|-----|------------|
| GPU Audio Inc. SDK readme | https://github.com/gpuaudio/gpuaudio-sdk | HIGH |
| GPU Impulse Reverb VST | https://gpuimpulsereverb.de/ | HIGH |
| KVR FIR Convolver reviews | https://www.kvraudio.com/product/fir-convolution-reverb-by-gpu-audio | HIGH |
| VkFFT HN thread + benchmarks | https://news.ycombinator.com/item?id=24610128 | HIGH |
| Sound on Sound: DSP-assisted plugins latency | https://www.soundonsound.com/techniques/dsp-assisted-audio-effects-latency | HIGH |
| GPU Audio Attack Magazine demo article | https://www.attackmagazine.com/features/long-read/gpu-audio-is-powering-plugins-with-your-graphics-card-... | HIGH |
| u-he developer KVR critique | https://www.kvraudio.com/forum/viewtopic.php?t=618321 | HIGH |
| JUCE forum critique | https://forum.juce.com/t/gpu-audio-sdk-is-out/65562 | HIGH |
| VkFFT scope/benchmarks | https://github.com/DTolm/VkFFT | HIGH |
| BedroomProducersBlog FIR Convolver review | https://bedroomproducersblog.com/2022/03/21/gpu-audio-fir-convolver/ | HIGH |

## Key findings

- **GPU Audio Inc.** is the major commercial player. KVR user reviews of their FIR Convolver: ">25% CPU usage despite GPU claims, audio dropouts after a few seconds, plugin crashing both DAW and audio device, not ready for release."
- **No mainstream DAW** ships GPU-FFT integration. Cubase / Pro Tools / Studio One / FL Studio all stay CPU-bound.
- **PCIe round-trip is fast** (~5 µs for one audio block on PCIe 3.0) — that's not the bottleneck.
- **Kernel launch overhead IS the bottleneck.** GPU Audio's own SDK readme cites "100-200 µs execution windows, 5-10 launches per ms." At 96 kHz / 96-sample buffer = 1 ms total budget, that's **20% of the audio budget consumed by scheduling overhead** before any FFT runs.
- **cuFFT/VkFFT lose to FFTW** below ~16K-point. NVIDIA's own forum: "CUFFT is not good for small sized FFTs." cuFFTW returns ~20-35 µs per 1024-2048 single-FFT call. CPU FFTW with AVX2 finishes in single-digit microseconds.
- **2× buffer-size latency tax per insert** is the universal rule for hardware-offloaded audio, established by Sound on Sound for UAD-1 and inherited by GPU offload.
- **u-he developer's architectural critique:** "GPUs are designed for huge amounts of parallel data. Audio has maybe 100 parallel streams, not millions, and it requires a few hundred times the rate of video."

## Recommendation

**Drop `CL-049` from Future State** OR move to a "Tech not yet feasible" sub-section of Considered & Dropped, with the evidence preserved so it can graduate back if the SDK / hardware / driver landscape changes.

The justifying workload (very long convolution reverb, 10+ second IRs ≈ 440K-point FFT) is the only one where GPU math actually flips. Even there, integration risk is high (driver crashes, DAW destabilization, AMD/NVIDIA compatibility skews). For BaySickDAW's actual workloads, **prefer partitioned-block convolution on CPU** (HiFi-LoFi/FFTConvolver-style) over a GPU bet.

---

# 2. FFT plan caching for many EQ8 instances

**Headline:** at 50 mixer strips × 2 FFTs each (mid + side), worst case after BLU-268..271 lands is **100 `juce::dsp::FFT` instances each with its own twiddle-factor table = 3.2 MB of redundant tables at FFT 2048, scaling to 12.8 MB worst-case at 4096 with multi-IR linear-phase.** Construction-on-first-touch causes a visible stall when adding aux strips mid-session.

**Recommendation:** add an `FftPlanCache` keyed by FFT order, owned by `VibeGraph`. JUCE's `dsp::FFT::perform()` is `const noexcept` — sharing FFT instances is safe IF scratch buffers stay per-instance. Standard pattern across PFFFT (explicit `Setup` thread-safe) + FFTW (new-array execute) + JUCE (const contract).

## Sources fetched

| Source | URL | Confidence |
|--------|-----|------------|
| JUCE dsp::FFT class docs | https://docs.juce.com/master/classdsp_1_1FFT.html | HIGH |
| JUCE FFT source | https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/frequency/juce_FFT.cpp | HIGH |
| FFTW Thread Safety | https://www.fftw.org/fftw3_doc/Thread-safety.html | HIGH |
| FFTW Wisdom and Saved Plans | https://www.fftw.org/fftw3_doc/Words-of-Wisdom_002dSaving-Plans.html | HIGH |
| PFFFT header (jpommier upstream) | https://bitbucket.org/jpommier/pffft/raw/master/pffft.h | HIGH |
| PFFFT GitHub fork (marton78) | https://github.com/marton78/pffft | MEDIUM (README-only benchmark claims) |
| FabFilter Pro-Q processing modes | https://www.fabfilter.com/help/pro-q/using/processingmode | MEDIUM (vendor marketing — internal sharing strategy proprietary) |
| FabFilter Pro-Q FFT CPU forum thread | https://www.fabfilter.com/forum/topic/1221/pro-q-fft-using-much-cpu | MEDIUM |
| JUCE forum: multiple realtime threads | https://forum.juce.com/t/multiple-realtime-threads/54323 | MEDIUM |

## Recommended implementation

```
Source/DSP/FftPlanCache.h   (new, header-only ~50 lines)
```

Public surface:
```cpp
std::shared_ptr<const juce::dsp::FFT> get(int order);   // O(1) on hit; locked construction on miss
size_t size() const;
void   clear();                                          // ~VibeGraph only
```

`EqLinearPhaseProcessor` swaps `std::unique_ptr<juce::dsp::FFT>` for `std::shared_ptr<const juce::dsp::FFT>` from the cache. `mFftScratch` (per-instance) stays unchanged. 7 files touched, all mechanical (one ctor parameter or one setter call). No audio-thread code changes — `perform()` call site unchanged because JUCE's `perform()` is `const noexcept`.

**Memory cost at BLU-270 (variable FFT size 256..8192):** at most 6 orders × ~32 KB = ~192 KB total, vs unmanaged 12.8 MB worst case. **Construction stall fix:** O(1) hit on existing orders eliminates the visible-glitch on aux-add.

## MT engine wrinkle

Once QA-Md ships and the MT path engages, two strips might call `perform()` on the same shared FFT simultaneously. The const-noexcept contract probably holds (FFTW + PFFFT explicitly publish this for new-array execute / shared-Setup), but should be verified under `tsan` before flipping MT on in Debug. Fallback if tsan complains: per-thread-pool FFT instances (cache key becomes `(order, threadIndex % poolSize)`).

## Future State implications

- **Add as `[CL-XXX / PE]`** in Cross-cutting Infrastructure bucket — engineering improvement, not a user-facing feature
- Compatible with BLU-268..271 (multi-IR linear-phase work in plan)
- Forward-compatible with future swap to PFFFT or FFTW if benchmarks warrant later

---

# 3. Lock-free MIDI dispatch UI → audio

**Headline:** **BaySickDAW's current single-slot atomic pattern is correct and idiomatic.** Same lock-free safety floor as moodycamel / farbot, just with capacity 1. Don't tear it out for performance.

**Surprising baseline confirmation:** Vital and Surge XT both delegate UI→audio MIDI to the host's `MidiBuffer` in `processBlock` and put their queue investment on parameter automation, NOT note dispatch. So we're aligned with the industry baseline, not behind it.

**Critical counter-recommendation:** **DO NOT** pull in `juce::MidiMessageCollector` — its source has `CriticalSection midiCallbackLock`, confirmed mutex-based. The whole reason BaySickDAW's atomic pattern exists is to avoid that mutex.

## Sources fetched

| Source | URL | Confidence |
|--------|-----|------------|
| JUCE forum: lock-free MIDI collector | https://forum.juce.com/t/is-there-a-lock-free-midi-collector/45591 | HIGH |
| JUCE forum: AbstractFifo SPSC | https://forum.juce.com/t/abstractfifo-single-consumer-single-producer-thread-safety/50749 | HIGH |
| JUCE forum: MidiMessage lock-free | https://forum.juce.com/t/midimessage-lock-free/30484 | HIGH |
| JUCE AbstractFifo source | https://raw.githubusercontent.com/juce-framework/JUCE/master/modules/juce_core/containers/juce_AbstractFifo.h | HIGH |
| JUCE MidiMessageCollector source | https://raw.githubusercontent.com/juce-framework/JUCE/master/modules/juce_audio_devices/midi_io/juce_MidiMessageCollector.h | HIGH |
| Timur Doumler: locks safely | https://timur.audio/using-locks-in-real-time-audio-processing-safely | HIGH |
| moodycamel: SPSC ring blog | https://moodycamel.com/blog/2013/a-fast-lock-free-queue-for-c++ | HIGH |
| moodycamel/readerwriterqueue | https://github.com/cameron314/readerwriterqueue | HIGH |
| farbot README | https://raw.githubusercontent.com/hogliux/farbot/master/README.md | HIGH |
| Vital sound_engine.cpp | https://raw.githubusercontent.com/mtytel/vital/main/src/synthesis/synth_engine/sound_engine.cpp | HIGH |
| Surge XT SurgeSynthesizer.cpp | https://raw.githubusercontent.com/surge-synthesizer/surge/main/src/common/SurgeSynthesizer.cpp | HIGH |

## Where upgrade IS justified

When (not if) these needs hit:

1. **Multi-event bursts** — chord audition, paste-to-roll, arpeggio-from-UI-clock, MIDI Learn rapid CC bursts
2. **Distinct event kinds packed without bespoke per-kind atomics** — currently 3 atomics per engine (`mAuditionNote` / `mAuditionHoldOn` / `mAuditionHoldOff`)
3. **Backpressure signal** when overflow happens

## Two pluggable upgrade options

| Option | Pros | Cons |
|--------|------|------|
| **In-tree** — `juce::AbstractFifo`-backed POD MIDI ring, capacity ~64-128, per-engine | No new vendored library | Strictly SPSC; no multi-producer support |
| **Vendored** — `farbot::fifo<MidiEvent, single + overwrite_or_return_default>` | Header-style, permissive license, "overwrite_or_return_default" mode = multi-element version of current atomic-slot semantic | Adds vendored library |

Fabian Renn-Giles (Meeting C++ 2019) authored farbot specifically for this UI-to-audio plumbing pattern. Closest existing answer to "do any vendors expose this as a separately-pluggable component."

## Future State implications

- **No immediate action** — current pattern is correct. Don't proactively rip out and replace.
- **`[CL-XXX / PE]` future entry** when arpeggio / paste-to-roll / chord-strum needs hit; flag farbot vs in-tree fifo as the spec call at that time.

---

# 4. Voice management priorities under voice-stealing

**Headline:** BaySickDAW's BLU-353 (same-pitch preempt + note-off strip) is **already correct** — matches Surge "Reuse Single" and Massive X "Reassign." The real gap is at `Source/VibePlayer/VibePlayerDSP.cpp:944-960` where the voiceCap loop only does oldest-active stealing. Industry consensus is **envelope-stage priority hierarchy**: Off > Release > Sustain > Decay > Attack with age tie-break.

## Sources fetched

| Source | URL | Confidence |
|--------|-----|------------|
| JUCE Synthesiser.cpp | https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_basics/synthesisers/juce_Synthesiser.cpp | HIGH |
| JUCE Synthesiser.h | https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_basics/synthesisers/juce_Synthesiser.h | HIGH |
| JUCE forum: override findVoiceToSteal | https://forum.juce.com/t/how-to-override-juce-findvoicetosteal-to-stop-protecting-top-and-bottom-voice/31615 | HIGH |
| Vital voice_handler.cpp | https://github.com/mtytel/vital/blob/main/src/synthesis/framework/voice_handler.cpp | HIGH |
| Surge XT manual | https://surge-synthesizer.github.io/manual-xt/ | HIGH |
| Surge architecture doc | https://github.com/surge-synthesizer/surge/blob/main/doc/Surge%20Architecture.md | HIGH |
| Massive X Voice page | https://native-instruments.com/ni-tech-manuals/massive-x-manual/en/voice-page | HIGH |
| Pianoteq forum on auto-polyphony | https://forum.modartt.com/viewtopic.php?id=10215 | HIGH |
| RNBO voice-stealing modes | https://rnbo.cycling74.com/learn/voice-stealing-modes | HIGH |
| RNBO polyphony tutorial | https://rnbo.cycling74.com/learn/rnbo-polyphony | HIGH |
| Cherry Audio Dreamsynth Assign | https://docs.cherryaudio.com/cherry-audio/instruments/dreamsynth/assign | HIGH |
| JUCE forum: voice steal pops | https://forum.juce.com/t/voice-steal-pops/30923 | MEDIUM |
| KVR voice stealing thread | https://www.kvraudio.com/forum/viewtopic.php?t=155873 | MEDIUM |

## Tier-hierarchy (industry consensus)

| Tier | Kill priority | What |
|------|---------------|------|
| 1 | Kill first | Voices in release phase. Oldest releasing wins. |
| 2 | Kill next | Voices whose key is up but envelope hasn't entered release (sustained / sostenuto). Oldest wins. |
| 3 | Apply user mode | Voices still held with key down. User-visible mode (Last / High / Low / Legacy). |
| 4 | Last resort | The highest and lowest currently-held pitch. JUCE protects these by default; some products expose toggle. |

## Concrete recommendations for BaySickDAW

1. **Add Tier 1/2 envelope-stage awareness to VibePlayer voiceCap loop** at `Source/VibePlayer/VibePlayerDSP.cpp:944-960`. Today: oldest-active only. Replace with: scan released voices first, tie-break by age. Fallback to oldest when no released voice exists.

2. **Mirror the algorithm to BaySickSynth/Bass/Harmless.** None currently have explicit voice-cap stealing logic; they rely on `juce::Synthesiser` default for Harmless or per-pitch preempt for the others. Either expose a `voiceCap` parameter analogous to VibePlayer, or override `findVoiceToSteal` on the JUCE side.

3. **Soft-stop instead of hard-stop on cap eviction.** `Source/VibePlayer/VibePlayerDSP.cpp:960` calls `oldest->stopNote(0.f, false)` — `false` is hard-kill. Change to `true` (allow tail-off) and rely on ADSR release. Force a short release override on stolen voices if natural release is multi-second (Vesa/Oval method).

4. **Expose user-visible mode as APVTS enum** matching Surge / Cherry vocabulary: Last / Highest / Lowest. Keep underlying engine-stage priority (Tier 1/2/3) as internal default; only Tier-3 disambiguation is user-facing.

5. **Per-engine cap default:** Surge XT 64, Massive X 1-64, Pianoteq 256. VibePlayer's current 1-16 default 16 is conservative — fine for beginner-audience DAW but worth flagging as something to revisit if users hit caps in practice.

6. **Cross-engine voice pool (CL-053)** — none of the searched literature directly addresses cross-engine voice budgets in a multi-instrument DAW. Wwise game-audio domain has the closest analog (per-bus voice budgets), but that's a different problem space. **Treat as Future-State R&D, not validated in commercial synth literature.** Hold CL-053; don't promote until per-engine work proves itself.

## Click prevention

Industry standard: **3-10 ms linear fade or DC ramp** on the stolen voice's last sample. BaySickDAW already has 1 ms declick fade-in on every `startNote` (handles new-voice-side click). Stolen-voice-side click needs a parallel fade-out (option 1 above) OR a benchwarmer (1-2 preallocated extra voices per engine for parallel fade-out).

**Pianoteq's CPU-aware auto-polyphony** (Optimistic 75% / Pessimistic 50% targets) is interesting — could fold into CL-054 (per-engine CPU budgets) when that batch surfaces.

## Future State implications

- **Add as `[CL-XXX / AQ]`** under Players bucket — voice-stealing tier hierarchy + soft-stop + user-visible mode. Concrete; mid-sized batch.
- **Document BLU-353 under CL-053** so the next maintainer doesn't undo it.

---

# 5. Sample streaming

**Headline:** BaySickDAW's `AudioClipStreamer` is structurally sound (SPSC ring + atomic seek + 100 MB RAM threshold) — don't rewrite. But it's per-clip with no sharing, and the sampler engines (VibeSampleManager + Phase D drum engines) bypass streaming entirely, which caps us at small SFZ libraries. Three concrete gaps with sketched fixes.

## Sources fetched

| Source | URL | Confidence |
|--------|-----|------------|
| sfizz `FilePool` source | local at `libs/sfizz/src/sfizz/FilePool.{h,cpp}` | HIGH |
| JUCE `BufferingAudioSource` source | local at `juce/modules/juce_audio_basics/sources/juce_BufferingAudioSource.cpp` | HIGH |
| DrumGizmo wiki disk streaming | https://drumgizmo.org/wiki/doku.php?id=dev:disk_streaming | HIGH |
| Logic Pro EXS24 / Sampler streaming | https://help.apple.com/logicpro/mac/9.1.6/en/logicpro/instruments/chapter_12_section_25.html | HIGH |
| Spitfire Audio: SSD + RAM guidance | https://support.spitfireaudio.com/en/articles/11815486-reducing-ram-usage-when-using-ssds | HIGH |
| HISE StreamingSampler docs | https://docs.hise.dev/hise-modules/sound-generators/list/streamingsampler.html | MEDIUM |
| Tracktion Engine Discussion #146 | https://github.com/Tracktion/tracktion_engine/discussions/146 | MEDIUM |
| Ardour preferences manual | https://manual.ardour.org/preferences-and-session-properties/preferences-dialog/ | HIGH |
| Kontakt DFD third-party tutorials | https://www.adsrsounds.com/kontakt-tutorials/how-to-use-and-optimize-kontakt-dfd/ | MEDIUM |
| Kontakt Memory Server (KMS) | https://aeonata.com/tutorial-how-to-optimize-kontakt-5s-ram-usage/ | MEDIUM |

## Comparative table

| Property | sfizz FilePool | JUCE BufferingAudioSource | HISE | Kontakt DFD | BaySickDAW today |
|---|---|---|---|---|---|
| Tier model | Preload + stream | Single ring | Preload + dual-buffer per voice | Preload + stream | RAM-fast OR single ring |
| RAM-load knob | Per-instance bool | None | `PreloadSize = -1` | Implicit | **Auto, 100 MB byte-size threshold** |
| Voice-share decoded | **Yes** (FileDataHolder refcount) | No | Yes (singleton preload) | **Yes** (KMS process-wide) | **No** |
| Bg thread | ThreadPool + dispatcher | TimeSliceThread | Dedicated | Dedicated | TimeSliceThread |
| Lock primitive | atomic_queue + RTSemaphore | CriticalSection | unknown | unknown | CriticalSection (reader only) |
| Seek dropout | Not supported (one-shot) | Silent silence + moveToFront | Auto-RAM-load fallback | Silent silence | Silent silence |
| Trigger-burst (50 voices same sample) | Single shared preload covers cold start | N/A | Per-voice buffer pair | Single shared preload | Each clip private — duplication |

## Where BaySickDAW is already strong

- SPSC lock-free ring + atomic seek protocol matches state-of-the-art.
- 100 MB RAM threshold is **aggressive** vs Kontakt defaults (60 KB per sample); most short-form audio (drum hits, loops, vocal phrases) ends up RAM-resident — clear win for cold-start sputter on common audio.

## Real gaps (with sketched fixes)

### Gap 1 — No shared-pool / refcount across voices
Today every clip has its own `AudioClipStreamer`. Same WAV used 8 times = 8× RAM + 8× disk reads.

**Fix:** process-wide `SamplePool` keyed by canonical file path. Each entry holds preloaded head + optional fully-decoded buffer + atomic refcount. Reuse across `AudioClipStreamer` + drum pads + sampler voices.

```cpp
class SamplePool {
public:
    struct Entry {
        juce::AudioBuffer<float> preloadHead;      // first ~256 KB always in RAM
        juce::AudioBuffer<float> fullPCM;          // populated if file ≤ 100 MB
        std::atomic<int> readerCount { 0 };
        std::atomic<int64_t> lastReleasedNs { 0 };
        bool fullyLoaded { false };
        // ... AudioFormatReader for streaming the rest
    };

    using Handle = std::shared_ptr<Entry>;
    Handle acquire (const juce::File& f);          // message thread; lazy-creates
};
```

`AudioClipStreamer` consumes a `Handle` instead of owning a private `AudioFormatReader`. Ring buffer + bg-thread logic stays unchanged. New code path: preload-head fast path before ring becomes ready.

GC lives on a separate `juce::Thread` polling `lastReleasedNs > NOW - 5s` (sfizz pattern), not in audio thread.

### Gap 2 — No "preload head" for streamed clips
When >100 MB falls back to streaming, pays full 3.5s synchronous pre-fill on first play.

**Fix:** unconditionally keep first ~256 KB in RAM. Audio thread reads from head buffer until streaming ring catches up. Eliminates cold-start sputter on big files.

### Gap 3 — No streaming layer for sampler engines
`VibeSampleManager` (`Source/VibePlayer/VibePlayerDSP.h`) + Phase D drum engines load entire files into RAM. A 200 MB SFZ kit costs 200 MB even if 90% never trigger.

**Fix:** Wire `VibeSampleManager`, drum-engine sample loaders, BaySickPlayer through `SamplePool::getOrLoadHead(path)`. Voice-trigger code reads preload immediately; if note holds past preload end, streamer kicks in. **This is what unlocks loading a real Kontakt-scale (multi-GB) library.**

### Gap 4 — MP3 brute-force workaround
G-7 polish raised RAM threshold to 100 MB to sidestep slow MP3 decode. More elegant: decode-to-PCM-once at clip-add time on message thread; cache result (in-memory or `.bsd-cache` file). Use PCM cache for all subsequent playback.

### Gap 5 — No telemetry
EXS24 surfaces "data not read from disk in time" as user counter. BaySickDAW silently drops to silence; debuggability poor.

**Fix:** atomic underrun counter + atomic peak-prefill-latency-ms gauge, surfaced on debug overlay. `jassertfalse` in Debug builds when underrun detected, so dialog appears instead of silent dropout.

## Future State implications

- **`[CL-XXX / PE]` SamplePool with refcount** — Cross-cutting Infrastructure bucket. Large effort but high impact (unlocks SFZ-scale libraries).
- **`[CL-XXX / PE]` Preload-head for streamed clips** — Cross-cutting. Small effort.
- **`[CL-XXX / PE]` MP3 decode-once cache** — Cross-cutting. Replaces FSW-121 (RAM-load <15MB clips); is a more elegant solution to the same problem.
- **`[CL-XXX / WP]` Streaming telemetry** — Cross-cutting. Cheap; big debuggability win.

---

# Aggregate Future State entry candidates

These are the architecture findings that should graduate to `Future State.md` as `[CL-XXX]` entries when we apply. Confidence ratings carry over from individual sweep methodology sections.

| ID-to-be | Tag | Title | Bucket | Source sweep |
|----------|-----|-------|--------|--------------|
| TBD | PE | FftPlanCache keyed by order, owned by VibeGraph | Cross-cutting Infrastructure | FFT cache |
| TBD | AQ | Voice-stealing tier hierarchy (Off > Release > Sustain > Decay > Attack) | Players | Voice mgmt |
| TBD | AQ | Soft-stop on voiceCap eviction (3-10 ms fade or benchwarmer) | Players | Voice mgmt |
| TBD | AQ | User-visible voice priority mode (Last / Highest / Lowest) | Players | Voice mgmt |
| TBD | PE | SamplePool with refcount + preload-head | Cross-cutting Infrastructure | Streaming |
| TBD | PE | Preload-head for streamed clips (256 KB always-in-RAM) | Cross-cutting Infrastructure | Streaming |
| TBD | PE | MP3 decode-once cache (replaces FSW-121) | Cross-cutting Infrastructure | Streaming |
| TBD | WP | Streaming telemetry (underrun counter + prefill-latency gauge) | Cross-cutting Infrastructure | Streaming |
| TBD | PE | Sampler engines consume SamplePool (VibeSampleManager + Phase D drums) | Cross-cutting Infrastructure | Streaming |

**Drop / move to "Tech not yet feasible":**
- CL-049 (GPU offload for FFT-based effects)

**No new entry needed (validation of current state):**
- BLU-353 same-pitch preempt — keep, document under CL-053
- Single-slot atomic for MIDI dispatch — keep, defer upgrade until use case appears

---

**End of architecture report.** When applying to Future State, pull these candidate entries together with the v2 competitive sweep entries from `competitive-research-2026-05-08.md`. CL-NNN renumbering happens at apply-time per the standard pattern.
