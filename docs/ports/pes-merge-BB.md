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

## Attribution (StoppageAblation, 04-11-26)

One Release binary on `7636546b` carried temporary env switches (`GF_ABLATE=A,B,...`, never committed); each row is the 2-minute seed-7 smoke through simslot, `PES_EXE` and `GF_IMPURE_BANKS` set. Controls: no switch gives the BB hashes (`399e180f39459c2b`, `38549be3476430fd`); every shared change reverted gives the pre-BB hashes (`3b73c968b4a377e0`, `0e480b158cf80bd4`), so no change outside the list below moves either hash. The `MovementDecision` fix is **not** a source on the switch-off path: the earlier [INFERENCE] was wrong.

|Switch|Reverts|Path|
|---|---|---|
|MDEC|TrapTick14's `MovementDecision` (keeper Deflect and not-held BallControl skipped)|both|
|PHYG|`PhysiqueG()` from the physique block, back to 1.0|both|
|TOPRATE|`TopRate`/`MoveTopRate` in PES's operation order and byte cast, back to the clamped form|both|
|DODGE|`BallDodgeEntryGate`, the head of FUN_140a070e0, in `_DodgeBallCandidate`|both|
|CLIPS|kTrap clip family = every Animetable motion, back to the five `gkcatch_*` clips|both (loaded in the clip set)|
|CTOR|`ConstructActorP`/`ConstructAnimPlugin` at match build|both (pes-loop aborts without them: the chain needs P)|
|GATE|`_ReceiveCandidate` drops the designation/presser gate|pes-loop only|
|TRAP|the translated trap chain (`WritePesTrapActors`, `RunTrapKind0`, `RunPesTrapTicks`, previous-position copy), back to the pre-BB `PickTrapClip` picker (code restored in the scratch binary)|pes-loop only|
|TOUCH|issued trap clips' contact frame (`PlayIssuedClip`)|pes-loop only|

### Switch-off (`3b73c968b4a377e0` -> `399e180f39459c2b`)

|Reverted|Hash|Ticks / stoppages|
|---|---|---|
|none (BB)|`399e180f39459c2b`|30213 / 27|
|MDEC, PHYG, TOPRATE, CLIPS or CTOR alone; PHYG+TOPRATE|`399e180f39459c2b`|30213 / 27|
|MDEC+PHYG+TOPRATE+CLIPS+CTOR (all but DODGE)|`399e180f39459c2b`|30213 / 27|
|DODGE alone|`2cafd1ed675e0d94`|33296 / 15|
|DODGE+MDEC, DODGE+PHYG, DODGE+CLIPS+CTOR|`2cafd1ed675e0d94`|33296 / 15|
|DODGE+TOPRATE (also DODGE+PHYG+TOPRATE, and all six)|`3b73c968b4a377e0`|30186 / 26|

**The switch-off move is the ball-dodge entry gate (DODGE).** Whatever else is reverted, the hash stays `399e180f39459c2b` while the gate is on, and it leaves that value only when the gate is off. The gate alone is not enough to restore `3b73...`: with it off, the TopRate arithmetic order (TOPRATE) also changes the run (`2cafd1ed...` -> `3b73...`), a sensitivity that exists only in the ungated world (TOPRATE alone never moves the gated one). The trap chain, the `MovementDecision` fix, PHYG, CLIPS and CTOR do nothing on this path.

### Pes-loop (`0e480b158cf80bd4` -> `38549be3476430fd`)

The pes-loop trajectory is chaotic: reverting any one of the changes below moves the hash to a hash of its own, so no single change "is" the move; the move is their sum.

|Reverted|Hash|Ticks / stoppages|
|---|---|---|
|none (BB)|`38549be3476430fd`|31196 / 15|
|TOPRATE alone|`38549be3476430fd`|31196 / 15|
|MDEC alone|`b7f0e19844d717f4`|26098 / 13|
|PHYG alone|`35bbf8e15dfc02f1`|30901 / 17|
|DODGE alone|`27b1a2236776549d`|56136 / 35|
|CLIPS alone|`fbd7e19586641f5e`|31987 / 17|
|GATE alone|`878d472cb8ab6be1`|28094 / 13|
|TRAP alone (trap chain off, pre-BB picker)|`a10952c6ae039482`|36993 / 28|
|TOUCH alone (contact frame)|`9df47315576feafb`|32220 / 19|
|GATE+TRAP+CLIPS+TOUCH (the trap path back to pre-BB)|`b52522003b8143b4`|30756 / 18|
|every switch except CTOR (and except TOPRATE)|`0e480b158cf80bd4`|43824 / 21|
|the same minus TOUCH (TRAP reverted: TOUCH is inert)|`0e480b158cf80bd4`|43824 / 21|
|the same minus one of TRAP / MDEC / DODGE / GATE / PHYG / CLIPS|`05b26fbd...` / `e9c71908...` / `277ff2ad...` / `5486132c...` / `dd19dc35...` / `7442937b...` (none is the pre-BB hash)|-|
|CTOR alone|aborts (`PES_DISPATCH_UNTRANSLATED`): the chain reads the P it builds|-|

Named: the pes-loop move needs **TRAP** (the translated trap chain replacing the picker), **GATE** (no designation gate on entry 3), **MDEC**, **PHYG**, **DODGE** and **CLIPS**, each necessary to return to `0e480b158cf80bd4` (leave-one-out) and each moving the hash alone. TOUCH moves the hash alone but only through the trap chain's issued clips (inert once TRAP is reverted). TOPRATE and CTOR are not sources here. Two changes, MDEC and PHYG, move the pes-loop hash but not the switch-off one.

Method: switches were read once per process from `GF_ABLATE` in a throwaway patch (not committed); the pre-BB trap picker was restored in the scratch binary for TRAP. Both control hashes reproduced.
