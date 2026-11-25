# RegressA: throw-in / restart reds on Integrate e154ed687 (WalkLane 66f2930d7 merge)

Scenario: none landed. Result: cause located in WalkLane's pair-level strike exemption; NO faithful fix in this run; the revert fallback is measured and REJECTED (it breaks the stall ratchet). Branch `RegressA` = 339fb968c (pushed, ls-remote confirmed) holds the revert as evidence only. NOT mergeable.

## 1. A/B (seed 3 = SEAMLESS_SEED; same env PES21_ROOT=<workdrive>/PES21)

Worktrees: `<workdrive>/gf-RegressA` (e154ed687), `<workdrive>/gf-RegressA-base` (1d682f19a, parent). Same build setup; only the merge differs.

| test | 1d682f19a (base) | e154ed687 (Integrate) |
|---|---|---|
| PesRestartDelay.AThrowInIsReleasedFromItsSpotAndTaken | RED: taker root 0.74888 m from spot, limit 0.65 (362 s) | RED: 1.2608 m (brief value; this test was not re-run to its end; the carry test below is the same match) |
| PesRestartDelay.AThrowInIsCarriedToItsSpotAndReleasedThere | RED: 0.74888 m vs 0.65 | RED: 1.26080167 m vs 0.65 (diag run, 156 s) |
| PesRestartDelay.PesLoopRestartsAreArmedAtTheirPesSpotsSidesAndReleaseFrames | GREEN (346 s) | RED: 109 frames vs gate 134, "released early for set piece 1" (1792 s) |
| pes_throwin_release_test (9 tests) | 9/9 PASSED (2795 s; ACeremony...HandOverTheSpot 1792 s) | 6 OK, then ACeremony... still running at the 3600 s cap (test 1: 1566 s vs 1001 s on base) |

CONFIRMED: the carry reds already fail on 1d682f19a (0.749 m, same match). The merge makes the carry offset worse (0.749 -> 1.261 m). The frames test is the only red the merge creates in these three.

The throw-in suite is not a hang on Integrate (it is still inside ACeremony), but it is slower: 1566 s vs 1001 s for test 1. Machine load was not controlled, so this is not separated from load. The ACeremony test is the slow one on both sides (base 1792 s).

Translated-layer check (`gen_translated.py --image pes_full.bin --root . --check`): RED on Integrate (11193 drifts, 11191 functions) AND identical on 1d682f19a (11193 drifts). Pre-existing; not caused by the merge (the merge touches no translated file or generator).

## 2. Mechanism

CONFIRMED by headless decode (`dec_composer.txt`, `dec_adjuster.txt`):
- FUN_141AFD400 (VA 0x141AFD400): while B+0x81 (strike valid) is set, the layer root = B+0x90/94/98 (store block).
- FUN_1406EEDF0 (VA 0x1406EEDF0): B+0x81 = PES root adjuster P+0x428 active; B+0x90.. = its output.
- FUN_140791EC0 (VA 0x140791EC0, throw-in sequencer): arms P+0x428 toward the carry spot S (FUN_141EB39A0). GF never calls it: GF's carry/lift warp is `AdvancePieceWalk` -> `WarpRoot`.

REJECTED (per-tick DIAG, env-gated, reverted):
- The strike-root override (SetStrikeRoot) does NOT act on the throw-in taker: `valid=0` on every tick of the stoppage. My first hypothesis (the composer pins the taker to a stale B+0x90) is wrong for this taker.
- The bounce-pass exemption never touches the taker in the stoppage (no push with force on him).

CONFIRMED by mutation M-a (pair-level exemption removed, all else as merged; `match.cpp:1590-1591` back to plain `OffsetPosition`):

| metric | merged (Integrate) | M-a | bound / base |
|---|---|---|---|
| PesLoopRestartsAreArmed... frames | 109 (RED) | PASSED (green) | gate 134 |
| AThrowInIsCarried... taker offset | 1.2608 m | 0.9498 m (RED) | limit 0.65; base 0.749 m |
| PesPassTarget.ATrapsRoot worst root gap | < 1 mm (green) | 21.5 mm (RED) | 1 mm |
| PesStallQuick forced_live | 49 (bound) | 477 (RED) | bound 49 |
| PesStall.ALiveBall liveFirings | 479 (bound) | 2462 (RED) | bound 479 |

Mechanism (INF): the pair exemption makes a strike-valid opponent not yield, so the non-valid throw-in taker absorbs the full pair separation tick after tick. M-a lowers the carry offset (1.261 -> 0.950), which supports this; it does not reach the base 0.749 m.

## 3. Fallback decision

The revert of the pair exemption (M-a) is REJECTED: it moves the stall ratchet from 49 to 477 (Quick) and 479 to 2462 (ALiveBall), breaks ATrapsRoot (21.5 mm), and still leaves the carry red. The exemption is load-bearing: it is the thing that holds the merged stall numbers down. Main should not merge M-a. Do not treat WalkLane as "the cause to revert".

## 4. Open: what is needed for the faithful fix (not done)

The faithful fix is PES's per-man contact share. FUN_1408C48C0 (VA 0x1408C48C0, per-man contact handler; writes P+0x624 and the anime root offset ap+0x248C, which the composer never reads) and its helper FUN_140A6D670 (decoded to `dec_push.txt`, not yet read in full). GF's pair push stands in for it (ponytail in match.cpp). The share's gates (FUN_1408C2760, FUN_1408C31C0, FUN_1408C26D0) are not decoded. Porting it is a multi-step lane and did not fit this run.

## 5. Gate and push

- Branch `RegressA` = 339fb968c, pushed (`git ls-remote origin RegressA` confirms). M-a revert only. NOT for merge.
- Fast gate (`GATE_TIER=fast`): NOT run. The throw-in suite is not complete on Integrate, and the translated-layer check is red on both sides (pre-existing).
- Test binaries run: pes_restart_delay_test (3 filters), pes_throwin_release_test (partial, 6 of 9 on Integrate; 9/9 on base), pes_pass_target_test (ATrapsRoot filter), PesStallQuick, PesStall.ALiveBall.
