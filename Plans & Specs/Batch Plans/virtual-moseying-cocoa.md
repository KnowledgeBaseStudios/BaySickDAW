# QA-Ed — Song-Mode Transport Integer-Sample Source-of-Truth (Issue 3) — Plan (virtual-moseying-cocoa)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/virtual-moseying-cocoa.md`
> Paired running notes: `Plans & Specs/Running Notes/virtual-moseying-cocoa.md`

> **For execution:** steps use `- [ ]` checkbox syntax. Build commands run by Jeff (`do_build.bat`) — never by Claude. Verify in Debug exe FIRST, then Release (CLAUDE.md Build System standing rule). MT works in Debug (QA-Md closed) — verify in the normal Debug→Release cycle.

---

## Context

**Why this batch.** "Issue 3" (Main Plan §5 QA-Ed): in song mode the transport intermittently (1) **drops the first note on a loop-wrap** and (2) lets pattern notes start **later than sample-positioned audio clips** (drift over a long arrangement). Root cause, code-confirmed: `StandalonePlayHead::advanceBlock` ([StandaloneApp.cpp:136](Source/Standalone/StandaloneApp.cpp:136)) holds the playhead position as a **float beat accumulator** (`mPPQPos += numSamples · bpm/(60·sr)` every block, `fmod` loop-wrap). Float rounding **accumulates** → pattern beats drift from sample-exact clips; the `fmod` wrap lands `beatStart` a fraction past the loop point → the boundary note is missed → today's `mPRLastBeatEnd`/`kWrapSlop`/`jumped`/`windowStart` band-aid in the pattern-mode scheduler ([PluginProcessor.cpp:1285-1320](Source/PluginProcessor.cpp:1285)).

**The fix.** Make an **int64 absolute sample counter** the transport source-of-truth; derive beat via a **tempo anchor** that re-bases on every BPM change (preserves tempo-automation behavior; no full tempo map — that's a future batch). The wrap becomes an **exact integer-sample** operation. The scheduler is refactored so a block that crosses the loop boundary schedules the wrapped notes **within that same block at their exact sample** (Jeff's locked choice: sample-accurate seam, not block-granular). The float band-aid (`mPRLastBeatEnd` + `kWrapSlop` + `jumped` + `windowStart`) is **removed**, replaced by exact loop-start/-end beats + an exact integer straddle test. Public playhead API stays in **beats** so every consumer (UI cursors, scheduler, MT render tasks) is untouched.

**Mechanism correction vs the §5 wording (surfaced + acknowledged 2026-06-01).** §5 implies the exact clock alone lets a plain `>= beatStart && < beatEnd` gate fix the drop. It does not: the scheduler runs **before** the playhead advances ([StandaloneApp.cpp:106](Source/Standalone/StandaloneApp.cpp:106) `processBlock`, then [:124](Source/Standalone/StandaloneApp.cpp:124) `advanceBlock`), so the wrap lands one block's overshoot past the loop start, leaving a sub-block gap the boundary note hides in. The correct fix retains loop-seam logic but driven by **exact** values (the band-aid is still fully removed). See the Spec-calls + Tasks below.

**Scope reality (read before approving).** Jeff chose the sample-accurate seam (option B). A Plan-agent design review confirmed B is a genuine **hot-path scheduler refactor**, not a deletion: it needs (a) a **seqlock** + single-writer anchor for thread-safe beat derivation, (b) the scheduler's straddle test sharing the playhead's **integer** wrap point via `PositionInfo::timeInSamples`, (c) a shared **window-based note-scheduling helper** replacing the ~14 per-engine inline loops across both modes, (d) a sample-accurate **off-sweep** at the wrap. This is larger than the §5 "remove the band-aid" framing. **Smaller fallback if appetite is exceeded:** option A (block-granular seam — the post-wrap block extends its window to the exact loop-start; first note fires at block granularity = today's precision, no drop). A is ~half the change and drops items (a)-partial, (c)-simpler, (d). Flagged here so the size is visible at approval; proceeding with B per the lock.

**Risk:** **HIGH** — hot-path transport + scheduler; every song/pattern playback path. **Mitigations:** single MT render path (QA-Ef deleted the serial tail — no dual-path mirroring); public API unchanged; one atomic commit (broken→fixed). **Effort:** large.

**Dependencies:** QA-Ea (closed), QA-Ef (closed — single MT path). No open blockers. **Out of scope:** 96-PPQ tick storage for notes/clips/automation = **QA-Ee** (next batch; rides on top of this int-sample transport — boundary verified clean 2026-06-01). Sample-accurate tempo *mapping* = future batch.

**Bucket:** Cross-cutting Infrastructure.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC-1 | **Tempo handling: int64 absolute sample counter = source-of-truth; beat derived via a tempo anchor that re-bases on each BPM change.** No sample-accurate tempo map. | Jeff 2026-06-01. Fixes float slop + loop-wrap drops; preserves current tempo-automation behavior (no regression). Tempo map = future batch (scope-creep rejected). |
| SC-2 | **One atomic source commit** (clock rework + scheduler refactor + band-aid removal land together). | Jeff 2026-06-01. Interdependent; band-aid removal only safe once the clock is exact. Clean broken→fixed boundary. |
| SC-3 | **No `/architecture` pass.** | Jeff 2026-06-01. Architecture known (int64 sample truth + tempo anchors = standard DAW transport). (A focused Plan-agent design review WAS run in-plan and is folded below.) |
| SC-4 | **Loop seam = sample-accurate (option B):** a block crossing the loop boundary schedules the wrapped notes within that block at their exact sample. | Jeff 2026-06-01 (chose B over block-granular A). Tighter than today; no flam at the loop point. |
| SC-5 | **Public playhead API stays in beats** (`getCurrentBeat()`, `getPosition().ppqPosition`); only the internal representation changes. Additionally publish `PositionInfo::timeInSamples` (already a standard field) so the scheduler shares the exact integer clock. | Bounds blast radius — every consumer reads beats and is untouched. `timeInSamples` is a standard `PositionInfo` field → no new coupling. |
| SC-6 | **Tick/96-PPQ work is OUT** — QA-Ee. QA-Ed is sample-domain transport only. | §5 + §9 2026-05-20 sequencing; boundary verified clean 2026-06-01. |
| SC-7 | **Silly-name = `virtual-moseying-cocoa`** (plan-mode runtime assignment; running-notes file matches). | Runtime-assigned, same as the `federated-bouncing-cupcake` precedent. |

---

## Sub-spec calls surfaced for ExitPlanMode

**None open.** All architecture decisions are locked (SC-1..SC-6). The seam-precision fork (block-granular vs sample-accurate) was surfaced in chat and resolved to **sample-accurate (B)** 2026-06-01. The remaining choices are pure internal correctness mechanism with no Jeff-preference fork (seqlock vs torn-read; integer straddle via `timeInSamples`; window-helper shape; off-sweep) — made in the plan body and described in plain-English behavior terms in the verify scripts, per `feedback_design_approval_in_plain_english.md`. Two items are recorded as **verify-watch** (not deferred spec calls) under Routing notes: (i) song-loop held-note release across a wrap (pre-existing; leave unless it reproduces), (ii) loops shorter than one audio block (degenerate; documented graceful fallback).

---

## Files to modify (one atomic commit — Task 1)

### Playhead — `Source/Standalone/StandaloneApp.h` + `.cpp`
- **`StandalonePlayHead` state** ([StandaloneApp.h:45-54](Source/Standalone/StandaloneApp.h:45)): remove `mPPQPos`. Add `std::atomic<int64_t> mSamplePos{0}` (source-of-truth; **sole audio-thread writer = `advanceBlock`**). Add the **seqlock-protected anchor tuple** `{double mAnchorBeat; int64_t mAnchorSample; double mAnchorBpm;}` + `std::atomic<uint32_t> mAnchorSeq{0}` (**single writer = message thread**). Make `mSampleRate` `std::atomic<double>`. Add `std::atomic<bool> mSeekDiscontinuity{false}` (set on a backward seek; consumed by the scheduler for the note-off flush).
- **Beat derivation** (`deriveBeat(int64 sample)` private, seqlock read): `mAnchorBeat + (sample - mAnchorSample) * mAnchorBpm / (60·sr)`. `getCurrentBeat()` → `deriveBeat(mSamplePos)`.
- **`advanceBlock`** ([StandaloneApp.cpp:136](Source/Standalone/StandaloneApp.cpp:136)): rewrite — `mSamplePos += n`; integer-sample wrap into `[loopStartSamp, loopEndSamp)` **preserving overshoot** (`%span`, NOT snap-to-start — preserving overshoot is what kills the drift). Recompute `loopEndSamp/loopStartSamp` from **live** bpm each block (never cache). Writes **only** `mSamplePos` — does NOT touch the anchor.
- **`setBPM`/`seekTo`/`start`/`reset`** (move `setBPM` from inline [:22](Source/Standalone/StandaloneApp.h:22) to `.cpp`): message-thread anchor writers, each through the seqlock. `setBPM` re-anchors `(deriveBeat(mSamplePos), mSamplePos, newBpm)`. `seekTo(beat)` sets `mSamplePos=llround(beat·spb)` + anchor `(beat, mSamplePos, bpm)`; sets `mSeekDiscontinuity` iff `beat < oldBeat`. `start`/`reset` per the anchor model below.
- **Loop-regime anchoring** (the single-writer trick): when the loop is (re)configured, the anchor is set so the **wrapped** `mSamplePos` derives the correct in-loop beat — i.e. anchor `= (loopStartBeat, loopStartSamp, bpm)`. Driven from the message thread (`setLoopBeats`/`setLoopStart` setters + `onGetLoopBeats` when params change). This is what lets `advanceBlock` wrap `mSamplePos` **without** writing the anchor (avoids a two-writer seqlock corruption). Tempo-change-while-looping self-heals within one `onGetLoopBeats` timer tick (documented edge).
- **`getPosition()`** ([:156](Source/Standalone/StandaloneApp.cpp:156)): `setPpqPosition(getCurrentBeat())` **and** `setTimeInSamples(mSamplePos.load())` (the standard field the scheduler reads for the exact integer straddle test).
- **New public accessors:** `std::atomic<bool>* getSeekDiscontinuityFlag()` (for wiring).

### Wiring — `Source/Standalone/StandaloneApp.cpp` + `Source/Standalone/StandaloneEditor.cpp`
- [StandaloneApp.cpp:342](Source/Standalone/StandaloneApp.cpp:342) (after `setPlayHead`): `mProcessor->setSeekDiscontinuityFlag(mPlayHead->getSeekDiscontinuityFlag())`.
- `onGetLoopBeats` ([StandaloneEditor.cpp:781-871](Source/Standalone/StandaloneEditor.cpp:781)): mirror loop-start into the processor — `mProcessor.mLoopStartBeats.store(startBeats)` at [:793](Source/Standalone/StandaloneEditor.cpp:793)/[:810](Source/Standalone/StandaloneEditor.cpp:810), `.store(0.0)` at [:820](Source/Standalone/StandaloneEditor.cpp:820). Also assert the loop-regime anchor here when params change.

### Scheduler — `Source/PluginProcessor.h` + `.cpp`
- [PluginProcessor.h:1014](Source/PluginProcessor.h:1014): **remove** `mPRLastBeatEnd`. Add `std::atomic<double> mLoopStartBeats{0.0}` + `std::atomic<bool>* mSeekDiscontinuity{nullptr}` + inline `setSeekDiscontinuityFlag(...)`.
- [PluginProcessor.cpp:1035-1561](Source/PluginProcessor.cpp:1035) — the refactor:
  - Read the exact integer clock: `const int64 samplePos = pos.getTimeInSamples().orFallback(0)`. Compute `loopEndSamp/loopStartSamp = llround(loopBeat·spb)` (same formula as the playhead). **Straddle predicate is integer:** `straddle = looping && samplePos < loopEndSamp && samplePos + numSamples > loopEndSamp`. Wrap sample-in-block = `loopEndSamp - samplePos` (exact).
  - Build a **`RollWindow[2]`** (1 normal, 2 on straddle) — see Task 1 code block.
  - **Shared helper** `scheduleRollWindows(...)` replaces the inline note-on bodies at the 7 pattern-mode engine sites ([1374-1560](Source/PluginProcessor.cpp:1374)) and inside the song-mode `scheduleRoll` lambda ([1166-1186](Source/PluginProcessor.cpp:1166)). Try-lock/active-flag guards (1422/1454/1481/1506/1542) unchanged.
  - **Off handling:** notes whose off overruns the loop → off fires at the exact **wrap sample** (sweep on straddle); a backward seek (`mSeekDiscontinuity` consumed) flushes pending offs (preserves today's cut-held-notes-on-seek). Removes the `jumped` flush + `mPRLastBeatEnd` write at [1277](Source/PluginProcessor.cpp:1277)/[1320](Source/PluginProcessor.cpp:1320)/reset at [1059](Source/PluginProcessor.cpp:1059).
  - **Sub-block-loop guard:** if `span < numSamples` (loop shorter than a block), fall back to window0 only + clamp (no stuck note); documented limitation.

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/virtual-moseying-cocoa.md` → `Plans & Specs/Batch Plans/virtual-moseying-cocoa.md` (Write); delete the home-dir copy.
- [ ] Add `**Plan file:** Plans & Specs/Batch Plans/virtual-moseying-cocoa.md` to the Main Plan §5 QA-Ed entry (replace the `<silly-name>.md` placeholder at line 1078).
- [ ] Seed `Plans & Specs/Running Notes/virtual-moseying-cocoa.md` (title / purpose blockquote / pair ref / convention ref + "Task 0: open" entry).
- [ ] Surface full git status. `/draft-commit`. Surface drafted message + status to Jeff. Commit on approval.

### Task 1 — Int-sample transport + tempo anchor + sample-accurate scheduler (THE atomic source commit, SC-2)

**1a — Playhead clock + seqlock anchor.**
- [ ] Rewrite `StandalonePlayHead` state + `advanceBlock`/`setBPM`/`seekTo`/`start`/`reset`/`getPosition` per Files-to-modify. Anchor reads/writes via a small seqlock (mirror the `SpectrumFeed` seqlock idiom referenced in CLAUDE.md):
```cpp
// READ (audio getPosition + message UI cursor):
double deriveBeat(int64_t s) const {
    uint32_t v; double ab, bpm; int64_t as; const double sr = mSampleRate.load();
    do { v = mAnchorSeq.load(std::memory_order_acquire);
         ab = mAnchorBeat; as = mAnchorSample; bpm = mAnchorBpm;
    } while (v & 1u || v != mAnchorSeq.load(std::memory_order_acquire));
    return ab + (double)(s - as) * bpm / (60.0 * (sr > 0 ? sr : 44100.0));
}
// WRITE (message thread only):
void publishAnchor(double beat, int64_t s, double bpm) {
    mAnchorSeq.fetch_add(1, std::memory_order_release);        // -> odd
    mAnchorBeat = beat; mAnchorSample = s; mAnchorBpm = bpm;
    mAnchorSeq.fetch_add(1, std::memory_order_release);        // -> even
}
```
- [ ] `advanceBlock` writes only `mSamplePos`; integer wrap preserving overshoot:
```cpp
void StandalonePlayHead::advanceBlock(int n, double sr) {
    mSampleRate.store(sr);
    if (!mPlaying.load()) return;
    int64_t pos = mSamplePos.load() + n;
    const double le = mLoopBeats.load();
    if (le > 0.0) {
        const double spb = 60.0 * sr / juce::jmax(1e-6, mAnchorBpm /*live bpm*/);
        const int64_t lE = (int64_t) std::llround(le * spb);
        const int64_t lS = (int64_t) std::llround(mLoopStart.load() * spb);
        const int64_t span = lE - lS;
        if (span > 0 && pos >= lE) pos = lS + ((pos - lS) % span);  // preserve overshoot
    }
    mSamplePos.store(pos);
}
```
- [ ] Loop-regime anchoring in the loop setters / `onGetLoopBeats`: when looping, `publishAnchor(loopStartBeat, loopStartSamp, bpm)` so the wrapped `mSamplePos` derives correctly without an audio-thread anchor write.

**1b — Wiring.**
- [ ] `setSeekDiscontinuityFlag` wire at StandaloneApp.cpp:342; `mLoopStartBeats` mirror in `onGetLoopBeats` (793/810/820).
- [ ] Remove `mPRLastBeatEnd` decl + its reset at 1059.

**1c — Scheduler window helper (both modes).**
- [ ] Build windows once after `beatStart/beatEnd` + integer `samplePos`:
```cpp
struct RollWindow { double winStart, winEnd; int sampleBase; };
std::array<RollWindow,2> win; int nWin = 0;
if (looping && span >= numSamples && samplePos < loopEndSamp && samplePos + numSamples > loopEndSamp) {
    const int wrapSmp = (int) juce::jlimit<int64_t>(0, numSamples-1, loopEndSamp - samplePos);
    win[nWin++] = { beatStart, loopEndBeat, 0 };
    win[nWin++] = { loopStartBeat, loopStartBeat + (beatEnd - loopEndBeat), wrapSmp };
} else {
    win[nWin++] = { beatStart, beatEnd, 0 };
}
```
- [ ] Helper (note fires in at most one window — the `break` is the structural cure for the old double-fire):
```cpp
// absMap: pattern -> note.startBeat ; song -> blkStartBeat + note.startBeat
// contentHi: song viewport (blkEndBeat) else +inf ; offHi: min(viewport, loopEndBeat)
for (const auto& note : notes) {
    if (note.muted) continue;
    for (int w = 0; w < nWin; ++w) {
        const double a = absMap(note);
        if (a >= contentHi) continue;
        if (a >= win[w].winStart && a < win[w].winEnd) {
            int smp = juce::jlimit(0, numSamples-1, win[w].sampleBase + (int)((a - win[w].winStart)/bs));
            emitPianoNoteOn(buf, note, smp);
            double off = juce::jmin(a + note.durationBeats, offHi);
            mPRPendingOffs.push_back({ off, note.midiNote, target });
            break;
        }
    }
}
```
- [ ] Replace the 7 pattern-mode inline note-on loops with one-line `scheduleRollWindows(...)` calls (guards unchanged). Rewire the song-mode `scheduleRoll` lambda to forward to the same helper (`songAbs`, `contentHi=blkEndBeat`, `offHi=min(blkEndBeat,loopEnd)`).

**1d — Off handling + seek flush.**
- [ ] In the per-block note-off pass: on a straddle, offs with `beatOff >= loopEndBeat` fire at the **wrap sample** (cut overruns exactly at the boundary; no leak). Consume `mSeekDiscontinuity` once/block → flush all pending offs (backward-seek cut). Remove the old `jumped` block.

**1e — Build + verify (Jeff).**
- [ ] Tell Jeff: "Run `do_build.bat`. Verify in Debug first, then Release." Then run the Verification scenarios below. Wait for results; iterate on failures.
- [ ] On pass: surface full git status. `/draft-commit`. Surface message + status. Commit (one atomic source commit). `/draft-doc running-notes` → apply.

### Close sequence
- [ ] `/draft-doc batch-close` → review → apply to `Implemented Work Log.md` (Edit).
- [ ] `/review-batch QA-Ed` → address BLOCKER/NEEDS-FIX; defer NITs into the close entry.
- [ ] Route side findings (Rule 3): in-batch → close-entry table; outside-batch → §9 Forks + §5/§6 edits (surface placement to Jeff).
- [ ] `/draft-commit` close commit (separate from the source commit — clean rollback boundary).

---

## Verification (end-to-end smoke — by ear, distinctive test patterns)

Use an audibly distinctive **beat-0 note** (loud click/rim) so a drop = missing click and a double = flam.
1. **Drift / clip-sync (the core fix):** song mode, a pattern note at beat 0 + an audio clip at beat 0 on another row; play a long arrangement (≥64 bars). They stay locked start-to-finish (today: pattern drifts late).
2. **Loop-wrap first note (pattern):** loop a 2-bar pattern with the beat-0 click + a last-1/16 note; loop ~1-2 min. Click present every loop, no flam. Repeat at several buffer sizes (64/128/512) and an awkward BPM (e.g. 137.3) so `loopEnd·spb` lands near a half-sample.
3. **Loop-wrap first note (song-loop):** song-loop on; same check at the song-loop seam.
4. **Time-selection loop:** select bars 3–5 (nonzero loop-start) in both Builder (song) and a piano roll (pattern); the loop-start note fires every wrap, correct notes only.
5. **Tempo automation:** a `global_tempo` ramp across a loop seam; no beat glitch / cursor jump; notes stay aligned.
6. **Seek:** scrub backward while a long note holds → note cuts (matches today). Forward seek → no stuck note. Seek to 0 / Stop → clean.
7. **Held note across loop:** a whole-note overrunning the loop end, looped 100×; no monotonic voice-count growth (no stuck notes).
8. **Pattern/song parity + count-in:** same pattern in both modes sounds identical; record count-in still aligns bar 1.

If by-ear is inconclusive on #2/#3, add a **temporary Debug-only** fire-count check (cataloged under Rule 4, stripped at close) — surface to Jeff before adding.

---

## Routing notes (Rule 3 application during execution)
- **In-batch finds** → close-entry routing table.
- **Verify-watch (NOT deferred spec calls):** (i) **song-loop held-note release** — today's song path doesn't flush offs across a song-loop wrap (pre-existing); left as-is. If a note hangs across a song-loop in verify, fix in-batch per `feedback_qa_batches_fix_bugs_dont_defer.md`. (ii) **sub-block loops** (`span < numSamples`) — degenerate; graceful window0-only fallback, documented limitation; multi-wrap-per-block is a future batch if ever needed.
- **Outside-batch finds** → §9 Forks entry + §5/§6/Future State edits; surface placement options to Jeff (don't pick the slot).

## Carry-Forward Reference touch points
- §1 **BlockContext** ([:71-77](Plans & Specs/Carry-Forward Reference.md:71)) — `bpm`/`posInfo` flow into the MT render path; keep consistent (single MT path now; `timeInSamples` newly populated). Read at 1c.
- §2 **STATE-04** ([:144-149](Plans & Specs/Carry-Forward Reference.md:144)) — playhead "keeps ticking" unless stopped; confirm project-load stop still holds with the int-sample clock (Verify #6). Read at 1a.
- Transport/scheduler are otherwise **greenfield** in the frozen snapshot (confirmed 2026-06-01) — source is the authority.

## Diagnostic Instrumentation Catalog (Rule 4)
None planned (verify by ear). If a temporary fire-count check is added for #2/#3, log it here (Site / Tag / Purpose / Disposition=Remove at batch close) in the running-notes file in the same edit pass, and strip at close after surfacing the list to Jeff.
