---
description: Generate test-signal recommendations for a DSP module to validate behavior.
argument-hint: <module name — e.g., EQ8DSP, CompressorDSP, PitchCorrectorDSP>
---

Dispatch the `dsp-test-signal` agent with `$ARGUMENTS` as the module name.

The agent will read the module's source, identify validation modes (identity / freq response / transient / edge cases / stability), and produce a concrete test plan with input signals, param settings, expected outputs, tolerances, and how to run each test in the BaySickDAW UI.

Show the output. The owner runs the tests; I help interpret results.
