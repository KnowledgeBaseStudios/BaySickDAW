# QA-VoicePool — Pre-allocated VibePlayer Voice Pool / Fat Voices / Lock-Free Stealing — Plan (tipsy-pulsing-octopus)

> **Canonical path:** `Plans & Specs/Batch Plans/tipsy-pulsing-octopus.md`
> Paired running notes: `Plans & Specs/Running Notes/tipsy-pulsing-octopus.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule). MT verify cadence Debug-then-Release per task per L10.

## Context

QA-VoicePool is the third batch in the QA-Eg-close-spawned perf-audit cluster (after QA-AudioMeters at `2cba7b7` + QA-InsertMaps at `3587ade`; before QA-EngineApvts). The cluster mass-attacks audio-thread hot-path inefficiencies surfaced by `/perf-audit` at QA-Eg close (2026-05-24). H3 = audio-thread heap allocation on every `VibeVoice::startNote` — fires on every drum hit, key press, audition gesture, etc.

The §5 entry (Main Plan §5:1201-1234) + §9 thirty-fourth Forks entry (Main Plan §9:4443-4502) carry Jeff's verbatim 4-section blueprint (pre-allocate in `prepareToPlay` + fat voices internal reuse + lock-free atomic occupancy + voice stealing with fade-out fallback). Plan-mode L1-L10 spec call surface (2026-05-25) locked the high-level approach.

**Critical pre-plan source finding** (drives L1 framework decision): VibePlayer is built on the `juce::Synthesiser` framework — NOT a custom voice pool. Existing state:
- [Source/VibePlayer/VibePlayerDSP.h:241](Source/VibePlayer/VibePlayerDSP.h:241) declares `static constexpr int kMaxVoices = 16;` — already matches Jeff's blueprint §1.
- [Source/VibePlayer/VibePlayerDSP.h:286](Source/VibePlayer/VibePlayerDSP.h:286) declares `juce::Synthesiser mSynth;`.
- [Source/VibePlayer/VibePlayerDSP.cpp:822-834](Source/VibePlayer/VibePlayerDSP.cpp:822) `VibeSynth::VibeSynth()` adds 16 `VibeVoice` instances to `mSynth` — voice pool ALREADY pre-allocated at engine-construct time.
- [Source/VibePlayer/VibePlayerDSP.cpp:899-906](Source/VibePlayer/VibePlayerDSP.cpp:899) same-pitch preemption (soft-stop via ADSR release) already implemented.
- [Source/VibePlayer/VibePlayerDSP.cpp:909-910](Source/VibePlayer/VibePlayerDSP.cpp:909) cut-self hard-stop already implemented.
- [Source/VibePlayer/VibePlayerDSP.cpp:917-919](Source/VibePlayer/VibePlayerDSP.cpp:917) voiceCap-based oldest-first stealing already implemented (via `mLastVoiceCap` + `mNoteStartCounter`).
- [Source/VibePlayer/VibePlayerDSP.cpp:919+](Source/VibePlayer/VibePlayerDSP.cpp:919) unison fan-out (N=1..8 voices per note-on) already implemented.

The voice pool itself ISN'T missing. The heap-alloc surface is INSIDE `VibeVoice::startNote`:
- [Source/VibePlayer/VibePlayerDSP.cpp:581-583](Source/VibePlayer/VibePlayerDSP.cpp:581) — `mMemSrc = std::make_unique<ReversedMemoryAudioSource>(...)` OR `std::make_unique<juce::MemoryAudioSource>(...)` per `mReverse` flag.
- [Source/VibePlayer/VibePlayerDSP.cpp:607](Source/VibePlayer/VibePlayerDSP.cpp:607) — `mResampSrc = std::make_unique<juce::ResamplingAudioSource>(mMemSrc.get(), false, 2);`.
- Three heap allocations per note-on. On a busy drum pattern (~32nd-note pace @ 130 bpm = ~17 hits/sec across 16 drums = ~270 hits/sec at peak) this surfaces measurable audio-thread pressure.

Plus a fourth allocation surface — [Source/VibePlayer/VibePlayerDSP.cpp:415-416](Source/VibePlayer/VibePlayerDSP.cpp:415) `std::vector<int> candidates; candidates.reserve(8);` inside `VibeSampleManager::findRegion()` — called once per note-on from `VibeVoice::startNote` at [:573](Source/VibePlayer/VibePlayerDSP.cpp:573). Reserve doesn't escape the heap-allocation hazard; the vector ctor still allocates.

L1=(a) approach: refactor INSIDE `juce::Synthesiser`. VibeVoice becomes fat (owns sources permanently), `startNote` re-points/resets instead of re-allocating. JUCE's framework keeps doing voice allocation + same-pitch preemption + cut-self + unison fan-out + voiceCap-stealing. Smaller scope, lower risk, preserves all existing renderNextBlock semantics. ("Fix the leaky pipe rather than replacing the plumbing" — Jeff, 2026-05-25.)

**Dependencies:** QA-InsertMaps closed (commit `3587ade`). The perf-audit cluster's flat-array migration settles before this batch's voice-lifecycle changes layer on top.

**Risk:** medium-high. Voice lifecycle + source re-pointing + `juce::ResamplingAudioSource` state reset between notes + ADSR-release self-deactivation + voice-stealing 64-sample fade-out. Worst case: voice mis-steal heard as wrong-note artifact (caught immediately by ear) OR ADSR self-deactivation fires before audible tail decays (premature voice cutoff, caught immediately by ear). No audio-thread allocation surface left post-batch.

**Effort estimate:** ~8-12 hours per §5 entry. Task 1 inventory + plan-finalize sub-spec calls may surface additional architectural decisions (see Sub-A / Sub-B / Sub-C below). Tasks 2 + 3 are the bulk; Task 4 is small; Task 5 verify + Task 6 cleanup + Task 7 close round out the cycle.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| L1 | Refactor INSIDE `juce::Synthesiser` framework. VibeVoice becomes fat (sources owned permanently). | Pre-batch source inventory confirmed the JUCE framework already has 16-voice pool + same-pitch preemption + cut-self + unison fan-out + voiceCap-stealing. Heap-alloc surface is inside `startNote`, not framework. "Fix the leaky pipe rather than replacing the plumbing." |
| L2 | Pool size = 16. | Matches existing `VibeSynth::kMaxVoices = 16` + Jeff's blueprint §1. |
| L3 | Sequencing = immediately after QA-InsertMaps, before QA-EngineApvts. | §6 sequencing arrow + §9 thirty-fourth Forks entry. Perf-audit cluster ordering. |
| L4 | Fat-voice source ownership = own BOTH forward + reverse sources as members; `startNote` picks which to feed to resampler. | "Memory is cheap; CPU is expensive." Pre-allocates both, no custom dual-mode DSP logic. |
| L5 | `findRegion` candidates allocation = stack-allocate `std::array<int, 32>`. | "Stack memory is the fastest possible option here." |
| L6 | Voice stealing fade-out = 64 samples (~1.5 ms at 44.1 kHz). | "10-20 samples is sub-millisecond and risks a zipper click. ~64 samples provides a much cleaner pro-audio de-click when stealing." |
| L7 | Voice stealing selection = hybrid release-phase-preferred fallback to overall-oldest. | "We never want to drop a new note. Prefer stealing a release-phase voice, but fall back to the overall oldest if none are releasing." |
| L8 | Task structure = SPLIT structural across 2 tasks (Task 2 fat voices / Task 3 lock-free occupancy + stealing + fade-out). | "Voice pooling is notoriously tricky; I want clean rollback boundaries between making the voices fat and implementing the lock-free occupancy/stealing." |
| L9 | Silly-name = `tipsy-pulsing-octopus`. | Claude's pick per `feedback_silly_name_is_my_pick.md`; Jeff confirmed "Lock-in tipsy-pulsing-octopus". |
| L10 | MT verify cadence = Debug-then-Release per task per QA-InsertMaps L5/L10 norm. | Standard. |

---

## Sub-spec calls surfaced for ExitPlanMode

These are tightly coupled to L1=(a)'s interpretation + Jeff's blueprint §3 wording. Resolve before Task 2/3 implementation lands. Plain-English numbered list per `feedback_design_approval_in_plain_english.md` extension (QA-InsertMaps Task 0 precedent).

### Sub-A — Atomic `isActive` flag under L1=(a)

Jeff's blueprint §3 specifies `std::atomic<bool> isActive{false}` per voice with `isActive.load()` for free-voice detection + `isActive.store(false)` set by the voice on ADSR-release-end. Under L1=(a) (keep `juce::Synthesiser` framework), JUCE already tracks voice-active state via `isVoiceActive()` / `getCurrentlyPlayingNote()` / `clearCurrentNote()`. The atomic flag is either redundant or supplemental.

- **(a)** Add the atomic flag per Jeff's blueprint §3. ADSR-release-end sets `mIsActive.store(false, release)`. Voice stealing's free-voice scan uses `mIsActive.load(acquire)` for cheap atomic check (no `dynamic_cast` loop). Explicit cross-system clarity + future-proofing if MT semantics ever change.
- **(b)** Rely on JUCE's existing voice-active state. Free-voice detection uses `mSynth.getVoice(i)->isVoiceActive()` + `dynamic_cast<VibeVoice*>` loop. ADSR-release-end calls `clearCurrentNote()` (existing pattern at [VibePlayerDSP.cpp:680-682](Source/VibePlayer/VibePlayerDSP.cpp:680)). Smaller surface change; one less atomic on the audio thread; literal blueprint §3 not implemented.

### Sub-B — fat-source ownership + resampler wiring model (REVISED 2026-05-25 post-Task-1 inventory)

**Task 1 inventory PIVOT:** `juce::ResamplingAudioSource::setSource()` does NOT exist — the input source pointer is fixed at construction (`OptionalScopedPointer<AudioSource> input;` private member at [juce_ResamplingAudioSource.h:90](juce/modules/juce_audio_basics/sources/juce_ResamplingAudioSource.h:90)). Public lifecycle methods are `setResamplingRatio` / `flushBuffers` / `prepareToPlay` / `releaseResources` / `getNextAudioBlock` only. The original Sub-B(a) framing (custom forward source class + own one of each; assumed the permanent resampler could be re-pointed) is invalid because we still can't re-target the resampler at the chosen direction at `startNote` without re-constructing it. The original Sub-B(b) JUCE-adapter path is also dead — `juce::MemoryAudioSource` has no setter either. Revised options:

- **(a) Custom forward source + dual permanent `juce::ResamplingAudioSource`** (one per direction). VibeVoice owns:
  - `VibeForwardMemoryAudioSource mForwardSrc` (custom ~30-line `juce::PositionableAudioSource` subclass with `setBuffer(juce::AudioBuffer<float>*)` + `setNextReadPosition` / `getNextAudioBlock` reading from the buffer pointer; pure forward delegation).
  - `ReversedMemoryAudioSource mReverseSrc` (existing class + new `setBuffer(juce::AudioBuffer<float>*)` member).
  - `juce::ResamplingAudioSource mForwardResamp` (permanent, ctor-bound to `&mForwardSrc`).
  - `juce::ResamplingAudioSource mReverseResamp` (permanent, ctor-bound to `&mReverseSrc`).
  - `juce::ResamplingAudioSource* mActiveResamp` (pointer to whichever direction is live this note).
  - At `startNote`: set `mActiveResamp` to the chosen direction's resampler, call `mActiveResamp->flushBuffers()` + `mActiveResamp->setResamplingRatio(ratio)`, set the chosen source's buffer pointer via `setBuffer()`.
  - Memory cost: ~2x ResamplingAudioSource state per voice (each resampler's internal `AudioBuffer<float> buffer` sized to `samplesPerBlockExpected * ratio` headroom — ~8-15 KB extra per voice × 16 voices = ~128-240 KB per engine instance). Not free but small relative to sample-data RAM.
  - Code complexity: 1 new small custom class (`VibeForwardMemoryAudioSource`, ~30 lines, pure delegation).

- **(b) Custom forward source + custom `VibeSourceFork` wrapper + single `juce::ResamplingAudioSource`**. VibeVoice owns:
  - `VibeForwardMemoryAudioSource mForwardSrc` (same ~30-line custom class as (a)).
  - `ReversedMemoryAudioSource mReverseSrc` (same existing-class extension as (a)).
  - `VibeSourceFork mFork` (NEW custom ~25-line `juce::PositionableAudioSource` subclass holding pointers to both `mForwardSrc` + `mReverseSrc` + a `bool mUseReverse` flag; `getNextAudioBlock` delegates to whichever is selected; `setNextReadPosition` / `prepareToPlay` / `releaseResources` forward to both).
  - `juce::ResamplingAudioSource mResampSrc` (permanent, ctor-bound to `&mFork`).
  - At `startNote`: set `mFork.setUseReverse(reverseMode)` + set the chosen source's buffer pointer, call `mResampSrc.flushBuffers()` + `mResampSrc.setResamplingRatio(ratio)`.
  - Memory cost: single resampler per voice (~half the resampler-state RAM of (a)).
  - Code complexity: 2 new small custom classes (`VibeForwardMemoryAudioSource` + `VibeSourceFork`, both pure delegation, zero DSP logic — explicitly NOT the "risky custom DSP logic" Jeff was avoiding when locking L4(a)).

- **(c)** Different approach Jeff proposes.

### Sub-C — Dummy buffer location for fat-source pre-allocation

The fat sources need a non-null buffer reference at construction time (before any sample is loaded). Where does the dummy buffer live?

- **(a)** Static `juce::AudioBuffer<float>` with 1 silent stereo sample, shared across all `VibeVoice` instances. Single source of truth; lifetime tied to first-use static initialization. ~6 bytes of shared state.
- **(b)** Per-`VibeVoice` instance member buffer (1 silent stereo sample). Each voice has its own; trivial allocation cost during voice ctor (16 voices x ~6 bytes = 96 bytes per engine instance).
- **(c)** No dummy buffer needed — write the custom forward source class (Sub-B(a)) to accept a nullptr internal buffer and short-circuit `getNextAudioBlock` until `setBuffer()` is called. Simplest if Sub-B(a) is picked.

---

## Files to modify

### Task 1 — Pre-flight inventory (read-only, docs only)
- [Source/VibePlayer/VibePlayerDSP.cpp](Source/VibePlayer/VibePlayerDSP.cpp) — full read; build complete VibeVoice / VibeSynth call-site map.
- [Source/VibePlayer/VibePlayerDSP.h](Source/VibePlayer/VibePlayerDSP.h) — full read; verify member layout matches Task 2's planned edits.
- [Source/VibePlayer/VibePlayerProcessor.cpp](Source/VibePlayer/VibePlayerProcessor.cpp) — confirm `prepareToPlay` chain reaches `VibeSynth::prepare`; verify no external readers of VibeVoice source internals.
- Grep across `Source/` for external readers of `mResampSrc` / `mMemSrc` / `mSampleBuffer` / `VibeVoice` ctor call sites.
- Verify `juce::ResamplingAudioSource` has `setSource()` + the lifetime semantics that allow re-pointing at the same source instance per note (vs ownership/deletion expectations).
- Verify `juce::ADSR` exposes an `isInRelease()`-style query OR what custom state we add for L7(b) hybrid stealing.
- Verify L5 `std::array<int, 32>` cap is sufficient — count typical region density per loaded SFZ / drum pack.
- Surface plan-finalize sub-spec calls (Sub-D / Sub-E etc.) discovered during inventory.
- Inventory result appended as table to running-notes file Task 1 entry.

### Task 2 — Fat-voice refactor (Jeff's blueprint §1 + §2)
- [Source/VibePlayer/VibePlayerDSP.h](Source/VibePlayer/VibePlayerDSP.h):
  - Replace `std::unique_ptr<juce::PositionableAudioSource> mMemSrc` (declaration around the existing :176 region) + `std::unique_ptr<juce::ResamplingAudioSource> mResampSrc` (:177) with the three fat members per Sub-B + L4(a) decisions.
  - Per Sub-B(a) decision (recommended): declare `VibeForwardMemoryAudioSource mForwardSrc;` + `ReversedMemoryAudioSource mReverseSrc;` + `juce::ResamplingAudioSource mResampSrc;` as direct members (no `unique_ptr` heap alloc).
  - Per Sub-C decision (recommended Sub-C(c) if Sub-B(a)): no dummy buffer needed if custom classes short-circuit on null internal buffer.
- [Source/VibePlayer/VibePlayerDSP.cpp](Source/VibePlayer/VibePlayerDSP.cpp):
  - Add internal `VibeForwardMemoryAudioSource` class (~30 lines, mirrors `juce::MemoryAudioSource` semantics + adds `setBuffer()`) inside the existing namespace block at [:461-513](Source/VibePlayer/VibePlayerDSP.cpp:461) alongside `ReversedMemoryAudioSource`.
  - Extend `ReversedMemoryAudioSource` with `void setBuffer(const juce::AudioBuffer<float>&)`.
  - `VibeVoice` ctor [:519-523](Source/VibePlayer/VibePlayerDSP.cpp:519) — adjust member initialization per Sub-C decision.
  - `VibeVoice::prepareForPlayback(int blockSize)` [:525-529](Source/VibePlayer/VibePlayerDSP.cpp:525) — call `mResampSrc.setSource(...)` + `mResampSrc.prepareToPlay(blockSize, mSampleRate)` ONCE on the message thread (this is where the per-note `prepareToPlay` call at [:609](Source/VibePlayer/VibePlayerDSP.cpp:609) gets hoisted to).
  - `VibeVoice::releaseResources()` [:534-540](Source/VibePlayer/VibePlayerDSP.cpp:534) — strip per-note resets; only clear playback flags + `mSampleBuffer` shared_ptr (release the buffer reference so loaded samples can eventually free).
  - `VibeVoice::startNote(int, float, ...)` [:566-645](Source/VibePlayer/VibePlayerDSP.cpp:566) — replace `make_unique` calls at :581-583 + :607 with `mForwardSrc.setBuffer(*mSampleBuffer)` OR `mReverseSrc.setBuffer(*mSampleBuffer)` + `mResampSrc.setSource(...)` (pick the chosen source) + `mResampSrc.setResamplingRatio(resampRatio)`. ResamplingAudioSource's prepareToPlay was hoisted to prepareForPlayback so no per-note call needed.
  - `VibeVoice::renderNextBlock` [:662+](Source/VibePlayer/VibePlayerDSP.cpp:662) — verify `mResampSrc.getNextAudioBlock` path still works post-refactor (no behavioral change expected; sources are switched but the resampler interface is unchanged).

### Task 3 — Lock-free occupancy + stealing + 64-sample fade-out (Jeff's blueprint §3 + §4)
- [Source/VibePlayer/VibePlayerDSP.h](Source/VibePlayer/VibePlayerDSP.h):
  - Per Sub-A decision: add `std::atomic<bool> mIsActive { false };` member (Sub-A(a)) OR omit + document the rely-on-JUCE-state choice (Sub-A(b)).
  - Add `bool mInRelease { false };` flag (set true on `stopNote(allowTailOff=true)`; cleared on note-end). Needed for L7(b) hybrid stealing.
  - Add fade-out state: `int mStealFadeOutSamplesLeft { 0 };` + `float mStealFadeOutGainStart { 1.0f };` — when stolen, set to 64 + 1.0; render-loop ramps from start to 0 over the remaining samples.
  - Add public `void initiateSteal() noexcept` method declaration.
- [Source/VibePlayer/VibePlayerDSP.cpp](Source/VibePlayer/VibePlayerDSP.cpp):
  - `VibeVoice::stopNote(allowTailOff=true)` [:648-659](Source/VibePlayer/VibePlayerDSP.cpp:648) — set `mInRelease = true` before `mAdsr.noteOff()`.
  - `VibeVoice::stopNote(allowTailOff=false)` same site — clear `mInRelease`.
  - `VibeVoice::startNote` [:566+](Source/VibePlayer/VibePlayerDSP.cpp:566) — at top of body, clear `mInRelease`; per Sub-A set `mIsActive.store(true, release)` if Sub-A(a).
  - `VibeVoice::renderNextBlock` ADSR-end path [:678-683](Source/VibePlayer/VibePlayerDSP.cpp:678) — clear `mInRelease`; per Sub-A set `mIsActive.store(false, release)` if Sub-A(a).
  - Implement `VibeVoice::initiateSteal()`: `mStealFadeOutSamplesLeft = 64; mStealFadeOutGainStart = 1.0f;` (idempotent: re-stealing an already-fading voice does nothing if already in fade-out).
  - Add fade-out application at top of `renderNextBlock` body (BEFORE existing ADSR application at [:676](Source/VibePlayer/VibePlayerDSP.cpp:676)) — when `mStealFadeOutSamplesLeft > 0`: apply linear ramp from current gain to next-block gain across the block's `numSamples`; decrement `mStealFadeOutSamplesLeft` by `min(numSamples, mStealFadeOutSamplesLeft)`; when hits 0, `clearCurrentNote()` + clear state. New note starts on next block's `startNote` call from JUCE's voice allocator.
  - Add `VibeSynth::findStealCandidate(int newPitch) -> VibeVoice*` private method implementing L7(b) hybrid: scan voices, prefer voice with `mInRelease == true` AND lowest `mNoteStartCounter` (= oldest-in-release); fallback to overall oldest `mNoteStartCounter` if no release-phase voice. Returns nullptr only if pool is empty (never happens post-Task-2).
  - Modify `VibeSynth::renderNextBlock` voiceCap-stealing branch [:917+](Source/VibePlayer/VibePlayerDSP.cpp:917) — when active voice count >= cap, call `findStealCandidate(note)` + `victim->initiateSteal()` + proceed with `mSynth.noteOn(...)` for the new note (JUCE's voice allocator will pick the fading-out voice as free once `clearCurrentNote()` fires at fade-out end; OR pick a different free voice if one is available — both paths sound clean since the stolen voice is fading to silence anyway).
  - **Note:** there's a subtle interaction with JUCE's `findFreeVoice()` — if we initiate steal but JUCE's allocator picks a DIFFERENT free voice (one already free from natural ADSR-end), the fade-out continues to completion on the stolen voice without interference. Audibly clean. This is OK — `initiateSteal` is a hint, not a guarantee that THIS voice will host the new note.

### Task 4 — `findRegion` candidates std::array stack-alloc (Jeff's blueprint addendum)
- [Source/VibePlayer/VibePlayerDSP.cpp:410-452](Source/VibePlayer/VibePlayerDSP.cpp:410) `VibeSampleManager::findRegion`:
  - Replace `std::vector<int> candidates; candidates.reserve(8);` at :415-416 with `std::array<int, 32> candidates {}; int numCandidates = 0;`.
  - Replace `candidates.push_back(i)` at :424 with `if (numCandidates < 32) candidates[numCandidates++] = i;`.
  - Adapt reads: `candidates.empty()` (:427) → `numCandidates == 0`; range-for `for (int idx : candidates)` (:431, :446) → indexed `for (int j = 0; j < numCandidates; ++j) { int idx = candidates[j]; ... }`.
  - `candidates[0]` reads (:435, :443, :451) unchanged.

### Task 5 — Stress-file verify (verify-only, no source commit)
- Verify-only against Jeff's existing big stress-test arrangement (same one used at QA-InsertMaps Task 3 + QA-AudioMeters Task 3).
- 10-point watchlist (see Verification section below).

### Task 6 — Cleanup / grep sweep
- `grep -rn "make_unique<juce::MemoryAudioSource>" Source/` → confirm zero.
- `grep -rn "make_unique<juce::ResamplingAudioSource>" Source/` → confirm zero (or only outside VibePlayer if other engines use it).
- `grep -rn "make_unique<ReversedMemoryAudioSource>" Source/` → confirm zero.
- `grep -rn "std::vector<int> candidates" Source/VibePlayer/` → confirm zero.
- Sweep stale comments referencing the per-note allocation pattern (e.g., comment at [VibePlayerDSP.cpp:594](Source/VibePlayer/VibePlayerDSP.cpp:594) "ResamplingAudioSource handles pitch shifting" — verify still accurate or rewrite).
- Sweep stale comments referencing pre-batch behavior (lifecycle path: `startNote → new MemoryAudioSource + new ResamplingAudioSource`).

### Task 7 — Close sequence
- `/draft-doc batch-close` → apply close entry to `Plans & Specs/Implemented Work Log.md`.
- `/review-batch QA-VoicePool` → address BLOCKERs / NEEDS-FIX in fix-up commit in-batch (NITs follow fix-or-reframe canon per `feedback_qa_batches_fix_bugs_dont_defer.md` — NEVER defer-to-close-entry-routing-table; that anti-pattern was overruled at QA-InsertMaps close per the same precedent).
- §5 STATUS banner update: insert `**STATUS (YYYY-MM-DD close):** **CLOSED.** <summary>` above the existing §5 entry per QA-InsertMaps precedent.
- Route side findings per Rule 3 (resolved-in-batch → close-entry routing table; outside-batch → §9 Forks entry + §5/§6 edits — surface placement options to Jeff, don't pick the slot).
- `/draft-commit` for close commit.

---

## Tasks

### Task 0 — Open commit (docs only)
- [ ] Mirror `~/.claude/plans/elegant-juggling-kay.md` → `Plans & Specs/Batch Plans/tipsy-pulsing-octopus.md` (Write tool; renaming on copy); delete the home-dir copy per `feedback_plan_mirror_one_way.md`.
- [ ] Update Main Plan §5 QA-VoicePool entry header — flip `**Plan file:** `<silly-name>.md (when started)`` placeholder to backticked `Plans & Specs/Batch Plans/tipsy-pulsing-octopus.md`.
- [ ] Seed `Plans & Specs/Running Notes/tipsy-pulsing-octopus.md` with title + purpose blockquote + pair file reference + convention reference per §0 running-notes required-sections. Include initial `## Diagnostic Instrumentation Catalog` skeleton (empty table) per §0 Rule 4.
- [ ] Surface full git status (incl. pre-existing untracked `Templates/My Templates/` surface-and-leave). Dispatch `/draft-commit`. Surface drafted message + git status to Jeff. Commit on approval.
- [ ] Mark Task 0 done.

### Task 1 — Pre-flight inventory (read-only, docs only)
- [ ] Read [VibePlayerDSP.cpp](Source/VibePlayer/VibePlayerDSP.cpp) end-to-end. Build complete inventory table (every site that touches `VibeVoice` lifecycle, `mResampSrc` / `mMemSrc` / `mSampleBuffer`, `findRegion`, `clearCurrentNote`, `releaseResources`).
- [ ] Read [VibePlayerDSP.h](Source/VibePlayer/VibePlayerDSP.h) end-to-end. Verify member layout matches Task 2's planned edits.
- [ ] Read [VibePlayerProcessor.cpp](Source/VibePlayer/VibePlayerProcessor.cpp) — confirm `prepareToPlay` chain reaches `VibeSynth::prepare`; verify no external readers of VibeVoice source internals.
- [ ] Grep across `Source/` for external readers of `mResampSrc` / `mMemSrc` / `mSampleBuffer`.
- [ ] Verify `juce::ResamplingAudioSource::setSource()` lifetime semantics — does it expect ownership? Will repeated calls with the same source pointer + different ratios work as the fat-voice pattern requires?
- [ ] Verify `juce::ADSR` exposes an `isInRelease()`-style query OR confirm the explicit `mInRelease` flag is the right tracking layer for L7(b) hybrid stealing.
- [ ] Count typical region density per loaded SFZ / drum pack — verify L5 `std::array<int, 32>` cap is sufficient. Surface as plan-finalize sub-spec if 32 turns out to be too tight.
- [ ] Verify the `juce::Synthesiser::findFreeVoice()` + `findVoiceToSteal()` interaction with the new fade-out state — JUCE may consider a fading-out voice "active" and skip it, OR may consider it "free" and try to allocate it. Document the actual behavior.
- [ ] Surface plan-finalize sub-spec calls discovered during inventory (Sub-D / Sub-E etc.). Resolve with Jeff BEFORE Task 2 lands.
- [ ] Dispatch `/draft-doc running-notes` and apply to running-notes file (Task 1 entry: scope note + inventory table + asymmetry findings + plan-finalize sub-spec call surface).
- [ ] Dispatch `/draft-commit`. Surface message + full git status. Commit on approval.

### Task 2 — Fat-voice refactor (Jeff's blueprint §1 + §2)
- [ ] Add internal `VibeForwardMemoryAudioSource` class per Sub-B(a) inside the existing namespace block at [VibePlayerDSP.cpp:461-513](Source/VibePlayer/VibePlayerDSP.cpp:461) alongside `ReversedMemoryAudioSource`.
- [ ] Extend `ReversedMemoryAudioSource` with `void setBuffer(const juce::AudioBuffer<float>&)` setter.
- [ ] Update `VibeVoice` member declarations in [VibePlayerDSP.h](Source/VibePlayer/VibePlayerDSP.h) per Sub-B + L4(a) + Sub-C decisions.
- [ ] Update `VibeVoice` ctor per Sub-C decision.
- [ ] Update `prepareForPlayback(int)` to hoist `mResampSrc.setSource(...)` + `mResampSrc.prepareToPlay(...)` to message-thread setup time (move from per-note `startNote`).
- [ ] Rewrite `startNote` :566-645: replace `make_unique` calls at :581-583 + :607 with `mForwardSrc.setBuffer(*mSampleBuffer)` OR `mReverseSrc.setBuffer(*mSampleBuffer)` + `mResampSrc.setSource(picked-source-ptr)` + `mResampSrc.setResamplingRatio(resampRatio)` re-pointing pattern.
- [ ] Strip per-note resets from `releaseResources()` :534-540; only clear playback flags + `mSampleBuffer` shared_ptr.
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug: load a sample / drum pack on a VibePlayer engine. Test:
  - (1) Play a sustained chord (16 voices) — verify all notes sound, no dropouts.
  - (2) Tap individual notes rapidly — verify each note triggers cleanly, no allocation hiccups.
  - (3) Toggle Reverse on a loaded sample, play notes — verify reverse playback sounds correct.
  - (4) Toggle Reverse OFF, play notes — verify forward playback restored.
  - (5) Swap sound on the VibePlayer (engine swap mid-playback) — verify voice pool teardown + new sample load works.
  - (6) Run for 1-2 minutes of mixed playback — verify no audio dropout, audio sounds identical to pre-batch."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: dispatch `/draft-commit`. Surface message + git status. Commit on approval.
- [ ] Dispatch `/draft-doc running-notes` and apply to running-notes file.

### Task 3 — Lock-free occupancy + stealing + 64-sample fade-out (Jeff's blueprint §3 + §4)
- [ ] Per Sub-A decision: add `std::atomic<bool> mIsActive { false }` member to VibeVoice (Sub-A(a)) OR document the rely-on-JUCE choice (Sub-A(b)).
- [ ] Add `bool mInRelease { false }` + fade-out state (`int mStealFadeOutSamplesLeft { 0 }; float mStealFadeOutGainStart { 1.0f };`).
- [ ] Update `startNote` to clear `mInRelease` + set `mIsActive` (if Sub-A(a)) at top of body.
- [ ] Update `stopNote(allowTailOff=true)` to set `mInRelease = true` before `mAdsr.noteOff()`.
- [ ] Update `stopNote(allowTailOff=false)` to clear `mInRelease`.
- [ ] Update `renderNextBlock` ADSR-end path :678-683 to clear `mInRelease` + `mIsActive` (if Sub-A(a)).
- [ ] Implement `VibeVoice::initiateSteal()` (idempotent: skips if already mid-fade-out).
- [ ] Add fade-out application at top of `renderNextBlock` body BEFORE existing ADSR application at :676 — linear gain ramp from current to next-block-gain across `numSamples`; decrement counter; call `clearCurrentNote()` at counter-hits-zero.
- [ ] Add `VibeSynth::findStealCandidate(int newPitch) -> VibeVoice*` private method implementing L7(b) hybrid.
- [ ] Modify `VibeSynth::renderNextBlock` voiceCap-stealing branch :917+ — replace the existing oldest-first logic with `findStealCandidate` + `initiateSteal` + new-note routing through JUCE's allocator.
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug:
  - (1) Play a sustained 16-note chord (= kMaxVoices) on a VibePlayer.
  - (2) Play the 17th note. Verify NO click on voice steal; listen for the 64-sample fade-out (~1.5 ms — subtle but audible if you're listening for it).
  - (3) Set voiceCap=4 (via the polyphony context menu). Play 5+ notes in quick succession with quick releases. Verify oldest-in-release voice gets stolen first.
  - (4) Hold 4 notes sustained (no release). Play a 5th — verify steal falls back to overall-oldest.
  - (5) Test in MT (default) AND 1-worker serial-diagnostic mode — voice behavior identical."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: dispatch `/draft-commit`. Surface message + git status. Commit on approval.
- [ ] Dispatch `/draft-doc running-notes` and apply.

### Task 4 — `findRegion` candidates stack-alloc (Jeff's blueprint addendum)
- [ ] Edit [VibePlayerDSP.cpp:410-452](Source/VibePlayer/VibePlayerDSP.cpp:410) `VibeSampleManager::findRegion`:
  - Replace `std::vector<int> candidates; candidates.reserve(8);` with `std::array<int, 32> candidates {}; int numCandidates = 0;`.
  - Replace push_back with bounded indexed write.
  - Replace empty-check + range-for + indexed reads per the Files section above.
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug:
  - (1) Load a heavily-layered SFZ (one with 32+ regions per note if available, or stack multiple drum packs into one folder).
  - (2) Play notes across the keymap. Verify region lookup works correctly (no crash on overflow; round-robin cycles correctly).
  - (3) Tap an audition button. Verify single-region lookup still works.
  - (4) Verify no behavioral change vs pre-Task-4 (this is a transparent perf optimization)."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: dispatch `/draft-commit`. Surface message + git status. Commit on approval.
- [ ] Dispatch `/draft-doc running-notes` and apply.

### Task 5 — Stress-file verify (verify-only, no source commit per L8 split structure)
- [ ] Tell Jeff: "Run `do_build.bat` (Release + Debug clean from Task 4). Open the big stress-test arrangement used at QA-InsertMaps Task 3 / QA-AudioMeters Task 3. Walk the 10-point watchlist below:
  - (1) Play long enough to trigger voice stealing on busy drum / chord patterns. No clicks. No mis-stolen notes.
  - (2) Tap audition gestures while pattern is playing. No allocation hiccups; audio thread stays smooth.
  - (3) Toggle Reverse on a VibePlayer mid-pattern. Reverse sound plays correctly.
  - (4) Switch sound on a VibePlayer mid-pattern (engine swap). Voice pool teardown + new sample load clean.
  - (5) Save + reload project. Voice pool initializes correctly on load.
  - (6) Audition + sustain across multiple VibePlayer engines simultaneously (Layer + Bass + Drums tabs each playing their own VibePlayer pattern).
  - (7) MT-on (production default) vs MT-off (1-worker serial-diagnostic mode) — identical voice behavior + identical CPU profile.
  - (8) Run for 5+ minutes of mixed playback. No audio dropout. No memory growth in Task Manager (i.e., no leak from the new fat-source / occupancy flag pattern).
  - (9) Test polyphony context menu's voiceCap slider — verify cap takes effect, stealing respects the new cap.
  - (10) Test unison fan-out (set unison voices > 1) — verify unison still works correctly under the new fat-voice + stealing patterns."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: NO commit (verify-only per L8 split structure mirror of QA-InsertMaps Task 3).
- [ ] Dispatch `/draft-doc running-notes` and apply.

### Task 6 — Cleanup / grep sweep
- [ ] `grep -rn "make_unique<juce::MemoryAudioSource>" Source/` — confirm zero (or only outside VibePlayer if other engines use this pattern; investigate any matches).
- [ ] `grep -rn "make_unique<juce::ResamplingAudioSource>" Source/` — confirm zero in VibePlayer (or only outside).
- [ ] `grep -rn "make_unique<ReversedMemoryAudioSource>" Source/` — confirm zero.
- [ ] `grep -rn "std::vector<int> candidates" Source/VibePlayer/` — confirm zero.
- [ ] Sweep stale comments referencing the per-note allocation pattern. Stale comment candidates: [VibePlayerDSP.cpp:594](Source/VibePlayer/VibePlayerDSP.cpp:594) "ResamplingAudioSource handles pitch shifting" + the lifecycle path doc at [VibePlayerDSP.h:107](Source/VibePlayer/VibePlayerDSP.h:107) "startNote → find region → create MemoryAudioSource + ResamplingAudioSource".
- [ ] Walk Diagnostic Instrumentation Catalog (running notes section). Surface `Remove` entries to Jeff for approval BEFORE stripping per §0 Rule 4.
- [ ] Tell Jeff: "Run `do_build.bat`. Confirm Release + Debug both clean. Brief play in Debug — no regression."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: dispatch `/draft-commit`. Surface message + git status. Commit on approval.
- [ ] Dispatch `/draft-doc running-notes` and apply.

### Task 7 — Close sequence
- [ ] Dispatch `/draft-doc batch-close` with synthesis of running-notes file (Task 0-6 entries + Diagnostic Instrumentation Catalog).
- [ ] Apply close entry to `Plans & Specs/Implemented Work Log.md` via Edit.
- [ ] Dispatch `/review-batch QA-VoicePool`.
- [ ] Address BLOCKERs / NEEDS-FIX in fix-up commit (in-batch). NITs follow fix-or-reframe canon per `feedback_qa_batches_fix_bugs_dont_defer.md` — fix concretely OR reframe as accepted design; NEVER defer to close-entry routing table (QA-InsertMaps close-pass anti-pattern overruled by Jeff 2026-05-25; extends QA-D mid-batch canon).
- [ ] §5 STATUS banner update: insert `**STATUS (YYYY-MM-DD close):** **CLOSED.** <summary>` above the existing §5 QA-VoicePool entry per QA-InsertMaps / QA-AudioMeters precedent.
- [ ] Route side findings per Rule 3 (resolved-in-batch → close-entry routing table; outside-batch → §9 Forks entry + §5/§6 edits — surface placement options to Jeff per `feedback_slot_placement_is_spec_call.md`, don't pick the slot).
- [ ] Surface full git status.
- [ ] Dispatch `/draft-commit` for close commit. Surface message + status. Commit on approval (separate from source commits for clean rollback boundary).

---

## Verification (end-to-end smoke)

After Task 6 lands:
1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **No heap alloc per note-on.** `grep` confirms zero remaining `make_unique<>` for `juce::MemoryAudioSource` / `juce::ResamplingAudioSource` / `ReversedMemoryAudioSource` in `Source/VibePlayer/`.
3. **No heap alloc per findRegion.** `grep` confirms zero `std::vector<int> candidates` in `Source/VibePlayer/`.
4. **16-voice polyphony works.** Sustained chord, no dropouts; audio identical to pre-batch.
5. **Voice stealing clean.** 17+ note chord, no clicks at voice steal, 64-sample fade-out audible if listening for it.
6. **Hybrid stealing logic correct.** Release-phase voice stolen first when available; falls back to overall-oldest when no release-phase voices.
7. **Reverse mode works.** Toggle Reverse, play notes, verify reverse playback identical to pre-batch.
8. **Engine swap works.** Swap sound mid-pattern, voice pool teardown + sample reload clean.
9. **Project save+load works.** Voice pool initializes correctly post-load.
10. **MT parity.** MT (default) + 1-worker serial-diagnostic mode → identical voice behavior + CPU profile.
11. **No memory growth.** 5+ min playback, no Task Manager memory creep.

---

## Routing notes (Rule 3 application during execution)

- Findings about other VibePlayer DSP issues (filter, ADSR, drive, treble shelf etc.) → out-of-scope; surface as §9 Forks entry candidates if discovered + route per Jeff's slot pick.
- Findings about ResamplingAudioSource state issues (if `setSource()` re-pointing requires state reset across notes) → expand Task 2 scope inline; record in close entry's routing table.
- Findings about ADSR-release-phase tracking gaps (if `juce::ADSR` lacks the API we need) → custom state tracking in Task 3 via the new `mInRelease` flag; surface as plan-finalize sub-spec call if it changes Task 3's design.
- Findings about per-engine voice-pool sharing (multiple VibePlayer instances each with 16 voices = ~448 voices total across busy projects; ~12 KB of per-engine source state) → out-of-scope for this batch; surface as Future State / §9 Forks if memory pressure observed.
- Findings about `juce::Synthesiser::findFreeVoice()` interaction with fading-out voices → resolved in Task 1 inventory.
- Findings about external readers of VibeVoice source internals (e.g., visualization, metering) → resolved in Task 1 inventory grep.

---

## Carry-Forward Reference touch points

- **Task 1 (inventory):** read Carry-Forward §1 (Render Engine Primitives) — VibePlayer is NOT enumerated in §1's file:line index (which covers MT render path primitives, not engine internals). No direct contradiction at the §1 architectural-primitive level. Per-batch impact: `VibeVoice::startNote` heap-alloc behavior is implicitly documented via the function reference but not architecturally locked in §1.
- **Task 2 (fat-voice):** no Carry-Forward contradiction at the architectural level. The fat-voice pattern is additive — VibeVoice's external interface (`startNote` / `stopNote` / `renderNextBlock` / `releaseResources`) is unchanged.
- **Task 3 (occupancy + stealing):** no Carry-Forward contradiction. New private members + private methods inside VibeVoice / VibeSynth; external interface unchanged.
- **Task 7 (close entry):** record "no Carry-Forward §1 architectural contradictions" + note the implicit `VibeVoice::startNote` allocation-pattern documentation is now stale (sources owned permanently). Per `feedback_closed_batch_carryforward_via_forks.md` Carry-Forward Reference is FROZEN; this is documented in the Work Log close entry's "Carry-forward contradictions" section, not in Carry-Forward itself.
