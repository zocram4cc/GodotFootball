# Ball-track arm, player +0x661 flag, P+0x2FC ring (S1Watch, rounds 11–13)

> **Main correction 19-10-26 (from S1Port):** §5 item 5 cites FUN_140A489F0 (P_ai), not FUN_140A48910, for the 0x1414/0x1420 pick; the x of the B+0x2920==4 branch is the live player x [player+0x55C]; the +0x5D0 compare is on the player record.

> **Main correction 19-10-26:** the K+0x6E ball-track arm is an AND, not the OR written in §7: raw asm `0x14063AAFA` `cmp [r12+0x4c],r13b; je skip`, `0x14063AB01` `test r14b,r14b; jne skip`, `0x14063AB06` set. So the flag is set iff B+0x4C != 0 AND r14 == 0 (r14 = the distance test via `cmova` at `0x14063AAF6`, combined with B+0x80).
> **Main audit 19-10-26:** `0x14063A520` `mov rax,[rcx+0x40]` (B = *(G+0x40)), `0x140876014` `movb [rcx+0x4c],1`, `0x140876019` `movb [rcx+0x4c],0`, `0x140433DF5` `movups xmm1,[rdx-0x30]` (the per-frame B<-N block copy): all match. B+0x4C = validity of the ball's first predicted point (pred[0].y >= 0).
> **Main audit 19-10-26 (round 12):** `0x1406BC829` `cmp [rax+0x661],r15b` (reader) and `0x1408C4CDD` `movb [rdi+0x729],0` (the clear) match. Several cited VAs (`0x140432AA5`, `0x140432AAD`, `0x140414A6C`, `0x1408C4CE4`) are hardware-watch trap IPs, i.e. the instruction AFTER the store. The P+0x2FC ring did not change in 90 s for any player (cadence unobserved).
> **Main 19-10-26 (round 13):** P+0x2FC ring static between events in every window measured (120 s, 21 kicks, stoppages); the writer is not found. +0x661 is set via the bVar2 copy path in FUN_1408C48C0, with the conditions open. Both PARKED.

Tags: **CONFIRMED** = decompile, image byte, or live read/watch with VA. **INF** = inference. **OPEN** = not resolved. Watch method: `perf record -I -e mem:<4-aligned addr>:w -t <tid>` on each JobGraph worker and the main thread. "Trap IP" = the address reported by perf, which is the instruction after the one that wrote. Source and destination come from the trap registers.

## 1. B+0x4C (round 11, CONFIRMED)

- `B = *(G+0x40)` (`FUN_14063A490` loads `[rcx+0x40]` at `0x14063A520`). Arm B at `0x14063AB06` sets `K+0x6E` when `K+0x6C == 0 && B+0x4C != 0 && (B+0x80 == 0 || !(d2 > d1))`.
- `B+0x4C == (B.pred[0].y >= 0)`, `pred[0] = B+0x1C`. Writer `FUN_140875FD0(obj, idx, vec)`: `obj+0x4C = (vec.y >= 0)` when `idx == 0` (stores at `0x140876014`/`0x140876019`). 1,211/1,211 live frames.
- `B+0x4C` is a mirror of `N+0x4C` (`N = B+0x296C`), restored each frame by `FUN_140433CF0` (trap `0x140433DF5`, dst `B`, src `N`).

## 2. Player +0x661 (ACCELERATE tail flag), CONFIRMED except where noted

**Reader.** `FUN_1406BB570` lines 653–658: `lVar20 = FUN_140A48A10(G, id)` (player object `p = *(G+0x1DD8+0x28·id)`), then `if (*(char*)(p+0x661) != 0)` (`cmp byte [rax+0x661], r15b` at `0x1406BC829`). Round-13 correction (ClipWords): the anime `vt+0x08` slot selects the record; the `+0x661` read sets only bVar23 (653–658). The bit stays a named input until the sub-functions below are decoded.

**Mirror.** `p+0xD1D == p+0x661` in 2,711 / 2,711 frames, transitions on the same frame.

**Per-frame copies into the player (CONFIRMED, registers at trap):**
- `FUN_1404313C0(dst, src)` (0x52-byte struct copy; byte moves at trap IPs `0x14043147E`/`0x140431486` write `dst+0x50`, `dst+0x51`). Per frame: `dst = p+0xCCC`, `src = A+0x6D8` (the AI record A).
- Block loop trap `0x140432AA5`/`0x140432AAD` (`FUN_140432D30` region, 128-byte stride): copies `p+0xD1C → p+0x660`, delta `+0x6BC`. So `p+0x661 = p+0xD1D` one frame later.

**AI record A.** `A` is `param_1` of `FUN_1408C48C0`. For player 7, `A = 0x32C1FB80`. `A+0x18` = player index. `A+0x6D8` = the 0x52-byte state record, so the flag byte is `A+0x6D8+0x51 = A+0x729` (CONFIRMED live: the source byte equals `p+0x661` in 730 samples).

**Writer of `A+0x729` (`FUN_1408C48C0`, 173 lines, CONFIRMED structure):**
- Top gate: `lVar8 = FUN_141B985E0(); if (lVar8[0xB9A0] == 0) goto LAB_1408C4CDD` (clear).
- `bVar2 = cVar4 || cVar5 || cVar6`: `cVar4 = FUN_1408C2760(A, &local_d0, A+0x624)`, `cVar5 = FUN_1408C31C0(A, &local_d0, &local_b8, local_d8)` (only if `cVar4 == 0`), `cVar6 = FUN_1408C3D10(A, &local_b8)`.
- `local_68 = CONCAT11(1, local_68)` is executed unconditionally after the bVar2 test (before the branch below). `local_68` is the 2-byte stack variable whose high byte sits at `local_b8 + 0x51` (INF from Ghidra's rbp-relative naming, which puts the 0x52-byte struct `local_b8..+0x51` ending exactly on `local_68`'s high byte). Data support: the byte `A+0x729` is 1 only right after a copy-in event (18 events in 150 s, flag 1 in 730 samples).
- `if (bVar2)`: `FUN_1404313C0(A+0x72C, A+0x6D8)` (save previous), `FUN_1404313C0(A+0x6D8, &local_b8)` (write current; the **rising-edge** trap), `A+0x77D = 0`, return.
- Else (`LAB_1408C4CDD`): `A+0x729 = 0` (trap `0x1408C4CDD` + 7 bytes = `0x1408C4CE4`) and `A+0x948 = 0`. This clear trapped about 8,060 times in 150 s (roughly every frame while the flag is 0).

**The three sub-functions (what writes into `local_b8`, CONFIRMED by grep of the decompile):**
- `FUN_1408C2760(A, &local_d0, A+0x624)`: does not receive `&local_b8`, so it cannot write the struct. Its return is `cVar4`. Body not decoded further here.
- `FUN_1408C31C0(A, &local_d0, &local_b8, local_d8)` (398 lines): writes `local_b8+0x18+slot = 1` (line 293, `slot = FUN_14081D8B0` list entry, ≤ 21) and `local_b8+0x1F = 1` (line 359, player 7's own flag), and dword fields `[0..0xE]`, `[0x12]`, `[0x13]`. It writes `local_d8[0] = 1` (`param_4`, line 275) which feeds only the `cVar3` test. It does **not** write `+0x51`.
- `FUN_1408C3D10(A, &local_b8)` (111 lines): reads `local_b8+0x1F`; for each entry calls `FUN_1408C3FA0(A, …, entry, &local_b8)` and returns `uVar13 = 1` if any call returns true. It writes nothing to `local_b8` itself.
- `FUN_1408C3FA0` (139 lines, `param_6 = &local_b8`): writes `+0x10 = 1` (line 94), `+0x18+slot = 1` (line 95), and dwords `[0]`, `[1..3]`, `[5]`, `[0xB]`, `[0x12]`, `[0x13]`. It does not write `+0x51` by constant offset.

**Conclusion for item 2:** no sub-function writes `local_b8+0x51` by a constant offset. The byte is set by `CONCAT11(1, …)` in `FUN_1408C48C0`. So `A+0x729 = 1` exactly when the bVar2 copy path ran in that frame; `bVar2` is the OR of the three return values, and their conditions are as above (the return-value conditions inside `FUN_1408C2760` and `FUN_1408C3FA0` are not fully decoded: OPEN).

**Caller gate (CONFIRMED, `FUN_1409F1640` line 25):** `if (*(char*)(A+0x7C8) != 0 && *(longlong*)(A+0x7D8) != 0)` then `FUN_1408C48C0(A)`. Second caller at `0x140A367E4` (not decoded). Writers of `A+0x7C8` (byte) and `A+0x7D8` (pointer): OPEN. The raw scan returns about 580 stores at `+0x7C8` and 82 at `+0x7D8`, mostly unrelated displacements, so they need a narrower filter.

## 3. P+0x2FC ring (round 13, corrected)

**Reader.** `FUN_141EB50B0(P, out, idx)`: `k = min(idx/10, 5)`; `out.xy = *(P+0x2FC+12k)`, `out.z = *(P+0x304+12k)`. Six 12-byte slots at `P+0x2FC..0x343`.

**Copy chain (CORRECTED).** Earlier I said the source was `A+0x1384`. The trap registers show the source of the slot-0 dword is `dst + 0x15755778`, i.e. `0x32C20F44` for dst `p+0x9B8` (dst `p+0x978+0x40`, copy trap `0x140414A6C`). Also `p+0x9B8 → p+0x2FC` is the `0x140432AAD` loop. **`A+0x1384` is not the ring source.** The copy chain is `0x32C20F44 → p+0x9B8 → p+0x2FC` each frame (CONFIRMED by trap registers; the source address is an inference from the delta).

**Writers.** Watch of `0x32C20F04` (the region I had called `A+0x1384`), 240 s on all workers and main: **0 writes**. Watch of the ring's source `0x32C20F44` was not run. The `0x32C20F04..0x32C20F4B` region changes in play (966 of 6,137 frames sampled in 120 s), but the changing dwords are not the one watched.

**Values and triggers (CONFIRMED, player 7, `ev1.json`, 6,137 frames over 120 s, 21 kick-like ball jumps ≥10 m/s, 5 match-state transitions, states 1/2/3 all present):**
- The player-7 ring `p+0x2FC` (slots 0–5) never changed value, in any state, including the kicks and the stoppages. The earlier 90 s all-player sample also had zero changes for all 22 players.
- Contents in play: slot 0 `(0,0,0)`, slot 1 `(0,0,−1)`, slot 2 `(1.02,0,0)`, slot 3 `(0,0,0)`, slot 4 `(0,0,−1)`, slot 5 `(0.15,0,0)`. These are fixed values (sentinel `z = −1` in slots 1 and 4). **Kickoff contents were not sampled.**
- Cadence: no change observed at any trigger in these windows (kick, restart, stoppage). The ring is **static between events** as far as these windows show. The event that does update it, if any, was not observed.

**OPEN.** The writer of the ring's real source (`0x32C20F44`-region, not yet watched), and whether a clip start (a motion change, not a kick) writes it.

## 4. K+0x40 point (round 11, CONFIRMED)

`P = K+0x40` (default `MA+0x2482C`); branches: `MI+0x155C` if `K.mode == 1`; `player+0x560/+0x564` if `B+0x2920 == 4`; a per-player branch through `FUN_140A6E300` (not decoded); `B+0x1414`/`+0x1420` by `FUN_140A58320`; else `FUN_140A57800(B)`.

## 5. Rules sent

- S1Port: B+0x4C rule (round 11).
- ClipWords: RULE 1 (flag chain, with the A+0x729 correction) and RULE 2 (ring; corrected in round 13 — the source is not `A+0x1384`, and the ring did not change across kicks or stoppages in the measured windows).

## 6. Open

1. The writer of the ring's real source (`0x32C20F44` region) and any clip-start trigger.
2. The return-value conditions of `FUN_1408C2760` and `FUN_1408C3FA0` (bVar2).
3. Writers of `A+0x7C8` and `A+0x7D8`, and the second caller `0x140A367E4`.
4. Kickoff ring contents (no kickoff in the measured windows).
