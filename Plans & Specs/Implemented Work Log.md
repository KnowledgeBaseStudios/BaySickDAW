# Implemented Work & Findings — Post-Batch-10 QA Execution Log

> **Append-only.** This is the running ledger of executed batches: what was
> done, what was found along the way, and what was done about each finding.
> Never edit prior entries. Surprise findings later get their own new entry.
> Contradictions of the carry-forward
> ([Carry-Forward Reference.md](Carry-Forward Reference.md))
> are recorded here as new entries — the carry-forward stays as the
> historical snapshot of 2026-05-07.

**How to read:** entries are chronological (oldest first). Each entry covers
one batch session OR one significant stopping point within a batch. Use
the table of contents below to jump to a batch.

## Header conventions

Cross-doc rules live in [Main Plan.md](Main Plan.md) §0 "Document
Formatting Conventions". Local layout for this doc:

- `#` — document title
- `##` — top-level section (How to read, Header conventions, Entry template, Table of contents, Entries)
- `### YYYY-MM-DD HH:MM PT — <Batch ID> — <Summary>` — individual batch close entry header
- `**Bucket:** <bucket1>[, <bucket2>...]` line immediately under each batch close entry header — names every canonical domain bucket from Main Plan §0 that the batch's scope touched. Multi-bucket batches list every bucket touched. Used for cross-doc grep: `Bucket: Effects` finds every batch-close that touched Effects.
- `#### <Sub-section>` — sub-section within an entry (Done, Found along the way, etc.)

Grep patterns:
- `^### ` finds all batch close date entries
- `\*\*Bucket:\*\*` finds the bucket tag line on each entry
- `^#### ` finds sub-sections within entries
- `^## ` finds top-level sections

Timestamps use 24-hour clock and `PT` (Pacific Time) generically — covers both PDT and PST without DST-overlap ambiguity.

Append-only. Never edit prior entries — surprise findings later get their own new entry. Carry-forward contradictions are recorded as new entries, not as edits to the carry-forward.

**Entry template** (paste at the bottom and fill in):

```
### YYYY-MM-DD HH:MM PT — <Batch ID> — <One-line summary>

##### Done
- [list of things completed in this session]

##### Found along the way
- [surprises, gotchas, things that contradicted the plan or carry-forward]

##### What was done about each finding
- [actions taken on each finding above; or "logged for follow-up batch X"]

##### Carry-forward contradictions (if any)
- [if §X of carry-forward turned out wrong, record here: "§X said Y, verified Z"]

##### Carry-over to next session (if not closing batch)
- See `## Carry-Over` block at bottom of the active per-batch plan.

##### Files touched
- [list of files modified in this session]

##### Commit(s)
- [hash + one-line message]
```

---

## Table of contents

(Append entries below this line. New entries auto-append to the bottom.)

---

### 2026-05-07 21:27 PT — Triage — Plan + carry-forward + implemented-work docs created

**Bucket:** Meta

#### Done
- Read the unified backlog end to end (64 entries: 59 active + 4 optional + 1 never).
- Read the MT batch map for resolved-en-route context.
- Spawned 3 parallel Explore agents to verify architectural primitives (MT engine, snapshot/lifecycle patterns, mixer/page lifecycle).
- Sanity-checked specific items against current code (DSP-10, BUILD-06, MIX-01, UI-01, DSP-09, hamburger MT toggle wiring).
- Diagnostic discovery: WAV/MP3 drop on Builder grid is silent under MT (regression), plays correctly under serial. Confirms DSP-12 channel-id conflict is the cause and elevates it to top-priority QA-0.
- Resolved 7 design calls with Jeff (DSP-12 fix shape, DSP-09 target behavior, FILE-02 routing/timing, NAV-03/04 specs, NAV-05 remove, MIX-03 = symptom of MIX-02).
- Wrote main plan ([Main Plan.md](Main Plan.md)) — 20 batches across 6 phases (15 bug-fix + 5 cleanup audit).
- Wrote carry-forward reference ([Carry-Forward Reference.md](Carry-Forward Reference.md)) — frozen snapshot of architectural primitives, file:line index, decisions, per-item status, patterns to reuse.
- Created this implemented-work doc (initial state, no batches executed yet).

#### Found along the way
- The hamburger MT toggle wiring exists ([StandaloneEditor.cpp:4450-4497](../Source/Standalone/StandaloneEditor.cpp)) — Explore agent missed it on first sweep. Self-verified.
- MIX-03 is not a direct bug — Jeff clarified it's a symptom of MIX-02. The auto-spawn-clips-strip behavior at recording time is correct; the bug is that on save→reload the Vox strip disappears (MIX-02) and the orphan recording on the Builder grid becomes a clips strip on next load.
- DSP-12 is not just an architectural design issue — it's the active cause of the WAV-drop-on-Builder regression. ClipPageTask wins the channel-id registration race and silences AudioInsertTask. Validation suite missed it because Vox/Inst recording tests use VoxStripTask/InstStripTask paths (different channel id ranges, no conflict).
- The dead `Properties...` menu entry at [BuilderPage.cpp:2561](../Source/Standalone/BuilderPage.cpp) has no `case 7` handler — it's added unconditionally for all block types and does nothing.
- Several agent reports overstated "FIXED" judgments (UI-01/UI-02 in particular). Jeff confirmed they're still open. Agents conflated "outer right-click handler is gated correctly" with "JUCE PopupMenu rejects right-click as item activation" — different code paths.

#### What was done about each finding
- DSP-12 elevated to QA-0 (first batch, top priority).
- MIX-03 folded into MIX-02 in the plan; not tracked as separate work.
- Dead Properties cleanup added to QA-E scope (touches the same Properties popup as FILE-02).
- Plan structure adjusted to mark Phase 6 (cleanup audit) as its own dedicated phase.
- Three-doc system + carry-over discipline added to plan §0 to prevent context loss across sessions.

#### Carry-forward contradictions (if any)
- None. Carry-forward was authored from this same triage session, so it reflects current findings.

#### Files touched
- `Main Plan.md` (created)
- `Carry-Forward Reference.md` (created)
- `Implemented Work Log.md` (created — this file)
- No source code changes.

#### Commit(s)
- None. Read-only triage session.

#### Next action
- Start QA-0 (MT Composite RenderTask). Read carry-forward §1-3, then read the actual `AudioInsertTask::run()` and `ClipPageTask::run()` bodies, then write the QA-0 per-batch implementation plan as a new file under `C:\Users\jeffm\Documents\BaySickDAW\Plans & Specs\Batch Plans\<silly-name>.md`.

---

### 2026-05-07 16:29 PT — QA-0a — Debug build workflow setup

**Bucket:** Cross-cutting Infrastructure

#### Done
- Annotated main plan with the QA-0a fork using the hybrid convention: §0 Rule updated, §5/§6/§7 inline back-refs, §9 master log appended.
- do_build.bat now builds Release + Debug. Build log writes per-config exit codes.
- Window title appends `[DEBUG]` under `#ifdef JUCE_DEBUG` in `StandaloneEditor::refreshWindowTitle`.
- CMakeLists.txt sets `JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING=1` to silence benign AAX paramID asserts (BaySickDAW is standalone-only).
- CLAUDE.md Build System section updated with the new dual-config workflow + standing rule "verify in Debug first, then Release" + cross-stream caveats (settings.xml shared, ASIO exclusivity, MT engine no-op in Debug).
- Cold-start triage of every assert that fired during Debug startup + project load - 13 findings cataloged. Real source-side fixes for 4 of them; 7 vendored-JUCE suppressions for benign warnings; 2 logged-and-deferred for dedicated batches.

#### Found along the way
- **Debug build was BROKEN before this session.** Two C3493 "cannot implicitly capture" errors in lambdas (PluginProcessor.cpp:2883 `kPeakNegInf`, SharedUI.h:733 `kMidiFirstId`) that Release silently tolerates via constant folding. Latent issue surfaced the moment we tried to actually use the Debug exe. Every "F5" before Tasks 5/6 launched a stale exe from the FIRST do_build.bat run. Fixed by adding explicit captures.
- **Voice ctors propagate sampleRate=0 into ADSR + StateVariableTPTFilter at construction.** All four engine synth ctors (VibeSynth, BaySickSynth, Harmless, main VibeSynthProcessor synth) call addVoice() before any sample rate is set. JUCE's Synthesiser::addVoice propagates the synth's current sample rate (0) to each new voice's ADSR + filter. Real values arrive later via prepare/prepareToPlay before any audio. Fixed at source: setCurrentPlaybackSampleRate(44100) before the addVoice loop in all four ctors. Also kept defensive belts in vendored JUCE in case a future engine misses this.
- **CMake JUCE+VS multi-config gotcha** prevented per-config ICON_BIG gating. Reverted the conditional - both exes ship with the BaySickDAW branded icon. User accepted "differentiation via window title only".
- **Project-wide em-dash sweep needed.** Initial KeyBindings.cpp fix surfaced ~150 more files with em-dashes. Bulk-replaced all U+2014 em-dashes with ASCII hyphens across every .cpp + .h under Source/ (217 files). Fixes both the JUCE ASCII-validity assert and the long-standing box-glyph render bug. Box-drawing decorations (U+2500 in `// ──` comments) untouched - they're not in string literals.
- **MT engine no-op under Debug** (finding #9). DSP meter shows identical readings whether MT is on or off; toggle persists to settings.xml correctly but the dispatcher isn't actually distributing work to threads. Real Debug-only bug; Release MT works fine. Logged for dedicated batch.
- **Dangling InstPage* in tab-click lambda** (finding #13). Confirmed via `0xDDDDDDDDDDDDDDDD` debug-fill marker. Use-after-free crash on tab click after engine swap or project reload. `[this, ip, syncPagePresetMenu, labels]` capture stores raw InstPage* that gets freed elsewhere. Real fix: SafePointer<InstPage> or per-click index lookup. Logged for dedicated batch.
- **MenuBarModel listener-dangle on closeAllDynamicTabs** (finding #8). MenuBarModel destroyed before MenuBarComponent during dynamic-tab teardown cascades. Real but lower-severity (JUCE's removeListener is set-safe no-op on miss). Logged for dedicated batch. closeAllDynamicTabs design itself is correct; this is a downstream wiring detail.

#### What was done about each finding
- All 13 findings cataloged with status (fixed at source / suppressed in vendored JUCE / logged for dedicated batch). See commit messages on `b34c54d` and `bd67fdf` for the per-finding rationale.
- Three deferred-real-bug findings (#8 MenuBar lifecycle, #9 MT no-op in Debug, #13 InstPage use-after-free) explicitly NOT fixed in QA-0a. They surfaced via QA-0a's diagnostic infrastructure and become candidate work items for future batches.

#### Carry-forward contradictions (if any)
- **Carry-forward §1 says MT engine is "production, default ON".** That's true for Release. **For Debug builds it's a no-op** - finding #9. Carry-forward stays as the historical 2026-05-07 snapshot; this entry records the contradiction.

#### Files touched
- Modified workflow: do_build.bat, CMakeLists.txt, CLAUDE.md.
- Modified targeted source: PluginProcessor.cpp (lambda capture + voice ctor sample rate), Standalone/StandaloneEditor.cpp (window title), Standalone/SharedUI.h (lambda capture), Standalone/KeyBindings.cpp (em-dash literals - 19 sites), VibePlayer/VibePlayerDSP.cpp + BaySickSynth/BaySickSynthDSP.cpp + Harmless/HarmlessSynth.cpp (voice ctor sample rate).
- Modified em-dash sweep: 217 files under Source/.
- Modified vendored JUCE: 7 files in juce/modules/.
- Modified plans: Main Plan.md (§0 rule update + §5/6/7 inline back-refs + §9 master log appended; convention rule for "Plan file:" header lines), Implemented Work Log.md (this entry).

#### Commit(s)
- `b34c54d` QA-0a: Debug build workflow setup + source-side fixes that the new Debug surfaced.
- `a472a44` QA-0a: project-wide em-dash sweep across Source/ (217 files).
- `bd67fdf` QA-0a: vendored JUCE assertion suppressions for benign Debug-only noise.

#### Next action
- QA-0 (MT Composite RenderTask / DSP-12 fix). Re-author the plan as a fresh silly-name file from chat history, then execute against the new dual-config build (verify in Debug first, then Release). Three deferred findings (#8 / #9 / #13) wait their turn as dedicated batches.

---

### 2026-05-07 18:23 PT — QA-0 — MT Composite RenderTask (DSP-12 restore)

**Bucket:** Cross-cutting Infrastructure, Players

#### Done
- New `Source/Engine/Tasks/CompositeAudioInsertTask.h/.cpp` per-row task that owns BOTH the arrangement-clip flow (was AudioInsertTask) AND the clip-engine MIDI trigger flow (was ClipPageTask) at audioInsert(N) channel ids.
- Order B execution inside `run()`: clear blockView once -> clip-engine flow if engine set -> arrangement-clip flow accumulating via `renderAudioClipsForRow` mtDest. Both contributions sum into mOutputBuffer.
- Strategy 1a lifecycle: `ensureAudioInsert(row)` constructs the Composite. `registerClipEngine(pageIdx, eng)` defensively ensures the Composite exists then sets its mClipEngine pointer. `unregisterClipEngine` clears the pointer; the per-row Composite stays alive.
- Old `AudioInsertTask.h/.cpp` + `ClipPageTask.h/.cpp` deleted (4 files). `mClipRenderTasks[]` array deleted from PluginProcessor. CMakeLists.txt updated. Friend declarations swapped to single `CompositeAudioInsertTask`.
- `RenderGraphDispatcher::registerTask` got a `jassertfalse` tripwire on its most-recent-wins fallback. For normal paths the branch is now unreachable; tripwire surfaces any future regression where a new task type re-introduces a multi-source channel without going through a composite shape. Release silent fallback stays as safety net.
- DSP-12 verified in Release with both MT-on and MT-off: WAV + MP3 drops on Builder play correctly; piano-roll triggers via main Piano Roll tab play correctly through the auto-spawned Clips engine; both flows sum on the same audio insert.

#### Found along the way
- **#14** Use-after-free crash on Clips player-page Piano Roll button (`StandaloneEditor.cpp:4048` lambda __l41 / __l10). Same family as #13 (showPageForTab tab-click lambda). Stale InstPage* / page captured by raw pointer; freed during engine swap or project reload.
- **#15** Dropping a WAV/MP3 on Builder grid auto-navigates to the player page; user expects to stay on Builder.
- **#16a** Pattern row-level mute has no effect on pattern playback. Pattern keeps dispatching MIDI to its engine regardless of row mute state.
- **#16b** Right-click pattern block -> mute -> no effect. Pattern keeps playing.
- **#16c** When audio row is muted, the audio clip's streamer pauses at its current expectedFilePos. On unmute it resumes from that frozen position rather than syncing to current project transport -- visible desync.
- **#17** App-shutdown crash in `BuilderPage::~BuilderPage` -> `TreeView::~TreeView` -> `TreeViewItem::setOwnerView` walks dangling subItem pointer. Pre-existing destructor-ordering bug (`BuilderPage.cpp:4418`).
- **#18** Muting a block resets its loop count to 1 (block stays visible but silent + loop state lost).
- **#19** Can't drag audio clips back onto Builder grid after deletion. Browser -> Builder drop only works for first-time imports.
- **#20** UX gap: clicking a pattern / audio clip / automation in browser should make it the "active drop type" for clicks on empty Builder space (mimicking piano-roll last-block-type behavior).
- **#21** Track row mute (with audio clip) permanently mutes; no way to unmute via existing UI.

#### What was done about each finding
All 8 new findings + 3 carried from QA-0a (#8, #9, #13) routed at QA-0 close per Rule 3 (§0). See main plan §9 Forks entry "QA-0 close routings" for full mapping. Summary:

| Finding | Routing |
|---|---|
| #8 | Folded into QA-D (project-load teardown surface) |
| #9 | Promoted to QA-Md (new dedicated Phase 1 batch immediately after QA-0) |
| #13, #14, #16a, #16b, #21 | Folded into QA-E (audio-row + tab-callback + mute-dispatch surfaces) |
| #15, #17, #18, #19, #20 | Folded into QA-H (Builder UX + state cluster) |
| #16c | Folded into QA-J (per-row audio-clip rendering surface) |

QA-Md promoted from Phase 5 to Phase 1 because Debug's diagnostic value depends on MT actually engaging there; downstream batches need this for precise MT bug diagnosis.

#### Carry-forward contradictions (if any)
- Carry-forward §1 says MT engine is "production, default ON". True for Release; **no-op under Debug per finding #9**. Already noted in QA-0a's implemented-work entry; reaffirmed here. QA-Md investigates.

#### Files touched
- New: `Source/Engine/Tasks/CompositeAudioInsertTask.h/.cpp`
- Deleted: `Source/Engine/Tasks/AudioInsertTask.h/.cpp`, `Source/Engine/Tasks/ClipPageTask.h/.cpp`
- Modified: `CMakeLists.txt`, `Source/PluginProcessor.h`, `Source/PluginProcessor.cpp`, `Source/Engine/RenderGraphDispatcher.cpp`
- Modified (plans): `Main Plan.md` (§5 entries for QA-D/E/H/J got Folded-in sub-bullets; new QA-Md §5 entry; §6 sequencing arrow updated; §9 Forks entry "QA-0 close routings" appended), `composite-merging-rivers-twilight.md` (QA-0 per-batch plan), `Implemented Work Log.md` (this entry).

#### Commit(s)
- `611db82` QA-0 Step 1: scaffold CompositeAudioInsertTask (skeleton).
- `f72cd09` QA-0 Step 2: implement Composite::run() (Order B both flows).
- `df6f0a3` QA-0 Step 3: wire CompositeAudioInsertTask at registration sites; drop separate ClipPageTask registration.
- `0ef0c95` QA-0 Step 4: remove orphaned AudioInsertTask + ClipPageTask sources.
- `4200479` QA-0 Step 5: jassertfalse tripwire on dispatcher most-recent-wins fallback.

#### Next action
- QA-Md (MT Engine Debug-Build Investigation). Plan file: TBD silly-name when batch starts. Diagnostic-first: determine why MT path is no-op under Debug (workers not picking up tasks, runUntilOrTimeout returning immediately, or compile-time gate). Then fix scope.

---

### 2026-05-08 10:42 PT — QA-Inventory — 1429 source-doc items triaged + plan docs populated

**Bucket:** Meta

#### Done
- Parsed all three pre-QA source docs end to end (`Files For Claude/Final Stretch Work.txt`, `Files For Claude/vibedaw_blueprint.md`, `C:/Users/jeffm/.claude/plans/lucky-discovering-tiger.md`); 1429 distinct items extracted to a master TSV at `C:/Users/jeffm/.claude/plans/qa-inventory-master.tsv`.
- Created Google-Sheet walkthrough via the user's Drive connector (auto-converted CSV upload). User filled the Decision column for buckets A/B/E (293 walked items) over a multi-day pass.
- Phase-4 source verification on the C-bucket (claimed-Done items): headline + 20% sample + escalate-on-miss. 968 items confirmed Done at source. FSW-244 reclassified during this phase (`MixerPage::showInputChannelPicker` had no diagnostic submenu → walked-to-Drop).
- Phase-5 fire-hose on bucket D: every plausible post-v1.0 idea captured to `Future State.md`.
- Cluster dedupe via subagent: 1120 combined Done rows reduced to 1089 across 29 clusters (saved 31). Report at `C:/Users/jeffm/.claude/plans/qa-inventory-dedupe-report.md`. Walked through every multi-row cluster with the user; merge / keep-all decisions captured (Cluster 5d treble-bug folded into §12 EQ8; Cluster 10 delete prompts kept pending sweep; etc.).
- Populated the new `Plans & Specs/` doc skeletons:
  - `Previously Implemented.md` — 1089 deduped entries written by background subagent in uniform 5-line shape under 25 phase/module sections. 6,644 lines / 262 KB final.
  - `Future State.md` — Section 3 (Considered & Dropped) populated with 17 walked-to-Drop entries; Section 4 populated with 6 walked-to-D-bucket entries.
  - `Implemented Work Log.md` — header convention section added (this entry's family); existing Triage / QA-0a / QA-0 entries bumped from `##` to `###` with retroactive PT timestamps for chronological consistency.
- Updated `Main Plan.md`:
  - Scope expansions folded into existing batches (QA-A / QA-E / QA-F / QA-J / QA-L) per Rule 3.
  - 9 new batches added (QA-Drum-Polish / QA-VibeSlider / QA-Verify / QA-Export / QA-RC / QA-Manuals / QA-Templates / QA-Installer / QA-Framework).
  - §6 Sequencing arrow rewritten with new `****` footnote covering close additions; new Phase 7 section added between QA-RC and §7.
  - §9 Forks fifth entry chronicles all routings, cluster decisions, dedupe stats, and side findings.

#### Found along the way
- **CL-024 / T-Pain hard-tune is achievable today** via existing realtime pitch-correction params (`RetuneSpeed=0` + `Strength=1` + `Chromatic`) — but the user's runtime test surfaced that **realtime pitch correction is broken**: YIN tracker reports "Detected --" forever even with `mPitchCorrector.process()` confirmed in the chain at `Source/BaySickVocal/BaySickVocalProcessor.cpp:317`. PitchCorrectorDSP defaults `bypass=true`, but the deeper bug is the YIN tracker not firing. Side finding routed to QA-F DSP-03 scope.
- **Vox AND Inst recordings don't play back on Builder**, not just Vox. User test confirmed both surfaces are affected; same family as BLU-470. Routed to QA-E REC-01 scope.
- **Pedalboard preset round-trip is broken** — user discovered while testing the recording flow. Save/load fails to restore the rack state. Routed to QA-E REC-01 (recording lifecycle owns preset XML).
- **PitchCorrectorDSP Formant-Preserve + Throat-Sim parameters are no-op stubs** — `Source/DSP/PitchCorrectorDSP.cpp:326-327` `juce::ignoreUnused (mFormantPreserve, mThroatSemis);` confirms. Routed to QA-F DSP-03.
- **BLU-605 voxRoll/instRoll** is NOT dead code — it's needed for Inst BaySickGuitars/Basses + reserved for future SFZ vocal. Reclassified from "Drop = remove" to "Drop = no work needed; keep infrastructure".
- **LDT-173 TTF embed in installer** is distinct from STYLE-02 font choices. Kept as scope inside the new QA-Installer batch.
- **Subagent dedupe initially over-counted** clusters (24 false dups including per-module DSP retrospectives like A1 ScopedNoDenormals × 9). Refined to 17 true clusters with module-key extraction.

#### What was done about each finding
| Finding | Routing |
|---|---|
| Realtime pitch correction broken at runtime | QA-F DSP-03 (existing surface) |
| Vox + Inst recordings don't play on Builder | QA-E REC-01 (broadened from BLU-470) |
| Pedalboard preset round-trip broken | QA-E REC-01 (preset round-trip cluster) |
| Formant-Preserve / Throat-Sim no-op | QA-F DSP-03 |
| BLU-605 voxRoll/instRoll | Drop (keep infra; not work) |
| LDT-173 TTF embed | New QA-Installer batch |
| Cluster dedupe corrections | Re-ran with module-key extraction; final 17 clusters approved |

All routings + cluster decisions documented in [Main Plan.md](Main Plan.md) §9 Forks fifth entry.

#### Carry-forward contradictions (if any)
- **None.** Carry-forward §1-§3 architectural primitives stayed accurate. The QA-Inventory pass added new derivative documents (`Previously Implemented.md`, `Future State.md`) rather than contradicting the existing snapshot.

#### Files touched
- New: `Plans & Specs/Previously Implemented.md` (1089 entries, 262 KB).
- Modified: `Plans & Specs/Future State.md` (+58 lines: Section 3 + Section 4).
- Modified: `Plans & Specs/Implemented Work Log.md` (header convention + retroactive timestamps + this entry).
- Modified: `Plans & Specs/Main Plan.md` (+279 lines: scope expansions, 9 new batches, Phase 7 section, §6 sequencing rewrite, §9 fifth entry).
- No source code changes (read-only batch by spec).
- Out-of-tree artifacts: `C:/Users/jeffm/.claude/plans/qa-inventory-master.tsv`, `qa-inventory-walked.csv`, `qa-inventory-deduped-final.tsv`, `qa-inventory-dedupe-report.md`, `qa-inventory-phase4-summary.md`.

#### Commit(s)
- `76b1442` QA-Inventory Phase 6: route 1429 source-doc items to plan docs.
- `310672c` QA-Inventory Phase 6: Previously Implemented.md populated.
- (this entry's commit appended after Phase 7 close.)

#### Next action
- CLAUDE.md cleanup pass (post-close, in this session): remove the stale `OPEN BUG carried into next session` BaySickSynth drum woofy entry (fixed weeks ago per memory `project_drum_woofy_bug_fixed.md`); audit "Next Steps" section against the 9 new batches; surface anything else stale. Then move to QA-Md (MT Engine Debug-Build Investigation) per the Phase 1 sequence.

---

### 2026-05-09 21:14 PT — QA-Md — MT engine works in Debug; original "no-op" was DSP meter cap saturation

**Bucket:** Cross-cutting Infrastructure

#### Done
- Wrote QA-Md per-batch plan as a new file ([Batch Plans/glittery-tinkering-salamander.md](Batch Plans/glittery-tinkering-salamander.md)); updated [Main Plan.md](Main Plan.md) §5 QA-Md `**Plan file:**` pointer (Task 0, commit `087aebe`).
- Added `MtDiagnostic` counter namespace to [Source/Engine/RenderEngineFlags.h](../Source/Engine/RenderEngineFlags.h) — 11 atomic counters (block count / leaves submitted / child submits / watchdog fires / main-thread tasks / worker tasks / worker spin finds / worker sleep finds / worker idle sleeps / worker wakes) + `Snapshot` POD + `reset()` + `snapshot()` helpers, all gated on `gCaptureOn` for zero hot-path cost when off (Task 1, commit `d9ed843`).
- Wired counters into [Source/Engine/VibeThreadPool.cpp](../Source/Engine/VibeThreadPool.cpp): `runOneTask` child cascade (gChildSubmits), `runUntilOrTimeout` (gMainThreadTasks per pop + gWatchdogFires on timeout), `workerLoop` (gWorkerSpinFinds, gWorkerSleepFinds, gWorkerTasks, gWorkerIdleSleeps, gWorkerWakes) (Task 2, commit `7c4ba0b`).
- Wired counters into [Source/Engine/RenderGraphDispatcher.cpp](../Source/Engine/RenderGraphDispatcher.cpp): gBlockCount at `dispatchBlock` entry, gLeavesSubmitted per leaf in seed loop (Task 3, commit `6709fdb`).
- Added "Run MT Diagnostic (2s capture)" menu item (id 203) to the Mixer hamburger in [Source/Standalone/StandaloneEditor.cpp](../Source/Standalone/StandaloneEditor.cpp) — `showOkCancelBox` confirm → reset → set capture flag → 2s message-thread sleep → snapshot → `showMessageBoxAsync` with formatted body (build label / MT mode / per-counter values + percentages) (Task 4, commit `830f103`).
- Smoke-tested in Release with a small Layers session: blocks=750, total submits=9750, total tasks run=9750 (perfect match), watchdog=0, main-thread=8.0% / workers=92.0%, wakes/idle-sleeps ratio = 99.96%. Confirmed instrumentation works end-to-end (Task 5).
- Captured Debug + MT-on with same session: blocks=750, total submits=9752, total tasks run=9752 (perfect match), watchdog=0, main-thread=8.2% / workers=91.8%, wakes/idle-sleeps ratio = 99.96%. Essentially identical to Release pattern (Task 6 part 1).
- Captured Debug + MT-off control: all counters at 0 — confirmed branch decision works correctly under Debug (serial path skips MT branch entirely) (Task 6 part 2).
- Identified the four-quadrant DSP meter readings (small file SF + big file BF, Release + Debug, MT-on + MT-off) as the missing data needed to confirm the original "MT no-op" claim. User captured: Release MT-on/off SF=4/5, BF=33/55; Debug MT-on/off SF=33/33, BF=200/200 (Task 7 quadrant capture).
- Diagnosed Debug-BF 200/200 as DSP-meter-cap saturation (`juce::jlimit(0.f, 2.f, ...)` at [PluginProcessor.cpp:2969](../Source/PluginProcessor.cpp)), not MT engine failure. Both modes were sitting above the 200% display ceiling.
- Raised the meter cap from `2.f` to `10.f` as a quick definitive test; user re-captured Debug BF: MT-on=450%, MT-off=870%. **MT works in Debug at full efficiency.** Reduction ratio (~48%) essentially identical to Release (~40%).
- Decided: keep meter cap permanent at `10.f` for active development with HOLD-FOR-Phase-6-review marker (V1 release value deferred to QA-Audit decisions docket).
- Decided: keep MT diagnostic counters + Mixer hamburger menu item active during development; Phase 6 decides whether to wrap behind `#if BAYSICKDAW_MT_DIAGNOSTIC` for V1 release shipping builds.
- Captured five total decisions in QA-Audit "Pre-release decisions to revisit" docket (extending the three from earlier mid-batch chore commit `399bb59`): CL-288 AlertWindow API migration, CL-289 audit-security agent, CL-290 crash-report + symbol-server pipeline, CL-291 DSP meter cap V1 value, CL-292 MT diagnostic compile-flag gate.
- Updated [Main Plan.md](Main Plan.md) §9 Forks with eighth entry chronicling the QA-Md outcome.

#### Found along the way
- **The original QA-0a finding #9 ("MT engine no-op under Debug") was a misdiagnosis.** The MT engine has been functioning correctly in Debug all along; the symptom was DSP meter cap saturation. Both Debug-MT-on (450%) and Debug-MT-off (870%) sit above the original 200% cap, so the meter clipped both to "200%" and they appeared identical — looked like MT had no effect when in fact it cuts wall-clock by ~48% in Debug.
- **JUCE AlertWindow API mismatch in original plan.** The plan-as-written used `bool ok = showOkCancelBox(...)` (synchronous-style) and `juce::AlertWindow::showAsync(MessageBoxOptions...)` (newer builder API). Pre-edit codebase audit found both wrong: `showOkCancelBox` returns void and is async-callback only; the codebase uses `showMessageBoxAsync(...)` exclusively (~25 sites) with zero builder-API call sites. Refactored Task 4 to async-callback + `showMessageBoxAsync` to maintain consistency.
- **Older AlertWindow convenience wrappers vs newer builder API.** Side conversation surfaced the question of long-term migration (does the codebase want to standardise on `MessageBoxOptions` builder shape vs the current convenience wrappers?). Captured as `CL-288` Future State + Phase 6 review item.
- **Security audit agent need.** User raised the question of whether to create a `/audit-security` agent for known-CVE scanning + file-parser hardening + DLL safety + auto-updater chain audit. Captured as `CL-289` Future State + Phase 6 review item.
- **Crash-report + symbol-server pipeline.** Came up while explaining what ships on a user's computer (`.pdb` files map crash addresses back to source lines). Captured as `CL-290` Future State + Phase 6 review item; pairs with QA-Updater scope.
- **DSP meter UX call for V1 release.** Active dev value of `10.f` (1000%) is right for diagnosis but may surprise novice users seeing 870% readings. Three plausible release values (2 / 5 / 10). Captured as `CL-291` Future State + Phase 6 review item.
- **Phase 3 fix branch was unnecessary.** Plan pre-spec'd Branch A / B / C / D fixes for plausible failure modes. None applied — the engine wasn't broken. Phase 3 skipped entirely.
- **Counter pattern proves wake protocol is healthy in Debug.** Worker wakes (20078) ≈ worker idle sleeps (20086) — 99.96% ratio. Workers in Debug spin-find tasks 99.5% of the time (8951/8952). No starvation, no race, no broken signal protocol.

#### What was done about each finding

| Finding | Routing |
|---|---|
| QA-0a finding #9 misdiagnosed | Marked resolved-as-misdiagnosed in §9 Forks eighth entry; carry-forward §1 statement now true for both Release AND Debug. |
| AlertWindow API plan deviations | Documented inline in Task 4 commit message (`830f103`); larger AlertWindow migration captured as `CL-288`. |
| `/audit-security` agent question | Captured as Future State `CL-289` + folded into QA-Audit "Pre-release decisions to revisit" docket. |
| Crash-report + symbol-server pipeline | Captured as Future State `CL-290` + folded into QA-Audit docket. Pairs with QA-Updater scope. |
| DSP meter cap V1 release value | Captured as Future State `CL-291` + folded into QA-Audit docket. Active-dev value (`10.f`) preserved with HOLD-FOR-Phase-6-review comment. |
| MT diagnostic compile-flag gate | Captured as Future State `CL-292` + folded into QA-Audit docket. Active during dev; Phase 6 decides whether to wrap behind `#if BAYSICKDAW_MT_DIAGNOSTIC` for V1 release. |
| Phase 3 fix unneeded | Plan section preserved as historical record; no Branch A/B/C/D commits shipped. |

#### Carry-forward contradictions (if any)
- **QA-0a's 2026-05-07 entry framed MT as "production, default ON" for Release but no-op for Debug (finding #9, contradicting Carry-Forward §1's MT primitive listing).** After QA-Md: the "production, default ON" statement now holds for both Release AND Debug. The Debug "no-op" was a meter-display artifact (cap saturation at 200%), not an engine issue. The earlier QA-0a contradiction is **resolved** by this entry — finding #9 was a misdiagnosis caused by the meter cap saturating at 200% on both MT-on and MT-off Debug readings. (Carry-Forward §1 itself never made the production claim; it indexes the file:line primitives. The production framing came from the QA-0a entry, which is what this resolution updates.)

#### Files touched
- New: [Plans & Specs/Batch Plans/glittery-tinkering-salamander.md](Batch Plans/glittery-tinkering-salamander.md) (QA-Md per-batch plan).
- New: `~/.claude/projects/C--Users-jeffm-Documents-BaySickDAW/memory/feedback_every_commit_via_draft_commit.md` (memory entry capturing the per-commit `/draft-commit` rule).
- Modified (source): [Source/Engine/RenderEngineFlags.h](../Source/Engine/RenderEngineFlags.h), [Source/Engine/VibeThreadPool.cpp](../Source/Engine/VibeThreadPool.cpp), [Source/Engine/RenderGraphDispatcher.cpp](../Source/Engine/RenderGraphDispatcher.cpp), [Source/Standalone/StandaloneEditor.cpp](../Source/Standalone/StandaloneEditor.cpp), [Source/PluginProcessor.cpp](../Source/PluginProcessor.cpp) (meter cap raise + comment).
- Modified (plans): [Main Plan.md](Main Plan.md) (QA-Md plan-file pointer; QA-Audit "Pre-release decisions to revisit" docket additions; §9 Forks 7th entry with note pointer + new 8th entry), [Future State.md](Future State.md) (CL-288, CL-289, CL-290, CL-291, CL-292), [Implemented Work Log.md](Implemented Work Log.md) (this entry).
- Modified (memory): `~/.claude/projects/C--Users-jeffm-Documents-BaySickDAW/memory/MEMORY.md` (added pointer for the new feedback file).

#### Commit(s)
- `087aebe` QA-Md: open batch with plan file reference. (Task 0 chore: plan file + Main Plan pointer.)
- `d9ed843` QA-Md Step 1: add MtDiagnostic counters in RenderEngineFlags.h. (Task 1.)
- `7c4ba0b` QA-Md Step 2: wire MtDiagnostic counters into VibeThreadPool. (Task 2.)
- `6709fdb` QA-Md Step 3: wire MtDiagnostic counters into RenderGraphDispatcher. (Task 3.)
- `399bb59` QA-Md side findings: route three pre-release items into Future State + QA-Audit docket. (Mid-batch chore: CL-288 + CL-289 + CL-290.)
- `830f103` QA-Md Step 4: surface MtDiagnostic counters via Mixer hamburger. (Task 4.)
- `eef899c` QA-Md decision capture: raise DSP meter cap to 1000% + log Phase 2 outcome. (Decision-capture chore: meter cap `2.f` -> `10.f` + CL-291 + CL-292 + §9 Forks 8th entry.)
- (this entry's commit appended after `/review-batch` + close.)

#### Next action
- QA-A is the next batch in the §6 sequencing arrow (`QA-0a* → QA-0 → QA-Inventory*** → QA-Md** → QA-A → ...`). QA-A scope is the STYLE cluster — unified TitleBar component (STYLE-01..06). See [Main Plan.md](Main Plan.md) §5 QA-A entry. No carry-over from QA-Md needed (batch closes cleanly).

---

### 2026-05-10 06:00 PT — QA-A — Unified BaySickTitleBar + ribbon variable-width fix (STYLE cluster close)

**Bucket:** UI / L&F / Theming, Players, System Pages, Meta

#### Done
- New shared title-bar component family in [Source/Standalone/BaySickTitleBar.h](../Source/Standalone/BaySickTitleBar.h) + [.cpp](../Source/Standalone/BaySickTitleBar.cpp): `BaySickTitleBar` (32 px tall, 16 pt bold engine-name on the left, optional bloom halo, `getTrailingArea(reserve)` for caller-owned right-side widgets), `BaySickEngineLabel` (drop-in replacement for `juce::Label` where existing toolbar chrome stays — paints just the engine name with bloom, no background or divider), and `BaySickPresetButton` (Phase 6 — `juce::TextButton` subclass with path-drawn 5x4 px filled down-chevron matching `MetroArrowButton` geometry pixel-for-pixel; locks LAF to `VibeLAF::get()` in ctor / clears in dtor so engine-local LAF overrides cannot reskin it). Bloom math is path-stroke + crisp-fill: engine name -> `juce::GlyphArrangement` -> `juce::Path` -> stroke at 2.5 px / 30% accent alpha -> fill on top, symmetric halo around every glyph contour. Static `paintEngineName` helper shared between the three classes so bloom stays in lock-step. Bloom default = `true`.
- Adopted `BaySickTitleBar` across all seven player engine editors: `VibePlayerEditor` (PoC, Phase 2), `HarmlessEditor`, `BaySickSynthEditor`, `BaySickBassEditor`, `BaySickNAMIREditor` (Phase 3), `BaySickVocalEditor` + `BaySickAlignEditor` + `BaySickPitchEditor` (Phase 4.1 — `BaySickVocalsPanel` inside `BaySickVocalEditor.cpp` adopts the full `BaySickTitleBar`; the two sibling sub-pages adopt `BaySickEngineLabel` to preserve their existing toolbar chrome), and `BaySickPedalsEditor` with the pedalboard preset button migrating from InstPage parent chrome into the title bar's trailing area (Phase 4.2). Engine accent table locked: Harmless `HarmlessLAF::kAccent`, BaySickPlayer `0xFFD4A017`, BaySickSynth `BaySickSynthLAF::kGreen`, BaySickBass `BaySickBassLAF::kGreen`, BaySickNAM/IR `0xFFE0303F` (Mesa red), BaySickVocal cluster teal `0xFF0FAFA5`, BaySickPedals navy `0xFF1C3A8A` (Inst-tab active color).
- Extended `AriaControlPanel::Binding` with optional `engineName` + `accentColor` fields (Phase 4.3) so the three sfizz-driven kit-artwork engines (BaySickGuitars, BaySickBasses, BaySickRustyDrums) can render their own title bars through the shared kit-artwork panel without duplicating the component. `AriaControlPanel` now manages a private `std::unique_ptr<BaySickTitleBar>` lifecycle keyed off whether the binding's `engineName` is non-empty; backward-compatible (empty `engineName` -> null bar -> identical render to pre-Phase-4.3).
- Wired the AriaControlPanel plumbing through three real callers: `InstPage::rebuildPlayerPanel` sets navy bindings for BaySickGuitars / BaySickBasses (Phase 4.4); `BaySickRustyDrumsPage` sets a Drums-tab-red `0xFFCC2222` binding (Phase 4.5).
- InstPage chrome cleanup (Phase 4.4): `kHeaderRowH = 36` constant, the dark-fill paint block, and the `mPedalsHeaderTitle` + `mPedalsPresetBtn` members all deleted. `mPedalsEditor->onPedalboardPresetMenu` callback hook wired in InstPage's ctor via `dynamic_cast<BaySickPedalsEditor*>`; the trailing-area button migrated in Phase 4.2 stops being a no-op. `showPedalboardPresetMenu`'s popup anchor switched from the deleted button to the editor (later refined in Phase 6 to anchor on the preset button itself via a new `getPedalboardPresetButton()` accessor — the editor-root anchor caused the popup to open at the app's top-left). Closes the "extra black bar above BaySickNAM/IR / BaySickPedals" issue Jeff flagged during Phase 3.4.
- BaySickRustyDrums tab-strip overlay positioner (Phase 4.5): `AriaControlPanel` tab strip rewired from "reserved band above kit artwork" to "draggable overlay anchored in NATIVE artwork coords" via new `mTabStripNativeOffset` (`juce::Point<int>`). Kit artwork stays centre-anchored (top-anchor regression caught mid-phase — pinned the kit flush against the title bar). Debug-only Y-axis-locked positioner ran the bake; final value `{0, -11}` baked as the literal initializer in `AriaControlPanel.h`. Positioner scaffolding (`DraggableTabButton` subclass, "Save Pos" trailing button, `debugSaveTabStripPositionToFile()` helper) stripped before commit — shipping binary carries only the literal offset.
- RibbonTabBar variable-width layout (Phase 5, closes STYLE-01): replaced the equal-share `totalW / kNumSlots` formula with a constraint-based algorithm — clamp desired width per slot to `[min, max]`; if sum-of-desireds <= `totalW`, distribute slack equally; else shrink proportionally to each slot's "shrink room" so no slot crosses its floor until they all hit floor together. Per-slot floors `kMinFixed = 60` (Mixer / Effects / Builder / Piano Roll) and `kMinVariable = 80` (engine-name slots); cap `kMaxSingleLine = 220` above which slots wrap to 2 lines. Paint site swapped to `drawFittedText(name, textR, centredLeft, /*maxLines=*/2, /*minScaleFactor=*/0.75f)`; new file-static `splitCamelCase` helper injects `\n` at the capital letter closest to mid-string for no-space brand names ("BaySickRustyDrums" -> "BaySick\nRustyDrums") so JUCE's text engine has a hard break point. Natural-width calc always uses BOLD font weight regardless of active state so a slot's allocated width doesn't twitch when the user clicks between slots. `kTabH` 30 -> 40 (fills parent transport bar's full vertical height). Effects + Builder hardcoded sub-page badge counters (returning 2 and 3) deleted — pure visual noise that ate ~20 px of slot width; the dropdown arrow already conveys "this slot has a sub-menu."
- Phase 6 cross-engine consistency sweep surfaced four real inconsistencies and drove a real Phase 6 commit (the plan's "verification-only / skip the commit if all consistent" framing turned out wrong — see findings). Created the `BaySickPresetButton` shared component; swapped 5 engine editors (Harmless / VibePlayer / BaySickSynth / BaySickBass / BaySickPedals) from `juce::TextButton mPresetBtn { "Preset v" }` (literal-'v' fake chevron) to `BaySickPresetButton mPresetBtn { "Preset" }`; fixed the InstPage popup-anchor bug via the new `getPedalboardPresetButton()` accessor; deleted VibePlayer's leftover `?` help button (spec drift); unified preset button width to 88 px across all five engines (Harmless 86 -> 88, VibePlayer 110 -> 88, others already 88). Per-engine onClick / showPresetMenu / withTargetComponent wiring intentionally untouched — visual-only unification per Jeff's "transferring look and feel only, since the preset boxes all have different setups" guardrail.
- Defensive HarmlessLAF zero-bounds guard shipped during Phase 3.1 (commit `679af33`, separate from the BaySickTitleBar refactor itself): early-return at top of the `LinearVertical` branch in `HarmlessLAF::drawLinearSlider` when `width <= 0 || height <= 0`. Stops a NaN-coord Direct2D `coordsToRectangle` assert that surfaced after Phase 3.1's `kHdrH 36 -> 32` shift in HarmlessEditor body layout. The upstream "why is the slider 0 px in the first place" question was deferred to Phase 6 / QA-Audit per D9 — this commit is the symptom-side fix only.
- Established a new `Plans & Specs/Running Notes/` subfolder with `<silly-name>.md` per-batch convention paired with `Batch Plans/`. §0 approved-subfolders list updated. Three new memory rules locked during the batch: `feedback_plan_mirror_one_way.md` (mirror once on ExitPlanMode + delete home-dir copy), `feedback_match_jeff_text_casing.md` (engine names in brand mixed-case, never up-case unilaterally even when legacy source did), `feedback_draft_doc_running_notes_every_checkpoint.md` (running-notes drafter fires at every checkpoint, not "at the end"; output goes to `Plans & Specs/Running Notes/<silly-name>.md`).
- Closes STYLE-01 (Phase 5), STYLE-02 (Phase 1 component + Phase 3 sweep), STYLE-03 (Phase 4.1 — BaySickVocal cluster), STYLE-04 (Phase 4.2 — title text corrected from "BaySickGuitars" to "BaySickPedals" per finding #27), STYLE-05 (Phase 3.4 — BaySickNAM/IR), STYLE-06 (Phases 3.2 + 3.3 — BaySickSynth + BaySickBass preset dropdown moved L -> R).

#### Found along the way
- **#22** Bloom math iterated three times. Original size-delta-with-offset approach (17 pt underlay at (-1,-1), 16 pt overlay at (0,0)) read as a directional shadow because both texts shared the left-aligned edge — larger underlay only extended right + top + bottom, not left. Removing the offset didn't fix it. Final fix is path-stroke halo (D3): `GlyphArrangement` -> `Path` -> 2.5 px stroke at 30% accent alpha -> crisp fill on top. Symmetric halo around every glyph contour.
- **#23** SettableTooltipClient detour during Phase 3.4. Tried extending `BaySickTitleBar` to inherit from `juce::SettableTooltipClient` to preserve BaySickNAM/IR's "Neural Amp Modeler + IR cabinet" hover tooltip; rebuild then crashed in `HarmlessLAF::drawLinearSlider`. Reverted SettableTooltipClient base; tooltip dropped permanently per D8. The Harmless crash turned out to be a separate latent bug, not caused by the SettableTooltipClient change (see #25).
- **#24** Plan-mirror-rule duplicate. Plan-mode forces the planning file to `~/.claude/plans/<silly-name>.md` for its UI; canonical project location is `Plans & Specs/Batch Plans/<silly-name>.md`. After ExitPlanMode mirror, I cp'd the canonical version back to home-dir after a scope edit "to keep them in sync" — Jeff caught the duplicate.
- **#25** HarmlessLAF latent zero-bounds bug surfaced by Phase 3.1's `kHdrH 36 -> 32` shift. When a vertical slider has zero-sized bounds (not yet laid out, or computed to 0), `fh = (float)height = 0` makes the `norm` calc divide by zero, NaN propagates through `thumbY` and the cap rect into Direct2D's `coordsToRectangle` clip-list assert. Latent — Phase 3.1's body-layout shift just happened to surface it on Harmless engine pick.
- **#26** Engine title casing — Steps 2 / 3 / 4 / 5 shipped with engine names in ALL CAPS ("HARMLESS" / "BAYSICKPLAYER" / "BAYSICKSYNTH" / "BAYSICKBASS") because legacy paint code had it that way. Jeff caught the unilateral up-casing during Phase 3.4.
- **#27** BaySickPedals title text correction — original plan had Phase 4.2's title text as "BaySickGuitars" per STYLE-04's literal phrasing. Pedals and Guitars are distinct engines (the latter is sfizz-driven and shares AriaControlPanel; the former is the FX rack).
- **#28** Phase 4 scope expansion — BaySickGuitars / BaySickBasses / BaySickRustyDrums were missing from the original plan. They share `AriaControlPanel` for kit-artwork rendering and have no BaySickDAW-style chrome of their own.
- **#29** InstPage parent chrome black bar — after BaySickNAM/IR title bar landed in Phase 3.4, Jeff noticed an "extra black bar" between the title bar and the page-level sub-tab buttons above. Diagnosis: InstPage's own `kHeaderRowH = 36` chrome strip, originally there to host the pedalboard preset button + engine title text.
- **#30** InstPage-vs-VoxPage chrome distinction (refines #29). The "extra black bar" is **only** on the Inst page, not the Vox page. VoxPage is a thin wrapper with no parent chrome. InstPage carries its own chrome strip — Phase 4.4 deletes it across all Inst-page sources; VoxPage requires no work.
- **#31** Top-anchor regression in Phase 4.5. My initial implementation of `AriaControlPanel::computePanelDrawArea` top-anchored the kit artwork to make room for the tab strip. Combined with `tabBarH = 0` (since the strip moved to overlay rather than reserved-band layout), this pinned the kit flush against the title bar with no breathing room. Jeff caught it ("you slid the player all the way up to the top beneath the title bar so there is no where to put the buttons").
- **#32** First-pass `drawFittedText` shrink-to-fit was insufficient for STYLE-01. Step 5.2.1's initial fix swapped the truncating `drawText` for `drawFittedText` with `maxLines=1` + `minScaleFactor=0.75f`. Visual verification still showed truncation: even with auto-shrink to 9 pt floor, "BaySickRustyDrums" couldn't fit because the slot itself was too narrow (192 px equal-share at 1920-wide window, minus ~58 px overhead = ~134 px text area; "BaySickRustyDrums" at 12 pt bold needs ~150 px). The slot allocation itself had to be rewritten as variable-width.
- **#33** Active-vs-inactive font weight changes natural width. Active tabs use bold; inactive use regular — same string measures wider in bold. If natural width is computed using current state, slot allocation twitches when the user clicks between slots. Solution: always use bold-font width for the natural-width calc.
- **#34** Effects + Builder hardcoded sub-page badge counters (returning 2 and 3) were pure visual noise eating ~20 px of slot width per slot for a signal the dropdown arrow already conveys.
- **#35** Phase 6's "verification-only" framing was wrong. Plan said "if all consistent, skip the commit." Cross-engine visual sweep surfaced four real inconsistencies that needed real code fixes, not zero. "Verification-only" phases should always budget for the possibility that the sweep finds work.
- **#36** Engine LAF overrides leak into shared components. First-pass propagation of `BaySickPresetButton` rendered black on the four LAF-overriding engines (HarmlessLAF / BaySickSynthLAF / BaySickBassLAF / VibePlayerLAF) because each engine's local LAF propagates to its child components. Pedals already rendered grey because InstPage uses the global VibeLAF.
- **#37** InstPage anchored its preset popup to the editor root. `withTargetComponent (mPedalsEditor.get())` -> editor root component, which JUCE pins at the editor's top-left = the app's top-left. Menu opened at the app's top-left. Same-pattern bug exists nowhere else (the four other engines anchor `withTargetComponent (mPresetBtn)` directly because the menu logic lives inside the editor itself).
- **#38** VibePlayer `?` help button was a leftover Jeff never asked for (popped a hardcoded AlertWindow about how to use the player). Spec drift.
- **#39** VibePlayer/* -> BaySickPlayer/* rename. Internal source file/class names still use `VibePlayer*` (per CLAUDE.md "internal source is still `VibePlayer*`; class / file renames deferred"); user-facing brand is `BaySickPlayer`. Jeff flagged commit-message hygiene mid-batch — future commits use "BaySickPlayer" in body text, file paths in diffs unavoidably show `Source/VibePlayer/...` until the rename lands.
- **#40** Piano Roll deep-link button crash re-sighted during Phase 4.4 verification. Stack: `StandaloneEditor::showPageForTab` line 4135's `<lambda_14>::operator()(int i)`. Same crash family as findings #13 + #14 from QA-0a / QA-0 (captured-raw `InstPage*` in lambda gets freed during engine swap or project reload). Already routed to QA-E per §9 Forks 3rd entry "QA-0 close routings". The crash path is **untouched** by the Phase 4.4 InstPage chrome refactor — captured here purely for traceability.
- **#41** Workflow gap — `/read-doc` underused. Used direct `Read` on Plans & Specs docs throughout the early session instead of the doc-reader agent.
- **#42** Workflow gap — running-notes not dispatched. Eleven QA-A commits with zero `/draft-doc running-notes` dispatches before Jeff flagged the "flying blind" pattern. Caught back-half of the batch.

#### What was done about each finding

| Finding | Routing |
|---|---|
| #22 (bloom math) | Resolved in commit `9c915c2`. Path-stroke halo replaces size-delta-with-offset. |
| #23 (SettableTooltipClient) | Reverted; D8 locked tooltip drop. The Harmless crash that surfaced during the rollback was a separate bug — see #25. |
| #24 (plan-mirror duplicate) | Memory rule `feedback_plan_mirror_one_way.md` locked; mirror once + delete home-dir copy. |
| #25 (HarmlessLAF zero-bounds) | Defensive guard shipped in commit `679af33`. Upstream "why is the slider 0 px in the first place" deferred to Phase 6 / QA-Audit per D9. |
| #26 (engine title casing) | Memory rule `feedback_match_jeff_text_casing.md` locked. Sweep commit `0c4431d` corrected source files + plan doc + per-engine accent table. |
| #27 (BaySickPedals vs BaySickGuitars) | Plan doc corrected in commit `0c4431d`; D10 locks the title-text convention. Source applied in Phase 4.2 commit `652998b`. |
| #28 (Phase 4 scope expansion) | Plan doc expanded in commit `d529602`; D6 locks Phase 4.3 / 4.4 / 4.5 sub-phases. Executed across Phases 4.3 / 4.4 / 4.5. |
| #29 + #30 (InstPage chrome strip) | Resolved in Phase 4.4 commit `1d0304d`. `kHeaderRowH = 36` constant + paint block + chrome members all deleted. VoxPage required no work. |
| #31 (top-anchor regression) | Reverted to centre-anchor in Phase 4.5 commit `e86a887`; D16 locks overlay model (not reserved-band). |
| #32 (drawFittedText alone insufficient) | Drove the variable-width + wrap design (D20-D24) that became the actual STYLE-01 fix in Phase 5 commit `9f30cfb`. |
| #33 (font-weight width twitch) | Natural-width calc always uses bold-font width regardless of active state; resolved in Phase 5 commit. |
| #34 (Effects + Builder badge noise) | `getBadgeCount` for Effects / Builder dropped in Phase 5 commit; D25 locks the rule. |
| #35 ("verification-only" wrong framing) | Phase 6 produced a real commit (`4689f0f`) covering the four inconsistencies the sweep surfaced. |
| #36 (engine LAF leaks into shared components) | `BaySickPresetButton` ctor pins LAF to `VibeLAF::get()`; D28 locks the rule (refines D1's "no LAF coupling"). |
| #37 (InstPage popup anchor) | New `BaySickPedalsEditor::getPedalboardPresetButton()` accessor; InstPage anchors to that. Resolved in Phase 6 commit. |
| #38 (VibePlayer `?` help button) | Deleted in Phase 6 commit; D30 locks the spec-drift removal. |
| #39 (VibePlayer/* -> BaySickPlayer/* rename) | **Routed at QA-A close per Rule 3** — dedicated batch **QA-PlayerRename** inserted into §5 Phase 1 immediately after QA-A; §6 sequencing arrow updated with new `******` footnote. Internal source rename + class renames; user-facing strings + commit-message hygiene already locked this batch. See ninth Forks entry. |
| #40 (Piano Roll deep-link crash) | Already routed to QA-E per §9 Forks 3rd entry "QA-0 close routings". Re-sighted during this batch's verification — no new action; captured for traceability. |
| #41 (`/read-doc` underuse) | Behavioural rule reinforced; no new memory entry — folds into existing agent-orchestration discipline in Main Plan §0. |
| #42 (running-notes not dispatched) | Memory rule `feedback_draft_doc_running_notes_every_checkpoint.md` locked (mid-batch). Retrospective backfill compiled the Phase 1-3 history; Phases 3-6 dispatched at every close checkpoint. |

#### Carry-forward contradictions (if any)
- None. Carry-forward §1-§3 architectural primitives stayed accurate. QA-A's scope was UI / theming + existing-component refactor; no architectural primitives changed shape. The new `BaySickTitleBar` family is additive and the InstPage chrome removal + RibbonTabBar variable-width fix are local layout changes within already-indexed components.

#### Files touched
- New: [Source/Standalone/BaySickTitleBar.h](../Source/Standalone/BaySickTitleBar.h), [Source/Standalone/BaySickTitleBar.cpp](../Source/Standalone/BaySickTitleBar.cpp).
- New: [Plans & Specs/Running Notes/twinkling-herding-twilight.md](Running Notes/twinkling-herding-twilight.md) (running-notes file paired with this batch).
- New: [Plans & Specs/Batch Plans/twinkling-herding-twilight.md](Batch Plans/twinkling-herding-twilight.md) (QA-A per-batch plan, mirrored from `~/.claude/plans/`).
- Modified (source — engine editors): [Source/VibePlayer/VibePlayerEditor.h](../Source/VibePlayer/VibePlayerEditor.h) + [.cpp](../Source/VibePlayer/VibePlayerEditor.cpp), [Source/Harmless/HarmlessEditor.h](../Source/Harmless/HarmlessEditor.h) + [.cpp](../Source/Harmless/HarmlessEditor.cpp), [Source/BaySickSynth/BaySickSynthEditor.h](../Source/BaySickSynth/BaySickSynthEditor.h) + [.cpp](../Source/BaySickSynth/BaySickSynthEditor.cpp), [Source/BaySickBass/BaySickBassEditor.h](../Source/BaySickBass/BaySickBassEditor.h) + [.cpp](../Source/BaySickBass/BaySickBassEditor.cpp), [Source/BaySickNAMIR/BaySickNAMIREditor.h](../Source/BaySickNAMIR/BaySickNAMIREditor.h) + [.cpp](../Source/BaySickNAMIR/BaySickNAMIREditor.cpp), [Source/BaySickPedals/BaySickPedalsEditor.h](../Source/BaySickPedals/BaySickPedalsEditor.h) + [.cpp](../Source/BaySickPedals/BaySickPedalsEditor.cpp), [Source/BaySickVocal/BaySickVocalEditor.cpp](../Source/BaySickVocal/BaySickVocalEditor.cpp), [Source/BaySickVocal/BaySickAlignEditor.cpp](../Source/BaySickVocal/BaySickAlignEditor.cpp), [Source/BaySickVocal/BaySickPitchEditor.cpp](../Source/BaySickVocal/BaySickPitchEditor.cpp).
- Modified (source — pages + shared): [Source/Standalone/AriaControlPanel.h](../Source/Standalone/AriaControlPanel.h) + [.cpp](../Source/Standalone/AriaControlPanel.cpp) (Binding extension + title-bar lifecycle + tab-strip overlay positioner), [Source/Standalone/BaySickRustyDrumsPage.cpp](../Source/Standalone/BaySickRustyDrumsPage.cpp) (title-bar binding + page bg fuse), [Source/Inst/InstPage.h](../Source/Inst/InstPage.h) + [.cpp](../Source/Inst/InstPage.cpp) (chrome strip removal + per-source-mode binding wiring + popup-anchor fix), [Source/Standalone/RibbonTabBar.h](../Source/Standalone/RibbonTabBar.h) + [.cpp](../Source/Standalone/RibbonTabBar.cpp) (variable-width layout + 2-line wrap + camelCase split helper + kTabH 30->40 + Effects/Builder badge drop).
- Modified (source — defensive guard): [Source/Harmless/HarmlessLAF.h](../Source/Harmless/HarmlessLAF.h) (zero-bounds early-return in `LinearVertical` branch of `drawLinearSlider`).
- Modified (build): [CMakeLists.txt](../CMakeLists.txt) (BaySickTitleBar.cpp added to Standalone source cluster).
- Modified (plans): [Main Plan.md](Main Plan.md) (QA-A `**Plan file:**` pointer; §0 approved-subfolders list updated for `Running Notes/`), [Implemented Work Log.md](Implemented Work Log.md) (this entry).
- New (memory): `~/.claude/projects/C--Users-jeffm-Documents-BaySickDAW/memory/feedback_plan_mirror_one_way.md`, `feedback_match_jeff_text_casing.md`, `feedback_draft_doc_running_notes_every_checkpoint.md`, all indexed in `MEMORY.md`.

#### Commit(s)
- `314fe37` QA-A: open batch with plan file reference. (Task 0 chore.)
- `d9a95be` QA-A Step 1: scaffold BaySickTitleBar shared component (skeleton). (Phase 1.)
- `9c915c2` QA-A Step 1b: BaySickTitleBar bloom uses path-stroke halo, on by default. (Phase 1 corrective.)
- `8fd4584` QA-A Step 2: BaySickPlayer editor adopts BaySickTitleBar (PoC). (Phase 2.)
- `9882630` QA-A Step 3: Harmless editor adopts BaySickTitleBar. (Phase 3.1.)
- `1face93` QA-A Step 4: BaySickSynth editor adopts BaySickTitleBar. (Phase 3.2 — closes STYLE-06 for Synth.)
- `4a02615` QA-A Step 5: BaySickBass editor adopts BaySickTitleBar. (Phase 3.3 — closes STYLE-06 for Bass.)
- `d529602` QA-A: expand plan scope to cover BaySickGuitars / BaySickBasses / BaySickRustyDrums. (Plan-doc.)
- `0c4431d` QA-A: correct engine title casing to brand mixed-case. (Plan-doc + source casing sweep.)
- `c900f55` QA-A: establish Running Notes subfolder + seed twinkling-herding-twilight.
- `679af33` QA-A: guard HarmlessLAF::drawLinearSlider against zero-sized bounds. (Defensive zero-bounds guard.)
- `27a10bd` QA-A Step 6: BaySickNAM/IR editor adopts BaySickTitleBar. (Phase 3.4 — closes STYLE-05.)
- `8c5924c` QA-A: append running-notes checkpoint after Phase 3 close.
- `1a31aba` QA-A Step 7: BaySickVocal cluster adopts BaySickTitleBar styling. (Phase 4.1 — closes STYLE-03; introduces `BaySickEngineLabel` + `paintEngineName` helper.)
- `40c8be8` QA-A: append running-notes checkpoint after Phase 4.1 close.
- `652998b` QA-A Step 8: BaySickPedals editor adopts BaySickTitleBar. (Phase 4.2 — closes STYLE-04 with corrected "BaySickPedals" title text.)
- `4fbe40d` QA-A: append running-notes checkpoint after Phase 4.2 close.
- `21394d5` QA-A Step 9: AriaControlPanel hosts optional BaySickTitleBar. (Phase 4.3 — plumbing-only.)
- `662effb` QA-A: append running-notes checkpoint after Phase 4.3 close.
- `1d0304d` QA-A Step 10: InstPage drops chrome strip, wires per-source title bars. (Phase 4.4.)
- `eea29e3` QA-A: append running-notes checkpoint after Phase 4.4 close.
- `e86a887` QA-A Step 11: BaySickRustyDrums adopts BaySickTitleBar + tab strip overlay. (Phase 4.5 — `mTabStripNativeOffset = {0, -11}` baked.)
- `6f5ed45` QA-A: append running-notes checkpoint after Phase 4.5 close.
- `9f30cfb` QA-A Step 12: ribbon tabs adopt variable-width layout + two-line wrap. (Phase 5 — closes STYLE-01.)
- `06eda10` QA-A: append running-notes checkpoint after Phase 5 close.
- `4689f0f` QA-A Step 13: unify engine preset buttons via BaySickPresetButton. (Phase 6 — cross-engine sweep + four real fixes.)
- `61173f8` QA-A: append running-notes checkpoint after Phase 6 close.
- (this entry's commit appended after `/review-batch` + close.)

#### Next action
- Per §6 sequencing arrow, QA-B is the next batch (`... QA-Md** → QA-A → QA-B → ...`). See [Main Plan.md](Main Plan.md) §5 QA-B entry for scope. Side finding #39 (VibePlayer/* -> BaySickPlayer/* rename) is parked on QA-Cleanup-1 in Phase 6 unless the parent elects a dedicated batch for it.

---
