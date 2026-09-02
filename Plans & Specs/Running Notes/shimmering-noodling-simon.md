# Running Notes — QA-EngineApvts (shimmering-noodling-simon)

> **Purpose:** Append-only running log for the QA-EngineApvts batch. A new entry is appended at every checkpoint — commit landed / sub-task verified / finding captured / spec call resolved / scope pivot — per `feedback_draft_doc_running_notes_every_checkpoint.md` and the Main Plan §0 running-notes rule. At batch close, `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Never edit prior entries; surprises get their own new entry.

> **Pair file:** `Plans & Specs/Batch Plans/shimmering-noodling-simon.md` (the batch plan).
> **Conventions:** Main Plan §0 "Document Formatting Conventions" + "Batch Plans + Running Notes layout (locked 2026-05-11)".

## Diagnostic Instrumentation Catalog

Per Main Plan §0 Rule 4. None added yet — this is a mechanical refactor; verification is via Jeff's audio/CPU testing, not log traces. Any temp `DBG` / `juce::Logger` / `AlertWindow` / temp `jassert` added during a verify-fail investigation gets a row here IN THE SAME EDIT PASS, and every `Remove` row is stripped (with the strip list surfaced to Jeff) before the relevant commit.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| `PluginProcessor.h` (`InstSlotDiag` struct + `mInstDiag` public array, after `mRustyIdleBlocks`) | `QA-EngineApvts DIAG` | per-Inst-slot gate-snapshot storage (atomics) | Remove at batch close |
| `InstStripTask.cpp` (3 capture blocks: FilePlay-branch top, post-`sfizzActive`, idle-gate inputs + reason) | `QA-EngineApvts DIAG` | audio-thread capture of which gate path the slot takes + inputs | Remove at batch close |
| `StandaloneEditor.h` (`flushInstDiag()` decl + `InstDiagTimer mInstDiagTimer` + `mInstDiagLastKey[64]`) | `QA-EngineApvts DIAG` | 30 Hz flush timer + per-slot transition-dedup state | Remove at batch close |
| `StandaloneEditor.cpp` (`mInstDiagTimer.startTimerHz(30)` ctor, `.stopTimer()` dtor, `flushInstDiag()` impl writing `Documents/BaySickDAW/InstDiag.txt`) | `QA-EngineApvts DIAG` | flush gate-snapshot transitions to a file Jeff can open | Remove at batch close |

> **Diagnostic purpose:** isolate why `BaySickGuitars`/`BaySickBasses` go silent (notes + audition dead) when off the piano-roll page after the Option-A tracker change. The InstDiag.txt line at the moment audio stops shows `reason=` (RENDER / IDLE-SUSPENDED / SFIZZ-INACTIVE / FILEPLAY-RETURN) + the gate inputs (`sfizzActive` / `midi` / `voices` / `audition`), pinpointing which input flips. All sites stripped at batch close (Rule 4).

> **STRIPPED 2026-05-31** (Jeff approved the strip list): reverted via `git restore` of the 4 diag-only files (`PluginProcessor.h` / `InstStripTask.cpp` / `StandaloneEditor.h` / `StandaloneEditor.cpp` — none were in the Option-A change set, so a full restore was the zero-risk strip). `git grep "QA-EngineApvts DIAG" -- Source/` returns 0. The runtime `Documents/BaySickDAW/InstDiag.txt` is untracked and never committed.

## 2026-05-30 — Task 0 — open

- Batch opened. Pre-batch ritual: `/standup`; full self-read of Main Plan §0 (Rules 1–5 + formatting conventions + buckets + orchestration rules); `/read-doc` bulk extractions (§5 QA-EngineApvts entry + §6 slot + §9 thirty-fifth / thirty-seventh / forty-fifth Forks; Carry-Forward primitives; Work Log recent closes). CLAUDE.md cross-check: "Next batch: QA-Md" + "Current position (2026-05-08)" confirmed **stale** vs Work Log — relying on Work Log / Main Plan for status (real next = QA-EngineApvts; QA-Md closed 2026-05-09).
- **Architecture surfaced + Jeff-locked across 4 rounds** (plan §"Spec calls already locked" L1–L8): Option B (reuse the existing `ApvtsDirtyTracker`) + centralized per-processor `replaceApvtsState()` reattach choke-point + RT-safe pointer-compare self-heal backstop + tempo-as-change for BaySickSynth/BaySickBass + ONE consolidated source commit + `BaySickSynthVoice::startNote` osc-reset fold-in. Scope = the 4 legacy engines only (L7).
- **Key mid-planning findings (all source-verified):**
  - (1) `ApvtsDirtyTracker` (2026-05-05) already exists in all engines as the project-dirty title-bar `*` marker bridge — Concept B, distinct from the perf gate (Concept A) the Work Log warned not to conflate.
  - (2) `apvts.replaceState` (`juce_AudioProcessorValueTreeState.cpp:407` → `state = newState`; `juce_ValueTree.cpp:612` `operator=`) **orphans** the tracker's separate `mState` wrapper — verified in JUCE source. Naive gate → engine stops responding to knobs after any preset/project load. Drove the reattach architecture.
  - (3) `replaceState` is called from **17 target-engine sites** (4 `setStateInformation` + 5 editors + 8 pages), not the 4 first assumed. `StandaloneEditor.cpp:10262` is a `BaySickRustyDrums` (sfizz) swap → out of scope.
  - (4) Tempo correctness: BaySickSynth/Bass `updateFromApvts` consume per-block `mHostBPM` for synced-LFO rate (Synth `.cpp:521/527/534`); VibePlayer/BaySickSolstice are APVTS-only. → tempo-as-change OR (L4).
  - (5) **Anticipated close-routing finding:** the 6 sfizz engines share the same latent tracker orphaning (project-dirty marker only; no gate here) — route at close per Rule 3 (slot is Jeff's).
- Plan written + approved (ExitPlanMode); mirrored to `Batch Plans/shimmering-noodling-simon.md`, home-dir copy deleted; §5 `**Plan file:**` pointer updated.
- Task 0 doc-open commit landed `eca72fb` (3 files, +251/-1; Co-Authored-By trailer bumped to Opus 4.8 per Jeff). Next: Task 1 implementation (single consolidated source commit).

## 2026-05-30 — Task 1 — implementation (all source edits landed, pre-build)

All source changes for the single consolidated commit are in (awaiting Jeff's Debug+Release verify before the commit):
- **1.1 `ApvtsDirtyTracker.h`** — `#include <atomic>`; new `AudioProcessorValueTreeState& mApvts` ref + `std::atomic<bool> mDirty{true}`; `hasChangedSinceLastBlock()` (read-and-clear `exchange` + pointer-compare self-heal `mState != mApvts.state`); `resync()` (remove/re-point/re-add listener to live `apvts.state` + set dirty); `mDirty.store(release)` added to the existing `valueTreePropertyChanged`. Member order set so the ctor init list `mApvts (apvts), mState (apvts.state)` is in declaration order.
- **1.2 `replaceApvtsState()`** — identical inline choke-point added to BaySickSolsticeProcessor.h / VibePlayerProcessor.h / BaySickSynthProcessor.h / BaySickBassProcessor.h (`apvts.replaceState(t); mDirtyTracker.resync();`).
- **1.3 reroute (17 sites)** — all target-engine `X.apvts.replaceState(Y)` -> `X.replaceApvtsState(Y)`: 4 setStateInformation (BaySickSolstice.cpp:1067 / VibePlayer.cpp:390 / Synth.cpp:553 / Bass.cpp:534) + 5 editors (BaySickSolsticeEditor :1271 + :1292 / VibePlayerEditor :779 / SynthEditor :1106 / BassEditor :1069) + 8 pages (LayersPage :375-377 / BassPage :362-364 / DrumPage :769 + :963).
- **1.4 gate** — BaySickSolstice.cpp:76 + VibePlayer.cpp:38 simple `if (mDirtyTracker.hasChangedSinceLastBlock()) updateFromApvts();`; Synth.cpp:44 + Bass.cpp:44 tempo-aware (`paramsChanged || (mHostBPM != mLastSyncedBpm)`, sets `mLastSyncedBpm` on run); `float mLastSyncedBpm { -1.0f }` member added to Synth.h + Bass.h.
- **1.5 osc-reset fold-in** — `mOsc.reset(); mOsc2.reset();` after the inline phase-accumulator resets in BaySickSynthVoice.cpp (after `:77`).
- **Verification grep (pre-build):** `apvts.replaceState(` now appears only in the 4 choke-point method bodies + the 6 sfizz engines + PluginProcessor (3) + StandaloneEditor:10262 (RustyDrums) — all out of scope per L7. `replaceApvtsState` totals 22 = 17 call sites + 4 method defs + 1 comment ref. Reroute confirmed complete; out-of-scope engines untouched.
- **Rule 4:** no diagnostic instrumentation added (mechanical refactor; verification is Jeff's audio/CPU testing).
- Next: Jeff runs `do_build.bat` + the 7-scenario Debug-then-Release verify ladder (plan Task 1.6). On pass: single consolidated source commit.

## 2026-05-30 — Task 1 — ARCHITECTURE PIVOT to Option A (direct-attach tracker); scope extended to all 10 engines

Jeff's question "why are the sfizz players out of scope / could this happen with pedal / effect-rack / page presets / a bunch of other situations?" triggered a source-grounded blast-radius audit that surfaced a simpler, better fix. The prior (choke-point) Task 1 entry above is superseded by this one.

- **Blast-radius finding:** the orphaning is NOT sfizz-specific — it is specific to `ApvtsDirtyTracker` being the ONLY listener attached to a *copy* of `apvts.state` (`mState (apvts.state)`). Every other state listener (all 4 engine editors, the Layers/Bass/Inst/Vox/Clips pages, `PluginProcessor`) attaches DIRECTLY to `apvts.state`, which JUCE's `ValueTree::operator=` migrates across `replaceState` (the `valueTreeRedirected` branch, `juce_ValueTree.cpp:620-630`). That is why editors keep updating after a preset load. Confirmed idiomatic: those editors/pages already override `valueTreeRedirected` (e.g. `BaySickSolsticeEditor.cpp:622`, `LayersPage.cpp:935`).
- **Correction (Jeff caught it):** I wrongly labelled `BaySickPedals` (and by extension NAMIR / Vocal) as "sfizz." Only **3** engines are sfizz (Guitars / Basses / RustyDrums). `BaySickPedals` is an internal 8-slot DSP pedal rack (EffectRack-based, Inst page); NAMIR is NAM/IR; Vocal is its own. All 10 (4 legacy + 3 sfizz + 3 other) use `ApvtsDirtyTracker`. `EffectRack` itself does NOT use it (own XML state) → effect-rack presets unaffected. Page presets apply correctly regardless (legacy via setStateInformation; sfizz via per-param `setValueNotifyingHost`).
- **Decision (Jeff):** extend to all 10, then **pivot to Option A — direct-attach** the tracker so JUCE migrates it on `replaceState` (the explicit framework state-migration contract, not the implicit per-param storm rejected in Option 2). Immunizes all 10 engines natively with zero per-call-site bookkeeping.

**Executed the pivot:**
- `ApvtsDirtyTracker.h` rewritten: direct-attach (`mApvts.state.addListener(this)` ctor / `removeListener` dtor; `mState` copy removed); `mDirty` set in BOTH `valueTreePropertyChanged` (+ `onAny`) AND `valueTreeRedirected` (load-swap re-arm, `onAny` deliberately not fired); `hasChangedSinceLastBlock()` = plain `exchange` (self-heal pointer-compare no longer needed).
- **Reverted:** the 4 `replaceApvtsState` choke-point methods + all 17 reroutes + the self-heal. Every site is back to plain `apvts.replaceState`.
- **Kept:** the 4 `processBlock` gates (BaySickSolstice.cpp:76 / VibePlayer.cpp:38 simple; Synth.cpp:46 / Bass.cpp:46 tempo-aware) + `mLastSyncedBpm` (Synth/Bass) + the `BaySickSynthVoice` osc-reset (:78-79).
- **Verification grep:** `replaceApvtsState`=0; `resync`/`mState` code gone (1 benign comment word); `hasChangedSinceLastBlock`=5 (1 def + 4 gates); `mLastSyncedBpm`=6; osc-reset present.
- **Net footprint:** `ApvtsDirtyTracker.h` + 4 `processBlock` gates + Synth/Bass headers + `BaySickSynthVoice.cpp` — far smaller than the choke-point approach, and covers all 10 engines.
- **Resolves in-batch** the sfizz/non-legacy project-dirty orphaning that was previously an L7 deferral — the shared-tracker fix hardens all 10. Plan L7 + Routing notes updated accordingly.
- Next: Jeff `do_build.bat` + Debug/Release verify (revised ladder — reattach now testable on any engine), then the single consolidated source commit.

## 2026-05-31 — Task 1 — verify outcome, diagnostic detour, + notated finding

Jeff's Debug verify surfaced a reported regression that resolved into stale-build noise + a one-off file issue. Net: **Option-A is sound.**

- **Off-page "stop" — NOT reproducible / stale-build noise.** Jeff reported BaySickGuitars/Basses parts stopping when off the piano-roll page. Built a temp diagnostic (InstStripTask gate snapshot -> `Documents/BaySickDAW/InstDiag.txt`; sites in the Rule 4 catalog above). Captured behavior is textbook-normal: `sfizzActive=1` throughout, `RENDER` when notes/voices present, `IDLE-SUSPENDED` only after a genuine 9-block silence gap (the pre-existing QA-C idle-suspend). The symptom stopped reproducing across the repeated rebuilds — consistent with stale-incremental-build inconsistency from the multiple `ApvtsDirtyTracker.h` (Option-A pivot) edits shifting engine member layout. New-file save/reload/play works correctly.
- **Verified independent of the tracker:** JUCE restores APVTS params on load via APVTS's OWN `valueTreeRedirected -> updateParameterConnectionsToChildTrees` ([juce_AudioProcessorValueTreeState.cpp:275/461](../../juce/modules/juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.cpp:461)), not through `ApvtsDirtyTracker`. The direct-attach change cannot zero saved params.
- **NOTATED FINDING (Jeff's call 2026-05-31: notate-only, NO §9 route / NO batch — pre-release, no user will hit it):** an older saved project (`Projects/Cool bass riff`) with a BaySickBasses Inst tab (keyswitch kit `01-darkblack_keysw.sfz`) loads silent — engine renders voices, no audio. Field-by-field identical to a working file (`Bass Test (2)`): same kit, `engineData="0."` (normal — the Inst chain wrapper's `getStateInformation` is empty by design, [StandaloneEditor.cpp:9614](../../Source/Standalone/StandaloneEditor.cpp:9614)), `sfizzEngineData` present + non-empty in both ([:9648](../../Source/Standalone/StandaloneEditor.cpp:9648)), identical mixer strip (`mixer_inst_0_level=0`/`mute=0`/`sendTo=8`). The only remaining difference is the saved BaySickBasses articulation/CC **content** inside `sfizzEngineData` (could not fully decode — JUCE custom base64). Domain = sfizz keyswitch/CC restore (QA-Sfizz), NOT QA-EngineApvts; most plausibly the file was saved during the build-churn's stale-build window. Recovery: re-pick the kit on the tab (re-inits CCs); riff notes intact.
- Next: strip the Rule 4 diagnostic instrumentation (strip list surfaced to Jeff), clean `do_build.bat`, then the single consolidated Option-A source commit.

## 2026-05-31 — Task 1 — VERIFIED (clean build) + 2 side findings (routing TBD by Jeff)

- **Option-A VERIFIED.** Forced full recompile (deleted `build/BaySickDAWStandalone.dir` — 316 objs — then `do_build.bat`); Release + Debug both pass; Jeff sanity checks green (fresh bass plays, knob takes effect, SAW-chord phase-reset consistent). Stale-build gremlins cleared. Fix is sound + ready to commit.
- **FND-1 (verify) — page-save-prompt-on-delete is inconsistent.** Deleting a Layers or Bass page prompts to save the page; no other page type does, but per Jeff all should (uniformity — mirror the Layers/Bass pattern). Domain = System Pages (page lifecycle / save prompt). Out of QA-EngineApvts scope; likely pre-existing (Layers/Bass have it, others never did — Option-A touches the engine project-dirty *tracker*, not page `isPatchDirty`/save-prompt). Route/address = Jeff's call.
- **FND-2 (verify) — sfizz Aria CC=64 default behaves as 0 until moved.** Some sfizz CCs (e.g. the Guitars fader on BaySickGuitars) sit at 64 (the Aria default set in QA-Sfizz Sub-E) but sound as if 0 until the control is moved; returning to 64 then differs from the "original 64", and you must go to 0 to match the original. Symptom of a CC-not-dispatched-at-init gap (param=64 but sfizz never received the CC → uses its internal 0/unset). Domain = QA-Sfizz (sfizz CC init/dispatch); **likely the same root cause as the notated "Cool bass riff loads silent" finding** (CC at 64 undispatched → silent/wrong articulation). Out of QA-EngineApvts scope (Option-A doesn't touch sfizz CC dispatch). Route = Jeff's call; may escalate the notated finding into a real route.
- **Routing decisions (Jeff, 2026-05-31)** — both predated / NOT QA-EngineApvts regressions, both routed OUT:
  - **FND-1 → folded into QA-ProjectSave** §5 scope ("address once project setup/save is known-good"). Rule 3 fold (expand the QA-ProjectSave §5 Items) + §9 Forks entry.
  - **FND-2 → NEW batch `QA-Sfizz-Followup`, slotted immediately after QA-EngineApvts** (before QA-Ed). Rule 3 new §5 row + §6 arrow update + new §6 footnote + §9 Forks entry. Scope: sfizz CC dispatch-at-init (Aria CC=64 default applied to the param but never sent to sfizz → behaves as 0 until moved). Bucket = Players.
  - The notated **"Cool bass riff loads silent"** finding shares this root (undispatched CC=64 → silent articulation) → recorded as a symptom under QA-Sfizz-Followup; no longer notate-only, covered by that batch's fix.
  - Routing edits (§5 fold + new §5 row + §6 + §9 ×2) applied during the close (Task 2), after the Task 1 source commit.

## 2026-05-31 — Task 2 — close

- **Task 1 consolidated source commit landed `b3cb0b6`** (8 files, +60/-9; verify date corrected to 2026-05-31; Co-Authored-By Opus 4.8). The verified Option-A fix is in.
- Close sequence: apply FND-1/FND-2 routing (§5 QA-ProjectSave fold + new §5 QA-Sfizz-Followup row + §6 arrow/footnote + §9 ×2), §5 QA-EngineApvts STATUS=CLOSED banner; `/draft-doc batch-close` -> Implemented Work Log entry; `/review-batch QA-EngineApvts`; then the separate close commit.

### 2026-05-31 — Task 2 — CLOSE-COMPLETE (paperwork applied; commit pending Jeff approval)

All close doc edits applied; ready for the close commit.

- **§9 is ONE combined forty-sixth Forks entry, NOT two** — superseding the earlier "§9 ×2" plan note (lines 83/88). Followed the QA-RustyMeter forty-fifth precedent: a single close entry records ALL close routings (FND-1 + FND-2) as inline bullets, rather than one §9 entry per finding. The doc-drafter flagged the same ambiguity; resolved to ×1 combined.
- **Main Plan edits applied:** §5 QA-EngineApvts `STATUS (2026-05-31 close): CLOSED` banner; §5 QA-ProjectSave FND-1 fold (page-save-prompt-on-delete uniformity Scope item); NEW §5 QA-Sfizz-Followup row (Bucket: Players; after QA-EngineApvts, before QA-F); §6 arrow gains `→ QA-Sfizz-Followup` (27-asterisk marker, between QA-EngineApvts + QA-Ed) + matching 27-asterisk footnote; §9 forty-sixth Forks entry (the combined close).
- **Implemented Work Log:** QA-EngineApvts close entry appended after the QA-RustyMeter entry (header `2026-05-31 22:54 PT`; close-commit SHA left `<TBD>` per the QA-RustyMeter precedent at line 1858).
- **Two factual fixes applied to the drafter's Work Log text** (review = factual/scope only): (1) header time 22:30 → **22:54 PT** (real `date`); (2) `ApvtsDirtyTracker.h` **+34/-5** (was the drafter's +39/-15 — impossible in a +60/-9 commit; confirmed via `git show --numstat b3cb0b6`). Note: b3cb0b6's own commit-message body carries the same +39/-15 typo — left as-is in the committed message (no amend of a Jeff-verified commit); the Work Log is the accurate ledger of record.
- **`/review-batch QA-EngineApvts`: READY-TO-COMMIT** — 0 BLOCKER / 0 NEEDS-FIX; 1 NIT (WHY comment on `mDirty`'s ctor-armed `{ true }`) applied to `ApvtsDirtyTracker.h:78`, riding the close commit.
- **Next:** `/draft-commit` → surface drafted message + full pre-commit `git status` for Jeff's explicit approval → close commit (the 4 docs + the 1-line `ApvtsDirtyTracker.h` NIT comment), via `git commit -F` per the project's commit mechanics.
