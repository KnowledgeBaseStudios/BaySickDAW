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
