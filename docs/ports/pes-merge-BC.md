# Batch BC: AIConcepts proofs (merged to main)

**Contents:** branch `AIConcepts` (`8adb7d30`), merged in `gf-MergeLane` as `MergeBatchBC` (`02e89758`), then current main (`b9f34673`). Proof-only, with no GF behaviour change. Report: `pes-port-AIConcepts.md`.

- Concept selector `0x140579840` proven 15/15 (12 real in-concept calls).
- Six concept slot roots proven, mostly on synthetic cases, labelled as such.

## Conflicts

- `SETS` in `gen_translated.py`: resolved as the union (trapaction + aiconcepts).
- The regenerated layer is byte-identical to the merged inputs (5,266 functions).
- `LEDGER.md` was regenerated and is current (38,114 rows).

## Gate

`GATE fast: GREEN` on slots 3 2:
- 2,321 of 2,321 tests passed; ctest took 15,858 s.
- Recorded re-check: 245 + 685 + 546 + 2,253 + 34 functions selected, no failure.

| Smoke (seed 7, 2 min) | Hash | Main baseline |
|---|---|---|
| switch-off | `399e180f39459c2b` | `399e180f39459c2b` |
| pes-loop | `38549be3476430fd` | `38549be3476430fd` |

Neither hash moved, which fits a proof-only batch.

Main's commits after the gate (`a7d11fce`..`3f572b53`) touch only `docs/` and `tools/dev/` (rendezvous watcher, auto_slim, gate.sh slot fallback, hook_plan target). They do not change the product, so the verdict still holds on the merged tree.

## Left

From `pes-port-AIConcepts.md`, the GF cutover is blocked by:
- the T record being too small for the reads at `T+0xB6E8..`;
- the concept manager constructor `0x1404F0530`, which needs models of `QueryPerformanceCounter` and `__chkstk`;
- the lack of a registry object.
