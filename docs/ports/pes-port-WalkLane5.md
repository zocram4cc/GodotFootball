# PES port: WalkLane5 (branch WalkLane; tip 843a2e30, pushed, `ls-remote` confirmed)

## 0. Verdict
- Item 1 done: the tip is honest. `158aadf5` (WIP production; PesStallQuick 1602 with the strike lock, 319 without it, vs 53) is reverted by `843a2e30`. Its diff is kept at `<workdrive>/gf-WalkLane-scratch/wip-158aadf5.patch` (`git show 158aadf5`). The production source of `843a2e30` is byte-identical to `a9d9e1b3`.
- The reverted tip holds the ratchet: PesStallQuick 53 (bound 53; two runs, both 53), ALiveBall PASS (bound 2,229; the exact count is not printed on pass, see section 2).
- fced68dd tests: both are red for a PES reason on the reverted tree (section 3), so both stay.
- Item 2: the blend is not reached by the sim. `0x1406F14E0` is already translated (closure member), so translation is not the missing piece; the body consumes a series by playing member 0 only. The census shows the stall is driven by the WIP's strike lock more than by the dribble loop, ids or walker (section 4).
- Item 3: nothing landed. No change turned a red green while holding both ratchets.

## 1. Commits
- `843a2e30` Revert "WIP WalkLane4 ...": 5 files (pesmovestep.cpp, pestrapactor.cpp, humanoid.cpp, humanoidbase.cpp, humanoidbase.hpp), 15 insertions, 50 deletions. Pushed: `ls-remote origin WalkLane` = `843a2e305437410bbb26cf9a077435c0d1361c9a`.
- `fced68dd` (tests) stays on the tip (section 3).

## 2. Ratchet on the reverted tip (`843a2e30`)
- Build: `pes_restart_delay_test`, `pes_pass_target_test`, BUILD_RC=0 (769 s, ccache warm).
- Env for constant-dependent tests: `PES21_ROOT=<workdrive>/PES21 PES_EXE=<home>/ghidra-projects/scratch/Unpacker/pes_full.bin`. Without `PES_EXE` every such test skips or fails with "install constants unavailable". My first attempts were invalid for that reason; every number below uses both vars.
- PesStallQuick (`PesStallQuick.*`): `forced_live=53` in two runs (`quick_base_dump.log`, `quick_base_rerun.log`). Ratchet 53: holds, deterministic on this tree.
  - WalkLane1's 28 was measured before the Integrate merge (c4404052). On the integration tip the base is 53, which is the brief's ratchet.
- ALiveBall (`PesStall.ALiveBall*`): PASS, exit 0, 1,215 s (`aliveball_base.log`). Bound 2,229. The test prints its count only on failure, so the exact count is NOT measured.
- `gen_translated.py --check --image pes_full.bin`: RC=0, "regenerate byte-identical" (`gen_check_revert.log`). The revert touches no translated file.
- PII: grep of the reverted files for `<workdrive>` and `<home>` finds nothing. I did not run `tools/check_pii.sh` from the main checkout (its exit status was not captured in this session), so it is not claimed as passed.

## 3. fced68dd tests on the reverted tree
- `PesPassTarget.ATrapsRootMovesByExactlyTheStrikeAdjusterStep`: RED, PES reason. `Expected worstRootGap_m <= 0.001, actual 0.15621858636509228`: "a trapping man's root moved 0.156 m more (or less) than his strike adjuster's step, in the 222 ticks sampled" (`red_atraps_revert.log`, line 234). The cause is the missing strike root lock (AnimMovePlugin +0x81, pes-re-anim-blend.md 7.1), which the WIP tried to port.
- `PesStall.APesDribblerNeverHoldsHisClipsLastFrame`: RED, PES reason. `Expected worstLastFrameHold <= 1, actual 343 vs 1`: "a PES dribbler's body sat on the last frame of his clip for 343 ticks in a row" (`red_dribble_revert2.log`, line 887). This matches WalkLane4's 343 and the series-freeze trace (WalkLane2 section 2, TrapContact10 section 15.1). The first two attempts were invalid (no `PES_EXE`, then the 1,100 s cap); the verdict is from the 2,600 s run (~17 min).

## 4. Census: what raises PesStallQuick, and the blend (item 2)
All runs: `PesStallQuick.*`, `GF_STALL_DUMP=1` (one `STALLDUMP` line per live firing: man, clip, ac8, owned). Files are in `<workdrive>/gf-WalkLane-scratch/`.

| tree | forced_live | by ac8 (all firings have owned=0) |
|---|---|---|
| reverted (`843a2e30`, base) | 53 (x2) | ac8=7: 34; ac8=21: 15; ac8=2: 4 |
| loop + ids + walker, lock forced off (`LockRoot(false && ...)`) | 319 | top: A11 `trap_0_0_000_y0_in_mirror` ac8=2 x40; A11 `sliding/idle/000.anim` ac8=11 x35; A11 `trap_0_0_000_y0_in_mirror` ac8=7 x31; A11 `dribblerun_3_0_045_sole_ver12_mirror` ac8=7 x25; H5 `tacklehold_waist_0_0_020` ac8=13 x24 |
| full WIP (`158aadf5` production) | 1602 | ac8=7: 695; ac8=2: 492; ac8=13: 234; ac8=11: 92; ac8=21: 89 |

- CONFIRMED: every firing in all three trees is on a man with `owned=0`, i.e. not a PES-owned dribbler. The dribble loop only acts on PES-owned men, so it cannot cause a firing directly; any effect is indirect, through other men.
- CONFIRMED: the strike root lock is the big term. Lock on adds 1,283 firings over lock off (319 to 1602). The lock is set for every man with `frame.strikeMoved` (`pesmovestep.cpp:728`), so it reaches the Trap, trip and sliding men that the breaker counts.
- CONFIRMED: loop + ids + walker alone take 53 to 319. The three were not split (no ablation for each), so the 319 is the sum of three changes. Open.
- INF: the blend. `0x1406F14E0` is translated as a closure member (`src/pes/translated/closure/pes_fn_1406F14E0.cpp`, "not recorded bit-exact on its own"). The body consumes a series in `PlayDribbleIssue` (`src/sim/onthepitch/teamAIcontroller.cpp:3532-3545`) by playing `SeriesFirstOf` (member 0) only; `PesFoxWorld::IssueSeries` (`pesfoxworld.cpp:291`) and `ReadPose` (`:339`) have no sim caller. So PES's bracketing blend (`0x141AF52B0`, position from the series writer) cannot run in the body, whatever the translation status. Whether the blend would prevent the quick firings is NOT measured: it needs the body to take the anime engine's pose, a multi-round change.

## 5. Open items (numbered)
1. ALiveBall: print the count on pass (one line in `tests/sim/pes_stall_test.cpp`), rebuild, rerun on the tip (~20 min). Bound 2,229; the count is not in hand.
2. Split the 319: loop alone vs ids alone vs walker alone (three builds, three quick runs at ~6 min each).
3. Strike root lock: it is set for every striking man (`pesmovestep.cpp:728`), which is what the Trap/trip/sliding census shows. Find PES's gating of AnimMovePlugin +0x81 for the Trap action only, apply the lock to Trap men alone, and measure against the 53 bound.
4. Blend: `IssueSeries` + `ReadPose` need a sim caller for the dribble series, replacing `PlayDribbleIssue`'s member-0 stand-in. Needs the series position from the decision (`0x1406F14E0` caller `0x1406E5FC0` at `0x1406E6325`). Multi-round.
5. Sliding (0xB): translate `0x140786D60`, `0x140788C00`; decide `0x140789D00` (WalkLane3 section 3). The A11 sliding idle clip (ac8=11, 35 firings in the lock-off run) is the visible symptom.
6. Before any landing: `gen_translated.py --check` and `tools/check_pii.sh` from the main checkout.

## 6. Worktree note
- In `<workdrive>/gf-WalkLane`, plain `git status`/`diff` fail with "expected submodule path 'extern/godot-cpp' not to be a symbolic link". `git -c submodule.recurse=false ...` works for log, revert, checkout, push, ls-remote. For commit identity, `-c` flags go before the subcommand (`git -c user.name=... revert`), or git reads them as a revision.
