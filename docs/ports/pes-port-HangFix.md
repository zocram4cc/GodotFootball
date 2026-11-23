# HangFix: PesHang on Integrate (status: NOT green, NOT pushed; uncommitted in `<workdrive>/gf-HangFix`)

## Verified
1. **Carry stall, cause and fix (verified by trace).** The seed-11 taker carried the ball through GF's MOVESTEP step loop, which orbits
   the carry spot S at 0.8-1.8 m. The carry command is `FloorVelocity(2*gap)` = dribble speed for any gap under 1.75 m, so he never
   decelerates. PES runs no MOVESTEP entry from pickup through lift (SEAMLESS_THROWIN, EV-TI-0008). `preciseArrival` changed nothing: the
   precise word was already set by PES's own `FillMoveTarget` rule.
   - Fix (`teamAIcontroller.cpp/.hpp`): the pickup's root warp now spans the carry (warp onto S at the carry clip 0xd00's natural
     root speed, APPROX), carry yaw turns toward S at 6.67 deg/frame, and the lift is a separate `pieceLifting` warp (pitch-heading yaw,
     stand and loop on its end). The 0.65 m trigger is unchanged.
   - Trace (seed 11): carry reaches S in 25 ticks, `lift=1` at f=3883, `lifted=1` at f=3896, the stand warp to 0.72 m outside.
2. **Throw-issue cadence.** The eliza pass command and the Movement hold command alternate across ticks; the body reverts to a
   Movement clip on the ticks the pass is not queued.
3. **MOVESTEP over the pass.** `PlayIssuedClip` shows the MOVESTEP FSM issuing locomotion clips every tick (motions 1747, 2840, 4155,
   4154, 3937, family locomotion) while the pass is current.

## Still failing (not resolved)
- **The pass is selected, then reverts.** At f=3924, `SelectAnim` returns found=1, the pass is current (fn 6, frame 0/90), and on the
  next tick the body is back on the throw-in loop (fn 14, frame 1/81). Interrupt 7 is `e_InterruptAnim_ReQueue`, not Cancel; the
  revert comes from the requeue path (`humanoid.cpp` ~509-512, `humanoidbase.cpp` ~180-217). Not identified.
- **PesHang is still red.** Stop frame 5448, longest stoppage 1758 ticks (the same signature as Integrate).
- **Ankle regression** (`PesThrowInRelease.TheThrowInTakersAnklesStayBehindTheLine`, failing with the carry fix: 415 of 629
  post-lift taking ticks short, worst 2.03 m). In the PesHang run the post-lift taker's P root equals his GF position at z -35.1 to -35.7
  (outward, so no ankle shortfall there). The suite's probe match was not re-run; the 2.03 m cause is not isolated.

## In the tree (uncommitted; none of it makes PesHang green)
- `teamAIcontroller.cpp/.hpp`: the carry warp, `pieceLifting`, the `ReleasePassPlaying` predicate (defined in the cpp), and a
  `pieceLifting` reset in `PrepareSetPiece`.
- `humanoid.cpp` (Main-authorized): the selection loop skips Movement and does not re-select a playing throw pass
  (`ReleasePassPlaying`).
- `pesmovestep.cpp` (NOT in Main's list; flagging it): the MOVESTEP entry returns while the throw pass plays (same predicate).
- `elizacontroller.cpp`: unchanged (the earlier once-only change was reverted: it let the Movement hold take the body).

## Not measured
PesStall.ALiveBall number, PesStallQuick, PesThrowInRelease 9/9, gen --check, PII check. Not pushed: the lane is red, with an ankle
regression I have not isolated.

## Notes
- The build lock `/tmp/gf-build.lock` was held by other lanes for long stretches; several runs waited 10-20 minutes.
- The `extern/godot-cpp` symlink was swapped to an empty dir for this report (contract: empty for git, relinked for builds).
