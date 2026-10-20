

> **Main audit 19-10-26:** `0x1425EFBE0` = (0, 3, 6, 10) and `0x1425AB4A8` = 0.23: match. Readers CONFIRMED; the writer of P+0x508 is open (live check queued).
> **Main 19-10-26 (live):** hip bar settled live = 0.8 ([payload+0x30]), feet 0.5 ([payload+0x2C]); the contact blob starts at payload+8 (S1Watch §11). The ball for live work is BallInfo+0x88 (zone-code validated 1065/1065).
## 11. Live hip-bar read (round 26, in a match in play). Result: hip bar SETTLED = 0.8; §10's blob base was wrong by 8.

**Live read (CONFIRMED, `hipbar_live.py`).** `DAT_1437F4808` object = `P.q(0x1437F4808)`; slot `0x29` entry = `*(arr + 0x29·8)`; `payload = *(entry + 8)` (= what `FUN_141E5CA20` returns, CONFIRMED by its asm `0x141E5CA52`). Values at the payload:
- `payload+0x0C` = **88.0** (the closing-speed field of the contact blob, `+0x04` of the blob) ⇒ **blob start = payload + 8**.
- `payload+0x28` = 0.6, `payload+0x2C` = **0.5**, `payload+0x30` = **0.8**, `payload+0x34` = **0.8**, `payload+0x20` = 0.15, `payload+0x24` = 0.2.

In blob coordinates (blob = payload+8): `+0x20` = 0.6, `+0x24` = 0.5, `+0x28` = 0.8, `+0x2C` = 0.8 — exactly the contact doc's forced-fall displacements and the install file (`constant_player.bin`, the byte-match read in round 23). The earlier file-offset 0x12DFA was the blob start, but the decompile's `[rax+…]` reads the **payload** (blob−8), so the file offset for the payload is 0x12DF2 (consistent with the 0x12DF2 column of §8's table).

**Hip compare (CONFIRMED instructions + live values).** The hip bar `xmm6 = xmm9 = [rax+0x30]` at `0x140844823` ⇒ `payload+0x30` = **0.8** (live). So the hip bar is **0.8**, the stock/no-record default too. The §10 "42" was the blob-start misread (`payload+0x30` ≠ `blob+0x30`); §10's conclusion that the layout is inconsistent is withdrawn.

**Other registers, same payload (live):**
- feet k=1,2 bar `xmm6` at `0x14084481E` ⇒ `[rax+0x2C]` = **0.5**;
- forced-fall second bar `xmm7` ⇒ `[rax+0x28]` = **0.6**;
- `xmm8` ⇒ `[rax+0x34]` = **0.8** (the tackling candidate; the branch selecting it is still not resolved, so the plain/tackling split remains open);
- head k=3 uses DAT_142565600 = 0.5.

**Feet gate.** `2·(0.23 − 0) = 0.46 < rec+0x2C` with the bar `xmm6 = payload+0x2C = 0.5` ⇒ the feet gate passes at 0.5 (0.46 < 0.5).

**Open:** which of `xmm6`/`xmm7`/`xmm8`/`xmm9` the tackling case uses (the flag predicate at `0x1408447E6..0x14084480B`, `0x1408448DB`). The hip bar itself is now settled at 0.8 for the compare at `0x1408448ED`.
