# Running Notes — QA-CutSelfReview (concurrent-fluttering-turtle)

> Append-only running log for QA-CutSelfReview. A new `## YYYY-MM-DD — Task N — <name>` entry is appended at **every checkpoint** — commit landed, sub-task verified, finding captured, spec call resolved, scope pivot — per `feedback_draft_doc_running_notes_every_checkpoint.md`. At batch close, `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Never edit prior entries; corrections get a new dated entry.

**Pair file:** [`Plans & Specs/Batch Plans/concurrent-fluttering-turtle.md`](../Batch Plans/concurrent-fluttering-turtle.md) (the plan).
**Conventions:** Main Plan §0 "Document Formatting Conventions" + "Batch Plans + Running Notes layout (locked 2026-05-11)".

---

## 2026-07-06 — Task 0 — Open

**Origin.** QA-Ee out-of-scope finding #2 (2026-06-04, `Running Notes/rhythmic-counting-octopus.md` line 491): *"Cut Self doesn't work on Layers or Bass. Works on drum-kit grid entries; not on Layers/Bass."* Routed at the QA-Ee close (§9 forty-ninth Forks). §5 scoped as program-wide investigation.

**Pre-batch investigation (this session) — root cause.** "Cut Self" is a fully-wired control (`cutSelf` APVTS bool + "CUT SELF" button + `setCutSelf()`) on all four engines. The "drum-kit vs Layers/Bass" framing was a red herring — note dispatch is identical across pages (confirmed). The real cause is an **engine differential**: Cut Self is implemented three ways —
- **BaySickPlayer** (typical drums): `allNotesOff()` — hard-kills all voices (obvious → "works").
- **BaySickSynth / BaySickBass** (Poly): same-note-only via injected MIDI note-off = JUCE **tail-off release** → the old note **bleeds** into the retrigger (subtle → "doesn't work").
- **BaySickSolstice**: same as the synths (same-note tail-off bleed).

Jeff confirmed live: switching a synth to **Mono** "fixed" it — but that was Mono cutting on its own; the Cut Self flag is *ignored* on the Mono path. The defect is the tail-off release (bleed) vs FL Studio's instant hard cut.

**Spec calls locked (Jeff, this session)** — see plan §"Spec calls already locked" for the full table. Headline:
- Fix the bleed → **instant, click-free** same-pitch cut on Synth/Bass (Poly) + BaySickSolstice.
- Add a **separate 2-way mode** `cutSelfMode` (**Same Pitch** / **Cut All**) beside the existing on/off, on all four engines (SC-uniform). Keeping the on/off bool + per-engine behavior-preserving default (Player→Cut All, others→Same Pitch) → **no migration** (SC-onoff-plus-mode / SC-default).
- UI per engine: BaySickSolstice = mode button right of full CUT SELF; Synth/Bass = halve CUT SELF, mode fills the freed half; Player = mode switch right of the CUT SELF switch (SC-ui-*). Labels "Same Pitch" / "Cut All" (ASCII, SC-labels).
- Declick = shortest possible; Jeff flags if it clicks (SC-declick).
- Drums (Player Cut All) + different-pitch Layers/Bass behavior left exactly as-is (SC-leave-alone).
- **All in this batch, no split** (SC-scope). **One pass** — 4 engines in a single build/verify/commit (SC-tasksplit, via AskUserQuestion).

**Mechanism (from voice-level investigation).** Click-free hard cut = a fast fade-**out** on the cut voice (not the agent's mistaken "new-note fade-in masks it"). BaySickSynthVoice already has a per-sample declick apply point to reuse; AdditiveVoice (BaySickSolstice) has none — add a ~1 ms fade-out ramp; VibePlayer reuses its existing `initiateSteal()` quick-release for the new Same-Pitch mode and keeps `allNotesOff(0,false)` for Cut All.

**Task 0 actions.** Plan mirrored `~/.claude/plans/` → `Plans & Specs/Batch Plans/concurrent-fluttering-turtle.md`; home-dir copy deleted. Main Plan §5 QA-CutSelfReview STATUS:OPEN + Plan-file pointer added. This running-notes file seeded. Commit next.

---

## 2026-07-06 — Task 1 — Cut Self mode across 4 engines + note-off cascade fix

**Code complete (all 4 engines).** Same Pitch / Cut All mode (`cutSelfMode` Bool param, per-engine behavior-preserving default: Player→Cut All, others→Same Pitch) + the instant click-free cut. Per-engine cut mechanism: BaySickSynthVoice `cutFast()` = ~1 ms fade-out ramp (custom `AdsrEnvelope` has no getters, so a fade multiplier not a quick-release); AdditiveVoice `cutFast()` + VibeVoice `initiateSteal()` = ~1.5 ms ADSR quick-release (juce::ADSR); VibePlayer Cut All keeps `allNotesOff` (drums unchanged). UI per SC-ui-* (Bass shares BaySickSynthDSP).

**VibePlayer editor placement correction.** First cut mis-sized the editor (assumed 480×400 from stale CLAUDE.md notes; it's actually 600×560). Box 0 has ample room. Per Jeff, the mode switch is a third `DualLabelToggle` spread across Box 0 (Reverse | CUT SELF | Same Pitch/Cut All), `setupNamed` (top=Same Pitch=off, bottom=Cut All=on).

**Verify #1 FAILED → deeper root cause (Jeff, 2026-07-06, BaySickSynth on a Layer, overlapping same-pitch notes played back).** The fade worked, but the 2nd note went silent + the 1st bled. Root cause = **per-pitch note-off cascade**: `juce::Synthesiser` matches note-offs by pitch number, so the earlier note's note-off (landing during the 2nd note) kills the retriggered voice. Same problem VibePlayer's QA-VoicePool already fixed (its "per-pitch note-off strip", `VibePlayerDSP.cpp:1264`); BaySickSynth + BaySickSolstice never got it.

**Fix (Jeff green-lit — scope expansion).** Per-pitch note-on reference counting: hold an earlier note's note-off until the LAST overlapping same-pitch note ends; deliver only when the count hits 0. Poly-only (BaySickSynthDSP: zeroed when not in Poly). Self-heals: counters zero whenever the synth is fully silent, so a lost note-off can't cause a permanent stuck note. Applied to BaySickSynthDSP (covers Bass) + BaySickSolsticeSynth. **Verified all-pass (Jeff, 2026-07-06)** — overlapping same-pitch notes retrigger cleanly (2nd cuts 1st instantly + plays its full length), different pitches unaffected, Cut All chokes, three-note chains + sustained-with-retriggers OK, no hang/stuck, no pop — across BaySickSynth / Bass / BaySickSolstice. Debug + Release.

---

## 2026-07-06 — Task 2 — Close: /review-batch + close docs

**`/review-batch QA-CutSelfReview`** — no BLOCKERs. **1 NEEDS-FIX (fixed in-batch):** the per-pitch note-off counter drifted on **CC-123 All-Notes-Off** — broadcast on transport stop / play-from-top / **every loop wrap** (`PluginProcessor.cpp:1330`). CC-123 isn't an `isNoteOff()`, so it fell through the strip's `else` branch un-counted while `juce::Synthesiser` released the voices, leaving `mNoteOnCount` ≥1; the idle-reset self-heal doesn't fire while a long-release tail keeps voices active, so retriggering that pitch during the tail hung it (bit even with Cut Self OFF — the strip runs unconditionally in Poly). Fix: zero `mNoteOnCount` on `isAllNotesOff() || isAllSoundOff()` in the strip's `else` branch (BaySickSynthDSP + BaySickSolsticeSynth, ~1 line each). **2 NITs deferred:** (a) `cutFast` fade-gain can go one sample negative before the retire check — sub-sample, inaudible (a `jmax(0,...)` on the multiply would make it exact); (b) BaySickSolstice strip comment cross-refs BaySickSynthDSP one-directionally (both now carry the CC-123 comment inline). Awaiting rebuild + targeted re-verify (hold audition note on a long-release pad → stop / loop → re-press same key = no hang).

---

## Diagnostic Instrumentation Catalog

_(Empty — no diagnostic instrumentation added this batch. Rule 4. Rows added here in the same edit pass if any `DBG`/`Logger`/temp `jassert`/debug `AlertWindow`/temp-file trace is introduced mid-task.)_

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| — | — | — | — |
