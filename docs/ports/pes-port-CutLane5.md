# CutLane5: the fixed-block gfx model FAILS the landing condition and is reverted to answer-0; wide test still red (scene list root not closed)

Branch CutLane. Tip **f4c4dd40** (`git ls-remote origin CutLane` = f4c4dd40, CONFIRMED). It is `git revert` of 297071b0 (CutLane4's round 4b) on top of 82ad8564 (Integrate). Worktree <workdrive>/gf-CutLane. Labels: CONFIRMED = read or run this round; INF = inference; OPEN = blocker.

## 0. Verdict

- Landing condition (23-11 gfx ruling): **FAILS.** The fixed-block model excludes the recorded recompute proof 0x140471A80 on seq203 (section 1). Per the ruling the answer-0 model stays: reverted in f4c4dd40.
- Wide test (GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes): **still red**, as on 82ad8564 (scene 1 vs PES 0x300D9; T (-57.58, 0, z 0) vs PES (-41.55, 0, z 34)). Not landable.
- Rig requests: none. No live reads. No new capture used (one scratch read of the seq203 snapshot, section 4).

## 1. Landing condition: every recorded proof that reaches the thunk 0x141957C50

### 1.1 The roots of Main's list (all snapdiff proofs; `driver --check` skips them)

| Root | Record | Re-run on the tip (fixed-block) | Re-run on 82ad8564 (answer-0) |
|---|---|---|---|
| 0x140471A80 recompute | results_core.json row; results_snapdiff_recompute.json (seq203 of pid2024691; trace gf-CutsceneLine-scratch/recompute/trace203) | **EXCLUDED** (counterexample): `fetch from code outside every PE module of the snapshot at 0x17b0000 x1`, calls=1 replayed=0 (CONFIRMED, cl5/run_recompute.log) | **EXCLUDED** (stale record): `GF service stub_141957C50 reached x1` (CONFIRMED, cl5/res_recompute_ctl.json). run.py:615 refuses every gf-service stub; stubs.json 141957C50 is `kind: gf-service` (op gfx_device_alloc, rax = 0). |
| 0x14047B460 tick | results_core.json row; results_snapdiff_tick.json (seq203_r4) | Proven only under fixed-block (its own record: 1 synthetic call, bit-exact). Under answer-0 the replay loops in the builder (CutLane4 R2.5), so it is not provable. Removed by the revert. | not provable (R2.5) |
| 0x141F7BFE0 proccam | results_proccam.json (18 calls: live2/pid3234533 seq6220/6224/6228, pid3741448 seq11590/11594/11598, seq71718..71750) | **NOT RE-RUN: captures absent.** pid3234533 and pid3741448 are in neither live2 nor the Dati III archive; only trace dirs remain (gf-ProcCamera6-scratch/tr/seq71718 etc.). | same |
| 0x141F7AE60 proccam ctor | results_proccam.json (5 synthetic states built from pid3234533 seq6218/6219/6220/6224/6228) | **NOT RE-RUN: captures absent** (the synthetic states need the mem.bin). | same |
| 0x14091E9F0 trapaction | results_trapaction.json (6 real calls; the record does not name them) | **NOT RE-RUN** (calls not named). Unaffected by construction (INF): closure_pressx.json (235 members, capture-bounded) has no thunk member, and a bounded closure aborts loudly on any call outside its members. | same |

The recompute counterexample alone decides the ruling. The proccam root is a second affected proof (closure_proccam.json lists 0x141957C50 as a member, so its replays entered the thunk), and it cannot be verified because its captures are gone.

### 1.2 Mechanism of the recompute counterexample

- CONFIRMED by disassembly: the thunk call in 0x14195BD30 at 0x14195BD60; 0x14195BD6D `je` on rax; the non-null path reaches 0x14195BD85 `call 0x14195B4E0` with rcx = the returned block. Under answer-0 (rax = 0) that call is skipped.
- With the block model (0x600000000000) the replay runs PES's bytes past the allocation, and a code fetch then lands at 0x17B0000: a 64 KB RWX page of the snapshot that no PE module covers. INF: the block is not the device (DAT_1437C9238 -> 0x1505BA0 in seq203, CutLane4 R2.5), so the code consumes a pointer the block does not hold. The caller of the bad fetch was not traced (OPEN).

### 1.3 Indirect reach (search over recorded roots)

- Tool: `<workdrive>/gf-CutLane-scratch/image_callers.py` (scratch): capstone over every .pdata function of pes_full.bin, E8/E9 edges, transitive closure of callers of 0x141957C50.
  - Direct callers of the thunk: 243. Functions reaching it through direct calls only: **1,646** (CONFIRMED, thunk_closure_image.json).
  - Recorded rows in results_*.json with proof or diffed: **3,716**. Rows whose root is in the direct-reach set: **0** (CONFIRMED).
  - The four roots reach the thunk only through indirect edges: 0x140471A80 and 0x14047B460 (static closure 341 and 375 members, no thunk member: closure.py --dry-run, CONFIRMED); 0x141F7BFE0 (its bounded closure lists the thunk; 13 of its members are direct-reach functions, CONFIRMED); 0x141F7AE60 and 0x14091E9F0 (no member in the direct-reach set).
  - The image has no static resolution of call-through-register or vtable targets, so indirect callers cannot be enumerated from it. Only the replays are a dynamic witness, and the results rows carry no per-replay stub-hit record: whether other rows reach the thunk is OPEN.
- The layer-based walk (callers_up.py, R4b.6) under-counted: 13 direct callers and 24 transitive, against 243 direct callers in the image.

### 1.4 Action

- Ruling: "if any changes, revert to answer-0 and report the counterexample." Done: f4c4dd40 reverts all of 297071b0 (44 files: the model in stubs.json, stubs.py, xlate.py, pes_state.hpp, pes_stubs.hpp, run.py, the fox test, the tick layer and its 375-member closure specs, results_core and results_snapdiff_tick rows, tools/dev/cutscene/tick_calls.py, and the ArrangeGoal tick path).
- Pre-push: gen --check **rc 0, 10,982 functions byte-identical** (3,906 recorded bit-exact, 7,076 closure; CONFIRMED, cl5/gen_check_revert.log). tools/check_pii.sh was not run from main's checkout; the revert adds no path text (deletions and restorations only).
- Main should know: **the baseline recompute proof does not replay on 82ad8564** (refused as a gf-service). The results_core and results_snapdiff_recompute "proven" records predate that classification, so they are stale. Re-proving 0x140471A80 needs a model that both replays and matches PES (OPEN, section 3).

## 2. Wide test (item 2): not green. What was decoded and tested

### 2.1 Tests on the reverted tip (= 82ad8564 behaviour; CONFIRMED runs)

- pes_goal_anchor_test: 25 pass, 1 fail (WideChannelScorerTakesTheElseArmAndEqualsPes: line 434 scene 1 vs 196825; line 438 T -57.5796 vs -41.5502 and z 0 vs 34; the same for DC 5, 100, 161).
- pes_fixdemo_select_test 8/8; pes_fixdemo_table_test 7/7; pes_fox_world_test 15/15 (the answer-0 version of the fox test).

### 2.2 The tick path is closed under answer-0

The tick (0x14047B460) is the PES call path that passes the list to the recompute (its recompute call passes r8 = D and rdx = ctx). Under answer-0 it loops in the builder (R2.5), so ArrangeGoal cannot call it. CutLane4's "ArrangeGoal on D" is reverted with the model.

### 2.3 Decode (CONFIRMED by Ghidra headless decompile: dec_pick.txt, dec_pick2.txt, dec_62ea0.txt; and by seq203 memory reads)

- The else arm of the recompute (BC not in {6, 8}; dec_7b460.txt lines 442-533): calls the list builder 0x1404B6BD0 (line 443), then the radius 0x141F5EC60, then the event builder 0x14047C500 (line 525, r8 = D).
- 0x1404B6BD0 (lines 672-700): `*(A0+8) = 0x1404B6400(A0, ctx, ..., BC)`. With BC = 2 on the wide scorer, **the scene pick is 0x1404B6400**.
- 0x1404B6400 (CONFIRMED): returns 0x3013A when A0+0x131 is set. Otherwise `lVar5 = 0x141E5D4C0(_, *(*(G+0x2C0)+0x1C))`, which is `entry[idx] + 8` of the cut library (the registry at `_DAT_1437F4808`, GF's kCutLibraryVa). The pick returns 0 when the entry is missing.
- seq203 (CONFIRMED read, cl5/rd203.py through index.json mappings): `*(G+0x2C0)+0x1C` = **113 (0x71)**; `+0x20` = 113 too. The object is a packed settings struct, not a vtable object: f32 +0x08 = 24.0, +0x0C = 50.0, +0x48 = 0.8, +0x4C = 0.2, +0x58 = 10.0, +0x5C = 1.0.
- GF writes the stadium cut key at +0x20 (fixdemo_select.cpp:530, ponytail; the test helper SetStadiumCutKey writes 0x71 at +0x20). Nothing in GF writes +0x1C.
- Key 0x140474C90 (CONFIRMED): `match+0x1588 == 1 ? G+0x1590 : 0x140A62EA0(ctx, match+0x15c8)`, returned as is below 0x16 and as 10 otherwise. **It does not read ctx**: 0x140A62EA0 ignores its first argument (CONFIRMED) and maps the side through the team record 0x140A489D0 (G+(0xA3+side)*0x28) and a 40-slot key search 0x140A3B320. So the ctx gap does not feed the key.
- ctx: PES passes rdx = `**(D+0x17860)` = owner+0x228 (docs/re/pes-re-sanction-caller.md: the 16-byte stack pair {owner+0x228, owner+0x18} published at Flow+0x17860 by 0x140444000; four publishers 0x140421DC4, 0x140422047, 0x14042334B, 0x140423B04). GF passes its event record as rdx (ArrangeGoal else arm, `s.gpr[2] = event`). The 82ad8564 comment that the list is {event, event} is a misreading (INF).

### 2.4 Mutations (run, reverted)

- M1 (run, reverted): also write the cut index at +0x1C (`s.wr(timeScale + 0x1C, stadiumCutKey, 4)`). Built pes_goal_anchor_test: **the armed scene stays 1 and T stays -57.5796 for DC 5, 100, 161 (red unchanged).** So +0x1C is not the sole cause, and the pick still returns 1 (neither 0 nor 0x300D9). The revert restored the 82ad8564 source; the build after it is the baseline.
- M2 (the fixed-block model vs answer-0): the counterexample in section 1; not a test mutation.

### 2.5 Ponytails not replaced

- The zeroed arg6 object and the empty {event, event} list are NOT replaced: the decode did not produce owner+0x228 (its constructor and the four pair publishers are unported), and the tick that would consume them is closed (2.2).
- The r9 Zalloc and the manual A0+0x18 write remain (named in fixdemo_select.cpp).

## 3. Open items (named, with the next experiment)

1. OPEN: which input makes 0x1404B6400 return 1 on GF rather than 0. Its other reads: the table record `0x140A48A10(G, key)+0x55C` and the gate A0+0x131, and `0x141E5D4C0(113)`: is entry 113 built by GF's cut-library builder 0x141E5C8F0? Experiment: a scratch dump of GF's cut library entry count and entry 113 at the armed call (no new capture).
2. OPEN: the block model needs a device-compatible answer. The seq203 device is a live object behind DAT_1437C9238 (vt +0x20 = 0x141B3D8E0), not the block. A model that both replays the recompute and matches PES is not designed; the ruling's condition is not met.
3. OPEN: owner+0x228 and the pair producer (the four 0x140444000 callers). Port them only after experiment 1 names the field the pick reads from owner+0x228.
4. Indirect callers of the thunk: the static image search gives 1,646 direct-reach functions and 0 recorded roots among them. Full indirect coverage needs per-replay stub-hit logs (a harness change; not done).
5. Recorded proofs stale on 82ad8564: 0x140471A80 (refused as gf-service). Main decides whether to re-record or mark them.

## 4. Scratch and housekeeping

- Scratch (gf-CutLane-scratch/cl5): run_recompute.log, res_recompute*.json, build logs, gen_check_revert.log, rd203.py (reads seq203 by index.json), ratchet logs. Root scratch: image_callers.py, dry_*.txt, dec_pick*.txt, dec_62ea0.txt, thunk_closure_image.json, indirect_fns.json.
- The recompute snapshot is gf-CutsceneLine-scratch/recompute/seq203 (not mine; read only). The tick snapshot copies were deleted in round 4b; nothing new was unpacked.
- No capture in live2 was read, moved or deleted.

## 5. Tests and ratchets (tip f4c4dd40)

- Built: pes_goal_anchor_test, pes_fixdemo_select_test, pes_fixdemo_table_test, pes_fox_world_test, pes_restart_delay_test (build rc 0; `/tmp/gf-build.lock` around the build step only).
- Run: goal_anchor 25/26 (wide red), fixdemo_select 8/8, fixdemo_table 7/7, fox_world 15/15 (all as on 82ad8564).
- gen --check: rc 0, 10,982 byte-identical.
- Ratchet, PesStallQuick: `pes_restart_delay_test --gtest_filter=PesStallQuick.*` on the tip: **forced_live = 53** (bound <= 53: at the bound; PASS; 390,986 ms).
- Ratchet, ALiveBall: `pes_restart_delay_test --gtest_filter='PesStall.ALiveBall*'` on the tip: **PASS** (1,324,838 ms; `EXPECT_LE(liveFirings, kBestMeasuredLiveFirings = 2229)` holds). The test prints no count on a pass, so the exact count is not recorded (OPEN: a failing bound would print it; not run, to save a 22-minute sim).

## 6. Landing verdict

**Not landable.** The gfx model is reverted to answer-0 (the ruling's fallback). The lane's named red (WideChannelScorerTakesTheElseArmAndEqualsPes) stays red: 25/26 as on the base.

## 7. Next exact steps

1. Main: decide the stale recompute record (section 1.4) and the block model's future (it needs a device-compatible answer, not a block of zeros).
2. Lane: experiment 3 (GF's cut-library entry 113 and the 0x1404B6400 reads, scratch only); then port the owner+0x228 pair producer only if experiment 3 names the field the pick reads from it.
