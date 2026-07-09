# Running Notes — QA-TempoMap (steady-marching-ibex)

> Append-only mid-batch log. New entry at every checkpoint. Under the BULK RUN this batch's Work
> Log entry is drafted at code-complete, HELD here under `## Held Work Log entry (apply at section
> pass)`, and applied only when its Master Test Plan §B section passes.

Pair file: [`Batch Plans/steady-marching-ibex.md`](../Batch Plans/steady-marching-ibex.md).
Conventions: Main Plan §0 + [`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md).

## 2026-07-08 — G1 group open — plan approved + surface map (source-verified)

Locked: 11 (stepped markers; ramps stay automation's job), B (last-writer-wins), E (field edits
base / displays live effective). Derived: markers song-mode only; marker bar math = uniform
4 beats/bar (song TS decorative). Verified refs:

**Anchor (being replaced):** `StandaloneApp.h:56-89` (transport atomics `:63-68`; anchor 4-tuple
`:82-85`; invariant doc `:73-81`); `deriveBeat` seqlock read `StandaloneApp.cpp:142-158`;
`publishAnchor` write `:160-171`; `setBPM` re-base `:228-234`; `start` `:212-218`; `reset`
`:220-227`; `seekTo` `:235-244`; `getPosition` `:246-256`; `advanceBlock` `:173-210` — sole
`mSamplePos` writer, loop wrap measured from the anchor line (`:181-208`, why-comment `:184-191`),
NEVER writes the anchor (audio thread reads only — preserve).

**Writers (all message-thread):** field/tap `StandaloneEditor.cpp:757-764` (origins
`GlobalTransportBar.cpp:497/501/787`); global_tempo applicator `:604-616` (range 20-300
`:602-603`) — NOT an APVTS param: the audio-thread automation pass skips it
(`PluginProcessor.cpp:1704-1772` applies only via apvts `:1770-1771`); it runs on the editor's
30 Hz timer (`:1381` start, body `:2903-2941`, beat source `:2893`) = the coarse path the map
replaces; project load `:10804-10821` (setBPM `:10814`, called `:10918`). Base store:
`PatternManager` `mGlobalTempo` (`.h:623-624`, `:638`; save `.cpp:837`, load `:1167`) — keeps
meaning BASE tempo.

**Consumers:** one `PositionInfo` per block `PluginProcessor.cpp:1276-1279`; PR scheduler
`:1375-1420` (beats-per-sample `:1375-1378`, loop-seam `:1390-1400`, straddle `:1410-1420`);
automation beat→bar `:1708` (`kBeatsPerBar = 4.0` `:1707`); strip tasks `:1896-1898`/`:1916-1917`;
`BlockContext.bpm` `:1954-1956`; MIDI recorder `:2009-2019`; metronome `:2154-2187` (accents per
pattern tsNum `:2168-2169`); clip ratio `:598-599`/`:975-979` (PV gate `:998-1001`, set `:1006`);
rebuild captures `originalBPM` `:2688` (kBPB=4.0 "decorative TS" comment `:2636-2638`). UI:
`BuilderPage.cpp:5732-5744` (`/4.0` at `:5738`/`:5743`); pages `LayersPage.cpp:266` /
`BassPage.cpp:254` / `DrumPage.cpp:398` / `PianoRollPage.cpp:56`; BPM field resync
`GlobalTransportBar.cpp:750-757` (E's display half rides this); loop/TS push
`GlobalTransportBar.cpp:619-631` + `StandaloneEditor.cpp:782-866`.

**D-2 substrate:** structs `PatternManager.h:248-259`; vectors `:635-636`; API `:475-490`; impls
`.cpp:341-413` (add-sort `:344-346`, near-bar `:368-379`, TS same-bar replace `:394-397`); XML
save `:1108-1130` / load `:1506-1536`; ruler paint `BuilderPage.cpp:1697-1732` (`barToX`
`:1703`/`:1719`); right-click menu `:2895-2935` (dispatch `:2927-2933`); prompts `:2937-3015`;
Alt+T keys `:5000-5006`; tooltip `:2863-2889`.

**Publish pattern (chosen):** immutable sorted `vector<TempoSegment{startSample,bpm,startBeat}>`
behind an atomic shared_ptr swap + off-thread retire — RCU precedent `AudioClipSnapshot`
(`PluginProcessor.cpp:2640-2645`); double-buffer precedent `HarmonicEngine.h:130-145`/`.cpp:33-188`.
Seqlock rejected for a variable-length list. TS math available (read-only, unused by playback):
`getEffectiveTimeSigAtBar` `PatternManager.cpp:416-425`, `getBeatsPerBarAtBar` `:427-433`,
`beatToBarAndBeatInBar` `:443-493`, `barStartBeat` `.h:505`.
