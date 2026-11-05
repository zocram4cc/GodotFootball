# PES port PressAI (session 4): PES's real press start ported (MATCH_UP writer FUN_140536210 -> coordinator -> eval), slot reset and order helper translated; the four loose-presser reds fixed; GATE fast not yet green (three seed-drift reds, waiting for batch BE) (04-11-26)

Branch `PressAI`, worktree `gf-PressAI`, Release dir `build-rel`. Repo copy `docs/ports/pes-port-PressAI.md`. CONFIRMED = tool output; INF = inference; OPEN = blocker named. Head: see `git ls-remote origin PressAI` (last pushed commit after 0a224060 = the merge of batch BD).

## What was done
1. Merged origin/main (028f0feb, 7636546b batch BB, 23aed782 batch BD), pushed; layer regenerated (5,286 functions), drift clean.
2. How PES starts a press (CONFIRMED, your press_order captures pid358243 seq39765/39767): `FUN_140A42940(T, slot, 0x34)` is called from `0x1406343E1`, inside the TARGET EVAL FUN_140633940, reached through the coordinator after the MATCH_UP writer FUN_140536210 wrote T+0x7C. The order-word loop (T+0xB748 == 0x26) never fired in 186 captures (all 0x30, two 0x24): concept 0x26 is NOT needed. T in seq39765: T+0x78 = 0xFF, T+0x7C = 15, A2A0[4] = 0x3A, T+0x558 = {-,10,-,9,8,-,-,-,7,-,-}, T+0xA294 = 500.
3. Translated and proven (snapdiff --bounded, results_press.json):
   - `0x140536210` MATCH_UP writer: **68 of 68 bit-exact = 12 REAL captures (pid1513193 press_matchup_536210) + 56 SYNTHETIC cases** over one full match state (own-block 0.9149).
   - `0x140536690` slot reset: 56 SYNTHETIC, bit-exact (0.8958). `0x14062AF10` order helper: 28 SYNTHETIC, bit-exact (0.8876). No hook/capture reached them before (the new hooks gave 3 slim reset captures per side only).
   - eval 28 and coordinator 80 REAL captures (two launches), bit-exact; writer: the committed 23 (7 real + 16 synthetic); all 101 real writer captures also replay bit-exact (not committed over the record).
   - The synthetic builder (`tools/dev/translate/synth_press_order.py`) chooses: men's words, positions, anime fields, ball-distance lists, T+0xA548/0xF0/0xF1/0x140/0x558, the G+0x180 list, the set-piece object and (MATCH_UP) a scratch event record (E's newest-record page is a hole in every capture). Every such case is chosen, not observed.
4. GF: `PesWorld::ResetPress` runs the translated reset (hand loop deleted); `RunPressOrders` (the dispatcher's order-word loop, inert: no order-word producer); `RunMatchUp` (the dispatcher's release of last tick's MATCH_UP man + FUN_140536210); `PesPressTick`: stage -> MATCH_UP -> coordinator -> reset -> writer -> orders. PES's press state PP now starts {0xFF,0xFF,0xFF} (captures: coordinator rcx before a first presser); it started {0,0,0} (man 0 = the keeper as a standing second presser). Red-first test.
5. Tests: `pes_press_world_test` 25 pass (new: 4 MATCH_UP, 12 order/reset, PP initial). Mutations (run, rebuilt, reverted): PRESS/DELAY swap; PP offset 0x54 -> 0x58; already-PRESS/DELAY skip; helper index k vs side*11+k. NOT run on the 4 MATCH_UP tests. `PesPressApproach.ALoosePresserCommandNamesTheActionPressTarget` re-staged by census (a PRESS man exists only with an opposing last toucher, never on a free ball: 0 of 4,527 free-ball ticks, seeds 3 5 7 9 11): it now follows the PRESS-word man while the ball is in play and no set piece is armed, same assertions; passes.

## The four earlier reds
- `PesRestartDelay.PesLoopRestartsAreArmed...`, `PesThrowInRelease.ACeremonyThrowInIsReleased...`, `PesPressApproach...`: PASS. `PesRestartDelay.ALoosePresserInTheTrapWindow...` no longer exists (BB).

## Gate (honest)
- Pinned gate on tree 4267efd9 (BB, before BD), slots 1 0: smoke-off `399e180f39459c2b` (= main); pes-loop `3b4e6803ea956fa9`. ctest 2255: 3 RED: `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` (253 ticks), `PesRestartDelay.AWhistleLeavesNoFlyingBallBehind`, `PesRestartDelay.ACarriedThrowInReachesItsSpot`. Recorded re-check 0 selected, ledger and off-path pass.
- On the BD-merged tree (0a224060, outside slots, one at a time): Whistle PASSES, CarriedThrowIn PASSES (so both were trajectory-dependent, gone with BD), **PesStall FAILS (8,281 breaker ticks)**.
- pes-loop smoke on the BD tree: `ebb29b8eeb18d38d` (main BD baseline `85c7d7c515cf92ae`; this tree contains BD). Ablation (RunMatchUp off, PES cutover otherwise): `5162f3b65166d647`; the difference between main and the ablation is the press cutover itself (hand port -> PES writer/coordinator/reset), the difference ablation -> full is MATCH_UP.
- PesStall (diagnosed, NOT re-staged; split to RestingBall by Main): at all 92 breaker reports the stuck man is his team's DESIGNATED man with `HasPossession()==1` but `GetBallRetainer()==null`, the ball at rest (0.08 m/s) at his feet, his T+0xA2A0 word 0 and P+0x5D0 0 (not PRESS/DELAY/MATCH_UP; no presser on either team). The restart buffer fields are stale after the kick (not the cause). Log excerpt (seed 7, 2 min, pes-loop, tree 48275157; episode 1 then 2):
  `[stalemate] 5.0s idle: inPlay 1 setPiece 0 | ball 0.08 m/s at -6.7,12.3,0.1 | buf active 0 prepared 1 started 1 piece 2 taker H1 start-in -1.0s | forcing H3 (2) from 18.6 m`; same ball point reported every 3 s up to 32 s+, H3 forced from 0.2 m, `[stalemate-act] H3 slot 2 word 0 manword 0 designated H3 possession 1 retainerNull 1`, clip `dribble_0_3_000_sole`.
  `[stalemate] 5.0s idle: ... ball 0.08 m/s at -35.3,12.5,0.1 | piece 2 taker H1 | forcing H1 (0) from 0.8 m`, `[stalemate-act] H1 slot 0 word 0 manword 0 designated H1 possession 1 retainerNull 1`, `[stalemate-who] H1 (0): movestep 1<-17 | fn 14 clip gkmovelow_guard_0_0 frame 37/244`; still firing at 86 s. The keeper's after-catch distribution (elizacontroller.cpp:412) needs `GetBallRetainer()==player`, which never becomes true.
  Restart-taker stalls occur with and without MATCH_UP (seed 3: 4 vs 13 stalemate lines, seed 11: 8 vs 1, longest 5-17 s; seed 7: 86 s with MATCH_UP, 5 s episodes without), so the shape is latent and trajectory-exposed.

## Smoke attribution (seed 7, 2 min, pes-loop; all hashes measured on 23aed782-based trees, outside slots)
| tree | hash | ticks |
|---|---|---|
| main BD (hand-port press) | `85c7d7c515cf92ae` | (Main's baseline) |
| main BD with `PesPressTick` off (scratch worktree 23aed782) | `cd8aef3a8a3000a9` | 30,462 |
| PressAI with `PesPressTick` off | `cd8aef3a8a3000a9` | 30,462 |
| PressAI, PES press without MATCH_UP (RunMatchUp off) | `5162f3b65166d647` | 47,647 |
| PressAI full (MATCH_UP on; PP {0xFF}x3 and {0}x3 give the same hash) | `ebb29b8eeb18d38d` | 36,957 |
- Everything else on the branch is hash-neutral against main: with the press tick off both trees agree to the bit (`cd8aef3a8a3000a9`). So the whole move from `85c7d7c5` is the press cutover: main's hand-ported presser (PesSecondPresserTick + PesLooseWriterTick) replaced by PES's writer, coordinator, reset (`5162f3b6`) and then the MATCH_UP step (`ebb29b8e`). PP's start value is hash-neutral. "Everything off reproduces 85c7d7c5" cannot hold literally: main's press is the hand port, which the cutover deletes (clean cutover); the ablated pair above is the exact equivalent.

## Left (numbered)
1. Gate GREEN needs RestingBall's fix on main, then: merge main, `PesStall` unchanged on seed 7, rerun GATE fast (pinned `GATE_SLOTS="1 0"`). The other two drift reds (Whistle, CarriedThrowIn) pass on the BD tree.
2. Stand-ins to remove (each `ponytail:` at its site): (a) T+0x558 marking partner words (nearest outfield man marks an opposing last toucher) -> the mark pass FUN_140546FB0 (0x140547814); (b) the anime {+0x1A, +0x1C} pair the MATCH_UP writer's first loop tests (GF: own men {0,1} for the run) -> its undecoded writer (pes-re-record-ids.md 4); (c) T+0xF1 armed always -> event-walk writers 0x140501283 / 0x140501468 (the reset's tail never runs).
3. The resting-ball stall (RestingBall).
4. MA+0x2470C (ball-distance list the order helper reads), concept 0x26 (order words; not how PES starts a press).
5. Writer synthetic states over a base that keeps the ball record page.
6. Delete `gf-PressAI-scratch` and `build-rel` when landed. Also: two merge conflict blocks (universe.json meta, gen_translated SETS) were resolved with a short python script, not the edit tool.
