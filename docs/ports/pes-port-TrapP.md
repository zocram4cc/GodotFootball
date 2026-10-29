# pes-port-TrapP: the Trap chain's P is derived from GF state; the oracle runs on six captures with the action bytes compared (29-10-26)

Branch `StoppageFlow` (worktree `<workdrive>/gf-StoppageFlow`; `git ls-remote origin StoppageFlow` is the head after the final push). Slices S0, S1 and S1b of `docs/re/pes-re-trap-inputs.md` section 5/6. Tags: **CONFIRMED** = read in a tool run this session; **INF** = inference; **OPEN** = not settled (blocker named). `origin/main` merged first (`3eaf7056`, pushed).

## 0. Verdict against the acceptance

| Acceptance | State |
|---|---|
| Oracle on six captures with P derived from GF state (except the named open fields) | **DONE.** `pes_trap_oracle_test` 6/6 tests (six captures `pid463316/seq4469, seq4531`, `pid1467004/seq019`, `pid1981120/seq23879, seq23906, seq23932`); P is written by `pes::WriteActorP` (new `pestrapactor.{hpp,cpp}`); none of P's bytes is copied from the capture except the named open inputs (section 3) |
| S0: six captures, action object bytes compared | **DONE** (issued flag, out word, and every byte written to P+0xC80..+0xCDF; the clip is the first two bytes) |
| S1b: `P+0x377C` | **DONE, ported before** (`pes::MoveTopRate`, hand port) and now **bit-equal to PES**: 6/6 captured ceilings, and 132/132 players in the six full captures (section 2.3) |

## 1. S0 (CONFIRMED)

- Oracle directories regenerated for all six (the earlier ones were removed from scratch): `frame_trace.py recorded ... --spec-dir specs/core` (the trace needs the core spec dir to record the entry call itself), `snapdiff.py --only 0x14079A470` over the six, then `trap_oracle.py`. Scratch: `<workdrive>/gf-TrapP-scratch/` (`tr6.sh`, `sd6.sh`, `or6.sh`, `oracle/`, `rate22.py`).
- **Defect found in `trap_oracle.py`** (CONFIRMED): the first run on seq23879 aborted in `0x140A58550` (a PES hash-check trap). PES's slot records are closer than the window's record size (slot 20 and 21 are `0x2B90` apart, the table assumes `0x2E50`); `which()` classified slot 21's live bytes (`+0x9B, +0xC8, +0xE4, +0x104, +0x124`) as slot 20's. Fixed: an address belongs to the record that starts last at or before it. Not a GF window problem (the window's blocks are `0x3F40` apart).
- `EveryCapturedCallOnGfsWindowGivesPesOutput` now also compares the Trap action object's bytes: the last byte the reference log stores at each address of `P+0xC80..+0xCDF` against the window's write log, plus `want.empty() == !issued` (an issued call writes the action object, a refusal writes nothing there). Result 6/6 equal. Red: the action comparison passed on its first run (no red; the window already commits PES's clip), so it is guarded by the binding test (every flipped binding that changes the call changes the compared write string, which includes the action bytes) and by a test-side check only, not by an independent code mutation (section 5).
- **Finding, not resolved:** `snapdiff.py` on the six calls replays 6 of 6 and reports `mismatches=1` (`proven=False`): seq23906 case 0, field `w` only (`rax`, `xmm0` equal). The first diverging write (index 35588 of 82195) is a stack-local store below the entry rsp (harness writes the float `0x3DCCCCCD` at a stack slot where the translated function writes `0x3E9B3A42` at the next slot), and a few later scratch bytes differ (`0x11` vs `0x12`). The issued flag, out word and every action byte are equal. INF: a callee local whose spill differs (a candidate-evaluation temporary), not a result. Next step: lockstep the two runs (`ls4310.sh` style) on seq23906.

## 2. S1 and S1b

### 2.1 P derivation (`pes::WriteActorP`, `src/sim/onthepitch/pestrapactor.{hpp,cpp}`)

Inputs: `PesActorFacts` (slot, height cm, phase byte, action/sub/previous ids, motion, kick-approach word, the phase-1 words, the Speed byte, `StaminaState`), the actor's man record in the window, and `PesActorConstants` read from the image behind opcode checks. Each field and its writer:

| P field | Source | PES site | Verdict |
|---|---|---|---|
| `+0x8/+0x14/+0x20` | man `+0x55C/+0x568/+0x554` (new `kManPrevPosXOffset`) | decode 1.2 | CONFIRMED 6/6 equal to the capture |
| `+0x2458` | facts.slot | `FUN_1406EB670` store `0x1406EB6C1` | CONFIRMED 6/6 |
| `+0x245C` | facts.phase | MoveStep phase byte | CONFIRMED 6/6 (copy) |
| `+0xAB8/+0xAC0` | image: `lea rax,[rip+d]` before each store, `0x1406E0412` and `0x1406E0420` (opcode `48 8D 05` checked) -> `0x1425E3508`, `0x1425E35B0` | ctor `FUN_1406E03D0` | CONFIRMED 6/6 |
| `+0xAC8..+0xACB` | facts (PES ids) | `FUN_1406EEBB0` stores | CONFIRMED 6/6 (the captures have id == sub in both pairs; a distinct-value test pins the four bytes) |
| `+0xAE0` | facts.motion | `FUN_1406F0F70` path | CONFIRMED 6/6 (copy) |
| `+0xB08` | `FUN_1406D8BF0`: phase 0 -> `speed <= 5.0 ? 1 : 3` (root speed `FUN_1406EA110` = rate*60*60/1000*sqrt(dz^2+dx^2) from P+8 - P+0x14); phase >= 0x18 -> 0; phase 1 and `P+0x2780 > P+0x277C` -> 4; else `DAT_14344E1F0[4*phase]` (`0x1406D8C92` opcode `0F B6 84 8F`, guard `cmp byte,0x18` at `0x1406D8C59` checked) | decode 1.2 | table branch CONFIRMED 6/6 (phases 15,4,7,6,3,7); the other branches decompile-only (tested for structure) |
| `+0x430` | facts.kickApproach | adjuster A state | CONFIRMED 6/6 (value 0) |
| `+0x3AD8` | `SlotVa(slot) + 0x15C0` | `FUN_140925470` | CONFIRMED 6/6 (offset from the slot record) |
| `+0x3B18` | `height_cm / 180.0f` (`divss` at `0x1406EB231`, operand `0x142598580`) | `FUN_1406EB210` | CONFIRMED 6/6 bit equal |
| `+0x377C` | `MoveTopRate(Speed byte, stamina, G)` | `FUN_1406F99F0` store `0x1406F9B4C` | CONFIRMED 6/6 and 132/132 (2.3) |

Constants read from the image and the instruction that names them: standing speed `0x1406D8C35 movss xmm1,[0x14259222C]` = 5.0; `0x1406EA14E/0x1406EA15B mulss xmm8,[0x14259A764]` = 60.0 (checked equal); `0x1406EA168 divss xmm8,[0x14256AE34]` = 1000.0; `0x141E532D0 movss xmm0,[0x14351AE54]` = 54.0; `0x1406EB231 divss xmm0,[0x142598580]` = 180.0.

### 2.2 The oracle on derived P

`Build()` no longer copies P bytes or the `F` pointer cells: it copies the actor's man bytes `+0x554..+0x57F` (the chain does not read them; P is made from them) and calls `WriteActorP`. All six calls still equal PES (issued, out, action bytes). `DerivedPEqualsPesPOnEveryDerivedField` compares 16 field ranges plus the slot pointer per capture with the captured P, named per field. `EachWindowBindingChangesTheCall` (17 bindings, forked) still 17/17 on derived P.

### 2.3 S1b: the rate ceiling

- GF already ported `FUN_1406F99F0` (`pes::MoveTopRate`, `pesmovestep.cpp:574`); the TrapInputs doc said "not checked". Checked now: **the six captured `P+0x377C` values are bit-equal** to `MoveTopRate(Speed byte slot+0x97, StaminaState decoded from the slot record, G)`; and a scan of every `match::Player` (vtable `0x1426329A8`) in the six full captures gives **132 players, 132 equal** (G from 175 to 199 cm players, Speed 80..99). The slot record's hash-checked integers (`+0xC8` tank cur, `+0xE8` max, `+0x108` breath; FNV-1a-64 check, all pass) are decoded by `trap_oracle.py` (`checked_int`).
- Changed to PES's operation order (decompile `0x1406EAE00`, `0x1406F99F0`): `base` branches `G <= lo`, `hi <= G`, else `1 - ((G-lo)/span)*drop` with `span = [0x1425E4A18]` (new `topRateGSpan`, not `hi - lo`), and `drop - q*q*drop` (not `drop*(1-q*q)`); the reserve percentage is read as a byte (`bVar6` at `0x1406F9AC0`); `MoveTopRate` takes PES's Speed byte (40..99), the 0..1 scaling moved to its one caller (`pesmovestep.cpp`, ponytail stand-in unchanged).
- **The data cannot tell the two operation orders apart:** over the 132 players the old formula and PES's order give identical floats (0 differing cases). The order is the decompile's, not a capture finding.
- **Reserve branches are decompile-only:** every captured reserve is 100 percent, so the share term (`(bp-20)/-20`, `bp <= 0` -> 1) is tested by relation only (`PesStamina.MoveTopRateTakesTheReserveShareOffTheSpeedByte`).
- `P+0x3778 == MI+0x1198 - 1` (the per-tick guard): not written (the chain does not read it, and the per-tick guard belongs with the movement tick).

## 3. Named open inputs (the ones that stay captured; `PesActorOpenInputs`, each with the PES site that removes it)

1. **G and its pointer chain**: `P+0xA08` (pointer, heap objects in `ext.segments`) and the scale `*(*(P+0xA08)+0xF0)+0xC0` (`FUN_141AF3180`). Writer `FUN_141EB5810` from the body's `root_main` joint scale: OPEN (decode 1.4, 6.3). G also feeds `P+0x377C`; observed (cm, G): 175 -> 0.971146, 185 -> 1.027875.
2. **`P+0x2FC` ring** (12 bytes read, `FUN_141EB50B0` idx >= 2): writer OPEN (decode 1.3; next step the SetMotion hook `0x1406F0F70` or two SIGSTOP snapshots).
3. **`P+0x3AC0`** (`AnalyzeInfoRef`, vtable `0x14259D590`, `PM+0x7E0`): the chain reads fields behind it (the `+0x18` pointer 8 bytes and `+0xE0` in the heap segments), so it is not only "handed on" (corrects decode 1.2's row). GF has no such object; the heap bytes stay captured segments.

Also still captured (not P): the loader data heap segments (motion table, Animetable manager, `trap.o` registry object: TrapData's slice S2), and the facts that GF will map from its own state when wired (phase, action ids, motion, kick-approach word, `P+0x277C/+0x2780`, height, Speed byte, stamina): in the oracle they are read from the capture; the derivation from them is what is tested.

## 4. Not done (numbered; each names its blocker)

1. **Wiring** `WriteActorP` into `Match` (decode S1: "called from Match where the man record is written"): `WriteManMovement` writes position, velocity, facing but **not the previous position `+0x568`**, and GF has no phase-byte map (GF `MoveState` -> PES phase), no PES action-id map, no PES motion number per player at that site. Blocked on the MoveStep phase mapping and S3 (`PesTrapExternals::image` at run time).
2. The three open inputs above (G, ring, AnalyzeInfoRef).
3. `FUN_1406D8BF0` branches phase 0, phase >= 0x18, phase 1 and the reserve share: decompile-only (no capture); phase-1 words `P+0x277C/+0x2780` have no writer decoded.
4. `snapdiff` seq23906 write-log mismatch (section 1).
5. `P+0x3778` guard and the reserve at drained values: need a capture with a drained reserve (all six have bp 100).

## 5. Verification and mutations

- `pes_trap_oracle_test` 6/6 (`GF_PES_TRAP_ORACLE=<six oracle dirs>`, `PES_EXE=pes_full.bin`, `PES21_ROOT`), `pes_stamina_test` 16/16 (the topRate constants gained `topRateGSpan`; a new relation test).
- **Mutations (run, rebuilt, reverted; the P derivation ones were applied together, each killed by its own named assertion):** position <- previous, previous <- position, heading <- position x, slot + 1, phase + 1, interface vtables swapped, motion + 1, kick approach | 1, slot pointer for slot + 1, height * 180 instead of / 180, Speed byte - 1: each fails `DerivedPEqualsPesPOnEveryDerivedField` on all six captures by field name (slot, locomotion phase, interface vtable 0/1, motion, kick approach, position, previous position, heading, physique scale, rate ceiling, and the slot pointer line). Action/sub and previous pair swapped: survives the six captures (id == sub there), killed by `WriteActorPStoresEachFactInItsOwnField` (4 assertions). Phase-0 speed test inverted, z displacement dropped, phase-1 `>` -> `>=`: killed by `LocomotionClassBranches` (the z mutation alone). `MoveTopRate`: byte mask removed -> `MoveTopRateTakesTheReserveShareOffTheSpeedByte` fails (the 260 percent line); the share term zeroed -> that test and `DrainedReserveCapsTopSpeedAndAboveTwentyPercentItDoesNot` fail. **Survive by design (data cannot separate them):** `topRateGSpan` vs `hi - lo`, `drop - q*q*drop` vs `drop*(1 - q*q)`.
- Not mutated: the S0 action-bytes assertion on its own (section 1).
- Fast-tier line: see the final message.
