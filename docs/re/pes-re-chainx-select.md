# PES chain X (second anime chain): static decode + live memory check (ChainXSelect, 17-11)

Scope: the flat image `pes_full.bin` (file offset = VA − 0x140000000), the stock install `<workdrive>/PES21/Data/dt13_all.cpk` (read-only), and three packed captures of pid3778393 (seq061, seq1128, seq1171; read by streaming `mem.bin.zst`). Nothing PES-derived is committed. Scratch: `<workdrive>/gf-ChainX-scratch` (`chainx_check.py`, `callers.py`, `capread.py`, `eyes/`).

Method: every instruction VA was checked by `chainx_check.py`: a linear decode from the `.pdata` begin of its containing function must reach it as an instruction boundary (`.pdata` at file offset 0x3AC5000, flat). Leaves with no `.pdata` entry are checked by a linear decode from the call target. Self-check: 46 claims + 9 vtable checks pass (55/55). Mutation: one planted wrong VA (0x1406F1148, one byte past a call) FAILs and the run exits 1. Callers were found by a byte scan of E8/E9 targets, each site verified as an instruction start. The control VA 0x14256AE1C holds 1.0f.

## Verdict
- X is built and armed in the captured live match. Singleton flag `0x1437F4CB0` = 1; X = `*S` is non-null; X's vtable is `0x142ABA840`; the gate int `S+0x14` = `0x100` in all three re-read ticks (seq061, seq1128, seq1171; one model, P = 0x3AF71500). The doc's 19 captures also show 0x100 (not re-read here). X's per-tick step is therefore not running in any observed tick.
- X is selected only by a SetMotion call (`FUN_1406F0F70`) with key k ≤ 0x1124 whose table word has bit 8 clear. The stock table has 234 such keys (§2).
- The 13 constant-key SetMotion call sites (12 distinct keys) all map to the sentinel 0x100. They never select X.
- Whether a normal match requests one of the 234 keys is OPEN. 143 call sites take a runtime key (117 register copies, 26 keys set in a predecessor block, 1 unresolved), and no live key has been captured.
- Cheapest live check: one hook line in §5.

## 1. Holder, table, loader (CONFIRMED unless marked)
- `FUN_1406B4EA0` (pdata 0x1406B4EA0–0x1406B4EF5): getter. Returns `rax = DAT_1436F2648`; allocates a 0x10-byte holder on first call (`LEA EDX,[RAX+0x10]` at 0x1406B4EBB). Holder layout: +0 byte loaded flag, +8 qword table pointer.
- Table: `FUN_1406B4D40` (pdata 0x1406B4D40–0x1406B4DC6) allocates `0x4494` bytes = 0x1125 dwords (`mov edx,0x4494` at 0x1406B4D68), clears the low 9 bits of each dword (loop at 0x1406B4D90), calls the fill stub at 0x1406B4DAE, then sets holder+0 = 1 (0x1406B4DB8). Index of key k = `dword[k] & 0x1FF` (`FUN_1406B4E70`, leaf, `cmp edx,0x1125` at 0x1406B4E70, `and eax,0x1ff` before its ret); sentinel 0x100 when k ≥ 0x1125, table null, or the entry's low 9 bits ≥ 0x100 (return at 0x1406B4E8D).
- Who calls it: `FUN_1406B4C80` (pdata 0x1406B4C80–0x1406B4D3E) at 0x1406B4D12. That is the virtual load slot of the EyesTarget object: vtable `0x1425DCE48` slot 0 = `0x1406B4C40`, slot 1 (+8) = `0x1406B4C80`. The object (0x98 bytes) is created by `FUN_1406B4F00` (called once from `FUN_140419610` at 0x1404196D5), which sets `DAT_1436F2650 = 0`. `FUN_1406B4FB0` (unload, reached through a thunk from `FUN_140419C00`) sets `DAT_1436F2650 = 1`, frees the table and clears holder+0. The load is gated on `DAT_1436F2650 == 0`.
- Fill stub: `FUN_1415A1708` (no `.pdata`) is `jmp qword ptr [rip+0xf8e662]`, i.e. through `.rdata` slot 0x14252FD70 (target computed = 0x14252FD70). The file's slot value 0x6FFFFEE67700 is a stale unpacker value. In the capture the slot holds 0x6FE9E5E67700, inside `msvcrt.dll` (a deferred mapping; its bytes are not in mem.bin). So the fill is a copy routine from msvcrt (identity inferred from the byte equality below, not from its code). Not an integrity or licence check.
- Source file: cpk name `cpk_dat/common/anime/EyesTarget/bin/Eyestarget.bin` (string at 0x1425DCC48). VFS path `common/anime/EyesTarget/bin/Eyestarget.bin` (GF `pestrapdata.hpp:28`). Stock archive: `dt13_all.cpk`, entry size 17556 = 0x4494. No other cpk under `PES21/download` or `PES21/content` carries it, and no loose copy exists. GF's `pestrapdata.cpp:164–184` reads the same file and copies it verbatim into the table. Same table as FUN_1406B4D40, so GF's claim matches.
- Capture seq061: `*0x1436F2648` = 0x20BEE300 (holder); holder flag = 1; holder+8 = 0x1DFBA960; the 17556 table bytes **equal the stock file byte for byte**; `DAT_1436F2650` = 0 (loaded).
- Layout note: the image's `DAT_1436F2648` is the single holder; the table is read through holder+8 only.

## 2. Keys with index < 0x100 (stock dt13_all.cpk; key = motion id)
- Of 4389 keys (0..0x1124), 234 have index < 0x100 and 4155 are sentinel. The 234 index values are distinct, covering 0x00..0xFF (max 0xFF). The list is `key:index`, with indices in hex:
  (The 234-entry `key:index` list is derived from the stock `Eyestarget.bin` and is not committed; regenerate it from your own `dt13_all.cpk` with the scratch script `chainx_check.py`.)
- Bit 8 of the dword decides it: sentinel ⇔ bit 8 set (the entry's low 9 bits ≥ 0x100). The stored index is the 9-bit value, which indexes X's own entry table (count at `*(X+0x100)+0x50`, entries 16 bytes at +0x58, checked in `FUN_141AFE810`).

## 3. Key function FUN_1406F0F70 and its 157 callers
- Key: `FUN_1406F0F70` (pdata 0x1406F0F70–0x1406F0F9D) keeps `rbp = movsxd(edx)` (0x1406F0F7E). Early exit `cmp ebp,0x1125; jge` (0x1406F0F91/0x1406F0F97): keys ≥ 0x1125 exit. Negative keys pass the signed test, then the index's unsigned test returns 0x100.
- Path to the wrapper, in its `.pdata` chunk 0x1406F10E6–0x1406F1182: holder getter (0x1406F1113), `cmp qword [rsi+0x3AE8],0` (0x1406F111D), `mov edx,ebp; call FUN_1406B4E70` (0x1406F1131), `mov rcx,[rsi+0x3AE8]` (0x1406F1136), `call FUN_141EBC4D0` (0x1406F1147).
- Callers: **157 direct E8 sites** (each pdata-verified; bridge xrefs list 100 of them, all inside the 157; 57 missed by the bridge). **0 E9 sites**. No 8-byte pointer to 0x1406F0F70 in the image, so no vtable or pointer-table entry. Indirect calls made from runtime-built pointers are not excluded.
- Grouped by how edx holds the key at the call (scan of the 24 insns before the call):
  - **Constant (13 sites, 12 keys)**: 163 (0x1407F2CC1), 1322 (0x14071BD00), 1642 (0x140745EE6), 1970 (0x140748432), 2022 (0x140745E7F), 2201 (0x1406EE828), 2253 (0x14077C88D), 3047 (0x14079D2AF), 3063 (0x1407E7D12), 3322 (0x140793B05), 3967 (0x14074051F), 4379 (0x1407A663D and 0x1407B180E). **All 12 keys index to 0x100.**
  - **Register copy (117 sites)**: `mov edx,ebx` 48; `edi` 11; `esi` 9; `r15d` 6, `r14d` 6, `r13d` 6, `r12d` 6; `eax` 3; `movzx edx, word [rbx]` 9 and other memory reads about 12 (motion-id records). The key is a caller variable here and is not resolved statically.
  - **Set in a predecessor block (26 sites)**: edx was not written in the call's block. Needs a CFG to resolve.
  - **Unresolved (1 site)**: 0x1407828F5.
- Wrapper FUN_141EBC4D0 (pdata 0x141EBC4D0–0x141EBC50E; cold chunk 0x141EBC50E–0x141EBC550): one caller, 0x1406F1147. It returns without storing when `DAT_1437F4CB0 == 0` or X == 0. For index ≥ 0x100 it stores 0x100 (0x141EBC4F8). Otherwise the `<` path: `call FUN_141AFE750` (0x141EBC52A), `call FUN_141AFF770` (0x141EBC53C), then `S+0x14 = index` (0x141EBC54D).
- `<` path, verified: `FUN_141AFE750` (pdata begin) passes tag 0xB8A0BF169F98 when its 7th argument is 0 and tail-jumps to `FUN_141AFE810` (0x141AFE805) with key 0. `FUN_141AFE810` guards `index < count` and a non-null entry before it plays that entry (body beyond the guards not decoded). `FUN_141AFF770` (leaf, no `.pdata`) writes 1.0f to `X+0x160[index]` and sets bit `index` of `X+0x178`, only when `X+0x158 != 0` and index < X+0x158 (0x141AFF78D, 0x141AFF79A). The wrapper calls it with index 0.
- Other callers, checked with the same scan: ctor FUN_141EBC1C0 1 site (0x1408BE6A0, stores S+0x14 = 0x100 at 0x141EBC1E5); gate FUN_141EBC820 1 site (0x1408BFD49, per tick); query FUN_141EBC300 1 site (0x1408BEBDD); index FUN_1406B4E70 1 site (0x1406F1131); table loader FUN_1406B4D40 1 site (0x1406B4D12); holder getter FUN_1406B4EA0 10 E8 sites; handle installer FUN_1406F0350 2 E8 sites (0x1408BE832, 0x1408BEFFF; rdx = [model+0x940] = S) and 1 E9 site (0x1408BE9E6, `xor edx,edx; jmp`, which clears the handle).

## 4. X object and vtable 0x142ABA840 (what it does)
X (0x180 bytes; vtable 0x142ABA840 in the captures and the image). Verified slots: +0x00 `FUN_141AFE690` (deleting dtor: base FUN_141AFE4F0; frees 0x180 bytes when flag bit 0 is set); +0x08 `FUN_141AFEE00` (release; `FUN_141EBC2A0` calls it); +0x10 `FUN_141AFF1A0` (leaf: `X+0x50 &= 7`; `X+0x58 = 0` when flag bit 0 is clear); +0x18, +0x28, +0x30 `FUN_141794410` (ret); +0x20 `FUN_141AFF1F0` (mode setter: if not running, `X+0x58 = 0`; otherwise `X+0x58 = mode`; the mode-3 branch is only partly decoded); +0x38 `FUN_141B00910` (per-tick step, `.pdata` 0x141B00910–0x141B00A23). The step: if X+0x5C ≠ 0 or the running bit is clear, `X+0x58 = 0` and return; if the anime block at X+0x88 has bit 10 set, return; otherwise call its own +0x10, then +0x20 with modes 3, 4, 7, 8, 9, 10 in order, then a cleanup call. The vtable has 8 slots (the word at +0x40 is 0x8000000000000000, not code).
- The gate `FUN_141EBC820` runs only when `S+0x14 < 0x100`: it calls `FUN_141AF7A90` (no `.pdata`, not decoded) and tail-jumps through `[X+0x38]` (0x141EBC853).
- Short statement: X is a second motion-blend object. The wrapper points it at one entry of its own table with weight 1.0; each tick it resets and runs its mode sequence. Its per-tick body beyond the slot list is not decoded.

## 5. Verdict and the cheapest live check
- Does X run in a normal match? **Not shown.** Its machinery is live (singleton 1, X present with the right vtable), but in every tick I re-read, `S+0x14 = 0x100`, so X did not run. Those three ticks are one model (P = 0x3AF71500). Which motions would select X is OPEN: 234 keys qualify, the 13 constant-key sites select none, and 143 runtime-key sites decide.
- Live check (one hook line; candidate, Main must vet through `hooks_for_image`):
  `hook chainx_key_1406f1103 0x1406f1103 16 1 1 482186d00a0000f3440f119668240000`
  - Window: 0x1406F1103 `and qword [rsi+0xad0],rax` (7 bytes) + 0x1406F110A `movss` (9 bytes) = 16 bytes, both verified, the next instruction 0x1406F1113 is a call. No branch and no RIP-relative operand inside. `rbp` is written only at 0x1406F0F7E, so `rbp` = the key at entry; `rsi` = P.
  - Predicate (watcher side, Main to check the syntax): `chainx_key_1406f1103=(d(q(q(0x1436F2648)+8)+(rbp&0xffffffff)*4)&0x100)==0 and (rbp&0xffffffff)<0x1125`. It fires only on calls that select X. The snapshot then gives P, S = `P+0x3AE8`, S+0x14 (index) and the key.
  - Cadence: SetMotion frequency is unmeasured. FIRST 1 EVERY 1 with cap 12 is needed for a complete answer. If match pace collapses, sample with EVERY 20, which can miss selections.

## 6. VA table (all decoded through `.pdata`, or a linear decode from a leaf entry)
| VA | what | status |
|---|---|---|
| 0x1406F0350 | handle store `mov [rcx+0x3AE8],rdx` (leaf) | CONFIRMED |
| 0x1406F0F70 | key function, `.pdata` begin | CONFIRMED |
| 0x1406F0F7E / F91 / F97 | key into rbp; `cmp ebp,0x1125`; `jge` exit | CONFIRMED |
| 0x1406F10E6 | separate `.pdata` chunk 0x1406F10E6–0x1406F1182 | CONFIRMED |
| 0x1406F1103 | hook window (16 bytes) | CONFIRMED |
| 0x1406F1113 / 111D / 1131 / 1136 / 1147 | holder getter; S test; index call; load S; wrapper call | CONFIRMED |
| 0x141EBC1E5 | ctor stores S+0x14 = 0x100 | CONFIRMED |
| 0x141EBC4D0 | wrapper `.pdata` begin | CONFIRMED |
| 0x141EBC4F0 / 4F8 | `cmp edx,0x100`; store 0x100 | CONFIRMED |
| 0x141EBC52A / 53C / 54D | `call FUN_141AFE750`; `call FUN_141AFF770`; store index | CONFIRMED |
| 0x141EBC820 | gate `.pdata` begin | CONFIRMED |
| 0x141EBC83A / 853 | `cmp [rbx+0x14],0x100`; `jmp [rax+0x38]` | CONFIRMED |
| 0x1408BFD49 | per-tick gate call | CONFIRMED |
| 0x141EBC34A | query compares S+0x14 | CONFIRMED |
| 0x141AFE750 | `<` shim `.pdata` begin | CONFIRMED |
| 0x141AFE805 | tail-jump to FUN_141AFE810 | CONFIRMED |
| 0x141AFF770 | leaf `.pdata`-less; entry and writes (0x141AFF78D, 0x141AFF79A) | CONFIRMED |
| 0x1406B4E70 / 4E8D | index leaf; `cmp edx,0x1125`; 0x100 return | CONFIRMED |
| 0x1406B4EA4 / 4EF4 | holder getter loads DAT_1436F2648; ret | CONFIRMED |
| 0x1406B4D40 / 4D68 / 4D90 / 4DAE / 4DB8 | table loader; alloc 0x4494; clear loop; fill call; loaded flag | CONFIRMED |
| 0x1406B4C80 | EyesTarget virtual load `.pdata` begin (vtable slot 1 of 0x1425DCE48) | CONFIRMED |
| 0x1415A1708 | fill stub `jmp [rip+0xf8e662]` (leaf) | CONFIRMED |
| 0x142ABA840…878 | X vtable slots 0x00–0x38 (8 slots) | CONFIRMED |
| 0x141AFE690, FEE00, FF1F0, B00910 | X vtable targets `.pdata` begins | CONFIRMED |
| 0x141AFF1A0, 0x141794410 | X vtable leaf and no-op (leaf decode) | CONFIRMED |
| 0x1408BE6A0 / BEBDD / BE832 / BEFFF / BE9E6 | ctor / query / installer callers | CONFIRMED (E8/E9 scan) |
| live: P 0x3AF71500, S 0x3AF6C8E0, X 0x2FA967D0, S+0x14 0x100 | seq061, seq1128, seq1171 | CONFIRMED (captured memory) |
| live: holder 0x20BEE300; table 0x1DFBA960 == stock | seq061 | CONFIRMED (captured memory, byte equal) |
| FUN_141AF7A90, FUN_141AFE810 body, FUN_141B00910 body | not fully decoded | OPEN |

## 7. Open items
1. The live key for the 143 runtime-keyed SetMotion sites (what matters for a normal match). Settled by §5's hook.
2. Key for the 26 predecessor-block sites and 0x1407828F5 (needs a CFG).
3. X's per-tick body (FUN_141B00910 modes, FUN_141AFF1F0 mode 3), FUN_141AFE810 playback, and FUN_141AF7A90: not decoded beyond the slot list above.
4. Who calls the EyesTarget object's load slot (the anime resource loader that passes the file buffer): not decoded.
5. The msvcrt copy routine behind slot 0x14252FD70 is identified by byte equality only; its code is in a deferred mapping and was not read.
6. Whether X+0x158 is nonzero in a live match (precondition of FUN_141AFF770): not read.
7. Doc corrections for `pes-port-AnimeLayer7-adjtick.md` §Second chain X: the holder's table is FUN_1406B4D40's 0x1125-word table, loaded from Eyestarget.bin via the EyesTarget object (not "not decoded further"); FUN_1406F10E6 is a separate `.pdata` chunk of the key function; the caller count is 157 (E8), not 100.
