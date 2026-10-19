# Ball-track arm and player flags: B+0x4C, player +0x661, P+0x2FC ring (S1Watch, rounds 11–12)

> **Main correction 19-10-26:** the K+0x6E ball-track arm is an AND, not the OR written in §7: raw asm `0x14063AAFA` `cmp [r12+0x4c],r13b; je skip`, `0x14063AB01` `test r14b,r14b; jne skip`, `0x14063AB06` set. So the flag is set iff B+0x4C != 0 AND r14 == 0 (r14 = the distance test via `cmova` at `0x14063AAF6`, combined with B+0x80).
> **Main audit 19-10-26:** `0x14063A520` `mov rax,[rcx+0x40]` (B = *(G+0x40)), `0x140876014` `movb [rcx+0x4c],1`, `0x140876019` `movb [rcx+0x4c],0`, `0x140433DF5` `movups xmm1,[rdx-0x30]` (the per-frame B<-N block copy): all match. B+0x4C = validity of the ball's first predicted point (pred[0].y >= 0).
> **Main audit 19-10-26 (round 12):** `0x1406BC829` `cmp [rax+0x661],r15b` (reader) and `0x1408C4CDD` `movb [rdi+0x729],0` (the clear) match. Several cited VAs (`0x140432AA5`, `0x140432AAD`, `0x140414A6C`, `0x1408C4CE4`) are hardware-watch trap IPs, i.e. the instruction AFTER the store. The P+0x2FC ring did not change in 90 s for any player (cadence unobserved).

Status: **B+0x4C CONFIRMED (round 11). Player +0x661 writer chain CONFIRMED to the AI record; its byte-level source (struct local_b8 +0x51) OPEN. P+0x2FC ring: copy chain CONFIRMED to AI record +0x1384; no value change observed in 90 s; cadence not observed; writer of +0x1384 OPEN.**

Tags: **CONFIRMED** = decompile, image byte, or live read/watch with VA; **INF** = inference; **OPEN** = not resolved.

Watch method (all rounds): `perf record -I -e mem:<aligned addr>:w -t <tid>` on each JobGraph worker and the main thread (only the workers write). Watch addresses must be 4-byte aligned (an odd byte address fails with "Invalid event in per-thread mode"). Registers at each trap (`perf script -F ip,iregs`) give the source and destination of a copy.

## 1. B+0x4C (round 11, unchanged)

- `B = *(G+0x40)` (`FUN_14063A490` loads `[rcx+0x40]`, `0x14063A520`). Arm B at `0x14063AB06` sets `K+0x6E` when `K+0x6C == 0`, `B+0x4C != 0`, and `B+0x80 == 0` (or `d2 > d1` fails).
- `B+0x4C == (B.pred[0].y >= 0)`, `pred[0] = B+0x1C`. Writer `FUN_140875FD0(obj, idx, vec)`: `obj+0x4C = (vec.y >= 0)` when `idx == 0` (stores `0x140876014`/`0x140876019`). Live 1,211/1,211 frames.
- `B+0x4C` is a mirror of `N+0x4C` (`N = B+0x296C`), restored every frame by `FUN_140433CF0` (trap `0x140433DF5`, dst `B`, src `N`).

## 2. Player +0x661 (ACCELERATE tail flag), round 12

**Reader.** `FUN_1406BB570` lines 653–658: `lVar20 = FUN_140A48A10(G, id)` (= `*(G+0x1DD8+0x28·id)`, the player object `p`), then `if (*(char *)(p+0x661) != 0)` (`0x1406BC829 cmp byte [rax+0x661], r15b`). CONFIRMED.

**Live behaviour (player 7, 30 s + 60 s).** `p+0x661` is 0 most of the time. 1-runs of 9–60 frames, a few per minute, in 9 of 22 players. Transitions counted: player 7 has 2 in 60 s (lengths 1 frame to 60).

**Mirror.** `p+0xD1D == p+0x661` in 2,711 / 2,711 frames, transitions on the same frame (CONFIRMED live). So `p+0xD1D` is the mirror of the flag.

**Per-frame copies into the player block (CONFIRMED by watch and registers):**
- `FUN_1404313C0(dst, src)`, a 0x52-byte struct copy. The byte moves at `0x14043147A` (`mov [r8+0x50],al`) and `0x140431482` (`mov [r8+0x51],al`) write `dst+0x50` and `dst+0x51`. Registers at the trap: `R8 = p+0xCCC` (dst) and `RDX = A+0x6D8` (src), `A` = the AI record. So `p+0xD1C/D1D` is `p+0xCCC+0x50/0x51`.
- `0x140432AA5` (in `FUN_140432D30`'s block loop, 128-byte stride): copies `p+0xD1C..` to `p+0x660..`, delta `+0x6BC`. The watched dword `p+0x660` was written about 8,500 times in 150 s by this single IP on the workers.

**AI record A.** `A = FUN_1408C48C0(param_1)`'s `param_1`. For player 7, `A = 0x32C1FB80`. `A+0x18` = player index; `A+0x6D8` = the 0x52-byte state record (so `A+0x6D9`... `A+0x6D8+0x51 = 0x32C202A9` is the source of the flag, CONFIRMED live: source byte equals `p+0x661` in 730 samples).

**Writer of `A+0x6D8+0x51` (CONFIRMED structure, `FUN_1408C48C0`, 173 lines):**
- Top gate: `lVar8 = FUN_141B985E0()`; `if (*(lVar8+0xB9A0) == 0) goto LAB_1408C4CDD`.
- `bVar2 = cVar4 | cVar5 | cVar6`, where `cVar4 = FUN_1408C2760(A, &local, A+0x624)`, `cVar5 = FUN_1408C31C0(A, &local, &local_b8, local_d8)` (only when `cVar4 == 0`), `cVar6 = FUN_1408C3D10(A, &local_b8)`.
- `if (bVar2)`: `FUN_1404313C0(A+0x72C, A+0x6D8)` (save previous), `FUN_1404313C0(A+0x6D8, &local_b8)` (write current; this is the copy at `0x1408C4CCF` and the **rising-edge** trap, 18 events per 150 s), `A+0x77D = 0`, return.
- Else `LAB_1408C4CDD`: `A+0x729 = 0` (`0x1408C4CDD`, = `A+0x6D8+0x51`) and `A+0x948 = 0` (`0x1408C4CE4`). This clear trapped about 8,060 times in 150 s.
- The `1` value therefore comes from byte `+0x51` of the stack struct `local_b8`, written by the bVar2 sub-functions (their byte writes are not decoded here: OPEN).

**Caller gate.** `FUN_1409F1640` (191 lines) calls `FUN_1408C48C0(param_1)` only when `*(char*)(A+0x7C8) != 0 && *(longlong*)(A+0x7D8) != 0` (`0x1409F1696` is the call). The other caller is at `0x140A367E4` (not decoded).

**Rule (verified for the copy chain, live):** `p+0x661 = A+0x6D8+0x51` one frame later (through the two copies), with `A+0x6D8+0x51 = 1` only when the bVar2 path wrote a stack struct whose byte `+0x51` is 1. Runs of 9–67 frames happen because the function is not called in the run (the clear does not trap during a run), so the value persists.

## 3. P+0x2FC position ring, round 12

**Reader.** `FUN_141EB50B0(P, out, idx)`: `k = min(idx/10, 5)`; `out.xy = *(P+0x2FC+12k)`, `out.z = *(P+0x304+12k)`: six 12-byte slots, `P+0x2FC..0x343`. CONFIRMED (round 7 decompile).

**Copy chain (CONFIRMED by watch and registers):**
- `A+0x1384` (`A = 0x32C1FB80`, so the source is `0x32C20F04`) → `p+0x978` each frame: `0x140414A6C` (`mov dword [r8+0x..],eax`, struct copy, `R8 = p+0x978`, `R9+R8 = 0x32C20F04`). 2,742 hits in 60 s (about 1 per frame).
- `p+0x978+0x44 → p+0x9BC`, and `p+0x9B8 → p+0x2FC` via the same `0x140432AAD` loop as the flag (delta `+0x6BC`). `p+0x2FC` was written about 3,420 times in 60 s (1 per frame).

**Value samples (CONFIRMED, 4,288 play frames, 22 players, `ringall2.json`):** every ring slot of every player was constant over 90 s: **0 value changes.** The slots hold fixed values (e.g. slot 2 = (1.02, 0, 0), slot 5 = (0.15, 0, 0)).

**Cadence:** not observed. "10 ticks per slot" is **not** supported by this sample. The ring may change only in actions not present in these 90 s (no kick-triggered change was seen either, although kicks occurred in other windows). INF: the ring is written at clip/motion start from a plan in `A+0x1384`.

**OPEN:** the writer of `A+0x1384` (not found; the same per-frame copy shape as B and the flag, so the source is itself a mirror). Next: watch `A+0x1384` (`0x32C20F04`, aligned `0x32C20F04`) across a kick, or an action that changes the ring.

## 4. K+0x40 point (round 11, unchanged)

`P = K+0x40` (default `MA+0x2482C`); branches: `MI+0x155C` if `K.mode == 1`; `player+0x560/+0x564` if `B+0x2920 == 4`; a per-player branch through `FUN_140A6E300`; `B+0x1414`/`+0x1420` by `FUN_140A58320`; else `FUN_140A57800(B)`.

## 5. Rules sent

- ClipWords: RULE 1 (flag chain) and RULE 2 (ring chain, with the open cadence) — `agent://ClipWords`.
- S1Port: B+0x4C rule (round 11).

## 6. Open

1. Byte writer of `local_b8+0x51` (the bVar2 sub-functions `FUN_1408C2760`, `FUN_1408C31C0`, `FUN_1408C3D10`).
2. Gate `A+0x7C8` / `A+0x7D8` writers, and the second caller `0x140A367E4`.
3. Writer of the ring source `A+0x1384` and a ring-changing event (cadence).
