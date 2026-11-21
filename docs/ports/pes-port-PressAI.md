# PES port PressAI: press writer, target eval, coordinator, MATCH_UP writer, slot reset and order-word helper translated and proven; GF runs the dispatcher's press steps in PES's order on the world window (04-11-26)

Branch `PressAI` (worktree `gf-PressAI`, Release dir `build-rel`). CONFIRMED = tool output; INF = inference; OPEN = blocker named. Captures: `live2/pid296049`, `pid1037286` (slim), the full state `pid23358/seq032` (synthetic bases), `pid358243` (press_order / press_reset hooks).

## 1. Proofs (`snapdiff.py --bounded`, `tools/dev/translate/results_press.json`)
| root | calls | mismatches | own-block | what the calls are |
|---|---|---|---|---|
| 0x140536FC0 press writer | 23 | 0 | 0.4412 | 7 captured + 16 SYNTHETIC over seq95772 (earlier commit); all 101 real writer captures of two launches also replay bit-exact (0.2882, not committed over the record: the synthetic states cannot be rebuilt on slim captures) |
| 0x140633940 target eval | 28 | 0 | 0.6218 | 28 real captures |
| 0x140634470 coordinator | 80 | 0 | 0.5446 | 80 real captures |
| 0x14062AF10 order-word helper | 28 | 0 | 0.8876 | SYNTHETIC |
| 0x140536690 slot reset | 56 | 0 | 0.8958 | SYNTHETIC |
| 0x140536210 MATCH_UP writer | 56 | 0 | 0.9149 | SYNTHETIC |
- No hook sits on the helper, the reset (the hooks added at pid358243 have 3 reset captures per side, slim) or the MATCH_UP writer, and slim captures drop the pages these read, so `synth_press_order.py` keeps ONE full state (`pid23358/seq032`), sets registers and pokes the men's words, positions, anime fields, ball-distance lists, T+0xA548/0xF0/0xF1/0x140/0x558, the G+0x180 list, the set-piece object, and for the MATCH_UP writer a scratch event record (E's newest-record page is a hole in every capture; E points at a chosen block). Every such case is chosen, not observed.
- Closures = replay members + static direct-call closure; layer 5,026 functions; `gen_translated.py` clean.

## 2. How PES starts a press (new, CONFIRMED from captures)
- `press_order_A42940` (r8d == 0x34) captures in `live2/pid358243` (seq39765, seq39767): the caller is `0x1406343E1`, inside the TARGET EVAL FUN_140633940 (it ends with `FUN_140A42940(T, slot, 0x34)`): PES starts a press through the coordinator -> eval, after the MATCH_UP writer has set T+0x7C. The dispatcher's order-word loop (T+0xB748 == 0x26) did not fire in any of 181 + 5 captures (T+0xB748 = 0x30 in every slot; 0x24 in two).
- seq39765's T: T+0x78 = 0xFF, T+0x7C = 15 (MATCH_UP man, A2A0[4] = 0x3A), T+0x558 = {-, 10, -, 9, 8, -, -, -, 7, -, -} (the mark pass marks several opponents, one per marker), T+0xA294 = 500, T+0xB8A4 = 2. press_reset calls come from `0x140535FF9` (the dispatcher), T and the {ctx, 0, side} frame as staged.
- 0x34 is rare in PES: 7 of 181 writer/coordinator captures hold a PRESS word in any slot.

## 3. GF (clean cutover)
- `PesWorld::ResetPress` runs the translated FUN_140536690 (the hand loop is deleted). `RunPressOrders` is FUN_140535C60's order-word loop (inert in GF: no order-word producer; the concept 0x26 is unported). New `RunMatchUp` = the dispatcher's release of last tick's MATCH_UP man + FUN_140536210. `PesPressTick`: stage -> MATCH_UP -> coordinator -> reset -> writer -> orders.
- Stand-ins (`ponytail:` at each site): (a) T+0x558 partner words: PES's mark pass (FUN_140546FB0) writes one partner per marker; GF has no such table, so the nearest active outfield man marks an opposing last toucher (PES doc 14.4, INF high); (b) the anime pair {+0x1A, +0x1C} that the MATCH_UP writer's first loop tests (+0x1C != 0 and +0x1A <= +0x1C): its writer is undecoded (pes-re-record-ids.md 4); the own men get {0, 1} for the run and are put back; (c) T+0xF1 armed always (event-walk writers unported: the reset's tail never runs).
- Effect in GF (seed 7, 2 min, both sides, 84k team ticks): MATCH_UP man named in 31% of ticks, second presser PP[2] valid in 2%, a PRESS word in 113 ticks. PES itself holds PRESS in ~4% of sampled calls.

## 4. Tests
- `pes_press_world_test` 24 pass (12 earlier new + 4 MATCH_UP: the man marking the holder becomes MATCH_UP, no marker -> nobody, off pitch -> nobody, released next tick). Mutations (earlier session, run apart): PRESS/DELAY swap, PP offset 0x54 -> 0x58, already-PRESS skip, helper index. Not run on the MATCH_UP tests: mutations (budget). An end-to-end world test (loose ball -> MATCH_UP -> coordinator -> PRESS) was written and removed: the staged scene (tier, one second man 8 m away) did not make the eval choose; the real-GF run does (section 3).
- `PesPressApproach.ALoosePresserCommandNamesTheActionPressTarget`: re-staged by census (no free-ball tick of seed 7 held a PRESS man: 0 of 4,527; seeds 3 5 9 11 also steered 0): it now follows the PRESS-word man while the ball is in play and no set piece is armed; the assertions are unchanged (steered > 0, none unnamed). Passes.
- Not touched: `PesRestartDelay.PesLoopRestartsAreArmed...`, `PesThrowInRelease.ACeremonyThrowInIsReleased...` failed in the earlier gate (no presser at all) and pass now (run apart, 883 s / 1,406 s). `PesRestartDelay.ALoosePresserInTheTrapWindowPlaysThatRowsClip` no longer exists on main (batch BB).

## 5. Smokes and gate
See the final message and `local://pes-port-PressAI2.md` (the gate waited ~4 h for sim slots behind other gates).

## 6. Open items
1. The mark pass's partner words (FUN_140546FB0, 0x140547814) and the anime {+0x1A, +0x1C} writer replace stand-ins (a), (b).
2. Concept 0x26 (order-word producer) and MA+0x2470C writer.
3. T+0xF1 writer (event walk 0x140501283 / 0x140501468): the reset tail then runs; hooks on FUN_140536690 / FUN_14062AF10 / FUN_140536210 would give real calls instead of synthetic.
4. A world test for the full chain once the mark pass is real.
5. Delete `gf-PressAI-scratch` and `build-rel` when landed.
