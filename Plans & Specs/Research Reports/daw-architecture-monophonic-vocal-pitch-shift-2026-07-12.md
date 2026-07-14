# DAW Architecture Research -- Monophonic Vocal Pitch Shifting

**Date:** 2026-07-12
**Method:** 4-angle research workflow (commercial reference tools; algorithm families; formant preservation; codebase audit) + synthesis, cross-checked against the actual source.
**Trigger:** QA-Fd pitch-editor work surfaced that the shared PsolaShifter stopped shifting pitch (a continuous-read-pointer change deleted the shift), and the phase-vocoder prototype chipmunked (no formant preservation). Owner requested a review of what Newtone/Melodyne/Auto-Tune/elastique do and whether to offer PSOLA + Vocoder as a user-selectable feature.

---

# BaySickDAW Vocal Pitch Shifting — Architecture Recommendation

**Author:** Lead audio architect · **Date:** 2026-07-12 · **Status:** for owner approval (not a coding agent)

**One-line verdict:** We are not missing a DSP algorithm. Both halves of the studio-grade solution already ship in the tree — TD-PSOLA (`PsolaShifter`) and a cepstral formant-imposition stage (`CepstralFormantEngine`). The problems are (a) one recent change that mathematically deleted the pitch shift, and (b) two stages that were built but never wired together. This is a wiring + grain-placement fix, not a rewrite.

---

## 1. Root-cause confirmation

**Confirmed: the current PSOLA cannot shift pitch, by construction.**

At `PitchShifters.h:155` the synthesis loop advances the analysis read pointer by `pOut` on every synthesis mark, and at `:167` a synthesis mark fires every `pOut` output samples. So the read pointer moves through the input at exactly **1.0× wall-clock**. TD-PSOLA gets its pitch change *only* from re-emitting whole glottal periods more often than they occur (up-shift) or dropping them (down-shift). Advancing the read at wall-clock rate means every grain reads a fresh, non-repeated chunk, the overlap-add reconstructs the input 1:1, and the measured output F0 ratio is ~1.00 for any requested shift. The drift-relock at `:157-158` only trips past half a period, which at steady wall-clock rate it essentially never does. This is a fundamental degeneracy, not a tuning miss. The in-code comment at `:133-146` claiming "pitch-only, no time stretch" is wrong reasoning — a wall-clock read is a pure delay at any ratio.

**Is the original nearest-epoch snap the right base to restore? Yes — with a caveat.**

The pre-2026-07-12 scheme (`target = outAbs - hw; read = nearestEpoch(target)` every mark, `:147-152`) is the correct TD-PSOLA mapping and *must* come back to get any shift at all. The mark-snap and the shift are inseparable in naive TD-PSOLA — you cannot smooth the staircase without deleting the shift, which is exactly the trap the recent change fell into.

**But** the moiré that motivated the change is real (measured 42-48% amplitude beat at f0·|ratio-1|). It is *not* inherent to correct PSOLA — textbook TD-PSOLA ripples a few percent, not 40%. Our beat is a **grain-coherence bug**: grains are the wrong length (ratio-coupled `hw = max(pOut, min(P, 2·pOut))` at `:115` instead of a fixed 2 periods) and are not guaranteed to be centered on the same glottal phase across a duplication, so when the same period is emitted twice `pOut` apart the two copies partially cancel. So: restore the snap to un-break the shift, then fix the moiré at its actual source (grain length + phase-coherent placement), *not* by removing the snap.

---

## 2. Recommended architecture

A two-engine design, both formant-preserving, behind one shared interface used by all three consumers.

**Engine A — PSOLA ("Natural / Live").** Fixed 2-period, GCI-centered TD-PSOLA. Lowest latency (~2 pitch periods, ~13 ms). Formant-preserving *for free* (grains copied unstretched carry the vocal-tract envelope). Default for the live monitor path (pitch pedal) and the pitch editor's real-time preview.

**Engine B — Vocoder ("Smooth / Studio").** Our existing `PhaseVocoder` (Laroche-Dolson, FFT 2048, 75% overlap) doing stretch-by-ratio + resample, with `CepstralFormantEngine(preserve=true)` chained on its output to re-impose the dry spectral envelope and kill the chipmunk. Higher latency (~40 ms PV + ~20 ms formant stage), so it's the offline/bake and Align default; offering it *live* is a spec call (see §10).

**Formant preservation** is engine-specific: PSOLA gets it intrinsically once grains are fixed; the vocoder gets it from the cepstral stage. A deliberate "throat"/character shift remains available on both via the same stage's `throatSemis` path (already implemented, `PitchShifters.h:773`).

**Shared surface:** all three shifters already expose `prepare/reset/processSample(in, ratio)`. Add a thin `IPitchShifter` interface (or a block-level engine switch) plus one "Pitch Engine" choice per consumer. `BaySickAlignDSP` already has a 3-way `pitchAlgo` selector — that's the template.

---

## 3. Engine A — PSOLA done right

Three fixes, in order:

**A1 — Restore the shift (the mark mapping).** Revert `:129-168` to per-mark nearest-epoch placement: every synthesis mark, snap the read to `nearestEpoch(outAbs - hw)`; keep `mNextSynthAbs += pOut` as the synthesis cadence only. This alone re-instates the shift (and, temporarily, the moiré).

**A2 — Kill the moiré at its source (grain coherence), not by removing the shift.** Three coupled changes:
- **Fixed grain length.** Set the grain half-window to the analysis period P (2-period grains), *independent of ratio*. The current ratio-coupled `hw` changes the captured spectral envelope with pitch and is a primary moiré/formant-drift source. Guard big up-shift combing by capping concurrent grain count, not by shrinking the window.
- **Exact GCI centering + polarity lock.** Center each grain on its detected glottal-closure epoch and hold epoch polarity (the polarity lock at `:253-267` already does this — keep it). When a period is duplicated, both copies are the same GCI-centered 2-period grain, so they overlap-add coherently instead of beating.
- **Sub-sample (fractional) placement.** Carry the fractional `(synthMark − epoch)` offset into the grain read (fractional-delay/interpolated read) so duplicated grains land on smooth sub-sample offsets rather than snapping to the integer P-grid. This is what drops the residual from ~45% to epoch-jitter level.

**A3 — (optional, belt-and-suspenders) WSOLA-style refinement.** If A2's residual is still audible on sustained voiced material, add a small cross-correlation search (±P/4) to pick the grain offset that maximizes overlap continuity. This is the textbook way to decouple beat from shift in the time domain. Reserve it as a polish pass — A1+A2 should get us to the bar.

Formants need no extra stage on Engine A: fixed-content, fixed-length, GCI-centered grains preserve the short-time envelope by construction. That is *why* PSOLA is the speech-industry default and does not chipmunk.

---

## 4. Engine B — Vocoder, formant-preserving

The prototype shifted F0 dead-on but chipmunked because it scaled formants with pitch (plain PV has no envelope handling). The fix is already coded and used elsewhere — just not composed with the PV.

**Minimal, recommended:** keep `PvShifter` as-is (stretch-by-ratio → resample), and chain `CepstralFormantEngine(preserve=true)` on its output, fed `dry` = the pre-shift sample and `wet` = the PV output. The engine extracts the dry log-spectral envelope via cepstral liftering (`:811-828`) and imposes it on the shifted frame (`:773-795`) — the exact elastique/Auto-Tune/Waves recipe. Cost: ~20 ms added latency (free offline). Zero new DSP primitives.

**Higher quality, more work (later):** move the envelope handling *inside* the phase vocoder — whiten (divide by envelope) before the shift, re-impose the original-frequency envelope after — one FFT stage, lower latency, no double-smoothing. Also add Laroche-Dolson identity/peak phase-locking to `PhaseVocoder.cpp` to fight PV "phasiness." **Open code-check:** confirm whether `PhaseVocoder.cpp` already does peak/identity locking or naive per-bin phase propagation; if naive, that's the single highest-value PV quality upgrade.

---

## 5. Formant plan (fixes chipmunk on both engines)

- **Engine A (PSOLA):** intrinsic — no separate stage. Formants ride along in the unstretched, fixed-length grains. Chipmunk is impossible here once A1-A2 land.
- **Engine B (Vocoder):** route through `CepstralFormantEngine(preserve=true)`. This is the missing-but-already-built piece. The stage is real, latency-compensated, audio-thread safe (no allocation after `prepare`), and already validated offline (`formantShiftMono`).
- **Rule that falls out naturally:** engine == Vocoder → cepstral preserve stage on; engine == PSOLA → rely on native preservation. A user "throat/character" control maps to the same stage's `throatSemis` on both engines (it already runs in the pitch editor for deliberate throat shift, `BaySickPitchDSP.cpp:756-758`, currently with `preserve=false`).
- **Quality ladder for later polish:** the cepstral lifter (fixed ~1 ms quefrency) is adequate but underestimates spectral peaks and dulls bright/high vocals. The pro-grade upgrade is the Roebel-Rodet **iterative "true envelope"** (DAFx-2005): re-smooth the max of the current envelope and the log-magnitude until it sits on the harmonic peaks. Far better on sopranos where harmonics are sparse. This is the superVP/Melodyne-class estimator. Reserve it for the offline bake path where its CPU cost is irrelevant. **Not a blocker.**

---

## 6. Shared interface across the 3 consumers

All three shifter classes already share `prepare(sr, maxBlock)` / `reset()` / `processSample(in, ratio)`. PSOLA adds `setPeriodSamples` / `feedSample` / `resyncToWriteHead`. Minimal seams:

1. **Interface.** Add `IPitchShifter` (virtual `processSample`; empty-default the PSOLA-only methods) and derive all three — per-sample virtual cost is negligible (already one call per sample). Or keep concrete types behind a block-level mode switch for zero virtual overhead.
2. **One "Pitch Engine" choice per surface.**
   - `BaySickAlignDSP` — already has `pitchAlgo` (0=PSOLA/1=Granular/2=PV, `:705/760`). Only needs the formant-preserve stage added to its PV branch (currently pure chipmunk — it references `CepstralFormantEngine` nowhere).
   - `BaySickPitchDSP::applyEditsToBuffer` — takes a hard `PsolaShifter*` (`:554`). Change to the interface pointer so the offline render path can select `PvShifter`.
   - `PitchCorrectorDSP` — hard-owns `std::array<PsolaShifter,2>` (`:118-119`); swap to the interface, guard the PSOLA-only period/feed calls. It *already* has `CepstralFormantEngine[2]` and engages it opt-in (`:289-325`), so it's closest to done.
3. **Formant auto-rule** wired once at the interface level (§5).

Net: one interface, one choice param, one auto-rule — no consumer needs bespoke logic.

---

## 7. What we're missing vs Melodyne / Newtone

The gap is small and specific:

- **Formant/pitch as separate axes.** Melodyne, Auto-Tune, Waves Tune, and elastique all decouple the spectral envelope from the excitation, shift pitch, then re-impose or independently scale the envelope. We *have* this stage; it's just off in the pitch/align paths.
- **Peak-accurate envelope estimation.** They use true-envelope (superVP) or equivalent; we use a cheaper fixed-quefrency lifter that dulls high vocals. Ladder item, not a wall.
- **PV phase-locking.** Pro tools use identity/peak locking to avoid phasiness; unconfirmed whether ours does.
- **Sub-sample GCI-coherent grain placement in PSOLA.** The difference between our 45% moiré and their clean few-percent ripple.
- **(Post-V1) A source-filter offline "studio" engine.** WORLD (F0 + CheapTrick envelope + D4C aperiodicity; modified-BSD, patent-free, license-clean for our closed standalone) is the true best-quality monophonic path and a natural third offline algorithm later. Not needed for V1.

We are **not** missing: TD-PSOLA, a phase vocoder, or a formant corrector. We ship all three.

**Do not buy elastique.** It's the industry default but a paid SDK, license-incompatible with a self-built engine, and redundant here.

---

## 8. Phased plan (un-break the shift FIRST, then quality, then the feature)

**Phase 0 — Un-break the shift (highest priority, smallest change).**
Revert `PitchShifters.h:129-168` to per-mark `nearestEpoch(target)` placement (§A1). Ship-blocking: nothing else matters until pitch shift measurably returns (verify F0 ratio ≈ 2^(semi/12) for ±3, ±7, ±12). Expect the moiré to return — that's acceptable at this checkpoint.

**Phase 1 — PSOLA quality (kill the moiré right).**
Fixed 2-period grains, exact GCI centering + polarity lock, sub-sample fractional placement (§A2). Verify the 42-48% beat drops below audibility on sustained vowels at 1-2 semitone shifts (the worst case). Optional A3 WSOLA refinement only if residual persists.

**Phase 2 — Vocoder formant fix (un-chipmunk Engine B).**
Chain `CepstralFormantEngine(preserve=true)` on `PvShifter` output (§4 minimal). Fix Align's PV branch first (it's the pure-chipmunk one). Verify upward shifts sound natural, not chipmunk.

**Phase 3 — Ship the user-selectable feature.**
`IPitchShifter` interface + "Pitch Engine" choice on all three consumers + the formant auto-rule (§6). UI: PSOLA (Natural/Live) vs Vocoder (Smooth/Studio). Align already has the selector to copy.

**Phase 4 — Polish (post-ship, as time allows).**
True-envelope estimator on the offline path; PV phase-locking; consider WORLD as a third offline "studio" engine.

Phases 0-1 restore and legitimize the flagship editor. Phase 2 makes the second engine usable. Phase 3 delivers the owner-requested choice. Phase 4 chases the last few percent toward the Melodyne bar.

---

## 9. Risks

- **PSOLA epoch detection robustness.** The whole time-domain path hinges on solid GCI detection. It's clean on voiced monophonic vocals but degrades on breathy/unvoiced/noisy material and consonants. Mitigation: fall back to the vocoder (or a gentler ratio) on low-confidence frames.
- **Latency mismatch for a live vocoder.** PV (~40 ms) + formant stage (~20 ms) is fine offline, marginal for live self-monitoring — conflicts with the low-latency-monitor rationale (Call 2a). Don't default the live pedal to Vocoder.
- **CPU budget across three consumers.** The formant stage runs per channel per active pill/voice; verify headroom on the pitch editor's polyphonic worst case.
- **Cepstral dullness on bright vocals.** Adequate but not transparent until the true-envelope upgrade; set expectations on high sopranos.
- **PV phasiness** if no phase-locking exists — could undercut the "Smooth/Studio" branding until Phase 4.
- **Regression surface.** Phase 0's revert touches the exact code a prior session changed to chase the moiré; make sure the moiré-fix (Phase 1) lands close behind so we don't sit on a known-buzzy build.

---

## 10. Open decisions for the owner

Plain-language calls only you should make:

1. **Live vocoder, yes or no?** The Smooth/Studio (vocoder) engine adds ~60 ms of delay. That's invisible on rendered/baked audio and in the pitch editor, but on a *live* singing-through-the-pedal monitor it's a noticeable lag. Option A: PSOLA is the only live engine, vocoder is offline/render-only. Option B: allow live vocoder with a "you'll hear extra delay" warning. Recommend A.

2. **Default engine per surface.** Pitch editor, Align, and the live pedal can each default to a different engine. Recommend: live pedal → PSOLA; pitch editor preview → PSOLA, render → user choice; Align → Vocoder. Confirm or adjust.

3. **How to label the choice to a first-time user.** These are people who've never made music. "PSOLA vs Phase Vocoder" is meaningless to them. Options: "Natural vs Smooth," "Live vs Studio," "Fast vs Best." Pick the pair of words.

4. **Character/"throat" control — expose it now or hide it?** The formant stage can also *deliberately* shift formants (make a voice sound bigger/smaller, more/less "chipmunk" on purpose). It's a creative toy but adds a knob. Ship it in V1 or defer?

5. **How far to chase transparency in V1.** The cepstral formant fix gets us "clearly good." The true-envelope upgrade gets us "Melodyne-transparent on high vocals" at real extra work. Is "clearly good" the V1 bar, with transparency as a later polish pass? Recommend yes.

6. **WORLD offline engine — on the roadmap or not?** Best possible monophonic quality, license-clean, but heavy and post-V1. Confirm it belongs in Future State, not V1.

---

**Bottom line:** revert the read-pointer change to get the shift back today (Phase 0), fix grain coherence to make PSOLA clean (Phase 1), wire the formant stage onto the vocoder to un-chipmunk it (Phase 2), then expose the two-engine choice (Phase 3). Every building block except the optional WORLD import already exists in the tree.

*(Two of the four research bundles suggested saving this to `Plans & Specs/Research Reports/daw-architecture-monophonic-vocal-pitch-shift-2026-07-12.md`. I did not write that file — returning the report here per instructions; the parent can persist it if wanted.)*

---

## Owner Decisions (Jeff, 2026-07-12)

1. **Live = PSOLA only.** Both PSOLA and Vocoder offered as options in the Pitch editor and in Align.
2. **Per-surface:** Pedal = PSOLA (it is PSOLA-based; live path). Editor defaults to PSOLA with a **toggle button in the button area** to swap PSOLA<->Vocoder. Align defaults to PSOLA and keeps its existing dropdown; **its third option is to be investigated** (fixed if broken) and, if usable, offered in the Pitch editor too.
3. **Engine labels:** "Fast (PSOLA)" and "Best (Vocoder)".
4. **Throat/character control ships in V1.**
5. **V1 transparency bar** -- RESOLVED: **full transparency for V1.** Implement the Roebel-Rodet iterative true-envelope estimator (superVP/Melodyne-class), not the cheap fixed-quefrency cepstral lifter -- so bright/high vocals stay transparent, not dulled.
6. **WORLD offline engine** -- RESOLVED: **add it** if license-clean (it is: modified-BSD, patent-free) AND it beats PSOLA / PV+formant. **Offline/render only** -- not usable during live recording, but must be usable immediately after. In V1 scope as the ultra quality third engine.


## Align engine investigation (2026-07-12)

Align dropdown `bsa_pitch_algo` = PSOLA / Granular / Phase Vocoder (`BaySickAlignEditor.cpp:818-820`). All three, with the one we had NOT already addressed (Granular) evaluated for possible use in the Pitch editor:

- **PSOLA (default, pitchAlgo 0):** shared `PsolaShifter` -- **currently broken** by the continuous-read-pointer change (no shift). Phase 0 fixes it here and in the editor + pedal simultaneously.
- **Phase Vocoder (pitchAlgo 2):** uses `PvShifter` (`PitchShifters.h:503`), a complete PhaseVocoder wrapper (stretch-by-ratio + resample). **Functions** -- correct pitch shift, offline-only (FFT latency), the exact recipe the Python prototype measured dead-on. Only limitation: **no formant preservation -> chipmunk** (Align's PV branch chains no `CepstralFormantEngine`). This IS "Engine B (Vocoder)"; it already exposes the same `processSample(in, ratio)` interface as PSOLA, so it drops into the pitch editor cleanly. Phase 2 chains the formant stage to un-chipmunk it (Align + Pitch).
- **Granular (pitchAlgo 1) -- the extra engine, evaluated:** separate `GranularShifter` (`PitchShifters.h:400`), fixed ~25 ms grains, 75% overlap, 4-6 concurrent, each grain resampled at `ratio` (`readPos += ratio`). Untouched by the change -> **works, shifts correctly.** But for vocals it is the weakest of the three: it **also chipmunks** (grain resampling shifts formants with pitch), and fixed non-pitch-synchronous grains add **warble** on sustained vowels. Its one real strength is **robustness** (no pitch/epoch detection -> survives breathy/whispered/noisy material where PSOLA's GCI detection struggles). **Verdict: keep it in Align as the robustness option; do NOT port to the Pitch editor** -- it is strictly worse than PSOLA (natural, formant-preserving) and PV+formant (smooth, formant-preserving) for vocals, and a third choice muddies the "Fast (PSOLA) / Best (Vocoder)" story for first-time users. (Owner decision.)

## Execution note
Phases 0+1 (restore the shift via mark-snap; kill the moire at its source with fixed 2-period GCI-centered sub-sample grains) require none of the above decisions and are the un-break/legitimize step for the flagship editor. Decisions above govern Phases 2-3 (vocoder formant wiring + the selectable-engine feature).
