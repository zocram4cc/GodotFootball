# PES RUN_TURN_RUN's clip pick: the row, the column, the empty-cell rule, and the swing rows

> **Main audit 20-10-26:** the column table `0x1425DD988` = 9, 11, 13, 15, 17, 8, 6, 4, 2, 0; the row table `0x1425DDA50` = 32 entries 1361..1388; the swing window `0x1406CEFBA` = `lea eax,[r13-0x561]; cmp eax,5; ja; mov ecx,0x29; bt ecx,eax` (range plus bitmask: rows 0x561, 0x564, 0x566 pass): all match.
> **Main audit, round 2:** `0x1406CF30B` = `mov r13d,0x570` (the 0x56F block's fall-through); `0x1406D7CBE` = `cmp r14d,0x5c5` (the explicit exclusion after the 0xFFFFFFF9 mask): match. The remap list and row list were corrected this round (ClipWords' corrections, confirmed).
> **Main audit, round 3 (21-10-26):** vt+0xD0 of the +0xAC0 vtable (`0x1425E35B0`+0xD0) = `0x1406B9D80`: match. **Unit correction:** FUN_14042D470 = ticks·60·60/1000·|displacement| is km/h (the port's GroundSpeedKmh), not m/s; the swing rows' 16.0 is 16 km/h.
> **Main audit, round 4 (21-10-26):** §5g's band walk (0x1406C4DCC-DEB) is accepted. **Its claim that the install's PES2021.exe is a hot-patched build with field offsets shifted by +0x3B00 is FALSE:** the install's exe (PE-mapped) and pes_full.bin carry identical bytes at 0x1406C4DCC (`8b6f7c…`, mov ebp,[rdi+0x7c]), 0x1406C142A and the table 0x1425DD9B0; live reads of +0x245C validate on the install's process.
> **Main note:** TeamDecode retracted the "modded exe" claim itself (a VA-to-file-offset map applied to the wrong file); §5g now verifies the walk against the install's own bytes.
> **Main audit, round 5 (22-10-26):** §5g's two-path rule is Main-confirmed from the bytes (0x1406C4D63 `mov ecx,2`; the special path's entry at 0x1406C4DAE..DCA): band = live + sgn(E − live), E = clamp(req[9], 0..2), or clamp(req[0x1F], 0..2) on the special path. §5h (the vt+0 issuer FUN_1406BA2F0, which returns a style code) is TeamDecode2's, not yet Main-audited.

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

**One more guard on the column: the column remap.** At `0x1406D7CAC` the entry remaps the
**column** before the lookup, when the row is in the "turn" family. (The first draft of this
report said the *bucket* was remapped — that was wrong; see §5b's correction (a) for the
listing and the jump table.) The listing:

```
1406D7CAC: LEA EAX,[R14 + -0x5bf]      ; the ROW
1406D7CB3: TEST EAX,0xfffffff9         ; the mask: 0x5BF / 0x5C1 / 0x5C3 / 0x5C5 / ...
1406D7CB8: JNZ 0x1406d7d5e
1406D7CBE: CMP R14D,0x5c5               ; ... excluding 0x5C5
1406D7CC5: JZ 0x1406d7d5e
1406D7CCB: COMISS XMM12,dword ptr [RSI + 0x44]
1406D7CD0: JC 0x1406d7d5e
1406D7CD6: LEA EAX,[R13 + -0x2]         ; the COLUMN - 2
1406D7CDA: CMP EAX,0xd
1406D7CDD: JA 0x1406d7d5e
1406D7CE8: MOV ECX,dword ptr [RDX + RAX*0x4 + 0x6d85e4]   ; the jump table
1406D7CF2: JMP RCX
```

**=> the column is remapped through the jump table at `0x1406D85E4`, indexed by `column - 2`**,
for the columns **2, 4, 6, 11, 13 and 15**; every other column in 2..15 is left alone. The
remap applies only when the row is one of **`0x5BF`, `0x5C1`, `0x5C3`** (the mask
`0xFFFFFFF9` admits `0x5BF`/`0x5C1`/`0x5C3`/`0x5C5`/…, and the separate `CMP R14D,0x5C5`
excludes `0x5C5`), and when `req[0x44]` (`kDistToTarget`) passes the `XMM12` comparison.
§5b lists the jump table's fourteen script-read entries.

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

## §5b The later swing rows `0x56F`, `0x570`, `0x571`, `0x573`, `0x575` — their conditions

*(Main's follow-up; ClipWords asked for these. All CONFIRMED from the raw dis of
`FUN_1406CE820`.)*

**First, two corrections to this report, both from ClipWords and both confirmed against the
listing:**

**(a) The turn-family remap acts on the COLUMN, not on the bucket.** The first draft of this
report said the *bucket* was remapped before the lookup. The listing shows the switch is on
**`R13D`, which by then holds `DAT_1425DD988[req[8]]` — the COLUMN**:

```
1406D6C31: MOVSXD RAX,dword ptr [RSI + 0x20]   ; req[8]
1406D6C43: MOV R13D,dword ptr [RCX + RAX*0x4 + 0x25dd988]   ; the COLUMN
...
1406D7CAC: LEA EAX,[R14 + -0x5bf]      ; the ROW
1406D7CB3: TEST EAX,0xfffffff9
1406D7CB8: JNZ 0x1406d7d5e
1406D7CBE: CMP R14D,0x5c5
1406D7CC5: JZ 0x1406d7d5e
1406D7CCB: COMISS XMM12,dword ptr [RSI + 0x44]
1406D7CD0: JC 0x1406d7d5e
1406D7CD6: LEA EAX,[R13 + -0x2]        ; the COLUMN - 2
1406D7CDA: CMP EAX,0xd
1406D7CDD: JA 0x1406d7d5e
1406D7CDF: LEA RDX,[0x140000000]
1406D7CE6: CDQE
1406D7CE8: MOV ECX,dword ptr [RDX + RAX*0x4 + 0x6d85e4]   ; the jump table
1406D7CEF: ADD RCX,RDX
1406D7CF2: JMP RCX
```

The remap is a **jump table at `0x1406D85E4`**, indexed by `column - 2` (0..13). Script-read, the
fourteen entries are:

| column − 2 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| target | `0x1406D7D5B` | `0x1406D7D5E` | `0x1406D7D53` | `0x1406D7D5E` | `0x1406D7D4B` | `0x1406D7D5E` | `0x1406D7D5E` | `0x1406D7D5E` | `0x1406D7D5E` | `0x1406D7D33` | `0x1406D7D5E` | `0x1406D7D3B` | `0x1406D7D5E` | `0x1406D7D43` |

**=> the remapped columns are 2→`0x1406D7D5B`, 4→`0x1406D7D53`, 6→`0x1406D7D4B`,
11→`0x1406D7D33`, 13→`0x1406D7D3B`, 15→`0x1406D7D43`**; every other column in 2..15 goes to
`0x1406D7D5E` (no remap). The first draft's "2→0, 4→2, 6→4, 11→13, 13→15, 15→17" was the
decompiler's rendering of the *old* switch and is **retracted**; the values above are the
jump table's real targets.

**(b) The row list is `0x5BF`/`0x5C1`/`0x5C3` only.** The mask is `TEST EAX,0xFFFFFFF9` with
`EAX = row - 0x5BF`; `0xFFFFFFF9` clears bits 0 and 1, so the test passes for `row - 0x5BF` in
`{0, 2, 4, 6, ...}` — that is rows `0x5BF`, `0x5C1`, `0x5C3`, `0x5C5` and every other even
offset. The **separate** `CMP R14D,0x5C5` / `JZ` at `0x1406D7CBE` then excludes `0x5C5`.
**=> the effective list is exactly `0x5BF`, `0x5C1`, `0x5C3`** — the first draft's
"`0x5BF, 0x5C0, 0x5C1, 0x5C3, 0x5C4, 0x5CC`" was wrong on all three excluded rows.
(Checked arithmetically: `0x5C0`→1, `0x5C2`→3, `0x5C4`→5 and `0x5CC`→13 all have a cleared bit
and fail the mask; `0x5C5`→6 passes the mask but is caught by the explicit compare.)

**The remap's other two gates** (besides the row list): `COMISS XMM12,[RSI+0x44]` / `JC` — the
`req[0x44]` word (byte `0x44` = word `0x11` = `kDistToTarget`) against `XMM12` — and
`req[0x11] <= fVar34` in the decompile's rendering. The jump table's six remapped columns are
the ones the decompile's `switch` showed as `case 2/4/6/0xb/0xd/0xf`; the **column** values
are 2, 4, 6, 11, 13, 15 (the `column - 2` indices 0, 2, 4, 9, 11, 13).

---

### §5b.1 `0x575` (1397) — the RUN_LOOP EnterAccelerate stand-in

**This is the row ClipWords' throw-in stall needs.** The gate is at `0x1406CFA2A`, and the row
is taken when **all** of the following hold (the block is skipped — to `0x1406CFA86` — otherwise):

```
1406CF9B6: MOVSS XMM0,dword ptr [0x1425a3590]      ; 1.1
1406CF9BE: COMISS XMM0,dword ptr [RBP + 0xb8]      ; (1) 1.1 >  the local at [RBP+0xB8]
1406CF9C5: JBE 0x1406cfa86
1406CF9CB: COMISS XMM11,XMM8                       ; (2) XMM11 > 0.0
1406CF9CF: JBE 0x1406cfa86
1406CF9D5: CVTPS2PD XMM0,XMM6
1406CF9D8: ANDPS XMM0,XMM9                         ; |XMM6|
1406CF9DC: CVTPD2PS XMM1,XMM0
1406CF9E0: COMISS XMM14,XMM1                       ; (3) 0.8 > |XMM6|
1406CF9E4: JBE 0x1406cfa86
1406CF9EA: COMISS XMM13,XMM12                       ; (4) 67.5 > XMM12
1406CF9EE: JBE 0x1406cfa86
1406CF9F4: COMISS XMM7,XMM15                        ; (5) 16.0 > XMM15
1406CF9F8: JBE 0x1406cfa86
1406CF9FE: LEA EAX,[R15 + -0xd]
1406CFA02: TEST EAX,0xfffffffd                     ; (6) the column is 0xD or 0xF
1406CFA07: JZ 0x1406cfa1d
1406CFA09: LEA EAX,[R15 + -0x2]
1406CFA0D: TEST EAX,0xfffffffd                     ;   or the column is 0x2 or 0x4
1406CFA12: JNZ 0x1406cfa86
1406CFA14: COMISS XMM6,dword ptr [0x142588200]      ;   then |XMM6| vs +0.1
1406CFA1B: JMP 0x1406cfa28
1406CFA1D: MOVSS XMM0,dword ptr [0x14259d648]      ;   or -0.1 vs XMM6
1406CFA25: COMISS XMM0,XMM6
1406CFA28: JBE 0x1406cfa86
1406CFA2A: MOV R13D,0x575
```

**The register meanings** (all traced in this pass):

| register | value | where it comes from |
|---|---|---|
| `R15D` | the **column** | `DAT_1425DD988[req[8]]` (or the remapped one) |
| `XMM8` | `0.0` | `XORPS XMM8,XMM8` at `0x1406CE960` |
| `XMM11` | a displacement | `[RSP+0x68]`, loaded at `0x1406CF8C1`; built at `0x1406CF81B` as `pos.z - target.z` (`[RBP-0x70] - [RBP-0x60]`) |
| `XMM6` | a displacement | `[RSP+0x60]`, loaded at `0x1406CF8C8`; built at `0x1406CF7F9` as `pos.x - target.x` |
| `XMM12` | `|req[1]|` (the heading) | `0x1406CF85D CVTPD2PS XMM12,XMM1` from `req[1]` |
| `XMM13` | `67.5` | `0x1425CCA34` |
| `XMM14` | `0.8` | `0x142572348` |
| `XMM15` | a speed-ish value | `0x1406CF8BD MOVAPS XMM15,XMM0` from `FUN_14042D470` |
| `XMM7` | `16.0` | `0x14259BE10` |
| `[RBP+0xB8]` | a float | `0x1406CF808 MOVSS dword ptr [RBP+0xb8],XMM0` — `FUN_140A8F860`'s return |

**=> the `0x575` rule**: take the row `0x575` when
`1.1 > [RBP+0xB8]` **and** `XMM11 > 0` **and** `|XMM6| < 0.8` **and** `|req[1]| < 67.5` **and**
`XMM15 < 16.0` **and** (the column is `0xD` or `0xF` **and** `XMM6 < −0.1`) **or**
(the column is `0x2` or `0x4` **and** `XMM6 > +0.1`).

(The `(6)` sub-test's `TEST EAX,0xFFFFFFFD` clears bit 1, so `R15D - 0xD == 0` admits `0xD` and
`0xF`, and `R15D - 0x2 == 0` admits `0x2` and `0x4`. The final `COMISS` is a strict
comparison in each direction, so the two branches are `XMM6 < −0.1` and `XMM6 > +0.1`
respectively.)

### §5b.2 `0x573` (1395)

The same five gates as `0x575`, then a **column** test, at `0x1406CF962`:

```
1406CF8AC: COMISS XMM9,dword ptr [RBP + 0xb8]      ; (1) 0.7 >  the local at [RBP+0xB8]
1406CF8E0: JBE 0x1406cf9b1
1406CF8E6: COMISS XMM11,XMM8                       ; (2) XMM11 > 0.0
1406CF8EA: JBE 0x1406cf9b1
1406CF8F0: MOVSS XMM0,dword ptr [0x142565600]      ; 0.5
1406CF903: COMISS XMM0,XMM2                        ; (3) 0.5 > |XMM6|
1406CF906: JBE 0x1406cf9b1
1406CF90C: COMISS XMM13,XMM12                       ; (4) 67.5 > XMM12
1406CF910: JBE 0x1406cf9b1
1406CF916: COMISS XMM7,XMM15                        ; (5) XMM7 > XMM15
1406CF91A: JBE 0x1406cf9b1
1406CF920: CMP R15D,0xd
1406CF924: JZ 0x1406cf949
1406CF926: CMP R15D,0x4
1406CF92A: JZ 0x1406cf949
1406CF92C: CMP R15D,0xf
1406CF930: JZ 0x1406cf956
1406CF932: CMP R15D,0x2
1406CF936: JZ 0x1406cf956
1406CF938: CMP R15D,0x11
1406CF93C: JZ 0x1406cf943
1406CF93E: TEST R15D,R15D
1406CF941: JNZ 0x1406cf9b1                          ; the column must be 0, 0xD, 0xF, 0x2, 0x4 or 0x11
1406CF943: COMISS XMM14,XMM2                        ; for the column 0/0x11: 0.8 > |XMM6|
1406CF947: JMP 0x1406cf954
1406CF949: MOVSS XMM0,dword ptr [0x142590d98]      ; for the column 0xD/0x4: 0.3 > |XMM6|
1406CF951: COMISS XMM0,XMM2
1406CF954: JBE 0x1406cf9b1
1406CF956: ...
1406CF962: MOV R13D,0x573
```

**=> the `0x573` rule**: take the row `0x573` when `0.7 > [RBP+0xB8]` **and** `XMM11 > 0` **and**
`|XMM6| < 0.5` **and** `|req[1]| < 67.5` **and** `XMM7 > XMM15` **and** the column is one of
`0`, `0x2`, `0x4`, `0xD`, `0xF`, `0x11`, **and** (the column is `0` or `0x11` and `|XMM6| < 0.8`)
or (the column is `0x4` or `0xD` and `|XMM6| < 0.3`).

### §5b.3 `0x56F` (1391) and `0x570` (1392)

**`0x570` is the FALL-THROUGH of the `0x56F` block**, not a row with a condition of its own:
every path that declines the `0x56F` row jumps to `0x1406CF30B`, which is
`MOV R13D,0x570`. (The jumpers are `0x1406CF103`, `0x1406CF114`, `0x1406CF161`, `0x1406CF18E`
and `0x1406CF19C`.)

**`0x56F`'s condition** — the row is taken at `0x1406CF0F1` when:

```
1406CF0CB: MOVZX ECX,byte ptr [RSI + 0x3d]        ; req byte 0x3D
1406CF0CF: TEST CL,CL
1406CF0D1: JNZ 0x1406cf0dc
1406CF0D3: CMP byte ptr [RSI + 0x3e],CL           ; req byte 0x3E
1406CF0D6: JZ 0x1406cf384                        ;   both 0 -> the other path
1406CF0DC: CMP dword ptr [RSI + 0x24],0x1        ; req[9] (the class)
1406CF0E0: JLE 0x1406cf384                        ;   class <= 1 -> the other path
1406CF0E6: TEST CL,CL                             ; req byte 0x3D
1406CF0E8: JZ 0x1406cf0fc
1406CF0EA: CMP byte ptr [RSP + 0x40],0x0          ;   and the swing flag
1406CF0EF: JNZ 0x1406cf0fc
1406CF0F1: MOV R13D,0x56f
```

**=> the `0x56F` rule**: take the row `0x56F` when **at least one of the request bytes `0x3D`
or `0x3E` is non-zero**, **and** `req[9] > 1`, **and** (byte `0x3D` is zero **or** the swing
flag at `[RSP+0x40]` is zero).

(The `0x3D`/`0x3E` test is the same pair that gates the swing's own row/column work in §5;
`req[9]` is `kCommandSpeedClass`; the `[RSP+0x40]` byte is the swing flag set at
`0x1406CEF3C`.)

**The `0x570` row's own follow-on.** When `0x56F` is declined, `0x570` is used, and the code
then *rewrites* `req[0x0C]` and `req[5]` — the same 4-or-9 dance as §5, but on the **column**
index rather than the bucket:

```
1406CF2CF: MOV EAX,dword ptr [RSI + 0x30]          ; req[0x0C]
1406CF2D2: MOV ECX,0x4
1406CF2D7: SUB EAX,0x3
1406CF2DA: CMP EAX,0x1                            ; req[0x0C] in {3, 4}
1406CF2DD: MOV EAX,0x9
1406CF2E2: CMOVA EAX,ECX                          ;   -> 4, else 9
1406CF2E5: MOV dword ptr [RSI + 0x30],EAX
1406CF2E8: CMP EAX,ECX
1406CF2EA: JNZ 0x1406cf2f6
1406CF2EC: MOVSS XMM0,dword ptr [0x1425ddffc]     ;   req[0x0C] == 4 -> req[5] = -180.0
1406CF2F6: MOVAPS XMM0,XMM15                      ;   else req[5] = XMM15
1406CF2FA: MOVSS dword ptr [RSI + 0x14],XMM0
```

### §5b.4 `0x571` (1393)

Taken at `0x1406CF60D`:

```
1406CF5BF: COMISS XMM0,XMM6                        ; a magnitude test
1406CF5C2: JBE 0x1406cf683
1406CF5C8: MOV EAX,dword ptr [RSI + 0x24]          ; req[9] (the class)
1406CF5CB: MOV EDX,R12D
1406CF5CE: CMP EAX,0x3
1406CF5D1: SETZ DL                                ; DL = (req[9] == 3)
1406CF5D4: CMP byte ptr [RBP + 0xb8],0x0
1406CF5DB: JZ 0x1406cf5ed
1406CF5DD: CMP EAX,0x3
1406CF5E0: MOV EDX,0x14                           ; 20
1406CF5E5: MOV ECX,0x1e                           ; 30
1406CF5EA: CMOVZ EDX,ECX                          ;   req[9] == 3 -> 30, else 20
1406CF5ED: TEST DIL,DIL
1406CF5F0: JNZ 0x1406cf602
1406CF5F2: MOV RCX,RBX
1406CF5F5: CALL 0x1406ebf30                       ; the percentage roll
1406CF5FA: TEST AL,AL
1406CF5FC: JZ 0x1406cf683
1406CF602: ...
1406CF60D: MOV R13D,0x571
```

**=> the `0x571` rule**: take the row `0x571` when the magnitude test at `0x1406CF5BF` passes
**and** (`req[9] == 3` **or** the percentage roll `FUN_1406EBF30` with the percentage
`20` (or `30` when `req[9] == 3` and the swing flag at `[RBP+0xB8]` is set) succeeds.

---

## §5c The two unnamed swing-row gate inputs

*(Main's follow-up: the two §5b gates whose inputs had no name.)*

### §5c.1 `[RBP+0xB8]` = `FUN_140A8F860`'s return — the **distance to the target**

`FUN_140A8F860` (`0x140A8F860`) is a **2D distance** between two positions. Its body:

```
140A8F860: F3 0F 10 09              MOVSS XMM1,[RCX]        ; p1.x
140A8F864: F3 0F 10 41 08           MOVSS XMM0,[RCX+8]      ; p1.z
140A8F869: F3 0F 5C 42 08           MULSS XMM0,[RDX+8]      ; (p1.z - p2.z) * p1.z
140A8F86E: F3 0F 5C 0A              MULSS XMM1,[RDX]        ; p1.x * p2.x
140A8F872: F3 0F 59 C0              SUBSS XMM0,XMM0
140A8F876: F3 0F 59 C9              SUBSS XMM1,XMM1
140A8F87A: F3 0F 58 C1              SUBSS XMM0,XMM1
140A8F87E: E9 FD 1E B1 00           JMP 0x1415A1780         ; the vector length
```

`FUN_1415A1780` is the **vector length** — the port's own name for it
(`movestep_turn.cpp:73`: "FUN_1415a1780: the vector length"); it is an imported
`JMP qword ptr [0x1425301F0]`, i.e. the SSE square-root. So the function returns
**√(dx² + dz²)** — the distance in **metres** between the two XZ positions. (Only the `x`
and `z` components are used — `param_1[0]`/`param_1[2]` and `param_2[0]`/`param_2[2]` — so
the distance is planar; the `y` is ignored.)

**Note the sign convention.** The body computes `p1.z * p2.z` and `p1.x * p2.x` and then
subtracts, so it is the **difference of the products** — the caller passes the *already
negated* second position, which is why the call site loads the target's `+0x55C`/`+0x564`
and the player's `+0x08` and the sums come out right. The magnitude is what the gate reads.

**Its call site** (`0x1406CF7EA`, the one whose result lands in `[RBP+0xB8]` at
`0x1406CF808`):

```
1406CF7BC: LEA RDX,[RBP + -0x78]          ; the target position
1406CF7C0: MOVSD XMM0,qword ptr [RDI + 0x8]   ; the player's position (x, z)
1406CF7CB: MOVSD qword ptr [RBP + -0x68],XMM0
1406CF7D0: MOVSD XMM0,qword ptr [RAX + 0x55c] ; the target's x, z
1406CF7D8: MOVSD qword ptr [RBP + -0x78],XMM0
1406CF7DD: MOV ECX,dword ptr [RAX + 0x564]     ; the target's z
1406CF7E3: MOV dword ptr [RBP + -0x70],ECX
1406CF7E6: LEA RCX,[RBP + -0x68]           ; the player's position
1406CF7EA: CALL 0x140a8f860
```

**=> `[RBP+0xB8]` is the distance in metres from the player to the target position**
(the target's `+0x55C`/`+0x564` fields).** CONFIRMED.

**Its use in the `0x575`/`0x573` gates.** The gates compare it against `1.1` (the `0x575`
gate, `0x1425A3590` = 1.1) and `0.7` (the `0x573` gate, `0x14257A194` = 0.7). Both are
**distances in metres** — so the gates read:

- `0x575`: **the target is within 1.1 m** of the player.
- `0x573`: **the target is within 0.7 m** of the player.

**=> the `0x575` and `0x573` swing rows are the "the player is already at the target" rows** —
the foot-shuffle / feint rows played when there is no distance left to cover. That is what the
motion names say: `0x575`'s family is `feint`/`feintrun` (Neymar's list 0) and `0x573`'s is the
`kick_long_3_0_inside_y0_045_curve_*` family.

### §5c.2 `XMM15` = `FUN_14042D470`'s return — the **distance covered in one second**

```c
float FUN_14042d470(float param_1) {
  float fVar1;
  fVar1 = (float)FUN_141e532d0();
  return ((fVar1 * DAT_14259a764 * DAT_14259a764) / DAT_14256ae34) * param_1;
}
```

- `FUN_141E532D0()` returns the **match's ticks per second** (`DAT_14351AE54`; the port calls
  it "the logic clock" and "the match's ticks per second", `movestep.hpp:585` and
  `movestep_run.cpp:1529`). At 54 Hz it is `54.0`.
- `DAT_14259A764` = **60.0** and `DAT_14256AE34` = **1000.0** (both script-read).
- So the factor is `ticks * 60 * 60 / 1000` = `54 * 3600 / 1000` = **194.4** at 54 Hz, and the
  result is `factor * param_1`.

**The argument** (`0x1406CF8A7`): the caller's `XMM0` is the **speed**, computed just before it
as the length of a velocity vector:

```
1406CF880: MOVSS XMM1,dword ptr [RAX]        ; the velocity's x
1406CF884: MOVSS XMM0,dword ptr [RAX + 0x4]    ; y
1406CF889: MOVSS XMM2,dword ptr [RAX + 0x8]    ; z
1406CF88E: MULSS XMM0,XMM0
1406CF892: MULSS XMM1,XMM1
1406CF896: MULSS XMM2,XMM2
1406CF89A: ADDSS XMM0,XMM1
1406CF89E: ADDSS XMM0,XMM2
1406CF8A2: CALL 0x1415a1780                   ; the vector length
1406CF8A7: CALL 0x14042d470                   ; x the (ticks*60*60/1000) factor
```

**=> `XMM15` is the distance the player would cover in one second at the current speed** — the
**speed in metres per tick × 60 × 60 / 1000**, i.e. the per-second travel distance. The port
already documents this shape at `movestep_run.cpp:261`: "`ticks * 60 * 60 / 1000 *
|last-tick displacement|` (FUN_141e532d0's rate, the two 60s, …)".

**Its use in the gates.** `0x575`'s gate is `XMM15 < 16.0` (`DAT_14259BE10` = 16.0) and `0x573`'s
is `XMM7 > XMM15` with `XMM7` = `DAT_14259BE10` = 16.0 — the same 16.0 in the other direction.

**=> the `0x575` row is the "the player is moving slowly" row**: it is taken only when the
speed is under 16 km/h (`FUN_14042D470` returns km/h; see the units below). `0x573` is the
opposite test: it is taken when the speed is **over** 16 km/h.

**=> `0x573` and `0x575` are the two ends of the same speed test, on opposite sides of
16 km/h.** That is the sense of the pair: `0x573` for a fast approach, `0x575` for a slow one
(and `0x575` additionally requires the player to be within 1.1 m of the target).

**The units, stated once:** distances in **metres**, the tick rate in **ticks per second** (54), and
the per-tick speed in **metres per second** (the input is the difference of two stored 3-vectors one
tick apart, from `FUN_141EB50B0(rec, out, 1)`: INF on the units). `FUN_14042D470` returns
`v * 54 * 60 * 60 / 1000`, which is **km/h** for `v` in metres per tick, so the `16.0` threshold is
**16 km/h** (about 4.44 m/s). The §5c.2 label "the distance covered in one second" is wrong for the
same reason: the output carries the factor 3.6.

---

## §5d `FUN_1406D2590` (EnterTurnStop) — the `vt+0xD0` motion selector

*(Main's follow-up: the motion EnterTurnStop issues comes from the `+0xAC0` interface's slot
`0xD0`.)*

**The call site** (`FUN_1406D2590`, decompile line 293, issued at line 326):

```
piVar24 = param_2 + 0x16;                 ; the column pointer
uVar26 = 0;
piVar23 = local_res8;                     ; the row pointer
(**(code **)(*plVar2 + 0xd0))(plVar2, local_res20, local_res18, local_res10, piVar23, piVar24, 0);
```

`plVar2 = param_1 + 0xAC0` — the `+0xAC0` interface, whose vtable is at `0x1425E35B0`.

**The slot's function: `FUN_1406B9D80` (`0x1406B9D80`)** — script-read from the vtable at
`0x1425E35B0 + 0xD0`. The port already knows it: it is `pes::StopRowFor`
(`movestep_run.cpp:331`).

```c
void FUN_1406b9d80(undefined8 param_1, undefined2 *param_2, int *param_3, int *param_4,
                   int *param_5, int *param_6, longlong param_7) {
  iVar6 = *(int *)(&DAT_1425ddad0 + ((longlong)*param_4 + (longlong)*param_3 * 8) * 4);
  uVar1 = (&DAT_1425dd988)[*param_5];
  iVar2 = *param_6;
  if (iVar2 != 1) {
    if ((((iVar2 != 0x11) && (iVar2 != 0x70)) && (iVar6 != 0x582)) && (*param_3 == 2)) {
      iVar6 = 0x582;
    }
  }
  lVar5 = param_7;
  if (param_7 == 0) { lVar5 = FUN_1406abba0(); }
  param_5._0_4_ = uVar1;                  // the column
  param_6._0_4_ = iVar6;                  // the row
  uVar4 = FUN_1406abb30(lVar5, &param_6, &param_5, 0);
  *param_2 = uVar4;                      // the motion number
  param_5._0_4_ = uVar1;
  param_6._0_4_ = iVar6;
  uVar3 = FUN_1406abd60(lVar5, &param_6, &param_5);
  *(undefined1 *)(param_2 + 1) = uVar3;    // the mirror flag
}
```

**Its inputs.** The call site (`FUN_1406D2590`, line 293) is

```
(**(code **)(*plVar2 + 0xd0))(plVar2, local_res20, local_res18, local_res10, piVar23, piVar24, 0);
```

and the function's signature is
`FUN_1406B9D80(param_1, param_2, param_3, param_4, param_5, param_6, param_7)`, so:

| argument | what it is | where it comes from |
|---|---|---|
| `param_2` (out) | the **motion number** (a `short`) and, at `+1`, the **mirror flag** (a `byte`) | `local_res20` |
| **`param_3`** (in) | the **class** — the request's **class word `req[0x1F]`**, **clamped to 0..3** | `local_res18` = `param_2[0x1f]`, then clamped (`FUN_1406D2590` lines 74-82) |
| **`param_4`** (in) | the **kind** — the request's `req[0]`, with the **stamp rule** | `local_res10` = `*param_2`, overridden to 1 under the stamp condition (`FUN_1406D2590` lines 286-289) |
| `param_5` (in) | the **bucket** — the request's `kMoveBucket` (`req[8]`) | `piVar23` = `local_res8` = `param_2[0xc]` |
| `param_6` (in) | the **action kind** — `param_2 + 0x16`, the request's `kCommandId` (`req[0x16]`) | `piVar24` |
| `param_7` (in) | the clip table, or 0 for the default (`FUN_1406ABBA0`, the anime table) | the literal `0` |

**=> the class is `req[0x1F]` (the class word), NOT `req[9]`.** `req[9]`
(`kCommandSpeedClass`) is never read by the selector. The **kind** is `req[0]`, and the
**override tests the CLASS** (`*param_3 == 2`), not the kind.

**The row's table index** is `*param_4 + *param_3 * 8` — the **kind + class × 8** — and the
override's `*param_3 == 2` is therefore the **class** too.

**CORRECTION (ClipWords' audit, applied).** This section was written before ClipWords'
audit of the argument order, and the audit was right: the first draft of this section named
`req[9]` as the class and had `param_3`/`param_4` swapped. The corrected mapping is in the
table above and the checker's seven §5d mutations guard it. §5e adds the class
word's writers. The first draft of this section said the class was
`req[9]` and the kind `req[0]`, i.e. it had `param_3` and `param_4` **swapped** and named the
wrong request word for the class. The port's `StopRowFor` (which the first draft cited as
agreeing) had the *order* right but was cited for the wrong reason.

**The class word's range — and a consequence for the port.** PES clamps it to **0..3**, and
`DAT_1425DDAD0` has **32 entries** (4 classes × 8 kinds), script-read:

| class | the eight rows (kind 0..7) |
|---|---|
| 0 | 1398, 1398, 1399, 1400, 1401, 1402, 1403, 1403 |
| 1 | 1404, 1404, 1405, 1406, 1407, 1408, 1409, 1409 |
| 2 | 1410, 1410, 1411, 1412, 1413, 1414, 1415, 1415 |
| **3** | **1416, 1416, 1417, 1418, 1419, 1420, 1421, 1421** |

(The first draft of this section listed only 24 entries and so omitted the whole class-3
block — a consequence of the same argument mix-up.)

**=> the class can be 3, so the class-3 rows are live.** The class word `req[0x1F]` is
written with the literal **3** in several places (see §5e), and PES's clamp admits it.

**NO PORT DIFFERENCE — RETRACTED.** This section first claimed that the port's `StopRowFor`
clamps the class to 0..2 and so drops the class-3 rows. **That was wrong**: it was read off an
older revision of the working tree. On the branch (`origin/RunTurnRunPicks`) the port has the
**0..3** clamp (`movestep_run.hpp`: `const int c = cls < 0 ? 0 : (cls > 3 ? 3 : cls);`) and
`StopRowFor` also takes the action word, so it can implement the selector's
`action != 1 && action != 0x11 && action != 0x70` guard — which the version I read could not,
because it hard-coded the "action word is 0" case. The port's `GotoRowFor` and
`TurnStopRowFor` use the same 0..3 clamp. **The port matches PES; there is nothing to file.**
(ClipWords' audit of the branch; the class-3 test
`EnterTurnStop.TheClassWordThreeReadsTheStopTableBeyondItsTwentyFourRows` passes there.)

**The clip table it picks from.** `FUN_1406ABBA0` returns `DAT_1436F2378` — the same **anime
clip table** the runturn pick uses (`§0` above). So the motion comes from the **same table**,
at the cell `table[(row * 9 + column) * 2]`.

**The two tables it uses:**

- **the row**: `DAT_1425DDAD0` (`0x1425DDAD0`), the **stop rows**, indexed by
  `kind + class * 8` — **32 entries** (4 classes × 8 kinds), script-read in the table above.
  (This is the table the port calls `t.stopRow`.)
- **the column**: `DAT_1425DD988` (`0x1425DD988`) — **the same 10-entry bucket→column table**
  as §2 above, indexed by the bucket.

**The override.** When the action kind is **not** 1, **not** 0x11, **not** 0x70, the row is not
already 0x582, **and the CLASS is 2**, the row is forced to **0x582 (1410)**. (`0x582` is the
"turn stop" row the port's `StopRowFor` also forces: `if (row != 0x582 && c == 2) row = 0x582` —
the port's `c` is the class, matching PES's `*param_3`.)

**What it returns when nothing fits.** `FUN_1406ABB30` (§0) returns **−1** when the
row × column cell is empty (the cell's bit 15 is set) or the row/column is out of range. That
−1 is written to `*param_2` — **so the "nothing fits" answer is −1**, and the mirror byte is
whatever `FUN_1406ABD60` returns for that cell (0 if the cell is empty). The caller
`FUN_1406D2590` then sees a motion of −1 and skips the clip issue.

**=> the rule for ClipWords: EnterTurnStop's motion is
`StopRowFor(kind, class)` × `DAT_1425DD988[bucket]`, looked up in the anime clip table, where
the **class is the class word `req[0x1F]` clamped to 0..3** and the **kind is `req[0]`** (with
the stamp rule); when the cell is empty the motion is −1 and no clip is issued.** The port's
`StopRowFor` and `AngleBucket` already implement both halves; the only difference is the
class's upper clamp (the port's 0..2 against PES's 0..3).

---

## §5e The class word `req[0x1F]` — who writes it

*(ClipWords' question: which PES writes of `param_2[0x1f] = 0` are in the decompiles? The
goto stop arm, `FUN_1406BDF10` around its line 215-230.)*

**The class word is `req[0x1F]` — the request's fourth word.** It is the word the
`vt+0xD0` selector reads as its class, and the word the port's `StopRowFor` takes as `cls`.

**In `FUN_1406BDF10` (the goto stop arm) there are three writes of `param_2[0x1f] = 0`:**

**At line 227** — the arm that goes to **STOP** (`FUN_1406D5D60`):

```
1406BDF10's line 223:  param_2[0x25] = iVar12;                 ; the previous bucket
                      uVar21 = (**(code **)(*(longlong *)(param_1 + 0xac0) + 0x60))();
                      param_2[0x1d] = 0x40a00000;               ; the blend (5.0)
                      param_2[0x1f] = 0;                        ; the CLASS word <- 0
                      param_2[0x20] = uVar21;                   ; the new bucket
                      FUN_1406d5d60(param_1,param_2);           ; -> STOP
                      return;
```

**At line 400** — the arm that goes to the **GOTO_RUN swing** (`FUN_1406CE820`):

```
LAB_1406BE554:
  param_2[0x1d] = 0x40a00000;
  param_2[0x1f] = 0;                          ; the CLASS word <- 0
  FUN_1406ce820(param_1,param_2,0);
  return;
```

**At line 419** — the arm that goes to `FUN_1406D38D0`, where the class word is set from the
player's **speed**:

```
if (DAT_14259222c <= fVar22) {                 ; 5.0 <= the per-second distance
  if (DAT_14259BDFC <= fVar22) { param_2[0x1f] = 2; param_2[0x1d] = 0x41800000; }   ; fast -> class 2
  else                      { param_2[0x1f] = 1; param_2[0x1d] = 0x41300000; }   ;       -> class 1
}
else { param_2[0x1d] = 0x40a00000; param_2[0x1f] = 0; }                         ; slow -> class 0
```

**=> in the goto stop arm the class word is 0 (stop), 1 or 2 (the swing, by speed), and the
stop arm clears it to 0 before the STOP handler.** CONFIRMED.

**The class word can be 3.** Across the MOVESTEP functions the literal **3** is written in:

| function | the line | the context |
|---|---|---|
| `FUN_1406BA900` (the MOVESTEP dispatcher) | 314 | a class-3 branch |
| `FUN_1406C14C0` | 288, 900 | two class-3 branches |
| `FUN_1406C3470` | 308 | a class-3 branch |
| `FUN_1406C7400` | 322, 431, 468, 629, 657, 764 | six class-3 branches |
| `FUN_1406C8D80` | 244 | a class-3 branch |
| `FUN_1406D6780` (the RUN_TURN_RUN entry) | 527, 804, 1042 | three class-3 branches |
| `FUN_1406D6D60` (the STOP handler) | 527, 804, 1042 | the same three |

**=> the class-3 rows of `DAT_1425DDAD0` (1416..1421) are live, and PES's 0..3 clamp is
correct.** The port's 0..3 clamp on the branch matches.

**The other class-word writers** (for completeness — the values, not the contexts):

| function | the values it writes |
|---|---|
| `FUN_1406BA900` | 0xFFFFFFFF (twice), 0, 1, 2, 3, and two computed values |
| `FUN_1406BDF10` | 0 (×3), 1, 2, and one computed value |
| `FUN_1406C0170` | 0, 1, and one computed value |
| `FUN_1406C1070` (the RUN_TO_STEP handler) | `param_2[0x1f] = param_2[0x24]` — the class is **copied from the previous class** (the byte at `req+0x90`) |
| `FUN_1406C6480` | 0, 1, 2, and `param_2[0x24]` (the same copy) |
| `FUN_1406C4C10`, `FUN_1406C6F80`, `FUN_1406C8D80`, `FUN_1406C3470`, `FUN_1406C7400`, `FUN_1406C14C0`, `FUN_1406D2F60`, `FUN_1406D38D0`, `FUN_1406CE820` | 0, 1, 2, 3 and computed values |

**The two "copy" writes** (`req[0x1F] = req[0x24]`) are the ones that make the class word
*stick* across a tick: `req[0x24]` is the **previous** class word (the byte at `req+0x90`).

---

## §5f The GOTO cruise tail: the `+0xAC0` interface's slot `0x00`

*(ClipWords' queued decode request: the `vt+0x00` of the `+0xAC0` object, called from
`FUN_1406BDF10`'s GOTO cruise tail.)*

**The call site** (`FUN_1406BDF10`, lines 346-379) — confirmed line for line:

```
346:  plVar2 = (longlong *)(param_1 + 0xac0);        // the +0xAC0 interface
       ...                                            // the rate is set at 0x4a
374:  iVar10 = (**(code **)(*plVar1 + 0x78))();      // plVar1 = param_1 + 0xab8
375:  param_2[0x1b] = 0;
376:  param_2[0x2e] = 0;
377:  *(undefined1 *)(param_2 + 0x50) = 0;             // the style byte
378:  *(bool *)(param_2 + 0x26) = iVar10 - 1U < 2;    // the foot flag
379:  (**(code **)*plVar2)(plVar2,param_2,iVar10,0); // the call
```

**`plVar1` is `param_1 + 0xAB8`** — CONFIRMED, and it is a *different* interface from
`plVar2` (`param_1 + 0xAC0`). The `vt+0x78` read is on the `+0xAB8` object; the call is on
the `+0xAC0` one.

### §5f.1 The function

**`FUN_1406D2030` (`0x1406D2030`)** — the `+0xAC0` interface's vtable slot `0x00`
(script-read from the vtable at `0x1425E35B0 + 0x00`). Its signature is
`void FUN_1406D2030(longlong *param_1, int *param_2, int param_3, uint8_t param_4)`, where
`param_1` is the `+0xAC0` object, `param_2` the request block, `param_3` the **foot argument**
(the `iVar10` from `vt+0x78` of the `+0xAB8` object) and `param_4` the **0** literal.

**The concrete class.** `param_1` is the `+0xAC0` interface of the **AnimePlayer** — the same
object whose `+0xAC0` field the command fill uses (`FUN_1406B7560`'s `LEA RBX,[R14 + 0xac0]`)
and whose `vt+0x80` the bucket index comes from. Inside, `param_1[-1]` is the **`+0xAB8`
interface** (the vtable is at `param_1 - 8`), and `param_1 + 0x158` is the AnimePlayer's own
record. CONFIRMED.

### §5f.2 Every word and byte of `param_2` it reads or writes

**READ**

| the offset | what |
|---|---|
| `param_2[0x00]` (word 0) | the move kind — tested against 3 and 6 |
| `param_2[0x09]` (word 9) | the command speed class |
| `param_2[0x0A]` (word 0x0A) | the desired speed |
| `param_2[0x0B]` (byte 0x2C) | a flag: when set, the class comes from `vt+0x38` instead of the band |
| `param_2[0x1C]` (byte 0x70) | a flag: when zero, the class-0 / kind-3-or-6 special case applies |
| `param_2[0x1D]` (word 0x1D) | the blend — read, clamped and written back |
| `param_2[0x1F]` (word 0x1F) | the class word — read, clamped to 0..3 and written back |
| `param_2[0x26]` (byte 0x26) | **the foot flag — READ and passed to `FUN_1406BA2F0`** |
| `param_2[0x50]` (byte 0x50) | the style byte — read and passed to `FUN_1406BA2F0` |

**WRITTEN**

| the offset | the value |
|---|---|
| `param_2[0x1F]` | the class word, **clamped to 0..3** |
| `param_2[0x1D]` | the blend, clamped into `[DAT_1425A2004 (28.0), DAT_14259222C (5.0)]` — the value is kept when it is between 5.0 and 28.0 |
| `param_2[0x4A]` | the rate: `vt+0x38` of the class (or the kind-3/6 special case), then clamped to `[0, vt+0x18 × DAT_1425AB124]` |
| `param_2[0x37]` | the **motion number** (from `FUN_1406BA2F0`'s return) |
| `param_2 + 0x50` (byte) | the motion number again — the style byte is **overwritten with the motion** |
| `param_2[0x30]` | the motion: `DAT_1425DDCF0[style × 0x10 + band]`, the band (4, 8 or 12) chosen from the rate against `DAT_1425DDF54` (0.225), `DAT_14259B89C` (0.4) and `DAT_14257A194` (0.7) |
| `param_2[0x4F]` | the playback speed: 0, 0.5, 0.75 or 1.0 (see below) |
| `param_2 + 0x26` (byte) | **the foot flag** — `param_3 - 1 < 2` |
| `param_2 + 0x31` (byte) | **the foot flag again**, then possibly forced to 1 or 0 |
| `param_2[0x09]` | the class, clamped to 0..3 |
| `param_2[0x36]` | 0 |
| `param_2[0x1B]` | `0x3DCCCCCD` (0.1) |
| `param_2[0x2D]` | 0 |
| `param_2[0x2E]` | 0 |
| `param_2[0x1A]` | 0 |
| `param_2[0x20]` | 0 |
| `param_2 + 0x38` (byte) | 1 — the motion-pending flag |
| `param_2[0x39]` | 3 — the next state |
| `param_2[0x32]` | 0.3 (or 0.1666667, or 0.1) — the playback blend |
| `param_2[0x1E]` | `*param_2` — the kind, copied forward |
| `param_2[0x35]` | 1.0 |
| `param_2[0x33]` | 0 |
| `param_2 + 0x3C` (byte) | 0 |

**=> byte `0x98` is NEVER read or written by this function.** CONFIRMED by a grep of the
decompile: no `0x98` operand at all. The foot flag lives at **byte `0x26`** (and is mirrored
into byte **`0x31`**).

**The foot argument and the 0 argument.**
- `param_3` is the **foot**: the value from `vt+0x78` of the `+0xAB8` object. The foot flag
  is `param_3 - 1 < 2`, i.e. the foot is 1 or 2 (left/right).
- `param_4` is the **0** literal at this call site. It is passed straight through to
  `FUN_1406BA2F0` as its last argument.

**The byte `0x31` rules** (the foot flag's second copy):
- it is first set to `param_3 - 1 < 2` (the foot flag);
- if the motion is below 0xD **and** its bit in the mask `0x1028` is set, it is forced to **1**;
- else, if the motion is above 0xB **and** its bit in the mask `0x814` is set, it is forced to
  **0**;
- and `param_2[0x4F]` (the playback) is then chosen from it: the foot flag set → 0 or 0.75
  (0.5 when `param_3` is 1 or 3), the foot flag clear → 0.5 or 0.25.

### §5f.3 What it issues to the body

**`FUN_1406BA2F0` (`0x1406BA2F0`)** is the issuer:

```c
uVar3 = FUN_1406ba2f0(plVar1, uVar2, uVar10, param_2, (char)param_2[0x26],
                     (char)param_2[0x50], 0, uVar11, param_4);
param_2[0x37] = uVar3;
*(char *)(param_2 + 0x50) = (char)uVar3;
```

Its arguments are the AnimePlayer record, the `+0xAB8` object's `vt+0x28` value, the
`+0xAC0` object's `vt+0x38` value, the request block, **the foot flag (byte 0x26)**, **the
style byte (byte 0x50)**, a 0, the `+0xAC0` object's `vt+0x18` value, and `param_4`.

So the issue is:

| what | the value |
|---|---|
| the **motion** | `FUN_1406BA2F0`'s return, written to `param_2[0x37]` and byte `0x50` |
| the **motion's clip** | `param_2[0x30]` = `DAT_1425DDCF0[style × 0x10 + band]`, the band (4, 8 or 12) chosen from the rate against `DAT_1425DDF54` and `DAT_14259B89C` |
| the **mirror** | the foot flag, mirrored into byte `0x31` (and byte `0x26`) |
| the **blend** | `param_2[0x32]` — 0.3, or 0.1666667 when the player is nearly on the target and the turn is small |
| the **playback** | `param_2[0x4F]` — 0, 0.25, 0.5 or 0.75, from the foot flag and `param_3` |
| the **rate** | `param_2[0x4A]` |

**And the port's stand-in.** The port issues `MotionRequest{-1, false, 0, k.blend1667, k.one,
0.0f, 0, 0.0f, req.F(kRate), foot}` (`movestep_run.cpp:828`) — the motion is **−1** and the
mirror is **false**. PES instead resolves the motion from `DAT_1425DDCF0[style × 0x10 + band]`
and mirrors the foot flag into byte `0x31`.

**A PORT-SIDE NAMING ISSUE worth checking (not a behaviour claim).** The port's
`SetByte(word, v)` writes the **low byte of word `word`** (`movestep.hpp:203`:
`words_[word] = (words_[word] & ~0xffu) | v;`). So:

- `req.SetByte(moveword::kFootFlag, ...)` with `kFootFlag = 0x98` writes **byte 0x260**;
- `req.SetByte(moveword::kMirror, ...)` with `kMirror = 0x31` writes **byte 0xC4**.

**PES writes the foot flag to byte 0x26** and mirrors it into **byte 0x31**. So the port's
`kFootFlag` constant does not name PES's byte `0x26` (which is word `0x09` in the port's
numbering), and the port's `kMirror` (byte 0xC4) is not PES's byte `0x31` (word `0x0C`).
**The port's own comment on `kFootFlag` says "byte req+0x26"**, so the constant's *value* and
its *comment* disagree. **This is reported as a naming/offset observation, not as a behaviour
defect** — whether it matters depends on whether the port's readers use the same constants
consistently, which is ClipWords' side to judge.

---

## §5g `FUN_1406C4C10` (STEP_LOOP): the band walk into `DAT_1425DD9B0`

*(ClipWords' question: the row index into the step kind/band grid is
`DAT_1425DD9B0[kind + band*8]` — which PES value is the band there, and is there any code
that steps the band by one toward or away from the command class?)*

**The band is the class word `live = req[0x1F]`, walked one step toward a target class `E`:**
**`band = live + sgn(E - live)`.** Both paths below give this rule. The bridge decompile of the same
function (`<workdrive>/gf-TeamDecode-scratch/bridge_c4c10.c`, lines 86-108) states it too: `iVar10`
is `E`, and lines 105-108 are the one-step walk. Two corrections to the earlier text of this section:
the decompile was **not** mis-reading the block (it agrees with the bytes), and the special condition
compares `req[0x1A]` with **`0.1`**, not `-1.0` (the earlier quote was a transcription error).

**The clamp** (CONFIRMED, `0x1406C4D60`-`0x1406C4D77`): `clamp(x) = 0` for `x < 0`, `x` for `0 <= x <= 2`,
and `2` for `x >= 3`. So `E` is always 0..2: a command class of 3 gives the default 2, never 3.

**The special condition** (CONFIRMED): the byte at `param_1+0x245D` is `0x13` or `0x10`
(`0x1406C4DAE`-`0x1406C4DBC`), **and** `req[0x1A] < 0.1`. The `0.1` is the float at `0x142588200`
(bits `0x3DCCCCCD`), loaded by `MOVSS XMM0,[0x142588200]` at `0x1406C4DBE`. The test is
`COMISS XMM0,[RDI+0x68]` at `0x1406C4DC6`, and `JBE` at `0x1406C4DCA` skips the block when it fails.
A NaN fails the test.

**The trace** (raw image bytes, CONFIRMED). The XMM work between `0x1406C4D77` and `0x1406C4DAE` writes
neither `EBP` nor `ECX`. `ECX` is set to 2 at `0x1406C4D63`, and nothing writes it again before
`0x1406C4DCF`; the earlier `ECX = live` at `0x1406C4CD6` is overwritten there.

```
1406C4D60: 8B 6F 24          MOV EBP,[RDI+0x24]        ; EBP = req[9]
1406C4D63: B9 02 00 00 00    MOV ECX,2                 ; ECX = 2
1406C4D6D: 3B E9             CMP EBP,ECX
1406C4D6F: 7E 04             JLE 0x1406C4D75
1406C4D71: 8B E9             MOV EBP,ECX               ;   req[9] > 2 -> 2
1406C4D73: EB 06             JMP 0x1406C4D7B
1406C4D75: 85 ED             TEST EBP,EBP
1406C4D77: 41 0F 48 EF       CMOVS EBP,R15D            ;   req[9] < 0 -> 0 (R15D = 0)
   ...                       (1406C4D7B-1406C4DAD: XMM work only)
1406C4DAE: 41 0F B6 86 5D 24 00 00   MOVZX EAX,byte [R14+0x245D]
1406C4DB6: 3C 13             CMP AL,0x13
1406C4DB8: 74 04             JE 0x1406C4DBE
1406C4DBA: 3C 10             CMP AL,0x10
1406C4DBC: 75 1F             JNE 0x1406C4DDD           ; common path: E = clamp(req[9])
1406C4DBE: F3 0F 10 05 3A 34 EC 01   MOVSS XMM0,[0x142588200]   ; 0.1
1406C4DC6: 0F 2F 47 68       COMISS XMM0,[RDI+0x68]
1406C4DCA: 76 11             JBE 0x1406C4DDD           ; test failed: common path
1406C4DCC: 8B 6F 7C          MOV EBP,[RDI+0x7C]        ; special path: EBP = live
1406C4DCF: 3B E9             CMP EBP,ECX               ; ECX is still 2
1406C4DD1: 7E 04             JLE 0x1406C4DD7
1406C4DD3: 8B E9             MOV EBP,ECX               ;   live > 2 -> 2
1406C4DD5: EB 06             JMP 0x1406C4DDD
1406C4DD7: 85 ED             TEST EBP,EBP
1406C4DD9: 41 0F 48 EF       CMOVS EBP,R15D            ;   live < 0 -> 0
1406C4DDD: 8B 47 7C          MOV EAX,[RDI+0x7C]        ; common tail: EAX = live, EBP = E
1406C4DE0: 3B C5             CMP EAX,EBP
1406C4DE2: 74 0A             JZ 0x1406C4DEE            ; E == live: band = live
1406C4DE4: 3B E8             CMP EBP,EAX               ; flags from E - live
1406C4DE6: 8D 68 01          LEA EBP,[RAX+1]           ; LEA sets no flags
1406C4DE9: 7F 03             JG 0x1406C4DEE            ; E > live: band = live + 1
1406C4DEB: 8D 68 FF          LEA EBP,[RAX-1]           ; E < live: band = live - 1
1406C4DEE: F3 0F 10 47 74    MOVSS XMM0,[RDI+0x74]
```

**The rule, by path** (CONFIRMED from the trace):

| path | `E` | band |
|---|---|---|
| common: byte is not `0x13` or `0x10`, or `req[0x1A] >= 0.1`, or NaN | `clamp(req[9])` | `live + sgn(E - live)` |
| special: byte is `0x13` or `0x10`, and `req[0x1A] < 0.1` | `clamp(live)` | `live + sgn(E - live)` |

So the walk steps `live` one step toward `E`: `E > live` gives `live + 1`, `E = live` gives `live`, and `E < live` gives `live - 1`. The earlier three-case rule (band = `live` for `live <= cmd`, `live + 1` only for a negative `live`, `live - 1` above `cmd`) holds on the special path only. On the common path it is wrong whenever `0 <= live < E`.

**The `live + 1` branch is REACHABLE on both paths**: on the common path when `0 <= live < E`, and on the special path when `live < 0` (`live = -1` gives band 0 on both).

**Worked traces** (CONFIRMED from the instructions above):
- Common path, byte `0`, `req[9] = 2`, `live = 0`: `E = 2`. `CMP EAX,EBP` gives `0 - 2`, then `CMP EBP,EAX` gives `2 - 0 > 0`, so `LEA EBP,[RAX+1]` runs and `JG` is taken: **band = 1**.
- Common path, `req[9] = 0`, `live = 2`: `E = 0`. `CMP EBP,EAX` gives `0 - 2 < 0`, so `LEA EBP,[RAX-1]` runs: **band = 1**.
- Special path, byte `0x13`, `req[0x1A] = 0`, `live = -1`: `JLE` is taken, `TEST` sets SF, and `CMOVS` gives `E = 0`. Then `E > live`: **band = 0**.
- Special path, byte `0x13`, `req[0x1A] = 0`, `live = 3`: `JLE` is not taken, so `E = ECX = 2`. Then `E < live`: **band = 2**.

**Range.** The arithmetic is CONFIRMED; the class word's own range is INF (the writers in §5e and §5f were not re-checked here). `E` is in {0, 1, 2}, so for `live` in {-1, 0, 1, 2, 3} the band is in {0, 1, 2} on both paths. `live = -1` gives 0, `live = 3` gives 2, and the step always stays between `live` and `E`. Band 3 needs `live >= 4`, giving `live - 1`. The walk has **no clamp on the band** itself: the `+1` and `-1` results are used as they are.

**Port's one line:** `band = live + sgn(E - live)`, with `E = clamp(req[0x1F])` when the special condition holds and `E = clamp(req[9])` otherwise.

**The lookup** (`0x1406C5D8E`):

```
1406C5D72: MOVSXD RCX,dword ptr [RDI]        ; RCX = the kind, *param_2
1406C5D7F: MOVSXD RDX,EBP                     ; RDX = the band (the walked EBP)
1406C5D82: MOV R15D,dword ptr [RAX + R15*0x4 + 0x25dd988]   ; the column
1406C5D8A: LEA R8,[RCX + RDX*0x8]             ; kind + band*8
1406C5D8E: MOV EBX,dword ptr [RAX + R8*0x4 + 0x25dd9b0]   ; the grid entry
```

**=> the row index is `kind + band*8` with the band the walked value, the column is
`DAT_1425DD988[R15]` — the same 10-entry bucket→column table as everywhere else — and the grid
is `DAT_1425DD9B0`, 32 entries (8 kinds × 4 bands) of int32.**

**The grid's contents** (script-read as int32; it is read as a float in the decompile but the
values are integers):

| band \ kind | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| **0** | 1325 | 1325 | 1325 | 1326 | 1331 | 1332 | 1333 | 1351 |
| **1** | 1337 | 1337 | 1337 | 1338 | 1340 | 1341 | 1342 | 1351 |
| **2** | 1345 | 1345 | 1345 | 1346 | 1348 | 1349 | 1350 | 1352 |
| **3** | 1345 | 1345 | 1345 | 1346 | 1348 | 1349 | 1350 | 1352 |

**These are the goalkeeper's block/cover/dive motions** (`pesfmt_dump motions`): 1325
`gkblockcover_f03_dive_3_0_y06`, 1326 `gkblockcover_s01_0_0_y02`, 1331
`gkblockcover_s02_dive_0_0_y06`, 1332 `gkblockcover_s03_dive_0_0_y02`, 1333
`gkblockcover_s03_dive_0_0_y04`, 1337 `gkblockcover_s04_dive_0_0_y02_punch`, 1338
`gkblockcover_s04_dive_0_0_y04`, 1340 `gkblock_lie_f00_0_0_000_dive`, 1341
`gkblock_lie_f00_0_0_y04_000_dive`, 1342 `gkblock_lie_f03_0_0_y04_000_dive_body`, 1345
`gkblock_lie_s02_0_3_090_dive`, 1346 `gkblock_lie_s03_0_0_y00_000_otherside`, 1348
`gkblock_s01_otherside_3_0_after_y01`, 1349 `gkblock_s01_otherside_3_0_after_y05`, 1350
`gkblock_s03_3_0_000_postcrash`, 1351 `gkcatch_b00_0_0_y00`, 1352 `gkcatch_b03_0_0_y08`.

**=> the grid is the goalkeeper's dive/cover table**, indexed by the move kind and the band. That
is the table the STEP_LOOP handler reads for the goalkeeper's block response.

### §5g.1 The walk verified in the install's exe (a claim I made and retracted)

**RETRACTION.** While verifying §5g I first concluded that the install's `PES2021.exe` was a
"modded / hot-patched build" whose struct field offsets were shifted by +0x3B00, and I told
ClipWords and Main so. **That was wrong**, and Main caught it: the install's exe and
`pes_full.bin` are the **same build** and read identically.

**What I got wrong.** I built the VA→file-offset map for `pes_full.bin` and the install's exe
with the same formula, but the two files have **different `.trace` raw offsets** (the install's
`.trace` starts at file `0x600`, and `pes_full.bin` carries a leading zero region, so its
matching code sits 0xA00 further in). Using one map on both files made me read a *different
region* of one of them, and the bytes I found there looked like `JMP rel32` trampolines and a
`+0x3B00` displacement. **Both were artefacts of the wrong offset.**

**The truth, with the correct map.** The install's `PES2021.exe` and `pes_full.bin` are the
same binary: at `0x1406C4DCC` both read `8B 6F 7C 3B E9 7E 04 …`, and the table at
`0x1425DD9B0` is byte-identical in both.

Decoded, the image's own instruction stream for the walk is:

```
1406C4DCC: 8B 6F 7C          MOV EBP,dword ptr [RDI + 0x7C]
1406C4DCF: 3B E9             CMP EBP,ECX
1406C4DD1: 7E 04             JLE 0x1406C4DD7
1406C4DD3: 8B E9             MOV EBP,ECX
1406C4DD5: EB 06             JMP 0x1406C4DDD
1406C4DD7: 85 ED             TEST EBP,EBP
1406C4DD9: 41 0F 48 EF       CMOVS EBP,R15D
1406C4DDD: 8B 47 7C          MOV EAX,dword ptr [RDI + 0x7C]
1406C4DE0: 3B C5             CMP EAX,EBP
1406C4DE2: 74 0A             JZ 0x1406C4DEE
1406C4DE4: 3B E8             CMP EBP,EAX
1406C4DE6: 8D 68 01          LEA EBP,[RAX + 0x1]
1406C4DE9: 7F 03             JG 0x1406C4DEE
1406C4DEB: 8D 68 FF          LEA EBP,[RAX + -0x1]
1406C4DEE: F3 0F 10 47 74    MOVSS XMM0,dword ptr [RDI + 0x74]
```

**This is the same walk the bridge's listing shows** — the same `CMP`/`JLE`/`MOV EBP,ECX`, the
same `TEST`/`CMOVS` clamp of the class to ≥ 0, the same `LEA EBP,[RAX+1]` / `LEA EBP,[RAX-1]`
band arithmetic, and the same field at `[RDI + 0x7C]`. The bridge prints `AD`/`C8` where the
image has `6F`/`E9` (the *other* encoding of the same instructions), and `7E 06`/`89 C8` where
the image has `7E 04`/`8B E9` — the same jumps and moves, different byte encodings.

**=> §5g's bytes are confirmed against the install's exe, not only against the bridge.** The walk's
instructions, the field offset `+0x7C`, the constant 2 in `ECX` and the grid lookup are as §5g states.
There is no modded build, no +0x3B00 shift, and no hot-patch trampolines.

**A caution worth keeping.** The bridge's *listing text* and the install's *bytes* differ in
their encodings for these instructions, so a byte-level check of a quoted instruction against
the install's exe can fail even when the reading is right. The checkers in this report therefore
verify the **listing text** for §5g and verify the **bytes** only where the two agree. Where a
byte check is wanted, the image's own instruction stream must be decoded — with the correct
VA→offset map for the file in hand.

---

## §5h The issuer `FUN_1406BA2F0` and its callees (ClipWords' item 2)

*(ClipWords' question: what the `+0xAC0` object's vt+0 issues through `FUN_1406BA2F0`, and which callees decide it. Every helper here was read from the bridge decompile or the image bytes. The instruction-level claims are checked against the bytes by `check_runtturn.py` §6h.)*

**The issuer returns a style code, not a clip.** `FUN_1406BA2F0` returns one of **0, 8, 10, 13, 15** (CONFIRMED at every return site below). `FUN_1406D2030` stores the code at `param_2[0x37]` and at byte `0x50` (decompile lines 95-96: `param_2[0x37] = uVar3; *(char *)(param_2 + 0x50) = (char)uVar3;`), so the style byte is overwritten by it. The §5f label "motion number" for word `0x37` means this style code. The rest of §5f stands.

### §5h.1 The secondary vtable of `AP+0xAB8` (slots `+0x00` to `+0x28`)

`AP` is the AnimePlayer. The `+0xAB8` interface's vptr is `0x1425E3508` (CONFIRMED: the primary vtable at `0x1425E34D8` has the same entries at its `+0x30` and `+0x38`, so the secondary table starts inside it). Each slot was read from the image (`qword` at `0x1425E3508 + 8k`), and each function was disassembled from its bytes.

| slot | function | body | behaviour | verdict |
|---|---|---|---|---|
| `+0x00` | `0x1406EAAF0` | `mov rax,[rcx+0x3008]; ret` | returns the qword at `AP+0x3AC0` | CONFIRMED bytes; name OPEN; the issuer discards the result |
| `+0x08` | `0x1406EA810` | `movsx eax,byte [rcx+0x19A0]; ret` | returns the player index, byte `AP+0x2458` | CONFIRMED bytes; the issuer requires index `<= 21` and passes it to `FUN_140A489F0` |
| `+0x10` | `0x1406EF320` | `cmp dx,[rcx+0x28]; je; mov [rcx+0x28],dx; mov byte [rcx+0x2CA6],1; ret` | setter: stores the motion-id word `AP+0xAE0` if it changed, and sets the dirty byte `AP+0x375E` | CONFIRMED bytes |
| `+0x18` | `0x1406E6BD0` | `lea rax,[rcx+0x28]; ret` | returns the address of the motion-id word `AP+0xAE0` | CONFIRMED bytes; `FUN_1406E6960` reads it as a `u16` (CONFIRMED decompile) |
| `+0x20` | `0x1406EADE0` | `lea rax,[rcx+0x19A4]; ret` | returns the address of the MOVESTEP phase byte `AP+0x245C` | CONFIRMED bytes; the phase name is from `pes-re-anim-select.md` (`+0x245c`, OBS) |
| `+0x28` | `0x1406E66B0` | `movsx eax,byte [rcx+0x11]; ret` | returns the action sub-id, byte `AP+0xAC9` | CONFIRMED bytes; the name is from `pes-re-a3helpers.md` (`+0xac9`, sub-id); the issuer tests it against `0x4A` |

The `+0xAC0` interface (vptr `0x1425E35B0`) supplies two more values: slot `+0x18` is `0x1406EADF0` (`movss xmm0,[AP+0x377C]`, the rate ceiling `rateMax`; `pes-re-anim-select.md` §3.5 names `P+0x377c` as rateMax), and slot `+0x38` is `0x1406B9E50` (the band rate, §5h.4). Both CONFIRMED bytes.

### §5h.2 The issuer's arguments

The caller `FUN_1406D2030` calls `FUN_1406BA2F0(AP, sub, rate, req, req[0x26], req[0x50], 0, rateMax, flag)` (decompile lines 92-93). Inside the issuer (CONFIRMED bytes):

- arg1 `RCX` = `AP`; arg2 `EDX` = `sub` (the `+0xAB8` slot `+0x28` value, the sub-id); arg3 `XMM2` = `rate` (the `+0xAC0` slot `+0x38` value, the band rate).
- arg4 `R9` = the request block `req` (moved to `RSI` at `0x1406BA323`).
- arg5 (foot, `req[0x26]`), arg6 (style, `req[0x50]`) and arg7 (`0`) are **not read** by the issuer.
- arg8 `[RSP+0xA8]` = `rateMax` (a `float`, `AP+0x377C`).
- arg9 `[RSP+0xB0]` = the caller's `param_4` byte (the docs' `+0x30` flag), read at `0x1406BA41A`.

Request words read: `req[0x16]` (`[RSI+0x58]`, the command; 5 is the dribble), `req[0x4A]` (`[RSI+0x128]`, the requested rate), `req[9]` (`[RSI+0x24]`, the command speed class), and byte `req+0x2C` (`[RSI+0x2C]`, a flag).

### §5h.3 The issuer's decision, in the order the bytes test it

Let `G = *(DAT_1436F3DC8)`, `mode = *(*(G+0x270)+0x116C)` (`mode == 1` is open play phase 1, per the docs), `R = *(G+0x1A8)`, `idx` = the `+0x08` slot value, `rec = FUN_140A489F0(G, idx)`, and `stam = FUN_140A58550(rec)` (the stamina %, §5h.4).

- **[A] 15.** If `sub == 0x4A` or `mode != 1` (`0x1406BA32E`-`0x1406BA34A`): if `R.dword[+0x88] != 0` (`0x1406BA365`-`0x1406BA36C`) or `R.dword[+4] != idx` (`0x1406BA375`-`0x1406BA388`), return **15**. The docs' 0x0F text is "not in open play phase 1, or command 0x4a, or another player owns the scripted slot" (`pes-re-anim-select.md` §3.3), and this is that test (CONFIRMED).
- **[B] 13.** If `idx <= 21` (`0x1406BA397`-`0x1406BA39A`) and the low byte of `stam` is `< 20` (`0x1406BA3C5`, `CMP AL,0x14`), return **13** (`0x1406BA3C9`). The docs' 0x0D text is "in-match stamina byte < 20".
- **[C] 0.** If `sub == 0x4A` (`0x1406BA3D7`), return **0**.
- **[D] 10.** Entered when `rate > req[0x4A] + 0.15` (`0x1425C48A0` = 0.15, `0x1406BA3E8`-`0x1406BA3F0`), or when `req[9] == 3` and `req+0x2C != 0` (`0x1406BA3F5`-`0x1406BA3FB`). It returns **10** when all of these hold: `rateMax * 0.895 > req[0x4A]` (`0x1425DDF74`, `0x1406BA40D`-`0x1406BA418`); arg9 `== 0` (`0x1406BA41A`); `req[0x16]` is in `{2, 5, 51, 52, 62}` (mask `0x4018000000000024`, `0x1406BA42D`-`0x1406BA43B`); and `rateMax * 0.825 > req[0x4A]` (`0x1425DDF6C`, `0x1406BA43D`-`0x1406BA44A`). The docs' row for 10 names the commands 2, 5, 0x33, 0x34 and 0x3E, which are the mask's bits (CONFIRMED).
- **[E] 8 or 0.** Entered when `req[0x16] == 5` and `req+0x2C != 0` and `mode == 1` (`0x1406BA456`-`0x1406BA487`). Then:
  - `k = int(fps * 20 / 60 + 0.5)`, which is 18 at 54 fps (`0x1406BA4AA`: `*20.0` at `0x14259BE1C`; `0x1406BA4BD`: `/60.0`; `0x1406BA4B5`: `XMM6 = 0.5`; `0x1406BA4C9`: truncation). `S = FUN_140A6CF70(BI, k)` with `BI = *(G+0x40)` (`0x1406BA4CD`).
  - The bearing is `FUN_140439E50(out, AP.xz, S.xz)` and the angle is `FUN_1404583B0(AP+0x20, bearing)` (`0x1406BA4F1`-`0x1406BA4FD`). The test passes when the angle is `< 45.0` (`0x14259BE30`, `0x1406BA502`-`0x1406BA50D`).
  - `dist2D(AP.xz, S.xz) < 3.0` (`0x14258C264`, `0x1406BA513`-`0x1406BA542`).
  - `0.9 * AP+0x3B18 > S.y`. The term is `FUN_1406E7380(AP) / 100 * XMM6`, and `XMM6` is **0.5** on this path, because `0x1406BA4B5` reloaded it: `param_3` is not used here (`0x1406BA551`-`0x1406BA559`).
  - `speed = FUN_14042D470(FUN_140A57650(BI, 1))`, in km/h. If `speed < 16.0` (`0x14259BE10`, `0x1406BA576`-`0x1406BA581`), return **8** (`0x1406BA5A1`).
  - Otherwise `angle2 = FUN_14043A020(AP+0x20, FUN_140A57710(BI))`. If `angle2 > 45.0` (`0x1406BA59C`), return **8**.
  - Otherwise return **0**.
- Default: return **0** (`0x1406BA5AB`).

The docs' table (`pes-re-anim-select.md` §3.3 and `pes-re-anim-select-2.md` §2.2: `0xf`, `0xd`, `0xa`, `8`, `0`) agrees with this. The bytes add three details: the `0.895` test and the arg9 flag in [D], the `0.15` gate in [D], and the `XMM6 = 0.5` term in [E].

### §5h.4 The callees

| callee | VA | body or semantics | verdict |
|---|---|---|---|
| `FUN_142348F80` | `0x142348F80` | `mov eax,[rcx+0x88]; ret`: the dword at `R+0x88` | CONFIRMED bytes; the name (scripted-slot flag) is INF, from the 0x0F docs text |
| `FUN_140A73E20` | `0x140A73E20` | `lea rax,[rcx+4]; ret`: `R+4`, the scripted slot's owner index | CONFIRMED bytes; name INF |
| `FUN_140A489F0` | `0x140A489F0` | `idx < 0x16`: `*(G + (idx+0xF3)*0x28)`; else `*(G+0x25F8)`. The per-player record pointer | CONFIRMED decompile; `P_ai` per `pes-re-A4Carrier.md` |
| `FUN_140A58550` | `0x140A58550` | `100 * V(rec+0xD8 ring) / V(rec+0xF8 ring)`: two checksummed, obfuscated 32-bit fields (rings selected by `rec+0xD4` and `rec+0xF4`; keys `rec+0xD0` and `rec+0xF0`; checks `rec+0xC8` and `rec+0xE8`; FNV-64 checks; a mismatch calls `FUN_1415A1714`). Returns 0 when the `rec+0xF8` value is 0 | CONFIRMED decompile (3955 bytes); the stamina % (`cur * 100 / max`) per `pes-re-clock-record.md` §1 |
| `FUN_1406E7380` | `0x1406E7380` | `movss xmm0,[P+0x3B18]; mulss xmm0,[180.0]`: `P+0x3B18 * 180` | CONFIRMED bytes; `P+0x3B18` is a per-player scale (`pes-re-a3helpers.md`, the hit solver); name INF |
| `FUN_141E532D0` | `0x141E532D0` | `movss xmm0,[0x14351AE54]; ret`: fps = 54.0 (a float) | CONFIRMED bytes; `pes-re-first-touch.md` cites `0x14351AE50`, an int 54 at a different word |
| `FUN_140A6CF70` | `0x140A6CF70` | `k < 250`: `BI + 0x88 + 40k`; else `BI+0x2770` (sample 249). Each sample is floats (x, y = height, z) | CONFIRMED decompile |
| `FUN_140A57650` | `0x140A57650` | `p == 0`: `sqrt(vx²+vy²+vz²) / fps` at `BI+0x2948/0x294C/0x2950`; `p != 0`: the XZ distance of samples `p` and `p+1`. The issuer uses `p = 1` | CONFIRMED decompile |
| `FUN_14042D470` | `0x14042D470` | `v * fps * 60 * 60 / 1000` (constants `0x14259A764` = 60.0 and `0x14256AE34` = 1000.0): `v` in m per frame to km/h | CONFIRMED bytes of the constants; the 16.0 gate is km/h |
| `FUN_140A57710` | `0x140A57710` | `lea rax,[BI+0x2944]; ret`: a pointer to the ball's angle float, which `FUN_14043A020` reads as degrees | CONFIRMED bytes; the field's writer is OPEN |
| `FUN_140439E50` | `0x140439E50` | `out` = the bearing from A to B in degrees, `[0, 360)`: `FUN_140A92D40(B.x - A.x, ...) * 57.2957763671875` (`0x14259BE40`), wrapped by `360.0` (`0x14259858C`) | CONFIRMED decompile; the atan argument order inside `FUN_140A92D40` is OPEN |
| `FUN_1404583B0` | `0x1404583B0` | `\|wrap180(*b - *a)\|`: the unsigned angle between `a` and `b` (wrapped by `±360`; the abs mask is `0x7FFFFFFFFFFFFFFF` at `0x142565D30`) | CONFIRMED decompile and mask |
| `FUN_14043A020` | `0x14043A020` | `\|wrap180(*p - fold360(h))\|`: the unsigned angle between heading `h` and `*p`, with the same abs mask | CONFIRMED decompile and mask. `pes-re-team-mode.md` gives it as signed; the mask makes it unsigned |
| `FUN_1406B9E50` | `0x1406B9E50` | class `0..3` (and not class 0 with kind 3 or 6): `DAT_1425DDE10[class]`, four floats; else `0.0`. Called as `+0xAC0` slot `+0x38` | CONFIRMED bytes; the table VA is the RIP operand at `0x1406B9E68`, and it is the port's `BAND_RATE_VA` (`movestep_run.cpp:151`) |
| `FUN_1406EADF0` | `0x1406EADF0` | `movss xmm0,[AP+0x377C]`: `rateMax` | CONFIRMED bytes; `pes-re-anim-select.md` §3.5 |
| `FUN_141EB51A0` | `0x141EB51A0` | `out = 0`; if `ctx+0xA04 != 0`, `idx < 0x13`, and `DAT_14351E540[idx] != 0`: `out` = the three floats at `ctx+0x538+12*idx` | CONFIRMED decompile; the `ctx` name is OPEN; callers `FUN_1406B8340`, `FUN_1406C3470`, `FUN_1406D2590` |
| `FUN_1404BCAD0` | `0x1404BCAD0` | rotates `(v[0], v[2])` in place by `angle` (degrees, wrapped to `[0, 360)`), using `FUN_140A92C80(angle + 90, 1.0)` and `FUN_140A92C80(angle, 1.0)` | CONFIRMED decompile; the sign of `FUN_140A92C80` is OPEN; callers `FUN_1406C3470`, `FUN_1406CE820` |

### §5h.5 Corrections and open items

- The §5f "motion number" for word `0x37` and byte `0x50` is the style code (top of this section).
- The 16.0 gate is **km/h**, not m/s (corrected in §5b's units paragraph). `FUN_14042D470` multiplies by `fps * 60 * 60 / 1000`, so §5b and §5c.2 read km/h after this correction.
- `FUN_14043A020` and `FUN_1404583B0` are **unsigned** (absolute) angle differences.
- The stamina test compares the **low byte** of `FUN_140A58550` with 20 (`CMP AL,0x14`).
- OPEN: the atan argument order in `FUN_140A92D40` and the rotation sign in `FUN_140A92C80`; the name of the record `R = *(G+0x1A8)`; the consumers of `AP+0x3AC0` (slot `+0x00`) and `AP+0x3B18`; the writer of the ball angle at `BI+0x2944`; the `ctx` of `FUN_141EB51A0`.

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
   overrides and the `0x561..0x566` window. The later rows `0x56F`/`0x570`/`0x571`/`0x573`/
   `0x575` have their own conditions, reduced in **§5b**.
   **The turn-family remap acts on the COLUMN, not the bucket** (§5b's correction (a)), and
   its row list is **`0x5BF`/`0x5C1`/`0x5C3` only** (§5b's correction (b)).
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
3. **The later swing rows** `0x56F`/`0x570`/`0x571`/`0x573`/`0x575` — **CLOSED** in §5b:
   each row's condition is reduced, with the registers traced to their sources. Two of the
   §5b gates still name a local the report does not give a PES name: the float at
   `[RBP+0xB8]` (the "1.1 >" / "0.7 >" gate) and `XMM15` (the "16.0 >" / "XMM7 >" gate).
   `XMM15` is `FUN_14042D470`'s return; `[RBP+0xB8]` is `FUN_140A8F860`'s return. What those
   two functions compute is **OPEN** — the gates themselves are CONFIRMED.
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
  -> the 14-entry remap jump table checked (6 remapped columns)
  -> the row mask and the row list checked
  -> the five later rows' constants checked
  -> the 12 §5b gate constants checked
  -> the §5b textual claims checked
  cited code VAs not inside a disassembled instruction: 0
  TOTAL mismatches: 0
  ```
  **Mutation-tested in the previous pass (its script was not kept, so the list is not re-run here):**
  value; the clip table's pointer; the `row * 9` stride; the special cell's fixed motion; the
  mirror byte's stride; the column bound; the row bound; the empty cell's `-1`; the entry's
  column source; the swing's column source; the `0x29` mask; the window's allowed rows; the
  empty cell's fallback handler; the swing's column write; the "the swing rows are hard-coded"
  claim; the "no `& 7` mask" claim; "the remap acts on the COLUMN"; the row list
  `0x5BF`/`0x5C1`/`0x5C3`; "`0x570` is the fall-through of the `0x56F` block"; the `0x575`
  0.8 gate; the `0x573` 67.5 gate; the `0x573` column list; the `0x56F` byte gate; the
  `0x56F` class gate; the `0x575` purpose; the 1.1 m gate; the 0.7 m gate; the 16 m/s
  threshold; the units; the `vt+0xD0` function; the stop-row table; the "nothing fits"
  answer; and the column table's name. Seven more cover the §5d correction: the class
  word; the class clamp; the override's subject; a class-3 stop-row value; the entry
  and the "the class is not `req[9]`" line. Nine more cover §5e: the goto stop arm's
  class values; the class-3 rows are live; the retraction of the port claim; the copy
  writes; the class-3 count; the RUN_TO_STEP copy write; and three per-function
  class-3 line lists. Twelve more cover §5f: the function's identity; the 0x98 exclusion;
  the foot flag's byte; the foot argument; the 0 argument; the issuer; the two interfaces;
  the port's stand-in motion; the blend's clamp; and the §5f constants.
- **§5g and §5h** are checked by `check_runtturn.py` (claims, byte pins, RIP operand targets, constants)
  and mutation-tested by `mut_runturn.py`: 27 mutations (21 on the report, 6 on the checker's tables),
  each must fire. All 27 fire. The earlier §5g battery (the three-case rule and its "dead branch" claim)
  was replaced.
- The port's own `movestep.hpp` / `movestep_run.cpp` were read to map the request word indices
  and to compare `GotoRowFor` and `Cell`.
- No GF code changed, no GF build/test run, no PES write/pin/input.
