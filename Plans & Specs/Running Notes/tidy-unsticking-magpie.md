# Running Notes — QA-L (tidy-unsticking-magpie)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/tidy-unsticking-magpie.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval. Composition: BLU-378/379/492 OUT (QA-ApvtsAutomation, marathon
18); per-drum MIDI notes IN (#11=B, QA-Drum-Polish batch dropped; kit fan-out #10=a/a,
default unmapped); #17=c Clips picker gating dropped; #18 FX Rack button on all six
page-tab rows (both Inst variants); C = two piano-roll menu-bar buttons ("Player Page" +
"FX Rack") right of the roll dropdown. Scout premise corrections in the plan (MIX-05 real
cause = orphaned Layer/Bass/Drum strips on page close; MIX-07 same asymmetry; UI-01 =
vendored-JUCE any-button trigger). Coding starts after QA-K.
