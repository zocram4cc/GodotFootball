# PES 2021 style-word reverter: writer scan and live revert timing (FrontmanSource)

Scope: find every writer of the in-match style word (roster block+0x48 and its live copy copy+0x70+0x48 = copy+0xB8) and the source the rebuild reads. Read-only: all live access was `pread` on `/proc/2510857/mem` (`sudo -n`). No live write was made by this pass. FrontmanDecode's `pin.py` was active during the timing run and was not touched.
Tags: **CONFIRMED** = Ghidra decompile/disassembly, E8/qword scan, or live read in this pass. **INF** = inferred. **OPEN** = blocked, with the block named.
Scratch: `<workdrive>/gf-FrontmanSource-scratch/` (scripts `stride128.py`, `store48.py`, `copy48.py`, `qscan.py`, `revert_timing.py`, `diff_probe.py`, `live_ro.py`, `wait_probe.py`; outputs `revert_t1.txt`, `store48_hits.txt`, `copy48_hits.txt`, `intersect.txt`, `roster_copy_fns.txt`, `callers_1414c0300.txt`, `cand_dec/`).

---

## 0. Verdicts

| # | Item | Verdict |
|---|---|---|
| 1 | Per-frame reverter (the function that rewrites `W` and `C` back to `0x1000` within ~17 ms of a write) | **OPEN.** Identity not found. Its write shape and cadence are CONFIRMED (§3). Candidates eliminated with evidence (§2.3). |
| 2 | Per-frame source the reverter reads | **OPEN.** The load-time source is CONFIRMED (§2.2): DB record field `0x3b` through `FUN_1414C0300`. Whether the per-frame path reads the same record is not established. |
| 3 | Normaliser `FUN_141B620D0` as reverter | **Ruled out** (consistent with the brief). Its clear arm sits behind a version-change gate, and the roster block fails its source precondition (§2.3). Its clear rule matches the observed shape exactly (bit 15 only), so it stays a shape match, not a source. |
| 4 | Writers of `block+0x48` that OR style bits from the record | **CONFIRMED**: one function, `FUN_141FE9F40`, reached only from the load chain (§2.1). |
| 5 | Revert cadence and order | **CONFIRMED** (live, read-only): ~16.8 ms mean (~59 Hz); `C` is written 30 µs before `W` in each pass; the only bit that moves is bit 15 (§3). |

---

## 1. Layout (CONFIRMED)

- Roster block: `W = P + 0xE0 + 0x48 + r*0x128` (block+0x48), `P = [G+0x1978]`, `r = P[4+4k]` for local slot `k`. `G = [0x1436F3DC8]`. Matches FrontmanDecode §6.1.
- Live copy: `copy = [G+(i+0xF3)*0x28]`, `C = copy+0xB8` (copy+0x70 is the embedded block; copy+0x70+0x48 = copy+0xB8).
- In-play sample, side 0 slot 1 (i=1, r=14): `W = 0x1D47B830`, `C = 0x1D4A3160`, both `0x1000` (ENFORCER, bit 12), with `MI+0x116C = 1`.
- Copy block and roster block differ in 134 bytes (first at +0x14, `diff_probe.py`). Style (+0x48) and COM (+0x4C) were equal in every sample. INF: the copy is a live-stat object that also carries the style and COM words, synced with the roster block.

---

## 2. Writers and source

### 2.1 The only record-to-block style writer (CONFIRMED)

| Function | Role (decompile) | E8/qword callers |
|---|---|---|
| `FUN_141FE9F40` | `block+0x48 |= 1 << bit`, where `bit = FUN_141FC0AD0(FUN_1414C0300(record))`. Then ORs the COM cards into `block+0x4C` from table `0x14352AC10`. | only `0x141FE9B64` (in `FUN_141FE9B00`). No absolute pointer (qword scan). |
| `FUN_141FE9B00` | Per-player fill: calls `FUN_141FEA170`, `FUN_141FEA000`, `FUN_141FEA250`, `FUN_141FE9F40`, `FUN_141FE9E50`, `FUN_141FE9D00`, `FUN_141FE9BA0`. Does **not** clear `+0x48`; the caller must zero it. | only `0x141FE40F3` (in `FUN_141FE3830`). No qword pointer. |
| `FUN_141FE3830` | Match-build loop. Per (side, slot): `lVar20 = FUN_1414C2170(db, side, slot)` (DB record), `lVar12 = FUN_1408C6560(...)` (element, stride 0xF0), `FUN_141FE9B00(lVar10, lVar12+8, lVar20, slot)`. `db = *(FUN_1414B6A60()+0x50) + 0x35960`. | only `0x141FE2208` (in `FUN_141FE20A0`). |

Field `0x3b` is the EDIT-order enum; `FUN_1414C0300` clamps it to 0..0x15 and maps it through the 22-entry table at `0x142979080` (CONFIRMED). For this player the value `0x1000` = bit 12 = ENFORCER (EDIT 9). Every observed revert value is consistent with that record value (INF until the record is read live).

### 2.2 The load chain and its root (CONFIRMED except where marked)

```
FUN_141FBE810 (E8 hit only inside itself, 0x141FBF805; no qword refs -> root OPEN)
  -> FUN_141FBE2A0 @0x141FBF13F
       -> FUN_141FD1690 @0x141FBE2D1 | FUN_141FD1B90 @0x141FBE2F7 | FUN_141FD5200 @0x141FBE30A
            -> FUN_141FE20A0 @0x141FD17D7 / 0x141FD1CEA / 0x141FD544F
                 -> FUN_141FE3830 @0x141FE2208
                      -> FUN_141FE9B00 @0x141FE40F3
                           -> FUN_141FE9F40 @0x141FE9B64
```

- Every hop has one direct E8/E9 caller (scan `calls.py`, E8 and E9). No absolute 8-byte pointer to any link (`qscan.py`). `FUN_141FE20A0` loads `PATCH10100` resource paths (CONFIRMED from its decompile strings).
- INF (high): the chain is load/setup only. Its root `FUN_141FBE810` has no direct caller, so it is reached by an indirect call that this pass did not resolve. Not CONFIRMED.

### 2.3 Candidates eliminated for the per-frame reverter

| Candidate | Why not the reverter |
|---|---|
| `FUN_141B620D0` (normaliser) | Clear arm `block+0x48 &= 0xFFFF7FFF` is inside `if (version != block+0x10)`, after a source-pointer check (decompile, FrontmanDecode `dec/141B620D0.c`). The roster block's `+0x20` is `0x4D014D4D4D4D4D4D` (FrontmanDecode §6.4, not re-read here), not an object pointer, so the normaliser cannot run on it. |
| `FUN_1404175A0`, `FUN_1408C6730`, `FUN_141238AC0`, `FUN_1413FFFC0` | Struct copy-assign with `+0x48` copies. `FUN_1404175A0` direct callers: `FUN_1404161E0`, `FUN_14041CE80`, `FUN_140500B90`, `FUN_141FE1B30` (load path), plus a tail jump from `FUN_1408C6730`. `FUN_1408C6730` callers: `FUN_141FDC530` (call site 0x141FDC860) and `FUN_141FE3830` (call site 0x141FE4022, load path). Per-frame use of `FUN_1404161E0`/`FUN_14041CE80`/`FUN_140500B90` is not established (INF from names only). `FUN_1413FFFC0` sits under the DB save/load routines below. |
| `FUN_1414016B0`, `FUN_141401C10` | Whole-database save/load walkers (table counts 0x2EE, 0x50, 0x514, 300). Not per frame. |
| `FUN_14101B620` | Builds a snapshot struct from a DB entry; style enum to `param_1+0x54`. No `+0x48` write in its Ghidra decompile (the byte-scan hit at `0x14101B761` is not in the aligned listing). Not a block writer. |
| `FUN_140447B50` | Phase-10 state machine. Writes `lVar12+0x48/+0x4C` from getter `FUN_140A74140`, not the roster or copy blocks. |
| `FUN_14092D580`, `FUN_1406E47A0` | Copy `+0x48` from `param_1+0x546C` / `+0x2D38`, into structs larger than 0x128 (`param_2+0x12BC`). Not the block. |
| `FUN_1408B4B60` | Per-player decision function (0x54-stride records). `add esp,0x128` is a stack frame, not a block copy. |
| `FUN_1405DC350` `or [rdx+0x48],ebx` at `0x1405DC773` | Byte-scan misdecode. Aligned Ghidra listing has `CALL 0x140A62180` at `0x1405DC772`. |

Searches that found nothing (CONFIRMED, scans over `.trace`, flat image, `file offset = VA - 0x140000000`, verified on `FUN_141FE9B00`'s prologue and E8 target):
- `btr dword [reg+0x48],15` (`0F BA /6`) and `and dword [reg+0x48],0xFFFF7FFF` (`81 /4`): 0 hits.
- Block-accessor callers intersected with `+0x48` stores: 87 functions. Copy-plus-roster accessor callers: 417 functions. Of the 417, 9 contain a `+0x48` or `+0xB8` store in Ghidra's decompile. All 9 were read; none is the reverter (§2.3 list, and `FUN_140447B50`).
- Callers of the field-`0x3B` reader `FUN_1414C0300` (23 functions, direct and tail calls): none also touches both the copy and the roster accessors.
- Raw byte-window store scans are noisy (misdecodes). Only Ghidra-aligned listings were trusted.

---

## 3. Live revert timing (CONFIRMED, read-only)

Condition: match in play (`MI+0x116C = 1`). FrontmanDecode's `pin.py` was rewriting `W` and `C` to `0x9000` on every loop. This pass only read them (`revert_timing.py`, `pread` only, 3.5 µs per sample).

- 570,037 samples over 2.00 s; 619 changes of `W` or `C`.
- Reverts `W: 0x9000 -> 0x1000`: 60. Revert-to-revert period (gaps > 5 ms, n = 38): **mean 16.83 ms, sd 0.82 ms, range 14.4 to 18.6 ms**, about 59 Hz.
- In each revert pass, `C` changes 30 µs before `W` (median; range 24 to 58 µs, n = 38). The pass writes `C` first, then `W`.
- Pin write to next game revert ("held"): median 14.2 ms, max 16.5 ms, min 6 µs (n = 59). The minimum is a write landing inside a revert pass.
- Only bit 15 moves in the 200 analysed events (`0x9000 <-> 0x1000`). Bits 0 to 14 and 16 to 31 are unchanged.

Discrepancy, OPEN: FrontmanDecode §6.4 observed single writes (no pin) reverted after 0 to 2.7 s, and a 2 ms logger never sampled `0x9000` in 60 s. A ~17 ms per-frame reverter would show some `0x9000` samples in 60 s at 2 ms resolution. So the reverter is probably gated (by match phase, or by a value that a single write does not reach). The gate is OPEN.

---

## 4. Source (verdict)

- **Load-time source (CONFIRMED):** DB record field `0x3b` at `lVar20 = FUN_1414C2170(db, side, slot)`, `db = *(FUN_1414B6A60()+0x50) + 0x35960`. Through `FUN_1414C0300` and `FUN_141FC0AD0` into `block+0x48` by `FUN_141FE9F40`. A single write to field `0x3b` would change the value this chain produces. It would hold only if the per-frame path re-derives the style from the same record, which is not established.
- **Per-frame source (OPEN).** The reverter writes both `C` and `W` with the same value, `C` first. That fits one routine that reads a single source for both blocks. The source's address is not identified. No per-frame function was found that reads field `0x3b` and touches both blocks (§2.3).

---

## 5. Open items and next steps (no live writes)

1. **Per-frame writer of `copy+0xB8`.** Start from the copy-table consumers (`FUN_140A489F0`, 939 direct callers) and the frame-loop callees that reach the block through `FUN_14124DAD0` (copy+0x70). Look for a per-player loop that writes `+0x48` through a pointer obtained from `FUN_140A489F0` and the roster accessor `FUN_140A3AF90`. Use Ghidra-aligned decompiles only.
2. **Root of the load chain.** Find the indirect caller of `FUN_141FBE810` (vtable or jump table). Resolves whether the record-to-block chain runs per transition.
3. **Gate.** The reverter runs every ~17 ms under the pin but not in single-write observations (§3). Identify the gate variable, read-only, from the frame loop once item 1 names the routine.
4. **Discriminating live test (needs an owner ruling; not run):** a single write of a different style bit on a player whose record field `0x3b` differs, to test whether the reverter re-reads the record (source) or restores a stored value. Not performed here.

No builds or tests were run (no code changed). Ghidra bridge (port 8080) calls and `sudo -n` `pread` samples are the only tool use, plus local capstone scans of `pes_full.bin`.

## 6. Session 2 (Main request, read-only): per-frame writer and indirect caller of FUN_141FBE810

Verdicts: step 2 **OPEN** (exhaustive negative, below). Step 1 **OPEN**: no per-tick writer identified. One true style-word copy found (FUN_140422030, §6.2), reached only through a vtable slot; its per-tick status is unresolved. This session was static only: Ghidra bridge, E8/RIP/qword/RVA scans, and capstone decodes of `pes_full.bin`. No live access, no writes.

### 6.1 Indirect caller of FUN_141FBE810 (step 2): OPEN

- E8/E9 into `0x141FBE810`: one site, `0x141FBF805`, inside `FUN_141FBE810` itself (self-call). No caller outside the function (`calls.py`).
- 8-byte VA references: none (`qscan.py`). Validated: the same scan finds vtable slot `0x142597580` entry 0 (`0x14041EC80`).
- RIP-relative `lea`/`mov`/`call` to `0x141FBE810`: none (`ripref.py`).
- 4-byte RVA entries: two, at `0x14332D550` (an ip-to-state map whose first pair is `FUN_141FBE810`'s own begin) and `0x143C3B754` (a `.pdata` RUNTIME_FUNCTION begin). Not a switch or jump table. `FUN_141FBE2A0` shows the same two-entry pattern.
- INF: the external caller is a runtime-computed pointer (closure, registered callback, or task body) that is not in the image. Not resolvable statically from this dump.

### 6.2 Per-tick writer search (step 1): OPEN; candidates eliminated

Method: flat image, VA = `0x140000000 + file offset` (an offset bug in two early scans was fixed; the earlier raw-byte store scans were noisy and were re-checked against Ghidra-aligned listings).

- Roster stride `0x128`: `imul r,r,0x128` at 18 sites in the roster getters `FUN_140A3AC20, FUN_140A3AF30, FUN_140A3AF60, FUN_140A3AF90, FUN_140A3B040, FUN_140A3B0E0, FUN_140A3B110, FUN_140A3B140, FUN_140A3B1B0, FUN_140A3B1F0, FUN_140A3B250, FUN_140A3B2A0` and five others (CONFIRMED). Pointer-walk loops `add r,0x128`: 62 functions (CONFIRMED). Intersected with `+0x48` stores (Ghidra-checked): `FUN_141C2E880`, `FUN_141CE3780`, `FUN_141DD7540`, `FUN_141DEDEC0` store a value from an event record (`piVar+6/+8`, type 2), not the style word; `FUN_1420D4A50` uses `bVar4*0x128` over two team-indexed records and accumulates floats; `FUN_1420C3B90` writes team name strings (its `+0x48` byte pattern is a decode artifact). None is the reverter.
- Literal 0x128 copy (`mov r32,0x128` before a call, `rep movsq` 37, `rep movsb` 0x128): none in the copy- or roster-accessor callers. The 458 raw `mov imm32 0x128` byte matches are `[rbp+0x128]` displacements (false positives, verified on `FUN_140588CA0` and `FUN_1409677C0`).
- Disp32 `+0xB8` stores (copy+0xB8 would be encoded this way when the copy base is held in a register): 4,198 instructions; 15 stores in copy- or roster-accessor callers. None is the style word: `FUN_1405103E0` copies an action record `+0x20 → +0xB8`; `FUN_1408A2180` writes floats; the rest are state or enum fields.
- The one true style-word copy found: `FUN_140422030`. `mov [RDX+0x48], EAX` after `mov EAX, [RCX+0x48]` (`0x1404224C2/C5`), with the 0x40 skills qword copied alongside (`0x1404224BA/BE`). Source `RCX = team+0x298C+0x80` (team object, fixed offsets), destination `RBX+0x15C0` region (`RBX = [team+0x68]`). It is one fixed-offset block copy, not a loop over 0x128 records. A single `IMUL R8,RDX,0x74` at `0x1404224FF` indexes a 0x74-stride table (player table, not the roster). The loop structure and the gate guarding the copy were not decoded (OPEN). No E8 callers.
- How it is reached (CONFIRMED): `FUN_140422030` is virtual slot `+0x98` of the primary vtable at `0x1425984C0` (slot 19; `0x1425984C0 + 0x98 = 0x142598558`). The class's secondary vtable is at `0x142598568` (stored at `object+0x10` by `FUN_140420480`). Slot 0 is `FUN_140421610` (scalar deleting destructor). The class object is 0x3658 bytes, allocated in `FUN_140418940` (`mov edx,0x3658` at `0x14041901F`); its ctor `FUN_140420480` is called only from `FUN_140421610`; `FUN_140418940` is called from `FUN_14041A710`, which is itself recursive (its only E8 caller is inside it, `0x14041B23E`). The outer caller is OPEN.
- Whether this object's slot 19 runs per tick, and whether its `RBX+0x15C0` / `RDX` alias the copy (`copy+0x70`) or the roster block, is OPEN.
- Other lead (unexamined): `FUN_141FE3830` (the match-build loop, §2.2) addresses its player blocks as `FUN_1408C6560(param_1+0x140, side, slot)+8`, i.e. stride `0x3A10` per side and `0xF0` per slot from `+0x68`. That is a second block layout, distinct from the 0x128 roster and not yet mapped to the copy or the roster. Its `+0x48` for side 0 slot 1 was not style-valued in the probe I ran (`mirror_probe.py`, base `G+0x140` assumed, OPEN).

### 6.3 Next steps (for Main or the next pass)

1. Find the `call [reg+0x98]` site(s) whose `reg` holds a `0x3658` object of class `0x1425984C0`. Start from the outer caller of `FUN_14041A710` (not an E8 target; search the other `FUN_14041A710` callers via a `call` through a function pointer, or the vtable of its parent). Then check whether that caller is per tick.
2. Decide whether `FUN_140422030`'s `RBX+0x15C0` destination is the copy block (copy+0x70) by its use of `[RBX+0x14DC..0x153C]` and `[RBX+0x15BC]`: both are written by the same function, so the layout is local to it.
3. Resolve the `FUN_141FE3830` element base (`param_1+0x140`) and check it against live `G`, read-only, if the match is in play.

Files: `<workdrive>/gf-FrontmanSource-scratch/` (`ripref.py`, `size128.py`, `loopfns.py`, `disp_b8.py`, `store48_hits.txt`, `disp_b8_hits.txt`, `loop_add128.txt`, `loop_imul128.txt`, `cand3/`, `tbl/`, `dis_140422030.txt`).

## 7. Session 3 (Main request, live, read-only): FUN_140422030 timing and the FUN_141FE3830 element layout

Verdicts. Step 1: **CONFIRMED not the style path** (its object holds no style word; its destination is neither copy nor roster); timing against the 59 Hz revert **not measurable** this session. Step 2: layout **CONFIRMED** live (11/11); the element array holds the stored style of every side-0 player; it is the **load-time** source. Whether a per-frame path reads it to rewrite W and C is **OPEN** (§7.4). No live write was made. Pin state: FrontmanDecode's pin was not active (W and C stayed at `0x1000` with no change over 6 s and 20 s windows), so no revert occurred to time.

### 7.1 Live state (CONFIRMED, reads only)

- `G = 0x371D5090`; `P = [G+0x1978] = 0x1D47A6D8`; side-0 roster r per slot: 11, 14, 2, 1, 7, 6, 8, 4, 21, 20, 22.
- Match state flickers in and out of play (MI null for minutes at a time). Reads were retried until a bound state was seen (mi116c = 3, set piece, at the bound moment).

### 7.2 Step 1: `call [reg+0x98]` and the copy destination

- **Object O = 0x1D69D230.** Found by a heap scan for the secondary vtable `0x142598568` at +0x10; qword at +0 = primary `0x1425984C0` (CONFIRMED). This is the 0x3658-byte class of §6.2. Slot 19 of the primary vtable is `FUN_140422030` (CONFIRMED).
- **Destination.** `RBX = [O+0x68] = D = 0x1E006980` (CONFIRMED, `obj_probe.py`). `D` is neither the copy object (`0x1D4A30A8`) nor the roster block (`0x1D47xxxx`), and not a team object (`T0 = 0x1D462434`, `T1 = 0x1D4804E4`). It is a separate team-level object (name not identified).
- **Source and destination words of the style offset.** Source `S = O+0x298C+0xC8 = 0x1D69FC84` read `0x0` throughout. Destination `D+0x15C0+0xC8 = 0x1E008008` read `0x0`, then changed once to `0xFFD6` at the start of a 20 s window and stayed there. `0xFFD6` is not a style value. So the copy moves zeros on the style offset (in play). **Not the style path.**
- **The object holds no style word.** A dword scan of O (0x3658 bytes) finds no `0x1000`, `0x9000`, `0x8000`, `0x80000`, `0x10000`, or `0x400` (CONFIRMED). A scan of D (0x2400 bytes) finds none either. The live copy (`copy+0xB8`) does hold `0x1000` (CONFIRMED). So `FUN_140422030` cannot be a source of the revert.
- **Timing against the 59 Hz revert.** Not measurable now: no revert events occurred (no pin), and the only write the copy would make is of zeros. Not measured.
- **Caller of the vcall.** Unchanged from §6.3: the outer caller of `FUN_14041A710` (the factory for O) is OPEN.

### 7.3 Step 2: the `FUN_141FE3830` element layout (CONFIRMED)

- **Element address (CONFIRMED, aligned listing).** `FUN_141FE3830` loads `RCX = R15+0x140` and calls `FUN_1408C6560(X, side, slot)` (`0x141FE395E` and `0x141FE40B0`). That returns `E = X + side*0x3A10 + slot*0xF0 + 0x68` (clamped: side < 2, slot < 0x28). The block passed to the field loader is `E+8` (`LEA RDX,[R14+8]`, `0x141FE40EC`, then `CALL 0x141FE9B00` at `0x141FE40F3`).
- **Style word.** `block+0x48 = E+0x50 = X + side*0x3A10 + slot*0xF0 + 0xB8`. For side 0: `X + 0xB8 + 0xF0*slot`. `FUN_141FE9F40` ORs the bit into it (`|=`); it never clears (CONFIRMED).
- **Live container X = 0x293CF70** (CONFIRMED by a 16-byte signature vote, `elem_scan2.py`): slot 0 and slot 2 signatures sit at `0x293D020` and `0x293D200`, differing by exactly `2 × 0xF0`.
- **Index is the roster index r, not the formation slot.** Element `E_r = X + 0xB8 + 0xF0*r`. Over a 20 s window (`final_probe.py`, 2.3 M samples), `element[r]` equals the roster block (style and COM) for **11/11** side-0 players (`element-vs-roster matches 11/11`), and `copy[i]` (`copy = [G+(i+0xF3)*0x28]`, style `+0xB8`) equals the same style for i = 0..10. No change in any of the three during the window.
- **Feed from the element to the roster block (CONFIRMED static).** `FUN_141FE20A0 → FUN_14041C3D0 → FUN_14041C280` builds a 40-entry element-pointer table per side (`FUN_1408C6560(container, side, slot)`) and calls `FUN_140A3B3A0`, which copies each 0xF0 element into the 0x128-stride side array (the array read by the roster getters `FUN_140A3AF90` etc.). Single caller of `FUN_140A3B3A0`: `FUN_14041C280`.
- **Other writers of the element array (CONFIRMED static).** `FUN_141FE1B30` copies 20 elements of 0xF0 bytes from a stack staging buffer (`RBP+0xC0`) into `R15+0x1A8` (= X+0x68, side 0) and into `R15+0x3BB8` (= X+0x3A78, side 1). `FUN_141FE66D0` (one call site). `FUN_141FE3830` (from the DB record, §2.1).

### 7.4 Is the element array the per-frame source? OPEN

- Every writer of the element array is inside the loader `FUN_141FE20A0`, which opens `PATCH10100` files (CONFIRMED from its strings). Its callers are `FUN_141FD1690`, `FUN_141FD1B90`, `FUN_141FD5200` → `FUN_141FBE2A0` → root `FUN_141FBE810` (root OPEN, §6.1). If that root runs per frame, so do all these writers. Not established.
- Readers of the element array: `FUN_1408C6560` has four callers (`FUN_14041C280`, `FUN_141FE20A0`, `FUN_141FE3830`, `FUN_141FE66D0`); all loader-side in the static graph. A direct-stride search found 96 functions with `imul ..., 0xF0`; none of them intersects the roster-getter callers, and only 11 have a `+0xB8` operand, none a roster-touching reader of the element style (CONFIRMED static; not exhaustive over computed pointers).
- So: the element array is the **stored per-player style source of the load path**, and both W and C equal it at every sampled instant (CONFIRMED live). A per-frame writer that rewrites W and C from it was **not found**.

### 7.5 Named source (for the next pass)

- **Candidate source a single write must target:** `E_r.style` at `X + 0xB8 + 0xF0·r` (live X = `0x293CF70`; r = roster index, for side 0 = 11,14,2,1,7,6,8,4,21,20,22). Upstream of it: the DB record field `0x3b` through `FUN_1414C2170(db, side, slot)` → `FUN_1414C0300` (for the `FUN_141FE3830` builder). A write to `E_r` would reach W and C only if a per-frame path copies element → roster block and copy. That path is not found, so this is **OPEN**.
- **Discriminating test (needs an owner ruling; not run):** write `E_r.style` (one dword, bit 15 set) and sample W and C at 1 ms. If W and C follow within one frame, the element array is the per-frame source. If they revert to `0x1000` at ~17 ms while E_r stays set, the revert is reading something else (the DB record or a staging copy).

### 7.6 Corrections to earlier sessions

- `mirror_probe.py` (§6.2 and session 2 notes) used `G+0x140` as the container. The correct container is `X = 0x293CF70` (live), not a G-relative address. Its values were not style values; that probe is void.
- §6.2's "the copy writes a fixed-offset team block" stands, with one addition: its destination `D` is a separate object, and on the style offset it moves zeros (§7.2).

Scratch: `<workdrive>/gf-FrontmanSource-scratch/` (`locate_obj.py`, `obj_probe.py`, `find_style.py`, `play_probe.py`, `elem_scan2.py`, `elem_probe.py`, `elem_by_r.py`, `final_probe.py`, `stride_f0.py`, `f0_b8.txt`, `final_run.txt`, `elem_run2.txt`, `dis_141fe3830.txt`, `dis_141fe20a0.txt`, `elem/`, `tbl/`).
