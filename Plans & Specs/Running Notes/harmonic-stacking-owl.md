# Running Notes — QA-Chords (harmonic-stacking-owl)

> Append-only mid-batch log. New entry at every checkpoint. Under the BULK RUN this batch's Work
> Log entry is drafted at code-complete, HELD here under `## Held Work Log entry (apply at section
> pass)`, and applied only when its Master Test Plan §B section passes.

Pair file: [`Batch Plans/harmonic-stacking-owl.md`](../Batch Plans/harmonic-stacking-owl.md).
Conventions: Main Plan §0 + [`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md).

## 2026-07-08 — G1 group open — plan approved + surface map (source-verified)

Locked: D (selection-based multi-resize; groups stay manual), D2 (same-delta, relative lengths
preserved), §5 Mode 1 (degree stacking off the roll's own Root+Scale) + Mode 2 (strict snap +
octave-collision resolver). Verified refs (all `Source/Standalone/PianoRoll.*` unless noted):

**Stamp:** `kChordDefs[]` `.cpp:45-60` (14 hardcoded-semitone types); `stampChordAt` `:388-408`
(root scale-snap `:392`, duration `snapUnitBeats()` `:394`, interval loop `:396-402` = the
injection point, finalize `:403-407` leaves the chord SELECTED); ghost preview mirrors `:2176-2194`
(`mStampPos` `:1198`). Menu: Chords = top-level index 3 (`:4055-4060`, handler `:4114`);
`selectChord` `:3276-3282` arms the hidden Stamp tool (`.h:648`).

**Resize (single-note today):** state `.h:307-317` (`mResizeNoteIdx` `:308`, `kResizeZone` 6px
`:226`); entry points `:1318-1329` (Draw) + `:1447-1458` (Select); `noteIndexNearRightEdge`
`:471-495`; drag apply `:1621-1646` (min-dur `:1626`; left-edge adjusts start+dur `:1635-1636`);
release `:1779-1785`. Multi-drag template = Move: `mMoveIndices` `.h:293`,
`expandForGroups(...)` at `:1352`/`:1477`, shared delta `:1603-1614`; `expandForGroups` impl
`:922-943`; `groupId` `PatternManager.h:70` (stamps do NOT set it — per D that stays true).

**Scale machinery:** grid state `.h:356-359`; `setScale` `:340-349` — GOTCHA: fills `mScaleInKey`
only when active (`:344-347`) → Task 2 makes the table unconditional, behaviors stay gated.
Container state persists independent of the toggle: `.h:695-698`; `updateScaleFromUI` `:3188-3193`;
`kScaleDefs` `:28-42` (13 scales). Primitives: `snapPitchToScale` `:356-369` (nearest, up wins
ties); `nextScalePitch` `:371-383` (= the collision-resolver step). Gated behavior sites (keep
gated): `:1608` move-snap, `:802` transpose, `:392` stamp-root, `:1912` row tint, `:3895` generate.
Mode-1 ancestor: `toolGenerateChords` `:3871-3972` — in-scale thirds `:3912-3914`, `pcToMidi`
`:3917-3922`, stack+clamp `:3929-3934`. Snap toggle: Scale menu id 28 (`:4038`, handler `:4112`,
`setScaleActive` `:3258-3262`). Undo brackets: stamp `:391`/`:405`, resize `:1322`/`:1782`.
