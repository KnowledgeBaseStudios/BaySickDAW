# QA-EngineApvts — Engine processors APVTS dirty-flag compliance (perf-audit M2) — Plan (shimmering-noodling-simon)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/shimmering-noodling-simon.md`
> Paired running notes: `Plans & Specs/Running Notes/shimmering-noodling-simon.md`

> **For execution:** steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule). All source changes land in ONE consolidated commit (Task 1) per the Jeff-locked commit-granularity decision.

## Context

QA-EngineApvts is the tail of the perf-audit cluster (origin: `/perf-audit` M2 at QA-Eg close, 2026-05-24; §9 thirty-fifth Forks entry). The 4 legacy engine processors — `BaySickSolsticeProcessor`, `VibePlayerProcessor`, `BaySickSynthProcessor`, `BaySickBassProcessor` — call `updateFromApvts()` **unconditionally every block**, each reading ~30–50 parameters via `apvts.getRawParameterValue(id)->load()` whether or not anything changed. In a real-time DAW (128-sample buffer ≈ 2.6 ms budget) this is per-instance cache pollution / L1 eviction that compounds across a many-track session toward dropouts. The fix brings these 4 engines into compliance with the documented BaySickDAW dirty-flag pattern (`feedback_apvts_dirty_flag_pattern.md`; reference impl `PluginProcessor.cpp:178`): only run `updateFromApvts()` when a parameter actually changed.

**Critical mid-planning finding (drove the architecture):** each engine *already* owns an `ApvtsDirtyTracker mDirtyTracker { apvts }` (added 2026-05-05) — a `juce::ValueTree::Listener` on the engine's `apvts.state` that fires `onAny` for the project-dirty title-bar `*` marker. Two further facts, both verified against source:

1. The tracker is the natural home for the perf flag (single centralized watcher — no parallel listener/atomic per engine).
2. **`replaceState` orphans the tracker.** Loading a preset/sound/project calls `apvts.replaceState(...)` (`juce_AudioProcessorValueTreeState.cpp:407` → `state = newState`). Per `juce::ValueTree::operator=` (`juce_ValueTree.cpp:612`), listeners live on the *wrapper*, so only APVTS's own wrapper re-points; the tracker's separate `mState` wrapper keeps pointing at the discarded tree and never fires again. With a naive gate, an engine would stop responding to knob turns after any preset/project load. `replaceState` is called from **17 sites** across the 4 engines + their editors + the Layers/Bass/Drum pages.

**Architecture (Jeff-locked 2026-05-30, pivoted to Option A mid-execution — see Running Notes "ARCHITECTURE PIVOT" entry):** make `ApvtsDirtyTracker` attach **directly** to `apvts.state` (not a copy), so JUCE's `ValueTree::operator=` migrates the listener across `replaceState` (the `valueTreeRedirected` framework contract the engine editors + pages already use). The tracker carries a lock-free `mDirty` flag set in BOTH `valueTreePropertyChanged` and `valueTreeRedirected`; the 4 legacy `processBlock`s gate `updateFromApvts()` behind `hasChangedSinceLastBlock()` (Synth/Bass also OR in a `mHostBPM != mLastSyncedBpm` scalar so synced-LFO tracks tempo). This immunizes **all 10** `ApvtsDirtyTracker` engines against the orphaning natively, with no per-call-site bookkeeping. Plus the QA-VoicePool-folded `BaySickSynthVoice::startNote` osc-reset (§9 thirty-seventh Forks entry).

> **Pivot note (2026-05-30):** an earlier locked approach (a per-processor `replaceApvtsState()` choke-point + 17 call-site reroutes + a pointer-compare self-heal) was implemented then reverted when Jeff's blast-radius question surfaced that the orphaning is specific to the tracker's *copy* of `apvts.state` — every other listener attaches directly and survives via JUCE migration. Direct-attach is the smaller, more robust, codebase-idiomatic fix and covers all 10 engines for free. L2 / L3 / L7 below are annotated with the supersession.

**Dependencies:** QA-VoicePool closed (`d44397a`); QA-RustyMeter closed (`77cb712`). Clear to start.

**Risk:** low-medium. No audio-path arithmetic changes; the only hot-path change is a single atomic exchange in 4 `processBlock`s, plus a one-line attach-point change in the shared tracker. The tracker change touches all 10 engines (it's their shared dirty-marker bridge), so the verify ladder exercises a sample across engine families. Worst case: a tracker mis-wire shows immediately on the first knob turn / preset load.

**Effort estimate:** ~3–4 hours (tracker rewrite + 4 `processBlock` gates + osc-reset + verify in Debug + Release). Smaller than the original §5 ~4–6 hr estimate now that Option A (direct-attach) replaced the 17-reroute choke-point approach.

**Bucket:** Cross-cutting Infrastructure, Players.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| L1 | **Flag wiring = Option B (reuse `ApvtsDirtyTracker`), not a parallel per-processor listener.** Add `std::atomic<bool> mDirty{true}` + lock-free `hasChangedSinceLastBlock()` to the existing tracker. | Single centralized watcher = max CPU efficiency; avoids duplicating atomic reads across the UI/audio thread boundary and a second listener on the same tree. (Jeff, 2026-05-30.) |
| L2 | **Reattach-on-load via direct attachment (Option A — pivoted 2026-05-30).** `ApvtsDirtyTracker` attaches to `apvts.state` directly (`mApvts.state.addListener(this)`), not to a copy. JUCE's `operator=` migrates the listener across `replaceState` and fires `valueTreeRedirected`, where the tracker re-arms `mDirty`. *(Supersedes the original `replaceApvtsState()` choke-point + 17 reroutes — implemented then reverted.)* | The orphaning was specific to the tracker's `mState` *copy*; every other state listener attaches directly and survives. Direct-attach is smaller, codebase-idiomatic (editors/pages already do it — e.g. `LayersPage.cpp:935`), and immunizes all 10 engines with no per-site bookkeeping. (Jeff, 2026-05-30.) |
| L3 | **No self-heal needed (pivoted 2026-05-30).** Direct attachment + `valueTreeRedirected` is JUCE's explicit migration contract, so a missed-site backstop is unnecessary — the tracker can never be orphaned. *(Supersedes the pointer-compare self-heal — reverted.)* | Removing the self-heal also removes a benign-but-real cross-thread read of `apvts.state` on the audio thread. `hasChangedSinceLastBlock()` is now a plain `exchange`. (Jeff, 2026-05-30.) |
| L4 | **Tempo handling = Option 1**: in `BaySickSynth` + `BaySickBass`, OR a scalar `mHostBPM != mLastSyncedBpm` check into the gate. | Those two engines compute tempo-synced LFO rate inside `updateFromApvts`; an APVTS-only gate would drop tempo tracking. Scalar compare is near-free and preserves real-time automation tracking without structural fragmentation. VibePlayer + BaySickSolstice don't consume per-block tempo, so they gate cleanly. (Jeff, 2026-05-30.) |
| L5 | **One consolidated source commit** across all four engines + the shared header. | We touch a shared header (`ApvtsDirtyTracker.h`); an atomic commit prevents a fractured Git history. (Jeff, 2026-05-30.) |
| L6 | **`BaySickSynthVoice::startNote` osc-reset fold-in** rides in the same single commit (`mOsc.reset(); mOsc2.reset();` after the inline phase-accumulator resets). | §9 thirty-seventh Forks entry; same `BaySickSynth` surface; per-note reset (cannot go in per-block `updateFromApvts`). (Jeff, locked at QA-VoicePool close.) |
| L7 | **Scope = all 10 `ApvtsDirtyTracker` engines (extended 2026-05-30).** The shared-tracker direct-attach fix hardens all 10: 4 legacy (BaySickSolstice/VibePlayer/Synth/Bass) + 3 sfizz (Guitars/Basses/RustyDrums) + 3 other (NAMIR/Pedals/Vocal). Only the 4 legacy engines additionally get the `processBlock` perf gate (the perf-audit M2 target); the other 6 gain only the project-dirty-marker hardening (no gate). `EffectRack` does **not** use the tracker → effect-rack presets unaffected. | Jeff extended scope so the project-dirty orphaning is fixed everywhere, not deferred — achieved for free by the shared-tracker change (no per-engine work). Corrects an earlier mislabeling of Pedals/NAMIR/Vocal as "sfizz" (only 3 engines are sfizz). (Jeff, 2026-05-30.) |
| L8 | Plan-file silly-name = `shimmering-noodling-simon` (assigned by plan-mode runtime; running-notes file matches). | Locked at plan-mode entry. |

---

## Sub-spec calls surfaced for ExitPlanMode

**No sub-spec calls open.** Every architectural decision (flag wiring, reattach mechanism, tempo handling, commit granularity, fold-in, scope) was surfaced in chat and Jeff-locked (per Main Plan §0 Rule 5). The 2026-05-30 mid-execution pivot to Option A (direct-attach) and the all-10-engine scope extension were both Jeff-directed in chat — see the Running Notes "ARCHITECTURE PIVOT" entry.

---

## Files to modify

All source changes land in the single Task 1 commit.

### Shared dirty-tracker (covers all 10 engines)
- [Source/Standalone/ApvtsDirtyTracker.h](Source/Standalone/ApvtsDirtyTracker.h) — attach DIRECTLY to `apvts.state` (ctor `mApvts.state.addListener(this)` / dtor `mApvts.state.removeListener(this)`; the `mState` copy is removed). Add `#include <atomic>`, `juce::AudioProcessorValueTreeState& mApvts`, `std::atomic<bool> mDirty{true}`. Set `mDirty` in BOTH `valueTreePropertyChanged` (+ keep the existing `onAny`) AND a new `valueTreeRedirected` override (re-arm on a load swap; `onAny` NOT fired there). `hasChangedSinceLastBlock()` = plain `mDirty.exchange(false, acquire)`.

### The 4 legacy processors — perf gate only (+ `mLastSyncedBpm` for Synth/Bass)
- [BaySickSolsticeProcessor.cpp:76](Source/BaySickSolstice/BaySickSolsticeProcessor.cpp:76) + [VibePlayerProcessor.cpp:38](Source/VibePlayer/VibePlayerProcessor.cpp:38) — gate `updateFromApvts()` behind `if (mDirtyTracker.hasChangedSinceLastBlock())`.
- [BaySickSynthProcessor.cpp:44](Source/BaySickSynth/BaySickSynthProcessor.cpp:44) + [BaySickBassProcessor.cpp:44](Source/BaySickBass/BaySickBassProcessor.cpp:44) — tempo-aware gate (`paramsChanged || mHostBPM != mLastSyncedBpm`); add `float mLastSyncedBpm { -1.0f };` beside `mHostBPM` in each header ([BaySickSynthProcessor.h:86](Source/BaySickSynth/BaySickSynthProcessor.h:86) / [BaySickBassProcessor.h:84](Source/BaySickBass/BaySickBassProcessor.h:84)).

### Synth-voice phase-reset fold-in
- [BaySickSynthVoice.cpp:78-79](Source/BaySickSynth/BaySickSynthVoice.cpp:78) — `mOsc.reset(); mOsc2.reset();` after the inline phase-accumulator resets, before `mFilter.reset()`. Public `reset()` at [WavetableOscillator.h:20](Source/WavetableOscillator.h:20).

**NOT touched (Option A makes them unnecessary):** no `replaceApvtsState()` methods, no `replaceState` call-site reroutes anywhere (the direct-attached tracker migrates natively); `EffectRack` (doesn't use the tracker); the per-engine editors/pages (their direct `apvts.state` listeners already migrate). The 6 non-legacy engines need **no** source change — they inherit the fix through the shared `ApvtsDirtyTracker`.

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/shimmering-noodling-simon.md` → `Plans & Specs/Batch Plans/shimmering-noodling-simon.md` (Write tool); delete the home-dir copy.
- [ ] Update the Main Plan §5 QA-EngineApvts entry `**Plan file:**` line to `` `Plans & Specs/Batch Plans/shimmering-noodling-simon.md` `` (backticked-path form).
- [ ] Seed `Plans & Specs/Running Notes/shimmering-noodling-simon.md` per §0 required sections (title / purpose blockquote / pair ref + convention ref / initial "Task 0: open" entry).
- [ ] Surface full git status. Dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval. Commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 1 — Implementation (single consolidated source commit)

**1.1 — Rewrite `ApvtsDirtyTracker` to direct-attach** ([ApvtsDirtyTracker.h](Source/Standalone/ApvtsDirtyTracker.h)) — *[done]*
- [x] Attach directly to `apvts.state` (no copy); `mDirty` set in `valueTreePropertyChanged` + `valueTreeRedirected`; `hasChangedSinceLastBlock()` = plain exchange:
```cpp
explicit ApvtsDirtyTracker (juce::AudioProcessorValueTreeState& apvts) : mApvts (apvts)
{
    mApvts.state.addListener (this);
}
~ApvtsDirtyTracker() override { mApvts.state.removeListener (this); }

bool hasChangedSinceLastBlock() noexcept { return mDirty.exchange (false, std::memory_order_acquire); }

private:
void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override
{ mDirty.store (true, std::memory_order_release); if (onAny) onAny(); }
void valueTreeRedirected (juce::ValueTree&) override   // load swap: JUCE migrates us here
{ mDirty.store (true, std::memory_order_release); }     // (onAny deliberately not fired)

juce::AudioProcessorValueTreeState& mApvts;
std::atomic<bool> mDirty { true };
```

**1.2 / 1.3 — (removed in the Option A pivot, 2026-05-30.)** The `replaceApvtsState()` choke-point + the 17 `replaceState` reroutes are unnecessary with the direct-attach tracker (1.1 migrates natively via `valueTreeRedirected`). The 6 non-legacy engines need no source change — they inherit the fix through the shared tracker. *(These tasks were implemented then reverted; see the Running Notes "ARCHITECTURE PIVOT" entry.)*

**1.4 — Add the dirty-gate to the 4 `processBlock`s**
- [ ] BaySickSolstice `.cpp:76` and VibePlayer `.cpp:38` — replace the unconditional `updateFromApvts();`:
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
  - **(1) Knob takes effect (gate fires on edit).** On a Layer set to each engine in turn (BaySickSolstice, BaySickPlayer, BaySickSynth, BaySickBass), turn a knob (e.g. filter cutoff) during a held/looped note — the change is audible immediately.
  - **(2) Reattach after a sound/patch load (the key regression test).** For each legacy engine: load a different patch via the engine editor's picker → the new sound applies right away; THEN turn a knob → it still responds. (Confirms the directly-attached tracker migrated with the swap.)
  - **(3) Project-load reattach.** Open a saved project using these engines → each sounds correct (loaded params applied) AND knobs still respond after load.
  - **(4) Tempo-sync (BaySickSynth + BaySickBass).** On a patch with LFO sync ON, start playback, then change the project tempo → the synced LFO speed tracks the new tempo without touching a knob.
  - **(5) Idle CPU (the win).** Play a busy multi-track session with several of these engines and DON'T move any control → CPU should sit at or below the pre-change baseline.
  - **(6) MT vs serial.** Repeat (1)–(5) in MT (production default) and in 1-worker serial-diagnostic mode → identical behavior (MT works in Debug per QA-Md).
  - **(7) Phase-reset.** On a BaySickSynth SAW patch, play the same chord 4× in a row → every hit sounds identical.
  - **(8) All-10 hardening.** On a non-legacy engine tab (e.g. BaySickGuitars / Pedals / Vocal): load a preset, then tweak a control → the title-bar `*` appears (the shared tracker survived the swap on those engines too)."
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

The gate is a single lock-free atomic exchange (`mDirty.exchange(false, acquire)` on the audio thread), set with `store(true, release)` on the message thread (`valueTreePropertyChanged` / `valueTreeRedirected`) — the same release/acquire handoff used by the proven `PluginProcessor` reference (`mEQsDirty.exchange`, `PluginProcessor.cpp:960`). No lock, no allocation on the audio thread.

Direct attachment (Option A) means the audio thread touches **only** `mDirty` — there is no cross-thread read of `apvts.state` (the earlier self-heal's pointer-compare is gone). The listener re-points on `replaceState` entirely on the message thread via JUCE's `ValueTree::operator=` migration + `valueTreeRedirected`, so there is no audio/message race on the tree pointer at all. MT (production) and 1-worker serial-diagnostic mode must show identical behavior (verify step 6).

---

## Verification (end-to-end smoke)

After the Task 1 commit lands:
1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **Gate fires on edit.** Every APVTS-bound control on each of the 4 legacy engines takes effect on the next block.
3. **Reattach after sound/patch load.** Load a patch → applies immediately + knobs still respond (each legacy engine).
4. **Reattach after project load.** Saved project restores correct sound + responsive knobs.
5. **Tempo tracking.** Synth/Bass synced LFO follows a live tempo change with no knob touch.
6. **Idle CPU at/below baseline** on a busy multi-engine session with no control movement.
7. **MT == serial.** Identical behavior in both dispatcher modes.
8. **Phase-reset.** Repeated SAW chord hits sound identical.
9. **All-10 hardening.** A non-legacy engine (sfizz / NAMIR / Pedals / Vocal) marks the project `*` on a post-preset-load edit (the shared tracker survived the swap).

---

## Routing notes (Rule 3 application during execution)

- **Non-legacy engine project-dirty orphaning — RESOLVED in-batch (2026-05-30).** Originally an L7 deferral for the 6 non-legacy `ApvtsDirtyTracker` engines (3 sfizz: Guitars/Basses/RustyDrums + 3 other: NAMIR/Pedals/Vocal). Jeff extended scope so the shared-tracker direct-attach fix (1.1) hardens all 10 — no per-engine source change, no §9 route needed. Recorded in the close entry's "what was done" notes.
- **Other engine-processor perf gaps** surfaced while in these files → log in the close routing table; fold into a not-yet-started batch's §5 surface if matched, else §9 + new row (Jeff picks slot).
- **Findings that touch a completed batch's surface** → annotate that batch's §5 entry with a one-line pointer; details in a §9 Forks entry. No new §5 row.

---

## Carry-Forward Reference touch points

- **Before Task 1.1 (tracker):** [Carry-Forward Reference.md:405](Plans & Specs/Carry-Forward Reference.md:405) (APVTS-synced dirty-flag pattern) + `:406` (audio-thread fast-path bypass via single atomic load) + `:411` (CPU-safeguarding standing rule). The reference impl itself lives in source: `PluginProcessor.cpp:178`/`:198`/`:960` + `PluginProcessor.h:1063`.
- **Before Task 1.4 (gate):** Carry-Forward `:410` (engine audition pattern — the `mAuditionNote.exchange(-1)` processBlock opening the gate sits beside).
- **Before Task 1.5 (fold-in):** §9 thirty-seventh Forks entry (Main Plan) for the locked osc-reset scope + repro.
