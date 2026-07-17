# Running Notes — QA-I (patient-veiling-tortoise)

> Append-only mid-batch log. New entry at every checkpoint (commit landed / sub-task
> verified / finding captured / spec call resolved / scope pivot). Consumed at doc-close
> (section pass under bulk-run R2) as the primary input for the held Work Log entry.

Pair file: `Plans & Specs/Batch Plans/patient-veiling-tortoise.md`
Convention: Main Plan §0 "Batch Plans + Running Notes layout" (exemplar
`federated-bouncing-cupcake.md`).

## 2026-07-17 — Group open (G3)

Seeded at G3 group approval. Greenfield overlay build; scout truths baked in the plan
(fully-synchronous message-thread load w/ three 30 ms sleeps -> overlay must pump paints;
black screen = window destroyed before teardown). Coding starts after QA-H.
