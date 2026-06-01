# Running Notes — QA-Sfizz-Followup (linear-fluttering-spark)

> **Purpose:** Append-only running log for the QA-Sfizz-Followup batch. A new entry is appended at every checkpoint — commit landed / sub-task verified / finding captured / spec call resolved / scope pivot — per `feedback_draft_doc_running_notes_every_checkpoint.md` and the Main Plan §0 running-notes rule. At batch close, `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Never edit prior entries; surprises get their own new entry.

> **Pair file:** `Plans & Specs/Batch Plans/linear-fluttering-spark.md` (the batch plan).
> **Conventions:** Main Plan §0 "Document Formatting Conventions" + "Batch Plans + Running Notes layout (locked 2026-05-11)".

## Diagnostic Instrumentation Catalog

Per Main Plan §0 Rule 4. None added — this is a known-root-cause fix (sfizz CC dispatch-at-init), verified via Jeff's audio testing, not log traces. Any temp `DBG` / `juce::Logger` / `AlertWindow` / temp `jassert` added during a verify-fail investigation gets a row here IN THE SAME EDIT PASS, and every `Remove` row is stripped (strip list surfaced to Jeff) before the relevant commit.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| (none yet) | | | |

## 2026-06-01 — Task 0 — open

- Batch opened. Pre-batch ritual: `/standup`; full self-read of Main Plan §0 (Rules 1-5 + Document Formatting Conventions + canonical buckets + Agent Orchestration Rules); targeted extractions (§5 QA-Sfizz-Followup entry + §6 footnote + §9 forty-sixth Forks origin; Work Log QA-EngineApvts close incl. FND-2/FND-4; QA-Sfizz Sub-E history; exemplar `federated-bouncing-cupcake.md`). CLAUDE.md cross-check: "Next Steps -> Next batch: QA-Md" confirmed **stale** vs Work Log (QA-Md closed 2026-05-09; real next = QA-Sfizz-Followup) — relying on Work Log / Main Plan for status.
- **Root cause confirmed in source** (beyond the FND-2/FND-4 notes): the only CC->sfizz path is `parameterChanged -> mSfizz->cc()`, which fires only on an actual APVTS value change. `loadKit`'s reset-to-64 `setValueNotifyingHost` no-ops when the param is already 64 (Guitars `:367-372`); `setStateInformation`'s `replaceState` no-ops when saved==current (Guitars `:425`). So the Sub-E 64 default never reaches sfizz at load -> CC-gated `<master>` articulations sound as if the CC were 0 until moved. Sustain risk of a blanket push verified real: sfizz `Defaults.cpp:75` `sustainCC {64}` + `:79` `checkSustain {true}`.
- **Spec calls locked by Jeff (2026-06-01):** SC-1 all three engines (Guitars + Basses + RustyDrums); SC-2 only kit-exposed controls (no blanket push); SC-3 one consolidated source commit; SC-4 direct `mSfizz->cc()` push (NOT the no-op `setValueNotifyingHost` forced-delta the §5 text suggested).
- **SC-5 grounded by real-kit cross-check:** GUI-XML knob CCs == `label_cc` set exactly for Black&Green Guitars + Black&Blue Basses; Big Rusty Drums `label_cc` is a superset incl. every GUI knob (CC4 hi-hat included). None of the three kits labels CC7/CC10/CC64, so dispatching `mCcLabel` never touches sustain/volume/pan (satisfies SC-2). "Exposed controls" therefore = `mCcLabel` keys, already populated by `loadKit`'s scan -> processor-local fix, no UI->processor plumbing. Resolves the SC-2 residual-risk caveat flagged when posing the spec call.
- Plan written + Jeff-approved (no edits); mirrored to `Batch Plans/linear-fluttering-spark.md` (home-dir copy deleted per `feedback_plan_mirror_one_way.md`); §5 `**Plan file:**` pointer added; this running-notes file seeded.
- **Next:** Task 0 open commit (docs only) after surfacing full git status + `/draft-commit` for approval.

## 2026-06-01 — Task 0 — open commit landed + model side-task

- Task 0 open commit `be6fd7e` (docs only, 3 files, +184/-1). Committed by explicit path; the pre-existing staged `.gitignore` (Jeff's personal `Issue Tracker.txt` ignore) left staged + untouched/excluded. Trailer `Claude Opus 4.8 (1M context)` (Jeff confirmed the 1M variant; re-pinned this session).
- Side-task (Jeff request): pinned the 1M model in `~/.claude/settings.json` (`"model": "opus[1m]"`). Root cause of the per-session revert to plain Opus 4.8: no `model` field set anywhere (user/project/env), so each new session fell back to the app default and `/model` picks were session-only. Also explains the `(1M context)` vs plain trailer alternation across recent batches.
- **Next:** Task 1 — the one consolidated source commit (`dispatchExposedCcsToSfizz()` across the 3 sfizz processors + call sites in `loadKit` + `setStateInformation`); then Jeff builds + verifies Debug then Release.

## 2026-06-01 — Task 1 — source written (awaiting build + verify)

- Source edits applied to all 3 sfizz engines (12 edits = helper impl + 2 call sites + header decl, x3). Self-check grep confirms 3 defs + 3 header decls + 6 call sites (`loadKit` + `setStateInformation` per engine):
  - Guitars: helper `.cpp:56`, loadKit call `.cpp:410`, setState call `.cpp:459`, decl `.h:154`.
  - Basses: helper `.cpp:53`, loadKit call `.cpp:401`, setState call `.cpp:450`, decl `.h:148`.
  - RustyDrums: helper `.cpp:56`, loadKit call `.cpp:665`, setState call `.cpp:1006`, decl `.h:187`.
- Helper body character-identical across the 3 (snapshots each engine's `mCcLabel` keys under `mCcLabelLock`, then pushes `getCcValue(cc)` -> `mSfizz->cc(0, cc, ...)` for each). No new includes (`<vector>` already transitively present via the existing `std::vector` render-ptr members). No diagnostic instrumentation added (Rule 4 catalog stays empty).
- **Next:** Jeff runs `do_build.bat`; verify Debug then Release per the 8-scenario script. On pass -> `/draft-commit` -> the one consolidated source commit.

## 2026-06-01 — Task 1 — VERIFY FAILED -> root-cause pivot (Sub-E blanket-64 default is the real bug)

- **Jeff's Debug build, Test 1 (Black&Green Guitars) FAILED.** With the dispatch helper live, the exposed CCs now reach sfizz at their APVTS value. For the 10 exposed-but-UNSET CCs (Feedback CC29, Muting CC70, Unison CC100, Unison detune CC102, vibratos CC111/112/113/116/117, Tailpiece bends CC118) that APVTS value is **64** (Sub-E's blanket default) = half-ON -> Guitars sounds wrong. Before this batch those CCs were never dispatched, so sfizz held them at **0 (OFF)** = correct. Jeff: "before ... acting as 0 and sounded correct, now its at 64 and actually at 64 and sounds wrong."
- **Root-cause pivot:** the bug is NOT "CCs aren't dispatched" (the batch's original premise / FND-2 framing) -- it's **Sub-E's blanket "default every CC to 64."** The kit author leaves a CC unset to mean "off" (sfizz's natural 0); explicit non-zero defaults come via `set_cc` (Guitars: 27=31 Release vol, 101=127 Unison width, 114=40 Vibrato speed). Sub-E forced unset CCs to 64; the dispatch gap had been harmlessly masking that until this batch made it audible. (Sub-E's "fuller sound" justification was already a documented diagnostic miss in the QA-Sfizz close.)
- **Corrected direction (Jeff confirmed 2026-06-01 "proceed"; manual knob check = 27/101/114 at kit values + the other 10 at 0 sounds right):**
  1. **Default unset CCs to 0** -- revert Sub-E's `64` in all 3 spots/engine (createLayout default, loadKit reset value, getKitDefaultCc fallback). Honor kit `set_cc` as the only non-zero defaults.
  2. **Resolve SFZ `#define` macros** in the loadKit scanner so macro-defined `set_cc` (RustyDrums hi-hat `set_cc4=$ht_lo_hi_init`=127) lands correctly -- was mis-parsed to 0 by `getIntValue("$...")`.
  3. **REMOVE the dispatch helper** added earlier this task (redundant once the default is 0; the "undispatched" premise was the misdiagnosis). Re-test FND-4 "Cool bass riff loads silent"; if it persists, diagnose the restore / `sfizzEngineData` path separately rather than blanket-dispatch.
  4. **Re-verify all 3 engines** so nothing Sub-E guarded regresses.
- **Mechanics:** `git restore` the 6 engine source files (helper never committed; HEAD = Sub-E-64 baseline) -> apply revert-to-0 + `#define` resolution. Scope grew from "dispatch stored CCs" to "correct the CC default model"; same Players bucket; FND-2/FND-4 still the driving symptoms.
- **Next:** apply corrected source -> Jeff builds + re-verifies.

## 2026-06-01 — Task 1 — corrected source VERIFIED (Debug + Release, all 8 scenarios PASS)

- Jeff verified the corrected fix (default unset CCs to 0 + `#define` resolution; helper removed) in Debug then Release: **all 8 re-verify scenarios pass.** Guitars + Basses correct on load with no control touch (unset effects off; `set_cc` CCs at kit values; knobs display the true defaults). RustyDrums correct; hi-hat "Position" defaults to Fully open (CC4=127 via the `#define` resolver, was mis-read as ~0). Cool bass riff (FND-4) loads audible. Another saved sfizz project loads correct. CC move / double-click reset / Full<->Basic program switch all behave; no hung notes; MT == serial.
- **FND-2 + FND-4 both resolved by the default-revert alone** — no dispatch helper, no separate restore-path fix needed.
- Source = 3 `.cpp` files (`git status` shows only BaySickGuitars/BaySickBasses/BaySickRustyDrums Processor.cpp modified). Headers untouched. No diagnostic instrumentation added (Rule 4 catalog stays empty).
- **Next:** surface full git status -> `/draft-commit` -> the one consolidated SOURCE commit (SC-3, 3 `.cpp` by explicit path; docs/plan/running-notes ride the Task 2 close commit). Then Task 2 close.

## 2026-06-01 — Task 1 — SOURCE COMMIT landed (`7695f4e`)

- `7695f4e` — the one consolidated source commit (SC-3): 3 engine `.cpp` (+173/-114, mostly comment rewrites; logic delta = 3 value flips + 1 scanner branch per engine). Committed by explicit path; `.gitignore` (Jeff's) + plan + running notes left uncommitted (docs ride the close). Trailer `(1M context)`. `/draft-commit` -> surfaced -> Jeff approved; one factual correction applied to the drafter text before commit ("four" -> "three" value flips, per `feedback_drafter_output_verbatim_no_restyle.md` factual-correction allowance).
- **Next:** Task 2 close — `/draft-doc batch-close` -> apply Work Log -> `/review-batch` -> address -> Rule-3 routing (in-batch Sub-E-64 revert + `#define` fold; annotate QA-Sfizz §5; §9 Forks close entry; §5 STATUS:CLOSED; §6 next = QA-Ed) -> `/draft-commit` close -> commit.

## 2026-06-01 — Task 2 — CLOSE

- `/draft-doc batch-close` -> Work Log close entry compiled + applied (append after the QA-EngineApvts entry). `/review-batch QA-Sfizz-Followup` -> **READY-TO-COMMIT**, no BLOCKER / no NEEDS-FIX; **1 style NIT** (`int resolved;` initializer in the `$`-resolution, all 3 engines — recorded, NOT fixed, to avoid re-touching the verified source for a non-functional change) + **1 informational note** (a kit `set_cc<N>=0` no longer dispatches an explicit 0 now that the baseline is 0; harmless — sfizz's natural default is already 0 and no shipping kit uses `set_cc=0`).
- Rule-3 routing applied: FND-2 + FND-4 resolved in-batch (the default-revert); `#define`-mis-parse fixed in-batch (new mid-batch finding); QA-Sfizz §5 annotated (Sub-E reverted-to-0, completed-batch pointer); §9 forty-seventh Forks close entry; §5 QA-Sfizz-Followup STATUS:CLOSED banner; §6 footnote CLOSED note.
- Rule 4: no diagnostic instrumentation added this batch (catalog empty; nothing to strip).
- **Next:** `/draft-commit` close -> surface message + git status for Jeff approval -> close commit (docs only, separate from source `7695f4e`). QA-Ed is next per §6.
