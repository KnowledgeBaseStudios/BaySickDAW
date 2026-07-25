# Running Notes — QA-SlideSampler (silky-gliding-lynx)

> **Purpose:** append-only running log for the blended multi-sample slide + native Bend batch.
> Append at EVERY checkpoint — a build gate cleared, a finding captured, a spec call asked/resolved,
> a scope pivot, a commit landed. At close, `/draft-doc batch-close` reads this as the primary input
> for the Implemented Work Log entry, drafted + HELD until the campaign pass (bulk-run R2).
>
> **Pair file:** [`Plans & Specs/Batch Plans/silky-gliding-lynx.md`](../Batch Plans/silky-gliding-lynx.md)
> **Conventions:** Main Plan §0 (three-doc system, Rules 1-9; Batch Plans + Running Notes layout).

## 2026-07-20 — Batch created (not yet started)

Split out of QA-SlideSliceGlide (wistful-sliding-otter) at its A-1 STOP: sfizz can't do per-note bend,
and the karoryfer guitar/bass patches ship only small up-only (guitar ~+3) / ±2 (bass) native bend, so
a real slide can't come from sfizz. Design (Option C hybrid: sfizz for normal notes + a custom
crossfading SlideSampler for the slide gesture) + feasibility were established in a workshop + spike on
2026-07-20. The decided design, locked spec calls (SL-1..SL-7), open sub-spec calls (SS-Q1..SS-Q5), and
tasks are in the plan file.

**Read at start:** the spike report
`Plans & Specs/Research Reports/daw-architecture-sample-based-continuous-pitch-slide-2026-07-20.md`
(kills the multi-sfizz-instance option; confirms the Kontakt/Ample crossfade-sustain recipe + the
at-or-below/bend-up-≤1-semi trick) and the QA-SlideSliceGlide running notes Task-6 section (the
capability checks + the actual patch bend-range analysis).

**Resume action:** read the plan file + spike report + QA-SlideSliceGlide Task-6 running notes, then
answer SS-Q1/SS-Q2 by reading the karoryfer map .sfz files, and start Task 1 (region extraction).

## 2026-07-21 — Session open — tree certified + Task 1 investigation (SS-Q1/SS-Q2 resolved), spec call A surfaced

**Pre-session:** read Main Plan sec 0 in full, the plan file, the spike report, the QA-SlideSliceGlide
Task-6 running notes, and this file. HEAD = d6abc38b (wistful never committed).

**Working tree certified — fully accounted for, nothing disturbed.** Every dirty/untracked entry maps to
a prior-session batch, none touch the guitar/bass processors or a SlideSampler:
- QA-OctavePedal G3 review-fixes (`locked-doubling-frog`): OctaveStyleDSP.h, BroadcastSynthesiser.h
  (QA-H glide-stash), MixerPage.*/MixerTrackStrip.h (QA-G automation re-register), RibbonTabBar.*
  (kit teardown), LayersPage.cpp/BassPage.cpp (QA-I busy overlay), UndoActions.h (QA-G row-state),
  StandaloneApp.*/StandaloneEditor.cpp (shared).
- QA-L-Fix drum-trigger (`eager-thumping-marmot`): CMakeLists.txt, DrumPage.*, DrumKitGrid.*,
  MidiLearn/* (incl. new DrumTriggerMap.*), StandaloneEditor.cpp, StandaloneApp.*, PluginProcessor.h.
- QA-SlideSliceGlide Tasks 1-5 (`wistful-sliding-otter`): PatternManager.*, PluginProcessor.cpp,
  PianoRoll.cpp, BuilderPage.*, BaySickSynthVoice.*, AdditiveVoice.*, VibePlayerDSP.*, Main Plan sec 9,
  v1-master-test-plan.md sec B.22.
- This batch's own docs + spike report. My targets (BaySickGuitarsProcessor, BaySickBassesProcessor, new
  SlideSampler) do not collide; PluginProcessor.cpp / PianoRoll.cpp / PatternManager.* are dirty from
  wistful and I stack on top.

**Library on disk:** `%LOCALAPPDATA%\BaySickDAW\CoreLibrary\` -> `Black&Green Guitars` (default program
`Programs/01-green_keyswitch.sfz`) + `Black&Blue Basses` (default `Programs/01-darkblack_keysw.sfz`).
loadKit already runs a depth-4 #include-walker (set_cc/label_cc/#define/#include) -> extend a parallel
walker to also collect <region> opcodes for the main sustain block.

**SS-Q1 (loops) — RESOLVED by reading the maps:** the main sustain articulation carries NO
loop_start/loop_end/loop_mode on either engine. Guitar green `maps_green/ord.sfz` (504 regions) = 0 loop
opcodes; bass `maps/darkblack_reg_mf_map.sfz` (168 regions) = 0. One-shots -- correct + expected for a
plucked string (you never loop-sustain a pluck). Implication: a MOVING slide restarts a fresh
offset-masked sample at each zone hop, so it continues for as long as it moves; a slide that STOPS on a
pitch decays with that sample's natural tail (realistic; a plucked note cannot sustain forever anyway).
No loop-synthesis needed. The spike's "sustain indefinitely?" worry does not apply to these instruments.

**SS-Q2 (size) — RESOLVED by measuring disk:** one vel layer + one RR of the main sustain articulation =
47 chromatic samples / ~28 MB (guitar green/ord mf rr1) and 42 samples / ~27 MB (bass darkblack/reg mf
rr1). Vs sfizz's own full-instance preload of ~58 MB (guitar) / ~72 MB (bass) from the spike, and vs the
full reg articulation (all vel+RR) of 426 MB. The slide table is ~half of what sfizz already preloads and
is the actual playable audio (no separate stream). Light-memory claim CONFIRMED. Flag: decoded to float32
in RAM ~= ~45 MB/patch; x up-to-20 Inst tabs if eagerly preloaded -> a lazy/decode-on-first-slide option
is a Task-2 preload-timing decision, not a Task-1 blocker.

**Structural findings (verified against the maps, feed Tasks 2-4):**
- **Fully chromatic** -- every semitone is recorded (guitar keys 40-86 E2-D6, bass 35-76 B0-E4), one zone
  per pitch. SL-2 ("pick at-or-just-below, bend up <=1 semi") is not just satisfiable, it is ideal: the
  max micro-bend is the fractional-pitch remainder < 1 semitone, both directions, no chipmunk. Confirmed.
- **No `offset` and no `tune` in the maps** -- SL-3's attack-offset is entirely OUR parameter; tuneCents
  defaults 0 (samples in-tune at pitch_keycenter). Table shape = {rootKey (= lokey = hikey =
  pitch_keycenter), wavPath, sampleLenFrames}; offset/loopStart/loopEnd/tuneCents are ours to synthesize.
- **Main-articulation identification is a heuristic, not "grab all regions"** -- the program stacks
  Center + Unison t1/t2 x poly/mono x keyswitch-articulation x tailpiece(hicc118/locc118) x velocity x RR,
  plus feedback(group=1)/noise(bend_up=0)/release(trigger=release) layers. The walker must select the
  CENTER voice (first <global>, before any `locc100` unison block), default keyswitch (sw_last == the
  program's sw_default), non-tailpiece (guitar hicc118=63), poly (bass hicc105=63), one vel layer, one RR.
  Biggest Task-1 risk.
- **Program coverage limit** -- the default keyswitch programs (01-*) have a sustain articulation; the
  single-articulation variants (e.g. 05-green_staccato, 09-darkblack_stac) do not, so a slide there would
  use short staccato one-shots (gappy). This is a Task-3/Task-4 routing/UI-gating spec call, flagged for
  when those tasks arrive -- it does NOT gate Task-1 extraction (which grabs whatever the active program's
  main articulation is).

**Spec call A surfaced (velocity layer for the slide voice) -- awaiting Jeff, no code yet.** SL-4 locks
"one vel layer held for the whole slide"; two SL-4-consistent readings + a compromise: (a) one FIXED
middle layer for all slides (~28/27 MB, constant timbre, loudness still scales via amplitude); (b)
velocity-PICKED-then-held (extract all recorded layers ~110 MB, pick the layer from the slide note's
velocity at start + hold it -- brightness tracks pick force, ~4x memory); (c) two-layer soft+loud
compromise (~2x, threshold at start). Posed in chat, no recommendation. Holding Task-1 code on the answer.

**Resume action:** on Jeff's answer to spec call A (+ gate-open), write the Task-1 region-extraction
walker for Guitars + Basses, then STOP at the build gate.

## 2026-07-21 — Spec call A RESOLVED + Task 1 (region extraction) — code complete, awaiting build gate

**Spec call A resolved — Jeff picked (b): full velocity bands.** The slide table extracts EVERY recorded
velocity band of the main sustain (guitar 3: p/mf/f; bass 4: p/mp/mf/f), one RR each, so the user keeps
the full velocity/timbre range. Memory kept sane by design: decoded buffers are SHARED via a path-keyed
cache (one copy per unique patch, not per tab) + band-lazy load (a band decodes on first use). Ceiling
~110 MB (guitar) / ~141 MB (bass) per unique patch, once, shared. The shared cache + decode land in
Task 2; Task 1 builds only the path/metadata table (no decode, ~0 RAM).

**Task 1 done (code complete):**
- NEW `Source/SlideSampler/SlideRegionMap.h` + `.cpp` — `SlideSample` / `SlideRegionMap` structs +
  `extractSlideRegions(programSfz)`. Expands the program's #include chain (textual splice, SFZ
  semantics), then a single scope-inheritance pass (region > group > master > global > control). Emits a
  sample per (key, velocity-band) for the CENTER-voice default-keyswitch SUSTAIN only, via the
  qualification filter: has `sample=`; NOT unison (`locc100`); `sw_last == sw_default` (requires a
  sw_default -> single-artic programs yield an empty table by design); NOT tailpiece (`locc118`) / mono
  (`locc105`) / release (`trigger=release`) / feedback|noise (`group` / `locc29` / `bend_up=0` /
  `loop_mode`); one RR (`seq_position` unset or 1). Sample paths resolve against the PROGRAM dir (sfizz's
  base), includes against each including file's dir; both path separators handled (guitar `/`, bass `\`).
  Dedupe guard on (rootKey, loVel, hiVel).
- `BaySickGuitarsProcessor.h/.cpp` + `BaySickBassesProcessor.h/.cpp` — `#include` the header, add
  `SlideRegionMap mSlideRegions` member + `getSlideRegions()` getter; `loadKit` calls
  `mSlideRegions = extractSlideRegions(sfzPath)` after the set_cc push (rebuilds on every program swap).
- `CMakeLists.txt` — registered `Source/SlideSampler/SlideRegionMap.cpp` + added `Source/SlideSampler`
  to the include dirs. (Stacked on the QA-L-Fix DrumTriggerMap.cpp line already in the dirty tree.)

**Source-verified before writing + predicted output (simulated the filter against the real maps, not
guessed):** guitar `green/ord.sfz` -> 141 samples (47 keys x 3 bands); bass `darkblack` reg maps -> 168
samples (42 keys x 4 bands). Control modules (`vibrato_g` etc.) carry only `bend_up=300` (global, stays
included; the noise layer's own master-scope `bend_up=0`/`locc29` still overrides to exclude) -- no
filter-key opcode that would wrongly include/exclude. So on a clean run the DBG should read
"01-green_keyswitch.sfz: 141 sustain samples across 3 velocity band(s), 0 missing" (guitar) and
"01-darkblack_keysw.sfz: 168 ... across 4 ... 0 missing" (bass).

**Diagnostic Instrumentation Catalog (Rule 4):**

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `SlideRegionMap.cpp` `extractSlideRegions` (inside `#if JUCE_DEBUG`) | `[SlideSampler]` | Log extracted sustain-sample count / velocity-band count / missing-path count on each kit load -- sanity check for the from-scratch SFZ walker | Remove at batch close (useful through Tasks 2-5) |

**Flagged for later (not Task 1):** single-articulation programs (04-11, e.g. staccato-only) have no
`sw_default` -> empty slide table by design; how RP Slide behaves there (gate off / fall back / pull from
the keyswitch sibling) is the Task-3/Task-4 program-coverage spec call, to be surfaced when those tasks
land.

**Resume action:** Jeff runs `do_build.bat` (Task 1 build gate). Clean -> surface SS-Q4 handoff + start
Task 2 (SlideSampler component: shared path-keyed cache + band-lazy decode + crossfade voices). Errors ->
diagnose + fix (no reverting the prior-session work).

**Task 1 BUILD GATE: CLEAN** (Jeff, 2026-07-21). Task 1 done + verified. Moving to Task 2.

## 2026-07-21 — Task 2 — SlideSampler component + shared decode cache — code complete, awaiting build gate

**Done (NEW files in `Source/SlideSampler/`):**
- `SlideSampleCache.h/.cpp` — process-wide shared decoded-sample store (`juce::SharedResourcePointer`
  singleton, one dedicated low-priority decode thread). `DecodedSlideSample` = mono float buffer +
  `kTailPad=32` zero-frame pad (safe interp overread) + atomic `ready`. Path-keyed, held WEAK in the map
  -> one decoded copy per unique wav shared across all tabs, freed when the last SlideSampler drops it
  (the spec-call-A shared-cache promise). `getHandle` (create, no decode) + `requestDecode` (enqueue) are
  message-thread; the audio thread only reads `buffer` after `ready` (release/acquire). WAV read matches
  the repo pattern (`reader->read(&buf,0,n,0,true,nc>1)`, VibePlayerDSP.cpp:577), stereo folds to mono.
- `SlideSampler.h/.cpp` — the blended-slide DSP. Per-velocity-band zone tables (sorted by rootKey) built
  in `setProgram` (message thread, processing gate off -> safe; verified loadKit runs between
  setProcessingEnabled false/true at PluginProcessor.cpp:6210-6224). Up to 4 voices (2 steady + brief 3rd
  on a hop + headroom), each a `LagrangeInterpolator` resampler. `startSlide(pitch,vel)` picks the band
  from velocity (spec-call-A(b): held for the slide) + triggers the at-or-below zone at full attack (SL-3
  first-note = no offset/no fade-in); `moveTo(pitch)` re-picks the at-or-below zone, bends the active
  voice UP <=1 semi (SL-2), and on a zone-boundary cross starts an equal-power crossfade (complementary
  `sin(fade*halfPi)` fades) into the new zone with an attack offset + short fade-in (SL-3); `release()`
  lets the one-shot ring its natural tail (SS-Q1). Band-lazy: `ensureBandLoaded(vel)` (message) kicks the
  decode; an audio-thread `startSlide` on an unloaded band sets an atomic `mWantBandVel` that
  `serviceLoads()` (message-thread pump) fulfils. Audio path (startSlide/moveTo/release/renderNextBlock)
  is strictly alloc/lock-free. SS-Q5 crossfade/offset are members w/ setters (defaults 45 ms / 30 ms;
  Task 5 sweeps).
- `BaySickGuitarsProcessor.h/.cpp` + `BaySickBassesProcessor.h/.cpp` — `SlideSampler mSlideSampler` member
  + `getSlideSampler()` getter (Task 3 drives it); `prepareToPlay` calls `prepare`, `loadKit` calls
  `setProgram(mSlideRegions)`. NOT yet driven by MIDI or mixed into the output -- that (route the RP Slide
  note, suppress the sfizz voice, pump serviceLoads, handoff) is Task 3.
- `CMakeLists.txt` — registered `SlideSampler.cpp` + `SlideSampleCache.cpp`.

**RT-safety review (self):** no alloc/lock on the audio thread (voices are a fixed array; interp state is
fixed; zone tables immutable during playback; shared_ptr only `.get()`-read on audio, never copied);
decode is off-thread; publish via atomic `ready` (release) / acquire read. mBands rebuilt only under the
processing gate. In Task 2 `renderNextBlock` is never called (no MIDI drive yet) -> zero audio access, so
the component is inert until Task 3. No new diagnostics (Rule 4 catalog unchanged; Task-1 DBG still the
only entry).

**Resume action:** Jeff runs `do_build.bat` (Task 2 build gate). Clean -> surface SS-Q4 (slide-end
handoff) as a chat spec call, then start Task 3 (pitch-path driver + route RP Slide to the SlideSampler +
suppress the sfizz voice + pump serviceLoads + handoff; reuse the CC84/CC5/CC85 transport). Errors ->
diagnose + fix.

**Task 2 BUILD GATE: CLEAN** (Jeff, 2026-07-21). Task 2 done + verified. Moving to Task 3.

## 2026-07-21 — SS-Q4 resolved (A, provisional) + velocity clarification + smoke items for the re-check

**SS-Q4 RESOLVED — Jeff picked (a): ring out on the SlideSampler** (idiomatic; how Kontakt/Ample/Shreddage
land a legato slide -- the target rings out as a sustain in the same sampler, NO re-attack; (b)'s
re-trigger is the artifact those engines avoid). PROVISIONAL: Jeff wants to A/B the landed-note thinness
at the smoke and may revisit (our hybrid split -- thin SlideSampler band vs the full sfizz stack -- is a
gap the pros don't have, since their sustain sample IS the full sound). If (a) reads too thin, the
idiomatic fix is to ENRICH the SlideSampler landing (crossfade the other velocity layers / a fuller
sample once settled), NOT (b)'s sfizz re-attack.

**Velocity is NOT inert (corrected a sloppy "one band" phrasing).** Because we extracted ALL bands
(spec-call-A(b)), the slide note's velocity: (1) picks WHICH velocity band (soft/med/loud) -> real
recorded-timbre change, exactly like velocity picks the vel-layer on a normal note; (2) scales loudness
(`mBaseGain`). "One band" only meant we HOLD one band per single slide (no band-swap mid-glide -- matches
real legato). It reaches the SlideSampler via CC86 (the slide-loudness transport QA-SlideSliceGlide added,
since an RP takeover has no noteOn velocity byte); Task 3 reads CC86 -> band pick + loudness. Could do more
(velocity->brightness tilt/slide-speed) if Jeff wants, but band+loudness is standard sampler behavior and
satisfies "not inert."

**Smoke-test items to author into sec B at Task 7 (the SS-Q4(a) re-check + velocity + slide quality):**
1. Landed-note thinness A/B: slide up into a target + let it ring, then play the SAME pitch as a normal
   (non-slide) note at the same velocity -- judge whether the slid-into note is noticeably thinner/weaker
   (the (a)->maybe-enrich/(b)/(c) trigger).
2. Velocity liveness: play the same slide at low/med/high velocity -- confirm band (timbre) AND loudness
   change audibly + sensibly (proves velocity isn't inert).
3. Both directions: slide up and slide down -- one continuous voice, no chipmunk.
4. Landing decay: confirm the landed note rings out naturally (one-shot tail), not an abrupt cut.
5. Zone-boundary artifacts: a slow wide slide across many semitones -- listen for clicks/combing at zone
   crossings (also a Task-5 / SS-Q5 tuning target).
6. Slide-into-a-held-note: slide into a target immediately followed by a held normal note at that pitch --
   confirm the SlideSampler tail -> sfizz held note transition doesn't double/click.

**Resume action:** on Jeff's go, build Task 3 with SS-Q4=(a): intercept CC84/85/5/37/86 in the
guitar/bass processBlock, run a per-block anchor->target pitch ramp driving mSlideSampler.moveTo(), suppress
the sfizz anchor (noteOff at slide start), read CC86 -> band+loudness, pump serviceLoads() from the message
thread, mix the SlideSampler output. Then STOP at the build gate.

## 2026-07-21 — Task 3 — slide path + routing + suppress + handoff (SS-Q4=a) — code complete, awaiting build gate

**Done (BaySickGuitarsProcessor + BaySickBassesProcessor, twins):**
- **Transport interception** (processBlock MIDI loop): CC84 (anchor) / CC5+CC37 (14-bit glide ms) / CC86
  (loudness=velocity) are gathered; CC85 (target) ARMS the slide via `armSlide`. None of the 5 forward to
  sfizz; all other CCs (expression 10/71/72/74, pitch wheel, patch CCs) + notes pass through. So a slide
  drives the SlideSampler, not the dead sfizz bend.
- **armSlide:** suppress the sfizz anchor (`mSfizz->noteOff(anchor)`), set up the anchor->target pitch
  ramp (`mSlidePitchStep`), `startSlide(anchor, vel, reAttack=false)` -- a takeover (offset + fade-in, no
  re-pluck; crossfades in under the anchor's sfizz release). velocity from CC86 -> band pick + loudness.
  Guarded by `hasProgram()` (empty slide table -> no arm, anchor holds on sfizz = graceful fallback for
  non-sustain programs; Task 4 gates the UI there).
- **Chain continuation fix** (base->A->B): every chain segment reports the same base anchor. If a slide is
  already active with the same anchor, armSlide RETARGETS from the current pitch (continuous glide, band
  held per SL-4) instead of re-triggering/snapping back to base.
- **Pitch-path driver** (render tail): the block renders in kSlideChunk=64-sample sub-blocks; each advances
  the pitch ramp + calls `moveTo` so the glide + zone crossfades stay smooth (per-block-only would step
  ~0.6 semi/10ms = audible). Arming block starts at the CC85 sample; continuing blocks at 0.
- **Handoff SS-Q4=(a):** anchor's chain-end noteOff (or all-notes-off) -> `mSlideSampler.release()` +
  mSlideActive=false; the landing rings out on the SlideSampler one-shot tail, sfizz not re-triggered.
- **Render mix + DSP gate:** the old "skip if sfizz idle" early-return now skips only when NEITHER sfizz
  NOR the slide is active (so a slide with sfizz suppressed still renders). sfizz renders into the scratch
  when it has voices; the SlideSampler mixes on top; outVol + the stereo/mono copy unchanged.
- **Band-lazy pump:** `startTimerHz(10)` moved to the CONSTRUCTOR (message-thread; prepareToPlay can run on
  the audio thread via the device re-prepare path, PluginProcessor.cpp:448) + `stopTimer()` in the dtor.
  `timerCallback` -> `serviceLoads()` fulfils the band an audio-thread startSlide requested.
- **SlideSampler.h/.cpp:** `startSlide` gained `bool reAttack=true` (takeover uses false -> offset +
  fade-in).

**RT-safety (self-review):** armSlide + the chunked render are alloc/lock-free (member state only; sfizz
noteOff/cc are audio-callable). mBands structure is immutable during playback (rebuilt only in setProgram
under the processing gate); serviceLoads (message/Timer) reads it + the atomic sample-ready flags only, and
the message thread is single so setProgram/timerCallback never overlap. Voices read decoded buffers via
raw ptr after the atomic ready.

**Known limitations (flagged; not spec calls):** (1) a slide note's per-note pan/cutoff/resonance/release
are INERT on the SlideSampler (raw samples; only velocity->band+loudness is wired). Velocity is the primary
control and IS live; the others are secondary. Could add a pan stage / filter later if wanted. (2) First
slide of an un-decoded band has a brief decode latency (band-lazy + the 100 ms pump) before it sounds;
subsequent slides of that band are instant. Both -> smoke observations / possible Task-5 or future polish.

**Diagnostics added:** none (Rule 4 catalog unchanged; Task-1 DBG still the only entry).

**Resume action:** Jeff runs `do_build.bat` (Task 3 build gate). Clean -> start Task 4 (native "Bend" +
engine-aware Note Properties redo; surface SS-Q3 -- new NoteType vs reuse + serialization -- as a chat spec
call BEFORE the code). Errors -> diagnose + fix.

## 2026-07-21 — Task 3 rework: decode timing (a/b discussion) -> option (b) sync-at-load

Two follow-ups from Jeff on the Task-3 limitations, resolved before the build gate:

**(1) "Inert" per-note controls -> Task 4's job, not a limitation.** Jeff: the note-props redo is exactly
where the guitar/bass panel gets pared to only usable controls. Agreed -- dropped as a standalone worry.
Folded into Task 4: determine which per-note controls actually do something on the sfizz engines and show
only those. Nuance to sort there (ties to SS-Q3): velocity works on both normal notes + slides; fine pitch
works on a normal sfizz note (pitch wheel) but not a slide (SlideSampler owns pitch); pan/cutoff/reso/
release are probably inert even on NORMAL notes because the karoryfer patches map their own CCs (27/29/70
guitar, 21/90-95 bass), not the generic CC10/74/71/72 -- VERIFY against the patches in Task 4.

**(2) Band-lazy decode latency is unacceptable -> switched to option (b).** The lazy first-slide latency
is ~150-300 ms (SSD) / up to ~1-2 s (HDD) = audible. Jeff's call (correct): a glitched/late first note is
a real musical defect (worse for beginners, who'll think it's broken), whereas a slightly longer load is a
benign one-time cost. Rejected (a) eager-background (residual first-slide-window) + (c) sync-middle-band
(still glitches at extreme velocity). Locked **(b): synchronously decode ALL bands during the load block**
so no slide ever waits. This SUPERSEDES the "band-lazy load" half of spec-call-A.

**Rework done (net REMOVAL of complexity):**
- `SlideSampleCache` -> fully synchronous: dropped the `juce::Thread` background decode, the request queue,
  the WaitableEvent, `run()`, `requestDecode`. Now just `getHandle` (weak path-keyed map) + `decodeNow`
  (synchronous decode on the caller; skips a handle another tab already decoded -> shared, decode-once).
- `SlideSampler` -> dropped `ensureBandLoaded` / `serviceLoads` / `mWantBandVel` / `Band.requested`.
  `setProgram` now decodes every band inline (`mCache->decodeNow` over all zones) after building the
  tables -- runs inside loadKit's processing-gate-off window, extending the existing "Loading Instrument..."
  block by the decode (~½ s SSD).
- Guitars/Basses processors -> dropped the `juce::Timer` inheritance + `timerCallback` + ctor
  `startTimerHz` + dtor `stopTimer`. armSlide / render / MIDI interception unchanged.
- Shared cache still dedups by path: a 2nd tab on the same patch reuses ready buffers (fast load).

Net: simpler than the band-lazy version (no threading/timer layer), and slides are guaranteed decoded
before the tab can play. RAM unchanged (~110/141 MB per unique patch, shared).

**Task 3 BUILD GATE: CLEAN** (Jeff, 2026-07-21, reworked to (b)). Task 3 done + verified. Moving to Task 4.

## 2026-07-21 — Task 4 — SS-Q3 resolved (A) + controls investigation

**SS-Q3 RESOLVED = A** (new `Bend` NoteType + new signed `bendSemitones` field, serialized separately).
Jeff's reasoning + the reinforcing fact: `portaLengthBeats` stays in ACTIVE use as "beats" on the
synth-family Porta notes (the redo strips Porta on Guitars/Basses ONLY), so reusing it for Bend would
overload a field still doing its real job elsewhere = a live cross-engine dual-meaning footgun. A keeps
each field single-meaning; cost is trivial (one enum value + one attribute; no backward-compat migration).

**Controls investigation (verified against the karoryfer patches + vendored sfizz, not guessed):**
- Guitar patch maps CCs 70/100/101/111/112/113/114/116/117/131/136; bass maps 21/24/25/90/92/100/101/107/
  112-117/131/133/135. NEITHER maps CC10 (pan) / CC74 (cutoff) / CC71 (reso) / CC72 (release) -- the bass
  has its OWN cutoff (CC90) + release (CC107), guitar none of those generic ones.
- sfizz has NO built-in CC10 pan / CC7 volume default (grep of MidiState/Synth/Voice) -> an unmapped pan
  CC does nothing. sfizz DOES consume pitch wheel globally (Synth::pitchWheel:1529 -> global bend on voices).
- => On Guitars/Basses: **Velocity** = functional (vel-layer + loudness; band+loudness on slides).
  **Fine Pitch** = partial (sfizz bends via the wheel, but GLOBAL/chord-wide + guitar is up-only ~+3 so a
  down-detune bends UP = broken direction on guitar; bass +/-2 works; also overlaps the new Bend, both use
  the wheel). **Pan / Cutoff / Resonance / Release** = INERT (CCs unmapped, no sfizz default).
  **Porta Length box** = N/A (Porta not offered on Guitars/Basses).

**Two Task-4 calls surfaced to Jeff (chat):** (1) proposed strip-list for the Guitars/Basses note-props
panel + the one genuine control decision (Fine Pitch keep vs strip); (2) the Bend shape (how the pitch
moves over a Bend note's duration). Awaiting his picks before the UI redo + Bend emit.

**Calls RESOLVED:** strip-list = strip Pan/Cutoff/Reso/Release (inert) + Fine Pitch (Jeff 1=b) + Porta box
+ the dead in-house RP/RT/Porta buttons; KEEP Velocity + Flat/RP Slide/Bend + Bend dropdowns + notice.
Bend shape = a per-note user DROPDOWN of all 4 shapes (adds a second field bendShape).

## 2026-07-21 — Task 4 — native Bend + engine-aware Note Properties redo — code complete, awaiting build gate

**Done:**
- **Data model** (`PatternManager.h/.cpp`): `NoteType::Bend` appended; `BendShape` enum {RampHold,
  RampWhole, UpBack, InstantHold}; `PianoNote.bendSemitones` (signed) + `bendShape` fields (KEEP-LAST);
  serialized as `"bs"` / `"bsh"`. `cycleNewNoteType` handles the new Bend case (guitar/bass-only -> cycles
  out to Standard).
- **Bend-range read** (`SlideRegionMap`): captured the MAIN articulation's effective `bend_up`/`bend_down`
  during extraction (guitar green vibrato_g = 300/300 up-only; bass unset -> sfizz default). `bendUpCents`/
  `bendDownCents` + derive helpers `bendMaxUpSemis()` (guitar 3 / bass 2) / `bendMaxDownSemis()` (guitar 0 /
  bass 2). A WHOLE-patch scan was rejected (guitar has bend_up 20-320 across articulations; only the main
  sustain regions -- which the walker already isolates -- carry the right range).
- **Note-props plumbing:** `PianoRollGrid::NoteEditContext {engineAware, bendUp, bendDown}` + a provider
  callback plumbed connection -> page -> container -> grid (mirrors keyswitchLabelProvider; set
  unconditionally so switching to a non-sfizz engine clears it). StandaloneEditor sets it for
  BaySickGuitars/BaySickBasses, querying `getSlideRegions().bendMax*Semis()`.
- **Engine-aware NotePropsPanel** (`PianoRoll.cpp`): dual-mode. Guitars/Basses -> Flat / RP Slide / Bend +
  Velocity + the gated Bend-amount dropdown (built from the patch range, no 0) + the shape dropdown +
  the always-on notice; the inert controls + Porta box stripped. In-house engines -> the full existing
  panel unchanged.
- **Bend emit:** scheduler `emitBend` (noteOn + CC87 amount=64+semis / CC88 shape / CC5+37 duration) +
  a Bend branch in `sched`. Guitars/Basses processBlock intercepts CC87/88, arms on the noteOn
  (`armBend` scales semitones -> wheel via the patch's real cents), advances a per-block wheel ramp in the
  chosen shape (RampHold rises over the first 25% + holds; RampWhole linear; UpBack up-then-back; Instant),
  and centers the wheel on the note's noteOff / all-notes-off. sfizz's bend_smooth interpolates between
  the per-block updates.

**Compile self-review:** exhaustive NoteType switch (cycle) handled; `<cmath>` added to both processors
(std::lround); ComboBox/target/NoteEditContext scope verified; no new audio-thread alloc (armBend/ramp are
member-state only). Bend uses sfizz's GLOBAL wheel -> chord-wide (the notice covers it). Known: Bend isn't
in the toolbar S-key type cycle (set via note-props only, like the design intends); bend-shape timings are
first-pass defaults (Task 5 can tune). No new diagnostics.

**Resume action:** Jeff runs `do_build.bat` (Task 4 build gate). Clean -> start Task 5 (SS-Q5 crossfade/
offset + bend-shape timing tuning). Errors -> diagnose + fix (biggest task; multi-file).

**Task 4 BUILD GATE: CLEAN** (Jeff, 2026-07-21). Task 4 done + verified. Moving to Task 5.

## 2026-07-21 — Task 5 — crossfade/offset structural pass (SS-Q5 = option 1) + TUNING CHECKLIST

**SS-Q5 resolved = option 1** (Jeff): do the artifact-reducers that are safe blind + set sensible
defaults NOW; dial the actual perceptual VALUES at the bulk-run smoke (ears required, verification is
deferred). No offline comb harness (would be false confidence without hearing). Note SS-Q1 found the
samples are one-shots -> no loops -> "loop-phase alignment" from the original wording is N/A; the real
concern is zone-boundary coherence between adjacent recorded pitches.

**Structural code done:**
- SlideSampler: zone-hop voice start OFFSET is now snapped to the nearest upward zero-crossing
  (`snapZeroCrossing`, ~3 ms bounded search, RT-safe) so the crossfade-in begins clean.
- Equal-power crossfade CONFIRMED (already complementary `sin(fade*halfPi)` fades -> constant power sum).
- Bend "Ramp + Hold": rise time changed from "25% of the note" to a FIXED ~120 ms (capped at the note),
  so a long bend rises quickly then holds instead of ramping over a quarter of a whole note.
- All tunable knobs marked with a `SS-Q5 TUNE` code comment (grep `SS-Q5 TUNE` to find every one).

---

## SS-Q5 TUNING CHECKLIST (do at the bulk-run smoke; a future session dials these by ear)

> These are the values that could NOT be finalized without hearing.  Grep `SS-Q5 TUNE` in Source/ to jump
> to each knob.  Each row: what it is / where / current default / what to listen for / which smoke item.

**SlideSampler slide (Source/SlideSampler/SlideSampler.h + .cpp):**
1. `mCrossfadeMs` (default **45 ms**, member + `setCrossfadeMs`) - zone-boundary crossfade length. Too
   short = audible clicks/steps at each semitone; too long = smeared / combed / doubled. Judge on a SLOW
   WIDE slide across many semitones (smoke item 5).
2. `mAttackOffsetMs` (default **30 ms**, member + `setAttackOffsetMs`) - how far into the sustain a hop
   voice starts (hides the re-pluck). Too small = re-pluck tick at each hop; too big = thin/weak hop.
   Slow slide, listen at each semitone crossing (smoke item 5).
3. Slide loudness map (`startSlide`: `mBaseGain = velocity/100`, floor 0.05) - slide level vs a normal
   note. Compare a slide against a normal note at the same velocity (smoke item 1/2).
4. `kSlideChunk` (=**64** samples, the processor render loop) - pitch-ramp sub-block granularity. If a
   FAST slide zippers, reduce it. Fast slide (smoke item 3).
5. **Landing thinness (SS-Q4=a re-check).** If the slid-into note reads too thin vs a normal note
   (smoke item 1), the fix is to ENRICH the landing (crossfade the other velocity layers / a fuller
   sample once settled) - a real code change, NOT a value. Decide at the smoke.

**Bend shape timings (Source/BaySick{Guitars,Basses}Processor.cpp, the per-block bend ramp):**
6. Ramp+Hold rise time (=**120 ms** fixed, capped at the note) - how fast the bend rises before holding.
   Too fast = abrupt; too slow = sluggish. A Ramp+Hold bend note.
7. Up+Back split (=**50/50** of the note) - the bend-up-then-release feel. An Up+Back bend note.
8. Bend wheel stepping - the wheel updates once per block, relying on sfizz's `bend_smooth` (patch = 40).
   If a SLOW Ramp(whole) bend steps audibly, add sub-block wheel updates (like the slide's kSlideChunk).
   A slow Ramp(whole) bend note.

**Not a tuning knob (by design):** the Bend + RP Slide use sfizz's GLOBAL pitch wheel -> chord-wide; the
always-on note-props notice covers that. Do not "fix" it.

**Resume action:** Jeff runs `do_build.bat` (Task 5 build gate). Clean -> start Task 6 (BaySickPlayer
reuse review - assess whether the SlideSampler can also back VibePlayer's SFZ-loaded sounds; surface the
finding to Jeff BEFORE building any VibePlayer path). Errors -> diagnose + fix.

**Task 5 BUILD GATE: CLEAN** (Jeff, 2026-07-21). Task 5 done + verified. Moving to Task 6.

## 2026-07-22 — Task 6 (VibePlayer reuse review) + Task 7 (close, NO commit per Jeff)

**Task 6 = assessed, DEFERRED to Future State (Jeff's call).** VibePlayer already has a note->sample table
(`VibeSampleManager::loadSFZ`/loadFolder/loadSingleFile) so extraction is free; `extractSlideRegions`
itself doesn't generalize (karoryfer-specific) but isn't needed. The blocker: the blended slide's benefit
requires DENSE/chromatic sampling; VibePlayer sounds are frequently sparse (single-file stretched across
the keyboard, folders one-per-octave), so the crossfade recipe has nothing coherent to blend (chipmunk/
mud). Jeff: not useful for current sounds -> Future State, revisit with denser SFZ setups. Added as
`Future State.md` §P2 **[CL-302 / AQ]**. No VibePlayer code landed.

**Task 7 close (NO commit -- Jeff: "that's for the boundary commit"):**
- APPLIED this session (not commit-coupled): §B.23 authored in the Master Test Plan (supersedes SS-A in
  §B.22); `/draft-doc batch-close` HELD Work Log entry (below); Future State §P2 [CL-302] (VibePlayer
  reuse deferral).
- `/review-batch` (batch-code-reviewer) dispatched over the QA-SlideSampler diff.
- QUEUED for the BOUNDARY COMMIT / R2 campaign pass (commit-coupled -- reference the close hash / mark
  completion, so deferred with the commit Jeff deferred): Main Plan §5 STATUS flip + §6 arrow + §9 Forks
  entry (sixty-second: A-1 REOPENED + DELIVERED by the SlideSampler, supersedes the global-pitch-wheel
  approach, back-ref the sixty-first entry) + QA-H §5 annotation (the engine-aware NotePropsPanel redo);
  strip the Task-1 `[SlideSampler]` DBG (borderline -- useful at the smoke; Jeff's call at the commit);
  apply the HELD Work Log entry + backfill timestamp/hash.
- HEAD unchanged; nothing committed this session.

---

## HELD Implemented Work Log entry (applies at the §B.23 campaign pass, bulk-run R2)

> Drafted at code-complete via `/draft-doc batch-close`; NOT yet applied to `Implemented Work Log.md`.
> Applies with the §5 STATUS flip when §B.23 passes the campaign walk (R2).  Backfill the timestamp +
> close commit hash at commit.  Bucket = Players (the core is the SlideSampler + Guitars/Basses engines;
> the diff also touches PianoRoll/StandaloneEditor UI + PatternManager serialization, but Players is the
> dominant domain per Jeff's call).

### 2026-07-2X — QA-SlideSampler — Blended multi-sample slide (new SlideSampler) + native Bend for the sfizz Guitars/Basses engines; reopens + delivers QA-SlideSliceGlide A-1

**Bucket:** Players. Batch `silky-gliding-lynx`. `blocks:` (backfill hash).

#### Done
- **Task 1 — Region extraction (`SlideRegionMap`).** NEW `Source/SlideSampler/SlideRegionMap.h/.cpp`:
  `extractSlideRegions` expands the program's #include chain + a scope-inheritance pass (region > group >
  master > global > control), emitting one sample per (key, velocity-band) for the CENTER-voice
  default-keyswitch SUSTAIN only (qualification filter excludes unison/tailpiece/mono/release/feedback/
  noise; one RR). Guitar green -> 141 samples (47 keys x 3 bands); bass darkblack -> 168 (42 keys x 4).
  Fully chromatic -> SL-2 ideal (micro-bend < 1 semi). SS-Q1 = one-shots (no loops); SS-Q2 = ~28/27 MB per
  band. Also captures the main articulation's native bend range (SL-6: guitar +3 up-only, bass +/-2).
- **Task 2 — `SlideSampler` + shared `SlideSampleCache`.** Path-keyed weak-held decoded store (one copy per
  unique wav across tabs), mono fold, tail-pad. 4 crossfade voices, `LagrangeInterpolator` resample,
  equal-power `sin` crossfade, at-or-below zone pick + <=1-semi micro-bend, attack-offset + fade-in on
  hops (SL-3), first note full attack. spec-call-A(b): full velocity bands, one band held per slide. Audio
  path alloc/lock-free.
- **Task 3 — RP Slide routing + suppress + handoff (Guitars/Basses twins).** processBlock intercepts the
  CC84/85/5/37/86 slide transport; `armSlide` suppresses the sfizz anchor + drives the anchor->target
  sub-block (kSlideChunk=64) pitch ramp into `moveTo`; chain-continuation retargets from the current pitch;
  SS-Q4=(a) ring-out on the anchor's chain-end noteOff; render gate skips only when neither sfizz nor slide
  is active. **Reworked to option (b): synchronous decode-at-load** (dropped the band-lazy timer/thread/
  queue layer; `setProgram` decodes all bands inline in the loadKit gate window -> no first-slide latency;
  supersedes the band-lazy half of spec-call-A). RAM ~110/141 MB per unique patch, shared.
- **Task 4 — Native Bend + engine-aware Note Properties redo.** `NoteType::Bend` + `BendShape` +
  signed `bendSemitones`/`bendShape` (KEEP-LAST) + `"bs"`/`"bsh"` serialization (SS-Q3=A: new type + new
  field, not reusing portaLengthBeats which stays live for synth Porta notes). Engine-aware NotePropsPanel
  (Flat / RP Slide / Bend + Velocity + gated Bend-amount dropdown + shape dropdown + always-on chord-wide
  notice; inert Pan/Cutoff/Reso/Release + Fine Pitch + Porta box + dead in-house buttons stripped there;
  in-house rolls unchanged) plumbed via `NoteEditContext` provider. Controls investigation verified against
  the patches (Velocity live; Pan/Cutoff/Reso/Release inert -- CCs unmapped, no sfizz default; Fine Pitch
  stripped per Jeff). Bend emit: `emitBend` (noteOn + CC87 amount / CC88 shape / CC5+37 duration) +
  `armBend` scales semis->wheel via the patch's real cents + a per-block wheel ramp in the 4 shapes,
  centered on noteOff.
- **Task 5 — SS-Q5 structural pass (option 1).** Zero-crossing-snapped hop offset; equal-power confirmed;
  Bend Ramp+Hold rise time fixed ~120 ms (capped). Perceptual values HELD for the smoke via the SS-Q5
  TUNING CHECKLIST (8 knobs + the landing-thinness decision), each marked `SS-Q5 TUNE` in code.

#### Found along the way / routed
- **Task 6 VibePlayer/SlideSampler reuse -> DEFERRED to Future State §P2 [CL-302].** Benefit needs dense/
  chromatic sampling; VibePlayer sounds are often sparse.
- **Reopens + delivers QA-SlideSliceGlide A-1** (purpose-built SlideSampler + native Bend supersede the
  global-pitch-wheel approach that batch STOPPED on). §9 Forks entry back-refs the QA-SlideSliceGlide entry.
- **Engine-aware note-props redo touches QA-H's NotePropsPanel** -> QA-H §5 annotated.
- **spec-call-A band-lazy half SUPERSEDED** by option (b) synchronous decode (paper trail: option removal).
- **SS-Q4=(a) is PROVISIONAL** -- A/B the landed-note thinness at the smoke (smoke item 1 / checklist row 5);
  if too thin, enrich the SlideSampler landing (crossfade more layers), NOT re-attack sfizz.
- **Program-coverage limit (flagged, not a spec call):** single-articulation programs (no sw_default) ->
  empty slide table -> audio degrades gracefully (`hasProgram()` no-arm -> anchor holds on sfizz). No
  explicit UI gating for those was added this batch (RP Slide/Bend are offered regardless of program);
  disable/relabel on non-sustain programs is a possible later refinement.
- **Known slide limitations:** per-note pan/cutoff/reso/release inert on a slide (velocity->band+loudness
  is the live control); Bend + RP Slide use sfizz's GLOBAL wheel -> chord-wide (the notice covers it; by
  design).

#### Verified
Per-task build gates all CLEAN (Jeff, 2026-07-21/22): Tasks 1-5. Behavioral verification DEFERRED to the
bulk-run R2 campaign pass against §B.23 (supersedes §B.22 SS-A) + the SS-Q5 tuning checklist by ear.

#### Diagnostic Instrumentation Catalog
| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `SlideRegionMap.cpp` `extractSlideRegions` (`#if JUCE_DEBUG`) | `[SlideSampler]` | log extracted sample/band/missing counts on kit load | Remove at boundary commit (borderline -- useful at the smoke/tuning; Jeff's call) |

## 2026-07-22 — /review-batch result + fixes (needs a re-build gate)

**batch-code-reviewer: 1 BLOCKER + 2 NEEDS-FIX + 3 NITs (scoped to QA-SlideSampler; prior batches excluded).**
- **BLOCKER (FIXED):** a note set to "Bend" via the type button never seeded `bendSemitones` (the combo's
  `setSelectedId` is silent + re-picking the shown value fires no onChange), so it stayed 0 -> `emitBend`
  sent CC87=64 = a SILENT bend on the natural path (the headline feature dead). Verified real. Fixed:
  `applyType(Bend)` now seeds the amount + shape from the dropdowns' shown values (PianoRoll.cpp).
- **NEEDS-FIX (FIXED):** dead `getSlideSampler()` getter (zero call sites -- Task 3 drives mSlideSampler
  internally). Removed from both processors (`feedback_clean_own_batch_dead_code_in_batch`).
- **NEEDS-FIX (deferred, documented):** Task-1 `[SlideSampler]` DBG -- left for the boundary commit per the
  catalog disposition above (borderline; Jeff's call at commit).
- **NITs (deferred):** no Bend glyph in the roll paint (cosmetic, not spec'd -> possible later parity);
  slide below the lowest recorded string bends DOWN (only reachable below the instrument's range, benign);
  SFZ `default_path` not handled in extraction (the 2 shipping karoryfer patches don't use it; graceful if
  a future one does). All logged for a later polish pass, none block the batch.
- Reviewer confirmed RT-safety, transport CC ordering, ASCII-clean literals, the `cycleNewNoteType` Bend
  case, and the bend-range derivation as correct.

The BLOCKER + dead-getter fixes changed code -> a re-build gate is needed before the batch is truly
code-complete. Files: `Source/Standalone/PianoRoll.cpp` (Bend seed), both processor `.h` (getter removed).

**Resume action:** Jeff runs `do_build.bat` (Task-7 review-fix build gate). Clean -> batch is code-complete
(docs applied: §B.23, Future State CL-302, HELD Work Log; §5/§9/QA-H + DBG-strip + Work-Log apply QUEUED for
the boundary commit). NO commit this session (boundary commit is Jeff's). Errors -> diagnose + fix.
