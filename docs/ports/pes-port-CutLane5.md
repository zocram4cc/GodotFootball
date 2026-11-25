# CutLane5: round 1 fixed-block refusal was a rendezvous-hook artefact (round 2: proven both ways); the fixed-block tree aborts the sim and is reverted; wide test still red

Branch CutLane. Tip **c5937155c** (`git ls-remote origin CutLane` = c5937155c, CONFIRMED; tree identical to f4c4dd40, the round 1 tip). Round 1 was f4c4dd40 (a revert of 297071b0 on top of 82ad8564, Integrate). Worktree <workdrive>/gf-CutLane. Labels: CONFIRMED = read or run this round; INF = inference; OPEN = blocker.

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
| 0x141F7BFE0 proccam | results_proccam.json (18 calls: live2/pid3234533 seq6220/6224/6228, pid3741448 seq11590/11594/11598, seq71718..71750) | **NOT RE-RUN: captures absent.** pid3234533 and pid3741448 are in neither live2 nor the <archive-drive> archive; only trace dirs remain (gf-ProcCamera6-scratch/tr/seq71718 etc.). | same |
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

# Round 2 (Main's IRC: the counterexample is a rendezvous-hook artefact; re-prove with the launch's hooks restored)

## R2.0 Verdict

- Main was right, and my round-1 mechanism was wrong. The "fetch at 0x17B0000" is NOT after the thunk returns. The recompute's own **entry** is patched by a live rendezvous hook: `0x140471A80` begins with `ff25 00000000` and an 8-byte target `0x17B0000` (CONFIRMED, scan below). The replay therefore fetches the trampoline on its first instruction, before any thunk call. Section 1.2 of round 1 is withdrawn.
- With the launch's hooks restored, the recompute proof is bit-exact under fixed-block (1/1) AND under answer-0 (1/1, harness side changed for the run only, see R2.3). The landing condition holds for 0x140471A80 and 0x14047B460.
- **But the fixed-block tree (297071b0 content) aborts the sim**: PesStallQuick dies with `PES_DISPATCH_UNTRANSLATED: 0x1419CB300` (CONFIRMED, twice: with the key at +0x1C and at +0x20). So the fixed-block model is NOT landable. Reverted on the branch (section R2.6).
- Wide test: still red. The pick move (M2) takes the scene from 0 to 0x300DE, not 0x300D9. Not kept (the commit that carries it is reverted with the model).

## R2.1 Live hook patches in seq203 (CONFIRMED)

- Scan: `scan_raw.py` (PressLane3's) run on the unpacked copy `gf-CutsceneLine-scratch/recompute/seq203/mem.bin` (the snapshot used by the proofs; the live2 copy is packed). Six live `ff25 00000000` patches, all at function entries:
  - 0x140471A80 -> 0x17B0000 (the recompute itself)
  - 0x140474F00 -> 0x17B0100 (anchor calc)
  - 0x1404B88B0 -> 0x17B0200 (zone writer)
  - 0x1414406A0 -> 0x17B0500
  - 0x1420F0EB0 -> 0x17B0400
  - 0x14217DEF0 -> 0x17B0300
- The trampolines lie in the 64 KB page at 0x17B0000 that the snapshot maps as RWX and no PE module covers; the first fetch lands there. Main's "35 hooks" count vs 6 live patches in this snapshot: the scan is over every mapped, non-hole page of the full capture, so 6 is the live set at seq203 (INF for the other 29: they sit in pages no hook of this launch touched, or are in holes; not checked).
- Restore config: `hooks_for_image` over those 6 VAs (first 1, every 1, prefix rv), 6 lines, 0 refusals. File: gf-CutLane-scratch/cl5/hooks203.cfg (displaced bytes from the flat image, never from the snapshot).

## R2.2 Fixed-block re-proof (CONFIRMED, tip = 297071b0 content re-applied)

- Trace: `frame_trace.py recorded seq203 --hooks hooks203.cfg --out ... --spec-dir specs/core` (rc 0). Snapdiff: `snapdiff.py --hooks hooks203.cfg --only 0x140471A80`: **calls 1, replayed 1, mismatches 0, own_block 0.3913, proven True**.
- Thunk reach on this replay: a scratch counter in the harness's fixed-block branch (reverted) logged **60 thunk replays** in the recorded trace (CONFIRMED, probe/trace.log). The block path is exercised, not skipped.
- Tick 0x14047B460 (synthetic call over seq203, tick_calls.py, trace with hooks): **calls 1, replayed 1, mismatches 0, own_block 0.4146, proven True** (CONFIRMED).

## R2.3 Answer-0 re-proof (CONFIRMED, with a scratch harness edit that is reverted)

- The current answer-0 tree (f4c4dd40) refuses the thunk in the harness: run.py answers every gf-service stub with "reached" and excludes the call. The translated side is `pes_service_gfx_device_alloc` (rax = 0, pes_stubs.hpp:200). To run answer-0 at all, I added one line to run.py (`kind == "service_gfx_device_alloc"` -> rax = 0), ran the proof, and reverted run.py (checkout; `grep PROBE` clean).
- Result: trace rc 0; snapdiff **calls 1, replayed 1, mismatches 0, own_block 0.3913, proven True** (CONFIRMED, a0/res.json).
- Caveat: the harness's answer-0 is the one-line rax = 0 stub, not a model shared with the stub table. Its reach count was not logged (INF: same path as fixed-block up to the answer).

## R2.4 Pick finding and the M2 mutation (CONFIRMED on the fixed-block tip)

- The scene pick 0x1404B6400 reads the cut index at `[G+0x2C0]+0x1C` (dec_pick.txt: `0x141E5D4C0(_, *(lVar5+0x1c))`). seq203 holds 0x71 there (and 0x71 at +0x20). GF wrote the key only at +0x20 (ponytail: "the cut-library lookup id field"). Nothing in GF writes +0x1C.
- M2 (run): the stadium key also at +0x1C, fixed-block tip: wide test armed scene **196830 (0x300DE)** (was 0), T (-38.6197, 0, 30.3613) (was -57.58 / 0); PES 0x300D9 (196825), (-41.5502, 0, 34). The pick now returns a real scene from the cut row, 5 off in the low byte, and the target is 2.9 m off in x and 3.6 m off in z. Remaining difference: the row or variant choice (not decoded, OPEN).
- Moved the key to +0x1C in production (the +0x20 write removed; test helper SetStadiumCutKey moved too). Build rc 0; pes_goal_anchor 25/26 (wide red, new numbers), pes_fixdemo_select 8/8, pes_fixdemo_table 7/7, pes_fox_world 15/15 (CONFIRMED).

## R2.5 The sim abort (CONFIRMED; the reason the fixed-block tree cannot land)

- Commit 87f4f928 (fixed-block + key at +0x1C), gen --check rc 0 (10,990 byte-identical; gen_check_r2.log), PII grep of the changed files clean.
- PesStallQuick (`pes_restart_delay_test --gtest_filter=PesStallQuick.*`, slot): **aborts** at 5 s of the match: `PES_DISPATCH_UNTRANSLATED: 0x1419CB300`, then core dump, rc 134 (r2_quick.log). PesStall.ALiveBall aborts identically (r2_alive.log).
- Bisect: the key back at +0x20 with the fixed-block model: the same abort (bisect_quick.log). So the key move is NOT the cause; the fixed-block tree is (CONFIRMED, two runs).
- Who calls 0x1419CB300: no direct E8 caller in the image (callers.py, empty); it is reached through a pointer (INF). Not run to ground (OPEN).
- Scan of the baseline: f4c4dd40 (answer-0) passed PesStallQuick (forced_live 53) and ALiveBall (PASS) in round 1.

## R2.6 Branch state (pushed)

- Tip **c5937155c** (ls-remote confirmed). Its tree is identical to f4c4dd40 (`git diff --quiet f4c4dd40 HEAD` rc 0): the fixed-block commit and its key move are reverted by revert commits (9193acf45 and c5937155c history, net no content change). Pushed as a fast-forward; no force push.
- The round-1 ratchet numbers (PesStallQuick 53, ALiveBall PASS, bound 2229) were measured on this same content.
- Re-applying fixed-block needs the sim abort fixed first (its source: an untranslated callee of the block path, reached through a pointer; the next experiment is to log the call stack at 0x1419CB300 in a sim with the block model).

## R2.7 Recorded-roots list (Main's item 3)

- 0x140471A80: proven under fixed-block (1/1) and answer-0 (1/1) with the hooks restored (R2.2, R2.3).
- 0x14047B460: proven under fixed-block (1/1) with the hooks restored (R2.2). Under answer-0 not re-run (the tick's recompute reaches the thunk; the replay loops under answer-0, round 1 R2.5, INF it is still unprovable).
- 0x141F7BFE0 and 0x141F7AE60 (proccam): **NOT re-run. Captures gone**: pid3234533 and pid3741448 are in neither live2 nor the <archive-drive> archive (only the proccam trace dirs in gf-ProcCamera6-scratch/tr remain, no mem.bin). Their closures list the thunk (closure_proccam.json), so these two proofs are unverified under either model. Named as OPEN.
- 0x14091E9F0 (trapaction): not re-run (its 6 calls are not named in the record); no thunk member in closure_pressx.json (INF unaffected).
- Indirect reach: unchanged from round 1 (1,646 direct-reach functions, 0 recorded rows among them); the indirect edges are not enumerable from the image.

## R2.8 Tests and ratchets (tip c5937155c = f4c4dd40 content)

- pes_goal_anchor 25/26 (wide red), fixdemo_select 8/8, fixdemo_table 7/7, fox_world 15/15.
- PesStallQuick forced_live 53 (PASS); PesStall.ALiveBall PASS (bound 2229); both from round 1 on this content.
- gen --check on this content: rc 0 (round 1: 10,982 byte-identical).

## R2.9 Next exact steps

1. Find the caller of 0x1419CB300 in the block-model sim: a call-stack log at the abort (the untranslated dispatch in pes_dispatch_gen.cpp:22015 prints only the VA). The block answer may feed a vtable or an object PES reads a function pointer from.
2. Once the sim is green with the block model: re-apply the fixed-block commit (87f4f928 content) and the key move; re-run gen --check, PesStallQuick, ALiveBall.
3. Proccam proofs need new captures (hook 0x141F7BFE0 proc_camera, INIT calls) or an owner call on the archive; Main decides.

Rig requests: none.
