# Implementation and fix reports

What each agent changed in GF, how it was tested, and what it left open. The decodes these ports follow live in `../re/` (index: `../re/README.md`).

|Report|Covers|
|---|---|
|`DefensiveShapePort.md`|DefensiveShapePort — PES defensive line, length/width and marking po|
|`keeperport2.md`|PES 2021 goalkeeper → GameplayFootball port (correction + positionin|
|`offside-port.md`|Offside port — PES 2021 detection into GameplayFootball|
|`offsideawareness.md`|Offside awareness for GF's attackers (OffsideAwarenessPort)|
|`pes-a1t2-Liftoff.md`|A1 T2 (lift after a landing) and landing vertical response, branch L|
|`pes-a1t4-Liftoff.md`|A1 T4 (rolling block) on branch Liftoff|
|`pes-a1t4b-Liftoff.md`|A1 T4b: PES's horizontal spin cap and rolling spin convention, branc|
|`pes-a1t4c-Liftoff.md`|Gate fix: PesBallConversion.ASecondOfSimTicksIsASecondOfPesTicksWhil|
|`pes-a3-salvage-table.md`|A3Salvage: what was kept or dropped from the MovestepRun/Step/Turn and Wave1MBF-diag worktrees and why, plus where the backups live|
|`pes-ball-A1Spin.md`|A1Spin: ball spin pump, kick spin, contact-mode index|
|`pes-bisect-accuracy.md`|AccuracyBisect: attributing the 20% -> 33% pass-accuracy jump (6114e|
|`pes-fix-A2Wiring.md`|A2Wiring report|
|`pes-fix-B6Stadium.md`|B6Stadium: home stadium from Team.bin, as the match resolver gives i|
|`pes-fix-BugFix.md`|BugFix: four decode-wave defects (red → green + mutations)|
|`pes-fix-ClipSwap.md`|pes-fix-ClipSwap: the FSM's issued clip is the clip the viewer plays|
|`pes-fix-ComStyleGaps.md`|pes-fix-ComStyleGaps|
|`pes-fix-MotionRecords.md`|MotionRecords: one motion-record read for MOVESTEP groups A, B, D (r|
|`pes-fix-NullHazards.md`|pes-fix-NullHazards|
|`pes-fix-ShootVerdict.md`|ShootVerdict: shootout verdict vs plan|
|`pes-fix-SpinRunaway-2.md`|Resting test, kick-writer spin, collision cap, and the 124.9 probe r|
|`pes-fix-SpinRunaway.md`|Rolling block audit: S3 weighting, spin writers, two decoded fixes (|
|`pes-fix-TSheetPort.md`|pes-fix-TSheetPort: free-kick kicker chooser FUN_1404a8ad0|
|`pes-fix-balltick-state.md`|BallTick handover state — Milestone A1 (54 Hz sim + PES ball tick)|
|`pes-fix-carriergainfix-state.md`|CarrierGainFix — handed-over state (no commits, no push)|
|`pes-fix-carrierimageport-state.md`|CarrierImagePort — hand-over state at rest (no code written)|
|`pes-fpk-FpkBounds.md`|FpkBounds: fpk member past EOF|
|`pes-goalkick-GoalKickPort.md`|GoalKickPort: PES's goal kick is always lofted (ground window unreachable), receiver picker stand-in for `FUN_142135230`, power solved so the PES ball comes down at the receiver (~0.69 at 56 m vs GF 1.04), R6b `FUN_1409DD470` is keeper positioning not distribution; fixes the pes_loop goal-kick loop|
|`pes-hygiene-Hygiene.md`|Hygiene: the six dead `PlayingStyles` knobs deleted; why `godot.sh --import` aborted (the GDExtension never loaded: `LocomotionStyle` missing from the link, hidden by `\|\| true`; then a Godot 4.7.2 shutdown race, godotengine/godot#111645, mitigated with `--quit-after 300`)|
|`pes-movestep-MoveStepWire.md`|MoveStepWire: MOVESTEP drives the humanoid on the PES path|
|`pes-port-A3States.md`|A3States: why MOVESTEP never left GOTO_RUN/RUN_LOOP (request block never filled from the AI command, `AnimeState()` a 0 stub, no foot-band writer), the `FUN_1406b7560` fill, GOTO entry rows, foot flag shared; STOP and ACCELERATE reached; on-path restart stall open|
|`pes-port-A4Units.md`|A4Units: carrier walk rules 7 (clear chain), 8 (Safety/Forward deaths, class filters `0x14063C9C0`/`0x14063C9A0`) and 11 (`UnitArmed` caps, 5 m gate); plan-record flags named; `PassLongUnit` (`0x14212AD40`, picker `0x142128A20`), `CrossUnit` (`0x142129590`, picker `0x142125AB0`); CurveShot fills kind 3; units still open listed|
|`pes-port-A4Walk.md`|A4Walk: Dribble chooser, unit table and walk, Keep removed|
|`pes-port-A5Port-blocker.md`|pes-port-A5Port-blocker: switch-on throw-in stall and free-kick chai|
|`pes-port-A5Port.md`|pes-port-A5Port: PES restart sequencer behind pes_loop|
|`pes-port-A6Whistle.md`|A6Whistle: post-gate judge and stoppage LIMIT writer (behind pes_loo|
|`pes-port-AccelRamp.md`|AccelRamp: MOVESTEP AccelerationRamp, RUN_LOOP jostle, ramp wiring|
|`pes-port-NearestList.md`|NearestList: PES nearest lists replace OppByDist|
|`pes-port-StylePort.md`|StylePort: the style gate (bit AND compat row `0x142613D20`, indexed by PES role class) and the Defensive Full-back consumer in the run manager on pes_loop; the unported effects with their hosts; Main correction: the six §6 knobs really are dead|
|`pes-seam-SeamWire.md`|SeamWire: MOVESTEP team seam, NearestOpponent from the RunWorld|
|`pes-stall-A4UnitsFix.md`|Why the 20-min `--pes-loop` smoke hits the tick cap after A4Units: a goal-kick loop (keeper HighPass at ~56 m lands ~25 m past and rolls out; 88 of 97 cycles); GF's goal-kick choice is a stand-in; A4Units only shifted the RNG into it|
|`pes-stall-IdleEpisodes-2.md`|IdleEpisodes round 2 (branch `IdleEpisodes`, SHA d47e785, pushed; ls|
|`pes-stall-IdleEpisodes.md`|IdleEpisodes: classification of the 54 `[idle]` episodes (main a47f5|
|`pes-style-StylePort2.md`|StylePort2: Goal Poacher and Fox in the Box forward-run depth +5 m (`FUN_1405D8B70`, `0x14259222C`) placed on pes_loop; every other style effect listed with why it could not be placed (missing GF inputs per host); corrections: `1405E6E80` is the off-ball team-tick builder, `140662A10` a distance floor in a reach test|
|`pes-sw-SwitchSW.md`|SW: pes_loop switch (branch SwitchSW)|
|`pes-verdict-MoveStepCheck.md`|MOVESTEP verdict (MoveStepCheck)|
|`pes-port-LanePort.md`|LanePort: PES's pass-lane blocker detector as a pure module (`peslane.*`: `ScanLane` 0x14063CD90, `LaneFree`, `HeadingAdd`, `IsAwaySlot`, `KeeperSlot`), 21 tests, 26 mutations; one-touch predicate and carrier wiring still open|
|`pes-port-A6Breaks.md`|A6Breaks: breaks between periods (`PeriodAfterWhistle`, A6Writers §5), match-speed units per frame (90 from `0x14259be5c`), stoppage F/LIMIT at 0.5/frame; break-end writers and HUD base open; the clock did not tick in stoppages (fixed on the PES path, see body)|
|`pes-test-A5Tail2.md`|A5Tail-2: stale-image hunt BLOCKED on reproduction — prepared-tick test passes on base; only pre-prepare dead ticks fail (reaction-lag model, INF); restart paths use `ResetSituation` not `SetPosition`; `SetPosition` refresh breaks `pes_tick_a1_test` 18/18→17/18 (writes current track into previous-tick snapshot); net branch diff = test file only|
|`pes-test-A5Repro.md`|A5Repro: throw-in taker red test, RED on main — 0.2 m over-line push makes every candidate prune at the OOB line check (humanoid.cpp:3166-3171, 34.17 m), taker H4 untaken within 1757 ticks; Special clip present but refusal is positional; pre-push same taker plays a pass at +1 tick|
|`pes-port-A4Lanes2.md`|A4Lanes2: BackSpace (kind 3) and Safety (kind 6) ported and wired behind `pes_loop` (branch `A4Lanes2` `7cac846`, not merged), 113/113 carrier, 21/21 lane; both units silent in the 2-min seed-7 run (1496 walks, no answer); Safety gate lane length is a stand-in (nearest + radius); 13 decode corrections to carrier-helpers|
|`pes-port-A4Lanes.md`|A4Lanes: GoalFront (kind 2, gate `0x14212ABA0`, picker `0x1421265C0`) ported on the lane core behind `pes_loop` (branch `A4Lanes` `d5e8417`); BackSpace and Safety stopped on undecoded helpers (`0x1421226C0`, `0x142125140`, `0x142126B70`, `0x142128490`); BackSpace doc corrections (36.0 is 6 m squared; no +180)|
|`pes-doc-propagation-r4.md`|PropAudit round 4: 35 corrections from community-qa, role-writer and carrier-helpers applied in place (33 edited/noted, 2 parked); O1 closed (array-B 0 = keeper)|
|`pes-port-A4Lanes3.md`|A4Lanes3: OneTwo (kind 10) and DirectHighBall (kind 11) ported and wired behind `pes_loop` (branch `A4Lanes3` `43c16ae`), 136/136 carrier; silence probe: all five lane units die on named stand-ins (team level G3, psi, K+0x6E, curve, score row S1); Safety length = carrier's own score-row d2|
|`pes-port-TeamSliceA.md`|TeamSliceA: team tick skeleton on the PES path (`pesteamtick.*`): attack-level drift `FUN_1404F8350`, flags B8A2/B8A3 `0x1404F6E1C-58`, restart word `0x1404F6F91`, B8A3-gated line step; branch `TeamSliceA` `24621ce`, not merged (on RoleArrays)|
|`pes-port-TeamSliceC.md`|TeamSliceC: designation publisher (compacted class buckets, rank table `0x142643596`), corner ordinal fill `FUN_14051B8B0` ported and wired on the PES path; branch `TeamSliceC` `b15073e`, not merged; lateral sign unverified live|
|`pes-port-StockConst.md`|StockConst: audit of internalized dt18 values (runtime stays on the VFS winner); test fixtures moved to stock values; 13 doc rows for the stock-docs pass; branch `StockConst` `bfcd6a9`|
|`pes-port-A4LaneInputs.md`|A4LaneInputs: psi (role class), team level G3 read, curve table + gate ported (branch `A4LaneInputs` `b36a845`, not merged); units still silent: OneTwo on the team-level value (stand-in 0), DHB on K+0x6E, BackSpace in the cut-back gates, Safety/GoalFront inner gates; attack-angle 0/180 vs 90/270 flagged|
|`pes-port-FirstTouch.md`|FirstTouch WIP (`origin/FirstTouchPort` `54413ac`): owner refusal + momentum exemption for PES locomotion; loose-ball test red 1216/270; root: PES carrier write MA+0x2601C at the trap touch not ported|
|`pes-port-A4Level.md`|A4Level (`origin/A4Level` `a1882b9`, not merged): G3 team level from the install (stock 200), one attack-heading convention (90/270, pes::AttackAngleDeg), aim-vs-facing frame fix (switch-off hash moves to 75a11cdb2b3c8ec5, ruled keep); run 2 `4126007`: G4 = the carrier's Intention (NextIntention), R4 case-B abs fix; OneTwo now dies at G11/G12|
|`pes-port-RoleArrays.md`|RoleArrays (`RoleArrays-on-main` `d6cb09b`): slot-class arrays + mask readers; restart stall fixed by porting FUN_140a92950 (StepHeading, was the FirstAngle stub) at all six movestep_run sites; switch-off hash moves to 20f28a3df9b0ce12 from the mask readers|
|`pes-port-ClipWords.md`|ClipWords (`origin/ClipWords` `03b182d`, not merged): MOVESTEP command-word fill from FUN_1406b7560, real move target (cmd+0x20), action-entry stores, BandSpeedKmh units fixed (m/s); open: the body ignores STOP/TURN_STOP, Speed byte 0x27, BandSpeedKmh decode|
|`pes-merge-A5Merge.md`|A5Merge: A5Tail stack on DeadBall (`9ca351e`, not landed) — A5Tail2's tip reverted its own mental-image fix; the merged tree stalls 1758 ticks on a throw-in chain that neither parent shows (DeadBall requeue gates × A5Tail's restart pieces)|
|`pes-port-A5Tail.md`|A5Tail: throw-in spot F1+0.25, release on reach (0.65 m), kick-off receiver gate, ball dead at the whistle (`Ball::Stop`); on-path stalls traced to the taker's pass anim being refused by the picker (bestAdv 0 at all distances), shared with DeadBall|
|`pes-port-DeadBall.md`|DeadBall: why a loose ball sits dead on the PES path: `SelectAnim` refuses a Movement command while the current clip is Special (`humanoid.cpp:1972-1978`), and PES locomotion clips are Special, so the step at the ball is never applied; a chase stand-in; shared root with A5Tail's taker that never kicks|
|`pes-doc-propagation-r3.md`|PropAudit round 3: carrier 9-plan order, supporter-side decode (no flip), stamina-subs model text; parked anim-select rows|
|`pes-doc-propagation.md`|PropAudit: table of every correction propagated into an older doc (old claim, new claim, source), and the items left open|
