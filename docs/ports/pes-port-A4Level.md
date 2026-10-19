# A4Level: the one-touch team level (G3) and the attack-heading convention

> **Main audit 19-10-26 (run 2, `4126007`):** `0x140698EDA` subss, `0x140698EE9` store +0x170, `0x140698F1D` call 0x140A8F820, `0x1421199B8` xor edi,edi, `0x142119CB6` ja: match. Citation slips: `0x14212AF86` is the `call 0x142128C10` (the `mov rcx,rdi` precedes it); `0x142119929` is `addss xmm0,[0x142588210]` (10.0, the B3C<=B58+10 arm), not the K+0x40 read. OneTwo now dies at G11 (plan heading stand-in) and G12.

Branch `A4Level` from `origin/A4LaneInputs` `b36a845`. Worktree `<workdrive>/gf-A4Level`.
Commits (pushed): `e296f1b` (level + attack-heading convention), `a1882b9` (the aim-vs-facing frame).
Files: `pescarrier.{hpp,cpp}`, `elizacontroller.cpp` (carrier path), `tests/sim/pes_carrier_test.cpp`.
Not touched: team.cpp / teamAIcontroller.* / pesteamtick.* (RoleArrays chain).
Image `pes_full.bin`, base `0x140000000`; file offset = VA - 0x140000000.

Legend: **CONFIRMED** = raw asm or a script read of the image this session. **INF** = inference.
**OPEN** = blocked, next step named.

## 0. Constants (script output, `struct.unpack`/`od` at VA-0x140000000; each paired with its instruction)

| VA | bytes | value | paired instruction this report |
|---|---|---|---|
| `0x14810F4E0` | `D1 CA 9C 48` | 0x489CCAD1 | `0x15B5D8372 MOV EAX,[RIP-0x134C8E98]` |
| `0x15B5D8378` | `35 D3 CA 9C 48` | XOR imm 0x489CCAD3 | `0x15B5D8378 XOR EAX,0x489CCAD3` |
| `0x15B5D837D` | `89 C2` | the switch's shared arm | `0x15B5D837D MOV EDX,EAX` (all 7 table entries land here) |
| `0x141FC3838` | 7 x `7D 83 5D 1B` | 7 x 0x1B5D837D | the switch table read at `0x15B5D8364 MOV R8D,[RCX+RDX*4+0x1FC3838]` |
| `0x140A602B0` | `6B C2 64` | IMUL EAX,EDX,0x64 (x100) | `0x140A602B0` |
| `0x140A602B5` | `3D BC 02 00 00` | CMP EAX,0x2BC (700) | `0x140A602B5` |
| `0x14259BE40` | `0x42652EE0` | 57.2957763671875 (180/pi) | `0x14045DFBD MULSS XMM0,[0x14259BE40]` |
| `0x1425656D0` | `0x80000000` | -0.0 (the sign-bit flip) | `0x14045DF70` `(uint)*pfVar2 ^ DAT_1425656d0` |
| `0x1425795E4` | `0x40C90FDB` | 6.2831854820251465 (2pi deg) | `0x14045DFB5 ADDSS XMM0,[0x1425795E4]` |

The masked default the setter falls back to: the global at `0x14810F4E0` (0x489CCAD1) XOR the
immediate of the `XOR EAX,imm32` at `0x15B5D8378` (0x489CCAD3) = **2**.

## 1. Team level (item 1): CLOSED to the value

Reader/gates (re-confirmed, lane-inputs §2 unchanged): `0x140A40BD0` = `+0xA29C ? 0 : +0xA294`;
`0x140A60360` = `*p/100 < arg` (magic `0x51EB851F`, `SAR 5`); `0x140A60340` = `*p/100`. G3 = `level/100 >= 2`.

### 1.1 The setter's value (CONFIRMED, raw)

Per-side writer `0x141FB9FB0` at `0x141FBA47D`:

```
141fba101  MOV dword ptr [RBP-0x60],0x2        ; the default level class 2
...
141fba473  MOV R8D,[RBP+0x58]                  ; the side index
141fba477  MOV EDX,[RBP-0x60]                  ; the value
141fba47a  MOV RCX,R15
141fba47d  CALL 0x140A424E0                    ; T(side)+0xA294 = value
141fba482  LEA RDX,[RBP+0x58]
141fba486  MOV RCX,R13
141fba489  CALL 0x1414C1950                    ; *(u8*)(side + 0x9200 + T18)
141fba48e  MOVZX EDX,AL
141fba491  MOV RCX,R15
141fba494  CALL 0x140A428B0                    ; T(side)+0xA29C = that byte
```

The value path (same function): the class is read from a match-setting record `lVar14`
(`*(longlong*)(this+0x1B070)`, filled by `thunk_EXT_FUN_15B5D8340(local_88, matchSetup)`);
when the setup object at `+0x1AFF0` is present and valid the class is re-stored through
`thunk_EXT_FUN_144286B70` = `if (value/100 < 7) *obj = value` (the 700 cap, `0x144286B70`:
`call 0xA60340; cmp $0x7,%eax; jge skip`), where the value comes from `FUN_140A602B0`:

```
0x140A602B0  IMUL EAX,EDX,0x64    ; class x 100
0x140A602B5  CMP EAX,0x2BC        ; cap 700
0x140A602BC  MOV dword ptr [RCX],0x2BC
0x140A602C8  TEST EAX,EAX ; CMOVS  ; floor 0
```

so **level = clamp(class x 100, 0, 700)**. The class is the switch selector `*(int*)(setup+0x18)`
mapped through `0x1B5D8340`:

```
0x1B5D8340  TEST RCX,RCX ; JE ret
0x1B5D8350  CALL 0x1B86CD0         ; = *(int*)(src+0x18)   (0x1B86CD0: MOV EAX,[RCX+0x18]; RET)
0x1B5D8355  CMP EAX,0x6 ; JA default
0x1B5D8364  MOV R8D,[RCX+RDX*4+0x1FC3838] ; JMP *R8   -> all 7 entries = 0x1B5D837D
default:    0x1B5D8372  MOV EAX,[RIP-0x134C8E98] ; XOR EAX,0x489CCAD3   (= 2)
            0x1B5D8384  CALL 0xA602B0 ; CALL 0x144286B70
```

The 7-entry table `0x141FC3838` is uniform (`7 x 0x1B5D837D`), so the switch is a **no-op**: every
class, and the out-of-range default, lands on the same arm. The stored value is therefore always
`clamp(class x 100, 0, 700)`, with class = the setup selector and class 2 (level 200) when the
selector is outside 0..6. A side whose `0x1414C1950` byte is set gets 500 (`FUN_140A602B0(&v,5)`).

So the value is a **per-team level class (0..7), stored x100**, from the match-setting record's
selector; it is not team strength, not the attack level `T+0xB8A4`, not a tactics byte.
INF(low): the selector is the match's team-level/difficulty setting. The live observation in
`pesdefshape.cpp` ("PES's live teams were both level 2") matches the class-2 default.

### 1.2 GF port

GF has no match-setting selector and no per-team level. The stock default is read from the install
through the same bytes the writer reads (no transcription): class = `ReadVa(0x14810F4E0)` XOR the
immediate at `0x15B5D8378`; scale and cap = the immediates at `0x140A602B0` / `0x140A602B5`; the
value = `clamp(class x 100, 0, cap)` = 200. That value lands on the existing named read
(`CarrierConstants::teamLevelRaw` -> `TouchInput.teamLevelRaw`), replacing the 0 stand-in; the zero
flag stays false (GF has no `+0x9200` byte). The `// ponytail:` at the field names the PES writer
`0x1B5D8340` (VA `0x15B5D8340`) and the `+0x18` selector so the settings system, when it exists,
removes it.

## 2. Attack heading (item 2): CLOSED

### 2.1 The value PES stores at T+0x250 (CONFIRMED, raw)

`T+0x250` has exactly one writer. The `MOVSS [reg+disp32],xmm` scan for disp 0x250 found
`0x140A42480` = `*(float*)(p+0x250) = xmm1`, whose only caller is `0x1404FEBD0`:

```
1404febb0  MOV RDX,[RSI+0x6D50]          ; T = the side object
1404febba  MOVZX EDX,byte ptr [RDX+0x254] ; the attack sign (+/-1)
1404febc1  CALL 0x140A71520
1404febcd  MOVAPS XMM1,XMM0
1404febd0  CALL 0x140A42480               ; T+0x250 = it
```

`0x140A71520` is a two-way select: `CMP DL,1; JNZ; MOVSS XMM0,[RCX+0x228D8]; RET; else [RCX+0x228D4]`
— sign +1 gives `+0x228D8`, sign -1 gives `+0x228D4`. Both are written by `0x14045DF70`:

```
+0x228D4 = bearing( -halfLen, 0 ) = 270   ; the xor 0x80000000 flips the sign of *pfVar2
+0x228D8 = bearing( +halfLen, 0 ) =  90   ; *pfVar2 = *(float*)(*(X+0x10)) = the pitch half-length
```

with `bearing = atan2(dx, dz)` x 180/pi wrapped (`0x140A92D40` x `0x14259BE40`). So

**T+0x250 = 90 for a team attacking +x, 270 for -x**, in PES's bearing convention
(0 = +z, 90 = +x). This is exactly what the curve call site already passes
(`attackSign < 0 ? 270 : 90`), and it is what the chooser's `0/180` gets wrong.

### 2.2 Readers (all take it as a bearing)

- `0x140A8F7C0` advances a point along it: `x += d*sin(theta)`, `z += d*cos(theta)`
  (`CALL 0x140A92C80(theta, d)`, then the same at `theta + 0x14259BE5C` = 90). CONFIRMED.
- `0x14043A0A0` = wrap180 of the difference of two bearings. CONFIRMED.
- lane theta `wrap360(T(side)+0x250)` (lane-structs §3.2, `0x1421291C6`); the carrier snapshot
  `C[4] = wrap360(T(side)+0x250)` (lane-core §7); defensive-shape §15.13.2's `receiver heading
  T+0x250 + 180`. All bearings.

### 2.3 GF sites (the one convention)

GF's `pes::PesBearingDeg(fromX, fromY, toX, toY)` (`pesdodge.hpp`) is that convention
("0 deg = +GF y (PES's +z), 90 deg = +GF x"), and `pescarrier.cpp`'s local `BearingDeg(a,b)` is the
same. The carrier path feeds the units through:

- the attack heading: `attackSign < 0 ? 180 : 0` (`elizacontroller.cpp` 2125, 2295, 2407, 2514) — wrong;
  the curve site's `270 : 90` (2753) — right;
- the facing/heading: `facingTheta/headingTheta = atan2(x, y)` (2706-2711) — right;
  `facingAngle = atan2(-y, -x)` (2293, 2406) and `velHeading_deg` (2525) — a mirrored frame
  (`270 - bearing`), the class A3States fixed;
- the pass direction: `bearing = atan2(dz, dx)` (2404) — a math angle, not a bearing.

Fix: one helper, `pes::AttackAngleDeg(attackSign)` = the PES bearing of the attack direction
(`PesBearingDeg(0, 0, sign, 0)`), used by every attack-heading consumer, and the facing/velocity/pass
sites expressed through the existing `PesBearingDeg`. No magic 90/270, one convention.

## 3. G4's team mode (run 2): the carrier's Intention, wired to the one-touch gate

`0x141052E30(p) = *(u32*)(p+8)`, and the object is **`bp+0x32B0`, the BallPlayer record's image
state**, not a team-AI field (S1Watch round 3, `docs/re/pes-re-team-mode.md` §1; the reader is called
with `param_1+0x32B0` by the driver `FUN_14210F110`). The one-touch predicate `0x142128C10` reads the
same object: its `param_1[6]` is the caller's `RDI`, and the OneTwo caller passes `MOV RCX,RDI` at
`0x14212AF86` with `RDI = R8` at the entry (`0x14212AEBF`), the carrier's own snapshot (its `+0x10`
is `C[4]`, lane-core §7). CONFIRMED raw this run.

The writer `FUN_1421197B0` is **already ported** as `pes::NextIntention` (`pescarrier.cpp`), from the
same decode (report 5 / Dribble inventory §2): 4 absorbing, the shoot-zone angle test, 3, 2, then the
corridor block. Verified against the raw asm this run (`0x142119862..0x142119CD2`): the stores are
`MOV EDI,0x4` (0x142119868, 0x1421198FC), `0x3` (0x14211993A), `0x2` (0x142119956, 0x142119AD9),
`0x1` (0x142119CB8), and **0** — `XOR EDI,EDI` at `0x1421199B8` survives to the store at
`0x142119CD2` when the corridor's own gate fails (`JA 0x142119CBD` at `0x142119CB6` skips the `MOV
EDI,0x1`). So the writer *can* store 0; the existing port's `corridorState1 ? 1 : 0` is correct.

What was missing was the **controller wiring**: both consumers passed the neutral stand-in
`teamMode = 2` (`elizacontroller.cpp`), which shut G4 for every carrier (the one-touch and the Safety
gate; the chooser read the same stand-in). They now pass the carrier's Intention, the ratchet the
block above writes:

- `touch.teamMode = intention` (G4's `== 3 || == 4`, and G8's increment at `== 2 || == 3`);
- `sf.teamMode = intention` (the Safety gate's `2 < mode` ceiling arm);
- the `attackDot` correction: `0x142119929` reads `K+0x40` (the ctx4 ball x), so the controller now
  uses the ball's x, not the carrier's.

`IsClose` is **not** stubbed: GF's `ShootZone` is the output of R4's `0x140698D10` (the same builder
PES's `0x140699820` consumes at `imageA+0x9CC`), so `pes::IsClose(zone)` is the R4 predicate itself.

Stand-ins, each named at its field with the PES site:

- the corridor block's three inputs (`corridorGuard` = `T+4 < 2`, the two `0x1421B7D40` counts, the
  `0x142119C9A` gate): GF carries no corridor evaluator, so they read false and the ratchet latches
  at 2. Named at `IntentionInput` with `0x142119960`.
- the B3C/B58 lead arm: `B58 = bp+0xC44 = Z.0x18C` (24.0 + 3·iVar10, S1Watch round 5), `B3C =
  bp+0xC28 = Z.0x170` (a float, R4-written; the `FUN_141193DC0` byte store does not fit the float
  read, S1Watch §7.4). GF does not carry either, so `ballLead_m`/`ballLimit_m` keep their existing
  ball-distance stand-in.

## 4. R4's zone and IsClose (run 2, after Main's follow-up)

**IsClose needed no port: GF's `pes::IsClose` already was the R4 predicate.** GF's `ShootZone` is the
output of R4's own builder `0x140698D10` (`pescarrier.hpp` names it at the struct), and the loader
reads R4's init immediates at R4's own VAs: `0x140699660` → nearCentral (18.0), `0x14069966A` →
farCentral (24.0), `0x140699674` → nearSide (16.0), `0x14069967E` → farSide (22.0), `0x14069964C` →
r0 (11.0), `0x140699656` → r1 (12.0), `0x140699688`/`0x140699692` → the 15/40 sector angles. The
field map is 1:1 with S1Watch §9: `d_m` = +0x170, `sector` = +0x178, `band` = +0x17C, `inBox` =
bit0 of +0x16C. GF's `IsClose` = `band == kBandClose` with the two thresholds chosen by the sector
(0x188 central, 0x190 side), i.e. S1Watch's corrected rule with T1 folded in; the `sign*K.x >= 0`
arm is the builder's behind-goal-line branch. Main accepted this (no port, no stand-in).

**One real divergence found and fixed: case B's distance took no absolute value.** R4 case B
(`pf[2] >= |K.z|`) is `+0x170 = |pf[0] - sign*K.x|` — the `andps` with the `0x142565D30` mask at
`0x140698EE1`, between the `subss` at `0x140698EDA` and the store at `0x140698EE9`. GF computed the
signed `halfLen - sign*ballX`, which goes negative for a ball past the goal line (55.0 gave −2.44).
Fixed to `std::fabs(...)`; a ball 2.44 m past the line now reads 2.44, both halves.

Case A (the ball outside the mouth band) was already exact: `d_m` is the 3D distance to
`(sign*pf[0], 0, sgn(K.z)*pf[2])`, the near post with no radius (`FUN_140A8F820` at `0x140698F1D`).

## 5. Open items

1. The match-setting record's selector (`+0x18`) producer and the per-side `+0x9200` byte's writer:
   not traced (GF has neither). The gate and the stock default are portable now.
2. The attack level `T+0xB8A4` (G8's other arm) is still the 0 stand-in (S4).
3. The corridor block's three inputs and the B3C/B58 lead arm: S1Watch rounds 4-6
   (`docs/re/pes-re-team-mode.md` §7-§9), not ported here.
4. The `FUN_140A92DC0` arctangent (S1Watch's 0.02° residual on +0x174) and the live zone-0/bit-0
   positive sample.
5. Score row S1, `K+0x6E`, Safety/GoalFront inner gates: unchanged (lane-inputs §8).

## 6. Verification

Branch `A4Level` (pushed): `e296f1b` (level + attack-heading convention), `a1882b9` (the aim-vs-facing
fix, split out on Main's ruling), `f2adf32` (G4's team mode) and the IsClose/case-B commit. Worktree
`<workdrive>/gf-A4Level`.
Test binaries run with `PES21_ROOT=<workdrive>/PES21`, directly, from the worktree root:

- `pes_carrier_test`: `[  PASSED  ] 156 tests.` (was 144 at the branch point; 12 new), no skips.
- `pes_lane_test`: `[  PASSED  ] 21 tests.`
- the whole `build/tests/sim` suite (46 binaries, 780 tests, all link `gfsim`, which carries the
  changed files): **0 failures, no skips**, re-run on the final tree (which includes the case-B abs
  fix in the shared builder).

Red first: each new behaviour was written as a test before the code and the build failed on it
(`'AttackAngleDeg' is not a member of 'pes'`, `'CarrierConstants' has no member named
'teamLevelRaw'`, `'AimVsFacingDeg' is not a member of 'pes'`), then green after the port. The two
zone value tests (`CaseBIsTheGoalLineDistanceToTheAbsoluteBallX`, `CaseAMeasuresToTheNearPost`) are
concrete positions with hand-computed outputs, not a recompute of the rule.
The G4 unit tests pass on the base tree too — `NextIntention` was already ported — so they lock the
writer rather than prove the wiring; the wiring is proven by the probe and the mutation below.

Mutations (each built, run, reverted, rebuilt):

1. `AttackAngleDeg` 90/270 -> 0/180 (the z axis, the old bug): caught by
   `OneTwoUnitTest.TheAttackBearingIsTheXAxisSoABehindOpponentDoesNotClose` and
   `PesCarrierBearing.TheAttackAngleIsTheBearingOfTheAttackDirection` (145/147).
2. The level scale dropped (`cls * scale` -> `cls`): caught by
   `PesCarrierInstall.TheStockTeamLevelIsAClassScaledByAHundred` (146/147).
3. `AimVsFacingDeg`'s facing put back into the mirrored frame (`270 - bearing`): caught by
   `PesCarrierBearing.ACarrierFacingTheGoalHeAttacksReadsZeroDegreesOffTheAim` (147/148).
4. The G4 wiring: `touch.teamMode = intention` back to the neutral `2`. The unit tests cannot see
   it (they set the field directly), and the 2-min hash does not move either — so it was proven with
   the temporary mode probe: the mutation reads `mode at G4: 2=22` (all 22 OneTouch calls), the
   wiring reads `3=22`. Built, run, reverted, rebuilt both times.
5. The case-B absolute value dropped (`std::fabs(halfLen - sign*ballX)` -> the signed form): caught
   by `PesCarrierZone.CaseBIsTheGoalLineDistanceToTheAbsoluteBallX` (155/156).

Smokes (2 min, seed 7, `teams.sql` 1 vs 2, run from the worktree root; exit 0 each):

- `--pes-loop`: `[determinism] 9fc0b08314812d8f`. G3 opens and G4 now passes, so the hash moves by
  design (it is the same value before and after the G4 wiring alone, because the Intention was
  already 3 on every OneTouch call the sim reaches — the probe shows that). The case-B abs fix does
  not move it either (2 min).
- switch-off: `[determinism] 75a11cdb2b3c8ec5`, against the base `a3d5e43a3b7b03af`. The move is
  `aimVsFacing` alone: the shoot gate's `|wrap180(aim - facing)|` (report 5.1's
  `angle(ball->aim, facing) > 100` at `0x142121920`, and `central_eval`'s 60/90 term), the one edit
  in the branch that reaches the non-`pes_loop` path. The old code subtracted a mirrored angle
  (`atan2(-y, -x)` = `270 - bearing`) from a PES bearing, so a carrier facing the goal he attacks
  read 180 instead of 0 and those gates inverted. Reverting only that line restores
  `a3d5e43a3b7b03af`. **Main's ruling: keep it** (a real mirrored-frame bug, in scope; split into
  `a1882b9` with its own red test and mutation, as recorded above). The G4 change leaves the
  switch-off hash untouched (the carrier path is `pes_loop`-only).

Gate: not run (Main gates). PII: changed files grepped for machine paths: none.

## 7. Silence probe (temporary counters, reverted; `--pes-loop` 2 min)

The probe is not on the branch. Two runs: before the G4 wiring (the level change only) and after it
(the re-probe Main asked for).

Before the G4 wiring: `OneTouch 22 reached 22 pass 0 | G3 0 G4 22 G5 0 ... G11 0 G12 0`, so G3 had
stopped closing OneTwo and all 22 calls died at G4 (the `teamMode = 2` stand-in).

After the G4 wiring (the re-probe): `OneTouch 22 pass 0 | G3 0 G4 0 G5 1 G6 0 G7 0 G8 0 G9 0 G10 0
G11 11 G12 10`, `answers OneTwo 0 DHB 0 BackSpace 0 Safety 0 GoalFront 0 | rethinks 0`. **G4 no
longer closes anything**, and OneTwo's closer has moved to **G11** (the 15 m plan-heading lane must
hold an opponent, so 11 calls died because the lane was FREE) and **G12** (the 0..-10 m curve band),
with one call at G5 (the 1600 score row).

| Unit (kind) | Answers | Where the rest die |
|---|---|---|
| OneTwo (10) | 0 | 22 reach `OneTouch`; G4 passes all 22; 11 die at G11 (the plan-heading lane is free), 10 at G12 (the curve band), 1 at G5 (the score row) |
| DirectHighBall (11) | 0 | unchanged: `K+0x6E` stand-in false |
| BackSpace (3) | 0 | unchanged: cut-back picker's validator / corner-curve gates |
| Safety (6) | 0 | unchanged: short picker's `ShortMateSurvives` |
| GoalFront (2) | 0 | unchanged: picker inner gates |

So OneTwo's next closer is **G11** (the lane 15 m along the plan heading must hold an opponent) and
then **G12** (the curve band, whose table `BuildCurve` builds from the opponents of the carrier's
side). Both are live-input problems rather than decode blockers: G11 reads `planHeading_deg`, which
the call site still stands in with the attack direction (the plan heading `ctx7+0xC0` is not carried),
and G12's band sign follows from the same table. G3 and G4 — this run's two gates — now pass every
OneTouch call the sim reaches.
