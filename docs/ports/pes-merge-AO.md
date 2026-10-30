# Batch AO merge

**Landed 30-10-26.** Main before: `bbd5dc7c` + docs/tooling. Lane `MergeBatchAO`.

## Contents

- **StoppageFlow `fc84d383`:**
  - trap kind-0 chain proven (`0x14079A470` 7/7, commit `0x14079AFD0` 12/12);
  - SETcc-to-memory translator fix;
  - function extent rule in `pdata.py` (chained chunks, continuations, tail calls);
  - hold-table fix (free kicks 5.0 s at `0x14259CE64`);
  - designated owner.
- **FdcCamera `b0e4ce38`:** PES camera pose, with two camera-matched PES/Godot frame pairs.
- **AOFix `d321b4de`:** `driver.reached_roots`. A `jmp reg` switch-case label is not a callee; driver and snapdiff share one rule.
- **GateTier `672882cd`:** `GATE_TIER=fast|full` and the recorded-check selector `select_recorded.py`.
- Layer regenerated on the merged tree: 4,075 functions, drift check byte-identical.
- `.gitignore` adds `__pycache__/` and drops 14 committed `.pyc` files.
- Ledger regenerated.

## Gate: full tier (the first run of the batch; the fast tier did not exist yet)

### Full run on the pre-AOFix tree

- **Smokes passed:**
  - 20-min switch-off `7c131a7bc2051620`;
  - 20-min pes-loop `3f685b6040c6f0d7`;
  - both moved by StoppageFlow, which is allowed.
- **ctest:** 2,116 of 2,120 passed. Four failures:
  - `diff_pilot_recorded` (2) and `diff_core_recorded` (24): every case was `callee …: no instructions`, the switch-case defect AOFix fixes;
  - `diff_cutscene_recorded` and `diff_presentation_recorded`: timeouts under load ~35;
  - ledger stale.

### Rerun on the merged lane (AOFix + GateTier + main)

| Test | Result |
|---|---|
| `diff_core_recorded` | passed (12,179 s) |
| `diff_pilot_recorded` | passed |
| `diff_presentation_recorded` | passed (1,881 s) |
| `translate_select_recorded_test` | passed |
| `translate_closure_walk_test` | passed |
| `translate_extent_test` | passed |
| `diff_cutscene_recorded` | hit its 2,400 s ctest cap again under load; its driver command run directly: all 685 recorded functions bit-exact (2,855 s) |
| ledger | current |

### Final merge of main

Docs plus two live-rig scripts (`rv_watch.py`, `hook_plan.py`); no test or build target uses them.

## Follow-up

`diff_cutscene_recorded`'s ctest TIMEOUT (2,400 s) is below what it needs on a loaded machine (2,855 s). Raise it with the next batch.
