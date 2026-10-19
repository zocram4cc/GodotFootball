# A4LaneInputs: psi, team level and the curve table replace three lane-unit stand-ins

Branch `A4LaneInputs` (worktree `<workdrive>/gf-A4LaneInputs`). Base `origin/A4Lanes3` `43c16ae`; merged `origin/main` `9d4daa1` as `67784d7`; port `b36a845`. `git ls-remote origin A4LaneInputs` = `b36a8452e812acc7a39bccfddb3200e747bdff42	refs/heads/A4LaneInputs`.
Files: `src/sim/onthepitch/pescarrier.{hpp,cpp}`, `src/sim/onthepitch/player/controller/elizacontroller.cpp` (carrier path), `tests/sim/pes_carrier_test.cpp`. Not touched: humanoid, referee, team, teamAIcontroller, pesoffball.

## What changed

1. **psi, OneTwo receiver P2** (gate `0x142124A83`, `CMPL $0,0x54(%RSP)`, `JE 0x142124AD8`; snapshot[0xD] = `T(side)+0x4BD4[r%11]`, store `0x14063CBEA/0x14063CC06`; lane-inputs §3, CONFIRMED). `TouchReceiver.psi` (bool) is replaced by `roleClass`; class 0 (keeper) is refused. The controller passes `CarrierRoleClass(mate)`: GF's formation role (GK 0, else 1), named as the stand-in until the RoleArrays array is on main. BackSpace and Safety mates take the same accessor (values unchanged).
2. **Team level, OneTouch G3** (reader `0x140A40BD0`: `*(char*)(p+0xA29C) != 0 ? 0 : *(int*)(p+0xA294)`; compare `0x140A60360`: `*p/100 < arg`; G3 passes at `level/100 >= 2`; lane-inputs §2.1-2.2, CONFIRMED against the bridge decompile this session). `TouchInput.teamLevel` is replaced by `teamLevelRaw` (level x 100) and `teamLevelZero`. G3 and G8 read `level/100`. The value is still a stand-in 0 (GF has no team level). Writers named at the field: `0x140A424E0` (+0xA294), `0x140A428B0` (+0xA29C). Source of the value: INF (lane-inputs §2.4). The mode read is unchanged (G4: mode in {3,4}, `0x141052E30 = *(M+8)`); its source stays a stand-in 2 (M+8 writer OPEN, lane-inputs §8.3).
3. **Curve table** (writer `0x142138220`, reader `0x142139650` (CurveEval), BackSpace gate `0x1421225E0`, one-touch G12 reads the same table). `BuildCurve`, `CurveEval` and `CurveGateOk` replace the `curveGate` and `curveX_m` stand-ins; the front-line stand-in in the controller is deleted. The builder's six constants are read from the install (VA = PE image; `pes_full.bin` offset = VA - 0x140000000):
   - `0x142598580` = 180 (lane angle a = wrap(psi - 180));
   - `0x14259BE34` = 50 (the two arms at a +- 50);
   - `0x14256AE34` = 1000 (arm length and dedupe radius);
   - `0x14253EE30` = 100 (dedupe cone = 0.5 x 100);
   - `0x14256AE1C` = 1 (segment step along psi);
   - `0x14259BDC4` = 1.1920929e-07 (parallel test).
   The grid (71 samples, centre 35, end 70) is structural: `0x47` loop bound, `0x14259A760` = 35, `0x14259BE4C` = 70.

## Decoded from the writer (CONFIRMED: disassembly `0x142138220`, decompile of the helpers)

- Candidates: block `flag*11 + j`, `flag = (side == 0)`, in play, not the side's keeper (`0x140A638C0`). Key `s*x`; sorted descending (comparator `0x14063EA50`). If none is listed, the writer keeps the old rows.
- Segment point `q = p + 1.0 * dir(psi)`. Dedupe (`0x140A53DF0`): skip `q` when some earlier point `r` has `0 < |q-r|^2 < 1000^2` and `|wrap180(bearing(r->q) - a)| < 50`. Each kept point carries `w = a`.
- Sample i (71): `z_i = (i-35)*s`; sample line from `(-s*F.x, z_i)` to `(s*F.x, z_i)`. The best starts at `-s*F.x`. Each segment has two arms from `q` at `a +- 50`, 1000 m long. A crossing counts when `u >= 0` (no bound on t, `0x140A90C20`). `x_i = max_s` (strict).
- CurveEval: `u = clamp(z*s + 35, 0, 70)`; `i = floor(u)`; `u == 70` reads `x[70]`; else the lerp. The decode multiplies and divides by `0x14253EE30` around the lerp (cancels to one rounding; dropped).
- BackSpace gate: `k*mate.x - k*curve(mate.z) >= 0`, frames `int(0.0+0.5) = 0` (`0x1421225E0`, CONFIRMED).
- Verified by test: one defender at (20, 0) with psi 90 gives `x(z) = 21 - |z|*tan 40 deg`; keeper excluded; mirror for psi 270.

Correction for the report record: the decode's "OPEN dropped arguments" of `0x140470D00` (X = 180, sub-turn) and `0x140A8F7C0` (r = 1.0) are resolved from the call sites (`MOVAPS XMM2,XMM9`, `MOVAPS XMM2,XMM14`, XMM14 = `0x14256AE1C` = 1, XMM9 = `0x142598580` = 180). The arms use X2 = `0x14259BE34` = 50 and r = `0x14256AE34` = 1000.

## Open items (each named at its field or in the code)

1. Candidate record filter `0x140A48910` (record +1 in {0x11, 'W'}, +8 bit 2): not decoded; GF carries no record bytes; no man is dropped.
2. Equal keys: the tie rule of `0x141E53540` is not decoded; GF keeps slot order.
3. No listed man: PES keeps the old rows (`0x142138CA9`); GF has no earlier table, so the gate and G12 are false.
4. Team level value (S4; writers above); team mode source `M+8` (S4).
5. OBSERVED, not changed: the chooser's `attackAngle` is 0 or 180 (`elizacontroller.cpp`, `attackSign < 0 ? 180 : 0`). PES's bearing convention (0 = +z, 90 = +x; `BearingDeg`) puts the attack on x at 90/270, so OneTwo's closing count and the chooser read the z axis. The curve takes 90/270 (ponytail at the call). Owner call.
6. K+0x6E (DHB), S1 score row, GoalFront/Safety inner gates, the cut-back picker's lane/align/advance gates for BackSpace.

## Verification

- RED (test binary on the merged tree, before the ports; `PES21_ROOT=<workdrive>/PES21`): 137 pass, 7 fail. Each fails on the behaviour:
  - psi: `OneTwoUnitTest.TheReceiverGateIsHisRoleClassNotTheKeeper` (keeper answered);
  - G3: `OneTwoUnitTest.TheTeamLevelIsTheRawLevelOverOneHundred` (raw 199 answered);
  - curve: `BackSpaceUnitTest.TheCurveGateAdmitsAMateOnTheCurveAndRefusesOneBehind` (no answer), `PesCarrierCurve.CurveEvalInterpolatesTheSamplesAndClampsTheEnds` (stub 0), and `CurveTableTest.{ADefenderBends..., AKeeperIsNotListed, AttackingTheOtherWayMirrorsTheCurve}` (no table).
- GREEN: `pes_carrier_test` `[  PASSED  ] 144 tests.`; `pes_lane_test` `[  PASSED  ] 21 tests.`; no skips.
- Mutations (each built, run, reverted, then rebuilt):
  - psi `roleClass == 0` to `roleClass <= 1` (class 1 refused): caught by `TheReceiverGateIsHisRoleClassNotTheKeeper` and 6 other OneTwo tests.
  - G3 `levelHundreds < 2` to `<= 2`: caught by `TheTeamLevelIsTheRawLevelOverOneHundred` and `TheFirstReceiverTakesAShortOneTouchBall`.
  - curve gate `>=` to `>`: caught by `TheCurveGateAdmitsAMateOnTheCurveAndRefusesOneBehind`.
- Smokes (2 min, seed 7, `teams.sql` 1 vs 2, `--pes21-root <workdrive>/PES21`, run from the worktree root):
  - switch-off: `[determinism] a3d5e43a3b7b03af` on the branch and on main `9d4daa1` (run this session; main's current value). Both exit 0.
  - `--pes-loop`: `[determinism] 0e9b3f7fe4db1ec6` on the branch (exit 0, passes 6-5, shots 0-1) and on main `9d4daa1` (exit 0, passes 6-5, shots 0-1). The A4Lanes3 value `a817a13303958f90` predates main's later commits; the ports change nothing in these 2 minutes.
- Gate: not run (Main gates).
- PII: changed files grepped for machine paths: none.
- Tool note: `gfsim_run` reads its assets relative to the CWD. From another directory it aborts with `std::out_of_range` at start-up (seen on both trees before the fix).

## Silence probe (temporary counters in a throwaway worktree at `b36a845`; not committed; `--pes-loop` 2 min, hash unchanged `0e9b3f7fe4db1ec6`)

17 carrier rethinks; curve table built in 17/17. No unit answers in any rethink.

| Unit (kind) | Answers | Where the rest die |
|---|---|---|
| OneTwo (10) | 0/17 | 16 reach OneTouch; G3 kills all 16 (level stand-in 0). Receiver predicate never reached |
| DirectHighBall (11) | 0/17 | 17 die at K+0x6E == 0 (stand-in false) |
| BackSpace (3) | 0/17 | Cut-back picker: 132 mate tests; 112 fail the PassThrough validator; 4 fail the corner/curve gate; 16 pass the curve gate and die in the later picker gates (not split) |
| Safety (6) | 0/17 | 6 gate lane free; 11 reach the short picker: 73 mate tests fail `ShortMateSurvives` (the other entries are the carrier himself, skipped); no survivor |
| GoalFront (2) | 0/17 | 7 gate lane free; 10 pickers find no mate (per-mate gates not split) |

The counts come from the temporary probe only; the probe is not on the branch.

## Left

1. OneTwo stays silent at G3 until the team level source lands. ψ and G12 are live-inert until then.
2. BackSpace now dies in the cut-back picker's lane and advance gates; split next.
3. DHB (K+0x6E), S1 score row, Safety and GoalFront inner gates: as before.
4. Owner call on `attackAngle` (item 5 above).
5. Process: the three inputs share `pescarrier.{hpp,cpp}` and the carrier test, so they land as one commit (`b36a845`), not one push each. Red and green were run per input in the working tree.
