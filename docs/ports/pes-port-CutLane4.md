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
