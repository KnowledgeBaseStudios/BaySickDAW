# QA-CutSelfReview — Cut Self on Layers/Bass: instant same-pitch cut + user-selectable Same-Pitch/Cut-All mode across 4 engines — Plan (concurrent-fluttering-turtle)

**Canonical path:** `Plans & Specs/Batch Plans/concurrent-fluttering-turtle.md` (mirror of this file, created at Task 0; home-dir copy deleted per plan-file hygiene).
**For execution:** read this plan top-to-bottom before each task; check the box as each step verifies. Paired running notes: `Plans & Specs/Running Notes/concurrent-fluttering-turtle.md`.

---

## Context

**Origin.** QA-CutSelfReview was teed up at the QA-Ee close (§9 forty-ninth Forks entry, 2026-06-05) from that batch's out-of-scope finding #2 (2026-06-04): *"Cut Self doesn't work on Layers or Bass. Works on drum-kit grid entries; not on Layers/Bass. Needs program-wide investigation (confirm the exact feature + why it's drum-kit-only)."* §5 scopes it as a program-wide investigation. Bucket: System Pages (+ Players — the fix is engine DSP).

**What the investigation found (this session).** "Cut Self" is not missing — it's a *fully wired* control (`cutSelf` APVTS bool + "CUT SELF" button + `setCutSelf()` DSP) on all four engines that back Layers/Bass/Drums. The bug is that **"Cut Self" is implemented three different ways**, and the "drum-kit vs Layers/Bass" framing was a red herring — it's an **engine differential**, not a page/piano-roll differential (note dispatch is identical across pages):

| Engine | Today's "Cut Self ON" behavior | Feel |
|---|---|---|
| **BaySickPlayer** (typical drums) | `allNotesOff()` — hard-kills **every** voice on each note-on | Aggressive, obvious → "works" |
| **BaySickSynth / BaySickBass** (Poly) | same-note-only, **tail-off release** | Bleeds → "doesn't work" |
| **Harmless** | same-note-only, **tail-off release** | Bleeds → "doesn't work" |

Drums are usually BaySickPlayer sample instances (the loud all-voices kill); Layers/Bass are usually the synths (the subtle same-note release). Jeff confirmed live: switching a synth to **Mono** "fixed" it — but that was Mono cutting everything on its own; the Cut Self flag is *ignored* on the Mono/Lead path ([BaySickSynthDSP.cpp:36-67](Source/BaySickSynth/BaySickSynthDSP.cpp:36)). The real defect: the Poly same-note cut injects a MIDI note-off, which JUCE always treats as a **tail-off release** (runs the voice's ADSR release), so the old note **bleeds** into the retrigger instead of stopping dead. FL Studio hard-stops instantly.

**Decision (Jeff, this session).** Two-part change, all in this batch:
1. **Fix the bleed** — the same-note cut becomes an **instant, click-free hard stop** on BaySickSynth/Bass (Poly) + Harmless.
2. **Expose the behavior as a user choice** — keep the existing on/off `cutSelf`, and add a **separate 2-way mode: "Same Pitch" vs "Cut All"** on all four engines, so each engine does *what it's set to, not what it is*. This dissolves the two edge cases (BaySickPlayer-on-a-layer, synth-on-a-drum) by letting the user pick.

**Outcome.** Consistent, user-selectable Cut Self across all four engines. Because the on/off bool is retained and the mode is a *new sibling param* (not a bool→enum conversion), **no old-project/preset migration is needed** — the new mode defaults per-engine to today's behavior.

---

## Spec calls already locked (Jeff, this session)

| ID | Decision | Reasoning |
|----|----------|-----------|
| **SC-behavior** | Two behaviors: **Cut Same Pitch** = instant hard-cut of the prior voice of the *same* note; **Cut All** = instant hard-cut of *all* ringing voices. Both instant + click-free. | The bleed fix (instant) is the core defect; "Cut All" is the existing Player behavior generalized. |
| **SC-onoff-plus-mode** | Keep the existing `cutSelf` bool (on/off) untouched. Add a **separate** 2-way mode param `cutSelfMode` (Same Pitch / Cut All) beside it. NOT a 3-way merge of the toggle. | Jeff's UI choice. Retaining the bool means old saved state still loads; the new mode just needs a behavior-preserving default → zero migration. |
| **SC-default** | `cutSelfMode` default is **per-engine, behavior-preserving**: BaySickPlayer → Cut All; BaySickSynth / BaySickBass / Harmless → Same Pitch. | Old projects/presets with Cut Self ON keep behaving exactly as today with no migration code. |
| **SC-uniform** | "Cut All" is added to **all four** engines, including BaySickSynth/Bass, despite overlapping heavily with their existing Mono voice-mode. | Jeff: uniformity — every engine offers the same two modes. |
| **SC-declick** | Declick fade is the **shortest possible** that avoids an audible pop. Jeff flags on verify if he hears a click; widen only then. | Truly-instant cuts click at non-zero-crossings; a sub-few-ms fade is perceptually instant + pop-free. |
| **SC-ui-harmless** | Harmless: existing "CUT SELF" button stays full-width on/off; add the mode control to its **right**. | Jeff answer 1. |
| **SC-ui-synthbass** | BaySickSynth + BaySickBass: shrink the "CUT SELF" button to **half width**; the mode control fills the freed half. | Jeff answer 1. |
| **SC-ui-player** | BaySickPlayer: the "CUT SELF" switch stays as-is; add a new mode **switch** to its right. | Jeff answer 1. |
| **SC-labels** | Umbrella control stays **"CUT SELF"**; mode control labels are **"Same Pitch"** / **"Cut All"** (pure ASCII). Feature is NOT renamed; avoids colliding with the parked drum "Choke Group" concept. | Jeff answer 1/3 + ASCII-only-UI-strings rule. |
| **SC-scope** | Entire feature (bleed fix + mode selector + DSP + param + UI on all four engines) lands in **QA-CutSelfReview**. No splitting to a later batch. | Jeff answer 4 (verbatim: this is what the batch is for). |
| **SC-tasksplit** | **One pass** — all four engines built + verified together in a single implementation commit (Task 1). Open + close are their own commits. | Jeff (AskUserQuestion, this session). |
| **SC-leave-alone** | Unchanged: different-pitch voices on Layers/Bass ring independently (Same-Pitch mode never touches them); Mono/Lead/Legato on Synth/Bass keep their own cut logic; BaySickPlayer **Cut All** keeps its exact `allNotesOff(0,false)` behavior (drums untouched). | Jeff confirmed current different-pitch + drum behavior is correct. |

**FYI (implementation call, chat-surfaced, not a fork):** on BaySickSynth/Bass the mode control only bites in **Poly** (Mono already cuts everything). It will be left **enabled-but-inert in Mono**, matching how the existing CUT SELF button already behaves in Mono today. Grey-out only if Jeff requests on verify.

---

## Sub-spec calls surfaced for ExitPlanMode

**No sub-spec calls open.** The one genuine fork — task-count / commit-boundary split — was surfaced via chat and resolved (**SC-tasksplit: One pass**). The remaining choices are implementer's calls with **no user-facing behavior fork** — all deliver the locked behavior (instant + click-free + labeled Same Pitch / Cut All):
- The exact click-free fade-out mechanism per voice (a dedicated ~1 ms fade-out gain ramp vs a quick-release ADSR override) — Jeff doesn't evaluate code shape; both are click-free and perceptually instant.
- `cutSelfMode` implemented as a `juce::AudioParameterBool` (attaches cleanly to the 2-state toggle/switch/button per SC-ui-*).
- The exact 2-state widget class per editor (matched to each editor's existing idiom).

---

## Files to modify

No changes to `PluginProcessor` / `VibeGraph` / the note scheduler — note dispatch is confirmed identical across pages and is **not** the cause. All work is in the four engines' DSP + voice + processor + editor.

**BaySickSynth + BaySickBass (shared DSP — one DSP/voice fix serves both):**
- [BaySickSynthDSP.h](Source/BaySickSynth/BaySickSynthDSP.h) / [.cpp](Source/BaySickSynth/BaySickSynthDSP.cpp) — add `setCutSelfMode(bool cutAll)` + `bool mCutAll`; replace the Poly cut-self note-off injection (~[81-96](Source/BaySickSynth/BaySickSynthDSP.cpp:81)) with a click-free hard-fade of same-note (Same Pitch) or all (Cut All) voices *before* `mSynth.renderNextBlock`.
- [BaySickSynthVoice.h](Source/BaySickSynth/BaySickSynthVoice.h) / [.cpp](Source/BaySickSynth/BaySickSynthVoice.cpp) — add `cutFast()`: a ~1 ms click-free fade-out (reuse the existing per-sample declick apply point ~596-602; add an `mCutFade` ramp 1→0, `clearCurrentNote()` at 0). Serves Bass too.
- [BaySickSynthProcessor.cpp](Source/BaySickSynth/BaySickSynthProcessor.cpp) / [.h](Source/BaySickSynth/BaySickSynthProcessor.h) — register `cutSelfMode` Bool default **false** (~178); `ParamCache` field (~109); value-change-guarded `setCutSelfMode` sync (~401-405).
- [BaySickSynthEditor.cpp](Source/BaySickSynth/BaySickSynthEditor.cpp) / [.h](Source/BaySickSynth/BaySickSynthEditor.h) — halve `mCutSelfBtn` width (~628); add `mCutSelfModeBtn` + attachment in the freed half (create ~134, attach ~332, deck-0 visibility ~899).
- [BaySickBassProcessor.cpp](Source/BaySickBass/BaySickBassProcessor.cpp) / [.h](Source/BaySickBass/BaySickBassProcessor.h) — register `cutSelfMode` Bool default **false**, cache, guarded sync (~389-393).
- [BaySickBassEditor.cpp](Source/BaySickBass/BaySickBassEditor.cpp) / [.h](Source/BaySickBass/BaySickBassEditor.h) — mirror the Synth editor (create ~132, attach ~329, bounds ~610, visibility ~864).

**Harmless:**
- [HarmlessSynth.cpp](Source/Harmless/HarmlessSynth.cpp) / [.h](Source/Harmless/HarmlessSynth.h) — add `setCutSelfMode(bool)` + `mCutAll`; replace the cut-self note-off injection (~[79-94](Source/Harmless/HarmlessSynth.cpp:79)) with the click-free hard-fade.
- [AdditiveVoice.h](Source/Harmless/AdditiveVoice.h) / [.cpp](Source/Harmless/AdditiveVoice.cpp) — add `cutFast()` with a **new** ~1 ms fade-out ramp (AdditiveVoice has no declick/quick-release today — add the ramp + apply it in its render loop).
- [HarmlessProcessor.cpp](Source/Harmless/HarmlessProcessor.cpp) / [.h](Source/Harmless/HarmlessProcessor.h) — register `cutSelfMode` Bool default **false**, cache, guarded sync (~673-677).
- [HarmlessEditor.cpp](Source/Harmless/HarmlessEditor.cpp) / [.h](Source/Harmless/HarmlessEditor.h) — add `mCutSelfModeBtn` to the **right** of the full-width `mCutSelfBtn` (layout row ~870; create ~270, attach ~443).

**BaySickPlayer (VibePlayer):**
- [VibePlayerDSP.cpp](Source/VibePlayer/VibePlayerDSP.cpp) / [.h](Source/VibePlayer/VibePlayerDSP.h) — add `setCutSelfMode(bool)` + `mCutAll`; at the cut-self site (~[1283-1285](Source/VibePlayer/VibePlayerDSP.cpp:1283)): **Cut All** → keep `allNotesOff(0,false)` unchanged; **Same Pitch** → `initiateSteal()` (~1064) + `stopNote(0.f,true)` on the same-note voices in the cached `mVoices[]` (instant ~1.5 ms fade).
- [VibePlayerProcessor.cpp](Source/VibePlayer/VibePlayerProcessor.cpp) / [.h](Source/VibePlayer/VibePlayerProcessor.h) — register `cutSelfMode` Bool default **true** (=Cut All), cache, guarded sync (~209-214).
- [VibePlayerEditor.cpp](Source/VibePlayer/VibePlayerEditor.cpp) / [.h](Source/VibePlayer/VibePlayerEditor.h) — add `mCutSelfModeTog` switch to the **right** of `mCutSelfTog` (create ~85, attach ~250, bounds ~438).

---

## Tasks

### Task 0 — Open
- [ ] Mirror `~/.claude/plans/concurrent-fluttering-turtle.md` → `Plans & Specs/Batch Plans/concurrent-fluttering-turtle.md` (Write); delete the home-dir copy.
- [ ] Main Plan §5 QA-CutSelfReview entry: add `**Plan file:** Plans & Specs/Batch Plans/concurrent-fluttering-turtle.md` + set STATUS:OPEN (match §5 convention).
- [ ] Seed `Plans & Specs/Running Notes/concurrent-fluttering-turtle.md` (title / purpose blockquote / pair-file ref / convention ref / "Task 0 — open" entry) per §0 running-notes required sections.
- [ ] Surface full git status. Brief one-liner (Rule 9): `QA-CutSelfReview Task 0 (open): plan mirrored + running notes seeded + Main Plan docket STATUS:OPEN + plan-file pointer (Main Plan, Batch Plans, Running Notes)`. Commit on approval.

### Task 1 — Cut Self "Same Pitch / Cut All" mode across all four engines (single build/verify/commit — SC-tasksplit)

**DSP + voice.**

- [ ] `BaySickSynthDSP` — add `setCutSelfMode(bool)` + `bool mCutAll`; replace the Poly note-off injection with a click-free hard-fade of the cut voice(s) **before** render. Before/after ([BaySickSynthDSP.cpp:78-101](Source/BaySickSynth/BaySickSynthDSP.cpp:78)):

```cpp
// BEFORE — injects a MIDI note-off => JUCE tail-off release => the bleed:
if (mCutSelf) {
    juce::MidiBuffer processed;
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            processed.addEvent (juce::MidiMessage::noteOff (msg.getChannel(), msg.getNoteNumber()),
                                meta.samplePosition);
        processed.addEvent (msg, meta.samplePosition);
    }
    mSynth.renderNextBlock (buf, processed, 0, buf.getNumSamples());
} else {
    mSynth.renderNextBlock (buf, midi, 0, buf.getNumSamples());
}

// AFTER — hard-fade the matching (SamePitch) or all (CutAll) active voices, then
// render the un-modified MIDI. cutFast() = ~1 ms fade-out, no ADSR tail => no bleed.
// Loop runs only on note-on events (off the per-sample hot path); dynamic_cast there
// is acceptable — cache voices only if /perf-audit flags it.
if (mCutSelf) {
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn()) continue;
        const int n = msg.getNoteNumber();
        for (int vi = 0; vi < mSynth.getNumVoices(); ++vi)
            if (auto* v = dynamic_cast<BaySickSynthVoice*> (mSynth.getVoice (vi)))
                if (v->isVoiceActive() && (mCutAll || v->getCurrentlyPlayingNote() == n))
                    v->cutFast();
    }
}
mSynth.renderNextBlock (buf, midi, 0, buf.getNumSamples());
```

- [ ] `BaySickSynthVoice::cutFast()` — a ~1 ms click-free fade-out reusing the existing per-sample declick apply point (~[596-602](Source/BaySickSynth/BaySickSynthVoice.cpp:596)); serves Bass too (shared DSP/voice):

```cpp
// BaySickSynthVoice.h — beside mDeclickGain/mDeclickStep (231-232):
void  cutFast() noexcept;
bool  mCutFadeActive { false };
float mCutFadeGain   { 1.0f };
float mCutFadeStep   { 0.0f };

// BaySickSynthVoice.cpp:
void BaySickSynthVoice::cutFast() noexcept {
    if (! isVoiceActive()) return;
    const int fadeSamples = juce::jmax (1, (int) (getSampleRate() * 0.001f)); // ~1 ms
    mCutFadeGain = 1.0f; mCutFadeStep = 1.0f / (float) fadeSamples; mCutFadeActive = true;
}
// per-sample, at the existing mDeclickGain apply point in renderNextBlock:
if (mCutFadeActive) {
    sample *= mCutFadeGain;
    mCutFadeGain -= mCutFadeStep;
    if (mCutFadeGain <= 0.0f) { mCutFadeActive = false; mAmpEnv.reset(); clearCurrentNote(); }
}
```

- [ ] `AdditiveVoice::cutFast()` (Harmless) — same pattern, but AdditiveVoice has **no** existing declick, so add the `mCutFade*` members **and** the per-sample apply point in its render loop.
- [ ] `HarmlessSynth` — add `setCutSelfMode(bool)` + `mCutAll`; same cut-then-render restructure as BaySickSynthDSP at [HarmlessSynth.cpp:79-94](Source/Harmless/HarmlessSynth.cpp:79) (iterate `mSynth.getVoice(i)` → `AdditiveVoice*` → `cutFast()`).
- [ ] `VibePlayerDSP` — add `setCutSelfMode(bool)` + `mCutAll`; branch the cut site ([VibePlayerDSP.cpp:1283-1285](Source/VibePlayer/VibePlayerDSP.cpp:1283)):

```cpp
// BEFORE:
if (mCutSelf)
    mSynth.allNotesOff (0, false);

// AFTER — CutAll unchanged (drums stay exact, SC-leave-alone); SamePitch fast-fades
// only the same-note voice(s) via the existing steal path (~1.5 ms, click-free):
if (mCutSelf) {
    if (mCutAll)
        mSynth.allNotesOff (0, false);
    else
        for (int vi = 0; vi < kMaxVoices; ++vi)
            if (auto* vv = mVoices[vi]; vv && vv->isVoiceActive()
                && vv->getCurrentlyPlayingNote() == note)
                { vv->initiateSteal(); vv->stopNote (0.f, true); }
}
```

**Params (all four processors).**

- [ ] Register `cutSelfMode` `AudioParameterBool` + cache field + value-change-guarded sync (CPU-safeguarding rule). Representative (BaySickSynth; Bass/Harmless default `false`, **VibePlayer default `true`**):

```cpp
// createLayout, beside the existing cutSelf reg (~178):
layout.add (std::make_unique<juce::AudioParameterBool> (
    vid (p + "cutSelfMode"), "Cut Self Mode", false));   // false = Same Pitch

// ParamCache (…Processor.h ~109):   int cutSelfMode { -1 };

// updateFromApvts, beside the cutSelf sync (~401-405):
const int csMode = geti ("cutSelfMode");
if (csMode != mCache.cutSelfMode) { mSynth.setCutSelfMode (csMode != 0); mCache.cutSelfMode = csMode; }
```

**Editors (all four).**

- [ ] Add the 2-state mode control per SC-ui-*, `ButtonAttachment` to `cutSelfMode`, ASCII labels **"Same Pitch" / "Cut All"**, matched to each editor's idiom + visibility path. Representative (BaySickSynth — halve the button, split the cell):

```cpp
// create (~134, mirroring mCutSelfBtn's switchToggle idiom):
mCutSelfModeBtn.setClickingTogglesState (true);
mCutSelfModeBtn.getProperties().set ("switchToggle", true);
mCutSelfModeBtn.setTooltip ("Cut Self mode: Same Pitch cuts only a retriggered note; "
                            "Cut All cuts every ringing note.");
addAndMakeVisible (mCutSelfModeBtn);
// attach (~332):
mCutSelfModeAtt = std::make_unique<ButtonAtt> (avts, pid ("cutSelfMode"), mCutSelfModeBtn);
// layout (~628) — split the old full-width cut cell in half:
auto cut = /* existing mCutSelfBtn bounds */;
mCutSelfBtn    .setBounds (cut.removeFromLeft (cut.getWidth() / 2 - 2));
mCutSelfModeBtn.setBounds (cut.withTrimmedLeft (2));
// deck-0 visibility (~899):  mCutSelfModeBtn.setVisible (visible);
```

  Per-engine placement: **Harmless** — new button to the right of the full-width CUT SELF in the layout row (~870, `{ &mCutSelfModeBtn, 72, 18 }` after `mCutSelfBtn`). **BaySickBass** — mirror the Synth split (~610). **BaySickPlayer** — a second toggle switch right of `mCutSelfTog` (~438), showing the two labels (DualLabelToggle idiom). The exact 2-label rendering (button text updating with state vs a two-label toggle) is matched per editor at execution.

**Hygiene:**
- [ ] Rule 6: fix the now-stale cut-self comments in the edited regions (the "inject a note-off … tail-off" comments become wrong — rewrite to the hard-fade approach). Rule 4: no diagnostic instrumentation planned; catalog stays empty unless a debug aid is added mid-task (catalog it in the same edit pass if so).
- [ ] Tell Jeff to run `do_build.bat` (Debug first, then Release) and run the Verification scenarios below.
- [ ] Wait for Jeff's Debug + Release verify result.
- [ ] On pass: surface full git status + brief one-liner (`QA-CutSelfReview Task 1: instant click-free Cut Self + Same Pitch/Cut All mode on BaySickSynth/Bass/Harmless/BaySickPlayer (…scope…)`); commit on approval.
- [ ] `/draft-doc running-notes` → apply.

### Task 2 — Close
- [ ] `/draft-doc batch-close` from the running notes.
- [ ] Apply the close entry to `Plans & Specs/Implemented Work Log.md` via Edit.
- [ ] `/review-batch QA-CutSelfReview`. Address BLOCKER / NEEDS-FIX in-batch; defer NITs into the close-entry routing table.
- [ ] Route side findings (Rule 3): in-batch → close-entry routing table; outside-surface → §9 Forks + §5/§6/Future State edits (slot = Jeff's call, surfaced not picked).
- [ ] Main Plan §5 QA-CutSelfReview STATUS:CLOSED + Work Log pointer; §6 arrow advances the active marker to QA-UICleanup.
- [ ] Surface full git status + brief close one-liner; commit the close (separate from the Task 1 commit — clean rollback boundary).

---

## Verification (end-to-end smoke)

**Build:** `do_build.bat` — Release + Debug both green.

Test on a Layers tab (BaySickSynth + Harmless), a Bass tab (BaySickBass), and a BaySickPlayer instance (a drum slot + a Player-on-a-layer). Gestures are piano-roll placement or hold+repress on the audition keyboard — both executable.

1. **Same-Pitch bleed fix (core).** BaySickSynth on a Layer, Poly, Cut Self ON, mode = Same Pitch, a pad patch with a long release. Place two overlapping notes of the **same** pitch (or hold + repress the same key). The old instance stops **instantly** on the retrigger — no release tail bleeding through — and no click/pop.
2. **Different pitches unaffected.** Same setup, overlapping notes of **different** pitches ring independently (Same Pitch never touches them).
3. **Cut All.** Switch mode → Cut All: any new note (same or different pitch) instantly chokes all ringing voices, click-free.
4. **Off.** Cut Self OFF → voices stack normally regardless of mode.
5. **Harmless / BaySickBass.** Repeat 1-4 (Harmless always poly; Bass has Poly/Mono like Synth).
6. **BaySickPlayer drums.** Default mode = Cut All → every hit chokes the prior (identical to pre-batch). Switch → Same Pitch → only same-pitch retrigs cut; different pitches ring.
7. **No-migration.** Open a project saved before this batch with Cut Self ON on a synth layer + a Player drum → synth layer = Same Pitch, drum = Cut All (defaults) = identical to pre-batch behavior; nothing lost.
8. **Mono inert (Synth/Bass).** Set Mono → the mode control is inert (Mono already cuts everything); no regression.
9. **UI layout.** All four editors show the mode control per SC-ui-*, labeled "Same Pitch" / "Cut All", ASCII, matched to each editor's style.

---

## Routing notes (Rule 3 application during execution)

- Findings scoped to Cut Self / voice-stop behavior surfaced mid-batch → fold here (QA default: fix in-batch).
- Findings about unrelated engine voice-management or the note scheduler → close-entry routing table; §9 Forks + new/existing §5 batch if outside surface (slot = Jeff's call).
- Dead code this batch's own changes orphan (e.g. the removed note-off-injection blocks) → remove in-batch (own-batch dead-code rule).

---

## Carry-Forward Reference touch points

- Read at Task 1 start: Carry-Forward §1-3 (architectural primitives / engine DSP file index) for BaySickSynthDSP + voice conventions before touching the DSP.
- CLAUDE.md references: Engine audition pattern; APVTS Binding Pattern; **CPU Safeguarding** (guard the new `setCutSelfMode` with a value-change compare); JUCE Gotchas (constructor/`resized()` null-guards when adding editor controls).
