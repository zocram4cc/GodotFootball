# PressLane3 final: switch-table fix (d3d21026) and gfx-service link fix + prove_bounded --native (44ee6899) landed; tick 0x140928FC0 proven 3/3 on its real calls, record NOT in the layer (318 closure members have no spec)

Branch `PressLane`, tip `44ee6899` (pushed; `git ls-remote origin PressLane` confirms). Tree clean, no untracked files.

## 1. Landed: switch tables inside a function's extent are data (d3d21026)
- `pdata.py` records each resolved switch's byte map and dword table as data; the linear walk stops at them. Test `SwitchTableInTheExtent`: red before, green now. Old switch tests, extent, computed_ret, cond_tail_call, closure_walk green.
- Layer regenerated: exactly 18 functions change (2 recorded: `ai/pes_fn_1404F0E50`, `gameplay/pes_fn_140575510`; 16 closure). `gen_translated --check`: byte-identical, 10,966 functions. `check_pii.sh` rc 0 in the worktree and in `<workdrive>/GodotFootball`.
- NOT done: bit-exact re-proofs of the two recorded functions. Their captures are gone (pid42407 and pid281026 are absent from live2, the archive, and scratch; no index names their hooks). The removed text is only dead tail after an unconditional terminator with no retained jump into it, so executed paths should be unchanged, but that is an argument, not a proof.

## 2. Landed: gfx service inline and `prove_bounded --native` (44ee6899)
- Non-native harness builds (snapdiff, prove_bounded) link no `pes_native.cpp`, so closure member `0x1419DD0D0` failed to link on `pes_service_gfx_device_alloc`. Definition moved to `pes_stubs.hpp` as `inline` (rax = 0, same answer), removed from `pes_native.cpp`.
- `prove_bounded.py --native` passes `--native` to snapdiff.
- `gen --check` green on this tree (byte-identical, 10,966). PII scan rc 0 in both checkouts.

## 3. Tick 0x140928FC0: proven 3 of 3 on real calls; NOT committed to the layer
- The earlier "0x22B0000 is protection code" concern is resolved: a raw scan of seq3076 (`scan_raw.py`) finds 10 live `ff25 00000000` rendezvous patches, with trampolines at 0x22B0000 + k*0x100. 0x22B0000 is the trampoline of the proven press driver `0x1405141F0`. The 4-hook replay config restored only the tick's own hooks, so the driver's entry still jumped into its trampoline.
- Replay config with all 10 hooks: `press2/hooks_tick_replay10.cfg` (generated through `hooks_for_image`, no refusals).
- `prove_bounded 0x140928FC0 --native` over pid2282538 seq3076, seq5518, seq8188: `calls=3 replayed=3 mismatches=0 own_block 54/81 proven=True`.
- Why it is not in the layer: the merged closure (`closure_pttick.json`) has 652 members, and 318 have no spec under `specs/trapaction` (list: `press2/pttick_missing.txt`; e.g. `0x1421025B0`, a callee of member `0x142103A20`). `gen_translated.py` refuses the merged record ("calls ..., which is not committed"), so the record was reverted from the tree to keep the tip green.
- Saved for resume in `press2/pttick_record/`: `results_trapaction.json`, `closure_pttick.json`, `140928FC0.json` (root spec), `closure-pttick.txt` (set file), `universe.json` (row).

## 4. Next (exact)
1. Write the 318 missing specs: `closure.py --spec-set trapaction --tag pttick --members-file closure_pttick.json` (idempotent; only missing specs written). Not run in this window; its runtime is unknown.
2. Restore the five saved record files, run `gen_translated.py` (full regen) and `--check`, commit the record, layer, universe, and set file, push.
3. Step 3 (RunActionPress through the tick, delete the `_BallControlCommand` walk in the same commit, red test, PesStallQuick <= 53, ankle 9/9) waits on step 2. The tick is not in GF until it is in the committed layer.
4. Re-prove `1404F0E50` and `140575510` when fresh captures of either root exist.

## Scratch, disk, handoffs
- Unpacked capture copies deleted; `gf-PressLane-scratch` 408 MB (scripts, logs, `press2/`, saved record). Disk 282 GiB free on <workdrive> at the last check.
- Archive: my `move_launches.sh` was killed by Main before any verify. Live2 sources intact; one partial copy remains at `<media>/Dati III/gf-capture-archive/pid1270903` (reported; Main's to clean).

## Not run
PesStallQuick, ankle 9/9, ALiveBall, PesHang (no behaviour change in any commit this window).
