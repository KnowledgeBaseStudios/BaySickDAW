# Running Notes — QA-N (honest-summing-falcon)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/honest-summing-falcon.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval. Scout truth in the plan: no per-worker accounting exists;
meter cap is live 10.0 w/ HOLD-FOR-Phase-6 comment (marathon 12d's 200% applies at Phase 6,
NOT here — cap untouched this batch). Coding starts after QA-M.
