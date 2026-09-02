# Running Notes - QA-Solstice (solar-scrubbing-sparrow)

> **Purpose.** Append-only mid-batch log of what was done, what was found, what
> was decided, and what was deferred during QA-Solstice execution.  Compiled from
> `/draft-doc running-notes` dispatches at every significant checkpoint
> (commit landed / sub-task verified / finding captured / decision made /
> scope pivot / spec call resolved).
>
> At batch close, `/draft-doc batch-close` reads this file (plus git log,
> memory entries, the per-batch plan, and conversation context) and produces
> the single Implemented Work Log entry that goes into `Plans & Specs/
> Implemented Work Log.md`.  This file is the source-of-truth intermediate
> artifact during the batch; the close entry is the durable summary.
>
> **Pair file:** `Plans & Specs/Batch Plans/solar-scrubbing-sparrow.md`
> (the per-batch plan).
>
> **Convention:** see `Plans & Specs/Main Plan.md` §0 (folder-scope rule +
> Agent Orchestration Rules' mid-batch checkpoint trigger).

## 2026-09-02 - Batch open - rulings

- **Trigger:** Jeff: "Harmless" is the original name of Image-Line's additive
  synth.  Root cause of the miss: `feedback_no_brand_names_in_user_facing_strings.md`
  (2026-06-07) listed `Harmless` as a brand-safe substitute, so every audit since
  skipped it.
- **Rulings, first round:** (1) saved projects + user patches break, no migration
  - "This is fine"; (2a) Claude syncs Jeff's `Documents/BaySickDAW` copy;
  (3b) scrub everything incl. history; (4c) own batch.  Brand sweep becomes a
  task in the new plan.  Jeff also asked for the dirty tree to be committed
  first - landed as `b240712a` (QA-ManualPress checkpoint + DSP Portability
  Matrix).
- **Rulings, second round (Rule 5 pose):** (1a) four tasks; (2a) commit per
  task, "no approval just go"; (3c) Main Plan gets a new Phase 8, Legal / Brand
  Safety; (4a) T4 uses a semantic-read agent.
- **Stated, no objection:** prefix `bso`; figure codes `BSSOL` / `BSSOLM`;
  `AdditiveVoice` / `HarmonicEngine` / `SpectralModules` / `VisualizerScreen`
  keep their names; `Templates/My Templates/Test Kit.xml` (user file) left alone.
- **Verified before planning:** unknown engine-type string degrades to an
  engineless tab (`EngineRig.cpp:566`), no crash; Jeff's disk has no
  `My Presets` under the old folder; `Tools/gen_factory_presets.py` writes
  directly to `Documents/BaySickDAW`; the on-screen `HARM` knob label
  (`HarmlessEditor.cpp:787`) collides with the old figure-code root and must
  survive the substitution; the adjective "harmless" appears in ~25 comments and
  one lowercase variable (`StandaloneEditor.cpp:15708`) is the engine.
- **Drift noted:** QA-EqPro and QA-ManualPress were never added to Main Plan
  §5 / §9; QA-Solstice's Phase 8 entry is the first Main Plan batch entry since
  QA-Layout.

## 2026-09-02 - Task 1 - engine rename (in progress, build gate running)

- `git mv Source/Harmless Source/BaySickSolstice` + 18 `Harmless*` -> `BaySickSolstice*`
  files; substitution pass over 62 files (Source, CMakeLists, generator):
  `Harmless` -> `BaySickSolstice`, `_harm_` -> `_bso_`, `"harm"` -> `"bso"`,
  `BSHARMM-`/`BSHARM-` -> `BSSOLM-`/`BSSOL-` anchors; by rule the lowercase
  engine variable (`StandaloneEditor.cpp:15708/15729` -> `solstice`), the
  harness window name + group key (`baysicksolstice`), generator identifiers
  (`BAYSICKSOLSTICE_*`, `ENGINE_BSO`, `BSO_DEFAULTS`).  Verify grep empty.  The
  `HARM` Blur knob label and the `blur_harm` param survived as intended.
- **Finding: the repo root IS the app root.**  `AppPaths::appRoot()` is
  `Documents/BaySickDAW`, which is where the repo lives, so the app reads the
  repo's `Presets/` and `Templates/` directly.  S5 (copy to Documents) is moot;
  `git rm -r Presets/Harmless` + the regenerated `Presets/BaySickSolstice/`
  (152 XML) + 24 rewritten `Templates/Factory` files ARE the on-disk state.
  User content lives in the same tree untracked (`Templates/My Templates`,
  `Kits/My Kits`, any `My Presets`) - untouched.
- **Finding (pre-existing, Rule 3 at close): generator drift.**  Run against a
  scratch home, `Tools/gen_factory_presets.py` output differs from the
  committed presets for BaySickBass (3), BaySickDrums (6), BaySickSynth (7) -
  sample: `tk_lay_0_bss_amp_release` 0.0 (generator) vs 0.001 (committed), a
  hand fix that never reached the recipes.  A real re-run would regress 16
  files.  BaySickNAMIR presets are hand-authored (generator does not emit
  them).  Only the renamed folder + `Templates/Factory` were taken.

## 2026-09-02 - Task 2 - manual (text half done while the T1 build runs)

- `git mv` BSHARM.html / BSHARMM.html -> BSSOL.html / BSSOLM.html and the two
  System Reference pictures; substitution over 25 manual-side files (src-m2
  prose, 10 In The Weeds files, marker-coords, generate-manual IMP map,
  control-blurbs, the cumulative code-rename map, the interim m2, bsd-docs.json,
  Callout Registry, Screenshot List).  Verify grep empty.
- **Lesson (do not repeat):** `fullcode.py` / `fullcode2.py` from QA-ManualPress
  are one-shot BUILDERS, not regenerators - re-running them clobbered the
  hand-assembled straggler dropdowns (the `imp*.txt` set) in 20 files, 900 net
  lines gone.  Reverted `src-m3` to HEAD and re-applied only the rename rules,
  which match the source by construction: 10 files, 829 quoted lines, 0 misses
  against the cited sources (whitespace-normalised).
- Pending the build: re-shoot (`--shot --docs`), copy staging over figures,
  `git rm` the two old-named PNGs, regenerate `manual.html`.

## 2026-09-02 - Task 1 - committed `005fb2ee` (400 files)

- Build gate green: six exit codes 0, four link lines, zero errors.  One
  comment (`EngineRig.cpp:113`, the adjective "Harmless for the kinds...") was
  restored AFTER the gate - comment-only, binary unaffected.

## 2026-09-02 - Task 3 - docs + memory scrub (done in the working tree, commit held until T2 lands)

- `git mv System Reference/Harmless.md -> BaySickSolstice.md`; substitution
  over 100 files: every Plans & Specs doc incl. history (Work Log, Running
  Notes, Batch Plans, Previously Implemented, Research Reports, Carry-Forward,
  Test Plans, Future State, Main Plan), CLAUDE.md, README.md, `.claude/agents`,
  `STANDALONE_UI_CHANGES.md`, and both memory dirs (18 files;
  `feedback_harmless_ghost_params.md` renamed).
- **Protected from the scrub** (the record must keep the old name to make
  sense): this plan, these notes, Main Plan's Phase 8 block + section-9
  seventieth entry, and the brand-rule memory - rewritten by hand: `Harmless`
  removed from "brand-safe substitutes", a "coined names are not auto-safe"
  clause added, the 2026-09-02 miss recorded as Incident 2 with the root cause.
  `MEMORY.md` hook updated to match.
- **Adjective trap:** "Harmless" capitalised at sentence start is the English
  word, and the substitution cannot tell.  Reviewed by grep for
  `BaySickSolstice <lowercase connective>`; three real hits restored
  (`EngineRig.cpp:113`, `Running Notes/gentle-swapping-gecko.md:106`,
  `Running Notes/deep-packing-badger.md:126`).  "a BaySickSolstice on Layers"
  style hits are the engine and stayed.
- Tree-wide `git grep Harmless` now returns only the batch record (plan 47,
  Main Plan 8, notes 6) and the three adjectives.  No source touched in T3
  beyond that one comment, so T1's gate stands.

## 2026-09-02 - Task 2 - figures re-shot, manual regenerated

- `--shot --docs` from the T1 Release exe: 130 written, 0 failed.  Against the
  shipped set: 116 byte-identical, 2 new (the renamed panel + shared engine-tab
  menu figures), 12 changed - `Ribbon + Menu` (the `+ Add BaySickSolstice` row,
  expected) and 11 pedal tiles.  The pedal diffs are anti-aliasing-level
  (`Pedal Fuzz` differs by ONE pixel; the others in 1-40 px strips), stable
  run-to-run (a second isolated shoot matched the first byte-for-byte), and
  visually identical side by side - shipped.
- `generate-manual.py`: 91 figures, 734 markers, 182 generated control dots,
  32 menu figures self-anchored, 89 of 89 topics placed; the six DOT MISMATCH
  lines are the standing grouped-callout exceptions, unchanged.  `BSSOL` is
  not on the DOT TODO list: all 27 panel dots + 15 menu dots anchored from
  `bsd-docs.json` (88 controls walked on the renamed panel).  Looked at both
  dot previews: dots in whitespace, title bloom reads BaySickSolstice, `HARM`
  knob label intact.  `manual.html` carries zero "Harmless".
- **Finding (pre-existing, Rule 3 at close):** five PNGs in `Manuals/figures`
  are referenced by nothing in the pipeline - `BaySickBass Menu.png`,
  `BaySickPlayer Menu.png`, `BaySickSynth Menu.png`, `Hosted Plugin Menu.png`,
  `Window Chrome.png` (superseded by the shared engine-menu figure and the
  chrome crops).  Left in place; deletion is Jeff's call.
- **Finding (pre-existing, Rule 3 at close):** the `Pedal NAM Pedal` tile clips
  its first two knobs (Input/Drive, Mid) off the tile's left edge - visible in
  the shipped QA-ManualPress figure and in this re-shoot alike.  A pedal-tile
  layout defect, not a harness one.

## 2026-09-02 - Task 2 - committed `fe4931d3`

## 2026-09-02 - Task 3 - committing the scrub (working-tree work logged above)
