# Danger writer 0x140699960 and pressure builder 0x14069A500 proven on real calls; forced-clear decoded; GF wiring blocked on the builder (MatchInfo4, 29-10-26)

Branch `EndClear` (worktree `gf-EndClear`), pushed head `65110b24` (confirm: `git ls-remote origin EndClear`). Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled (blocker named). Gate not run (Main gates).

## 0. Verdict

| Step | State |
|---|---|
| 1 merge origin/main, push | DONE (`e84c026c`). Also merged `origin/StoppageFlow` (`9d80d134`: its bounded decode of pdata-less thunks is needed by this closure; merged clean). |
| 2 danger flag | **PARTLY.** Writer `0x140699960` translated (closure 193 members with the builder) and **PROVEN 10/10 bit-exact on real calls**, own-block 0.7345 (130/177). Pressure builder `0x14069A500` (feeds the writer's input record) **PROVEN 6/6**, own-block 0.9596. Builder `0x142119390` NOT proven (blockers below). **The GF stand-in in `elizacontroller.cpp` is NOT deleted**: writing imageA+0x31C1 from the translated writer needs the pressure record built by 0x14069A500, which is proven but not yet in the committed layer (§3). |
| 3 forced clear | **Decoded** (§4), not ported: its inputs are the builder's outputs (imageA lists and pressure record). `forcedClear` stays false. Hook captured. |
| 4 angle inputs | Decoded two (§5); no GF writer added. |
| 5 binaries | All 8 left-unrun binaries + 3 more run: GREEN (§6). |

## 1. Git

- `git merge origin/main`: clean (`e84c026c`). Symlink trap (CONFIRMED): plain git fails on `extern/godot-cpp`; used `git -c submodule.recurse=false -c status.submodule=none -c diff.ignoreSubmodules=all`, identity `-c` before the subcommand.
- Pushed commits: red memmove test, memmove model, writer closure + layer regen (4246 functions, 3687 recorded bit-exact, 559 closure), results, proofs.

## 2. Captures (rendezvous)

- First danger captures: `live2/pid1792161/seq16184` (builder), `seq16185` (writer), `seq16456/16457`. The earlier misses were the disk floor (other hooks' captures filled the headroom; Main then restricted the watcher to an allowlist).
- Later: `pid1965411` seq033/034 (builder/writer call 1), seq495/496 (call 2001), walk seq035/627; `pid1981120` writer seq4048, 5316, 5771, 6464, 6755, 7080; pressure seq4047, 5315, 5770, 6463, 6754, 7079; builder seq4046, 4444, 4891, 5314, 5769, 6161; walk seq3914, 4518, 5130, 5878, 6294, 6695.
- **All 10 writer calls return danger = 0** (al = 0 in every replay: CONFIRMED by `frame_trace.py recorded`). The danger = 1 arm has no real call (the hook samples by count, so it may take more launches). Own-block coverage 0.7345 means 47 writer blocks, including the arm that returns 1, are unexercised.

## 3. Proofs and tooling

- `closure.py --spec-set core --tag danger 0x142119390 0x140699960`: 193 members (0 universe rows added; 104 committed specs kept; set file `specgen/sets/closure-danger.txt`).
- `frame_trace.py recorded SNAP --hooks hooks.cfg --out TRACE --spec-dir specs/core` then `snapdiff.py --only 0x140699960 0x14069A500 --snapshot SNAP=TRACE ...`: `0x140699960 calls=10 replayed=10 mismatches=0 own_block=0.7345 proven=True`; `0x14069A500 calls=6 replayed=6 mismatches=0 own_block=0.9596 proven=True` (CONFIRMED, last run). Counts file `tools/dev/translate/results_danger.json`; the writer is a record in `results_core.json` (`proof: snapdiff`) and is in the committed layer (`src/pes/translated/ball/pes_fn_140699960.cpp`).
- **The pressure builder is NOT yet a layer record**: recording it changes the layer (new files, `translated.hpp`, dispatch table), and `gen_translated.py --check` must stay current. Main flips it: add a `proof: snapdiff` record for `0x14069A500` to `results_core.json` from `results_danger.json`, run `gen_translated.py`, commit. (A regen is 11 min, a rebuild of `pestranslated` about an hour.)
- **New harness/translator model: `VCRUNTIME140!memmove`** (the danger closure's lane-list insert `0x140A790AB` calls IAT slot `0x14252FD68`; the first replays died at `unmodelled OS export VCRUNTIME140.dll!memmove`). `import_stubs.json` (code_hex model, backward copy when `src < dst < src + n`), `import_slots.json` (`0x14252FD68`), `xlate.import_model_lines` (C++ mirror, same registers and stack bytes). Red first: `test_memmove_model.py` failed 7/7 with `KeyError`; green 7/7; mutation std->nop caught by `test_overlap_destination_above_source_is_copied_backward`, reverted. The mirror is checked by the 10 writer replays (they reach memmove: `140A79050` runs twice per call).
- Not done: **builder `0x142119390`**. Its replay stops at the `powf` thunk `0x1415A17CE` (libm: no harness model; asked of NativeSeam) and it allocates 15 times through `0x1403FB410` (NativeSeam's native heap, branch not on main). Also not translated: memset `0x1415A1720` (reached only through the allocator pool constructor `0x1403FCD90`, never in a replay).
- The merge of `origin/StoppageFlow` was needed: before it, `gen_translated` failed with 10 conflicts on the import thunks `0x1415A1708` and `0x1415A1720` (their decode ran into the next function under one set).

## 4. Forced clear (rule 7) decoded (CONFIRMED from `~/ghidra-projects/scratch/Unpacker/out/carunits5.c`, walk `0x14210F110`; constants read from `pes_full.bin`, instruction/operand VA pairs from capstone)

The walk loops the priority list at BP+0x3E20 (`0x14211EF40` kind at index, `0x14211EF20` answer record). Its return (`edi` at `0x142110267`, CONFIRMED by disassembly) is the chosen kind: `0x14210023A..` `mov edi,3|4|9|6` are the replacements, `mov edi, r14d` accepts the current kind, `0x142110264` returns the loop's stored candidate (starts 0x19 = none).

Entered when the current answer type is 5 (Clear). Let P = `0x14069A3E0(imageA+0x2124, 0xFF)` (the pressure record the builder step `0x14069A500` fills), E0 = first event (`0x140A3E190(E,0)`), BP = outer record (param_1; imageA = BP+0xEC).

1. **Skip gate** (L190-209): if `P+0x30 == 0 || P+0x34 < 2`: when BP+0xE4 == 0 and E0 kind in {1,2,3,0x10} and E0[1] != BP+0x24, go to 3; else if `0x141052E30(BP+0x32B0) != 0` and answer `BP+0x4204` valid (`0x142119DF0`) and `[BP+0x4204] == 3` and `[BP+0x420C] > 2` and `0x140481B00(BP+0x3380, [BP+0x4210]) < 60.0` (`0x14210F226 movss xmm13,[0x14259A764]`), **skip this unit** (continue).
2. (else falls to 3.)
3. `own = (E0 kind in {1,2,3,0x10}) ? E0[1] == BP+0x24 : 0`; `opp = 0x140A4EDF0(ctx, side-of-opponent, BP+0x1C, 0)`; `opp > 0x15` skips the unit; `z = 0x140A4C820(ctx, BP+0x1C, BP+0x24)` (pitch-zone test, MatchInfo3 §2); band `th = z ? 90.0 (0x14259BE5C) : 135.0 (0x14259BE60)`.
4. N1 = max over the 11 opponents of the list entries' count fields: for each k and each of the two list kinds (`0x142118C90/D40` over BP+0xC58 = imageA+0xB6C): the entry's `+0xE8 >= th` -> `0x140481B00(entry+0xE0, P+0x50+0x1C*(0x1404587B0(opp)))` > th lifts N1 to the entry's first int.
5. cls = min lane class over the 11 lanes (`P+0x4C+0x1C*k`, predicates `0x140A6E450/E0F0/E3C0/E4B0/E580` on `0x140A48910` records and `P+0x10`, `P+0x14/0x18/0x1C`, 1.5 `0x14259BDDC`, 90.0 `0x14259BE5C`), start 3.
6. **Forced clear** = the replacement chain is NOT entered. The chain (3, 4, 9, 6 in that order, each only if `0x14211EF80(table, cur, k)` and the answer is valid `0x142119DF0`) is entered when `F997` is reached: from the outer test `(ball height < 1.0 (0x14256AE1C) || MA+0x24830 < 1.0 || own || cls > 2 || N1 > 4)` true and then either `N1 >= 4 || cls >= 3`, or (`N1 < 4 && cls < 3`) with `dist(MA+0x228D4+0x58*BP+0x1C, ball copy) >= 90.0` or `P+0x4C+0x1C*(opp%11) - dist(man,MA ball) >= 2.0 (0x142565604)`; at `F997` it runs iff `!0x140A4C7A0(ctx, man+0x55C, side) || N1 > 3 || cls > 2 || P+0x30 < 2`. Everything else (the outer test false, or F997's guard false) sets the candidate to the clear and continues: forced. A kind-6 answer additionally returns to the clear when `BP+0xE4 == 0` and `0x14063E960`-class test on the target's distance > 90.0.
- The GF walk's `forcedClear` bool maps to "chain not entered". Not ported because N1, cls and the guards read imageA+0xB6C (lists), P (imageA+0x2124) and BP+0x3380/0x4204, all outputs of the builder `0x142119390` (flags & 6 builds the lists, flags & 1 the pressure record). Without the translated builder the window holds none of them.
- **Hook**: `carrier_walk_210F110` captured (pid1965411 seq035/627; pid1981120 seq3914, 4518, 5130, 5878, 6294, 6695). To use it as the oracle the walk must be translated: `closure.py --tag walk 0x14210F110` gives 87 members (5 new specs, rest already in the layer); I dropped those specs again to keep this change small. The walk then becomes the differential oracle for a C++ `forcedClear` on a PES-layout unit table.

## 5. Angle inputs (CONFIRMED decodes, no GF writer added)

- **Man +0x5D0** (7 store sites): the stores at `0x140431F73` (`0x140431DC0`: man-record copy from param_2), `0x1404964B1` (`0x140496440`) and `0x1404989D7` (`0x140498970`) zero a qword, `0x14049FBBB`/`0x1404BCDA5`/`0x1404F0147` are qword stores `[rbx+0x5D0]` after `[rbx+0x5C8]`, and `0x140413965` stores r12 after `call 0x141B55850` on `[rsi+0x5D0]` (a release). INF: +0x5C8/+0x5D0 is an object handle pair (a pointer cleared on release), not an s32 action word; GF's name `kManActionOffset` (s32) is wrong, and the angle's compare reads the low dword of a pointer. The live pointer is assigned by a non-constant-offset helper; not found.
- **Ball +0x1C/+0x24 and +0x4C**: written by `0x140875FD0(obj, idx, vec)` (CONFIRMED decompile): `*(obj + 0x1C + 12*idx) = vec` for idx < 3, and for idx == 0 `obj+0x4C = (vec.y >= 0)`. Its only caller is `0x140874100` (CONFIRMED asm): a 3-slot collector (counts at +0x1D/+0x1E, indices stored at obj+8 table +0x40/+0x74) that appends the first point of a ball prediction sample whose y >= `[0x14259...]`-compared threshold and whose height-velocity test `[obj+0x14] < 0 < [vec+0x10]` holds. So B+0x1C/+0x24 is the first of three predicted crossing points, not the current ball position: GF would need its own ball-prediction (B+0x88 samples) feeding this collector. Not ported. +0x80 not decoded.
- Anime +0x11C, slot +0xA1/+0xA4: not decoded (budget).

## 6. Verification (all `tools/dev/simslot.sh timeout 900`, GF_SIM_SLOTS=4)

| Binary | Result |
|---|---|
| `pes_events_test` | 39/39 |
| `pes_possession_test` | 24/24 |
| `pes_event_wiring_test` | 4/4 |
| `pes_team_tick_test` | 26/26 |
| `pes_clock_match_test` | 2/2 |
| `pes_match_info_test` | 2/2 |
| `pes_involvement_test` | 6/6 |
| `pes_world_test` | 6/6 |
| `pes_tick_a1_test` | 26/26 |
| `pes_carrier_test` | 189/189 |
| `pes_angle_oracle_test` | 4 skipped (`GF_PES_ANGLE_ORACLE` not set in this run) |
| `test_memmove_model.py` | 7/7 (7 red before the model) |

No GF C++ file changed; every `pes_*` binary that links the layer is affected only through the regenerated layer, and I ran the eleven above after the full `pestranslated` rebuild. The other `pes_*` binaries were not run. `gen_translated.py --check`: see the final message line.

Mutation: memmove model std -> nop (one run, rebuilt/reverted by file copy): FAILED `test_overlap_destination_above_source_is_copied_backward` (1 of 7), the others stay green, as designed.

## 7. Open items (numbered)

1. **Delete the danger stand-in**: record `0x14069A500` in the layer (Main: see §3), then add to the window a BP block (imageA at BP+0xEC: lists, pressure record, danger byte; BP+0x1C unit {slot, side BP+0x24, sign float BP+0x30}; K at BP+0x78, updated by `0x14063A490`) in place of `PesClearUnitBlock`, call `0x14069A500` then `0x140699960` and store al at imageA+0x31C1; the builder's tail rule (`0x142119714-0x142119770`: `sign*x < pitch*c/c2 && P+0x30 != 0 && 0x142118CB0(imageA+0xB6C,3,1) == 0` sets 1) needs the lists (flags & 6, `0x142117C30`) which are in the unproven builder closure.
2. **Builder `0x142119390`**: a `powf` harness model (libm, NativeSeam) and the native heap (NativeSeam) are its blockers (15 allocations per call).
3. **Forced clear**: translate the walk (87 members) and use it as the oracle, or port §4 over the builder's lists; `forcedClear` stays false until then.
4. **Danger = 1 real call**: none yet (10/10 return 0); the 47 uncovered writer blocks include its arm.
5. Angle inputs (§5): ball prediction collector `0x140874100`, man +0x5C8/+0x5D0 handle, anime +0x11C, slot +0xA1/+0xA4.
6. Cleanup: remove `gf-EndClear-scratch` (traces, snapdiff builds) once archived.
