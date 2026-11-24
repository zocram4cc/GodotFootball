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

## 9. Round 2 (Main's IRC: wire ActionPress now, land only at both ratchets)

**Status: NOT LANDED.** Wiring reverted to the merged tip. Branch `PressLane4` pushed at `f95743c1` (merge of origin/Integrate `c4404052`,
verified by `git ls-remote`), with NO behaviour change. `gen_translated.py --image pes_full.bin --check` on that tree: 10,990 functions, byte-identical.
Wiring patch kept at `<workdrive>/gf-PressLane4-scratch/actionpress_wip.patch` (416 lines, src and tests only).

### 9.1 Measured on the merged tip (`f95743c1`, no wiring; the reference)
- `PesStallQuick`: **53** (bound 53) on this tip, 339 s. PASS.
- `PesStall.ALiveBall*`: PASS (bound 2229), 1318 s. The printed count only appears on failure; the dump-line count (4074) does not reconcile with a PASS and is UNRESOLVED. Re-measure with the count printed before relying on the 2229 headroom.
- `PesPressApproach.ADesignated...` on this tip: RED, 2 of 369 (422 s, round 1, unchanged code).
- `PesThrowInRelease.TheThrowInTakersAnklesStayBehindTheLine`: PASS on the earlier tip (796 s); not re-run on the merge.

### 9.2 Wiring built (in the scratch patch, reverted here)
- `PesWorld::RunActionPress(slot)` runs `pes_fn_140962440` on the slot object's A0 (X+0x10E70) with A2 (X+0xACE8), rdx = out, r9 = A2+0x50,
  writes A2's slot id, ctx and owner (P) each call (pes-re-press-owner.md 4 item 3). Latches persist in A0.
- `ElizaController::_ActionPressTarget()` runs once per frame (cache by sim frame); the movement section's `pressMan` (loose presser, or the
  designated man with no holder who stands in for the press) queues `_PressApproach(target)` and skips `_MovementCommand`. The hand port
  (`PressTargetIsBall`, `PressTargetTick`) and its tests are deleted; `_MovementCommand` loses its `desiredTarget` parameter.

### 9.3 Blockers found while wiring (CONFIRMED in the run, each with its VA)
1. **G+0x3B0 skip pointer never stored.** ActionPress reads its match object through G+0x3B0 (FUN_14234E8E0 reads obj+0x8C). The window
   held the object but not the pointer. Fixed in the patch (`StoreU64(Bytes(), kSkipPtrOffset, SkipVa())`; the header already says GF holds the
   object with bytes clear). CONFIRMED: first crash `PES_DISPATCH_UNTRANSLATED: 0`, gone after the fix.
2. **P+0xAC0 speed-profile interface vtable not loaded.** The planner 0x1406B9270 calls through P+0xAC0 (the P ctor stores vt1 = 0x1425E35B0,
   `LoadActorConstants`, site 0x1406E0420). Its bytes were not in any mapped segment: slot reads returned 0. Fixed in the patch
   (`ReadActorInterfaceVtable` in pesimagetables.cpp, from the install).
3. **Interface vtable span 0x90 too short.** The solver 0x140705BF0 calls slot +0x90 (translated line 416). Span widened to 0x100 (pesimagetables.hpp).
   After 1-3, ALoosePresserCommandNamesTheActionPressTarget **PASSES** in normal mode (711 s, no dispatch abort).

### 9.4 Result with the wiring (CONFIRMED, measured)
| test | merged tip (no wiring) | wired (patch) | bound |
|---|---|---|---|
| `PesPressApproach.ALoose...` | RED (21 of 10,022, round 1) | **GREEN** (711 s) | green |
| `PesPressApproach.ADesignated...` | RED 2 of 369 | RED **30 of 380** (907 s) | green |
| `PesStallQuick` forced live | 53 | **6623** (705 s) | <= 53 |
| `PesStall.ALiveBall*` forced live | PASS (<= 2229) | **19247** (2101 s) | <= 2229 |

- Quick census (wired): forced men are the designated stand-in (man 2, H3, 7, H8, 6) with **named** commands (usetgt=1) whose target
  is **0.2 m from the man** (e.g. t=154444 ms: `cmdv=1.39 usetgt=1 tgtdist=0.26`, ball 5.6 m away). ActionPress's target is his
  BASE, not the ball: FUN_140639100 takes the base when the ball sample at k=5 ticks sits nearer it (press-life §7).
- Cause of the base: ActionPress reads the presser's base from the window's AI record (AI+0x1414/18/1C) and GF writes no such anchor
  (`kSlotAnchorXOffset` 0x1414 is declared; no writer). With no writer the base is zero, so a far designated man walks to a point beside himself.
  The old hand port used `GetAdaptedFormationPosition` as its base (ponytail); the translated path needs the same anchor written.
- ADesignated (wired): 30 unnamed. Cause is the GF designation order: `RecomputePesDesignatedOwner` and the designated-man choice run AFTER the
  team ticks (match.cpp ~960-980), so a man designated this tick decided last tick as a non-designated man (magnet blend). INF for the
  unnamed rows' exact share; CONFIRMED that the designated stand-in decides from last tick's designation.

### 9.5 Why it is not landed, and what it needs
- The change makes both ratchets far worse (6623 and 19247 against 53 and 2229). Under the rule "land or revert", it is reverted.
- The remaining work, in order:
  a. Write PES's presser anchor (AI+0x1414/18/1C, the man's base) from the formation. Writer undecoded: this is a ponytail stand-in
     (GetAdaptedFormationPosition, as the old port) until PES's writer is found. Rule 8 needs the proof that PES's writer is out of reach.
  b. Decide the designated stand-in's scope. PES's writer marks no PRESS man on a free ball (0 of 4527 seed-7 ticks). If the stand-in
     stays, ActionPress should run only where PES would (PRESS word), not for every designated man: otherwise it multiplies the
     breaker's stall (the numbers above).
  c. Designation order (ADesignated): either move the designated recompute ahead of the team ticks (PES's order for the gate 4c owner) or
     name the stand-in from the same-tick designation. Measure the ratchets after each; the pre-fix tip is the reference.
  d. Only then: the press tick 0x140928FC0 (Main's core record, still blocked by 0x14063DE20/DD20) and deletion of the walk for the press man.
- Mutation checks: not run (nothing landed). Gate: not run.

### 9.6 Budget and rig
- Rig requests: 0. Sims only through `simslot.sh`, each with a timeout. Concurrency: one or two sims at a time.
- Scratch: `<workdrive>/gf-PressLane4-scratch/` (w1..w4 wired binaries and logs; `actionpress_wip.patch`; `m_base/` merged-tip baseline logs).
- Worktree: `extern/godot-cpp` is an empty dir for git (symlink only during builds). Clean at `f95743c1`.
