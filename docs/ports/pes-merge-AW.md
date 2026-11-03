# Batch AW merge

Lane `MergeBatchAW` = `origin/main` + `CutsceneTiming` (`181523c1`).

## What lands

- **ProcCamera3's translator fix:** xlate implements `PMOVSXBD`, `PMULLD`, `PUNPCKLBW/WD`, `CVTDQ2PD`, `MAXSD/MINSD` and `BSWAP`. Before, they were silent no-ops in 15 layer files. Any other unknown instruction now traps, and `gen_translated` refuses the no-op marker.
- **Goal cutscene timing:** the goal gate `0x140472640` and the foul gate `0x14047EB00` are translated. PES's goal scene list comes from the builder `0x140477BC0` on GF state. The approach-cap fix keeps the cap at 2800 ms for headless runs and uses 6000 ms only for the run to PES's celebration spot (CutsceneGate).
- **Procedural camera:**
  - The FixDemo constructor chain and engine `0x141F7BFE0` are proven on 18 live captures (closure 272).
  - The CAMERA_TARGET registry is a native service.
  - The PES→Godot pose conversion is pinned by tests.
  - The camera engine runs through all three steps of the seed-7 goal; step 0 shows PES's telephoto on a player (ProcCamera7).
- **Link rule:** `gen_translated.py` links any callee recorded bit-exact in any results file (ProcCamera7).
- **Reports:** CutsceneTiming4-7, ProcCamera..ProcCamera8, CutsceneGate.

## Gate

- **First run, on CutsceneTiming `ebfdef4c`:** RED on `ProcCamera.AnUncapturedRecordFailsTheFrameNotTheProcess` only. The test expected a failed camera frame, but ProcCamera6 had proven that preset, so the engine returned a real pose (fov 0.0738). The branch head replaces the test with `AnUnrecordedPresetFailsTheFrameNotTheProcess`. Its merge (`181523c1`) is the fix.
- **Second run:** `gen_translated.py --check` 4837 functions (3719 recorded bit-exact, 1118 closure) byte-identical; ledger current (38076 rows). **GATE fast: GREEN.**
  - ctest: 2277 of 2277 passed.
  - Recorded re-check: every row in every set re-checked (245 + 685 + 546 + 2253 + 34).
  - No unrecorded status regression (18595 rows compared).
  - Off-path ports: 125, all allow-listed.
- **Smokes** (seed 7, 2 min): switch-off `3b73c968b4a377e0`, pes-loop `0e480b158cf80bd4`, both unchanged from main. The 20-minute match ran 43824 sim ticks to full time.
