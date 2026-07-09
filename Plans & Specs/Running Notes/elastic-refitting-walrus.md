# Running Notes — QA-Ec (elastic-refitting-walrus)

> Append-only mid-batch log. New entry at every checkpoint. Under the BULK RUN this batch's Work
> Log entry is drafted at code-complete, HELD here under `## Held Work Log entry (apply at section
> pass)`, and applied only when its Master Test Plan §B section passes.

Pair file: [`Batch Plans/elastic-refitting-walrus.md`](../Batch Plans/elastic-refitting-walrus.md).
Conventions: Main Plan §0 + [`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md).

## 2026-07-08 — G1 group open — plan approved + surface map (source-verified)

Locked: 2a/2b (dual-trigger; Resample=varispeed, Stretch=pitch-locked), F (plain drag trims;
Shift+drag re-fits), G (true-length import at project tempo; `originalBPM` = import tempo; NO
rounding — supersedes the marathon table's bar-rounding wording, corrected there 2026-07-08).
Two findings already recorded: BUILD-06's "missing rebuild trigger" claim is STALE (wired at QA-Ea
Task 0c); today any project tempo != 120 silently stretches every import (the 120 default) — G's
import rule removes the root cause. Verified refs (§5's `:510-533`-era refs are stale):

**Path A** `renderAudioClipsForRow` `PluginProcessor.cpp:508-905`: snapshot `:531`; `readRatio`
`:581`; ratio model `:598-604` (stretchRatio `:598-599` — Resample hardwired 1.0 = the gap; pitch
`:600`; varispeed `:601`; eff terms `:602-604`); window math `:611-617`; **GUARD `:618`**
(`outSamples<=0 → continue` = the silence bug); PV gate `:640-641`, `setStretchRatio` `:645`;
seekNeeded `:650-660`; direct read passes `fileRate` `:765-773`.

**Path B** `decodeFilePlayClip` `:912+` (split note `:906-911`): ratio `:975-979`; guard `:990`;
PV `:998-1006`. No pitch/varispeed terms. LOCKSTEP RULE: every Task-2 edit lands in both paths in
the same commit.

**Streamer:** stateless contract `AudioClipStreamer.h:10-13`; `readAndMix` `:65-71` — fit ratio
applies ABOVE it in the callers.

**Data:** `ArrangementBlock` `PatternManager.h:265` — `originalBPM` `:304` {120}, `stretchMode`
`:305` {true}, `contentStartSamples` `:343`, ticks `:291`/`:282`, `effectiveLengthBeats`
`:351-355`; `AudioLibraryEntry` `:648-664` (bpm/stretch `:662-663`); audio-thread copy
`AudioClipPlayer` `PluginProcessor.h:492-540`.

**Import sites (both get G):** `importAudioFile` `BuilderPage.cpp:3388` — reader `:3411`, length
`:3424-3426`, **120 default `:3422`**, beats `:3427`, block build `:3459-3470`;
`placeAudioLibraryEntry` `:3498` — **120 default `:3519`**, block `:3530-3540`; drop entry
`:3610`→`:3660` (+ callers `:5533`/`:5776`); rebuild reopens the reader
`PluginProcessor.cpp:2668-2669` (content length available there too).

**Stub + resize:** no-op stub `BuilderPage.cpp:4450-4454`; `mStretching` = Shift+right-edge
`:4237`; drag apply `:4419-4457` (`setLengthBeats` `:4430`); mouseUp `commitEdit` `:4767`
(+why-note `:4769-4773`) → `onArrangementChanged` `:2536` → `rebuildAudioClipPlayers` wiring
`StandaloneEditor.cpp:2118-2121`. Mode UI: props dialog combo `:3176-3178`, apply
`:3316-3318`/`:3380-3382`, menu `:3074`/`:3121`; distinct from grid `EditMode{Slip,Stretch}`
(`BuilderPage.h:365`/`:683`) — do not conflate.

**Persistence (already round-trips):** block `PatternManager.cpp:1076-1077`/`:1468-1469` (+
contentStart `:1094-1095`/`:1485-1486`, ticks `:1070`/`:1456-1461`); library
`:1143-1145`/`:1553-1555`. PV degenerate-ratio backstop `PhaseVocoder.cpp:44`.
