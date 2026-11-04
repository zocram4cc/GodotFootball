# Batch AR merge

Lane `MergeBatchAR`, cut from `origin/main` after batch AQ and its report commits.

## Branches merged

|branch|head|what it brings|report|
|---|---|---|---|
|`VfsOrder`|`e3802b89`|GF's default mount is stock PES: `Data\` cpks patched by `download\` cpks, no sider roots or Lua modules; `--sider` opts in for `gfsim_run`/`pesfmt_dump`; a named gameplay mod without `--sider` is refused|`pes-port-VfsRoots.md`|
|`ToolFixes`|`9afdf163`|tooling fixes|`pes-port-ToolFixes2.md`|
|`DribbleProposal`|`9717947c`|dribble proposal translation|`pes-port-DribbleProposal5.md`|

All three merged without conflicts. `origin/main` was merged again before landing; that brought 9 commits of reports and hook-plan targets, with no conflicts.

## Checks

- `gen_translated.py --check`: 4456 functions (3697 recorded bit-exact, 759 closure) regenerate byte-identical. `0x14202BA30` stays held out because its closure reaches a mistranslated callee, `0x14202C250`.
- `ledger.py --check`: LEDGER.md is current (38065 rows).
- `GATE_TIER=fast tools/dev/gate.sh` (slots 1/0): **GATE fast: GREEN**.
  - ctest: 2180 of 2180 passed.
  - Recorded re-check: every row in every set was selected and re-checked: results 245, cutscene 685, presentation 546, core 2253, impure 34.
  - No unrecorded status regression (18584 rows compared, 160 scope moves recorded).
  - Off-path ports: 125, all allow-listed.
  - The 20-minute match ran 25942 sim ticks to full time.
- 2-minute smokes, seed 7, unchanged from main:
  - switch-off `e663d1e917d6c4f4`
  - pes-loop `980c54e27f93ced2`

## Not in this batch

- `FoulSanction` / `CardLevel2Cutover`: the fast gate was red on two ceremony throw-in tests. CardLevel7 has re-staged the seeds; its gate verdict is pending.
- `StoppageFlow`: `pes_stall_test` still aborts. The next untranslated call is `0x1406E85B0`, whose hook is armed.
- `CutsceneTiming`: carries the xlate silent-no-op fix (ProcCamera3). That fix rebuilds every translated TU, so it needs its own gate.
- `KickMotion`: deferred until StoppageFlow lands.
