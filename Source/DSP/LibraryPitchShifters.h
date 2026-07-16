#pragma once
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
// LibraryPitchShifters - QA-Fe Task 2 (2026-07-13)
// ─────────────────────────────────────────────────────────────────────────────
// Offline pitch-shift bake seam over the three vendored library engines that
// replace the retired PSOLA/Granular shifters on the editor + Align paths:
//   * Rubber Band R3  (BAYSICK_HAS_RUBBERBAND) - "Balanced" (default)
//   * Signalsmith     (BAYSICK_HAS_SIGNALSMITH) - "Lightest (Low CPU)"
//   * WORLD           (BAYSICK_HAS_WORLD)       - "Highest Quality (High CPU)"
//
// bake() does a LENGTH-PRESERVED mono shift with a per-sample pitch-ratio
// envelope + a per-sample formant/throat scale.  The library engines are
// block/whole-buffer (not per-sample like PSOLA), so a time-varying pitch is fed
// per block/frame inside each implementation.
//
// THREADING: bake() ALLOCATES and is heavy (WORLD especially: a ~3-min clip
// transiently needs hundreds of MB) -- WORKER / MESSAGE THREAD ONLY, never the
// audio thread.  The live edit-during-playback path (Rubber Band + Signalsmith
// streamed under the playhead on the audio thread, QA-Fe Option 3) is a separate
// component; WORLD has no live path and always bakes.
// ─────────────────────────────────────────────────────────────────────────────

// Engine ids -- match the bsp_engine / bsa_pitch_algo param values (QA-Fe B1/B2).
enum class PitchEngine { RubberBand = 0, Signalsmith = 1, World = 2 };

class IPitchShifter
{
public:
    virtual ~IPitchShifter() = default;

    // Shift `n` mono samples (`in`, sample rate `sr`) into `out` (length == n),
    // formant-preserving.  pitchRatio[i] = 2^(semis/12) (> 0).  formantScale[i]
    // = independent formant multiplier (1.0 = neutral; > 1 raises formants /
    // "smaller throat", < 1 lowers / "bigger throat").  Either envelope may be
    // nullptr => treated as all-1.0.  `in` and `out` MUST NOT alias.
    virtual void bake (const float* in, float* out, int n, double sr,
                       const float* pitchRatio, const float* formantScale) = 0;

    // QA-Fe Option A: time-warp AND pitch in ONE native pass, replacing the old
    // "Align applyWarp pre-warps, engine re-pitches" split (whose phase vocoder
    // wobbled on the pitch editor's sharp per-note map).  Reads `nIn` SOURCE
    // samples and produces `nOut` EDITED-timeline samples.  srcPosForOut[j] =
    // the (fractional) SOURCE sample index that edited output sample j reads
    // from -- monotone within a run; a backward step marks a detach-pill cut the
    // engine resets across (a clean splice, not a bend).  pitchRatio/formantScale
    // are indexed on the OUTPUT (edited) timeline (length nOut); either may be
    // nullptr => all-1.0.  Streaming engines (Rubber Band / Signalsmith) stretch
    // as they read; WORLD re-times its own analysis frames.  `in` and `out` MUST
    // NOT alias.
    virtual void bakeWarped (const float* in, int nIn, float* out, int nOut, double sr,
                             const double* srcPosForOut,
                             const float* pitchRatio, const float* formantScale) = 0;

    virtual const char* name() const noexcept = 0;
};

// Returns nullptr if the requested engine isn't compiled in (BAYSICK_HAS_*).
std::unique_ptr<IPitchShifter> makePitchShifter (PitchEngine engine);
