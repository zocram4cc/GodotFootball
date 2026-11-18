# pes-port-StallBreaker: the unexplained breaker ticks on MergeBKBJ (abe9f81d)

Status: diagnosis done, reasoned stop, no production fix. Branch `StallBreaker` (off `origin/MergeBKBJ` abe9f81d), pushed at **a9cda8669b** (`git ls-remote origin StallBreaker` confirmed). The only change is test-only and env-gated: `tests/sim/pes_stall_test.cpp` prints one `STALLDUMP` line per live breaker firing the series freeze does not explain (`GF_STALL_DUMP=1`).

## 1. Like-for-like count (same census, same seed-7 two-minute PES match)

`tests/sim/pes_stall_test.cpp` is byte-identical on `51a32323` (TrapContact) and `abe9f81d` (MergeBKBJ). The same counting patch is applied to both trees. Both runs: `pes_restart_delay_test --gtest_filter=PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`, `PES21_ROOT=<workdrive>/PES21 PES_EXE=<local>/ghidra-projects/scratch/Unpacker/pes_full.bin`, sim slot via `tools/dev/simslot.sh`. (`PES_EXE` is required: without it `FillMissingPesConstants` fails on the trap source.)

| Tree | live firings (headline) | series-frozen dribbler | during a freeze | unexplained (the STALLDUMP lines) | episodes | last dump tick |
|---|---|---|---|---|---|---|
| TrapContact 51a32323 | 7,491 | 4,240 | 91 | **3,160** | 60 | 40,197 |
| MergeBKBJ abe9f81d | 1,775 | 0 | 0 | **1,775** | 120 | 34,968 |

Verdict: MergeBKBJ did **not** add unexplained forced ticks. On a like-for-like count it has 1,385 fewer (3,160 to 1,775). The "1,775 vs 1,389/642" comparison was an artefact: the headline (1,775) counts every live firing including the series-frozen ones, while `pes-port-TrapContact10.md` (1,389 total, 642 unexplained) was measured on an older TrapContact tip with a different census state. Caveat: the two runs end at different ticks, so this is per-tree totals over one full census run, not a same-tick comparison. The 4,240 series-frozen firings on 51a32323 are gone on MergeBKBJ (0); I did not attribute that drop to a commit.

## 2. Classification of the MergeBKBJ unexplained firings (1,775, per-firing dump)

Every firing has a designated man (`man=none` count 0). Grouped by the clip the man plays (GF-native `assets/gf_anims`), with TrapContact in brackets:

| Group | MergeBKBJ | TrapContact | Notes |
|---|---|---|---|
| Walker / idle / run (`idle_0_3_run_*`, `idle`, `run`, `mid_delay`, `parallel`, fn 14, ac8 2) | **487** (idle fn14 ac2 354) | 535 | the largest group |
| Slide (`sliding/idle/000` 432, `sliding/sprint` 9; fn 13, ac8 11) | **441** | 201 | largest single clip (432) |
| Other action (highpass, pass, ballcontrol, avoidjump, trip, shot) | 441 | 304 | |
| Dribble (`dribblerun_*`, fn 14, ac8 7 / 2) | 175 | **1,122** | the series-frozen ones were here on TrapContact |
| Stand-up (`movement_special/.../000_stand_up_from_back`, fn 1) | 114 | 126 | |
| Trap (fn 14, ac8 7 / 2) | 93 | 225 | trap pose not the cause (see 4) |
| autogen (fn 1) / other | 1 / 23 | 286 / 361 | |

Top episodes (MergeBKBJ): H10 150 ticks `dribblerun_3_0_045`, H10 106 ticks slide, A10 95 ticks stand-up, A8 76 ticks slide, H11 65 ticks trap, A5 58 ticks dribble.

Slide frames (7 `stalemate-who` lines, MergeBKBJ): `sliding/idle/000` at frames 0, 9, 27, 33, 45 of 47. The slider is held through the whole slide clip, not only at its last key. (INF: 7 samples.)

## 3. Causes

**Largest single clip: the slide (441).** The breaker forces the nearest man (0.8 to 2 m from a loose ball) who is still in GF's slide clip (`sliding/idle/000`, 47 keys, not looping), or who has just stood up (`stand_up_from_back`) and is re-slid (the 349-tick slide/stand-up loop recorded in `pes-port-TrapContact10.md` §15.4). The get-up in PES is a phase machine (`anime+0x245C`, `docs/re/pes-re-getup-recovery.md`): rise gate and frame-test answers are CONFIRMED, but the **release at the end of the stand-up is RETRACTED** (§4: "the release is via action 80 InplayDemoContact → Move, not a zero-remainder frame test") and the track counts are unread (§6). So no decoded PES rule states when a slider may leave the slide or be re-slid. The GF stand-ins (native slide clip, GF stand-up clip, slide predicate with no get-up gate) have no decoded replacement. Attribution: GF side; PES rule not decoded.

**Largest grouped class: walkers (487).** A GF-native walk/idle clip under a live MOVE command, with the man 0.3 to 1.9 m from a loose ball at rest or walking at 0.3 m/s, who never plays the ball. This is the designated-man / possession shape already documented as open in `docs/ports/pes-stall-IdleEpisodes-2.md` (Group A): the touch-acceptance rule that would decide when a designated man 0.4 to 1.0 m from a resting ball plays it has no decoded PES source (the touch-reach stand-ins tried there were worse: 92 stalls and 103 episodes, and 73 stoppages). The MOVESTEP arrival/IDLE–STEP_LOOP issue (`pes-port-StepAudit6.md` §7.4) is the related open item. Attribution: GF side; PES rule not decoded in this tree.

Neither largest class has a decoded PES rule standing behind a GF stand-in, so the brief's fix path (red test from PES facts, one mutation, push) is not available. Stopped; no production change.

## 4. Trap pose

Trap-family firings are 93 on MergeBKBJ (225 on TrapContact): not the largest class, and the trap-end handler `0x14079E5C0` is already the proven path (`pes-port-TrapContact4/8`). The trap pose on OneWorld-series-wip is not the cause of the largest classes here, so no stop is forced by it.

## 5. Open

1. Decode the stand-up end release (PES action 80 `InplayDemoContact` to Move) and the stand-up clip's track counts (`pes-re-getup-recovery.md` §6). A decoded rule would allow a PES-derived red test for "no re-slide while the get-up is in progress".
2. Walker group (487): decode the touch-acceptance / possession rule for a designated man 0.4 to 1.0 m from a resting ball (`pes-stall-IdleEpisodes-2.md` Group A).
3. Why the series-frozen dribblers (4,240 on TrapContact) are absent on MergeBKBJ (0): not attributed; a bisect of the merge would answer it.
4. The census dump (`a9cda8669b`) is the tool for the like-for-like count on any tip: `GF_STALL_DUMP=1 ./build/tests/sim/pes_restart_delay_test --gtest_filter=PesStall.ALive*`, then group the `STALLDUMP` lines by `clip=`.

## 7. Round 2 (Main's request): stand-up release, re-slide gate, touch acceptance

Status: decode done for the release and the slide predicate; the re-slide hypothesis was TESTED and FALSIFIED; no production change landed; the red test is NOT committed (see below). Branch `StallBreaker` stays at `a9cda8669b`.

### 7.1 Stand-up end release (CONFIRMED static, image + vtables)
- Action names: `ANIME_DEMO_INPLAY_CONTACT` = 80, `ANIME_FALL_DOWN` = 17, `ANIME_SLIDING` = 11 (inline table at 0x1426523E0, read by script).
- Action 80 vtable 0x1425E28F8 (+0x70 frame test `0x1407E8D20`, +0x50 end handler `0x1407EBD90`, +0x20 phase dispatcher `0x1407E7260`). FALL_DOWN vtable 0x1425DF250 (+0x10 `0x14072E840`, +0x20 `0x14072C7A0`, +0x70 `0x14072E360`, +0x100 rise gate `0x14072C5C0`). SLIDING vtable 0x1425E1878 (+0x50 `0x140788C00`, +0x70 `0x140787EC0`). Vtable bases recovered from the slot pointers (image search).
- The post-fall rise is the FALL_DOWN phase-1 handler, headless decompile at `0x14072EA00`. Its exits: `FUN_1406EEBB0(P,2,0x5E)` (MOVE) at decompile line 203 (the call's own address was NOT read) when the rise track has run out; the injury branch admits DEMO_INPLAY_CONTACT via `FUN_1406B4580(P,0x50,...)` then `FUN_1406EEBB0(P,0x50,0x5E)` (decompile line 75). The only `mov edx,0x50` site feeding `FUN_1406EEBB0` in the image is 0x14072EED9.
- Action 80's own end: phase dispatcher `0x1407E7260` phase 1 switches to MOVE with `FUN_1406EEBB0(P,2,0x5E)` inside its track loop, under the gates the decompile shows. Its +0x50 handler `0x1407EBD90` in phase 2 leaves through `FUN_1406E6620(P,1)` once the elapsed count is negative.
- SLIDING's own end `0x140788C00` (+0x50): leaves through `FUN_1406E6620(P,1)` when the motion's remaining count is negative, otherwise keeps the clip.
- OPEN: the stand-up track counts (rec+5 entry count and per-track frames, `FUN_1406AFBC0` / `FUN_1406AF620`). They live in the install's motion records and were not read. Not needed for the verdict below.

### 7.2 Slide predicate class gate (CONFIRMED decompile of `FUN_140971BA0`)
- Refusals, open play only: own anime class bit 7 (`FUN_140A6E4B0`, line 94), bits 6/7/9 (`FUN_140A6E0D0`), bit 13 (`FUN_140A6E3C0`, or anime+8 bit 13), bit 5 (`FUN_140A6E5E0`), bit 15 (`FUN_140A6E300`). Each returns true on a set bit, and the predicate returns false on it.
- Install table 0x1426484F0 (read by script): SLIDING 11/12 = 0x4080, TRAP 7 = 0x4020, TACKLE 13 = 0x4040, DIVE 15 = 0x2000, STAGGER 16 = 0x3002, FALL_DOWN 17 = 0x3000, JOSTLE 72 = 0x8000 are refused; MOVE 2, DRIBBLE 4, DODGE 21 and the passes are not.
- GF's `_SlideCandidate` (elizacontroller.cpp ~1409) has NO own-class gate. That is a real PES rule GF lacks.

### 7.3 Re-slide red test: FALSIFIED on this tree
- Probe added (uncommitted): a sliding man's clip restarts while he is still Sliding (new clip, or the same clip back at a lower frame). Full seed-7 match, shared run: `PesStall.ASlidingManIsNeverSlidAgainWhileHeSlides` PASSES on `abe9f81d` (0 restarts, 759 s).
- So GF never re-slides a man who is still sliding. The 0/47 frame restarts in the stall dump come after a gap: the man left the slide (stand-up clip, function 1 in the dump) and was slid again. The loop is slide, stand-up, slide.
- Consequence: a bit-7 gate alone is a no-op on this tree. I did not land it. A red test for the actual loop needs a probe that counts slide starts from a man whose previous clip was a stand-up or a slide (PES refuses both: rise = action 17, bit 13; slide = 11, bit 7). GF's stand-up is not typed as FALL_DOWN, so the faithful fix is to type the rise as action 17 during its phase 1 (decode 7.1), which is a larger change than this round allowed.
- The probe and its test are left UNCOMMITTED in `tests/sim/pes_stall_test.cpp` on the worktree (not pushed). They pass; they are not a red test. Discard or repurpose them.

### 7.4 Touch acceptance for a designated man 0.4-1.0 m from a resting ball (item 2)
- Census (MergeBKBJ, section 2): the walker group is MOVE (action 2) or idle/run men, almost none in TRAP. So the walkers never start a trap. PassArrival's mechanism is the reach test inside a TRAP action (`0x140799060`). It does not apply to these men. Verdict: NOT the same mechanism; that item is stopped here.
- What admits a touch for these men: a trap start. TRAP (7) is written at 0x1407B0D57 (FUN_1407B0C33) and 0x1407B35E6 (FUN_1407B35DA). Both starters are reached indirectly (no direct caller in the image). The admission `FUN_1406B4580` has 76 direct callers. Decoding which one admits a resting-ball touch for a designated man is OPEN; not done in this round.

### 7.5 Verification run this round
- `pes_restart_delay_test --gtest_filter=PesStall.ASlidingManIsNeverSlidAgainWhileHeSlides`: PASSED (759 s, sim slot, `PES_EXE` set). No mutation (no fix landed). Movestep and throw-in not re-run (no production change).

### 7.6 Next step (one line)
- Probe slide starts from a man whose previous clip is a stand-up or a slide, expect >0 on this tree, then type the rise as FALL_DOWN (17) in GF during phase 1 of the get-up (decode 7.1) so that `_SlideCandidate` can refuse by the install class word.
