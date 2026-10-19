# HitData frame bits vs FUN_1406B10D0 track header (S1Watch, round 17)

> **Main audit 19-10-26:** `0x1406B0478` `shr ebx,0xd` (HitData frame = bits 13..20, no +2): matches. The +2 belongs to the clip tick count (FUN_1406B10D0, track-header word), not the touch frame.

Tags: CONFIRMED = raw asm read from `pes_full.bin` this round. INF = inference. OPEN = not resolved.

## 1. Two different words, both using bits 13..20

**FUN_1406B10D0 (clip tick count), raw asm (CONFIRMED):**
- `1406b10fb movzx eax, byte [rcx+4]` : track count.
- `1406b1107 mov rdx, [rcx+rax*8+0x90]` : **track header pointer** = `rec+0x90+8·track`. Not the HitData array.
- `1406b111b mov ebx, dword [rdx]` : `w` = u32 at the header.
- `1406b111d shr ebx, 0xd`, `1406b1120 add bl, 2` : `bl = low8(w>>13) + 2`.

So `n = byte((w>>13)+2)` is shorthand for that: the low byte of `(w>>13)` plus 2. It is not an offset read at +2. The `+2` is an add to the 8-bit register `bl`. Used in `2n/rate · 54/60`.

**FUN_1406B03D0 (hit frame for a track), raw asm (CONFIRMED):**
- HitData array: `rec+0x20+8·k` (`1406b045d movzx eax, byte [rcx+1]` count, `1406b0465 mov rdx, [rcx+rbx*8+0x20]`).
- `1406b0476 mov ebx, dword [rdx]` : `w` = u32 at HitData `+0`.
- `1406b0478 shr ebx, 0xd` ; `1406b043b`/`1406b0438` is the sub-hit path (`shr 0xb; and bl,0xf; inc bl`).
- `1406b0458 jmp 1406aff30` after `movzx ecx,bl` on the HitData path: the frame is `low8(w>>13)` with **no +2**.

So the HitData touch/hit frame = bits 13..20 of the u32 at HitData `+0`, read directly. This agrees with `pes-re-anim-select.md` §6.1 and with FirstTouchPort2's reading.

## 2. Which is which

| word | address | reader | use | offset added |
|---|---|---|---|---|
| track header `w` | `rec+0x90+8·track` | FUN_1406B10D0 | clip tick count `trunc(0.5+2n·54/60/rate)` | `+2` (n = field+2) |
| HitData `w` | `rec+0x20+8·k` `+0` | FUN_1406B03D0 (HitData branch) | hit frame = field | none |
| sub-hit `w` | `rec+0x48+8·k` `+0` | FUN_1406B03D0 (sub-hit) | hit frame = bits 11..14 + 1 | `+1` |

My earlier note used the track-header word for the clip length, which is a separate field. It did not give the hit frame.

## 3. Clip 1351 (gkcatch_b00_0_0_y00)

Not computed. The clip is in `PES21/Data/dt13_all.cpk` (string found there). That archive is packed and was not unpacked this round. The hit frame is `(u32 at HitData+0 >> 13) & 0xFF`, where HitData is `motion[0] + 1351·0x390 → +0x20 → [k] → +0`. The value must be read from the decoded motion record (or live from the loaded motion object). OPEN.

## 4. Message to FirstTouchPort2

Sent: the HitData frame is bits 13..20 directly; the `+2` belongs to the track-header clip-length field, not to the touch frame.
