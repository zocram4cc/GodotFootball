# CutLane3: BeginEvent's crash fixed at the root; the wide-channel scene pick is still RED (named blocker)

Branch CutLane (worktree <workdrive>/gf-CutLane). Pushed `0c8047f1` (ls-remote confirmed). Labels: CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

## 1. Landed (pushed, green)

- `0c8047f1` fixdemo: `BeginEvent` calls PES's translated ctx reset `pes_fn_141F50B90` on the ctx instead of zeroing the whole 0x52E0 bytes. Pick cache (ctx+0x30) is still a fresh sentinel per event (`InitPickMap`).
- Root cause of the SIGSEGV (CONFIRMED): the zeroing wiped the second std::map head at ctx+0x40, which the constructor builds (`InitHistory(ctx+kCtxSecondMap)`). The goal arrangement's map walk (translated `0x14155E260`) then dereferenced null. Reproduced: probe with the kind-two event facts, rc 139 before the fix, rc 0 after.
- PES's reset does NOT write ctx+0x30 or ctx+0x40 (CONFIRMED: grep of translated `pes_fn_141F50B90.cpp` for +0x30/+0x38/+0x40 writes: none). So the old "reset frees the nodes" ponytail was wrong; the leak ponytail stays and is named in code.

## 2. Tests and mutation

- New red-first test: `GoalAnchorKindTwo.EventFactsBeforeTheArrangementKeepTheArenaValid` (tests/pes_goal_anchor_test.cpp). Facts are PES's seq204 ctx values (stadium 0x52, subject 1, event side 1, slot 8, goal diff -1, 11 players a side).
- Mutation (zeroing restored in BeginEvent, rebuilt, reverted): the new test SIGSEGVs, KILLED. Fix restored: passes.
- `pes_fixdemo_select_test` 8/8, `pes_fixdemo_table_test` 7/7, `pes_goal_anchor_test` 25 passed + 1 failed (`WideChannelScorerTakesTheElseArmAndEqualsPes`, the still-open wide test).
- `gen_translated.py --check`: started in background (log <workdrive>/gf-CutLane-scratch/gen_check4.log); no translated file was changed by this commit.
- `tools/check_pii.sh`: run from <workdrive>/GodotFootball, produced no output (exit code not captured through the pipeline; re-run pending).

## 3. Wide test: still RED. What was tried and what it shows

- Fed the seq204 event facts plus seq203's category table (tbl = 2,13,2,12,9,13,14,3,9,2,2) and Random1000 (seed 332164861, phase 640). Result: scene 1, bc 2 (PES 0x300D9). Neither the table nor the RNG nor the event facts alone move the pick (matches the earlier round).
- CONFIRMED: the ctx facts survive BeginEvent and `ZoneDecision` (zone 2) intact, and are cleared inside `ArrangeGoal`'s else arm (stadium 0x52 -> 0, subject 1 -> 0, event side 1 -> 0, slot 8 -> 0xFF, goal diff -1 -> 0). The slot pointer at 0x1437F5D50 is unchanged, so it is not a new ctx.
- CONFIRMED: the clobber is the recompute 0x140471A80 calling the event builder 0x14047C500 (translated `cutscene/pes_fn_140471A80.cpp`, call at its line ~1007). The builder reads the event's subject (+0), position (+4/+8/+C), slot (+0x10) and goal diff (+0x14), and calls the ctx setter 0x141F514D0. Its event argument there is a stack struct the recompute builds in its own frame (gpr5-0x59, filled from gpr5-0x21 by xmm0), not GF's event object.
- Tried and reverted: filling GF's `event` object (Zalloc'd, kEventBytes) in PES's layout. The builder never read it: no effect (CONFIRMED by probe).
- INF: the builder's source for those fields is the recompute's caller 0x14047B460 (untranslated) through the stack struct. The stand-in in ArrangeGoal (`gpr[8] = a0`, `gpr[9] = Zalloc`) is what the untranslated tick passes; it is NOT the builder's event argument, since the recompute sets gpr8 for the builder from its own frame. So the event facts reach PES's ctx only through the untranslated tick's arguments (or by the stack data the recompute reads from A0-relative frame slots). This is the next thing to decode: the recompute's frame reads at gpr5-0x21 / gpr5+0x6F, and 0x14047B460's call to the recompute.
- Matcher-input facts (CONFIRMED, seq203 vs seq204 ctx dumps, <workdrive>/gf-CutLane-scratch/ctxdiff.txt): the captured ctx differs between the two calls only at 0x38 (pick-cache size 4 -> 1), 0x5208/0x5210 (event position), 0x5244, and 0x5248/0x524C (scene 0x30113 -> 0x300D9). So the matcher's inputs are the captured ctx; GF's ctx after the arrange has to match it.
- PES's pick cache at seq204 (CONFIRMED, decoded from the capture): one node, key 0x300D9, value row 0 (pickdump at head 0x22848c60, root 0x31d0e4f0). The key is the scene id, not the cut group 0x30005.
- The doc's claim "PES's ctx at seq203 has zero stadium/players/event/subject" is WRONG (CONFIRMED: ctx+0x50 = 0x52, +0x5204 = 1 at seq203 and seq204). Corrected here.

## 4. run+0x18 writer (Main's item 2): not found this round

- Not attempted in depth: the time went to the BeginEvent root cause and the event-builder chain (section 3). The manual write at fixdemo_select.cpp (`kRunTarget`, the ponytail at `ArrangeGoal`'s end) stays. No translation and no deletion of the manual write.
- Next: the store to run+0x18 (scorer run object, G+0x21E8 slot 19, object 0x2DDCCFF4 at seq203/204). Store scan through 0x14047B460's callees, or diff seq203 -> seq204 writes at that address (the run object mirrors A0+0x30 at both captures, CONFIRMED in CutLane2-r2).

## 5. Open items (numbered)

1. Wide test red: the event builder's event argument. Decode the recompute's frame reads (gpr5-0x21, gpr5+0x6F) and the untranslated caller 0x14047B460's arguments; the kind-two event facts must reach the ctx through PES's own path, not through ctx stores in BeginEvent.
2. PES's stadium (ctx+0x50 = 0x52) is written by the setter path from the world's environment record, which GF does not build (ponytail at fixdemo_select.cpp, "environment record (world+0x35960+0x70), none"). The arrange clears it; this must come from the world.
3. run+0x18 writer: not found; manual write stays (ponytail).
4. Pick cache leak (ponytail): PES's clear is not translated.
5. Scratch: <workdrive>/gf-CutLane-scratch (text dumps, probes, no unpacked captures). Build dir <workdrive>/gf-CutLane/build was configured and built with ccache (kept for the next round).

## Verification summary

- Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER_LAUNCHER=ccache` (rc 0); targets pes_goal_anchor_test, pes_fixdemo_select_test, pes_fixdemo_table_test built under `/tmp/gf-build.lock`, nice 19, -j8.
- Env: PES_EXE=<local>/pes_full.bin, GF_DIFF_PYTHON=<workdrive>/gf-DiffHarnessRE-scratch/venv/bin/python, PES21_ROOT=<workdrive>/PES21; tests run under `env -u GF_DEBUG_RECEIVE -u GF_DEBUG_ANKLE -u GF_PES_SURVEY`.
- Probes (temporary, removed before the commit): BeginEvent/ArrangeGoal ctx dumps, the zone-decision bisect, the slot-pointer check. Scratch reads used `zr.py` on the packed captures (streamed, nothing unpacked).
