# Running Notes — QA-Eg (squishy-scribbling-flurry)

> Append-only mid-batch log. A new `## YYYY-MM-DD — Task N — <name>` entry is
> appended at every checkpoint (commit landed / sub-task verified / finding
> captured / spec call resolved / scope pivot) per
> `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close,
> `/draft-doc batch-close` consumes this file as the primary input for the
> single Implemented Work Log entry. Never edited retroactively.

**Pair:** `Plans & Specs/Batch Plans/squishy-scribbling-flurry.md` (the plan).
**Conventions:** Main Plan §0 — Document Formatting Conventions + running-notes
required sections (locked 2026-05-11) + Rule 4 (Diagnostic Instrumentation Catalog).

---

## 2026-05-23 — Task 0 — Batch open (docs)

- **Scope.** QA-Eg unifies bus-meter draining onto the G1 pattern (each bus node
  owns its peak; UI polls nodes directly via `drainMeterAtomicsForUI` — the FL
  Studio mixer model). Migrates the 8 G2-mirror buses (AudioClips / FX / Vox /
  Inst / Rusty + Vox2 / Inst2 / Inst3) off the `PluginProcessor::*Run`
  intermediate mirrors onto node-owned atomics + VibeGraph public-member
  atomics + a unified `drainAndMerge` loop. Supersedes QA-Ef's interim FX-bus
  Group-2-style fix.
- **Pre-batch.** `/standup` (QA-Ef closed `ad956bf` + paperwork `fcc2297`; 3
  unpushed commits ahead of `origin/main`; tree clean of source; only untracked
  `Templates/My Templates/` from QA-Ef close NIT-v). Full direct self-read of
  Main Plan §0 (lines 1-600 covering Rules 1-4 + Document Formatting Conventions
  incl. the locked Batch Plans + Running Notes required-sections rule + the
  federated-bouncing-cupcake exemplar + canonical buckets + Agent Orchestration
  Rules). `/read-doc`-style targeted reads: §5 QA-Eg entry (`Main Plan.md:1141-
  1159`), §6 sequencing arrow + footnotes (`:1686-1785`), §9 twenty-eighth
  Forks (QA-Eg routing, `:3933-3974`) + twenty-ninth (QA-NativeDialogs) +
  thirtieth (QA-ProjectSave). CLAUDE.md status claims spot-checked: QA-Ef
  shown closed in §5 STATUS banner; commit ladder matches Work Log.
- **Pre-plan source reads** (per `feedback_check_code_before_calling_it_expected.md`):
  - G1 BusNode struct definitions ([Source/VibeGraph.cpp:223 / :426 / :593 /
    :757 / :906](Source/VibeGraph.cpp) — each carries internal `peakDb / peakDbL
    / peakDbR` atomics).
  - G1 exchange-store pattern ([VibeGraph.cpp:1486-1488 master + :1545-1547
    layers + :1554-1556 bass + :1563-1565 drums](Source/VibeGraph.cpp:1486)).
  - G2 publishing site — CAS-max into `mBusPeakRefs` in `processBus` generic-bus
    section ([VibeGraph.cpp:1684-1705](Source/VibeGraph.cpp:1684)).
  - G2 pointer registration ([PluginProcessor.cpp:288-315](Source/PluginProcessor.cpp:288)).
  - `drainMeterAtomicsForUI` full body ([PluginProcessor.cpp:2085-2161](Source/PluginProcessor.cpp:2085)
    — G1 loop at `:2096-2108`, G2 promotion at `:2112-2151`, QA-Ef interim
    FX-bus block at `:2115-2133`).
  - InstrChannelNode definition ([VibeGraph.cpp:1276-1332](Source/VibeGraph.cpp:1276) —
    confirmed has NO peak atomics; just name/preEq/rack/eq/pPolarity/pWidth).
  - Per-row Builder audio meters ([PluginProcessor.h:645-654 + :620-622 +
    CompositeAudioInsertTask.cpp:113-115](Source/PluginProcessor.h:645) —
    confirmed same dual-mirror architecture as G2 buses; routed to S2 deferral).
- **Spec calls resolved with Jeff (2026-05-23):**
  - **S1 = all 8 G2 buses** (Clips / FX / Vox / Inst / Rusty + Vox2 / Inst2 /
    Inst3). Leaving secondaries on `*Run` re-creates exactly the split this
    batch removes.
  - **S2 = per-row Builder audio meters DEFERRED** to a new dedicated batch.
    Same architectural smell confirmed by source-read, but folding adds ~3-5
    hours + `kMaxAudioRows` verify scenarios + touches CompositeAudioInsertTask
    (the DSP-12 surface). Routed at QA-Eg close via §9 Forks + new §5 batch
    per Rule 3; slot surfaced to Jeff at close-time.
  - **S3 = per-bus tasks** (FX → Clips → Vox+Vox2 → Inst+Inst2+Inst3 → Rusty →
    cleanup). Each task individually verifiable by ear; clean rollback
    boundaries; matches `feedback_commit_at_checkpoints.md`. FX first because
    EffectsBusNode already G1-shaped (smallest scope to validate the pattern).
  - **S4 = publishPeakReading() with the per-block latency-comp ring buffer**
    (not the simpler CAS-max). Whole point of standardizing is uniformity
    across all 11 buses; simpler approach would re-introduce a subtle ballistics
    split.
  - **S5 = silly-name `squishy-scribbling-flurry`** (my pick per
    `feedback_silly_name_is_my_pick.md`; runtime-assigned by plan-mode).
- **Plan structure.** 9 tasks total: Task 0 open commit (this entry) + Task 1
  read-only pre-flight inventory + Tasks 2-6 per-bus migrations + Task 7
  infrastructure cleanup + comment sweep + final stress + Task 8 close
  sequence. Plan file written to `~/.claude/plans/squishy-scribbling-flurry.md`,
  approved via ExitPlanMode, mirrored to `Plans & Specs/Batch Plans/squishy-
  scribbling-flurry.md`, home-dir copy deleted per `feedback_plan_mirror_one_
  way.md`.
- **Files changed in Task 0 (docs-only, no source touched):**
  - `Plans & Specs/Batch Plans/squishy-scribbling-flurry.md` (new — the plan).
  - `Plans & Specs/Running Notes/squishy-scribbling-flurry.md` (new — this file).
  - `Plans & Specs/Main Plan.md` (1 line edit — §5 QA-Eg `**Plan file:**`
    pointer flipped from `` `<silly-name>.md (when started)` `` to the
    backticked-path form, matching sibling §5 entries).
- **Risk this commit:** none — docs-only. Source work begins at Task 1
  (read-only inventory) then Task 2 (FX bus migration, first source touch).

## 2026-05-23 — Task 0 — Open commit landed (`6594b3a`)

- **Commit:** `6594b3a` (top of `main`, 4 ahead of `origin/main`). Docs-only —
  3 files, 374 insertions / 1 deletion.
- **Files in commit:** `Plans & Specs/Batch Plans/squishy-scribbling-flurry.md`
  (new — the plan), `Plans & Specs/Running Notes/squishy-scribbling-flurry.md`
  (new — this file), `Plans & Specs/Main Plan.md` (1 line — §5 QA-Eg
  `**Plan file:**` pointer flip).
- **Pre-commit surface:** drafted commit message + full `git status` surfaced
  to Jeff per `feedback_surface_drafted_commit_message_for_approval.md`;
  disposition table covered all 4 entries (3 staged + 1 untracked-not-staged);
  Jeff approved before `git commit` ran.
- **Working tree post-commit:** clean of source changes. Only untracked is
  `Templates/My Templates/` (Jeff's local verify-test asset directory,
  intentionally NOT staged per
  `feedback_surface_full_git_status_before_commit.md` — carry-forward from
  QA-Ef close NIT-v, unchanged this batch).
- **Build state:** N/A — docs-only commit, no source touched, no build run.
- **Next action.** Task 1 — read-only pre-flight inventory. Re-verify the 8 G2
  buses against current post-QA-Ef source (re-measure the `*Run` mirror list +
  `processBus` CAS-max call sites + `drainAndMerge` promotion path +
  `publishPeakReading` ring-buffer signature L/B/D/Master share). Surface
  inventory to Jeff in plain English before any source touch in Task 2.

## 2026-05-23 — Task 1 — Pre-flight inventory (read-only) + S6 surfaced

- **Method.** Read-only Task 1 per the plan — direct reads of all 5 G1 BusNode
  struct definitions + the universal publish helper + every consumer site of
  the registration / mirror / drain infrastructure against current post-QA-Ef
  source (no source edits, no commit). Line numbers below are current and
  confirm the plan's deletion + addition map.

#### Plan assumptions validated

- **Uniform G1 peak field set.** All 5 G1 BusNodes carry an IDENTICAL field
  list: `peakDb / peakDbL / peakDbR` atomics + `peakRingL / peakRingR` arrays +
  `peakRingIdx int`. LayersBusNode ([VibeGraph.cpp:217](Source/VibeGraph.cpp:217)),
  BassBusNode ([:420](Source/VibeGraph.cpp:420)), DrumsBusNode
  ([:587](Source/VibeGraph.cpp:587)), MasterBusNode ([:752](Source/VibeGraph.cpp:752)),
  EffectsBusNode ([:901-1042](Source/VibeGraph.cpp:901)). Task 3 InstrChannelNode
  extension copies this exact pattern — no invention.
- **publishPeakReading is the universal publish helper.** Defined
  [VibeGraph.cpp:91](Source/VibeGraph.cpp:91); 5 G1 call-sites at `:413, :580,
  :743, :885, :1038` — each at the END of its respective BusNode processBlock
  with the canonical 7-arg signature `(buf, peakRingL, peakRingR, peakRingIdx,
  peakDbL, peakDbR, peakDb)`. Tasks 3-6 add the same call to processBus's
  generic-bus section pointing at the new InstrChannelNode fields.
- **FX exchange-store insertion point confirmed.** Right after
  `processEffectsBus(buf, bpm, anyBus, panLaw)` returns in processBus's `kFxBus`
  dispatch ([:1531](Source/VibeGraph.cpp:1531)), BEFORE the `return;`. Parallel
  to L/B/D pattern at `:1545-1547 / :1554-1556 / :1563-1565`.
- **Cleanup infrastructure is fully self-contained.** `registerBusPeakAtomics`
  has 7 callers (all [PluginProcessor.cpp:288-315](Source/PluginProcessor.cpp:288));
  `mBusPeakRefs` used ONLY by `registerBusPeakAtomics` + the CAS-max site in
  `processBus` ([:1687](Source/VibeGraph.cpp:1687)); `drainEffectsBusPeakDbStereo`
  has exactly 1 caller ([PluginProcessor.cpp:2120](Source/PluginProcessor.cpp:2120)
  — the QA-Ef interim block Task 2 deletes). NO consumers outside PluginProcessor
  + VibeGraph. Task 7's wholesale delete is safe.
- **No surprises.** The deletion + addition map in the plan matches reality.

#### Side finding — S6 (dead field across all 5 G1 BusNodes)

- **`peakDecayDbPerBlock` is DEAD STATE on every G1 BusNode.** Initialised +
  recalculated in `prepare()` (Layers `:233 / :293`, Bass `:435 / :488`, Drums
  `:602 / :653`, Master `:766 / :816`, EffectsBus `:916 / :954`) but NEVER read
  on any BusNode. InsertNode has its own copy at `:1095 / :1134 / :1261-1262`
  that IS used — separate field. Dead carry-over from pre-2026-05-02
  meter-ballistics model that the lock-free `publishPeakReading` rewrite
  obsoleted.

#### S6 resolution (Jeff, 2026-05-23)

- **Skip the dead field on the new InstrChannelNode** (Task 3 plan note updated).
- **Also delete the dead field + its prepare-time recalc lines from the 5
  existing G1 BusNodes as part of Task 7 cleanup** (Task 7 plan step inserted).
- **Net post-batch:** all 11 buses uniform with no dead carry-over.

#### Plan file edits (3 targeted Edits per `feedback_targeted_edits_not_wholesale_rewrite.md`)

1. New row **S6** added to "Spec calls already locked" table — records
   dead-field decision + reasoning.
2. Task 3 InstrChannelNode-extension code block updated — comment explicitly
   notes the field set EXCLUDES `peakDecayDbPerBlock` per S6.
3. New step inserted at the TOP of Task 7's checklist — "S6 dead-field cleanup"
   with file:line references for all 10 deletion points (5 nodes x 2 sites
   each: declaration + prepare-time recalc).

- **Source / commit / build.** None this task — Task 1 is read-only by design.
  No diagnostic instrumentation added.
- **Next action.** Surface to Jeff for green-light on Task 2 — FX bus migration
  (first source touch): add `fxBusPeakDb*` VibeGraph members, add the 3-line
  exchange-store in processBus's `kFxBus` dispatch (post-`processEffectsBus`,
  pre-`return`), add 3 `drainAndMerge` lines to the G1 loop in
  `drainMeterAtomicsForUI`, delete the QA-Ef interim FX-bus drain block at
  `PluginProcessor.cpp:2115-2133`, delete `mFxBusPeakDb*Run` declarations +
  initialisers.

## 2026-05-23 — Task 2 — FX bus migration to G1 (commit `0b33ffe`)

- **Commit:** `0b33ffe` (top of `main`, 5 ahead of `origin/main`). 6 files, 121
  insertions / 28 deletions.
- **Files in commit:** 4 source — `Source/VibeGraph.h` (+3), `Source/VibeGraph.cpp`
  (+14/-1), `Source/PluginProcessor.cpp` (-19/+3), `Source/PluginProcessor.h`
  (-3) — + 2 docs — `Plans & Specs/Batch Plans/squishy-scribbling-flurry.md`
  (+6/-1 S6 catch-up), `Plans & Specs/Running Notes/squishy-scribbling-flurry.md`
  (+97 Task 0 commit-landed + Task 1 catch-up).

#### Source changes (FX bus on G1, no `*Run` hop)

- **Added `fxBusPeakDb / fxBusPeakDbL / fxBusPeakDbR` VibeGraph public-member
  atomics** ([VibeGraph.h:661](Source/VibeGraph.h:661), after `masterPeakDbR`).
- **Rewrote `kFxBus` dispatch in `processBus`** ([VibeGraph.cpp:1531-1547](Source/VibeGraph.cpp:1531))
  from single-line into multi-line block with 3 exchange-stores parallel to
  L/B/D — same shape as `:1545-1547 / :1554-1556 / :1563-1565`.
- **Moved `constexpr float kBusNegInf` declaration UP** ([VibeGraph.cpp:1540](Source/VibeGraph.cpp:1540))
  out of the L/B/D-specific scope so FX shares the same declaration.
- **PluginProcessor.cpp** — added 3 `drainAndMerge` lines to the G1 loop after
  `masterPeakDbR` drain + DELETED the entire QA-Ef interim FX-bus drain block
  at `:2115-2133` + updated Group 2 comment to remove "FxBus" mention.
- **PluginProcessor.h** — deleted `mFxBusPeakDbRun / mFxBusPeakDbLRun /
  mFxBusPeakDbRRun` declarations.
- **Net behaviour:** FX bus meter publishes via the identical end-to-end path
  as L/B/D/Master. No intermediate `*Run` mirror. QA-Ef's interim Group-2-style
  fix is gone.

#### Verify (PASSED, Debug, 2026-05-23)

- Scenarios derived from source-reads of MixerPage + cable + aux code per the
  new `feedback_verify_scenarios_read_app_first.md` rule (saved this task —
  see process notes below). The 4-scenario rig:
  1. New project + Layer with sound + Mixer "Add Aux Strip" (auto-routes to
     FX Bus per 5F-4b) + Layer's "+" → Send → click aux → trigger sound →
     FX Bus meter reads activity matching L/B/D/Master ballistics. PASSED.
  2. Stop sound → FX Bus meter decays cleanly. PASSED.
  3. Multi-core OFF (Mixer hamburger toggle) → repeat (1) → meter still
     reads. PASSED.
  4. Multi-core ON, save + reload project → meter still reads post-reload.
     PASSED.
- **Regression check** — L/B/D/Master meters untouched + verified still
  reading on the same project. PASSED.

#### Process notes

- **Verify-rig pivot.** Initial surface to Jeff was a fabricated
  cable-drag-to-FX-Bus workflow that did not match how the app routes to FX.
  Jeff overruled ("No like this isn't at all how this app functions" + "The
  simplest is for you to do your job. Look at how it actually works and tell
  me what I need to test"). New memory `feedback_verify_scenarios_read_app_first.md`
  saved — every verify rig comes from source reads, not a generic DAW mental
  model. Explore agent then derived the real workflow above from MixerPage +
  cable + aux code, which is what's recorded in the commit message body.
- **Pre-commit surface.** Drafted commit message + full `git status` surfaced
  to Jeff per `feedback_surface_drafted_commit_message_for_approval.md`. Jeff
  caught one factual error in the message's "Next:" line (referenced
  BaySickGuitars/BaySickBasses as if buses — they're engine processors);
  proposed fix (Task 3 = AudioClips bus migration + InstrChannelNode peakDb
  plumbing), approved-with-fix, fix landed in the committed message.
- **No diagnostic instrumentation added this task.** Catalog stays empty
  post-Task-2.

#### Next action

- **Task 3 — AudioClips bus migration.** Task 3 carries the one-time
  structural addition that all 7 InstrChannelNode-backed buses inherit:
  extend `InstrChannelNode` ([VibeGraph.cpp:1276-1332](Source/VibeGraph.cpp:1276))
  with `peakDb / peakDbL / peakDbR` atomics + `peakRingL / peakRingR` arrays +
  `peakRingIdx int` — matching the 5 G1 BusNodes' field set EXCLUDING
  `peakDecayDbPerBlock` per S6. On top of that one-time extension Task 3 then
  wires the AudioClips bus only: `audioClipsPeakDb*` VibeGraph members +
  `processBus` exchange-store branch + `drainAndMerge` wiring + delete the
  AudioClips `*Run` mirror + delete the `registerBusPeakAtomics` call for
  `kClipsBus`. Tasks 4-6 then ride the now-extended `InstrChannelNode` to
  migrate the remaining 6 buses.

## 2026-05-23 — Task 3 — AudioClips bus migration + InstrChannelNode peakDb plumbing (commit `2c66bdc`)

- **Commit:** `2c66bdc` (top of `main`, 6 ahead of `origin/main`). 5 files, 123
  insertions / 15 deletions.
- **Files in commit:** 4 source — `Source/VibeGraph.h` (+3), `Source/VibeGraph.cpp`
  (+38 — InstrChannelNode struct extension + processBus refactor), `Source/PluginProcessor.cpp`
  (-4 net), `Source/PluginProcessor.h` (-3) — + 1 doc — `Plans & Specs/Running Notes/squishy-scribbling-flurry.md`
  (+80 Task 2 close catch-up).

#### Source changes (one-time InstrChannelNode extension + AudioClips per-bus wire-up)

- **`InstrChannelNode` struct extension** ([VibeGraph.cpp:1276-1332](Source/VibeGraph.cpp:1276))
  — added the LIVE peak-meter field set parallel to L/B/D/Master/FX BusNodes:
  `peakDb / peakDbL / peakDbR` atomics + `peakRingL / peakRingR` arrays +
  `peakRingIdx int`. **EXCLUDES** `peakDecayDbPerBlock` per S6. One-time
  structural change; all 7 InstrChannelNode-backed buses inherit it.
- **`processBus` dispatcher refactor** ([VibeGraph.cpp:~1592-1631](Source/VibeGraph.cpp:1592))
  — added `InstrChannelNode* node = nullptr;` in the variable-decl block;
  `kClipsBus` case sets `node = mAudioClipsBusNode.get();`. Other generic-bus
  cases leave node null (Tasks 4-6 add their lines).
- **`processBus` peak-publish block** ([VibeGraph.cpp:~1694-1715](Source/VibeGraph.cpp:1694))
  — refactored from unconditional CAS-max-into-`mBusPeakRefs` to conditional:
  if `node != nullptr`, call shared `publishPeakReading` into node's peak fields;
  else fall back to existing CAS-max (for non-migrated buses).
- **`processBus` exchange-store block** added at end of function — for
  `kClipsBus`, exchange-stores `mAudioClipsBusNode->peakDb*` into
  `VibeGraph::audioClipsPeakDb*` member atomics. Tasks 4-6 extend with else-ifs.
- **`Source/VibeGraph.h`** — added `audioClipsPeakDb / audioClipsPeakDbL /
  audioClipsPeakDbR` after `fxBusPeakDbR`.
- **`Source/PluginProcessor.cpp` `drainMeterAtomicsForUI`** — added 3
  `drainAndMerge` lines for AudioClips in G1 loop; DELETED 3 G2 promotion
  lines for AudioClips; updated "Group 2:" comment.
- **`Source/PluginProcessor.cpp` `prepareToPlay`** — DELETED
  `registerBusPeakAtomics(kClipsBus, ...)` call.
- **`Source/PluginProcessor.h`** — DELETED 3 `mAudioClipsBusPeakDb*Run`
  declarations. KEPT the snapshot mirrors `mAudioClipsBusPeakDb / L / R`.
- **Net behaviour:** AudioClips bus meter publishes via the identical
  end-to-end path as L/B/D/Master/FX. No intermediate `*Run` mirror.

#### Verify (PASSED, Debug, 2026-05-23)

- Scenarios derived from source-reads of `BuilderPage.cpp:3437-3442`
  (audio-clip-added callback) + `MixerPage.cpp:1155 / 1227` (Clips Bus strip
  always-visible) per `feedback_verify_scenarios_read_app_first.md`:
  1. New project + drop WAV on Builder grid + play → Clips Bus meter reads
     activity matching the audio's loudness vs Master. PASSED.
  2. Stop playback → Clips Bus meter decays cleanly. PASSED.
  3. Multi-core OFF → repeat (1) → meter still reads. PASSED.
  4. FX Bus regression (Task 2 rig) → still reads correctly post-Task-3
     structural change. PASSED.
- **L/B/D/Master regression scan:** PASSED.

#### Side finding to route at batch close (per Rule 3) — Dirty-flag refactor (new dedicated batch)

- **Finding (surfaced by Jeff 2026-05-23 mid-Task-3 testing):** clicking a
  solo button and unclicking it marks the project dirty even though the net
  state matches the saved file. Verified by code-read: `ApvtsDirtyTracker`
  ([Source/Standalone/ApvtsDirtyTracker.h:39-42](Source/Standalone/ApvtsDirtyTracker.h:39))
  is a `ValueTree::Listener` that fires `onAny` on every property write
  regardless of old-vs-new equality; `ProjectManager::markDirty`
  ([Source/ProjectManager.cpp:98-102](Source/ProjectManager.cpp:98)) just sets
  `mDirty = true` unconditionally. The flag tracks "anything touched since
  load" — NOT "state differs from file." No before-vs-after comparison.
- **Jeff's routing call (2026-05-23):** new dedicated batch at end of Phase
  5. Full spec text (verbatim from Jeff, to be carried into the per-batch
  plan file when the new batch opens):

  > We are refactoring BaySickDAW's dirty state tracking to mimic major DAWs.
  > Currently, ProjectManager::mDirty is a simple boolean triggered by an
  > APVTS ValueTree::Listener. We need to replace this with an Undo-aware
  > transaction pointer system so that if the user hits Ctrl+Z to return to
  > the exact state of the last save, the dirty flag clears automatically.
  >
  > **Strict UndoManager Plumbing:**
  > Audit the entire codebase for state mutations and enforce strict
  > UndoManager registration. Ensure the global UndoManager is correctly
  > passed into the AudioProcessorValueTreeState (APVTS) constructor. Audit
  > all direct ValueTree writes. Any instance of setProperty(id, val,
  > nullptr) must be rewritten to pass the global UndoManager*. Ensure all
  > custom UI components either use JUCE's ParameterAttachments (which handle
  > undo grouping automatically) or explicitly call
  > undoManager->beginNewTransaction() before modifying parameters.
  >
  > **Implement the Transaction Pointer:**
  > Since JUCE's UndoManager does not expose a native state ID, implement a
  > TransactionTracker to act as the source of truth for the project's
  > modification state. Create an integer tracking system: `int
  > currentUndoStep = 0;` and `int savedUndoStep = 0;`. Wrap the DAW's global
  > Undo and Redo commands. Triggering an Undo decrements currentUndoStep,
  > and triggering a Redo increments it. Whenever a new edit is registered,
  > increment currentUndoStep. **CRITICAL EDGE CASE:** If a new edit is made
  > while `currentUndoStep < savedUndoStep`, the user has branched the undo
  > history and destroyed the previously saved future. You must set
  > `savedUndoStep = -1` (or an unreachable constant) so the project
  > correctly remains dirty indefinitely until the next save.
  >
  > **Dynamic Dirty State Evaluation:**
  > Remove the static `mDirty = true` logic inside ProjectManager and
  > ApvtsDirtyTracker. The project is now considered dirty only if
  > `currentUndoStep != savedUndoStep`. When ProjectManager::save()
  > successfully writes to disk, sync the pointer: `savedUndoStep =
  > currentUndoStep;`. Update the UI header to observe this dynamic
  > evaluation so the dirty asterisk instantly vanishes when Ctrl+Z lands
  > exactly on savedUndoStep.
  >
  > **Reference:** Vars, Values and ValueTrees: State Management in JUCE
  > (ADC23) — architectural overview of keeping JUCE application state
  > synchronized across the UI, UndoManager, and project saves.

- **Slot region locked (Jeff 2026-05-23):** end of Phase 5. Exact slot
  relative to QA-ProjectSave (the consolidated save/template/sample batch
  currently at end-of-Phase-1-5-chain) to be surfaced for spec call at QA-Eg
  Task 8 batch close — proposing before/after QA-ProjectSave + sequencing
  rationale for each, Jeff picks.
- **Routing artifacts to draft at Task 8 close (per Rule 3):** §9 Forks
  entry recording this finding + decision; new §5 batch entry with the
  spec text above + slot rationale; §6 arrow update.
- **Working name for the new batch:** to surface for spec call at Task 8
  close (proposing `QA-DirtyFlag` matching the QA-NativeDialogs / QA-VibeSlider
  naming pattern; Jeff picks final).

#### Process notes

- **Pre-commit surface.** Drafted commit message + full `git status`
  surfaced to Jeff. Jeff caught one factual error in the "Next:" line
  (drafter said "Task 4 (Vox bus migration...)" — actual Task 4 = Vox +
  Vox2 buses in one task); proposed fix, approved-with-fix, fix landed in
  the committed message.
- **No diagnostic instrumentation added this task.**

#### Next action

- **Task 4 — Vox + Vox2 buses migration.** First per-bus wire-up onto the
  InstrChannelNode field set landed here; both primary Vox bus and
  secondary Vox bus migrated in one task. Mechanical mirror of Task 3's
  dispatcher + drain + prepareToPlay + header pattern. No new structural
  change (InstrChannelNode already extended).

## 2026-05-23 — Task 4 — Vox + Vox2 buses migration (commit `02a0a5b`)

- **Commit:** `02a0a5b` (top of `main`, 7 ahead of `origin/main`). 5 files,
  167 insertions / 23 deletions (running notes Task 3 close catch-up is the
  bulk; actual source change is +26 / -23 across 4 files).
- **Files in commit:** 4 source — `Source/VibeGraph.h` (+6), `Source/VibeGraph.cpp`
  (+14), `Source/PluginProcessor.cpp` (+6/-13 net), `Source/PluginProcessor.h`
  (-6) — + 1 doc — `Plans & Specs/Running Notes/squishy-scribbling-flurry.md`
  (+138 Task 3 close + S6 forward-ref + dirty-flag finding routed to Task 8).

#### Source changes (mechanical mirror of Task 3 applied to Vox + Vox2)

- **VibeGraph.h** — added 6 atomics: `voxBusPeakDb / voxBusPeakDbL /
  voxBusPeakDbR` + `voxBus2PeakDb / voxBus2PeakDbL / voxBus2PeakDbR` after
  `audioClipsPeakDbR`.
- **VibeGraph.cpp `processBus` switch** — `kVoxBus` case sets
  `node = mVoxBusNode.get();`, `kVoxBus2` case sets `node = mVoxBus2Node.get();`.
- **VibeGraph.cpp `processBus` exchange-store block** — 2 else-if branches
  added for Vox + Vox2 (exchange-stores node-internal peak atomics into
  VibeGraph member atomics).
- **PluginProcessor.cpp `drainMeterAtomicsForUI`** — +6 G1 drain lines for
  Vox + Vox2, -6 G2 promotion lines, "Group 2:" comment updated.
- **PluginProcessor.cpp `prepareToPlay`** — -2 `registerBusPeakAtomics` calls
  (`kVoxBus`, `kVoxBus2`). 4 remaining for Tasks 5 + 6.
- **PluginProcessor.h** — -6 `*Run` declarations. KEPT snapshot mirrors.
- **Net:** Vox + Vox2 meters now G1-shaped end-to-end. *Run hop gone for
  Vox + Vox2; survives only on the 4 remaining G2 buses (Inst / Inst2 /
  Inst3 / Rusty).

#### Verify (PASSED, 2026-05-23, Jeff's own rig)

- I proposed two verify options (cable-drag-from-Layer-to-Vox-Bus + the
  Add-Vox-Strip + speak-into-mic flow). Jeff overruled both: "Both of these
  options show you don't know how the program functions, I'll test vox
  myself thanx." Jeff ran his own Vox + Vox2 test rig and reported PASSED.
- **Process correction this task:** even after saving the
  `feedback_verify_scenarios_read_app_first.md` rule at Task 2 close, I
  surfaced verify options based on a 20-line `addVoxChannel|kVoxBus` grep
  without actually tracing the audio flow into / out of the Vox strip.
  Memory file extended with the partial-read-trap refinement (see Process
  notes below).

#### Process notes

- **Memory update.** Extended `feedback_verify_scenarios_read_app_first.md`
  with the partial-read-trap addition: "read the app code" means trace the
  full audio + UI flow for the feature being tested, NOT grep one symbol
  and infer. Buses backed by live input (Vox / Inst) behave differently
  from pattern-playback (L/B/D) / Builder-drop (AudioClips) buses; don't
  assume cross-applicability.
- **Pre-commit surface.** Drafted commit message + full `git status`
  surfaced to Jeff; approved.
- **No diagnostic instrumentation added this task.**

#### Next action

- **Task 5 — Inst + Inst2 + Inst3 buses migration.** Three buses in one
  task (parallel to Task 4 doing two Vox buses). Same mechanical pattern.
  Before surfacing the verify rig: dispatch Explore agent to trace the
  actual Inst bus audio + UI flow (Inst is a live-input bus like Vox; the
  partial-read trap will re-apply if I just grep symbols).

## Diagnostic Instrumentation Catalog

(per Main Plan §0 Rule 4 — append a row WITH the diagnostic add, walk + strip
at task/batch close. Format: Site / Tag / Purpose / Disposition.)

No diagnostics added through Task 2 (FX bus migration was a pure exchange-store
+ drain rewire — no instrumentation needed).

**Pre-existing Keep entries** (retro-added per Rule 4 "pre-existing diagnostics
get retro-added with Keep when first surfaced"):

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `PluginProcessor.cpp:1857-1885` (DSP-load meter, MT-Md hamburger readout) | n/a | "Run MT Diagnostic" + DSP-load smoothing/cap | Keep (V1 release fixture; surfaces under the Mixer hamburger). |
| `RenderEngine::gMultiThreadedEngineEnabled` toggle | n/a | Multi-core Rendering ON/OFF runtime gate | Keep (Production toggle, persisted to settings.xml; ON = parallel render, OFF = serial-diagnostic mode with workers parked). |
