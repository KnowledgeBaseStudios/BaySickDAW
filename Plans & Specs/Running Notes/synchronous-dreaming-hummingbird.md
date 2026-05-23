# Running Notes — QA-Ef (synchronous-dreaming-hummingbird)

> Append-only mid-batch log. A new `## YYYY-MM-DD — Task N — <name>` entry is
> appended at every checkpoint (commit landed / sub-task verified / finding
> captured / spec call resolved / scope pivot) per
> `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close,
> `/draft-doc batch-close` consumes this file as the primary input for the
> single Implemented Work Log entry. Never edited retroactively.

**Pair:** `Plans & Specs/Batch Plans/synchronous-dreaming-hummingbird.md` (the plan).
**Conventions:** Main Plan §0 — Document Formatting Conventions + running-notes
required sections (locked 2026-05-11) + Rule 4 (Diagnostic Instrumentation Catalog).

---

## 2026-05-21 — Task 0 — Batch open (docs)

- **Scope.** QA-Ef deletes the serial (ST) render path so MT
  (`RenderGraphDispatcher`) is the single, unconditional render path. The
  serial-execution bisect tool ("is the bug in the parallelism or the logic?")
  is preserved without a duplicate code path via a worker-park serial-diagnostic
  mode.
- **Pre-batch.** `/standup` (QA-Ea closed; tree clean; 2 unpushed commits;
  QA-Ef next per §6). Full direct self-read of Main Plan §0. `/read-doc`
  extractions: §5 QA-Ef + §6 arrow + §9 twenty-fifth (ST-only/mirrored/inert
  inventory) + §9 twenty-seventh (re-slot) + Carry-Forward §1/§4/§6 + Work Log
  QA-Ea/QA-E closes.
- **Spec calls resolved with Jeff (2026-05-21):**
  - SC-diag-ui = **reuse** the Mixer "Multi-core Rendering" toggle (OFF = serial
    diagnostic); keep `<MultiCoreRendering>` persistence. User-facing meaning of
    the toggle is unchanged.
  - SC-serial-meaning = **fully serial** (all workers park, audio thread does
    100%). Answers Jeff's "which most resembles the setup we are removing" — the
    deleted serial path ran with zero workers.
  - SC-cleanup-scope = **both cleanups** folded in (dead busAnySolo +
    BlockContext slim-down; orphaned L/B/D buffers + Part B Task 2 params).
  - SC-relabel (minor sub-call) = lean keep "Multi-core Rendering"; Jeff may
    override at any point.
- **Code grounding done in plan mode (exact bounds re-verified in Task 1):**
  MT branch at `PluginProcessor.cpp:1931`; early `return` `:2005`; serial tail
  ~`:2008` → `processBlock` close (~`:2838`). Shared feeds called from the MT
  body: `applyPostMixRecordAndMetro` `:1987`, `drainMeterAtomicsForUI` `:1994`,
  `measureDspLoadAndOverload` `:2004` (defs `:3046` / `:3117`, outside the tail).
  `VibeThreadPool` clamps workers `jlimit(1, kMaxWorkers=8, …)` + the audio
  thread drains via `runUntilOrTimeout` → fully-serial = park all workers.
  `CompositeAudioInsertTask` present (DSP-12 resolution).
- **CLAUDE.md staleness noted (NOT this batch's job):** "Next batch QA-Md" and
  the "Bus (5 total)" mixer table are both stale (QA-Ef is next; code registers
  11 buses — flagged in the QA-Ea close).
- **Task 0 actions:** plan mirrored to `Batch Plans/` (+ home copy deleted);
  §5 QA-Ef `**Plan file:**` pointer set; this running-notes file seeded.
- **Task 0 commit:** `f28e08a` (docs-only — plan + running-notes seed + §5 pointer).

## 2026-05-21 — Task 1 — Pre-flight inventory (read-only) + scope decision

- **Method.** Read-only Task 1 per the plan — re-measured exact serial-tail
  bounds + every dead-symbol reference against current post-QA-Ea source by
  direct read (no source edits this task). Line numbers below are current and
  carried into the Task 2 deletion map surfaced to Jeff.

#### Deletion map (re-verified)

- **Serial tail = `PluginProcessor.cpp:2008-2837`** — `processBlock` closes
  `:2838`; MT branch `if` at `:1931`; early `return` at `:2005`. Everything
  between the early-return and the function close is the serial path Task 2
  deletes.

#### Dies WITH the tail (serial-only — vanish automatically in Task 2)

- Per-engine scratch buffers `mLayerEngineScratch` / `mBassEngineScratch`.
- The `pushScToEngine` stack lambda (`:2014`).
- Serial-tail engine spin-lock **usages** at `:2027` / `:2068` — but the lock
  **objects** `mLayerEngineLock` / `mBassEngineLock` **survive** (also used by
  `prepareToPlay` `:238` / `:243` and engine add/remove `:4280+`). Delete the
  usages, keep the objects.
- The serial `busAnySolo` computation (`:2657+`).

#### Cleaned in Task 3 (the "Both cleanups" Jeff agreed to fold in)

- Dead MT-branch `busAnySolo` (`:1953` / `:1964`) + the `BlockContext.busAnySolo`
  field + its consumers.
- Orphaned vestigial buffers `mLayerEngineSum` (decl `:798`, prepare `:222`,
  pre-branch clear `:1763`) + `mBassEngineBuf` (decl `:800`, prepare `:224`).
  Also check `mDrumsEngineBuf` (named in the `:2782` legacy comment).

#### Confirmed survives / safe (no action, or preserved automatically)

- Shared `applyPostMixRecordAndMetro` (def `:2848`, MT call `:1987`) — the
  "MT proven on all 3" gate; survives.
- `measureDspLoadAndOverload` (def `:3117`), and **the DSP meter cap `10.f`
  lives at `:3132` INSIDE it** — so CL-291 Keep is preserved automatically by
  deleting only the tail.
- `drainMeterAtomicsForUI` (MT call `:1994`); FilePlay pre-scan
  (`:1880-1916`, before the branch).
- **`CompositeAudioInsertTask` is REGISTERED at `:4765`** + stored in
  `mAudioRenderTasks` — DSP-12 resolved, so deleting the serial summation will
  NOT re-expose clip / audio-insert silence.
- No offline / bounce path assumes the serial tail (Export is unwired / future).

#### Scope decision (Jeff, 2026-05-21) — SC-routeinsert = Option B (full removal)

- **Finding that triggered it.** `routeInsertOutput` (serial fan-out) is
  reachability-serial-only, but it lives inside two **MT-SHARED** helpers —
  `renderAudioClipsForRow` (called by `CompositeAudioInsertTask::run`) and
  `renderFilePlayPlayer` (called by `VoxStripTask::run` / `InstStripTask::run`)
  — gated behind `if (mtDest == nullptr)`.
- **Options surfaced.** A = tail-only delete, defer `routeInsertOutput` cleanup
  (my recommendation); B = full removal now.
- **Jeff overruled to B**, as a batch-wide **"leave no serial ghost"**
  principle: leftover serial code/comments would re-surface to a future session
  or to the doc-reader (which greps `serial` / `routeInsertOutput`) as if serial
  were still live — defeating the batch's whole purpose.
- **B folds into Task 2.** Delete `routeInsertOutput`; collapse both shared
  helpers to MT-only (drop the `mtDest == nullptr` serial branch, make the
  destination a required non-null buffer, update the 3 callers); sweep every
  stale serial-referencing comment across `Source/`.
- **Plan file updated** accordingly: Context "leave no serial ghost" principle
  + an SC-routeinsert row + the expanded Task 2 scope. **Route at close:** record
  the A→B scope expansion in the batch-close entry's routing table (Rule 3
  in-batch-resolved); no mid-batch §9 Forks needed.

- **Next action.** Task 2 — collapse to the single unconditional MT path: delete
  the serial tail (`:2008-2837`) + `routeInsertOutput` + serial branches in the
  two shared helpers, add the one net-new block (`VibeThreadPool::workerLoop`
  park-when-OFF gate), and sweep stale serial comments.

## 2026-05-21 — Task 2 — Serial removal (PluginProcessor half) + Option 1 (full)

- **Scope decision — Option 1 (full removal), Jeff 2026-05-21.** I leaned Option 2
  (sweep comments, defer the deeper dead infrastructure); Jeff overruled — twice now
  I've leaned to the half-measure on the core task. Decision: remove ALL serial-dead
  code in QA-Ef, not just the processBlock tail. "Leave no serial ghost" includes the
  dead infrastructure, not just comments.
- **PluginProcessor half DONE (on disk, not yet committed/built):**
  - Worker-park serial-diagnostic gate added to `VibeThreadPool::workerLoop`.
  - `processBlock` collapsed to the unconditional dispatch — serial tail `:2008-2837`
    deleted (~830 lines) via a guarded anchor cut; bare scope block retained for the
    dispatch body.
  - `routeInsertOutput` deleted; the 3 helper serial branches (`renderAudioClipsForRow`
    + `renderFilePlayPlayer` vox/inst) unwrapped to MT-only. Kept `mtDest` as an
    always-non-null pointer (documented) rather than a reference — avoids
    alignment-sensitive edits to hot-path continuation lines; same leave-no-ghost result.
  - Orphaned `bpmForInserts` removed; `RenderEngineFlags.h` flag doc + header API docs
    + the immediate render-path comments de-ghosted.
- **VibeGraph half — full serial-dead chain found (NEXT):** the legacy monolithic
  `VibeGraph::processBlock` (1502-1586) has ZERO call sites (only comments) — its caller
  was the deleted serial tail; MT runs the task graph. Dead with it: `mChannelAccum` +
  `getChannelAccumulator` + `clearChannelAccumulators` (MT uses arena/pull, not the
  accumulators); `processAuxInserts` (zero callers; MT does aux via `PassiveStripTask`);
  scratch `mLayersBuf/mBassBuf/mDrumsBuf/mSumBuf`; the prepare-time sizing; the
  `clearChannelAccumulators()` call at `PluginProcessor.cpp:1700`. This is the cleanup the
  struck QA-Ea Part B Task 2 left behind. `processBus`/`processMasterBus` SURVIVE (MT tasks
  call them). SC-recv buffers SURVIVE (`pullSidechainPredecessorsToGraph` populates them
  under MT).
- **Safety basis:** every removal is serial-only / MT-unreached — MT is the production
  default and never called `routeInsertOutput` or `VibeGraph::processBlock`. The build +
  Jeff's MT verify is the safety net.
- **Checkpoint:** verifying the PluginProcessor half (the 830-line hot-path deletion) in
  isolation before layering the VibeGraph removal, so a regression bisects cleanly.

## 2026-05-22 — Task 2 (cont.) — Aux fixes (preexisting, surfaced by the cleanup)

- **Directive (Jeff, 2026-05-22):** bugs surfaced *by* this cleanup get fixed
  in-batch, not handed to a follow-up. Overruled my route-to-follow-up lean
  (third deferral-lean correction this batch — internalized: do the complete
  job, fix what's found). Both aux bugs folded into QA-Ef.
- **Both confirmed preexisting:** found during the VibeGraph-half verify; both
  live in code QA-Ef never touched, the build compiled, and the MT-path
  behavior is unchanged by the serial-only removal. The serial removal itself
  is audio-verified (Jeff: aux audio routes correctly).
- **Bug A — aux strips not restored on project load** (`StandaloneEditor.cpp`
  `deserializeUIState`). Root cause: `writeStripNames` saves an AuxNames record
  only for USER-RENAMED strips (`name != defaultName`, :9348); on a load into
  the already-open app the AuxNames list is the only thing recreating aux UI
  strips (the MixerPage ctor's `getAuxIndices()` rebuild doesn't re-run). So a
  default-named aux strip was restored in the engine (`restoreAuxStripsFromState`)
  but got no UI strip, and its incoming send param survived -> dangling cable.
  **Fix:** rebuild aux UI strips from `mVibeGraph.getAuxIndices()` (authoritative
  engine state, idempotent) in `deserializeUIState` before AuxNames overlays
  custom names.
- **Bug B — FX-bus meter dead under MT** (`PluginProcessor.cpp`
  `drainMeterAtomicsForUI`). Root cause: the FX bus carries its peak on
  `EffectsBusNode` (like L/B/D/Master) but is absent from Group-1 node-drains;
  its Group-2 `mFxBusPeakDb*Run` mirrors were populated only by the serial tail
  (`drainEffectsBusPeakDbStereo()` + CAS-max) -> serial-only -> dead under MT all
  along. **Fix:** drain `drainEffectsBusPeakDbStereo()` + CAS-max into the Run
  mirrors in `drainMeterAtomicsForUI` so the existing Run->snapshot promotion
  feeds the meter.
- Audio routing was never affected (solo test confirmed); both were meter / UI-
  state only.

## 2026-05-22 — Finding routed to a NEW batch — bus-meter draining is two ad-hoc mechanisms

- **Architectural finding (surfaced by the FX-bus meter diagnosis):** bus peak
  metering uses TWO mechanisms with no deliberate decision behind the split:
  - **G1** (Layers/Bass/Drums/Master): the node owns its peak, published as a
    VibeGraph member atomic that `drainMeterAtomicsForUI` reads directly
    (node -> UI snapshot).
  - **G2** (Clips/Vox/Inst/Rusty/FX): a centralized PluginProcessor running-max
    mirror that `processBus` CAS-maxes into during the block; the drain then
    promotes mirror -> snapshot.
  I (codebase author) introduced G1, then G2 when later buses were added, and
  never surfaced "which do we standardize on" as a spec call. Unilateral
  architectural choice (`feedback_dont_make_unilateral_spec_calls.md`).
- **Decision (Jeff, 2026-05-22):** standardize on **G1** — each node owns its
  peak, the UI polls nodes directly (the FL Studio mixer model). G2's
  centralized mirror is a VST/AU plugin-segregation workaround unnecessary for a
  standalone that owns the whole graph. Migrate the G2 buses (Clips / Vox / Inst
  / Rusty + FX) off the mirror.
- **Routing (per Jeff's "fix small in-batch / plan big to its own batch", 2026-05-22):**
  this is the "big" kind -> **NEW dedicated batch**, NOT folded into QA-Ef.
  Formalize at QA-Ef close: §9 Forks entry + new §5 batch row + §6 slot (slot
  SURFACED to Jeff, not picked, per `feedback_slot_placement_is_spec_call.md`).
  Source material for the batch: Jeff's FL Studio G1 breakdown (2026-05-22 chat).
- **QA-Ef's FX-meter fix is interim (G2-style):** deliberately not re-churned
  during this batch (avoid disrupting Jeff's in-flight verify); the unification
  batch migrates FX + the other G2 buses to G1 together.

## 2026-05-22 — Task 3 — dead-code cleanup (busAnySolo + orphaned engine buffers)

- **Dead bus-solo bookkeeping removed:** `BlockContext.busAnySolo` field + the
  dispatch-body `soloOfBus`/`busAnySolo` compute + PassiveStripTask's solo-flag
  switch + the now-redundant `processBus(... anySolo ...)` parameter. Verified
  safe: `processBus` self-computes `anyBus = anyBusSoloed()` and `ignoreUnused`'d
  the caller arg (QA-Ea Part A), passing `anyBus` to every bus case incl. FX
  (`processEffectsBus(buf, bpm, anyBus, panLaw)`, VibeGraph.cpp:1535). The FX-bus
  solo path (`EffectsBusNode` `busAnySolo` param) is untouched — it receives the
  self-computed `anyBus`. Single `processBus` caller (PassiveStripTask) updated;
  signature now `(busChId, buf, bpm, panLaw)`.
- **Orphaned engine buffers removed:** `mLayerEngineSum` / `mLayerEngineScratch`
  / `mBassEngineBuf` / `mBassEngineScratch` (+ prepare-time sizing + the
  dispatch-time `mLayerEngineSum` size/clear + the orphaned `numRenderCh`). Dead
  since the serial-tail deletion — same family as the `mChannelAccum` removed in
  Task 2. Confirmed 0 references post-removal. Kept in QA-Ef (Jeff: they're
  unrelated to the meter-unification batch, so no reason to move dead code there;
  "if they have nothing to do with the other piece why would we move them").
- **QA-Ef code complete:** serial removal + worker-park serial-diagnostic + the
  two aux fixes (A persistence, B FX meter) + Task 3. Handed off for the
  comprehensive build + verify; close sequence (`/draft-doc batch-close` →
  `/review-batch QA-Ef` → `/draft-commit` → commit) after Jeff confirms.

## 2026-05-22 — Regression found + fixed during verify — project-load crash on the single MT path

- **What happened.** During QA-Ef verify (Multi-core Rendering OFF — single-core
  diagnostic, per the test plan) Jeff loaded a save file and hit a crash. Stack:
  `RenderGraphDispatcher::dispatchBlock` :239 (the leaf-seed loop deref of
  `t->mInitialDeps`) <- `VibeSynthProcessor::processBlock` <- WASAPI audio thread,
  on project load.
- **Owned as exposed-by-QA-Ef (not introduced).** The bug is a latent rebuild
  race that QA-Ef unmasked: before QA-Ef, single-core diagnostic mode ran a
  SEPARATE serial path that never called `dispatchBlock` during a block, so the
  race could not fire in that mode. QA-Ef put every mode on the one
  `RenderGraphDispatcher` path, so `dispatchBlock` now runs during a block in
  both modes and the latent race began crashing on load. Per
  `feedback_own_the_codebase_no_git_alibi.md` — lead with diagnosis + fix, not
  attribution.
- **Root cause (diagnosed).** The project-load shield (`mProjectLoadInProgress`
  atomic, which makes `processBlock` bail to silence during a load) covered only
  the TEARDOWN half of a load — `StandaloneEditor::closeAllDynamicTabs` cleared
  the shield on exit. The REBUILD half (recreating the loaded project's
  tabs/engines/tasks) ran with the shield DOWN. During rebuild,
  `RenderGraphDispatcher::registerTask` does `mTasks.push_back` on a bare
  `std::vector<RenderTask*>` with no reserved capacity; loading a project larger
  than the app's current task capacity grows + relocates the vector while the
  audio thread is concurrently iterating `mTasks` every block (in `dispatchBlock`'s
  leaf-seed loop AND `rebuildLinks`) -> use-after-free / garbage pointer -> crash.

#### Spec call (Jeff, 2026-05-22) — SC-loadcrash = Option C (Both)

- Surfaced: (A) pre-size the task list only; (B) extend the shield only;
  (C) Both. Jeff chose **(C) Both** (most robust — defense in depth: the reserve
  is a universal safety floor, the shield is the correct lifecycle fix).

#### Fix (implemented, on disk — pending build + verify)

- **(1) Reserve — universal floor.** `RenderGraphDispatcher.cpp` ctor:
  `mTasks.reserve(kMaxStripChannels + 64)` + `mSyntheticDeps.reserve(256)` so
  registration never reallocates under the audio thread. Covers EVERY rebuild
  path — load, New Project, restore-backup, live tab adds.
- **(2) Shield the open-save-file rebuild.** `PluginProcessor.cpp`
  `deserializeProject` raises the shield (`setProjectLoadInProgress(true)` +
  `juce::Thread::sleep(30)` drain) at the top of its full body and lowers it at
  the end, after `onDeserializeUIState` fires the UI rebuild — so the heavy
  tab/engine rebuild runs shielded.
- **(3) Nest-aware teardown.** `StandaloneEditor.cpp` `closeAllDynamicTabs` now
  saves the prior shield state and only drains/clears when it is the OUTERMOST
  owner — so when it is wrapped by `deserializeProject` it leaves the shield
  raised through the rebuild.
- **(4) Shield the post-load audio-row rebuild.** `StandaloneEditor.cpp`
  `restoreAudioStripsFromArrangement` shields the audio-row rebuild
  (`ensureAudioInsert` -> `registerTask` + `applyPendingRackStates`) when
  `isLoadContext`, also nest-aware.
- Plus a doc-comment update on `mProjectLoadInProgress` in `PluginProcessor.h`
  documenting that the shield must wrap the full load (teardown + rebuild), not
  just teardown.

#### Residual noted (crash-safe)

- A blank New Project's `addDefaultDynamicTabs` registers a few tasks
  unshielded, but is crash-safe via the reserve (capacity never grows during
  registration) and a fresh project has no audio playing. No further shield
  added there.

- **Status.** Implemented; pending Jeff's Debug + Release build and verification
  that the save-file load no longer crashes. **Next action.** Jeff builds +
  verifies the load path (both modes — Multi-core ON and OFF/diagnostic) on
  Debug then Release; on green, resume the QA-Ef close sequence.

## 2026-05-22 — Shield scope extended — New Project / apply-template (Jeff's catch)

- **Jeff caught my dismissal.** I had filed the blank New Project rebuild as
  "crash-safe via reserve, fresh project has no audio playing." Wrong on both
  counts: (1) File > New can be hit while a prior project is open AND playing;
  (2) the audio device calls `processBlock` -> `dispatchBlock` -> iterates the
  task list every block regardless of transport state. So New Project / template
  apply ARE load-type rebuilds that touch the graph while audio is live. Owned.
- **What the trace showed.** The default New Project seeds from a template ->
  `loadTemplate` (`StandaloneEditor.cpp:6061`) tears down (shielded) then rebuilds
  a kit + 8 layers + 4 basses via `loadKitImpl` + `spawn*FromTemplate`,
  registering ~13 render tasks WITH THE SHIELD DOWN. `loadTemplate` has two
  callers: a standalone "Load Template" menu (`:6261`) and `doFileNew` (`:8964`).
- **Crash already handled, gap was cleanliness.** The Task-0 reserve prevents the
  realloc, so New-while-playing won't crash even unshielded (x86 TSO + reserve);
  the gap was a brief partial-graph render (possible blip), not a crash.

#### Spec call (Jeff, 2026-05-22) — SC-loadcrash-scope = shield all load-type rebuilds

- Surfaced: (A) shield all load-type rebuilds; (B) reserve-only here; (C) New
  Project only. Jeff chose **(A)** — give New Project + apply-template the same
  shield as project load; leave LIVE edits (direct kit load, single-tab adds)
  unshielded by design so audio doesn't drop when editing mid-playback (reserve
  keeps those crash-safe).

#### Fix (implemented, on disk)

- **`loadTemplate`** (`StandaloneEditor.cpp`) — nest-aware shield wrap of its full
  body. Covers the standalone "Load Template" menu caller; nest-inherits when
  `doFileNew` already raised it. Inner `closeAllDynamicTabs` + `loadKitImpl` +
  `spawn*FromTemplate` run under it.
- **`doFileNew`** (`StandaloneEditor.cpp`) — nest-aware shield wrap of the rebuild
  section (the blank-New `addDefaultDynamicTabs` path + the `loadTemplate` call).
- Already-covered load paths (no new edit): open / recent / restore-backup /
  project-as-template all rebuild via the now-shielded `deserializeProject`
  (+ `restoreAudioStripsFromArrangement`).
- **Intentionally left unshielded (reserve-protected, by design):** direct kit
  load (`loadKitImpl` at `:6436`), live single-tab adds, editor-ctor default
  tabs (`:1551`, fresh start / no prior audio). Shielding these would drop audio
  on a mid-session edit.
- **Status.** Implemented; folds into the same pending Debug + Release build.
  Verify scenarios to add: File > New (default template) while a project is
  playing, and the "Load Template" menu while playing — both should go briefly
  silent then resume cleanly with no crash.

## 2026-05-22 — Aux persistence RE-diagnosed (systematic-debugging) — it's SAVE-side

- **My earlier Bug-A "fix" was wrong + I overclaimed.** I'd asserted "save works,
  the bug is load-side, every link reads correct" — but I'd checked a DIFFERENT
  save file and never traced the teardown. Jeff handed me the actual file he
  opened (`Untitled Project (42)/project.xml`) + two facts: (1) aux strips aren't
  torn down between loads (open 16, load another project, all 16 remain); (2)
  adding an aux doesn't flag dirty. Invoked `superpowers:systematic-debugging`
  and gathered evidence instead of eyeballing. Owned per
  `feedback_own_the_codebase_no_git_alibi` + `feedback_check_code_before_calling_it_expected`.
- **Root cause (confirmed, save-side).** `serializeProject` saves via
  `apvts.copyState()` (PluginProcessor.cpp:2911). Lazily-registered mixer-strip
  params (aux added mid-session via "Add Mixer Strip") get a RangedAudioParameter
  but NO node in `apvts.state` until a `replaceState` rebind runs
  (`updateParameterConnectionsToChildTrees` appends a node for any adapter
  lacking one — `juce_AudioProcessorValueTreeState.cpp:438-442`; triggered by
  `replaceState`'s `state = newState` at :407 -> `valueTreeRedirected`). That
  rebind only fires on load / New Project, so an aux added AFTER the last rebind
  is invisible to `copyState()` -> never saved -> nothing for
  `restoreAuxStripsFromState` to find on reload.
- **Evidence.** (42) has `<AuxOrder>` entries (aux strips existed at save) but
  ZERO `mixer_aux_*` params; the equally-lazy `mixer_layer_0_level` IS saved
  (it was rebound during the load/new flow). Explains the (41)-vs-(42)
  inconsistency (luck of whether a rebind ran after the aux was added) and the
  no-dirty-flag clue (creating a param at default fires no value-change).
- **Fix #3 (implemented, on disk).** `serializeProject` runs
  `apvts.replaceState (apvts.copyState())` before snapshotting, materializing
  nodes for every lazy param. Mirrors `applyPendingRackStates`' load-side idiom.
  **Verify (fresh launch to avoid the leak confound):** add an aux to a fresh
  project -> save -> fully close + relaunch -> open -> aux strip returns.

#### QA-Ef scope (full, per Jeff 2026-05-22) — no eliding

- **Done, pending verify:** (1) project-load crash fix; (2) load-shield extended
  to New Project / apply-template.
- **To do, in QA-Ef:** (3) aux save [implemented, above]; (4) aux strips leak
  across loads -> tear down aux (UI + engine nodes) on project load before
  restore; (5) adding an aux doesn't flag dirty -> mark dirty on add; (6) File >
  New isn't blank -> stop applying the default template on New (blank like first
  launch); (7) New-from-Template menu (issues 1+2+3) -> submenu **New from
  Default Template / Premade Templates / My Templates**, reading the correct
  template folders (not the projects folder). Jeff's call: 1-3 + the blank-New
  bug fold into QA-Ef ("cleanup uncovered them, don't leave broken functionality").
- **Deferred — Main Plan §5/§6 entries at QA-Ef close, NO plan docs until they
  open** (`feedback`: routing = Main Plan entry only): (8) bus-meter draining
  unification (FX-bus meter fix already in QA-Ef is the interim piece); (9)
  COMBINED batch = **(9a) native OS file dialogs everywhere** (issue 4 — replace
  custom internal browsers with the Windows file opener, routed to the correct
  folder) **+ (9b) template content** (issue 5 — templates save channels but NOT
  clips; Vox/Inst live recordings fine to drop, but clips may carry the intended
  sound incl. a MIDI component; SAVE dropped samples, DROP live recordings, or
  give a user option; routing-aware — a sample routed to a Vox/Inst channel is
  still a sample and must not be dropped for its route; key off the clip's nature
  not its route). Open 9b spec calls: samples-only vs save-time option; include
  clip MIDI/trigger data?; confirm "key off clip nature not route."

## 2026-05-22 — Cable persistence — fix #3 v2 (Save Diag pinpointed the reset)

- **Symptom found mid-verify:** after fix #3 (aux save) landed and the strip
  restored on first reload, drawing a **Layers Bus -> Aux** cable + saving +
  reloading showed the aux strip back but the cable **gone** until a re-route
  + second save.  Jeff: clicking a send did NOT bring it back (ruled out
  pure UI refresh); his read was a save/load-ordering issue.
- **Decisive evidence -- the Save Diag** (temp instrumentation in
  `serializeProject`, reporting `mixer_layers_send0_to` before rebind / after
  rebind / written-to-file).  First save: live = 100 BEFORE rebind, = -1
  AFTER, written = -1.  Second save: 100 / 100 / 100.  The rebind reset the
  send on the first save.
- **Root cause (JUCE-internal).** Fix #3's
  `apvts.replaceState(apvts.copyState())` triggers
  `updateParameterConnectionsToChildTrees`
  (`juce_AudioProcessorValueTreeState.cpp:424-447`).  For any adapter whose
  tree is invalid (lazy param never bound to a node yet), the rebind creates
  an EMPTY `<PARAM>` node (id only, no `value` property) at the line-442
  `appendChild`.  That `appendChild` fires `valueTreeChildAdded` ->
  `setNewState` (`:417-421`), which reads the new node's missing-value
  default and calls `setDenormalisedValue(default)` -> live param RESET to
  default.  For `mixer_layers_send0_to` the default is -1, so the cable's
  send was silently nulled on the first save after drawing the cable.  The
  second save worked because the first save's rebind had already materialized
  the node with value = -1; the re-drawn cable then wrote 100 into the
  existing node and the next rebind preserved it.
- **Fix #3 v2 (implemented in `serializeProject`):** REPLACE the
  `apvts.replaceState(apvts.copyState())` with manual node creation -- iterate
  `getParameters()`, and for each `RangedAudioParameter` that lacks a node in
  `apvts.state`, append a `<PARAM>` child with its CURRENT live value
  **pre-set**.  The same `valueTreeChildAdded` listener fires but reads the
  correct value -> `setDenormalisedValue` is a no-op -> param keeps its
  current value.  Achieves fix #3's original goal (lazy params saved) without
  the destructive reset.
- **Verified by Jeff:** cable persists on the **FIRST reload** after a save.
  QA-Ef fix #3 (aux strip + cable save) is fully working.
- **Next:** QA-Ef #4 (aux strips leak across loads -- tear down aux UI +
  engine nodes on project load before restore).

## 2026-05-22 — #4 (aux strips leak across loads) — fixed after two iterations

- **Spec call (Jeff, 2026-05-22).** Tear down aux engine state on project load.
  The UI side was already cleared via `MixerPage::clearDynamicStrips`, but the
  engine-side `mAuxInserts` + `mAuxRenderTasks` had no teardown hook —
  `ensureAuxInsert`'s comment literally said "auxes persist for the project
  lifetime."

#### First attempt (broken)

- Added `mProcessor.clearAllAuxInserts()` to `StandaloneEditor::closeAllDynamicTabs`.
- **Broke restore on open-project.** `deserializeUIState` calls
  `closeAllDynamicTabs` AGAIN (nested, at :9685) AFTER
  `restoreAuxStripsFromState` had already rebuilt the aux nodes in
  `deserializeProject`. The nested call wiped the just-restored aux state; then
  my `getAuxIndices()` UI-rebuild loop in `deserializeUIState` found an empty
  list -> no UI strips. Jeff: "Now the strips don't come back at all after load."

#### Refactor

- Moved `clearAllAuxInserts()` out of `closeAllDynamicTabs` entirely. Added it
  to the three load-entry points instead, each already under its own load shield
  (so audio-thread mutation of the render task list is safe):
  - `VibeSynthProcessor::deserializeProject` — right after shield-set + drain,
    before `replaceState`.
  - `StandaloneEditor::doFileNew` — after shield-set + drain.
  - `StandaloneEditor::loadTemplate` — after shield-set + drain.
- Inner `closeAllDynamicTabs` inside `deserializeUIState` no longer touches aux,
  so the restored state survives.

#### Second bug found mid-verify — phantom rebind nodes

- Same-session open AT1 (3 auxes) -> open AT2 (1 aux) showed all 3 still.
- **Root cause.** `apvts.replaceState` triggers JUCE's rebind, which appends
  EMPTY `<PARAM>` nodes for adapters that lack a tree child (any param
  registered in a prior session but absent from this file). On the AT1->AT2
  transition, `mixer_aux_1/2` are still registered `RangedAudioParameter`s
  (`createAndAddParameter` has no inverse), so the rebind creates phantom empty
  nodes for them. `restoreAuxStripsFromState`'s scan of `apvts.state` for
  `mixer_aux_N_level` ids then sees the real `mixer_aux_0` from AT2 + the
  phantom `mixer_aux_1/2` -> calls `ensureAuxInsert` for all three -> phantom
  auxes recreated.
- **Fix.** Changed `restoreAuxStripsFromState` to take
  `const juce::ValueTree& sourceState`. Callers (`setStateInformation` +
  `deserializeProject`) take a `state.createCopy()` deep snapshot BEFORE
  `replaceState` and pass it in. The scan now only finds auxes ACTUALLY in the
  saved file. (Deep copy because `replaceState` does `apvts.state = newState`
  (shared underlying), so a non-copied reference would be mutated by the rebind
  too.)

#### Files

- `Source/VibeGraph.h` + `.cpp` — new `clearAuxInserts()` clears `mAuxInserts`.
- `Source/PluginProcessor.h` + `.cpp` — new `clearAllAuxInserts()` unregisters
  tasks + clears `mAuxRenderTasks` + calls `VibeGraph::clearAuxInserts()`;
  `restoreAuxStripsFromState` signature changed; new call sites in
  `deserializeProject` and updated call in `setStateInformation`.
- `Source/Standalone/StandaloneEditor.cpp` — `clearAllAuxInserts()` calls in
  `doFileNew` and `loadTemplate`; the `closeAllDynamicTabs` addition was
  reverted.

#### Status

- Verified by Jeff: AT1 (3 auxes) -> AT2 (1 aux) -> shows only 1; fresh-launch
  + open AT2 shows 1; File > New + add auxes -> open another project -> clean.

## 2026-05-22 — #5 (adding an aux doesn't flag dirty) — fixed + Untitled title follow-up

- **Spec call.** Mark dirty on user-initiated aux add but NOT on load restore.

#### Fix

- Added `if (mProcessor.onAnyStateChange) mProcessor.onAnyStateChange();` to
  `MixerPage::addAuxChannel()`. That's the only user entry point — both the
  "Add Aux Strip" button click and the cable-spawn-new-aux path (when the user
  drags a send and there's no existing aux to target) route through
  `addAuxChannel()`. The load path uses `addAuxChannelAtIndex` directly and
  bypasses this entirely. `markDirty` itself respects
  `ProjectManager::mIgnoreDirty` as a belt-and-suspenders.

#### Second bug surfaced by verify — Untitled title suppressed the dirty marker

- Opening a project + adding an aux dirtied correctly, but **fresh app launch
  + adding an aux showed no dirty marker.**
- **Root cause.** `StandaloneEditor::refreshWindowTitle` only added the " *"
  suffix INSIDE the `if (mProjectManager->hasProject())` branch -- on a fresh
  launch with no project folder set yet, `hasProject()` returns false, so the
  title bar conditionally suppressed the marker even though `markDirty` had
  correctly set `mDirty=true` and `onDirtyChanged` had fired
  `refreshWindowTitle`.
- **Fix.** `refreshWindowTitle` now shows `BaySickDAW - <project name>` if a
  project is set OR `BaySickDAW - Untitled` if none, and appends ` *` whenever
  `isDirty()` regardless. Fresh-launch + edit now reads
  `BaySickDAW - Untitled *`.

#### Status

- Verified by Jeff.

## 2026-05-22 — #6 (File > New isn't blank) — fixed after a misdiagnosis I owned

- **Spec call.** File > New always loads blank, identical to the editor ctor's
  first-launch state; default-template auto-application moves to a future
  "New from Default Template" menu item (deferred to project-save batch).

#### Misdiagnosis owned

- I initially claimed File > New was loading the user's default template
  (`Test Kit.xml`); Jeff explicitly corrected me ("it's not the default
  template loading — it's a blank template with 1 layer + 1 bass with engines
  pre-selected and locked"). I dug further: `settings.xml` did show
  `defaultTemplate=Test Kit.xml`; the template file structurally contained 1
  Layer + 1 Bass + 16 Drums with `locked="0"`.
- **Reconciliation.** Test Kit DOES load via
  `loadTemplate(getDefaultTemplate())` in `doFileNew`, but its kit drums come
  back **broken on the load side** -- tab name missing, content empty except
  for `+ New Drum` / `+ Rusty`. So Jeff was seeing partial-load: Layer + Bass
  restored, Drums broken. The "pre-selected and locked" was the natural
  "lock after first engine pick" behavior fired by `loadTemplate` restoring
  the engine choice.
- **Pattern feedback from Jeff:** I describe what code does without asking
  "is this even right?" Noted.

#### Fix

- In `doFileNew`, removed `const auto tpl = mProjectManager->getDefaultTemplate();`,
  the `if (tpl.existsAsFile() ...) loadTemplate(tpl)` block, and the entire
  `needDefaults` check + loop. The function now unconditionally calls
  `addDefaultDynamicTabs()` -> `mRibbon->selectTab(3)` -> `onTabSelected(3)`,
  matching the editor ctor's invocation. The default-template settings entry
  remains in `settings.xml` and is still surfaced under Options > Set Default
  Template; it just no longer auto-applies on plain File > New.

#### Files

- `Source/Standalone/StandaloneEditor.cpp` — two edits in `doFileNew`.

#### Status

- Verified by Jeff.

## 2026-05-23 — #7 (template menu + load functionality) DEFERRED in full to the project-save batch

- **Original scope.** Issues 1+2+3 from Jeff's earlier triage (the
  New-from-Template menu restructure, the wrong-folder bug, the submenu shape)
  plus the drum-inline-load bug surfaced during #6.
- **During scoping investigation.** Read `saveTemplateAs`
  (`Source/Standalone/StandaloneEditor.cpp:6148-6224`) and confirmed it saves
  only `<Drum>` entries (inside `<Kit>`), `<Layer>` entries, and `<Bass>`
  entries -- **no vox/inst/clip/rusty/aux**. Cross-checked `loadTemplate`: it
  calls `closeAllDynamicTabs()` which tears down **every** dynamic tab type
  (Layers/Bass/Drums/Inst/Vox/Clip/Rusty), then restores only what the template
  contains -- L/B/D. **Net effect: loading any template DESTROYS
  vox/inst/clip/rusty tabs in the destination project.**
- **Framing miss.** I framed the choice as a 3-option spec call (leave-as-is /
  preserve-other-tabs / expand-template-scope).
- **Jeff overruled the framing.** The L/B/D-only template scope was
  fundamentally incomplete for what "template" means, the destructive teardown
  is functional brokenness (not a tradeoff), and option (3) -- templates save
  everything -- was the right shape from the start. Pattern feedback: "this is
  yet another perfect example of you not following up on all the places things
  need to be updated."
- **Jeff's call.** Move all of #7 (drum-inline-load fix, destructive-teardown
  fix, scope expansion, menu restructure, `doFileNewFromTemplate` removal,
  `showTemplateMenu` cleanup, unified dirty-check load flow) **into the
  project-save batch** -- there's no point wiring a polished menu to a
  fundamentally broken target.
- **Unified Load Template flow confirmed for that future batch.** Blank/clean
  current project -> load template directly into current state (no new-project
  prompt); dirty current project -> discard/save/cancel prompt then load.
  Single "New from Template" submenu (Default / Premade Templates ▸ / My
  Templates ▸) replaces both menu items 102 (`New from Template...` -> the old
  `doFileNewFromTemplate` project-browser bug) AND 109 (`Load Template...` ->
  the existing `showTemplateMenu` popup).

## 2026-05-23 — Project save batch — consolidated scope (for close-time Main Plan §5 entry)

The project-save batch (one of the three deferred batches added at QA-Ef close)
absorbs:

- **Template scope expansion** -- `saveTemplateAs` and the template XML format
  extended to save vox/inst/clip/rusty/aux/samples in addition to L/B/D.
- **`loadTemplate` non-destructive teardown** -- only tear down what the
  template will replace, leave other tab types untouched (OR symmetric to the
  expanded scope above).
- **Drum inline-load fix** -- `loadTemplate` currently only handles
  `<Kit path="..."/>` factory references; needs to also iterate inline
  `<Drum>` children that `saveTemplateAs` writes for user templates.
- **New-from-Template submenu** -- New from Default Template (greyed when no
  default set, label suffix = current default's name when one is) / Premade
  Templates ▸ (recursive walk of `factoryTemplatesDir()`) / My Templates ▸
  (recursive walk of `userTemplatesDir()`). Each pick runs the unified
  dirty-check flow above.
- **Removals** -- `doFileNewFromTemplate` function + menu item 102;
  `showTemplateMenu` function + menu item 109; the duplicate `kIdSaveAs`
  "Save Template As..." entry inside `showTemplateMenu` (the top-level item
  106 "Save as Template..." stays).
- **Sample retention / FL-Studio-style file handling** -- the parked
  discussion. Design space: reference-by-path (low disk, fragile) vs
  per-project copy (current model, duplicates samples across projects) vs
  source-aware hybrid (Factory + user library = reference, volatile drops =
  copy). Plus an explicit "Pack project" action for portability. Plus
  migration story for existing per-project copies. Plus UI indicators
  (reference vs copy). Lean from the discussion: source-aware hybrid + Pack
  action, matching FL Studio expectations.
- **Save Template As dialog text** stays "saved kit + layers + basses" as long
  as the scope is L/B/D-only; updates to reflect the expanded scope land with
  the scope-expansion work.

## 2026-05-23 — Process corrections captured this turn (from Jeff)

- **Drafter at the END of each task, not mid-task.** I had been firing
  `/draft-doc running-notes` (and even writing entries directly) too often
  during execution earlier in QA-Ef. Going forward: one drafter dispatch per
  completed task, applied once verified. Mirrors the established protocol Jeff
  articulated -- mid-task drafts are noise.
- **No per-task commits at this point.** We are past where per-task commits
  would have made sense; ONE commit at QA-Ef close covering the whole batch.
- **"Not following up on all the places things need to be updated."**
  Recurring pattern Jeff is calling out -- I describe what code does and stop
  there instead of asking "is this even right" or "does this affect everywhere
  it should." Surfaced twice this turn (the L/B/D-only template scope framing;
  the option-1/2 framing of the destructive teardown). Internalize: when the
  analysis surfaces an incomplete-by-design state, FLAG it as a problem rather
  than presenting it as a viable option.

## 2026-05-23 — Three deferred batches identified for close-time Main Plan §5/§6 entries

1. **Bus-meter draining unification** -- Group-1 standardization across
   L/B/D/Master vs Group-2 (Clips/Vox/Inst/Rusty/FX) bus peak draining; the
   FX-bus meter fix already in QA-Ef is the interim Group-2-style piece.
   Decided early in QA-Ef when the FX-meter fix surfaced the inconsistency.
2. **Native OS file dialogs everywhere** -- replace the custom internal
   browsers (project browser, etc.) with native Windows file/folder dialogs
   everywhere an on-disk file is opened, each routed to the correct default
   folder for its context. Pure UX swap; independent of templates/samples.
   (Original issue 4 from Jeff's earlier triage.)
3. **Project save / template / sample handling** -- the consolidated batch
   absorbing all of #7's deferred work plus sample retention plus the related
   save-format/migration questions. Scope detailed in the "Project save batch
   scope" sub-entry above.

## 2026-05-23 — Status — end of QA-Ef code work

- All in-batch code work complete: #1 crash fix, #2 shield extension, #3 aux
  save (and refined v2 after the rebind-reset diagnosis), #4 aux leak teardown
  (across three load-entry points, with the rebind-snapshot fix), #5 aux dirty
  flag (with the Untitled-title fix), #6 File > New blank.
- #7 fully deferred to project-save batch.
- **Next action.** This running-notes draft applied, then Main Plan §5/§6
  entries for the three deferred batches, then `/draft-doc batch-close`,
  `/review-batch QA-Ef`, `/draft-commit`, then ONE commit for the whole QA-Ef
  batch.

## Diagnostic Instrumentation Catalog

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| _(none added yet — QA-Ef is mostly deletion)_ | | | |

**Pre-existing (Keep):** MT diagnostic counters (`RenderEngineFlags.h`
`MtDiagnostic` namespace) + "Run MT Diagnostic (2s capture)" Mixer menu item 203
(`StandaloneEditor.cpp`) — CL-292; instruments the MT dispatch path and pairs
with the new serial-diagnostic mode (shows ~100% main-thread when serial). DSP
meter cap `10.f` in `measureDspLoadAndOverload` — CL-291 (HOLD-FOR-Phase-6).
