# PES 2021 goal kick on pes_loop: the keeper's lofted pass, launch solved on PES's ball flight (GoalKickPort)

Branch `GoalKickPort`, commit `8961feb2473471348d7913a564af02163bdbc63d` (ls-remote confirmed). Base `origin/main` `c43fb22`.

## 0. Verdict

- The PES goal kick is the keeper's **lofted pass**. The ground option is unreachable in the goal-kick think (§2). GF's 0.4 coin (high pass to a random midfield point, or ground pass) is replaced on pes_loop by a lofted pass to the picked mate.
- The launch was overshooting by about 25 m because GF's HighPass power (`AI_GetAutoPass`, 1.04 at 56 m) is not PES's. On pes_loop the power is now solved so the ball, flown by PES's own ball integrator, comes down through the 1.5 m receiving height at the receiver's distance (§5).
- The receiver picker, the receiver-less target and the exact PES solver are not fully decoded. They are stand-ins with named `ponytail:` comments (§6).
- Off path is unchanged: `[determinism] 96ce33d3b51d4199`, 108183 ticks.
- pes_loop seed 7, 20 min: full time at 154566 ticks (cap 194400), rerun identical (`4d051f9d906b9b07`). Main: cap at 194400 with the goal-kick loop.

## 1. Who takes it (CONFIRMED structure; DOC for the setplay table)

- Taker = the keeper: the slot with role 0 (`T+0xc48+0x3f8c+4·slot` == 0), first on-pitch entry of `T+0x8928[]`. Source: `pes-re-setpiece-kicker.md` §1.2 (OBS decompile of `FUN_1404a7d60`, MODE 3). Live: every goal kick of the sampled match was taken by the keeper (`pes-re-setpiece-kicker.md` §1.2 "all goal kicks to the keeper").
- Restart programme: judge J1 end-line → restart 3 (goal kick) when `dir_T·ball.x ≥ 0`; builder `FUN_14044fb10` (CONFIRMED in `pes-re-A5Judge.md` §4.3); taker release is counter-gated (OPEN-6 in that doc). Not changed here; `referee.cpp` restart paths belong to A5Tail and were not touched.
- GF already hands the goal kick to the keeper on pes_loop (`teamAIcontroller.cpp:1538`).

## 2. The choice: lofted always (DOC for the think table; reasoning CONFIRMED from that table)

ThinkUnitSetPlay, vtable slot 2 = `0x142135c50`, MODE 3, per frame for the kicker (`pes-re-setpiece-choreography.md` §9.2 and `pes-re-setpiece-kicker.md` §2.4, OBS decompile by an earlier pass). `elapsed = MI+0x1198 − ctx[+8]` in frames; `r` = int fps at `0x14351ae50` (static 54).

```
0: mark; no eligible team-mates -> 5; else 1
1: (uint)(fps+0.5) <= elapsed  -> mark, 2          ; (uint)(54.5) = 54 frames
2: elapsed < (uint)(r*0.5+0.5) -> emit ground pass FUN_142132660 (short)
   else MI+0x14c4==1 && tactics[+0x18] <= 0.3 -> mark, 3
   else -> 4
3: online -> return; MI+0x14c4==1 -> cmd 0xb; elapsed < (uint)(r*3+0.5) -> return; else mark, 4
4: elapsed >= (uint)(r*0.5+0.5) -> mark, 5; else t = FUN_142135230 (lofted); t <= 0x15 -> cmd 2, player t
5: cmd 2, player 0xff, arg 1 (lofted into space)
```

- **CONFIRMED (arithmetic):** state 1 holds until `elapsed ≥ 54`, so state 2 always runs with `elapsed ≥ 54`. The short window `elapsed < (uint)(r·0.5+0.5) = 27` (0.5 at `0x142565600`) is therefore never true on the goal-kick path. The ground kick is unreachable from the think.
- **DOC:** `MI+0x14c4` is 0 on a normal goal kick (`pes-re-defensive-shape`/`pes-re-defence-leftovers`: MODE 3 with `+0x14c4 == 1` is the special case). So state 4 is the default route: lofted.
- Result: the goal kick is `cmd 2` type 1 (lofted) to `t` from `FUN_142135230`, or lofted into space (`0xff`).
- INF: the keeper's think answers GKPassShort/GKPassLong (`pes-re-goalkeeper.md` §11.6) govern the in-play distribution after a catch, not the goal kick. The one PES live goal-kick launch on record is KP_GOALKICK (63, lofted); no PES live goal kick with a ground pass is on record.
- **Not re-read this session:** `0x142135c50` has no Ghidra function (`No function found`), so the table above is the earlier pass's decompile.

## 3. The lofted picker `FUN_142135230` (CONFIRMED decompile this session; stand-in in GF)

`uint FUN_142135230(undefined8, longlong *param_2)` (2133 B):
- List: `FUN_140a714e0(team, side)` returns `team+0x22b9c` if the side flag is 1, else `team+0x229b4` (CONFIRMED).
- Group: `FUN_142124f90(list, g)` = `byte[list+0xb0+g]` for `g < 2` (count), else 0 (CONFIRMED). Groups are 22 entries (`0x16`) each: first candidate `list + g·0x16`.
- Candidate 0: if it is the kicker → `0xff`; else rating `r0 = byte(playerdata+0x31)/100` (`0x14253ee30` = 100).
- Loop over the group's other entries: if `mate.x·dir < first.x·dir − 5` (`0x14259222c`) → return the best so far (line test); otherwise a strictly higher rating replaces the best.
- Constants: 22 (`0x16`) is the picker's valid-index bound (`uVar14 < 0x16`); 21 (`0x15`) is the think's `t <= 0x15` player bound; `+0xb0` count offset, `+0x31` rating byte, `+0x55c` player x (INF from the field use), 5.0 (`0x14259222c`), 100 (`0x14253ee30`).
- **Stand-in in GF (ponytail):** `PickGoalKickReceiver` takes the best GF `technical_highpass` stat among active mates other than the taker. No group order, no 5 m line test. The PES `+0x31` byte is not mapped to a GF stat (INF).

## 4. The target

- PES request at contact−1 for a goal kick: target point `ap+0x2bf4` = a teammate spot about 45 m away, receive height `ap+0x2bf8 = 1.5 m` (OBS, one live sample, `pes-re-setpiece-kicker.md` §4). The 1.5 m writer is not decoded (OPEN). Constant `kLoftedReceiveHeight_m = 1.5` carries that provenance; there is no install VA.
- Port: the receiver's ground position is the target. The flight must come down through 1.5 m there.
- Receiver-less case (player 0xff): no decoded target (OPEN). Stand-in: the midfield point on the attacking side at the old x (`pitchHalfW · −side · 0.2`) with the taker's y. `ponytail:` in `elizacontroller.cpp`.

## 5. The power: solved on PES's ball flight (CONFIRMED pieces; solver OPEN)

- The request power `ap+0x2bd0` is 0 at contact−1 on non-cross kinds (OBS, `pes-re-setpiece-kicker.md` §4). The speed is therefore not in the request; it is produced by the kick writer.
- The kick writer `FUN_1407324b0` (CONFIRMED decompile this session) applies the execution error: direction and speed-factor scatter from the per-player LCG (`pes-re-kick-launch.md` §2; GF already ports this as `ApplyPesKickError`, `peskickerror.*`). It takes the launch vector as input (`r8`, `r9`), so it does not compute the speed.
- The predictor that computes the launch vector is `FUN_140731d80` (doc name, `pes-re-kick-launch.md` §0.7). Ghidra has no function at that address and the caller `0x140731ffb` is not decompiled. **Solver: OPEN.**
- Port (`pesgoalkick.cpp`): fly `BallTick` (PES's own integrator, `pesballphysics.hpp`) from the ball spot along the GF HighPass direction (`AI_GetAutoPass`, elevation rule kept), and bisect the speed in `[0, 60]` m/s (40 steps) until the ball first falls through 1.5 m at the receiver's horizontal distance. `ponytail:` the search bound is GF's, and the solver is not PES's.
- Touch: `humanoid.cpp` builds `touchVec = dir · 36 · (power + 0.3)` (GF convention, not PES). `PowerForLaunchSpeed` inverts it. The 36 and 0.3 are named constants now shared by both files.
- Probe (temporary, removed before commit), install ball constants, test elevation: solved speed at 18.6° / 20°:
  - 20 m: 22.6 / 21.9 m/s
  - 30 m: 26.1 / 25.6 m/s
  - 40 m: 29.1 / 28.5 m/s
  - 45 m: 30.9 / 29.8 m/s
  - 56 m: 35.8 / 34.3 m/s
- Cross-check against the single live PES sample (45 m, 26.19 m/s at 19.8°): the solver gives about 14% more speed (INF: receive reference, the sample's `45 m` frame, or PES's extra drag/curl). **OPEN.** Pinned by no test (not a PES value).
- GF's old power at 56 m was about 1.04 (`AI_GetAutoPass`); the new one at 56 m is about 0.69.

## 6. Install constants used (VA, verdict)

| name | value | VA | verdict |
|---|---|---|---|
| gravity | 9.80665 | `0x1425a9678` | CONFIRMED (`pesball.hpp`, loaded by `LoadPesBallConstants`) |
| ball radius | 0.10868593 | `0x142598958` | CONFIRMED (loaded) |
| tick | 54 Hz | `0x14259d624`, int fps `0x14351ae50` | CONFIRMED (`pesball.hpp`); fps static 54 (DOC) |
| 0.5 | 0.5 | `0x142565600` | CONFIRMED (think gate) |
| 3.0 | 3.0 | `0x14258c264` | DOC (the think's `r·3`; not re-read) |
| 5.0 | 5.0 | `0x14259222c` | CONFIRMED (picker line, `FUN_142135230` read) |
| 100 | 100 | `0x14253ee30` | CONFIRMED (picker rating divisor, `FUN_142135230` read) |
| 1.5 m receive | 1.5 | none | OBS only (writer OPEN) |
| 36, 0.3 | GF | `humanoid.cpp` literal | GF convention, not PES (ponytail) |
| 60 m/s, 40 steps | GF | none | GF search bounds (ponytail) |
| 22, 21, `+0xb0`, `+0x31`, `+0x55c`, `+0x22b9c`, `+0x229b4` | structure | in `FUN_142135230`/`FUN_142124f90`/`FUN_140a714e0` | CONFIRMED (read) / `+0x55c` INF |

## 7. Correction to the style-effects doc

`pes-re-style-effects.md` (and `pes-re-playing-styles.md` R6b) label `FUN_1409DD470` (bits 19/20) "keeper distribution". The decompile (this session) returns a **position** (`*local_res10 = (x, z)`, `return local_res10`) and uses bit 20 for ×0.25 / +1.1 / ×0.6 factors on the ball distance: a keeper positioning term, not a distribution choice. The goal kick does not read it. **CONFIRMED** (decompile), interpretation INF(high).

## 8. Changed code

- `src/sim/onthepitch/pesgoalkick.{hpp,cpp}` (new): `GoalKickAfterWait`, `PickGoalKickReceiver`, `PowerForLaunchSpeed`, `LoftedFlightDistance`, `LoftedKickSpeed`; constants `kPassTouchSpeedPerPower_mps`, `kPassTouchPowerOffset`, `kLoftedReceiveHeight_m`, `kLoftedSearchTop_mps`.
- `src/sim/onthepitch/player/controller/elizacontroller.cpp`: pes_loop goal-kick branch (receiver target, forced receiver, space stand-in) and the solved power override after `AI_GetPass`. GF coin is on the off path (unchanged).
- `src/sim/onthepitch/player/humanoid/humanoid.cpp`: touch speed uses the shared constants (same numbers, no behaviour change off path).
- `tests/sim/pes_goal_kick_test.cpp` (new), `tests/sim/CMakeLists.txt`.

## 9. Tests (install-backed, `PES21_ROOT=<workdrive>/PES21`)

`build/tests/sim/pes_goal_kick_test`: **8/8 pass** at the commit (no probe left in the file; worktree clean).

- Choice: lofted with or without a receiver.
- Picker: best rating excluding the taker; alone → −1; empty → −1.
- Touch: `36·(power + 0.3)` returns the solved speed.
- Solve: the speed re-flown from the same spot lands at 20/30/45 m within 0.25 m; farther needs faster.
- Flight: lower receiving height is reached further along (3.0 m < 1.5 m < 0.5 m distances).
- Drag: the flight stays below the drag-free parabola at the receiving height.

Mutations, each applied with the edit tool, rebuilt, run, then reverted (all reverted; final rerun 8/8):

| id | mutation | result |
|---|---|---|
| M1 | picker does not exclude the taker | caught: 2 tests |
| M2 | bisection comparator inverted | caught: 2 tests |
| M3 | crossing ignores receiving height (landing used) | caught: 1 test (height ordering) |
| M4 | touch power offset dropped | caught: 1 test |
| M5 | choice inverted (receiver → space) | caught: 1 test |
| M6 | GF→Fox lateral sign flipped | **not caught**: equivalent. Mirror in the lateral axis leaves the horizontal distance unchanged; output is distance only |
| M7 | height axis not mapped (gravity on lateral) | caught: 3 tests |

## 10. Smoke (run by the owner's request, 20 min; no gf-sim.lock taken)

- `gfsim_run --seed 7 --minutes 20 --pes-loop`: `[determinism] 4d051f9d906b9b07`, `154566 sim ticks to full time (cap 194400)`, exit 0. Rerun identical (same hash, same ticks). 70 goal-kick preps (H1 42, A1 28). Restarts 69 stoppages, longest 23.1 s. Idle 151 episodes, stalls 62 (not a target).
- `gfsim_run --seed 7 --minutes 20` (off path): `[determinism] 96ce33d3b51d4199`, `108183 sim ticks to full time`. Matches main.
- Gate: not run (Main gates).

## 11. Open items

1. Receiver: the full `FUN_142135230` order (group list, 5 m line test) and the `+0x31` rating's GF stat. Stand-in: best GF `technical_highpass`.
2. Receiver-less kick (player 0xff): target not decoded. Stand-in: midfield point at the old x.
3. Solver: `FUN_140731d80` not in Ghidra; the solved speed is about 14% above the one live sample (45 m, 26.19 m/s). Needs a live comparison at a logged goal kick with receiver position and launch.
4. Receive height 1.5 m: writer not decoded (OBS only).
5. Elevation: GF's HighPass rule kept (about 19–20° at 45 m, the live sample's angle).
6. The touch pipeline still applies `GetBestPossibleTouch`, the curl and `ShapePassTouch` after the solve (fiddling is on). Their effect on the landing is not measured.
7. `FUN_142135c50` (think) not re-read: the state table is the earlier pass's decompile. Ground-branch arithmetic is CONFIRMED from that table only.
8. Idle stalls (151 episodes, 62 stalls) remain on pes_loop; not in scope.
