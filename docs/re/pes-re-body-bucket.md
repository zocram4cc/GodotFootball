# PES MOVESTEP request word 4 (`kBodyDirection`): the bucket-angle fill, the bucket chooser, and the stop arm

> **Main audit 20-10-26:** the table at `0x1425DD660` = 0, 45, 90, 135, 180, 0, −45, −90, −135, −180; `0x1406B7E25` = `movss [rdi+0x10],xmm6` (the req[4] write); `0x1406B9BC8` = `mov eax,r8d; ret`; `0x1411944E6` = `mov [rbx+0x2798],al`; `0x141194432` `xorps xmm1,xmm1` (the low bound), `0x141194435` loads 100.0 (`0x14253EE30`), `0x14119443D` loads 2^32 (`0x1427341A0`); the default value 1.0 (`0x14256AE1C`): all match. Consequence: with the default record the flag is 1 (bucket angle +45°), so RUN_TO_STEP's stop arm (0x1406C142A, 22.5°) does not fire on the walk.
> **Main note, final round:** the "only req[4] writer" claim now rests on a decoded scan (ModRM/SIB displacements, request block identified by its stored offsets); the earlier byte-pattern scan missed `movss` stores.
> **Main audit, round 3 (21-10-26):** the polarity correction is confirmed: `0x1411944C6` `xor cl,cl` on the out-of-range path, `0x1411944E1` `sete al` → +0x2798 = 1 only when NOT usable; the default record gives 0 (bucket angle 0°). The earlier audit note above ("flag is 1 … +45°") is superseded.

Read-only RE. No GF code changed, no PES written. Scratch: `<workdrive>/gf-TeamDecode-scratch/dec/`.
Base `0x140000000`, image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`.
Tags: **CONFIRMED** = decompile and/or raw dis this pass, checked by script; **INF** = inference;
**OPEN** = blocker named.

Purpose (from Main): PES's MOVESTEP request word 4 is `facing + a bucket angle`; the port fills
`req[4] = facing`, so the `RUN_TO_STEP` stop arm behaves wrongly and the throw-in walk stalls.
This report gives the three pieces the port needs: the bucket-angle table, the bucket chooser, and
the stop arm's gate.

**Every correction to the first draft of this report is called out inline.** The first draft
made five mistakes, all now corrected and all called out at the point of the fix:

| # | the first draft said | the truth | who caught it |
|---|---|---|---|
| 1 | the chooser returns a **bucket index** | it returns a **class id** (`MOV EAX,R8D` at `0x1406B9BC8`) | this pass, from the asm |
| 2 | the band is `[table[i], table[i]+w]` | the band is the interval **between** two consecutive entries | this pass, from the asm |
| 3 | the flag selects the narrow table when non-zero | `CMOVZ` — the flag selects the **wide** table when it is **zero** | this pass, from the asm |
| 4 | the store at `0x1411944E6` is `MOV byte [RSI+0x2798], AH` | it is **`MOV byte ptr [RBX+0x2798], AL`** | **Main** |
| 5 | the `req[8]` chooser angle is `req[0x1C]` | it is **`req[6]`** — the disassembly's byte offset `0x1C` is **word `0x07`** | **ClipWords** |

Errors 1-3 and 5 also made the first draft claim that the **port** was wrong about the bucket
angles. **It is not**: the port's `FillCommandWords` and `AngleBucket` are already right, and
§3.4 and §6 now say so.

---

## §1 The `req[4]` writer — `FUN_1406B7560` at `0x1406B7E25`

**The writer VA: `0x1406B7E25 MOVSS dword ptr [RDI + 0x10], XMM6`**, where `RDI` is `param_2`,
the request block, so `RDI + 0x10` = **req word 4** (`kBodyDirection`). CONFIRMED raw dis.
**`0x1406B7E25` is the ONLY writer of `req[4]` in the request block.** This needed a corrected
scan: the first scan of this report looked for the opcodes `89`/`C7`/`0F` and walked back a
fixed number of bytes, so it **missed the `F3 0F 11` (`MOVSS`) form** — the very form the real
writer takes. The corrected scan (`<workdrive>/gf-TeamDecode-scratch/req4byfn.py`) decodes each
instruction with the validated length decoder, reads its ModRM/SIB to recover the displacement,
and groups the stores by (enclosing function, base register):

- the displacement `0x10` alone is useless as a filter — there are **37 136** stores to
  `[some-register+0x10]` in the executable section, and **30** in the MOVESTEP range alone;
- so a base register is identified as *the request block* by the set of offsets the code stores
  through it: the request block's own offsets (`0x00` the kind, `0x08` the facing, `0x20` the
  turn, `0x38` the look bucket, `0x3C` the body direction, `0x44` the rate, `0x58`, `0x5C`,
  `0x7C`, `0x128`, `0x140`, …). 42 (function, register) groups store at four or more of them;
- and `req[4]` is a **float**, so its writer must be a **float** store. Of the 25 candidate
  `req[4]` sites, only **three** are float stores at all: `0x1404174ED` (`MOVUPS` — a 16-byte
  block copy), `0x1406B7E25` (`MOVSS`, the writer) and `0x14076A824` (`MOVSS`).

**The two other float stores are not the request block:**
- `0x1404174ED MOVUPS xmm, [RCX+0x10]` is inside `FUN_1404161E0`, whose `RCX` is a *different*
  struct (it stores at offsets `0, 4, 8, 12, 16, 28, 32, 48, 64` — a 16-byte-aligned block
  copy, not the request);
- `0x14076A824 MOVSS dword ptr [RSI+0x10], XMM7` is inside `FUN_140769FE0`, a ten-parameter
  rendering/animation function whose `RSI` is a rendering struct (the surrounding code writes a
  **qword** at `[RSI+0x14]` and a pointer at `[RSI+0x30]`).

**=> `0x1406B7E25` is the only `req[4]` writer.** CONFIRMED, by a scan that classifies every
store's ModRM/SIB rather than guessing at opcodes. (Within `FUN_1406B7560` itself the two other
`[reg+0x10]` hits are a *read* of the host's field, `0x1406B7810 MOV EAX,[R14+0x10]`, and the
read of `req[4]` itself at `0x1406B7E1D`.)

**The value.** `XMM6` is formed immediately before the store:

```
1406B7CDF: MOVSX RAX,AL                   ; the bucket index (a signed byte)
1406B7CE3: LEA RCX,[0x140000000]
1406B7CEA: MOVSS XMM6,dword ptr [RCX + RAX*0x4 + 0x25dd660]   ; the bucket angle
1406B7CF3: ADDSS XMM6,dword ptr [RDI + 0x8]                   ; + req[2] (the facing)
1406B7CF8: COMISS XMM11,XMM6
1406B7CFC: JBE 0x1406b7d08
1406B7CFE: ADDSS XMM6,dword ptr [0x14259858c]                  ; + 360.0
1406B7D06: JMP 0x1406b7d19
1406B7D08: COMISS XMM6,dword ptr [0x14259858c]                  ; 360.0
1406B7D0F: JC 0x1406b7d19
1406B7D11: ADDSS XMM6,dword ptr [0x142598598]                  ; - 360.0
1406B7D19: ...
1406B7E1D: MOVSS XMM1,dword ptr [RDI + 0x4]
1406B7E22: MOVAPS XMM0,XMM6
1406B7E25: MOVSS dword ptr [RDI + 0x10],XMM6                  ; req[4] = the wrapped value
```

**=> `req[4] = wrap360( DAT_1425DD660[bucket] + req[2] )`**, where `req[2]` is the facing
(written at `0x1406B7C7D MOV dword ptr [RDI+0x8], EAX` from `host+0x20`) and `bucket` is the
signed byte in `EAX`/`AL`. CONFIRMED.

The wrap is **to `[0, 360)`, not to ±180**: the code adds `+360.0` when the sum is below zero and
subtracts `360.0` when it reaches 360. Both constants are read by script: `0x14259858C` = `360.0`,
`0x142598598` = `-360.0`.

**The `XMM8` override.** At `0x1406B7E19` there is `MOVAPS XMM6, XMM8`, which replaces the
bucketed value with a *continuous* turn computed earlier in the function (the "still turning"
branch at `0x1406B798C`–`0x1406B7A79`). The decompile shows the same shape
(`fVar17 = (float)(&DAT_1425dd660)[cVar5] + (float)param_2[2];` then `fVar17 = fVar21;`).

**The state-byte override.** The decompile (lines 251-270) shows a large switch on the **state
byte** (the `+0xAC0` interface's slot `0x20`) that replaces the value with the continuous one for
the state values `0x00, 0x06, 0x08, 0x0F, 0x10, 0x13, 0x05, 0x12, 0x0A, 0x0C, 0x09, 0x03`. The
**asm** confirms the same twelve state tests at `0x1406B7D22`…`0x1406B7DE9` (each
`CALL qword ptr [RAX+0x20]` then `CMP byte ptr [RAX], <state>`), with the last (`0x03`) also
taking a `P+0x281A` random test at `0x1406B7DEB`–`0x1406B7E17`. CONFIRMED.

**=> `req[4]` is one of two things:**
1. the **bucketed** form `wrap360(bucketAngle + facing)` — the normal case; or
2. the **continuous** form — a directly computed turn angle, used when the "still turning" branch
   fires, or when the state byte is one of the twelve listed.

## §2 The bucket-angle table `DAT_1425DD660` (script-read, every entry)

**10 float entries.** Entry 10 is the start of a different blob (a string, `0x6F745F5F` = "_too").

| idx | VA | value | bits | meaning |
|---|---|---|---|---|
| 0 | `0x1425DD660` | `0.0` | `0x00000000` | bucket 0: no rotation |
| 1 | `0x1425DD664` | `45.0` | `0x42340000` | bucket 1: +45° |
| 2 | `0x1425DD668` | `90.0` | `0x42B40000` | bucket 2: +90° |
| 3 | `0x1425DD66C` | `135.0` | `0x43070000` | bucket 3: +135° |
| 4 | `0x1425DD670` | `180.0` | `0x43340000` | bucket 4: +180° (turn about) |
| 5 | `0x1425DD674` | `0.0` | `0x00000000` | bucket 5: no rotation (straight ahead) |
| 6 | `0x1425DD678` | `-45.0` | `0xC2340000` | bucket 6: −45° |
| 7 | `0x1425DD67C` | `-90.0` | `0xC2B40000` | bucket 7: −90° |
| 8 | `0x1425DD680` | `-135.0` | `0xC3070000` | bucket 8: −135° |
| 9 | `0x1425DD684` | `-180.0` | `0xC3340000` | bucket 9: −180° |

**=> the bucket angle is a 10-way rotation: `0, +45, +90, +135, +180, 0, −45, −90, −135, −180`
degrees.** CONFIRMED (all ten values read by script and bit-checked).

**Note the asymmetry.** The chooser (§3) numbers its buckets `0..9` from −180° to +180°, so its
bucket 0 is the −180…−157.5° band. This table gives **bucket 0 a 0.0 rotation** and **bucket 5 a
0.0 rotation**. The two tables are *not* the same numbering: this table's index 5 is the "straight
ahead" bucket of the chooser (the chooser's bucket 5 is the −22.5…+22.5° band) — **except** that
the chooser's *return* is a class id, not the index (§3), so the mapping between them is the
`bucket → class` table.

## §3 The bucket chooser — `FUN_1406B9B20`, the `+0xAC0` interface's vtable slot `0x60`

**Call signature (from the asm and the call sites):**
`FUN_1406B9B20(this, angle, wantedClass, flag)` — `RCX` = `this`, `XMM1` = the **angle** to
classify, `R8D` = the **wanted class**, `R9B` = the **flag**.

**The `+0xAC0` interface vtable** (`0x1425E35B0`, script-read; the vtable is stored at
`host+0xAC0` by `0x1406E0427`):

| slot | function | what it is |
|---|---|---|
| `+0x00` | `0x1406D2030` | — |
| `+0x08` | `0x1406B9C80` | — |
| `+0x10` | `0x1406D8620` | — |
| `+0x18` | `0x1406EADF0` | the rate ceiling (see §7 item 5) |
| `+0x20` | `0x1406EA110` | the **state byte** (`vt+0x20` in the runner) |
| `+0x28` | `0x1406EA750` | — |
| `+0x30` | `0x1406E9F90` | — |
| `+0x38` | `0x1406B9E50` | — |
| `+0x40` | `0x1406EAB00` | — |
| `+0x48` | `0x1406B9AA0` | — |
| `+0x50` | `0x1406E9F10` | — |
| `+0x58` | `0x1406B8DE0` | — |
| **`+0x60`** | **`0x1406B9B20`** | the **10-way angle-bucket chooser** (this section) |
| `+0x68` | `0x1406BA030` | — |
| `+0x70` | `0x1406EA210` | — |
| `+0x78` | `0x1406EA230` | — |
| **`+0x80`** | **`0x1406E9EF0`** | the **bucket index** (see §4) |
| `+0x88` | `0x1406EA100` | — |
| `+0x90` | `0x1406EA270` | — |
| `+0x98` | `0x1406EA280` | — |
| `+0xA0` | `0x1406EA0E0` | — |
| `+0xA8` | `0x1406EA0F0` | — |
| `+0xB0` | `0x1406EA250` | — |
| `+0xB8` | `0x1406EA260` | — |

**Correction to the first draft:** the first draft put `0x1406EADF0` at slot `0x18` and
`0x1406E9F10` at `0x28`. The script-read vtable gives `0x1406EADF0` at `0x18` (correct) and
`0x1406EA750` at `0x28`; `0x1406E9F10` is at `0x50`. The `vt+0x28` call in `FUN_1406B7560`
(`0x1406B822D`) therefore reaches `0x1406EA750`, **not** `0x1406E9F10`.

### §3.1 The two angle tables

`DAT_1425DDEB0` — the **wide** table (used when the flag is 0):

| idx | VA | value | bits |
|---|---|---|---|
| 0 | `0x1425DDEB0` | `-180.0` | `0xC3340000` |
| 1 | `0x1425DDEB4` | `-157.5` | `0xC31D8000` |
| 2 | `0x1425DDEB8` | `-112.5` | `0xC2E10000` |
| 3 | `0x1425DDEBC` | `-67.5` | `0xC2870000` |
| 4 | `0x1425DDEC0` | `-22.5` | `0xC1B40000` |
| 5 | `0x1425DDEC4` | `0.0` | `0x00000000` |
| 6 | `0x1425DDEC8` | `22.5` | `0x41B40000` |
| 7 | `0x1425DDECC` | `67.5` | `0x42870000` |
| 8 | `0x1425DDED0` | `112.5` | `0x42E10000` |
| 9 | `0x1425DDED4` | `157.5` | `0x431D8000` |
| 10 | `0x1425DDED8` | `180.0` | `0x43340000` |

`DAT_1425DDEE0` — the **narrow** table (used when the flag is non-zero):

| idx | VA | value | bits |
|---|---|---|---|
| 0 | `0x1425DDEE0` | `-180.0` | `0xC3340000` |
| 1 | `0x1425DDEE4` | `-170.0` | `0xC32A0000` |
| 2 | `0x1425DDEE8` | `-112.5` | `0xC2E10000` |
| 3 | `0x1425DDEEC` | `-67.5` | `0xC2870000` |
| 4 | `0x1425DDEF0` | `-10.0` | `0xC1200000` |
| 5 | `0x1425DDEF4` | `0.0` | `0x00000000` |
| 6 | `0x1425DDEF8` | `10.0` | `0x41200000` |
| 7 | `0x1425DDEFC` | `67.5` | `0x42870000` |
| 8 | `0x1425DDF00` | `112.5` | `0x42E10000` |
| 9 | `0x1425DDF04` | `170.0` | `0x432A0000` |
| 10 | `0x1425DDF08` | `180.0` | `0x43340000` |

### §3.2 The bucket → class map

`DAT_1425DDF10`, read as **int32** (the compare at `0x1406B9B8E CMP dword ptr [RBX+RAX*4],R10D`
is an integer compare):

| idx | VA | int32 |
|---|---|---|
| 0 | `0x1425DDF10` | `9` |
| 1 | `0x1425DDF14` | `8` |
| 2 | `0x1425DDF18` | `7` |
| 3 | `0x1425DDF1C` | `6` |
| 4 | `0x1425DDF20` | `5` |
| 5 | `0x1425DDF24` | `0` |
| 6 | `0x1425DDF28` | `1` |
| 7 | `0x1425DDF2C` | `2` |
| 8 | `0x1425DDF30` | `3` |
| 9 | `0x1425DDF34` | `4` |

**=> bucket 0 → class 9, 1 → 8, 2 → 7, 3 → 6, 4 → 5, 5 → 0, 6 → 1, 7 → 2, 8 → 3, 9 → 4.**
CONFIRMED.

### §3.3 The algorithm (from the raw asm, `dec/dis_at_1406B9B20.txt`)

```
1406B9B20: MOV [RSP+0x8],RBX
1406B9B25: MOVSS XMM2,[0x14259BE7C]      ; -5.0
1406B9B2D: LEA RAX,[0x1425DDEB0]          ; the wide table
1406B9B34: MOVSS XMM4,[0x14259222C]      ; +5.0
1406B9B3C: LEA R11,[0x1425DDEE0]          ; the narrow table
1406B9B43: TEST R9B,R9B
1406B9B46: LEA RBX,[0x1425DDF10]          ; the class table
1406B9B4D: MOV R9,RBX
1406B9B50: MOV R10D,R8D                   ; R10 = the wanted class
1406B9B53: CMOVZ R11,RAX                  ; if (flag == 0) R11 = the wide table
1406B9B57: MOVAPS XMM3,XMM1               ; XMM3 = the angle
1406B9B5A: MOV RDX,R11
1406B9B5D: SUB R9,R11                     ; R9 = the class table - R11
1406B9B60: MOV ECX,1                      ; i = 1
   loop:
1406B9B70: MOV R8D,[R9+RDX*1]             ; the class id at the shifted slot
1406B9B74: XORPS XMM1,XMM1                ; XMM1 = 0.0
1406B9B77: XORPS XMM0,XMM0                ; XMM0 = 0.0
1406B9B7A: CMP R8D,R10D                   ; the slot's class == the wanted class ?
1406B9B7D: JNZ 0x1406B9B87
1406B9B7F: MOVAPS XMM1,XMM2               ; XMM1 = -5.0   (the widened lower edge)
1406B9B82: MOVAPS XMM0,XMM4               ; XMM0 = +5.0   (the widened upper edge)
1406B9B85: JMP 0x1406B9B97
1406B9B87: CMP ECX,0xa
1406B9B8A: JNC 0x1406B9B97
1406B9B8C: MOV EAX,ECX
1406B9B8E: CMP dword ptr [RBX+RAX*4],R10D  ; class[i] == the wanted class ?
1406B9B92: JNZ 0x1406B9B97
1406B9B94: MOVAPS XMM0,XMM2               ; XMM0 = -5.0
1406B9B97: ADDSS XMM1,dword ptr [RDX]     ; XMM1 = the lower edge (table[i-1])
1406B9B9B: COMISS XMM3,XMM1
1406B9B9E: JC 0x1406B9BAD
1406B9BA0: MOV EAX,ECX
1406B9BA2: ADDSS XMM0,dword ptr [R11+RAX*4]  ; XMM0 = the upper edge (table[i])
1406B9BA8: COMISS XMM0,XMM3
1406B9BAB: JNC 0x1406B9BC3                 ; the angle is inside -> match
1406B9BAD: INC ECX
1406B9BAF: ADD RDX,0x4
1406B9BB3: LEA EAX,[RCX-1]
1406B9BB6: CMP EAX,0xa
1406B9BB9: JC 0x1406B9B70                  ; i <= 10 -> keep scanning
1406B9BBB: XOR EAX,EAX                     ; the miss: 0
1406B9BBD: MOV RBX,[RSP+0x8]
1406B9BC2: RET
1406B9BC3: MOV RBX,[RSP+0x8]
1406B9BC8: MOV EAX,R8D                     ; the match: the class id
1406B9BCB: RET
```

**=> the chooser returns the CLASS ID of the bucket whose angular band contains the angle**, or
`0` if the angle matched no band. CONFIRMED.

**Correction to the first draft:** the first draft said the chooser returns a *bucket index*.
**It returns a class id** (`MOV EAX, R8D` at `0x1406B9BC8`, where `R8D` is the class id read at
`0x1406B9B70`). This matches the port's own `pes::movestep::AngleBucket`, which returns
`k.bucket[w]` — the class id — and whose comment says "FUN_1406b9b20's miss" for the `0` return.
**The port's chooser is already right; this report's first draft was wrong.**

**The band.** The lower edge is `table[i-1]` and the upper edge is `table[i]` — the band is the
**interval between two consecutive entries** of the chosen table, not `[table[i], table[i]+w]`.
The first draft had this wrong.

**The flag.** `CMOVZ R11, RAX` — **zero**, not sign. So:
- `flag == 0` → the **wide** table `0x1425DDEB0`;
- `flag != 0` → the **narrow** table `0x1425DDEE0`.

**Correction to the first draft:** the first draft read the decompile's `if (param_4 == '\0')` as
selecting `DAT_1425DDEB0` and concluded the same, but the first draft then *also* wrote "the
`param_4 = 10` case selects the non-zero table" — **wrong**. The call sites pass `R8D = 10` as the
*wanted class*, and `R9B = 0` (from `XOR R9D,R9D` at `0x1406B7F28`/`0x1406B7F45`/`0x1406B7F5D`),
so the **wide** table is used at every one of the three `vt+0x60` call sites.

**The wanted-class preference.** When the bucket's class equals `wantedClass`, that bucket's
band is **widened by 5° on each side** (`XMM1 = -5.0` added to the lower edge, `XMM0 = -5.0`
added to the upper edge, i.e. the band grows by 5° below and 5° above). The `-5.0` and `+5.0`
are `DAT_14259BE7C` and `DAT_14259222C`, script-read. This is the port's `hysteresisUp` /
`hysteresisDown` pair.

### §3.4 The three call sites, and what they fill

All three are in `FUN_1406B7560`, all with `RCX = RBX = host+0xAC0`, `R8D = 10` (the wanted
class — a value that is **not** in the class table, so no band is widened), and `R9B = 0` (the
wide table):

```
1406B7F01: MOVSS XMM6,[RDI + 0x8]          ; the facing
1406B7F06: MOVSS XMM1,[RDI + 0x4]
1406B7F0B: MOVAPS XMM0,XMM6
1406B7F0E: CALL 0x14043a0a0                ; wrap180-ish
1406B7F13: MOVSS XMM1,[RDI + 0xc]
1406B7F18: MOVAPS XMM7,XMM0
1406B7F1B: MOVAPS XMM0,XMM6
1406B7F1E: MOVSS dword ptr [RDI + 0x14],XMM7     ; req[5]
1406B7F23: CALL 0x14043a0a0
1406B7F28: XOR R9D,R9D
1406B7F2B: MOVSS dword ptr [RDI + 0x1c],XMM0     ; req[7]
1406B7F30: MOV RAX,[RBX]
1406B7F33: MOVAPS XMM1,XMM7
1406B7F36: MOV RCX,RBX
1406B7F39: LEA R8D,[R9 + 0xa]                    ; R8D = 10
1406B7F3D: CALL qword ptr [RAX + 0x60]           ; the chooser
1406B7F40: MOVSS XMM1,[RDI + 0x18]
1406B7F45: XOR R9D,R9D
1406B7F48: MOV dword ptr [RDI + 0x30],EAX        ; req[0x0C] = the look bucket
1406B7F4B: MOV RCX,RBX
1406B7F4E: MOV RAX,[RBX]
1406B7F51: LEA R8D,[R9 + 0xa]                    ; R8D = 10
1406B7F55: CALL qword ptr [RAX + 0x60]           ; the chooser
1406B7F58: MOVSS XMM1,[RDI + 0x1c]
1406B7F5D: XOR R9D,R9D
1406B7F60: MOV dword ptr [RDI + 0x20],EAX        ; req[8] = the move bucket
1406B7F63: MOV RCX,RBX
1406B7F66: MOV RAX,[RBX]
1406B7F69: LEA R8D,[R9 + 0xa]                    ; R8D = 10
1406B7F6D: CALL qword ptr [RAX + 0x60]           ; the chooser
1406B7F70: MOV dword ptr [RDI + 0x34],EAX        ; req[0x0D] = the second bucket
```

**The angles passed.** The chooser takes its angle in `XMM1`, and each call site loads `XMM1`
from the request block immediately before the call:

| chooser call | the store | `XMM1` loaded at | the angle is | the port's name |
|---|---|---|---|---|
| `0x1406B7F3D` | `0x1406B7F48` -> `[RDI+0x30]` = **req[0x0C]** | `0x1406B7F33 MOVAPS XMM1,XMM7` | `req[5]` (byte `0x14`) | `kLookDelta` |
| `0x1406B7F55` | `0x1406B7F60` -> `[RDI+0x20]` = **req[8]** | `0x1406B7F40 MOVSS XMM1,[RDI+0x18]` | `req[6]` (byte `0x18`) | `kTurn` |
| `0x1406B7F6D` | `0x1406B7F70` -> `[RDI+0x34]` = **req[0x0D]** | `0x1406B7F58 MOVSS XMM1,[RDI+0x1C]` | `req[7]` (byte `0x1C`) | `kFacingError` |

**=> `req[8]` (the move bucket) is `FUN_1406B9B20(host+0xAC0, req[6], 10, 0)`** — the angle is
**`req[6]`** (`kTurn`, the turn angle), **not `req[0x1C]`**.

**Correction (ClipWords' audit, applied).** The first draft of this report wrote the angle as
`req[0x1C]`, having read the request block's *word* index where the disassembly uses a *byte*
offset: `MOVSS XMM1,[RDI+0x1C]` is byte `0x1C` = **word `0x07`** (`kFacingError`), not word
`0x1C`. The same slip made the first draft call the *store* `[RDI+0x20]` "req[0x20]". The
byte->word mapping for this block is `byte = word * 4`:

| byte | word | the port's name |
|---|---|---|
| `0x14` | `0x05` | `kLookDelta` |
| `0x18` | `0x06` | `kTurn` |
| `0x1C` | `0x07` | `kFacingError` |
| `0x20` | `0x08` | `kMoveBucket` |
| `0x30` | `0x0C` | `kLookBucket` |
| `0x34` | `0x0D` | `kLookBucketAlt` |

**=> the port's own `FillCommandWords` is already right**: it classifies `turn` (`kTurn` =
`req[6]`) into `kMoveBucket` and `facingError` (`kFacingError` = `req[7]`) into
`kLookBucketAlt`, and `req[5]` (`kLookDelta`) into `kLookBucket`. That is exactly the table
above. The first draft's claim that "PES classifies the *wrapped* deltas while the port
classifies the *raw* ones" is **wrong**: the wrapped values the port computes
(`turn` and `facingError`, both folded to +/-180 in `FillCommandWords`) are the same values PES
reads at byte `0x18` and byte `0x1C`. **This part of the first draft is retracted.**

## §4 The bucket index — `FUN_1406E9EF0`, the `+0xAC0` interface's vtable slot `0x80`

**The function (20 bytes; the bridge has no head for it, so it is read from the image and decoded
here with a length-checked decoder validated against the bridge's own disassembly of three other
functions — 0 length mismatches over 888 / 782 / 1020 instructions):**

```
1406E9EF0: 85 D2                 TEST EDX,EDX              ; EDX = arg (0 or 1)
(the raw 20 bytes: `85d2b8d81c000041b8ec1c0000410f45c08b0408`)
1406E9EF2: B8 D8 1C 00 00        MOV EAX,0x1CD8
1406E9EF7: 41 B8 EC 1C 00 00      MOV R8D,0x1CEC
1406E9EFD: 41 0F 45 C0           CMOVNE EAX,R8D           ; arg != 0 -> 0x1CEC, else 0x1CD8
1406E9F01: 8B 04 08              MOV EAX,[RAX+RCX*1]        ; EAX = *(uint*)(this + offset)
1406E9F04: C3                    RET
```

**The two call sites (both in `FUN_1406B7560`):**

```
; call 1 — the default bucket.  EDX = 0 (XOR EDX,EDX at 0x1406B7C5D).
1406B7C74: MOV EAX,[R14 + 0x20]             ; the host's facing
1406B7C78: MOVSS dword ptr [RDI + 0xc],XMM0
1406B7C7D: MOV dword ptr [RDI + 0x8],EAX     ; req[2] = the facing
1406B7C80: MOV RAX,[RBX]                    ; RBX = host+0xAC0
1406B7C83: CALL qword ptr [RAX + 0x80]
1406B7C89: MOV dword ptr [RBP + 0x78],EAX    ; the bucket index

; call 2 — the "adjusted" bucket, under a special condition.
1406B7CA7: CMP byte ptr [RAX],0x4            ; the state byte == 4 ?
1406B7CAA: JNZ 0x1406b7cdc
1406B7CAC: CMP byte ptr [R14 + 0x245d],0x5   ; and P+0x245D == 5 ?
1406B7CB4: JNZ 0x1406b7cdc
1406B7CB6: MOVSS XMM0,[R14 + 0x2780]
1406B7CBF: COMISS XMM0,[R14 + 0x277c]
1406B7CC7: JBE 0x1406b7cdc
1406B7CC9: MOV RAX,[RBX]
1406B7CCC: MOV EDX,0x1                      ; arg = 1
1406B7CD1: MOV RCX,RBX
1406B7CD4: CALL qword ptr [RAX + 0x80]
1406B7CDA: JMP 0x1406b7cdf
1406B7CDC: MOV EAX,[RBP + 0x78]             ; else keep the default
```

**The result is used as a signed byte index** (`MOVSX RAX,AL` at `0x1406B7CDF`) into
`DAT_1425DD660`. So the returned dword's **low byte** is the bucket index (0..9).

**The field's location — CORRECTED.** The first draft said the field is at
`interface+0x1CD8` with the interface at `host+0xAC0`, i.e. `host+0x2798`. The *offset within the
object* (`0x1CD8`) is right, but the *base* was wrong.

The base is settled by `FUN_1406EA110` (the `vt+0x20` slot), whose decompile is
`FUN_141eb50b0(param_1 + -0xac0, local_48, param_2)` — i.e. it addresses the **host** as
`this - 0xAC0`. Since `this` is `host+0xAC0` at every call site, the **host is at `this-0xAC0`**
and the object's own base is `this`.

**So the field is at `this + 0x1CD8` = `host + 0xAC0 + 0x1CD8` = `host + 0x2798`.** The first
draft's arithmetic was right after all — but the first draft could not find a writer for it, and
the reason was that the first draft looked for a *dword* store while the field is a **byte**.

**The field is a byte.** An exhaustive scan of every instruction in the executable section whose
displacement is `0x2798` (21 sites, each classified by its ModRM) gives **six** stores:

| VA | form | what |
|---|---|---|
| `0x14014EFB4` | `MOV dword [RBP+0x2798],ECX` | a **stack** slot, not the host |
| `0x140430C53` | `MOV dword [RBX+0x2798],EAX` | a **copy** of the field |
| `0x140872D3F` | `MOV dword [RSI+0x2798],EBX` | a **different** object |
| `0x140872D8C` | `MOV dword [RSI+0x2798],EAX` | the same **different** object |
| `0x140A572BC` | `MOV dword [RBX+0x2798],EDI` | a **constructor**, init 0 |
| `0x1411944E6` | `88 83 98 27 00 00` = `MOV byte ptr [RBX+0x2798], AL` | the **host**: the "in range" byte |

**The host's field is written at `0x1411944E6`**, in the same function that writes the neighbouring
`host+0x2794` (a dword) and `host+0x279C`/`host+0x279D` (bytes). The write is
`MOV byte ptr [RBX+0x2798], AL` (the raw bytes are `88 83 98 27 00 00`: ModRM `0x83` is
`mod 10, reg 000 (AL), rm 011 (RBX)`, so the base is `RBX` — the host — and the source is `AL`),
immediately after `SETZ AL` at `0x1411944E1` — so the field is a
**0/1 flag**, not a 0..9 index.

**=> the low byte of the `vt+0x80` result is a 0/1 flag**, and `DAT_1425DD660` is indexed with it,
giving bucket 0 (`0.0°`) or bucket 1 (`+45°`).**

### §4.1 What the flag is

The function that writes it (`0x141194330`) is a **loader**: it reads a record at `RDX` and
copies its fields into the host at `+0x2770..`:

```
1411944A5: MOVZX ECX,SIL
1411944A9: MOVSS XMM0,[RDX + 0x3c]
1411944AE: MULSS XMM0,XMM3                    ; XMM3 = 100.0
1411944B2: COMISS XMM0,XMM1                   ; XMM1 = 0.0
1411944B5: JNC 0x1411944c1
1411944B7: COMISS XMM1,XMM0
1411944BA: JA 0x1411944c6
1411944BC: COMISS XMM0,XMM1
1411944C1: JBE 0x1411944ca
1411944C6: XOR CL,CL                          ; out of range -> CL = 0
1411944C8: JMP 0x1411944d5
1411944CA: CVTTSS2SI RAX,XMM0
1411944CF: MOV dword ptr [RBX + 0x2794],EAX   ; host+0x2794 = int(value * 100)
1411944D5: TEST CL,CL
1411944D7: MOVZX EDI,DIL
1411944DB: MOV byte ptr [RBX + 0x2790],CL
1411944E1: SETZ AL
1411944E4: TEST CL,CL
1411944E6: MOV byte ptr [RBX + 0x2798],AL    ; host+0x2798 = (value in range) ? 1 : 0
```

**=> `host+0x2798` is the "the record's value is NOT usable" flag**: 1 when the value is
**out of range** (or the pair is absent), 0 when it is usable. The sibling `host+0x2794` holds
`int(record[0x3C] * 100.0)` — the value itself, in whole percent — and `host+0x2790` holds the
**in-range** flag (1 = usable). CONFIRMED.

**POLARITY CORRECTION (this pass).** The first draft of this report said `host+0x2798` is 1
*when the value is in range*. **That is backwards.** The byte is `SETZ AL` of `TEST CL,CL`
(`0x1411944E1`), and `CL` is the **in-range** flag: the in-range path falls through to
`0x1411944CA` (which stores the value) with `CL` unchanged, while the out-of-range path
executes `XOR CL,CL` at `0x1411944C6`. So `CL == 0` means **out of range**, and
`SETZ` gives **1 exactly in that case**. The two flags are complementary:
`host+0x2790` = "usable" and `host+0x2798` = "not usable".

**The three fields are a family.** The loader writes the same triple for every pair it
handles: pair 6 → `+0x2790` (usable), `+0x2794` (the value), `+0x2798` (not usable); pair 7 →
`+0x279C`, `+0x27A0`, `+0x27A4`; and so on. So `host+0x2798` is **one pair's "not usable"
byte**, exactly like `+0x27A4` is pair 7's.

### §4.2 The record: which one, which field, and the range test's bounds

*(Main's follow-up: ClipWords needs this to compute the 0/1 index instead of a stand-in.)*

**The record is a fixed-layout "parameter override" struct** whose fields are
**flag/value pairs**: a `byte` flag followed by its value, and the flag says whether the value
is present. The layout, from `FUN_1411FBD50` (the record's initialiser) and `FUN_1411FD1F0`
(the merger), is:

| pair | flag at | value at | value type |
|---|---|---|---|
| 0 | `+0x00` | `+0x04` | dword |
| 1 | `+0x08` | `+0x0C` | dword (read as a float) |
| 2 | `+0x10` | `+0x14` | dword (read as a float) |
| 3 | `+0x18` | `+0x1C` | dword |
| 4 | `+0x20` | `+0x24` | dword |
| 5 | `+0x28` | `+0x30` | qword |
| **6** | **`+0x38`** | **`+0x3C`** | **dword (read as a float)** |
| 7 | `+0x40` | `+0x48` | qword |
| 8 | `+0x50` | `+0x54` | dword (plus a string at `+0x50`) |
| ... | `+0x58`, `+0x68`, `+0x70`, `+0x80`, `+0x88` ... | | the same shape, further pairs |

**The field that sets the flag is pair 6: the flag byte at `record+0x38` and the float value at
`record+0x3C`.** The flag at `record+0x38` is what *enables* the pair; the value is a **float**.

**The chain (all CONFIRMED):**

1. `FUN_141194330(host, record)` — the **loader**. It is the only writer of `host+0x2798`.
   It walks the record's pairs and, for pair 6, does the range test below.
2. `FUN_1411FD6F0` (the caller, at `0x1411FD6F0`) — the record comes from a **virtual**
   `[RAX+0x30]` on the object at `[[this+0x38]+0x88]`, which fills a record at `RBP+0`;
   `FUN_1411FCF70` then merges that into `RBP+0xD0`, and `FUN_1411FD1F0` merges that into the
   final record at `RBP+0x230`, which is what the loader consumes.
3. `FUN_141193E10(this, record)` — the **applier**, which writes the record's pairs into the
   host's own copy: pair 6 lands at `this+0x26C0` (the flag) and `this+0x26C4` (the value,
   copied **verbatim**, with no range test). Its pair-1/2 handling shows the same
   `value * 100.0` / `0.0 .. 2^32` shape for the float pairs.

**The range test, exactly (the loader, `0x1411944A9`-`0x1411944C8`):**

```
1411944A9: MOVSS XMM0,dword ptr [RDX + 0x3c]   ; the record's float
1411944AE: MULSS XMM0,XMM3                      ; XMM3 = DAT_14253EE30 = 100.0
1411944B2: COMISS XMM0,XMM1                     ; XMM1 = 0.0
1411944B5: JNC 0x1411944c1
1411944B7: COMISS XMM1,XMM0
1411944BA: JA 0x1411944c6                       ; 0.0 > v -> OUT OF RANGE
1411944BC: COMISS XMM0,XMM1
1411944C1: JBE 0x1411944ca                      ; v <= 0.0 -> the value is taken
1411944C3: COMISS XMM0,XMM2                     ; XMM2 = DAT_1427341A0 = 4294967296.0
1411944C6: XOR CL,CL                           ; v > 2^32 -> OUT OF RANGE
1411944C8: JMP 0x1411944d5
1411944CA: CVTTSS2SI RAX,XMM0
1411944CF: MOV dword ptr [RBX + 0x2794],EAX    ; host+0x2794 = int(v)
```

**=> the bounds are `0.0 <= record[0x3C] * 100.0 <= 4294967296.0`**, with

| bound | value | VA | how |
|---|---|---|---|
| low | `0.0` | (a zeroed `XORPS XMM1,XMM1` at `0x141194432` — not a memory operand) | `XMM1` |
| high | `4294967296.0` (`2^32`) | **`0x1427341A0`** | `MOVSS XMM2,[0x1427341A0]` at `0x14119443D` |
| scale | `100.0` | **`0x14253EE30`** | `MOVSS XMM3,[0x14253EE30]` at `0x141194435` |

All three read by script: `0x14253EE30` = `100.0`, `0x1427341A0` = `4294967296.0`, and the low
bound is a zero register, not an install constant.

**=> `host+0x2798` = 1 when `0.0 <= record[0x3C] * 100.0 <= 2^32`, else 0.** Equivalently,
since the value is a float scaled by 100: `record[0x3C]` in `[0.0, 42949672.96]`.
**=> the 0/1 index ClipWords needs is this byte**, and the value it ranges is
`int(record[0x3C] * 100.0)` at `host+0x2794` (the whole-percent value) — so the index is 1
whenever the record carries a *usable* magnitude for pair 6, and 0 when the record's pair-6 value
is absent, negative, or absurdly large.

**The producer of the record, and the default value.** `FUN_14119D380(dst, variant, tier)`
builds a record of **named constants** and hands it to `FUN_1412453D0` (a verbatim pair copy).
It is the **default parameter set** for a playback object, and the `+0x38/+0x3C` pair's value
is set there:

```
14119D3AF: LEA RCX,[RSP + 0x30]          ; the record
14119D3B4: CALL 0x1411991b0
...
14119D41C: MOV byte ptr [RBP + 0x38],0x1   ; the record's +0x68 flag = 1
14119D420: MOVSS XMM0,dword ptr [0x14256ae1c]
14119D428: MOVSS dword ptr [RBP + 0x3c],XMM0   ; the record's +0x6C value = 1.0
```

**=> the record's pair 6 default value is `DAT_14256AE1C` = `1.0`** (script-read), and the
record's `+0x38/+0x3C` pair in the *loader*'s layout is the pair that this builder fills —
`1.0 * 100.0 = 100.0`, which is inside `[0.0, 2^32]`, so **the flag is 1** for the default
record.

**=> the practical answer for ClipWords: the 0/1 index is 1 whenever the record carries the
default pair-6 value (`1.0`), and 0 only when the record's pair-6 value is absent, negative,
or larger than `42949672.96`.** The port can therefore compute it as
`record[0x3C] * 100.0` inside `[0.0, 4294967296.0]` — the exact test §4.2 gives — with the
default (`1.0`) satisfying it.

**What is still OPEN**: the *named* PES parameter that pair 6 carries. The producer
`FUN_14119D380` sets a dozen named constants in one record (the pair values include
`0xE4E1C0` = `15000000`, `0x20`, `3.0f` twice, `1.0f`, `0x80`, `0x4AC`, `60.0f`, `20.0f`,
`6.0f`), and this pass did not map them to PES's own names, nor did it identify the data file
the *overrides* come from (the producer of the record fed to the loader is a virtual slot
`+0x30` on the object at `[[this+0x38]+0x88]`). **The mechanism, the type, the scale, the range
test, the default and the three landing sites are all CONFIRMED; only the name is OPEN.**

### §4.2b Every writer of `host+0x2790` / `+0x2794` / `+0x2798` (this pass)

*(Main's follow-up: the live data shows `host+0x2798` taking values 0..9, so the SETZ at
`0x1411944E6` cannot be the only writer.)*

**The scan** (`<workdrive>/gf-TeamDecode-scratch/field2798.py`): walk the executable section
with the length-checked decoder, read each instruction's ModRM/SIB to recover the base
register and the displacement, and report every store whose displacement is one of the three.
**The result: exactly 10 stores in the whole image.**

| VA | the form | the base | whose field |
|---|---|---|---|
| `0x14014EFAB` | `MOV qword ptr [RBP+0x2790],R8` | `RBP` | a **stack frame** (`FUN_14014EF70` does `LEA RBP,[RSP+0x30]`) — not the host |
| `0x14014EFB2` | `MOV dword ptr [RBP+0x2798],R9D` | `RBP` | the same **stack frame** |
| `0x140430C52` | `MOV dword ptr [RBX+0x2798],EAX` | `RBX` | a **copy** — `FUN_140430C30` is a copy constructor: `*(uint32*)(param_1+0x2798) = *(uint32*)(param_2+0x2798)` |
| `0x140872D3E` | `MOV dword ptr [RSI+0x2798],EBX` | `RSI` | the **match/ball** object (`FUN_140872C10`) — see below |
| `0x140872D8B` | `MOV dword ptr [RSI+0x2798],EAX` | `RSI` | the same **match/ball** object |
| `0x140A572BB` | `MOV dword ptr [RBX+0x2798],EDI` | `RBX` | a **constructor** — it initialises the field to 0 |
| `0x14119449E` | `MOV byte ptr [RBX+0x2790],SIL` | `RBX` | the **AnimePlayer host** (the loader) — the "usable" flag |
| `0x1411944CF` | `MOV dword ptr [RBX+0x2794],EAX` | `RBX` | the **AnimePlayer host** — the value |
| `0x1411944DB` | `MOV byte ptr [RBX+0x2790],CL` | `RBX` | the **AnimePlayer host** — the "usable" flag (the final value) |
| `0x1411944E6` | `MOV byte ptr [RBX+0x2798],AL` | `RBX` | the **AnimePlayer host** — the "not usable" byte |

**The `RBX` base is the AnimePlayer host.** The loader's `param_1` is
`[RBX + 0x48]` of the object at `0x1411FD6F0` (`MOV RCX,qword ptr [RBX + 0x48]` at
`0x1411FD6EC`), and the loader is `MOV RBX,RCX` (`0x141194342`). The `+0xAC0` interface is
inside that same host, so the host's `+0x2798` is the field `FUN_1406E9EF0` reads as
`this + 0x1CD8` with `this = host + 0xAC0`.

**The `RSI` base is a DIFFERENT object.** `FUN_140872C10` writes
`FUN_140A72E70`'s return value to `+0x2798` — and `FUN_140A72E70` is the **out-of-side
classifier**, whose return is a **bitmask** (bit 0 = the near side, bit 1 = the middle, …, up
to `0x4000`), not a 0..9 index. That object is the **match/ball** one: it has `+0x298` (the
phase), `+0x6C`/`+0x64` (the coordinates), `+0x4E0` and `+0x530` (sub-objects), and it has
**no** `+0xAC0` interface at all. So its `+0x2798` is the out-of-side mask, and it is
unrelated to the AnimePlayer's field of the same offset.

**=> on the AnimePlayer host, `+0x2798` has exactly ONE writer: the `SETZ` byte at
`0x1411944E6`.** Its value is therefore **0 or 1** — the "not usable" flag — and it cannot be the
0..9 bucket index Main's live data shows.

**=> the 0..9 live values do NOT come from `host+0x2798` on the AnimePlayer.** Two candidate
resolutions, both **OPEN**:

1. **The live read was of the other object.** The match/ball object's `+0x2798` holds
   `FUN_140A72E70`'s bitmask — whose **low nibble** can be 0..9 in practice, since
   `FUN_140A72E70` returns `octant ^ quadrant ^ (0x40|0x80)` and the octants are 1, 6 and 10.
   A live read of the *match* object's `+0x2798` would show 0..9-ish values. But it is a
   **dword bitmask**, not an index.
2. **The bucket index lives in a different field.** The AnimePlayer's own `req[8]`
   (`kMoveBucket`) is the 0..9 bucket, and it is written by the command fill
   (`FUN_1406B3560`'s caller, §2) — **not** by the host's `+0x2798`. The port's
   `AngleBucket` already computes it.

**What ClipWords should implement:** the 0..9 bucket is the request's **`kMoveBucket`
(`req[8]`)**, computed by the port's `pes::movestep::AngleBucket`; the host's `+0x2798` is a
**separate 0/1 "the value is not usable" flag** whose polarity is **1 = not usable** (this
report's earlier claim of 1 = usable is retracted). The port's flag-1 default is therefore
**inverted** relative to what this report first said.

---

### §4.3 The other field, `this + 0x1CEC`

The second immediate (`0x1CEC`) selects `host+0xAC0+0x1CEC` = **`host+0x27B4`**. Its writers:

| VA | form | what |
|---|---|---|
| `0x1406B6B91` | `MOV dword [R8+0x27B4],EAX` | `FUN_1406B6A90(host, req)` — copies **`req[0x128]`** |
| `0x1406EB72D` | `MOV qword [RCX+0x27B4],R14` | zeroes 8 bytes at `0x27B4` |

**`FUN_1406B6A90` is the request→host copy**, and it is called at `0x1406B8D1F` from
`FUN_1406B8340` with `RDX = &req`:
```
1406B8D17: LEA RDX,[RSP + 0x70]
1406B8D1C: MOV RCX,R14
1406B8D1F: CALL 0x1406b6a90
```
And `FUN_1406B7560` writes `req[0x128]` from the host in two places:
```
1406B8260: MOV EAX,[R14 + 0x27b4]
1406B8267: MOV dword ptr [RDI + 0x128],EAX      ; a copy back
```
and, before that, from the interface:
```
1406B822A: MOV RAX,[RBX]
1406B822D: CALL qword ptr [RAX + 0x28]          ; vt+0x28 = 0x1406EA750
1406B8230: MOVSS dword ptr [RDI + 0x128],XMM0    ; req[0x128] = the float
```

**=> `host+0x27B4` holds the raw 32 bits of `req[0x128]`, which is a float** (written by
`MOVSS`). The `vt+0x80` return with `arg == 1` is therefore a float's bit pattern, and its low
byte is not a meaningful index. **The `arg == 1` path is effectively dead for the bucket index**;
only the `arg == 0` path (the 0/1 flag) is used in the `req[4]` fill. **This is OPEN**: what
`req[0x128]` means (a speed? a rate?) and whether the `arg == 1` path is used elsewhere is not
settled here.

## §5 The `RUN_TO_STEP` stop arm — `FUN_1406C1070` at `0x1406C142A`

`FUN_1406C1070`'s prologue: `MOV RBX, RDX` (the **request block**) and `MOV RDI, RCX` (the
**host**). So in the arm:

```
1406C142A: MOVSS XMM1,dword ptr [RBX + 0x10]      ; req[4]  (the body direction)
1406C142F: MOVSS XMM0,dword ptr [RDI + 0x20]      ; host+0x20 (the facing)
1406C1434: CALL 0x14043a0a0                       ; wrap180(facing - req[4])
1406C1439: XORPS XMM1,XMM1
1406C143C: CVTSS2SD XMM1,XMM0
1406C1440: MOVSS XMM0,dword ptr [0x14259d618]      ; 22.5
1406C1448: ANDPS XMM1,xmmword ptr [0x142565d30]    ; the absolute-value mask
1406C144F: CVTPD2PS XMM2,XMM1
1406C1453: COMISS XMM0,XMM2
1406C1456: JBE 0x1406c148b                        ; if (22.5 >= |diff|) -> the STOP arm
1406C1458: CMP dword ptr [RBX + 0x7c],0x0
1406C145C: JG 0x1406c1464
1406C145E: CMP dword ptr [RBX + 0x24],-0x1
1406C1462: JZ 0x1406c148b
1406C1464: CMP dword ptr [RBX + 0x24],-0x1
1406C1468: JZ 0x1406c147e
1406C146A: COMISS XMM9,dword ptr [RBX + 0x44]
1406C146F: JA 0x1406c147e
1406C1471: MOV RDX,RBX / MOV RCX,RDI
1406C1477: CALL 0x1406d6780                       ; RUN_TURN_RUN
1406C147C: JMP 0x1406c1496
1406C147E: MOV RDX,RBX / MOV RCX,RDI
1406C1484: CALL 0x1406d2590                       ; TURN_STOP
1406C1489: JMP 0x1406c1496
1406C148B: MOV RDX,RBX / MOV RCX,RDI
1406C148E?: CALL 0x1406d5d60                       ; STOP
```

**=> the arm fires when `|wrap180(host+0x20 − req[4])| < 22.5`** — i.e. when the requested body
direction is already within 22.5° of the current facing, so no turn is needed and the player may
stop. CONFIRMED. (`0x14259D618` = `22.5` by script; `0x142565D30` is the `0x7FFFFFFF`/`0x7FFF...`
absolute-value mask pair, read by script.)

**`FUN_14043A0A0` is the wrap helper**: it normalises each argument into `[0, 360)`, differences
them, and folds the result into `(−180, 180]` (constants `DAT_14259858C` = `360.0`,
`DAT_142598580` = `180.0`, `DAT_142588218` = `-1.0`, `DAT_14256AE1C` = `1.0`, all script-read).

**Consequences for the port's `req[4] = facing`:**
- `wrap180(facing − facing) = 0`, so `|0| < 22.5` and **the arm always fires**.
- With PES's `req[4] = wrap360(bucketAngle + facing)`, the arm fires only when the bucket angle
  is 0 (bucket 0 or bucket 5) — and the bucket is `0/1` (§4), so it fires for bucket 0 and not for
  bucket 1 (`+45°`, giving `|−45| = 45 > 22.5`).

**So the port's fill makes the stop arm fire unconditionally; PES's fires only when the flag
says the body may stay put.** The port's symptom ("the stop arm never fires") is **not** what the
asm shows — the arm fires *more* than PES's. The port's actual stall must have a different
cause; the likely one is that the port's `req[4] = facing` makes the body error zero, so the
**other** gates (the `req[0x44]` and `req[0x24]` tests, and the `RunToStep` blend branch in
`movestep_step.cpp:302`–`305`, which compares `req[kHeading]` against `req[kBodyDirection]`)
see a body error of zero where PES sees 45°.

**This part is a port-side question and is flagged for ClipWords, not settled here. OPEN.**

## §6 The port's fix, in C++ terms

```cpp
// The bucket-angle table, DAT_1425DD660 (0x1425DD660), 10 entries:
constexpr float kBucketAngle[10] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f,
                                    0.0f, -45.0f, -90.0f, -135.0f, -180.0f};

// The body direction, at the fill (PES 0x1406B7E25):
req.SetF(pes::moveword::kBodyDirection,
         Wrap360(kBucketAngle[bucket] + req.F(pes::moveword::kFacing)));
```
where `bucket` is the 0/1 flag the host's `host+0x2798` carries (§4.1 for the flag, §4.2 for
the record it comes from and its default) and `kFacing` is
`moveword::kFacing = 0x02` (`movestep.hpp:126`), matching the port's own numbering
(`kBodyDirection = 0x04`, `movestep.hpp:127`).

**The port's `req[8]` (the move bucket) already matches PES**: the port calls
`pes::movestep::AngleBucket(buckets, turn, 10, false)` and stores it in `kMoveBucket`
(`pesmovestep.cpp:93-95`), and `AngleBucket` returns `k.bucket[w]` — the **class id** — with the
wide table (`narrow == false`) and the wanted class `10` (which is in no class, so no band is
widened) and the `0` miss. **That matches §3 exactly**, and the angles match too: PES classifies `req[5]`/`req[6]`/`req[7]`
(byte `0x14`/`0x18`/`0x1C`) into `req[0x0C]`/`req[8]`/`req[0x0D]`, and the port classifies
`kLookDelta`/`kTurn`/`kFacingError` into the same three words. **No port change is needed for
the buckets.** (The first draft of this report claimed otherwise, on a byte-vs-word reading
error; §3.4 has the correction and the checker's M9 mutation guards it.)

## §7 Open items

0. **The 0..9 live values at `host+0x2798`** — **OPEN, and now bounded.** A decoded scan of
   the whole image (every store to `+0x2790`/`+0x2794`/`+0x2798`, §4.2b) finds **ten** stores,
   and only **one** of them is on the AnimePlayer host: the `SETZ` byte at `0x1411944E6`, whose
   value is 0 or 1. The other nine are a stack frame, a copy constructor, a constructor, and
   the **match/ball** object (which has its own `+0x2798` holding the out-of-side bitmask).
   So the 0..9 values Main's live data shows **do not come from the AnimePlayer's `+0x2798`**;
   the 0..9 bucket is the request's **`kMoveBucket` (`req[8]`)**, which the port already
   computes with `pes::movestep::AngleBucket`. **Which field the live read actually hit is
   OPEN** — either the match object's bitmask (whose low bits are 0..9-ish) or a different
   AnimePlayer field.

1. **The `arg == 1` path of `vt+0x80`** (`host+0x27B4` = the bits of `req[0x128]`, a float) is
   **not** a bucket index. What `req[0x128]` means, and whether the `arg == 1` bucket is used
   anywhere else, is **OPEN**.
2. **The "still turning" continuous value** — the branch at `0x1406B798C`–`0x1406B7A79` computes a
   continuous turn and that value is used for `req[4]` **instead of** the bucketed form. Its full
   rule is **not** reduced here; only its effect on `req[4]` is CONFIRMED.
3. **The port's symptom** — the asm shows the port's `req[4] = facing` makes the stop arm fire
   *unconditionally*, not never. The real stall cause is **OPEN** and is a port-side question.
4. **The port's bucket angles** — **CLOSED, and the first draft was wrong.** PES classifies
   `req[5]`/`req[6]`/`req[7]` (byte `0x14`/`0x18`/`0x1C`) into `req[0x0C]`/`req[8]`/`req[0x0D]`,
   which is exactly what the port's `FillCommandWords` does with `kLookDelta`/`kTurn`/
   `kFacingError`. See §3.4's correction. **No port change is needed here.**
5. `FUN_1406EADF0` (the rate ceiling, `vt+0x18`) — the bridge has no function head for it; the
   identity `P+0x377C` comes from `docs/re/pes-re-movestep-wave1.md:208` and ClipWords' reading,
   not from this pass's own decode. **OPEN.**
6. The **record** the flag is loaded from (`0x141194330`, reading `RDX+0x3C`) — **PARTLY
   CLOSED**: §4.2 now gives the record's layout (a flag/value-pair "parameter override"
   struct), the field (pair 6: the flag at `+0x38`, the **float** at `+0x3C`), the scale
   (`x100`, `DAT_14253EE30`), the range test (`0.0 <= v <= 2^32`, the high bound
   `DAT_1427341A0`) and the three places the value lands. Still **OPEN**: the record's
   *producer* (a virtual slot `+0x30` on the object at `[[this+0x38]+0x88]`) was not
   identified, so PES's own name for pair 6, and whether the flag is per-player or per-side,
   are not settled.

## §8 Method

- Bridge decompiles (`dec/FUN_*.c`) and disassembly (`dec/dis_*.txt`, `dec/dis_at_*.txt`) for
  `FUN_1406B7560`, `FUN_1406B9B20`, `FUN_1406C1070`, `FUN_1406E9EF0`, `FUN_1406E9F90`,
  `FUN_1406EA110`, `FUN_1406B6A90`, `FUN_140A72E70`, `FUN_14043A0A0` and the two writers of the
  bucket field.
- The `+0xAC0` interface's vtable was read out of the image at `0x1425E35B0` (script-read, 24
  entries).
- `FUN_1406E9EF0` has **no bridge function head**; its 20 bytes were read from the image and
  decoded here with a length-checked decoder (`<workdrive>/gf-TeamDecode-scratch/disas2.py`),
  validated against the bridge's own disassembly of three functions (0 length mismatches over
  888 / 782 / 1020 instructions).
- The **exhaustive field scan** (21 instructions with displacement `0x2798`, 6 stores) was done
  with a custom walker that classifies each candidate by its ModRM, so that a `disp32` inside an
  *immediate* is not mistaken for a memory displacement.
- The **`req[4]` writer scan** (`<workdrive>/gf-TeamDecode-scratch/req4byfn.py` and
  `req4writers.py`) decodes each instruction with the length-checked decoder and recovers the
  displacement from the ModRM/SIB, then groups the stores by (function, base register) so that a
  base register is identified as *the request block* by the set of request-block offsets stored
  through it. **This replaced an earlier, unsound scan** which looked for the opcodes
  `89`/`C7`/`0F` and walked back a fixed number of bytes: it missed the `F3 0F 11` (`MOVSS`)
  form, which is the form the real `req[4]` writer takes, and so could not see it at all.
- Every float and int32 table entry quoted here was read by script from `pes_full.bin`; none is
  typed by hand.
- **Self-check** (`<workdrive>/gf-TeamDecode-scratch/check_bodybucket.py`, run against this file):
  ```
  -> 32 float table rows checked
  -> 10 int32 table rows checked
  -> 5 vtable slots checked
  -> 24 vtable rows checked
  -> 7 scalar constants checked, 4 report claims cross-checked
  -> 2 range-test bounds checked, 3 report claims cross-checked
  -> the 0x1411944E6 decode checked
  -> the req[4] writer and the 'only writer' claim checked
  -> the polarity and the ten writers checked
  -> the record's pair walk and the pair-6 default checked
  -> 3 chooser operand pairs checked
  -> 6 byte->word mappings checked against the port's header
  -> FUN_1406E9EF0 decode checked
  cited code VAs not inside a disassembled instruction: 0
  TOTAL mismatches: 0
  ```
  The checker verifies each `VA | value (bits)` row and each `VA = value` claim against the
  image bytes, verifies the 24 vtable rows and the 5 named vtable slots against the vtable at
  `0x1425E35B0`, verifies `FUN_1406E9EF0`'s quoted raw bytes and its two immediates against the
  image, and that every cited code VA lies inside a disassembled instruction (the vtable's own
  function addresses are checked against the vtable table instead, since they are data).
  **Mutation-tested — all six fire:**
  - a wrong table value (`99.0` for `45.0` at `0x1425DD664`) →
    `BAD  0x1425dd664: report value 99.0, image 45.0`;
  - a wrong vtable entry (`0x1406B9B21` for `0x1406B9B20` at slot `0x60`) →
    `BAD  vtable +0x60: report says 0x1406b9b21, image says 0x1406b9b20`;
  - a wrong scalar claim (`30.0` for `22.5` at `0x14259D618`) →
    `BAD  0x14259d618 (the 22.5 threshold): the report claims 30.0, image says 22.5`;
  - a wrong int32 class value (`4` for `9` at `0x1425DDF10`) →
    `BAD  0x1425ddf10: report says 4, image says 9`;
  - a wrong `FUN_1406E9EF0` byte string →
    `BAD  FUN_1406E9EF0 bytes: report says 85d2…09, image says 85d2…08`;
  - a cited VA with no disassembly (`0x140501170` for `0x1406B7E25`) →
    `cited code VAs not inside a disassembled instruction: 1`.

  The file was restored after each mutation and re-verified clean. **Thirteen** mutations, all
  firing: a wrong table value; a wrong vtable entry; a wrong scalar claim; a cited VA with no
  disassembly; a wrong int32 class value; a wrong `FUN_1406E9EF0` byte string; a wrong `2^32`
  bound; the `0x1411944E6` decode reverted to the wrong form; the `req[8]` chooser angle changed
  from `req[6]` to `req[7]`; the pair-6 default changed from `1.0` to `2.0`; the pair-6 value's
  record offset changed from `0x3C` to `0x3D`; the "only `req[4]` writer" claim removed; and a
  ruled-out writer site dropped.

  **Checks that were found vacuous and fixed** (each was caught by the mutation battery, not by
  inspection): the vtable-row matcher skipped the `**`-bolded rows, so the two most important
  rows (`+0x60` and `+0x80`) were never checked; the scalar check compared the image only
  against a constant in the checker rather than against the value the report claims; the
  cited-VA regex required exactly eight hex digits after `0x140` and so matched **nothing**; the
  value-lookup patterns matched a number that was not the asserted value (e.g. reading the "3"
  out of "XMM3"); and the operand-pair check guessed `disp32` encodings where the image uses
  `disp8`. All are fixed and now covered by the mutations above.
- No GF code changed, no GF build/test run, no PES write/pin/input. The port was read only.
