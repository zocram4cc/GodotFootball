# Net dodge: the gate reads the dodger (NetDodge)

Commit `6f72b26` on `refs/heads/NetDodge` (off `origin/main` 2f081e3). Worktree `gf-NetDodge`, scratch `gf-NetDodge-scratch`.

## Answer to the handover item 2

- **goalHalfWidth 3.7 vs PES.** The gate's post half-width is FieldInfo F[2] = 3.66 (`pes-re-fieldinfo.md` §1). It is loaded at `0x140a08013` (`movss xmm15,[rdi+0x8]`) from the FieldInfo pointer `[G+0x2aa8]` (`0x140a07eb2`). GF's 3.7 is not PES's value, but a 0.04 m change moves a bearing by about 1 degree. It cannot explain 9 versus 2,300.
- **The 12 m window.** PES has no such window. The dispatcher `FUN_140a07dd0` (table @`0x14263ecb0`) checks feature `FUN_140ca1a50(0x23)` (always true, `mov al,1`), then calls the gate at `0x140a07dfe` with no distance test and no team test. The gate's only x test is `|x| >= F[0] - 1` (`F[0]` loaded at `0x140a07ec5`, edge margin 1.0 at `0x14256ae1c`), applied at either end of the pitch.
- **The real cause of 9.** `FUN_140a07e40` never calls a ball prediction (raw call list `0x140a07e40`–`0x140a08340`). GF's predicate tested the ball's 200 ms prediction (`GetBallPrediction(200)`), added `into` and `|ball.y|` pre-checks, and used distance-to-ball as the step. PES's gate reads the dodger's own record: position `+0x55c/+0x564`, velocity `+0x574/+0x57c` (via `FUN_140a6c990` and `FUN_140458740`), and the goal marks.

## Evidence (VAs)

- Table `0x14263eca0`: 16-byte records `{fn, flags}`. Ball `0x140a070e0`, net `0x140a07dd0`, post `0x140a08340`, human `0x140a08ea0`.
- Callers: ball gate from `0x140a071a0`, post gate from `0x140a08350`, human gate from `0x140a08ebe`, net gate from `0x140a07dfe`.
- Net gate loads: F[0] `0x140a07ec5`, F[2] `0x140a08013`, F[14] `0x140a08019`. Anime slot test `0x140a07eef` (`FUN_1406b4c10`, slot 0x15).
- Constants: step floor `100 / (gs*72)` per frame, with `100 @0x14253ee30`, `72 @0x1425cb47c`, `gs = 54 @0x14351ae54`, so 100/72 m/s. Residual `15 @0x14259be0c`. Wrap `180/360 @0x142598580 / 0x14259858c / 0x142598598`.
- Bearing: `FUN_140439e50` = `atan2(dx, dz)` in degrees, wrapped to [0, 360), compared numerically.
- `DAT_14259bdd0 = 0.694444` is the look-ahead / stamina floor. In the net gate it only reaches the velocity branch (state 5). Not carried; see stand-ins.

## Done

- **Port.** `pesdodge.{hpp,cpp}`: `NetGoalMarks`, `NetGateInput` (dodger position and movement), `PesBearingDeg`, `NetDodgeGate` (edge, anime slot, speed floor, heading interval with inside and outside branches, 15 deg residual). `elizacontroller.cpp`: `_DodgeNetCandidate` reads only the dodger, with marks `{pitchHalfW + lineHalfW, goalInnerHalfWidth, pesNetFloorBackX}`. `gametypes.hpp`: `pesNetFloorBackX = 54.43359f` (F14, new, the only added constant).
- **Tests** (`tests/pes_dodge_test.cpp`, synthetic marks 50/1/52): eight `PesDodgeNetGate` tests, including the bearing-convention pin `ThePesBearingIsAtan2DxDzWithZeroAlongGfY`. One install relation test `TheNetGateConstantsAreTheExesOwn` (1.0, 15, 100, 72), which passes on the install.
- **Mutation checks, each reverted and md5-checked:** post width 3.7; behind point at 3.0 m; no outside branch; GF heading `atan2(y,x)`; sign flip in `PesBearingDeg`; old gs-divided floor; fixed +x goal; no residual; no edge margin. Each fails at least one test. The first eight were run on the first version; the two convention mutants on the final version.
- **Gate.** `ctest` on the final tree: 100% passed, 695 tests. The standing skips `VfsInstall.NestedOriginalsSurviveWithoutDownloads` and `EditSaveInstall.DecryptsAndMatchesTheRunningGame` are the only ones not run.
- **Measured, seed 7, 20 min:**

| | origin/main 2f081e3 | 6f72b26 |
|---|---|---|
| `[dodge] net` | 9-9 | 26-32 |
| `[pes-clips] played dodge` | 1162-1129 | 1198-1109 |
| `[pes-clips] tackle` | 4-6 | 10-2 |
| `[dodge] ball` | 4750-6463 | 4357-5334 |
| `[dodge] post` | 1365-782 | 2121-784 |
| `[dodge] human` | 1863-1720 | 1596-1591 |
| passes | 56-47 | 59-46 |
| forced | 11305 | 10756 |
| goals | 2-5 | 2-4 |
| determinism | 95515192750cd51b | 19a8569ee718dd6b |

  The baseline reproduces the handover exactly. The other dodges and trajectories shift because a net dodge pre-empts the reaction slot.
- **Rate breakdown** (temporary per-term counters, reverted; the instrumented run reproduces determinism `19a8569ee718dd6b`). Over 164,031 gate evaluations: the edge test rejects 163,283 (99.5%). Of the 748 that pass it, the speed floor rejects 416, the heading interval rejects 251 (inside 192, outside 59), and the residual rejects 19. That leaves 62 gate passes and 58 commands (26 home, 32 away). The 4-command gap is not split between selector and chooser.
- **Explanation.** The net dodge is set by geometry: the dodger must stand within 1.0 m of his goal line (`|x| >= 51.56`), which is 0.46% of evaluations. Half of those are slower than 1.39 m/s, and the heading must lie between the posts. Both teams fire because the gate has no team term.

## Not done / next

1. **Post predicate** `_DodgePostCandidate` has the same 12 m window, GF's `pitchHalfW` and `goalHalfWidth`, and reads the ball. PES's post gate `FUN_140a084d0` has a 1.0 m edge test in both axes (`0x14256ae1c`) and a 5.0 m distance test (`0x14259222c`), with no 12 m term. Next: port it like the net gate (dodger record, F0/F2 marks), drop the window, measure `[dodge] post`.
2. **Caller gating.** `_ReactionCommands` calls the net candidate for every in-play player. The PES chooser's caller (`FUN_140a06dd0` per `pes-re-defender-decisions.md` §1) is not decoded, so attackers dodge into the net too. Next: locate the loop that indexes the table at `0x14263eca0` (no direct xref; use a computed-index search), decode its team and engaging-defender gate, and port it into `_ReactionCommands`.
3. **FieldInfo pointer conflict.** `pes-re-fieldinfo.md` §2 says `*(G+0x2aa8)` is null. `FUN_140a07e40`, `FUN_14073c5d0` and `FUN_140a084d0` dereference it, so it is non-null in play. Next: a read-only live check during a match (`/proc/<pid>/mem`, sudo) that `*(G+0x2aa8)` = block+4 with F[0] = 52.56, then correct that report line.
4. **Stand-ins** (each has a `ponytail:` note). The exe's movement-state branch `+0x5b0` is replaced by the speed floor. The anime state byte `hu(i)+0x4c == 2` is treated as not set, so the outside branch always applies past the line. `animeSlot` is always true. `NetSelector` and `speed > 22` are unchanged.
5. **Docs.** `docs/re/pes-re-dodge-gates.md` (the net "Stand-ins" bullet and open item 2) is superseded by this report. Archive via `tools/dev/archive_re.sh`; agents do not edit `docs/re/`.
6. **Human gate.** PoseVeto's note (roster-slot term, newest-event window) is not touched here.
