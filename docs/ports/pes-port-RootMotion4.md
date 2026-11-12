# RootMotion4 report (agent RootMotion, 12-11-26)

Blockers from Main, in order: (1) the shootout keeper that never saves, bisected over `0c3fdaac..f84b1d99`; (2) the first-throw-in stall with the playback fix; (3) the ClipOwner merge numbers and the 4 -> 17 idle stalls.

## 1. Shootout bisect (in progress)
Probe: `gfsim_run --seed 7 --minutes 1 --pes-loop --set penalty_shootout_only=true` (shootout only), through simslot.
| commit | kicks | goals | saved | note |
|---|---|---|---|---|
| `0c3fdaac` TeamTick base (gf-TeamTick binary) | 8 | 6 | 2 | won by HOM 4-2, 2687 ticks |
| `d2a3d5b6` (gf-RootMotion binary) | 10 | 9 | 1 | won by HOM 5-4, 3383 ticks: **the keeper still saves** |
| `f84b1d99` (A/B with both fix switches off) | 541 | 541 | 0 | every kick a goal |
Bisect of the five remaining steps (each a shootout-only run, built per commit in gf-RootMotion / gf-RootMotion2):
| commit | kicks | goals |
|---|---|---|
| `d2a3d5b6` | 10 | 9 (keeper saves) |
| `2dd6d027` (GF's movement requeue refused on a PES locomotion clip) | 541 | 541 |
| `2af64b31` | 541 | 541 |
| `93f5f144` | 541 | 541 |
| `f84b1d99` (switches off) | 541 | 541 |
**The commit is `2dd6d027` (mine): one line in `Humanoid::SelectAnim`'s ReQueue branch, `if (OnPesLocomotion() && command.desiredFunctionType == e_FunctionType_Movement) return false;`.**
Keeper trace per 4 ticks of each kick (`d2a3d5b6` against `2dd6d027`): the good keeper alternates PES idle/loco clips with GF clips (`movement/idle/000_idlelevel1`, `autogen [v0 b0] => [v0 b0 a45]` turns, `idle/000_accel`): GF's movement requeue turns/steps him. The bad keeper never leaves the PES clips (`stand_a/c/d/e`, `idle_0_3_run_000_v3`). Experiment (uncommitted): the same refusal for every role except `e_PlayerRole_GK` on `2dd6d027`: shootout-only seed 7 = 18 kicks, 1 save, "won by AWA 8-9" (the keeper saves again). So the keeper's GF movement requeue is what the save path needs; PES's keeper has its own mover (KP actions, not translated), so GF's requeue is the keeper's stand-in (the code already says so in pesmovestep.cpp MovementDecision).
Keeper command trace (`RM_TRACE`, uncommitted): the requeue the keeper needs is a stand-still Movement command with a look-at on the ball (`dir 0,0 vel 0 lookat 1 useMove 1`), issued every 3-4 ticks while he is on `stand_*` / `idle_0_3_run_*` PES clips, `desig 0` before the kick and `desig 1` once the ball is struck.
Red test `MoveStepWire.AShootoutKeeperSavesAKickInTheShootoutOnlyRun` (pes_loop + penalty_shootout_only, counts `lastOutcome == Save` over the kicks until the shootout ends, cap 150): red on the tip `253b17e8` (40 kicks, 0 saved).
Fix variants on the tip (each built, wire test run):
| exemption | keeper-walk test | kick-off test | shootout |
|---|---|---|---|
| none (tip) | green | green | 40 kicks, 0 saved (RED) |
| GK unless (designated and ball at rest) | green | green | 40 kicks, 0 saved |
| every GK | RED (the thrash is back) | - | 140 kicks, 1 saved, shootout ends |
So the exemption is needed but does not give back the base's save rate: kicks to the first save are base 8 (2 saves), `d2a3d5b6` 10, `2dd6d027`+every-GK 18, tip+every-GK 140. A second cause sits between `2dd6d027` and the tip, or in the playback fix (running `f84b1d99` + every-GK exemption to split those: kicks to the first save).

Second cause: NOT real. Kicks to the first save depend on the seed. With the exemption on the tip binary (env switches, uncommitted, seed 7 shootout-only): both halves of the playback fix on = 26 kicks, 1 save; old clock + new scale = 9 kicks, 1 save; both off = 18 kicks, 1 save (the wire test's own seed gave 140 kicks, 1 save). Against 541 kicks with no save without the exemption. So the exemption restores the saves; the save rate of GF's stand-in keeper is a few percent either way (the base's "8 kicks, 2 saved" was seed luck).

**Fix (RootMotion `a5061ead`, test `89d0b5f0`, pushed; ClipOwner needs `a5061ead` ON TOP of its cherry-pick of `2dd6d027`, it applies to the same lines, merged cleanly into the scratch merge):** `Humanoid::SelectAnim` ReQueue: the Movement-requeue refusal on a PES locomotion clip no longer applies to the goalkeeper unless he is the designated man beside a ball at rest (the thrash 2dd6d027 fixed). The line carries a `ponytail:` naming the PES keeper actions (KP_PRESS action 0x3e and the keeper decider ticks) it stands for and the upgrade (translate the KP action ticks, delete the exception).
Red test `MoveStepWire.AShootoutKeeperSavesAKickInTheShootoutOnlyRun` (seed 7, pes_loop + penalty_shootout_only, cap 60 kicks): with the fix 26 kicks, 1 saved (43 s). Without it: the same configuration run through `gfsim_run` gave 541 kicks / 0 saved; the test's earlier version at the wire-test seed gave 40 kicks / 0 saved on the tip (the seed-7 test itself was not re-run red; not built twice for cost). Blanket every-GK exemption is wrong: it turns `ADesignatedKeeperNextToABallAtRestWalksToIt` red (the thrash). Green with the fix: `TheKickoffTakerClosesToTheBall`, `ADesignatedKeeperNextToABallAtRestWalksToIt`, the shootout test.
Open list: (0) 1 save in 26 kicks is still below the TeamTick base (2 in 8): the stand-in brings the keeper back but is not PES's keeper (seed noise cannot be excluded either way at n=8..26); (1) keeper movement is GF's requeue (stand-in for the KP action ticks, untranslated); (2) the exception's "designated beside a ball at rest" half is GF's thrash guard, to go with it; (3) the seed-7 test red run is by the equivalent gfsim_run, not the test.

## 2. First-stoppage stall with the playback fix (trace `RM_TRACE2`, uncommitted; tip `253b17e8`, seed 7, pes-loop, 2 min)
The "stoppage" is not a restart: the buffer reads `active 0 prepared 1 started 1` (the executed kick-off, `taker H2` is stale) and the match is `inPlay 1 setPiece 0`: a ball at 0.08-0.7 m/s at (-2.4, 16.3) that no man plays for 1758 ticks. Who is near it (designated man every 5 ticks, t=300..1000, `tr_t3.log`):
- A8 (team 1, designated, forced by the breaker) plays GF's SLIDING clips (`fn 13`: `sliding/walk/000`, `sliding/idle/000`, then GF `000_stand_up_from_back`), MOVESTEP states 13 / 6 / 9, 0.8-4.8 m from the ball at 0.1-8 m/s, and never reaches it.
- H3 (team 0, designated) 0.5-0.9 m from the ball: `trap_0_0_f045_y0_sole` (PES, f 27-110: no touch), `run_3_0_135`, `mid_jockey_*` (PES), then 400+ ticks of GF `autogen [v0 b0] => [v0 b0 a0]` idle (`fn 1`) and `pass/idle/180` (`fn 4`), `mov 0.0`.
- H6 (team 0): `dribblerun_*` to 0.16 m from the ball, `avoidjump_3_3` and away.
So the men next to the ball spend the stall on GF-owned clips (fn 1 autogen, fn 13 sliding, fn 4 pass idle): the GF-clip path ClipOwner removes. (Running the same trace on the ClipOwner merge to show the same stoppage there.)

## 3. ClipOwner merge numbers (scratch `gf-RootMotion4` = RootMotion `89d0b5f0` + `origin/ClipOwner` `55d44bbb`, merge `67b7f064`, unpushed; seed 7, pes-loop, 2 min; the keeper fix is in)
| | ClipOwner alone (its doc) | ClipOwner + RootMotion (playback fix + keeper fix) |
|---|---|---|
| census `NoPesLoopManPlaysAClipOfGfsOwn` | 34.2 % | **148,422 / 1,652,376 man-ticks = 9.0 %** (red, as the assertion is `== 0`) |
| `AMovestepClipInATurnOrStopStateIsNeverReplacedByAGfClip` | - | red: 2181 turn clips lost |
| ticks to full time | 42,667 (31,570 before the elapsed read) | **75,108** (level after regular+extra time: a 76-kick shoot-out, HOM 37-38 AWA with 1 save, ends it; without the keeper fix this never ended) |
| `ALiveBallNeverLiesStillLongEnoughForTheBreaker` | 1,638 (1,345 before) | **1,577** (red: the breaker forced a live ball) |
| smoke hash | - | `7c2feb57474c608b`, `[idle] 14 episodes, stalls 14`, 36 stoppages, longest 23.6 s (1277 ticks) |
| `ATrapClipIsNeverRestartedBeforeItsContactFrame` | - | OK (740 s) |
All other `PesStall.*` OK (12/15). So the playback fix cuts the GF-clip census from 34 % to 9 % and the breaker from 1,638 to 1,577; it does not close either. The tick count is not comparable to ClipOwner's (that run ended in regulation or its own extra time; this one adds the shoot-out, ~22k ticks for 76 kicks).
Stall of the first stoppage: the same trace on this merge passes that stoppage but still reports resting-ball stalls (`[stalemate] 5.0 s / 8.0 s` at (-0.3, 20.6) and (4.7, -33.0)); its designated men also spend ticks on GF clips (fn 1, 12, 13 counts 14 / 5 / 18 of 139 samples against 25 / 0 / 37 without ClipOwner): ClipOwner reduces, not removes, the GF-clip path. Verdict for item 2: the stall is the GF-clip path (designated men on GF sliding / autogen / pass-idle clips beside a resting ball), not a defect of the playback fix's own code (it changes only the rate PES clips advance and the speed scale). Landing with ClipOwner makes it pass this stoppage; it does not remove the family (14 idle stalls).

## 4. The receiver's request (4 -> 17 idle stalls): partly checked, NOT closed
Targeted print in `ElizaController::_PassGetApproach` (uncommitted, reverted) on the RootMotion tip, seed 7, ball speed <= 0.5: 60 calls in one stoppage (t=314..336): H6 (team 0) is still the PASS_GET receiver while the ball rolls at 0.49 -> 0.28 m/s (CONFIRMED: `Team::PassGetReceiver()` holds a receiver for a pass event whose ball has stopped), `ReceivingPoint` answers the ball itself (target = path[0] = the ball, dist 4.5 m), so the request is a walk to the ball, not StepLand2's cmd=0 / heading 90 / 8 m ahead / 28.8 km/h (8.0 = `sprintVelocity`, so that request is a clamp of a far target, which this episode does not produce). That request is not reproduced on this tree; it needs StepLand2's `StepAudit5-bk` tree (MoveStepCell `46d6be94` + RootMotion). Defect candidate stays `PassGetReceiver()` never expiring once the pass ball is at rest (team.cpp:449, no test that the ball still moves): not fixed here (it would change main's switch-off behaviour of the PASS_GET gate; PES's own gate for the receiver state is the movement selector state 2 `FUN_140931c60`).
## Open list (final)
1. Keeper: GF's requeue stands in for the untranslated KP actions; 1 save in 26 kicks is below the base (2 in 8); the exception's "designated beside a ball at rest" half is GF's thrash guard.
2. ClipOwner merge: census 9.0 % (was 34.2 %), breaker 1,577 ticks, 14 idle stalls, 75,108 ticks incl. a 76-kick shoot-out; the GF-clip path is reduced not removed (autogen idle fn 1, sliding fn 13, pass idle fn 4 beside resting balls).
3. `PassGetReceiver()` stale after the pass ball stops.
4. The seed-7 shootout test was not re-run red (gfsim_run's 541/541 is the red evidence).
