# QA-EqPro — KBS EQ Pro take-back + upgrade — Plan (natural-notching-narwhal)

> **Canonical path:** `Plans & Specs/Batch Plans/natural-notching-narwhal.md`
> **For execution:** read Main Plan §0 (rules + agent orchestration), this file, and
> the paired `Plans & Specs/Running Notes/natural-notching-narwhal.md` at session
> open.  Build gate at EVERY task (Rule + `feedback_build_gate_every_task`).
> Bulk-run mode applies (per-task ear verify deferred to the end-of-batch smoke +
> Master Test Plan campaign); ALL mid-run spec calls still go to Jeff.

---

## Context

**Why this batch.** The KBS Plugins project rebuilt BaySickDAW's EQ8 as
`kbs::ParametricEq` — JUCE-free, 24-band, all-but-one of the 26 recorded EQ8
defects fixed, every behavior pinned by `Tests/test_core.cpp`.  Its own header
says it is "the reference implementation the DAW takes back when it is done."
Jeff's instruction (2026-08-26): upgrade our EQ to everything the plugin has —
full feature scope, no exceptions.  The workshop (this session, 2026-08-26)
locked all architecture calls; they are in the table below.

**Effort estimate.** The biggest single batch since QA-ModelShell.  Ten tasks:
engine vendor + two engine extensions, a test target, a wrapper + graph swap, a
parameter-scheme rework with lazy registration, a two-task display port
(~3,700 lines adapted), a features task, threading + cleanup fixes, the export
trim, docs.  Every strip in the app is touched at the parameter level.

**Risk.**
- Parameter migration breadth: every consumer of the `..._eq{b}{Suffix}` id
  scheme moves (audio-thread cache, automation sweep, FxRackPresetIO, page
  presets, reset sweeps, save/load).
- The per-domain linear-phase extension is new DSP (2x2 matrix convolution) —
  mitigated by tests written in the same task.
- Old projects' EQ settings do NOT carry (id scheme changes; pre-v1, no
  migration shims per `feedback_no_backward_compat_pre_v1`).  Jeff ruled 1A
  knowing this.

**Dependencies.** None external.  fontaudio gets vendored from
`KBS Plugins/libs/fontaudio` (license verified at vendor time, expected OFL).

**Reference material (read before the task that uses it):**
- `Files For Claude/EQ Build Notes.md` — the KBS build ledger (esp. test passes
  1-6: keybinds, dynamics semantics, presets, glyph traps).
- `Files For Claude/EQ Fixes.md` — the 26-defect ledger (what the engine fixes).
- `Files For Claude/EQ Future Updates.md` — the upgrade list.
- KBS source root: `C:\Users\jeffm\KnowledgeBase Studios\Plugin Production\KBS Plugins\`
- Workshop reports (this session): scratchpad `eq-engine.md`, `eq-product.md`,
  `eq-ui.md`, `eq-surface.md`, `eq-threading.md`, `eq-latency.md` — file:line
  cites for every claim below.  These are session-scratchpad files; the facts
  they carry are restated here where load-bearing.

---

## Spec calls already locked

| ID | Decision | Reasoning |
|----|----------|-----------|
| SC-1 | **Single 24-band set per EQ bank** (pre and post each), replacing the mid+side dual-engine pairs (Jeff: 1A). | One band pool, per-band domain; halves engines per strip (2 not 4) and halves linear-mode latency (mid+side ran in series). |
| SC-2 | **24 bands, per-band lazy registration** — bands 1-8 register with the strip, 9-24 on first activation (Jeff: 2a). | Registered != running; lazy grain refined so unused bands cost nothing anywhere. |
| SC-3 | **Per-domain linear phase** — fix C3 properly in the engine (Jeff: 3A): the linear path honors per-band domain via a 2x2 matrix convolver. | The KBS engine did NOT fix C3 (verified: `rebuildLinearCurve` ignores channel; one FIR both channels).  Build-notes claim gets corrected. |
| SC-4 | **Sidechain: extend the engine** to the DAW's per-band 4-slot pick (Jeff: 4A); tests extended with it. | Engine currently has one SC feed + per-band bool. |
| SC-5 | **KBS window layout ported, modified** (Jeff: 5A-mod): no brand block; presets in the page hamburger menu; A/B next to the "+" chip; **three views Stereo/Mid/Side (Jeff: 1b)** — view IS the domain, no routing gesture; L/R offered only on Stereo-view bands via right-click (Mid/Side removed from picker); 24 bands shared across views; **ghost**: other views' curves + dots drawn dimmed, live, non-interactive; **our dark look kept** (Jeff, 2026-08-26 addendum): the current EQ's dark background, NOT the KBS skeuomorphic brushed-plate/recessed surfaces — and the look is one-way, KBS keeps its own. | Mid/side two-picture editing kept (plugin lacks it — recorded as KBS fix-back). "Mono" is the mid by definition; no separate option. L/R inside M/S views is mathematically meaningless — not offered. |
| SC-6 | **Full feature scope** (Jeff: "ALL OF IT"): spectrum grab, EQ Match (current/reference/file), collision view, piano strip, auto-gain + amount, spectrogram heatmap, 12 factory presets + Default, listen/isolate, placement on Stereo-view bands, output trim + polarity. | — |
| SC-7 | **Mode set = the plugin's** (Jeff: 7a): Zero Latency / Natural Phase / Linear Low / Medium / High / Very High / Maximum. Old 5-mode set (Standard/Linear/HQ+/HQ Linear/HQ Extended) retired. | Precision folds into the mode menu, as Jeff approved for the plugin. |
| SC-8 | **B4 threading fixes in-batch** (Jeff: 8A): shield the EQ options-menu setters and the page-preset import path (incl. clipboard paste). | Two confirmed use-after-free paths; house shield+settle idiom exists everywhere else. |
| SC-9 | **Registration grain split** (Jeff, eager-buses workshop): bus ROUTING params stay eager (BLU-447 stays dead); bus EQ params go lazy like insert strips. | Jeff never asked for eager EQ registration; measured cost ~600 KB of blank entries in every project file (Display Project: 9,792 entries / 3.28 MB). |
| SC-10 | **Export leading-latency trim in-batch** (Jeff, adjacent-1). | Never fixed in any prior export work (verified); linear phase compounds the offset up to ~a quarter second at Maximum precision. |
| SC-11 | **Legacy audio-row duplicate EQ node removed** (Jeff, adjacent-2). | Never actually removed; three live creation call sites today. |
| SC-12 | **Dead display bind modes deleted** (Jeff, adjacent-3). | `BindMode::APVTS` (dead id scheme) + `BindMode::DSP`: zero callers; E4 lives there. Dies with the display rebuild. |
| SC-13 | **KBS EQ test sections + runner become a DAW test target** (Jeff, adjacent-4). | Post-batch follow-up logged: sweep the codebase for other test-pinning opportunities. |
| SC-14 | Old projects' EQ settings reset on load under the new scheme. | Pre-v1 standing rule; Jeff ruled with this stated. |
| SC-15 | **Mode/oversampling params excluded from automation** (Jeff: 1a): registered for state+undo, no Automate entry (precedent: `part_sel` deliberately unstamped), applied through the shielded message-thread path. | The setters reallocate; house shield+settle convention governs, not the plugin's audio-thread config-action stance. |
| SC-16 | **A/B stays DSP-side** (Jeff: 2A): today's spare-bank mechanics kept — A/B state serializes with each EQ point; button moves next to the "+" chip. | — |
| SC-17 | **User EQ presets in `Documents\BaySickDAW\Presets\EQ\`** (Jeff: 3a). | Beside the existing preset folders. |
| SC-18 | **Band view-move = right-click > "Move to Mid / Side / Stereo view"** (Jeff: 4a). | Move re-domains the band in place (settings kept). |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open — all four resolved by Jeff 2026-08-26 (SC-15..SC-18).

---

## Files to modify

**Task 1 (engine vendor + extensions)** — NEW `Source/DSP/Kbs/`: ParametricEq.h,
EqLinearPhase.h, EqMatch.h, Devices.h, SVF.h, Oversampler.h, FFT.h,
MacroParameter.h, TargetNames.h (from KBS `Source/Core/`, `kbs` namespace kept).
Extensions land in the vendored copies.

**Task 2 (tests)** — NEW `Tools/EqTests/main.cpp` (+ CMake target `BaySickEqTests`),
ported from KBS `Tests/test_core.cpp` sections 10-11 (test_core.cpp:978-1603).

**Task 3 (wrapper + graph)** — NEW `Source/DSP/StripEq.h/.cpp`;
`Source/BaySickGraph.h/.cpp` (InstrChannelNode :589-598, InsertNode :285-307,
chainLat :1600-1605, restoreEqs :2065-2076, saveRackStates :1901-2011,
pushScArrayToStrip :3156-3208); `Source/DSP/DSPBase.h` (SC buffer surface).
DELETE at task end of Task 6: `Source/DSP/EQ8DSP.h/.cpp`,
`Source/DSP/EQ8MsDSP.h/.cpp`, `Source/DSP/EqLinearPhaseProcessor.h/.cpp`.

**Task 4 (params)** — `Source/PluginProcessor.h` (:2044-2087 cache, :2059
kEqBands, :2067-2073 slot counts) / `Source/PluginProcessor.cpp`
(addParamsForEQBank :7595-7647, EqBandIds :7685-7702, ensureMixerStripParams
:8851-8861, ensureMixerBusAndMasterParams :8874-8911, updateEQFromCache
:5561-5619, resetEqStatesToDefaults :5679-5724, kEqBuses :5507-5526,
kEqInsertFamilies :5530-5541); `Source/Standalone/FxRackPresetIO.h`;
`Source/Standalone/StandaloneEditor.cpp` (formatMixerSuffix :4638-4662).

**Tasks 5-7 (display + features)** — NEW `Source/Standalone/EqWindowUI/`
(graph, rail, analyser, match panel, adapted from KBS `Source/UI/` +
`Source/Products/EqProEditor.h`); `Source/Standalone/EffectWindows.h/.cpp`
(EffectEqWindow :136-193, bindToChannel :577-593); `Source/Standalone/SharedUI.h/.cpp`
(DELETE ParametricEQDisplay ~1307-1658 h / large cpp span); NEW vendored
`libs/fontaudio/`; `CMakeLists.txt`.

**Task 8 (threading + cleanup)** — `Source/Standalone/SharedUI.cpp` (menu
setter sites — post-rebuild locations), `Source/PagePresetIO.cpp` (:451-488,
:726-735), `Source/Standalone/StandaloneEditor.cpp` (:2720 clipboard paste,
:7676-7684 Rusty preset menu, addAudioRowChannel callers :10686, :19759,
:20796), `Source/BaySickGraph.cpp` (addAudioRowChannel :2214-2223,
getAudioRowEQ :2235-2246, legacy `<InstrCh>` save :1950-1968).

**Task 9 (export trim)** — `Source/Standalone/BuilderPage.cpp` (runOfflineLoop
:9278+, consumeBlock area :9763-9806, Tail handling :9370-9371, :9471).

**Task 10 (docs)** — `Manuals/` (EQ pages + regenerate + PDFs), `Plans & Specs/
System Reference/` EQ docs, `Plans & Specs/Test Plans/v1-master-test-plan.md`
(new §B.38), `Files For Claude/EQ Build Notes.md` (fix-back entries),
`Plans & Specs/Future State.md` draft text (test-pinning sweep) in Routing notes.

---

## Tasks

### Task 1 — Vendor the engine, extend it (per-domain linear + 4-slot sidechain)

- [ ] Copy the nine files from KBS `Source/Core/` into `Source/DSP/Kbs/`
      verbatim (keep `kbs` namespace and headers-only shape); add to CMake
      include path.  Verify fontaudio is NOT needed here (it is UI-only, Task 5).
- [ ] Strip nothing on the way in; the DAW-facing surface is the wrapper's job
      (Task 3).  `MacroParameter.h`/`TargetNames.h` ride along only because
      `Devices.h` includes them.
- [ ] **Extension A — per-domain linear phase (SC-3).**  Generalize
      `EqLinearPhase` to a 2x2 matrix convolver and build its four spectra from
      the per-domain curve products.  Per-bin arithmetic (all zero-phase real
      magnitudes; M/S encode is `M=(L+R)/2, S=(L-R)/2`):

      ```cpp
      // Per bin: Hst = product of stereo-domain band curves,
      //          Hl/Hr = left/right-domain, Hm/Hs = mid/side-domain.
      // L/R-domain diagonal first, then the M/S-domain stage folded back:
      //   HLL = ((Hm + Hs) / 2) * Hst * Hl;   HLR = ((Hm - Hs) / 2) * Hst * Hr;
      //   HRL = ((Hm - Hs) / 2) * Hst * Hl;   HRR = ((Hm + Hs) / 2) * Hst * Hr;
      // Degenerates to today's two independent convolutions when no band is
      // routed mid/side (HLR = HRL = 0) - keep that fast path.
      ```
      One latency stage, unchanged figure.  Four FIRs designed with the same
      Kaiser recipe; cross-term convolutions skipped when identically zero.
- [ ] **Extension B — 4-slot sidechain (SC-4).**  `EqBandParams` gains
      `int scSource = -1` (slot 0..3; -1 = internal detection).  Engine gains
      `setSidechainSlot (int slot, const float* l, const float* r, int n)`
      (4 copied blocks, same recipe as today's `setSidechain`); the dynamics
      detector picks the band's slot.  Keep `scExternal` mapping to slot 0 so
      the KBS plugin side stays source-compatible.
- [ ] Rule 6 pass over touched regions only.
- [ ] Build gate (`cmd.exe /c "C:\Users\jeffm\Documents\BaySickDAW\do_build.bat"`,
      background, judge by build_log.txt six exit codes + four link lines).
- [ ] Checkpoint commit (one-liner, Rule 9) + running-notes entry.

### Task 2 — The proof target

- [ ] Port the parametric EQ / linear phase / dynamics / match sections of KBS
      `Tests/test_core.cpp` (:978-1603) + the `check`/`near`/`gainAt` harness
      into `Tools/EqTests/main.cpp`; new CMake console target `BaySickEqTests`
      (built by do_build.bat? NO — keep it out of the gate script; built and
      run on demand in this batch, wiring into any CI is out of scope).
- [ ] NEW tests for the extensions:
  - [ ] Per-domain linear: a Side-routed +6 dB band in a linear mode moves the
        side signal and leaves a mono (mid-only) signal untouched within
        0.1 dB; same for Mid vs a pure-side signal; L/R likewise; flat still
        bit-exact; impulse latency still exact-to-the-sample with mixed
        domains active.
  - [ ] 4-slot sidechain: band detecting slot 2 ducks when slot 2 carries the
        loud signal and ignores slots 0/1/3.
  - [ ] The C3 regression: the exact broken case (side band, linear mode,
        mono probe) asserted fixed.
- [ ] Run `BaySickEqTests` — all sections green (I run this; it is a console
      tool, not the app smoke).
- [ ] Build gate + checkpoint commit + running notes.

### Task 3 — StripEq wrapper + graph swap

- [ ] NEW `StripEq` (Source/DSP/StripEq.h/.cpp): owns ONE `kbs::ParametricEq`,
      the pre/post spectrum feeds (host-rate taps, feed poll buffers sized to
      the RING per the KBS lesson), A/B spare bank (SC-16: DSP-side, serializes with the EQ),
      identity fast path (skip `process` entirely when the engine is
      identity — engine's fresh state is bit-exact pass-through, keep a cheap
      wrapper-side check), state blob (new schema: bands + globals + spare +
      view prefs), `getLatencySamples()` = engine figure (exact), SC delivery
      (forward the strip's 4 receive buffers via `setSidechainSlot`).
- [ ] Swap `EQ8MsDSP preEq/eq` -> `StripEq preEq/eq` in `InstrChannelNode` and
      `InsertNode`; update `chainLat`, `restoreEqs`, `saveRackStates`,
      `pushScArrayToStrip`, getters.  NOTE: single engine per bank — reported
      linear latency HALVES vs the old mid+side serial pair; `updateBusLatencies`
      consumers unchanged in shape.
- [ ] Keep `EQ8DSP`/`EQ8MsDSP` compiling beside StripEq until Task 6 deletes
      them (the old display still binds to them until the new UI lands) — the
      graph nodes carry StripEq from THIS task; the old EQ window is
      non-functional between Tasks 3 and 5 (acceptable inside one batch, noted
      for the smoke).
- [ ] Build gate + checkpoint commit + running notes.

### Task 4 — Parameter scheme + lazy registration

- [ ] New id scheme (one set per bank): `{stripPrefix}_{bankSub}eq_b{N}_{suffix}`,
      bankSub `""`/`pre_`, N = 0..23, 18 suffixes (on/type/freq/gain/q/slope/
      chan/place/mute/iso/dyn/thr/ratio/atk/rel/relauto/range/scsrc) + per-bank
      globals (mode/os/propq/autogain/agamt/outgain/polarity — SC-15: state +
      undo only, EXCLUDED from automation, applied via the shielded path).  Type ordinals = the kbs enum (no OFF type; `on` bool);
      slope = the 9-entry dB/oct table; chan = Stereo/Left/Right only
      (SC-5/1b: the domain rides a separate per-band `view` field in the blob,
      NOT a param — a band's M/S domain is structural, set by creation view).

      ```text
      Wait — decision recorded plainly: domain IS per-band engine channel
      (stereo/mid/side/left/right) under the hood.  The PARAM `chan` exposes
      only Stereo/Left/Right (the Stereo-view picker).  Mid/Side domains are
      written by the VIEW at band creation/move, land in the same engine
      field, serialize with the band, but are not offered on the param picker.
      One field, two writers, picker restricted - not two fields.
      ```
- [ ] Per-band lazy registration: bands 0-7 register at strip-param creation;
      8-23 register on first activation (view "+" or preset/state load carrying
      an active high band).  Audio-thread cache becomes nullable per band
      (sweep skips unregistered bands); `kEqBands` -> 24 with lazy slots.
- [ ] SC-9 split: `ensureMixerStripParams` separates routing-core (eager for
      the 18 buses, unchanged guarantee) from the EQ block (lazy everywhere,
      triggered by: EQ window open, EQ state restore with content, band
      activation, FxRackPresetIO/page-preset load carrying EQ params).
- [ ] Update every consumer: `updateEQFromCache` (new cache + engine setters
      via StripEq), automation sweep labels (`formatMixerSuffix` — new
      suffixes, no more Mid/Side B{n} naming; views named instead),
      `resetEqStatesToDefaults` (new shape), FxRackPresetIO prefix rewrite,
      undo gesture ids in the new UI (Task 5).
- [ ] Old-project note lands in running notes (SC-14: EQ settings reset).
- [ ] Build gate + checkpoint commit + running notes.

### Task 5 — Display port I: graph + analyser + views

- [ ] Vendor fontaudio (`libs/fontaudio` from KBS) — verify license file on
      disk (expected OFL) and record it for /audit-licenses.
- [ ] Port `EqAnalyser` (8192-pt, N/4 normalization, tilt 0/3/4.5 default 4.5,
      three speeds, freeze, peak hold, arm-hold, findPeakNear, spectrogram
      column writer + 5-stop heat palette) — poll buffer sized to the FEED
      RING (KBS lesson, EqAnalyser.h:36-39).
- [ ] Port `EqGraph` into `Source/Standalone/EqWindowUI/`: engine-query-only
      drawing (summed curve/band fills/extent ghost/phase/GR from StripEq's
      engine), handles (glyphs, drawn Tilt, mini 3x22 GR meters, badges),
      band washes, piano strip, grid, drag readout, hover panel, the full
      keybind/mouse map from KBS test passes 1-6 (drag=freq+gain, Shift=fine,
      Ctrl=gain-only, Ctrl+Shift=freq-only, wheel-on-handle=Q, double-click
      empty=add / handle=MUTE, Alt-click=reset with home-freq restore for
      bands 1-8, Delete=delete, listen latch, grab arm G/button, arrows/Tab,
      Ctrl-Z/Y through our UndoManager with `beginParamUndoGesture`).
- [ ] **Views (SC-5, ruling 1b):** three views — STEREO / MID / SIDE.
  - [ ] View = domain: a band created in a view gets that engine channel;
        Stereo-view bands may be flipped Left/Right via right-click (badge);
        Mid/Side absent from every picker.
  - [ ] 24-band budget shared across views (chip row shows the pool; chips
        badge their view).
  - [ ] **Ghost:** the other views' summed curves + dots drawn dimmed,
        live, non-interactive, every frame.
  - [ ] Band-move: right-click > "Move to Mid / Side / Stereo view"
        (SC-18), settings kept, domain rewritten.
- [ ] House adaptations: our LookAndFeel/colors — the current EQ's DARK
      background kept (Jeff), no KBS Palette/Surface brushed-plate or recessed
      panes (Surface.h does not vendor; the look stays DAW-side and is NOT a
      fix-back), VibeTooltip (parentless, per the Z-ORDER TRAP — no
      juce::TooltipWindow child), WorkspaceWindow sizing, peer-keyed timers in
      `parentHierarchyChanged` (page-poll convention).
- [ ] Build gate + checkpoint commit + running notes.

### Task 6 — Display port II: rail, window, menus; old display deleted

- [ ] Port `BandRail` (+ DragNumber/SegmentRow/GrMeter) with the budget-fit
      layout; type glyph cells capped by WIDTH (KBS glyph trap); Slope combo
      9 entries; DOWN/UP direction row (Nova model); THR/RATIO/ATK/REL knobs +
      GrMeter; PAN knob only on Stereo-view gain bands.
- [ ] Rebuild `EffectEqWindow` content: chip row + graph + collapsible rail;
      NO brand block (Jeff); A/B control next to the "+" chip (Jeff); Pre/Post
      + view buttons in the title strip; options menu -> page hamburger via
      `installPageMenu` (existing convention) carrying: Reset All, A/B, Copy
      A->B, Lock, view selector (analyser/spectrogram/phase/piano), Processing
      Mode submenu with latency readouts COMPUTED from the engine at the
      session rate (retires the hard-coded mirror), Proportional Q, Auto-Gain
      + amount, Output/polarity, Keyboard & Mouse card, **presets** (12
      factory by category + Default + user presets + Save; user folder
      Documents\BaySickDAW\Presets\EQ per SC-17).
- [ ] DELETE: `ParametricEQDisplay` (both dead bind modes with it, SC-12),
      `EQ8DSP`/`EQ8MsDSP`/`EqLinearPhaseProcessor` files, `EffectEqWindow::
      mFallbackEq`, the old options/band menus in SharedUI.cpp.  Grep sweep
      for stragglers (`EQ8`, `evalBandDb`, `evalPhaseRad`, `BindMode`,
      `kEQDefaultFreqs`).
- [ ] Build gate + checkpoint commit + running notes.

### Task 7 — Features: grab, Match, collision, presets

- [ ] Spectrum grab: arm via G/crosshair button, arm-hold spectrum, any empty
      click takes the marker, cut bell from prominence/width, one grab per arm.
- [ ] EQ Match: port EqMatchPanel (capture current = pre feed; capture
      reference = a picked SC receive slot, our 4-slot world; Load Reference
      File = offline whole-file spectrum); replace-not-overlay apply through
      the graph's band writers; smoothness + band budget controls.
- [ ] Collision view vs the picked SC slot's feed.
- [ ] 12 factory presets ported as data + Default entry; user preset save/load
      in Documents\BaySickDAW\Presets\EQ (SC-17); dirty tracking via our param listeners.
- [ ] Build gate + checkpoint commit + running notes.

### Task 8 — Threading (SC-8) + cleanups (SC-11)

- [ ] Mode/oversampling changes route through the house idiom (nest-aware
      shield + settle — the pattern at PluginProcessor.cpp:7254-7260) from the
      new options menu; param plumbing per SC-15 (no automation lane,
      shielded apply).
- [ ] Page-preset import: wrap the `importPagePreset -> applyRacks ->
      loadRackStates` chain (PagePresetIO.cpp:451-488, :726-735) in the same
      idiom; cover the clipboard-paste variant (StandaloneEditor.cpp:2720) and
      the Rusty preset menu (StandaloneEditor.cpp:7676-7684).
- [ ] SC-11: stop creating the legacy per-audio-row `InstrChannelNode` (three
      callers), simplify `getAudioRowEQ` to the InsertNode only, drop the
      legacy `<InstrCh>` blob write; grep for readers of the legacy entries.
- [ ] Note: the old audio-thread detector allocation (EQ8DSP updateDetector)
      died with Task 6's deletion — record in running notes.
- [ ] Build gate + checkpoint commit + running notes.

### Task 9 — Export leading-latency trim (SC-10)

- [ ] `runOfflineLoop`: after the graph is primed, discard the first
      `totalLatencySamples` rendered samples before the first `consumeBlock`
      (same figure the live recorder already skips); extend the end condition
      by the same amount so `Tail::Cut` does not truncate real content;
      `measureRender` consumes the same trim (loudness math sees aligned
      audio).  Freeze untouched (pre-rack tap, no EQ latency baked in).
- [ ] Build gate + checkpoint commit + running notes.

### Task 10 — Docs, fix-backs, close

- [ ] Manual: EQ window pages rewritten (window layout, views + ghost, modes,
      dynamics, grab, match, presets), Callout Registry + Screenshot List +
      Verbatim Strings updated, manual regenerated, PDFs reprinted (screenshot
      re-shoots that need the running app -> flagged to Jeff like ANLZ was).
- [ ] System Reference EQ docs rewritten to the new architecture.
- [ ] Test plan §B.38 (EQ-1..n) MUST-PASS rows: views + ghost, 24-band pool,
      L/R only in Stereo view, linear-mode band edits reach the audio, flat
      linear EQ stays aligned (B2's negation), mode change mid-playback (no
      crash, clean fade), per-domain linear (side band leaves mono untouched),
      SC slots, grab, match, presets, export trim (grid alignment), old
      project loads with EQ reset (SC-14), page-preset import mid-playback.
- [ ] `Files For Claude/EQ Build Notes.md` — append the fix-back entry
      (KBS-facing): C3 correction (linear path ignored channel; the 26-fixed
      claim amended), the per-domain matrix-FIR design, the 4-slot SC
      extension, the Stereo/Mid/Side view + ghost design (Jeff: reference
      back to KBS as the EQ Pro fix).
- [ ] Future State draft text (Routing notes below): post-batch test-pinning
      sweep ("where else can we build proof targets"), any deferred polish.
- [ ] `/draft-doc batch-close` -> `/review-batch QA-EqPro` -> apply -> final
      commit (surface message + FULL git status; Jeff approves batch close).

---

## Verification (end-to-end smoke)

Tell Jeff (Debug exe first, then Release; one pass at batch end):
(1) Open a strip EQ: three views present, band made in Side view audibly moves
    only the sides (pan-wide source), ghost of the other views visible while
    editing, 24 chips shared across views.
(2) Stereo-view band right-click offers Left/Right and nothing mid/side.
(3) Linear mode: drag a band — the SOUND follows immediately; flat linear EQ
    on one bus stays sample-aligned with a parallel bus (no comb).
(4) Change Processing Mode while music plays: no crash, clean fade (repeat
    ten times fast).
(5) Import a page preset carrying EQ state while playing: no crash.
(6) Dynamics: THR pulls down = dotted extent line moves with it, GR meters
    (rail + handle) agree; DOWN/UP directions behave per the Nova model.
(7) Grab: arm, click a resonance on playing material — cut bell lands on it.
(8) Match: capture a reference file, apply, curve approximates it.
(9) Export a project with a linear-mode EQ: file starts on the grid (no
    leading offset), end not truncated.
(10) Load an OLD project: EQ comes up at defaults (expected reset), everything
    else intact; A->B->A still clean (TL-21 spirit).
(11) Fresh build: parameter count sanity — no EQ params registered until an
    EQ is touched (View > automation lists confirm).

## Routing notes (Rule 3 application during execution)

- Findings in EQ-adjacent code (racks, routing, windows): fix in-batch if
  they block a task, else surface to Jeff for routing (QA batches fix bugs
  found — deferral needs his call).
- Closed-batch discoveries route via Main Plan §9 Forks entries.
- Main Plan §5/§6 registration text for QA-EqPro: drafted at close in this
  section for Jeff to apply (he owns the plan docs).
- Post-batch follow-up (Jeff-ruled): test-pinning sweep across the codebase —
  drafted as a Future State entry at close.

## Carry-Forward Reference touch points

- §2 Lock-Free + Lifecycle Primitives — before Task 3 (wrapper) and Task 8
  (shield idiom reuse; do not reinvent publication patterns).
- §3 Mixer / Page Lifecycle File:Line Index — before Tasks 3-4 (strip spawn
  cascades, param materialization ordering).
- §4 Decisions Already Made — before Task 4 (no re-litigating settled
  registration/undo decisions).
- §6 Patterns To Reuse + §8 Anti-Patterns — before Tasks 5-6 (window/timer/
  tooltip conventions; the Z-ORDER TRAP; peer-keyed suspend).
