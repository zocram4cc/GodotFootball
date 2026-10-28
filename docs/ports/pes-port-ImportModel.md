# PES port: ImportModel: last three referee/restart hooks (28-10-26)

Branch `ImportModel` (worktree `<workdrive>/gf-ImportModel`), pushed head `2eccea24` (`git ls-remote origin ImportModel` = `2eccea24`). Commits: `059a453c` (merge `origin/RvReplay`, no conflicts; carries RefReplay3's closure-stub fix, sweep tooling and `slim.py`), `b21098fe` (red check), `2eccea24` (budget fix). Gate not run (Main gates). Scratch, PES-derived, not committed: `<workdrive>/gf-ImportModel-scratch/` (sweep in `sweep2/`). Labels: CONFIRMED = read in tool output this session; INF = inferred, reason given; OPEN = not settled.

## 0. Summary

- **Verdicts (sweep over 66 surviving captures, 132 snapshot pairs):** 445E90 PROVEN (3/3 calls). 480910 NOT PROVEN. 44C880 NOT PROVEN. The other seven hooks stay PROVEN (table §4).
- **445E90 (CONFIRMED):** the refusal of the 9-byte leaf `0x140440FA0` was a budget artifact, not a property of its bytes. Fixed generally (§3).
- **480910 (CONFIRMED):** 2 aborts at `PES_DISPATCH_UNTRANSLATED: 0x6FFFFEC77E00`. The thunk is a `ucrtbase.dll` export whose name I could not recover (§2). A second change is a **regression I introduced**: 3 captures that were bit-exact are now excluded (§3.3). Bit-exact count went 7 to 5.
- **44C880 (CONFIRMED):** 8 of 9 calls bit-exact. The 9th enters `sqrt` thunk `0x1415A1780`, which is not in the spec's `stub_refs` (§3.4).
- **Checks green:** `driver.py --check` rc 0 (34 of 34 bit-exact); `gen_translated.py --check` rc 0 (3951 functions byte-identical).
- **8 captures are gone:** `live2/seq008, 009, 012, 013, 014, 015, 016, 017` have no directory any more (CONFIRMED). They are not replayable and were dropped from the sweep (`captures_missing.txt`).

## 1. Setup

- Worktree `-b ImportModel` from `origin/main` (`f7dbd120`). godot-cpp symlinked (CONFIRMED).
- The RefReplay3 fixes are not on `origin/main`. They are on `origin/RvReplay` (`a9946f5d`). I merged `origin/RvReplay` into `ImportModel` (`059a453c`). Main should merge `origin/main` into this branch after batch AL2 lands (AL2 is gating now).
- Slip (disclosed): the first `write` of a check file landed in the session cwd checkout `<local>/Code/GameplayFootball`. I moved it into the worktree at once. No other file was written there.

## 2. Item 2: import thunk `0x6FFFFEC77E00` (480910)

What is confirmed:
- The slot that holds it is `0x142530660` (CONFIRMED). The snapshot holds `0x6FFFFEC77E00` there in `live/seq085` and `live/seq089`. The unpacked image calls through that slot from many sites (CONFIRMED, universe closure of 480910 has 5590 functions, and the slot appears in about 40 of them). `import_slots.json` marks it `unmapped` (CONFIRMED).
- The thunk lies in module base `0x6FFFFEC60000` (CONFIRMED). That is an MZ image with ImageBase equal to its load address, `SizeOfImage 0xF4000`, 6 sections, and an export directory named `ucrtbase.dll` with 2480 exports. RVA `0x17E00` is export function index 2383, which is ordinal 2384 (CONFIRMED by export-directory walk).
- The export **name** cannot be read. The name strings are zero in the slim captures (the replay never read those pages). `live2/seq088` has an unreadable export directory at this base (CONFIRMED).
- No on-disk `ucrtbase.dll` matches. I checked 9 builds: the Wine prefix copy (879134 B, `SizeOfImage 0x44A000`, 2487 functions), the Wine builtin (same file), and Proton/GE builds (`0xD1000` to `0xFC000`). None has `0xF4000` (CONFIRMED).
- The on-disk `PES2021.exe` import directory has no `ucrtbase` import at all. The packer resolves this slot at run time (CONFIRMED).
- Thunk bytes (CONFIRMED, capstone on the snapshot): `push rbx; sub rsp,0x20; xor ebx,ebx; test rcx,rcx; je; test rdx,rdx; je; test r8,r8; jne 0x6FFFFEC77E32; mov [rcx],bl; call 0x6FFFFEC6A550; mov ebx,0x16; mov [rax],ebx; call 0x6FFFFECC84C0; mov eax,ebx; ...; ret`. This is a copy-with-validation routine (INF: `strcpy_s`-shaped). The null-source path writes `*rcx = 0` and `errno = EINVAL`. The `r8 != 0` path at `0x6FFFFEC77E32` does the real work.

Blocker (exact): a named model needs the export name, and the name is not recoverable from the captures or from disk. A correct model also needs the `r8 != 0` path, plus the two DLL-internal callees `0x6FFFFEC6A550` (errno getter) and `0x6FFFFECC84C0`. The harness refuses DLL code by design (`frame_trace.py` `LazyMap`). I did **not** add a model: a model keyed by a guessed name would be a guess.

Stale text (CONFIRMED): the RefReplay3 claim "import slot `14252F6E8` has no name" is out of date. `import_slots.json` names it `ntdll.dll!RtlLeaveCriticalSection`. `14252FD70` is `msvcrt.dll!memcpy`. Blocker strings in the 44C880 spec and in `stubs.json` still carry the old text.

## 3. Item 3 and the cap fix

### 3.1 The 445E90 leaf (CONFIRMED)

- `runnable_extra` used one budget (`EXTRA_CAP = 1100`) over every seed's walk. When 445E90's ~20 seeds used it up, every seed still on the stack was marked `closure larger than 1100`, including the leaf `0x140440FA0`. The leaf's bytes are `test edx,edx; js; cmp edx,0x16; jae; test r8b,r8b; movsxd; setne; mov [rax+rcx+0x2F56],r8b; ret` (no direct call).
- Uncapped measurements (CONFIRMED): 445E90's walk is 1871 functions at depth 25. 44C880's walk is 1865 functions at depth 80. At depth 25, 33 functions of 44C880's walk are blocked with `call depth > 25`.

### 3.2 Fix (generic, `2eccea24`)

- `EXTRA_CAP` 1100 to 2000 (ponytail comment: one budget; the ceiling is that a walk over it blocks every unseen seed; upgrade is a per-seed budget). `depth_cap` default 25 to 80.
- Red first (`b21098fe`): `check_runnable_extra.py` on HEAD gives `RED: 0x140440FA0 blocked: closure larger than 1100`, rc 1.
- Green after (`2eccea24`): `GREEN: 0x140440FA0 runnable (1906 runnable, 4 blocked for 0x140445e90)`, rc 0. The 4 blocked are the `sqrt` and cookie stubs, the `powf` thunk, and `140A79640`.

### 3.3 Regression in 480910 (CONFIRMED, caused by 3.2)

- Under the 1100 budget, 480910 had 7 bit-exact calls: `live_seq083, 087, 095, 097, live2_seq088, 092, 094`. Its 3 aborts were `live_seq085, 089, live2_seq090` (RefReplay3 table).
- Now: `live2_seq088, 092, 094` are **excluded** (replayed 0), with reasons `unmapped read at 0x0` (2) and `fetch from unmapped 0x0` (1). The run-real callee that the wider walk now includes reads address 0. `live2_seq090` is now bit-exact. The two aborts (`live_seq085, 089`) remain.
- Net: 5 bit-exact, 2 mismatched, 3 excluded. This is an exclusion, not a false pass. Root cause (which callee, and why it reads 0 in the replay) is OPEN. Main must decide whether to keep the budget at 2000 (needed for 445E90) and handle 480910 separately.

### 3.4 Restart_44C880 callees

- `1404488E0`: six-case switch, table `0x1404489AC` (RefReplay3 decode). Its spec text says "switch case bodies not enumerated (closure incomplete)". No captured call of 44C880 is excluded for it. Its case bodies make vtable calls (`[rax+0x168]`, `[r8+0x28]`, `[rax+0x160]` at `0x1404489D0`). Whether the switch blocks any replay is OPEN.
- `140449B00`: its indirect calls are vtable dispatch (for example `141B4B596 [rax+0x20]`, `1403FB501 [rax+8]`). They are not blocked by `runnable_extra`. INF: their targets come from the trace-derived dispatch table (`pes_dispatch_gen.hpp` of 44C880 lists 2268 targets). Per-site coverage is not recorded (OPEN).
- `140A79640` (stub-table entry, `kind unmodellable`): its closure calls through a function pointer held on the stack (`CALL qword ptr [RSP+0x98]` at `141E535F9` and `141E5362F`, in `141E53540`). It also has a call-depth-10 path and global stores. Its import slot is now named (see §2). The stub's `why` text is stale. It is blocked but **not entered** by any captured 44C880 or 445E90 call (no exclusion recorded, INF). To model it one needs the call-time value of `[RSP+0x98]`, read from the captures, and a model for `RtlLeaveCriticalSection` (OPEN).
- `sqrt` thunk `0x1415A1780` (`jmp [0x1425301F0]`, slot `unmapped`): the only 44C880 exclusion (`unmodellable callee 0x1415a1780 entered`). `reach_stubs(0x14044C880)` returns no stubs, although 215 closure functions call `0x1415A1780` or the cookie `0x14159F3E0` directly (CONFIRMED). Caps are not the cause: the largest of those bodies has 405 instructions, and `body()` caps at 6000 (INF). The walk drops them somewhere (OPEN).

### 3.5 Restart_driver_445E90

- Now PROVEN: 3 of 3 calls (live `seq031, 033, 035`) bit-exact, none excluded. Before the fix it had 0 of 4 (the fourth was `live2_seq008`, now gone).

## 4. Item 4: verdict table (66 captures, 132 snapshot pairs; `sweep2/sweep.json`)

| hook | VA | calls | replayed | bit-exact | excluded | mismatched | verdict |
|---|---|---|---|---|---|---|---|
| foul_sanction_47F170 | 0x14047F170 | 20 | 20 | 20 | 0 | 0 | PROVEN |
| foul_sanction_caller_480910 | 0x140480910 | 10 | 7 | 5 | 3 | 2 | NOT PROVEN (§2, §3.3) |
| ref_involve_A3A690 | 0x140A3A690 | 22 | 22 | 22 | 0 | 0 | PROVEN |
| ref_judge_A3A930 | 0x140A3A930 | 12 | 12 | 12 | 0 | 0 | PROVEN |
| ref_latch_A3A870 | 0x140A3A870 | 6 | 6 | 6 | 0 | 0 | PROVEN |
| ref_restart_cmd_A39A80 | 0x140A39A80 | 6 | 6 | 6 | 0 | 0 | PROVEN |
| linesman_lookat_A38F20 | 0x140A38F20 | 6 | 6 | 6 | 0 | 0 | PROVEN |
| restart_446390 | 0x140446390 | 7 | 7 | 7 | 0 | 0 | PROVEN |
| restart_44C880 | 0x14044C880 | 9 | 8 | 8 | 1 | 0 | NOT PROVEN (§3.4, sqrt stub) |
| restart_driver_445E90 | 0x140445E90 | 3 | 3 | 3 | 0 | 0 | PROVEN |

Counts are per call. The 8 removed captures are the `live2` dirs listed in §0. No slim-page read was reported in any hook's notes (CONFIRMED on the notes I read).

## 5. Item 5: checks and tests

- `driver.py --check --spec-dir specs/impure --seed-bank banks_union9l`: rc 0, "all 34 recorded-diffed functions still diff bit-exact" (CONFIRMED, `driver_check_final.log`).
- `gen_translated.py --image PES --check`: rc 0, "3951 functions (3672 recorded bit-exact, 279 closure) regenerate byte-identical" (CONFIRMED, `gen_check_final.log`).
- `check_runnable_extra.py`: red on `b21098fe`'s parent (HEAD before the fix), rc 1; green on `2eccea24`, rc 0.
- Mutation: `EXTRA_CAP = 1100` in the current tree reproduces the red (the HEAD run). Depth-25 mutation not run separately.
- `check_closure_stubs.py` not re-run: `reach_stubs` and `harness_fields` are unchanged in this branch.

## 6. Open items (numbered)

1. **480910 regression** (§3.3): `live2_seq088, 092, 094` now excluded by a run-real callee that reads address 0. Find the callee and why it reads 0. Main to decide the budget trade-off.
2. **ucrtbase thunk `0x6FFFFEC77E00`** (§2): export name unknown. Needs a full capture with readable export names (or the loaded `ucrtbase.dll` build), then a model for the `r8 != 0` path and the two internal callees.
3. **`reach_stubs` gap for 44C880** (§3.4): `sqrt` (`0x1415A1780`) and cookie (`0x14159F3E0`) are not reached. Fix the walk, then re-run 44C880.
4. **`140A79640`**: needs the call-time function pointer at `[RSP+0x98]` and a `RtlLeaveCriticalSection` model. Its stub `why` text is stale.
5. **`1404488E0` switch**: enumerate case bodies, or confirm no replay reaches them. Per-site indirect coverage is not recorded.
6. **Stale blocker texts** in specs and `stubs.json` (`14252F6E8`, `14252FD70` are named now).
7. **Main**: merge `origin/main` after AL2 lands; slim second pass on `live2` (RefReplay3 item 4).
8. **Ghidra bridge down**: used capstone on `pes_full.bin` and the snapshot. Use `dec.sh` for decompiles (Main's message).


## Main's note (28-10-26)

The ucrtbase thunk's name: PES runs under Wine with the prefix's own `drive_c/windows/system32/ucrtbase.dll`, whose SizeOfImage is `0xF4000` (the size the captures show). Its export index 2383 (ordinal 2384, base 1) is **`strcpy_s`** (read from that DLL's export table). Wine's builtin `ucrtbase.dll` (SizeOfImage `0x44A000`) is not the one loaded.
