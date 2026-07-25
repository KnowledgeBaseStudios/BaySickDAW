# DAW Architecture Research — Sample-Based Continuous Pitch Slide (Fretboard Glissando) — 2026-07-20

> Feasibility spike for QA-SlideSliceGlide Task 6 (A-1). Guitars/Basses play complex karoryfer SFZ
> patches through vendored sfizz. Goal: a continuous fretboard-style slide, in either direction,
> built from the REAL discrete per-note samples (not one sample stretched across the whole slide),
> reading as one continuous voice. Feeds a build-vs-defer decision — NOT a build.
> Parent-reviewed (key premises re-verified against in-repo source) and applied per the drafter-only rule.

## The two sfizz walls (source-verified)

1. **sfizz mixes ALL voices into one stereo buffer** (`BaySickGuitarsProcessor.cpp` one `renderBlock`
   into a single scratch). The host cannot isolate an individual note's audio to crossfade it.
2. **Pitch bend is GLOBAL and small/asymmetric.** `Synth::pitchWheel` (Synth.cpp:1529) writes one
   global bend read by every voice; no MPE / per-note bend anywhere in `libs/sfizz/src`. Native
   ranges: guitar main articulation `bend_up≈270-320`, `bend_down` POSITIVE = ~3 semitones UP ONLY;
   bass = sfizz default ±2.

sfizz's static SFZ key-crossfade opcodes (`xfin_lokey/xfout_*`, Region.h:328-336) crossfade regions
at note-TRIGGER by note number — there is no host-drivable continuous crossfade position during a
held note. Useless for a sweep. sfizz has no glide/portamento; the maintainer says real glide "would
require extensions to the SFZ opcode set" (github sfizz discussions/1211, issue #894). So any slide
is 100% our code, on top of sfizz or outside it.

## State of the art (how real engines do it)

- **SFZ/ARIA/sforzando glide = pitch-envelope on a SINGLE held sample** (`egNN_pitch_oncc140` + CC109
  time). That's the "one sample stretched" chipmunk approach we're rejecting; cheap (1 voice), fine
  only for small bends. (sfzformat.com/tutorials/legato, KVR.)
- **Kontakt scripted legato / SIPS + commercial guitar libs (Ample "Slide Smoother", Shreddage):**
  the pro recipe = **2 voices steady-state, a brief 3rd during handoff, equal-power crossfade of real
  SUSTAIN samples, a sample OFFSET (start into the sustain body) to hide the re-pluck attack, and a
  small per-sample pitch correction** to sit on the continuous path. Ample auto-triggers a slide when
  two notes overlap on a string; long-slide speed from destination velocity. (vi-control legato
  threads, Pulse Downloader "Black Art of True Legato" [HIGH], Ample/Shreddage manuals [snippet-level].)
- **Formant physics (why the micro-bend trick works):** a big pitch stretch scales formants
  (chipmunk); a SMALL shift keeps them effectively fixed. Capping the per-sample stretch at ≤1
  semitone caps formant drift to inaudible, in EITHER direction. Bitwig's Sampler has a duophonic
  "crossfade two voices" mode for exactly this smoothing. (Bitwig Sampler, Zynaptiq/Bernsee.)

## Comparative analysis

Every real slide engine needs three things sfizz denies us on its samples: (1) isolate a sample's
audio, (2) crossfade 2-3 on a continuous position, (3) micro-bend each. The whole question is HOW to
get sample isolation.

| Approach | Clean sample isolation | Micro-bend both dirs | Memory | CPU while sliding | State-sync | Sound ceiling | Effort |
|---|---|---|---|---|---|---|---|
| **(A) N dedicated sfizz instances** | **NO** — each instance renders the FULL layer stack (Center + Unison t1/t2 + feedback) already mixed; you'd crossfade layer-mud (8-12 layers), not clean samples | **NO** — still sfizz, still up-only | N × full-patch preload (58/72 MB each; heavy, ×20-tab cap) | N full renders/block | HIGH — every KS/CC/RR/vel-layer must be bit-identical across instances | LOW-MED | Med, fragile |
| **(B) Purpose-built sampler on raw wavs** | **YES** — you own playback | YES — trivial | +1 copy of the SUSTAIN wavs only | 2-3 light voices, only while sliding | LOW | HIGH | High (region extraction + crossfade tuning) |
| **(C) Hybrid: sfizz for normal notes + small SlideSampler for the slide gesture** | **YES** (in the SlideSampler) | YES | +sustain wavs; ZERO extra sfizz instances | 2-3 voices while sliding, ZERO otherwise | LOW | HIGH | High-minus |

Decisive facts:
- **(A) does not solve the problem.** Even 3 instances can't crossfade two CLEAN samples — sfizz hands
  you the full region stack per note (our patches stack Center + Unison t1/t2 + feedback, and combo =
  Green+Black, so one "note" is already 4-6 layers; crossfading two = 8-12 = mud). And each instance
  still has the up-only bend, so no downward micro-bend. You pay 3× memory + 3× CPU to still sound
  mediocre. **Don't build A, not even as a stopgap.**
- **(B)/(C) "lost fidelity" is mostly the RIGHT sound for a slide.** A finger slide is ONE string voice
  moving; the unison-doubling/feedback/keyswitch stack is exactly what smears a crossfade. Dropping it
  FOR THE SLIDE GESTURE is a feature. Normal notes still play the full-fat sfizz patch.
- **Region extraction is NOT a from-scratch SFZ parser.** We already own an `#include`-walking parser
  in `loadKit` (depth 4, harvests set_cc/bend_up). Extending it to capture `sample=`/`lokey/hikey`/
  `pitch_keycenter`/`offset`/`loop_start/end` for the MAIN articulation is a bounded add and avoids
  coupling to sfizz's private `Region` type. Biggest de-risker for B/C.
- **Voice/CPU:** 2 voices steady, 3 briefly at each zone handoff; pick ONE round-robin at slide start
  and HOLD it. Cost is zero when not sliding.

## Recommendation

**Build option (C), the hybrid, as its OWN batch when there's room. Do NOT build (A) even as a stopgap.
If no room now, DEFER (C) cleanly and ship the native global "Bend" as the small-realistic-bend interim.**

- (A) is a memory/CPU sink that fails on its own terms (no clean-sample isolation, still up-only) — wrong
  direction for a beginner-facing app with up to 20 sfizz-backed Inst tabs.
- (C) confines ALL slide complexity to one owned component, needs ZERO extra sfizz instances, loads only
  the sustain articulation (a fraction of the patch). sfizz stays the sole engine for every normal note;
  a slide note routes to the SlideSampler, which crossfades 2-3 real sustain samples with a ≤1-semitone
  micro-bend (pick the sample at-or-below, bend UP — works both directions) and attack-offset per hop.
  This is exactly the Kontakt/Ample recipe on samples we already own.
- Honest residual limits of (C): (i) zone-boundary crossfades can comb/beat if adjacent samples are out
  of phase — mitigate with short equal-power crossfades near loop points; (ii) a real aggressive scoop's
  continuous pick/finger noise isn't in discrete sustains, so it reads as "smooth gliss," not "dive
  bomb." For this audience that's an upgrade, not a compromise.

## Implementation sketch (option C, ~2-4 weeks honest — its own batch)

1. **Region extraction** — extend the loadKit include-walker to also capture, for the MAIN sustain
   articulation, `note → {wavPath, rootKey, offset, loopStart, loopEnd, tuneCents}` (one vel layer + one
   RR). ~2-3 days.
2. **SlideSampler (JUCE)** — tiny 2-3 voice sampler: equal-power crossfade, per-voice resample via
   `juce::LagrangeInterpolator`/WindowedSinc for the ≤1-semi micro-bend, attack-offset + short amp
   fade-in on non-first voices. Preload the extracted wavs. ~3-5 days.
3. **Slide path + handoff** — route the "RP Slide" note type on Guitars/Basses to the SlideSampler (not
   sfizz) for that note; suppress the sfizz voice during the gesture; hand back cleanly on slide-end.
   Reuse the existing CC84/CC5/CC85 transport. ~2-3 days.
4. **Pitch-path driver** — per block advance source→target over the glide ms; pick the sample
   at-or-just-below (bend up ≤1 semi) and crossfade to the next zone as the path crosses each boundary.
5. **Note-props UI** — split Guitars/Basses note types into "RP Slide" (this blended slide) + "Bend"
   (native global pitch-wheel, semitone dropdown gated by the patch range). ~2-3 days.
6. **Crossfade/offset QA** — tune crossfade length, equal-power law, per-sample offset, loop-phase
   alignment so boundaries don't click/comb. Budget generously. ~2-4 days.

## Open questions (resolve at build time)

- Do the karoryfer main-articulation regions carry usable `loop_start/loop_end` (so a slide can sustain
  indefinitely) or are they one-shots?
- On-disk size of ONE sustain articulation per patch (drives the "small memory" claim).
- Mono-slide-only, or per-string poly-slide (Ample-style)? Start mono; poly multiplies voices.
- Zone-crossfade phase coherence between adjacent recorded pitches — quick offline comb test before committing.

## Sources

- WebFetched (HIGH): github.com/sfztools/sfizz discussions/1211 + issues/894; sfzformat.com/tutorials/legato;
  kvraudio SFZ portamento thread; pulsedownloader.com "Black Art of True Legato".
- WebSearch snippet (MEDIUM): vi-control legato threads (x2); amplesound.net Ample product page;
  Impact Soundworks Shreddage 2 manual (PDF text-extract failed); bitwig.com/sampler; Zynaptiq/Bernsee.
- In-repo source-verified (HIGH, direct read): sfizz global-bend/no-MPE/one-buffer-mix; guitar up-only /
  bass ±2 native ranges; CC84/85 glide protocol; loadKit include-walker.
- Confidence in the recommendation: HIGH — the decisive facts (sfizz can't isolate a voice or bend
  per-note; small pitch shifts preserve formants; our patches are multi-layer per note) are all
  source-verified, and (A)'s failure follows from them directly.
