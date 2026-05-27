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

---

## 2026-05-26 — Task 1 — Pre-flight inventory (read-only)

Read-only inventory pass per Sub-I (a) 6-task lock + L8(b)/Sub-K(c) helper-struct implementation prep + QA-VoicePool Task 1 inventory-first precedent. No source touched. Direct reads of `Source/VibePlayer/VibePlayerDSP.cpp` + `.h` + `libs/sfizz/src/sfizz/parser/Parser.cpp` + `libs/sfizz/src/sfizz/Region.h` + BaySickRustyDrums/Guitars/Basses load-handoff sites + sampled Core Library `.sfz` files. Output is plan-finalize confirmation (no new sub-spec calls per Rule 5) + a major Track 3 inventory finding (BaySick* loader-handoff asymmetry).

### Section 1 — parseSFZ opcode coverage confirmed (Track 1 dependency)

12-opcode list confirmed at [`VibePlayerDSP.cpp:151-232`](../../Source/VibePlayer/VibePlayerDSP.cpp:151):

- `sample` (audioData + sampleFile + fileSampleRate; readToEOL=true for spaces in paths) at :151-165
- `key` (sets loNote + hiNote + rootNote to same value; shorthand) at :169-177
- `lokey` (loNote) at :182-183
- `hikey` (hiNote) at :186-187
- `pitch_keycenter` (rootNote) at :192-193
- `lovel` (loVel) at :198-199
- `hivel` (hiVel) at :202-203
- `tune` (tuneOffset, cents -> semitones via /100.f) at :208-209
- `volume` (volumeOffset, dB) at :214-215
- `group` (articulationGroup, clamped 0-3 via jlimit, 1-indexed in SFZ so v-1) at :220-222
- `seq_position` (roundRobinIndex, 1-indexed in SFZ) at :227-228
- `seq_length` (roundRobinTotal) at :231-232

Word-boundary check at `sfzOpcode` ([`VibePlayerDSP.cpp:252-256`](../../Source/VibePlayer/VibePlayerDSP.cpp:252)) correctly disambiguates `key=` vs `lokey=`/`hikey=`/`pitch_keycenter=` (rejects matches preceded by alphanumeric or underscore). All 12 opcodes route through the new `state.currentTarget()` helper in Task 2 per Sub-K (c).

### Section 2 — VibeRegion struct baseline (Sub-K helper-struct copy semantics)

Confirmed at [`VibePlayerDSP.h:11-31`](../../Source/VibePlayer/VibePlayerDSP.h:11). Twelve numeric fields with sensible defaults (rootNote=60 / loNote=0 / hiNote=127 / loVel=0 / hiVel=127 / roundRobinIndex=0 / roundRobinTotal=0 / articulationGroup=0 / tuneOffset=0.0f / volumeOffset=0.0f) + `audioData` shared_ptr (default empty) + `sampleFile` (default empty) + `fileSampleRate=44100.0`. Default-constructed `VibeRegion{}` produces clean baseline; copy-construction is value-only (shared_ptr ref-counted). `mGroupDefaults = mMasterDefaults` and `current = mGroupDefaults` in the new state-machine work cleanly.

### Section 3 — Tuba-KS.sfz file content confirmed (Track 1 verify anchor)

Located at `AppData/Local/BaySickDAW/CoreLibrary/Brass Package/Tuba-KS.sfz` (NOT under Documents/BaySickDAW/). File has 962 lines. `grep "^<"` confirms structure:

- Line 1: `<control>` (default_path=Brass\Tuba\sus\)
- Line 4: `<group>` "Begin Sustain Group" (sw_default=c6 / sw_lokey=c6 / sw_hikey=c#6 / sw_last=c6 / sw_label="C6 Sustain" / ampeg_attack=0.001 / ampeg_release=0.6 / ampeg_dynamic=1 / volume=0 / group_label=gr_1) — sustain group has NO seq_* opcodes
- Lines 20-247: `<region>` blocks for sustain samples
- Line 250: `<control>` (default_path swap to Brass\Tuba\stac\)
- Line 253: `<group>` "Begin Staccato Group 1" + lines 266-267 `seq_length=4` + `seq_position=1`
- Line 431: `<group>` "Begin Group 2" + lines 444-445 `seq_length=4` + `seq_position=2`
- Line 609: `<group>` "Begin Group 3" + lines 622-623 `seq_length=4` + `seq_position=3`
- Line 786: `<group>` "Begin Group 4" + lines 799-800 `seq_length=4` + `seq_position=4`
- 4 staccato regions per `<group>` map to the 4-variant RR cycle

§9 thirty-eighth Forks diagnosis is CONFIRMED — the file specifies the 4-variant RR via `<group>`-scoped `seq_position=1..4` + `seq_length=4`. Pre-batch parser drops all `<group>`-scoped opcodes via the early-return at [`:148`](../../Source/VibePlayer/VibePlayerDSP.cpp:148) -> all 4 staccato regions end up with seq_position=0 + seq_length=0 -> `findRegion` sees no rotation gate -> audibly "they all sound the same." Track 1 verify on Tuba-KS post-Task-2 should hear 4 distinct staccato variants on repeated key strikes. **Bonus finding (Sub-B coverage payoff):** the sustain `<group>` block has `volume=0` at group scope — pre-batch parser drops that too. Post-Task-2 the `volume=0` will correctly apply as a per-region baseline (no audible change since 0 dB is identity, but the inheritance is now correctly implemented).

### Section 4 — sfizz parser landmarks (Track 3 starting points)

sfizz uses a listener-based architecture; the parser is a tokenizer + emitter, semantic accumulation lives in the listener:

- [`libs/sfizz/src/sfizz/parser/Parser.cpp:218-255`](../../libs/sfizz/src/sfizz/parser/Parser.cpp:218) `processHeader()`: generic — calls `flushCurrentHeader()` + sets `_currentHeader = name` (just a string) + emits `_listener->onParseHeader(range, name)`.
- [`libs/sfizz/src/sfizz/parser/Parser.cpp:257`](../../libs/sfizz/src/sfizz/parser/Parser.cpp:257) `processOpcode()`: collects opcodes into `_currentOpcodes` + emits to listener via `onParseOpcode`.
- [`libs/sfizz/src/sfizz/parser/ParserListener.h`](../../libs/sfizz/src/sfizz/parser/ParserListener.h) defines the `onParseHeader` / `onParseOpcode` interface; concrete implementations are in `Region.cpp` + `Layer.cpp` + `SynthMessaging.cpp` (per grep).
- [`libs/sfizz/src/sfizz/Region.h:265-307`](../../libs/sfizz/src/sfizz/Region.h:265) `Region` struct: `group` (line 265, int64) + `sequenceLength` (line 306, uint8) + `sequencePosition` (line 307, uint8) — single fields per Region, NOT cascaded into separate global/master/group like amplitude/volume.
- [`libs/sfizz/src/sfizz/Region.h:341-346`](../../libs/sfizz/src/sfizz/Region.h:341): `globalAmplitude` / `masterAmplitude` / `groupAmplitude` / `globalVolume` / `masterVolume` / `groupVolume` — these ARE separate per-level fields on the Region. The asymmetry implies sfizz DOES inheritance for amplitude/volume (multi-level cascade preserved on the Region), but stores RR fields as a single per-region value (which means inheritance must happen at PARSE / Layer-building time, with the inherited value baked into the final Region).
- Layer.cpp is the likely state-builder for `<group>`-scoped opcode inheritance.

### Section 5 — BaySick* loader-handoff asymmetry (Track 3 investigation surface — MAJOR FINDING)

Three engines have DIFFERENT load paths:

- **BaySickRustyDrums** at [`BaySickRustyDrumsProcessor.cpp:485-504`](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:485) — calls `buildOutputRoutedSfzWrapper(sfzPath, kitRoot)` (declared at [`.h:202`](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h:202); defined at [`.cpp:623`](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:623)) which synthesizes a wrapper SFZ string that inlines every piece's `_map*.sfz` content + INJECTS `output=N` into every `<master>` and `<group>` line. Then calls `mSfizz->loadSfzString(sfzPath, wrapperText)`. Falls back to plain `mSfizz->loadSfzFile(sfzPath)` only if the wrapper synthesis returned empty OR loadSfzString failed (no per-strip routing in fallback). **Risk surface:** the wrapper synthesis REWRITES SFZ content — if it strips, reorders, or mis-injects around `<group>` boundaries, that could break RR opcode inheritance even if sfizz parses correctly.
- **BaySickGuitars** at [`BaySickGuitarsProcessor.cpp:255-261`](../../Source/BaySickGuitars/BaySickGuitarsProcessor.cpp:255) — plain `mSfizz->loadSfzFile(sfzPath.getFullPathName().toStdString())`. No wrapper synthesis.
- **BaySickBasses** at [`BaySickBassesProcessor.cpp:246-252`](../../Source/BaySickBasses/BaySickBassesProcessor.cpp:246) — plain `mSfizz->loadSfzFile(sfzPath.getFullPathName().toStdString())`. No wrapper synthesis.

**Track 3 investigation triage matrix** (locked at this inventory; reduces Task 3 search space):

- If Rusty shows RR loss but Guitars/Basses don't -> wrapper synthesis at `BaySickRustyDrumsProcessor.cpp:623` is the suspect (Rusty-specific loader-handoff bug).
- If Guitars/Basses show RR loss but Rusty doesn't -> sfizz state-builder gap (Region.cpp/Layer.cpp inheritance), distinct from Track 1's BaySickPlayer parser bug but symptomatically similar.
- If ALL THREE show RR loss -> sfizz state-builder gap (the wrapper synthesis isn't the only path).
- If NONE show RR loss -> Jeff's historical observation may be content-specific (file-content issue, not parser / loader).

### Section 6 — Core Library SFZ usage patterns

Sampled files for `<group>` / `<master>` / `<global>` usage:

- Tuba-KS.sfz: `<group>`-scoped `seq_length`/`seq_position` (RR cycling, Track 1 anchor).
- karoryfer big-rusty-drums `Programs/01-full.sfz` + `Programs/05-toms.sfz`: top-level files are `<control>`-only with `#include` directives + `label_ccN=` + `set_ccN=` opcodes; actual `<group>`/`<region>` content lives in `mappings/*/`.
- karoryfer mapping files (15+ files at `Programs/mappings/tom_22/*.sfz` + `Programs/mappings/tom_18/*.sfz`): use `<group>`-scoped `seq_length`/`seq_position` for RR variants on the percussion samples.
- karoryfer files use `#include` directives and `#define` variable expansion + ARIA dialect features (`label_ccN`, `set_ccN`, `set_hdcc`, etc.) — these are sfizz-engine territory (Track 2), out of BaySickPlayer's hand-rolled parser scope (no `#include` support).

**Out-of-scope finding (informational only, NOT folded into Track 1):** BaySickPlayer's hand-rolled `VibeSampleManager::parseSFZ` does NOT support `#include` / `#define` directives. So Karoryfer-style content (Tuba-KS-style content works fine — no #include) couldn't load through BaySickPlayer even if Sub-C 4-level inheritance lands. This is a pre-existing parser-coverage gap, NOT in QA-SfzGroup's 12-opcode lock. Route as Future State or a future BaySickPlayer-SFZ-coverage batch if surfaced as user-facing requirement.

### Section 7 — No new sub-spec calls discovered (Rule 5 compliance)

Inventory confirmed the 11 plan-finalize spec calls (Sub-A through Sub-K) all remain correct against actual code surface. No edge cases surfaced that would require new sub-spec call surface. Sub-K's helper-struct shape (function-local `SfzParseState` + `enterGlobal`/`enterMaster`/`enterGroup`/`enterRegion`/`enterControl`/`exitOther`/`tryFlushRegion` + `currentTarget()`) is viable against the actual parseSFZ structure — per-line dispatch becomes `auto* t = state.currentTarget(); if (! t) continue;` then all 12 opcode blocks write to `*t`.

### Diagnostic Instrumentation Catalog

Nil for Task 1 (read-only inventory, no source touched).

### Next action

Task 2 — Track 1 parser fix per Sub-K(c) helper-struct pattern. Edit `parseSFZ` function body at [`VibePlayerDSP.cpp:90-237`](../../Source/VibePlayer/VibePlayerDSP.cpp:90). After build clean, tell Jeff to verify per Sub-D scope (Tuba-KS staccato 4-variant RR cycling + Tuba-KS sustain regression + non-`<group>` SFZ regression + RR determinism).
