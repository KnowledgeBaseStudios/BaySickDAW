# QA-ClipPlayback — Timeline-WAV Player Controls + Per-Grid-Row Clip Mute — Plan (memoized-inventing-flask)

> **Canonical path** (mirrored after ExitPlanMode + approval):
> `Plans & Specs/Batch Plans/memoized-inventing-flask.md`
> Paired running notes: `Plans & Specs/Running Notes/memoized-inventing-flask.md`

> **For execution:** use `superpowers:executing-plans` inline. Steps use `- [ ]` checkbox syntax. Builds are Jeff's (`do_build.bat`) — never Claude's. Verify in the Debug exe FIRST (watch for `jassert` dialogs — screenshot), then Release (per CLAUDE.md Build System standing rule + `feedback_walk_jeff_through_debug`).

---

## Context

Two **pre-existing** clip-drop findings in the ClipsPage-BaySickPlayer subsystem, surfaced during the QA-MultiBlockHazard Task-1 verify and routed here at that batch's close (Main Plan §9 fifty-third Forks entry; §5 docket lines 1201-1207). Neither was caused by QA-MultiBlockHazard's summing work (`git diff` + `git blame` confirmed audio-only, untouched mute/decode lines). Jeff bundled both into one batch — "kind of all one thing," same subsystem.

**Finding 1 (feature build) — Player controls dead on timeline-WAV playback.** A ClipsPage clip is dual-purpose by design: play it from the piano roll → it's a **sampler** (MIDI-triggered through the ClipsPage BaySickPlayer engine, "Flow A"); play it as a Builder timeline block → it's an **editable WAV** (decoded off disk by `renderAudioClipsForRow`, "Flow B"). Flow A honors the BaySickPlayer's Player controls (volume/pan/pitch/filter/tone/width/ADSR + more); Flow B applies only `masterGain` + a 5 ms declick and sums raw. It's the playback **MODE** that gates the knobs, not the add-path — both add-paths (Builder drop + Clip-tab dropdown) behave identically. The WAV half of the dual-purpose design is only half-wired. Fix = wire the full applicable Player control set into the decode path, read live from the ClipsPage engine and applied to each decoded clip **on the way out** (before the raw sum), while keeping the existing BPM stretch + slip-trim intact.

**Finding 2 (bug) — Builder-grid mute keys on the owner page, not the grid row.** In `renderAudioClipsForRow` the per-clip audibility check `isRowAudible(row)` passes `row` = the clip's **owner page** (audioInsert index, from the route-by-owner refactor `c616f0d` 2026-06-02) where it should pass the clip's **grid row**. Two clips on one player page → both resolve to the same audibility → muting either grid track silences both. Fix = key that check on `player.trackRow`; the mixer **strip** mute (`audioRowMute[row]`) + routing stay owner-keyed.

**Bucket:** Players, System Pages.
**Risk:** MEDIUM. Finding 2 is a one-line row-key swap (gate verified). Finding 1 is a real DSP build in the hot audio path under MT — the length-preserving pitch-shift (Task 3) is the highest-risk piece since it must coexist with the existing phase-vocoder BPM stretch.
**Effort estimate:** ~6-9h across 3 source tasks + Jeff's Debug→Release verify cycles (verification-heavy on Task 2/3).
**Dependencies:** none. QA-MultiBlockHazard closed at `014d4d8`; working tree clean.

---

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning / Source |
|----|----------|--------------------|
| S1 | **Control set = all applicable BaySickPlayer knobs.** Task 2 (buffer-level): volume, pan, filter (cutoff+res), tone (treble), width (stereo), ADSR, drive, reduction (reduct), muffle, hardness, sample-start, reverse. Task 3 (pitch domain): tune, detune, vibrato LFO (amt+rate). Velocity-driven routings (velToVolume / velToMuffle / velToHardness / sensitivity) are **inert** on a timeline clip (no note velocity). | Jeff 2026-07-03 ("All applicable knobs") + QA-MultiBlockHazard session ("essentially all Player controls can map"). |
| S2 | **Signal flow: read LIVE, apply per-clip before the sum.** Player values read each block from the row's ClipsPage engine (the task's `mClipEngine`, cast to `VibePlayerProcessor*`), applied to each decoded clip in `clipScratch` **before** the raw add into the row buffer. Live (knob moves heard immediately, like the sampler path); per-clip-in-scratch avoids double-processing Flow A's engine output; stretch + trim preserved. | Jeff: "the player would affect the output past where the clip sends its signal from"; QA-MultiBlockHazard session ("read from the engine, applied on the way out, keeping stretch+trim"). |
| S3 | **ADSR = full A/D/S/R, clip-level.** Attack = fade-in at clipStart, release = fade-out ending at clipEnd, decay/sustain shape the middle. Supersedes the existing 5 ms declick when active; declick stays as the anti-click floor when attack/release are ~0. | QA-MultiBlockHazard session decision (verbatim: "attack maps to a fade-in at the clip's start and release to a fade-out at its end (decay/sustain shape the middle)"). |
| S4 | **Pitch = length-preserving pitch-shift.** The pitch/tune/detune knobs shift pitch WITHOUT changing the clip's grid length (not a tape-speed change), coexisting with the BPM time-stretch. | Jeff 2026-07-03 ("make both work in full") + the locked "keep stretch + trim" constraint (tape-speed pitch would break trim). Jeff offered a veto if he actually wants tape-speed; none given. |
| S5 | **Task structure = T1 mute fix / T2 feature core / T3 pitch / T4 close.** Pitch isolated to its own commit because it's the highest-risk DSP. | Jeff 2026-07-03 ("Mute fix, then feature in two stages"). |
| S6 | **Both findings in ONE batch.** | Jeff at QA-MultiBlockHazard close ("kind of all one thing"). |
| S7 | **Engine-type guard.** Player controls apply only when the ClipsPage engine is a `VibePlayerProcessor` (BaySickPlayer). A NAM/IR or null clip engine leaves the decode raw (current behavior). | Defensive default — `mClipEngines` can hold non-BaySickPlayer engines (CLAUDE.md: "VibePlayer / BaySickNAM/IR instances owned by ClipsPage tabs"); a non-sampler engine has no Player knobs to read. |
| S8 | Plan-file silly-name = `memoized-inventing-flask` (plan-mode runtime-assigned; running-notes file matches). | Per `feedback_silly_name_is_my_pick.md` — not a Jeff spec call. |

---

## Sub-spec calls surfaced for ExitPlanMode

**None open.** Every design decision (control set, signal-flow architecture, ADSR mapping, pitch semantics, task/commit structure) was surfaced via chat and locked BEFORE this plan body was written (see S1-S5). Implementation-shape choices below (exact DSP order, where per-clip state lives, param-read mechanism) match the existing engine and are not spec calls.

Two implementation risks that could surface a NEW spec call **mid-execution** (will pause + ask, per Rule 5, not pre-decide):
- **Task 3 pitch feasibility.** If length-preserving pitch-shift can't be made clean atop the current phase vocoder at acceptable CPU, Task 3 opens with a feasibility spike; if it fails, surface options to Jeff (accept tape-speed fallback / dedicated pitch-shifter / defer pitch) rather than silently shipping a compromise.
- **`sampleStart` vs slip-trim overlap + `reverse` vs stretch (Task 2).** `sampleStart` (skip-into) composes additively with the existing `contentStartSamples` slip-trim; `reverse` reverses a possibly-stretched read. If either proves nonsensical on a timeline clip during implementation, surface to Jeff rather than silently dropping (they're in scope per S1's "all applicable").

---

## Files to modify

### Task 1 — Finding 2 (per-grid-row mute)
- [Source/PluginProcessor.cpp:457](Source/PluginProcessor.cpp:457) — in `renderAudioClipsForRow`, change `builderRowMuted` to key on `player.trackRow` instead of `row`. One line. `rowMuted` (line 456, strip mute, owner-keyed) unchanged.

### Task 2 — Finding 1 core (buffer-level controls, no pitch)
- [Source/Engine/Tasks/CompositeAudioInsertTask.h](Source/Engine/Tasks/CompositeAudioInsertTask.h) / [.cpp](Source/Engine/Tasks/CompositeAudioInsertTask.cpp) — `setClipEngine` caches a `VibePlayerProcessor*` (mirroring the existing `mScEngine` dynamic_cast cache, `.cpp:22-27`); `run()` passes it into the clip context (`.cpp:124-134`).
- [Source/PluginProcessor.h](Source/PluginProcessor.h) — (a) extend `AudioClipBlockContext` with a `VibePlayerProcessor* clipPlayer` field; (b) add per-clip DSP state to the `AudioClipPlayer` struct (`:492-531`) — a per-channel filter (`juce::dsp::StateVariableTPTFilter<float>`) + treble-shelf/muffle state, mirroring the existing per-clip `vocoder`/`pvInBuf` members. Reset at `rebuildAudioClipPlayers` time (state resets on clip edit — acceptable, matches the vocoder).
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — in `renderAudioClipsForRow` (`:404-634`), after the decode into `clipScratch` (`:523-595`) and folded with the existing declick (`:597-617`), apply the Player-control chain to `clipScratch` before the raw sum (`:619-629`); read control values once at function top from `ctx.clipPlayer->apvts`. Possibly a small file-local DSP helper. `rebuildAudioClipPlayers` (`:2363`) inits per-clip filter state.

### Task 3 — Finding 1 pitch (length-preserving)
- [Source/PluginProcessor.h](Source/PluginProcessor.h) — per-clip pitch-shift state on `AudioClipPlayer` (or reuse the existing `vocoder`).
- [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) — combine the tune/detune ratio + vibrato LFO into the decode read/stretch math in `renderAudioClipsForRow`, preserving grid length.
- [Source/DSP/PhaseVocoder.h](Source/DSP/PhaseVocoder.h) / [.cpp](Source/DSP/PhaseVocoder.cpp) — only if the vocoder needs a pitch-shift entry point beyond its current stretch API (confirm at Task 3 open; prefer reuse over new DSP).

---

## Tasks

### Task 0 — Open commit
- [ ] Mirror `~/.claude/plans/memoized-inventing-flask.md` → `Plans & Specs/Batch Plans/memoized-inventing-flask.md` (Write); delete the home-dir copy (plan-file hygiene).
- [ ] Add `**Plan file:** [Plans & Specs/Batch Plans/memoized-inventing-flask.md](Plans & Specs/Batch Plans/memoized-inventing-flask.md)` to the Main Plan §5 QA-ClipPlayback docket; add `**STATUS: OPEN.**` + effort estimate.
- [ ] Seed `Plans & Specs/Running Notes/memoized-inventing-flask.md` per §0 required sections (title / purpose blockquote / pair ref / convention ref / empty Diagnostic Instrumentation Catalog / Task 0 entry).
- [ ] Surface full `git status`. Write the brief one-liner directly (Rule 9 — skip `/draft-commit`). Surface message + status; commit on approval.

### Task 1 — Finding 2: per-grid-row Builder mute
- [ ] **Pre-step (read-before-change):** confirm `PatternManager::isRowAudible(int)` reads the **builder-grid** row mute/solo (grep/read; lines 1263/1446 already pass `blk.trackRow` to it, so it takes a grid row). Confirm `audioRowMute` is indexed by the **owner/audioInsert** index (line 456, `row`). Confirm the parallel `decodeFilePlayClip` Vox/Inst path already keys on `player.trackRow` (`:671-674`) so it needs no change.
- [ ] Change the one line (PluginProcessor.cpp:456-457):
```cpp
const bool rowMuted        = mx.audioRowMute[(size_t) row];
const bool builderRowMuted = ! mPatternManager->isRowAudible (player.trackRow);   // was isRowAudible (row)
```
- [ ] Rule 6: fix the stale line-443 comment ("`row` is therefore the owner row below, so the mute/strip checks already key on the clip's own strip") — it's now only half-true (strip mute owner-keyed; grid mute grid-row-keyed).
- [ ] **Tell Jeff:** "Run `do_build.bat`, Debug first then Release. Mute test:
  - **(1)** One Clips page. Add two audio clips to it and drag them onto **two different Builder grid rows**. Play in song mode.
  - **(2)** Toggle grid-row-A's track mute → only clip A silences; clip B keeps playing. Toggle grid-row-B's mute → only clip B silences. (Before: muting either row killed both.)
  - **(3)** The Clips **mixer strip** mute still silences the whole page (both clips) — owner-level mute unchanged.
  - **(4)** Move a clip to a different grid row, then re-test its mute follows the new row."
- [ ] On pass: brief one-liner, surface message + `git status`, commit on approval.
- [ ] `/draft-doc running-notes` → apply to the running-notes file.

### Task 2 — Finding 1 core: Player controls into the WAV decode path (no pitch)
- [ ] **Plumb the engine into the decode context.**
  - `CompositeAudioInsertTask::setClipEngine`: cache `mClipPlayer.store(dynamic_cast<VibePlayerProcessor*>(engine))` alongside the existing `mScEngine` cache (`.cpp:22-27`) — avoids a per-block dynamic_cast on the audio thread.
  - `run()`: set `clipCtx.clipPlayer = mClipPlayer.load(acquire)` (`.cpp:~134`).
  - Extend `AudioClipBlockContext` (PluginProcessor.h) with `VibePlayerProcessor* clipPlayer = nullptr;`.
  - **Thread-safety:** reads of `clipPlayer->apvts.getRawParameterValue(id)->load()` are lock-free atomic reads; the pointer piggybacks the existing `mClipEngine` lifecycle (already deref'd in `run()` for `processBlock`), so no new lifetime risk.
- [ ] **Add per-clip DSP state** to `AudioClipPlayer` (mirror the `vocoder`/`pvInBuf` pattern): per-channel `StateVariableTPTFilter` (filter) + one-pole treble-shelf state + any muffle/reduction state. Init in `rebuildAudioClipPlayers` (`:2363`) at `prepareToPlay` sample rate.
- [ ] **Apply the control chain** in `renderAudioClipsForRow`, per clip, to `clipScratch` before the raw sum. Read values once at function top (guarded on `ctx.clipPlayer` per S7 — null → today's raw decode). DSP order matches `VibeVoice`/`VibeSynth::renderNextBlock`: filter → drive → reduction/muffle/hardness → volume → pan → width (M/S) → treble shelf, ADSR as the amplitude envelope. Sketch:
```cpp
// once, top of fn:
const auto* pl = ctx.clipPlayer;
float vol=1, pan=0, cutoff=20000, res=0.7f, treble=0, width=1, atk=0, dec=0, sus=1, rel=0;
if (pl) {
    auto rd = [pl](const char* n){ return pl->apvts.getRawParameterValue (pl->pid (n))->load(); };
    vol=rd("volume"); pan=rd("pan"); cutoff=rd("cutoff"); res=rd("res");
    treble=rd("treble"); width=rd("stereo");
    atk=rd("attack"); dec=rd("decay"); sus=rd("sustain"); rel=rd("release");
    /* + drive / reduct / muffle / hardness / sampleStart / reverse */
}

// per clip, after decode (~:595), before the sum (~:619):
if (pl) {
    player.filterL.setCutoffFrequency (cutoff); player.filterR.setCutoffFrequency (cutoff);
    player.filterL.setResonance (res);          player.filterR.setResonance (res);
    const float pL = std::cos ((pan+1)*0.25f*pi), pR = std::sin ((pan+1)*0.25f*pi);
    for (int s = 0; s < outSamples; ++s) {
        const float env = clipAdsr (outPosInClip + s, clipLenOutSamples, atk, dec, sus, rel);
        float l = player.filterL.processSample (0, clipScratch.getSample (0, bufOffset+s));
        float r = clipScratch.getNumChannels() > 1
                    ? player.filterR.processSample (1, clipScratch.getSample (1, bufOffset+s)) : l;
        applyWidthTreble (l, r, width, treble, player);   // M/S width + one-pole treble shelf
        clipScratch.setSample (0, bufOffset+s, l * vol * env * pL);
        clipScratch.setSample (1, bufOffset+s, r * vol * env * pR);
    }
}
```
`clipAdsr` ramps attack over `atk`s at clipStart, decays to `sus` over `dec`s, holds, releases to 0 over `rel`s ending at clipEnd — supersedes the 5 ms declick when active; declick stays the floor at a/r ≈ 0.
- [ ] **`sampleStart` / `reverse` interaction (S1 fiddly set):** `sampleStart` composes additively with the existing `contentStartSamples` slip-trim; `reverse` reverses the decoded read within the clip window. Implement; if either reads as nonsensical on a timeline clip, PAUSE and surface to Jeff (do not silently drop — Rule 5).
- [ ] **No audio-thread allocation:** all per-clip DSP state pre-sized at rebuild / `prepareToPlay`; `clipScratch` already reused. Confirm no `setSize`/`new` in the render loop.
- [ ] **Tell Jeff:** "Run `do_build.bat`, Debug first then Release. Player-control test (pitch is the NEXT task — don't test pitch yet):
  - **(1)** Drop a WAV on Builder, play in **song mode**. Turn the BaySickPlayer **volume** — you now hear it shape the WAV live (was dead before). Same for **pan, filter (cutoff/res), tone, width**.
  - **(2)** **ADSR** — set a slow attack: the clip fades in at its start. Set a release: it fades out at its end. Decay/sustain shape the middle.
  - **(3)** **Regression:** the same sound played from the **piano roll** (sampler) is unchanged — Flow A untouched.
  - **(4)** **Regression:** a clip on a Clips page with NO knob changes sounds the same as before this batch (defaults = transparent).
  - **(5)** drive / reduction / muffle / hardness / sample-start / reverse each do something sensible on the WAV (tell me if any is weird — sample-start + trim and reverse + stretch are the interaction-risky ones)."
- [ ] On pass: brief one-liner, surface + commit on approval. `/draft-doc running-notes` → apply. Log any diagnostics added to the Catalog **in the same edit pass** (Rule 4).

### Task 3 — Finding 1 pitch: length-preserving pitch-shift
- [ ] **Open with a feasibility read** (per the Sub-spec-calls risk note): how the existing `PhaseVocoder` stretch path (`renderAudioClipsForRow:495-595`) can also pitch-shift while keeping grid length. Standard technique: pitch-shift = resample by 2^(semitones/12) then time-stretch by the inverse to restore length — combine the tune/detune ratio with any active BPM stretch ratio. Confirm the vocoder can express this (or needs a pitch-shift entry point); prefer reuse.
- [ ] **If clean:** wire tune + detune (→ one pitch ratio) + vibrato LFO (amt/rate → per-sample pitch modulation) into the decode read/stretch math, preserving `[clipStart, clipEnd)` grid length. Per-clip pitch state on `AudioClipPlayer`.
- [ ] **If NOT clean at acceptable CPU:** PAUSE — surface options to Jeff (tape-speed fallback / dedicated pitch-shifter / defer pitch to a follow-up). Do not ship a silent compromise.
- [ ] **Tell Jeff:** "Run `do_build.bat`, Debug first then Release. Pitch test:
  - **(1)** Drop a WAV on Builder, play in song mode. Raise **pitch/tune** — the pitch rises but the clip's **length and position on the grid stay put** (not sped up).
  - **(2)** **Stretch + trim still work** with pitch engaged — a BPM-stretched, slip-trimmed clip also honors pitch, all three compose.
  - **(3)** **detune** (fine cents) + **vibrato** (LFO) shape pitch as expected.
  - **(4)** Regression: pitch = 0 → identical to end-of-Task-2."
- [ ] On pass: brief one-liner, surface + commit on approval. `/draft-doc running-notes` → apply. Update the Diagnostic Catalog.

### Task 4 — Close sequence
- [ ] `/draft-doc batch-close` — compile the Implemented Work Log entry from the running-notes file.
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit (parent session, not the agent).
- [ ] `/review-batch QA-ClipPlayback` — diff vs this plan + CLAUDE.md rules + memory gotchas. Address BLOCKER / NEEDS-FIX in-batch; defer NITs into the close entry.
- [ ] **Strip diagnostics:** walk the running-notes Diagnostic Instrumentation Catalog; surface the `Remove` strip list to Jeff for approval, then strip.
- [ ] **Route side findings (Rule 3):** resolved-in-batch → close-entry routing table; outside-batch → §9 Forks entry + §5/§6/Future State edits (surface slot options to Jeff, don't pick).
- [ ] Main Plan §5 QA-ClipPlayback docket → `STATUS: CLOSED` + Work Log pointer. §6 arrow / QA-CutSelfReview sequencing already point past this batch (no re-point needed).
- [ ] Surface full `git status`. Write the brief close-commit one-liner directly; commit the close separately from source commits (clean rollback boundary), on approval.

---

## Verification (end-to-end smoke)

After Task 3 commit lands, one combined Debug+Release pass:
1. **Build clean.** `do_build.bat` Release + Debug both green.
2. **Per-grid-row mute** (Task 1): two clips on one Clips page, two grid rows, independent mutes; strip mute silences the page.
3. **WAV honors Player controls** (Task 2): a Builder-played WAV tracks volume/pan/filter/tone/width/ADSR live; sampler (piano-roll) path unchanged; defaults transparent.
4. **Length-preserving pitch** (Task 3): pitch shifts tone without changing grid length; stretch + trim compose with pitch.
5. **No regression** on non-Clips audio rows, Vox/Inst FilePlay, or the QA-MultiBlockHazard single-pass/overlap behavior.
6. **MT parity** (per QA-Md — MT works in Debug AND Release): the control chain runs identically serial vs MT; verified in Jeff's normal Debug→Release cycle (no separate MT gate).

---

## Routing notes (Rule 3 application during execution)

- Findings scoped to the ClipsPage-BaySickPlayer clip-drop subsystem → fold here.
- Findings in adjacent-but-separate surfaces (e.g. the Vox/Inst FilePlay decode, BUILD-06 resize-staleness at QA-H, multi-clip stacking DSP-06 at QA-J) → §9 Forks + the owning batch; surface slot to Jeff, don't pick.
- Every `DBG`/`Logger`/temp `jassert`/debug `AlertWindow`/temp-file trace → a Diagnostic Instrumentation Catalog row in the running-notes file IN THE SAME EDIT PASS (Rule 4); strip `Remove` entries at task/batch close after Jeff approves the list.

---

## Carry-Forward Reference touch points

Read at the noted task starts (Carry-Forward is the frozen 2026-05-07 snapshot — reconcile names against current source via the Work Log; the composite/decode task types were renamed since):
- **§1 (Task subclasses) + §4 (DSP-12 fix shape "sum before insert DSP")** — the Flow A (sampler-MIDI) / Flow B (timeline-WAV) split + the composite-sum decision QA-ClipPlayback builds on. Read at Task 2 open.
- **§2 (AudioClipSnapshot RCU) + §3 (`onAudioClipAdded` drop→owner cascade)** — how clips are owned lock-free + the route-by-owner ownership Finding 2 keys off. Read at Task 1 + Task 2 open.
- **§8 (Diagnose before fixing)** — clip-silence/attenuation bugs get A/B + mute diagnostics from Jeff before any speculative fix. Standing.
