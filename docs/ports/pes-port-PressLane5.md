# PressLane5: tip baseline, the rethink census, ADesignated verdict, and why the rethink cannot land

Branch `PressLane4`, worktree `<workdrive>/gf-PressLane4`. Tip `6f982593` (Integrate `82ad8564` merged). This round made **no commit and no push**.
The worktree is clean at `6f982593`. Scratch: `<workdrive>/gf-PressLane4-scratch/` (outputs `base_*.out`, `fix_*.out`, `build_*.log`).
Rig requests: 0 (no live PES use). Gate: not run (Main gates). `check_pii`: not run (nothing pushed).

## 0. Result in one table (all runs on this tip, measured this round unless marked)
| test | baseline (HEAD, no fix) | rethink patch (`rethink_fix.patch`) | ratchet / red bound |
|---|---|---|---|
| `PesStallQuick.ALiveBallNeverLiesStillInTheQuickWindow` (forced live, 12960 ticks) | **53** PASS (327 s) | **1242** FAIL (298 s) | <= 53 |
| `PesStall.ALiveBall*` (forced live, full match) | **2229** PASS (1158 s) | **2340** FAIL (1160 s) | <= 2229 |
| `PesPressApproach.ALoosePresserCommandNamesTheActionPressTarget` | RED: 21 of 10,022 (previous round; not re-run) | **GREEN** (476 s) | must be green |
| `PesPressApproach.ADesignatedManWithNoHolderIsCommandedToTheRestingBall` | RED: **2 of 369** (422 s) | RED: **15 of 1187** (426 s) | must be green |
| `PesPressApproach.ANoManWhoDoesNotHoldTheBallIsDecidedByTheDribble` | not run | GREEN (423 s) | green |
| `PesThrowInRelease.TheThrowInTakersAnklesStayBehindTheLine` | PASS (796 s, 3000 s slot) | PASS (1283 s, 3000 s slot) | green |

- The tip is **at both ratchets exactly** (53 and 2229): zero headroom. Any rise fails.
- The rethink patch is not landable: it fails both ratchets and leaves ADesignated red.
- The ADesignated "timeout" from last round was load, not a hang: both variants finish in about 7 minutes now.
- The rethink patch is still **not** in the tree (reverse-applied; the tree is clean at HEAD). Patch kept at `gf-PressLane4-scratch/rethink_fix.patch`.

## 1. Working-tree state found at the start (resume warning)
- The previous round's patch was already applied when this round began, so its earlier "baseline" builds were patched. This round
  reverse-applied it, rebuilt the true HEAD baseline (`bin_base`), then reapplied it (`bin_patch`), and finally reverse-applied it again.
- `git` refuses every command while `extern/godot-cpp` is a symlink (gitlink check). Swap it out around git ops, then restore it for builds.

## 2. Census: which men the breaker forces, rethink vs baseline (CONFIRMED from `GF_STALL_DUMP` lines, quick run)
| run | forced live | forced men (dump lines) | forced commands with `usetgt=0` (no named target) |
|---|---|---|---|
| baseline | 53 | man 8 (24), man 3 (23), man 25 (4), man 2 (2) | 51 of 53 |
| rethink | 1242 | man 7 (396), man 5 (335), man 2 (257), man 26 (140), man 19 (68), man 3 (20), man 9 (15) | 770 of 1242 |

- The breaker (`match.cpp` ~991-1006, the stalemate recovery) designates the closest man across both teams when no man holds the ball.
  The ball is at rest (0.08 m/s). Every forced man is the **designated man** (`GetDesignatedPossessionPlayer`).
- The forced men's commands are **unnamed movements** (`useDesiredTarget` false). Example, rethink run, t=155000 ms: man 7 is 0.4-0.5 m from the ball,
  `cmdv=8.00 usetgt=0 tgtdist=22.8`, body 4 m/s, for 150 ms: he circles the ball and never takes it.
- **Correction to the draft.** The unnamed command for a designated non-holder is not the `_BallControlCommand` walk. It is
  `PlayerController::_MovementCommand` (`playercontroller.cpp` 561-764): the auto/manual blend from `AI_GetToBallMovement`
  (line ~630), which sets `useDesiredTarget` only when a caller passes a target (line 758). The `_BallControlCommand` walk
  (`AI_GetBallControlMovement`, line ~302) runs only for a keeper rush or a holder in PES loop (`elizacontroller.cpp` ~830). Which
  caller emits each forced command is INF (not traced per tick).
- The rethink adds forced men on the loose PRESS side: man 26 at t=155148 ms is the loose presser (`forcedIsPresser=1`), 2.3 m from
  the ball, `usetgt=1 tgtdist=0.00` (his target is the ball), still in a sliding clip. The re-think gives him a target, but the breaker still fires.

## 3. What PES does in that state (CONFIRMED, `docs/re/pes-re-press-life.md` section 8; `pes-re-first-touch.md` 7.1-7.2)
- ActionPress `0x140962440` runs **every frame** for a PRESS man (state 0x34). It always writes a target: ball-now or his base, chosen by
  `FUN_140639100` (ball sample at k = 5 ticks vs presser base). There is no think to go stale.
- Latching: the stored point `action+0x68` is copied to `out` each frame and written back when it differs (steps 3 and 8); on a touch
  frame the stored point steps 5 m along man-to-target (`0x14259222C`, step 5); the arrival writer `FUN_1406378F0` owns `action+0x78`
  (step 7); the planner ETA flag `action+0x30 = (n + 10 < eta)` (step 6).
- A loose ball is reached by the reaction's TRAP (`pes-re-first-touch.md` 7.2), not by a walk. GF's blend has none of the latches above.
- The rethink patch re-runs `Think` (the controller's side effects repeat, e.g. the dodge tally) and has no latch. It moves the
  decision, not the cause. That matches the census: the forced men change but the count goes up.

## 4. ADesignatedMan verdict: RED, not a timeout
- Baseline: 2 of 369 designated-man movement ticks in reach name no target (422 s). Patched: 15 of 1187 (426 s). The rate went up.
- Its unnamed cases are the designated man's non-holder blend (section 2). PES names a target for him (ActionPress). The fix is that
  wiring, not the re-think. The test is kept as is: no weakening.

## 5. Step 4 (press tick 0x140928FC0, `RunActionPress`, delete the walk): NOT STARTED, blocked
- Blocked on Main: the core record that clears `0x1409F6960` (translator bug row in `results_core.json`, see `tasks/22-11-26.md`). No message from Main has arrived.
- Ready in the layer: `src/pes/translated/ai/pes_fn_140962440.cpp` (ActionPress) and `src/pes/translated/impure_sample/pes_fn_140639100.cpp` (target);
  both are in `pes_dispatch_gen.cpp` (cases 0x140639100, 0x140962440). No GF caller (grep). The wiring lives in `PesWorld` with the press tick.
- Not done yet, on purpose: deleting `AI_GetBallControlMovement`'s use for the designated man now would leave him with no command. The ADesignated test counts a man with no command as unnamed (`pes_press_approach_test.cpp` 108-117; it skips only a man whose command is a non-movement action), so the test would turn RED on that tick. Deleting the walk without the ActionPress target is therefore a regression, not a fix. Rejected.

## 6. Ankle (keep green)
- Baseline PASS in 796 s; rethink PASS in 1283 s, both on a 3000 s slot. Green on both.

## 7. Next exact step (for Main)
1. Land the core record that clears `0x1409F6960` (Main). Then re-record `results_core.json` and enter the tick `0x140928FC0` into the layer.
2. In `PesWorld`: stage the PRESS slot's ActionPress context and call `pes_fn_140962440` once per PRESS man per frame; the command takes its
   target (named). Red first: the ADesignated and ALoose tests (both must go green), then delete the `AI_GetBallControlMovement` use for the press man in the same commit.
3. Only then rerun both ratchets on the tip (PesStallQuick <= 53, ALiveBall <= 2229), and the ankle.
Rethink patch: do not land. It measures 1242 (quick) and 2340 (ALiveBall), both over their bounds.

## 8. Open items (INF unless marked)
- Which caller emits each forced unnamed command (`_MovementCommand` vs set-piece walk) per tick: not traced. INF.
- Whether ActionPress alone brings the forced count below 53: unknown until wired (the census would then answer it).
- The ALoose baseline count (21 of 10,022) is from the previous round, not re-run.
