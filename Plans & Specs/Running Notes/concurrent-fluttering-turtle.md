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
- **Harmless**: same as the synths (same-note tail-off bleed).

Jeff confirmed live: switching a synth to **Mono** "fixed" it — but that was Mono cutting on its own; the Cut Self flag is *ignored* on the Mono path. The defect is the tail-off release (bleed) vs FL Studio's instant hard cut.

**Spec calls locked (Jeff, this session)** — see plan §"Spec calls already locked" for the full table. Headline:
- Fix the bleed → **instant, click-free** same-pitch cut on Synth/Bass (Poly) + Harmless.
- Add a **separate 2-way mode** `cutSelfMode` (**Same Pitch** / **Cut All**) beside the existing on/off, on all four engines (SC-uniform). Keeping the on/off bool + per-engine behavior-preserving default (Player→Cut All, others→Same Pitch) → **no migration** (SC-onoff-plus-mode / SC-default).
- UI per engine: Harmless = mode button right of full CUT SELF; Synth/Bass = halve CUT SELF, mode fills the freed half; Player = mode switch right of the CUT SELF switch (SC-ui-*). Labels "Same Pitch" / "Cut All" (ASCII, SC-labels).
- Declick = shortest possible; Jeff flags if it clicks (SC-declick).
- Drums (Player Cut All) + different-pitch Layers/Bass behavior left exactly as-is (SC-leave-alone).
- **All in this batch, no split** (SC-scope). **One pass** — 4 engines in a single build/verify/commit (SC-tasksplit, via AskUserQuestion).

**Mechanism (from voice-level investigation).** Click-free hard cut = a fast fade-**out** on the cut voice (not the agent's mistaken "new-note fade-in masks it"). BaySickSynthVoice already has a per-sample declick apply point to reuse; AdditiveVoice (Harmless) has none — add a ~1 ms fade-out ramp; VibePlayer reuses its existing `initiateSteal()` quick-release for the new Same-Pitch mode and keeps `allNotesOff(0,false)` for Cut All.

**Task 0 actions.** Plan mirrored `~/.claude/plans/` → `Plans & Specs/Batch Plans/concurrent-fluttering-turtle.md`; home-dir copy deleted. Main Plan §5 QA-CutSelfReview STATUS:OPEN + Plan-file pointer added. This running-notes file seeded. Commit next.

---

## Diagnostic Instrumentation Catalog

_(Empty — no diagnostic instrumentation added this batch. Rule 4. Rows added here in the same edit pass if any `DBG`/`Logger`/temp `jassert`/debug `AlertWindow`/temp-file trace is introduced mid-task.)_

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| — | — | — | — |
