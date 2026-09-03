# KBS DAW Fork - Design (living doc, workshop started 2026-09-02)

**Status: in workshop.**  Decisions land here as Jeff makes them.  The code map is
[`Research Reports/lite-shell-map-2026-09-02.md`](Research Reports/lite-shell-map-2026-09-02.md)
(gap map) + `lite-shell-map-2026-09-02-reports.md` (eleven reader reports verbatim).
The critic's 11 areas that did not get a second read (render-engine internals, audio
device init, templates, strip-EQ removal depth, export + loudness + VersionCapture,
theming, manuals window, project lifecycle shell, Core Library call sites, auto-freeze,
installer) are implementation-plan detail, mapped at the writing-plans phase.  When the design is approved this becomes the spec the
implementation plan is written from.

## What it is

A one-time FORK of BaySickDAW's SHELL, built here in a `KBS DAW/` folder that
Jeff moves to the KBS side himself.  BaySickDAW continues as the full free
product; the fork becomes a free, closed-source (no GPL) DAW shell under KBS
in which every instrument and effect is a hosted VST3 and the built-in
players / effects of BaySickDAW do not exist.  The fork's mixer is FL-style
(inserts that already exist, free routing, buses).  The per-instance tab model is
replaced: the ribbon becomes five icon buttons and instruments live in a list
window (decision 5, which retired the earlier tabs-from-buses idea).

## Decisions so far (Jeff, 2026-09-02)

| # | Question | Decision |
|---|---|---|
| 1 | Purpose | BaySickDAW stays the full free product.  The shell ports to KBS as a separate product - free, closed source, no GPL. |
| 2 | Relationship | (b) One-time fork.  "A lot of moving parts over here that won't be what we are doing over there."  Built here under `KBS DAW/`, then moved; nothing appears on the KBS side until Jeff moves it. |
| 3 | Keep list | Main frame + contained windows; ribbon + tabs (reworked); Builder (arrangement, audio clips on the grid with our own stretch, automation lanes); Piano Roll, Event Editor, the generator; mixer + routing (reworked); transport / tempo / metronome; audio device + ASIO; MIDI input + MIDI Learn; VST3 hosting for instruments and effects incl. the out-of-process helpers; effect racks on strips holding VST3 slots; project files, undo, settings; offline export with loudness normalize; freeze; manuals window + F1; installer. |
| 3 | Drop list | Every built-in instrument (Solstice, Synth, Bass, Player, Vocal, NAM/IR, Pedals, Guitars, Basses, RustyDrums, SlideSampler); every built-in effect and pedal DSP; the Core Library fetcher and all factory presets / kits / templates; the Clips, Vox and Inst tab types. |
| 3a | Live input | Stays, as a property of the mixer strip: routed on the strip, detached from any player. |
| 3b | Strip EQ | The rack keeps its two EQ slots as load buttons like the other slots: `+ Add Pre EQ` on the left, `+ Add Post EQ` on the right.  They are filled by the free KBS Plugins EQ (Jeff has the KBS side add it).  No built-in EQ engine in the fork. |
| 3c | MP3 | Kept.  LAME ships as a separate DLL (LGPL: replaceable by the user, credited in About), never statically linked into the closed binary. |
| 4.1 | Insert count | (b) A starting bank that grows on demand; the maximum is high, FL-scale, and the user can keep adding. |
| 4.2 | Routing | (a) Any insert routes to any insert, Master by default; a "bus" is just an insert that others point at.  FL-style in full: every route carries its own level knob, any-to-any, no per-insert send cap (today's four-send limit goes). |
| 4.3 | Sidechain | (a) Any route can be flagged sidechain.  A hosted effect never auto-picks a key input - the user picks it, always. |
| 4.4 | Per-insert | (a) Audio-input source (none / device channel pair), level, pan, mute, solo, polarity, width, effect slots with the pre-EQ and post-EQ slots at either end, routes out, record-arm to disk, name and color.  Input gain is new (nothing exists today). |
| 4.5 | Effect slots | 10 per insert plus the two EQ slots.  Consequence: the effects rack window is resized to fit 12. |
| 4.6 | Instrument to insert | The routing is set ON THE PLAYER, the way FL's channel settings carry the insert field - the instrument window gets an insert selector. |
| 4.7 | Master | (a) An insert like the others - rack, EQ slots - and the only one that cannot route anywhere. |
| 4.8 | Multi-out instruments | (a) Each output assignable to its own insert from day one.  Jeff: "definitely needs to be fixed for this port build" - multi-out AND hosted-instrument latency compensation are in scope. |
| 5 | Tabs-from-buses | **Retired** (workshopped 2026-09-02).  It tied navigation to routing: tabs would churn as you mix, an instrument routed straight to Master has no home except a "Master tab" that is just an instrument list, multi-out instruments (4.8) feed several inserts at once, and grouping-by-bus already exists in the Mixer (groups strips by destination) and the Effects page (bus + members). |
| 5A | Ribbon | (a) Five icon buttons: Builder, Mixer, Effects, Piano Roll, Instruments.  No per-engine instance tabs.  The freed width is free space - what goes there is open (candidates only: window toggles, master meter, browser, snap / grid, project name); nothing for now. |
| 5B | Instruments window | (a) A list window like the effects rack: one row per loaded instrument - name, color, the insert selector (4.6), mute, solo, open editor, open piano roll - with "+ Add instrument" at the bottom.  The channel-rack idea minus the step sequencer (the Builder covers that). |
| - | Generator name | "Riff Machine" -> **Tune Generator** (brand docket item 1, ruled 2026-09-02; applies to BaySickDAW too). |

## Open (in the order the answers depend on each other)

4. ~~The mixer model~~ - decided (rows 4.1-4.8).
5. ~~Tabs from buses~~ - retired; ribbon + Instruments window decided (5A, 5B).
6. Hosting as the only path - what VST3 hosting still lacks (from the code map).
7. Project files, product name, paths, settings.
8. Manual, installer, credits / notices; whether In The Weeds exists in a
   closed-source product.
9. Where the fork lives while it is built (tracked here or not) and its build.
10. Sequencing against QA-Solstice T5, the QA-ManualPress close, the V1 campaign.

## Code facts that shape the design (from the code map, 2026-09-02)

**Size.** `Source/` is 411 files / 234,169 lines; the engine + DSP + effect-UI
bucket is 114,322 lines (48.8%), so the shell is roughly 120k lines.  The
coupling to built-in engines is concentrated: `StandaloneEditor.cpp` 120 sites,
`EffectPresetIO.cpp` 90, `SlotComponent.cpp` 89, `EffectParamMap.cpp` 51,
`EffectEditorPanels.cpp` 43, `EffectRack` 51, `EngineRig` 30, `RibbonTabBar` 29,
`PluginProcessor` 36.  `EffectType::` built-in enumerators appear ~430 times
against 11 `VST3Plugin` sites - the rack collapses to `None` + `VST3Plugin`.

**Mixer / routing - what already exists** (so the FL model is closer than it
looks): sends carry a level (-60..+6 dB) and pre/post, four per strip; sidechain
is complete and strip-generic (four receives per strip, delay-matched key taps,
cycle and topo participation); `RoutingGraph` has cycle detection and Kahn
topological ordering and imposes no legality of its own; aux-to-aux routing
already sums; live-input params (`_inputChannelIdx`, `_inputChannelStereo`,
`_listen`, `_monitorMode`) and `_arm` exist as strip params; the engine-to-strip
plumbing is generic and already carries hosted VST3 instruments.

**Mixer / routing - the real gaps:** (1) only the bus and master tasks SUM their
predecessors - every other task clears and renders its own source, so an
insert-to-insert edge is built, ordered and silently dropped; (2) strips are
created by tab/engine events, never as a bank; identity is family + index
(`mixer_<family>_<i>`), not a bank slot, and the instrument-to-strip binding is
index-derived at registration with no parameter to reassign it; (3) "bus" is 17
hard-coded ids with hand-written accessors, not a role a strip can take; (4)
sends are aux-only and the routing menu is a 10-branch family whitelist; (5)
live input and arm are gated to the Vox / Inst families; (6) no input gain
anywhere; (7) hosted-instrument latency is not in PDC; (8) main-out edges are
unity (no level on the default route).

**Tabs.** Tab identity lives in four records and five parallel enums
(`TabKind`, `TabType`, `EngineKind`, `StripKind`, `InsertKind`) mapped by hand
in five places.  With hosted plugins only, `TabKind` collapses to `Plugins`, the
ribbon already hides zero-instance types, and `PluginsPage` is "deliberately the
thinnest page".  The bus-to-members enumeration the bus tabs need already
exists in two places (`MixerPage::layoutScrollContent` bucketing by `_sendTo`;
`EffectsPage::addBusAndMembers`).  Builder rows are NOT tabs (500 free rows).

**Hosting - what is missing for "VST3 is the only path":** sidechain into a
hosted instrument; hosted-instrument latency in PDC; bridged latency updates
after load; multi-output instruments (bus 0 only, one strip per tab); MIDI out
from a plugin / plugin-to-plugin MIDI / MIDI effects; per-track MIDI input
device or channel filter and more than one live target; `.vstpreset` browsing;
MIDI Learn onto plugin params; plugin-tab window user-resizable; the 32-bit
bridge never tested with a real 32-bit VST3.  Complete today: scan / allowlist /
crash isolation, instruments as first-class engines, effects in any rack slot,
editor hosting in-process and bridged, state, automation by stable id,
transport, crash containment, both helpers built and installed.

**Build.** No shell / engine split exists in CMake (one flat source list); the
`BAYSICK_HAS_SFIZZ` / `BAYSICK_HAS_NAM_CORE` gates are dead (headers included
unconditionally), so engines must go before their libs; LAME is statically
linked today (becomes a DLL, 3c); JUCE's bundled ASIO headers are what compile.
Everything path- or name-bound is listed in the map's section 9 (AppPaths root,
six settings files, project root tag, helper exe names, bridge handshake id,
installer keys, undo owner tags, lane prefixes).

**One report line superseded:** the build reader asserted JUCE needs a paid
licence for closed source; Jeff's verified answer (2026-09-01) is that the free
tier covers it at current revenue.  The licence text governs at publish time.

## Legal prerequisites for the closed-source shell (from the DSP Portability Matrix)

- Rubber Band (GPL) is not in the shell - it lives only in the vocal tools,
  which are dropped.  The Builder's clip stretch is our own `PhaseVocoder`.
- ASIO SDK: Steinberg's proprietary licence branch (free, signed), not GPLv3.
- VST3 SDK (bundled by JUCE): Steinberg's VST3 licence (free, signed).
- LAME: DLL only (3c).
- JUCE: the free tier covers closed-source at current revenue (Jeff, 2026-09-01).
- Ship the notices: fontaudio OFL 1.1 + CC BY 4.0 texts, WebView2 notice,
  VST / ASIO trademark acknowledgements, a complete About / credits surface.
