# PES kick-scatter input decode (unblock `KickSoftMin`)

Source: linear `objdump`-style raw disassembly of the unpacked image
`<local>/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − `0x140000000`),
cross-checked against the `DecompAt` output in `dec_all.txt`. Ghidra HTTP bridge was down
(HTTP 000); `dec.sh` (headless, needs the exclusive `proj.lock`) could not run, so raw
disassembly is the primary evidence and every constant was script-read from the image.
Read-only RE; no repo edits. Scratch: `<workdrive>/gf-KickDecode-scratch/`.

Tags: **CONFIRMED** (raw disassembly / image-read constant / repo code),
**OPEN** (not determined + exactly what blocks it).

The scatter is computed in `FUN_14083cc30` (body `0x14083cc30..0x14083cdab`, then a second
range `0x14083cdcd..0x14083cf70`+; the Q product spans `0x14083cdcd..0x14083e607`). It calls
the soft-min fold `FUN_1408413c0` (already ported, bit-exact) **five times** with five
error-component arguments, then applies `FUN_1408414a0` (post-scale). The five soft-min call
sites are `0x14083ee62 / ee74 / ee97 / eeb0 / eecc`. Below is each term.

---

## (a) Q — every input byte of the situation product  **CONFIRMED**

All reads are off `ap = param_1` (the player/kicker context struct, ~0x2c00 bytes). The
"Q product" is built in the `0x14083cdcd..0x14083e607` block. Its inputs:

| # | What Q reads | VA read | value set | set by |
|---|---|---|---|---|
| 1 | **kick kind** (signed short) | `ap+0x2752` | 1..0x96 (151), or −1 | PES kick-setup (writer VAs below) |
| 2 | kind byte hi (stack concat) | `ap+0x2754` | — | folded into `uStack_150` with #1 |
| 3 | `iVar11` (first-time / volley flag) | `ap+0x2756` (byte) | 0..N (1 = first-time) | kick-setup |
| 4 | `uVar14` hi (0xf/0xd/0x11 override) | `ap+0x2755` (byte) | 0xf/0xd/0x11 or 0 | motion switch (below) |
| 5 | **contact class** | `FUN_1407205a0(kind)` = table byte **13** of `0x14344e380 + kind·16` | 0,1,2,3,4 (counts 49/43/29/28/1) | the kick-kind table row |
| 6 | **subclass** | `FUN_14071e520(kind)` = table byte **12** of same row | 0..13 (14 distinct) | the kick-kind table row |
| 7 | **motion descriptor byte** | `**(ap+0xab8 vtbl)+0x18` → byte at returned ptr | motion ids incl 0x9b0/0x9b4/0x9be/0xa20/0xa41/0xa42/0xa45/0xa7d | the live motion the player is running |
| 8 | **first-time/volley window** | `ap+0x2764` (float) `< 360.0` AND (#3==1 OR #5==2) | — | timing + class |
| 9 | **first-touch bool** | `**(vtbl)+0x30`(ap+0xab8) | 0/1 | the touch state |
| 10 | **body-turn Δ** | `ap+0xbe4` (float) − clamp(`ap+0x247c`) | degrees | facing vs heading |
| 11 | **heading deg** | `ap+0x247c` (float) | −360..360, clamped to [0,360] then Δ | kinematics |
| 12 | **AI bit** | `(ap+0x2460 >> 0x1d) & 1` | 0/1 | the 0x2460 flag word bit 29 |

**Contact-class → factor** (the class multiplier Q folds in, decomp lines 845-883; `uVar10`
= class `FUN_1407205a0`, `uVar28`/`uStack_13c` = subclass `FUN_14071e520`). Three steps:
1. **base by class**: `fVar36 = 0.9` (`0x14258c260`); the class branch remaps it to
   `0.88` (`0x1425ddf70`) for class 1 and `0.86` (`0x1425eb850`) for class 2/3 (class 0
   keeps 0.9). *(The exact per-class split is read from the lossy decompiler class branch;
   the three base constants are image-confirmed.)*
2. **× subclass multiplier** — `switch(subclass)`: 1→`×0.95` (`0x1425ab124`), 5→`×0.8`
   (`0x142572348`), 6→`×0.85` (`0x1425ab120`), 7/0xb→`×0.9` (`0x14258c260`), 8→`×0.7`
   (`0x14257a194`).
3. **× AI/volley gate**: if `FUN_1407240f0(kind)==0` AND AI-bit==0 AND (the
   `0x140724590`/`0x140726610`/`0x140726640` "first-time/high/lob" predicates) AND
   `*motion_short != 0x9f1` → `×0.95` (class<2) / `×0.925` (`0x1425ef9dc`, class 3) /
   `×0.9` (else).
Confirmed against the raw getters `FUN_1407205a0` (byte 13, `movups`+`movsx byte ptr
[rsp+0xd]`) and `FUN_14071e520` (byte 12, `movsx byte ptr [rsp+0xc]`) — the DecompAt
`>>28`/`>>20` was a mis-assignment (it read bytes 7/5, all zero); the **assembly** shows
**byte 13 (class)** and **byte 12 (subclass)**, which are non-zero. The port's Q must
read **byte 13 (class)** and **byte 12 (subclass)**, not 7/5.

**Kind table** `0x14344e380`, 16 bytes/row, rows 1..0x96 (getter `cmp [rcx−1],0x95`). Row
layout: `byte0`=kind id, `byte4`=family id (0..15), `byte12`=**subclass** (0..13),
`byte13`=**contact class** (0..4), `byte15`=0. Getter for the +0xc uint32 (used by
`FUN_140724740`): row `+0xc`, byte0 = `(byte11)`, returns `CONCAT31(byte11>>7, byte11<2)`.

**The kind writer** (`ap+0x2752` is a signed short; stores are `66 89 8x 52 27 00 00` /
`44 89 8x 52 27 00 00`). Six writers in `.text`:
`0x1406eb6d7`, `0x1406f6b6b`, `0x140719dcf`, `0x14071bb69`, `0x140724f26`, `0x140725fff`.
These are PES's own kick-action/motion setup; the value set is the 151-row table index.

**First-time/volley factor** (Q's 0.925): when #8 true and contact-class==3, Q multiplies by
`0.925` (`0x1425ef9dc`). The `uVar14` override (0xf/0xd/0x11) is set by the motion-id switch
at `0x14083cdc4`: motion 0x9be→0xf; 0xa20/0xa7d→0xd; 0x9b0/0x9b4/0xa41/0xa42/0xa45 with
kind 0x2b/0x3a→9. Thresholds: `157.5` (`0x1425cca3c`), `130.0` (`0x1425a9bf0`),
`115.0` (`0x1425c48d4`), `110.0` (`0x14259d628`).

### PES kick-kind → GF kick-type map  **CONFIRMED (with a namespace gap)**

Three distinct namespaces, all confirmed:
1. **GF `e_FunctionType`** (`src/sim/gamedefines.hpp:52`): None, Movement, BallControl, Trap,
   ShortPass, LongPass, HighPass, Header, Shot, Deflect, Catch, Interfere, Trip, Sliding, Special.
   `IsKickFunction` = ShortPass | LongPass | HighPass | Shot (`team.cpp:26`).
2. **PES action kind** (ring-record payload+0, `pesevents.hpp:46`): `kActionKick=1`,
   `kActionHighKick=2`, `kActionShot=5`, `kActionTouch=7`. GF emits these via
   `PesActionKindOf` (`team.cpp:32`) → `QueuePesTouch` (`team.cpp:383`).
3. **PES kick kind** = the 151-row table index (the short at `ap+0x2752`) — **the one Q
   actually reads**.

 GF's chooser: `e_FunctionType_ShortPass/LongPass→kActionKick`, `HighPass→kActionHighKick`,
 `Shot→kActionShot`, everything else (Header, Trap, Deflect, …)→`kActionTouch`. There is **no
 `Header`/clear→PES-kick-kind path** in the port: `e_FunctionType_Header` maps to
 `kActionTouch` (7), so a header is treated as a plain touch, not a kick. (Note: three
 GF "is-kick" predicates disagree on Header — `team.cpp:26 IsKickFunction` excludes it
 (this gates `inKickBeforeContact`, `team.cpp:401`), while `gamedefines.hpp:72` and
 `match.cpp:1643` include it. The PES-action wire `PesActionKindOf` sends Header→7
 regardless.)
The bridge between (2) and (3): when `kind==−1` Q reads the action via
`**(ap+0xab8 vtbl)+0x28` and remaps `0x33→0x2a`, `0x3f→0x1c`, `0x41→0x41`, else uses a
default. So the only confirmed action→kind mappings are those three remaps; the full
151-kind namespace is set only by the six PES writers in #2 above.

**What GF cannot produce:** GF drives Q's **`kind==−1` fallback** (it never sets `ap+0x2752`),
so it can only reach the kinds reached via that path (the three remaps + default), not the
151 table kinds. Concretely GF **cannot produce** any of: the direct table kinds 1..0x96
(other than via the −1 remap), a **Header as a kick** (Header→Touch 7), or any **clear**
(GF has no clear function type at all — `e_FunctionType` has no Clear). To reproduce Q
exactly the port must either set the 151-kind short or reproduce the −1 fallback's
vtable+0x28 action path. **This is the open mapping gap** (see Unresolved).

---

## (b) run term R — `FUN_140723320`  **CONFIRMED (inputs/weights/clamp) · TENTATIVE (per-case affine)**

Inputs: `rcx` = player context (`ap`). It reads the **3-float contact vector** at
`ap+0x2bf4/+0x2bf8/+0x2bfc`, minus the **kicker position** (from `FUN_140a6cf70` →
`rax`), producing three clamped approach components. It also reads a **state id** from
`ap+0xab8`'s `vtbl[+8]()` and a per-opponent **distance** (a `sqrt`, `0x1415a1780`,
`/1000.0` `0x14256ae34`).

Structure (raw `0x140723320..0x14072394a`; `ret` at `0x14072394a`):
1. **per-component clamp**: each of the 3 components is `abs`'d (float→double `andps`
   `0x3FF…` idiom, `0x1407235ca`/`0x140723615`) then gated against `180.0`
   (`0x142598580`, `comiss` at `0x1407235d5`/`0x140723620`); when the gate fires the
   component is reduced by a `±1.0·k`. All three end up in [0, 180] (`xmm8`=x, `xmm9`=y,
   `xmm12`=z).
2. **near-opponent flag** `r12`: `1` if the state id is `<0`, `∈[0,0xb)`, or `≥0x16`;
   else `0` (`0x1407233a1..0x1407233c3`).
3. **range gate** `d`: the contact-vector length scaled by a per-opponent table float
   `[r13 + rbx·4 + 0x16a8]` (`0x140723597`), clamped to [0,1]
   (`0x1407235b4..0x1407235c6`).
4. **weighted sum**: an accumulator over **products of the clamped components** (each
   multiplied by `d`), each term weighted by one float of the **8-float weight table
   `0x14344ef78..0x14344ef98`** (loaded at `0x140723779` and in a loop at
   `0x140723848..0x140723863`). Weights (image-read): `[0.012021291069686413,
   0.0001723970053717494, 0.09834727644920349, 0.00019764300668612123,
   0.0013995419722050428, 0.002476111985743046, −2.2503371238708496,
   −0.048660777509212494]`.
5. **per-case affine remap**: `case = (int8)r15[1] − 8`; if `0 ≤ case ≤ 0xd` the jump
   table `0x14072394c` (14 entries) selects one of a handful of `mulss/subss` forms
   (`0x1407236dc/0x1407236f2/0x14072370c/0x14072374e`); a default path uses
   `vtbl[+0x20]()` byte ≠ 0xb → `0x14072369d` else `0x1407236e0`. The per-case
   multipliers/adders are **decompiler-read** (`~×2.0`, `×0.75`, `×1.3+(1−0.8)`, `×0.6`,
   `×0.8`, `×0.4+10.0`) and **TENTATIVE** (my RIP-const re-reads were misaligned — exact
   values to be re-confirmed before port). The `r15[1]` byte and the `ap+0xb04`/
   `r15+0x3c` byte gates are CONFIRMED as the case selectors.
6. **final scale + clamp**: `R = maxss(Σ·k, 0)` then `comiss` against `1.0`
   (`0x14256ae1c`), returned in [0,1] as the run term that multiplies into the scatter.
   (The scale `k` at `0x140723865` is a small constant, TENTATIVE value.)

Constants CONFIRMED: weight table `0x14344ef78..0x14344ef98` (8 floats, above);
`180.0` `0x142598580`; `1.0`/`−1.0` clamp `0x14256ae1c`/`0x142588218`; `/1000.0`
`0x14256ae34`. Constants TENTATIVE (decomp-read, re-confirm before port): the per-case
affine multipliers/adders and the final `k` scale.
GF stand-in today: a hard-coded run factor — the port must compute this from a nearby
opponent (the `r12`/state-id flag + the contact vector + the 8-float table).

---

## (c) F1/F2 — the motion descriptor byte  **CONFIRMED**

`FUN_1406b0250(longlong* tbl, uint motionId, char flag)` (`0x1406b0250..0x1406b02a2`):

```
if motionId == 0xe42: return 0x22
if motionId == 0xf7c: return 0x18
if motionId >= 0x1125: return *(tbl + 8) + 1 + flag     ; out-of-table: base+1
return *(tbl[1] + motionId*0x390 + 1 + flag)            ; table byte
```

- `tbl[1]` (`*rcx+8` when `≥0x1125`, or the `[rax]` word path) is the **per-motion table
  base**. At the call site `0x14083e757..0x14083e76a`: `call [vtbl+0x18]` → motion-id object;
  `movzx ebx, word [rax]` (the 16-bit motion id); `call 0x1406b0ed0` (returns the table
  base, a **lazy singleton** at `0x1436f23c0`, built via `0x1406aecf0` if null);
  `call 0x1406b0250` with `edx=motionId`, `r8d=flag`.
- **F1/F2 descriptor byte** = `tableBase + motionId·0x390 + 1 + flag`. Each motion row is
  `0x390` bytes; byte `+1` (flag 0) is the descriptor byte, byte `+0` is a companion.
- Special: motion `0xe42`→descriptor `0x22`; motion `0xf7c`→descriptor `0x18`; motion
  `≥0x1125`→`tableBase+8+1` (base+1, i.e. no per-motion row, uses the header).
- The descriptor byte's **value set** is the byte at each `row+1`; the consumer
  (`0x14083e76f`) tests `== 1` to decide the "in-flight" branch. GF stand-in: the port uses
  a fixed descriptor — it must read this byte per motion (needs the motion-id object +
  table base, both obtainable).

---

## (d) the `xmm8` source before `0x14083ed60`  **CONFIRMED (register) · TENTATIVE (exact byte→[0,1] map)**

Two distinct factors live here; the KickSoftMin doc's "angRamp (the xmm8 factor in D)"
conflated them. At `0x14083ed61` (`comiss xmm0, xmm7`) the registers hold:

**`xmm8` = a motion-descriptor contact byte, normalized to [0,1]** — the last `xmm8`
write before `0x14083ed60` is at `0x14083ecda..0x14083ed32` (fast path
`0x14083ecd4`). Source object `rax` is looked up by id:
`id = FUN_1406ea340(r13, 0)` (`0x14083ec88`), `obj = 0x140a48910(id)`
(`0x14083ecc9`). Then:
```
if byte[obj + 1] == 0x48:            xmm8 = 1.0              ; 0x14083ecd4 (xmm12)
else:  B = byte[obj + 0x2c]          ; 0x14083ecda
       xmm8 = clamp/normalize(B) in [0,1]   ; 0x14083ed17/ed23/ed32,
              bounds xmm14=0.0 (0x14083cf19) / xmm12=1.0 (0x14256ae1c),
              scaled by the lazily-init global int 0x36=54 at 0x1416c7b6a
              (getter 0x141e532e0, init 0x141e53300) and by id/4 (shr edx,2).
```
The exact `B → [0,1]` affine is **TENTATIVE** (the magic-multiply/shift normalize at
`0x14083ed01..0x14083ed32` is not a clean single division; structure + fast path +
bounds are CONFIRMED). This is the `xmm8` the soft-min `FUN_1408413c0` folds in.

**Separate "angRamp" factor = `xmm7`** (NOT xmm8), built at `0x14083ed40..0x14083ed87`:
```
ang  = FUN_1406e6660(r13, 0x28) − byte[r13 + 0x28]      ; 0x14083ed47/ed58
       (FUN_1406e6660 reads byte[obj2+0x28], obj2 = lookup of byte[r13+0x2458])
ramp = ang < −20.0 ? 0.0 : ang > 20.0 ? 1.0 : (ang + 20.0)/40.0
       ; −20.0 0x1425a946c, 20.0 0x14259be1c, 40.0 0x14259be2c
       (then ×0.4 0x14259b89c at 0x14083edd4; folded at 0x14083edf4..0x14083ee3e)
```
So the port's "angRamp" = `clamp((err+20)/40,0,1)·0.4`, `err = byte[obj2+0x28] −
byte[r13+0x28]`, carried in **xmm7**; the **xmm8** the soft-min also folds is the
motion-descriptor contact byte above.
---

 ## (e) post-scales in `FUN_1408414a0` — kind/motion-id gating **CONFIRMED (structure, constants, jump targets)**

 Args (prologue `0x1408414bc..0x1408414d4`): `rcx`=player (`r14`), `rdx`→`r15`=&x,
 `r8`→`rbx`=&y, `r9`→`rbp`=&z. Reads `kind=(short)[player+0x2752]`,
 `b=[player+0x2755]` (`movsx r12d`, `0x1408414c6`). `r13`=subclass=`FUN_14071e520(kind)`
 (`0x140841516`). `rdi`=`player+0xab8` (motion object; `0x14084151b`). Raw `raw_14a0.txt`.

 1. **kind remap** (only when `kind==−1`, `0x1408414d9..0x14084150f`):
    `action=vtbl+0x28(player+0xab8)`; `kind = action==0x33?0x2a : action==0x3f?0x1c :
    (action==0x41?0x41 : 0x27)`.
 2. **Tier 0** (`0x140841530..552`): if **NOT** `0x140a6e340(action)` → `z·=0.1`
    (insn `0x14084153e`→`0x142588200`), `*(z+4)=0.5` (`0x3f000000` imm), return.
 3. **Tier 1 — clear-like** (`0x140841557..5a6`): if `0x1407246e0(player)` **AND**
    `0x1407241a0(kind)` → `x·=0.9` (`0x140841577`→`0x14258c260`), `y·=0.1`
    (`0x140841588`→`0x142588200`), `z·=0.1` (`0x140841599`→`0x142588200`), return.
 4. **Tier 2 — motion-id flag table** (`0x1408415ab..`; reached when Tier-1 2nd test is
    false): `motionId = word[vtbl+0x18(player+0xab8)]`.
    - `motionId==0x1093` → fall to `0x140841747` (`*(y+4)=1.0`).
    - `motionId∈[0x842,0x8AF]` (`motionId−0x842 ≤0x6d` unsigned) → flag byte
      `f=[0x1408417f8+(motionId−0x842)]` ∈ {0,1,2}, then `jmp [0x1408417ec+4f]`
      (**3-way on the flag value**; `movzx eax` clobbers the index; `lea
      rdx,[rip−0x8415da]`=image base). Targets CONFIRMED: f=0→`0x140841747`
      (`*(y+4)=1.0`, `0x3f800000` imm); f=1→`0x1408415ee`:
      `*(y+4)=(*(y+4)+1.0)·0.5` (`0x1408415f3`→`0x14256ae1c`,
      `0x1408415fb`→`0x142565600`); f=2→`0x14084160d` (Tier-3 blend). Flag histogram
      over the 0x6e entries: 0×12, 1×21, 2×77.
    - else → Tier 3.
 5. **Tier 3 — u² blend** (`0x14084160d..684`): if `0x140726610(kind)` **OR**
    `kind==0x16` → `0x140841747` (`*(y+4)=1.0`); else `u=[rsp+0x78]`,
    `t = (u²≤0.2 ? 1.0 : u²≥1.0 ? 0.0 : (u²−1.0)/(−0.8))`, `s = 0.5·t+0.5`,
    `*(y+4) = s·*(y+4)+(1−s)`. Constants: `0.2` (`0x14084162b`→`0x142598578`),
    `1.0` (`0x140841633`→`0x14256ae1c`), `−0.8` (`0x140841657`→`0x1425c7674`),
    `0.5` (`0x14084165f`→`0x142565600`, `0x140841667`→`0x142565600`). `u` = caller's
    7th stack arg (`[rsp+0x78]`≡caller `[R+0x30]`); its value source is OPEN (needs the
    `FUN_1408414a0` call site). Then return (`0x140841684`→`0x1408417bc`).
 6. **Tier 3-alt — class-based** (`0x140841689..`, when `0x1407246e0` is false): if
    `subclass−5 ∈ [0,1]` (unsigned ≤1): `vtbl+0x20()` byte==8 **AND**
    `0x1406ead40(player,0x18)` → `*(y+4)=1.0` (`0x3f800000`); elif
    `0x140a6e340(action2)` **AND** `0x1406ead40(player,9)` → `*(y+4)=2/3`
    (`0x3f333333`); else `*(y+4)=4/3` (`0x3f666666`). Then **second flag table**
    (`0x1408416ef..`): `motionId−0x97c ≤0xd7` else return; flag
    `f=[0x140841884+(motionId−0x97c)]` ∈ {0..6}, `jmp [0x140841868+4f]` (**7-way**).
    Targets CONFIRMED: f=0→`0x140841747` (`*(y+4)=1.0`); f=1→`0x140841796` (if
    `b==0xf`: `*(x+4)=(vtbl+0x30() ? 1.0 : 0.0)` with `1.0`
    `0x1408417a9`→`0x14256ae1c`; else unchanged); f=2→`0x140841750`
    (`*(x+4)=(vtbl+0x30() ? 1.0 : −1.0)`, `0x14084175d`→`0x14256ae1c`,
    `0x140841767`→`0x142588218`); f=3→`0x14084177a`
    (`*(y+4)=*(y+4)·0.2+0.8`, `0x14084177f`→`0x142598578`,
    `0x140841787`→`0x142572348`); f=4→`0x140841771` (`*(y+4)=0`); f=5→`0x14084172b`
    (`*(y+4)=*(y+4)·0.25+0.75`, `0x140841730`→`0x142592228`,
    `0x140841738`→`0x14259bdd4`); f=6→`0x1408417bc` (return).
 7. **Epilogue override** (`0x1408417bc..7cb`): if `[player+0x2d14]&1` → `x=0`
    (`mov [r15],0`), then return. Applies after every tier.

 Per-motion flag bytes are install data read at port time (`PeImage::ReadVa`):
 Tier-2 `0x1408417f8` (0x6e bytes), Tier-3-alt `0x140841884` (0xd8 bytes). Only `u`
 (Tier-3) needs a caller trace.

 ## (f) yaw redraw — the `[r14]·[r14+8]` sign test  **CONFIRMED (mechanics) · OPEN (rbp provenance)**

 In the yaw loop `0x14083a48d..0x14083a598` (raw `raw_8d5.txt`):
 - `r14 = [rbp−0x78]` (`0x14083a48d`; a pointer-sized slot; `[rbp−0x78]` is read-only
   in this function, so it is an incoming stack slot — the caller's 5th+ arg or a
   context field; `rbp` itself is live-on-entry with no prologue setup in
   `0x1408398d5..`, so its provenance needs the caller).
 - `r14` points at a ≥12-byte struct with floats at `+0` (x) and `+8` (y) — the yaw
   reference vector. The sign test (`0x14083a510..0x14083a527`):
   ```
   xmm0 = [r14+8]        ; y
   xmm0 = xmm0 · [r14]   ; y·x
   comiss xmm10, xmm0    ; xmm10 == 0.0 (xorps at 0x140839924, read-only after)
   jbe -> B              ; 0.0 ≤ x·y → xmm0 = xmm9
   A: xmm0 = xmm11       ; x·y < 0 → xmm11
   B: xmm0 = xmm9         ; x·y ≥ 0 → xmm9
   ```
   i.e. the **sign of `x·y`** selects `xmm11` (opposite signs) vs `xmm9` (same sign) —
   a quadrant/handedness pick between two pre-computed yaw values.
 - Redraw loop (`0x14083a52b..0x14083a593`): if picked `xmm0 != xmm8` (current) AND
   `xmm6 > xmm7`, then `cmp ebx,5; jg exit` (ebx=0 on entry at `0x14083a4f8` — up to
   **6 draws**, ebx=0..5); else a draw via `call 0x140a92590` (int→float via
   `movd/cvtdq2ps`, scaled by `xmm13/xmm15`; function identity not traced — presumed
   RNG), tested by `call 0x1406ebf30`; pass→`xmm8=xmm9`, fail→`xmm8=xmm11`; `inc ebx`,
   loop back to `0x14083a510`. Exit with `xmm6=xmm7` when `ebx>5` (`0x14083a598`).

---

 ## (g) shot pitch — `xmm14`, `xmm7`, `xmm1` before `0x14083da40`  **CONFIRMED**

 - **`xmm14` = 0.0.** Zeroed once at `0x14083cf19` (`xorps xmm14,xmm14`); the only
   later `xmm14` mentions in `0x14083cf19..0x14083da40` are reads (`movaps` copies,
   `comiss` clamps at `d19e/d1e2/d290/d2a9/d5a8/d93b`, `mulss xmm7,xmm14` at `d9e9`
   which zeroes `xmm7`). It is `0.0` at `0x14083da35` (`[rsp+0x64]=xmm14`) and at every
   `xmm14`-read. No accumulation — the "accumulator" hypothesis is dead.
 - **`xmm7` = 10.0.** Last write before `0x14083da40` is `0x14083da10`:
   `movss xmm7,[rip+...]` → `0x142588210` = `10.0`. (All earlier `xmm7` values —
   `d806/d833` copies of `xmm14`, the `d839..d84c` normalize, `d9cd/d9e9/d9f0` cases —
   are clobbered by this load.) `xmm7` is the running pitch, reset to 10.0 here.
 - **`xmm1` = (double)`xmm11`.** On the fall-through path (`0x14083da41`, when
   `0x1407245d0` returns 0): `0x14083da47` `xorps xmm1,xmm1`, then `0x14083da4d`
   `cvtss2sd xmm1,xmm11`. So `xmm1` := `(double)xmm11`; `xmm11` = the second range
   term from `0x14083d147..0x14083d188`: `xmm11 = F·60.0·60.0/1000.0·sqrt(|dir|²)`
   (`F`=`0x141e532d0()` result at `d147`; `60.0` `0x14259a764` at `d0ec`;
   `1000.0` `0x14256ae34` at `d16c`; `sqrt`=`0x1415a1780` at `d17e`;
   `|dir|²` from `[rsi]`/`[rsi+4]`/`[rsi+8]` at `d12c/d130/d136`).
   (`xmm1`'s pre-`da40` value `[rsp+0x68]` at `d1b9` is dead — unconditionally zeroed
   at `da47` on this path; on the taken path `xmm1` is unused.)

 Downstream (`0x14083da41..0x14083db50`, decoded with constants — the pitch that lands
 in `[rsp+0x64]`): with `xmm2=[rsp+0x7c]` (first range term), `xmm6=0.7`
 (`0x14083da08`→`0x14257a194`), `xmm12=1.0` (`0x14083cf1d`→`0x14256ae1c`), `xmm9=0.85`
 (`0x14083da1f`→`0x1425ab120`), `xmm14=0.0`:
 - If `(double)(xmm2·0.7) > (double)xmm11` (`0.7` double `0x1425ef9e8` at `da52`):
   `a=xmm2·0.3` (`0x142590d98`), `b=xmm2·0.7`; `xmm6 = a≥xmm11 ? 1.0 : xmm11≥b ? 0.0 :
   (xmm11−b)/(a−b)`; `xmm6 = xmm6·0.4+0.5` (`0x14259b89c`, `0x142565600`);
   `[rsp+0x64]=xmm6`.
 - Else if class-gate (`r14d≤0x11` + bit test) fails → `[rsp+0x64]=0.0`; elif a second
   gate (`ebx==8` after `0x14071ecf0` filling `[rsp+0x60]=120.0`/`[rsp+0x68]=30.0`)
   → `[rsp+0x64]=0.0`; else a second lerp of `[rsp+0x60]` with `xmm6/xmm9/xmm11`
   (`0.5` `0x142565600` at `db35`) → `[rsp+0x64]`.
 Constants CONFIRMED (capstone RIP targets): `10.0` `0x142588210`, `0.7` `0x14257a194`,
 `0.85` `0x1425ab120`, `1.0` `0x14256ae1c`, `0.7`(dbl) `0x1425ef9e8`, `0.3`
 `0x142590d98`, `0.4` `0x14259b89c`, `0.5` `0x142565600`, `60.0` `0x14259a764`,
 `1000.0` `0x14256ae34`, `120.0`/`30.0` immediates `0x42f00000`/`0x41f00000`.
---
 ---

 ## Unresolved (exactly what blocks each)

 1. **(a) GF→PES-151-kind bridge.** GF sets the action kind 1/2/5/7, never the 151-kind
    short; Q uses the `kind==−1` fallback (vtable+0x28 action, remap 0x33→0x2a/0x3f→0x1c/
    0x41→0x41). To close: confirm the vtable+0x28 action value set and whether the port
    should set `ap+0x2752` directly (needs the six writer VAs' input logic) or reproduce
    the −1 path. Header/clear have no PES-kick-kind path in the port today.
 2. **(b) per-case affine + final k.** The 14-entry jump table `0x14072394c` targets and
    the per-case mul/add constants plus the final scale `k` at `0x140723865` need aligned
    (8-byte) RIP re-reads; the earlier attempt used 7-byte lengths and produced garbage.
    Bounded: re-run the constant script with `ln=8` on the `0x1407236dc..0x140723865`
    cases.
 3. **(d) exact B→[0,1] affine.** Fast path (`byte==0x48`→1.0), bounds (0.0/1.0), and the
    ×54 shr-id scaling are CONFIRMED; the magic-multiply/shift normalize sequence at
    `0x14083ed01..0x14083ed32` needs one careful decode pass.
 4. **(e) Tier-3 `u` source.** `[rsp+0x78]` ≡ caller's 7th stack arg; needs the single
    `FUN_1408414a0` call site (who passes what in R+0x30). Everything else in (e) is
    closed.
 5. **(f) `rbp`/`[rbp−0x78]` provenance.** Needs the `FUN_1408398d5` caller (which game
    vector r14 points at). Mechanics, polarity, and loop bound are closed.
 6. **Live confirm (optional).** A read-only `pread` of `ap+0x2752/0x2755/0x2756/0xbe4/
    0x247c/0x2460` during a live shot would confirm the kind/first-time/AI value sets
    in-game; the static table image is authoritative for class/subclass ranges.

 All constants script-read from `pes_full.bin` (file offset = VA − `0x140000000`); operand
 VAs are the `movss`/`movsd`/`mulss`/`divss`/`addss` RIP sites as listed per term.
 
