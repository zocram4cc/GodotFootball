# PES mark table (T7a) + per-holder pair table: RE report (MarkTableRE, 23-10-26)

> **Main audit 23-10-26 (bytes):** `0x1404F6354 subss xmm1, [rip+0x20A5A94]` → `0x14259BDF0` = 7.0f (the ramp base); `FUN_1404587F0` = `lea rax, [rcx+0x16A8]; imul rdx, i, 0x58; add` and 0x228D4 + 0x16A8 = 0x23F7C: match.

Title: T7a mark-table rule re-verified; `TM+0x23F7C` writer found (split displacement via row helpers); mark-table reader roles decoded.

## 0. Constant table (script-read from `pes_full.bin`, `offset = VA − 0x140000000`)

Script: `<workdrive>/gf-MarkTableRE-scratch/floats.py` + `check_floats.py`. Output pasted:

```
0x1425795e4 f32=6.2831854820251465 bits=0x40c90fdb   (2pi)
0x14259be40 f32=57.2957763671875 bits=0x42652ee0    (180/pi)
0x142598580 f32=180.0 bits=0x43340000
0x14259858c f32=360.0 bits=0x43b40000
0x142598598 f32=-360.0 bits=0xc3b40000
0x14259be5c f32=90.0 bits=0x42b40000
0x14259be44 f32=64.0 bits=0x42800000
0x14259be04 f32=12.0 bits=0x41400000
0x142596e6c f32=80.0 bits=0x42a00000
0x14259bdf0 f32=7.0 bits=0x40e00000
0x14258c264 f32=3.0 bits=0x40400000
0x14259be0c f32=15.0 bits=0x41700000
0x14256ae1c f32=1.0 bits=0x3f800000
0x1425c48e4 f32=1369.0 bits=0x44ab2000   (37^2)
0x1425c48d0 f32=105.0 bits=0x42d20000
0x142565d30 bits=0xffffffff u64=0x7fffffffffffffff (fabs abs-mask, NOT a value)
```

## 1. Writer of `TM+0x23F7C`: FOUND — `FUN_14045B420` (CONFIRMED)

**Why the direct search failed:** there is NO direct `disp32 0x23F7C` store in the image. A raw opcode scan of all
`F3 0F 11` / `0F 11` / `0F 29` / `C7` / `89` stores with disp in `[0x22000, 0x25000]` finds only `0x228Dx` (S1 head),
`0x24xxx` (other MA blocks) and one `0x24FDC` — zero hits covering `0x23F7C` (`writerscan*.py` in scratch).
The writer reaches the table through two row helpers, so the displacement is split:

- `FUN_1404587F0(base, i)` = `base + 0x16A8 + i*0x58` (decompile CONFIRMED this session).
  Called with `base = local_198 = MA+0x228D4`, so the row = `MA+0x228D4+0x16A8+i*0x58` = **`MA+0x23F7C+i*0x58`**.
  Arithmetic: `0x228D4+0x16A8 = 0x23F7C` (verified by script). The pair row for holder `i` is 22 f32 (`0x58` = 88 bytes; whole block 22 rows = `0xA660` bytes).
- `FUN_140458810(base, i)` = `base + (i+7)*200` (decompile CONFIRMED this session).
  Called with the same base, so the NL row = `MA+0x228D4+(i+7)*200` = **`MA+0x22E4C+i*200`** (`0x228D4+1400 = 0x22E4C`, verified).
  This is the same NearestList the T7a reader walks.

**Writer body** (`FUN_14045B420`, bridge decompile + `dis_14045B420.txt` 618 lines saved in scratch):

- Entry: `param_1` = TA object; `lVar16 = *(param_1+0x508)` = MA; `local_198 = MA+0x228D4`.
- The pair/NL section is UNGATED (runs on every call — it is outside the `param_2 & 1/2/4` arms; CONFIRMED by the
  decompile: the owner loop `do { ... } while (owner < 0x16)` follows all three masked arms).
- Per owner `a` in `0..21` (`EBX`, `0x14045BB80` loop head): if on-pitch (`[MI+0x1748+a] != 0`, `0x14045BBF3/FF/03`):
  - Zero/scratch the pair row: `LEA RDI,[base+0x16A8]` gated on `a < 0x16` (`0x14045BBE2 CMOVL`), `REP STOSD`
    22 dwords (`0x14045BBE6/EBE8/EBEA`; `ECX=0x16`). So each call clears all 22 f32 of row `a` first.
  - Inner loop over `b` in `0..21` (`EDI`, `0x14045BC30..0x14045BD27`): skip `b == a` (`0x14045BC30 CMP EBX,EDI;
    JZ skip`); else:
    - d²(i=b? see below) into the NL row via `FUN_140458810` (`0x14045BC6E CALL`; store pair `(id, d²)` at
      `0x14045BC96/CA6`; count `INC` at `0x14045BCAD`).
    - Bearing store: `XMM0 = pl[a].x − pl[b].x` (`0x14045BCC9/C1`), `XMM1 = pl[a].z − pl[b].z` (`0x14045BCB5/BD`)
      — note the order: differences of (owner − other); coincident test against zero (`0x14045BCC6..CE3`,
      `UCOMISS/JP/JNZ` chain; equal → `XMM6 = 0.0` via `MOVAPS XMM6,XMM9` where XMM9 = 0); else
      `CALL FUN_140A92D40(dx, dz)` (`0x14045BCF3`), `if (r < 0) r += 2π` (`0x14045BCFB COMISS; 0x14045BD01 ADDSS XMM11`;
      XMM11 = 2π from `0x1425795E4`), `r *= 180/π` (`0x14045BD06 MULSS XMM12`; XMM12 from `0x14259BE40`).
    - Commit: `CALL FUN_1404587F0` (`0x14045BD10`) then `MOVSS [RAX+RBP],XMM6` (`0x14045BD15`), where
      `RBP = 4*b` running (`ADD RBP,4` at `0x14045BD20`). So **`P[a][b] = bearing(a→b)` in degrees**.
  - Second pass (`0x14045BD2D..0x14045BE35`, `EBX` = owner bound): copies the NL d² of the owner's row into the
    per-team sorted rows via `FUN_140458810` (`0x14045BDA1 CALL`, stores `0x14045BDC4/D4`), then the wrap
    `v = P + 180; if (v >= 0) { if (v >= 360) v -= 360 } else v += 360` (`0x14045BDE3..E03`;
    XMM10/XMM7/XMM8 = 180/360/−360 from `0x142598580/58C/598`) written back through `FUN_1404587F0`
    (`0x14045BE0A CALL`, `0x14045BE14 MOVSS`). **This OVERWRITES the bearing just stored with a wrapped copy.**
    Same overlap S1Watch reported for A_tab — here it is inside the pair rows themselves.
  - Tail: `CALL FUN_140A79640` rank-sort (`0x14045BE3E`).
- Inputs per pair: only the two players' on-pitch positions (`pl+0x55C` = x, `pl+0x564` = z) and the on-pitch
  byte (`MI+0x1748+i`). No attributes, no RNG, no ball. When: every `FUN_14045B420` call (see §3 for tick placement).

**Semantics (CONFIRMED): `P[a][b]` = `deg(atan2(x_a − x_b, z_a − z_b))`, wrapped toward [0,360), 0.0 when coincident.**
This matches the readers' use (bearings compared with `FUN_14043A0A0` wrap180 diffs) and GF's `PesBearingDeg`
(contract: `atan2(dx, dz)` in degrees wrapped to [0,360); `pesdodge.hpp:252`). The atan core `FUN_140A92D40` itself
is still INF per lane-core O3 (PES polynomial vs `std::atan2` — port must label the approximation).

**`FUN_140439FE0` reader (CONFIRMED, re-read this session):**
```c
if (a < 0x16) return *(MA + 0x23F7C + a*0x58 + b*4); else return *(MA + 0x23F7C + b*4);
```
i.e. `P[a][b]` with the row-guard fallback. The `T18+0x58·a` stride in lane-core §8 / press-coord §11.7 is the same.

**Status: writer CONFIRMED; per-pair formula CONFIRMED (asm-verified operand order + coincident-zero arm);
second-pass wrap-overwrite CONFIRMED (asm); atan core INF (O3 inherited). 27A0-unbound-7 is CLOSED.**

## 2. `FUN_1404F61E0` (T7a) re-verified at the bytes (CONFIRMED, one correction)

Bridge disassembly re-read in full this session (243 lines). TeamT1's decode stands with ONE correction:

- **CORRECTION: the ramp base is 7.0, not 8.0.** `0x1404F6354 SUBSS XMM1,[0x14259BDF0]`, and the script-read value at
  `0x14259BDF0` is `7.0` (`0x40E00000`), not 8.0. (tick-steps2 §6 says "SUBSS 8.0 (`[0x14259BDF4]`)": the digit after
  `BDF` matters — `BDF0` = 7.0, `BDF4` = 8.0 per `check_floats.py`; the instruction's operand VA is `...BDF0`.)
  Any port citing 8.0 here must be re-pointed to 7.0.
- Rest CONFIRMED as written: zero-init `T+0xB7EC..B7F6` (`0x1404F61F7/1FE/206`); `uVar17 = i*11 + [T+0xB7D8]`
  (`0x1404F620D/211/215`); `uVar17 < 0x16` gate (`0x1404F621C/220`); holder `FUN_140A627E0` + `MA+0x2483C` fallback
  (`0x1404F623B/247/24C`); `FUN_140A6E320` + `holder != target` gates (`0x1404F626D/272/27A/27D`); NL row
  `LEA R15,[R8+0x22E4C]` + `IMUL RCX,RSI,0xC8` = holder*200 (`0x1404F62F6/2FA/30A/31D`); slot walk 11
  (`0x1404F63E0..65EB`, `INC R14B; CMP R14B,0xB; JC`); `ADDSS 3.0` (`0x1404F635C [0x14258C264]` = 3.0);
  `-15.0` (`0x1404F638F [0x14259BE0C]` = 15.0); d² ceiling 1369.0 = 37² (`0x1404F6439 [0x1425C48E4]`);
  105.0 floor (`0x1404F64D9 [0x1425C48D0]`); 80.0 (`0x1404F650D [0x142596E6C]`); the two `FUN_140439FE0` pair reads
  (`0x1404F64BA` = `P[cand][target]`, `0x1404F64EB` = `P[cand][holder]`); `FUN_14043A0A0` wrap-diffs;
  `FUN_140A53DA0` args (90.0 at `0x1404F653C [0x14259BE5C]`, 12.0 at `0x1404F6558 [0x14259BE04]`) and
  `FUN_140A4EAF0` args (64.0 at `0x1404F656C [0x14259BE44]`); commit `MOV [RAX+RCX+0xB7EC],1`
  (`0x1404F65CF`) with `id%11` (`MUL 0xBA2E8BA3; SHR 3; IMUL 11; SUB` at `0x1404F65AA..65CA`) and max-3
  (`CMP R9B,3; JNC` at `0x1404F65D7/DB`).

Rule (port spec, inputs named to GF state or "none"):
1. Zero `T+0xB7EC..B7F6` (11 bytes). GF: new `markTable[11]` (no counterpart exists — grep for
   `B7EC/markTable` in `src/` + `tests/` is empty, CONFIRMED this session).
2. `tgt = slotBase*11 + [T+0xB7D8]`; exit unless `tgt < 22`. GF input: per-team slot base + `T+0xB7D8`
   (designation id; GF has no `T+0xB7D8` — **none: needs designation word**).
3. `holder = FUN_140A627E0(...)`, else `MA+0x2483C`; exit unless `< 22`. GF: holder (GF has carrier/holder
   in team AI; wireable).
4. Exit if `FUN_140A6E320(holderClass) != 0` or `holder == tgt`. GF: holder class gate (needs role-class read).
5. `cone = ramp(sign·[pl+0x55C]) − 15.0` with ramp base 7.0 / top `+3.0` (see correction). GF: sign + holder x.
6. For each of the holder's 11 NL-row entries (id, d²): skip `id ≥ 22`, `id == holder`, `id == tgt`;
   **hard exit (return, not continue) if `d² > 1369.0`** (`0x1404F643D JA` leaves the function — CONFIRMED);
   skip unless `FUN_140A4C720` side test, `pl+0x5D0 ∈ {1, 0x3F}`, `|wrap180(P[cand][tgt] − head)| ≤ 105.0`,
   `|wrap180(P[cand][holder] − head)| ≤ 80.0`, `80.0 ≤ |wrap180(P[cand][holder] − holderHead)|`
   (third test's floor is the same 80.0 XMM10 — CONFIRMED `0x1404F6532 COMISS XMM10,XMM2; JA skip`),
   and `FUN_140A4EAF0(...) <= (FUN_140A53DA0(...) != 0)`. GF pair inputs: **pair table from §1**
   (now CONFIRMED-writable); `FUN_140A53DA0/53DF0` cone is OBS per TeamT2 (still OPEN).
7. Commit `markTable[id % 11] = 1`, stop after 3.

## 3. Tick placement of the pair writer (CONFIRMED)

`FUN_140457070` decompiled this session: `FUN_14045B420(TA, mask)` is called every pass (`0x140457119` region),
with `mask = {MI+0x1198 & 3 → 0: 0x01, 1: 0x02, 2: 0x24, 3: 0x10}`, or `0xFF` when the flag arg is nonzero.
The pair/NL owner loop is outside the mask arms, so **the pair table is rebuilt on every S1 call regardless of mask**.
S1 runs from the listener step-5 path (`FUN_140426760 ← FUN_1404582C0 ← FUN_140458880 ← FUN_140457070`), NOT the
team tick `FUN_1404F6C30` (per `pes-re-s1-writer.md` §6, unchanged). T7a therefore reads a table that is at most
one S1 pass old — GF must order the pair rebuild before the mark build in its tick.

## 4. Mark-table readers: `+0xB7EC` roles (CONFIRMED addresses, INF names)

Image scan re-run: 12 `disp32 0xB7EC` hits (script `scandisp.py`): the 2 T7a self-sites (`0x1404F61FA` zero-init,
`0x1404F65D2` commit) + 8 code sites + 2 data-region hits (`0x142F4D819`, `0x15AC82D61/83A1E` area — not decoded,
likely address tables, OPEN). Code sites:

- `0x14051A996` (in `FUN_14051A670`, decompiled this session): **NOT a mark-table read** — the function never touches
  `+0xB7EC`; the hit is the `ADD 0xB7D8` / nearby `0xB7xx` immediate of a neighbouring instruction (false hit at the
  byte level; the function's real stores are `param_2+0x8D4 = 0x10/0x11`, a duel-verdict word). Status: FALSE HIT.
- `0x14054B729`: no function contains it (bridge: "No function found") — bytes inside another body or padding; OPEN.
- `0x14057A350` + `0x14057A52D` (in `FUN_14057A1F0` / `FUN_14057A3E0`, full disassembly read this session):
  **bulk CLEAR writers**, not readers: after an 11-slot `FUN_140A40820` release + `FUN_140A84E30` loop they zero
  `T+0xB7D8` (`= 0xFF`), `T+0xB7DC..B820` (8 qwords + word + byte + `B824 = 3`) **including `T+0xB7EC..B7F6` = 0**.
  Gated on `FUN_140579E10` returning non-null and `[T+0xB6E8] != 0` (then cleared). These are the mark table's
  lifetime end: any GF `markTable` must be cleared on this path (exact gate OPEN — `FUN_140579E10` not decoded).
- `0x140969974` (in `FUN_1409696A0`): **TEST reader**: `CMP byte [R9+RAX+0xB7EC],0` (`0x140969970`) — a per-slot
  presence test feeding a duel branch that also calls `FUN_140439FE0` three times (`0x140969A26/B01/B25/B35`).
  GF consumer: duel/press decision reading `markTable[slot]`. Exact branch OPEN (needs deeper decode).
- `0x140993253` (in `FUN_1409931C0`): **TEST reader**: `CMP byte [RAX+RCX+0xB7EC],0` (`0x140993250`) → sets a
  frame flag (`[RSP+0xF0] = 1` at `0x140993265`) consumed at `0x14099369C CMP [RSP+0xF0],0`. GF consumer: same family.
- `0x1409ADB44` (in `FUN_1409AD830`): **TEST reader**: `CMP byte [R14+RBX+0xB7EC],0` (`0x1409ADB40`) in a duel
  function that also calls `FUN_140439FE0` (`0x1409ADC17`), `FUN_140439EF0` twice, `FUN_14043A0A0` twice.
  GF consumer: same family.

GF mapping: no GF consumer exists yet (grep empty). When ported, the readers are GF's duel/press branch code
(`pesoffball.cpp` trap/press predicates are the current live-bearing ponytail sites; e.g. `pesoffball.cpp:1622`
names `FUN_140439FE0` explicitly). T7a output feeds press/duel presence tests — **a wired T7a stays inert until a
duel reader is ported**, as TeamT2 already notes.

## 5. Port spec for MarkTablePort (CONFIRMED pieces only)

- **P1 — pair table (CONFIRMED, port now):** `pair[22][22]` f32 (row stride `0x58`/22 f32; direct index, NOT sorted).
  Writer: per S1 call, for each on-pitch owner `a`, zero the row then `pair[a][b] = bearingDeg(a→b)` for all `b ≠ a`
  (0.0 when coincident), using GF's `PesBearingDeg` (label the `std::atan2` approximation per lane-core O3), then
  apply the wrap second pass? — **NO: do NOT port the wrap pass.** Rationale: the wrap overwrites the bearing with
  `wrap180-ish(P)+?` into the same bytes while NL carries the only other output; S1Watch already verdicts the
  overlapping A_tab dead/transient (INF high). The wrap-back copy's consumer is unidentified; porting it would
  destroy the bearing the readers need. Mark with `// ponytail:` naming `0x14045BD2D..0x14045BE35` as the site
  that will remove it. Inputs: on-pitch flags + positions (GF has both). No ball/attributes/RNG.
- **P2 — mark table (CONFIRMED, port when designation word exists):** `markTable[11]` bytes + the §2 rule with the
  7.0 correction. Blocked inputs: `T+0xB7D8` (none: needs designation word), `FUN_140A53DA0` cone (OPEN/OBS),
  `FUN_140A4C720` side test (elsewhere-decoded?), `FUN_140A4EAF0` (needs decode). The pair-table input is now CLOSED.
- **P3 — clear path (INF, do not port yet):** `FUN_14057A1F0/3E0` bulk clear incl. `B7EC`; gate `FUN_140579E10` OPEN.
- Readers `0x1409696A0/0x1409931C0/0x1409AD830` (INF branch roles, do not port yet).

## 6. Checker with mutations (in scratch)

`<workdrive>/gf-MarkTableRE-scratch/check_marktable.py` — self-checking, no PES bytes in repo:
- asserts the helper arithmetic (`0x228D4+0x16A8 == 0x23F7C`, `0x228D4+7*200 == 0x22E4C`, row size `22*0x58 = 0x7D0`,
  NL span `22*200 = 0x1130` ending exactly at `0x23F7C`);
- asserts the script-read constants (`7.0` at `BDF0` vs `8.0` at `BDF4` — the correction);
- asserts the port rule on a synthetic pair table (zero-init, `id%11` commit, max-3 cap, `d² > 1369` hard-exit);
- mutations (each run, rebuilt, reverted — all kill the checker): `7.0→8.0`, cap `3→4`, `%11→%10`, `1369→1368`,
  hard-exit→continue. All 13 checks green this session; mutations `MUT-KILLED 5/5`.

## 7. Status summary

- 27A0-unbound-7 (`TM+0x23F7C` writer): **CLOSED** — `FUN_14045B420` via `FUN_1404587F0`, §1.
- T7a rule: **CONFIRMED** with the 7.0 correction, §2. Remaining OPEN inputs: designation `T+0xB7D8`,
  `FUN_140A53DA0/53DF0` cone (OBS), `FUN_140A4EAF0`/`FUN_140A4C720` predicates.
- Mark readers: 3 TEST readers CONFIRMED as presence tests (branch payloads OPEN); 2 bulk-clear writers CONFIRMED;
  1 false hit (`0x14051A996`); 1 uncontained (`0x14054B729`); 2 data-region hits OPEN.
- GF state: no `markTable`/`B7EC` anywhere in `src/`+`tests/`; pair reads currently live-bearing ponytails
  (`pesoffball.cpp:1622`, `elizacontroller.cpp:3316`) — the §1 writer retires the table half of those ponytails.

## 8. Open questions / blockers

1. `FUN_140A92D40` atan polynomial (lane-core O3) — bit-exact bearings need it.
2. Wrap-back second pass consumer: does any reader use the wrapped value? (No reader found; recommend NOT porting.)
3. `FUN_140579E10` gate + `0x14054B729` site + 2 data-region `B7EC` hits — undecoded.
4. Duel-reader branch payloads (`0x1409696A0` etc.) — which GF predicate each feeds.
