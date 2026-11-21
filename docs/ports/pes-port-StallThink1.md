# pes-port-StallThink1: PES's fired reaction replaces an earlier movement, and the dribble decides only the PES holder; PesStall.ALiveBall 2,401 -> 118 (still red) (StallThink, 21-11-26)

Status: PARTIAL. The arbitration fix landed and is pushed. PesStall.ALiveBall is still red at **118** forced live-ball firings (HEAD 1b56b726: 2,401). Branch `StallThink`, pushed SHA **e49318d30613111afdf16ec2657cbaa21ba3f3a0** (`git ls-remote origin StallThink` confirmed). Based on 1b56b726. StallLive13 was messaged the SHA for integration.

Commit touches four files: `src/sim/onthepitch/pesmovestep.cpp` (`MovementDecision` and the `holdsBall` argument in `MoveHostAdapter::Think`), `src/sim/onthepitch/pesmovestep.hpp` (comment), `tests/sim/pes_movestep_wire_test.cpp` (one red test), `tests/sim/pes_press_approach_test.cpp` (one red test). Note for integration: `MovementDecision` lives in `pesmovestep.cpp`, which is in StallRun's area; the hunk is one function (about 20 lines) plus one line in `Think`.

## 1. Measurements (seed 7, two-minute PES match, `PesStall.ALiveBall`, GF_STALL_DUMP census)

Census of the 2,401 HEAD firings (1b56b726, temporary queue census, reverted; numbers from the log, `census1.log`):

| Queue shape at the firing | forced man | loose presser |
|---|---|---|
| Movement queued before a fired Special/Sliding (old rule: Movement wins; PES: the reaction wins) | 249 | 192 |
| Reaction (Special/Sliding) queued before the Movement (both rules: reaction wins) | 34 | 149 |
| No reaction, decision BallControl (dribble) | 1,132 | 689 |
| No reaction, decision Movement | 986 | 1,252 |

Reading: a fired reaction queued *after* the press approach is a real population (249 forced), but the dominant live-ball population is the dribble decision of a man who holds no ball (1,132 forced). GF's `holdsBall` was `player_.HasPossession()`, reach-based, while the controller's carrier brain reads PES's holder (`hasIt`). Killed: "reaction-before-press is the stall driver" (only 34 forced firings).

| Step | Live-ball firings | Note |
|---|---|---|
| HEAD 1b56b726 | 2,401 | known baseline, reproduced |
| + arbitration rule (`MovementDecision`) | 1,508 | `census2.log` |
| + holder (`holdsBall` = retainer or PES holder) = e49318d3 | **118** | `census4.log`, final tree. Test asserts 0: RED |

Classification of the 118 (`census4.log`): 73 men whose current function is still Special (fn=14) or the body is in a trap/walk clip, with decision Movement; 17 men in Sliding (fn=13) with decision Sliding; 24 with decision BallControl and current function BallControl; 4 others. INF: the 73 are men whose reaction has ended, or never reached the body, while the body keeps the reaction's action; the busy gate PES applies before a reaction replaces a movement (section 3) is the candidate mechanism, not verified.

## 2. Change (what PES does, and what GF now does)

- PES builds one movement command (PRESS, DELAY or MARK locomotion), then the reaction chooser replaces it with the reaction it fires (`docs/re/pes-re-defender-decisions.md` section 1, OBS: `FUN_1409f4db0` builds the movement, `FUN_140a06dd0` adopts the first true predicate, and the reaction replaces the movement). Ball actions travel on a separate channel (`docs/re/pes-re-decision-read.md` 1, 3).
- GF before: `MovementDecision` returned the first queued non-ball command, so a designated man's ActionPress queued earlier kept the decision over a later fired slide, special or interfere.
- GF now: a queued non-movement command is the decision wherever it sits; the first movement stands only when none is queued. Keeper Deflect and a non-holder's BallControl stay skipped.
- `Think()` now computes `holdsBall` as `GetBallRetainer() == player || GetPesHolder() == player`, the same predicate the controller uses for `hasIt`. Before: `player_.HasPossession()`.

## 3. Tests (red first, then green)

- `MoveStepWire.AFiredSlideReplacesTheApproachOfAPressDesignatedMan`: red on HEAD (decision Movement, expected Sliding); green after the rule.
- `PesPressApproach.ANoManWhoDoesNotHoldTheBallIsDecidedByTheDribble` (whole two-minute match): red on the rule-only tree, **1,564** non-holder dribble decisions; green after the holder change (0). HEAD's count was not measured directly (the rule-only tree had the same queue shapes).

## 4. Final-tree acceptance (e49318d3, diagnostics removed before the commit; no sim behaviour difference, the census code was env-gated and read-only)

| Check | Result |
|---|---|
| PesStall.ALiveBall | **RED: 118** forced live-ball firings (HEAD 2,401) |
| PesStall (other 7) | not rerun on the final tree (the filter ran only ALiveBall) |
| MoveStepWire (wire suite, 40 tests) | **40/40 PASS** (`wire-final.log`) |
| PesPressApproach.ALoosePresserCommandNamesTheActionPressTarget | PASS (`press-final2.log`). On the rule-only tree it was red at 1 of 3,858; the holder change removed that case |
| PesPressApproach.ANoManWhoDoesNotHoldTheBallIsDecidedByTheDribble | PASS (0) |
| PesPressApproach.ADesignatedManWithNoHolderIsCommandedToTheRestingBall | RED: **4 of 259** in-reach designated movement ticks name no target (HEAD: 6 of 563; red on HEAD too, pre-existing) |
| PesHang seed 11 | **PASS** (`hang.log`) |
| PesPassTarget | **PASS** (`passtarget.log`) |
| Throw-in (pes_throwin_release_test, 9 tests) | **9/9 PASS** (`throwin2.log`, `THROWIN2_RC=0`). ACeremony PASS at 3,181,881 ms (about 53 min, under the 4000 s deadline); it is the slow test, so a 3600 s cap would have killed it |
| Seed-7 two-minute smoke (`gfsim_run --seed 7 --minutes 2`) | completes, **29,807 ticks** to full time, determinism **c2016bf08fd0bc98**. HEAD: 50,435 ticks, determinism dae72e72a5e831e2 (StallLive16 section 1). Trajectory moved; no crash, no hang |
| gen_translated --check (on e49318d3) | **GREEN**: 10,150 functions byte-identical (3,824 bit-exact, 6,326 closure) |
| ledger --check | GREEN on the rule+holder tree before the diagnostics were removed (47,642 rows, no regression; `ledger-check.log`). Not rerun on the final commit; the commit only changes two source files and two test files, which the ledger does not reference |

## 5. Killed theories

- K1. "A reaction queued before the press drives the live-ball stall." Killed: 34 of 2,401 forced firings.
- K2. "GF's reach-based HasPossession is the right holder for the dribble." Killed: 1,132 forced firings took the dribble while the PES holder was none.
- K3. "The movement-first rule explains most of the 2,401." Partly: it explains 249 forced firings (first step, 2,401 -> 1,508 together with nothing else).

## 6. Open items (numbered)

1. The 118 remaining firings (section 1): men whose body stays in Special or Sliding while the decision is Movement. Likely PES's busy gate before a reaction replaces a movement (`docs/re/pes-re-defender-decisions.md` section 1: `FUN_140a6e090` on `+0x44e8`, `FUN_140a6e340` and `FUN_140a6e1c0` on `+0x2b08`). GF has no busy gate. Decode the two `+0x44e8` / `+0x2b08` fields against the player record before porting. Settle by logging the reaction's action class per firing.
2. Controller: its BallControl push still uses GF `HasPossession()` (`elizacontroller.cpp` `RequestCommand`, the `_BallControlCommand` call). A PES holder with no GF possession gets no dribble, and a non-holder gets a dribble that `MovementDecision` now skips. Unifying the predicate there is the next small step.
3. The 24 firings with decision BallControl and current function BallControl (section 1): holder flag 0 in the dump. INF: the holder changes between the think and the dump. Not verified.
4. The designated-man test: 4 of 259 unnamed. Pre-existing (6 of 563 on HEAD). Earlier notes (StallLive15 section 4) point at the designated man changing within a tick; not decoded here.
5. The PASS_GET target fix (StallLive15) was not re-measured on this tree. Re-measure only after item 1.

## 7. Process notes

- Two runs were invalid and redone: `census3` ran a stale binary (only the press test had been rebuilt after the holder edit); `wire-full` ran the rule-only binary. Both are discarded; numbers above are from rebuilt binaries.
- The census run takes about 23 minutes; the full `PesStall.*` filter runs past 900 s, so the ALiveBall filter was run alone.
- One mistake: I ran `pkill -P 0` to stop a watcher. It targets processes whose parent is PID 0 (kernel threads); no PID of mine was in that set, and my census and wire lanes were still running afterwards. No other process was signalled by name.
- Diagnostics (env-gated `DiagQueues`, `STALLQUEUE`/`STALLPQUEUE` prints, `UNNAMEDPRESS`) were removed before the commit; the commit contains no DIAG code.
- Logs: `<workdrive>/gf-StallThink-scratch/` (`census1.log` baseline, `census2.log` rule only, `census4.log` final, `wire-final.log`, `press-final2.log`, `hang.log`, `passtarget.log`, `throwin2.log`, `smoke-final.log`, `gen-check-final.log`, `ledger-check.log`). `classify.py` is in the same directory.
