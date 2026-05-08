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

**Entry template** (paste at the bottom and fill in):

```
## YYYY-MM-DD — <Batch ID> — <One-line summary>

### Done
- [list of things completed in this session]

### Found along the way
- [surprises, gotchas, things that contradicted the plan or carry-forward]

### What was done about each finding
- [actions taken on each finding above; or "logged for follow-up batch X"]

### Carry-forward contradictions (if any)
- [if §X of carry-forward turned out wrong, record here: "§X said Y, verified Z"]

### Carry-over to next session (if not closing batch)
- See `## Carry-Over` block at bottom of the active per-batch plan.

### Files touched
- [list of files modified in this session]

### Commit(s)
- [hash + one-line message]
```

---

## Table of contents

(Append entries below this line. New entries auto-append to the bottom.)

---

## 2026-05-07 — Triage — Plan + carry-forward + implemented-work docs created

### Done
- Read the unified backlog end to end (64 entries: 59 active + 4 optional + 1 never).
- Read the MT batch map for resolved-en-route context.
- Spawned 3 parallel Explore agents to verify architectural primitives (MT engine, snapshot/lifecycle patterns, mixer/page lifecycle).
- Sanity-checked specific items against current code (DSP-10, BUILD-06, MIX-01, UI-01, DSP-09, hamburger MT toggle wiring).
- Diagnostic discovery: WAV/MP3 drop on Builder grid is silent under MT (regression), plays correctly under serial. Confirms DSP-12 channel-id conflict is the cause and elevates it to top-priority QA-0.
- Resolved 7 design calls with Jeff (DSP-12 fix shape, DSP-09 target behavior, FILE-02 routing/timing, NAV-03/04 specs, NAV-05 remove, MIX-03 = symptom of MIX-02).
- Wrote main plan ([Main Plan.md](Main Plan.md)) — 20 batches across 6 phases (15 bug-fix + 5 cleanup audit).
- Wrote carry-forward reference ([Carry-Forward Reference.md](Carry-Forward Reference.md)) — frozen snapshot of architectural primitives, file:line index, decisions, per-item status, patterns to reuse.
- Created this implemented-work doc (initial state, no batches executed yet).

### Found along the way
- The hamburger MT toggle wiring exists ([StandaloneEditor.cpp:4450-4497](../Source/Standalone/StandaloneEditor.cpp)) — Explore agent missed it on first sweep. Self-verified.
- MIX-03 is not a direct bug — Jeff clarified it's a symptom of MIX-02. The auto-spawn-clips-strip behavior at recording time is correct; the bug is that on save→reload the Vox strip disappears (MIX-02) and the orphan recording on the Builder grid becomes a clips strip on next load.
- DSP-12 is not just an architectural design issue — it's the active cause of the WAV-drop-on-Builder regression. ClipPageTask wins the channel-id registration race and silences AudioInsertTask. Validation suite missed it because Vox/Inst recording tests use VoxStripTask/InstStripTask paths (different channel id ranges, no conflict).
- The dead `Properties...` menu entry at [BuilderPage.cpp:2561](../Source/Standalone/BuilderPage.cpp) has no `case 7` handler — it's added unconditionally for all block types and does nothing.
- Several agent reports overstated "FIXED" judgments (UI-01/UI-02 in particular). Jeff confirmed they're still open. Agents conflated "outer right-click handler is gated correctly" with "JUCE PopupMenu rejects right-click as item activation" — different code paths.

### What was done about each finding
- DSP-12 elevated to QA-0 (first batch, top priority).
- MIX-03 folded into MIX-02 in the plan; not tracked as separate work.
- Dead Properties cleanup added to QA-E scope (touches the same Properties popup as FILE-02).
- Plan structure adjusted to mark Phase 6 (cleanup audit) as its own dedicated phase.
- Three-doc system + carry-over discipline added to plan §0 to prevent context loss across sessions.

### Carry-forward contradictions (if any)
- None. Carry-forward was authored from this same triage session, so it reflects current findings.

### Files touched
- `Main Plan.md` (created)
- `Carry-Forward Reference.md` (created)
- `Implemented Work Log.md` (created — this file)
- No source code changes.

### Commit(s)
- None. Read-only triage session.

### Next action
- Start QA-0 (MT Composite RenderTask). Read carry-forward §1-3, then read the actual `AudioInsertTask::run()` and `ClipPageTask::run()` bodies, then write the QA-0 per-batch implementation plan as a new file under `C:\Users\jeffm\Documents\BaySickDAW\Plans & Specs\Batch Plans\<silly-name>.md`.

---

## 2026-05-07 — QA-0a — Debug build workflow setup

### Done
- Annotated main plan with the QA-0a fork using the hybrid convention: §0 Rule updated, §5/§6/§7 inline back-refs, §9 master log appended.
- do_build.bat now builds Release + Debug. Build log writes per-config exit codes.
- Window title appends `[DEBUG]` under `#ifdef JUCE_DEBUG` in `StandaloneEditor::refreshWindowTitle`.
- CMakeLists.txt sets `JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING=1` to silence benign AAX paramID asserts (BaySickDAW is standalone-only).
- CLAUDE.md Build System section updated with the new dual-config workflow + standing rule "verify in Debug first, then Release" + cross-stream caveats (settings.xml shared, ASIO exclusivity, MT engine no-op in Debug).
- Cold-start triage of every assert that fired during Debug startup + project load - 13 findings cataloged. Real source-side fixes for 4 of them; 7 vendored-JUCE suppressions for benign warnings; 2 logged-and-deferred for dedicated batches.

### Found along the way
- **Debug build was BROKEN before this session.** Two C3493 "cannot implicitly capture" errors in lambdas (PluginProcessor.cpp:2883 `kPeakNegInf`, SharedUI.h:733 `kMidiFirstId`) that Release silently tolerates via constant folding. Latent issue surfaced the moment we tried to actually use the Debug exe. Every "F5" before Tasks 5/6 launched a stale exe from the FIRST do_build.bat run. Fixed by adding explicit captures.
- **Voice ctors propagate sampleRate=0 into ADSR + StateVariableTPTFilter at construction.** All four engine synth ctors (VibeSynth, BaySickSynth, Harmless, main VibeSynthProcessor synth) call addVoice() before any sample rate is set. JUCE's Synthesiser::addVoice propagates the synth's current sample rate (0) to each new voice's ADSR + filter. Real values arrive later via prepare/prepareToPlay before any audio. Fixed at source: setCurrentPlaybackSampleRate(44100) before the addVoice loop in all four ctors. Also kept defensive belts in vendored JUCE in case a future engine misses this.
- **CMake JUCE+VS multi-config gotcha** prevented per-config ICON_BIG gating. Reverted the conditional - both exes ship with the BaySickDAW branded icon. User accepted "differentiation via window title only".
- **Project-wide em-dash sweep needed.** Initial KeyBindings.cpp fix surfaced ~150 more files with em-dashes. Bulk-replaced all U+2014 em-dashes with ASCII hyphens across every .cpp + .h under Source/ (217 files). Fixes both the JUCE ASCII-validity assert and the long-standing box-glyph render bug. Box-drawing decorations (U+2500 in `// ──` comments) untouched - they're not in string literals.
- **MT engine no-op under Debug** (finding #9). DSP meter shows identical readings whether MT is on or off; toggle persists to settings.xml correctly but the dispatcher isn't actually distributing work to threads. Real Debug-only bug; Release MT works fine. Logged for dedicated batch.
- **Dangling InstPage* in tab-click lambda** (finding #13). Confirmed via `0xDDDDDDDDDDDDDDDD` debug-fill marker. Use-after-free crash on tab click after engine swap or project reload. `[this, ip, syncPagePresetMenu, labels]` capture stores raw InstPage* that gets freed elsewhere. Real fix: SafePointer<InstPage> or per-click index lookup. Logged for dedicated batch.
- **MenuBarModel listener-dangle on closeAllDynamicTabs** (finding #8). MenuBarModel destroyed before MenuBarComponent during dynamic-tab teardown cascades. Real but lower-severity (JUCE's removeListener is set-safe no-op on miss). Logged for dedicated batch. closeAllDynamicTabs design itself is correct; this is a downstream wiring detail.

### What was done about each finding
- All 13 findings cataloged with status (fixed at source / suppressed in vendored JUCE / logged for dedicated batch). See commit messages on `b34c54d` and `bd67fdf` for the per-finding rationale.
- Three deferred-real-bug findings (#8 MenuBar lifecycle, #9 MT no-op in Debug, #13 InstPage use-after-free) explicitly NOT fixed in QA-0a. They surfaced via QA-0a's diagnostic infrastructure and become candidate work items for future batches.

### Carry-forward contradictions (if any)
- **Carry-forward §1 says MT engine is "production, default ON".** That's true for Release. **For Debug builds it's a no-op** - finding #9. Carry-forward stays as the historical 2026-05-07 snapshot; this entry records the contradiction.

### Files touched
- Modified workflow: do_build.bat, CMakeLists.txt, CLAUDE.md.
- Modified targeted source: PluginProcessor.cpp (lambda capture + voice ctor sample rate), Standalone/StandaloneEditor.cpp (window title), Standalone/SharedUI.h (lambda capture), Standalone/KeyBindings.cpp (em-dash literals - 19 sites), VibePlayer/VibePlayerDSP.cpp + BaySickSynth/BaySickSynthDSP.cpp + Harmless/HarmlessSynth.cpp (voice ctor sample rate).
- Modified em-dash sweep: 217 files under Source/.
- Modified vendored JUCE: 7 files in juce/modules/.
- Modified plans: Main Plan.md (§0 rule update + §5/6/7 inline back-refs + §9 master log appended; convention rule for "Plan file:" header lines), Implemented Work Log.md (this entry).

### Commit(s)
- `b34c54d` QA-0a: Debug build workflow setup + source-side fixes that the new Debug surfaced.
- `a472a44` QA-0a: project-wide em-dash sweep across Source/ (217 files).
- `bd67fdf` QA-0a: vendored JUCE assertion suppressions for benign Debug-only noise.

### Next action
- QA-0 (MT Composite RenderTask / DSP-12 fix). Re-author the plan as a fresh silly-name file from chat history, then execute against the new dual-config build (verify in Debug first, then Release). Three deferred findings (#8 / #9 / #13) wait their turn as dedicated batches.

---

## 2026-05-07 — QA-0 — MT Composite RenderTask (DSP-12 restore)

### Done
- New `Source/Engine/Tasks/CompositeAudioInsertTask.h/.cpp` per-row task that owns BOTH the arrangement-clip flow (was AudioInsertTask) AND the clip-engine MIDI trigger flow (was ClipPageTask) at audioInsert(N) channel ids.
- Order B execution inside `run()`: clear blockView once -> clip-engine flow if engine set -> arrangement-clip flow accumulating via `renderAudioClipsForRow` mtDest. Both contributions sum into mOutputBuffer.
- Strategy 1a lifecycle: `ensureAudioInsert(row)` constructs the Composite. `registerClipEngine(pageIdx, eng)` defensively ensures the Composite exists then sets its mClipEngine pointer. `unregisterClipEngine` clears the pointer; the per-row Composite stays alive.
- Old `AudioInsertTask.h/.cpp` + `ClipPageTask.h/.cpp` deleted (4 files). `mClipRenderTasks[]` array deleted from PluginProcessor. CMakeLists.txt updated. Friend declarations swapped to single `CompositeAudioInsertTask`.
- `RenderGraphDispatcher::registerTask` got a `jassertfalse` tripwire on its most-recent-wins fallback. For normal paths the branch is now unreachable; tripwire surfaces any future regression where a new task type re-introduces a multi-source channel without going through a composite shape. Release silent fallback stays as safety net.
- DSP-12 verified in Release with both MT-on and MT-off: WAV + MP3 drops on Builder play correctly; piano-roll triggers via main Piano Roll tab play correctly through the auto-spawned Clips engine; both flows sum on the same audio insert.

### Found along the way
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

### What was done about each finding
All 8 new findings + 3 carried from QA-0a (#8, #9, #13) routed at QA-0 close per Rule 3 (§0). See main plan §9 Forks entry "QA-0 close routings" for full mapping. Summary:

| Finding | Routing |
|---|---|
| #8 | Folded into QA-D (project-load teardown surface) |
| #9 | Promoted to QA-Md (new dedicated Phase 1 batch immediately after QA-0) |
| #13, #14, #16a, #16b, #21 | Folded into QA-E (audio-row + tab-callback + mute-dispatch surfaces) |
| #15, #17, #18, #19, #20 | Folded into QA-H (Builder UX + state cluster) |
| #16c | Folded into QA-J (per-row audio-clip rendering surface) |

QA-Md promoted from Phase 5 to Phase 1 because Debug's diagnostic value depends on MT actually engaging there; downstream batches need this for precise MT bug diagnosis.

### Carry-forward contradictions (if any)
- Carry-forward §1 says MT engine is "production, default ON". True for Release; **no-op under Debug per finding #9**. Already noted in QA-0a's implemented-work entry; reaffirmed here. QA-Md investigates.

### Files touched
- New: `Source/Engine/Tasks/CompositeAudioInsertTask.h/.cpp`
- Deleted: `Source/Engine/Tasks/AudioInsertTask.h/.cpp`, `Source/Engine/Tasks/ClipPageTask.h/.cpp`
- Modified: `CMakeLists.txt`, `Source/PluginProcessor.h`, `Source/PluginProcessor.cpp`, `Source/Engine/RenderGraphDispatcher.cpp`
- Modified (plans): `Main Plan.md` (§5 entries for QA-D/E/H/J got Folded-in sub-bullets; new QA-Md §5 entry; §6 sequencing arrow updated; §9 Forks entry "QA-0 close routings" appended), `composite-merging-rivers-twilight.md` (QA-0 per-batch plan), `Implemented Work Log.md` (this entry).

### Commit(s)
- `611db82` QA-0 Step 1: scaffold CompositeAudioInsertTask (skeleton).
- `f72cd09` QA-0 Step 2: implement Composite::run() (Order B both flows).
- `df6f0a3` QA-0 Step 3: wire CompositeAudioInsertTask at registration sites; drop separate ClipPageTask registration.
- `0ef0c95` QA-0 Step 4: remove orphaned AudioInsertTask + ClipPageTask sources.
- `4200479` QA-0 Step 5: jassertfalse tripwire on dispatcher most-recent-wins fallback.

### Next action
- QA-Md (MT Engine Debug-Build Investigation). Plan file: TBD silly-name when batch starts. Diagnostic-first: determine why MT path is no-op under Debug (workers not picking up tasks, runUntilOrTimeout returning immediately, or compile-time gate). Then fix scope.

---
