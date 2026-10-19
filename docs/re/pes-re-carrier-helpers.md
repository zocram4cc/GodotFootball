# PES carrier helpers behind BackSpace (kind 3) and Safety (kind 6): decode + port spec

Decode-only. No engine edits, no live writes, no commits. Read-only: Ghidra bridge 8080
(decompile/disassemble), `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (base 0x140000000)
for constants and raw checks, saved decompiles in `gf-A4Lanes-scratch` / `gf-A4Carrier-scratch`,
copied to `<workdrive>/gf-CarrierHelpers-scratch/`. Port target: `pescarrier.{hpp,cpp}`,
`peslane.{hpp,cpp}` on `origin/A4Lanes` `d5e8417` (GoalFront port).

Legend: **CONFIRMED** = decompile + asm agree (compare senses from raw jumps). **INF** =
inference, named as such. All VAs image VAs. ctx = the unit context (`param_1`/`param_3`):
`ctx[0]` receiver record (`[0]` slot, `[2]` side, `[5]` attack sign k as float ±1/0,
`+0x40` a flag int), `ctx[1]` team ctx (T18 tables at `+0x18`), `ctx[4]` ball ctx (`+0x38`
carrier facing deg, `+0x40..48` ball point, `+0x6C` human/owned flag, `+0x6E`, `+0x6F`),
`ctx[5]` imageA (team image: `+0x9CC` shoot zone, `+0xB6C` receiver table, `+0x2124` pressure
table, `+0x898` per-slot table stride 0x1C, `+0xB44/+0xB48/+0xB3C/+0xB40` box/sector scalars,
`+0x31C0` goal-mouth flag, `+0x31C1` danger flag), `ctx[7]` plan (`+0xA0` plan id, `+0xC0`
heading, `+0xC4` mode). Player record `P`: pos `+0x55C/560/564`, vel per frame
`+0x574/578/57C`, facing `+0x554`, action `+0x5D0`. `fps = 54` (`0x141E532D0` float /
`0x141E532E0` int). `F = *(G+0x2AA8)` pitch info (`[0]` half length, `[5]`, `[6]`, `[8]` bound).

## 0. Float audit (19-10-26, Main spot-check follow-up)

Every hex float immediate and every .rdata float VA cited in this report, re-converted
mechanically (`struct.unpack('<f')` on the immediate / on the image bytes at VA-0x140000000).
Two values were wrong (both lane-1 immediates, §1.1 item 11); all .rdata values were right.

Immediates (`hex -> value`): `0x40A00000 -> 5.0`, `0x41F00000 -> 30.0`, `0x40000000 -> 2.0`,
`0xFFFF00FF -> NaN-as-int` (dead lane arg, not a float). Changed in place: §1.1 item 11 +
immediates line (len 10.0→5.0, cone 28.0→30.0), §4.3 `ScanLane(..., 10, 28, 2, ...)` →
`ScanLane(..., 5, 30, 2, ...)`, §6 item 10 (cone 28.0→30.0). Slot order rechecked: the block
at `0x142122CC1/C8/D8` fills `L[4]` (len) = 5.0, `L[5]` (cone) = 30.0, `L[6]` (rad) = 2.0 —
offsets `RBP-0x30/-0x2C/-0x28` against block base `RBP-0x40` = `L[4..6]`, matching
`peslane.hpp` `LaneRequest{len_m, cone_deg, rad_m}` field order and lane-core §3.1. No slot
reorder needed; the report's (len, cone, rad) reading was right, only the two conversions.

.rdata (`VA -> value`, all as cited): `0x14253EE30 -> 100.0`, `0x142565600 -> 0.5`,
`0x142565604 -> 2.0`, `0x1425656C0 -> 4.0`, `0x14256AE1C -> 1.0`, `0x14256AE30 -> 120.0`,
`0x142588210 -> 10.0`, `0x142588218 -> -1.0`, `0x14258C264 -> 3.0`,
`0x142590D98 -> 0.30000001192092896`, `0x142591140 -> 30.0`, `0x14259222C -> 5.0`,
`0x142596E6C -> 80.0`, `0x142598580 -> 180.0`, `0x14259858C -> 360.0`,
`0x14259A760 -> 35.0`, `0x14259A764 -> 60.0`, `0x14259BDD0 -> 0.6944444179534912`,
`0x14259BDDC -> 1.5`, `0x14259BDEC -> 6.0`, `0x14259BDF4 -> 8.0`, `0x14259BE04 -> 12.0`,
`0x14259BE0C -> 15.0`, `0x14259BE1C -> 20.0`, `0x14259BE20 -> 25.0`, `0x14259BE2C -> 40.0`,
`0x14259BE30 -> 45.0`, `0x14259BE4C -> 70.0`, `0x14259BE5C -> 90.0`, `0x14259BE60 -> 135.0`,
`0x14259BE64 -> 140.0`, `0x14259BE70 -> 225.0`, `0x14259CE40 -> 1600.0`,
`0x14259D60C -> 9.0`, `0x14259D628 -> 110.0`, `0x1425A946C -> -20.0`,
`0x1425A9BF0 -> 130.0`, `0x1425C48B8 -> 14.0`, `0x1425C65F4 -> 900.0`,
`0x1425CA310 -> 220.0`.

## 1. BackSpace helpers

### 1.1 `0x1421226C0` — PassThrough validator (CONFIRMED, full 418 lines + 661-line asm)

Signature: `bool (ctx*, targetSlot)` (a third byte arg is passed by the cut-back picker at
`0x14212538F` — `MOVZX R8D,[RBP+0x108]`, the picker's `param_2` — but the body never reads R8;
dead). Entry rejects `target > 21` (`0x142122749/74C`).

Inputs: carrier slot/record, target slot/record, b6c status `0x142118D20` + record B
`0x142118D40` of target, team-table lines `T18+0x40+side*0x14`, pitch `F`, zone scalars
`imageA+0xB44/0xB48/0xB3C/0xB40`, plan mode `ctx7+0xC4`, per-slot table `imageA+0x898`,
ball point `ctx4+0x40`, pressure/entry tables (tail loop), NearestEntry table `T18+0x22E4C`.

Gates in order (any fail → return 0; the tail loop's three exits return 0 only when the
lane entry is not the `0xFF` sentinel — the sentinel is never stored, so in practice they
reject):
1. b6c status of target == 0xB (`0x1421227C4`).
2. Side test: `k*mate.x <= k*line` where `line = T18+0x40+oppSide*0x14`
   (`0x142122804` COMISS; oppSide = `side==0` test on `ctx[0][2]`).
3. `k*recB.xyz <= F[8]` and (danger `+0x31C1` set or `recB+0xF0 <= 1.0`
   `0x14256AE1C`) (`0x142122838/860`).
4. Two `0x140439EF0` bearing calls (target pos → recB target; carrier pos → recB target);
   result kept in XMM13 for gate 8. `0x140A8F890` call on (recB target, corner point) —
   result discarded (dead call, CONFIRMED asm `0x1421228D5` no use of XMM0).
5. `*(imageA+0x898+carrierSlot%11*0x1C) <= *(imageA+0x898+targetSlot%11*0x1C)` (two
   `0x142139630` + `0x141A6EC20` derefs, `0x14212292D` JA exits).
6. Only when plan mode `ctx7+0xC4 == 1`: snapshot `0x14063C560` of target, `0x140698CC0`
   init, zone `0x140698D10` at the target's point predicted `int(54*0.3+0.5) = 16` frames
   (`0x142590D98` = 0.3, `0x142122978/985`). Then: skip the rest if
   `imageA+0xB48 >= 2` AND `k*ball.x >= F[6]-5.0` (`0x14259222C`); else require
   `imageA+0xB44 >= zone+0x178` (`0x142122A48` JG exits), `+0x31C0 == 0`, and when equal:
   `local_118 -= imageA+0xB3C`; if that is > 0 then mode must be neither 1 nor 3 (mode IS 1
   here, so **positive ⇒ reject**); then reject if `6.0 <= local_118` (`0x14259BDEC`) AND
   `imageA+0xB40 <= local_114` with `!=` (`0x142122A93/A1`, JA senses).
   `local_110/118/114` = zone `+0x178/+0x170/+0x174` (offsets `RBP+0x1C8/0x1C0/0x1C4`
   against zone base `RBP+0x50`).
7. `dist(ball, recB target) >= 6.0` (`0x14259BDEC`, `0x142122B1A`); if below, skip gate 8.
8. `0x1404C8E00 < 8.0` (`0x14259BDF4`) AND `|wrap180(headMate-headCarrier)| < 45.0`
   (`0x14259BE30`, `0x142122B96` — note: **reject when the headings AGREE within 45°**,
   the JNC-at-`0x142122C27` shape: pass (continue) iff `45 <= |Δ|`) ⇒ reject.
9. `|ball.x-mate.x| <= 20.0` (`0x14259BE1C`) AND that `+ |ball.z-mate.z| <= 40.0`
   (`0x14259BE2C`) AND `recB+0xF0 <= 1.0` (`0x142122BB6/BDE`).
10. If `20.0 <= dist(ball,recB)` OR `recB[0] < 4`: angle gate `0x140481B00(ctx4+0x38,
    bearingFromGate4) < limit`, limit 90.0 (`0x14259BE5C`), 120.0 (`0x14256AE30`) when
    human AND `dist < 20.0`; `limit <= angle` ⇒ reject (`0x142122C24` JNC).
11. Lane 1 (direct core `0x14063CD90` at `0x142122D1D`, NOT via the packer): origin =
    ball point advanced 1.0 m along `recB+0xE0` (`0x140A8F7C0` with `fVar2 = 1.0`);
    theta = `recB+0xE0`; len 5.0 (`0x40A00000` imm), cone 30.0 (`0x41F00000` imm — **30°,
    not a .rdata constant**), rad 2.0 (`0x40000000` imm); c = carrier, s = opp side of
    carrier, keep 0, h = targetSlot (raw int!), u2 0, dead -1. Count != 0 ⇒ reject
    (`0x142122D2D` JNZ). **INF (med): h = target slot looks like a register mix-up in PES
    (`MOV [RSP+0x30],ESI` at `0x142122D00`), but it is what the asm passes — port as is.**
12. Lane 2 (packer B `0x14063D2F0` at `0x142122F2E`, caller-visible out): origin = target
    predicted `int(54*0.3+0.5) = 16` frames; theta = `wrap360(side+0x250 +
    clamp(wrap180(bearing(mate→recB)-side+0x250)*2.0, -20.0, 20.0))`
    (`0x142565604` = 2.0, `0x14259BE1C` = 20.0 cap via JA, `0x1425A946C` = -20.0 floor via
    MAXSS); len = `max(3.0, min(|carrier.z-mate.z|, 4*clamp01-like(|carrier.x-mate.x|)) +
    2.0)` where the x term is 0 below 3.0 (`0x14258C264`), 1 above 15.0 (`0x14259BE0C`),
    else `(x-3)/12` (`0x14259BE04`), times 4.0 (`0x1425656C0`), plus 6.0 (`0x14259BDEC`,
    MINSS), plus 2.0 (`0x142565604`), floored at 3.0 (MAXSS); cone 140.0 (`0x14259BE64`),
    rad 0.0, keep 0, h = 16 (same int), u2 0, dead `0xFFFF00FF` imm. Count < 1 ⇒ **return 1
    (accept)** (`0x142122F3F` JC). Else loop over the lane entries (count re-read each iter):
    per entry q (skip unless `< 22`): predict q `fps/6 = 9` frames; exit-A (reject): d2(ball,
    qPred) > 100.0 (`0x14253EE30`) AND `|mate.x-q.x| < |carrier.x-mate.x|*0.5`
    (`0x142565600`, first iter; later iters `*0.5` too — reloaded at `0x14212320F`);
    exit-B (reject): `|ball.z-q.z| <= |ball.z-mate.z|+1.0` AND facing-agree
    (`|wrap(facingQ-facingMate)| < 90.0` after ±360 fold) AND heading-agree
    (`|wrap180(headQ-headMate)| < 30.0`, `0x142591140`); exit-C (reject): q's NearestEntry
    rank-0 entry is the target (`T18+0x22E4C+q*200+side*0x58 == target`) AND
    `k*(mate.x+mate.vx*fps/3)+(z-order bit) < k*(q.x+q.vx*fps/3)` where the bit is
    `( |ball.z-mate.z| < |ball.z-q.z| ) ? 1 : 0`. Loop end ⇒ **return 1**.
    Exit polarity (`0x14212321C/223`): `uVar10 == 0xFF → return 1 else return 0` — entries
    hold real slots, so exits reject. CONFIRMED.

Constants (all read from image): 0.5 `0x142565600`, 0.3 `0x142590D98`, 6.0 `0x14259BDEC`,
20.0 `0x14259BE1C`, 40.0 `0x14259BE2C`, 90.0 `0x14259BE5C`, 45.0 `0x14259BE30`,
8.0 `0x14259BDF4`, 0.6944444 `0x14259BDD0`, 120.0 `0x14256AE30`, 1.0 `0x14256AE1C`,
2.0 `0x142565604`, -20.0 `0x1425A946C`, 3.0 `0x14258C264`, 15.0 `0x14259BE0C`,
12.0 `0x14259BE04`, 4.0 `0x1425656C0`, 140.0 `0x14259BE64`, 100.0 `0x14253EE30`,
30.0 `0x142591140`, 5.0 `0x14259222C`, 180/360/±360 `0x142598580/58C/598`, -1.0 `0x142588218`;
immediates: len 5.0 `0x40A00000`, cone 30.0 `0x41F00000`, rad 2.0 `0x40000000`, dead `0xFFFF00FF`.

Callees that read state GF lacks: `0x14063C560` (snapshot filler — ported shape in GF?
  `ComputeShootZone` covers `0x140698D10`, but the snapshot struct + `0x14063C9F0` field
  layout is GF-absent; writer `0x14063C9F0` undecoded internals beyond lane-core doc §7 —
  stand-in candidate: reuse `ComputeShootZone` at the predicted point, name `0x14063C560`
  as the removing site); `0x1404C8E00`/`0x140439F90` (team-table distance/angle — GF has
  NearestList but not the `0x140A714A0` +0x22AD4/+0x228EC table; writer unknown — §5);
  `0x14063CD90`/`0x14063D2F0` (lane core — GF HAS via peslane); `0x140698D10` (GF HAS via
  ComputeShootZone modulo snapshot input).

### 1.2 `0x142125140` — cut-back picker, lines 101-294 (CONFIRMED + asm)

Signature: `slot|0xFF (ctx*, byte ctx1+0x3D)`. Head (lines 1-100, per A4Lanes doc, re-verified):
side from carrier slot; `0x14063E960(team, carrier, 0xC)` (skill bit 0xC = ONE_TOUCH_PASS,
result discarded — dead call); lane packer `0x14063DE20` at `0x142125311` with origin =
carrier pos, theta = `wrap360(side+0x250)`, len = `dist(carrier, plan+0xB4)` (runtime via
`0x140A8F860`), cone 120.0 (`0x14256AE30`), rad 1.0, keep/h/u2 0 — result discarded (dead
call, CONFIRMED: `TEST AL,AL` absent after `0x142125311`).

Loop `k = 0..10`, `q = side*11+k`, skip carrier and `q > 21` (CONFIRMED `0x142125340/349`):
1. Records non-null + `0x1421226C0(ctx, q, param_2)` true (`0x14212538F` — the arg IS
   forwarded, `MOVZX R8D,[RBP+0x108]`; the validator ignores it, §1.1).
2. `d2(recB[q].target, cornerPoint) <= 225.0` (`0x14259BE70`, `0x1421253EA` JBE passes) OR
   `0x1421225E0(ctx, q)` true; corner = `(±F[0], 0, 0)` with sign flip unless k==1.
3. `dMate = dist(carrier, q)`, `dTarg = dist(carrier, recB[q].target)` (both `0x1415A1780`
   sqrt, linear). `need = 0x1404C8E00(T18, k, q)`, `ang = 0x140439F90(T18, k, q)`,
   `needC = 0x1404C8E00(T18, k, carrier)`, `spd = 0x140477130(q)` (km/h).
4. `crowded = 0x140A4F720(team, qPred13, oppSide, 3.0, fps/4 & 0xFF, 0)` — counts opponent
   slots whose 13-frame predicted pos is within 3 m (`0x14258C264`) of q's 13-frame
   prediction (`fps>>2 = 13`, `0x1421254CD/522`). `free13 = (crowded == 0)`.
5. Lane A `0x14063DE20` at `0x14212565A`: origin = q pred 13, theta = `wrap360(side+0x250)`,
   len 20.0, cone 90.0 (`0x14259BE5C`), rad 3.0, h = 13 → `laneA_free`.
6. Lane B `0x14063DE20` at `0x142125753`: origin = q pred 13, theta = `wrap360(ang)`,
   len = need, cone 90.0, rad 3.0, h = 13 → `laneB_free` (this bool feeds gate 8 below).
7. `b = (q.action == 0x25 || q.action == 0x1E)` (`0x142125767/77A` CMOV pair).
8. If !free13: need `dTarg < 15.0` (`0x14259BE0C`, `0x142125784` JA→pass) else need
   `dMate < 15.0` (`0x14212578A` JBE→fail); if free13: only `dTarg < 15.0` lets the speed
   branch run, else skip to gate 9. Speed branch (only if `spd > 10.0`, `0x142588210`):
   `b |= |wrap180(head(q)-side+0x250)| < 70.0` (`0x14259BE4C`, `0x1421257EF` CMOVA).
9. `b |= (spd > 14.0 (`0x1425C48B8`) && |wrap180(head(q)-side+0x250)| < 70.0)`;
   `b |= laneB_free` (`0x142125862` CMOVNZ).
10. Box branch (all required to set b): `imageA+0x31C0 == 0`, `|ball.z| <= |recB.z|`,
    `(spd <= 0 || |wrap180(head-side+0x250)| <= 100.0 (`0x14253EE30`))`,
    `k*recB.x < F[6]`, `F[5] <= |recB.z|` with `!=`, `laneA_free` (`0x142125869/8A3/8BA/
    8D8/8EC/8F7`).
11. Advance branch: if `needC <= need` → candidate check; else need `0x140699710(zone)` OR
    (`0x14063E010(team, q, fps/3)` false) OR (`20.0 <= need` continues? — NO: `0x142125947/
    94F/953`: `COMISS XMM0(20.0),XMM12(need)` JA→0x14212595E means 20 > need (below) goes to
    candidate check; `20 <= need` falls to `TEST R14B` at `0x142125955` which needs b —
    i.e. far + no-box + `0x14063E010` true + b false ⇒ skip). Precisely: reach candidate
    check if `0x14063E010` false, or (`20 > need`), or b. Else skip.
12. Candidate check: b must be true (`0x142125955/958` JZ skips). First candidate taken
    (`0x14212595E/964`). Later: if `IsCentral(zone) && IsMid(zone)` → keep min
    `0x14061E7C0(T18, k, slot)` (strict `<` replaces, `0x1421259DE` JBE keeps);
    else → the `ctx[0]+0x40` flag picks min-`|z|` (`==0`: replace if `|best.z| < |q.z|`...
    exact: flag==0: `COMISS XMM0(q),XMM2(best)` JBE keep = replace iff best < q, i.e. keep
    the max-|z|; flag!=0: replace iff q < best, keep min-|z|; `0x142125A25/A2F`).
    **Correction to A4Lanes-era reading: the tiebreak is on |z| with a flag-chosen
    min/max sense, not "closer wins".**

Outputs: best slot or 0xFF. Constants: 120.0 `0x14256AE30`, 1.0 `0x14256AE1C`,
225.0 `0x14259BE70`, 15.0 `0x14259BE0C`, 10.0 `0x142588210`, 70.0 `0x14259BE4C`,
3.0 `0x14258C264`, 20.0 `0x14259BE1C`, 90.0 `0x14259BE5C`, 14.0 `0x1425C48B8`,
100.0 `0x14253EE30`, 360.0 `0x14259858C`; frames fps/4 = 13, fps/3 = 18.

Callees needing GF-absent state: `0x1404C8E00` + `0x140439F90` (team-table need/angle —
  same table as §1.1, writer unknown §5); `0x140A4F720` (crowd count — pure from records,
  PORTABLE); `0x14063E010` (zone-class + `0x140698C30` + `0x140699710` wrapper — needs the
  `0x140698340/830` builders; stand-in candidate, writer chain `0x140698340` undecoded);
  `0x14061E7C0` (same team table, linear read — same writer question); `0x14063E960`
  (skill bit — GF HAS skills, portable).

### 1.3 `0x140A6CF70` — ball point/height at a frame index (CONFIRMED)

`ptr = (idx < 250) ? base + (idx*5+0x11)*8 : base + 0x2770`. Each record 40 bytes
(5×8); record 0 starts at `base+0x88`. BackSpace reads `+4` of record 0 = ball height y
(after `0x140A57800` which calls it with idx 0). Callers of the *writer* side: the ball
track is filled by the ball sim (`0x140A57450` reads it per press-layer doc; the track
base is `G+0x40`, read in the BackSpace gate prologue). GF HAS the ball position/height
(ball track) — the port reads height directly; no helper port needed. Stand-in: none.

### 1.4 `0x1421225E0` asm (CONFIRMED — the `*0.0` is REAL, not an artefact)

Raw (`asm_1421225E0.txt`): `MULSS XMM0,XMM7` with `XMM7 = 0` (`0x142122649`), so frames =
`int(0.0 + 0.5) = 0` always. The function is: reject if `target > 21`; else
`0 <= k*(mate.x + 0*vx) - k*curve(imageA+0x2B0, mate.z + 0*vz)`, i.e.
`k*mate.x >= k*curve(mate.z)` with the CURRENT (unpredicted) position. `0x142139650` is
the curve-table interp (lane-core doc §6.3, table contents runtime/OPEN). The A4Lanes
"needs an asm check" is closed: port with frames = 0, no prediction. GF needs: mate x/z,
attack sign, the curve table (runtime → stand-in candidate, writer undecoded).

## 2. Safety helpers

### 2.1 Gate `0x14212B520` lines 64-137 (CONFIRMED + raw asm; bridge has no function here)

Lines 64-113 (pressure → k → lane):
- `k = 0`; if `press+0x30 > 0` → `k = 2` (`0x14212B5F9` JBE shape: `cmp [0x30],0; jbe
  zero-path` — the decompile's `== 0 / else 2` matches); else if `press+0x34 > 1` → `k = 1`
  (`0x14212B620/623` CMOVA). CONFIRMED raw.
- If `press+0x38 != 0` (`0x14212B648` JBE skips): `d = facing(ctx4+0x38) - planHead(ctx7+0xC0)`,
  folded once past ±180 (`0x14212B675/67C/682`), then `k++` if `|d| > 135.0` (`0x14259BE60`,
  `0x14212B6A8/6AF`).
- Lane `0x14063DE20` at `0x14212B7C0` (packer args CONFIRMED raw): origin = carrier pos,
  theta = `wrap360(wrap360(angleTable[carrier]+180))` (DOUBLE wrap — two successive wrap
  blocks `0x14212B6D4/6E0` and `0x14212B6ED/6F9`, matching decompile lines 76-91; net =
  single wrap), len = `sqrt(nearestOppD2)` via `0x140A714A0` table + `0x1415A1780`
  (`0x14212B71D-76F`), cone 80.0 (`0x142596E6C`), rad 3.0 (`0x14258C264`), keep/h/u2 0.
  Lane FREE ⇒ return, no answer (`0x14212B7D6/7D8` JNE exits). **Correction to A4Cont
  "free lane 80 m / 3.0": the 80 is the cone (deg), the length is runtime sqrt(d2).**
- Danger (`+0x31C1 != 0`) skips all of the above with `k = 3` (`0x14212B5BE/5C0`).

Lines 114-137 (answers, CONFIRMED raw `0x14212B7DE-88F`):
- `+0x31C0 != 0` ⇒ return. Else a dead `0x14069A3E0` call (result unused), then
  `short = 0x142126B70(ctx, 1, 1, ctx1+0x3D)` — note arg3 = 1 imm (`0x14212B805`), arg2 = 1
  (`0x14212B808` from r8b=1; the decompile's `param_2 = 1` is CONFIRMED).
- If `k == 0` → short branch (`0x14212B816/819` JE). Else `thru = 0x142128490(ctx,
  ctx1+0x3D)`; if `thru > 21` → short branch; else answer type 2, target thru, b = 3,
  +0x12 = 1 (f12).
- Short branch: if `short < 22`: answer type 2, target short, **b = esi (zeroed — 0)**,
  and +0x10 = 1 (f10) iff `0x142128C10(ctx)` AND `0x142124950(ctx, short)` both true.
  The decompile's `param_2[3] = 0` is really `mov [r14+0xC],esi` with esi = 0 (loop-var
  residue) — value 0 CONFIRMED.

### 2.2 `0x142126B70` — Safety short picker (CONFIRMED structure + scoring + exits; 492 lines + 917-line asm)

Signature: `slot|0xFF (ctx*, scoredFlag, unused, byte ctx1+0x3D)`. The Safety gate passes
scoredFlag = 1. Third arg (r8 = 1) is never read in the body — dead.

Head: `keeper = 0x140A62690(team, side)`; carrier predicted `int(54*0.5+0.5) = 27` frames;
`nearOpp = 0x140A4EE40(team, oppSide, carrierPred, 0)`; if none (≥ 22) ⇒ return 0xFF
immediately (the whole loop is inside `if (uVar13 < 0x16)`). `dCarrierOpp2 =
d2(carrierPred, nearOppPred27)`. `b28 = (press+0x10 == 0) || (press+0x10 == 1 &&
press+0x34 > 1)` — "carrier free-ish" (`0x142126DC0/DE7/EB/06`).

Per mate k = 0..10, q = side*11+k, skip carrier / q > 21 (`0x142126E97/A0`):
- Record A `0x142118C90`, status `0x142118D20 == 0xB` else skip (`0x142126F4E`).
  `turnA = recA+0xE4` as float; `over110 = turnA > 110.0` (`0x14259D628`); `under80 =
  turnA < 80.0` (`0x142596E6C`, `!bVar4` in decompile = `turnA >= 80`... careful: bVar4 =
  `fVar34 < 80`, so `!bVar4` means turnA >= 80).
- Keeper mates (`side+0x4C00[k] == 0`) need danger OR `press+0x30 != 0`, then a ball-speed
  gate: human, or `recA+0xEC >= 15.0` (`0x14259BE0C`), or (`0x140A6D0D0` false AND
  `ballSpeed(0x140A57650)*factor <= 60.0` where factor = `54*60*60/1000 = 194.4`;
  `0x142126FBA/FE4/DA/DE`), then human or (`recA+0xEC <= 40.0` (`0x14259BE2C`) or
  carrier-side test) AND (`+0x6E == 0` or `ctx4+0x44 < 1.5` (`0x14259BDDC`)) AND
  `press+0x10 == 0`. Non-keeper mates need human or `0x140587E70(T18,k,q) >= 225.0`
  (`0x14259BE70`, d2 to nearest opp — `0x14212708A/A4/A7`).
- **Scoring (only if scoredFlag != 0 AND (press+0x30 < 2 OR recA[0] < 5 OR
  `recA+0xEC >= 10.0`))** (`0x1421270E4/0EA/0EF/0FD`):
  `wSpd = carrierSpd < 5 ? 1 : carrierSpd < 25 ? (spd-25)/(-20) : 0`
  (5.0 `0x14259222C`, 25.0 `0x14259BE20`, -20.0 `0x1425A946C`; `0x14212710E/12A` JC senses);
  `wDist = dist < 10 ? 1 : dist < 30 ? (dist-30)/(-20) : 0` (10.0 `0x142588210`,
  30.0 `0x142591140`); `need = wSpd*30 + 60 + wDist*30` (30.0 `0x142591140`,
  60.0 `0x14259A764`); `+20` if class == 4 (20.0 `0x14259BE1C`), `+30` if class == 5
  (30.0); `= 120.0` (`0x14256AE30`) in danger. Keeper mates skip the score (score stays
  135/45 defaults). Limit (`0x1421271B7-1D2`, CONFIRMED): keeper → `lim = max(need, 45.0)`;
  non-keeper with `need > 135.0` → `lim = 45.0`; else → `lim = max(need, 45.0)`. Reject iff
  `recA+0xE8 > lim` (strict JA, `0x1421271DA/1DD`). If over110: `aDiff =
  |wrap180(attack-vs-facing)|` via `0x14043A0A0`, `fDiff = 0x140481B00(ctx4+0x38, facing)`;
  reject iff `aDiff < 90.0` AND `fDiff > 45.0` (`0x142127237` JBE passes iff `90 <= aDiff`;
  `0x142127243` JA rejects).
- Distance window: floor = keeper ? 10.0 : 5.0; reject iff `floor > recA+0xEC`
  (`0x142127256` JA — pass iff `dist >= floor`). Ceiling cascade (`0x142127263-2C8`,
  CONFIRMED): start 40.0 (`0x14259BE2C`); if over110: if `recA+0xE8 > 100.0` (`0x14253EE30`)
  AND class <= 3 → 15.0; elif `k*ball.x > 0.0` → 15.0; elif keeper AND class >= 5 → 60.0
  (`0x14259A764`); else stays 40.0. Then team mode `0x141052E30(ctx[6]) >= 3` ⇒ ceiling =
  15.0 (`0x1421272C3/2C6` JL keeps). Reject iff `recA+0xEC > ceiling` or `recA+0xF0 > 1.0`
  (strict JA, `0x1421272D4/2E5`).
- Mate-speed branch: `mSpd = 0x140477130(q)`; reject if `recA+0xEC < 5.0` AND `mSpd > 20.0`
  (`0x14259BE1C`, `0x14212732D/337/33B` — pass iff `5 <= dist` or `mSpd <= 20`).
- Frames `fr = int((recA+0xEC / (80.0*1000/(54*3600))) )` — `0x1404C8DC0(80.0)` =
  `80*1000/(54*3600) = 0.4115` dist-per-frame; `fr = dist/0.4115` (≈ dist*2.43).
  Predict mate AND his nearest opp (`0x140A4EE40` of mate pred) by fr.
- Opp-near-mate gate (only if opp found): if keeper OR `0x14057D930(T18,k,q) < 35.0`
  (`0x14259A760`, sqrt of d2 — 35 m, `0x14212745C/464/467`) OR (`turnA >= 80` AND
  `0x14063E140(team,q,0)` false). `lim`: keeper OR `sqrt(d2) < 35.0` arm →
  `lim = 0.4*dist > 20.0 ? 20.0 : max(0.4*dist, 10.0)` (i.e. `clamp(0.4d, 10, 20)`);
  `0x14063E140`-false arm → `lim = 0.4*dist > 10.0 ? 10.0 : max(0.4*dist, 3.0)` (i.e.
  `clamp(0.4d, 3, 10)`). Reject iff `d2(matePred, oppPred) <= lim²` (`0x1421274B2/4B5`
  JA-shape + `0x142127525/528` JNC — pass iff `lim² < d2`).
- Second opp test: `d2(matePred, oppPred_by_same_fr) < (sqrt(dCarrierOpp2)+pad)²` ⇒ reject
  (`0x1421275EC/5EF`); pad = 0, or 2.0 (`0x142565604`) if over110, when `k*mate.x > 0`;
  else 3.0, or 5.0 if keeper.
- Pressure gate (only if `turnA >= 80`): `pc = press[q%11]+0x10`, `cc = press[carrier]+0x10`;
  reject if `pc <= cc`, or (`pc < 2` AND `15.0 < dist`), or (!b28 AND over110 AND `pc < 2`)
  (`0x14212762D/630/636/643/64D/65A`).
- Class floor: `need = b28 ? (turnA < 80 ? 2 : 3) : 3`; keeper → 4. Reject if
  `recA[0] < need` (`0x142127688`).
- Lane (`turnA >= 80` only): origin = ball advanced `dist` m along `recA+0xE0`, theta =
  bearing(origin→ball) (`0x140474C40`), len = `clamp(dist/5, 4, 12)` (4.0 `0x1425656C0`,
  12.0 `0x14259BE04`, `0x1421276DA/6E4/6EC`), cone 220.0 (`0x1425CA310`!), rad = 0.0 imm,
  keep 0, h = fps/4 = 13, u2 = ctx1+0x3D, via wrapper D `0x14063DAB0` at `0x14212776F`
  (bool only, opp side forced). Blocked ⇒ reject (`0x142127774/776`).
- Keeper lane (keeper only): origin = ball, theta = bearing(ball→keeperPred13), len =
  dist(ball, keeperPred13) (runtime), cone 90.0, rad 1.0, keep 1 (!), via `0x140A4FEA0` at
  `0x142127852` (count != 0 ⇒ reject, `0x142127857/859`). `0x140A4FEA0` =
  `0x140A4FBC0(team, oppSide, ballPt, facing, theta, len, 1.0, keep=1, h=?)` — decoded §3.
- Tiebreak (a surviving q vs best): if `0x140A4C8E0(team, carrier)` (carrier on own half
  side — decoded §3: true iff `side-sign * carrier.x >= 0`... exact: attack+1: x<=0;
  attack-1: x>=0) AND `|wrapDeg(team+0x250, facing)| > 130.0` (`0x1425A9BF0`,
  `0x14043A020` abs-diff) AND `oppTable+0x2470C[opp].+4 <= 9.0` (`0x14259D60C`,
  `0x142127926/92A` JC exits): if `best.turnA <= q.turnA` keep best unless best is the
  keeper (then take q). Else if q not keeper AND best is not keeper: if
  `d2(ball, ownGoalCorner) >= 1600.0` (`0x14259CE40`) take the more advanced (larger k*x);
  else if `q.class <= best.class` (with ==) AND `dOpp(best) <= dOpp(q)` (NearestList d2
  via `T18+0x22E4C`, sqrt both) AND `|dOppB-dOppQ| < 5.0` AND q more advanced → take q.
  First candidate always taken. **Tiebreak senses CONFIRMED (`0x14212792A/93D/958/97D/
  A42/A4C/A5B/A7B/AB0`), field layout INF-free; the `+0x2470C` table's meaning is INF.**

Undecoded sub-calls: NONE structural — every callee is decoded or a named reader. Runtime
tables: `0x140A714A0` (+0x22AD4/+0x228EC, two-sided NearestEntry), `+0x2470C` pressure-ish
pair (INF meaning), b6c/pressure writers (named, §5).

### 2.3 `0x142128490` — Safety through picker (CONFIRMED, 189 lines + 318-line asm)

Signature: `slot|0xFF (ctx*, byte ctx1+0x3D)`. Pre-gates (any fail → 0xFF):
1. If `int(54*3.0+0.5) = 162 < press+0x20`: then need `pressTable[oppSide]+4 > 9.0`
   (`0x14259D60C`, `0x1421285B8/5BC` JA→proceed) else 0xFF (`0x1421285C2` falls into gate 2
   — careful: gate-1-fail goes to gate 2, not 0xFF! `0x1421285BC JA 0x14212864F` skips gate
   2; else gate 2 runs. So: (162 < +0x20 AND +4 > 9) OR gate-2-pass ⇒ proceed).
2. `carrier > 21` → 0xFF; carrier's opp-table d2 (`0x140A714C0` + rank) `> 1600.0`
   (`0x14259CE40`, `0x142128605/60A`) else 0xFF — **carrier's nearest opp must be 40+ m
   away**; then `k*carrier.x < 0` AND `k*carrier.x < k*lineB` ⇒ 0xFF
   (`0x142128628/62C/646/649`, lineB = `T18+0x40+(side+3)*0x14`... exact idx
   `(uVar25+3)*0x14`).
3. Loop over `0x140507740(T18, carrier)` = carrier's NearestEntry row, j = 0..10
   (`q = row[j].id`): `q > 21` → **return 0xFF whole-picker** (`0x1421286D3`); `row[j].d2 >
   900.0` (`0x1425C65F4`) → **return 0xFF** (`0x1421286DE/6E1`) — the row is distance
   sorted, so this ends the scan.
4. Per q: keeper (`side+0x4C00 == 0`) → next (`0x14212871F`). If `spd(q) >= 5.0`
   (`0x14259222C`): need `|wrap180(head(q)-side+0x250)| <= 70.0` (`0x14259BE4C`) else next.
5. b6c status `0x142118D20 == 0xB` else next. Record B.
6. Aim = recB target advanced `recB+0xEC*0.5` m along `wrap360(side+0x250)` (`0x140A8F7C0`,
   `0x1421287F4/82D`); `|wrap180(bearing(ball→aim) - side+0x250)| < 135.0` (`0x14259BE60`,
   `0x14212886D/874` JNC→next means pass iff `< 135`); `recB[0] >= 3` (`0x14212887A/87D`);
   `recB+0xF0 <= 1.0`; `recB+0xE8 <= 60.0` (`0x14259A764`).
7. `k*aim.x >= k*ball.x` (`0x1421288C4/8C7`); `100.0 <= d2(ball, aim) <= 900.0`
   (`0x14253EE30`, `0x1425C65F4`, `0x1421288D7/8E0/8E4`); `0x1421226C0(ctx, q, 0)` true.
8. Final lane `0x14063DE20` at `0x14212899A`: origin = recB target (unadvanced), theta =
   `wrap360(side+0x250)` (`0x1405D06C0`), len 10.0 (`0x142588210`), cone 60.0
   (`0x14259A764`), rad 5.0 (`0x14259222C`), keep 0, h = `int(54+0.5) = 54`, u2 = ctx1+0x3D.
   Free ⇒ **return q immediately** (`0x14212899F/A1`); else next j. End ⇒ 0xFF.

First-q-wins; exits are all next/0xFF/return-q. No scoring beyond the gates.

## 3. Small-callee contracts (all CONFIRMED via bridge decompile + call-site asm)

| VA | contract | GF |
|---|---|---|
| `0x140A8F860` | `dist(a, b)` linear xz (sqrt via `0x1415A1780`) | HAS (own dist) |
| `0x140A8F890` | `d2(a, b)` squared xz | HAS |
| `0x1415A1780` | sqrt (CRT-imported thunk; INF name, CONFIRMED behaviour: all call sites use it as sqrt of d2) | HAS |
| `0x140477130` | speed km/h = `fps*60*60/1000 * \|vel_eff\|`, `vel_eff = 0x140A6C6D0(P, 0.6944444/fps)` | portable (needs `0x140A6C6D0` semantics: vel or facing-fallback — lane-core doc §6.3) |
| `0x140458740` | heading deg of `vel_eff(P, r)`; 0 if zero vector | same |
| `0x140A6C6D0` | vel_eff (not re-read; lane-core §6.3 C-dec) | port spec exists |
| `0x140A489F0` | per-player AI record ptr `G+(slot+0xF3)*0x28` | GF-absent struct; only `+0x70` (via `0x14124DAD0`) + bit reads flow into `0x140698D10` — stand-in candidate |
| `0x14124DAD0` | `p+0x70` | trivial |
| `0x14063E010` | zone-build + `0x140699710`-on-temp wrapper (like `0x14063E140`/`0x14063E270` but returns `0x140699710` = central-test of the built zone) | needs `0x140698340/830` builders — stand-in candidate |
| `0x14063E140` | same shape, returns `0x1406997A0` (mid-test) | same |
| `0x14063E270` | same shape, returns `0x140699820` (close-test) | same |
| `0x14061E7C0` | `0x140A714A0(T18,k)[slot].d2` linear read (nearest-opp d2 of slot) | needs team table — §5 |
| `0x140439F90` | `wrap360(angleTable(T18,k)[slot] + 180)` | needs angle table — §5 |
| `0x1404C8E00` | `sqrt(0x140A714A0(T18,k)[slot].d2)` | same |
| `0x140587E70` | `0x140A714C0(T18,k)[slot].d2` (same layout, flipped table) | same |
| `0x14057D930` | `sqrt(0x140A714C0(T18,k)[slot].d2)` | same |
| `0x140A714A0` | `T18 + (k==1 ? 0x22AD4 : 0x228EC)` (NearestEntry array) | §5 |
| `0x140A714C0` | `T18 + (k==1 ? 0x228EC : 0x22AD4)` (same, flipped) | §5 |
| `0x140A71260` | `T18 + (k==1 ? 0x22C64 : 0x22A7C)` (per-slot angle table, float[22]) | §5, writer unknown |
| `0x140507740` | `T18 + 0x22E4C + slot*200` (NearestEntry row of slot — GF's NearestList) | GF HAS |
| `0x14043A0A0` | `wrap180(b - a)` | HAS (WrapDeg) |
| `0x14043A020` | `\|wrap180(*p - a)\|` abs variant | HAS |
| `0x140481B00` | `\|wrap180(θ - *p)\|` | HAS |
| `0x140474C40` | `out = bearing(a, b)` via `0x14046F260` | HAS |
| `0x140470D60` | `out = wrap360(wrap360(*p) + add)` | HAS |
| `0x1405D06C0` | `*p = wrap360(v)` in place | HAS |
| `0x1404C8DC0` | `x*1000/(fps*3600)` — deg-ish per-frame scale; used as dist→frames divisor | trivial |
| `0x1406997A0` | IsMid(zone) | HAS |
| `0x140699710` | IsCentral(zone) | HAS |
| `0x140699820` | IsClose(zone) | HAS |
| `0x140A4F720` | crowd count: opponents (side s3, mask param_6 bits skip) with 13-frame-pred pos within `param_4` of point | portable (pure) |
| `0x140A4FBC0` | behind-`0x140A4FEA0`: count scanned-side players with h-pred pos in annulus `(rIn², rOut²)` of point AND `\|wrap180(bearing(point→pred) - facing)\| < cone*0.5` | portable (pure); keeper-exclusion arm reads `side+0x4BD4` (INF: active-phone? — the `param_8==0` arm reads `+0x4BD4/0xC48`-ish; Safety passes 1, arm dead) |
| `0x140A4C8E0` | `(slot<22 && record) ? 0x140A4C930(team, slot, x) : 0` | portable |
| `0x140A4C930` | `attack==1 ? x <= 0 : (attack==-1 ? x >= 0 : 0)` — carrier behind-half test on raw x | portable (needs attack sign, GF HAS) |
| `0x141052E30` | `*(M+8)` team mode int | GF-absent scalar — stand-in candidate (GF has no team mode; writer undecoded) |
| `0x140A57650` | ball speed: idx 0 → `\|track+0x2948\|/fps`; else `dist(track[i], track[i-1])` linear xz | GF HAS ball vel (portable, no helper needed) |
| `0x140A6D0D0` | `*(u8*)(G+0x40 + 0x4C)` flag | GF-absent byte — stand-in candidate (writer undecoded) |
| `0x1421B7D40` | `*p` (lane count read) | trivial |
| `0x141587AA0` | `*p = 0` | trivial |
| `0x141E532D0/E0` | fps float/int (54) | HAS (`kPesTickHz`) |
| `0x14069A3E0` | pressure entry: `0xFF → carrier idx`, else `slot%11`, stride 0x5C, base +0x33 | GF-absent table; WRITER `0x14069A500` (called once from `0x1421196FA` in image builder `0x142119390`) — §5 |
| `0x14063E960` | skill bit: roster → attr → bit `param_3` (0xC = ONE_TOUCH_PASS) | GF HAS skills |
| `0x140A4EE40/60` | nearest side-s slot to point (rank k) | GF HAS via NearestOpponents |
| `0x142118C90/D20/D40` | b6c record A / status / record B | GF-absent; WRITERS `0x142117C30` (enum, from `0x1421196D0`) → `0x1421174F0` (records) — §5 |
| `0x142139630` | `slot>10 ? base : base+slot*0x1C` | trivial |
| `0x14211CB00` | plan+0xC+plan*0x10 record (case 10 = plan id) | GF-absent plan record — stand-in candidate |
| `0x142139650` | curve interp (lane-core §6.3) | runtime table — stand-in candidate |
| `0x14063DAB0` | wrapper D = LaneFree-shape with explicit (len, cone) and u2 passthrough | GF HAS via peslane (bool only) |
| `0x14063C560` | snapshot zero + `0x14063C9F0` fill | §1.1 |
| `0x140698CC0` | zone-array init (13×`0x140698320` + `0x140699610`) | GF HAS shape via ComputeShootZone |
| `0x140698D10` | zone classifier (re-read: gates + 13-subscriber tail; tail `0x140698B80/8BE0/950` evaluated but only `+0x170/174/178/17C` read by our callers) | GF HAS via ComputeShootZone |

## 4. Port specs

### 4.1 BackSpace answer as a function of named inputs

```
BackSpace(ctx) -> (type 2, target, b) | no-answer
  c = carrierSlot; k = attackSign; zone = imageA.zone (+0x9CC)
  # lane branch (only if IsMid(zone)):
  if IsMid(zone) and LaneFree(players, c, {carrierPos, wrap360(sideAngle+0x250), 20, 80, 1}, 0, 0):
      q = nearestOppSide(oppSide(c), ballPoint)   # GF: NearestOpponents
      if q exists and d2(q.pos+q.vel*9, ballPoint) > 36: return NO-ANSWER
  # block 2:
  if ctx4+0x6F != 0: return NO-ANSWER
  if ctx4+0x6C == 0 and ballHeight > 1.0: return NO-ANSWER
  if plan == 4:
      rec = planRecord(10)                        # STAND-IN (§5)
      t = rec.target
      if t > 21: return NO-ANSWER
      if planRecBytesFail: return NO-ANSWER       # pcVar7[1]==0 && *pcVar7!=0 (STAND-IN bytes)
      if !Validator(ctx, t): return NO-ANSWER     # §1.1
      if d2(recB[t].target, corner) > 225 and !CurveGate(ctx, t): return NO-ANSWER  # §1.4
      return (2, t, 3)
  else:
      t = CutBack(ctx)                            # §1.2
      if t == 0xFF: return NO-ANSWER
      b = 3
      if CloseTest(teamCtx, t) and press[t].cls > 1 and recA[t].class > 1: b = 0
      return (2, t, b)
```

GF HAS: players/records, NearestOpponents, ball point/height, zone + IsMid, skills,
`ComputeShootZone`, peslane (all lane shapes incl. direct-core + packer-B with out —
`ScanLane` covers both; wrapper-D bool = `LaneFree`), sqrt/dist/wrap/bearing, fps,
attack sign/side angle (side+0x250 — GF carries team attack? elizacontroller HAS attackSign;
the +0x250 float per se is INF §5 — likely same).
STAND-INS (§5 for writers): plan record slot 10 + its 2 bytes; b6c status/records A+B
(status filter exists as enumeration stand-in per GoalFront; record FIELDS are new:
recB target/angle/dist/difficulty/class, recA class/dist); pressure entries (+0x10 class;
the validator tail also reads +0x30/+0x34 only via Safety paths — BackSpace needs +0x10);
team tables T18 (`0x140A714A0` need + `0x140439F90` angle + `0x14061E7C0` line value);
`0x14063E010` (close-test-of-built-zone); curve table `0x142139650`; `ctx4+0x6F/+0x6C`
flags (GoalFront already stand-ins `humanCarrier`); `ctx1+0x3D` byte (forwarded but dead
in validator; live as u2 in Safety lane — carry it, name `0x14063DAB0` as removing site);
`imageA+0x898` per-slot floats; box scalars `+0xB44/48/3C/40`; `+0x31C0`.
Plan-4 reachability: plan 4 IS admitted in GF (`BuildUnitList` pushes 3,2 for PassThrough)
but GF's plan id is its own enum — the `plan == 4` test maps to `kPlanPassThrough`;
the slot-10 record has no GF counterpart at all.

### 4.2 Safety answer as a function of named inputs

```
Safety(ctx) -> (type 2, target, b, f10?, f12?) | no-answer
  press = pressure[carrier]
  if danger: k = 3
  else:
      k = 0; if press.c1 > 0: k = 2 elif press.c2 > 1: k = 1   # +0x30/+0x34
      if press.c3 != 0 and |fold(facing - planHead)| > 135: k++  # +0x38
      if LaneFree(players, c, {carrierPos, wrap360(angleTable[c]+180), sqrt(nearOppD2), 80, 3}, 0, 0):
          return NO-ANSWER
  if goalMouthReachable: return NO-ANSWER                      # +0x31C0
  short = ShortPicker(ctx, scored=1)                            # §2.2
  if k != 0:
      thru = ThroughPicker(ctx)                                 # §2.3
      if thru <= 21: return (2, thru, 3, f12=1)
  if short > 21: return NO-ANSWER
  ans = (2, short, 0)
  if OneTouch(ctx) and ReceiverPred(ctx, short): ans.f10 = 1
  return ans
```

GF HAS: same lane/record/ball/zone base as §4.1, plus `OneTouch` (one-touch predicate —
  ported? NOT on A4Lanes; it is specified in lane-core §6 — implementer ports or stands in,
  name `0x142128C10`) and `ReceiverPred` (`0x142124950` — specified in lane-core §7/8 —
  same).
STAND-INS: pressure counts +0x30/+0x34/+0x38 and per-slot +0x10 (writer `0x14069A500` §5);
danger + goal-mouth flags (writers per carrier-ai §4.2: `0x140699960` + `0x142119390` —
  named, unported); angle table `0x140A71260` (writer UNKNOWN — GoalFront's facing stand-in
  is the pattern); b6c status + record A fields (class/dist/turn/difficulty) + record B
  (through picker); team tables (`0x140A714A0/C0` d2 reads, `+0x2470C` pair (INF meaning),
  `0x140587E70`); team mode `0x141052E30`; ball-track flag `0x140A6D0D0+0x4C`;
  `ctx4+0x6E/+0x44` (receiving flag + scalar); plan heading `+0xC0` (GF plans don't carry
  headings — stand-in); facing `ctx4+0x38` (GF HAS facing); `ctx1+0x3D` u2 byte.

### 4.3 Validator pseudocode (port-ready, §1.1 condensed)

```
Validator(ctx, t):
  if t > 21: return false
  if b6cStatus(t) != 0xB: return false
  if k*mate[t].x > k*teamLine(oppSide): return false
  if k*recB[t].target.x > pitchBound8: return false
  if !danger and recB[t].diff > 1.0: return false
  bC = bearing(carrier, recB[t].target)
  if slotStat(carrier) > slotStat(t): return false        # imageA+0x898 floats
  if planMode == 1:
      z = zoneAt(matePred16(t))
      if !(boxN2 >= 2 and k*ball.x >= pitchF6 - 5):
          if boxSector < z.sector: return false           # +0xB44 vs +0x178
          if goalMouth: return false
          if boxSector == z.sector:
              m = z.margin - boxMargin                    # +0x170 - +0xB3C
              if m > 0: return false                      # (mode==1 arm)
              if m >= 6.0 and boxAngle <= z.angle and boxAngle != z.angle: return false
  d = dist(ball, recB[t].target)
  if d >= 6.0 and teamNeed(t) < 8.0 and |wrap180(head(t)-head(c))| < 45.0: return false
  if |ball.x-m.x| > 20 or |ball.x-m.x|+|ball.z-m.z| > 40: return false
  if recB[t].diff > 1.0: return false                     # re-read, same field
  if d >= 20 or recB[t].class < 4:
      lim = (human and d < 20) ? 120.0 : 90.0
      if angleDiff(facing, bC) >= lim: return false
  if ScanLane(oppSide, ballAdv1m(recB[t].ang), recB[t].ang, 5, 30, 2, keep 0, h=t).count != 0:
      return false                                        # h = t, verbatim PES
  th = wrap360(sideAng + clamp(wrap180(bearing(mate,recB)-sideAng)*2, -20, 20))
  wx = |c.x-m.x|; fx = wx<3 ? 0 : wx>15 ? 1 : (wx-3)/12
  ln = max(3.0, min(|c.z-m.z|, 4*fx + 6) + 2)
  out = ScanLane(oppSide, matePred16, th, ln, 140, 0, keep 0, h=16)
  if out.count == 0: return true
  for q in out.entries:
      if q.slot > 21: continue
      qP = q.pos + q.vel*9
      if d2(ball,qP) > 100 and |m.x-q.x| < |c.x-m.x|*0.5: return false
      zM = |ball.z-m.z|; zQ = |ball.z-q.z|
      if zQ <= zM + 1.0 and facingAgree(q,m,90) and |wrap180(head(q)-head(m))| < 30:
          return false
      if nearestRank0(q, side) == t:
          bit = (zM < zQ) ? 1 : 0
          if k*(m.x+m.vx*18)+bit < k*(q.x+q.vx*18): return false
  return true
```

### 4.4 Short-picker scoring pseudocode (§2.2 condensed)

```
need(class, dist, carrierSpd, danger):
  wS = carrierSpd<5 ? 1 : carrierSpd<25 ? (carrierSpd-25)/(-20) : 0
  wD = dist<10 ? 1 : dist<30 ? (dist-30)/(-20) : 0
  n = wS*30 + 60 + wD*30; if class==4: n += 20; if class==5: n += 30
  return danger ? 120.0 : n
per-mate gate (scored): lim = keeperMate ? 45.0 : need(...); if keeperMate or need <= 135:
  lim = max(lim, 45.0); reject if lim < recA.turnVsFacing  # +0xE8
ceiling(dist): 40 default; teamMode > 2 → 15; over110-path: 60 iff keeperMate and class > 4
  and k*ball.x <= 0 and (turnVsAttack <= 100 or class > 3); 15 iff same but not (keeper and
  class>4); else 40. Reject if dist > ceiling or diff > 1.0.
```

## 5. GF-absent state, writers where findable, else stand-in candidates

| State | writer (VA evidence) | GF status |
|---|---|---|
| Pressure table `imageA+0x2124` (+0x10 class, +0x30/34/38 counts, +0x20 int, +0x4 side pair) | `0x14069A500`, called once at `0x1421196FA` from image builder `0x142119390` (bridge decompile L105; 1 E8 caller CONFIRMED by image scan) | stand-in candidate; name `0x14069A500` |
| b6c table `imageA+0xB6C` (status + records A/B) | enum `0x142117C30` (from `0x1421196D0`, 1 caller) → records `0x1421174F0` (2 callers `0x142118325/618`) | stand-in candidate; name `0x1421174F0` |
| Angle table `T18+0x22A7C/0x22C64` (`0x140A71260`) | UNKNOWN — only readers found (80 E8 callers of the reader; the two offset immediates appear ONLY in the two reader functions `0x140A71260/0x140A71440`, so the filler uses a computed address, not the immediate) | stand-in candidate (GoalFront facing pattern); writer OPEN |
| Team dist tables `T18+0x22AD4/0x228EC` (`0x140A714A0/C0`) | UNKNOWN (computed-address filler like the angle table; the one non-reader `0x22AD4` hit `0x14126D964` is a RIP-relative ref to `0x14351AE30/34`, rejected) | stand-in candidate |
| `T18+0x2470C` side pair (+4 float vs 9.0) | UNKNOWN | stand-in candidate, meaning INF |
| Danger `+0x31C1` / goal-mouth `+0x31C0` | `0x140699960` + image builder `0x142119390` (carrier-ai §4.2; not re-verified here) | stand-in candidate |
| Box scalars `+0xB44/0xB48/0xB3C/0xB40` | image builder (not traced to a writer VA here) | stand-in candidate |
| Curve table `imageA+0x2B0` | runtime (lane-core §6.3 O6) | stand-in candidate |
| Plan record `+0x48` slot-10 target + 2 bytes; plan heading `+0xC0`; plan mode `+0xC4` | plan image units `0x14211xxxx` (carrier-ai §4.4) | stand-in candidate |
| `imageA+0x898` per-slot floats | UNKNOWN | stand-in candidate |
| Team mode `*(M+8)` | UNKNOWN | stand-in candidate |
| Ball-track flag `G+0x40+0x4C` | UNKNOWN | stand-in candidate |
| `ctx4+0x6C/0x6E/0x6F/+0x44`, `ctx1+0x3D`, `ctx[0]+0x40` | context structs (not traced) | stand-in candidate (GoalFront pattern exists for +0x6C) |
| Snapshot `0x14063C560` struct layout | filler `0x14063C9F0` (layout per lane-core §7; full field semantics OPEN) | stand-in via ComputeShootZone |
| `0x140698340/830` zone builders (behind `0x14063E010/140/270`) | undecoded | stand-in candidate |

## 6. Corrections to existing docs (doc:line, old, new, evidence)

1. `docs/ports/pes-port-A4Lanes.md:44`: "Then `0x1421226C0(ctx, target, 0)` must be true
   (UNDECODED, 418 lines), `0x1421225E0(ctx, target)` (partly decoded, §2.2), and
   `0x140A8F890` against 225.0" — old: ordering/arity vague. New: the gate ALSO tests two
   plan-record bytes first (`pcVar7[1]==0 && *pcVar7!=0` kills the answer even when the
   validator passes), and the distance test is `d2(recB[target].target, corner) <= 225`
   OR `0x1421225E0` (the OR polarity was right; the distance's first point is the record-B
   TARGET, not the mate). Evidence: `new_142129110.c` L100-118 re-read + §1.1/1.4.
2. `docs/ports/pes-port-A4Lanes.md:52`: "The Ghidra text `fVar5*0.0 + DAT_142565600` looks
   like a decompiler artefact (the frames term reads as 0), so it needs an asm check" —
   new: the `*0.0` is REAL (`MULSS XMM0,XMM7`, XMM7 = 0 at `0x142122649`); frames = 0
   always; `0x1421225E0` compares CURRENT positions. Evidence: `asm_1421225E0.txt`.
3. `docs/ports/pes-port-A4Lanes.md:49`: callee list for `0x1421226C0` names `0x140474C40`
   etc. but omits the direct core call `0x14063CD90` (at `0x142122D1D`) and packer-B
   `0x14063D2F0` (at `0x142122F2E`), and `0x140A48A30`; it names `0x140481B0` (typo for
   `0x140481B00`). New: full callee list §1.1 fines. Evidence: asm CALL scan.
4. `docs/ports/pes-port-A4Lanes.md:50`: cut-back "Loop body (lines 100-291) not read" —
   new: read and specified (§1.2). Also the `0x14063E960` call result is discarded (dead),
   and the head lane call result is discarded (dead). Evidence: asm `0x142125249/311`.
5. `docs/re/pes-carrier-A4Cont.md:16` (BackSpace row): "opponent-on-line `0x140a4ee40`
   within 36 m → no answer" — already corrected by A4Lanes §2.4; CONFIRM the correction
   and add: the gate ALSO requires the plan-record bytes (§6.1 above). Evidence: §1.1.
6. `docs/re/pes-carrier-A4Cont.md:18` (Safety row): "Lines 41–110 and 114–137 NOT read" —
   new: all read (§2.1). "free lane 80 m / 3.0" (A4Cont §4 table row kind 6 in
   pes-re-A4Carrier.md:113 carries it too: "free lane 80 m / 3.0 (`0x14063DE20`)") — new:
   cone 80°, radius 3.0 m, LENGTH = runtime `sqrt(nearestOppD2)`; and lane-FREE returns
   with NO answer (the gate continues only on blocked). Evidence: raw `0x14212B71D-7D8`.
7. `docs/re/pes-re-A4Carrier.md:113` (Safety row): "short `0x142126B70`; through
   `0x142128490`" with inputs "k = press counts + facing vs plan angle > 135°" — new:
   k increments only when `press+0x38 != 0` AND the folded diff exceeds 135°; k starts 2
   (not 1) when `+0x30 > 0`. Evidence: raw `0x14212B5F5-6B1`.
8. `docs/re/pes-re-attack-decisions.md:295-318` (BackSpace OBS): "lead dist vs 36.0 →
   return with NO answer if beyond" — new: squared distance vs 36.0 (6 m), and only when
   the lane branch runs (IsMid) AND the lane is free AND an opp is found. "lane_ok
   (`FUN_1421225e0` or dist<=225.0)" — new: the dist is record-B-target-based (§6.1).
   Evidence: §1.1 + A4Lanes §2.4 (agrees).
9. `docs/re/pes-re-attack-decisions.md:377-390` (cut-back OBS): "lane
   `FUN_14063de20(len = dist, wid 120.0, h 1.0)`" — new: that head-lane result is dead;
   the loop's live lanes are A (20/90/3, h=13, side-angle) and B (need/90/3, h=13,
   team-angle). "tiebreaks prefer the closer mate" — new: `IsCentral&&IsMid` → min
   `0x14061E7C0` line value (strict); else flag-chosen min/max |z|. Evidence: §1.2.
10. `docs/re/pes-re-attack-decisions.md:392-396` (`0x1421226C0` OBS): "Returns void; 0 =
    reject (early return before the accept label), accept falls to LAB_14212322b" — new:
    returns bool; TWO accept sites (lane-2-empty at `0x142122F3F`, loop-end at
    `0x142123229`) and the tail-loop exits reject. Constant list misses lane-1's immediate
    cone 30.0 and the h=target-slot quirk. Evidence: §1.1.
11. `docs/re/pes-re-attack-decisions.md:398` (`0x1421225E0` OBS): "`addss 0.5` only — a
    score nudge, not a gate" — new: it IS a gate (bool return, callers branch on it);
    the +0.5 makes frames = 0. Evidence: §1.4 + call sites `0x1421253F8`, gate L113.
12. `docs/re/pes-re-attack-decisions.md:417-433` (short picker OBS): the score formula is
    CONFIRMED (constants match §2.2 exactly); "need2: dist <= need (else skip)" — new:
    the scored limit is `max(need,45)` vs `recA+0xE8` (a TURN angle), not vs dist; dist
    has its own ceiling cascade (40/15/60 + mode cap). "width dist/5 clamp(4,12)" —
    CONFIRMED (it is the lane LENGTH, via wrapper D, cone 220). Evidence: §2.2.
13. `docs/re/pes-re-attack-decisions.md:434-446` (through picker OBS): "needs the
    team-space value `> 1600.0` ... `*pfVar1 <= 1600 → return 0xff`" — new: that gate
    applies to the CARRIER's opp-table d2 (carrier must be 40+ m from nearest opp... see
    §2.3 gate 2 — it is inside the else-arm: only when gate 1's `(162 < +0x20 AND +4 > 9)`
    fails). "turn-error `< 70.0` → skip" — new: `> 70.0` → next (pass iff `<= 70`).
    "`dist <= 100.0`" — new: `100 <= d2 <= 900` (a BAND, squared). Final lane CONFIRMED
    (10/60/5, h = 54). Evidence: §2.3 + asm senses `0x142128777/8D7/8E0`.
14. `docs/re/pes-re-lane-structs.md:5.3` (interior-entry note): Safety `0x14212B520` "is
    inside FUN_14212AEB0 (OneTwo...)" — new: the CURRENT bridge DB has NO function
    containing `0x14212B520` at all ("No function found at or containing address" for
    both decompile and disassemble; the saved `new_14212b520.c` must come from an older
    DB state). The raw bytes disassemble cleanly as the Safety gate (ret at `0x14212B8A8`,
    falls into SideChange code after). Evidence: bridge probes + raw disasm. (BackSpace
    `0x142129110` likewise has no bridge function — consistent with A4Carrier §4 row 3.)
15. `docs/re/pes-re-carrier-ai.md:§4.3` record-B fields: no correction — the offsets used
    here (+0xE0 angle, +0xE4 attack-angle, +0xE8 facing-angle, +0xEC dist, +0xF0 diff,
    +0xF4 openness, [0] class) match exactly. Recorded as cross-checked, not corrected.

## 7. Open items / INF list

1. Short-picker ceiling cascade + clamp arms (§2.2) — CLOSED this session (raw re-read
   `0x1421271AD-2B9`, `0x142127426-4BE`).
2. `T18+0x2470C` table meaning — INF (pressure-ish pair); needs its writer (budget call).
3. Angle-table writer — OPEN (§5); GoalFront stand-in pattern applies.
4. Team dist-table writer — lead `0x14126D964` examined and REJECTED (that immediate is a
   RIP-relative reference to `0x14351AE30/34`, not a `T18+0x22AD4` write; the filler uses a
   computed address like the angle table). Writer OPEN.
5. `0x140A6C6D0` not re-read (taken C-dec from lane-core §6.3).
6. `0x140698340/830/320/610/950/B80/BE0` zone-builder internals not re-read (only the
   four output fields our callers read are specified).
7. Live validation: none (no PES process running; all CONFIRMED tags are static).

## 8. Reproducing

Saved inputs: `b_1421226c0.c` (418 lines), `d_142125140.c` (294), `b_1421225e0.c`,
`d_142126B70.c` (492), `d_142128490.c` (189), `new_142129110.c`, `new_14212b520.c`,
`dec_*.c` (30+ small callees), `asm_1421225E0.txt`, `asm_1421226C0.txt` (661 lines),
`asm_142125140.txt` (541), `asm_142126B70.txt` (917), `asm_142128490.txt` (318) — all in
`<workdrive>/gf-CarrierHelpers-scratch/`. Constants: image read at VA-0x140000000.
Raw Safety gate: objdump `--adjust-vma=0x140000000 --start-address=0x14212B520
--stop-address=0x14212B9C0`. E8 scans: naive 0xE8 rel32 sweep of `pes_full.bin`.
Bridge budget used: ~50 requests (30 decompiles, 5 disassembles, ~15 probes/raw reads).
