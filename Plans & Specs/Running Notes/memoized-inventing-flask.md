# Running Notes — QA-ClipPlayback (memoized-inventing-flask)

> Append-only mid-batch log.  A new dated entry is added at EVERY checkpoint
> (commit landed / sub-task verified / finding captured / spec call resolved /
> scope pivot), per `feedback_draft_doc_running_notes_every_checkpoint.md`.
> At batch close, `/draft-doc batch-close` reads this file as the primary input
> for the single Implemented Work Log entry.  Never edit prior entries.

**Pair file:** [`Plans & Specs/Batch Plans/memoized-inventing-flask.md`](../Batch Plans/memoized-inventing-flask.md)
**Conventions:** Main Plan §0 Document Formatting Conventions + the Batch Plans / Running Notes required-sections rule (locked 2026-05-11).

## Diagnostic Instrumentation Catalog

Per §0 Rule 4 — every `DBG` / `juce::Logger` / temp `jassert` / debug `AlertWindow` /
temp-file trace gets a row IN THE SAME EDIT PASS.  Strip every `Remove` at task/batch
close after surfacing the strip list to Jeff.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| (none yet) | | | |

## 2026-07-02 — Task 0 — Batch open

- Plan approved + mirrored to `Batch Plans/memoized-inventing-flask.md`; home-dir copy deleted (plan-file hygiene).
- **Batch scope:** two pre-existing clip-drop findings routed from the QA-MultiBlockHazard close (Main Plan §9 fifty-third Forks entry). Same ClipsPage-BaySickPlayer subsystem ("kind of all one thing", Jeff).
  - **Finding 1 (feature build):** the timeline-WAV decode path (`renderAudioClipsForRow`, "Flow B") never runs a clip through the ClipsPage BaySickPlayer controls; only the piano-roll/sampler path ("Flow A") does. It's the playback MODE that gates the knobs, not the add-path. Fix = wire the full applicable control set into the decode path, read LIVE off the row's ClipsPage engine and applied per-clip in `clipScratch` before the raw sum, keeping stretch + trim.
  - **Finding 2 (bug):** `renderAudioClipsForRow:457` keys the builder-grid mute on `row` (owner page) instead of `player.trackRow` (grid row), so two clips on one page share a mute. Fix = key on `player.trackRow`; strip mute (`audioRowMute[row]`) + routing stay owner-keyed. Gate verified (block move -> `commitEdit` -> `rebuildAudioClipPlayers` refreshes `p.trackRow`).
- **Spec calls locked (pre-plan chat):** S1 all-applicable control set (velocity-driven routings inert on a timeline clip); S2 live per-clip read before the sum (piggybacks the task's `mClipEngine`); S3 full clip-level ADSR (attack@clipStart / release@clipEnd / decay+sustain middle; supersedes the 5ms declick when active); S4 length-preserving pitch (NOT tape-speed); S5 task split T1 mute / T2 core controls / T3 pitch / T4 close; S7 engine-type guard (apply only when the clip engine is a `VibePlayerProcessor`).
- **Prior-session recovery (process note):** at batch open I re-asked design questions (pitch / signal-flow / ADSR) that were already settled in the QA-MultiBlockHazard session; Jeff flagged the token waste. The decisions were partly in this batch's routed running-notes predecessor and fully in his paste of that chat. Correction going forward: when a batch is routed from a prior session's findings, synthesize that session's design decisions BEFORE surfacing spec calls.
- **Next:** commit Task 0 (open, docs-only), then Task 1 — per-grid-row mute fix.
