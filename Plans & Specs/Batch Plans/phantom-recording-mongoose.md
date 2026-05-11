# QA-E — Vox/Inst Lifecycle + Recording + DSP-09 + FILE-02 — Plan (phantom-recording-mongoose)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/phantom-recording-mongoose.md`
> Paired running notes: `Plans & Specs/Running Notes/phantom-recording-mongoose.md`

> **For execution:** steps use `- [ ]` checkbox syntax for tracking. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (per CLAUDE.md Build System standing rule).

## Context

QA-E is the consolidated Vox/Inst lifecycle + recording + DSP-09 + FILE-02 batch in the post-Batch-10 QA cycle (Phase 3). Pre-open corrective + scoping work landed in commit `54c99dd` 2026-05-11:

- §9 11th Forks entry routed QA-D NIT-1/2/3 here as Sub-Phase Z; NIT-4 routed to QA-Cleanup-1 (dead-code shape)
- §9 12th Forks entry expanded Sub-Phase A crash scope from 2 to 7 page-type branches in `showPageForTab`
- §9 13th Forks entry routed per-clip FilePlay sequential-vs-summed restructure into QA-J (OPT-A)
- FILE-01 wording corrected: not "browser bin" but "Vox wet+dry + Inst dry browser visibility via VoxPage dryClipPath + audioLibrary registration for every recorded file"
- New memory rule `feedback_closed_batch_carryforward_via_forks.md` locked

**Risk:** highest of any Phase 3 batch (multi-file, multi-callback, audio + project serialization + bus DSP). Per Main Plan §5 QA-E entry, "most likely to break unrelated paths."
**Effort estimate:** ~12-16 hours (bumped from ~10-14h at QA-E open per scope expansions; preset work moved to QA-Verify per R3B-i).
**Dependencies:** QA-0 (Composite RenderTask for FILE-02 stability) + QA-D (clean project-load baseline). Both landed.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| S1 | Bundled batch (not split into QA-E1/E2/E3) | All items touch MixerPage spawn cascade + StandaloneEditor XML restore + bus DSP. Splitting causes merge churn (Main Plan §5 QA-E entry). |
| S2 | Mute verify-and-close (M1) first; then Crashes → Lifecycle → FILE-01 → REC-01 verify → DSP-09 → FILE-02 → Sub-Phase Z → Close | Mute hopefully closes quickly (source review shows gates present); crashes block testing of everything else; lifecycle is the spine for FILE-01/REC-01/FILE-02. |
| S3 (memory rule) | 3c: new `feedback_closed_batch_carryforward_via_forks.md` + cross-ref on existing `feedback_qa_batches_fix_bugs_dont_defer.md` | Locked + saved 2026-05-11. |
| S4 (NIT placement) | 4a: Sub-Phase Z (Task 8) for NIT-1/2/3; NIT-4 → QA-Cleanup-1 (dead-code) | Locked 2026-05-11. |
| M1 (mute disposition) | Verify-and-close Task 1; clean → close-time §9 Forks routing as no-longer-reproducible; failed → M2 dig | Source review at QA-E open shows dispatch gates present at `PluginProcessor.cpp:1194-1195` / `:493-501` / `StandaloneEditor.cpp:2406`. |
| C-i (crash capture) | SafePointer-at-outer-scope across all 7 page-type branches | Matches existing convention in same file; JUCE-idiomatic; silent no-op vs crash. |
| R-1-c (BLU-470) | Documentation + verify + fix anything verify surfaces | Per `feedback_qa_batches_fix_bugs_dont_defer.md`. |
| R-2-a (Vox/Inst playback) | Subsumed by FILE-01 fix (no separate sub-task) | Browser registration + page-binding restores audio clip player's ability to find + play recorded files. |
| R-3-b-i (preset work) | All preset work moved to QA-Verify (BaySickPedals fix + all-engines audit) | QA-Verify already lists pedalboard preset bug in scope. |
| F1+F2 (FILE-01 shape) | Page-bound model (F2); VoxPage gains `dryClipPath` slot; `addAudioToLibrary` for every recorded file | Jeff's correction — pitch is just a consumer that reads off page bindings, not an owner. |
| F-3 (dry-as-first-class) | Vox dry behaves identically to wet for drag/drop/routing | Allows user to A/B wet vs dry vs BaySickPitch-generated variants. |
| 7a (page of origin) | Creator page (page that recorded the file) | Resolved via `block.routeChannel` set at recording finalize. |
| 7b (rebuild on change) | Re-route block's playback through new page's `InsertNode` (block stays on same row) | Surgical — change routing only, not grid position. |
| 7c (multi-recording) | Allow multiple clips per page; all pass through that page's chain | User can drop both wet + dry on grid + route both to same Vox page. |
| 7d (audition-on-hover) | None — click-to-commit only | Less surprising; matches standard popup UX. |
| Q2-A (FILE-02 placement) | Consolidate Routing INTO existing showAudioClipProperties popup; delete dead item 7; rename item 6 to "Properties..." | Cleaner UX — one entry point for all clip-level metadata. |

---

## No sub-spec calls open for ExitPlanMode

All spec calls were resolved in the conversation before plan-mode entry. If anything surfaces during execution that needs your call, I'll surface options + wait.

---

## Files to modify (per task)

### Task 1 — Mute verify-and-close (M1)
- **No source changes if Debug+Release verify passes.** Verify-only task.
- If verify fails → escalation to M2 expands scope; likely surfaces: [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) dispatch gates, [Source/Standalone/BuilderPage.cpp:4092](Source/Standalone/BuilderPage.cpp) LED handler, [Source/PatternManager.cpp:1432-1450](Source/PatternManager.cpp) `isRowMuted` / `isRowAudible`.

### Task 2 — Crash family SafePointer fix (Sub-Phase A)
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — `showPageForTab` function, 7 page-type branches:
  - LayersPage (4061-4096), BassPage (4097-4130), ClipsPage (4131-4172), VoxPage (4173-4208), InstPage (4209-4270), DrumPage (4271-4324), BaySickRustyDrumsPage (4325-4345)
- No header changes; no other files touched.

### Task 3 — Vox/Inst lifecycle (MIX-02/04/06)
- [Source/Standalone/MixerPage.cpp](Source/Standalone/MixerPage.cpp) — Vox/Inst spawn cascade (Carry-Forward §3 lines 187-194)
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — `onTabClosed` Vox/Inst branches (Carry-Forward §3 lines 196-201) + XML restore walker (lines 203-209)

### Task 4 — FILE-01 (Vox wet+dry + Inst dry browser visibility)
- [Source/Vox/VoxPage.h](Source/Vox/VoxPage.h) — add `mDryClipPath` field + `setDryClipPath` setter + `getDryClipPath` getter
- [Source/Standalone/StandaloneEditor.cpp:2255-2310](Source/Standalone/StandaloneEditor.cpp) — `onEnumerateAudio` Vox branch: emit two entries (wet + dry) per Vox page
- [Source/Standalone/StandaloneEditor.cpp:9842-9866](Source/Standalone/StandaloneEditor.cpp) — recording finalize: register all recorded files in `audioLibrary`; bind Vox dry to VoxPage

### Task 5 — REC-01 R-1-c (BLU-470 doc+verify+fix)
- [Plans & Specs/Carry-Forward Reference.md](Plans & Specs/Carry-Forward Reference.md) — add §3 sub-section documenting recording lifecycle
- Source files: only if verify surfaces actual bugs (likely none after Task 4 lands)

### Task 6 — DSP-09 (Bus solo)
- [Source/VibeGraph.cpp](Source/VibeGraph.cpp) — bus solo logic (Carry-Forward §3 lines 274-287 reference)
- Possibly [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — if solo dispatch lives there

### Task 7 — FILE-02 (Properties dialog consolidation + Routing dropdown)
- [Source/Standalone/BuilderPage.cpp:2543-2561](Source/Standalone/BuilderPage.cpp) — `showClipContextMenu`: delete dead item 7, rename item 6 label
- [Source/Standalone/BuilderPage.cpp:2625+](Source/Standalone/BuilderPage.cpp) — `showAudioClipProperties`: add Routing dropdown UI
- [Source/PluginProcessor.cpp:3209-3214](Source/PluginProcessor.cpp) — `rebuildAudioClipPlayers` already handles `routeChannel` correctly; verify no additional wiring needed
- [Source/PatternManager.h](Source/PatternManager.h) — `ArrangementBlock::routeChannel` field already exists (line 471)

### Task 8 — Sub-Phase Z (QA-D NIT corrections)
- [Source/Standalone/StandaloneEditor.cpp:1263-1315](Source/Standalone/StandaloneEditor.cpp) — `onTabRenamed`: add BaySickRustyDrumsPage branch (NIT-1)
- [Source/Standalone/StandaloneEditor.h](Source/Standalone/StandaloneEditor.h) — `restoreAudioStripsFromArrangement` signature: add `bool isLoadContext` parameter (NIT-2)
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — `restoreAudioStripsFromArrangement` body: gate `clearDirty()` on `isLoadContext`; `advanceCountersFromRestoredTabs`: extend parser for legacy bare names (NIT-3)

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/luminous-kindling-horizon.md` → `Plans & Specs/Batch Plans/phantom-recording-mongoose.md` (Write tool); delete the home-dir copy per `feedback_plan_mirror_one_way.md`.
- [ ] Add `**Plan file:** [Batch Plans/phantom-recording-mongoose.md](Batch Plans/phantom-recording-mongoose.md)` line to §5 QA-E header in Main Plan.
- [ ] Seed `Plans & Specs/Running Notes/phantom-recording-mongoose.md` with header + initial "Task 0: open" entry.
- [ ] Surface full git status. Dispatch `/draft-commit`. Surface drafted message + git status to Jeff for approval. Commit on approval.
- [ ] Dispatch `/draft-doc running-notes` post-commit and apply.

### Task 1 — Mute Findings Verify-and-Close (M1)

Source review at QA-E open confirms dispatch gates are present:
- Pattern dispatch: [Source/PluginProcessor.cpp:1194-1195](Source/PluginProcessor.cpp) — `if (blk.muted) continue; if (!isRowAudible(blk.trackRow)) continue;`
- Audio rendering: [Source/PluginProcessor.cpp:493-501](Source/PluginProcessor.cpp) — `if (rowMuted || builderRowMuted) continue;`
- Block mute: [Source/Standalone/StandaloneEditor.cpp:2406](Source/Standalone/StandaloneEditor.cpp) — `if (block.muted) continue;`
- LED click: [Source/Standalone/BuilderPage.cpp:4092](Source/Standalone/BuilderPage.cpp) — `mPM.setRowMuted(row, !mPM.isRowMuted(row));`

`git log -L` confirms these date from `cc011e0` (MT engine batches) + initial commit — both predate QA-0's finding capture. So either the original findings were captured inaccurately, OR an unrelated commit since incidentally fixed them.

- [ ] Tell Jeff: "Run `do_build.bat`. Test sequence in Debug:
  - **(1) Pattern row mute via LED.** Add a Layers tab. Open the piano roll, paint a few pattern notes. Place a pattern block on a Builder grid row. Hit Play. Click the mute LED in the row header. Pattern should silence. Click again. Pattern resumes.
  - **(2) Audio row mute via LED.** Drop an audio clip on a different Builder grid row. Hit Play. Click the mute LED in that row's header. Audio should silence. Click again. Audio resumes (NO sticking).
  - **(3) Pattern block right-click mute.** Right-click a pattern block on the grid → Mute. Pattern should silence. Right-click → Unmute. Pattern resumes.
  - **(4) Audio block right-click mute.** Right-click an audio block on the grid → Mute. Audio should silence. Right-click → Unmute. Audio resumes.
  - All four scenarios pass in Debug? Repeat in Release for confirmation."
- [ ] Wait for Jeff's verify result.
- [ ] **If all four scenarios pass clean:** add to close-entry routing table (or §9 14th Forks entry if scope warrants) — "#16a/#16b/#21 no-longer-reproducible at QA-E open verify; source review shows gates present at PluginProcessor.cpp:1194-1195 / :493-501 / StandaloneEditor.cpp:2406; gates date from cc011e0 (MT engine batches) + initial commit — predate finding capture; root cause of original captures unidentified but moot." No source commit.
- [ ] **If any scenario fails:** escalate to M2. Diagnose which gate is missing/broken. Surface root cause to Jeff with options. Fix in-batch.
- [ ] Dispatch `/draft-doc running-notes` post-verify and apply.

### Task 2 — Crash Family SafePointer Fix (Sub-Phase A)

**Pattern (applies uniformly to all 7 page-type branches in `showPageForTab`):**

```cpp
// BEFORE — example from LayersPage branch (StandaloneEditor.cpp:4061):
else if (auto* lp = dynamic_cast<LayersPage*>(mVisiblePage))
{
    auto syncPagePresetMenu = [this, lp] (int /*subTabIdx*/)
    {
        if (! mPageMenuBar || lp == nullptr) return;
        juce::Component::SafePointer<LayersPage> safe (lp);    // <-- TOO LATE
        mPageMenuBar->setMenuBuilder (
            [safe] (juce::Component* anchor)
            {
                if (auto* p = safe.getComponent())
                    p->showPageActionsMenu (anchor);
            });
    };

    mPageMenuBar->setTabSlots({"Player", "Piano Roll"},
        [this, lp, syncPagePresetMenu](int i) {                // <-- raw lp captured
            if (i == 1) { ...; mPianoRollPage->selectEngine ({ EngineKind::Layer, lp->getPageIndex() }); return; }
            lp->switchTab(i);                                  // <-- crashes if lp dangles
            mPageMenuBar->updateTabActive(i);
            mPageMenuBar->setMidSideVisible(false);
            syncPagePresetMenu (i);
        }, lp->getActiveTab(), lp->getPageColor());
    syncPagePresetMenu (lp->getActiveTab());
    mPageMenuBar->setMidSideVisible(false);
}

// AFTER — same branch, SafePointer lifted to outer scope:
else if (auto* lp = dynamic_cast<LayersPage*>(mVisiblePage))
{
    juce::Component::SafePointer<LayersPage> safe (lp);        // <-- LIFTED to outer scope

    auto syncPagePresetMenu = [this, safe] (int /*subTabIdx*/)
    {
        if (! mPageMenuBar) return;
        if (auto* p = safe.getComponent())
        {
            mPageMenuBar->setMenuBuilder (
                [safe] (juce::Component* anchor)
                {
                    if (auto* pp = safe.getComponent())
                        pp->showPageActionsMenu (anchor);
                });
        }
    };

    mPageMenuBar->setTabSlots({"Player", "Piano Roll"},
        [this, safe, syncPagePresetMenu](int i) {
            if (auto* p = safe.getComponent())
            {
                if (i == 1)
                {
                    if (mRibbon) mRibbon->selectTab (4);
                    onTabSelected (4);
                    if (mPianoRollPage)
                        mPianoRollPage->selectEngine ({ EngineKind::Layer, p->getPageIndex() });
                    return;
                }
                p->switchTab(i);
                mPageMenuBar->updateTabActive(i);
                mPageMenuBar->setMidSideVisible(false);
                syncPagePresetMenu (i);
            }
        }, lp->getActiveTab(), lp->getPageColor());            // setup-time still uses lp (alive here)
    syncPagePresetMenu (lp->getActiveTab());
    mPageMenuBar->setMidSideVisible(false);
}
```

**Apply the same pattern to all 7 branches:**

| Branch | Outer-scope SafePointer line |
|---|---|
| LayersPage (4061) | `juce::Component::SafePointer<LayersPage> safe (lp);` |
| BassPage (4097) | `juce::Component::SafePointer<BassPage> safe (bp);` |
| ClipsPage (4131) | `juce::Component::SafePointer<ClipsPage> safe (cp);` |
| VoxPage (4173) | `juce::Component::SafePointer<VoxPage> safe (vp);` |
| InstPage (4209) | `juce::Component::SafePointer<InstPage> safe (ip);` |
| DrumPage (4271) | `juce::Component::SafePointer<DrumPage> safe (dp);` |
| BaySickRustyDrumsPage (4325) | `juce::Component::SafePointer<BaySickRustyDrumsPage> safe (rp);` |

For the 6 branches that have an "inner SafePointer-too-late" line (Layers 4070 / Bass 4104 / Clips 4144 / Vox 4185 / Inst 4219 / Drum 4287), **delete that line** — the outer-scope SafePointer replaces it.

For BaySickRustyDrumsPage (no current inner SafePointer line), the outer-scope construction is purely additive.

**Steps:**
- [ ] LayersPage branch: lift SafePointer; replace raw `lp` captures with `safe`; delete inner-SafePointer line 4070; use `safe.getComponent()` inside lambdas.
- [ ] BassPage branch: same treatment.
- [ ] ClipsPage branch: same treatment.
- [ ] VoxPage branch: same treatment.
- [ ] InstPage branch: same treatment (note this branch has an extra `labels` capture; preserve it).
- [ ] DrumPage branch: same treatment.
- [ ] BaySickRustyDrumsPage branch: same treatment (no inner-SafePointer line to delete).
- [ ] Tell Jeff: "Run `do_build.bat`. Test sequence in Debug:
  - **(1) Original repro — DrumPage Drum Kit sub-tab.** Open a Drums tab with the 16-sound-picker DrumPage. Engine-swap it (delete + add new), OR delete the tab + add a new Drums tab, OR File → Open another project. Then click the 'Drum Kit' sub-tab on the new DrumPage's menu bar. Previously crashed; should now no-op silently (lambdas detect the original page is gone).
  - **(2) Layers sub-tab Piano Roll.** Repeat (1) with a Layers tab → engine swap → click 'Piano Roll' sub-tab. Should no-op cleanly (no crash).
  - **(3) Bass sub-tab Piano Roll.** Same with Bass tab.
  - **(4) Vox / Inst / Clips / Rusty sub-tabs.** Quick spot-check on each — engine swap or project reload, then click a sub-tab. None should crash.
  - **(5) Normal operation still works.** Without any destruction, click sub-tabs on every page type — navigation still works normally (Piano Roll button navigates, Player switches sub-tab, etc.)."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit`, surface drafted message + git status, commit on approval.
- [ ] Dispatch `/draft-doc running-notes` and apply.

### Task 3 — Vox/Inst Lifecycle (MIX-02/04/06)

**Investigation phase first** — surface in source how the spawn cascade + XML restore currently behaves, then fix.

- [ ] Read Carry-Forward Reference §2 + §3 (lifecycle primitives + file:line index). Specifically: `closeAllDynamicTabs` ([§2 lines 138-149](Plans & Specs/Carry-Forward Reference.md)), `mProjectLoadInProgress` ([§2 lines 151-155](Plans & Specs/Carry-Forward Reference.md)), spawn cascade ([§3 lines 187-194](Plans & Specs/Carry-Forward Reference.md)), tab close dispatch ([§3 lines 196-201](Plans & Specs/Carry-Forward Reference.md)), XML restore walker ([§3 lines 203-209](Plans & Specs/Carry-Forward Reference.md)).
- [ ] Read `MixerPage.cpp` spawn cascade for Vox + Inst paths.
- [ ] Read `StandaloneEditor.cpp` `onTabClosed` Vox + Inst branches + XML restore walker.
- [ ] Diagnose: why does the strip disappear on reload but not on initial spawn? Likely candidates:
  - Strip widget exists in `mVoxStrips` / `mInstStrips` vector but isn't attached to MixerPage's component tree after deserialize
  - Spawn cascade fires on initial add but not on XML restore path
  - Phantom strips = orphan `mixer_vox_N_*` / `mixer_inst_N_*` APVTS params register strips that the XML restore walker doesn't clean up
- [ ] Surface root cause to Jeff with proposed fix shape before implementing.
- [ ] Implement fix (concrete shape TBD per investigation outcome).
- [ ] Tell Jeff: "Run `do_build.bat`. Test sequence in Debug — full Vox lifecycle walk:
  - **(1) Create Vox.** Add a Vox tab via the ribbon + button. Verify Vox strip appears in the mixer.
  - **(2) Record.** Arm the Vox strip. Press Record + Play. Sing/play into the mic. Press Stop. Verify a WAV file lands in `<project>/Samples/`.
  - **(3) Save.** File → Save (or Ctrl+S).
  - **(4) Close tab.** Right-click Vox ribbon tab → Close. Verify strip disappears. Verify recording file stays on disk (no-file-delete contract).
  - **(5) Reopen Vox.** Add Vox tab again. Verify strip respawns clean.
  - **(6) Save + Close project + Reopen project.** File → Save, then File → Open the saved project. Verify Vox tab respawns, mixer strip exists, recording binding intact (drag the recording from browser back onto grid — plays through Vox chain).
  - **(7) Delete tab + verify no phantom strip.** Right-click Vox tab → Delete. Verify mixer strip removed cleanly + no leftover phantom strip in mixer view.
  - **(8) Repeat (1)-(7) for Inst.** Same lifecycle for the Inst path."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 4 — FILE-01 (Vox wet+dry + Inst dry browser visibility)

**VoxPage extension:**

```cpp
// Source/Vox/VoxPage.h — additions inside the class declaration:
public:
    void  setDryClipPath (const juce::String& path) { mDryClipPath = path; }
    const juce::String& getDryClipPath() const     { return mDryClipPath; }
    // existing getClipFilePath() unchanged — still returns the WET path

private:
    juce::String mDryClipPath;
```

**Recording finalize at [Source/Standalone/StandaloneEditor.cpp:9842-9866](Source/Standalone/StandaloneEditor.cpp):**

```cpp
// BEFORE — Vox branch (around line 9844):
for (const auto& [chId, dryFile] : res.stripFiles)
{
    if (isVoxCh (chId))
    {
        const juce::File wetFile = findWet (chId);
        const juce::String dryRel = "Samples/" + dryFile.getFileName();
        mPM->addAudioToLibrary (dryRel);                          // DRY only
        if (wetFile.existsAsFile())
            dropWavAsClip (wetFile, chId);                        // WET on grid
        else
            dropWavAsClip (dryFile, chId);
    }
    else if (isInstCh (chId))
    {
        dropWavAsClip (dryFile, chId);                            // DRY on grid (no library add)
    }
    ...
}

// AFTER — Vox + Inst both register all files in library; Vox dry binds to page:
for (const auto& [chId, dryFile] : res.stripFiles)
{
    if (isVoxCh (chId))
    {
        const juce::File wetFile = findWet (chId);
        const juce::String dryRel = "Samples/" + dryFile.getFileName();

        // Register DRY in library (existing).
        mPM->addAudioToLibrary (dryRel);

        // NEW: register WET in library so browser walk's findLibIdx succeeds.
        if (wetFile.existsAsFile())
        {
            const juce::String wetRel = "Samples/" + wetFile.getFileName();
            mPM->addAudioToLibrary (wetRel);
        }

        if (wetFile.existsAsFile())
            dropWavAsClip (wetFile, chId);                        // WET on grid + binds to Vox page via routeChannel
        else
            dropWavAsClip (dryFile, chId);                        // fallback

        // NEW: bind DRY to the Vox page so browser walk emits a dry entry.
        const int voxIdx = chId - MixerChannelIds::kVoxBase;
        if (voxIdx >= 0 && voxIdx < kMaxVoxPages)
        {
            // Walk mPages to find the Vox page bound to this channel id and stamp dry path.
            for (auto* entry : mPages)
            {
                if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
                {
                    if (vp->getPageIndex() == voxIdx)
                    {
                        vp->setDryClipPath (dryFile.getFullPathName());
                        break;
                    }
                }
            }
        }
    }
    else if (isInstCh (chId))
    {
        // NEW: register Inst dry in library.
        const juce::String dryRel = "Samples/" + dryFile.getFileName();
        mPM->addAudioToLibrary (dryRel);

        dropWavAsClip (dryFile, chId);
    }
    ...
}
```

**Browser walk at [Source/Standalone/StandaloneEditor.cpp:2276-2292](Source/Standalone/StandaloneEditor.cpp):**

```cpp
// BEFORE — Vox branch emits one entry per Vox page:
else if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
{
    const juce::String path = vp->getClipFilePath();
    if (path.isEmpty()) continue;   // empty until G-9 records
    const int libIdx = findLibIdx (path);
    if (libIdx < 0) continue;
    CategorizedAudioEntry e;
    e.audioLibIdx = libIdx;
    e.category    = "Vox";
    e.displayName = vp->getTabName().isNotEmpty()
                       ? vp->getTabName()
                       : juce::File (path).getFileName();
    ...
    out.push_back (std::move (e));
}

// AFTER — emit one entry per non-empty page-bound path (wet + dry separately):
else if (auto* vp = dynamic_cast<VoxPage*> (entry->component.get()))
{
    // Helper lambda to emit one entry for a path, if registered + non-empty.
    auto emitEntry = [&](const juce::String& path)
    {
        if (path.isEmpty()) return;
        const int libIdx = findLibIdx (path);
        if (libIdx < 0) return;
        CategorizedAudioEntry e;
        e.audioLibIdx = libIdx;
        e.category    = "Vox";
        // Display: alias-if-set, else filename (filename carries -DRY/-WET suffix per record-time convention).
        e.displayName = mPM->getAudioLibraryAlias (libIdx).isNotEmpty()
                           ? mPM->getAudioLibraryAlias (libIdx)
                           : juce::File (path).getFileName();
        e.fullPath    = mProcessor.resolveProjectFile (path).getFullPathName();
        if (e.fullPath.isEmpty()) e.fullPath = path;
        e.accent      = juce::Colour (0xff0fafa5);   // Vox teal
        out.push_back (std::move (e));
    };

    emitEntry (vp->getClipFilePath());      // WET (existing path)
    emitEntry (vp->getDryClipPath());       // NEW — DRY
}
```

**Inst branch:** unchanged (still emits one entry via existing `ip->getClipFilePath()`). The library registration fix at finalize makes `findLibIdx` succeed for the existing entry.

**Steps:**
- [ ] Add `mDryClipPath` field + `setDryClipPath` + `getDryClipPath` to VoxPage.h.
- [ ] Modify recording finalize at StandaloneEditor.cpp:9842-9866 per the AFTER snippet above (Vox + Inst both `addAudioToLibrary`; Vox dry binds to page).
- [ ] Rewrite Vox branch of browser walk at StandaloneEditor.cpp:2276-2292 per the AFTER snippet.
- [ ] Verify `dropWavAsClip` doesn't already call `addAudioToLibrary` internally (audit the function). If it does, the explicit `addAudioToLibrary` calls in finalize are redundant; remove them. If it doesn't, the additions stand.
- [ ] Tell Jeff: "Run `do_build.bat`. Test sequence in Debug:
  - **(1) Record Vox.** Add a Vox tab. Arm it. Record + Play + sing/play + Stop. Open the Audio browser tab on the Builder page. Expand the Vox category. You should see TWO entries: one with `-WET.wav` suffix and one with `-DRY.wav` suffix.
  - **(2) Drag wet to grid.** Drag the WET entry onto a Builder grid row. Hit Play. WET plays through the Vox page's signal chain.
  - **(3) Drag dry to grid.** Drag the DRY entry onto a different Builder grid row. Hit Play. DRY also plays through the Vox page's chain.
  - **(4) Both on grid simultaneously, same Vox page.** Drop both wet + dry onto grid rows both routed to the same Vox page. Both pass through the chain (note: sequential per-clip processing is a known quirk routed to QA-J — that's OK for now).
  - **(5) Delete one from grid.** Right-click the wet block → Delete. WET disappears from grid but stays in browser (no-file-delete contract).
  - **(6) Rename in browser.** Right-click WET entry in browser → Rename → 'My Vocal Take'. WET entry now displays 'My Vocal Take'. DRY entry still displays its filename.
  - **(7) Record Inst.** Add an Inst tab. Arm. Record. Stop. Open browser → Inst category should show one entry (dry only, since Inst is dry-only by current design).
  - **(8) Save + reload project.** File → Save. File → Open same project. Verify Vox wet + dry entries still appear in browser; rename persists."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 5 — REC-01 R-1-c (BLU-470 documentation + verify + fix)

**Doc deliverable** — add to `Plans & Specs/Carry-Forward Reference.md` §3 (after the existing recording-related entries):

```markdown
### Recording lifecycle (per-armed-strip WAV capture)

**StripRecorder** ([Source/PluginProcessor.h:645-658](Source/PluginProcessor.h:645))
- One per armed Vox/Inst strip; per `_arm` APVTS flag scan in `startRecording` ([Source/PluginProcessor.cpp:3478-3521](Source/PluginProcessor.cpp:3478)).
- Vox: dry writer (raw pre-chain ASIO input) + wet writer (post-realtime-pitch BaySickVocalProcessor tap).
- Inst: dry writer only (no realtime stage to bake into wet capture).

**Tap helpers** ([Source/PluginProcessor.cpp:3572-3590](Source/PluginProcessor.cpp:3572))
- `tapDryRecorder(channelId, monoSource, numSamples)` writes raw mono into the dry file.
- Called from serial path in processBlock + (under MT flag) from VoxStripTask / InstStripTask.

**Finalize** ([Source/Standalone/StandaloneEditor.cpp:9842-9866](Source/Standalone/StandaloneEditor.cpp:9842))
- `stopRecording()` returns `RecordResult` with stripFiles (dry) + stripWetFiles (wet, Vox only) maps.
- For each Vox strip: register wet + dry in audioLibrary; drop wet onto grid linked to Vox page; bind dry to VoxPage via `setDryClipPath` (post-FILE-01).
- For each Inst strip: register dry in audioLibrary; drop dry onto grid linked to Inst page.
- Both wet + dry surface in the audio browser via the page-walk enumerator at [Source/Standalone/StandaloneEditor.cpp:2255-2310](Source/Standalone/StandaloneEditor.cpp:2255).

**File naming convention** ([Source/PluginProcessor.cpp:3490-3501](Source/PluginProcessor.cpp:3490))
- Dry: `<project> - <Vox|Inst> N - <ts> - DRY.wav`
- Wet: `<project> - <Vox|Inst> N - <ts> - WET.wav` (Vox only)
- Filename suffix is the wet/dry tag; browser display falls back to filename when no alias is set.
```

**Verify:** end-to-end recording lifecycle test on Vox + Inst.

- [ ] Edit Carry-Forward Reference.md per the doc deliverable above.
- [ ] Tell Jeff: "Run `do_build.bat`. Test sequence in Debug:
  - **(1) Full Vox record cycle.** Add Vox tab. Arm. Press Play → Record. Sing for 5 seconds. Stop. Verify two files on disk in `<project>/Samples/` — one with `-DRY.wav` and one with `-WET.wav`. Verify both appear in browser Vox category.
  - **(2) Full Inst record cycle.** Add Inst tab. Arm. Press Play → Record. Play guitar for 5 seconds. Stop. Verify one file on disk with `-DRY.wav`. Verify it appears in browser Inst category.
  - **(3) Master-mix fallback.** Without arming any strip, Press Play → Record (audio mode). Stop. Verify one master-output WAV in `<project>/Samples/`. Verify it's NOT in Vox/Inst browser categories (it goes to Clips or stays as a raw library entry — confirm current behavior).
  - **(4) Per-track arm.** Arm Vox 1 + Inst 1 simultaneously. Press Play → Record. Stop. Verify TWO sets of files (Vox dry+wet + Inst dry).
  - **(5) Debug pops.** Listen carefully on each start/stop cycle. No clicks, no pops, no glitches. If any audible artifact, note when (start, stop, mid-record)."
- [ ] Wait for Jeff's verify result. Document findings in running notes.
- [ ] **Fix any bugs verify surfaces** (per `feedback_qa_batches_fix_bugs_dont_defer.md`). Surface root cause + fix to Jeff before implementing.
- [ ] On pass (or after fixes): `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 6 — DSP-09 (Bus solo)

**Pre-task spec call** — Carry-Forward §3 + §4 lock target behavior. Read them, then surface any still-open sub-calls to Jeff.

- [ ] Read Carry-Forward Reference §3 "Bus solo" ([lines 274-287](Plans & Specs/Carry-Forward Reference.md)) + §4 Decisions Already Made for the locked DSP-09 target behavior.
- [ ] Read [Source/VibeGraph.cpp](Source/VibeGraph.cpp) bus-solo dispatch logic + `processBus` for the existing per-strip `_solo` APVTS handling.
- [ ] Surface any still-open sub-calls to Jeff (e.g. solo + mute interaction, solo persistence across project save/load, solo-of-multiple-buses semantics — confirm or override defaults).
- [ ] Implement bus-solo dispatch per locked target behavior. Likely shape: at master mix stage, if any bus has `_solo` set, mute all bus outputs except the soloed ones. Per-strip `_solo` already handled at the per-insert stage.
- [ ] Tell Jeff: "Run `do_build.bat`. Test sequence in Debug:
  - **(1) Solo Layers bus.** Add tracks: a Layer with a pattern playing, a Bass with a pattern, a Drum with a pattern. Hit Play. Verify all three play. Click Solo on the Layers bus strip in the mixer. Only Layers audible at master output; Bass + Drums silent.
  - **(2) Unsolo.** Click Solo again on Layers bus. All three audible.
  - **(3) Multi-bus solo.** Click Solo on Layers AND Bass buses. Both audible at master; Drums silent.
  - **(4) Solo + mute interaction.** Solo Layers bus, then click Mute on the Layers bus strip. Master should be silent (mute overrides solo? or solo overrides mute? per locked behavior).
  - **(5) Persistence.** Solo Layers bus. File → Save. File → Open project. Solo state persists in the mixer; Layers is still soloed."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 7 — FILE-02 (Properties dialog consolidation + Routing dropdown)

**Step 1: clean up the right-click menu at [Source/Standalone/BuilderPage.cpp:2557-2561](Source/Standalone/BuilderPage.cpp):**

```cpp
// BEFORE:
if (b.clipType == ClipType::Audio)
    m.addItem(6, "Properties (pitch / stretch)...");
if (b.clipType == ClipType::Automation)
    m.addItem(8, "Open in Event Editor...");
m.addItem(7, "Properties...");                          // dead — no handler in switch

// AFTER:
if (b.clipType == ClipType::Audio)
    m.addItem(6, "Properties...");                      // renamed; now hosts routing too
if (b.clipType == ClipType::Automation)
    m.addItem(8, "Open in Event Editor...");
// item 7 deleted entirely
```

**Step 2: extend `showAudioClipProperties` ([Source/Standalone/BuilderPage.cpp:2625+](Source/Standalone/BuilderPage.cpp)) with the Routing dropdown.**

Shape of the additions (inside the popup's content component):

```cpp
// New ComboBox listing available Vox/Inst/Clips pages:
auto routingCombo = std::make_unique<juce::ComboBox>();

// Walk the editor's pages, adding one entry per Vox/Inst/Clips page.
// Each entry's ID = the page's mixer channel id (so we can write it directly to block.routeChannel).
const auto& blk = mPM.getBlock (blockIdx);
auto* editor = mProcessor.getStandaloneEditorRaw();    // accessor — add if needed
if (editor)
{
    int comboId = 1;   // ComboBox needs nonzero IDs; build a map id -> channelId.
    std::map<int, int> idToChannel;
    for (auto* entry : editor->getPages())
    {
        if (auto* vp = dynamic_cast<VoxPage*>(entry->component.get()))
        {
            const int chId = MixerChannelIds::voxInsert (vp->getPageIndex());
            routingCombo->addItem ("Vox " + juce::String (vp->getPageIndex() + 1), comboId);
            idToChannel[comboId] = chId;
            ++comboId;
        }
        else if (auto* ip = dynamic_cast<InstPage*>(entry->component.get()))
        {
            const int chId = MixerChannelIds::instInsert (ip->getPageIndex());
            routingCombo->addItem ("Inst " + juce::String (ip->getPageIndex() + 1), comboId);
            idToChannel[comboId] = chId;
            ++comboId;
        }
        else if (auto* cp = dynamic_cast<ClipsPage*>(entry->component.get()))
        {
            const int chId = MixerChannelIds::clipInsert (cp->getPageIndex());
            routingCombo->addItem ("Clips " + juce::String (cp->getPageIndex() + 1), comboId);
            idToChannel[comboId] = chId;
            ++comboId;
        }
    }

    // Select the page-of-origin entry (block.routeChannel matches one of the channelIds).
    for (const auto& [id, ch] : idToChannel)
    {
        if (ch == blk.routeChannel) { routingCombo->setSelectedId (id, juce::dontSendNotification); break; }
    }

    // On change: update routeChannel + trigger rebuild so AudioClipSnapshot picks up new routing.
    routingCombo->onChange = [this, blockIdx, idToChannel, comboPtr = routingCombo.get()]
    {
        if (blockIdx < 0 || blockIdx >= mPM.getNumBlocks()) return;
        const int selId = comboPtr->getSelectedId();
        auto it = idToChannel.find (selId);
        if (it == idToChannel.end()) return;
        mPM.getBlock (blockIdx).routeChannel = it->second;
        mProcessor.rebuildAudioClipPlayers();
        if (onArrangementChanged) onArrangementChanged();
    };
}
```

Place the routing combo in the popup's layout alongside the existing pitch/stretch controls (label it "Routing" or "Plays through:" — pick one user-facing label).

**Steps:**
- [ ] Delete line 2561 (`m.addItem(7, "Properties...");`).
- [ ] Rename line 2558 label to `"Properties..."`.
- [ ] Verify `showAudioClipProperties` already takes the audio-clip block index; it does (per Read at line 2625).
- [ ] Add the routing ComboBox UI + onChange handler per the shape above.
- [ ] Verify `mProcessor.rebuildAudioClipPlayers()` correctly picks up the new `block.routeChannel` value — Read [Source/PluginProcessor.cpp:3209-3232](Source/PluginProcessor.cpp:3209) confirms it does (it copies `blk.routeChannel` into the new player struct at line 3232).
- [ ] If `getStandaloneEditorRaw` accessor doesn't exist on the processor, add a minimal accessor — or thread the page list in via a callback set at editor-construction time.
- [ ] Tell Jeff: "Run `do_build.bat`. Test sequence in Debug:
  - **(1) Properties popup with routing.** Set up: Vox 1 + Inst 1 + Clips 1 all present in the project; an audio clip on the Builder grid that originated from Vox 1 (drop a Vox recording onto the grid). Right-click that audio clip → 'Properties...'. The popup opens. Verify pitch + stretch controls present (unchanged). Verify a new Routing dropdown appears, listing 'Vox 1', 'Inst 1', 'Clips 1'. Verify 'Vox 1' is the selected (ticked) entry — that's the page of origin.
  - **(2) Reroute to Inst 1.** With the popup open, change the dropdown to 'Inst 1'. Close the popup. Hit Play. Audio now plays through the Inst 1 page's signal chain (not Vox 1's). Audible difference if the two chains have different effects.
  - **(3) Reroute back.** Right-click → Properties → change dropdown back to 'Vox 1'. Audio plays through Vox 1 chain again.
  - **(4) Item 7 dead Properties gone.** Right-click any clip on the grid. Verify there's only ONE 'Properties...' item (no duplicate Properties below it).
  - **(5) Multi-recording.** Drop both Vox wet + Vox dry on the grid, both routed to Vox 1. Hit Play. Both pass through the Vox 1 chain (sequential per-clip processing; known quirk routed to QA-J per §9 13th Forks entry — not blocking).
  - **(6) Reroute one of two.** With both wet + dry on the grid routed to Vox 1, change wet's routing via Properties → 'Vox 2' (add a Vox 2 tab if needed first). Now wet plays through Vox 2, dry plays through Vox 1. Both audible at master."
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 8 — Sub-Phase Z (QA-D NIT corrections)

**NIT-1: BaySickRustyDrumsPage in `onTabRenamed` dispatch.**

```cpp
// Source/Standalone/StandaloneEditor.cpp — inside onTabRenamed dispatch (around line 1264-1310):
// ADD this branch alongside the existing 6 (Layers/Bass/Drum/Inst/Clips/Vox):
else if (auto* rp = dynamic_cast<BaySickRustyDrumsPage*> (entry->component.get()))
{
    rp->setTabName (finalName);
    if (mPianoRollPage)
        mPianoRollPage->setEngineDisplayName ({ EngineKind::BaySickRustyDrums, 0 }, finalName);
    // Mixer-strip rename handled by separate path (parallel to Drum branch handling).
}
```

**NIT-2: `restoreAudioStripsFromArrangement` `clearDirty()` defensive guard.**

```cpp
// Source/Standalone/StandaloneEditor.h — signature change:
void restoreAudioStripsFromArrangement (bool isLoadContext = true);

// Source/Standalone/StandaloneEditor.cpp — function body update:
void StandaloneEditor::restoreAudioStripsFromArrangement (bool isLoadContext)
{
    // ... existing strip-restore logic unchanged ...

    if (isLoadContext)
    {
        mProjectManager->clearDirty();   // was unconditional; now gated
    }
    // (otherwise leave dirty state alone — caller is not in a load context)
}
```

All 5 current callers (XML restore walker + project-open paths) take the default `isLoadContext = true`; no caller-site changes needed.

**NIT-3: Legacy tab-name counter migration.**

```cpp
// Source/Standalone/StandaloneEditor.cpp — advanceCountersFromRestoredTabs:
// AFTER existing parser that handles "Layer N" / "Bass N" / etc. with numeric suffix,
// ADD handling for legacy bare names:

for (auto* entry : mPages)
{
    if (! entry || ! entry->component) continue;
    const juce::String tabName = ...; // existing extraction

    // Existing trailing-number parse — unchanged.
    // ... extracts number suffix, advances matching counter to max(found) + 1 ...

    // NEW: handle pre-QA-D bare legacy names.
    if      (tabName == "Layers") mNextLayerNameNum = juce::jmax (mNextLayerNameNum, 1);
    else if (tabName == "Bass")   mNextBassNameNum  = juce::jmax (mNextBassNameNum, 1);
    else if (tabName == "Drums")  mNextDrumNameNum  = juce::jmax (mNextDrumNameNum, 1);
    // (Vox/Inst/Clips all had numeric suffixes pre-QA-D so no legacy-bare case.)
}
```

**Steps:**
- [ ] Add BaySickRustyDrumsPage branch to `onTabRenamed` (NIT-1).
- [ ] Add `bool isLoadContext = true` parameter to `restoreAudioStripsFromArrangement` + gate `clearDirty()` (NIT-2).
- [ ] Extend `advanceCountersFromRestoredTabs` with legacy-bare-name handling (NIT-3).
- [ ] Tell Jeff: "Run `do_build.bat`. Test sequence in Debug:
  - **(NIT-1) Rusty rename.** Add a BaySickRustyDrums tab. Open the unified Piano Roll page → BaySickRustyDrums view visible (kit graphic). Now rename the Rusty tab in the ribbon to 'My Kit'. Verify mixer strip name updates to 'My Kit'. Verify piano-roll context label (top-right of piano roll) updates to include 'My Kit' / engine type.
  - **(NIT-2) defensive guard.** Not user-testable directly — verify via reasoned static analysis: 5 current callers of `restoreAudioStripsFromArrangement` are all load paths; new signature defaults to `isLoadContext=true` so their behavior is unchanged. Note this in running notes; no Debug repro needed.
  - **(NIT-3) Legacy migration.** Open a pre-QA-D saved project (any project saved before 2026-05-10 that has tabs named bare 'Layers' / 'Bass' / 'Drums'). Verify the project loads. Now click the + Add button to add a new Layers tab. The new tab should be named 'Layer 2' (NOT 'Layer 1' — collision avoidance worked). Repeat for Bass, Drums. If no pre-QA-D project is handy, create one with bare names manually (rename a tab to 'Layers' to simulate) + save + reload — same expected behavior.
  - **All three pass?**"
- [ ] Wait for Jeff's verify result.
- [ ] On pass: `/draft-commit`, surface, commit on approval. (Single commit covers all 3 NITs — they're mechanical + small.)
- [ ] `/draft-doc running-notes` → apply.

### Task 9 — Close sequence
- [ ] Dispatch `/draft-doc batch-close` with a synthesis of the running-notes file.
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit.
- [ ] Dispatch `/review-batch QA-E`.
- [ ] **Surface each `/review-batch` finding individually** per `feedback_closed_batch_carryforward_via_forks.md` — no bulk "all deferred" framing regardless of severity. Each NIT gets options (fold-into-this-batch / route-to-batch-X / defer-without-target); Jeff picks.
- [ ] Address BLOCKERs / NEEDS-FIX in source if any; commit fixes; re-run `/review-batch` if scope warrants.
- [ ] Route side findings per Rule 3 — surface placement options to Jeff, never pick slot unilaterally.
- [ ] **M1 disposition (from Task 1):** if mute verify was clean, draft §9 14th Forks entry (or add to close-routing table if scope fits) routing #16a/#16b/#21 as no-longer-reproducible; surface for approval.
- [ ] Surface full git status.
- [ ] Dispatch `/draft-commit` for close commit. Surface message + status. Commit on approval.

---

## Verification (end-to-end smoke after Task 8)

After all 8 source tasks land + Task 8 commits:

1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **Mute behavior.** Row mute (pattern + audio) toggles cleanly. Block right-click mute toggles cleanly. (If M1 verify was clean, no source-level change; just confirm no regression.)
3. **No sub-tab crashes.** Engine swap or project reload on each of 7 page types → click each sub-tab → all silent no-op (no crash, no asserts).
4. **Full Vox lifecycle.** Create → record → save → close → reopen → record → reassign routing via Properties → reload → delete. No phantom strips. Recordings persist.
5. **Browser visibility.** Vox tab post-record shows wet + dry entries in browser; Inst shows dry. Both wet + dry draggable to grid; routing through Vox page chain works for both.
6. **Bus solo.** Solo Layers bus → only Layers audible. Multi-bus solo. Persistence across save/load.
7. **Routing dropdown.** Properties popup shows pitch + stretch + Routing; routing change reroutes playback through new page's chain.
8. **NIT-1/2/3.** Rusty rename propagates; legacy tab name loads bump counters correctly.

---

## Routing notes (Rule 3 application during execution)

- **Findings about other lifecycle bugs** (e.g. orphan auto-spawned strip clones, FilePlay routing not clearing on stop) → fold here if scoped to Vox/Inst lifecycle surface; route to §9 + new §5 batch otherwise.
- **Findings about DSP-09 surface that aren't bus-solo per se** (e.g. solo + sidechain interaction) → log; route to QA-Audit or watch-item per Jeff's call.
- **Findings about FILE-02 surface that go beyond audio clips** (e.g. routing for pattern blocks, automation blocks) → defer; the §5 entry explicitly scopes routing to audio clips only.
- **`/review-batch` NITs at close** → surface each individually per the new memory rule; no bulk-defer regardless of severity (BLOCKER / NEEDS-FIX / NIT).

---

## Carry-Forward Reference touch points

| Section | Lines | When |
|---|---|---|
| §1 MT primitives | 40-110 | No-op for QA-E (FilePlay restructure routed to QA-J) |
| §2 Lifecycle primitives | 113-181 | Read at Task 3 start (closeAllDynamicTabs + projectLoadInProgress) |
| §3 File:line index | 185-293 | Heavy use Tasks 3 (spawn cascades, XML restore), 6 (bus solo), 7 (FILE-02 surfaces) |
| §4 Decisions Already Made | 297-311 | Read at Task 6 start (DSP-09 target behavior) + Task 7 start (FILE-02 dropdown options) |
| §5 Per-Item Status Snapshot | 315-371 | Reference if Rule 3 routing surfaces during execution |
| §6 Patterns to Reuse | 375-395 | Apply throughout (APVTS dirty flag, audio-thread fast-path bypass, engine audition pattern) |
| §8 Anti-Patterns | 419-458 | Re-read at every task start |

---

## Plan-mode → ExitPlanMode mirror discipline

Per `feedback_plan_mirror_one_way.md`:
1. Plan-mode wrote this file to `~/.claude/plans/luminous-kindling-horizon.md`
2. On ExitPlanMode approval, mirror content to `Plans & Specs/Batch Plans/phantom-recording-mongoose.md` (canonical silly-name per `feedback_silly_name_is_my_pick.md`)
3. Delete the home-dir copy so only one source of truth exists
4. Add §5 QA-E `**Plan file:**` pointer to canonical path
5. Task 0 commit covers the mirror + pointer + running notes seed

The plan-mode-assigned home-dir name (`luminous-kindling-horizon`) is transient and unused going forward.
