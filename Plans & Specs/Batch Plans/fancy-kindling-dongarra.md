# QA-ClipDrop — Audio-Clip Drop Path Diagnostic + Fix — Plan (fancy-kindling-dongarra)

Canonical copy: `Plans & Specs/Batch Plans/fancy-kindling-dongarra.md` (mirrored from
`~/.claude/plans/` on ExitPlanMode; home-dir copy deleted — one version only).
Paired running notes: `Plans & Specs/Running Notes/fancy-kindling-dongarra.md`.

**For execution:** read alongside Main Plan §5 QA-ClipDrop entry (line ~1094) + §0
(Rule 2 carry-over, Rule 3 findings routing, Rule 4 diagnostic catalog, Rule 5
sub-spec discipline) + Carry-Forward §1/§2/§3 clip-path entries. This is a
**long-running diagnostic batch** — Task 1 lands a trap, the batch is then held
open while later batches proceed, and reactivates only when the trap fires.

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

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC-A | Slot: immediately after QA-Ed, before QA-Ee | Jeff locked 2026-06-01; §6 arrow + §9 forty-eighth Forks entry. |
| SC-B | Framing: **regression that recurred**, intermittent / session-state-dependent — NOT pre-existing | Jeff's correction 2026-06-02. §5 "pre-existing" wording corrected inline at open. |
| SC-C | Approach: **instrument-and-catch** (no blind fix) | Jeff 2026-06-02; `feedback_diagnose_before_fixing.md`. |
| SC-D | Diag form: **AlertWindow popup + append-to-log** in `Documents/BaySickDAW/`, active in **Debug AND Release** | Jeff 2026-06-02. Popup only fires on an already-failed / anomalous drop, so it never interrupts working audio. |
| SC-E | Diag coverage: **full drop cascade** (copy-on-drop + library-add + block-add + page/strip spawn) | Jeff 2026-06-02; avoids instrumenting only the leading suspect and catching nothing. |
| SC-F | Lifecycle: held open until evidence OR close-as-not-reproduced by end of QA | Jeff 2026-06-02. |

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

## Files to modify (Task 1 only — fix files TBD at Task 2)

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

### Task 2+ — Root-cause + fix (CONDITIONAL — only when the trap fires)
- [ ] On Jeff reporting the popup/log fired: read `clipdrop_diag_log.txt` → identify the exact bail step + reason (most-likely: importSample copy-failed → stale `mCurrentFolder`/`Samples/`).
- [ ] Surface the fix scope as a fresh sub-spec (DS-3) before implementing.
- [ ] Implement fix → **Tell Jeff verify** the now-known repro, Debug then Release.
- [ ] `/draft-commit` + commit. `/draft-doc running-notes`.

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
- **Task 2 (when it lands):** under the now-known repro the captured failure no longer
  occurs, verified Debug then Release.

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
