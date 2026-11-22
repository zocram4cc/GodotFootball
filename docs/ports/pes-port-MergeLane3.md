# PES port MergeLane3: attribution of the four MergeLane2 aborts, and the MergeLane2 gen check

Status: partial. The MergeBKBJ half is complete. The PassArrival half is incomplete (stopped at wrap-up). StallBreaker2's SHA has not arrived, so nothing was merged and the nine-binary rerun was not done. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named.

## 1. MergeLane2 HEAD (CONFIRMED)
- `origin/MergeLane2` = `db9bfef3` (`git ls-remote`). Local HEAD equals it. Tree clean (only the godot-cpp symlink).
- `gen_translated.py --image pes_full.bin --check` on `db9bfef3`: rc 0, `10637 functions (3879 recorded bit-exact, 6758 closure) regenerate byte-identical`. Log: `gf-MergeLane3-scratch/gencheck_lane2_head.log`.
- So Main's gen-check failure (12 drifts since 55ff8f64, untracked input) is not present on `db9bfef3`. Whatever StallBreaker2 saw is on a different HEAD, or depends on an untracked input I do not have. OPEN: StallBreaker2 to name the HEAD it checked.

## 2. Attribution experiment (scratch worktrees, detached)
Worktrees: `<workdrive>/gf-MergeLane3-bkbj` (abe9f81d, origin/MergeBKBJ), `<workdrive>/gf-MergeLane3-pa` (fb3f144f, origin/PassArrival). Each: `cmake -G Ninja -DCMAKE_BUILD_TYPE=Release`, build of the four targets under `/tmp/gf-build.lock` (both builds rc 0, 0 errors). Tests run in sim slots with `timeout 1800`, env `-u GF_DEBUG_RECEIVE -u GF_DEBUG_ANKLE -u GF_PES_SURVEY`, `PES_EXE=pes_full.bin`, `PES21_ROOT=<workdrive>/PES21`.

| test | MergeLane2 (db9bfef3, reference) | origin/MergeBKBJ abe9f81d | origin/PassArrival fb3f144f |
|---|---|---|---|
| pes_press_approach_test | ABORT `PES_DISPATCH_UNTRANSLATED 0x14073DD50` after 1 test started | PASS 1/1, no abort | PASS 1/1, no abort |
| pes_pass_target_test | ABORT `0x14073DD50` before first test | FAIL 2 of 3 (IsSetInFlightAndNeverStaleWhileTheBallRests, EveryPassGetEpisodeIsWithinPesLiveRange), 1 pass, no abort | NOT RUN (stopped at wrap-up) |
| pes_throwin_release_test | ABORT `0x14073DD50` after 1 started | NO ABORT: timed out at 1800 s (rc 124) with 4 started, no PES_DISPATCH line | NOT RUN |
| pes_restart_delay_test | ABORT `0x14073DD50` after 2 started | NO ABORT: timed out at 1800 s (rc 124) with 9 started, no PES_DISPATCH line | NOT RUN |

Logs: `gf-MergeLane3-scratch/bkbj-*.log`, `pa-*.log`; summary `attr_summary.txt`.

## 3. What this says about the aborts
- CONFIRMED: `0x14073DD50` does not appear on origin/MergeBKBJ (four of four tests: no abort line in any run) nor on origin/PassArrival (press_approach: no abort). So the four aborts are not pre-existing on those two branches.
- CONFIRMED: MergeLane2 = StallLive (1b56b726, the base) + PressCut + MergeBKBJ + PassArrival + the kick-family fix. The two branches I tested do NOT contain StallLive. So the attribution is not yet a clean one: the aborts could come from StallLive alone, or from PressCut on StallLive, or from a merge of those with the rest. INF: StallLive or PressCut on StallLive. OPEN: run `1b56b726` and `2b9e4786` (PressCut) with the same four tests. Those are the experiment that settles it, and I did not run them.
- CONFIRMED on MergeBKBJ: `pes_throwin_release_test` and `pes_restart_delay_test` made progress (4 and 9 tests started) and then hit the 1800 s timeout with no abort. That is either a stall or slowness under machine contention; the sim slots were shared with several other agents' runs, and I did not measure which. OPEN: rerun with a longer timeout and a per-test log, or check the stall probe output.
- `pes_pass_target_test` on MergeBKBJ: 2 failures. These are pre-existing on MergeBKBJ (its own test, its own code, not a merge artifact). CONFIRMED that they fail there; they are not failing on MergeLane2 only because MergeLane2 aborts earlier.

## 4. Not done
- StallBreaker2's SHA: not received, so no cherry-pick and no nine-binary rerun. Nothing merged into MergeLane2 in this round.
- PassArrival: `pes_pass_target_test`, `pes_throwin_release_test`, `pes_restart_delay_test` not run. I killed the script at wrap-up; those were the processes I started (PIDs under `attr.sh` 1512926). No other agents' processes were touched.
- The StallLive and PressCut heads (section 3, OPEN).

## 5. Housekeeping
- Build dirs deleted: `gf-MergeLane3-bkbj/build` and `gf-MergeLane3-pa/build`. The two detached worktrees remain (no build dirs, godot-cpp symlink only); remove them once this report is archived.
- Scratch: `<workdrive>/gf-MergeLane3-scratch/` (logs only).
- MergeLane2 is unchanged this round: `db9bfef3`.

## 6. Open items (numbered)
1. Run `1b56b726` (StallLive) and `2b9e4786` (PressCut) with the four tests, to attribute the `0x14073DD50` aborts.
2. PassArrival: run the three remaining tests.
3. MergeBKBJ throw-in and restart-delay timeouts: longer timeout or stall-probe output; determine stall vs slow.
4. StallBreaker2: name the HEAD it ran gen --check on (db9bfef3 passes).
5. StallBreaker2's SHA: merge, rerun the nine binaries, fix merge-induced reds.
