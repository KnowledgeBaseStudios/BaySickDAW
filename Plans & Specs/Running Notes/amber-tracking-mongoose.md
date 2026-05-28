# Running Notes — QA-Sfizz (amber-tracking-mongoose)

> Append-only execution log for QA-Sfizz. Each entry captures the state at a checkpoint trigger (post-commit / post-sub-task verify / post-finding / post-spec-call / post-scope-pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Append-only — never edit prior entries; new findings surface as new entries.

**Pair file:** `Plans & Specs/Batch Plans/amber-tracking-mongoose.md`
**Convention reference:** Main Plan §0 "Document Formatting Conventions" + "Plan file + Running Notes required sections" (locked 2026-05-11) + Rule 4 Diagnostic Instrumentation Catalog (locked 2026-05-12) + Rule 5 Plan-mode sub-spec-call surface discipline (locked 2026-05-26).

---

## Diagnostic Instrumentation Catalog

Per §0 Rule 4: every diagnostic addition (`DBG` / `juce::Logger::writeToLog` / temp `jassert` added for diagnosis / debug `juce::AlertWindow` popups / temp file logging / `std::cout`-style traces) gets logged here in the same edit pass as the source change. At task/batch close, walk this table and strip every `Remove` entry from source — surface the strip list to Jeff BEFORE running the strip pass.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| [libs/sfizz/src/sfizz/Synth.cpp top + finalizeSfzLoad end](../../libs/sfizz/src/sfizz/Synth.cpp) | `[QA-Sfizz Task3]` | Post-load Region dump per Sub-D=(c) part (a) — verifies whether loaded Region state correctly reflects SFZ-declared `seq_length`/`seq_position`/`sw_last`/`sw_label` opcodes | `Remove at Task 3 close` |
| [libs/sfizz/src/sfizz/Layer.cpp top + registerNoteOn](../../libs/sfizz/src/sfizz/Layer.cpp) | `[QA-Sfizz Task3]` | Note-on dispatch trace per Sub-D=(c) part (b) — captures per-call `counter`, `sequenceSwitched_`, `isSwitchedOn()` to identify which gate blocks RR cycling | `Remove at Task 3 close` |

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
