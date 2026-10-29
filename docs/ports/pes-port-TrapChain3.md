# pes-port-TrapChain3: SETcc-to-memory was a no-op in xlate; trap kind-0 root 0x14079A470 PROVEN 7/7 and commit 0x14079AFD0 PROVEN, both recorded (29-10-26)

Branch `StoppageFlow`, head `fc84d383` (`git ls-remote origin StoppageFlow`). Tags: **CONFIRMED** = read from a tool run this session; **INF** = inference; **OPEN** = not settled. `git merge origin/main` done first (commit `2affd387`; the merge re-created `extern/godot-cpp` as an empty dir: `rm -rf` + symlink again; plain `git status` then errors "expected submodule path ... symbolic link", so use path-limited `git status -- src tools tests docs`).

## 1. Root cause (CONFIRMED)

Lockstep (`lockstep.py`) on `pid463316/seq4310`, 0x14079A470 closure built with `GF_XLATE_TRACE=1` (342 targets), harness trace written with `GF_LOCKSTEP_OUT`, translated trace with `GF_TRACE_OUT` (scratch `ls4310.sh`):
- 105,113 harness records (import models dropped) vs 143,522 translated. **CONTROL divergence at record 12344**: harness `0x14084892E`, translated `0x140848922`; all registers and xmm identical at record 12343 (`0x140848920 je 0x14084892E`), so the difference was a **flag from memory**: the preceding `cmp byte ptr [rcx+0x20], 0` (`0x14084891C`) read a different byte. `rcx = 0x1BCFEA90` is the record built by `0x140846C70` (called from the root at `0x14079A755`).
- The write logs (`corpus.jsonl` vs `translated.jsonl`, 18 hex digits per byte) pin the producer: translated wrote `0x20` to `0x1BCFEAB0` (= record+0x20) where PES wrote `0x00`. The writer is `0x14084CE92 mov qword ptr [rdi+0x1C], rax`, `rax = [rsp+0x30]`; byte `[rsp+0x34]` is set by `0x14084CE88 sete byte ptr [rsp+0x34]` (`ebx = [rdi+0x74] = 2`, `cmp ebx,1` -> 0).
- Generated line for it: `/*SET-MEM*/(void)0;`. **`xlate.emit_set_cmov` emitted a no-op for every SETcc with a memory destination** (only the register form was implemented), so the byte kept its stale stack value (0x20) and the 8-byte copy carried it into the record.

## 2. Fix, test, mutation (CONFIRMED)

- `tools/dev/translate/xlate.py` `emit_set_cmov`: the memory form now stores the condition as one byte through `mem_store(..., 8)`. General (every SETcc [mem]), no per-function case. Commit `3631204c`.
- Red test `tools/dev/translate/test_setcc_store.py` (`translate_setcc_store_test` in `tests/CMakeLists.txt`; skips 77 without `GF_DIFF_PYTHON`): synthetic PE image (no game bytes) with `cmp ebx,1; sete byte ptr [rsp+0x34]; ret`; the translated function is compiled with g++ and run over a 0xAA-filled stack; asserts the byte is 1 / 0 for ebx = 1 / 2 and that the neighbours stay 0xAA. Failed before the fix (`['aa','aa','aa']` both cases), passes after (2 OK).
- Mutation (run, reverted): store width 8 -> 32: both tests FAIL (neighbour byte clobbered). The condition-true and condition-false branches are each pinned by their own test.

## 3. Layer and recorded re-runs (CONFIRMED)

- Only **2** recorded files contained the no-op: `impure_sample/pes_fn_1405DC350.cpp` and `gameplay/pes_fn_140A81F70.cpp`. No closure file changed. Both re-run with `driver.py --check` (subset record): **impure 1405DC350 bit-exact, core 140A81F70 bit-exact**. **Verdict changes: none** (pilot/cutscene/presentation: no changed file, nothing to re-run). Commit `b8ac7b80`.
- After the two records were added (below): `gen_translated.py` wrote 4,039 functions (3,683 recorded bit-exact, 356 closure; +107 closure files, + `trapchain/pes_fn_14079A470.cpp`, `pes_fn_14079AFD0.cpp`); `gen_translated.py --check`: **byte-identical, 4,039 functions**. The note "held out 0x14202BA30: its closure reaches a mistranslated callee (0x14202C250)" was already there before this change (OPEN, not investigated).

## 4. Kind-0 and commit verdicts (CONFIRMED; `snapdiff.py --spec-dir specs/core --hooks <PES21>/rendezvous/hooks.cfg`, closure rebuilt on the fixed xlate)

| root | captures | replayed | bit-exact | verdict |
|---|---|---|---|---|
| **0x14079A470** kind-0 | pid463316 seq4310 4329 4351 4402 4469 4531 + pid1467004 seq019 | 7 of 7 | **7/7** (rax, xmm0, every written byte; before the fix 1/7) | **PROVEN**, own-block 64/103 = 0.6214 |
| **0x14079AFD0** commit (198-target closure) | the 6 own (seq4273-4278) + the 6 nested in the kind-0 captures | 12 of 12 | **12/12** | **PROVEN**, own-block 7/63 = 0.1111 |
| 0x14079AFD0, new captures | pid1792161 seq3889-3892 (4 `trap_commit_79AFD0`; traced here with `frame_trace.py recorded ... --out`) + seq4273-4278 | 10 of 10 | **10/10** | PROVEN (scratch run, not folded into the committed counts) |

Kind-0 captures that exist: exactly the 7 above (`live2/pid*/seq*/index.json` scanned for `trap_kind0_79A470`; pid1645544/1660292/1675604/1679850/1694737 have none). The xmm0 mismatches of seq4351/4402/4531 were the same defect (same closure, same record byte).

Recorded: `results_core.json` gets two records (`diffed: true`, `proof: "snapdiff"`, `snapdiff.source: tools/dev/translate/results_trap.json`, counts only, no values), commit `fc84d383`. `driver --check` skips snapdiff-proof records (corpus = captures). Not recorded: PES-derived captures stay in scratch.

## 5. Pdata migration (CONFIRMED)

`argroots.pdata_functions` and `recordmap.pdata_ends` (own .pdata readers) are deleted; both read `pdata.Pdata(f).entries` (every entry, as before; `Pdata.dropped` counts corrupt entries). Outputs compared with the old readers on the image: `recordmap.py analyze` byte-identical (390 functions); `argroots.py` identical except `pdata_dropped` in `impure_roots_evidence.json` 58 -> **29** (the old reader counted zero padding entries; 29 = the corrupt entries TrapChain2 reported; file regenerated). Neither tool uses `Pdata.extent` yet: they still treat chunk entries as separate "functions" as before (INF: argroots would type a call site inside a chunk against the chunk, not its function; no site in the 8 typed roots is affected, output unchanged). Commit `5d63e2c2`.

## 6. Verification run

`test_setcc_store.py` 2 OK, `test_extent.py` 19 OK, `test_closure_walk.py` 13 OK (harness Python, `PES_EXE` set); 1 mutation run and reverted; `gen_translated.py --check` byte-identical; `driver.py --check` on the 2 changed recorded files bit-exact; `snapdiff.py` as in §4. No C++ test binary built or run (no C++ changed; the layer gained files, Main's gate compiles them). Gate not run (Main gates): run `gen_translated.py --check`, `translate_setcc_store_test`, `translate_extent_test`, `translate_closure_walk_test`, `diff_*` with `GF_DIFF_PYTHON`, `PES_EXE`. Scratch: `<workdrive>/gf-TrapChain3-scratch/` (lockstep traces `h_4310.pcs`, `t_4310.pcs`; `ls4310.sh`, `sd_*.sh`).

## 7. Unfinished (numbered)

1. GF side of the chain (unchanged from TrapChain/TrapChain2 §5-6): records of the §5 list in a PES-layout window (MatchInfo's `pesworldrecord*`), `_TrapCommand` on the translated chain, delete `PickTrapClip`, re-enable `PesStall.DISABLED_...`. The translated roots are now committed and proven, so this is unblocked.
2. `results_trap.json` counts 79AFD0 as 12/12; the 4 pid1792161 captures (10/10 with seq4273-4278) are not folded in.
3. argroots: use `Pdata.extent` so a site inside a chunk resolves to its function (output unchanged today).
4. `0x14202BA30` held out by `gen_translated` (callee `0x14202C250` mistranslated): pre-existing, not looked at.
