# QA-Layout — Whole-app layout under the windowed shell — Plan (roomy-retiling-ocelot)

Canonical path: `Plans & Specs/Batch Plans/roomy-retiling-ocelot.md`. Paired running notes: `Plans & Specs/Running Notes/roomy-retiling-ocelot.md`.
**For execution:** per Main Plan §0 Rule 1, read Main Plan §0 + this batch's §5 entry, Carry-Forward §1–§3, all Implemented Work Log entries since QA-ModelShell, and the held layout entries in `Running Notes/grand-inverting-mammoth.md` (2026-07-28/29) before touching source.

## Context

- **Why:** QA-ModelShell shipped the contained-window shell; every page was written assuming the full content rect. Jeff's authored spec `Files For Claude/Final V1 Layout.md` is the primary input for this batch; the held scope from grand-inverting-mammoth supplements it. All spec conflicts were reconciled 2026-08-03 (planning session) — Jeff's doc wins throughout; supersessions on record below.
- **Position:** runs directly after QA-ModelShell + its post-close fix run (`1cd1f5d6`). **This batch is part of G4 and gets NO batch smoke** — all functional verification rides the G4 boundary smoke (`Files For Claude/G4 Boundary Smoke.txt` + Test Plans §B.31). Per-task build gates stand (bulk-run retired functional pauses, never the compile gate).
- **Verification debt carried to the G4 smoke:** everything bridged-specific from `1cd1f5d6` (program-name relay, param-touch relay, 32-bit path) has never executed — no 32-bit VST3 on hand. The G4 smoke must not assume it.
- **Risk:** widest UI surface of any batch to date; the mid-batch handoff (Jeff's sizing pass) splits it into pre-data and post-data halves. The caps task and the group-bus task touch engine/routing registration — Carry-Forward §1/§3 discipline applies.
- **Effort:** largest since QA-ModelShell. 14 tasks, one commit each.
- **Buckets:** UI / L&F / Theming, Mixer / Routing, Players, System Pages, Cross-cutting Infrastructure, Effects, Other / Platform / Deferred.

## Spec calls already locked (Jeff, 2026-08-03)

| ID | Decision | Reasoning |
|----|----------|-----------|
| L1 | Test Plans §B.31.0 rewritten in place to describe the diag-driven collection | Window-5 supersedes the drag-and-report table; the G4 walk must read correctly |
| L2 | Naming: "BaySickLiveInst" in the + menu; "LiveInst" on tab/strip/titles | + menu rows are BaySick-family names; short form elsewhere |
| L3 | + menu keeps "BaySickVocal" (singular); VST entry reads "VSTPlugin" | Jeff's strings, verbatim |
| L4 | Engine pickers deleted, not moved: Layers + Bass combos, Clips decorative combo, Drums "Pick a sound" button | Engine is chosen at the + menu before the page exists; supersedes the held "pickers onto title bar" note |
| L5 | Full-screen toggle ships on every window; button order [preset] [full-screen] [close] | **Reverses locked call 5a** (no-maximize); the toggle's restore path answers 5a's original objection |
| L6 | Diag = continuous append-to-file on every resize, capturing identity + WxH + effects Basic/Advanced mode; floors lifted while active | Basic/Advanced share one panel; multiple takes needed |
| L7 | Sequencing: title-bar work + Window-7 land BEFORE the sizing pass | Size the decoupled windows, not the current composites |
| L8 | Window-6 collapse + pedalboard one-pedal view GATED on sizing data; re-docketed mid-batch | Can't judge block splits without floors |
| L9 | Window-7: all five sub-pages (Pitch / Align / Vocal Chain / Pedals / NAM-IR) become own windows; in-page views retired; title-strip buttons open them | Simultaneous-adjust workflow; no sys cost (panels already always-built) |
| L10 | LiveInst primary window = the pedalboard; tab click fronts it; NAM/IR button on the pedalboard title strip; no Inst page window for live-input tabs | No empty page, no forced double-open |
| L11 | Ribbon dropdown "Pages:" section becomes a per-instance window list; LiveInst entry reads "Pedals" | Menu lists what the instance actually has |
| L12 | Mixer "+": Send… / Sidechain… / Move Output… submenus listing every legal target; drag-place/move retired; cables stay visual; right-click menus stay; Master "+" stays Analyzer | Small windows can't scroll mid-cable-drag |
| L13 | Add menu (titled bar entry): Add Aux Strip, Add Vox Bus, Add Inst Bus, Add Layers Bus, Add Bass Bus, Add Clips Bus, Add Plugins Bus; Vox/Inst strip rows dropped | Strips already have an add path via "+" tabs; group-mapping buses new |
| L14 | Added-bus lifecycle: a new bus persists until it has had ≥1 route and then loses all routes → hides; per-bus has-ever-had-route flag persisted in project | Doesn't vanish on arrival, doesn't sit forever after use |
| L15 | VibePlayer knobs: literal 1/3 — 55px → ~18px | Jeff's call, FL-scale knobs, values show in popups |
| L16 | Window-state persistence: three-lifetime model in-batch, incl. persistKeyFor fix + stable per-instance keys; **crash survival = timer flush into project autosave** | Closes the open ruling from 2026-07-28 |
| L17 | Hosted-plugin stretch: resizable plugins resize natively; fixed-size plugins get **free transform zoom**; silent clipping dies | Ruling 13=B |
| L18 | Caps: Layers 20, Bass 10, Drums 32, Clips 100, Vox 10, Inst 30 — in-batch | All fit existing 100-wide id blocks; PR-target shift accepted (existing projects' PR routing breaks once, pre-v1, no migration) |
| L19 | Drums 17–32 get a second drum-kit piano-roll entry; mechanics workshopped with Jeff before implementation | Kit grid was built around one kit of 16 |
| L20 | SYS coloring = per-token: SYS colors only itself; DSP load/overload drives the rest | Background apps stop yellowing an idle project's readout |
| L21 | BLU-110 three-zone limiter panel builds in this batch per Limiter.txt §1–2 | Jeff: "Build it" |
| L22 | Saturation on vocal chain: default Console; param range widened so Tape sticks | Root cause: `sat_type` range 0..1 clamps Tape=2 back to Console |
| L23 | Inst "no audio loaded" box removed | Dead chrome; nothing updates it on a live-input page |
| L24 | Transport perf readout → three rows (SYS/DSP | MEM/LAT | UND/PF) + gutter fix | The 160px label vs 120px reserve overlap is the real bug |
| L25 | Two-row ribbon tab labels: name top; badge + arrow bottom | Labels unreadable at one row |
| L26 | App name + logo back to title-bar center | Reverts TS7's left-align + icon drop |
| L27 | Piano Roll tab ordered next to Effects | Four default tabs together; order[] array only, fixed ids untouched |
| L28 | BaySickDrums + menu entry (> BaySickPlayer / BaySickSynth) absorbs the drum options from Player/Synth submenus | More intuitive drum grab; Synth auto-collapses to flat Layers |
| L29 | Piano-roll control lane user-resizable: current 240px = max, collapsed = min; DrumKit lane mirrors | Height rides the persistence stores |
| L30 | MIDI trigger velocity moves to Options > Audio Settings beside the MIDI inputs | Mixer hamburger is the wrong home |
| L31 | Hamburger buttons everywhere → text button "Menu" | Same dropdowns, discoverable label |
| L32 | No batch smoke; Work Log entry expected to HOLD to the G4 boundary per the mammoth/badger precedent (confirm at close) | Batch is inside G4 |

## Sub-spec calls surfaced for ExitPlanMode (genuinely deferred)

| ID | Call | When it resolves |
|----|------|------------------|
| D1 | Window-6 collapse scope (which pages, which block splits) | Re-docket after Task 7 floors land |
| D2 | Pedalboard one-pedal-at-a-time collapse shape | Same gate as D1 |
| D3 | Drum-kit second-16 PR entry mechanics (target-list appearance, kit→pages mapping) | Workshop with Jeff at Task 11 open, before code |
| D4 | Ribbon dropdown "EQ" entry disposition (J-6 deferred cleanup collides with Window-7) | Enumerated during Task 4, posed to Jeff |
| D5 | Per-window floor numbers | Jeff's sizing pass output (Task 6→7 boundary) |
| D6 | Whether Guitars/Basses Inst tabs carry Pedals/NAM-IR windows | Enumerate source at Task 4; pose only if ambiguous |
| D7 | Window title layout at narrow widths (Vox/Inst strips carry tab slots + title + preset + 2 buttons) | Informed by D5 numbers |

## Files to modify (primary surfaces per task; line refs verified 2026-08-03)

- **T1:** `Source/Standalone/GlobalTransportBar.cpp` (:586-593, :795-822, :945), `Source/Standalone/StandaloneEditor.cpp` (:10391-10424), `Source/Standalone/RibbonTabBar.cpp`/`.h` (paint :1047-1219, widths :404-502, hitTest :504-523, order[] :80-86), `Source/Standalone/SharedUI.cpp` (:872-890 title bar)
- **T2:** `RibbonTabBar.cpp` (:566-676 add menu, :836-877 dropdown adds), `Source/Standalone/LayersPage.cpp/.h`, `BassPage.cpp/.h`, `Source/Clips/ClipsPage.cpp/.h`, `Source/Standalone/DrumPage.cpp`, `SharedUI.cpp` (:1321, :1800, :1809), `Source/Standalone/MixerPage.cpp` (strip label), page-menu builder sites in `StandaloneEditor.cpp` (:5710-:6269)
- **T3:** `Source/Standalone/WorkspaceWindow.cpp/.h` (:25-48, :225-236), `SharedUI.cpp/.h` (ChromeCloseButton :851-869, PageMenuBar :1796-1862), `Source/Standalone/BaySickTitleBar.*` + the five engine editors hosting `BaySickPresetButton`, `Source/Standalone/BaySickRustyDrumsPage.cpp` (:208, :332-336, :548-554), `StandaloneEditor.cpp` (:6095-6097)
- **T4:** `Source/Vox/VoxPage.cpp/.h`, `Source/BaySickVocal/BaySickVocalEditor.cpp/.h`, `BaySickPitchEditor.cpp` (:2435, :2450), `BaySickPitchSubEditor.cpp` (:437), `Source/Inst/InstPage.cpp/.h`, `Source/BaySickPedals/BaySickPedalsEditor.*`, `Source/BaySickVocal/BaySickNAMIREditor.*`, `RibbonTabBar.cpp` (:795-810), `StandaloneEditor.cpp` (window hosting :13021-13205), `Source/BaySickVocal/BaySickVocalProcessor.cpp` (:132), `Source/DSP/SaturationDSP.h` (:173), `Source/Standalone/SlotComponent.cpp` (mode chain :992-1117)
- **T5:** `WorkspaceWindow.cpp/.h` (Persistence), `StandaloneEditor.cpp` (persistKeyFor, load path `hostPageInWindow` sites), `Source/Standalone/StandaloneApp.cpp` (settings write timing), project XML save/restore walker
- **T6:** `WorkspaceWindow.cpp` (readout + diag hook), `Plans & Specs/Test Plans/v1-master-test-plan.md` (§B.31.0)
- **T7:** floors table in `StandaloneEditor.cpp` (:13033-13036, aux call sites :13158/:13183/:13205), `Source/Harmless/HarmlessEditor.*`, `Source/VibePlayer/VibePlayerEditor.cpp` (:6-15, :426-433), `Source/Standalone/EffectEditorPanels.cpp` (pedal-mode branches, :4888-4915 pattern)
- **T8:** per D1/D2 ruling
- **T9:** `Source/Standalone/PianoRoll.h` (:494-495, :686, :768) `/PianoRoll.cpp` (:4071-4117, :3923-3929), `Source/Standalone/DrumKitGrid.h` (:376, :475-529)
- **T10:** `StandaloneEditor.cpp` (:6357-6519 mixer menu, AudioSettingsDialog :86-539), `Source/Standalone/MixerPage.cpp/.h` (add buttons :1662-1696, CableOverlay :502-657, :890-910), `Source/Standalone/MixerTrackStrip.cpp` (:241-256), `Source/VibeGraph.h/.cpp` (bus ids, prefix/friendly tables :87-176, registration), `Source/PluginProcessor.cpp` (task registration), `SharedUI.*` (PageMenuBar second titled menu)
- **T11:** `Source/VibeGraph.h` (:60-74 + literal rows :105-113, :170-176), `Source/VibesynthConstants.h` (:17-18 + page caps), repo-wide literal sweep, piano-roll target derivation + kit grid per D3
- **T12:** `Source/Hosting/HostedPlugin.*`, `HostedPluginEditor`, `WorkspaceWindow.cpp` (resize→content push), bridged `mRemoteHost` path
- **T13:** `Source/Standalone/EffectEditorPanels.*` (LimiterPanel), `Files For Claude/DSP Review/Limiter.txt` (read-only spec); consumes T17's component
- **T14:** overlay sweep sites (grep-driven), `CLAUDE.md`, Rule 4 catalog strips
- **T17:** new shared visual component under `Source/Standalone/`, `EffectEditorPanels.*` (PanelContext + view plumbing), `EffectWindows.cpp/.h` (View menu + per-view sizing, `onFloorChanged`), `SharedUI.*` (`setViewMenu` reuse from T8), the effect DSP classes that publish capture data + their gating atomic
- **T18:** `Source/Standalone/EffectEditorPanels.*` (nine panels), the matching DSP modules for whatever each visual reads
- **T19:** `Source/Standalone/WorkspaceWindow.cpp/.h` (`clampToWorkspace` split, drag path, open/restore placement), `EffectWindows.cpp` (`onFloorChanged` → set size, not just floor)

## Tasks

Ordering honors L7: T1–T5 land before the T6 handoff; T9/T10/T11 may run while Jeff's sizing pass is in flight; T7/T8 need his numbers. T15 (ruled mid-sizing 2026-08-03) executes between T9 and T10, deliberately before T7 encodes floors; remaining order after the sizing hand-back: T15 → T10 → T11 → T7 → T8 → T12+. One commit per task; message + full `git status` surfaced for approval each time; build gate = five exit codes 0, four link lines, zero error greps. `/draft-doc running-notes` after every commit/finding/ruling.

### Task 1 — Transport readout + ribbon visuals + app title

- [ ] Perf readout to three rows: `SYS/DSP`, `MEM/LAT`, `UND/PF` (string build at GlobalTransportBar.cpp:795-804; 34px label holds three 9pt rows — no bar-height change).
- [ ] Gutter fix: make the ribbon's right reserve and the label rect agree (kCPUReserve 120 vs removeFromRight(160)) so the readout never overlaps the "+" slot.
- [ ] L20 per-token coloring: SYS token colors off whole-machine CPU alone; DSP/overload drives DSP + the rest (current whole-label tint at :806-822 replaced with per-segment draw).
- [ ] L25 two-row tab labels: name top row; badge + arrow bottom row. Drop `+kArrowW`/badge terms from `naturalSingleLineWidth`; retire the camelCase wrap machinery it existed for; arrow hit zone = bottom-row right (hitTestSlot). Fix the stale "index 6" comment at :137-142 while in-region (Rule 6).
- [ ] L26 app title: `VibeLAF::drawDocumentWindowTitleBar` centers the text and draws the icon (stock-JUCE placement); title string source unchanged.
- [ ] L27 Piano Roll next to Effects: reorder `order[]` (:80-86) ONLY — the ctor `addFixed` ids are persisted and stay.
- [ ] Build gate → surface commit + status → commit on approval → running notes.

### Task 2 — "+" menu, naming sweep, engine-picker retirement, "Menu" buttons

- [ ] Reorder `showAddMenu` to the locked list: BaySickVocal · BaySickLiveInst · BaySickGuitars · BaySickBasses · VSTPlugin (submenu) · Harmless > Layers/Bass · BaySickSynth (flat — Layers only once Drums leaves) · BaySickPlayer > Layers/Bass/Audio Clips · BaySickBass · **BaySickDrums > BaySickPlayer/BaySickSynth** (new; absorbs the Drums targets from rows 2–3) · BaySickRustyDrums.
- [ ] L2 rename sweep: + menu string, ribbon tab label, mixer strip label (reconcile "Instrument Input" to LiveInst), page/PR titles spelling "Inst". `TabType::Inst` enum untouched (persisted integers).
- [ ] L4 removals: Layers/Bass engine combos + `LockableCombo` scaffolding; Clips decorative combo; Drums "Pick a sound v" button. Each page's engine right-click menu (Lock / Polyphony / Rename / Duplicate / Choke / Save Patch / Load Preset / Delete) merges into that page's Menu dropdown — dedupe against `showPageActionsMenu` entries; nothing dropped silently.
- [ ] L31 hamburger → "Menu": ctor string, width 22→fit, title x-offset; sweep every other hamburger-style button (effect windows etc.) to the same label.
- [ ] Close the layout gap where engine rows were deleted (full re-layout waits for T7 floors).
- [ ] Build gate → commit on approval → running notes.

### Task 3 — Window title strips: toggle, titles, presets

- [ ] L5 full-screen toggle on every WorkspaceWindow: second chrome button (ChromeCloseButton pattern), toggling last-user-bounds ↔ workspace fill (existing clamp primitives). Order right-to-left: close, full-screen, preset. Plan records the 5a reversal.
- [ ] L2/Window-4: engine editors' internal `BaySickTitleBar` dissolved; the colored player name renders centered on the window title strip (PageMenuBar gains a centered-title path; today title is suppressed when tab slots exist — resolve for D7 review).
- [ ] Window-3: `BaySickPresetButton` relocated to the title strip for VibePlayer, BaySickSynth, BaySickBass, Harmless, Pedals board; RustyDrums' bar-hosted Player Preset + Load Player combo move to its strip; editors reclaim the freed internal-bar height.
- [ ] L23: drop the `addExtraRightComponent` mount of the dead clip-file label for LiveInput Inst pages.
- [ ] Build gate → commit on approval → running notes.

### Task 4 — Window-7: sub-page windows + LiveInst restructure + vocal-chain fix

- [ ] Five sub-pages to own contained windows (satellite pattern: resolve target per tick, hold no raw pointers): BaySickPitchEditor, BaySickAlignEditor, VocalChainPanel, BaySickPedalsEditor, BaySickNAMIREditor. In-page tab views retired from BaySickVocalEditor and InstPage.
- [ ] Convert the two `findParentComponentOfClass` escapes (pitch "Send Notes to…" + sub-editor title) to injected callbacks BEFORE re-hosting; the Vox 4Hz re-analyze poller stays page/engine-side.
- [ ] Vox page window content = the BaySickVocals main panel; its title-strip buttons open the four satellite windows.
- [ ] L10 LiveInst: pedals window is the player — tab click fronts it; NAM/IR button on the pedalboard's title strip; no Inst page window for live-input tabs.
- [ ] L11 dropdown: "Pages:" section built from the active instance's real windows; LiveInst rows read "Pedals" / "NAM/IR". D4: enumerate the "EQ" entry's actual target and pose its disposition. D6: enumerate Guitars/Basses pedals/NAM-IR carriage from source; wire accordingly (pose only if ambiguous).
- [ ] New windows get persistence keys per T5's scheme (coordinate if T5 lands after — keys designed here, stores wired there).
- [ ] L22 saturation (same surface as the vocal-chain window work):

```cpp
// BaySickVocalProcessor.cpp:132 — was addI("sat_type", ..., 0, 1, 0);  0..1 clamps Tape=2
addI ("sat_type", "Saturation Type", 0, 2, 1);   // 0=Tube 1=Console 2=Tape; default Console
```
  plus the DSP member default (`SaturationDSP.h:173` Tube → Console) covering the pre-first-push window.
- [ ] Build gate → commit on approval → running notes.

### Task 5 — Window-state persistence: three lifetimes

- [ ] Lifetime 1 universal: in-memory persist-key → bounds map (`Persistence::Session` exists — becomes the single live store; every move/resize/close writes it, every open reads it).
- [ ] persistKeyFor defect: key on type + stable per-instance index (`pageIndexHint` exists but is unread and unfilled for Clip/Vox/Inst — fill it; fix the wrong comment).
- [ ] settings.xml written ON EXIT from a filtered view (sizes for all window types; placement for default tabs only) — kills the parse-and-rewrite-whole-file-per-close smell.
- [ ] Project file: full map + per-window open/closed on save; replaces the map on load; the load path stops force-framing every page (today three `hostPageInWindow` sites open everything).
- [ ] L16 crash survival: timer-flush the map into the project autosave.
- [ ] Build gate → commit on approval → running notes.

### Task 6 — Window-5 diag → HANDOFF to Jeff's sizing pass

- [ ] Title-strip WxH readout on every contained window (B.31.0's assumed readout — built here).
- [ ] Continuous diag: every resize appends `persist-key | title | WxH | effects panel mode (Basic/Advanced)` to `Documents/BaySickDAW/window-sizing-diag.txt` (existing folder convention). Rule 4 catalog row: tag `[QA-Layout DIAG]`, disposition **Remove at batch close**.
- [ ] Floors dropped to the absolute minimum (120x80) while the diag is active.
- [ ] L1: rewrite Test Plans §B.31.0 in place to describe this flow.
- [ ] Build gate → commit on approval → running notes.
- [ ] **HANDOFF: Jeff sizes every window (multiple takes; both effects modes), hands back the diag doc.** T9/T10/T11 may proceed while this is in flight; T7/T8 wait.

### Task 7 — Floors + layout reworks (post-data)

- [ ] Set real floors from the diag doc (replace the provisional 640x400 / 300x250 defaults + per-call-site aux floors; hosted-plugin windows keep plugin-derived floors).
- [ ] Subtractive-size-math sweep over every panel that was squeezed below design size during the pass — the JUCE 8 negative-`fillRect` Debug-assert class (the pedal-EQ crash was this).
- [ ] Specific-2 Harmless full rework (respecting the Part A/B dual-bind + `rebindToPart` machinery — both parts always render; automation stays parameter-addressed).
- [ ] L15 VibePlayer knobs: `kKnobSz` 55 → 18 (routing arrows + detune selector follow their existing kKnobSz-relative math; label/cell geometry rebalanced).
- [ ] Specific-4 pedal tiles: give every pedal-capable panel missing a `PanelMode::Pedal` branch one (OctaveStylePanel/FurmanEQ 3x2/3x3 grids are the reference); knob overlap dies.
- [ ] Build gate → commit on approval → running notes.

### Task 8 — Window-6 + pedalboard collapse (post-data) — RULED 2026-08-05

D1/D2 re-docket answered by Jeff. The general page-collapse half is **NOT built for
V1**; the pedalboard half **is**, in a different shape than the plan assumed.

- [ ] ~~Which pages collapse into tabbed blocks below floor + block splits~~ — **DROPPED to Future State `CL-306`.** A generic threshold is bounded work, but a good compact view still needs a per-player decision about which sections group and in what order, so it becomes a bespoke second design per engine; the T16 Harmless re-layout is the cost evidence. Open question carried into CL-306: whether the real driver is "two players on screen at once", which window tabbing or the fill/restore toggle may answer more cheaply.
- [ ] **Content minimums RESTORED at the measured sizes** (they were suspended pending this ruling). `setDefaultWindowSize` sets the opening size AND the constrainer minimum; a remembered size below the new floor is raised. These are the smallest-still-readable measurements, so a window refusing to go below its own is correct behaviour rather than a limitation.
- [ ] **Pedalboard one-pedal-at-a-time SHIPS** as an explicit Compact view, not an automatic below-threshold collapse: a `View` heading on the pedals window strip (between `Menu` and the `NAM/IR` button) offering Standard / Compact. Standard is the 4x2 board at 1534x455; Compact is a dropdown of the eight slots plus one display box, in a 357x268 window — the Effects window's footprint. The dropdown labels each slot by its current effect and re-labels on a type change. Rationale: a pedal chain is a row of discrete units and paginates without loss, unlike a synth panel meant to be read at a glance.
- [ ] **View-swap mechanics built for reuse per Jeff's instruction** — `PageMenuBar::setViewMenu` (generic heading + two closures), editor-owned `ViewMode` + `windowSizeFor`, host-owned window resize. Notated as Future State `CL-307` so the next window adopting it does layout work only.
- [ ] Build gate → commit on approval → running notes.

### Task 9 — Piano-roll control lane resize

- [ ] L29: lane height user-draggable via the existing 16px header strip; max = current 240, min = collapsed; grid keeps its 120px floor; `DrumKitControlLane` mirrors in lockstep; height persisted via T5's stores alongside the lane-visible flag.
- [ ] Build gate → commit on approval → running notes.

### Task 10 — Mixer: menu moves, target dropdowns, Add menu + group buses

- [ ] L30: "MIDI trigger velocity" out of the mixer Menu into `AudioSettingsDialog` beside the MIDI inputs (dialog height math adjusts; persistence unchanged).
- [ ] L12: per-strip "+" menu → Send… / Sidechain… / Move Output… submenus enumerating every legal target from the graph (`mActiveChannels` + `friendlyName`, filtered by `isValidBusSendTarget` / `isRouteAllowed` / `wouldCreateCycle`); picking writes the same params the drag paths wrote. Placement/move drag paths retired; cable painting + right-click menus (delete, pre/post) stay; Master untouched.
- [ ] L13: PageMenuBar extended with a second titled menu entry; "Add" menu = Aux Strip, Vox Bus, Inst Bus, Layers Bus, Bass Bus, Clips Bus, Plugins Bus; the five title-strip buttons removed. "Add" renders exactly like the corrected "Menu" entry (Jeff, 2026-08-03): a flat native-menu-bar-style text heading (`TitleStripMenuItem`), NOT a chrome button — the strip reads "Menu  Add" like a native window's menu bar.
- [ ] Four new group buses on the kVoxBus2 pattern (next bus ids after 13): registration, prefix/friendly rows, lazy strip UI, `_sendTo` targets, persistence, automation lanes registered model-side with the offline branch in the same pass (EngineRig rule: any new lane class needs live + `applyOfflineLaneValue` together). Cross-check `reference_mixer_strip_pattern_audit.md` before the diff lands.
- [ ] L14 lifecycle: per-bus has-ever-had-route flag (project-persisted); hide only after first-use-then-empty.
- [ ] Build gate → commit on approval → running notes.

### Task 11 — Instance caps

- [ ] D3 workshop FIRST (second drum-kit PR entry mechanics) — no code before Jeff's ruling.
- [ ] L18 constants: Layers 20, Bass 10, Drums 32, Clips 100, Vox 10, Inst 30 — each with its mirror (`kMax*Strips` + `kMax*Pages`).
- [ ] Literal sweep: the prefix/friendly tables hardcode `+16`/`+50` for Layers/Bass/Drums/Audio (VibeGraph.h:105-108 etc.); repo-wide grep for stale literals in range checks.
- [ ] Second drum-kit PR entry per D3.
- [ ] PR-target shift lands (accepted): note in running notes that pre-existing projects' piano-roll routing is invalidated once.
- [ ] Build gate → commit on approval → running notes.

### Task 12 — Hosted-plugin stretch (L17)

- [ ] Resizable plugins: window resize pushes through the plugin's own resize/constraint path.
- [ ] Fixed-size plugins: free transform scale of the hosted surface following the window (aspect preserved); no silent clipping; floor derives from a minimum usable scale.
- [ ] Bridged editors: establish what scaling means for the remote child peer; if the bridged case can't scale, it letterboxes and the narrowing is surfaced to Jeff, not shipped silently.
- [ ] Build gate → commit on approval → running notes.

### Task 13 — BLU-110 three-zone limiter panel (L21) — executes AFTER T17

- [ ] `EffectEditorPanels::LimiterPanel` rebuilt per `Limiter.txt` §1–2: Zone A scrolling waveform (input trace, GR curve from top, red ceiling line, right→left), Zone B Gain/Ceiling/Sat big knobs, Zone C Attack/Release/Ahead/Curve; skeuomorphic knob LAF; glass overlay; `#00FFF2` cyan GR / `#FF9100` orange sat; monospace readouts. Existing LUFS meter + target-line features carry over.
- [ ] **Zone A is built AS T17's reusable component, not standalone** (Jeff, 2026-08-05). This is the whole reason T17 comes first: the other nine panels in T18 consume the same component, so a bespoke Zone A here would be built twice and diverge.
- [ ] Panel behaves at rack size and in the pedal context (PanelContext) with no subtractive-math hazards; floors from T7 data.
- [ ] Build gate → commit on approval → running notes.

### Task 14 — Z-order audit + close bookkeeping

- [ ] App-wide drawn-overlay sweep: any drawn component parented to the editor and expected to cover the workspace gets its own desktop window (HeavyOperationOverlay pattern; the failure is silent — sweep by grep for `setAlwaysOnTop` + overlay-shaped components, not by waiting).
- [ ] CLAUDE.md stale facts corrected in the architecture notes touched this batch (ArrangementGrid kNumRows 32 → 500; anything else found en route).
- [ ] Rule 4: surface the diag strip list (the `[QA-Layout DIAG]` sites) for approval, then strip.
- [ ] Author this batch's §B section of the Master Test Plan at code-complete (bulk-run R2), reconciled against what actually shipped, including the new-surface scenarios (windows, buses, caps, stretch) — walked at the G4 boundary, not at close.
- [ ] Batch close sequence: `/draft-doc batch-close` → `/review-batch QA-Layout` (BLOCKERs before proceeding) → apply draft via Edit → close commit. L32: Work Log entry held to the G4 boundary — confirm with Jeff at close.

### Task 15 — Strip nav buttons into Menu dropdowns + sfizz titles (ruled mid-sizing 2026-08-03; executes between T9 and T10)

- [ ] Every player page's title-strip buttons sitting between the Menu dropdown and the swing knob stop being strip buttons and become entries at the top of that window's own Menu dropdown (tick on the active local view). Per page: Rusty + Drums {Drum Kit, Player, Piano Roll}; Layers/Bass/Clips {Player, Piano Roll}; Vox {Vocal Chain, BaySickPitch, BaySickAlign, NAM/IR}; Inst {Pedals, NAM/IR, Piano Roll}; Plugins {Piano Roll}. EXCLUDED (Jeff): the Piano Roll page's jump cluster, the pedals window's NAM/IR launcher, the EQ windows' Pre/Post pair.
- [ ] The missed T3 title treatment: BaySickRustyDrums, BaySickGuitars, BaySickBasses internal title bands dissolved; names centered on the title strip (Rusty red `#CC2222`, Inst navy `#1C3A8A`). The widgets the Inst band hosted (program label + Load button, CUT SELF pair) re-home to the strip's right extras; CUT SELF attachments wired independently of any title bar.
- [ ] Purpose: declutter + make the centered strip titles visible. Sequenced BEFORE T7 deliberately — it changes strip contents, which affects the min widths T7 encodes.
- [ ] Build gate → commit on approval → running notes.

### Task 16 — Sizing model rework + strip consolidation + Harmless re-layout (Jeff-directed mid-batch 2026-08-04; shipped `3cfdf4c2`)

- [ ] **Two QA-ModelShell regressions, both Jeff-found, neither surfaced by the shell work that caused them.** (a) Right-click Automate was dead on every player and mixer strip: `GlobalAutoRightClick` is installed once over the editor's own component tree, and a `WorkspaceWindow` is a separate native peer, so the listener never saw a click inside one. VKnob-based controls were unaffected (a VKnob listens to its own slider and tags it so the global handler skips it), which is why the effect panels and pedals kept working and the `VibeSlider` sections did not — VibeSlider swallows the right-click on purpose and depends entirely on that listener. Each window owns a listener now. (b) Every tooltip raised from the transport bar painted BEHIND the page windows: a parented `TooltipWindow` positions inside its parent and draws there, and a native child always covers that. The editor's tooltip is parentless (desktop) now, and the per-window + KeyBinds tooltips are gone — a parentless tooltip's peer gate always passes, so keeping them raised two tips at once.
- [ ] **Sizing model: DEFAULTS, not floors (Jeff's ruling).** `floorSizeFor` → `defaultSizeFor`, returning `std::optional` and answering NOTHING when the engine is unbound — the old fallback was 490x455, which is BaySickPlayer's REAL floor, so an unresolved window was indistinguishable from a legitimately small one and always erred toward too-small. Defaults install at engine-bind (`onEngineEditorRebuilt`, which also tracks a live Drums swap) PLUS a 5 Hz healing sweep, because that callback is a single slot and an engine that bound before `showPageForTab` installed it fired into nothing — intermittent, and likelier on a second player whose engine loads faster once warm. **Content minimums are SUSPENDED** until T8 decides them; only the 120x80 anti-degenerate clamp survives. NOTE for T8: the original "collapse below floor" framing assumed these numbers were floors, and they are opening sizes now — the D1/D2 ruling has to redefine the threshold.
- [ ] **settings.xml carries ONLY the four default tabs**, size AND position. It had defaulted every page window to `Persistence::Disk` and written every window's SIZE globally while gating only POSITION to the four — the exact inverse of T5's ruling, and what left 143 records feeding stale sizes to players on every cold start. Everything else is Session (lifetime 1) + project (lifetime 3); a cold start with no project opens at the default. Stale records stripped.
- [ ] **Resize magnetism**: `applyMagnetism` was only ever called from `mouseDrag`, so the magnet worked on move and did nothing on resize. `applyResizeMagnetism` snaps each edge independently and only edges that moved — translating the whole window is right for a drag, wrong for a resize.
- [ ] **Title strips**: FX Rack + Freeze become entries in the per-window Menu (the T15 sweep took the tab-slot cluster and left these, which was a scoping error); Freeze shows greyed-and-locked with the unlock path in its tooltip rather than hidden; swing knob to the far left beside Menu; a window with a logo shows no plain title, one without centres it; Pitch/Align/NAM-IR logos moved onto their window strips; Rusty's Aria band restored to host its Program + Player Preset (T15 dissolved it and left both homeless); Inst cut-self pair moved onto the player itself with the clip label + program button at 2/3.
- [ ] **Builder**: own `Edit/Tools/Clips/View` row deleted (Clips → window Menu, Edit + View → strip headings, Tools removed as a duplicate of the toolbar), grid reclaims the 20px; browser collapse is a magnetic ramp with the collapsed strip as the pull-back handle (the `<<` button and `View > Toggle Browser` were two click-paths to one action with no drag path); track-header corner blanked and rows clipped to the ruler; vertical zoom decoupled from window size — the Alt+scroll clamps were viewport-derived, so the same gesture bottomed out at ~12px rows full-screen and ~4px in a contained window.
- [ ] **Transport bar**: ribbon `+` sized to twice its glyph and carved off first (it was floored to 60-80px AND handed an equal share of every leftover pixel); `TransportPerfReadout::kWidth` 120 → 95 with the 25px to the tabs, and the tooltip carries LIVE values above the legend because truncation there is silent.
- [ ] **Harmless re-layout**: knobs halved (44/32 → 22/16), all four filter knobs one size, faders → knobs (Unison, LFO depths, and Routing — Routing is its own component and was missed first pass), one box per filter with its ADSR, sections sized to content with knobs distributed inside, horizontal strips replacing narrow columns, labels sized to their TEXT (knob-width boxes clipped everything past four characters at 16px), mod editor knobs matched at 16 and its tool row shrink-to-fit on ONE row. Snap + grid moved onto the app's unified divisions from `VibesynthConstants.h` **including triplets** — this was the one place in the app a triplet could not be snapped to — with segment counts derived from `snapDivToTicks` and the grid following the selected division via `gridLadderForSnap`. The tick system deliberately does NOT reach in: this axis is per-note 0-1 phase, not song position.
- [ ] Build gate → commit on approval → running notes.

### Task 17 — Effect-panel visual foundation (Jeff-directed mid-batch 2026-08-05)

Workshopped with Jeff 2026-08-05.  Rulings recorded here because they decide the
other three tasks: the space comes from HEIGHT (all three panel classes are 268
tall; width already varies); the visual is a **third view, not an Advanced-gated
feature** — a beginner cannot hear what a compressor does, so gating the picture
behind Advanced hands the explanation to the people who least need it; and Basic
is allowed to carry it (the replica rule was about not burying a learner under
ten unfamiliar knobs, not about literal fidelity).

- [ ] **Reusable visual-strip component.** One component the other panels
      configure, NOT a per-panel bespoke drawing job — the limiter's Zone A is
      its first consumer, not a one-off. Everything after T13 is then layout.
- [ ] **Visibility-gated audio feed.** Any audio-thread capture (samples,
      spectrum, GR history) sits behind ONE atomic that is true only while a
      visual is on screen — the `mAnyXActive` fast-path-bypass pattern. Without
      this, ten effects feed data into the void forever and no view or window
      state saves anything; this is the actual CPU lever, bigger than either
      geometry option considered.
- [ ] **One shared timer** driving every visible visual rather than N
      independent ones; peer-keyed start/stop like every other repeating cost
      in the shell.
- [ ] **Menu entry on EVERY effect panel** (Jeff, 2026-08-05): a title-bar Menu
      item that opens the Visual view, present on all of them and **greyed +
      unusable when that effect has no visual**, with the reason in its tooltip
      — the same show-it-disabled treatment T16 gave locked Freeze, and for the
      same reason: a user who closes or switches away has to be able to find it
      again, and an entry that vanishes teaches nothing.
- [ ] **"Visual" third view** through the T8 view-swap machinery (`setViewMenu`,
      editor-owned `ViewMode`, `windowSizeFor`, host-owned resize — CL-307
      exists precisely so this is layout-only). The view declares its OWN window
      height, which is what dissolves the fixed-panel-size problem. Switching
      away tears the visual down; the effect window stays open and the DSP never
      notices. Effect windows are their own windows with their own peer-keyed
      poll, so close here is a genuine teardown (unlike page windows, where the
      page object survives — see `canRebuildType`).
- [ ] Build gate → commit on approval → running notes.

### Task 18 — The remaining nine effect visuals (Jeff-directed mid-batch 2026-08-05)

Jeff's call: ALL ten, this batch. Built on T17's component, so each is layout +
its own drawing, not new plumbing. Ranked by whether it TEACHES (the audience has
never made music) rather than by how it looks.

- [ ] **Compressor** — GR history trace + transfer curve with the knee drawn and
      a live dot at the current operating point.
- [ ] **Chorus / Flanger / Phaser** — animated LFO scope (waveform + current
      phase) plus the comb-notch curve.
- [ ] **Transient Shaper** — before/after envelope ghosting on a waveform strip.
- [ ] **Saturation / Tape** — live harmonic bars + the shaper's transfer curve.
- [ ] **Delay** — repeats laid out against the beat grid, ping-pong shown L/R.
- [ ] **Reverb** — decay envelope with pre-delay / early reflections / tail as
      distinct regions.
- [ ] **Manual-facing explanations written AT the component that draws them**
      (Jeff, 2026-08-05: the manuals get researched from the whole corpus later,
      so there is no separate notes doc — the corpus has to carry it). Every
      visual whose meaning is not self-evident says what it is telling the user:
      harmonic bars above all (nobody who has never made music knows what a third
      harmonic is), plus what the GR trace means, why reverb pre-delay is its own
      region, and what the comb notches represent.
- [ ] Build gate → commit on approval → running notes.

### Task 19 — Window placement + Basic/Advanced swap sizing (Jeff-found 2026-08-05)

- [ ] **Windows must LAND on screen.** Open/restore clamps POSITION into the
      workspace. Verified cause of the second half: `clampToWorkspace` does two
      jobs — it fits the SIZE to the workspace and then nudges the position — and
      the drag path calls it every mouse move, so the first drag of an oversized
      window shrinks it to workspace width ("moving it instantly locks it into
      the width of the screen"). Not yet traced: why they open outside in the
      first place; `clampWindowsIntoView` runs only from the workspace's
      `resized()`, so a window framed or re-defaulted after that is never
      clamped — confirm before fixing.
- [ ] **Drag bound becomes the CURSOR, not the window** (Jeff's ruling, FL
      behaviour): a window may hang off any edge, the mouse may not leave the
      workspace. This retires the size clamp from the drag path, which is the
      destructive half. Supersedes the locked call 2b containment rule.
- [ ] **Basic/Advanced swap resizes to the new variant's default.** The swap
      fires `onFloorChanged` (1047x268 Advanced / 691x268 Basic) and the owner
      routes it to `setDefaultWindowSize`, which only ever GROWS to a floor — so
      Advanced→Basic stays stuck at 1047 wide and Basic→Advanced grows rightward
      with no re-clamp, landing half off screen. Set the size outright on a
      variant change, then re-clamp position into view.
- [ ] Build gate → commit on approval → running notes.

## Verification (end-to-end)

None at batch level — **this batch is part of G4**. All functional verification rides the G4 boundary smoke (`Files For Claude/G4 Boundary Smoke.txt` + Test Plans §B.31 with B.31.0 rewritten by T6 + this batch's §B section from T14). The smoke must not assume the untested bridged-specific `1cd1f5d6` items (no 32-bit VST3 on hand). Per-task build gates are the only in-batch gates.

**Correction (T16, 2026-08-04):** the recorded fact "magnetism composes correctly with resize (no re-test needed)" was WRONG and is withdrawn. `applyMagnetism` was reachable only from `mouseDrag`, so the magnet never ran on a resize at all — there was nothing to compose. T16 added `applyResizeMagnetism`. The walk must test snapping on resize as well as on move.

## Routing notes (Rule 3 during execution)

Findings that touch a not-yet-started task here → fold in (running-notes entry). Completed-batch surfaces → §9 Forks back-ref, fix rides this batch. Genuinely new areas → new §5 row, Jeff slots it. QA batches fix bugs found — deferral needs Jeff's explicit call. Spec calls discovered mid-execution surface in chat before landing in this file (Rule 5).

**Finding (2026-08-05) — `canRebuildType`'s stated reason does not hold for Rusty.**
Traced at Jeff's instruction while explaining why four page types close
frame-only. The comment gives one reason for all four ("construction is
entangled with spawning a mixer strip"); it is correct for three and unverified
for the fourth:

- **Vox / Inst** — confirmed. `addVoxChannelAtIndex` / `addInstChannelAtIndex`
  create the STRIP and fire `onVoxStripAdded` / `onInstStripAdded`, which spawn
  the page. The page is downstream of the strip, so there is no way to rebuild
  the page without re-entering at strip creation.
- **Clips** — confirmed, different mechanism. `createClipStripAndPage` builds the
  strip and the page as one unit, and the page's identity is a Builder row plus a
  sample file rather than anything the rig holds.
- **BaySickRustyDrums** — NOT confirmed. Its strips are created by
  `onKitLoaded` firing `addRustyChannelAtIndex` per engine channel — a kit-LOAD
  event, which is the same lazy pattern that makes Layers/Bass/Drums rebuildable.
  Its engine is processor-owned rather than rig-owned (the sfizz trio's
  race-safe load paths), which may be the actual reason it was excluded, but that
  is a guess and is recorded as one. If it turns out Rusty CAN be rebuilt, that
  is a memory dividend on one of the heaviest pages — worth a look, not worth
  assuming.

Contrast that makes the rule legible: Layers / Bass / Drums create the page
first and the strip arrives later from `onEngineSelected`, so a page rebuild
never re-runs strip creation.

## Carry-Forward Reference touch points

- T4/T5: §2 (lifecycle primitives, closeAllDynamicTabs, project-load barrier) before touching the load path.
- T10: §1 (dispatcher/task registration — most-recent-registration-wins trap) + §3 (spawn cascades, restore walker) before bus registration.
- T11: §3 restore walker; §8 anti-patterns.
- T12: §2 RetirementQueue/RCU patterns if editor lifetime work surfaces.
