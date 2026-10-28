# PES Clear unit (kind 1) and corridor angle: real-call proofs, GF wiring blocked (EndClear3)

Branch `EndClear`, worktree `gf-EndClear`. Pushed head `c5c7a999` (`git ls-remote origin EndClear` = `c5c7a999889dd15e7ee8a8efa97f0fe5c8418e32`). Earlier pushes: `18922346` (merge of origin/main 5c9f02f3 and origin/RecordMap 3091f83f). Tags: CONFIRMED = read in tool output this session or a replay verdict; INF = inference; OPEN = not settled.

## 0. Verdict

- **Proven from real calls (bit-exact on every replayed call, no exclusions):** the corridor angle `0x142108630` (6/6), its helpers `0x14063A290` (6/6), `0x14063A490` (6/6), packer B `0x14063D2F0` (12/12), and the Clear unit `0x142109EB0` on its **danger-0 arm only** (5/5).
- **Not proven:** the Clear unit's danger-1 arm. No real Clear call with danger set exists in the captures (§2.1). Its own wrapper code on that arm (store answer type 5, call the angle, store the float) is unverified.
- **Not done:** steps 3, 4, 5 of the assignment. No GF code changed. The danger flag has no ported writer, and the angle needs PES world state GF does not carry (§4). The stand-ins at `elizacontroller.cpp:2774` and `:2951` remain. `FillUnitTable` kind 1 stays unanswered. `forcedClear` is still unset. No red test, no green, no mutation.
- Test counts: 34 `pes_*` binaries built and run, 758 passed, 0 failed, 3 skipped (§5). No C++ file in this change was modified, so no test binary links a changed file.

## 1. Git

- `git merge origin/main` (5c9f02f3) and `git merge origin/RecordMap` (3091f83f) landed clean.
- The symlink trap: `extern/godot-cpp` is a symlink in the worktree. Plain `git fetch`, `merge` and `push` fail with `expected submodule path 'extern/godot-cpp' not to be a symbolic link` (CONFIRMED). The commands that worked: `git -c fetch.recurseSubmodules=false fetch origin`, `git -c submodule.recurse=false -c status.submodule=none merge`, and `git -c push.recurseSubmodules=no -c status.submodule=none push`. The trailing error after a merge did not block the commit.
- Commits pushed: `18922346` (merge), `c5c7a999` (spec fix and results, §3).

## 2. Proofs

Method: `frame_trace.py recorded SNAP --hooks hooks.cfg --out TRACE --spec-dir specs/core` on each capture of `live2/pid407792`, then `snapdiff.py --hooks hooks.cfg --snapshot SNAP=TRACE --only VA`. Replay is from the snapshot's own memory, with the recorded GPRs, XMM, RFLAGS and MXCSR. Verdict per call: rax, xmm0 and every written byte equal.

| VA | Function | Real calls (captures) | Bit-exact | Own-block coverage | Context of the calls |
|---|---|---|---|---|---|
| 0x142109EB0 | Clear unit | 5 (seq019, 047, 076, 095, 116) | 5/5 | 2 of 4 blocks (0.50) | all danger = 0: returns with no store, no angle call |
| 0x142108630 | corridor angle | 6 (seq021, 028, 029, 032, 034, 035) | 6/6 | 187 of 366 (0.511) | all entered from Clear at return 0x142109EE0 (danger = 1) |
| 0x14063A290 | snapshot zero + 0x14063A490 | 6 | 6/6 | 2 of 2 (1.0) | from the angle |
| 0x14063A490 | snapshot fill | 6 | 6/6 | 116 of 222 (0.522) | from the angle |
| 0x14063D2F0 | packer B | 12 | 12/12 | 6 of 6 (1.0) | from the angle |

- Replay verdicts (`frame_trace.py recorded`, no exclusions): the five Clear captures and six angle captures all return cleanly (`err` null), about 0.4 s each. The Clear captures return to 0x14211EEE3 without an angle call.
- Case counts: 5 + 6 + 6 + 6 + 12 = 35 real calls across 11 distinct captures (seq019, 047, 076, 095, 116 for Clear; seq021, 028, 029, 032, 034, 035 for the angle, which also feeds the three helpers).
- Coverage is per block (EndClear2 §0 method). The 2 uncovered Clear blocks are the danger-1 arm (`mov [rdx],5`, the angle call, the float store). That is why the Clear proof is partial.
- Result files: `tools/dev/translate/results_endclear_clear.json` (Clear) and `tools/dev/translate/results_endclear_angle.json` (the other four). Counts only, no mismatch values.

## 3. Two harness fixes that were needed (CONFIRMED, both reproduced)

1. **Stack-cookie stub.** The first angle run failed on every call with `PES_DISPATCH_UNTRANSLATED: 0x14159f3e0` (`__security_check_cookie`, closure). The fix is `stub_refs: ["14159F3E0", "1415A1780"]` in `specs/core/142108630.json`, `14063A290.json`, `14063A490.json`. `stubs.json`/`stub_table` models the cookie check as a no-op (`engine-noop`) and `sqrtf` as `sqrt_xmm0`. After the fix, 14063A290 and 14063A490 became bit-exact. Caveat: the specs were hand-edited, and the generic fix (`classify.reach_stubs` plus `harness_fields`, RvReplay `d8a61a62`) is on `origin/RvReplay`, not on main. The specs should be regenerated with specgen once RvReplay lands.
2. **Hook patch at the entry.** Without `--hooks`, snapdiff replays the entry bytes as the hook left them (`jmp [rip+0]`). Every Clear and angle call mismatched on the written bytes (`w`): the translated side wrote the correct `push rbx`, the harness wrote a different value. With `--hooks <workdrive>/PES21/rendezvous/hooks.cfg`, all 11 calls match. This is why the proof needs `--hooks` for any hooked entry.

## 4. What is needed for GF wiring (steps 3–5), with the decode

- **Danger flag writer (CONFIRMED, decompile of `0x140699960` and `0x142119390`):** `FUN_140699960` returns a byte. The builder `FUN_142119390` stores it at `param_1+0x31C1` (imageA) at its sites near `0x1421196A7`: `*(param_1+0x31C1) = uVar7`. A second rule sets `+0x31C1 = 1` under `func_0x142118CB0(param_1+0xB6C, 3, 1) == 0` and the distance test. The writer's body is about 300 decompile lines with about 30 callees (lane readers `0x140A48A30/0x140A489F0/0x140A48A10/0x140A48910`, the `0x140A6E*` predicates, `0x14043A0A0`, `0x140439FE0/0x140439F50`). Neither function is translated or proven. Neither has a capture: no hook on either entry (`hooks.cfg` has 32 hooks; the two VAs are not among them).
- **Angle world inputs (CONFIRMED from the spec and the captures):** `ctx` (A0, the Clear unit's `+8`): `+0x18` (readable to +0x2601C) and `+0xE0` (the E0 ring, `+0x1900` count, `+0x1904` head). Match global G: `+0x40` (carrier record, identity OPEN, readable to +0x2954), `+0x270` (MI), `+0x360`, `+0x2AA8`, and the per-player accessors (`0x140A48A10/0x140A48910/0x140A489F0`) reading the anime/man/slot tables. The lane reader `0x140A714E0` reads `[RCX+0x22B9C]` and `[RCX+0x229B4]`. GF's `PesWorld` (`src/sim/onthepitch/pesworldrecord.hpp`) holds G's header tables and 22 player blocks only. G+0x40's record, the E0 ring, the team records and the lane state are not in it, and no GF writer produces them.
- The Clear unit's record (`unit+0` carrier record, `unit+0x28` imageA) is still a heap object with no GF owner (RecordIds §2, OPEN).
- Conclusion: GF cannot yet run the translated angle from its own state. Porting the danger writer and the world-window writers is the work, and it is larger than this ticket. Deleting the stand-ins now would leave the danger flag and the angle with no source, so I did not touch the stand-ins.

## 5. Verification run

- **Build:** the 34 `pes_*` targets (`tests/CMakeLists.txt`) built with `nice -n 19 ionice -c3 cmake --build build -j3` (BUILD_RC=0, 360 s).
- **Run:** each binary via `tools/dev/simslot.sh timeout 600 nice -n 19 ./build/tests/<name>` (rc 0 for all 34). Totals: 758 passed, 0 failed, 3 skipped (`pes_editsave_test` 1, `pes_formats_test` 1, `pes_stadium_test` 1: existing `GTEST_SKIP`s). Per-binary counts: animetable 4, anim 14, camera 17, clip_pose 10, clock 51, condition_loader 2, derivation 22, dodge 45, editsave 7 (1 skip), effects 7, events 39, exe 3, fmdl_uv 2, formation 15, formats 25 (1 skip), fox2 4, handseq 8, lighting 13, locomotion 4, marking 15, mbinfo 9, movestep_run 99, movestep_step 46, movestep 114, movestep_turn 55, outside 8, playerdb 8, playstate 4, possession 25, rig 14, stadium 10 (1 skip), strcode 2, tackle 21, uniform 36.
- **Test binaries linking a changed file:** none. No C++ file changed in this ticket. The merged `src/pes/pesrecordmap.hpp` is unwired.
- **Red/green and mutations:** none. No GF code changed, and there is no red test to commit without a green implementation (the suite must stay green). Mutations of the proof harness are not reported here; the one wiring mutation the brief asks for depends on §4.
- **Seed-7 10-minute `--pes-loop` run (clears / corners / goal kicks vs main):** not run. The branch's GF code equals main's, so the two runs would be identical by construction, and the brief bars agents from `gfsim_run`. Main can run it if wanted.
- **Gate:** not run (Main gates).

## 6. Hook requests for Main (Main owns `hooks.cfg`)

1. Danger writer: `hook danger_writer_699960 0x140699960 14 1 1 40555356574154415541564157488dac` (14 bytes read from the image; `hook_plan.py` must check the window).
2. Danger builder: `hook danger_builder_119390 0x142119390 14 1 1 405553564154415541564157488d6c24`.
3. Clear danger-1 sampling. `carrier_clear_2109EB0` is `FIRST 1 EVERY 3000` (hooks.cfg line 26): every capture is danger = 0 and the danger-1 arm is never seen. A sampler that reaches danger-1 calls is needed. Main's choice how.

## 7. Open items (numbered)

1. Clear danger-1 arm: a real call with imageA+0x31C1 = 1 (request 3). Own-block coverage stays 2 of 4 until then.
2. Danger writer `0x140699960` and builder `0x142119390`: translate and prove from real calls (request 1, 2). Not started beyond the decode in §4.
3. Angle world inputs: G+0x40 record (identity and layout OPEN), G+0x270, G+0x360, G+0x2AA8, the E0 ring (`A0+0xE0`), team records (`+0x250/+0x254`), lane reader state (`0x140A714E0`). Coordinate with the owner of `pesworldrecord*`.
4. GF wiring: `FillUnitTable` kind 1 with `{type 5, a = angle}`, `walk.forcedClear` with the opponent-count test `0x140A4C820` (not decoded this session), and deletion of the stand-ins at `elizacontroller.cpp:2774` and `:2951`, all in one change after 2 and 3.
5. Red then green tests and one mutation per term: not started (blocked on 4).
6. Stub refs in the three specs are hand-added. Regenerate with specgen after RvReplay (`d8a61a62`) merges.
7. Recording these proofs into the committed layer: `gen_translated.py` reads only `results.json`, `results_impure.json`, `results_cutscene.json`, `results_presentation.json` and `results_core.json`, not the snapdiff results. Whether to record them there (`driver.py --record`, about 2.5 h for the full check) is Main's call.
8. Seed-7 10-minute run: not run (§5).
