# CutsceneAnchor10 (CutLibrary): the cut library is the match-constant table, not cut files; the key is a stadium demo-area index; the demo-area node parser is proven and translated; the live node does not match the install's object, so the fill is not wired. Wide test still RED.

Branch `CutsceneAnchor`. Pushed this round: `4d33baa3` (node parser), on top of `4d087301` (builder, CutsceneAnchor9). `git ls-remote origin CutsceneAnchor` = `4d33baa3667d564bee4c428de5401484a42b8f1e`. Labels: CONFIRMED = read or run this round; INF = inference; OPEN = named blocker; SYNTHETIC = values chosen over a real capture's memory. Live reads: read-only `sudo -n dd`/`pread` of `/proc/678718/mem`, no signal, no write, no pause. Ghidra: the headless `dec.sh` (bridge still down).

## 0. Outcome

- Wide test `GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes`: RED, 9 EXPECT failures for DC 5/100/161 (CONFIRMED by run, unchanged): `T[0]` -41.5502167 vs -41.6211472, `pos[0]` 0 vs -41.5502167, `pos[2]` 0 vs 34.
- Landed: step 2's parser (translated and proven, not yet wired). Not landed: the fill (blocked, section 3), the key writer (not found, section 4).
- Decisions applied: (a) VFS boundary as approved; (b) thunk resolved; (c) widened scan and sampler done; the hook is not yet supplied (section 4.4).

## 1. Correction: the 234-entry library is the match-constant table

- The 234 names are literal `.rdata` strings at `0x142B48CA0 + i*0x10` (name pointer, factory pointer), all `DevelopData/common/match/constant/<group>/<name>.json` (CONFIRMED by script over the image; `names.txt` in scratch). None is a `.fdc` file.
- The compiled data is WESYS-zlib archives in `common/match/constant/`: `constant_stadium.bin` (84 objects of 432 B named `demoarea_stNNN.o`, `demoarea_bill_*.o`, `studiumPositionData.o`), `constant_match.bin` (27 objects, see `docs/re/pes-re-ball-constants.md`), and the others. The VFS lists them (`pesfmt_dump ls common/match/constant`, CONFIRMED).
- Index-to-name: index 111 `demoarea_bill_u_shaped`, index 112 `demoarea_st000`, 113 `demoarea_st001`, 158 `demoarea_st046`, 181 `demoarea_st081` (CONFIRMED). The 70 `demoarea_st` names are contiguous at 112..181.
- The index is a table position, not the stadium id: `index = 112 + id` holds for ids 0..52; the id gap at 53 (and 58, 60, 67-69, 72-73, 75-77, 80) shifts later indices (CONFIRMED by script).
- So the else arm's key selects a stadium demo-area constant. INF: key = 112 + the table position of the match stadium's demo area. It is per launch, not per goal (section 4.2).

## 2. (b) The .impdata factory 0x15B35DBD0 (resolved; CONFIRMED)

- `0x141E649B0`: `jmp 0x15B35DBD0`. Live bytes at `0x141E649B0` (`e91b924f19502f0c…`) and at `0x15B35DBD0` (`4883ec284531c08b…`) equal the image bytes: the rig has not patched them.
- `0x15B35DBD0` (the factory for name records 0..?; 34+21+84+... names use other factories, see section 5): `sub rsp,0x28; xor r8d,r8d; mov ecx,[rip-…]; xor ecx,0x4a08b1b; …call 0x1403FB410` (the PES allocator, a native service in production: heap_alloc) with size 0x30; then stores vtable `0x142B502C8`, zeroes +0xC, +0x10, +0x14, +0x1C, +0x24, +0x2C. It returns the object.
- Classification (owner rule 17-11-26): an object constructor and allocation, game logic. No integrity, licence or environment check in its body. The XOR constant `0x4a08b1b` is the allocator's tag argument (the same tag pattern as `0x15B35DBD0`'s neighbours). Not modelled.
- The 0x30 object's vtable `0x142B502C8`: slot +0 `0x141E6FAC0` (dtor), slot +8 `0x141E64A60` (reads 11 named fields from a parsed JSON object: `0x1403842A0` is a `std::map` key lookup), slot +0x10 `0x141E64A10` = `jmp 0x15B35E0C0` (the load method; relocated code, NOT decoded yet: OPEN).

## 3. (a) The fill chain and the VFS boundary (decided; the node parser is proven)

- `0x141E5CB40` (the per-name step): strips directory and extension, matches the table's names, creates the entry with `0x141E5E0A0` (a 0x18-byte entry, `+0x10 = 0xEA`), calls `0x141E5E210`, which calls the record's factory and then the object's slot +0x10 with `(obj, param_3, param_4)` (CONFIRMED decompile, `dec_cutlib2.txt`).
- The records' factories: 84 entries `0x141E6C240` (the 0x158-byte demo-area node, see below), 34 `0x141E5F430` (tutorialMobile), 21 `0x141E621B0`, 15 `0x141E7C240`, and one each for the rest (CONFIRMED by count).
- Demo-area node path (the case the test needs, index 0x71): factory `0x141E6C240` (CONFIRMED disasm): `call 0x1403FB410` (0x158 bytes, allocator native service), then `call 0x141E6C100` (the constructor; sets the final vtable `0x142B54228`). Live check: entry 0x71's `+8` node has vtable `0x142B54228`, `+8 = 16.0`, `+0xC = -80.0`, entry `+0x10 = 0x100000071` (CONFIRMED by live read at 02:49).
- Node parser `0x141E6C280` (CONFIRMED disasm): `rdx` = the object (the WESYS `.o` bytes); for each self-relative word it calls `0x141E823E0` (returns `rdx + ecx`, or 0 for a zero offset; CONFIRMED) and copies 4-byte fields into `rcx + 8 … +0x58` and on. The parse is a field copy with no PES-internal calls besides the resolver.
- PROOF (CONFIRMED by run): snapdiff --bounded, 1 SYNTHETIC call over seq321's memory (`synth.py`, static `as` spec, `hex` fields at `0x4A0000` (zeroed 0x158 node) and `0x4A1000` (the 432 B `demoarea_st001.o`)), 0 mismatches, every written byte, own-block 12/13. Closure: `0x141E823E0`. Record: `results_core.json` (`closure_demonode.json`, `results_snapdiff_demonode.json`).
- Boundary (for Main, approved in (a)): NATIVE = `pes::PesVfs::Read("common/match/constant/constant_stadium.bin")` → `UnwrapWesys` (`src/pes/inflate.hpp`) → the WESYS object table (`u32 count`, `u32 table` at +4; entries `(dataOff, size, nameOff)`, 12 B each; checked: 84 entries, all size 432) → a list of object pointers, which the caller passes in `rdx`. TRANSLATED above it: the name match `0x141E5CB40`, the entry `0x141E5E0A0/0x141E5E210`, the factory and constructor `0x141E6C240/0x141E6C100`, the parser `0x141E6C280` and the resolver `0x141E823E0`.
- BLOCKER (OPEN): the live node does not come from this object. The live entry 0x71 node has `+8 = 16.0`, `+0xC = -80.0`. The `.o` has `-80.0` at 0x30 and `16.0` at 0x34, and the loader reads `u32[b+u32[b]]`, `+4`, `+8`, … For every 4-byte word `b` of the object, `u32[b+u32[b]] == 0x41800000` and `u32[b+u32[b]+4] == 0xC2A00000` fails (CONFIRMED by script). A search of live memory for the object's first 64 bytes found nothing (`livefind.py`, CONFIRMED). So either PES's loaded copy differs from the mounted winner, or the entry does not parse from this object. The VFS is mounted with the download cpks (`pesfmt_dump mounts`): the winner is what we dumped; no `4cc` cpk lists `constant_stadium.bin` by a readable name (names are encrypted in the cpk tables), so I did not identify the overriding source.
- Not wired: GF has no caller of the fill. Wiring it would feed the else arm wrong data, so it waits on the item in section 6.1.

## 4. (c) The key writer of [T+0x1C]

### 4.1 Static (widened)
- Every `.pdata` function (158,597) with a load of a field at `+0x2C0` through any register AND a `+0x1C` write through any register: 7 (`wscan.out`): `0x140161CB0` (a frame-local `[rbp+0x2C0]` byte load; its `+0x1C` writes are frame stores), `0x1404812B0` (indexed store `[rbp+rsi*8+0x1C]`), `0x140791EC0` and `0x1420C9080` (byte stores), `0x14086D8E3` (qword store to `[rbp+0x1C]`), `0x1411014D3` and `0x142167E60` (rejected in CutsceneAnchor9). None is a dword store to `T+0x1C` with T loaded from `G+0x2C0` in the same function.
- Functions that store to `+0x2C0` through a register (`w3scan.out`): 191. The worldctor `0x140441490` is not among them (it calls a sub-constructor that does the store). The list is too broad to hook without a dynamic filter.
- INF: the writer is in a callee that receives T as an argument (the widened scan does not follow calls). Not verified.

### 4.2 Live (CONFIRMED, read-only)
- `G = [0x1436F3DC8] = 0x1EBCED20` (CONFIRMED live). `T = [G+0x2C0]` is NOT stable: it toggles between 0 and `0x0ABEF534` about 30 times per second. While set, `[T+0x1C] = 0x9E` and `[T+0x5C] = 1.0`.
- `sampler.py` (60 s, pid 678718, 15.8 M reads): 1778 set/zero cycles, and the key was `0x9E` every time it was set. No goal in the window, so no per-goal change was seen.
- The cut-library global `[0x1437F4808] = 0x1F33D4A0` (live, set), and entry `0x71` is live (section 3).
- INF: the key is per launch (the stadium), not per goal: launch pid678718 (0x9E = stadium 46) vs launch pid2024691 (0x71 = stadium 1, CutsceneAnchor7's anchor). Not verified against the stadium of each launch.

### 4.3 What would settle it
- A goal in a live match with the sampler running (MI non-null, `G+0x270`). If the key stays 0x9E through a goal, the key is per launch. If it changes, the writer is the function that runs at that time.
- The stadium id of pid678718 and pid2024691 (the home team's `Team.bin` stadium, low u16 at +0x3C; GF has `TeamStadiumId`). If they are 46 and 1, key = 112 + stadium id is confirmed for the two launches.

### 4.4 Hook (Main's request)
- No hook line supplied: I have no candidate writer that the evidence supports. Arming one of the 191 `+0x2C0` writers blind would park a hot path. Proposal for the next relaunch, in one batch: (1) a sampler run across a goal; (2) if the key moves, a hook on the `+0x2C0` writers that run within 1 s before the change (I can derive the list from the sampler's timestamps and the hook's own log). I can produce the exact line with `untranslated_callees.py hooks_for_image` once a candidate is named.

## 5. Checks (CONFIRMED)

- `gen_translated.py --image PES --check`: rc 0, 5760 functions (3766 recorded bit-exact, 1994 closure), byte-identical.
- `tools/dev/ledger.py` and `--check`: rc 0.
- `cmake --build build --target pestranslated`: built (the two new TUs compile).
- Tests (`env -u GF_DEBUG_RECEIVE -u GF_DEBUG_ANKLE -u GF_PES_SURVEY`, `PES21_ROOT`, `PES_EXE`, `GF_DIFF_PYTHON` set): `pes_fixdemo_select_test` 7/7 (incl. `TheCutLibraryHoldsOneEmptySlotPerCutName`), `pes_registry_test` 7/7, `pes_goal_anchor_test --gtest_filter=-GoalAnchorKindTwo.WideChannel*` 22/22, `pes_goal_anchor_test --gtest_filter=GoalAnchorKindTwo.WideChannel*` RED (9 failures, section 0).
- No GF code changed this round: no mutation run. Gate not run (Main gates).
- Build dir removed; unpacked seq321 memory removed (the packed original stays in `live2/pid678718/seq321`).

## 6. Open items (numbered; the next blocker for each)

1. Identify the live copy of `constant_stadium.bin` / demoarea object (blocks the fill, the T.x check and any wiring). Method: live pread over PES's heap for the WESYS header and the object table (`u32 count=84`, `u32 table=8`) and for the demoarea node's 0x158 bytes at `0x32894AC0` (entry 0x71's node); decode its source offsets; compare with the winner file. If PES reads another winner, the VFS mount order must follow the live one.
2. Decode the record factories for the other 233 names (84 `0x141E6C240`, 34 `0x141E5F430`, 21 `0x141E621B0`, 15 `0x141E7C240`, and singles). The else arm needs only entry 0x71 for the test, but the fill is the library.
3. Decode the load method of vtable `0x142B502C8` slot +0x10 (`0x141E64A10` → `0x15B35E0C0`), needed for the 0x30 objects (the animeAging/ball group).
4. Translate the name match `0x141E5CB40` and the entry init `0x141E5E0A0/0x141E5E210` under bounded proofs, with the list object from the caller (the caller of the fill is not found yet: vtable `0x142B48C88` has no `lea` or qword reference in the image, so its installer is OPEN).
5. The key writer (section 4.3): a goal sample, then a candidate hook (4.4).
6. After 1-5: wire the fill as a native read plus translated parse (the boundary in section 3); re-run the wide test; then T.x = -41.6211472 at the event tick (INF, not verified).
7. Correction to earlier rounds: CutsceneAnchor6-8 called the 234 entries "cut records"; they are match-constant nodes. The else arm's read (`0x1404B6400` → `0x141E5CA20` → `entry+8`) is the same; only the meaning of the key changes.
