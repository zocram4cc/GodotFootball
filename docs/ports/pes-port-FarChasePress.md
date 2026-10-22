# PES second presser: target eval, gate, cover, class-5 and maintain (FarChasePress)

> **Main note 22-10-26:** earlier round on branch FarChasePress (`bf65391`); superseded by pes-port-FarChasePressMain.md.

Status: PARTIAL, pushed. origin/FarChasePress = bf65391 (merge of origin/main cc20625 with the pure port). The coordinator wiring (call site) is NOT on the branch: it regresses the enabled 2-minute pes-loop restart test, and the disabled loose-ball test stays red. The wiring is saved as a patch (<workdrive>/gf-FarChasePress-scratch/integration.patch, 245 lines, applies with git apply).

## 1. On the branch (pure ports, CONFIRMED against the decodes in docs/re/pes-re-press-eval.md and pes-re-press-coord.md)
- PressEvalPick (FUN_140633940): the first passing man in the holder's nearest list (ascending d^2; table order on ties). Gates in doc order: G3/G2 (excluded, active), G4 (G180, only while W), G5 (excluded kinds), G6 (flag 4C00 and flag 15), G7 (B748 == 0x24), sticky D_ev window, mode-4 pre-gate, S = |ref-cand|, theta (22 with press timer or W; 15 with flag 14; 13 in the aggressive phase; else 10; +3 incumbent), Block A (+ dil), accept-1, G_sub (forward of the holder, or W, or the press window), theta2 (max(12, theta) at attack level >= 3; mode 4: theta; else 12 for class 6 or aggressive, else 6), accept-2, mode-7 takeover (LR from the excluded man's class, bearing test 50 deg / 40 deg for the incumbent), mode-4 lateral test (100 deg; final margin). Writes: the caller applies PRESS and B88C = mode. No keep-max.
- PressGatePasses (FUN_140632780): FAIL on ball height > 1.5 m or T+0x78 valid; PASS on press timer, flag 13, attack level >= 3, class 5, or cover; else facing (|T+0x250 - holder facing|) <= 60.
- PressCoverByHolder (FUN_140632c70): dist < 2.5 true; dist > 14 false; else 0 < dist < 11 and the bearing within 70 deg of the heading.
- PressClassFiveMan (FUN_1406324a0 predicate): the holder's nearest side man within 3 m of the ball (5 m for kind 10); clause; keeper and match-up exclusions.
- PressPresserKept (FUN_140632e70, doc section 7 order): in play, slot < 11 with table word 0x34, keeper hold, holder change, reason-5 class-5 man, lifted/grounded kick clauses, reason-5 distance (9 m^2), holder-from-touch clause, reason 4/7 bearing band (45 deg, flag 13 off) and role class, stamp window (2 fps), marker-validity, and the d^2 keep (carrier ramp end, d^2 <= 25 m^2). Step 7 (reason 3) is unreachable (no writer); step 8 (refusal) is stand-in false.
- Constants: 42 new fields in PressConstants, each read by VA from the image (table kPressVas). Source of each VA: the eval doc claims table, the coord doc table. Evidence gap: theta 15 (VA 0x14259be0c) is from the coord doc, not the eval claims.
- Tests: pes_press_test 89 of 89 pass (21 new this round: eval 16, gate 3, cover 3, class five 2, maintain 13 replaced). Mutations, one per rule, each run, rebuilt and reverted, each caught by its own test: G_sub (ManBehindTheHolderIsRefusedUnlessFlagThirteen), accept-two (AcceptTwoNeedsSixMetresUnlessTheAttackLevelIsThree, BlockAWidensTheAcceptTwo...), excluded skip (ExcludedManIsSkipped, now mode 1 so G3 alone decides), facing (FacingWithinSixtyDegreesPassesAndBeyondFails, AnyCoverFlagPassesWhateverTheFacing), cone (TheConeDecidesTheMiddleBand), maintain 9c band (ReasonFourOrSevenNeeds...), maintain 9f boundary (StaleStampFallsThrough...). 7 of 7 caught.

## 2. Stand-ins on the pure port (each named ponytail in pespress.hpp)
flag 13 (W), flag 14 (Z), flag 15, G180 (G4), T+0xB748 order word, T+0x4C00 flag (G6a), T+0x4B4C (keepX), B8C4, match-up man (T+0xA2A0 == 0x3A), keeper hold (FUN_140a53f90), refusal FUN_140633350 (needs the MARK table T+0x558), P10 (Block A tail, OPEN), record bit 23, the press point (FUN_14063FDC0, OPEN), the team point (ball copy), the level class CLS (T+0xA294), the attack level (T+0xB8A4).

## 3. The wiring (not pushed) and its measured effect
Wiring: PesPressTick becomes PesLooseWriterTick + PesSecondPresserTick (coordinator, after the writer in the same tick); holder from the newest event with a valid on-pitch actor (Ring1Key(0), the K=8 ring, is empty in GF: its kind is not routed there); T+0x78 = newest touch by this side; X = the side's man nearest the holder (stand-in for T+0x7C); class from the class-5 predicate; gate; aggressive flag; dispatch (class 5 takes the holder's nearest side man; class 7 needs A; else the eval).
Trace (once per second, temporary, removed): class 7 on most ticks, class 5 on some (the designation works: second = 20 on side 1 with holder 10); the gate passes on 63 of 182 traced ticks (facing); the eval runs 28 times and never accepts (flag 13 = 0 gives theta 10 and accept-two 6, so no man is in reach of a far holder).
Measured:
- Base 91d707f (older main): DISABLED_ALooseDeadBallInPlayIsReachedAndPlayed 1567 ticks (pure port, no wiring), 779 (wiring), 531 (wiring + holder fix). Bound 270.
- Base bf65391 (merged): wiring gives DISABLED 1132 ticks; enabled PesLoop every-stoppage test FAILS with "a stoppage ran past 1757 ticks (longest 1758)". The pure port alone (no wiring) passes the enabled every-stoppage tests (2 of 2) and press tests (89 of 89).
So the wiring regresses the restart watch. Not pushed.
Hypothesis (not verified): GF's IsInPlay() stays 1 while a restart is pending (test log: "inPlay 1 setPiece 0 ... buf active 1 prepared 1 started 0"), so the coordinator designates a PRESS man during the restart preparation, and the restart does not start. Next check: gate the coordinator on the referee buffer being inactive (match->GetRefereeBuffer().active), then re-run the enabled test first.

## 4. Why the far-ball path cannot come from this coordinator as documented (CONFIRMED by docs)
- Flag 13 (FUN_140A63950(ctx, s, 13, 0xFF)): the team-instruction tables G+0x25CF8 / G+0x25D38 have no literal writer in PES (docs/re/pes-re-OffBallSelectors-inventory.md, row MA+0x25cf8: "none found ... no literal writer"). So W is 0 in PES data too, and theta 22 / accept-two bypass come only from the press timer.
- Press timer T+0xA53C: setter FUN_140536170, predicate FUN_140535650, a late-game losing-side press (INF(med): docs/re/pes-re-constant-team.md section 6, pes-re-transitions.md). Live kick-off samples read T+0xA53C = 0 throughout.
- Consequence (INF): in the early game the coordinator designates a presser only within 6 m of the holder (accept-two), ahead of him. PES's live pressers were 3.3 m from the ball (median), consistent with a holder near the ball. After a goal kick the holder (keeper) can be 4.6 m from the ball while the other side is far away, and then no one is designated by the coordinator in PES either (INF). The DISABLED test's keeper case must then be served by the keeper rush / first touch / sweep (arm 3), which are not ported here.

## 5. Not done
- Class 4 and class 1 predicates (FUN_140632850, FUN_140632320): not ported; class is 5 or 7 only.
- FUN_140633710 (lane clear), the mode-1 replacement, the refusal (MARK table), P10 tail.
- PRESS read in pesoffball.cpp: not done. The loose PRESS man is already chased in elizacontroller (GetLoosePressMan); no pesoffball change is needed unless the wiring returns.
- Writer and maintain interaction (OPEN): PesLooseWriterTick resets every non-winner each tick, so a second presser is released by the maintain table test each tick. PES's reset (FUN_140536690) clears only PRESS men absent from T+0xA548; the second presser is absent too, so the same reading holds unless the reset is gated.
- Doc conflict: pes-re-press-coord.md calls FUN_1404C8EA0 a sqrt; the eval doc's asm check (H38) says it returns d^2. I used d^2 (the mode-1 test is 30.25 = 5.5^2, consistent with d^2).
- Repro (5-minute seed-7 pes-loop gfsim run): not run in this round (time).

## 6. Commits and verification
- 2c6f04d pure picker tests; 0cbf5ba merge; 91d707f merge of MergeBatch4 and main; 1d3ad15 eval, gate, cover, class-5, maintain (pushed, green); bf65391 merge of origin/main cc20625 (pushed, green: pes_press_test 89/89; EveryStoppage tests 2/2).
- Builds at nice -n 19 -j2. Sim and test runs under flock <workdrive>/gf-sim.lock, one at a time. No /tmp use.
