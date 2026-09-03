# KBS DAW Fork - Design (spec)

**Status: approved design, 2026-09-02** (Jeff: "proceed with making the plan").
Program plan: [`KBS DAW Fork - Plan.md`](KBS DAW Fork - Plan.md).  First batch:
[`Batch Plans/lithe-lopping-lynx.md`](Batch Plans/lithe-lopping-lynx.md).  Code map:
[`Research Reports/lite-shell-map-2026-09-02.md`](Research Reports/lite-shell-map-2026-09-02.md)
(+ `-reports.md`, eleven reader reports verbatim).

## 1. What it is

A one-time FORK of BaySickDAW's SHELL, built here in `Documents\BaySickDAW\KBS DAW\`
as its own git repository (ignored by this one), then moved to the KBS side by
Jeff.  BaySickDAW continues as the full free GPL product.  The fork becomes
**KBS DAW**: a free, closed-source (no GPL) DAW shell in which every instrument
and effect is a hosted VST3 and none of BaySickDAW's built-in players or
effects exist.  Its mixer is FL-style - a growable bank of inserts, any insert
routes to any insert with a level per route, a bus is just an insert others
point at.  Instruments live in a list window; the ribbon is five icon buttons.

## 2. Decisions (the requirements - Jeff, 2026-09-02)

| # | Question | Decision |
|---|---|---|
| 1 | Purpose | BaySickDAW stays the full free product.  The shell ports to KBS as a separate product - free, closed source, no GPL. |
| 2 | Relationship | (b) One-time fork.  Built here under `KBS DAW/`, then moved; nothing appears on the KBS side until Jeff moves it. |
| 3 | Keep | Main frame + contained windows; ribbon (reworked); Builder (arrangement, audio clips on the grid with our own stretch, automation lanes); Piano Roll, Event Editor, the generator; mixer + routing (reworked); transport / tempo / metronome; audio device + ASIO; MIDI input + MIDI Learn; VST3 hosting for instruments and effects incl. the out-of-process helpers; effect racks holding VST3 slots; project files, undo, settings; offline export with loudness normalize; freeze; manuals window + F1; installer. |
| 3 | Drop | Every built-in instrument (Solstice, Synth, Bass, Player, Vocal, NAM/IR, Pedals, Guitars, Basses, RustyDrums, SlideSampler); every built-in effect and pedal DSP; the strip EQ engine; the Core Library fetcher and all factory presets / kits / templates; the Clips, Vox, Inst and per-engine tab types. |
| 3a | Live input | A property of the mixer insert - routed on the insert, detached from any player. |
| 3b | EQ slots | The rack keeps two EQ slots as load buttons: `+ Add Pre EQ` (left) and `+ Add Post EQ` (right), filled by the free KBS Plugins EQ.  No built-in EQ engine. |
| 3c | MP3 | Kept.  LAME ships as a separate DLL (LGPL: user-replaceable, credited), never statically linked. |
| 4.1 | Insert count | A starting bank that grows on demand; FL-scale maximum; the user can keep adding. |
| 4.2 | Routing | Any insert routes to any insert, Master by default; a bus is just an insert others point at.  Every route carries its own level; any-to-any; no per-insert send cap. |
| 4.3 | Sidechain | Any route can be flagged sidechain.  A hosted effect never auto-picks a key input; the user picks it. |
| 4.4 | Per-insert | Audio-input source (none / device channel pair), input gain (new), level, pan, mute, solo, polarity, width, effect slots with the pre-EQ and post-EQ slots at either end, routes out, record-arm to disk, name, color. |
| 4.5 | Effect slots | 10 per insert plus the two EQ slots (12); the rack window is resized to fit. |
| 4.6 | Instrument to insert | Set ON THE INSTRUMENT (FL's channel-settings insert field): the instrument's row and window carry an insert selector. |
| 4.7 | Master | An insert like the others (rack, EQ slots), the only one that cannot route anywhere. |
| 4.8 | Multi-out | Each output of a multi-output instrument assignable to its own insert from day one; hosted-instrument latency in delay compensation.  In this build.  Mechanism (Jeff, 2026-09-02): the plugin owns its INSIDE mapping (which internal channel hits which output bus, set in the plugin's own UI); the host owns the OUTSIDE - enumerate the plugin's output buses and names, activate them, render every active bus into its own buffer, and give each a destination insert through a per-instrument output map, with a one-click "assign remaining outputs to new inserts" helper. |
| 5 | Tabs-from-buses | Retired: it tied navigation to routing (tab churn as you mix; no home for Master-routed instruments; multi-out feeds several inserts; grouping-by-bus already exists in the Mixer and Effects views). |
| 5A | Ribbon | Five icon buttons: Builder, Mixer, Effects, Piano Roll, Instruments.  No per-engine instance tabs.  Freed width is free space; nothing in it for now. |
| 5B | Instruments window | A list window like the effects rack: one row per loaded instrument - name, color, insert selector (4.6), mute, solo, open editor, open piano roll - with `+ Add instrument` at the bottom. |
| 6 | Hosting gaps in this build | Sidechain into a hosted instrument; bridged latency updates after load; MIDI out of a plugin, plugin-to-plugin MIDI, MIDI-effect plugins; per-instrument MIDI input device + channel, more than one live target; preset browsing for hosted plugins (`.vstpreset` / program list); MIDI Learn onto hosted-plugin params; bridging an instrument on demand; plus 4.8. |
| 6.7 | Resizable plugin windows | In - a drag handle ONLY for plugins that declare host-resize support and can veto or snap sizes; never host-forced.  Plugin-initiated resize keeps working. |
| 6.9 | 32-bit bridge | In; tested at the bridge task with a Win32 build of a Steinberg SDK example plugin (Jeff has no 32-bit VST3). |
| 7.1 | Name | **KBS DAW**. |
| 7.2 | User-data root | `Documents\KBS DAW\` resolved at runtime from the Documents folder (settings, audio settings, plugin list, keymap, projects).  Independent of the source location: moving the source needs no data handoff.  Folder name assumed `KBS DAW` pending Jeff. |
| 7.3 | Project files | Same XML shape, new root tag `KBSDAWProject`; a BaySickDAW project is refused cleanly. |
| 7.4 | Instrument presets | The page-preset container (plugin state + its rack) stays as the instrument preset, alongside plugin-native presets (6.5). |
| 8.1 | In The Weeds | Not in the fork's manual (it publishes the source).  In View + In Depth only. |
| 8.2 | Manual | The pipeline (harness, generator, control tables) reused with shell chapters only; figures re-shot from the fork. |
| 8.3 | Notices | Built in from day one: About box listing every third-party component with its licence, a `NOTICES.txt` beside the exe, VST and ASIO trademark lines, the LAME credit. |
| 8.4 | Installer | The NSIS installer renamed (product, paths, registry keys); packages the LAME DLL and both helpers; no Presets / Kits / Templates inputs. |
| 9.1 | Where it lives | Its own git repository at `Documents\BaySickDAW\KBS DAW\`, ignored by this repo; the folder moves with its history. |
| 9.2 | Build | Its own `CMakeLists.txt` + `do_build.bat`, building into `KBS DAW\build\`, helpers included. |
| 9.3 | Dependencies | JUCE and the kept libraries copied into the fork: self-contained and movable. |
| 10 | Sequencing | Copy now; brand-docket renames are applied in both trees later.  QA-Solstice T5 and the QA-ManualPress close wait. |
| 11 | Carve strategy | Copy then cut, ledger-driven, the app runnable at every task boundary; clean-start repository (see section 10). |
| - | fontaudio | Not in the fork (Jeff, 2026-09-02): KBS ships its own glyphs.  Prune-listed; its module and link go with the strip EQ (the EQ rail was the only consumer); its OFL / CC BY obligations drop off the Ship list. |
| - | Generator name | "Riff Machine" -> **Tune Generator** (brand docket item 1; both products). |

## 3. Target architecture

Same process shape as BaySickDAW - one `AudioProcessor` owning the graph, a
model layer owning instruments and inserts, pages as non-owning views inside
contained native child windows - with the engine layer replaced by hosting.
Internal C++ class names keep their BaySickDAW names where nothing user-facing
depends on them (`BaySickDAWProcessor`, `BaySickGraph`, `EngineRig`); product
identity lives in strings, paths, tags and the installer, not in identifiers.

| Component | Responsibility in the fork | Today's owner |
|---|---|---|
| **Instrument model** | The list of loaded instruments: stable id, name, color, plugin description + state, insert assignment per output, MIDI input device + channel, mute, solo.  Creates / destroys hosted instruments; the only engine kind. | `EngineRig` (`TabKind::Plugins` arm only survives) |
| **Insert bank** | The growable list of inserts: stable slot index, name, color, input source, input gain, level / pan / mute / solo / polarity / width, rack (12 slots), routes out (dst, level, sidechain flag), arm.  Master = slot 0, no routes out. | `BaySickGraph` + `PluginProcessor` per-strip params, rebuilt from a bank rather than from tab events |
| **Routing graph** | Edges from routes; cycle detection; topological order; delay compensation incl. hosted-instrument latency; every insert SUMS its predecessors. | `RoutingGraph` (+ the task prologue change) |
| **Hosting** | Scan, allowlist, in-process + bridged instantiation, editors, state, automation, sidechain (instrument and effect), MIDI in / out / thru, latency reporting, presets, multi-out bus layouts. | `Source/Hosting/*`, `HostedPlugin`, `HostedPluginEffect`, the two helper exes |
| **Rack** | 12 slots per insert: pre-EQ, 10 free, post-EQ; VST3 only. | `EffectRack` collapsed to `None` + `VST3Plugin` |
| **Sequencer** | Builder (arrangement, audio clips + our stretch, automation lanes), Piano Roll, Event Editor, Tune Generator; patterns target instruments by stable id. | `PatternManager`, `BuilderPage`, `PianoRoll*` |
| **Shell UI** | Main frame, contained windows, ribbon (5 icons), Mixer, Effects page, Instruments window, plugin windows, settings dialogs, About / notices, manuals window. | `StandaloneEditor`, `WorkspaceWindow`, `RibbonTabBar`, `MixerPage`, `EffectsPage`, new `InstrumentsWindow` |
| **Persistence** | `KBSDAWProject` XML: instruments, insert bank, routes, racks, arrangement, automation, settings; undo. | `ProjectManager`, `StandaloneEditor` serialize / restore |
| **Transport + device** | Playhead, tempo, metronome, audio device + ASIO, MIDI devices. | `StandalonePlayHead`, `StandaloneApp` |
| **Export + freeze** | Offline render, loudness normalize, per-instrument freeze. | `BuilderPage::runOfflineLoop`, the freeze path |

## 4. Data model changes

**Instrument** (new record; replaces the `(TabKind, pageIndex)` identity):
`{ id: uuid, name, color, plugin: PluginDescription, state blob, outputs: [ { busIndex, insertSlot } ], midiInputDevice: name-or-"all", midiChannel: 0-16 (0 = all), muted, solo, frozen }`.
Undo owner tag `inst:<id>`; automation lane prefix `inst_<id>_`; instrument
preset = `{ state, rack of its primary insert }` (7.4).

**Insert** (new record; replaces `mixer_<family>_<i>`):
`{ slot: int (stable, 0 = Master), name, color, input: { deviceChannel, stereo } | none, inputGainDb, levelDb, pan, mute, solo, polarity, width, arm, rack: 12 slots, routes: [ { dstSlot, levelDb, sidechain } ] }`.
Param ids `insert_<slot>_<param>`.  The bank grows by appending slots; slots are
never renumbered; deleting an insert retires its slot number.

**Route** = an edge with a level; `sidechain=true` edges feed the destination's
sidechain receive buffers and do not sum into its audio.  Master (slot 0) has
no routes; every other insert defaults to one route `{ 0, 0 dB, false }`.

**Pattern targeting**: notes and automation in `PatternManager` target an
instrument by `Instrument.id`, not by `(EngineKind, index)`.  The piano roll's
context dropdown lists instruments.

**Project XML** root `KBSDAWProject`, children `<Instruments>`, `<Inserts>`
(with nested `<Routes>` and `<Rack>`), `<Arrangement>`, `<Automation>`,
`<Settings>`.  A `BaySickDAWProject` root is refused with a message (7.3).

## 5. Data flow

**Audio.** Each instrument renders into the insert(s) its outputs are assigned
to (one per output bus for multi-out).  Multi-out: the host queries the plugin's
output bus count and names, activates the buses named in the instrument's
output map, renders once per block into one buffer per active bus, and sums
each buffer into its mapped insert; an active bus with no destination is
rendered and discarded; the plugin's internal channel-to-bus assignment is the
plugin's own business.  The bridged helper negotiates the same layout and
carries N buses of audio per block.  Every insert: sum predecessors (routed
inserts + its own instruments + live input) -> input gain -> pre-EQ slot ->
10 slots -> post-EQ slot -> polarity / width -> fader / pan -> meters -> out
along its routes (each scaled by the route level; sidechain routes go to the
destination's key buffers instead).  Master sums and feeds the device.  Order
is the topological order from the routing graph; delay compensation includes
hosted-instrument and rack latency, re-read when a plugin reports a change.

**MIDI.** Devices are opened individually; an instrument receives from its
selected device (or all) on its selected channel (or all).  Scheduled notes from
patterns go to the pattern's target instrument.  Plugin MIDI output is
collected per block and delivered to any instrument that selects that plugin as
its MIDI source (plugin-to-plugin MIDI, MIDI-effect plugins).

**Persistence.** Save = model records to XML (section 4).  Load = refuse a
foreign root tag; create inserts from `<Inserts>` first (slots), then
instruments (bound by slot), then routes (validated against the cycle check),
then arrangement + automation (validated against existing instrument ids;
orphans reported through the missing-file report the way missing samples are).

## 6. UI surfaces

- **Ribbon:** five icon buttons (Builder, Mixer, Effects, Piano Roll,
  Instruments) + transport as today.  The freed width is empty for now.
- **Instruments window:** the list (5B).  `+ Add instrument` opens the plugin
  picker (today's allowlist picker).  Row controls: name (rename), color, insert
  selector (per output for multi-out - a submenu listing the plugin's own bus
  names, Out 1 defaulting to the instrument's insert, the rest unrouted until
  assigned, plus "Assign remaining outputs to new inserts"), mute, solo,
  editor, piano roll, and a right-click menu: replace plugin, duplicate, save / load
  instrument preset, freeze, bridge on demand, MIDI input device + channel,
  delete.
- **Mixer:** the insert bank in slot order, Master pinned; `+ Add insert`; each
  strip: input source + gain (utility row on every strip), fader, pan, mute,
  solo, polarity, width, arm, routes (the "+" menu offers every other insert,
  each with a level and a sidechain toggle), the rack button.  The cable overlay
  stays.
- **Effects page:** as today - a bus and what feeds it - over inserts.  The rack
  view holds 12 slots (pre-EQ | 10 | post-EQ), resized to fit.
- **Plugin windows:** contained windows as today; drag handle only when the
  plugin declares host-resize support (6.7).
- **Settings:** Audio (unchanged), File Settings shrunk to freeze / capture
  rows, Options without "Get Sound Content".
- **About / Help:** About lists every component + licence; Help opens the
  shell-only manual.

## 7. Error handling

- Missing plugin on load: the existing dead-marker path (state retained,
  slot silent, "Retry" available) - unchanged.
- Routing cycle: refused at the menu (the cycle check runs before the edge is
  written); a cycle found in a loaded project drops the offending route and
  reports it.
- Plugin crash / deadline: the existing per-slot containment - unchanged;
  extended to instruments run through the bridge on demand.
- Foreign project (BaySickDAW root tag): refused with an explanatory message,
  never half-loaded.
- Orphaned pattern targets (instrument id not present): reported, notes kept,
  silent until reassigned.
- Bridged plugin never answers a latency query: last known value stays; the
  compensation warning surfaces in the strip.

## 8. Testing

- **Build gate per task**, the fork's own `do_build.bat` judged by the same six
  exit codes and link lines.
- **Headless launch smoke per task:** the shot harness constructs the
  processor, pages and menus with no window or device; running it after every
  carve task proves the app still constructs (`KBS DAW.exe --shot "Builder"`).
- **Milestone smokes** (Jeff, Debug then Release): each batch plan carries its
  own walk sheet; M2's golden test is "empty project, add a VST instrument from
  the Instruments window, hear it through Master", and every later milestone
  re-runs it.
- **No unit-test harness** exists in this codebase beyond the EQ tests (which
  die with the EQ); none is introduced by the fork.

## 9. Not in scope

Built-in DSP of any kind; VST2 / CLAP / AU hosting; a browser panel; step
sequencer; project compatibility with BaySickDAW; the freed ribbon space;
the KBS Plugins EQ itself (KBS side); code signing; an updater.

## 10. Carve strategy and milestones

**Repository:** a clean start (`git init`, first commit = the pruned shell),
not a `git clone`.  Reason: this repo's `.git` is 1.16 GB and its history
carries 124 MB of vendored libraries including GPL Rubber Band, 830 preset
files and every engine ever written - the opposite of "just the files you
need".  `git blame` for archaeology stays available here; the fork's README
records the source commit.  (Jeff's caveat 2026-09-02 - flip to a full clone
by saying so before batch 1 starts.)

**Prune list** (tracked in BaySickDAW, not copied): `Presets/`, `Kits/`,
`Templates/`, `Resources/Acoustic IRs/`, `Resources/Tape/`,
`Assets/big_rusty_drums.png`, `libs/rubberband`, `libs/world`,
`libs/signalsmith-stretch`, `libs/signalsmith-linear`, `libs/sfizz`,
`libs/NeuralAmpModelerCore`, `libs/fontaudio` (KBS glyphs replace it), `Plans & Specs/` (the fork gets its own, seeded
with this spec + plan), `Files For Claude/`, `.claude/` (the fork gets its own
CLAUDE.md), `Tools/gen_factory_presets.py`, `Tools/EqTests/`,
`Tools/rusty_kit_hitboxes.txt`, `run_eq_tests.bat`, `Manuals/src-m3/`,
`Manuals/src-m2/instrument/`, `Manuals/src-m2/mixing-effects/` (rewritten at
M5), engine figure groups, the built installer exes.  Copied: `Source/`,
`juce/`, `libs/{asiosdk,concurrentqueue,lame,webview2}`,
`Resources/Filmstrips/`, the remaining `Assets/`, `Manuals/` pipeline +
shell chapters, `Installer/*.nsi`, `CMakeLists.txt`, `do_build.bat`,
`do_configure.bat`, `make_installer.bat`, `BUILDING.md`, `.gitignore`,
`.gitattributes`, `THIRD_PARTY_LICENSES.md` (rewritten at M5), a new `LICENSE`.

**Milestones** (each a batch; details in the program plan):

| ID | Milestone | Exit criterion |
|---|---|---|
| KBS-Seed (M0 + M1) | Clone-and-prune builds untouched; the carve lands - engines, DSP, effect UI, engine pages, strip EQ, Core Library gone; enums collapsed; dead code hunted | The fork builds green, launches, and a hosted instrument on the legacy Plugins tab plays through Master |
| KBS-Core (M2) | Instrument model + Instruments window + five-icon ribbon; patterns target instrument ids; project root tag | Golden test: empty project, `+ Add instrument`, hear it through Master, save, reload |
| KBS-Mixer (M3) | Insert bank, every insert sums, route levels, bus-as-role, sidechain flags, live input + gain per insert, instrument insert selector, multi-out output map, latency compensation, the 12-slot rack | FL-style walk: route an instrument to an insert, bus two inserts into a third, sidechain a compressor, record live input, map a multi-out drum VST's outputs to inserts and hear kit pieces on separate strips |
| KBS-Host (M4) | The remaining hosting gaps (6.1-6.8), 32-bit bridge test (6.9) | Each gap has a scenario on the walk sheet and passes |
| KBS-Ship (M5) | Identity (name, data root, strings, helper names, bridge id), LAME DLL, notices + About, installer, shell-only manual | Installer output installs and runs on a clean user account; manual opens on F1 with zero engine content |

## 11. Legal prerequisites for the closed-source shell

- Rubber Band (GPL) is not in the shell - it lives only in the vocal tools.  The
  Builder's clip stretch is our own `PhaseVocoder`.
- ASIO SDK: Steinberg's proprietary licence branch (free, signed).
- VST3 SDK (bundled by JUCE): Steinberg's VST3 licence (free, signed).
- LAME: DLL only (3c), with the LGPL notice and the LAME credit.
- JUCE: the free tier covers closed-source at current revenue (Jeff,
  2026-09-01).  The licence text governs at publish time.
- Ship the notices (8.3): VST + ASIO trademark lines, WebView2, concurrentqueue,
  LAME, JUCE's bundled dependencies.  fontaudio is gone with the EQ.  The fork's
  `LICENSE` is KBS's EULA, not GPLv3.

## 12. Code facts that shape the design (from the code map, 2026-09-02)

**Size.** `Source/` is 411 files / 234,169 lines; the engine + DSP + effect-UI
bucket is 114,322 lines (48.8%), so the shell is roughly 120k lines.  Coupling
to built-in engines is concentrated: `StandaloneEditor.cpp` 120 sites,
`EffectPresetIO.cpp` 90, `SlotComponent.cpp` 89, `EffectParamMap.cpp` 51,
`EffectEditorPanels.cpp` 43, `EffectRack` 51, `EngineRig` 30, `RibbonTabBar` 29,
`PluginProcessor` 36.  `EffectType::` built-in enumerators appear ~430 times
against 11 `VST3Plugin` sites.

**Mixer / routing - exists today:** sends with level (-60..+6 dB) and pre/post,
four per strip; strip-generic sidechain (four receives per strip, delay-matched
key taps, cycle + topo participation); `RoutingGraph` cycle detection + Kahn
ordering with no legality of its own; aux-to-aux summing; live-input params
(`_inputChannelIdx`, `_inputChannelStereo`, `_listen`, `_monitorMode`) and
`_arm` as strip params; generic engine-to-strip plumbing that already carries
hosted VST3 instruments.

**Mixer / routing - the gaps:** (1) only bus and master tasks SUM predecessors;
every other task clears and renders its own source, so an insert-to-insert edge
is built, ordered and dropped; (2) strips are created by tab events, never as a
bank; identity is family + index; the instrument-to-strip binding is
index-derived with no parameter to reassign it; (3) "bus" is 17 hard-coded ids
with hand-written accessors; (4) sends are aux-only behind a 10-branch family
whitelist; (5) live input and arm are gated to Vox / Inst; (6) no input gain;
(7) hosted-instrument latency is not in PDC; (8) main-out edges are unity.

**Tabs.** Identity in four records and five parallel enums (`TabKind`,
`TabType`, `EngineKind`, `StripKind`, `InsertKind`) mapped by hand in five
places.  With hosted plugins only, `TabKind` collapses to `Plugins`; the ribbon
already hides zero-instance types; `PluginsPage` is the thinnest page.  Bus
membership enumeration exists in `MixerPage::layoutScrollContent` and
`EffectsPage::addBusAndMembers`.  Builder rows are not tabs.

**Hosting - missing today:** sidechain into a hosted instrument; hosted
latency in PDC; bridged latency updates; multi-out (bus 0 only); plugin MIDI
out / thru; per-track MIDI input and multiple live targets; `.vstpreset`
browsing; MIDI Learn onto plugin params; user-resizable plugin windows; the
32-bit bridge untested.  Complete: scan / allowlist / crash isolation,
instruments as engines, effects in any rack slot, editors in-process and
bridged, state, automation by stable id, transport, containment, both helpers.

**Build.** No shell / engine split in CMake (one flat list); the sfizz and NAM
gates are dead (headers included unconditionally) so engines go before their
libs; LAME is statically linked today; JUCE's bundled ASIO headers compile.
Every path- and name-bound identifier is listed in the map's section 9.

## 13. Open items

- 7.2 data-folder name (`KBS DAW` assumed).
- What, if anything, goes in the freed ribbon width.
- The brand-docket renames (both trees, 10b).
- The Tune Generator's eight step names (brand docket item 1, "still open").
