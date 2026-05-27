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
