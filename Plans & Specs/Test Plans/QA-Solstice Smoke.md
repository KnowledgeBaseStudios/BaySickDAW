# QA-Solstice Smoke - Harmless -> BaySickSolstice (2026-09-02)

**Status: not yet walked.**  Jeff walks it; record results in the tables.
Batch: `Batch Plans/solar-scrubbing-sparrow.md`.

## What to run

- **Debug first:** `build\BaySickDAWStandalone_artefacts\Debug\BaySickDAW.exe`
  (title shows `[DEBUG]`).  Any `jassert` dialog: screenshot the path + line +
  condition.
- **Then Release:** `build\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe`.
- Both exes are current: the last source change was T1 (`005fb2ee`, gate
  green - six exit codes 0, four link lines, zero errors); T2-T4 touched docs,
  figures and the manual only.  Do not run both exes at once (ASIO is
  exclusive).
- Expected loss, not a bug: any project saved before this batch with a
  Harmless tab restores that tab with NO engine (engine id, state tag and
  param prefix all changed; no migration pre-v1).  Any user patch that was
  under `Presets/Harmless/My Presets/` is gone with that folder - none existed
  on this machine when checked.

## Part 1 - the engine (T1)

| # | Do | Expect | Debug | Release |
|---|---|---|---|---|
| 1 | Layers ribbon `+ Add` | The engine row reads **BaySickSolstice**; adding one opens a panel whose title strip reads BaySickSolstice in the usual bloomed orange, same accent as before | | |
| 2 | On that panel, `Load Preset` | The menu walks `BaySickSolstice / <category> / ...` with all 20 categories (Arp & Sequencer Tones through Synthwave & Vintage); pick any patch - it loads and sounds | | |
| 3 | `Save Preset` with any name | The file lands in `Documents\BaySickDAW\Presets\BaySickSolstice\My Presets\` and reappears in the Load menu | | |
| 4 | File > New from Template > **Bass Music** | 5 Layer tabs + 3 Bass tabs populate as BaySickSolstice with their patches loaded (the template's engine and presetPath values were regenerated) | | |
| 5 | Right-click any BaySickSolstice knob > Automate | The Builder lane label reads `Pg Layer 1 - BaySickSolstice - <knob>` (the `bso` tag resolves to the new name) | | |
| 6 | Open a project saved before this batch that had a Harmless tab | The tab restores with no engine ("(no engine)" in the piano-roll context label); the app does not crash or assert.  Expected loss per the batch rulings | | |
| 7 | The BLUR / PRISM section of the panel | The harmonics knob is still labeled `HARM` (an abbreviation, kept on purpose) | | |
| 8 | Bass ribbon `+ Add` and the Replace Engine submenu on a Layer tab | Both list BaySickSolstice (the shared engine-tab menu figure was re-shot from this) | | |

## Part 2 - the manual (T2)

Open with F1 (or Help > Manual).

| # | Do | Expect | Result |
|---|---|---|---|
| 9 | Sidebar, instruments group | **BaySickSolstice** is listed; its chapter opens on the new panel figure with 27 numbered dots and an In Depth control table (88 controls) | |
| 10 | Search "Harmless" | No results.  Search "Solstice" finds the chapter and its menu page | |
| 11 | Eyeball the two HAND captures: `Main frame` (the Window Chrome / main-frame chapter) and `Hosted Plugin` (the Plugins chapter) | Neither shows the word "Harmless" anywhere.  If one does, that figure is a re-capture for you - the harness cannot shoot those two | |
| 12 | The BaySickSolstice Menu page | 15 numbered rows, dot 1 = Player, dot 15 = Delete Layer | |

## Part 3 - the factory content on disk

| # | Do | Expect | Result |
|---|---|---|---|
| 13 | Explorer: `Documents\BaySickDAW\Presets\` | `BaySickSolstice\` exists (152 patches in 20 folders); no `Harmless\` folder.  This is the repo folder - the app reads it directly | |
| 14 | Explorer: `Documents\BaySickDAW\Templates\Factory\` | Opening any of the 24 engine-bearing templates in a text editor shows `engine="BaySickSolstice"` | |

## If something fails

- Wrong or missing engine name anywhere in the app: it is a string the
  substitution did not reach - note the exact text and where; `git grep` finds
  it in seconds.
- A crash on #6: that is a real bug (the unknown-engine path at
  `EngineRig.cpp:566` is supposed to return null cleanly) - screenshot the
  jassert.
- Manual shows old figures: hard-refresh (the manual window caches); the
  figures at `Manuals/figures/` were replaced at `fe4931d3`.

## Results

Walked on: ______   Debug: pass / fail   Release: pass / fail
Notes:
