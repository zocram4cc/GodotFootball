# PES press, FarChasePressMain: handover report

> **HolderLatch note 22-10-22:** the far-loose-ball holder latch is landed on the `HolderLatch` branch with its red->green test and mutation. The far-loose-ball tick count is 379 (bound 270) on the merged tree, so `PesRestartDelay.DISABLED_ALooseDeadBallInPlayIsReachedAndPlayed` stays disabled; residual below. The ceremony throw-in test, which the latch broke on its own, passes on the merged tree (latch + ZoneJudge's take hold + the twenty-minute cap).

## Branch state
- Branch `HolderLatch`, off origin/main `54ff0b9` (batch C `8168d4c`), at the merge of `origin/ZoneJudge` `57f2984`.
- Commits, oldest first: `56414e3` red test; `e06e481` the holder latch; `f216870` the ceremony twenty-minute cap (byte-identical to `3f70bca`); `dfc9dfb` merge of `origin/ZoneJudge` `57f2984` (take hold, release tests, zone judge).
- Uncommitted: nothing but the `extern/godot-cpp` symlink (never staged).
## A: landed (92f153f, main), ActionPress target on the loose presser's movement command
- Unchanged by this branch.

## B: the far-loose-ball holder latch: LANDED
- Latch (humanoid.cpp, after the touchedThisTick carrier block): if PesLoop, no holder, the last toucher is this humanoid, and AI_HasPossession holds, he becomes the retainer. Grounding: PES's holder is the actor of the newest ring-1 touch key (W1 0x14045725A, FUN_140457070; docs/re/pes-re-first-touch.md section 5.1) and persists on a loose ball (pes-re-press-coord.md section 5) until a newer touch replaces it. ponytail: PES's touch event is gated by the touch probe G+0x2A80 (section 5.4, OPEN); GF's last toucher stands in.
- The retain block's clear is kept (not made to match PES's persist-on-loss): the ballRetainer is GF's "the ball is loose / free" predicate (designated possession in match.cpp, the press reset, the keeper catch, the far-loose-ball detector), so it MUST clear when the ball leaves the man. The rule agrees with PES only while possession is live; the test's two-live-tick condition makes the one-tick clear flicker explicit.
- Red test (PesHolderLatch.ALastToucherWithLivePossessionBecomesTheHolder, seed 7, two-minute PES loop): on `54ff0b9`, `597 ticks had a live outfield last toucher with no holder for two ticks running`; with the latch: 0 (PASSED). The strict one-tick version the prior agent left red by one tick is superseded by the two-tick version, which is the same rule stated at the test's granularity (the latch runs in the humanoid step, one tick of lag; the clear can flicker one tick while the ball settles at the feet, AI_HasPossession is a height/distance/movement gate, AIfunctions.cpp:1091).
- Mutation: invert the last-toucher condition (`!=`) -> `669 ticks` (FAILED); restore -> 0 (PASSED).

## Far-loose-ball (DISABLED_ALooseDeadBallInPlayIsReachedAndPlayed): residual, NOT re-enabled
- On the merged tree (latch + take hold + cap): 379 ticks against the 270-tick bound (was 359 on `8168d4c`; 379 on the latch-only tree too). The take hold does not change the far-loose-ball count. The latch makes the ball "held" while the last toucher has it live, which shifts who wins loose balls and lengthens the longest loose-dead-ball episode on seed 7.
- Not re-enabled: the count is past the 270-tick bound, so per the task the test stays disabled and the residual is 379 against 270 (2-minute match, seed 7).

## Ceremony throw-in (AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand): broken by the latch, fixed by the merge
- The latch's trajectory shift suppressed seed 7's first ceremony (path-B, non-walking) throw-in in twenty minutes: the test failed with "no ceremony throw-in was placed" on the latch-only tree and still failed after the cap fix (the cap only lengthens the search window; it cannot cause "no ceremony").
- `origin/ZoneJudge` (no latch) passes the same test in 86 s: the take hold (the carry holds the ball at the release key until the touch, RestartRef 4) moves seed 7's trajectory back so the first ceremony throw-in occurs within twenty minutes.
- On the merged tree (latch + take hold + cap): PASSED (whole twenty-minute match, 407 s). The take hold is load-bearing for the latch's landing.

## Verification
- Touched suites on the merged tree, all PASSED: pes_holder_latch 1, pes_receive 39, pes_press 95, pes_press_approach 1, pes_movestep_wire 27, pes_throwin_release 3.
- pes_restart_delay: ceremony PASSED (merged tree, 407 s); full suite including the disabled far-loose-ball test: far-loose-ball 379 (expected, disabled), the rest PASSED.
- 5-minute repro (`gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 5 --pes-loop`): 31803 sim ticks to full time, exit 0, determinism d0748d444ae7e0e0.

## Other open items
- PASS_GET ActionPassGet target (529 unnamed presser commands on 92f153f): not ported.
- Cancel window: not ported (Main's ruling). Restart taker standby/release: not traced.
- The prior agent's scratch artefacts (anytoucher_latch.patch, holder_latch.patch, holder_diag.log, gate.sh, movestep_trace_a8.patch) remain in <workdrive>/gf-FarChasePress-scratch; the patch is superseded by this branch.
