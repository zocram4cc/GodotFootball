# pes-port-TrapTick9: LoaderData oracle re-taken on 12 FULL No-Mod ticks (green, Mbinfo kind 4); the two `FAIL w` replays were a decoder bug (switch cases past a ret), fixed; 797CD0 proven over 26 ticks; the window tick and the stall are NOT closed; gate not run (31-10-26)

Branch `StoppageFlow` (head `7efbf281`, `git ls-remote` confirmed). Image `pes_full.bin` (base `0x140000000`). CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Sims ran directly at `nice -n 19` (slots contended); the layer rebuild took ~2.5 h under load 40-48.

## 0. Verdict

| Item | State |
|---|---|
| 1. `git merge origin/main`, push | done (`a2d53fb0`, one docs/ports/README.md conflict, took main's TrapTick8 row) |
| 2. LoaderData oracle re-take | **GREEN** on the 12 FULL No-Mod ticks; Mbinfo kind 4 added. Derived-P differences: not fixed (section 2) |
| 3. live-ball stall | **STILL RED** (`pes_stall_test` on `7efbf281`: `ALiveBallNeverLiesStillLongEnoughForTheBreaker` fails, 670 breaker ticks, 3 stalls: ball 0.08 m/s at (-5.5,28.8) forced from 0.2 m; (-31.2,3.4) forced from 15.7 m, 6.3 m; (-43.5,33.0) forced from 24.8, 14.4, 2.8 m; the other test OK, 605 s run directly at nice 19). The switch-default defect (section 4) was NOT the cause; status-2 decode partial (section 3); no capture requested yet |
| 4. `FAIL w` seq2028 / seq2035 | **translation bug, fixed** (section 4) |
| 5. `GATE fast` | **not run**: the tick oracle (`EveryCapturedTickOnGfsWindowEqualsPes`) still aborts; gate would be RED. Interim only |

## 1. Oracle re-take (CONFIRMED)

- Captures (the ones the test comment cites): `live2/pid3741448` `trap_action_tick_797CD0` **seq1877, 1883, 1889, 1895, 1917, 1924, 1938, 1948, 1982, 4790, 4798, 4803** (FULL, No-Mod pinned launch). `replay_verdicts.py --hook 0x140797CD0` (unbounded snapdiff): **12/12 PROVEN**. `trap_oracle.py` built 12 oracle dirs (16 MB, scratch). Main may slim the 12 only after item 2's second test (`EveryCapturedTick...`) lands: it needs the same dirs (keep FULL until then).
- Loader: `kTrapMbinfoKinds` gains kind 4 = `JumpData.bin` (`FUN_1406AD810`): path index 4 of the table at `0x14344DC40` (`cpk_dat/common/anime/Mbinfo/bin/JumpData.bin`), 4 bytes a record, key `(word0 & 0x1FFF) + 0x11` (the `+0x11` at `0x1406AD810`'s `uVar3 = (word & 0x1fff) + 0x11`), list at record `+0x90` (`FUN_1406AE810` slot 4: `param_1 + 0x90`), cap 2 (`DAT_1425DC370[4]`, read by the loader from the exe). Before: capture motion 0xDA9 count `+0x4` = 1, GF 0.
- `pes_trap_oracle_test --gtest_filter=TrapTickOracle.LoaderData*` with `GF_PES_TRAP_TICK_ORACLE` = the 12 dirs: **OK** (the "unexplained H ranges" lines it prints are informational). The test's comment now says what the dirs are (stock trap.o `+0x8` = 0x07 vs a mod's 0x0a).
- Not a mutation-tested change in the strict sense: the red state is the run without the kind-4 row (TrapTick8 saw it on seq3244); reverting the row reproduces it.

## 2. Derived P (OPEN, found, not fixed)

`EveryCapturedTickOnGfsWindowEqualsPes` (with `stdbuf -o0`, else the abort eats the output): the first four captures have no derived mismatch; **seq1917**: P `+0x47e` GF `ff` / capture `0a`, `+0x47f` GF `ff` / capture `00`, `+0x78e` GF `80` / capture `c4`, `+0x78f` GF `3f` / capture `3e` (0x3f80 vs 0x3ec4: a float high half). Root not traced. Then the window run aborts: `PES_DISPATCH_UNTRANSLATED: 0` (a null indirect target) in `pes_fn_141B52380` called from `pes_fn_1406EC660` <- `1406E2C30` <- `140797890`; before the 26-tick closure it aborted at `0x1404BEF90` (section 5).

## 3. Stall (OPEN; partial decode, CONFIRMED where stated)

- `FUN_14079A470`'s refusal status is the struct `{reason +0, rank +4}` of the candidate search: `FUN_140857220` -> `FUN_140858B90` (tries mirror modes `+0x80` = 1, 0, 2) -> `FUN_1408586A0` -> `FUN_14085B900` -> `FUN_140858260` -> `FUN_14085B3E0` -> `FUN_140857CB0` -> `FUN_140857280` (per-candidate test, writes reasons 8 at `0x14085751B`, 9 at `0x140857544`, else `*param_5 = low dword of the solver's out`) -> displacement solver `FUN_1406ECBB0`. Best failure kept by `FUN_14085F150` (priority table `0x1425F20C0` = 0,1,2,1,1,1,1,3,2,3,1,1,1,2,2,2,2,4 for reasons 0..0x11, read from the image).
- Solver reasons (decompile): x axis 3 when the lateral shortfall exceeds the limit; z axis 4/5/6 (x ok) or 10/11/12 (x failed). `FUN_140863C40` then ranks a failed candidate: rank (`+4`) = 2 "near miss" at `0x140863D1A, 0x140863DA3, 0x140863E40, 0x1408640AF, 0x1408641DB` (all `mov dword ptr [rsi+4], 2`, scan of `0x140846000..0x140866000`), 3 otherwise. The chain refuses only when rank == 2 and `FUN_140849400` is false (`local_94 == 2`, `0x14079A8xx`), returning `local_98` (the reason); otherwise `FUN_140799A00` retries.
- **No constant store of 2 into a reason exists in `0x140846000..0x140866000` or `0x140798000..0x14079F000`** (capstone scan, `mov dword ptr [reg], 2`): so TrapTick8's "status 2" is a computed reason, the rank, or a mis-read; not established. Not decoded: what PES does instead for a man within 1.5 m of a ball at rest (INF from `pes-re-trap-pick.md` 3: the 0x5D data-driven touch, entry 3 step 5, which needs order kind 0x4A or `ai+0x14 == 5`; not confirmed live).
- **A real defect found on the way (section 4) lives in this chain**: the closure members `0x1405E1670` and `0x1408653E0` (the limits the solver reads) silently took the switch DEFAULT for every case past the first `ret`; ball-relative classes are such switches. Whether it moves the stall is untested: `pes_stall_test` was queued behind other sims when this report was written.
- A capture, if still needed after that test: `trap_kind0_79A470` with `--when` on the ball record's speed (< 0.3 m/s) and the man's distance (< 1.5 m): the ball record is the entry's `r8` (`FUN_140A6CF70(param_3, t)` samples, stride in `pes::kBallSampleStride`) and the man is `rdx`; I did not derive the exact predicate expression, Main will need `pestrapactor.hpp`'s offsets.

## 4. seq2028 / seq2035: translation bug (CONFIRMED), fixed

- Both `FAIL w` replays differ only in write ORDER of a dead stack slot (final images identical, multiset differs by 2 float stores at `rsp-0x8b8`); `lockstep.py` (GF_XLATE_TRACE build, 191,492 instructions each side, ~850 s) shows a CONTROL divergence at record 66,430: after `jmp rcx` at `0x140865533` (`FUN_1408653E0`'s switch, `edi` = 1, byte map `0x1408655E8`[1] = 0, dword table `0x1408655D4` = {0x865558, 0x865535, 0x86559E, 0x86557B, 0x8654A8}) PES goes to `0x140865558`, the translation to the default `0x1408654A8`.
- Root cause: `pdata.py _walk` stops a range at its first `ret` when no forward branch is pending, and a switch's case targets are not branches: the cases `0x140865558/57B/59E` lie after the ret and were never decoded, so `xlate` emitted a `switch` with only 2 cases and **default = the original `ja` target, silently**. `gen_translated.py --check` over the whole layer: **exactly two files drifted** once fixed: closure `0x1408653E0` (+54 lines) and `0x1405E1670` (+179 lines, 4 more cases, 2 constants).
- Fix: `pdata.Pdata._switch_targets` resolves a `jmp reg` switch from the already-decoded lines (cmp N / ja default / lea base / byte map / `mov r,[base+idx*4+disp]` / add / jmp; the pattern `xlate.prepass` uses) and feeds the table targets to the walk. Red-first test: `test_extent.py test_switch_cases_past_the_first_ret_are_decoded_and_the_table_is_not` (synthetic image, new `.pdata` entry): fails on the old `pdata.py` (extra garbage and missing cases), passes now; the whole `test_extent` suite OK (20 tests, 7 skipped without PES_EXE). Mutation: the old `pdata.py` restored -> red (run, restored).
- After the regenerated layer, `lockstep` was not re-run; the unbounded replay of seq2028/2035 was not repeated (the bounded 26-tick proof of section 5 covers other captures). Re-run them to close the item formally.

## 5. 0x140797CD0 proven over 26 ticks; the window tick still aborts

- `snapdiff.py --bounded --only 0x140797CD0` over the 14 TrapS5 ticks and the 12 No-Mod ticks: **calls=26 replayed=26 mismatches=0 proven=True**, closure 238 functions (was 154). `closure_trapaction.json` / `results_trapaction.json` updated (indent 1, small diffs), spec `specs/trapaction/1404BEF90.json` added (`closure.py --tag trapbef`; 1404BEF90 has no callees), layer regenerated: **4596 functions (+41), 6 existing files changed**, no OUT-OF-SET marker left in `14079CB50`.
- Why it was needed: the GF window run of seq1877 (and the others) entered `0x1404BEF90` from `14079CB50`, which the committed layer held as an OUT-OF-SET abort (`bVar2` = action+0x27 = 1, `fVar14` = 1.37, RotData word `0x0008141F` = 0 -> `iVar8` = 0 <= fVar14; GF and the capture agree on those inputs; PES enters the same branch). The harness replays were fine because `snapdiff` translates it.
- Still red after the regeneration: seq1917's derived P (section 2) and a further untranslated/null target. `pes_trap_oracle_test` therefore is NOT 15/15: `LoaderData` OK, `EveryCapturedTick...` aborts, `TheCapturesCover...` aborts (same window run), the 11 `TrapOracle` tests were not re-run on the final build.

## 6. Unfinished (numbered)

1. seq1917 derived P: `+0x47e/0x47f` (GF 0xff, PES 0x0a/0x00) and `+0x78e/f`; find the P writer GF mis-derives; then the window tick's remaining aborts (null target in `141B52380`).
2. Stall: the fixed layer shifts the stalls but does not remove them (result in section 0). Next: the status decode (section 3) or the capture; the stall at (-5.5,28.8) forces a man 0.2 m from a ball at rest, the textbook case (a man at the ball who does not take it).
3. Re-run unbounded replays of `pid3061186` seq2028/2035 and `gen_translated.py --image <pes_full.bin> --check` (the regeneration was committed from a scratch root, the check itself was not re-run on the final tree); rebuild `diff_*_recorded` selection for the 6 changed closure files.
4. `GATE_TIER=fast GATE_SLOTS="3 2" tools/dev/gate.sh` with the gate env (`PES_EXE`, `GF_DIFF_PYTHON`, `PES21_ROOT`, `GF_IMPURE_BANKS`): not run.
5. The 12 FULL ticks stay cited by the oracle test until item 1 lands; the 14 older oracle dirs (`gf-TrapS5-scratch`) are mod captures and no longer serve the LoaderData test.

`GATE fast`: not run (interim reason above).
