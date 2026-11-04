# Batch BB merge

Lane `MergeBatchBB` = `origin/main` + `StoppageFlow` (`9e461258`+), then `origin/main` again (report and hook-plan commits only).

## What lands (TrapTick6–14, StoppageFlowLand)

- **PES's trap chain, translated and running in GF:**
  - the action switch (31/31 proven);
  - the per-action table at P+0xC40, built by PES's own constructors;
  - the anime-interface slot `0x1406E72A0` (12/12), the displacement `0x1406E85B0` and the trap-tick callee `0x14084F140`;
  - the trap tick `0x140797CD0`, proven on 38 ticks including No-Mod start and mirrored ones (closure 350).
- **Translator fixes:** `pdata.py` decodes switch cases placed after a function's first `ret`; a `memset` model was added.
- **Loader data:** the pad-manager singleton, `constant::Ball`, R's 11 joint objects, and Mbinfo kinds 4, 9 and 12.
- **Stall fix:** `MovementDecision` skips ball actions, Deflect and BallControl for a man who doesn't hold the ball. A keeper's queued hand action no longer hides his walk to a ball at rest. `pes_stall_test` is green.
- **Issued trap clips** touch at their contact frame.
- **Seeds:** ceremony seeds re-staged by census on this tree; `PesRestartDelay.AReleasedThrowInCarriesItsBallInsideTheTouchline`'s timeout raised from measured runtime under load (the assertion is unchanged).

## Gate

`GATE fast: GREEN` (lane, slots 1/0, 16,520 s under load 15–68 with four other gates).
- `gen_translated --check`: 5002 functions byte-identical.
- ctest: all passed.
- Recorded re-check selected 624 rows (69 + 29 + 32 + 465 + 29), all still bit-exact.
- No unrecorded status regression (18622 rows); ledger current (38103 rows).

## Smoke hashes move

|path (seed 7, 2 min)|before (main)|after|
|---|---|---|
|switch-off|`3b73c968b4a377e0`|`399e180f39459c2b`|
|pes-loop|`0e480b158cf80bd4`|`38549be3476430fd`|

Both runs reach full time; restarts stay within the bound.

**Open:** an ablation attributing the move. The likely sources are the translated trap chain on the pes-loop path, and the `MovementDecision` fix, which is the only change shared with the switch-off path [INFERENCE]. StoppageFlowLand died in the 03-11-26 reboot before running it; it is assigned as a follow-up.
