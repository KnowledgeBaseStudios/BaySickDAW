# Running Notes — QA-VoicePool (tipsy-pulsing-octopus)

> Append-only execution log for QA-VoicePool. Each entry captures the state at a checkpoint trigger (post-commit / post-sub-task verify / post-finding / post-spec-call / post-scope-pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Append-only — never edit prior entries; new findings surface as new entries.

**Pair file:** `Plans & Specs/Batch Plans/tipsy-pulsing-octopus.md`
**Convention reference:** Main Plan §0 "Document Formatting Conventions" + "Plan file + Running Notes required sections" (locked 2026-05-11) + Rule 4 Diagnostic Instrumentation Catalog (locked 2026-05-12).

---

## Diagnostic Instrumentation Catalog

Per §0 Rule 4: every diagnostic addition (`DBG` / `juce::Logger::writeToLog` / temp `jassert` added for diagnosis / debug `juce::AlertWindow` popups / temp file logging / `std::cout`-style traces) gets logged here in the same edit pass as the source change. At task/batch close, walk this table and strip every `Remove` entry from source — surface the strip list to Jeff BEFORE running the strip pass.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| _(none yet)_ | | | |

---

## 2026-05-25 — Task 0 — Open

Open commit (docs only). Plan-mode session converted §9 thirty-fourth Forks entry's Jeff-verbatim 4-section blueprint + §5 entry's scope into the canonical batch plan file [tipsy-pulsing-octopus.md](../Batch Plans/tipsy-pulsing-octopus.md). Ten high-level spec calls (L1-L10) locked at ExitPlanMode:

- **L1 = (a)** Refactor INSIDE `juce::Synthesiser` framework. Pre-plan inventory surfaced that the framework already has 16-voice pool + same-pitch preemption + cut-self + unison fan-out + voiceCap-stealing. Heap-alloc surface is inside `VibeVoice::startNote`, not framework. Jeff: "Fix the leaky pipe rather than replacing the plumbing."
- **L2 = 16** voices (matches existing `kMaxVoices` + Jeff's blueprint §1).
- **L3 = after QA-InsertMaps, before QA-EngineApvts** (perf-audit cluster ordering per §6 + §9 thirty-fourth Forks entry).
- **L4 = (a)** VibeVoice owns BOTH forward + reverse sources as members; startNote picks which to feed to resampler. Jeff: "Memory is cheap; CPU is expensive."
- **L5 = (a)** `std::array<int, 32>` stack-alloc for `findRegion` candidates. Jeff: "Stack memory is the fastest possible option here."
- **L6 = (d) 64 samples** fade-out duration. Jeff: "10-20 samples is sub-millisecond and risks a zipper click. ~64 samples provides a much cleaner pro-audio de-click when stealing."
- **L7 = (b)** Hybrid stealing: prefer release-phase oldest, fallback to overall-oldest. Jeff: "We never want to drop a new note."
- **L8 = (b) SPLIT** structural across 2 tasks (Task 2 fat voices / Task 3 lock-free occupancy + stealing + fade-out). Jeff: "Voice pooling is notoriously tricky; I want clean rollback boundaries between making the voices fat and implementing the lock-free occupancy/stealing."
- **L9** = silly-name `tipsy-pulsing-octopus` (Claude's pick per `feedback_silly_name_is_my_pick.md`; Jeff locked).
- **L10** = MT verify cadence Debug-then-Release per task per QA-InsertMaps L5/L10 norm.

Three sub-spec calls surfaced in the plan file for resolution before Task 2 lands: **Sub-A** (atomic `isActive` flag under L1=(a) — redundant with JUCE state or supplemental?), **Sub-B** (`juce::MemoryAudioSource` fat-mode implementation — custom class vs adapter), **Sub-C** (dummy buffer location for fat-source pre-allocation). To be resolved post-Task-1-inventory.

Critical pre-plan source finding: VibePlayer uses `juce::Synthesiser` framework (NOT custom pool). `kMaxVoices = 16` + 16-voice pool already pre-allocated at engine-construct time. Per-note heap allocations are at [VibePlayerDSP.cpp:581-583](../../Source/VibePlayer/VibePlayerDSP.cpp:581) (`make_unique<ReversedMemoryAudioSource>` OR `make_unique<juce::MemoryAudioSource>`) + [:607](../../Source/VibePlayer/VibePlayerDSP.cpp:607) (`make_unique<juce::ResamplingAudioSource>`). Plus fourth allocation at [:415](../../Source/VibePlayer/VibePlayerDSP.cpp:415) `std::vector<int> candidates` in `findRegion`.

**Outside-scope working tree:** `Templates/My Templates/` untracked user artifact pre-dating QA-Eg; surface-and-leave per QA-Eg / QA-AudioMeters / QA-InsertMaps precedent; not staged.

**Rule 4 Diagnostic Instrumentation Catalog:** nil for Task 0 (docs-only commit, no source instrumentation added).

**Next action:** Task 1 pre-flight inventory (read-only) — full `VibePlayerDSP.cpp` + `.h` + `VibePlayerProcessor.cpp` reads, external-reader grep, `juce::ResamplingAudioSource::setSource()` semantics verify, `juce::ADSR` is-in-release query verify, `std::array<int, 32>` cap sufficiency verify, JUCE `findFreeVoice` / `findVoiceToSteal` interaction with new fade-out state verify. Surface plan-finalize sub-spec calls before Task 2 lands.

---

## 2026-05-25 — Task 1 — Pre-flight inventory (read-only)

Read-only inventory pass per L8(b) Task 1 scope + L11/Sub-F pattern carried over from QA-InsertMaps' inventory-first discipline. No source touched. Direct reads of `Source/VibePlayer/VibePlayerDSP.cpp` + `VibePlayerDSP.h` + `VibePlayerProcessor.cpp` + `VibePlayerProcessor.h`, plus targeted JUCE-header reads at `juce/modules/juce_audio_basics/sources/juce_ResamplingAudioSource.h` + `juce/modules/juce_audio_basics/utilities/juce_ADSR.h` to verify the API assumptions L4 / L7(b) / Sub-B were built on. Output is the Sub-B PIVOT below (the load-bearing finding) + several plan-assumption confirmations + the external-reader sweep that proves the fat-voice refactor is fully internally contained.

### Section 1 — Scope confirmation (sources verified, every §5 + §9-thirty-fourth-Forks claim checked against current post-QA-InsertMaps source)

- Heap-alloc surface 1 of 3 in `VibeVoice::startNote`: [VibePlayerDSP.cpp:581](../../Source/VibePlayer/VibePlayerDSP.cpp:581) `make_unique<ReversedMemoryAudioSource>(*mSampleBuffer)` — CONFIRMED reverse-mode allocation per-note-on.
- Heap-alloc surface 2 of 3: [VibePlayerDSP.cpp:583](../../Source/VibePlayer/VibePlayerDSP.cpp:583) `make_unique<juce::MemoryAudioSource>(*mSampleBuffer, false, false)` — CONFIRMED forward-mode allocation per-note-on.
- Heap-alloc surface 3 of 3: [VibePlayerDSP.cpp:607](../../Source/VibePlayer/VibePlayerDSP.cpp:607) `make_unique<juce::ResamplingAudioSource>(mMemSrc.get(), false, 2)` — CONFIRMED per-note-on resampler allocation.
- Heap-alloc surface 4 (region-finder): [VibePlayerDSP.cpp:415-416](../../Source/VibePlayer/VibePlayerDSP.cpp:415) `std::vector<int> candidates; candidates.reserve(8);` — CONFIRMED. Sized by L5(a) std::array<int, 32> stack-alloc replacement.
- Voice-pool baseline: [VibePlayerDSP.cpp:822-834](../../Source/VibePlayer/VibePlayerDSP.cpp:822) — `VibeSynth::VibeSynth()` ctor adds 16 `VibeVoice` instances to `mSynth` via `mSynth.addVoice(new VibeVoice(mManager))`. CONFIRMED the L1=(a)-locked finding that the 16-voice pool already exists; this batch's job is making each pool voice fat, not building a parallel pool.
- Pool size: [VibePlayerDSP.h:241](../../Source/VibePlayer/VibePlayerDSP.h:241) `static constexpr int kMaxVoices = 16;` — CONFIRMED matches L2 lock + Jeff's blueprint §1.
- Same-pitch preemption already in framework: [VibePlayerDSP.cpp:899-906](../../Source/VibePlayer/VibePlayerDSP.cpp:899) — `VibeSynth::noteOn` walks `mSynth.getNumVoices()`, finds any active voice with matching pitch, calls `v->stopNote(0.f, true)` for ADSR-release soft-stop. CONFIRMED — this is NOT re-implementation territory for QA-VoicePool; Task 3's L7(b) hybrid stealing only replaces the voiceCap oldest-first block, not this preemption pass.
- Cut-self path: [VibePlayerDSP.cpp:909-910](../../Source/VibePlayer/VibePlayerDSP.cpp:909) `if (mCutSelf) mSynth.allNotesOff(0, false);` — CONFIRMED.
- Unison fan-out: [VibePlayerDSP.cpp:913+](../../Source/VibePlayer/VibePlayerDSP.cpp:913) `const int N = juce::jlimit(1, 8, mUnisonVoices);` — CONFIRMED unison range is **1..8**, NOT the 4-way I described in the Task 0 commit message (minor inaccuracy, no impact on the plan). Each unison voice consumes a separate pool slot via the synthesiser's normal voice allocation.
- voiceCap-based oldest-first stealing block: [VibePlayerDSP.cpp:944-960](../../Source/VibePlayer/VibePlayerDSP.cpp:944) — scans `mSynth.getNumVoices()`, finds the oldest by `mNoteStartCounter` among active voices, calls `oldest->stopNote(0.f, false)` (hard stop, no tail). CONFIRMED — THIS is the block Task 3's L7(b) hybrid stealing replaces (selection logic upgraded to prefer release-phase oldest with fallback to overall-oldest + L6 64-sample fade-out added on the chosen victim).

**External-reader sweep** (the "fully internally contained" check): grep across `Source/` for `mResampSrc` / `mMemSrc` / `mSampleBuffer` returns matches ONLY in `Source/VibePlayer/VibePlayerDSP.cpp` + `Source/VibePlayer/VibePlayerDSP.h`. Zero hits in PluginProcessor / VibeGraph / Standalone pages / other engines / Engine/Tasks/. Fat-voice refactor is fully internally contained — no cross-file API breakage, no caller-side churn. Cross-engine `VibeSynth` references at [BaySickSolsticeSynth.cpp:6](../../Source/BaySickSolstice/BaySickSolsticeSynth.cpp:6) + [BaySickSynthDSP.cpp:7](../../Source/BaySickSynth/BaySickSynthDSP.cpp:7) are COMMENTS ONLY (design-pattern back-references documenting the placeholder-sample-rate-before-addVoice pattern, not actual code dependencies).

### Section 2 — JUCE API findings (LOAD-BEARING — drives the Sub-B pivot below)

Direct read of `juce/modules/juce_audio_basics/sources/juce_ResamplingAudioSource.h` + `juce/modules/juce_audio_basics/utilities/juce_ADSR.h`. Three findings, one of them invalidates the plan's original Sub-B(a) framing:

- **`juce::ResamplingAudioSource::setSource()` does NOT exist.** The input source pointer is fixed at construction. Internal member is `OptionalScopedPointer<AudioSource> input;` (private, line 90 of `juce_ResamplingAudioSource.h`). The public lifecycle surface is exactly: `ResamplingAudioSource(AudioSource* input, bool deleteWhenRemoved, int numChannels)` ctor / `setResamplingRatio(double)` / `getResamplingRatio()` / `flushBuffers()` / `prepareToPlay(int, double)` / `releaseResources()` / `getNextAudioBlock(const AudioSourceChannelInfo&)`. **This invalidates the plan's original Sub-B(a) wording** ("Write a small custom `VibeForwardMemoryAudioSource` class with `setBuffer(juce::AudioBuffer<float>*)`. VibeVoice owns one instance of each. Pairs cleanly with L4(a).") — that alone is insufficient because we still can't re-point the resampler at the chosen direction at startNote without re-constructing the resampler itself. Sub-B must be revised; see Section 3.

- `juce::ResamplingAudioSource::flushBuffers()` EXISTS — clears resampler internal state (the per-channel filter histories, the readahead buffer position). Safe to call per-note as the inter-note reset hook. This is the per-note "back to a clean slate" call regardless of which Sub-B variant we pick.

- `juce::ResamplingAudioSource::setResamplingRatio(double)` EXISTS — can be called anytime; not bound to construction. Per-note ratio change (different pitch → different ratio) is the supported pattern.

- `juce::ResamplingAudioSource::prepareToPlay(samplesPerBlockExpected, sampleRate)` allocates: `AudioBuffer<float> buffer` (sized to `samplesPerBlockExpected * ratio` headroom) + `HeapBlock<float*> destBuffers` + `HeapBlock<const float*> srcBuffers` + `HeapBlock<FilterState> filterStates`. **This must be hoisted to message-thread setup (`VibeVoice::prepareForPlayback`)** — calling it per-note as the existing :609 effectively does would re-allocate every note-on. Plan already correctly anticipates this hoist; flagging here so Task 2 doesn't miss it.

- `juce::ADSR::isActive()` EXISTS and returns true for attack/decay/sustain/release (anything except idle). But the `State` enum is PRIVATE (`enum class State { idle, attack, decay, sustain, release };` in the `private:` section at line 305 of `juce_ADSR.h`). **There is NO public query for "is in release phase".** That means the L7(b) hybrid stealing's "prefer release-phase oldest" predicate cannot read JUCE state directly — it requires VibeVoice to track its own `bool mInRelease` flag, set true on `stopNote(velocity, allowTailOff=true)` and reset on every fresh `startNote`. Confirms the plan's assumption that an explicit `mInRelease` member is mandatory; not a sub-spec call, just a Task 3 implementation hook to remember.

### Section 3 — Sub-B PIVOT (revised options post-inventory)

LOAD-BEARING finding. The original Sub-B(a) framing in the plan file assumed `juce::ResamplingAudioSource::setSource()` existed; it doesn't. The actual question Sub-B is now answering is: given the resampler can't be re-pointed after construction, how do we make the resampler-input-source fat (forward + reverse, picked at startNote) without re-allocating the resampler? Two viable approaches (the previous (b) JUCE-class adapter path is dead — there's no `juce::MemoryAudioSource` fat mode and no adapter that would let us swap input mid-flight):

- **(a) Custom forward source + dual permanent `juce::ResamplingAudioSource`** (one per direction). VibeVoice owns:
  - `VibeForwardMemoryAudioSource mForwardSrc` (custom ~30-line `juce::PositionableAudioSource` subclass with `setBuffer(juce::AudioBuffer<float>*)` + `setNextReadPosition` / `getNextAudioBlock` reading from the buffer pointer; pure forward delegation)
  - `ReversedMemoryAudioSource mReverseSrc` (existing class, new `setBuffer(juce::AudioBuffer<float>*)` member added; pure reverse delegation)
  - `juce::ResamplingAudioSource mForwardResamp` (permanent, ctor-bound to `&mForwardSrc`)
  - `juce::ResamplingAudioSource mReverseResamp` (permanent, ctor-bound to `&mReverseSrc`)
  - `juce::ResamplingAudioSource* mActiveResamp` (pointer to whichever direction is live this note)
  - startNote sets `mActiveResamp` to the chosen direction's resampler, calls `mActiveResamp->flushBuffers()` + `mActiveResamp->setResamplingRatio(ratio)`, sets the chosen source's buffer pointer via `setBuffer()`.
  - Memory cost: ~2x ResamplingAudioSource state per voice. Each resampler's internal `AudioBuffer<float> buffer` is sized to `samplesPerBlockExpected * ratio` headroom — call it ~8-15 KB extra per voice depending on max block size + max ratio. 16 voices = ~128-240 KB per engine instance × N engine instances in a session. Not free but small relative to sample-data RAM.
  - Code complexity: 1 new small custom class (`VibeForwardMemoryAudioSource`, ~30 lines, pure delegation).

- **(b) Custom forward source + custom `VibeSourceFork` wrapper + single `juce::ResamplingAudioSource`**. VibeVoice owns:
  - `VibeForwardMemoryAudioSource mForwardSrc` (same ~30-line custom class as (a))
  - `ReversedMemoryAudioSource mReverseSrc` (same existing-class extension as (a))
  - `VibeSourceFork mFork` (NEW custom ~25-line `juce::PositionableAudioSource` subclass holding pointers to both `mForwardSrc` + `mReverseSrc` + a `bool mUseReverse` flag; `getNextAudioBlock` delegates to whichever is selected; `setNextReadPosition` / `prepareToPlay` / `releaseResources` forward to both)
  - `juce::ResamplingAudioSource mResampSrc` (permanent, ctor-bound to `&mFork`)
  - startNote sets `mFork.setUseReverse(reverseMode)` + sets the chosen source's buffer pointer, calls `mResampSrc.flushBuffers()` + `mResampSrc.setResamplingRatio(ratio)`.
  - Memory cost: single resampler per voice. ~half the resampler-state RAM of (a).
  - Code complexity: 2 new small custom classes (`VibeForwardMemoryAudioSource` + `VibeSourceFork`, both pure delegation, zero DSP logic — explicitly NOT the "risky custom DSP logic" Jeff was avoiding when locking L4(a) the first time).

- **(c)** Different approach Jeff proposes.

My read: (b) is slightly cleaner (one resampler per voice, lower RAM, the fork wrapper is pure delegation so the "risky custom code" surface is still effectively zero), but the extra class is one more concept to maintain. (a) is more brute-force but each direction's resampler is independently flushable which might be a future-proof advantage if the engine ever wants to crossfade between directions instead of hard-switching. Spec call surface to Jeff; not mine to pick per `feedback_dont_make_unilateral_spec_calls.md`.

### Section 4 — Sub-A and Sub-C status (unchanged by Task 1 findings)

- **Sub-A** (atomic `isActive` flag under L1=(a) — redundant with JUCE `SynthesiserVoice::isVoiceActive()` or supplemental?) — unchanged from plan; still open for Jeff's pick at Task 2 plan-finalize. Task 1 didn't surface anything that biases either way.
- **Sub-C** (dummy buffer location for fat-source pre-allocation) — unchanged; pairs with Sub-B's pivot. Sub-C(c) "no dummy buffer; source class short-circuits on null internal buffer" remains the cleanest variant if Sub-B(a) or Sub-B(b) is picked (the custom forward source class can just check `if (mBuffer == nullptr) { info.clearActiveBufferRegion(); return; }` in `getNextAudioBlock`, and `ReversedMemoryAudioSource` gets the same null-guard added).

### Section 5 — Other inventory findings (confirmed plan assumptions, NOT sub-spec calls)

- **L5 `std::array<int, 32>` cap sufficiency**: 32 covers every realistic sample mapping. Worst-case region density for a single `(midiNote, velocity, articGroup)` tuple is round-robin sample stacks; typical SFZ packs ship 2-8 RR variations per zone, heavily-layered packs rarely exceed 16. 32 is safe with comfortable headroom. No revision to L5.
- **`juce::Synthesiser::findFreeVoice()` / `findVoiceToSteal()` interaction with new fade-out state**: JUCE's framework sees a fading-out voice as `isVoiceActive() == true` until our `renderNextBlock` calls `clearCurrentNote()` at fade-out end. Two consequences: (a) JUCE will skip the fading-out voice when finding a free voice for a brand-new note (allocates to a different free slot), OR (b) if the pool is fully occupied JUCE may try to steal the fading-out voice via its own `findVoiceToSteal` — but since our L7(b) hybrid stealing runs BEFORE the JUCE allocation path on note-on, the new note will already be allocated to the freshly-stolen-and-now-fading voice's slot via the natural framework flow. The fade-out continues to completion on the OLD voice's source/filter state in renderNextBlock and `clearCurrentNote()` fires at fade-out end. Audibly clean. Plan already correctly documents this; flagging here as a confirmed-not-broken interaction.
- **`mInRelease` flag tracking mandatory**: per Section 2's ADSR finding. Plan-listed `mInRelease` member is the L7(b) implementation hook. Set true on `VibeVoice::stopNote(velocity, allowTailOff=true)`, reset to false on every fresh `VibeVoice::startNote`. Read in `VibeSynth::noteOn`'s stealing-victim selection loop.

### Section 6 — VibeVoice / VibeSynth / VibeSampleManager / VibePlayerProcessor class shape (sanity check, no new findings)

- `VibeVoice : juce::SynthesiserVoice` — owns `mAdsr` (juce::ADSR), `mFilter` (juce::dsp::StateVariableTPTFilter<float>), `mTmpBuffer` (juce::AudioBuffer<float>), the 3 source unique_ptrs we're killing this batch (`mMemSrc` + `mResampSrc`, will be replaced per Sub-B resolution), plus per-note state for envelope phase / filter cutoff / LFO phase / drive / bit-reduct / etc.
- `VibeSynth` — owns `mManager` (`VibeSampleManager`), `mSynth` (`juce::Synthesiser`), the CPU-guard cache for ~30 APVTS parameters, RNG for unison detune, stereo-width + treble-shelf state.
- `VibeSampleManager` — owns `mFormatManager` (`juce::AudioFormatManager`), `mRegions` (`std::vector<VibeRegion>`), `mLoadedFolder` path, the per-`(note, artic)` round-robin counter table for RR variation rotation.
- `VibePlayerProcessor : juce::AudioProcessor` — owns `mSynth` (`VibeSynth`) + `apvts` (`AudioProcessorValueTreeState`) + audition atomics + the param-cache mirror. `prepareToPlay` calls `mSynth.prepare(sr, blockSize)`. Confirmed the prepareToPlay chain reaches `VibeSynth::prepare` which calls `forEachVoice(v.prepareForPlayback(maxBlockSize))` — this is the message-thread setup point where the L4(a) resampler hoist lands in Task 2.

### Section 7 — Rule 4 Diagnostic Instrumentation Catalog

Nil for Task 1 (read-only inventory; no source touched; no `DBG` / `juce::Logger::writeToLog` / temp `jassert` / debug `juce::AlertWindow` added).

### Section 8 — Next action

Surface Sub-A + Sub-B-revised + Sub-C to Jeff as plain-text numbered list for resolution. Edit the plan file's Sub-B section to replace the original (a)/(b)/(c) options with the revised (a)/(b)/(c) options from Section 3 above (the original (b) JUCE-adapter path is dead per the `setSource` finding); land as part of this Task 1 docs-only commit alongside the running-notes append. Task 2 (fat-voice refactor) blocked on Sub-A + Sub-B + Sub-C resolution at Task 2 plan-finalize. Once unblocked: Task 2 lands fat voices + L4(a) resampler-hoist + Sub-B/Sub-C source-wiring as the first structural commit; Task 3 follows with the lock-free occupancy + L6 64-sample fade-out + L7(b) hybrid stealing as the second structural commit (per L8(b) split rollback boundary); Task 4 lands the L5(a) `std::array<int, 32>` swap in `findRegion`; Task 5 stress-file verify (no commit per QA-InsertMaps Task 3 precedent); Task 6 cleanup + grep sweep; Task 7 close.

### Section 9 — Cross-batch tooling rule folded in (NEW commit-mechanics convention)

Two heredoc-based commit attempts in this same batch (Task 0 + Task 1) both hit `unexpected EOF while looking for matching '`' parser errors in the Bash tool harness. Diagnosis: the harness's outer command wrapping collides with the ~30+ apostrophes per long commit message in this project's multi-paragraph technical-narrative style. Jeff's resolution at Task 1 surface time: default to `git commit -F <file>` for long messages; write message to `.git/COMMIT_EDITMSG_<batch>-<task>.txt`, commit-F, rm. File-based bypasses all shell parsing of the message body. Heredoc kept for short single-paragraph commits without apostrophe-dense narrative. Rule landed in five places this Task 1 commit (scope-fold per Task 0's `.gitignore` precedent):

1. [CLAUDE.md](../../CLAUDE.md) — new "## Git Commit Mechanics" section inserted between "## Build System" and "## Source Layout". Every BaySickDAW session reads CLAUDE.md → sees the rule.
2. [Plans & Specs/Main Plan.md](../Main Plan.md) — new sub-bullet under §0 Agent Orchestration Rules > Batch lifecycle > Pre-commit (alongside the existing `/draft-commit` rule). Every batch open reads Main Plan §0 → sees the rule.
3. [Files For Claude/batch_session_boilerplate.md](../../Files For Claude/batch_session_boilerplate.md) — new sub-bullet under "Mid-batch discipline" > "Every commit" (gitignored file, no commit needed; edit lives on disk for the next session paste).
4. `~/.claude/agents/commit-drafter.md` (global agent file, outside repo) — frontmatter description updated from "...for the main session to use with `git commit -m`" to "...via `git commit -F <file>` or `git commit -m` per project convention (`-F` is the safer default for long apostrophe-dense narrative messages)". Strict-rules "Never run `git commit`" bullet expanded to spell out the `-F` vs `-m` choice. Propagates the -F awareness to the CotBB game project + any future project using the global agent.
5. [.claude/agents/commit-drafter.md](../../.claude/agents/commit-drafter.md) (NEW project-specific override) — full self-contained agent file derived from the global with BaySickDAW-specific additions: title line explicitly references CLAUDE.md "## Git Commit Mechanics", in-tree commit-hash exemplars (`3587ade`, `e9fe545`, `68050a8`, `eb718bf`, `cb40412`, `fbdc0e0`, `a1211cd`) for the drafter to match style against, brand-casing rule (`BaySickPlayer` not `VibePlayer` per `feedback_match_jeff_text_casing.md`), ASCII-only rule, and the `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>` trailer locked. Project file takes precedence over the global when both exist (Claude Code agent resolution: project-scope overrides global-scope same-named agents).

Mirrored across docs + agents so future sessions see the rule no matter where they enter: every batch open reads CLAUDE.md + Main Plan §0 + the boilerplate; every `/draft-commit` dispatch loads the project-scope agent file; any future project loads the global agent file. Per Jeff's directive 2026-05-25: "update the rules on the main plan so that you are aware every time what needs to be done" + "1 and 2 [project + global agent updates] as this will likely be the same problem once I start working more on the game [CotBB]". Convention spans the BaySickDAW + CotBB toolchain.

---

## 2026-05-25 — Task 2 — Fat-voice structural refactor (L4(a) + Sub-B(a) + Sub-C(c) wiring)

Structural one-shot per L8(b) split's first half. Two source files modified (`Source/VibePlayer/VibePlayerDSP.h` + `.cpp`); the 3 per-note `make_unique` sites at the pre-batch [:581 / :583 / :607](../../Source/VibePlayer/VibePlayerDSP.cpp:581) and their `mMemSrc` / `mResampSrc` unique_ptr members are fully gone. VibeVoice now owns forward + reverse `juce::PositionableAudioSource` subclass instances + their dedicated `juce::ResamplingAudioSource` permanently as members; `startNote` re-points an active-source / active-resampler pointer pair instead of allocating. Lock-free occupancy flag (Sub-A's `mIsActive` atomic), L7(b) hybrid stealing, and L6 64-sample fade-out are NOT in Task 2 (Task 3 per the split). L5(a) `std::array<int, 32>` `findRegion` swap is NOT in Task 2 (Task 4 per the plan). Both Release + Debug build clean. Jeff-verified PASS on all 6 BaySickPlayer scenarios after one misdirected first-attempt BaySickSynth verify surfaced a separate pre-existing bug (routed to QA-EngineApvts per Rule 3 — see Section 4).

### Section 1 — Sub-spec calls resolved at Task 2 plan-finalize

All three Task 1-surfaced sub-spec calls locked by Jeff 2026-05-25 immediately before Task 2 source work began. Plan file's Sub-A / Sub-B / Sub-C sections will be updated at close to reflect the resolutions; recording verbatim here so Task 3 (Sub-A wiring) doesn't re-litigate:

- **Sub-A = (a)** Add explicit `std::atomic<bool> mIsActive { false };` per voice (supplemental to JUCE's `SynthesiserVoice::isVoiceActive()`, not a replacement). Jeff: "We absolutely want to avoid dynamic_cast loops on the hot audio path. Scanning the atomic flags will be vastly faster for voice stealing." **NOTE: Sub-A is Task 3 implementation territory** — the flag is not added in Task 2 (would be dead state without the stealing-loop reader); locked here so Task 3 has the resolution in hand.

- **Sub-B = (a)** Dual permanent `juce::ResamplingAudioSource` (one per direction). Jeff: "We are happy to trade 240 KB of RAM to avoid adding extra virtual dispatch/abstraction layers (VibeSourceFork) to the audio thread." VibeVoice owns `VibeForwardMemoryAudioSource mForwardSrc` + `ReversedMemoryAudioSource mReverseSrc` + `mForwardResamp` ctor-bound to `&mForwardSrc` + `mReverseResamp` ctor-bound to `&mReverseSrc` + an `mActiveSrc` / `mActiveResamp` per-note pointer pair that startNote swings to the chosen direction. Sub-B(b)'s `VibeSourceFork` wrapper path was eliminated on RAM-vs-virtual-dispatch tradeoff: extra ~120 KB per engine instance is a clean exchange for one fewer virtual-dispatch layer in `getNextAudioBlock` on the audio thread, and the dual-resampler shape leaves the door open to future per-direction crossfade should the engine ever want it (Section 3 of Task 1's pivot writeup flagged this future-proof advantage).

- **Sub-C = (c)** No dummy buffer. Both custom source classes implement `if (mBuf == nullptr) { info.clearActiveBufferRegion(); return; }` null-guard in `getNextAudioBlock`. Jeff: "If there is no buffer, they should just clearActiveBufferRegion() and return." Pairs cleanly with Sub-B(a) — both `VibeForwardMemoryAudioSource` and `ReversedMemoryAudioSource` get the same null-guard pattern so pre-`setBuffer` construction-time `getNextAudioBlock` calls (if the resampler's `prepareToPlay` does any pre-fill from its internal headroom buffer) are silent rather than UB on a null pointer dereference.

### Section 2 — Task 2 source edits landed (fat-voice structural one-shot)

Two source files modified per Sub-B(a) + Sub-C(c) + L4(a) (fat-voice ownership). Diff total: 2 files changed, +202 insertions, -80 deletions (net +122 lines, mostly the new `VibeForwardMemoryAudioSource` class + the moved `ReversedMemoryAudioSource` in the header).

#### `Source/VibePlayer/VibePlayerDSP.h` (+165 lines, +5 new VibeVoice members, 2 source classes hoisted to header for direct membership)

- **NEW** `class VibeForwardMemoryAudioSource : public juce::PositionableAudioSource` (~70 lines, file-scope in header). Mirrors `juce::MemoryAudioSource` semantics (delegates to a `juce::AudioBuffer<float>` for read; tracks `mPos` for `setNextReadPosition` / `getNextReadPosition`; reports `getTotalLength` from the buffer's sample count). Adds the load-bearing `void setBuffer(const juce::AudioBuffer<float>& buf) noexcept` member that re-points the internal buffer pointer + resets `mPos = 0`. Implements the Sub-C(c) null-guard short-circuit at the top of `getNextAudioBlock`: `if (mBuf == nullptr) { info.clearActiveBufferRegion(); return; }`. Hoisted to header (file-scope class, not anon-namespace) because it's a direct VibeVoice member and anon-namespace classes can't be members of a header-declared class.

- **MOVED** `class ReversedMemoryAudioSource` from the pre-batch `Source/VibePlayer/VibePlayerDSP.cpp` anon-namespace at `:461-513` to header file-scope. Extended with three additions: a `void setBuffer(const juce::AudioBuffer<float>& buf) noexcept` member (mirrors `VibeForwardMemoryAudioSource::setBuffer`); a default constructor (the existing const-reference-taking ctor required a buffer at construction time, which doesn't fit fat-voice ownership where the voice is constructed before any sample is loaded); the same Sub-C(c) null-guard short-circuit in `getNextAudioBlock`. Original const-ref constructor preserved against future need (currently unused now that startNote uses `setBuffer`); kept rather than deleted to avoid forcing a churn-pass on any future caller that might construct a one-shot reversed source.

- **REMOVED** VibeVoice members: `std::unique_ptr<juce::PositionableAudioSource> mMemSrc;` + `std::unique_ptr<juce::ResamplingAudioSource> mResampSrc;` (the OLD per-note heap-allocated pair killed by this batch's whole point).

- **ADDED** VibeVoice members per Sub-B(a):
  - `VibeForwardMemoryAudioSource mForwardSrc;`
  - `ReversedMemoryAudioSource mReverseSrc;`
  - `juce::ResamplingAudioSource mForwardResamp { &mForwardSrc, false, 2 };`
  - `juce::ResamplingAudioSource mReverseResamp { &mReverseSrc, false, 2 };`
  - `juce::PositionableAudioSource* mActiveSrc { nullptr };`
  - `juce::ResamplingAudioSource* mActiveResamp { nullptr };`

  **Declaration order matters** — `mForwardSrc` / `mReverseSrc` are declared BEFORE their respective resamplers so the resamplers' ctor-bound input pointers (`&mForwardSrc` / `&mReverseSrc`) reference fully-constructed objects. C++ member initialization order is declaration order within the same access section (C++ standard), independent of init-list order; got this right on the first pass by laying the sources out above the resamplers in the header rather than relying on init-list ordering.

#### `Source/VibePlayer/VibePlayerDSP.cpp` (-117 / +85 net, 5 surgical edits)

- **DELETED** the anon-namespace `ReversedMemoryAudioSource` class at the pre-batch `:461-513` (moved to header per above). The cpp now has a one-paragraph "moved to header for direct VibeVoice membership per QA-VoicePool Sub-B(a)" pointer comment at the old location so external readers searching the cpp see the rename history without bouncing to the header.

- `VibeVoice::prepareForPlayback(int blockSize)` at `:525-529` — added `mForwardResamp.prepareToPlay(blockSize, mSampleRate); mReverseResamp.prepareToPlay(blockSize, mSampleRate);`. This is the Section 2 / Task 1 inventory hoist: the resampler's `prepareToPlay` allocates `AudioBuffer<float> buffer` (sized to `samplesPerBlockExpected * ratio` headroom) + three `HeapBlock` allocations for filter state + dest/src buffer pointer tables; running it per-note as the pre-batch `:609` effectively did would re-allocate every note-on. Hoisting to the message-thread `prepareForPlayback` setup point makes every per-note resampler reuse those allocations.

- `VibeVoice::releaseResources()` at `:534-540` — stripped per-note source resets; now only clears `mActiveSrc = nullptr; mActiveResamp = nullptr; mSampleBuffer.reset(); mIsPlaying = false;`. Fat sources stay allocated permanently (the whole point of fat-voice ownership); per-note state is the active-pointer pair + the `shared_ptr<AudioBuffer<float>>` to the region's audio buffer that gets reset on note-off. No source object construction or destruction on the audio thread.

- `VibeVoice::startNote` at `:566-645` — source-block rewrite. Replaced the 3 `make_unique` calls at the pre-batch `:581-583` + `:607` with the fat-voice re-pointing pattern:

  ```cpp
  if (mReverse)
  {
      mReverseSrc.setBuffer(*mSampleBuffer);
      mActiveSrc = &mReverseSrc;
      mActiveResamp = &mReverseResamp;
  }
  else
  {
      mForwardSrc.setBuffer(*mSampleBuffer);
      mActiveSrc = &mForwardSrc;
      mActiveResamp = &mForwardResamp;
  }
  ```

  Sample-start positioning now uses `mActiveSrc->setNextReadPosition(startPos)` (replaces the pre-batch source-ctor-time positioning). Per-note resampler reset uses `mActiveResamp->flushBuffers(); mActiveResamp->setResamplingRatio(resampRatio);` (replaces the pre-batch per-note resampler construction). NO per-note `prepareToPlay` call — hoisted to `prepareForPlayback` per the previous bullet.

- `VibeVoice::renderNextBlock` at `:662+` — `!mResampSrc` guard replaced with `mActiveResamp == nullptr` guard (active-pointer pair is the new "no live source" sentinel); `mResampSrc->getNextAudioBlock(info)` replaced with `mActiveResamp->getNextAudioBlock(info)`. No other body changes — filter / ADSR / treble-shelf / stereo-width are unchanged.

#### Grep cleanliness post-edits

- `grep -rn "mResampSrc|mMemSrc" Source/VibePlayer/` returns ZERO matches. The unique_ptr members + every per-note use site are fully gone.
- `grep -rn "make_unique<juce::MemoryAudioSource>|make_unique<juce::ResamplingAudioSource>|make_unique<ReversedMemoryAudioSource>" Source/` returns ZERO matches. The 3 per-note heap-alloc sites are fully gone.
- L5(a) `findRegion` `std::vector<int> candidates` swap is **NOT** done in Task 2 — that's Task 4 (kept separate per the plan's task split so the structural fat-voice work is a clean independently-reviewable commit before the orthogonal `findRegion` tweak lands).

#### Build status

Both Release + Debug build clean: `RELEASE_EXIT_CODE=0`, `DEBUG_EXIT_CODE=0`. Only pre-existing warnings (C4100 unreferenced formal parameter / C4996 deprecation / C4324 structure-padded / etc.) survive — no new warnings introduced by Task 2.

### Section 3 — Verify PASS (Jeff, 2026-05-25)

**First verify attempt was on BaySickSynth** (out of Task 2 scope; QA-VoicePool is BaySickPlayer-only per L1=(a) + the §5 entry's scope). Chord variation observed there + recorded master at `Projects/Fresh Test/Samples/Fresh Test - Master - 2026-05-25 15-03-07.wav` shows waveform varies between consecutive chord hits. Initially handwaved as "pre-existing behavior" — Jeff overruled correctly. Real bug, not Task 2's. Diagnosis + routing in Section 4. Process lesson: `feedback_check_code_before_calling_it_expected.md` applies — should have asked which engine BEFORE handwaving.

**Re-test on BaySickPlayer (Task 2 surface): all 6 scenarios PASS:**

- **(1)** 3-6 note chord — all notes sound, no dropouts, no clicks.
- **(2)** Rapid individual notes (drum-roll pace) — each triggers cleanly, no allocation hiccups.
- **(3)** Reverse ON — `mReverseSrc.setBuffer` + `mActiveResamp = &mReverseResamp` swing correct; sample plays backwards as before.
- **(4)** Reverse OFF — `mForwardSrc.setBuffer` + `mActiveResamp = &mForwardResamp` swing back; forward identical to pre-batch.
- **(5)** Engine swap mid-playback — `releaseResources` correctly clears active-pointer pair without touching fat sources; teardown + new sample load clean.
- **(6)** 1-2 min sustained playback — audio identical to pre-batch; no dropouts; memory stable.

Per L10 verify-cadence (Debug-then-Release per QA-InsertMaps norm + QA-Md MT-works-in-Debug fact). Task 2 verifies clean. Rollback boundary intact: if Task 3's lock-free occupancy needs revert, this commit stands alone as the fat-voice refactor.

### Section 4 — BaySickSynth side-finding (routed to QA-EngineApvts per Rule 3 + Jeff's scope call)

Diagnosed during the BaySickSynth misdirected first verify attempt. NOT Task 2's fat-voice refactor; pre-existing missing-oscillator-reset bug in `BaySickSynthVoice::startNote`:

- `BaySickSynthVoice` owns two `WavetableOscillator` members `mOsc` + `mOsc2` at [BaySickSynthVoice.h:122-123](../../Source/BaySickSynth/BaySickSynthVoice.h:122).
- `WavetableOscillator` has internal `float mPhase { 0.0f };` at [WavetableOscillator.h:38](../../Source/WavetableOscillator.h:38) + exposed `void reset();` at [WavetableOscillator.h:20](../../Source/WavetableOscillator.h:20).
- `BaySickSynthVoice::startNote` at [BaySickSynthVoice.cpp:36-123](../../Source/BaySickSynth/BaySickSynthVoice.cpp:36) resets inline phase accumulators (`mPhase1` / `mPhase2` / `mPhase3` / `mFMCarrierPhase` / `mFMModPhase` / `mDeafSawState` at `:72-77`) but does NOT call `mOsc.reset()` or `mOsc2.reset()`. Wavetable phase persists across notes — same MIDI note replayed on same voice slot → different starting wavetable phase → different waveform shape → audibly different sound per hit.
- **Affected waveforms** (use `mOsc` / `mOsc2`): SAW (default), SAW+SAW, SAW+SQUARE, SQUARE+SQUARE, SUPERSAW. **Inline-phase waveforms** (PULSE, BELL, DEAF SAW, SPREAD OCT, SPREAD 5TH, SINE) reset deterministically — unaffected.

Spec-call surface: (1) new dedicated batch / (2) fold into QA-EngineApvts / (3) fold into QA-VoicePool close-routing / (4) §9 Forks entry only, slot later.

Jeff's decision 2026-05-25: **Option (2) — fold into QA-EngineApvts.** Rationale (verbatim): "We need to strictly enforce our rollback boundaries. QA-VoicePool is about lock-free memory allocation for the sample player. I do not want to introduce synth DSP state changes into this commit." Scope-discipline lock — QA-VoicePool stays scope-pure.

**Action at QA-VoicePool close (per Rule 3 routing-at-close):**
- Write §9 Forks entry recording the finding + QA-EngineApvts routing + Jeff's rollback-boundary rationale.
- Expand §5 QA-EngineApvts entry to add the 2-line `mOsc.reset(); mOsc2.reset();` source-edit alongside the dirty-flag pattern work.
- No source change in QA-VoicePool itself.

### Section 5 — Rule 4 Diagnostic Instrumentation Catalog

Nil for Task 2 (structural refactor; no `DBG` / `juce::Logger::writeToLog` / temp `jassert` / debug `juce::AlertWindow` added).

### Section 6 — Next action

Task 3 — lock-free occupancy + L7(b) hybrid stealing + L6 64-sample fade-out (per L8(b) split's second half). Adds: `std::atomic<bool> mIsActive { false }` per Sub-A(a) + `bool mInRelease { false }` + `int mStealFadeOutSamplesLeft { 0 }` + `float mStealFadeOutGainStart { 1.0f }` + `VibeVoice::initiateSteal()` + fade-out application in `renderNextBlock` + `VibeSynth::findStealCandidate(int newPitch)` hybrid logic + voiceCap-stealing branch rewire at [VibePlayerDSP.cpp:944-960](../../Source/VibePlayer/VibePlayerDSP.cpp:944). Estimated 3-4 hour cycle. Blocked on Jeff's commit of Task 2 → commit lands → Task 3 begin.

---

## 2026-05-25 — Task 3 — Lock-free occupancy + L7(b) 3-tier hybrid stealing + L6 ADSR quick-release via physical over-provisioning + look-ahead noteOff pre-scan

Structural one-shot per L8(b) split's second half. Task 3's deliverable shape evolved through three Jeff-driven course corrections during execution — initial 2-tier-hybrid + synchronous-render-fade design became 3-tier-with-isKeyDown-protection + physical-over-provisioning + look-ahead-noteOff-pre-scan by the time Release verify PASSed. The corrections matter to record verbatim because they're the load-bearing pro-DAW-engineering judgment that future readers (and future me) need to be able to reconstruct without re-living the same diagnosis. Two source files modified ([VibePlayerDSP.h](../../Source/VibePlayer/VibePlayerDSP.h) + [VibePlayerDSP.cpp](../../Source/VibePlayer/VibePlayerDSP.cpp)); diff total +247 / -23 net. Both Release + Debug build clean. Jeff-verified PASS on 6 scenarios (A: 16-chord-loop+lead, B: 4-chord-loop+lead, C: release-phase-preferred regression, D: all-keys-held fallback regression, E: Reverse-mode parity, F: MT parity) per L10 Debug-then-Release cadence.

### Section 1 — Design journey (3-correction narrative — load-bearing)

#### Correction 1: synchronous-render fade-out → physical over-provisioning + ADSR quick-release

Initial Task 3 implementation prepared `VibeVoice::initiateSteal()` as a 64-sample synchronous render INTO the output buffer at MIDI-dispatch time — the literal reading of L6 = (d) 64 samples fade-out. Jeff caught the fundamental safety flaw at design-surface time, verbatim: "Rendering 64 samples forward synchronously during an event callback is fundamentally unsafe. If the audio block size is small (e.g., 32 samples) and a steal happens near the end of the block, writing 64 samples will cause a buffer overflow and crash the DAW."

Pivot to **physical over-provisioning + ADSR quick-release**. The mechanism:

- `kMaxVoices` in [VibePlayerDSP.h:241](../../Source/VibePlayer/VibePlayerDSP.h:241) raised 16 → 24. The juce::Synthesiser pool now holds 24 fat voices, NOT 16.
- New `static constexpr int kLogicalCap = 16;` added — the user-facing polyphony default. Existing `voiceCap` APVTS unchanged (still 1..16 range; default 16). The 8 reserve voices (`kMaxVoices - kLogicalCap`) are invisible to the user — they exist solely as overflow slots for stolen-voice fade-outs.
- `VibeVoice::initiateSteal()` overrides the voice's `mAdsr.release` to 0.0015 sec (~1.5 ms = ~66 samples @ 44.1 kHz, ~72 samples @ 48 kHz; matches the L6 = (d) 64-sample target closely). Saves the user's pre-steal `juce::ADSR::Parameters` into a new `mPreStealAdsrParams` member + sets `mAdsrOverridden = true` so the next `startNote` can restore.
- After `initiateSteal()` fires, the voiceCap-stealing branch calls `victim->stopNote(0.f, true)` — `allowTailOff=true` lets the voice enter its (now quick) ADSR release naturally. The fade-out runs sample-accurately inside the subsequent `juce::Synthesiser::renderNextBlock` call's standard per-voice render loop. Zero buffer-overflow risk: JUCE's render loop bounds writes to the actual buffer length, the fade-out continues across blocks as needed, `clearCurrentNote()` fires at ADSR end exactly as it does for any natural noteOff release.
- The new note allocates to one of the 8 reserve voices via `juce::Synthesiser::findFreeVoice` (the just-stolen voice is still `isVoiceActive() == true` during its fade-out, so JUCE picks a different free voice). No custom allocation path needed; JUCE's standard framework handles it.

Tradeoff captured: 8 extra VibeVoice instances cost ~50% more pool memory per VibePlayer engine instance (each VibeVoice now carries dual permanent resamplers per Task 2's Sub-B(a)). At ~15 KB per voice for the resampler-state RAM alone, 8 extra voices = ~120 KB per engine. Across N engine instances in a session, the cost is small relative to sample-data RAM and entirely acceptable for the safety property gained.

#### Correction 2: 2-tier hybrid → 3-tier with `isKeyDown()` protection

Initial L7(b) implementation was the literal L7(b) wording — "hybrid stealing: prefer release-phase oldest, fallback to overall-oldest". 2 tiers. Jeff identified the "held lead + looping chord" pathology at verify time, verbatim:

> "If I hold down a sustained lead note (a 5th note), and let a 4-note chord sequence loop repeatedly in the background, my sustained lead note is eventually stolen. Because it was held down across multiple bars, it becomes the 'oldest active note,' so the engine kills it to make room for the newer chords. The Pro DAW Standard: voice stealing is based on a hierarchy that respects physical key state (Note-On vs. Note-Off). An actively held note should almost never be stolen by newer notes that have already received their Note-Off commands."

Upgraded to **3-tier hybrid** using JUCE's built-in `juce::SynthesiserVoice::isKeyDown()` accessor (verified public at `juce/modules/juce_audio_basics/synthesisers/juce_Synthesiser.h:233` — no need for VibeVoice to track its own physical-key-state flag). Tier definitions:

- **Tier 0** = `isInRelease()` — voice already in ADSR release phase (`mInRelease == true`). Released voices are by definition on their way out; stealing them first matches the "least-disruptive-to-the-mix" principle the original L7(b) was reaching for.
- **Tier 1** = `isNoteOffQueued() || !isKeyDown()` — voice has received noteOff (so `isKeyDown() == false`) but is not yet in release (sustain pedal holds it). OR voice has a noteOff queued for this block per Correction 3's look-ahead pre-scan. These are "the user has lifted the key but the voice is still sustaining" candidates.
- **Tier 2** = `isKeyDown() == true` AND `!isNoteOffQueued()` — physically held key, no queued noteOff. PROTECTED. Only stolen as last resort when no Tier 0 / Tier 1 candidate exists.

Within each tier, picks the oldest by `mNoteStartCounter` (preserves the original "oldest-first" intuition). The 3-tier scan walks lowest-priority tier first, returns from the first populated tier found.

#### Correction 3: MIDI-event-ordering issue at loop boundaries → look-ahead noteOff pre-scan

With the 3-tier implementation, Jeff re-verified and reported: "my 17th note is still getting stolen after 2 bars and a 5th note with 4 voices set still gets stolen after 2 bars. please think out what is happening before proceeding and let me confirm." The bug isn't in `findStealCandidate` — it's in the MIDI event ordering at piano-roll loop boundaries.

Diagnosis trace: at a piano-roll loop boundary, the old chord's noteOffs and the new chord's noteOns land in the same MIDI buffer (the loop wraparound delivers both events in the same block). `VibeSynth::renderNextBlock` already dispatches noteOns IMMEDIATELY via `mSynth.noteOn` — which is what fires the voiceCap-stealing branch. But noteOffs are DEFERRED into `filteredMidi` for later dispatch via `mSynth.renderNextBlock(buffer, filteredMidi)` at the bottom of the function (the same buffer is used for same-pitch-strip filtering elsewhere). At the moment of the new-chord-note voiceCap decision, the old chord voices are STILL `isKeyDown() == true` (their noteOff event hasn't been dispatched to `juce::Synthesiser` yet — it's sitting in `filteredMidi` waiting for the renderNextBlock at the end of the function). All voices appear as Tier 2 to `findStealCandidate`; the protection layer protects no one; findStealCandidate picks the genuinely oldest = the held lead (older `mNoteStartCounter` than the chord notes that turn over each loop iteration).

Jeff confirmed via test setup: "16 sustained 1 bar notes over 4 bars (16notes each bar) and one played key continuously" (Scenario A) and "4 note looping pattern + 5th sustained" (Scenario B). Both reproduce the lead-stolen pathology on the 3-tier implementation.

Fix: **look-ahead noteOff pre-scan** that flips a transient `bool mNoteOffQueued` flag on voices whose noteOffs will be delivered this block, BEFORE the noteOn dispatch loop runs. `findStealCandidate`'s Tier 1 check picks up `isNoteOffQueued()` alongside `!isKeyDown()` — flagged voices become Tier 1 candidates even though `isKeyDown()` is still true at the moment of the check.

Two alternatives surfaced (pre-pass-rewrite vs look-ahead-flag). Jeff's verbatim decision: "We absolutely must go with the Look-Ahead Pre-Scan. Losing sample-accurate MIDI timing (the pre-pass alternative) introduces MIDI jitter, which is unacceptable for a pro DAW. Preserving sub-block timing is entirely worth the extra ~25 lines of code." The pre-scan does NOT reorder events; the actual MIDI dispatch order is unchanged, so sample-accurate timing of every noteOn + noteOff is preserved. The pre-scan only annotates voices with "noteOff is coming for you this block" so the stealing logic can make a more informed Tier 1 decision.

The look-ahead pre-scan mirrors the existing same-pitch-strip logic — it walks the MIDI buffer ONCE before the noteOn dispatch loop, marking voices whose noteOffs are pending. It correctly handles the same-pitch-strip case: if a later same-pitch noteOn would strip the noteOff (cancel it via the existing same-pitch preemption path), the flag is NOT set on that voice (no actual noteOff will be delivered, so the voice doesn't deserve the Tier 1 promotion).

### Section 2 — Task 3 source edits landed (final shape after all 3 corrections)

Two source files modified. Diff total: +247 insertions, -23 deletions, net +224 lines.

#### `Source/VibePlayer/VibePlayerDSP.h`

- **Includes**: `<atomic>` + `<array>` added explicitly (both transitive via JuceHeader but explicit improves clarity at the top of the new state member declarations).
- **VibeVoice new public accessors** (read-only predicates for `findStealCandidate` + the look-ahead pre-scan):
  - `bool isActive() const noexcept` — atomic load of `mIsActive` (`std::memory_order_acquire`). Sub-A = (a) public surface for dynamic_cast-free voice scans.
  - `bool isInRelease() const noexcept` — returns `mInRelease`. Tier 0 predicate.
  - `bool isNoteOffQueued() const noexcept` + `void setNoteOffQueued(bool v) noexcept` — Correction 3 look-ahead flag accessor pair.
  - `void initiateSteal() noexcept` — Correction 1 entry point for the ADSR quick-release override.
- **VibeVoice new private members**:
  - `std::atomic<bool> mIsActive { false };` — Sub-A = (a) explicit-atomic occupancy flag. Set true in `startNote` / cleared in `releaseResources` with release semantics. Read by `findStealCandidate`'s voice scan loop on the audio thread without acquiring locks.
  - `bool mInRelease { false };` — Tier 0 predicate state. Set true in `stopNote(velocity, allowTailOff=true)` BEFORE `mAdsr.noteOff()`; cleared in `startNote`. Read by `isInRelease()`.
  - `juce::ADSR::Parameters mPreStealAdsrParams {};` — Correction 1 save/restore buffer for the user's ADSR params when `initiateSteal()` overrides release to 1.5 ms.
  - `bool mAdsrOverridden { false };` — Correction 1 sentinel. True after `initiateSteal()`; cleared in `startNote` (after the user's params are restored).
  - `bool mNoteOffQueued { false };` — Correction 3 transient flag. Reset to false at the top of every `VibeSynth::renderNextBlock` (look-ahead pre-scan); set true for voices whose noteOff will be delivered this block.
- **VibeSynth changes**:
  - `kMaxVoices = 24` (was 16) — Correction 1 over-provisioning. 8 reserve voices for stolen-voice fade-out overflow.
  - `static constexpr int kLogicalCap = 16;` NEW — user-facing polyphony default. Existing voiceCap APVTS range (1..16) unchanged.
  - `std::array<VibeVoice*, kMaxVoices> mVoices {};` NEW — direct VibeVoice* cache populated alongside each `mSynth.addVoice(v)` in the ctor. Replaces every audio-thread `dynamic_cast<VibeVoice*>(mSynth.getVoice(i))` with a single pointer-array read. Sub-A = (a) hot-path optimization fully realized.
  - `VibeVoice* findStealCandidate(int newPitch) const noexcept;` NEW private — L7(b) 3-tier hybrid scan (see Section 1 Correction 2).
  - `forEachVoice` template refactored to iterate `mVoices[]` instead of dynamic_casting. Hot — called from every APVTS parameter change broadcast + every unison fan-out iteration.

#### `Source/VibePlayer/VibePlayerDSP.cpp`

- **VibeSynth ctor**: populate `mVoices[i] = v` alongside each `mSynth.addVoice(v)`. Direct pointer cache per Sub-A = (a).
- **`VibeVoice::setAdsr`**: routes new user setting into `mPreStealAdsrParams` (instead of `mAdsr.setParameters` directly) when `mAdsrOverridden == true`. Preserves the in-flight 1.5 ms quick-release; applies the new user setting on the next `startNote` via the restore path below. Without this guard, a user-driven ADSR change during a steal-in-progress would stomp the quick-release and either extend the fade-out indefinitely or cut it short audibly.
- **`VibeVoice::startNote` (top of body, after `releaseResources()` call)**: restore user's ADSR params if `mAdsrOverridden == true`, then clear `mAdsrOverridden`. End of body: `mIsActive.store(true, std::memory_order_release)` + `mInRelease = false`. Marks the voice as freshly active for the steal-candidate scan + restores the user's release time so the new note has its expected envelope shape.
- **`VibeVoice::stopNote(velocity, allowTailOff=true)`**: set `mInRelease = true` BEFORE `mAdsr.noteOff()` so `findStealCandidate` sees the Tier 0 state correctly on any same-block subsequent voice scan.
- **`VibeVoice::releaseResources()`**: clear `mIsActive` (`std::memory_order_release`) + clear `mInRelease`. `mAdsrOverridden` + `mPreStealAdsrParams` persist across `releaseResources` — they're cleared / restored in `startNote` instead. (Reasoning: `releaseResources` fires at fade-out end, but the next `startNote` is the natural restore point so `setAdsr` calls between fade-out-end and next-startNote still route correctly through the override guard.)
- **`VibeVoice::initiateSteal()` NEW**: idempotent override of the voice's ADSR release to 0.0015 sec. Saves the user's pre-steal `juce::ADSR::Parameters` into `mPreStealAdsrParams` + sets `mAdsrOverridden = true`. Calls `mAdsr.setParameters` with the override params. Idempotent on the "already overridden" case (saves again would clobber the saved user params with the quick-release params — the early-return prevents that).
- **`VibeSynth::findStealCandidate()` NEW**: 3-tier scan over `mVoices[]`. Within each tier, picks the oldest by `mNoteStartCounter`. Returns from the lowest-priority populated tier.
- **`VibeSynth::renderNextBlock` top of body NEW look-ahead pre-scan**: ~25 lines. Clear all `mNoteOffQueued` flags via `for (auto* v : mVoices) v->setNoteOffQueued(false)`. Walk the MIDI buffer; for each noteOff event, check if a later same-pitch noteOn would strip it (mirrors the existing same-pitch-strip logic) — if NOT stripped, find the voice currently playing that pitch and set `mNoteOffQueued = true`. Resets every block (transient flag; no cross-block state).
- **`VibeSynth::renderNextBlock` same-pitch preemption loop** at the pre-batch `:899-906`: replaced `dynamic_cast<VibeVoice*>(mSynth.getVoice(vi))` with `mVoices[vi]` iteration. Hot — fires on every note-on. Sub-A = (a) hot-path optimization.
- **`VibeSynth::renderNextBlock` voiceCap-stealing branch** at the pre-batch `:944-960`: full rewrite. Replaced the pre-batch oldest-first dynamic_cast loop + `oldest->stopNote(0.f, false)` (hard stop, no tail) with: (1) active-voice count scan over `mVoices[]` reading `mIsActive` atomics; (2) if count >= cap, call `findStealCandidate(note)`; (3) if a victim is found, `victim->initiateSteal(); victim->stopNote(0.f, true)` — the `allowTailOff=true` lets the quick ADSR release run naturally during subsequent renderNextBlock calls. Quick-release tails do NOT stack — multiple stolen voices fade out independently in their own slots.
- **`cap` calculation**: `const int cap = mLastVoiceCap > 0 ? mLastVoiceCap : kLogicalCap;` (was `kMaxVoices`). Critical fix-as-you-go: with `kMaxVoices=24` post-Correction-1, leaving `cap = kMaxVoices` would have given cap=24 by default, defeating the entire over-provisioning intent (the 8 reserve voices would be available for user-facing polyphony, not for stealing-fade-out overflow). `kLogicalCap=16` is the right default sentinel; user-set `voiceCap` still respects 1..16 as before.

#### Grep cleanliness post-edits

- `grep -rn "dynamic_cast<VibeVoice" Source/VibePlayer/` returns ZERO matches. All hot-path voice scans use `mVoices[]` per Sub-A = (a).
- All four new flag members + `mPreStealAdsrParams` accounted for in header.
- No `juce::ADSR::isInRelease()` calls anywhere — that API doesn't exist (Task 1 inventory Section 2 finding); `mInRelease` member tracking is the load-bearing replacement.

#### Build status

Both Release + Debug build clean. Only pre-existing warnings.

### Section 3 — Verify PASS (Jeff, 2026-05-25) across 3 successive rounds

**Round 1** — after over-provisioning + 2-tier + ADSR quick-release (Correction 1 only). Jeff reported the held-lead pathology: lead-note-held-across-looping-chord scenario steals the lead after 2 bars. Round 1 verify FAILED — surfaced the need for Correction 2.

**Round 2** — after 3-tier with `isKeyDown()` protection (Correction 1 + Correction 2). Jeff re-tested with the looping-chord-at-loop-boundary scenario. Lead still stolen "after 2 bars". Round 2 verify FAILED — surfaced the diagnosis that led to Correction 3 (MIDI-event-ordering at loop boundaries).

**Round 3** — after look-ahead noteOff pre-scan (Correction 1 + Correction 2 + Correction 3). Jeff confirmed his test setup matched the diagnosis: "16 sustained 1 bar notes over 4 bars (16notes each bar) and one played key continuously" (Scenario A) and "4 note looping pattern + 5th sustained" (Scenario B). Round 3 verify PASS — all scenarios cleared:

- **Scenario A** (16-chord-loop + held lead, default voiceCap=16): held lead survives all loop iterations. The 16 chord notes' noteOffs at the loop boundary flag the chord voices as `mNoteOffQueued = true` (Tier 1) BEFORE the next-loop chord's noteOns dispatch; new-chord noteOns steal the Tier 1 chord-voices via `findStealCandidate`; held lead stays in Tier 2 untouched.
- **Scenario B** (4-chord-loop + held lead, voiceCap=4): held lead survives all iterations. Same mechanism with 4 instead of 16 looping voices.
- **Scenario C regression check** (release-phase-preferred order at voiceCap=4 + 5 short-duration notes): 5th note's voiceCap-steal picks the oldest Tier 0 voice before any Tier 1 / Tier 2. Tier ordering preserved correctly.
- **Scenario D regression check** (all-keys-held fallback at voiceCap=4): hold 4 physical keys + tap a 5th. No Tier 0 or Tier 1 candidate available — `findStealCandidate` falls back to Tier 2, picks the oldest held key. Confirms the protection isn't infinite.
- **Scenario E** (Reverse mode + Scenarios A or B): reverse playback still works + steal still click-free.
- **Scenario F** (MT parity, MT-on vs MT-off): identical voice behavior across both transport-thread settings.

### Section 4 — Sub-spec calls + Lx clarifications captured during execution (worth re-noting in §5 STATUS banner at close)

Task 3's final shape includes four refinements to the original L1-L10 + Sub-A/B/C plan locks. None are blockers for batch close — Task 3 ships exactly as Jeff approved — but they ARE the as-shipped behavior and should be documented at QA-VoicePool close so the deliverable is recorded against the L1-L10 plan accurately:

- **L6 "64-sample fade-out" reinterpreted** — original blueprint-literal reading was "render 64 samples of fade audio synchronously". Final implementation is **ADSR quick-release with release=1.5 ms** (matches the 64-sample target at 44.1 kHz). Per Jeff's Correction 1 decision.
- **L7(b) "hybrid release-preferred fallback to overall-oldest" upgraded to 3-tier with key-down protection** — same release-preferred bias as locked, but adds the Tier 2 protect-held-keys layer that wasn't in the original L7(b) wording. The 3-tier ordering (release → noteOff-queued-or-key-released → key-down) matches the pro-DAW standard Jeff articulated mid-Correction-2. The original 2-tier "fallback overall-oldest" becomes Tier 2 within-tier ordering (still oldest-first), so L7(b) behavior is a strict superset of the locked spec.
- **NEW: physical over-provisioning** — `kMaxVoices=24`, `kLogicalCap=16`, 8 reserve voices. Not in the original L1-L10 spec calls; emerged from Correction 1 as the safe alternative to synchronous-render fade-out. The user-facing polyphony default and APVTS range are UNCHANGED.
- **NEW: noteOff look-ahead pre-scan** — `mNoteOffQueued` flag + ~25-line pre-scan loop at the top of `VibeSynth::renderNextBlock`. Not in the original L1-L10 spec calls; emerged from Correction 3. Preserves sample-accurate MIDI timing — does NOT reorder events; only annotates voices with "noteOff is coming this block".

### Section 5 — Rule 4 Diagnostic Instrumentation Catalog

Nil for Task 3 (no `DBG` / `juce::Logger::writeToLog` / temp `jassert` / debug `juce::AlertWindow` added during execution; all 3 design pivots resolved via static analysis + Jeff's verify rounds, no in-source diagnostics needed).

### Section 6 — Next action

Task 4 — `findRegion` `std::vector<int> candidates` → `std::array<int, 32>` stack-alloc per L5(a). Small orthogonal task (~30 min). Touches only `VibeSampleManager::findRegion` at [VibePlayerDSP.cpp:415-416](../../Source/VibePlayer/VibePlayerDSP.cpp:415) + the 4 read sites. After Task 4: Task 5 stress-file verify (no commit per QA-InsertMaps Task 3 precedent) → Task 6 cleanup + grep sweep → Task 7 close (the §9 Forks entry for the BaySickSynth `mOsc.reset()` finding routed to QA-EngineApvts per Jeff's Task 2 scope-discipline lock + the Section 4 L6 / L7(b) refinements captured as accepted-design notes vs literal-blueprint deviations both land in the close routing).

---

## 2026-05-25 — Task 4 — findRegion stack-alloc + SFZ <group> parser bug surfaced + routed to QA-SfzGroup

L5(a) source edit — small orthogonal task per the plan's task split. Single file modified ([VibePlayerDSP.cpp](../../Source/VibePlayer/VibePlayerDSP.cpp)); diff total +22 / -11 net, fully contained inside `VibeSampleManager::findRegion`. The fourth and final heap-alloc site identified at Task 1 inventory Section 1 is now closed; the audio-thread per-note allocation surface for VibePlayer is fully zero per the original §9 thirty-fourth Forks entry blueprint. Both Release + Debug build clean. Jeff-verified PASS via smoke test (Tuba-KS.sfz loaded, played across the keymap, no crash + audio works) — and the verify surfaced a load-bearing pre-existing bug in the SFZ `<group>` parser that is OUT of QA-VoicePool's scope but routed at Jeff's call to a new dedicated batch (QA-SfzGroup) slotted as the very next batch after this one closes.

### Section 1 — Task 4 source edit landed (L5(a) `std::array<int, 32>` stack-alloc replacement)

Single file modified ([VibePlayerDSP.cpp](../../Source/VibePlayer/VibePlayerDSP.cpp)). One surgical block + three loop-body conversions. No header touches.

#### `Source/VibePlayer/VibePlayerDSP.cpp` (+22 / -11)

- **Allocation site at the pre-batch [:415-416](../../Source/VibePlayer/VibePlayerDSP.cpp:415)** — replaced `std::vector<int> candidates; candidates.reserve(8);` with:

  ```cpp
  // Gather all candidates matching note + velocity + artic.  Use indices to
  // avoid iterator invalidation concerns.
  //
  // QA-VoicePool Task 4 (L5=(a)): std::array<int, kMaxCandidates> stack-alloc
  // replaces the pre-batch std::vector<int> + reserve(8), eliminating the
  // per-note-on heap allocation on the audio thread.  Cap of 32 covers every
  // realistic sample mapping (typical SFZ packs ship 2-8 RR variations per
  // (note, velocity, articGroup) tuple; heavily-layered packs rarely exceed
  // 16).  Overflow silently drops the 33rd+ candidate per Jeff's L5=(a) lock
  // at Task 0 ExitPlanMode ("UX-acceptable degradation for an edge case that
  // should never fire on Jeff's libraries").
  constexpr int kMaxCandidates = 32;
  std::array<int, kMaxCandidates> candidates {};
  int numCandidates = 0;
  ```

- **Push site** — `candidates.push_back(i);` replaced with bounded `if (numCandidates < kMaxCandidates) candidates[numCandidates++] = i;`. The bounded guard implements the Sub-overflow disposition Jeff locked at Task 0 ExitPlanMode: the 33rd+ candidate is silently dropped (no jassert, no log) per the "UX-acceptable degradation for an edge case that should never fire on Jeff's libraries" rationale. The 32-cap sufficiency was sanity-checked at Task 1 inventory Section 5 — worst-case region density for a single `(midiNote, velocity, articGroup)` tuple is the round-robin sample stack; typical SFZ packs ship 2-8 RR variations per zone, heavily-layered packs rarely exceed 16, 32 has comfortable headroom.

- **Emptiness check** — `candidates.empty()` replaced with `numCandidates == 0`.

- **Iteration sites (two)** — both `for (int idx : candidates)` range-fors (the hasRR pre-scan loop + the round-robin selection loop) replaced with index-counted `for (int j = 0; j < numCandidates; ++j) { int idx = candidates[j]; ... }`. The std::array's full storage is never iterated — only the populated prefix `[0, numCandidates)` — matching the pre-batch std::vector behavior exactly. No spurious work on the unused tail.

#### Grep cleanliness post-edits

- `grep -rn "std::vector<int> candidates" Source/VibePlayer/` returns ZERO matches. Heap-alloc kill complete.
- `grep -rn "candidates.push_back\|candidates.empty\|candidates.reserve" Source/VibePlayer/` returns ZERO matches. All vector-API call sites are gone.

#### Build status

Both Release + Debug build clean: `RELEASE_EXIT_CODE=0`, `DEBUG_EXIT_CODE=0`. Only pre-existing warnings survive — no new warnings introduced by Task 4.

#### Scope discipline

Task 4 is purely orthogonal to Task 2 (no fat-voice state touched) and Task 3 (no `dynamic_cast` involved, no voice-pool or stealing logic touched). The L5(a) `findRegion` swap lands as its own clean small commit per L8(b)'s split-task discipline — heap-alloc surface 4 of 4 closed independently from the structural voice-pool work. With Task 4 in, the original §9 thirty-fourth Forks entry blueprint's heap-alloc-elimination scope is fully realized: zero heap allocations on the audio thread per VibePlayer note-on.

### Section 2 — Verify PASS (Jeff, 2026-05-25)

**Test setup**: loaded `Tuba-KS.sfz` (Core Library / Brass Pack) into BaySickPlayer. Played across the keymap. Smoke-test-level — no extended polyphony stress (that's Task 5 territory).

**Result**: PASS. No crash. Audio works. No functional regression vs pre-batch.

**Load-bearing secondary finding from this verify**: round-robin variation was **NOT clearly audible** when the same key was struck repeatedly on Tuba-KS — Jeff reported "they all sound the same though and I've never noticed it making any sort of variation like that". This pulled at a thread that turned into the Section 3 finding — the L5(a) `findRegion` swap is functioning correctly, the RR rotation counter is iterating correctly, the candidate list is populated correctly, but the candidate list itself is degenerate (length 1 per `(note, velocity, artic)` tuple instead of length 4 as the SFZ file specifies). The verify was correct for Task 4's scope; the absent RR variation is a pre-existing SFZ parser bug that the verify-time inspection uncovered.

Per L10 verify-cadence (Debug-then-Release per QA-InsertMaps norm + QA-Md MT-works-in-Debug fact). Task 4 verifies clean.

### Section 3 — Pre-existing SFZ `<group>` parser bug surfaced (routed to NEW QA-SfzGroup batch per Rule 3 + Jeff's scope call)

The verify-time absent-RR observation triggered a two-track investigation Jeff explicitly asked for ("To confirm I don't just mean missing in the SFZ file but in how we are reading them so check both"). Both tracks landed concrete findings; the second is the load-bearing bug.

#### Track 1 — SFZ file inspection (Tuba-KS.sfz under Core Library)

Inspected the file directly. The file DOES specify 4-variant round-robin cycling via `<group>`-scoped `seq_length` + `seq_position` opcodes per the SFZ v1 spec:

```
<group>
seq_length=4
seq_position=1
...
<region> sample=tuba_C2_v1.wav ... </region>

<group>
seq_length=4
seq_position=2
...
<region> sample=tuba_C2_v2.wav ... </region>
```

Each `<group>` block declares its own `seq_position` (1, 2, 3, 4); the following `<region>` is meant to inherit that group-scoped opcode per SFZ v1 inheritance rules. So the file content is correct — the RR cycling IS specified in the source data.

#### Track 2 — Parser source inspection (`VibePlayerDSP.cpp` `parseSFZ`)

The bug is in our parser, not the file. The line-walking state machine in [`parseSFZ` at :90](../../Source/VibePlayer/VibePlayerDSP.cpp:90) sets `inRegion = true` only on `<region>` headers (at [:120](../../Source/VibePlayer/VibePlayerDSP.cpp:120)); on `<group>` headers it resets `inRegion = false` (at [:106](../../Source/VibePlayer/VibePlayerDSP.cpp:106)). The per-line opcode-extraction block contains an early-return `if (!inRegion) continue;` at [:148](../../Source/VibePlayer/VibePlayerDSP.cpp:148) BEFORE any opcode-write to the current region. Result: **every opcode inside a `<group>` block (including `seq_length` / `seq_position` and any other group-scoped inherited setting) is silently dropped**. Per the SFZ v1 spec, opcodes inside `<group>` are supposed to be inherited by every `<region>` that follows until the next `<group>` or EOF — our parser implements zero of this inheritance.

For Tuba-KS specifically: every region's `seq_position` is dropped → every region defaults to `seq_position=0` (no rotation gate) → `findRegion` sees one candidate per `(note, velocity, artic)` tuple → no rotation possible → the 4-variant cycling specified in the file never reaches the audio thread. Audible result is "they all sound the same".

This is a **pre-existing bug, NOT a Task 4 regression**. The early-return predates QA-VoicePool entirely; Task 4 only touched `findRegion`, not `parseSFZ`. It's also completely independent of QA-VoicePool's scope — the parser runs on the message thread (file load time), not the audio thread (heap-alloc territory). The reason Task 4's verify caught it is that the verify exercised the candidate list directly: the new stack-alloc populates the same data the old vector did, but with the data corrupted upstream by the missing inheritance, the populated list is degenerate.

#### Spec-call surface and Jeff's routing decision

Spec-call surface: (1) new dedicated batch (e.g. QA-SfzGroup) / (2) fold into QA-VoicePool close-routing / (3) fold into a downstream batch (QA-EngineApvts, etc.) / (4) §9 Forks entry only, slot later.

Jeff's decision 2026-05-25, verbatim: **"Incredible catch... We are going with Option 1: New dedicated batch (e.g., QA-SfzGroup). I want this fixed, but our strict rollback boundaries must remain intact. This current batch is strictly about real-time audio-thread heap allocations. The SFZ loader is a text parser running on the message thread. Let's queue up QA-SfzGroup to be our very next batch after we close QA-VoicePool. That batch will cover fixing the `<group>` state-machine inheritance and investigating the Aria/sfizz RR loss."**

Rollback-boundary discipline lock — same scope-purity reasoning Jeff applied at Task 2's BaySickSynth `mOsc.reset()` finding (Section 4 of Task 2's entry). QA-VoicePool stays scope-pure: real-time audio-thread heap allocations only, no parser changes, no DSP state changes.

#### QA-SfzGroup batch scope (locked at this surface for the §5 entry that lands at QA-VoicePool close)

- Fix the `<group>` opcode-inheritance state machine in `parseSFZ`. The `if (!inRegion) continue;` early-return at [VibePlayerDSP.cpp:148](../../Source/VibePlayer/VibePlayerDSP.cpp:148) is wrong; opcodes inside a `<group>` should accumulate into a group-default state that the next `<region>` inherits as its baseline. Implementation outline: track a `VibeRegion mGroupDefaults` accumulator; on `<group>` header reset the accumulator; on `<region>` header copy the accumulator into the new region; route opcode writes to the accumulator while `!inRegion` and to the current region while `inRegion`.
- Investigate the Aria-player + sfizz-driven engines (BaySickRustyDrums, BaySickGuitars, BaySickBasses) for the same RR-loss symptom Jeff has noticed historically. Confirm whether the sfizz code path has its own equivalent state-machine gap or whether the issue is in the file content / loader handoff. Two independent investigations — VibePlayer's hand-rolled parser vs sfizz's library parser — bundled into the same batch because they share the symptom even if not the cause.
- Slot: very next batch after QA-VoicePool close. Sequencing arrow update at close: `... -> QA-VoicePool -> QA-SfzGroup -> QA-EngineApvts -> QA-Ed -> ...`.

#### Action at QA-VoicePool close (per Rule 3 routing-at-close)

- Write §9 Forks entry recording the SFZ `<group>` finding + QA-SfzGroup routing + Jeff's rollback-boundary rationale (mirrors the Task 2 Section 4 BaySickSynth `mOsc.reset()` routing pattern; both close-routings cite the same scope-discipline reasoning).
- Write NEW §5 QA-SfzGroup batch entry — scope (the two-part state-machine-fix + sfizz-investigation surface above) + slot (next-batch-after-QA-VoicePool).
- Update §6 sequencing arrow to insert QA-SfzGroup between QA-VoicePool and QA-EngineApvts.
- **No source change in QA-VoicePool itself.** Parser source stays untouched this batch.

### Section 4 — Rule 4 Diagnostic Instrumentation Catalog

Nil for Task 4 (no `DBG` / `juce::Logger::writeToLog` / temp `jassert` / debug `juce::AlertWindow` added — the `findRegion` swap is a pure data-structure change, the SFZ parser finding was diagnosed via static reads of the source + the file, not via in-source instrumentation).

### Section 5 — Next action

Task 5 — stress-file verify (NO commit per L8(b) split + QA-InsertMaps Task 3 precedent). Heavy SFZ libraries, long sustained sessions, voice-pool stress to confirm no audible regressions vs pre-batch across Task 2 + Task 3 + Task 4 combined. After Task 5: Task 6 cleanup + grep sweep -> Task 7 close. Close-routing pass lands five docs touches:

1. §9 Forks entry for the BaySickSynth `mOsc.reset()` finding routed to QA-EngineApvts (per Task 2 Section 4 lock).
2. §9 Forks entry for the SFZ `<group>` parser finding routed to NEW QA-SfzGroup (per Section 3 above).
3. NEW §5 QA-SfzGroup batch entry — scope + slot.
4. §5 STATUS banner update for Task 3's Correction 1 / 2 / 3 deviations from the literal L1-L10 spec calls (per Task 3 Section 4 lock).
5. §6 sequencing arrow update to insert QA-SfzGroup between QA-VoicePool and QA-EngineApvts.

---

## 2026-05-25 — Task 6 — Cleanup + grep sweep (no source changes; all sweep targets clean)

Cleanup + grep sweep pass per the plan's task-split discipline. Zero source diffs in this task; build status carries forward unchanged from Task 5's last clean Release + Debug build. Task 6's deliverable is the formal sweep + this running-notes entry — sweep results captured for the close-pass Implemented Work Log compilation + the §5 STATUS banner reference at close. Six targeted greps run across `Source/` (or `Source/VibePlayer/` where the scope is engine-local); all six returned ZERO hits. The audio-thread per-note heap-alloc surface for VibePlayer is confirmed fully closed by the grep evidence + the structural Tasks 2-4 commits are confirmed fully internally consistent (no orphaned references to the killed members / classes / patterns lingering anywhere in the codebase).

### Section 1 — Grep sweep results (all six clean)

Six sweeps run; ZERO hits on all six. Each grep maps to a specific structural deliverable across Tasks 2-4 and confirms the no-stale-references invariant at close.

- **Sweep 1: pre-batch unique_ptr source members** — `grep -rn "mResampSrc\|mMemSrc" Source/` returns ZERO matches. The pre-batch VibeVoice unique_ptr members (`std::unique_ptr<juce::PositionableAudioSource> mMemSrc;` + `std::unique_ptr<juce::ResamplingAudioSource> mResampSrc;`) are fully gone, replaced by Task 2's fat-voice members (`mForwardSrc` / `mReverseSrc` / `mForwardResamp` / `mReverseResamp` / `mActiveSrc` / `mActiveResamp`). No stale documentation references or carry-over comments anywhere in `Source/`.

- **Sweep 2: hot-path `dynamic_cast<VibeVoice*>`** — `grep -rn "dynamic_cast<VibeVoice" Source/` returns ZERO matches. All hot-path voice scans use the `mVoices[]` direct pointer cache per Task 3's Sub-A=(a) realization (cache populated alongside each `mSynth.addVoice(v)` in the VibeSynth ctor; read on the audio thread at `forEachVoice` template + same-pitch preemption loop + voiceCap-stealing branch + look-ahead noteOff pre-scan). RTTI cost stripped from the entire audio thread per Sub-A's hot-path optimization intent.

- **Sweep 3: pre-batch `findRegion` vector allocation** — `grep -rn "std::vector<int> candidates\|candidates.push_back\|candidates.empty\|candidates.reserve" Source/VibePlayer/` returns ZERO matches. The pre-batch `std::vector<int> candidates; candidates.reserve(8);` + the 3 call-site uses (`candidates.push_back` / `candidates.empty()` / range-fors) are fully gone, replaced by Task 4's L5=(a) `std::array<int, 32>` stack-alloc + `numCandidates` count + bounded-write push + index-counted iteration. Heap-alloc surface 4 of 4 closed by grep evidence.

- **Sweep 4: per-note-on `make_unique` sites** — `grep -rn "make_unique<juce::MemoryAudioSource>\|make_unique<juce::ResamplingAudioSource>\|make_unique<ReversedMemoryAudioSource>" Source/` returns ZERO matches. The 3 pre-batch per-note-on heap-alloc sites at [VibePlayerDSP.cpp:581 / :583 / :607](../../Source/VibePlayer/VibePlayerDSP.cpp:581) are fully gone, replaced by Task 2's fat-voice re-pointing pattern (`setBuffer` + `flushBuffers` + `setResamplingRatio` on the active resampler). Heap-alloc surface 1-3 of 4 closed by grep evidence.

- **Sweep 5: pre-Task-3 `kMaxVoices` literal** — `grep -rn "kMaxVoices\s*=\s*16" Source/VibePlayer/` returns ZERO matches. Constant was raised 16 -> 24 in Task 3's Correction 1 over-provisioning (8 reserve voices for stolen-voice ADSR-quick-release fade-out overflow); the old `16` literal value should not survive anywhere as a hardcoded reference. The user-facing `kLogicalCap = 16` is the right post-batch sentinel for user-visible polyphony defaulting; existing `voiceCap` APVTS range (1..16) unchanged.

- **Sweep 6: diagnostic instrumentation residue** — `grep -rn "DBG(\|juce::Logger::writeToLog\|temp_jassert\|temporary.*jassert" Source/VibePlayer/` returns ZERO matches. No `DBG` / `juce::Logger::writeToLog` / temp `jassert` / debug `juce::AlertWindow` added during any of Tasks 0-4 — confirms the running-tally of "nil for Task N" in every prior Rule 4 Diagnostic Instrumentation Catalog section across all tasks. This sweep is the formal "nothing accumulated" close-out per the Rule 4 expectation that the catalog is walked + strip list surfaced before close; the catalog table at the top of this file remains empty as locked, no strip pass needed.

### Section 2 — Comments / breadcrumbs intentionally preserved (KEEP decisions)

Audit pass on every in-source comment / tag added during Tasks 0-4 to decide KEEP-or-STRIP at close. All preserved verbatim — the in-source provenance markers pair with the §5 entry + the running notes for full traceability and cost nothing in line count.

- **`Source/VibePlayer/VibePlayerDSP.cpp:466-469`** — the four-line `"QA-VoicePool Task 2 (2026-05-25): ReversedMemoryAudioSource + new sibling VibeForwardMemoryAudioSource moved to VibePlayerDSP.h..."` comment sits in dead space between `findRegion` and `VibeVoice` ctor. The breadcrumb is the intentional Task 2 navigational pointer so external readers searching `ReversedMemoryAudioSource` in the cpp see the rename history without bouncing to the header. **Decision: KEEP.** The breadcrumb is short + tag-prefixed + lives between functions where it costs nothing. Removing it would leave grep'ers looking for the class definition in the cpp confused with no signal that the class moved.

- **All `// QA-VoicePool Task N: ...` tag prefixes on the new headers + impl blocks** — `// QA-VoicePool Task 2: ...` on the new fat-voice members + resampler ctor-init in the VibeVoice header; `// QA-VoicePool Task 3: ...` on the new atomic / flag / params members + `findStealCandidate` + `initiateSteal` + look-ahead pre-scan body + the over-provisioning constants in both header and cpp; `// QA-VoicePool Task 4 (L5=(a)): ...` on the `findRegion` constexpr cap + array declaration. **Decision: KEEP all verbatim.** These are the in-source provenance markers for the post-batch reader; they pair with §5 + the running notes for full traceability and follow the same pattern as the QA-InsertMaps / QA-AudioMeters / QA-Ea / QA-Eg in-tree precedent.

### Section 3 — Rule 4 Diagnostic Instrumentation Catalog

Nil for Task 6 (no source touched; no instrumentation to strip). Sweep 6 above is the formal walk + strip-list surface per the Rule 4 close expectation — the running-tally across Tasks 0-4 already confirmed zero instrumentation accumulated; the catalog table at the top of this file remains empty as locked, no strip pass needed. The Rule 4 close discipline is satisfied with the grep evidence.

### Section 4 — Next action

Task 7 — close sequence. Five docs deliverables per Task 4 Section 5:

1. §9 Forks entry for the BaySickSynth `mOsc.reset()` finding routed to QA-EngineApvts (per Task 2 Section 4 lock).
2. §9 Forks entry for the SFZ `<group>` parser finding routed to NEW QA-SfzGroup (per Task 4 Section 3 routing).
3. NEW §5 QA-SfzGroup batch entry — scope + slot.
4. §5 QA-VoicePool STATUS banner update for Task 3's Correction 1 / 2 / 3 deviations from the literal L1-L10 spec calls (per Task 3 Section 4 lock).
5. §6 sequencing arrow update to insert QA-SfzGroup between QA-VoicePool and QA-EngineApvts.

Plus the standard close-pass: `/draft-doc batch-close` -> Implemented Work Log append + `/review-batch QA-VoicePool` -> address any BLOCKER / NEEDS-FIX (NITs per `feedback_qa_batches_fix_bugs_dont_defer.md` -> fix-or-reframe canon) + `/draft-commit` -> close commit landing all five docs touches + the running-notes close-pass section.

---

## 2026-05-26 — Task 7 — NIT fix-up (NITs 1 / 3 / 6 fixed; NITs 2 / 4 / 5 reframed as accepted design)

`/review-batch QA-VoicePool` close-pass pass surfaced six NITs with zero BLOCKER + zero NEEDS-FIX; recommendation **READY-TO-COMMIT**. Per `feedback_qa_batches_fix_bugs_dont_defer.md` extended to close-pass NITs (QA-InsertMaps Task 5 fix-up at `e9fe545` established the precedent — close-pass NITs follow the same fix-or-reframe canon as mid-batch findings, no bulk-defer-to-routing-table anti-pattern that QA-AudioMeters Task 5 fix-up at `2cba7b7` originally set), each NIT got Jeff's explicit fix-or-reframe disposition. **Three FIXES landed** as a Task 7 fix-up commit (NITs 1 / 3 / 6 — all source-comment / clamp-symbol-swap, no behavior changes); **three REFRAMES** captured here as accepted-design rationale for the post-batch reader (NITs 2 / 4 / 5 — all small academic memory-hygiene smells where the as-shipped behavior is intentional and the alternative would be worse UX). Diff total for the fix-up commit: 2 files changed, +20 / -3 net. Both Release + Debug build clean post-fix-up. No verify needed — all three FIXES are comment additions + one clamp-symbol swap with no observable runtime behavior change.

### Section 1 — `/review-batch` close-pass results summary

`/review-batch QA-VoicePool` dispatched 2026-05-26 (Sonnet). Recommendation: **READY-TO-COMMIT**. Findings:

- **BLOCKER**: 0.
- **NEEDS-FIX**: 0.
- **NIT**: 6 total. NIT 1 (voiceCap clamp upper bound = kMaxVoices instead of kLogicalCap — forward-compat hazard for the 8 reserve voices). NIT 2 (`juce::Synthesiser::findVoiceToSteal` fallback may run in 24-voice catastrophic-overflow scenario + pick a different victim than L7(b)). NIT 3 (declaration-order subtle dependency in VibeVoice header — `mForwardSrc` / `mReverseSrc` MUST precede `mForwardResamp` / `mReverseResamp` because the resampler member-init expressions take their addresses). NIT 4 (`mAdsrOverridden` + `mPreStealAdsrParams` survive `releaseResources()` — startNote is the natural restore point, existing source comment terse). NIT 5 (`mAdsrOverridden` cleared only in `startNote` — stays true forever if voice is stolen + faded + never re-triggered). NIT 6 (stale lifecycle comment at VibePlayerDSP.h:251 saying "create MemoryAudioSource + ResamplingAudioSource" — post-Task-2 the lifecycle re-points fat sources rather than creating anything).

Jeff's dispositions (full verbatim approval of my proposed picks 2026-05-26): **FIX NIT 1 / NIT 3 / NIT 6** (all surgical comment / clamp-symbol touches, no behavior risk, all guard against latent future hazards or stale documentation); **REFRAME NIT 2 / NIT 4 / NIT 5** as accepted design (NIT 2: disabling JUCE fallback would drop notes, worse UX than imperfect-victim selection in 25-note catastrophic overflow; NIT 4 + NIT 5: purely academic memory hygiene; startNote state reconciliation handles it safely). All three reframes have running-notes-grade rationale recorded in Section 3 below so the deeper reasoning survives close.

### Section 2 — NITs 1 / 3 / 6 — FIXES applied (source touches)

Three surgical edits across two files: VibePlayerDSP.cpp (+9 / -1) for NIT 1, VibePlayerDSP.h (+11 / -1) for NIT 3 + NIT 6. Net +20 / -3. No new code paths, no behavior changes — all three fixes are documentation hardening (NITs 3 + 6 are source-comment touches; NIT 1 is a single-symbol swap that has no observable effect today since the APVTS range never triggers the difference, but closes a latent forward-compat hazard).

#### NIT 1 — `VibeSynth::setVoiceCap` clamp upper bound: kMaxVoices -> kLogicalCap

**Hazard**: clamp at [Source/VibePlayer/VibePlayerDSP.cpp:1360](../../Source/VibePlayer/VibePlayerDSP.cpp:1360) wrote `juce::jlimit(1, kMaxVoices, cap)` where `kMaxVoices = 24` post-Task-3-Correction-1 over-provisioning. The APVTS `voiceCap` range is currently 1..16 (registered with the existing VibePlayerProcessor range; no public path drives the clamp upper bound beyond it). So today the clamp never actually fires — every legal `cap` is already <=16 by the time it reaches `setVoiceCap`. But if the APVTS range ever bumps OR a saved-state `voiceCap=20` from a future version loads into the current binary, the clamp would silently let `mLastVoiceCap=20` through, and the voiceCap-stealing branch's `cap` calculation would consume 4 of the 8 reserve voices for user-facing polyphony. The fade-out mechanic per Task 3's Correction 1 physical over-provisioning depends on those 8 reserve voices being available for stolen-voice ADSR-quick-release overflow — consuming any of them for user-visible polyphony breaks the safety guarantee.

Jeff verbatim 2026-05-26: "We absolutely must protect those 8 reserve voices to ensure the fade-out mechanic never breaks."

**FIX applied**: clamp upper bound changed `kMaxVoices` -> `kLogicalCap`. Added 7-line explanatory comment above the clamp documenting the kMaxVoices vs kLogicalCap distinction + why kLogicalCap is the right value here even though kMaxVoices is the physical pool size.

```cpp
void VibeSynth::setVoiceCap (int cap) noexcept
{
    // Enforcement happens in renderNextBlock manual dispatch (oldest-first steal).
    // QA-VoicePool Task 7 NIT 1 fix-up: clamp upper bound is kLogicalCap (16),
    // NOT kMaxVoices (24).  The user-facing polyphony range is 1..16; the 8
    // reserve voices (kMaxVoices - kLogicalCap) are RESERVED for stolen-voice
    // ADSR-quick-release fade-out overflow per Task 3 Correction 1 physical
    // over-provisioning.  Clamping at kMaxVoices here would let a future APVTS
    // range bump (or a saved-state voiceCap=20 from a future version) silently
    // consume the reserve voices and break the fade-out mechanic.
    if (cap == mLastVoiceCap) return;
    mLastVoiceCap = juce::jlimit (1, kLogicalCap, cap);
}
```

#### NIT 3 — Declaration-order subtle dependency in VibeVoice header (strict-warning comment)

**Hazard**: resampler member-init expressions at [Source/VibePlayer/VibePlayerDSP.h:349-357](../../Source/VibePlayer/VibePlayerDSP.h:349) read `mForwardResamp { &mForwardSrc, false, 2 }` + `mReverseResamp { &mReverseSrc, false, 2 }` — they take the address of `mForwardSrc` / `mReverseSrc` declared above. C++ member-initialization order is declaration order within the same access section (independent of init-list order); for the address-taking to reference a fully-constructed object, the source members must precede the resamplers in declaration order. The Task 2 entry's Section 2 already flagged this constraint internally ("Declaration order matters — `mForwardSrc` / `mReverseSrc` are declared BEFORE their respective resamplers...") but there was no in-source warning at the actual class declaration site. A drive-by alphabetize pass (running through the alphabet would order `mForwardResamp` BEFORE `mForwardSrc`) or an accident-of-refactor reorder during a future audit would silently produce a use-of-uninitialized-member at VibeVoice construction and segfault on the first note-on. No compiler warning would catch the reorder — the resamplers' ctor takes a pointer, and a pointer to an uninitialized class member is valid C++ syntax at construction time.

Jeff verbatim 2026-05-26: "We don't want anyone alphabetizing the header and causing a segfault."

**FIX applied**: 8-line strict warning comment added in the source-class declaration block above `mForwardSrc`:

```cpp
// IMPORTANT (QA-VoicePool Task 7 NIT 3 fix-up): mForwardSrc + mReverseSrc
// MUST be declared BEFORE mForwardResamp + mReverseResamp.  The resampler
// member-init expressions below take `&mForwardSrc` / `&mReverseSrc` at
// construction time; C++ requires the referenced member to be fully
// constructed before the reference is taken, which means strict
// declaration order matters.  Do NOT reorder.  A drive-by alphabetize or
// accident-of-refactor reorder will silently produce a use-of-uninitialized-
// member at VibeVoice construction and segfault on the first note-on.
```

#### NIT 6 — Stale lifecycle comment at VibePlayerDSP.h:251 (single-line rewrite)

**Hazard**: lifecycle comment at [Source/VibePlayer/VibePlayerDSP.h:251](../../Source/VibePlayer/VibePlayerDSP.h:251) read `// startNote -> find region -> create MemoryAudioSource + ResamplingAudioSource`. Post-Task-2 the lifecycle does NOT "create" anything per note — it re-points the fat sources via `setBuffer` + flushes + re-targets the active resampler via `flushBuffers` / `setResamplingRatio`. The "create" language survived from the pre-batch heap-alloc-per-note pattern and is now misleading to a fresh reader. Task 6 grep sweep missed it because the stale comment text doesn't match any of the swept patterns (Sweep 1: `mResampSrc` / `mMemSrc` / Sweep 2: `dynamic_cast<VibeVoice` / Sweep 3: `std::vector<int>` / Sweep 4: `make_unique<*>` / Sweep 5: `kMaxVoices = 16` / Sweep 6: `DBG(`) — the words "create MemoryAudioSource" weren't in any sweep target.

**FIX applied**: single-line comment rewrite:

```cpp
//   startNote   -> find region -> re-point fat sources -> reset ADSR / filter / LFO
```

### Section 3 — NITs 2 / 4 / 5 — REFRAMED as accepted design

Three NITs that surface real-but-minor edge cases where the spec-call surfaces would either degrade UX (NIT 2) or be purely cosmetic memory-hygiene cleanup with no observable behavior change (NITs 4 / 5). All three reframed by Jeff with explicit rationale; documented here so the post-batch reader sees the design intent + understands why the as-shipped code is not "missing a fix" but is "intentionally chosen behavior".

#### NIT 2 — JUCE's `findVoiceToSteal` fallback may run in 25-note 24-voice catastrophic overflow

**Scenario**: 24 voices all mid-fade (all 16 user voices were just stolen + the 8 reserve voices got stolen too). 25th note arrives. L7(b)'s `findStealCandidate` picks a Tier 0 victim. The stolen victim still reports `isVoiceActive() == true` until ADSR-end + `clearCurrentNote()` in a future renderNextBlock. JUCE's `findFreeVoice` runs next + finds zero inactive voices. JUCE then falls back to its OWN `findVoiceToSteal` algorithm (protect highest/lowest pitches + steal oldest among the rest) which MAY pick a different voice than ours did.

**Spec-call surface**: (1) disable JUCE's fallback via `mSynth.setNoteStealingEnabled(false)` in VibeSynth ctor — guarantees L7(b) is the sole stealing path; the 25th note in a 24-fading scenario gets DROPPED instead of taking a slightly-different victim / (2) REFRAME as accepted design.

**Jeff's call: REFRAME.** Verbatim: "Disabling JUCE's fallback (NIT 2) would result in dropped notes, which is worse UX than an imperfect steal in a 25-note catastrophic overflow."

**Accepted-design rationale**: In the realistic worst case (16-note voiceCap, 8 reserve voices, all 24 mid-fade), the 25th note IS rare + audibly indistinguishable between L7(b)-picked victim and JUCE-picked victim. Both algorithms steal "the oldest" within their respective populations; the only divergence is L7(b)'s 3-tier ordering (release > noteOff-queued > key-down with oldest-within-tier) vs JUCE's protect-highest/lowest bias. At the point where the user has 24 mid-fade voices in flight, they are already past graceful behavior — the engine is buying them a note, any note, in preference to silence. The choice here prioritizes hearing the new note over hearing the user's "perfect" steal. Aligns with Jeff's L7(b) lock at Task 0 ExitPlanMode: "We never want to drop a new note."

#### NIT 4 — `mAdsrOverridden` + `mPreStealAdsrParams` survive `releaseResources()`

**Scenario**: voice gets stolen via Task 3 path — `initiateSteal()` sets `mAdsrOverridden = true` + saves the user's ADSR params into `mPreStealAdsrParams`. Voice plays out its 1.5 ms quick-release. At fade-end, `clearCurrentNote()` calls `releaseResources()`. Task 3's `releaseResources()` body clears `mIsActive` + `mInRelease` but does NOT clear `mAdsrOverridden` + does NOT restore `mPreStealAdsrParams` to `mAdsr`. The save/restore happens in `startNote` instead at [Source/VibePlayer/VibePlayerDSP.cpp:548](../../Source/VibePlayer/VibePlayerDSP.cpp:548). Existing source comment at cpp:507-509 is accurate but terse.

**Spec-call surface**: (1) expand the source comment to capture the running-notes deeper reasoning / (2) REFRAME as accepted design.

**Jeff's call: REFRAME.** Verbatim: "NIT 4... [is] purely academic memory hygiene; startNote state reconciliation handles it safely."

**Accepted-design rationale**: The state persists harmlessly across `releaseResources` because the next `startNote` is the natural restore point. `setAdsr` calls between fade-out-end and next-startNote route correctly through the `mAdsrOverridden`-guarded path in `setAdsr` itself — incoming user-driven param changes get diverted to `mPreStealAdsrParams` (not stomping the in-flight quick-release params) when `mAdsrOverridden == true`. The behavior is deterministic + already documented in this running-notes file at Task 3 Section 2.

#### NIT 5 — `mAdsrOverridden` cleared only in `startNote` — dangles forever if voice never re-triggered

**Scenario**: voice gets stolen, plays out its 1.5 ms quick-release, hits `clearCurrentNote()` + `releaseResources()`. Per NIT 4, `mAdsrOverridden` stays true post-`releaseResources()`. User changes engines or stops playback for hours. `mAdsrOverridden` stays true forever (the voice is never re-triggered, so the `startNote` restore path never fires). Memory cost: one bool + one `juce::ADSR::Parameters` struct per voice. Already accounted for in the per-voice RAM budget.

**Spec-call surface**: (1) clear `mAdsrOverridden` in `releaseResources` instead of waiting for next `startNote` / (2) REFRAME as accepted design.

**Jeff's call: REFRAME.** Verbatim: "NIT 4 and 5 are purely academic memory hygiene; startNote state reconciliation handles it safely."

**Accepted-design rationale**: Same shape as NIT 4 — `mAdsrOverridden` + `mPreStealAdsrParams` form a save/restore pair. If the next `startNote` never fires, the saved state is never observed, so the dangling-true flag has zero observable effect. The "clear in startNote on next-note-on" pattern is also slightly more defensive: if any future code path were to read `mAdsrOverridden` between fade-out-end + next-startNote, the flag would correctly report "yes, this voice's ADSR has been overridden" — which is true until the next startNote restores it.

### Section 4 — Build status

Both Release + Debug build clean after the 3 NIT fixes. `RELEASE_EXIT_CODE=0`, `DEBUG_EXIT_CODE=0`. Only pre-existing warnings survive. Jeff-confirmed `do_build.bat` post-fix-up: "Builds clean". No runtime verify needed for this fix-up — all three FIXES are non-behavioral: NIT 1 swaps a clamp upper-bound symbol that the current APVTS range never triggers (no observable runtime change); NIT 3 adds a strict-warning comment (no code touched); NIT 6 rewrites a comment line (no code touched).

### Section 5 — Rule 4 Diagnostic Instrumentation Catalog

Nil for Task 7 NIT fix-up (no `DBG` / `juce::Logger::writeToLog` / temp `jassert` / debug `juce::AlertWindow` added — all 3 fixes are comment additions + one symbol swap; no runtime instrumentation surface). Cross-task running tally across Tasks 0-4 + Task 6 was already nil per the prior Rule 4 sections + Sweep 6 confirmation; Task 7 NIT fix-up continues the nil tally.

### Section 6 — Next action

QA-VoicePool close commit lands the five docs deliverables per Task 6 Section 4 + the running-notes close-pass section appended below this entry. Drafters fire in parallel for the five touches: §9 Forks entry for BaySickSynth `mOsc.reset()` -> QA-EngineApvts (per Task 2 Section 4 lock); §9 Forks entry for SFZ `<group>` parser -> NEW QA-SfzGroup (per Task 4 Section 3 routing); NEW §5 QA-SfzGroup batch entry (scope + slot); §5 QA-VoicePool STATUS banner update for Task 3's Correction 1 / 2 / 3 deviations from the literal L1-L10 spec calls (per Task 3 Section 4 lock); §6 sequencing arrow update to insert QA-SfzGroup between QA-VoicePool and QA-EngineApvts. Plus the Implemented Work Log batch-close entry compiled by `/draft-doc batch-close` reading this file as primary input. Surface all five drafts + the commit message draft to Jeff for approval; commit via `git commit -F <file>` per the CLAUDE.md Git Commit Mechanics rule.

---

## 2026-05-26 — Close-pass — QA-VoicePool batch closed

Close commit landed all five docs deliverables in a single commit (SHA TBD — recorded post-commit by the parent session):

1. **`Plans & Specs/Implemented Work Log.md`** — full QA-VoicePool batch-close entry appended at the bottom of the chronological log (oldest-first per file convention).  Entry covers all 7 executable tasks (`a1211cd` open / `bddcaa6` Task 1 / `c42e729` Task 2 / `0dcfe50` Task 3 / `add0bfc` Task 4 / Task 5 verify-only / `f49fbe4` Task 6 / `36fe7fb` Task 7 NIT fix-up) + the close commit + all 10 findings (FND-1 through FND-10) with routing dispositions + the 6-NIT close-pass disposition table (3 FIXED, 3 REFRAMED, 0 deferred) + Carry-Forward §1 contradiction recording for the fat-voice architectural change.

2. **`Plans & Specs/Main Plan.md` §5 QA-VoicePool STATUS banner** — inserted between the `#### **QA-VoicePool: ...** *(NEW — inserted 2026-05-24)*` heading and the `**Plan file:**` line.  Documents L1-L10 + Sub-A/B/C executed; Task 3 Correction 1 / 2 / 3 deviations from literal L1-L10 wording captured as as-shipped behavior (L6 reinterpreted from synchronous-render-fade to ADSR quick-release via physical over-provisioning kMaxVoices 16->24 + kLogicalCap=16; L7(b) upgraded from 2-tier to 3-tier with isKeyDown protection; NEW look-ahead noteOff pre-scan emerged from MIDI-event-ordering diagnosis at loop boundaries); 6-NIT close-pass disposition table; the 4-of-4 audio-thread heap-alloc surface closed declaration; effort actual ~12-15 hours vs ~8-12 hour estimate.

3. **`Plans & Specs/Main Plan.md` NEW §5 QA-SfzGroup batch entry** — inserted between QA-VoicePool and QA-EngineApvts.  Scope per FND-2 routing: (1) fix `<group>` opcode-inheritance state machine in `VibeSampleManager::parseSFZ`; (2) investigate Aria/sfizz RR loss across BaySickRustyDrums + BaySickGuitars + BaySickBasses.  Slot: very next batch after QA-VoicePool close per Jeff's verbatim "very next batch".

4. **`Plans & Specs/Main Plan.md` §6 sequencing arrow update** — `QA-VoicePool*********************` followed by NEW `QA-SfzGroup***********************` (23-asterisk footnote) followed by `QA-EngineApvts**********************`.  New 23-asterisk QA-SfzGroup footnote inserted between QA-VoicePool's and QA-EngineApvts's footnotes in the post-arrow footnote list.  QA-EngineApvts §5 entry's Sequencing field + QA-EngineApvts footnote text updated from "after QA-VoicePool" to "after QA-SfzGroup" + cross-ref to §9 thirty-eighth Forks entry.

5. **`Plans & Specs/Main Plan.md` §9 thirty-seventh + thirty-eighth Forks entries** — thirty-seventh records the BaySickSynth `mOsc.reset()` finding routing to QA-EngineApvts (FND-1 in the Work Log batch-close entry); thirty-eighth records the SFZ `<group>` parser inheritance bug routing to NEW QA-SfzGroup (FND-2).  Both back-ref this running-notes file at Task 2 Section 4 (FND-1) + Task 4 Section 3 (FND-2).  QA-EngineApvts §5 Scope bullet list expanded with the 2-line `mOsc.reset(); mOsc2.reset();` fix per the thirty-seventh Forks entry's fold-in directive.

6. **`Plans & Specs/Running Notes/tipsy-pulsing-octopus.md`** — this close-pass section appended at end of file; pairs with the Implemented Work Log entry as the canonical post-batch record per `feedback_draft_doc_running_notes_every_checkpoint.md`.

**Next batch:** QA-SfzGroup (close-spawned per FND-2 routing).  After QA-SfzGroup closes, sequencing returns to QA-EngineApvts (the fourth batch in the QA-Eg close-spawned perf-audit cluster; also absorbs FND-1's BaySickSynth `mOsc.reset()` 2-line fix per the thirty-seventh Forks entry fold-in).

**Rule 4 Diagnostic Instrumentation Catalog:** nil for the close pass (no source diffs landed in the close commit — docs-only across Main Plan + Implemented Work Log + this Running Notes file).  Running tally across Tasks 0-7 + this close: all nil.  Catalog table at top of file remains empty as locked; no strip pass needed; Rule 4 close discipline satisfied.

**Batch closed.**
