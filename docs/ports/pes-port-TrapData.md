# pes-port-TrapData: the trap chain's loader data (motion records, Animetable buffer, constant::Trap) built from the install and equal to six captures (29-10-26)

Branch `TrapData` (from `origin/StoppageFlow` `6a868ee0`, `origin/main` merged; `git ls-remote origin TrapData` is the head after the final push). Slice S2 of `docs/re/pes-re-trap-inputs.md` §6. Tags: **CONFIRMED** = a tool run this session (test, decompile, capture bytes); **INF** = inference; **OPEN** = not settled. Image `pes_full.bin`, base `0x140000000`. Captures: `pid463316/seq4469, seq4531`, `pid1467004/seq019`, `pid1981120/seq23879, seq23906, seq23932` (never deleted or slimmed).

## 0. Verdict

| Acceptance | State |
|---|---|
| Every loader byte the chain reads is produced from the install and equals the capture | **DONE on the six kind-0 captures**: `pes_trap_data_test.EveryLoaderByteTheChainReadIsProducedFromTheInstall` (exact per-byte read lists from a unicorn replay of each capture: 73+186+50 bytes for seq23906, 73+170+12 seq4469, 37+91+146 seq4531, plus the 21 `constant::Trap` payload bytes and the 8-byte pointer cells in each of the three that reach the registry; the other three captures read **no** loader data) |
| Stronger than asked: the **whole** structures equal the capture | **DONE**: `TheWholeStructuresEqualTheCapture`: the full Animetable buffer (0x2824E bytes), every kind-0/1/6/13 list of all 0x1125 motions (counts, caps and the raw records behind the pointers) and 413 of 472 bytes of the `constant::Trap` object (all 120 stores of its loader) equal the live process, in 6/6 (5/6 for the Trap object, see §3) |
| `trap.o` runtime fields the report left unmapped | **ALL MAPPED** (CONFIRMED): the runtime object is built by the class's own binary loader, vtable `0x142B53158` slot 2 = `FUN_141E67B70`, not by the JSON accessors `FUN_141E681C0` names (that is slot 1, the text-file path) |
| Chain run on install-built externals (S2's "six captures give the same issued flag, out word and action bytes") | **NOT DONE**, see §6 item 1 |

## 1. What landed

- `src/sim/onthepitch/pestrapdata.{hpp,cpp}`: `BuildTrapLoaderData(exe, vfs, &err)` -> one segment at `kTrapLoaderVa = 0x2100000000` (above the world window `0x2000100000`) holding: the motion-table singleton, the anime manager, the registry object/array/entry, the `constant::Trap` object, the Animetable buffer, the 0x1125 x 0x390 record block and the four raw Mbinfo files; plus the three image globals `0x1436F23C0`, `0x1436F2378`, `0x1437F4808` as `(VA, pointer)` pairs. `PesTrapLoaderData::AddTo(PesTrapExternals&)` plugs it into the existing externals (no edit to `pestrapworld.hpp`).
- `src/sim/onthepitch/pestrapconstant_fields.inc`: 120 rows `{runtime offset, bytes, leaf offset, chain, offset-word chain}` **generated** by `tools/dev/translate/gen_trapconstant.py` from the decompile of `FUN_141E67B70` (`curl .../decompile_function?address=0x141e67b70`). Not a value table: it is the loader's store list; every value comes from `trap.o` through `ReadConstantPlayerObject`.
- `src/pes/animetable.{hpp,cpp}`: `Animetable::BufferCell(row, col)` (the file cell as `FUN_1406AA440` stores it: no patch, mirror bit kept on an empty cell).
- `tools/dev/translate/trap_reads.py`: replays a capture (`trap_oracle.replay`) and writes `<capture>.reads.tsv` (the exact loader bytes read, in logical coordinates) and `<capture>.full.tsv` (the whole structures). PES-derived, never committed; usage in its docstring.
- `tests/pes_trap_data_test.cpp` (+ `tests/CMakeLists.txt`), `tests/pes_animetable_test.cpp` (`BufferCellIsTheFileCellWithoutThePatch`).

## 2. Decodes

**Motion table (CONFIRMED by the whole-structure test, 6/6).** `FUN_1406AEF60` dispatches per kind to the builders; for the four kinds the chain reads I decompiled each: kind 0 `FUN_1406AC4D0` (record 36, key `w0 & 0x1FFF`), kind 1 `FUN_1406AD300` (108, key `(w0 & 0x1FFF) + 0x11`), kind 6 `FUN_1406AE4F0` (8, `w0 & 0x1FFF`), kind 13 `FUN_1406AC660` (20, **`w0 & 0x1FFF`**: `pes-re-gaps-anime.md` §3 line 477 wrongly says `+0x11` for kind 13; its own §2 table and the decompile say no add). Each builder: `if (count byte rec[kind] < DAT_1425DC370[kind]) { list(kind)[count] = record pointer; ++count }`, `list(kind) = rec + FUN_1406AE810's offset` (0x18, 0x20, 0xC8, 0x210). The records are **pointers into the file buffer** (`FUN_1415A1708` copies the file in once; the builders store `buf + i*recordSize`). The caps are read from the exe (`0x1425DC370 + 4*kind`).
**Animetable buffer (CONFIRMED).** `FUN_1406AA440`: s16 cells `(file & 0x1FFF) - 1` for 0xBE5 x 0x12 cells (109,620 bytes), **then one flag byte per cell** (`= file >> 13 & 1`, at `+0x1AC34`), 164,430 = 0x2824E bytes. The flag array is read by `FUN_1406ABD60` (`*(buf + col + (row + 0x17CA) * 0x12)`), which is what the six captures' extra single-byte reads were (the TrapInputs report counted only the s16 part). The patch cell (0x7C5, 2) is applied by the lookups `FUN_1406ABB30`/`ABD60`, not stored in the buffer.
**constant::Trap (CONFIRMED).** The object tree of `trap.o` is offset-linked (each u32 word is an offset from the object start, 0 = null; `FUN_141E823E0`). Slot 2 `FUN_141E67B70` copies leaves into the runtime object; the generated rows are its stores (e.g. `+0xD4` = byte at `w[7]->+0`; `+0x12C` = u32 at `w[0xD]->+0`; `+0x150` = byte at `w[0xD]->w[1]->+8`; `+0x17C/+0x184/+0x18C/+0x194` = f32 at `w[0xD]->w[4]->{w[0]->{w[0],w[1]}, w[1]->{w[0],w[1]}}`; `+0x1A0` = byte at `w[0xD]->+0x14`; `+0x1CC` = u32 at `w[0x13]->+0`). Disk offsets `0x350/0x360/0x3E0` of the report's value matches are the leaves of those chains. **The airBall/groundBall ambiguity is resolved by the chain, not by values.**
**Registry (CONFIRMED by read lists).** `*(0x1437F4808)` +0 = entry array; `array[0x3E]` = entry; `*(entry+8)` = payload (the Trap object). The chain reads only those cells.

## 3. Oracle results (CONFIRMED, dev build of `pes_trap_data_test`, `PES21_ROOT` set, `GF_PES_TRAP_READS` = the six `.reads.tsv/.full.tsv`)

4/4 pass: `EveryLoaderByteTheChainReadIsProducedFromTheInstall`, `TheGlobalsReachTheLoaderObjects`, `TheWholeStructuresEqualTheCapture`, `TrapDataRefusal.AnUnmountedInstallBuildsNothing` (an empty VFS is refused with the file named). `pes_animetable_test` 5/5.
**Mounts.** The install's VFS winner for `trap.o` is the 4cc gameplay mod's (`download/4cc_06_gameplay.cpk`); five captures (pid463316, pid1981120) hold exactly the mod's object (0 differing built bytes against the mod, 69 against stock). **`pid1467004/seq019` ran with a third `constant_player.bin`** (23 built bytes differ from stock, 84 from the mod, all in the reach/ground/air reaction floats): its Trap object is not compared (`trap_reads.py ...:notrap`); its call reads no loader data. No value is pinned: every assertion is capture-bytes == install-built bytes. Stock-vs-mod: nothing in code or tests quotes a `trap.o` value.
**Not built, listed (never read by these calls):** the 59 padding bytes of the Trap object (3-byte gaps after byte stores; the capture holds heap residue there, e.g. `+0x19..0x1B`, `+0x1A1..0x1A3`, so they are not a function of the file); the default motion record (`table+8`); count bytes and lists of the other 18 Mbinfo kinds; the table singleton's other fields (`+0x10..` file buffers, `+0xC0` loaded flags), the manager's other words, registry entries other than 0x3E (the registry's end/capacity words are set to the materialized span), the entry's id/name fields. The read lists confirm none is read in the six calls (the test fails if a `D` row, or an `M/T/G/R/A/E` byte outside the built set, appears).
**Reads that are not loader data (for TrapP, S1):** in all six captures the remaining unnamed reads are `P+0xA08 -> +0xF0` (8 B) and its f32 at `+0xC0`, `P+0x3AC0` object `+0x18`/`+0xE0` (8 B each), thread data (`0x7ff...`, the stack cookie), and in seq4531/seq019/seq23906 a **48-byte table** read by `0x140A3B320` (finds a slot id among 12 dwords of a side record: `IMUL eax,[rcx],0xB`). That table is not in the window's typed records.

## 4. Mutations (each run, rebuilt, reverted; the mutated copy lived in scratch, the source was not touched)

| # | Mutation | Killed by |
|---|---|---|
| 1 | HitData key without `+0x11` | `EveryLoaderByte...`, `WholeStructures` |
| 2 | list cap not applied | `WholeStructures` only (a cap overflow is not in the read set) |
| 3 | RotData list offset `+8` | `EveryLoaderByte...`, `WholeStructures` |
| 4 | Animetable flag bytes 0 | `EveryLoaderByte...`, `WholeStructures` |
| 5 | `BufferCell` mirror shift `+1` | both install tests and `BufferCellIsTheFileCellWithoutThePatch` |
| 6 | s16 cell `motion + 1` (no `- 1`) | `EveryLoaderByte...`, `WholeStructures` |
| 7 | registry entry's payload pointer at `+16` | `TheGlobalsReachTheLoaderObjects` |
| 8 | `BufferCell` drops the mirror bit on an empty cell | **install tests pass (equivalent: 0 of 40,148 empty cells carry the bit in this install)**; killed by the unit test |
| 9 | field row `+0x18C` chain through the air-ball subtree | `EveryLoaderByte...`, `WholeStructures` |

## 5. Verification run

- Gate not run (Main gates). **`GATE_TIER` does not exist in this branch's `tools/dev/gate.sh`**, so the fast-tier line is the fallback the brief allows: `GATE fast: NOT RUN (no GATE_TIER on this branch); ran pes_trap_data_test 4/4 and pes_animetable_test 5/5 (dev link: pesfmt + the new files, install and captures present); gen_translated.py --check: see the final message`.
- **Build caveat.** `pes_trap_data_test` is registered in `tests/CMakeLists.txt` against `gfsim pesfmt pestranslated` like the oracle test, but `pestranslated` is 4,387 objects and did not build in this worktree in the budget (killed at ~100 objects). I built the test and `pesfmt` with CMake and linked the test by hand (`c++ ... tests/pes_trap_data_test.cpp src/sim/onthepitch/pestrapdata.cpp <shim for ReadConstantPlayerObject> libpesfmt.a gtest`); the shim has the same body as `ReadConstantPlayerObject`. **Main must build the real `pes_trap_data_test` target on the merged tree.**
- Scratch `<workdrive>/gf-TrapData-scratch/` (`reads/*.tsv` extractor output, `run.sh`, `mut/` mutants, `dev/` dev build, decompiles `dec_67b70.c`): Main removes it with the worktree.

## 6. Open items

1. **Chain run on install-built externals** (S2's own oracle text: issued flag, out word, action bytes). Needs TrapP's oracle directories (`trap_oracle.py`, S0) and a built `pestranslated`. Wiring: replace the oracle's `H` segments and loader `I` globals by `data->AddTo(ext)` (the segment is at `kTrapLoaderVa`; the 48-byte side table above is still an `H` segment). Expected: equal, because every loader byte the chain read equals the capture.
2. `pestrapdata.cpp` hands the 4.9 MB segment to `PesMem::Seg` by copy per call (`RunTrapKind0`); S3 should build it once per match and share it (a `PesMem` view of a shared buffer).
3. The default motion record (`table+8`) and motions `>= 0x1125` (PES files those records past its own block): not modelled (`ponytail:` in the code).
4. `FUN_141E67B70` stores as a generated table need the Ghidra bridge to regenerate (`gen_trapconstant.py`); a capstone symbolic pass over the image would remove that dependency.
5. Provenance of `pid1467004`'s constant_player.bin (a third `trap.o`): not identified.
6. `pes-re-gaps-anime.md` line 477 (kind 13 `+0x11`) is wrong for kind 13 (§2); fix when archiving.
