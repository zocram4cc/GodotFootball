# pes-port-StallLive16: the receive admission is not what refuses the trap; the trap chain refuses on 98% of man-ticks; MOVESTEP never reaches RUN for the commanded runs; no sim change landed; ALiveBall still red at 2,401 (StallLive, 21-11-26)

Status: NOT MET, and nothing sim-side landed. Branch `StallLive`, HEAD **1b56b7264df7b8984707f3936fb245ae45d5e2e1** (ls-remote confirmed; unchanged this round). Worktree clean: `src/` and `tests/` equal HEAD (all diagnostics reverted; the diagnostic patch is archived as `pes-port-StallLive16-diagnostics.patch`). Build dir deleted. No commits this round: no change was green, and the two red tests stay uncommitted (section 5).

## 1. Final-tree acceptance (1b56b726; no sim change this round, so this equals round 15's tree)

| Check | Result |
|---|---|
| PesStall.ALiveBall | **RED: 2,401 forced ticks** (baseline 223) |
| PesStall (other 7) | PASS |
| PesHang seed 11 | PASS (954,636 ms) |
| Throw-in 9/9 | PASS: 8 in one run (ankle incl.) + ACeremony PASS (1,797,166 ms) |
| PesPassTarget | PASS |
| PesPressApproach: ALoosePresserCommandNamesTheActionPressTarget | PASS |
| PesPressApproach: ADesignatedManWithNoHolderIsCommandedToTheRestingBall | **RED: 6 of 563** |
| Kick-clear mutation (match.hpp:130 removed) | RED: receiverTicks 0 vs 0, foreignNamedTicks 0 vs 0; reverted, match.hpp = HEAD |
| gen_translated --check | GREEN: 10,150 functions byte-identical |
| ledger --check | GREEN: 47,642 rows |
| Seed-7 2-min smoke | 50,435 ticks, determinism dae72e72a5e831e2 (round 15 identical) |

## 2. Item 1: the receive admission passes; the trap chain refuses (MEASURED, diagnostics in `pes-port-StallLive16-diagnostics.patch`)

Diagnostic (env-gated, reverted): every `_ReceiveCandidate` refusal and every `RunTrapKind0` outcome, logged for each man within 3 m of a ball at rest (speed < 0.3 m/s) with no PES holder, over one seed-7 2-minute PES match.

- Receive entry 3 (`_ReceiveCandidate`): **19,369 accepted** (team 0: 10,669; team 1: 8,700). Refusals: 1,075, all by the **carrier clause** (`GetPesHolder()==me && !CarrierMayTouch`): the PES holder latched beside a resting ball cannot touch it again. No refusal by designated owner, restart taker, retouch lockout or distance.
- Trap action already running: 1,179 team-0 ticks (plus team-1 ticks) with the man in TRAP (ac8=7). The early return (`playercontroller.cpp:524`) queues nothing and leaves the body to the trap tick. Not separately traced further.
- Trap chain (translated `RunTrapKind0` = 0x14079A470): **196 issued, about 16,200 refused** over the match. Refusal codes in the `out` word (diag run 3, `issued=0`): out=10: 3,442; out=0 (no status write, i.e. the race refusal FUN_140799440): 2,712; out=2: 2,643; out=7 (empty candidate query, status 7): 2,328; out=3 (bearing or commit failure, status 3): 2,147; out=4: 1,344; out=11: 1,226; out=5: 389.
- Reading (CONFIRMED structure, INF meaning): the admission gates are not the wall. The wall is the trap chain (candidate query and race), which is translated code and so reports PES's own refusals given GF's inputs. Codes 2, 4, 5, 10, 11 are not decoded (the commit-failure branch, UNREAD in `pes-re-trap-kind0.md` section 2 step 10). Divergence therefore sits in the chain's *inputs* (the man's anime state and velocity written by the adapter, the teammate arrival table the race reads, the clip rows), not in the admission predicate.
- Re-applying the PASS_GET named target (round 15 section 2; the saved patch is empty, the change is two lines in `_PassGetApproach`: `useDesiredTarget = true; desiredTarget = target.Get2D();`): **NOT re-measured this round.** Round 15 measured it: designated test 381 of 938 -> 5 of 2,161 unnamed, ALiveBall 2,401 -> 2,880. The trap counts above explain the mechanism: an arrived receiver with a named target is in range and still refused by the chain, so he stands at the ball.

## 3. Item 2: the Think arbitration (PARTIAL)

- `pesmovestep::MovementDecision` (pesmovestep.cpp:719) returns the first queued command that is not a ball action (`IsBallActionFunction`: Trap, ShortPass, LongPass, HighPass, Header, Shot; gamedefines.hpp:71), not Deflect, and not BallControl without a holder. So Movement, Special (tackle and dodge), Sliding and Interfere all win over a later press approach.
- Census of the decisions behind the fast issues (58,574 issues with command >= 2 m/s): decision type is Movement (dfn=1) in every top bucket, and the commanded speeds are **5.00 and 8.00 exactly**. Those are RangeVelocity's quantised levels (velocitystate.hpp: 3.5, 5.0, 8.0), so they come from the non-press movement (`_PassGetApproach`, `_MovementCommand`), not from `_PressApproach`, which is unquantised (`PressCommandSpeed_ms`). So the press approach is not what sets the speeds in the stall population.
- NOT measured this round: how often a Special or Sliding command from `_ReactionCommands` preceded the press approach for a loose presser. That histogram is the next step.

## 4. Item 3: MOVESTEP never reaches RUN for the commanded runs (MEASURED; cause located, fix NOT made)

Issue-level diagnostic (`[issue]` lines, pesmovestep.cpp Issue): 58,574 issues with command >= 2 m/s over the match, bucketed by MOVESTEP state:

| state (MoveState) | issues | command m/s | body m/s | request rate | playback |
|---|---|---|---|---|---|
| 1 IDLE, single clip | 4,838 | 5.11 | 0.26 | 0 | 1.00 |
| 16 RUN_TURN_STEP, single | 4,960 | 4.41 | 2.63 | 0 | 1.00 |
| 7 GOTO_RUN, single | 6,548 | 6.00 | 1.21 | 0.03 | 1.01 |
| 3 RUN_LOOP, single | 6,825 | 5.56 | 2.42 | 0 | 1.02 |
| 4 STEP_LOOP, parametric | 28,614 | 4.23 | 1.49 | 0.017 | 1.10 |
| 3 RUN_LOOP, parametric | 1,180 | 7.09 | **5.29** | **0.75** | 1.00 |

- 56,001 of 58,574 issues have request rate 0. The rate ceiling (`RateCeiling`) is 0.91-0.97 on every issue, so the `RunRateAllowed` guard is not zeroing rates.
- When MOVESTEP *is* in RUN_LOOP with a parametric issue, the body reaches 5.29 of 7.09 m/s (75%). The run path works when entered; the problem is entry. The stall population sits in IDLE, GOTO_RUN and STEP_LOOP, where the issued clip is a single walk-speed clip.
- Speed-class stand-in: `CommandSpeedClassOf` (pesmovestep.cpp:241, from the quantised levels; the ponytail at line 45 says so) maps dribble to jog (2), walk to run (3), sprint to dash (4). The class and rate inputs to the GOTO_RUN and RUN entries (`kCommandSpeedClass`, `kUseClassRate`, `kDesiredSpeedKmh`) are the suspects. NOT verified. The entry conditions are in `movestep_run.cpp` (GotoRun, lines 699-900: the stop, turn and run-entry gates), which I read but did not trace against a live state.
- **Correction to round 15:** the "2,138 SetMotion issues with body 1.39 m/s at command 3.25" reading is a state artefact. Those are IDLE, GOTO_RUN and STEP single clips, not a blend playing its own member at the wrong speed. The blend stand-in (`Issue`, grp 80) is 68 firings of 2,206, and is not the lever.

## 5. Tests and patches

- Red tests on HEAD: `ADesignatedManWithNoHolderIsCommandedToTheRestingBall` (6 of 563; committed in 05cb00d4).
- Not committed (still red, so not "green"): `<workdrive>/gf-StallLive-scratch/patches/pes-port-StallLive15-redtests.patch` (copy in local: `pes-port-StallLive15-redtests.patch`). It has the PASS_GET-inclusive designated test (381 of 938 red on the tree without the PASS_GET fix) and the speed test (median body/command 0.39 over 10,329 fast presser commands, threshold 0.7). Committing them now would add reds to a branch with one red and no fix behind them; I left them out.

## 6. Open items (numbered, each with the step that settles it)

1. Trap chain inputs (section 2): decode the commit-failure statuses (2, 4, 5, 10, 11; `pes-re-trap-kind0.md` step 10, UNREAD callees `FUN_140849400`, `FUN_140799A00`, `FUN_14085B3E0`). Settle by logging the chain's inputs (the man's anime state bytes, velocity, teammate arrival) for one refused and one issued case, and comparing against a PES capture of the same state.
2. The race refusal (out=0, 2,712 ticks): check which teammate it names and whether that teammate is refused for another reason (deadlock). One-line log in `FUN_14079FBC0`'s inputs.
3. Carrier clause (1,075 ticks): a PES holder beside a resting ball who cannot re-touch. Compare `pes-re-holder-clear.md` section 3 (the holder beside a resting ball) against the carrier clause `CarrierMayTouch`.
4. Think arbitration (section 3): histogram of the first queued command type for loose pressers, to see how often Special or Sliding precedes the press approach.
5. GOTO_RUN and IDLE exit into RUN (section 4): trace `GotoRun` and `EnterRun` for one commanded 5 m/s man, and check the class and rate inputs the adapter writes against `pes-re-anim-select.md` section 2.3 (the run-entry bands 5, 11, 16, 28 km/h).
6. Re-measure PASS_GET named target on the tree that contains item 2 or 5. Do not land it alone (it raises ALiveBall).

Summary for Main: no commit, no ALiveBall progress, and the designated-man test stays red at 6 of 563. The trap chain refusing on ~16,200 man-ticks (vs 196 issues) is the most concrete finding, plus MOVESTEP staying out of RUN for commanded 4-8 m/s runs. Both are localised to named functions above.
