# pes-port-FirstTouch: a loose dead ball in play is reached and played (PES path). STATUS: NOT GREEN

Branch `FirstTouchPort`. origin `d50ef3c` -> `f0c9c15` (WIP) -> `54413ac` (WIP, final).
`git ls-remote origin FirstTouchPort` -> `54413acb1640b101938ab4a1a5a264f508fdb376 refs/heads/FirstTouchPort`.

## Result

- Target `PesRestartDelay.ALooseDeadBallInPlayIsReachedAndPlayed`: RED on `54413ac`.
  Verbatim: `a loose dead ball in play lay for 1216 ticks (bound 270)`.
  Base `d50ef3c`: `a loose dead ball in play lay for 2133 ticks (bound 270)`.
- Full `pes_restart_delay_test`: NOT green. Not run to completion on any final state (the d50ef3c run hit
  the 2400 s limit on the shared machine).
- `pes_receive_test`: `[  PASSED  ] 25 tests.` (includes the 4 owner tests). `pes_press_test`: `[  PASSED  ] 45 tests.`
- Restart pair on the `f0c9c15` tree (after narrowing the chase gates): `[  PASSED  ] 2 tests.`
  (AThrowInIsReleasedFromItsSpotAndTaken 177944 ms; ARestartTakerPlaysHisBallActionAfterRelease 200044 ms).
- Smokes and base hash: NOT run.

## The four steps

1. Owner refusal: IMPLEMENTED, unit-tested. `pes::RefusedAsDesignatedOwner` (pure), `pesreceive.hpp/.cpp`,
   wired into `PlayerController::_ReceiveCandidate` on the PES path in place of the possession gate cc8dca5
   dropped. Owner = restart taker while a restart is held; else the newest touch's actor while he still holds
   the ball; else none (a loose ball has no owner, so a chaser is not refused).
   ponytail: FUN_140457070's ring-0 designated-owner scan (`+0x2600C`) is not decoded.
   Mutation: NOT RUN (budget). Red state: the 4 `PesReceiveOwner.*` tests did not compile against base.
   Main's ruling kept: the newest-holder clause is not weakened. Measured: disabling only that clause took the
   longest episode from 1216 to 525 ticks (diagnostic run, not committed). The rule stays faithful; the dodge
   misfire it exposes is open (item 1b).
2. Wiring (press command reaches the body): PARTLY DIAGNOSED, NOT FIXED. See "Piece 2 premise" below.
3. Latch step 4b (`ContestBallPredicate`, first-touch §3.2 / §4.2): NOT STARTED.
4. Target and suite: target red at 1216 (see item 1). Suite and smokes not run to green.

## Piece 2 premise (correction to the brief)

- The base (`d50ef3c`) has ONE `_MovementCommand` call (`elizacontroller.cpp:738`). Nothing re-runs it after the
  PRESS override. The press command is built and reaches `Humanoid::Process`.
- Measured blockers, in order:
  a. Designated man's `_BallControlCommand` (queued ahead of the reaction walk, elizacontroller.cpp:643) is a
     BallControl at zero velocity and is accepted first: `[ftvel] f=3360 q=3 found=1 ftype=2 cmdVel=0.00
     cur=0.16 dist=1.68` ... `f=3420 ... cur=0.00 dist=1.58`. The first queued command, not a press speed, is the
     stand. Gating it off for the presser made it WORSE (2339 ticks); reverted. That run's designated man stood at
     2.72 m in an idle Movement clip for about 2200 ticks. Whether he was the presser was not instrumented.
  b. SelectAnim's "requeue not needed" momentum test refused every Movement entry of a PES locomotion clip:
     `[ftmom] f=7200 planned=1.19 desired=8.00 dot=2.25 dist=7.80` (6 m/s planned while the body stood still, every
     sampled tick). FIXED for PES locomotion clips in `54413ac` (humanoid.cpp). Mutation NOT run.
     After the fix the chaser reaches contact (`f=7260 dist=0.16 hasPoss=1`) instead of standing at 5.2 m for 900 ticks.
  c. The GF designated non-presser still runs the clip-led run (`useDesiredMovement=false`): it never ramps
     (see the earlier report: "the every-3-tick requeue restarts the accel clip"). NOT FIXED.

## Trace of the red test (54413ac)

- Episode 1 (presser A9 path, earlier build): 9.10 m at f=312, 1.41 m at f=432, TRAP (`func=3`) fires: contact works.
- Longest episode 19446-20661 (1216 ticks): designated men H5 / A10 alternate. Sliding (func 13) and dodge
  (`avoidjump`, func 14) at 0.15-2.8 m, `hasPoss=1` with `GetBallRetainer()` null for hundreds of ticks, no
  retain. The test calls the ball loose while a man holds it (`loose = IsInPlay && !SetPiece && !GetBallRetainer
  && speed <= kRestSpeed`).

## Piece 2/retainer: which is wrong, the port or the test? (Main's ruling 2)

- PES: the carrier is `MA+0x2601C`, written by FUN_140457070 (VA 0x140457070) from the actor of the newest
  ring-1 key (K=8); K=8 is pushed by FUN_140A0DFD0 (VA 0x140A0DFD0) on the touch record. Once valid it persists
  until a key is invalid (first-touch §5.1, §5.3; CONFIRMED structure). Latch step 2: a valid carrier makes
  the team the possessing side. So PES's "loose" = no carrier.
- GF: the retainer is the stand-in carrier. Writers: humanoid.cpp:1218 (keeper catch, `canRetain`), 1898
  (`SelectRetainAnim`), 1356 (cleared at humanoid.cpp:1354-1356 when the current anim has no
  `outgoing_retain_state`). First-touch §8 row "Carrier (MA+0x2601C)": ABSENT (animation-driven in GF).
- Verdict: a MISSING PORT (the carrier after a touch), not a wrong test definition. The test's "loose" matches
  PES's "no carrier" once the GF retainer is the carrier.
  NOT DONE. Not verified: whether the stock trap clip has `outgoing_retain_state` (INF: no retain, so the clear
  at 1356 runs). The port would: set the carrier at the PES touch, keep it while he holds the ball (ponytail:
  GF keeps it while `AI_HasPossession`, not the K=8 invalidation), PesLoop-gated.

## Open (numbered, what is left)

1. Target red at 1216 ticks. Causes measured: (a) the designated non-presser's clip-led run, item c above;
   (b) the missing PES carrier after a trap touch (retainer, FUN_140457070 / FUN_140A0DFD0, port above);
   (c) the owner's dodge misfire after the newest-holder refusal (first-touch §1: entries 4-7 misfire onto the ball
   when entry 3 is refused). Main ruled the refusal stays; the fix for (c) is (b) plus the owner's own carrier brain.
2. Piece 2 presser wiring: identify the presser per tick (not instrumented); decide whether the designated
   non-presser's loose-ball run should be dropped on the PES path (spec §7.1: "the designated man is not a chaser").
   Test on the command the body receives: not written.
3. Piece 3 latch step 4b (`ContestBallPredicate(holder)`): NOT STARTED.
4. Mutations not run: piece 1 (flip `RefusedAsDesignatedOwner` -> the 4 owner tests should go red); momentum
   exemption (restore the test -> target back to the 1175/1216 episode).
5. Full `pes_restart_delay_test` not green on any tree. Restart pair green on `f0c9c15`; the momentum change
   (`54413ac`) was not re-run against the restart suite.
6. Smokes not run: 2-minute switch-off and `--pes-loop` on the final tree, and the base hash. The base build is
   present at `<workdrive>/gf-FirstTouchBase` (detached `d50ef3c`, `build/apps/gfsim_run` built) for Main's base hash.
7. Rebase onto `origin/main` (8d8eeba) not done.
8. Debug diagnostics (FT_DBG, FIRSTTOUCH_TRACE prints) are removed from `54413ac`; the trace prints in the
   existing sim test (FIRSTTOUCH_TRACE) pre-date this work and are unchanged.
