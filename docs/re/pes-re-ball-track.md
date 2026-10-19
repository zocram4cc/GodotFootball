# Ball-track arm: B+0x4C writer and K+0x40 point (S1Watch, round 11)

> **Main correction 19-10-26:** the K+0x6E ball-track arm is an AND, not the OR written in §7: raw asm `0x14063AAFA` `cmp [r12+0x4c],r13b; je skip`, `0x14063AB01` `test r14b,r14b; jne skip`, `0x14063AB06` set. So the flag is set iff B+0x4C != 0 AND r14 == 0 (r14 = the distance test via `cmova` at `0x14063AAF6`, combined with B+0x80).

> **Main audit 19-10-26:** `0x14063A520` `mov rax,[rcx+0x40]` (B = *(G+0x40)), `0x140876014` `movb [rcx+0x4c],1`, `0x140876019` `movb [rcx+0x4c],0`, `0x140433DF5` `movups xmm1,[rdx-0x30]` (the per-frame B<-N block copy): all match. B+0x4C = validity of the ball's first predicted point (pred[0].y >= 0).

Status: **B+0x4C meaning and writer CONFIRMED** (static decode + live watch + live rule test). K+0x40 point chain CONFIRMED (static). Two extra items from Main (player +0x661 writer; P+0x2FC ring cadence) **not done** in this round (see §6).

Tags: **CONFIRMED** = decompile, image byte, or live read with VA; **INF** = inference; **OPEN** = not resolved.

## 1. `B` = `*(G+0x40)` (CONFIRMED)

- `FUN_14063A490(K, ctx, cmd)` loads `B` at `0x14063A520` (`mov rax,[rcx+0x40]`) with `rcx = G`; the same block loads `[rcx+0x2AA8]` (FieldInfo). `mov [rsp+0x70],rax` at `0x14063A524`; read back as `r12` at `0x14063AA95`.
- Live: `*(G+0x40) = 0x0A3E8B04` in this match.

## 2. The arm (CONFIRMED, `0x14063AA5E..0x14063AB11`)

```
14063AA5E  mov byte [rdi+0x6e], r13b          ; K+0x6E := 0
14063AA91  mov byte [rdi+0x6e], 1             ; arm A (K+0x44 > P.byte31/100)
14063AA9A  movzx r14d, byte [r12+0x80]        ; r14 = B+0x80
14063AAA3  test r14b,r14b ; jne 0x14063AAFA
14063AAA8  call 0x140A6D0D0                   ; al = *(B+0x4C)   (FUN_140A6D0D0 = return *(p+0x4C))
14063AAFA  cmp byte [r12+0x4c], r13b
14063AB01  test r14b,r14b ; jne 0x14063AB11   ; needs r14 == 0
14063AB06  mov byte [rdi+0x6e], 1            ; arm B
```

So arm B sets `K+0x6E` iff `K+0x6C == 0`, `B+0x4C != 0`, and `B+0x80 == 0` (when `B+0x80 != 0` the distance test `d2 > d1` decides, see `pes-re-team-mode.md` §8). Live: `B+0x4C = 1` only with `B+0x80 = 0` in 3,419 samples.

## 3. What `B+0x4C` means (CONFIRMED)

**It is a validity bit for predicted ball point 0.** The ball's prediction block has three `vec3` samples at `B+0x1C`, `B+0x28`, `B+0x34` (`FUN_140A6CF30(B, i)` = `B + (3i+7)·4`, CONFIRMED), and `B+0x40..0x4F` holds their tick counts and the bit.

- **Writer:** `FUN_140875FD0(obj, idx, vec)` (CONFIRMED decompile): for `idx < 3` copies `vec` to `obj+0x1C+12·idx`; **if `idx == 0`: `obj+0x4C = (vec.y ≥ 0) ? 1 : 0`** (`0x140876014` `mov byte [rcx+0x4C],1`, `0x140876019` `mov byte [rcx+0x4C],0`, under `test dl,dl`).
- **Sample filler:** `FUN_140874100(p, tick, vec)` (CONFIRMED decompile; no direct E8 caller, reached indirectly): sets latch `p+0x1C = 1` when `vec.y ≥ 1.0`; then, when `p+0x14 ≤ 0, ≠ 0` and `vec[4] > 0` and `p+0x1D < 3`, calls `FUN_140875FD0(p+8, p+0x1D, vec)` and stores `tick+1` (capped at 0xF9) at `obj+0x40+4·slot`. Slot counter `p+0x1D` advances.
- **Sentinel:** the reset loops (`FUN_1408709B0`, `0x140875D50`, `0x140875E00`, `0x140870FA0`) fill each sample with `(0, −1.0, 0)` and clear the bit. So `y = −1.0` means "no point".
- **Live rule (CONFIRMED):** `B+0x4C == (B.sample0.y ≥ 0)` in 1,211 / 1,211 frames (and `N+0x4C == (N.sample0.y ≥ 0)` likewise). When it is 1, `sample0 = e.g. (25.385, 0.109, −20.391)`, a real ground point at the ball's rest height; when 0, `sample0 = (0, −1, 0)`.

**Meaning:** `B+0x4C` = "the ball prediction has a valid first point (a ground point the ball reaches)". So arm B fires when the ball prediction has a valid first point and the `B+0x80` gate is clear.

## 4. Mirror and restore (CONFIRMED)

- `N = B + 0x296C`, a second copy of the ball block (`N == B` in position in 1,135 / 1,135 frames; `N+0x4C == B+0x4C` in 741 / 741 frames).
- **Restore copy `FUN_140433CF0(X)`** (128-byte block loop, `0x140433DC0..0x140433E11`), copies `N → B` per frame. The live trap (`perf -I`, JobGraph thread) at IP `0x140433DF5` shows dst `AX = 0x0A3E8B84` (block base `B`) and src `DX = 0x0A3EB4F0` (block base `N`), 2,003 hits per 150 s per thread. Callers (E8): `0x140432DB5` (`FUN_140432D30`, guarded by `*(p+0x90) != 0`) and seven in `0x141FB…`/`0x141FC…`.
- **Writers of the mirror `N+0x4C` (live, `perf -I`, 300 s, all JobGraph threads and main):** IPs `0x140872B7D` (store `mov [r14+0x4C],0` at `0x140872B78`, in `FUN_140872570`), `0x140875DC4` (store `mov [r9+0x4C],r11b`, `r11 = 0`, in the reset at `0x140875D50`), `0x140875E54` (store `mov [r8+0x4C],r9b`, `r9 = 0`, `0x140875E00`), `0x140870A04` (reset `FUN_1408709B0` family), `0x140876018` (`FUN_140875FD0`, the 1/0 store — the rule above), `0x140870FC7`. So the bit is set by `FUN_140875FD0` and cleared by the resets and by `FUN_140872570`.

## 5. K+0x40 point (CONFIRMED structure, `FUN_14063A490`)

`P = K+0x40` (`pfVar1 = param_1 + 0x10`; `param_1[0x10..0x12]`):

1. **Default:** `P = MA+0x2482C` (`MA = *(ctx+0x18)`, `local_f0`), with `MA+0x24830`, `MA+0x24834` for the other two words (`0x14063AA..`, decompile lines 166–170).
2. **If `*(K+0)` (`*param_1`) == 1:** `P = MI+0x155C` (`local_118 = *(G+0x270)` = MI; `pfVar18 = local_118+0x155C`).
3. **Else if `B+0x2920 == 4`:** `P.y = player+0x560`, `P.z = player+0x564` (`lVar8 = FUN_140A48A10(G, cmd[0])`), x not reassigned in this branch (decompile artifact; INF).
4. **Else if `player+0x5D0 == 5`:** a per-player branch through `FUN_140A6E300` (not decoded here).
5. **Else, `K+0x1B == 0`:** `P = lVar16+0x1414` or `+0x1420` (`lVar16 = FUN_140A48910(G, cmd[0])`), by `FUN_140A58320(...) == 4`.
6. **Else:** `P = FUN_140A57800(B)` (the ball's own predicted point getter).

## 6. Not done this round

- **Main's extra item 1 (player `+0x661` writer, `FUN_1406BB570` ACCELERATE tail):** not started.
- **Main's extra item 2 (ring `P+0x2FC` writer, 6 slots of 12 bytes, cadence):** not started. The static store search (disp `0x2FC`, byte-level) was not run.

## 7. Rule for S1Port

`K+0x6E := 1` by arm B iff `K+0x6C == 0 ∧ B+0x4C ≠ 0 ∧ (B+0x80 == 0 ∨ ¬(d2 > d1))`, where `B+0x4C = (B.pred[0].y ≥ 0)` and `pred[0]` is the ball's first predicted point (sentinel `y = −1`).
