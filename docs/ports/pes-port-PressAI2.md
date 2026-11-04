# PES port PressAI (session 3): dispatcher slot reset FUN_140536690 and order-word helper FUN_14062AF10 translated and proven; order-word loop ported; GATE fast RED on four loose-presser tests (no producer of the 0x26 order words) (04-11-26)

Branch `PressAI` head `e23bb4cd` + a docs commit (confirm with `git ls-remote origin PressAI`); worktree `gf-PressAI`, Release dir `build-rel`. Repo copy: `docs/ports/pes-port-PressAI.md` (the full tables). CONFIRMED = tool output; INF = inference; OPEN = blocker named.

## Done
1. `git merge origin/main` (028f0feb) resolved (tests/sim/CMakeLists.txt: both test targets kept), pushed (ad28986f).
2. `0x14062AF10` (order-word helper) and `0x140536690` (the dispatcher's slot reset, whole function: entry store T+0x214, first loop, set-piece tail) translated; proofs = SYNTHETIC states over one full match state (`pid23358/seq032`), `tools/dev/translate/synth_press_order.py`: helper 28 calls bit-exact, own-block 0.8876; reset 56 calls bit-exact, own-block 0.8958. No hook sits on either function and slim captures cannot serve them (pages dropped), hence synthetic.
3. GF: `PesWorld::ResetPress` now runs the translated reset (the hand loop is deleted); `PesWorld::RunPressOrders` = FUN_140535C60's order-word loop (0x140536011-0x140536059), called after the writer in `PesPressTick`.
4. Proofs widened with real captures (`tools/dev/translate/proof_press_captures.py`): eval 3 -> 28 (0.6218), coordinator 3 -> 80 (0.5446), both bit-exact, records and closure files updated; writer: all 101 real captures of both launches replay bit-exact (0.2882) but the committed writer record (23 calls incl. 16 synthetic, 0.4412) is kept, because the 16 synthetic states cannot be rebuilt on today's slim captures.
5. Layer regenerated (4,864 functions), `gen_translated.py` clean; drift test passes in the gate.
6. Tests: `pes_press_world_test` 20 pass (12 new). Mutations run (rebuilt, reverted): PRESS/DELAY mapping swapped; PP offset 0x54 -> 0x58; already-PRESS/DELAY skip removed; helper index k instead of side*11+k (the first two applied together with the others masked each other; re-run apart). Not run: the order-word compare.

## Gate
`GATE fast: RED PesRestartDelay.PesLoopRestartsAreArmedAtTheirPesSpotsSidesAndReleaseFrames PesRestartDelay.ALoosePresserInTheTrapWindowPlaysThatRowsClip PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot PesPressApproach.ALoosePresserCommandNamesTheActionPressTarget` (2220 ctest tests, 4 failed; recorded re-check 0 selected; ledger and drift pass).
Smokes (seed 7, 2 min): switch-off `3b73c968b4a377e0` = main baseline; pes-loop `429656652892a0b9` (no abort). The first test run apart: "steered 0: no loose presser was steered in the match".

## Root cause of the RED (CONFIRMED for the first test, INF for the rest)
Nothing in GF starts a fresh loose-ball press since the press cutover: the writer only renews a man whose word is already PRESS; the coordinator's class-5 needs an opponent holder; the order-word loop needs T+0xB748 == 0x26, which team concept 0x26 (+0x7B8, end 575EE0, unported) writes; the reset's tail needs T+0xF1 == 0 (staged armed). 0 of 181 captured writer/coordinator calls hold 0x26 in any slot (B748 = 0x30, two with 0x24), so PES may start loose-ball presses another way (7 of 181 hold a 0x34 word).

## Left (numbered)
1. Producer of the loose-ball press start: either port concept 0x26 (start, end 575EE0, slot assignment) or a `ponytail:` stand-in naming the man (invents PES's pick). A hook on FUN_140A42940 with `--when r8d == 0x34` (and the caller) would show how PES starts a press at a loose ball: ask Main. Then re-stage the four tests by census (never weaken) and re-run GATE fast.
2. MA+0x2470C list writer (the helper reads man 0 repeated in GF).
3. Reset tail never runs in GF (T+0xF1 armed stand-in); hook on FUN_140536690 / FUN_14062AF10 would give real calls.
4. Writer synthetic states over a base that keeps the ball record page.
5. Delete `gf-PressAI-scratch` and `build-rel` when landed.
