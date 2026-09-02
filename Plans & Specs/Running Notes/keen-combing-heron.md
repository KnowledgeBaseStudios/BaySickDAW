# Running Notes — QA-Soundness (keen-combing-heron)

> Append-only mid-batch log. Entries land at every checkpoint (commit landed / finding captured
> / spec call resolved / scope pivot) per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At code-complete, `/draft-doc batch-close` consumes this file to compile the Implemented Work
> Log entry, which is HELD here (bulk-run R2) and applied at the batch's campaign section pass.
>
> **This batch carries a FINDINGS LEDGER as its primary artifact** — every confirmed finding, every
> REFUTED one and why it was refuted, and every file touched. A sweep this wide produces
> plausible-but-wrong findings, and the record of what was rejected is as load-bearing as the record
> of what was fixed.
>
> Pair file: [`Plans & Specs/Batch Plans/keen-combing-heron.md`](../Batch%20Plans/keen-combing-heron.md).
> Conventions: Main Plan §0 (Batch Plans + Running Notes layout, locked 2026-05-11).

*(Seeded 2026-07-25 at plan approval. Runs LAST in G4, after `clean-pointing-stoat` —
which subsequently merged into `long-rewinding-yak`.)*

## 2026-08-06 — Batch open — Rule 1 reads + the three plan-mandated corrections

Rule 1 reads done: Main Plan §0 (direct read), Carry-Forward Reference (full),
Implemented Work Log via doc-reader (last applied entry is QA-UICleanup
2026-07-08; every G4 close is HELD per R2 in its batch's running notes), heron
plan (including Jeff's 2026-08-06 documentation-capture addition), yak running
notes carry-forward material.

**Correction 1 — re-measured surface (2026-08-06, supersedes the 2026-07-25
pre-measurement):**

| Measure | 2026-07-25 | 2026-08-06 |
|---|---|---|
| `juce::String err;` | 29 sites / 11 files | **42 sites / 15 files** (StandaloneEditor.cpp 8, BaySickNAMIREditor.cpp 8, BaySickPedalsEditor.cpp 5, SlotComponent.cpp 3; new since: Hosting/ (HostedPlugin, PluginHostMain), EffectsPage, EffectEditorPanels, EffectPresetIO, NAMPedalStyleDSP) |
| `juce::String& outErr` | 27 decls / 13 files | **51 decls / 20 files** (new since: BuilderPage .h/.cpp 10, EffectPresetIO 8, FxRackPresetIO 4, Mp3Writer, LoudnessReportWriter, MicSimDSP, NAMPedalStyleDSP) |
| `existsAsFile()` | 150 sites / 38 files | **206 sites / 46 files** (StandaloneEditor 39, BuilderPage 19, ProjectManager 13, StandaloneApp 13) |

Growth is exactly where mammoth/yak churned: export/render, hosting, preset IO.

**Correction 2 — badger dedupe (no double-fix, no double-claim):**

- `BaySickSolsticeEditor.cpp:293` mPartSel comment — **FIXED by badger Task 9** (comment
  now states the true mechanism: slider IS made visible, never gets bounds).
  NOT a heron item.
- `kStateVersion` — **DECIDED by badger Task 9**: kept write-only with a keeper
  comment (post-v1 migration hook; matches heron Task 6's note-but-don't-act
  stance). NOT a heron item beyond Task 6 confirming the comment survives.
- `InstPage.h:19` Vox/NAM-IR hosting claim — **NOT in badger's scope** (badger's
  InstPage item was the `engineRootTag` label rename). Verified still live
  2026-08-06: InstPage.h:16-21 claims its NAM/IR sub-tab is the "same setup as
  the Vox page's NAM/IR sub-tab" / "mirrors Vox page layout", but Vox hosts
  NAM/IR as an embedded sub-processor inside BaySickVocal with its editor in a
  contained window off the title strip (VoxPage.h:50-52, :70-71), not as a
  direct BaySickNAMIRProcessor sub-tab. Stays a heron Task 3 item.

**Correction 3 — stale motivator:** "automation registry grows on tab churn" was
fixed by badger Task 7 step 1, whose mechanism was itself superseded by mammoth
TS3 (all registration model-side; applicators resolve through the model at apply
time). Not chased.

**Yak carry-forward hazards internalized (sweeps must NOT "correct" these):**

- FOUR vendored JUCE files carry deliberate patches: `juce_AudioProcessorValueTreeState.h/.cpp`
  (write-time phase marking, keep-history + StateSwapAction, liveness registry +
  tags + lazy bind + boundary flush), `juce_ParameterAttachments.cpp` (gesture
  naming + boundary flush), `juce_Windowing_windows.cpp` (in-process key-forward
  suppression). `juce_ParameterAttachments.h` is byte-identical to stock again.
  Any vendored-JUCE update must re-apply ALL FOUR. Vendored libs are out of
  sweep scope (2=b) regardless.
- The (1,N) undo depth cap IS the honesty mechanism (ruling 5), not a bug.
- The undo tag registry + boundary flushes are load-bearing.
- Carry-Forward §2 patterns (seqlock, atomic snapshot + RetirementQueue) are
  deliberate; Task 4 verifies findings against §2 before believing them.

**Uncommitted tree at open (folds into this batch's ONE commit):** heron plan
+31 lines (Jeff's 2026-08-06 documentation-capture addition), yak running notes
+ v1-master-test-plan `5c43cfa0` hash backfills (two doc-only lines from the yak
close).

**Batch shape reminder:** sweep -> adversarial verify (REFUTED findings recorded
with why) -> fix confirmed in-batch (1=a) -> build gate -> notes. Ends when a
re-sweep round is clean (Task 8). Task 9 consolidates the documentation ledger
into `Plans & Specs/System Reference/` AFTER Task 8.

## 2026-08-06 — Task 1 — Silent failures + swallowed errors (sweep -> verify -> fix -> gate)

**Sweep (3=a multi-agent):** six finder agents over the re-measured surface — errstrings (57
sites examined), outerr-callers (53), existsasfile-standalone (113), existsasfile-rest (87),
silent-skip-persistence (84), silent-skip-engines (82) = 476 sites classified. 84 raw findings,
64 unique after cross-finder dedupe (the err-string/outErr/existsAsFile shapes overlap heavily
at the same sites). 124 sites explicitly recorded deliberate-silent with reasons.

**Adversarial verify:** every unique finding went to a refuter agent (grouped per file,
refute-first stance). **57 CONFIRMED / 6 REFUTED / 1 UNCLEAR.**

**Fix pass — all 57 fixed in-batch (1=a):** 41 findings in 17 disjoint single-file clusters via
parallel fix agents working from the verified fixNotes; the 16 cross-coupled hub findings
(StandaloneEditor.cpp, PluginProcessor.h/.cpp, DrumPage, ProjectBundler) fixed directly in the
main session. Every agent diff reviewed against its fixNote before the gate; zero improvised
deviations (the recorded ones are all narrower-than-spec, listed at the end).

### Confirmed findings (fixed)

- `Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:1103` — Project-load restore of the NAM amp model and amp IR (both slots) swallows every load failure - logged to namir_state_log.txt only, never MissingFileReport, while the editor still displays the remembered filename as if loaded.
- `Source/Standalone/StandaloneEditor.cpp:18754` — At record commit, a Denoise::cleanFile failure is dropped without even a DBG - the uncleaned take silently lands on the grid in place of the cleaned take the user opted into.
- `Source/BaySickPedals/BaySickPedalsEditor.cpp:597` — The per-pedal preset menu drops every EffectPresetIO error: savePreset (597), saveAsDefault (606), loadPreset factory (615), and loadPreset user (625) all capture err and ignore the bool return.
- `Source/Inst/InstPage.cpp:1373` — Pedalboard preset save (1373) and load (1392) both capture err and drop it along with the bool result.
- `Source/DSP/NAMPedalStyleDSP.cpp:280` — Restoring a NAM pedal whose .nam file exists but fails to parse drops the load error: no MissingFileReport entry (the sibling missing-file branch has one), and the remembered path is discarded so the panel cannot even name the broken capture.
- `Source/Standalone/StandaloneEditor.cpp:17361` — Staggered per-pattern freeze fill render failure is DBG-only; the pattern presumably stays pending, so the blocking render + overlay flash can repeat every quiet tick with no user signal.
- `Source/Standalone/StandaloneEditor.cpp:17342` — Automatic stale-freeze re-render failure is DBG-only: the render notice overlay flashes, then the freeze silently stays stale (tab plays live, CPU relief lost) with no user signal ever.
- `Source/Standalone/StandaloneEditor.cpp:17444` — Auto-freeze (CPU-threshold) render failure drops err entirely - not even a DBG - and just tries the next tab.
- `Source/BaySickPedals/BaySickPedalsEditor.cpp:606` — Save as Default on a pedal slot ignores the saveAsDefault result and drops the error string.
- `Source/BaySickPedals/BaySickPedalsEditor.cpp:615` — Loading a factory pedal preset ignores the loadPreset result and drops the error string.
- `Source/BaySickPedals/BaySickPedalsEditor.cpp:625` — Loading a My Presets pedal preset ignores the loadPreset result and drops the error string.
- `Source/Inst/InstPage.cpp:1392` — Loading a pedalboard preset from the Inst page menu ignores the result and drops the error string.
- `Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:1194` — The pre-A/B legacy global mic-IR restore silently skips a missing file and drops loadUserMicIr's result and error.
- `Source/Standalone/StandaloneEditor.cpp:17447` — Auto-freeze drops the freezeTab/freezeRustyKit error string entirely -- not even a DBG -- and just moves on to the next tab.
- `Source/Standalone/StandaloneEditor.cpp:17536` — Project-load freeze restoration reports a failed re-freeze via DBG only, so a hand-frozen tab silently comes back UNFROZEN.
- `Source/Standalone/StandaloneEditor.cpp:16785` — Project-load Rusty Drums restore reads the persisted KitPath as a raw path, so the stable-root refs every shipped kit has been saved with since QA-ProjectSave Task 5 (2026-07-26) fail existsAsFile() and the entire kit + CC restore is silently skipped.
- `Source/Standalone/StandaloneEditor.cpp:16604` — Project-load restore of a sfizz Inst tab (BaySickGuitars/BaySickBasses) silently substitutes the default factory kit when the saved kit path does not resolve â€” no MissingFileReport entry, no dialog.
- `Source/Standalone/StandaloneEditor.cpp:8402` — v1 factory-template apply silently skips a missing kit file and silently skips missing Layer/Bass preset files â€” the template loads half-empty with no report, while the v2 template branch reports missing files.
- `Source/Standalone/BassPage.cpp:805` — Factory BaySickPlayer preset load applies the APVTS state, renames the tab to the preset, then silently skips loading the preset's <Sample> SFZ/file when the path is missing â€” the engine renders silence while presenting as loaded.
- `Source/Standalone/ProjectBundler.cpp:247` — Folder-mode bundle write silently swallows per-file copy failures and silently aliases distinct source files that share a basename â€” the bundle reports success while missing or mismatching audio, contradicting the module's own "Missing files are REPORTED, never silently dropped" contract.
- `Source/Standalone/StandaloneEditor.cpp:18752` — Recording commit captures the de-noise error string from Denoise::cleanFile and drops it â€” a failed clean silently falls back to placing the uncleaned take on the grid despite the user opting into cleaned takes in File Settings.
- `Source/Standalone/BuilderPage.cpp:1333` — The Properties-dialog "Copy to ..." flow (browser entry and grid clip variants) silently dead-ends when the source file is missing or the physical duplicate fails â€” the user confirms a Copy and nothing happens.
- `Source/Standalone/BuilderPage.cpp:5231` — ArrangementGrid::importAudioFile bails on file-not-found with only a hidden diagnostic-file log â€” a dead drop with no UI â€” while the sibling placeAudioLibraryEntry received an explicit QA-H fix adding an AlertWindow for the identical situation.
- `Source/Standalone/BaySickRustyDrumsPage.cpp:459` — Preset picked from a menu silently no-ops when its file is deleted or its XML is corrupt â€” a class of nine load-preset entry points across the pages share the shape.
- `Source/Standalone/EventEditor.cpp:1897` — Import MIDI CC Data silently no-ops on an unreadable or unparseable .mid file.
- `Source/Standalone/BuilderPage.cpp:1377` — "Duplicate..." on an audio-library browser row silently no-ops when the entry's backing file is missing.
- `Source/DSP/AcousticSimulatorStyleDSP.cpp:182` — (Adjacent to slice, traced from the Standalone IR panels) Both acoustic IR effects restore a persisted user-IR path and silently skip a missing file on project load â€” no MissingFileReport entry, unlike the NAM pedal which reports.
- `Source/DSP/AcousticPreampStyleDSP.cpp:176` — A persisted Acoustic Preamp user IR whose file is gone silently degrades the effect to an identity (bypass) convolution on project restore.
- `Source/VibePlayer/VibePlayerDSP.cpp:260` — During SFZ load, any region whose sample= file is missing is silently dropped - the region is never flushed - so those keys are simply dead with no count, no log, no report.
- `Source/Clips/ClipsPage.cpp:349` — savePagePreset ignores every write failure - the audio copy into My Samples (337), the preset writeTo (349), and the fallback replaceWithText (353) - then clears the dirty flag and fires onSaved, which in the "Save Page Preset & Delete" flow deletes the tab even though nothing was saved.
- `Source/Vox/VoxPage.cpp:209` — VoxPage::savePagePreset fires onSaved (the chained delete) even when the export came back empty or the preset write failed - both are unchecked.
- `Source/Inst/InstPage.cpp:522` — InstPage::savePagePreset has the identical unchecked-write + unconditional onSaved chain as VoxPage.
- `Source/ProjectManager.cpp:188` — Autosave failures are permanently invisible: timerCallback discards writeBackup()'s false return, and timerCallback is writeBackup's only caller.
- `Source/ProjectManager.cpp:167` — restoreBackup ignores the result of copying the backup over project.xml, then clears dirty - so a failed disk write leaves the session showing the restored state as clean while project.xml still holds the old state.
- `Source/Inst/InstPage.cpp:1192` — A failed sfizz program switch from the program picker is a silent no-op: switchSfizzProgramWithUndo bails on switchSfizzProgram's false return with no message (and the preset-load spawn path at 618-620 discards the same loaders' returns).
- `Source/Clips/ClipsPage.cpp:399` — Loading a Clip page preset whose embedded clipRef audio has vanished from My Samples silently restores the page without its audio, and a missing/corrupt preset file is a silent no-op click (same guards exist in VoxPage.cpp:133/218 and InstPage.cpp:354/531).
- `Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:1193` — The legacy (pre-A/B-snapshot) global mic user IR restore path still has the original silent-skip: missing file has no else, and loadUserMicIr's error is explicitly discarded.
- `Source/PluginProcessor.cpp:5907` — stopRecordingSession silently omits any armed strip whose recorder failed to produce a file - the take dialog simply lacks that strip with no message.
- `Source/ProjectManager.cpp:646` — saveSettings ignores the replaceWithText result, so loss of settings.xml writes (recent projects, default template, prompt-skip flags) is silent and repeats forever.
- `Source/Hosting/PluginManager.cpp:598` — saveToDisk ignores root.writeTo's result, so a failed write of the plugin database (added plugins + scan folders) is silent and the user's plugin list quietly reverts on next launch.
- `Source/Standalone/EffectPresetIO.cpp:668` — migrateTapeFolderToSaturation deletes the user's Tape preset even when the copy to Saturation/My Presets failed.
- `Source/Standalone/BuilderPage.cpp:9185` — renderToFile ignores Mp3Writer::close()'s failure return, so an MP3 export whose tail flush failed is reported as a successful export.
- `Source/Standalone/StandaloneEditor.cpp:16796` — BaySickRustyDrums project restore silently drops the whole saved kit + CC state when the kit file is missing, the engineData blob is corrupt, or reloadForProjectRestore fails.
- `Source/Standalone/PagePresetIO.cpp:767` — Page-preset import failure is silent end to end: the legacy wrapper drops the v2 importer's bool, and no caller of either API surfaces failure, so a corrupt preset file is a silent no-op.
- `Source/Standalone/StandaloneEditor.cpp:8326` — applyTemplate silently no-ops on a corrupt/invalid template file, and loadTemplate silently no-ops on a missing one.
- `Source/Standalone/FxRackPresetIO.cpp:118` — FX Rack preset load silently skips the entire effect-rack half when the <Rack> blob is corrupt or missing, and still returns success.
- `Source/Standalone/StandaloneEditor.cpp:16159` — The project restore walker's shared applyEngineState helper silently skips a present-but-corrupt engineData blob, leaving that tab's engine at defaults with no signal.
- `Source/ProjectManager.cpp:360` — saveProjectAs ignores the inner saveProject() result and returns true even when the new folder's project.xml was never written.
- `Source/Standalone/StandaloneEditor.cpp:12575` — The initial project.xml write after creating a project is fire-and-forget at three sites, so a failed first save produces a project that later vanishes from the browser.
- `Source/VibePlayer/VibePlayerProcessor.cpp:440` — BaySickPlayer (VibePlayer) state restore silently skips the saved sample folder/SFZ/file when it no longer resolves -- no MissingFileReport, no dialog -- so the tab restores looking loaded and plays nothing.
- `Source/Standalone/StandaloneEditor.cpp:16616` — Inst sfizz tab restore silently substitutes the factory default kit when the saved kit path is gone (no MissingFileReport), and ignores the kit-load wrapper's failure return, so a present-but-unparseable kit restores as a silent tab.
- `Source/Standalone/PagePresetIO.cpp:142` — When a page preset's sfizz kit file exists but fails to load, applyEngineSlotFromXml returns false with NO alert (only the missing-file branch alerts), importPagePreset propagates false, and the only caller of the struct API (InstPage::loadPagePreset) discards the return -- so the preset load half-applies with zero feedback.
- `Source/Inst/InstPage.cpp:615` — Loading a sfizz page preset onto a tab whose engine does not yet exist silently produces a kitless sfizz tab when the saved kit path is missing -- the alert the code comment promises can never fire because the Sfizz slot never makes it into the import config.
- `Source/Inst/InstPage.cpp:1139` — A program pick from the Inst program menu that fails to load (sfizz parse failure on an existing file) silently does nothing -- switchSfizzProgram returns false and both callers drop it.
- `Source/Standalone/DrumPage.cpp:1157` — Drum sound-preset import leaves the engine's sample slot empty when the referenced sample file/folder is gone, while still stamping the preset's name onto the tab -- explicit silent-skip, no report.
- `Source/VibePlayer/VibePlayerDSP.cpp:44` — VibeSampleManager's three load entry points produce an empty region list on failure that is indistinguishable from success, and no caller anywhere checks the region count -- unreadable/corrupt files and SFZs whose samples are missing all load as silence.
- `Source/PluginProcessor.cpp:5084` — rebuildAudioClipPlayers silently drops any arrangement audio clip whose file exists but cannot be decoded (createReaderFor returns null) -- the block stays visible on the grid and plays silence; the QA-ProjectSave missing-clip report only covers the file-not-found case at project load.

### Refuted findings (recorded per plan — an agent finding is a lead, not a fact)

- `Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:698` — CLAIMED: loadImpulseResponse's not-yet-prepared branch promises "IR will load on next prepare" but no code ever performs that deferred load, and the state-restore caller drops the false return.
  REFUTED: The cited code is real (cpp:695-701 parks the path and promises a reload; prepareToPlay at cpp:187-240 only resets/prepares mIr and never reads mIrPaths; grep confirms no other reload site) but the failure scenario is structurally unreachable: a state restore can never land before the processor's first prepareToPlay. The tree has exactly two construction sites for BaySickNAMIRProcessor, and both prepare it synchronously at creation, before any restore path can hold a pointer to it: (1) EngineRig.cpp:501-503 â€” the Inst chain creates the NAMIR and calls `nam->prepareToPlay (44100.0, 512)` in the same block, before tab.namIr is published; InstPage (the restore caller via importInstState, Inst [full trace in the verify record]

- `Source/BaySickGuitars/BaySickGuitarsProcessor.cpp:682` — CLAIMED: On restore, a Guitars kit file that EXISTS but fails sfizz parsing is swallowed - loadKit's false return is discarded - so the tab restores looking normal and produces nothing; only the file-missing case is reported.
  REFUTED: The cited code exists as described, but the path is structurally unreachable: BaySickGuitarsProcessor::setStateInformation has zero callers anywhere in the tree. Every route that restores a Guitars tab deliberately bypasses it because re-running loadKit without the active-flag dance is a documented sfizz crash path: (1) project restore (StandaloneEditor.cpp:16579-16668, comment 16625-16628) calls VibeSynthProcessor::loadBaySickGuitarsKit + replaceStateKeepingUndoHistory directly; (2) undo tab resurrection (StandaloneEditor.cpp:11993-12086) mirrors the same bypass, and its generic applyEngineState lambda only runs in the non-sfizz branch against InstPage::getEngineProcessor(), which returns t [full trace in the verify record]

- `Source/BaySickBasses/BaySickBassesProcessor.cpp:675` — CLAIMED: Same as Guitars: setStateInformation discards loadKit's false return, so an existing-but-unparseable Bass kit restores silently broken.
  REFUTED: Structurally unreachable: BaySickBassesProcessor::setStateInformation has zero call sites in the tree. Every Basses restore flow deliberately bypasses engine setStateInformation because re-running loadKit without the active-flag guard is a documented crash path (J-9 race fix, comments at StandaloneEditor.cpp:16625-16628 and 16769-16771, PagePresetIO.cpp:123-128): project load (StandaloneEditor.cpp:16579-16668) and tab resurrection (:11993-12086) load the kit through the checked wrapper VibeSynthProcessor::loadBaySickBassesKit (PluginProcessor.cpp:7802 checks loadKit and returns false) then replaceStateKeepingUndoHistory directly on the apvts; page-preset import always has kitLoadCallback set [full trace in the verify record]

- `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:1009` — CLAIMED: Same as Guitars/Basses: the Rusty Drums restore path ignores loadKit's false return, so a kit that exists but fails BOTH the wrapper string-load and the plain-file fallback restores as a silent kit.
  REFUTED: Structurally unreachable: BaySickRustyDrumsProcessor::setStateInformation has no caller anywhere in the tree, so line 1009 never executes. Both live Rusty restore paths deliberately bypass it (documented crash-path comments at StandaloneEditor.cpp:16769 and PagePresetIO.cpp:125): project load (StandaloneEditor.cpp:16749-16819) and undo tab-resurrection (StandaloneEditor.cpp:12088-12139) decode the KitPath blob manually and route through BaySickRustyDrumsPage::reloadForProjectRestore -> loadBaySickRustyDrumsKit, which DOES check the engine loadKit return (PluginProcessor.cpp:7883-7890); page presets go through PagePresetIO's kitLoadCallback branch (buildRustyPresetCfg at StandaloneEditor.cpp: [full trace in the verify record]

- `Source/Standalone/BuilderPage.cpp:9271` — CLAIMED: During export, a stem whose strip tap resolves null is silently skipped per block, producing a short, empty, or time-misaligned stem file inside a 'successful' export.
  REFUTED: The null branch is structurally unreachable during a render, and the claimed misalignment mechanism is impossible even hypothetically. getStripOutputForTap forwards to ChannelBufferArena::getStripBuffer (ChannelBufferArena.h:70-77), which returns null only for a channelId outside 0..999 (RenderEngine::kMaxStripChannels = 1000, RenderEngineFlags.h:75) or an unprepared arena. Neither can occur here: (1) every stem/mix channelId the export surfaces supply is a real MixerChannelIds id <= 919 - the stems dialog lists only visible mixer strips (MixerPage::getStemPickEntries, MixerPage.cpp:3042-3050, consumed at StandaloneEditor.cpp:12988-12991), pattern renders use getPatternTracks (BuilderPage.cp [full trace in the verify record]

- `Source/PluginProcessor.cpp:6322` — CLAIMED: deserializeProject cannot distinguish 'child absent because old project' from 'child present but unreadable' -- a project.xml with a damaged PatternManager or Processor subtree opens as a success with that whole subsystem at defaults.
  REFUTED: Both cited silent branches are unreachable for any corruption the app or OS can produce; every real damage class lands on the ALERTED path. (1) 'Child present but unreadable' is structurally dead: juce::ValueTree::fromXml (juce_ValueTree.cpp:1013-1028) returns an invalid tree ONLY for XML text elements, and root.getChildByName("PatternManager") can never return a text element - so pmTree.isValid() cannot be false at 6325. Any actual byte damage inside the subtree breaks well-formedness of the WHOLE document, XmlDocument::parse returns nullptr, ProjectManager::openProject returns false (ProjectManager.cpp:301-302), and all three call sites alert the user ('...or the file is corrupt' - Standal [full trace in the verify record]

### Unclear — spec call posed to Jeff (RESOLVED same session, see the docket entry below)

- `PagePresetIO.cpp:110` — `applyEngineSlotFromXml` treats a present-but-corrupt (non-empty,
  undecodable) engine blob as success under the documented "missing-data cases return true"
  back-compat tolerance. Whether that tolerance should split "absent" (fine) from
  "present-but-garbage" (fail loudly) is a product call, and the dominant corruption route
  (base64 decodes fine, blob magic-check no-ops inside the engine) needs blob validation beyond
  the cited branch. **Jeff 2026-08-06: 1=b — fail loudly.** Shipped in the Task 1b/3a/4b pass.

### Leads routed to later tasks (from refutations — Rule 3, stays in-batch)

- Task 2 (dead code): `BaySickGuitarsProcessor::setStateInformation` /
  `BaySickBassesProcessor::setStateInformation` / `BaySickRustyDrumsProcessor::setStateInformation`
  have ZERO callers tree-wide (every restore path deliberately bypasses them for the race-safe
  wrappers) — including the dead `MissingFileReport::add` at `BaySickRustyDrumsProcessor.cpp:1013`
  (QA-Export Task 5 fixed a site that never runs). `BuilderPage.cpp:9254/:9271` null-tap continue
  branches are structurally unreachable (arena pre-allocates all 1000 ids). NAMIR
  `loadImpulseResponse` not-yet-prepared branch (cpp:695-701) is dead from every live path.
- Task 3 (comment truth): the NAMIR dead branch's "IR will load on next prepare" promise is stale
  text — no code performs the deferred load.

### Residuals (recorded, not silent)

- Interactive SFZ loads with PARTIALLY missing samples: `parseSFZ` now files per-sample
  MissingFileReport entries, but interactive load paths have no drain — entries surface at the
  NEXT project/template load's dialog (mis-attributed timing, correct content). The zero-regions
  case does alert immediately via `hasAnyRegions()`. Revisit at Task 8 re-sweep.
- Mid-session unreadable-clip discovery (`rebuildAudioClipPlayers`) drains at
  `restoreAudioStripsFromArrangement` tail (covers every load path); a corrupt WAV dropped
  mid-session reports at the next load-type event rather than instantly.
- ProjectBundler basename-collision uniquify: the " (N)"-renamed copy is not referenced by the
  bundled project.xml, so on the destination machine that reference reports MISSING (visible)
  instead of playing the WRONG file (silent) — full fix would rewrite bundle-internal references,
  which is a feature change (Jeff's call if wanted). **RESOLVED: Jeff 2026-08-06 4=b — rewrite.
  Shipped, see the docket pass below; this residual is closed.**
- `ProjectManager::saveProjectAs` failure path leaves mCurrentFolder pointing at the failed
  target (fixNote called rollback optional; dirty stays set, caller alert fires).

### Narrower-than-spec agent deviations (all deliberate, none behavioral)

- Acoustic label: applied "Acoustic Sim user IR" (the Sim section's own label) over the
  cross-reference's "Acoustic Simulator user IR".
- StandaloneEditor restore-backup alert text broadening was done in the hub pass (agent's file
  boundary), as was the DrumPage/ClipsPage `hasAnyRegions()` caller-side checks.
- NAMIR legacy pre-A/B mic-IR fallback branch KEPT (reporting added); deleting it outright is
  Jeff's call (docket).

### Files touched (Task 1 fix pass, 33 source files)

BaySickNAMIRProcessor.cpp; BaySickPedalsEditor.cpp; ClipsPage.cpp/.h; AcousticPreampStyleDSP.cpp;
AcousticSimulatorStyleDSP.cpp; NAMPedalStyleDSP.cpp; PluginManager.cpp/.h; InstPage.cpp;
PluginProcessor.cpp/.h; ProjectManager.cpp/.h; BassPage.cpp; BaySickRustyDrumsPage.cpp;
BuilderPage.cpp; DrumPage.cpp/.h; EffectPresetIO.cpp; EventEditor.cpp; FxRackPresetIO.cpp;
LayersPage.cpp; PagePresetIO.cpp; ProjectBundler.cpp/.h; StandaloneEditor.cpp/.h;
VibePlayerDSP.cpp; VibePlayerEditor.cpp; VibePlayerProcessor.cpp/.h; VoxPage.cpp

**Build gate:** GREEN first try — five exit codes 0, four `vcxproj -> .exe` link
lines (Release + Debug BaySickDAW, Host64, Host32), zero `error C|LNK|MSB` hits.

## 2026-08-06 — Task 1 follow-on — Bundle-walk coverage + Jeff's docket (1=b, 2=a, 3=a, 4=b)

**Trigger:** the Task 1 acoustic-IR fix raised a question the fix itself could not
answer — is the bundler's attribute walk actually reaching the engine-held file
references it claims to? A dedicated adversarial verification agent traced the
whole path. **Claim CONFIRMED, and worse than filed.**

### Bundler findings (all CONFIRMED by file:line trace, all fixed)

1. **Two of the six `kPathAttrs` names were PHANTOMS.** `"namPath"` / `"irPath"`
   matched no producer anywhere in the tree — the NAM/IR processor writes
   `nam_filepath` / `ir_filepath` (+ `_b` for slot B) and `mic_user_ir_path`
   (`BaySickNAMIRProcessor.cpp:1040-1044`). Every NAM capture and amp IR has been
   invisible to the walk since the list was written. FIXED: real names in,
   phantoms out, plus `modelPath` (NAM pedal) and `userIR` (both Acoustic units).
2. **The Inst chain walk found nothing it was written to find.** `instChainState`
   holds `<NamIrState data=...>` / `<PedalsState data=...>` whose payload is
   BASE64, not attributes (`InstPage.cpp:1287-1301`) — the plain-attribute walk
   could never see inside. The pedal board nests AGAIN (each `<Slot data=...>`
   holds one pedal's state, `BaySickPedalsProcessor.cpp:352-363`), so a NAM
   pedal capture sits two blob layers deep. FIXED: the walk now descends every
   base64 blob (depth-capped at 4) and the nested-XML `instChainState` attribute.
3. **Mixer rack states were never handed to the walker at all.** Both Acoustic
   units are user-placeable in any rack slot (`SlotComponent.cpp:797,802`), and
   racks serialize under `<Processor>` (`PluginProcessor.cpp:6224-6232`), a
   subtree `enumerate` never received. FIXED: `enumerate` takes an optional
   rack-states snapshot; the export site feeds it `saveRackStates`.
4. **Two incompatible base64 encodings.** `MemoryBlock::toBase64Encoding` writes a
   `"<size>."` prefix (chain children, pedal slots) while `juce::Base64::toBase64`
   writes plain base64 (VibeGraph rack props, EffectRack slots), and
   `fromBase64Encoding` REJECTS the plain form. A single-form decoder would have
   silently walked past half the tree — the same silent-skip class this batch
   exists to kill. FIXED: `decodeAnyBase64` tries both and records which form, so
   the rewrite re-encodes in the form it found.

### Jeff's docket rulings (2026-08-06), all shipped this pass

- **1=b — present-but-corrupt engine data fails loudly.** `applyEngineSlotFromXml`
  keeps the documented ABSENT-data tolerance, but non-empty data that fails
  base64 decode OR the `getXmlFromBinary` magic check now alerts and aborts the
  import instead of reporting success. The magic check is the part that matters:
  in-attribute corruption usually still base64-decodes, and `setStateInformation`
  then no-ops silently inside the engine.
- **2=a — NAM/IR legacy pre-A/B mic-IR fallback KEPT** as fixed in Task 1
  (reporting added, branch retained). No further change.
- **3=a — acoustic interactive IR loads now report.** `loadUserIR` on both units
  returns `bool` + `outErr` and PROBES the pick with an AudioFormatReader before
  committing: `juce::dsp::Convolution::loadImpulseResponse` reports nothing back
  and `reloadConvIR` falls back to a one-sample identity IR, so an unreadable
  pick used to land as "the effect silently does nothing". A failed pick now
  leaves the previous IR in place and alerts. The documented empty-File clear
  path is preserved.
- **4=b — the bundle re-references what it relocates.** `write` now records
  `storedPath -> Samples/<final name>` for every file it copies (including a
  " (N)" collision rename) and rewrites the BUNDLED project.xml through the
  mirror of the walk — attributes, nested `instChainState`, and every base64
  blob layer re-encoded in its original form. Folder mode rewrites the copied
  project.xml in place; zip mode substitutes a rewritten temp for that one entry
  (the copy pass had to move ahead of the file-add pass, since a zip entry
  cannot be rewritten once added).
  **Scope note, surfaced deliberately:** this closes a hole wider than the
  collision case it was asked for. Self-contained bundles copied UserSamples /
  absolute references into `Samples/` but never re-pointed them, so those copies
  arrived referenced by their ORIGINAL absolute path and reported missing on the
  destination machine. Same machinery, same ruling — flagged here rather than
  folded in silently.

### Also fixed in this pass

- `adoptTemplateSampleRefs` (StandaloneEditor): deleted the chain-walk block that
  looked for the same phantom attribute names on the chain's elements — it could
  never adopt anything. Its header comment claimed NAM captures and user IRs were
  adopted "through the same helper"; the comment now states the real boundary
  (they live in base64 blobs, template-time adoption does not decode them) rather
  than describing a shape that does not exist. Comment-truth defect, Task 3 class,
  fixed at the site per `feedback_no_docs_only_commit_fix_wrong_comments`.

**Files:** ProjectBundler.cpp/.h; StandaloneEditor.cpp; PagePresetIO.cpp;
AcousticPreampStyleDSP.cpp/.h; AcousticSimulatorStyleDSP.cpp/.h;
EffectEditorPanels.cpp.

**Build gate:** GREEN first try — five exit codes 0, four link lines, zero error hits.

## 2026-08-06 — Task 2 — Dead code + dead registrations (sweep -> verify -> delete -> gate)

**Sweep (3=a multi-agent):** six finders — deadfn-standalone (246 sites examined),
deadfn-engines (346), dead-params (38), dead-ui (380), dead-branches (314),
dead-classes (330) = **1,654 sites classified**, 42 explicitly recorded as
deliberate-keep with reasons. 113 raw findings deduped to 112 unique candidates.

**Adversarial verify (refute-first, one verifier per file group, 68 agents):**
**77 CONFIRMED_DEAD / 3 NOT_YET_WIRED / 3 KEEP / 1 REFUTED / 28 ASK_JEFF.**
Two runs were killed mid-flight by usage limits (6 finders, then 11 verifiers);
both resumed from the same run id with completed agents replaying from cache, so
no verification was lost or repeated.

**The verification earned its keep.** Multiple candidates that looked mechanically
dead were saved by it: `ClipDropDiag` (REFUTED — live at ~25 call sites, and the
finder's premise that its batch had closed was wrong: Main Plan still holds
QA-ClipDrop open by Jeff's ruling), `BaySickBassesProcessor::setStateInformation`
(KEEP — pure-virtual override, cannot be deleted), the BuilderPage null-tap
branches (KEEP — defensive, structurally unreachable but correct to retain), and
`G3PlayheadDiag` (KEEP — explicitly ruled alive by Jeff).

### Deleted (77 confirmed, all with verified per-symbol co-deletion plans)

- `Source/Standalone/BuilderPage.cpp` :: BuilderPage::doNew / doSave / doOpen / doExport
- `Source/Standalone/InstrumentPage.cpp` :: class InstrumentPage (entire translation unit)
- `Source/Standalone/SharedUI.h` :: class SeqRoutingBar
- `Source/Standalone/SharedUI.h` :: class BasicSequenceGrid
- `Source/Standalone/SharedUI.h` :: class BasicEnvelopeEditor
- `Source/Standalone/SharedUI.h` :: class WaveformDisplay
- `Source/Standalone/SharedUI.cpp` :: PageMenuBar::addActionButton / clearActionButtons (+ mActionBtns)
- `Source/Standalone/SharedUI.cpp` :: PageMenuBar::setTabSlotArrow (+ class SplitTabButton)
- `Source/Standalone/BuilderPage.cpp` :: ArrangementGrid::setGhostClip / clearGhostClip / placeGhostClip
- `Source/Standalone/BuilderPage.cpp` :: BuilderPage::doNavigatePage
- `Source/Standalone/MixerPage.cpp` :: MixerPage::drawSectionLabel
- `Source/Standalone/StandaloneEditor.cpp` :: StandaloneEditor::promptCreateProject
- `Source/Standalone/EffectEditorPanels.h` :: EditorPanelBase::onTypeChanged
- `Source/Standalone/SharedUI.h` :: ParametricEQDisplay::drawToolbar
- `Source/Standalone/StandaloneEditor.h` :: StandaloneEditor::getUndoManager
- `Source/Standalone/StandaloneEditor.h` :: StandaloneEditor::getCommandManager
- `Source/Standalone/BuilderPage.h` :: assorted zero-reference inline getters (7 sites)
- `Source/Standalone/WorkspaceWindow.h` :: WorkspaceWindow::tetherFollower / tetherLeader
- `Source/BaySickNAMIR/BaySickNAMIRProcessor.h` :: getNamErrorMessage / getIrErrorMessage
- `Source/BaySickNAMIR/BaySickNAMIRProcessor.h` :: mLastMicIrError
- `Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp` :: setNamFilePath / setIrFilePath
- `Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp` :: BaySickNAMIRProcessor::hasImpulseResponse
- `Source/BaySickSolstice/BaySickSolsticeSynth.h` :: BaySickSolsticeSynth combined-part 'back-compat' setters (setPrismAmount, setPluckDecay, setBlurSize, setFilterMaskAmount, setPhaserMaskRate, setBrownianAmount, setBlurTime, setBlurHarm, setPrismMode, setPluckBlur)
- `Source/BaySickSolstice/BaySickSolsticeXYZPad.cpp` :: BaySickSolsticeXYZPad::setXY
- `Source/DSP/DelayDSP.cpp` :: DelayDSP::setHpHz / setLpHz / setPingPong
- `Source/DSP/FlangerDSP.cpp` :: FlangerDSP::setDryLevel / setWetLevel
- `Source/DSP/EQ8DSP.cpp` :: EQ8DSP::setBandKey
- `Source/DSP/EQ8DSP.cpp` :: EQ8DSP::getBandEffectiveGainDbAtRangeLimit
- `Source/DSP/EQ8DSP.cpp` :: EQ8DSP::getMagnitudeForFrequencyDb
- `Source/EffectRack.cpp` :: EffectRack::getSlotInputLevel / getSlotOutputLevel
- `Source/EngineRig.cpp` :: EngineRig::tabsOf / allocateFreeIndex
- `Source/PatternManager.cpp` :: PatternManager::isComplexSequenceActive
- `Source/PatternManager.cpp` :: PatternManager::renameAutomationTemplate
- `Source/PatternManager.cpp` :: PatternManager::getBeatsPerBarAtBeat + getBeatsPerBarAtBar
- `Source/PatternManager.cpp` :: PatternManager::removeAudioFromLibrary (by path)
- `Source/ProjectManager.cpp` :: ProjectManager::setAutosaveIntervalSeconds
- `Source/Hosting/PluginManager.cpp` :: PluginManager::isOnAddedList
- `Source/SlideSampler/SlideRegionMap.cpp` :: SlideRegionMap::numBands
- `Source/VibePlayer/VibePlayerProcessor.cpp` :: VibePlayerProcessor::getLoadedSampleFile
- `Source/VibeGraph.cpp` :: VibeGraph::hasAudioRowChannel
- `Source/BaySickSynth/BaySickSynthDSP.cpp` :: BaySickSynthDSP::findVoiceForNote
- `Source/PluginProcessor.cpp` :: addLiveInputParams (ids local)
- `Source/VibeGraph.cpp` :: InsertNode::pArm
- `Source/Standalone/BuilderPage.h` :: BrowserPanel::onImportAudio (std::function member)
- `Source/Standalone/BuilderPage.h` :: ArrangementGrid::onImportAudioRequested (std::function member)
- `Source/Standalone/BuilderPage.h` :: ArrangementGrid::onRequestRebuildPlayers (std::function member)
- `Source/Standalone/SlotComponent.h` :: SlotComponent::onBasicModeChanged (std::function member)
- `Source/Standalone/SharedUI.h` :: VKnob::onDragStarted (std::function member)
- `Source/Standalone/SharedUI.h` :: ParametricEQDisplay::onBandChanged (std::function member)
- `Source/Standalone/GlobalTransportBar.h` :: GlobalTransportBar::onRecordModeChanged (std::function member)
- `Source/BaySickSolstice/BaySickSolsticeXYZPad.h` :: BaySickSolsticeXYZPad::onXYChanged (std::function member)
- `Source/Standalone/SharedUI.h` :: BasicStepCell, BasicEnvelopeEditor (1332), SeqRoutingBar (1349), FXChainStrip (1369), WaveformDisplay (1387), BasicSequenceGrid (2096)
- `Source/PluginProcessor.h` :: mLayersPeakDb (+16 sibling mono bus peak mirrors, h:643-702)
- `Source/VibeGraph.h` :: mGlobalFxBypassPtr
- `Source/PluginProcessor.h` :: mAudioRowScratch (+7 sibling dead engine scratch buffers)
- `Source/PluginProcessor.h` :: mMasterFadeGain
- `Source/PluginProcessor.h` :: mInitCounter
- `Source/DSP/ReverbDSP.h` :: mChamberOuterCoef, mChamberInnerCoef, mRoomCombFB, mRoomCombDampA, mVbFDNFeedGain, mVbDampAlpha (h:277-278, 300-301, 311, 313)
- `Source/BaySickSolstice/BaySickSolsticeEditor.h` :: mModXDest, mModYDest, mModZDest (+ mModXDestAtt/mModYDestAtt/mModZDestAtt at h:228)
- `Source/BaySickSolstice/BaySickSolsticeRoutingMatrix.h` :: mToggles[kNumToggles]
- `Source/VibePlayer/VibePlayerEditor.h` :: mFilterArticKnob (+ mFilterArticLbl h:113, mFilterArticAtt h:147)
- `Source/BaySickSynth/BaySickSynthEditor.h` :: mWaveformLbl
- `Source/Standalone/PianoRoll.h` :: mDrumMode (+ PianoRollContainer::setDrumMode, PianoRoll.cpp:3623)
- `Source/Standalone/BuilderPage.h` :: mFileDragPath, mFileDragX, mFileDragY (h:945-947)
- `Source/Standalone/BuilderPage.h` :: mSlipEditDragOrigX
- `Source/Standalone/BuilderPage.h` :: mAutomCurveHandleOrigTension
- `Source/Standalone/BuilderPage.h` :: mAutomDragOrig
- `Source/Standalone/SharedUI.h` :: mDragStartAngle, mDragStartIdx (ChickenHeadSelector)
- `Source/Standalone/SharedUI.h` :: mDragStartFreq, mDragStartGain (ParametricEQDisplay)
- `Source/Standalone/StandaloneEditor.cpp` :: ExportAudioDialog::mLastMeasure, mHaveMeasure (cpp:13338-13339)
- `Source/BassSynth.h` :: BassSynthVoice::mEnvAmp, mEnvPitch (h:42-43)
- `Source/AudioFileRecorder.h` :: mCurrentSampleRate
- `Source/VibePlayer/VibePlayerDSP.h` :: mReductHold
- `Source/Standalone/InstrumentPage.h` :: InstrumentPage (class + whole compilation unit InstrumentPage.h/.cpp)
- `Source/Standalone/BuilderPage.h` :: PatternRowButton (class)
- `Source/BaySickBass/BaySickBassVisualizerScreen.cpp` :: BaySickBassVisualizerScreen.cpp (tombstone file)
- `CMakeLists.txt` :: VibeSynthSamples (juce_add_binary_data target + VIBESYNTH_LINK_LIBS entry line 216)

**Whole files removed:** `Source/Standalone/InstrumentPage.h` + `.cpp` (the
legacy tabbed-era base class — never instantiated, never subclassed, no file even
included it; its CMake entry went with it) and
`Source/BaySickBass/BaySickBassVisualizerScreen.cpp` (a tombstone file holding
only an "intentionally left empty" comment, in no build target — its live `.h`
stays). Both were flagged by the harness as irreversible local destruction; both
were re-verified by hand afterwards against their fixNotes and against a tree-wide
reference grep (zero hits remaining, live siblings intact), and both are
git-tracked so recoverable. Recorded here because a file deletion deserves a
paper trail even when it is right.

### HOLD-FOR markers added instead of deleting (3, NOT_YET_WIRED)

- `LRXHelper::drawVignette` — HOLD-FOR-GL-RENDERER (disabled 2026-04-21 for
  CPU-renderer banding; re-enable plan is in Future State).
- `WorkspaceWindow::releaseContent` — HOLD-FOR-move-between-frames (zero callers
  BY DESIGN; delete only if that idea is formally dropped).
- Main menu Help item 601 — HOLD-FOR-MANUALS-WINDOW (no handler by design per
  Jeff's 2026-07-29 ruling; do not wire a placeholder, do not retire as dead UI).

### Cascade found mid-fix and closed in-batch

Deleting PluginProcessor's mono peak-meter mirrors orphaned **17 VibeGraph mono
peak atomics** (`layersPeakDb` .. `pluginsBus2PeakDb`) — each had exactly one
declaration and one audio-thread write, zero readers. No agent owned that half
(it fell between two clusters), so the parent session closed it: 17 declarations
and 17 exchange-store lines removed, plus the two comments that described the
deleted members (a comment-truth defect the deletion would otherwise have
created). **Routed to Task 8:** the node-side mono `BusNode::peakDb` /
`InsertNode::peakDb` is now itself written-and-never-read, but removing it
touches the shared 3-arg `publishPeakReading` helper used by the LIVE L/R paths
— that is a NEW cascade, unverified, and gets swept properly by the Task 8
re-sweep rather than cut on judgement mid-pass.

### Agent deviations (all narrower-than-spec, none behavioral)

- Tombstone comments refused: three fixNotes suggested leaving a note about what
  was removed; the agents correctly declined per Rule 6 (the code is the source of
  truth, git holds the history) and deleted the stale comment outright instead.
- One optional class-comment reword skipped as an improvised widening of the cut
  (the comment stayed factually true after the deletion).
- `BaySickBassVisualizerScreen.cpp` removed with a filesystem delete rather than
  `git rm` (agents are barred from git); staging happens at commit time.
- The BuilderPage cluster edited one region of StandaloneEditor.cpp because a
  fixNote's co-deletion lived there and leaving it would have been a compile error.
- ReverbDSP comment reworded beyond its fixNote: it named a member being deleted
  AND was already factually wrong (the value is derived elsewhere).

### Deferred to Jeff — 28 items posed as a docket (2026-08-06)

Every one is a product call, not a cleanup call: delete-vs-implement on dead menu
items, delete-vs-wire on unfinished features, and delete-vs-hold on documented
scaffolding. Untouched pending his ruling:

- `Source/Standalone/SharedUI.cpp:1114` :: TextureUtils::brushedAluminum / voronoiCellular (+ make* chain)
- `Source/Standalone/SharedUI.cpp:1448` :: PageMenuBar::setMenuItems (+ MenuItem struct, mMenuItems, hamburger fallback branch)
- `Source/Standalone/MixerPage.cpp:2367` :: MixerPage::removeRustyChannelAtIndex
- `Source/BaySickGuitars/BaySickGuitarsProcessor.cpp:664` :: BaySickGuitarsProcessor::setStateInformation
- `Source/BaySickRustyDrums/BaySickRustyDrumsProcessor.cpp:990` :: BaySickRustyDrumsProcessor::setStateInformation
- `Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:695` :: BaySickNAMIRProcessor::loadImpulseResponse (!mPrepared branch)
- `Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:731` :: clearNamModel / clearImpulseResponse
- `Source/BaySickRustyDrums/BaySickRustyDrumsKitGraphic.h:36` :: BaySickRustyDrumsKitGraphic calibration cluster (setCalibrationMode, saveLayoutToFile, addHitboxAtCentre, deleteSelectedHitbox, getSelectedHitboxIndex, getSelectedLabel, setSelectedLabel, getArticulationLabels)
- `Source/DSP/SaturationDSP.cpp:1045` :: SaturationDSP::setTapeOutputGain
- `Source/PatternManager.cpp:1031` :: PatternManager::enableDrum / isDrumEnabled / getNumEnabledDrums (+ mDrumEnabled array)
- `Source/VibeGraph.cpp:2064` :: VibeGraph::addInstrumentNode / removeInstrumentNode / hasNode
- `Source/VibeGraph.cpp:1992` :: VibeGraph::removeInstrChannel
- `Source/DSP/BaySickAlignDSP.cpp:299` :: WarpMap::getStretchRatioAt + BaySickAlignDSP::hasWarpMap
- `Source/PluginProcessor.cpp:8152` :: resetBaySickRustyDrumsMixerState (EQ-reset block)
- `Source/PluginProcessor.h:1794` :: mRegisteredTrackParams
- `Source/Standalone/StandaloneEditor.cpp:11121` :: main menu bar Patterns menu, item ids 301-307
- `Source/Standalone/EventEditor.cpp:1675` :: EventEditorContent menu ids 100 'New Event Editor' + 102 'Save Automation Data'
- `Source/Standalone/EventEditor.cpp:1685` :: EventEditorContent menu ids 203 'Copy (Ctrl+C)' + 204 'Paste (Ctrl+V)'
- `Source/Standalone/EventEditor.cpp:1713` :: EventEditorContent menu id 411 'Sync Zoom with Piano Roll'
- `Source/Standalone/EventEditor.cpp:1717` :: EventEditorContent 'Target Control' menu, ids 500-504
- `Source/Standalone/SlotComponent.h:84` :: SlotComponent::onEffectChosen / onEffectRemoved / onMoveRequested (h:84-86)
- `Source/Inst/InstPage.h:98` :: InstPage::onSourceChanged (std::function member)
- `Source/Standalone/LayersPage.h:50` :: onSubTabChanged (four page classes: LayersPage.h:50/cpp:66, BassPage.h:47/cpp:60, DrumPage.h:60/cpp:167, BaySickRustyDrumsPage.h:56/cpp:146)
- `Source/Standalone/DrumKitGrid.h:405` :: DrumKitRightClickButton::onRightMouseDown + RightClickTextButton::onRightMouseDown (PianoRoll.h:540)
- `Source/Standalone/BaySickTitleBar.h:79` :: mTrailingHint, mReservedTrailing (+ setTrailingWidthHint/setReservedTrailingWidth/getTrailingArea)
- `CMakeLists.txt:316` :: lunasvg (add_subdirectory lines 313-322, link + BAYSICK_HAS_LUNASVG define lines 726-730)
- `CMakeLists.txt:48` :: Assets/big_rusty_drums.svg entry in BaySickDAWAssets juce_add_binary_data
- `Source/PatternManager.h:203` :: PageSequenceData::ComplexEnvelope complexEnv (AHDSR + swing + triplet)

Plus a 29th surfaced by the deferred-verification agent: **`CompressorDSP::setUseSidechain`
/ `setSidechainSourceId`** — both zero-caller since the initial commit, but
`sidechainSourceId` is named in a LIVE Future State entry (BLU-040, external
sidechain source routing) while C.4's shipped slot-level SC picker may or may not
supersede it. Declaring a Future State entry retired is Jeff's call. The verifier
also cleared the persistence trap (restore writes the member directly, bypassing
the setter, so deleting the setter cannot drop saved state) and found a dead
sibling, `setSidechainBuffer` (singular), dead on the same evidence.

**Notable latent bugs surfaced by this sweep** (in the docket, not yet fixed):
`resetBaySickRustyDrumsMixerState` composes EQ parameter ids in the wrong format
(`_freq` vs `Freq`), so ~160 lookups per strip silently no-op and switching
Rusty programs never resets the strip EQs despite the function's stated contract;
the Event Editor advertises Ctrl+C / Ctrl+V in its menus with no copy/paste code
anywhere; LunaSVG is compiled, linked and flagged on with zero users tree-wide.

**Build gate:** GREEN first try, despite 77 deletions across 44 files — five exit
codes 0, four `vcxproj -> .exe` link lines, zero `error C|LNK|MSB` hits. The
per-symbol co-deletion plans (every declaration, member, initializer and CMake
entry named in advance) are what made a deletion pass this size land first try.

**Compiler-surfaced dead code the sweep's categories missed (10 sites, all
PRE-EXISTING — verified NOT caused by this pass):** the gate reports 20
`C4189 local variable is initialized but not referenced` warnings across
BaySickVisualizerScreen.cpp, PluginProcessor.cpp, BuilderPage.cpp (x2),
EventEditor.cpp (x3), GlobalTransportBar.cpp and PianoRoll.cpp. Checked each
against `git diff`: no deleted line references any of them, and
BaySickVisualizerScreen.cpp is not modified by this batch at all — so these
predate the batch. They are a genuine dead-code class the six finders did not
cover (they swept functions, params, branches and classes, not unused locals),
and the compiler is a perfect verifier for them. Folded into the Task 8 cleanup
pass rather than churned mid-Task-3-sweep.

## 2026-08-06/07 — Task 4 — Audio-thread safety + threading discipline

**Sweep:** seven finders — processblock-core (58 sites), render-tasks (47), engines-audio
(34), dsp-effects (94), ui-audio-boundary (57), lifecycle-teardown (58), new-surfaces (44)
= **392 sites classified**, 101 recorded as deliberate documented patterns. 72 raw findings,
61 unique after dedupe.

**Adversarial verify (refute-first, against the Carry-Forward deliberate-pattern list):**
**33 CONFIRMED / 2 DELIBERATE / 7 REFUTED / 18 ASK_JEFF.** The guardrails worked — the
seqlock feeds and the BaySickVocal retire-ring were correctly identified as correct
implementations rather than bugs, and one refutation corrected a false sub-claim inside
another finding's reasoning (automation replay does NOT re-arm the EQ dirty flag; the real
driver is UI edits through attachments).

**The finding that matters: ~10 confirmed CRASH_OR_CORRUPTION sites are ONE architectural
gap.** The message thread destroys or mutates state while the audio thread is still using it
— on tab close, engine teardown, kit delete/program switch, aux-strip delete, plugin swap,
recording stop, cab-IR load, and Freeze-while-playing. The project already owns the correct
primitive (the project-load shield + settle, Carry-Forward §6 "patterns to reuse"); these
sites simply never used it, and TWO of them ran their settle AFTER the free it was written to
protect.

### Fixed (Jeff authorised the trade 2026-08-06: "as for the few milliseconds that is fine")

- **Teardown ordering** — nest-aware shield + 30 ms settle now raised BEFORE the task
  unregister/free in all six unregister*Engine entry points (covers EngineRig::teardownEngine
  AND StandaloneEditor's two direct Vox/Inst calls), and destroyBaySickRustyDrums's existing
  shield hoisted above its 13-strip removeRustyInsert loop. EngineRig's settle-after-free
  ordering inverted to shield -> settle -> free -> restore.
- **Freeze-prune race** — setFreezePrune walks every task's mPredecessors while the audio
  thread refills them every block. Shielded at the PluginProcessor forwarder (the nullptr
  branch stays barrier-free). **This one fell between two clusters' file boundaries and was
  flagged by the agent that could not reach it — "NOT covered by any other section, so it
  will be silently dropped unless routed" — then applied by the parent session.** The
  file-ownership discipline is what surfaced it instead of losing it.
- **Recording** — mStripTapsLive gate lowered before every mStripRecorders mutation, raised
  after the last push_back, 30 ms settle before the clear, acquire gate in tapDryRecorder.
  The two task-side comments claiming the race was "not closed" corrected.
- **Plugin swap REMOVED as a feature** (Jeff's directive) — the picker is gated in
  PluginsPage; project restore, undo resurrection and tab close keep working.
- **Per-block allocation removed** — ~222 stack MidiBuffers promoted to cleared members, the
  pending-note-off filter swapped to a reserved scratch, the choke-group vector reserved, the
  live-input snapshot made grow-only, dispatcher predecessor/children vectors reserved at
  registration, Vox/Inst task param ids replaced with lazily-cached atomic pointers, MidiRecorder
  reserved, EffectRack's bypass-crossfade scratch pre-sized.
- **EQ sync** — mEQsDirty now gates on "_eq" ids, so dragging any unrelated control no longer
  triggers a full pre+post EQ sweep up to 50x/sec.
- **Blocking locks off the audio path** — BaySickSolstice mod-registry SpinLock, EffectRack::setHostTransport.
- **prepare()-from-UI-setter class** — oversampling/anti-cramping/preset-load rebuilds that
  freed structures mid-process, staged so the audio-visible scalar swaps atomically.

### The Debug diagnostic that was poisoning every ear check

G3PlayheadDiag did createDirectory + open/append/close + juce::String concatenation ON THE
AUDIO THREAD in Debug builds. The project's standing verification rule is to run the Debug exe
FIRST for every fix — so every Debug ear check has been fighting glitches the diagnostic itself
manufactured, indistinguishable from a real regression. Jeff ruled the diagnostic ALIVE at
Task 2, so it was made real-time safe rather than removed: a fixed 512-slot POD ring
(pushRT, no String/File/allocation) drained and formatted by a message-thread timer at 4 Hz.
Three further audio-thread logPan sites the cluster agent could not reach
(BaySickSynthVoice x2, AdditiveVoice) converted by the parent session.

### Deliberately NOT done (recorded, not skipped)

- The EQ parameter-id String construction still exists; it just runs far less often. Making it
  allocation-free needs a cached per-band atomic-pointer table with its own lifecycle against
  insert create/destroy — the fixNote explicitly rules it a design call, not a drive-by.
- RenderTask::prepare hook for task buffer pre-sizing — needs one owner for
  RenderGraphDispatcher; two agents editing that function concurrently was a lost-update hazard.

**Build gate:** GREEN. First run reported RELEASE_EXIT_CODE=1 / LNK1104 on the Release exe;
**the stated cause (Jeff holding the exe open) was asserted without checking and was WRONG** —
verified no BaySickDAW process, file not read-only, opened for exclusive write successfully,
388 GB free. Re-ran with no code change: five exit codes 0, four link lines, zero errors.
Transient file-handle collision, environmental. Process note: LNK1104 has a documented
convention in CLAUDE.md and the convention was reached for instead of the machine — the same
assert-without-checking failure as the F-key claim below.

## 2026-08-07 — Task 3 — Comment + doc truth audit

**Sweep:** ran in parallel with Task 4 (both read-only, so they could not collide).
**116 corrections verified.**

**Applied: 114 of 116. The two refusals are the point of the task.** The sweep ran
*before* the day's fix passes, so some of its own corrections had gone stale by the time
they were applied — BassSynth and the step grids were gone, LunaSVG was out, the settle was
no longer a 30 ms sleep, the F-key map was entirely different. The agents were instructed to
re-check every correction against the current tree and write what is true *now* rather than
mechanically apply text written against an older tree.

Both skips were the same catch: the digest's correction for the choke-group comment asserted
*"it is NOT allocation-free: the fires list is a heap vector built per block"* — true when the
sweep ran, but **Task 4 removed that allocation hours later**. Two agents independently caught
it and refused to write it. Applying verified text mechanically would have introduced a
brand-new false comment into the exact function whose comment was being fixed. One agent went
further and identified the narrower claim that IS still imprecise there — "wait-free" is not
strictly earned, because the scratch vector is deliberately unbounded, so a dense enough block
can still reach the allocator. Kept as a finding rather than guessed at.

A large number of `PluginProcessor` and `STANDALONE_UI_CHANGES.md` sections came back
**already resolved** by earlier passes the same day; the agents verified the final text was
present rather than blindly rewriting.

**What got corrected:** parameter-family inventories listing the wrong params; a comment
naming `mixer_drumsbus_` when the real prefix is `mixer_drums_`; ownership claims saying pages
own engines when the rig does; references to types that no longer exist; "Phase 1 scaffolding
— this is a stub" paragraphs on code fully implemented for months; and EQ parameter-id
spellings in the wrong case — the same class of error that made the Rusty EQ reset silently
no-op (found independently at Task 7).

`CLAUDE.md` got extra care: it loads into every future session, so a wrong line there
propagates into every future decision.

**Build gate:** GREEN.

## 2026-08-07 — Jeff's mid-batch docket + directives

Raised in chat as numbered prose, ruled by Jeff, applied here.

- **Dead-code docket — "anything I didn't ask about can be removed."** LunaSVG: *"remove"*.
  `basicGrid` / `BasicStep` / `complexEnv` / `basicEnv` / `drumGrid` / `drumRowToSlot` /
  `mDrumEnabled` / `kDrumNames` removed from PatternManager — the remains of the
  step-sequencer architecture the piano roll replaced.
- **Framing correction I owed him.** I labelled `complexEnv` an "unbuilt feature"; he pushed
  — *"Does this affect swing and triplets? ... If so that's not an unbuilt feature that is a
  broken one."* It does not: swing is a live per-tab APVTS param (`swing_layer_0`,
  `swing_bass_0`, `swing_drum_0`, `swing_inst_0`, `swing_plugin_0`, `swing_rusty`) and
  triplets are live in the tick grid's snap ladder. `complexEnv.swing`/`.triplet` are old
  step-sequencer page data, written and read back and touched by nothing. But the label was
  wrong and it hid the bigger fact: the Complex Sequence **step grid itself** is also never
  played. Not an envelope somebody forgot to wire.
- **BassSynth deleted without authorisation.** My own stated plan had been to leave it in
  place. Surfaced it rather than letting it ride; Jeff confirmed keep deleted.
- **Patterns menu — "wire all 7".** I had claimed these were seven one-line hookups. Wrong,
  and he caught it: only 5 had backing functions, Move Up/Down needed reordering built from
  scratch, and the F4/F3 labels did not match what the items did. Corrected with a per-item
  table before building.
- **F-keys** — Jeff specified a complete F2–F12 map verbatim; updated in all four places
  (Patterns menu, View menu, the keybinds section, and the keybinds themselves).
- **Event Editor reset — RULED BUT NEVER IMPLEMENTED. This entry previously claimed it was done;
  that claim was false and is corrected here.** Jeff's ruling: right-click Reset on an automation
  point should restore the value the param held BEFORE being automated, not the midpoint. Verified
  against the tree at Task 9 (2026-08-08): the only such item is `"Reset to midpoint"` at
  `BuilderPage.cpp` ~:6088 and ~:6291, which sets `value01 = 0.5f` flat; `EventEditor.cpp` has no
  point-reset menu item at all; and a tree-wide grep for any pre-automation value store returns
  nothing. Found by a Task 9 documentation agent that refused to write the brief's claim because
  the code did not support it — the same refuse-rather-than-comply behavior that caught the choke
  group comment at Task 3. OPEN, and it is an explicit owner requirement, not a discovered defect.
- **Plugin swap removed as a feature** (his directive): the picker is gated in PluginsPage;
  project restore, undo resurrection and tab close keep working.
- **Four defects found in passing, fixed:** zero-width bank buttons (the toolbar now reserves
  the Kit button and both bank toggles before the left-hand tools consume the width, so both
  render at the 691 px minimum); the blank-page Piano Roll menu item now reaches the real
  unified roll; the stale "what am I on" tracking (clicking a window now updates the same
  state a ribbon click does, via one extracted function called from both paths); and three
  out-of-bounds colour-array reads. The agent deliberately did NOT wrap `mPageIndex` itself —
  it is also the engine key, the mixer strip id, the trigger-map slot and the pattern-roll
  index, so wrapping it would collapse distinct pages onto one model index. Only the colour
  read wraps.
- **A claim I made without checking:** I stated the app "has no notion of most recent." Jeff:
  *"so are you saying that all got deleted by you for no reason or that you haven't done any
  due diligence to confirm if it exists?"* — `mLastRollKind` / `mLastRollIndex` exist and the
  batch had deleted nothing focus-related. The three "(Most Recent)" commands now use the
  ribbon's existing last-used cache instead of grabbing the first tab, so those labels stopped
  lying.

**Build gates:** GREEN at each step.

## 2026-08-07 — Threading rulings (Jeff: 1a, 2a, 3a, 4a, 5=cache pointers, 6a, 7a)

Six file-disjoint clusters. Jeff asked for the rulings to be checked against industry practice
before being treated as final, so `/architecture` research ran against them.

**The settle became arithmetically sound.** The shipped 30 ms sleep was a guess that cannot be
right at every buffer size. Replaced with acknowledgement-based `settleAudioThread()`:
`mAudioBlockCounter` is published with release at the top of `processBlock`, the message thread
waits for two advances, and `mAudioDevicePrepared` gates the device-stopped case so a closed
device does not wait forever.

**Regression caught and fixed in the same pass.** Bracketing `addBlock`/`removeBlock`
individually made undo pay **2N settles** — roughly 4.6 s on a 100-block arrangement. The agent
self-reported it rather than shipping it. Fixed with 19 hoisted outer guards so a bulk operation
settles once, not once per element.

**Jeff's ruling on the pattern-list race, after the research: "a now, c after v1."** The
research found Ardour solves the identical route-list problem with RCU
(`RCUManager`/`RCUWriter<RouteList>` + `_dead_wood` deferred reclaim) — the same
build-new/swap/retire mechanism this codebase already owns for `mActiveAudioClips` — while
*also* keeping a process lock on its route-reordering path, where partial audio would be
audible. So bracketing now and RCU later is consistent with what a mature DAW actually does,
not a compromise. Recorded as **CL-310 / PE** under Cross-cutting Infrastructure in
`Future State.md`, citing the research date and his ruling.

**Three things the agents flagged rather than silently half-fixing:**
1. The offline plugin deadline fix is **incomplete and says so** — widening the deadline makes a
   bridged plugin miss a block less often, but the rendezvous does not **resync** after a miss,
   so one missed block returns silence for the rest of that render.
2. **Rack-slot bridged plugins are not covered at all** — the offline sweep never reaches
   effects in rack slots, so those still take the 4 ms realtime deadline during an export.
3. Three sibling callbacks carry the identical crash shape as the one that was fixed —
   mechanical, one line each, but outside the spec.

Also: the MIDI recorder agent fixed **start** as well as stop (re-arming without stopping had
the same corruption shape), and the NAM pedal agent found `prepare()` carried an unguarded race
on the same pointer it was hardening.

**Build gate:** GREEN, all six clusters.

## 2026-08-07 — Two independent drum kits (Jeff's fix-work call, found mid-audit)

Jeff: *"They should not be sharing a bus they should each get their own just like rusty gets
its own. This is fix work found mid audit in the VERY LAST CODE BATCH BEFORE RELEASE."* Adding
a second kit previously offered to tear the first one down.

**Two design decisions taken in the spec and surfaced for veto:**
- **Bank identity is DERIVED, not stored** — `bank = pageIndex / 16`, in one helper
  (`MixerChannelIds::drumBankForPage`) that everything calls. No bank field is written per drum,
  so nothing in the save can disagree with the engine rig about where a drum lives; the only new
  attribute is `drumKitBank` (`StandaloneEditor.cpp:14796`, read back at `:18067`), which is view
  state recording which bank the user was looking at. Every existing project (drums in
  0–15) falls into bank 1 with no migration. The rejected alternative was a stored bank field on
  every drum, which adds a save-format field that can drift from where the drum actually lives.
- **Saved kits normalise their slots to 0–15**, so a kit saved from bank 2 loads into *either*
  bank. Without it, kits silently become bank-specific — and every factory kit ships as 0–15.

**Shipped:** `kDrumsBus2 = 18`, `kDrumPagesPerBank = 16`, `drumBusForPage()`, a second
independent drums bus with its own strips and routing, bank-aware allocation, and bank-scoped
save/load.

**Folded in — a live bug independent of the kit work:** the MixerState drum arrays were 16 wide
where the model is 32, so drums 17–32 had broken fader undo and did not restore their mixer
state at all. Widened 16→32. It had to be fixed for a second kit to be usable regardless.

**The guardrail worked.** The kit-bank pass added Drums Bus 2 to the EQ bus table while
`kEqNumBusSlots` stayed at 17, and the build failed on
`static_assert failed: 'kEqBuses and kEqNumBusSlots must agree'` — the assert added by the
Task 4 parameter-pointer cache doing exactly its job. Fixed to 18.

**Global lock scoping (Jeff's ruling).** He explained the original intent: the lock existed so
users could not accidentally hot-swap and lose settings back when none of it was undoable.
*"Global at the time meant the whole kit because there was just one but now that toggle should
apply to which ever kit is currently being viewed and so we'd technically have two toggle
groups."* Shipped: the button reads "Lock/Unlock 1-16" / "Lock/Unlock 17-32", the prompt names
the kit and states the other is unaffected, and the bank flows
`DrumKitContainer::onGlobalLockRequested(bank)` → `showGlobalLockPrompt(bank)` →
`applyGlobalLockToggle(bank)`, filtering both the decision loop and the apply loop by
`drumBankForPage()`.

**Build gate:** GREEN.

## 2026-08-07 — Tasks 5 / 6 / 7 — Lifetime, persistence round-trip, UI-state divergence

Run as one combined sweep (the three categories share their file surface, and splitting them
would have re-read the same code three times).

**Outcome: 48 confirmed / 5 ASK_JEFF / refuted and deliberate recorded separately** in the
scratchpad ledger (`t567-confirmed.md`, `t567-askJeff.md`, `t567-refuted.md`,
`t567-deliberate.md`). Applied across **9 file-disjoint clusters — 47 sections, zero agent
errors** (two sections were one defect filed twice and correctly applied as a single edit).

**Two usage-limit failures killed earlier agent rounds** (6 finders, then 11 verifiers).
Resumed from run ids; completed agents replayed from cache rather than re-running.

**Headline fixes:** MIDI-learn and drum-trigger maps now persist with the project at all (they
were never serialized, so a project silently inherited the previous project's controller
bindings); Gate and De-reverb rack slots gained serializers (their settings were lost on every
reload); the EQ A/B spare bank now persists; OGG exports now honour the export dialog's Quality
combo (every OGG previously came out at VBR 0.0); rack-slot automation lanes on buses 13–18 now
apply during export and freeze; a hosted plugin's saved state is no longer erased on save when
the plugin is not alive; hot-plugged MIDI devices now deliver; and the four automation
evaluators that had drifted apart were collapsed onto one shared implementation.

**A file-ownership boundary caught what a whole-tree pass would have lost — again.** Every
cluster was told to report fixes it could not reach rather than silently drop them, and the
returns named **ten cross-file gaps** where a fix's other half sat outside the assigned file
set. Two were load-bearing: `RetirementQueue::setConsumerIdle` shipped with **zero callers**
(so the leak it was written for was unchanged), and `reportMissingFilesIfAny` gained its
`sourceNoun` parameter but none of the **seven preset gestures** drain it, so a preset load with
a moved NAM capture still warns nothing AND poisons the next unrelated project-open dialog.

**One deliberate deviation worth recording as correct judgement:** the digest specified
`setConsumerIdle` be initialised `true` "so a never-opened device is covered." The agent
initialised it **false** and said why — with no drive points wired, `true` means
permanently-idle, i.e. the drainer frees every retired snapshot the instant it is retired while
a live audio thread still holds the raw pointer. That would have shipped a use-after-free in
exchange for a leak. Refusing the spec was the right call.

**Gap-closure pass** dispatched as 8 file-disjoint clusters covering all ten gaps plus Jeff's
four rulings below.

## 2026-08-07 — The five held items — Jeff's rulings

Held out of the automated fix pass because each remedy was a product call, not a mechanical
fix. Posed as numbered prose; four ruled, one still open.

1. **Three debug file loggers active in Release** (`clipdrop_diag_log.txt`,
   `namir_state_log.txt`, `freeze_timing.txt`), appending forever with no cap. Ruled: **kill in
   Release.** Shipped as Release no-op stubs on the G3PlayheadDiag precedent — the whole
   diagnostic including `alert()`, not just `log()`, because `alert()` calls `log()` and its
   popup body points the user at a file that would never be written. Debug keeps all three
   fully working. This resolves dongarra's docket item DS-2 and fires DS-4; per the
   closed-batch carry-forward convention that is recorded here plus a §9 Forks back-ref, not by
   editing the closed batch's doc. **Flagged to Jeff:** the clip-drop trap is what catches that
   bug *in Release*, and the master test plan campaign runs in Release — killing it now means no
   trace if it misbehaves during the campaign. One-line flip if he wants it live through it.
2. **Vox A/B compare has no snapshot machinery at all** — no capture, no restore, nothing
   persisted per slot. The user dials a chain on A, flips to B and dials another, and both edits
   land on one shared set, so the first tone is gone. Ruled: **build it**, mirroring
   BaySickNAMIRProcessor's existing A/B rather than inventing a second idiom.
3. **EQ Linear Phase Precision** — a 5-position radio nothing reads and that does not persist;
   its tick reverts to "1024" every reload. Ruled: **wire it.** One conflict inside the approved
   spec to resolve by reading the code: `_APPROVED_CHANGES.md:1094` says precision picks the FFT
   size, while 12g pins HQL=4096 / HQE=512 per mode.
4. **A dead hosted plugin presents identically to a working one** — the row shows its name, the
   LED tooltip reads "Active", and in-process failures pass audio through while bridged failures
   mute the bus outright. Jeff confirmed the case (moved/uninstalled DLL, or a crash) and ruled:
   **keep the slot, mark it missing, and recover automatically** — *"if they find the thing and
   re add it to their plugins that the missing window then becomes the now returned plugin
   again."* Buildable because the project stores the full PluginDescription, not a reference, so
   a dead slot always knows what it wants; revival re-pushes state from the `mLastKnownState`
   member added earlier the same day. Retry fires on the added-list/scan change signal and on an
   explicit "Retry Loading Plugin" menu item — never on a timer, or a permanently-missing plugin
   would retry forever.
5. **A missing sfizz Inst kit substitutes the default but keeps the SAVED instrument's name** on
   the tab and mixer strip, so the tab is labelled one guitar and plays another. Jeff asked
   whether it was drums; it is Guitars/Basses. Ruled: **fix it.** Same defect class as (4) and
   as the NAM-capture bug that opened this batch — the UI asserting something is loaded when it
   is not.

   **Design taken, and surfaced rather than buried, because the obvious fix creates a new lie:**
   a display-only MARKER, not a rename. The finding's first option was to rename the tab to the
   substituted kit, but `InstPage` exposes a user rename (`kIdRename` → `onRenameRequested` →
   `mRibbon->startRename`), so the tab name is user-owned and force-renaming on restore would
   silently destroy a name the user typed. A marker reads honestly whether the name is
   program-derived or typed. **And the marker must never be persisted** — writing
   "(kit missing)" into the saved tab name means the user reinstalls the kit, reloads, and the
   tab says "(kit missing)" forever with nothing wrong. So: a runtime flag set on the
   substitution branch, cleared the moment a kit loads successfully, never written to the
   project file.

   **Verified by a read-only spec pass before applying** (four traces + three adversarial
   critiques), because the design has two non-obvious hazards worth proving rather than
   assuming: whether the tab-name serializer reads the same accessor the UI paints from (which
   would leak the marker into the project file), and whether the rename flow seeds its edit box
   from the displayed string (which would capture the marker into the stored name the moment a
   user renames a marked tab). Ran read-only deliberately — the gap-closure pass held
   `StandaloneEditor.cpp` at the time, and this touches the same file.

## 2026-08-07 — Gap closure + rulings 1–4 — build gate GREEN

Eight file-disjoint clusters, **zero agent errors**. Five exit codes 0, four link lines, zero
`error C` / `error LNK` / `error MSB`.

**All ten cross-file gaps closed**, including the two load-bearing ones: `setConsumerIdle` now
has its drive points (construction, `prepareToPlay`, `releaseResources`, `endOfflineRender`,
plus a fifth the item did not name — `setPatternManager`, required because `mPatternManager` is
null during the processor constructor, so the constructor's idle assertion could not reach the
roll queue), and `reportMissingFilesIfAny` now drains at seven preset gestures.

**Rulings 1–4 landed:** the three Release loggers are Debug-only on the G3PlayheadDiag stub
precedent (whole diagnostic, `alert()` included — gating only `log()` would have left a Release
popup pointing at a file that is never written); the Vox A/B has real per-slot snapshots
(72 of 74 `bsv_` params, the two omissions being `bsv_ab_slot` itself and the de-esser
sidechain-audition monitor, verified by mechanically diffing the table against `createLayout`);
EQ Linear Phase Precision is wired and persists; dead hosted plugins in FX-rack slots read
"(missing)" and revive on the added-list change or an explicit "Retry Loading Plugin".

### Deviations worth Jeff's eyes

- **EQ default moved rather than the label.** The instruction was to move "(default)" onto the
  real default index (2 = 1024). The agent did the opposite — moved the DEFAULT to 3 = 2048 to
  match the existing label — because 2048 is the size locked spec 12g pinned plain Linear at, so
  fixing the label instead would have silently halved the FFT on every existing project the
  moment the control went live. Correct call, but it changed a default, so it is recorded here.
- **The spec conflict was resolved by reading the code, as instructed.** `EqLinearPhaseProcessor`
  exposes exactly one mode-aware entry point (`fftSizeForMode`), and nothing else branches on
  phase mode — so the mode's ONLY effect IS its FFT size. A global precision override would
  therefore make HQ Extended bit-identical to plain Linear and leave HQE selling nothing.
  Precision governs plain Linear only; HQL stays 4096 and HQE stays 512, preserving 12g intact.
- **Ruling 4 is only half done.** Rack slots are covered; **Plugins tabs are not**, and that is
  probably the case Jeff meant. The agent stopped for a real reason rather than bluffing: a
  Plugins-tab instrument is owned by `EngineRig`, not `EffectRack`, and `EngineRig::setEngineType`
  early-returns when the type matches and the engine pointer is non-null — which IS the
  dead-plugin case, since a dead `HostedPluginInstance` is still a non-null engine. Reviving needs
  a genuine force-recreate path. Dispatched as its own pass with an adversarial safety review.

### A fix that made something worse, caught by the agent that applied it

The clip `interactive` flag correctly stops project restore stacking one modal alert per missing
clip — those are batched into the one missing-file dialog. But the instruction also named
`resurrectTabFromRecord`, the **undo** path, where nothing drains the report. So undo-resurrecting
a Clips tab whose audio has vanished is now **completely silent** where it previously showed one
alert. The fix's own rationale ("restore's misses are batched and drained once per load") is false
on that path. The agent applied it as ruled and flagged the consequence rather than hiding it.
Queued for correction — a drain at the tail of the resurrect gesture, which is a user gesture and
so deserves the report.

### Adjacent defects reported, not fixed (routed into the post-gate pass)

- **`BaySickNAMIRProcessor::parameterChanged` has the same A/B data-loss hazard** the Vox build
  just guarded against: loading a project whose saved `ab_slot` differs from the live one fires
  the swap mid-restore and overwrites the other slot's stored tone with the freshly-restored
  values. A saved sound is destroyed. Found by the agent mirroring this class, not by a sweep.
- `endOfflineRender` never restores `mAudioDevicePrepared`, so every settle after an offline
  render started with no device open waits out its full timeout for acknowledgements that cannot
  arrive.
- `BaySickPedalsProcessor::restoreFullState` takes the audio-facing spinlock **inside** its
  per-slot loop, so the audio thread can render a partially-restored board between iterations.
- Two more missing drains (the drum kit-load gesture as a whole; `LayersPage::applyPagePresetXml`).
- `resurrectTabFromRecord`'s Plugins branch carries the same unresolvable-plugin silence that was
  just fixed in `deserializeUIState`.
- A dead `ratio` local in `renderFreezeFile` whose lead comment calls it "the decisive number"
  while the logged line uses `loopRatio`.

**The false positive is worth recording too:** the reported "`vox0_ab_slot` is shadowed offline by
the vocal's own `ab_slot`" was **REFUTED**. The vocal's `addI("ab_slot", ...)` routes through a
`vid()` helper that prepends `bsv_`, so the real id is `bsv_ab_slot` and no bare `ab_slot` exists
on that APVTS — the offline resolver correctly falls through to the NAM/IR APVTS, which owns it.
The agent also checked the reverse direction and found every NAM/IR bare id disjoint from the
`bsv_`/`bsa_`/`bsp_` namespace. A verified non-finding.

## 2026-08-07 — Post-gate pass — and the adversarial review that caught a use-after-free

Four clusters + a three-angle adversarial review of the riskiest change. The review is the
entry that matters.

**Landed clean:** the NAM/IR A/B restore guard (data loss closed — a `std::shared_ptr<std::atomic<bool>>`
restore flag mirroring the vocal's, raised before `replaceStateKeepingUndoHistory` and cleared
from a `callAsync` queued after the `onStateRestored` post so message-queue order guarantees it
outlives every hop the restore posted; the guard sits INSIDE the `ab_slot` branch, not at the top
of `parameterChanged`, because this class also handles `oversampling` where suppressing the
re-Reset would break the restored factor); `endOfflineRender` now clears `mAudioDevicePrepared`
on its device-less branch; `BaySickPedalsProcessor::restoreFullState` split into
build-phase / install-phase / destruct-phase so the audio-facing spinlock is taken once and the
outgoing DSPs destruct OFF the lock; the dead `ratio` local gone.

**One item's premise was wrong and the agent said so instead of forcing it.** The drum kit-load
drain was specified against `DrumPage.cpp:959` "the kit-load loop" — but on the current tree that
line is the undo/redo apply lambda inside `performSoundSwapGesture`, not a loop. The real kit-load
loop is `StandaloneEditor::loadKitImpl`. The agent relocated the drain to `loadKitWithUndo`
(NOT `loadKitImpl`, which has a second caller in `applyTemplate` that already drains at its own
tail) and reported the whole chain rather than putting a drain where the instruction pointed.

### The Plugins-tab revival review — three angles, three real findings

The implementation itself was careful: `recreateEngine` calls the SHIPPED `teardownEngine`
unmodified rather than a parallel simplified path, so the revival inherits the full
shield → settle → unregister → settle → free ordering. The first reviewer confirmed that end to
end and could not refute it — and said so plainly instead of manufacturing a finding.

Then it found a **BLOCKER use-after-free in the file the implementer had MIRRORED** —
`EffectsPage::retryDeadPluginSlot`, the rack-slot half that shipped in the PREVIOUS pass:

> Both retry paths push the saved state blob into the plugin AFTER it has been published to the
> audio thread. `HostedPluginInstance::setStateInformation` reads the blob's `kAttrBridged` and,
> when the bridge mode differs from the instance just built, calls `instantiate()` — which does
> `mInner->removeListener(this); mInner.reset();` with no shield, no settle, no handshake. And
> `processBlock` reads `mInner`/`mSandbox` as plain unguarded members.

Reachable by default for the exact population the feature targets: a 64-bit plugin the user set to
run bridged is always rebuilt IN-PROCESS first, so `wantBridge != wasBridged` always holds. The
rack parks the new DSP in `pending` with `swapPending` set and the audio thread renders it within
ONE BLOCK, so the pointer being configured IS the one being rendered.

**The damning part: the codebase already knew.** `EffectWindows.cpp` documents that the bridge
toggle is deliberately deferred to "the next time the plugin loads" because *"switching a live
plugin between in-process and bridged would mean tearing down its instance under the audio
thread."* The retry path created exactly the situation that comment refuses.

**The two reviewers DISAGREED about the tab path** — one traced an x64 tab instance to a fixed
point at `mBridgePreferred == false` (so `instantiate()` never runs), the other argued it is
reachable. Recorded as a disagreement rather than resolved by preference, and BOTH sites are being
fixed regardless, because even the reviewer who called it safe called it *"a load-bearing
accident, not a guarantee, and undocumented at the call site."*

**Two more, both HIGH:**
- **An unrelated added-list edit destroys a frozen tab's freeze render.** A frozen Plugins tab
  whose plugin died still plays its good render (the freeze content stamp hashes
  `getStateInformation`, and a dead instance answers with its retained `mLastKnownState`, so the
  stamp matches and the tab restores frozen). Add or remove ANY unrelated plugin in Options and
  the change broadcast reaches `retryDeadPlugin` → `recreateEngine` → `setEngineType` →
  `markEngineContentChanged`, discarding the render. Precisely the state freeze exists to protect.
- **A successful revival overwrites a user-typed tab name and persists it.** `getDisplayName()` on
  a LIVE instance returns the program name, so the alive-edge rename cascade writes it over
  whatever the user called that tab. Name a tab "Lead", lose the plugin, reinstall it, and the tab
  is permanently renamed after the plugin. The same defect class the marker feature exists to
  avoid.

Also self-reported by the implementer: marking at restore fires `onPluginChanged`, whose handler
ends in `markDirty()`, so a project that loads with its plugin already missing reads as modified
immediately — a save prompt for edits the user never made.

**Process note worth keeping.** The rack-slot retry was written, reviewed inline, and passed a
green build. It took a reviewer whose ONLY instruction was *"try to refute its safety, default to
UNSAFE if you cannot prove otherwise"* to find it — and it found it in a file it had not been
assigned, because it followed the implementer's own citation of what it had copied.

## 2026-08-07 — Item 5 marker + the revival crash fixes — build gate GREEN

Both passes were killed mid-flight when the session process exited; resumed from run id, so
completed agents replayed from cache and only in-flight work re-ran. Nothing was lost.

**Item 5 shipped and came back CLEAN from all three reviewers.** No path found by which the
marker can reach saved state (`getSlotDisplayName` has exactly two consumers, both display-only;
the rename dialog seeds from raw `Tab::name`; the strip writes colour + tooltip and never label
text, so `<InstNames>`, the in-place rename editor and stem WAV filenames are all untouched). No
false positives across eight lifecycle scenarios — notably no transient flash on a still-loading
tab, because `loadBaySickGuitarsKit` is fully synchronous and both SET sites compute
`kitSubstituted` from the raw ref BEFORE the substitution branch overwrites it, then write once.
Spec §6's "must NOT be touched" table verified untouched.

**A reviewer refuted the implementer's own caveat.** The implementer warned that a multi-file drop
could stack N missing-file dialogs on one redo via `wrapTabAddUndo(asRider=true)`. The reviewer
traced the actual chain — `filesDropped` → `importAudioFile` → `onAudioClipAdded` →
`createClipStripAndPage` → `spawnClipsTabIfMissing` — and found it never reaches
`wrapTabAddUndo` at all. No action needed. Recorded because a self-reported caveat is a lead, not
a fact, exactly like an agent finding.

**Two gaps the reviewers found, fixed by the parent session:**
- **Undo-resurrecting a Rusty tab whose kit vanished was completely silent.** The resurrect branch
  collapsed missing-kit, null-page and failed-load into one condition, so none of the three said
  anything, while its `deserializeUIState` twin reports both cases properly. Now mirrors the twin.
- **A dropped-but-undecodable clip banked a report nobody drained.** Drop a truncated WAV: the
  block appears, plays silence, and the entry sits in the process-wide store until some unrelated
  gesture drains it — so loading an FX rack preset later announces *"this preset refers to files
  that are no longer where they were saved"* naming a WAV that is neither missing nor related to
  the preset. Fixed at the PRODUCER (`rebuildAudioClipPlayers`) rather than its ~10 call sites, so
  a caller added later cannot forget it, gated on the project-load shield: raised means a load is
  running and the entry belongs in that load's batched dialog, lowered means a live edit that owns
  its own report.

### The revival crash fixes — and the disagreement resolved against BOTH reviewers

Both use-after-free fixes had landed before the crash (option (ii): nest-aware shield →
`settleAudioThread` → build+publish → push state → restore, at BOTH sites so they cannot diverge).
The resumed agent re-derived each mechanism end to end rather than trusting the comments — and
**corrected two comments that stated false mechanisms**, which is the Task 3 defect class
reappearing inside a Task 5 fix.

**The reviewer disagreement is settled, and neither reviewer had it right.** The one who traced
Site B (Plugins tab) to a fixed point at `mBridgePreferred == false` was CORRECT that
`instantiate()` can never fire there — `setBridgePreference` has exactly one caller outside the
class, `EffectWindows.cpp`'s RACK slot window, and a Plugins tab exposes no bridge toggle at all.
But both reviewers then treated that as meaning the site was safe. It is not: `setStateInformation`
still runs `mInner->setStateInformation` on a live in-process plugin already published to the
dispatcher, which almost no VST3 tolerates concurrently with its own `processBlock`. **So the
shield at Site B is load-bearing today, not defensive.** Fixing both sites regardless was right for
a better reason than the one it was ordered on.

**The secondary consequence is only HALF closed, and the comment now says so instead of
overclaiming.** The shield fully closes the RENDERING half — no audio passes through the
momentarily-unbridged instance. It does NOT close the LOADING half: `loadEffect` /
`createEngineFor` instantiate before any blob is read, so a 64-bit plugin the user bridged FOR
STABILITY is still loaded into the host address space during a retry, and a crash inside its own
module load still takes the app down. Closing that needs the preference to reach the build, which
`loadEffect` has no channel for (would touch `HostedPlugin.h/.cpp`, `EffectRack.cpp`,
`HostedPluginEffect.cpp`). Routed, not silently dropped. Also corrected: the original framing said
"force-bridged", but force == 32-bit, and `isBridgeForced()` makes `instantiate()` refuse the
in-process fallback outright — the real exposure is a user bridge PREFERENCE on a 64-bit plugin.

**A THIRD instance of the same crash, pre-existing and the most reachable of the three.**
`EffectsPage::applySlotSnapshots` — the undo/redo apply shared by Load Effect / Remove Effect /
Move Effect — calls `loadEffect` with NO `PluginDescription`, so a VST3 slot publishes with
`mHosted` null and the state push then CONSTRUCTS (or on a re-apply destroys) the instance through
`setPlugin`, while `HostedPluginEffect::process` reads `mHosted` as a plain unguarded
`unique_ptr`. Reachable by undo/redo of a rack VST3 load during playback. Fixed by the parent
session with the same shield, bracketed around the WHOLE slot loop rather than per slot — which
additionally makes undo of a Move Effect atomic against the audio thread instead of letting it
observe a half-restored rack. `juce::ScopeGuard` was written first and then replaced with the
explicit restore the same file already uses fifty lines up: there are no early exits, and
introducing the tree's only `ScopeGuard` in the last code batch is not worth the novelty.

**Freeze preservation verified as correct, not just present.** The route is a page-scoped
suppression of `markEngineContentChanged` around the rebuild, and three supporting pieces are all
present and all necessary: `teardownEngine` does not clear `frozen`/`freezeStreams` so the record
survives; `freezeProcStampSeen` is resynced to the FRESH listener after the state push or
`drainEngineChangeStamps` would mark stale after the fact; and the frozen source is re-published
into the NEW RenderTask or the tab silently drops to live. The failure branch is right too — when
the recreate returns null the mark IS issued, because the tab lost its engine type and there is
nothing left for the render to be a render of.

**Build gate:** GREEN (five exit codes 0, four link lines, zero errors).

## 2026-08-07 — Task 8 — Re-sweep to clean

The task the batch plan defines as "the batch ends when a sweep round produces zero new confirmed
findings." Round 1 and round 2 are done and both gates are GREEN; round 3 is running.

### Round 1 — 42 confirmed / 5 refuted, and TWENTY were self-inflicted

Seven category sweeps re-run over the post-fix tree, each pipelined into a refute-first verifier.
**4 CRASH / 4 DATA_LOSS / 13 WRONG_STATE / 1 GROWTH / 20 COSMETIC.**

**The number that justifies the whole task: 20 of the 42 were introduced by this batch's own
fixes.** Nearly half of round 1 would not exist if the batch had stopped after Task 7 — and it
would all have shipped, because it passed a green build and per-unit review.

**Three of the four CRASH findings are new, and two of them are the batch re-committing errors it
had just spent the day fixing:**
- `stopRecording`'s settle placed AFTER the writers were freed — the same inverted ordering Task 4
  fixed in `EngineRig::teardownEngine`.
- `deleteAuxStrip`'s NEW shield using a fixed 30 ms sleep instead of `settleAudioThread()` — the
  arithmetically-unsound thing Task 4 replaced, re-introduced in a Task 4-shaped fix.
- `mLife.reset()` racing in the sandbox client destructor.

**The worst finding is older and unrelated to the fixes: "Save Page Preset & Delete" destroys the
tab even when the preset write FAILED.** Five pages do it; four sibling pages already guard it and
say "The tab was not deleted." So on those five, a full disk or a read-only presets folder means
the tab, its mixer strip, its rack and its roll are destroyed with nothing on disk. On PluginsPage
it compounds: the failed write still re-captures the clean baseline, so the NEXT delete skips its
unsaved-work prompt too.

**All four bundler findings are regressions from this batch's own bundle rewrite** — the pass
written to stop bundles shipping orphaned references was creating them a different way.

### Round 1 fixes — 46 applied across 9 clusters, gate GREEN

Crash fixes verified by two reviewers: two came back **"COULD NOT REFUTE"** with the reviewer
reading the vendored JUCE IPC source to confirm `killWorkerProcess` performs an UNBOUNDED join of
the reader thread before the destructor body returns — which is the fact the token fix rests on.
The third passed with one finding: the G3 ring's NEW publication-stamp comment claims more than a
one-way publish flag can prove. **The comment-truth defect class, appearing inside a threading fix,
for the third time this batch.**

**Two of my own docket entries were corrected by the agents, both rightly:**
- My drum-drain instruction said "three callers, all single-item gestures." There are TWO, and one
  is called IN A LOOP by the kit-load undo/redo lambdas — the drain I specified would have stacked
  up to 14 dialogs on one Ctrl+Z, which is the exact failure the batch's own comment six lines away
  warns about. The agent drained the one caller that genuinely is single-item and said why.
- The Rusty program item became spec-call-adjacent; the agent shipped the conservative end and then
  PROVED it costs nothing — no shipping path can produce a third kit filename, and it specifically
  checked whether the six other SFZs in the kit folder are unwired programs (they are not; they are
  auto-discovered zoom sub-tabs). Evidence instead of a question.

### Round 2 — 19 more, closing what round 1 could only report across a file boundary

**Export Project Bundle was still broken and this is what fixed it.** Round 1 shipped the path
REWRITING; the engine-side READERS still built bare `juce::File` handles, so every rewritten
reference resolved against the process working directory. The cluster solved the seam problem
rather than brute-forcing it: `NAMPedalStyleDSP` and `AcousticSimulatorStyleDSP` are built by the
`EffectRack` factory, which holds no processor reference, so constructor injection would have meant
threading a resolver through every rack, pedalboard, Inst chain and preset surface that can own a
slot. It added `Source/ProjectFileResolver.h` on the precedent `MissingFileReport.h` already sets
for exactly this problem, and left it inert-but-safe (falling back to today's behavior) until the
hookup landed.

**It went beyond the literal item twice, and both were load-bearing:** a restored reference is now
written BACK in the form it arrived in (otherwise opening a bundle and saving rewrites
`Samples/x.nam` to the recipient's absolute paths and the fix silently undoes itself on the next
re-bundle), and an already-loaded test now compares RESOLVED files rather than raw strings (a
bundled snapshot holds `Samples/<name>` while MicSimDSP recorded an absolute path, so the string
compare could never match and would have reloaded the IR on every A/B slot switch).

**Parent session closed four things the clusters could not reach**, two of them build breaks:
`ProjectFileResolver::install` wired into `setCurrentProjectFolder` (chosen because ProjectManager
calls it BEFORE `deserializeProject`, so the resolver is live before the first engine reads a
stored reference) and retired in `~VibeSynthProcessor` (the callable captures `this`);
`EngineRig::addTab` losing a parameter across 13 call sites; `EngineRig::renameTab` deleted with its
6 callers and the comment at each claiming to be "the one sync point for the model tab's name";
and the per-bank lock-prompt argument at two sites.

**Also landed in round 2:** the version-capture recorder given the same gate+settle as the master
recorder (identical crash shape); `startRecording` made safe by construction rather than by accident
of caller ordering; NINE remaining fixed-millisecond settles converted; the dropped-block count wired
from `AudioFileRecorder` through `RecordResult` to the record-problems dialog, so a take that lost
audio to a slow disk stops presenting as complete; `savePatchAs` write guards; five lock toggles
undo-wrapped; `MissingFileReport::reportIfAny` coalescing same-turn drains; `skipGlobalLockPrompt`
made per-bank.

**Refused on implementation, correctly:** ClipsPage's lock toggle cannot be undo-wrapped — that page
has no route to the undo context at all. Reported as 3-of-4 rather than half-wrapped.

**Build gates:** GREEN after round 1 fixes, GREEN after round 2 + the parent-session closures.

### Round 3 — 30 confirmed / 4 refuted, and the point where whack-a-mole stopped

**4 CRASH -> 0. Threading came back COMPLETELY CLEAN. Self-inflicted findings fell from 20-of-42
to 11-of-30.** The dead-code sweep examined 5,850 sites (round 1: 78) — one of its scan commands
failed to produce output and the agent recovered by searching a different way, so the round went
BROADER, not thinner. Severity mix: 5 DATA_LOSS / 14 WRONG_STATE / 11 COSMETIC, no crashes.

**But two defect FAMILIES had produced new members every single round, and the batch had been
fixing them one site at a time:**
- *"the write result is discarded, so a failed save reports success"* — FIVE sites in round 1
  (which also destroyed the tab), THREE more in round 2, and FOUR more in round 3
  (`saveTemplateAs`, `saveKitAs`, the Rusty page-menu save — a NINTH member and a different save
  from the already-guarded `savePlayerPresetAs` on the same page — and an engine-editor
  `savePreset`). Twelve sites, each discovered individually.
- *"a gesture banks a MissingFileReport entry and never drains"* — seven in round 1, more in
  round 2, five more in round 3.

Round 3 also established that **the already-guarded sites had drifted from each other**: some
sanitize the typed filename, most do not; some auto-suffix on a name collision, others silently
destroy the user's existing file. So the "fixed" family was itself inconsistent.

**Ruling taken: stop fixing members, kill the families.** Round 3's fix pass built the shared
infrastructure FIRST and then routed every site through it:
- `Source/UserFileSave.h` (new) — the single write-to-a-user-named-file path: sanitize via
  `createLegalFileName`, auto-suffix on collision in the exact spelling LayersPage and
  BaySickRustyDrumsPage already used, create the directory, write, and on failure raise the
  eight-site-majority warning box. Returns a `Result` the caller must check to abort the rest of
  its gesture (several callers must NOT clear a dirty flag, rename a tab, or fire a chained
  delete on failure). Supports both write shapes in use. Exposes `resolveTarget` and
  `showWriteFailure` deliberately, so a site with a bespoke write still cannot become a
  thirteenth divergent copy of the wording, plus a named `kTabNotDeleted` constant so nine
  adopters cannot spell the tail sentence nine ways.
- `MissingFileReport::ScopedGesture` — RAII drain firing on destruction, nest-aware via the
  save/restore "was one already open" flag (NOT a refcount, matching the shield discipline), so
  only the outermost scope drains and an inner gesture cannot steal the outer's entries.
- `MissingFileReport::clear()` deleted (zero callers, re-verified).

**USER-VISIBLE BEHAVIOR CHANGE, recorded loudly:** saving a preset/kit/template under a name that
already exists no longer DESTROYS the existing file — it becomes `name (2).xml`. And a name with
illegal characters now saves with them stripped (`Lead 1/2` -> `Lead 12.xml`) instead of silently
failing to write. Both are improvements, both change what lands on disk, and neither rewrites or
migrates anything already there.

**28 findings applied across 5 clusters + 6 out-of-set items closed by the parent session**
(the fourth engine-editor save site, three drum sample-load drains, two stale comments, one dead
accessor). Two agents REFUSED their instructions and were right both times: the drum-sample drain
I specified had no gesture boundary inside the file I assigned, so the agent traced and named the
three real boundaries in a different file rather than hand-rolling one in the wrong place; and a
save finding filed against one engine editor turned out to have a fourth sibling outside that
cluster's set.

**Build gate:** GREEN.

**Round 4 is the test of whether the family fix actually closed it** — the sweeps are instructed
to hunt specifically for a THIRTEENTH member and for wrong adoption of the two new helpers.

### Round 4 — 39 confirmed / 2 refuted, and the number that reframes it: ZERO self-inflicted

Round 4's count ROSE (30 -> 39) and its DATA_LOSS rose (5 -> 8), which reads like divergence. It
is not. **None of the 39 were introduced by round 3's fixes** — down from 20-of-42 at round 1 and
11-of-30 at round 3. The fixes had stopped generating defects entirely.

**The count rose because round 4 was the first sweep aimed at ADOPTION, so it enumerated the
family's whole remaining tail at once instead of finding four members per round.** And it caught
the parent session overstating: the round-3 brief told the agents the save family had "twelve-plus"
sites. Actual adoption was **EIGHT**, with roughly fourteen more untouched. The agents converted
exactly what they were pointed at; the pointing was wrong. That error is the parent session's, and
it is why round 4 existed at all.

**Two findings worth naming:**
- **Deleting a tab could destroy it with no undo.** `UndoSnapshotStore::writeNew` discarded its
  write result and returned the File unconditionally; `deleteTabWithUndo` closed the tab on the
  VERY NEXT STATEMENT. With the snapshot folder unwritable the tab is gone, the History window
  shows a "Delete" entry as though it worked, and Ctrl+Z restores nothing — or on the L/B/D spine
  restores an EMPTY tab with the right name, which is worse because it looks successful.
- **Every preset the BaySickPlayer editor saved was permanently unloadable.** `savePreset` wrote
  the FLAT apvts XML; `loadPreset` expects a NESTED shape, so the root-tag gate passes, nothing is
  applied, and `onPatchLoaded` fires anyway — renaming the tab and mixer strip as if it loaded. The
  verifier specifically checked the other three engine editors: they write flat AND read flat, so
  this was the only mismatch, not a family problem.

**And the most embarrassing finding of the round: `UserFileSave.h`'s own header comment claimed
universal adoption and "no remaining hand-rolled copies."** The comment-truth defect class, inside
the documentation of the helper built to kill a defect family. Its replacement deliberately carries
NO adoption census — a count goes stale the moment a site is added, and a stale one reads as "the
rest are covered," which is exactly how it went wrong. It now ships two grep recipes instead, and
the old recipe was itself wrong: it grepped for a loop no non-adopter has ever contained, so it
matched only that file and an empty result read as "everything is converted."

**39 applied across 6 clusters + 2 parent-session closures**, gate GREEN.

### Round 5 — 21 confirmed / 4 refuted, and two CRASHes after two clean threading rounds

Count fell 39 -> 21, still zero self-inflicted. But threading — clean at ~195 sites in rounds 3 and
4 — found **two CRASHes** at 62 sites this time, because round 5's brief pointed it somewhere
narrower. Both pre-existing, both with a sibling in the tree that already does it right:
- **`loadBaySickGuitarsKit` / `loadBaySickBassesKit`** flip their active gate and go straight into
  `loadKit`, which mutates sfizz internal state for SECONDS, with no settle. The gate is
  check-then-act: once the audio thread is past its `mProcessingEnabled` load it is committed to
  `renderBlock` for that whole block. `loadBaySickRustyDrumsKit` in the SAME FILE does shield ->
  settle -> load -> restore and its comment names this exact hole.
- **`VibePlayerProcessor`'s three sample-load wrappers are bare** — no shield, no settle, no lock —
  while `VibeSampleManager::clear()` empties `mRegions` and the loaders refill it one decoded file
  at a time, and the audio thread walks that same vector: `findRegion` re-evaluates `.size()` every
  iteration, indexes it, and copies a `shared_ptr` out. No mutex anywhere in the class. Loading a
  sample on a Layers or Bass tab during playback is an ordinary gesture.

### Jeff's ruling: collision must PROMPT, and it made the helper async

The auto-suffix had fixed silent destruction but broken the ordinary case: the save dialogs PREFILL
the currently-loaded name, so re-saving a tweaked patch produced "Kick Tight (2)", renamed the tab
to match, and accumulated another number on every re-save with no way to ever update in place.
**Jeff ruled B: prompt on collision — Replace / Save a Copy / Cancel.**

JUCE modals are async, so a call that may prompt cannot return its result — `writeText`/`writeXml`
became `writeTextAsync`/`writeXmlAsync` with a completion callback, across ~23 sites. Three
deliberate design choices worth recording:
- **The old names were DELETED rather than kept as wrappers**, so a missed site is a hard compile
  error instead of silently changed semantics. That paid off immediately — see below.
- `Outcome` distinguishes **Succeeded / Failed / Cancelled**, because they are not interchangeable:
  Failed has already shown its box (a second is a duplicate), and Cancelled must show NOTHING (an
  error box on a deliberate choice is its own defect).
- The three-button alert is built with explicit `addButton` return values rather than
  `AlertWindow::showAsync`, because JUCE's own LookAndFeel maps a 3-button alert to 1/2/0 while
  `NativeMessageBox` maps it to 0/1/2 and `showAsync` silently picks between them.

**The deletion of the old names caught three sites the same day.** The build failed on
`InstPage.cpp` x2 and `PluginsPage.cpp` — neither was in any round-5 cluster's file set (the
parent session put InstPage in a cluster with the API recipe suppressed, and PluginsPage in none at
all). Both were "Save Page Preset & Delete" first-halves, so their whole tail had to move inside
the success branch; on PluginsPage that tail re-captures the clean baseline, which outside the
branch would mean a FAILED save marks the page clean and the next delete skips its unsaved-work
prompt — the exact bug round 1 found there. Wrappers would have let all three keep compiling and
quietly do the wrong thing.

### Jeff's ruling: delete both held dead-code items

- **The 85%-overload arm.** Not merely dead state: its body took a `juce::Synthesiser`
  CriticalSection **on the audio thread** to achieve provably nothing, at exactly the moment the
  audio thread was already in trouble. Deleting it removes real work from the hot path.
- **The vestigial legacy synth.** `mSynth` constructed eight voices at startup and re-prepared them
  on every sample-rate change for zero samples of output. Removed with its ctor setup, prepare
  call, the no-op `allNotesOff()` wrapper and its one editor caller (annotated "keep legacy
  built-in flush too", flushing nothing), `buildFixedTopology`'s ignored `juce::Synthesiser&`
  parameter, and five orphaned files plus two CMake entries.
  **`SynthSound.h/.cpp` deliberately KEPT** — it sits in the same orphan set and looks like part of
  it, but BaySickSolstice genuinely uses it (`AdditiveVoice` dynamic_casts to it, `BaySickSolsticeSynth`
  constructs one). Deleting the set wholesale would have taken BaySickSolstice down.

**Build gates:** GREEN after round 5's fixes; one RED on the three unconverted async sites (by
design); GREEN after converting them and landing both deletions.

### Round 6 — 21 confirmed / 2 refuted, 0 CRASH — and the plateau named honestly

Same count as round 5, so the loop has PLATEAUED rather than converged: in a 150k-line tree each
sweep finds ~20 more real-but-mostly-minor things, and the plan's "run until a round is clean"
criterion is probably not reachable in a finite number of rounds. Severity tells the real story —
round 1 had four crashes, round 6 has none, and only 2 of 21 came from the previous round's fixes
(the async refactor, ~23 call sites of moved tails, introduced almost nothing; lifetime came back
COMPLETELY CLEAN, and that category was aimed hardest at exactly its risk).

Jeff's ruling on the weekly-limit constraint: fix all 21 (the batch's locked 1=a), run round 7,
then stop and decide.

**All 21 applied + three cross-file halves closed by the parent session, gate GREEN:**
- **The two DATA_LOSS shared one root and got one mechanism:** `UndoSnapshotStore::writeNew`
  returns the same empty File for "nothing to snapshot" and "write failed", and DrumPage's
  sound-swap replay treated empty as a deliberate CLEAR — so with the snapshot folder unwritable,
  Ctrl+Z destroyed the loaded drum and (the verifier's correction) Ctrl+Y could not bring it back:
  both snapshots fail together, unrecoverable in both directions. Fixed with intent flags captured
  at snapshot time (was the side genuinely engine-less?) so a missing-but-had-content snapshot
  no-ops like BassPage. The verifier found the IDENTICAL conflation at a third site the finding
  never named — the kit-load capture, whose replay "tolerates the empty xml" and silently
  re-creates tabs EMPTY — fixed with the same mechanism in the same pass.
- **Browser auto-close** now snapshots BEFORE closing and, when the write fails, warns honestly
  ("The tab was still closed, but undo cannot bring it back") without registering a dead
  resurrection lane — the verifier established no clean abort exists there (the library half is
  already committed before the callback fires), so warn-and-close is the correct minimum, not a
  compromise.
- **The freeze-under-shield fix landed OUT OF SET, correctly:** the finding was filed against
  StandaloneEditor but the safe fix lives in `begin/endOfflineRender` (stash the shield after
  `suspendProcessing(true)`, restore before `suspendProcessing(false)`) — the in-set alternative
  would have dropped the shield with the DEVICE LIVE, letting real blocks render against mid-load
  state. Load-time freeze re-renders are no longer shield-bailed to silence.
- **The sfizz CC race is closed on ALL THREE engines** (parent session finished the two siblings
  the cluster could not reach): `parameterChanged` never calls `mSfizz->cc()` on the writer's
  thread — it marks a per-CC dirty bit (release) and the audio thread drains the marks (acquire)
  through a raw-pointer cache. Rusty additionally gained the `mCcRaw` cache it lacked, so the
  drain does no string-keyed lookups on the audio thread; its drain sits after `updateFromApvts()`
  because Rusty has no engine-level gate — its load safety is the processor-level shield, so marks
  made mid-load accumulate and flush into a fully-parsed engine. All four "the listener forwards
  to sfizz" comments corrected.
- **BassPage's savePatchAs** got the LayersPage native-format mirror (the old
  `<BaySickEnginePreset>` wrapper was silently rejected by every engine editor's loader).
- **Cancelling a template save no longer strands sample copies:** the adopt must run BEFORE the
  write (the template carries the adopted refs), so `SampleLibrary::adoptIntoUserSamples` gained a
  `createdOut` collector reporting only files it NEWLY copied, and the save deletes exactly those
  on Cancel/failure — a re-save reusing an earlier adoption deletes nothing.
- Also: the pedalboard save's half-applied fix completed (the guard that made this batch's own
  error box unreachable deleted); Vox/Inst ribbon renames now reach the mixer strip; FxRackPresetIO
  routed through the async helper; ClipsPage copies clip audio only in the save success branch;
  PluginManager distinguishes never-configured from deliberately-emptied scan folders; four dead
  symbols removed, with the BaySickSolsticeSynth::allNotesOff twin swept per the verifier's "sweep both
  in one pass or the trap survives" instruction.

**Round 7 is running as the decision round** — its sweeps are instructed that the owner decides
after it whether to keep sweeping, so an inflated severity or padded count directly causes wasted
spend and a missed real defect ships. Grade exactly, both directions.

### Round 7 — 25 confirmed / 1 refuted. ZERO crashes, ZERO data-loss, ZERO self-inflicted

**The severity floor dropped out.** 9 WRONG_STATE / 8 COSMETIC / 7 dead-code / 1 GROWTH, and
nothing at all above WRONG_STATE for the first time in the loop.

**Round 7's first attempt did not run:** all seven sweep agents died on the 5-hour session limit.
Its `totalConfirmed: 0` was a FAILURE, not a clean round — recorded here because that zero is the
single most expensive thing in this batch to misread. The agents were read-only so the tree was
untouched; the round was re-run in full (nothing cached to resume from).

**The parent session also got a course correction it deserved.** On resuming, it proposed skipping
round 7 and going straight to `/review-batch` + commit "to protect the work." Jeff: *"I did not
tell you to run that at all, we are not done and you are not just skipping over the fucking
work."* He was right — "can we get started again" meant resume the batch, and turning that into
skipping the remaining task is the same avoid-the-work pattern he called out earlier in this batch.

**FOUR separate category sweeps independently landed on ONE root cause** — the clearest signal yet
that the sweeps are finding real structure rather than noise. `adoptIntoUserSamples`' DIRECTORY
branch: JUCE's `copyDirectoryTo` abandons on the first failed child leaving everything before it on
disk, that branch returned before `createdOut->add(dest)` so round 6's template rollback never
learned about it, and the NEXT call short-circuited at `if (dest.isDirectory())` and BLESSED the
incomplete folder as a complete adoption. Nothing reported it because `hasAnyRegions()` is true for
12 of 30 wavs. The verifier's tell: the FILE branch already self-heals (a partial copy fails the
size+modtime reuse test and gets suffixed rather than referenced) — the directory branch simply had
no completeness test.

**The fix agent REJECTED the verifier's own stated minimal fix, correctly.** The verifier said to
register the partial in `createdOut`; but the failure scenario is a SUCCEEDED template save whose
folder copy failed part-way (adopt returns {}, the ref stays absolute, the template writes fine), so
the rollback never fires and registering it would change nothing. It took the alternative —
`deleteRecursively()` before returning {} — which covers the cancelled AND succeeded cases and keeps
the `createdOut` contract honest. It then added an O(1) in-progress marker file on top, because
delete-on-failure alone cannot cover a crash/kill mid-copy or a delete that fails on the same locked
child: a folder carrying the marker is one we abandoned and gets re-copied, a folder without one is
still short-circuited as the same asset. One existence test, no deep tree compare — which the
branch's own documented ruling refuses. It also flagged honestly that a partial folder from a
PRE-fix build carries no marker and would still be blessed, and that detecting those is
backward-compat work, forbidden pre-v1.

**Also fixed:** renaming a Plugins mixer strip renamed a DIFFERENT tab (the rename callback carried
only a tab id; it now carries `StripKind` + page index); undo-resurrecting a deleted Vox tab
restored the wrong name; a mixer-typed Audio/Clips strip name was never persisted; BassPage and
DrumPage could not READ the `mysamples:` refs this batch's own adoption writes; the file branch's
identity test moved inside the auto-suffix loop so repeated saves stop cloning; five stale sfizz
CC-queue comments; and eight dead symbols (the parent session finished four the cluster could not
reach, and reconciled the `kCurveHz` constant with the bare `0.1` literal it was written for).

**Held rather than deleted:** `BaySickAlignDSP::hasWarpMap` (caller-less, but flagged to Jeff
earlier in this batch as needing a real call rather than a blanket removal, and never ruled on), and
BassPage/LayersPage's legacy `<BaySickEnginePreset>` READER branch (removing it is an unprompted
behavior change; and the finding's option (a) — lifting that reader into the engine editors — was
ruled out by the verifier as a legacy-load shim, forbidden pre-v1, so the editors got an honest
error message instead of a silent no-op).

**Build gate:** GREEN.

### Round 8 -- 26 confirmed / 3 refuted, and the severity floor came back off the floor

**1 CRASH / 3 DATA_LOSS / 9 WRONG_STATE / 12 COSMETIC / 1 GROWTH.** After a round 7 that had zero
of the top two classes, that reads like a regression and it is not. Round 8's threading brief was
aimed at the OFFLINE-RENDER boundary -- the editor sub-components and the recorder taps -- and no
earlier round had examined either surface. The count is the same as round 7 to within one; the
severity is a function of where the brief pointed, not of the tree getting worse. The dead-code
sweep examined 9,160 sites, the widest of the loop (round 3's 5,850 was the previous high).

**The CRASH is the offline render writing into the UI.** `BssFilterXYPad::parameterChanged`,
`BssLedRadio::parameterChanged` and `BaySickVisualizerScreen::parameterChanged` all called
`repaint()` inline. The verifier proved every link rather than gesturing at it: the vendored
`AudioProcessorParameter::setValueNotifyingHost` dispatches listeners SYNCHRONOUSLY under a plain
lock and the vendored APVTS comment says so, `BuilderPage::runExportWithProgress` runs the render on
a `ThreadWithProgressWindow`, and `runOfflineLoop` calls `applyOfflineAutomationAt` every block --
so an ordinary automation lane on `flt_cutoff` during an export repaints a Component off the message
thread. Two things were ruled BEFORE the agents saw it: do not fix it by posting to the message
queue (this tree's idiom is an atomic the UI polls), and do not merely relocate the dead try/catch
around the IR load. Fixed with an atomic mirror plus a timer poll, peer-keyed in
`parentHierarchyChanged()` per the CLAUDE.md suspend convention.

**Two of the three DATA_LOSS are the same gap: recorder taps that never learned about offline
render.** `applyPostMixRecordAndMetro` gates `mMasterRecorder` and `mCaptureRecorder` on
`! isNonRealtime()` with a comment naming the hazard, while `mMidiRecorder.processBlock` and the
`mPreRollSamples` counter carry no such term -- and the function is called from `processBlock` AFTER
`dispatchBlock`, outside the task graph, so `setFreezePrune` can never protect it. The inflated
pre-roll count drives `commitRecordingResult`'s content length negative and the take is discarded.
The per-strip `tapDryRecorder` had the same hole, narrower only because the freeze prune skips
non-target strip tasks. The third is the rename gesture (below).

**Divergence and persistence landed on ONE root from two directions: renaming is not a first-class
action.** The verifier's framing is the useful one -- there are FOUR parallel name stores for one
logical channel (`RibbonTabBar::Tab::name`, `Page::mTabName`, `MixerTrackStrip::mNameLabel`, and the
`<VoxNames>`/`<InstNames>`/`<AuxNames>` lists), and every finding is a place where two of the four
fail to agree. Underneath all of them: `TransactionTracker` is advanced from exactly one place,
`StandaloneEditor::changeListenerCallback`, which derives events purely from UndoManager depth
deltas -- so a gesture that performs no UndoableAction cannot dirty the project BY CONSTRUCTION.
Neither rename route performs one. Rename a tab, close, and it is gone with no prompt and no undo.

**The fix followed round 3's precedent -- kill the family, do not patch the members.** One private
`RenameFamily` spine (`findRenameTarget` / `uniqueTabNameFor` / `applyPageRename` /
`performPageRename`) now serves both the ribbon route and the mixer route: four name sites written
for all eight families, ONE `StructuralOpAction` banked per gesture, undo/redo lambdas re-resolving
the target through the live page list on both ends. Targets are keyed by (family, page index) rather
than `ribbonTabId` because a resurrected tab gets a fresh ribbon id. A no-op rename applies but banks
no transaction, so it cannot dirty the project over nothing. `MixerPage::StripKind` gained Vox and
Inst, which closes the exact reverse of what round 7 fixed for Audio. An Audio-row rename with no
owning Clips tab is now REFUSED -- label snapped back off `persistedAudioRowName`, one info box --
rather than inventing an `<AudioNames>` list that would drift against the tab name on every owned row.

### Round 8 -- the self-inflicted number the round recorded is wrong, and the real one is 6

The round-by-round table written into the chat after this round records **2** self-inflicted. That
number came from a script counting the `newFromRound7` flag on the verifiers' `confirmed` entries,
and only the silent-failures verifier preserved the field -- the other six categories dropped it, so
every flag in them evaluated false. Counting the flag where the FINDERS set it: **8 flagged, 1
refuted outright (the ClipsPage parity comment), 1 whose round-7 attribution the verifier
specifically struck (`mPPBeat` was already read-nowhere at HEAD; round 7 only removed a second
writer), leaving SIX confirmed findings that round 7's own fixes created.**

Round 7's fixes did not stop generating defects the way rounds 4-7 suggested. What is true is the
CLASS collapsed: all six are 3 COSMETIC + 3 WRONG_STATE, nothing crash-class, nothing data-loss,
against 20-of-42 at round 1 with four crashes in it. Named, since that is the point of this file:
- **Round 7's adoption marker has a hole, and it is a good one.** `deleteRecursively` ANDs across
  every child rather than stopping at the first failure, so the cleanup on a failed copy always
  deletes the marker (a file we just created) and may fail on a payload child -- leaving a payload
  with NO marker, which is precisely the state the marker exists to make detectable. The verifier
  threw out the finder's speculative antivirus trigger for a deterministic one: Win32 `CopyFile`
  propagates `FILE_ATTRIBUTE_READONLY` and JUCE's `deleteFile` clears no attributes, so adopting a
  folder off read-only media reproduces it every time. The lifetime sweep found the same code from
  the other end -- the marker is deleted BEFORE the payload -- and both were fixed with one helper,
  `wipeIncompleteAdoption`, which re-plants the marker when the wipe reports false and makes the
  retry path bail rather than copy into a directory it could not empty.
- **Round 7's honest preset-load message stopped at three of six loaders.** BaySickSolstice,
  BaySickBass and BaySickSynth got "That preset file could not be read."; BaySickPlayer and the
  Bass/Layers page loaders kept their bare returns.
- **Two comments round 7 wrote are stronger than the code it shipped** -- `SampleLibrary.h`'s
  unconditional copy-cleanup contract (true of the directory branch only) and DrumPage's
  persisted-ref justification (it cites a `savePatchAs` behavior that function does not have). The
  comment-truth class, inside the same pass's own new prose, again.
- **The ownerless Audio-row rename** is round 7's Audio-strip fire meeting a state the design did
  not anticipate.

### Round 8 fixes -- 26 routed, two clusters lost to the session limit, and a half-fix that was worse than none

The fix pass launched five file-disjoint clusters. Two -- `audio-thread` and `rename-and-undo`, i.e.
the crash and all three data-loss findings -- died on the 5-hour session limit. The resume did NOT
replay the other three from cache as expected: all five re-ran, and three of them found their own
work already in the tree and correctly refused to re-apply it. **`audio-thread` had already WRITTEN
its edits before the limit killed it**, so a cluster the workflow reported as failed had in fact
landed; the resumed agent re-derived each fix from the code instead of trusting either the digest or
its own prior run.

**The half-fix is the thing to remember from this round.** `tapDryRecorder` got its
`isNonRealtime()` gate; the vocal WET write did not, and it sits in a different file. With only the
DRY half gated, freezing a Vox tab produces a DRY take that EXCLUDES the render's blocks and a WET
take that INCLUDES them -- the pair drifts apart by the render's length and the aligned pair the
whole Vox page is built on stops lining up. That is strictly worse than the shared bug it replaced.
Same shape on the crash: `BaySickVisualizerScreen` was the third site and outside every cluster's
file set, so two thirds of a crash fix would have shipped. Both were closed by the parent session
after the agents reported them loudly, and the agent was right that the visualizer fix was CHEAPER
there -- that class already runs an unconditional 30 Hz timer, so it needed a flag and no new timer.
The first cut of the WET gate was mine and was wrong: nulling `wetRec` for the render's duration
also trips the arm-edge detector below it, reading as a disarm going in and a re-arm coming out,
re-priming the latency skip mid-take. The gate had to wrap the edge detector, not just the write.

**One agent reversed its instructions and was right.** The digest said the dead
`EEAutomationGrid::setBeatsPerBar` should be WIRED, not deleted, and the first pass shipped the wired
version -- deriving beats-per-bar from the song-level time-signature marker. The resumed agent
refuted it against five in-tree statements of the C.5b (post-revert) ruling, including the actual
automation playback evaluator's `const double kBeatsPerBar = 4.0`: the Builder grid an automation
block sits on is uniform 4-beat-per-bar and song-level TS markers are decorative there. The wired
version was a no-op with no marker present and a NEW editor-vs-playback divergence with one. It
deleted the setter and pinned `static constexpr int kBeatsPerBar = 4` with the ruling in a comment.

**Recorded and left OPEN, all verified still open in the tree:**
- `MicSimDSP::loadUserIr` -- the same provably-dead try/catch and unconditional `return true` the
  NAM/IR loader was fixed for. Outside the cluster's file set.
- `BassPage::loadPreset` and `LayersPage::loadPreset` -- still return silently at every pre-mutation
  exit, so the preset-message family has three of six members, not six.
- The Pattern Per Track stem naming. Only the `Drums N` -> `Drum N` label landed; naming stems from
  live strip labels needs a resolver wired from StandaloneEditor. And the cleanup agent proved it is
  worse than cosmetic: the index a tab occupies is reused on delete while the NAME counter never
  decrements, so a per-track render can write `Pattern_1 - Layer 1.wav` containing the audio of the
  tab the user sees as `Layer 4`, with no tab named `Layer 1` in the project at all.
- AUX strip renames open no transaction either (they persist through `<AuxNames>` once a save
  happens, but never cause one). Renameable BUS strips are worse -- no transaction AND no
  persistence at all; `BusNames` returns zero hits tree-wide, so renaming "Vox Bus 2" survives the
  session and is gone on reload. Closing either needs a previous-name cache or a new persisted
  element, neither of which was in scope.
- The FIRST adoption failure is still silent -- `if (adopted.isNotEmpty())` has no else branch, so a
  refused adoption quietly keeps the volatile source path in the template. The marker fix makes that
  refusal correctly persist, which makes the missing message MORE visible, not less.
- The Event Editor LFO rate unit divergence (drawn as cycles-per-bar, played as a period in
  normalized clip position) and the Snap dropdown's "Bar" entry collapsing onto "Beat".
- Four of the five unused standard-library includes, all in files outside the cluster's set.

### Round 8 -- the line count I made up, and where the loop actually stands

Jeff caught it: I had been telling every sweep prompt this was a 150k-line codebase, in all eight
rounds, and had never measured it. **Measured: 214,983 lines across 375 files in `Source/`** (his
4.7 million is the whole repo including vendored `juce/` and `libs/`, deliberately out of scope). I
also told the sweeps the batch had applied "~250 fixes" before Task 8; adding the real per-task
counts puts it past 347 before Jeff's rulings and the kit-bank work. Both were estimates presented as
counts. That is the same defect class this batch spent eight rounds hunting, in the instructions
being used to hunt it. It invalidates no finding -- the agents read real files and re-located every
symbol themselves, and no verdict rested on my arithmetic -- but the round-by-round numbers below
are the only figures in this document that came from actual workflow returns, and any bare number I
give without showing the command should be discounted.

**Confirmed by round: 42 / 19 / 30 / 39 / 21 / 21 / 25 / 26.** Round 8 lands exactly where rounds
5-7 did. The plan's termination condition -- run until a round is clean -- is not reachable in a
finite number of rounds, and round 8 is the fourth consecutive round saying so. What round 8 does
NOT support is the stronger claim rounds 4-7 encouraged, that the fixes had stopped generating
defects: six of 26 came from round 7. What HAS held is that the self-inflicted findings are now
small ones. And severity is noise rather than a trend -- round 7 found zero crashes and zero
data-loss, round 8 found one and three, purely because the briefs pointed somewhere new. Each round
still finds genuine defects because no round has covered the whole surface. There is no stopping
point in the data; stopping is a judgment call about diminishing returns against the master test
plan campaign, which reaches a dimension eight rounds of reading code structurally cannot.

**Build gate:** GREEN (five exit codes 0, four link lines, zero errors).

## 2026-08-08 -- Task 9 -- System Reference consolidation

The documentation task, and the only task in the batch with no build gate (docs-only, per the
plan). It produced one new directory, `Plans & Specs/System Reference/`, which is the sole
untracked entry in the tree at batch close.

### What the set actually is -- 39 documents, 15,056 lines

**39 `.md` files: an INDEX, 37 area documents, and one screenshot capture plan.**
919,282 bytes of markdown; with the `Pictures/` subfolder the directory is 7.8 MB on disk.

- **`INDEX.md`** (125 lines). Navigation for someone opening the set cold. It groups the 37 area
  docs into seven bands -- the shell, the tab families, the instruments, writing the music,
  mixing/effects/tone, the vocal chain, and content/files/output -- one line each saying what that
  document covers. It also carries two things that are not navigation: the statement that the code
  is the authority and that an honest "not determined" beats a confident guess, and one
  load-bearing fact placed above everything else -- **the Core Library is installed separately from
  the application**, the app creates the folder EMPTY on first run (`ProjectManager.cpp:455`, so the
  `Sample Library.lnk` shortcut lands on a real path) but never populates it, and on a machine
  without the content the sampled
  instruments simply come up empty. That is the single most confusing first-run experience a new
  user can have, and it now leads the set rather than sitting buried in `Sample Library.md`.
- **The 37 area documents** (611,848 bytes). One doc per build-thing, per the blueprint organizing
  rule -- never per phase or batch. Every one of them runs the same skeleton: **Purpose** (two to
  four sentences), **How it operates** (mechanism, ownership, thread residency, real class and file
  names), **User-facing behavior** (the manual-source section, and by a wide margin the longest --
  every control by its on-screen name, with range, unit, default, and what moving it does),
  **Parameters and persistence**, **Lifetime and teardown**, **Cross-references**, and a
  **Differs from Carry-Forward** tail. That last section is present in 38 of the 39 files; the only
  file without it is the screenshot list. Carry-Forward stays a frozen 2026-05-07 snapshot and is
  not edited -- where the two disagree the System Reference doc records the delta and the code
  wins. Three files carry an explicit **Not determined** list (`BaySickRustyDrums.md`, plus the
  convention itself stated in `INDEX.md`).
- **`MANUAL-1 Screenshot List.md`** (4,055 lines, 298,163 bytes -- a third of the whole set by
  size). Not a reference doc: it is the capture plan for the visual atlas. **723 shot entries,
  `SHOT-001` through `SHOT-723`**, counted in the file, organized into 29 sitting headings -- 28
  navigation sittings plus a fault-state sitting F. The file's own accounting: 684 shots reachable
  by ordinary navigation, 39 that need something broken or missing on disk first, deduped down from
  roughly 826 raw entries by merging about 103 shared surfaces (title strips, preset menus, the
  Automate right-click, effect panels that appear in both the rack and the vocal chain). Each entry
  gives a click-by-click Reach from a known state assuming no codebase knowledge, the elements that
  need labeling, and a "why separate" note where the shot is a variant. Ids are declared stable --
  manuals 2 and 3 footnote against them, so a dropped shot retires its id and a new one appends.
- **`Pictures/`** -- 42 PNGs, 7,027,651 bytes, added 2026-08-09, after the documents were written.
  They are named by screen (`Mixer.png`, `BaySickSolstice.png`, `BaySickRustyDrums Kick.png`), not by
  `SHOT-` id.

### How the documentation ledger was discharged

The ledger at the end of these notes accumulated across every sweep specifically to feed this task:
**12 `# Capture from finder:` blocks in two groups** -- Task 2's dead-code sweep captures
(1,654 sites read across the whole tree) and Task 1's pre-fix captures -- running from the ledger
heading to the end of the file, 503 lines.

**It was consumed as raw material, not transcribed.** The plan's accuracy bar is that a System
Reference claim contradicting the code is the same defect class Task 3 exists to kill, and half the
ledger describes the PRE-fix tree, which eight rounds of fixes then changed underneath it. So every
document was written and spot-verified against the post-fix tree, with the ledger used to know
where to look and what the ownership and threading shape was. The ledger stays in place in these
notes as the audit trail; it is not the source of any sentence that shipped.

### Findings raised and DOCUMENTED but NOT fixed -- all six open, awaiting Jeff's ruling

Writing user-facing descriptions of every control surfaced behavior that is wrong or misleading but
is not a defect with an obvious right answer -- each one changes what the user hears or reads, so
each is a spec call. They are recorded here in full so they cannot be lost between the docs and the
manuals. **Every one was re-verified against the source before being written down.**

1. **The BaySickSynth / BaySickBass filter, LFO and envelopes are pinned to 44100 Hz. CONFIRMED.**
   `BaySickSynthDSP::prepare` hands the real device rate to the `Synthesiser`, and
   `BaySickSynthVoice::setCurrentPlaybackSampleRate` forwards it to `mOsc` and `mOsc2` -- those are
   the only two `.prepare` calls in the whole voice. `SynthFilter`, `LFO` and the three
   `AdsrEnvelope` members were never prepared anywhere in the tree, so
   `SynthFilter::mSampleRate`, `LFO::mSampleRate` and `AdsrEnvelope::mSampleRate` all sit at their
   `44100.0` initializer for the life of the process.  **FIXED at the buffer-matrix pass on
   2026-08-09**, after this was written: `setCurrentPlaybackSampleRate` now prepares all seven
   rate-dependent members.  Recorded here rather than deleted because the Task 9 documentation
   pass is what found it. On a 48 kHz device the filter computes
   `g = tan(pi * cutoff / 44100)` and so realizes a cutoff 48000/44100 = about **8.8 percent above
   the number on screen**; the LFO's phase increment is fast by the same factor, and the envelope
   coefficients are short by it. **BaySickBass has the identical defect** -- `BaySickBassProcessor`
   holds a `BaySickSynthDSP`, it is the same engine. BaySickSolstice and BaySickPlayer are NOT affected;
   their filters are `juce::dsp` objects prepared with a real `ProcessSpec`. Everything else in the
   voice reads the live rate through `getSampleRate()` and is correct, which is why this has never
   presented as an obvious tuning problem -- the oscillators are right and only the filter, LFO and
   envelopes drift.
2. **BaySickSolstice VOL has a discontinuity at the top of its travel. CONFIRMED.** The routing-matrix
   output stage in `BaySickSolsticeSynth.cpp` is gated `if (mRmVol != 1.0f || mRmClip > 0.001f)`, and
   inside it the gain is `jlimit(0, 2, mRmVol * 1.5f)`. So at exactly 1.0 with no clip the stage is
   skipped entirely and the signal passes at unity, while at 0.999 the gain is 1.4985 -- about
   **+3.5 dB louder than 1.0**. 0.667 is the other unity point. Nudging VOL down from the top makes
   the engine louder, which is the opposite of what the control says it does.
3. **The tremolo / vibrato WAVE icon draws a different wave from the one that plays. CONFIRMED, and
   it is an icon-order mismatch rather than a trem-versus-vib mismatch.**
   `BaySickSolsticeWaveformButton` paints positions as 0 sine, 1 saw, 2 square, 3 triangle -- its own
   header comment says so and `paint()` matches it. `AdditiveVoice::lfoSample` maps 0 sine,
   1 triangle, 2 saw, 3 square. Both the tremolo and the vibrato selector read through `lfoSample`,
   so **both** are wrong at positions 2, 3 and 4 and correct only at position 1.
4. **BaySickPlayer's "vibrato" is amplitude modulation. CONFIRMED.** The block in `VibePlayerDSP`
   is commented "Vibrato/shimmer LFO" and does `mod = 1 + mLfoAmt * 0.12f * sin(phase); d[i] *= mod`
   -- it multiplies the sample. That is tremolo, at up to about 12 percent depth, and nothing in
   the path touches the read rate. The knob's tooltip in `VibePlayerEditor.cpp` reads "LFO /
   vibrato amount" and the member comment reads "vibrato / shimmer depth", so the naming is wrong
   in three places for one behavior.
5. **Mono and Lead are the same setting. CONFIRMED.** `BaySickSynthDSP::renderNextBlock` returns
   early for Legato and then routes everything that is not Poly through one branch; there is no
   `BssVoiceMode::Lead` case anywhere in `Source/`. `BaySickSynthVoice` stores its own copy via
   `setVoiceMode` and never reads it. The four-way choice is offered on both engines and the value
   round-trips through the preset, but two of the four buttons do the same thing.
6. **Two shipped tooltips name a real commercial product.** `EffectEditorPanels.cpp`, the FET
   compressor panel's Input and Output knob tooltips -- three mentions of the product name across
   the two strings. The standing rule allows comments, commits and internal ids to reference a
   modeled product and forbids it in user-facing strings, so the surrounding comments in that same
   file are fine and the tooltips are not. A tree-wide grep of string literals for the obvious
   hardware names turned up no other site. **Routed to QA-LegalReview and deliberately not
   rewritten** -- the replacement wording is a spec call, and the About box's attribution list is
   already flagged as known-incomplete pending that batch.

### The agent that refused to write a false claim

**A Task 9 documentation agent refused to write the brief's claim that Jeff's Event Editor
point-reset ruling had been implemented, because the code did not support it.** The claim was
checked -- the only such item is `"Reset to midpoint"` in `BuilderPage.cpp`, which sets
`value01 = 0.5f` flat, the only such items are `"Reset to midpoint"` at `BuilderPage.cpp:6091`, `BuilderPage.cpp:6294`
   and `EventEditor.cpp:725`, every one of which sets a flat `value01 = 0.5f` rather than
   restoring the pre-automation value, and a tree-wide grep
for any pre-automation value store returns nothing -- and the earlier running-notes entry that
asserted it was done was corrected in place rather than left standing. The ruling is real and OPEN;
it is an explicit owner requirement, not a discovered defect. This is the documented case of the
process catching a false claim before it shipped, and it is the same refuse-rather-than-comply
behavior that caught the choke-group comment at Task 3.

## 2026-08-09 - Buffer-matrix pass - correctness at every buffer size and sample rate

Jeff's question, mid-close, after I justified a fix with arithmetic at a 1024 buffer: *"Where are
you coming up with this 1024 size? I run a 128 buffer. Plus we are building something for all
settings not just the one that's easiest for you to build."*

He was right twice.  The 1024 came from a comment THIS BATCH wrote
(`PluginProcessor.cpp:7263`), and I had restated it as "the sizes you actually run" without ever
asking what he runs.  At 128 the sleep I was arguing against is nearly seven block periods long
and works fine.  The honest argument never needed a scary number: a fixed duration is a guess that
lands on the safe side of one config and the wrong side of another.

### The sweep

Six dimensions over `Source/`, each finding adversarially verified at a concrete (rate, block)
pair before being believed: fixed-duration waits used as synchronization barriers; sample-rate
assumptions; block-size assumptions and fixed-capacity scratch; prepare / re-prepare lifecycle
completeness; ring, delay-line, lookahead and reported-latency sizing; and the quiet class -
behavior expressed per BLOCK instead of per SECOND, where nothing crashes and the same project
simply sounds different at 128 than at 1024.

**24 confirmed / 2 refuted.**  The matrix assumed was block sizes 32..4096 and rates 44100..192000,
with a short final block and a mid-session device change both treated as ordinary.

Six areas came back CLEAN and are recorded as such so they are not re-audited: every remaining
fixed wait in the tree (the offline-render entry's `sleep(30)` is dead weight, not a hazard -
JUCE's own callback lock already provides that barrier); sample-rate propagation everywhere except
the six named sites, including the LUFS meter's bilinear-transformed weighting filter and the
reverb's rate-scaled delay lines; block-size handling across all eight render tasks; the
prepare / re-prepare lifecycle including the full offline save-and-restore set; latency
compensation and every ring; and per-sample versus per-block rates throughout the effect DSP layer.

Two claims were TESTED AND REFUTED and must not be re-fixed: the render watchdog does NOT punch
silent holes in exported files (the arithmetic needs the machine at 7x slower than realtime, and
in single-core diagnostic mode it cannot fire on slowness at all), and the DSP-load meter's jitter
does NOT spuriously arm the auto-freeze (that needs three continuous seconds over threshold with
the transport stopped, and jitter makes false arming LESS likely, not more).

### What reached Jeff's own 128 buffer

Three, and the framing matters because "128 is the problem" would be the wrong lesson:

- **BaySickSolstice's two filter envelopes advanced once per BLOCK**, so wall-clock envelope length was
  the set time multiplied by the buffer size: a 10 ms attack took 1.28 s at 128.  Every patch with
  Filter Env Amount off zero had no audible filter attack, ever, at any buffer.  This one is not
  about his setting at all - the multiplier IS the buffer size, so 128 was the mildest case of a
  defect that was wrong everywhere, and raising the buffer would have made it worse.  The
  surrounding comments show the author reasoning themselves into it ("Trade: take one sample now
  and apply that for the whole block", "we accept the 1-sample lag"): they believed they were
  accepting a one-sample lag.  The amp envelope beside it was always correct, so it was an
  isolated slip rather than a house convention.
- **Inst-tab idle suspend counted 9 blocks**, which is 26 ms at 128 and 836 ms at 4096 - and the
  suspend is a hard buffer clear, not a fade, so a still-ringing reverb, delay or NAM cab tail was
  cut off in one sample.  This is the one where low latency was actively punished.
- **A bridged 32-bit plugin could stall the whole device** rather than muting its own slot, because
  the 4 ms rendezvous ceiling exceeds one block period below 256 frames.  The code comment claimed
  this could not happen; the claim held only at 256 and above.

### The worst one, which nobody would ever have reported as a bug

A bridged plugin in a mixer RACK SLOT rendered SILENCE into exported WAVs and frozen tabs, at any
buffer, at any rate, with the export reporting success.  Rack slots are `DSPBase` entries inside an
`EffectRack`, not rig engines, so `beginOfflineRender`'s sweep never reached them and the LIVE 4 ms
deadline was applied to the 2048-sample render block - 8.6% of realtime, which virtually any real
plugin exceeds on the first block.  The tab-instance version of the same plugin was handled
correctly.  Only rack slots were missed.

### Fixed

Ten file-disjoint clusters, each adversarially reviewed before the gate.  The reviews mattered:
every cluster came back with problems, five of them NEW defects introduced by the fix itself.

- BaySickSolstice filter envelopes advance per sample, with the cutoff applied on a sample-counted
  control tick (a fixed 0.5 ms at every rate) rather than once per block - "once per block" as a
  control rate is the same defect wearing a different hat.
- Idle suspend, render watchdog and the bridged-plugin deadline all derive from the live block
  period.  The bridge also gained a resync latch, so ONE missed block no longer means permanent
  silence until the device is reopened, and a wedged helper costs a render one hang ceiling rather
  than one per block (at 8,400 blocks that was the difference between a bounded failure and a
  70-hour export).
- BaySickSynth and BaySickBass prepare all seven rate-dependent voice members, not just the two
  oscillators.  Before this, filter, LFO and all three envelopes sat at their 44100 initializer for
  the life of the process, so at 48 kHz every preset ran 8.84% fast: a 500 Hz filter behaved like
  544 Hz, a 5 Hz LFO like 5.44 Hz, a 2 s release died in 1.84 s.  Pitch was correct, which is why
  it read as "the presets sound a bit bright" rather than as breakage.
- Layers / Bass / Drums engines are re-prepared at the live device config before registration, the
  way the Plugins and Clips/Vox/Inst cases in the same function already were.  They had kept a
  creation-time `prepareToPlay(sr, 512)`, so opening any project above a 512 buffer left every one
  of those tabs prepared for 512 while receiving larger blocks.
- Rack-slot hosted plugins now receive the non-realtime state through a new
  `VibeGraph::setAllRackSlotsNonRealtime`, called from both render edges.  The twenty-five-rack
  walk was extracted into one `forEachRack` shared with `promoteAllRackSlotSnapshots` rather than
  copied, because a second hand-maintained copy of that list is precisely the drift a newly added
  bus rack falls through.
- Pitch tracking holds its frequency floor by DECIMATING rather than by growing the window.  The
  window-growing fix was the one the first pass wrote, and the reviewer killed it with arithmetic:
  YIN's difference function is O(W^2), so at 192 kHz it needs 456% of a core against the hop
  period, and once the worker falls behind `pushAudio` drops the NEWEST samples, splicing the
  rolling window from discontinuous audio.  The failure mode is not a late reading, it is a WRONG
  one - a wrong note name on the tuner and a wrong retune target in the vocal chain.
- Phase-vocoder ring sized from the real block and maximum stretch, with a bounds guard kept
  regardless so a future sizing mistake is survivable rather than silent.  This was another
  monitor-clean / export-broken defect: the render loop runs at a fixed 2048 block whatever the
  device buffer, so a heavily stretched clip sounded perfect while working and came out garbled.
- Pattern renders and per-strip stem bounces follow the session rate instead of always writing
  44100; the export dialog and the Audio Settings rate list carry the rates a session can actually
  run at; the EQ analyzer's frequency axis follows the live rate (its setter existed and had zero
  callers, so at 48 kHz the spectrum drew 8.1% flat and a band dragged onto a visible peak parked
  about a semitone and a half off); and the tempo map is republished on a device rate change, which
  had been leaving 120 BPM playing as 130.6 with the old number still on screen.
- De-reverb's gain-smoother coefficients and the delay feedback DC-blocker's pole are derived from
  durations and a corner frequency rather than being bare per-frame and per-sample constants.  Both
  preserve their measured 44.1 kHz behavior exactly.
- Effect-visual time axes and mixer-strip RMS are time-based, not block-based.  The visual axis had
  a 557x spread across the range, which made the ghost-in / solid-out comparison the visuals exist
  for unreadable at large buffers.

### The five defects the fix pass introduced, and one it hid

Recorded because the pattern is the point: a fix pass that is not adversarially reviewed ships new
bugs at roughly the rate it removes old ones.

1. `kIdleSuspendSeconds` was set to **5.0 seconds** against a pre-fix calibration of 104 ms at
   512/44.1k - 48x longer there and 192x longer at Jeff's 128.  Every Inst tab plus Rusty would
   have kept its whole chain live for five seconds after the last note, so tabs that suspended
   dozens of times a minute would never suspend at all, in exactly the sparse arrangements the
   gate exists for.  Reverted to 0.1 s, which reproduces the old reference behavior at every pair.
   The agent DID flag it, which makes it a spec call taken without asking rather than a concealed
   change.  **The tail-guillotine it was trying to solve is still open** and wants a fade at the
   clear sites (`InstStripTask` / `RustyDrumsProducerTask`), not a longer hold.
2. The offline hang ceiling was paid PER BLOCK rather than per render (see above).
3. and 4. The pitch-tracker window fix, in two consequences (wrong pitch, and a thread-stop timeout
   whose margin fell from 35x to under 2x).
5. The effect-visual input envelope was not accumulated across blocks while the output was, so
   below one column per block the ghost trace under-read against the solid one - the exact wrong
   reading for a display whose whole purpose is the difference between them.

And the one it hid: `HostedPluginEffect::setNonRealtime` and `PhaseVocoder::prepare` were both
added with **zero callers tree-wide**.  Both fixes were dead code in the shipping binary and both
cluster reports described them as done.

### Build gate

GREEN.  Five exit codes 0, four `vcxproj -> ....exe` link lines, zero `error C` / `error LNK` /
`error MSB`.  Full rebuild, since headers changed.

### Held for a ruling

- The Inst-tab tail guillotine (item 1 above) - needs a fade, not a longer hold.
- BaySickSynth's white noise is still rate-dependent while pink and brown are now rate-invariant,
  so its in-band energy shifts with the sample rate.  Fixing it changes the level of every
  noise-using patch, so it was not done silently.
- `PhaseVocoder`'s `kFFTSize` / `kHopSize` are fixed sample counts at the FILE's rate, so they
  encode a duration that moves with the source file.  Public constexpr consumed in arithmetic by
  two other translation units; not safely changed inside one cluster.

## 2026-08-09 -- CODE-COMPLETE -- the findings ledger, the post-review fixes, and what is carried open

The batch plan calls the findings ledger this batch's primary artifact, so this entry is the ledger's
index: what nine tasks and eight re-sweep rounds looked for, what they confirmed, what they refuted,
what Jeff ruled, and -- the part that matters most on the last code batch before v1 -- what is
recorded and deliberately still OPEN, so nobody re-discovers it as new.

### What the batch was, in one paragraph

Jeff's call 2026-07-25: G4 is the last coding group, so before it closes, audit the whole codebase
rather than trusting that per-batch work caught everything. Locked spec: findings are FIXED IN-BATCH,
all of them (1=a); scope is `Source/` + `CMakeLists.txt` + build config with vendored `libs/` excluded
(2=b); multi-agent sweep with every finding adversarially verified before being believed (3=a); slot
is 9th of 9 in G4, running last. Seven category tasks, a re-sweep task that ran until judgment said
stop, and a documentation-consolidation task. Every sweep agent also returned a documentation capture,
which is what Task 9 consumed.

**Measured surface (2026-08-08, after eight rounds of telling the sweeps a number I had never
measured): 214,983 lines across 375 files in `Source/`.** The whole repo including vendored `juce/`
and `libs/` is far larger and is deliberately out of scope.

### The ledger, task by task

- **Task 1 -- silent failures + swallowed errors.** Six finders over the re-measured surface, **476
  sites classified**, 124 explicitly recorded as deliberate-silent with reasons. 84 raw findings, 64
  unique after cross-finder dedupe. Adversarial verify: **57 CONFIRMED / 6 REFUTED / 1 UNCLEAR**. All
  57 fixed -- 41 in 17 disjoint single-file clusters, the 16 cross-coupled hub findings in the main
  session. **33 source files.** The five refutations that mattered were all "structurally
  unreachable": three engine `setStateInformation` overrides with zero callers tree-wide, the
  BuilderPage null-tap branches, and the `deserializeProject` corrupt-subtree branch. Every one is
  recorded with its trace, because a refutation is as load-bearing as a fix.
- **Task 1 follow-on -- the bundle walk.** A question the Task 1 fix could not answer -- is the
  bundler's attribute walk reaching the engine-held file references it claims to? Four findings, all
  confirmed by file:line trace, all worse than filed: two of the six `kPathAttrs` names were PHANTOMS
  matching no producer in the tree, the Inst-chain walk could never see inside its own base64 payload,
  mixer rack states were never handed to the walker at all, and two incompatible base64 encodings
  meant a single-form decoder would have silently walked past half the tree. Jeff's docket the same
  day (1=b fail loudly on present-but-corrupt engine data, 2=a keep the NAM/IR legacy mic-IR fallback,
  3=a acoustic interactive IR loads report, 4=b the bundle re-references what it relocates) shipped in
  the same pass.
- **Task 2 -- dead code + dead registrations.** Six finders, **1,654 sites classified**, 42 recorded
  deliberate-keep. 113 raw -> 112 unique. Verify: **77 CONFIRMED_DEAD / 3 NOT_YET_WIRED / 3 KEEP /
  1 REFUTED / 28 ASK_JEFF**. 77 deletions across 44 files, gate green first try -- the per-symbol
  co-deletion plans are what made a cut that size land. Two whole files removed. Three HOLD-FOR
  markers instead of deletions. One cascade closed in-batch (17 orphaned VibeGraph mono peak atomics).
  29 items deferred to Jeff as a docket, every one a product call rather than a cleanup call.
- **Task 3 -- comment + doc truth.** **116 corrections verified, 114 applied.** The two refusals are
  the task: the sweep ran BEFORE the day's fix passes, so its own correction for the choke-group
  comment had gone stale by the time it was applied, and two agents independently refused to write it
  rather than introduce a brand-new false comment into the exact function whose comment was being
  fixed.
- **Task 4 -- audio-thread safety + threading discipline.** Seven finders, **392 sites classified**,
  101 recorded as deliberate documented patterns. 72 raw / 61 unique. Verify: **33 CONFIRMED /
  2 DELIBERATE / 7 REFUTED / 18 ASK_JEFF**. The headline: ~10 confirmed crash-class sites were ONE
  architectural gap -- the message thread destroying or mutating state while the audio thread was
  still using it -- and the project already owned the correct primitive; those sites had simply never
  used it, and two of them ran their settle AFTER the free it was written to protect. Also the finding
  that had been poisoning every ear check for months: the playhead diagnostic did file IO and String
  concatenation on the audio thread in Debug builds, and the standing verification rule is to run
  Debug FIRST.
- **Threading rulings + the settle.** Jeff's rulings, checked against industry practice by an
  `/architecture` pass before being treated as final. The shipped fixed-millisecond sleep was a guess
  that cannot be right at every buffer size; replaced with acknowledgement-based `settleAudioThread()`.
  **`Source/` carried 22 `juce::Thread::sleep` sites at the batch base and carries 6 now.** The
  pattern-list race went "a now, c after v1" and is recorded as CL-310 in Future State, citing the
  research that found Ardour solving the identical problem with RCU while ALSO keeping a process lock
  -- so bracket-now/snapshot-later is what a mature DAW actually does, not a compromise.
- **Tasks 5 / 6 / 7 -- lifetime, persistence round-trip, UI/state divergence.** Run as one combined
  sweep because the three categories share their file surface. **48 confirmed / 5 ASK_JEFF**, applied
  across **9 file-disjoint clusters, 47 sections, zero agent errors**. The file-ownership boundary
  earned its keep again: the returns named **ten cross-file gaps** where a fix's other half sat
  outside the assigned file set, two of them load-bearing (`RetirementQueue::setConsumerIdle` shipped
  with zero callers; `reportMissingFilesIfAny` gained a parameter that no preset gesture drained). All
  ten closed in a dedicated 8-cluster pass.
- **The five held items.** Held out of the automated pass because each remedy was a product call:
  three Release file loggers appending forever, Vox A/B with no snapshot machinery at all, an EQ
  precision radio nothing read, a dead hosted plugin presenting identically to a working one, and a
  missing sfizz kit keeping the SAVED instrument's name on the tab. Jeff ruled all five; all five
  shipped, and the kit-marker design was surfaced for veto rather than buried because the obvious fix
  (rename the tab) would have destroyed a user-typed name.
- **Jeff's mid-batch docket + directives.** LunaSVG removed; the step-sequencer remains stripped out of
  PatternManager; BassSynth deletion surfaced and confirmed; the Patterns menu wired properly after I
  claimed seven one-line hookups and was wrong; a complete F2-F12 map specified verbatim and applied
  in four places; plugin swap removed as a feature; four defects found in passing and fixed. Two
  claims I made without checking were caught by him and corrected in place.
- **Two independent drum kits (fix work found mid-audit).** Bank identity DERIVED (`bank = pageIndex /
  16`) rather than stored, so nothing new is written to the project file and no migration exists;
  saved kits normalize their slots to 0-15 so a kit saved from bank 2 loads into either. Folded in: the
  MixerState drum arrays were 16 wide where the model is 32, so drums 17-32 had broken fader undo and
  restored no mixer state at all. The Task 4 parameter-pointer cache's `static_assert` caught the EQ
  bus-table mismatch this work introduced, doing exactly its job.
- **The adversarial review that caught a use-after-free.** A reviewer whose ONLY instruction was
  "try to refute its safety, default to UNSAFE if you cannot prove otherwise" found a BLOCKER in a file
  it had not been assigned, because it followed the implementer's own citation of what it had copied.
  Three instances of the same crash shape were eventually found and fixed; the two reviewers disagreed
  about one of them and the disagreement was settled against BOTH of them.
- **Task 8 -- re-sweep to clean.** Eight rounds. **Confirmed by round: 42 / 19 / 30 / 39 / 21 / 21 /
  25 / 26.** Self-inflicted findings -- defects created by this batch's own earlier fixes -- ran
  20-of-42 at round 1, 11-of-30 at round 3, zero at rounds 4 through 7, and 6-of-26 at round 8. Crash
  count ran 4 / - / 0 / - / 2 / 0 / 0 / 1. Round 3 stopped fixing family members and killed two
  families outright (`Source/UserFileSave.h`, `MissingFileReport::ScopedGesture`). Round 5 turned the
  save helper async on Jeff's collision-prompt ruling and deliberately DELETED the old names so a
  missed site was a compile error -- which caught three sites the same day. **The loop has no stopping
  point in the data.** Four consecutive rounds landed in the same band; the plan's "run until a round
  is clean" criterion is not reachable in a finite number of rounds, and stopping is a judgment call
  about diminishing returns against the master test plan campaign, which reaches a dimension eight
  rounds of reading code structurally cannot.
- **Task 9 -- System Reference consolidation.** 39 documents, 15,056 lines, 919,282 bytes, plus a
  42-file `Pictures/` folder. Six findings raised, documented and NOT fixed because each one changes
  what the user hears or reads and so is a spec call. Full detail in the Task 9 entry above.

### Post-review fixes -- 2026-08-09, after `/review-batch`

- **`EngineRig.cpp:128` -- the anti-pattern this batch fixed twice and then re-committed once.** The
  guard between retracting a frozen tab's source pointers and erasing the tab is a fix THIS batch
  added: at the batch base `removeTab` had no settle there at all. It first shipped as a fixed 20 ms
  sleep -- the arithmetically-unsound thing Task 4 replaced everywhere else, and the same defect round
  1 caught in `deleteAuxStrip`. The batch reviewer caught the third instance. **Now
  `mProc.settleAudioThread()`, verified in the tree; `EngineRig.cpp` carried two 20 ms sleeps at the
  batch base and carries none now, against five `settleAudioThread` calls.**
- **The vendored `libs/lunasvg` tree removed.** Jeff ruled "remove" mid-batch and it was cut from
  `CMakeLists.txt` then (13 insertions / 52 deletions in that file), which left the directory as dead
  weight compiled by nothing. **68 files, 1,373,565 bytes (1.31 MB), staged as deleted.** The stale
  mention in `Source/Hosting/Helper/CMakeLists.txt` -- which listed lunasvg among the x86 builds a
  root-project Win32 configure would drag in -- was corrected in the same pass.
- **Jeff ruled KEEP on the five test-plan edits** made to sections E3, E4, B.19, B.18 and A1 (the E3/E4
  collision-prompt + native-format + delete-chaining rewrites, the two drums-bank re-scopes, and the
  A1 gate criterion corrected from two exit codes to the real five-plus-four).
- **Master Test Plan section B.34 authored** -- **47 scenarios, SND-1 through SND-47**, plus a
  four-item KNOWN-OPEN tail so the campaign does not re-file what this entry carries open. The section
  is built as break-it-deliberately scenarios (prove a formerly-silent failure now REPORTS) plus
  targeted checks of every surface that genuinely behaves differently, because a soundness batch is
  invisible when it works.
- **Build gate after all of the above: GREEN.** `build_log.txt` 2026-08-09 08:08 -- `RELEASE_EXIT_CODE`
  / `DEBUG_EXIT_CODE` / `HELPER64_EXIT_CODE` / `HELPER32_CONFIG_EXIT_CODE` / `HELPER32_EXIT_CODE` all 0,
  four `vcxproj -> ....exe` link lines (Release + Debug `BaySickDAW.exe`, `BaySickPluginHost64.exe`,
  `BaySickPluginHost32.exe`), zero `error C` / `error LNK` / `error MSB` hits.

### CARRIED OPEN -- recorded, deliberately not fixed, all re-verified against the tree at close

1. **The Event Editor point-reset ruling is still unimplemented.** Jeff ruled that right-click Reset on
   an automation point should restore the value the parameter held BEFORE it was automated. Nothing in
   the tree stores a pre-automation value (tree-wide grep: zero). Three sites still offer
   "Reset to midpoint" and set `value01 = 0.5f` flat: `BuilderPage.cpp:6091`, `BuilderPage.cpp:6294`
   and `EventEditor.cpp:725`. An explicit owner requirement, not a discovered defect.
2. **The ClipsPage lock toggle is not undo-wrapped.** Four lock toggles were wrapped this batch; this
   page has no route to the undo context at all, so it was reported as 3-of-4 rather than half-wrapped.
   `ClipsPage.cpp:176` still calls `setLocked` directly.
3. **`NAMPedalStyleDSP::restoreState` leaves the previous model AUDIBLE on both failure branches.**
   The empty-path branch calls `unloadModel()`. The missing-file branch and the failed-load branch both
   set `mModelMissing = true` and file a `MissingFileReport` entry -- and neither unloads -- so a slot
   restored from a state whose capture is gone keeps rendering whatever capture the slot happened to
   hold, correctly labeled missing and still making sound.
4. **`MicSimDSP::loadUserIr` is the one IR loader that never got the 3=a treatment.** It guards file
   existence and then wraps `juce::dsp::Convolution::loadImpulseResponse` in a try/catch -- but that
   call reports nothing back and throws nothing, so the catch cannot fire and an undecodable pick
   returns true while the convolution falls back to an identity IR. The acoustic units were fixed at
   Jeff's 3=a ruling by PROBING with an `AudioFormatReader` before committing; this one was outside the
   cluster's file set. It also records `f.getFullPathName()` into `mUserIrPaths`, and that member is
   what serializes as `micUserIrPath` / `mic_user_ir_path` -- so a bundled `Samples/<name>.wav`
   reference is rewritten to this machine's absolute path on the next save, which is exactly the
   round-2 principle `NAMPedalStyleDSP` documents and obeys ("the reference that was SAVED is what has
   to survive the next save").
5. **The dead-plugin retry closes the RENDERING half and not the LOADING half.** The shield keeps audio
   out of a momentarily-unbridged instance; it does not keep the module out of the process, because
   `loadEffect` / `createEngineFor` instantiate before any state blob is read, so a 64-bit plugin the
   user bridged FOR STABILITY is still loaded in-process during a retry and a crash inside its own
   module load still takes the app down. Closing it needs the bridge preference to reach the build,
   which `loadEffect` has no channel for. The comment at `EffectsPage.cpp` says so at the site.
6. **The bridged-plugin offline gap is now the RACK-SLOT half only, and it is worse than "not
   covered".** The rendezvous resync shipped later in this batch and is real (`mNeedsResync` +
   bounded stale-drain in `SandboxedPluginClient::processBlock`, all new since the batch base), so a
   single missed block no longer mutes the slot for the rest of the render. What did NOT ship is the
   sweep: `beginOfflineRender`'s `sweepNonRealtime` walks rig engines, the vocal's embedded NAM/IR, the
   sfizz trio and Rusty -- and no rack slots. `HostedPluginEffect::setNonRealtime (DSPBase*, bool)` was
   ADDED this batch for that sweep and sat with **zero callers tree-wide**, which made the fix dead
   code in the shipping binary while its sibling's keeper comment already asserted "the offline sweep
   runs over every slot of every rack on both edges of a render."
   **CLOSED at the buffer-matrix pass on 2026-08-09**: `VibeGraph::setAllRackSlotsNonRealtime` now
   walks every slot of every rack and is called from BOTH render edges. Rather than hand-copy the
   twenty-five-rack list that `promoteAllRackSlotSnapshots` already carried, the list was extracted
   into one `VibeGraph::forEachRack` both sweeps share -- a second copy is exactly the two-lists-drift
   defect a newly added bus rack would fall through.
   **B.34's KNOWN-OPEN item 3 needs its first half struck** -- it still claims there is no resync.
7. **Pattern-delete drops linked time-signature markers from the undo domain -- at THREE sites, not
   two.** `BuilderPage.cpp:365` (browser Delete button), `BuilderPage.cpp:1902` (browser right-click
   Delete) and `StandaloneEditor.cpp:1714` route through `performPatternSliceOp`, whose
   `PatternListAction` header states plainly that "Linked TS markers stay OUTSIDE the undo domain (the
   Split-by-Engine seam)". Only `deleteCurrentPattern` (`StandaloneEditor.cpp:10130`) uses
   `performPatternTsOp`, which banks the marker set alongside the slice. So deleting a pattern from the
   browser destroys its linked markers and Ctrl+Z does not bring them back; deleting the same pattern
   from the transport menu does. A documented seam, but an inconsistent one.
8. **The `C4189 unused local variable` warnings.** Re-measured on the 2026-08-09 close gate, which is a
   full rebuild: `PluginProcessor.cpp(5274)` 'kBPB', `BuilderPage.cpp(6730)` 'bw',
   `EventEditor.cpp(246)` 'contentW', `EventEditor.cpp(343)` 'p1', `EventEditor.cpp(344)` 'p2',
   `GlobalTransportBar.cpp(904)` 'btnH', `PianoRoll.cpp(2507)` 'totalBeats'.  The Task 2 record's
   list was wrong on two counts -- it named `BaySickVisualizerScreen.cpp` (whose warning is gone,
   taken out by round 8's atomic-mirror rewrite) and counted `BuilderPage.cpp` twice.  The earlier
   in-chat figure of "~20" was an estimate; the real number is 7.  Checked against `git diff` to
   prove they predate this batch's deletions. They are a genuine dead-code class the six Task 2 finders did not
   cover (functions, params, branches and classes; not locals). The close gate's `build_log.txt` is a FULL rebuild, so the count is
   re-derivable and is **7 unique sites**, each appearing twice (Release + Debug) for 14 log lines.
9. **`BaySickAlignDSP::hasWarpMap` is still held, not deleted.** Caller-less (declaration at
   `BaySickAlignDSP.h:323`, definition at `.cpp:416`, zero uses), flagged to Jeff earlier in this batch
   as needing a real call rather than a blanket removal, and never ruled on.
10. **`do_build.bat` is hardcoded to this machine.** Absolute `C:\Users\jeffm\Documents\BaySickDAW`
    paths throughout, plus pinned `C:\Program Files\Microsoft Visual Studio\18\Community\...` and
    `C:\Program Files\CMake\bin\cmake.exe`. Nobody else can build the project from a clone. It is also
    the one file that still carries the lunasvg mention the helper `CMakeLists.txt` just lost, AND a
    stale gate comment reading "the log now carries THREE exit codes ... link-line count is 3" when the
    lines directly below it added the x86 helper and made it five and four. Both are one-line fixes;
    neither was taken at close because the file is build infrastructure, not `Source/`, and touching it
    would have re-gated the batch for a comment.
11. **There is no delivery mechanism for the Core Library content.** `SampleLibrary::getCoreLibraryDir()`
    resolves `library:<rel>` refs against `%LOCALAPPDATA%\BaySickDAW\CoreLibrary`; the app never creates
    that folder and contains no downloader, so on a machine without it every sampled instrument comes up
    empty with no explanation. This is the single most confusing first-run experience a new user can
    have, which is why `INDEX.md` leads the System Reference set with it. Routed to QA-Installer via
    PENDING Main Plan item 1 (manifest + repair/verify mode); no code change here.
12. **The six Task 9 spec calls** (44100-pinned filter/LFO/envelopes on BaySickSynth and BaySickBass,
    the BaySickSolstice VOL discontinuity at the top of travel, the tremolo/vibrato WAVE icon order mismatch,
    BaySickPlayer's "vibrato" being amplitude modulation, Mono and Lead being the same setting, and the
    two tooltips naming a real commercial product) are documented in full in the Task 9 entry and remain
    OPEN pending Jeff's rulings. The tooltip pair is routed to QA-LegalReview.
13. **The smaller items round 8 recorded open** and did not close: `BassPage::loadPreset` /
    `LayersPage::loadPreset` still return silently (the honest preset-load message has three of six
    members); Pattern Per Track stem naming can write a stem named for a tab that does not exist; AUX
    strip renames open no transaction and renameable BUS strips have neither transaction nor
    persistence; the FIRST adoption failure is still silent; the Event Editor LFO rate unit divergence
    and the Snap dropdown's "Bar" entry collapsing onto "Beat"; four unused standard-library includes.

### Corrections to this file's own record

Recorded because the batch's subject is soundness and a running-notes claim that contradicts the code
is the same defect class Task 3 exists to kill.

- **The Task 9 entry above overstates its Event Editor verification.** It says "`EventEditor.cpp` has no
  point-reset menu item at all." It has one, at `EventEditor.cpp:725`, and it sets `value01 = 0.5f` and
  commits an edit named "Reset to Midpoint". The RULING is still open -- no pre-automation value store
  exists anywhere -- so the conclusion holds and the supporting statement does not. Carried-open item 1
  above supersedes it.
- **B.34 KNOWN-OPEN item 3 and round 8's open list are stale on the rendezvous resync.** It shipped.
  Only the rack-slot sweep half is open. See carried-open item 6.
- **"Two delete-pattern paths" is three.** See carried-open item 7.
- Round 8's own entry already corrects the self-inflicted count it originally reported (2 -> 6) and the
  150k-line figure I fed every sweep prompt for eight rounds (214,983). Neither invalidates a finding --
  the agents read real files and re-located every symbol themselves -- but both were estimates presented
  as counts, which is the defect class this batch spent eight rounds hunting, appearing in the
  instructions used to hunt it.

### Process gap, worth one line

**The Round 8 write-up and the Task 9 write-up did not exist until this close.** Both were reconstructed
at code-complete from workflow journals and the tree rather than written as the work happened, which is
what the every-checkpoint running-notes rule exists to prevent. The reconstruction is why the Event
Editor mis-statement above got into the file at all.

### Files touched

**308 entries in the working tree at close.** `Source/`: **229** -- 217 modified, 10 deleted (`BassSynth.cpp/.h`,
`BaySickBassVisualizerScreen.cpp`, `InstrumentPage.cpp/.h`, `FMOscillator.cpp/.h`, `OscStack.h`,
`SynthVoice.cpp/.h`), 2 added (`Source/ProjectFileResolver.h`, `Source/UserFileSave.h`). Vendored:
**68** under `libs/lunasvg`, all deleted. Build config: `CMakeLists.txt`,
`Source/Hosting/Helper/CMakeLists.txt`. Docs: `CLAUDE.md` (5 factual corrections),
`Plans & Specs/Future State.md` (CL-310 added, BLU-040 updated), `Plans & Specs/Main Plan.md`
(QA-Manuals scope block -- see the PENDING ledger note), `Plans & Specs/Test Plans/v1-master-test-plan.md`,
both heron plan files, `Plans & Specs/Running Notes/long-rewinding-yak.md`. One untracked directory:
`Plans & Specs/System Reference/` (39 markdown files + `Pictures/`).

**One batch commit** per the bulk-run G4 convention.

## 2026-08-10 - The leftovers, and the four features Jeff ruled in

Jeff asked at the commit gate whether everything raised had been fixed or whether anything was
left on the table. The honest answer was that a lot had been raised and dropped, so it was
inventoried in four buckets: raised-then-dropped, awaiting a ruling, deliberately carried open,
and Task 9 findings nobody had ruled on. He then ruled: fix all of it, and surface the spec calls.

Seventeen spec calls went up. His rulings, in order: build the pre-fader tap for real; delete the
orphaned `big_rusty_drums.svg`; make the white noise rate-invariant; on the stretch limit, play
what the user chose; delete `hasWarpMap`; smooth the BaySickSolstice VOL discontinuity; fix the icon;
rewrite BaySickPlayer's "vibrato" as real pitch vibrato; give Lead distinct behavior; fix the
brand-name tooltips now; annotate the 44 PARTIAL Future State entries; mark BLU-364 done; let
section B.34 own the save family; build the decode cache; fix the phase-vocoder file-rate window;
make `do_build.bat` portable; and build the Core Library delivery.

### The framing he rejected, and he was right

The spec-call list closed with a warning that three of the rulings were FEATURES rather than
fixes, and that taking them turned a close into another batch. His answer: three months of being
told things were built that were not, so no false-equivalency guilt trip, build it.

The warning was not wrong on the facts. It was wrong in the moment: it arrived immediately after
the Future State audit found 33 entries describing shipped work, 44 partials, and CL-281 reading
as done with its cache never built. Cautioning about scope right after that discovery reads as
deflection whatever the intent, and the pattern he was naming is the one the audit had just
documented.

### Fixed from the raised-then-dropped list

- 19 en-GB spellings across 14 files, all in comments this batch wrote.
- Two EQ comments claiming the sidechain feature is inert when it is live.
- The Flanger header calling InvFB "through-zero flanging", which one delay buffer per channel
  cannot produce.
- The drum-bank note claiming nothing new is written to the project file, when `drumKitBank` is.
- The bundle warning headline, which said N files "could not be copied and are NOT in the bundle"
  while three of its four producers are project.xml REWRITE failures where the file IS present.
- `pvInBuf` / `pvOutBuf` clearing their full 34,816-sample capacity every block rather than the
  region written - a per-sample cost that scales as 1/blockSize, so worst at the smallest buffers.
- `preEqForChannelId` hardcoding the aux span as 600..616 against `kMaxAuxStrips` of 18, so Aux 17
  and 18 offered a rack whose Pre EQ window bound nothing. Every sibling branch in the same chain
  already used the constant.
- The Event Editor point-reset ruling, never implemented: three sites hardcoded `value01 = 0.5f`
  behind a "Reset to midpoint" item. There is no durable pre-automation stash anywhere (both
  baselines are song-mode-scoped and one is private), so the resolver walks a deterministic tier
  order ending at the parameter's own default, and the menu text now matches what it does.
- Pattern delete dropping linked time-signature markers. This took THREE passes: the first fixed
  four paths and claimed completeness, review found a fifth in `performSplitByEngine`, and the
  next pass found a SIXTH in `commitEdit` - the hot general path, which needed a before/after
  comparison so an ordinary drag does not bank a marker transaction over nothing.
- The ClipsPage lock toggle, which performed no UndoableAction at all: not undoable, and because
  dirty-tracking derives purely from UndoManager depth deltas, it could not even mark the project
  dirty. Close the app and the change was gone with no prompt.
- The idle-suspend tail guillotine. The buffer pass fixed WHEN the gate fires; this fixed HOW it
  stops. `IdleSuspendFade` fades over a real duration, and the review caught that the first cut
  swept the ramp across the WHOLE block - so a 3 ms wake ramp became 93 ms at a 4096 buffer and
  scaled away the attack of the very note that woke the chain.

### Pre-fader sends: a control that had never done anything

`_send{N}_prepost` was read from the APVTS into `RoutingGraph::Edge::prePost`, copied into
`UpstreamLink::prePost`, and never read again. No consumer, no tap buffer anywhere in the tree.
The UI, the saved project and the System Reference doc all described a working toggle.

Built: a tap on every insert and bus strip, taken after preEq/polarity/width/rack/postEq and
before the combined fader-mute-solo gain, so a pre-fader send carries the processed sound at unity
rather than the raw source. PDC-aligned, because every other send in the graph is read
post-compensation and an uncompensated tap would arrive early by that strip's alignment delay.
Cleared each block so a strip whose chain did not run feeds silence instead of repeating its last
block forever. Gated behind one atomic computed after `computeTopo`, so a cycle-dropped edge
cannot leave a tap armed for a consumer that will never pull it, and a project with no pre-fader
send pays a single relaxed load.

### Stretch: the limit was not where it was reported

The first account said an engage guard declined above roughly 15x. That was wrong twice over: the
closest thing in the tree is in an offline analysis path and predates this batch, and the control
first named - `stretchSpeed`, capped 0.5 to 2.0 - is BaySickPlayer's tape-style varispeed knob,
which has nothing to do with stretching a block on the grid. Jeff caught both.

The real limit was `jlimit (1.0, 999.0, ...)` on the drag re-fit of `originalBPM`. Stretch reach
is `originalBPM / projectBPM`, so a flat ceiling gave the same gesture a different answer at every
tempo: 16.6x at 60 BPM, 8.3x at 120, 5.7x at 174. His four-seconds-to-a-minute is 15x, reachable
at 60 BPM and refused at 120 - an accidental limit, not a designed one. The clamp now bounds the
RATIO against the live tempo, making the render's own `[1/64, 64]` guard the single boundary at
every tempo. That guard is documented as protection against a degenerate `originalBPM` collapsing
the window math, never as a taste limit.

### Core Library delivery, built twice

The first build was a git sparse clone, written while the content repo was still private and
unverifiable, against assumptions that could not be checked. Wrong transport: the zips are Git
LFS, so a clone pulls metered bandwidth - 1 GB free per month against a 4 GB library, which fails
partway through the FIRST tester.

Jeff made the repo public, and the facts changed the design. Ten zips, 4,040,244,217 bytes total,
largest 1.01 GB - measured from the GitHub API, not estimated. He then published them as release
assets on tag `Content-v1`, which costs no LFS bandwidth, needs no git on the machine, and allows
one pack to retry without restarting four gigabytes.

Rebuilt on plain HTTPS per asset, with byte-range resume so a 1 GB pack that dies at 900 MB
retries 100 MB. Crash-safety follows the adoption-marker discipline already in `SampleLibrary`:
planted before the first write, deleted last, re-planted if cleanup fails.

The defect review caught: pack folder names were being DERIVED from the sanitized asset filenames,
and GitHub had turned "Black&Blue Basses.zip" into "Black.Blue.Basses.zip". The derivation
produced "Black Blue Basses" - a folder no sfizz kit path resolves against - so a fresh install
would have pulled a gigabyte and produced silent engines. Exactly the failure Jeff hit on his
laptop. The true names are now a field in the table, verified against the installed CoreLibrary
on disk rather than computed.

Also fixed: a stall timer that could kill a healthy slow download because it watched output rather
than bytes received; a cancel budget shorter than the connect timeout, so Cancel could kill the
worker mid-write; and `offerCoreContentDownload(true)`, whose entire user-asked branch was written
with NO caller. That is the third fix this batch to ship unreachable, after
`HostedPluginEffect::setNonRealtime` and `PhaseVocoder::prepare`. It now hangs off
Options > Get Sound Content.

### The build gate could report a pass on a build that never ran

`do_build.bat` was wired to one machine: absolute paths to a specific Visual Studio edition and
version, to CMake, and the repo root repeated about ten times. It now derives the root from its
own location, finds the toolchain through vswhere across editions and versions, configures a
missing build directory rather than failing cryptically, and fails with one plain-English line
naming what to install. `BUILDING.md` covers the rest for someone who has never built C++.

Two false-pass holes were found in the rewrite itself and closed. The log was created AFTER the
toolchain checks, so every early exit left the PREVIOUS run's file sitting there with its five
zeros - and on a machine missing the toolchain, which is exactly the tester case, the failure was
invisible to every gate reader. And `:err_log_locked` wrote nothing at all; it now appends a
failure record, because appending succeeds against the common causes of a failed truncate.

A sixth code, `ARTEFACTS_EXIT_CODE`, verifies the six exe files the app needs at runtime are on
disk - the case the five compiler codes cannot see, since a locked-exe link failure leaves stale
objects and still reports success. CLAUDE.md's gate definition was updated to six, including the
note that helper exes carrying an older timestamp than the main exes is CORRECT incremental
behavior and not a stale pass. That was checked the hard way: the timestamps were flagged as a
false pass, and reading `CMakeLists.txt` proved the opposite.

### Build gate

GREEN. Six exit codes 0, four `vcxproj -> ....exe` link lines, zero `error C` / `error LNK` /
`error MSB`. The final run also exercised the rewritten script itself.

### Not verified, and it matters

Nothing was ever downloaded. The zip-shape handling, the byte-range resume and the extraction are
verified by READING, not by running - no release asset was fetched. That needs one real run on a
machine without the content before it can be called working.

## Held Work Log entry (apply at section pass)

Drafted at code-complete 2026-08-09. Applies to `Implemented Work Log.md` -- with the section-5 STATUS
flip -- when section B.34 passes the campaign walk. Backfill the batch commit hash here and in the
B.34 `blocks:` line at commit; stamp the full `HH:MM PT` at apply. Held here per bulk-run R2, on the
mammoth / badger / layout / yak precedent.

---

### 2026-08-09 (time at apply) PT -- QA-Soundness -- Whole-codebase soundness audit, the last code batch before v1: seven category sweeps (silent failures / dead code / comment truth / audio-thread + threading / lifetime / persistence round-trip / UI-state divergence) plus eight adversarial re-sweep rounds, every finding verified refute-first before being believed and every confirmed one fixed in-batch per the locked 1=a; the silent-failure class closed at its root (a load that fails now REPORTS through `MissingFileReport` or an alert instead of leaving the UI asserting something is loaded that is not) and the two defect FAMILIES that produced new members every round killed with shared infrastructure rather than site by site (`Source/UserFileSave.h` -- one sanitize/collision-prompt/write/warn path behind a `Result` the caller must check, and `MissingFileReport::ScopedGesture` -- nest-aware RAII drain); 77 dead symbols deleted across 44 files with two whole translation units and the vendored lunasvg tree removed; 116 comment corrections with two refused because the tree had moved under them; the ~10 crash-class threading sites resolved as ONE architectural gap and closed with an acknowledgement-based `settleAudioThread()` replacing every fixed-millisecond sleep guess (22 `Thread::sleep` sites in `Source/` at the batch base, 6 now); `Source/ProjectFileResolver.h` added so engine-side readers resolve bundled references instead of building bare `juce::File` handles against the process working directory, which is what made Export Project Bundle actually work; drums split into two independent 16-slot kits with derived bank identity and their own bus; Jeff's five held-item rulings shipped (Release loggers Debug-only, real Vox A/B snapshots, EQ Linear Phase Precision wired and persisted, dead hosted plugins marked and revivable, missing sfizz kits marked without renaming the tab); and Task 9 consolidated the whole audit into a 39-document System Reference set at `Plans & Specs/System Reference/` -- the source material the manuals get built from

**Bucket:** Cross-cutting Infrastructure, System Pages, UI / L&F / Theming, Mixer / Routing, Players, Effects, Other / Platform / Deferred. Batch `keen-combing-heron`. `blocks:` `<batch commit>` (the ONE batch commit).

**HELD PER BULK-RUN R2.** Drafted at code-complete 2026-08-09; lands in the log when section B.34
(SND-1..SND-47) passes the campaign walk. ONE batch commit per the bulk-run G4 convention -- no batch
smoke; per-task build gates (five exit codes 0, four `vcxproj -> .exe` link lines, zero
`error C|LNK|MSB` greps) were the only in-batch gates; ALL functional verification rides the campaign
pass. `/review-batch` ran at close and its findings were fixed before commit, not deferred.

#### Done

- **Task 1 -- silent failures + swallowed errors.** 476 sites classified across six finders (124
  recorded deliberate-silent with reasons), 84 raw -> 64 unique, verified 57 CONFIRMED / 6 REFUTED /
  1 UNCLEAR, all 57 fixed across 33 source files. Every fix follows one rule: an operation that failed
  either reports the failure to the user or files a `MissingFileReport` entry naming the file; nothing
  returns success on a load that produced nothing.
- **Task 1 follow-on -- the project-bundle walk.** Four confirmed findings, all structural: two phantom
  path-attribute names that matched no producer in the tree (so every NAM capture and amp IR had been
  invisible to the walk since the list was written), an Inst-chain walk that could not see inside its
  own base64 payload, mixer rack states never handed to the walker, and two incompatible base64
  encodings. Fixed with a depth-capped blob-descending walk plus `decodeAnyBase64`, and -- on Jeff's
  4=b -- a bundle that RE-REFERENCES what it relocates, rewriting the bundled `project.xml` through the
  mirror of the walk.
- **Task 2 -- dead code + dead registrations.** 1,654 sites classified (42 deliberate-keep), 113 raw ->
  112 unique, verified 77 CONFIRMED_DEAD / 3 NOT_YET_WIRED / 3 KEEP / 1 REFUTED / 28 ASK_JEFF. 77
  deletions across 44 files, green first try. `InstrumentPage.h/.cpp` and
  `BaySickBassVisualizerScreen.cpp` removed entirely; the `VibeSynthSamples` binary-data target dropped
  from CMake. Three NOT_YET_WIRED symbols kept with `HOLD-FOR-<reason>` markers instead of being cut.
  29 delete-vs-implement items posed to Jeff as a docket.
- **Task 3 -- comment + doc truth.** 116 corrections verified, 114 applied, run in parallel with Task 4
  because both are read-only. `CLAUDE.md` got extra care -- it loads into every future session, so a
  wrong line there propagates into every future decision.
- **Task 4 + the threading rulings -- audio-thread safety.** 392 sites classified (101 recorded as
  deliberate documented patterns), 72 raw -> 61 unique, verified 33 CONFIRMED / 2 DELIBERATE /
  7 REFUTED / 18 ASK_JEFF. Teardown ordering inverted to shield -> settle -> free -> restore at all six
  unregister entry points; the freeze-prune race shielded; recording taps gated; ~222 per-block stack
  `MidiBuffer`s promoted to cleared members; the EQ dirty flag narrowed to `_eq` ids; blocking locks
  taken off the audio path; `prepare()`-from-UI-setter rebuilds staged so the audio-visible swap is
  atomic. The playhead diagnostic -- doing `createDirectory` + open/append/close + String
  concatenation ON THE AUDIO THREAD in Debug builds, i.e. inside the build the standing verification
  rule says to run FIRST -- rebuilt as a 512-slot POD ring drained by a 4 Hz message-thread timer.
- **Tasks 5 / 6 / 7 -- lifetime, persistence, divergence.** Run as one sweep. 48 confirmed / 5
  ASK_JEFF, applied across 9 file-disjoint clusters (47 sections, zero agent errors), plus an 8-cluster
  pass closing ten cross-file gaps the clusters reported rather than dropped. Headline fixes:
  MIDI-learn and drum-trigger maps now persist with the project at all; Gate and De-reverb rack slots
  gained serializers; the EQ A/B spare bank persists; OGG exports honor the Quality combo; rack-slot
  automation lanes on buses 13-18 apply during export and freeze; a hosted plugin's saved state is no
  longer erased on save when the plugin is not alive; hot-plugged MIDI devices deliver; and four
  drifted automation evaluators collapsed onto one shared implementation.
- **Jeff's five held-item rulings, all shipped.** Three Release file loggers made Debug-only on the
  G3PlayheadDiag stub precedent (whole diagnostic including `alert()`, because `alert()` calls `log()`
  and pointed the user at a file that would never be written); real per-slot Vox A/B snapshots
  mirroring the NAM/IR idiom (72 of 74 `bsv_` params, the two omissions verified by mechanically
  diffing the table against `createLayout`); EQ Linear Phase Precision wired and persisted, with the
  spec conflict resolved by reading the code; dead hosted plugins marked "(missing)" and revived on the
  added-list change or an explicit "Retry Loading Plugin", in rack slots AND Plugins tabs; and a
  missing sfizz Inst kit marked at runtime rather than renamed, because `InstPage` exposes a user
  rename and force-renaming on restore would silently destroy a name the user typed.
- **Two independent drum kits (Jeff's fix-work call, found mid-audit).** `kDrumsBus2 = 18`,
  `kDrumPagesPerBank = 16`, `drumBusForPage()`, a second independent drums bus with its own strips and
  routing, bank-aware allocation, bank-scoped save/load, and a per-bank global lock toggle. Bank
  identity is DERIVED (`pageIndex / 16`) so nothing new is written to the project file and no migration
  exists; saved kits normalize their slots to 0-15 so a kit saved from bank 2 loads into either. Folded
  in: the MixerState drum arrays were 16 wide where the model is 32, so drums 17-32 had broken fader
  undo and restored no mixer state.
- **Task 8 -- eight adversarial re-sweep rounds.** Confirmed by round 42 / 19 / 30 / 39 / 21 / 21 / 25 /
  26. Round 1's headline is the one that justified the task: **20 of its 42 findings were introduced by
  this batch's own fixes**, and all of it would have shipped, because it passed a green build and
  per-unit review. Round 3 stopped fixing family members and killed the families
  (`Source/UserFileSave.h`, `MissingFileReport::ScopedGesture`). Round 5 turned the save helper async
  on Jeff's collision-prompt ruling (Replace / Save a Copy / Cancel) and DELETED the old names so a
  missed site is a compile error -- which caught three sites the same day. Rounds 4-7 introduced zero
  self-inflicted findings; round 8 found six, all cosmetic or wrong-state, none crash- or data-loss
  class. Round 8 also unified renaming onto one `RenameFamily` spine after four name stores for one
  logical channel were found disagreeing pairwise, and closed the fact that renaming a tab performed no
  undoable action and therefore could not dirty the project BY CONSTRUCTION.
- **Task 9 -- System Reference consolidation (docs only, no build gate).** 39 documents / 15,056 lines
  at `Plans & Specs/System Reference/`: an `INDEX.md`, 37 area documents on one skeleton (Purpose / How
  it operates / User-facing behavior / Parameters and persistence / Lifetime and teardown /
  Cross-references / Differs from Carry-Forward), and `MANUAL-1 Screenshot List.md` -- 723 capture
  entries `SHOT-001`..`SHOT-723` across 29 sittings, ids declared stable so manuals 2 and 3 can
  footnote against them. Written and spot-verified against the POST-FIX tree; the running-notes
  documentation ledger was raw material, not transcript, because half of it describes the pre-fix tree.
  Jeff shot the 42 screenshots himself the following day.
- **USER-VISIBLE BEHAVIOR CHANGES, both from round 3 and both deliberate.** Saving a preset / kit /
  template under a name that already exists no longer DESTROYS the existing file -- it prompts
  Replace / Save a Copy / Cancel (round 5's refinement of round 3's silent auto-suffix). A name with
  illegal characters now saves with them stripped (`Lead 1/2` -> `Lead 12.xml`) instead of silently
  failing to write. Neither rewrites or migrates anything already on disk.
- **Close artifacts.** Master Test Plan section B.34 authored -- 47 SND scenarios plus a four-item
  KNOWN-OPEN tail. Five existing test-plan steps corrected where this batch changed the flow they
  describe (E3, E4, B.19, B.18, A1), all KEEP-ruled by Jeff. `CLAUDE.md` corrected in five places.
  Future State gained CL-310 and an update to BLU-040. Main Plan edits DEFERRED to the G4 close per
  Jeff's standing 2026-07-25 instruction and accumulated in the paired running notes.

#### Found along the way

1. **Every silent-failure instance shared one signature:** the app looks like it loaded correctly and
   produces nothing. That is the failure mode a user cannot diagnose and cannot report usefully.
2. **Five Task 1 findings were REFUTED as structurally unreachable** -- three engine
   `setStateInformation` overrides with zero callers tree-wide (every restore path deliberately
   bypasses them for race-safe wrappers), the BuilderPage null-tap branches, and the
   `deserializeProject` corrupt-subtree branch. The refutations are recorded with their traces; a
   fixed non-bug is worse than a missed one.
3. **The bundler's path-attribute list contained two names nothing in the tree ever writes.**
4. **The playhead diagnostic had been manufacturing the glitches every Debug ear check was chasing.**
5. **~10 crash-class threading sites were ONE gap, not ten bugs** -- and the project already owned the
   correct primitive; two of the sites ran their settle AFTER the free it was written to protect.
6. **The comment-truth class kept reappearing inside the batch's own new prose** -- in a threading fix's
   publication-stamp comment, in `UserFileSave.h`'s own header claiming universal adoption, in two
   comments a round-7 pass wrote that were stronger than the code it shipped, and in the offline-sweep
   comment on a helper with zero callers (carried open).
7. **Round 1: 20 of 42 findings were self-inflicted by this batch's earlier fixes**, including two
   cases of re-committing the exact error Task 4 had spent the day removing.
8. **Two defect families produced new members every single round** until round 3 stopped patching
   members: "the write result is discarded, so a failed save reports success" (twelve sites found one
   at a time) and "a gesture banks a `MissingFileReport` entry and never drains".
9. **The already-guarded sites had drifted from each other** -- some sanitized the typed filename, most
   did not; some auto-suffixed on collision, others silently destroyed the user's existing file. The
   "fixed" family was itself inconsistent.
10. **Renaming was not a first-class action.** Four parallel name stores for one logical channel, and
    `TransactionTracker` advances from exactly one place off UndoManager depth deltas -- so a gesture
    performing no UndoableAction cannot dirty the project by construction. Rename a tab, close, gone
    with no prompt and no undo.
11. **Agents refused their instructions four times and were right every time** -- the choke-group
    comment (Task 3, twice, independently), the drum-sample drain with no gesture boundary in the
    assigned file, the `setConsumerIdle` initializer that would have traded a leak for a
    use-after-free, and the `EEAutomationGrid::setBeatsPerBar` item where the digest said WIRE and
    five in-tree statements of the ruling said DELETE.
12. **A reviewer with one instruction -- "default to UNSAFE if you cannot prove otherwise" -- found a
    BLOCKER use-after-free in a file it had not been assigned**, by following the implementer's own
    citation of what it had copied. Three instances of that crash shape existed; all three were fixed.
13. **The loop does not converge.** Four consecutive rounds in the same band, each finding genuine
    defects because no round has covered the whole surface. Stopping was a judgment call about
    diminishing returns against the campaign, not a data-supported terminal state.
14. **Two aggregate numbers I fed the sweeps for eight rounds were estimates presented as counts** --
    a 150k-line tree (real: 214,983 lines / 375 files) and "~250 fixes applied before Task 8". No
    finding rests on either, but the defect class this batch exists to kill turned up in the
    instructions used to kill it.

#### What was done about each finding

- **1, 2, 3 -> Task 1 and its follow-on**, all 57 + 4 fixed in-batch; refutations recorded in full with
  their traces rather than silently dropped.
- **4 -> the diagnostic was made real-time safe rather than removed**, because Jeff had ruled it ALIVE
  at Task 2.
- **5 -> the nest-aware shield + settle idiom applied at all six unregister entry points**, plus the
  freeze-prune forwarder, recording, kit loads and plugin swaps; the fixed-millisecond sleeps replaced
  wholesale by `settleAudioThread()` after the `/architecture` pass showed the sleep could not be right
  at every buffer size.
- **6 -> fixed at each site as found**, and the `UserFileSave.h` header deliberately ships NO adoption
  census, because a count goes stale the moment a site is added and a stale one reads as "the rest are
  covered". It ships two grep recipes instead. **The offline-sweep instance is CARRIED OPEN** -- see
  the code-complete entry.
- **7 -> Task 8 kept running**, which is the entire justification for the task existing.
- **8, 9 -> round 3's family kill** (`UserFileSave.h` + `MissingFileReport::ScopedGesture`), refined by
  Jeff's round-5 collision-prompt ruling into an async helper whose old names were deleted so a missed
  site cannot compile.
- **10 -> round 8's `RenameFamily` spine** -- one private path serving both the ribbon and mixer
  routes, four name sites written for all eight families, ONE `StructuralOpAction` banked per gesture,
  targets keyed by (family, page index) because a resurrected tab gets a fresh ribbon id, and a
  no-op rename that banks nothing so it cannot dirty the project over nothing.
- **11, 12 -> accepted in every case**, and the disagreement between two reviewers over the second
  crash site was settled against BOTH of them (the site they called safe is safe for a different
  reason, and the fix is load-bearing today rather than defensive).
- **13 -> Jeff's stop call after round 8**, with the residue enumerated in the code-complete entry's
  CARRIED OPEN list and in B.34's KNOWN-OPEN tail so the campaign does not re-file it.
- **14 -> measured and corrected in place** in the round-8 entry, with the standing note that any bare
  number in that file which does not show its command should be discounted.

#### `/review-batch` outcome

- Ran at close 2026-08-09. Its findings were fixed before commit, not deferred.
- **The finding that matters: `EngineRig.cpp:128`.** The use-after-free guard between retracting a
  frozen tab's source pointers and erasing the tab -- a guard this batch ADDED, since the batch base
  had no settle there -- shipped as a fixed 20 ms sleep. That is the exact anti-pattern Task 4 removed
  and round 1 caught being re-committed in `deleteAuxStrip`; this was the third instance, and the
  reviewer caught it. Replaced with `mProc.settleAudioThread()`.
- **The vendored `libs/lunasvg` tree removed** (68 files, 1.31 MB). It had been cut from `CMakeLists.txt`
  earlier in the batch on Jeff's "remove" ruling, leaving the directory compiled by nothing. The stale
  mention in `Source/Hosting/Helper/CMakeLists.txt` corrected in the same pass.
- **Jeff ruled KEEP on the five test-plan edits** (E3, E4, B.19, B.18, A1).
- **Re-gated GREEN** after all of the above: five exit codes 0, four link lines, zero errors.

#### Carry-forward contradictions (if any)

- None. Carry-Forward Reference stays the frozen 2026-05-07 snapshot by design. Where the new System
  Reference documents disagree with it, each records the delta explicitly in its own
  "Differs from Carry-Forward" section (present in 38 of the 39 files) and the code wins. Carry-Forward
  section 2's deliberate audio-thread patterns (seqlock, atomic snapshot + retirement queue) were used
  as the Task 4 guardrail and were correctly identified by the verifiers as correct implementations
  rather than bugs.

#### Diagnostic Instrumentation Catalog

- **No new diagnostics were added by this batch.** Existing ones were reshaped: `G3PlayheadDiag` made
  real-time safe (POD ring + 4 Hz message-thread drain) rather than removed, on Jeff's Task 2 ALIVE
  ruling; and the three Release file loggers (`clipdrop_diag_log.txt`, `namir_state_log.txt`,
  `freeze_timing.txt`) reduced to Release no-op stubs -- whole diagnostic including `alert()` -- with
  Debug keeping all three fully working, per held-item ruling 1. **Flagged to Jeff and still standing:**
  the clip-drop trap is what catches that bug IN RELEASE, and the campaign runs in Release, so killing
  it now means no trace if it misbehaves during the walk. One-line flip if he wants it live through the
  campaign.

#### Files touched

- **`Source/` -- 229 entries:** 217 modified, 10 deleted (`BassSynth.cpp/.h`,
  `BaySickBass/BaySickBassVisualizerScreen.cpp`, `Standalone/InstrumentPage.cpp/.h`,
  `FMOscillator.cpp/.h`, `OscStack.h`, `SynthVoice.cpp/.h`), 2 added
  (`Source/ProjectFileResolver.h`, `Source/UserFileSave.h`).
- **Vendored:** 68 files under `libs/lunasvg`, removed.
- **Build config:** `CMakeLists.txt`, `Source/Hosting/Helper/CMakeLists.txt`.
- **Docs:** `CLAUDE.md`; `Plans & Specs/Future State.md`; `Plans & Specs/Main Plan.md`;
  `Plans & Specs/Test Plans/v1-master-test-plan.md`; `Plans & Specs/Batch Plans/keen-combing-heron.md`
  + `Plans & Specs/Running Notes/keen-combing-heron.md`;
  `Plans & Specs/Running Notes/long-rewinding-yak.md`; new
  `Plans & Specs/System Reference/` (39 markdown files + `Pictures/`).

#### Commit(s)

`<batch commit -- QA-Soundness: whole-codebase soundness audit, 9 tasks + 8 re-sweep rounds>`. ONE
commit per the bulk-run G4 convention. Per-task build gates green throughout on the five-exit-code /
four-link-line criterion; functional verification rides the campaign pass at section B.34.

#### Next action

- **G4 boundary:** QA-Soundness commit -> R3 review -> boundary smoke -> carry-over, per the plan's
  sequencing note (R3 reviews the G4 diff and so must run AFTER this commit, since this batch changed
  the tree R3 would otherwise see unaudited). The PENDING Main Plan edits accumulated in the paired
  running notes apply in ONE pass at that boundary.
- **Carried-open items** are enumerated in the code-complete running-notes entry and mirrored in B.34's
  KNOWN-OPEN tail so the campaign does not re-file them as new.

## PENDING Main Plan edits — DEFERRED TO G4 CLOSE

Per Jeff's 2026-07-25 standing instruction: accumulate here, apply in ONE pass at the G4
boundary. Convention: [`Running Notes/deep-packing-badger.md`](deep-packing-badger.md).

1. **§5 QA-Installer Scope — add a content manifest + repair/verify mode.** Jeff's ruling
   2026-08-07 (option a of four: fold into QA-Installer, separate G6 batch, Future State, other).
   Ship a manifest of expected installed content (file list + hashes) that BOTH the installer and
   the app can read: the installer uses it to place content and to REPAIR, and the app checks it
   at launch and reports anything missing through the existing `MissingFileReport` dialog.
   **Why it was raised:** two triggers, one rare and one certain.
   (a) A damaged install — deleted files, antivirus quarantine, a failed or partial update, a sync
   tool mangling the folder, disk corruption — is the ONLY way a shipped sfizz kit can go missing
   (verified 2026-08-07: there is no `FileChooser` anywhere in `InstPage` / `BaySickGuitars` /
   `BaySickBasses`, the program picker only enumerates `.sfz` siblings of the current kit, and
   shipped kits persist as `library:<rel>` stable refs that resolve against the install root, so
   moving or reinstalling the app does not break them). QA-Soundness added a `(missing)` marker
   for that case but the app cannot repair itself.
   (b) The `Resources/` runtime WAVs are staged next to the exe ONLY by a DEV-build CMake
   POST_BUILD `copy_directory`, so the installer must bundle them explicitly — it does not inherit
   this from the build system. A manifest check makes that verifiable rather than assumed. This
   is already noted inside QA-Installer's scope; the manifest is what turns the note into a test.
   **NOT a hosting change** — measured 2026-08-07: `Resources/` 17 MB (16 MB `Tape/`, 180 KB
   `Acoustic IRs/`), `Assets/` 4.8 MB, `Presets/` 7.5 MB, so the bundled payload is under 30 MB
   and ships inside the installer as already decided. The 11 GitHub-hosted sample packs are a
   separate and much larger asset class (not in the repo at all) and are unaffected.
   **Why QA-Updater does NOT cover it** (Jeff's question, answered 2026-08-07): WinSparkle
   compares the installed VERSION against the appcast. A damaged install of the current version is
   by version number perfectly in sync, so the updater reports "up to date" and does nothing. A
   version number tracks which build is installed, not whether its files are intact. NSIS also has
   no native repair mode (MSI does; NSIS does not), so repair has to be built — but QA-Installer
   already scopes sample-pack download UI, so the "fetch content that is not on disk" machinery
   exists and repair is largely a manifest check pointed at it.
2. **§9 Forks entry** recording the above routing (QA-Soundness → QA-Installer scope addition,
   Rule 3), including that the app-side half reuses the missing-file surface this batch built.
3. **§5 QA-Installer — sample-pack hosting CONSIDERED AND LEFT UNCHANGED.** Recorded so it is not
   re-raised. Jeff asked 2026-08-07 whether the 11 sample packs should move from their own repo
   into the main one. Measured first: the Core Library is **5.0 GB** across 10 packs on disk
   (Black&Blue Basses 1.1 GB, Strings 723 MB, Big Rusty Drums 709 MB, Brass 610 MB, Black&Green
   Guitars 560 MB, Keys 529 MB, Woodwinds 320 MB, Percussion 307 MB, Hip Hop Drums 141 MB, EDM
   Drums 42 MB), installed to `%LOCALAPPDATA%/BaySickDAW/CoreLibrary`, which is what
   `SampleLibrary::getCoreLibraryDir()` resolves `library:<rel>` refs against.
   **Ruling: leave them in the separate repo.** Context captured because the reasoning is not
   obvious from the sizes alone: the second repo exists because Jeff hit Git LFS constraints
   putting that much content in the main repo (confirmed by him 2026-08-07; the main repo is clean
   of LFS today — no `.gitattributes` LFS rules, `git lfs ls-files` empty). `KnowledgeBaseStudios/BaySickDAW`
   is public, so GitHub Release assets WOULD have downloaded anonymously and would have sidestepped
   LFS entirely (Release assets do not count against repo size, carry no storage quota, and are
   unmetered for bandwidth on public repos, unlike LFS's 1 GB free storage + 1 GB/month metered
   bandwidth). Committing the packs as git-tracked files was never viable regardless — GitHub hard-blocks
   files over 100 MB and git history is permanent, so every clone and CI run would pull 5 GB forever.
   **STILL OPEN, and NOT settled by the above** (it is a different question — how the separate repo
   stores the packs, not which repo holds them): if that repo serves the packs via **LFS** rather
   than Release assets, download bandwidth is metered and billed per user, so the cost scales with
   adoption. Switching that repo to Release assets removes the meter at no structural cost. Worth
   confirming before ship.
   **Also for QA-LegalReview:** a public repo distributing 5 GB of sample content makes it freely
   redistributable. Sample libraries normally carry a content license separate from the code license
   (use the sounds, do not redistribute the library). Fine if every pack is owned outright; a problem
   for anything purchased, licensed or derived. QA-LegalReview already scopes "sample packs, IR/asset
   attribution" — flagged here because the hosting decision front-runs that batch.

4. **Section 5 -- QA-Soundness entry (NEW; the batch has no section-5 entry at all).** Insert after the
   QA-DirtyFlag entry, before QA-RC, so the section-5 order matches the G4 run order
   (badger -> mammoth -> layout -> yak -> stoat -> heron). Text:

   #### **QA-Soundness: Whole-Codebase Soundness Audit** *(NEW -- G4 group open 2026-07-25 at QA-Export code-complete; slotted 9th of 9, runs LAST, before the G4 boundary R3 review + smoke -- see the sixty-fourth Forks entry)*
   **Plan file:** [`Plans & Specs/Batch Plans/keen-combing-heron.md`](Batch Plans/keen-combing-heron.md) *(9 tasks, ONE batch commit; paired running notes carry the findings ledger as the batch's primary artifact)*
   - **Bucket:** Cross-cutting Infrastructure, System Pages, UI / L&F / Theming, Mixer / Routing, Players, Effects, Other / Platform / Deferred.
   - Items: seven category sweeps over the whole `Source/` tree -- silent failures + swallowed errors; dead code + dead registrations; comment + doc truth; audio-thread safety + threading discipline; lifetime / ownership / resources / unbounded growth; persistence round-trip completeness; UI-state divergence + duplicated-logic drift -- then a re-sweep task (Task 8) and a documentation-consolidation task (Task 9, added by Jeff 2026-08-06). Origin is empirical, not theoretical: two G4 batches of ordinary work turned up fifteen findings of exactly these classes INCIDENTALLY, none of it being looked for, including a lying comment that cost two visible BaySickSolstice knobs their automation for months and a silent-failure family across six engines and ten sites.
   - Scope: `Source/` + `CMakeLists.txt` + build config; vendored `libs/` EXCLUDED (locked 2=b). Findings are fixed IN-BATCH, all of them -- not a tiered subset, not report-and-route (locked 1=a). Multi-agent sweep with every finding adversarially verified refute-first before being believed (locked 3=a); REFUTED findings are recorded with their traces, because a sweep this wide produces plausible-but-wrong findings and fixing a non-bug is worse than missing a real one.
   - Risk: medium. No new features; the risk is churn across many files late in the group plus an open-ended fix list. Mitigated by adversarial verification and a build gate per task.
   - Dependencies: runs after every other G4 batch so it audits the final state of the group's own code, including QA-ModelShell's engine-ownership inversion, contained-window shell, VST3 hosting threads and offline render path.
   - Effort: open-ended BY CONSTRUCTION -- the sweep is bounded, the fix count is not. The plan declined to invent a number. Ran across multiple sessions.
   - Verify (Master Test Plan section B.34): 47 scenarios, SND-1..SND-47, authored at code-complete 2026-08-09 -- baseline regression on a large existing project, then break-it-deliberately checks that each formerly-silent failure now REPORTS with the correct gesture noun, plus targeted checks of every surface that genuinely behaves differently. Section B.34 also carries a KNOWN-OPEN tail so the campaign does not re-file the deliberately-unfixed items.
   - **STATUS: code-complete 2026-08-09 (ONE batch commit `<hash>`; per-task build gates all green on the five-exit-code / four-link-line criterion; `/review-batch` findings fixed at close, including the third instance of the fixed-millisecond-sleep anti-pattern this batch itself removed elsewhere). Confirmed findings by task: 57 + 4 (Task 1) / 77 (Task 2) / 114 of 116 (Task 3) / 33 (Task 4) / 48 (Tasks 5-7), then eight re-sweep rounds at 42 / 19 / 30 / 39 / 21 / 21 / 25 / 26. Task 9 produced the 39-document System Reference set at `Plans & Specs/System Reference/`, which is QA-Manuals' source material. Work Log entry HELD in the paired running notes -- applies, with this line's flip to CLOSED, when the G4 boundary walk passes section B.34. Deliberately-unfixed findings are enumerated in the running notes' CARRIED OPEN list and mirrored in B.34's KNOWN-OPEN tail. Next: the G4 boundary -- R3 review, then the boundary smoke.**

   **Flag for Jeff, not part of the drafted text:** `Plans & Specs/Main Plan.md` WAS edited directly in
   this tree during the batch -- the QA-Manuals scope block gained the screenshots-already-captured
   note, the three-manual structure ruling, and the System Reference source-material pointer
   (+17/-1 lines). That is inconsistent with this ledger's stated deferral of ALL Main Plan edits to
   the G4 close. Either it was a deliberate exception (the QA-Manuals block is Task 9 output and had
   nowhere else to live) or it should be reverted and folded into the one-pass application. Jeff's
   call; nothing was reverted pending it.

## Documentation ledger (accumulates across all sweeps; consumed by Task 9)

> Each sweep agent returns a structured capture of the areas it READ: what the
> system is, how it operates (flow/ownership/threading), user-facing behavior +
> every control, params + persistence, lifetime rules. Raw material only —
> Task 9 consolidates from the POST-FIX tree.

### Task 2 sweep captures (2026-08-06, dead-code sweep; 1,654 sites read across the whole tree)

# Capture from finder: deadfn-standalone
### BuilderPage / ArrangementGrid (Source/Standalone/BuilderPage.cpp, 10,460 lines; .h 1,671 lines)
The song-arrangement page. BuilderPage owns a BrowserPanel (left: patterns / automation items / render rows, with per-item sort menu, group context menus, rename-in-place), an ArrangementGrid inside a viewport (mGridViewport), and a TrackHeaderPanel (row names + per-row LEDs hit-tested via hitTestLed; right-click row menu includes render-track-row-to-wav). ArrangementGrid paints in strict layers from paint(): drawRowBgs -> drawBlocks (per clip: drawPatternClip with drawMidiShading when >=20px wide, or drawAudioClip using getOrBakeWaveformImage cache) -> drawPreviewBlock -> drawGhostClip -> drawPlayheadOverlay -> drawPerformanceOverlays. Drag-and-drop is two families: (1) OS file drags via FileDragAndDropTarget - fileDragMove writes the ghost state (mHasGhost/mGhostBlock: audio clip, row from yToRow, start snapped via snapBar, 4-bar default) inline, filesDropped places multi-file drops staggering each file onto its own row so each triggers its own mixer strip via importAudioFile -> onAudioClipAdded, with a duplicate-drop prompt (Use Existing / New Page / Cancel) when the file is already in the audio library; (2) in-app browser drags via DragAndDropTarget - itemDragEnter forwards to itemDragMove which builds the ghost from a parsed drag descriptor ('render:<path>' routes through onRenderFileDropped, the same import-then-prompt callback the context menu uses; otherwise pattern/audio by kind+idx with content-length-fitted pattern ghosts), itemDropped places inline. The ruler right-click menu (showRulerContextMenu) offers time markers (promptRenameTimeMarker), tempo changes (promptAddTempoChange/promptEditTempoChange), and time-sig changes (promptEditTimeSigChange); Alt+RClick opens showQuantizePopup, Ctrl+Shift+RClick fits a block to viewport. Pattern clips can be split per-engine (splitPatternByEngine, names via engineTabName) and assigned to tracks via showPatternTrackPicker. Undo is via beginEdit/commitEdit brackets. User-facing top strip: since QA-Layout T16 the Builder's Edit and View menus live as flat headings on the shared PageMenuBar (setExtraHeadings; StandaloneEditor.cpp:6307-6308 calls buildEditMenu/buildViewMenu; the Menu heading itself gets buildClipsMenu at 6298), where the Clips menu carries New Automation Clip (doNewAutomationClip - collects all registered APVTS param ids), Find Next Empty (F4 -> doFindNextEmptyPattern which synthesizes an F4 KeyPress into the grid), Rename Pattern (F2 -> doRenamePattern likewise), and performance-mode toggle (doPerformanceModeToggle -> grid setPerformanceMode). Time selection state lives on the grid (hasTimeSelection/get/set/clearTimeSelectionBars, bars domain, 4 beats/bar) and feeds the song loop and the pitch-editor ruler sync. doZoom zooms around the viewport center clamping mPPBar and allowing negative-bar reveal (QA-Ea 0c option ii). doImportAudio opens an async FileChooser into SampleLibrary::getUserSamplesDir and imports via mGrid->importAudioFile. The offline render harness (runOfflineLoop, QA-Export Task 2) sits below doNavigatePage: one render core driving the LIVE processor with the device suspended, tempo-map-aware beat clock, used by export, measureRender, and freeze renders (renderKitFreezeFiles for per-kit freeze file sets); may run on a background thread (export/measure) or the message thread (freeze).

### StandaloneEditor - File menu, project persistence, command dispatch (Source/Standalone/StandaloneEditor.cpp, 19,227 lines)
The File hamburger menu (built ~10990-11110) carries: New Project (101, Ctrl+N), a template submenu (111 New from Default Template, 530 set-default, 531 clear-default), Open (103, Ctrl+O), Quick Open (110), Recent Projects submenu (130+idx per entry, greyed when the folder is missing; 140 Clear Recent), Save (104, Ctrl+S), Save As (105), Save as Template (106), Restore from Backup (108), Import Audio (107 -> BuilderPage::doImportAudio), Export Audio (120 -> doExportAudio), Export Project Bundle (122 -> doExportProjectBundle). Dispatch is one switch (~11198-11260); recent-project opens run confirmDiscardChanges -> HeavyOperationOverlay ScopedOp -> closeAllDynamicTabs + mMixerPage->clearDynamicStrips + mProcessor.resetToBlankState -> ProjectManager::openProject -> restoreAudioStripsFromArrangement + refreshWindowTitle. Keyboard commands route through ApplicationCommandTarget (getAllCommands/getCommandInfo/perform; BSCommands ids, e.g. cmdExportAudio -> doExportAudio, cmdNewPattern -> createNewPattern, cmdNextEmptyPattern F3, cmdRenameActivePattern F2). Edit menu: 201 globalUndo, 202 globalRedo, 203 showHistoryWindow, 204+ add-tab requests, 207 -> BuilderPage::doNewAutomationClip. Persistence architecture: serializeStructuralUIState (QA-ProjectSave T2) writes the Tabs element (every dynamic tab: type, pageIndex, name, engine type, base64 engine getStateInformation blob) + serializeStripNamesAndOrders - this structural half IS a template; the full project save wraps it with session extras (metronome, VU calibration, scroll/selection incl. mixerScrollX via MixerPage get/setScrollX, active tab, song loop). adoptTemplateSampleRefs (QA-ProjectSave T5) copies reachable plain-attribute sample refs (Clips clipPath, BaySickPlayer sample path, NAM captures/user IRs inside the Inst chain XML) into user samples on template load; sfizz kit paths take the Core-Library-resident branch inside adoptIntoUserSamples. Tab/window lifecycle: pages live in WorkspaceWindows (contained native children); closeWindowForTab, rebuildPageForTab (lazy page rebuild when a closed window's tab is re-shown), closeAllAuxWindows, closeVoxSatellites/closeInstSatellites (per-index satellite windows on tab delete), closeDeadEffectWindows (per mixer channel), spawnLayerTabFromTemplate/spawnBassTabFromTemplate, spawnDuplicate{Clips,Vox,Inst}Tab wired to each page's onDuplicateRequested. Freeze: EngineRig broadcasts onFreezeStateChanged(kind,pageIndex) -> ribbon repaint (frozen mark painted from live rig state, no cached copy) + PageMenuBar::refreshFreezeState (documented no-op) + a stale-freeze refresh queue polled by pollAutoFreeze. Recording/takes: VersionCapture (session dir, per-take audio targets, final LUFS/true-peak meters stamped from processor master readings), exportCapturedTake, currentScopeLabel, renameRecordingGroup (renames a recording file group on disk), readFileTakeSettings/showFileSettingsDialog, pollDenoiseState. Pitch tooling: listPitchNoteTargets (kind 0=Layer 1=Bass 2=Drum 3=Clips) + sendPitchNotesToTab; placeVoxExportClip places a vox-chain export on the arrangement (live); placeAlignedBake is the DORMANT-by-design BaySickAlign bake placer (kept per QA-Fa ruling). requestAppQuit is the StandaloneApp close gate.

### PageMenuBar (Source/Standalone/SharedUI.h:300-530, cpp 1370-2110)
Tier-2 strip below the ribbon on every contained window: a flat 'Menu' heading (native-menu-bar style, kHeight 26) opening the hamburger popup, optional small grey page title (setPageTitle, suppressed when tab slots exist), a centered colored engine name in BaySickTitleBar bloom style (setCenterTitle, QA-Layout T3). The hamburger's content comes from an installed MenuBuilder (setMenuBuilder - the builder populates AND shows so it can use submenus/checkmarks; pass nullptr to clear); pages install these per show (branch-top clear in showPageForTab). Additional flat headings: setAddMenuBuilder ('Add', QA-Layout T10), setExtraHeadings (arbitrary labeled headings with index+anchor callback, T16 - Builder uses Edit/View), setViewMenu (T21-era reusable view-mode switcher listing mode names with a tick, designed to prevent bespoke per-player switchers, see CL-307). Menu slots: setVisualSlot (T17/T20 - 'Visual' entry opening the effect's visual window; availability is a PRESENCE gate, absent effects show no row), setFxRackSlot, setFreezeSlot (state closure: 0 not frozen / 1 frozen / 2 frozen-stale; getDisabledReason shows a DISABLED row with tooltip because a capability the user cannot see is one they cannot ask for; isVocal appends the whole-chain warning; freeze menu rows are rebuilt per open so refreshFreezeState is a documented no-op). Right side: addExtraRightComponent/removeExtraRightComponent/clearExtraRightComponents (non-owning SafePointers - engine-swap can kill a mounted component before page-hide clears it), setBankIndicator (no-op on same pointer to avoid stack-on-tab-click). Tab slots (owned buttons after the Menu heading): setTabSlots/updateTabActive/clearTabSlots/setTabSlotWidth/setTabSlotTooltip; remaining users post-T15 are the EQ windows' Pre/Post pair, PianoRollPage's jump cluster, and the pedals window's NAM/IR launcher (compact width 30 vs 74). setMidSideSlots serves the EQ M/S pair. Layout in resized(): extra-right components flush right (dead SafePointers skipped without consuming width), then the (always-empty) action buttons, mCenterFreeL/R track the free span for the center title.

### SharedUI widget inventory (Source/Standalone/SharedUI.h/.cpp, 2,134/8,698 lines)
Live: VibeLAF (app-wide LAF incl. scrollbars, document-window title bars/buttons, toggle buttons, group outlines), Filmstrips (filmstrip image accessors), LRXHelper realism kit (drawAO, drawWithBloom, drawFresnelRim, drawAnisotropicHL, drawMountingScrews - used by SharedUI skeuomorphic knob painters and VibePlayerLAF), DynamicsLAF (kKnobVariant property selects drawDualLayerAluminum vs drawModernAnalog; paintLA2APanel used by Compressor/Saturation-family panels), JewelIndicator, BypassLedButton, ChickenHeadSelector (multi-position rotary selector, angle->index; heavily used by BaySickNAMIR editor), ParametricEQDisplay (dual-mode binding: bindAPVTS(apvts,layerIdx) for Layers page vs bindDSP(EQ8DSP*) for Effects page; syncFromAPVTS polled from page timer; heatmap + phase-curve overlays, dynamic-params popout per band, spare-band triggerLock, hamburger integration via the PageMenuBar menu-builder), VUMeter (horizontal + vertical-VU paint modes), LufsReadoutBox (momentary/short-term/integrated modes, mode picked from a right-click menu, persisted; fed from PluginProcessor master LUFS; mounted on the master MixerTrackStrip), ColoredSectionLAF (popup section headers; also called by SlotComponent), VKnobAutomation namespace (global sOnAutomate/sOnRegisterApplicator hooks - the model-side automation registration seam). Dead quartet discovered this audit (never instantiated anywhere): BasicEnvelopeEditor (h:1332), SeqRoutingBar (h:1349), WaveformDisplay (h:1387), BasicSequenceGrid (h:2096) - relics of the old 'Basic sequence' UI generation; their data types (SeqRouting enum, BasicStep struct) live on in PatternManager serialization. TextureUtils (h:224) is a cached procedural-texture generator (brushed aluminum, voronoi, finger grunge keyed into a static map) with no remaining live entry point.

### WorkspaceWindow / Workspace (Source/Standalone/WorkspaceWindow.cpp/.h)
Real native WS_CHILD windows inside the fixed fullscreen frame; a child peer positions and reads back in the parent's client space. Content modes: setContent (owned via mContent unique_ptr; StandaloneEditor.cpp:15057) or setContentNonOwned (raw; 15003) - both park the component behind the chrome via toBack; dtor removes the child only in non-owned mode. Each window has a PageMenuBar (getPageMenu, never null while the window lives) and, since QA-Layout T21, a title strip on effect-visual windows. Placement persistence is two-tier static state: registerPlacementPersistentKey + seedGlobalPlacementKeys (seeded at editor construction ~2487) declare which persist keys exist; replaceSessionBounds swaps the whole session-bounds map on project load (~16200); applySavedBounds applies per-window on attach (Workspace::attach loop 1244). Sizing: setDefaultWindowSize makes the measured 'smallest still readable' size both the opening size and the minimum (Jeff 2026-08-05); applyResizeMagnetism snaps drag-resize edges (called from the resize path at 876); clampPositionToWorkspace bounds moves (726). Fill: toggleWorkspaceFill (button wired at 103) with loadSavedFillState so both normal and filled sizes survive restart (Jeff 2026-08-06); dual fill sizes per the QA-UndoCoverage commit. Tether (T21): mTetherLeader/mTetherFollower SafePointers, isTetherLocked requires a live partner via tetherPartner(); locked pairs move/close/open together; lock state persists with the project. Attach protocol: windows queueForAttach then Workspace::addWindow (647/665); page poll timers and vblank drains are peer-keyed via parentHierarchyChanged on getPeer()!=nullptr.

### MixerPage - Rusty strip family lifecycle (Source/Standalone/MixerPage.cpp, 4,039 lines)
Big Rusty Drums gets a per-kit strip family (J-5): addRustyChannelAtIndex(idx,name) spawns one DrumChannel-type strip per kit sound (called from StandaloneEditor.cpp:9516 in a loop over discovered channels after kit load; channel ids kRustyBase 800+idx onto the always-allocated RustyDrums bus 12); the first strip activates the bus strip's visibility. Teardown is bulk-only: clearAllRustyChannels (called at StandaloneEditor.cpp:5817 tab delete and 9511 pre-respawn) removes every strip, clears mRustyStrips/mRustyOrder, hides the bus strip, invalidates the strip cache (perf-audit H2), and fires onAudioStripRenamed. Strip erasure marks mStripCacheDirty. Aux strips delete per-index via a confirm prompt -> deleteAuxStrip -> removeAuxChannel; Plugin/Bass/Drum channels remove per-index from StandaloneEditor tab-delete paths (removePluginChannel/removeBassChannel/removeDrumChannel). Bus deactivation on last member uses isSecondaryBus + getBusEverRouted + deactivateBusFlagOnly. Live-input strips refresh via refreshLiveInputStrip(channelId) SafePointer callbacks; strip renames flow through setAuxStripName/setVoxStripName wired from StandaloneEditor (~17054). wireMasterCallbacks + syncApvtsFromMixerState run at construction (5F-4a).

### EffectsPage menu + rack preset surface (Source/Standalone/EffectsPage.cpp)
The page's PageMenuBar Menu heading is populated by buildTitleMenu (called from StandaloneEditor.cpp:6324): Save FX Rack Preset (saveRackPreset) and a Load submenu (loadRackPresetMenu enumerating FxRackPresetIO presets). Slot rows wire onBypass -> onSlotBypassToggled and plugin choice -> onPluginChosen(slot,desc); slot removal runs through a prompt whose OK executes performSlotRemoval. Rack automation registration is model-side and static: registerRackAutomationForAllChannels(proc) called once from StandaloneEditor.cpp:17815; preEqForChannelId is the static VibeGraph lookup EffectWindows' EQ window uses to find the pre-EQ DSP for a channel (EffectWindows.cpp:526).

### InstrumentPage - dead legacy base (Source/Standalone/InstrumentPage.h/.cpp)
The pre-contained-window-era design: a base class for Layers/Bass/Drums tabs owning a full-screen engine-selector ComboBox (setupEngineDropdown/buildSelectorPanel), a locked-after-choice engine commit (commitEngine), three sub-tabs (Sound/Piano Roll/EQ via setSubTabContent), an EngineType enum (None/BaySickSolstice/VibePlayer/Legacy808909/BaySickSynth) and a mixer track id. Nothing includes or derives from it today - LayersPage/BassPage derive from juce::Component and own their engine selection directly; the EQ sub-tab concept itself was retired by the J-6 EQ unification (all EQ on the Effects page). Compiled but unreachable.

### LoudnessReportWriter (Source/Standalone/LoudnessReportWriter.cpp)
Export loudness report: write() emits an HTML report (buildHtml, embedding a machine-readable data block via buildDataBlock) and a CSV (buildCsv) from a BuilderPage::MeasureResult + Context; readEmbedded parses the data block back out of a previously written HTML file (consumed at StandaloneEditor.cpp:3666 to reload past measurements). All four methods live.

# Capture from finder: deadfn-engines
### Sfizz engine state save/restore (BaySickGuitars / BaySickBasses / BaySickRustyDrums)
The three sfizz-backed engines (Guitars, Basses, RustyDrums) are PROCESSOR-owned (not EngineRig-owned) and each carries its own APVTS ("BaySickGuitarsState" / "BaySickBassesState" / "BaySickRustyDrumsState" root tags, per-engine UndoManager wiring via apvts.undoOwnerTag). Save side: getStateInformation is live and used two ways - (1) tab serialization encodes it base64 into the Tab record ("sfizzEngineData" attribute for Inst tabs, "engineData" for the Rusty tab), (2) EngineRig::freezeContentStamp hashes the full blob (FNV-1a) so any knob/kit/preset change invalidates freezes; the stamp also explicitly hashes the processor-owned sfizz engines and the kit because the rig's own blob cannot see them, plus per-tab swing params from the MAIN APVTS and the tempo/time-sig maps. Restore side: setStateInformation is NEVER called on these three engines - the J-9 race fix established that it re-runs loadKit without the processing-enabled flip protection, so all three restore paths decode inline instead: (a) project load (StandaloneEditor ~16650+): setSource -> hide strip live-input LEDs -> race-safe loadBaySickGuitarsKit/loadBaySickBassesKit (which create the engine slot, flip processing off, load, flip on) with fallback to the shipped default kit + MissingFileReport on a missing path -> wireEngineDirtyHook -> decode sfizzEngineData blob -> replaceStateKeepingUndoHistory on the engine APVTS; (b) undo resurrection (resurrectTabFromRecord ~12040+): same flow plus ScopedProgrammaticParamWrites around the state apply and a setValueNotifyingHost sweep, plus an immediate reportMissingFilesIfAny drain because resurrection has no downstream drain point; (c) Rusty (12147+/16879): addBaySickRustyDrumsTab, decode blob, resolve the kit path via SampleLibrary::resolvePersistedRef (Rusty persists library-stable refs; Guitars/Basses still persist absolute paths - an asymmetry worth knowing), BaySickRustyDrumsPage::reloadForProjectRestore(kitFile), then replaceStateKeepingUndoHistory. Restore ordering is load-bearing everywhere: kit loads FIRST (kit SFZ stamps set_cc<N> defaults into every brd_cc/prefix-cc param), saved APVTS overlays SECOND so user CC edits win. User-facing: none of this is visible except through project/tab restore correctness and the missing-file dialog listing "Guitar kit"/"Bass kit"/"Drum kit" with the failed path.

### BaySickNAMIRProcessor (NAM amp sim + IR cab, A/B slots)
Stereo-in/stereo-out effect processor hosted three ways: Vox chain sub-processor (owned by BaySickVocalProcessor, created in its ctor so Vox page-preset capture picks it up), Inst chain stage (EngineRig creates Pedals -> NAM/IR fanned by an EngineChainProcessor wrapper; both prepared synchronously at 44100/512 on creation), and FX rack slot. Signal path: input gain -> gate (cached coeffs, NaN-invalidated on prepare) -> low/high cut IIRs -> mono sum -> oversampling (0/2x/4x polyphase IIR half-band, latency reported via setLatencySamples) -> NAM model (nam::DSP, double buffers) -> IR convolution (juce::dsp::Convolution, stereo/trim/normalise) -> Mic Sim + Mic Placement stages A and B (B SUMS into the main buffer when nam_micb_active - two-mics-add semantics, byte-identical single-mic chain when off) -> output. A/B slots: ab_slot param (0/1); on slot flip the processor snapshots the OUTGOING slot's knob/toggle/mic state into a SlotSnapshot and restores the INCOMING one; ab_slot and oversampling stay global. Loading: loadNamModel parses via nam::get_dsp on the message thread, detects full-rig captures from metadata gear_type (amp_cab/amp_pedal_cab -> editor suggests IR bypass), ResetAndPrewarm at the active OS rate if prepared, then hands the model to the audio thread through a lock + spin-bounded swap-pending atomic handshake (mNamPending/mNamSwapPending per slot). loadImpulseResponse loads straight into the Convolution. Failure reporting flows through the bool return + outErr reference param (the mLastNamError/mLastIrError/mLastMicIrError members duplicate outErr but nothing reads them). prepareToPlay sizes scratch for 4x OS, re-preps both IRs and mic stages, re-warms loaded NAM models, and does NOT perform any deferred IR load (the !mPrepared park-the-path branch promises one that never happens). Persistence: getStateInformation stamps nam/ir file paths per slot (A + B suffixed properties) into a COPY of the APVTS state; setStateInformation restores paths then re-loads models/IRs per slot with MissingFileReport::add("NAM model"/"Amp IR") on missing/failed, then fires onStateRestored so the editor refreshes file-name labels. UI: browse buttons for NAM and IR with right-click recent-file menus; mic-sim user IRs have right-click clear; NO clear gesture exists for the NAM model or amp IR themselves. Dirty tracking via ApvtsDirtyTracker wired to ProjectManager::markDirty. Sidechain primitives (setSidechainBuffers/getSidechainLevel) delegate to an ScHelper.

### BaySickRustyDrumsKitGraphic (kit photo hitboxes + calibration mode)
The Rusty Player tab's clickable kit photo. Hitboxes are rotatable ellipses (cx,cy,rx,ry,rotDeg + perspective tiltDeg rendered as ry*cos(tilt)) in a fixed 2000x1200 image space, each labeled with one of 25 articulations; clicks resolve hitTestPiece -> engine audition via the kit-graphic hitbox path; hover shows tooltips (SettableTooltipClient) and tracks mHoverIdx/mPressedIdx for paint feedback. Kit-loaded gate: until setKitLoaded(true), photo + side panels render at 50% opacity with "Pick a program to begin" and clicks no-op. Calibration mode (J-8c) is a dev-only editing layer: move/resize (8 handles)/rotate/tilt/add/delete hitboxes, label them from a dropdown, saveLayoutToFile dumps the table as text that gets baked back into source defaults (resetLayoutToDefaults restores the baked table). The entire calibration layer is currently unreachable - setCalibrationMode has no caller, so mCalibrationMode is permanently false and the paint/mouse edit branches never run.

### BaySickSolstice synth engine setter surface
BaySickSolsticeSynth is a dual-part additive engine: Part A and Part B are simultaneous layers (never alternate modes), crossfaded by timbre_blend, with per-part spectral modules (prism, pluck decay, blur size/time/harm, filter mask, phaser mask, brownian, prism mode, pluck blur). S3.5 (2026-04-19) split every combined setter into per-part A/B variants; BaySickSolsticeProcessor's param plumbing calls ONLY the A/B variants (one caller each). The ten combined setters remain declared "back-compat" but have zero callers. Other live surface confirmed in passing: strum (time/dir/tension applied via applyStrum in the midi path), unison type/alt/phase, part-B shape, phase init, global LFO vol/pitch depth appliers, output phaser + EQ mix, and getAggregatedPartialAmplitudes feeding the central spectrogram at ~30 Hz from the GUI thread (VisualizerScreen::tickFromSynth is called from a header inline - looks dead to naive grep but is live). BaySickSolsticeXYZPad: a 3-param pad whose knobs drive APVTS attachments; QA-ModelShell TS3 made attachToApvts stamp-only (applicators registered model-side against the engine APVTS so they survive pad destruction); its programmatic setXY has no callers.

### Effect DSP parameter flow (rack effects)
Parameters reach effect DSPs through exactly two live channels: (1) DSP/EffectParamMap.cpp - the (EffectType, param-index) table of set/get lambdas the rack + automation use (SET_GET macro rows; PanelContext exists because the same DSP can sit in the rack vs the pedal board with different knob scalings - e.g. dB into setTapeInputGain in the rack vs 0..10 into setFlowers on the board); (2) Standalone/EffectEditorPanels.cpp knob onValueChange lambdas calling setters directly. State persistence is per-DSP getStateInformation/setStateInformation ValueTree/XML blobs where RESTORE WRITES FIELDS DIRECTLY, bypassing setters - so a setter with zero callers is genuinely dead even when its field is persisted. Confirmed live-vs-dead splits: DelayDSP - the modern surface (delay model 0-3 incl. ping-pong, feedback filter type/cutoff/res, lo-fi, mod, diffusion, FB distortion, tone -1..1 = HP/LP morph, type umbrella Echo/VocalDoubler, self-ducking) is wired; of the Legacy API only setFeedback/setWet/setSyncBPM/setSyncNote still have callers, while setHpHz/setLpHz/setPingPong are orphaned (their fields persist and pingPong migrates old saves to delayModel 2). CompressorDSP - C.4 Phase 1 made the strip's SC array (slot.scPick -> mScPick each block) the single sidechain source of truth; process() overwrites useSidechain from getActiveSidechain() every block; the setUseSidechain/setSidechainSourceId legacy/scaffolding pair is dead, sidechainSourceId persists as scSourceId but is never read for behavior. SaturationDSP tape section - input gain wired, output gain setter orphaned (field persists as tape_outputGain and still multiplies in the tape loop at its default). FlangerDSP - dry/wet LEVEL trims (dB fields, applied in process, persisted) have no writers; setCrossLevel is wired. EQ8DSP - 8 fixed bands (kNumBands=8 is a locked decision), band setters incl. per-band channel (12h) and the 12j Dynamic EQ family are wired; setBandKey (piano-key freq), getBandEffectiveGainDbAtRangeLimit (ghost-range endpoint for a Dynamic-EQ widget outline never built), and the Db variant of the magnitude query are unconsumed; the old 'upward' field is a documented preset-stability keep. Several DSPs keep display-only query methods documented as called from the visual window's paint, never processBlock (ChorusDSP.h:67, FlangerDSP.h:67, PhaserDSP.h:76, SaturationDSP.h:207) - that is the T18 self-gated visual-feed convention, live.

### EngineRig (model-owned engine registry) - lifecycle details read this pass
EngineRig owns every dynamic-tab engine keyed (TabKind, pageIndex) across Layers/Bass/Drums/Clips/Vox/Inst/Plugins (+ Rusty as a singleton kind). Engine creation (createEngineFor): Vox = BaySickVocalProcessor (undoOwnerTag = rigTag, its embedded NAM/IR gets rigTag+".namir", prepared 44100/512 at creation); Plugins = Hosting::HostedPluginInstance prepared at srOr44100/512; Inst "Chain" = BaySickPedalsProcessor (rigTag+".pedals") + BaySickNAMIRProcessor (rigTag+".namir") both prepared, fanned by an EngineChainProcessor wrapper - wrapper is the registered engine, both stages land in tab.ownedStages with raw convenience pointers tab.pedals/tab.namIr. registerWithProcessor hands each new engine the transport playhead explicitly because the processor's per-block propagation is gated on the playhead POINTER changing (which happens once, long before late-created engines exist) - for hosted VST3s that push is their entire transport. setEngineType swap path: unregister + destroy with no settle sleep (message-thread + strip task early-outs on null engine cover it), then create; teardown with freeze-file cleanup fires onFreezeFileObsolete. Freeze staleness: freezeContentStamp (FNV-1a over engine blobs + owned stages + processor-owned sfizz engines for Inst/Rusty + swing params + tempo map + TS map) with freezeProcStampSeen change-detection driving markEngineContentChanged; isFreezeStale is the query. tabsOf/allocateFreeIndex are unconsumed utility surface.

### PatternManager areas read
Automation templates: add/remove/getNum/get + setAutomationTemplateUserName (display-name-only rename that deliberately never touches paramId to keep applicator bindings stable) are the live surface; renameAutomationTemplate (paramId retarget) is orphaned. Audio library: entries keyed by index (removeAudioFromLibraryAt) because paths can collide post-schema-change; the by-path remover is orphaned; alias + manual-group operations live. Time-sig machinery: pattern-owned TS (C.5b) - getPatternContentBeats computes per-pattern loop length in pattern-bars; the bar-math surface getEffectiveTimeSigAtBar etc. is live while the getBeatsPerBarAtBar/AtBeat pair is a dead two-function chain. Legacy monolithic-drums residue: mDrumEnabled[46] bit-string serialization round-trips in every save with no behavioral reader, plus the enableDrum/isDrumEnabled/getNumEnabledDrums surface. MAX_DRUM_SOUNDS=46 and kDrumSoundNames (VibesynthConstants.h) are separate, still-referenced symbols.

### EffectRack slot metering
Per-slot input (linear RMS) / output (dBFS) metering runs as paired atomics: audio thread accumulates Run atomics, promoteSlotPeakSnapshots() promotes Run -> Snap at end of each audio block (called from VibeGraph's promoteAllInsert sweep), and the UI drains the snapshots once per vblank via drainSlotInputLevel/drainSlotOutputLevel (exchange-with-sentinel: 0.0 / -96). The 2026-05-02 drain design superseded the plain getters, which remain as dead surface. Multi-slot mutations (move/pack/setStateInformation) take mSlotsLock; per-slot param writes are lock-free.

### VibeGraph instrument/audio-row channels
Mixer-side per-source channels live in mInstrChannelNodes (id-keyed node map with an mInstrChannelOrder display order; audio rows key at 400+row) with EQ per node (getInstrChannelEQ/getAudioRowEQ live; QA-InsertMaps moved insert lookups to a flat InsertNode array and those EQ getters check InsertNode FIRST, falling back to the channel map). Channel creation is graph-internal and fires onInstrChannelListChanged; nothing ever calls removeInstrChannel (asymmetric lifecycle). A separate abandoned 'Phase-2 instrument nodes' design (addInstrumentNode/removeInstrumentNode/hasNode over the embedded AudioProcessorGraph) predates EngineRig ownership and has no callers. Rack plumbing verified live in passing: rebindAllRackHooks, getAuxRack, clearAllRackStates/clearAuxInserts, RoutingGraph::rebuildFromApvts + computeTopo, applyScRecvDelay, getFreezeTapBuffer.

### Hosting tree (verified live in passing)
PluginManager: VST3-only format, scan-folder CRUD with defaults reset, async scan with progress/skipped lists, known-plugin list persisted via loadFromDisk at construction, added-list add/remove/query (query orphaned). SandboxedPluginClient: spawns BaySickPluginHost64/32.exe (helperExecutable picks arch), talks over a MemoryBlock protocol; handleMessageFromWorker is a vendored-JUCE ChildProcessCoordinator override (framework-called - do not flag). HostedPluginInstance/HostedPluginEditor: bridged editor attach/detach/ownerDestroyed/remoteEditorSized handshake all internally wired; setStateInformation on the inner plugin is live (HostedPlugin.cpp:553, HostedPluginEffect.cpp:171, PluginHostMain.cpp:452). CMake: the helper targets compile ONLY Source/Hosting/Helper/PluginHostMain.cpp (+JUCE), so helper-exe liveness cannot rescue anything outside that file.

### Methodology note for the verifier
Primary sweep: extracted 2205 out-of-class method definitions (Class::method at line start) across the slice, counted every identifier's total occurrences across all Source/*.cpp+*.h in one pass, and screened the 346 methods whose name occurs <=3 times tree-wide; every count-2 name (decl+def only) is mechanically zero-caller, every count-3 name had its third occurrence read and classified (internal call = live, comment/decl = dead). Known traps hit and handled: calls from header inlines and member initializers (computeRenderWorkerCount, tickFromSynth - both live), vendored-JUCE virtual overrides (handleMessageFromWorker - live), name collisions across classes (setBrownianAmount). Residual risk: a zero-caller method whose NAME also appears >3 times in unrelated comments/classes would evade the screen; the targeted self-declared-dead grep (unused/no longer called/vestigial/obsolete) partially covers that gap.

# Capture from finder: dead-params
### Main-APVTS parameter surface (PluginProcessor)

The app has ONE main APVTS (`VibeSynthProcessor::apvts`, tree name "BaySickDAWState", constructed with `&mUndoManager`) whose static layout (`createParameterLayout`, PluginProcessor.cpp:276-358) is deliberately tiny - 7 globals: `masterGain` (0-1, default 0.8, multiplied into the master chain and into each strip task's clip context), `master_fx_bypass` (kill-all: OR-ed into every strip/bus rack bypass each block), `master_pan_law` (0=Circular -3dB / 1=Triangular -6dB / 2=Square, FL parity, read per block by every node applying `_pan`; user sets it via a Settings menu in StandaloneEditor), `Unified_RecordQuantizeDiv` (0-10 on the shared 11-label snap scheme, Record-button dropdown, applied at MIDI record commit), `Unified_BuilderSnapDiv` and `Unified_PianoRollSnapDiv` (live snap for Builder grid / every piano roll, default 1 = Line), `Unified_QuantizeDiv` (Tools > Quantize resolution 0-3 = 1/4..1/32, shared across rolls + drum kit). Everything else in the main APVTS is registered LAZILY via `apvts.createAndAddParameter` - JUCE has no remove API, so params persist as harmless zombies once created (an explicit, repeatedly documented convention).

**Per-mixer-strip lazy families** (`addParamsForMixerStrip` + `ensureMixerStripParams`, PluginProcessor.cpp:7228-7408): every strip prefix gets `_level` (-60..+5.6 dB - the +5.6 cap deliberately matches the fader's drawn dB column), `_pan`, `_width` (M/S 0..2); Bus/Insert kinds add `_mute`/`_solo`/`_polarity`; Master adds only `_mute` (solo/polarity deliberately omitted - nothing to solo against, polarity inversion at master reads as 'broken'); all kinds get `_bypass` (per-strip rack FX bypass); buses only get `_collapsed` (UI collapse state, persisted, no audio reader); `_arm` ONLY on `mixer_vox_`/`mixer_inst_` inserts (record-arm; Batch E #6 removed it from other inserts as zombie preset bloat); routing params on every strip (`_sendTo` main-out with natural-parent default, 4 x `_send{s}_to/_amount/_prepost`); 4 x `_sc_recv{s}_from` sidechain receive lines (source channel id, -1 empty; DSP modules pick a line via the rack slot's scPick field - the '_sc_pick' spelling in comments is a field, not a param); `_chokeGroup` (inserts only, 0-16); `_playNote` (drum inserts only - the MIDI note the drum sounds at, default 60). Every strip ALSO gets two full 8-band M/S EQ banks (`addParamsForEQBank`): post-rack at `{prefix}_{mid|side}_eq{b}{Suffix}` and pre-rack at `{prefix}_preeq_{mid|side}_eq{b}{Suffix}` - NOTE the suffixes are CAPITALIZED with no separator (`...eq0Freq`, `Gain`, `Q`, `Type`, `On`, `Slope`, `Mute`, `Solo`, `Channel`, `Dynamic`, `Threshold`, `Ratio`, `Attack`, `Release`, `Range`, `Upward`, `ScSource`) - 19 params per band, synced to EQ8DSP in `updateAllPreRackEQsFromApvts`-family code around PluginProcessor.cpp:4900. The pre-QA skill-doc spelling `drums_mid_eq{b}_freq` is the DELETED legacy scheme (Â§P4.3 B7). Bus registration is eager at startup (`ensureMixerBusAndMasterParams`: master + layers/bass/drums/fx/clipsbus/voxbus/instbus/voxbus2/instbus2/instbus3/rustybus/pluginbus/layersbus2/bassbus2/clipsbus2/pluginbus2 - every bus always registered so `rebuildRoutingFromApvts` finds its `_sendTo` edge; pluginbus was the TS6 lesson). Insert registration happens at strip creation (`ensureAuxInsert`/`ensureVoxInsert`/`ensureInstInsert`/`ensureRustyInsert`), which also creates the VibeGraph InsertNode and (aux/rusty) the render task; vox/inst also get the live-input family (`addLiveInputParams`): `_inputChannelIdx` (-1..127 ASIO channel), `_listen` (audible monitor toggle - strip still processes and records when off, silenced at strip exit to prevent speaker feedback), `_inputChannelStereo` (stereo-pair capture), `_monitorMode` (vox 0-2: True Dry / Bypass Corrector / With Effect default; inst 0-1: Dry / With Effect). Channel NAME is a non-APVTS `apvts.state` property (`{prefix}_inputChannelName`) under a CriticalSection.

**Swing family** (`ensureSwingParams`, QA-G3Smoke SW-6): `globalSwing` + per-page `swing_{layer|bass|drum|inst|plugin}_{N}_mix/_trunc` + `swing_rusty_mix/_trunc`, eagerly registered at startup with raw atomic pointers cached in mSwing* members read per block by the note scheduler (PluginProcessor.cpp:2689-2876). Vox has NO swing params (no vox MIDI - Jeff ruling) and clips follow the global knob (no per-page params). `makeSwingKnobBinding` hands UI get/set closures over these ids. The swing values are also folded into `EngineRig::freezeContentStamp` so a swing change invalidates freezes.

**On strip teardown** the pattern is: reset params to defaults programmatically (under `ScopedProgrammaticParamWrites` so undo history is not polluted) since the params can't be unregistered - see `removeRustyInsert` (level/pan/width/mute/solo/polarity/bypass) and `resetBaySickRustyDrumsMixerState` (program-swap reset of all 14 rusty prefixes; its EQ sub-block is currently a no-op due to a suffix-case mismatch - reported as a finding).

### VibeGraph node param binding

`InsertNode` (VibeGraph.cpp:~300-498) and the folded single bus/master node type `InstrChannelNode` (CL-301: all 11 buses share one implementation after the 5 hand-written bus structs caused divergence incidents) cache raw `std::atomic<float>*` per param at `rebindApvts` (message thread, after ensureMixerStripParams) and read them lock-free per block. Insert chain order: pre-rack EQ -> polarity -> M/S width -> rack (bypass = strip OR global) -> post-rack EQ -> fader x mute x solo (gain-ramped from the previous block's gain to prevent zipper) -> pan (project pan law) -> SC tap stash -> PDC comp delay -> peak/RMS publish. Bus chain differs: fader/mute/solo BEFORE polarity/width. Master chain (`processMasterChain`): pre-EQ -> rack -> post-EQ -> masterGain x fader x mute -> pan -> width (pan BEFORE width, kept verbatim from the old MasterBusNode) -> EBU R128 LUFS -> gated master spectrum + true-peak feed (seqlock, one relaxed load when the analyzer window is closed) -> peak publish. Nodes also carry the TS7 'Source Only' freeze tap (pre-chain copy + sequence counter so gaps render as silence, not repeats) and the QA-Fe2 SC delay-match tap. Routing resolution: `rebuildFromApvts` (block-rate) loads `_sendTo`/`_send{s}_*`/`_sc_recv{s}_from` per strip into the RoutingGraph with Kahn topo sort + cycle drop.

### Per-engine APVTS instances

Every engine processor owns its OWN APVTS with per-instance-prefixed param ids so multiple tabs coexist; ALL verified 1:1 created-vs-read (no zero-reader params): **BaySickSynth / BaySickBass** (52 ids each - `{prefix}outVol/waveform/transpose/modifier/dualOscMode/oscSync/ringMod/drift/unison_*/noise/noiseOnly/noiseColor/voiceMode/glide/cutSelf/cutSelfMode/modWheel*/amp_*/pEnv_*/trans_*/burst_*/flt_*/lfo_*/velAmpTrack`; read via `updateFromApvts`'s getf/geti over `mPrefix + name`, prefixes like `tk_bas_0_bsb_`, `tk_drm_0_s5_bss_`). **VibePlayer** (32 ids: volume/pan/tune/attack..release/cutoff/res/drive/reduct/lfoAmt/lfo_rate/muffle/hardness/velTo*/stretch/stereo/treble/sampleStart/reverse/detune(+Mode)/unison*/artic_group/voiceCap/cutSelf(+Mode)/sensitivity). **BaySickSolstice** (93 ids incl. partB_* mirrors, rm_* routing matrix, ophaser_*, trem/vib, unison family, strum, pitch, filters; `part_sel` is editor-only by design). **BaySickVocal** - THREE prefix families in one APVTS: `bsv_` (mix, ab_slot, realtime pitch-corrector board, 6-slot vocal chain: gate/dereverb/deesser/comp/sat/limiter with full per-module param sets), `bsa_` (BaySickAlign: align_on/mode/maxShift/fineTune, pitch_on/transpose/range/variation/algo/typeGuide/typeDub, formant_*, preset(+dirty), leader/follower_channel), `bsp_` (BaySickPitch editor: focus/mod/speed/throat/root/scale/snap/mode/on/engine). **Sfizz trio** (Guitars `bgg_<idx>_`, Basses `bbb_<idx>_`, RustyDrums singleton `brd_`): `outVol` + `cc0..cc511` (one Int per possible ARIA-surface CC, default 0 NOT 64 - the QA-Sfizz Sub-E blanket-64 default was reverted because it half-enabled amount controls; kit `set_cc` directives layer real defaults on top) + cutSelf/cutSelfMode (Guitars/Basses); CC params drive sfizz hdcc via parameterChanged listeners + cached mCcRaw pointers. **BaySickNAMIR** (26 ids: gain stages, gate, nam/cab bypass, low/high cut, cab_mix, oversampling, ab_slot, dual mic-sim + placement A/B families; A/B snapshots capture/apply the whole set programmatically - undoing ab_slot re-fires the handler, which is self-healing). **BaySickPedals** (only `pedal_slot{N}_bypass` Bools; per-pedal DSP params are NOT APVTS - they go through EffectParamMap like rack slots; slot policy locks slot 0 to Tuner and slot 7 to the 3 EQ styles).

### Automation lane registration (model-side, QA-ModelShell TS1/TS3)

Two editor-owned maps - `mAutomationApplicators[pid]` and `mAutomationValueReaders[pid]` - are the single dispatch surface for automation clip playback. Registration entry points: (1) `registerStaticAutomationHandlers` walks EVERY current main-APVTS param and registers a direct setValueNotifyingHost applicator, plus the `"{mixer prefix}_fader"` alias for each `mixer_*_level` (the lane id predates the param; saved lanes keep their spelling) and the special `global_tempo` lane (0-1 <-> 20-300 BPM, drives `mPlayHead.setLiveTempo` as a LIVE override - deliberately does NOT write the persisted base tempo). It is re-run at every project boundary AND from `onMixerStripParamsCreated` (fired inside `ensureMixerStripParams`, so lazily-created strip params get lanes at materialization, not at view creation). (2) `registerModelEngineAutomation` (rig `onEngineCreated`): per-tab lanes with prefixes - Layers/Bass/Drums/Clips use the engine's bare globally-unique ids; Vox uses `vox<N>_` + id (covers both the vocal engine and its embedded NAM); Inst uses `inst<N>_` + NAM ids plus `registerPedalAutomation` (re-fired by `onSlotAutomationChanged` on pedal swap); every applicator RE-RESOLVES its target through the rig at apply time (never a captured pointer) - BaySickSolstice additionally gets non-APVTS mod-editor lanes (`<targetParamId>_mod<N>_depth|length` writing BaySickSolsticeModRegistry fields). Vocal applicators carry a capture-gate veto (writes to the realtime board are dropped while the strip records; the lane is not consumed - next tick applies). (3) `registerSfizzEngineAutomation` (processor event `onSfizzEngineReady`) - same walk over the sfizz APVTS, lane id == param id, re-resolving through the processor because kit loads destroy/recreate engines. (4) `registerPluginTabAutomation` (hosted VST3 instrument tabs, `plugtab<N>_vst_<paramId>` lanes, re-fired by onParamListChanged). MIDI-learn (`MidiLearnRegistry`) is a parallel per-paramId->hardware-event map (CC/pitch-bend/channel-pressure, per-channel + per-device, Omni widening), dispatched at block rate via try-lock, persisted per-project as `<MidiCCMappings>` plus a global-defaults file `Documents/BaySickDAW/MidiMappings.xml`; learn capture is a lock-free flag handshake (no audio-thread allocation, same shape as DrumTriggerMap). Its `formula` field is persisted-but-unevaluated v1 scaffolding (linear only).

### Offline lane replay (export/freeze parity)

`BuilderPage::applyOfflineLaneValue` (BuilderPage.cpp:9820-10077) is the offline mirror of the live applicator map - every lane class MUST have a branch here or it plays live and silently vanishes from exports (the standing rule). Branch taxonomy in order: main-APVTS ids replay inside processBlock (skipped upstream at 9807); `plugtab<N>_vst_*` -> HostedPluginInstance::applyParamNorm; `vox<N>_`/`inst<N>_` page lanes (digits immediately after the word; vocal falls back engine-then-NAM); `inst<N>_pedals_<slotUuid>_<suffix>` -> EffectParamMap with PanelContext::Pedal (slot by UUID, never index); bare engine ids via a rig sweep; sfizz ids via forEachSfizzApvts; BaySickSolstice `_mod<N>_depth|length` registry writes + publishSnapshot; legacy `_fader` -> `_level`; finally rack lanes `<channelPrefix>_<slotUuid>_<suffix>` resolved over the full channel-id vocabulary (buses 1-12, drums 100+, layers 200+, basses 300+, audio 400+, aux 600+, vox 700+, inst 800+, rusty 900+, plugin strips 1000+), where `output_vol` is the one rack-level non-DSP control and `vst_*` forks to hosted plugin effects.

### EffectParamMap (rack/pedal DSP automation table)

`Source/DSP/EffectParamMap.{h,cpp}` is THE single home for rack-effect knob->DSP math, keyed `(EffectType, variant)` where variant comes from the DSP's own character mode (Compressor Modern/FET/Opto/CS etc.) plus `PanelContext` (Rack vs Pedal, ordinal kPedalVariant=100) because 7 types have pedal-face twins reusing knob labels at different ranges/setters (Saturation 'Drive' is -24..+24 dB tape input gain in the rack vs 0..10 'Flowers' on the board). Suffixes are derived from knob LABELS (lowercase, space/slash->underscore, stamped as `<channelPrefix>_<slotUuid>_<suffix>` componentIDs in `EditorPanelBase::setSlotContext`); toggles carry explicit suffixes via addAutomatableToggle. Each ParamDef holds natural lo/hi, apply and read fns, optional dynamic rangeOf (Phaser Rate's Slow/Fast bound), and an affectsLatency flag that triggers a bus PDC refresh on automation ticks (panels do it via onLatencyChanged). Audit result: all 141 table suffixes correspond to live panel controls (140 knob labels + the 'freeze' toggle); no orphaned entries found. Panels self-sync FROM the DSP on a 10 Hz DisplaySync timer using the same suffix strings.

# Capture from finder: dead-ui
### Application command system (KeyBindings / KeyBindsWindow / StandaloneEditor)
BSCommands (Source/Standalone/KeyBindings.h/.cpp) is the central catalog of every editable keyboard shortcut: 31 stable int command ids (0x10001-0x10071) each with a Category (General / Builder / PianoRoll / DrumKit / MouseReference / VocalEditors / EventEditor), display name, beginner tooltip, and default KeyPress. StandaloneEditor is the single ApplicationCommandTarget: getAllCommands() registers straight from the catalog vector, and perform() is one switch with a case for every id (StandaloneEditor.cpp 9123-9268) delegating to transport methods (GlobalTransportBar::togglePlayPause etc.), page switches (handleCommandMessage), file ops (doFileNew/Open/Save/SaveAs/doExportAudio), pattern nav, undo/redo (globalUndo/globalRedo), the Builder slip/stretch toggle, and typing-keyboard MIDI. User-facing: Help > Key Binds... opens the editor window; user rebinds persist via saveMappings/loadMappings to keymap.xml next to audio_settings.xml in Documents/BaySickDAW. MouseRefRow entries are display-only reference rows (mouse gestures + page-local hardcoded keys, not rebindable); findHardcodedConflicts() warns during capture when a new binding collides with a page-local key that fires before the command system. Categories VocalEditors and EventEditor are entirely reference rows.

### Main menu bar (StandaloneEditor MenuBarModel)
Six menus - File, Edit, Patterns, View, Options, Help (getMenuBarNames 10986). Built in getMenuForIndex (10991-11186), dispatched in the single menuItemSelected (11188-11354). File: New (101), New-from-Template submenu (dynamic walk of factory + user template dirs, ids from kTemplateMenuLoadBase resolving through mTemplateMenuFiles rebuilt on every open; 111 = New from Default Template with missing/unset disabled states), Open (103), Quick Open (110), Open Recent (130-139 + 140 Clear, dirty-prompt + full state wipe before load), Save/SaveAs/Save-as-Template (104/105/106), Restore from Backup (108), Import Audio (107 -> mBuilderPage->doImportAudio), Export Audio (120, one dialog after QA-Export retired the dead 120/121 WAV/MP3 submenu), Export Project Bundle (122). Edit: undo/redo/history (201-203), New Layers/Bass tab (204/205; 206 Drums disabled - permanent), New Automation Clip (207). Patterns menu (301-307): built but has NO dispatch - see findings. View: 401-407 switch pages via handleCommandMessage / ribbon walk / showLastUsedPianoRoll. Options: default-template submenu (530/531), File Settings (502), Audio Settings dialog (503, AudioSettingsDialog safe-apply), Plugins manager (504), Undo History Size radio (510-513 -> setMaxNumberOfStoredUnits(1, N) per the honest-count ruling + rebuildHistoryLabels), read-only MIDI-omni row (520 disabled). Help: Help Index 601 (dead - finding), Key Binds (603), Rusty Drums Map (604), About (602 - discloses sfizz BSD + LAME LGPL; list known-incomplete pending QA-LegalReview).

### EventEditor (automation clip editor)
EventEditorContent owns a local MenuBarModel with File/Edit/Tools/View/Target Control/Import MIDI menus (1664-1729). Live paths: Close (101), Undo/Redo (200/201 - unified onto the app history via UndoContext performViaCtx/doUndo/doRedo; editor-local Ctrl+Y retired at QA-UndoCoverage docket 14=a), Select All (202), Erase to Range Start (205, AutomationLaneEditAction), Convert to Clip Douglas-Peucker (206), tool selectors (300-305 mirroring P/B/D/I/E/Z keys), Snap submenu (400-405 -> setSnapSub), LFO-mode toggle (410), Import MIDI CC (600/Ctrl+M). Dead surface: 100, 102, 203, 204, 411, 500-504 (see findings). Toolbar has a working New-Automation button (doNewAutomation: enumerates onGetParamList, creates via onCreateAutomation, ids i+1 dispatched arithmetically). Window close copies onClosed to a local before teardown (2134-2136) then notifies StandaloneEditor (assigned 3772) - live lifecycle pattern.

### Builder page callback fabric (BrowserPanel / ArrangementGrid / BuilderPage / StandaloneEditor)
BrowserPanel (browser sidebar; collapsed = width zero, 5px chevron grip only - Jeff 2026-08-06) exposes hooks wired by BuilderPage/StandaloneEditor: onPatternSelected, onDropTypeChanged (last-clicked entry arms the grid's drop type), onRenderPattern/onSplitPattern (pattern right-click), onArrangementChanged, onClosePageForChannelId (deleting a page's last library entry closes its ribbon tab - wired despite a stale 'Unwired' comment), onResolveDisplayName (automation lane labels), onEnumerateAudio (page-walk for the unified Audio tree). ArrangementGrid exposes onToolChanged, onUndoRedoStateChanged, onSeek, onRowHeightChanged, onOpenEventEditor, onAudioClipAdded, onArrangementChanged (block edits -> rebuildAudioClipPlayers), onTempoMapChanged, onEnumerateRoutablePages/onCreateRoutablePage ('Routes to:' dropdown), onDuplicateFileForCopy/onTagCopiedEntry (copy-fork), onRenderFileDropped (Exports drag), onApplyLibraryProperties (linked Move), onImportSampleRequest/onResolveStoredPath (P4 project-relative paths), onGetBPM/onGetSampleRate (slip math), onGetSnapDiv/onSnapDivChanged (Unified_BuilderSnapDiv APVTS). Menus verified fully dispatched: browser sort (1-3), category Create Group (r!=1 guard), reports context (Open in Analyzer/Add to Project/Reveal), pattern context (Rename 1 / Render 2 / Delete 3 / Duplicate 4 / Revert-auto-name 5 / Color 6 / Split 7 + choke submenu 200-216 decoded via kIdChokeBase), ruler menu (edit/delete marker-TS-tempo 1-6, add 10/11/12), block menu (Cut/Copy/Paste/Delete/Mute/Properties/Reset-Stretch/Open-in-Event-Editor 1-8), move/copy target submenu (2t+1/2t+2 arithmetic decode), track header (1-9 incl. Insert Track Above case 8 far below the show call), Slip/Stretch dropdown, per-pattern render scope (Per Track/Full Mix/Select Tracks). Undo integration: performLibraryOp / performPatternSliceOp / performMarkerSetOp / performPatternTsOp wrap ops into the shared UndoContext; pattern-slice resurrection rides onCapturePatternSlice/onApplyPatternSlice (both wired).

### SlotComponent + vocal chain
SlotComponent is the per-slot effect frame: bypass LED, effect title, Preset button (Save/Load/Restore/Default/Manage), Mode dropdown (Compressor Modern/FET/Opto, Saturation Tube/Console/Tape, Limiter Limiter/Maximizer - onModeChanged), Sidechain source dropdown (enumerates the strip's 4 _sc_recvN_from APVTS lines, 'Off'=1, picks 10+s, disabled 99 info row when none routed), Basic/Advanced toggle (flag owned by EffectRack slot, persisted; re-applies panel layout in place). Presentation::PanelOnly suppresses header hit regions. Its only live instantiation is BaySickVocalEditor::VocalChainPanel: 6 locked pre-loaded slots (Gate / De-reverb / De-esser / Compressor / Saturation / Limiter) bound to mVocalChainRack, editors mounted + rebound to bsv_ APVTS prefix on construction and again on onChainStateRestored. The unlocked-rack affordances (add-effect click, up/down/close) are unreachable in this configuration and their hooks unassigned (findings). The static showEffectPickerMenu (grouped rack-effect picker, pedals one submenu deep per QA-ModelShell TS5; slots load by EffectType so saved projects never consult the menu) and effectTypeName are shared by EffectsPage and the pedals editor.

### SharedUI inventory (live vs legacy)
Live: VKnob (drag knob with soft lockout overlay preserving tooltips, MIDI-learn outline, onDragEnded undo hook), PageMenuBar convention (pages install menus into the page hamburger), ParametricEQDisplay (8-band EQ display: band drag editing, A/B spare banks with lock + Copy-A-to-B + bank indicator injected into PageMenuBar, heatmap + phase overlays, anti-cramping 2x OS toggle, Processing Mode radio 10-14 with live latency readouts, Linear Phase Precision 20-24, IIR Mod Speed 25-29, Proportional Q 30, per-band sidechain source popup, M/S view pill, APVTS/DSP/MsDSP bind modes, onLatencyChanged PDC refresh - options menu fully dispatched), LufsReadoutBox (Momentary/Short-Term/Integrated picker, arithmetic dispatch), the QA-RustyMeter split peak/RMS meter (vblank-driven, peer-keyed). Legacy dead cluster (findings): BasicStepCell/BasicSequenceGrid (step grid), BasicEnvelopeEditor (AHDSR), SeqRoutingBar (Basic/Complex dropdown + Go), FXChainStrip (6-slot toggle+3-knob strip), WaveformDisplay (start/end markers + body-drag LFO rate) - the old basic-sequence editing UI; their model types (BasicStep, BasicEnvelope, SeqRouting) remain live in PatternManager persistence.

### GlobalTransportBar record path
Record button chevron opens the record dropdown: ASIO (1) vs MIDI-into-piano-roll (2) modes calling setRecordMode (internal state; consumers poll getRecordMode - StandaloneEditor.cpp:1015), plus Global Record-Quantize submenu (ids 100-110 over the shared kUnifiedSnapLabels 11-division scheme; state owned by APVTS Unified_RecordQuantizeDiv in StandaloneEditor, accessed via onGetRecordQuantizeDiv/onRecordQuantizeDivChanged - both wired). Transport actions (togglePlayPause/stopAndDisarm/toggleRecord/toggleMetronome/toggleSongMode) are public methods invoked by the command system, not key handlers. onPlay/onPause/onStop/onRecord/onMetronomeToggle/onTempoChanged wired by StandaloneEditor.

### RibbonTabBar dropdowns
Add-tab menu uses pushChoice() dynamic id registration for engine choices (BaySickSolstice/BaySickSynth/BaySickPlayer/BaySickBass per tab kind, hosted-VST3 instruments via createIdentifierString), fixed ids 1/2/3 for the sfizz trio (Guitars/Basses/RustyDrums with cap/liveness gating) - all dispatched (634-636). Per-tab dropdown: sub-page selectors for Effects (Rack/Pre EQ/Post EQ), Builder (Patterns/Audio Clips/Automation), Drums (Sound/Piano Roll/EQ) dispatch as result-1 through onSubPageSelected; instance menu handles Rename (-1), Delete (-2 with confirm), Add-BaySickPlayer (-3), Add-Rusty (-4), Add-Guitars (-5), Add-Basses (-6), engine adds (kAddEngineBaseId+), Vox Exports rows (kVoxExportBaseId+), page rows (-10..-98); -99 rows are disabled headers.

### Piano Roll / Drum Kit menu bars and lanes
PianoRollMenuBar: Edit (1-6), Tools (60-68 tools, Chop submenu 70-74 arithmetic, Quantize Settings 110-113), Scale (28 snap toggle, roots 201-212, scales 301+), Chords (401+), View (51-57 zoom/scroll/ghosts/lane) - every id dispatched, arithmetic ranges resolved. DrumKitMenuBar is the drums-parity subset (no arpeggiate/chords/transpose per SC15) - fully dispatched. Both ControlLanes (velocity/pan/pitch-bend/filter-cutoff on piano roll; velocity/pan on drums) switch display mode via a header-click popup with range-checked dispatch. Both snap 'magnet' buttons are instances of right-click-hook button subclasses whose hook is never assigned (finding - right-click currently swallowed). PianoRollPage's engine dropdown (Drum Kit + dropdownEnumerator entries, ids 1 and 100+) is built by the page but dispatched by StandaloneEditor (6157-6180).

### Master meter context menu (StandaloneEditor ~7150-7420)
Pan Law radio (101-103 -> master_pan_law with undo gesture), Master Output submenu (300+i stereo pairs / 400+i mono onto MasterOutputRouting atomics + settings persistence), Latency-compensate meters toggle (201, recomputes from device output latency), Multi-core Rendering hot-swap (202, release-store paired with worker acquire-loads, persisted to settings.xml synchronously), MT Diagnostic 2s capture (203, message-thread sleep + AlertWindow report). Fully dispatched.

### Mixer menus
Vox bus delete (single-item confirm flow), input channel picker (enumerates device channels via getInputChannelNames, grid-default picks 300-303 via onSetGridDefault, Disarm 99, channel picks arithmetic), MixerTrackStrip Listen-LED right-click monitor-mode selectors (Vox three-mode True Dry / Bypass Pitch Corrector / With Effect; Inst two-mode Dry / With Effect - both write _monitorMode APVTS with undo gestures). All dispatched.

### Hosting client callback marshaling
SandboxedPluginClient's reader thread receives helper-process events and marshals to the message thread by capturing COPIES of the std::function members ([cb = onLoadResult] ... cb()) - onLoadResult (ok/error/isInstrument/acceptsMidi), onEditorSize, onParameterList; all assigned by HostedPlugin. This capture pattern defeats naive invocation greps; verified live.

### EngineRig freeze callbacks
onFreezeStateChanged / onFreezeFileObsolete are assigned by StandaloneEditor (4799/4805, multi-line assignments) and fired from EngineRig transitions plus several PluginProcessor freeze/stale paths - live spine of the freeze-badge UI.

# Capture from finder: dead-branches
### Build targets and dead-macro landscape
`do_build.bat` builds exactly three things: `BaySickDAWStandalone` (Release + Debug), `BaySickPluginHost` x64 (into `build\`), and `BaySickPluginHost` x86 (own tree `build32\`, configured from `Source\Hosting\Helper`). The legacy `BaySickDAW` plugin target (`VIBESYNTH_VST=1`) exists in CMake but is never built by the gate script - its three `#ifdef VIBESYNTH_VST` blocks in PluginProcessor.cpp (editor include :46, createEditor :6501, createPluginFilter :6514) are per-target code, documented as a deliberate keep in CLAUDE.md. Custom macros in Source/ are limited to `VIBESYNTH_VST` and the CMake-conditional `BAYSICK_HAS_*` family (NAM_CORE, SFIZZ, LUNASVG, WORLD, RUBBERBAND, LAME, SIGNALSMITH), each defined when its vendored lib is present; everything else is `JUCE_DEBUG`/`JUCE_WINDOWS`. There are ZERO `#if 0` blocks in Source/. Structural scans also came back clean: no `if(false)`/`if(true)`/`&&false`/`||true` constant conditions anywhere, and a heuristic awk scan over all 163 .cpp files found no statement following an unconditional `return;`/`continue;`/`break;` at reachable indentation (limitation: single-line returns only; multi-line return expressions not covered).

### Meter drain chain (audio -> UI)
Every bus meter follows one chain: BusNode publishes peak atomics on the audio thread (`publishPeakReading`), VibeGraph exchange-stores them into its member atomics per block, and `VibeSynthProcessor::drainMeterAtomicsForUI()` (PluginProcessor.cpp:4664+) merges them into processor-side mirror atomics with a max-merge CAS loop, all back-to-back so a UI vblank sees a coherent snapshot. Twelve buses x three atomics (mono/L/R), plus per-kind insert mirrors (8 InsertKinds; Audio kind drains into legacy-named mAudioRowPeakDb arrays for Builder-grid compat), plus rack-slot snapshot promotion for effect-panel meters. UI readers: MixerPage::timer drains L/R pairs via `drainStereoBus` into strip meters; MasterAnalyzerWindow reads master L/R. THE MONO LAYER OF THE BUS CHAIN HAS NO READER - user-visible meters are all stereo. (Historical note baked into a comment at cpp:4716: the Plugins-bus drain was missed entirely until TS7 2026-07-31; Jeff spotted the dBFS meter flatlining while the waveform meter worked.)

### Offline export / stems sink machinery (BuilderPage ~9200-9330)
`renderToFile` builds a `FileSink` list: sinks[0] = main mix only when `opts.writeMainFile` (deliberately not opened for Per-Track stem exports so no file-existence window), then one sink per ticked stem (`"<base> - <stripName>.<ext>"` beside the main file in `<project>\Exports\`). Per-block callback inside `runOfflineLoop` (the single render core shared by export/measure/freeze): main file is either the master buffer or a summed post-chain tap mix over `opts.mixTapChannels` (mixScratch allocated once, not per block); stems copy each strip's arena slot (`getStripOutputForTap` = the strip's render-task post-chain output for that block, valid only between processBlock calls while device processing is suspended). Sends are separate stems; sidechain-driven content stays in its stem. Close-then-delete on failure (Windows: open handle blocks delete). User-facing: Export dialog (ExportAudioDialog in StandaloneEditor.cpp ~13200) with format/quality/srate/dither combos, normalize toggle with typed LUFS target, stems toggle + per-strip checklist, Measure button (runs the render with meters only, writes NO files per Jeff's 2026-07-31 ruling, shows result in-dialog and forwards it to the analyzer window via mOnMeasurementReady), progress bar with cancel.

### BaySickSolstice editor attachment architecture
Two binding families: (1) plain per-param attachments created in the ctor (~60 of them, BaySickSolsticeEditor.cpp:376-506); (2) the S3.5 dual-bind family - per-part-able controls (blur_size/blur_time/blur_harm/prism_amt/prism_mode/brownian/pluck_decay/pluck_blur/phaser_mask_rate + partB_* twins) are NOT attached in the ctor; `rebindToPart()` destroys/recreates the live attachment against whichever part is selected, and the header's unique_ptr slots for those params are INTENTIONALLY left null forever (documented at cpp:382-385, 418, 425-426, 430-431, 438). Those null slots look dead to a naive sweep but are documented design. mod_x/y/z attach through BaySickSolsticeXYZPad::attachToApvts; mod routing destinations live per-target in BaySickSolsticeModRegistry (processor-owned), which the mod editor pulls directly. BaySickSolsticeRoutingMatrix: 6 rotary knobs (SUB/PROT/CLIP/FX/VOL/ENV, 16px packed row per Jeff 2026-08-04), attached via attachToApvts with componentID stamped for the Automate menu (applicator registered model-side, outlives the matrix); the design doc's 6 LED toggles were ruled out by Jeff 2026-04-19 and never added.

### ReverbDSP structure
One class, algorithm-dispatched (cpp:511): Hall = the main 8-line FDN (default fall-through); Plate (allpass chain + 4 combs); Chamber (H-9 stage D: 4 nested-allpass outer blocks, Gardner/Bricasti-style, per-channel buffers); Room (stage E: 15-tap early-reflection cloud with prime-spaced tap times + 4-comb Schroeder tail); VocalBooth (stage F: tiny 4-line FDN, 7-15ms prime delays, heavy HF damp, NO tail modulation - vocal-safe for pitch correction, Hadamard mixing). Each `processX` returns false to fall through to Hall if its buffers aren't ready. All per-algorithm coefficients (allpass coefs, comb feedback from decay, damping alphas, FDN feed gains) are computed as locals per block from the user params (decay/diffusion/damp) - the header's default-initialized coefficient members are vestigial.

### ArrangementGrid drag state (BuilderPage)
Three drag machines, each snapshotting origin state at mouseDown: slip-edit (audio clip content slip via edge-drag; anchors to origin-mouse-BAR so viewport auto-scroll composes correctly - the QA-Ea Task 0c comments explain the beats-precision + drift rationale), automation point/curve-handle drag (undo via beginEdit full-snapshot, labels \"Move Automation Point\"/\"Adjust Curve\"/\"Add Automation Point\"), and OS file-drag (fileDragMove drives a ghost block preview - mGhostBlock at 50% alpha, snapped to bar, 4-bar default length; accepts wav/mp3/aiff/flac/ogg/aif). Undo throughout is snapshot-based (beginEdit), which is why several \"orig state\" members from an older delta-based scheme are now write-only.

### Preset / template back-compat (all LIVE, verified against real producers)
PagePresetIO accepts three formats and normalizes onto the v2 in-memory layout: v2 consolidated (<BaySickPagePreset version=2> with <Engines>/<Strips>/<Racks> wrappers), v1 single-engine, and the K-7 Aria shim (<RustyDrumsPagePreset>/<GuitarsPagePreset>/<BassesPagePreset> roots). The K-7 shim has a real consumer on disk: `Presets/Rusty Drums Page/My Presets/My Rusty Drums Setup.xml` is checked into the repo in K-7 format - do NOT flag this shim as dead. Template load (StandaloneEditor.cpp:8378): version>=2 templates restore through the project path with NEW-PROJECT semantics (full teardown mirroring doFileNew, Jeff's docket 19 ruling); the version<2 branch is the v1 FACTORY schema that the shipped template set (generate_factory_templates output) still uses - also live. PatternManager schema is versioned (\"version\" property, writer stamps 1, reader reads it into an ignoreUnused local - consistent with the badger Task 9 post-v1 migration-hook keep).

### BaySickTitleBar
Paints the engine name (optionally with a stroked-path bloom halo - path-stroke chosen over shifted-font for symmetric glow, per the long comment) over the standard dark strip with 1px divider. Since the 2026-08-06 QA-Layout moves (Rusty band controls to the window title strip; section tab row laid out by AriaControlPanel) it owns NO hosted widgets, resized() is empty, and its parent-managed trailing-area API (getTrailingArea + two width-hint setters + two members) has zero callers - flagged ASK-JEFF above.

### PluginProcessor engine hosting (relevant slice)
Per-page engine pointer arrays (mLayerEngines/mPluginEngines/mBassEngines/mDrumEngines/mClipEngines/mVoxEngines/mInstEngines) each guarded by a SpinLock + a fast-path `mAnyXPageActive` atomic (the standing one-atomic-gate bypass pattern). Per-drum play-pitch is a pre-resolved atomic pointer-to-atomic per page (release-store at registerDrumEngine, acquire-load per trigger). The per-page scratch AudioBuffers that used to accompany these arrays are now dead (MT render tasks own their own buffers - VoxStripTask documents the move); the mixing happens in the task/arena layer. Swing: cached raw-param atomic pointers registered eagerly (ensureSwingParams), scheduler reads through pointers, no per-block APVTS lookups; Vox excluded (no vox MIDI), clip rolls ride the global (Jeff 2026-07-23).

### Method + coverage notes for the verifier
Primary instrument was a full-tree member-identifier occurrence census (rg -o '\\bm[A-Z][A-Za-z0-9_]{2,}\\b' over Source *.h/*.cpp -> 42296 occurrences -> binned): 55 ids occur once, 236 twice; every one was classified by pulling its actual source lines. Comment-only mentions (historical removal notes like mDrumsEngine, mSnapMode, mRustyInserts, mEnvCurvePoints, mPedalsHeaderTitle etc.) were discarded, decl+create attachment pairs (RAII-live) discarded, decl+lock-use and read-modify-write singles discarded. Limitation: identifiers not matching the mXxx convention (locals, statics sX, constants kX, graph-side camelCase fields) were only checked where a finding led there; a same-name member in two classes with combined count >2 would be missed (none of the reported findings have that shape - each was verified against its specific class). The Chamber/Room/Booth reverbs, the BaySickSolstice dual-bind family, the K-7 preset shim, and the v1 factory-template branch all LOOK dead to naive greps and are all live - documented above so the next pass doesn't re-litigate them.

# Capture from finder: dead-classes
### Build topology (CMakeLists.txt, read in full)

Four buildable products from one CMakeLists: (1) **BaySickDAWStandalone** - the real app, juce_add_gui_app, compiles VIBESYNTH_DSP_SOURCES (~60 model/DSP files) plus ~120 standalone UI/DSP files; (2) **BaySickDAW** - legacy juce_add_plugin VST3 target, deliberately kept unshipped as scaffolding, its only exclusive source is Source/PluginEditor.cpp; (3) **BaySickPluginHost** - the plugin-sandbox helper (one source file, Source/Hosting/Helper/PluginHostMain.cpp, links only 3 JUCE modules and NO vendored libs - deliberate, so the 32-bit twin stays cheap; built twice: x64 into build/, x86 via a separate -A Win32 configure into build32/, PRODUCT_NAME carries the 64/32 suffix that SandboxedPluginClient::helperExecutable looks for); (4) two juce_add_binary_data targets. Vendored libs wired conditionally on existence probes with BAYSICK_HAS_* defines: NAM core (static lib, C++20, /WHOLEARCHIVE to keep the registry static-initializer), sfizz (add_subdirectory, options forced OFF for tests/demos), lunasvg (linked but consumed by nothing - see finding), WORLD/RubberBand/Signalsmith/LAME (QA-Fe pitch + QA-Export MP3), ASIO SDK auto-detect with JUCE_ASIO_DEBUGGING. Release builds carry /Zi + /DEBUG PDBs and /arch:AVX2 (hard-crash on pre-Haswell by decision). A POST_BUILD step mirrors repo Resources/ (cassette IRs, acoustic IRs - runtime-loaded overrides) beside the exe, and the helper exe is staged into BOTH app configs' artefact dirs.

### Binary assets pipeline + Rusty kit graphic

Two embed namespaces. **BaySickAssets** (HEADER_NAME BaySickAssets.h to avoid colliding with the default BinaryData.h): 4 files - BaySickDAWLogo.png (consumed twice in StandaloneApp.cpp:679/1198 - splash/about surfaces - plus ICON_BIG), big_rusty_drums.png + control_tab.png (both loaded by name via BaySickAssets::getNamedResource in BaySickRustyDrumsKitGraphic's constructor), and big_rusty_drums.svg (embedded, never loaded - finding). **VibeSynthSamples** (default BinaryData namespace): globs Assets/Samples/*.wav, dir is empty so it embeds a stub text file; zero code consumers ever (finding). The kit graphic itself: user sees the Big Rusty Drums photo-real kit; clicking a drum piece fires an audition through elliptical hitboxes (kDefaultEllipses -> mHitboxes, each with center/radii/rotation/tilt, calibrated 2026-05-04 against the PNG via an in-app calibration overlay since removed); a side-band strip image (control_tab.png) and a static articulation-label dropdown API (getArticulationLabels from kArtics) round it out. Hover/selection indices repaint; layout resets via resetLayoutToDefaults().

### StandaloneEditor timer idiom

Recurring pattern: private structs deriving juce::Timer with an owner back-reference, declared AND instantiated in one statement - 'struct XTimer : juce::Timer { ... } mXTimer { *this };' - so the type name never appears at a separate member declaration (grep trap). Four instances: **DenoisePollTimer** (5 Hz; polls denoise worker state, and deliberately piggybacks pollAutoFreeze + pollPendingWindowDefaults on the same tick - TS7 ruling that auto-freeze must not cost its own wakeup); **CountInTimer** (single-shot; when count-in ends it clears mMetro.countInActive and starts the playhead - Play never edits tempo); **AutomationTimer** (30 Hz; applyAutomationAtCurrentPosition + pollVersionCapture); **PatternLabelTimer** (cheap poll of refreshPatternBox because pattern changes originate in places that cannot reach the box - same no-change-listener polling idiom as tab detection). Also nearby: mApplicatorBaseline (non-APVTS twin of the processor's automation baseline, captured at song entry / restored at exit on the message thread) and restoreAudioStripsFromArrangement (idempotent post-load rebuild of strips/routing for audio-clip rows; isLoadContext guard so a future non-load caller cannot wrongly clear dirty state).

### PatternManager sequence model (PageSequenceData region)

Per page, two parallel sequence representations selected by SeqRouting (VibesynthConstants.h: BasicSequence=0 / ComplexSequence=1), user-switchable per page via a routing ComboBox in SharedUI (id 1=Basic, 2=Complex; SharedUI.cpp:2896-2918 shows/hides the complex controls). **Basic**: basicGrid of BasicStep [MAX_DRUM_SOUNDS rows x MAX_STEPS_TOTAL] + one BasicEnvelope. **Complex** (A1-style): complexGrid of ComplexStep { StepType, velocity 0.8, fxAmount 1.0, note 60, active } - consumed by playback (activity scan + step fetch at PatternManager.cpp:1498/1504) and persisted with per-step properties t/v/fx/n. Rows cover 4 layers + 1 bass + 10 drums under one MAX_DRUM_SOUNDS-row array. bars/stepsPerBar with totalSteps() clamped to MAX_STEPS_TOTAL. **ComplexEnvelope complexEnv** (per page, not per step: AHDSR + swing + triplet) is persistence-only today - saved as cEnvA/H/D/S/R/cSwing/cTriplet, loaded, and read by nothing else (finding). Pattern save/load lives in PatternManager.cpp ~1465 (write) / ~1870 (read).

### One-off diagnostic-trap convention

Two live examples of the namirLog() convention: header-only namespaces of inline functions appending timestamped lines to a file under AppPaths::appRoot() (Documents/BaySickDAW/*.txt), each Rule-4 catalogued in its batch's running notes with an explicit strip-or-keep-at-batch-close disposition. **ClipDropDiag** (QA-ClipDrop, 2026-06-02): Debug AND Release; logs every step of the audio-clip drop cascade to clipdrop_diag_log.txt and raises a popup ONLY on a bail/'produced nothing' anomaly - built to catch a session-state-dependent failure that clears on restart; wired into ProjectManager, BuilderPage, StandaloneEditor. **G3PlayheadDiag** (QA-G3Smoke): #if JUCE_DEBUG only; logs every roll click (x -> raw/snapped beat, snap div, playhead beat) and playhead paint tick to g3_playhead_log.txt, plus a [G3 PAN] channel discriminating pan-ramp arm candidates; wired into 7 files including two voice classes and PluginProcessor. Both batches are closed; neither strip ruling was executed (findings).

### InstrumentPage - the un-adopted Phase-1 page architecture

The original Phase-1 plan (The Path.txt 1E / Pre-Flight Decisions) specified a common base class for Layers/Bass/Drums pages: full-screen engine-selector ComboBox (enum EngineType None/BaySickSolstice/VibePlayer/Legacy808909/BaySickSynth) that locks after first pick, then three sub-tabs (Sound / Piano Roll / EQ; Drums skips the selector for a 14-row roll), plus a mixer-graph node id set in Phase 5. The base was built (setupEngineDropdown, pure-virtual onEngineChosen, addToSubTab helpers) and compiled ever since, but the 'update existing LayersPage/BassPage/DrumsPage' half never happened - all three pages inherit juce::Component directly and grew their own engine-picker/preset architectures. The unit is a time capsule of the pre-dynamic-tab design (superseded by Phase D per-tab engine instances and the EngineRig model).

### Header topology facts worth keeping

Most-included Source headers found during the sweep: DSPBase.h (41 includers - the effect-DSP base with the QA-Layout visual-feed capture/push helpers), AppPaths.h (30 - the QA-ProjectSave central path resolver), UndoActions.h (18) / UndoBracket.h (16) - the undo spine, EngineSidechainHelper.h + MissingFileReport.h (11 each - the latter being QA-Soundness Task 1's new silent-failure reporting), ApvtsDirtyTracker.h + BlockContext.h (10). Filename!=symbol traps catalogued: OscStack.h defines OscLayer; MidiLearnUI.h also defines MidiLearnOutlineOverlay; SharedUI.h hosts BypassLedButton consumed by EffectWindows.h. Every Source header except InstrumentPage.h has at least one external includer.

### Task 1 sweep captures (2026-08-06, pre-fix tree; Task 8 refreshes changed areas)

# Capture from finder: errstrings
### MissingFileReport (Source/MissingFileReport.h)
The QA-Export mechanism for external files that a project references by absolute path but cannot find at load. A header-only namespace over a CriticalSection-guarded static vector of `{what, path}` entries. Restore sites call `MissingFileReport::add("NAM capture", path)` instead of skipping quietly; duplicate `{what, path}` pairs are collapsed (a kit shared by three tabs reports once); the project-load path calls `drain()` once on the message thread and shows a single dialog listing everything missing (drain site referenced from PluginProcessor.cpp:6407). Thread-safe because engine restores can run off the message thread during load. `clear()`/`isEmpty()` complete the API. Current adopters found in this sweep: BaySickGuitars/Basses/RustyDrums processors (sfizz kits), BaySickNAMIRProcessor (mic-sim user IRs only - NOT its primary NAM/IR assets, see finding), NAMPedalStyleDSP (missing .nam only, not corrupt), StandaloneEditor, PluginProcessor.

### NAM/IR amp-sim unit (Source/BaySickNAMIR/)
BaySickNAMIRProcessor is the full amp-sim chain: NAM neural model -> IR convolution -> Mic Sim (A) -> Mic Placement (A), plus a parallel Mic B pair that SUMS into the main buffer (`nam_micb_active` off = byte-identical single-mic chain). It rides on multiple hosts (Vox sub-tab, Inst sub-tab, FX rack slot). Two A/B slots selected by APVTS `ab_slot`; each slot owns its own NAM model + convolution; slot switching is instant because both stay resident. NAM model swap is a wait-free per-slot pattern (mNamPending + mNamSwapPending atomics, audio thread does the swap); a DSP gate bypasses the whole unit when neither slot has a model or IR. Oversampling changes re-Reset loaded models at the OS rate (message-thread listener, spin-waits on pending swaps).
User-facing controls (editor): Browse buttons for .nam model and .wav IR with recent-files popup menus (stored in the app's settings.xml under per-tag recents), drag-and-drop of .nam/.wav onto the editor, A/B slot buttons ("Loads A's NAM model + IR. Click B to A/B compare"), full-rig hint label ("cabinet already included - consider bypassing the IR") driven by the .nam metadata gear_type, Mic Sim mode combo (None / Built-in model / User IR) with per-mode control visibility, Mic B mirror set, placement distance/angle/polar/mix knobs. Load failures from user gestures show an AlertWindow AND set an inline error label.
Persistence: APVTS state plus custom string properties nam_filepath / ir_filepath / nam_filepath_b / ir_filepath_b / mic_user_ir_path, plus SlotA/SlotB ValueTree snapshots holding per-slot tone + per-slot mic user-IR paths. setStateInformation re-loads everything; per-slot mic IRs load into dedicated buffers (one-time cost at project load, no reload on slot switch). A diagnostic file log (Documents/BaySickDAW/namir_state_log.txt) records every save/restore path and load result in both build configs. Failure reporting is split: mic user IRs -> MissingFileReport (both missing and failed-to-load); primary NAM/IR -> log file only (finding). getNamErrorMessage/getIrErrorMessage/mLastMicIrError are dead (no readers).

### NAM Pedal (Source/DSP/NAMPedalStyleDSP.*, panel in EffectEditorPanels.cpp)
A pedal-format NAM: input/drive (-24..+24 dB pre-model gain) -> mono-sum -> NAM model -> 3-band EQ (low/mid/high +-15 dB) -> blend (dry/wet) -> output (-24..+12 dB). Value-change-guarded setters (CPU-safeguard rule). Model load uses the same pending/swap-atomic pattern as the big unit; loadModel only commits mModelPath on success. getModelName appends " (missing)" when mModelMissing so the panel never presents a name it did not load. Panel: Load button + filename label ("(no file loaded)" when empty), 6 knobs, dBFS out meter; picking the NAM Pedal type in a pedal slot immediately prompts for a .nam file (panel is useless without one). State: ValueTree with the six params + bypassed + modelPath. Restore: existing file -> loadModel (err currently dropped - finding); missing file -> remember path, flag missing, MissingFileReport.

### BaySickPedals pedalboard (Source/BaySickPedals/)
A chain of pedal slots (guitar-pedal UI) hosted per Inst page in its own contained window. Each slot has a swappable-type menu (sections: User NAM Pedal / Dynamics / Harmonics / ...), per-pedal preset button with Save / My Presets / Restore Defaults / Save as Default / Reveal Folder (all EffectPresetIO-backed; failures currently silent - finding), bypass, reorder arrows, remove. Whole-board presets: savePedalboardPreset sanitizes the name (filesystem-unsafe chars -> _), writes `<Pedalboard version name>` XML wrapping captureFullState() to a Pedalboards folder; loadPedalboardPreset validates root tag + accepts the V2 payload tag or the pre-2026-07-25 legacy tag, validates the ValueTree, then restoreFullState. InstPage's pedals window menu drives save/load/reveal (errors currently dropped - finding).

### FX rack effect presets (Source/Standalone/EffectPresetIO.cpp, SlotComponent.cpp, EffectsPage.cpp)
EffectPresetIO persists per-effect-type presets under Presets/Effects/... (Factory dir + My Presets dir per type) via the DSP's getStateInformation/setStateInformation, so Type-umbrella state round-trips. Boot-time seeding writes factory presets from a table (temp DSP per preset, prepared at 44.1k/512, configure lambda, XML write), idempotent, re-seeds when factory_seed_version.txt < current version, best-effort silent by design. Folder tree is pre-created for every pedal type so new installs see the full layout. SlotComponent::showPresetMenu is the canonical consumer: Save Current (name dialog), Load Factory / Load My Presets submenus, Restore Defaults, Save Current as Default, Manage Presets (opens folder) - every failure shows an AlertWindow, and a successful load re-mounts the panel so a Type change inside the preset shows the right layout. EffectsPage adds whole-rack presets (FxRackPresetIO): save dialog ("Saves all six slots and both EQs for this channel"), load menu; both report failures via NativeMessageBox, and a successful rack load re-registers slot automation for every occupied slot (preset carries its own slot uuids), refreshes rows, and updates bus latencies.

### Freeze system - automatic paths (StandaloneEditor::pollAutoFreeze + restorePendingFreezes; render internals in PluginProcessor.cpp)
Freeze renders a tab to files in <project>/Freeze/ (per-tab song render + per-pattern renders; the Rusty kit has its own thirteen-strip render path that freezeTab cannot express - freezeRustyKit routes it). Frozen playback substitutes file streams; a stale freeze RETRACTS published pointers so the tab falls back to the live engine (audio never breaks). Content stamps (FNV-1a) enable per-pattern reuse: editing pattern 3 re-renders only pattern 3 + the song scope.
pollAutoFreeze (5 Hz) drives three automatic paths, all gated on transport stopped + a quiet period since the last content edit (a drag re-renders ONCE when hands come off) and skipped entirely while any offline render is active (two begin/endOffline sequences interleaving would corrupt both): (1) stale-refresh drain - one queued job per tick, song scope only per ruling 2-b; (2) staggered pattern fill - one short pattern render per quiet tick so no uninvited action stalls the app for a whole set; (3) CPU auto-freeze - arms after 3 s continuously over the File Settings threshold (fsAutoFreezeCpu, >100 = Off), fires at stop, one tab per trip, skips tabs the user explicitly unfroze (session-scoped userUnfroze), requires a saved project (needs the Freeze dir). Every automatic render shows a render notice overlay first (an unexplained multi-second stall being the worst failure mode). All three failure paths are currently DBG-or-nothing (findings).
Manual freeze lives in the page menu bar (wireFreezeSlotForVisiblePage): tri-state slot (0 not frozen / 1 frozen / 2 frozen-stale), gated behind File Settings "Enable Instrument Level Freeze" (read LIVE, not captured at wiring - a captured read left open pages stale), runs under the HeavyOperationOverlay with progress + cancel, and reports failure in a "Could not freeze" AlertWindow. Restore-on-load (restorePendingFreezes): sweeps orphan freeze files first, honors byUser (a hand freeze restores on ANY machine; an auto freeze only where auto-freeze is armed), seeds saved stamps so unchanged projects render NOTHING, re-renders on span (length@bpm) mismatch; freeze files are a regenerable cache excluded from bundles.

### Export / measure dialog (ExportAudioDialog in StandaloneEditor.cpp; BuilderPage::runExportWithProgress)
One dialog, two jobs. Measure: runs measureRender (CL-227 backend - meters, writes no files by Jeff's ruling), progress bar + percent label, result shown in-dialog and forwarded to the master analyzer window; failure text lands in the dialog's measure line ("Measurement cancelled." when err empty). Export: optional normalize does measure-then-gain (pass 1 = 0-50% of the bar, gain = LUFS target minus measured, boost capped by the true-peak ceiling, applied uniformly at every writer including stems), then renderToFile; failure shows an "Export failed" AlertWindow unless the user cancelled. Options: source select, tail, format/quality/sample rate/dither combos, normalize toggle with typed LUFS target, stems toggle, per-strip include list, loudness-spec combo with custom target. BuilderPage::runExportWithProgress is the older ThreadWithProgressWindow route (launchThread, self-deleting job) - failure also AlertWindow'd. Take-capture reports (VersionCapture::onPersistTake) write LoudnessReportWriter HTML into the project Reports dir; a write failure DBGs every time and alerts ONCE per session (mTakeReportErrorShown) because it fires automatically at every take end.

### Vocal align/pitch editors + auto re-analyze (BaySickAlignEditor, BaySickPitchEditor, VoxPage poll)
Align editor: Analyze (stop-gated for RE-analysis since it swaps the time map mid-play; a FIRST analysis runs even during playback as explicit user intent), ANALYZING... badge with a 30 ms paint pass before the synchronous work, failure pops the pushed undo entry and shows an AlertWindow. Render exports the aligned Follower to Aligned/{name}_align_v{N}.wav with a Standard vs High Resolution choice (oversampled ~384 kHz-class warp phase); failure AlertWindow'd. Versions dropdown lists applied states newest-first with "(grid changed)" markers. Pitch editor: analyze defers during playback for an analyzed channel (Deferred state), one analysis in flight at a time, failure sets a visible Failed state carrying the error; Render bakes to Pitched/{name}_pitch_v{N}.wav (High-Res choice retired here - the phase-vocoder pass it oversampled no longer exists in the pitch path); Reset clears all edits under undo; version snapshots are restore points, never an edit gate. VoxPage runs a 4 Hz auto re-analyze poll for BOTH: fires only when the analysis is stale, the clip signature has been stable ~1 s, the transport is stopped, and this exact state has not already failed (lastAttempt key); errors deliberately unreported (background convenience; manual path reports). renderAlignedPreview discards its error by design (preview-only).

### Recording commit + de-noise (StandaloneEditor::commitRecordingResult, regenerateDenoise, renameRecordingGroup, pollDenoiseState)
A vox record produces DRY (and WET when realtime correction was active at record start) takes; File Settings checkboxes select which variants to keep (Dry / Dry Cleaned / Wet / Wet Cleaned, enum-ordered) plus a de-noise strength. AUTO grid pick = highest-order variant the user asked for (replacing the legacy wet-if-exists rule that put the UNCLEANED wet take on the grid while the cleaned file sat unused - which mattered because pitch/align analyze whatever is ON the grid); an explicit per-strip choice from the arm-LED menu still wins; wet variants are excluded when no wet file was recorded. De-noise profiles come from live learners, else self-learn from the take's own file; profiles are stored per base name. Clean failures currently fall back silently (finding). Unselected source takes are deleted; the picked take lands on the grid, other kept variants go to the audio library routed to the channel. regenerateDenoise (menu, stop-gated) re-cleans a CLEANED file from its source take and reports failures in an AlertWindow with a playback-lock hint. renameRecordingGroup renames all four variant files atomically-ish (checks collisions first, repoints library/block references, rebuilds players in a load-bearing order so streamers release old files).

### Hosted plugin loading (Source/Hosting/)
HostedPluginInstance::instantiate: two tiers - bridged (SandboxedPluginClient -> BaySickPluginHost64/32 helper process) and in-process. Bridge is FORCED for 32-bit (a 64-bit process physically cannot load a 32-bit DLL) and preferred per-plugin otherwise; a non-forced bridge failure falls back to in-process by design. States: Ok / NeedsBridge / Crashed / FailedToLoad, with mError carrying the reason; getStateMessage feeds the editor's dead-marker so a failed/crashed plugin is visibly labeled while the window stays open. The bridged load result arrives async (it used to be invisible - a helper-side failure looked loaded forever) and is marshalled to the message thread; the helper also corrects 32-bit metadata (the 64-bit scanner's description was a filename-only guess), reports the real editor size, streams the parameter list (id TAB name per line) so automation can target bridged parameters, and seeds program info. Helper side (PluginHostMain.cpp): rescans the file, matches by identifier with a sole-description fallback for the 32-bit guess mismatch, and replies error text + load-failed on every failure path. SandboxedPluginClient::mLastError is lock-guarded (reader thread writes, message thread reads) and surfaced via loadFailed()/getLastError().

### Audio device init (StandaloneApp.cpp)
Two-stage open from audio_settings.xml: parse, strip channel-mask attributes (defaults from initialise args), force ASIO input name = output name BEFORE opening (ASIO is one-device-per-driver; a mismatched pair made JUCE silently fall back to the old device so the user's pick never took), then initialise with selectDefaultDeviceOnFailure=FALSE - with TRUE, JUCE opens the type's default device and CLEARS the error, so Jeff picked the UMC ASIO driver and silently got ASIO4ALL, and shutdown then saved ASIO4ALL over his real choice. FALSE hands failures to the app's own fallback, which logs every step to audio_setup_log.txt, keeps the driver's own error text (initErr), names the refused device in a dialog, and preserves the user's choice on disk. Stage 2 rewrites the setup with every channel enabled + restartLastAudioDevice (setAudioDeviceSetup alone can no-op). Multi-core-rendering and kit-trigger-velocity prefs load BEFORE initialise so the first callback routes correctly. Live buffer-size Apply (StandaloneEditor dialog) quiesces the render callback around setAudioDeviceSetup (removeAudioCallback blocks until in-flight callbacks return - the WASAPI-exclusive teardown-race shield) and reports a rejected size in an AlertWindow with previous settings intact; other device changes go through a pending-settings-file + restart-prompt flow, with a written-failure message when the settings file cannot be saved. The ASIO vendor control-panel button stays enabled for a CREATED-but-unopenable device (a panel misconfiguration used to be unrecoverable from inside the app).

### Project bundler (ProjectBundler.h + doExportProjectBundle)
Bundles a project as Zip or Folder, scope References (files the project owns) or SelfContained (+ My Samples + absolute references); Core Library content is never copied (anyone opening the project has it installed). enumerate() walks PatternManager references plus, given the <Tabs> XML from project save, engine-held references: clip paths and sfizz kit paths as attributes, NAM captures + user IRs from Inst chain XML, BaySickPlayer sample paths decoded out of base64 engine-state blobs. estimateCopyBytes drives a size confirmation BEFORE the write. Result carries ok/error/missing/filesCopied; the editor shows write failures in an AlertWindow and lists up to 10 missing references (with an ...and N more line) in the completion dialog - missing files are reported, never silently dropped.

# Capture from finder: outerr-callers
### Freeze system (TS7 Â§6 â€” processor driver + editor orchestration)

**What it is.** Per-tab "freeze": renders an instrument tab's pre-rack output to WAV files in `<project>\\Freeze\\` and substitutes the file for the live engine during playback, saving CPU. The frozen tab's rack, EQ and fader stay live and editable because the render is PRE-rack.

**How it operates.** The processor (`VibeSynthProcessor`) drives freeze state; the RENDERER lives on `BuilderPage` and arrives as hooks (`onRenderFreezeFile`, `onRenderKitFreezeFiles`, `onFreezeStep`) wired by `StandaloneEditor` (~line 4736) â€” the model never reaches into a view. `freezeTab(kind,page,outErr,byUser,reuseValid,songScopeOnly)` renders the SONG scope plus one file per pattern the tab plays in (`patternsWithContentFor`), opens each file as an `AudioClipStreamer` on the shared audio-file thread (synchronous seek(0) pre-fill), then publishes: streams into the rig tab record, pointers into the strip's `RenderTask` via `setFrozenSource`/`setFrozenPatternSource`. Publish-last ordering plus a 20 ms settle guards the audio thread against use-after-free on re-freeze. The Rusty drum kit is ONE action over THIRTEEN strips: `freezeRustyKit` renders all strips in one offline pass (`onRenderKitFreezeFiles`); partial strip results unwind completely ("some strips frozen and some live would play the kit against itself"). Content stamps (FNV-1a via `EngineRig::freezeContentStamp`) gate reuse: `reuseValid` (passed by project restore) skips any file whose stamp still matches; per-pattern stamps are independent so editing pattern 3 leaves 1/2/4 valid. Per-pattern render failures are deliberately NON-fatal â€” the tab freezes in song scope and plays live in the failed pattern (Â§6.6 fall-back-to-live). `refreshFreeze` re-renders a stale tab: nulls the task pointers first (use-after-free guard, found 2026-07-31), 20 ms settle, clears streams, calls freezeTab/freezeRustyKit fresh; on failure it UNFREEZES the tab rather than pointing at a stale file. `renderPatternFreeze(kind,page,pattern,outErr)` is the staggered filler's single-pattern renderer; `findPendingPatternFreeze` scans all frozen non-stale tabs for patterns-with-content lacking a stream entry. Stale marks retract published pointers immediately (`retractFrozenSources` â€” atomic nulls only); a stale freeze plays LIVE, never old audio. Frozen tabs substitute SILENCE while the transport is stopped.

**User-facing behavior + controls.** (1) Manual: title-bar Freeze entry per player page â€” LOCKED (greyed) by default with an unlocking tooltip; enabled by "Enable Instrument Level Freeze" in File Settings (read LIVE, fixed 2026-08-05). Requires a saved project ("the freeze file lives beside it"). Runs under the HeavyOperationOverlay with determinate progress + working Cancel; failure shows an AlertWindow "Could not freeze". Vox tabs carry a Â§6.9 vocal warning. Manual freezes render the full pattern set with stepped progress ("Freezing Drums - pattern 3 of 7"). (2) Automatic: a 5 Hz editor poll arms an auto-freeze after DSP load holds â‰¥ the File Settings threshold (`fsAutoFreezeCpu`, default 80, >100 = Off) for 15 consecutive ticks (3 s); it FIRES only when transport is stopped AND a quiet period since the last edit has passed (arm-on-load, fire-at-stop ruling 2026-07-31). Auto renders are song-scope only (ruling 2-b); pattern files fill in one-per-quiet-tick via the staggered filler. Auto skips tabs the user explicitly unfroze (`userUnfroze`, session-scoped). Freezable kinds: Layers, Bass, Drums, Plugins, Vox, Inst, Clips, Rusty. (3) A frozen tab shows a ribbon mark (painted from live rig state) and the render notice ("Freezing <tab>...") whenever an automatic render fires. (4) Explicit unfreeze via the same menu entry.

**Persistence.** Freeze FILES are a regenerable cache â€” excluded from bundles, never saved with the project. The project saves per-tab freeze state: kind/page, byUser, stale, span (lengthBeats+bpm), pattern stamps. `restorePendingFreezes` runs after engines/racks/strips are rebuilt: sweeps orphan files first, skips auto-freezes when auto is disarmed on this machine (hand freezes restore on ANY machine), seeds saved stamps into the tab, then re-freezes with `reuseValid` so an unchanged project renders NOTHING. Span mismatch (tempo/length changed) re-renders. `sweepOrphanFreezeFiles` deletes files whose tab is gone; a removed frozen tab's file is deleted at removal (`onFreezeFileObsolete`).

**Lifetime/teardown.** Rusty has no rig tab by default â€” one is created lazily (identity-only, null engine) when its freeze slot is wired. Stale-refresh jobs are queued (never rendered inside an edit gesture) and drained one per quiet tick; a dead job (tab deleted/unfrozen meanwhile) is skipped. Never renders while another offline render is active (interleaved suspend/restore would corrupt both).

### Offline render / export path (BuilderPage + ExportAudioDialog)

**What it is.** One offline render loop (`BuilderPage::runOfflineLoop`) with four consumers: `renderToFile` (export), `measureRender` (loudness pre-check), `renderFreezeFile` (single-strip freeze), `renderKitFreezeFiles` (thirteen-strip kit freeze). The loop owns span/scope math, the offline drive (begin/endOfflineRender + restores), the lane-aware clock, per-block automation replay at `kOfflineBlock` = 2048, and tail handling (Tail::Included capped at 60 s for never-decaying content). Consumers differ only in what they do with each block; consumeBlock returning false aborts with an IO error.

**renderToFile.** Opens ALL file sinks BEFORE touching the live processor (file failures never disturb the session). One `FileSink` per output: main mix (skipped entirely under Per-Track pattern export so no transient unwanted file exists on disk) plus one per ticked stem strip, all fed by the same pass. Formats: WAV (16/24/32-bit), OGG (quality index), MP3 via `Mp3Writer`. 16-bit WAV offers TPDF dither, flat or 2nd-order noise-shaped (coeffs 1.0/-0.5, textbook minimal shaper). Per-sink post-gain is uniform across main + stems so the stem sum matches the mix. A failed stem-sink open closes + deletes all already-opened files and returns the error.

**Export dialog (user-facing).** Export Audio dialog: scope selector, tail mode, format/quality/samplerate/dither combos, Normalize toggle with typed LUFS target (measure pass = 0..50 % of the bar, then gain capped so estimated true peak stays under the ceiling, then render pass), stem strip tick-list, and a Measure button (spec combo incl. Custom LUFS). Measure runs on a background thread, returns to the options box, prints result lines in-dialog, and pushes the curve into the master analyzer window; it deliberately writes NO files (ruling 2026-07-31 option B). Export failure shows "Export failed" + reason; cancel is silent and deletes partials. Unsaved projects are walked through Save first (exports land in `<project>\\Exports\\`). `runExportWithProgress` wraps renderToFile in a `ThreadWithProgressWindow` with working Cancel; failures show "Export failed".

**Mp3Writer.** Thin front end over vendored libmp3lame (CBR 128/192/256/320, joint stereo, quality 2, Xing/VBR header rewritten on close). Without BAYSICK_HAS_LAME the stub returns false with "MP3 export is not available in this build." Offline-only (allocates in open/encode).

**LoudnessReportWriter.** Writes "<base> - Loudness Report.html" (+ optional CSV) into the project Reports dir. HTML is always the primary (inline SVG loudness curve, target bar, violation moments, true-peak markers, pass/fail); self-contained and reloadable â€” the measurement data is embedded in an HTML-comment block (`<!--BSDAW-REPORT-DATA`) that `readEmbedded` parses back so clicking a report in the Files browser repopulates the in-app analyzer. Not rendered in-app (JUCE_WEB_BROWSER=0). Retained version-capture takes persist their analysis through this same writer (violations from Short-Term UI-timer capture; curve/summary/verdicts real); a take-report write failure alerts once per session then suppresses (documented).

### Effect preset IO (EffectPresetIO + rack SlotComponent + pedals slot menu)

**What it is.** Generic per-effect preset save/load for every rack DSP. Folder layout: `Documents/BaySickDAW/Presets/Effects/{TypeName}/Factory|My Presets/{name}.xml` + optional `Default.xml`. Preset XML = wrapper element with metadata + the DSP's getStateInformation blob base64-encoded, so Type-umbrella state (Compressor/Saturation/Delay variants, A/B snapshots) round-trips. Factory presets are seeded from a static configure-lambda table on first launch (write-on-missing, so a deleted factory preset reappears next launch). `restoreDefaults` copies a fresh DSP's state over the live one. `safeFileName` strips path-unsafe chars, falls back to "untitled". Shared non-per-type folders: `userNamPedalsDir()` (.nam captures), `irDir()`.

**User-facing.** In the FX rack, each slot's Preset menu offers Save Current Preset (name prompt), Load: Factory / Load: My Presets submenus, Restore Defaults, Save Current as Default, Manage Presets (opens folder). After a load the inline panel is re-mounted so a Type change in the preset shows the right layout. Rack-side failures all alert ("Could not save preset" / "Could not load preset" / "Could not save default"). The pedalboard's per-pedal slot menu offers the same operations (Save Preset As / Factory Presets / My Presets / Restore Defaults / Save as Default / Reveal Folder) â€” but currently drops all four error results (findings above) and does not re-mount after load.

### FX Rack presets (FxRackPresetIO + EffectsPage)

**What it is.** Saves a WHOLE strip's effects setup as one preset: all six rack slots (types, order, uuids, bypass, output gains, every DSP blob) plus the strip's pre-rack and post-rack EQ8 M/S. Distinct from EffectPresetIO (one effect) and PagePresetIO (whole page). The rack is a state BLOB but the EQs are captured as APVTS PARAMETERS (`_mid_eq`/`_side_eq`/`_preeq_*`) because processBlock pushes params into the EQ DSP every block â€” restoring DSP state would be overwritten within one block. Loading rewrites the saved EQ prefix to the destination strip's, so a vocal-strip preset loads onto a guitar strip. Folder: `Documents/BaySickDAW/Presets/FX Rack/My Presets/`.

**User-facing.** EffectsPage menu: Save FX Rack Preset (name prompt, default "<channel> Rack"), Load FX Rack Preset submenu, Open Presets Folder, VU Calibration (-18..-14 dBFS). Save/load failures alert via NativeMessageBox. After a load, every automation lane on the channel is re-registered against the NEW slot uuids (the preset carries its own), rows refresh, and bus latencies update.

### BaySickNAMIR (guitar amp sim: NAM model + cab IR + dual mic sim)

**What it is.** Amp-sim processor: input gain â†’ gate â†’ NAM neural model (per A/B slot) â†’ cab IR convolution â†’ Mic Sim A (+ optional Mic B) with placement stages â†’ low/high cut â†’ output. Two full tone slots (A/B) with per-slot snapshots; slot switch is instant because per-slot NAM models, IRs and mic IRs are all resident.

**How loads work.** `loadNamModel`/`loadImpulseResponse` run on the caller's (message) thread: parse/validate, ResetAndPrewarm at the active oversampling rate, then hand the model to the audio thread via a pending-pointer + swapPending atomic (wait-free adoption at block start; no dealloc on the audio thread). Full-rig captures (`gear_type` = amp_cab/amp_pedal_cab) are flagged so the editor can suggest bypassing the IR. On success the path is recorded (`mNamPaths`/`mIrPaths`); failure leaves the previous model active. `loadImpulseResponse` before prepare stores the path and returns false claiming the IR "will load on next prepare" â€” currently no prepare-time reload exists (finding). MicSimDSP `loadUserIr` loads a per-slot user mic IR convolution (Stereo/Trim/Normalise), records path + loaded flag.

**User-facing.** Editor: Browse buttons for .nam model and .wav cab IR with recent-files menus (stored in settings.xml), drag-and-drop of .nam/.wav anywhere on the panel, file-name labels, inline error label + AlertWindow on any interactive load failure. Mic Sim per mic: mode None / Built-in (archetype dropdown with tooltip) / User IR (file picker; successful load auto-switches mode to User IR with an undo gesture). Params (APVTS): input_gain, output, gate_threshold/release, low_cut/high_cut, cab_mix, nam_bypass, cab_bypass, oversampling (0/1/2 â†’ 1x/2x/4x, polyphase IIR, latency reported), mic sim mode/model/mix + placement distance/angle/polar/mix for A and `_b_` twins, nam_micb_active, ab_slot.

**Persistence.** getStateInformation saves APVTS + path properties (nam_filepath/ir_filepath + _b variants, mic_user_ir_path) + SlotA/SlotB snapshot children (per-slot tone + per-slot user-IR paths). Restore re-loads both slots' NAM models and IRs (namirLog diag file `Documents/BaySickDAW/namir_state_log.txt` records each attempt), restores snapshots, loads each slot's mic IRs (missing/failed ones go to MissingFileReport â€” the one-dialog QA-Export mechanism), syncs active slot, and fires `onStateRestored` async so editor labels refresh. Param-only changes that need mic-sim reconfiguration are handled on the message thread (prepareToPlay won't re-fire).

### NAMPedalStyleDSP (NAM pedal inside FX rack / pedalboard)

Rack-effect wrapper around a NAM capture used as a pedal: Input/Low/Mid/High/Blend/Output knobs + model file. Same message-thread-load + pending-swap pattern as the amp sim (drainPendingSwap at block start). `getModelName` appends " (missing)" when the saved capture is absent (QA-Export Task 5: never present a name that was not actually loaded). Both interactive load paths (rack panel Load button, pedalboard's prompt-on-add) alert "NAM Load Failed" + reason. State = ValueTree with the six knobs, bypass, modelPath; restore reports a MISSING capture via MissingFileReport but silently drops a corrupt-but-present one (finding). Adding a NAM Pedal to a pedalboard slot immediately prompts for a .nam file (the panel is useless without one).

### Pedalboard presets (BaySickPedalsProcessor + InstPage menu)

`savePedalboardPreset`/`loadPedalboardPreset` capture/restore the ENTIRE pedalboard (all slots + state) as one XML in `Documents/BaySickDAW/Presets/Pedalboards/`. Save sanitizes the name, creates the folder, writes root tag + version + full-state child. Load accepts both the V2 payload tag and the pre-2026-07-25 legacy tag. Errors are precise (missing file / wrong tag / missing or invalid payload) â€” but the only caller (InstPage's pedalboard preset menu, anchored to the title-bar preset button) drops both results (findings). Menu contents: Save Pedalboard Preset (name prompt), enumerated preset list (ids 1000+), Reveal Folder.

# Capture from finder: existsasfile-standalone
### MissingFileReport (the load-time reporting spine)
`Source/MissingFileReport.h` is a header-only, thread-safe accumulator for external files that fail to resolve during a restore. Restore sites call `MissingFileReport::add(what, path)` instead of silently skipping; the project-load path drains once (`VibeSynthProcessor::reportMissingFilesIfAny`, also called at the end of the v2 template apply) and shows ONE dialog listing everything. Dedupes on (what, path) so a kit shared by three tabs reports once. Engine-side consumers today: NAM captures (NAMPedalStyleDSP), sfizz kits via the three engine processors' setStateInformation, NAMIR, and the editor's clip-audio check at StandaloneEditor.cpp:16479. Direct user operations (menu preset picks, drops) report via `juce::AlertWindow` instead, e.g. PagePresetIO's kit-missing alert, loadKitImpl's three-stage failure alerts, placeAudioLibraryEntry's \"Audio File Not Found\".

### Big Rusty Drums page (BaySickRustyDrumsPage)
One singleton sfizz engine, one ribbon tab. USER-FACING: a \"Load Player\" dropdown offers the two programs, Full and Basic; first pick loads with no prompt (one undoable transaction, undo returns to the empty player - ruling 1A 2026-08-06); switching programs raises a warning that mixer settings and the piano roll across every pattern will reset, confirm/cancel, with the dropdown reverting on cancel. A missing program SFZ is reported with an AlertWindow and the dropdown reverts. \"Player Preset\" menu (separate from the hamburger's Page Preset) saves/loads CC-value overlays only: every registered CC (kCcCount, includes extended CCs >= 128 like the hi-hat macro CC400/401) plus brd_outVol, written to `<approot>/Presets/Rusty Player/*.xml` with auto-numbered collision names. Loading a player preset saved on a DIFFERENT program prompts before switching; same program applies CCs immediately under ScopedProgrammaticParamWrites (no dirty, no undo-history pollution). Undo wraps at the TERMINALS inside loadPlayerPresetFromFile because the cross-program branch finishes in an async confirm callback (regression fix 2026-08-06). Kit GUI: the ARIA control panel renders the program's GUI XML from the kit's GUI folder; missing XML degrades to placeholder text.

### ARIA control panel (AriaControlPanel)
Renders a sfizz kit author's GUI XML (sfizz/ARIA `<GUI>` format) as live controls bound to the engine's APVTS. Tab strip: \"Main\" = the program XML, plus auto-discovered 03-kick..08-noises zoom pages (drum kits only; strip suppressed entirely when only Main exists - K-5 fix #3). Parses StaticImage (full-surface image becomes the background), StaticText, and widget elements; images load from the kit's GUI dir through a filename-keyed image cache, missing images render as nothing. Colors parse #RRGGBB/#RRGGBBAA with a fallback. `setEngine` rebinds all widget attachments when the engine instance is replaced (program switch / InstPage chain rebuild). Native size read from the XML (default 775x335) and scaled to fit.

### Layers / Bass pages - engine presets and page presets
Two near-identical twins (LayersPage/BassPage). USER-FACING: right-click context menu offers Load Preset (submenu listing `<engine>` preset dir XMLs), Save Current Patch As (name prompt -> `BaySickEnginePreset` wrapper XML with base64 engine state), plus the hamburger's Save/Load Page Preset (full page: engine + mixer strip + racks via PagePresetIO; loading renames the tab to the preset filename). loadPreset accepts three formats: BaySickPlayer factory wrapper (`BaySickPlayerState` outer with inner apvts + `<Sample kind path>` sibling, path optionally \"library:<rel>\" against the Core Library), `BaySickEnginePreset` base64 wrapper, or raw apvts XML. On load it rewrites per-tab param-id prefixes (`tk_<row>_<idx>_<engineTag>_` - the F-2 fix derives the engine tag as the segment before the trailing underscore, because saved presets carry a different page index) so state applies across pages, applies the tree, then loads the Sample reference through the processor wrappers (stamps path onto apvts.state for project-save replay; NO normalizeRootNotes on Layers/Bass - melodic keymaps preserved; DrumPage normalizes to 60). Multi-second SFZ loads run under a HeavyOperationOverlay busy indicator. Structural undo (QA-UndoCoverage T7): performChainSwapGesture snapshots full Page Preset XML before/after into UndoSnapshotStore temp files; apply resolves the LIVE page through the undo context (delete+resurrect replaces the object; SafePointer is fallback only).

### DrumPage sound management
Per-drum tab owning one BaySickPlayer or BaySickSynth engine through the EngineRig. USER-FACING sound menu: library instruments (Core Library scan), factory/user drum patches (root tag routes to player vs synth loader), Browse Samples (file/folder chooser), Load SFZ, New Patch, Save Current Patch As, None (clear). Every sound swap runs through performSoundSwapGesture = one StructuralOpAction whose before/after are full Page Preset XML snapshots; an empty side means \"no engine\" and applies as clear; undo of a sample load is itself a load (accepted async property, Jeff 2026-08-06). loadPlayerPreset applies the apvts child (prefix rewrite on `_bsp_` ids) via replaceStateKeepingUndoHistory, then reloads the `<Sample>` reference (library: or absolute); missing sample leaves the engine's sample slot empty by code comment. loadPagePreset resyncs the page's non-apvts tracking (mLoadedSampleKind/Path/mSoundName) from the engine's bsp_loadKind/bsp_loadPath after import, else savePatchAs could not reconstruct the load call. Dirty tracking: takeStateSnapshot baseline vs current getStateInformation.

### Effect preset IO (EffectPresetIO)
Folder layout `<approot>/Presets/Effects/.../<TypeName>/{Factory, My Presets, Default.xml}`; pedals additionally get their full folder tree pre-created at boot so fresh installs show a complete layout (I-15c), plus a separate User NAM Pedals folder for .nam capture files. Preset file = `<EffectPreset version type name blob>` with base64 of the DSP's getStateInformation. save/load/read all report through `juce::String& outErr` return-bool contracts. Boot sequence: migrateTapeFolderToSaturation (moves legacy Tape My Presets, skips name collisions, wipes stale Tape Factory + Default), then versioned factory seeding - kFactorySeedVersion stamp file forces a factory rewrite when the table or serialization changes, otherwise skip-if-exists keeps the seed idempotent; My Presets never touched; failures during boot seeding are documented best-effort silent.

### Acoustic IR / NAM panels (EffectEditorPanels)
AcousticPreampStylePanel: 4 knobs + chickenhead body selector (Dreadnought/Parlor/Jumbo/User) + \"Load IR...\" (active-looking only in User mode, alpha-dimmed otherwise). Picking a .wav calls dsp->loadUserIR and auto-switches to User mode. AcousticSimulatorStylePanel mirrors it with modes Standard/Jumbo/Enhanced/Piezo/User; in User mode the Body knob becomes IR wet/dry. Both DSPs persist the IR path in state (\"userIR\") and reload on restore; named modes convolve a bundled capture, gracefully degrading to curves-only if the bundled file is absent. NAMPedalStylePanel loads .nam models through `loadModel (f, err)` and shows a \"NAM Load Failed\" AlertWindow with the driver-supplied reason - the model error-reporting pattern the acoustic panels lack.

### Project bundler (ProjectBundler)
Free-standing walker + writer shared by Export Bundle and Pack Project. enumerate() collects every external audio reference: audio library entries, arrangement block paths, and (given the serialized `<Tabs>`) engine-held refs - explicit attribute whitelist (bsp_loadPath, kitPath, namPath, irPath, micUserIrPath, micbUserIrPath, plus `path` on KitPath/Sample tags only), including refs inside base64 engine blobs, deduped on stored path. classify() buckets each as ProjectRelative / UserSamples / CoreLibrary / Absolute / Missing. Core Library NEVER copies (docket 22=b - target installs have it; copying would drag GB-scale sfizz folders). Scope References copies nothing extra; SelfContained copies UserSamples + Absolute into `Samples/` in the bundle. estimateCopyBytes gives a pre-write size figure. write() supports Folder (copyDirectoryTo, then deletes the regenerable Freeze cache) and Zip (freeze-excluded file walk into a ZipFile::Builder); Missing refs surface in Result::missing; creation/copy-project/zip-write failures set Result::error; per-file copy failures in the Folder branch currently do not (finding).

### Standalone app shell (StandaloneApp)
Startup: splash -> Tape migration + factory preset seed -> processor + playhead -> AudioDeviceManager. Audio settings live in `Documents/BaySickDAW/audio_settings.xml` (legacy Roaming path is a read fallback pre-migration). The audio-settings dialog writes `audio_settings_pending.xml` + restart; startup promotes pending over live atomically, and shutdown skips the auto-save when a pending file exists so the old device never overwrites the user's new pick. Device open: saved channel masks are stripped pre-initialise (stale masks disable most channels when swapping to a bigger interface), ASIO input name is forced equal to the output name pre-open (one-device-per-driver; a stale input name made JUCE silently fall back to the old device), selectDefaultDeviceOnFailure is FALSE (2026-08-05) so a refused device returns null + the driver's own error - logged to audio_setup_log.txt and shown in a failure dialog - instead of silently opening a device the user never picked; saveAudioSettings never lets an empty device name (failed open, asleep headset, fallback session) overwrite a saved non-empty one, so the user's choice survives hardware absence. Master-output routing (first channel + mono) persists per-machine in a sibling master_output.xml. settings.xml (shared, sibling-preserving XML) holds Multi-core Rendering (read before initialise so the first audio callback routes correctly), MIDI trigger velocity, transport display, LUFS readout mode, recent colors/files/projects. MIDI: all detected inputs get callbacks; first launch enables all; saved MIDIINPUT entries pin enablement, with a stale-identifier safety net (no saved id matches any device -> enable-all, covers USB re-instancing). Shutdown: bounds flushed while windows are alive, editor teardown under a self-pumping overlay (Software Renderer peer), window destroyed last, settings.xml written once from the in-memory bounds map.

### Templates system (StandaloneEditor)
Two schemas. v2 (user templates, QA-ProjectSave T2): one XML carrying the PROJECT shape - writeProcessorState + structural UI state (tabs, strip names/orders; deliberately no session extras), loaded through the project restore path with NEW-PROJECT semantics (docket 19: current song, patterns, arrangement, library all cleared first) and ends with reportMissingFilesIfAny. Volatile sample refs are adopted into My Samples at save (docket 23/24) so templates never point at Downloads. v1 (the shipped factory set): bespoke Layer/Bass/Kit attribute schema; teardown is scoped to Layers/Bass/Drums only (that is all it can rebuild); kit loads via loadKitImpl from the factory Kits tree, Layer/Bass tabs spawn via spawnLayer/BassTabFromTemplate (selectEngine + optional preset + lock). Every template load passes the confirmDiscardChanges dirty gate (T3) placed in loadTemplate itself so all entry points inherit it. Menu: File > New from Template - Default (shows its name, greys with \"(missing)\" if the pointer is dangling), Premade (recursive factory walk), My Templates; default set via a chooser (doFileSetDefaultTemplate), cleared under Options. Whole apply runs under the project-load shield (mProjectLoadInProgress) with aux-insert teardown at every load-type entry point.

### Drum kit system (loadKitImpl / saveKitAs)
Kit XML root `BaySickKit`, entries either factory refs (presetPath relative to `<approot>/Presets`, engine attr names BaySickSynth/BaySickPlayer) or embedded per-drum state. Load: three up-front failure alerts (missing file / unparseable / wrong root); tears down all DrumPage tabs but NEVER the Rusty tab (also Drums-typed; LIFE-01 singleton) under a HeavyOperationOverlay; spawns a tab per entry; per-drum failures (preset missing on disk, unknown engine) accumulate into loadFailures for an end-of-load AlertWindow - the model for partial-failure reporting. Kit loads do not frame their drum windows (2026-08-04: a full kit of player windows at once is noise; window appears on first tab select). Landing surface = the piano-roll DRUM KIT view, not an individual player. Zero Drums tabs is legal (docket 18 removed the respawn). loadKitWithUndo wraps the whole swap as one StructuralOpAction over full-preset snapshots of every drum tab.

### Project open / tab restore (deserializeUIState)
doFileOpen: dirty gate -> directory picker (a project IS a folder) -> validates project.xml BEFORE teardown (resetToBlankState is not undoable) with an alert on a non-project folder -> close all dynamic tabs, clear strips, blank state, ProjectManager::openProject (alert on failure), restore strips + UI. Tab loop restores per type: Clip tabs re-match their file against the audio library by owner channel + resolved path, report unresolvable clip audio via MissingFileReport (\"Clip audio\"), then createClipStripAndPage + engine state; Vox/Inst ensure their mixer strip exists (legacy persist-renames-only semantics dropped strips of unrenamed tabs); sfizz Inst tabs set source first, then race-safe kit load through the processor (loadBaySickGuitars/BassesKit - engine slot created with processing disabled around the sfizz load), then decode the sfizzEngineData blob and replaceStateKeepingUndoHistory directly (setStateInformation would re-run loadKit without the active-flag guard - crash path), then program cache + piano-roll registration; Rusty restores through the page's reloadForProjectRestore + CC overlay (same setStateInformation bypass; currently broken for stable-root kit refs - see finding 1). Piano-roll active engine restores last, falling back to Drum Kit silently when the saved engine no longer exists.

### Recording commit (commitRecordingResult)
A take is ONE undoable gesture (Jeff 2026-08-06): library entries, grid blocks, and captured MIDI go into a single transaction; WAV files always stay on disk. Master capture (no strips armed) moves to the project's Exports folder with dupe-numbering and appears in the browser panel - never auto-dropped on the grid (adding it is the user's own routed gesture, which owns the undo); a failed move falls back to the legacy Audio-row drop rather than losing the take. Per-strip files: Vox strips produce dry+wet; the written take set = File Settings checkboxes (Dry / Dry Cleaned / Wet / Wet Cleaned) union the session pick; AUTO pick = highest-order variant the user asked for (replaces \"wet if exists\" which silently denied the pitch/align editors the cleaned audio); explicit per-strip arm-LED pick wins; wet variants clamp down when no wet writer ran; unselected SOURCE takes are deleted per spec; the pick lands on the grid via dropWavAsClip (routeChannel links Vox/Inst clips to their page chain; channel 0 spins up a new Audio row + strip), everything else goes browser-only. De-noise: live learner profiles preferred, self-learn from file as fallback, profiles stored per base name; clean failures currently fall back silently (finding). FL-style pre-roll: contentStartSamples stamps the count-in head so the visible clip starts at the downbeat; stop-during-count-in takes (no post-roll content) keep the WAV but create no entry/clip by design. Captured MIDI routes to the last-touched piano roll; drum recordings demux PER NOTE through trigger bindings into pat.drumRolls[].

### Version capture / takes (VersionCapture + analyzer integration)
Automatic loudness \"takes\": every play-press starts one; a loop wrap starts one only if the project changed since the last kept take (change stamp). Curve sampled at kHistoryHz into short-term LUFS history + LRA; per-tick true-peak / short-term violations coalesced with tick-granularity spans (rendered measurements resolve to the audio block instead - different granularity, same rule). Takes with no measured audio (scrub, instant stop) are discarded, including their audio file. Audio capture is optional (File Settings): targets are timestamped \"Take N - <stamp>.wav\" (session counter resets, so never bare Take N - a FileOutputStream APPENDS on collision and corrupted both takes), living in a per-session temp folder deleted on teardown, or under `<project>/Reports` in retain mode (which also persists the analysis via onPersistTake). Master Analyzer window lists takes; takes without audio show \"(no audio)\" and their Export button disables; export copies (never moves) with a failure alert. Loudness reports write HTML (+optional CSV, partial-success worded) into Reports; the HTML embeds a machine-readable data block between BSDAW-REPORT-DATA markers that \"Open in Analyzer\" parses back, verdicts recomputed against the CURRENT spec table; foreign/hand-edited files are refused with an explanatory dialog rather than opened empty.

### Browser / grid audio import (BrowserPanel + ArrangementGrid)
Browser tree: library entries (persistent, owner-channel grouped) plus Exports (*.wav/ogg/mp3) and Reports (*.html/csv) rows scanned live from the project folders. Exports rows offer \"Add to Project...\" (flat target list: every routable page + new Clip/Vox/Inst page - no Move/Copy question, file is unrouted) and Reveal; report rows offer Open in Analyzer. Library rows: Rename, Regenerate De-noise (light/strong), Duplicate (name prompt, no-overwrite contract - Rename/Cancel only, physical copy then page spawn with full state clone; copy failure alerted), Reveal (parent-folder fallback), Properties, Choke Group, Delete (confirmation with block/page cascade counts). Grid clips: missing files paint RED (resolve-through-project first - the QA-E fix that stopped relative paths from false-missing), route-type colors otherwise (Clips amber / Vox teal / Inst navy / unrouted grey-teal). importAudioFile computes true wall-clock length at the tempo in effect at the drop bar (originalBPM = that tempo so the stretcher is off at import) and reads length from the file header only; placeAudioLibraryEntry places existing entries without re-import, alerting on missing files (QA-H). Clip Properties dialog: per-clip pitch/BPM/stretch with library-master follow semantics (isOverride derived), detected-tempo display (path-cached), Move (relinks the shared entry) and Copy (physical duplicate + retarget) flows. The clip-drop cascade is instrumented end-to-end by ClipDropDiag (append-only file log + alert() for produced-nothing anomalies).

# Capture from finder: existsasfile-rest
### MissingFileReport (Source/MissingFileReport.h)
The QA-Export reporting mechanism for external files that fail to resolve at load. A header-only namespace with a CriticalSection-guarded static vector of `{what, path}` entries. Restore sites call `MissingFileReport::add(what, path)` instead of silently skipping; adds dedupe on (what, path) so a kit shared by three tabs reports once. The project-load path calls `drain()` once and shows a single dialog listing everything missing. Thread-safe because engine restores can run off the message thread during load. Current confirmed users: Guitars/Basses/Rusty kit restore (missing-file case only), NAMIR Mic A/B user IRs (both missing and failed-to-load), NAM pedal capture (missing-file case only). Gaps found this sweep: NAMIR's main NAM model + cab IR restore, both Acoustic DSP user IRs, corrupt-file (parse-failure) cases across the sfizz trio and NAM pedal, VibePlayer SFZ sample resolution.

### ProjectManager (Source/ProjectManager.cpp)
Owns project lifecycle for Documents\\BaySickDAW\\Projects\\<name>\\ folders (each with project.xml + Samples/). Settings live in Documents\\BaySickDAW\\settings.xml (moved from Roaming 2026-04-23; audio_settings.xml is separate). Name validation enforces Windows filename rules incl. reserved device names and 255-char cap; collisions auto-suffix \" (N)\".
- Dirty tracking (QA-UndoCoverage T9): dirty = transaction-pointer mismatch on the processor's TransactionTracker; markDirty() now only advances the change stamp used by version capture. clearDirty() at load boundary wipes undo history + snapshot store + tracker.
- Autosave: timer (default 15 min) fires writeBackup() regardless of dirty (FL convention). Backups land in <project>\\Backups\\ or Documents\\BaySickDAW\\Backups\\Unsaved\\ when no project is open, named `<stem>_backup_YYYY-MM-DD_HH-MM.xml`, rolling retention newest 10. Backups never clear dirty.
- restoreBackup: copies live project.xml to project.xml.before-restore as a safety net, copies the backup over project.xml, replays state in-memory via deserializeProject with mIgnoreDirty raised, then clearDirty.
- openProject: fires onBeforeOpenProject (transport stop), sets the project folder BEFORE deserialize so path resolution works, then pushes recents.
- saveProjectAs duplicates the whole current folder (preserving Samples/) then rewrites project.xml from memory. deleteProject uses moveToTrash. duplicateProject copies with auto-suffix.
- importSample (source-aware, QA-ProjectSave T4): files already under Core Library or My Samples are referenced via SampleLibrary::makeStableRef, never copied; volatile sources copy into <project>/Samples/ with size+modtime dedupe and auto-suffix. All steps log to ClipDropDiag (file log; alert popup only on a real copy failure). duplicateSample always produces a NEW numbered copy (Properties \"Copy\" contract).
- Settings XML preserves foreign sections on rewrite; stores recents (max 10), shortcutCreated, migratedFromRoaming, skipGlobalLockPrompt, skipKitReplacePrompt, defaultTemplate.
- First-launch housekeeping: one-shot Roaming->Documents migration (audio_settings.xml + Presets/), one-shot \"Sample Library.lnk\" shortcut creation pointing at the Core Library dir.

### ClipsPage (Source/Clips/ClipsPage.cpp)
The Clips tab page: single-engine (BaySickPlayer only), a non-owning view over the rig-owned engine keyed (TabKind::Clips, pageIndex), APVTS prefix `clip_<N>_` via rig trackIdFor. Header strip shows the bound audio filename (green label, \"(no clip)\" when unbound). User-facing controls all live on the title strip's Menu dropdown (the decorative picker was removed in QA-Layout T2): Lock, Rename, Duplicate Clip (new tab), Choke Group (None/1-16), Save Page Preset As..., Load Page Preset (recursive factory+user submenu from `Clip Page/` presets dir), Delete Clip. Delete warns that the Player/strip/rack/roll/library entry go away while the WAV in Samples/ stays; a dirty page offers \"Save Page Preset & Delete\" which chains the delete through savePagePreset's onSaved callback. Page presets (PagePresetIO, kind Clip) capture engine + strip (`mixer_audio_<N>`) state and embed a copy of the bound audio into Documents/BaySickDAW/My Samples with a relative `clipRef` attribute so presets travel across projects; load resolves clipRef back out of My Samples. Dirty tracking is a ValueTree listener on the engine APVTS with a suppress flag during bulk restores. setClipFilePath also tags the audio library entry with the STORED/relative path (dedup contract) under the page's audioInsert channel id. Duplicate flow uses exportClipState/importClipState (base64 engine blob with APVTS prefix substitution).

### Sfizz engine trio kit load/restore (BaySickGuitars/BaySickBasses/BaySickRustyDrums processors)
Each engine wraps a processor-owned sfizz instance (NOT rig-owned - race-safe load paths). loadKit: verify file + sfizz handle, `loadSfzFile` (Guitars/Basses) - Rusty instead synthesizes an output-routed wrapper SFZ (inlining every piece's _map*.sfz with output=N injected) loaded via loadSfzString so each kit piece routes to its own sfizz output bus/mixer strip, dumping `_wrapper_dump.sfz` into the kit root for inspection, and falls back to a plain file load (summed single output) if synthesis fails. After load, all three walk the program's #include chain (depth 4) collecting `#define $macro`, `set_cc<N>=` and `label_cc<N>=` directives to seed CC defaults into APVTS (`<prefix>cc<N>`) + snapshot for double-click reset + panel labels. Rusty additionally: discoverChannels buckets `*_map.sfz` mapping files by a drummer-order rule table, filtered by which mappings the loaded program #includes (Basic ~8 strips vs Full 13); discoverPianoRollKeymap parses Programs/keymap/keymap.sfz `#define $name <midi>` lines into the roll's labeled keys.
State: getStateInformation persists APVTS + KitPath (Rusty persists via SampleLibrary::refForPersist so shipped-kit paths survive account changes; resolved back with resolvePersistedRef). setStateInformation order is load-bearing: kit FIRST (its set_cc defaults stomp APVTS), replaceStateKeepingUndoHistory SECOND so user CC values win. Missing kit file reports through MissingFileReport (\"Guitar kit\"/\"Bass kit\"/\"Drum kit\"); a parse failure of an existing kit is currently swallowed (finding).
PluginProcessor wrappers (loadBaySickGuitarsKit/BassesKit/RustyDrumsKit): the race-safe entry - flip the per-slot active atomic false, spin up the engine under a SpinLock if absent (with playhead + prepare), disable per-instance processing during the multi-second sfizz mutation, report load progress via onLoadProgress (drives the load overlay), then flip active true and fire onSfizzEngineReady for model-side automation registration.

### BaySickNAMIR (amp sim: processor + editor)
Two A/B slots, each with a NAM model (neural amp capture, .nam) and a cab IR (.wav convolution). loadNamModel: message-thread file I/O, nam::get_dsp, full-rig auto-detect from metadata.gear_type (amp_cab/amp_pedal_cab -> editor hints to bypass the IR), ResetAndPrewarm at the oversampled rate, then a lock+spin swap-pending handoff to the audio thread. loadImpulseResponse loads into the slot's juce Convolution (stereo/trim/normalise); if called before prepare it stores the path for the next prepare and returns false with an explanatory outErr. Both use the outErr+bool contract; the editor's browse/drop paths alert failures and maintain per-type recent-file lists (RecentNAMFiles/RecentIRFiles) in the shared settings.xml. Gate: fixed 1 ms attack, user release knob, threshold in dB.
Mic sim: two MicSimDSP instances (A and B), each with per-slot resident user IRs (instant A/B switching, no reload). Per-slot tone state lives in SlotA/SlotB snapshot ValueTrees (H-6d) capturing knobs + mic sim/placement params + per-slot user IR paths; getStateInformation captures the active slot's snapshot before serializing, setStateInformation restores both snapshots, loads each slot's mic IRs (MissingFileReport on missing OR failed load), syncs active-slot pointers, applies the active snapshot to APVTS, and finally fires onStateRestored async so editor labels refresh (Bug C fix - labels are otherwise only touched by explicit browse/load). A pre-snapshot global `mic_user_ir_path` fallback exists for old projects. Diagnostics: namirLog appends to Documents/BaySickDAW/namir_state_log.txt in both build configs. Restore failures of the MAIN model/IR paths only reach this log (finding).

### NAM Pedal (Source/DSP/NAMPedalStyleDSP.cpp)
A pedalboard-slot effect hosting one NAM capture: signal = input trim -> mono-sum -> NAM inference (dual-mono spread) -> 3-band post EQ (RBJ shelves/peak at 100/1k/5k) -> blend -> output trim; empty slot passes the input-gained signal through. Model swap uses a pending/active unique_ptr pair with an atomic swapPending drained at block start. Audio-thread inference exceptions fill the block with silence (correct silent bail). getModelName appends \" (missing)\" when the remembered capture didn't resolve (QA-Export: never present a name that isn't loaded). State persists knobs + modelPath + bypass; restore reports a missing capture via MissingFileReport but drops a corrupt one silently (finding). When the user first assigns a NAM Pedal to a slot the editor immediately opens the .nam chooser (panel is useless without one) and alerts load failures.

### Acoustic Preamp / Acoustic Simulator style DSPs (Source/DSP/)
Acoustic Preamp: piezo-to-real acoustic body correction. Named bodies (Dreadnought/etc.) convolve a bundled base-correction IR (Resources; missing -> adaptive-only processing by design) plus an adaptive de-quack/modal/size-tilt layer; User body runs a static wet/dry convolution of a user-loaded IR (Resonance knob = wet). Controls: Body selector (incl. User), Resonance, Ambience (Schroeder), Notch (50-1000 Hz, on/off), Level; Load IR button dims unless Body=User. Biquad coefficients are computed with in-house RBJ math specifically to avoid JUCE's heap-allocating coefficient makers at block rate.
Acoustic Simulator: electric-to-acoustic. Named modes = voicing EQ curves over a bundled shared body-capture IR (missing -> curves-only); User mode = user IR convolution. Controls: Mode, Top, Body, Reverb, Level.
Both persist `userIR` as an absolute path and reload via reloadConvIR() at setStateInformation; a vanished user IR silently becomes a 1-sample identity IR (finding - no MissingFileReport). loadUserIR clears the stored path if handed a nonexistent file. IR loads are message-thread/prepare only, never in process().

### MicSimDSP (Source/DSP/MicSimDSP.cpp)
Mic coloration for the NAMIR chain: model mode (named mic EQ fingerprints as peak-filter banks rebuilt on model change) or user-IR mode with two per-slot resident Convolutions (A/B switch is pointer-flip instant). loadUserIr uses the outErr+bool contract (missing file, or convolution throw); per-slot loaded flags are atomics read by the audio thread. Mode::None is a passthrough.

### SaturationDSP tape resources (Source/DSP/SaturationDSP.cpp, resource paths only)
Tape mode's cassette coloration convolves per-cassette IRs from `<exe>/Resources/Tape/IRs/cassette tape_<N>.wav`; a missing IR loads a unit impulse (passthrough) by design. Cassette hiss beds load from `Resources/Tape/Samples/cassette tape_<N>_noise.wav` (capped 2ch / 8M samples); if none load, hiss falls back to synthetic pink noise. Cassette switch reloads the IR only when the index changes; hiss switches by an audio-thread index read.

### BaySickPlayer SFZ engine + editor presets (Source/VibePlayer/)
VibePlayerDSP's SFZ parser: streams lines, tracks scope (<control>/<global>/<master>/<group>/<region>), honors default_path, resolves sample= relative to the SFZ dir, and accumulates opcodes into a per-scope defaults chain copied into each new region (inheritance). A region is only flushed into mRegions if it has audioData - so a missing sample file silently produces a dead key (finding). Supports key/lokey/hikey shorthand, root note, and the usual envelope opcodes.
VibePlayerEditor patch menu: Open Folder / Open SFZ / Open Sample (choosers rooted at the library), plus a recursive Core Library browser filtered to melodic packs (drum packs hidden outside drum context). loadPreset format: wrapper `BaySickPlayerState` containing the APVTS child (PARAM ids rewritten across track prefixes, e.g. tk_lay_0_bsp_ -> tk_bas_0_bsp_) + a `<Sample kind=\"sfz|file|folder\" path=\"library:rel | absolute\"/>` reference reloaded after state apply (drum context normalizes root to MIDI 60). onPatchLoaded renames the tab/strip to the patch name - it fires even when the sample reference didn't resolve (finding).

### Vox / Inst pages (Source/Vox/VoxPage.cpp, Source/Inst/InstPage.cpp)
Both follow the ClipsPage page-preset pattern (PagePresetIO kinds Vox/Inst, `mixer_vox_<N>` / Inst strip prefixes, dirty ValueTree listener with suppress-during-restore, Save-&-Delete chaining through savePagePreset's onSaved). Vox is always BaySickVocal (`bsv_` prefix) with a bus-activation fallback query for kVoxBus2. Inst is multi-source: LiveInput / BaySickGuitars / BaySickBasses; its preset config always includes Pedals + NAM/IR slots plus the active sfizz engine. Inst preset LOAD order matters: peek the saved Source mode and switch first, then peel the sfizz KitPath out of the base64 engine blob and run the race-safe loader to spawn the engine BEFORE building the import config (else the Sfizz slot is dropped and the tab arrives kitless); missing kit paths alert via PagePresetIO::importPagePreset; the chain is rebuilt again AFTER import so the sfizz stage splices in. Program switching (switchSfizzProgram): caches the outgoing program's APVTS under its filename, race-safe kit load, restores the incoming program's cached CC state by explicit setValueNotifyingHost walk (replaceState alone doesn't reliably fire listeners), refreshes the ARIA panel + pretty program-name linkage (tab/strip/roll rename, interactive picks only); wrapped in an undo transaction resolving the live page at undo time. The ARIA panel loads from Programs/<file>.sfz; kit label strips the NN- numeric prefix and title-cases.

### SlideRegionMap (Source/SlideSampler/SlideRegionMap.cpp)
Offline extraction of slide-articulation data from a sfizz program for the slide sampler: expands the #include chain (depth 6) into one line list, walks SFZ headers with full scope inheritance (region>group>master>global>control), buckets regions into articulations by sw_last (keyswitch-less programs land in the -1 bucket), classifies layers (unison cc100, tailpiece cc118, feedback-gated cc29, looped, release, mono-set cc105 choke policy), captures bend ranges from center sustain regions, custom <curve> tables, and keeps only default round-robin (seq_position 1). Missing sample wavs are counted and reported via DBG in Debug builds only. Compat mirror copies the default articulation's center set into the legacy fields.

### SampleLibrary stable refs (Source/SampleLibrary.cpp, adoption region)
makeStableRef/refForPersist encode files under known roots (Core Library, My Samples) as portable refs so persisted paths survive machine/user changes; resolvePersistedRef reverses. Adoption into My Samples: sfz files are excluded by design (their sample= web can't travel alone - the bundler reports them if missing); directories dedupe by name; files dedupe by size+modtime else auto-suffix \" (N)\"; failures return {} and callers fall back to project-copy import.

### MIDI learn persistence (Source/MidiLearn/MidiLearnRegistry.cpp)
Mappings serialize to a ValueTree (paramId, channel, cc, min/max, formula); global defaults live in Documents/BaySickDAW/MidiMappings.xml via saveAsGlobalDefaults/loadGlobalDefaults (bool contract; absent file on first launch = empty registry). onChanged fires after a tree load.

### Plugin hosting (Source/Hosting/)
PluginManager: background scan with progress/finish callbacks bridged to the message thread by AsyncUpdater; persists ScanFolders + KNOWNPLUGINS xml in its data file; on load, saved folders that no longer exist are dropped, and a saved-but-empty folder list is intentionally NOT re-seeded with defaults (user may have removed them). SandboxedPluginClient: out-of-process plugin bridge - helper exe (BaySickPluginHost32/64.exe beside the app binary, chosen by the plugin's architecture) launched as a ChildProcessCoordinator; handshake carries protocol version + host arch; LoadPlugin payload is path + identifier '\\n'-separated (v3 - the identifier alone contains no path). start() reports failures through outErr.

### Mp3Writer close path (Source/DSP/Mp3Writer.cpp)
LAME-based MP3 export: close() flushes the encoder tail, then re-opens the finished file with fopen(\"r+b\") to write the Xing/LAME header (only possible once stream length is known - without it the file plays but seek/duration are wrong). Returns the accumulated ok flag.

### PluginProcessor: freeze system, record takes, kit-load wrappers (regions read)
Freeze: per-instrument renders into <project>\\Freeze\\ - one song-scope file plus one per pattern the tab plays in; the Rusty kit renders 13 per-strip files per scope through onRenderKitFreezeFiles with a producer task kept live (the tap reads the engine while it renders). Reuse is stamp-gated: a freeze file is skipped only when its FNV content stamp matches AND the file exists; per-pattern reuse is independent so editing pattern 3 leaves 1/2/4 valid. Per-pattern render failures are deliberately non-fatal (tab stays frozen in song mode, plays live in that pattern); partial KIT open failures unwind completely (some-frozen-some-live would double the kit). Publish order is load-bearing: null the audio thread's frozen-source pointers, settle 20 ms if the tab was frozen, then replace stream storage; pattern streams map-INSERT only, never whole-map assign. Ruling 2-b: automatic freezes render song scope only; a staggered filler completes pattern coverage on quiet ticks.\nRecord takes: stopRecordingSession drains the pre-roll counter, stops the master recorder, then per-strip clears the vocal wet-recorder pointer BEFORE stopping the writer (audio-thread push race), and collects only files that exist into the result's strip/wet lists. Clip-block content signatures fold path hash + mtime + size (Regenerate De-noise rewrites in place - path/geometry hashing alone served stale audio) + start/length/stretch/BPM + the tempo timeline.

# Capture from finder: silent-skip-persistence
### MissingFileReport (Source/MissingFileReport.h)
The QA-Export mechanism for load-time missing external files. A thread-safe static store: restore sites call `MissingFileReport::add(what, path)` instead of skipping quietly; `VibeSynthProcessor::reportMissingFilesIfAny()` drains it ONCE at the end of every project/template load and shows a single dialog listing up to 12 entries ("The affected parts loaded WITHOUT them and will not make sound"). Dedupes identical (what, path) pairs so a kit shared by three tabs reports once. Current registered reporters: Guitars/Basses/Rusty kit paths (processor setStateInformation path only), NAM captures (NAMPedalStyleDSP), Mic A/B user IRs (NAMIR processor, both missing and failed-to-load), and Clip audio (editor restore walker). Known gaps found this sweep: the editor-side sfizz Inst restore and the Rusty editor-side restore bypass the reporting paths.

### ProjectManager (Source/ProjectManager.cpp)
Owns project lifecycle on disk. A project IS a folder under `Documents\BaySickDAW\Projects\<name>\` containing `project.xml`, `Samples\`, `Backups\`, and (regenerable) `Freeze\`. Name validation enforces Windows rules (illegal chars, reserved device names, 255 cap, trailing dot/space strip); collisions auto-suffix " (N)". `openProject` stops the transport via onBeforeOpenProject, parses project.xml, sets the folder BEFORE deserialize (path resolution needs it), replays state under mIgnoreDirty, then clears dirty (which also wipes undo history + snapshot store -- loads don't transact). `saveProject` serializes to project.xml via replaceWithText and pins the undo save-pointer (markSaved) on success. `saveProjectAs` copies the whole current folder (preserving Samples) or creates fresh, then rewrites project.xml from memory. Dirty state = transaction-pointer mismatch on TransactionTracker (QA-UndoCoverage Task 9); markDirty also feeds a monotonic change stamp consumed by version capture. Autosave: timer (default 15 min) fires writeBackup() regardless of dirty (FL convention) into `<project>\Backups\` or `Backups\Unsaved\` for unnamed sessions; filenames are `<stem>_backup_YYYY-MM-DD_HH-MM.xml` with same-minute " (N)" suffixing; rolling retention keeps newest 10. Restore from Backup parses the backup, preserves live project.xml as `project.xml.before-restore`, copies backup over live, replays in memory. Settings (`Documents\BaySickDAW\settings.xml`): recent projects (max 10), shortcutCreated / migratedFromRoaming one-shot flags, skip-prompt flags, defaultTemplate path; saveSettings preserves foreign XML sections it doesn't own. Sample import (`importSample`): stable-root files (Core Library / My Samples) are REFERENCED via stable refs, never copied; volatile sources copy into `<project>\Samples\` with size+modtime dedupe and " (N)" collision numbering; failures alert via ClipDropDiag. `duplicateSample` always makes a fresh numbered copy (Properties "Copy" action).

### Project XML format + restore walker (PluginProcessor.cpp + StandaloneEditor.cpp)
Root `<BaySickDAWProject version=1>` with children: `<Processor>` (APVTS tree incl. VibeRackStates -- every mixer strip's fader/pan/width/mute/solo/polarity/routing/sends plus per-insert rack + post-rack EQ), `<PatternManager>` (patterns, rolls, arrangement, libraries, mixer snapshot), `<DenoiseProfiles>` (QA-Fe2 per-recording-base raw/wet base64 pairs), `<UIState>` (editor-owned). Save side: writeProcessorState first MATERIALIZES tree nodes for lazy-registered params with their CURRENT value pre-set (the QA-Ef 100->-1 rebind-reset fix), then copyState minus/plus a fresh VibeRackStates. deserializeProject runs under the project-load shield (audio thread bails to silence; 30 ms drain sleep), tears down prior aux inserts, applies processor state (per-insert rack states are STASHED in mPendingProjectRackState and replayed later by applyPendingRackStates once InsertNodes exist; fixed-bus racks load immediately), restores PatternManager and denoise profiles, fires onDeserializeUIState into the editor, drops the shield, then drains MissingFileReport. UIState restore (StandaloneEditor::deserializeUIState): `<Windows>` map REPLACES session bounds (global-placement keys excluded -- those live in settings.xml), `<Open>` records carry pedals compact-view and visual tether lock (absent attr = locked), `<VisClosed>` = visuals user closed by hand; ControlLane prefs restore before tab rebuild. Then one pass over `<Tabs>/<Tab>` records (type/pageIndex/name/engine/engineData base64, plus per-type extras): Plugins (stash restore description from blob, selectPluginById, then state), Layers/Bass/Drum (create page at index, wire ~8 callbacks, selectEngine, applyEngineState), Clips (library re-tag matching for the relative path, MissingFileReport on missing audio, createClipStripAndPage), Vox (spawn + idempotent strip ensure), Inst (live-input vs sfizz-source branch: setSource, race-safe kit load with silent default fallback, sfizzEngineData replaceStateKeepingUndoHistory + force-fire, program cache, chain rebuild), BaySickRustyDrums (singleton: addTab, decode blob for KitPath, reloadForProjectRestore, saved CCs overlay kit defaults). Frozen-tab records collect into mPendingFreezes for restorePendingFreezes (runs after the whole graph exists; per-pattern content stamps allow render-free reuse). Post-pass: PianoRollSelection (exhaustive kind table), arrangement view/zoom/selection, mixer scroll, metronome (+precount legacy migration), VU calibration, meter latency comp (recomputed from live device), song loop, strip names/orders (renames-only persistence with idempotent ensure calls).

### Templates (StandaloneEditor.cpp)
Two loadable formats. v2 = the project shape minus arrangement (`<BaySickTemplate version=2>` wrapping `<Processor>` + `<UIState>`); loading is NEW-PROJECT semantics -- gate through confirmDiscardChanges, blank-state teardown mirroring doFileNew, then applyProcessorState + deserializeUIState + restoreAudioStripsFromArrangement, ending with the missing-files drain. v1 FACTORY = attribute schema (`<Kit path>` relative to Kits/Factory, `<Layer slot engine presetPath locked>` etc.); tears down Layers/Bass/Drums only and spawns via spawn*FromTemplate. Template save adopts volatile sample refs into My Samples (adoptTemplateSampleRefs rewrites clipPath, the BaySickPlayer bsp_loadPath inside the engine blob via decode->rewrite->re-encode, and Inst chain path attributes). Default template lives in settings.xml.

### PagePresetIO (Source/Standalone/PagePresetIO.cpp)
Whole-page presets under `Presets\<Kind> Page\My Presets\`. v2 format `<BaySickPagePreset version=2 pageType sourceMode?>` with wrappers `<Engines>` (per-slot label/rootTag/prefix/base64 data), `<Strips>` (per-strip natural-unit Param dump filtered by APVTS prefix), `<Racks>` (BusRack + InsertRack records from VibeGraph::saveRackStates filtered by page config). Import accepts three generations (v2, v1 single-engine, K-7 Aria roots) by synthesizing v2 wrappers over legacy shapes. Strip apply rewrites the saved prefix onto the destination (Layer 0 preset loads onto Layer 3) and remaps send destinations off inactive secondary buses back to natural parents. Sfizz engine slots restore via a race-safe kit-load callback (peel KitPath, load through the active-flag wrapper, then walk saved PARAMs with setValueNotifyingHost BEFORE replaceState so CC deltas actually dispatch); missing kit shows an inline alert and aborts. All preset param writes are ScopedProgrammaticParamWrites (never undo history). peekEngineType/peekSourceMode allow the page to swap engine/source before applying.

### EffectPresetIO (Source/Standalone/EffectPresetIO.cpp)
Per-effect presets under `Presets\Effects\<Type>\{Factory, My Presets}` (pedal types nest under `Effects\Pedals\`). File format `<EffectPreset version=1 effectType name blob>` wrapping the DSP's getStateInformation as base64. save/load/saveAsDefault all return bool + outErr with specific messages (the rack SlotComponent surfaces them; the pedals panel currently drops them). restoreDefaults = fresh DSP of the type -> capture -> setStateInformation. Factory seeding at boot: ~40 presets across Compressor/Reverb/Chorus/Delay/Saturation(+3 Tape)/Flanger/Overdrive/Phaser/TransientShaper/Limiter/DeEsser, versioned by `factory_seed_version.txt` (bump kFactorySeedVersion to force-rewrite Factory files on serializer changes; My Presets never touched); also pre-creates the full pedal folder tree + User NAM Pedals dir. One-shot Tape->Saturation migration moves user Tape presets and wipes the stale Tape Factory/Default.

### FxRackPresetIO (Source/Standalone/FxRackPresetIO.cpp)
Whole-rack-plus-EQ presets under `Presets\FX Rack\My Presets\`. `<BaySickFxRackPreset version=1>` with `<Rack data=base64>` (EffectRack::getStateInformation) and `<Eq prefix>` holding the strip's four EQ param families (mid/side x post/pre) stored as SUFFIX + natural value so a preset saved on one strip loads onto another. Load applies rack blob then EQ params (programmatic writes); errors surface through outErr at the EffectsPage caller.

### ProjectBundler (Source/Standalone/ProjectBundler.cpp)
Export Project Bundle (folder or zip). enumerate() walks three reference sources: the audio library, every arrangement block's audioFilePath, and (docket 21) engine-held references from the SAME tab serialization project save uses -- explicit path attribute allowlist (bsp_loadPath, kitPath, namPath, irPath, micUserIrPath, micbUserIrPath, plus tag-scoped generic "path" on KitPath/Sample), including decoding base64 engineData/sfizzEngineData blobs. Each ref classifies as ProjectRelative / UserSamples / CoreLibrary / Absolute / Missing; Core Library NEVER copies (docket 22=b -- any install has it), ProjectRelative is already inside, UserSamples+Absolute copy only in SelfContained scope. write() excludes the regenerable Freeze/ cache, lands copies under Samples/, reports Missing refs and errors in a Result the dialog shows (size estimate confirmed with the user before writing). Zip mode streams via ZipFile::Builder with abort support.

### VersionCapture (Source/Standalone/VersionCapture.cpp)
The take/versions system behind the Master Analyzer. Polled at kTimerHz off transport edge counters: a play-press ALWAYS opens a take; a loop wrap opens one only when the change stamp moved since the last kept take. Per tick (decimated to kHistoryHz) it records the short-term LUFS curve, max short-term, LRA accumulator, and spec violations (true peak + optional short-term ceiling) with tick-granular coalesced spans. Empty-curve takes (instant stop/scrub) are discarded along with their audio file. Optional audio per take via onBeginAudio/onEndAudio (the processor's master capture); retained-in-project mode writes audio into `<project>\Reports\` as "Take N - <timestamp>.wav" (timestamped because the take counter is session-scoped -- plain names collided ACROSS sessions and FileOutputStream appends, corrupting both) and persists each take's analysis as a standard loudness report via onPersistTake (report-write failures alert once per session). Session (non-retained) audio lives in a unique temp folder deleted on teardown; teardown order is callbacks-null -> endTake -> discardSessionAudio so the recorder closes before the folder delete.

### Offline render core + export (Source/Standalone/BuilderPage.cpp)
runOfflineLoop is the ONE render loop (QA-ModelShell TS2 rule: never copy it); consumers = export (renderToFile), measurement (measureRender), freeze (renderFreezeFile / renderKitFreezeFiles). It computes the content span in beats (Song = getSongEndBeats, Section = ruler selection, Pattern = getPatternContentBeats -- Pattern.bars is documented dead data), walks a lane-aware OfflineHead clock (tempo ruler map + song-scope global_tempo lane stepped blockwise, publishing bpm/ppq/timeInSeconds/timeInSamples -- the last is what frozen tracks read for file position), enters beginOfflineRender (suspend + setNonRealtime sweep + re-prepare; one render at a time), pins pattern-scope loop bounds to the pattern's own span (restoring the LIVE transport's values afterward), replays non-main-APVTS automation lanes per block, drives the LIVE processor's processBlock, and hands each block to a consumer callback whose false aborts with outErr. Tail::Included renders past content until 0.25 s below -100 dBFS or the ceiling; progress is throttled to 10 Hz (the overlay repaint was costing more than the render). The whole loop runs under ScopedProgrammaticParamWrites so exports leave undo history byte-identical. renderToFile opens all sinks BEFORE the offline drive: main mix (or a summed post-chain strip tap for consolidated/full-mix), plus one stem sink per ticked strip reading getStripOutputForTap per block in the same single pass (sends stay separate; sidechain-driven content stays in its stem). Sinks handle WAV (16/24-bit), Ogg (quality index), MP3 (LAME CBR), per-sink post-gain, and selectable TPDF dither at the 16-bit WAV boundary (flat, or 2nd-order noise-shaped with 1.0/-0.5 error feedback). Failure deletes all partials; success/failure surfaces through the FL-style ExportAudioDialog (persistent options box -> progress + live cancel; normalize = measure pass then gain-compensated render capped by the true-peak ceiling; Measure job writes no files by ruling and feeds the analyzer window).

### Freeze render path (BuilderPage.cpp + PluginProcessor.cpp)
renderFreezeFile writes 24-bit WAV at project rate into `<project>\Freeze\`, Tail::Cut (file length must equal the timeline span), capturing the insert's PRE-RACK signal via a single-arm freeze tap with a sequence-number stale-tap guard (idle/gap blocks clear instead of repeating the last buffer -- valid-WAV-wrong-sound was the failure). Dependency pruning skips unrelated tasks during the render. A tap that never fired FAILS LOUDLY and deletes the file (silence is indistinguishable from a quiet part). The kit freezes thirteen strips in ONE pass through the engine's strip views (no tap). freezeTab (processor) orchestrates: song render + one render per pattern the instrument plays in, per-pattern content stamps enabling render-free reuse on restore, per-pattern failures non-fatal (fall back to live for that pattern). Timing telemetry appends to freeze_timing.txt with setup/loop/teardown split. Manual freeze failures alert; auto-freeze and load-time restore failures fall back to live playback by documented design.

### Mp3Writer (Source/DSP/Mp3Writer.cpp)
Thin LAME front end, compiled out cleanly when BAYSICK_HAS_LAME is absent (open() then errors 'not available in this build' and the dialog hides MP3). CBR only, quality 2, JOINT_STEREO/MONO, Xing/VBR tag enabled. write() feeds planar -1..1 floats straight to lame_encode_buffer_ieee_float (JUCE's layout, no interleave); 0 encoded bytes = encoder buffering, not an error. close() flushes the encoder tail, then reopens the finished file with fopen r+b to back-patch the Xing header (seek/duration correctness) and returns false if the tail write failed.

### LoudnessReportWriter (Source/Standalone/LoudnessReportWriter.cpp)
Self-contained HTML loudness report + optional CSV into `<project>\Reports\`. HTML carries verdict badge (IN SPEC/OUT OF SPEC vs the resolved spec incl. a Custom target), summary table (integrated LUFS, true peak dBTP, LRA, max short-term/momentary, duration), an SVG loudness-over-time chart (-40..0 LUFS, gated silence not drawn, over-target orange bars, dashed target line), an optional spectrum snapshot (log-band average power, omitted rather than flat-lined when the render was shorter than one FFT), and the coalesced violations table. A machine-readable key=value block rides inside an HTML comment so readEmbedded() can reopen any report in the analyzer with no parser dependencies; verdicts are RECOMPUTED against the current spec table on reload rather than trusted from the file. write() reports partial success explicitly ('The HTML report was written, but the CSV could not be').

### PatternManager persistence (Source/PatternManager.cpp)
toValueTree/fromValueTree round-trips: global tempo, current pattern, the legacy group Mixer node (levels/pans/mutes/solos + CSV drum-slot and audio-row arrays), DrumEnabled bits, arrangement row mute/solo + names/groups/colors, Patterns (per-pattern name/bars/spb/intrinsic time-sig+lock+bound-marker-uid/color, drum step grids as bit strings, page sequencer data with basic+complex grids and envelopes, and all roll families: layer/bass/drumRolls/clip/vox/inst/plugin/Rusty singleton, with two documented legacy migrations -- old drumRoll midiNote->slotIndex decoding and legacy-drumRoll->drumRolls[] folding), the Arrangement (blocks with tick-precision start/length preferred over legacy float beats over bar ints, clip audio path/alias/pitch/BPM/stretch/mute/route/override/content-start, automation lanes with typed curve points and LFO config), time markers, and time-signature changes. Restore is defaults-tolerant per property for backward compatibility; notes use tick fields when present with beat-float fallback.

# Capture from finder: silent-skip-engines
### MissingFileReport (the load-time reporting mechanism)
`Source/MissingFileReport.h` is a header-only, lock-protected global collector (QA-Export Task 5, folded from QA-Verify). Engines that persist absolute/stable paths to external files call `MissingFileReport::add(what, path)` at restore time instead of silently skipping; duplicates (same what+path) collapse to one entry. `VibeSynthProcessor::reportMissingFilesIfAny()` (PluginProcessor.cpp:6413) drains once at the end of project load AND template load and shows a single dialog: "This project refers to files that are no longer where they were saved... The affected parts loaded WITHOUT them and will not make sound", listing up to 12 entries. Thread-safe because engine restores can run off the message thread. Current registered users: Guitars/Basses/Rusty setStateInformation (kit paths), NAMIR mic user IRs (both slots, missing AND failed-to-load), NAMPedal captures (missing only), StandaloneEditor clip audio (missing only, at deserialize). NOT wired: VibePlayer sample restore, the editor's own Inst/Rusty kit restore branches, NAMIR's primary NAM/cab IR loads.

### sfizz trio load architecture (BaySickGuitars / BaySickBasses / BaySickRustyDrums)
Each Inst tab whose Source is BaySickGuitars/Basses owns one processor instance keyed by pageIndex (`bgg_<idx>_` / `bbb_<idx>_` APVTS prefixes); Rusty is a singleton (`brd_` prefix). All three wrap a `sfz::Sfizz` with preload size 4096. **Load protocol (load-bearing):** the PluginProcessor wrappers `loadBaySickGuitarsKit/loadBaySickBassesKit(instIdx, sfzPath)` / `loadBaySickRustyDrumsKit(sfzPath)` are the ONLY safe entry points -- they drop the per-slot active flag AND the engine's `mProcessingEnabled` gate before `loadKit` (sfizz mutates internal hash maps for seconds; renderBlock against a half-parsed kit crashes), then flip both back and fire `onSfizzEngineReady`. Rusty additionally raises the project-load shield for the whole mutation window, tears down and respawns 13 mixer InsertNodes + render tasks from `discoverChannels`. Calling engine->setStateInformation directly re-runs loadKit WITHOUT this dance -- documented crash path; the editor's restore branches therefore decode the engine blob manually and replaceState the APVTS. `loadKit` itself: existsAsFile + loadSfzFile gate, then a depth-4 #include scan collecting `set_cc<N>` defaults (with `#define $macro` resolution) and `label_cc<N>` labels; resets ALL CCs to 0 (programmatic writes, not undo history), applies kit defaults through APVTS (listener dispatches to sfizz), rebuilds the SlideRegionMap + SlideSampler program (Guitars/Basses). Rusty's variant synthesizes an output-routed wrapper SFZ (`output=N` injected per piece) via loadSfzString, falling back to plain file load (kit then sums to one output). **Restore ordering rule:** kit first (stamps set_cc defaults), replaceState second (saved CCs win). **User-facing:** Inst program dropdown lists sibling .sfz files in the kit's Programs dir (tick on current); picking one is `switchSfizzProgramWithUndo` -- outgoing program's APVTS cached in a per-page session cache (`mProgramStateCache` keyed by file name), incoming program's cache force-fired param-by-param after the kit load. Rusty's program dropdown (Full/Basic) confirms with a mixer-reset/roll-clear warning; switch is one undoable structural transaction. Kit CC state is user-facing via the ARIA control panel (knobs bound to `<prefix>cc<N>` params; double-click reset returns the kit's set_cc default via getKitDefaultCc, unset CCs = 0 per the QA-Sfizz-Followup revert of the blanket-64). Cut-self params (cutSelf/cutSelfMode: Same Pitch vs Cut All) gate note-on choke behavior. Audition: packed note+velocity atomic exchanged in processBlock. RP Slide: CC84/5/37/86/85 transport intercepted to the blended SlideSampler; CC87/88 arm the native pitch-wheel bend using the kit's real bend_up/bend_down cents.

### Persistence shape for sfizz tabs (and the stable-ref split)
Engine blobs (`getStateInformation`) wrap the APVTS copy plus a `<KitPath path=...>` child. Rusty writes the path through `SampleLibrary::refForPersist` (so shipped kits persist as `library:<rel>` -- machine-independent); **Guitars/Basses still write raw absolute paths** (their setStateInformation reads them raw, consistent, but user-name-dependent). Tab records in the project XML (serializeProject ~13948): every page kind writes type/pageIndex/name/engine/engineData(base64 blob); Inst adds instChainState (Pedals+NAMIR via exportInstState), and for sfizz sources adds source, kitPath (refForPersist), sfizzEngineData, and a ProgramStateCache child. Rusty's record is just engineData. Restore paths: deserializeProject (~16560+) and resurrectTabFromRecord (~11990+, the undo/tab-resurrection twin) -- the twin got the 2026-08-06 resolvePersistedRef fix for Rusty's KitPath; the project-load twin did not (finding). Inst restore resolves kitPath via resolvePersistedRef and falls back to that source's default factory kit if missing (silent -- finding), then overlays sfizzEngineData APVTS manually, restores the program cache, registers piano roll, rebuilds the chain.

### BaySickRustyDrumsPage lifecycle
Page (singleton) owns: kit graphic (hitbox audition vs engine), ARIA panel (binding = apvts + ccParamId brd_cc<N> + kitDefault + label closures; hostTitleBar hosts the section tab row per Jeff 2026-08-06), program combo + player-preset button (mounted on the WINDOW title strip). `loadKit` = busy overlay + wrapper call + kit-graphic rebind + dirty-listener reattach + onKitLoaded (mixer strips) + onSoundNameChanged. `loadProgram` alerts on missing program SFZ (the one alerted path), tears down the prior program (detach dirty listener -> reset mixer APVTS -> clear ARIA/kit-graphic widgets BEFORE engine death -> destroyBaySickRustyDrums), loads, fires onProgramChanged AFTER mCurrentProgram updates (label ordering rule). `reloadForProjectRestore` maps file name -> Program enum (01-full/02-basic), loads via the page funnel, syncs combo so a same-program re-pick no-ops (set_cc stomp prevention), renders the ARIA panel; caller replaceStates saved CCs afterward. LIFE-02: re-adding the tab in-session auto-reloads the last kit. Player Presets: kit-CC-only overlay presets under Presets/Rusty Player/My Presets.

### Inst page (live-input + sfizz sources)
Chain per tab: [sfizz front stage when source is Guitars/Basses] -> Pedals -> NAM/IR; pages are views, engines resolve through the processor/rig at use time. Unified page preset (PagePresetIO v2) captures Source mode + engine slots (Pedals, NamIr, Sfizz) + `mixer_inst_<N>` strip params + insert rack + both EQs. loadPagePreset: peeks sourceMode, switches source BEFORE state apply, pre-spawns the sfizz engine by peeling KitPath out of the saved blob (because makeInstPresetConfig drops the Sfizz slot when the engine pointer is null -- documented trap), then importPagePreset, then notifySourceEngineChanged (chain resplice + ARIA rebind). Pedalboard presets (board-only, Presets/Pedalboards) save/load through BaySickPedalsProcessor with err-out strings (currently dropped by the page -- finding).

### PagePresetIO
One consolidated importer/exporter for all 8 page kinds. v2 root `BaySickPagePreset` with <Engines>/<Strips>/<Racks>; accepts v1 and K-7 Aria legacy shapes by synthesizing wrappers. Engine slots match by label; sfizz-backed slots (kitLoadCallback set) peel KitPath, ALERT on missing kit file and abort the import (returns false), call the race-safe wrapper, then force-fire each saved PARAM via setValueNotifyingHost before replaceState (listener-short-circuit workaround) -- all programmatic (no undo history). Non-sfizz slots use plain setStateInformation. Strip apply remaps saved prefix -> destination prefix and falls secondary buses (VoxBus2/InstBus2/3/group Bus2s) back to their parent when inactive. Rack apply feeds VibeGraph::loadRackStates; single-insert pages override the saved index with the destination.

### BaySickPlayer (VibePlayerProcessor + VibeSampleManager)
The sample-player engine behind Layers/Bass/Drum tabs (internally still VibePlayer*). Three load kinds -- folder (all wav/aiff/flac non-recursive, root note from filename heuristic), single file (root 60), SFZ (own 4-level-inheritance parser: control/global/master/group/region; supports key/lokey/hikey/pitch_keycenter/lovel/hivel/tune/volume/group/seq_position+length round-robin, and the Sub-L/N/O/Q keyswitch family sw_lokey..sw_up/sw_default/sw_label with scope-priority default resolution). Samples cap at 60 s; mono duplicated to stereo. `detectRootNote` parses note names then bare 0-127 numbers (the Kick_01 -> pitch-30x trap; drum wrappers pass normalizeRoot=60). Persistence: load kind/path/normalizeRoot stamped as non-PARAM properties on apvts.state (bsp_loadKind/bsp_loadPath/bsp_loadNormalize, path via refForPersist); setStateInformation replays the load if the resolved path exists (silently skips otherwise -- finding). No load API reports failure; regions vector empty = silence (finding).

### DrumPage sound loading
Per-tab engine choice (BaySickPlayer or BaySickSynth) via rig. Sample pickers route through the processor wrappers (path stamping). Sound presets (`BaySickPlayerState` XML with apvts child + <Sample kind path> where path may be `library:rel`): apply rewrites the trackId prefix (tk_..._bsp_) to the local page, replaceStates, reloads the sample if kind+path resolve, else leaves the slot empty by explicit comment while still renaming tab/strip to the preset (finding). Sound-swap gestures are one StructuralOpAction with before/after full-page-preset snapshots in UndoSnapshotStore files; undo of a load is itself a load.

### BaySickNAMIRProcessor (amp sim + cab)
Live-input amp/cab stage owned per Inst (and per Vox via BaySickVocal's embedded instance). Chain per block: pending-NAM swap adopt (wait-free, per slot) -> passthrough gate when no model AND no IR in either slot -> input gain -> noise gate (1 ms attack, knob release, pre-model by design) -> mono-sum -> NAM inference (direct or via 2x/4x polyphase oversampling; exceptions fill silence) -> broadcast to stereo -> HPF/LPF -> IR convolution with cab_mix wet/dry -> Mic A sim (10 built-in models or user IR) -> Mic A placement (distance/angle/polar) -> optional parallel Mic B branch (same controls, sums +6 dB on identical settings, 15 ms activation ramp) -> output gain. A/B slots: full tone snapshot per slot (SlotSnapshot: all knobs + mic modes/models + per-slot user IR paths), captured/applied on ab_slot changes via message-thread callAsync; both slots' NAM models and mic IRs stay resident for instant switching. Params: input_gain, output, gate_threshold/release, nam_bypass, cab_bypass, low_cut, high_cut, cab_mix, oversampling (1x/2x/4x; reResets models at OS rate, updates latency), ab_slot, full mic A/B sim + placement sets. Full-rig detection from NAM metadata gear_type (amp_cab/amp_pedal_cab -> editor suggests IR bypass). Persistence: APVTS + nam_filepath / ir_filepath (+_b) as string props + SlotA/SlotB snapshot children; restore reloads both slots' models/IRs (results currently only logged to namir_state_log.txt -- finding) then both slots' mic IRs (MissingFileReport-instrumented), then applies the active slot's snapshot. onStateRestored callback refreshes editor labels. loadImpulseResponse before prepare stores the path and returns false with 'will load on next prepare' -- but prepareToPlay never replays it (dormant-path caveat recorded in the NAMIR finding).

### NAMPedalStyleDSP (pedal-slot NAM)
Single-model pedal: input dB -> NAM (mono-sum, dual-mono out) -> 3-band shelving EQ (100 Hz / 1 kHz / 5 kHz) -> blend -> output trim. Swap-pending pattern for the model pointer, drained in process(). getModelName appends \" (missing)\" when mModelMissing (QA-Export honesty rule: never present a name that did not load). State: knobs + modelPath + bypassed; restore reports missing captures via MissingFileReport but drops parse failures (finding).

### BaySickPedalsProcessor (live-input pedalboard)
8 slots: 0 locked Tuner, 7 locked EQ family (Graphic/BassGraphic/FurmanEQ), 1-6 free (any effect except the locked types; Compressor defaults to CS type in a pedal slot, Overdrive to Pedal type). Per-slot bypass bools in APVTS (`bsp_slot<N>_bypass`). Slot mutation via per-slot swap-pending (audio wait-free); moveSlot shifts under both locks; audio try-locks and skips the block during multi-slot mutation. State: `BaySickPedalsRoot` (V2 tag; legacy `BaySickPedalsState` accepted -- load-tolerance, not migration) wrapping the APVTS copy + per-slot {index, type, uuid (automation identity), base64 DSP blob}. Restore builds DSPs off-thread then installs directly into active under lock (project-load fast path), re-fires onSlotsExternallyChanged (editor rebinds -- Bug B) and onSlotAutomationChanged (lane re-key), both marshalled. Pedalboard preset library under Presets/Pedalboards with descriptive outErr strings on every failure path.

### BaySickVocalProcessor (restore shape)
setStateInformation peels NamIrState / AlignEdits / VocalChainState / PitchEdits children out of the tree before replaceState (transient blobs must not enter APVTS), raises mRestoringState across the hook storm, migrates pre-QA-Fe pitch-algo picks to Rubber Band, restores per-slot vocal-chain DSP blobs IN PLACE on existing DSP instances (panel holds raw pointers), rebuilds align state (warp map, sync points, protected areas, render entries, version history) and pitch state ditto, re-baselines staleness generations, republishes align playback (sorted/monotone-clamped anchor SoA with Catmull tangents), restores the embedded per-Vox NAM/IR blob, fires onChainStateRestored. Interactive align/pitch operations all carry errorOut strings surfaced by their callers. Align/pitch render entries reference files in the project folder; their absence handling was not traced this sweep.

### AudioClipStreamer + clip players + freeze streams
AudioClipStreamer: constructed WITH an already-open AudioFormatReader (creation failure is the caller's problem); RAM-loads files whose float-PCM size fits kRamThresholdBytes, else a 4 s ring with a background TimeSliceThread prefetcher and 2 s synchronous prefill on seek. Audio-thread reads (readRaw / readAndMix with Catmull-Rom interpolation for fractional rates) return silence-and-request-seek on a window miss; offline renders (`sOfflineRender`) instead refill synchronously and blocking (ensureRangeBlockingForOffline) -- offline outruns prefetch by design. Underruns during continuous playback bump sUnderrunCount + jassert (Debug tripwire). Consumers that open readers: rebuildAudioClipPlayers (arrangement audio clips; null reader = clip dropped -- finding for the corrupt-file case), freeze streams (single-track + Rusty 13-strip + per-pattern; song-scope failure is fatal with outErr, per-pattern failure documented fall-back-to-live).

### SampleLibrary (stable refs + adoption)
Core Library at %LOCALAPPDATA%/BaySickDAW/CoreLibrary; My Samples under the app root (with a Core Library shortcut seeded in it). Stable refs: `library:<rel>` / `mysamples:<rel>` with forward slashes; refForPersist falls back to the absolute path for anything outside both roots; resolvePersistedRef inverts (raw absolute passes through). adoptIntoUserSamples copies loose user assets into My Samples on template save (size+modtime dedup, auto-suffix, dirs by name) but deliberately never adopts bare .sfz files (they are pointers into a sample tree; kept absolute and left to the bundler's missing-file reporting). scan() builds drum vs melodic pack lists by folder-name heuristic ('Drums'/'Percussion' substring).

### SlideRegionMap (RP Slide extraction)
`extractSlideRegions(programSfz)` re-parses the kit SFZ independently of sfizz (include-splice depth 6, header+opcode tokenizer with key= anchors, full scope inheritance, custom <curve> tables). Classifies regions per articulation (sw_last bucket; -1 for keyswitch-less programs) into center/tUp/tDown/tailpiece/feedback/noise/releases layers via locc discriminators, captures the cc105 mono-set choke policy, bend_up/bend_down cents at articulation level, default round-robin only, dedups per (layer, root, band). Missing sample files increment a Debug-only counter (DBG summary line); Release ignores it. Default articulation = sw_default's bucket else first with center content; compat mirror copies the default's center set + bend ranges to map.samples for the SlideSampler zone build.

### VST3 hosting chain
**PluginManager** (singleton, juce::Thread scanner): VST3-only format (deliberately not addDefaultFormats). plugins.xml at app root stores scan folders + the added list (KNOWNPLUGINS). Scan pipeline: pass 1 flags every .dll in nominated folders as 'VST2 not supported' (they are invisible to VST3PluginFormat, so this is the only way they surface); pass 2 splits candidates by PE-header architecture -- 32-bit plugins become minimal filename-guess descriptions ('32-bit, bridged' intake tier; corrected later via LoadReply -> refineDescription); pass 3 loads 64-bit ones via PluginDirectoryScanner with the dead-mans-pedal crash blacklist (plugins_scan_crashes.txt). Failures/blacklists/VST2s all land in the SkippedPlugin list with reasons (the reported baseline). **HostedPluginInstance**: wraps either an in-process juce plugin (mInner) or a SandboxedPluginClient (mSandbox). Bridge tiers: forced (32-bit -- architecture, not policy) and per-plugin preference (64-bit; applies on next load, i.e. at setStateInformation which re-instantiates when the saved preference differs). Failure states: HostedState Ok/NeedsBridge/Crashed/FailedToLoad with getStateMessage; the editor renders a dead marker (title + reason) whenever there is no live editor -- this is the designed report surface. Async bridge load results arrive via onLoadResult (used to be invisible -- fixed). processBlock: bridged deadline miss or dead helper = this slot silent, never a callback stall; unloaded instrument = silence, unloaded effect = passthrough. State: HostedPlugin XML wrapper carrying bridged flag + full PluginDescription + the plugin's own blob (base64) -- restores without consulting the added list. **SandboxedPluginClient**: launches BaySickPluginHost32/64.exe next to the app binary; handshake with protocol-version check BOTH directions; audio rides a named shared-memory block (fresh name per prepare, name travels with the Prepare message) with sequence-echoed request/done events and a 4 ms audio deadline; MIDI hand-serialized (cross-arch layout distrust); parameter list/programs/last-touched relayed over the pipe; getState bounded at 2 s (dead helper = empty state, documented); helper death -> handleConnectionLost -> onCrashed -> Crashed marker. **PluginHostMain** (helper): hosts exactly one plugin; loads by path (identifier disambiguates shell plugins, sole-description fallback for 32-bit filename guesses); replies Error + LoadReply(ok=0) on every failure; renders directly into the host's mapping on a TIME_CRITICAL thread; editor reparented into the host-supplied native window, real size replied via EditorOpened; quits when the coordinator connection drops.
