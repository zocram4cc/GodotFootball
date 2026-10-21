# PES attack level (mentality) → GF port plan (v3)

> **Main note 20-10-26:** plan v3 (MentalityPlan), read-only. It supersedes v2: the forward pull's array P (`[ctx+0xBA8]`) is the same pointer as RoleArrays' S = T+0xC48, so the pull reads RoleArrays B (and A for line membership); the pull is gated by the possession flag F (local_458+0x10C, inverted on even frames once MI+0x119C > 1), not the phase. Its corrections to pes-re-attack-level-pull.md are not yet Main-audited.

v3 supersedes v2. v2's base claims and gating were partly wrong. What changed:

1. The base chain is CONFIRMED end to end (decompile, asm, and an E8 scan of `pes_full.bin`). RE-16 is closed.
2. The pull's inputs come from the role arrays through P = S: A (`S+0x3F8C`) for line membership, B (`S+0x3FB8`) for the line sort, the pick, the forward switch and the B==2 gate.
3. The pull's branch is not the phase. It is the team-tick flag F, stored at `local_458+0x10C`. The defensive block (681FF0 → 682770, with the pull) runs when F = 0. F is the possession bit, except on one parity sub-path, which inverts it on even frames in play (§0e).
4. EC600 runs in phases 2 and 3 as well as phase 1, under the MI+0x1775 band (phase 3 on a 5-frame cadence). v2 said phase 1 only. GF runs its defensive block only in play, so F4 must lift that gate.
5. Three write-order divergences between PES and GF (W1–W3): the bulk-write gate, the head flag (with the parity inversion), and the tail swap.
6. Read-only. No code edits, no builds, no tests, no sim lock. I hold no jobs; `proc://` lists none of mine.

Verdicts: CONFIRMED = read this pass (disassembly, decompile, image bytes, or the E8 scan). INF = inference, with its reason. OPEN = blocker named. Sources: `docs/re/pes-re-attack-level-pull.md` (the spec); `pes_full.bin` (VA − 0x140000000, 462,929,920 bytes); GF `main` = `ed4cb7c`; `origin/ClipWords-integrated-wip` = `fbaef6f` (RoleArrays `8cbf2a1`, `8f57eda`; FUN_140a92950 port `9d31415`).

## Summary

1. **Dial.** `TeamInstructions::State::mentality` is PES's T+0xB8A4, levels 0..4 (identity, `pessetpieceteam.hpp:113-119`). Wired: set-piece counts, the Chance cap, the offball permit mask, the ForeCheck flag (T+0xB8A3). Not wired: the defensive shape. `teamAIcontroller.cpp:416` is the literal 2.
2. **Forward pull** (sub_140682770, reached via 681FF0): not ported. GF's out-of-possession forward target is the high end at every mentality (`teamAIcontroller.cpp:634-638`).
3. **Quota** (sub_140500200): ported with literal constants, matching the asm (ladder, ahead gate, −100 stamp, forward admission). ClipWords reads class B from `T+0x4C00` (CONFIRMED asm, 0x140500663; T+0x4C00 = S+0x3FB8).
4. **Base (CONFIRMED).** P = `[ctx+0xBA8]` = S = `T+0xC48` for the whole tick. The head and tail bulk writes, the pull and the master's CF test all address P. So the pull's inputs are RoleArrays A and B (§0f).
5. **Branch (CONFIRMED).** The defensive block runs when F = 0 (`local_458+0x10C`, tested at 0x1405ECC21). F = T+0x255 != 0 (the possession bit) on every path except one parity sub-path in play: once MI+0x119C > 1, F is inverted on even frames, so the pull runs for an in-possession team on even frames and for an out-of-possession team on odd frames (§0e).
6. **Phases (CONFIRMED).** EC600 runs in phase 1 on every tick (51D9E0 arg2 = 0), and in phases 2 and 3 under the MI+0x1775 band (arg2 = 1). So the pull can run in dead-ball and set-piece stoppages as well as in play.
7. **Divergences (CONFIRMED on the PES side; GF read).** W1: PES skips the bulk writes when MI+0x1775 != 0; GF always writes. W2: PES's head flag is F, including the parity inversion; GF's head phase is always 2, and its comment says the head takes flag 0. W3: PES's tail swap (T+0x89D4/0x89D8) exists; GF never swaps.
8. **Ready now:** Q1 (on the landing tree) and F1 (pure). Busy-file slices wait for MOVESTEP; level wiring waits for TeamSliceA; F4 waits for W1, W2 and the phase rules (RE-20, RE-25).

## §0 Image constants (script output, pasted)

Script: read 4 bytes at VA − 0x140000000 from `pes_full.bin`, unpack little-endian float32. The VA after each constant is where the disassembly read the operand. (Unchanged from v2.)

```
0x142592228 0000803e 0.25   forward factor        (0x140682BE4 MULSS XMM7,[0x142592228])
0x1425656c0 00008040 4.0    level clamp           (0x140682BD5 COMISS XMM7,[0x1425656C0])
0x14256ae1c 0000803f 1.0    unit                  (0x140682B59 MOVSS XMM9,[0x14256AE1C])
0x1425656d0 00000080 -0.0   XMM12 reload          (0x1406830BC MOVSS XMM12,[0x1425656D0])
0x14259bdf4 00000041 8.0    style pull            (0x1405E7318 MOVSS XMM15,[0x14259BDF4]; doc 0x1405E7AE3)
0x14259bdf0 0000e040 7.0    other term            (0x1406828B8 ADDSS XMM1,[0x14259BDF0]); NOT the style pull
0x14259222c 0000a040 5.0    window half-width     (0x1405E7723 MOVSS XMM0,[0x14259222C]; doc 0x1405E7B39)
0x14259d5d0 6666263f 0.65   rank scale            (0x1406832D9 MOVSS XMM6,[0x14259D5D0])
0x142598578 cdcc4c3e 0.2    rank scale            (0x1406830C7 MOVSS XMM15,[0x142598578]; 0x140682E30 MULSS XMM7,XMM15)
0x142591140 0000f041 30.0   pick bonus, CF        (0x14068305C ADDSS XMM6,[0x142591140]); dead for the pick
0x14259be84 000020c1 -10.0  pick bonus            (0x140683089 ADDSS XMM6,[0x14259BE84]); dead for the pick
0x1425c6614 0050c3c7 -100000.0 pick guard         (0x140682FA2 MOVSS XMM12,[0x1425C6614])
0x142588210 00002041 10.0   quota near edge; +10 m (0x14050059B MOVSS XMM15,[0x142588210]; 0x1406834BD MOVSS XMM2,[0x142588210])
0x14259be20 0000c841 25.0   quota far edge        (0x1405005AE MOVSS XMM7,[0x14259BE20])
0x14259d668 000070c1 -15.0  quota falloff divisor (0x1405005B9 MOVSS XMM12,[0x14259D668])
0x14259d654 000040c0 -3.0   quota turning side    (0x1405005CB MOVSS XMM10,[0x14259D654])
0x142588218 000080bf -1.0   sign                  (0x140500470 MOVSS XMM14,[0x142588218])
0x14259be0c 00007041 15.0   turning threshold     (0x1405004F0 COMISS XMM1,[0x14259BE0C])
0x14259be34 00004842 50.0   midpoint scale       (0x140682DE9 MULSS XMM8,[0x14259BE34])
0x14253ee30 0000c842 100.0  normaliser            (0x140682CA8 MOVSS XMM13,[0x14253EE30])
0x142565600 0000003f 0.5    half                  (0x140682CB1 MOVSS XMM15,[0x142565600])
0x14259a760 00000c42 35.0   ceiling               (0x140683536 COMISS XMM1,[0x14259A760])
0x1425c48c0 0000b041 22.0   ceiling               (0x140682EF8 COMISS XMM0,[0x1425C48C0])
0x14259be04 00004041 12.0   ramp span             (0x140682F08 DIVSS XMM6,[0x14259BE04])
0x142565604 00000040 2.0    base                  (0x140682B79 SUBSS XMM0,[0x142565604])
0x14257a194 3333333f 0.7    weight                (0x140682CD7 MOVSS XMM11,[0x14257A194])
```

Immediates (CONFIRMED asm): quota ladder `MOV byte [RSP+0x20],imm` = 0/2/4/6 for L = 0..3 (0x140500321, 0x14050031A, 0x140500313, 0x14050030C); L ≥ 4 `CMP EBX,0x4` at 0x140500235; forward stamp `MOV dword [RSP+RAX*8+0x44],0xC2C80000` = −100.0 (0x140500693); class gate `CMP EDX,0x7` (0x140500663).

## §0b Switch and jump tables (script output, unchanged from v2)

```
table 0x140683D60 (pick arm, JMP RCX at 0x140683025):        class 1-6 -> 0x140683099 (skip); class 7,8,9 -> 0x140683027 (arm)
table 0x140683D3C (line class, JMP RCX at 0x140682D61):      class 1,2 -> EDX=0 (0x140682D71); class 3-6 -> EDX=1 (0x140682D63); class 7,8,9 -> EDX=2 (0x140682D6A)
table 0x140683D18 (A-array class, JMP RCX at 0x140682A2A):   class 1,2 -> 0 (0x140682A3A); 3-6 -> 1 (0x140682A2C); 7-9 -> 2 (0x140682A33)
table 0x140683D84 (mid loop, JMP RCX at 0x140683160):        class 1,2 -> 0 (0x140683178); 3-6 -> 1 (0x140683162); 7-9 -> 2 (0x14068316D)
```

Class names (`pesstyle.hpp:25`, `pesroles.hpp`): GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9. Forwards = {7,8,9}. The directive doc's "6/7/8 arms" is wrong (C3).

## §0c (v3) Base chain — CONFIRMED

| step | VA | fact | evidence |
|---|---|---|---|
| a | 0x1404F6D02 | the only call to FUN_14051C840: `[ctx+0xBA8] := param_4 = T+0xC48` | E8 scan; decompile of 1404F6C30 l.174-175; decompile of 14051C840 l.5 |
| b | 0x14051D9E0 | FUN_14051D9E0's `param_1` is `int *`, so `*(undefined8*)(param_1+0x2ea)` = `*(ctx+0xBA8)` = P (`0x2EA*4 = 0xBA8`) | decompile signature `int *param_1`; the bulk-write argument |
| c | 0x14051DA5A (head), 0x14051E2E7 (tail) | the bulk writer FUN_1404F5F10(P, sheet, MI, flag) stores A and B at P | decompile of 51D9E0 l.37-38 and l.350-352; role-writer §1 |
| d | 0x1405EC9C4 – 0x1405EC9FC | the EC600 copy: `LEA RAX,[R9+RCX*4]` with R9 = R8 − (RBP+0x2A0), so the address is P+0x3FB8+4k, k = 0..10 | asm; R8 = `[R15+0xBA8]` at 0x1405EC788 and 0x1405ECEA6; R15 = ctx (`MOV R15,RCX` at 0x1405EC65E); no write to R8 in between |
| e | 0x1405ECEC7 → 0x1405F4AF0 → 0x1405DB800 → 0x1405E6E80 | P passes unchanged: EC600 `R8 = [ctx+0xBA8]`; F4AF0 `MOV RBP,RCX` (0x1405F4B0A), `MOV R8,[RBP+0xBA8]` (0x1405F4CD4); DB800 `MOV R12,R8` (0x1405DB842); the master's arg3 = R12 (`MOV R8,R12` at 0x1405DBE8F) | asm |
| f | 0x1405ECC44 | 681FF0(ctx, local_458, …) → 682770(local_458, P, …) | decompile of 681FF0 l.66; asm 0x1405ECC44 |
| g | 0x1404F6EE8, 0x1404F6EFF | restart block (MI+0x1775 == 0x17): FUN_14051D9E0, then the teardown `[ctx+0xBA8] := 0` (0x1404F6EFF), then return | E8 scan; decompile of 1404F6C30 l.239-247 |
| h | 0x1404F76CC, 0x1404F7A80 | the non-restart block: FUN_1404F9DC0 (step 37, 0x1404F76CC), then the step-54 teardown (0x1404F7A80), then clear T, return | E8 scan; decompile l.482-483 and l.638-642 |

**Verdict.** Every EC600 call sees P = S: the init precedes each EC600 call, and each teardown follows it. RE-16 is closed. On the tick path P is set and cleared only by these three stores. Computed-base stores were not excluded (role-writer §4), so RE-1 stays open, narrowed.

## §0d (v3) Call graph — CONFIRMED (E8 scan of `pes_full.bin`)

| callee | call sites | caller |
|---|---|---|
| FUN_14051C840 (alias init) | 0x1404F6D02 | the tick |
| FUN_14051B3E0 (alias teardown) | 0x1404F6EFF, 0x1404F7A80 | the tick (restart block; end of non-restart block) |
| FUN_14051D9E0 (role rebuild) | 0x1404F6EE8 | the tick, restart block |
| FUN_14051D9E0 | 0x1404F9F24, 0x1404FA237, 0x1404FA6DB | FUN_1404F9DC0 (three call sites, §0e) |
| FUN_1404F9DC0 (parity dispatch) | 0x1404F76CC | the tick, non-restart block |
| FUN_1405EC600 (defence row) | 0x14051DE03 (arg2 = 0), 0x14051E264 (arg2 = 1) | FUN_14051D9E0 |
| FUN_1404F5F10 (bulk A/B writer) | 0x14051DA5A (head), 0x14051E2E7 (tail), 0x141FBB06A (setup) | FUN_14051D9E0; setup |
| FUN_1404F6C30 (tick) | 0x14041EDD7 | the frame |
| FUN_1405EBFA0 (stores the flag) | 0x1405EC705 | EC600 |
| FUN_140681FF0 | 0x1405ECC44 | EC600, flag F = 0 |
| FUN_14067B640 | 0x1405ECC2A | EC600, flag F ≠ 0 |

## §0e (v3) Flag F, phases and bulk-write gates — CONFIRMED unless marked

- **The flag F reaches the defensive-block branch.** 51D9E0 passes its param_3 as EC600's arg4 (51D9E0 l.91 and l.153: `FUN_1405ec600(param_1, 0|1, param_2, param_3)`). EC600 holds arg4 in EDI and R9 (0x1405EC663 `MOVZX EDI,R9B`; 0x1405EC6CE `MOVZX R9D,DIL`). The call at 0x1405EC705 passes it to FUN_1405EBFA0 with RCX = local_458 (`LEA RCX,[RBP-0x80]` at 0x1405EC6F6). EBFA0 stores it: `MOVZX R15D,R9B` (0x1405EBFCD) and `MOV byte [RDI+0x10C],R15B` (0x1405EC092). So `local_458+0x10C` = F. EC600 tests it at 0x1405ECC21 (`CMP byte [RBP+0x8C],R14B`; RBP+0x8C = local_458+0x10C): F = 0 → JZ 0x1405ECC31 → FUN_140681FF0 (E8 at 0x1405ECC44); F ≠ 0 → FUN_14067B640 (E8 at 0x1405ECC2A). FUN_682770 tests the same byte as `CMP byte [R12+0x10C],0` at 0x14068386B (R12 = local_458).
- **The branch is the flag, not the team's possession.** F = 0 runs the pull. Outside the parity sub-path below, F = (T+0x255 != 0), so F = 0 means out of possession. This matches `pes-re-defensive-shape.md:187` (`ctx+0x10C == 0` → out-of-possession block). Inside the sub-path F is inverted on even frames (next bullet).
- **F's value.** On the restart path, F = (T+0x255 != 0): 1404F6C30 passes `bVar22`. On the non-restart path, FUN_1404F9DC0 receives param_2 = the low byte of `local_394`. That is 1 only in phase 1 (MI+0x116C == 1) with MI+0x119C > 1, and 0 otherwise (1404F6C30 near 0x1404F6D65–0x1404F6DAA). MI+0x119C is the "raw kick sum" in GF's `team.cpp` comment. With param_2 = 0 (the usual case in other phases), F = (T+0x255 != 0) (9DC0 l.61). With param_2 = 1, 9DC0 calls on odd MI+0x1198 with F = (T+0x255 != 0) (l.212), and on even MI+0x1198 with F = (T+0x255 == 0) (l.68). So in play, once MI+0x119C > 1, the pull runs for an out-of-possession team on odd frames and for an in-possession team on even frames. That alternates the two shapes (INF on intent). OPEN: RE-25.
- **Phases.** 51D9E0 calls EC600 with arg2 = 0 when MI+0x116C == 1 (E8 at 0x14051DE03). It calls with arg2 = 1 when MI+0x116C == 2, or == 3 with MI+0x1198 % 5 == 0, and MI+0x1775 is in the band (< 9, or > 0x10 and != 0x18) (E8 at 0x14051E264). The role of arg2 inside EC600 is OPEN (RE-26): it reaches FUN_1405F9CD0 as its param_2.
- **Bulk-write gates and flags.** Head: gate `MI+0x1775 == 0` (0x14051DA3B, role-writer §3.1); call 0x14051DA5A with flag = param_3 = F (l.37-38). Tail: gate `MI+0x1775 == 0` (0x14051E2A8, role-writer §3.1); call 0x14051E2E7 with flag = (T+0x255 != 0) (l.350-352), not F. The tail swap (0x14051E31E, FUN_140A422C0 flag 1, role-writer §4b) needs T+0x255 != 0 and FUN_140A41BE0(S), with arguments T+0x89D4 and T+0x89D8 (l.353-356).
- **Order inside 51D9E0 (CONFIRMED):** head bulk (l.37-38) → EC600 row 0 or row 1 (l.91, l.153) → tail bulk (l.350-352) → tail swap (l.353-356).

## §0f (v3) What the pull reads — CONFIRMED

| input | VA | array | copy or live |
|---|---|---|---|
| line membership (class → line) | 682770 0x1406829F6 `LEA R11,[R15+0x3F8C]`; 0x140682A03 `MOV EAX,[R11]` (R15 = arg2 = P; `MOV R15,RDX` at 0x1406827DC) | A | live |
| line sort: count (+0x58) and active (+0x59), in FUN_140682320 | 0x140682337 `MOV RDI,RDX` (RDX = local_458); 0x1406823B0 `MOV RAX,[RDI+0x128]` and 0x14068240B `MOV RCX,[RDI+0x128]` | B | copy: `local_458+0x128` = &local_2A0 (set at 0x1405ECA0A) |
| protected pick: class | 682770 0x140682FF2 `MOV RAX,[R12+0x128]`; 0x140682FFA `MOV ESI,[RDI+RAX]` | B | copy |
| forward switch: class | 682770 0x140682D32 `MOV RAX,[R12+0x128]`; 0x140682D3A `MOV ECX,[R14+RAX]` | B | copy |
| B == 2 gate (final target) | 682770 0x1406834DE `MOV ECX,[RAX+R8*4+0x3FB8]`; 0x1406834F9 `CMP ECX,2` (RAX = arg2 = S) | B | live |
| master's CF exemption | E6E80 0x1405E73C9 `MOV EDI,[R15+RAX*4+0x3FB8]` (R15 = arg3 = P, reloaded at 0x1405E73A6); 0x1405E7B25 `CMP EDI,9` | B | live |

**Consequence (INF, from the A/B split in RoleArrays).** After the tail swap B changes and A does not, so line membership (A) and the sort, pick and switch (B) can disagree. The port must keep the split and test it (W3).

## §1 GF consumers (v2 table, with these updates)

- Row 15 (forward/line index): `teamAIcontroller.cpp:441, 625, 654` use `PesLineIndexForRole(GF role)`. F4 replaces them with the A and B classes. The midfield catch-up at `:441` is one of them.
- **New row 18 (CONFIRMED).** `Team::Process` runs `WriteRoleArrays(true)` unconditionally (`team.cpp:527`) and `WriteRoleArrays(false)` unconditionally (`team.cpp:574`). The defensive block is `TeamAIController::Process` → `CalculateDefensiveShape` (`teamAIcontroller.cpp:252`), and `Team::Process` calls `teamController->Process()` only when `IsInPlay() && !IsInSetPiece()`. So GF's pull runs in play only. PES's runs in phases 2 and 3 too (§0e).

## §2 Rule → GF mapping (v3)

Columns: PES rule (doc §) · PES evidence (VA, verdict) · GF change (file:line) · GF today · missing data (§3) · RoleArrays input (A/B) · pinning test (§5).

| ID | PES rule (doc §) | PES evidence | GF change | GF today | Missing | RoleArrays | Test |
|---|---|---|---|---|---|---|---|
| R1 | Dial = T+0xB8A4, rungs 0..4 (§1) | store 0x1404F86E7; gauge strings 0x1425C3B64 | none | identity, wired | — | no | existing `pes_setpiece_team_test.cpp:431-433` |
| R2 | Level drift and flags (§1, §6) | 0x1404F8350; 0x1404F6E1C–58 | TeamSliceA (already written) | sheet level only | D12 | no | slice L0b |
| R3 | Forward factor = clamp(level,0,4)·0.25 (§2) | 0x140682BB7–0x140682BEC; 0.25 @0x142592228; 4.0 @0x1425656C0 | new `pesforward.cpp` `ForwardFactor`; wire at `teamAIcontroller.cpp:625-638` | none | D12 | no | F1 `FactorIsLevelQuarter` |
| R4 | Forward target = hi + (lo−hi)·f·s, i.e. deep + (high−deep)·factor (§2) | line-2 path 0x140682DD3–0x140682E59; hi/lo filled by 0x140687FB0 (0x140682896) and 0x1406855F0 (0x140682901) | F2 `ForwardTarget` | `line + len` (high end always) | D4 | yes (band rows: INF S+0x7B10 base) | F2 `TargetIsDeepPlusFactorSpan` |
| R4b | Per-slot targets fold by MAXSS into running maxima, initialised to −100000: `[RSP+0x4C]` (path B, 0x140682F84) and `[RSP+0x50]` (path A, 0x140682E4D) (CONFIRMED) | init 0x140682C91/0x140682C98 | the line target is a max over forwards | none | D4, RE-3 | yes (via the targets) | F2 `LineTargetIsMaxOverForwards` (after RE-3) |
| R5 | Protected forward = last eligible in slot order; the first counter or advance fallback ends the scan (§2) | pick loop 0x140682FA2–0x1406830AE; fallback 0x1406830B0 → 0x1406830B3; guard COMISS 0x140683091, CMOVNC 0x140683095 | `pesforward.cpp` `ProtectedSlot` (pure) | none | D2, D3 | **B copy** (eligibility) | F1 `ProtectedIsLastEligibleForward`, `CounterTargetStopsTheScan` |
| R6 | Eligible = on-pitch and class ∈ {7,8,9} (§2) | arm table 0x140683D60 (idx 6–8) | `ProtectedSlot` input | `PesLineIndexForRole` (GF role) | — | **B copy** | F1 `ArmsAreSevenEightNine` (reads the table at runtime) |
| R7 | Line membership and sort (§2) | membership: A (0x1406829F6). Sort/count: B copy (682320, 0x140682337) | `pesforward.cpp` `LineOfClass`; the port keeps A and B separate | GF formation role | — | **A and B** | F1 `LineMapFromTable`; W3 for the split |
| R8 | Rank scales for unprotected forwards, level < 4 (§2, §5). Path A (line-2 target): ×0.2 iff the scalar `[obj+0x128] > 2`, obj = `[R12+0x118]` (gate 0x140682E02), predicate (0x140682E1B), level (0x140682E24, `CMP R13B,4`), scale at 0x140682E30. Path B (pair): count ≥ 3 (0x140683285), not protected (0x14068328A: R13D = protected slot), mode-0 predicate (0x1406832A0), level < 4 (0x1406832B1); mode-1 predicate (0x1406832C6) → ×0.2 (0x1406832CF), else ×0.65 (0x1406832D9) | `pesforward.cpp` `RankScale` (predicate injected) | none | D3, D5 | predicate's 2nd arg = S (R15 at 0x140682E18, 0x140683297, 0x1406832C6) → yes | F3 `RankScalesOnlyWithThreeForwards`, `LineTwoScaleNeedsScalarAboveTwo` |
| R9 | Floor fmax(−ref, target) on the **protected** forward only (§2, §5). Arm 0x140683340 (CMP R13D,ESI), floor 0x140683363 (MAXSS), re-applied 0x1406837B6; ref `[[ctx]]+0x44` (0x14068335A); gate BL (C10) | — | `pesforward.cpp` `ProtectedFloor` | none | D6 | no | F3 `FloorOnProtectedOnly` (gate OPEN) |
| R10 | Instruction 15 adds +10 m to the line-0 (back) target (team-layer doc §6) | 0x14068348E (R10 = 0); LEA R8D,[R10+0xF] 0x14068349F; CALL 0x140A63950 0x1406834B8; ADDSS +10.0 0x1406834C9 | F3 hook | none | D2 | no | F3 `ReadyCounterAddsTenMetresOnBackLine` (injected) |
| R11 | Style pull +8 m toward own goal, every forward past two gates (§3). Gate 1: AL, `TEST AL,AL; JNZ 0x1405E7B40` (0x1405E7ACD). Gate 2: `CMP dword [RBP+0xC],0; JZ 0x1405E7B40` (0x1405E7AD1), where [RBP+0xC] = FUN_1405E0010(arg2 = local_458, k = 1) (call 0x1405E718E, result stored 0x1405E7198). Then `MULSS 8.0` (0x1405E7AE3), `ADDSS XMM8` (0x1405E7AEE). The designated-CF gate FUN_140A63680 (0x1405E7AFA) follows and gates only the window (R12) (C16) | `pesforward.cpp` `StylePullMetres`; wire in the master-equivalent slot target (`teamAIcontroller.cpp:~640-660`) | none | D7, D16, D18 | gate 2: RE-17 | no | S1 `EveryForwardPastGatesGetEightMetres` |
| R12 | +5 m window (clamp half-width) on forwards that are not CF, not Fox, not designated-CF, and have the S+0x80D4 byte clear (§3, §4). CF: `CMP EDI,9` (0x1405E7B25), EDI = B (live). Fox: `CheckStyleAtPosition(…, EDX = 2)` (0x1405E7B13), `JNZ 0x1405E7B40` (0x1405E7B1A). Designated-CF: FUN_140A63680 (0x1405E7AFA), `JNZ 0x1405E7B40` (0x1405E7B01). Byte: `CMP byte [R15+RAX+0x80D4],0` (0x1405E7B2E). Window `ADDSS XMM6,XMM9` (0x1405E7B39), XMM9 = 5.0 (0x1405E7B1C) | `pesforward.cpp` `WindowMetres` | none | D7, D13, D17, D18 | **B (CF)** | S2 `WindowFiveMetresForNonExempt` |
| R13 | Fox exemption = style bit 2 | `pes::style::Gate` exists (`pesstyle.hpp:34`; EngineBit FoxInTheBox = 2, `pesstyle.cpp:17-21`) | reuse `Gate`; class from B | Gate present, unused by the master | D15 | **B** | S2 `FoxSkipsWindow` |
| R14 | Quota ladder: need = 0/2/4/6 for L = 0..3; all for L ≥ 4 (§4) | 0x1405002F4–0x140500321; 0x140500235 | none | ported (`pesoffball.cpp:1641-1643`) | — | class: B | Q1 `QuotaLadderByLevel` |
| R15 | Forwards force-permitted (BTS at 0x1405006A2) and stamped −100.0 (0x140500693) (§4) | forward path 0x140500663–0x1405006A6 | class test `roleClass ≥ 7` | ClipWords ported (`pesoffball.cpp:1665-1667`); main uses `position` | — | **B** | Q1 `ForwardsAdmittedAndCountTowardNeed` |
| R16 | Ahead gate: if max(FUN_140A4F230 count, front-first beyond count) < need, permit all (§4, C5) | CALL 0x140A4F230 at 0x140500341; compare 0x1405003ED; JNC 0x1405003F1 → all-slot fill 0x1405003F3–0x1405003FD; else continue 0x140500402. Beyond count: loop 0x140500370–0x1405003D6 | none (ported `pesoffball.cpp:1644-1652`) | ported | RE-15 | no | Q1 `AheadGateAdmitsEveryone` |
| R17 | Non-forward score: line-list float − 5·falloff over 10..25 m; −3 when turning and same side (§4) | score load 0x1405006DF; falloff 0x140500721–0x14050074B; side test 0x14050074F–0x14050078E | Q1 constants from the image; score source (RE-14) | literals 10/25/−15/5/−3 (`pesoffball.cpp:1680-1683`); score = `pesFieldHalfLength − s·x` (INF) | D10, D14 | class: **B**; score source: no | Q1 `FalloffAndSideAreImageConstants` |
| R18 | Turning test: \|team+0x250 − receiver heading\| > 15.0 (§4, C9) | team+0x250 load 0x1405004B8; CALL 0x14043A0A0 0x1405004D9; COMISS 15.0 0x1405004F0 | Q3 | velocity vs facing (`pesoffball.cpp:1664-1668`) | D10 | no | Q3 `TurningUsesHeadingNotVelocity` |
| R19 | Receiver id = FUN_1405969B0 (§4, C8) | restart taker (MI+0x173C/0x1740 under MI+0x1554 == 5 and MI+0x1558 == team), holder on team, MA+0x2483C, else T+0x78 | Q2 (`teamAIcontroller.cpp:2393-2397`) | carrier / last touch / designated | D9 | no | Q2 `ReceiverOrder` |
| R20 | L1–3: FUN_1405AAF00 true → permit every slot with class > 1; no quota (§4, C6) | CALL 0x1405AAF00, TEST AL,AL 0x140500262, JZ 0x1405002CA; mask loop 0x140500266–0x1405002C5 | Q4 (branch in `AttackSlotMask`) | never true (stand-in) | D8 | class: **B**; predicate: no | Q4 `PermitAllAboveOneWhenPredicateTrue` |
| R21 | b8a3: level < 1 blocks positions 0..4 (non-forward through-ball) | ported (`pesoffball.cpp:1734`) | re-base check (RE-13) | ClipWords `roleClass` at `:1724` | — | **B** (verify) | Q5 `LevelZeroBlocksClassFourOrLess` |
| R22 | Level drives the defensive shape: line, length, width, steal radius (`pes-re-defensive-shape.md:296`: live level 2 for both teams) | FUN_1405F9EB0 ramp; length 46/42 lerp; width −6·ramp (doc) | L1: replace literal 2 (`teamAIcontroller.cpp:416`); steal `:960-962` | literal 2 | D12 | no | L1 `DefensiveLineFollowsMentality`, `StealRadiusFollowsLevel` |
| R23 | Carrier run count reads the level (`pescarrier.cpp:2227-2228`) | — | L2: `elizacontroller.cpp:2918` = `pes::AttackLevel(mentality)` | literal 0 | — | no | L2 `CarrierTouchCarriesMentality` |
| R24 | Dial steers registry style state (0/1 → 1, 2 → flag, 3/4 → 0) (§4) | 0x140501061–0x140501121 | none yet | none | RE-15 | no | deferred |
| R25 | Equal drops of two strikers; the Goal Poacher partner reason (§5) | doc §5 (dead reasons, audited) | no port of the partner; pin the equal-drop shape | — | — | no | F2 `TwoForwardsAtMinusOneShareLine` |
| R26 | Live check (§6): the COM byte stayed 2; the retune claim unsupported in that window | doc §6 | validation item | — | RE-12 | no | live frame |
| R27 (v3) | The defensive block (and the pull) runs when F = 0. F is the possession flag except on the parity sub-path, where it is inverted on even frames in phase 1 once MI+0x119C > 1 (§0e). F = 1 → FUN_14067B640 (unported) | 0x1405ECC21–0x1405ECC2A; 1404F6C30 `local_394`; 9DC0 l.61/68/212 | F4: call the block when the PES flag (with the W2 rule) is 0 | the block runs in play, for every team (`team.cpp`: `Process` gate) | RE-23, RE-25 | no | F4 `DefenceBlockRunsOnTheFlagZero` |
| R28 (v3) | EC600 runs in phase 1 on every tick (arg2 = 0), and in phases 2 and 3 under the band and cadence (arg2 = 1) (§0e) | 51D9E0 l.40–41, l.98–100; E8 0x14051DE03, 0x14051E264 | F4 gate by phase, not by IsInPlay | in play only (`team.cpp`, `Process`) | RE-20 (band), MI+0x1198 (frame) | no | F4 `DeadBallRunsTheBlockOnTheBand` |
| R29 (v3) | Bulk writes are gated on MI+0x1775 == 0; head flag = F, tail flag = T+0x255 (§0e) | 51D9E0 l.37, l.350; VAs 0x14051DA3B, 0x14051E2A8 | W1, W2 | always written (`teamAIcontroller.cpp:2323`, comment) | RE-20, RE-22, RE-25 | **yes (A and B)** | W1 `StoppageFreezesRoleArrays`; W2 `HeadPhaseFollowsFlag` |
| R30 (v3) | Tail swap: B entries swap (flag 1) when T+0x255 != 0 and FUN_140A41BE0(S); args T+0x89D4, T+0x89D8 (§0e) | 51D9E0 l.353–356; 0x14051E31E | W3 (`RequestRoleSwap`, no caller in GF) | never swaps | RE-21 | **yes (B only)** | W3 `TailSwapChangesBNotA` |

## §3 Missing data (v3)

| ID | Data | PES source | GF today | Lands with RoleArrays? | RE |
|---|---|---|---|---|---|
| D1 | (v3) **Resolved for the arrays.** The pull reads A (line membership) and B (sort, pick, switch, gates) through P = S. Open: P re-pointing (RE-1, narrowed) | §0c | none | **A and B** | RE-1 |
| D2 | Counter-target record (instruction id 15) and id 2 (designated CF); advanced-instruction table (+0x25CF8/+0x25D38) | tester FUN_140A63950 (0x140683038, 0x1406834B8); FUN_140A64030 | none ("no advanced instructions", `teaminstructions.hpp:1-3`) | no | RE-6 |
| D3 | Scalar bytes of the per-tick object `[ctx+0x118]` (the RBP+0xD0 struct): +0x129 (advance; ≤ 1 → fallback, 0x140683049–0x140683050) and +0x128 (path A gate > 2, 0x140682E02; pick gate, 0x140683064) | as listed | defaults (advance > 1; scalar not modelled) | no | RE-5 |
| D4 | Line-2 band rows: lo = `[RBP−0x48+4·line]`, hi = `[RBP−0x3C+4·line]`, filled by FUN_140687FB0 (0x140682896) and FUN_1406855F0 (0x140682901); read at 0x140682DD3, 0x140682DD9. INF: source S+0x7B10+0x14·line (+0x3C in possession, defensive-shape EV-DEF-0009) | fills and reads | back and mid bands only (`teamAIcontroller.cpp:457-469`) | no | RE-3 |
| D5 | Rank predicate FUN_1406405C0: calls at 0x140682E1B (path A), 0x140683080 (pick, dead), 0x1406832A0 (path B, mode 0), 0x1406832C6 (mode 1). 2nd argument is S (R15) | — | none | inputs yes (S); body no | RE-2 |
| D6 | Floor reference `[[ctx]]+0x44`; floor gate BL = FUN_140687A80's return (CALL at 0x140682C74; stored `[RSP+0x20]` at 0x140682C9F) | 0x14068335A, 0x140683350 | none | no | RE-4 |
| D7 | Style-pull gate 1: the AL value at 0x1405E7ACD (upstream predicates, not traced) | doc §3; asm | none | no | RE-10 |
| D8 | FUN_1405AAF00 sub-predicates (holder FUN_140A627E0; FUN_140A50250(…,2); RNG FUN_141E532D0; FUN_140A57800; FUN_140A4C9A0(…,−12.0 @0x14259D664); FUN_140A4C720 (instruction-2 gate); SPACE_RUN scan over T+0xA2A0; thresholds DAT_14259D620, DAT_1425A9BF0, DAT_14259D5D4; MI+0x11A0) | decompile of 0x1405AAF00 | none (predicate = false) | no | RE-7 |
| D9 | Receiver: FUN_140A6E320 (restart-taker predicate), FUN_140A627E0 (holder), MA+0x2483C, T+0x78 | decompile of 0x1405969B0 | carrier / last touch / designated | no | RE-8 |
| D10 | Turning operands: team+0x250 (GF uses the attack direction, `teamAIcontroller.cpp:2467`), FUN_1404AB8E0 (receiver heading), FUN_14043A0A0 (fold); side operands | 0x1405004B8–0x1405004F7; side 0x14050052F–0x14050053F | velocity vs facing | no | RE-9 |
| D11 | pesPenaltyAreaLineX = `[[0x1436F3DC8]+0x2AA8]+0x18` | 0x14050035A–0x140500368 | GF constant (value not compared) | no | RE-15 |
| D12 | Attack level per team = drifted T+0xB8A4 (PES path) | TeamSliceA FUN_1404F8350 | sheet mentality; literal 2 for defence | TeamSliceA not on ClipWords | L0b |
| D13 | Designated slot = P+0x3EF8 (during a tick). Read by the master after the reload at 0x1405E9166 (RDI = arg3); FUN_140A63680 (D18) is the designated-CF test. Writer OPEN | §0c; `CMP dword [RDI+0x3EF8],R13D` at 0x1405E94EC, 0x1405E97AE | "none" (TeamSliceC notes) | P-block yes; writer no | RE-18 |
| D14 | Quota score source: line-list record float | FUN_140A714E0 records; load 0x1405006DF | `pesFieldHalfLength − s·x` | no | RE-14 |
| D15 | Per-player style dword (doc: player+0x48) | `CheckStyleBitmask` (doc §3) | one style per player → one bit (`pesstyle.cpp:17-21`) | no (unless the roster carries several bits) | RE-10 |
| D16 | (v3) FUN_1405E0010(arg2 = local_458, k) for k = 0, 1, 2 (calls at 0x1405E717E, 0x1405E718E, 0x1405E719E). Gate 2 is k = 1. Body OPEN | asm above | none | no | RE-17 |
| D17 | Directive word P+0x80A0 (init 0 by FUN_140A411C0, role-writer §1); master reads it after the reload at 0x1405E9166 (0x1405E91D8, 0x1405E9AF9). Writer FUN_1405DB000 (p4 = S or T: OPEN) | as listed | none | P-block yes; writer no | RE-19 |
| D18 | FUN_140A63680(ctx, slot): the designated-CF gate (doc: fires only with instruction record id 2 armed). Called at 0x1405E7AFA (window only) | as listed | none (no instruction records) | no | RE-6 |
| D19 (v3) | F's computation on the 9DC0 parity sub-path: param_2 = 1 only in phase 1 with MI+0x119C > 1 (`local_394`, 1404F6C30 near 0x1404F6D65); an even MI+0x1198 inverts F (9DC0 l.68) | 9DC0 l.61, l.68, l.212 | no equivalent | no | RE-25 |
| D20 (v3) | The MI+0x1775 map: what each value means (stoppage kinds), and the GF state that corresponds | 51D9E0 l.37, l.98–100, l.350; 1404F6C30 l.239 (0x17) | GF stand-in: always open (`teamAIcontroller.cpp:2323` comment) | no | RE-20 |
| D21 (v3) | Tail-swap writer for T+0x89D4 and T+0x89D8, and when the swap fires | 51D9E0 l.353–356 | no caller (`RequestRoleSwap`) | no | RE-21 |
| D22 (v3) | The sheet's phase rows f = 0, 1, 2 for each slot and plan (`sheet[((plan·11+slot)·3+f)+10]`, role-writer §5) | role-writer §5 | one role for all phases | no | RE-22 |
| D23 (v3) | FUN_14067B640 (the in-possession block) | EC600 l.315–316 | not ported | no | RE-23 |
| D24 (v3) | Role of EC600's arg2 (0 or 1) inside the function (passed to FUN_1405F9CD0 as param_2) | EC600 l.264 | not ported | no | RE-26 |

## §4 Corrections to docs/re/pes-re-attack-level-pull.md (and to v2)

- **C1 (v3, REVISED).** The pull's class inputs are A (line membership, 682770 at 0x1406829F6) and B (sort, pick, switch and gates). Both come from P = S. v2 said B only. The doc's "not array B at +0x4C00" is wrong, and so is its "raw TeamData" for r15 (arg3 = P).
- **C2.** The protected scan is last-wins until a fallback slot. The fallback is a counter-target test or the scalar advance byte `[[ctx+0x118]+0x129] ≤ 1` (one byte per tick, 0x140683041–0x140683050). The directive doc §2.4 wrongly makes advance per-slot.
- **C3.** The pick arms are classes 7, 8, 9 (table 0x140683D60, idx 6–8). The directive doc's "6/7/8" is off by one.
- **C4.** The forward path BTS-es the forward into the mask and writes its −100 entry into the sorted list (0x1405006A2, 0x140500693). The first `need` sorted entries take the quota, so forwards count toward it. The doc's "top 2 non-forwards" is wrong. GF `pesoffball.cpp:1673-1687` is right.
- **C5.** The quota applies only if max(FUN_140A4F230 count, front-first beyond count) ≥ need; otherwise every slot is permitted (0x1405003ED, 0x1405003F1, 0x140500402).
- **C6.** If FUN_1405AAF00(w, team) is true (TEST AL,AL at 0x140500262), the mask is every slot with class > 1 and the quota is skipped (0x140500266–0x1405002C5). The predicate has an RNG draw and an instruction-2 gate. GF stubs it false.
- **C7.** Quota 0 is right, and forwards are still admitted through the BTS path, so the level-0 mask is the forward set alone.
- **C8.** The "−3 designated side" uses FUN_1405969B0's id: restart taker, else holder on team, else MA+0x2483C, else T+0x78 designated.
- **C9.** The turning test is |team+0x250 − receiver heading| > 15.0 (0x14259BE0C, 0x1405004F0). GF's velocity-vs-facing test is a different quantity.
- **C10.** The floor gate at 0x140683350 is `TEST BL,BL` with BL = FUN_140687A80's return, stored at `[RSP+0x20]` (0x140682C9F). "level != 0" is INF until FUN_140687A80 is decoded.
- **C11.** 8.0 is the style pull (0x14259BDF4). 7.0 (0x14259BDF0) is another term at 0x1406828B8. The community-QA "7.0 pull" stays refuted.
- **C12.** The counter target exits the scan (the fallback is a break). "Last live forward" holds only when no fallback slot exists.
- **C13.** `0x140500663` `CMP EDX,7` reads TeamData+0x4C00 = B, not the registry position. RoleArrays provides that class.
- **C14.** The rank scale has two paths: path A (line-2 target; needs the scalar `[[R12+0x118]+0x128] > 2`; no count test) and path B (the pair; needs count ≥ 3 and not protected). The doc merges them.
- **C15.** The "designated" in the doc's §5 and in the Main audit for 0x14068328A, 0x14068328D and the floor (0x140683340) is the protected slot: R13D, set by the pick loop. It is not the designated man (P+0x3EF8).
- **C16 (v3, REVISED).** The +8 gate's call is FUN_1405E0010(arg2 = local_458, k = 1), not (ctx, k). The arg6 statement stands: DB800 passes arg6 = 0 (0x1405DBE86), and arg6 only selects the pair loop (0x1405E7020).
- **C17 (v3).** The doc's §3 "Correction" paragraph says the master's r15 is raw TeamData and that its +0x3FB8 is the position-family array. That does not survive §0c step e: arg3 = P = S.
- **C18 (v3, REVISED).** The doc's "row" is not the phase. The branch byte is the flag F (EC600 arg4), stored at `local_458+0x10C`. F = 0 runs the defensive block with the pull; F ≠ 0 runs FUN_14067B640 (§0e, CONFIRMED). F is the possession bit except on the parity sub-path, which inverts it on even frames in play. `pes-re-defensive-shape.md:187` describes the possession reading.
- **C19 (v3, NEW).** The pull is not phase-1 only. EC600 runs in phases 2 and 3 under the MI+0x1775 band and frame cadence (51D9E0 l.98–100, l.153). A team out of possession gets the pull in dead-ball and set-piece stoppages as well as in play.
- **C20 (v3, NEW).** GF's comment on `WriteRoleArrays` (`teamAIcontroller.cpp`, the block above the function) says "The head takes flag 0". PES's head flag is F (51D9E0 param_3), so GF's head phase differs when possession is set. GF's head phase is `WritePhase(false)` = 2 always. The same comment says the gate is open; PES's is MI+0x1775 == 0 (W1).

## §5 Implementation plan (RE/TDD slices)

Dependency order: landing L0 (combined tree = ClipWords-integrated-wip + GoalKick + ThrowInPathB; Main's batch) and L0b (TeamSliceA, not yet in the combined tree) come from Main. RE tickets run in parallel with L0. Each TDD slice: red test on the landing tree first, the smallest change, mutation check per branch, rebuilt. Tests needing the install read it from PES21_ROOT and `GTEST_SKIP` when unset. Constants are read at runtime from the install (`PeImage`). No numeric literal in code. Clean cutover: the stand-in a slice replaces is deleted in the same change.

**Verification tier (Main's directive, effective now).** Per commit: the unit suites the change touches, plus the ONE sim test that reproduces its bug. Before a push: the 2-min smoke (seed 7; `--pes-loop` if the PES path changed). Not per commit: the 20-min restart test, full restart lists, 20-min smokes, ctest-all (Main's gate.sh, once at the landing). A long test runs only when it is the sole reproduction, and the report says so.

| Slice | Goal (PES site) | RE prerequisite | Files | Red tests (numbers; mutation) | Depends on | Busy file? | RoleArrays input |
|---|---|---|---|---|---|---|---|
| L0 | Combined landing (RoleArrays into main) | — | Main | — | Main batch | no | brings A and B |
| L0b | TeamSliceA (drift, flags, restart word) | — | `pesteamtick.*`, `teamAIcontroller.cpp` | sketched in `pes-re-team-layer.md` §3 A: `Level3SetsFullAttack`, `Level0SetsFullDefence`, `LevelFrozenWhileDeadBall` | L0 | **yes** | no |
| Q1 | Quota: image constants, ladder, ahead gate, forwards (C4, C5) | none (CONFIRMED) | `pesoffball.cpp` (`AttackSlotMask`), `pesoffball.hpp:238-240`, `tests/sim/pes_offball_test.cpp` | `QuotaLadderByLevel` (need 2/4/6; mutate need(3)=5); `ForwardsAdmittedAndCountTowardNeed` (3 forwards at L1 → those 3; mutate no BTS → 3rd missing); `AheadGateAdmitsEveryone` (1 ahead at L2 → 11; mutate gate off → 4 non-forwards); `FalloffAndSideAreImageConstants` (10 m vs 25 m gap = 5.0; mutate 3.0) | L0 | no | B |
| Q5 | Level-0 b8a3 on the class base (RE-13) | RE-13 | `pesoffball.cpp:1724-1734` | `LevelZeroBlocksClassFourOrLess` (class 4 rejected at L0, class 7 admitted; mutate block 7) | L0, RE-13 | no | B |
| F1 | Pure forward module: factor, arms {7,8,9}, protected scan, line map (R3, R5–R7) | none | new `pesforward.{hpp,cpp}`, new `tests/sim/pes_forward_test.cpp`, CMake | `FactorIsLevelQuarter` (f(L) = L·0.25; mutate 0.2); `ProtectedIsLastEligibleForward` (eligible {1:P9, 4:P7} → 4; mutate first-wins → 1); `CounterTargetStopsTheScan` ({1:P7 counter, 4:P9} → 1; mutate no-break → 4); `AdvanceAtMostOneStopsTheScan` (advance 1, {1:P8, 4:P9} → 1); `ArmsAreSevenEightNine` (read 0x140683D60 → {7,8,9}; mutate {6,7,8}); `LineMapFromTable` (read 0x140683D3C) | none (pure; A and B are inputs) | no | input |
| F2 | Line-2 target hi + (lo−hi)·f·s (R4); max-fold (R4b) | RE-3 | `pesforward.cpp`, tests | `TargetIsDeepPlusFactorSpan` (deep −20, high 0, f 0.25 → −15; mutate f 0.5 → −10); `TwoForwardsAtMinusOneShareLine` | F1, RE-3 | no | no |
| F3 | Rank scales (paths A, B), floor (protected only), +10 m hook (R8–R10) | RE-2, RE-4, RE-5, RE-6 | `pesforward.cpp`, tests | `RankScalesOnlyWithThreeForwards` (path B: count 3, level 1 → ×0.2; mutate 0.65); `LineTwoScaleNeedsScalarAboveTwo` (path A: scalar 2 → none; 3 → ×0.2; mutate threshold); `FloorOnProtectedOnly` (protected −30, ref 25 → −25; mutate all forwards); `ReadyCounterAddsTenMetresOnBackLine` (+10.0 = 0x142588210; mutate +5) | F1, F2, RE-2/4/5/6 | no | input (S) |
| S1 | +8 pull for every forward past gates 1 and 2 (R11) | RE-17 (gate 2), RE-10 (gate 1) | `pesforward.cpp`, tests | `EveryForwardPastGatesGetEightMetres` (classes 7/8/9 → −8.0 toward own goal, 0x14259BDF4; mutate gate 2 off); `NonForwardsNeverPulled` (class 6 → 0) | F1, RE-17, RE-10 | no (pure) | B |
| S2 | +5 m window with exemptions (R12, R13) | RE-6 (FUN_140A63680), RE-10 (S+0x80D4) | `pesforward.cpp`, tests | `WindowFiveMetresForNonExempt` (0x14259222C; mutate exempt Fox); `FoxSkipsWindow` (`pes::style::Gate(compat, class, word, EngineBit(FoxInTheBox))`); `CfSkipsWindow` (B value 9); `DesignatedCfSkipsWindow` | S1, RE-6, RE-10 | no | B |
| W1 | Bulk-write gate parity: skip the A and B writes when the PES stoppage state is non-zero (R29) | RE-20 | `teamAIcontroller.cpp:2323` (`WriteRoleArrays`), `team.cpp:527,574` | `StoppageFreezesRoleArrays` (B and A unchanged while the gate is closed; mutate gate always open → fail). Unit test on `WriteRoleArrays` with a stoppage state: no sim test needed | RE-20 | **yes** | A and B |
| W2 | Head flag: F (possession; inverted on even frames in phase 1 once MI+0x119C > 1, §0e), not the constant 0 (R29, C20) | RE-22 (are the sheet phase rows different?), RE-25 (the inversion) | `teamAIcontroller.cpp:2323` (phase argument), `team.cpp:527` | `HeadPhaseFollowsFlag` (head phase = 2 − possession; mutate constant 2 → fail, once RE-22 shows rows differ); `EvenFrameInvertsTheFlagInPlay` (after RE-25; mutate no inversion → fail) | RE-22, RE-25 | **yes** | B |
| W3 | Tail swap: B entries swap, A untouched, guarded by the latch and the S-guard (R30) | RE-21 | `teamAIcontroller.cpp:2323` (`roleSwap`), `RequestRoleSwap` callers | `TailSwapChangesBNotA` (A unchanged; mutate swap both → fail); `SwapSkippedWhenGuardFails` | RE-21 | **yes** | B only |
| Q2 | Receiver = FUN_1405969B0 (R19) | RE-8 | `teamAIcontroller.cpp:2393-2397` (BuildRunWorld), tests | `ReceiverOrder`: restart taker > holder on team > MA fallback > T+0x78 (mutate swap holder/fallback) | RE-8, MOVESTEP | **yes** | no |
| Q3 | Turning = heading vs team direction; −3 on same-side candidates (R18, C9) | RE-9 | `pesoffball.cpp:1664-1668`, RunWorld heading field, BuildRunWorld | `TurningUsesHeadingNotVelocity`; `SideAgainstAimPenalisesSameSide` (−3.0 = 0x14259D654; mutate opposite side) | RE-9, Q2, MOVESTEP | builder yes | no |
| Q4 | L1–3 permit when FUN_1405AAF00 true (R20, C6) | RE-7 | `pesoffball.cpp` (`AttackSlotMask`), `teamAIcontroller.cpp:2452-2495`, `pesrandom.hpp` | `PermitAllAboveOneWhenPredicateTrue` (mask = class > 1; mutate quota); `PredicateFalseKeepsQuota` | RE-7, Q1 | builder yes | B |
| L1 | Mentality drives the defensive line, length, width and steal radius (R22) | L0b (or an interim mentality), RE-12 | `teamAIcontroller.cpp:413-416, 425, 472, 658-661, 960-962` | `DefensiveLineFollowsMentality` (mentality m → `defensiveAttackLevel == m`; mutate literal 2 at m ≠ 2); `StealRadiusFollowsLevel` (20 m below level 3; mutate threshold 2) | L0b or interim, MOVESTEP | **yes** | no |
| L2 | Carrier touch carries the mentality (R23) | none | `elizacontroller.cpp:2917-2918` | `CarrierTouchCarriesMentality` (touch.attackLevel == AttackLevel(m); mutate 0) | MOVESTEP | **yes** | no |
| F4 | Wire the pull into the defensive block, with the flag F and the phase rules (R3–R10, R27, R28) | RE-1 (P), RE-3, RE-17, RE-20 (band), RE-25 (parity), RE-23 (scope) | `teamAIcontroller.cpp:325-335` (delete mixup offsets), `:625-638` (replace), `:252` (gate), `pesforward` calls | `DefenceBlockRunsOnTheFlagZero` (flag 0 → pull; mutate invert → fail); `DeadBallRunsTheBlockOnTheBand` (phase 2, band open → pull; band closed → none); `ParitySubPathFlipsTheFlag` (after RE-25; mutate no flip → fail); sim: a level-0 out-of-possession team's forwards at the deep anchor, level 4 at high (one sim test; the sole reproduction of the level effect) | F1–F3, S1–S2, L0b, W1, W2, MOVESTEP | **yes** | A and B |
| X | Cutover deletions | with F4, L1, L2, Q2 | stand-ins at `teamAIcontroller.cpp:416`, `:2393-2397`, `:325-335`, `elizacontroller.cpp:2918`, the `PesLineIndexForRole` calls | — (no shim) | the slice that replaces each | yes | — |

**Order.** Q1 (after L0) and F1 now (no busy files). RE-1, RE-3, RE-5, RE-6, RE-7, RE-8, RE-9, RE-10, RE-17, RE-20, RE-21, RE-22, RE-23, RE-25 and RE-26 in parallel. Then L0b and MOVESTEP land. Then F2, F3, S1, S2, W1, W2, W3, Q2, Q3, Q4, L1, L2, F4 and X, in that order.

## §6 Open RE items (v3)

| # | Question | Blocks | Cheapest decisive test |
|---|---|---|---|
| RE-1 (narrowed) | Can P (`[ctx+0xBA8]`) be re-pointed away from S during a tick? On the tick path the direct stores are the init and two teardowns (§0c); computed-base stores are not excluded | the A/B-from-P claim (§0c) | Read-only: `[ctx+0xBA8]` at EC600 entry (0x1405EC788) on a play tick and a restart tick; equal to T+0xC48 ⇒ closed for those ticks |
| RE-2 | FUN_1406405C0 body (rank predicate; the R9B mode argument; 2nd argument S) | F3 | Decompile; test with two synthetic states |
| RE-3 | Line-2 band rows `[RBP−0x48]`/`[RBP−0x3C]` (fills by FUN_140687FB0 and FUN_1406855F0; INF source S+0x7B10+0x14·line); consumer of the maxima `[RSP+0x4C]`/`[RSP+0x50]` (R4b) | F2, F4 | Decompile the fills; trace where the maxima are stored |
| RE-4 | FUN_140687A80 return (floor gate BL); floor reference `[[ctx]]+0x44` | F3 | Decompile; a value read on a live tick |
| RE-5 | Writers of the scalar bytes of the per-tick object (+0x129 advance; +0x128 path gate) | F1 (≤ 1 branch live), F3, F4 | Static scan of the object's writers (5FBCB0 / 5F9CD0 / 5FBE10 family) |
| RE-6 | Instruction records (ids 2, 15): the advanced-instruction table's writer; bodies of FUN_140A63950, FUN_140A63680, FUN_140A64030 | F3, S1, S2, Q4 | Find the in-match writer. Until then GF stays "none" (ponytail: FUN_140A63950 and FUN_140A63680 = false) |
| RE-7 | FUN_1405AAF00 sub-predicates (D8); RNG draws per call | Q4 | Decompile each; count RNG draws per tick on a live team |
| RE-8 | FUN_140A6E320 (restart taker) and FUN_140A627E0 (holder) bodies | Q2 | Decompile |
| RE-9 | FUN_1404AB8E0 (receiver heading pointer), FUN_14043A0A0 (fold), team+0x250 meaning | Q3 | Decompile; compare with GF's attack-direction stand-in |
| RE-10 | Gate 1 of the +8 pull (the AL value at 0x1405E7ACD); the per-slot byte S+0x80D4 writer; the exits 0x1405E7B40 vs 0x1405E7B49 | S1, S2 | Decompile the master upstream of 0x1405E7ACD; find the S+0x80D4 writer |
| RE-11 | The "B == 2 and level ∈ {3,4}" term at 0x1406834DE–0x140683553 | F4 (full final-target loop) | Decompile the 0x140683553 block |
| RE-12 | Attack-level source for AI teams: the doc's live check (COM byte stayed 2) vs GF UpdateStrategy vs the sub_14051E970 retune claim | L1 | Read `docs/re/pes-re-match-management.md`; a live sample with a non-2 strategy |
| RE-13 | Is the b8a3 test (pos ≤ 4) on class or registered position? | Q5 | Decompile the caller at 0x140606CD0 |
| RE-14 | Score source of the quota list (FUN_140A714E0 record float) and the sort comparator | Q1 exactness | Decompile the 0x140A714E0 record builder |
| RE-15 | FUN_140A4F230 vs GF `AttackersAhead`; pesPenaltyAreaLineX value | Q1 | Compare GF bodies with the decompile |
| RE-16 (CLOSED) | The ordering of the alias and the EC600 calls | — | Closed by §0c and §0d: the init precedes both EC600 call paths; the teardowns follow them |
| RE-17 | FUN_1405E0010(arg2 = local_458, k): the k = 1 result's meaning; the body | S1 (gate 2) | Decompile FUN_1405E0010; read its switch table |
| RE-18 | Designated slot writer (P+0x3EF8) | D13; the master's designated tests | Static writer scan for the P-block `+0x3EF8` (computed base excluded?); live read |
| RE-19 | Directive word writer FUN_1405DB000: which object is p4 (S or T)? The init says S+0x80A0; the directive doc says T | D17 | Decompile FUN_1405DB000's caller; compare with the init store |
| RE-20 (NEW) | The MI+0x1775 map: each value's meaning; the GF stoppage state that matches; the band for phases 2 and 3 | W1, F4 (phase rules) | Decompile the writers of MI+0x1775; a live read of B across one restart (read-only) |
| RE-21 (NEW) | The tail-swap writer (T+0x89D4, T+0x89D8) and when the swap fires (T+0x255 and FUN_140A41BE0) | W3 | Find the computed-base writer of the pair; live read of A vs B after one swap |
| RE-22 (NEW) | The sheet's phase rows f = 0, 1, 2 for the default formation: do they differ, and for which slots? | W2 (is GF's single phase equivalent?) | Read the registry values from the install's team data (`sheet[((plan·11+slot)·3+f)+10]`) |
| RE-23 (NEW) | FUN_14067B640 (the in-possession block): what it computes | F4 (scope note); the full shape | Decompile it |
| RE-24 (CLOSED) | Writer of `local_458+0x10C` (the branch byte) | — | Closed by §0e: EC600 arg4 → FUN_1405EBFA0 (R15B = R9B) → `[local_458+0x10C]` |
| RE-25 (NEW) | MI+0x119C's meaning (GF's comment calls it the raw kick sum) and its writers; the purpose of the even-frame inversion at 9DC0 l.68 (INF: alternating the two shapes); how often the sub-path runs in play | W2, F4 (the flag) | Decompile MI+0x119C's writers and the 1404F6C30 `local_394` assignment; read MI+0x119C on a live play tick (read-only) |
| RE-26 (NEW) | Role of EC600 arg2 (0 or 1): passed to FUN_1405F9CD0 as param_2 | D24 | Decompile FUN_1405F9CD0 |

## §7 Coordination

- **Busy files** (no edits until the MOVESTEP landing): `teamAIcontroller.cpp/.hpp`, `elizacontroller.cpp`, `match.cpp`. Slices L1, L2, Q2, Q3 (builder), F4, W1, W2, W3 and X touch them.
- **Free now:** `pesoffball.cpp/.hpp`, new `pesforward.{hpp,cpp}`, `tests/sim/pes_offball_test.cpp`, new `tests/sim/pes_forward_test.cpp`. Slices Q1, F1, F2, F3, S1, S2 touch only these. Q1 needs the landing tree (B is absent on main).
- **Tick order to preserve in F4 (§0e, CONFIRMED):** head bulk write (A, B) → EC600 (copy of B; sort; pull when the flag is 0) → tail bulk write → tail swap. The tail swap changes B only.
- **Live reads:** read-only only. Running PES: no writes, no pins of MI+0x1168/0x116C/0x1574. RE-1, RE-18, RE-20, RE-21 and RE-25 need read-only memory checks.
- **Behaviour risks.** L1 changes the defensive line for every level other than 2; the live sample had both teams at 2 (doc §6), so L1 is unvalidated live for other levels. F4 changes the shape in dead balls, which GF does not run today, and reproduces the parity alternation in play. Both need a frame before a smoke claim.
- **Archive:** Main archives this report into `docs/re/` when delivered.

## §8 Verification status (this pass)

- **Read:** `decompile_function` of 1404F6C30 (whole), 51D9E0 (whole), 9DC0 (whole), 14051C840, 14051B3E0, 681FF0 (partial), EC600 (partial), 682320 (grep); `disassemble_function` of EC600 (copy loop, branch, EBFA0 call), 682770 (A and B reads, gates), E6E80 (arg3 reloads, B reads, +8/+5 gates), DB800 (R12, R14, R15 setup), EBFA0 (R15B store), F4AF0 (P reads). GF: `team.cpp` (whole), `teamAIcontroller.cpp` (lines 1–554 and 2050–2603 on ClipWords), `pesroles.cpp` (whole, on ClipWords).
- **E8 scan** of `pes_full.bin` (462,929,920 bytes) for nine callees (§0d).
- **Not done:** live reads (RE-1, RE-18, RE-20, RE-21, RE-25 need them); the decompile of FUN_14067B640, FUN_1405E0010 and FUN_1405F9CD0; the body of FUN_140682320; the DB800 prologue beyond the R12/R14/R15 moves. The bridge's xref endpoint returns 404 for code addresses; the E8 scan replaced it. The role-writer's gate VAs (0x14051DA3B, 0x14051E2A8) and the tail-swap VA (0x14051E31E) are cited from `pes-re-role-writer.md` and were not re-read this pass.
- **No builds, no tests, no sim lock, no jobs of mine.**
