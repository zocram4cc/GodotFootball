# Batch BA merge

Lane `MergeBatchBA` = `origin/main` + `FoulSanction` (`63b19094`), then `origin/main` again.

## What lands (CardWhistle)

- **Stoppage words:** a foul stop's stoppage words come from the translated whistle `0x14050E770`, called by the translated commit `0x143F546F0`. GF's stand-in writes are deleted.
- **Commit proofs:** kinds 1, 3, 4 and the `G+0x400` arm are proven bit-exact, 27/27 blocks, on 19 synthetic states (labelled) plus the 11 real calls.
- **Match-level test:** it drives seed 7 to a foul stop and reads the booking (a plain level-1 foul).

## Gate

`GATE fast` on the lane (slots 1/0, 8,120 s): everything green except two GPU tests, `godot_post_compose_test` and `godot_post_meter_test`. Both hit ***Timeout at 180 s under load 30–60, with five gates and the PES rig sharing the machine.
- **The rest:** `gen_translated --check` 4842 byte-identical; ctest otherwise all passed; no unrecorded status regression; ledger current.
- **Smokes unchanged:** `3b73c968b4a377e0` / `0e480b158cf80bd4`.
- **The two GPU tests:** rerun alone after the main merge, they pass (1.1 s and 0.9 s).

While they were rerunning under that load, the amdgpu driver hung (SDMA ring timeout, failed GPU reset) and the machine rebooted.

Main `fef92e87` now holds both GPU tests under one machine-wide lock (`gf-gpu.lock`), with a 900 s timeout that covers waiting for it. CardWhistle's change is sim-side and touches no rendering code.
