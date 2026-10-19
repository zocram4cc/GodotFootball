# A4Level: the one-touch team level (G3) and the attack-heading convention

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

## 3. Open items

1. The match-setting record's selector (`+0x18`) producer and the per-side `+0x9200` byte's writer:
   not traced (GF has neither). The gate and the stock default are portable now.
2. Team mode `M+8` (G4) and the attack level `T+0xB8A4` (G8) stay as before (S4).
3. Score row S1, `K+0x6E`, Safety/GoalFront inner gates: unchanged (lane-inputs §8).

## 4. Verification

Branch `A4Level` (pushed): `e296f1b` (level + attack-heading convention) and `a1882b9` (the
aim-vs-facing fix, split out on Main's ruling). Worktree `<workdrive>/gf-A4Level`.
Test binaries run with `PES21_ROOT=<workdrive>/PES21`, directly, from the worktree root:

- `pes_carrier_test`: `[  PASSED  ] 148 tests.` (was 144; 4 new), no skips.
- `pes_lane_test`: `[  PASSED  ] 21 tests.`
- the whole `build/tests/sim` suite (46 binaries, all link `gfsim`, which carries both changed
  files): every binary 0 failures, no skips.

Red first: each new behaviour was written as a test before the code and the build failed on it
(`'AttackAngleDeg' is not a member of 'pes'`, `'CarrierConstants' has no member named
'teamLevelRaw'`, `'AimVsFacingDeg' is not a member of 'pes'`), then green after the port.

Mutations (each built, run, reverted, rebuilt):

1. `AttackAngleDeg` 90/270 -> 0/180 (the z axis, the old bug): caught by
   `OneTwoUnitTest.TheAttackBearingIsTheXAxisSoABehindOpponentDoesNotClose` and
   `PesCarrierBearing.TheAttackAngleIsTheBearingOfTheAttackDirection` (145/147).
2. The level scale dropped (`cls * scale` -> `cls`): caught by
   `PesCarrierInstall.TheStockTeamLevelIsAClassScaledByAHundred` (146/147).
3. `AimVsFacingDeg`'s facing put back into the mirrored frame (`270 - bearing`): caught by
   `PesCarrierBearing.ACarrierFacingTheGoalHeAttacksReadsZeroDegreesOffTheAim` (147/148).

Smokes (2 min, seed 7, `teams.sql` 1 vs 2, run from the worktree root; exit 0 each):

- `--pes-loop`: `[determinism] 9fc0b08314812d8f`. G3 now opens, so the hash moves by design.
- switch-off: `[determinism] 75a11cdb2b3c8ec5`, against the base `a3d5e43a3b7b03af`. The move is
  `aimVsFacing` alone: the shoot gate's `|wrap180(aim - facing)|` (report 5.1's
  `angle(ball->aim, facing) > 100` at `0x142121920`, and `central_eval`'s 60/90 term), the one edit
  in the branch that reaches the non-`pes_loop` path. The old code subtracted a mirrored angle
  (`atan2(-y, -x)` = `270 - bearing`) from a PES bearing, so a carrier facing the goal he attacks
  read 180 instead of 0 and those gates inverted. Reverting only that line restores
  `a3d5e43a3b7b03af`. **Main's ruling: keep it** (a real mirrored-frame bug, in scope; split into
  `a1882b9` with its own red test and mutation, as recorded above).

Gate: not run (Main gates). PII: changed files grepped for machine paths: none.

## 5. Silence probe (temporary counters, reverted; `--pes-loop` 2 min)

The probe is not on the branch. It answers the A4Lanes3 question per unit:

| Unit (kind) | Answers | Where the rest die |
|---|---|---|
| OneTwo (10) | 0 | 24 rethinks reach `OneTwoUnit`; 22 reach `OneTouch`; **G3 now passes all 22** (it was the closer before this change); **all 22 die at G4** (the team-mode stand-in `teamMode = 2`, so the receiver predicate is still never reached) |
| DirectHighBall (11) | 0 | unchanged: `K+0x6E` stand-in false |
| BackSpace (3) | 0 | unchanged: cut-back picker's validator / corner-curve gates |
| Safety (6) | 0 | unchanged: short picker's `ShortMateSurvives` |
| GoalFront (2) | 0 | unchanged: picker inner gates |

`[probe]` lines: `OneTouch calls 22 reached 22 pass 0 | G3 0 G4 22 G5 0 G6 0 G7 0 G8 0 G9 0 G10 0
G11 0 G12 0` and `answers OneTwo 0 DHB 0 BackSpace 0 Safety 0 GoalFront 0 | OneTwo rethinks 24
mateTests 0`.

So the level change does what item 1 asked (G3 is no longer the closer) and moves OneTwo's live
closer to **G4**, whose input is the team mode `0x141052E30` = `*(M+8)` — still the `2` stand-in
(lane-inputs §8.3, writer OPEN). That is the next decode for OneTwo, not this ticket's.
