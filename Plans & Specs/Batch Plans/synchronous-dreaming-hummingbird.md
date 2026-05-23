# QA-Ef — Serial (ST) Render-Path Deletion — Single MT Path — Plan (synchronous-dreaming-hummingbird)

**Canonical path:** `Plans & Specs/Batch Plans/synchronous-dreaming-hummingbird.md`
(mirrored here on ExitPlanMode; the `~/.claude/plans/` copy is deleted per
`feedback_plan_mirror_one_way.md`).
**For execution:** read this file top-to-bottom; checkboxes are the punch-list.
Paired running notes: `Plans & Specs/Running Notes/synchronous-dreaming-hummingbird.md`.
Structure follows the Main Plan §0 required-sections rule (locked 2026-05-11;
exemplar `federated-bouncing-cupcake.md`).

---

## Context

**Why this batch.** MT (`RenderGraphDispatcher`) has been the production,
default-ON render path since Batch 10. The codebase still carries a *second*,
hand-maintained serial ("ST") render path — the ~830-960-line tail of
`PluginProcessor::processBlock` after the MT early-return. Dual hand-maintained
ST/MT parity is a **proven recurring bug class**: the §9 twenty-fifth Forks
audit (2026-05-18) found three feeds that lived only in the serial tail and
silently never ran under MT (master recorder, MIDI recorder, metronome/count-in).
QA-Ea Task 0b fixed those by extracting a shared `applyPostMixRecordAndMetro`
helper, satisfying the "MT proven on all 3" gate (`f28319e`). With that gate met,
QA-Ef removes the structural source of the bug class: **delete the serial path so
MT is the single, unconditional render path.**

**Re-slot rationale.** QA-Ea Part B Task 1 (output-routing refactor) was STRUCK
as redundant but its source was left in tree, not reverted (owner's call — ST is
not the production binary, no current release). It carries a **known interim
ST-only L/B/D routing regression** (a null-test residual, un-root-caused). MT is
unaffected (every Task-1 edit lives after the MT early-return). Deleting the ST
path **clears that regression by deletion** and removes the dual-maintenance
burden before QA-Ed/Ee/Eb/Ec touch the audio path — which is why QA-Ef was
re-slotted up to immediately after QA-Ea (§9 twenty-seventh Forks; §6 arrow:
`QA-Ea -> QA-Ef -> QA-Ed -> QA-Ee -> QA-Eb -> QA-Ec -> QA-F`).

**Preserving the diagnostic.** The serial path's one enduring value was a bisect
tool: "is this bug in the *parallelism* or in the *logic*?" That is preserved
**without a duplicate code path** — a runtime serial-diagnostic mode that parks
all worker threads so the audio thread runs the entire graph itself through the
identical dispatcher/task code.

**Leave no serial ghost (Jeff, 2026-05-21).** The batch's purpose is single-path
clarity — a future session (or the doc-reader on its next grep) must never mistake
leftover serial code/comments for live, maintainable behavior. So every serial
artifact is removed or rewritten, never left as "dead but harmless": the serial
tail, the `gMultiThreadedEngineEnabled` *branch*, `routeInsertOutput` + the
now-dead serial branches inside the surviving MT helpers, and every comment that
implies a live serial path. The ONLY surviving "serial" concept is the worker-park
*diagnostic* mode, explicitly labeled as such.

**Risk: HIGH** — deletes the tail of the single hottest function. Mitigated by:
(1) a read-only pre-flight inventory re-verify (Task 1) before any cut; (2) the
serial-diagnostic mode landing *before* the deletion so the bisect tool never
lapses; (3) mandatory `/review-batch` at close (hot-path rip-out); (4) the §9
twenty-fifth mirrored/inert inventory as the starting checklist.

**Effort:** medium-large. **Bucket:** Cross-cutting Infrastructure.
**Dependencies / GATE:** "MT proven on all 3" — SATISFIED 2026-05-21 (Task 0b
`f28319e`). DSP-12 composite-task resolution must be confirmed present
(Task 1) so deleting the serial summation does not re-expose clip/audio-insert
silence.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC-scope | Delete the serial render tail; MT becomes the single unconditional path. | §5 QA-Ef entry (locked); root-cause fix for the ST/MT divergence bug class. |
| SC-diag-ui | **Reuse the Mixer hamburger "Multi-core Rendering" toggle** for the serial diagnostic. ON = full parallel; OFF = serial diagnostic. Keep the existing `<MultiCoreRendering>` settings.xml persistence. | Jeff 2026-05-21 (Q1). User-facing meaning of the toggle is unchanged ("use all cores or not"); only the *implementation* of OFF changes (park workers instead of a separate code path). Reuses UI + persistence + the sibling "Run MT Diagnostic" readout. |
| SC-serial-meaning | **Fully serial** — when OFF, ALL workers park and the audio thread does 100% of render work. | Jeff 2026-05-21 (Q2, answered "which most resembles the setup we are removing"). The deleted serial path ran with **zero** worker threads; fully-serial reproduces exactly that. "Keep one worker" would be *less* like the removed setup. |
| SC-cleanup-scope | **Both cleanups** fold into QA-Ef: (1) dead `busAnySolo` + `BlockContext.busAnySolo` slim-down; (2) orphaned L/B/D buffers (`mLayerEngineSum`/`mBassEngineBuf`) + QA-Ea Part B Task 2 leftover signature params. | Jeff 2026-05-21 (Q3). Both are dead *only because* the serial path is gone — this is their natural home. (1) was explicitly deferred to "QA-Ef ST deletion + MT BlockContext slim-down" by a QA-Ea code comment. |
| SC-routeinsert | **Option B — full removal ("leave no serial ghost").** Delete `routeInsertOutput`; collapse the two MT-shared helpers (`renderAudioClipsForRow`/`renderFilePlayPlayer`) to a single MT-only behavior (drop the `mtDest==null` serial branch + make the destination a required non-null buffer); sweep every stale serial-referencing comment. Folded into Task 2. | Jeff 2026-05-21 (Task 1 surface). A-style "dead but harmless" leftovers would re-surface to a future session / the doc-reader (which greps "serial"/"routeInsertOutput") as if serial were still live — defeating the batch's purpose. Cannot guarantee A is confusion-free, so B. |
| SC-slot | QA-Ef immediately after QA-Ea. | Jeff (§9 twenty-seventh Forks); already reflected in §5/§6. Not re-litigated here. |
| SC-name | Plan silly-name `synchronous-dreaming-hummingbird`. | My pick (`feedback_silly_name_is_my_pick.md`); fittingly on-theme. |

---

## Sub-spec calls surfaced for ExitPlanMode (recommendations Jeff can override)

- **SC-relabel (minor):** Keep the menu label **"Multi-core Rendering"** as-is
  (still accurate: checked = use all cores). I lean *keep* — no relabel needed
  since the user-facing meaning is unchanged. Jeff said in Q1 "I'd relabel it";
  if he wants an explicit diagnostic hint instead, options at approval:
  (a) keep "Multi-core Rendering" [lean]; (b) add a tooltip noting "off = single-core
  diagnostic"; (c) a different short label he supplies. ASCII + brand-casing apply.
- **SC-serial-confirm:** Locking **Fully serial** (SC-serial-meaning) as the
  answer to Jeff's Q2 question. Flagged here so approval doubles as explicit
  confirm (`feedback_plan_and_wait_for_explicit_confirm_on_semantics_changes.md`).
- **No other sub-spec calls open.** The dead-symbol inventory (Task 1) is a
  re-verify, not a decision; any *new* finding it surfaces routes via Rule 3 with
  the slot surfaced to Jeff.

---

## Files to modify

### Task 1 — Pre-flight inventory (READ-ONLY; no source change)
- `Source/PluginProcessor.cpp` — re-measure the exact serial-tail bounds + dead-symbol references.
- `Source/Engine/Tasks/CompositeAudioInsertTask.{h,cpp}` — confirm registered (DSP-12 resolved).
- `Source/Engine/RenderGraphDispatcher.cpp` — confirm MT master-publish path intact.
- Output → running notes (deletion map + dead-symbol list). Surface to Jeff before Task 2.

### Task 2 — Collapse to single MT path + serial-diagnostic park gate + leave no serial ghost
- `Source/Engine/VibeThreadPool.cpp` (`workerLoop`, ~line 156) — **NEW** park-when-OFF gate (the one net-new code block).
- `Source/Engine/RenderEngineFlags.h:17-44` — update `gMultiThreadedEngineEnabled` doc-comment (false now = workers park / serial diagnostic, not "serial loop").
- `Source/PluginProcessor.cpp` — (2b) remove the `if` wrapper (:1931) + `return;` (:2005); **delete the serial tail :2008-2837**; dedent the dispatch body. Delete serial-only dead symbols (`mLayerEngineScratch`/`mBassEngineScratch`, `pushScToEngine` lambda, serial-tail lock usages, serial `busAnySolo`). (2c) delete `routeInsertOutput` (:403 def / :579 decl); collapse `renderAudioClipsForRow` + `renderFilePlayPlayer` to MT-only (drop serial branch + nullable `mtDest`).
- `Source/Engine/Tasks/CompositeAudioInsertTask.cpp:154`, `Source/Engine/Tasks/VoxStripTask.cpp:107`, `Source/Engine/Tasks/InstStripTask.cpp:95` — (2c) update the 3 helper callers for the new non-null destination signature.
- `Source/Engine/SidechainPullHelper.h`, `Source/Engine/UpstreamLink.h`, `Source/DSP/EngineSidechainHelper.h`, `Source/VibeGraph.cpp`/`.h`, `Source/PluginProcessor.h` — (2c) serial-ghost comment sweep (rewrite/remove comments naming `routeInsertOutput` or implying a live serial path).
- `Source/Standalone/StandaloneEditor.cpp:5152-5214` — keep menu item 202 + its handler; update the now-stale ST/MT comments (no behavior change). Keep item 203 "Run MT Diagnostic" (CL-292).
- `Source/Standalone/StandaloneApp.cpp:228-267` — persistence unchanged; verify doc-comments still accurate.

### Task 3 — Folded Both-cleanups (touch the surviving MT path)
- `Source/PluginProcessor.cpp` — delete the dead MT-branch `busAnySolo` compute (was ~:1948-1958); delete orphaned `mLayerEngineSum`/`mBassEngineBuf` member decls + any Part B Task 2 leftover bespoke-`addFrom` signature params (per Task 1 inventory).
- `Source/PluginProcessor.h` — remove the deleted member declarations.
- `Source/Engine/BlockContext.h` — drop the `busAnySolo` field.
- `Source/Engine/Tasks/PassiveStripTask.cpp` (+ `MasterTask.cpp` if it reads it) — drop the `ctx.busAnySolo` consumer; `VibeGraph::processBus` already computes its own via `anyBusSoloed()` (QA-Ea Part A), so this is pure dead-field removal.

---

## Tasks

### Task 0 — Open commit (docs)
- [ ] Mirror this file to `Plans & Specs/Batch Plans/synchronous-dreaming-hummingbird.md`; delete the `~/.claude/plans/` copy.
- [ ] Add `**Plan file:**` pointer to the §5 QA-Ef entry (Main Plan.md:1125), backticked-path form matching sibling entries.
- [ ] Seed `Plans & Specs/Running Notes/synchronous-dreaming-hummingbird.md` per §0 running-notes required sections (title / purpose blockquote / pair ref / convention ref / `## Diagnostic Instrumentation Catalog` stub).
- [ ] `/draft-commit` → surface message + FULL `git status` → Jeff approves → commit (docs only).
- [ ] `/draft-doc running-notes` → apply.

### Task 1 — Pre-flight inventory re-verify (READ-ONLY)
Re-verify the §9 twenty-fifth inventory against CURRENT source (line numbers
drifted post-QA-Ea `c5c5deb`/`f28319e`/`c648fb7`). No source edits.
- [ ] Bound the serial tail exactly: first line after the MT `return;` through the `processBlock` closing brace.
- [ ] Confirm the 3 once-ST-only feeds are now genuinely shared and the unconditional dispatch body calls `applyPostMixRecordAndMetro` (verified at :1987), `drainMeterAtomicsForUI` (:1994), `measureDspLoadAndOverload` (:2004); confirm their *definitions* (:3046, :3117) sit OUTSIDE the tail.
- [ ] Confirm the **DSP meter cap `10.f`** (CL-291 Keep) lives inside `measureDspLoadAndOverload` (shared, def ~:3117), NOT in the tail — so it survives. If it is in the tail, preserve `10.f` on the shared path.
- [ ] Confirm `CompositeAudioInsertTask` is **registered** (not just present) so deleting the serial summation doesn't re-expose DSP-12 clip/audio-insert silence.
- [ ] Build the dead-symbol list: grep every reference to `mLayerEngineSum`, `mBassEngineBuf`, `mLayerEngineScratch`, `mBassEngineScratch`, `mLayerEngineLock`, `mBassEngineLock`, `routeInsertOutput`, the serial `busAnySolo` (~:2504), the `pushScToEngine` lambda. Classify each: dies-with-tail / MT-also-uses-keep / vestigial-in-both (the §9 twenty-fifth "separate open question" buffers).
- [ ] Confirm no offline/bounce path assumes the serial tail (Export is unwired/placeholder per QA-Export future — note if otherwise).
- [ ] Record the deletion map + dead-symbol classification in running notes (`/draft-doc running-notes`). **Surface to Jeff before starting Task 2.**

### Task 2 — Single MT path + serial-diagnostic worker-park gate
**2a — worker-park gate** (`VibeThreadPool::workerLoop`, top of the `while (!mShutdown)` loop):
```cpp
// QA-Ef: serial-diagnostic mode. When "Multi-core Rendering" is OFF, every
// worker parks so the audio thread (runUntilOrTimeout) runs the whole graph
// itself -- genuinely serial through the identical dispatcher/task code, no
// duplicate path. Reuses the existing waiter/waker bookkeeping so submit()
// wakes parked workers the instant parallel mode resumes on the next block.
if (! RenderEngine::gMultiThreadedEngineEnabled.load (std::memory_order_acquire))
{
    mImpl->activeWaiters.fetch_add (1, std::memory_order_release);
    waker.wait (-1);
    mImpl->activeWaiters.fetch_sub (1, std::memory_order_release);
    continue;
}
```
- [ ] Add the gate. Note the accepted trade-off: in serial mode `submit()` flutters parked workers awake (they re-check + re-park, never popping) — fine for a dev-only diagnostic; the audio thread drains everything via `runUntilOrTimeout`.

**2b — collapse processBlock to unconditional MT** (single atomic edit — the `if`/`return` removal and the tail deletion MUST land together or the block double-processes):
```text
BEFORE:                              AFTER:
  ...pre-branch (FilePlay pre-scan)    ...pre-branch (FilePlay pre-scan)
  if (gMT...) {                        // dispatch body, now unconditional:
     ...dispatch body...               ...busAnySolo(removed in T3)/mtCtx...
     return;                           mRenderDispatcher.dispatchBlock(...)
  }                                    applyPostMixRecordAndMetro(...)
  ...SERIAL TAIL (~830 lines)...       drainMeterAtomicsForUI()
}                                      measureDspLoadAndOverload(...)
                                     }
```
- [ ] Remove the `if (RenderEngine::gMultiThreadedEngineEnabled...)` wrapper (:1931) + the `return;` (:2005); dedent the dispatch body.
- [ ] Delete the serial tail — **confirmed bounds `PluginProcessor.cpp:2008-2837`** (function closes :2838) — + the serial-only dead symbols that vanish with it: `mLayerEngineScratch`/`mBassEngineScratch`, the `pushScToEngine` stack lambda, the serial-tail engine spin-lock *usages* (`:2027`/`:2068` — the lock OBJECTS survive: used by prepareToPlay :238/:243 + engine add/remove :4280+), the serial `busAnySolo` (`:2657`+).
- [ ] Update stale comments: `RenderEngineFlags.h:17-44` (false = workers park, not "serial loop"); `PluginProcessor.cpp:1918-1930` (no more branch); `StandaloneEditor.cpp:5152-5157,5200-5210` (toggle now parks workers).
- [ ] Keep menu items 202 (toggle) + 203 (Run MT Diagnostic) + the `<MultiCoreRendering>` persistence intact.

**2c — leave no serial ghost** (SC-routeinsert = B; after 2b the two helpers are MT-only-called, so this is safe):
- [ ] Delete `routeInsertOutput` (def `PluginProcessor.cpp:403`, decl `:579`).
- [ ] Collapse the two MT-shared helpers to MT-only: `renderAudioClipsForRow` (serial branch `:683-694`) + `renderFilePlayPlayer` (serial branches `:933-944` / `:963-...`) — drop each `if (mtDest == nullptr) routeInsertOutput(...)` branch; make the destination a required buffer (`mtDest` pointer → reference, rename `dest`); update the 3 MT callers (`CompositeAudioInsertTask.cpp:154`, `VoxStripTask.cpp:107`, `InstStripTask.cpp:95`).
- [ ] Serial-ghost comment sweep: grep `routeInsertOutput` + live-serial-path references across `Source/` (VibeGraph.cpp/.h, SidechainPullHelper.h, UpstreamLink.h, EngineSidechainHelper.h, the helper headers, StandaloneEditor.cpp) and rewrite/remove every comment naming the deleted function or implying a live serial path. Surface the sweep list in running notes; if large, show Jeff before applying.
- [ ] **Tell Jeff to build** (`do_build.bat`), Debug first. Verify scripts:
  - (1) Default (parallel, toggle ON): an arrangement exercising Layers + Bass + Drums + an audio-clip row (CompositeAudioInsertTask) + a Vox page + an Inst page (the `renderFilePlayPlayer`/`renderAudioClipsForRow` helpers edited in 2c); confirm all play correctly.
  - (2) Mixer hamburger → uncheck "Multi-core Rendering" during playback → audio stays correct (now fully serial: workers parked).
  - (3) With it OFF, "Run MT Diagnostic (2s capture)" during playback → AlertWindow shows ~100% main-thread / ~0% worker tasks (proves serial). Re-check ON → re-run → work distributes across workers.
  - (4) Toggle ON/OFF several times mid-playback → no glitches/dropouts on flip.
  - (5) The 3 once-divergent feeds in BOTH modes: master record (output WAV has real audio, not the old 104-byte stub); metronome + record count-in audible; MIDI record if a keyboard is on hand.
  - (6) Quit with toggle OFF → relaunch → confirm it persisted (workers park from launch; audio still correct).
  - (7) Release build: golden path (1) + (2).
- [ ] `/draft-commit` → surface message + FULL `git status` → approve → commit.
- [ ] `/draft-doc running-notes` → apply (incl. Diagnostic Instrumentation Catalog if any temp logging was added).

### Task 3 — Folded Both-cleanups (dead code on the surviving MT path)
- [ ] Delete the dead MT-branch `busAnySolo` compute (QA-Ea left it as dead state; ~:1948-1958).
- [ ] Drop `BlockContext.busAnySolo` field + its `PassiveStripTask`/`MasterTask` consumer(s). `VibeGraph::processBus` already self-computes via `anyBusSoloed()` (QA-Ea Part A) — pure dead-field removal. GUARDRAIL (§9 nineteenth Forks): do NOT re-introduce strip-level solo into bus gating.
- [ ] Delete orphaned `mLayerEngineSum`/`mBassEngineBuf` member decls + Part B Task 2 leftover bespoke-`addFrom` signature params (per Task 1 classification; only if confirmed unreferenced post-tail-deletion).
- [ ] **Tell Jeff to build**, Debug first. Verify (dead-code removal — nothing should change):
  - (1) Golden-path playback identical to end-of-Task-2.
  - (2) **Bus solo regression check** (the BlockContext.busAnySolo removal): solo Layers bus → only Layers audible; solo Drums → only Drums; solo FX/Clips/Vox/Inst → correct; clear → all return. All 11 buses (QA-Ea Part A coverage).
  - (3) Master record still produces real audio.
  - (4) Release golden path.
- [ ] `/draft-commit` → surface + approve → commit.
- [ ] `/draft-doc running-notes` → apply.

### Task 4 — Close sequence (mandatory order)
- [ ] `/draft-doc batch-close` — compile the Implemented Work Log entry from running notes.
- [ ] Review draft; apply to `Plans & Specs/Implemented Work Log.md` via Edit (parent session, not the agent).
- [ ] **`/review-batch QA-Ef`** (MANDATORY — hot-path rip-out). Address BLOCKER/NEEDS-FIX; defer NITs into the close entry.
- [ ] Route side findings per Rule 3: in-batch-resolved → close-entry routing table; outside-batch → §9 Forks entry + §5/§6/Future State edits, **surfacing slot options to Jeff** (don't pick).
- [ ] Strip any `Remove`-marked diagnostic-catalog entries (surface strip list to Jeff first). Confirm no temp logging survives.
- [ ] `/draft-commit` for the close → surface + approve → commit (separate from source commits — clean rollback boundary).

---

## Verification (end-to-end smoke, after all source tasks land)
1. Fresh launch (toggle persisted ON): full arrangement (Layers + Bass + Drums + audio clip + a Vox/Inst page) plays correctly.
2. Toggle OFF mid-playback → identical audio; "Run MT Diagnostic" confirms ~100% main-thread.
3. All 11 buses solo/mute correctly (QA-Ea Part A guardrail intact).
4. Master record → real-audio WAV; metronome/count-in audible; both modes.
5. Save → reload project → state intact; no dirty-on-load beyond the sanctioned FND-4 old-project case.
6. No `do_build.bat` warnings introduced by the deletion (dead-symbol removal should *reduce* warnings).
7. Release build golden path clean.

## Routing notes (Rule 3 application during execution)
- **Expected, already-deferred (do NOT re-open):** FND-4 old pre-Task-0c projects load empty + dirty (Jeff's call, no release); FND-5 Task-1 ST null-test residual (moot once ST is deleted — confirm it's gone post-Task-2, log the closure).
- **Known separate open questions (do NOT chase mid-rip-out):** the vestigial `mLayerEngineSum`/`mBassEngineBuf` NaN guards being "vestigial in BOTH paths" (§9 twenty-fifth out-of-scope) — SC-cleanup-scope folds the *buffer* cleanup in, but if Task 1 finds a live MT/shared reference, surface to Jeff before touching. DSP-meter sum-of-cores-under-MT (DIAG-02/QA-N) stays out of scope.
- **New findings:** route per Rule 3 — fold into an open §5 batch's scope or a §9 Forks entry; **surface the slot to Jeff** (`feedback_slot_placement_is_spec_call.md`). Never bulk-defer.

## Carry-Forward Reference touch points
- **§1 (MT Render Path Primitives)** — read at Task 1 + Task 2 start: dispatcher flow, `dispatchBlock` master-publish, the BOTH-branches helpers (`measureDspLoadAndOverload`/`drainMeterAtomicsForUI`/`rebuildLinks`), the RenderTask inventory. Note: every line:number is the 2026-05-07 frozen snapshot — re-verify (Task 1).
- **§6 (Patterns to Reuse)** — the "No dead wiring" rule *retires* for the render engine once the flag stops gating two paths; the Composite RenderTask is now the only summation path. Note this in the close entry so future readers don't apply the moot rule.
- **§4 (Decisions)** — DSP-12 = Composite RenderTask (confirm registered, Task 1).
