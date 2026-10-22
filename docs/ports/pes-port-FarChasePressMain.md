# PES press, FarChasePressMain: handover report

> **Main note 22-10-26:** branch FarChasePress2 `92f153f`, landed on main in batch C `8168d4c` (gate green); on main the far-ball test is 359 ticks against 270. The holder latch is a patch in scratch, red by one tick; the decision is in tasks/22-10-26.md.

## Branch state
- origin/FarChasePress2 = 92f153f (A landed and pushed). Nothing else is committed.
- Worktree HEAD = 92f153f. Uncommitted: the B latch in humanoid.cpp, the CMake target, and tests/sim/pes_holder_latch_test.cpp.
- Patch of the uncommitted tracked changes: <workdrive>/gf-FarChasePress-scratch/holder_latch.patch (checked: `git apply --check -R` matches the tree). Test copy: <workdrive>/gf-FarChasePress-scratch/holder_latch_test.cpp.
- Latch not landed: its gate is not green.

## A: landed (92f153f), ActionPress target on the loose presser's movement command
- Red on f3aa913: 7183 of 7471 loose-presser commands named no target.
- Mutation (target dropped, scoped test): 5536 of 5536 named no target, exit 1. Restored: PASSED 1.
- Gate on 92f153f: build 0; wire, restart_taking, press, clips, sim_heading 0; enabled 2/2; trap 1/1; smoke plain bed0bbd00dd506b2 (unchanged), loop f89cf25ffafdc923.
- DISABLED longest episode: 631 (f3aa913) -> 737 (92f153f), bound 270. Kept as faithful; cause not traced.

## B: any-toucher holder latch (approved by Main, conditioned on live possession): NOT landed
- Latch (humanoid.cpp, after the touchedThisTick carrier block): if PesLoop, no holder, the last toucher is this humanoid, and AI_HasPossession holds, he becomes the retainer. Grounding: PES's holder is the actor of the newest touch key (W1 0x14045725A; docs/re/pes-re-first-touch.md section 5.1). ponytail: PES's touch event is gated by the touch probe G+0x2A80 (section 5.4, OPEN); GF's last toucher stands in. The loose-ball predicate is unchanged.
- Red test (PesHolderLatch.ALastToucherWithLivePossessionBecomesTheHolder, seed 7, two-minute PES loop): on HEAD, `578 ticks had a live outfield last toucher with no holder after two live ticks`. With the latch: 1.
- Residual, diagnosed to one event: tick 327037, player A11. Trace (<workdrive>/gf-FarChasePress-scratch/holder_diag.log): `[hviol] t=327037 p=A11`; `[hstep] t=327037 p=A11 live=0`, so the latch step saw no possession. No latch print for A11 at tick 327036, so someone else held the ball at that step or he did (not decoded).
- Inference, not verified: the retain block cleared the holder on a step where AI_HasPossession was false, possession returned after that step, and it was false again at the next latch step. The two-tick test counts this as a miss.
- Not run: the mutation, the full gate, and DISABLED with the latch. The experiment without the test (same condition) measured DISABLED 737 -> 419 (anytoucher_latch.patch, bound 270). Not a pass.

## Open decision (Main)
- (a) The retain block's clear should not drop a holder on a one-tick flicker. This changes GF's clear rule, so it is a scope question.
- (b) The test tolerates a one-tick flicker, with that reason written in the test.

## Next concrete action
1. Confirm the flicker: add one temporary env-gated print at the retain block's clear (the `SetBallRetainer(0)` under the CarrierAfterContact check), run the holder test, and look at ticks 327036-327037 for A11 (who held the ball, and whether AI_HasPossession was false at the clear). Remove the print afterwards.
2. Decide (a) or (b) with Main.
3. Then: mutation (invert the last-toucher condition) red, restore green; `cmake --build build -j2`; gate (`sh <workdrive>/gf-FarChasePress-scratch/gate.sh <tag>`, which must also list pes_holder_latch_test and pes_carrier_test); DISABLED. Commit B only if green; otherwise keep it as the patch.

## Other open items
- Batch B (DodgeGate2, MergeBatch4 a54c22e) is not on this base. After it lands: merge main into FarChasePress2, then re-measure DISABLED and the enabled pair on the real base.
- PASS_GET ActionPassGet target (529 unnamed presser commands on 92f153f): not ported.
- Cancel window: not ported (Main's ruling). Restart taker standby/release: not traced.

## Scratch artefacts
- anytoucher_latch.patch (experiment, DISABLED 419), holder_latch.patch (the B latch, uncommitted), holder_latch_test.cpp, holder_diag.log, gate.sh (fixed list; add pes_holder_latch_test), movestep_trace_a8.patch (a reverted trace).
- The older report pes-port-FarChasePress.md (earlier round) is untouched.
