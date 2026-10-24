# PES team-tick T3 (steps 19–21): re-assessment, new decodes, port spec (T3RE, 24-10-26)

Read-only RE. Image `<local>`
(file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`, one request
at a time. Scratch `<workdrive>/gf-T3RE-scratch/`
(`dis_500200.txt` 395 lines, `dis_4FF8B0.txt` 478 lines, `dis_4F7BA0.txt`
409 lines, `d_A714E0/d_5969B0/d_A4F230/d_4AB8E0/d_A40BD0/d_A55D40/d_5AAF00`
decompiles, `check_t3.py`: RESULT OK, `--selftest` OK).
No repo edits; `src/sim` searched read-only.

Verdicts: **CONFIRMED** = disassembly/decompile + image bytes this pass;
**DOC** = cited doc, not re-checked; **INF** = inference; **OPEN** = blocker named.

Prior: `docs/re/pes-re-tick-steps.md` §7–§9 (T3 bodies),
`docs/ports/pes-port-TeamT2.md` "T3-T13 status (run 5)" (the blocked verdict),
`docs/re/pes-re-team-tickets.md` steps 19–21 + §5.5 (reader census).

## §0 Constants (checker output, script-read, never typed)

```
0x14259D658 = -3.5        (step-20 angle-band divisor, 0x1405001A4 DIVSS)
0x14259D668 = -15.0       (step-19 ramp span, 0x140500741 DIVSS)
0x14259D654 = -3.0        (step-19 holder-match add, 0x14050078E ADDSS)
0x14259BE20 = 25.0        (step-19 ramp ceiling, 0x140500732 COMISS)
0x14259BE0C = 15.0        (step-19 angle gate, 0x1405004F0 COMISS)
0x14259BE84 = -10.0       (step-21 carrier penalty, 0x1404F81A1 ADDSS)
0x14259222C = 5.0         (step-19 ramp scale, 0x140500746 MULSS)
0x1425AB4C4 = 625.0       (step-20 d² gate 25², 0x1404FFDE9 COMISS)
0x1425C48E0 = 525.0       (step-20 far ratio gate, 0x1404FFE00 DIVSS)
0x1425C48D4 = 115.0       (step-20 mid ratio gate, 0x1404FFF67 MOVSS)
0x1425C4904 = -85.0       (step-20 near-angle floor, 0x1404FFF81 DIVSS)
0x14259BDEC = 6.0         (step-20 6.0 gate, 0x1404FFCE4 MULSS)
0x14259BE2C = 40.0        (step-21 chip-ramp low, 0x1404F8128 COMISS)
0x14259BDE4 = 2.5         (step-20 score add, 0x1404FFEE9 ADDSS)
0x14259BE1C = 20.0        (step-21 support add, 0x1404F826C ADDSS)
0x14259BDD0 = 0.69444..   (step-21 rate, 0x1404F7DFA MOVSS)
0x14257A190 = 0.6         (step-21 score scale, 0x1404F81B8 MULSS — NOT 0.7)
0x14259A764 = 60.0        (step-21 tight cone, 0x1404F81DB COMISS)
0x14259BE6C = 160.0       (step-21 loose cone, 0x1404FFAFA COMISS)
0x1425A9BF0 = 130.0       (step-21 loose span, 0x1404F820E DIVSS)
0x14259BE34 = 50.0        (step-21 loose scale, 0x1404F8216 MULSS)
0x14256AE38 = -1000.0     (step-21 best floor, 0x1404F7EB1 MOVSS)
0x14256AE1C = 1.0         (ramp tops / sign select)
0x142565600 = 0.5         (tick rounding addend)
0x14259D5C8 = 0.35        (step-21 jitter rate, 0x1404F7FBD MULSS)
0x142588200 = 0.1         (step-20 interp rate, 0x1404FFA75 MULSS)
0x142588210 = 10.0        (step-19/21 10 m gates)
0x142588218 = -1.0        (sign select)
```

## §1 Re-assessment table (run-5 blocker → status now)

| # | Run-5 blocker (TeamT2) | Status now | Closed by / decoded here |
|---|---|---|---|
| 1 | T3a: list block `FUN_140A714E0` "no writer found" | **CLOSED (was a mis-framing)** | This §2: `FUN_140A714E0` is a 13-line LEA selector (bytes CONFIRMED), not a table with a writer. The *selected tables* (`MA+0x229B4/0x22B9C`) are image-rows §3.3 OPEN — but step 19 only *reads* them (float + u8 index), so T3a ports with a `// ponytail:` naming the table writer hunt, same standing as the need tables (record-layouts §5 precedent) |
| 2 | T3a: sort `FUN_140A79640` tie rule OPEN | **CLOSED** | DOC lane-leaves §1: ascending by float key, stable (equal keys keep slot order), rank = sorted index. Step-19 call at `0x1405007DF` (`XOR EDX,EDX; LEA RCX,[RSP+0x40]; CALL`) CONFIRMED this pass |
| 3 | T3a: `FUN_1405969B0` INF | **CLOSED** | DOC mark-slot §1 + re-fetch this pass (`d_5969B0.txt`): stoppage designee → same-side holder → `T+0x78` fallback. Step-19 call at `0x14050046B` CONFIRMED |
| 4 | T3b: `pl+0x124` OPEN | **CLOSED as read/role; writer OPEN** | This §3: AI-record (`A48910`) `+0x124` f32, degrees, a facing/heading mirror. Step-20 read `0x1404FFF04` (subtracted from a `439E50` bearing), step-21 read `0x1404F7DA7` (substitutes for slot facing `+0x554`). Writer OPEN (anime-publisher family, same as 27A0-inputs §6 card words). GF 1:1 = player facing (`GetDirectionVec`) |
| 5 | T3b: P+0x40 keeper bit (27A0 unbound 2) | **CLOSED** | DOC 27a0-inputs §2: `(P+0x40>>0x24)&1` read + `*0.5` branch CONFIRMED; writer OPEN (publisher). NB: step 20 does NOT read P+0x40 — run 5 listed it via `FUN_140439FE0`/pair-table path confusion; the actual step-20 bearer inputs are `+0x55C/564/560/574/578/57C` + `+0x124`. The keeper bit gates 27A0 only |
| 6 | T3b: pair table TM+0x23F7C writer OPEN | **CLOSED** | DOC mark-table §1: `FUN_14045B420` via `FUN_1404587F0` (`0x228D4+0x16A8=0x23F7C`), `P[a][b]=bearingDeg`. NB: same caveat as #5 — step 20 reads the pair table NOWHERE in its 478 lines (no `439FE0` call; verified by disassembly). The pair table feeds T7a/step 35, not T3b. Run 5's "through FUN_140439FE0" is refuted for step 20 |
| 7 | T3c: `pl+0x124` | **CLOSED as read/role** | Same as #4 (step-21 site) |
| 8 | T3c: `T+0x78` fallback writer open | **CLOSED** | DOC lane-leaves §4 (writer chain `4FB2A0→4FB380/4FB780→F0E50`) + team-tickets step 40c. Step-21 fallback sites `0x1404F7C53–6E` re-read this pass: holder `627E0` → else `T+0x78`, gate `< 0x16` at `0x1404F7C72` |
| 9 | T3c: NearestEntry (27A0 named 5) | **CLOSED (layout + writer; tie rule closed)** | DOC image-rows §3.2: `MA+0x2470C` layout + writer `FUN_14045B090` CONFIRMED. Tie rule: the writer sorts with `FUN_140A79640` = stable/slot-order (lane-leaves §1), so "tie rule OPEN" in image-rows §3.2 is closed by transitivity. NB: step 21 does NOT read NearestEntry directly — its carrier check is `627E0` + `MA+0x2483C` fallback; NearestEntry feeds step 19 via `FUN_1405AAF00` (`MA+0x24710/247BE` d²/rank test) |
| 10 | T3a: `T+0x78` (step-19 `5969B0` tail) | **CLOSED** | Same as #8 |
| 11 | Step-19 score composition (tick-steps §15 item 9) | **CLOSED** | This §4: exact key build + ramp + holder-match add, no RNG in step 19 (the "RNG·0.35" in tick-steps §7 is step 21's `0x1404F7FBD`, misattributed — correction §7) |
| 12 | Step-21 loose-path XMM15 provenance (tick-steps §15 item 10) | **CLOSED** | This §6: XMM15 = 30.0 (`0x142591140`, loaded `0x1404F7F64`), the loose-arm floor. The "which call result feeds 0x1404F81EE" is answered: no call — it is the constant, compared at `0x1404F81EE COMISS XMM15,XMM0` |
| 13 | Step-21 `×0.7` (tick-steps §9) | **CORRECTED → ×0.6** | This pass: `0x1404F81B8 MULSS XMM6,[0x14257A190]` resolves by disp to `0x14257A190` = 0.6 (`0x3F19999A`). Neighbour `0x14257A194` = 0.7 differs — checker mutation row. No 0.7 read exists in step 21 |

## §2 `FUN_140A714E0`: decoded (13-line selector, bytes CONFIRMED)

```c
longlong FUN_140A714E0(longlong base, char sign) {
  if (sign == 1) return base + 0x22B9C;
  return base + 0x229B4;
}
```

Bytes at `0x140A714E0`: `48 8D 81 9C 2B 02 00` (LEA RAX,[RCX+0x22B9C])
`80 FA 01` (CMP DL,1) `74 07` (JZ) `48 8D 81 B4 29 02 00`
(LEA RAX,[RCX+0x229B4]) — checker rows `a714e0.bytes/alt`.
Callers pass `base = MA` (`[RAX+0x18]`, the match-team block) and
`sign = T+0x254`: step-19 sites `0x140500387` and `0x1405006BA`
(both `MOVZX EDX,R13B; MOV RCX,[RAX+0x18]; CALL`, CONFIRMED dis).
The "writer" hunt (image-rows) is therefore about the *selected tables*,
not this function: `puVar + (half·11 + u8[base+slot+0xB2])·8 + 4`
(step-19 `0x1405006CF–6DF`: `MOVZX EAX,[RAX+R15+0xB2]; IMUL RCX,RCX,0xB;
ADD RCX,RAX; MOVSS XMM6,[RDX+RCX*8+4]`) — a u8 index array at
`+0xB2` and an 8-byte-stride float table (float at +4), the same shape
as the record-layouts §5 need tables. **There is no third writer to find
for T3's sake**: both call sites' inputs are (MA selected table, sign,
slot) and the read rule is exact. The table *contents* writer stays OPEN
(image-rows §3.3) and rides a `// ponytail:`.

## §3 `pl+0x124` (AI record +0x124): read + role CONFIRMED, writer OPEN

Record identity: both readers take `RAX = FUN_140A48910(G, id)` —
`*(G + (id+0xA5)·0x28)` for `id < 0x1A` (re-fetched this pass) — the
published AI/anime-state record (27A0-inputs §6: same `PA` family as the
card words `+0x1A/1C/38/40/60`).

- Step-20 site `0x1404FFF04`: `MOVSS XMM6,[RAX+0x124]` where
  `RAX = A48910(G, holder)`. The call at `0x1404FFF0C` is
  `FUN_140439E50(out=[RBP-0x58], ball=[RSP+0x58], pred=[RSP+0x48])`
  (bearing); then `0x1404FFF1D SUBSS XMM1,XMM6` — the `+0x124` float is
  subtracted from a degree bearing and the fabs'd diff is gated against
  the 30.0/115.0/−85.0 bands (`0x1404FFF5B–81`). **A `+0x124` in degrees.**
- Step-21 site `0x1404F7DA7`: `MOVSS XMM7,[R15+0x124]` where
  `R15 = A48910(G, carrier)`. At `0x1404F7E85` the code tests
  `[R15+0x40] == 1`; on that arm with `|wrap(bearing − facing)| ≤ 10.0`
  (`0x1404F7EA3/A6`), `XMM14 := XMM7` — i.e. `+0x124` **substitutes for
  the slot facing `[R14+0x554]`** loaded at `0x1404F7DE6`. **A facing in
  degrees on the AI record, mirroring slot `+0x554`.**

Writer: OPEN. No `+0x124` store attribution was established this pass
(the publisher copier `FUN_140A47290`, disassembled, is a small
`0x348`-area copier, not the origin). Same family as the card-word
writers (anime-state publisher bulk copy, 27A0-inputs §2/§6) — one hunt,
not two. GF 1:1: player facing (`GetDirectionVec` covers heading reads
per holder-tree §3 PARTLY row; `pescarrier.hpp` attack-angle helpers).
Port: feed from GF facing with `// ponytail: AI+0x124 writer OPEN
(publisher family); 0x1404FFF04/0x1404F7DA7`.

## §4 Step 19 (`FUN_140500200`, 395 lines): exact rule

Full disassembly `dis_500200.txt`, all sites below re-read.

**Head — level ladder** (`0x14050022C–327`):
`level = T+0xB8A4`. `≥ 4`: all-11-bits mask (`0x140500240 BTS EDI,ESI`
loop), store `T+0xB8B8` at `0x1405008BD` (`41 89 B8 B8 B8 00 00`,
checker row), return. Else level→licence map
(`0x1405002F2 TEST; 0x1405002F6/2FB/300 SUB EBX,1/JZ` chain):
0→0, 1→2, 2→4, 3→6, else 5 → `[RSP+0x20]` (the licence count).

**Class branch** (`0x14050024F–2C5`): if `0 < level` and
`FUN_1405AAF00(MA, side)` (in-play + holder/NearestEntry + run-state +
RNG + ball-height gates; decompile `d_5AAF00.txt`, INF detail):
11-scan from `side·11`; per slot `< 0x16`, fold `slot%11`
(`MUL 0xBA2E8BA3; SHR 3; IMUL 11; SUB` at `0x140500280–28A`),
`T+0x4C00[fold] > 1` → `BTS EDI, (slot − side·11)`; store `T+0xB8B8`
at `0x1405002BE` (checker row), return. (Note: `> 1`, not `> 0` —
`0x1405002A7 CMP EAX,0x1; 0x1405002AA JLE skip`.)

**List branch** (`0x1405002CA–88E`):
1. `n = FUN_140A4F230(MA, &cnt, &bits)` (`0x140500341`): target count +
   bitmask (decompile `d_A4F230.txt`: 11-walk over `A714E0`-selected
   rows with the 1.5/12.0/5.0/7.0/52.0 band tests; constants
   `0x14259BDDC/14259BE04/14259222C/14259BDF0/1425CDAA0`
   script-read §0; INF detail inside, contract CONFIRMED).
2. Side-sign walk (`0x140500370–3D6`): per global slot, `id =
   A714E0(MA,sign)[k]` (`0x140500387`); if `id < 0x16` and
   `A48A10(G,id)` exists and `sign·ballX(G+0x2AA8+0x18) ≤
   sign·pl[id].x` (`0x1405003C1 MULSS / 0x1405003C9 COMISS / JA`)
   → `R12B++`. `cnt = min(cnt, R12)` (`0x1405003D8–3E9 CMOVC`);
   if `cnt ≥ licence` (`0x1405003ED CMP / 0x1405003F1 JNC`) → **full
   mask** (`0x1405003F3 BTS` loop), store at `0x14050088E`, return.
   (The `T+0x78` fallback's remaining arm: the `JA 0x1405003D1` skip
   counts only sign-side slots — slots behind the ball-side line
   never increment R12. No second arm exists; the walk is the whole rule.)
3. Resolver + record test (`0x140500402–53F`): `man =
   FUN_1405969B0(MA, side)` (`0x14050046B`); if `man < 0x16`:
   `rec = A48A10(G, man)`; `ang = 43A0A0(4AB8E0(rec, buf))`
   (`0x1405004CD` = `439E50(buf, rec+0x55C, rec+0x5EC)`;
   `0x1405004D9` = wrap-diff); if `|ang| > 15.0`
   (`0x1405004F0 COMISS / JBE skip`) and role test
   `FUN_140A76240(rec2)` (`0x14050051D`; returns `rec+8` or
   `rec+(byte[rec+4]+1)·0xC`) with `[man.z − rec.z] sign vs −1.0/1.0`
   (`0x140500527–53B`) → `[RSP+0x22] = 1` (holder-match flag).
4. Sort-key build (`0x140500544–7D8`): two half-lists at `[RSP+0x40]`
   (init `0xFF`, `0xA0A0…` rank-fill at `0x1405005D9–5F0`); per own
   slot `EBX = side·11+k` (`0x140500600–604`), on-pitch test
   (`MI+0x1748`, `0x140500621`), class fold `T+0x4C00[k%11]`
   (`0x14050063B–65D`): `≥ 7` → key `0xC2C80000` (= −100.0, far-last)
   with instant `BTS EDI,R14D` (`0x140500663–6A6`); else key =
   `tableFloat − distRamp·5.0 [+ (−3.0) on holder-match]`, where
   `tableFloat = A714E0(MA,sign)[half·11 + u8[base+k+0xB2]]`
   (`0x1405006BA–6DF`), `d = sqrt((manX−ballX)² + (manZ−ballZ)²)`
   (`0x1405006F6–721` via `1415A1780`), ramp =
   `d ≤ 10 → 1.0; d ≥ 25 → 0.0; else (d−25)/(−15)`
   (`0x140500726–741`; XMM15/XMM7/XMM12 = 10.0/25.0/−15.0),
   `× 5.0` (`0x140500746`, XMM11); holder-match
   (`[RSP+0x22]`, `0x14050074F–78E`): `holderRec =
   A48A10(G, resolverMan)`; if `|holderZ − manZ|` sign-matches
   XMM13 (±1.0 from step 3) exactly (`UCOMISS + JP/JNZ` at
   `0x140500786–78C`) → `key += −3.0` (`0x14050078E`, XMM10).
   Keys stored as `(slot, key)` pairs (`0x140500793–7C4`).
5. Sort + pick (`0x1405007D8–88E`): `FUN_140A79640([RSP+0x40], 0)`
   (`0x1405007DF`); walk own half in rank order
   (`0x140500841–85C` list-index math); per entry `< 0x16`,
   `fold = id%11`, `BTS EDI,fold` for the first `licence`
   (`[RSP+0x20]`) entries (`0x14050086E CMP R8B,R12B / JNC stop`).
   Store `T+0xB8B8` at `0x14050088E`, return.

**Score composition (closes tick-steps §15 item 9):**
`key = tableFloat − ramp(d)·5.0 − (holderMatch ? 3.0 : 0)`,
`ramp(d) = d≤10 ? 1 : d≥25 ? 0 : (d−25)/(−15)`, `d` = man-to-ball
(analysis ball `MA+0x2482C/24834` at `[RSP+0x30]`).
**No RNG anywhere in step 19** (zero `141E532D0` calls in 395 lines —
tick-steps §7's "RNG·0.35+0.5" is step 21's `0x1404F7FBD`, §7 correction).
Constants: 10.0/25.0/−15.0/5.0/−3.0/−1.0/1.0/−100.0-key/15.0-gate.

## §5 Step 20 (`FUN_1404FF8B0`, 478 lines): exact rule

Full disassembly `dis_4FF8B0.txt`.

**Gates** (all must pass, else return with tables untouched):
`T+0x255 ≠ 0` (`0x1404FF8D9`); carrier `627E0 < 0x16` else
`T+0x78 < 0x16` (`0x1404FF8EF–907`); `MI+0x116C == 1`
(`0x1404FF946/94E JNZ 0x140500134` — whole body skipped otherwise).

**Per-slot loop** (`R12` = 0..10, `0x1404FFA40–0x1405000AE`):
`cand = side·11 + k` (`0x1404FFA40 IMUL`); `> 0x15` → miss
(`0x1404FFA4C JA 0x1405001CD`: `8C35=0, 8C40=0xFF`);
off-pitch (`MI+0x1748`, `0x1404FFA55`) → miss; `cand == carrier`
(`0x140500174 CMP`) → miss (skip write, keep scanning);
roster scan (24 × `0x74`, `0x140500060–07D`): `+0x20 == cand` and
`+0xD == 0` required, else miss; bands
(`0x14050017F–1A4`): `XMM13 = clamp01((XMM13 − 4.0)/−3.5)`
with the `0.5`-select (`0x14050017F COMISS XMM0(0.5),XMM13; JC`):
below 0.5 → 1.0, above 4.0 → 0.0, else scaled —
then `T+0x8D50[k] := XMM13` (`0x1405001BE`, only on this path),
`8C35/8C40` written at `0x140500036/03E` on the hit path
(`0x1404FFE38` region: `DL` flag, `ECX` id).
Interior geometry (hit path, `0x1404FFB96–0x140500015`): predicted
positions `pred = pos + tick·vel` with `tick = (int)(rnd·0.1+0.5)`
(`0x1404FFA70/1404FFBED` RNG sites), `625.0/525.0` d²/ratio gates,
`6.0/5.0` window, `30.0/5.0/25.0/3.0/2.5` score arms, `115.0/−85.0`
angle bands, `+0x124` facing adjust (§3); INF detail inside, all
operand VAs paired in §0.

**Outputs:** `T+0x8C35[k]` u8 found flag (`0x140500036`,
`42 88 94 20 35 8C 00 00`, checker row); `T+0x8C40[k]` u32 target id,
`0xFF` = none (`0x14050003E`; miss path `0x1405001E7`
`42 C7 84 A0 40 8C 00 00 FF 00 00 00`, checker row);
`T+0x8D50[k]` f32 score (`0x1405001BE`) — write-only diagnostic
(DOC team-tickets §5.5: single disp32 site image-wide; NOT ported
as state, but the store is exact and cheap — port it to keep the
write stream 1:1; no reader exists).

**Readers (DOC team-tickets §5.5, not re-scanned):** `8C35` has 8+
(`0x1404F8055` = step 21 self-read, `0x14052C061`, `0x14054C8C2`,
`0x1405574DD`, `0x1405603B4`, `0x1405632A9`, `0x140563DDD`,
`0x140570295`, `0x140957F68`, `0x140959A83`, `0x140A1E57B`,
`0x140A3060E`); `8C40/8D50` have none.

## §6 Step 21 (`FUN_1404F7BA0`, 409 lines): exact rule

Full disassembly `dis_4F7BA0.txt`.

**Gates** (fail → `T+0xB8D0 = 0xFF`, `0x1404F7BE3`):
`T+0x255 == 0` (`0x1404F7BB3`); `FUN_140A55D40(roster, side, 3) == 0`
(`0x1404F7BCC–DA`; decompile `d_A55D40.txt`: some roster entry with
`+0x20 < 0x16` on this side, `+0xD == 0`, `+0x68`-flag/`+0x10E8`
cascade — INF detail); `FUN_140A60340(FUN_140A40BD0(T, buf)) ≥ 5`
(`0x1404F7C05–15`; `A40BD0`: `T+0xA294` level word unless
`T+0xA29C` set, decompile `d_A40BD0.txt`); side-1 flag
(`0x1404F7C1B–33`: `MA+0x24C68+side == 0` when `side < 2`).
Carrier: `627E0` (`0x1404F7C46`); `> 0x15` → `T+0x78` fallback
(`0x1404F7C53–6E`); still `> 0x15` → tail return **without writing
`B8D0`** (`0x1404F7C76 JA 0x1404F833A` — the only path that leaves
the old value; every other fail writes `0xFF`).

**Restart-type gate** (`0x1404F7D28–35`): `rec = A3E190(T+0xE0, 0)`;
`(kind−1) ≤ 2 || kind == 0x10`, else the record-window arm
(`0x1404F7D37–99`: `A3E880/A62EA0` + `(frame − recFrame) <
(int)(rnd·0.5+0.5)` retest keeps the incumbent `B8D0`).

**Per-slot loop** (`RSI` = 0..10, `0x1404F7F72–297`):
`cand = sideFold·11 + k` (`0x1404F781 IMUL EDI,EAX,0xB; ADD EDI,R14D`);
`> 0x15` → skip; off-pitch → skip; predicted pos
`pred = pos + tick·vel`, `tick = (int)(rnd·0.35+0.5)`
(`0x1404F7FBD MULSS 0.35 / 0x1404F7FC8 ADDSS 0.5 /
0x1404F7FD0 CVTTSS2SI`); `cand == incumbent B8D0` → admit w/o the
`8C35/B8C4` tests (`0x1404F802B–2D JZ 0x1404F80DA`); else
`8C35`-row test (`0x1404F804E–5C`: any of own 11 `8C35` set skips
the `558`-table scan) + `T+0x558`-table scan (`0x14050068–0B8`:
`id == cand` → `T+0xB8C4` set required, `0x1404F80CB`);
`cand == carrier` → skip (`0x1404F80DA`); `d = |carrier − pred|`,
gate `2·5.0 ≤ d ≤ 40.0` (`0x1404F810F–12F`: `XMM1 = 5.0·ramp...`
CONFIRMED arms); bearing `439E50` + facing diff vs `+0x554`
(`0x1404F8135–19D`); `cand == incumbent` → `−10.0`
(`0x1404F81A1`); `× 0.6` (`0x1404F81B8` → `0x14257A190`,
**correction vs tick-steps §9's 0.7**); `+ d` (`0x1404F81D0`);
level cone (`0x1404F81C9 CMP B8A4,1`): tight (`≤ 1`):
`43A020(...) ≤ 60.0` else skip (`0x1404F81D6–E4`); loose:
`score > 30.0 ? 0.0 : angle ≥ 160.0 ? 50.0 :
((angle − 30.0)/130.0)·50.0`, added (`0x1404F81E9–21E`;
XMM15 = 30.0 floor — closes tick-steps §15 item 10);
record gate (`0x1404F8222–26C`: kind/0x10 arms + `A62EA0 == cand`
→ `+ 20.0`); best tracking from `−1000.0` (`0x1404F8274–28A`:
`UCOMISS/JP/JZ/JBE` — NaN-safe strict-greater take-over).

**Outputs:** `T+0xB8D0 := best` (`0x1404F82E8`,
`44 89 B8 D0 B8 00 00`, checker row; `0xFF` = none, `0x15` = row
sentinel on the empty scan); `brain+0x6D40 := MI+0x1198`
(`0x1404F82FC`) only when `best ≤ 0x15`.

## §7 Corrections to existing docs

| doc:line | old | new | evidence |
|---|---|---|---|
| tick-steps §7 "Score: RNG·0.35+0.5 …" for step 19 | RNG in step-19 score | step 19 has NO RNG (zero `141E532D0` in 395 lines); `0.35/0.5` is step 21's `0x1404F7FBD/C8` tick build | `dis_500200.txt` CALL census (§4); `dis_4F7BA0.txt:223` |
| tick-steps §9 "×0.7" (step 21) | `×0.7 [0x14257A194]` | `×0.6 [0x14257A190]` (disp-resolved `0x1404F81B8+8+0x2081FD0 = 0x14257A190` = 0.6) | bytes + script-read §0; checker `c.0.6-not-0.7` |
| tick-steps §9 "loose-path XMM15 provenance OPEN" (§15 item 10) | OPEN | XMM15 = 30.0 (`0x142591140`, `0x1404F7F64`) | `dis_4F7BA0.txt:200` |
| TeamT2 run-5 "T3b reads P+0x40 / pair table via 439FE0" | step-20 inputs | step 20 calls neither; its bearer inputs are pos/vel/`+0x124`/roster/8C35-self | `dis_4FF8B0.txt` (no `439FE0`, no P+0x40 read) |
| TeamT2 run-5 "T3c reads NearestEntry" | step-21 input | step 21 reads carrier/`T+0x78`/roster/records; NearestEntry feeds step 19 via 5AAF00 | `dis_4F7BA0.txt` (no `2470C` read) |
| image-rows §3.2 "tie rule OPEN" | OPEN | CLOSED by transitivity: writer sorts via A79640 = stable/slot-order | lane-leaves §1 + `0x14045B090` writer |

## §8 Port spec (replaces the T3 `// ponytail:` leaves)

Tick order (unchanged): 19 → 20 → 21, after step 17, before step 22.
All three run every team tick (no phase gate of their own; step 20
additionally requires `MI+0x116C == 1` in-body).

```cpp
// New TeamTick state (pesteamtick.hpp; init: mask 0, flags 0, ids 0xFF, scores 0.0):
uint32_t eligibleMask = 0;        // T+0xB8B8 (step 19)
uint8_t  slotFlag[11] = {};       // T+0x8C35 (step 20; the only live table)
uint32_t slotTarget[11];          // T+0x8C40 (step 20; init 0xFF; no reader — keep for 1:1 writes)
float    slotScore[11] = {};      // T+0x8D50 (step 20; write-only diagnostic; keep for 1:1 writes)
uint32_t bestSlot = 0xFF;         // T+0xB8D0 (step 21; 0xFF none, 0x15 row sentinel)
int      bestStamp = 0;            // brain+0x6D40 (step 21; MI+0x1198 frame)
// +0x124 feed: player facing (GetDirectionVec) with:
//   // ponytail: AI+0x124 writer OPEN (publisher family); 0x1404FFF04/0x1404F7DA7.
// Selected-table feed (A714E0 rows): caller-supplied u8-index + float tables with:
//   // ponytail: MA+0x229B4/0x22B9C writer OPEN (image-rows §3.3); 0x140A714E0.
```

Inputs with GF homes (read-only grep over `src/sim`, origin/main):
`T+0xB8A4` level → `TeamTick::b8a4` (YES 1:1); `T+0x4C00` class →
`SideRoles::b` (YES 1:1, `pesroles.hpp`); `T+0x254` sign →
`Team::GetSide()` (YES 1:1, `team.cpp:238`); latch `T+0x255` →
`lastTickPossession` (YES); carrier → `GetBallRetainer()` (YES);
`T+0x78` → `teamKickMan/kickValid` stand-in
(`teamAIcontroller.cpp:2512-2513`; closable per holder-tree §3);
ball pos → `GetBall()->Predict(0)` (YES); analysis ball
`MA+0x2482C/34` → NONE (new: two floats; writer image-rows §3.1
`FUN_14045DF70/14045B090` CONFIRMED); `MI+0x1748` on-pitch →
`GetActivePlayers()` (YES); `MI+0x116C` phase → `IsInPlay()`+phase
(PARTLY: no 1/2/3 enum); roster `G+0x180` → NONE (new 24-entry
table; predicates `A55D40/A489F0` portable pure once it lands);
`T+0x558` table → NONE (new 11 ids); `T+0xB8C4` row → NONE (new
11 bytes); `MI+0x1198` frame → `GetActualTime_ms()` (PARTLY: ms
clock, PES frame = ÷54); RNG → `pesrandom.hpp` stream (NOT the
global cell — steps 20/21 call `FUN_140A57800`-adjacent
`141E532D0`; GF routes all RNG via `pesrandom.hpp` per brief rules);
`A3E190` record ring → NONE (record-layouts §1: 8×800 B + count/head
+ current record; k=0 + the one k=1 site); `T+0xA294/A29C` level
words → `teamLevelRaw` (YES per 27A0-inputs §8); `MA+0x24C68` side
flag → NONE (one byte); `MA+0x2483C` holder fallback → holder word
(YES fragment); `MA+0x24710/247BE` NearestEntry → `BuildNearest`
(PARTLY: layout+rule exact, time-slice/tie now closed §1#9);
`T+0x250` man angle → facing (PARTLY).

Slices (≤3 pieces each, red tests named):
- **T3a** (step 19 → `eligibleMask`): (1) level≥4 all-ones +
  licence map, test `LevelFourMasksEverySlot`; (2) class branch
  (`4C00>1` + 5AAF00 gate), test `Level1to3GatesOnClass`;
  (3) list walk + sort + pick, test `Level0PicksRankedSlots`.
- **T3b** (step 20 → `slotFlag/slotTarget/slotScore`): (1) latch +
  carrier/`T+0x78` + in-play gates, test
  `NoTargetRecordedWithoutBall`; (2) roster + miss values
  (`8C40=0xFF`), test `OffPitchSlotMisses`; (3) band write
  (`8D50=(x−4.0)/−3.5` clamp), test `ScoreBandClamps`.
- **T3c** (step 21 → `bestSlot/bestStamp`): (1) no-possession +
  restart-type gates, test `RestartReceiverOnlyWithoutPossession`;
  (2) tight/loose cone + `−10.0/+20.0/×0.6` score,
  test `TightConeKeepsSixty` / `LooseConeAddsFifty`;
  (3) stamp write, test `StampWrittenOnlyOnPick`.

## §9 Checker with mutations

`<workdrive>/gf-T3RE-scratch/check_t3.py` — self-checking, no PES bytes
in repo: 30 float rows (§0) + `A714E0` body bytes + 4 store opcodes
(`0x1405002BE/88D` B8B8, `0x1404F82E8` B8D0, `0x140500036` 8C35,
`0x1405001E7` 8C40=0xFF) + MULSS-disp resolution to `0x14257A190`
(0.6, not the 0.7 neighbour) + `--selftest` neighbour check.
RESULT OK; SELFTEST OK. Mutations demonstrated: byte-row truncations
(4/4 caught during construction, reverted), 0.6↔0.7 neighbour
(caught by `c.0.6-not-0.7`, reverted).

## §10 Open items (honest remainder)

1. `A714E0`-selected table contents writer (image-rows §3.3) — reads
   exact, contents need the writer hunt (72 unchecked A79640 callers).
2. AI `+0x124` origin writer (publisher family; §3) — one hunt shared
   with the card-word writers.
3. `FUN_140A4F230` interior band detail (§4 item 1; contract exact).
4. Step-20 interior geometry INF detail (all operand VAs paired; the
   reduction to a formula per arm is one more pass).
5. Step-21 `A55D40` `+0x68`-flag/`+0x10E8` cascade detail (gate exact).
6. `FUN_1405AAF00` interior (gate contract exact; run-state + RNG arms
   INF detail).
7. `T+0x8C35` non-tick readers (`0x140957F68` etc.; DOC list, bodies OPEN).
8. Live confirmation (read-only): `B8B8/B8D0/8C35` during a match.
