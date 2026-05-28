# QA-Sfizz — sfizz follow-up: keyswitch label discoverability + Guitars/Basses RR-loss diagnosis + BaySickRustyDrums MT bit-crusher fix — Plan (amber-tracking-mongoose)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/amber-tracking-mongoose.md`
> Paired running notes: `Plans & Specs/Running Notes/amber-tracking-mongoose.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule).

> **Plan-mode home-dir naming note:** plan-mode runtime auto-assigned `cryptic-popping-hanrahan.md`; user-approved silly-name is `amber-tracking-mongoose` per `feedback_silly_name_is_my_pick.md`. Plan was written to BOTH home-dir paths during plan mode (this one for ExitPlanMode UI; the approved-name copy for naming continuity). Task 0 mirrors to `Plans & Specs/Batch Plans/amber-tracking-mongoose.md` then deletes BOTH home-dir copies per `feedback_plan_mirror_one_way.md`.

## Context

QA-Sfizz is the close-spawned follow-up from QA-SfzGroup (closed `42f7253` on 2026-05-27 16:00 PT). Slot per Sub-T at QA-SfzGroup close: immediately after QA-SfzGroup, before QA-EngineApvts. Bucket: Players + Cross-cutting Infrastructure. Effort estimate: ~6-12 hr.

**Origin:** three findings surfaced during QA-SfzGroup Task 2 + Task 3 verify sessions, all sharing the same vendored-library (`libs/sfizz/`) + sfizz-parser/state-machine surface. Routed to a new dedicated batch per Jeff verbatim "we are gonna route that batch to immediately after this one so that we are all done with the sfz aria stuff all in 'one' go." See Main Plan §5 QA-Sfizz entry (line 1314) + §9 thirty-ninth Forks entry (line 4801).

**Three Items, Jeff-locked verbatim in §5:**

**Item 1 — Keyswitch label discoverability for the 3 sfizz-driven engines** (BaySickRustyDrums + BaySickGuitars + BaySickBasses). QA-SfzGroup Sub-P(a) limited the amber-highlight + `sw_label` UI rendering to BaySickPlayer engines only (the 4 `dynamic_cast<VibePlayerProcessor*>` closures at Layer/Bass/Drum/Clip piano-roll registration sites). The 3 sfizz-driven engines cast-fail there → no amber/label rendering. The piano-keyboard render plumbing (`PianoRollConnection::keyswitchLabelProvider` field at [PianoRollPage.h:73](Source/Standalone/PianoRollPage.h:73), `PianoKeyboard` amber-paint branch at [PianoRoll.cpp:180-207](Source/Standalone/PianoRoll.cpp:180), tooltip provider priority at [PianoRoll.cpp:107-126](Source/Standalone/PianoRoll.cpp:107), `PianoRollPage::registerEngine` wiring at [PianoRollPage.cpp:112](Source/Standalone/PianoRollPage.cpp:112)) was added by QA-SfzGroup Task 2 — Item 1 wires 3 new closures at the 3 sfizz-engine registration sites and exposes the sfizz keyswitch-label data via a new public-API accessor (SC-4 patch).

**Item 2 — Guitars/Basses round-robin loss diagnosis.** sfizz `<group>` cascading inheritance verified correct in QA-SfzGroup Task 3 code review (sfizz's `Synth::Impl::buildRegion` 4-layer cascade is per spec). BaySickRustyDrums uses `buildOutputRoutedSfzWrapper` to synthesize a wrapper SFZ then `loadSfzString` — RR works there (Jeff verbatim "i hear variation on rusty"). BaySickGuitars + BaySickBasses use plain `loadSfzFile` (no wrapper synthesis) — Jeff verbatim "still no rr". Likely-resolved-by-Item-1 hypothesis (§9 thirty-ninth Forks entry): Jeff couldn't engage articulations or test at consistent velocity due to label invisibility. If RR audible post-Item-1, Item 2 closes verify-only. If not, deeper sfizz dive (`Default::sequence` semantics + sample-data inspection + loaded-`Region` inheritance state at runtime).

**Item 3 — BaySickRustyDrums MT-mode bit-crusher diagnosis + fix.** Catastrophic audio degradation on cymbals/hi-hats when Multi-Threading is enabled; kick/snare clean both ways; MT-off completely clean. QA-SfzGroup Sub-R/S landed a defense-in-depth atomic patch on [libs/sfizz/src/sfizz/Layer.h:151](libs/sfizz/src/sfizz/Layer.h:151) + [Layer.cpp:63/:191](libs/sfizz/src/sfizz/Layer.cpp:63) `sequenceCounter_` (`int` → `std::atomic<int>` with `fetch_add(1, std::memory_order_relaxed)`) but empirical verify confirmed the bit-crusher symptom UNCHANGED. Actual MT-only race source is elsewhere in sfizz. Per QA-SfzGroup Work Log Item-3 routing notes ([Implemented Work Log.md:1452](Plans & Specs/Implemented Work Log.md:1452)): candidates are voice allocation at `Synth::Impl::startVoice` (Synth.cpp:~1300), voice manager state, sample buffer concurrent access in Voice.cpp / RegionStateful.cpp, polyphony manager. Likely needs file-logging instrumentation (`DBG`/`OutputDebugString` invisible in Release without DebugView; project convention is `juce::FileLogger` at `Documents/BaySickDAW/` per `feedback_follow_existing_folder_convention.md`) for per-voice / per-Layer / per-noteOn state capture during multi-voice cymbal hit. Effort ~3-6 hr investigation + ~1-2 hr fix.

**Dependencies:** QA-SfzGroup closed (`42f7253`); the post-QA-SfzGroup vendored sfizz state (post Sub-R/S atomic patch) is the baseline for QA-Sfizz.

**Risk:** medium. Item 1 is surgical (bounded sfizz patch + 3 closure additions, all bounded by existing patterns). Item 2 is conditional (likely-resolved-by-Item-1; otherwise diagnostic). Item 3 has open-ended scope (MT race diagnosis is notoriously difficult); fallback options at Track 3 close (Sub-A/B/C below).

**Effort estimate:** ~6-12 hr per §5 entry. (Item 1 ~1-2 hr per §5 / ~2-3 hr per Work Log routing-table; Item 2 ~1-2 hr if resolved by Item 1 / ~3-4 hr otherwise; Item 3 ~3-6 hr investigation + ~1-2 hr fix.)

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC-1 | **7-task structure** (Task 0 open / Task 1 inventory / Task 2 Item 1 / Task 3 Item 2 verify-then-diag / Task 4 Item 3 diagnosis / Task 5 Item 3 fix / Task 6 close) | Jeff verbatim 2026-05-27: "Option (c) 7-task. We are separating the Item 3 MT race diagnosis from the fix. Since this is an asynchronous bug in a vendored library, I want to see the empirical logging and root-cause isolation in Task 4 before any source fixes are attempted in Task 5." Clean save point between dx and fix. |
| SC-2 | **Item 1 split into 2 sub-commits by loader path** (Task 2A: BaySickRustyDrums wrapper-load path + sfizz public accessor patch; Task 2B: BaySickGuitars + BaySickBasses file-load path) | Jeff verbatim 2026-05-27: "Option (c) Two commits. Split them by the loader path (Rusty first via string wrapper, then Guitars/Basses via file load). This gives us a clean rollback boundary if the string wrapper path behaves differently regarding sfizz state access." sfizz public accessor patch lands in Task 2A's commit since both 2A and 2B depend on it. |
| SC-3 | **Bounded targeted vendored-sfizz patches authorized in-batch** (single-symbol type change / single-function additions; broader state-machine architectural changes route to follow-up batch) | Jeff verbatim 2026-05-27: "Option (b) Bounded targeted patches. If the MT race fix is a targeted, localized patch (e.g., adding a lock or fixing an atomic), do it in-batch. If it requires broad state-machine architectural changes to sfizz, route it out." Mirrors QA-SfzGroup Sub-R amendment precedent. Originally scoped to Item 3 (Track 3 / MT race); extended to Item 1 by SC-4. |
| SC-4 | **SC-3 authorization extends to Item 1's sfizz public-accessor patch** (single new method `Synth::getKeyswitchLabels()` + parallel forward on `sfz::Sfizz` wrapper; mirrors existing `getKeyLabels()` / `getCCLabels()` pattern exactly at [Synth.h:707](libs/sfizz/src/sfizz/Synth.h:707) / [sfizz.hpp:1029](libs/sfizz/src/sfizz.hpp:1029); no logic change; no threading change) | Jeff verbatim 2026-05-27: "Option (a). Extend the SC-3=(b) authorization to include Item 1 in the plan body. A read-only public accessor that mirrors existing API shapes is perfectly safe, bounded, and exactly the kind of localized patch we want to handle in-batch to unblock the feature." Phase 1 research confirmed sfizz exposes `getKeyLabels()` ([Synth.h:707](libs/sfizz/src/sfizz/Synth.h:707)) and `getCCLabels()` ([Synth.h:713](libs/sfizz/src/sfizz/Synth.h:713)) but NO public keyswitch-label accessor; internal data lives at [SynthPrivate.h:288](libs/sfizz/src/sfizz/SynthPrivate.h:288). |
| SC-5 | Silly-name `amber-tracking-mongoose` | Plan-mode runtime auto-assigned `cryptic-popping-hanrahan`; user-approved name overrides per `feedback_silly_name_is_my_pick.md`. amber = keyswitch labels; tracking = MT race diagnosis; mongoose = quick-strike. |
| SC-6 | Verify cadence: Debug-then-Release per task | Mirrors QA-VoicePool / QA-InsertMaps / QA-SfzGroup norm + CLAUDE.md "Build System Standing rule" + `feedback_no_full_release_reverify_at_batch_close.md`. |
| SC-7 | Source-file references in commit bodies use brand-mixed-case engine names (BaySickRustyDrums / BaySickGuitars / BaySickBasses) | Per `feedback_match_jeff_text_casing.md`; file paths in diffs unavoidable until QA-PlayerRename lands per session-open boilerplate. |

---

## Sub-spec calls surfaced for ExitPlanMode

Genuinely deferred until Task 4 surfaces a root cause (NOT picks the agent wants to make pre-investigation; per Main Plan §0 Rule 5 + the QA-D `federated-bouncing-cupcake.md` exemplar's table shape).

| ID | Question | Trigger | Options |
|----|----------|---------|---------|
| Sub-A | If Task 4's diagnostic instrumentation does NOT isolate the MT race root cause, what fallback policy ships in Task 5? | Task 4 close (root-cause investigation complete with no actionable lead). | (a) Ship conservative serial-execution-mode fallback for cymbals/hi-hats regions in Task 5 (accept perf hit; SC-3=(b) authorization applies if patch is bounded; reframe Item 3 as workaround vs fix). (b) Route Item 3 fix entirely to a NEW follow-up batch beyond QA-Sfizz (QA-Sfizz closes Items 1+2; bit-crusher stays known-issue per §9 Forks entry). (c) Document as known-issue with §9 Forks entry; no fix this batch; no follow-up batch. |
| Sub-B | If Task 4 surfaces a fixable root cause, in-tree vendored-sfizz patch only vs upstream sfizz PR submission also? | Task 4 close (root-cause identified). | (a) In-tree vendored-sfizz patch only (BaySickDAW-local fix; QA-SfzGroup Sub-R/S precedent). (b) Both in-tree patch AND upstream sfizz PR submission (in-tree ships for V1; upstream tracks separately). (c) Upstream sfizz fix only (delay V1 ship of Item 3 fix on upstream merge; not recommended given Item 3's catastrophic-bug rating). |
| Sub-C | If Task 4's instrumentation reveals a race source whose fix requires broad state-machine architectural changes to sfizz (exceeding the SC-3=(b) bounded-patch envelope), what's the in-batch vs route-out call? | Task 4 close (root-cause identified BUT fix shape exceeds bounded-patch envelope). | (a) Route Item 3 fix to NEW follow-up batch beyond QA-Sfizz; close QA-Sfizz with Items 1+2 complete + Item 3 diagnosed-but-not-fixed (§9 Forks routing entry). (b) Re-amend SC-3 scope mid-batch to allow the broader patch (mirrors QA-SfzGroup Sub-R amendment precedent; Jeff would re-authorize via chat per Rule 5). |

---

## Files to modify

### Task 2A — sfizz public accessor patch + BaySickRustyDrums label provider

**Vendored sfizz (bounded patch per SC-4):**
- [libs/sfizz/src/sfizz/Synth.h:714](libs/sfizz/src/sfizz/Synth.h:714) — add `getKeyswitchLabels()` declaration after `getCCLabels()` at :713 (parallel doc-comment + signature shape).
- [libs/sfizz/src/sfizz/Synth.cpp:~2406](libs/sfizz/src/sfizz/Synth.cpp:2406) — implementation after `getCCLabels()` at :2400 (`return impl->keyswitchLabels_;`).
- [libs/sfizz/src/sfizz.hpp:1036](libs/sfizz/src/sfizz.hpp:1036) — add `getKeyswitchLabels()` declaration after `getCCLabels()` at :1035.
- [libs/sfizz/src/sfizz/sfizz.cpp:~401](libs/sfizz/src/sfizz/sfizz.cpp:401) — implementation after `getCCLabels()` forward at :397 (`return synth->synth.getKeyswitchLabels();`).

**BaySickRustyDrums engine processor accessor:**
- [Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h) — add `juce::String getKeyswitchLabel(int midiNote) const noexcept;` to public section.
- [Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp) — implementation (iterate `mSfizz->getKeyswitchLabels()` for matching midiNote; return `juce::String::fromUTF8` on match, else empty).

**Closure wiring:**
- [Source/Standalone/StandaloneEditor.cpp:5893](Source/Standalone/StandaloneEditor.cpp:5893) — inside `registerBaySickRustyDrumsPianoRoll()` (5832-5896), add `conn.keyswitchLabelProvider = [this](int n) -> juce::String { ... }` closure after `conn.allKeysWhite = true;` and before `mPianoRollPage->registerEngine(...)` at :5895.

### Task 2B — BaySickGuitars + BaySickBasses label provider closures

**BaySickGuitars engine processor accessor:**
- [Source/BaySickGuitars/BaySickGuitarsProcessor.h](Source/BaySickGuitars/BaySickGuitarsProcessor.h) — add `juce::String getKeyswitchLabel(int midiNote) const noexcept;`.
- [Source/BaySickGuitars/BaySickGuitarsProcessor.cpp](Source/BaySickGuitars/BaySickGuitarsProcessor.cpp) — implementation (same body shape as 2A).

**BaySickBasses engine processor accessor:**
- [Source/BaySickBasses/BaySickBassesProcessor.h](Source/BaySickBasses/BaySickBassesProcessor.h) — add `juce::String getKeyswitchLabel(int midiNote) const noexcept;`.
- [Source/BaySickBasses/BaySickBassesProcessor.cpp](Source/BaySickBasses/BaySickBassesProcessor.cpp) — implementation (same body shape as 2A).

**Closure wiring (1 file, 2 closure additions):**
- [Source/Standalone/StandaloneEditor.cpp:7980](Source/Standalone/StandaloneEditor.cpp:7980) — inside the `kind == EngineKind::BaySickGuitars` branch (7972-7980), add `conn.keyswitchLabelProvider` closure.
- [Source/Standalone/StandaloneEditor.cpp:7993](Source/Standalone/StandaloneEditor.cpp:7993) — inside the `kind == EngineKind::BaySickBasses` branch (7982-7994), add `conn.keyswitchLabelProvider` closure.

### Task 3 — Item 2 verify-then-diag (verify-only by default; conditional source touches)

- No source files known a priori. If verify surfaces a real RR-loss bug (sfizz inheritance gap for `loadSfzFile` path vs `loadSfzString` path), source files surface dynamically — most likely candidates: `libs/sfizz/src/sfizz/Parser.cpp`, `libs/sfizz/src/sfizz/Region.cpp`, `libs/sfizz/src/sfizz/Synth.cpp` around `loadSfzFile`/`loadSfzString` paths. Surface any new sub-spec call to Jeff per Rule 5 BEFORE source touch.

### Task 4 — Item 3 diagnosis (instrumentation only; no source fix beyond instrumentation)

Candidate instrumentation sites per QA-SfzGroup Work Log Item-3 routing notes:
- [libs/sfizz/src/sfizz/Synth.cpp:~1300](libs/sfizz/src/sfizz/Synth.cpp:1300) — `Synth::Impl::startVoice` (voice allocation).
- [libs/sfizz/src/sfizz/Voice.cpp](libs/sfizz/src/sfizz/Voice.cpp) — voice render path / sample buffer access.
- [libs/sfizz/src/sfizz/RegionStateful.cpp](libs/sfizz/src/sfizz/RegionStateful.cpp) — per-region state (RR counter / phase accumulators / flag bits).
- [libs/sfizz/src/sfizz/Layer.cpp](libs/sfizz/src/sfizz/Layer.cpp) — already-touched by Sub-R/S; additional state capture if needed.
- Polyphony manager (file path surfaces during Task 1 inventory — search `libs/sfizz/src/sfizz/` for "Polyphony" / "VoiceManager" / "VoiceList").

Instrumentation pattern (Rule 4 catalog):
- Logger: `juce::FileLogger` writing to `Documents/BaySickDAW/qa-sfizz-task4-trace.log` per `feedback_follow_existing_folder_convention.md`. Single logger instance shared across instrumented sites (heap-alloc'd at first use; flushed per-write).
- Tag prefix: `[QA-Sfizz Task4]` per existing convention.
- Per-site capture: thread id (`std::this_thread::get_id()`), voice index, region pointer, sample position, RR counter, any shared-mutable state visible.
- Disposition: `Remove at Task 5 close` for every site.

### Task 5 — Item 3 fix (or routing decision; TBD per Task 4 root-cause)

One of (per Sub-A / Sub-B / Sub-C resolution at Task 4 close):
- **(a) Targeted bounded sfizz patch** (per SC-3=(b)): file:line surfaces during Task 4; mirrors Sub-R/S precedent shape.
- **(b) Conservative serial-execution fallback**: scope-specific to cymbals/hi-hats sample region; file:line surfaces during Task 4.
- **(c) Routing-only**: no source change in Task 5; §9 Forks entry + new follow-up batch.

---

## Tasks

### Task 0 — Open commit (docs)

- [ ] Mirror `~/.claude/plans/amber-tracking-mongoose.md` (approved-name home-dir copy) → `Plans & Specs/Batch Plans/amber-tracking-mongoose.md` (Write tool); delete BOTH home-dir copies per `feedback_plan_mirror_one_way.md` (`amber-tracking-mongoose.md` + plan-mode auto-assigned `cryptic-popping-hanrahan.md`).
- [ ] Seed `Plans & Specs/Running Notes/amber-tracking-mongoose.md` per Main Plan §0 running-notes required sections (title / purpose blockquote / pair ref / convention ref).
- [ ] Update Main Plan §5 QA-Sfizz entry: replace `**Plan file:** \`<silly-name>.md (when started)\`` at [Main Plan.md:1316](Plans & Specs/Main Plan.md:1316) with `` **Plan file:** `Plans & Specs/Batch Plans/amber-tracking-mongoose.md` ``.
- [ ] Surface full `git status` (every dirty + untracked entry per `feedback_surface_full_git_status_before_commit.md`).
- [ ] Dispatch `/draft-commit`; surface drafted message + full git status to Jeff for approval per `feedback_surface_drafted_commit_message_for_approval.md` + `feedback_drafter_output_verbatim_no_restyle.md`.
- [ ] Commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-Sfizz-Task0.txt` per CLAUDE.md "Git Commit Mechanics" (multi-paragraph technical-narrative style uses file-based commit, not bash heredoc). Remove the temp message file post-commit.
- [ ] Dispatch `/draft-doc running-notes` (mode=append) and apply to running-notes file per `feedback_draft_doc_running_notes_every_checkpoint.md`.
- [ ] Mark Task 0 done via TaskUpdate.

### Task 1 — Inventory (docs only; read-only; no commit)

Confirm Phase 1 research findings + surface additional sfizz internals needed for Tasks 2-5. No source touch.

- [ ] Confirm `Synth::getKeyswitchLabels()` patch shape by re-reading [libs/sfizz/src/sfizz/Synth.h:707-714](libs/sfizz/src/sfizz/Synth.h:707) + [libs/sfizz/src/sfizz/Synth.cpp:2394-2406](libs/sfizz/src/sfizz/Synth.cpp:2394) (`getKeyLabels()` existing pattern).
- [ ] Confirm `sfz::Sfizz::getKeyswitchLabels()` wrapper shape by re-reading [libs/sfizz/src/sfizz.hpp:1025-1035](libs/sfizz/src/sfizz.hpp:1025) + [libs/sfizz/src/sfizz/sfizz.cpp:392-400](libs/sfizz/src/sfizz/sfizz.cpp:392).
- [ ] Confirm the 3 sfizz-engine piano-roll registration sites in StandaloneEditor.cpp:
   - BaySickRustyDrums: [StandaloneEditor.cpp:5832-5896](Source/Standalone/StandaloneEditor.cpp:5832) `registerBaySickRustyDrumsPianoRoll()`.
   - BaySickGuitars: [StandaloneEditor.cpp:7972-7980](Source/Standalone/StandaloneEditor.cpp:7972) inside `registerInstSourcePianoRoll(InstPage*)`.
   - BaySickBasses: [StandaloneEditor.cpp:7982-7994](Source/Standalone/StandaloneEditor.cpp:7982) inside same function.
- [ ] Confirm `mSfizz` member of each of 3 engine processors:
   - [BaySickRustyDrumsProcessor.h:185](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h:185) — `std::unique_ptr<sfz::Sfizz> mSfizz;`.
   - [BaySickGuitarsProcessor.h:146](Source/BaySickGuitars/BaySickGuitarsProcessor.h:146) — same.
   - [BaySickBassesProcessor.h:140](Source/BaySickBasses/BaySickBassesProcessor.h:140) — same.
- [ ] Confirm existing BaySickPlayer keyswitch closure pattern (reference for Task 2 mirror):
   - [StandaloneEditor.cpp:5734-5738](Source/Standalone/StandaloneEditor.cpp:5734) Layer.
   - [:5780-5784](Source/Standalone/StandaloneEditor.cpp:5780) Bass.
   - [:5820-5824](Source/Standalone/StandaloneEditor.cpp:5820) Drum.
   - [:7890-7894](Source/Standalone/StandaloneEditor.cpp:7890) Clip.
- [ ] Confirm PianoRoll plumbing from QA-SfzGroup Task 2 still in place (no regression):
   - `PianoRollConnection::keyswitchLabelProvider` field at [PianoRollPage.h:73](Source/Standalone/PianoRollPage.h:73).
   - `PianoRollContainer::setKeyswitchLabelProvider` at [PianoRoll.cpp:2904-2907](Source/Standalone/PianoRoll.cpp:2904).
   - `PianoKeyboard` amber-paint branch at [PianoRoll.cpp:180-207](Source/Standalone/PianoRoll.cpp:180).
   - Tooltip priority at [PianoRoll.cpp:107-126](Source/Standalone/PianoRoll.cpp:107).
   - `PianoRollPage::registerEngine` wiring at [PianoRollPage.cpp:112](Source/Standalone/PianoRollPage.cpp:112).
- [ ] Quick inventory pass for Item 3 instrumentation candidate sites (read-only):
   - [libs/sfizz/src/sfizz/Synth.cpp:~1300](libs/sfizz/src/sfizz/Synth.cpp:1300) — locate exact line range of `Synth::Impl::startVoice`.
   - [libs/sfizz/src/sfizz/Voice.cpp](libs/sfizz/src/sfizz/Voice.cpp) — locate voice-render entry point.
   - [libs/sfizz/src/sfizz/RegionStateful.cpp](libs/sfizz/src/sfizz/RegionStateful.cpp) — locate region-state mutation sites.
   - Search `libs/sfizz/src/sfizz/` for "Polyphony" / "VoiceManager" / "VoiceList" — locate polyphony manager file path.
- [ ] If Task 1 surfaces any new sub-spec call (e.g. sfizz internal accessor not callable from public `Sfizz` wrapper; or polyphony-manager surface different from expected), pose to Jeff in chat per Main Plan §0 Rule 5 BEFORE proceeding to Task 2.
- [ ] Dispatch `/draft-doc running-notes` and apply.
- [ ] **No commit on Task 1** (docs-only inventory pass; mirrors QA-VoicePool Task 1 docs-only precedent at `bddcaa6` + QA-SfzGroup Task 1 docs-only precedent at `196d72e`; per QA-InsertMaps Task 3 verify-only no-commit pattern).

### Task 2 — Item 1: keyswitch label discoverability for the 3 sfizz-driven engines (2 sub-commits per SC-2)

#### Task 2A — sfizz public accessor patch + BaySickRustyDrums label provider closure

Per SC-2=(c), this commit covers the wrapper-load-path engine (BaySickRustyDrums) first; the sfizz public accessor patch lands here as shared infrastructure (Task 2B depends on it being in tree).

**Sfizz public accessor patch (4 files):**

- [ ] [libs/sfizz/src/sfizz/Synth.h:714](libs/sfizz/src/sfizz/Synth.h:714) — after `getCCLabels()` declaration at :713, add:
  ```cpp
  /**
   * @brief Get the keyswitch labels, if any
   *
   * @return const std::vector<NoteNamePair>&
   */
  // QA-Sfizz Task 2A 2026-05-27: BaySickDAW local-patch - read-only public
  // accessor exposing impl->keyswitchLabels_ for piano-roll discoverability
  // on the 3 sfizz-driven engines (BaySickRustyDrums + BaySickGuitars +
  // BaySickBasses).  Mirrors getKeyLabels()/getCCLabels() pattern; no
  // logic/threading change.  See Plans & Specs/Batch Plans/
  // amber-tracking-mongoose.md.
  const std::vector<NoteNamePair>& getKeyswitchLabels() const noexcept;
  ```

- [ ] [libs/sfizz/src/sfizz/Synth.cpp:~2406](libs/sfizz/src/sfizz/Synth.cpp:2406) — after `getCCLabels()` implementation at :2400-2404, add:
  ```cpp
  const std::vector<NoteNamePair>& Synth::getKeyswitchLabels() const noexcept
  {
      return impl->keyswitchLabels_;
  }
  ```

- [ ] [libs/sfizz/src/sfizz.hpp:1036](libs/sfizz/src/sfizz.hpp:1036) — after `getCCLabels()` declaration at :1035, add:
  ```cpp
  /**
   * @brief Get the keyswitch labels, if any.
   * @since (BaySickDAW local-patch - QA-Sfizz Task 2A 2026-05-27)
   */
  const std::vector<std::pair<uint8_t, std::string>>& getKeyswitchLabels() const noexcept;
  ```

- [ ] [libs/sfizz/src/sfizz/sfizz.cpp:~401](libs/sfizz/src/sfizz/sfizz.cpp:401) — after `getCCLabels()` forward at :397-400, add:
  ```cpp
  const std::vector<std::pair<uint8_t, std::string>>& sfz::Sfizz::getKeyswitchLabels() const noexcept
  {
      return synth->synth.getKeyswitchLabels();
  }
  ```

**BaySickRustyDrums engine processor accessor (2 files):**

- [ ] [BaySickRustyDrumsProcessor.h](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h) — add to public section (near `auditionNote` / `getPianoRollKeymap` declarations):
  ```cpp
  // QA-Sfizz Task 2A: keyswitch label accessor for piano-roll discoverability.
  // Iterates sfizz's parsed sw_label storage (populated at SFZ load time from
  // <region>/<group> sw_label opcodes); returns the label for the given MIDI
  // note if it's a keyswitch in the currently loaded kit, else empty.
  juce::String getKeyswitchLabel(int midiNote) const noexcept;
  ```

- [ ] [BaySickRustyDrumsProcessor.cpp](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp) — implementation:
  ```cpp
  juce::String BaySickRustyDrumsProcessor::getKeyswitchLabel(int midiNote) const noexcept
  {
      if (! mSfizz) return {};
      for (const auto& pair : mSfizz->getKeyswitchLabels())
          if (static_cast<int>(pair.first) == midiNote)
              return juce::String::fromUTF8(pair.second.c_str());
      return {};
  }
  ```

**Closure wiring (1 file):**

- [ ] [StandaloneEditor.cpp:5893](Source/Standalone/StandaloneEditor.cpp:5893) — inside `registerBaySickRustyDrumsPianoRoll()` after `conn.allKeysWhite = true;` and before `mPianoRollPage->registerEngine(...)` at :5895:
  ```cpp
  // QA-Sfizz Task 2A: keyswitch label provider for sfizz-driven engine.
  // Mirrors the BaySickPlayer pattern at registerLayerPianoRoll (:5734-5738) /
  // registerBassPianoRoll (:5780-5784) / registerDrumPianoRoll (:5820-5824) /
  // registerClipPianoRoll (:7890-7894).
  conn.keyswitchLabelProvider = [this](int n) -> juce::String {
      if (auto* eng = mProcessor.getBaySickRustyDrums())
          return eng->getKeyswitchLabel(n);
      return {};
  };
  ```

- [ ] Tell Jeff: "Run `do_build.bat`. Verify in Debug FIRST then Release:
   - **(1)** Open the PianoRoll page. Engine dropdown → BaySickRustyDrums. Open piano roll for it.
   - **(2)** **If the currently loaded BaySickRustyDrums kit has keyswitch ranges** (most karoryfer big-rusty-drums Programs files have keyswitches like `sw_default=c0` + `sw_lokey=c0 sw_hikey=b0` mapping to articulations): the keyswitch keys should render with amber backgrounds + bold dark-amber `sw_label` text + tooltip on hover. Compare visually to a BaySickPlayer Tuba-KS piano roll which is the QA-SfzGroup Task 2 reference UX.
   - **(3)** **If the loaded kit has NO keyswitches** (unlikely for BaySickRustyDrums but possible for some kits): the keyboard should render unchanged - no amber bleed-through; existing J-7b `noteLabelProvider` drum labels (`Snare Center`, `Hi-hat Tight Closed`, etc.) still readable.
   - **(4)** Switch the loaded BaySickRustyDrums kit to a different one (if more than one is available). Confirm amber labels refresh from the newly-parsed keyswitch data on next piano-roll repaint.
   - **(5)** Regression check: BaySickPlayer Layer/Bass/Drum/Clip tabs still render amber correctly for Tuba-KS (QA-SfzGroup Task 2 wiring untouched).
   - **(6)** Regression check: Harmless / BaySickSynth / BaySickBass piano rolls render unchanged (no false amber on engines that don't implement `getKeyswitchLabel`)."
- [ ] Wait for Jeff's verify result.
- [ ] **On verify pass:** surface full git status. Dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval. Commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-Sfizz-Task2A.txt`. Remove temp message file post-commit.
- [ ] Dispatch `/draft-doc running-notes` and apply.

#### Task 2B — BaySickGuitars + BaySickBasses label provider closures

Builds on Task 2A's sfizz public accessor patch already in tree.

**BaySickGuitars engine processor accessor (2 files):**

- [ ] [BaySickGuitarsProcessor.h](Source/BaySickGuitars/BaySickGuitarsProcessor.h) — add `juce::String getKeyswitchLabel(int midiNote) const noexcept;` to public section (same comment block as 2A header).
- [ ] [BaySickGuitarsProcessor.cpp](Source/BaySickGuitars/BaySickGuitarsProcessor.cpp) — same implementation body as 2A, on `BaySickGuitarsProcessor`.

**BaySickBasses engine processor accessor (2 files):**

- [ ] [BaySickBassesProcessor.h](Source/BaySickBasses/BaySickBassesProcessor.h) — add `juce::String getKeyswitchLabel(int midiNote) const noexcept;`.
- [ ] [BaySickBassesProcessor.cpp](Source/BaySickBasses/BaySickBassesProcessor.cpp) — same implementation body as 2A, on `BaySickBassesProcessor`.

**Closure wiring (1 file, 2 closure additions):**

- [ ] [StandaloneEditor.cpp:7980](Source/Standalone/StandaloneEditor.cpp:7980) — inside the `kind == EngineKind::BaySickGuitars` branch (7972-7980), after `conn.auditionOff = [](int) {};` and before the branch's closing `}`:
  ```cpp
  conn.keyswitchLabelProvider = [proc, idx](int n) -> juce::String {
      if (auto* eng = proc->getBaySickGuitars(idx))
          return eng->getKeyswitchLabel(n);
      return {};
  };
  ```

- [ ] [StandaloneEditor.cpp:7993](Source/Standalone/StandaloneEditor.cpp:7993) — inside the `kind == EngineKind::BaySickBasses` branch (7982-7994), after `conn.defaultTopNote = 48;` and before the branch's closing `}`:
  ```cpp
  conn.keyswitchLabelProvider = [proc, idx](int n) -> juce::String {
      if (auto* eng = proc->getBaySickBasses(idx))
          return eng->getKeyswitchLabel(n);
      return {};
  };
  ```

- [ ] Tell Jeff: "Run `do_build.bat`. Verify in Debug FIRST then Release:
   - **(1)** Add an Inst tab → select BaySickGuitars. Load a Karoryfer-style content kit (or any sfizz content with `sw_label` opcodes - search Core Library if any guitars kit has keyswitches; otherwise we can pick a known-keyswitched sample to validate).
   - **(2)** Open piano roll for it. Keyswitch keys render with amber backgrounds + bold dark-amber `sw_label` text + tooltip on hover.
   - **(3)** Same test for BaySickBasses - add a tab, select source = BaySickBasses, load content, open piano roll.
   - **(4)** Verify LiveInput-source Inst tabs (no engine) still register normally with no keyswitch rendering (closure null when engine processor is null).
   - **(5)** Engine kit-reload: load another kit on the same tab; amber labels refresh on next piano-roll repaint.
   - **(6)** Regression check: BaySickRustyDrums amber rendering from Task 2A still works."
- [ ] Wait for Jeff's verify result.
- [ ] **On verify pass:** surface git status. `/draft-commit`. Surface drafted message + status. Commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-Sfizz-Task2B.txt`. Remove temp message file post-commit.
- [ ] `/draft-doc running-notes`; apply.

### Task 3 — Item 2: Guitars/Basses round-robin loss verify-then-diag

Verify-first per §9 thirty-ninth Forks entry's "most likely resolved by Item 1" hypothesis. Source touches conditional on verify outcome.

- [ ] Tell Jeff: "With Item 1 labels now visible on BaySickGuitars + BaySickBasses piano rolls, retry the RR test that was inaudible in QA-SfzGroup Task 3 verify.
   - **(1)** Load a Karoryfer-style content kit on BaySickGuitars (any sfizz content with `<group>`-scoped `seq_length` + `seq_position`).
   - **(2)** Engage the articulation that the amber labels surface as having RR variants (e.g. open strum / mute / pick attack).
   - **(3)** Place 4-8 piano-roll notes at the same MIDI pitch and same velocity (use a fixed-velocity value via piano-roll velocity-paint or the Velocity control lane - removes human-strum velocity variance that QA-SfzGroup Task 3 verify suffered from).
   - **(4)** Play the pattern. Listen: do you hear 4-variant audible rotation matching the spec? Or still one variant always?
   - **(5)** Repeat on BaySickBasses with a basses content kit (same fixed-velocity placement)."
- [ ] Wait for Jeff's verify result.

- [ ] **Branch A (Item 2 closes verify-only):** If RR audible per spec post-Item-1, Item 2 is closed. No source touch. Record finding in running notes ("Item 2 resolved by Item 1 discoverability - RR was always working in sfizz; the symptom was Jeff-couldn't-test-it-meaningfully without amber labels"). **No commit on Task 3 in Branch A.**

- [ ] **Branch B (deeper diag needed):** If RR still not audibly cycling, dive into:
   - sfizz `Default::sequence` value semantics (check libs/sfizz/src/sfizz/Defaults.h or equivalent).
   - Sample-data inspection (does the kit actually have multiple sample variants per RR slot? `<region>` blocks per RR position?).
   - Loaded-`Region` inheritance state at runtime - instrument with file-logging if needed (add catalog row with `Remove at Task 3 close` disposition per Rule 4).
   - If diagnosis surfaces a bounded sfizz patch (per SC-3=(b) / SC-4 authorization), implement + verify in Task 3.
   - If diagnosis surfaces a broader fix shape (exceeds bounded-patch envelope), surface to Jeff per Rule 5 for route-in-batch-vs-route-out call.
   - Surface ANY new sub-spec call to Jeff per Rule 5 BEFORE source touch.

- [ ] **On Branch B commit needed:** surface git status → `/draft-commit` → surface message → commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-Sfizz-Task3.txt`. Remove temp message file post-commit.
- [ ] Strip any Branch-B instrumentation if disposition=Remove. Surface strip list to Jeff for approval before strip pass per Rule 4.
- [ ] `/draft-doc running-notes`; apply.

### Task 4 — Item 3: BaySickRustyDrums MT-mode bit-crusher diagnosis (instrumentation only; no source fix)

Pure investigation. NO source fix attempted in Task 4. Output: root-cause report in running notes at end of task + answers to Sub-A / Sub-B / Sub-C if relevant.

- [ ] **Step 1 — Reproduce baseline.** Tell Jeff: "Run `do_build.bat`. In Release (the bit-crusher is real under MT-on in both Debug and Release per QA-SfzGroup Task 3 verify):
   - **(1)** Open BaySickRustyDrums tab.
   - **(2)** Toggle MT ON via Mixer hamburger menu.
   - **(3)** Hit a cymbal/hi-hat sound repeatedly (rapid taps on the same drum hitbox / piano-roll placement at the cymbal's MIDI note with a short loop).
   - **(4)** Confirm bit-crusher distortion audible.
   - **(5)** Toggle MT OFF. Repeat - confirm clean.
   - **(6)** Capture WAV samples of both states if convenient (mute Master record arm, play patterns, save audio export - both states for A/B reference)."
- [ ] Wait for Jeff's baseline confirmation.

- [ ] **Step 2 — Instrument candidate sites.** Per QA-SfzGroup Work Log Item-3 routing notes ([Implemented Work Log.md:1452](Plans & Specs/Implemented Work Log.md:1452)), candidates: voice allocation (`Synth::Impl::startVoice` at Synth.cpp:~1300), voice manager state, sample buffer concurrent access (Voice.cpp / RegionStateful.cpp), polyphony manager. For each candidate site:
   - Add file-logging instrumentation per Rule 4 catalog.
   - Logger: `juce::FileLogger` writing to `Documents/BaySickDAW/qa-sfizz-task4-trace.log` per `feedback_follow_existing_folder_convention.md`. Single shared logger instance (heap-alloc'd at first use; flushed per-write so a crash doesn't lose the tail).
   - Tag prefix: `[QA-Sfizz Task4]` per existing convention.
   - Per-site capture: thread id (`std::this_thread::get_id()`), voice index, region pointer, sample position, RR counter, any shared-mutable state visible at the site.
   - Catalog row in running notes file (SAME edit pass per Rule 4): `| Site (file:line) | Tag | Purpose | Disposition |` with Disposition=`Remove at Task 5 close` for every site.

- [ ] **Step 3 — Capture multi-voice cymbal hit trace.** Tell Jeff: "Run `do_build.bat`. Reproduce the bit-crusher under MT-on while the file logger is active (Release build, since the bit-crusher is real in both Debug and Release per QA-SfzGroup verify). The log file at `Documents/BaySickDAW/qa-sfizz-task4-trace.log` will fill - keep the session short (a few cymbal hits, ~5 seconds of audio max) to keep the file scannable."
- [ ] Wait for Jeff's trace capture.

- [ ] **Step 4 — Analyze trace for race signature.** Look for:
   - Two threads writing the same memory address concurrently.
   - Same voice slot acquired by two `startVoice` calls back-to-back (voice-manager race).
   - Sample buffer pointer overwrite mid-render.
   - Non-atomic flag bit twiddle / phase accumulator shared across voices.
   - Polyphony group counter race.
   - Thread interleaving showing the same RR-counter value increment by both threads (paste-cycle race that the Sub-R/S atomic was supposed to fix, but somewhere else now).
- [ ] **Step 5 — Document root-cause hypothesis in running notes.** Either:
   - **Identified:** Root cause is `<specific race site>`; fix shape is `<bounded patch vs broader change>`; per SC-3 authorization → Task 5 lands the fix in-batch (if bounded) OR Sub-C surfaces (if broad).
   - **Not identified:** Instrumentation surfaced no clear race signature. Surface Sub-A to Jeff (fallback policy: conservative serial-execution fallback / route-to-follow-up batch / known-issue documentation).

- [ ] If Step 5 surfaces a fixable root cause, surface Sub-B to Jeff per Main Plan §0 Rule 5 (in-tree only vs in-tree + upstream PR).
- [ ] If Step 5 surfaces a fixable root cause whose fix exceeds bounded-patch envelope, surface Sub-C to Jeff per Main Plan §0 Rule 5 BEFORE Task 5 begins.

- [ ] **Task 4 keeps instrumentation in tree** (`Remove at Task 5 close` disposition); the trace-capable build stays available for Task 5 verify if needed. Don't strip until Task 5's fix is verified.
- [ ] **No commit on Task 4 by default** (instrumentation rolls into Task 5's fix commit, mirroring the diagnostic-strip-with-fix pattern). **If** Task 4 instrumentation is structurally useful as a checkpoint (e.g. fix-by-fix iteration over multiple cymbal-hit captures), surface to Jeff as a sub-spec call (commit-or-fold) per Rule 5.
- [ ] Per Main Plan §0 Rule 4: catalog every Site/Tag/Purpose/Disposition in running-notes file in the SAME edit pass as the instrumentation add.
- [ ] `/draft-doc running-notes`; apply (with Diagnostic Instrumentation Catalog as a separate section per Rule 4).

### Task 5 — Item 3: bit-crusher fix (or routing decision)

Source touch shape depends on Task 4's root-cause output + Jeff's Sub-A / Sub-B / Sub-C answers at Task 4 close.

- [ ] **Branch A (bounded sfizz patch):** Implement the targeted patch (SC-3=(b)/SC-4 authorization scope). Pattern mirrors Sub-R/S precedent - single-symbol type change OR single-function addition. Add QA-Sfizz Task 5 local-patch comment block per vendored-library convention. File:line surfaces from Task 4 root-cause.

- [ ] **Branch B (conservative fallback per Sub-A=(a)):** Implement serial-execution-mode fallback scoped to cymbals/hi-hats sample region (file:line surfaces from Task 4). Document the workaround vs fix framing in commit body + running notes. SC-3=(b) authorization applies if patch is bounded.

- [ ] **Branch C (routing-out per Sub-C=(a) or Sub-A=(b)/(c)):** No source change in Task 5; record routing decision in running notes for Task 6 close-entry §9 Forks entry + new follow-up batch per Sub-C. Surface slot/placement of new follow-up batch to Jeff per `feedback_slot_placement_is_spec_call.md`.

- [ ] **Strip Task 4 instrumentation** per Rule 4 catalog Dispositions. Surface strip list to Jeff for approval BEFORE strip pass. Strip-only sites: all `[QA-Sfizz Task4]` tagged additions. Keep any catalog entries with Disposition=Keep (none expected; all should be Remove).

- [ ] Tell Jeff verify scenarios (TBD per Task 4 root-cause; minimum):
   - **(1)** BaySickRustyDrums cymbals/hi-hats clean on MT-on (no bit-crusher distortion); kick/snare unchanged.
   - **(2)** MT-off behavior unchanged from QA-SfzGroup close baseline (clean both ways).
   - **(3)** Serial-MT parity preserved across both modes.
   - **(4)** No regression on BaySickPlayer / BaySickGuitars / BaySickBasses (existing Item 1 + 2 work intact).
- [ ] Wait for verify.
- [ ] **On verify pass (Branch A or B):** surface git status → `/draft-commit` → surface message → commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-Sfizz-Task5.txt`. Remove temp message file post-commit.
- [ ] **On Branch C:** no source commit in Task 5; Task 5 closes as documentation-only and rolls into Task 6 close commit.
- [ ] `/draft-doc running-notes`; apply.

### Task 6 — Close sequence

- [ ] Dispatch `/draft-doc batch-close` with running-notes file as primary input.
- [ ] Apply the close-entry to `Plans & Specs/Implemented Work Log.md` via Edit (per `feedback_targeted_edits_not_wholesale_rewrite.md` - never wholesale Write).
- [ ] Update Main Plan §5 QA-Sfizz entry: append STATUS banner reflecting close outcome (mirror QA-SfzGroup precedent at Main Plan §5).
- [ ] Update Main Plan §6 sequencing-arrow if any §6 footnote requires adjustment (QA-EngineApvts footnote already locked to "after QA-Sfizz" at QA-SfzGroup close - verify no further change needed; update arrow STATUS markers per close).
- [ ] Add new Main Plan §9 Forks entry (fortieth) describing QA-Sfizz close + any side-finding routings + Sub-A/B/C resolutions + carry-forward state.
- [ ] Dispatch `/review-batch QA-Sfizz`.
- [ ] Address BLOCKERs / NEEDS-FIX in-batch per `feedback_qa_batches_fix_bugs_dont_defer.md` extended to close-pass NITs precedent (QA-InsertMaps Task 5 fix-up `e9fe545` / QA-VoicePool Task 7 fix-up `36fe7fb` / QA-SfzGroup close-pass NIT 1 ASCII fix at `42f7253`).
- [ ] Defer NITs into close-entry routing table only if Jeff explicitly approves; otherwise fix in-batch per `feedback_closed_batch_carryforward_via_forks.md`.
- [ ] Surface full git status (close commit touches docs only; should not include source changes - source already committed in Tasks 2A / 2B / 3 Branch B / 5).
- [ ] Dispatch `/draft-commit` for close commit. Surface drafted message + git status to Jeff for approval per `feedback_drafter_output_verbatim_no_restyle.md`. Commit on approval via `git commit -F .git/COMMIT_EDITMSG_QA-Sfizz-Task6.txt`. Remove temp message file post-commit.
- [ ] Mark all session todos completed; close out batch.

---

## Verification (end-to-end smoke)

After Task 5 commit lands (or Task 6 close if Branch C / known-issue):

1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **Item 1 — Amber keyswitch labels on the 3 sfizz engines.** BaySickRustyDrums (if any keyswitch kit available) + BaySickGuitars + BaySickBasses piano rolls render keyswitch keys with amber backgrounds + bold dark-amber `sw_label` text + tooltip on hover. Matches the BaySickPlayer Tuba-KS UX from QA-SfzGroup Task 2.
3. **Item 1 — Engine swap.** Switch engine on an Inst tab (BaySickGuitars ↔ BaySickBasses ↔ LiveInput); amber rendering follows correctly (visible on sfizz engines; absent on LiveInput).
4. **Item 1 — Kit reload.** Load another kit on the same engine; amber labels refresh from the newly-parsed keyswitch data.
5. **Item 1 — Non-sfizz regression.** BaySickPlayer Layer/Bass/Drum/Clip tabs still render amber correctly (QA-SfzGroup Task 2 wiring untouched). Harmless / BaySickSynth / BaySickBass piano rolls render unchanged (no false amber).
6. **Item 2 — Round-robin cycling.** BaySickGuitars + BaySickBasses with content that has `<group>`-scoped `seq_length` + `seq_position` produces audible 4-variant rotation when striking the same note at fixed velocity with the keyswitched articulation engaged.
7. **Item 3 — BaySickRustyDrums MT-mode cymbals/hi-hats.** Clean audio (no bit-crusher distortion) when MT is enabled. Kick/snare unchanged. MT-off behavior unchanged from QA-SfzGroup close baseline (i.e. clean both ways).
8. **MT/serial parity.** Toggle MT ON ↔ OFF via Mixer hamburger menu; behavior consistent across all 3 sfizz engines + no other-engine regressions.
9. **No regression on QA-SfzGroup work.** BaySickPlayer Tuba-KS still cycles 4 variants on staccato regions; keyswitching state machine still flips articulations; `sw_label` text still renders on BaySickPlayer piano rolls.

---

## Routing notes (Rule 3 application during execution)

Per Main Plan §0 Rule 3:

- **Findings during execution that touch QA-Sfizz scope** (sfizz / sfizz-driven engines / their piano-roll registration sites) → fold into the current task's scope.
- **Findings that touch a not-yet-started batch's surface in §5** → fold into that batch's scope expansion at QA-Sfizz close (Edit §5 entry + §9 Forks entry).
- **Findings that touch a closed batch's surface** → annotate closed batch's §5 entry with one-line pointer; full details in §9 Forks entry. NO new §5 batch row.
- **Findings with no §5 surface match** → new dedicated §5 batch row + §9 Forks entry. Surface slot/placement to Jeff per `feedback_slot_placement_is_spec_call.md` - do not pick.

**Specific Rule-3 watch-items during QA-Sfizz:**
- Any sfizz vendored-library finding beyond the locked Items 1-3 (e.g. unrelated parser bug, voice-manager bug, etc.) → surface to Jeff for in-batch vs route-out decision per SC-3 authorization scope.
- Any new BaySickRustyDrums / BaySickGuitars / BaySickBasses Processor-level bug surfaced during verify → fold into current task if scope-bounded; route to follow-up batch with §9 Forks entry if broader.
- Pre-existing diagnostic instrumentation surfaced during code-walk (NAMIR / Pedals / Audio Setup state logs / MT QA-Md hamburger menu) → retro-add to catalog with Disposition=Keep per Rule 4.

---

## Carry-Forward Reference touch points

Read at task start:

- **All tasks:** `Carry-Forward Reference.md` §1 (Render Engine Primitives) - confirm MT path + engine processor lifecycle expectations.
- **Task 2A/B:** `Carry-Forward Reference.md` §3 (Mixer/Page Lifecycle) - confirm StandaloneEditor.cpp registration patterns for the 3 sfizz engines.
- **Task 4:** `Carry-Forward Reference.md` §2 (Lock-Free + Lifecycle Primitives) - confirm MT primitives (atomic, seqlock, `mProjectLoadInProgress` barrier) before instrumenting sfizz's internal threading model. Sub-R/S patch supplemented Carry-Forward §2 without contradicting it; Task 4's investigation may surface additional MT primitives that need supplementing.
- **Task 5:** Re-read §2 if implementing a bounded sfizz patch; confirm new patch matches existing MT-primitive conventions (memory orderings, atomic types, etc.).

Carry-Forward contradictions surfaced during execution → record as new entries in `Implemented Work Log.md` per Rule 2 (do NOT edit Carry-Forward).
