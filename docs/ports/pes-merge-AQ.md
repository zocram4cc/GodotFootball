# Batch AQ merge

**Landed 31-10-26.** Lane `MergeBatchAQ` = main + LayerHeaders `2cc86e0d` + SwitchTables `8bc504dc` + ForcedClearReal `4563bd5a`.

## Contents

- **LayerHeaders:** the dispatch table is its own TU (`pes_dispatch_gen.cpp`). Adding one translated function rebuilds 2 objects instead of 4,076. A static test guards the include graph.
- **SwitchTables:**
  - The translator bakes switch jump tables.
  - A generation-time lint fails on any live read of a read-only image address that is neither baked nor bound.
  - `ro_tables.json` declares 45 table windows.
  - 298 reads in 137 functions that returned 0 in GF now return PES's bytes.
- **ForcedClearReal:** the Clear arm is proven on real captured states (6 of 12 danger-predicated `carrier_clear` captures). The walk on GF's window equals PES on 22 states.

## Gate: fast tier

| Check | Result |
|---|---|
| Layer on the merged tree | `gen_translated.py --check`: 4,346 functions regenerate byte-identical |
| ctest | 2,153 of 2,153 passed (15,496 s at load ~35) |
| Recorded re-check | all sets selected (runtime header changed): 245 + 685 + 546 + 2,253 + 34, all bit-exact |
| Ledger | current; no unrecorded status regression |
| Smokes (2 min, seed 7) | switch-off `e663d1e917d6c4f4`, pes-loop `980c54e27f93ced2`, both unchanged from batch AP |
| Result | `GATE fast: GREEN` (19,015 s) |

Final merge of main: docs, tasks and `hook_plan.py` only.
