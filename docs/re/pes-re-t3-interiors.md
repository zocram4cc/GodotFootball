# PES team-tick steps 20/21: per-slot interiors, port spec (T3InteriorRE, 24-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − 0x140000000).
Bridge `http://127.0.0.1:8080`, one request at a time. Scratch `<workdrive>/gf-T3InteriorRE-scratch/`
(`d_4FF8B0/d_4F7BA0/d_A55D40` + 16 helper decompiles, `dis_4FF8B0` 478 lines, `dis_4F7BA0` 409 lines).
No repo edits. T3Port branch (`origin/T3Port`) read for the ported pure functions.

Verdicts: **CONFIRMED** = disassembly + image bytes this pass; **DOC** = cited doc; **INF** = inference; **OPEN** = named blocker.
Every float below was script-read from the image, never typed.

Prior: `docs/re/pes-re-t3.md` §5–§6 (§10 items 4, 5 = this ticket's subject), `pes-port-T3Wire.md` (T3Port's open helpers).

## §0 Constants (script-read; operand VA paired per brief rule 95)

Step-20 geometry (§1): `0x14259858c`=360.0 (`0x1404FFA19`), `0x14256AE1C`=1.0 (`0x1404FF9E9`),
`0x142565600`=0.5 (`0x1404FFA87`), `0x142588200`=0.1 (`0x1404FFA75`), `0x1425795E4`=2π (`0x1404FFB48`),
`0x14259BE40`=57.29578 (rad→deg, `0x1404FFB51`), `0x142598580`=180.0 (`0x1404FFB5E`),
`0x142598598`=−360.0 (`0x1404FFB7F`), `0x14253EE30`=100.0 (`0x1404FFB8D`), `0x14259222C`=5.0 (`0x1404FFEAE`),
`0x142591140`=30.0 (`0x1404FFE99`), `0x14259BE20`=25.0 (`0x1404FFEB1`), `0x14258C264`=3.0 (`0x1404FFED6`),
`0x14259BDE4`=2.5 (`0x1404FFEE9`), `0x14259BDEC`=6.0 (`0x1404FFCE4`), `0x1425656C0`=4.0 (`0x1404FFE08`),
`0x142565604`=2.0 (`0x1404FFE19`), `0x1425AB4C4`=625.0 (`0x1404FFDE9`), `0x1425C48E0`=525.0 (`0x1404FFE00`),
`0x1425C48D4`=115.0 (`0x1404FFF67`), `0x1425C4904`=−85.0 (`0x1404FFF81`), `0x142588210`=10.0 (`0x1404FFF93`),
`0x14259D658`=−3.5 (`0x1405001A4`), `0x14259BE5C`=90.0 (A8F7C0 advance, decompile `d_140A8F7C0`).
A92D40 returns ±π/2 on the degenerate axis (`0x1425A35A0`/`0x14260B394`) and ±π on the negative-x arm
(`0x142578480`/`0x142609F98`); A90FD0's degenerate epsilon is `0x14259BDC4`=1.19e-7.
Step-21 loop (§2): `0x14259BDD0`=0.69444 (`0x1404F7DFA`), `0x14259D5C8`=0.35 (`0x1404F7FBD`),
`0x14259BE2C`=40.0 (`0x1404F8128`), `0x14257A190`=0.6 (`0x1404F81B8`), `0x14259A764`=60.0 (`0x1404F81DB`),
`0x14259BE6C`=160.0 (`0x1404F81FA`), `0x1425A9BF0`=130.0 (`0x1404F820E`), `0x14259BE34`=50.0 (`0x1404F8216`),
`0x14259BE84`=−10.0 (`0x1404F81A1`), `0x14259BE1C`=20.0 (`0x1404F826C`), `0x14256AE38`=−1000.0 (`0x1404F7EB1`).

## §1 Step 20 (`FUN_1404FF8B0`): per-slot scan, statement sequence

Gates (unchanged from T3 §5, all CONFIRMED in `dis_4FF8B0.txt:9-34`):
`T+0x255 != 0` (`0x1404FF8D9`); carrier `FUN_140A627E0(match,0)` (=`[match+0x18]+0x2601C`, `d_140A627E0`)
`< 0x16` else `T+0x78 < 0x16` (`0x1404FF8EF–907`); `MI+0x116C == 1` (`0x1404FF946/94E JNZ 0x140500134`).

Per-outer-slot `k` = 0..10 (`R12D`, `0x1404FFA40–0x1405000AE`):

1. `cand = side·11 + k` (`0x1404FFA40 IMUL R15D,[R13+8],0xB; ADD R15D,EAX`). `cand > 0x15` → MISS
   (`0x1404FFA4C 0F87 …` = JA, bytes CONFIRMED). Miss = `8C35[k]=0, 8C40[k]=0xFF`, no band write.
2. On-pitch: byte at `MI-block + cand + 0x1748` (`0x1404FFA55`); zero → MISS.
3. `cand == carrier` (`0x140500174 CMP R15D,[RSP+0x28]; JZ 0x14050007F`) → skip write, keep scanning
   (falls to the roster-loop end WITHOUT storing: the slot keeps its previous tick values).
4. `found = 1` (`0x1404FFA69 MOV [RBP+0xB0],1`). RNG `tick = (int)(rnd·0.1+0.5)` (two sites:
   `0x1404FFA70 CALL 141E532D0; 0x1404FFA75 MULSS 0.1; 0x1404FFA87 ADDSS 0.5; 0x1404FFA93 CVTTSS2SI;
   0x1404FFA9D MOV ECX,EBX` — **int32 truncation**, CONFIRMED). Predicted pos
   `pred = pos + tick·vel` from `A48A10(G,cand)` `+0x560/55C/564 + +0x578/574/57C`
   (`0x1404FFABC–E4`; x/z only — no y).
5. `local_128 = d²(holderPos, outerPred)` (`0x1404FFB06–118`); holder pos was loaded once at head
   (`0x1404FF91B MOVSD holderXZ; 0x1404FF929 holderZ`). Bearing `A92D40(dz,dx)` → wrap `[0,360)` →
   `×57.29578` (`0x1404FFB22–51`); `local_184 = bearing + 180`, wrapped to `[0,360)`
   (`0x1404FFB5A–87`: `XMM15`=360.0, `-360.0` arm).
6. Inner loop over OPPONENT half (`R14D` = 0..10, `0x1404FFBB0–0x140500008`):
   `opp = (side==0)·11 + k2` (`0x1404FFB9–C6`: `CMP ECX,1; JA; TEST; SETZ AL; IMUL EDI,EAX,0xB` —
   side 0 → 11..21, side 1 → 0..10, **opponents**, CONFIRMED). On-pitch test, own RNG tick + predicted
   pos (`0x1404FFBED–C63`, same 0.1/0.5/int32 rule; `local_134/138/130`).
7. `d²outer↔inner` (`0x1404FFC71–93`), then `FUN_140A90FD0(&pts, holderPos, outerPred, innerPred)`
   (`0x1404FFC97`): segment-projection; `param_1[1] = −1` on degenerate (`fVar7 < 1.19e-7`, `< 0`, `> 1`).
   `local_16c = pts[1]`; `0x1404FFC9E COMISS 0.0,[RSP+0x3C]; JBE 0x1404FFE41`:
   - `local_16c ≥ 0` → GEOMETRY arm (§1.1) directly.
   - `local_16c < 0` (degenerate, incl. `−1`) → WRAP arm (§1.2).
8. Stores (`R12D` = outer k, `RAX` = `T = [R13+0x6D50]`):
   - Fall-through (all 11 opponents passed): `EDX = found = 1` (`0x140500015 MOVZX`),
     `ECX = 0xFF` (`0x14050001C MOV ECX,[RSP+0x68]`; `local_140` is init-`0xFF`, never rewritten — CONFIRMED).
   - Breaks: `DL = 0` + `ECX = opp` (ring-ratio fails `0x1404FFE38`/`0x140500140`) or `DL = 0` + `ECX = 0xFF`
     (ring-gate fail `0x140500149` → `0x14050001C`). All three skip the MOVZX (JMP polarity CONFIRMED).
   - `T+0x8C35[k] = DL` (`0x140500036`); `T+0x8C40[k] = ECX` (`0x14050003E`).
   - Meaning (byte-settled, §1.3): `(1, 0xFF)` = slot open, no opponent near;
     `(0, opp)` = opponent `opp` blocks the lane; `(0, 0xFF)` = slot out of holder range.
9. Roster scan (24 × `0x74` from `G+0x180`, `0x140500046–07D`): first entry with `+0x20 == cand`
   (`0x14050006D CMP …,R15D; JZ 0x140500150`), else no band write. On match: `+0xD == 0` required
   (`0x140500163 CMP …,0; SETNZ; JNZ skip` — nonzero skips, CONFIRMED); `cand != carrier`
   (`0x140500174`) required; then `T+0x8D50[k] = band(XMM13)` (`0x1405001BE`) where XMM13 is the
   running min (§1.1/1.2). Band = `Step20Band` (T3Port, §3 correction for NaN below).

### §1.1 Geometry arm (`LAB_1404FFE4F`, `0x1404FFE4F–0x140500140`)

`closest = (local_170, local_168)` (first A90FD0 result, or the second call's on the bVar4 path).
`d²close↔inner` (`0x1404FFE4F–7C`); `d = sqrt(d²holder↔close)` (`0x1404FFE81–94`, `1415A1780` = sqrt thunk);
ramp `d ≤ 5 → 0; d ≥ 30 → 1; else (d−5)/25` (`0x1404FFEAD–D1`: `XMM0`=5.0, `XMM8`=30.0, `XMM9`=25.0 —
note inverted sense vs step 19's ramp: 0 near, 1 far); `score = ramp·3.0 + 2.5` (`0x1404FFED6/E9`).
Holder facing `XMM6 = [A48910(G,holder)+0x124]` (`0x1404FFF04`); bearing
`FUN_140439E50(out, holderPos, outerPred)` (`0x1404FFF0C`); `diff = |wrap180(bearing − facing)|`
(`0x1404FFF19–57`, fabs via `ANDPS [0x142565D30]`); band
`diff ≤ 30 → 1.0; diff ≥ 115 → 0.0; else (diff−115)/(−85)` (`0x1404FFF5B–81`).
Ring gate: `(10 + 25·band)² < local_128` → `(0, 0xFF)` break (`0x1404FFF89–149`; `JA 0x140500149`,
CONFIRMED). Else `score = max(score, XMM7ramp)` (`0x1404FFFA8 MAXSS`), `XMM13 = min(XMM13, sqrt(d²close↔inner))`
(`0x1404FFFB4–FC4`, SIL-gated: SIL=1 skips the update and goes to the tail), then
`score² < d²close↔inner` → `(0, opp)` break (`0x1404FFFCF–E0` region → `0x140500140`, CONFIRMED),
else inner-loop tail (restore XMM10=100.0 etc., `0x1404FFFDD–0008`, continue).

### §1.2 Wrap arm (`0x1404FFCA8–DD3`, degenerate projection fallback)
Sign-projected side-word gate: `XMM6 = sign` (`T+0x254` byte → float, `0x1404FF95B–9F2`);
`ax = MA-side-word(+0x40)·sign`, `ox = outerPredX(+0x55C)·sign`, `az = MA-side-word(+0x38)·sign`,
where the MA words are `[MA+0x18]+side·0x14+0x40/+0x38` (`0x1404FFA0B/12`, saved at `[RBP+0xB8/C0]`);
ratio `sel = (ox ≤ ax) ? 1.0 : ((ox < az) ? (ox−az)/(ax−az) : 0.0)`
(`0x1404FFCA8–CE0`: `COMISS bx,ox; JC` ⟺ `bx < ox` i.e. `ox > ax` takes the sub-arm; sub-arm
`COMISS ox,az; JC` ⟺ `ox < az` takes the DIVSS, else 0.0 — branch-ladder polarity CONFIRMED from bytes;
the decompile's `fVar1/fVar2` are these same two MA words, not ball coords);
`f = sel·6.0 + 5.0` (`0x1404FFCE4/EC`); `d²outer↔inner < f²` required else next opponent
(`0x1404FFCF4–FB JBE 0x140500000` = continue). On pass: `SIL = 1`, save outer pred, wrap the
outer bearing (`0x1404FFD01–3D`), `A8F7C0` advance pair with `fVar22 = 100.0` (`0x1404FFD47/74`),
second `A90FD0` (`0x1404FFD79`); if it returns a different buffer, copy to locals
(`0x1404FFD8F–BD`). If `local_16c#2 < 0` → inner tail (continue, SIL stays 1);
else 100/625/525 ratio arm: `d² > 100` ? (`d²−100`)/525 : (`d² < 625` ? 1.0 : 0.0)
(`0x1404FFDDD–FFE08`), `g = ratio·4.0 + 2.0` (`0x1404FFE14–19`), `XMM7 = min(XMM7, XMM13-running)`
(`0x1404FFE26 MINSS`), `g² ≤ d²` → GEOMETRY arm (`0x1404FFE2F JBE 0x1404FFE4F`, CONFIRMED),
else `(0, opp)` break (`0x1404FFE38`).

### §1.3 Polarity audit (bytes, all CONFIRMED)

- Found flag: set ONLY by init-1 + explicit DL=0 breaks; the MOVZX path preserves 1. So
  `8C35 = 1` ⟺ inner loop exhausted (all opponents passed) — "slot open", NOT "target found".
  T3 §5's "found flag" gloss is kept as the field name; the sense is open-vs-blocked.
- `8C40 = opp` ⟺ that opponent broke the scan (blocked); `= 0xFF` ⟺ open or out-of-range.
  The two `(0,·)` cases are distinguished ONLY by ECX (opp vs 0xFF).
- `8D50` written only on the hit path AND roster-match AND `+0xD==0` AND `!= carrier`;
  value = `Step20Band(runningMin)`; NOT written on outer-miss (jump `0x1405001CD→09E` skips the scan).
- NaN edge (byte-settled): `0x140500187 COMISS 0.5,XMM13; JC → 1.0` — NaN sets CF → **NaN → 1.0**.
  T3Port `Step20Band` returns NaN for NaN (both comparisons false). One-line divergence, §3.

### §1.4 Geometry helpers (contracts for the port; decompiles read, pure parts CONFIRMED)

- `FUN_140A90FD0` (49 lines): segment closest-point `holder→outer` vs `inner`: returns buffer with
  `[1] = −1` degenerate (`fVar7 < 1.19e-7`, `t < 0`, `t > 1`), else the projection + `[1] = 0`…
  (tail lines: `if (param_1[1] <= 0 && != 0) param_1[1] = 0`). INF detail inside; contract CONFIRMED.
- `FUN_140A92D40` (24 lines): `atan2(dz,dx)` via `A92DC0(x/z)` + quadrant folds (±π/2 degenerate,
  ±π negative-x arm); radians. Contract CONFIRMED.
- `FUN_140439E50(out, a, b)` (26 lines): `*out = wrap360(deg(A92D40(*(b−a))))`; 0.0 when equal.
  Degrees. CONFIRMED.
- `FUN_14043A020(arg, bearingPtr)` (33 lines): `wrap180(*ptr − wrap360(arg))`-ish fabs fold
  (`±180` gate at `[0x142598580]`, `−360/−1·360` arms). Degrees. Contract CONFIRMED.
- `FUN_140A8F7C0(pt, angDeg, r)` (13 lines): `pt.x += cos/sin-path(ang)·r` via `A92C80(ang)` and
  `A92C80(ang+90)`; x/z plane advance. Contract CONFIRMED.
- `FUN_140A48910(G,id)` (10 lines): `id < 0x1A ? [G+(id+0xA5)·0x28] : [G+0x19C8]` — AI/anime record.
  `FUN_140A48A10` same with `+0xBF/0x1DD8` (slot record); `FUN_140A48A30` with `+0xA1/0x1928`, arg 0/1
  (team record by side). `FUN_140A627E0(match,0)` = `uint32 [[match+0x18]+0x2601C]` (carrier). CONFIRMED.
- `FUN_140A55D40(roster, side, 3)` (49 lines, `d_A55D40`): 24-entry scan (`0x74` stride, clamp 0x17);
  admit entry: `+6 == 0`, `+0x20 < 0x16`, `+0xD == 0`, side-fold match (`uVar2 == side`), then
  `flag = [roster+0x10E8+k]`; `flag == 4` → `flag = FUN_141F9C3B0(roster+k·0x74+0x68)`; admit iff
  `flag ≤ 3`. Returns 1/0. Detail INF (141F9C3B0 un-opened); gate contract CONFIRMED. **Closes T3 §10 item 5
  to contract+one-call.**

## §2 Step 21 (`FUN_1404F7BA0`): per-slot loop, statement sequence

Gates (T3 §6, re-CONFIRMED `dis_4F7BA0.txt:1-50`): latch clear (`0x1404F7BB3`);
`A55D40(roster,side,3) == 0` i.e. returns nonzero → `0xFF` (`0x1404F7BCC–DA`, note inverted sense:
nonzero = some entry admits = gate FAILS); `A60340(A40BD0(T)) ≥ 5` (`0x1404F7C05–15`;
`A40BD0` = `T+0xA29C ? zero : T+0xA294`, `A60340` = `x/100` — level-word/100, DOC condition-bands §86);
side-1 flag (`0x1404F7C1B–33`: `side < 2` and `MA+0x24C68+side == 0`).
Carrier `627E0`, else `T+0x78` (`0x1404F7C46–6E`); still `> 0x15` → tail return WITHOUT writing B8D0
(`0x1404F7C76 JA 0x1404F833A` — the only path that preserves the old value; CONFIRMED).

Restart-record gate (`0x1404F7D28–99`, CONFIRMED): `rec = A3E190(match+0xE0, 0)` (current record =
`match+0xE0+0x180698`, `d_140A3E190`); kinds `{1,2,3} (0x1404F7D2D CMP ECX,2; JBE)` or `0x10`
(`0x1404F7D32/35`) take the freshness arm, else fall to the loop: `A3E880` (record base),
`A62EA0(match, kind)` (support id; `d_140A62EA0`: kind<2 → `A489D0→A3B320` chain else 0xFF),
`age = MI+0x1198 − rec+0x10` vs `(int)(rnd·0.5+0.5)` (`0x1404F7D6E–85`): `age < rndInt` AND old B8D0
`≤ 0x15` (`0x1404F7D91/99`) → early return keeping B8D0 (RNG-gated hysteresis: `rndInt ∈ {0,1}`,
so a same-frame record keeps the pick iff the draw rounds to 1 — 50/50 per tick, CONFIRMED).

Pre-loop (`0x1404F7DA7–F64`, CONFIRMED):
- `XMM7 = [carrierAI+0x124]`, `XMM14 = [carrierSlot+0x554]` (`0x1404F7DA7/E6`).
- `dir = 458740(carrierSlot, 0.69444/rnd)` (`0x1404F7DF5–E09`; A6C6D0 + A92D40 chain, INF inside),
  wrapped `[0,360)` (`0x1404F7E22–39`), minus slot facing, wrapped vs ±180 (`0x1404F7E42–85`).
- Facing substitute: if `[carrierAI+0x40] != 1` (`0x1404F7E85 CMP …,1; JZ skip`) AND
  `|diff| ≤ 10.0` (`0x1404F7EA3 COMISS 10.0,XMM1; JA skip`) → `XMM14 := XMM7`
  (AI `+0x124` substitutes for slot `+0x554`, CONFIRMED — same read as T3 §3,
  substitute CONDITION now closed: AI-state≠1 + near-aligned).
- `oppMan = [A48A30(G, side==0)+0x250]` (`0x1404F7EAC–E0` — the OPPONENT team's +0x250 word).
- Ball ramp: `sign = (float)(int8)[T+0x254]`; `ramp = seg01(sign·ballX vs sign·MAx−20 / −10)`
  (`0x1404F7EFD–F5F`: `< −20 → 0; > −10 → 1; else (x−lo)/(hi−lo)`), where the MA word is
  `MA+[fold·5·4+0x44]` (`fold = side==0`: `MA+0x44` vs `MA+0x58`, `0x1404F7F29`) and ball x is
  `MA+0x2482C` (`0x1404F7F20`). 0 beyond 20 m, 1 within 10 m (sign-projected).

Per-slot loop (`RSI = R14D` = 0..10, dual counters same value, `0x1404F7F72–297`):

1. `cand = (side==0)·11 + k` (`0x1404F7F75–84`, same fold bytes as step 20) — **OPPONENT half**
   (side 0 → 11..21, side 1 → 0..10). `> 0x15` → skip; off-pitch (`MI+0x1748`) → skip.
2. `pred = pos + tick·vel`, `tick = (int)(rnd·0.35+0.5)` (`0x1404F7FBD MULSS 0.35; +0.5; CVTTSS2SI;
   MOV EDX,EDX` — int32 truncation CONFIRMED). x/z only.
3. Admit test (CONFIRMED polarities): `cand == oldB8D0` → admit (`0x1404F802B JZ 0x1404F80DA`,
   skips both table tests). Else: scan opponent's `8C35` row (`A48A30(G,side==0)`, `0x1404F8033–5C`):
   ANY nonzero in 11 → admit (`JNZ 0x1404F80DA`, CONFIRMED) — i.e. if the opponents' step-20 row
   has any open slot, everyone is admittable. Else the `T+0x558` scan (`0x1404F805E–B8`): per own
   slot `s` (on-pitch only), `id = T+0x558[s]` (`0x1404F809B`; `> 10 index → 0xFF`, `0x1404F8087–92`);
   `id == cand` → require `T+0xB8C4[mirrorIdx] != 0` (`0x1404F80CB CMP …,0; JZ skip`, CONFIRMED:
   zero skips the candidate), else continue scanning; no match → fall to carrier check (admit).
   (`mirrorIdx R9D` counts only on-pitch own slots, `0x1404F8087 JC` — off-pitch own slots don't
   consume a B8C4 index. INF-glue detail, bytes CONFIRMED.)
4. `cand == carrier` → skip (`0x1404F80DA JZ`).
5. `d = |carrierPos − pred|` (`0x1404F80E3–103`, sqrt). Distance gate
   (`0x1404F810F–2F`): `5·ramp+5 ≤ d` (`COMISS lo,d; JA skip`, CONFIRMED) and `d ≤ 40.0`
   (`COMISS d,40; JA skip`). So nearer than 5–10 m (ramp-scaled!) is EXCLUDED — anti-clump.
6. Bearing `439E50(buf, carrierPos, pred)` (`0x1404F8135–47`), `diff = |wrap180(bearing − facing)|`
   with the same fabs idiom (`0x1404F814C–8E`); `cand == oldB8D0` → `−10.0` (`0x1404F819D/1A1` —
   incumbent handicap, CONFIRMED); `× 0.6` (`0x1404F81B8 → 0x14257A190`); `+ d` (`0x1404F81D0`).
   (So `score = (diff − 10·incumbent)·0.6 + d`. T3Port `Step21Score` matches, operator order CONFIRMED:
   ADD, MUL, ADD.)
7. Level cone (`0x1404F81C9 CMP B8A4,1; JA loose` — tight iff `B8A4 ≤ 1`, CONFIRMED):
   tight: `43A020([RSP+0x130]=oppMan, buf) ≤ 60.0` else skip (`0x1404F81D6–E4`, `JBE 0x1404F8222` admit).
   loose: `score > 30.0 (XMM15 floor, 0x1404F7F64) → +0.0; angle ≥ 160 → +50.0;
   else +((angle−30)/130)·50` (`0x1404F81E9–21E`; T3Port `Step21LooseCone` matches, CONFIRMED).
8. Record support gate (`0x1404F8222–26C`): same kind arms; on restart kinds,
   `A62EA0(match, *A3E880(…)) == cand` → `+ 20.0` (`0x1404F8268/6C`, CONFIRMED).
9. Best: `UCOMISS best(−1000.0 init, XMM9), score; JP→CM; JZ→take; COMISS; JBE→skip`
   (`0x1404F8274–84`): first valid score takes over unconditionally (JZ on the −1000 init, CONFIRMED),
   then strict-greater, NaN-safe (NaN → JP → COMISS → JBE-taken → skip, CONFIRMED).
   (`R15D = cand` on take-over, `0x1404F828A`.)
10. Output: `T+0xB8D0 = best` (`0x1404F82E8`; `0xFF` init, `0x15` empty-scan sentinel — sentinel arises
    because take-over writes `EDI ≤ 0x15` and no-take-over leaves `R15D = 0xFF`… wait: `R15D` init
    `0xFF` (`0x1404F7EBA`); take-over sets `R15D = EDI` (a real cand ≤ 0x15). The `0x15` sentinel in the
    T3 doc comes from T3Port's `Step21Best` returning 0x15 on empty — PES's actual empty value is
    `0xFF` (init). **Correction: PES writes 0xFF on empty scan, not 0x15** (bytes: no other R15D writer).
    `0x15` appears only as the RANGE bound (`CMP R15D,0x15`, `0x1404F82EF`) for the stamp gate.)
    Stamp: `best ≤ 0x15` → `brain+0x6D40 = MI+0x1198` (`0x1404F82EF–FC`, CONFIRMED).
    The `> 0x15 → return WITHOUT writing B8D0` path is only the pre-loop carrier-fail
    (`0x1404F7C76`); post-loop always writes (even `0xFF`).

## §3 GF input map (per statement; GF symbol or new word + writer VA)

| PES read | GF home (CONFIRMED present) or NEW + writer |
|---|---|
| `T+0x255` latch | `lastTickPossession` (`teamAIcontroller.hpp:375`) — YES (inverted sense: latchSet = possession) |
| carrier `627E0` = `[[match+0x18]+0x2601C]` | `match->GetBallRetainer()` (`match.hpp:223`) → global id — YES |
| `T+0x78` fallback | local at `teamAIcontroller.cpp:~2516` (T3Wire §open-1); lane-leaves §4 rule DOC — PARTLY (needs member) |
| `MI+0x116C` phase | `IsInPlay()`+phase (`pes Né A5Judge`) — PARTLY (no 1/2/3 enum in GF) |
| `MI+0x1748` on-pitch | `GetActivePlayers()` — YES |
| `G+0x180` roster 24×0x74 (`+0x20` id, `+0xD` flag, `+6`, `+0x68`, `+0x10E8`) | NEW table; predicates `A55D40/A489F0` portable pure once it lands |
| `T+0x558[11]` mirror ids | NEW (11 ids); writer = mark-assign passes (defensive-shape §14.2: cleared 0xFF, `FUN_140536210` etc.) |
| `T+0xB8C4[11]` chain bytes | NEW (11 bytes); writer OPEN (defence-leftovers `FUN_140a63950` call-site named) |
| `T+0x8C35[11]` self-read (step 21) | `slotFlag` (T3Port `TeamTick`) — YES once wired |
| `T+0xB8D0` old (incumbent/record arms) | `bestSlot` (T3Port) — YES once wired |
| `T+0xB8A4` level (cone select) | `TeamTick::b8a4` — YES 1:1 |
| `T+0x254` sign byte | `Team::GetSide()` (`team.cpp:238`) — YES 1:1 |
| ball x/z + `MA+0x18` match block | `GetBall()->Predict(0)` — YES |
| `MA+0x2482C/34` analysis ball | NEW two floats; writer image-rows §3.1 `FUN_14045DF70/14045B090` DOC |
| `MA+0x44/0x58` per-side words (ramp) | NEW two floats; writer OPEN (no attribution this pass) |
| `MA+0x24C68+side` side flag | NEW one byte; writer OPEN |
| `[oppTeam+0x250]` man angle | facing (`GetDirectionVec`) — PARTLY (word packs more than facing; `pescarrier.hpp:489` T+0x250 note) |
| `AI+0x124` facing mirror | `GetDirectionVec` + `// ponytail:` (writer OPEN publisher family; `0x1404FFF04`/`0x1404F7DA7`) |
| `[carrierAI+0x40] == 1` test | NEW one byte on the AI record; writer OPEN (same publisher family) |
| `MI+0x1198` frame | `GetActualTime_ms()` — PARTLY (ms clock; PES frame = ÷54 per T3 §8) |
| `A3E190` record ring (8×800 B + count/head + current) | NEW; record-layouts §1 DOC (k=0 + k=1 sites) |
| `T+0xA294/A29C` level words | `teamLevelRaw` (27A0-inputs §8) — YES |
| RNG `141E532D0` draws (2 in step 20, 2+0/1 in step 21) | `pesrandom.hpp` stream — route per brief rules (stream index OPEN) |
| pl pos/vel `+0x55C/64/60 + +0x574/78/57C` | player kinematics — YES (slot-record layout assumed; offset map OPEN) |

## §4 Port spec (statement deltas vs T3Port's pure functions)

T3Port already ports (all CONFIRMED above, no change): `Step20GatesPass`, `Step20Band` (**one fix**:
NaN → 1.0 per `0x140500187 JC`; add `if (std::isnan(x)) return 1.0f;` or the early `x < 0.5`… note
`x < 0.5` is false for NaN so an explicit NaN arm is required), `Step21GatesPass`, `Step21LooseCone`,
`Step21Score` (order ADD/MUL/ADD CONFIRMED), `Step21Best` (first-valid-takes via JZ CONFIRMED;
T3Port's `!(s > best)` loop matches, but its `0x15`-sentinel comment is wrong: PES empty = `0xFF`
(init `R15D=0xFF`); `0x15` is only the stamp-gate bound — fix the comment, keep the code…
actually T3Port returns `0x15` on empty (`int id = 0x15`) — **PES returns 0xFF**. One-line fix:
init `id = 0xFF`). Hmm — but `Step21Best` takes scores only; the `0x15` never appears in the loop.
PES: no take-over → R15D stays `0xFF` → writes `0xFF`. So T3Port's `0x15` init is wrong; must be `0xFF`.
(The `0x15` "row sentinel" in T3 §6/§8 = misread of the `CMP R15D,0x15` stamp bound.)

Still OPEN (the per-slot bodies; GF homes in §3):
- **T3b** (step 20 → `slotFlag/slotTarget/slotScore`): outer sequence §1.1–9 + geometry §1.1/wrap §1.2
  as one function over (gates, holder pos/facing, ball line, roster table, own/opponent kinematics,
  two RNG draws). Red tests (T3 §8, still good): `NoTargetRecordedWithoutBall`,
  `OffPitchSlotMisses`, `ScoreBandClamps` (+ new: `BlockedSlotNamesOpponent`, `OpenSlotScoresFF`,
  `NaNGivesOne`).
- **T3c** (step 21 → `bestSlot/bestStamp`): record gate + pre-loop + per-cand sequence §2.1–10.
  Red tests: `RestartReceiverOnlyWithoutPossession`, `TightConeKeepsSixty` / `LooseConeAddsFifty`,
  `StampWrittenOnlyOnPick` (+ new: `IncumbentHandicapped`, `EmptyScanKeepsFF`, `NearClumpExcluded`).

## §5 Remaining OPEN items (honest remainder)

1. `A714E0`-table contents writer (image-rows §3.3) — step 19 only; unchanged.
2. AI-record writers: `+0x124` (facing mirror), `+0x40` (state byte), roster `+0x68/+0x10E8`
   cascade input `FUN_141F9C3B0` — one publisher-family hunt (T3 §10 item 2, widened).
3. `FUN_140A4F230` interior bands (contract exact) — step 19; unchanged.
4. ~~T3 §10 item 4 (step-20 geometry INF)~~ — CLOSED to statement level this pass (§1.1/1.2);
   residue: `A90FD0` projection detail INF + `A92DC0` polynomial (one pass, pure math, harness-able).
5. `A55D40` `+0x68`-flag/`+0x10E8` cascade — CLOSED to contract + one call (`141F9C3B0`); that call OPEN.
6. `FUN_1405AAF00` interior (contract exact) — step 19; unchanged.
7. `T+0x8C35` non-tick readers (`0x140957F68` etc. DOC list) — bodies OPEN; unchanged.
8. `FUN_140458740 → FUN_140A6C6D0` chain (step-21 facing term `0.69444/rnd`) — INF inside; needs one pass.
9. `MA+0x44/0x58` per-side ramp words + `MA+0x24C68` side flag writers — OPEN (no attribution this pass).
10. Live confirmation (read-only): `8C35/8C40/8D50/B8D0` during a match — unchanged.
11. T3Port corrections from §4: `Step20Band` NaN→1.0; `Step21Best` empty→0xFF (not 0x15);
    `Step21Best`'s `0x14256AE38`-site comment cites the wrong VA (`0x1404F7BCC` is the A55D40 call;
    the best-floor load is `0x1404F7EB1`).
12. Harness: no full-function runs were needed (every ambiguity settled from bytes; §1.3/§2 audits).
    If GF's port later diverges on the `A90FD0`/`458740` interiors, the diff harness (`run.py` + unicorn)
    can corpus them: stub `A48A10/A48910/A48A30/627E0` as getters, `141E532D0` as recorded draws,
    `1415A1780` as real sqrt, `A92DC0` real.
