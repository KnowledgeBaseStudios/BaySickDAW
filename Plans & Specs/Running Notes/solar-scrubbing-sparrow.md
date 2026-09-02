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
