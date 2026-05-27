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

---

## 2026-05-27 — Task 2 — Track 1 parser fix + keyswitching + UI discoverability (scope expanded 2x)

Structural source-touch task per Sub-I (a) Task 2 scope. Started as the Sub-K(c) helper-struct rewrite of `VibeSampleManager::parseSFZ` covering the original 12-opcode set (Sub-B (a)) plus 4-level cascading inheritance (Sub-C (c)). Mid-task verify surfaced a latent divide-by-zero crash exposed by the parser fix (real pre-existing bug masked by the same bug the batch is fixing), which Jeff overruled the defensive-math fix options on in favor of expanding scope to full SFZ keyswitching — locking four new sub-spec calls (Sub-L / Sub-M / Sub-N / Sub-O) via chat per Rule 5. After keyswitching landed and verified, a second scope expansion added piano-roll UI discoverability (highlight + label keyswitch keys on the keyboard like BaySickRustyDrums shows kit names), locking two more sub-spec calls (Sub-P / Sub-Q). All three layers (parser refactor + keyswitching + UI) ship in Task 2 as a single rollback boundary per Sub-P (a) in-batch fold. Ten source files modified across `Source/VibePlayer/` + `Source/Standalone/`; diff total +473 / -56 net. Both Release + Debug build clean. Jeff-verified PASS across 10 scenarios on the consolidated checklist.

### Section 1 — Initial Sub-K helper-struct parser refactor (the locked Task 2 scope)

Per Sub-K (c) the parseSFZ function body at [`VibePlayerDSP.cpp:90-237`](../../Source/VibePlayer/VibePlayerDSP.cpp:90) was rewritten around a function-local state machine. New surface:

- `enum class Scope { None, Global, Master, Group, Region };` — current header-block context.
- `struct SfzParseState` — function-local helper holding four cascading-default `VibeRegion` accumulators (`globalDefaults` / `masterDefaults` / `groupDefaults` / `current`) + an `inControl` flag + the current `Scope`.
- Six enter-scope methods (`enterGlobal()` / `enterMaster()` / `enterGroup()` / `enterRegion()` / `enterControl()` / `exitOther()`) that wire the cascading copy chain per the SFZ v1 spec: enterMaster copies `globalDefaults` → `masterDefaults`; enterGroup copies `masterDefaults` → `groupDefaults`; enterRegion copies `groupDefaults` → `current` as the region's baseline. `<control>` block stays orthogonal (default_path scope only, `Scope = None` + `inControl = true`).
- `tryFlushRegion(regions)` — pushes `current` into the `regions` vector when transitioning out of a region scope, sets `current` back to clean state.
- `currentTarget()` — returns the right `VibeRegion*` by scope (Global → `&globalDefaults`, Master → `&masterDefaults`, Group → `&groupDefaults`, Region → `&current`, None → `nullptr`).

Per-line dispatch is now `auto* t = state.currentTarget(); if (! t) continue;` then every opcode block writes to `*t`. Sub-B (a) original 12 opcodes flow through unchanged. The pre-batch early-return `if (!inRegion) continue;` at [`:148`](../../Source/VibePlayer/VibePlayerDSP.cpp:148) — the original §9 thirty-eighth-Forks-diagnosed bug — is GONE; replaced by `auto* t = state.currentTarget(); if (! t) continue;` which now correctly routes Group / Master / Global opcodes into their respective accumulators instead of dropping them.

SfzParseState gained four explicit-tracking fields for the Sub-O scope-priority init (added later in Section 3 once keyswitching scope landed): `explicitGlobalSwDefault` / `explicitMasterSwDefault` / `explicitGroupSwDefault` / `firstRegionSwDefault` + `captureExplicitSwDefault(int)` + `getInitialSwLast() const` accessors. End-of-parseSFZ resolves the active starting keyswitch via the priority chain.

### Section 2 — Latent crash exposure mid-Task-2 verify (the load-bearing finding that drove the first scope expansion)

First Task 2 verify attempt (post-initial-edit, pre-keyswitching) crashed at [`VibePlayerDSP.cpp:520`](../../Source/VibePlayer/VibePlayerDSP.cpp:520) on the very first pitched-key press of Tuba-KS.sfz. Stack trace traced to `findRegion:520`'s `counter = (counter % rrTotal) + 1;` with `rrTotal == 0`.

Diagnosis per `feedback_diagnose_before_fixing.md` — not a defensive-handwave: a latent divide-by-zero existed pre-batch but was MASKED because parseSFZ dropped every `<group>`-scoped opcode → no region ever had `roundRobinTotal > 0` → `hasRR` was always false → line 510 early-returned → `:520` was unreachable. Track 1 parser fix correctly populated `roundRobinTotal=4` on the 4 staccato regions (via inheritance from each `<group>`'s `seq_length=4`) but sustain regions still had `roundRobinTotal=0`. Tuba-KS.sfz's sustain regions and staccato regions OVERLAP in pitch range — both cover the full playing range, differentiated only by SFZ keyswitching which the pre-Task-2 parser did not implement. So a single pitched key now matched BOTH a sustain candidate (RR-less) AND a staccato candidate (RR-marked). Sustain regions load first into the candidate list → `candidates[0]` was a sustain region with `roundRobinTotal=0` → `findRegion:519` read 0 from `candidates[0].roundRobinTotal` → `:520` divided by zero → crash.

Surfaced to Jeff with 4 fix options (defensive guard at :519 / defensive guard at :520 / skip-non-RR-candidates filter / expand scope to full keyswitching). Per `feedback_dont_make_unilateral_spec_calls.md`, none picked unilaterally — went to Jeff.

### Section 3 — First scope expansion 2026-05-27: keyswitching (Sub-L / Sub-M / Sub-N / Sub-O locks)

Jeff overruled the three defensive-math fix options + expanded scope to full keyswitching implementation. Rationale verbatim: "The goal of this batch is to fix the SFZ parser so these sample libraries actually play correctly. Leaving keyswitching out means the parser is still functionally broken for pro content, and I am not splitting the core SFZ fixes across multiple batches."

Four sub-spec calls surfaced via chat per Rule 5 + answered:

- **Sub-L = (b)** — 6 new opcodes: `sw_lokey` + `sw_hikey` + `sw_last` + `sw_down` + `sw_up` + `sw_default`. Engine uses explicit `sw_lokey..sw_hikey` range for keyswitch detection (strict spec compliance, not union-of-values inference). Sub-B amended 12 → 18 opcodes; plan file Spec-calls table updated.
- **Sub-M = (c)** — Keyswitch interception at `VibePlayerProcessor::processBlock`. After audition events, walk the MIDI buffer; for any note-on / note-off where `mgr.isKeyswitchNote(note)` is true, update manager state + STRIP the event from the buffer. `VibeSynth` only ever sees playable notes — no voice-stealing trigger from keyswitch presses, no wasted findFreeVoice cycles, no `<region>`-match attempts on keyswitch notes. Scratch buffer `mKeyswitchFilteredMidi` member added to `VibePlayerProcessor` to avoid per-block allocation.
- **Sub-N = (a)** — Keyswitch state lives on `VibeSampleManager`. Members: `int mActiveSwLast { -1 }` + `std::array<bool, 128> mSwDownHeld {}` + `std::array<bool, 128> mIsKeyswitch {}`. Public methods: `isKeyswitchNote(int) const noexcept` + `handleKeyswitchNoteOn(int) noexcept` + `handleKeyswitchNoteOff(int) noexcept`. Single-threaded (audio thread only — same thread that calls `findRegion`); cache-friendly because findRegion already lives there.
- **Sub-O = (c)** — Scope-priority `sw_default` init. SfzParseState's 4 explicit-tracking fields (Section 1 above) capture per-scope `sw_default` writes during parse. At end of parseSFZ: `mActiveSwLast = state.getInitialSwLast()` resolves the Global > Master > Group > first-Region priority chain (first-wins per scope; higher-scope override wins overall). Matches the SFZ v1 spec's "earliest enclosing scope wins" inheritance for the initial keyswitch state.

### Section 4 — Keyswitching implementation (the architectural payoff that also closes the latent div-by-zero)

parseSFZ end-of-function gained population of `mIsKeyswitch[]` from the union of `swLokey..swHikey` ranges across regions (every note in any region's keyswitch range is marked as a keyswitch note — used by `isKeyswitchNote()` to identify intercept-targets in Sub-M's pre-scan). Then init of `mActiveSwLast` via the Sub-O scope-priority chain. `clear()` now calls `resetKeyswitchState()` as part of the lifecycle reset so re-loading or engine swap-out doesn't leave stale keyswitch state.

`findRegion` candidate loop gained 3 new keyswitch filter conditions BEFORE the existing note / velocity / articulationGroup checks:

- `if (r.swLast != -1 && mActiveSwLast != r.swLast) continue;`
- `if (r.swDown != -1 && ! mSwDownHeld[r.swDown]) continue;`
- `if (r.swUp != -1 && mSwDownHeld[r.swUp]) continue;`

**Natural divide-by-zero fix (Sub-L/M/N/O architectural payoff):** keyswitching now isolates sustain candidates from staccato candidates BEFORE the RR logic runs at `findRegion:519-520`. A mixed candidate pool can't form — when the active keyswitch is C6 (sustain), staccato regions filter out via `swLast` mismatch; when active is C#6 (staccato), sustain regions filter out. The latent `findRegion:520` divide-by-zero is structurally resolved by the architectural shape, not by a defensive `if (rrTotal == 0)` guard in `findRegion`. No defensive math added.

`VibePlayerProcessor::processBlock` gained the Sub-M pre-scan loop between the audition-events block and `mSynth.renderNextBlock`. Walks the incoming MIDI buffer once; for each event, queries `mgr.isKeyswitchNote(note)`; if true, calls `mgr.handleKeyswitchNoteOn(note)` or `mgr.handleKeyswitchNoteOff(note)` based on event type + skips appending to `mKeyswitchFilteredMidi`. Non-keyswitch events copy through to `mKeyswitchFilteredMidi`. Final dispatch uses the filtered buffer.

### Section 5 — Convention clarification mid-verify 2026-05-27 (octave naming, no source change)

First post-keyswitching verify attempt: Jeff reported he couldn't engage the keyswitch — verbatim "f2 to d5 is the range on the piano roll for the tuba where notes are actually produced for me so I'm not sure where the c7 comes in as that does nothing."

Diagnosis: octave-naming convention mismatch between the SFZ file and the BaySickDAW UI. SFZ file uses **IPN convention** (C4 = MIDI 60) per the file's literal `sw_default=c6` → our parser correctly resolves to MIDI 84. BaySickDAW UI uses **FL Studio convention** (C5 = MIDI 60) — confirmed by direct read at [`Source/Standalone/DrumKitGrid.cpp:17`](../../Source/Standalone/DrumKitGrid.cpp:17) where the C-octave display calculation matches FL's naming. So the file's `c6` / `c#6` (MIDI 84 / 85) displays in the app keyboard as `C7` / `C#7` — two-octave offset.

After the verify scenarios were re-stated with FL-convention notation (press app-C#7 / app-C7 to toggle articulation), Jeff confirmed: pressing app-C#7 → pitched notes play staccato (post-C#7) — different timbre from default sustain. Pressing app-C7 → back to sustain. Keyswitching CONFIRMED WORKING. No source change from this convention check — the FL display behavior is correct + the SFZ IPN parse is correct; documenting here so future verify scripts use FL-convention notation for BaySickPlayer.

### Section 6 — Second scope expansion 2026-05-27: UI discoverability (Sub-P / Sub-Q locks)

Post-keyswitching verify, Jeff requested keyswitch labels on the piano keyboard — the way BaySickRustyDrums shows "Snare Center" / "Hi-hat Tight Closed" labels on its kit-keymap keys. Without the labels, the user has no way to discover which keys are keyswitches vs playable notes; the feature is functionally present but invisibly so.

Two sub-spec calls surfaced via chat per Rule 5 + answered:

- **Sub-P = (a)** — In-batch fold. Keyswitch UI lands in QA-SfzGroup Task 2 as part of the same rollback boundary. Rationale: "fix the parser COMPLETELY + show users the keyswitches exist" is one coherent deliverable; splitting UI into a follow-up batch would ship a keyswitching feature that no user can find. Sub-A's Track 2 investigation-only lock STILL holds — Sub-P's UI applies to BaySickPlayer only; the 3 sfizz-driven engines (BaySickRustyDrums / BaySickGuitars / BaySickBasses) get UI labels in the Track 2 follow-up batch (the one that opens after Task 3's investigation surfaces the root cause).
- **Sub-Q = (c)** — Both highlight + label. Requires a 19th opcode `sw_label` which Tuba-KS.sfz already provides (`sw_label="C6 Sustain"` at the sustain group, `sw_label="C#6 Staccato"` at each staccato group). New `juce::String swLabel` field on VibeRegion. New `std::array<juce::String, 128> mKeyswitchLabels` member on VibeSampleManager + `getKeyswitchLabel(int) const noexcept` public method. UI consumes the label string for both keyboard rendering + hover tooltip.

Sub-B amended 18 → 19 opcodes; plan file Spec-calls table updated.

### Section 7 — UI implementation in 3 phases (parser / manager → piano-roll plumbing → per-engine wiring)

**Phase A: parser + manager.** Added the `sw_label` opcode handler (`readToEOL=true` since label values contain spaces — same pattern as `sample`). New `swLabel` field on VibeRegion. New `mKeyswitchLabels[128]` member on VibeSampleManager. End-of-parseSFZ population: walk regions, for each region with a non-empty `swLabel`, assign the label to whichever `swLast` / `swDown` / `swUp` key the region declares (first-wins per keyswitch note — earliest region's label is what shows). `getKeyswitchLabel(int)` returns the cached string or empty if no label. `resetKeyswitchState()` clears labels too.

**Phase B: piano-roll plumbing.** Added a `std::function<juce::String(int)> keyswitchLabelProvider` field to `PianoRollConnection` ([`Source/Standalone/PianoRollPage.h`](../../Source/Standalone/PianoRollPage.h)). Wired `roll->setKeyswitchLabelProvider(...)` in `PianoRollPage::registerEngine` so the closure passes through from page-registration time. Added a matching `setKeyswitchLabelProvider` setter on `PianoKeyboard` + `PianoRollContainer` ([`Source/Standalone/PianoRoll.h`](../../Source/Standalone/PianoRoll.h)) — the container delegates to the keyboard. `PianoKeyboard::paint` gained a keyswitch-key branch at the TOP of the per-note loop (BEFORE the existing black / white key logic) — keyswitch keys render as **full-width amber background** (resting `Colour(0xffe8c060)`, brighter when previewed via current note tracking `Colour(0xfff5d690)`) + **bold dark-amber label text** drawn over the key body (font size scaled to row height so the label always fits the visible key). `getTooltip` updated to surface the keyswitch label string on hover (priority over the regular `noteLabelProvider` so kit-name systems don't collide if both happen to be set — won't happen in practice since only BaySickPlayer sets the keyswitch provider, but the priority is defensive against future cross-pollination).

**Phase C: closures wired in 4 piano-roll registration sites.** Each site adds a closure that does a `dynamic_cast<VibePlayerProcessor*>` guard on the registered engine processor (via the existing `cast` lambda variable already in scope at each site) and returns `v->getSynth().getManager().getKeyswitchLabel(n)` on success or empty string on cast failure.  Wired at the Layer / Bass / Drum / Clip registration sites in [`Source/Standalone/StandaloneEditor.cpp`](../../Source/Standalone/StandaloneEditor.cpp) (4 sites — same registration cluster as the existing `auditionMomentary` + `noteLabelProvider` wires).  Non-BaySickPlayer engines (Harmless / BaySickSynth / BaySickBass) hit the `dynamic_cast` fail path → return empty → existing piano-keyboard behavior preserved unchanged.  BaySickRustyDrums uses its existing `noteLabelProvider` (drum kit names) which is unaffected by the new keyswitch provider — Sub-A's Track 2 investigation-only lock means RustyDrums won't have a keyswitch provider until the follow-up batch lands.

### Section 8 — Verify pass 2026-05-27 (Jeff, all 10 scenarios PASS)

Debug + Release across the consolidated 10-scenario checklist per Sub-D scope + Sub-H Debug-then-Release cadence:

- **(1)** Both Release + Debug build clean — only pre-existing warnings.
- **(2)** No crash on any pitched key press post-Tuba-KS-load — the latent `findRegion:520` div-by-zero is structurally resolved by the keyswitching filter; verified across the full f2-d5 playing range Jeff reported plus edge keys.
- **(3)** Tuba-KS staccato 4-variant RR cycling deterministic — repeated key strikes on a staccato note (with app-C#7 keyswitch active) cycle through the 4 RR variants in order; same key, same sequence on every repeat.
- **(4)** Keyswitching state machine — app-C7 (file `c6`) sets sustain; app-C#7 (file `c#6`) sets staccato. Pressing either keyswitch flips the active articulation for subsequent pitched notes.
- **(5)** UI labels visible — amber-background keys at app-C7 + app-C#7 with bold dark-amber "C6 Sustain" / "C#6 Staccato" text. Tooltip on hover surfaces the same label string.
- **(6)** Regular keys unchanged — black + white keys + the C-octave markers at app-C5 etc. render identically to pre-batch (no amber bleed-through, no font changes).
- **(7)** Non-keyswitch SFZ regression — load a plain single-articulation SFZ (no `sw_*` opcodes); no amber rendering on any key; plays identically to pre-batch with correct region selection.
- **(8)** Non-SFZ paths (`loadFolder` / `loadSingleFile`) regression — load via folder browser / single WAV; no amber, no crash, plays normally. The keyswitch state is initialized to harmless defaults via the manager's value-init.
- **(9)** BaySickRustyDrums drum-kit name labels render unchanged — load a kit, verify "Snare Center" / etc. labels still appear on the kit-keymap keys via the existing `noteLabelProvider`. Sub-P's BaySickPlayer-only scope holds; no regression on the other engine's label system.
- **(10)** Harmless / BaySickSynth / BaySickBass piano keyboards show no amber + no false positives — the closure's `dynamic_cast` fail path correctly returns empty for non-BaySickPlayer engines.

MT / serial parity confirmed via Mixer hamburger toggle — identical behavior across both transport-thread settings (per `project_mt_engine_works_in_debug.md` MT-works-in-Debug fact, no Release-only verify needed).

### Section 9 — Sub-spec calls discovered + resolved this task (Rule 5 compliance)

Sub-L / Sub-M / Sub-N / Sub-O surfaced via chat at the first scope expansion (keyswitching) + answered before any source touch. Sub-P / Sub-Q surfaced via chat at the second scope expansion (UI discoverability) + answered. No unilateral picks. Plan file Spec-calls table amended 2x during the task: Sub-B amended 12 → 18 opcodes at the keyswitching surface, then amended 18 → 19 opcodes at the UI surface (`sw_label`). Plan body Task 2 scope description amended to reflect the as-shipped 3-layer shape.

### Section 10 — Diagnostic Instrumentation Catalog

Nil for Task 2 (no `DBG` / `juce::Logger::writeToLog` / temp `jassert` / debug `juce::AlertWindow` / temp file logging added during the task — all 3 design pivots and the latent-crash diagnosis resolved via static analysis + Jeff's verify rounds + the existing crash stack trace).

### Section 11 — Next action

Task 3 — Track 2 sfizz parser + BaySick* loader-handoff investigation (BaySickRustyDrums / BaySickGuitars / BaySickBasses). Read-only per Sub-A (a) Track 2 investigation-only lock. Doc-only commit at Task 3 end per Sub-J (a) clean-save-point lock. The MAJOR Track 3 finding from Task 1 inventory (BaySickRustyDrums' wrapper-synthesis at [`BaySickRustyDrumsProcessor.cpp:623`](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:623) REWRITES SFZ content via `buildOutputRoutedSfzWrapper` vs Guitars / Basses' plain `loadSfzFile` with no wrapper synthesis) gates the Track 3 triage matrix per the Task 1 entry's Section 5 — first determine which engines show the RR-loss symptom, then the matrix collapses the search space to either the wrapper-synthesis path (Rusty-specific) or the sfizz Region.cpp / Layer.cpp inheritance state-builder (cross-engine).

### Section 12 — Files modified summary

- [`Source/VibePlayer/VibePlayerDSP.h`](../../Source/VibePlayer/VibePlayerDSP.h) — +39: VibeRegion gained 6 keyswitch numeric fields + `swLabel`; VibeSampleManager gained 3 keyswitch state members + 4 new public methods (`isKeyswitchNote` / `handleKeyswitchNoteOn` / `handleKeyswitchNoteOff` / `getKeyswitchLabel`) + `resetKeyswitchState()`.
- [`Source/VibePlayer/VibePlayerDSP.cpp`](../../Source/VibePlayer/VibePlayerDSP.cpp) — +329 / -49: SfzParseState struct + scope-enter methods + per-line opcode dispatch + end-of-parseSFZ keyswitch population + findRegion keyswitch filter + `clear()` reset hook + 4 new method bodies on VibeSampleManager.
- [`Source/VibePlayer/VibePlayerProcessor.h`](../../Source/VibePlayer/VibePlayerProcessor.h) — +6: `mKeyswitchFilteredMidi` scratch buffer member.
- [`Source/VibePlayer/VibePlayerProcessor.cpp`](../../Source/VibePlayer/VibePlayerProcessor.cpp) — +31: Sub-M pre-scan loop in `processBlock` between audition events and renderNextBlock.
- [`Source/Standalone/PianoRoll.h`](../../Source/Standalone/PianoRoll.h) — +10: `setKeyswitchLabelProvider` declarations on PianoKeyboard + PianoRollContainer.
- [`Source/Standalone/PianoRoll.cpp`](../../Source/Standalone/PianoRoll.cpp) — +48: amber keyswitch-key paint branch + tooltip priority update + 2 setter impls.
- [`Source/Standalone/PianoRollPage.h`](../../Source/Standalone/PianoRollPage.h) — +8: `keyswitchLabelProvider` field on PianoRollConnection.
- [`Source/Standalone/PianoRollPage.cpp`](../../Source/Standalone/PianoRollPage.cpp) — +1: `roll->setKeyswitchLabelProvider(...)` wire in `registerEngine`.
- [`Source/Standalone/StandaloneEditor.cpp`](../../Source/Standalone/StandaloneEditor.cpp) — +30: 4 piano-roll registration-site closures (Layer / Bass / Drum / Clip).

Total: 10 source files, +473 / -56 net.

---

## 2026-05-27 — Task 3 — Track 2 sfizz investigation + targeted sfizz atomic patch (Sub-R/S amendment)

Read-only investigation per Sub-A (Track 2 investigation-only) that surfaced two unforeseen findings mid-task: (1) the BaySickRustyDrums MT-mode bit-crusher symptom on cymbals/hi-hats, which Jeff flagged as catastrophic + in-scope for QA-SfzGroup's RR mandate; (2) a discoverability blocker on BaySickGuitars + BaySickBasses preventing meaningful in-app RR verification.  Triggered a Sub-A scope amendment (Sub-R locked: targeted vendored-sfizz patches allowed when the fix is bounded; Sub-S locked: `std::memory_order_relaxed`) + a 1-symbol-type-change patch landed in `libs/sfizz/src/sfizz/Layer.{h,cpp}` to convert `int sequenceCounter_` to `std::atomic<int>` with `fetch_add(1, relaxed)` at both call sites.  Patch is a real correctness improvement (plain-int RMW under MT is UB) + ships as defense-in-depth, but **did NOT resolve the bit-crusher symptom** — empirical verification confirmed the patch is in the binary (clean rebuild after deleting `build/sfizz_build/`) but Jeff still hears bit-crusher under MT-on Rusty cymbals.  Hypothesis-was-wrong outcome owned per `feedback_diagnose_before_fixing.md` + `feedback_own_the_codebase_no_git_alibi.md`; per Jeff's call (verbatim "We are going with Option (b): Keep the atomic patch + route the deep investigation to the follow-up batch"), the actual root cause + fix routes to the Track 2 follow-up batch alongside discoverability + Guitars/Basses RR-loss investigation.

### Section 1 — sfizz parser/state-builder code review — INHERITANCE WORKING

Via Explore agent trace (read-only): sfizz correctly cascades `<group>`-scoped `seq_position` / `seq_length` opcodes into child `<region>` blocks.  Code path:

- [`libs/sfizz/src/sfizz/parser/Parser.cpp`](../../libs/sfizz/src/sfizz/parser/Parser.cpp) `processHeader()` (line 218) + `processOpcode()` (line 257) tokenize SFZ + emit `onParseFullBlock` listener events.
- [`libs/sfizz/src/sfizz/Synth.cpp`](../../libs/sfizz/src/sfizz/Synth.cpp) `Synth::Impl::onParseFullBlock()` (lines 91-142) caches opcodes per-scope: `globalOpcodes_ = members` (:104), `masterOpcodes_ = members` (:115), `groupOpcodes_ = members` (:122).
- [`libs/sfizz/src/sfizz/Synth.cpp:156-180`](../../libs/sfizz/src/sfizz/Synth.cpp:156) `Synth::Impl::buildRegion()` performs the **4-layer parseOpcodes cascade** (global → master → group → region; lines 177-180 in order) on each `<region>` header.  Each parseOpcodes call writes opcodes into the region's struct fields via `Region::parseOpcode()`.
- [`libs/sfizz/src/sfizz/Region.cpp:376-381`](../../libs/sfizz/src/sfizz/Region.cpp:376) `Region::parseOpcode()` `case hash("seq_length")` + `case hash("seq_position")` assign directly to `sequenceLength` + `sequencePosition` (single per-Region uint8 fields per [`Region.h:306-307`](../../libs/sfizz/src/sfizz/Region.h:306)).
- [`libs/sfizz/src/sfizz/Layer.cpp:60-61`](../../libs/sfizz/src/sfizz/Layer.cpp:60) `Layer::registerNoteOn()` runtime activation reads the inherited values via `sequenceSwitched_ = ((sequenceCounter_++ % region.sequenceLength) == region.sequencePosition - 1);`.

Validation: sfizz explicitly collects `groupOpcodes_` (Synth.cpp:122) and replays them during every `buildRegion()` call (Synth.cpp:179) — the parse order ensures region-scoped opcodes correctly override group defaults.  **No equivalent of BaySickPlayer's pre-batch `if (!inRegion) continue;` gap in sfizz.**

### Section 2 — BaySickRustyDrums wrapper synthesis review — opcode-preserving

[`Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:485-504`](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:485) `loadKit()` calls `buildOutputRoutedSfzWrapper(sfzPath, kitRoot)` at line 492 (declared at `.h:202`; defined at [`.cpp:623`](../../Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:623)) which:

- Walks every line of the top-level SFZ; for `<master>` / `<group>` headers, APPENDS `output=N` to the same line (line 670 in `annotateMastersWithOutput()`, line 692 in the top-level walk).  Does NOT strip, reorder, or modify any other opcode lines.
- Inlines `#include` piece-file content via `annotateMastersWithOutput()` (line 720) which applies the same header-append-only treatment.
- Passes all other content (control blocks, opcodes, comments, blank lines) through unchanged at line 738.

The resulting wrapper string preserves every opcode including `<group>`-scoped `seq_position` / `seq_length`.  Wrapper-synthesized SFZ is parsed by sfizz via `loadSfzString()` (line 497) using the same parser as the plain `loadSfzFile()` path.  **Verdict: wrapper synthesis is correct + opcode-preserving; not the source of any RR or articulation bug.**

### Section 3 — BaySickGuitars + BaySickBasses loader-handoff confirmed asymmetric but both correct

[`Source/BaySickGuitars/BaySickGuitarsProcessor.cpp:255-261`](../../Source/BaySickGuitars/BaySickGuitarsProcessor.cpp:255) `loadKit()` uses plain `mSfizz->loadSfzFile(sfzPath.getFullPathName().toStdString())` — no wrapper synthesis, no opcode transformation.  [`Source/BaySickBasses/BaySickBassesProcessor.cpp:246-252`](../../Source/BaySickBasses/BaySickBassesProcessor.cpp:246) `loadKit()` identical pattern.  Both engines hand the on-disk SFZ file directly to sfizz; sfizz's inheritance cascade (Section 1) handles opcode propagation correctly.

### Section 4 — CoreLibrary content sample confirmation (sfizz-driven engines DO have RR + keyswitches)

Sampled the actual content files on disk:

- **Black&Blue Basses** at `AppData/Local/BaySickDAW/CoreLibrary/Black&Blue Basses/Programs/` — 11 numbered `.sfz` programs.  `01-darkblack_keysw.sfz` + `02-darkblack_keysw_warm.sfz` declare keyswitches at `<global>` scope (`sw_lokey=27 sw_hikey=31 sw_default=31` = MIDI 27-31 = app-D#3 to G3 in FL convention).  Multiple `<master>` blocks for sw_last=27/28/29/30/31 with `sw_label=Pluck` / `sw_label=Ghost` / etc.  Map files (`maps/babyblue_fake_det_ff_map.sfz` + others) contain **49 per-region `seq_position` declarations** with up to 8-way RR per pitched note.
- **Black&Green Guitars** at `AppData/Local/BaySickDAW/CoreLibrary/Black&Green Guitars/Programs/` — 3 keyswitch programs (`01-green_keyswitch.sfz` / `02-black_keyswitch.sfz` / `03-combo_keyswitch.sfz`) + 8 single-articulation programs.  Map files (`modules/maps_black/btb.sfz` + others) contain **33+ per-region `seq_position` declarations** with 2-way RR per pitched note.

**Conclusion:** content fully RR-equipped + keyswitch-equipped on disk.  No file-content issue.

### Section 5 — Live test results (Jeff, 2026-05-27)

- **Rusty (initial test, pre-Sub-R/S patch):** RR variation HEARD on snare-style hits.  Jeff confirmed audible cycling.
- **Guitars / Basses (initial test):** NO RR variation heard.  Jeff also reported "Guitars and Basses have keyswitch setups but i have no idea where those buttons are and they aren't labeled" — confirming the discoverability blocker (Sub-A locked sfizz-engine keyswitch labels out of Sub-P/Q's scope, so those engines have no amber-key UI).
- **Bit-crusher discovery (mid-test on Rusty cymbals + hi-hats):** Jeff reported "almost seems like it might be playing multiple files at once crashing into each other" + diagnostic answers locked the pattern: MT-only, scales with RR variant count (cymbals/hi-hats with 6-8 RR slots affected; kick/snare with fewer slots clean), single-hit reproducible.  Symptom = textbook race-on-RR-counter signature.

### Section 6 — Sub-R/S scope amendment + atomic patch applied (diagnostic miss)

Per `feedback_qa_batches_fix_bugs_dont_defer.md` (real bugs surfaced mid-batch get fixed in-batch by default) + Jeff verbatim 2026-05-27 ("A targeted, 1-line type change to fix a thread-safety RR bug absolutely belongs in this QA-SfzGroup batch"), two sub-spec calls surfaced via chat per Rule 5 + answered:

- **Sub-R = (a)** — Sub-A scope amendment: targeted vendored-sfizz patches ALLOWED in QA-SfzGroup when the fix is bounded (single-symbol type change, no state-machine logic rewrite).  Spirit of Sub-A's "no full sfizz state-machine patching mixed with parser work" preserved.
- **Sub-S = (a)** — `std::memory_order_relaxed` for the `sequenceCounter_` atomic increment (monotonic counter, not synchronizing other state); call sites use `sequenceCounter_.fetch_add(1, std::memory_order_relaxed)` explicitly (NOT `sequenceCounter_++` which on atomic defaults to `seq_cst`).

Patch applied:

- [`libs/sfizz/src/sfizz/Layer.h:11`](../../libs/sfizz/src/sfizz/Layer.h:11) — added `#include <atomic>`.
- [`libs/sfizz/src/sfizz/Layer.h:151`](../../libs/sfizz/src/sfizz/Layer.h:151) — `int sequenceCounter_ { 0 };` → `std::atomic<int> sequenceCounter_ { 0 };` + QA-SfzGroup local-patch comment.
- [`libs/sfizz/src/sfizz/Layer.cpp:63`](../../libs/sfizz/src/sfizz/Layer.cpp:63) (registerNoteOn) — `sequenceCounter_++` → `const int counter = sequenceCounter_.fetch_add(1, std::memory_order_relaxed);` + modulo check against `counter` local.
- [`libs/sfizz/src/sfizz/Layer.cpp:191`](../../libs/sfizz/src/sfizz/Layer.cpp:191) (registerCC) — identical pattern.

**Empirical verification (Jeff, 2026-05-27):**

- Initial post-patch test: Jeff reported "All pass" — but later clarified he was testing under MT-OFF (he thought MT was on but it was off).  MT-off has always been clean; that wasn't a meaningful patch-effect verification.
- Bass program transient artifact: Jeff observed a "tiny plucked note" on `01-darkblack_keysw` post-patch that he attributed to my work — diagnostic check via MT-off revealed it as a one-off glitch (not reproducible); no source change connection.
- Clean rebuild verification (deleted `build/sfizz_build/` + ran `do_build.bat`): patch IS compiled into the binary, but **the bit-crusher symptom UNCHANGED under MT-on Rusty cymbals**.  Patch did not fix the symptom.

**Diagnostic conclusion:** the hypothesis that `sequenceCounter_` was the race source was wrong.  The atomic patch removes a real data race (plain-int RMW under MT is UB per C++ spec) + improves codebase correctness as defense-in-depth, but the actual MT-only bit-crusher root cause is elsewhere in sfizz (likely voice allocation / dispatch / sample buffer access — areas I haven't investigated yet).  Per `feedback_own_the_codebase_no_git_alibi.md`, miss owned.

### Section 7 — Three findings routed to Track 2 follow-up batch

Per Jeff's verbatim "We are going with Option (b): Keep the atomic patch + route the deep investigation to the follow-up batch" + "I am not letting a deep-dive MT hunt into a vendored library derail our current close-out trajectory", three sfizz-engine findings route to a new dedicated batch (slot is Jeff's call at QA-SfzGroup close per `feedback_slot_placement_is_spec_call.md`).  Working title for the routing recommendation: tentative `QA-SfizzFollowup` (final naming + slot at close).

**Routing-recommended scope for the follow-up batch:**

1. **Keyswitch label discoverability for sfizz-driven engines** (BaySickRustyDrums + BaySickGuitars + BaySickBasses).  Extend Sub-P/Q's amber-highlight + label rendering to those engines' piano keyboards.  Requires extracting keyswitch labels from sfizz's parsed Region/Layer data structures (the parsed `sw_label` opcode + `sw_lokey..sw_hikey` range data are accessible via sfizz's existing public API once a kit is loaded).  Wire the existing `PianoRollConnection::keyswitchLabelProvider` field (already added in Task 2) at the BaySickRustyDrums + BaySickGuitars + BaySickBasses piano-roll registration sites in `StandaloneEditor.cpp` with closures that pull labels from each engine's `mSfizz` instance.  Estimated effort: medium (~2-3 hr — sfizz API navigation + closure plumbing).
2. **Guitars/Basses RR-loss diagnosis** — most likely resolved by item 1 (Jeff can engage articulations + test at consistent velocity once labels are visible).  If RR still doesn't audibly cycle post-discoverability-fix, dive deeper (sfizz `Default::sequence` value semantics, sample-data inspection, etc.).  Estimated effort: small (~1-2 hr) IF item 1 resolves it; medium (~3-4 hr) if separate fix needed.
3. **Bit-crusher MT race fix** — the catastrophic bug.  Atomic patch in `libs/sfizz/Layer.{h,cpp}` is in place as defense-in-depth but does NOT resolve the symptom.  Actual root cause needs investigation: instrumentation via file-logging (since DBG / OutputDebugString is invisible in Release without DebugView), capture per-voice / per-Layer / per-noteOn state during a multi-voice cymbal hit, identify the racing state (candidates: voice allocation in `Synth::Impl::startVoice` at `Synth.cpp:1300`, voice manager state, sample buffer concurrent access, polyphony manager, or some other non-atomic shared state in `Voice.cpp` / `RegionStateful.cpp`).  Estimated effort: medium-large (~3-6 hr investigation + 1-2 hr fix).

**Total estimated effort for the follow-up batch:** ~6-12 hours.

### Section 8 — Sub-spec calls discovered + resolved this task (Rule 5 compliance)

Sub-R + Sub-S surfaced via chat at the mid-Task-3 bit-crusher discovery + answered before any source touch.  No unilateral picks.  Plan file Spec-calls table amended once during the task: Sub-R + Sub-S rows added (Sub-A is amended in-place via Sub-R's text rather than re-edited).  Plan body Task 2 scope description was NOT updated for Sub-R/S since the patch landed during Task 3 — Sub-R/S table rows are the canonical record.

### Section 9 — Diagnostic Instrumentation Catalog

Nil for Task 3 (no `DBG` / `juce::Logger::writeToLog` / temp `jassert` / debug `juce::AlertWindow` / temp file logging added during the task — the atomic patch is a real fix attempt + ships as defense-in-depth, NOT diagnostic instrumentation).

### Section 10 — Next action

Task 4 — cleanup grep sweep per Sub-I (a) 6-task lock.  Sweep `Source/VibePlayer/VibePlayerDSP.{cpp,h}` + broader `Source/` for stale comments referencing pre-batch broken-inheritance behavior; verify zero diagnostic instrumentation in source post-batch (Rule 4 catalog should remain empty); apply any cleanup edits + commit if changes (else skip commit, fold cleanup result into Task 5 close commit).

### Section 11 — Files modified summary

- [`libs/sfizz/src/sfizz/Layer.h`](../../libs/sfizz/src/sfizz/Layer.h) — +9 / -1 (Sub-R/S patch: `<atomic>` include + `std::atomic<int>` type change for `sequenceCounter_` + QA-SfzGroup local-patch comment).
- [`libs/sfizz/src/sfizz/Layer.cpp`](../../libs/sfizz/src/sfizz/Layer.cpp) — +10 / -2 (Sub-R/S patch: `fetch_add(1, std::memory_order_relaxed)` at both call sites in `registerNoteOn` + `registerCC` + QA-SfzGroup local-patch comments).
- [`Plans & Specs/Batch Plans/magical-petting-dijkstra.md`](../Batch%20Plans/magical-petting-dijkstra.md) — Sub-R + Sub-S rows added to Spec-calls table (amends Sub-A scope, locks `std::memory_order_relaxed` semantics + call-site `fetch_add` requirement).
- [`Plans & Specs/Running Notes/magical-petting-dijkstra.md`](magical-petting-dijkstra.md) — this Task 3 entry appended.

Total source: 2 vendored sfizz files, +19 / -3 net.  Total docs: 2 plan/running-notes files.

---

## Task 4 — Cleanup grep sweep (no-op outcome)

Sub-I(a) 6-task lock dictated a cleanup grep sweep at Task 4.  Goals:

1. Sweep for stale comments referencing pre-batch broken-inheritance behavior anywhere in the SFZ loading path.
2. Verify zero diagnostic instrumentation in source post-batch (Rule 4 catalog should remain empty).
3. Apply any cleanup edits + commit if changes (else skip commit, fold cleanup result into Task 5 close commit).

### Section 1 — Sweep outcome: NO source edits required

`Source/VibePlayer/VibePlayerDSP.{cpp,h}` — read post-Task-2 + Task-3 final state.  The early-return `if (!inRegion) continue;` at the pre-batch `:148` was replaced by Sub-K(c)'s helper-struct rewrite at Task 2 — the line itself no longer exists; per-line dispatch now uses `auto* t = state.currentTarget(); if (! t) continue;` which is a structural improvement, NOT a stale comment.  No comment touches `<group>` inheritance behavior at all post-Task-2; the helper-struct's enter-scope methods (`enterGlobal` / `enterMaster` / `enterGroup` / `enterRegion`) are self-documenting through their function names.

Broader `Source/` grep for: `inRegion` / `seq_length` / `seq_position` / `roundRobinTotal` / `parseSFZ` references — all matches are in the rewritten `VibePlayerDSP.{cpp,h}` code itself or in `Source/VibePlayer/VibePlayerProcessor.{cpp,h}` (Sub-M pre-scan) or in `Source/Standalone/StandaloneEditor.cpp` (keyswitch label closures) or in `Source/Standalone/PianoRoll.{cpp,h}` + `Source/Standalone/PianoRollPage.{cpp,h}` (keyswitch label provider plumbing).  No stale comments referencing pre-batch behavior surface anywhere.

`libs/sfizz/src/sfizz/Layer.{h,cpp}` — the Sub-R/S patch comments are intentional + accurate (documenting the QA-SfzGroup local-patch + the C++ memory ordering rationale).  No stale comments.

### Section 2 — Diagnostic instrumentation catalog: confirmed empty

Per Rule 4 catalog discipline, Section 9 entries across Task 0 → Task 3 are all "Nil."  No `DBG` / `juce::Logger::writeToLog` / temp `jassert` / debug `juce::AlertWindow` / temp file logging added during this batch.  Confirmed via grep across all touched files — no diagnostic instrumentation remains.

### Section 3 — Task 4 commit decision

Per L10 commit cadence + the Sub-I(a) 6-task lock: cleanup tasks that produce zero source edits do NOT get standalone commits.  Task 4's no-op outcome folds into Task 5 close commit as documentation-only material (this Task 4 entry + the close-pass section below).  Matches QA-VoicePool Task 5 (stress-file verify PASS, no commit) precedent + QA-InsertMaps Task 3 (verify-only, no commit) precedent.

### Section 4 — NIT 1 fix-up (PianoRoll.cpp em-dash + arrow ASCII conversion)

`/review-batch QA-SfzGroup` close-pass surfaced NIT 1: em-dash (`—`) + Unicode arrow (`→`) characters in [`Source/Standalone/PianoRoll.cpp:185-186`](../../Source/Standalone/PianoRoll.cpp:185) comment added during Sub-P (keyswitch tooltip surfacing) violate `feedback_ascii_only_ui_strings.md` (extended by precedent to source comments).  Fix: replace em-dash with ASCII hyphen `-` + replace Unicode arrow with ASCII `->`.  Applied in-batch per `feedback_qa_batches_fix_bugs_dont_defer.md` extended to close-pass NITs (QA-InsertMaps Task 5 fix-up at e9fe545 precedent).  Single 2-character touch + zero behavior change.

---

## Task 5 — CLOSE: docs + close commit

### Section 1 — /review-batch QA-SfzGroup outcome

`/review-batch QA-SfzGroup` dispatched at close-pass review.  Outcome: **READY-TO-COMMIT** with 2 NITs.

- **NIT 1 (FIXED in-batch at Task 4 Section 4):** em-dash + Unicode arrow in `Source/Standalone/PianoRoll.cpp:185-186` comment.  Fixed per `feedback_ascii_only_ui_strings.md`.
- **NIT 2 (REFRAMED as accepted design):** Sub-A's original Track 2 investigation-only lock now reads slightly misleadingly post-Sub-R/S amendment — the Spec-calls table shows Sub-A with the original investigation-only wording AND Sub-R/S amending it, but the plan body Task 3 description still reads as investigation-only.  Reframed because (a) Spec-calls table is the canonical record per Task 3 Section 8 + the close commit documents the Sub-R/S amendment explicitly; (b) editing the plan body Task 3 description post-hoc would violate `feedback_targeted_edits_not_wholesale_rewrite.md` (the task description was correct at lock time + has been superseded structurally not edited); (c) the §9 thirty-ninth Forks entry's "Trigger" section explicitly walks the Sub-A → Sub-R/S amendment chain.  No source / plan edit; accepted as a documentation layering pattern that's already well-served by the existing Spec-calls table + Forks entry.

0 BLOCKER + 0 NEEDS-FIX + 2 NITs (1 FIXED + 1 REFRAMED).  Recommendation: READY-TO-COMMIT.

### Section 2 — Three findings routed to NEW QA-Sfizz batch + slot lock

Per Jeff's verbatim 2026-05-27 mid-Task-3 close: "We are going with Option (b): Keep the atomic patch + route the deep investigation to the follow-up batch... Halt the Investigation... Please proceed with the remaining Task 4 cleanup sweep and prepare the final close-out steps."  + Item 2 routing follow-up: "we are gonna route that batch to immediately after this one so that we are all done with the sfz aria stuff all in 'one' go".

Three findings routed to NEW **QA-Sfizz** follow-up batch:

1. **Item 1: keyswitch label discoverability for BaySickRustyDrums + BaySickGuitars + BaySickBasses piano rolls.**  QA-SfzGroup Sub-P=(a) limited amber-highlight + `sw_label` text rendering to BaySickPlayer engines only.
2. **Item 2: Guitars / Basses round-robin loss diagnosis.**  Profile the actual Aria-player content load path; verify whether Karoryfer big-rusty-drums-style content parses correctly through sfizz's `loadSfzFile` path.
3. **Item 3: BaySickRustyDrums MT-mode bit-crusher diagnosis + fix.**  Sub-R/S atomic patch landed but DID NOT resolve the bit-crusher symptom; actual MT-only race source is elsewhere in sfizz.

**Slot (Jeff-locked 2026-05-27):** immediately after QA-SfzGroup, before QA-EngineApvts (closes the Aria/sfizz cluster in one continuous sweep).

### Section 3 — NEW §0 Rule 5 codified at Task 0 commit `a92f55a`

QA-SfzGroup plan-finalize surfaced an `feedback_dont_make_unilateral_spec_calls.md` violation: I drafted the plan with Sub-I/J/K recommendations BAKED INTO the task bodies as truth rather than surfacing them to Jeff.  Jeff caught: "instead of posing any of them to me have just included your suggestions as truth and you've done that on the last 3 plans."

Resolution shipped at QA-SfzGroup Task 0 commit `a92f55a`:

- **Main Plan §0 Rule 5 ADDED:** "Sub-spec calls discovered during planning surface via chat BEFORE landing in the plan body."
- **`feedback_dont_make_unilateral_spec_calls.md`** — "Plan-mode discovery rule" section appended.
- **`Files For Claude/batch_session_boilerplate.md`** — step 5b strengthened + standing rule added.

Discipline locks at planning time (not just execution time) so the surface-then-wait pattern fires consistently across both phases.

### Section 4 — Close-pass plan-doc edits applied at Task 5

- `Plans & Specs/Main Plan.md` §5 — QA-SfzGroup STATUS banner ADDED ("CLOSED.  Twenty-one spec calls Sub-A through Sub-T executed...") at the existing QA-SfzGroup entry header.
- `Plans & Specs/Main Plan.md` §5 — NEW QA-Sfizz entry INSERTED between QA-SfzGroup and QA-EngineApvts with full scope (Items 1-3), Risk/Dependencies/Sequencing/Effort/Bucket/Verify fields.
- `Plans & Specs/Main Plan.md` §5 — QA-EngineApvts Sequencing field updated: "immediately after QA-SfzGroup, before QA-Ed" → "immediately after QA-Sfizz, before QA-Ed".
- `Plans & Specs/Main Plan.md` §6 — arrow updated to insert `→ QA-Sfizz************************` between QA-SfzGroup*********************** and QA-EngineApvts**********************.
- `Plans & Specs/Main Plan.md` §6 — NEW 24-asterisk QA-Sfizz footnote ADDED between QA-SfzGroup footnote and QA-EngineApvts footnote.
- `Plans & Specs/Main Plan.md` §6 — QA-EngineApvts footnote updated from "after QA-SfzGroup" to "after QA-Sfizz".
- `Plans & Specs/Main Plan.md` §9 — thirty-ninth Forks entry APPENDED covering QA-SfzGroup close + Sub-A amendment via Sub-R/S + Sub-T 3-finding routing + NEW §0 Rule 5 codification + 2x mid-batch scope expansions.
- `Plans & Specs/Implemented Work Log.md` — QA-SfzGroup batch-close entry appended (was applied earlier in Task 5 sequence per the standing batch-close protocol).
- `Plans & Specs/Running Notes/magical-petting-dijkstra.md` — Task 4 + this Task 5 close-pass section appended (this section).

### Section 5 — Carry-Forward §1 contradictions

None architectural.  Carry-Forward §1 (Render Engine Primitives) doesn't document SFZ `<group>` opcode-inheritance, keyswitching, or sfizz MT-execution behavior.  All three are bug-fix / feature-build territory, not architectural-primitive territory.  Note: Carry-Forward §1 also doesn't document the now-fixed BaySickPlayer parser inheritance behavior — the pre-QA-SfzGroup broken-inheritance state was the de-facto state for the entire history of `parseSFZ`; QA-SfzGroup brought it into SFZ v1 spec compliance.

### Section 6 — Process meta-findings

**FND-M1 (plan-mode discipline lock):** the Rule 5 codification was a process-side fix triggered by my plan-finalize unilateral-pick error.  Captures a pattern where the unilateral-pick rule (already in feedback memory) needed an EXPLICIT plan-mode extension since plan-mode is a distinct execution phase from per-task work.  Locked at Task 0 commit + the boilerplate update means future batches see this discipline whether they read CLAUDE.md or Main Plan §0 or the boilerplate.

**FND-M2 (diagnostic miss owned):** Sub-R/S atomic patch hypothesis was wrong (bit-crusher MT race is NOT in `sequenceCounter_`).  Owned at Task 3 close per `feedback_own_the_codebase_no_git_alibi.md`.  Patch still ships as defense-in-depth (plain-int RMW under MT is UB per C++ spec) + the actual race investigation routes to QA-Sfizz.  Lesson: when hypothesizing a race source, ground it in empirical observation BEFORE committing source changes — "it's the most obvious shared-state RMW" is not the same as "this is the race that causes this specific symptom."

**FND-M3 (scope expansion via Rule 5 + canon precedents):** 2x mid-batch scope expansions (Sub-L/M/N/O keyswitching engine; Sub-P/Q UI discoverability) + 1 Sub-A amendment (Sub-R/S atomic patch) all surfaced via chat per Rule 5 + answered with verbatim Jeff quotes captured in running notes BEFORE any source touch.  Pattern works.

### Section 7 — Effort: actual vs estimate

- Estimate at Task 0 batch-open: ~4-6 hours (Track 1 parser fix ~1-2 hr + Track 2 sfizz investigation ~1-2 hr + verify ~1-2 hr).
- Actual: ~12-16 hours (2x mid-batch scope expansions + Sub-R/S amendment + diagnostic-miss cycle on bit-crusher + 2-track verify ladder + 2 plan-finalize re-drafts + 5-commit cadence).

Over by ~2.5-3x.  Bulk of the overage was the scope expansions (keyswitching engine + UI discoverability ~4-6 hr) + the Sub-R/S diagnostic-miss cycle (~2-3 hr) + the Rule 5 surface + plan re-draft (~1-2 hr).  Estimate at Task 0 framed the batch as pure parser-fix-plus-investigation; the audible RR-loss gating on keyswitching + the UI surface need + the bit-crusher discovery were all out-of-frame.

### Section 8 — Commits

| # | SHA | Task | Scope |
|---|---|---|---|
| 1 | `a92f55a` | Task 0 | Batch-open: plan-file mirror + running-notes seed + Main Plan §5 plan-pointer flip + §5/§9 `Source/sfizz/` → `libs/sfizz/` path-typo fix + NEW Main Plan §0 Rule 5 codification (sub-spec calls discovered during planning surface via chat BEFORE landing in the plan body). |
| 2 | `196d72e` | Task 1 | Pre-flight inventory (docs-only): parseSFZ 12-opcode coverage confirmed + VibeRegion struct copy-semantics validated for Sub-K(c) helper-struct + Tuba-KS.sfz §9 thirty-eighth Forks diagnosis verified at the file level + MAJOR Track 3 finding (BaySickRustyDrums wrapper-synthesis asymmetry) + sfizz parser landmarks captured. |
| 3 | `5cd5f17` | Task 2 | Structural source (2x scope expansion mid-task per Sub-P(a) in-batch fold): Full `<group>` opcode-inheritance state machine + 6-opcode keyswitching engine + 7th opcode `sw_label` + piano-roll UI discoverability.  11 files, +595 / -55 net. |
| 4 | `821b561` | Task 3 | sfizz parser/state-builder inheritance code review (read-only) + BaySickRustyDrums wrapper synthesis opcode-preservation audit + BaySickGuitars/BaySickBasses loader-handoff confirmation + CoreLibrary content sample verification + live test session that surfaced a catastrophic MT-mode bit-crusher on BaySickRustyDrums cymbals/hi-hats + Sub-R/S atomic patch (targeted vendored-sfizz fix: `int sequenceCounter_` → `std::atomic<int>` + `fetch_add(1, std::memory_order_relaxed)` at both call sites).  4 files, +123 / -3 net.  Diagnostic-miss outcome owned (patch is semantically correct + ships as defense-in-depth but empirical verification post-clean-rebuild confirmed the bit-crusher symptom UNCHANGED). |
| 5 | TBD | Task 5 | CLOSE: docs (§5 + §6 + §9 + Running Notes + Implemented Work Log) + NIT 1 fix-up (PianoRoll.cpp em-dash + arrow ASCII conversion).  No source changes beyond NIT 1.  Task 4 (cleanup grep sweep) was no-op outcome — folded into this close commit. |

### Section 9 — Next action

QA-SfzGroup CLOSED.  Next batch per the updated §6 sequencing arrow: **QA-Sfizz** (Items 1-3 routed at this close per Sub-T + Jeff's "we are gonna route that batch to immediately after this one so that we are all done with the sfz aria stuff all in 'one' go" lock).  Per-batch plan file + running-notes seed deferred until QA-Sfizz opens per `feedback_plan_mirror_one_way.md` discipline.
