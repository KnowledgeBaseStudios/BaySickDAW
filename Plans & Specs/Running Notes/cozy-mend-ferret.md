# Running Notes — QA-C (cozy-mend-ferret)

> **Purpose.** Append-only mid-batch log of what was done, what was found, what
> was decided, and what was deferred during QA-C execution.  Compiled from
> `/draft-doc running-notes` dispatches at every significant checkpoint
> (commit landed / sub-task verified / finding captured / decision made /
> scope pivot / spec call resolved).
>
> At batch close, `/draft-doc batch-close` reads this file (plus git log,
> memory entries, the per-batch plan, and conversation context) and produces
> the single Implemented Work Log entry that goes into `Plans & Specs/
> Implemented Work Log.md`.  This file is the source-of-truth intermediate
> artifact during the batch; the close entry is the durable summary.
>
> **Pair file:** `Plans & Specs/Batch Plans/cozy-mend-ferret.md`
> (the per-batch plan).
>
> **Convention:** see `Plans & Specs/Main Plan.md` §0 (folder-scope rule +
> Agent Orchestration Rules' mid-batch checkpoint trigger).  Running-notes
> subfolder convention established 2026-05-09 mid-QA-A.

---

## 2026-05-10 — Source edits applied (Tasks 1+2+3, pre-commit)

### Done so far

#### Task 1 — `isAuditionPending()` accessor on the 3 sfizz engines
- Added `bool isAuditionPending() const noexcept` to:
  - `Source/BaySickGuitars/BaySickGuitarsProcessor.h`
  - `Source/BaySickBasses/BaySickBassesProcessor.h`
  - `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.h`
- Each implementation reads `mAuditionNote.load(std::memory_order_acquire) != -1`.
- Placed adjacent to the existing `getNumActiveVoices()` accessor (the other
  idle-suspend predicate input) for readability — both predicates' inputs
  now sit together in each engine's public API.

#### Task 2 — DSP-10 idle-suspend predicate fix at all 4 sites
- **MT path — Inst:** `Source/Engine/Tasks/InstStripTask.cpp:115-119` —
  Guitars + Basses peek before predicate; form `if (midiEmpty && noVoices && ! auditionPending)`.
- **MT path — Rusty:** `Source/Engine/Tasks/RustyDrumsProducerTask.cpp:35-38` —
  engine peek before predicate; same form.
- **Serial path — Inst:** `Source/PluginProcessor.cpp:2272-2292` —
  Guitars + Basses peek; same form.
- **Serial path — Rusty:** `Source/PluginProcessor.cpp:2032-2045` —
  engine peek; same form.
- All 4 sites use identical predicate shape so future readers can grep one
  pattern and find every idle-suspend gate.

#### Task 3 — MIX-01 Vox strip drop on tab close
- `Source/Standalone/StandaloneEditor.cpp` (Vox close branch):
  - Captured `voxStripIdx` mirroring the existing `instStripIdx` capture
    convention.
  - Added `mMixerPage->removeVoxChannel(voxStripIdx)` adjacent to the
    existing `removeInstChannel` call.
  - Refreshed the G-4 comment to describe new behavior: strip widget
    drops; APVTS + recordings stay alive (matches post-2026-05-05 Inst
    convention, no-file-delete contract preserved).

### In-flight

- **9 edits total across 7 files.**  None committed yet.
- Source commit per spec call C2 (single commit covering Tasks 1+2+3) is
  next, post-build-verify and post-Jeff-approval-of-drafted-message.
- Build not yet run; `do_build.bat` is owned by Jeff.

### Found along the way

*(none new since the 4-site investigation; all findings from that pass
captured in the pre-batch checkpoint below)*

### Decisions made

*(none new since pre-batch lock-in; see pre-batch checkpoint below for D1-D5)*

---

## 2026-05-10 — Pre-batch + plan + Task 0 close

### Done so far

#### Pre-batch reads
- `/standup` + 4 `/read-doc` dispatches confirmed QA-C as the immediate next
  batch.  QA-B was deferred 2026-05-10 to after QA-E per §9 tenth Forks
  entry (mute-isolation dependency).
- §5 QA-C entry had no `**Plan file:**` pointer; plan didn't exist.

#### Investigation — predicate gap surface area
- §5 framing of QA-C as "InstStripTask.cpp:115-119 + Rusty equivalent"
  was incomplete: the same idle-suspend predicate gap exists at **4 sites
  total** (MT-path Inst, MT-path Rusty, serial-path Inst, serial-path
  Rusty).  Surfaced 4 spec calls to Jeff before drafting the plan.
- Explore agent confirmed `mAuditionNote` is private on all 7 engines
  (BaySickPlayer, BaySickSynth, BaySickBass, BaySickSolstice, BaySickGuitars,
  BaySickBasses, BaySickRustyDrums); no public read accessor existed.
  QA-C scope therefore expanded to add `isAuditionPending()` to the 3
  sfizz engines (Guitars / Basses / RustyDrums) the predicates reference.

#### Spec calls resolved (locked in plan)
- **A2** — fix all 4 idle-suspend sites (not just MT-path pair).
- **B1** — silly-name `cozy-mend-ferret`.
- **C2** — one source commit + close commit (2 total).
- **D1** — audible audition test for verification (no diagnostic instrumentation).
- **Accessor name** — `isAuditionPending() const noexcept -> bool`
  (codebase convention is `is...()` for state flags).

#### Plan + Task 0
- Plan written.  Mirrored to `Plans & Specs/Batch Plans/cozy-mend-ferret.md`;
  home-dir `~/.claude/plans/compressed-foraging-starfish.md` deleted (one-way
  mirror rule, `feedback_plan_mirror_one_way.md`).
- §5 QA-C entry's `**Plan file:**` pointer updated.
- **Commit `03e12d6`** — open commit ("QA-C open: plan file + Main Plan
  pointer (Tiny One-Liners batch).").

### In-flight

- Source edits queued behind plan close (see next checkpoint).

### Found along the way

1. **4-site predicate gap surfaced beyond §5 framing.**  §5 listed
   InstStripTask + Rusty equivalent only.  Investigation via grep on the
   idle-suspend predicate pattern surfaced 2 additional sites in the
   serial-path branches of `PluginProcessor::processBlock`.  Scope locked
   to all 4 via spec call A2.

2. **G-4 comment refresh queued.**  Pre-existing comment at
   `StandaloneEditor.cpp:3525-3529` ("mixer Vox strip and any bound
   recording stay intact (no-file-delete contract)") needed refresh —
   strip widget will drop with MIX-01, but APVTS + recording stay alive
   (matches post-2026-05-05 Inst convention).  Refresh applied as part of
   Task 3 source edits.

3. **`isAuditionPending()` accessor net-new on 3 engines.**  Originally
   the QA-C plan assumed the 4 sfizz-engine predicate sites could read
   `mAuditionNote` directly.  Explore agent confirmed it's private on
   every engine — Guitars / Basses / RustyDrums had no public state-flag
   accessor for audition state.  Scope expanded to add the accessor on
   all 3.  Player / Synth / Bass / BaySickSolstice are not touched by QA-C
   (their predicate sites are not in the 4-site set), so their internal
   audition handling remains untouched this batch.

4. **Process slip — Task 0 commit ran without surfacing drafted message
   + git status to Jeff for approval.**  Caught mid-batch by Jeff.
   Memory rule locked (`feedback_surface_drafted_commit_message_for_approval.md`).
   Source-commit and close-commit will surface drafts before commit
   runs.  Pairs with the existing
   `feedback_surface_full_git_status_before_commit.md` and
   `feedback_every_commit_via_draft_commit.md` rules.

### Decisions made

- **D1**: A2 — fix all 4 idle-suspend predicate sites (MT + serial paths,
  Inst + Rusty).
- **D2**: B1 — silly-name `cozy-mend-ferret`.
- **D3**: C2 — single source commit covering Tasks 1+2+3, plus a separate
  close commit (2 batch commits total beyond Task 0 open).
- **D4**: D1 — audible audition test for verification; no diagnostic
  instrumentation added.
- **D5**: Accessor named `isAuditionPending() const noexcept -> bool`,
  matching codebase `is...()` state-flag convention.
