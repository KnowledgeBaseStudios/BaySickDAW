# QA-Sfizz-Followup — sfizz CC dispatch-at-init (push the kit's exposed CCs into sfizz at kit/project load) — Plan (linear-fluttering-spark)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/linear-fluttering-spark.md`
> Paired running notes: `Plans & Specs/Running Notes/linear-fluttering-spark.md`

> **For execution:** steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in the Debug exe FIRST, then Release (CLAUDE.md Build System standing rule). One consolidated source commit (Jeff-locked SC-3), so Task 1 is the single source change across all three engines.

---

## Context

**Why this batch:** QA-EngineApvts verify (Jeff, 2026-05-31) surfaced FND-2 + FND-4, routed out per §0 Rule 3 to this new batch (Bucket: Players; slotted immediately after QA-EngineApvts, before QA-Ed; §9 forty-sixth Forks entry). **Pre-existing bug from QA-Sfizz Sub-E — NOT a QA-EngineApvts regression.**

**The bug (root cause confirmed in source):** QA-Sfizz Sub-E (`f477e39`, 2026-05-28) defaulted every engine APVTS CC param to **64** (the Aria-host convention the Karoryfer kits are authored around) for the three sfizz-driven engines. But that value is **never dispatched into the sfizz instrument at load**. The only path that pushes a CC into sfizz is `parameterChanged -> mSfizz->cc(0, cc, v)`, which fires *only when an APVTS value actually changes*:

- `loadKit` resets every CC to 64 via `setValueNotifyingHost` (Guitars `:367-372`) — but writing 64 to a param already at 64 is a **no-op**, so the listener never fires and sfizz keeps its internal value (0). The comment even *claims* "The reset hits sfizz too via setValueNotifyingHost -> parameterChanged" — that claim is the bug.
- `setStateInformation` does `loadKit` then `apvts.replaceState` (Guitars `:417-425`). Saved CC values that equal the current value (e.g. both 64) also don't fire `parameterChanged` -> sfizz never receives them -> **FND-4 "Cool bass riff loads silent."**

Net user symptom: a CC-gated `<master>` articulation behaves as if the CC were **0** until the user physically moves the control (BaySickGuitars / BaySickBasses / BaySickRustyDrums).

**The fix (Jeff-locked SC-4):** add a small processor-local helper that pushes the kit's **exposed** CC controls' current APVTS values **directly** into sfizz (`mSfizz->cc(0, cc, getCcValue(cc))`), bypassing the no-op APVTS gate — called at the end of `loadKit` and after `replaceState` in `setStateInformation`. The §5-suggested "generalize the `setValueNotifyingHost` forced-delta pattern" is explicitly NOT used — that path *is* the no-op that causes the bug.

**Intended outcome:** on a fresh kit pick and on project load, CC-gated articulations sound correct immediately with no control touch; the "Cool bass riff" project plays audibly; no stuck notes / no unexpected sustain (generic controllers are never asserted).

**Risk:** medium. Touches the sfizz CC dispatch on the load path. Mitigated: the change is purely additive, processor-local, and runs on the **same thread (message thread) in the same call sites** as the existing `loadKit`/`replaceState` dispatch — no new concurrency surface. The MT bit-crusher race for RustyDrums was already cured at QA-DispatcherAffinity.

**Effort:** ~2-4 hours (per §5 estimate), dominated by Jeff's per-engine verify cycle.

**Dependencies:** none. QA-EngineApvts closed (`0db22f3`).

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC-1 | **Engine scope = all three** (BaySickGuitars + BaySickBasses + BaySickRustyDrums). | Jeff 2026-06-01. RustyDrums has the identical CC architecture + identical load-time bug (its hi-hat pedal CC4 gate never reaches sfizz at load). "Leaving known, identical bugs in parallel modules creates technical debt." |
| SC-2 | **CC dispatch breadth = only the kit's UI-exposed controls.** | Jeff 2026-06-01. A blanket push would send CC64=64 (sfizz sustain pedal — `Defaults.cpp:75`, `checkSustain=true`) -> stuck notes, and CC7=64 -> volume cut. "The DAW must only assert state over the specific macro and articulation parameters the UI explicitly exposes for that kit." |
| SC-3 | **Commit structure = one consolidated source commit** across all three engines. | Jeff 2026-06-01. "One logical architectural fix applied across the sfizz family." Separate files (no shared header), but Jeff chose a single commit. |
| SC-4 | **Dispatch mechanism = direct `mSfizz->cc(0, cc, value)` push**, NOT the `setValueNotifyingHost` forced-delta. | Jeff 2026-06-01. The forced-delta path is exactly the no-op that *is* the bug; a direct push is robust and has identical audible result. "Bypassing the APVTS gate to push directly to the engine via mSfizz->cc(...) during initialization is the correct architectural move." |
| SC-5 | **"Exposed controls" = the kit's `label_cc` set (`mCcLabel`)**, each dispatched at its current APVTS value. | Grounded by direct kit cross-check (2026-06-01), not a preference. For Black&Green Guitars + Black&Blue Basses, the ARIA GUI-XML knob CCs == the `label_cc` set **exactly**; for Big Rusty Drums, `label_cc` is a superset that includes every GUI knob (incl. CC4 hi-hat). None of the three kits labels CC7/CC10/CC64, so dispatching `mCcLabel` **never** touches sustain/volume/pan — satisfies SC-2 precisely. `mCcLabel` is already populated by `loadKit`'s scan, so the fix is processor-local with no UI->processor plumbing. This resolves the residual-risk caveat I flagged on the SC-2 option ("could miss a gating CC the kit references but doesn't label"): confirmed not the case for the shipping kits. |
| SC-6 | **Silly-name = `linear-fluttering-spark`** (adopted the plan-mode runtime assignment). | My pick per `feedback_silly_name_is_my_pick.md`; adopting the assigned name keeps plan/running-notes/mirror filenames consistent. |
| SC-7 | **Verify cadence = Debug-then-Release per task** (Jeff drives `do_build.bat`). | Standing rule (CLAUDE.md Build System; `feedback_no_full_release_reverify_at_batch_close.md`). |

---

## Sub-spec calls surfaced for ExitPlanMode

**None open.** All decisions resolved before the plan body was written: SC-1/SC-2/SC-3 locked by Jeff in chat (2026-06-01); SC-4 (mechanism) confirmed by Jeff; SC-5 (exposed-control definition) grounded empirically against the three shipping kits, collapsing what looked like an A/B fork into a single forced answer. Per §0 Rule 5, nothing here is a pre-picked recommendation baked into the plan.

---

## Files to modify

**Task 1 — the single consolidated source commit (6 files: 3 processors + 3 headers).**

The helper body is **character-identical** across all three engines (it calls each engine's own `getCcValue` + reads each engine's own `mCcLabel`/`mCcLabelLock`/`mSfizz` members). Per-engine call-site anchors:

- [Source/BaySickGuitars/BaySickGuitarsProcessor.h](Source/BaySickGuitars/BaySickGuitarsProcessor.h) — add private decl `void dispatchExposedCcsToSfizz();` (near `updateFromApvts()` `:150`).
- [Source/BaySickGuitars/BaySickGuitarsProcessor.cpp](Source/BaySickGuitars/BaySickGuitarsProcessor.cpp) — add helper impl; call it in `loadKit` immediately before `return true;` (`:384`, after the kit-defaults loop `:377-382`) and in `setStateInformation` immediately after `apvts.replaceState (apvtsState);` (`:425`).
- [Source/BaySickBasses/BaySickBassesProcessor.h](Source/BaySickBasses/BaySickBassesProcessor.h) — add private decl.
- [Source/BaySickBasses/BaySickBassesProcessor.cpp](Source/BaySickBasses/BaySickBassesProcessor.cpp) — impl; `loadKit` before `return true;` (`:375`); `setStateInformation` after `replaceState` (`:416`).
- [Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h) — add private decl.
- [Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp](Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp) — impl; `loadKit` before `return true;` (`:638`, after the kit-defaults loop `:624-629` + the multi-out scratch resize `:631-637`); `setStateInformation` after `replaceState` (`:971`).

No other files change. No APVTS params added (the CC params already exist). No editor/UI changes. No CMake changes.

---

## Tasks

### Task 0 — Open commit (docs only)
- [ ] Mirror `~/.claude/plans/linear-fluttering-spark.md` -> `Plans & Specs/Batch Plans/linear-fluttering-spark.md` (Write tool); delete the home-dir copy (`feedback_plan_mirror_one_way.md`).
- [ ] Add the `**Plan file:**` pointer to the §5 QA-Sfizz-Followup entry (`Main Plan.md:1394`), backticked-path form matching the other §5 entries.
- [ ] Seed `Plans & Specs/Running Notes/linear-fluttering-spark.md` per the §0 running-notes required sections (title / purpose blockquote / pair ref / convention ref / Task 0 open entry).
- [ ] Surface full `git status` (every dirty + untracked entry, incl. the pre-existing staged `.gitignore` — propose disposition). Dispatch `/draft-commit`. Surface drafted message + status to Jeff for approval. Commit on approval (docs only — clean rollback boundary per §0).
- [ ] Mark Task 0 done.

### Task 1 — sfizz CC dispatch-at-init (the one consolidated source commit)

**The helper (identical in all three processors; ASCII-only comment per the standing rule):**

```cpp
void BaySickGuitarsProcessor::dispatchExposedCcsToSfizz()
{
    if (! mSfizz) return;

    // Push the kit's UI-exposed (label_cc) CC controls straight into sfizz at
    // their current APVTS value.  Deliberately bypasses the parameterChanged
    // path: setValueNotifyingHost() is a no-op when the value is unchanged, so
    // the Sub-E 64 default (and any saved value that equals the current one)
    // never reaches sfizz at load -> CC-gated <master> articulations sound as
    // if the CC were 0 until the control is moved.  Labeled CCs are exactly the
    // kit's exposed control surface (the ARIA GUI knobs), so this asserts state
    // only over real macro/articulation controls -- never generic controllers
    // (sustain CC64 / volume CC7 / pan CC10), which these kits never label.
    std::vector<int> exposed;
    {
        const juce::SpinLock::ScopedLockType lk (mCcLabelLock);
        exposed.reserve (mCcLabel.size());
        for (const auto& entry : mCcLabel) exposed.push_back (entry.first);
    }
    for (const int cc : exposed)
        mSfizz->cc (0, cc, getCcValue (cc));
}
```

- [ ] **BaySickGuitars:** add private decl to the header; add the helper impl to the `.cpp`; insert `dispatchExposedCcsToSfizz();` before `return true;` in `loadKit` and after `apvts.replaceState (apvtsState);` in `setStateInformation`.
- [ ] **BaySickBasses:** same three edits (helper body identical; uses `mCcParamRoot`-based `getCcValue` internally).
- [ ] **BaySickRustyDrums:** same three edits (helper body identical; uses `brd_cc`-based `getCcValue` + the same `mCcLabel`/`mCcLabelLock` members).
- [ ] Confirm no diagnostic instrumentation added (this is a known-root-cause fix, not a hunt). If any temp `DBG`/`jassert`/log is added during debugging, add a `## Diagnostic Instrumentation Catalog` row in the running notes IN THE SAME EDIT PASS (§0 Rule 4) and strip at task close.
- [ ] **Tell Jeff:** "Run `do_build.bat`. Verify in **Debug** first, then **Release**:
  1. New BaySickGuitars Inst tab -> pick the Black&Green Guitars kit. WITHOUT touching any knob, play a few notes / a chord. The articulation should sound correct immediately (the way it currently only sounds *after* you wiggle a control). (FND-2, Guitars.)
  2. New BaySickBasses Inst tab -> pick Black&Blue Basses. Same check: correct articulation on first notes, no control touch. (FND-2, Basses.)
  3. New BaySickRustyDrums tab -> pick Big Rusty Drums. Play the kit without touching controls; confirm it sounds right. NOTE: the hi-hat default (CC4) now reaches sfizz at its Aria 64 (half-open) value instead of staying at sfizz's 0 (closed) — confirm the default hi-hat sound is acceptable; flag if you'd expect closed-by-default.
  4. Open the saved `Projects/Cool bass riff` (BaySickBasses keyswitch kit). Confirm it now plays audibly (not silent). (FND-4.)
  5. Open one other saved sfizz project — confirm its articulations sound correct on load with no control touch.
  6. Regression: move a CC knob (should still take effect); double-click a knob to reset (should still work); switch kit programs (Full/Basic on Rusty) — no leaked CC values, no stuck/sustained notes.
  7. Confirm no hung/sustained notes anywhere (proves we did not blanket-push CC64).
  8. Both MT (production default) and 1-worker serial-diagnostic mode behave identically."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface full `git status`; dispatch `/draft-commit`; surface drafted message + status to Jeff for approval; commit the **one consolidated source commit** on approval (commit body uses `BaySickPlayer`-style brand names; file paths in the diff are unavoidable).
- [ ] Dispatch `/draft-doc running-notes`; apply to the running-notes file.

### Task 2 — Close (docs only, separate commit)
- [ ] `/draft-doc batch-close` — compile the Implemented Work Log entry from the running notes.
- [ ] Review the draft; apply to `Plans & Specs/Implemented Work Log.md` via Edit (append-only; `**Bucket:** Players` line).
- [ ] `/review-batch QA-Sfizz-Followup` — audit the diff vs this plan + CLAUDE.md rules + memory gotchas. Address BLOCKER/NEEDS-FIX; record NITs in the close entry.
- [ ] Route side findings per §0 Rule 3: resolved-in-batch -> close-entry routing table; outside-batch -> §9 Forks entry + §5/§6/Future State edits (surface placement options to Jeff; don't pick the slot).
- [ ] Apply close paperwork: §5 QA-Sfizz-Followup `STATUS: CLOSED` banner; §9 Forks close entry; §6 arrow position note if needed.
- [ ] Surface full `git status`; `/draft-commit`; surface for approval; commit the close (separate from the source commit — clean rollback boundary).

---

## Verification (end-to-end smoke)

After Task 1 lands, the full smoke is scenarios (1)-(8) under Task 1 above. Pass criteria:
- Fresh kit pick on each of the three engines -> correct CC-gated articulation with zero control interaction (FND-2 across the sfizz family).
- `Cool bass riff` + at least one other saved sfizz project load audibly with correct articulations (FND-4).
- No stuck/sustained notes anywhere; volume/pan unaffected at load (proves SC-2 — only labeled CCs dispatched, never CC64/CC7/CC10).
- Existing CC interactions (knob move, double-click reset, program switch) still behave.
- MT and serial-diagnostic identical.

`grep` self-check at close: three new `dispatchExposedCcsToSfizz` definitions + three header decls + two call sites per engine (six call sites total).

---

## Routing notes (Rule 3 application during execution)

- **Hi-hat CC4 default change (RustyDrums):** dispatching CC4 at its Aria-64 (half-open) value is the direct consequence of SC-1 + SC-2 (CC4 is a labeled/exposed control). If Jeff's verify shows the default should be closed (0) instead, that's a kit-default decision (his call) — handle in-batch if small, else route per Rule 3. Flagged in verify scenario (3).
- **A kit needing CCs beyond its `label_cc` set:** not the case for the three shipping kits (SC-5 cross-check), but if a future/third-party kit gates an articulation on an unlabeled CC, that's a scope extension — surface to Jeff, route per Rule 3 (do not silently widen to a blanket push, which SC-2 rejected).
- **`/review-batch` NITs:** record in the close entry (do not bulk-defer; `feedback_closed_batch_carryforward_via_forks.md`).
- Real bugs found mid-batch get fixed in-batch by default (`feedback_qa_batches_fix_bugs_dont_defer.md`).

---

## Carry-Forward Reference touch points

The Carry-Forward Reference is frozen at 2026-05-07, predating the sfizz engines (Phase J BaySickRustyDrums / Phase K BaySickGuitars / Phase L BaySickBasses), so it has no directly applicable section for this surface. The authoritative architecture context lives in the **QA-Sfizz Work Log close entry** (Implemented Work Log.md, 2026-05-28 — Sub-E CC=64 default; FND-5 Aria-host convention) and this plan. No Carry-Forward contradiction is expected (the tracker/dirty-flag work from QA-EngineApvts is orthogonal to CC dispatch); if one surfaces, record it as a new Work Log entry per §0 (never edit Carry-Forward).
