# pes-port-StoppageFlow5: the stall is the unported first-touch pickup, not the press decode; throw-in window re-staged (28-10-26)

**Status.** Branch `StoppageFlow` at `b095afa4` (pushed; `ls-remote` confirmed). Not green. `pes_stall_test` `ALiveBallNeverLiesStillLongEnoughForTheBreaker` stays red (147 live firings). Its cause is the unported loose-ball pickup (`docs/re/pes-re-first-touch.md`), not the press-command decode, so no fix landed. `pes_restart_delay_test` `AThrowInIsNotAwardedAgainOnItsOwnSpot` is green after a window re-stage: the throw-in still arises, just past tick 15000. Sweep of all 96 `pes_*` binaries: 95 exit 0, 1 red (`pes_stall_test`, 1 of 2 tests).

## 1. Commits
- `260faa54`: merge of `origin/main` (`a585a4fd`), docs and `tools/dev/rendezvous/rv_watch.py` only. The `extern/godot-cpp` symlink was swapped for an empty directory for the merge and restored after. Pushed.
- `b095afa4`: `tests/sim/pes_restart_delay_test.cpp` only (window constant and its comment). Pushed.
- Both live probes were temporary and reverted. At `b095afa4`, `git diff` on `src`, `apps` and `tests` shows only the restart test change.

## 2. Stall: `ALiveBallNeverLiesStillLongEnoughForTheBreaker` (RED; attributed, not fixed)

**Baseline (CONFIRMED).** Seed 7, 2-min `pes_loop` match: `liveFirings` = 147, 82 s. The breaker (`src/sim/onthepitch/match.cpp:941`) designates the nearest active man over both teams once `GetBallRetainer()==0` and the idle clock has run out.

**Probe (CONFIRMED, temporary, reverted).** At every breaker firing I logged the pre-override designee and the forced man: `GetPesDecision().command` (function type and speed), move-step state and previous, the record rate and class, and `IsLoosePresser`. Two episodes, 147 firings. Ball speed was 0.078 m/s on every sampled tick. Sampled firings:

| n | t (ms) | forced man (id) | d to ball | own speed | command fn (speed) | state←prev | rate | cls | presser |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 322370 | A4 (19) | 0.70 m | 5.03 m/s | 2 BallControl (8.00) | 12←3 | 0.600 | 4 | 1 |
| 1 | 322388 | A4 (19) | 0.66 m | 5.09 | 2 (8.00) | 12←3 | 0.600 | 4 | 1 |
| 2 | 377796 | A3 (18) | 6.73 m | 2.70 | 1 Movement (6.11, press target) | 12←3 | 0.600 | 2 | 1 |
| 25 | 378222 | A3 | 5.90 m | 4.36 | 1 (8.00) | 12←3 | 0.600 | 2 | 1 |
| 70 | 379055 | A3 | 3.46 m | 6.38 | 1 (8.00) | 12←3 | 0.600 | 2 | 1 |
| 100 | 379611 | A3 | 1.58 m | 5.00 | 1 (5.00) | 12←3 | 0.600 | 2 | 1 |
| 106 | 379722 | A3 | 1.28 m | 5.00 | 2 BallControl (8.00) | 12←3 | 0.600 | 2 | 1 |
| 125 | 380074 | A3 | 0.51 m | 3.18 | 2 (8.00) | 12←3 | 0.600 | 2 | 1 |
| 129 | 380148 | A3 | 0.41 m | 2.29 | 2 (8.00) | 12←3 | 0.600 | 2 | 1 |

Function types are GF's `e_FunctionType` (`src/sim/gamedefines.hpp:53-68`): 1 Movement, 2 BallControl, 3 Trap. State 12 is `kRunTurnRun`, 3 is `kRunLoop` (`src/pes/movestep.hpp:41-66`).

**Reading.**
- CONFIRMED: the forced man is the loose-ball presser on every sampled tick (`IsLoosePresser`).
- CONFIRMED: the press mechanism in `docs/re/pes-re-press-cmd.md` §6 is not what happens here. That mechanism needs the record rate at 0.182, below the keep-rate 0.3, which makes him cycle IDLE and RUN. Here the rate is 0.600 and the state is 12←3 on every sampled tick, with no IDLE cycle.
- CONFIRMED: he runs in to 0.4–0.7 m of the ball and gets no contact. No TRAP command (GF function 3, PES command 7) appears on any sampled tick. He is commanded Movement, then BallControl, at 2–5 m/s past the ball. The ball speed never changes, and the breaker only fires while there is no retainer, so no pickup happens.

**Mechanism (decoded in the docs, not ported; the GF-side links are INF until the port exists).**
- PES's pickup is a TRAP from reaction entry 3 (`FUN_1409F8220`), issued within 10 m of a loose ball. Reaction entry 3 refuses the designated owner (`MA+0x2600C`). `docs/re/pes-re-first-touch.md` §7.2 gate 4c. The TRAP clip picker (`FUN_14081FF70`) is OPEN (§7.2 item 6).
- On the PES path a clip with `touchFrame == -1` never reaches its contact frame (`src/sim/onthepitch/player/humanoid/humanoid.cpp:597`). PES clips carry −1 (`pes-re-first-touch.md` §6 item 4, which cites line 589; the line has moved). The GF controlled-collision gate (`humanoid.cpp:533-535`) cannot fire for an arriving man (§6 item 2).
- The breaker designates the nearest man, which here is the presser. Under PES's rule (§7.2 gate 4c) he would be refused the TRAP even after the port lands. INF: needs the port to test.

**Verdict.** The stall is the unported first-touch pickup (FirstTouchPort; `docs/ports/pes-port-Cutover6.md` C3, `docs/ports/pes-port-Cutover7.md` loose-ball test). It is not the press-command decode. I stopped here. The port needs OPEN items (the TRAP clip picker; the PES touch frame from the HitData record, `pes-re-anim-select.md` §6.1) and touches shared humanoid code. I did not disable the test (see §6).

## 3. Throw-in: `AThrowInIsNotAwardedAgainOnItsOwnSpot` (GREEN after re-stage)

**The scenario still arises after the hold change, past the window (CONFIRMED).** Probe (temporary, reverted): every stoppage of seed 7's twenty-minute `pes_loop` match over 40000 ticks (139 s). Stop times converted at 54 Hz:
- kick-offs (piece 1, `gametypes.hpp:34`): ticks ≈1, 676, 4290, 8154, 13297;
- free kicks (piece 3): ≈5726 (A2, (0.75, 9.39)), ≈10714 (A2, (17.44, 15.28));
- **first touchline out, throw-in (piece 5), ≈15577**: t = 288462 ms, taker A3, ball (−7.36, 34.44);
- throw-ins ≈17284 (H2, (−20.83, −34.57)) and ≈22840 (H3, (40.55, −34.71));
- pieces 2 and 4 at ≈22056, ≈23824, ≈26358, ≈39450 (not named in the docs; the positions alone do not settle goal kick or corner).

The window was 15000 ticks, so `throwIns` stayed 0 (red; `pes-port-StoppageFlow4.md` §4).

**Fix (re-stage, not a referee change).** `kFirstThrowInWindowTicks` 15000 → 16500 (`b095afa4`). The old comment (a throw-in prepared at 14428 and released at 14521) describes an older tree; I replaced it with the probe's facts. Result: `pes_restart_delay_test` 23 passed, 0 failed in the sweep. The referee's award path is unchanged, and the test's `reawarded == 0` assertion held on this tree. No mutation was run, since there is no code change.
- Not checked: whether this throw-in is a walker or a placed one, and its release tick (the test finds it inside 16500).
- INF: the shift from 14428 to 15577 is from the R1 foul-hold change and/or the Cutover merge.

## 4. Sweep (`build/tests/**/pes_*`, tree `b095afa4`)
- 96 binaries, slots 2 and 3 (held via `flock` on `gf-sim.lock.2` and `.3`), `timeout 1800` (3600 for `pes_restart_delay_test`). Wall 2336 s. No timeouts.
- 95 exit 0. 1 red: `pes_stall_test` (1 passed, 1 failed).
- Test cases: 1982 passed, 1 failed. Restart-delay: 23 passed, 0 failed, 4 `DISABLED_` not run.
- Gate: not run (Main gates). Smoke: not run (agents do not run `gfsim_run`).

## 5. Mutations
None. No code fix landed: the stall is attributed, and the throw-in change is a test window. Both probes were reverted.

## 6. Open items
1. Stall: port the first-touch pickup. That means TRAP from reaction entry 3 for a non-designated presser, and the PES touch frame from HitData. Blocked on the TRAP clip picker `FUN_14081FF70` (OPEN, `pes-re-first-touch.md` §7.2 item 6).
2. Stall: the breaker designates the presser, and PES refuses the designated owner's TRAP (§7.2 gate 4c). Decide whether the breaker should skip presser designation on the PES path. Needs the `MA+0x2600C` decode.
3. **Decision for Main:** `pes_stall_test` is red. AGENTS.md ("The suite is green") requires a fix or a deletion with a reason. Either hold the branch until item 1 lands, or disable the test with a reason. I did not disable it.
4. `pes-re-press-cmd.md` §6: its IDLE/RUN cycle is refuted on this trajectory (rate 0.600). Do not land its change on the StoppageFlow tree on this evidence. Its §8 open items are untouched.
5. Last toucher at the stall is not traced. The `humanoid.cpp:1349` gate needs `GetLastTouchPlayer()==presser` to hand over possession. INF.
6. Throw-in: the release tick and the throw-in type on this tree are not recorded.
