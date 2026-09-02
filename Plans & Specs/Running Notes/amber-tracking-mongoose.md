# Running Notes — QA-Sfizz (amber-tracking-mongoose)

> Append-only execution log for QA-Sfizz. Each entry captures the state at a checkpoint trigger (post-commit / post-sub-task verify / post-finding / post-spec-call / post-scope-pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Append-only — never edit prior entries; new findings surface as new entries.

**Pair file:** `Plans & Specs/Batch Plans/amber-tracking-mongoose.md`
**Convention reference:** Main Plan §0 "Document Formatting Conventions" + "Plan file + Running Notes required sections" (locked 2026-05-11) + Rule 4 Diagnostic Instrumentation Catalog (locked 2026-05-12) + Rule 5 Plan-mode sub-spec-call surface discipline (locked 2026-05-26).

---

## Diagnostic Instrumentation Catalog

Per §0 Rule 4: every diagnostic addition (`DBG` / `juce::Logger::writeToLog` / temp `jassert` added for diagnosis / debug `juce::AlertWindow` popups / temp file logging / `std::cout`-style traces) gets logged here in the same edit pass as the source change. At task/batch close, walk this table and strip every `Remove` entry from source — surface the strip list to Jeff BEFORE running the strip pass.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| [libs/sfizz/src/sfizz/Synth.cpp top + finalizeSfzLoad end](../../libs/sfizz/src/sfizz/Synth.cpp) | `[QA-Sfizz Task3]` | Post-load Region dump per Sub-D=(c) part (a) — verifies whether loaded Region state correctly reflects SFZ-declared `seq_length`/`seq_position`/`sw_last`/`sw_label` opcodes | **Removed at Task 3 close** (commit `f477e39` via `git restore --source=HEAD`) |
| [libs/sfizz/src/sfizz/Layer.cpp top + registerNoteOn](../../libs/sfizz/src/sfizz/Layer.cpp) | `[QA-Sfizz Task3]` | Note-on dispatch trace per Sub-D=(c) part (b) — captures per-call `counter`, `sequenceSwitched_`, `isSwitchedOn()` to identify which gate blocks RR cycling | **Removed at Task 3 close** (commit `f477e39` via `git restore --source=HEAD`) |
| [Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp top + processStrips entry + processBlock entry + 2 noteOn dispatch sites](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp) | `[QA-Sfizz Task4]` | Engine boundary trace per Sub-F=(e) for Item 3 MT bit-crusher instance-access-violation hypothesis test — captures thread_id + mSfizz wrapper ptr + buffer ptr at audio-thread entry + thread_id + mSfizz ptr + note + velocity at noteOn dispatch sites | `Remove at Task 5 close` |
| [libs/sfizz/src/sfizz/Synth.cpp top + Synth::renderBlock entry](../../libs/sfizz/src/sfizz/Synth.cpp) | `[QA-Sfizz Task4]` | sfizz vendored boundary trace per Sub-F=(e) for Item 3 MT bit-crusher — single log entry at Synth::renderBlock entry capturing thread_id + Synth instance ptr + numFrames; multiple thread IDs hitting same `this` = smoking gun for instance-access-violation hypothesis | `Remove at Task 5 close` |

---

## 2026-05-27 — Task 0 — Open

Open commit (docs only). Plan-mode session converted §9 thirty-ninth Forks entry's Jeff-locked verbatim 3-item scope (Item 1: keyswitch label discoverability for the 3 sfizz-driven engines / Item 2: Guitars/Basses RR-loss diagnosis / Item 3: BaySickRustyDrums MT-mode bit-crusher diagnosis + fix) into the canonical batch plan file [amber-tracking-mongoose.md](../Batch Plans/amber-tracking-mongoose.md). Seven spec calls (SC-1 through SC-7) locked at ExitPlanMode after SC-1/SC-2/SC-3 were surfaced via chat per Main Plan §0 Rule 5 + SC-4 was surfaced mid-Phase-1-research after `Synth::getKeyswitchLabels()` was confirmed to NOT exist in sfizz's public API (single-symbol additions required, fits Sub-R/S bounded-patch precedent):

- **SC-1 = (c)** 7-task structure (Task 0 open / Task 1 inventory / Task 2 Item 1 / Task 3 Item 2 verify-then-diag / Task 4 Item 3 diagnosis / Task 5 Item 3 fix / Task 6 close). Jeff verbatim: "We are separating the Item 3 MT race diagnosis from the fix. Since this is an asynchronous bug in a vendored library, I want to see the empirical logging and root-cause isolation in Task 4 before any source fixes are attempted in Task 5."
- **SC-2 = (c)** Item 1 split into 2 sub-commits by loader path (Task 2A: BaySickRustyDrums wrapper-load path + sfizz public accessor patch; Task 2B: BaySickGuitars + BaySickBasses file-load path). Jeff verbatim: "This gives us a clean rollback boundary if the string wrapper path behaves differently regarding sfizz state access."
- **SC-3 = (b)** Bounded targeted vendored-sfizz patches authorized in-batch (single-symbol type change / single-function additions); broader state-machine architectural changes route to follow-up batch. Mirrors QA-SfzGroup Sub-R amendment precedent.
- **SC-4 = (a)** SC-3 authorization extends to Item 1's sfizz public-accessor patch (single new method `Synth::getKeyswitchLabels()` + parallel forward on `sfz::Sfizz` wrapper; mirrors existing `getKeyLabels()` / `getCCLabels()` pattern exactly; no logic change; no threading change). Surfaced mid-Phase-1-research after sfizz public-API gap was confirmed; SC-3 was originally scoped to Item 3 only (Track 3 / MT race).
- **SC-5** = silly-name `amber-tracking-mongoose` (my pick per `feedback_silly_name_is_my_pick.md`; plan-mode runtime auto-assigned `cryptic-popping-hanrahan.md` — overridden).
- **SC-6** = Debug-then-Release per task verify cadence.
- **SC-7** = source-file references in commit bodies use brand-mixed-case engine names (BaySickRustyDrums / BaySickGuitars / BaySickBasses).

Three sub-spec calls deferred until Task 4 surfaces a root cause (NOT picks I want to make pre-investigation; genuinely deferred per Rule 5 + the `federated-bouncing-cupcake.md` exemplar's table shape):

- **Sub-A** (deferred to Task 4 close) — if MT race root cause NOT isolated, fallback policy (a) conservative serial-execution-mode fallback for cymbals/hi-hats / (b) route fix entirely to follow-up batch / (c) document as known-issue.
- **Sub-B** (deferred to Task 4 close) — if root cause identified, in-tree only vs in-tree + upstream sfizz PR.
- **Sub-C** (deferred to Task 4 close) — if fix shape exceeds bounded-patch envelope, in-batch re-amendment vs route-out.

**Phase 1 research findings landed in plan body:**

- sfizz public API for keyswitch labels: NO accessor exists. Internal data lives at [libs/sfizz/src/sfizz/SynthPrivate.h:288](../../libs/sfizz/src/sfizz/SynthPrivate.h:288) `std::vector<NoteNamePair> keyswitchLabels_;` + map at :289. Patch shape: add `Synth::getKeyswitchLabels()` after `getCCLabels()` at [Synth.h:713](../../libs/sfizz/src/sfizz/Synth.h:713) + implementation after [Synth.cpp:2400](../../libs/sfizz/src/sfizz/Synth.cpp:2400) + wrapper forward on `sfz::Sfizz` after [sfizz.hpp:1035](../../libs/sfizz/src/sfizz.hpp:1035) + implementation after [sfizz.cpp:397](../../libs/sfizz/src/sfizz/sfizz.cpp:397).
- Sfizz instance members in 3 engine processors: all `std::unique_ptr<sfz::Sfizz> mSfizz;` — [BaySickRustyDrumsProcessor.h:185](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h:185) + [BaySickGuitarsProcessor.h:146](../../Source/BaySickGuitars/BaySickGuitarsProcessor.h:146) + [BaySickBassesProcessor.h:140](../../Source/BaySickBasses/BaySickBassesProcessor.h:140). Each is private; need to add public `getKeyswitchLabel(int)` method per engine.
- Piano-roll registration sites: BaySickRustyDrums has dedicated `registerBaySickRustyDrumsPianoRoll()` at [StandaloneEditor.cpp:5832](../../Source/Standalone/StandaloneEditor.cpp:5832); BaySickGuitars + BaySickBasses share `registerInstSourcePianoRoll(InstPage*)` at [:7934](../../Source/Standalone/StandaloneEditor.cpp:7934) with EngineKind branches at 7972-7980 (Guitars) and 7982-7994 (Basses). The 2-file vs 1-file split aligns naturally with SC-2's loader-path commit split (Rusty wrapper-load → Task 2A; Guitars/Basses file-load both in `registerInstSourcePianoRoll` → Task 2B).
- PianoRoll plumbing from QA-SfzGroup Task 2 verified in place: `PianoRollConnection::keyswitchLabelProvider` field at [PianoRollPage.h:73](../../Source/Standalone/PianoRollPage.h:73), `PianoKeyboard` amber-paint branch at [PianoRoll.cpp:180-207](../../Source/Standalone/PianoRoll.cpp:180), tooltip priority at [PianoRoll.cpp:107-126](../../Source/Standalone/PianoRoll.cpp:107), `PianoRollPage::registerEngine` wiring at [PianoRollPage.cpp:112](../../Source/Standalone/PianoRollPage.cpp:112). All four sites unchanged since QA-SfzGroup commit `5cd5f17`.
- Existing BaySickPlayer closure pattern (4 sites — Layer/Bass/Drum/Clip at StandaloneEditor.cpp:5734-5738 / :5780-5784 / :5820-5824 / :7890-7894) is the mirror reference for Item 1's 3 new closures.

**Files touched in Task 0 commit (docs-only):**

- `Plans & Specs/Batch Plans/amber-tracking-mongoose.md` — NEW (mirrored from plan-mode home-dir + delete both home-dir copies per `feedback_plan_mirror_one_way.md`).
- `Plans & Specs/Running Notes/amber-tracking-mongoose.md` — NEW (this file).
- `Plans & Specs/Main Plan.md` — §5 QA-Sfizz `**Plan file:**` line at 1316 updated from `<silly-name>.md (when started)` placeholder to backticked canonical path.

**No source changes in Task 0.** All source work begins at Task 2A (sfizz public accessor patch + BaySickRustyDrums label provider closure).

**Outcome:** Task 0 commit landed at `828f6b9` (3 files changed, 510 insertions / 1 deletion - 2 docs new + 1 doc edited per files-touched list above). Drafter output reviewed + 2 minor factual fixes applied before commit (`keyswitchLabelMap_` -> `keyswitchLabelsMap_` per Phase 1 research at SynthPrivate.h:289; effort estimate `~5-8 hours` -> `~6-12 hours` to align with §5 entry). Clean working tree post-commit. Branch ahead of origin/main by 25 commits.

**Next:** Task 1 — read-only inventory pass confirming the Phase 1 research findings + surfacing any remaining sfizz keyswitch-state plumbing gaps before Task 2A's sfizz public accessor patch lands.

---

## 2026-05-27 — Task 1 — Inventory (read-only)

Read-only confirmation pass per the plan's Task 1 checklist. All Phase 1 research findings re-verified by direct Read against current HEAD (post-`828f6b9` baseline); no new sub-spec call surfaced (Rule 5 gate clear for Task 2 progression).

**SC-4 sfizz public accessor patch shape verified line-by-line:**

- `Synth::getKeyLabels()` impl at [libs/sfizz/src/sfizz/Synth.cpp:2394-2398](../../libs/sfizz/src/sfizz/Synth.cpp:2394) uses `Impl& impl = *impl_; return impl.keyLabels_;` pattern (NOT a single-line return; the local-ref unwrap is explicit).
- `Synth::getCCLabels()` impl at [Synth.cpp:2400-2404](../../libs/sfizz/src/sfizz/Synth.cpp:2400) same pattern.
- Patch insertion point: after :2404 (before `Synth::getResources()` at :2406). Patch body mirrors the `Impl& impl = *impl_;` lead so the new accessor matches the existing pair exactly — no logic divergence, no shortcut.
- `sfz::Sfizz::getKeyLabels()` wrapper forward at [libs/sfizz/src/sfizz/sfizz.cpp:392-395](../../libs/sfizz/src/sfizz/sfizz.cpp:392) — single-line `return synth->synth.getKeyLabels();`.
- `sfz::Sfizz::getCCLabels()` at [sfizz.cpp:397-400](../../libs/sfizz/src/sfizz/sfizz.cpp:397) same shape.
- Patch insertion point: after :400 (before `ClientDeleter::operator()` at :402).
- [SynthPrivate.h:283-289](../../libs/sfizz/src/sfizz/SynthPrivate.h:283) storage confirmed: `keyLabels_` at :283 + `keyLabelsMap_` at :284 (existing pair); `keyswitchLabels_` at :288 + `keyswitchLabelsMap_` at :289 (the new pair that SC-4's public accessor exposes).

**3 sfizz engine processor mSfizz members verified:**

- [BaySickRustyDrumsProcessor.h:185](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h:185) — `std::unique_ptr<sfz::Sfizz> mSfizz;` (private; J-7b multi-output wrapper SFZ - "single sfizz instance with `output=N`-routed wrapper SFZ" per comment at :179-184).
- [BaySickGuitarsProcessor.h:146](../../Source/BaySickGuitars/BaySickGuitarsProcessor.h:146) — same private field shape.
- [BaySickBassesProcessor.h:140](../../Source/BaySickBasses/BaySickBassesProcessor.h:140) — same private field shape.

Each will gain a public `juce::String getKeyswitchLabel(int midiNote) const noexcept;` declaration (placement: alongside the existing public `setOnAnyStateChange()` / `getUndoManager()` accessor cluster, BEFORE the `private:` keyword that begins the `mSfizz` etc. cluster) + a .cpp implementation that iterates `mSfizz->getKeyswitchLabels()` and returns the matching label by midiNote.

**Item 3 instrumentation file landscape (Task 4 scope clarification):**

The plan-body's "polyphony manager" abstract reference resolves to THREE separate sfizz files (not one) plus the orchestrator + per-voice paths. The §5 entry's Item 3 scope ("dig into sfizz's MT execution model: worker pool / lock-free queues / shared state") already covers any of these files — the file split isn't a new spec call, just a finding that expands Task 4's instrumentation candidate list:

- [libs/sfizz/src/sfizz/PolyphonyGroup.h](../../libs/sfizz/src/sfizz/PolyphonyGroup.h) + [.cpp](../../libs/sfizz/src/sfizz/PolyphonyGroup.cpp) — small class; `registerVoice` / `removeVoice` / `removeAllVoices` / `numPlayingVoices` / `setPolyphonyLimit`. Per-group voice tracking.
- [libs/sfizz/src/sfizz/VoiceManager.h](../../libs/sfizz/src/sfizz/VoiceManager.h) + [.cpp](../../libs/sfizz/src/sfizz/VoiceManager.cpp) — separate file; voice scheduler / lifecycle. Holds `voiceManager_` in `Synth::Impl` (referenced from [Synth.cpp:221](../../libs/sfizz/src/sfizz/Synth.cpp:221) + :389 `voiceManager_.ensureNumPolyphonyGroups(...)`).
- [libs/sfizz/src/sfizz/VoiceStealing.h](../../libs/sfizz/src/sfizz/VoiceStealing.h) + [.cpp](../../libs/sfizz/src/sfizz/VoiceStealing.cpp) — separate file; stealing algorithm.
- `Synth::Impl::startVoice` orchestrator at [Synth.cpp:1300](../../libs/sfizz/src/sfizz/Synth.cpp:1300) (declared at [SynthPrivate.h:166](../../libs/sfizz/src/sfizz/SynthPrivate.h:166)) — calls `selectedVoice->startVoice(layer, delay, triggerEvent)` at :1310; recursive sister-voice ring builder at :1344 + :1380.
- `Voice::startVoice` at [Voice.cpp:408](../../libs/sfizz/src/sfizz/Voice.cpp:408) (declared at [Voice.h:117](../../libs/sfizz/src/sfizz/Voice.h:117)) — per-voice startup.
- [Region.cpp](../../libs/sfizz/src/sfizz/Region.cpp) + [RegionSet.cpp](../../libs/sfizz/src/sfizz/RegionSet.cpp) + [RegionStateful.cpp](../../libs/sfizz/src/sfizz/RegionStateful.cpp) — region-state lifecycle (Stateful = per-load mutable layer; the non-stateful Region is parse-once shared state).
- [Layer.cpp](../../libs/sfizz/src/sfizz/Layer.cpp) — already touched by QA-SfzGroup Sub-R/S `sequenceCounter_` atomic patch; Task 4 may revisit for additional state capture.

Task 4 will land Rule-4-cataloged `juce::FileLogger` calls at whichever candidate sites the trace-capture cycle implicates; the expanded file landscape doesn't change the SC-3=(b) bounded-patch envelope authorization.

**3 sfizz-engine piano-roll registration sites + Task 2 closure insertion points:**

Confirmed at Phase 1; re-verified at Task 0 (Reads at StandaloneEditor.cpp:5825-5896 + :7930-8004 during plan-mode + the Task 0 plan-pointer Edit pass). No source drift since QA-SfzGroup close at `42f7253`.

- [StandaloneEditor.cpp:5832-5895](../../Source/Standalone/StandaloneEditor.cpp:5832) `registerBaySickRustyDrumsPianoRoll()` — BaySickRustyDrums singleton. Task 2A closure inserts after `conn.allKeysWhite = true;` at :5893 and before `mPianoRollPage->registerEngine(...)` at :5895.
- [StandaloneEditor.cpp:7972-7980](../../Source/Standalone/StandaloneEditor.cpp:7972) `EngineKind::BaySickGuitars` branch inside `registerInstSourcePianoRoll(InstPage*)` at :7934. Task 2B Guitars closure inserts after `conn.auditionOff = [](int) {};` at :7980 and before the branch's closing `}`.
- [StandaloneEditor.cpp:7982-7994](../../Source/Standalone/StandaloneEditor.cpp:7982) `EngineKind::BaySickBasses` branch inside same function. Task 2B Basses closure inserts after `conn.defaultTopNote = 48;` at :7993 and before the branch's closing `}`.

**Plan-body fix landed (Task 0 close flag resolved):** the Task 1 section's "**No commit on Task 1**" inverted line replaced with "**Commit Task 1 on completion**" + corrected precedent framing (inventory commits exist at QA-SfzGroup `196d72e` + QA-VoicePool `bddcaa6`; verify-only no-commit pattern is QA-InsertMaps Task 5 / QA-AudioMeters Task 3, NOT inventory tasks). Single targeted Edit per `feedback_targeted_edits_not_wholesale_rewrite.md` folded into this Task 1 commit.

**No new sub-spec calls surfaced.** Rule 5 gate clear for Task 2A progression.

**Files touched in Task 1 commit (docs-only):**

- `Plans & Specs/Running Notes/amber-tracking-mongoose.md` — this Task 1 entry appended + the Task 0 "Outcome" line from the post-Task-0 in-progress edit (already in working tree pre-commit; both fold into the same Task 1 commit).
- `Plans & Specs/Batch Plans/amber-tracking-mongoose.md` — Task 1 plan-body line "No commit on Task 1" → "Commit Task 1 on completion" single-line fix.

**No source changes in Task 1.** Source work begins at Task 2A (sfizz public accessor patch + BaySickRustyDrums label provider closure).

Next: surface full git status → `/draft-commit` → surface drafted message + git status for Jeff approval → commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-Sfizz-Task1.txt`.

---

## 2026-05-27 — Task 2A — BaySickRustyDrums label provider closure + sfizz public accessor patch

Task 2A commit landed at `30289f6` per the SC-2=(c) loader-path split (wrapper-load-path engine first). 7 files / +61/-0 pure additive. Build clean both configs. Jeff verified: BaySickRustyDrums piano roll renders normally with no amber (his kit has no `sw_label` opcodes - karoryfer big-rusty-drums uses MIDI-note mapping for articulations, not keyswitches). BaySickPlayer regression check pass. No new sub-spec calls per Rule 5.

Running-notes entry intentionally minimal here - the Task 2A commit body at `30289f6` captures full implementation details + verify outcome + closure-pattern rationale. This entry exists as the checkpoint marker per `feedback_draft_doc_running_notes_every_checkpoint.md` (was missed at the actual commit time + caught up here pre-Task 3 commit per the post-Task-1 "rolls forward into next commit" pattern).

---

## 2026-05-27 — Task 2B — BaySickGuitars + BaySickBasses label provider closures (Item 1 close)

Task 2B commit landed at `6368fac` per the SC-2=(c) loader-path split (file-load-path engines second). 5 files / +54/-0 pure additive. Build clean both configs. Jeff verified: visible amber-label payoff confirmed - both Guitars + Basses piano rolls render keyswitch keys with amber backgrounds + bold dark-amber `sw_label` text + tooltip on hover (the value-prop win that drove the QA-Sfizz batch routing). LiveInput Inst tabs untouched per early return at registerInstSourcePianoRoll :7949 (LiveInput has no piano roll - my Task 2B verify script briefly included a nonsensical LiveInput check which Jeff caught + I corrected).

**Item 1 status: COMPLETE.** All 3 sfizz-driven engines now expose keyswitch label discoverability through the same `getKeyswitchLabel(int)` interface. QA-SfzGroup Sub-P=(a) BaySickPlayer-only scope extension is resolved.

Running-notes entry intentionally minimal here for the same reason as Task 2A above.

---

## 2026-05-28 — Task 3 — Item 2 verify + diagnosis (in flight)

Item 2 (BaySickGuitars/BaySickBasses round-robin loss) is in deep-diagnosis territory. Jeff verified post-Item-1 on 2026-05-27 that amber labels are visible on both engines but RR does NOT cycle audibly when playing same MIDI note at fixed velocity with a keyswitched articulation engaged - **Branch B** per the plan body. Static code review pass on 2026-05-28 completed; runtime instrumentation per Sub-D=(c) landed (not yet committed; awaiting Jeff's trace capture).

**Sub-D = (c) lock (Jeff 2026-05-28):** both post-load Region dump (a) + note-on dispatch trace (b) — full visibility into kit-load state + per-keypress dispatch decisions. Implementation lives entirely inside sfizz with file-logging via `std::fopen`/`std::fprintf` to `Documents/BaySickDAW/qa-sfizz-task3-trace.log` (per `feedback_follow_existing_folder_convention.md`). Mutex-protected lazy-init helpers in anonymous namespaces at top of Synth.cpp + Layer.cpp.

**Static analysis findings (no bug surfaced; all paths look correct on paper):**

- sfizz `Default::sequence` = 1 at [libs/sfizz/src/sfizz/Defaults.cpp:83](../../libs/sfizz/src/sfizz/Defaults.cpp:83) — correct default per SFZ v1 spec.
- Karoryfer Black&Green Guitars + Black&Blue Basses content DOES declare `seq_length=4` + `seq_position=1..4` across map files (`Programs/modules/maps_green/ord.sfz` shows 4 regions per velocity layer per key for Twang articulation; same shape for hammer/stac). Content is correct.
- `loadSfzFile` ([Synth.cpp:636](../../libs/sfizz/src/sfizz/Synth.cpp:636)) + `loadSfzString` ([:665](../../libs/sfizz/src/sfizz/Synth.cpp:665)) share the same parser via `impl.parser_`.
- sfizz's parser correctly preserves `<master>` scope across `#include` boundaries (parser state `_currentHeader`/`_currentOpcodes` is file-stack-independent; `flushCurrentHeader` only emits on new `<` header).
- Listener `Synth::Impl::onParseFullBlock` ([Synth.cpp:91](../../libs/sfizz/src/sfizz/Synth.cpp:91)) correctly tracks `<global>`/`<master>`/`<group>` scopes via `globalOpcodes_`/`masterOpcodes_`/`groupOpcodes_` member-vector state, applies via `parseOpcodes` at `buildRegion` time.
- RR cycling math at [Layer.cpp:51-66](../../libs/sfizz/src/sfizz/Layer.cpp:51) is per-region (each Layer has its own `sequenceCounter_`) and works correctly on paper.

**Structural Rusty-vs-Guitars asymmetry observed (NOT a confirmed bug — runtime trace will confirm or refute):**

- Rusty content: `seq_length` declared at `<master>` scope + regions only specify `seq_position`. `<group>` wraps regions inside per-piece map files. Pattern: A.
- Guitars/Basses content: `seq_length` + `seq_position` declared directly on each `<region>`. No `<group>` wrapper. Regions are direct children of `<master>` (via `#include` resolution at parse time). Pattern: B.
- Both patterns valid per SFZ v1 spec.

**Sub-D=(c) instrumentation landed (NOT YET COMMITTED — pending trace capture + analysis):**

- `libs/sfizz/src/sfizz/Synth.cpp` — anonymous-namespace helper at top of `namespace sfz {` (mutex-protected lazy-init `FILE*` to `Documents/BaySickDAW/qa-sfizz-task3-trace.log`, USERPROFILE-resolved on Windows / HOME-resolved on POSIX) + post-load Region dump at end of `finalizeSfzLoad` (walks `layers_`, dumps per-Region key range + seq_length + seq_position + sw_last + sw_label).
- `libs/sfizz/src/sfizz/Layer.cpp` — mirror helper in own anonymous namespace (each TU has its own `FILE*`, `fopen "a"` mode is OS-thread-safe at the filesystem level so interleaving across files is acceptable) + per-call trace inside `registerNoteOn` `if (keyOk)` block (logs note + velocity + keyRange + counter + seq state + sequenceSwitched_ result + isSwitchedOn() result).

**Catalog rows already added to the Diagnostic Instrumentation Catalog section above** per Rule 4 (same logical pass as the code edits).

**Next action:** Jeff builds → reproduces RR-loss scenario → captures `Documents/BaySickDAW/qa-sfizz-task3-trace.log` → shares back. Analyze trace. Based on what the trace shows: (a) if `seq_length=1` always in LOAD dump → parser bug, scope to bounded sfizz patch per SC-3=(b)/SC-4; (b) if seq state correct in LOAD dump but `sequenceSwitched=0` always in NOTEON trace for non-position-1 regions → counter or modulo-check logic bug; (c) if all switched_ flags correct but no audible variation → audio-path or sample-loading issue downstream of region dispatch; (d) if the kit's seq_length/seq_position fields are silently 0 or otherwise corrupted in LOAD dump → content / inheritance edge case. Each outcome surfaces a new sub-spec call per Rule 5 before the corresponding fix.

---

## 2026-05-28 — Task 3 — Trace #1 analysis + Sub-D refinement (in flight)

First trace captured by Jeff (8029 lines).  **sfizz RR cycling verified working correctly** — cross-tab of `isSwitchedOn=1` lines by `(counter, seq_pos)` showed exactly 4 layers fire per cycle position with seq_pos cycling cleanly 1→2→3→4→1→... across 19 key presses.  No parser bug, no counter bug, no modulo logic bug.  Karoryfer Black&Green Guitars + Black&Blue Basses kit content DOES declare `seq_length=4` + `seq_position=1..4` correctly across `Programs/modules/maps_green/ord.sfz` etc.  Sample files (twang_f3_f_rr1..4.wav) verified distinct on disk via `md5sum` (4 unique hashes + 4 distinct file sizes 702K/630K/652K/822K).

But 4 isSwitchedOn=1 per cycle position is unexpected (expected 3 — one per velocity layer in main Twang `<master>`).  Trace doesn't capture (a) which sample each Layer plays nor (b) whether the firing decision actually returns true (velOk + randOk gates aren't logged).  **Sub-D refinement landed:** added sample path + velocity range to LOAD dump + new `NOTEON-FIRE` trace at end of `Layer::registerNoteOn` with velOk + randOk + final `fired` result + sample path.  Refinement is incremental to existing Sub-D=(c) scope — not a new spec call.

---

## 2026-05-28 — Task 3 — Trace #2 analysis + Item 2 verify (closed Branch A)

Second trace (post-refinement) confirmed RR is fully working: 15 fires across 4 unique samples in pattern rr1→rr2→rr3→rr4 repeating cleanly through ~3.75 full cycles.  4 NOTEON-FIRE candidates per press = 3 velocity layers (p/mf/f from main Twang `<master>`) + 1 release-trigger (triggerOnNote=false → fired=0).  Only the f-layer passes velOk for Jeff's vel=0.787 → 1 sample fires per press → cycling produces 4 distinct samples across 4 presses.

Jeff's perceptual test on raw `.wav` files outside BaySickDAW (Windows Media Player / Audacity): **the 4 sample files sound identical or near-identical** despite being distinct files with different MD5 hashes + file sizes.  Conclusion: **Item 2 is a kit content limitation, NOT a code bug.**  Karoryfer's RR variants for f-velocity F3 are too perceptually similar to produce audible variation under default kit settings.

**Item 2 closes as Branch A (verify-only, no source change).** sfizz / parser / dispatch / cycling all work correctly.

---

## 2026-05-28 — Task 3 — Sub-E scope expansion (mid-batch amendment)

While investigating the Aria-player-settings angle, surfaced a **separate side-finding** that Jeff confirmed correlates with a long-standing perception: "the drum kit has always sounded quiet or 'thin' to me and I wasn't sure why."

**Root cause identified:** the K-5 fix (2026-05-05, [BaySickGuitars](../../Source/BaySickGuitars/BaySickGuitarsProcessor.cpp) + L-5 in [BaySickBasses](../../Source/BaySickBasses/BaySickBassesProcessor.cpp) + K-5 equivalent in [BaySickRustyDrums](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp)) defaults all APVTS CC params to 0 to match sfizz's "unset CC = 0" SFZ-spec internal convention.  But Karoryfer kits — the primary content for all 3 sfizz-driven engines — are designed for Aria hosts which default CCs to 64 (MIDI center).  Karoryfer's `<master>`/`<group>` blocks gate on CC ranges expecting CC=64 default:

- **Big Rusty Drums:** 88 masters gate on CC4 (hi-hat position) + 72 on CC25 + 64 on CC28 + many more (CC15, 16, 26, 27, 80, 81, 82) — none declared via `set_cc`, all silently 0 under K-5.
- **Black&Green Guitars:** unison masters gate on `locc100=1`; tailpiece masters on `locc118=64`; feedback masters on `locc29=1` — none in kit's `set_cc` list, all silently inactive under K-5.
- **Black&Blue Basses:** similar pattern.

Under K-5's CC=0 default, those gated `<master>` blocks silently didn't fire → fewer voices stacked per press → "thin sound" perception across all 3 sfizz-driven engines.

**Sub-E = (a) + (i) lock (Jeff 2026-05-28):** "Where to fix" = (a) fold into QA-Sfizz Task 3 in-batch (scope expansion).  "What to fix" = (i) default ALL APVTS CCs to 64 for the 3 sfizz-driven engines (Aria emulation).  Jeff verbatim: "We are here to get these instruments sounding correct, and we are going to fix the CC defaults right now... I am not deferring this."  Jeff verbatim on shape: "The K-5 KVR fix makes sense for generic SFZ, but Rusty, Guitars, and Basses are Aria hosts. They need to emulate the Aria environment (CC=64) so the unison layers and hi-hats fire properly by default.  Update the loadKit initialization logic to set the baseline to 64 instead of 0 before applying the kit's specific set_cc overrides."

**Sub-E source touch landed (9 Edits across 3 engines):**

- [BaySickRustyDrumsProcessor.{h,cpp}](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp): `getKitDefaultCc` fallback `return 0` → `return 64` ([cpp:109](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:109)); `createLayout` AudioParameterInt default `0` → `64` ([cpp:141](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:141)); `loadKit` reset baseline `0.0f` → `64.0f` ([cpp:~595](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:595)).
- [BaySickGuitarsProcessor.{h,cpp}](../../Source/BaySickGuitars/BaySickGuitarsProcessor.cpp): parallel changes (fallback at :99, createLayout at :130, loadKit reset at :353).
- [BaySickBassesProcessor.{h,cpp}](../../Source/BaySickBasses/BaySickBassesProcessor.cpp): parallel changes (fallback at :96, createLayout at :127, loadKit reset at :344).

All 9 sites have updated comments explaining the Aria-host-convention rationale + cross-referencing the K-5/L-5 history.  Kit-author `set_cc<N>=<int>` directives still override during loadKit (so if a kit explicitly sets CC100=0, that wins over the 64 baseline).  Slider visual baseline now matches sfizz's Aria-emulating CC=64 internal state — no slider/audio mismatch since both sit at midpoint by default.

**Diagnostic instrumentation (Sub-D=(c)) intentionally left in tree for Sub-E verify** so Jeff can capture a third trace post-Sub-E to confirm the firing pattern changes as expected (e.g., on BaySickGuitars Twang test, NOTEON-FIRE candidates per press should now grow from 4 to 8+ as unison + tailpiece masters become active).  Diagnostic strip happens at the Task 3 close commit per Rule 4 (strip list will be surfaced to Jeff for approval before stripping).

**Next action:** Jeff builds → tests perceptual sound quality on BaySickRustyDrums + BaySickGuitars + BaySickBasses with default kits (no manual CC tweaking).  Listening for: drums sound fuller (hi-hat positions / mute groups fire); guitars/basses have unison + tailpiece + feedback layers engaged by default; overall "thin sound" perception resolved.  Trace also captured for confirmation that more `<master>` blocks fire post-Sub-E.

---

## 2026-05-28 — Task 3 — Sub-E mechanism correction + close-out

Jeff built Sub-E, tested, measured via DBFS: **no audible level change.** Mechanism reasoning was wrong on my part.

**Misdiagnosed mechanism (what I claimed):** CC=64 default would *add* `<master>` blocks firing → more layers stacked per press → fuller sound.

**Actual mechanism:** Karoryfer kits author `<master>` blocks in OPPOSITE-direction CC ranges - e.g., main Twang on `hicc118=63` (CC ≤ 63) and tailpiece Twang on `locc118=64` (CC ≥ 64) - mutually exclusive ranges that select WHICH variant fires, not whether AN ADDITIONAL variant fires.  Flipping CC=0 → CC=64 SWAPS which variant the user hears by default (main → tailpiece on this example), it doesn't ADD a tailpiece-on-top-of-main layer.  Same number of `<master>` blocks firing per press, same level → DBFS unchanged.  Jeff verbatim: "I wouldn't say it sounds louder, and dbfs proves that, but any more action on this is chasing a dragon."

**But Sub-E's CORE purpose (Aria-host convention emulation) stands separate from the loudness hypothesis.** The kit author DESIGNED their `<master>` selection around Aria's CC=64 default - which articulation variant Aria users hear by default IS the author's intended default.  Under K-5's CC=0, users hear a DIFFERENT default articulation variant than Aria-authored kits intend.  Sub-E corrects that authorship-vs-runtime mismatch, regardless of DBFS.  Jeff verbatim on keep-vs-revert: "You found an issue with how you set something up, it didn't fix an issue you knew it wouldn't, and then you said lets remove the fix for the other issue because it didn't fix this?" - calling out the bad conflation in my proposal to revert.

**Sub-E stays.** Aria emulation is the right behavior for these 3 sfizz-driven engines.  The "fuller sound" follow-up hypothesis is documented here as a learning artifact so future sessions don't re-attempt the loudness angle.  "Thin sound" perception remains a real issue but is NOT fixable via blanket CC default change - it's a per-kit / per-CC user-control surface, not a code default.  Future investigation (if Jeff wants to pursue) would be per-kit articulation-engagement defaults, which is QA-Sfizz-out-of-scope.

**Item 2 final disposition: closes Branch A** (verify-only, no source change for Item 2).  sfizz parser + dispatch + RR cycling all verified working; Karoryfer RR variants are perceptually too similar to produce audible variation under default kit settings.  NOT a code bug.

**Sub-D=(c) diagnostic strip at Task 3 close (per Rule 4 catalog Disposition):**

Stripped via `git restore --source=HEAD -- libs/sfizz/src/sfizz/Synth.cpp libs/sfizz/src/sfizz/Layer.cpp` (reverts ONLY those 2 files to HEAD state = post-Task-2A `30289f6` + post-QA-SfzGroup `42f7253` baseline; Sub-E changes in the 3 engine processors are unaffected).  Both Rule 4 catalog rows (Sub-D=(c) Synth.cpp post-load Region dump + Sub-D=(c) Layer.cpp note-on dispatch trace) now have actual disposition `Removed at Task 3 close`.

**Stale artifact left in repo root:** `qa-sfizz-task3-trace.log` (~8K lines from Trace #2).  Untracked.  Jeff to delete manually when desired - the .log filename is a one-off and adding .gitignore for it is YAGNI (won't recur).

**Task 3 close commit shape (next):**

- Sub-E source landing (3 engine processors): BaySickRustyDrumsProcessor.cpp + BaySickGuitarsProcessor.cpp + BaySickBassesProcessor.cpp (+44/+44/+42 ≈ +130 / -27 net)
- Sub-D=(c) diagnostic strip (2 sfizz files): Synth.cpp + Layer.cpp back to HEAD state
- Plan file: add Sub-D + Sub-E to mid-batch Spec-calls-locked table; Task 3 section updated to reflect Branch A closure
- Running Notes: this final Task 3 entry + previous Sub-D/Sub-E sections preserved as journey record

**Item 2 closes Branch A.** Items 1 + 2 of QA-Sfizz complete.  Next batch step: Task 4 (Item 3 BaySickRustyDrums MT-mode bit-crusher diagnosis per SC-1=(c)'s diagnosis/fix separation lock).

---

## 2026-05-28 — Task 4 — Item 3 bit-crusher diagnosis (in flight)

Task 4 opens for the BaySickRustyDrums MT-mode bit-crusher diagnosis.  Jeff confirmed at Task 4 Step 1 baseline reproduction (post-Sub-E binary `f477e39`): **bit-crusher symptom UNCHANGED from QA-SfzGroup Task 3 baseline** — same long-sustaining cymbals/hi-hats distortion under MT-on, clean under MT-off, kick/snare/toms clean both ways.  Sub-E's CC=64 Aria-emulation flip did NOT affect Item 3's race.  Outcome (a) confirmed: proceed with Task 4 instrumentation plan.

**Sub-F = (e) lock (Jeff 2026-05-28):** Engine Boundary & Instance Trace.  Jeff overrode the 4 mechanical options (a/b/c/d) I proposed (broad sweep / bounded breadth / streaming-targeted / iterative voice-allocation) with a custom architectural-hypothesis-driven option.  Jeff verbatim: "We need to pull back a layer before we dive into sfizz's internal voice architecture.  None of the mechanical options (A-D) address the primary architectural hypothesis we established earlier: an Instance Access Violation at the engine boundary.  The symptoms (long-tail cymbals crashing under MT) strongly suggest that VibeThreadPool might be parallelizing at the voice/note level and throwing multiple threads at the exact same sfz::Sfizz instance concurrently.  sfizz is not thread-safe for concurrent renders on a single instance."

**Sub-F=(e) instrumentation plan (Jeff-locked):**

- **Entry Point:** file-logger injection at the top of `BaySickRustyDrumsProcessor::processBlock` + inside the noteOn dispatch wrapper.
- **Payload:** `std::this_thread::get_id()` + memory address of `sfz::Sfizz` instance + memory address of output buffer array.
- **sfizz Boundary:** single log entry inside `Synth::renderBlock` (entry point only) capturing thread + instance address — verifies what crosses the vendored boundary.
- **Hypothesis test:** multiple thread IDs hitting the same Rusty instance OR summing into the same floating-point buffer simultaneously during the 6-cymbal crash test = race confirmed at the engine boundary (NOT sfizz-internal).  Clean boundary (strictly one thread per block per instance) → expand to Option (a) broad voice-lifecycle sweep.

**Sub-F=(e) instrumentation landed in working tree (NOT YET COMMITTED — pending Jeff's trace capture):**

- [Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp) (+99 lines):
  - Anonymous-namespace helper appended to existing one (mutex-protected lazy-init FILE* via USERPROFILE → `Documents/BaySickDAW/qa-sfizz-task4-trace.log`).
  - **RUSTY-PROCESS-STRIPS entry trace** at top of `processStrips(int numFrames, juce::MidiBuffer& midi)` — Rusty's MAIN multi-out audio-thread entry path (called from PluginProcessor for per-strip InsertNode dispatch under multi-output routing).  Logs thread + mSfizz wrapper ptr + numFrames + midi count + multi-out scratch ptr.
  - **RUSTY-PROCESS-BLOCK entry trace** at top of `processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)` — standalone-fallback audio-thread entry path.  Logs thread + mSfizz wrapper ptr + numFrames + numChannels + output buffer ptr + midi count.
  - **RUSTY-NOTEON-AUDITION trace** before the audition-path `mSfizz->noteOn` call (line ~211) — UI-thread audition exchanged into audio thread via mAuditionNote atomic.  Logs thread + mSfizz ptr + note + velocity.
  - **RUSTY-NOTEON-MIDI trace** before the MIDI-buffer-iteration `mSfizz->noteOn` call (restructured from inline to braced block) — logs thread + mSfizz ptr + note + velocity + delay.

- [libs/sfizz/src/sfizz/Synth.cpp](../../libs/sfizz/src/sfizz/Synth.cpp) (+45 lines):
  - Anonymous-namespace helper at top of `namespace sfz {` (parallel to the Rusty side; each TU has its own `FILE*` writing to the same path — `fopen "a"` mode is OS-thread-safe at the filesystem level).
  - **SFIZZ-RENDER-BLOCK entry trace** at top of `Synth::renderBlock(AudioSpan<float> buffer) noexcept` (line :1093) — single log per invocation capturing thread + `this` (Synth inner instance ptr) + numFrames.  **NOT** Rusty-only — fires for ALL sfizz instances (Rusty + Guitars + Basses).  Filter by `synth=` pointer correlating with Rusty's `mSfizz=` log line.

**Rule 4 Diagnostic Instrumentation Catalog rows added above** (2 new Sub-F=(e) entries; both `Remove at Task 5 close`).

**Diff stat:** 2 files / +143 / -1.  Working tree includes Sub-F=(e) instrumentation + the qa-sfizz-task3-trace.log artifact (untracked, left from Task 3).

**Next action:** Jeff deletes any existing `qa-sfizz-task4-trace.log` (separate filename from task3) → builds → reproduces the bit-crusher with the 6-cymbal crash test under MT-on → captures the new trace.  Analyze for: (a) multiple thread IDs in RUSTY-PROCESS-STRIPS lines with the same mSfizz pointer (= multiple audio threads hitting same Rusty instance concurrently — race confirmed at boundary); (b) multiple thread IDs in SFIZZ-RENDER-BLOCK lines with the same `synth` pointer (= same race confirmed inside sfizz boundary); (c) same outBufPtr or output buffer overlap across thread IDs (= concurrent buffer write race).  If any of (a)/(b)/(c) fires → instance-access-violation hypothesis confirmed; fix is dispatcher-side serialization (NOT sfizz-internal).  If boundary is clean → expand to Option (a) broad voice-lifecycle sweep per Jeff's escalation plan.

---

## 2026-05-28 — Task 4 — Trace analysis + Sub-G lock (race confirmed)

Jeff captured `qa-sfizz-task4-trace.log` during 6-cymbal crash MT-on test.  **Hypothesis confirmed empirically:** 9 distinct thread IDs hit the SAME Rusty `mSfizz` wrapper pointer + the SAME inner `synth` pointer + summed into overlapping output-buffer scratch across consecutive blocks.  VibeThreadPool's RenderGraphDispatcher is task-stealing audio blocks for the same `BaySickRustyDrumsProcessor` instance across multiple worker threads — `sfz::Sfizz` is NOT thread-safe for concurrent renders on a single instance, so the audible bit-crusher is concurrent voice-state mutation + buffer-write interleaving inside sfizz.  Cymbals/hi-hats are audibly affected because their long sustains stack many voices over time; kick/snare/toms have short transients with low concurrent voice counts, so the same race is present but inaudible (or perceived as transient-edge artifacts swamped by the drum's natural attack).  Same race exists on BaySickGuitars + BaySickBasses despite no observable artifact — their content lacks the long-sustaining bit-crusher-prone samples but the dispatcher pattern is identical.

Jeff verbatim on the dual-track resolution: **"Bullseye. The logging proves exactly what we suspected... Here is how we are handling Sub-G: The Immediate Fix: Option (a) juce::SpinLock... The Architectural Roadmap: Option (d) Dispatcher Bug Hunt..."**

**Sub-G = (a) lock (Jeff 2026-05-28):** `juce::SpinLock` + `ScopedLockType` at the audio-thread entry points on all 3 sfizz-driven engines (Rusty / Guitars / Basses).  Rusty needs both `processStrips` (multi-out path) + `processBlock` (standalone fallback path).  Guitars + Basses single stereo out → `processBlock` only.  Rationale per Jeff: "It is the lowest-risk, smallest-diff fix to immediately address the audible bit-crusher symptom while we plan the architectural rework.  It addresses the immediate race hypothesis (concurrent renders on one Sfizz instance) by ensuring strictly serial execution of that instance, even if the dispatcher attempts to parallelize."

**Sub-H = (d) lock (Jeff 2026-05-28):** Dispatcher Bug Hunt as architectural roadmap — investigate `VibeThreadPool` / `RenderGraphDispatcher` to find why a single engine instance gets parallelized across blocks (correct DAG-MT semantics dispatch one block per node-instance per audio cycle, never multiple workers on the same instance).  Routed to its own batch per Main Plan §0 Rule 3 (new architectural work area outside QA-Sfizz's diagnosis scope).

---

## 2026-05-28 — Task 5 — Sub-G=(a) SpinLock implementation (Item 3 fix)

Sub-G=(a) `juce::SpinLock` landed in working tree across all 3 sfizz-driven engine processors.  Sub-F=(e) Task 4 instrumentation stripped from `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp` + `libs/sfizz/src/sfizz/Synth.cpp` via `git restore --source=HEAD` per Rule 4 catalog Disposition (mirrors the Sub-D=(c) strip pattern at Task 3 close `f477e39`).

**Source changes (6 files, +60/-0 pure additive):**

- [BaySickRustyDrumsProcessor.h](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h) (+14 lines): added `juce::SpinLock mSfizzAccessLock;` member after `mSfizz` declaration with an 11-line comment block citing Sub-G=(a) + the Sub-F=(e) trace findings + the QA-DispatcherAffinity retirement plan (§9 fortieth Forks entry / Main Plan §5).
- [BaySickRustyDrumsProcessor.cpp](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp) (+12 lines): `const juce::SpinLock::ScopedLockType lk (mSfizzAccessLock);` at top of `processStrips(int, juce::MidiBuffer&)` (line 194) AND at top of `processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&)` (line ~295) — both audio-thread entry paths covered.  Each wrap carries a 2-line cross-reference comment pointing back to the `.h` declaration.
- [BaySickGuitarsProcessor.h](../../Source/BaySickGuitars/BaySickGuitarsProcessor.h) (+12 lines): same `juce::SpinLock mSfizzAccessLock;` member with a parallel comment block noting the defensive application (Guitars has no observable bit-crusher artifact, but the dispatcher race is structurally identical to Rusty so the lock is correct preventative coverage until QA-DispatcherAffinity lands).
- [BaySickGuitarsProcessor.cpp](../../Source/BaySickGuitars/BaySickGuitarsProcessor.cpp) (+5 lines): SpinLock wrap at top of `processBlock` (line 176) — single audio-thread entry path on Guitars (no separate `processStrips` since it's stereo-out only, no multi-out routing).
- [BaySickBassesProcessor.h](../../Source/BaySickBasses/BaySickBassesProcessor.h) (+12 lines): parallel to Guitars `.h` — defensive lock, comment cites same Sub-F=(e) reasoning.
- [BaySickBassesProcessor.cpp](../../Source/BaySickBasses/BaySickBassesProcessor.cpp) (+5 lines): SpinLock wrap at top of `processBlock` (line 173) — single audio-thread entry path on Basses.

**Lock characteristics:**

- `juce::SpinLock` is the audio-thread-safe primitive for short critical sections (vs `juce::CriticalSection` which is a kernel mutex with priority-inversion risk).  Spin-wait is bounded to the duration of one block's sfizz render (~1-5 ms in practice for typical voice counts).
- `ScopedLockType` is RAII — lock released on scope exit including exception unwind.
- No reentrancy: `processStrips` and `processBlock` are mutually exclusive entry points (PluginProcessor picks one or the other per dispatch, never both for the same instance in the same block), so no nested-lock risk on Rusty.
- Audition path (`mSfizz->noteOn` called from `processBlock`/`processStrips` body after the lock) is already inside the locked region — no separate handling needed.

**Why defensive lock on Guitars + Basses despite no observable bit-crusher:**

The dispatcher bug is engine-agnostic.  VibeThreadPool's task-stealing behavior depends on which `AudioProcessor` instance the worker thread happens to pick — it's deterministic per-block but the cross-block scheduling is non-deterministic across thread workers.  Guitars + Basses kits today don't produce long-sustaining samples that make the race audible (single-note guitar / bass attacks decay quickly, low concurrent voice counts), but if a kit author ships a long-sustained pad-style guitar patch in the future the race would surface.  Better to land the lock now (band-aid uniformly across all 3 sfizz engines) than re-open the file 6 months later when a new kit triggers the race on Guitars.

**Rule 4 Catalog disposition update (Sub-F=(e) rows):**

Both Sub-F=(e) rows in the Diagnostic Instrumentation Catalog above will be updated from `Remove at Task 5 close` → `Removed at Task 5 close (commit <hash> via git restore --source=HEAD)` at the Task 5 commit step (post-Jeff-verify).  The strip is already done in the working tree.

**QA-DispatcherAffinity routing landed in Main Plan (per Jeff "Wait that's not just a forks entry but a whole new batch entry, correct?" → confirming Rule 3):**

Per Main Plan §0 Rule 3, a new work area NOT in scope for the current batch needs a NEW §5 batch row + §6 sequencing arrow update + §9 Forks entry.  5 edits landed:

1. **§6 arrow update:** Added QA-DispatcherAffinity immediately after QA-Sfizz / before QA-EngineApvts in the Phase 1 sequencing arrow.
2. **§5 new batch entry:** QA-DispatcherAffinity row added with full scope (replace VibeThreadPool single-Sfizz parallelization with DAG-correct dispatcher; retire the Sub-G=(a) SpinLock from all 3 sfizz engines after dispatcher fix verifies; broader audit for other AudioProcessor types that may share the same race risk).
3. **§5 QA-EngineApvts sequencing update:** "after QA-Sfizz" → "after QA-DispatcherAffinity" in the prerequisites line; cross-reference added.
4. **§6 new 25-asterisk footnote** for QA-DispatcherAffinity + **QA-EngineApvts footnote update** to cite the new precedent slot.
5. **§9 fortieth Forks entry** documenting the Sub-F=(e) trace finding + the dual-track Sub-G=(a) immediate fix + Sub-H=(d) QA-DispatcherAffinity routing + Jeff's verbatim "Bullseye" lock + cross-reference to QA-Sfizz Task 5 commit (hash pending) for the SpinLock retirement target.

QA-DispatcherAffinity slot lock = (a) immediately after QA-Sfizz close, before QA-EngineApvts.  Batch name lock = `QA-DispatcherAffinity` (no "QA-DispatcherBug" or "QA-MTRace" alternatives surfaced — Jeff's a-and-a pick endorsed the canonical name pattern).

**Files touched in Task 5 (pre-commit):**

- 3 engine processors × 2 files each (.h + .cpp) = 6 source files modified per the diff above.
- 2 sfizz library files (`Synth.cpp` + `BaySickRustyDrumsProcessor.cpp`) had their Sub-F=(e) instrumentation stripped via `git restore --source=HEAD` — `Synth.cpp` is fully back to HEAD baseline; `BaySickRustyDrumsProcessor.cpp` is back to HEAD baseline THEN the SpinLock additions land on top (clean separation: strip first, add new code second, no merge conflict between the two).
- `Plans & Specs/Main Plan.md` (5 edits per the §5/§6/§9 routing summary above).
- `Plans & Specs/Running Notes/amber-tracking-mongoose.md` (this Task 5 entry + the Task 4 trace-analysis entry preceding it).

**Stale artifacts in repo root:** `qa-sfizz-task3-trace.log` + `qa-sfizz-task4-trace.log`.  Both untracked.  Jeff to delete manually when desired; .gitignore additions deferred (YAGNI — task-numbered .log filenames won't recur outside diagnostic windows).

**Next action:** Jeff builds `do_build.bat` → runs Debug + Release per per-task verify cadence.  Verify scope:

- **BaySickRustyDrums MT-on, 6-cymbal crash test:** bit-crusher GONE on cymbals + hi-hats (cleanly serialized sfizz renders, no concurrent buffer-write race).  Kick/snare/toms unchanged from MT-on baseline (no audible difference since they weren't racing audibly to begin with).
- **BaySickRustyDrums MT-off:** unchanged (lock is a no-op when only one thread enters the audio path).
- **BaySickGuitars + BaySickBasses:** defensive lock applies — no observable change expected (their kits don't produce the long-sustain race trigger).  Verify no regression: same audible output as pre-Task-5 baseline + no CPU jump under polyphonic chord playback (SpinLock contention should be effectively zero since the dispatcher rarely parallelizes these engines in current workloads).
- **No interaction with Sub-E (CC=64 Aria emulation):** Sub-E lives in `loadKit` / `createLayout` / `getKitDefaultCc` — none of those paths are inside the SpinLock-wrapped audio entry points, so the Sub-E behavior is unaffected by Sub-G.

On Jeff verify pass: `/draft-commit` → surface drafted message + full git status → commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-Sfizz-Task5.txt`.  Then Task 6 (batch close): `/draft-doc batch-close` → apply to Implemented Work Log → `/review-batch` → QA-Sfizz §5 STATUS banner update → final close commit.

---

## 2026-05-28 — Task 5 follow-up — Sub-G=(a) verify FAIL + Sub-I=(c) widened leaf-node A/B test + Main Plan §5/§6/§9 demote

**Sub-G=(a) verify outcome (Jeff 2026-05-28 post-build):** "Still bit crusher sound on rusty."  The narrow audio-entry-only SpinLock (processStrips on Rusty / processBlock on Guitars/Basses) did NOT cure the symptom.  Bit-crusher unchanged on long-sustaining cymbals + hi-hats under MT-on.

**Trace-vs-concurrency reinterpretation — own the misread:**

Re-examined the Sub-F=(e) trace evidence post-Sub-G fail.  The trace captured ENTRY-only thread IDs (no exit timestamps).  9 distinct thread IDs across 1804 calls over a 6-second test is fully consistent with WORK-STEALING task-pool affinity churn (JUCE/VibeThreadPool rotates worker handling consecutive blocks; non-deterministic which worker gets a given block, but only ONE worker per block).  The trace did NOT prove overlap.  An uncontended SpinLock around `processStrips` + `processBlock` adds ns-scale overhead per block when only one thread holds it at a time — exactly the observed result post-Sub-G=(a) (no audible behavior change).

I called the Sub-F=(e) trace "Bullseye" evidence at Sub-G=(a) lock; that was a misread.  Concurrent renderBlock execution is now RULED OUT as the bit-crusher root cause.

**Sub-I=(f) + Sub-I=(c) lock (Jeff 2026-05-28 follow-up):**

Jeff's pick: revert Sub-G=(a) (Option (f)) then widen the lock (Option (c)).  Widened scope spec: "apply a temporary, broad lock that synchronizes PluginProcessor::processBlock WITH the parameterChanged APVTS listener (and any other asynchronous paths like sendCc or MIDI message injection).  Let's see if fencing off the entire plugin instance from asynchronous state mutation cures the bit-crusher."

Lock-scope clarification surfaced: engine-level vs PluginProcessor-level.  Jeff confirmed engine-level: "A PluginProcessor-level lock is far too aggressive and would cause unnecessary blocking across independent tracks.  Each sfizz instance needs its own localized fence to protect its specific state."

**Reentrancy issue surfaced + path matrix re-locked (Jeff 2026-05-28 follow-up):**

Walking the call chain pre-implementation revealed three deadlock paths in the broad coverage:

1. `processBlock` (Rusty) calls `processStrips` internally — locking both would deadlock the standalone-fallback path on a non-reentrant SpinLock.
2. `sendCc` writes APVTS via `setValueNotifyingHost` which fires `parameterChanged` synchronously on the calling thread — locking sendCc deadlocks against parameterChanged's lock acquire.
3. `loadKit` writes ~512 APVTS CC params via `setValueNotifyingHost` in the Sub-E reset loop — same deadlock pattern in a loop.

Proposed (and Jeff-approved 2026-05-28) leaf-node-locking matrix avoids all three:

| Path | Rusty | Guitars | Basses | Reason |
|------|-------|---------|--------|--------|
| `processStrips` | LOCK | n/a | n/a | Real audio render path on Rusty |
| `processBlock` | skip | LOCK | LOCK | On Rusty: delegates to processStrips (would deadlock).  On Guitars/Basses: it IS the audio path |
| `parameterChanged` | LOCK | LOCK | LOCK | Where the actual `mSfizz->cc()` race lives |
| `sendCc` | skip | skip | skip | Goes through APVTS → parameterChanged (locked downstream); locking would deadlock |
| `loadKit` | skip | skip | skip | `mProcessingEnabled` gate fences the audio thread; interior APVTS writes for CC defaults fire parameterChanged sync → would deadlock |
| `getNumActiveVoices` | LOCK | LOCK | LOCK | Reads sfizz state directly; `mutable` lock to allow const accessor |
| `getKeyswitchLabel` | LOCK | LOCK | LOCK | Reads sfizz state directly; `mutable` lock to allow const accessor |

Jeff verbatim on the matrix: "We are absolutely going with Option (a): Proposed effective coverage (Leaf-node locking).  Locking at the leaf node (parameterChanged) is the standard practice.  It safely catches the mutations coming from sendCc and loadKit without risking a reentrant deadlock."

Jeff verbatim on lock primitive: "Do not use juce::CriticalSection; we cannot risk priority inversion on the audio thread for an A/B test."  → `juce::SpinLock` retained (audio-thread-safe), `mutable` for the const accessors.

**Sub-I=(c) source landing — 6 files / +100 / -0 pure additive:**

- All 3 sfizz engine processors got `mutable juce::SpinLock mSfizzAccessLock;` member declarations with Sub-I=(c) comment blocks (one engine has full rationale; Guitars + Basses cross-ref Rusty for brevity).
- 12 `const juce::SpinLock::ScopedLockType lk (mSfizzAccessLock);` acquires across the 3 .cpp files matching the matrix above (4 per engine).
- Guitars + Basses `processBlock` acquires placed AFTER the `mProcessingEnabled` early-exit check so the disabled-path early-exit doesn't acquire the lock for no work.
- Rusty `processStrips` acquires at the very top (no equivalent gate); `processBlock` on Rusty unchanged (delegates to processStrips which now holds the lock).

**Sub-G=(a) revert mechanics:**

`git restore --source=HEAD -- <6 engine processor files>` to bring all 3 sfizz engine .h + .cpp pairs back to post-Task-3-close baseline `f477e39`.  Then the Sub-I=(c) edits land cleanly on top.  Main Plan + Running Notes modifications stay untouched throughout.

**Main Plan §5/§6/§9 demote (Sub-J = soften, Jeff 2026-05-28):**

Jeff's pick on the QA-DispatcherAffinity routing: "Soften to a §9 Forks placeholder.  Since thread churn across blocks is expected behavior, we shouldn't commit an entire batch to tearing apart the dispatcher.  Demote the QA-DispatcherAffinity batch to a §9 Forks placeholder until we have hard proof the dispatcher is actually at fault."

Five Main Plan edits landed (`Plans & Specs/Main Plan.md` net +63 / -5):

1. §5 — QA-DispatcherAffinity batch row REMOVED entirely.
2. §5 — QA-EngineApvts Sequencing field reverted from "after QA-DispatcherAffinity" to "after QA-Sfizz" + history trail simplified.
3. §6 — sequencing arrow QA-DispatcherAffinity insertion removed.
4. §6 — 25-asterisk QA-DispatcherAffinity footnote REMOVED entirely.
5. §6 — QA-EngineApvts 24-asterisk footnote reverted from "after QA-DispatcherAffinity" to "after QA-Sfizz" wording.
6. §9 — fortieth Forks entry REWRITTEN as PLACEHOLDER: captures Sub-F=(e) trace finding + Sub-G=(a) try-and-revert + Sub-I=(c) widened lock A/B test underway + dispatcher-batch routing DEFERRED pending A/B outcome.  If Sub-I=(c) cures the bit-crusher → no dispatcher batch needed (lock stays as production fix; entry stays as historical record).  If Sub-I=(c) does NOT cure → APVTS-mutation hypothesis falsified; next phase routes to Sub-I (d) or (e) before any QA-DispatcherAffinity routing is reconsidered.

**Rule 4 Diagnostic Instrumentation Catalog status (no updates required):**

Both Sub-F=(e) catalog rows (Synth.cpp + BaySickRustyDrumsProcessor.cpp instrumentation) were already stripped + marked `Removed at Task 5 close (via git restore)` at the original Sub-G=(a) close prep.  Sub-G=(a) lock did NOT carry diagnostic instrumentation (it was a production-shape fix attempt, not a trace), so no catalog rows to add or strip at revert time.  Sub-I=(c) widened lock is also production-shape (no instrumentation); no new catalog rows.

**Files touched in Task 5 follow-up (working-tree state, pre-commit):**

- 3 engine processor .h files: `mutable juce::SpinLock mSfizzAccessLock;` member declarations (+44 across 3 files; Rusty has the verbose rationale block, Guitars + Basses cross-ref Rusty).
- 3 engine processor .cpp files: 4 `ScopedLockType` acquires per file (+56 across 3 files; comments at each acquire site).
- `Plans & Specs/Main Plan.md` — 5 edits (§5 batch row delete + §5 QA-EngineApvts sequencing revert + §6 arrow revert + §6 footnote delete + §6 QA-EngineApvts footnote revert + §9 fortieth Forks entry placeholder rewrite).  Net +63 / -5.
- `Plans & Specs/Running Notes/amber-tracking-mongoose.md` — this Task 5 follow-up entry appended.

**Stale artifacts in repo root:** `qa-sfizz-task3-trace.log` + `qa-sfizz-task4-trace.log` (both untracked, both from prior task diagnostic windows; Jeff to delete manually when desired).

**Next action:** Jeff builds `do_build.bat` → runs Debug + Release per per-task verify cadence.  Verify scope:

- **BaySickRustyDrums — MT ON — 6-cymbal crash test (the A/B test pivot point):**
  - If bit-crusher GONE → APVTS-mutation-during-render IS the cause.  Sub-I=(c) lock stays as production fix.  Close QA-Sfizz at Task 6 with the leaf-node lock as the Item 3 resolution.  §9 fortieth Forks entry stays as historical record; no dispatcher batch needed.
  - If bit-crusher PERSISTS → async APVTS mutation is NOT the cause.  Hypothesis falsified.  Sub-I=(c) lock decision deferred to next investigation phase (Sub-I (d) sfizz-internal-threading or (e) symptom-characterization mode); §9 fortieth Forks entry stays as PLACEHOLDER until next outcome.
- **BaySickRustyDrums — MT OFF — full pattern playback:** expected unchanged (lock contention zero when only one thread enters audio path).
- **BaySickGuitars + BaySickBasses — chord playback + keyswitched articulations:** expected no audible regression (defensive locks on those engines; no observable race on their content).
- **CPU sanity (optional):** polyphonic chord playback under MT-on, watch DSP meter for any unexpected jump.  Leaf-node lock contention should be effectively zero in current workloads (the 4 leaf paths are short critical sections; parameterChanged only fires on actual CC changes, not continuously).

On Jeff verify outcome:
- A/B PASS → `/draft-commit` for Task 5 follow-up commit (single commit landing both Sub-I=(c) source + Main Plan §5/§6/§9 demote + this Running Notes entry).  Then Task 6 batch close.
- A/B FAIL → Sub-K spec call for next investigation phase (no commit yet; revert decision part of Sub-K scope).

---

## 2026-05-28 — Task 5 follow-up #2 — Sub-I=(c) verify FAIL → Sub-K=(custom Serial Fallback) lands; Candidate A → QA-DispatcherAffinity restored

**Sub-I=(c) verify outcome (Jeff 2026-05-28 post-build):** "It persists and confirmed in st it still works fine."  Bit-crusher persists on Rusty MT-on; ST/MT-off confirmed clean.  Both Sub-G=(a) narrow audio-entry lock AND Sub-I=(c) widened leaf-node lock failed to cure the symptom.  APVTS-mutation-during-render hypothesis FALSIFIED — race is NOT at the engine/sfizz state level on either the audio-entry surface or any APVTS leaf path.

**Empirical rule-outs from the two-fix sequence:**

| Hypothesis | Tested via | Result |
|------------|-----------|--------|
| Concurrent `renderBlock` on same `mSfizz` instance | Sub-G=(a) narrow audio-entry lock | FAILED → ruled out |
| Async APVTS mutation racing with audio render | Sub-I=(c) widened leaf-node lock | FAILED → ruled out |
| ST/MT-off path | Jeff's confirmation | CLEAN → it's specifically a threading issue, not a code-correctness issue |

The race must live in a path that bypasses BOTH the audio-entry SpinLock AND the parameterChanged SpinLock — i.e., DOWNSTREAM of processStrips OR INSIDE sfizz via internal threading.

**Candidate A surfaced (mMultiOutScratch read-write race across block boundaries):**

Rusty's `processStrips` writes 13 strips into the engine-internal `mMultiOutScratch`.  PluginProcessor's per-strip RustyInsertTask family then reads each strip from that scratch via `getStripBuffer` — and runs in parallel on workers.  If the dispatcher doesn't BARRIER between blocks (i.e., block N+1's `RustyDrumsProducerTask` starts WRITING `mMultiOutScratch` before block N's 13 RustyInsertTasks have finished READING it), readers see torn samples → bit-crusher artifact at the InsertNode output stage.

This matches:
- Why ONLY Rusty has the symptom (it's the only engine with multi-out scratch + per-strip InsertNode fan-out — Guitars/Basses are single stereo out, no scratch sharing).
- Why ONLY long sustains audibly express it (cymbals hold the same scratch slot in use across many consecutive blocks; transient kicks finish before block N+1 fires).
- Why Sub-G + Sub-I both failed (both lock the WRITE side; the InsertNode READ side is downstream — `RustyInsertTask::run()` reads `getStripBuffer` then runs its insert chain on workers without acquiring any sfizz-engine lock).
- Why the Sub-F=(e) trace showed 9 thread IDs on one `mSfizz` (sequential block dispatches on different workers — thread-affinity churn — NOT concurrent on the same block; the trace evidence was misread at Sub-G lock time).

**Secondary hypothesis (Candidate B, less compelling but in-scope for QA-DispatcherAffinity):** sfizz thread-local-state-vs-thread-affinity migration.  When the same engine's producer task runs on worker A in block N and worker B in block N+1, any per-thread state in the vendored sfizz library (thread-local sample buffers / RNG / voice scratch) gets ping-ponged across workers; long-sustaining cymbal voice state may not survive that migration cleanly.  Less compelling because it should affect Guitars + Basses too (same vendored library), and the bit-crusher is Rusty-only — but Jeff's "keep Guitars/Basses safe on the serial path for now" defensive note covers it.

**Sub-K = (custom Serial Fallback) lock (Jeff 2026-05-28 follow-up):**

Jeff verbatim on the architectural-batch scoping discipline: "Candidate A is a fantastic architectural catch.  The mMultiOutScratch read-write race across block boundaries perfectly explains why this only affects the multi-out Rusty engine, why the FX rack InsertNodes are catching torn samples on long sustains, and why both lock attempts failed.  However, building a barrier-based synchronization system for the dispatcher is way out of scope for QA-Sfizz.  We need to stabilize the audio and close this batch."

Jeff verbatim on Sub-K shape: "Sub-K: Custom Option (e) Serial Fallback.  We are bypassing the bug for now.  Modify the dispatch logic to route BaySickRustyDrums, BaySickGuitars, and BaySickBasses OUT of the MT path.  Force them to render strictly on the serial path.  (Even though Guitars/Basses don't show the symptom, they use the same vendored library, so keep them safe on the serial path for now)."

Jeff verbatim on roadmap: "Restore the QA-DispatcherAffinity batch to the Main Plan.  Add Candidate A (mMultiOutScratch cross-block boundary race) to its scope definition.  We will build the proper dispatcher barriers in that dedicated batch."

**Critical architectural constraint discovered at Sub-L planning (RenderEngineFlags.h):**

The pre-existing "MT-off" toggle (`gMultiThreadedEngineEnabled`) does NOT route through a separate serial code path — that was deleted at QA-Ef 2026-05-21 ("the serial render path was deleted and the dispatcher became the single, unconditional render path").  MT-off just parks the worker threads; the audio thread drains the ENTIRE graph itself via `runUntilOrTimeout`.  Per-engine serial fallback therefore requires PER-TASK audio-thread affinity within the unified dispatcher, not routing to a non-existent separate code path.

**Sub-L = (impl-1) lock (Jeff 2026-05-28 follow-up):**

Per-task `bool mAudioThreadOnly` flag on `RenderTask` + dual-queue MPSC infrastructure in `VibeThreadPool`.  Worker-eligible tasks go to the existing MPMC queue (`queue`); audio-thread-only tasks go to a new MPSC queue (`audioThreadQueue`) that ONLY `runUntilOrTimeout` drains.  Workers never see the audio-thread queue.  Priority pop: audio thread tries `audioThreadQueue.try_dequeue` BEFORE `tryPop()` (worker queue) so audio-thread tasks make timely progress.

Jeff lock: "This is the correct, standard lock-free pattern for thread-pinning.  Option 2 (worker requeue) would cause catastrophic cache trashing and priority inversion spinning.  Option 3 breaks the DAG model."

**Sub-M = (eng-b) lock (Jeff 2026-05-28 follow-up):**

InstStripTask's `mAudioThreadOnly` flag is set at engine-swap time on the message thread inside `VibeSynthProcessor::registerInstEngine` (PluginProcessor.cpp:3768).  Detection via `dynamic_cast<BaySickGuitarsProcessor*>(eng)` or `dynamic_cast<BaySickBassesProcessor*>(eng)` — both engine headers already included in PluginProcessor.cpp.  The `unregisterInstEngine` → `registerInstEngine` sequence on every engine swap means a fresh InstStripTask is built with the current engine kind, so the flag stays in lockstep with the active engine on the page.

Jeff lock: "Using RTTI (dynamic_cast) inside the audio thread's per-block dispatch loop is a massive anti-pattern that can cause non-deterministic execution times.  Computing the flag strictly on the UI/Message thread during an engine swap guarantees zero overhead in the audio hot path."

**Sub-K source landing — 5 files / ~+125 / -0 pure additive:**

- [Source/Engine/RenderTask.h](../../Source/Engine/RenderTask.h) (+16 lines) — `bool mAudioThreadOnly = false;` field with Sub-K/Sub-L rationale block; declared as a plain bool (not atomic) since it's set on the message thread at task construction or engine-swap time, never from the audio thread.
- [Source/Engine/VibeThreadPool.cpp](../../Source/Engine/VibeThreadPool.cpp) (+66 lines) — second `moodycamel::ConcurrentQueue<RenderTask*> audioThreadQueue` in `Impl`; `submit()` routes by `task->mAudioThreadOnly` (audio queue skips worker wake; worker queue keeps existing wake behavior); `runUntil` + `runUntilOrTimeout` priority-pop `audioThreadQueue.try_dequeue` ahead of the existing `tryPop()` worker queue; `clearQueues` drains both.
- [Source/Engine/Tasks/RustyDrumsProducerTask.cpp](../../Source/Engine/Tasks/RustyDrumsProducerTask.cpp) (+10 lines) — constructor sets `mAudioThreadOnly = true;` natively.
- [Source/Engine/Tasks/RustyInsertTask.cpp](../../Source/Engine/Tasks/RustyInsertTask.cpp) (+12 lines) — constructor sets `mAudioThreadOnly = true;` natively (covers all 13 strip tasks for Rusty's multi-out).
- [Source/PluginProcessor.cpp](../../Source/PluginProcessor.cpp) (+21 lines) — `registerInstEngine` does `dynamic_cast<BaySickGuitarsProcessor*>(eng) != nullptr || dynamic_cast<BaySickBassesProcessor*>(eng) != nullptr` and sets `task->mAudioThreadOnly = true` before `mRenderDispatcher.registerTask(task.get())`.

**Sub-I=(c) lock revert mechanics:**

`git restore --source=HEAD -- Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.{h,cpp} Source/BaySickGuitars/BaySickGuitarsProcessor.{h,cpp} Source/BaySickBasses/BaySickBassesProcessor.{h,cpp}` to bring all 3 sfizz engine .h + .cpp pairs back to post-Task-3-close baseline `f477e39`.  Working tree contains only the Sub-K source changes + the Main Plan §5/§6/§9 restore + this Running Notes entry.

**QA-DispatcherAffinity restored to Main Plan with Candidate A added to scope:**

Six Main Plan edits (re-restoring the QA-DispatcherAffinity routing that was demoted at Sub-I lock + adding Candidate A to its scope):

1. §5 — QA-DispatcherAffinity batch entry RESTORED between QA-Sfizz and QA-EngineApvts with NEW scope adding "Candidate A — cross-block barrier enforcement" (primary) + "Candidate B — instance affinity / thread-local-state continuity" (secondary).
2. §5 — QA-EngineApvts Sequencing field re-updated from "after QA-Sfizz" to "after QA-DispatcherAffinity".
3. §6 — sequencing arrow QA-DispatcherAffinity insertion restored: `... → QA-Sfizz************************ → QA-DispatcherAffinity************************* → QA-EngineApvts**********************...`.
4. §6 — 25-asterisk QA-DispatcherAffinity footnote restored with the new Candidate A primary + Candidate B secondary framing + the Sub-K Serial Fallback band-aid retirement scope.
5. §6 — QA-EngineApvts 24-asterisk footnote re-updated from "after QA-Sfizz" to "after QA-DispatcherAffinity".
6. §9 — fortieth Forks entry REWRITTEN from PLACEHOLDER → SETTLED ROUTING: captures full Sub-F=(e) trace → Sub-G=(a) fail → Sub-I=(c) fail → Sub-K Serial Fallback lands → QA-DispatcherAffinity restored journey; documents both Candidates A + B as the architectural-batch scope; lessons-learned bullet on entry-only-trace misread.

**Rule 4 Diagnostic Instrumentation Catalog status (no updates required):**

Sub-K Serial Fallback is production-shape (no diagnostic instrumentation); no new catalog rows.  The earlier Sub-F=(e) catalog rows from Task 4 are still marked `Removed at Task 5 close` from the original Sub-G prep — that disposition is correct regardless of the Sub-G→Sub-I→Sub-K pivot since the instrumentation was indeed stripped (just before Sub-G/Sub-I/Sub-K were tried).

**Files touched in Task 5 follow-up #2 (working-tree state, pre-commit):**

- 5 source files modified per the Sub-K landing summary above (total ~+125 lines pure additive).
- `Plans & Specs/Main Plan.md` — 6 edits per the QA-DispatcherAffinity restore summary above.  Net ~+125 / -10 relative to pre-Task-5 baseline.
- `Plans & Specs/Running Notes/amber-tracking-mongoose.md` — this Task 5 follow-up #2 entry appended.

**Stale artifacts in repo root:** `qa-sfizz-task3-trace.log` + `qa-sfizz-task4-trace.log` (both untracked, both from prior task diagnostic windows; Jeff to delete manually when desired).

**Next action:** Jeff builds `do_build.bat` → runs Debug + Release per per-task verify cadence.  Verify scope (the FINAL 6-cymbal crash test for QA-Sfizz):

- **BaySickRustyDrums — MT ON — 6-cymbal crash test (the bit-crusher repro):**
  - Expect: bit-crusher GONE on cymbals + hi-hats.  Sub-K Serial Fallback pins all 14 Rusty tasks (1 producer + 13 inserts) to the audio thread → strictly sequential execution → no cross-block scratch race → no torn samples → no bit-crusher.
  - Listen for: pure crash decays, no granular digital artifact riding the tail.
- **BaySickRustyDrums — MT OFF — full pattern playback:** expected unchanged (audio-thread-only pinning is a no-op when only one thread enters the audio path).
- **BaySickGuitars + BaySickBasses — keyswitched articulation + chord playback:** expected no observable change (defensive pinning; their content doesn't exhibit the Rusty multi-out race trigger, but they're now on the audio thread too).
- **Non-sfizz engines (BaySickSolstice / BaySickSynth / BaySickPlayer / BaySickBass / Drums / Layers / Vox live-input):** expected no regression.  The 4 sfizz task families pinned to audio thread are a small minority of total tasks per block; worker-eligible task pool unchanged for all other engines.
- **CPU sanity (optional):** polyphonic chord + heavy drum-pattern playback under MT-on, watch DSP meter for any unexpected jump.  Audio-thread pinning means the 14 Rusty tasks + any sfizz Inst tasks now run on the audio thread instead of workers — slight reduction in MT parallelism for that subgraph, but the worker pool is still active for the rest of the graph.

On Jeff verify outcome:
- **PASS** (bit-crusher gone, no regressions) → `/draft-commit` for Task 5 follow-up commit (single commit landing Sub-K source + Main Plan §5/§6/§9 restore + this Running Notes entry).  Then Task 6 batch close (`/draft-doc batch-close` → Implemented Work Log → `/review-batch` → QA-Sfizz §5 STATUS banner update → final close commit).
- **FAIL** (bit-crusher persists OR new regression introduced) → Sub-N spec call.  Multiple branches possible: investigate whether the InsertNodes downstream of RustyInsertTask are themselves running on workers (the Sub-K pinning doesn't extend to the per-strip InsertNode mixer-chain processing — a finding that would point at the actual race being further downstream than the scratch read); revert Sub-K + escalate to QA-DispatcherAffinity opening immediately as the next batch; OR symptom-characterization mode (mute/solo/voice-count A/Bs) per `feedback_diagnose_before_fixing.md`.

---

## 2026-05-28 — Task 5 follow-up #3 — Sub-O = (int-a) Candidate B sub-mechanism expansion

**Sub-K verify outcome (Jeff 2026-05-28 post-build):** "Pass" — 6-cymbal crash test confirms bit-crusher gone on Rusty MT-on; no regressions on Guitars/Basses/Drums/Layers/Vox/non-sfizz engines.  Sub-K Serial Fallback achieves QA-Sfizz Item 3's bypass goal.

**Pre-commit scope check (Jeff 2026-05-28 follow-up):** Jeff surfaced 3 additional bit-crusher root-cause hypotheses he wanted captured before commit so they wouldn't be lost to chat history:

1. **Non-Atomic RR Voice Swapping** — voice metadata + RR index racing across workers (shared state torn mid-block).
2. **False Sharing / Cache Line Invalidation** — adjacent memory addresses bouncing cache lines between cores, causing stalled workers to miss sub-buffer windows and return stale/uninitialized data.
3. **Disk Streaming Engine Contention** — non-thread-safe streaming queue cross-contaminating sample requests under high-RR I/O load.

Mapped against current QA-DispatcherAffinity scope (Candidate A + B at coarse level): all 3 are NOT currently called out as specific sub-mechanisms; all 3 are in the same general territory as Candidate B (sfizz internals); each is a distinct failure mode worth surfacing as its own sub-mechanism so QA-DispatcherAffinity's investigation phase doesn't miss any.

**Sub-O = (int-a) lock (Jeff 2026-05-28):** "Add to §9 + §5 now... I do not want to lose these specific failure modes to chat history."  Per `feedback_canonical_structure_no_eliding.md` ethos — capture full scope while context is fresh, don't elide for terseness.

**Elegant convergence captured in scope (the architectural insight that drove Jeff's pick):**

Candidate B's per-engine-instance worker-affinity intervention naturally addresses ALL 4 sub-mechanisms (Candidate A's mMultiOutScratch race included) by single-threading sfizz access per engine instance:

- **Candidate A — cross-block scratch race:** killed — same worker handles consecutive blocks sequentially on the same thread.
- **B.1 — Thread-local-state continuity:** killed — only one worker touches the engine's thread-local state, no migration.
- **B.2 — Non-atomic RR voice swap:** killed — only one worker touches voice metadata + RR index, no concurrent reads/writes.
- **B.3 — False sharing:** killed — only one worker touches the engine's hot state, no cross-core cache line bouncing.
- **B.4 — Disk streaming contention:** killed — only one worker requests samples per engine, eliminating cross-thread queue contention.

And worker-affinity doesn't sacrifice non-sfizz MT parallelism (other engine types still run on the worker pool, only sfizz engine tasks are pinned per-instance).

**Sub-O source landing — DOCS ONLY:**

- `Plans & Specs/Main Plan.md` — 2 edits expanding Candidate B in both §5 batch row + §9 fortieth Forks entry into B.1/B.2/B.3/B.4 + sub-mechanism convergence note.  No source code touched.

**Files touched in Task 5 follow-up #3 (working-tree state, pre-commit):**

- `Plans & Specs/Main Plan.md` — Sub-O edits above (on top of the prior Task 5 follow-up #2 §5/§6/§9 restore).
- `Plans & Specs/Running Notes/amber-tracking-mongoose.md` — this Sub-O entry appended.
- (No new source changes from Task 5 follow-up #2 baseline.)

**Next action:** `/draft-commit` for the Task 5 follow-up commit (single commit landing Sub-K source + Main Plan §5/§6/§9 restore-and-expansion + this Running Notes append).  Then Task 6 batch close: `/draft-doc batch-close` → apply to Implemented Work Log → `/review-batch` → QA-Sfizz §5 STATUS banner update → final close commit.
