# QA-Soundness — Whole-codebase soundness audit (silent failures, dead code, lying comments, audio-thread + threading, lifetime + resources, persistence round-trip) — Plan (keen-combing-heron)

> **Canonical path:** `Plans & Specs/Batch Plans/keen-combing-heron.md`. **For execution:** bulk-run
> G4 batch **9 of 9 — runs LAST**, after `clean-pointing-stoat`, before the G4 boundary R3 review +
> smoke. §B authored at code-complete; one source commit.

## Context

Jeff's call 2026-07-25, at QA-Export code-complete: G4 is the last coding group, so before it closes
he wants a whole-codebase pass confirming everything is sound rather than shipping on the assumption
that per-batch work caught everything.

**The case for it is empirical, not theoretical.** Two G4 batches of ordinary work turned up all of
the following *incidentally* — none of it was being looked for:

| Found | Where | Class |
|---|---|---|
| `registerParamsForTrack` + 4 engine helpers — whole param family, zero readers | PluginProcessor | dead registration |
| `addParamsForEffectRack` — 6 slots x 15 params PER TRACK, zero readers | PluginProcessor | dead registration |
| Menu items 120/121 with no dispatch case — silent no-ops | StandaloneEditor | dead UI |
| `doNew`/`doSave`/`doOpen`/`doExport` stub quartet, zero callers | BuilderPage:7718 | dead code |
| "lfo_rate / lfo_shape ripped" — both params live, consumed, attached | HarmlessEditor:538 | **lying comment** (cost 2 knobs their automation) |
| CLAUDE.md wrong about `oeq_mix` (claimed absent; present since 2026-04-19) | CLAUDE.md | lying doc |
| "invisible by never being addAndMakeVisible'd" — it IS added | HarmlessEditor:293 | lying comment |
| InstPage.h claims Vox hosts NAM/IR as Inst does — it does not | InstPage.h:19 | lying comment |
| Song-end math duplicated between transport + export | StandaloneEditor / BuilderPage | drift risk |
| Pedals state tag colliding with the APVTS child — worked by tree-order accident | BaySickPedalsProcessor | latent data bug |
| `EffectType` ordinals implicit — one insertion from repointing every saved slot | EffectRack.h | latent data bug |
| Export job could outlive the Builder tab that launched it | BuilderPage | lifetime |
| Automation registry grows on tab churn, no unregister | StandaloneEditor | unbounded growth |
| Missing external files skipped in silence — **6 engines, 10 sites** | NAM pedal / Guitars / Basses / RustyDrums / NAMIR Mic A+B | **silent failure** |
| `loadUserIr` error captured and DISCARDED — corrupt IR failed silently | BaySickNAMIRProcessor | **swallowed error** |

Every silent-failure instance shared one signature: **the app looks like it loaded correctly and
produces nothing.** That is the failure mode a user cannot diagnose and cannot report usefully.

**Pre-measured surface** (greps run 2026-07-25, before this plan):

- `juce::String err;` — **29 sites / 11 files**. Each captured an error that may be discarded.
- `juce::String& outErr` — **27 declarations / 13 files**. The functions that CAN report failure.
- `existsAsFile()` — **150 sites / 38 files**. Most legitimate; the risky subset is a guard wrapping
  a `load*()` with no `else`.
- Empty `catch` blocks — **zero**. Searched; that class does not exist here. Not a task.

- **Risk:** medium. No new features; the risk is (a) churn across many files late in the group, and
  (b) an open-ended fix list. Mitigated by adversarial verification before anything is called a
  finding, and a build gate per task.
- **Effort:** **genuinely unknown, and the plan says so rather than inventing a number.** The sweep is
  bounded; the fix count is not. Expect multiple sessions.
- **Dependencies:** runs after every other G4 batch so it audits the final state of the group's code,
  including this group's own changes.

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| Jeff 2026-07-25 (1=a) | **Findings are FIXED IN-BATCH — all of them.** Not a tiered subset, not report-and-route | G4 is the last coding group; a routed list would ship unsound |
| Jeff 2026-07-25 (2=b) | Scope = `Source/` + `CMakeLists.txt` + build config. **Vendored `libs/` excluded** | Third-party internals we do not own |
| Jeff 2026-07-25 (3=a) | **Multi-agent sweep**, one agent per category, findings adversarially verified before being believed | ~40 source areas; an inline pass would miss things and I said so |
| Jeff 2026-07-25 | Slot: **9th G4 batch, runs LAST**, before the boundary R3 + smoke | Catch anything needing fixing before G4 closes |
| — | Silly-name `keen-combing-heron` | Mine per convention |

**Open-ended by construction (consequence of 1=a, stated so it is not a surprise):** the batch ends
when the sweep comes back clean, not on a task count. Task 8 is a re-sweep for exactly this reason.

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open. The three that existed were surfaced in chat and answered before this body
was written (Rule 5); their answers are the locked table above.

## Files to modify

**Not enumerable in advance — that is the nature of the batch.** The audit SURFACE is every file
under `Source/` plus `CMakeLists.txt`. Files actually modified are whatever the verified findings
touch, and the running notes record each one as it is fixed.

Known-in-advance starting points, from the pre-measured surface:

- Task 1: the 29 `juce::String err;` sites (concentrations: `BaySickNAMIREditor.cpp` 8,
  `BaySickPedalsEditor.cpp` 4, `SlotComponent.cpp` 3) + the `existsAsFile()` load-guard subset
- Task 2: `Source/Standalone/BuilderPage.cpp:7718-7736` (the stub quartet, already logged by QA-Export)
- Task 3: `Source/Harmless/HarmlessEditor.cpp:293`, `Source/Inst/InstPage.h:19` (both already
  identified as lying), plus whatever the sweep finds

## Tasks

Every task follows the same shape, and it is the shape that makes a sweep this size trustworthy:

```
sweep (agents, one per sub-category)
  -> adversarially VERIFY each finding before believing it
  -> fix the confirmed ones
  -> build gate
  -> record in running notes (including anything REFUTED, and why)
```

**Verification is not optional.** An agent finding is a lead, not a fact
(`feedback_verify_subagent_finding_premise`). A sweep this wide will generate plausible-but-wrong
findings, and fixing a non-bug is worse than missing a real one — it churns working code.

### Task 1 — Silent failures + swallowed errors

- [ ] Sweep the 29 `juce::String err;` sites: is the error USED after the call, or dropped? Every
  drop is a finding.
- [ ] Sweep the 27 `outErr`-taking functions: does every caller check the return?
- [ ] Filter the 150 `existsAsFile()` sites to the risky subset — a guard wrapping a `load*()` with
  no `else` branch. That exact shape produced all 10 sites QA-Export fixed.
- [ ] Sweep for other silent-skip shapes the above misses: `if (ptr) doThing();` with no else on a
  path the user expects to act, early `return` on a failed precondition with no signal.
- [ ] Verify each finding, then fix: report through `MissingFileReport` where it is a missing
  external file; surface an error to the user where an operation genuinely failed.
- [ ] Build gate.

### Task 2 — Dead code + dead registrations

- [ ] Sweep for zero-caller functions, zero-reader parameter families, menu items with no dispatch
  case, and unreachable branches.
- [ ] For each: confirm zero references tree-wide before deleting — a single missed caller is a
  build break, and a `#if`-guarded caller will not show in a naive grep.
- [ ] Fix the QA-Export-logged stub quartet (`doNew`/`doSave`/`doOpen`/`doExport`,
  `BuilderPage.cpp:7718-7736`) as part of this task rather than leaving it routed.
- [ ] Distinguish DEAD from NOT-YET-WIRED. Some stubs are deliberate scaffolding for later work;
  those keep a `HOLD-FOR-<reason>` comment (Rule 6 keeper category 1) instead of being deleted.
  **When ambiguous, ask — do not delete on a judgement call.**
- [ ] Build gate.

### Task 3 — Comment + doc truth audit

- [ ] Sweep for comments that contradict the code they describe. This class caused real damage:
  two visible Harmless knobs had no automation for months because a comment said their params were
  "ripped" when they were live, consumed and attached.
- [ ] Fix wrong comments **wherever they live**, not only in edited regions
  (`feedback_no_docs_only_commit_fix_wrong_comments` — Rule 6's edited-regions scoping governs STYLE
  audits, not factual errors).
- [ ] Include `CLAUDE.md` in the sweep — it was wrong about `oeq_mix`, and a wrong entry there
  propagates into every future session's assumptions.
- [ ] Known starting points: `HarmlessEditor.cpp:293` (addAndMakeVisible claim), `InstPage.h:19`
  (Vox/NAM-IR hosting claim).
- [ ] Build gate.

### Task 4 — Audio-thread safety + threading discipline

- [ ] Sweep `processBlock` and everything it reaches for allocation, locking, file IO, or unbounded
  work. This is the one class whose symptom is dropouts rather than wrong behavior, so it does not
  show up as a bug report — it shows up as "the app is glitchy."
- [ ] Sweep for functions documented message-thread-only being called from elsewhere, and for
  shared state read/written across threads without an atomic or lock.
- [ ] Cross-check against the existing `performance-auditor` agent's remit so this does not
  duplicate `/perf-audit` — this task owns CORRECTNESS (is it safe), that agent owns SPEED.
- [ ] Verify every finding against `Carry-Forward Reference` §2 audio-thread rules before fixing;
  several patterns here are deliberate and documented (seqlock, atomic snapshot + retirement queue).
- [ ] Build gate.

### Task 5 — Lifetime, ownership, resources, unbounded growth

- [ ] Sweep async callbacks and lambdas for captured `this` / raw references that can outlive their
  owner. QA-Export found one: an export job could outlive the Builder tab that launched it.
- [ ] Sweep registries and long-lived containers for entries that are added and never removed. The
  automation registry was one — it grows on tab churn and needed an explicit prefix erase.
- [ ] Sweep file/stream handling for handles held across a delete or rename. QA-Export hit the
  Windows case where an open handle blocks deletion of the file being written.
- [ ] Build gate.

### Task 6 — Persistence round-trip completeness

- [ ] For each engine and DSP with `getStateInformation`/`setStateInformation`: does every field
  that can be SET actually save AND restore? A field that saves but never loads is invisible until
  someone reloads a project.
- [ ] Sweep for enum ordinals persisted as raw ints without explicit values (the `EffectType` class
  of bug — one insertion repoints every saved slot). QA-Verify pinned that one; find the rest.
- [ ] Resolve QA-Verify's carried finding: EQ / Tuner pedal serializer field completeness, still
  unverified by desk-read.
- [ ] Note but do NOT act on version fields written and never read (`kStateVersion`) — they are the
  hook a real migration would use.
- [ ] Build gate.

### Task 7 — UI/state divergence + duplicated logic drift

- [ ] Sweep for controls that DISPLAY something different from what is stored or loaded. The NAM
  pedal showing a capture name it never loaded is the archetype, and it read as "working."
- [ ] Sweep for the same computation implemented in two places. Song-end math was one (now shared);
  two copies of anything is a drift risk, and the drift is silent.
- [ ] Build gate.

### Task 8 — Re-sweep to clean

- [ ] Re-run every category sweep over the POST-FIX tree. Fixes introduce their own findings, and a
  single pass would miss anything the earlier fixes created.
- [ ] Repeat until a sweep round produces zero new confirmed findings. **This is what "the batch ends
  when it is clean" means in practice** (consequence of 1=a).
- [ ] Build gate.

## Batch close (bulk-run per-batch loop — one commit per batch)

- [ ] Build both configs clean.
- [ ] Author this batch's Master Test Plan §B section from the Verification list below.
- [ ] `/draft-doc batch-close` -> append under `## Held Work Log entry (apply at section pass)` in
  the running notes. Do NOT touch the Implemented Work Log or the §5 STATUS line now (R2).
- [ ] Append the running-notes code-complete entry, including the **full findings ledger** — every
  confirmed finding, every REFUTED one and why, and every file touched.
- [ ] Add this batch's items to the **PENDING Main Plan edits** ledger (deferred to G4 close per
  Jeff's standing instruction 2026-07-25), including a §5 entry for QA-Soundness itself, which does
  not exist yet.
- [ ] ONE batch commit (Rule 9): `QA-Soundness: <one-line what> (<scope>)` + `Co-Authored-By`
  trailer; surface message + FULL git status; commit only on Jeff's approval.

## Verification (authors into Master Test Plan §B)

A soundness audit is mostly invisible when it works, so §B focuses on **not having broken anything**
plus spot-checks of the classes that now behave differently:

1. Regression: the app launches, loads a large existing project, plays, saves, reloads — Debug first,
   then Release.
2. Regression: every surface touched by a fix gets a targeted check, listed per-fix at §B authoring
   from the findings ledger.
3. Silent-failure fixes: for each newly-reporting site, break the file deliberately and confirm the
   report fires and names it correctly.
4. Dead-code deletions: confirm nothing that was deleted was reachable — each deletion's surface
   still behaves as before.
5. Audio-thread fixes: play a heavy arrangement and confirm no new dropouts vs the pre-batch
   baseline, at 64 / 256 / 1024 buffer sizes.
6. Persistence fixes: save/reload round-trip on each engine whose serializer changed.

## Routing notes (Rule 3)

- **Findings are fixed in-batch (1=a), so routing is the exception, not the rule.** Anything NOT
  fixed needs an explicit reason recorded in the running notes at the moment the decision is made —
  never a silent skip (no-silent-caps).
- A finding that turns out to need a genuine feature change rather than a fix goes to Jeff as a spec
  call when found, not batched to close.
- Findings in vendored `libs/` are OUT OF SCOPE (2=b) but get logged if spotted, since they still
  ship in the binary.

## Carry-Forward Reference touch points

§2 audio-thread rules before Task 4 — several patterns that look wrong are deliberate and documented
there (seqlock, atomic snapshot + retirement queue), and "fixing" them would be a regression. §1
architectural primitives before Tasks 5 and 7. The G4 batches' own running notes before Task 8, so
the re-sweep knows what this group already changed.

## Interaction with the G4 boundary R3 review

R3 reviews the G4 **diff**; this batch reviews the whole tree, including everything R3 would see.
Running R3 afterwards is still worth it — it is a different lens, focused on plan-conformance rather
than soundness — but **R3 should run AFTER this batch's commit**, not before, so it also covers what
this batch changed. Sequencing note for the boundary: QA-Soundness commit -> R3 -> smoke -> carry-over.

## Conflict-review note — 2026-07-27 (QA-ModelShell inserted upstream)

**QA-ModelShell** (`grand-inverting-mammoth.md`) now runs between QA-ProjectSave and
QA-UndoCoverage; this batch STILL RUNS LAST (G4 order: … badger → mammoth → yak → stoat →
heron), which strengthens its premise — it audits the post-inversion, post-shell tree.

1. **Re-measure the pre-measured surface at open.** The 2026-07-25 counts (29 err-string
   sites, 27 outErr functions, 150 existsAsFile) predate the largest churn of the QA era.
2. **Ownership dedupe with deep-packing-badger Task 9:** the HarmlessEditor.cpp:293 comment,
   the InstPage.h:19 hosting claim, and the `kStateVersion` decision are ALSO badger Task 9
   items. Check badger's close state before sweeping these — no double-fix, no double-claim.
3. **Stale context-table motivator:** "automation registry grows on tab churn, no
   unregister" was FIXED by badger Task 7 step 1 (self-cleaning registry). Do not chase.
4. **Task 4 scope additions from mammoth:** the offline render thread (drives the live graph
   with the device suspended), the native-child window shell, VST3 hosting threads, and the
   BLU-302 sandbox process are new threading surfaces this sweep must cover.
5. **Boundary LOCKED (Jeff 2026-07-27):** the G4 R3 review + smoke covers only
   yak/stoat/heron; mammoth is verified by its own per-task-set commits + TS8 batch smoke.
   This batch's "R3 runs AFTER this commit" sequencing note stands.
