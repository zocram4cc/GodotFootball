# PES port: WalkLane1 (branch WalkLane, base 82ad8564 = origin/Integrate)

## 0. Rig request (top, as the queue asked)
Forfeit abort `PES_DISPATCH_UNTRANSLATED 0x1408BBF20`, called out of set from `0x1409F2990` (member of closure fbuildF, root `0x1409F1F20`).
Main has already queued hook `fbbf_1408BBF20` (per Main's update). What the capture must show: a call to `0x1408BBF20` from `0x1409F2990` on the
branch that GF never took in the captured calls, with the full memory the call reads. That answers the 17% own-block coverage question for the framebuild closure.
`0x1403FB5B0` is NOT a translator bug (Main, PressLane 0127b131: harness closure gap, a pdata-less leaf `0x1403FB600`). Dropped from this queue.

## 1. Landed
- `2f8fcca3` on `WalkLane` (pushed, `ls-remote` confirmed): `AnimeActionOfClip` (src/sim/onthepitch/pestrapactor.cpp) maps ball clips to PES action ids.
  - ShortPass -> SHORT_PASS 9 (constant site 0x1407822CB); LongPass/HighPass -> LONG_PASS 8; Shot/Header -> SHOOT 0xA; Sliding -> SLIDING 0xB;
    Trip/Interfere -> TACKLE 0xD; Catch/Deflect -> KP_CATCH 0x38 / KP_DEFLECT 0x3A.
  - Id names from the image table `0x1434A5470` (index = action id; read from pes_full.bin): 8 ANIME_LONG_PASS, 9 ANIME_SHORT_PASS, 0xA ANIME_SHOOT,
    0xB ANIME_SLIDING, 0xD ANIME_TACKLE, 0x38 ANIME_KP_CATCH, 0x3A ANIME_KP_DEFLECT. CONFIRMED (table read).
  - Derivation: the rows without a constant site come from the pending-command commit (FUN_1406EEA90 at 0x1406EEB78, cmd[1]); the constant-site survey is docs/ports/pes-port-TrapAdmit2.md section 3. Header, HighPass, Trip and Interfere are INF.
  - The old ponytail said these ids abort the run (PES_DISPATCH_UNTRANSLATED 0x140789D00, reverted in 694e354e). Measured on this tree: PesStallQuick ran 12960 of 12960 ticks, forced_live=28, no abort. The ratchet 28 <= 53 holds.
  - Tests: `pes_receive_test` 60/60 green (includes PesAnimeAction.ABallActionIsNeverTakenForMovementOrForTheTrapItself, red before).
  - Gate: `gen_translated.py --check` 10,982 functions byte-identical; `check_pii` grep of the changed file clean (full `check_pii.sh` is run from the main checkout by Main).
  - Mutation: not run. The change is a mapping table; the red test fails on the old mapping (the red was measured before the edit, in the Integrate tree).
  - Throw-ins: pes_throwin_release_test ACarriedThrowInIsReleased and TheThrowInTakersAnklesStayBehindTheLine: see section 5.

## 2. SetKickStand.AFarSetPieceWalkerIsCommandedAtTheWalkDownSpeed: NOT green (diagnosed, not ported)
Per-tick probe (temporary, reverted, not committed): `tests/sim/pes_setpiece_stand_test.cpp` (the taker's state, pos, facing, command) and
`src/sim/onthepitch/pesmovestep.cpp` (the MOVESTEP entry state, facing, body direction, look heading, turn; the adjuster frame), taker only (IsPieceTakerWalking).
Measured (taker, seed 7, throw-in on the opening kick-off's preparation):
- The COMMAND is right: cmdv = 3.056 m/s (11 km/h), `useDesiredTarget`, target = spot (0, -34.3). MOVESTEP's req: `kmh=11.00`, and `heading = 35.6` = atan2(x, -y) of the command direction (0.58, -0.81). CONFIRMED.
- t=0..5: state 1 (IDLE), the body stands; the turn is 0 to 0.7 deg a tick. Stance is correct, nothing moves (0.24 m/s). INF: IDLE does not start the run at the command speed.
- t=5 -> t=6 (state 1 -> 4, STEP_LOOP): one-tick root jump of 0.156 m (8.4 m/s). The body's facing and the root direction change to (0.64, -0.77). CONFIRMED in position data. The cause is a new clip starting with a non-zero first-frame root offset (startPos + positions[frame], humanoidbase.cpp:248-251 and humanoid.cpp:1700). INF.
- t=8 -> t=9 (state 4 -> 5, STEP_LOOP_CHANGE): one-tick jump of 0.63 m (33.8 m/s). The same mechanism at the clip start. CONFIRMED in position data; the cause is the clip's root track, INF.
- t=17 -> t=18: the root direction snaps from (0.205, -0.979) (facing 11.85 deg, the desired 37 deg) to (0.891, 0.454) (facing about 117 deg). The PREQ at t=18 shows the MOVESTEP facing was right (39.7 deg, rootFlag=1, rootAngle 37.5 deg). Between the adjuster print and the tick end, the body's orientation is re-written from the clip (animApplyBuffer.orientation = startAngle + rotationSmuggleOffset, humanoidbase.cpp:~262), which overrides the yaw the MOVESTEP facing asked for. CONFIRMED that the facing request is right and the body disagrees; the writer is the animation orientation (INF until a probe on that write).
- The every-10-ticks halving (t=19, 28, 38, 48, 58) is the same override and a second clip start; INF.
- The yaw adjuster frame (ReadAdjusterFrame) carries no yaw at t=0..6 (yawSet=0) and a steady yawSet=1 from t=7; its absolute yaw (B+0xA4/AC) sits 20 deg from the body (bodyTrapDeg 150 to 160 vs the wanted 144). CONFIRMED in the trace.
- Conclusion (INF): MOVESTEP's heading and command are PES-correct. The divergence is in GF's animation layer: clip starts add a root offset and the clip's orientation overrides the yaw adjuster's facing. This is the same mechanism section 3 names for the pass target's strike root.
- NOT changed: the test scenario (no evidence it is wrong about PES). NOT ported: the clip-start root offset and the orientation override need the anime driver port (PES's anime player blends from the previous motion; GF's clip swap does not). Next step: hook PES's anime player root write for the STEP_LOOP entry (rotate/offset the clip's first frame to the body) and compare the first 6 ticks with a PES capture.

## 3. PesPassTarget.ATrapsRootMovesByExactlyTheStrikeAdjusterStep: NOT green (diagnosed)
- The test compares the PASS_GET man's GF root step to the change in the strike adjuster's root point A+0x14 (pes_pass_target_test.cpp:121-143).
- GF's body position each tick is `startPos + positions[frame]` (humanoidbase.cpp:248-251). `StepAdjusters` applies the adjuster's root displacement with `WarpRoot` (pesmovestep.cpp:727), which adds to startPos. The clip's own root track is still added, so the body moves by the adjuster step plus the clip's step.
- PES replaces the layer's root translation with the adjuster's root point while the strike flag B+0x81 is set (test comment; pes-port PassArrival section 10). GF needs the clip's root contribution suppressed while `strikeMoved`.
- Also the adjuster's displacement is `B+0x90 - O2+0xD0` (pesworldrecord.cpp:1228-1231), while the test compares with `A+0x14`. These coincide only if O2 root == A+0x14 at each entry (the claim in the PassArrival doc, not re-checked here). INF.
- Not run: the full PES-loop match (6-minute cap) per attempt. No change made.

## 4. PesPassTarget.EveryPassGetEpisodeIsWithinPesLiveRange: NOT green (diagnosed, not ported)
- Episodes are consecutive ticks in which one man is `PassGetReceiver()` (team.cpp:453). The receiver is the T+0x78 target of the side's record, rewritten each team tick by RunPesPassTarget (team.cpp ~405-440) and PesWorld::RunPassTarget.
- The team.cpp comment itself names the stand-in: the arrival-time counter (B) is GF's time-to-ball until the AI record writer (FUN_14092ED30) is driven, and the selector's pass target flips between near-tied men. A man who never reaches the ball keeps the target for as long as the record names him.
- The 10 episodes over 2.94 s (median 1.63 s, longest 11.7 s) therefore come from the target stand-in plus the receiver walk (the idle stall of docs/ports/pes-port-PassReceiver.md section 3). INF: not measured per episode in this session.
- Next step: drive FUN_14092ED30 from the team tick (its ponytail), then re-measure the 10 long episodes by tick.

## 5. Throw-in regression (walker change not made; mapping change only)
- Ran on the committed mapping build (2f8fcca3): `pes_throwin_release_test` `PesThrowInRelease.ACarriedThrowInIsReleased` OK (1,135,882 ms), `PesThrowInRelease.TheThrowInTakersAnklesStayBehindTheLine` OK; 2 of 2 PASSED. Walker not changed, so these are regression checks only.

## 6. Blockers and open items
- Two of three reds (SetKickStand, PesPassTarget x2) need the animation-layer port (sections 2-3) and the pass selector (section 4). None is a one-line fix; none was landed.
- Gate: not run (Main gates). Ratchet: PesStallQuick 28 <= 53 (measured).
- Killed theory (this session): "the walker's MOVESTEP heading is wrong": killed by the trace (MOVESTEP's facing is right at every tick; the body disagrees after the animation write).
- Killed theory: "the ball-action ids abort the run": killed by PesStallQuick 12960/12960.
- Build: fresh worktree build took about 70 min (ccache cold for translated closures); incremental rebuilds since are minutes.

## 7. Round 2 (root-motion composition; ALiveBall regression)
- **Walker (SetKickStand): green on its own, NOT landed.** Fix in `PlayIssuedClip`: clip rotated by the root yaw `startAngle` (not the travel direction); clip anchored at `spatialState.position - positions[frameNum]`. Patch: `gf-WalkLane-scratch/walker-clipstart.patch`. Walker test OK.
- **It fails the live-ball bound:** `PesStall.ALiveBall*` on that change over 82ad8564 = **2375 vs 2229** (FAILS). Worst episodes are clip starts on `trap_0_0_000_y0_sole` (156 ticks) and `sliding/idle/000.anim` (145, 87, 63).
- **Strike root (ATrapsRoot...): RED, not landed.** Placing the body at A+0x14 gave a **27.5 m** worst gap (reverted). Needs a per-tick probe of the strike man before a retry.
- **Action ids (2f8fcca3): not diagnosed** (budget). Next: ALiveBall with one function's rows at a time (ShortPass alone first). INF cause: the kicker's own body under the new ids is refused the trap gate that MOVE admitted.
- **FUN_14092ED30 writer: not started.**
- **Tree:** WalkLane tip stays at `2f8fcca3`, which Main has removed from Integrate; revert it on WalkLane (no force-push) before any further push. Worktree clean. Full notes: `gf-WalkLane-scratch/report-round2.md`.
