# QA-ClipDrop — Audio-Clip Drop Path Diagnostic + Fix — Plan (fancy-kindling-dongarra)

Canonical copy: `Plans & Specs/Batch Plans/fancy-kindling-dongarra.md` (mirrored from
`~/.claude/plans/` on ExitPlanMode; home-dir copy deleted — one version only).
Paired running notes: `Plans & Specs/Running Notes/fancy-kindling-dongarra.md`.

**For execution:** read alongside Main Plan §5 QA-ClipDrop entry (line ~1094) + §0
(Rule 2 carry-over, Rule 3 findings routing, Rule 4 diagnostic catalog, Rule 5
sub-spec discipline) + Carry-Forward §1/§2/§3 clip-path entries. This batch is a
**diagnostic trap (Task 1) + a deterministic clip-coupling fix (Tasks 2/3, SC-G..J)**,
then **held open** for the intermittent saved-project copy-failure (Task 4) — see
"Diagnosis outcome" below.

---

## Context

**The bug.** The audio-clip drop path intermittently fails. Originally logged
(Main Plan §5) as 5 drag-drop symptoms: (1) one WAV → two browser entries;
(2) a second file → no clip + no page; (3) re-dropping it → "already exists in the
project" prompt yet no clip/page; (4) "Use Existing" → unattached clip that still
creates a strip + plays; (5) "New Page" → adds correctly but "as though it already
existed."

**Framing correction (Jeff, 2026-06-02).** NOT pre-existing (the §5 entry's wording
is wrong and gets corrected at open). This is a **regression of a bug we already
fixed once that came back** — "we went through the whole process of fixing and then
it happened again."

**Diagnosis so far (collaborative, 2026-06-02).** The bug is **intermittent and
session-state-dependent — it clears on a fresh app restart.** During this session:
- The 5 drag-from-Explorer symptoms did **not** reproduce (3 targeted repro
  scenarios all passed; the original pattern-and-clip test rig re-tested clean).
- The reliably-observed failure was the **Clips ribbon `+ Add New Clip` file-picker
  doing nothing on file-select** — no page, no strip, no browser entry.
- That **also cleared on a full restart** — confirming session-accumulated state,
  not a deterministic break and not file-specific.

**Code trace (source-verified).** `+ Add New Clip`
([StandaloneEditor.cpp:3704](Source/Standalone/StandaloneEditor.cpp:3704)) and
drag-drop ([BuilderPage.cpp:3562](Source/Standalone/BuilderPage.cpp:3562)) both
converge on `ArrangementGrid::importAudioFile`
([BuilderPage.cpp:3350](Source/Standalone/BuilderPage.cpp:3350)). The **missing
browser entry proves the drop bails BEFORE `addAudioToLibrary`**
([:3415](Source/Standalone/BuilderPage.cpp:3415)) — i.e. at one of two silent
early-returns: file-not-found ([:3365](Source/Standalone/BuilderPage.cpp:3365)) or
copy-on-drop empty ([:3402](Source/Standalone/BuilderPage.cpp:3402), where
`importSample` returned `{}`). With a project open (drag works), `importSample`
([ProjectManager.cpp:484](Source/ProjectManager.cpp:484)) only returns empty on
**`copyFileTo` failure** → the project's `Samples/` folder state is bad for that
session; a restart rebuilds `mCurrentFolder` → works again.

**Conclusion.** Very likely ONE intermittent session-state failure (copy-on-drop
poisoned by mid-session project lifecycle state — Jeff was doing new/save/open/reload
churn for the QA-Ed test rig), not 6 separate deterministic bugs. We cannot
reproduce on demand → we cannot fix-and-verify blind
(`feedback_diagnose_before_fixing.md` — this is the Phase-D-drum rabbit-hole trap).

**Approach (Jeff's call, SC-C).** Instrument-and-catch: land a diagnostic trap that
captures the failure live, then fix with evidence.

**Risk.** Task 1 (diagnostic) = **LOW** — append-only file logging + popup-on-anomaly,
no behavior change; mirrors the existing `namirLog()`/`pedalsLog()` diagnostic
convention. Eventual fix (Task 2+) = **TBD** until the trap fires.

**Effort.** Task 1 = **Low** (one source commit). Total = **TBD** (held open).

**Dependencies.** None (independent of the QA-Ed transport rework).

**Bucket.** System Pages.

**Lifecycle (SC-F).** Open → Task 1 lands the trap → **held open** while QA-Ee and
later batches proceed; Jeff watches for the trap to fire → on evidence, Task 2+
root-cause + fix → close. If never reproduced **by end of QA → close as "not
reproduced"** (with a Rule-4 strip/keep decision on the probes at that point).

---

## Diagnosis outcome (2026-06-02 — trap fired, fix scoped)

The trap armed + caught evidence immediately, splitting the original report into **two
distinct problems**:

1. **Deterministic clip ↔ grid-row coupling (the "huge issue" Jeff surfaced) — FIXED in
   Tasks 2/3.** Clips are keyed to their Builder grid row: strips named "Track N",
   "+ Add New Clip" drops a block on row 0, and moving a clip to a row with no strip
   silences it. Verified mechanism: `renderAudioClipsForRow` routes by `trackRow`
   (PluginProcessor.cpp:405); a block move changes only `trackRow` with no routing/strip
   update (BuilderPage.cpp:4451). NOT a QA-E regression and NOT the cosmetic retag — the
   coupling is present in the earliest visible commit (the 2026-04-28 re-baseline
   `d595ee3`); the independent-clip behavior Jeff remembers predates the re-baseline
   (squashed, un-bisectable — stated honestly, not used to deflect). Spec: SC-G..SC-J.
   **Affects clips only** — Vox/Inst WAVs already route by their page (`routeChannel`),
   independent of the grid (renderFilePlayPlayer:623 / pre-scan:1564, verified 2026-06-02).
2. **Intermittent saved-project copy-failure (the ORIGINAL report) — STILL HELD OPEN
   (Task 4).** "+ Add New Clip does nothing" that cleared on restart, in a SAVED project
   (Jeff, 2026-06-02). The trap has only caught the no-project skip so far (log line 49:
   copy-empty with NO `importSample` call = `hasProject()` false); the saved-project
   copy-fail mode has NOT recurred. The shared `importSample` probe stays in to catch it.

Jeff's call (2026-06-02): fix #1 in-batch now (Tasks 2/3), keep the batch held-open for
#2 (Task 4).

---

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC-A | Slot: immediately after QA-Ed, before QA-Ee | Jeff locked 2026-06-01; §6 arrow + §9 forty-eighth Forks entry. |
| SC-B | Framing: **regression that recurred**, intermittent / session-state-dependent — NOT pre-existing | Jeff's correction 2026-06-02. §5 "pre-existing" wording corrected inline at open. |
| SC-C | Approach: **instrument-and-catch** (no blind fix) | Jeff 2026-06-02; `feedback_diagnose_before_fixing.md`. |
| SC-D | Diag form: **AlertWindow popup + append-to-log** in `Documents/BaySickDAW/`, active in **Debug AND Release** | Jeff 2026-06-02. Popup only fires on an already-failed / anomalous drop, so it never interrupts working audio. |
| SC-E | Diag coverage: **full drop cascade** (copy-on-drop + library-add + block-add + page/strip spawn) | Jeff 2026-06-02; avoids instrumenting only the leading suspect and catching nothing. |
| SC-F | Lifecycle: held open until evidence OR close-as-not-reproduced by end of QA | Jeff 2026-06-02. |
| SC-G | "+ Add New Clip" → browser listing + Clips page + strip, **no grid block** | Jeff 2026-06-02. It currently routes through the drag-drop importer (which places a block on row 0). |
| SC-H | Clip mixer-strip name follows the **Clips page/tab name** (synced like Layers/Bass/Drums) | Jeff 2026-06-02. Today named from the grid row label ("Track N"); Clips has no tab→strip rename wiring. |
| SC-I | **Route a clip by its owning Clips-page strip, NOT its grid row** — nothing auto-attaches to a grid track; moving a clip never breaks playback. Both drag-drop + "+ Add New Clip" | Jeff 2026-06-02 (emphatic). Brings clips in line with how Vox/Inst WAVs already route (by page, not row). |
| SC-J | No-project case: keep the "name your project first" prompt | Jeff 2026-06-02. |

---

## Sub-spec calls surfaced for ExitPlanMode

These are **genuinely deferred** decisions (close-time / evidence-time) — not picks
I am making now:

- **DS-1 (close-time):** §5.5 Domain Coverage "System Pages" row is missing
  QA-ClipDrop (and "System Pages / Cross-cutting" rows are missing QA-Ed +
  QA-TempoMap — all three missed at their insert). Backfill in THIS batch's close,
  or route to a separate doc-cleanup pass? **Placement is Jeff's call at close.**
- **DS-2 (close-time):** Rule-4 disposition of the diagnostic probes IF the bug never
  reproduces — strip at close, or keep (in case it recurs post-V1)? Borderline
  Keep/Remove → Jeff's call, surfaced at close.
- **DS-3 (evidence-time):** the actual fix scope (Task 2+) is unknown until the trap
  fires. Will be surfaced as a fresh sub-spec when evidence lands.
- **DS-4 (end-of-QA):** the exact "end of QA" trigger for close-as-not-reproduced
  (last QA-* batch close / pre-V1 tag) — Jeff confirms when we reach it.

---

## Files to modify (Task 1 diagnostic; Task 2/3 fix)

- **NEW `Source/ClipDropDiag.h`** — shared diagnostic helper (`log()` + `alert()`),
  mirroring `namirLog()` ([BaySickNAMIRProcessor.cpp:18](Source/BaySickNAMIR/BaySickNAMIRProcessor.cpp:18))
  / `pedalsLog()` ([BaySickPedalsProcessor.cpp:21](Source/BaySickPedals/BaySickPedalsProcessor.cpp:21)).
  One header shared by the 3 instrumented TUs (vs. per-TU copies — single definition,
  no duplication; minimal, removed/kept wholesale at close).
- **`Source/ProjectManager.cpp`** — `importSample` ([:484-514](Source/ProjectManager.cpp:484)):
  log the **specific empty-return reason** (no-project / file-missing / copy-failed)
  + resolved `samplesDir` + `target` + dir-exists. *This is the key WHY probe.*
- **`Source/Standalone/BuilderPage.cpp`** — `importAudioFile` ([:3350](Source/Standalone/BuilderPage.cpp:3350)):
  enter probe; **`alert()` on file-missing bail** ([:3365](Source/Standalone/BuilderPage.cpp:3365));
  **`alert()` on copy-on-drop-empty bail** ([:3402](Source/Standalone/BuilderPage.cpp:3402));
  post-library-add / post-block-add / onAudioClipAdded-fired-or-skipped probes.
  `filesDropped` ([:3562](Source/Standalone/BuilderPage.cpp:3562)): enter + which
  branch (`onDuplicateFileDrop` vs `importAudioFile`).
- **`Source/Standalone/StandaloneEditor.cpp`** — `onAddTabRequest` Clip branch
  ([:3704](Source/Standalone/StandaloneEditor.cpp:3704)): picked-file / cancel probe;
  `grid->onAudioClipAdded` lambda ([:2020](Source/Standalone/StandaloneEditor.cpp:2020)):
  enter + strip-add + spawnClips-call probes; `spawnClipsTabIfMissing`
  ([:7697](Source/Standalone/StandaloneEditor.cpp:7697)): which early-return
  (dedup-by-path / row-taken) vs new-page-created — **`alert()` if an explicit
  `+ Add New Clip` produced no new page AND no new library entry** ("nothing
  happened" anomaly).

**Task 2/3 fix files (deterministic clip-coupling fix, SC-G..J):**
- `Source/PluginProcessor.cpp` — `renderAudioClipsForRow` filter → route by owner ([:405](Source/PluginProcessor.cpp:405)); key the row mute on the owner row.
- `Source/PatternManager.cpp` — block deserialize migration ([:1452](Source/PatternManager.cpp:1452)): `routeChannel==0 && Audio → audioInsert(trackRow)`.
- `Source/Standalone/StandaloneEditor.cpp` — rewrite `onAddTabRequest` Clip branch (no block); extract `createClipStripAndPage` helper from `onAudioClipAdded` ([:2036](Source/Standalone/StandaloneEditor.cpp:2036)); wire ClipsPage `onTabRenamed`→strip rename ([:1308](Source/Standalone/StandaloneEditor.cpp:1308)); reload strip name from the Clips tab.
- `Source/Standalone/MixerPage.h/.cpp` — add `StripKind::Audio` + `renameChannel` case (or `renameAudioChannel`) ([MixerPage.cpp:2909](Source/Standalone/MixerPage.cpp:2909)).
- `Source/Standalone/BuilderPage.cpp` — confirm `placeAudioLibraryEntry` owner stamp ([:3502](Source/Standalone/BuilderPage.cpp:3502)); no move-handler change (routeChannel preserved on move).

**Diag helper sketch:**

```cpp
// Source/ClipDropDiag.h — QA-ClipDrop trap. Rule-4 catalog'd; strip/keep at close (DS-2).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ClipDropDiag
{
    inline juce::File logFile()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("BaySickDAW").getChildFile ("clipdrop_diag_log.txt");
    }
    inline void log (const juce::String& stage, const juce::String& detail)
    {
        auto f = logFile(); f.getParentDirectory().createDirectory();
        f.appendText (juce::Time::getCurrentTime().toString (false, true, true, true)
                       + "  [QA-ClipDrop DIAG] " + stage + " | " + detail + juce::newLine);
    }
    inline void alert (const juce::String& stage, const juce::String& detail)
    {
        log (stage, detail);                                   // always record
        const juce::String body = stage + "\n\n" + detail;
        juce::MessageManager::callAsync ([body] {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Clip-Drop Diagnostic", body); });
    }
}
```

**Key probe sketch (the WHY):**

```cpp
// ProjectManager.cpp importSample — record every empty-return path:
if (! externalFile.copyFileTo (target)) {
    ClipDropDiag::log ("importSample:COPY-FAILED",
        "src="       + externalFile.getFullPathName()
      + " target="   + target.getFullPathName()
      + " samplesDir=" + samplesDir.getFullPathName()
      + " dirExists=" + (samplesDir.isDirectory() ? juce::String("Y") : "N"));
    return {};
}
// BuilderPage.cpp importAudioFile — copy-on-drop empty bail (the popup):
if (storedPath.isEmpty()) {
    ClipDropDiag::alert ("importAudioFile BAIL: copy-on-drop returned empty",
        "path=" + path + "  (drop produced no clip/page/entry — see importSample reason in log)");
    if (onDropWithoutProject) onDropWithoutProject (f, targetRow, targetBar);
    return;
}
```

---

## Tasks

### Task 0 — Batch open (docs only)
- [ ] Mirror this plan → `Plans & Specs/Batch Plans/fancy-kindling-dongarra.md`; delete the `~/.claude/plans/` copy (one version only — `feedback_plan_mirror_one_way.md`).
- [ ] Seed `Plans & Specs/Running Notes/fancy-kindling-dongarra.md` (title / purpose blockquote / pair-file ref / convention ref) per §0 running-notes required sections.
- [ ] Main Plan §5 QA-ClipDrop entry — **targeted Edits only** (`feedback_targeted_edits_not_wholesale_rewrite.md`): add `**Plan file:** \`Plans & Specs/Batch Plans/fancy-kindling-dongarra.md\``; add `**STATUS (2026-06-02 open):** OPEN — long-running diagnostic batch; Task 1 lands the trap, held open until evidence or end-of-QA`; correct the "pre-existing clip-path issue" wording to "regression (recurred), intermittent / session-state-dependent" (SC-B).
- [ ] `/draft-commit` → surface drafted message + FULL git status → Jeff approves → commit Task 0 (docs only).
- [ ] `/draft-doc running-notes` → apply.

### Task 1 — Full-cascade clip-drop diagnostic trap (one source commit)
- [ ] Add `Source/ClipDropDiag.h` (sketch above) + register in CMake source list if it needs explicit listing (header-only `#pragma once` — likely no CMake change; verify).
- [ ] Instrument `importSample` (ProjectManager.cpp): enter + every return path (no-project / file-missing / `target==src` short-circuit / collision-skip / auto-number / copy-failed) with samplesDir + target + result.
- [ ] Instrument `importAudioFile` (BuilderPage.cpp): enter; `alert()` at file-missing bail; `alert()` at copy-on-drop-empty bail; `log()` post-library-add, post-block-add, onAudioClipAdded fired/skipped.
- [ ] Instrument `filesDropped` (BuilderPage.cpp): enter + duplicate-vs-import branch.
- [ ] Instrument `onAddTabRequest` Clip branch + `onAudioClipAdded` + `spawnClipsTabIfMissing` (StandaloneEditor.cpp): picked file; cascade steps; which spawn early-return vs created; `alert()` on the explicit-add-produced-nothing anomaly.
- [ ] Record every probe in Running Notes `## Diagnostic Instrumentation Catalog` in the SAME edit pass (Site / Tag `[QA-ClipDrop DIAG]` / Purpose / Disposition `Remove at batch close — or Keep per DS-2`).
- [ ] **Tell Jeff — build `do_build.bat`, then verify the trap is ARMED** (not the bug itself — it's intermittent):
  1. **Debug:** open/save a project → `+ Add New Clip` → pick a normal WAV → a new Clips page + strip + Builder-browser entry appear (works) AND `Documents/BaySickDAW/clipdrop_diag_log.txt` shows the full successful cascade (enter → importSample success → library-add → block-add → onAudioClipAdded → spawnClips created). **No popup on success.**
  2. **Release:** repeat — confirm the log writes in Release too.
  3. **Popup wiring:** temporarily add one forced `ClipDropDiag::alert("SMOKE","popup wiring test")` at `importAudioFile` enter, build, confirm the popup shows in Debug + Release, then **remove that one line before commit** (catalog Disposition `Remove at Task 1 close`).
- [ ] Strip the temp SMOKE alert; surface the strip list to Jeff first.
- [ ] `/draft-commit` → surface + FULL git status → Jeff approves → commit Task 1 (source). Long message → `.git/COMMIT_EDITMSG_QA-ClipDrop-Task1.txt` + `git commit -F` + `rm`.
- [ ] `/draft-doc running-notes` → apply.
- [ ] **On Task 1 commit: QA-ClipDrop → HELD-OPEN. Proceed to QA-Ee per §6.** Jeff watches for the trap during normal work across later batches.

### Task 2 — SC-I: route clips by their owning Clips-page strip, not the grid row (audio hot path)

**Goal:** a clip's audio renders into its owning Clips-page strip regardless of which Builder
grid row the block sits on. A block move changes only `trackRow`, which no longer affects routing
→ nothing attaches to a grid track; moving never breaks playback. (Matches Vox/Inst, which already
route by page via `routeChannel`.) **HIGH-RISK — audio hot path + saved-project compatibility.**

- [ ] **Render by owner, not trackRow.** In `renderAudioClipsForRow` ([PluginProcessor.cpp:405](Source/PluginProcessor.cpp:405)), replace the `trackRow` filter, placed AFTER the existing Vox/Inst skip ([:408-414](Source/PluginProcessor.cpp:408)) so Vox/Inst still go via the FilePlay path:
  ```cpp
  // was: if (player.trackRow != row) continue;
  const int ownerRow =
      (player.routeChannel >= MixerChannelIds::kAudioBase
    && player.routeChannel <  MixerChannelIds::kAudioBase + kMaxAudioRows)
          ? (player.routeChannel - MixerChannelIds::kAudioBase)   // route by owning Clips-page strip
          : player.trackRow;                                      // legacy/unset (routeChannel 0) → grid row
  if (ownerRow != row) continue;
  ```
  Verified safe vs the FilePlay path: Audio-range `routeChannel` (400–449) is disjoint from Vox (600)/Inst (700); `renderFilePlayPlayer` returns false for non-Vox/Inst ([:623](Source/PluginProcessor.cpp:623)) and the pre-scan ([:1564](Source/PluginProcessor.cpp:1564)) only flags Vox/Inst.
- [ ] **Owner stamped at creation** (mostly already true): the grid-drop retag (`onAudioClipAdded`, [StandaloneEditor.cpp:2071-2097](Source/Standalone/StandaloneEditor.cpp:2071)) sets `blk.routeChannel = audioInsert(addRow)` — it stops being cosmetic and becomes the authoritative owner; `placeAudioLibraryEntry` ([BuilderPage.cpp:3502](Source/Standalone/BuilderPage.cpp:3502)) already stamps `routeChannel = owner`. Moves leave `routeChannel` untouched.
- [ ] **Old-project migration (one-time load fixup).** In the block deserialize ([PatternManager.cpp:1452](Source/PatternManager.cpp:1452), after reading `routeChannel`) — or a post-load fixup if include-layering blocks it (MixerChannelIds is in VibeGraph.h):
  ```cpp
  if (b.clipType == ClipType::Audio && b.routeChannel == 0)
      b.routeChannel = MixerChannelIds::audioInsert (b.trackRow);   // legacy clips → owner-routing
  ```
  Pre-fix projects keep playing AND gain move-survival (the exact equivalence the old code relied on).
- [ ] **Mute follows the clip's strip.** Where `renderAudioClipsForRow` applies row mute / `isRowAudible`, key it on the owner row, so a clip's mute matches its strip (verify against the current mute logic).
- [ ] **Tell Jeff — build, verify Debug then Release:** (1) drag a WAV onto the grid → plays; **move the block to another row → still plays**; (2) move it to a never-used row → still plays; (3) open an OLD project (TESTIES) → existing clips play, strips unchanged; (4) Vox/Inst WAVs unaffected (drag/move still play through their page strip). Trap log shows clips routing to their owner row.
- [ ] `/draft-commit` → surface drafted message + FULL git status → Jeff approves → commit Task 2 (source; `git commit -F`).
- [ ] `/draft-doc running-notes` → apply.

### Task 3 — SC-G + SC-H + SC-J: "+ Add New Clip" makes no grid block; strip named from the clip

- [ ] **SC-G — extract a shared strip+page helper** from `onAudioClipAdded` (the trio `addAudioRowChannel` + `ensureAudioInsert` + `mMixerPage->addAudioChannel`, [StandaloneEditor.cpp:2036-2044](Source/Standalone/StandaloneEditor.cpp:2036)) into a reusable `createClipStripAndPage(row, path, name)`. Used by both `onAudioClipAdded` (drag-drop) and the new "+ Add New Clip" path.
- [ ] **SC-G — rewrite `onAddTabRequest` Clip branch** ([StandaloneEditor.cpp:3704](Source/Standalone/StandaloneEditor.cpp:3704)): on file pick → copy the sample via the same copy-on-drop step (keeping `onDropWithoutProject` → New-Project prompt + retry, **SC-J**) → find next free Clips slot `P` → `addAudioToLibrary(storedPath, {}, audioInsert(P))` (owner = P) → `createClipStripAndPage(P, storedPath, name)` (page + strip, **no** `addBlock`). Replaces the current `importAudioFile(f, 0, 0)` call.
- [ ] **SC-H — name the strip from the clip/page** (not the row label). In the helper, name the strip from the sample/Clips-tab name (`File(path).getFileNameWithoutExtension()` / `cp->getTabName()`), not `mRowNames[row]` / "Track N". On reload, name the Audio strip from the restored Clips tab (today `restoreAudioStripsFromArrangement` uses `displayAlias`/"Audio N" — reconcile so the page name wins).
- [ ] **SC-H — tab→strip rename sync.** Add `Audio` to `MixerPage::StripKind` ([MixerPage.h:186](Source/Standalone/MixerPage.h:186)) + a `case StripKind::Audio: → mAudioStrips[pageIdx]->setTrackName(name)` in `renameChannel` ([MixerPage.cpp:2909](Source/Standalone/MixerPage.cpp:2909)) — or a dedicated `renameAudioChannel`. Wire the ClipsPage `onTabRenamed` branch ([StandaloneEditor.cpp:1308](Source/Standalone/StandaloneEditor.cpp:1308), currently "left untouched") to call it. Persistence: add Audio strip names to `writeStripNames` ([:9508](Source/Standalone/StandaloneEditor.cpp:9508)) OR push the (already-persisted) Clips tab name to the strip on restore.
- [ ] **Tell Jeff — build, verify Debug then Release:** (1) "+ Add New Clip" in a saved project → Clips page + sample-named strip + browser listing, **nothing on the Builder grid**; (2) same in a brand-new/unsaved project → New-Project prompt → name it → clip added (still no grid block); (3) rename the Clips tab → its mixer strip renames too; (4) drag that clip from the browser onto the grid → block plays through the same strip, survives a move.
- [ ] `/draft-commit` → surface + FULL git status → Jeff approves → commit Task 3 (source; `git commit -F`).
- [ ] `/draft-doc running-notes` → apply.

### Task 4 — Held open: the intermittent saved-project copy-failure (the 2nd case) — CONDITIONAL
- [ ] The original report was a "+ Add New Clip does nothing" in a SAVED project that cleared on restart — a copy-fail-WITH-project mode the trap has NOT yet caught (only the no-project skip was seen 2026-06-02). The trap (esp. the shared `importSample copyFileTo FAILED` probe) stays in across Tasks 2/3 + later batches to catch it.
- [ ] On Jeff reporting the trap fired WITH a project open: read `clipdrop_diag_log.txt` → identify the exact reason (`copyFileTo` failed + samplesDir state, OR `hasProject()` flipped false mid-session) → surface fix scope (fresh sub-spec, DS-3) → fix → verify Debug+Release → commit.

### Task N — Close (one of two paths)
- [ ] `/draft-doc batch-close` → apply to Implemented Work Log.
- [ ] `/review-batch QA-ClipDrop` → address BLOCKER / NEEDS-FIX; defer NITs into the close entry.
- [ ] Rule-4 strip pass on the diagnostic probes (surface strip list) OR keep per DS-2.
- [ ] Route side findings (Rule 3): DS-1 §5.5 backfill (placement = Jeff); any new failure modes the trap revealed outside the copy-on-drop root.
- [ ] `/draft-commit` → close commit (separate from source — clean rollback boundary).
- **Path (a) FIXED:** close entry = the captured root cause + fix.
- **Path (b) NOT REPRODUCED by end of QA:** close entry = "trap armed, never fired"; DS-2 strip/keep recorded.

---

## Verification (end-to-end smoke)

- **Task 1 (trap armed):** a successful `+ Add New Clip` AND a successful drag-drop
  each write a full success trace to `clipdrop_diag_log.txt` in **both Debug and
  Release**; popup wiring confirmed via the temp forced-alert (removed before commit);
  **no popup on success**. This proves the trap is live without needing the
  (intermittent) bug.
- **Task 2 (routing decouple):** drag a clip → move it across grid rows (incl. a never-used
  row) → keeps playing; an OLD project (TESTIES) loads with clips playing + strips unchanged;
  Vox/Inst WAVs unaffected (drag/move still play through their page strip).
- **Task 3 ("+ Add New Clip" + naming):** "+ Add New Clip" → Clips page + sample-named strip +
  browser listing, **nothing on the Builder grid** (saved + brand-new project); Clips tab rename
  → strip renames; browser-drag of that clip onto the grid → plays + survives a move.
- **Task 4 (held-open 2nd case):** when the trap fires with a project open, the captured
  copy-failure no longer reproduces after its fix, Debug + Release.

---

## Routing notes (Rule 3 application during execution)

- §5 "pre-existing" → "regression / intermittent" framing correction — done inline at Task 0 open (SC-B).
- §5.5 Domain Coverage backfill (QA-ClipDrop + QA-Ed + QA-TempoMap absent) — DS-1, Jeff's placement call at close.
- Any NEW failure mode the trap reveals OUTSIDE the copy-on-drop root — route per Rule 3 (fold into a not-yet-started batch / annotate a closed one / new §5 row) at close; surface placement to Jeff.
- Diagnostic catalog maintained in Running Notes per Rule 4; strip pass (or keep per DS-2) at close.

---

## Carry-Forward Reference touch points

- **§3 Builder grid drop & block resize** (BuilderPage `filesDropped`/`importAudioFile`) — read at Task 1. NOTE: frozen-2026-05-07 line numbers have drifted post-QA-E; use current source (verified in this plan).
- **§3 Page-binding** (`pageOwnerChannelId` library-driven model) — read at Task 1/2.
- **§1 ClipPageTask/AudioInsertTask channel-id landmine** (DSP-12 → CompositeAudioInsertTask via QA-0) — read at Task 2 IF the fix touches registration/routing.
- **§2 AudioClipSnapshot RCU + `rebuildAudioClipPlayers`** — read at Task 2 IF the fix touches clip players.

---

## Carry-Over (Rule 2)

- **Completed:** Plan authored; root-cause investigation done (failure isolated to
  `importAudioFile` bailing before library-add, via `importSample` empty-return =
  copy-on-drop failure; session-state-dependent, clears on restart). Diag design
  locked (SC-D/E), reuse pattern found (`namirLog`).
- **In-flight:** Awaiting plan approval → Task 0 open.
- **Assumptions changed:** §5 "pre-existing" framing is wrong → regression (SC-B).
  Carry-Forward §3 clip-path line numbers have drifted post-QA-E.
- **Resume action:** On approval — mirror plan, seed running notes, edit §5, commit
  Task 0; then implement Task 1 diagnostic.
- **Implemented-work entry needed (at close):** QA-ClipDrop diagnostic trap + (if
  fired) the captured root cause + fix; OR not-reproduced close.
