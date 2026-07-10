# Running Notes — QA-Fb′ (doubled-tracking-badger)

> Append-only running log for QA-Fb′. New `## YYYY-MM-DD — <checkpoint>` entry at every checkpoint per `feedback_draft_doc_running_notes_every_checkpoint.md`. Under BULK-RUN mode ([`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md)) there are no per-task verify entries; the Work Log close entry is drafted + HELD here under `## Held Work Log entry (apply at section pass)` at code-complete, applied at Master Test Plan §B.8 section pass (R2).
>
> Pair file: [`Plans & Specs/Batch Plans/doubled-tracking-badger.md`](../Batch Plans/doubled-tracking-badger.md). Conventions: Main Plan §0.

## 2026-07-09 — Group open (G2) — seeded

Plan approved 2026-07-09 (G2 group approval, R5). QA-Fb′ SHRANK vs the §5 docket: composite renderer moved to QA-F (Call 1a), clip-resize is now verify-only (QA-Ec wired it). Remaining real work: dual-buffer WET-bleed gate + conditional-WET + dirty-flag fix.

### Locked spec calls
- **Marathon 3b** — bleed gate: new take = live input ONLY; prior-take playback never bleeds in; no bounce toggle.
- **§3** — conditional-WET: skip WET recorder allocation when realtime pitch bypassed.
- **Call 1a** — composite renderer built in QA-F; QA-Fb′ consumes/verifies (not built here).
- **Verify-only** — clip-resize propagation (QA-Ec); dirty-flag fix = mirror Aux's `onAnyStateChange()` on Vox/Inst page-create.

### Surface map (current code, verified 2026-07-09 via Explore agent B)
- Dual recorders exist: `StripRecorder` `PluginProcessor.h:730-743`. DRY tap fires from strip tasks' live branch (`tapDryRecorder`, `VoxStripTask.cpp:154-157` / `InstStripTask.cpp:198-201`; helper `PluginProcessor.cpp:3261-3277`), gated on `armed`. WET tap `BaySickVocalProcessor.cpp:319-340`, triggered solely by `mWetRecorder` non-null.
- **Bleed unguarded:** WET attached `startRecording :3185` (arm-only, no FilePlay check), stays live whole session. Prior-take overlap → `VoxStripTask.cpp:46-120` FilePlay branch → engine processBlock → WET captures take 1; the branch `return`s at :120 so take 2's live DI isn't captured either.
- **Conditional-WET absent** — WET writes unconditionally.
- **Clip-resize WORKS** (verify): `BuilderPage.cpp:4690`→`commitEdit :5049`→`StandaloneEditor.cpp:2162-2165`→`rebuildAudioClipPlayers`→`clipEndBeat` bounds both decode paths (`PluginProcessor.cpp:1019-1020`/`:575-577`).
- **Dirty asymmetric:** record-finalize marks dirty (`commitRecordingResult` `StandaloneEditor.cpp:11314/11401`); Vox/Inst page-create does NOT (`MixerPage::addVoxChannelAtIndex :2026-2064` / `addInstChannelAtIndex :2358`), unlike `addAuxChannel :1819` (`onAnyStateChange()`).

### Fork-out (Rule 3)
- Do NOT test overlapping same-row multi-take here — that is the QA-AudioMeters BLOCKER's neighbor → campaign QA-J-Verify (§C ledger items 1-2).

## 2026-07-10 — Tasks 1-3 coded (Vox side) — InstStripTask spec call OPEN, build pending

- **Cross-check (session open):** Fb′ plan body vs the QA-Fa recovery — NO model contradiction (this plan targets the RECORDING path; the recovery rewired playback). Plan's line refs were pre-recovery; every surface re-read before coding per the resume protocol.
- **Task 1 — dual-buffer WET-bleed gate + conditional-WET — CODED (Vox side).**
  - `Source/Engine/Tasks/VoxStripTask.cpp` restructured: APVTS strip params hoisted above the FilePlay branch; new `filePlay` eligibility bool folds the pre-scan gates (flag + posInfo + isPlaying + patternManager + songMode); shared `decodeRoutedClips` lambda (builds AudioClipBlockContext + decodes routed clips into mEngineScratch) feeds both branches. Pure-playback branch is now `filePlay && !armed` — verbatim old behavior (decode → finalizeFilePlayStrip → return). ARMED+overlap path: live input copied + DRY tap fires → engine processes ONLY the live stream (WET tap fires on it) → if !listen the live monitor is cleared (capture already happened) → prior takes decoded + merged POST-engine (audible through the insert chain, never captured; they skip the vocal-chain engine while armed — one stateful engine can't process both streams without double-advancing) → single processInsert. No-overlap paths byte-identical to before (tail listen gate kept).
  - `Source/Engine/Tasks/VoxStripTask.h`: stale flow comment rewritten (referenced the deleted serial loop "lines ~1446-1525" + the old skip-if-FilePlay shape).
  - `Source/BaySickVocal/BaySickVocalProcessor.cpp` processBlock: WET tap now gated capture-eligible — `const bool filePlaySrc = mForcePitchBypass.load(acquire)` hoisted (it also drives the existing pitch mux); tap wrapped in `if (!filePlaySrc)`. **Shape judgment (logged):** reused the force-bypass atomic as the gate instead of adding a second atomic — it IS the live/FilePlay source-mux discriminator (exactly two writers: VoxStripTask live path = false, finalizeFilePlayStrip = true); a parallel flag would be redundant state that can drift. Plan asked for "a capture-eligible gate (atomic flag)" — satisfied with the existing atomic, zero new state.
  - `Source/PluginProcessor.cpp` startRecording: conditional-WET (G2-condWET) — WET recorder allocated only when the strip's BaySickVocalProcessor exists AND its `bsv_pitch_realtime_bypass` < 0.5 (realtime pitch ACTIVE). Param default is true (bypassed) → default recordings are DRY-only. Downstream verified: commitRecordingResult already falls back to placing the DRY file on the grid when no WET entry exists (`StandaloneEditor.cpp` ~:11730-11733), and `PatternManager::addAudioToLibrary` dedups on (path, pageOwnerChannelId) → the DRY double-add in that path is a no-op. Side effect: a Vox strip whose engine cast fails now gets NO wet file at all (previously an empty orphan WET wav with no tap attached) — strictly better.
- **Task 2 — dirty-flag on Vox/Inst page-create — CODED.** `Source/Standalone/MixerPage.cpp`: `addVoxChannelAtIndex` + `addInstChannelAtIndex` now fire `mProcessor.onAnyStateChange()` at tail (mirror of addAuxChannel / QA-Ef #5 — createAndAddParameter never fires a value-change, so the APVTS dirty hook can't see a page-add). Fired in the AtIndex body, NOT the addVoxChannel/addInstChannel wrappers, because user gestures reach AtIndex directly (onAddTabRequest, addBaySickGuitarsTab/addBaySickBassesTab, spawnAndLoadFromEmptyState, spawnDuplicateVox/InstTab, addVoxFromExport, createBuilderPage's free-page finder). Load-safety verified: the only load-path caller is deserializeUIState (3 sites) via onDeserializeUIState, which fires synchronously inside deserializeProject, and both ProjectManager call sites wrap that with mIgnoreDirty=true → markDirty no-ops. Record-finalize dirty confirmed still present (commitRecordingResult :11754 + MIDI branch :11841) — no change.
- **Task 3 — verify-only — DONE at code level (runtime rides §B.8).**
  - Clip-resize propagation HOLDS: BuilderPage resize paths (setLengthBeats ~:3287 + :3602, plain-drag + stretch) → commitEdit :2567 → onArrangementChanged :2588 → StandaloneEditor :2166-2169 → rebuildAudioClipPlayers → clipEndBeat bounds both decode paths (`PluginProcessor.cpp` :589 decodeFilePlayClip / :1033 renderAudioClipsForRow; rebuild stamps clipEndBeat at :3282). Plan's pre-recovery line refs drifted slightly; every link present.
  - Composite renderer consume-verified: `renderChannelComposite` (`PluginProcessor.cpp:3364`) — multi-clip iteration, routeChannel-to-Vox matching, alignBake exclusion intact post-recovery, effectiveStart/LengthBeats + EOF clamp; consumed via the `VoxPage.cpp:499` hook by BaySickAlign/BaySickPitch.

### Spec call OPEN (asked in chat; Inst coding HELD)

- **InstStripTask scope.** InstStripTask has the identical FilePlay early-return — an armed Inst take-2 over a prior take loses live DI capture + monitoring (no WET half; Inst has no wet recorder). Plan's files-to-modify lists only VoxStripTask; plan context says "Vox/Inst recording path". Options posed: **(a)** same restructure to InstStripTask in-batch (DRY + monitor merge only), **(b)** Vox-only per the plan file list, Inst gap routed via Rule 3 at the §B.8 section pass.

### Found along the way (Rule 3 — route at section pass unless Jeff pulls in-batch)

- (a) **WET tap allocates on the audio thread:** `juce::AudioBuffer<float> monoView (1, numSamples)` constructed EVERY block while recording (BaySickVocalProcessor processBlock tap body) — pre-existing since I-16 G-9 (2026-05-03), RT-danger class. Fix shape = a member scratch. Surfaced to Jeff (fix-in-batch vs route = his call).
- (b) **Unarmed + listen-on strip with overlapping FilePlay clips:** the pure-playback branch preempts live monitoring (listen-only monitor drops during clip overlap) — pre-existing, structural, NOT armed-record scope. Route.
- (c) **Design consequence flagged to Jeff** (inherent to the locked G2-3b model, not a new call): while armed over overlapping takes, the monitored prior takes skip the vocal-chain engine (no chain FX / pitch applicator on them during the armed window; align warp still applies at decode). They keep insert-rack/EQ/fader processing. Reverts to full chain the moment recording stops.

### Status

- Rule 4 diagnostic catalog: **no diagnostic instrumentation added** (catalog empty for QA-Fb′).
- InstStripTask untouched pending the spec call; §B.8 authoring + the held Work Log entry follow once the call resolves and the build runs clean.
- Working tree: 5 files modified (`VoxStripTask.cpp/.h`, `BaySickVocalProcessor.cpp`, `PluginProcessor.cpp`, `MixerPage.cpp`), UNCOMMITTED, BUILD NOT YET RUN.

## 2026-07-10 — All five spec calls resolved + Option A monitor rebuild — code-complete, build pending

All five open calls resolved in chat (Jeff, 2026-07-10, after a design workshop). Call 4 reworks the monitor model — the As-built section below SUPERSEDES the prior entry's ARMED+overlap post-engine-merge shape.

### Spec calls resolved

- **1 — conditional-WET extended:** skip the WET writer when the page's MASTER bypass (`bsv_bypass`) is on too, not just realtime-pitch bypass — kills the empty-orphan-WET-on-grid edge. CODED (`PluginProcessor.cpp` startRecording).
- **2 — InstStripTask scope:** option (a) — fixed IN-BATCH (full mirror restructure, As-built below).
- **3 — WET-tap audio-thread allocation** (prior entry's finding (a)): fix in-batch. CODED — `mWetMonoScratch` member sized in prepareToPlay, lazy-grow guarded like `mDryScratch`, non-owning view into it for writeBlock (`BaySickVocalProcessor`).
- **4 — Monitor design = Option A + A1 (the big rework).** Jeff rejected the prior entry's (c) design consequence — prior takes monitored WITHOUT the vocal chain "defeats the point". Workshop key insight: post-stop playback runs ALL stacked takes through ONE shared chain (finalizeFilePlayStrip processes the summed decode once), so summing the live stream into that same chain during tracking IS the accurate playback preview — per-stream chains (Jeff's floated hybrid: dup rack, shared NAM) would produce a monitor that never matches playback. Locked: Option A (sum into the one chain at the post-corrector / post-WET-tap point) + A1 (takes' BaySickPitch note edits stay audible via a monitor-side applicator stream).
- **5 — Listen-only fold-in** (prior entry's finding (b)): the merged path gates on ACTIVE (armed OR listen) — live monitoring coexists with playing clips on both Vox and Inst.

### As built (supersedes the prior entry's post-engine-merge shape)

- `Source/DSP/BaySickPitchDSP.h/.cpp`: new `processFilePlayMonitor()` — processFilePlay's twin over a second stream state set (`mMonShifters` / `mMonFormant` / `mMonState`, prepared in prepare()); shared snapshot + knob loads + fast-path gates; the two streams share nothing mutable.
- `Source/BaySickVocal/BaySickVocalProcessor.h/.cpp`: block-scoped monitor-merge contract `setMonitorMergeForThisBlock(takes, timelineSample, muteLive)` — plain members (same-thread: VoxStripTask sets immediately before its synchronous processBlock call), CONSUMED + CLEARED at the very top of processBlock so early-outs (empty block / shutdown / master bypass) can never leave a stale task-scratch pointer. Master-bypass path still applies the merge RAW (prior takes stay audible under bypass, matching pure playback). Merge point: after corrector + WET tap, before the rack — muteLive (`armed && !listen`) clears the live stream from the monitor AND from `mDryScratch` (else the Mix crossfade would reintroduce it); raw takes are added to the dry reference BEFORE the monitor applicator mutates them (matches pure playback's dry-stash-before-pitch-stage order); then `mPitch.processFilePlayMonitor` (A1), then buffer += takes → rack + NAM process the summed stack.
- `Source/Engine/Tasks/VoxStripTask.cpp/.h`: pure-playback gate widened `!armed` → `!active` (call 5); the live path decodes routed clips and hands them to the engine hook BEFORE processBlock (replaces the prior entry's post-engine merge); null-vocal-engine fallback keeps the old post-engine raw merge; tail listen gate now `!filePlay && armed && !listen` (overlap blocks handled by muteLive in-engine). Header flow map rewritten.
- `Source/Engine/Tasks/InstStripTask.cpp/.h` (call 2): full mirror restructure — sfizz detection + params hoisted, `filePlay` bool, shared `decodeRoutedClips` lambda, pure-playback branch `filePlay && !active` (sfizz strips always land there — arm/listen forced off), idle-suspend unchanged. Live path merges decoded takes with the live DI PRE-engine (one pedals/amp pass over the stack = bit-identical to playback; DRY tap reads the raw snapshot so capture never sees the merge); `armed && !listen` clears live pre-merge; tail gate now `!filePlay && armed && !listen`. Header flow map rewritten. No WET side (Inst has none by design).
- `Source/PluginProcessor.cpp` startRecording (call 1): `rtPitchActive` now also requires `bsv_bypass < 0.5`.
- **§B.8 AUTHORED** in [`Test Plans/v1-master-test-plan.md`](../Test Plans/v1-master-test-plan.md): FB-1 no-bleed, FB-2 no-missing-stretches, FB-3 conditional-WET (3 legs incl. master bypass), FB-4 monitor-preview (Option A/A1 + idle fast-path cost), FB-5 armed+listen-off, FB-6 Inst multi-take, FB-7 listen-only coexist, FB-8 dirty page-create, FB-9 dirty record-finalize, FB-10 resize+composite regression confirms. Same-row overlap explicitly EXCLUDED (campaign §C ledger items 1-2); sound-quality listening rides the G2 boundary ear-check.

### Found-along-the-way dispositions (closes out the prior entry's list)

- (a) WET-tap RT alloc — **FIXED in-batch** (call 3).
- (b) unarmed + listen-on preempt — **FIXED in-batch** (call 5).
- (c) takes-skip-chain design consequence — **DISSOLVED** (Option A supersedes it: monitored takes now run the one shared chain while armed).

### Status

- Code-complete pending Jeff's `do_build.bat` (both configs to clean).
- Rule 4 diagnostic catalog: still empty for QA-Fb′.
- Held Work Log entry: drafting in parallel; applied below under its own heading when it lands.
- Working tree: 10 source files + 2 Plans & Specs files (test plan + this file) modified, UNCOMMITTED. One commit at close per bulk-run.

## Held Work Log entry (apply at section pass)

> Apply to `Implemented Work Log.md` when §B.8 passes (R2). Stamp `HH:MM PT` at apply time.

```markdown
### 2026-07-10 — QA-Fb' — Recording lifecycle: bleed gate + Option A monitor merge + conditional-WET + dirty-flag

**Bucket:** Cross-cutting Infrastructure, System Pages, Players, Effects
**Plan:** `Batch Plans/doubled-tracking-badger.md` · **Running notes:** `Running Notes/doubled-tracking-badger.md` · **Commit:** `66fea472`

#### Done

- **Task 1 — dual-buffer WET-bleed gate + conditional-WET + Option A monitor merge.** The armed-record FilePlay early-return in `VoxStripTask` / `InstStripTask` — which both BLED prior-take playback into a new take's WET file AND dropped the take's live capture entirely during overlap — is gone. New shape (Option A, locked in a 2026-07-10 chat workshop with Jeff): a LIVE strip (armed OR listen) over FilePlay clips decodes the prior takes and merges them into the SINGLE engine chain at the correct point — post-stop playback runs all stacked takes through one shared chain, so the monitor is now an accurate playback preview. Vox: merge inside `BaySickVocalProcessor::processBlock` after the corrector + WET tap, before the rack (block-scoped same-thread contract `setMonitorMergeForThisBlock`, consumed + cleared at top-of-block; the master-bypass path merges raw; muteLive covers armed-without-listen incl. the Mix-crossfade dry reference). A1: the takes' BaySickPitch note edits stay audible during tracking via `BaySickPitchDSP::processFilePlayMonitor` — a second applicator stream state (own PSOLA/formant/glide cursors, shared snapshot). Inst: takes sum with the live DI PRE-engine — one pedals/amp pass over the stack, bit-identical to playback; the DRY tap reads the raw input snapshot pre-merge. WET tap gated capture-eligible on `!mForcePitchBypass` (the live/FilePlay source-mux discriminator — its only two writers are the VoxStripTask live path and `finalizeFilePlayStrip`; zero new state). Conditional-WET in `startRecording`: the WET writer is allocated only when the strip's vocal engine exists AND realtime pitch is active AND master bypass is off (G2-condWET + Jeff's 2026-07-10 master-bypass extension); default = DRY-only takes; `commitRecordingResult`'s existing no-wet fallback + `addAudioToLibrary` (path, channelId) dedup verified downstream-clean. Listen-only fold-in (#5): the pure-playback gate moved from `!armed` to `!active`, so unarmed+listen monitoring now coexists with playing clips (pre-batch the clips silenced the live monitor). WET-tap RT allocation fixed (member `mWetMonoScratch` + non-owning view; was a per-block AudioBuffer construction).
- **Task 2 — dirty-flag: Vox/Inst page-create.** `MixerPage::addVoxChannelAtIndex` + `addInstChannelAtIndex` fire `mProcessor.onAnyStateChange()` at tail (mirror of addAuxChannel / QA-Ef #5; `createAndAddParameter` fires no value-change). In the AtIndex body because user gestures reach it directly (onAddTabRequest, guitars/basses tab adds, spawnAndLoadFromEmptyState, tab duplicates, addVoxFromExport, Builder free-page finder); load-safe because deserializeUIState runs via onDeserializeUIState synchronously inside deserializeProject, which both ProjectManager call sites wrap with mIgnoreDirty. Record-finalize dirty confirmed unchanged (commitRecordingResult :11754 + :11841).
- **Task 3 — verify-only.** Clip-resize propagation HOLDS end-to-end (BuilderPage `setLengthBeats` plain-drag + stretch paths → `commitEdit` → `onArrangementChanged` → `rebuildAudioClipPlayers` → `clipEndBeat` bounds both decode paths; the plan's pre-recovery line refs drifted, every link present). Composite renderer consume-verified (`renderChannelComposite`: multi-clip iteration, routeChannel matching, alignBake exclusion intact post-recovery; consumed via the VoxPage hook by Align/Pitch).
- **Test plan.** §B.8 authored (FB-1..FB-10) — no-bleed, no-missing-stretches, conditional-WET 3 legs, monitor-preview, armed-no-listen, Inst multi-take, listen-only coexist, dirty x2, resize + composite regression confirms. Same-row overlap excluded (campaign §C items 1-2); sound-quality listening rides the G2 boundary ear-check.
- **Files:** `Source/Engine/Tasks/VoxStripTask.cpp/.h`, `Source/Engine/Tasks/InstStripTask.cpp/.h`, `Source/BaySickVocal/BaySickVocalProcessor.cpp/.h`, `Source/DSP/BaySickPitchDSP.cpp/.h`, `Source/PluginProcessor.cpp`, `Source/Standalone/MixerPage.cpp` + test plan §B.8 + running notes.

#### Found along the way

- WET tap allocated on the audio thread every block while recording (pre-existing since I-16 G-9, 2026-05-03) — FIXED in-batch (Jeff call).
- Listen-only monitor preempted by overlapping clips (pre-existing, structural) — FIXED in-batch via the #5 fold-in (Jeff call).
- The batch's own first-cut design (prior takes monitored WITHOUT the vocal chain) was caught by Jeff at review — superseded same-day by the Option A workshop; the "takes skip the chain" trade-off no longer exists.
- Master bypass during recording produced an allocated-but-never-written WET file that the commit path would place as an empty clip (pre-existing edge) — FIXED in-batch via the conditional-WET master-bypass leg.

#### Spec calls

- Locked pre-batch: G2-3b bleed gate (new take = live input only), G2-condWET, G2-1 composite consume, G2-resize verify-only, G2-dirty.
- Mid-batch, all resolved in chat 2026-07-10: (1) conditional-WET master-bypass extension — yes; (2) InstStripTask same-fix in-batch — yes; (3) RT-alloc fix in-batch — yes; (4) monitor design Option A + A1 (workshop: one-shared-chain monitoring IS the playback-accurate preview; per-stream chains rejected as a monitor that never matches playback); (5) listen-only fold-in — yes.

#### Routed (Rule 3)

- Overlapping-same-row multi-take scenarios stay campaign QA-J-Verify (§C ledger items 1-2) — §B.8 setup explicitly excludes them. No other new routings; all found items were pulled in-batch by Jeff's calls.
```
