# QA-C — Tiny One-Liners — Plan (cozy-mend-ferret)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/cozy-mend-ferret.md`
>
> Plan-mode UI file at `~/.claude/plans/compressed-foraging-starfish.md`
> deleted on mirror per `feedback_plan_mirror_one_way.md`.
>
> **Pair file (running notes):** `Plans & Specs/Running Notes/cozy-mend-ferret.md`
> (seeded on first mid-batch checkpoint).

---

## Context

QA-C is the closing batch of Phase 1 in the post-Batch-10 QA cycle.  Two
independent low-risk fixes:

- **DSP-10** — the idle-suspend dispatcher's wake predicate
  (`midiEmpty && noVoices`) is missing the `!auditionPending` term that
  the dispatcher comment promised.  When a sfizz-Inst tab or
  BaySickRustyDrums chain has been silent for >=200ms, audition notes
  arriving through `auditionNote(int)` go silent because the chain is
  skipped before `processBlock` can `mAuditionNote.exchange(-1)` and
  trigger the note.

- **MIX-01** — Vox-tab `onTabClosed` branch unregisters the audio engine
  but never calls `removeVoxChannel` on the mixer page, leaving the strip
  widget orphaned in `mVoxStrips`.  Mirror of the 2026-05-05 Inst-tab
  fix at `StandaloneEditor.cpp:3631-3632`.

Locked spec calls (from pre-batch session):
- **A2** — fix DSP-10 at all 4 sites (MT-path Inst + Rusty, serial-path
  Inst + Rusty), not just the MT-path pair §5 cites.
- **B1** — silly-name `cozy-mend-ferret`.
- **C2** — one source commit + close commit (2 total).
- **D1** — audible audition test for verification (no temporary
  diagnostic instrumentation).
- **Accessor name** — `isAuditionPending() const noexcept -> bool`,
  matching dominant codebase convention (`isActive`, `isRecording`,
  `isProcessingEnabled`, `isLocked`, etc).

---

## Findings to apply

### DSP-10: 4 idle-suspend predicate sites

All four sites have identical shape `if (midiEmpty && noVoices) { ... }`
guarding chain skip-and-suspend.  Need to add `!auditionPending` term.

| # | Path | Engines | Active when |
|---|---|---|---|
| (1) | `Source/Engine/Tasks/InstStripTask.cpp:115-119` | BaySickGuitars + BaySickBasses | MT engine on (default) |
| (2) | `Source/Engine/Tasks/RustyDrumsProducerTask.cpp:35-43` | BaySickRustyDrums | MT engine on (default) |
| (3) | `Source/PluginProcessor.cpp:2272-2292` | BaySickGuitars + BaySickBasses | MT engine off |
| (4) | `Source/PluginProcessor.cpp:2032-2045` | BaySickRustyDrums | MT engine off |

Per Carry-Forward §1: "Missing: `auditionPending = eng->mAuditionNote.load() != -1` check.  Idle-suspend dispatcher comment promises 'audition' as a wake condition — contract specified but not implemented."

### Engine accessor gap

`mAuditionNote` is a private `std::atomic<int>` member on all 7 engines
that have one.  No public read accessor exists; only `auditionNote(int)`
setter.  QA-C needs to add `isAuditionPending()` to the 3 sfizz engines
the predicates reference (Guitars / Basses / RustyDrums).

Decision: only add to the 3 engines QA-C touches.  Adding to the other 4
(BaySickSynth / BaySickBass / Harmless / VibePlayer) is parity-only with
no functional need; deferred (no §9 entry — trivially recoverable if
ever needed).

### MIX-01: Vox close path

At `Source/Standalone/StandaloneEditor.cpp:3530-3535` (Vox close
branch), capture the page index analogous to `instStripIdx` at `:3546`.
At the post-removal block near `:3631-3632`, mirror the
`removeInstChannel` call with `removeVoxChannel`.  `MixerPage::removeVoxChannel`
already exists at `MixerPage.cpp:2331-2337` — same shape as
`removeInstChannel`.

Refresh the G-4 comment block at `:3525-3529` so it accurately describes
the new behavior (strip widget drops; APVTS params + recordings stay
intact, matching the post-2026-05-05 Inst convention).

---

## Implementation tasks

### Task 0 — Open the batch
- Mirror this plan from `~/.claude/plans/compressed-foraging-starfish.md`
  to `Plans & Specs/Batch Plans/cozy-mend-ferret.md`.
- Delete `~/.claude/plans/compressed-foraging-starfish.md`.
- Update `Plans & Specs/Main Plan.md` §5 QA-C entry: add
  `**Plan file:** Plans & Specs/Batch Plans/cozy-mend-ferret.md`.
- Seed `Plans & Specs/Running Notes/cozy-mend-ferret.md` (header +
  pair-file pointer, mirroring the QA-A running-notes header).
- `/draft-commit` -> commit Task 0 with the open message.

### Task 1 — Add `isAuditionPending()` accessor to 3 sfizz engines
Header-only inline addition.  Same one-liner on each of:
- `Source/BaySickGuitars/BaySickGuitarsProcessor.h`
- `Source/BaySickBasses/BaySickBassesProcessor.h`
- `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h`

```cpp
bool isAuditionPending() const noexcept
{
    return mAuditionNote.load (std::memory_order_acquire) != -1;
}
```

Placement: alongside other public state-flag accessors (e.g.
`isProcessingEnabled()` near header line 115 in Guitars/Basses;
`isHiHatPedalClosed()` in RustyDrums).  ASCII-only.

### Task 2 — Apply DSP-10 predicate fix at all 4 sites

#### Site (1) — `Source/Engine/Tasks/InstStripTask.cpp:115-119`
Insert before the `if`:
```cpp
bool auditionPending = false;
if (auto* g = mProcessor->getBaySickGuitars (mIndex))
    auditionPending = g->isAuditionPending();
if (! auditionPending)
    if (auto* b = mProcessor->getBaySickBasses (mIndex))
        auditionPending = b->isAuditionPending();
```
Then update the `if`: `if (midiEmpty && noVoices && ! auditionPending)`.

#### Site (2) — `Source/Engine/Tasks/RustyDrumsProducerTask.cpp:35-38`
Insert before the `if`:
```cpp
const bool auditionPending = engine->isAuditionPending();
```
(`engine` is already in scope at line 24-25 from the spinlock branch.)
Then update the `if`: `if (midiEmpty && noVoices && ! auditionPending)`.

#### Site (3) — `Source/PluginProcessor.cpp:2272-2292` (Inst serial path)
Insert before the `if (midiEmpty && noVoices)` at line 2282:
```cpp
bool auditionPending = false;
if (auto* g = getBaySickGuitars (ii)) auditionPending = g->isAuditionPending();
if (! auditionPending)
    if (auto* b = getBaySickBasses (ii)) auditionPending = b->isAuditionPending();
```
Then update the `if`: `if (midiEmpty && noVoices && ! auditionPending)`.

#### Site (4) — `Source/PluginProcessor.cpp:2032-2045` (Rusty serial path)
Insert before the `if` at line 2035:
```cpp
const bool auditionPending = mRustyDrumsEngine->isAuditionPending();
```
(`mRustyDrumsEngine` is in scope inside the spinlock branch at line 2024.)
Then update the `if`: `if (midiEmpty && noVoices && ! auditionPending)`.

### Task 3 — Apply MIX-01 fix in StandaloneEditor

`Source/Standalone/StandaloneEditor.cpp` Vox close branch:

1. Capture `voxStripIdx` analogous to `instStripIdx`.  Modify the block
   at `:3530-3535`:
   ```cpp
   int voxStripIdx = -1;   // mirrors instStripIdx convention
   if (auto* vp = dynamic_cast<VoxPage*>(mPages[i]->component.get()))
   {
       int idx = vp->getPageIndex();
       if (idx >= 0)
       {
           mProcessor.unregisterVoxEngine (idx);
           voxStripIdx = idx;
       }
   }
   ```

2. Refresh the G-4 comment at `:3525-3529` to describe the new behavior
   (strip widget drops; APVTS + recordings preserved, matching Inst
   convention).

3. Mirror the `removeInstChannel` call.  After the existing block at
   `:3631-3632`:
   ```cpp
   if (voxStripIdx >= 0 && mMixerPage)
       mMixerPage->removeVoxChannel (voxStripIdx);
   ```

### Task 4 — One-source-commit verification (per spec call C2)
Tasks 1 + 2 + 3 land in a single commit via `/draft-commit`.  No
intermediate commits between Task 0 (open) and this source commit.

### Task 5 — Build verify
- Run `do_build.bat` (Jeff runs).  Confirm Release + Debug both green.
- Standing rule: Debug exe first, then Release.

### Task 6 — Manual audition test (D1)
- **DSP-10 Rusty path test (most direct UI):**
  1. Open BaySickDAW Release exe.  Add a BaySickRustyDrums tab.
  2. Pick a kit program (any factory preset).
  3. Wait 1-2 seconds of full silence (no MIDI, no kit clicks) to let
     the chain idle-suspend (>=200ms threshold = 9 blocks at 256/44.1k).
  4. Click a hitbox on the kit graphic UI (BaySickRustyDrumsKitGraphic
     in Tab 0 "Drum Kit").  Expected: drum hit is audible.
     Pre-fix: silent (chain suspended, audition swallowed).
  5. Repeat with MT engine OFF (Mixer hamburger -> "Use multi-threaded
     render").  Same expected result.  Tests serial-path site (4).
- **DSP-10 Inst path test (Guitars/Basses):** Verification by 1:1
  textual mirror with Rusty test.  Same predicate shape, same engine-
  accessor pattern.  No clean UI audition path identified for these
  engines (audition reaches them via piano-roll input which wakes via
  `midiEmpty=false`, a different predicate term).
- **MIX-01 test:**
  1. Add a Vox tab.  Note its mixer strip is present.
  2. Close the Vox tab via the X.  Expected: strip widget removed from
     mixer.  Pre-fix: strip stays orphaned.
  3. Add a Vox tab again.  Expected: spawns at the next free idx (since
     APVTS params for closed strips intentionally stay alive, similar
     to Inst convention).

### Task 7 — Close sequence
1. `/draft-doc batch-close` -> compile Implemented Work Log entry from
   running notes.
2. Apply the draft to `Plans & Specs/Implemented Work Log.md` via Edit.
3. `/review-batch QA-C` -> audit diff vs this plan + CLAUDE.md rules +
   memory gotchas.
4. Address BLOCKERs / NEEDS-FIX inline.  Defer NITs into the close entry.
5. Route side findings (none anticipated for this batch — trivial fixes
   with no architectural implications).
6. `/draft-commit` -> close commit message.
7. Commit the close (separate from the source commit).

---

## Files touched

**Source (Task 1-3):**
- `Source/BaySickGuitars/BaySickGuitarsProcessor.h` — `isAuditionPending()` accessor
- `Source/BaySickBasses/BaySickBassesProcessor.h` — `isAuditionPending()` accessor
- `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h` — `isAuditionPending()` accessor
- `Source/Engine/Tasks/InstStripTask.cpp` — DSP-10 site (1)
- `Source/Engine/Tasks/RustyDrumsProducerTask.cpp` — DSP-10 site (2)
- `Source/PluginProcessor.cpp` — DSP-10 sites (3) + (4)
- `Source/Standalone/StandaloneEditor.cpp` — MIX-01 + comment refresh

**Plans & Specs (Task 0 + Task 7):**
- `Plans & Specs/Batch Plans/cozy-mend-ferret.md` — this plan, mirrored
- `Plans & Specs/Main Plan.md` — §5 QA-C `**Plan file:**` pointer
- `Plans & Specs/Running Notes/cozy-mend-ferret.md` — mid-batch log
- `Plans & Specs/Implemented Work Log.md` — close entry

---

## Risk + reversal

- All 7 source edits are inline / additive.  Zero structural risk.
- Predicate change worst-case: chain wakes too aggressively, costs
  trivial CPU.  No correctness regression possible.
- `isAuditionPending()` accessor is `const noexcept` and reads an atomic
  with acquire ordering — wait-free, audio-thread-safe.
- MIX-01 is a 1-call-site addition mirroring an already-shipped Inst
  pattern.
- Single-commit rollback boundary per spec call C2 (Task 4).

---

## Out of scope

- Adding `isAuditionPending()` to the other 4 engines (BaySickSynth /
  BaySickBass / Harmless / VibePlayer) — no functional need, parity-only.
- Refactoring the audition pattern itself (mAuditionNote ring upgrade is
  a Future State CL-272..CL-274 item, deferred per CLAUDE.md "Engine
  audition pattern" technical note).
- Diagnostic instrumentation for audition-wake events (D1 picked over
  D2/D3; no counter / AlertWindow added).
- Save/reload Vox-strip-disappears bug Jeff flagged in pre-batch
  discussion — different code path (project state serialization, not
  tab close).  Likely already routed to QA-D (project state hardening)
  or QA-E (REC-01); not part of QA-C scope.
