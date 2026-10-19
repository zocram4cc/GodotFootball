# PES lane structs: channel map at the five unit callers + complete lane-core caller enumeration (doc O14)

Decode-only session. No source file touched, no live PES, no commits. All VAs are image VAs of
`pes_full.bin` (image base `0x140000000`), read with `objdump -D -b binary -m i386:x86-64`
plus the project's function list (`gf-LaneStruct-scratch/funcs.txt`, 117471 entries) for containment.
The Ghidra bridge on 8080 was **down** for this pass (xref endpoints returned non-JSON), so the
caller enumeration was done by an image-wide scan for `E8 rel32` whose target equals the core VA,
plus a scan for any 8-byte pointer in the image equal to the core/packer VA. Scratch:
`~/ghidra-projects/scratch/Unpacker/pes_full.bin`, `<workdrive>/gf-LaneStruct-scratch/`.

Legend: **C-asm** = raw disassembly. **C-dec** = decompile/flow earlier confirmed. **INF** =
inference, stated as such. **OPEN** = not decoded, blocker named.

---

## 1. Verdict (this closes O14's blocker)

1. **The teammate branch of the core IS live.** `DirectHighBall` (`0x142129BE0`) calls the core
   (via packer `0x14063D2F0`) at `0x14212A22D` with `r9d = r14d`, and `r14d` is the inlined
   `IsAwaySlot(c)` of the *same* slot `c = r13d` it passes in `r8d` (`0x142129C4A`-`0x142129C58`
   computes it, `0x14212A1F1`-`0x14212A222` passes it). **C-asm.** So `s` = the reference's own
   side = a teammate scan. The spec `docs/re/pes-re-lane-core.md` §3.6 claim that every observed
   caller used the opposite side, i.e. that the teammate branch is dead, is **wrong** — it was
   derived from two callers only.
2. **BackSpace, GoalFront and Safety cannot ask for a teammate scan at all.** All three go
   `0x14063DE20` (packer) -> `0x14063DC10` (LaneFree), and `0x14063DC10` derives the scan side
   *itself*: `0x14063DC42`-`0x14063DCAF` computes `IsAwaySlot(c)` from its `edx` and then
   `r9d = r10d ^ 1` -> `s = !IsAwaySlot(c)`. **C-asm.** Same for `0x14063DAB0` (5 callers).
3. **OneTwo** reaches the core only through the one-touch predicate `0x142128C10` (core call at
   `0x142129076`), which passes `s = !IsAwaySlot(R)` where `R` is the *receiver* record
   (`0x142129035`-`0x142129043`). **C-asm.**
4. The core has **13 direct call sites and 0 indirect callers** (no qword in the image equals
   `0x14063CD90`). The spec's "the only callers are `0x14063DC10` and `0x14063D2F0`" is **wrong**;
   see §4. Only 2 of the 4 packer/wrapper containers are those two.

---

## 2. Core ABI recap (asm-checked; matches `peslane.hpp`)

```
0x14063CD90(out* rcx, _unused rdx, c r8d, s r9b,
            [rsp+0x20]=L*, [0x28]=keep(bool), [0x30]=h(int), [0x38]=u2(bool), [0x40]=dead(int))
L = { x,y,z at L[0..2] (ground origin), theta_deg at L[3], len at L[4], cone at L[5], rad at L[6] }
out = { int nearest(0xff = none) ; int count ; 8-byte entries {int id; float d2} from out+8 }
```
- `h` is an **int** (prediction horizon in PES frames): all call sites pass `0`, `ebp`/`esi` = 0, or
  a `cvttss2si`/`shr eax,2` int. **C-asm** (`0x14212489C`, `0x14212A1D1`, `0x14212B535`).
- `dead` is fed `-1` by DHB/Cross/the predicate and `0` by BackSpace; it is dead in the core
  (C-dec, spec §3.5).
- The wrapper returns `out[1] == 0`: `0x14063DCEC` `cmp DWORD PTR [rsp+0x64],0x0 / sete al`. **C-asm.**

---

## 3. Table: the five units -> lane channel sources (acceptance table)

Columns: call VA / path / out / c (reference slot) / s (scan side) / origin / theta / len / cone /
rad / keep / h / u2 / dead / post-call use.

### 3.1 BackSpace `0x142129110` (no bridge function in the CURRENT DB — see §5 item 3 note)
- call VA: `0x14212926C` -> `0x14063DE20` (packer) -> `0x14063DC10` -> core. **C-asm.**
- out: `rcx = r15 = ctx[1]`. `0x14063DC10` forwards its `rcx` to the core's dead `rdx`
  (`mov rbx,rcx` / `mov rdx,rbx`) and keeps its own local out at `[rsp+0x60]` (entries `[rsp+0x68]`,
  pre-filled `{0xff, 0.0f}` by `0x14063DC60`). **The unit receives only the bool.** **C-asm.**
- c: `edx = edi`, `edi = [ctx[0]] = ctx[0][0]` = the *carrier's own slot* (`0x14212912C`, `0x142129141`,
  `0x14212922D`). **C-asm.**
- s: **forced `!IsAwaySlot(c)`** inside `0x14063DC10`. The unit has no side channel. **C-asm.**
- origin: `r8 = &[rsp+0x70]`; `[rsp+0x70] = movsd [r13+0x55C]` (x,y) and `[rsp+0x78] = [r13+0x564]`
  (z), where `r13 = 0x140A48A10(rcx=0x1436F3DC8, edx=c)` = the carrier's own record.
  i.e. **origin = the carrier's own position** (+0x55C/+0x564). **C-asm.**
- theta: `r9 = &[rsp+0xE0]`; `[rsp+0xE0] = wrap360( [0x140A48A30(edx = IsAwaySlot(c)) + 0x250] )`
  (`0x1421291C6`-`0x1421291FF`, wrap with `360.0` = `0x14259858C`). So **theta = a
  per-side float at +0x250 of the side object**, not a per-player value. INF: the semantics
  ("the side's facing/attack direction"). **C-asm for the source + wrap.**
- len `= 20.0` (`0x14259BE1C`), cone `= 80.0` (`0x142596E6C`), rad `= 1.0` (`0x14256AE1C`). **C-asm.**
- keep `= 0` (`[rsp+0x38]`), h `= 0` (`[rsp+0x40] = ebp`, `xor ebp,ebp` at `0x14212912F`),
  u2 `= 0` (`[rsp+0x48]`), dead `= ebp = 0` (DE20 copies its `[0x40]` into that slot twice). **C-asm.**
- post-call: `test al,al / je 0x142129336` = the lane must be free to continue. **C-asm.**
- gating: the call sits behind `0x142139630([ctx+0x28]+0x898, [ctx[0]+4])` and
  `0x1406997A0([ctx+0x28]+0x9CC)` returning non-zero. **C-asm.**

### 3.2 GoalFront `0x14212ABA0` (a real function entry in the DB)
- call VA: `0x14212ACE9` -> `0x14063DE20` -> `0x14063DC10` -> core. **C-asm.**
- out: `rcx = r15 = ctx[1]` (same dead channel as BackSpace). **C-asm.**
- c: `edx = ebx` (the reference slot, guarded by its own inlined `IsAway` 3-compare at
  `0x14212ABC7`-`0x14212ABD0`). **C-asm.**
- s: forced `!IsAwaySlot(c)` inside `0x14063DC10`. **C-asm.**
- origin: `r8 = &[rsp+0x60]`; built from `r13 = 0x140A48A10(ebx)` (`0x14212ABF3`) ->
  `movsd xmm0,[r13+0x55C]` + `eax = [r13+0x564]` (`0x14212AC79`-`0x14212AC97`). **origin = the
  reference record's own position.** **C-asm.**
- theta: `r9 = &[rsp+0xB0]`; from `0x140A71260(T18, slot)` (`0x14212AC1E`, the heading table),
  wrap-normalised with `360.0`/`-360.0` (`0x14212AC46`-`0x14212AC77`). **C-asm.**
  (Earlier doc note "+180" is not visible in this window; the window only shows the 360 wrap.)
- len `= 20.0` (`0x14259BE1C`), cone `= 80.0` (`0x142596E6C`), rad `= 1.0` (`0x14256AE1C`). **C-asm.**
- keep `= 0`, h `= 0` (`[rsp+0x40] = 0`), u2 `= 0` (`[rsp+0x48] = 0`). **C-asm.**
- post-call: `test al,al` at `0x14212ACEE`. **C-asm.**

### 3.3 Safety `0x14212B520` (no bridge function in the CURRENT DB — see §5 item 3 note)
- call VA: `0x14212B7C0` -> `0x14063DE20` -> `0x14063DC10` -> core. **C-asm.**
- out: `rcx = [rsp+0x80] = ctx[1]` (`0x14212B568`, `0x14212B78E`). **C-asm.**
- c: `edx = ebp`, `ebp = [ctx[0]] = ctx[0][0]` = the **carrier's own slot** (`0x14212B532`,
  `0x14212B542`), with its inlined `IsAway` guard into `ebx` (`0x14212B544`-`0x14212B550`). **C-asm.**
- s: forced `!IsAwaySlot(c)` inside `0x14063DC10`. **C-asm.**
- origin: `r8 = &[rsp+0x70]`; `[rsp+0x70/0x78] = ([rax+0x55C], [rax+0x564])` where
  `rax = 0x140A48A10(c)` (`0x14212B582`) -> the carrier's own record pointer stored at
  `[rsp+0x70]` and re-read at `0x14212B748`. **origin = the carrier's position.** **C-asm.**
- theta: `r9 = &[rsp+0x60]` = `xmm6` = `0x140A71260(T18 = r12, slot)` (`0x14212B6C2`,
  `r12 = [ctx[1]+0x18]`), stored at `0x14212B751`. **C-asm.**
- **len is runtime, not a constant**: `xmm8 = NearestEntry[ rank[+0xB2 + slot + (slot>=11 ? 11 : 0)] ].d2`
  (`0x14212B71D`-`0x14212B741`, 8-byte records, float at +4), then `len = 0x1415A1780(xmm8)`
  (`0x14212B748`-`0x14212B76F`). `0x1415A1780` is an **imported CRT thunk** (IAT slot
  `0x1425301F0` in the dump; the dump has no import directory). INF: it is `sqrt`, so
  `len = distance to the nearest opponent` (NearestEntry stores `d^2`), i.e. Safety's lane length
  is "how far the nearest opponent is". **C-asm for the table read + call.**
- cone `= 80.0` (`0x142596E6C`), **rad `= 3.0`** (`0x14258C264`), keep `= 0`, h `= 0`
  (`[rsp+0x40] = esi`, `xor esi,esi` at `0x14212B535`), u2 `= 0`. **C-asm.**
- post-call: `test al,al` at the `0x14212B7C5`-ish path. **C-asm.**

### 3.4 OneTwo `0x14212AEB0` (a real function entry)
- The unit's own lane call is the **one-touch predicate** `0x142128C10`, called at `0x14212AF86`
  (`rcx = rsi = ctx`). The core call inside the predicate is `0x142129076`. **C-asm.**
- out: `rcx = &[rsp+0x90]` (predicate-local); the count is read right after at `[rsp+0x94]`
  (`0x14212907B` `lea rcx,[rsp+0x94]`). **C-asm.**
- c: `r8d = [rcx] = R[0]` where `R` = the **receiver** record (`0x142129043`). **C-asm.**
- s: `r9b = (R[0x8] == 0)` = `!IsAwaySlot(receiver)` = the receiver's **opponent** side
  (`0x142129035`-`0x14212903F`, the <=1 guard then `sete`). **C-asm.**
- L: `[rsp+0x20] = &[rsp+0x58]` = the predicate's own snapshot (spec §7: the snapshot filler
  `0x14063C9F0`; doc G11 lane = len 15, cone 60, rad 2). keep `= 0`, **h `= 0`**,
  u2 `= 0`, dead `= -1` (`0x142129057`-`0x142129071`). **C-asm for the arg slots**; the snapshot
  fill itself is C-dec from the earlier session.
- post-call: the count at `out+4`; `test al,al / je` at `0x14212A252` gates the OneTwo branch.
  **C-asm.**

### 3.5 DirectHighBall `0x142129BE0` (interior entry of `FUN_142129590`; see §5.3)
- call VA: `0x14212A22D` -> `0x14063D2F0` (packer B) -> core directly. **C-asm.**
- out: `rcx = &[rbp-0x30]`, nearest pre-set `0xff` at `0x14212A197`, count read at `out+4 =
  [rbp-0x2C]` through `0x1421B7D40` (`0x14212A232`); result used as `(count == 0)` in `ebx`
  (`0x14212A23B`-`0x14212A247`). **C-asm.**
- c: `r8d = r13d`, `r13d = [rax]` (`0x142129C42`) = the **carrier's own slot**. **C-asm.**
- s: `r9d = r14d`; `r14d = IsAwaySlot(r13d)` computed at `0x142129C4A`-`0x142129C58`
  (`xor r14d,r14d / test / cmp 0xb / cmp 0x16 / setl`) and saved to `[rsp+0x70]`. **s = the
  carrier's own side = TEAMMATE scan.** **C-asm.**
- origin: `L[0..2] = [rbp-0x70]`, copied from `[rbp-0x80]` which is the output of the
  advance helper `0x140A8F7C0(&[rbp-0x80], theta, r)` called at `0x14212A189`. **C-asm.** INF on
  which ball/anchor position feeds it (the ball position advanced along theta).
- theta: `[rsp+0x28] = &[rsp+0x78]`, and `[rsp+0x78] = xmm0` where `xmm0` comes from the aim
  helper `0x140439F90` (`0x14212A144`, re-stored at `0x14212A1B6`). **C-asm.**
- len `= xmm6 = 15.0` (`0x14259BE0C`, `0x14212A0FE`, clamped by the `comiss` at `0x14212A113`),
  cone `= 60.0` (`0x14259A764`), rad `= 5.0` (`0x14259222C`). **C-asm** (INF which side of the
  clamp the len ends on).
- keep `= 0` (`[rsp+0x48]`), h `= 0x141E532E0() >> 2` (`0x14212A1CC`-`0x14212A1E1`),
  u2 `= 0` (`[rsp+0x58]`), dead `= -1` (`[rsp+0x60]`). **C-asm.**
- post-call: `(count == 0)` plus the predicate result `0x142128C10` select `[r15+0xC] = 3` vs `0`
  and, when the predicate is true, `0x142124950(rsi, r12d)`. **C-asm.**

---

## 4. Complete caller enumeration of the lane core `0x14063CD90` (O14 item 2)

Direct `E8 rel32` call sites, image-wide scan (no qword reference to the core VA anywhere, so no
indirect/vtable caller): **13**. Containers from `funcs.txt`.

| # | call site | container (DB name) | `s` at the site | verdict |
|---|---|---|---|---|
| 1 | `0x14063D2C4` | `FUN_14063D1C0` (packer C) | its own `r9d` (from `FUN_142123290`: `r14d`) | INF (one caller) |
| 2 | `0x14063D3AF` | `FUN_14063D2F0` (packer B) | its own `r9d` (passed through) | per caller, see below |
| 3 | `0x14063DBD4` | `FUN_14063DAB0` (wrapper D) | `!IsAwaySlot(edx)` hardcoded (`0x14063DB5B`-`0x14063DB64`) | C-asm, opponent |
| 4 | `0x14063DCE7` | `FUN_14063DC10` (wrapper A = LaneFree) | `!IsAwaySlot(edx)` hardcoded (`0x14063DCA6`-`0x14063DCAF`) | C-asm, opponent |
| 5 | `0x142119C83` | `FUN_1421197B0` | `sete` on `[rcx+8]` (inlined isAway idiom) | C-asm, opponent |
| 6 | `0x14211C162` | `FUN_14211BFF0` | `rcx=[rsi+8]`, `eax=[rcx+8]`, guard, `sete r9b` (`0x14211C125`-`0x14211C134`) | C-asm, opponent |
| 7 | `0x14211D911` | `FUN_14211D710` | `eax=[rcx+8]`, guard, `sete r9b` | C-asm, opponent |
| 8 | `0x142122D1D` | `FUN_1421226C0` | `sete` on `[rcx+8]` | C-asm, opponent |
| 9 | `0x1421248AE` | `FUN_1421240B0` | `rcx=[rsi]`, `eax=[rcx+8]`, `sete r14b`, `r9d=r14d` (`0x142124866`-`0x142124899`) | C-asm, opponent |
| 10 | `0x142129076` | `FUN_142128C10` (one-touch predicate) | `!IsAwaySlot(R)`; `R` = the receiver | C-asm, opponent |
| 11 | `0x142129804` | `FUN_142129590` (Cross) | inlined isAway idiom on `edi`, `c = esi`, L = `[rbp-0x70]` (len 5.0, plus 30.0/2.0) | C-asm for the idiom, INF `edi` provenance |
| 12 | `0x14212FD0E` | `FUN_14212F1C0` | `eax=[rcx+8]`, guard, `sete` | C-asm, opponent |
| 13 | `0x1421309F1` | `FUN_14212FE70` | same idiom as #12 | C-asm, opponent |

Packer/wrapper families and their callers (level 2, needed to prove what reaches the core):

| container | core call | callers | `s` they can request |
|---|---|---|---|
| `0x14063DC10` (wrapper A, bool only) | #4 | `0x14063DDFF` in `FUN_14063DD20`, `0x14063DECB` in packer A `0x14063DE20`, plus `0x14063ACF1`/`0x14063ADB8` in `FUN_14063A490` | none; always `!IsAwaySlot(c)` |
| `0x14063DE20` (packer A, L from vec3+theta+7 floats) | via wrapper A | 17 sites: `0x14210FE30`, `0x14211C2FD`, `0x14211F522`, `0x14211F7E2`, `0x14211FBDB`, `0x14212068F`, `0x142124051`, `0x142125311`, `0x14212565A`, `0x142125753`, `0x14212899A`, `0x14212926C` (BackSpace), `0x14212A4EB`, `0x14212ACE9` (GoalFront), `0x14212B7C0` (Safety), `0x14212C485`, `0x1421324F3` | none |
| `0x14063D2F0` (packer B, explicit `s`, caller-visible out) | #2 | 17 sites: `0x142108E6F` .. `0x142109D95` (11, in `FUN_142108630`, `s = [rbp+0x68]` = its own arg -> INF), `0x142119AC7` (opponent, C-asm), `0x142122F2E` (opponent, C-asm), `0x142123C5B` (`s = !edi` -> opponent, INF), **`0x14212A22D` (DHB, TEAMMATE, C-asm)**, `0x142131B22` and `0x142131E1B` (`ebx = [rax+8]`, guard, `s = !ebx` -> opponent, C-asm) | **the teammate branch is reachable here** |
| `0x14063D1C0` (packer C, 1 caller) | #1 | `0x1421236CC` in `FUN_142123290` (`r9d = r14d`, INF) | unknown (INF) |
| `0x14063DAB0` (wrapper D, bool only) | #3 | `0x1421261E6` (`FUN_142125C80`), `0x14212776F` (`FUN_142126B70`), `0x14212A5D9` (`FUN_14212A2D0`), `0x14212BFC5` (`FUN_14212BB00`), `0x142135A7E` (`FUN_142135700`) | none; always `!IsAwaySlot(c)` |

Containment note: call sites `0x14063D2C4` and `0x14063D3AF` looked like "packer B calls the core
twice" in an earlier pass; they are **one site each in two different containers** (`FUN_14063D1C0`
and `FUN_14063D2F0`).

**Answer to O14's question** ("enumerate ALL callers of the lane core entry; if the only callers
are the five listed plus the one-touch G11 call, state that"): the five listed are *not* the only
ones. Eight further unit functions call the core directly (#5-#9, #11-#13), and the four
containers in the table above sit between the rest. The teammate branch is not only reachable but
**taken by DirectHighBall** (§3.5, #2 in the table). O14 is closed for this question; the five
units' channels are in §3.

---

## 5. Corrections to `docs/re/pes-re-lane-core.md` and `docs/ports/pes-port-LanePort.md`

1. §3.6 "the opposite of the reference in every observed caller" and the O14 "the teammate branch
   has no caller yet (INF)": **wrong**, DHB takes it. Update the spec + the port doc's O14 line.
2. §3.6 "Both observed callers of the core are `0x14063DC10` and `0x142129BE0`(?)": there are 13
   direct sites; the containers are `0x14063DC10`, `0x14063DAB0`, `0x14063D2F0`, `0x14063D1C0`.
3. Three of the doc's five "`FUN_`" names are **not function entries in the project's Ghidra DB**;
   they are interior entries of a larger analyzed function (each has its own prologue and is
   reached by a jump):
   - `0x142129110` (BackSpace) is inside `FUN_142128C10` (`0x142128C10`..`0x14212958F`, same
     function as the one-touch predicate `0x142128C10`).
   - `0x14212B520` (Safety) is inside `FUN_14212AEB0` (OneTwo; body `0x14212AEB0`..`0x14212B9BF`).
   - `0x142129BE0` (DirectHighBall) is inside `FUN_142129590` (Cross; body `..0x14212A2CF`).
   GoalFront `0x14212ABA0` and OneTwo `0x14212AEB0` *are* entries (`funcs.txt` confirms).
   An implementer should `create_function` at the three interior VAs (or use the doc's names) or
   the bridge's `decompile_function` will resolve the wrong (outer) body.
   _Note 19-10-26 (pes-re-carrier-helpers.md §6 item 14): the CURRENT bridge DB has NO function containing `0x14212B520` at all ("No function found at or containing address" for both decompile and disassemble); the saved `new_14212b520.c` must come from an older DB state. The raw bytes disassemble cleanly as the Safety gate (ret at `0x14212B8A8`, falls into SideChange code after). BackSpace `0x142129110` likewise has no bridge function (consistent with A4Carrier §4 row 3). The §3.1/§3.3 containment readings above describe the older DB state._
4. The `out` channel: for BackSpace/GoalFront/Safety the caller's `rcx` is **not** the candidate
   buffer. `0x14063DC10` copies its `rcx` into `rbx` and passes it as the core's dead `rdx`
   (`0x14063DCE2`/`0x14063DCC3`), while the core's `out` is DC10's own local `[rsp+0x60]` with
   entries at `[rsp+0x68]`. Those three units get only the bool. Only the direct callers (#5-#13)
   and DHB (packer B) see `out`.
5. Candidate record layout: 8 bytes `{int id; float d2}`; the wrapper pre-fills `{0xff, 0.0f}`
   (`0x14063DC60`) and pre-sets `out[0] = -1`, `out[1] = 0` (`0x14063DC9D`); DHB pre-sets its
   nearest to `0xff` (`0x14212A197`) and reads the count with `0x1421B7D40(out+4)`.
6. `0x14063DE20`'s ABI (needed if a unit is wired by hand):
   `(out rcx [forwarded to the core's dead rdx], c edx, origin vec3* r8, theta float* r9,
   len [rsp+0x20], cone [0x28], rad [0x30], keep byte [0x38], h int [0x40], u2 byte [0x48])`
   -> builds `L` in place and calls `0x14063DC10(out, c, L, keep, h, u2)`.
   `0x14063D2F0`'s ABI: `(out rcx, dead rdx, c r8d, s r9d, origin vec3* [0x20], theta float* [0x28],
   len [0x30], cone [0x38], rad [0x40], keep byte [0x48], h int [0x50], u2 byte [0x58],
   dead int [0x60])` -> calls the core directly, so the caller keeps `out`. **C-asm.**
7. Shared lane constants: BackSpace and GoalFront use the **same lane** 20.0 m / 80 deg / 1.0 m;
   Safety uses 20.0->runtime `sqrt(d2)` / 80 deg / **3.0 m**; DHB uses 15.0 m / 60 deg / 5.0 m.
   (`0x14259BE1C`=20.0, `0x142596E6C`=80.0, `0x14256AE1C`=1.0, `0x14258C264`=3.0,
   `0x14259BE0C`=15.0, `0x14259A764`=60.0, `0x14259222C`=5.0, `0x14259858C`=360.0,
   `0x142598598`=-360.0, `0x142598580`=180.0.) **C-asm.**

---

## 6. Wiring note (what an implementer can do without re-decoding)

- `pes::lane::LaneFree` as ported (fixed `!IsAwaySlot(reference)`) **is verbatim correct for
  BackSpace, GoalFront and Safety**, including the keeper-keeping flag (`keep = 0` for all three)
  and `h = 0`: pass `reference = c`, `origin/theta/len/cone/rad` from §3.1-3.3, `keepKeeper = false`,
  `horizonFrames = 0`. The three differ only in the lane source: BackSpace = a side-level heading
  + the carrier's position; GoalFront = the carrier's heading table value + the carrier's position;
  Safety = the carrier's heading + the carrier's position with `len = sqrt(nearest-opponent d2)`
  and `rad = 3.0`.
- **DHB needs `ScanLane` with an explicit side** (`side = IsAwaySlot(c)`, the teammate branch) and
  `out[1] != 0` (a candidate exists) — the current `LaneFree` cannot express it. This is the
  missing capability the doc flagged as "no caller": now it has one.
- OneTwo needs `ScanLane` with `side = !IsAwaySlot(receiver)` and the snapshot lane (15/60/2,
  `h = 0`, `keep = false`).
- The direct-core units (#5-#9, #11-#13) all pass `s = opponent`, `keep = 0`, `h` from an int
  conversion of a float distance, and their own `out`; they are the same shape as `LaneFree`
  except for `h`.

## 7. Open items / INF list (with the blocker)

1. `0x1415A1780` (used by Safety as `len = f(d2)`, called at `0x14212B76F`): **OPEN** — it is an
   imported CRT thunk; the dump's IAT slot `0x1425301F0` holds the live HBASE+VA and the image has
   no import directory, so the name cannot be resolved from the file. INF: `sqrt`.
2. `0x141E532E0` (DHB's `h = ret >> 2`, `0x14212A1CC`): **OPEN** — semantics unknown. INF: a
   distance/step helper.
3. `0x140A48A30` + `0x250` (BackSpace's theta, `0x14212917D`/`0x1421291CE`): **INF** — it is the
   side object's float; whether it is the side's facing/attack direction is not shown here.
4. `0x142129590` (Cross) site #11: the `edi` that feeds the `sete` idiom is not resolved in this
   session; the `L` build (`len 5.0` + `30.0`/`2.0`) is read but the two slots' order is INF.
5. `FUN_142123290` (packer C's single caller) and the 11 `FUN_142108630` packer-B callers: their
   `s` is an argument, not an inlined side test -> **INF**.
6. `FUN_14063A490`'s two wrapper-A calls (`0x14063ACF1`, `0x14063ADB8`) are outside the five units;
   not analyzed.
7. Non-direct callers: none found (0 eight-byte image references to any of
   `0x14063CD90`/`0x14063DE20`/`0x14063DC10`/`0x14063D2F0`/`0x14063D1C0`/`0x14063DAB0`).

## 8. Reproducing

```
# enumeration: direct calls to a VA
python3 - <<'EOF'   # scan for E8 rel32 == target, and for qword pointers == target
EOF
# containers: bisect funcs.txt
# per-site asm: objdump -D -b binary -m i386:x86-64 -M intel \
#   --adjust-vma=0x140000000 --start-address=.. --stop-address=.. pes_full.bin
# constants: struct.unpack('<f', f.read(4)) at va-0x140000000
```
Earlier scratch from this lane's decode session: `asm_backspace.txt`, `asm_goalfront.txt`,
`asm_safety.txt`, `asm_dhb.txt`, `funcs.txt` (all under `<workdrive>/gf-LaneStruct-scratch/`).
