# PES port Cutover6: the breaker waits out armed restarts; the carrier's hold bias runs on PES's holder clock; the loose-ball stall stays OPEN

Branch `Cutover` (origin): `8154ca73` (fix and tests), on `3f39d602` (merge of origin/main, docs only; the four new commits touch no code), on `744ad7fa` (Cutover5). `git ls-remote origin Cutover` = `8154ca73`. Labels: CONFIRMED = read from code, a per-tick probe trace, or a test run; INF = inference; OPEN = blocker named.

## 1. Result

- **Kick-off stall (item 2): the premise does not reproduce.** The taker takes the kick. On HEAD seed 7 the restart flag clears at tick 144 with the ball leaving at 15.4 m/s, and in no run is a kick-off armed while play is live (census: 0 ticks at HEAD and after, both seeds). The 5-s wait is the ball at rest in play, held by H11 (cause C2). CONFIRMED.
- **Possessor stall (C2): fixed.** The first 5-s stall (HEAD seed 7, ticks 270-541) is gone; the first breaker firing after the fix is at tick 16531.
- **In-play static ball, 32.5 s (item 3): not fixed.** The stretch is a loose ball with no possessor (C3). The cause is named; the fix needs the PES loose-ball approach, which is not ported. OPEN.
- **Breaker firings, PES 5 min:** seed 7 6726 -> 26 ticks; seed 3 4503 -> 296 ticks. Idle episodes (RestartWatch): 40 -> 2 and 13 -> 1.
- **GF mode unchanged:** switch-off 5 min seed 7 = `7ca3d08f68567016`, 31521 ticks, equal to main.
- **Tests:** `pes_stall_test` 2/2 green in PES mode. Each fix's mutation turns its test red (section 5).

## 2. Census (PES, `--pes-loop`, 5 min; per-tick probe, read-only; scratch only, not committed)

The probe is an env-gated line per tick in `apps/gfsim_run.cpp` (referee buffer, restart flags, the ball, the taker and the ball-retainer's clip, move state, possession clock and reach). With it attached the seed-7 hash is `9df315a3c6d7d58b` (HEAD), identical to the probe-free value, so it is read-only. Breaker firings are classed by the state of play on the firing tick.

| class (HEAD `744ad7fa`) | seed 7 ticks / episodes | seed 3 ticks / episodes | after `8154ca73` |
|---|---|---|---|
| goal celebration: restart armed, ball beyond the goal line, release 16-17 s away | 3996 / 5 | 2979 / 4 | 0 |
| goal-kick hold: armed and due, release by the PES ladder (4.9 s; PES 238-253 frames, A5 §4.7 OPEN-6) | 539 / 2 | 713 / 4 | 0 |
| corner hold (armed; PES release 365 frames, `pessequencer.cpp`) | 0 | 297 / 1 | 0 |
| half-time walk-off (period-end restart, piece 7) | 455 / 1 | 472 / 1 | 0 |
| possessor H11 (slot 10) holds a ball at rest, never releases it (C2) | 205 / 27 | 0 | 0 |
| loose ball at rest, live, no possessor (C3) | 1531 / 5 | 42 / 3 | 26 / 2 (seed 7), 296 / 1 (seed 3) |
| **total** | **6726 / 40** | **4503 / 13** | **26 / 2, 296 / 1** |

- Armed-restart firings are 99% of the census on seed 3 and 74% on seed 7. CONFIRMED.
- Post-fix seed 7 firings: tick 16531 (1 tick) and tick 27621 (25 ticks), ball at (-27.55, 1.26) and (-16.61, -28.63), `ret=-1` (no possessor), restart buffer stale (`active 0`). Seed 3 post-fix: one episode of 296 ticks from tick 6652, ball at (-17.67, -32.59), no possessor. CONFIRMED.

## 3. Causes

**C1 - the breaker's idle clock ran through armed restarts.** The idle clock (`match.cpp` stalemate block) measures ball movement only and designated the nearest man once it passed 5 s, whether or not the referee had a restart armed (`buffer.active`, set at `referee.cpp:394,442,519`, and released by `ReleasesRestart`, `referee.cpp:234-244`). Goal celebrations, half-time and the goal-kick and corner holds are all armed holds, so the breaker fired inside each. CONFIRMED (census classes; the breaker block). Not a play defect: the restart itself was on schedule.

**C2 - the possessor's 4 s hold bias never fired.** This is the first named stall. Seed 7 HEAD, per tick:
- Tick 144: the kick-off taker touches the ball and the restart flag clears through the taken exit (`referee.cpp:716-721`); ball 15.4 m/s. CONFIRMED. The taker's clip at tick 144 was not traced; at tick 180 it is an autogen kick clip (`autogen ... _mirror`) (INF on tick 144's clip).
- Tick 180: H11 (id 10) becomes the ball retainer, `ret=10`, with the ball at (-0.44, -8.95) at 0.9 m/s. The ball comes to rest at (-0.62, -9.42) by tick 270. CONFIRMED.
- Ticks 188-344: `ret=10` and H11's possession clock grows 19 -> 1796 ms. He stands in `mid_jocky_standby_noloop` and walks toward the origin at idle speed, 0.84-0.9 m from the ball. He never passes. CONFIRMED.
- Ticks 356-428 and 512-548: the reach test fails and `ret=-1` for about 72 and 36 ticks, which resets the possession clock to 0 (its growth is 0 on every such tick). The breaker fires only on these ticks, which is why it looked intermittent. CONFIRMED.

The mechanism, from code:
- `Player::hasPossession = AI_HasPossession(ball, this)` every tick (`player.cpp:408-411`), a 1.0 m reach test (`AIfunctions.cpp:1091-1107`).
- `possessionDuration_ms` grows only while `hasPossession` and resets to 0 otherwise (`player.cpp:483-486`).
- The retainer clears on the same reach test (`humanoid.cpp:1389-1393`, `CarrierAfterContact` kKeep is the GF stand-in in `pesreceive.cpp:353`).
- The in-play carrier takes the keeper-distribution branch (`elizacontroller.cpp:412`, "keeper fetched ball, probably") and passes only if `doCommand`, which requires a teammate within 10 m, an opponent within 10 m of him, and `possessionDuration_ms > 4000` (`elizacontroller.cpp:457-462` HEAD). Without it he stands (`elizacontroller.cpp:556-573`). CONFIRMED.
- PES's carrier is not cleared by a loss of the ball: MA+0x2601C "persists on a loose ball", `pesreceive.hpp:236-238`, `match.hpp:230-235`, `pes-re-first-touch.md §5.1` (cited in the code). GF's `ballRetainer` is documented there as "GF's own loose-ball predicate ... clears on the possession flicker". CONFIRMED (docs).

**C3 - a loose ball at rest with no possessor is not picked up (OPEN).** The 32.5-s stretch (HEAD seed 7, ticks 12153-13920, ball (-26.37, -16.06), 0.08 m/s): `ret=-1` on all 1768 ticks, so no possessor. The breaker designated the nearest man, H3 (id 2), 1.0-1.1 m from the ball, from tick 12423; he moves in run and jostle clips and never reaches it. CONFIRMED (probe, stalemate-who lines). The cause: the designated chase is GF's and it is disabled on the PES path (`elizacontroller.cpp:626-629`: "a PES loose ball is approached by the PRESS man's ActionPress (pes-re-press-life §8), and the designated man is no chaser (pes-re-first-touch §7.1)"). The PRESS loose-ball approach is not ported (`pes_restart_delay_test.cpp:278-281`: "the loose-ball pickup lands here (PRESS chase + HitData touch frame + latch step 4b). Red until then."). INF on the exact gate until the PRESS port lands. OPEN.

**C4 - goal-kick hold spots (OPEN, not investigated).** Several goal-kick holds sit with the ball at midfield, not on the goal-area line: seed 3 ticks 1385 (3.5, -0.1) and 21959 (3.7, 5.3); seed 7 HEAD ticks 25656 (2.1, 21.0) and 27181 (12.0, 9.3). CONFIRMED (positions). The PES goal-kick spot is a goal-area spot (`pesgoalkick.hpp`, `pes-re-A5Judge.md §4.7`). Not traced.

**Kick-off premise (item 2) and the misleading "armed" print.** The `[stalemate]` line prints `prepared 1 started 1` after a taken restart. The taken exit (`referee.cpp:716-726`) clears only `active` and leaves `prepared` and `started` set. So the print reads as armed while `active 0`. Not changed (the exit is shared with GF). CONFIRMED.

## 4. Fixes (commit `8154ca73`, PES path only; GF mode is untouched)

- **F1 (`match.cpp`, stalemate block):** the breaker does not designate while the referee has a restart armed, `pesLoop && GetRefereeBuffer().active`. The referee releases its own restarts and their timers are in the restart programme; the breaker is for play and for a dead ball with nothing armed. A genuinely overdue armed restart is now visible through RestartWatch's bound, not hidden by the breaker.
- **F2 (`match.hpp`, `elizacontroller.cpp`):** `Match` records the tick each PES holder took the ball (`RecomputePesHolder`, `GetPesHolderSince_ms`). The carrier's 4 s hold bias reads that holder clock in PES mode, and the GF possession clock otherwise. PES's holder MA+0x2601C persists on a loose ball (C2 docs), so a reach flicker no longer restarts the bias.

Not changed (scope): the outfield carrier's use of the keeper-distribution branch (`elizacontroller.cpp:412`), the stale `prepared`/`started` flags, the 125-frame kick-off release stand-in (`pessequencer.cpp`).

## 5. Proof

**Test** `tests/sim/pes_stall_test.cpp` (new; registered in `tests/sim/CMakeLists.txt`): one 2-min seed-7 PES match (`pes_loop` on), breaker firings counted per tick and filed by state of play.
- `TheBreakerNeverForcesABallUnderAnArmedRestart`: armed firings must be 0.
- `ALiveBallNeverLiesStillLongEnoughForTheBreaker`: live firings must be 0.
- With F1 and F2: both PASS (113 s).
- Mutation M1 (armed gate forced off): the armed test FAILS, 4160 ticks. CONFIRMED.
- Mutation M2 (holder clock replaced by the GF possession clock): the live test FAILS, 2088 ticks. CONFIRMED.
- Process note: my first red run built the match with empty `Properties`, which runs GF mode, so it was not PES evidence (`pesLoop` reads `pes_loop`, `match.cpp:81`). Corrected to `config.SetBool("pes_loop", true)`, as the other PES tests do. The reds above are from PES mode.

**Runs** (simslot; hashes are the `[determinism]` line):

| run | HEAD `744ad7fa` (Cutover5 / probe) | after `8154ca73` |
|---|---|---|
| PES 5 min seed 7 | `9df315a3c6d7d58b`, 29719 ticks, goals 1-4, idle 40/32 stalls, breaker 6726 ticks | `45e830a988ff01cf`, 28663 ticks, goals 1-0, idle 2/2, breaker 26 ticks |
| PES 5 min seed 3 | `218528866e51a229`, 27469 ticks, goals 0-4, idle 13/2, breaker 4503 ticks | `2c0749ccc8ca6735`, 27548 ticks, goals 2-1, idle 1/1, breaker 296 ticks |
| switch-off 5 min seed 7 (GF) | `7ca3d08f68567016`, 31521 ticks (main, Cutover5) | `7ca3d08f68567016`, 31521 ticks (unchanged) |
| PES 2 min seed 7 (smoke) | `1b5b777d77932430`, 39261 ticks (Cutover5, same invocation) | `8f62fef96d033a84`, 27352 ticks, goals 4-0, idle 0 episodes |
| switch-off 2 min seed 7 (smoke) | not measured on main | `f5fff64a9fd74b8c`, 24274 ticks, goals 1-2, completes |

**Suites** (each binary through `tools/dev/simslot.sh`, `PES21_ROOT=<workdrive>/PES21`):
- Green: `pes_stall_test` 2/2, `pes_restart_taking_test` 3/3, `pes_holder_latch_test` 2/2, `pes_receive_test` 39/39, `pes_tick_order_test` 4/4, `pes_stoppage_timing_test` 10/10, `pes_restart_sequencer_test` 15/15, `pes_goal_kick_test` 19/19, `pes_setpiece_taker_test` 9/9, `pes_throwin_release_test` 4/4, `pes_movestep_wire_test` 28/28, `pes_designation_test` 10/10.
- `pes_restart_delay_test`: 14 of 16 active tests pass (the `DISABLED_` tests are not run). The 2 failures are pre-existing: `ACarriedThrowInReachesItsSpot` and `AThrowInCarrierStandsAtThePickupClipsContactOffset` fail with "no throw-in carrier picked the ball up" on the pre-fix build too (`3f39d602` match.cpp and elizacontroller.cpp, rebuilt and run). CONFIRMED. Not caused by this change. Its first run was cut by a 1500-s wrapper limit; the rest was run by name (517 s).
- Failing, not re-run pre-fix: `pes_carrier_test` `PesCarrierInstall.TheZonesAndLaddersAreTheGamesOwn` (`bsBoxSkipCount` and `bsAngleClass` read 0 from `<workdrive>/PES21`). The test builds no `Match`, and this diff does not touch the constant loader. INF that it is an install-data assertion, not a regression. 186/187 pass otherwise.
- Not run: `ctest`, the GF whole-match suites (`sim_match_test` and the rest), the landing gate. The switch-off 5-min hash is the GF proof.
- PII: `grep` of the five changed files for machine paths and machine paths is clean. `tools/check_pii.sh` was not run from the main checkout.

## 6. What remains (numbered)

1. **C3 loose-ball pickup (OPEN).** The 32.5-s stretch and the 26/296 post-fix ticks are this class. Needs the PES PRESS loose-ball approach (`pes-re-press-life §8`) and the first-touch pickup (`pes-re-first-touch`), i.e. FirstTouchPort. The GF chase at `elizacontroller.cpp:629` stays off on the PES path until then.
2. **Throw-in carry pickup (pre-existing, OPEN).** Two `pes_restart_delay_test` cases fail on HEAD. The throw-in carrier never picks the ball up.
3. **`pes_carrier_test` `TheZonesAndLaddersAreTheGamesOwn` (not verified at HEAD).** Install-constant assertion, reads 0.
4. **Goal-kick spots at midfield (C4, OPEN).** The spot is not a goal-area spot in several holds.
5. **Stale buffer flags.** `prepared` and `started` stay set after a taken restart (`referee.cpp:716-726`). Misleads the `[stalemate]` print. Not changed.
6. **Outfield carrier on the keeper branch** (`elizacontroller.cpp:412`). A GF stand-in until A4. Not changed.
7. **Coverage.** The live test covers 2 minutes of seed 7 only. Seed 3 at 5 minutes still has one loose-ball episode (296 ticks).
8. **Kick-off release** is still the 125-frame stand-in (`pessequencer.cpp`); the kick-off trigger decode is `pes-re-kickoff-trigger.md`. Not part of the census (kick-offs are taken).
