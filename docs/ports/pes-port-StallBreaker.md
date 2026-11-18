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

## 8. Round 3 (Main): slide starts by prior state, class gate, mutation

Result: no fix landed and nothing new pushed. `StallBreaker` is still `a9cda8669b` (`git ls-remote` confirmed); the worktree matches HEAD (`git diff` empty). Reason: the class gate does not touch the measured re-slides, and the PES refusal that does is a different rule (8.2).

### 8.1 Counts (full seed-7 match, `abe9f81d` tree, the shared census run)
- Strict prior state (the tick before the slide start was a fall, a stand-up or a slide): **0** slide starts. `PesStall.NoSlideStartsFromAManWhoIsDownOrSliding` PASSED (865 s), so the strict rule holds on this tree.
- Window of one second (100 ticks of 10 ms, the census's own clock): **126 of 243** slide starts had the man down or sliding within the second: 111 from a slide, 15 from a fall, **0 from a stand-up**; 117 had no down state in the second. Run FAILED as red (1,166 s). This is a proxy, not a PES fact: the window is my choice.
- Round 2: sliding-to-sliding clip restarts: 0.
- Reading: the stall re-slides are men who leave the slide for at least one tick (not a stand-up) and slide again within a second. The class gate tests the class at the decision tick, so it cannot remove them (the class is back to MOVE by then). Strict count 0 both with and without the gate.

### 8.2 The PES refusal that does cover this (CONFIRMED decompile of `FUN_140971BA0`, `p_140971BA0.txt` around the loop at lines 105-120)
- The predicate scans the man's own newest four events of the event store (`FUN_140A3E8C0` count, `FUN_140A3E190` entry: type at [0], frame at [4]; the store is `anime record +0xE0`, from `lVar20 = local_2b8` = the man's anime record). It breaks when the event is older than `fps * DAT_142588210 + DAT_142565600` frames (`fVar29 = FUN_141E532D0()`), and returns **false** when an event of type **0xB** (slide) is in the window and `FUN_140A60300` matches its actor.
- This is refusal (d) in `docs/re/pes-re-getup-recovery.md` §1. GF's `_SlideCandidate` has `_AnyTeammateSliding()` only; it has no own-slide-event window. This is the missing PES rule for the re-slide loop.
- OPEN: the window constants (`DAT_142588210`, `DAT_142565600`, not read), `FUN_140A60300`'s actor test, and whether GF's translated event store writes kind-0xB events for slides.

### 8.3 Class gate (drafted, measured, NOT landed)
- Drafted and reverted: `pes::AnimeClassBlocksSlide` over bits {5, 6, 7, 9, 13, 15} (the four decoded class tests: `FUN_140A6E4B0` bit 7, `FUN_140A6E0D0` bits 6/7/9, `FUN_140A6E5E0` bit 5, `FUN_140A6E3C0` bit 13, `FUN_140A6E300` bit 15; `FUN_140A6E3C0`'s anime+8 flag not modelled); `AnimeClassOf` maps a Movement clip of animType `special` (the stand-up) to FALL_DOWN (17); `_SlideCandidate` gate; the teamAI record byte carries `AnimeClassOf` for non-keepers.
- Not landed: it changes no measured count (8.1), and the record byte is read by `pesoffball.cpp` (the halved-reach check) and the translated PES readers, so landing it changes trajectories without a measured gain. No mutation or full run was done for it.

### 8.4 Item 3 (which of the 76 callers of `FUN_1406B4580` admits a resting-ball touch for a MOVE man): OPEN
- The callers are the generic admission; the TRAP starters `0x1407B0C33` / `0x1407B35DA` have no direct caller (indirect calls). Not decoded this round.

### 8.5 Verification run this round
- `PesStall.NoSlideStartsFromAManWhoIsDownOrSliding` (probe discarded after measuring, diff kept at `<scratch>/r3-probe.diff`): 0 strict, 126/243 windowed (red), on `abe9f81d`. No production change, so no movestep or throw-in run.

### 8.6 Next steps (in order)
1. Read the window constants and `FUN_140A60300` (decompile), confirm GF writes kind-0xB slide events in the translated store. Then the red test is the PES fact itself: no slide start inside the man's own kind-0xB window (expected 0 on PES by refusal (d)).
2. Land the own-event window in `_SlideCandidate` (one rule), mutation, then the class gate from 8.3 as a separate commit.

## 9. Round 4 (Main): window constants, actor test, kind-0xB writes, verdict

Result: no fix landed, nothing pushed. `StallBreaker` is still `a9cda8669b`; the worktree matches HEAD (`git diff` empty). Reason: the kind-0xB event that the window scans is written by a ball-state detector whose inputs GF does not model, so the write cannot be made faithful yet (9.3). Round-3 section 8.2 said the window belongs to the slide action; that was WRONG (9.2 corrects it).

### 9.1 Constants (VERIFIED from the flat image, `pes_full.bin`, offset = VA - 0x140000000)
- `0x142588210` = **10.0** (float `0000 2041`); `0x142565600` = **0.5** (float `0000 003F`). Both are the reads Main gave.
- Frame rate `0x14351AE54` = 54 (`FUN_141E532D0` returns `DAT_14351AE54`; GF's `kPesTickHz` = 54.0, `pesball.hpp:61`).
- Window = `(uint)(54 x 10.0 + 0.5)` = **540 frames** (10 s). Break when `(uint)(now - frame) > 540` (PES compares unsigned).

### 9.2 The scan (CONFIRMED decompile of `FUN_140971BA0`, lines ~105-120 of `p_140971BA0.txt`)
- The actor test `FUN_140A60300(a, b)` (CONFIRMED): true when the two 8-byte keys are equal (`a[0]==b[0] && a[1]==b[1]`). `FUN_140A62FE0(man)` is the man's actor key.
- The scan visits the event log entries `n = 0..3` (`FUN_140A3E190(E, n)`, `E = anime record +0xE0`; count from `FUN_140A3E8C0`). Payload: kind at +0 (`ActionKind()` = record +0x14), actor at +4/+8, frame at +0x10 (`ActionFrame()` = record +0x24). Break on age > 540; return false on kind 0xB with the man's actor.
- GF's `pes::EventAt(store, n)` reads the same newest-first log (`NewestOrRing0`), so the scan ports directly.
- CORRECTION: kind 0xB is NOT the slide action. It is a **ball-state event**: `FUN_140A18690` (the ball's second state, GF `BuildSecondBallEvent`, `pesevents.cpp:578`) writes kind 0xB when the ball state byte `G+0x40+0x2914` is nonzero and the toucher's anime class has bit 7 (`FUN_140A6E4B0`, the SLIDING class). The other kinds: 0xC when the class has bit 6 (`FUN_140A6E580`) and the holder byte 5 is in range, 10 when bit 8 (`FUN_140A6E070`) with the same range, else 0xD; state 0 or above 6 gives 0xD (`FUN_140A18690` decompile).

### 9.3 Does GF write kind-0xB events? NO (CONFIRMED by code reading and by the test)
- `Team::SetLastTouchPlayer` queues every touch with `PesActionKindOf(function)` (`team.cpp:33`): kicks 1, high kick 2, shot 5, **everything else 7 (touch)**. A slide touch is therefore a 7, never a 0xB; the store's `in.actionKind` cannot carry the class.
- The second-state inputs are never set by any caller (`holder` = G+0x2910 defaults to -1, `holderFlag` = G+0x2914 to 0, `holderByte5` to 0, `holderClassKind` to 0xD; grep: no writer outside `pesevents.hpp`). So `BuildSecondBallEvent` returns 0xD for every GF ball event. GF never writes 0xB.
- MEASURED: the windowed probe `NoSlideStartsInsideItsOwnSlideEventWindow` PASSED on `abe9f81d` (873 s) with 0 slides inside the window. That zero is BLIND: GF has no kind-0xB events to be inside. It is not a red test, so it was discarded (diff kept at `<scratch>/r4-window-probe.diff`).

### 9.4 Why the fix stops here
- A faithful kind-0xB write needs the ball-state bytes G+0x2910 (holder slot), G+0x2914 (state byte, nonzero in play), G+0x2915 (byte 5). GF's world record has no such fields and their writer is NOT decoded. Deriving them from GF's possession or ball state would be a stand-in on the input of a PES predicate, which the brief forbids.
- The class mapping itself is decoded (bits 7 / 6 / 8 above, with the 0xB > 0xC > 10 > 0xD order: bVar4 block overrides 0xB). It is ready to port once the state bytes exist.

### 9.5 Class gate (Main's round-4 option): NOT committed
- The class gate writes an anime-record byte that PES readers consume (`pesoffball.cpp:1437`, the translated readers). That byte is PES's own write (`lane-units2 S12`, not decoded). GF's `AnimeClassOf` would be a stand-in on it, so it is not PES-correct on that field yet. Not committed; no measurement of it has changed the stall.

### 9.6 Verification this round
- Red/green probe: PASSED (blind, 0 by construction; discarded). PesStall full binary, movestep and throw-in: not run (no production change).
- Worktree clean; no push.

### 9.7 Next steps
1. Decode the writer of G+0x2910 / G+0x2914 / G+0x2915 (the ball-holder and ball-state records, `FUN_140A18690`'s inputs), find which GF state they map to, and wire them in `matchevents.cpp` (`BallEventInput`): then the own-window rule is real and its red test is "a slide touch with ball state nonzero writes kind 11".
2. Then `_SlideCandidate` gate on the window (constants 10.0 and 0.5 read from `0x142588210` / `0x142565600`, the scan as `EventAt(n)`, n < 4).
3. Item 3 (the 76 callers of `FUN_1406B4580`) remains OPEN.
