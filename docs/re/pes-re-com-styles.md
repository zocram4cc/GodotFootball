# PES 2021 COM playing styles and the image/intention system: verification of the rentry write-up

Scope: verify the claims of "How COM Playing Styles Work in PES 2021" (rentry.org/PES2021ForbiddenKnowledge, full text at the scratch dir/rentry.md) against the binary; cross-check our docs; fidelity check of the GodotFootball port. Decode only. No src/ edits, no commits.

Sources and tools:
- Ghidra bridge (127.0.0.1:8080, `decompile_function` / `disassemble_function`), 144 + 24 addresses pulled to the scratch dir/out/ (dec_*.txt, asm_*.txt).
- Raw bytes from `<RE scratch>` (base 0x140000000), via capstone (`disasm.py`, `consts.py`), for addresses the bridge has no function record for (0x14211B460, 0x14211B1D0, 0x14211AF20, 0x14211E0F0/E140/E160, 0x14211D710, 0x142110C90, 0x14210E5F0, 0x14212CC20, 0x14212FE70, 0x142130EE0, 0x14212E970, 0x14212A2D0, 0x14063ADC8).
- Table dumps (`val.py`, `tables.py`, `names.py`); RNG re-emulation (`rngsim.py`).
- Scratch: the scratch dir/.

Notation: **OBS** = re-verified against the binary with its VA. **INF** = inference, not re-verified in this pass. Verdicts: **CONFIRMED**, **CORRECTED** (correct value given), **REFUTED**, **NOT VERIFIED** (not re-checked in this pass; reason given).

dt18 note (from the user's interjection): no dt18 data was read in this pass. The carrier constants are read from the PES exe via `PeImage::ReadVa`. If a dt18-derived value is compared later, it must come from `<PES21 Install>/Data/dt18_all.cpk`, not `download/4cc_07_gameplay`. `PES21_ROOT` is not set in this shell, so the cpk was not opened.

---

## 0. Answers to the three direct questions

**Q1. Does GF re-seed a temporary RNG per evaluator from the captured RandomInfo+8 × K, so draws repeat while the seed is unchanged? Or does it draw fresh?**
GF draws fresh on each rethink. It does not use RandomInfo+8 at all. `elizacontroller.cpp:2003-2004` sets `nowFrame` from the carrier's possession duration (`PesFramesToGfTicks(GetPossessionDuration_ms()/kTick_ms)`), and `PlanRoll` (`pescarrier.cpp:619-622`) seeds slot 2 with `int(float(nowFrame)·K)`. The possession clock changes every tick, so every rethink gets a new draw. PES (OBS, §3 below) seeds `int(member+0x18 × K)`, where `member+0x18` is a copy of RandomInfo+8 that changes only on the member's update rule (negative-or-counter-1). While that copy is unchanged, PES repeats the same draw. GF's RNG generator itself (`pesrandom.cpp:91-96`, `RangeRaw = trunc(float(step)·2⁻³¹·M)`) is bit-exact with `sub_140A924E0`, so the difference is entirely the seed source.

**Q2. Stat-bonus truncation order and floors/caps per evaluator.**
PES (OBS): Breakthrough and MiddleShot add each stat's bonus as a truncated integer (`CVTTSS2SI` then `ADD EBX`); Cross, CurveShot and Long add each bonus to a float then truncate that step (`ADDSS` then `CVTTSS2SI`), so fractions are discarded between stats. Floors/caps: Breakthrough cap 70, no floor; MiddleShot cap 80 then floor 5; Cross cap 80 then floor 40; CurveShot cap 70, floor 5; Long none/none.
GF: sums a single float stat term and truncates once at the call site. GF reads only one stat per plan, so its per-stat ordering is moot; the real mismatches are the missing stats (§5). Floors and caps match for MiddleShot and Cross. Breakthrough's cap matches; CurveShot is not ported; Long's floor/cap is none, matching.

**Q3. Dispatcher priority.**
PES (OBS): `[9, 8, 2, 3, 1, 4, 5, 6, 7]`; entry 7 is inline in the dispatcher code, not in the 8-entry table at 0x142BF02C0 (`0x14211E2D0`). GF: `{9,8,2,3,1,4,5,6}` (`pescarrier.hpp:386`), and its actual admission loop tries Breakthrough (1) before MiddleShot (2), omits CurveShot (3) entirely, and omits SideChange (7). GF also re-decides every rethink; PES keeps a selected image until its cancel test fires, and admits only when none is selected.

---

## 1. Verdict table (per rentry section)

### §1 Storage
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 1.1 | Skill QWORD at roster +0x48; live +0x40 | CONFIRMED | `CheckSkill_Live` 0x14063E960 reads `[stat+0x48]` (dec); live `[rbx+0x40]` read at 0x14211ACAF |
| 1.2 | Regular styles live +0x48 (DWORD) | CONFIRMED (live). Roster +0x50 NOT VERIFIED | 0x14211AC90 `mov eax,[rbx+0x48]; shr 7` (Cross Specialist) |
| 1.3 | COM mask live +0x4C, roster +0x54 | CONFIRMED | `CheckCOMStyle_Live` 0x14063E830 reads `[lVar2+0x54]` (dec); live `[rbp+0x4c]`/`[rax+0x4c]` reads at 0x14211A98A, 0x142130A67, 0x142130C22, 0x14211AC9E, 0x14211B046, 0x14211B2D1 |
| 1.4 | Copy path writes 80 bytes to dest+0x70 (GetLiveStatOutput = x+0x70) | CONFIRMED for the 80-byte copy (`0x140A58F00`: ten qwords to +0x70..+0xB8). "x+0x70" accessor NOT VERIFIED separately | dec 0x140A58F00 |
| 1.5 | Mask builders 0x141FEA250 (skills), 0x141FE9F40 (regular+COM), table 0x14352AC10 | Table CONFIRMED (§2). Builders NOT VERIFIED | — |
| 1.6 | `CheckCOMStyle_Live`: playerId ≤0x15; ≥11 = other team; `_bittest` on roster +0x54 | CONFIRMED | dec 0x14063E830; asm 0x14063E960 region (same team split `CMP EBX,0xB; SETGE`) |
| 1.7 | `CheckSkill_Live` reads QWORD at +0x48, not interchangeable with COM | CONFIRMED | dec and asm 0x14063E960 (`mov rax,[rax+0x48]`) |
| 1.8 | Byte access at live +76 reads low byte of COM mask | CONFIRMED | 0x1407DDFDE `test byte ptr [rax+0x4c],1`; 0x1408527AF `movzx byte [rax+0x4c]; and al,1` |

### §2 Mapping tables
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 2.1 | UI field table 0x1429790D8 = 60..66 | CONFIRMED | `tables.py` dump |
| 2.2 | UI name short IDs 0x0160000B/0D/4C/12/4D/0F/13 (rows 0–6) | CONFIRMED (IDs). all.str resolution NOT VERIFIED | 0x1427D35B0 dump |
| 2.3 | Runtime rows (bit, field, UI index) at 0x14352AC10: 0:(0,60,0) 1:(1,62,2) 2:(2,61,1) 3:(3,65,5) 4:(4,63,3) 5:(5,66,6) 6:(6,64,4) | CONFIRMED (all 7 rows) | dump |
| 2.4 | Direct COM readers: bit0 byte-test 0x1407DDFDE and 0x1408527AF; bit1 0x142130A67 (`shr 1`); bit2 0x142130C22 (`shr 2`); bit3 0x14211AC9E (`shr 3`); bit4 0x14211B046 (`shr 4`); bit6 0x14211B2D1 (`shr 6`) | CONFIRMED (all listed sites) | `disasm.py` |
| 2.5 | Bit 5 readers: helper 0x14211FE20 calls COM-5 three times | CALL SITE NOT VERIFIED (the MiddleShot COM-5 call at 0x14211B600, `r8d=5`, IS confirmed) | 0x14211B5FD–B600 |
| 2.6 | Dribble grade 0x141513A50 (player,0): fields 17/33/37 averaged; −2 if DRI or SPD <82; +≤3 for skills 67/68/69/99/104/105; grade 0 <83, 1 at 83–87, 2 ≥88 | CONFIRMED (every constant and branch in the decompile) | dec 0x141513A50; skill list dwords at 0x142980E50 = 67,68,69,99 (+104,105 locals) |
| 2.7 | Ball samples: `BallInfo_GetSample0` 0x140A57800, `GetSampleClamped` 0x140A6CF70; sample 0 is the starting point | NOT VERIFIED (fetched, not read) | — |
| 2.8 | Action names: 3 DRIBBLE, 21 CLEAR, 26 KICK_FEINT, 27 FEINT, 18 LONG_PASS, 19 CENTERING, 20 EARLY_CROSS | CONFIRMED (table at 0x1434A4D50, resolved strings) | `names.py` |
| 2.9 | Feint kinds (0x1434A5760): 8 SCISSORS_IN, 9 SCISSORS_OUT, 10 SCISSORS_OUT_LOOP, 11 BODYFAKE, 12 KICKFEINT, 14 BURST, 15 MEIALUA, 17 DOUBLETOUCH, 18 ERACICO | CONFIRMED (27 ONETIME not re-read; output truncated) | `names.py` |
| 2.10 | Animation property table (0x1426484F0): Trap 7 flag 0x20, Feint 18 flag 0x10, Jostle flag 0x8000 | NOT VERIFIED (AnimeAction_IsFeint 0x14210E… structure NOT re-read) | — |
| 2.11 | Live stat map: 29 rows; selector→packed table (0:14, 1:18, …, 28:41); live 53 → selector 25 | CONFIRMED (all 29 rows and 29 selector entries). Stat *names* from packed indices NOT RE-RESOLVED | `tables.py` (0x14352AB10, 0x142978FD0) |
| 2.12 | "DARTING_RUN" internal name for bit 1; Japanese/English long-description ids | NOT VERIFIED | — |

### §3 Probability machinery
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 3.1 | RNG ctor 0x140A923F0 initializes streams 0..3 to 0x2f3d0+i; seed write 0x140A928A0 writes stream; draw 0x140A924E0; dtor 0x140A92430 | CONFIRMED | dec/asm |
| 3.2 | LCG `s' = (17683 − 757248739·s) & 0x7FFFFFFF` | CONFIRMED (`IMUL 0x2d22b2e3`, `SUB 0x4513`, `BTR 31`) | asm 0x140A924E0 |
| 3.3 | Output = trunc(float(s')·2⁻³¹·range); 2⁻³¹ at 0x1426550D8 | CONFIRMED | asm; val.py (0x1426550d8 = 0x30000000 = 2⁻³¹) |
| 3.4 | Accept when score ≥ draw | CONFIRMED (Breakthrough `CMP EBX,EAX; JNC` 0x14211AACF; MiddleShot `JAE`; Cross `CMOVNC`) | asm |
| 3.5 | Draws 0–99 for every seed the evaluators can produce | CONFIRMED (emulation over seeds 0..K for all five K: min 0, max 99, no 100) | `rngsim.py` |
| 3.6 | K = 1100 / 1200 / 1300 / 1500 / 1600 at 0x142BF02B4 / 0x14260BA7C / 0x14260B388 / 0x1425E9094 / 0x14259CE40 | CONFIRMED (values); the multiply sites Breakthrough 0x14211AA98, MiddleShot 0x14211B738, CurveShot 0x14211B124, Cross 0x14211AD7C, Long 0x14211B3E8 CONFIRMED | val.py; asm |
| 3.7 | Seed = int(float(`*(a2+16)`+0x18) × K) on stream 2; load sites Breakthrough 0x14211AA8F, MiddleShot 0x14211B72F, CurveShot 0x14211B11B, Cross 0x14211AD73, Long 0x14211B3DF | CONFIRMED (all five load sites) | asm |
| 3.8 | The seed member is the embedded BallPlayer+0x78 object; MainEval passes it as `lea r9,[rbp+0x78]` (0x142110E37, 0x142110E63); Subsystem4 stores it as ctx[2]; ctx[4] written at 0x14211ED4A | CONFIRMED (both lea sites; ctx[4] store; Subsystem4's ctx build at 0x14211D710 stores a4 (=r9) at ctx+0x10, so a2[2] is the member) | `disasm.py`: 0x142110E37, 0x142110E63, 0x14211ED4A, 0x14211D710 stack layout |
| 3.9 | Member update 0x14063A490: counter +8 increments; copies `*(gMatchContext+0x360)+8` (RandomInfo+8) into member+0x18 when the stored value is negative or counter==1 | CONFIRMED (asm 0x14063A72B–74C: `COMISS 0 > [rdi+0x18]; JA`; `CMP [rdi+8],1`; `MOV RCX,[RAX+0x360]`; `MOV EAX,[RCX+8]`) | asm |
| 3.10 | Driver 0x142110C90: predicate 0x14210E200; true → byte[+0x10]=1, `lea rcx,[rbx+0x78]` at 0x142110CEB, tail jump 0x14063A490; false → byte 0, jump 0x14210E5F0 | CONFIRMED | `disasm.py` 0x142110C90 |
| 3.11 | Reset 0x14063A3B0 writes −1.0 to member+0x18 at 0x14063A3C1; reached via 0x14210E6EA from reset 0x14210E5F0 | CONFIRMED | asm 0x14063A3B0; `disasm.py` 0x14210E5F0 line 39 |
| 3.12 | Initializer 0x14063A310 writes −1.0 at +0x18 (0x14063A329); BallPlayer ctor call site 0x14210D38E | Initializer CONFIRMED. Ctor call site NOT VERIFIED | asm |
| 3.13 | Ctor 0x14063A290 writes −1.0 then runs one update | CONFIRMED (dec) | dec 0x14063A290 |
| 3.14 | RandomInfo producer 0x140435EF0: draws stream 2 as int (→+0x28), range 100 (→+0x2C), range 2 (→+0x2D), range 999999 then `/999999.0` (→ float at +8 of the published record) | CONFIRMED (dec and asm 0x140435EF0; val.py 0x14259bc10 = 999999.0). Note: the producer stores directly to `*(this+0x58)+8`; the "publish via 0x140435C50" step is NOT VERIFIED (no function record at that VA) | dec 0x140435ef0 |
| 3.15 | Callback 0x14041E2B0 → 0x140435B30 | 140435B30→140435EF0 CONFIRMED; callback NOT VERIFIED (no function record) | dec 0x140435b30 |
| 3.16 | RandomInfo "28-byte record"; random block at +12..20 (0x140435BA0); match-setup writer 0x141FE288C at +24 | NOT VERIFIED | — |
| 3.17 | Stat bonuses: Breakthrough `clamp((s−70)/20,0,1)×15`, saturates at 90; MiddleShot `/29 ×15`, saturates 99; Cross `/29 ×20`; CurveShot `/29 ×10`; Long `/29 ×10` | CONFIRMED (divisors 0x14259BE1C=20, 0x1425A9460=29; gains 0x14259BE0C=15, 0x142588210=10, 0x14259BE1C=20 used as Cross gain; 0x14259BE5C=90 saturation; 0x14256AE1C=1.0) | val.py; asm of each evaluator |
| 3.18 | Breakthrough: each bonus truncated then added (integer); MiddleShot likewise (0x14211B6D4/D9, 0x14211B70A/70F); Cross/CurveShot float-then-truncate (0x14211AD0E, 0x14211AD4D, 0x14211B0B8, 0x14211B0F5); Long one step (0x14211B3CF) | CONFIRMED | asm |
| 3.19 | "Cross 50 with bonuses 80/80 → 56 then 62, not 63" | CONFIRMED (arithmetic from the per-step truncation in 3.18) | — |
| 3.20 | Floors/caps: Breakthrough none/70; CurveShot 5/70; MiddleShot 5/80; Cross 40/80; Long none/none | CONFIRMED (Breakthrough `CMOVA` cap 70; MiddleShot cap 80 at 0x14211B714–716, floor `LEA R15D,[RSI+5]` at 0x14211B5E7; Cross `MOV EDI,0x50` and `MOV EAX,0x28`; CurveShot `MOV EAX,5`/cap 70 at 0x14211B0FA–110; Long none) | asm |

### §4 Image unit manager and dispatcher
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 4.1 | Manager at BallPlayer+12992 (0x32C0); slot = mgr+16·id via 0x14211CA80 | CONFIRMED (reset 0x14210E5F0 `lea rcx,[rsi+0x32c0]`; dec 0x14211CA80) | `disasm.py` |
| 4.2 | +0xA0 selected id; +0xCC = vtable[7] result | CONFIRMED (dispatcher reads `param_1[0x14]`=+0xA0 and writes +0xCC) | dec 0x14211E2D0 |
| 4.3 | +0xA4 frames, +0xC0 heading, +0xC4 mode (init 2), +0xC8 prev mode, +0xD9 latch | NOT VERIFIED (heading +0xC0 consistent with trick reads; others not re-read) | — |
| 4.4 | Vtable: [1] update, [2] admission, [3] cancel, [4] on-select, [5] active, [7] getter | CONFIRMED (dispatcher calls offsets +8, +0x10, +0x18, +0x20, +0x28, +0x38) | dec 0x14211E2D0 |
| 4.5 | Mode producers: Basic 0x14211DC00 (1/2/3, Intention 4→1) | NOT VERIFIED (only the entry was fetched) | — |
| 4.6 | Breakthrough, MiddleShot, ShotPassFromGoal: mode always 1 (0x140A4C0F0) | CONFIRMED (`MOV EAX,1; RET`) | asm 0x140A4C0F0 |
| 4.7 | Cross mode always 4 (0x140E8CFF0) | CONFIRMED (`MOV EAX,4`) | asm |
| 4.8 | BackDFLine mode always 2 (0x141F05C10) | CONFIRMED (`MOV EAX,2`) | asm |
| 4.9 | Long/SideChange mode 8 or 2 (0x14211E140) | CONFIRMED (`cmp [rcx+8],0x15; mov eax,2; mov edx,8; cmovbe`) | `disasm.py` |
| 4.10 | CurveShot mode sticky `unit+12` set on byte+111 or evalX > halfLen·0.5 (0x14211E0F0) | CONFIRMED (`0x142565600`=0.5; `[r8]`=pitch[0]) | `disasm.py` |
| 4.11 | PassThrough mode (0x14211E160): 1 if Intention≥3, byte+111, or self-record +52 ≠0; else 2; the 3-returning tail is unreachable (+52 must be 0 to enter, then ≥2 on the same read) | CONFIRMED (the tail is unreachable as stated: the same +52 field is 0 at 0x14211E1A1 and must be ≥2 at 0x14211E1D8) | `disasm.py` |
| 4.12 | Dispatcher order `[9,8,2,3,1,4,5,6,7]` (dwords at 0x142BF02C0 plus inline 7) | CONFIRMED | dec 0x14211E2D0 (`local_58`, `local_38=7`); val.py (0x142BF02C0..DC = 9,8,2,3,1,4,5,6; 0x142BF02E0=0) |
| 4.13 | Dispatcher clears selection when `+0x116C==2` (gMatchContext+624+4460) and `+0x11D4 ≠0` (+4564) | CONFIRMED | dec 0x14211E2D0 |
| 4.14 | Normal path: update [1] on all units; cancel [3] on selected; admission [2] only if selected==0 in priority order; on new selection call [4]; call [5] on active; store [7] | CONFIRMED | dec 0x14211E2D0 |
| 4.15 | Gates: Long Ranger state <3 (MiddleShot 0x14211B460); Incisive ≥3 (CurveShot 0x14211AF20); SB ≥3 (Breakthrough 0x14211A800 `CMP EAX,3; JL`); Early Cross admission has no Intention test (0x14211AB10); Long rejects 3/4 via 0x14211A570 | CONFIRMED (each cited instruction) | asm/`disasm.py` |
| 4.16 | Mazing evaluator 0x14212CC20 has no state read, caller 0x14212CB30 requires 3/4 | Evaluator no-state-read NOT RE-CHECKED; caller NOT VERIFIED | — |
| 4.17 | Breakthrough cancel 0x14211A550 = Intention < 3 | CONFIRMED | asm |
| 4.18 | Subsystem4 (0x14211D710): body runs only when `*a4` DWORD is zero (`CMP [R9],0`); sequence selection → mode → heading/target → byte-217 | Gate CONFIRMED. Sequence beyond the dispatcher call NOT VERIFIED | `disasm.py` |
| 4.19 | Target helper 0x14211C610, heading 0x14211C380, mode-1 target 0x14211E650 | NOT VERIFIED | — |
| 4.20 | Speeding Bullet is the only permission for mode 1 with ID∉{1,2,3} | INF (derivation from 4.6–4.9 and the planner gate; not re-verified end to end) | — |

### §5 Spatial ratchet (Intention)
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 5.1 | Intention member at BallPlayer+12976 (0x32B0), state at +8 (= BallPlayer+12984), init 1 | CONFIRMED for location and +8 (reset `lea rsi+0x32b0; call 0x1415595D0`; updater store `*(local_150+8)`). Init value 1 via 0x1415595D0 NOT VERIFIED | `disasm.py` 0x14210E5F0; dec 0x1421197B0 |
| 5.2 | Updater BallPlayer_Subsystem3 0x1421197B0 | CONFIRMED | dec |
| 5.3 | State 4: already 4, or `sub_140699820(a5+2508)` and facing diff `< 105°` (no position test) | **CORRECTED: 100°, not 105°.** asm 1421198F7–1421198FA: `COMISS XMM3(=[0x14253EE30]=100.0), |diff|; JBE` | val.py 0x14253EE30 = 100.0 |
| 5.4 | State 3: already 3, or `attackDir · (a4+0x40) > pitch[6]`; or `(a5+2904)+10 > (a5+2876)` | CONFIRMED (pitch[6] = +24 of pitchDim; 0x142588210=10.0) | dec 0x1421197B0 |
| 5.5 | State 2 via `a6+4 < 2 && state ≠ 2` query block; otherwise 2 (latching) | CONFIRMED (control flow); corridor-query internals NOT VERIFIED | dec |
| 5.6 | State 1: `TeamData+308 ≥ 3`, or `sub_140A62180 ≠ 1`, or `2·GameSpeed ≤ a4+16` (JA skips) | CONFIRMED | dec (`local_140` = `[lVar9+0x134]`, `local_13c`, `iVar6*2 <= a4[+16]`) |
| 5.7 | State 0 fallthrough | CONFIRMED (`uVar14` left 0 when the query conditions fail) | dec |
| 5.8 | 3 and 4 absorbing; 2 latches; 1/0 not latching | CONFIRMED (no path to lower states outside the reset) | dec |
| 5.9 | pitchDim values (+0 halfLen 52.56, +24 penalty line 36.14, +28 penalty half-width 20.12, …) | NOT RE-VERIFIED (pitch[6] use confirmed; numeric fields not dumped) | — |
| 5.10 | Reset 0x14210E5F0 resets Intention, image manager, seed member, image A | CONFIRMED (calls 0x1415595D0, 0x14211BF60, 0x14063A3B0, 0x142119330) | `disasm.py` |
| 5.11 | Reset is invoked by the driver when predicate 0x14210E200 is false | CONFIRMED | asm 0x142110D0B–D21 |
| 5.12 | "339-second sample, 70 rises / 65 resets" | NOT CHECKABLE (artifact not provided) | — |

### §6 Per style
**Bit 0 Trickster**
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 6.0.1 | Live byte +76 & 1 getter 0x140852770 | CONFIRMED | 0x1408527AF |
| 6.0.2 | Byte-width read 0x1407DDFDE; ANDed with skills 3 and 28 via 0x1406EAD40 | Read CONFIRMED; `0x1406EAD40` NOT VERIFIED | — |
| 6.0.3 | ThinkUnitDribble trick gates: heading >= 0; cooldown 0; duel slot ≤0x15; ctx[4]+111==0, +109≠0; Intention exactly 3 (0x1421306A4); heading diff < 45 | Constants CONFIRMED (45.0 at 0x14259BE30 loaded at 0x1421302B3; 0.1 at 0x142588200 used in the cooldown path; 0x14212FE70 range); branch order NOT RE-DECODED | `consts.py` 14212fe70 |
| 6.0.4 | Scissors selector: compare 0.7 (0x14257A194) against ctx[4]+0x18, pick 9 when below, else 8 | CONFIRMED (0x142130778 `movss xmm0,[0x14257a194]; comiss xmm0,[rax+0x18]`; `rax = [rsi+0x20]`) | `disasm.py` |
| 6.0.5 | Cooldown 4 at unit+692; kinds stored at unit+700/708 | NOT VERIFIED | — |

**Bit 1 Speeding Bullet**
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 6.1.1 | Breakthrough reject gates: Intention<3; `!0x1406997A0(pred+2508) && signedX ≤ pitch[6]`; `0x140699710 && !0x140699820`; TeamData+47266; angle diff > 60° | CONFIRMED (all five; 60.0 at 0x14259A764) | asm 0x14211A800 |
| 6.1.2 | Score priority: grade==1 → 40; COM bit1 or bit4 → 20; COM bit2 → 20; skill 37 or grade==2 → 70; else 0 | CONFIRMED as a priority list. Note: COM bits 1/2/4 OVERRIDE a 70 (CMOVNZ/EBX=20 at 0x14211A98A–A9AE) | asm 0x14211A800 |
| 6.1.3 | Breakthrough stats +23, +39, +40, each `/20` truncated, ≤15 each, cap 70 | CONFIRMED (bytes `[rbp+0x17]`, `[rbp+0x27]`, `[rbp+0x28]`) | asm |
| 6.1.4 | Stage-2 read 0x142130A67 (bit1 in ThinkUnitDribble) | CONFIRMED read site; surrounding boolean NOT RE-DECODED | `disasm.py` |
| 6.1.5 | BeatMarkerPlanner 0x142130EE0: urgency 0.9, duel slot >0x15, distance 1.0, own score >6.25 (0x1425A9BE4) → exit; ±80° when dist² ∈ (4,25); 60–90° clamp | CONFIRMED for constants 0.9 (0x14258C260), 1.0, 6.25, 16, 0.5, 5.0. dist² band (4,25) constants NOT re-read | `consts.py` 142130ee0 |
| 6.1.6 | Branch (a) DT type 4 kind 17; (b) type 3 subtype 3 with grade ≥2; (c) CarryOrRelease | NOT RE-DECODED | — |
| 6.1.7 | BURST subtype-6 gate: 30° (0x142591140) | CONFIRMED value/site (30.0; used in 0x14212E970 and 0x142130463) | `consts.py` |
| 6.1.8 | Subtype payload dispatch 0x14210E880 (3/4 → dribble, 5 → dribble out 4, 6 → FEINT kind 14, 7 → kind 15) | NOT VERIFIED | — |
| 6.1.9 | CarryViability 0x14212E970: speed >15 with state ≥3 rejection; distance threshold 1.2·max(15,d) | CONFIRMED (15.0 at 0x14259BE0C; 1.2 at 0x14259D5D4) | `consts.py` 14212e970 |
| 6.1.10 | Movement tiers 5/11/16/28 km/h via `TierToFloat` 0x140A76070 | NOT VERIFIED | — |
| 6.1.11 | Latch behaviour (0x1421308B4), byte-217 governor, adoption, clearance suppression (0x14210F4EC) | NOT VERIFIED (latch read site 0x1421308B4 confirmed as a jump) | — |

**Bit 2 Mazing Run**
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 6.2.1 | Gate `CheckSkill(4) || CheckCOMStyle(2)` at 0x14212CD01 | Branch site CONFIRMED; the call pair NOT RE-READ | `disasm.py` |
| 6.2.2 | Caller state gating 0x14212CB30 (state 3: C2C0/D180/C670 then Mazing; state 4: C670 then Mazing) | NOT VERIFIED | — |
| 6.2.3 | Fresh evaluation exclusions: Dive 15, Stagger 16, FallDown 17, Feint 18; Trap conditional | NOT VERIFIED | — |
| 6.2.4 | Constants: spatial value <900; ≤4 m (16 = 4²); height ≤0.5; heading within 100°; carrier 15 / target 20 km/h; target >20 km/h; 30°; 45°; 90°; 80°; 5 m | CONFIRMED (900 at 0x1425C65F4; 16 at 0x14259BE10; 0.5 at 0x142565600; 100 at 0x14253EE30; 15/20 at 0x14259BE0C/BE1C; 30 at 0x142591140; 45 at 0x14259BE30; 90 at 0x14259BE5C; 80 at 0x142596E6C; 5 at 0x14259222C) | `consts.py` 14212cc20 |
| 6.2.5 | Output kind 12 KICKFEINT, heading H minus ±80°; H from the penalty/goal-area rule | Kind 12 name CONFIRMED (table). Heading logic NOT RE-DECODED | — |
| 6.2.6 | Dribbling ≥80 fallback (live +23 at 0x142130C35); second angle tail unreachable with unchanged inputs | Constants/reads partly CONFIRMED (read at 0x142130C22); unreachability NOT RE-DECODED | `disasm.py` |
| 6.2.7 | Cached follow-up: retained latch, copies cached answer as type 3/subtype 4; latch cleared on Feint animation or command ≠ FEINT | NOT VERIFIED (0x14212D9A6 `cmp [rax+1],0x12` CONFIRMED as the action-18 test) | `disasm.py` |
| 6.2.8 | Feint unit reset clears cache and latch (sub_1421107D0 via reset loop) | NOT VERIFIED (0x1421107D0 fetched, not read) | — |

**Bit 3 Early Cross**
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 6.3.1 | ImageUnitCross selects region record at pred+0x278 or +0x294 by sign | CONFIRMED | asm 0x14211AB10 (`ECX=0x278/0x294`) |
| 6.3.2 | Base from eval+0x54: 1 → 50, 2 → 70; when the field is ≤0, fallback 50 from four sources: sub_14063A3A0(eval+56), regular style bit 7, COM bit 3, skill 14 | CONFIRMED (asm 0x14211AC4C–ACC2, `LEA EDI,[RAX+0x31]`=50, `LEA EDI,[RAX+0x44]`=70; fallbacks at 0x14211AC7D, AC8D, AC9E, ACAF) | asm |
| 6.3.3 | Stats Kicking Power 0x2A and Finishing 0x1A, +20 each `/29`, clamp [40, 80] | CONFIRMED | asm 0x14211AB10 |
| 6.3.4 | Seed ×1500 | CONFIRMED | asm 0x14211AD7C; val.py |
| 6.3.5 | Roll skipped for designated crosser or TeamData+47266; region gate still applies | CONFIRMED (0x14211AC36, 0x14211AC44 bypass to 0x14211ADCC) | asm |
| 6.3.6 | 11-term OR gate: designated; sub_14063A3A0; +46816; +47266; skill 14; (+2888 ≤0 and +2884≠0); \|Z\|<pitch+28 and +2884≠3; two GameSpeed thresholds; COM bit 3; (a2+16)+113 | CONFIRMED (order and all terms, 0x14211ADD6…AED3). Thresholds are 5.0 and 10.0 × GameSpeed + 0.5 (0x14259222C, 0x142588210) = 270 / 540 frames at 54 Hz | asm |
| 6.3.7 | TeamData+46816 and +47266 read as byte compares at 0x14211ADFD and 0x14211AE0A | CONFIRMED (0xB6E0, 0xB8A2) | asm |
| 6.3.8 | Early-cross planner (ThinkUnitPassEarlyCross 0x14212A2D0): two opponent-space queries, receiver selection, 2 m check (0x14212A66B) | Partially CONFIRMED (2.0 at 0x142565604 near 0x14212A6A2; 100 at 0x14253EE30) | `consts.py` 14212a2d0 |
| 6.3.9 | Zone flags: +113 when X > penalty line − 2 (34.14) and \|Z\| > goal-area half-width (9.16); +114 when 0.2·halfLen < X < penalty line and \|Z\| > half-width + 3 (12.16); the two are set independently | CONFIRMED (writer 0x14063ADC8: 0x14063AE1C `mov [rdi+0x71],1`; 0x14063AE60 `mov [rdi+0x72],1`; constants 2.0, 0.2 at 0x142598578, 3.0 at 0x14258C264, 9.16 via +20) | `consts.py` 14063adc8; asm 0x14063ADE0 |
| 6.3.10 | Receiver = greatest Height (byte +49, 0x142125C25); passer ≥2 m from reference | Height byte NOT VERIFIED; 2 m partly confirmed (see 6.3.8) | — |
| 6.3.11 | Action 19 CENTERING vs 20 EARLY_CROSS are separate | CONFIRMED (action names) | names.py |
| 6.3.12 | early_cross_analysis counts (446/367/15/6/1) | NOT CHECKABLE (historical file) | — |

**Bit 4 Incisive Run**
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 6.4.1 | CurveShot requires Intention ≥3; pred+2880 not >0; TeamData+47266 clear | Intention ≥3 and 47266 CONFIRMED; pred+2880 NOT RE-READ | asm 0x14211AF20 (`CMP EAX,3; JL`; `CMP [rbp+0xb8a2],0`) |
| 6.4.2 | Side check uses eval+72 (stronger foot) | NOT VERIFIED | — |
| 6.4.3 | Base: COM4 → 30, skill 7 → 30; 0/30/30/60 | CONFIRMED (`shr eax,4; and al,1; neg; sbb; and ecx,0x1e`; `shr al,7; lea ebx,[rcx+0x1e]; cmove`) | asm 0x14211B046–061 |
| 6.4.4 | +10 each KP/Finishing `/29`, floor 5, cap 70, seed ×1300; final gate 47266 → skill 7 → COM 4 | CONFIRMED (0x142588210 = 10; 0x1425A9460 = 29; `mov eax,5` at 0x14211B106; cap 70; K at 0x14260B388; gate sites 0x14211B169/B185/B19D) | asm |

**Bit 5 Long Ranger**
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 6.5.1 | MiddleShot reads COM bit 5 at 0x14211B600 (constant 5) | CONFIRMED (call site `R8D=R15D=5`, `R15D=[RSI+5]`) | asm 0x14211B5E7–B600 |
| 6.5.2 | Returns 0 if Intention ≥3 | CONFIRMED (0x14211B4EB `CMP EAX,3; JGE`) | asm |
| 6.5.3 | `(a2[3]+2884)==3` requires signedX ≥ pitch[6]; else `0x1406997A0` | CONFIRMED (0x14211B4F8–B51E) | asm |
| 6.5.4 | `a2[2]+111 == 0` | CONFIRMED (0x14211B52E `CMP [RSI+0x6F],0`) | asm |
| 6.5.5 | Facing to goal ≤60° | CONFIRMED (0x14211B5C6 vs 0x14259A764 = 60) | asm |
| 6.5.6 | Base 70 if skill 39, COM 5, skill 8, skill 33, or skill 34 | CONFIRMED (order and sites 0x14211B5F4, B5FD, B614, B61F, B639, B643, B658–B669) | asm |
| 6.5.7 | +40 if state==2 or TeamData+47266 | CONFIRMED (0x14211B686 `add ebx,0x28`; state at 0x14211B677) | asm |
| 6.5.8 | +15 each KP/Finishing `/29`, floor 5, cap 80, seed ×1200 | CONFIRMED (gain 0x14259BE0C; divisor 0x1425A9460; cap 80; K 0x14260BA7C) | asm |
| 6.5.9 | Helper 0x14211FE20: three COM-5 calls; XZ²<49; OR-gate on signedX ≥ pitch[6] and skills 8/34/33 or COM 5 | NOT VERIFIED (only the entry was fetched) | — |

**Bit 6 Long Ball Expert**
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 6.6.1 | ImageUnitLong reads COM at 0x14211B2D1 | CONFIRMED | `disasm.py` |
| 6.6.2 | Veto 0x14211A570 rejects Intention 3/4, signed-X test, engagement (+16) <1, stored target slot >21 | CONFIRMED | asm 0x14211A570 (`JZ`/`JA`/`JL`/`SETA`) |
| 6.6.3 | Team +4 == 3, designated == self, or TeamData+47266 set → bypass the score | CONFIRMED, **but the bypass is an ACCEPT** (`0x14211B437: MOV AL,1`). Our doc (carrier-ai §4.4 row 6) treats these as gates; that is wrong (see §4) | asm 0x14211B259–B279 |
| 6.6.4 | Path split on eval+0x3C (`cmp [rdx+0x3c],0`) | CONFIRMED (0x14211B27F) | asm |
| 6.6.5 | eval+0x3C is a cached team-setting byte via 0x14063A420; team settings from TeamData+36360 | NOT VERIFIED (0x14063A420 not read) | — |
| 6.6.6 | Path A: engagement 30 (<2) / 60 (2) / 80 (≥3) from `sub_14069A3E0(+8484,255)+16`; COM6 +50; skill 13 +50; skill 17 +50; level (eval+0x54) +20/level, max +40; Lofted 10·ramp/29; no floor, no cap | CONFIRMED (0x14211B289–B3CF; the +50s at 0x14211B2D9, B2EF, B2FC; level at B303–B313) | asm |
| 6.6.7 | Path B (+0x3C ≠0): skill 13/17 +10 each, Lofted ramp +10, max 30; no COM6, no engagement | CONFIRMED (0x14211B35A–B3D4) | asm |
| 6.6.8 | Seed ×1600 | CONFIRMED (0x14259CE40 at 0x14211B3E8) | asm |
| 6.6.9 | PES 2015 heritage (sub_29CD500) | NOT CHECKABLE (archived handoff) | — |

### §7 Live-stat map
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 7.1 | 29 rows `(live, selector)` at 0x14352AB10 with live 53 → selector 25 | CONFIRMED | tables.py |
| 7.2 | Selector → packed mapping 0..28 | CONFIRMED (all 29 entries) | tables.py (0x142978FD0) |
| 7.3 | Live stat names (e.g. 26 Finishing, 42 Kicking Power, 28 Lofted Pass, 23 Dribbling) | Consistent with the code reads (0x1A Finishing, 0x2A Kicking Power, 0x1C Lofted Pass, 0x17 Dribbling); names via all.str NOT re-resolved | asm |

### §8 Team posture gauge
| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| 8.1 | +47266 (0xB8A2) = gauge ≥4 "full attack"; +47267 (0xB8A3) = gauge ≤0 "full defense"; gauge int at +47268 (0xB8A4) | CONFIRMED (setter 0x1404F6C30: 0x1404F6E1C writes 0, 0x1404F6E33 sets 1 if `cmp [..+0xB8A4],4; jl`; 0x1404F6E58 sets +47267 when gauge ≤0) | `disasm.py` 1404f6e00 |
| 8.2 | Update 0x1404F8350: ±1 nudge and automatic target-posture switch | NOT VERIFIED | — |
| 8.3 | Effects: boosts Long Ranger +40; bypasses Early Cross/Long gates; hard-rejects Incisive and Breakthrough | Bypass semantics CONFIRMED for Long (accept) and Cross (roll skip). "Hard-rejects Incisive and Breakthrough": Incisive CurveShot gates 47266 (rejects, 0x14211B1A9 path); Breakthrough 47266 check (0x14211A8E3) rejects — CONFIRMED | asm |
| 8.4 | +46816 writer unidentified (rentry §10) | Consistent with the 0x14211ADFD read; writer NOT RESOLVED | — |

### §9–§11
| ID | Claim | Verdict |
|---|---|---|
| 9 | 339-second live sample (8 Breakthrough selections, 57 s mode 1) | NOT CHECKABLE (capture not provided) |
| 10 | Open items | Accepted as the write-up's open list; none resolved here except the items marked above |
| 11 | `research/test_com_control_flow.py` (19 tests), evidence bundles | NOT CHECKED (file is outside this repo pass) |

---

## 2. Our-doc cross-check

Documents read: docs/re/pes-re-carrier-ai.md (§4.4 lines 366–400; §6 lines 609–670; lines 711, 738–739), docs/re/pes-re-carrier-port.md (§3.6, lines 258–330), docs/re/pes-re-foundation.md (P-IMG line 49; T-LVL line 45; MB rows), docs/re/pes-re-matchlogic.md (COM id table lines 133–140), docs/re/pes-re-gaps-anime.md (RNG slot 2, lines 32, 170), docs/re/pes-re-carrier-units*.md (no image-state claims found).

| Doc | Item | Status vs binary |
|---|---|---|
| carrier-ai §4.4 | Priority order "9, 8, 2, 3, 1, 4, 5, 6 (table @0x142bf02c0)" | **CORRECTED**: the list is `9,8,2,3,1,4,5,6,7`; 7 (SideChange) is inline in 0x14211E2D0. Table itself has 8 dwords. |
| carrier-ai §4.4 | "roll = RNG(seed = int(ctx4+0x18 × K))… (INF: a carrier clock)" | **CORRECTED**: `ctx4+0x18` is `BallPlayer+0x78 +0x18`, a copy of RandomInfo+8 taken by the member update (0x14063A490). It is not a carrier clock. The draw repeats while the copy is unchanged. |
| carrier-ai §4.4 | K table 1100/1200/1300/1500/1600 | CONFIRMED |
| carrier-ai §4.4 row 1 (Breakthrough) | Base 70 (+0x68==2 or DT), 40 (+0x68==1), 20 (COM 1/4/2), +15·ramp/20 on 0x17, 0x27, 0x28, cap 70 | Values and stats CONFIRMED. **Missing**: priority order. COM 1/2/4 overrides the 70 (0x14211A98A–A9AE). Grade==1 precedence over DT and COM is implied, not stated. Per-stat truncation not stated. |
| carrier-ai §4.4 row 2 (MiddleShot) | "+ 15·clamp01((x−70)/29) for 0x2A (INF Kicking Power) and 0x1A (INF Finishing)" | **CONFIRMED** (INF→OBS). Per-stat integer truncation is also needed. |
| carrier-ai §4.4 row 3 (CurveShot) | 30 (COM 4) + 30 (skill 7); +10·ramp/29 on 0x2A, 0x1A; clamp 5..70; final gate skill 7 or COM 4 unless team flag | **CONFIRMED** (the final gate also has the 47266 accept first). |
| carrier-ai §4.4 row 4 (PassThrough) | Admission at `0x14211b7b0` | NOT VERIFIED; the mode producer is 0x14211E160. The admission address in our doc is unconfirmed. |
| carrier-ai §4.4 row 5 (Cross) | 50 from style 7 / COM 3 / skill 14 / "team instruction BallPlayer+0x70 = 1→50, 2→70"; +20·ramp/29 on 0x2A, 0x1A; clamp 40..80; timing 270/540 frames | **CONFIRMED** on stats, clamp and the 270/540 frame thresholds (5 × 54, 10 × 54). **CORRECTED** on the base: `BallPlayer+0x70` is eval+0x54 (the same field), and it sets the base (1→50, 2→70) only when it is ≥1; the four-source fallback to 50 applies when the field is ≤0 and includes sub_14063A3A0 (team setting #1 == 0), which the doc omits. |
| carrier-ai §4.4 row 6 (Long) | "team mode ≠ 3, not designated" as gates; "not already vetoed (0x14211a570)" | **CORRECTED**: team +4 == 3, designated == self, and TeamData+47266 are **accepts that bypass the score** (0x14211B437 `MOV AL,1`), not gates. The veto 0x14211A570 is confirmed. Pressure class maps to engagement 30/60/80 (confirmed). Level +20/+40 (BallPlayer+0x70) confirmed; Path B confirmed. |
| carrier-ai §4.4 row 7–9 | SideChange, ShotPassFromGoal, BackDFLine addresses and gates | NOT VERIFIED in this pass (ShotPassFromGoal's gate numbers CONFIRMED by constants: 17 m → 289, 80°, 60°, 15 km/h, 45°; address 0x14211A030 confirmed as its entry). |
| carrier-ai line 663 | "stats 0x2A, 0x1A for crossing looks odd — labels need a check" | **CONFIRMED** (Cross reads 0x2A and 0x1A). |
| carrier-ai line 638 | COM bit list "6 LONG_PASS (INF: = Trickster, Speeding Bullet, Mazing Run, Early Cross, Incisive Run, Long Ranger, Long Ball Expert)" | **CONFIRMED** (bit order 0..6 matches the rentry's runtime table, 2.3). INF → OBS. |
| carrier-ai line 738–739 | "ctx4+0x18 (plan-roll seed), image state +0x32B0+8 (0..4, 0x1421197b0)" | Role CONFIRMED; the state range 0..4 and the writer 0x1421197B0 CONFIRMED. |
| carrier-ai line 160 | Basic: "unless image state = 4" | Consistent with the ratchet (state 4 absorbing); not otherwise verified. |
| carrier-port §3.6 | "`kPlanPriorityOrder` is 9, 8, 2, 3, 1, 4, 5, 6" | **CORRECTED**: add 7 (inline). GF's table at `pescarrier.hpp:386` lacks it. |
| carrier-port §3.6 | Roll "seeds slot 2 with `int(carrierClock × K)`… the carrier clock is the carrier's possession, which is what PES's `ctx4+0x18` is (INF)" | **REFUTED** (the PES input is the RandomInfo+8 snapshot, §3.8–3.9; `ctx4+0x18` is not the possession clock). |
| carrier-port §3.6 | "`mov edx,2` at `0x14211aaa5`; `cmp ebx,eax; jae` at `0x14211aacd`; `RangeRaw(2,100)` exact" | CONFIRMED |
| carrier-port §3.6 | P-table bases: Breakthrough 70 @0x14211a956; COM 20 @0x14211a991; cap 70 @0x14211aa7e; MiddleShot 70 @0x14211b628; cap 80 @0x14211b716; Cross 50 @0x14211ac86; floor 40 @0x14211ad5e; cap 80 @0x14211ad57; Long 30/60/80 @0x14211b289/b2c5/b2a9 | All CONFIRMED (immediates at those VAs). |
| carrier-port §3.6 | "MiddleShot state-2 40 @0x14211bcf9" | **CORRECTED**: 0x14211BCF9 is `mov r8d,0x28` in an unrelated CheckSkill call. The +40 is `add ebx,0x28` at **0x14211B686** (immediate at 0x14211B687). The value is 40 either way; the VA is wrong, and `pescarrier.cpp:~300 imm32(0x14211bcf9,...)` reads a wrong byte. |
| carrier-port §3.6 | MiddleShot floored at 5 "(−5 comparison at 0x14259be7c)" | **CORRECTED**: the floor is `LEA R15D,[RSI+5]` (0x14211B5E7) and `CMP EBX,R15D; CMOVB` (0x14211B71D–720). 0x14259BE7C holds −5.0 but is not the floor's source. |
| carrier-port §3.6 | Stat gains "15/15/20/10 at 0x14259be0c, 0x14259be0c, 0x14259be2c" and the same term for every plan | **CORRECTED**: Breakthrough's divisor is **20** (0x14259BE1C), not 29, and it reads three stats (0x17, 0x27, 0x28). Cross's gain is **20** at **0x14259BE1C**; 0x14259BE2C is **40.0**. CurveShot and Long gains are 10 at 0x142588210. The term is not the same for every plan. |
| carrier-port §3.6 | "MiddleShot's stat is `technical_shot` (PES's Kicking Power 0x2A, INF)" | **CORRECTED**: GF's `technical_shot` is Finishing (its own comment at `pescarrier.hpp:212` says stat 0x1A). PES MiddleShot reads 0x2A and 0x1A. |
| carrier-port §3.6 | "Breakthrough's stat is technical_dribble (Dribbling 0x17)" | Partially: PES reads 0x17, 0x27, 0x28 (§6.1.3). |
| carrier-port §3.6 | Long's stat `technical_highpass` = Lofted Pass 0x1C; base by pressure class | CONFIRMED (stat); base-by-class is the engagement value (CONFIRMED). |
| matchlogic COM labels 0x42–0x47 | Player.bin field IDs | Consistent as a different numbering (not contradicted). |
| foundation P-IMG (line 49) | "1421197B0 per Dribble (TP candidates conflict); 140a596f0; 92d580" | **CORRECTED**: the image state is the Intention member at BallPlayer+0x32B0 (+8), written by 0x1421197B0 (Subsystem3). The image manager is at +0x32C0 (0x14211E2D0). The seed member is BallPlayer+0x78 (0x14063A490). The "Dribble" attribution is unsupported. |
| foundation T-LVL (line 45) | posture bytes b8a2/b8a3/b8a4/b8c0 | CONFIRMED (b8a2 = full-attack flag, b8a3 = full-defence flag, b8a4 = gauge int). |
| foundation (MB-H, line 34) | tick order | New open dependency: the RandomInfo producer and member update order vs player evaluations is unresolved (see §4). |

---

## 3. Fidelity of the GodotFootball port (src/sim/onthepitch/pescarrier.*, elizacontroller.cpp)

### 3.1 Image admission dice
| # | GF | PES | Mismatch |
|---|---|---|---|
| F1 | `PlanRoll` (pescarrier.cpp:619–622) seeds slot 2 with `int(float(carrierClock)·K)`; `carrierClock` = `nowFrame` = possession duration (elizacontroller.cpp:2003–2004, 2073) | seed = `int(float(member+0x18)·K)`; member+0x18 = RandomInfo+8 snapshot (0x14063A490; seed loads at 0x14211AA8F, B72F, B11B, AD73, B3DF) | **Wrong seed source.** GF draws fresh each rethink; PES repeats while the snapshot is unchanged. |
| F2 | RNG `RangeRaw` (pesrandom.cpp:91–96) | `sub_140A924E0` | MATCH (bit-exact formula and `≤` vs `≥` accept at elizacontroller.cpp:2073 vs PES `score ≥ draw`). |
| F3 | K values read from exe (pescarrier.cpp:253–256) | 0x142BF02B4 etc. | MATCH (1100/1200/1300/1500/1600). |
| F4 | Rethink gate `RethinkDue` 5 PES frames (elizacontroller.cpp:2014; pescarrier.hpp) | Driver cadence 0x142110C90 | NOT VERIFIED (cadence sources differ, see write-up §3 timing). |

### 3.2 Plan order and selection
| # | GF | PES | Mismatch |
|---|---|---|---|
| F5 | `kPlanPriorityOrder = {9,8,2,3,1,4,5,6}` (pescarrier.hpp:386) | `[9,8,2,3,1,4,5,6,7]` (0x14211E2D0) | **Missing 7 (SideChange).** |
| F6 | Choices order: Breakthrough (1), MiddleShot (2), Cross (5) (elizacontroller.cpp:2046–2068); Long (6) and CurveShot (3) are admitted by their own evaluators | Order 2 (MiddleShot) before 3 (CurveShot) before 1 (Breakthrough), then 5, 6 | **Breakthrough checked before MiddleShot** in the choice list; PassThrough (4), SideChange (7), ShotPassFromGoal (8), BackDFLine (9) unrolled (documented as ponytail in pescarrier.hpp). CurveShot rolled since F25; Long since F23/F24. |
| F7 | First eligible success wins (elizacontroller.cpp:2072–2080) | Dispatcher: admission only when nothing is selected; cancel test (vtable[3]) each tick; selection persists (0x14211E2D0) | **No selection persistence** in GF; re-decided each rethink. Breakthrough cancel = Intention < 3 (0x14211A550) not ported. |
| F8 | `imageState = 0` always (elizacontroller.cpp:2174, "ponytail: no GF counterpart") | Intention ratchet 0x1421197B0 (states 0–4) | **Intention absent.** Every Intention gate is missing: Breakthrough ≥3 (0x14211A8??/0x14211A887), MiddleShot <3 (0x14211B4EB), CurveShot ≥3 (0x14211AFA1), Long veto 3/4 (0x14211A570), Long Ranger <3, Incisive ≥3, SB ≥3; the planner `imageState` branches (pescarrier.cpp:498–500, 563, 569) always see state 0. |

### 3.3 P-table and stats
| # | GF | PES | Mismatch |
|---|---|---|---|
| F9 | Breakthrough: `skill ? 70 : (com ? 20 : 20)` (pescarrier.cpp:594–599; caller elizacontroller.cpp:2037–2044) | grade==1→40; grade==2→70; DT→70; COM 1/2/4 → 20 **overrides** (0x14211A956–A9AE) | **Wrong order and missing grade terms.** GF has no dribble grade (sub_141513A50 not ported); COM does not override DT in GF; grade-1 → 40 missing. |
| F10 | Breakthrough stat: `technical_dribble`, divisor `statSpan` = 29 (pescarrier.hpp:162; cpp:590) | Three stats 0x17, 0x27, 0x28, divisor 20 (0x14211A9B3–AA7C) | **Wrong divisor (29 vs 20); one stat instead of three; no per-stat truncation.** |
| F11 | `breakthroughStatGain` **never assigned in production** (declared pescarrier.hpp:165, read pescarrier.cpp:584; only tests/sim/pes_carrier_test.cpp:128 sets 15) | gain 15 at 0x14211A9DB (0x14259BE0C) | **BUG: Breakthrough's stat term is 0 at runtime.** The test passes because it sets the gain itself. |
| F12 | `crossStatGain` **never assigned in production** (pescarrier.hpp:173 with a wrong VA comment "0x14259be2c"; read cpp:586; test sets 20 at tests/sim/pes_carrier_test.cpp:136) | gain 20 at 0x14259BE1C (0x14259BE2C = 40.0) | **BUG: Cross's stat term is 0 at runtime.** Also the VA comment is wrong. |
| F13 | MiddleShot: `longShotSkill` (Long Range Shooting, Knuckle, Dipping) as skill (elizacontroller.cpp:2053–2054) | skills 39, 8, 33, 34 (0x14211B5F4…B669) | **Skill 34 Rising missing.** |
| F14 | MiddleShot COM bit 5 passed as `com = false` (elizacontroller.cpp:2053–2054) | COM 5 → base 70 (0x14211B600) | **Missing COM 5 (Long Ranger).** |
| F15 | MiddleShot `state2 = false` always (elizacontroller.cpp:2054) | +40 if Intention = 2 or TeamData+47266 (0x14211B67C–686) | **+40 never applied.** |
| F16 | MiddleShot stat: `finishing` only (`technical_shot`, = Finishing 0x1A per pescarrier.hpp:212) | KP 0x2A and Finishing 0x1A, each `/29`, ×15, per-stat truncation | **Missing Kicking Power; no per-stat truncation.** |
| F17 | MiddleShot floor 5: `middleShotFloor = 5` (pescarrier.cpp:301, VA comment 0x14259BE7C) | immediate 5 at 0x14211B5E7 | Value MATCH; VA comment wrong. |
| F18 | MiddleShot gate `IsMid(zone)` (elizacontroller.cpp:2061) | Intention <3; `+2884==3` signed-X test; `+111 == 0`; facing ≤60° (0x14211B4EB–B5CD) | **Eligibility gate approximated** (region test instead of PES gates). |
| F19 | MiddleShot cap 80 then floor 5 (pescarrier.cpp:605–606) | cap 80 (0x14211B714), floor 5 (0x14211B71D) | MATCH (order and values). |
| F20 | Cross base: `(skill||com) ? crossBase(50) : 0` (pescarrier.cpp:609) | eval+0x54 base 1→50, 2→70; fallback 50 from four sources (0x14211AC4C–ACC2) | **Base wrong**: missing field-driven base 70, the regular style bit 7 (Cross Specialist), sub_14063A3A0 team setting, and the field precedence. |
| F21 | Cross stat `finishing` only (elizacontroller.cpp:2058) | KP 0x2A and Finishing 0x1A (0x14211AB10 +20/29 each) | **Missing Kicking Power.** |
| F22 | Cross eligibility `IsClose(zone)` (elizacontroller.cpp:2059–2061) | Region records 0x278/0x294 via 0x140698950; 11-term OR gate; roll skipped for designated / 47266 | **OR-gate and roll bypass missing**; region approximated. |
| F23 | Long score: `LongScore` (pescarrier.cpp) reads the engagement base 30/60/80, COM 6, skills 13/17 at 50 each, the team level at 20 a step up to 40, and the lofted ramp; Path B (`teamSetting`) takes 10 per skill and the ramp | Engagement 30/60/80 (0x14069A3E0+16); COM 6 +50; skills 13/17 +50 each; level +20/level max +40; Lofted 10·ramp/29; Path B if eval+0x3C≠0 (10 per skill, ramp) | **Ported.** Stand-ins (ponytails in elizacontroller.cpp): level 0 (eval+0x54 / BallPlayer+0x70 not carried), Path B off (eval+0x3C not carried), engagement = GF's nearest-opponent class (report 4.2) for sub_14069A3E0+16. |
| F24 | Long veto `0x14211A570` (Intention 3 or 4, engagement < 1) and the accepts (team+4==3, designated, 47266) | — | **Ported.** The signed-X and stored-slot tests of the veto are stand-ins (never veto). The designated accept reads the team's designated possession player (team.cpp, the carrier); team+4 and 47266 stand in as `false`. |
| F25 | CurveShot: admitted at priority 3 through `CurveShotAdmitted` (K 0x14260B388, skill 7 = LONG_RANGE_DRIVE) | 0x14211AF20 full logic (F-state ≥3, base 0/30/30/60, stats, floor 5/cap 70, seed 1300) | **Ported** (roll wired). Its cancel was always true (`planChoice(3)` null), so an admitted curve shot was dropped at the next rethink; the cancel is now `PlanStillHolds` (head gates). Trajectory, side check and 47266 stay as the existing stand-ins. |
| F26 | Stat ramp: `Clamp01((stat − statFloor)/statSpan)` (pescarrier.cpp:590) with statFloor 70 (0x14259BE4C) | Same floor; divisor 29 (Cross/MiddleShot/CurveShot/Long) and 20 (Breakthrough) | Floor MATCH; Breakthrough divisor MISMATCH (F10). |
| F27 | Per-stat integer truncation | Breakthrough/MiddleShot integer per stat; Cross/CurveShot/Long float-then-truncate per stat | GF has a single term, so it never truncates per stat. Moot until F13–F16, F21 and F10 are fixed. |

### 3.4 Smaller items
- `middleShotStateGain` loaded from 0x14211BCF9 (pescarrier.cpp ~300): wrong VA (`mov r8d,0x28`). Value 40 coincidentally matches. Moot while `state2` is always false (F15).
- `longStatGain = 10` hard-coded (pescarrier.cpp:303) and `middleShotStatGain = 15` hard-coded (pescarrier.cpp:302): values MATCH 0x142588210 / 0x14259BE0C.
- `statSpan` = 29 from 0x1425A9460 (pescarrier.cpp:256): MATCH for MiddleShot, Cross, CurveShot, Long.
- Test coverage: `tests/sim/pes_carrier_test.cpp:128,136` assigns `breakthroughStatGain` and `crossStatGain`, masking F11–F12.

---

## 4. Foundation structures affected (P-IMG, MB-*, T-*)

- **P-IMG** (carrier image and holder rows): the Intention state is BallPlayer+0x32B0 (+8), written by 0x1421197B0 (Subsystem3) and reset by 0x14210E5F0 → 0x1415595D0; the image manager is at +0x32C0 (0x14211E2D0). The image seed is the member at BallPlayer+0x78 (+0x18), not "a carrier clock". Status in GF: `imageState` stand-in = 0 (elizacontroller.cpp:2174). Re-tag row as "Intention ratchet and image manager: absent" with the new VAs above.
- **MB-H** (match setup and tick order): new open item. The RandomInfo producer (0x140435EF0) must run before the seed copies (0x14063A490) on the same tick. The producer's caller/callback (0x14041E2B0, unverified) and the publish step (0x140435C50, unverified) decide the order. Needs the callback decoded.
- **MB-F** (ball record and prediction): the ball sample 0 (`BallInfo+136`, 0x140A57800 / 0x140A6CF70) is the geometry base for MazingRun and ShotPassFromGoal (not re-verified here).
- **T-LVL** (level and strategy): posture bytes confirmed (+47266 full attack, +47267 full defence, +47268 gauge, writer 0x1404F6C30). Team setting bank at TeamData+36360 (sub_1408C5030; setting #1 read by Cross's fallback and Long's path split) is **not in any row**; candidate new row "T-SET".
- **T-POSS / T-LVL**: TeamData+46816 (Early Cross OR-gate term) remains unidentified (rentry §10); not resolvable here.
- **Consumers of these rows in GF**: pescarrier (P-IMG, S6/S7/S8 "shot aim / shoot sub-kind / pass chooser" rows cite P-IMG), S6, S7, S8 (foundation line 144–146) all read P-IMG.

---

## 5. GF port mismatches, consolidated (file:line → PES VA)

1. **pescarrier.cpp:619–622** (`PlanRoll`) seed source is possession clock, not RandomInfo+8 via member+0x18 → 0x14063A490, 0x14211AA8F.
2. **elizacontroller.cpp:2003–2004** `nowFrame` = possession duration; used as the roll clock → same as 1.
3. **pescarrier.hpp:386** priority table omits 7 → 0x14211E2D0 `local_38=7`.
4. **elizacontroller.cpp:2046–2068** choices order puts Breakthrough (1) before MiddleShot (2) → 0x14211E2D0 priority table.
5. **elizacontroller.cpp:2046–2068** CurveShot (3) absent → 0x14211AF20.
6. **elizacontroller.cpp:2072–2080** no selection persistence; no cancel test → 0x14211E2D0 (selected==0 gate; vtable[3] → 0x14211A550).
7. **elizacontroller.cpp:2174** `imageState = 0` → 0x1421197B0 (Intention ratchet).
8. **pescarrier.cpp:594–599** Breakthrough order: DT before COM and no grade terms → 0x14211A800 (asm 0x14211A93x–A9AE).
9. **pescarrier.cpp:590, pescarrier.hpp:162** Breakthrough divisor 29 → 20 (0x14259BE1C) at 0x14211A9DB.
10. **pescarrier.hpp:165 / pescarrier.cpp:584** `breakthroughStatGain` never set in production (test-only) → gain 15 (0x14259BE0C).
11. **pescarrier.hpp:173 / pescarrier.cpp:586** `crossStatGain` never set in production; VA comment wrong (0x14259BE2C = 40) → gain 20 at 0x14259BE1C.
12. **elizacontroller.cpp:2053–2054** MiddleShot: skill 34 missing; COM 5 passed false; state2 passed false → 0x14211B460 (B5F4…B686).
13. **elizacontroller.cpp:2058** MiddleShot/Cross stat: Finishing only; Kicking Power missing → 0x2A / 0x1A.
14. **pescarrier.cpp:302, 301** VA comments mismatch (0x14259BE0C OK; floor source is immediate 5 at 0x14211B5E7).
15. **pescarrier.cpp ~300** `middleShotStateGain` read from 0x14211BCF9 (wrong) → +40 at 0x14211B686.
16. **pescarrier.cpp:609** Cross base flat 50 → eval+0x54 precedence (0x14211AC4C–ACC2) with four fallback sources.
17. **elizacontroller.cpp:2059–2061** Cross eligibility approximated; OR-gate (11 terms) and roll bypass missing → 0x14211AB10 / 0x14211ADCC.
18. **pescarrier.cpp (`LongScore`)** Long base by engagement, COM 6, skills 13/17, level, Path B → 0x14211b289–b3cf, b35a–b3d4. **Ported** (level and Path B inputs stand in, see F23).
19. **elizacontroller.cpp (Long admission)** veto 0x14211a570 and accepts 0x14211b259–b279. **Ported** (signed-X and stored-slot stand-ins; team+4 and 47266 stand in as `false`; designated is live).
20. **elizacontroller.cpp (admit)** CurveShot rolled through `CurveShotAdmitted` (K 0x14260b388); its cancel is no longer always true (`PlanStillHolds`); skill 7 is LONG_RANGE_DRIVE, not Scotch Move (the engine bit order, section 6.1 of pes-re-carrier-ai.md). **Ported.**
21. **pescarrier.cpp:570–590 (statTerm)** single-stat design; no per-stat truncation → Breakthrough/MiddleShot integer per stat. **OPEN** (CurveShot already truncates per stat; the other plans do not).
22. **pescarrier.hpp (plan comment)** named the stats the code does not read. **Fixed**: it names what the code reads and where PES reads more.

Matches (no action): RNG generator (pesrandom.cpp:91–96); K values (cpp:253–256); RangeRaw(2,100) (cpp:621–622); plan base immediates 70/20/70/80/50/40/80/30/60/80 at the cited VAs (cpp:291–295, 597–599, 605–611); Breakthrough cap 70; MiddleShot cap 80 / floor 5 ordering; Cross cap 80 / floor 40 ordering; Long no floor/cap; `score ≥ draw` acceptance.

---

## 6. Items the write-up states that this report did not confirm

Not re-verified in this pass: the ball-sample functions (2.7); the animation property table (2.10); mask builders (1.5); RandomInfo "28 bytes", random block and the 0x141FE288C writer (3.16); callback 0x14041E2B0 and publish 0x140435C50 (3.15); Basic mode producer (4.5); Subsystem4 target helper (4.19); Mazing caller state gate and exclusions (6.2.2–6.2.3); ThinkUnitDribble trick tree and cooldown (6.0.3, 6.0.5); SB stage-2 boolean and branch a/b/c (6.1.4, 6.1.6); payload dispatch and movement tiers (6.1.8, 6.1.10); helper 0x14211FE20 (6.5.9); Incisive side check (6.4.2); eval+0x3C cache (6.6.5); team posture nudge (8.2); TeamData+46816 writer (8.4); the 339-second sample (5.12, 9); the test file (11).

---

## 7. Proposed docs/re/pes-re-com-styles.md (draft for archive_re.sh; sources cited)

```
# PES 2021 COM playing styles: image and intention system (verified)
Status: OBS unless marked INF. Binary: PES2021.exe (memory image pes_full.bin, base 0x140000000).
Source: rentry write-up, re-verified per section; see report pes-re-com-styles.md.

## Storage
- COM mask: live +0x4C (DWORD, 7 bits), roster +0x54. CheckCOMStyle_Live 0x14063E830 (playerId ≤0x15; ≥11 other team).
- Skills: live +0x40 (QWORD), roster +0x48. CheckSkill_Live 0x14063E960.
- Regular styles: live +0x48 (DWORD).
- Copy: 0x140A58F00 writes 80 bytes to live+0x70 (x+0x70).

## COM bit table (runtime bit, roster field, UI index) — 0x14352AC10
0 Trickster (60,0); 1 Speeding Bullet (62,2); 2 Mazing Run (61,1); 3 Early Cross (65,5);
4 Incisive Run (63,3); 5 Long Ranger (66,6); 6 Long Ball Expert (64,4). Names: short IDs
0x0160000B/0D/4C/12/4D/0F/13 (rows 0–6). UI field table 0x1429790D8 = 60..66.

## Seed and RNG
- RNG 0x140A923F0/0x140A928A0/0x140A924E0: s'=(17683−757248739·s)&0x7FFFFFFF; draw=trunc(float(s')·2⁻³¹·range).
- Accept if score ≥ draw. Draws for seeds 0..K are in 0..99.
- Seed = int(member+0x18 × K) on stream 2; K 1100/1200/1300/1500/1600.
- Member = BallPlayer+0x78. Update 0x14063A490 copies RandomInfo+8 (via gMatchContext+0x360) when member+0x18 < 0 or counter == 1.
- Reset 0x14063A3B0 (−1.0 at +0x18). Driver 0x142110C90 (predicate 0x14210E200; lea rcx,[rbx+0x78] at 0x142110CEB).
- RandomInfo producer 0x140435EF0: +0x28 int draw; +0x2C range 100; +0x2D range 2; +8 float = draw(999999)/999999.0.

## Intention (BallPlayer+0x32B0; state +8)
Updater 0x1421197B0. Init 1; reset 0x14210E5F0 → 0x1415595D0.
- 4 (absorbing): already 4, or sub_140699820(pred+2508) and |facing diff| < 100° (0x14253EE30).
- 3: already 3, or attackDir·(a4+0x40) > pitch[6], or (a5+2904)+10 > (a5+2876).
- 2: a6+4 < 2 and state ≠ 2 → corridor query block; otherwise 2 (latch).
- 1: TeamData+308 ≥3, or sub_140A62180 ≠ 1, or 2·GameSpeed ≤ a4+16.
- 0: fallthrough from the query block.
Transitions only upward (2→3→4); reset returns to 1.

## Image manager (BallPlayer+0x32C0)
Dispatcher 0x14211E2D0. Priority [9,8,2,3,1,4,5,6,7] (table 0x142BF02C0 + inline 7).
Admission only when selected==0; cancel vtable[3] (Breakthrough: Intention<3, 0x14211A550).
Clear when +0x116C==2 (gMatchContext+624+4460) and +0x11D4 ≠0.
Mode producers: Cross 4 (0x140E8CFF0); Breakthrough/MiddleShot/ShotPassFromGoal 1 (0x140A4C0F0);
BackDFLine 2 (0x141F05C10); Long/SideChange 8 or 2 (0x14211E140); CurveShot sticky (0x14211E0F0);
PassThrough 1/2 (0x14211E160; 3-tail unreachable).

## Per-image admission (P = percent)
| Image | Admission | Base | Stat ramp | Floor/cap | Seed K |
| Breakthrough 0x14211A800 | Int≥3; not 47266; angle≤60 | grade1→40; grade2 or DT→70; COM1/2/4→20 (override) | Dri 23, Spd 39, Con 40; /20; ×15; int-trunc each; sat 90 | cap 70 | 1100 |
| MiddleShot 0x14211B460 | Int<3; +2884==3 test; +111==0; facing≤60 | skill 39/8/33/34 or COM5 → 70, else 0; +40 if Int=2 or 47266 | KP 42, Fin 26; /29; ×15; int-trunc | 5/80 | 1200 |
| CurveShot 0x14211AF20 | Int≥3; not 47266 | COM4 +30; skill 7 +30 | KP, Fin; /29; ×10; float-trunc | 5/70 | 1300 |
| Cross 0x14211AB10 | region 0x278/0x294; 11-term OR; roll skipped for crosser/47266 | eval+0x54 1→50, 2→70; else 50 if sub_14063A3A0 / style bit7 / COM3 / skill14 | KP, Fin; /29; ×20; float-trunc | 40/80 | 1500 |
| Long 0x14211B1D0 | veto 0x14211A570 (Int 3/4; signedX; eng<1; slot>21); accept if team+4==3, designated, or 47266 | engagement 30/60/80; COM6 +50; skill13 +50; skill17 +50; level +20/level (max 40); Path B (eval+0x3C≠0): skill13/17 +10 each | Lofted 28; /29; ×10 | none | 1600 |

## Feint and dribble payloads
Action names: 3 DRIBBLE, 18 LONG_PASS, 19 CENTERING, 20 EARLY_CROSS, 21 CLEAR, 26 KICK_FEINT, 27 FEINT.
Feint kinds: 8 SCISSORS_IN, 9 SCISSORS_OUT, 10 SCISSORS_OUT_LOOP, 11 BODYFAKE, 12 KICKFEINT, 14 BURST, 15 MEIALUA, 17 DOUBLETOUCH, 18 ERACICO.

## Dribble grade (sub_141513A50, player,0)
mean(Dribbling 17, Speed 33, Accel 37) rounded; −2 if Dri or Spd <82; +≤3 for skills 67/68/69/99/104/105; grade 0 <83, 1 at 83–87, 2 ≥88.

## Team posture (TeamData)
+47268 gauge (int); +47266 = gauge ≥4 (full attack); +47267 = gauge ≤0 (full defence). Writer 0x1404F6C30.

## Open (not verified here)
Ball samples 0x140A57800/0x140A6CF70; RandomInfo callback/publish; Basic mode; Mazing caller gates; ThinkUnitDribble trick tree; payload dispatch 0x14210E880; helper 0x14211FE20; TeamData+36360 settings; TeamData+46816 writer.
```

---

## 8. Recommended next steps

1. Fix GF (not done here; this pass is decode-only): (a) seed the plan roll from the RandomInfo snapshot (member+0x18) instead of possession time, and port the member update rule; (b) set `breakthroughStatGain`/`crossStatGain` in production and correct divisors (Breakthrough 20, three stats); (c) correct the Breakthrough order (grade, COM overrides); (d) correct MiddleShot skills/COM5/state2/KP; (e) port the Cross base precedence and the 11-term OR gate; (f) port the Long engagement/COM6/skill13/17/level and the veto/accept rules; (g) port CurveShot; (h) fix the dispatcher order (add 7; MiddleShot before Breakthrough) and selection persistence; (i) port the Intention ratchet to replace `imageState = 0`. Replace the test-only gain assignments with a real loader check.
2. Decode the RandomInfo callback (0x14041E2B0) and publish path to settle the MB-H tick-order item.
3. Re-verify the unverified items in §6 before any archive; these are the remaining gaps in the write-up's per-style sections.
4. Any dt18-derived comparison: open `<PES21 Install>/Data/dt18_all.cpk` (set PES21_ROOT), not the 4cc overlay.

Blockers: none for the verified claims. Unverified items are listed in §6 with the reason (not fetched, or no function record in the bridge, or outside this pass). No code in src/ was edited; no commits; no sub-agents used.
