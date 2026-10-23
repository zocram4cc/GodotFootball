# PES mark table: T+0xB7D8 writer, cone test, clear gate, reader payloads (MarkConeRE, 23-10-26)

Title: T7a blockers closed to port spec. `T+0xB7D8` = per-team within-side slot word (0..10, 0xFF empty), writers CONFIRMED. Cone `FUN_140A53DA0/53DF0` CONFIRMED at the bytes. Clear gate `FUN_140579E10` + teardown CONFIRMED. Three presence readers CONFIRMED as tests, payloads INF.

Read-only RE. Image `pes_full.bin`, file offset = VA − 0x140000000. Bridge `http://127.0.0.1:8080`.
Scratch: `<workdrive>/gf-MarkConeRE-scratch/` (`t7a.dec`, `teamtick.dec/asm`, `r1/r2/r3.dec`, `clear1/2.dec`, `check_markcone.py`).
Prior: `pes-re-mark-table.md` (pair writer, T7a rule with 7.0 correction) stands; §1 below answers its OPEN B7D8, §2 its OPEN cone.

## 0. Constant table (script-read from the image, never typed)

```
0x14259BE5C f32= 90.0 bits=0x42B40000   (coneDeg)
0x14259BE04 f32= 12.0 bits=0x41400000   (cone outerLen)
0x14259BE44 f32= 64.0 bits=0x42800000   (4EAF0 gate radius)
0x142565600 f32= 0.5 bits=0x3F000000    (half-cone scale)
0x1425795E4 f32= 6.2831855 bits=0x40C90FDB (2pi)
0x14259BE40 f32= 57.29578 bits=0x42652EE0 (180/pi)
0x142565D30 bits=0xFFFFFFFF (abs-mask low word, NOT a value; next words 0x7FFFFFFF)
0x142578480 f32= 3.1415927 bits=0x40490FDB (pi)
0x1425A35A0 f32= 1.5707964 bits=0x3FC90FDB (pi/2)
0x14260B394 f32= -1.5707964 bits=0xBFC90FDB (-pi/2)
0x142609F98 f32= -3.1415927 bits=0xC0490FDB (-pi)
0x142591140 f32= 30.0 bits=0x41F00000   (A06BF0 bearing limit)
0x14259D60C f32= 9.0 bits=0x41100000    (A06BF0 d2 limit)
0x14259BDF0 f32= 7.0 bits=0x40E00000    (T7a ramp base; NOT 8.0)
0x14258C264 f32= 3.0 bits=0x40400000    (T7a ramp top add)
0x14259BE0C f32= 15.0 bits=0x41700000   (T7a cone offset)
0x1425C48E4 f32= 1369.0 bits=0x44AB2000 (37^2 ceiling)
0x1425C48D0 f32= 105.0 bits=0x42D20000  (wrap test 1)
0x142596E6C f32= 80.0 bits=0x42A00000   (wrap tests 2+3)
```

## 1. T+0xB7D8: writer FOUND, meaning CONFIRMED

**Meaning: per-team within-side slot word, range 0..10, `0xFF` = empty.** It is NOT the designation id. Every read builds the full target as `side*11 + B7D8`:

- T7a `FUN_1404F61E0` at `0x1404F620D MOVSXD RBP,[RDX+8]` (side) / `0x1404F6211 IMUL R13D,EBP,11` / `0x1404F6215 ADD R13D,[R8+0xB7D8]` / `0x1404F621C CMP R13D,0x15; JA exit`. So `tgt = side*11 + B7D8`, exit unless `< 22`.
- `FUN_140A06B50` reader at `0x140A06BED MOV EAX,[R8+0xB7D8]`; builds `uVar9 = uVar7*11 + B7D8` (decompile; `uVar7 = slot%11` side) and **returns 0 immediately if `[B7D8] == 0xFF`**. Fast path at `0x140A06B83 CMP EDX,[R8+0xB7DC]`: if `side%11 == [B7DC]` then `*param_5 = 0x11`, return 1.

**Writers (raw `disp32 0xB7D8` scan: 49 hits; classification by opcode bytes):**

- `FUN_140546FB0` (`0x140546FB0`–`0x140547849`, CONFIRMED by decompile): `0x14054743D C7 86 D8 B7 00 00 FF 00 00 00` = `*(param_3+B7D8) = 0xFF` (match arm: `[param_1+0x1C] == FUN_1405969B0(...)`, sets `[param_1+0x18] = 1`); `0x140547814 44 89 B6 D8 B7 00 00` = `*(param_3+B7D8) = uVar10` (live arm), where `uVar10 = [param_1+0x1C] % 11` (`< 22`, else 0; decompile head). Same function also keeps `uVar9 = [param_1+0x20] % 11` and `uVar11 = holder % 11 or 0xFF`.
- `FUN_140549D30` (CONFIRMED by decompile): `0x1405497FD 41 C7 86 D8 B7 00 00 FF 00 00 00` (`= 0xFF` match arm, sets `[param_1+0x18] = 2`); `0x140549E0D C7 87 D8 B7 00 00 FF 00 00 00` (`= 0xFF`); `0x140549E34 89 8F D8 B7 00 00` (`= uVar2`, live arm, `uVar2 = [param_1+0x24] % 11`-family slot). Also writes the sibling slot word `B7A0[uVar10] = uVar9 / 0xFF` at the tails.
- `FUN_14057A1F0` (`0x14057A1F0`) / `FUN_14057A3E0` (`0x14057A3E0`): bulk-clear writers, `0x14057A333 C7 87 D8 B7 00 00 FF 00 00 00` / `0x14057A510 C7 83 D8 B7 00 00 FF 00 00 00` = `*(T+B7D8) = 0xFF` as part of the teardown (§4). Gated on `FUN_140579E10 != null && [T+0xB6E8+i] != 0`.
- Remaining raw hits (`0x14053F99C`, `0x140540F70`, `0x140544430/439/8E7/8F0/4B55/4B5E`, `0x14054EFD8/FE0`, `0x140551441/695/69D/1C84`, `0x140553055/32A5`, `0x14055658C`, `0x14055AAAAC`, `0x14055F4F9`, `0x14056232C`, `0x140563712/A37`, `0x14056BA02/AD7`, `0x14056CA99/AA4`, `0x14056D072`, `0x14056FDA1`, `0x140572089`, `0x140573DA0/DA9`, `0x14054B71E/A2E`, `0x14054C816`, `0x14055A42C`, `0x14055FBDE`, `0x14080E32C`): same `89/C7 … D8 B7 00 00` store shapes (slot copies, `0xFF` clears) in team-slice code the bridge has no function body for ("No function found" = inside unanalysed/JIT-adjacent bytes, not decoded). OPEN, not needed: the three writers above already fix range, sentinel and lifetime.

**When written:** on every live arm of `546FB0/549D30` (slot `% 11` recomputed from `param_1+0x1C/0x20/0x24` words each call) and `0xFF` on the match/clear arms; bulk `0xFF` on the §4 teardown. No attributes, no RNG, no ball.

**GF carrier: none (grep for `B7D8/markTable` in `src/`+`tests/` empty). Needs a NEW per-team within-side slot word** (0..10 + `0xFF` empty), e.g. beside `TeamTick`, plus `markTable[11]`. It is not `DesignationRecord` (that names a global slot; B7D8 is side-relative and pairs with `side*11` at both readers).

Status: **CONFIRMED** (writers, range, sentinel, both readers). Unresolved siblings list OPEN (no port impact).

## 2. Cone test FUN_140A53DA0 / FUN_140A53DF0: CONFIRMED at the bytes

Call site (T7a `0x1404F653C`–`0x1404F6567`): `FUN_140A53DA0(TA, holderSlot, candPos=pl+0x55C, headArg=P[cand][holder], 90.0@[0x14259BE5C via 0x1404F653C], 12.0@[0x14259BE04 via 0x1404F63BF→0x1404F6558], ~0.0)`.
Partner gate: `FUN_140A4EAF0(TA, cand, headArg, 64.0@[0x14259BE44 via 0x1404F656C], 12.0, low-byte-0, 1)` (`0x1404F656C`–`0x1404F6598`).

- Wrapper `FUN_140A53DA0` (`0x140A53DA0`–`0x140A53DE3`, dis CONFIRMED): saves head (`XMM3→XMM6` at `0x140A53DAD`), resolves holder record `CALL 0x140A48A10` (`0x140A53DBF`, EDX = holder slot inherited from caller), `LEA RDX,[RAX+0x55C]` = holder pos (`0x140A53DCD`), restores `R8` = candidate pos, `JMP 0x140A53DF0`.
- Inner `FUN_140A53DF0` (`0x140A53DF0`–`0x140A53EA4`, dis CONFIRMED): `d² = (hx−cx)² + (hz−cz)²` (`0x140A53DF6`–`0x140A53E1C`; both differends read `[RDX]`/x and `[RDX+8]`/z — origin vs candidate on the ground plane). Strict annulus: `outer² vs d²` (`0x140A53E20/26/2D JBE exit`), then `inner² vs d²` (`0x140A53E2F/38/3F JBE exit`); touching either radius FAILS. `half = 0.5@[0x142565600] × coneDeg` (`0x140A53E49/4F`). Live bearing `CALL 0x140A439EF0` (`0x140A53E57`, RCX = candidate pos, RDX = holder pos → bearing(cand→holder), §2.1). `CALL 0x140A43A0A0` (`0x140A53E62`, XMM0 = headArg, XMM1 = live bearing → wrapped `bearing − head`). fabs via `ANDPS mask@[0x142565D30]` (`0x140A53E76`), `COMISS half,result` (`0x140A53E81`), `CMOVA` true. **TRUE iff `inner² < d² < outer²` and `|wrap180(liveBearing − headArg)| < 0.5*cone`.**
- Inner-radius quirk (INF, port as 0.0): T7a writes `[RSP+0x30] = 0.0` (`0x1404F654F`) then overwrites its low byte with 1 (`0x1404F6580`); the dword reaching `53DF0` is `0x00000001` ≈ 1.4e-45, i.e. effectively 0. Named so the port does not invent a 1.0 m inner radius.
- Head arg = `P[cand][holder]` (`0x1404F6544 MOVAPS XMM3,XMM6`, XMM6 = second `439FE0` read). With the §6 wrap-pass caveat the table at read time holds the pair row bytes; port reads the direct pair value (all readers index it directly).

### 2.1 Bearing/wrap/atan helpers (CONFIRMED shapes; polynomial bit-exactness INF)

- `FUN_140439EF0` (dis CONFIRMED): coincident `(x,z)` → 0.0 (`0x140439EF4`–`0x140439F17` UCOMISS/JP/JNZ chain); else `dx = RDX.x−RCX.x, dz = RDX.z−RCX.z`, `CALL 0x140A92D40`, `if (r<0) r += 2π@[0x1425795E4]` (`0x140439F2C/31`), `r *= 180/π@[0x14259BE40]` (`0x140439F39`). So `bearingDeg(dx,dz) = wrap360(deg(atan2(dx,dz)))`.
- `FUN_14043A0A0` (decompile CONFIRMED): fold each input once by ±360 (`[0x14259858C]`), return `b−a` pulled back by one ±360 when `|b−a| > 180@[0x142598580]` (fabs via abs-mask). = wrapTo(−180,180]-ish single-fold diff. Matches GF `WrapDeg`.
- `FUN_140439E50` (decompile CONFIRMED): same coincident-zero arm, bearing, then single-fold into [0,360). Used by `FUN_1404AB8E0` (`0x1404AB8E0`: `439E50(out, RCX+0x55C, RCX+0x5EC)`) which yields T7a's candidate "head" (`0x1404F64A8 CALL`, bytes `E8 33 54 FB FF` → target `0x1404AB8E0`). The `+0x5EC` vector semantics (velocity vs facing) is INF — bytes CONFIRMED, meaning OPEN ("none: needs head-vector X").
- `FUN_140A92D40/140A92DC0` (decompile CONFIRMED): quadrant atan2 shell (`±π/2@[0x1425A35A0]/[0x14260B394]`, `±π@[0x142578480]/[0x142609F98]` for `x<0`) over the `140A92DC0` polynomial (`|t|` bands at `0x3EDFFFFF/0x3F980000/0x401C0000`, constants `0x142656158 = 0.78539818`, `0x14265615C = 0.98279375`, `1.5@[0x14259BDDC]`, `1/3@[0x1425903B4]`, sign flip `^0x80000000@[0x1425656D0]`). Port labels `std::atan2` an approximation (lane-core O3 inherited).

### 2.2 Side test + head gates in T7a (CONFIRMED addresses)

- `FUN_140A4C720(holderX, candX, sign, cone)` pure (`0x1404F6477 CALL`; decompile: `(sign*cone + holderX)*sign < sign*candX`). No GF state beyond its four inputs — direct port.
- `pl+0x5D0 ∈ {1, 0x3F}` (`0x1404F6484 CMP 1 / 0x1404F648F CMP 0x3F`). GF input: none — needs action/state word (OPEN).
- `sign = [T+0x254]` byte (`0x1404F6316`), `cone = ramp(sign*holderX) − 15.0` with base `wind?−7.0@[0x14259BDF0]` (`0x1404F6354`), top `+3.0@[0x14258C264]` (`0x1404F635C`), clamp ladder (`0x1404F6364`–`0x1404F638A`), `−15.0@[0x14259BE0C]` (`0x1404F638F`). `wind? = [[DAT+0x2AA8]+0x18]` (`0x1404F633C/343`). GF: sign exists in team-tick context (wireable, confirm); the `+0x2AA8` object is "none: needs ramp-base X" (OPEN).
- NL row: `LEA R15,[R8+0x22E4C] + holder*0xC8` (`0x1404F62FA/30A/31D`); 11-slot walk with `side*11+slot` addressing when `side<2` (`0x1404F63E0`–`0x1404F6434`); skips `id≥22/==holder/==tgt`; **hard exit (return, not continue) if `d² > 1369.0@[0x1425C48E4]`** (`0x1404F6439/43D JA 0x1404F65F1`, CONFIRMED); three wrap tests `≤105.0@[0x1425C48D0]` (`0x1404F64D9`), `≤80.0@[0x142596E6C]` (`0x1404F650D`), `≥80.0` floor on the same XMM10 (`0x1404F6532 COMISS XMM10,XMM2; JA skip`, CONFIRMED); commit `markTable[id%11]=1` (`0x1404F65CF`, `%11` via `MUL 0xBA2E8BA3` at `0x1404F65AA`–`0x1404F65CA`), stop after 3 (`0x1404F65D7 CMP R9B,3; JNC return`).

## 3. Cone partner FUN_140A4EAF0: wrap prologue CONFIRMED, counter OPEN

- `FUN_140A4EAF0` (`0x140A4EAF0`–`0x140A4EAF0+0x83`, dis CONFIRMED): single-folds both float args by ±360 (`[0x14259858C]`, `0x140A4EAF4`–`0x140A4EAF0+0x4B`), then `CALL 0x140A4E7C0` with `(u,v, 0, lowByte0, 1)`.
- `FUN_140A4E7C0` (decompile read, OPEN): per-opponent scan over the holder's side rows with `d² < 64²`-family radius test (`param_5²`), live atan2 bearings, `140A4C4E0/140481B00` correction, counted against a threshold; returns a count in AL. T7a commits iff `count <= (coneResult != 0)`. Exact count semantics need their own decode — **third P2 blocker**, VAs `0x140A4E7C0`, callees `140A48A10/140A6C6D0/140A4C4E0/140481B00/141E532D0`.

## 4. Clear gate FUN_140579E10 + teardown: CONFIRMED

- `FUN_140579E10(param_1, idx)` (decompile CONFIRMED): if `idx < 0x30` return `table[idx]` where `table = {param_1+0, +0x20, +0x60, …, +0x898}` (48 entries; `[0x14] = 0`, `[0x2F] = 0`); else NULL. Pure index→object-pointer fetch; the "gate" is existence of the slot object.
- Teardown `FUN_14057A1F0` (all-48 loop, skips `param_4`) / `FUN_14057A3E0` (single index) (both decompile CONFIRMED): require entry non-null AND `[T+0xB6E8+i] != 0` (then clear it); call `vtbl+0x20` release; per-slot `A2A0`-row reset (`[0x52A] = 0x30`, `[0x535] = 0`, `[0x540] = 0xFF`, `140A40820` when `*row ∈ {1,0x22}`, `140A84E30`); then bulk: `B7D8 = 0xFF`, `B7DC..B7E4 = 0xFF…/0`, **`B7EC..B7F6 = 0`**, `B824 = 3`, `B7F8..B820 = 0`. This is the mark table's lifetime end.
- Callers: `FUN_140579840` (pre-T7a prep: registry-gated `140A40BD0/140A604D0` + phase gates, else all-48 clear) → `FUN_140579BF0` (per-index `14057A070`/role-count loop, `14057A3E0` on hit) → T7a `FUN_1404F61E0` at `0x1404F76AC` inside `FUN_1404F6C30` (team tick, after `579840/579BF0`, before `57D240/4F9DC0`; dis CONFIRMED). So T7a runs in the team tick on state at most one S1 pass old (pair writer placement per prior doc §3 unchanged).

## 5. Presence-test readers: TEST role CONFIRMED, payloads INF (GF has no reader)

All three are per-slot `CMP byte [T+slot+0xB7EC],0` walks over 11 slots:

- `0x140969974` in `FUN_1409696A0` (bytes `41 80 BC 01 EC B7 00 00 00 / 0F 85 …`, CONFIRMED): any-mark scan whose hit jumps out of the slot loop; function also calls `439FE0` ×3 (`0x140969A26/B01/25/35` per prior doc) and `43A0A0`, verdicts into `[param_1+0xC4]` / duel word `[...+0x8D4]` paths (`0x11/0x10/0x24`-family stores nearby, `param_5 == 3` gated). GF consumer: duel/press branch reading `markTable` (e.g. near `pesoffball.cpp` trap/press predicates). Exact branch OPEN.
- `0x140993253` in `FUN_1409931C0` (bytes `80 BC 08 EC B7 00 00 00 / 75 0B …`, CONFIRMED): hit sets frame flag `[RSP+0xF0] = 1` (`0x140993265`), consumed at `0x14099369C CMP [RSP+0xF0],0`; same duel family, also `43A0A0` wraps nearby. Payload INF.
- `0x1409ADB44` in `FUN_1409AD830` (bytes `41 80 BC 1E EC B7 00 00 00 / 75 0B …`, CONFIRMED): hit breaks the 11-scan (`bVar14 = true`); function is a duel verdict writer (`[param_3+0x8D4] ∈ {2,4,0x10,0x11}` on `pl+0x54/0x5D0/0x5F5/0x7C` + `43A0E0/439FE0/439EF0/43A0A0` gates, `90.0@[0x14259BE5C]` floor at the mark-gated arm). Payload INF.
- Prior false hit `0x14051A996` (ADD-neighbour, never reads B7EC), uncontained `0x14054B729`, data-region hits unchanged (prior doc §4).

GF mapping: no consumer exists yet. A wired T7a stays inert until a duel reader is ported (as TeamT2 already notes).

## 6. Port spec for MarkTablePort

- **P0 — new state (no GF carrier, grep CONFIRMED empty):** per-team `markSlot` word (`B7D8`: 0..10, `0xFF` empty) + `markTable[11]` bytes (`B7EC`). Writers: §1 arms + §4 teardown (`0xFF` + zero). Clear together on the §4 path.
- **P1 — pair table (CLOSED input):** unchanged from prior doc §5-P1 (`pair[22][22]`, S1 rebuild, do NOT port the second-pass wrap-back: overwrite CONFIRMED at `0x14045BDE3`–`0x14045BE14` through the row helper; source-column load lacks a column index in the disassembly read, so its exact broadcast semantics are INF — and no reader wants the wrapped value).
- **P2 — mark build (port when P0 lands):** §2.2 rule verbatim with the 7.0 base; cone = §2 (`0.5*90.0`, outer 12.0, inner 0.0 + quirk note); side test `4C720` direct; T7a commit/`%11`/cap-3/hard-exit as cited. Still OPEN inputs: `B7D8` (CLOSED by P0), `+0x2AA8/+0x18` ramp base (needs X), `pl+0x5D0` word (needs X), `+0x5EC` head vector (needs X), `4EAF0/4E7C0` count gate (needs decode §3).
- **P3 — clear (do not port yet):** §4 teardown incl. `B7EC`; gate `579E10` is a plain object-table fetch (port trivially when the teardown path lands).
- **Readers (do not port yet):** §5 addresses are the duel/press predicates that will consume `markTable`.

## 7. Checker

`<workdrive>/gf-MarkConeRE-scratch/check_markcone.py` — self-checking, no PES bytes in repo: asserts the §0 script-read constants (incl. 7.0-vs-8.0), helper arithmetic (`0x228D4+0x16A8 == 0x23F7C`), and the §2 rule on synthetic geometry (ahead recorded / behind rejected / outside-outer rejected). **17/17 green; mutations 5/5 killed** (`90→91`, `12→13`, `1369→1368`, `64→65`, `0.5→0.6` each fail exactly one check, reverted).

## 8. Open questions

1. `140A92D40/2DC0` bit-exactness (lane-core O3) — bearings match to ~1e-7, not bits.
2. Second-pass wrap source column (load at `0x14045BDE3` has no column index) — INF, no port impact per P1 verdict.
3. `4EAF0/4E7C0` count gate — needs its own decode (VAs §3).
4. `+0x2AA8/+0x18` ramp base, `pl+0x5D0`, `+0x5EC` head vector, `140A6E320` class-table row — each names its GF `needs <X>`.
5. Duel-reader verdict branches (§5) — which GF predicate each feeds.
