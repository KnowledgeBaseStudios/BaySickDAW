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
(inserts that already exist, free routing, buses) and its tabs derive from
buses rather than from engine instances.

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
| - | Generator name | "Riff Machine" -> **Tune Generator** (brand docket item 1, ruled 2026-09-02; applies to BaySickDAW too). |

## Open (in the order the answers depend on each other)

4. The mixer model - insert count, routing rules, sends, sidechain, per-insert
   properties, instrument-to-insert assignment, Master, multi-out instruments.
   Jeff defines the FL behavior; the fork matches it.
5. Tabs from buses - what makes a strip a bus, what a bus tab shows, where an
   instrument lives before it is routed, where Master fits, the instrument list.
6. Hosting as the only path - what VST3 hosting still lacks (from the code map).
7. Project files, product name, paths, settings.
8. Manual, installer, credits / notices; whether In The Weeds exists in a
   closed-source product.
9. Where the fork lives while it is built (tracked here or not) and its build.
10. Sequencing against QA-Solstice T5, the QA-ManualPress close, the V1 campaign.

## Legal prerequisites for the closed-source shell (from the DSP Portability Matrix)

- Rubber Band (GPL) is not in the shell - it lives only in the vocal tools,
  which are dropped.  The Builder's clip stretch is our own `PhaseVocoder`.
- ASIO SDK: Steinberg's proprietary licence branch (free, signed), not GPLv3.
- VST3 SDK (bundled by JUCE): Steinberg's VST3 licence (free, signed).
- LAME: DLL only (3c).
- JUCE: the free tier covers closed-source at current revenue (Jeff, 2026-09-01).
- Ship the notices: fontaudio OFL 1.1 + CC BY 4.0 texts, WebView2 notice,
  VST / ASIO trademark acknowledgements, a complete About / credits surface.
