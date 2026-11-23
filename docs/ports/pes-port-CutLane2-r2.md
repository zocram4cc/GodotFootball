# CutLane2 round 2: the goal's control table (T3) is loaded; the scene pick is still wrong; run+0x18's writer is not found

Branch CutLane. Labels: CONFIRMED = tool output this round; INF = inference; OPEN = blocker named.

## 1. The scene pick (Main's item 2): CONFIRMED loader gap, fixed; a second gap remains

- `0x1404B6400` (BC<6) returns the u32 at +0x28 of the record `FUN_141F4FC30(ctx, cutId)` gives (decompile dec_b6400.txt, dec_4fc30.txt).
- `FUN_141F4FC30` reaches `0x140A7CAE0` -> `0x14428BBD0` (decompile dec_428bbd0.txt, CONFIRMED): a lookup over DB+cat*0x20 +0x2C8 (groups, u16 start/count), count u32 at +0x2D8, records at +0x2E0 with stride 0x2C. That table is PES's control table T3 (docs/re/pes-re-cutscenes.md section 2.2, "T3: CTRL table"). The 0x78-byte cut library at +0x168 is a different table (CB40).
- GF decoded T3 and discarded it (`fixdemo_table.cpp`, "consumed to keep the reads exact; not kept"), and `BuildDb` never built DB+0x2C8. So the lookup returned nothing. FIXED: the decoder keeps T3 (`FixDemoCtrlGroup`/`FixDemoCtrlRow`), and `BuildControlTable` writes it in PES's layout.
- Layout CONFIRMED on seq203 (PES bytes): group 0x30005 (CTRL index 4) = firstRow 245, count 99; its records have keys u16[10] at +0, nf at +0x14 (0), ni at +0x15 (1 in the sampled rows), floats at +0x18 (0), and the scene id u32 at +0x28. Record 6 of the group holds 0x300D9 (the scene PES picked). The file lists several ids per row in some rows; the runtime keeps the first (`ni` kept on the row). CTRL group count is 8 and DB+0x2C8 / +0x2D8 / +0x2E0 hold 0x2D3F44C0 / 8 / 0x323CAF10 in PES.
- After the fix, GF's lookup returns a real record (probe, debug build since reverted): `FUN_141F4FC30` gives record 0x24000D5660 with +0x28 = 0x300DB. PES's pick is 0x300D9. The pick differs by one row.
- OPEN (second gap): the arm (`ArrangeGoal`) still gives A0+8 = 1, not 0x300D9. The difference is in the matcher's state, not the table: (a) the ctx facts. PES's ctx at seq203 reads as zero at the stadium, players, event and subject fields (ctx+0x50, 0x2910, 0x5140, 0x5204 are 0), while the test does not call `BeginEvent`; (b) the match RNG (ctx+0x5228 == 2 selects Random1000 at 0x1437F6340, seeded by `Seed`, which the test does not call); (c) the category rotation word tbl[3] (PES 0xC at seq203). Which of (a)-(c) decides the row is not isolated. Next: call `Seed` and `BeginEvent` with PES's values in the test, or read PES's Random1000 state at 0x1437F6340 at the call.

## 2. The wide test (still RED)

- `GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes` now uses the install's goal table (`MakeInstalled`).
- T passes. `armed->sceneId` 1 vs 0x300D9 and `pos` fail (their cause is the scene row, which follows the scene).

## 3. run+0x18's writer (Main's item 1): OPEN, partly located

- CONFIRMED (streamed zstd, nothing unpacked): the scorer's run object (G+0x21E8 table, slot 19 entry at G+0x24E0 = 0x2DDCCFF4) has run+0x18 = (-37.56, 0, 40.56) at seq203 and (-41.6211, 0, 35.56) at seq204. A0+0x30 has the same values at both captures, so run+0x18 mirrors A0+0x30.
- CONFIRMED: at seq204 A0+8 = 0x300D9 and A0+0xC = 0 (PES's arm), so the recompute wrote the scene and the radius.
- CONFIRMED: A0+0x158 = vtable 0x14259C9E0 (the DemoInfo's sub-object), A0+0x168 = 0x10E1EB90, a command object (vtable 0x14259C940) whose +0x30 holds (scene << 32 | 1): 0x300AB at seq203, 0x300D9 at seq204. It is not the run object.
- Mutation (earlier round, reconfirmed): removing GF's manual run+0x18 write fails the T-writer test. The translated chain does not write run+0x18.
- OPEN: the writer. Candidates: 0x140A60770 (called by 0x14047AB20 on the object `0x1404511E0(A0+0x158)`) if that object's link reaches the run object; GF's stand-in for that link is missing. Next: decode 0x1404511E0 and 0x140A60770 and check the link from A0+0x158/0x168 to the run object.
- The manual write stays, labelled ponytail, until the writer is translated.

## 4. Checks run this round

- `pes_fixdemo_table_test` 7/7 (new `GoalControlTableKeepsEveryRowInItsGroups` GREEN). Mutation (decoder drops the control groups): KILLED by that test; reverted.
- `pes_fixdemo_select_test` 8/8.
- `pes_goal_anchor_test` 24/25; only the wide test is red (section 2).
- Probes (debug prints, two translated-function calls in a test) were added and removed before the commit.
- `ledger.py --check` and `gen_translated.py --check`: see the commit message for the final results.

## 5. Wrap-up state (session end, Main's ~20-minute window)

- Pushed: CutLane 9dd5011c (ls-remote confirmed). Tree clean at that commit; no uncommitted work.
- Gates for 9dd5011c: gen --check 10959 byte-identical; ledger current; check_pii pass (main checkout); table 7/7, select 8/8, anchor 24/25 (wide test red).
- Matcher-state probes (this round, all reverted, not committed):
  - seq203 Random1000 (0x1437F6340): seed 332164861, phase 640. Category table tbl[3] = 12.
  - Setting tbl[3] = 12 alone: armed scene stays 1. Setting Random1000 seed/phase alone: stays 1.
  - BeginEvent with the kind-two event facts crashes (SIGSEGV, unbounded recursion in translated 0x14155E260, the map-node walk reached from InitPickMap/reset). Not a verified production fault; the probe route is unreliable. Unresolved.
  - So neither tbl[3] nor the RNG alone moves the pick. The event facts (BeginEvent) remain the untested input.
- run+0x18 writer: 0x140A60770 is NOT in the translated layer. 0x1404511E0 is translated (closure). Decompile of both was started (dec_writer.txt) but not read. Next step: read dec_writer.txt; identify the store to run+0x18 (the object reached via 0x1404511E0(A0+0x158) / the command object at A0+0x168); translate 0x140A60770 with closure.py, prove on seq203 snapdiff, delete the manual write in fixdemo_select.cpp (line ~720).
- Next steps in order: (1) make BeginEvent safe (inspect 0x14155E260's map walk vs the InitPickMap header), then test the event facts; (2) the run+0x18 writer as above; (3) wide test green, mutation, gen/ledger --check, check_pii, push.
- Scratch: <workdrive>/gf-CutLane-scratch is 332 KiB of text (no unpacked captures; streamed reads only). Nothing to delete for captures.

## 6. Late result (after section 5)

- Decompile dec_writer.txt (CONFIRMED):
  - `0x1404511E0(obj)`: if obj+8 is clear, runs the vtable's allocation/notify and sets the flag; then returns obj+0x20 (or sets it to `[obj+0x10]+0xBC` once). It returns a sub-object pointer, not the run object.
  - `0x140A60770(obj, src)`: copies three u32 (12 bytes) from `src` into `obj+0x98..0xA0`, skipping the copy if the pointers are equal. It writes the command/sub-object, not the run object's +0x18.
- Conclusion: neither function is the writer of the scorer's run+0x18. The writer is elsewhere; the only translated path that touched it was ruled out by the mutation (manual write removed, test killed).
- Next step: find the store to run+0x18 (scorer run object, G+0x21E8 entry for slot 19, object 0x2DDCCFF4 at seq203/204) by a store scan through the run pointer in 0x14047B460 and its callees, or diff seq203 -> seq204 writes at that address. Do not assume 0x140A60770.
