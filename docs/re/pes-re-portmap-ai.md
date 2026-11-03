# Decision-side port map (AI) — PortMapAI

Static RE only; no engine code changed. Ranked map of GF-native decision code and hook requests.

## 1. GF-native decision code (src/sim) — what is still GF logic

GF-native decision logic remains in:
- src/sim/onthepitch/player/controller/elizacontroller.cpp (3705 lines; 93 ponytail stand-ins): RequestCommand, Process, GetOnTheBallCommands, _AddPanicPass/_GetPassingOdds, _ReactionCommands, LoadStrategies/GetLazyVelocity/GetSupportPosition.
- src/sim/onthepitch/teamAIcontroller.cpp (3754 lines; 69 ponytail): Process, CalculateDefensiveShape, CalculateManMarking, GetAdaptedFormationPosition, UpdateTactics/UpdateStrategy, PrepareSetPiece, ProcessOffBall/ProcessRunManager.
- src/sim/onthepitch/player/controller/strategies/offtheball/* (default_def/mid/off, goalie_default) — GF formation-position strategies.
- src/sim/onthepitch/AIsupport/AIfunctions.cpp (1503 lines; 0 ponytail but GF-native helpers): AI_GetAdaptedFormationPosition, AI_GetMindSet, AI_GetPassRatings, AI_GetShotDirection, AI_GetPass, AI_GetBestDribbleMovement, AI_GetToBall/BallControl/ForceField/TimeNeeded/ClosestPlayers, AI_GetSituationRating.
- src/sim/data/playingstyles.cpp (1 ponytail: COM cards stand-in), playerskills.cpp (0 ponytail).
- src/sim/onthepitch/player/controller/playercontroller.cpp (1 ponytail) — GetDefendPosition/AddDefensiveComponent, _BallControlCommand/_MovementCommand.

Everything else in src/sim decision area is PES-ported (pes*.cpp with VAs). Evidence: ledger.md AI subsystem 1362 rows (90 ported, 128 diffed, 56 translated, 48 stand-in, 383 decoded, 533 undecoded); pes-ai-map.md maps each PES system to GF counterpart.

## 2. PES function map (entry VA, caller chain, records, closure)

- Carrier brain: entry 0x142110C90 (driver) → 0x14210F110 walk; 0x142109EB0 Clear; 0x142119390 danger builder; 0x140699960 danger writer; 0x14069A500 pressure builder; 0x1405B7470 judge decide; 0x143F546F0 judge commit. Caller chain: team tick FUN_1404F6C30 → carrier BP update (per player). Records: side record T+0x4B4C/+0x898/+0x31C1, MA+0x2601C holder, MI+0x1775 walk state. Closure: 80+ members (impure sample).
- Team tick: 0x1404F6C30 → 0x14051D9E0 designation → 0x14051E350 publisher → 0x1404F8350 drift → 0x1404FBCA0 flags → 0x1404F6F91 restart word → 0x1404FA840 arm → 0x140500B90 selector. Records: T+0x8E08 sheet, T+0x134 mode, T+0xB8A4 level, T+0xA294 team level, T+0x255 possession. Closure ~50.
- Off-ball runs: 0x140596C30 ChanceSpaceRun → 0x140606CD0 executor; 0x14060D210 SecondLine; 0x14061A640 Score; 0x14061F940 PostPlay; 0x1406013D0 Overlap. Caller: team tick alternate frame. Records: T+0x26c point, T+0xB774 run state. Closure 60-160.
- Press: 0x140634470 coordinator → 0x140633940 eval → 0x140536FC0 writer; 0x14063FDC0 press point. Caller: FUN_140535C60 dispatcher. Records: MA+0x22E4C nearest list, T+0xA548 slot table, T+0x4C00 flags, T+0xB748 order word. Closure 84 (eval+coord) / 112 (writer).
- Tackles: 0x140A06DD0 reaction chooser (17 entries) → 0x14096F870 standing tackle, 0x140971B60 slide, 0x140A08EA0 block, 0x140A08BE0 jostle. Records: player ai+0x134 human challenge, tackle.o constants. Closure ~100.
- Receive: 0x1409F8220 receive → 0x140933450/0x140931C60 PASS_GET. Caller: reaction entry 3. Records: pass team target T+0x78, offside line. Closure ~30-111.
- Keeper: 0x1409E8880 after-catch, 0x142115100/0x1421162C0 rush, 0x1409E0390 base position, 0x1409DC040 dispatch. Records: MA+0x2600C, T+0xB6DE. Closure 17-172.
- Set-piece taker: 0x14044E230 seamless throw-in, 0x140732D40 SetKick placer, 0x140732E30 corner body. Caller: restart sequencer. Records: restart spot, taker slot. Closure small-medium.
- Mental image / perception: 0x140A48A10/0x140A48A30 (nearest), 0x140A4C8E0 (pressure). Caller: carrier units. Closure ~10-50.
- Strategy: 0x14051E970 decider → attack level T+0xB8A4. Caller: team tick. Closure ~20.
- Offside line: 0x14045E3DB reorder (not writer; writer open). Caller: team tick. Closure ~30.

## 3. Ranking (impact / cost to prove)

Top 12 (visible impact / cost):
1. 0x140536FC0 press writer (high visible: pressing behaviour; cost M, 112 members; GF has ponytail press)
2. 0x140633940 press eval (84 members; GF ported pass-2 but writer missing)
3. 0x140634470 press coordinator (feeds eval; 84 members combined)
4. 0x1404F6C30 team tick root (50 members; orchestrates everything)
5. 0x140579840 concept selector (20 members; GF ported 6 concepts)
6. 0x140681FF0 lane line (160 members; GF ported lane words partially)
7. 0x140682770 forward pull (94 members; not ported)
8. 0x140606060 ChanceSpaceRun (158 members; GF ported Chance only)
9. 0x14060D210 SecondLine (153 members)
10. 0x14061F940 PostPlay (66 members)
11. 0x142129590 Cross unit (135 members)
12. 0x1409F8220 receive (111 members)

## 4. Hook requests (top 12)

Append to tools/dev/rendezvous/hook_plan.py TARGETS under PortMapAI block. All VAs verified against pes_full.bin; windows checked via displaced_window (PASS expected per prior baseline 61/61).
