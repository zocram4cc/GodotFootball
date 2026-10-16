# PES 2021 penalty keeper: dive direction, timing and the verdict

Result: Done for the keeper port and the verdict. The dive target is ported from PES. The shootout verdict is the physical outcome; the plan-equality test is removed and `SimMatch.ShootoutVerdictIsWhatTheBallDid` is green (see §8 follow-up). PES's dive timing gate is still open (§7).

Scope: the defending keeper in a penalty (set piece 6, gate `FUN_1409d5860`). Read-only RE of `PES2021.exe` (base 0x140000000, Ghidra bridge `pes_full.bin`) and the install's `.rdata`/`.data`. Tags: CONFIRMED (decompile or install bytes at the VA), INF (inferred), OPEN (not decoded).

## 0. VA discrepancy with the existing docs

`docs/re/pes-re-keeper-port.md` §1 cites `ActionKeeperDeflect::exec` at `0x1409d75d0` and its penalty variant `FUN_1409d78e0`. In the bridge both return "No function found at or containing address" (`0x1409d75d0`, `0x1409d75e0`, `0x1409d78e0`). The gate `FUN_1409d5860` does resolve, and its callers are `FUN_1409e5570` and `FUN_1409e4bd0`. The penalty branch that does the work is `FUN_1409e5600`, reached from `FUN_1409e5570` on the gate. Every VA below is one the bridge resolves. The doc's names are not used.

## 1. Gate (CONFIRMED)

`FUN_1409d5860` (0x1409d5860):
```
penalty := (MI[0x116c] == 3 && MI[0x1554] == 6) || ball.kind(FUN_140a3e190(ball,0)+0x1c) == 6
```
`MI` = `DAT_1436f3dc8 + 0x270`. `116c` = set-piece phase, `1554` = set-piece kind (6 = penalty).

## 2. Entry (CONFIRMED)

`FUN_1409e5570`: clears the "already staged" latch `p1[0x20]`, requires `FUN_140518fe0()` and `FUN_140a65db0(kind & 0x7f) == 3` (kind 3 = deflect), then `if (FUN_1409d5860(p2)) return FUN_1409e5600(...)`.

## 3. Penalty state machine: `FUN_1409e5600` (CONFIRMED)

State word `p1[8]` (0 idle, 2 stay, 3 armed, 4 choose, 5 deflect point set).

- **State 0, timing gate.** `rec = FUN_140a3e940(state, 0x11)` (the shot record; `rec[2]` = its time). `taker = FUN_140a48910(reg, MI[0x173c])`. The keeper holds his own position (`param3+0x878 = 2`, target `K.pos`) unless all of:
  - `MI[0x173c] <= 0x15` and `rec[0] != 0x37`;
  - `taker.role == '6'` (byte `+1`);
  - `MI[0x1198] - rec[2] > gs*0.5 + gs*0.5`, where `gs = FUN_141e532e0()` (`DAT_14351ae50`, §5);
  - `taker.u16[0x1c] - taker.u16[0x1a] >= gs*14/48` (INF: taker animation frames since the strike; OPEN which field).
- **Arming.** `p1[8] = 3`, `p1[0x28] = -1.0`, then the shot direction from the action payload `FUN_140a65a00`.
- **State 4** (`FUN_140590c90` chooses a save candidate; success → `FUN_1409e5af0`, fail → `FUN_1409e5e10`).
- **State 5** is the deflect point: `p1[0x10..0x18]`, `param3+0x8b0`, `param3+0xe8c` = the chosen point, `param3+0xe6c = 10`, `FUN_1406f0590(…, 4)`, `param3+0xfdc = 1`.

## 4. Dive direction and target (CONFIRMED)

`FUN_1409e5e10` (0x1409e5e10), the penalty deflect chooser:
```
a := penalty bearing (FUN_140458740) when the taker is role 'G' on team 0x766, else p1[0x24] when stored
if (80 <= a <= 100) || (260 <= a <= 280):          // goal axis, within 10 degrees
    first trajectory record i < int(gs*1.1) with |traj_i.xz - K.pos.xz|^2 < 12.25  -> target := traj_i
else target := K.pos                                // stay
```
`FUN_140458740` (0x140458740) = `FUN_140a6c6d0` vector → `FUN_140a92d40` bearing × 57.2957764, wrapped by +360 when negative. `FUN_140a6c6d0` (0x140a6c6d0) returns K's own movement (+0x574/+0x57c) when moving, else the heading vector `thr·(sin h, cos h)` (`h = K+0x554`, `FUN_140a8f7c0`), with `thr = 0.6944444 / gs`. Bearing is measured from +z, so 90° is the +x axis. So the dive target is the first predicted ball record within 3.5 m of him, and only while he is running or facing along the goal axis. Otherwise he stays.

## 5. Constants (install bytes, section table)

| VA | value | role |
|---|---|---|
| `0x14351ae50` (`.data`) | 54 (u32 0x36) | `gs`: logic frames/s (`FUN_141e532e0`); on-disk, not checked live |
| `0x14259d624` (`.rdata`) | 54.0 | tick rate (also `kPesTickHz`) |
| `0x14259bdd0` | 0.6944444 | penalty movement threshold ÷ gs (m/s) |
| `0x142596e6c` / `0x14253ee30` | 80.0 / 100.0 | goal-axis bearing window (low) |
| `0x142598584` / `0x142598588` | 260.0 / 280.0 | goal-axis bearing window (high) |
| `0x14259d610` | 12.25 | 3.5² m intercept radius |
| `0x1425a3590` | 1.1 | scan length factor (`int(gs·1.1)` = 59 records) |
| `0x14259be40` | 57.2957764 | degrees per radian |
| `0x1425795e4` | 6.2831855 | 2π (the 360° wrap) |

## 6. Verdict

The keeper holds until the taker's strike and an animation-frame gate pass (§3), then runs to the §4 target. Whether the ball is stopped is the existing contact model. So the verdict is the ball's, not the plan's (INF; consistent with `penaltyshootoutcontroller.cpp` design comment at ~372).

## 7. Open (OPEN)

- `FUN_140a3e940(state,0x11)` and `rec[2]`: the shot record and its time field. This sets the timing exactly.
- Which `taker.u16` pair is the animation frame (§3 gate).
- Who writes K's movement `+0x574/+0x57c` in a penalty (the keeper AI, which is what drives the bearing).
- `FUN_140a6cf70` trajectory list writer (250 slots, 5 per point).
- `FUN_140470eb0`, `FUN_1406f0590` bodies (deflect target write, anim request).

## 8. Port and result

Ported (GF, `src/sim/onthepitch/`):
- `peskeeper.hpp/.cpp`: `KeeperConstants` gains the §5 values, loaded from the install (`PenaltyDiveInput`, `PesPenaltyDiveTarget`: the §4 window, the 3.5 m intercept, the bearing of movement or heading).
- `player/controller/strategies/offtheball/goalie_default.cpp`: `PenaltyDiveSpot` builds the ball path (`int(gs·1.1)` records, from `GetBallPrediction`) and returns the target. `RequestInput` uses it while `IsKeepingKick`.
- `player/controller/elizacontroller.cpp`: the keeper's movement/deflect branch also runs during a live kick (it was gated on "in play and not in a set piece", and a kick is a set piece, so the defending keeper never ran it).
- `penaltyshootoutcontroller.hpp`: `IsKeepingKick(teamID)`.
- Not ported: the §3 timing gate (`ponytail:` comment in `PenaltyDiveSpot`).

Evidence (test binary `build/tests/sim/sim_match_test`, `PES21_ROOT=$PES21_ROOT`):
- Red on the unmodified logic (before): kicks 2 and 7 mismatch (`red-run1.log`).
- After the port: kicks 1, 2 and 4 mismatch (`mut-run.log` style runs, `green-try2.log`, `diag1.log`). The keeper now runs to the intercept (`DIAG` trace: keeper (49.89, 0.41) runs to (48.55, 0.64) = the ball's path) and deflects. The deflect's outgoing ball goes wide (`y=8.76`, `z=2.59`), which the classifier logs as MISSED.
- 20 of 20 runs of the single test fail (exit 1, same kicks), so the red state is deterministic.
- Mutation (`IsKeepingKick` returns false): the verdicts return to the baseline (kicks 2 and 7). The test is sensitive to the port.
- Full `sim_match_test`: the other eight `SimMatch` tests pass in two runs (`full-run1.log`, `full-run2.log`). One run in three crashed (exit 139, SIGSEGV) in `SimMatch.AIManagerSwitchGovernsTactics`, not reproduced in two reruns, under gdb with its predecessor, or alone. Cause not found; the change only affects shootouts and that test runs no shootout.

Verdict follow-up (item 2 resolved, the test is now the physical one):
1. The keeper's touch did register. Traced on seed 7 (kick 1): the engine's last-touch record named the keeper at 0.85 m, inside the 1.2 m latch. My earlier "2 m deflect point" explanation was wrong.
2. The miss came from `ObserveKick`: a keeper touch counted as a save only once the ball stopped or left the field. The deflected ball rolled past the 5 s kick timeout, and the timeout returned Miss. Fixed in `ObserveKick` (a settled kick, including the timeout, takes the touch as a save). The touch is now read from the engine's touch record, not the proximity latch.
3. The premise changed (Main ruling): the verdict is the physical outcome, not the pre-rolled plan. `SimMatch.ShootoutVerdictIsThePlannedOutcome` is removed; `SimMatch.ShootoutVerdictIsWhatTheBallDid` replaces it. See `docs/re/pes-re-shootout-verdict-mismatch.md`.

Not run: other test binaries that link `gfsim` (Main gates).
