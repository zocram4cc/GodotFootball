# PES port: RestingBall6: CVTPS2DQ proven, the 661-member action-4 closure and two anime-interface leaves translated and proven, the trap re-issue root-caused (receive gate 3's carrier clause); every failing test's match finishes (06-11-26)

Branch `RestingBall3` (worktree `gf-RestingBall`), merged with `origin/main` `406ab7e2`. Image `pes_full.bin`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Pushed through `14d47a87` (`git ls-remote origin RestingBall3`).

## 1. Commits
| SHA | What |
|---|---|
| 512398f8 | merge of `origin/main` 406ab7e2 (conflicts: tests/CMakeLists.txt, tools/dev/README.md, both resolved to main's order; one stale "not folded in" bullet dropped) |
| (CVT commit) | `xlate.py` CVTPS2DQ lowering + `PesState::cvt_i32` byte-identical to KickMotion's; `test_sse_forms.py` case |
| 43ea4e50 | five read-only tables declared; the 661-member closure of `0x1407A38E0` regenerated (layer 5650) |
| 2aefbfdd | receive gate 3's carrier clause (`CarrierMayTouch`), wiring, tests |
| 14d47a87 | `0x1406E9F10` and `0x1406E9F90` translated and proven; layer 5652 |

## 2. CVTPS2DQ (item 1): kept, proven
- RestingBall5's uncommitted lowering duplicated KickMotion's (`origin/KickMotion` 9ba19ea4: same helper, `do_cvtv`). I dropped mine and applied KickMotion's three hunks verbatim (`pes_state.hpp`, `xlate.py`, `test_sse_forms.py`), so either branch merges textually identical.
- `test_sse_forms.py` 4/4 (harness Python): 2.5 -> 2, 3.5 -> 4, -2.5 -> -2, 3e9 -> 0x80000000. **Mutation run:** `nearbyint` -> `trunc`: `[2, 3, 4294967294, 2147483648] != [2, 4, ...]`; reverted.
- Hardware check (scratch, not committed): `cvt_i32((double)f)` against `_mm_cvtps_epi32` over 20,000,154 floats (20 M random bit patterns, every half-integer in -35..35, NaN, +-inf, +-0, the 2^31 boundaries): 0 differences.
- Not done: a bit-exact replay of a layer function that uses it (`0x1423B90E0`, `0x1423BB360` are KickMotion's proofs).

## 3. Tables and closure (item 2)
- `ro_tables.json`: byte tables `0x1403A7E14`, `0x1403A7EE4`, `0x1403A7FB0` (0xBC each; `cmp eax,0xBB; ja` at `0x1403A764C`, `0x1403A77A2`, `0x1403A78FC`); dword tables `0x1403A7E00`, `0x1403A7ED0` (5 entries; largest byte-table value 4, scripted over the image). `0x1403A7FA0` (4 entries) was already declared.
- `gen_translated.py --image pes_full.bin` (6 m 37 s, then 12 m 9 s after the two leaves): **5652 functions (3758 recorded bit-exact, 1894 closure)**; `--check` byte-identical; `ledger.py --check` current (38,338 rows); one held-out line, `0x14202BA30` (pre-existing). `untranslated_callees.py 0x1407A38E0 0x140862CE0 0x140862AE0 0x1407C1F40 0x1406E9F10 0x1406E9F90`: 684 translated functions walked, **0 untranslated callees**.

## 4. The two virtual leaves (new; found by survey)
After the closure the forfeit and throw-in tests aborted at `PES_DISPATCH_UNTRANSLATED 0x1406E9F90`. `GF_PES_SURVEY` (throw-in test, one run): `0x1406E9F10` and `0x1406E9F90`, 1 hit each, frame 26147, both called from `pes_fn_1407C1F40` (return offsets `0x13F0FA5`, `0x13F0CB0`, named with `nm`). They are slots of the anime interface's second vtable (data ref `0x1425E35E0`), called with this = `P+0xAC0`.
- `0x1406E9F10(this, float x)` = `x*x*c + c' + f*c''` with `f` = `[[[P+0xA08]+0xF0]+0xC0]` (decompile; leaf `FUN_141AF3180`).
- `0x1406E9F90(this, flag)`: the man id `[P+0x2458]`, stat bytes `[slot+0x70+0x2B]` / `+0x15`, the hash-checked stamina cell (`FUN_140A581C0`).
- `closure.py --spec-set trapaction --tag trapvirt`: 7 members, 2 new. Proof: `synth_specs/trap_virt_f10.json` (16 states) and `trap_virt_f90.json` (13) over the memory of the real `pid615595/seq7805` (rcx = P+0xAC0 of that call's rdx; thresholds read from the image by `synth_trapvirt_specs.py`), `prove_bounded.py --merge`: **`0x1406E9F10` 16/16 bit-exact, own-block 7/7; `0x1406E9F90` 13/13, 19/20 (0.95)**. Every case SYNTHETIC; no real call exists (no hook). Disk: each state is a 2.1 GB `mem.bin`; all deleted, `seq7805` re-packed.

## 5. Trap re-issue cadence (item 3): root cause found, fixed with a red test
**CONFIRMED (decompile, asm, capture):**
- `FUN_1409F8220` (reaction entry 3) calls `FUN_1409FA400` (gate 3). `0x1409FA48F CALL 0x140A627E0` gives the carrier (`MA+0x2601C`). If `carrier == me`, the receive goes on only when the man's anime action byte `[FUN_140A48910(G,me)+1]` (`0x1409FA4AD MOVZX ECX,byte ptr [R14+0x1]`) is `0x44` (`0x1409FA4B2 CMP CL,0x44`) with `MA+0x251FA[side] == 0` (`0x1409FA4B9`), or `0x10` (`0x1409FA4C8 CMP CL,0x10`), or `0x15` (`0x1409FA4CD CMP CL,0x15`); else `0x1409FA4D2 XOR AL,AL` refuses. Names from the image table `0x1434A5470`: 0x10 ANIME_STAGGER, 0x15 ANIME_DODGE, 0x44 ANIME_KP_DROP_BALL; 2 MOVE, 3 STEP_MOVE, 4 DRIBBLE, 7 TRAP.
- So a holder in his Trap, Dribble or Move is never handed the receive again. Capture `pid615595/seq7805` (the only real `0x1407A38E0` call): the holder's record `[G+(15+0xA5)*0x28]` (= `FUN_140A48910`) reads `07 07 02 02`; its byte +1 is the same as `P+0xAC9`; every other man's record is `02 02 03 03` or `03 03 02 02` (MOVE/STEP_MOVE), matching `P+0xAC8..0xACB`.
- The trap end handler `0x14079E5C0` (decompile) ends a holder's trap into the dribble action (vtable +0x58 -> `FUN_1406EEBB0(P,4)`, the real call at ret `0x14079E6C5`) or MOVE (`FUN_1406EEBB0(P,2,0x5E)`, the `0x14079E68D` site); `FUN_1406EEBB0` itself downgrades action 4/5 to MOVE when `MA+0x2601C != me`.
- GF's `_ReceiveCandidate` had gate 3's distance test only (the ledger marked `FUN_1409FA400` "ported"). With GF's holder (`Match::GetPesHolder`, the `MA+0x2601C` stand-in) in TRAP/DRIBBLE/MOVE, the reaction re-issued `_TrapCommand` every 2-3 ticks.

**Fix (the one gate every caller routes through):** `pes::CarrierMayTouch(actionType, keeperClaimFlag)` (`pesreceive.cpp`, every constant cited by VA), wired in `PlayerController::_ReceiveCandidate` on the PES path, reading `P(slot)[AC9]`.
- `// ponytail:` the keeper claim flag `MA+0x251FA[side]` reads as clear; its writer is NOT FOUND (pes-re-foundation-areas.md K-4).
- **Not ported (OPEN):** the second clause of the same gate (a teammate holds the ball and my rank byte `MA+0x247BE[me] != 0` refuses): the rank list is not in GF's window.

**Tests:** `PesReceiveCarrier.*` (3 new; `pes_receive_test` 20/20). `PesStall.AHolderIsNeverHandedANewTrap` (seed 7; counts trap clips that began for the PES holder).
**Mutation run, rebuilt, reverted:** gate disabled (`false &&`): `9 of 45 trap clips began for the man who already held the ball` (a clean EXPECT failure). With the gate: all five `PesStall.*` pass.

**Observation (TeamTick5's message, OPEN):** on their tree a named man stands 0.3-0.5 m from a resting ball with `MA+0x2601C == 255` and `GetBallRetainer() == null`. Gate 3's carrier clause does not engage there (no holder), so that stall is a different cause: the man is not handed the receive at all (INF: gate 4d's newest-event refusal, or the PASS_GET state). Not examined here.

## 6. The failing tests (item 4), on the final tree (built after the layer regeneration)
| Test | Result |
|---|---|
| `pes_forfeit_test` (all 9, incl. `ARedCard…` 556 s, `AYellowThenRed…` 392 s) | **9/9 pass** (before: abort at `0x1407ABBF0`, then `0x1406E9F90`) |
| `pes_restart_delay_test` (27: restart delay, throw-in carry, `PesStall.*`) | **27/27 pass** (before: abort at `0x1407C18B0`) |
| `pes_throwin_release_test` | 5 pass; **`ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` fails: "no ceremony throw-in was released in any searched two-minute match"**: `kCeremonySeeds {8,3}` drifted (SEED DRIFT, merge-lane census; not re-staged here) |
| `PesStall` on seed 7 | 5/5, with the final layer |
| keeper-stand mutation (`commandQueue.push_back(stand)` removed) | **CLEAN red**: `12 clip ends found no applicable anim` (an EXPECT failure, no abort); reverted |
| `GoalSceneHold…/PesLoop` | not run separately (in the gate selection) |

## 7. Item 5: real `rb5_*` captures
**None have landed.** `live2/pid1887719/` holds only `ai_arrival_14045E0D0` (12) and `kick_dd80_14071DD80` (1); `live2/pid1318045/` is empty; no `rb5_*` capture exists in any launch folder. The contact-leaf proofs (`0x140862CE0` 9/9, `0x140862AE0` 2/2) stay SYNTHETIC.

## 8. Gate and smokes (item 6)
`GATE_TIER=branch` attached, env as the brief (gate output `<workdrive>/gf-RestingBall6-scratch/gate.out`): select_recorded all five sets fully selected, 2304 tests selected; smokes ran (rc 0 both): switch-off **`a0fbe2d5cd445a52`** (= RestingBall4/5's: the pass-target stand-in `Team::PassGetReceiver`, pending TeamTick; main is `399e180f39459c2b`), pes-loop **`17941efc16b4a6a5`** (main `85c7d7c515cf92ae`; moved by the trap family, the carrier gate and the translated leaves, not split).

**`GATE branch: RED pes_throwin_release_test`**: the only failure; 2303 of 2304 selected tests pass (ctest wall 7,351 s); the recorded re-checks of all five sets, the ledger (38,338 rows, no regression) and the off-path ports (120, all allow-listed) are clean. The failing test is the seed drift of section 6 (`kCeremonySeeds {8,3}`: "no ceremony throw-in was released in any searched two-minute match"), which the brief expects for the merge-lane census; every other test of that binary passes. `GoalSceneHold`, `pes_forfeit_test` (9/9) and `pes_restart_delay_test` (27/27) are inside the passing 2303.

## 9. Resume state
1. Seed drift: `kCeremonySeeds {8,3}`: census in the merge lane.
2. Nothing else is red in the branch tier; landing needs the `kCeremonySeeds` re-stage after the merge-lane census.
3. OPEN: gate 3's second clause (`MA+0x247BE` rank), the keeper claim flag `MA+0x251FA`, TeamTick5's resting-ball shape (holder 255).
4. `ALiveBall` walk-vs-run on the PressAI tree: TeamTick4's.
5. The pass-target stand-in `Team::PassGetReceiver` stays until TeamTick.
6. Real `rb5_*` captures: replace the synthetic contact-leaf proofs when they land.
7. Captures: `pid615595/seq7805` re-packed (protected); `pid1572516/seq398` untouched.
