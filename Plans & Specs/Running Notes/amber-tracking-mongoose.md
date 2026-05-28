# Running Notes — QA-Sfizz (amber-tracking-mongoose)

> Append-only execution log for QA-Sfizz. Each entry captures the state at a checkpoint trigger (post-commit / post-sub-task verify / post-finding / post-spec-call / post-scope-pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Append-only — never edit prior entries; new findings surface as new entries.

**Pair file:** `Plans & Specs/Batch Plans/amber-tracking-mongoose.md`
**Convention reference:** Main Plan §0 "Document Formatting Conventions" + "Plan file + Running Notes required sections" (locked 2026-05-11) + Rule 4 Diagnostic Instrumentation Catalog (locked 2026-05-12) + Rule 5 Plan-mode sub-spec-call surface discipline (locked 2026-05-26).

---

## Diagnostic Instrumentation Catalog

Per §0 Rule 4: every diagnostic addition (`DBG` / `juce::Logger::writeToLog` / temp `jassert` added for diagnosis / debug `juce::AlertWindow` popups / temp file logging / `std::cout`-style traces) gets logged here in the same edit pass as the source change. At task/batch close, walk this table and strip every `Remove` entry from source — surface the strip list to Jeff BEFORE running the strip pass.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| _(none yet)_ | | | |

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

Next: surface full git status → `/draft-commit` → surface drafted message + git status for Jeff approval → commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-Sfizz-Task0.txt`.
