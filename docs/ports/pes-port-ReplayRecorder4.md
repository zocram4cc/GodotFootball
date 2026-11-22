# pes-port-ReplayRecorder4: the widened paired rule landed; 1408BED00 and 1408BE3C0 both proven on real captures, merged and translated; the 11 reader tables declared with evidence; a crash in the round-2 cave rule fixed; the bind's element class confirmed (ReplayHuman, a playback loop, not the bind); the unit-bind caller still NOT identified (hook queued by Main)

Branch `Presentation`, pushed `f2084bda..c61efcf4`. `git ls-remote origin Presentation` = `c61efcf497d6748e239e8a2fc3dc4a4e4d412793` (confirmed). One commit, `c61efcf4`. Build dir removed; capture copies removed; scratch 3.6 MB (logs and scripts only).

Labels: CONFIRMED = a unittest run, a `prove_bounded` run on real captures, a headless read of the flat image (`pes_full.bin`, file offset = VA - 0x140000000), a unicorn trace probe, a C++ test binary, `gen_translated --check`, `ledger.py --check`. INF = inferred, labelled where it appears.

## 1. Paired rule, widened (owner ruling, round 4): DONE

- `boundary_guard.py`: a memory write whose base register holds an object the region stored into the packet is admitted (refcount or flag). Any other write outside the packet still fails.
- Tests: `test_boundary_guard.py` 24/24. New synthetic cases: a paired `xchg [rcx+0x2c],eax` after a store of rcx into the packet passes; the same write with no store fails; a write into a different object than the one stored fails. Real: the closure functions 0x1419DC350, 0x1419DC480, 0x1419DC540, 0x1419DCCA0 (1408BED00) and 0x1419DBAF0 (1408BE3C0) pass every allocator site.
- Mutation: the widened clause disabled (`paired = False`) on a scratch copy, edit tool; two tests red (the real 1408BED00 site and the synthetic paired write). Restored by not copying back.

## 2. 1408BED00: PROVEN, merged

- Capture pid1845381 seq7492 (copied, unpacked, removed). `prove_bounded --merge`: calls 1, replayed 1, mismatches 0, own-block 7/7 (1.0). Boundary entry with the ruling and the sites written to the record.
- The record's closure reads the table windows declared in section 4 (0x142B67F40..0x142B68980).

## 3. 1408BE3C0: PROVEN, merged, translated

- Six captures pid950952 seq002/004/006/008/010/012 (copied, unpacked, removed). `prove_bounded --merge`: calls 6, replayed 6, mismatches 0, own-block 23/55 (0.418). Boundary entry written (sites 0x14195BA9E, 0x1419DBC0C, 0x1419DC774, 0x1419DC9ED, 0x1419DCFB5).
- Its closure's 11 reads of read-only image tables are now covered by the declared windows in section 4.

## 4. Declared tables (ro_tables.json, evidence per row)

Three windows cover the 11 reads of 0x1419DB1D0, 0x1419DB2E0 and 0x141AD9220 (CONFIRMED addresses by disassembly; flat base 0x140000000 from each function's `lea reg,[rip+..]`):

- `0x142A95698`, 0x4C0 bytes: 16-byte rows indexed by ecx (`cmp byte [r8+rax*8+0x2a95698]`, rax = 2*ecx). Byte0 is the flag, byte1 (0x142A95699) and byte2 (0x142A9569A) are the kind byte. The switch in 0x1419DB1D0 admits ecx in [0x14,0x4B], but the flag is read before that check, so the window is rows 0..75. OBSERVED ecx 20..56 (0x1419DB1D0) and 2..71 (0x1419DB2E0) on the boot captures and seq7492 (argprobe, trace layer). Bounds beyond observed: INF.
- `0x142A947C0`, 0x60 bytes: six float rows, 16 bytes apart (0x142A947C0 + 16j, j = 0..5), indexed by edx*4. OBSERVED edx 0..2; the window covers 0..3. INF beyond observed.
- `0x142AB6B90`, 0x10 bytes: CONFIRMED bounded by the code: index `(ecx>>6)&3` (0x141AD9296/0x141AD929A).
- Earlier `0x142B67EF0` window corrected to 0x44 (round 3's 2-byte entry was wrong). Eight `0x141EBBDB0` windows as in round 3; their row counts 31 and 20 are one observation each (INF capacities).

## 5. Bind 140A8EE20: element class CONFIRMED; the caller NOT identified

- Loop 0x142175540 (CONFIRMED): walks an array of objects at [rcx+0x38] (count [+0x238], [+0x240]) and calls `[elem+0x38]` on each. The element vtable is 0x142BF7D58 (the ReplayHuman vtable, the same one `replay_human.cpp` writes into the record as `kReplayVtableVa`), and slot +0x38 of that vtable is 0x14217DEF0 (the frame reader). So the loop plays back recorded humans. The goal-replay capture (pid1267009 seq53249, Main's) returns into it at 0x1421755D9. That is playback, not the unit bind.
- 0x142175540's only direct caller is 0x142174264, inside the playback state machine 0x142174140.
- Bind 140A8EE20 is UnitReplay slot +0x38 (CONFIRMED, vtable 0x142653F58). A whole-image scan for `call [reg+0x38]` with a `+0x6A8` load (the manager's current unit) in the 24 preceding instructions finds two sites: 0x142174230 (receiver is the playback object, not a unit) and 0x140759AB1 (receiver is `[rsi+0xAB8]`, the anime state object). Neither is the bind. The bind's caller is NOT identified.
- Static candidate for the unit selection that precedes the bind: 0x141D82AC0 writes `[rbp+0x6A8]` from registers at 0x141D8419A, 0x141D84818, 0x141D848D0, 0x141D849C9 and 0x141D84C7C (INF that it is the selector; the other `+0x6A8` writes are immediate stores of 0xF to a different field). Hook line (through `hooks_for_image`): `hook unitsel_141D82AC0 0x141d82ac0 19 1 1 488bc4554154415541564157488da8f8f4ffff`. Main has queued it with rec_140A8EE20 for the next relaunch.
- Hook note: rec_140A8EE20 (EVERY 1) has not fired in 6.5 h and 11 goals on pid191227. So the bind is not reached on the COM goal path at all (CONFIRMED by observation), and the goal replay plays through 0x142174140/0x142175540 without it.

## 6. Bug fixed (round 2 code)

- The relocated-cave rule (`pdata.py _cave`) read the image at `t - IMAGE_BASE` for any jump target, so a function's jump below the image base raised `OSError`. Found by the whole-image bind scan. Guarded (`t < IMAGE_BASE` returns no cave). Regression test `test_a_function_jump_below_the_image_base_does_not_crash_the_extent`: red without the guard (scratch copy, edit tool), green with it.

## 7. Layer, checks

- `gen_translated.py --image pes_full.bin`: 10459 functions, 3834 recorded bit-exact (was 3832: + 1408BED00, + 1408BE3C0), closure 6625. `--check`: byte-identical, rc 0.
- The regenerated diff: 9 committed closure files each gain one call to a newly included callee (e.g. pes_fn_141AFEB40 now calls pes_fn_141AE8280 instead of an out-of-set dispatch stub), and 227 closure files are new. This is the closure of the two merged records, not unrelated churn. Their correctness is the records' own proofs (closure members are not separately proven, as before).
- Pre-existing "held out" lines (three translator-bug callees) print at every run.
- `ledger.py --check`: current (44479 rows).
- C++ test binaries, built (FULL) and run:
  - pes_fox_world_test: 15 pass.
  - pes_replay_human_test: 5 pass, 3 skip (GF_PES_COPY_CAPTURES not set).
  - pes_native_service_test: 7 pass.
  - pes_hud_clock_test: 4 pass.
  - pes_anime_engine_test: 11 pass, 1 skip (render-plot env var).
- Python: test_boundary_guard 24/24; test_pdata_cave 6/6; test_closure_walk, test_cond_tail_call, test_tail_jump_loud, test_vtable_tail_jump, test_prove_bounded, test_getcurdirw_model, test_strstr_model, test_service_answer: all OK.

## 8. Process notes

- The owner asked for edit-tool-only mutations. The scratch copy for the bind scan was made by a `sed` on a scratch script (not a repo file); the scan itself was rerun without it. Disclosed.
- Background shell jobs started with plain `&` are not tracked by the wait tool; the tracked async form was used for the final runs.
- `commit` namespace cannot see this worktree (extern/godot-cpp symlink); committed by path-limited git with the owner identity.

## 9. Open items

1. Bind 140A8EE20 caller: not identified. Hook `unitsel_141D82AC0` (queued by Main) is the next sample; if it fires on a replay start, the bind's caller is the next call from that path.
2. Table bounds in section 4 are observed, not measured by callers: ecx > 71 and edx > 2 are INF. A capture with a larger value would confirm or widen the windows.
3. The 1408BED00 and 1408BE3C0 closure members (227 new functions, 9 changed) are not individually proven; they are proven as members of the two records' closures only.
4. Slice 3 (playback from recorded unit frames) and slice 4 (state-7 record create) not started; the cursor advance (`+0x10`, wrap 660) not decoded.
5. The duplicate `0x142581374` entry in ro_tables.json (pre-existing) is left as found.
