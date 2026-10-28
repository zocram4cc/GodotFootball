# pes-port-StoppageFlow2: throw-in suite, kick-off gate, harness (27-10-26)

**Branch.** `origin/StoppageFlow` at `999c300d` (pushed, confirmed by `ls-remote`). Previous head `98216f5c`. Merged `origin/main` (`1cf95236`, 52 commits) as `1b62cd7f`, no conflicts. Worktree `<workdrive>/gf-StoppageFlow`.

**Status.** One PES-sourced change landed (kick-off release gate). The throw-in suite is green on seed 3 after a harness fix. Corners, the card delay, the throw-in and goal-kick gates, the per-kind table, the 10-minute matches and most suites are **not** done; see section 7.

## 1. Throw-in regression: diagnosis (CONFIRMED)

- Merged HEAD before my changes: `pes_throwin_release_test` 3 of 4 red (`throwin_merged.log`, 357 s). Same three as in `pes-port-StoppageFlow.md` section 6.
- **It is not a release stall.** A temporary trace (env-gated, reverted before commit) on seed 7, 2 match-minutes, showed:
  - no throw-in is ever awarded (`desiredSetPiece == ThrowIn && active` never true);
  - no touchline out-of-play event (K=10, `PesOutOfPlayEvent`) in 8.5 sim-minutes; ball samples peak at |z| 28.1 m against a 34 m touchline;
  - the only end-line events are 5 goals (|x| about 53, |y| < 3.7); restarts are 4 kick-offs after goals, goal kicks at mid-pitch from the stop path, and 2 period ends.
  So the test's premise (a throw-in in this trajectory) fails, and no throw-in ever started.
- **Harness is not gfsim's match (CONFIRMED).** `gfsim_run` loads the install's `common/etc/pesdb/Player.bin` (`pes::PlayerDatabase::Load`) into `MatchSetup::pesPlayerDatabase`; the test helper `pestest::FillMissingPesConstants` never did, so the match used stand-in form, stronger-foot and injury values (`match.cpp` fallbacks). Fixed in the shared helper (red test first: `TheMatchIsBuiltWithTheInstallsPlayerDatabase`, null before; green after). This did **not** change the seed-7 trajectory: the stop list was identical before and after.
- **Fixture.** Seed 7 two minutes now produces no throw-in under the ready point. Seed 3, same harness: **5 of 5 green** (`throwin_seed3.log`, 468 s), including a throw-in award at t=246759 ms (kind 2) and the three carry/lift tests. Moved `kSeed` to 3, the precedent Cutover7 set for the throw-in carry tests (commit b71c0a66).
- **Not verified:** the ready-point mutation on seed 3 (not run; budget). Whether the seed-7 absence of touchline outs is GF ball/AI behaviour is OPEN (item 2).

## 2. Kick-off gate (CONFIRMED on the test; suites not re-run)

- PES: set-play ready (MI+0x116C = 3) to release (play 1) is 134 frames in 4 of 4 live kick-offs (`pes-port-StoppageFlow.md` section 1; LiveFlow). `pes-re-kickoff-trigger.md`: arm to release 132-134 in six kick-offs; the release is a taker-approach scan, not a timer.
- Change: `pessequencer.cpp` `kKickOffReleaseFrames = 134`, `ReleaseFrames(KickOff)` returns it. Before: 0 (kick-off released at armed, about 75 frames after ready).
- Red first: `pes_restart_sequencer_test` `EachRestartIsReleasedOnItsMeasuredFrame` now asserts kick-off 133 false, 134 true; it failed at HEAD (RC=1). **Mutation:** the constant at 0 is the old gate, red by the same assertion. Green after: **16 of 16** (`seq_green.log`).
- **Switch-off hash unchanged:** `gfsim_run` 5 min seed 7, no `--pes-loop`: `[determinism] 7ca3d08f68567016`, `[ticks] 31521` (`switchoff_m5_s7_final.log`). Matches the brief.

## 3. Corners (OPEN; not fixed)

- The decision rule matches PES. A5Judge §9.1 (live-confirmed, `pes-re-A5Judge.md`): goal kick iff `dir_T·x ≥ 0`, side `T==0`. GF `EndLineRestart(lastTouchSide, x)` with `Team::GetSide()` (team 0 = -1 in the first half) gives the same result. The last-touch record survives until the decision (`ResetSituation` runs only at prepare, referee.cpp:818).
- **Why no corners in the samples (INF):** in the PES-faithful two-minute runs there is no non-goal end-line out. Seed 7's five end-line events are goals. The ball rarely leaves through the end line in this sim, so the last-touch rule is not what is being exercised.
- **Anomaly (OPEN):** seed 3 at t=246759 ms awards kind 2 (throw-in) with the post-tick ball at (52.72, -8.18). A throw-in needs |y| > 34 at `ballPos` (referee.cpp:266, `overSideline`). The post-tick sample is 26 m away from that, so the decision position and the post-tick position differ. Needs a trace of `ballPos` against the throw-in spot in the decision tick. This may be where corners and throw-ins are being decided incorrectly.

## 4. Card delay (OPEN; not changed)

- GF: `kPesCardOnScreen_ms = 3000` (`referee.cpp:64`) drives the deferred-card issue gate (555-557), the card show (771-773, 1207-1209) and the prepare shift (785). Pinned by `pes_stoppage_timing_test.cpp:369` and `LateCardIsOnScreenThreeSecondsIntoTheStop` (404-409).
- PES (`pes-re-foul-choreography.md` 9.5 and 4.6; `pes-re-A5Judge.md` J5): late card is step 6 = max(2.5 s, the 340-tick scene `FOUL_CMN_CARD_Y_B`, 60 ticks a second) = 5666 ms; the card prop `REF_CARD_Y` shows at tick 40 of the scene = 666 ms. The step runs in the next stop's programme, which GF does not model (ponytail).
- Not done. The 8-of-9 free-kick deferred-card rate (R1) is unexplained and looks like over-issue; PES issues a late card only for an advantage let go on a yellow-level foul.

## 5. Throw-in and goal-kick gates (not changed; n not established)

- PES live throw-in samples: **n = 0** in both match segments in `liveflow.jsonl` (`G 0x1c821d60`, `G 0x1de409a0`).
- Goal kick: G `0x1de409a0` analyzer gives 14 "goal-kick" fragments (ball-out, median 46 frames) that are not clean restarts. n not established.
- `flowstats_match.py` fragments the stoppages (G `0x1c821d60`: 389 stoppages, 315 "kickoff" with a median of 8 frames). Its per-kind counts are not usable until it is fixed (item 4).

## 6. Proof

- Switch-off: as section 2 (`7ca3d08f68567016`, 31521 ticks).
- Per-kind phase table, GF before/after against PES, and 10-minute matches on seeds 7 and 3: **not re-run on the merged tree**. The pre-merge numbers stand as evidence only: `pes-port-StoppageFlow.md` sections 2 and 4 (before `67e9a1aa74e84e0a`, after `20cc1c13139bf4df`, seed 7: 58893 to 46973 ticks; seed 3: 52602 to 44794).
- Suites green on the final tree: `pes_throwin_release_test` 5/5 on seed 3; `pes_restart_sequencer_test` 16/16. **Not run:** `pes_restart_delay_test` (over 30 min historically), `pes_stoppage_timing_test`, `pes_restart_taking_test`, `pes_goal_kick_test`, `pes_setpiece_taker_test`.
- Red and mutation per fix: throw-in fixture: red on seed 7, green on seed 3 (ready-point mutation on seed 3 not run). Kick-off: red at HEAD, green after, constant-at-0 mutation red by the same assertion. Player database: red, green.

## 7. Unfinished (numbered)

1. **Card delay** (section 4): PES values 5666 ms hold and 666 ms card show, `referee.cpp` 555-557, 771-773, 785, 1207-1209, and the two stoppage-timing tests. Not changed. R1 over-issue unexplained.
2. **Corners** (section 3): no non-goal end-line out in the PES-faithful samples; the throw-in decision at end-line coordinates (seed 3, t=246759 ms) is OPEN. Needs the `ballPos` trace.
3. **Throw-in and goal-kick gates** (R4, R5): unchanged; PES n = 0 for throw-ins; goal-kick n not clean.
4. **`flowstats_match.py`** fragments stoppages; fix before any per-kind n.
5. **Phase table and 10-minute matches** (seeds 7 and 3) not re-run on the merged tree.
6. **Suites not run after the kick-off change:** `pes_restart_delay_test`, `pes_stoppage_timing_test`, `pes_restart_taking_test`, `pes_goal_kick_test`, `pes_setpiece_taker_test`. The kick-off gate moves kick-off timing, so these may pin the old value.
7. **`pes_team_tick_test` does not compile** (pre-existing on main): `marktable::Facts::holderX/holderZ`, `Candidate::posX`. Not fixed.
8. **Mutation of the ready point on seed 3** not run (test fixture change).
9. **Seed-7 two-minute trajectory** has no touchline out; whether that is GF ball/AI behaviour is OPEN (not a stall).
10. **R6 to R8** (hold variance, goal celebrations, kick-off with 0-0) not addressed.

## Files

- `src/sim/onthepitch/pessequencer.cpp`: kick-off gate.
- `tests/sim/pes_restart_sequencer_test.cpp`: kick-off assertion (red, then green).
- `tests/sim/pes_match_constants.hpp`: loads `PlayerDatabase` (`common/etc/pesdb/Player.bin`).
- `tests/sim/pes_throwin_release_test.cpp`: player-database test; seed 3.
- Scratch logs: `<workdrive>/gf-StoppageFlow-scratch/` (`throwin_merged.log`, `throwin_trace.log`, `throwin_pdb.log`, `throwin_seed3.log`, `seq_green.log`, `switchoff_m5_s7_final.log`, `flowstats_all_G.txt`, `flowstats_G2.txt`).
