# pes-port-StallBusy: the PES busy gate never holds at any of the 92 live-ball firings (PES's own action bytes are MOVE 2 or TRAP 7); no change landed, tree reverted (StallBusy, 21-11-26)

Status: REVERTED, NOTHING LANDED. The premise (a busy body wrongly takes a fired reaction) is falsified by the census. Branch `StallBusy` was StallRun a2439354 plus a merge of origin/FastStall 88ae7714 (merge d0fd8c22, test-only quick target). That merge was reset: the local branch is back at a2439354 and was never pushed. The baseline ALiveBall count on this tree is **92**, the same as StallRun.

## 1. Measurements (seed 7, GF_STALL_DUMP=1, tree a2439354 + FastStall test)

| Run | Window | Forced live-ball ticks | Log |
|---|---|---|---|
| PesStallQuick | first 240 s (12,960 ticks) | **58** | `gf-StallBusy-scratch/census-base.log` |
| PesStall.ALiveBall (full) | whole match | **92** (FAILED, as StallRun) | `gf-StallBusy-scratch/census-full.log` |

Build: the first configure and build of `pes_restart_delay_test`, `pes_movestep_wire_test`, `pes_hang_test`, `pes_pass_target_test` took 58 s with the shared ccache. The first quick run lacked `PES_EXE` and failed with "install constants unavailable" (not a stall); the reported runs have the gate env (`PES_EXE` = unpacked `pes_full.bin`, `PES21_ROOT`, `GF_DIFF_PYTHON`).

## 2. Census of the 92 firings (ContactAction/`e_FunctionType` ids from `gamedefines.hpp`; `ac8` = PES anime action byte P+0xAC8)

| Body fn (current) | Decision (cmdfn) | ac8 | Count | Window |
|---|---|---|---|---|
| 14 Special (PES locomotion/trap clip) | 1 Movement | 2 MOVE | 36 | 0 to 60 s |
| 14 Special (trap clip `trap_0_0_f045_y0_sole`) | 1 Movement | 7 TRAP | 18 | 0 to 60 s |
| 13 Sliding (`sliding/walk/000.anim`, ball 2.2 to 2.4 m) | 13 Sliding | 2 MOVE | 21 | 480 s on |
| 12 Trip (`trip_t1 ... protectfront`) | 12 Trip | 2 MOVE / 7 TRAP | 13 / 2 | 480 s on |
| 1 Movement | 1 Movement | 2 | 2 | 480 s on |

- The quick window (0 to 240 s) holds exactly the first 58 (all in the first minute). The 34 at about 480 to 540 s are the second episode (not reached by quick).
- Ball speed is 0.08 m/s in every firing. Man to ball 0.8 to 2.4 m.

## 3. The busy gate (CONFIRMED against the image)

PES fires a reaction over a movement only when the body's current action is not busy (`pes-re-defender-decisions.md` 1). The busy tests are class-table bits on `0x1426484f0` (4 bytes per anime action; read from `pes_full.bin`, offset VA - 0x140000000):

| Action class | word | bit 4 | bit 12 (stagger/fall, `6e090`) | bit 16 (kick, `6e340`) |
|---|---|---|---|---|
| 2 MOVE | 0x00000001 | 0 | 0 | 0 |
| 7 TRAP | 0x00004020 | 0 | 0 | 0 |
| 8 / 9 / 10 (pass, shoot) | 0x00010000 | 0 | 0 | 1 |
| 16 STAGGER / 17 FALL | 0x00003002 / 0x00003000 | 0 | 1 | 0 |

Result: **no firing in the 92 has an action byte in a busy class** (every `ac8` is 2 or 7). A busy gate on reaction issue would have had zero effect on this stall, quick or full. The 15 Trip-in-Trip re-fires are GF's Trip mapped to STAGGER by `Match::PesActionOf`, but PES's own byte reads MOVE (2), so PES's gate does not see them as busy either. The gate is not ported: a port with no firing to change is a no-op, and the owner's land-or-revert rule reverts it.

## 4. What is left (INF, not decoded here)

- Bucket 1 (58 firings, first minute): MOVE/TRAP bodies with a Movement decision, mid-clip, ball 0.8 to 1.2 m away, ball at rest, no man touching it. A movement or trap-touch question (`ProcessMoveStep` holds the FSM during non-movement clips; `humanoid.cpp:415`), not a reaction arbitration. The GOTO flips (`ms=6<-7`, `7<-6`) and the trap touch frame are the candidates. Owners: the issue/start path (StallAnkle) and trap touch.
- Bucket 2 (21 Sliding-in-Sliding, 2.2 to 2.4 m from a ball at rest): a fired slide re-fires each tick while the body is in its own slide. Slide (0xb/0xc) is not in the busy set, so it is not a busy-gate case. A re-fire loop in the reaction chooser's repeat logic (Think/MovementDecision, my area) is a separate hypothesis, not tested.
- Bucket 3 (15 Trip-in-Trip): as above, not a busy-set case by PES's byte.

## 5. Mutation and tests

None. No source change, so no red test (TDD applies to a change, and a gate with zero candidate firings is a no-op). The brief's PesHang, PesPassTarget and MoveStepWire suite were not rerun: the final tree is StallRun a2439354 and its results are in `pes-port-StallRun2.md` (PesHang PASS, PesPassTarget PASS, MoveStepWire 40/40 on the e49318d3 tree). `gen_translated --check` not run: no translated file changed and nothing is pushed.

## 6. Open items

1. Bucket 2: test whether the slide's repeat is a re-fire every tick (chooser cadence, `pes-re-defender-decisions.md` 1: the chooser runs every logic frame, slide predicate on odd frames only). Next experiment: count per-tick `Sliding` decisions for a single man in the second episode.
2. Bucket 1: the GOTO and trap-touch decode (StallRun2 items 3, 4; the trap touch frame in `PlayIssuedClip`).
3. The trap-touch frame for `trap_0_0_f045_y0_sole` (man 17, frame 16 to 18 of 121, ball 0.79 m): does the contact frame touch the ball?
