# QA-SfzGroup — SFZ `<group>` opcode-inheritance state machine + Aria/sfizz RR loss investigation — Plan (magical-petting-dijkstra)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/magical-petting-dijkstra.md`
> Paired running notes: `Plans & Specs/Running Notes/magical-petting-dijkstra.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule + L10/QA-InsertMaps norm).

## Context

QA-SfzGroup is the close-spawned detour from QA-VoicePool FND-2 (§9 thirty-eighth Forks entry, 2026-05-26).  Two independent investigations bundled because they share the same audible symptom — Aria/sfizz round-robin (RR) variation loss — even though the root causes likely differ:

- **Track 1 — BaySickPlayer hand-rolled parser bug (confirmed):** `VibeSampleManager::parseSFZ` at [Source/VibePlayer/VibePlayerDSP.cpp:90](Source/VibePlayer/VibePlayerDSP.cpp:90) has an early-return `if (!inRegion) continue;` at [:148](Source/VibePlayer/VibePlayerDSP.cpp:148) that silently drops every `<group>`-scoped opcode before any opcode-write.  Per the SFZ v1 spec, opcodes inside `<group>` are supposed to be inherited by every `<region>` that follows until the next `<group>` or EOF.  Our parser implements ZERO of this inheritance.  Tuba-KS.sfz (Core Library Brass Package) verified: 4 `<group>` blocks at lines 253 / 431 / 609 / 786 each declaring `seq_length=4` + `seq_position=1..4` for staccato variants — RR cycling specified in the file, dropped by our parser.

- **Track 2 — sfizz-driven engine RR loss (NOT yet confirmed):** Same audible symptom across BaySickRustyDrums + BaySickGuitars + BaySickBasses (all three use vendored sfizz at `libs/sfizz/`).  Root cause TBD: (i) sfizz state-machine gap (same kind of bug as Track 1, inside vendored sfizz); (ii) BaySick* loader-handoff (state lost in pre-load / file-routing code); or (iii) Aria-player file content (files don't specify RR).  Investigation-only this batch per Jeff's Sub-A lock; any fix routes to a NEW dedicated batch.

**Dependencies:** QA-VoicePool closed `d44397a`.  QA-VoicePool Task 4's L5(a) `findRegion` `std::array<int, 32>` stack-alloc is what surfaced FND-2 — the candidate-list refactor exercised the data path directly + the populated list was degenerate due to missing upstream inheritance.

**Risk:** **low-medium**.  Track 1 = parser-only change on the message thread (file load time); audio path unaffected.  Worst case: the new accumulator-baseline-into-region copy introduces an unexpected per-region behavior delta (caught by ear on any existing Core Library file that doesn't use `<group>` opcodes — should sound identical to pre-batch).  Track 2 is read-only; zero risk.

**Effort estimate:** ~4-6 hours total per §5 — Task 1 inventory ~1 hr; Task 2 Track 1 parser state-machine fix ~1.5-2 hr; Task 3 Track 2 investigation ~1-2 hr; Task 4 cleanup grep sweep ~0.5 hr; Task 5 close ~1 hr.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| Sub-A | **Track 2 investigation-only** — read findings, document in running notes; route any fix (sfizz patch OR loader-handoff fix OR file-content explanation) to a NEW dedicated batch. | Jeff verbatim 2026-05-26: "I do not want to mix custom BaySickPlayer parser logic with vendored sfizz state-machine patching in the same batch." Keeps QA-SfzGroup scope-pure (BaySickPlayer parser only) per `feedback_qa_batches_fix_bugs_dont_defer.md`-extended rollback-boundary discipline. |
| Sub-B | **ALL parseSFZ-supported opcodes** flow through the new inheritance accumulator: `sample` / `key` / `lokey` / `hikey` / `pitch_keycenter` / `lovel` / `hivel` / `tune` / `volume` / `group` / `seq_position` / `seq_length` (12 opcodes total). | Jeff verbatim 2026-05-26: "If we are building the state machine, shore up the full contract for our extracted opcode set." Same state-machine code regardless of which opcodes flow through; (a) shores up the full SFZ v1 inheritance contract for our extracted opcode set once. |
| Sub-C | **Full 4-level cascading inheritance**: `<global>` → `<master>` → `<group>` → `<region>` per SFZ v1 spec — override on the §5 verbatim "group only" scope-lock. | Jeff verbatim 2026-05-26: "Libraries use `<global>` constantly; ignoring it now just leaves technical debt. Implement the full 4-level cascading inheritance." `<control>` block (default_path) is orthogonal to the inheritance hierarchy and stays unchanged. |
| Sub-D | **Verify breadth = Tuba-KS + any Track 2-surfaced degenerate files.** Tuba-KS.sfz is the canonical Track 1 verify case (4-variant RR cycling on staccato hits); any sfizz-engine content Track 2 surfaces as degenerate gets added to the verify list at Task 3 close. | Jeff verbatim 2026-05-26: "We don't need a massive library sweep; normal usage will organically test the rest over time." Regression check on non-`<group>` files happens organically when loading any other SFZ during the normal verify cadence. |
| Sub-F | **Path correction folded into Task 0**: `Source/sfizz/` → `libs/sfizz/` in Main Plan §5 QA-SfzGroup entry + §9 thirty-eighth Forks entry. | Jeff approved 2026-05-26.  Two references confirmed at [Main Plan.md:1260](Plans%20%26%20Specs/Main%20Plan.md:1260) (§5) and [Main Plan.md:4690](Plans%20%26%20Specs/Main%20Plan.md:4690) (§9). |
| Sub-G | **Silly-name = `magical-petting-dijkstra`** (assigned by plan-mode runtime per `feedback_silly_name_is_my_pick.md`). | Locked at plan-mode entry. |
| Sub-H | **Verify cadence = Debug-then-Release per task** (QA-InsertMaps L10 + QA-VoicePool L10 norm + CLAUDE.md Build System standing rule). | Locked. |
| Sub-I | **Strict 6-task structure**: Task 0 open / Task 1 inventory / Task 2 Track 1 parser fix / Task 3 Track 2 investigation / Task 4 cleanup grep sweep / Task 5 close. | Jeff verbatim 2026-05-26: "We are sticking to the strict 6-task structure. I want absolute separation between the custom parser fix, the Track 2 investigation, and the cleanup phases. Predictable rollback boundaries are mandatory." |
| Sub-J | **Doc-only commit at Task 3 end**: running-notes update committed in its own commit as a clean save point before Task 4 starts. | Jeff verbatim 2026-05-26: "If we find the root cause for the sfizz RR loss, I want those running notes committed immediately as a clean save point before we start Task 4." |
| Sub-K | **Helper member function on a local state-struct**: encapsulate the 4-level cascading accumulators inside a parseSFZ-local struct (e.g. `SfzParseState`) with header-transition methods (`enterGlobal` / `enterMaster` / `enterGroup` / `enterRegion` / `enterControl` / `exitOther` / `tryFlushRegion`) + a `currentTarget()` method returning the correct `VibeRegion*` by scope.  Per-line dispatch: `auto* t = state.currentTarget(); if (! t) continue;` → all 12 opcode blocks write to `*t`. | Jeff verbatim 2026-05-26: "Helper member function on a local state-struct. Encapsulate the 4-level cascading accumulators neatly. I want to avoid repetitive pointer checks or brittle per-line target assignments." |

(No sub-spec calls remain open at plan finalization. New sub-spec calls discovered mid-batch surface via chat per Main Plan §0 Rule 5 + `feedback_dont_make_unilateral_spec_calls.md` plan-mode section.)

---

## Files to modify

### Task 0 — Open
- `Plans & Specs/Main Plan.md` — §5 QA-SfzGroup entry: add `**Plan file:** Plans & Specs/Batch Plans/magical-petting-dijkstra.md` line; fix `Source/sfizz/` → `libs/sfizz/` typo at line 1260.
- `Plans & Specs/Main Plan.md` — §9 thirty-eighth Forks entry: fix `Source/sfizz/` → `libs/sfizz/` typo at line 4690.
- `Plans & Specs/Batch Plans/magical-petting-dijkstra.md` — mirrored from `~/.claude/plans/magical-petting-dijkstra.md`; home-dir copy deleted post-mirror per `feedback_plan_mirror_one_way.md`.
- `Plans & Specs/Running Notes/magical-petting-dijkstra.md` — seeded with title + purpose blockquote + pair file ref + convention ref + Task 0 Open entry + empty Diagnostic Instrumentation Catalog.

### Task 1 — Pre-flight inventory (read-only)
- No source files modified.
- `Plans & Specs/Running Notes/magical-petting-dijkstra.md` — Task 1 inventory entry appended with: full parseSFZ opcode-list confirmation (all 12 + any unsupported group-scoped opcodes used by Core Library files); Tuba-KS.sfz `<group>` block locations confirmed; libs/sfizz parser landmarks (`Parser.cpp` processHeader / `Region.h` sequenceLength/sequencePosition / `ParserPrivate.cpp` opcode dispatch); BaySick* engine load-handoff file:line refs; sample of 5-10 Core Library `.sfz` files for `<group>` / `<master>` / `<global>` usage patterns; any sub-spec calls discovered (surfaced via chat per Rule 5 if any).

### Task 2 — Track 1 parser fix
- [Source/VibePlayer/VibePlayerDSP.cpp:90-237](Source/VibePlayer/VibePlayerDSP.cpp:90) — `parseSFZ` function body refactored per Sub-K (c) helper-struct pattern (see Task 2 implementation outline below).
- No header change (`SfzParseState` is function-local; VibeRegion struct unchanged).
- `Plans & Specs/Running Notes/magical-petting-dijkstra.md` — Task 2 entry with implementation summary + verify pass results.

### Task 3 — Track 2 sfizz parser + BaySick* loader-handoff investigation (read-only)
- No source files modified.
- `Plans & Specs/Running Notes/magical-petting-dijkstra.md` — Task 3 investigation entry: sfizz `Parser.cpp` + `Region.cpp` + `ParserPrivate.cpp` reads with file:line landmarks for group/region inheritance + sequenceLength/sequencePosition handling; BaySickRustyDrumsProcessor.cpp:502 + BaySickGuitarsProcessor.cpp:260 + BaySickBassesProcessor.cpp:251 load-handoff reads; 2-3 Aria-player content file inspections; root-cause hypothesis + routing recommendation for a new dedicated batch (slot/placement is Jeff's spec call at close time per `feedback_slot_placement_is_spec_call.md`).

### Task 4 — Cleanup + grep sweep
- `Source/VibePlayer/VibePlayerDSP.{cpp,h}` — sweep for stale comments referencing the pre-batch broken-inheritance behavior; edit out any that became stale post-fix.
- `Source/` (broader sweep) — grep for any SFZ-parser comments that became stale after the new state machine.
- `Plans & Specs/Running Notes/magical-petting-dijkstra.md` — Task 4 entry with grep results + any source touches.

### Task 5 — Close
- `Plans & Specs/Implemented Work Log.md` — new batch-close entry compiled by `/draft-doc batch-close`.
- `Plans & Specs/Main Plan.md` — IF Track 2 surfaced a fix-warranting root cause: new §5 batch row + §9 Forks entry routing the fix to a new dedicated batch (slot is Jeff's spec call at close time per `feedback_slot_placement_is_spec_call.md`); IF Track 2 surfaced no actionable fix (e.g. file-content-only / sfizz handles correctly): §9 Forks entry documenting the investigation outcome with no §5 add.
- `Plans & Specs/Main Plan.md` — §5 QA-SfzGroup entry STATUS banner update.
- `Plans & Specs/Running Notes/magical-petting-dijkstra.md` — close-pass section appended (NIT routing per QA-VoicePool Task 7 precedent if `/review-batch` surfaces any).

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/magical-petting-dijkstra.md` → `Plans & Specs/Batch Plans/magical-petting-dijkstra.md` (Write tool); delete the home-dir copy.
- [ ] Edit [Main Plan.md:1260](Plans%20%26%20Specs/Main%20Plan.md:1260) — fix `Source/sfizz/` → `libs/sfizz/` in §5 QA-SfzGroup entry; add `**Plan file:** Plans & Specs/Batch Plans/magical-petting-dijkstra.md` line directly below the `####` header (matching existing §5 entries' convention).
- [ ] Edit [Main Plan.md:4690](Plans%20%26%20Specs/Main%20Plan.md:4690) — fix `Source/sfizz/` → `libs/sfizz/` in §9 thirty-eighth Forks entry.
- [ ] Seed `Plans & Specs/Running Notes/magical-petting-dijkstra.md` with required §0 sections (title / purpose blockquote / pair file ref / convention ref / Diagnostic Instrumentation Catalog header (empty initially) / Task 0 Open entry).
- [ ] Surface full git status. Dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval. Commit on approval via `git commit -F <file>` (long technical-narrative messages — CLAUDE.md "## Git Commit Mechanics").
- [ ] Mark Task 0 done.

### Task 1 — Pre-flight inventory (read-only)
- [ ] Read [VibePlayer/VibePlayerDSP.cpp:90-237](Source/VibePlayer/VibePlayerDSP.cpp:90) end-to-end; confirm 12-opcode list (`sample` / `key` / `lokey` / `hikey` / `pitch_keycenter` / `lovel` / `hivel` / `tune` / `volume` / `group` / `seq_position` / `seq_length`) and identify any edge cases the Explore agent's grep missed.
- [ ] Read [VibePlayer/VibePlayerDSP.h](Source/VibePlayer/VibePlayerDSP.h) VibeRegion struct (lines 11-31) to confirm field names + default initializers (the helper-struct copy semantics depend on default-constructed VibeRegion behavior).
- [ ] Sample 5-10 Core Library `.sfz` files from `Documents/BaySickDAW/Files For Claude/karoryfer.big-rusty-drums-1.100/Programs/` + `AppData/Local/BaySickDAW/CoreLibrary/` for `<global>` / `<master>` / `<group>` usage patterns + opcode coverage we don't currently extract (informational — may surface routing candidates for future batches).
- [ ] Read [libs/sfizz/src/sfizz/parser/Parser.cpp](libs/sfizz/src/sfizz/parser/Parser.cpp) around line 128 (`processHeader()`) + line 218 (definition) + line 252 (`_currentHeader = name`) for the header state machine landmarks (Task 3 starting points).
- [ ] Read [libs/sfizz/src/sfizz/Region.h](libs/sfizz/src/sfizz/Region.h) lines 265 / 306-307 / 341-346 for `group` / `sequenceLength` / `sequencePosition` / `groupAmplitude` / `masterAmplitude` fields (Task 3 starting points).
- [ ] Read [libs/sfizz/src/sfizz/parser/ParserPrivate.cpp](libs/sfizz/src/sfizz/parser/ParserPrivate.cpp) for opcode dispatch + inheritance logic landmarks (Task 3 starting points).
- [ ] Read [BaySickRustyDrumsProcessor.cpp:502](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:502) + [BaySickGuitarsProcessor.cpp:260](Source/BaySickGuitars/BaySickGuitarsProcessor.cpp:260) + [BaySickBassesProcessor.cpp:251](Source/BaySickBasses/BaySickBassesProcessor.cpp:251) load-handoff sites (Task 3 starting points).
- [ ] If any sub-spec call surfaces from inventory (e.g. unexpected parser shape edge case, different default for `<global>` initialization): surface to Jeff via chat per Main Plan §0 Rule 5 BEFORE Task 2 lands.  Otherwise note "no new sub-spec calls" in running notes.
- [ ] Dispatch `/draft-doc running-notes` with the full inventory output; apply to running notes via Edit.
- [ ] Tell Jeff: "Task 1 is read-only inventory; no source touched. Running notes updated with full inventory output + sfizz/BaySick* starting-point landmarks for Task 3. Any sub-spec calls that surfaced are listed above (or 'none' if none)."
- [ ] Surface git status. Dispatch `/draft-commit`. Surface drafted message + git status for approval. Commit on approval (doc-only commit — running notes + any tiny doc adjustments only).
- [ ] Mark Task 1 done.

### Task 2 — Track 1 fix: 4-level cascading inheritance state machine via local state-struct
- [ ] Edit [VibePlayer/VibePlayerDSP.cpp:90-237](Source/VibePlayer/VibePlayerDSP.cpp:90) — `parseSFZ` function body rewrite per Sub-K (c) helper-struct pattern.  Implementation outline:

  ```cpp
  void VibeSampleManager::parseSFZ (const juce::File& sfzFile)
  {
      const juce::File sfzDir = sfzFile.getParentDirectory();
      juce::StringArray lines;
      lines.addLines (sfzFile.loadFileAsString());

      enum class Scope { None, Global, Master, Group, Region };

      struct SfzParseState
      {
          Scope scope { Scope::None };
          bool  inControl { false };
          VibeRegion globalDefaults;
          VibeRegion masterDefaults;
          VibeRegion groupDefaults;
          VibeRegion current;

          void enterGlobal()  { globalDefaults = VibeRegion{};
                                masterDefaults = globalDefaults;
                                groupDefaults  = masterDefaults;
                                scope = Scope::Global;  inControl = false; }
          void enterMaster()  { masterDefaults = globalDefaults;
                                groupDefaults  = masterDefaults;
                                scope = Scope::Master;  inControl = false; }
          void enterGroup()   { groupDefaults = masterDefaults;
                                scope = Scope::Group;   inControl = false; }
          void enterRegion()  { current = groupDefaults;
                                scope = Scope::Region;  inControl = false; }
          void enterControl() { scope = Scope::None;    inControl = true;  }
          void exitOther()    { scope = Scope::None;    inControl = false; }

          void tryFlushRegion (std::vector<VibeRegion>& regions)
          {
              if (scope == Scope::Region && current.audioData)
                  regions.push_back (current);
              current = VibeRegion{};
          }

          VibeRegion* currentTarget()
          {
              switch (scope)
              {
                  case Scope::Region: return &current;
                  case Scope::Group:  return &groupDefaults;
                  case Scope::Master: return &masterDefaults;
                  case Scope::Global: return &globalDefaults;
                  default:            return nullptr;
              }
          }
      };

      SfzParseState state;
      juce::String defaultPath;

      for (const auto& rawLine : lines)
      {
          juce::String line = rawLine.upToFirstOccurrenceOf ("//", false, false).trim();
          if (line.isEmpty()) continue;

          // Header dispatch
          if      (line.containsIgnoreCase ("<region>"))  { state.tryFlushRegion (mRegions); state.enterRegion();
                                                            line = line.fromFirstOccurrenceOf ("<region>", false, true).trim(); }
          else if (line.containsIgnoreCase ("<group>"))   { state.tryFlushRegion (mRegions); state.enterGroup();   continue; }
          else if (line.containsIgnoreCase ("<master>"))  { state.tryFlushRegion (mRegions); state.enterMaster();  continue; }
          else if (line.containsIgnoreCase ("<global>"))  { state.tryFlushRegion (mRegions); state.enterGlobal();  continue; }
          else if (line.containsIgnoreCase ("<control>")) { state.tryFlushRegion (mRegions); state.enterControl(); continue; }
          else if (line.startsWithChar ('<'))             { state.tryFlushRegion (mRegions); state.exitOther();    continue; }

          // <control> default_path handling unchanged (orthogonal to hierarchy)
          if (state.inControl)
          {
              auto v = sfzOpcode (line, "default_path", true);
              if (v.isNotEmpty()) defaultPath = v.replace ("\\", "/");
              continue;
          }

          // Opcode dispatch — single target-pointer per line via helper
          auto* t = state.currentTarget();
          if (! t) continue;

          // sample= (path resolution preserved; writes to *t — meaningful only when Region but cheap accumulator-write otherwise)
          { auto v = sfzOpcode (line, "sample", true);
            if (v.isNotEmpty()) {
                juce::String rel = defaultPath + v.replace ("\\", "/");
                juce::File f = sfzDir.getChildFile (rel);
                if (f.existsAsFile()) {
                    double sr = 44100.0;
                    t->audioData      = loadFile (f, mFormatManager, sr);
                    t->sampleFile     = f;
                    t->fileSampleRate = sr;
                }
            }
          }
          { auto v = sfzOpcode (line, "key");           if (v.isNotEmpty()) { int n = sfzNote(v); t->loNote = n; t->hiNote = n; t->rootNote = n; } }
          { auto v = sfzOpcode (line, "lokey");         if (v.isNotEmpty()) t->loNote = sfzNote(v); }
          { auto v = sfzOpcode (line, "hikey");         if (v.isNotEmpty()) t->hiNote = sfzNote(v); }
          { auto v = sfzOpcode (line, "pitch_keycenter"); if (v.isNotEmpty()) t->rootNote = sfzNote(v); }
          { auto v = sfzOpcode (line, "lovel");         if (v.isNotEmpty()) t->loVel = v.getIntValue(); }
          { auto v = sfzOpcode (line, "hivel");         if (v.isNotEmpty()) t->hiVel = v.getIntValue(); }
          { auto v = sfzOpcode (line, "tune");          if (v.isNotEmpty()) t->tuneOffset = v.getFloatValue() / 100.f; }
          { auto v = sfzOpcode (line, "volume");        if (v.isNotEmpty()) t->volumeOffset = v.getFloatValue(); }
          { auto v = sfzOpcode (line, "group");         if (v.isNotEmpty()) t->articulationGroup = juce::jlimit(0, 3, v.getIntValue() - 1); }
          { auto v = sfzOpcode (line, "seq_position");  if (v.isNotEmpty()) t->roundRobinIndex = v.getIntValue(); }
          { auto v = sfzOpcode (line, "seq_length");    if (v.isNotEmpty()) t->roundRobinTotal = v.getIntValue(); }
      }

      state.tryFlushRegion (mRegions);
  }
  ```

  Notes on the pattern:
  - The pre-batch `if (!inRegion) continue;` at [:148](Source/VibePlayer/VibePlayerDSP.cpp:148) is replaced by `auto* t = state.currentTarget(); if (! t) continue;` — semantically equivalent for the prior behavior (None scope returns nullptr → continue), but now correctly routes Group / Master / Global opcodes into their respective accumulators instead of dropping them.
  - `enterRegion()` copies `groupDefaults → current` — that's the inheritance: the new region's baseline is whatever the group accumulator currently holds, which itself was inherited from master/global on the most recent `enterGroup` / `enterMaster` / `enterGlobal` call.
  - `<control>` block stays orthogonal: setting `scope = Scope::None` + `inControl = true` ensures opcode dispatch is skipped (currentTarget returns nullptr) AND the `if (state.inControl)` branch above handles `default_path` extraction.
- [ ] Tell Jeff: "Run `do_build.bat`. Then in Debug:
  - **(1)** Load Tuba-KS.sfz onto BaySickPlayer (Brass Package → Tuba-KS).  Switch to staccato articulation (key switch C#6 or the next staccato key per the file's `sw_label`).  Hit the same key 4 times in a row.  Verify each hit sounds audibly distinct — 4-variant RR cycling.
  - **(2)** Switch back to sustain articulation (key switch C6).  Hit a sustained note.  Verify it sounds identical to pre-batch (the sustain `<group>` has no `seq_*` opcodes — only ampeg_* / sw_* / volume opcodes none of which parseSFZ extracts; sustain should sound clean).
  - **(3)** Load any other Core Library SFZ file that doesn't use `<group>`-scoped RR (e.g. a bass / synth SFZ).  Hit any note.  Verify it sounds identical to pre-batch (regression check on non-`<group>` files).
  - **(4)** Reload Tuba-KS.sfz; hit a staccato key again; confirm RR variant assignment is deterministic (same key → same variant order per hit cycle)."
- [ ] Wait for Jeff's verify result on Debug.  On pass, repeat for Release per L10/Sub-H.
- [ ] On Debug+Release pass: dispatch `/draft-commit`, surface drafted message + full git status, commit on approval via `git commit -F <file>`.
- [ ] Dispatch `/draft-doc running-notes` and apply to running-notes file.
- [ ] Mark Task 2 done.

### Task 3 — Track 2 sfizz parser + BaySick* loader-handoff investigation (read-only)
- [ ] Deep-read [libs/sfizz/src/sfizz/parser/Parser.cpp](libs/sfizz/src/sfizz/parser/Parser.cpp) around `processHeader()` (line 128 + definition at 218) + `flushCurrentHeader` (line 403-406): how does sfizz transition between `<group>` / `<region>`?  Does it copy state between blocks or use a different mechanism?
- [ ] Deep-read [libs/sfizz/src/sfizz/parser/ParserPrivate.cpp](libs/sfizz/src/sfizz/parser/ParserPrivate.cpp) opcode-dispatch + accumulation logic.
- [ ] Deep-read [libs/sfizz/src/sfizz/Region.h](libs/sfizz/src/sfizz/Region.h) `Region` struct around `sequenceLength` (line 306) + `sequencePosition` (line 307) + `groupAmplitude` (lines 341-346): are RR fields cascaded the way amplitude/volume are?  If not, the asymmetry IS the bug.
- [ ] Read [BaySickRustyDrumsProcessor.cpp:502](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:502) + surrounding `loadKit()` flow.  How does the kit path get to `mSfizz->loadSfzFile()`?  Is there any state-stripping / file-rewriting before the handoff?
- [ ] Read [BaySickGuitarsProcessor.cpp:260](Source/BaySickGuitars/BaySickGuitarsProcessor.cpp:260) + [BaySickBassesProcessor.cpp:251](Source/BaySickBasses/BaySickBassesProcessor.cpp:251) for the same pattern.
- [ ] Run BaySickDAW (Jeff launches), load 2-3 Aria-player content files into the affected engines, hit the same MIDI note 4x in a row, listen for RR variation.  Each engine + content combination: pass / fail.
- [ ] Cross-reference: if a content file specifies `<group>`-scoped `seq_length` / `seq_position` AND sfizz parses it AND no RR is heard in-app → loader-handoff or content issue.  If a content file specifies RR AND sfizz doesn't parse it correctly → sfizz state-machine gap.  If a content file does NOT specify RR → file-content issue.
- [ ] Dispatch `/draft-doc running-notes` with the full Track 2 investigation output: (i) sfizz state-machine status (does it cascade RR?); (ii) loader-handoff status (any state lost?); (iii) file-content status (do Aria-player files specify RR?); (iv) root-cause hypothesis; (v) routing recommendation (which type of new batch + rough effort estimate + dependencies; slot/placement is Jeff's spec call at close per `feedback_slot_placement_is_spec_call.md`).  Apply to running notes via Edit.
- [ ] If any sub-spec call surfaces from the investigation (e.g. scope ambiguity about how deep to investigate sfizz internals): surface to Jeff via chat per Main Plan §0 Rule 5 BEFORE the Task 3 commit.
- [ ] Tell Jeff: "Task 3 is read-only investigation; no source touched.  Running notes updated with full investigation output + routing recommendation for a new dedicated batch.  Slot/placement of the new batch is YOUR spec call at close time — not now."
- [ ] Surface git status (running notes only). Dispatch `/draft-commit`. Surface drafted message + git status for approval. Commit on approval (doc-only commit per Sub-J — clean save point before Task 4 starts).
- [ ] Mark Task 3 done.

### Task 4 — Cleanup + grep sweep
- [ ] Grep `Source/VibePlayer/VibePlayerDSP.{cpp,h}` for stale comments referencing the pre-batch broken-inheritance behavior (e.g. comments saying "opcodes only inside `<region>`" / "we don't support `<group>` inheritance" / similar).
- [ ] Grep `Source/` broader for any SFZ-parser comments that became stale post-batch.
- [ ] Verify zero diagnostic instrumentation in source (Rule 4 catalog should be empty at task start — verify by grepping for `[QA-SfzGroup]` / `[SFZ]` / temp `DBG` / temp `juce::Logger` additions; cross-check against the running notes catalog).
- [ ] If comments / dead bits found: edit them out.  If none: this task is a no-op + the running-notes entry records the grep sweep result.
- [ ] Dispatch `/draft-doc running-notes` and apply.
- [ ] If source edits happened: surface git status, dispatch `/draft-commit`, surface, commit on approval.  If no source edits: skip the commit; the running-notes update folds into the close commit.
- [ ] Mark Task 4 done.

### Task 5 — Close
- [ ] Dispatch `/draft-doc batch-close` with the running notes file as primary input.
- [ ] Review draft for factual accuracy, scope completeness, routing-table correctness; apply to `Plans & Specs/Implemented Work Log.md` via Edit.
- [ ] Dispatch `/review-batch QA-SfzGroup`.  Address BLOCKERs / NEEDS-FIX in-batch per `feedback_qa_batches_fix_bugs_dont_defer.md`.  Defer NITs to close entry only after explicit fix-or-reframe-as-accepted-design pass per QA-VoicePool Task 7 precedent (e9fe545 QA-InsertMaps Task 5 fix-up canon, NOT the QA-AudioMeters Task 5 bulk-defer pattern).
- [ ] Route side findings per Rule 3:
  - **Track 2 → new dedicated batch** (if Track 2 surfaced a fix-warranting root cause).  Slot is Jeff's spec call (NOT mine) per `feedback_slot_placement_is_spec_call.md`.  §9 Forks entry written + new §5 batch row inserted + §6 sequencing arrow + footnote updated.
  - **Track 2 → no actionable fix** (file-content-only / sfizz handles correctly / etc.).  §9 Forks entry documenting the investigation outcome with no §5 add.
- [ ] Update Main Plan §5 QA-SfzGroup STATUS banner with close outcome.
- [ ] Surface full git status.
- [ ] Dispatch `/draft-commit` for close commit. Surface message + status. Commit on approval via `git commit -F <file>`.
- [ ] Mark Task 5 done.

---

## Verification (end-to-end smoke)

After Task 4 commit lands:

1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **Tuba-KS RR cycling (Track 1 primary):** load Tuba-KS.sfz; switch to staccato articulation; hit same key 4 times in a row → 4 audibly distinct variants (per Sub-D verify breadth).
3. **Tuba-KS sustain (Track 1 regression):** switch to sustain articulation; hit a sustained note → sounds identical to pre-batch (the sustain `<group>` has no `seq_*` opcodes; only ampeg_* / sw_* / volume which parseSFZ doesn't extract; sustain regression-checks clean).
4. **Non-`<group>` SFZ regression:** load any other Core Library SFZ that doesn't use `<group>`-scoped opcodes; verify any note sounds identical to pre-batch.
5. **Track 2 verify (per investigation outcome):** specific verify scenarios locked at Task 3 close depending on what Track 2 surfaced (e.g. if sfizz cascades RR correctly + loader-handoff is clean, the verify is "Aria content has RR but plays without variation" → file-content issue; verify = the routing decision is correct).
6. **No regression on non-SFZ paths:** `loadFolder` + `loadSingleFile` paths unchanged (separate code paths from parseSFZ; spot-check by loading a single .wav drum sample into BaySickPlayer).
7. **MT vs serial parity:** SFZ parsing is message-thread only; MT mode setting has no impact on parseSFZ behavior.  Verify by toggling MT in the Mixer hamburger menu before each verify gesture per L10/Sub-H Debug-then-Release norm.

---

## Routing notes (Rule 3 application during execution)

- **Other parseSFZ opcode-coverage gaps surfaced during inventory** (e.g. `<group>`-scoped `ampeg_attack` / `sw_default` / `sw_lokey` / `sw_hikey` / `group_label` — none of which parseSFZ currently extracts but the Tuba-KS.sfz file uses): record in running notes; route to Future State if not in-scope for QA-SfzGroup's "12-opcode coverage" lock.  Coverage expansion is a separate future-batch concern, NOT a QA-SfzGroup scope creep.
- **Track 2 sfizz/loader/file-content root cause** → new dedicated batch (slot is Jeff's spec call at close time per `feedback_slot_placement_is_spec_call.md`).  Investigation findings + routing recommendation land in running notes mid-batch via Task 3; the canonical Forks entry + §5 batch row land at close.
- **QA-VoicePool re-sighted NIT / leftover finding** (none expected, but if surfaced via `/review-batch` at close): per `feedback_closed_batch_carryforward_via_forks.md`, fix in this open batch + §9 Forks entry back-refs QA-VoicePool.  Never reopen closed-batch commits.
- **Diagnostic instrumentation added during execution** (e.g. for Track 2 investigation if SFZ parse output needs to be visualized): catalog row added to running notes IN THE SAME EDIT PASS per Rule 4; Disposition = `Remove at task close` unless Jeff flags `Keep`.  Strip at task close.
- **New sub-spec calls discovered mid-batch** (not pre-known at plan finalize): surface to Jeff via chat per Main Plan §0 Rule 5 + `feedback_dont_make_unilateral_spec_calls.md` plan-mode section.  Never pre-pick + bake into source.

---

## Carry-Forward Reference touch points

- **§1 Render Engine Primitives**: SFZ parsing is message-thread (file load time); audio-thread API contracts unaffected.  Read at Task 0 to confirm no architectural-primitive contradiction surfaces (none expected — `parseSFZ` is not documented as an audio-thread primitive).
- **§2 Lock-Free + Lifecycle Primitives**: not touched.
- **§3 Mixer/Page Lifecycle Index**: not touched.
- **§4 Decisions Already Made**: not touched.

Carry-Forward Reference is FROZEN per §0 Rule 1 — contradictions land in the Implemented Work Log close entry as new entries, not as edits to Carry-Forward.

---

## Pre-condition checks at Task 0 launch

- [ ] `git status` shows: 1 modified file `juce/modules/juce_gui_basics/widgets/juce_TreeView.cpp` (pre-existing JUCE-vendor modification carried across multiple recent batches; surface-and-leave per QA-VoicePool / QA-AudioMeters / QA-InsertMaps / QA-Eg precedent — NOT staged).
- [ ] `git log -1` shows: `d44397a QA-VoicePool Task 7 CLOSE` — QA-VoicePool close landed; QA-SfzGroup is the next batch per §6 sequencing.
- [ ] Working tree clean of any prior batch artifacts (no leftover `.git/COMMIT_EDITMSG_*` from QA-VoicePool — verify before Task 0 commit).
