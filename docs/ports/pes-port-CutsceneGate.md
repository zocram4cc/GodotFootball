# CutsceneGate: CutsceneTiming landed green: the two throw-in reds and both smoke-hash moves were one constant (the 6 s scorer approach cap); fast gate GREEN; proc camera engine proven on 6 captures (31-10-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`, build dir `build` = the CMake default RelWithDebInfo, absolute path to the gate). Labels: CONFIRMED = run this session. Scratch (PES-derived census, replay output) stays in `gf-CutsceneGate-scratch/`, never committed. All sims ran at `nice -n 19` directly (slots contended); the gate held slots 3 2.

## 0. Status

| ticket item | result |
|---|---|
| 1. merge origin/main, push | DONE three times (`77933429`; batch AS `99861bb1` with the layer regenerated on the merged tree, 4,810 functions, `--check` byte-identical, ledger regenerated, universe.json closures meta keeps `cardlevel` + the `cutanchor*` sets; batch AT/AU `2d8b2907`, conflict-free). |
| 2. the two reds | **Seed drift of a branch-wide behaviour change, not a stall of the branch's code**: both go green with one fix (section 1). A latent main weakness the drift exposed is documented (section 2), not fixed. |
| 3. smoke hash move | **Explained and removed** by ablation: the one cause is `kApproachCap_ms` 2800 -> 6000 (section 3). After the fix the hashes equal main's. |
| 4. fast gate | **`GATE fast: GREEN`** on `99861bb1` (section 4). |
| 5. widen the proc camera | DONE for the captures (6/6 bit-exact, closure 241); the GF-visible hole (`0x140892100`) is NOT closed by them (section 5). Uncaptured GOAL_CELEBRATE records listed. |

## 1. Root cause of both reds and both hash moves (CONFIRMED by ablation)

Candidates ablated: the xlate SSE/BSWAP/MAXSD/CVTDQ2PD/PMOV fixes (ProcCamera3), the goal scene list / gates / camera rig (CutsceneTiming 4-7, ProcCamera 1-5), the CutAnchor stack, and the sim edits. Diff of the branch against main in everything a headless `gfsim_run` links: only `goalcelebration.hpp/.cpp` (cap 2800 -> 6000, `Choose/PickCamera/SeedFor` deleted), `match.cpp/.hpp` (spot setter, cleared with the goal state), `elizacontroller.cpp/.hpp` (spot override, `AnimeClassOf` export), `referee.hpp` (accessor). The modified translated functions that exist on main are PREFETCH/PAUSE marker renames (no effect), one MAXSD site in `0x1411757D0` (no caller in the sim: dispatch only), three BSWAP sites in `pre_match`, and callee swaps in closure members of the camera set: none runs in a match. `gf_match.cpp` and the camera rig are in the Godot extension, not in `gfsim_run`.

The ablation (temporary `GF_CAP` env read in `HasArrived`, removed): with the branch as it was (cap 6000) seed 7, 2 min: switch-off `f3bfd3293c8a7612`, pes-loop `6ecbe00cfaec17e0`; with `GF_CAP=2800` the same binary gives **`e663d1e917d6c4f4` / `980c54e27f93ced2` = main's baselines exactly**. So the cap is the only cause; nothing else on the branch moves the sim. The carried throw-in test: cap 6000 fails (121 s, "did not reach his spot within 270 ticks"), cap 2800 passes (103 s).

Why the cap moves matches: `HasArrived(distance, goalScoredTimer)` decides when a scorer stops running to his celebration point and performs (`ElizaController::_AddCelebration`). CutsceneTiming5 raised it to 6 s so a scorer running the whole way to PES's spot T (a Godot-only input, `Match::GetGoalCelebrationSpot`) is not cut off before PES's 4 s gate; but it applied to the capped headless `RunTarget` run too, which changes the scorer's behaviour for every goal and so every match after the first goal (seed 7 scores in 2 minutes; seed 3's 20-minute match scores at tick 5114 and 9791, so its first throw-in moved to tick 18615).

Fix (commit `708d4452`): `kApproachCap_ms = 2800` stays the capped run's; `kPesSpotApproachCap_ms = 6000` (the `ponytail:` comment moved with it) is passed only when the scorer runs to the PES spot; `HasArrived(distance, waited, cap)`. Clean cutover: one caller. Test `pes_goal_celebration_test` (2 tests: the capped runner performs at its cap and not before; the PES-spot runner gets longer than the capped one and a scorer within the radius performs at once). Mutation run, rebuilt, killed, reverted: `HasArrived` ignoring its `cap_ms` argument (uses `kApproachCap_ms`) fails `APesSpotRunnerIsGivenLongerThanACappedRunner`. NOT covered by a unit test: that the controller passes the right cap per run kind (needs a Match fixture); that is covered end to end by the smoke hashes and the two throw-in tests above.

Merged tree (batch AS: main's translated judge) after the fix, seed 7 / 2 min: **`2802089ed1b4da32` / `e7c1462426cef558` = main's new baselines**; `PesRestartDelay.ACarriedThrowInReachesItsSpot` 49 s PASSED, `PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` 168 s PASSED (main's re-staged ceremony seeds {2, 15, 16}; no seed census needed on this branch).

## 2. Latent weakness the drift exposed (CONFIRMED observation, root cause OPEN)

With cap 6000 the seed-3 match's first carried throw-in (t = 18615, taker 17, spot 36.78, -34.31) is picked up at t = 18956 and the carrier reaches 0.05 m of `restartPos` by +108 ticks, but `pieceCarrySpot` is (37.949, -34.760): he stops 1.08-1.39 m from it and creeps toward it at about 0.04 m/s, so the lift gate (`|pieceCarrySpot - pos| < throwInCarryStop_m` = 0.65 m, `teamAIcontroller.cpp:1336`) is still closed at +270 (d = 1.083 m): no lift, `PieceTakerArrived()` false. The walk speed command is `FloorVelocity(pes::kPieceTakerApproachGain * gap)` (`elizacontroller.cpp:186`); the MOVESTEP port's StayDecision ramp decides he is near enough to stand. This is main's code (the branch does not touch it) and a different seed trajectory shows it. Not fixed here: the throw-in is bounded by the restart watch in matches (smokes: longest stoppage 23.7 s of the 32.5 s bound) but takes far longer than PES's root-warp lift. Next step: why the walker stays 1.08 m from the carry spot while PES's gate is 0.65 m (is `pieceCarrySpot` PES's `S` and the gate's `ap` a different point than the root?).

## 3. Smoke hashes (CONFIRMED)

| tree | switch-off | `--pes-loop` |
|---|---|---|
| branch as reported (cap 6000), pre-merge | `f3bfd3293c8a7612` | `6ecbe00cfaec17e0` |
| same binary, cap 2800 (ablation) | `e663d1e917d6c4f4` (main AR/AP) | `980c54e27f93ced2` (main AR/AP) |
| fix, merged with batch AS | `2802089ed1b4da32` | `e7c1462426cef558` (main AS) |

The move was accidental (a Godot-only cap leaking into the headless run); fixed, not a PES-fidelity move. The xlate SSE fixes (silent no-ops now computing) do not move either hash: no translated function they change runs in a match.

## 4. Fast gate (CONFIRMED)

`GATE_TIER=fast GATE_SLOTS="3 2"` with `GF_DIFF_PYTHON`, `PES_EXE`, `PES21_ROOT`, `GF_IMPURE_BANKS=gf-Captures-scratch/banks_union9l`, absolute build dir, on `99861bb1` (batch AS merged, fix included): ctest **100% passed, 2231 of 2231** (13,057 s), recorded re-check 245 + 685 + 546 + 2,253 + 34 selected of the same totals (select base `c38d2743`, 732 changed paths: everything re-diffed), both smokes rc 0 (`2802089ed1b4da32`: 25 stoppages, longest 23.2 s; `e7c1462426cef558`: 28 stoppages, longest 23.7 s; bound 32.5 s), ledger current (38,076 rows), no unrecorded status regression (18,595 rows). Final line: **`GATE fast: GREEN`**.

Not re-gated: `2d8b2907` (batch AT/AU merged, docs and dribble members, conflict-free) and the widening commit below; their checks: `gen_translated.py --check` byte-identical (4,815 functions), `ledger.py --check` current, `pes_proc_camera_test` and `pes_goal_anchor_test` (see the final line of the report). Main's batch gate re-checks the merged head.

## 5. Proc camera widening (CONFIRMED)

The three captures `live2/pid3741448/seq11590, 11594, 11598` are **state 2 per-frame updates** of the record `goal_2019_ins_lostFP_cam00.fdc` (third camera record, map key 2): mode 4, target kind 16, rate 90, zoom stages `[-1, 0, -1, -1, 300, -1, -1, -1]` (first zero stage `+0xBC`, stage 4 at tick 300). Not the record of the earlier three (`goal_2018_run_cmnCam_S_*`, `[-1,-1,0,...]`, rate 50). It plays in scenes `GOAL_I_SET02..09`, **not in any GOAL_CELEBRATE scene**.

`frame_trace.py recorded` (3/3 rc 0) then `snapdiff.py --bounded` over all six captures: **`calls=6 replayed=6 mismatches=0 own_block=0.3673 (83/226) proven=True`**; closure 231 -> 237 functions (+4 allocator natives = 241): new members `0x140892700, 0x1408957E0, 0x1408A0330, 0x1408A0530, 0x1408A0900, 0x1408A0C60` (the last is the screen-space framing smoother that stopped the debug track on `goal_2018_run_cmnCam_S_LB`). `closure_proccam.json`, `results_proccam.json`, universe, set file, layer (4,815 functions, `--check` byte-identical) and ledger updated (commit after `2d8b2907`).

**What it does not do:** `0x140892100` (the stage-1 preset that GF's `outM00` / `noSwitch` / `outH10` records call at init) is still an `OUT-OF-SET CALL` in `pes_fn_141F7BFE0.cpp` (as are `0x140891E40/E60/F40/FE0`, `0x1408921A0`, `0x140892240/2E0`, `0x140892420/4C0/560`, `0x14089AE20`, `0x14089D4B0`, `0x141F7A970/9C0/AD90`). The new captures are updates, not the init (state 1) call, which is where the preset is chosen. So GF's goal movies still fall back to the broadcast camera at step start. Needed: a `proc_camera` capture of **state 1** (the init call) for a record whose first zero stage is 1 (`0x140892100`), 2, 3, 4.

### GOAL_CELEBRATE camera records still without a capture

215 GOAL_CELEBRATE scenes reference 859 distinct `.fdc`; their procedural camera records (empty clip path, type 6, 284 bytes) are **125 byte-distinct records; 1 has a capture** (`goal_2018_run_cmnCam_S_RB.fdc` = the seq6220/24/28 bytes; modes 4: 85, 5: 7, 6: 33; first zero stage 0: 17, 1: 49, 2: 14, 3: 9, 4: 35, 5: 1). **124 lack captures**, among them (scenes using it): `goal_cmnCam_outH00` (99), `noSwitch10/20` (97), `goal_cmnCam_inH00` (58), `goal_cmnCam_outM00` (53; what seed-7's goal requests), `inM00` (44), 44 per-scene `goal_celebrate_NNNN_outH1x` copies, 2019 `outH00` / `outM00` (15 each), `inH00p` (15), 2019 `inH00` (10), `inM00p` (8), `outH00p` (6), `outM00p` (4), and `goal_2018_run_cmnCam_B_B`, `B_M`, `S_LB`, `S_LM`, `S_RM` (1 each; byte-different from S_RB), plus 106 per-scene records. Full table: scratch `celeb_records.json` (not committed). The lostFP record (seq11590/94/98) is no GOAL_CELEBRATE record.

## 6. Open items

1. State-1 (init) `proc_camera` captures for GOAL_CELEBRATE records (section 5), first `goal_cmnCam_outM00`, `noSwitch10`, `outH00`; ask Main to re-arm `proc_camera_141F7BFE0` for the init call.
2. The carried throw-in carrier stopping 1.08 m from the carry spot (section 2).
3. The controller's per-run-kind cap choice has no unit test (needs a Match fixture).
4. The batch AT/AU merge head and the widening commit have not been through the fast gate (checks above).
