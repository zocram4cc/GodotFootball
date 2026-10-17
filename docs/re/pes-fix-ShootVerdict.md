# ShootVerdict: shootout verdict vs plan

Result: Partially done (diagnosis + disabled red test; fix over budget).

SHA: e4f6b3d, pushed to refs/heads/ShootVerdict (confirmed by ls-remote).

Cause: the defending keeper is never driven to the planned save. The plan (penaltyshootoutcontroller.cpp:189-197) only shapes the taker's aim (elizacontroller.cpp:231-237); no keeper-side code reads it. The verdict is the ball's, so a planned Save that the keeper does not stop is recorded as GOAL.

Test: SimMatch.DISABLED_ShootoutVerdictIsThePlannedOutcome (tests/sim/sim_match_test.cpp), seed 7, shootout-only match. Red with --gtest_also_run_disabled_tests: kick 2 aimed for SAVED!, ball at x=52.68 y=0.397 z=0.380 goes in, recorded GOAL, FAILED (26 s). Default run: disabled, 0 run.

Mutation check: not applicable; the fix is not made. No production behaviour changed; only GetPlannedOutcome() added.

Verification: sim_match_test built (RelWithDebInfo, worktree <workdrive>/gf-ShootVerdict) with no gate run and no gf-sim lock taken.

Left: PES's keeper penalty logic, to drive the defending keeper to the planned outcome; then enable the test. Diagnosis in docs/re/pes-re-shootout-verdict-mismatch.md.
