# Running Notes — QA-SfzGroup (magical-petting-dijkstra)

> Append-only execution log for QA-SfzGroup. Each entry captures the state at a checkpoint trigger (post-commit / post-sub-task verify / post-finding / post-spec-call / post-scope-pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Append-only — never edit prior entries; new findings surface as new entries.

**Pair file:** `Plans & Specs/Batch Plans/magical-petting-dijkstra.md`
**Convention reference:** Main Plan §0 "Document Formatting Conventions" + "Plan file + Running Notes required sections" (locked 2026-05-11) + Rule 4 Diagnostic Instrumentation Catalog (locked 2026-05-12) + Rule 5 Plan-mode sub-spec-call surface discipline (locked 2026-05-26).

---

## Diagnostic Instrumentation Catalog

Per §0 Rule 4: every diagnostic addition (`DBG` / `juce::Logger::writeToLog` / temp `jassert` added for diagnosis / debug `juce::AlertWindow` popups / temp file logging / `std::cout`-style traces) gets logged here in the same edit pass as the source change. At task/batch close, walk this table and strip every `Remove` entry from source — surface the strip list to Jeff BEFORE running the strip pass.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| _(none yet)_ | | | |

---

## 2026-05-26 — Task 0 — Open

Open commit (docs only). Plan-mode session converted §9 thirty-eighth Forks entry's Jeff-locked verbatim 2-track scope into the canonical batch plan file [magical-petting-dijkstra.md](../Batch Plans/magical-petting-dijkstra.md). Eleven spec calls (Sub-A through Sub-K) locked at ExitPlanMode after Sub-I/J/K were surfaced via chat per the newly-adopted Main Plan §0 Rule 5 (plan-mode sub-spec-call surface discipline — locked this batch, ahead of the source work):

- **Sub-A = (a)** Track 2 investigation-only; route any fix to a NEW dedicated batch. Jeff verbatim: "I do not want to mix custom BaySickPlayer parser logic with vendored sfizz state-machine patching in the same batch."
- **Sub-B = (a)** ALL parseSFZ-supported opcodes (12 total) flow through the new inheritance accumulator.
- **Sub-C = (c)** OVERRIDE on the §5 verbatim "group only" lock — full 4-level cascading inheritance `<global>` → `<master>` → `<group>` → `<region>` per SFZ v1 spec. Jeff verbatim: "Libraries use `<global>` constantly; ignoring it now just leaves technical debt."
- **Sub-D = (a)** Verify breadth = Tuba-KS + any Track 2-surfaced degenerate files.
- **Sub-F** = path correction `Source/sfizz/` → `libs/sfizz/` folded into Task 0 (Main Plan §5 line 1302 + §9 thirty-eighth Forks line 4732).
- **Sub-G** = silly-name `magical-petting-dijkstra` assigned by plan-mode runtime.
- **Sub-H** = Debug-then-Release per task verify cadence.
- **Sub-I = (a)** Strict 6-task structure (Task 0 open / Task 1 inventory / Task 2 Track 1 parser fix / Task 3 Track 2 investigation / Task 4 cleanup grep sweep / Task 5 close). Jeff verbatim: "I want absolute separation between the custom parser fix, the Track 2 investigation, and the cleanup phases. Predictable rollback boundaries are mandatory."
- **Sub-J = (a)** Doc-only commit at Task 3 end. Jeff verbatim: "If we find the root cause for the sfizz RR loss, I want those running notes committed immediately as a clean save point before we start Task 4."
- **Sub-K = (c)** Helper member function on a local state-struct (`SfzParseState`) encapsulating the 4-level cascading accumulators. Jeff verbatim: "I want to avoid repetitive pointer checks or brittle per-line target assignments."

**Process meta-finding at Task 0 open (recurring violation, now codified):** initial plan-mode draft included Sub-I / Sub-J / Sub-K as table rows in the "Sub-spec calls surfaced for ExitPlanMode" section while the task body assumed my picks (Task 2 implementation code-block, Task 3 commit shape, 6-task structure). Jeff caught this pattern (third batch in a row after QA-VoicePool + QA-InsertMaps). Five corrections landed at Task 0 ahead of the plan-finalize:
1. `feedback_dont_make_unilateral_spec_calls.md` — appended "Plan-mode discovery rule" section codifying the recurring pattern + correct flow.
2. Main Plan §0 — inserted **Rule 5** ("Sub-spec calls discovered during planning surface via chat BEFORE landing in the plan body") between Rule 4 and Document Formatting Conventions.
3. `Files For Claude/batch_session_boilerplate.md` — strengthened step 5b + added a new standing-rule bullet under the session-open injection.
4. Plan file stripped of pre-picked Sub-I/J/K + their assumed Task bodies; surfaced via chat as actual questions; Jeff answered; locked picks baked back into plan body.
5. Plan file final draft passed ExitPlanMode + approved by Jeff.

Critical pre-plan inventory finding (from Phase 1 Explore agent + supplemental reads): the §9 thirty-eighth Forks diagnosis is CONFIRMED — Tuba-KS.sfz (`AppData/Local/BaySickDAW/CoreLibrary/Brass Package/Tuba-KS.sfz`) has 4 `<group>` blocks at lines 253 / 431 / 609 / 786 each declaring `seq_length=4` + `seq_position=1..4` for staccato variants. parseSFZ's early-return at [`VibePlayerDSP.cpp:148`](../../Source/VibePlayer/VibePlayerDSP.cpp:148) drops every group-scoped opcode → all 4 RR variants get `seq_position=0` → `findRegion` sees no rotation → "they all sound the same." Track 1 fix is structurally a local state-struct (`SfzParseState`) with `enterGlobal/Master/Group/Region/Control/Other` + `tryFlushRegion` + `currentTarget()` methods; per-line dispatch becomes `auto* t = state.currentTarget(); if (! t) continue;` + 12 opcode writes to `*t`. sfizz parser lives at `libs/sfizz/src/sfizz/parser/` (Parser.cpp / ParserPrivate.cpp / Region.h) — Track 3 investigation starting points already located.

**Outside-scope working tree:** `juce/modules/juce_gui_basics/widgets/juce_TreeView.cpp` shows as modified (pre-existing JUCE-vendor modification carried across multiple recent batches; surface-and-leave per QA-VoicePool / QA-AudioMeters / QA-InsertMaps / QA-Eg precedent; not staged).

**Rule 4 Diagnostic Instrumentation Catalog:** nil for Task 0 (docs-only commit, no source instrumentation added).

**Next action:** Task 1 pre-flight inventory (read-only) — full `VibePlayerDSP.cpp` parseSFZ re-read for opcode coverage edge cases, `VibePlayerDSP.h` VibeRegion struct field confirmation, 5-10 Core Library `.sfz` sample for `<global>` / `<master>` / `<group>` usage patterns, sfizz parser landmark reads, BaySick* engine load-handoff site reads. Surface any sub-spec calls that emerge from inventory before Task 2 lands per Rule 5.
