# CutLane4: G+0x1978 table, writer decoded, content not yet located; wide test still RED

Branch CutLane, worktree <workdrive>/gf-CutLane. Labels: CONFIRMED = read or run this session; INF = inference; OPEN = blocker.
Scratch: <workdrive>/gf-CutLane-scratch (dec_w490.txt, dec_arr.txt, arr203.hex, arr204.hex, findtab.py, scanvfs.py, vt.py).

## 0. Landed

- `82ad8564` (origin/Integrate) fast-forwarded into CutLane and pushed. `git ls-remote origin CutLane` = 82ad8564 (CONFIRMED).
  No code change on top of it. Git needs `-c submodule.recurse=false` in this worktree (the godot-cpp symlink makes fetch and
  status fail otherwise).
- gen_translated.py --check: started, in progress when this report was written (see section 7).

## 1. Tests (no code change; baseline on 82ad8564, CONFIRMED)

- pes_goal_anchor_test: 25 pass, 1 red. The red is GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes at
  tests/pes_goal_anchor_test.cpp:434 (armed sceneId: GF 1, PES 196825 = 0x300D9) and :438 (rot/pos: GF -57.5796, PES -41.5502167;
  z GF 0, PES 34). The same for DC 5, 100, 161.
- pes_fixdemo_select_test 8/8, pes_fixdemo_table_test 7/7.

## 2. The G+0x1978 writer (CONFIRMED static, dec_w490.txt)

- Function 0x140A490B0 (world load), full decompile in dec_w490.txt. Guard: `if (*(G+0x2E50) == 0)` at its start; it sets
  G+0x2E50 = 1 at its end.
- Its loops process named resources: each entry is a 0x28-byte block at G+0x1918 (entries at 0x1918 and 0x1940), with:
  - a name object inline at entry-8 (vtable 0x142596CD8) and a second name object at entry+0x48 (vtable 0x1425968E8);
  - a handle at entry+8 (factory 0x140A44600 for the first, 0x140A44150 / 0x140A444B0 for other blocks) and a handle at
    entry+0x58 (factory 0x140A44360, the refcounted 0x20-byte array, vtable 0x142596848);
  - the handle is filled by 0x141B55730 (the resource lookup), and `if (!entry[+0x60]) entry[+0x60] = [[entry+0x58]+0x10] + 8`.
  So G+0x1978 = data of the array handle + 8 (for the 0x1918 entry). The brief's writer claim is CONFIRMED in this form.
- 0x141B55730 (CONFIRMED decompile): `svc = [*(GameImpl)+0x38]()` (GameImpl = [0x1437F0AC8]), `h = svc.vt+0x70(svc, key1, key2)`.
  On a miss it calls the factory, stores the handle at param_3, then `svc.vt+0x68(svc, key1, key2, handle)` (insert). On a hit it
  assigns the found handle (param_7). GF's PesCameraRegistry models exactly these natives (src/pes/translated/pes_native.hpp).
- Keys (CONFIRMED by decode): the name getters are 0x14044E0C0 (vt slot +0x10: `lea rax,[0x142596900]`, returns the string "INFO")
  and 0x140421F40 (vt slot +8: `xor eax,eax; ret`, returns NULL). Both vtables (0x142596CD8, 0x1425968E8) share these slots, so
  every block's key is ("" , "INFO").
- CONTRADICTION (OPEN): with identical keys the second lookup should hit the first handle. The capture shows two distinct objects:
  seq203 G+0x1920 = 0x2DD41DD0 (vt 0x142596C38, factory 0x140A44600) and G+0x1970 = 0x2DD5A0A0 (vt 0x142596848, factory 0x140A44360).
  Either PES's find keys on more than the two strings (the type object or factory), or a getter reads something I did not decode.
  Not resolved; the service's vtable 0x70 / 0x68 bodies are not decoded.
- Callers (CONFIRMED scans over pes_full.bin): no E8 or E9 rel32 to 0x140A490B0, no rip-relative lea to it, no stored qword
  0x140A490B0. The loader is reached indirectly (not found yet).

## 3. The array's content (CONFIRMED from captures, and what is not known)

- seq203 (pid2024691): G = 0x2DB0C420. G+0x1970 object: vt 0x142596848, refcount 0x0C at +8, data 0x2DD5A0D0 at +0x10.
  G+0x1978 = 0x2DD5A0D8. seq204 holds the same 0x200 bytes at 0x2DD5A0D0 (cmp equal; arr203.hex == arr204.hex).
- Data at 0x2DD5A0D0: u32 1, 0, then 0, 0, then u32 6, 0xB, 4, 3, 2, 5, 7, 8, 9, 0xA, 0xF, 0x10, 0xD, 0xE, 1, 0x13, 0x11, 0x12,
  0x16, 0x14, 0x15, 0xC, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C. From +0x10 on it is a permutation of 1..0x1C (28 values).
  (INF: 28 matches the camera registry's kTargets = 28; not established.)
- The factory (0x140A44360, CONFIRMED decompile) makes the 0x20-byte object (refcount 1) and calls FUN_14041D4F0(obj, 2, 1): that
  allocates a 0x5E08-byte buffer with u32 1 at its start and two 0xBC0-stride sub-objects (FUN_140A3AAE0). The permutation is NOT
  produced by that constructor. The fill that writes it is OPEN.
- Install search (CONFIRMED tools run): raw bytes of the first 8 values, as u32, u16 and u8, found nothing in pes_full.bin, and
  nothing in the raw files under <workdrive>/PES21 (findtab.py). cpk entries are compressed, so raw search cannot see them.
- VFS listing (pesfmt_dump ls, 121,075 paths): no file named INFO and no file named *info* that could be a resource key.
  The VFS scan of the 3,468 .bin files (scanvfs.py, via pesfmt_dump cat) is recorded in section 7.

## 4. Correction to the brief

- 0x140A489D0 (CONFIRMED, dec_w490.txt lines 474-483): `if (idx < 2) return *(G + (idx + 0xA3) * 0x28); return *(G + 0x1978);`.
  G+0x1978 is read only for idx >= 2, and it returns the array pointer's value. The brief's "for index < 2 returns *(G+0x1978+...)" is wrong.

## 5. Experiment (run, reverted; not committed)

- In BuildWorld, G+0x1978 = (seq203's 0x200 array bytes copied into a Zalloc'd buffer) + 8. Wide test: still RED with the same
  numbers (scene 1; target -57.5796, PES -41.5502). So the table content alone does not move the wide result. The event path needs
  the other inputs too: the tick 0x14047B460's arg5/arg6 (queue item; GF passes a0 and a Zalloc as recompute r8/r9).
- Scratch edits reverted with `git checkout -- src/pes/fixdemo_select.cpp`; `git status` clean on the branch.

## 6. Not done (named)

1. Port 0x140A490B0 (world resource load): blocked by the OPEN key contradiction (section 2) and the untraced fill (section 3).
   No port, no GF stand-in removed. The ponytail manual run+0x18 write stays.
2. Wide test: needs the tick 0x14047B460 translated and called with PES's arg5/arg6 (queue item), plus the table (section 5).
3. run+0x18 writer: not located this session.

## 7. Next exact steps

1. Decode the service vtable +0x70 (find) and +0x68 (insert) bodies (GameImpl +0x38 -> service; vt of the service object) to
   settle the key contradiction in section 2.
2. Find the caller chain of 0x140A490B0 through its neighbours (indirect: vtable or a table of functions); the scans found none direct.
3. Find the fill of the 0x5E08-byte buffer (FUN_14041D4F0 path, FUN_140A3AAE0), which writes the permutation at data+0x10.
4. Then the tick 0x14047B460 (queue item) and the wide test.

Results: gen_translated.py --check rc=0, 10,982 functions byte-identical (gen_check6.log). Full VFS scan of 3,468 .bin files
outside Asset/ (scanvfs2.out): 0 hits for the u32, u16 or u8 pattern of the first 8 values, so the table is not a raw .bin under
common/. Not searched: Asset/ and the other extensions (.fdc, .xml, .fpk, .ask, .gani).

# Round 2 (answer to Main's queue item: translate and prove the tick 0x14047B460, call it per tick)

Outcome: the tick is NOT translated into the layer, NOT wired into GF, and the wide test is still red. Nothing landed (no commit on top of 82ad8564). Blocker named in section 2.6 (the harness cannot replay the tick's gfx path).

## R2.1 The tick's call site and its six arguments (CONFIRMED: disassembly of the image, call at 0x140446D2D)

The DemoInfo handler 0x140446B20 (kind 4) calls the tick at 0x140446D2D after the gate 0x140472640 (0x140446D03):
- rcx = D+0x1E8 (A0), D = rdi (the handler's rcx at entry, the DemoInfo).
- rdx = **(D+0x17860)  (ctx: the list object's first qword).
- r8  = *(D+0x17860)   (the list pointer; the tick's param_3, read as puVar4[0], puVar4[1] = begin/end).
- r9  = D+0x2790       (the tick's param_4 object).
- arg5 [rsp+0x28 at entry] = D (stored at 0x140446D25 as `mov [rsp+0x20], rdi`; 5th argument).
- arg6 [rsp+0x30 at entry] = rbp, the handler's rdx at its entry (0x140446B45 `mov rbp, rdx`; stored at 0x140446D19).
- The handler's rdx comes from its dispatcher 0x1404463B0 at 0x14044640B: `mov rdx, [rsp+0x60]` in the dispatcher's frame (its caller's stack argument). Not decoded (OPEN): who calls 0x1404463B0 and what it passes (its callers: not found by a rel32 scan; likely an indirect dispatch).
- Recompute call inside the tick: 0x14047B851 region passes (A0, ctx, D, arg6) (CONFIRMED in dec_7b460.txt line 162). So the recompute's r8 IS the DemoInfo D, and its r9 IS arg6.
- seq203 (pid2024691, recompute entry) values, CONFIRMED: rcx 0x31C53DB8, rdx 0x31C50798, r8 0x31C53BD0 (= D; check rcx = D+0x1E8 holds), r9 0x10E1F08C (arg6), rsp 0x1C4FF5D8. The list pointer *(D+0x17860) = 0x1C4FFC60 is a STACK address (the handler's frame, 0x688 above rsp), not heap: the list object is a stack-local of the step machine. GF has no such frame.

## R2.2 The recompute's r8 is an out-object, not A0 (CONFIRMED by decompile and one experiment)

- Recompute 0x140471A80 writes `*r8 = scene` (line 178 of dec_7b460.txt region: `*param_3 = uVar18`), the list at r8+0xC (`param_3[3]`, entries at param_3[uVar9*7+4]), r8+0x78 pointers, and calls the event builder `FUN_14047c500(param_3, ...)` (line 319) with r8 as its first argument. So r8 is the event/out record, and it is D in PES.
- Experiment (scratch, reverted): GF passes r8 = a0 today (the ponytail in ArrangeGoal says so). Replacing it with a separate Zalloc(0x1000): the armed scene goes 1 -> 0 (GF pick cleared; target -57.5796 unchanged; still red at 434 and 438). So the current scene 1 is an aliasing artefact of r8 = A0, not a pick. The stand-in must not stay as is; PES's real D is needed.

## R2.3 The table experiment (scratch, as Main asked): no change, kept out of the branch

- seq203's 0x200 array bytes copied into a Zalloc'd buffer at G+0x1978 (+8): wide test unchanged (scene 1, target -57.58). The table alone does not reach the pick (R1 section 5).

## R2.4 Closure of the tick (CONFIRMED by closure.py run; reverted, nothing committed)

- `closure.py --spec-set core --tag tick 0x14047B460`: 375 members (1 seed). Written: 375 specs, of which 367 committed specs were kept (121 of them differ from the generator's output; the committed ones are kept without --regenerate) and 8 new specs in specs/core. The new set file, the universe.json rows and the 8 specs were reverted (they are reproducible with the command above, and only a proof-backed translation may land).
- Members include the recompute (translated), the event builder 0x14047C500, the judge/sanction chain (0x14050AB20, 0x14050DE50, 0x14050AE30, 0x14050B2B0, 0x14050C3C0, 0x14050A8A0), the command object 0x1404511E0, the squad table 0x140A62EA0, and the callee 0x141B55830 whose vtable +0x88 is a deferred-job dispatch (indirect; the harness must model it).
- Not translated: the tick's layer regeneration (gen_translated --check 10,982 byte-identical before this run; a new closure member set would change it).

## R2.5 Synthetic-call proof of the tick (attempted; blocked)

- Built `tick_calls.py` (scratch, not committed): one synthetic call of 0x14047B460 over seq203's full snapshot (rcx = D+0x1E8, rdx = ctx, r8 = *(D+0x17860), r9 = D+0x2790, [rsp+0x28] = D, [rsp+0x30] = arg6, [rsp] = 0x140446D32, the call record's ret matching [rsp], CONFIRMED: the first run failed on the ret mismatch, fixed). The replay ran PES's bytes for 50,000,000 instructions and stopped in a loop.
- The loop is the graphics allocator: the stack at the stop holds 0x14195BD65 (after `call 0x141957C50`, the gfx device allocator thunk, stubs.json "service_gfx_device_alloc": rax = 0) inside the command-string builder 0x14195BD10, reached from the recompute's own callees (0x1404720B0 -> 0x140A62ED5 -> 0x141F5E3AB -> 0x1403FBCDC -> 0x141F55D12 -> 0x141F58F65 -> 0x1403FB504). CONFIRMED by stack peek.
- Why the stub fails: the snapshot holds a live device object (DAT_1437C9238 = 0x1505BA0, CONFIRMED) whose vtable (0x7FF5FFEF0010) has +0x20 = 0 in the capture (the device DLL's code is not in the snapshot). The clean-boot stub says rax = 0, the captured state is a live device, so the replay enters a path PES did not take in this model. The real allocator result is not in any capture (OPEN).

## R2.6 Rig request (what would settle the tick proof; Main to decide)

Hook: the allocator's result for the recompute's calls. Ask for:
- hook 0x14195BD10 (the command-string builder's entry), `--when` the caller is inside the recompute body: `ret >= 0x140471A80 and ret < 0x1404722F4`; cap 6; FULL; plus
- hook 0x140446B20 (the kind-4 handler entry), cap 6; FULL, so the handler's dispatcher argument (arg6) and the tick's real entry state are on disk.
What the capture must show: the rax that 0x141957C50 returned to 0x14195BD65 in PES (captured as the builder's stack slot [rsp+0x50] after the call, read from the next entry's memory), and the DemoInfo tick entry state with A0+0xEA = 1. Until one of these exists, the harness cannot replay the tick past its gfx allocation (INF: the same result for every call of the tick's recompute path).

## R2.7 What GF did this round

- No code change. `pes_goal_anchor_test` GoalAnchorKindTwo: wide test still red (scene 1, target -57.58; PES 0x300D9, -41.55, z 34). Other tests were not re-run this round (no code change since round 1's green: pes_goal_anchor 25/26, fixdemo_select 8/8, fixdemo_table 7/7).
- The ponytail stand-in (r8 = a0, r9 = Zalloc) stays; the manual run+0x18 write stays. Both are named in code.

## R2.8 Next exact steps

1. Rig request R2.6 (or Main decides the gfx allocator model: the return value of 0x141957C50 in PES).
2. Then: translate the tick (closure above, 375 members; regenerate the layer), replay the tick over seq203 with the allocator result from the capture, and assert the tick's arg5/arg6 path.
3. Only after the tick proves: GF calls it with D (a DemoInfo object of 0x17860 + list bytes) instead of the r8 = a0 stand-in; then measure the wide test.
4. The handler's dispatcher 0x1404463B0 caller (arg6 source) is still OPEN.

Scratch (not committed): <workdrive>/gf-CutLane-scratch/tick/ (replay snapshot, 3.9 GB unpacked mem.bin: delete when done), tick_calls.py, stackpeek.py. seq203 is unpacked in gf-Rendezvous-scratch (3.9 GB) and was packed again (capz pack, CONFIRMED).

Verdict (INF): the tick is translatable (closure is 375 members, all in the translator's universe), but its proof and its GF wiring both need the gfx allocator's real result and the deferred-job service, which GF has no model for. The wide test cannot move until the tick's arg5 (D) is an object GF builds from the same fields.
