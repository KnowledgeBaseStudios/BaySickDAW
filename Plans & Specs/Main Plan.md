# BaySickDAW Post-Batch-10 QA Triage & Batching Plan

## Context

Batch 10 of the multi-threaded render engine project shipped on 2026-05-06.
The MT path is now production: runtime atomic toggle (`gMultiThreadedEngineEnabled`),
hamburger-menu UI, settings.xml persistence, default ON.

The active backlog at close-out was 64 entries (59 active + 4 optional + 1 never)
in `Files For Claude/BaySickDAW Master QA & Issue Backlog (UNIFIED 2026-05-07).txt`.
This plan converts that backlog into a sequence of small, granular batches.

**Diagnostic discovery during this triage** (2026-05-07): WAV/MP3 drops on the
Builder grid produce silent playback under MT. Toggling MT off restores
correct playback. This is **DSP-12 in action** — `ClipPageTask` (auto-spawned
by `spawnClipsTabIfMissing`) and `AudioInsertTask` both register at the same
channel id under MT, the dispatcher's most-recent-registration-wins rule
overwrites AudioInsertTask, and the Clips-tab page has no MIDI notes
triggering its sampler so the row outputs silence. Validation suite missed
it because Vox/Inst recording tests use different render-task types
(`VoxStripTask`/`InstStripTask`) with no channel-id conflict.

**This bug is now the first batch** — it blocks core arrangement
functionality (drop a WAV, hear it play) and gates every downstream audio-path
batch's verification baseline.

**Plan goal:** convert the backlog into a sequence of small batches that
preserve the MT-path discipline, each with its own commit/rollback boundary,
and avoid one-fix-breaks-another rework by coordinating items that touch
the same code surface. **This is a triage plan, not implementation plans** —
each batch gets its own `<silly-name>.md` plan file when it starts.

**Sequencing decision (user-confirmed):** Option A — confidence-first.
**DSP-12 fix shape (user-confirmed):** Composite RenderTask.
**FILE-02 reassignment timing (user-confirmed):** Immediate (next audio block).

---

## 0. Three-Doc System + Carry-Over Discipline (READ FIRST)

This plan operates with **three companion documents** that together preserve
context across the 15-batch execution. Per-batch plan authors read all
three at start; per-batch execution updates them in different ways. The
three-doc structure protects against losing context across sessions —
the #1 risk when a 15-batch plan stretches over weeks rather than days.

| Doc | Filename | Purpose | Update cadence |
|-----|----------|---------|---------------|
| **Plan** | `Main Plan.md` (this file) | Master sequencing + per-batch scope + dependencies. The "what to do, in what order". | **Hybrid: inline back-refs + master fork log.** When work scope changes, edit the affected section(s) inline with a one-line back-ref to the master fork log (e.g. "QA-0a fork — see §9"), AND append a full narrative entry to §9 Forks. Inline annotations preserve the linear-read mode (sections describe their actual content); the master log preserves the chronological "what's changed since plan write?" overview. Original content is never deleted — inline back-refs add a line, master log appends entries. Convention adopted 2026-05-07 (see §9 first entry). |
| **Carry-Forward Reference** | `Carry-Forward Reference.md` | Architectural primitives + file:line index + decisions already made + per-item status + patterns to reuse. The "what was true on 2026-05-07 when triage closed". | **Frozen after creation.** This is the reference point captured during the 2026-05-07 triage session — never edited. New findings during execution go into the implemented-work doc instead. If a carry-forward entry turns out to be wrong, the implemented-work doc records "carry-forward §X said Y, verified to be Z" — the carry-forward stays as the historical snapshot. |
| **Implemented Work & Findings** | `Implemented Work Log.md` | Running log of executed batches: what was done, what was found along the way, what was done about each finding. The "what's happened since plan write". | **Append-only.** New entry per batch (or per significant stopping point within a batch). Never edit prior entries — surprise findings later get their own new entry. Also captures contradictions of the carry-forward as new entries. |

The carry-forward and implemented-work files are created when this plan
exits plan mode. The carry-forward starts populated with everything
verified during this triage session and is then sealed. The
implemented-work log starts empty.

Three standing rules apply to every per-batch plan derived from this plan.

**Rule 1 — Every per-batch plan starts by reading all three companion docs.**
Plan: read your batch's section (scope, dependencies, risk). Carry-forward:
read §1-3 minimum (architectural primitives, files, decisions); skim the
rest. Implemented-work: read every entry since your batch's predecessors
were planned, so you know what changed beneath your feet.

**Rule 2 — Every stopping point produces a carry-over block.** "Stopping
point" = any time the work pauses long enough that context will be lost
(end of session, end of day, end of batch, blocked-on-decision pause).
Before pausing, write a 5-10 line carry-over block at the bottom of the
active per-batch plan file under a clear `## Carry-Over` heading covering:

- **Completed**: which steps in the per-batch plan finished + verified.
- **In-flight**: which step is mid-execution + what state the code is in
  (uncommitted edits? failing build? partial test?).
- **Assumptions changed**: anything learned during the batch that
  contradicts the plan, the carry-forward file, CLAUDE.md, or the
  implemented-work log so far. Carry-forward contradictions go into the
  implemented-work doc as new entries — the carry-forward itself is not
  edited.
- **Resume action**: the literal first thing the next session should do
  (e.g., "re-run `do_build.bat`", "read carry-forward §3 then re-read
  CompositeAudioInsertTask::run()", "ask Jeff about edge case X").
- **Implemented-work entry needed**: a one-line summary of what to log
  in the implemented-work doc when the batch closes. Includes any
  carry-forward contradictions surfaced during the session.

Non-negotiable for any batch that takes more than one session. Skipping
it = guaranteed re-discovery cost when the next session opens.

**Rule 3 — Findings discovered during batch execution get routed at
batch close (no spaghetti, no Phase-6 punt).** Adopted 2026-05-07
during QA-0 execution. At every batch close, review the implemented-
work entry's "Found along the way" items + any open prior findings.
For each:

- **Touches a not-yet-started batch's surface in §5** → fold into that
  batch's scope (expand the Items + Scope of the existing §5 entry).
  §9 Forks entry records the addition.
- **Touches a completed batch's surface** → annotate the completed
  batch's §5 entry with a one-line pointer (e.g., `*Post-close
  findings: see §9 [date]*`). All details (the finding, the fix's
  plan-file path, eventual commit hashes) live in the §9 Forks entry.
  **No new §5 batch row.**  Commits stay sequential as flag points;
  the plan reflects conceptual ownership.
- **No surface match** (genuinely new work area) → new dedicated §5
  batch row, slotted into the appropriate phase. §9 Forks entry
  chronicles the addition.
- **Phase 6 stays reserved** for dead/dormant code cleanup. Functional
  bugs never get punted there.

Net: §5 stays the planned-sequence map; §9 Forks is the canonical
"what changed since plan write" log; the implemented-work doc is the
chronological execution ledger. Reviewing §9 gives you every fork +
addition + post-close finding in one place.

**Initial carry-over for this plan (2026-05-07):**

- **Completed**: Triage + verification + plan write + carry-forward
  reference write + implemented-work doc creation (empty).
- **In-flight**: Plan approved by user; entering execution phase.
- **Assumptions changed**: WAV/MP3 drop on Builder is silent under MT
  (was working pre-Batch-10). DSP-12 fix is now top-priority blocker.
- **Resume action**: Start QA-0 by reading the carry-forward file's
  §1-3 + the actual `AudioInsertTask::run()` and `ClipPageTask::run()`
  bodies. Then write the QA-0 per-batch implementation plan.
- **Implemented-work entry needed**: Initial entry "2026-05-07 Triage —
  plan + carry-forward + implemented-work docs created. No code changes."

---

## 1. Architectural Orientation (informs sequencing)

All items below confirmed against current HEAD by reading code. Citations
are `file:line` so you can grep back.

### MT render path (production, default ON)

- **Flag**: `RenderEngine::gMultiThreadedEngineEnabled` is `inline std::atomic<bool>`
  at [Source/Engine/RenderEngineFlags.h:44](Source/Engine/RenderEngineFlags.h:44).
  Read with acquire at [PluginProcessor.cpp:1831](Source/PluginProcessor.cpp:1831);
  toggled with release from Mixer hamburger at [StandaloneEditor.cpp:4450-4497](Source/Standalone/StandaloneEditor.cpp:4450).
  Persistence: `VibesynthStandaloneApp::saveMultiCoreRenderingPref()`
  ([StandaloneApp.cpp:240-264](Source/Standalone/StandaloneApp.cpp:240)).
- **Dispatcher**: `RenderGraphDispatcher::rebuildLinks()` runs every block
  ([PluginProcessor.cpp:1737](Source/PluginProcessor.cpp:1737)). `dispatchBlock(const BlockContext&)`
  is the parallel pump under flag=true.
- **BlockContext** ([Source/Engine/BlockContext.h](Source/Engine/BlockContext.h)) carries
  `numSamples`, `bpm`, `posInfo`, `anySolo`, `busAnySolo`, `panLaw`, 7 per-engine
  MIDI buffers, `liveInputSnapshot`.
- **Tasks** in `Source/Engine/Tasks/`:
  - `EngineInsertTask` (Layer/Bass/Drum)
  - `PassiveStripTask` (Aux + Bus)
  - `MasterTask` (sink)
  - `VoxStripTask` / `InstStripTask` (live input + source-mode aware)
  - `ClipPageTask` / `AudioInsertTask` — **conflict at `audioInsert(N)`**
    (DSP-12 root). Comment at [PluginProcessor.cpp:4240-4245](Source/PluginProcessor.cpp:4240)
    acknowledges composite case unresolved.

### Lock-free + lifecycle primitives (Batch 9c shipped — reuse, do not reinvent)

- **AudioClipSnapshot RCU** ([PluginProcessor.h:512-516](Source/PluginProcessor.h:512))
  + atomic publish via `mActiveAudioClips.exchange(...)`. Audio thread
  load-acquires once at top of processBlock, uses same pointer for FilePlay
  scan, Pass 2, applyChokeGroupDispatch, AudioInsertTask, VoxStripTask,
  InstStripTask.
- **RetirementQueue<T>** ([Source/Engine/RetirementQueue.h](Source/Engine/RetirementQueue.h))
  generic generation-stamped queue + dedicated drainer thread. Currently
  used for `RetirementQueue<AudioClipSnapshot>` only.
- **closeAllDynamicTabs** ([StandaloneEditor.cpp:8672-8719](Source/Standalone/StandaloneEditor.cpp:8672)):
  sets `mProjectLoadInProgress(true)` → 30 ms sleep → close every dynamic
  tab → clear ribbon → reset barrier. **First step of `~StandaloneEditor`**
  ([:1304-1326](Source/Standalone/StandaloneEditor.cpp:1304)). Called before
  project-open at [:7670, :8076](Source/Standalone/StandaloneEditor.cpp:7670).
  **Reuse this — do not invent parallel teardowns.**
- **mProjectLoadInProgress barrier** ([PluginProcessor.h:885-889](Source/PluginProcessor.h:885)):
  audio thread acquire-loads, clears buffer to silence if true.
- **mShuttingDown gate** in `BaySickVocalProcessor` only
  ([BaySickVocalProcessor.h:170](Source/BaySickVocal/BaySickVocalProcessor.h:170)).
- **drainMeterAtomicsForUI** ([PluginProcessor.cpp:2880](Source/PluginProcessor.cpp:2880))
  on audio thread, called from BOTH branches.
- **measureDspLoadAndOverload** ([PluginProcessor.cpp:2951-2956](Source/PluginProcessor.cpp:2951))
  wall-clock measurement, called from BOTH branches. Audio-thread-only
  measurement under MT — full sum-of-cores reading is DIAG-02 work item.
- **pullSidechainPredecessorsToGraph** ([Source/Engine/SidechainPullHelper.h:42-63](Source/Engine/SidechainPullHelper.h:42)).

### Mixer/page lifecycle (key file:line index)

- **Spawn cascades** ([MixerPage.cpp](Source/Standalone/MixerPage.cpp)):
  `addVoxChannelAtIndex` (:1677), `addInstChannelAtIndex` (:1999),
  `removeVoxChannel` (:2331), `removeInstChannel` (:2323). Maps:
  `mVoxStrips` (:311), `mInstStrips` (:316).
- **onTabClosed Vox vs Inst** ([StandaloneEditor.cpp:3525-3535 vs :3631-3632](Source/Standalone/StandaloneEditor.cpp:3525)):
  Inst calls `removeVoxChannel`-equivalent; Vox does NOT. **MIX-01 confirmed open.**
- **Project XML restore walker** ([:6571 Vox, :6623 Inst](Source/Standalone/StandaloneEditor.cpp:6571)):
  spawn calls present, but tab-reload-destroys-strip still happens — bug is
  downstream of spawn (post-spawn teardown or guard fail in spawn helpers).
  **MIX-02/04/06 confirmed still open by user.**
- **Recording finalize** ([StandaloneEditor.cpp:9443-9520](Source/Standalone/StandaloneEditor.cpp:9443)):
  `addAudioToLibrary` at :9455 (master) / :9503 (Vox dry); `dropWavAsClip`
  at :9478, :9506, :9512, :9517. **REC-01 confirmed still open by user**
  (calls present but library doesn't show the entries).
- **MIX-03 nature**: NOT an auto-spawn-at-recording bug. Recording correctly
  attaches to Vox strip; on save→reload the Vox strip disappears (MIX-02
  cause) and the orphan recording on the Builder grid becomes a clips strip
  on next load. **MIX-03 = symptom of MIX-02; fixes together.**
- **Effects-page dropdown** ([EffectsPage.cpp:28](Source/Standalone/EffectsPage.cpp:28)):
  `onInstrChannelListChanged` callback wired but doesn't fire on tab-close
  cascade in practice. **MIX-07 confirmed still open by user.**
- **WAV-clip stretch** ([BuilderPage.cpp:3505-3509](Source/Standalone/BuilderPage.cpp:3505)):
  block resize doesn't call `rebuildAudioClipPlayers()`. **BUILD-06 confirmed.**
- **Idle-suspend gate** ([InstStripTask.cpp:115-119](Source/Engine/Tasks/InstStripTask.cpp:115)):
  missing `auditionPending = eng->mAuditionNote.load() != -1` predicate.
  **DSP-10 confirmed.**
- **Bus solo** ([VibeGraph.cpp:358 Layers](Source/VibeGraph.cpp:358)):
  current per-group anySolo formula doesn't match observed behavior (Drums
  plays when Layers solos despite formula including `drumSolo`). User-specified
  target behavior: solo a bus → that bus + everything routed into it plays;
  every OTHER bus silenced at master mix. **DSP-09 confirmed open with
  specified behavior.**
- **Right-click → Automate** ([SharedUI.cpp:1751-1794 VKnob::mouseDown](Source/Standalone/SharedUI.cpp:1751)):
  outer right-click correctly gated. The bug is JUCE's default `PopupMenu`
  accepting any mouse button as item-activation — needs wrapper. **UI-01
  confirmed open.**
- **Automation lane UUID resolver** ([StandaloneEditor.cpp:2538-2544](Source/Standalone/StandaloneEditor.cpp:2538)):
  reachable from `sOnAutomate` at :2416-2420. **UI-02 confirmed still open
  by user.** Diagnose with UI-01.
- **Dead Properties duplicate** ([BuilderPage.cpp:2561](Source/Standalone/BuilderPage.cpp:2561)):
  `m.addItem(7, "Properties...");` added unconditionally for all block types,
  no `case 7` in switch. **Dead. Delete this line entirely.**

---

## 2. Verify-Before-Touching List

Most items the user has confirmed still open during this triage round.
Only two remain in pure verification mode:

| Item | Action |
|------|--------|
| **DSP-07** (single observed silent-first-drop, didn't repro) | Watch-item only. No code action. If it surfaces again, suspect one-block routing-graph rebuild latency. |
| **DSP-12 verification matrix** (after QA-0 fix) | Test all 4 cases: {WAV, MP3} × {drop on Builder, drop on Clips tab}. User confirmed MP3 hits the same auto-spawn cascade as WAV. |

All other items previously listed for verification are confirmed still open
and folded into their respective batches below.

---

## 3. Items to Park / Defer / Fold

Math first, since the v1 number was wrong:

- Total entries in unified backlog: **64**
- Active queue (the 59 work items, ignoring optional + never):       **59**
- Long-horizon optionals (OPT-01..04 + softened NEVER-01):           **5** (kept on the long-horizon shelf, see below)

Of the 59 active items:

- **Folded** (resolves with another item, no separate work):
  - **MIX-03** → folds into MIX-02 fix (symptom of Vox-strip-disappears-on-reload).
  - **STATE-03** → folds into APP-03 (modal load progress dialog cures the symptom).

- **Parked** (no batch unless conditions change):
  - **DIAG-01** — synthetic test for `rebuildLinks`. Internal `jassert` provides enough coverage. Park unless a real test-infra batch surfaces.
  - **APP-01** — shutdown wait climbing. Test-scenario inflation, not a real bug. Park unless shutdown timing logs show >10 sec on a clean session.
  - **DSP-07** — single observed silent-first-drop, didn't repro. Watch-item only.

- **Long-horizon optional** (deferred but not killed — revisit if circumstances change):
  - **OPT-01** — per-stage parallelism inside a strip. Unlikely needed at 17-task DAG; revisit if profiling shows we want finer grain.
  - **OPT-02** — worker thread priority elevation. Add cautiously after MT runtime data shows measured benefit.
  - **OPT-03** — TSAN integration in CI.
  - **OPT-04** — replace serial path entirely. Estimate 6+ months post-Batch 10.
  - **NEVER-01** — per-band EQ parallelism. Current measurement says overhead > benefit at 8 bands. **Not actively planned; not blocked from future reconsideration if EQ topology changes (e.g. more bands, dynamic EQ adds heavier per-band cost).**

That gives **active queue = 59 − 2 folded − 3 parked = 54 items addressed across 15 batches**, plus the 5 long-horizon items kept on the shelf. **DIAG-02 stays in the active queue per user — full sum-of-cores DSP meter is required (lands as QA-N).**

---

## 4. Items Needing Design Call BEFORE Implementation

Resolved during triage:
- **DSP-09 Bus Solo** — user specified target behavior (solo a bus → that
  bus + incoming strips plays; other buses silenced at master).
- **DSP-12 fix shape** — user confirmed Composite RenderTask.
- **FILE-02 routing dropdown** — user confirmed Vox + Inst + Clips options.
- **FILE-02 reassignment timing** — user confirmed Immediate.
- **NAV-04 Piano Roll buttons** — deep-link buttons keyed to active piano-roll
  dropdown selection (visually match standard nav buttons).
- **NAV-03 FX Rack button** — routing per page type:
  - Layers/Bass/Vox/Inst pages → that player's per-strip FX rack
  - Individual drum tab → that drum's individual FX rack
  - Drum Kit (sequencer) page → that kit's drum bus rack
  - Rusty's main page → Rusty's drum bus rack
- **NAV-05 Builder hamburger** — remove. Reclaim vertical space.
- **MIX-03** — symptom of MIX-02; not a separate design call.

**No outstanding design calls remain.** Plan can execute end-to-end without
further blocking input from user.

---

## 5. Proposed Batching Structure

15 batches grouped into 5 phases. Phase 1's three batches can run in
parallel (different code surfaces); within each later phase, batches are
sequential per Option A.

**Convention (2026-05-07):** when a per-batch plan file is created
(start of any batch), its absolute path is recorded as a bold
**`Plan file:`** line directly below the batch's `####` header. This
makes the file lookup one scan away when reviewing §5 — no grep
needed to find what you should pull up to review the work.

### Phase 1 — Critical regression fix + fast wins

#### **QA-0a: Debug Build Workflow Setup**  (forked in 2026-05-07 — see §9)
**Plan file:** `Batch Plans\i-want-you-to-adaptive-dongarra.md`
- Items: workflow infrastructure (no items from the unified backlog).
- Scope: modify `do_build.bat` to build BOTH Release and Debug
  configs; gate the embedded exe icon for Release-only so Debug
  exe shows the generic Windows .exe icon (taskbar pins
  differentiate); append " [DEBUG]" to the window title in Debug
  builds; cold-start triage of existing `jassert` calls; document
  the new workflow in CLAUDE.md.
- Risk: low. Build infrastructure, no audio code.
- Dependencies: none. Runs first in Phase 1.
- Effort: small-medium (~2 hours). Triage is the variable.
- Why before QA-0: QA-0 ships a `jassertfalse` tripwire on the
  dispatcher's most-recent-wins fallback. Without QA-0a's Debug
  build, that tripwire is compiled out of Release and never fires
  in user workflow. QA-0a makes the tripwire actually useful.

#### **QA-0: MT Composite RenderTask (DSP-12 restore)**  ⚠️ TOP PRIORITY
**Plan file:** `Batch Plans\composite-merging-rivers-twilight.md`
- Items: DSP-12.
- Scope: build a `CompositeAudioInsertTask` that owns BOTH render flows
  (`AudioInsertTask` arrangement-timeline path + `ClipPageTask` sampler-MIDI
  path) and sums them internally before insert DSP. Replaces the
  most-recent-registration-wins behavior at `audioInsert(N)` channel ids.
  - Decision contract: when a Clips-tab page IS auto-spawned for a row,
    use Composite (both flows contribute). When only one is present
    (no Clips-tab page, or no Builder-grid clip), the active flow runs
    standalone within the composite.
  - Critical files: new `Source/Engine/Tasks/CompositeAudioInsertTask.h/.cpp`,
    edits to `RenderGraphDispatcher::registerTask` ([dispatcher .cpp](Source/Engine/RenderGraphDispatcher.cpp)),
    edits to spawn cascade in `StandaloneEditor::createBuilderPage`'s
    `onAudioClipAdded` ([:1914-1949](Source/Standalone/StandaloneEditor.cpp:1914))
    + `spawnClipsTabIfMissing`.
  - Reuse pattern: existing `AudioInsertTask` and `ClipPageTask` `run()`
    bodies become helpers invoked by the composite.
- Risk: medium-high. MT-only audio path. Must preserve serial vs MT parity
  (serial path already sums them — composite restores MT to match).
- Dependencies: none. First batch.
- Effort: small-medium (~3-5 hours). Composite shape is well-understood.
- Verification: full DSP-12 verification matrix below.

#### **QA-Inventory: Comprehensive Source-Doc Triage** (added 2026-05-07 via Rule 3 — see §9)
**Plan file:** N/A — executed inline from chat breakdown (per user direction; no separate per-batch plan file in `Batch Plans/`).
- Items: review + categorize every distinct entry across the three pre-QA source documents and route each per its bucket's rules.
- Source docs:
  - `Files For Claude/Final Stretch Work.txt` (~886 lines)
  - `Files For Claude/vibedaw_blueprint.md` (~3333 lines)
  - `C:/Users/jeffm/.claude/plans/lucky-discovering-tiger.md` (~3629 lines — original location preserved as historical record per user direction; Plans & Specs/ is forward-only).
- Buckets:
  - **A** — claimed not-done, still needed → walk one-by-one with user; route per Rule 3 (fold into existing §5 batch OR new dedicated §5 batch).
  - **B** — claimed not-done, drop candidate → confirm with user; one-line entry in `Future State.md` "Considered & Dropped".
  - **C** — claimed done + source-verified → entry in `Previously Implemented.md`.
  - **D** — Claude future-state additions across Audio Quality / Performance / User Tools / Workflow Polish → entries in `Future State.md`. User direction: every plausible idea, not bounded.
  - **E** — claimed done but unverified → walked with user alongside A; per-item Decision dropdown: Update (= reroute to A as work-to-do) / Archive (= reroute to C as verified) / Delete (= folds into Phase 6 cleanup carry-list).
- Workflow: parse → initial bucket → create Google Sheet via Drive connector with all rows pre-filled + frozen-row legend of valid Decision values → walk A/B/E with user (checkpoint pings; Claude downloads CSV to read decisions back) → source-verify C in parallel → fire-hose D → apply decisions to Main Plan §5/§9 + `Previously Implemented.md` + `Future State.md` → close with §9 Forks routing entry.
- Source verification depth: headline-verify every claim (file/class/function exists) + sample 20% of named sub-items per claim + escalate to full verification on any miss + skip anything already verified in Carry-Forward Reference.
- Risk: zero — read-only triage + doc updates; no source code changes.
- Dependencies: QA-0 closed (✓ 2026-05-07).
- Effort: large (~10–15 hours, multi-session expected).
- Why this slot: pre-QA source docs intermix shipped work, claimed-but-unverified work, abandoned work, and future-state ideas without disambiguation. Without the triage, every downstream batch's plan author re-parses the same ambiguity. Done as a dedicated batch (vs scattered across each downstream batch start) because the parse work is identical across docs and most efficient as a single sweep. Also populates the new `Plans & Specs/` doc skeletons (`Previously Implemented.md`, `Future State.md`) created in commit `c05ce61`.

#### **QA-Md: MT Engine Debug-Build Investigation** (added 2026-05-07 via Rule 3 — see §9; promoted from Phase 5 to Phase 1 same day)
**Plan file:** TBD (silly-name file when batch starts)
- Items: QA-0a/QA-0 finding #9 — MT engine is a no-op under Debug
  builds.  DSP meter reads identical with MT on vs MT off; settings.xml
  persistence works; toggle UI works; but the dispatcher isn't actually
  distributing work to threads.  Workers aren't picking up tasks OR the
  MT branch is degrading to single-thread silently.  Release MT works
  fine (verified during Batch 10 ship + QA-0 12-case matrix).
- Scope: diagnostic investigation — determine the Debug-only failure
  point.  Likely candidates:
  - `VibeThreadPool` worker creation skips under Debug.
  - `dispatchBlock`'s `runUntilOrTimeout` returns immediately under
    Debug timing (workers can't keep up).
  - A compile-time gate elsewhere short-circuits the MT branch.
- Why this slot: QA-0 stood up the diagnostic Debug build; the whole
  point of that infrastructure is MT-aware diagnosis going forward.
  Every downstream batch that touches audio (most of them) needs Debug
  to actually engage MT to surface MT-specific bugs precisely.
  Without QA-Md, downstream batches fly blind on MT in Debug.
- Risk: low-medium. Diagnostic-first; fix scope depends on what's
  found.  No audio path changes likely.
- Dependencies: QA-0 (Composite must be in place so dispatcher work
  has tasks to distribute).
- Effort: small-medium (~2-4 hours diagnostic + ~1-2 hours fix
  depending on cause).

#### **QA-A: STYLE Cluster — Unified TitleBar Component** (parallel with QA-0)
- Items: STYLE-01 (BaySickPlayers truncated), STYLE-02 (logo size/font
  standardize), STYLE-03 (Vox "Page Controls" → "BaySickVocals"),
  STYLE-04 (Inst "BaySickGuitars" between player + sub-tabs),
  STYLE-05 (extra black bar on BaySickNAM/IR), STYLE-06 (Synth/Bass preset
  dropdown to right + green title logo).
- Scope: build `Source/Standalone/BaySickTitleBar.h/.cpp` with standardized
  height/font/flex layout + per-engine color slot. Refactor each player page.
- Risk: very low. UI-only.
- Dependencies: none.
- Effort: medium (~4-6 hours).

#### **QA-B: Verification Sweep** (parallel with QA-0 + QA-A)
- Items: DSP-07 + DSP-12 verification matrix (post QA-0 lands).
- Scope: NO code changes. Diagnostic session.
- Risk: zero.
- Dependencies: QA-0 must land before DSP-12 matrix can be exercised.
- Effort: small (~1-2 hours).

#### **QA-C: Tiny One-Liners**
- Items: DSP-10 (idle-suspend audition wake — predicate fix at
  [InstStripTask.cpp:115-119](Source/Engine/Tasks/InstStripTask.cpp:115) +
  Rusty equivalent), MIX-01 (Vox-tab `onTabClosed` missing `removeVoxChannel`
  call — mirror Inst branch at :3631).
- Scope: two tiny single-file patches.
- Risk: very low.
- Dependencies: independent.
- Effort: tiny (~1 hour total).

### Phase 2 — Project state hardening

#### **QA-D: Project State Reset**
- Items: STATE-01 (dirty flag triggers on load), STATE-02 (Guitar/Bass
  counters don't reset on new project), STATE-04 (load while playing
  doesn't stop playback first).
  - **Folded in 2026-05-07 (QA-0a/QA-0 finding #8 via Rule 3)**:
    MenuBarModel listener-dangle during `closeAllDynamicTabs` cascade
    -- a shared MenuBarModel can be destroyed before all
    MenuBarComponents that reference it, fires `removeListener`
    assertion (suppressed in vendored JUCE, real fix queued here).
    Touches `PianoRollPage::unregisterEngine` + the closeAllDynamicTabs
    teardown ordering.  See §9 Forks first entry + 2026-05-07 implemented
    log.
- Scope: `StandalonePlayHead::stop()` at top of project-open path
  (STATE-04). APVTS listener-silent gate around the load window
  (STATE-01). `resetProjectState()` helper invoked from `closeAllDynamicTabs`
  (STATE-02).  **Plus (folded):** ensure MenuBarModel outlives any
  MenuBarComponent that references it during the closeAllDynamicTabs
  cascade -- likely move ownership of the shared MenuBarModel to a
  longer-lived parent OR clear each component's model before the
  model itself is destroyed.
- Risk: medium. Project-load critical path.
- Dependencies: none.
- Effort: medium (~4-6 hours; folded MenuBarModel item adds 1-2 hours).
- MT-awareness: verify `mProjectLoadInProgress` barrier still engages
  with the new playhead-stop step.

### Phase 3 — Vox/Inst lifecycle + DSP cluster (the big consolidated batch)

#### **QA-E: Vox/Inst Lifecycle + Recording + DSP-09 + FILE-02**
- Items consolidated per user direction (Q6 = Option A bundling):
  - **MIX-02 / MIX-04 / MIX-06** — Vox/Inst tab reload destroys mixer
    strip + phantom strips on reload (MIX-03 falls out as side effect).
  - **REC-01** — Vox/Inst recording library hand-off broken.
  - **FILE-01** — Vox wet delete should land in browser bin, not OS delete
    (tie to RetirementQueue / Browser bin).
  - **DSP-09** — Bus solo: solo a bus → that bus + incoming strips plays;
    other buses silenced at master mix.
  - **FILE-02** — Multi-recording on a single player page via
    Properties-popup routing dropdown (Vox / Inst / Clips, defaulting to
    creating page; immediate rebuild on change).
  - **Dead Properties cleanup** — delete [BuilderPage.cpp:2561](Source/Standalone/BuilderPage.cpp:2561).
  - **Folded in 2026-05-07 (QA-0a finding #13 via Rule 3)**: use-after-free
    crash in `StandaloneEditor::showPageForTab` lambda at
    `StandaloneEditor.cpp:4135` -- `[this, ip, syncPagePresetMenu, labels]`
    capture stores a raw `InstPage*` that gets freed when the InstPage
    destructs (engine swap / project reload).  Confirmed via
    `0xDDDDDDDDDDDDDDDD` debug-fill marker.  Fix: capture by
    `juce::Component::SafePointer<InstPage>` OR look up the page by index
    inside the lambda each click.
  - **Folded in 2026-05-07 (QA-0 finding #14 via Rule 3)**: same family as
    #13 -- use-after-free on Clips player-page Piano Roll button at
    `StandaloneEditor.cpp:4048` lambda __l41 / __l10.  Same SafePointer
    or index-lookup fix.
  - **Folded in 2026-05-07 (QA-0 findings #16a + #16b + #21 via Rule 3)**:
    pattern row-level mute + per-pattern-block right-click "mute" + audio
    track row mute (with audio clip) all have asymmetric / non-functional
    behavior.  Pattern dispatch ignores both row and block mute states;
    audio row mute is sticky / no way to unmute.  Same surface family as
    DSP-09 bus solo (mute/solo dispatch).  Touch:
    `PatternManager::isRowAudible`, MIDI dispatch loop in
    `processBlock`, audio-row mute UI binding.
  - **Folded in 2026-05-08 (QA-Inventory close via Rule 3)** — REC-01 scope expansion:
    - **BLU-470** "Audio recording findings" — document master mix + per-track arm + debug pops; verify recording lifecycle works end-to-end.
    - **Vox recording not playing on Builder after recording** (QA-Inventory walk runtime test) — Vox strip records audio successfully, file lands in library, but builder grid playback shows the clip silent. Likely related to FilePlay routing (`mForcePitchBypass=true` set on FilePlay paths but never cleared after stop) OR the auto-spawned ClipsBus path stealing the audio (DSP-12 family).
    - **Inst recording not playing on Builder after recording** (same issue, also tested) — same surface as Vox; covers the parallel Inst path through `BaySickGuitars` / `BaySickBasses` / `BaySickPedals` chain.
    - **Pedalboard presets don't work** (QA-Inventory walk runtime test) — preset save/load for `BaySickPedalsProcessor` either round-trips wrong slot configuration or doesn't restore parameters. Same surface family as REC-01 (engine-level state restoration). NOTE: this also expands QA-Verify scope to verify ALL preset paths across all engines (Harmless, BaySickPlayer, BaySickSynth, BaySickBass, BaySickPedals, BaySickVocal, BaySickGuitars, BaySickBasses, BaySickRustyDrums, BaySickNAMIR).
- Scope: SINGLE coordinated batch because all touch the MixerPage spawn
  cascade + project XML restoration walker + StripRecorder finalize +
  bus DSP path. Splitting causes merge churn. Walk the full Vox/Inst
  lifecycle (create → save → close → reopen → load → record → reassign
  recording target → reload → delete) and fix every break in one pass.
  - Critical files: `MixerPage.cpp` (spawn cascade), `StandaloneEditor.cpp`
    (XML restore walker, `onTabClosed`, recording finalize, automation
    UUID resolver context), `VibeGraph.cpp` (bus solo logic + processBus),
    `BuilderPage.cpp` (Properties popup edit + dead line delete),
    `PluginProcessor.cpp` (rebuildRoutingFromApvts on FILE-02 reassignment).
- Risk: highest of any batch. Multi-file, multi-callback, audio + project
  serialization + bus DSP. Most likely to break unrelated paths.
- Dependencies: QA-0 (FILE-02's "fix Clips setup so they work in both
  places" depends on Composite RenderTask), QA-D (clean project load
  baseline).
- Effort: large (~10-14 hours, possibly multiple sessions).
- Trade-off: COULD split into QA-E1 (Vox/Inst lifecycle MIX-02..06 + REC-01),
  QA-E2 (DSP-09 bus solo standalone), QA-E3 (FILE-02 multi-record routing).
  User confirmed bundled (Q6 Option A). Re-evaluate at start if scope feels
  off; the split is mechanical.

#### **QA-F: Vox DSP Disconnect (Cluster 1, regression fixes only)**
- Items: DSP-02 (Vox FX bypassed), DSP-03 (Vox pitch correction does
  nothing), DSP-05 (BaySickAlign review).
- Scope: audit `BaySickVocalProcessor::processBlock` for FX-array pipeline
  wiring. DSP-02/03 likely co-occur. DSP-05 is a verification pass on
  warp markers reaching the phase vocoder path.
  - **Folded in 2026-05-08 (QA-Inventory close via Rule 3)** — DSP-03 sub-scope expansion:
    - **Formant Preserve + Throat Shift no-op stubs** in `PitchCorrectorDSP` (`Source/DSP/PitchCorrectorDSP.cpp:326-327`): code comment says *"Formant Preserve / Throat Shift toggles are stored but DSP is no-op for H-5 -- a follow-up batch will add cepstral envelope swap"*. The UI exposes the knobs with descriptive tooltips ("Keeps the vocal character intact while correction shifts pitch... pitch-shift artifacts (chipmunk-up, demon-down)") but the DSP literally `juce::ignoreUnused (mFormantPreserve, mThroatSemis);`. UI-promised, DSP-not-delivered. Wire actual cepstral-envelope swap. Affects both realtime path (BaySickVocals tab) and offline path (BaySickPitch).
    - **BaySickVocal H-1..H-6 cluster review** (BLU-445 / BLU-608 / BLU-609 / BLU-610 / BLU-611 / BLU-612) — QA-Inventory walk reclassified all six from "claimed Done" to "Review" because the realtime pitch correction was confirmed broken at runtime (YIN tracker not detecting pitch despite live audio reaching the engine; granular shifter idles at ratio=1.0 producing "faint vibration" artifact only). Whole subsystem (skeleton + comp ext + de-esser + YIN + pitch correction + editor) needs end-to-end re-verification as part of QA-F's `BaySickVocalProcessor::processBlock` audit.
- Risk: medium. Audio-thread DSP. MT-orthogonal at the inside-engine
  level (VoxStripTask calls engine.processBlock; the FX-array runs there).
- Dependencies: QA-E (shares VoxInsertNode surface).
- Effort: medium-large (~6-10 hours; folded items add 2-4 hours).

#### **QA-Fa: BaySickPitch Audio Import (additive feature, split from QA-F)**
- Items: DSP-04.
- Scope: drag-and-drop file listener on BaySickPitch, wire to the
  pitch-detection input path.
- Risk: low. Additive feature; no regression surface.
- Dependencies: QA-F (BaySickPitch must be functional first).
- Effort: small-medium (~2-3 hours).

### Phase 4 — Builder + UX work

#### **QA-G: Timeline Geometry**
- Items: BUILD-01 (100 tracks), BUILD-02 (ruler freeze), BUILD-03 (zoom
  alignment).
- Scope: refactor BuilderPage Viewport. Extract ruler from vertical
  Viewport, float-precision zoom math, bump array limit + recompute
  scrollable height.
- Risk: low-medium. UI-only.
- Dependencies: none.
- Effort: medium (~4-6 hours).

#### **QA-H: Builder Polish + Piano Roll Features**
- Items: BUILD-04 (ghost notes static), BUILD-05 ('s' keybind dead),
  BUILD-06 (WAV-clip stretch missing rebuild trigger), NAV-05 (REMOVE
  Builder hamburger), MIDI-01 (Ctrl+click row select), MIDI-02
  (control-lane dots follow slider), MIDI-03 (control-lane reference
  grid lines), MIDI-04 (Humanize tool).
  - **Folded in 2026-05-07 (QA-0 finding #15 via Rule 3)**: dropping a
    WAV/MP3 on Builder grid auto-navigates to the player page; user
    expects to stay on Builder.  Touches `onAudioClipAdded` cascade in
    `StandaloneEditor.cpp` -- remove the post-spawn tab-select.
  - **Folded in 2026-05-07 (QA-0 finding #17 via Rule 3)**: app-shutdown
    crash in `BuilderPage::~BuilderPage` -> `TreeView::~TreeView` ->
    `TreeViewItem::setOwnerView` walks dangling subItem pointer.
    Pre-existing destructor-ordering bug in BuilderPage's tree teardown
    (`BuilderPage.cpp:4418`).  Fix: ensure TreeView outlives its
    TreeViewItems OR restructure ownership so JUCE manages item lifetime.
  - **Folded in 2026-05-07 (QA-0 finding #18 via Rule 3)**: muting a
    block resets its loop count to 1 (block stays visible but silent +
    loop state lost).  Block-mute logic must preserve loop count.
  - **Folded in 2026-05-07 (QA-0 finding #19 via Rule 3)**: can't drag
    audio clips back onto Builder grid after deletion -- browser ->
    Builder drop only works for first-time imports.  Drop handler in
    `BuilderPage::ArrangementGrid::filesDropped` needs to accept
    library-resolved paths.
  - **Folded in 2026-05-07 (QA-0 finding #20 via Rule 3)**: UX gap --
    clicking a pattern / audio clip / automation in browser should make
    it the "active drop type" for clicks on empty Builder space
    (mimicking piano-roll last-block-type behavior).
- Scope: coherent UX pass on Builder + piano roll.  Plus folded Builder
  block state, drop handler, shutdown teardown, and UX click-to-place.
- Risk: low-medium. UI-only mostly. BUILD-06 calls
  `rebuildAudioClipPlayers()` (message thread, MT-aware).  Folded #17
  is a real teardown bug -- moderate risk if ownership restructure
  ripples.
- Dependencies: QA-G (timeline geometry foundation).
- Effort: medium-large (~8-12 hours; folded items add 2-4 hours).

#### **QA-I: Heavy Operation Progress Overlay**
- Items: NAV-02 (engine swap loading sign), APP-02 (shutdown overlay
  replacing black-screen), APP-03 (project load modal dialog), STATE-03
  (folded into APP-03 — symptomatic, fixed by UX), STATE-04 (already
  fixed in QA-D, gets visual feedback layer here).
- Scope: build reusable `Source/Standalone/HeavyOperationOverlay.h/.cpp`
  (modal/non-modal, step labels, progress bar, busy cursor). Wire each
  long-running operation to dispatch step updates.
- Risk: medium. Touches UI lifecycle around shutdown + load. APP-02
  window-management pattern needs careful Windows DWM testing.
- Dependencies: QA-D (STATE-04 playhead-stop) for the load-progress
  path to deliver perf benefit.
- Effort: medium-large (~6-10 hours).

### Phase 5 — Audio engine cleanup + UI polish

#### **QA-J: Multi-Clip Stacking Fix (DSP-06)**
- Scope: restructure per-row audio-clip rendering so the rack/EQ runs
  ONCE per row per block on the SUM of the row's clips, not N times per
  block.
  - **Folded in 2026-05-07 (QA-0 finding #16c via Rule 3)**: when an
    audio row is muted, the audio clip's streamer pauses at its current
    `expectedFilePos`; on unmute the streamer resumes from that frozen
    position rather than syncing to current project transport, causing
    visible desync.  Same surface (`renderAudioClipsForRow`, audio-clip
    streamer position management).  Fix likely advances streamer's
    expectedFilePos even when mute-gated, OR seeks on unmute to current
    project transport.
  - **Folded in 2026-05-08 (QA-Inventory close via Rule 3)** — BLU-501 "Prune stale applicators on swap" (memory cleanliness in audio-thread automation applicator map; memory leak on engine swap). Same surface family as the audio-thread renderAudioClipsForRow restructure.
- Risk: high. Architectural restructure, audio thread, MT-aware.
- Dependencies: QA-0 (composite task pattern established) + QA-E
  (audio clip surface stability).
- Effort: large (~8-12 hours; folded streamer-sync + applicator cleanup adds ~2 hours).

#### **QA-K: Audio Engine Polish**
- Items: APP-04 (SetPriorityClass + MMCSS), APP-05 (Open ASIO Control
  Panel button), DSP-08 (Tascam Model 24 outputs 21/22 stereo bug),
  DSP-11 (live ASIO buffer-size change), DSP-01 (Harmless lazersaw silent
  + headless preset audit test).
- Scope: small audio-system polish items. APP-04 ~5 lines in
  `VibesynthStandaloneApp::initialise`. APP-05 single button via
  `juce::AudioIODevice::showControlPanel`. DSP-08 needs hardware in
  hand. DSP-11 may end up "out of scope, document workaround".
- Risk: low-medium each.
- Dependencies: independent.
- Effort: medium (~4-6 hours total).

#### **QA-L: UI Polish**
- Items: UI-01 (right-click on PopupMenu activates item — JUCE wrapper),
  UI-02 (auto-lane "(deleted slot)" UUID — diagnose with UI-01), MIX-05
  (mixer strip overlap after delete — missing `resized()`/`repaint()`
  trigger), MIX-07 (Effects-page dropdown stale entries — verify why
  the wired callback doesn't fire on tab-close), NAV-01 (window resize
  layout — strict FlexBox/Grid + min size), NAV-03 (FX Rack button on
  player pages), NAV-04 (Piano Roll deep-link buttons), FILE-03
  (browser delete removes all duplicate-named instances — auto-numbering
  on duplicate drop).
  - **Folded in 2026-05-08 (QA-Inventory close via Rule 3)** — UI-Polish scope expansion:
    - **BLU-378** "Right-click Automate menu gap" (componentID on sliders) — UI-01 sub-item: sliders need stable `componentID` so the Automate menu can identify the target. PRESET-SAFE.
    - **BLU-379** "A9 slider-sync verify" — verification that all editor sliders use `SliderAttachment` correctly so APVTS round-trips and sync paths are consistent.
    - **LDT-394** "General UI Touch-ups (5F-8)" — Piano roll mouse accuracy + final spacing/alignment pass.
    - **BLU-492** "Combo-box automation infrastructure" — make combo-box selections become APVTS params so they're automatable. PRESET-BREAK at preset-format level (combo selections currently aren't in preset state).
    - **LDT-026** "D1.5 Per-drum MIDI input note + UI" — populate the MIDI Map placeholder in the per-drum context menu so pad-controllers can map to specific drums. Per-drum `mInputNote` field + UI.
    - **FSW-123** "Picker-disable-during-playback for Clips" — UX polish: disable engine/sound pickers while transport is playing on Clips tabs to prevent mid-playback engine swaps.
- Scope: collection of UI polish. Group so same surface touched once.
- Risk: low-medium each.
- Dependencies: independent.
- Effort: medium-large (~6-10 hours).
- Trade-off: COULD split into QA-L1 (UI-01/02 PopupMenu wrapper +
  automation UUID), QA-L2 (everything else). User stated preference for
  granular commits — recommend split if scope feels off at start.

#### **QA-M: Engine Restoration Lifecycle**
- Items: LIFE-01 (DrumKit kit-load destroys Rusty), LIFE-02 (re-add Rusty
  doesn't auto-reload kit).
- Scope: dedicated debug session in DrumPage / DrumKitGrid kit-load path
  + auto-reload kit on Rusty re-add.
- Risk: medium.
- Dependencies: independent.
- Effort: medium (~4-6 hours).

#### **QA-Drum-Polish: Per-drum MIDI Note Map (D1.5)** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-026 (D1.5 Per-drum MIDI Note Map for pad-controller mapping).
- Scope: implement per-drum `mInputNote` field that pad-controllers can map to. Populates the MIDI Map placeholder in the per-drum context menu (Phase D D1.4-fix(c) shipped placeholder; D1.5 wires it).
- Risk: low. Per-drum APVTS param + MIDI dispatch routing.
- Dependencies: QA-M (drum lifecycle stable).
- Effort: small-medium (~2-4 hours).
- Why this slot: drum-related work cluster.

#### **QA-N: DSP Meter Sum-of-Cores (DIAG-02)**
- Items: DIAG-02.
- Scope: refine the DSP meter under MT to sum audio-thread + per-worker
  times (or use wall-clock dispatch-entry-to-mAllDone). Target: "% of
  one core" reading that tracks total render work.
- Risk: low. Read-only measurement; no audio path changes.
- Dependencies: independent.
- Effort: small-medium (~3-5 hours).

#### **QA-VibeSlider: App-wide juce::Slider → VibeSlider refactor** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD (silly-name file when batch starts).
- Items: BLU-493 (App-wide refactor; PRESET-SAFE; ~150-300 sites).
- Scope: replace every plain `juce::Slider` instance across the app with `VibeSlider` (defined in `Source/Standalone/SharedUI.h:956`), which swallows right-click events. Without this, right-clicking a `LinearVertical` slider with snap-to-mouse enabled snaps the value to the click Y — UX bug whenever the user is trying to right-click to reach the Automate menu. Currently only EQ widget + DynamicParamsPopout + MixerTrackStrip pan/width/fader use VibeSlider; everything else (Harmless / BaySickSynth / BaySickBass / VibePlayer / Pedals / NAMIR / Vocal editors + all effect panels) still uses raw `juce::Slider`.
- Risk: low. Per-class subclass swap; `VibeSlider` inherits all `juce::Slider` API. Build verifies + per-page interactive sanity.
- Dependencies: independent (could run alongside any other batch).
- Effort: medium (~5-8 hours). 150-300 mechanical sites.
- Why this slot: blocks the right-click Automate workflow being usable across the app; runs late in Phase 5 because nothing depends on it but it's needed before QA-RC's UX checklist verification.

#### **QA-Verify: Phase 5A/5B/5C systems verification** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-169 (5A Project Serialization), LDT-170 (5B Template System), LDT-171 (5C Per-Engine Preset System).
- Scope: end-to-end verification that project save/load + templates + presets work correctly across every engine (Harmless, BaySickPlayer, BaySickSynth, BaySickBass, BaySickPedals, BaySickVocal, BaySickGuitars, BaySickBasses, BaySickRustyDrums, BaySickNAMIR). Includes the **pedalboard preset bug** confirmed in QA-Inventory walk runtime testing — `BaySickPedalsProcessor` preset save/load doesn't restore correctly. Per-engine: load every factory preset, verify all params restore + audio plays as expected; save user preset, reload, verify identical state; test save/load round-trip across project save/load.
- Risk: medium. Touches every engine's preset state path; pedalboard preset bug is concrete known regression.
- Dependencies: all preceding QA batches (must verify against final-state engines).
- Effort: medium-large (~6-10 hours; one engine at a time).
- Why this slot: late Phase 5 because final-state engines must be present. Feeds into QA-RC test plan.

#### **QA-Export: Audio Export rebuild + Project Bundle** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: FSW-065 (D-9 Export Audio rebuild — Ctrl+R, format/bitdepth/SR/tail; render path itself broken), LDT-172 (5D Audio Export — WAV/MP3/OGG codecs), BLU-529 (Project Bundle & Export — copy samples into target folder/.zip).
- Scope: rebuild the song-mode audio export pipeline. Currently the only render-to-WAV path is per-pattern right-click in BrowserPanel (`BuilderPage.h/.cpp`); no song-mode export, no MP3/OGG, no format/bitdepth/SR/tail options. **Plus** Project Bundle & Export: zip-all-samples-and-project-into-shareable-archive. User confirmed in QA-Inventory walk that zip bundle is REQUIRED (not deferred per original 5D-BUNDLE plan).
- Risk: medium. New audio-export code path. Bundle path involves filesystem operations on user samples.
- Dependencies: QA-Verify (need confirmed-working preset/state restore so exported project restores intact on the receiving end).
- Effort: large (~8-12 hours; export pipeline + bundle pipeline + format codecs + UI).
- Why this slot: late Phase 5 because depends on stable preset/state from QA-Verify.

#### **QA-RC: Pre-Release Test Plan + RC Build** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-414 (Q&A clean build + 2nd clean build + testing plan + test to failure) — original Phase 5G work that was never executed; expanded with QA-Inventory walk findings (LDT-096 menu audit, LDT-097 keybinds audit, LDT-296 Global Tooltip System review, FSW-303 global FX bypass verify).
- Scope:
  - **2nd clean build**: delete entire `build/` directory, fresh Release+Debug rebuild from scratch, audit ALL compiler warnings.
  - **Page-by-page test plan**: documented checklist covering every page (Builder, Mixer, Effects, Layers, Bass, Drums, Clips, Vox, Inst, Rusty, NAMIR) and every workflow (audio I/O, MIDI I/O, transport, effects, mixer routing, save/load, recording, automation, undo/redo). Execute the plan, log findings.
  - **Test to failure**: long sessions, large projects, edge cases (100 tracks, 50 audio clips, 10 plugins routed sidechains, hours of continuous playback, sample-rate switches mid-session, ASIO buffer-size changes, project reload while recording).
  - **Menu audit** (LDT-096): walk every menu (File menu / page menus / right-click menus / global menus) for completeness + correctness + keyboard shortcuts.
  - **Keybind audit** (LDT-097): verify every keybind in the catalog actually fires + does what it says + doesn't conflict.
  - **Tooltip review** (LDT-296): every UI control has tooltip, tooltip text accurate, tooltip explains action not implementation.
  - **Global FX bypass verify** (FSW-303): the master strip's global FX bypass actually bypasses every bus's rack — verify per-bus with audible test.
- Risk: zero (read-only verification). Findings spawn fixes via Rule 3 to other batches OR new follow-ups.
- Dependencies: QA-Audit + QA-Cleanup-1..4 should land first (don't waste time testing code that's about to get cleaned up). QA-Verify + QA-Export.
- Effort: large (~10-15 hours possibly multiple sessions). Bounded by app surface, not complexity.
- Why this slot: AFTER all bug-fix + cleanup work lands. The whole point is verifying the cleaned-up build.

### Phase 6 — Pre-Release Cleanup Audit (its own phase, AFTER all 15 bug batches)

The goal: ship a release that doesn't carry dead/bloated code, and where
"people who are most interested in seeing how the back end works" can read
the source without confusion from dormant or unhooked code paths. Phase 6
runs only after Phases 1-5 land — cleaning up code that hasn't been touched
yet would be wasted work.

Every component in the build gets classified into:
- **Dead** — not referenced, not hooked up, not invoked at runtime.
  Candidate for deletion.
- **Dormant** — present but inactive. Two sub-categories:
  - *Holding for future state* — keep with documented reason
    (inline `// HOLD-FOR-<reason>` comment + one-line entry in the
    implemented-work doc). Kept intentionally. The inline comment is
    the source of truth; the implemented-work entry is the searchable
    index of what's dormant and why.
  - *No reason found* — promote to Dead.
- **Active** — in use by the build today. No-action.

#### **QA-Audit: Codebase classification sweep (read-only)**
- Scope: sweep entire build (`Source/`, `libs/`, `Assets/`, `Kits/`,
  `Presets/`, `Templates/`, plus dev-repo scaffolding) and produce a
  classification manifest. NO code changes; this is the input doc that
  drives QA-Cleanup-1..4.
- Method:
  - File-level: cross-reference against `CMakeLists.txt` + `#include` chains.
  - Function/class/macro-level: grep for symbol references.
  - Asset-level: cross-reference against installer config + runtime load paths.
  - Vendored-lib-level: cross-reference against actually-linked targets
    (memory says over-pruning sfizz broke configure on 2026-05-03 — be careful).
- Output: manifest appended to the implemented-work doc, structured per
  top-level directory.
- Risk: zero (read-only).
- Dependencies: all 15 prior batches landed.
- Effort: large (~10-15 hours, possibly multiple sessions). Bounded by
  codebase size, not complexity.

#### **QA-Cleanup-1: Source code cleanup**
- Items: execute the source-code section of the QA-Audit manifest.
- Scope: delete Dead source files; add `// HOLD-FOR-<reason>` comments
  for Dormant + one-line implemented-work entries for each; clean up
  stale comments referencing deleted code.
- Risk: medium — wrong deletion breaks build.
- Mitigation: build after every delete; full verification ladder
  (Section 7's per-batch list) after each meaningful chunk.
- Dependencies: QA-Audit.
- Effort: medium-large (~6-10 hours).

#### **QA-Cleanup-2: Vendored libraries cleanup**
- Items: execute the `libs/` section of the QA-Audit manifest.
- Scope: prune unused vendored libs; for each kept lib, prune unused
  subdirs after grepping for unconditional `configure_file()` references.
- Risk: medium — over-prune breaks configure (sfizz precedent).
- Mitigation: `do_build.bat` configure + build after each lib pruned.
- Dependencies: QA-Audit.
- Effort: medium (~4-6 hours).

#### **QA-Cleanup-3: Assets + presets cleanup**
- Items: execute the assets section of the QA-Audit manifest.
- Scope: prune unreferenced assets, kits, presets, templates; verify
  installer config still produces a functional shipping bundle; document
  any Dormant kept for factory-content reasons.
- Risk: low-medium. Mostly drops bloat from installer.
- Dependencies: QA-Audit.
- Effort: medium (~4-6 hours).

#### **QA-Cleanup-4: Dev-repo scaffolding cleanup (non-shipping)**
- Scope: review and triage non-shipping dev artifacts:
  - `Files For Claude/` legacy docs — what's still relevant vs stale.
  - `build_*.txt`, `*_log.txt`, `null` and similar build-byproduct files
    that escaped `.gitignore`.
  - Old experimental `Tools/` scripts and `*.bat` variants.
  - **The three companion plan docs (plan + carry-forward +
    implemented-work) — KEEP per user direction; they are the historical
    record.**
- Decide per item: keep, `.gitignore`, or delete.
- Risk: low.
- Dependencies: independent of QA-Audit (could run alongside if desired).
- Effort: small-medium (~2-4 hours).

### Phase 7 — Documentation, Templates, Installer (added 2026-05-08 via QA-Inventory close — see §9)

These four batches were planned in the original `lucky-discovering-tiger` Phase 6 (Documentation, Templates, Presets & Installer) but had no representation in the post-Batch-10 Main Plan. QA-Inventory close adds them as a dedicated phase. Runs AFTER Phase 6 cleanup so docs/installer reflect the final cleaned codebase.

#### **QA-Manuals: 3 manuals (Quick Start + Music Tech + Design Tech)** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-176 + LDT-415 (Manual 1 Quick Start, 10-15 pages, annotated screenshots), LDT-416 (Manual 2 Music Technical Reference, 40-50 pages, every knob/button/slider organized by page/engine), LDT-417 (Manual 3 Design Technical Document, formulas + signal flow + architecture diagrams).
- Scope: write all three manuals. Image workflow per LDT-415 spec: Claude writes manual + detailed image descriptions, Jeff hands to Copilot for image generation, images returned for compilation. Update VibeDAW references to BaySickDAW.
- Risk: low (no code changes).
- Dependencies: QA-RC (need final stable feature set).
- Effort: large (~30-50 hours, multi-session).
- Why this slot: Phase 7 documentation runs after all features stable.

#### **QA-Templates: Factory templates + AI-Assisted Skill** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-177 + LDT-418 (in-app factory presets per engine + factory drum kits + 5 genre-specific starter templates: hip-hop / pop / electronic / lofi / orchestral) + LDT-419 (AI-Assisted Template & Preset Generation Claude Skill — separate skill document).
- Scope: build out factory presets (quantities TBD after engines + tested) for every engine; ship genre-specific templates; create the Claude skill document for AI-assisted preset/template generation per the workflow spec in `lucky-discovering-tiger.md:3487-3502`.
- Risk: low.
- Dependencies: QA-Verify (need preset system verified working) + QA-RC.
- Effort: medium-large (~10-20 hours).
- Why this slot: factory presets + templates ship with the installer.

#### **QA-Installer: NSIS Installer + TTF embed** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-178 + LDT-420 (NSIS Installer with sample-package downloads — `vibedaw_installer.nsi`, 11 sample packs from GitHub) + **LDT-173** (5E Font & Asset Bundling — embed TTF in `BaySickDAWAssets` BinaryData so VibeLAF font choices render correctly on clean Windows installs without relying on system fonts).
- Scope: build the installer; bundle TTFs alongside existing PNG/SVG assets; configure sample-pack download UI; update VibeDAW references to BaySickDAW; verify clean-machine install produces functional shipping bundle.
- Risk: medium. Installer build = first time touching NSIS for this project; bundling decisions affect download size + first-run experience.
- Dependencies: QA-Manuals + QA-Templates (installer ships them).
- Effort: medium-large (~10-15 hours).
- Why this slot: installer last; everything ships through it.

#### **QA-Framework: Framework Document** (added 2026-05-08 via Rule 3 — see §9)
**Plan file:** TBD.
- Items: LDT-179 + LDT-421 (Framework Document — architecture patterns reusable blueprint for future projects).
- Scope: distill BaySickDAW's architectural patterns (APVTS lazy registration, RetirementQueue<T>, closeAllDynamicTabs barrier, MT render-task DAG, source-mux engine wrappers, etc.) into a reusable framework document. Update VibeDAW references to BaySickDAW.
- Risk: zero (documentation only).
- Dependencies: independent (could parallel QA-Manuals).
- Effort: medium (~6-10 hours).
- Why this slot: meta-deliverable; ships as a separate document alongside the manuals.

---

## 6. Sequencing — Option A confirmed

**Bug-fix phases (1-5):**
```
QA-0a* → QA-0 → QA-Inventory*** → QA-Md** → QA-A → QA-B → QA-C → QA-D → QA-E → QA-F → QA-Fa
   → QA-G → QA-H → QA-I → QA-J → QA-K → QA-L → QA-M → QA-Drum-Polish**** → QA-N
   → QA-VibeSlider**** → QA-Verify**** → QA-Export****
```

\* QA-0a inserted 2026-05-07 ahead of QA-0 — Debug build workflow
setup so QA-0's dispatcher tripwire is useful in the user's
shipping-binary workflow. See §9 first entry.

\*\* QA-Md inserted 2026-05-07 immediately after QA-0 (originally
queued in Phase 5; promoted to Phase 1) — MT engine is a no-op in
Debug per finding #9; downstream batches need working MT under Debug
for diagnostic purposes. See §9.

\*\*\* QA-Inventory inserted 2026-05-07 between QA-0 and QA-Md —
comprehensive triage + bucket categorization across the three pre-QA
source docs (`Final Stretch Work.txt`, `vibedaw_blueprint.md`,
`.claude/plans/lucky-discovering-tiger.md`). Read-only; no source
code changes. Populates the new `Plans & Specs/` doc skeletons
(`Previously Implemented.md`, `Future State.md`) and routes
still-needed work per Rule 3. See §9.

\*\*\*\* Inserted 2026-05-08 at QA-Inventory close. **QA-Drum-Polish**
(after QA-M) — beginner UX polish on Phase D dynamic-drum architecture
(LDT-298 sound-pack ribbon, LDT-299 audition button, etc.). **QA-VibeSlider**
(after QA-N) — refactor every right-click-swallowing slider in the codebase
to the existing `VibeSlider` subclass (SharedUI.h:956); ~493 sliders flagged.
**QA-Verify** (after QA-VibeSlider) — quick verification batch that walks
every "Done-claimed-but-unverified" / E-bucket item flagged during the
QA-Inventory triage; targeted Release smoke pass per item. **QA-Export**
(after QA-Verify) — wire the Export Stems / Export Master flows that the
ribbon/menu placeholders point at. See §9 QA-Inventory close entry.

QA-0, QA-A, QA-B can run in **parallel** (different code surfaces, no
audio-path overlap between QA-A UI work and QA-0 dispatcher fix).
Everything Phase 2 onward is sequential per Option A.

**Pre-release cleanup phase (6) — runs ONLY after all of QA-0..N + the
2026-05-08 QA-Inventory close additions have landed and verified:**
```
QA-Audit  →  QA-Cleanup-1  →  QA-Cleanup-2  →  QA-Cleanup-3  →  QA-Cleanup-4  →  QA-RC****
```

QA-Audit is the keystone — it produces the manifest that drives 1..3.
QA-Cleanup-4 (dev-repo scaffolding) is independent and could ride
alongside QA-Audit if the user prefers; default sequencing keeps it last.
**QA-RC** (release-candidate verification) was added 2026-05-08 at
QA-Inventory close as the gate before Phase 7 — a full project lifecycle
sweep across the cleaned-up build to confirm nothing regressed during
the cleanup phase.

**Phase 7 — Documentation, Templates, Installer (runs ONLY after QA-RC):**
```
QA-Manuals****  →  QA-Templates****  →  QA-Installer****  →  QA-Framework****
```

All four added 2026-05-08 at QA-Inventory close. **QA-Manuals** — the
beginner manual + in-app help screens (LDT-218, LDT-219, etc.). **QA-Templates**
— factory project templates / starter packs (LDT-220, LDT-221). **QA-Installer**
— Windows installer build with embedded TTF fonts (LDT-173) and licence /
EULA flow. **QA-Framework** — final installable framework checks (icons,
version stamping, registry keys, signed binary path). See §9 QA-Inventory
close entry for the full per-batch source-trace.

---

## 7. Verification Approach

**Per-batch verification (every batch must pass before commit):**

> **Note (2026-05-07):** post QA-0a, every "build" step in this list
> produces both Release and Debug exes. The standing rule is to
> verify in the Debug exe FIRST (any `jassert` fires as a precise
> dialog you can screenshot), then re-run the same checks in Release
> as the actual user-facing test. See §9 first entry + CLAUDE.md
> Build System.

1. `do_build.bat` clean.
2. App launches, audio plays at default settings.
3. Open existing big project (5 Guitars + 5 Bass + 1 Rusty) — no crash,
   audio plays.
4. Save → close → reopen → load — round-trip clean.
5. **MT toggle round-trip**: hamburger → toggle MT off → audio plays
   serial-path identically → toggle on → audio plays MT-path identically.
   Settings.xml persistence verified across restart.
6. Item-specific repro from the unified backlog.
7. Regression sweep on neighboring items in the same cluster.

**QA-0-specific verification matrix (DSP-12 after composite lands):**

| Test | Expected (MT on) | Expected (MT off) |
|------|------------------|-------------------|
| WAV drop on Builder grid → Builder grid playback | Plays | Plays |
| WAV drop on Builder grid → piano roll playback (auto-spawned Clips page) | Plays | Plays |
| MP3 drop on Builder grid → Builder grid playback | Plays | Plays |
| MP3 drop on Builder grid → piano roll playback | Plays | Plays |
| WAV drop on Clips tab → Builder grid playback (when block placed) | Plays | Plays |
| MP3 drop on Clips tab → piano roll playback | Plays | Plays |
| Both placed simultaneously (Builder + piano roll) | Both play, summed | Both play, summed |

**Cross-batch verification (every 2-3 batches):**

1. `git log --oneline` since last verification — readable + each commit
   compiles + passes (1)-(5).
2. Multi-take recording session (8-bar loop, 5 instruments, 2 vocal
   takes, 1 audio clip drop, 1 MP3 drop) end-to-end. Confirms project
   XML restoration walker + recording finalize + clip browser + mixer
   strip lifecycle all healthy.
3. DSP meter under MT reads sensible values across batch sizes
   (32, 64, 128, 256, 512). After QA-N lands, also verify the
   sum-of-cores reading.

---

## 8. Critical Files (touched by ≥1 proposed batch)

Pre-flight a `git status` on each before starting any batch — uncommitted
work here is a red flag for cross-batch contamination.

- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — every audio-thread batch.
- [Source/PluginProcessor.h](Source/PluginProcessor.h) — APVTS, snapshot, barrier.
- [Source/Engine/RenderGraphDispatcher.cpp/.h](Source/Engine/RenderGraphDispatcher.cpp) — task
  registration, link rebuild. **QA-0 epicentre.**
- New: [Source/Engine/Tasks/CompositeAudioInsertTask.h/.cpp](Source/Engine/Tasks/CompositeAudioInsertTask.h) — created in QA-0.
- [Source/Engine/Tasks/AudioInsertTask.cpp](Source/Engine/Tasks/AudioInsertTask.cpp) +
  [ClipPageTask.cpp](Source/Engine/Tasks/ClipPageTask.cpp) — body becomes
  helpers invoked by composite in QA-0.
- [Source/Engine/Tasks/InstStripTask.cpp](Source/Engine/Tasks/InstStripTask.cpp) — idle-suspend
  gate. QA-C.
- [Source/Engine/Tasks/RustyDrumsProducerTask.cpp](Source/Engine/Tasks/RustyDrumsProducerTask.cpp) — idle-suspend gate parallel. QA-C.
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — onTabClosed,
  closeAllDynamicTabs, project XML restore walker, recording finalize,
  hamburger menu, automation lane resolver. QA-A (potentially), QA-C, QA-D,
  QA-E, QA-I, QA-L.
- [Source/Standalone/MixerPage.cpp](Source/Standalone/MixerPage.cpp) — Vox/Inst spawn
  cascade. QA-C, QA-E, QA-L.
- [Source/Standalone/BuilderPage.cpp](Source/Standalone/BuilderPage.cpp) — timeline
  geometry, block resize, dead Properties line, FILE-02 Properties popup
  edit. QA-E, QA-G, QA-H.
- [Source/Standalone/PianoRoll.cpp](Source/Standalone/PianoRoll.cpp) — MIDI features. QA-H.
- [Source/Standalone/EffectsPage.cpp](Source/Standalone/EffectsPage.cpp) — channel dropdown
  refresh path. QA-L.
- [Source/Standalone/SharedUI.cpp](Source/Standalone/SharedUI.cpp) — VKnob right-click,
  PopupMenu wrapper. QA-L.
- [Source/VibeGraph.cpp](Source/VibeGraph.cpp) — bus DSP, solo logic. QA-D
  (potentially), QA-E (DSP-09), QA-J.
- [Source/BaySickVocal/BaySickVocalProcessor.cpp](Source/BaySickVocal/BaySickVocalProcessor.cpp) — Vox FX pipeline. QA-F.
- [Source/Standalone/StandaloneApp.cpp](Source/Standalone/StandaloneApp.cpp) — settings.xml,
  initialise, MT preference. QA-K (APP-04).

---

## End of plan

Plan executes end-to-end without further blocking input. Each batch
gets its own per-batch implementation plan when started, in
`C:\Users\jeffm\Documents\BaySickDAW\Plans & Specs\Batch Plans\<silly-name>.md`. Each per-batch plan
follows the three-doc system + carry-over discipline established in
Section 0.

**Three companion docs (created on plan-mode exit):**
- [Carry-Forward Reference.md](Carry-Forward Reference.md) — architectural reference + decisions + per-item status
- [Implemented Work Log.md](Implemented Work Log.md) — running log of executed work + findings

**Cleanup at project end:** when all 20 batches have landed (15 bug-fix
in Phases 1-5 + 5 cleanup in Phase 6) and the plan is closed out, the
three companion docs become the historical record of this work. Keep
them in place; they're the source of truth for "why was X done this
way?" questions in future sessions. (Phase 6 explicitly excludes the
companion docs from its cleanup-targets list — they're scaffolding
that's earned its keep.)

First batch: **QA-0a — Debug Build Workflow Setup** (forked in 2026-05-07; see §9). Then QA-0.

---

## 9. Forks

Chronological log of scope/sequencing changes since plan write
(2026-05-07). Entries are append-only. Each entry pairs with inline
back-references in the affected sections (per §0 Rule's hybrid
convention).

### 2026-05-07 — QA-0a inserted before QA-0 (Debug build workflow setup)

**Trigger:** during the QA-0 plan-mode session, the user decided the
dispatcher's most-recent-wins fallback should be tightened to
`jassertfalse`. Investigation surfaced that `jassertfalse` is
compiled out of Release builds and `do_build.bat` only builds
Release, so the tripwire would never fire in user workflow.

**Deeper finding:** the user is a solo developer with no coding
background. Today's diagnostic loop is bottlenecked by describing
behavior in plain English (no precise error messages from Release).
A Debug build alongside Release short-circuits that loop — when a
`jassert` fires, Windows pops up a precise file:line dialog the user
can screenshot. Round-trips collapse from many to one.

**Decision:** insert QA-0a (Debug build workflow setup) BEFORE QA-0.
QA-0a's dispatcher tripwire (in QA-0) becomes genuinely useful from
the moment QA-0 lands.

**QA-0a scope:** modify `do_build.bat` to build both Release and
Debug; gate the embedded exe icon for Release-only (Debug exe shows
generic Windows .exe icon, so taskbar pins differentiate); append
" [DEBUG]" to the window title in Debug builds; cold-start triage of
existing `jassert` calls that fire on a clean default session;
document the new workflow in CLAUDE.md.

**Inline back-refs:**
- §0 Rule updated to document the hybrid annotation convention.
- §5 Phase 1 has new QA-0a entry above QA-0.
- §6 Sequencing arrow updated.
- §7 Verification gained a Debug-first note.

**Plan files:**
- QA-0a: `Batch Plans\i-want-you-to-adaptive-dongarra.md`
- QA-0: `Batch Plans\composite-merging-rivers-twilight.md`

**Verification:** QA-0a closes when the dual-config build produces
both exes, taskbar differentiation works, cold-start triage is done,
and CLAUDE.md reflects the new workflow.

### 2026-05-07 — Rule 3 added to §0 (findings-during-execution routing convention)

**Trigger:** during QA-0 Task 5 execution, multiple real-bug findings
had accumulated from QA-0a's cold-start triage that weren't in §5's
sequence (#8 MenuBarModel listener-dangle, #9 MT no-op in Debug, #13
InstPage* use-after-free in tab-click lambda). User raised the
question: how do we route these without fragmenting work or polluting
Phase 6?

**Decision:** add Rule 3 to §0 codifying the routing convention.
Findings touching a not-yet-started batch's surface fold into that
batch; findings touching a completed batch get a §5-entry annotation
+ §9 Forks entry but no new §5 row (commits stay sequential, plan
reflects conceptual ownership); findings with no surface match get
a new dedicated §5 batch row. Phase 6 reserved for dead-code cleanup
only.

**Inline back-refs:** §0 "Standing rules" header updated from "Two"
to "Three"; new Rule 3 paragraph added after Rule 2's non-negotiable
note.

**Application timeline:** convention applies retroactively to QA-0a's
findings at QA-0 close — finding #8 folds into QA-D, finding #13
folds into QA-E, finding #9 becomes a new dedicated §5 batch.

### 2026-05-07 — QA-0 close routings (Rule 3 first application)

QA-0 closed 2026-05-07 with DSP-12 verified in Release under both
MT-on and MT-off (Composite RenderTask sums clip-engine and
arrangement-clip flows correctly).  Final commits: `611db82` /
`f72cd09` / `df6f0a3` / `0ef0c95` / `4200479`.

Per Rule 3, findings accumulated during QA-0a + QA-0 execution were
routed at close.  Summary of routings:

**Folded into not-yet-started batches:**
- **QA-D** ← finding #8 (MenuBarModel listener-dangle during
  closeAllDynamicTabs cascade).  Same surface as QA-D's project-load
  teardown work.
- **QA-E** ← finding #13 (use-after-free in showPageForTab tab-click
  lambda, `0xDDDDDDDDDDDDDDDD` confirmed); finding #14 (same family,
  Clips player-page Piano Roll button); findings #16a + #16b
  (pattern row mute / right-click pattern block mute have no effect);
  finding #21 (track row mute permanently mutes audio clip).  All
  touch the same audio-row + tab-callback + mute-dispatch surfaces
  QA-E already targets.
- **QA-H** ← finding #15 (Builder drop auto-navigates to player page);
  finding #17 (BuilderPage TreeView destructor dangling subItem on
  app shutdown); finding #18 (muting a block resets loop count);
  finding #19 (can't drag audio clips back to Builder after deletion);
  finding #20 (UX: clicking a browser item should set "active drop
  type" for empty Builder clicks).  Builder UX + state cluster.
- **QA-J** ← finding #16c (audio row mute desync — streamer pauses
  at expectedFilePos; on unmute resumes from frozen position rather
  than syncing to current project transport).  Same surface as
  QA-J's per-row audio-clip rendering work.

**Promoted from Phase 5 deferred to Phase 1 immediate:**
- **QA-Md** (MT Engine Debug-Build Investigation, finding #9) —
  originally queued as deferred Phase 5 batch; promoted to Phase 1
  immediately after QA-0 because every downstream batch that touches
  audio code needs Debug to actually engage MT for diagnostic
  precision.  QA-0a built the diagnostic Debug build infrastructure;
  QA-Md restores the MT path under Debug so that infrastructure
  delivers value going forward.  New sequence:
  `QA-0a → QA-0 → QA-Md → QA-A → QA-B → ...`.

**Suppressed in vendored JUCE (not real bugs by design):** findings
#1-7, #10-12 from the QA-0a cold-start triage were suppressed via
inline `// jassert(...)` patches in JUCE source + the
`JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING` define + the em-dash
ASCII sweep + the voice-ctor sample-rate-at-source fix.  All shipped
in QA-0a commits (`b34c54d` / `a472a44` / `bd67fdf`).  Not routed
to future batches — already addressed.

**Inline back-refs:** §5 entries for QA-D, QA-E, QA-H, QA-J each
got "Folded in 2026-05-07" sub-bullets per Rule 3.  §5 has new QA-Md
entry between QA-0 and QA-A.  §6 sequencing arrow updated with
QA-Md slot + footnote.

**Plan files affected:**
- QA-0 plan: `composite-merging-rivers-twilight.md` (closed).
- QA-Md plan: TBD silly-name file when batch starts (next).
- §9 of this main plan: this entry (third Forks entry).

### 2026-05-07 — QA-Inventory inserted between QA-0 and QA-Md (comprehensive source-doc triage)

**Trigger:** post-QA-0 close, the three pre-QA source docs
(`Files For Claude/Final Stretch Work.txt`,
`Files For Claude/vibedaw_blueprint.md`,
`C:/Users/jeffm/.claude/plans/lucky-discovering-tiger.md`) intermix
shipped work, claimed-but-unverified work, abandoned work, and
future-state ideas without disambiguation. Without a dedicated
triage pass, every downstream batch's plan author re-parses the
same ambiguity. Additionally, the new `Plans & Specs/` doc
skeletons (`Previously Implemented.md`, `Future State.md`) created
in commit `c05ce61` need to be populated with verified content.

**Decision:** insert QA-Inventory between QA-0 (closed) and QA-Md
(next). Comprehensive review + bucket categorization across the
three docs:

- **A** — claimed not-done, still needed → walk one-by-one with user;
  route per Rule 3 (fold into existing §5 batch OR new dedicated §5 batch).
- **B** — claimed not-done, drop candidate → confirm with user;
  one-line entry in `Future State.md` "Considered & Dropped".
- **C** — claimed done + source-verified → entry in
  `Previously Implemented.md`.
- **D** — Claude future-state additions across Audio Quality,
  Performance, User Tools, Workflow Polish → entries in
  `Future State.md`. User direction: every plausible idea, not bounded.
- **E** — claimed done but unverified → walked with user alongside A;
  per-item Decision dropdown: Update (reroute to A) / Archive (reroute
  to C) / Delete (folds into Phase 6 cleanup carry-list).

**Source verification depth (locked):** headline-verify every claim
(file/class/function exists) + sample 20% of named sub-items per
claim + escalate to full verification on any miss + skip anything
already verified in the 2026-05-07 Carry-Forward Reference snapshot.

**Walkthrough medium:** Google Sheet created via the user's Drive
connector with all items pre-filled (`ID | Bucket | Source Doc |
Section/Line | Title | Original Status Claim | Brief Context |
Decision | Your Notes`); valid Decision values listed in a frozen
first-row legend; user fills Decision column at own pace; Claude
downloads sheet as CSV at checkpoints to apply decisions to plan
docs.

**Plan file:** N/A — executed inline from chat breakdown (per user
direction; no separate per-batch plan file in `Batch Plans/`).

**Inline back-refs:**
- §5 has new QA-Inventory entry between QA-0 and QA-Md.
- §6 sequencing arrow updated; new `***` footnote added.
- §9 Forks: this entry (fourth).

**Verification:** QA-Inventory closes when (a) every distinct item
across all three source docs has a recorded bucket + decision, (b)
`Previously Implemented.md` is populated for all bucket-C items, (c)
`Future State.md` is populated for B "Considered & Dropped" + all
D entries, (d) Main Plan §5 reflects all bucket-A foldings (existing
batch scope expansions) + new batches if any, (e) §9 Forks has a
"QA-Inventory close routings" entry chronicling all foldings/
additions/deletions-to-Phase-6, (f) `Implemented Work Log.md` has
the QA-Inventory closing entry.

### 2026-05-08 — QA-Inventory close routings (1429 items triaged across 3 source docs)

QA-Inventory closed 2026-05-08 after a multi-day triage pass.  Final
counts: 1429 items extracted from the three source docs; 1089 routed
to `Previously Implemented.md` after exact-title + cross-doc dedupe
(31 rows merged across 29 clusters; report at
`C:\Users\jeffm\.claude\plans\qa-inventory-dedupe-report.md`); 17
walked-to-Drop entries + 6 walked-to-D-bucket entries written to
`Future State.md`; 26 walked-to-Update entries (E→A reroute) folded
into existing §5 batch scopes per Rule 3; 9 brand-new §5/§6/§7
batches added for items with no existing surface match.

**Cluster decisions (per dedupe report):**
- Cluster 1 (BLU/FSW G-1.x parity) — keep all (FSW = primary; BLU
  rows merged into single FSW entry).
- Cluster 2 (BLU/FSW G-2 / G-3 / G-5 parity) — keep all (same
  treatment as Cluster 1).
- Cluster 3 (BLU/LDT D1.x dynamic-drum parity) — confirmed merge
  (LDT = primary; BLU rows referenced LDT).
- Cluster 4 (BLU/FSW F-1 per-pattern colour) — confirmed merge.
- Cluster 5 (BLU/LDT/FSW EQ8 §12 cluster) — confirmed merge; 5d
  (treble range-mapping bug) folded into §12 EQ8 entry rather than
  living as separate row.
- Cluster 6 (LDT L&F sprint cross-refs L1/L8/L9/L11) — keep all
  (kept the duplicate row count rather than collapsing because L&F
  sub-items shipped at different commits).
- Cluster 7 (Project zip-bundle / lifecycle) — confirmed merge.
- Cluster 8 (Mixer page review backlog) — confirmed merge.
- Cluster 9 (Audition vs playback level mismatch) — confirmed merge.
- Cluster 10 (Delete prompt UX) — keep both rows pending full
  delete-prompt review (deferred sweep).
- Cluster 11 (Browser panel collapsible) — confirmed merge.

**Folded into existing not-yet-started §5 batches (Rule 3):**
- **QA-A** ← STYLE-02 / LDT-167 (font + size sweep findings).
- **QA-E** ← REC-01 cluster: BLU-470 (vox + inst recordings not
  playing on Builder), pedalboard-preset round-trip (recording
  finalize touches the same surface).
- **QA-F** ← DSP-03 cluster: realtime pitch correction broken at
  runtime (BaySickVocal `mPitchCorrector.process()` is in the chain
  but YIN tracker never detects); Formant-Preserve / Throat-Sim
  no-op confirmed (PitchCorrectorDSP.cpp:326 `juce::ignoreUnused`).
  + BaySickVocal H-1..H-6 sub-items.
- **QA-J** ← BLU-501 (per-row audio-clip rendering polish).
- **QA-L** ← Clips/Browser cluster: BLU-378/379 (browser panel
  state), LDT-394 (clip context-menu), BLU-492 (clips ribbon
  badge), LDT-026 (clip rename), FSW-123 (picker-disable when no
  clip selected).

**New §5/§6/§7 batches added (no existing surface match):**

| Batch | Position | Source-trace |
|-------|----------|---------------|
| **QA-Drum-Polish** | after QA-M | LDT-298 (sound-pack ribbon), LDT-299 (audition button), LDT-300 (per-drum locked-state polish), LDT-301..305 (Phase D polish backlog). |
| **QA-VibeSlider** | after QA-N | ~493 sliders flagged across the codebase that swallow right-click; refactor each call site to `VibeSlider` (SharedUI.h:956). User-approved as own batch given scope. |
| **QA-Verify** | after QA-VibeSlider | walks every E-bucket "Done-claimed-but-unverified" item flagged during inventory; Release smoke pass per item; reroutes any miss to a fresh §5 follow-up batch. |
| **QA-Export** | after QA-Verify | wires the Export Stems / Export Master flows that the existing ribbon/menu placeholders point at (no audio path written yet). |
| **QA-RC** | after QA-Cleanup-4 | release-candidate sweep across the cleaned-up build before Phase 7 documentation/installer work begins. |
| **QA-Manuals** | Phase 7 | beginner manual + in-app help screens (LDT-218, LDT-219, etc.). |
| **QA-Templates** | Phase 7 | factory project templates / starter packs (LDT-220, LDT-221). |
| **QA-Installer** | Phase 7 | Windows installer with embedded TTF fonts (LDT-173) + EULA + signed binary path. |
| **QA-Framework** | Phase 7 | final framework checks (icons, version stamping, registry keys). |

**Walked-to-Drop (B bucket → `Future State.md` Considered & Dropped):**
17 entries written to Section 3 of `Future State.md`. Highlights:
12× Harmless UI polish items (cosmetic-only, decided against pre-v1.0);
BLU-423 (legacy DrumsPage refactor — superseded by Phase D dynamic-drum);
3× ambiguous spec items deferred from Phase 5F-5 / 5F-6; FSW-244 (Show
Input Diagnostics dialog — verified at MixerPage.cpp:1886-1976 as not
built, agreed to drop until proven needed); BLU-605 (voxRoll/instRoll
infrastructure — kept; reclassified as Drop because it's NEEDED for
Inst BaySickGuitars/Basses + reserved for future SFZ vocal, not work).

**Walked-to-D-bucket (B bucket → `Future State.md` Section 4):**
6 entries: BLU-088, BLU-146, BLU-147, BLU-407, FSW-121, FSW-330. Each
is a plausible post-v1.0 idea the user wants kept on the radar but
not in scope for the QA cycle.

**Phase 6.1 dedupe stats (subagent ab7220a69335dc191):**
- Input rows: 1120 (Done-claimed-and-verified across 3 docs).
- Output rows: 1089.
- Saved (rows removed): 31.
- Clusters with 2+ source rows: 29.
- Singletons: 1060.
- Output: `C:\Users\jeffm\.claude\plans\qa-inventory-deduped-final.tsv`.

**Side findings surfaced during walkthrough (routed at close, not
held over):**
- BaySickVocal realtime pitch correction broken at runtime — DSP path
  exists, YIN doesn't fire (process is wired in chain but reports
  "Detected --"). Routed to QA-F DSP-03 (existing surface) rather
  than CL-024 fresh entry (CL-024 was for T-Pain hard-tune, which is
  achievable via existing realtime params once DSP-03 lands).
- Pedalboard preset round-trip broken — verified user side; routed
  to QA-E REC-01 (recording lifecycle owns preset XML).
- LDT-173 TTF embed (font in installer) is distinct from STYLE-02
  font choices — kept as scope inside QA-Installer; not collapsed
  into QA-A.

**Inline back-refs:**
- §5 entries gained "QA-Inventory fold-in 2026-05-08" sub-bullets
  for QA-A / QA-E / QA-F / QA-J / QA-L.
- §5 has new entries for QA-Drum-Polish, QA-VibeSlider, QA-Verify,
  QA-Export, QA-RC, QA-Manuals, QA-Templates, QA-Installer,
  QA-Framework.
- §6 sequencing arrow rewritten with all 9 new batches; new `****`
  footnote covers the close additions.
- New Phase 7 section added in §6 between QA-RC and the §7 header.
- §9 Forks: this entry (fifth — QA-Inventory close).

**Plan files affected:**
- `Plans & Specs/Previously Implemented.md` — populated with 1089
  deduped entries (subagent output).
- `Plans & Specs/Future State.md` — Section 3 populated (17 Drops);
  Section 4 populated (6 D-bucket items).
- `Plans & Specs/Implemented Work Log.md` — header convention
  section added; existing entries bumped from `##` to `###` with
  PT timestamps; QA-Inventory close entry appended (Phase 7 of
  this batch).
- `Plans & Specs/Main Plan.md` — this entry + scope expansions
  noted above.
- `CLAUDE.md` — post-close cleanup pass scheduled (stale OPEN BUG
  drum-woofy entry, etc.).

**Verification:** every (a)-(f) condition from the QA-Inventory
insertion entry above met. Closure commit follows after Phase 6.5
(per-chunk commits) + Phase 7 (Implemented Work Log entry).
