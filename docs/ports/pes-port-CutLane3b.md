# CutLane3b (addendum to pes-port-CutLane3.md): the DemoInfo tick 0x14047B460 and the event struct; NOT translated this round

Branch CutLane, last pushed commit 0c8047f1 (unchanged by this addendum). Labels: CONFIRMED = read this session; INF = inference; OPEN = blocker.

## 1. The tick 0x14047B460 (CONFIRMED from dec_7b460.txt, Win64 args)
- Signature: `(A0 = rcx, event = rdx, list = r8, r9 = object with +0x1bc..+0x2230, arg5 = [rsp+0x28], arg6 = [rsp+0x30])`.
  A0 is the DemoInfo (zone at A0+0xBC via `param_1[0x2f]`, decision count at +0xDC).
- The tick calls the recompute as `FUN_140471a80(A0, event, arg5, arg6)`: the recompute's r8 is the tick's 5th argument and its r9 the 6th.
- GF's stand-in (fixdemo_select.cpp, ArrangeGoal else arm) passes `r8 = a0` and `r9 = Zalloc(...)`. That is WRONG: PES passes the tick's own arguments. The "ponytail" in ArrangeGoal names this; it is not yet a proof either way.
- Recompute's r8 is a cut-list object (count at +0xC, 0x1C-byte entries, written as `param_3[3]`, `param_3[uVar9*7+4]`), not the event. The event struct is NOT r8.

## 2. The event struct the builder reads (CONFIRMED, translated cutscene/pes_fn_140471A80.cpp ~lines 960-1007)
- The recompute builds the struct in its own frame at gpr5-0x59 (a stack struct), from: gpr5-0x21 (a u32 the earlier code computed), gpr5-0x11 (a u32 from 0x140A3B040), gpr5+0x17 (0x140A3B2A0's output), gpr5+0x6F (a pointer, read as the builder's gpr2).
- Those four callees are TRANSLATED and read world state:
  - 0x140A489D0: `if (u32 index >= 2)` returns `*(G+0x1978 + (index+163)*5*8)`, else returns the pointer `G+0x1978` itself.
  - 0x140A3B040 / 0x140A3B2A0: read the MI table (0x14351AE30/34), [r8] and per-man `+0xD8` fields.
  - 0x140A62EA0: reads `[G]+0x3C` style fields keyed by the slot.
- CONFIRMED: PES's G+0x1978 at seq203 (G = 0x2DB0C420) holds the pointer 0x2DD5A0D8. GF's world never writes it (grep: no `0x1978` store in fixdemo_select.cpp), so GF's 0x140A489D0 returns 0 and the event struct comes out zeroed (subject 0, slot 0xFF, goal diff 0), which is the clobber measured in CutLane3.
- The only translated store to G+0x1978 is in `camera/pes_fn_14089B780` (CONFIRMED, line 45: `s.wr(G+0x1978, 0, 8)`, a zeroing init of the camera block 0x1958..0x1998). It writes 0; it is NOT the writer of the pointer 0x2DD5A0D8. The real writer is OPEN.

## 3. run+0x18 (OPEN)
- The tick does not store to the run object (G+0x21E8 slot 19) directly; the recompute's writes go to the command object (0x1404511E0(A0+0x158) -> obj+0x98..0xA0) and A0+0x30. The run+0x18 writer is still OPEN. Not located this round.

## 4. Not done (named)
1. Translate + prove the tick 0x14047B460 (and call it per tick in FixDemoSelector with PES's own arg5/arg6). Blocked by time: the gen_translated --check I started in CutLane3 has been running for over 35 minutes and has not returned. A new translation's regeneration cannot be confirmed in this window.
2. The writer of G+0x1978's pointer (0x2DD5A0D8) and of the slot-indexed records (0x140A489D0 table). Decode needed: store scan for +0x1978 in the image (not only translated code) plus the seq203 -> seq204 diff at 0x2DD5A0D8.
3. The wide test stays RED (scene 1, PES 0x300D9).

## 5. Checks
- Nothing new was committed in this addendum. Last green state: pes_fixdemo_select 8/8, pes_fixdemo_table 7/7, pes_goal_anchor 25/26 (only the wide test red), PII clean (main checkout).
- gen_translated.py --check: still RUNNING at the time of writing (log <workdrive>/gf-CutLane-scratch/gen_check4.log). Not confirmed.
