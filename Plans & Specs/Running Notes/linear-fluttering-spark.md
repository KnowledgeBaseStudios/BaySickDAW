# Running Notes — QA-Sfizz-Followup (linear-fluttering-spark)

> **Purpose:** Append-only running log for the QA-Sfizz-Followup batch. A new entry is appended at every checkpoint — commit landed / sub-task verified / finding captured / spec call resolved / scope pivot — per `feedback_draft_doc_running_notes_every_checkpoint.md` and the Main Plan §0 running-notes rule. At batch close, `/draft-doc batch-close` reads this file as the primary input when compiling the single Implemented Work Log entry. Never edit prior entries; surprises get their own new entry.

> **Pair file:** `Plans & Specs/Batch Plans/linear-fluttering-spark.md` (the batch plan).
> **Conventions:** Main Plan §0 "Document Formatting Conventions" + "Batch Plans + Running Notes layout (locked 2026-05-11)".

## Diagnostic Instrumentation Catalog

Per Main Plan §0 Rule 4. None added — this is a known-root-cause fix (sfizz CC dispatch-at-init), verified via Jeff's audio testing, not log traces. Any temp `DBG` / `juce::Logger` / `AlertWindow` / temp `jassert` added during a verify-fail investigation gets a row here IN THE SAME EDIT PASS, and every `Remove` row is stripped (strip list surfaced to Jeff) before the relevant commit.

| Site | Tag | Purpose | Disposition |
|------|-----|---------|-------------|
| (none yet) | | | |

## 2026-06-01 — Task 0 — open

- Batch opened. Pre-batch ritual: `/standup`; full self-read of Main Plan §0 (Rules 1-5 + Document Formatting Conventions + canonical buckets + Agent Orchestration Rules); targeted extractions (§5 QA-Sfizz-Followup entry + §6 footnote + §9 forty-sixth Forks origin; Work Log QA-EngineApvts close incl. FND-2/FND-4; QA-Sfizz Sub-E history; exemplar `federated-bouncing-cupcake.md`). CLAUDE.md cross-check: "Next Steps -> Next batch: QA-Md" confirmed **stale** vs Work Log (QA-Md closed 2026-05-09; real next = QA-Sfizz-Followup) — relying on Work Log / Main Plan for status.
- **Root cause confirmed in source** (beyond the FND-2/FND-4 notes): the only CC->sfizz path is `parameterChanged -> mSfizz->cc()`, which fires only on an actual APVTS value change. `loadKit`'s reset-to-64 `setValueNotifyingHost` no-ops when the param is already 64 (Guitars `:367-372`); `setStateInformation`'s `replaceState` no-ops when saved==current (Guitars `:425`). So the Sub-E 64 default never reaches sfizz at load -> CC-gated `<master>` articulations sound as if the CC were 0 until moved. Sustain risk of a blanket push verified real: sfizz `Defaults.cpp:75` `sustainCC {64}` + `:79` `checkSustain {true}`.
- **Spec calls locked by Jeff (2026-06-01):** SC-1 all three engines (Guitars + Basses + RustyDrums); SC-2 only kit-exposed controls (no blanket push); SC-3 one consolidated source commit; SC-4 direct `mSfizz->cc()` push (NOT the no-op `setValueNotifyingHost` forced-delta the §5 text suggested).
- **SC-5 grounded by real-kit cross-check:** GUI-XML knob CCs == `label_cc` set exactly for Black&Green Guitars + Black&Blue Basses; Big Rusty Drums `label_cc` is a superset incl. every GUI knob (CC4 hi-hat included). None of the three kits labels CC7/CC10/CC64, so dispatching `mCcLabel` never touches sustain/volume/pan (satisfies SC-2). "Exposed controls" therefore = `mCcLabel` keys, already populated by `loadKit`'s scan -> processor-local fix, no UI->processor plumbing. Resolves the SC-2 residual-risk caveat flagged when posing the spec call.
- Plan written + Jeff-approved (no edits); mirrored to `Batch Plans/linear-fluttering-spark.md` (home-dir copy deleted per `feedback_plan_mirror_one_way.md`); §5 `**Plan file:**` pointer added; this running-notes file seeded.
- **Next:** Task 0 open commit (docs only) after surfacing full git status + `/draft-commit` for approval.
