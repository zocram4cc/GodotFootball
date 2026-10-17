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
