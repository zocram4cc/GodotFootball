# PassLane2: the PASS_GET aim is stale, not zero-sampled; the cadence port was refuted; no behaviour change landed

Branch `PassLane` (origin tip `7104ed38d`, ls-remote confirmed; this lane's only commit is the red test from PassLane1). Merged `origin/Integrate` (a316278d1): already contained, no merge commit. Worktree `<workdrive>/gf-PassLane`; scratch `<workdrive>/gf-PassLane-scratch` (probe patches, logs, helper scripts). Build dir and unpacked captures deleted. Gate: not run (Main gates). CONFIRMED = run or decode this session; INF = inference; OPEN = blocker named.

## 0. Verdict
1. **Task 1 premise is false for this symptom (CONFIRMED by a live-sim probe).** At every probed tick of the 43 m red, the 250 GF ball samples (`SetBallSample`, B+0x88+0x28j) lie within 0.25 m of the rolling ball, so the aim is not read from zeroed or far samples. The aim sits still while the ball moves (section 2).
2. **The aim is frozen, not computed from the ball (CONFIRMED).** R+0x894 holds one value across consecutive ticks while the ball moves 0.2 m. GF's `RunPassGetAction` is called from `ElizaController::_PassGetApproach`, which runs only inside `HumanoidBase` requeue (`humanoid.cpp:505-525`, `RequeueOnFrame` predicates, `humanoid.cpp:426-503`), so the tick is not per-frame in GF.
3. **Cadence port REFUTED (CONFIRMED by test).** Running the tick per team tick (`Team::RunPesPassTarget`) and reading R in the controller moved the aim red from 43.26 m to 51.32 m; `EveryPassGetEpisode` stayed at 10 of 30 over. Reverted (patch kept: `port_cadence_REFUTED.patch`). So "requeue cadence is the cause" is not the whole story.
4. **Nothing landed.** No PES writer ported, no GF behaviour changed. The red aim test stays red. ALiveBall and PesStallQuick were not run (no behaviour change to measure). No ratchet number moved.
5. **Task 3 (TRAP reach gate): not started** (budget).

## 1. Ball samples in PES and in GF (CONFIRMED where marked)
- Sample layout (CONFIRMED, k8tmA captures): sample j = B+0x88+0x28j is the ball position j ticks ahead; `(sample1 − sample0) × 54` equals B+0x2948 (velocity) to float precision on seq002 (vel (-12.72, 1.12, 10.51) m/s vs sample difference ×54 (-12.6, ...)). Samples run to 249 (14-40 m away on the six moving-ball captures).
- Capture set: `live2/pid3880818/seq002..seq007` (six `k8tmA_140945EC0` calls, route BallFollow). Ball speeds at the call 9.2-20.8 m/s, frames 169-612 (`k/` summary, `tools/k8sum.py`). All six are in play with a moving ball; **none has a resting ball, so none checks the resting-ball samples in PES.** (OPEN for PES's rest case; the live rest rows are in PassReceiver3 §14.)
- GF writer (CONFIRMED by source): `Ball::CalculatePrediction` (`ball.cpp:135-267`) writes samples 1..249 each call with `SetTrackSample(tick, nextPos)` and sample 0 with `positionBuffer`; it runs from `Ball::Process` (`ball.cpp:295`) and `ResetSituation` (`ball.cpp:311-318`). `team.cpp:445` still says "the prediction samples 1..249 stay zero": **that comment is stale** (StallTrap10 §2 said so; I confirmed it by probe). Fixed in the reverted port only; still open in the branch.
- PES writer: NOT decoded (OPEN). `0x140A6CF70` is only an address calculator (`B+0x88+0x28j`, j ≥ 250 clamps to 249; translated, `pes_fn_140A6CF70`). I looked for the PES writer three ways: callers of the ball integrator `0x140876360` (E8 scan: 40 direct callers, mostly the pass/kick planners 0x14085C..0x14086B, not the sampler), `0x140873FBA` (the N-step loop, no direct caller), and a .pdata-wide store scan for `[reg+0x88..0x90]` plus a 0x28 operand (the scan matched ~thousands of functions and was abandoned). The step function `0x1408764F0` (`dec_ball1.txt`) is the ball's own integrator tick, not a sampler. OPEN: the sampler's VA. Next: hook a PES sample store with a predicate on B's speed < 0.5 m/s (rest) and check its values.

## 2. Probe (seed 7, 2 min, `PesPassTarget.ThePassGetAimIsTheBallWhileTheBallRests`, probe patch `probe_aim.patch`; logs `probe1.log`, `probe2.log`)
- Reproduced the red: **43.26 m** (matches PassLane1).
- Side 0 slot 5, 60 consecutive ticks with miss > 5 m: aim (9.22, 12.53) fixed; ball (4.21, 16.22) → (4.40, 16.08), speed 0.39 → 0.13 m/s; miss 6.22 → 5.99 m. GF samples at every tick: s0 (4.21, 16.22), s1..s10 within 0.05 m, s249 (4.46, 16.03): **all samples on the ball** (CONFIRMED).
- Side 1 slot 21, 60 ticks: aim (-27.69, 11.35) fixed; ball (-4.65, -13.94) → (-4.71, -13.82) (PES (x, z) = GF (x, y)); miss 34.2 → 34.1 m. Same sample picture.
- `PROBECALLS` counters (the second probe, `probe2.log`) were not read before the port (see §3).

## 3. The PASS_GET tick (CONFIRMED by headless decompile, `dec_pg.txt`)
- `0x14094E0E0` (ActionPassGet tick): returns at the top when `[G+0x3B0]` is non-null and `0x1423E7FD0` holds with `0x140A60F00` = 4 or 5. G+0x3B0 is the skip object (`pesworldrecord.hpp:83`, `TeamTick` doc: 0 in every capture; GF zeroes it), so this gate is not the cause.
- Route walk `0x140945EC0` runs when `*(this+8)` is 0 or 1. Then `this+0x6C` (result 0..8) and `R+0xE48`. Then the base finalize `0x1405141F0`.
- `0x1405141F0` writes **R+0x894 (aim) on every call**: `uStack_90` starts as the man's own position (`0x140A48A10(G, slot)+0x55C/+0x564`, `0x1405141F0` lines 1-30 of the listing), is overwritten only by the vtable slots `0x80`, `0x88`, `0x58` (route and receiving), and is then stored at `param_3+0x894`. So a frozen aim means either (a) the tick is not called for that man (the requeue cadence, refuted as the sole cause by §0.3), or (b) the tick is called but `uStack_90` stays at a value the vtable calls never set, so R+0x894 holds a point that is not the ball (INF: the aim equalling the man's own position would give a 6-34 m miss if he stands there; not probed).
- OPEN: which of (a) or (b). Probe needed: the man's P+0x55C position and vtable 0x80/0x88/0x58 return values per tick, plus `*(this+8)` (route stage) and `this+0x6C` (route result), for slot 5 of seed 7 at the 6.2 m ticks.

## 4. The port (REFUTED, reverted)
- Change: `RunPassGetAction` moved from `_PassGetApproach` to the team tick (`Team::RunPesPassTarget`, after `RunPassTarget`), with `PassGetResult(slot)` read by the controller (no re-run). Patch `port_cadence_REFUTED.patch` (86 lines).
- Result: `ThePassGetAimIsTheBallWhileTheBallRests` **51.32 m** (was 43.26 m; red). `EveryPassGetEpisodeIsWithinPesLiveRange` still 10 over (red). `ATrapsRootMovesByExactlyTheStrikeAdjusterStep` red (pre-existing, WalkLane's). `EveryPassInFlightHasItsOwnSidesReceiver` green. `IsSetInFlight…` skipped (named gap).
- Why it did not help: the tick now runs every team tick, yet the aim is still far off. So the cadence is not the only fault: the finalize's `uStack_90` path (§3b) or the route stage sets the far point. The port also changes route state (stage and counter increments) per tick, which PES does too but GF's inputs differ, so it may be a wrong-input port as well; not separated (OPEN).

## 5. Measurements (what ran)
| test | result | notes |
|---|---|---|
| `PesPassTarget.ThePassGetAimIsTheBallWhileTheBallRests` | RED 43.26 m (baseline); RED 51.32 m (port) | probe §2; port reverted |
| `PesPassTarget.EveryPassGetEpisodeIsWithinPesLiveRange` | RED 10 of 30 over 2.94 s (baseline and port) | unchanged |
| `PesPassTarget.ATrapsRootMovesByExactlyTheStrikeAdjusterStep` | RED (pre-existing) | unchanged |
| `PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver` | GREEN | unchanged |
| `PesPassTarget.IsSetInFlightAndNeverStaleWhileTheBallRests` | SKIPPED | named gap in the test |
| PesStallQuick, census delta, ALiveBall, PesHang | not run | no landed behaviour change; the port was reverted before any of them |

Test binaries run: `build/tests/sim/pes_pass_target_test` (full `PesPassTarget.*`, once with the port, once with the probe). No mutation checks (no behaviour change). The census per-bucket delta was not needed (no change).

## 6. Scope questions I could not close (for Main)
- **Rig:** no live reads. Every number above comes from six captured k8tmA calls (moving ball) and the live-sim probe. The rest-ball PES samples and the route state need a hook: `0x14094E0E0` at the finalize store (`0x1405141F0` the write to `+0x894`, predicate: the receiver's speed < 0.5 m/s, EVERY 20, cap 12, FULL) and the vtable slots of the man's route object (`0x80`, `0x88`, `0x58`) for the same man. Please say whether Main will arm these; I have not requested them to avoid another PES restart.
- **Stale comment:** `team.cpp:445` (samples 1..249 "stay zero") is false; a doc-only fix belongs in the next landing on PassLane.

## 7. Next steps (exact)
1. Probe §3's two cases (own position vs ball) for slot 5 and slot 21 on seed 7: the man's P+0x55C, vtable 0x80/0x88/0x58 return, `this+8`, `this+0x6C`, and whether the tick was called (the `gProbePassGetCalls` counter in the reverted probe patch). Decides (a) or (b).
2. Only then port the writer that PES runs for R+0x894 (or the route stage). Acceptance: `ThePassGetAimIsTheBallWhileTheBallRests` green and `EveryPassGetEpisodeIsWithinPesLiveRange` lower, with a PES-cited decode.
3. Task 3 (TRAP admission for a receiver at 0.06-0.28 m, PassLane1 class A: 84% of the over-bound time) is still open; the PassLane1 census still holds.

## Artifacts (scratch, not committed)
`probe_aim.patch` (probe, not for landing), `port_cadence_REFUTED.patch` (port, refuted), `probe1.log` (43.26 m run), `probe2.log` (probe2 build failed on a namespace error; re-run not done), `suite1.log` (the port's suite run), `dec_pg.txt` (0x14094E0E0, 0x1405141F0, 0x140945EC0 decompile), `dec_ball1.txt` (0x140870020, 0x140873FBA, 0x1408764F0, 0x140876360), `tools/capmem.py`, `tools/k8sum.py`, `tools/callers.py`, `tools/pdata.py`.
