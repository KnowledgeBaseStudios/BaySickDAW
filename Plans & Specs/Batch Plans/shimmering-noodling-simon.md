# QA-EngineApvts — Engine processors APVTS dirty-flag compliance (perf-audit M2) — Plan (shimmering-noodling-simon)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/shimmering-noodling-simon.md`
> Paired running notes: `Plans & Specs/Running Notes/shimmering-noodling-simon.md`

> **For execution:** steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule). All source changes land in ONE consolidated commit (Task 1) per the Jeff-locked commit-granularity decision.

## Context

QA-EngineApvts is the tail of the perf-audit cluster (origin: `/perf-audit` M2 at QA-Eg close, 2026-05-24; §9 thirty-fifth Forks entry). The 4 legacy engine processors — `HarmlessProcessor`, `VibePlayerProcessor`, `BaySickSynthProcessor`, `BaySickBassProcessor` — call `updateFromApvts()` **unconditionally every block**, each reading ~30–50 parameters via `apvts.getRawParameterValue(id)->load()` whether or not anything changed. In a real-time DAW (128-sample buffer ≈ 2.6 ms budget) this is per-instance cache pollution / L1 eviction that compounds across a many-track session toward dropouts. The fix brings these 4 engines into compliance with the documented BaySickDAW dirty-flag pattern (`feedback_apvts_dirty_flag_pattern.md`; reference impl `PluginProcessor.cpp:178`): only run `updateFromApvts()` when a parameter actually changed.

**Critical mid-planning finding (drove the architecture):** each engine *already* owns an `ApvtsDirtyTracker mDirtyTracker { apvts }` (added 2026-05-05) — a `juce::ValueTree::Listener` on the engine's `apvts.state` that fires `onAny` for the project-dirty title-bar `*` marker. Two further facts, both verified against source:

1. The tracker is the natural home for the perf flag (single centralized watcher — no parallel listener/atomic per engine).
2. **`replaceState` orphans the tracker.** Loading a preset/sound/project calls `apvts.replaceState(...)` (`juce_AudioProcessorValueTreeState.cpp:407` → `state = newState`). Per `juce::ValueTree::operator=` (`juce_ValueTree.cpp:612`), listeners live on the *wrapper*, so only APVTS's own wrapper re-points; the tracker's separate `mState` wrapper keeps pointing at the discarded tree and never fires again. With a naive gate, an engine would stop responding to knob turns after any preset/project load. `replaceState` is called from **17 sites** across the 4 engines + their editors + the Layers/Bass/Drum pages.

**Architecture (Jeff-locked across 4 rounds, 2026-05-30):** reuse the centralized tracker; route every state swap through a single per-processor `replaceApvtsState()` choke-point that re-subscribes the tracker (`resync()`); add a lock-free `hasChangedSinceLastBlock()` gate with a pointer-compare self-heal backstop; treat a host-tempo change as a parameter change in the two tempo-synced engines. Plus the QA-VoicePool-folded `BaySickSynthVoice::startNote` osc-reset (§9 thirty-seventh Forks entry).

**Dependencies:** QA-VoicePool closed (`d44397a`); QA-RustyMeter closed (`77cb712`). Clear to start.

**Risk:** medium. No audio-path arithmetic changes, but the gate touches the hot path in 4 engines and the reattach touches 13 files. Worst case is a missed reattach site → an engine stale after a load; the pointer-compare self-heal is the safety net, and the per-engine verify ladder catches it immediately.

**Effort estimate:** ~5–7 hours (mechanical pattern x4 + 17 call-site reroutes + verify per engine in Debug + Release). Up modestly from the original §5 ~4–6 hr estimate because of the reattach footprint.

**Bucket:** Cross-cutting Infrastructure, Players.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| L1 | **Flag wiring = Option B (reuse `ApvtsDirtyTracker`), not a parallel per-processor listener.** Add `std::atomic<bool> mDirty{true}` + lock-free `hasChangedSinceLastBlock()` to the existing tracker. | Single centralized watcher = max CPU efficiency; avoids duplicating atomic reads across the UI/audio thread boundary and a second listener on the same tree. (Jeff, 2026-05-30.) |
| L2 | **Reattach-on-load via a centralized `replaceApvtsState()` choke-point** on each of the 4 processors (`apvts.replaceState(t); mDirtyTracker.resync();`). Tracker gains `resync()` (re-point listener to live `apvts.state` + set dirty) and holds the APVTS reference. | `replaceState` orphans the tracker (verified in JUCE source). One choke-point replaces 17 scattered annotations with deterministic, future-proof state-mutation. Explicit lifecycle management over implicit magic. (Jeff, 2026-05-30.) |
| L3 | **Pointer-compare self-heal backstop** in `hasChangedSinceLastBlock()`: `if (mState != mApvts.state) return true;`. | RT-safe (pointer compare, no allocation); guards any *future* `replaceState` site that forgets to route through `replaceApvtsState()`. Belt-and-suspenders to L2. (Jeff, 2026-05-30.) |
| L4 | **Tempo handling = Option 1**: in `BaySickSynth` + `BaySickBass`, OR a scalar `mHostBPM != mLastSyncedBpm` check into the gate. | Those two engines compute tempo-synced LFO rate inside `updateFromApvts`; an APVTS-only gate would drop tempo tracking. Scalar compare is near-free and preserves real-time automation tracking without structural fragmentation. VibePlayer + Harmless don't consume per-block tempo, so they gate cleanly. (Jeff, 2026-05-30.) |
| L5 | **One consolidated source commit** across all four engines + the shared header. | We touch a shared header (`ApvtsDirtyTracker.h`); an atomic commit prevents a fractured Git history. (Jeff, 2026-05-30.) |
| L6 | **`BaySickSynthVoice::startNote` osc-reset fold-in** rides in the same single commit (`mOsc.reset(); mOsc2.reset();` after the inline phase-accumulator resets). | §9 thirty-seventh Forks entry; same `BaySickSynth` surface; per-note reset (cannot go in per-block `updateFromApvts`). (Jeff, locked at QA-VoicePool close.) |
| L7 | **Scope = the 4 legacy engines only.** The 6 sfizz-family engines (Guitars/Basses/RustyDrums/NAMIR/Pedals/Vocal) also use `ApvtsDirtyTracker` and share the same latent orphaning, but get **no** perf gate here. The shared-header change is backward-compatible for them (new members unused). `StandaloneEditor.cpp:10262` is a `BaySickRustyDrums` swap → out of scope. | §5 scope is the 4 legacy engines. Sfizz orphaning only affects their project-dirty marker (no gate), pre-existing + masked → close-time routing candidate, not in-batch. |
| L8 | Plan-file silly-name = `shimmering-noodling-simon` (assigned by plan-mode runtime; running-notes file matches). | Locked at plan-mode entry. |

---

## Sub-spec calls surfaced for ExitPlanMode

**No sub-spec calls open.** Every architectural decision (flag wiring, reattach mechanism, self-heal, tempo handling, commit granularity, fold-in, scope) was surfaced in chat and Jeff-locked across four rounds before this plan body was written (per Main Plan §0 Rule 5). Implementation details visible in the Task code blocks below (dedicated `mLastSyncedBpm` member; `replaceApvtsState()` inline in each header) are shown for review at ExitPlanMode.

---

## Files to modify

All source changes land in the single Task 1 commit.

### Shared dirty-tracker
- [Source/Standalone/ApvtsDirtyTracker.h](Source/Standalone/ApvtsDirtyTracker.h) — add `#include <atomic>`; store `juce::AudioProcessorValueTreeState& mApvts` + `std::atomic<bool> mDirty{true}`; add `hasChangedSinceLastBlock()` (with self-heal) + `resync()`; set `mDirty` in the existing `valueTreePropertyChanged` (`:39`). Ctor init order: `mApvts (apvts), mState (apvts.state)`.

### The 4 engine processors (gate + `replaceApvtsState()` + setStateInformation reroute; +`mLastSyncedBpm` for Synth/Bass)
- [Source/Harmless/HarmlessProcessor.h](Source/Harmless/HarmlessProcessor.h) — add inline `replaceApvtsState()` (near the get/setStateInformation overrides). `.cpp`: gate at processBlock `:76`; reroute `apvts.replaceState` → `replaceApvtsState` at `:1067`.
- [Source/VibePlayer/VibePlayerProcessor.h](Source/VibePlayer/VibePlayerProcessor.h) — add inline `replaceApvtsState()`. `.cpp`: gate at `:38`; reroute at `:390`.
- [Source/BaySickSynth/BaySickSynthProcessor.h](Source/BaySickSynth/BaySickSynthProcessor.h) — add inline `replaceApvtsState()`; add `float mLastSyncedBpm { -1.0f };` next to `mHostBPM` (`:85`). `.cpp`: tempo-aware gate at `:44`; reroute at `:553`.
- [Source/BaySickBass/BaySickBassProcessor.h](Source/BaySickBass/BaySickBassProcessor.h) — add inline `replaceApvtsState()`; add `float mLastSyncedBpm { -1.0f };` next to `mHostBPM` (`:83`). `.cpp`: tempo-aware gate at `:44`; reroute at `:534`.

### Reroute the remaining 13 external `replaceState` call sites → `replaceApvtsState()`
Pattern: `X.apvts.replaceState(Y)` → `X.replaceApvtsState(Y)`.
- Editors (in-editor patch picker / reset): [HarmlessEditor.cpp:1271](Source/Harmless/HarmlessEditor.cpp:1271) + [:1292](Source/Harmless/HarmlessEditor.cpp:1292); [VibePlayerEditor.cpp:779](Source/VibePlayer/VibePlayerEditor.cpp:779); [BaySickSynthEditor.cpp:1106](Source/BaySickSynth/BaySickSynthEditor.cpp:1106); [BaySickBassEditor.cpp:1069](Source/BaySickBass/BaySickBassEditor.cpp:1069).
- Pages (prefix-substitute clone/restore path): [LayersPage.cpp:375-377](Source/Standalone/LayersPage.cpp:375) (Synth/VibePlayer/Harmless); [BassPage.cpp:362-364](Source/Standalone/BassPage.cpp:362) (Bass/VibePlayer/Harmless); [DrumPage.cpp:769](Source/Standalone/DrumPage.cpp:769) (`bss`=Synth) + [:963](Source/Standalone/DrumPage.cpp:963) (`vp`=VibePlayer).

### Synth-voice phase-reset fold-in
- [Source/BaySickSynth/BaySickSynthVoice.cpp](Source/BaySickSynth/BaySickSynthVoice.cpp) — insert `mOsc.reset(); mOsc2.reset();` after the inline phase-accumulator resets (`:77`, before `mFilter.reset()`). Members `mOsc`/`mOsc2` at [BaySickSynthVoice.h:122-123](Source/BaySickSynth/BaySickSynthVoice.h:122); public `reset()` at [WavetableOscillator.h:20](Source/WavetableOscillator.h:20).

**NOT touched:** `StandaloneEditor.cpp` (its only `apvts.replaceState` is `BaySickRustyDrums`, out of scope per L7); the 6 sfizz-family processors.

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/shimmering-noodling-simon.md` → `Plans & Specs/Batch Plans/shimmering-noodling-simon.md` (Write tool); delete the home-dir copy.
- [ ] Update the Main Plan §5 QA-EngineApvts entry `**Plan file:**` line to `` `Plans & Specs/Batch Plans/shimmering-noodling-simon.md` `` (backticked-path form).
- [ ] Seed `Plans & Specs/Running Notes/shimmering-noodling-simon.md` per §0 required sections (title / purpose blockquote / pair ref + convention ref / initial "Task 0: open" entry).
- [ ] Surface full git status. Dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval. Commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 1 — Implementation (single consolidated source commit)

**1.1 — Extend `ApvtsDirtyTracker`** ([ApvtsDirtyTracker.h](Source/Standalone/ApvtsDirtyTracker.h))
- [ ] Add `#include <atomic>` after `#include <functional>`.
- [ ] Store the APVTS reference + a dirty flag; add the gate + resync:
```cpp
explicit ApvtsDirtyTracker (juce::AudioProcessorValueTreeState& apvts)
    : mApvts (apvts), mState (apvts.state)        // mApvts declared before mState
{
    mState.addListener (this);
}

// QA-EngineApvts: lock-free per-block gate (audio thread).  Returns true and clears
// if any param changed since the last call.  Self-heal backstop: if the apvts swapped
// its state tree (replaceState) our listener is orphaned -- detect via a cheap pointer
// compare and force a sync until resync() re-subscribes on the message thread.
bool hasChangedSinceLastBlock() noexcept
{
    if (mState != mApvts.state)                   // orphaned by a state swap
        return true;
    return mDirty.exchange (false, std::memory_order_acquire);
}

// QA-EngineApvts: message-thread re-subscribe after a destructive state swap.
// Called by <Engine>Processor::replaceApvtsState() right after replaceState().
void resync()
{
    mState.removeListener (this);
    mState = mApvts.state;
    mState.addListener (this);
    mDirty.store (true, std::memory_order_release);
}
```
- [ ] In `valueTreePropertyChanged` (`:39`) add `mDirty.store (true, std::memory_order_release);` before the existing `if (onAny) onAny();`.
- [ ] Add members: `juce::AudioProcessorValueTreeState& mApvts;` (before `mState`) and `std::atomic<bool> mDirty { true };`.

**1.2 — Add `replaceApvtsState()` to the 4 processors** (inline in each `.h`, near the get/setStateInformation overrides)
- [ ] Identical method in `HarmlessProcessor.h`, `VibePlayerProcessor.h`, `BaySickSynthProcessor.h`, `BaySickBassProcessor.h`:
```cpp
// QA-EngineApvts: single choke-point for destructive APVTS state swaps (preset /
// project / clone load).  Routes every swap through here so the dirty-tracker
// re-subscribes -- replaceState orphans the prior listener (ApvtsDirtyTracker::resync).
void replaceApvtsState (const juce::ValueTree& newState)
{
    apvts.replaceState (newState);
    mDirtyTracker.resync();
}
```

**1.3 — Reroute the 17 `replaceState` call sites** → `replaceApvtsState()`
- [ ] In-processor (`setStateInformation`): `apvts.replaceState(...)` → `replaceApvtsState(...)` at Harmless `.cpp:1067`, VibePlayer `.cpp:390`, Synth `.cpp:553`, Bass `.cpp:534`.
- [ ] Editors: `mProc.apvts.replaceState(...)` → `mProc.replaceApvtsState(...)` at HarmlessEditor `:1271` + `:1292`, VibePlayerEditor `:779`, SynthEditor `:1106`, BassEditor `:1069`.
- [ ] Pages: `<cast>->apvts.replaceState(...)` → `<cast>->replaceApvtsState(...)` at LayersPage `:375-377`, BassPage `:362-364`, DrumPage `:769` + `:963`.
- [ ] Read each site before editing to confirm the variable type carries the method (all are concrete target-engine pointers per pre-batch grep).

**1.4 — Add the dirty-gate to the 4 `processBlock`s**
- [ ] Harmless `.cpp:76` and VibePlayer `.cpp:38` — replace the unconditional `updateFromApvts();`:
```cpp
if (mDirtyTracker.hasChangedSinceLastBlock())
    updateFromApvts();
```
- [ ] BaySickSynth `.cpp:44` and BaySickBass `.cpp:44` — tempo-aware gate (`mHostBPM` already read just above, `:38-42`):
```cpp
const bool paramsChanged = mDirtyTracker.hasChangedSinceLastBlock();
const bool tempoChanged  = (mHostBPM != mLastSyncedBpm);   // synced-LFO tracks tempo
if (paramsChanged || tempoChanged)
{
    mLastSyncedBpm = mHostBPM;
    updateFromApvts();
}
```
- [ ] Add `float mLastSyncedBpm { -1.0f };` to Synth.h (`~:85`) + Bass.h (`~:83`), beside `mHostBPM`.

**1.5 — Osc-reset fold-in** ([BaySickSynthVoice.cpp:77](Source/BaySickSynth/BaySickSynthVoice.cpp:77))
- [ ] After the inline phase-accumulator resets (`mDeafSawState = 0.0f;` at `:77`, before `mFilter.reset()` at `:79`):
```cpp
mOsc.reset();    // QA-EngineApvts fold-in: wavetable phase zero at every note-on
mOsc2.reset();   // (SAW / SAW+SAW / SAW+SQUARE / SQUARE+SQUARE / SUPERSAW consistency)
```

**1.6 — Build + verify + commit**
- [ ] Tell Jeff: "Run `do_build.bat`. Verify in Debug first, then Release:
  - **(1) Knob takes effect (gate fires on edit).** On a Layer set to each engine in turn (Harmless, BaySickPlayer, BaySickSynth, BaySickBass), turn a knob (e.g. filter cutoff) during a held/looped note — the change is audible immediately.
  - **(2) Preset reattach (the key regression test).** For each engine: load a different patch via the engine editor's patch picker → the new patch's sound applies right away; THEN turn a knob → it still responds. (Confirms `resync()` re-subscribed after the swap.)
  - **(3) Project-load reattach.** Open a saved project using these engines → each sounds correct (loaded params applied) AND knobs still respond after load.
  - **(4) Tempo-sync (BaySickSynth + BaySickBass).** On a patch with LFO sync ON, start playback, then change the project tempo → the synced LFO speed tracks the new tempo without touching a knob.
  - **(5) Idle CPU (the win).** Play a busy multi-track session with several of these engines and DON'T move any control → CPU should sit at or below the pre-change baseline.
  - **(6) MT vs serial.** Repeat (1)–(5) in MT (production default) and in 1-worker serial-diagnostic mode → identical behavior (MT works in Debug per QA-Md).
  - **(7) Phase-reset.** On a BaySickSynth SAW patch, play the same chord 4× in a row → every hit sounds identical."
- [ ] Wait for Jeff's Debug + Release verify result.
- [ ] If a verify fails and needs a temp `DBG`/`AlertWindow` trace: add a `## Diagnostic Instrumentation Catalog` row in the running notes IN THE SAME EDIT PASS (Site / Tag / Purpose / Disposition) per §0 Rule 4; strip `Remove` rows before commit (surface strip list to Jeff first).
- [ ] On pass: surface full git status. Dispatch `/draft-commit`. Surface drafted message + git status. Commit on approval — **single consolidated commit** (long message → `git commit -F .git/COMMIT_EDITMSG_QA-EngineApvts-1.txt`, then `rm`).
- [ ] `/draft-doc running-notes` → apply.

### Task 2 — Close sequence
- [ ] `/draft-doc batch-close` — compile the Implemented Work Log entry from running notes.
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit.
- [ ] `/review-batch QA-EngineApvts` — audit diff vs plan + CLAUDE.md rules + memory gotchas.
- [ ] Address BLOCKERs / NEEDS-FIX in-batch; defer NITs into the close entry.
- [ ] Route side findings per §0 Rule 3 (see Routing notes below). Surface placement options to Jeff; don't pick the slot.
- [ ] Surface full git status. `/draft-commit` for the close commit. Surface message + status. Commit on approval (separate doc commit — clean rollback boundary).

---

## MT-awareness static-analysis

The gate is a single lock-free atomic exchange (`mDirty.exchange(false, acquire)`), written by `valueTreePropertyChanged`/`resync` on the message thread (`store(true, release)`) and read-and-cleared by `processBlock` on the audio thread — the same release/acquire handoff used by the proven `PluginProcessor` reference (`mEQsDirty.exchange`, `PluginProcessor.cpp:960`). No lock, no allocation on the audio thread.

The self-heal `mState != mApvts.state` is a pointer comparison (`ValueTree::operator==` compares the shared-object pointer). On the Windows x86-64 target, aligned pointer reads are atomic, so the worst case is the audio thread reading the pre- or post-swap pointer for a single block — both valid, because the tracker's `mState` pins the old shared object (refcount ≥ 1) until `resync()` runs on the message thread; `replaceApvtsState` calls `replaceState` then `resync()` sequentially, so the orphaned window is at most one block and is handled correctly (returns `true` → `updateFromApvts` runs → reads live params). No use-after-free, no torn read on target. MT (production) and 1-worker serial-diagnostic mode must show identical behavior (verify step 6).

---

## Verification (end-to-end smoke)

After the Task 1 commit lands:
1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **Gate fires on edit.** Every APVTS-bound control on each of the 4 engines takes effect on the next block.
3. **Reattach after preset load.** Load a patch → applies immediately + knobs still respond (per engine).
4. **Reattach after project load.** Saved project restores correct sound + responsive knobs.
5. **Tempo tracking.** Synth/Bass synced LFO follows a live tempo change with no knob touch.
6. **Idle CPU at/below baseline** on a busy multi-engine session with no control movement.
7. **MT == serial.** Identical behavior in both dispatcher modes.
8. **Phase-reset.** Repeated SAW chord hits sound identical.

---

## Routing notes (Rule 3 application during execution)

- **Sfizz-family latent orphaning (anticipated finding).** The 6 sfizz engines share `ApvtsDirtyTracker` and the same `replaceState`-orphaning, affecting only their project-dirty `*` marker (no perf gate here). If confirmed during execution, route at close as a §9 Forks candidate (give them the same `replaceApvtsState()` + gate in a future perf batch, OR a project-dirty-marker fix) — surface slot/placement options to Jeff; do not fold into this batch (L7 scope lock).
- **Other engine-processor perf gaps** surfaced while in these files → log in the close routing table; fold into a not-yet-started batch's §5 surface if matched, else §9 + new row (Jeff picks slot).
- **Findings that touch a completed batch's surface** → annotate that batch's §5 entry with a one-line pointer; details in a §9 Forks entry. No new §5 row.

---

## Carry-Forward Reference touch points

- **Before Task 1.1 (tracker):** [Carry-Forward Reference.md:405](Plans & Specs/Carry-Forward Reference.md:405) (APVTS-synced dirty-flag pattern) + `:406` (audio-thread fast-path bypass via single atomic load) + `:411` (CPU-safeguarding standing rule). The reference impl itself lives in source: `PluginProcessor.cpp:178`/`:198`/`:960` + `PluginProcessor.h:1063`.
- **Before Task 1.4 (gate):** Carry-Forward `:410` (engine audition pattern — the `mAuditionNote.exchange(-1)` processBlock opening the gate sits beside).
- **Before Task 1.5 (fold-in):** §9 thirty-seventh Forks entry (Main Plan) for the locked osc-reset scope + repro.
