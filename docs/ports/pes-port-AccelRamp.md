# AccelRamp: MOVESTEP AccelerationRamp, RUN_LOOP jostle, ramp wiring

Branch `AccelRamp` (from `origin/A3Salvage` f9d0401), commit `9c7f9f6`, confirmed by `git ls-remote origin AccelRamp`. Not merged.

## Scope

- `AccelerationRamp` (FUN_1406cbf70) in `src/pes/movestep_ramps.cpp`: rev2 §4.2 stages 2 to 7.
- `RampsBeforeDispatch`: steps 4 and 5 of anime-select §1, before `Dispatch`. The ramp runs only in RUN_LOOP (3) and STEP_TO_RUN (13). The damping runs in every state.
- RUN_LOOP jostle override (FUN_1406c0170, rev2 §5.2) and the Arm 5 change gate and block (§5.1, §5.3).
- `TeamAi` seam (`src/pes/movestep.hpp`). The adapter answers it with named stand-ins (`src/sim/onthepitch/pesmovestep.cpp`).

## Red to green

- Red, `pes_movestep_test`: link failure on `AccelerationRamp` and `RampsBeforeDispatch` (feature missing).
- Red, `pes_movestep_run_test`: 5 `RunLoopJostle` tests failed with style 0 where 7, 0x10 or 6 was expected.
- Red, an intermediate build break: `RequestedRate` declared in `pes` clashed with the anonymous-namespace copy. Fixed as a pure move, before the red run.
- Green: `pes_movestep_test` 91 (76 + 15), `pes_movestep_run_test` 40 (35 + 5), `pes_movestep_turn_test` 42, `pes_movestep_step_test` 13. `PES21_ROOT=<workdrive>/PES21`.
- Adapter object `src/sim/CMakeFiles/gfsim.dir/onthepitch/pesmovestep.cpp.o` compiles. `gfsim` itself is not linked.

## Mutations (each run, rebuilt, reverted)

| # | Mutant | Caught by |
|---|---|---|
| 1 | drop keeper branch (a) | `TheSideKeeperAimsAtTheCeiling` |
| 2 | special-role step 0.04 to 0.03 (c) | `ASpecialRoleTakesTheReserveFactor` |
| 3 | latch step 0.03 to 0.05 (e) | `ALatchedOwnerTakesTheLatchFactor` |
| 4 | DA step 0.05 to 0.03 (d) | `AFreeOwnerTakesTheDribbleFactor` (and the cap-boundary test) |
| 5 | cap depth 0.2 to 0.3 | `AnOpponentRightAheadCapsTheTargetAtTheProximityFactor` |
| 6 | accelerating slope 1.9 to 1.4 | `ClimbsAtTheAccelerationSlopeAboveSixTenths` |
| 7 | jostle 112.5 boundary `<=` to `<` | `RunLoopJostle.ExactlyOneHundredTwelvePointFiveDegreesIsStyleSeven` |
| 8 | branch (b) own above 21 drops 0.93 | `FreeOwnerOutsideTheListAimsAtTheFreeFactor` |

## VAs and constants

- Install values read by VA (`.rdata` file offset = VA − 0x140000A00): 0.93 `0x1425ddf7c`, 0.05 `0x1425ddf40`, 0.88 `0x1425ddf70`, 0.03 `0x1425ddf38`, 0.04 `0x1425ddf3c`, 55 `0x14259be38`, 65 `0x14259be48`, −10 `0x14259be84`, 20 `0x14259be1c`, 80 `0x142596e6c`, −60 `0x1425c48fc`, 49 `0x14259d620`, 1.9 `0x1425e4a60`, 1.4 `0x14259d5d8`, 1.25 `0x1425ab128`, −3 `0x14259d654`, 10 `0x142588210`.
- τ = FUN_1406eab00: raw bytes from `PES2021.exe` at file offset 0x6EA100, disassembled with objdump. The Ghidra bridge has no function there.
- Base target = FUN_1406b9ff0 (`RequestedRate`). Its arguments in RUN_LOOP match the ramp's stage 2.

## Corrections found (not fixed here)

- `0x14259bdc4` is FLT_EPSILON (1.19e-7 on the install), not 0.001. The `RunConstants::rampFloor` comment is wrong; the loaded value is right.
- `0x142588210` is 10.0 on the install. `RunConstants::turnP2` and `MoveConstants::near01` comments say 0.1 for that VA. Both comments are wrong.
- Arm 5's representative motion used `band / 4`. §5.3 says `band`. The change block now uses `style * 4 + band`.
- Arm 5 wrote `req[0x4a]` with the requested rate. PES's ramp owns that word. The write is removed.

## Open

1. Stage 1 (phase-3 class side effect): the speed reads the position-history ring, whose writer is not decoded.
2. Closing cap after the step (rev2 §4.3): needs the partner and touch records.
3. RUN_LOOP entry gate (§5.1 steps 1 and 2, `FUN_1406b70d0`, dwell): not ported.
4. `req[0x20]`: written as 0, the word has no name yet.
5. Adapter stand-ins: no team roles, latch, tackle or set piece; DA and PC read 0; Acceleration from `physical_acceleration` × rec99 (unverified against PES); `PhysiqueG` 1.0. The NearestOpponent seam answers "none" until NearestList merges. NearestList's accessor is `pes::offball::NearestOpponents(RunWorld, own)` on branch NearestList 2a3050f; the adapter must call it on a `RunWorld`, and test `.empty()`.
6. Untested branches: the sub-state-2 decel (−3 d) and the phase-13 turn slow.
7. Not verified: a live frame or a live-PES comparison; the full gate and gfsim (Main gates).
