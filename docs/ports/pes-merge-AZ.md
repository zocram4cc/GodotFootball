# Batch AZ merge

Lane `MergeBatchAZ` = `origin/main` + `CutsceneTiming` (`ff362f40`).

## What lands (CameraKeeper)

- **Goal cutscene targets:** `GfMatch::cutscene_actor` returns the CAMERA_TARGET slot list: the scorer (0), the conceding keeper (21, the other side's roster slot 0, checked on captures by `tools/dev/cutscene/test_goal_cast.py`) and the referee (22). `match.gd` feeds each slot. Step 1 now frames the conceding keeper (frames examined).
- **Restart hold:** `Match::HoldRestartForScene` holds the goal restart while the goal timeline plays, so the kick-off placement no longer fires under the scene; the 44 m jump at step 2 is gone (frames examined). `pes_goal_scene_hold_test`, red first, with mutations.

## Gate

`GATE fast: GREEN`, 5,226 s.
- `gen_translated --check` 4837 byte-identical.
- ctest all passed.
- The recorded re-check selected 0 rows: no translated or harness file changed.
- No unrecorded status regression; ledger current.
- Smokes unchanged: `3b73c968b4a377e0` / `0e480b158cf80bd4`. `gfsim_run` never raises the hold.

## Open

- Slot 23 (assistant referee) is not fed.
- The team-mate/opponent slot permutation (CameraSelect continues it).
- The layer pick at tick 480.
