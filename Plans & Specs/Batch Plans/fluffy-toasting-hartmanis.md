# QA-MultiBlockHazard — Audio/Vox/Inst Multi-Call-Per-Block Stateful-Effect Hazard — Plan (fluffy-toasting-hartmanis)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/fluffy-toasting-hartmanis.md`
> Paired running notes: `Plans & Specs/Running Notes/fluffy-toasting-hartmanis.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule). Rule 9 commits: write the brief one-liner directly, skip `/draft-commit`.

## Context

QA-MultiBlockHazard was split out of QA-EffectsReview item (d) at the fiftieth Forks entry (2026-06-06) — it is an **engine/hot-path restructure**, not effect-DSP fidelity. It sits immediately after QA-EffectsReview (closed 2026-07-02), before QA-CutSelfReview (§6 arrow, 34 asterisks).

**The bug.** On **Audio / Vox / Inst** mixer strips, a strip's insert chain (`VibeGraph::processInsert` = polarity → preEQ → width → **rack** → postEQ → fader × mute × solo → PDC → peak) runs **once per source in the block instead of once per block**. Stateful DSP (delay lines, reverb tails, LFO phase, compressor envelopes) therefore advances 2–3× per block whenever sources overlap on a strip, and corrupts. Two exact sites:

- **Audio** — [`renderAudioClipsForRow`](Source/PluginProcessor.cpp:623) calls `processInsert` **inside** the per-clip loop. N overlapping arrangement clips = N rack passes. Plus Flow A's clip-engine pass in [CompositeAudioInsertTask.cpp:94](Source/Engine/Tasks/CompositeAudioInsertTask.cpp:94) = one more.
- **Vox/Inst** — [`renderFilePlayPlayer`](Source/PluginProcessor.cpp:641) is called **per routed clip** ([VoxStripTask.cpp:107](Source/Engine/Tasks/VoxStripTask.cpp:107) / [InstStripTask.cpp:95](Source/Engine/Tasks/InstStripTask.cpp:95)), and each call runs **both** `eng->processBlock` (the ENGINE — lines 865/889) **and** `processInsert` (866/890). So on Vox/Inst the engine itself, not just the rack, advances per-clip.

The engine already documents the multi-call as a known compensation: the CAS-max metering at [VibeGraph.cpp:2497-2511](Source/VibeGraph.cpp:2497) literally says "*processInsert is called MULTIPLE TIMES PER BLOCK … CAS-max preserves the maximum across all calls*." It compensates the **meter**, not the **DSP state**. That is the bug.

**This fix completes the original design intent — it does not invent one.** Carry-Forward §4 (Decisions Already Made) records the Composite-task shape as *"sums them internally **before insert DSP**. Matches serial-mode summation."* QA-0 built the composite task to kill the most-recent-wins silencing (DSP-12) but only summed the *post-rack outputs* — it never moved summation to *before* the rack. This batch finishes that.

**The fix (uniform rule): sum every source into one buffer, then run the shared processing once.**
- Audio: Flow A engine → `blockView` (raw, no rack); Flow B decodes each clip → adds **raw** to `blockView`; **one** `processInsert(blockView)` after both.
- Vox/Inst: decode each FilePlay clip → sum raw into one scratch; **one** `eng->processBlock` → **one** `processInsert` → route.

**Dependencies:** none functionally. QA-EffectsReview closed clean (its one carry-forward, BUILD-06 resize-rebuild half, is a different subsystem). Local `main` is 9 commits ahead of `origin/main` (the whole QA-EffectsReview + QA-Rules run) — a push before this HIGH-risk hot-path work is Jeff's call, noted at open.

**Risk:** HIGH — hot audio path under MT. **Mandatory full live-input + playback regression pass on Audio/Vox/Inst before close** (per §5 docket).

**Effort estimate:** code ~3–4h across 2 tasks (Task 2 is the bigger refactor — splitting a ~260-line helper + restructuring two strip tasks). Verification-heavy: Jeff's Debug→Release regression cycles on all three strip types are the real wall-clock.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC1 | **Two source commits.** Task 1 = Audio path (`renderAudioClipsForRow` + `CompositeAudioInsertTask`). Task 2 = Vox+Inst path (`renderFilePlayPlayer` split + `VoxStripTask` + `InstStripTask` + CAS-max comment). | Jeff, 2026-07-02 (AskUserQuestion). Natural code seam — Vox+Inst share `renderFilePlayPlayer`; Audio uses a separate helper. Independent rollback + regression per path on a HIGH-risk hot-path batch. |
| SC2 | **CAS-max metering comment (`VibeGraph.cpp:2497`) gets a comment-only update in Task 2** — note the multi-call is gone; CAS-max retained as a harmless single-call max. No metering code change. | Jeff, 2026-07-02 (AskUserQuestion). After both tasks, `processInsert` is once/block so the "MULTIPLE TIMES PER BLOCK" comment is stale; Rule 6 keeps comments honest. Folded into Task 2 (the comment is only fully stale once both paths are single-call). |
| SC3 | **Overlap semantics = SUM sources → single shared processing.** Audio: sum engine-flow + all clips → 1 rack pass. Vox/Inst: sum all FilePlay clips → 1 engine pass → 1 rack pass (engine sees the summed audio). | Original design intent (Carry-Forward §4 "sums them internally before insert DSP. Matches serial-mode summation"). Confirmed with Jeff via chat. Single-source case is bit-identical; only 2+ overlap changes (corrects). |
| SC4 | **Scope guard: N→1 only, never 0→1.** Idle/tail behavior unchanged — the rack still does NOT run on a strip with 0 sources this block. Gate the single `processInsert` on "≥1 source contributed." | Tightest scope matching the docket ("run the rack exactly ONCE per block" = collapse N calls to 1). Tail-past-clip-end is a separate future concern, explicitly OUT. |
| SC5 | **No decode-path unification.** `renderAudioClipsForRow` and `renderFilePlayPlayer` share ~180 near-identical decode lines; they stay separate. Only the per-clip→per-block boundary moves. | Don't-add-abstractions-beyond-the-task. Unifying two hot-path decoders is a separate refactor with its own risk. |
| SC6 | Plan-file silly-name = `fluffy-toasting-hartmanis` (assigned by plan-mode runtime; running-notes file matches). | Not a spec call (`feedback_silly_name_is_my_pick`); locked at plan-mode entry. |

---

## Sub-spec calls surfaced for ExitPlanMode

**No sub-spec calls open.** Both genuine decisions (commit split SC1, CAS-max comment SC2) were surfaced via chat before this plan body was drafted and answered by Jeff (Rule 5 compliance). The overlap-semantics model (SC3) and scope guard (SC4) were confirmed in the same chat. No decision is deferred to execution.

---

## Files to modify

### Task 1 — Audio path (rack once per block)
- [Source/PluginProcessor.h:594-616](Source/PluginProcessor.h:594) — `renderAudioClipsForRow` return type `void` → `bool` (returns "≥1 clip contributed"); update the doc comment to the new "sum RAW into mtDest; caller runs the rack once" contract.
- [Source/PluginProcessor.cpp:402-635](Source/PluginProcessor.cpp:402) — `renderAudioClipsForRow`:
  - Add `bool anyContributed = false;` at top.
  - **Remove** the per-clip `processInsert` call at [:623-624](Source/PluginProcessor.cpp:623) (the loop now only decodes + declicks + adds RAW `clipScratch` into `mtDest`, which the existing addFrom at :628-633 already does).
  - Set `anyContributed = true;` on each contributing clip; `return anyContributed;`.
  - Update the stale `5F-4a Batch 6 / QA-AudioMeters` comment at :616-622 (edited region, Rule 6).
- [Source/Engine/Tasks/CompositeAudioInsertTask.cpp:43-141](Source/Engine/Tasks/CompositeAudioInsertTask.cpp:43) — `run()`:
  - `bool anySource = false;` after `blockView.clear()`.
  - **Flow A:** remove the `processInsert` at [:94-95](Source/Engine/Tasks/CompositeAudioInsertTask.cpp:94); engine writes raw to `blockView`; set `anySource = true` when `clipEngine != nullptr`.
  - **Flow B:** convert the four early-`return`s at [:111-114](Source/Engine/Tasks/CompositeAudioInsertTask.cpp:111) into a **guarded `if` block** (so the single `processInsert` below still runs for the Flow-A-only case). `if (renderAudioClipsForRow(...)) anySource = true;`.
  - After both flows: `if (anySource) mGraph->processInsert(Audio, mIndex, blockView, bpm, anySolo);` — the single rack pass.
  - Update the stale Flow-A/Flow-B/QA-AudioMeters comments at :53-110 (edited region, Rule 6).
- [Source/Engine/Tasks/CompositeAudioInsertTask.h:12-42](Source/Engine/Tasks/CompositeAudioInsertTask.h:12) — update the header doc comment (Order B / "processInsert applies the per-row chain in-place" per flow → "sources summed, single rack pass").

**Fix skeleton (CompositeAudioInsertTask::run):**
```cpp
blockView.clear();
pullSidechainPredecessorsToGraph(*mGraph, channelId, mPredecessors, n);
bool anySource = false;

// Flow A: clip-engine writes RAW into blockView (no rack yet).
if (auto* clipEngine = mClipEngine.load(std::memory_order_acquire)) {
    /* SC push (unchanged) */  clipEngine->processBlock(blockView, *midi);
    anySource = true;
}

// Flow B: arrangement clips sum RAW into blockView (guarded, not early-return).
if (mCtx->posInfo && mCtx->posInfo->getIsPlaying()
    && mProcessor->mPatternManager
    && mProcessor->mSongMode.load(std::memory_order_relaxed)) {
    /* build clipCtx (unchanged) */
    if (mProcessor->renderAudioClipsForRow(mIndex, clipCtx, &blockView))
        anySource = true;
}

// ONE rack pass on the summed sources (N->1, never 0->1).
if (anySource)
    mGraph->processInsert(VibeGraph::InsertKind::Audio, mIndex, blockView, mCtx->bpm, mCtx->anySolo);
```

### Task 2 — Vox+Inst path (engine + rack once per block)
- [Source/PluginProcessor.h:618-649](Source/PluginProcessor.h:618) — replace the single `renderFilePlayPlayer` decl with **two**:
  - `bool decodeFilePlayClip (AudioClipPlayer&, const AudioClipBlockContext&, juce::AudioBuffer<float>& sumDest)` — decode ONE clip into `ctx.clipScratch`, declick, ADD raw into `sumDest`; returns "contributed."
  - `void finalizeFilePlayStrip (int routeCh, const AudioClipBlockContext&, juce::MidiBuffer& engineMidi, juce::AudioBuffer<float>* mtDest, juce::AudioBuffer<float>& engineSum)` — run the engine ONCE + `processInsert` ONCE + route, on the summed `engineSum`.
  - New doc comments for both.
- [Source/PluginProcessor.cpp:637-902](Source/PluginProcessor.cpp:637) — split `renderFilePlayPlayer`:
  - `decodeFilePlayClip` = the decode half (range/mute/choke/EOF checks + PV-or-direct read into `clipScratch` + F3 declick, [:649-827](Source/PluginProcessor.cpp:649)), ending by adding raw `clipScratch` → `sumDest` and returning true.
  - `finalizeFilePlayStrip` = the tail ([:829-901](Source/PluginProcessor.cpp:829)): `pushScToEng` lambda + the `isVoxRoute`/`isInstRoute` branch (BaySickVocal `setForcePitchBypass(true)` for Vox), operating on `engineSum` in place: `eng->processBlock(engineSum, engineMidi)` → `processInsert` → `mtDest->addFrom(engineSum)`.
  - Update the `Batch 9b Item 9` banner comment at :637-640 (edited region, Rule 6).
- [Source/Engine/Tasks/VoxStripTask.cpp:44-110](Source/Engine/Tasks/VoxStripTask.cpp:44) — FilePlay branch: size + clear `mEngineScratch` (the sum, 2ch × n) before the loop; `for each routed player: if (decodeFilePlayClip(player, clipCtx, mEngineScratch)) any = true;`; after loop `if (any) finalizeFilePlayStrip(channelId, clipCtx, engineMidi, &blockView, mEngineScratch);`. Keep the existing SC pull + gates.
- [Source/Engine/Tasks/InstStripTask.cpp:42-98](Source/Engine/Tasks/InstStripTask.cpp:42) — same restructure as VoxStripTask.
- [Source/Engine/Tasks/VoxStripTask.h:13-32](Source/Engine/Tasks/VoxStripTask.h:13) + [Source/Engine/Tasks/InstStripTask.h:12-34](Source/Engine/Tasks/InstStripTask.h:12) — update the header doc comments describing the FilePlay flow (edited region, Rule 6).
- [Source/VibeGraph.cpp:2497-2511](Source/VibeGraph.cpp:2497) — **comment-only** update per SC2: the multi-call is gone; note CAS-max is now a single-call max retained as harmless safety. No code change.

**Fix skeleton (VoxStripTask / InstStripTask FilePlay branch):**
```cpp
// (gates + clipCtx setup unchanged)
mEngineScratch.setSize(2, n, false, false, true);
mEngineScratch.clear();                       // the per-strip SUM buffer
bool any = false;
for (auto& player : mProcessor->mCurrentBlockClipSnapshot->players) {
    if (player.routeChannel != channelId) continue;
    if (mProcessor->decodeFilePlayClip(player, clipCtx, mEngineScratch)) any = true;
}
if (any)
    mProcessor->finalizeFilePlayStrip(channelId, clipCtx, engineMidi, &blockView, mEngineScratch);
return;   // FilePlay handled; skip live-input branch (unchanged)
```

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/fluffy-toasting-hartmanis.md` → `Plans & Specs/Batch Plans/fluffy-toasting-hartmanis.md` (Write tool); delete the home-dir copy.
- [ ] Add `**Plan file:** [\`Plans & Specs/Batch Plans/fluffy-toasting-hartmanis.md\`](Batch Plans/fluffy-toasting-hartmanis.md)` line to the Main Plan §5 QA-MultiBlockHazard docket header (backticked-path form, matching neighbors).
- [ ] Seed `Plans & Specs/Running Notes/fluffy-toasting-hartmanis.md` per §0 required sections (title / purpose blockquote / pair ref / convention ref + initial "Task 0: open" entry).
- [ ] Surface full `git status` (every dirty + untracked entry) with disposition. Write the brief Task 0 one-liner directly (skip `/draft-commit`); surface message + status; commit on approval.
- [ ] Mark Task 0 done; update todos.

### Task 1 — Audio path: rack once per block
- [ ] Re-read `renderAudioClipsForRow` (PluginProcessor.cpp:402-635) + `CompositeAudioInsertTask::run` (43-141) immediately before editing.
- [ ] `renderAudioClipsForRow`: change signature to `bool`; add `bool anyContributed`; remove the per-clip `processInsert` (:623-624); set/return `anyContributed`; keep the raw `addFrom` into `mtDest`. Update the :616-622 comment.
- [ ] `PluginProcessor.h`: `bool renderAudioClipsForRow(...)` + doc-comment rewrite to the new contract.
- [ ] `CompositeAudioInsertTask::run`: add `anySource`; drop Flow A `processInsert`; convert Flow B early-returns to a guarded block; add the single post-flows `processInsert` gated on `anySource`. Update stale comments + the `.h` header comment.
- [ ] **Rule 8 self-check before build:** confirm gating is bit-identical to today — `processInsert` fires iff `(clipEngine != nullptr) || (≥1 clip contributed)` (matches the pre-fix "Flow A always + Flow B per-clip" union). Confirm no audio-thread allocation added (reuses `blockView` + existing `clipScratch`).
- [ ] Tell Jeff: "Run `do_build.bat`. Debug FIRST, then Release. Audio-strip tests:
  - **(1) Single-clip regression.** One audio clip on a Clips-page/audio row, a Delay (or Reverb) in that strip's FX rack. Play. It should sound exactly as it did before this batch (this is the 99% path — must be unchanged).
  - **(2) Clips-page engine regression.** A Clips page with a sampler engine driven by its piano roll, Delay in the rack. Play. Unchanged.
  - **(3) THE FIX — overlapping clips.** Put **two audio clips overlapping in time on the SAME audio row/strip**, Delay in that strip's rack. Play across the overlap. Pre-fix: the delay taps double/thicken during the overlap (two rack passes). Post-fix: one clean delay. (If placing two clips on one strip isn't how your workflow does it, tell me and we'll find the real gesture — I derived this from `renderAudioClipsForRow` iterating all same-owner-row clips.)
  - **(4) Engine + clip together.** A Clips-page engine AND an overlapping arrangement clip on the same strip, compressor in the rack. Both now share ONE rack pass (correct: the strip is one signal). Confirm no pumping-doubling artifact."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface full `git status`; write the brief Task 1 one-liner directly; surface + commit on approval. Stage only the 4 Task-1 files.
- [ ] `/draft-doc running-notes` → apply to running-notes file.

### Task 2 — Vox+Inst path: engine + rack once per block
- [ ] Re-read `renderFilePlayPlayer` (PluginProcessor.cpp:641-902) + both strip-task FilePlay branches immediately before editing.
- [ ] Split `renderFilePlayPlayer` → `decodeFilePlayClip` (decode half, add raw to `sumDest`, return contributed) + `finalizeFilePlayStrip` (engine once + `processInsert` once + route, on `engineSum`). Update `PluginProcessor.h` decls + doc comments + the .cpp banner.
- [ ] `VoxStripTask::run` FilePlay branch: size+clear `mEngineScratch` as the sum; loop `decodeFilePlayClip` accumulating into it + tracking `any`; after loop `if (any) finalizeFilePlayStrip(channelId, …)`. Preserve the SC pull + song-mode gates.
- [ ] `InstStripTask::run` FilePlay branch: same restructure.
- [ ] Update `VoxStripTask.h` + `InstStripTask.h` FilePlay-flow header comments.
- [ ] `VibeGraph.cpp:2497` CAS-max comment-only update per SC2 (no code change).
- [ ] **Rule 8 self-check before build:** confirm the engine now sees the SUMMED clip audio (SC3) and runs exactly once when ≥1 clip contributes, zero when none do (matches the pre-fix loop that skipped when no clip was in range). Confirm `mEngineScratch` is sized with `avoidReallocating=true` (no audio-thread malloc). Confirm the live-input (non-FilePlay) branch is untouched.
- [ ] Tell Jeff: "Run `do_build.bat`. Debug FIRST, then Release. Vox/Inst tests (use your prerecorded-audio-through-the-page rig):
  - **(1) Vox live regression.** Arm a Vox strip on a live input, monitor + record. Unchanged. Dry recording still captured pre-chain.
  - **(2) Vox FilePlay single-clip regression.** Route ONE prerecorded vocal clip to a Vox (BaySickVocal) strip, Delay in the rack. Play. Pitch/formant + delay sound exactly as before.
  - **(3) Vox FilePlay — THE FIX.** Two prerecorded vocal clips **overlapping on the same Vox strip**, Delay in the rack. Pre-fix: delay doubles + the pitch engine advances twice during the overlap. Post-fix: one clean pass.
  - **(4) Inst live regression.** A BaySickGuitars/Basses (sfizz) Inst tab playing from its piano roll; and a live-input Inst tab armed. Both unchanged. Idle-suspend still works (silence → suspend → wakes on note/audition).
  - **(5) Inst FilePlay single-clip regression.** One clip routed to an Inst strip. Unchanged vs before."
- [ ] Wait for Jeff's Debug+Release verify result.
- [ ] On pass: surface full `git status`; write the brief Task 2 one-liner directly; surface + commit on approval. Stage only the Task-2 files.
- [ ] `/draft-doc running-notes` → apply.

### Task 3 — Close sequence
- [ ] `/draft-doc batch-close` — compile the Implemented Work Log entry from the running-notes file.
- [ ] Review draft; apply to `Plans & Specs/Implemented Work Log.md` via Edit.
- [ ] `/review-batch QA-MultiBlockHazard` — audit diff vs this plan + CLAUDE.md rules + memory gotchas.
- [ ] Address BLOCKERs / NEEDS-FIX in-batch. Defer NITs into the close entry's routing table.
- [ ] Route side findings per Rule 3 (in-batch → close routing table; outside-batch → §9 Forks + §5/§6/Future State — surface placement options to Jeff, don't pick).
- [ ] Strip any `Remove`-disposition diagnostics from the catalog (surface strip list for approval first).
- [ ] Surface full `git status`; write the brief close-commit one-liner directly; surface + commit the close (separate commit from source commits).

---

## Verification (end-to-end smoke)

After Task 2 commit lands, one combined Debug+Release pass:
1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **Audio single-clip = identical** to pre-batch (delay/reverb in rack).
3. **Audio overlap = single clean rack pass** (no doubled delay during overlap).
4. **Clips-page engine playback** unchanged.
5. **Vox live monitor + record** unchanged; dry recording pre-chain.
6. **Vox FilePlay single + overlap** — single unchanged, overlap now single-pass.
7. **Inst live (sfizz MIDI + live input)** unchanged; idle-suspend intact.
8. **No new CPU spike / glitch under MT** on a busy project (several Audio/Vox/Inst strips active). Confirm in Release (Debug runs slower; MT audio that glitches in Debug under load may be fine in Release — always confirm the real user test in Release).

---

## Routing notes (Rule 3 application during execution)

- Findings about the shared decode paths (e.g. a decode bug common to both helpers) → fix in-batch if it's in the edited region; route to §9 + Future State (SC5 keeps unification out of scope) otherwise.
- If the audible delay-doubling test (Task 1 (3) / Task 2 (3)) is ambiguous, a **temporary** per-`(kind,index)` `processInsert` call-counter DBG (logged 1×/sec) can confirm N→1. Only add if needed; catalog it in the running-notes `## Diagnostic Instrumentation Catalog` in the same edit pass (Site / Tag / Purpose / Disposition=`Remove at task close`) per §0 Rule 4; strip before commit.
- Any Vox/Inst-FilePlay-through-engine semantic surprise (e.g. Inst sfizz ignores input audio) surfaced during verify → note in running notes; it's orthogonal to the multi-call fix (out of scope) unless it blocks the regression pass.

---

## Carry-Forward Reference touch points

- **Before Task 1:** §2 (AudioClipSnapshot RCU — the `mCurrentBlockClipSnapshot` iterated by both helpers is alive for the whole block via the RetirementQueue ack protocol; no per-site lock).
- **Before Task 2:** §1 (VibeGraph render tasks — `VoxStripTask`/`InstStripTask` read `mLiveInputSnapshot`; Inst is source-mode aware) + §4 (Decisions Already Made — the "sum before insert DSP, matches serial-mode summation" intent this batch completes) + §6 (Patterns To Reuse — "Composite RenderTask … sums them before insert DSP").
- **Throughout:** §1 dispatcher/BlockContext (single MT render path post-QA-Ef; `gMultiThreadedEngineEnabled` gates worker-park vs full-parallel within the same dispatcher — MT works in Debug AND Release, QA-Md).
