# Batch BD: MotionClips (merged to main)

**Contents:** branch `MotionClips` (`c7874a3c`), merged onto main `0dd1c2b9` in `gf-MergeLane` as `MergeBatchBD`. Outside `docs/` and `tools/dev/`, the merged tree is identical to the gated tree. Report: `pes-port-MotionClips2.md`.

- **Zero-record fallback arms proven** (live captures, `snapdiff --bounded`, 0 mismatches): cancel `0x1406AF8E0` (21 calls), clip length `0x1406B10D0` (17), tick length `0x1406B11E0` (17). Recorded in `results_motion.json`.
- **Not proven, no captures:** blend `0x1406AF620`, RotData end `0x1406B1D00`, RotData offset `0x1406B1F00`. Their arms stay INF until a capture lands.
- **TurnStop terms** (commit `6ff18eb9`), with `RunAbilityByte` / `RunEntryAim` bound to the slot's own P.

## Gate

`GATE fast: GREEN` on `c7874a3c` (slots 1 0, main `0dd1c2b9` merged): ctest 2342/2342; ledger and off-path ports clean.

| Smoke (seed 7, 2 min) | Hash | Before |
|---|---|---|
| switch-off | `399e180f39459c2b` | unmoved |
| pes-loop | `85c7d7c515cf92ae` | `38549be3476430fd` |

**New pes-loop baseline: `85c7d7c515cf92ae`.**

### Ablation of the pes-loop move

- The ablation attributes the move to TurnStop commit `6ff18eb9` alone. The reader cutover, the DECELERATE divisor, AbilityByte, EntryAim and the tired-tank row are inert on this seed.
- Inside TurnStop, three terms each move the run: the class-word test `req[0x1F] < 3`, the midpoint stop test, and the idle-exit `elapsed < blend` guard.
- The toe-off lock, replay dwell and 0.85 divisor floor are inert here.
- Passes and shots are non-zero on both paths.

## Seeds re-staged by census

Census over seeds 1–12, serial, assertions untouched:

| List | Old | New |
|---|---|---|
| `kThrowInSeeds` | `{3,3,13,11}` | `{6,11,10,5}` |
| `kCeremonySeeds` | `{2,15,23}` | `{8,3}` |

## Left

- Capture `mot_blendrec_AF620`, `mot_rotend_6B1D00` and `mot_rotoff_6B1F00` with their record-absent predicates, then prove the three remaining arms.
- Mutate the `RunAbilityByte` / `RunEntryAim` binding (not done).
- `DribbleProposal` re-stages the same throw-in seed lists. Its census runs on main after this batch.
