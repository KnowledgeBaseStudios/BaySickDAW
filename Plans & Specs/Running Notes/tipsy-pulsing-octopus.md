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

**External-reader sweep** (the "fully internally contained" check): grep across `Source/` for `mResampSrc` / `mMemSrc` / `mSampleBuffer` returns matches ONLY in `Source/VibePlayer/VibePlayerDSP.cpp` + `Source/VibePlayer/VibePlayerDSP.h`. Zero hits in PluginProcessor / VibeGraph / Standalone pages / other engines / Engine/Tasks/. Fat-voice refactor is fully internally contained — no cross-file API breakage, no caller-side churn. Cross-engine `VibeSynth` references at [HarmlessSynth.cpp:6](../../Source/Harmless/HarmlessSynth.cpp:6) + [BaySickSynthDSP.cpp:7](../../Source/BaySickSynth/BaySickSynthDSP.cpp:7) are COMMENTS ONLY (design-pattern back-references documenting the placeholder-sample-rate-before-addVoice pattern, not actual code dependencies).

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
