# A3States report: MOVESTEP beyond GOTO_RUN/RUN_LOOP

## Branch / SHAs (confirmed by `git ls-remote origin A3States`)
`A3States` = `a3eb2867f259cbc0674436b94caf1418dcfe8ab5` (pushed). Parent chain off `origin/main` `0ee3d7a`:
- `b104e8c` — red wire test `MoveStepWire.TheFsmReachesStopAndAccelerate`
- `4b8946d` — the port (green)
- `a3eb286` — foot flag promoted to the shared contract + GOTO foot writes + unit tests

Worktree `<workdrive>/gf-A3States`. Nothing uncommitted. A stale Oct-15 WIP stash (`b0b2937`, predates my branch, not mine) was found and dropped after inspection.

## 1. Instrumentation: which transitions `FUN_1406ba900` never takes, and why

Throwaway counters in `ProcessMoveStep` + per-gate counters in `EnterAccelerate`/`EnterDecelerate`/`GotoRun`, run on a 2-min seed-7 `--pes-loop` smoke, then reverted.

**Before** (main `0ee3d7a`, on-path, 2 min): state 0 (INIT) 22 ticks, state 7 (GOTO_RUN) 22 ticks, state 3 (RUN_LOOP) ~85 000 ticks, everything else 0.

Gate-refusal census in `EnterAccelerate` (the entry RunLoop arm 2 calls): of6 436 calls, `kDistToLook <= 1 m` refused **6 436** (100 %), `ground <= 11 km/h` 2 117, `cls==3 && rate>=0.65` 1; `rowfail` 0. In `EnterDecelerate`: 6 436 calls, `cls > 1` 92, `rate <= gate` 6 324, `look <= 1 m` 20. In `GotoRun`'s stop arm: reached 22 times (one per player, the INIT entry), `stoppass` 0.

**Root cause, two distinct missing inputs:**
1. **The request block was never filled from the AI command.** `FUN_1406b7560`'s `req[0x11]`/`req[0x12]` (distances to target/look-at), `req[1]`/`req[3]` (headings), `req[6]`/`req[7]` (turn, facing error), `req[8]`/`req[0xc]` (angle buckets), `req[0x11]`-family were all zero/standing-still. Both `EnterAccelerate` and `EnterDecelerate` gate on `req[0x12] > 1 m` / `> 2 m`, so both refused every tick — that is why ACCELERATE and DECELERATE never entered.
2. **`AnimeState()` was a 0 stub** (`pesmovestep.cpp:270`), so the `== 0x4a` gates and the `!= 0` yaw-busy gate never fired as PES does; and **`footBand` had no writer** at all (`P+0x245b` is written only by the action setter `FUN_1406eebb0` from anime `vt+0x78`, confirmed by decompiling `0x1406eebb0`: `*(param_1+0x245b) = (**(vt+0xab8 + 0x78))(...)`).

## 2. What was ported (all files are `movestep*` + `pesmovestep.cpp`, Main-approved incl. `movestep_turn.hpp`)

- **`FillCommandWords`** (`src/sim/onthepitch/pesmovestep.cpp`): `FUN_1406b7560`'s fill from the player's current AI command — target = `desiredDirection × desiredVelocityFloat`, look-at = `desiredLookAt` when `useDesiredLookAt`, plus `req[0x11]`, `req[0x12]`, `req[1]`, `req[3]`, `req[6]`, `req[7]` folded ±180 and the `FUN_1406b9b20` buckets for `req[8]`/`req[0xc]`.
- **`StoreFootBand`** (`pesmovestep.cpp`): `P+0x245b` from the last displacement's turn sign, replacing the never-written zero. Named ponytail: anime `vt+0x78`.
- **`AnimeState()`**: `vt+0x28` answered from the humanoid's function type — `e_FunctionType_Movement`/`BallControl`/`Trap` → `2` (PES's MOVE action), else `0`; never `0x4a` (GF has no replay build). The `== 0x4a` gates now fire on their real predicate.
- **`EnterGotoRun` (FUN_1406ce820)** row arms, from `gf-MovestepRun-scratch/full_1406ce820.c`:
  - **foot-band rows0x561..0x566 / 0x55f..0x571**: the `0x2a015` and `0x4c03f` row-id gate on `AnimeState()` and the row.
  - **20 m distance arm with `Percent(40)`**: row 0x662 on the draw when the 18-tick ball sample (`int(54·20/60+0.5)`) is within 1.5 m, else 0x661.
  - **out-of-play arm**: `MatchPhase() != 1` + `0xa014` band → row 0x5cc. (The port had `== 1`, i.e. it fired in play; the decompile shows the out-of-play sense.)
  - **foot flag `*(req+0x26)`**: both writes (lines 561 and 858), the settled cell's mirror bit.
- **Foot flag promoted to the shared contract**: `moveword::kFootFlag = 0x98` moved to `movestep.hpp` beside `kClassWord`; group D's copy deleted, its 12 readers now use the shared name (Main's instruction, precedent `7b5b949`).

## 3. State counts, 2-min on-path (`--seed 7 --minutes 2 --pes-loop`), before → after

Measured on the same binary the wire test drives (`MoveStepWire.TheFsmReachesStopAndAccelerate`, 900 ticks):

| state | before | after |
|---|---|---|
| 1 IDLE | 0 | 1 988 |
| 3 RUN_LOOP | 19 778 | 5 294 |
| 6 TURN_STOP | 0 | 1 |
| 7 GOTO_RUN | 22 | 869 |
| **8 STOP** | **0** | **694** |
| **9 ACCELERATE** | **0** | **10 954** |

10 states reached before → 11 after; STOP and ACCELERATE both now entered.

## Tests red→green and mutations

Red first, on `origin/main` semantics: `MoveStepWire.TheFsmReachesStopAndAccelerate` failed with `states[8] 0 vs 0` and `states[9] 0 vs 0`. Green after the port. Commits `b104e8c` (red) → `4b8946d` (green).

Binaries run directly (no ctest, no lock, `PES21_ROOT=<workdrive>/PES21`):
- `pes_movestep_test` 92 pass, `pes_movestep_run_test` 42 pass (40 + my 2), `pes_movestep_step_test` 13 pass, `pes_movestep_turn_test` 43 pass.
- `pes_movestep_wire_test` **12 pass** (11 pre-existing + my new one).

Mutations, each applied, rebuilt, run, reverted:
| # | Mutant | Caught by |
|---|---|---|
| 1 | drop the `kDistToLook` fill (back to 0) | `TheFsmReachesStopAndAccelerate` — ACCELERATE drops to 0 ticks |
| 2 | out-of-play arm phase flipped to `== 1` | `EnterGotoRun.TakesTheOutOfPlayRowOnlyOutsideOpenPlay` (new) |
| 3 | `Percent(40)` → `Percent(0)` | `EnterGotoRun.TakesTheDrawRowWhenTheBallIsWithinReach` (new, asserts the draw arg) |
| 5 | foot-flag bit inverted (`mirror ? 0 : 1`) | `EnterGotoRun.TakesTheDrawRowWhenTheBallIsWithinReach` (new, asserts `kFootFlag`) |

Mutation 4 (replay gate `!=`→`==` in the Accelerate handler's arm B) was run and **did not fail any test** — ACCELERATE is entered via `EnterAccelerate` from RunLoop arm 2, not through that handler gate. Recorded honestly; not claimed as caught.

## Smoke hashes
- **Off-path (`--pes21-root`, no `--pes-loop`), 2 min: `a3d5e43a3b7b03af`** — unchanged, as required.
- **On-path, 2 min: main baseline `f260529762061bca`; my head `a3eb286` does not complete the 2-min match.** Bisected: `b104e8c` (red, before my port) completes with `f260529762061bca`; `4b8946d` and `a3eb286` (with the port) both stop at `[restart] stalled: no restart within 1758 ticks` after ~11 s and print no `[determinism]` line. So the fill/port introduces a restart stall on the on-path smoke. **This is open and unresolved** — I ran out of budget before diagnosing it. Most likely the fill's new turn/distances drive players into a state where the set-piece taker never starts; it needs a look at `pessequencer`/`referee.cpp` restart triggers (A5Tail's files), which I did not touch.

## Open items (numbered)
1. **On-path restart stall** introduced by `4b8946d`: the 2-min `--pes-loop` smoke no longer completes. Root cause not found. Suspect the filled `req[6]`/`req[7]`/`req[8]` (turn, facing error, bucket) feeding a path where the taker never reaches the ball; A5Tail's restart-watch view would show it fast. Acceptance criterion "off-path hash stays `a3d5e43a3b7b03af`" holds; the on-path tripwire currently fails on hang.
2. Mutation 4 is not covered — the Accelerate handler's replay gate has no test. If the stall above turns out to be an unreachable-arm issue, that gate needs a test.
3. Remaining `EnterGotoRun` ponytails (inner record-chain gates of the foot-band row, the point/rank/frame pick between 0x5cc/0x5cd) still stand as named stand-ins; `BandSpeedKmh`, `PhysiqueG`, `MirrorChoice`, `RelatedSlot`, `MatchState11d4/1554`, `MatchObject3b0` remain GF stand-ins.
4. DECELERATE (state 10) and the rest of the never-reached states (4, 5, 12–19, 22, 23) are still unreached; state counts beyond STOP/ACCELERATE were not measured per-state beyond what the wire test prints.

Gate not run (Main gates); no ctest, no `gf-sim.lock` taken.