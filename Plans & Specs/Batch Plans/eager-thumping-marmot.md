# QA-L-Fix — Per-Drum MIDI Kit Triggers (MIDI Learn + note/CC + play-pitch) — Plan (eager-thumping-marmot)

> **Canonical path:** `Plans & Specs/Batch Plans/eager-thumping-marmot.md`.
> **For execution:** implementation plan for the QA-L per-drum-MIDI redesign. The design was locked
> with Jeff in a full workshop on 2026-07-19 — **every decision below is already answered; do NOT
> re-litigate or re-ask them.** Executed in a dedicated session. Lands in the G3 boundary work
> **before** the boundary commit (it is a G3 defect, it blocks that commit).

## Context

QA-L (#10, commit `2e2df50a`) shipped "per-drum MIDI notes with kit fan-out." Surfaced at the G3
boundary on 2026-07-19: **the feature does not do the job it exists for.**

**What is actually in the tree (source-verified 2026-07-19):**
- `mixer_drum_{N}_inputNote` — ONE note number, default `-1`, picked from an octave submenu,
  **note-only**. Menu at [DrumPage.cpp:1104](Source/Standalone/DrumPage.cpp:1104); param registered
  at [PluginProcessor.cpp:5711](Source/PluginProcessor.cpp:5711) (`addI(..., -1, 127, -1)`).
- Dispatch at [PluginProcessor.cpp:2756](Source/PluginProcessor.cpp:2756): an incoming **note** fans
  out to drums whose `inputNote` matches — but **only while a drum tab is focused** (`kind == 3`),
  and the loop **skips the focused drum** (`if (di == idx) continue;`).
- Net behavior: on a drum's OWN tab it plays chromatically and its `inputNote` is ignored; the
  assignment only does anything while a *different* drum tab is focused.
- **No CC support anywhere.** No MIDI Learn.

**Why this is a defect and not a preference:** the kit-trigger workflow the feature exists for is
**unreachable on the kit**. The fan-out only ran while a *drum tab* held MIDI focus, and it skipped
the focused drum — so with the Drum Kit selected (the surface you actually play a kit from) it fired
nothing at all, and on a drum's own tab the engine just played chromatically with the assignment
ignored. Jeff's call (2026-07-19): this is a fix, not new scope, and it **blocks the G3 boundary
commit**.

> **Premise correction (2026-07-19, mid-Task-2).** This section originally read "Jeff's drum pads
> send **CC**, not notes. The feature therefore fires *nothing* on the target hardware." **That was
> wrong.** Hardware-tested on the target controller (Novation FLkey 61) using shipped app behavior:
> (A) right-click knob -> MIDI Learn -> hit a pad -> **no capture**, and the learn queue admits only
> CC / pitch-bend / aftertouch, so the pads are **not sending CC**; (B) select a Layer with an
> instrument in the Piano Roll -> hit a pad -> **it plays**, so the pads **are sending notes** and
> they do reach the app. Matches Novation's own documentation, which describes FLkey pads as
> Note-based in every mode (the CC "Type" selector exists on the sibling Launchkey MK4, not FLkey).
> The defect is real either way — for the focus/skip reason stated above, which is hardware-
> independent — but the original stated cause was not. **Consequence:** D-8 split note vs CC
> triggers on the assumption CC was the primary path; notes are in fact the *only* path on this
> hardware, which makes D-8's focus gate the whole feature rather than a fallback. Re-posed to Jeff
> before implementation — see D-16.

**Risk:** medium — new trigger subsystem + audio-thread dispatch + a play-pitch semantics change.
**Effort:** ~6-10h.
**Dependencies:** none. The 12 G3 boundary review-fixes are already in the working tree and build
clean (Debug + Release, confirmed by Jeff 2026-07-19). Do not disturb them.

## Architecture already confirmed — do NOT re-derive

- **The drum kit and the 16 drum pages are the same data.** `DrumKitGrid` reads/writes
  `mPM->currentPattern().drumRolls[pageIdx].notes` — the *same* array the drum page's piano roll
  edits ([DrumKitGrid.cpp:500](Source/Standalone/DrumKitGrid.cpp:500),
  [:556](Source/Standalone/DrumKitGrid.cpp:556)). Two views, one dataset. The kit↔page display link
  already works for free; do not build a sync layer.
- **The kit grid is pitch-agnostic** — it only touches `startBeat`/`durationBeats`, never a note's
  pitch. A note at any pitch shows as a hit on that drum's kit row.
- **`DrumPage::showContextMenu (anchor)` is SHARED** by the page sound-picker
  ([DrumPage.cpp:1029](Source/Standalone/DrumPage.cpp:1029)) AND the kit-graphic right-click
  ([StandaloneEditor.cpp:6190](Source/Standalone/StandaloneEditor.cpp:6190) and
  [:6308](Source/Standalone/StandaloneEditor.cpp:6308) — both commented "without leaving the kit",
  reached when `isEngineLocked()`).
- **The processor already knows the focused surface** via `mLiveMidiTargetKind` /
  `mLiveMidiTargetIndex` ([PluginProcessor.cpp:2706-2716](Source/PluginProcessor.cpp:2706)):
  1=Layer, 2=Bass, 3=Drum-page, 4=Clip, 7/8=Inst, 9=Rusty. There is **no** "drum kit focused" state
  yet — Task 2 adds one.
  *(Execution correction 2026-07-19: it already existed. `EngineKind::DrumKit` is `0`, and
  `StandaloneEditor` pushes it into `mLiveMidiTargetKind` on every engine selection including the
  boot-time push — so `mLiveMidiTargetKind == 0` IS "kit focused". Task 2 reused it; no new state was
  added. See the superseded checkbox in Task 2.)*
- **`MidiLearnRegistry`** (`Source/MidiLearn/`) learns **CC / PitchBend / ChannelPressure only**
  (no Note type — see its `MessageType` enum) and dispatches to **APVTS param values** via
  `setValueNotifyingHost`. It is **not** a drop-in for note-or-CC *triggers*. Its learn-capture UX
  (`MidiLearnUI::beginLearn` → registry captures the next matching event → commits, 30 s timeout,
  dashed outline overlay) is a good **pattern to mirror**.

## Spec calls already locked (Jeff, 2026-07-19 workshop — do NOT re-ask)

| ID | Call | Decision |
|----|------|----------|
| D-1 | Scope | The mapping is for the **drum KIT** — playing the kit live off a pad controller. It has **nothing** to do with the per-drum piano rolls. |
| D-2 | Where the UI lives | **Kit only.** Split the shared `showContextMenu`: MIDI items render for the kit entry points, NOT the page picker. Pages get neither item. |
| D-3 | Menu contents (kit) | Keep **"MIDI Note"**; add **"MIDI Learn"** directly BELOW it. |
| D-4 | "MIDI Note" meaning | The drum's **play pitch** — what the drum sounds at. Default **C5 (60)**. Changing it pitches the drum up/down. |
| D-5 | Menu label | Replace the "Unassigned" item with **"Assigned: <note>"** showing the current note. Every drum starts assigned to C5 (param default `-1` → `60`). |
| D-6 | Changing the play pitch | Re-pitches that drum's existing hits **that sit at the OLD assigned note** to the new note (undoable), so the page roll shows the true played pitch. Hits deliberately placed at other pitches on the page roll **stay put**. |
| D-7 | Trigger input | **MIDI Learn** captures a **note OR a CC** as the drum's trigger. **One trigger per drum.** |
| D-8 | Focus rule | **CC triggers fire globally** (any focus). **Note triggers fire ONLY while the kit is the focused surface** — on a page the note plays that engine normally, so it never double-fires. |
| D-9 | What a trigger plays | Fires the drum at its **assigned MIDI Note** (default C5). The learned note/CC is only the *input*, never the pitch. |
| D-10 | Learn-a-note prompt | When the learned trigger is a **note** (not CC), prompt **"Also set this drum's play note to \<note\>?"** (show the note name), Yes/No. Yes → assigned note = learned note (D-6 re-pitch applies). **No prompt for CC.** |
| D-11 | Velocity | From the controller (note-on velocity / CC value). Plus a **global** app-wide toggle **"MIDI trigger velocity: From controller / Fixed"**; Fixed = the standard drawn-note velocity, for non-velocity-sensitive gear. |
| D-12 | Recording | Nothing new — triggers just **play**. Existing MIDI recording captures the live play if armed. Do not build a record path. |
| D-13 | Implementation shape | Jeff's explicit call: **this one is the agent's decision, not his.** Drum triggers get their **own binding store + capture + dispatch**, a sibling to `MidiLearnRegistry` (which is param-value-only). Do not shoehorn drum triggers into the param registry. |
| D-14 | Binding persistence | Saves **with the project** (part of the kit setup, like the assigned note). A global "make these my defaults" can come later; **not in scope**. |
| D-15 | Kit-row audition pitch | **Follows the drum's assigned play note** (`DrumPage::getPlayNote()`), so the press-and-hold row preview matches how the drum sounds everywhere else. *Not from the workshop* — surfaced during Task 1 execution (the two audition dispatchers hardcoded C5 with the comment "60 = C5 = the kit-grid placement note", the exact assumption D-4 made per-drum) and answered by Jeff in chat 2026-07-19. |
| D-16 | Note-trigger focus gate, re-looked after the premise correction | **D-8 stands: notes fire only while the Drum Kit is the focused engine.** *Not from the workshop* — D-8 split note vs CC assuming CC was the primary path; the premise correction made notes the ONLY path, so the focus gate went from governing a fallback to governing the whole feature. Re-posed 2026-07-19 with options (a) as-locked / (b) fire globally scoped to the pads' MIDI channel / (c) fire globally with the note consumed; Jeff answered **(a)**. Option (b) stays available if the pads turn out to transmit on a different channel than the keys — the MIDI Learn menu label shows each binding's channel, so it is checkable at the boundary smoke. |

## Sub-spec calls surfaced for ExitPlanMode

**No sub-spec calls open.** Every decision is locked above. If execution surfaces a genuinely NEW
spec call, **ASK Jeff in chat** (numbered options, lettered choices, no recommendations) and wait —
bulk-run ask-always rule. Never code past a call on a guess. Do not re-ask anything in the D-table.

*Execution update 2026-07-19:* Task 1 surfaced exactly one such call — the kit-row audition pitch.
Asked in chat, answered, and folded into the table above as **D-15**. The D-table remains the single
source of truth for this batch's design.

## Files to modify

**Task 1**
- [Source/Standalone/DrumPage.h](Source/Standalone/DrumPage.h) — `showContextMenu` signature (+ `fromKit`).
- [Source/Standalone/DrumPage.cpp:1029](Source/Standalone/DrumPage.cpp:1029) (page caller passes `false`), [:1104](Source/Standalone/DrumPage.cpp:1104) (MIDI Note block → kit-gated + "Assigned:" label), [:1152](Source/Standalone/DrumPage.cpp:1152) (result handler).
- [Source/Standalone/StandaloneEditor.cpp:6190](Source/Standalone/StandaloneEditor.cpp:6190), [:6308](Source/Standalone/StandaloneEditor.cpp:6308) — kit callers pass `true`.
- [Source/PluginProcessor.cpp:5711](Source/PluginProcessor.cpp:5711) — param default `-1` → `60`.
- [Source/Standalone/DrumKitGrid.cpp](Source/Standalone/DrumKitGrid.cpp) — new kit hits stamped at the drum's assigned note; re-pitch helper (undoable via the existing `DrumKitSnapshot` undo).

**Task 2**
- New: `Source/MidiLearn/DrumTriggerMap.h/.cpp` (binding store + capture + persistence).
- [Source/PluginProcessor.h](Source/PluginProcessor.h)/[.cpp](Source/PluginProcessor.cpp) — own the map, publish to the audio thread, add the new dispatch to the live-MIDI loop in `processBlock` (the old fan-out it was to replace is already gone — deleted in Task 1), add the kit-focus atomic, save/load in `getStateInformation`/`setStateInformation` (mirroring `mMidiLearn.saveToValueTree` at [:4630](Source/PluginProcessor.cpp:4630)/[:4680](Source/PluginProcessor.cpp:4680)).
- [Source/Standalone/DrumPage.cpp](Source/Standalone/DrumPage.cpp) — "MIDI Learn" item + the D-10 prompt.
- [Source/MidiLearn/MidiLearnUI.h](Source/MidiLearn/MidiLearnUI.h) — reuse/mirror the learn overlay + 30 s timeout.
- [Source/Standalone/StandaloneEditor.cpp](Source/Standalone/StandaloneEditor.cpp) — publish "drum kit focused" when the Piano Roll page is showing the Drum Kit.

**Task 3**
- Settings toggle (hamburger / MIDI settings) + `settings.xml` persistence; applied in the Task-2 dispatch.

## Tasks

### Task 1 — Menu split + "MIDI Note" becomes the play pitch

- [x] Add a `bool fromKit` parameter to `DrumPage::showContextMenu`. Page caller (`DrumPage.cpp:1029`) passes `false`; both kit callers (`StandaloneEditor.cpp:6190`, `:6308`) pass `true`.
- [x] Gate the entire MIDI block (`DrumPage.cpp:1104-1131`) behind `fromKit` — pages show neither MIDI Note nor MIDI Learn.
- [x] Change the `_inputNote` param default `-1` → `60` (`PluginProcessor.cpp:5711`). Its meaning is now the drum's **play pitch**, not an input filter. Rename the id to `_playNote` if convenient (pre-v1, no back-compat needed — see `feedback_no_backward_compat_pre_v1`); update all readers incl. `mDrumInputNotePtr` (`PluginProcessor.h:1001`, `:5361`, `:5393`).
- [x] Drop the "Unassigned" item; the submenu top / parent label reads **"Assigned: \<note\>"** using the existing FL-style naming (`getMidiNoteName(n, true, true, 5)`, C5 = 60).
- [x] New kit hits are stamped at the drum's assigned note (`DrumKitGrid`).
- [x] On assignment change: re-pitch that drum's notes **currently at the old assigned note** to the new note, wrapped in the existing kit undo (`beginEdit`/`commitEdit` + `DrumKitSnapshot`) so one Ctrl+Z restores. Leave notes at other pitches alone (D-6).
- [x] Build gate: Jeff runs `do_build.bat`. Fix until Debug + Release are clean.
- [x] `/draft-doc running-notes` → append to `Plans & Specs/Running Notes/eager-thumping-marmot.md`.

### Task 2 — MIDI Learn (note + CC) + trigger dispatch

- [x] Build `DrumTriggerMap`: per-drum binding `{ enum Kind { Note, Cc }, int number, int channel (0 = omni), String deviceName }`, message-thread mutation + lock-protected reads, ValueTree save/load, `onChanged` listener. Mirror `MidiLearnRegistry`'s threading contract (try-lock on the audio thread; skip if contended).
- [x] Learn capture: `beginLearn(drumIndex, callback)` + `tryCaptureLearn(deviceName, msg)` accepting **note-on OR CC** (this is why it's a sibling system — the param registry has no Note type). 30 s timeout + the dashed-outline overlay pattern from `MidiLearnUI`.
- [x] "MIDI Learn" menu item on the kit menu, directly below MIDI Note; show the current binding in the label (e.g. `MIDI Learn: CC 42` / `MIDI Learn: D5` / `MIDI Learn` when unset).
- [x] D-10 prompt: on capturing a **note**, `AlertWindow` "Also set this drum's play note to \<note\>?" Yes/No (ASCII-only). Yes → set the assigned note (Task-1 path, incl. re-pitch). No prompt on CC.
- [x] ~~Publish a **"drum kit focused"** flag from `StandaloneEditor` to the processor (atomic), set when the Piano Roll page is showing the Drum Kit and cleared otherwise.~~ **Superseded — no new flag was added.** The state already existed: `EngineKind::DrumKit` is `0` and `StandaloneEditor` already pushes it into `mLiveMidiTargetKind` on every engine selection incl. the boot-time push, so `mLiveMidiTargetKind == 0` IS "kit focused". Reused rather than duplicated (one source of truth). Note the semantics this inherits: it tracks the Piano Roll's *selected* engine, so it stays true when the user navigates to Mixer/Effects — consistent with how every other engine kind behaves for live MIDI.
- [x] Build the new dispatch in the live-MIDI loop in `processBlock`:
  - CC message matching a drum's CC binding → fire that drum **regardless of focus**.
  - Note message matching a drum's note binding → fire that drum **only if the kit-focused flag is set**; otherwise fall through to normal routing (the focused engine plays it).
  - Firing = inject note-on (+ matching note-off) into that drum's `drumPageMidi[di]` **at the drum's assigned MIDI Note**, velocity per D-11.
- [x] Persist the map with the project (`getStateInformation`/`setStateInformation`).
- [x] ~~Delete the now-dead `inputNote` fan-out remnants.~~ **Done in Task 1** — the old `kind == 3` fan-out (formerly `PluginProcessor.cpp:2749-2766`) was deleted there rather than here, because once `_playNote` defaulted to 60 for every drum that block would have fired all 16 drums on any incoming C5 while a drum tab was focused. Nothing left to replace; only the new dispatch above remains to build. Post-deletion, the routing-destination line survives at [PluginProcessor.cpp:2713](Source/PluginProcessor.cpp:2713).
- [x] Build gate: Jeff runs `do_build.bat`. Fix until clean.
- [x] `/draft-doc running-notes` → append.

### Task 3 — Global velocity toggle

- [x] Add the app-wide setting **"MIDI trigger velocity"**: `From controller` (default) / `Fixed`. Persist to `settings.xml`.
- [x] Apply in the Task-2 dispatch: `From controller` → note-on velocity, or CC value scaled to velocity. `Fixed` → the standard drawn-note default velocity.
- [x] Build gate + `/draft-doc running-notes`.

### Task 4 — Close

- [ ] `/review-batch` over this batch's diff.
- [ ] Draft + HOLD the Implemented Work Log entry in the running notes (applies at the §B.18 campaign pass, R2).
- [ ] Surface the commit message (Rule 9 one-liner + `Co-Authored-By`) **plus the full `git status`** — every dirty entry with a disposition — and commit **only on Jeff's explicit approval**.
- [ ] **Do not touch the 12 G3 boundary review-fixes already in the tree**, and do not run the boundary smoke — that is Jeff's, after this lands.

## Verification (end-to-end smoke)

Per bulk-run **R4**, verify scripts are authored into the Master Test Plan, **not run mid-execution**.
The scenarios for this fix are **already written** into
[`Plans & Specs/Test Plans/v1-master-test-plan.md`](Test Plans/v1-master-test-plan.md) §B.18 —
**L-8 marked SUPERSEDED; new L-9 … L-14** cover the redesign. The G3 boundary smoke also picks up a
check for it.

**Do NOT ask Jeff to test this mid-batch.** Build gates only (he runs `do_build.bat`); verification
happens at the G3 boundary smoke + the §B.18 campaign pass.

## Routing notes (Rule 3 application during execution)

- This is a **QA-L defect fix** executed at the G3 boundary. At doc-close it gets a **§9 Forks entry**
  back-referencing QA-L: the per-drum-MIDI redesign, why the shipped version was unreachable
  (the fan-out was drum-tab-focus-gated AND skipped the focused drum, so it did nothing on the kit
  itself), and the locked D-1…D-16 design. Note for whoever writes it: the original "vs. CC pads"
  framing was **disproven by hardware test** — see the Premise correction blockquote in Context.
- Findings discovered mid-execution: real bugs get fixed in-batch (standing rule); anything touching
  a not-yet-started batch folds into that batch's §5 scope; anything else → §9 at close.
- Any NEW spec call → ask Jeff in chat immediately; never self-decide.

## Carry-Forward Reference touch points

- §1-3 (architectural primitives, file index, decisions) at **Task 2** start, before touching the
  audio-thread dispatch.
- The dynamic-drum model (Phase D) context in `CLAUDE.md` "Source Layout" for `DrumPage` /
  `drumRolls` / per-drum APVTS prefixes.
