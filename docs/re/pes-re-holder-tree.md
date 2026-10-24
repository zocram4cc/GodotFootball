# PES holder tree: FUN_1404F0E50 scoring, output, port spec (HolderTreeRE, 26-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`, one request
at a time. Scratch: `<workdrive>/gf-HolderTreeRE-scratch/`
(`d_4F0E50.c`, `dis_4F0E50.txt` = 3424 lines, `d_4F6110.c`, `d_4F4AB0.c`,
`check_holder_tree.py`: RESULT OK — 49 floats + 26 opsites + 8 structural +
6 byte rows; `--selftest` OK: base clean, 8 mutations each caught).
Prior: `pes-re-lane-leaves.md` §4 (writer chain 4FB2A0→4FB380→F0E50),
`pes-re-team-helpers.md` §6 (spine + contract), `pes-re-mark-slot.md` §3
(constants 9.0/36.0/0.1/0.35/0.7/25.0/3.0/0.0).

Verdicts: **CONFIRMED** = decompile + asm (checker byte-covers pinned sites);
**INF** = inference; **OPEN** = not decoded, blocker named.

Closes lane-leaves §8 item 1 (`FUN_1404F0E50` not decoded).

## §0 Constants (checker output, script-read, never typed; operand VA per brief 19-10-26)

```
0x142565600 = 0.5       (int-noise addend: 1404F2063 ADDSS, 1404F2D60, 1404F3AA6, 1404F40A9 MULSS-arm, ...)
0x142565604 = 2.0       (lag addend: 1404F171A ADDSS score gate, 1404F281D, 1404F3A1C, 1404F41D7/431D ADDSS, 1404F3EE6 MULSS-arm of F0 target)
0x1425656C0 = 4.0       (4.0 leg: 1404F2E32 COMISS XMM7,[..]; 4.0-radius floor test)
0x14256AE1C = 1.0       (1.0 ramp top: 1404F3126 COMISS, 1404F436D-arm, 1404F4373 MOVSS fallback)
0x142588200 = 0.1       (int-noise rate: 1404F3A9D MULSS XMM0,[..] in the F1b probe)
0x142588210 = 10.0      (10.0 ceiling: 1404F2104 MOVSS, 1404F2452/2844 COMISS, 1404F3ED5 ADDSS-arm)
0x142588218 = -1.0      (sign select: 1404F382D-arm region / wrap arms)
0x14258C264 = 3.0       (3.0 near radius: 1404F391B SUBSS, 1404F3B15 MOVSS XMM6 (d<3.0 arm), 1404F407E COMISS, 1404F44C6 MULSS-arm)
0x142590D98 = 0.3       (state-gate weight: 1404F3C60 MOVSS XMM11 — bVar42-gated need weight input)
0x142591140 = 30.0      (30.0 scale: 1404F3EC1 MULSS XMM9,[..] in the F0-target rate arm)
0x142592228 = 0.25      (0.25 tick: 1404F4276 ADDSS XMM8,[..] gated on [RSP+0x28]==0)
0x14259222C = 5.0       (5.0 radius: 1404F2104 MOVSS, 1404F3F0E/3FE1/4023 MOVSS XMM6, 1404F2D15 COMISS)
0x142596E6C = 80.0      (80.0 cap: 1404F3688 MOVSS / 1404F3695 COMISS; 1404F1F5F region wrap arm)
0x142598578 = 0.2       (0.2 rate: 1404F183A MULSS, 1404F2460-region ADDSS-arm)
0x142598580 = 180.0     (wrap base: 1404F4226 COMISS XMM0,[..])
0x14259858C = 360.0     (wrap span: 1404F4243 MULSS XMM0,[..])
0x142598598 = -360.0    (wrap span neg: 1404F1F14 ADDSS-arm)
0x14259A760 = 35.0      (35.0 angle floor: 1404F38A3 MOVSS / 1404F38B3 COMISS)
0x14259A764 = 60.0      (60.0 angle gate: 1404F4259 COMISS XMM0,[..])
0x14259B89C = 0.4       (0.4 weight: 1404F4115 MOVSS XMM8,[..] — holder-leg weight input)
0x14259BDD0 = 0.69444   (zone-width rate: 1404F24D0 MOVSS XMM6,[..] DIVSS-arm)
0x14259BDDC = 1.5       (press-lane margin: 1404F12E3 MOVSS XMM6 (head latch test), 1404F37C3 MULSS-arm)
0x14259BDEC = 6.0       (6.0 class floor: 1404F296E MOVSS XMM11,[..])
0x14259BE0C = 15.0      (15.0 ceiling: 1404F22A7 MOVSS / 1404F2104-arm; 1404F381B SUBSS-arm)
0x14259BE10 = 16.0      (16.0 bar: 1404F3610 COMISS XMM0,[..])
0x14259BE1C = 20.0      (20.0 radius: 1404F2417 ADDSS XMM8,[..]; 1404F264A/2F17 MOVSS; 1404F437D COMISS-arm of ramp ceiling)
0x14259BE20 = 25.0      (25.0 gate: 1404F3B71 COMISS XMM3,[..])
0x14259BE2C = 40.0      (40.0 addend: 1404F23F9 ADDSS XMM6,[..])
0x14259BE38 = 55.0      (55.0 divisor: 1404F4101 DIVSS XMM6,[..])
0x14259BE40 = 57.29578  (180/pi: 1404F1FBB MOVSS XMM13 — atan2→deg scale)
0x14259BE4C = 70.0      (70.0 band: 1404F1ED0-region wrap gate)
0x14259BE5C = 90.0      (90.0 aim bias: 1404F2C54/3DE1/3F64 COMISS/MOVSS — aim-point arms)
0x14259BE84 = -10.0     (ramp divisor: 1404F4398 DIVSS XMM6,[..])
0x14259CE38 = 3.6       (3.6 rate: 1404F2642 MULSS, 1404F2F0F MULSS — reach-scale arms)
0x14259D60C = 9.0       (9.0 score floor: 1404F3A3F COMISS XMM10,[..]; also 1404F2F46 COMISS / 1404F3909 COMISS XMM10 (same word))
0x14259DA9C = 1.7       (1.7 latch gate: 1404F12D9 COMISS XMM0,[..])
0x1425A2670 = 400.0     (d² gate 20²: 1404F1353 COMISS XMM0,[..])
0x1425A3590 = 1.1       (1.1 scale: 1404F3575 MULSS XMM1,[..] in the F5-window arm)
0x1425A9464 = 36.0      (36.0 score span: 1404F3A4F MOVSS XMM0,[..])
0x1425A9BF0 = 130.0     (130.0 ceiling: 1404F40EE COMISS XMM6,[..])
0x1425AA1BC = 75.0      (75.0 floor: 1404F40DB MOVSS XMM0,[..])
0x1425AB128 = 1.25      (1.25 rate: 1404F345F MULSS XMM0,[..] — need-gate weight input)
0x1425C4890 = 0.0       (0.0 bar: 1404F3AE0 COMISS XMM0,[..] — F1b probe output gate)
0x1425C48A0 = 0.15      (0.15 rate: 1404F1200/2C6C MULSS — frame-window build)
0x1425C48D8 = 289.0     (d² gate 17²: 1404F18D0 COMISS XMM0,[..])
0x1425C48F0 = -0.3      (rate trim: 1404F3645 MULSS XMM0,[..] — reach-scale arm)
0x142572348 = 0.8       (0.8 window: 1404F331C MOVSS XMM14,[..] — F5-window scale)
0x1425795E4 = 2pi       (1404F1FB2/2506/2525/2855 MOVSS XMM12 — full-circle add arms)
0x14257A194 = 0.7       (0.7 scale: 1404F37AC MULSS XMM0,[..] — head need-scale arm)
```

Helpers (own decompiles, CONFIRMED shapes):
`FUN_1404F6110(x) = (long)(rnd_float()*x + 0.5)` (int-noise: 2 lines;
calls `FUN_141E532D0` = global-float-cell read).
`FUN_1404F4AB0(a,b,c) = a<=b ? 0 : c<=a ? 1 : (a-b)/(c-b)` (0..1 ramp: 11 lines).

## §1 The scoring region: register trace (CONFIRMED)

Scope: the per-candidate score `iVar13` built in the main loop and the
final compare at `0x1404F451C`. The function's 3424 lines hold many early-exit
arms (spine: team-helpers §6); only the score path is traced here.

### §1.1 Score variables (decompile names → asm homes)

- `iVar13` = running score (EBX through the tail: `ADD EBX,EAX` at
  0x1404F42E5/4306/4329/44F5; `MOV EBX,EDX`-family init at 0x1404F403F).
- `uVar33` = candidate accumulator (R14D/R14 slot id rides beside it;
  `LEA EBX,[RBX+RAX*0x4]` at 0x1404F41DF, `SHR EDX,0x1` = /3 counter folds).
- `uVar12` = counter-noise term (`(counter*10)/100`-family at 0x1404F4033-41,
  `MUL ECX / SHR EBX,0x5` = /32 fold at 0x1404F403D-41).
- `iVar37` = incumbent best (entry `local_110` low half; `CMP R15D,EBX` at
  0x1404F451C decides).
- `iVar11` = elapsed-frame count (`FUN_140A4D660` head value; the
  `iVar13 < iVar37` / `iVar11 < iVar14` comparisons).
- Floats: XMM6 = radius leg, XMM7 = distance leg, XMM8 = weight accumulator
  (the `0.4`-seeded holder-leg weight, §1.3), XMM10/XMM12 = probe baselines
  (`[RBP-0x70]`/`[RBP+0x10]` snapshot words), XMM13 = lag level.

### §1.2 Score formula (per candidate slot uVar39, CONFIRMED dataflow)

```
score = base                                          # EBX init from 0x1404F403F chain (counter folds)
      + F6110(XMM11)            # 0x1404F3FFC CALL: (long)(rnd*XMM11 + 0.5); XMM11 = 0.3-word rate cell
      + F6110(XMM13)            # lag-level int-noise (0x1404F3954 CALL on the bVar8 arm)
      + 6CF10(slot)             # 0x1404F42AB/42CF/42E0 CALL: slot need value (A6CF10 family)
      + [local_res18 ? counter : counter/3]           # 0x1404F42C8 ADD EBX,EDX (/3 fold) or 0x1404F42F5 *4 arm
      + counter/3               # 0x1404F4308 CALL 141E532E0; 0x1404F4329 ADD EBX,EDX
      + [T+0x78==slot ? 6CF10 + (bVar30 ? +counter : +counter*4) + counter/3 : 0]
                                # 0x1404F443B-44F5 holder-bonus arm (see §1.4)
      + (long)(counter * ramp)  # 0x1404F44E3-44F5: counter × F4AB0-ramp (XMM6), CVTTSS2SI
      - [bVar7-arm: (long)(rnd * XMM8)]               # 0x1404F4284 MULSS / 0x1404F4289 CVTTSS2SI / NEG
                                # XMM8 = 0.4 + lane-rate term (+ 0.25 when [RSP+0x28] set)
```

Ramp (CONFIRMED, `d_4F4AB0.c`): `F4AB0(a,b,c)` clamps a 0..1 ramp used twice:
- 0x1404F3EAE: `F4AB0(XMM2=rnd*XMM6+XMM11-upto-30+10, XMM7, XMM6)` — gate ramp on the F0-target arm.
- 0x1404F3F09: `F4AB0(XMM0=counter*[RSP+0x58]-as-float, XMM6=5.0, XMM0prev=... +3.0)` —
  `score += (long)(counter * that)` then `+ 5.0`, `+ 3.0` arm included.
- 0x1404F3C60-region: `XMM11 = 0.3`-word rate; the `F4AB0`-equivalents at
  0x1404F4086-4101 (`3.0`-floor / `5.0`-ceiling / `1.0`-top / `55.0` divisor)
  and 0x1404F40DB-4101 (`75.0`-floor / `130.0`-ceiling / `55.0` divisor) and
  0x1404F437D-4398 (`20.0`-ceiling / `-10.0` divisor) are the same ramp shape
  with different (lo,hi) — see §1.3 for which leg each weights.

Which term each constant weights (each paired with its reading instruction):

| constant | instruction | term it weights |
|---|---|---|
| 9.0 [0x14259D60C] | 0x1404F3A3F COMISS XMM10 | score floor: `XMM10 > 9.0` required (candidate admitted) |
| 36.0 [0x1425A9464] | 0x1404F3A4F MOVSS XMM0 | score span: `XMM0=36.0; 36.0 <= XMM11` required (span gate) |
| 0.1 [0x142588200] | 0x1404F3A9D MULSS XMM0 | int-noise rate: `(long)(rnd*0.1 + 0.5)` probe-slot draw (F1b arm) |
| 25.0 [0x14259BE20] | 0x1404F3B71 COMISS XMM3 | range gate: `XMM3 <= 25.0` required (near-man arm) |
| 3.0 [0x14258C264] | 0x1404F3B15 MOVSS XMM6 | near radius: `d < 3.0` admits (F1b probe output gate) |
| 3.0 (same word) | 0x1404F407E COMISS XMM0 | near radius: `3.0 < d` → far arm (radius-leg ramp floor) |
| 3.0 (same word) | 0x1404F44C6 MULSS XMM0 | int-noise weight: `(long)(rnd*3.0 + 0.5)` on the holder arm |
| 0.0 [0x1425C4890] | 0x1404F3AE0 COMISS XMM0 | probe bar: `0.0 <= probeOut` required (F1b output gate) |
| 0.35 [0x14259D5C8] | decompile `fVar56*0.35`-family arm (mark-slot §0 pair) | need-chain scale (head need arm; INF detail — same word as mark-slot's x0.35 arm) |
| 0.7 [0x14257A194] | 0x1404F37AC MULSS XMM0 | head need-scale: `rnd <= iVar11*(0.7-arm)` gate (need vs elapsed) |
| 1.5 [0x14259BDDC] | 0x1404F12E3 MOVSS XMM6 (head latch) | press-lane margin (head latch test; same word as 0x1404F37C3 MULSS-arm scale) |
| 0.5 [0x142565600] | 0x1404F40A9 MULSS-arm / 0x1404F39A6 ADDSS | int-noise addend + radius-leg `×0.5` scale |
| 2.0 [0x142565604] | 0x1404F171A ADDSS (score gate) | lag addend: `fVar50+2.0 <= fVar44` far-man switch arm |
| 2.0 (same word) | 0x1404F3EE6 MULSS XMM6 | target-rate arm: `counter-draw × 2.0` in F0-target build |
| 4.0 [0x1425656C0] | 0x1404F2E32 COMISS XMM7 | 4.0-radius floor test (radius-leg floor) |
| 1.0 [0x14256AE1C] | 0x1404F3126 COMISS / 0x1404F4373 MOVSS | ramp tops (F4AB0-equivalent ceiling; 0x1404F4504 tie ADDSS-arm) |
| 10.0 [0x142588210] | 0x1404F2104 MOVSS / 0x1404F2452 COMISS | 10.0 ceiling (radius-leg ceiling; F0-target `+10.0` arm at 0x1404F3ED5) |
| 15.0 [0x14259BE0C] | 0x1404F1758 MOVSS | 15.0 ceiling (head radius leg; 0x1404F381B SUBSS-arm) |
| 5.0 [0x14259222C] | 0x1404F3F0E/3FE1/4023 MOVSS XMM6 | ramp ceiling 5.0 (score ramp; 0x1404F2D15 COMISS-arm) |
| 6.0 [0x14259BDEC] | 0x1404F296E MOVSS XMM11 | 6.0 class floor (class-gate arm) |
| 20.0 [0x14259BE1C] | 0x1404F2417 ADDSS / 0x1404F437D COMISS | 20 m radius arm + ramp ceiling (0x1404F437D-4398 ramp) |
| 30.0 [0x142591140] | 0x1404F3EC1 MULSS XMM9 | target-rate scale: `F0val × 30.0 + 10.0` |
| 0.3 [0x142590D98] | 0x1404F3C60 MOVSS XMM11 | state-gate weight input (bVar42 arm) |
| 0.4 [0x14259B89C] | 0x1404F4115 MOVSS XMM8 | holder-leg weight seed (XMM8 init when T+0x78 ≠ slot) |
| 0.25 [0x142592228] | 0x1404F4276 ADDSS XMM8 | weight tick: `XMM8 += 0.25` when [RSP+0x28] set |
| 0.8 [0x142572348] | 0x1404F331C MOVSS XMM14 | F5-window scale (window arm) |
| 1.1 [0x1425A3590] | 0x1404F3575 MULSS XMM1 | F5-window depth scale |
| 1.25 [0x1425AB128] | 0x1404F345F MULSS XMM0 | need-gate weight (need vs 6CF10 arm) |
| 3.6 [0x14259CE38] | 0x1404F2642/2F0F MULSS XMM0 | reach scale (reach-vs-20.0 arms) |
| 16.0 [0x14259BE10] | 0x1404F3610 COMISS | 16.0 bar (reach-gate arm) |
| 40.0 [0x14259BE2C] | 0x1404F23F9 ADDSS XMM6 | 40.0 addend (range-leg arm) |
| 55.0 [0x14259BE38] | 0x1404F4101 DIVSS XMM6 | ramp divisor (75→130 ramp) |
| -10.0 [0x14259BE84] | 0x1404F4398 DIVSS XMM6 | ramp divisor (20.0-ceiling ramp) |
| 75.0 [0x1425AA1BC] | 0x1404F40DB MOVSS XMM0 | ramp floor (angle-leg ramp) |
| 130.0 [0x1425A9BF0] | 0x1404F40EE COMISS XMM6 | ramp ceiling (angle-leg ramp) |
| 90.0 [0x14259BE5C] | 0x1404F3F64 MOVSS / 0x1404F2C54 COMISS | aim bias 90° (aim-point arms) |
| 70.0 [0x14259BE4C] | 0x1404F1ED0-region | 70 m band gate (wrap arm) |
| 80.0 [0x142596E6C] | 0x1404F3688 MOVSS / 0x1404F3695 COMISS | 80.0 cap (reach-gate arm) |
| 60.0 [0x14259A764] | 0x1404F4259 COMISS XMM0 | 60° angle gate (angle-leg admit) |
| 35.0 [0x14259A760] | 0x1404F38A3 MOVSS / 0x1404F38B3 COMISS | 35° angle floor (mid-arm) |
| 180.0 [0x142598580] | 0x1404F4226 COMISS | wrap base 180° |
| 360.0 [0x14259858C] | 0x1404F4243 MULSS | wrap span 360° |
| 57.29578 [0x14259BE40] | 0x1404F1FBB MOVSS | atan2→degrees scale |
| 2pi [0x1425795E4] | 0x1404F1FB2 MOVSS | full-circle add |
| 0.2 [0x142598578] | 0x1404F183A MULSS | 0.2 rate (frame-window build) |
| 0.15 [0x1425C48A0] | 0x1404F1200 MULSS | 0.15 rate (frame-window build) |
| 0.69444 [0x14259BDD0] | 0x1404F24D0 MOVSS | zone-width rate (DIVSS-arm) |
| 400.0 [0x1425A2670] | 0x1404F1353 COMISS | d² gate 20² (head latch: far-man arm) |
| 289.0 [0x1425C48D8] | 0x1404F18D0 COMISS | d² gate 17² (near-man arm) |
| 1.7 [0x14259DA9C] | 0x1404F12D9 COMISS | 1.7 latch gate (head) |
| -0.3 [0x1425C48F0] | 0x1404F3645 MULSS | reach-trim rate |
| -1.0 [0x142588218] | wrap sign-select arms | sign select |
| -360.0 [0x142598598] | 0x1404F1F14 ADDSS-arm | wrap span (neg) |

### §1.3 The three ramp legs (F4AB0 shape, CONFIRMED at asm level)

Each leg is `lo < d ? (d >= hi ? top : (d-lo)/(hi-lo)) : 0`-with-top where
noted; the decompile shows them as COMISS/JC cascades (F4AB0 inlined by the
compiler at these sites, called as a function at 0x1404F3EAE/3F09):

1. Radius leg (0x1404F407E-40B1): `d = sqrt(dx²+dy²)` via `1415A1780`
   (0x1404F4072 CALL); `3.0 < d` else 0; `d >= 5.0 → 1.0` else
   `(d-3.0)*0.5`. Inputs: man pos `[RAX]/[RAX+8]` minus `[RBP-0x48]/[RBP-0x40]`
   (ball-snapshot words).
2. Angle leg (0x1404F40DB-4101): `a = 43A020(...)` (0x1404F40D3 CALL);
   `75.0 < a` else 0; `a >= 130.0 → 1.0` else `(a-75.0)/55.0`.
3. Holder-need leg (0x1404F435C-4398): `d = 1415A1780(...)` (0x1404F435C CALL);
   `10.0 < d` (0x1404F436D COMISS XMM9,XMM6) else `1.0`-fallback
   (0x1404F4373 MOVSS XMM6,1.0); `d >= 20.0 → 0.0` else `(d-20.0)/-10.0`.
   Extra input: `*(rec+0x5D0)==5` gate (0x1404F43A8 CMP) + `FUN_1404C8E00`
   carrier-need call (0x1404F43D5) on the 5-arm.

Final blend (0x1404F4267-4289, CONFIRMED):
`XMM8 = 0.4 + laneTerm; XMM1 = (laneRateOut)*(radiusLeg)*(angleLeg);
XMM8 += XMM1 (+ 0.25 if [RSP+0x28]); score -= (long)(rnd*XMM8)` (NEG EBX).
So the weight accumulator XMM8 is a *penalty*: larger XMM8 lowers the score.

### §1.4 Comparison, winner, ties (CONFIRMED)

- Incumbent: `iVar37` (entry: `local_110` low half = `uVar39==0` ? 1 : 0...
  actually `(uint)(uVar39==0)`; loop keeps the best).
- Per candidate: `iVar13 = uVar33 + uVar12` (0x1404F44F5: `ADD EBX,EAX`;
  EBX=uVar33-accumulator, EAX=uVar12-counter-noise).
- Take-over (0x1404F451C-4528):
  `CMP R15D,EBX; JLE skip; TEST SIL,SIL; JNZ skip; TEST AL,AL; JZ store` —
  i.e. **take over iff `score > best` strictly AND `!bVar41` AND `bVar40`**,
  where `bVar40 = (uVar39==param_2) || (3.0 + 1.0 < fVar50)` i.e.
  `fVar50 > 4.0` tie-extra arm (0x1404F44F7-4519:
  `ADDSS XMM12,1.0; COMISS XMM13,XMM12; CMOVA EAX,EDX`).
- **Tie handling: strict `>` — ties keep the incumbent.** No equal-arm
  re-store exists. First-best-wins on equal scores. CONFIRMED by the
  `JLE skip` (not `JL`) at 0x1404F451F.
- Fallback (0x1404F452E-464A): if no take-over (`score <= best` or vetoes)
  and `param_2 != uVar39`, a `(long)(rnd*XMM11+0.5) < iVar11` retest
  (0x1404F4537-454E) may still return 0xFF after the record/geometry
  cross-check (`A3E190` + `A3E900` + `A62EA0` + role bytes `+0x1==7`,
  `+0x38==4`, then `d_new < d_old → 0xFF` at 0x1404F4640-4644).

## §2 Output: what lands in T+0x78 and when (CONFIRMED)

- Signature: `FUN_1404F0E50(Y, iVar1) → slot id` (u32; `0xFF` = none).
  Sole caller `0x1404FB70E` in `FUN_1404FB380` (bridge xrefs CONFIRMED, prior).
- Stores (all in-caller 4FB380 / tail of F0E50 itself):
  - Fast arm: `T+0x6D00 := uVar12; return uVar12` (holder directly reused;
    decompile l.~137; no F0E50 call on this path).
  - Mid arms: `T+0x6D00 := uVar39; return uVar39` (decompile
    `LAB_1404F26F2`, `LAB_1404F2DA8`-chain; single-candidate resolves).
  - Scored arm: `T+0x6D00 := uVar39; return uVar39` (0x1404F4677
    `MOV [RSI+0x6D00],R14D`), then 4FB380's `T+0x78 := F0E50(...)`.
    (Note: F0E50 writes `param_1+0x6D00` = the Y+0x6D00 word, NOT T+0x78
    itself; the `T+0x78 :=` store is the caller's at 4FB380 l.138.)
  - Reject arms: `return 0xFF` (head `iVar11==5` → `return param_2`;
    `LAB_1404F3B7E` sets `[param_1+0x6D00]=uVar39 + return 0xFF`;
    `LAB_1404F4653`-fallthrough validates `uVar39<0x16` + on-pitch
    `[+0x1748]` else 0xFF).
- When: only on the `MI+0x116C == 3` arm of 4FB2A0→4FB380 (lane-leaves §4),
  after the holder/count/tree gates pass. `param_2` (iVar1) = the incoming
  candidate from 4FB380's decision tree.

## §3 Inputs: GF 1:1 or exactly-what-is-needed (read-only grep over `src/`, origin/main)

| PES input | GF carries 1:1 | Exactly what is needed |
|---|---|---|
| T+0x78 id word | PARTLY: `kickMan/kickValid` stand-in (`teamAIcontroller.cpp:2446-2448`, ponytail names the writer — now closable) | per-side dword id + `MI+0x116C/0x11D4/0x1558/0x11CC/0x1775` words |
| attack sign T+0x254 | YES: `Team::GetSide()` (`team.cpp:295`: id0→-1 else +1; used as `±pitchHalfW` attack direction through `teamAIcontroller/elizacontroller/humanoid`) | 1:1 — sign only; PES `T+0x254` is the same ±1 |
| man x/y P+0x55C/0x564 | YES: `Player::GetPosition().coords[0/1]` (pervasive) | 1:1 |
| ball x/y MA+0x2482C/0x24834 | YES: `match->GetBall()->Predict(0)` (pervasive) | 1:1 |
| on-pitch [+0x1748] | YES: `GetActivePlayers()` squad membership (`match.cpp:1606`, `teamAIcontroller` filters) | 1:1 (bool per slot) |
| play-state MI+0x116C | PARTLY: `Match::IsInPlay()` + `GetMatchPhase()` (`match.hpp:197`, `e_MatchPhase` in `gamedefines.hpp`) — the 1/2/3 phase values have no 1:1 enum | new word or map: 116C==1 in-play / ==2 / ==3 arms |
| record table A3E190 ring | NO (no 800-byte ring; `MentalImage` predictions are the functional analogue, not the struct) | new: per-team record ring (`+0x0` kind, `+0x1C` class, `+0x20` slot, `+0x40/+0x44` floats, `+0x10` frame) |
| slot records A48910/48A10/489F0 (+0x5D0 action, +0x1430/0x1438 counters, +0x55C/0x564 pos) | PARTLY: `PlayerData`/formation role + `GetTimeNeededToGetToBall_ms()` cover need/role fragments; `+0x5D0` action word and `+0x1430/0x1438` counters have no carrier | new per-slot: actionWord (+0x5D0/0x5D4/+0x6B8 triple, mark-slot §2), two int counters |
| MI frame +0x1198, rec +0x10 | PARTLY: `GetActualTime_ms()` (ms clock; PES frame = 1/54 s ticks) | frame counter in PES ticks (÷54 ms) for the `(frame-recFrame) vs (long)(rnd*rate+0.5)` windows |
| global float cell 141E532D0 | NO (`pesrandom.hpp` is per-stream RNG, not this cell) | new: one global float cell + counter cell 141E532E0 (tick-float 54.0 default per team-helpers §3.1) |
| sqrt dispatch 1415A1780 | YES: `sqrtf` (libm; shape CONFIRMED) | 1:1 |
| 6CF10/6CF50/6CF70 need fns | PARTLY: `GetTimeNeededToGetToBall_ms()` is the need analogue (`playercontroller.cpp:316`) | need value per slot per record (float) |
| 6E150/5E0/340/050/4B0/580 class fns | NO | new: per-slot class bytes (the `+0x1` class word family) |
| 71540 side table | NO | new: side-keyed record select (same family as lane-leaves A71500) |
| 76240 commit target | NO | new: 32-byte target block (team-helpers Named 3 family) |
| 57800/57710/57720 aim recs | NO | new: aim-record selects (or `// ponytail:` stub returning "no record" as in T9 port) |
| 439E50/43A0A0/43A020 heading | PARTLY: `GetDirectionVec()`/`GetAngle2D()` cover heading reads | wrap-180/360 helpers (pure; port verbatim) |
| 4F6110 int-noise | NO | pure: `(long)(rnd(cell)*x + 0.5)` — ports verbatim once the cell lands |
| 4F4AB0 ramp | YES (pure) | `(a<=b)?0:(c<=a)?1:(a-b)/(c-b)` — no input needed |
| T+0x8920 bitmask, T+0x8D7C/80 ids, T+0x4C00 class | NO | new words (team-helpers Named 9/10) — read on the tail arms |
| T+0x6D00/0x6CFC/0x6D04 words | NO | new: `6D00` id word (F0E50's own store), `6CFC` id + `6D04` short counter (score-loop state) |

## §4 Port spec (replaces the F0E50 `// ponytail:` leaf)

```cpp
// Score: integer accumulator. best init from (uVar39==0); per candidate:
//   score = folds + F6110(rate) + F6110(lag) + Need(slot)
//         + cnt/3 + cnt (+ holder bonus when T+0x78==slot:
//             + Need(slot) + (bVar30 ? cnt : 4*cnt) + cnt/3)
//         + (long)(cnt * RampF4AB0(...)) - [(long)(rnd * w)]
//   where w = 0.4 + radiusLeg*angleLeg*laneRate (+0.25 if late-window bit)
//   radiusLeg = d<3?0 : d>=5?1 : (d-3)*0.5          // 3.0/5.0/0.5
//   angleLeg  = a<75?0 : a>=130?1 : (a-75)/55       // 75.0/130.0/55.0
//   needLeg   = d<10?1 : d>=20?0 : (d-20)/-10       // 10.0/20.0/-10.0, gated on actionWord==5 + carrier-need
// Take-over iff score > best (STRICT; ties keep incumbent) && !veto && tieExtra
//   tieExtra = (slot==param_2) || (lagLevel + 1.0 < lagOther)   // 1.0 [0x14256AE1C]
// Gates (all must pass for the scored arm): probeOut in [0.0, 3.0-radius...],
//   XMM10 > 9.0, span 36.0-check, range <= 25.0, angle-leg < 60° admit path,
//   d²-vs-400/289 head gates, 1.7-latch gate, class gates (6E150 family).
// Output: T+0x6D00 := winner (F0E50 itself); caller 4FB380: T+0x78 := winner or 0xFF.
// FUN_1404F6110(x) = (long)(GlobalFloat()*x + 0.5)              // port verbatim
// FUN_1404F4AB0(a,b,c) = a<=b?0 : c<=a?1 : (a-b)/(c-b)          // port verbatim
```

Order inside F0E50 (CONFIRMED): 4D660-5-gate → 4F50F0 fast arm →
param_2-seed arm → class-filter scan → geometry preamble (wrap-180/360) →
candidate loop (early-return arms + score build) → take-over compare →
record/geometry cross-check → `T+0x6D00 := winner` / `0xFF`.

## §5 Evidence ledger

| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| HT-01 | §0 floats (49) | CONFIRMED (script-read) | checker float rows; bits pasted |
| HT-02 | each constant ↔ reading instruction | CONFIRMED | checker opsites (26); table §1.2 |
| HT-03 | F6110 = (long)(rnd*x+0.5) | CONFIRMED | `d_4F6110.c` (2 lines) + 0x1404F3954/3FFC CALL sites |
| HT-04 | F4AB0 = 0..1 ramp | CONFIRMED | `d_4F4AB0.c` (11 lines) + 0x1404F3EAE/3F09 CALL sites |
| HT-05 | three ramp legs (floors/ceilings/divisors) | CONFIRMED | asm 0x1404F407E-40B1, 0x1404F40DB-4101, 0x1404F435C-4398 (COMISS/JC cascades + CALL 1415A1780) |
| HT-06 | score accumulator shape + NEG penalty | CONFIRMED | asm 0x1404F4033-44F5 (ADD EBX,EAX chain, MULSS/NEG at 0x1404F4284-428D) |
| HT-07 | strict-> take-over, ties keep incumbent | CONFIRMED | asm 0x1404F451C CMP + 0x1404F451F JLE skip |
| HT-08 | tie-extra `+1.0` arm | CONFIRMED | asm 0x1404F4504 ADDSS + 0x1404F4515 CMOVA |
| HT-09 | output `T+0x6D00 := winner`, caller sets T+0x78 | CONFIRMED | asm 0x1404F4677 MOV [RSI+0x6D00],R14D; lane-leaves §4 caller store |
| HT-10 | GF carriage table | CONFIRMED (grep) | §3 paths (origin/main 26-10-26 checkout) |
| HT-11 | checker + mutations | CONFIRMED | `check_holder_tree.py`: RESULT OK; `--selftest` OK (8/8 caught) |
| HT-INF-01 | 0.35/0.7 need-chain scales | INF detail | decompile `*0.35`-family arm; 0x1404F37AC pairs 0.7; upstream need-record layout OPEN |
| HT-OPEN-01 | A3E190-ring / slot-record / class-byte layouts | OPEN | blocks exact port; word + writer path ported independently |

## §6 Open items (for Main)

1. Record-ring + slot-record + class-byte layouts (HT-OPEN-01) — blocks exact
   F0E50 port; the T+0x78 word + writer chain + score formula above are done.
2. `FUN_140A627E0` window, `A57800`-family interiors — predicate-level only.
3. Live confirmation (read-only): `T+0x6D00` store cadence during a match.
