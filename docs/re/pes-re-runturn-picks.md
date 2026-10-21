# PES RUN_TURN_RUN's clip pick: the row, the column, the empty-cell rule, and the swing rows

> **Main audit 20-10-26:** the column table `0x1425DD988` = 9, 11, 13, 15, 17, 8, 6, 4, 2, 0; the row table `0x1425DDA50` = 32 entries 1361..1388; the swing window `0x1406CEFBA` = `lea eax,[r13-0x561]; cmp eax,5; ja; mov ecx,0x29; bt ecx,eax` (range plus bitmask: rows 0x561, 0x564, 0x566 pass): all match.

Read-only RE. No GF code changed, no PES written. Scratch: `<workdrive>/gf-TeamDecode-scratch/dec/`.
Base `0x140000000`, image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`.
Tags: **CONFIRMED** = decompile and/or raw dis this pass, checked by script; **INF** = inference;
**OPEN** = blocker named.

For ClipWords: the `RunTurnRunPicks` branch of the MOVESTEP port. Five items, each with a rule.
Every table is script-read from the image; every operand pair is checked.

---

## §0 The three helpers (the whole pick is these three)

The pick is a **row × column** lookup into one 16-bit-cell table. All three helpers take the
same three arguments: `(table, &row, &column)`.

| helper | VA | returns | meaning |
|---|---|---|---|
| `FUN_1406ABDD0` | `0x1406ABDD0` | `byte` | **is the cell populated?** 1 = yes, 0 = no |
| `FUN_1406ABB30` | `0x1406ABB30` | `short` | the **motion number**, or **−1** when the cell is empty |
| `FUN_1406ABD60` | `0x1406ABD60` | `byte` | the cell's **mirror** flag |

**The table** is `DAT_1436F2378` — a runtime pointer to the clip table
(`FUN_1406ABBA0` returns it verbatim: `return DAT_1436f2378;`).
`DAT_1436F2378` holds `0x2985710` in this image (script-read) — the base of the table.

**The cell index** (CONFIRMED, raw dis of `FUN_1406ABDD0`):

```
1406ABDD0: MOVSXD RAX,dword ptr [RDX]        ; the row
1406ABDD3: MOV R9,RCX
1406ABDD6: CMP EAX,0xbe4                     ; row < 0xBE5 ?
1406ABDB: JA 0x1406abe0a                     ;   -> empty
1406ABDDD: MOVSXD RDX,dword ptr [R8]         ; the column
1406ABDE0: CMP EDX,0x11                      ; column < 0x12 ?
1406ABDE3: JA 0x1406abe0a                    ;   -> empty
1406ABDE5: CMP EAX,0x7c5                     ; the special row 0x7C5 (1989) ?
1406ABDEA: JNZ 0x1406abdf4
1406ABDEC: CMP EDX,0x2                       ;   and column 2 ?
1406ABDEF: JNZ 0x1406abdf4
1406ABDF1: MOV AL,0x1                        ;   -> always populated
1406ABDF3: RET
1406ABDF4: LEA RCX,[RAX + RAX*0x8]           ; row * 9
1406ABDF8: MOV RAX,qword ptr [R9]            ; the table
1406ABDFB: LEA RDX,[RDX + RCX*0x2]           ; col + row*9*2
1406ABDFF: MOVZX EAX,word ptr [RAX + RDX*0x2]   ; the 16-bit cell
1406ABE03: SHR AX,0xf                        ; bit 15
1406ABE07: XOR AL,0x1                        ; -> 1 when bit15 is CLEAR
1406ABE09: RET
1406ABE0A: XOR AL,AL                         ; empty
1406ABE0C: RET
```

**=> the cell is at `table[(row * 9 + column) * 2]`, a 16-bit entry, and its bit 15 is the
"empty" flag: bit 15 SET = empty, bit 15 CLEAR = populated.**
The row stride is **9** (`LEA RCX,[RAX + RAX*0x8]`), and the row range is `0..0xBE4`
(≤ 3044) with the column range `0..0x11` (≤ 17).

**The special cell**: row `0x7C5` (1989) with column 2 is **always populated**; `FUN_1406ABB30`
returns the fixed motion `0x9B4` (2484) for it instead of reading the table.

**The motion number**: `FUN_1406ABB30` returns the 16-bit cell value when bit 15 is clear
(`CMOVNS AX,DX` after `TEST DX,DX`), and **−1** otherwise
(`NEG R9B` / `SBB R10W,R10W` produce `-1` from the `param_4` flag; the out-of-range and
empty paths leave that value in `EAX`).

**The mirror flag**: `FUN_1406ABD60` reads a **byte** at `table[row * 0x12 + 0x17CA + column]`
— a *separate* byte table, not the 16-bit cell.

---

## §1 (Item 1) The row: what AnimeRow returns

**The row is NOT a player slot. It is a "row" in the clip table** — an index into the
motion-record rows, and the values in play are in the `0x565..0x575` range.

There are two distinct things in this area, and they must not be confused:

**(a) The anime player's *slot*.** `FUN_140A627E0` and `FUN_140A62960` return the anime
player's identity:

- `FUN_140A627E0(player, 0)` → `*(uint *)(*(player + 0x18) + 0x2601C)` — a field of the
  player's own record.
- `FUN_140A627E0(player, n)` for `n != 0` → `FUN_140A3E7D0(player+0xE0, &out, n)` then
  `FUN_140A3B320(FUN_140A489D0(DAT_1436F3DC8, out), out._4_4_)`; **0xFF** if the lookup fails.
- `FUN_140A62960(obj, n)` → scans the 11-entry int32 table at `obj + 0xA2A0` for the id
  **`0x3A`** (58) and returns **`index + n * 0xB`**, or **0xFF** if not found.

So `FUN_140A62960` returns a value in `0..10 + n*11` — **a slot, not a clip row**. It is
used in `FUN_1406D6780` only to pick *which* interface/player to talk to, never as a table row.

**(b) The clip-table row.** The row used for the pick is a **motion row** in the `0x565..0x575`
range, and it comes from a **row table** indexed by the request's kind and class.

**The row table: `DAT_1425DDA50`** — script-read, 32 entries:

| idx | VA | value | | idx | VA | value |
|---|---|---|---|---|---|---|
| 0 | `0x1425dda50` | 1361 | | 16 | `0x1425dda90` | 1375 |
| 1 | `0x1425dda54` | 1361 | | 17 | `0x1425dda94` | 1375 |
| 2 | `0x1425dda58` | 1362 | | 18 | `0x1425dda98` | 1376 |
| 3 | `0x1425dda5c` | 1364 | | 19 | `0x1425dda9c` | 1377 |
| 4 | `0x1425dda60` | 1365 | | 20 | `0x1425ddaa0` | 1383 |
| 5 | `0x1425dda64` | 1366 | | 21 | `0x1425ddaa4` | 1384 |
| 6 | `0x1425dda68` | 1364 | | 22 | `0x1425ddaa8` | 1385 |
| 7 | `0x1425dda6c` | 1367 | | 23 | `0x1425ddaac` | 1385 |
| 8 | `0x1425dda70` | 1368 | | 24 | `0x1425ddab0` | 1378 |
| 9 | `0x1425dda74` | 1368 | | 25 | `0x1425ddab4` | 1378 |
| 10 | `0x1425dda78` | 1369 | | 26 | `0x1425ddab8` | 1379 |
| 11 | `0x1425dda7c` | 1370 | | 27 | `0x1425ddabc` | 1380 |
| 12 | `0x1425dda80` | 1372 | | 28 | `0x1425ddac0` | 1386 |
| 13 | `0x1425dda84` | 1373 | | 29 | `0x1425ddac4` | 1387 |
| 14 | `0x1425dda88` | 1374 | | 30 | `0x1425ddac8` | 1388 |
| 15 | `0x1425dda8c` | 1374 | | 31 | `0x1425ddacc` | 1388 |

**The index** is `kind + class * 8` (CONFIRMED, the raw dis of the swing site):

```
1406CEF52: MOVSXD RCX,dword ptr [RSI]        ; req word 0  (the kind)
1406CEF55: MOVSXD RDX,dword ptr [RSI + 0x24]  ; req word 9  (the command speed class)
1406CEF5E: LEA R8,[RCX + RDX*0x8]             ; kind + class*8
1406CEF66: MOV R13D,dword ptr [RDI + R8*0x4 + 0x25dda50]   ; the row
```

`RSI` is the request block, so `[RSI]` = **word 0** (`kKind`) and `[RSI+0x24]` = **word 9**
(`kCommandSpeedClass`) — the port's own `moveword` numbering (`movestep.hpp:123` and `:132`).

**=> `row = DAT_1425DDA50[req[0] + req[9] * 8]`.** This is exactly the port's
`pes::GotoRowFor(kind, cls, t)` (`movestep_run.cpp:323`, whose comment already cites
`0x1425dda50` and the same `kind + class*8` order). **The port's `GotoRowFor` is right.**

**The values are motion-row numbers, 1361..1388** (`0x551..0x56C`), which are *rows* of the
clip table, **not** player slots. The `row < 0x16` bound that appears elsewhere in these
functions (`CMP EAX,0x15`) is a bound on the **anime player's slot** (the interface id), not on
the row.

---

## §2 (Item 2) The column: the bucket index and its source word

**The column table: `DAT_1425DD988`** — script-read, 10 int32 entries:

| idx | VA | value | | idx | VA | value |
|---|---|---|---|---|---|---|
| 0 | `0x1425dd988` | 9 | | 5 | `0x1425dd99c` | 8 |
| 1 | `0x1425dd98c` | 11 | | 6 | `0x1425dd9a0` | 6 |
| 2 | `0x1425dd990` | 13 | | 7 | `0x1425dd9a4` | 4 |
| 3 | `0x1425dd994` | 15 | | 8 | `0x1425dd9a8` | 2 |
| 4 | `0x1425dd998` | 17 | | 9 | `0x1425dd9ac` | 0 |

(Entries 10-13 at `0x1425DD9B0`.. are `0x52D`, `0x52D`, `0x52D`, `0x52E` — the *style* table the
step code also reads; they are outside the 10-entry bucket range.)

**=> the column table maps the 10-way bucket to a clip column: `9, 11, 13, 15, 17, 8, 6, 4, 2, 0`.**
Note the order: bucket 0 → column 9, bucket 1 → 11, … bucket 4 → 17, bucket 5 → 8, bucket 6 → 6,
bucket 7 → 4, bucket 8 → 2, bucket 9 → 0.

**The source word differs between the two functions:**

| site | the read | the word |
|---|---|---|
| `FUN_1406D6780` (the RUN_TURN_RUN entry) | `0x1406D6C31 MOVSXD RAX,dword ptr [RSI + 0x20]` → `0x1406D6C43 MOV R13D,[RCX + RAX*0x4 + 0x25dd988]` | `[RSI+0x20]` = **word 8** = `kMoveBucket` |
| `FUN_1406CE820` (the swing) | `0x1406CEF62 MOVSXD RCX,dword ptr [RSI + 0x30]` → `0x1406CEF6E MOV R15D,[RDI + RCX*0x4 + 0x25dd988]` | `[RSI+0x30]` = **word 0x0C** = `kLookBucket` |

So: **the entry's column is `DAT_1425DD988[req[8]]` (the move bucket) and the swing's column is
`DAT_1425DD988[req[0x0C]]` (the look bucket).** CONFIRMED by the two reads above.

**Is there a `& 7`? NO.** The index is used raw:
- the entry: `MOVSXD RAX,dword ptr [RSI + 0x20]` then `MOV R13D,[RCX + RAX*0x4 + 0x25dd988]`
  — a full sign-extended 32-bit index, no masking;
- the swing: `MOVSXD RCX,dword ptr [RSI + 0x30]` then `MOV R15D,[RDI + RCX*0x4 + 0x25dd988]`
  — the same.

The bound is **not** a mask but the *cell* check's column range: `CMP EDX,0x11` in
`FUN_1406ABDD0` (column ≤ 17) and `CMP EDX,0x12` in `FUN_1406ABB30` (column < 18). Since the
column table's values are 0, 2, 4, 6, 8, 9, 11, 13, 15, 17, all of them are inside that range.

**But the *bucket* index into the column table is not range-checked either** — a bucket outside
0..9 would read past the table. The port's `AngleBucket` returns 0..9 by construction, and PES's
`FUN_1406B9B20` returns a class id in 0..9, so the index is in range in practice.

---

## §3 (Item 3) The empty-cell rule

**`FUN_1406ABDD0(table, &row, &col)` — "is it populated?"**

| condition | result |
|---|---|
| `row >= 0xBE5` | **0** (empty) |
| `col >= 0x12` | **0** (empty) |
| `row == 0x7C5 && col == 2` | **1** (always populated) |
| otherwise | `(cell >> 15) ^ 1` — **1** when bit 15 is clear |

**`FUN_1406ABB30(table, &row, &col, flag)` — the motion number**

| condition | result |
|---|---|
| `row == 0x7C5 && col == 2` | the fixed `0x9B4` (2484) |
| `table == 0`, or `row < 0`, or `row >= 0xBE5`, or `col < 0`, or `col >= 0x12` | **−1** |
| the cell's bit 15 is clear | the 16-bit cell value (the motion number) |
| the cell's bit 15 is set (empty) | **−1** |

**So: an empty cell gives `FUN_1406ABDD0 → 0` and `FUN_1406ABB30 → −1`.**
The port's "−1 means no clip" is **correct**. CONFIRMED.

**What the caller does with an empty cell.** In the entry `FUN_1406D6780` the pattern is
(three sites, at `0x1406D75C1`/`0x1406D7C7E`/`0x1406D8320`):

```
1406D75B3: LEA RDX,[RBP + 0x98]      ; &column
1406D75BA: MOV dword ptr [RBP + 0x98],R14D
1406D75C1: CALL 0x1406abdd0          ; is the cell populated?
1406D75C6: TEST AL,AL
1406D75C8: JZ 0x1406d8523            ; EMPTY -> bail out
...
1406D8523: FUN_1406d2590(local_res8,param_2)   ; the TURN_STOP handler
```

**=> when the row × column cell is empty, the entry does NOT pick another row and does NOT
pick another column: it falls through to `LAB_1406D8523`, which calls `FUN_1406D2590` — the
TURN_STOP handler.** There is no retry and no fallback row at these three sites.

**Where a different row *is* chosen**, it is chosen **before** the lookup, by the row
computation itself — the `switch` on the bucket at `0x1406D7CAC` and the `uVar23` ladder
(`0x5B3`, `0x5B9`, `0x5BC`, `0x5BB`, `0x5B5`, `0x5BE`, `0x5BF`, `0x5C0`, `0x5C1`, `0x5C2`,
`0x5C3`, `0x5C5`, `0x5CC`, `0x5D0`, `0x5D1`, `0x5D2`, `0x5D3`, `0x5D4`, `0x5D5`, `0xBE5`) — not by
retrying the cell lookup.

**One more guard on the column: the bucket remap.** At `0x1406D7CAC` the entry remaps the
column *bucket* before the lookup, when the row is in the "turn" family:

```
1406D7CAC: if (((uVar23 - 0x5bf & 0xfffffff9) == 0) && (uVar23 != 0x5c5) &&
                ((float)param_2[0x11] <= fVar34)) {
   switch(uVar10) {                 // uVar10 = the bucket
   case 2:   uVar10 = 0;  break;
   case 4:   uVar10 = 2;  break;
   case 6:   uVar10 = 4;  break;
   case 0xb: uVar10 = 0xd; break;
   case 0xd: uVar10 = 0xf; break;
   case 0xf: uVar10 = 0x11;
   }
}
```

**=> the bucket is remapped `2→0, 4→2, 6→4, 11→13, 13→15, 15→17`** (a shift down by two
buckets, i.e. by 90° of turn), under the condition that the row is one of
`0x5BF, 0x5C0, 0x5C1, 0x5C3, 0x5C4, 0x5CC` (the `(row - 0x5BF) & ~6 == 0` test, excluding
`0x5C5`) **and** `req[0x11] <= fVar34`. `req[0x11]` is **word 0x11** = `kDistToTarget`
(`movestep.hpp:138`).

Note this remap is applied to the **bucket**, *before* it indexes `DAT_1425DD988` — so the
remapped bucket then goes through the column table like any other.

---

## §4 (Item 4) The swing rows

`FUN_1406CE820` is the **GOTO_RUN / swing** entry. Its row and column are computed at
`0x1406CEF66`/`0x1406CEF6E` (see §1 and §2), and then **overridden** in three places.

**The default** (CONFIRMED, `0x1406CEF66`): `R13D = DAT_1425DDA50[req[0] + req[9]*8]` — the
same `GotoRowFor` table and the same index as the entry.

**Override 1 — row `0x572` (1394).** At `0x1406CEF78`, taken when the "swing" flag was set:

```
1406CEF35: MOV byte ptr [RBP + 0xb8],0x1     ; the swing flag
1406CEF3C: MOV byte ptr [RSP + 0x40],0x1
...
1406CEF46: CMP byte ptr [RSP + 0x40],0x0
...
1406CEF76: JZ 0x1406cef83
1406CEF78: MOV R13D,0x572                    ; the row becomes 0x572
1406CEF7E: JMP 0x1406cf059
```

The flag is set at `0x1406CEF35` when the swing's angle test fails — `UCOMISS XMM1,XMM0` at
`0x1406CEF12` with the two compared angles being `req[0x14]` (word 5, `kLookDelta`) and the
target's. So **row `0x572` is the "the swing's own angle does not match, use the swing row"**
row.

**Override 2 — row `0x566` (1382) and a column change.** At `0x1406CEF83`:

```
1406CEF83: CMP byte ptr [RSI + 0x3f],0x0    ; req byte 0x3F
1406CEF87: JZ 0x1406cefa8                   ;   0 -> no override
1406CEF89: LEA EAX,[R15 + -0x8]             ; column - 8
1406CEF8D: MOV R13D,0x566                   ; the row becomes 0x566
1406CEF93: CMP EAX,0x1
1406CEF96: JA 0x1406cefa8                   ;   column-8 > 1 -> no column change
1406CEF98: COMISS XMM8,dword ptr [RSI + 0x1c]
1406CEF9D: MOV R15D,R12D
1406CEFA0: SETA R15B
1406CEFA4: ADD R15D,0x8                     ; the column becomes 8 or 9
```

**=> when `req` byte `0x3F` is non-zero, the row becomes `0x566`; and when the column is 8 or
9 (column − 8 ≤ 1), the column is replaced by `8` or `9` according to the comparison
`XMM8 > req[0x1C]` (word 7, `kFacingError`) — `SETA` gives 1, so the column is 9 when
`XMM8 > req[7]` and 8 otherwise.**

**Override 3 — the row window: a RANGE *and* a BITMASK.** At `0x1406CEFBA`:

```
1406CEFBA: LEA EAX,[R13 + -0x561]      ; 41 8D 85 9F FA FF FF  (the disp32 is 0xFFFFFF9F)
1406CEFC1: CMP EAX,0x5
1406CEFC4: JA 0x1406cf059               ; row - 0x561 > 5 -> skip the pick
1406CEFCA: MOV ECX,0x29
1406CEFCF: BT ECX,EAX
1406CEFD2: JNC 0x1406cf059              ; the bit is clear -> skip the pick
```

**=> the swing picks a clip only when the row is in `0x561..0x566` (1377..1382) *and* the bit
`(row - 0x561)` is set in the mask `0x29` (`0b101001`).** The mask's set bits are 0, 3 and 5, so
**the row must be one of `0x561` (1377), `0x564` (1380) or `0x566` (1382)**; every other row in
the range is skipped, as is every row outside it. (The `LEA`'s displacement is a **disp32**,
`0xFFFFFF9F` = −0x561 — read as a single byte it would look like −0x61, which is wrong.)

**The other row constants in `FUN_1406CE820`** (all CONFIRMED from the raw dis):

| VA | row | when |
|---|---|---|
| `0x1406CEF78` | `0x572` (1394) | the swing-angle flag is set |
| `0x1406CEF8D` | `0x566` (1382) | `req` byte `0x3F` is non-zero |
| `0x1406CF046` | `0x565` (1381) | `CMOVA R13D,EAX` — taken when `XMM6 > XMM0` (the random-vs-threshold test at `0x1406CF04B`) |
| `0x1406CF0F1` | `0x56F` (1391) | a later branch |
| `0x1406CF30B` | `0x570` (1392) | a later branch |
| `0x1406CF60D` | `0x571` (1393) | a later branch |
| `0x1406CF962` | `0x573` (1395) | a later branch |
| `0x1406CFA2A` | `0x575` (1397) | a later branch |

**`GotoRowFor`'s table `0x1425DDA50` vs the swing rows.** The port's `GotoRowFor`
(`movestep_run.cpp:323`) returns `t.gotoRow[kind + cls*8]`, i.e. a value from
`DAT_1425DDA50` — the values **1361..1388** (`0x551..0x56C`). The swing's constant rows are
`0x566` = 1382**, `0x56F` = 1391, `0x570` = 1392, `0x571` = 1393, `0x572` = 1394,
`0x573` = 1395, `0x575` = 1397 — **none of them is a value in the 32-entry `DAT_1425DDA50`
table**, so the swing's constant rows are **hard-coded rows**, not table lookups. `0x566` =
1382 is inside the *range* of the table's values but is not one of them.
Of the swing's hard-coded rows, only **`0x566` (1382)** passes the window test — `0x562`,
`0x563` and `0x565` are in the range but their mask bits are clear, and the rest are outside
the range entirely.

**=> the port's `GotoRowFor` is the *default* row only. The swing's rows `0x566` and `0x572`
are hard-coded overrides that the port must add as constants.**

---

## §5 (Item 5) The swing column read: `DAT_1425DD988[req[0x0C]]`

**CONFIRMED, and with a twist: the swing *writes* `req[0x0C]` before it reads it.**

```
1406CE952: MOV EDI,0x9                       ; EDI = 9
1406CE97F: LEA R15D,[RDI + -0x5]             ; R15D = 9 - 5 = 4
...
1406CEB18: MOV ECX,dword ptr [RSI + 0x30]    ; the current req[0x0C]
1406CEB1B: CMP ECX,R15D
1406CEB1E: JNZ 0x1406ceb2a
1406CEB20: MOVSS XMM0,dword ptr [0x1425ddffc]  ; -180.0
1406CEB28: JMP 0x1406ceb2e
1406CEB2A: MOVAPS XMM0,XMM15                 ; 180.0
1406CEB2E: CMP ECX,R15D                      ; ZF set iff req[0x0C] == 4
1406CEB31: MOVSS dword ptr [RSI + 0x14],XMM0  ; req[5] = (req[0x0C]==4) ? -180 : 180
1406CEB36: MOV EAX,R15D                      ; EAX = 4
1406CEB39: MOV byte ptr [RBP + 0xb8],0x1
1406CEB40: CMOVZ EAX,EDI                      ; if (req[0x0C] == 4) EAX = 9
1406CEB43: MOV dword ptr [RSI + 0x30],EAX     ; req[0x0C] = 4, or 9 when it already was 4
```

**=> the swing sets `req[0x0C]` (word `0x0C` = `kLookBucket`) to `4`, or to `9` if it was
already `4`; and it sets `req[5]` (`kLookDelta`) to `-180.0` when the old column was `4`,
else `+180.0`.** The two constants are script-read: `0x1425DDFFC` = `-180.0`, and `XMM15` =
`DAT_142598580` = `180.0`.

**The gate on that write** (the `bVar22` condition ClipWords asked about):

```
1406CE9C0: CMP EAX,R15D                      ; EAX = the swing's own column (4 or 9)
1406CE9C3: JZ 0x1406ce9cd
1406CE9C5: CMP EAX,EDI                       ; vs 9
1406CE9C7: JNZ 0x1406ceb57                   ; neither 4 nor 9 -> skip the write
```

**=> the swing's column write happens only when the swing's own column is 4 or 9.**

**Then the read**, at `0x1406CEF62`/`0x1406CEF6E`:

```
1406CEF62: MOVSXD RCX,dword ptr [RSI + 0x30]              ; the (possibly just written) req[0x0C]
1406CEF6E: MOV R15D,dword ptr [RDI + RCX*0x4 + 0x25dd988] ; the column
```

**=> the swing's column is `DAT_1425DD988[req[0x0C]]`, read *after* the swing's own
column write.** With the write in place, the index is `4` or `9`, so the column is
`DAT_1425DD988[4] = 17` or `DAT_1425DD988[9] = 0`.

**The table's size.** `DAT_1425DD988` is a **10-entry** bucket→column table
(0x1425DD988..0x1425DD9AC, 40 bytes). The bytes after it (`0x1425DD9B0` = `0x52D`,
`0x1425DD9B4` = `0x52D`, `0x1425DD9B8` = `0x52D`, `0x1425DD9BC` = `0x52E`) belong to the
*style* table the step code reads (`DAT_1425DD9B0` is `STEP_GRID_TABLE_VA` in the port,
`movestep.cpp:190`), so they are **not** bucket-table entries.

**The `req` byte `0x3D` condition** (ClipWords' other question): at `0x1406CEB57`,

```
1406CEB57: CMP byte ptr [RSI + 0x3d],0x0
1406CEB5B: JNZ 0x1406cef41        ; req byte 0x3D set -> skip the whole swing-row block
1406CEB61: CMP byte ptr [RSI + 0x3e],0x0
1406CEB65: JNZ 0x1406cef41        ; req byte 0x3E set -> skip it too
1406CEB6B: MOVSS XMM0,dword ptr [RSI + 0x14]   ; req[5] (kLookDelta)
1406CEB7B: COMISS XMM0,XMM1                    ; vs 135.0 (DAT_14259BE60)
1406CEB7E: JBE 0x1406cef41                     ; |lookDelta| <= 135 -> skip
1406CEB84: CMP dword ptr [RSI + 0x24],0x1      ; req[9] (kCommandSpeedClass)
1406CEB88: JLE 0x1406cef41                     ; class <= 1 -> skip
1406CEB8E: MOV EAX,dword ptr [RSI + 0x30]      ; req[0x0C]
1406CEB91: CMP EAX,0x3
1406CEB94: JZ 0x1406ceb9f
1406CEB96: CMP EAX,0x8
1406CEB99: JNZ 0x1406cef41                     ; not 3 and not 8 -> skip
```

**=> the swing's own row/column work is gated by: `req` byte `0x3D == 0` AND byte `0x3E == 0`
AND `|req[5]| > 135.0` AND `req[9] > 1` AND `req[0x0C] ∈ {3, 8}`.**
The byte `0x3D` is at request offset `0x3D` = word `0x0F` = `kPrecise`
(`movestep.hpp:137`); byte `0x3E` is at offset `0x3E`, which is the low byte of word `0x0F`+1
= word `0x0F`… — `0x3D` is the low byte of word `0x0F` and `0x3E` the next byte, so
`0x3D`/`0x3E` are **two separate flag bytes** at request offsets `0x3D` and `0x3E`
(the port has no name for `0x3E`).

---

## §6 The rules, in one place

1. **Row** = `DAT_1425DDA50[req[0] + req[9]*8]` (the port's `GotoRowFor`), with hard-coded
   overrides in the swing: `0x572` (the swing-angle flag), `0x566` (`req` byte `0x3F` set),
   `0x565` (the random-vs-threshold test), and `0x56F`/`0x570`/`0x571`/`0x573`/`0x575` in the
   later branches. The swing only picks a clip when the row is in `0x561..0x566`.
2. **Column** = `DAT_1425DD988[bucket]`, where the bucket is **word 8** (`kMoveBucket`) in the
   RUN_TURN_RUN entry and **word 0x0C** (`kLookBucket`) in the swing. The table is
   `9, 11, 13, 15, 17, 8, 6, 4, 2, 0`. **No `& 7` mask** — the index is the raw word; the only
   range check is the cell's own `col < 0x12`.
3. **Empty cell** = the 16-bit cell's **bit 15 set**. `FUN_1406ABDD0` returns **0**,
   `FUN_1406ABB30` returns **−1**. The caller does **not** retry: it falls through to
   `LAB_1406D8523` → `FUN_1406D2590` (TURN_STOP). The port's "−1 means no clip" is right.
4. **Swing rows** = `GotoRowFor`'s table for the default, plus the hard-coded `0x566`/`0x572`
   overrides and the `0x561..0x566` window.
5. **Swing column** = `DAT_1425DD988[req[0x0C]]`, read after the swing has written
   `req[0x0C] = 4` (or `9` when it was already `4`) and set `req[5]` to `∓180.0`.

---

## §7 Open items

1. **The anime player's slot vs the clip row.** `FUN_140A62960` returns `index + n*0xB` (a slot)
   and `FUN_140A627E0` returns a player's field or `0xFF`. The port's stand-in for the anime
   player is 0. The **slot** and the **row** are different things, and this report decodes the
   row. **What the anime player's slot selects** (it picks which of the eleven entries of the
   `0xA2A0` table to use, and `FUN_140A62960` is called with the *interface*'s value) is
   **OPEN** — the `0xA2A0` table is populated at runtime and its contents are not in the image.
2. **The `0x3E` request byte** has no name in the port's `moveword` header.
3. **The later swing rows** `0x56F`/`0x570`/`0x571`/`0x573`/`0x575` were located but their
   **branch conditions were not reduced** in this pass — only the two overrides the port needs
   (`0x566` and `0x572`) and the `0x561..0x566` window are fully decoded.
4. **The `FUN_1406B9BD0` row table's other users**: `DAT_1425DDA50` is read by the port's
   `GotoRowFor`; the swing's hard-coded rows do not appear in that table (1382, 1391..1395 and
   1397 are not among its 1361..1388).

---

## §8 Method

- Bridge decompiles (`dec/FUN_*.c`) and disassembly (`dec/dis_*.txt`) for `FUN_1406D6780`,
  `FUN_1406CE820`, `FUN_1406ABDD0`, `FUN_1406ABB30`, `FUN_1406ABD60`, `FUN_1406BA7D0`,
  `FUN_140A627E0`, `FUN_140A62960`, `FUN_1406ABBA0`, `FUN_1406D2590`.
- Every table (`DAT_1425DDA50`, `DAT_1425DD988`, `DAT_1436F2378`) is script-read from the
  image; no value is typed by hand.
- Every operand pair quoted above is checked by
  `<workdrive>/gf-TeamDecode-scratch/check_runtturn.py`:
  ```
  -> 42 table rows checked
  -> the row table's 32 entries checked
  -> the clip table's pointer checked
  -> the three helpers' operand encodings checked
  -> the swing's column write and its constants checked
  -> the swing row constants and the 0x561..0x566 window checked
  -> the two column-table reads and the row-table read checked
  -> 18 textual claims checked
  cited code VAs not inside a disassembled instruction: 0
  TOTAL mismatches: 0
  ```
  **Mutation-tested — all seventeen fire:** a wrong column-table value; a wrong row-table
  value; the clip table's pointer; the `row * 9` stride; the special cell's fixed motion; the
  mirror byte's stride; the column bound; the row bound; the empty cell's `-1`; the entry's
  column source; the swing's column source; the `0x29` mask; the window's allowed rows; the
  empty cell's fallback handler; the swing's column write; the "the swing rows are hard-coded"
  claim; and the "no `& 7` mask" claim.
- The port's own `movestep.hpp` / `movestep_run.cpp` were read to map the request word indices
  and to compare `GotoRowFor` and `Cell`.
- No GF code changed, no GF build/test run, no PES write/pin/input.
