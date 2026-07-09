# Running Notes — QA-Eb (stretchy-framing-gecko)

> Append-only mid-batch log. New entry at every checkpoint. Under the BULK RUN this batch's Work
> Log entry is drafted at code-complete, HELD here under `## Held Work Log entry (apply at section
> pass)`, and applied only when its Master Test Plan §B section passes.

Pair file: [`Batch Plans/stretchy-framing-gecko.md`](../Batch Plans/stretchy-framing-gecko.md).
Conventions: Main Plan §0 + [`Batch Plans/swift-stampeding-caribou.md`](../Batch Plans/swift-stampeding-caribou.md).

## 2026-07-08 — G1 group open — plan approved + surface map (source-verified)

Locked: C (min-size clamp shape; NO outer Viewport; floor derived by Claude, Jeff tunes at smoke).
Premise correction recorded: §5's fixed-design-size assumption is false — layout is fully
proportional; the §5 Viewport scope is superseded (original §5 text preserved; correction noted in
the §5 plan-file pointer line + here). Verified refs:

**Window:** `VibeSynthWindow` `StandaloneApp.cpp:11-29` (ctor `:14-17`, `allButtons` — maximize
button exists but inert); init `:688-704` — `setContentOwned(editor,true)` `:699` (editor direct,
NO viewport anywhere), `setResizable(false,false)` `:702` (THE line), `setFullScreen(true)` `:703`
(launch behavior — keep). `mWindow` `.h:131`.

**Editor layout:** `StandaloneEditor::resized()` `.cpp:9016-9055` — proportional from
`getLocalBounds()`; fixed chrome 24 (menu) + 40 (bar) + 26 (page menu) = 90px; pages z-stacked with
identical bounds `:9048-9054`, toggled by visibility (`showPageForTab` `:4553-4582`); only guard =
ribbon `>60` `:9043`; editor never self-sizes.

**Self-scrolling surfaces (sole scroll authority — do not wrap):** PianoRoll H+V ScrollBars
(`PianoRoll.h:640-641`, `.cpp:2789-2797`, `:3161-3170`, layout `:3341-3388`, `kMinGridH=120`
`:3342`); Builder `mGridViewport` (`BuilderPage.h:1008`, `.cpp:5547-5551`, `:5684`, header sync
`:5118`/`:5757-5758`); Mixer viewport + custom H bar (`MixerPage.h:240-241/265`,
`.cpp:1465-1482`, `:3701-3727`); DrumKitGrid H bar (`DrumKitGrid.h:478/402/469`). NOT
self-scrolling: `EffectsPage` (`.cpp:976-1002`, `slotH = h/6` `:990`) — the floor bounds its
squeeze.

**Precedents:** `setResizeLimits(600,360,2000,1200)` `EventEditor.cpp:2048`; resizable children
`KeyBindsWindow.cpp:452` etc. No ComponentBoundsConstrainer, no window-state persistence anywhere
(persistence = out of scope; Future State if wanted). Starting floor: **1100 x 700**.

## 2026-07-08 — Task 1 CODE-COMPLETE (built clean with QA-Ec, Jeff)

- `StandaloneApp.cpp` window-init block: `setResizable(false,false)` -> `(true,false)` (border
  drag, no corner widget — matches the resizable child-window precedents) +
  `setResizeLimits(1100, 700, 32000, 32000)` (floor derivation in the code comment; Jeff tunes at
  the G1 smoke via EB-4) + launch stays maximized (`setFullScreen(true)` untouched — no unprompted
  launch-behavior change). That is the entire code half; everything else in this batch is §B.4
  runtime verification (the C=1 re-shape shrank it exactly as planned).
- Window-size persistence deliberately NOT added (net-new, unscoped -> Future State if wanted).
- **Diagnostics:** none. **Files:** `StandaloneApp.cpp` only + test plan §B.4.

## Held Work Log entry (apply at section pass)

> Apply verbatim at §B.4 section pass; fill `<hash>` + date/outcome; group review line at G1 boundary.

### <APPLY-DATE> — QA-Eb — User-resizable app window + working maximize + 1100x700 floor (min-size-clamp shape per the G1 premise correction; outer-Viewport scope superseded — no fixed design size exists and the layout is fully proportional)

**Bucket:** UI / L&F / Theming

#### Done

- `setResizable(true, false)` + `setResizeLimits(1100, 700, 32000, 32000)` on the main window
  (`StandaloneApp.cpp` init block); maximize button live via the pre-existing `allButtons`; launch
  stays maximized. Floor values are EB-4-tunable starting points derived from the 40px bar's fixed
  occupants (~1050w) + chrome/page minimums (~700h).

#### Found along the way

- (From plan-time, recorded at open) §5's outer-Viewport scope was written against a fixed design
  size that does not exist — layout is fully proportional and pages are z-stacked in one content
  component, so a wrap would double-wrap the four self-scrolling surfaces. Jeff's C=1 pick
  re-shaped the batch to the clamp; original §5 text preserved with the correction noted at the
  plan-file pointer.

#### What was done about each finding

- Re-shape locked pre-plan (G1 answer C); nothing further routed.

#### Group review (R3)

- <G1-boundary outcome>

#### Diagnostic Instrumentation Catalog

- None added.

#### Files touched

`Source/Standalone/StandaloneApp.cpp`, test plan §B.4, paired plan + running notes.

#### Commit(s)

`<hash>` (Task 1 + §B.4 + held entry — single batch commit). Verified via Master Test Plan §B.4,
<section-pass date/outcome>.

#### Next action

- <filled at apply>.
