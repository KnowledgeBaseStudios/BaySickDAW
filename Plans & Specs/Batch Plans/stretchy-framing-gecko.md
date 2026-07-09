# QA-Eb — App-Window Resizability (min-size clamp shape) — Plan (stretchy-framing-gecko)

> **Canonical path (post-approval):** `Plans & Specs/Batch Plans/stretchy-framing-gecko.md` (mirrored
> at G1 group approval; home-dir copy deleted). **For execution:** BULK-RUN mode — no per-task verify
> pauses; Verify scripts author into Master Test Plan §B; Work Log entry drafted + HELD; one source
> commit.

## Context

QA-Eb (§5, inserted 2026-05-17) planned resizable window + maximize + min-size clamp + an outer
Viewport for sub-design-size scrolling. **Premise correction (2026-07-08 surface map):** the window
launches fullscreen with `setResizable(false,false)` and `StandaloneEditor::resized()` is already
fully proportional — there is NO fixed design size, and pages are z-stacked inside ONE content
component, so an outer Viewport would double-wrap the four self-scrolling surfaces (piano roll,
Builder grid, Mixer, drum-kit lane) — the exact scroll-fight §5 forbade. **Jeff's locked call (C=1,
2026-07-08): min-size clamp shape — enable resize + working maximize + a floor the layout stays
usable at; NO outer Viewport.** Batch shrinks accordingly.

- Risk: low — window-chrome flags + resize limits; no audio/DSP surface.
- Effort: ~2-3h (was ~3-5h under the Viewport shape).
- Dependencies: none. Slot: fourth in G1 (testing-efficiency adjacency as originally justified —
  a resizable window speeds every later verify).
- **Bucket:** UI / L&F / Theming (per §5 entry's original intent; entry has no Bucket line — add one
  at section-pass close).

## Spec calls already locked (with reasoning)

| ID | Decision | Reasoning |
|----|----------|-----------|
| C | Min-size clamp shape; no outer Viewport | Jeff 2026-07-08, after the no-design-size premise correction. Pages already reflow; four surfaces self-scroll; EffectsPage squeeze is bounded by the floor. |
| C (delegated) | I derive the floor value; Jeff tunes at the G1 smoke | Part of the C=1 pick. |
| — | Launch behavior unchanged (starts maximized) | Existing `setFullScreen(true)` behavior is kept; only user-resizability + working maximize/restore are new. Changing launch size would be an unprompted behavior change. |

## Sub-spec calls surfaced for ExitPlanMode

No sub-spec calls open.

## Files to modify

### Task 1
- `Source/Standalone/StandaloneApp.cpp:697-704` — the window config block:
  `setResizable(false,false)` → user-resizable; add `setResizeLimits(...)` (child-window precedent:
  `EventEditor.cpp:2048`); keep `setFullScreen(true)` launch.
- `Source/Standalone/StandaloneApp.cpp:11-29` — `VibeSynthWindow` (only if maximize/restore needs a
  hook; `allButtons` already advertises the maximize button).
- No `StandaloneEditor::resized()` changes expected (`:9016-9055` is proportional; the `ribW > 60`
  guard plus the floor cover the narrow case).

## Tasks

### Task 1 — resizable window + maximize + floor

- [ ] `mWindow->setResizable(true, false)` (native-border resize, no corner widget — matches the
      resizable child-window precedents); confirm `allButtons` maximize now works with restore.
- [ ] Derive the floor from the fixed chrome + control minimums: width ≥ transport
      `kControlsWidth` 520 + pattern 176 + readout ~100 (QA-TransportDisplay lands first) + ribbon
      min 60 + CPU reserve 120 + margins ≈ **1100**; height: menu 24 + bar 40 + page menu 26 +
      smallest usable page (PianoRoll `kMinGridH` 120 + toolbars/lane, EffectsPage 6 slots ≥ ~64px
      each) ≈ **700**. Ship `setResizeLimits(1100, 700, 32000, 32000)` as the starting floor —
      **Jeff tunes at the G1 boundary smoke** (delegated).
- [ ] Sanity-pass every page at exactly the floor size (code-read + spot-run): the four
      self-scrolling surfaces keep sole scroll authority; EffectsPage slots remain operable; no
      negative-bounds paths (PianoRoll `kMinGridH`, ribbon guard).
- [ ] Rule 6 pass on touched regions.

### Task 2 — batch close (bulk-run shape)

- [ ] Author Master Test Plan §B section "QA-Eb" from the Verify scripts (`blocks:` = batch commit).
- [ ] Draft + HOLD Work Log entry in `Running Notes/stretchy-framing-gecko.md`; append
      code-complete running-notes entry (records the premise correction: §5's Viewport scope
      superseded by C=1 — original §5 text preserved, per the inline-back-ref convention at
      section-pass close).
- [ ] One source commit (Rule 9): message + full git status → Jeff approves → commit.

## Verify scripts (→ Master Test Plan §B; Debug first, then Release)

1. Launch → window still opens maximized; restore-down gives a movable, border-resizable window.
2. Drag-resize smoothly in both axes — pages reflow live, no paint artifacts, no jasserts (Debug).
3. Maximize button toggles maximize/restore correctly (position + size restored).
4. Shrink hard toward zero → window stops at the floor; at the floor: transport bar fully usable
   (all buttons + readout + pattern dropdown + some ribbon), piano roll scrolls via its own bars,
   Builder grid via its viewport, Mixer via its h-scroll, EffectsPage slots clickable.
5. Every page visited at the floor and at an in-between size (e.g. 1400x900) — no clipped-off
   controls anywhere, exactly one scroll authority per surface.
6. Save/reopen project at a small window size → no layout-dependent state issues.

## Routing notes (Rule 3 application during execution)

Page-level layout defects found at small sizes that are NOT floor-fixable route to QA-L (UI polish,
G3) or log as findings for the section pass; do not creep per-page relayout into this batch (§5
explicitly post-V1s proportional page relayout). Window-state persistence (remember size/position)
is net-new and NOT scoped — if wanted, it routes to Future State.

## Carry-Forward Reference touch points

- §1 skim only (window/app bootstrap); the 2026-07-08 QA-Eb surface map supersedes the §5 entry's
  Viewport-era assumptions (original §5 text stays; correction recorded at close per convention).
