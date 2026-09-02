# Manual Pipeline (QA-ManualPress, 2026-08-28)

The manual is GENERATED. Prose is authored; every screenshot, every control
table number, and most callout dots come from the code, so the halves that
rot cannot rot. This doc is the operating manual for that pipeline.

## The pieces

| Piece | Where | What it produces |
|---|---|---|
| `bsd_shot` harness | `Source/Standalone/ShotHarness.h/.cpp` | Every figure PNG + `bsd-docs.json` |
| Menu capture hook | `Source/Standalone/ShotMenuHook.h` + 13 sites | Menus rendered headless |
| Dialog factories | `Source/Standalone/ShotFactories.h` (+ impls in `StandaloneEditor.cpp`) | The app's own dialog components + two build-only menus |
| Control blurbs | `Manuals/assets/control-blurbs.py` | Authored per-control table text |
| Assembler | `Manuals/assets/generate-manual.py` | `Manuals/manual.html` |
| Hand coords | `Manuals/assets/marker-coords.py` | Dot EXCEPTIONS only (see below) |
| Master figure data | `Plans & Specs/System Reference/Callout Registry.md` | Figure list, callout labels |

## bsd_shot - the figure harness

```
BaySickDAW.exe --shot [--docs] [--out=DIR] [--scale=X] ["Figure Name" ...]
```

- Runs headless: real processor, real components, no window, no audio
  device, no user. Output defaults to `Manuals/shots-staging/` (gitignored);
  the shipped `Manuals/figures/` set is only replaced deliberately (the
  Task 13 diff-sheet gate at QA-ManualPress set the precedent).
- 88 of the 90 shipped figures are automated. `Main frame.png` and
  `Hosted Plugin.png` stay hand captures.
- Scale defaults to 1.25 per figure: the original masters were captured on
  a 125% desktop and the assembler renders crops at native master pixels.
- `--docs` additionally writes `Manuals/assets/bsd-docs.json`: per figure,
  every stamped control (componentID on plain widgets, `VKnob::paramId` on
  knobs) with slider-rendered range strings, resolved parameter metadata
  (name, default, skew, choices by NAME), percent bounds, plus every
  captured menu's row rects. The `kExtraDocs` table in `ShotHarness.cpp`
  documents controls the tree walk cannot see.

Headless rules (violate one and figures silently break):

- Timers never fire. Polled state is set through the `pollNow()` /
  `pollForShot()` seams; the ONE timer pump (vox family) is
  `Thread::sleep(550)` + `callPendingTimersSynchronously()`.
- No component reaches the desktop. `WorkspaceWindow` grows a peer only in
  `attachTo`, which the harness never calls; `CallOutBox` is parented.
- The default LookAndFeel is installed by hand at run start.
- Figure order carries state (the registry comment in `kFigures[]` is the
  contract): the empty rack shoots before effects load, Builder seeds the
  pattern content Piano Roll and the Event Editor read, engine menus ride
  earlier figures' kit loads, and the `StandaloneEditor` figure group runs
  LAST (its constructor re-points the processor's PatternManager and
  installs process statics).

## Menus

A real menu cannot show headless (`JUCE_MODAL_LOOPS_PERMITTED=0`,
`MenuWindow` is module-private), so:

- `MenuCanvas` (in `ShotHarness.cpp`) reproduces `MenuWindow`'s layout and
  paints through the SAME LookAndFeel virtuals the real window calls -
  drawn equals shown, verified at 1-2px of the hand masters.
- Build-only builders are called directly (`BuilderPage::build*Menu`,
  `EffectsPage::buildTitleMenu`, `RibbonTabBar::buildAddMenu`,
  `PianoRollMenuBar`/`DrumKitMenuBar`, `StandaloneEditor::getMenuForIndex`,
  `MixerPage::buildAddMenu`, `shots::buildMixerTitleMenu`,
  `shots::buildAnalyzerMenu`, `GlobalAutoRightClick::buildControlMenu`).
- Build-and-show sites hand their menu over through
  `shots::maybeCapture(m)` - one line before each `showMenuAsync`, armed
  only while the harness shoots. Public triggers exist where the entry was
  private (`PageMenuBar::triggerMenuForShot` / `triggerExtraHeadingForShot`,
  `MixerPage::showSendMenuForShot`,
  `GlobalTransportBar::showRecordMenuForShot`,
  `BaySickPedalsEditor::showSwapMenuForShot`).
- The ONE replica: the BaySickRustyDrums page menu (its builder lives
  inline in a StandaloneEditor lambda) - drift risk documented in the
  QA-ManualPress running notes.

## Control tables

`generate-manual.py` renders a Controls table (Control / What it does /
Default / Range) at the top of every In Depth chapter whose figure has
dumped controls. Numbers come from `bsd-docs.json` - never typed; enum
choices render by NAME. Blurbs are authored in
`Manuals/assets/control-blurbs.py`, keyed by param id (or
`"<figure>|<label>"` with `#n` ordinals for id-less sibling-stage knobs -
stage order is walk order).

## Callout dots - generated plus exceptions

- MENU figures self-anchor: callouts number down the rows by convention,
  the harness emits each menu's row rects, and the assembler rebuilds
  `C[code]` from them when the Callout Registry's callout count matches
  the row count (tried with, then without, section headers). 32 of 38
  menu figures generate; a mismatch keeps hand coords and prints
  `DOT MISMATCH` in the build output.
- The six standing dot exceptions use deliberately GROUPED callouts (one
  dot for a run of rows): ANLZM, BSPDLP, EQB, FXPICK, MIXADD, PRC.
- Every other figure's dots stay hand-coordinated in
  `marker-coords.py` (`bsd-docs.json` already carries every stamped
  control's percent bounds if id-anchoring is ever extended there).
- A menu that GROWS breaks its count on purpose: add the new callout row
  to the Callout Registry AT ITS VISUAL POSITION (renumber below it, and
  renumber the chapter's `id="CODE-n"` anchors to match) and the figure
  self-anchors again. Help Menu / `View Projects MidiMap` (2026-08-28) is
  the worked example.

## The regeneration workflow

1. UI changed, or docs changed: `do_build.bat`, then
   `BaySickDAW.exe --shot --docs`.
2. Inspect `Manuals/shots-staging/` against `Manuals/figures/` (the
   Task 13 `diffsheet` pattern: side-by-side old vs new).
3. Replace the shipped figures deliberately (copy staging over
   `Manuals/figures/`), never as a side effect.
4. `python Manuals/assets/generate-manual.py` - watch `menu dots gen`,
   `DOT MISMATCH`, and `topics placed: 89 of 89`.
5. PDFs, one per level:
   `msedge --headless=new --disable-gpu --landscape --no-pdf-header-footer
   --virtual-time-budget=90000 --print-to-pdf=<out>
   "file:///<repo>/Manuals/manual.html?level={view|depth|weeds}&print=1"`

## Layout rulings that live in the assembler's CSS

- The content column is pinned at FULLSCREEN width (`--fsw`) so
  screenshots never rescale and scrolling stays vertical-only (Jeff's
  ruling); TEXT and TABLES additionally cap at the live window width with
  a right gutter (Jeff, 2026-08-28) - only the figure strip clips at
  narrow windows. Print is exempt from the cap.
- `pre` scrolls horizontally inside its own block (In The Weeds excerpts).

## In The Weeds discipline (5C)

Every `IMP-*.html` excerpt is a verbatim quote with a path + function +
line codehead. When code moves, fix the citation; when code changes,
re-quote from source - never paraphrase. Audited in full at QA-ManualPress
Task 11 (3943 quoted lines matched against the tree).
