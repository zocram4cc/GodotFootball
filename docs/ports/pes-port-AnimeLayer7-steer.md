# PES parametric steering: who writes the blend, how often, and what GF does (AnimeLayer7)

Branch OneWorld-series-wip, commits fe3fa45d (z-arm) and 736c82d2 (series position + steering test). Evidence labels: CONFIRMED (decompile/asm/capture), INF (inferred), OPEN.

## 1. The setter and its one caller
- SetParametric FUN_141AF52B0 (A=rcx, desc=rdx, params=r8). CONFIRMED: its only caller in the image is the parametric flush FUN_1406E5FC0, call at 0x1406E6325 (`lea r8,[P+0x3B80]; lea rdx,[P+0x3B90]; rcx=[P+0xA08]`), return 0x1406E632A. CONFIRMED on 8/8 real captures (live2/pid661338 seq001-008, call.bin+384 = 0x1406E632A, P = call.bin+40). No vtable or table slot in the image holds 0x141AF52B0 (qword scan).
- The flush runs every sub-step of its callers FUN_1409F1BB0 (up to 60 sub-steps per call, message type 0x14, `uVar7 = 0x3C`), FUN_1409F20C0 and FUN_140A36ED0 (Ghidra UNCONDITIONAL_CALL). CONFIRMED.
- SetParametric is issued only when P+0x375E is pending (the flush's branch on P+0x375C == 2). CONFIRMED.

## 2. The fields that vary per request
- Position x = P+0x3BB4 (descriptor +0x24). Written by the request filler FUN_1406F14E0 from its param_7. CONFIRMED: SetParametric passes it as xmm2 to FUN_141AEE840, which brackets x on the rate table (rates at descriptor +0x18) and gives exactly 1.0 to the member whose rate equals x. Captured values: 0, 0.15, 0.3 (the rate-table values), 8 of 8 calls.
- Params blend = P+0x3B88 (params +8). Written by FUN_1406F14E0 from its param_6, which is FUN_1406E7CE0(P, x) = (1-w)·x + w·K (K = DAT_142590D98, w from P's state). CONFIRMED formula. Captured values 0.2667, 0.1667, 0.3. The kind byte P+0x3B84 is 0 in all 8.
- Writers of P+0x3B88 and P+0x3BB4: FUN_1406F14E0 only. Its callers: FUN_1407BEC50 (3 sites; 12 decision functions call it), FUN_1406B8340 (MOVESTEP tail; 3 callers) and FUN_1406F1280 (1 caller). CONFIRMED by xrefs.

## 3. How often (per request, not per tick)
- The 8 captures are the hook's first 8 calls of launch pid661338 (rv.calls 1..8). Their QPC gaps minus the snapshot pauses (`pause_secs`) are 0.4, 0.4, 0.9, 1.9, 1.5, 1.3, 1.8 s of game time. CONFIRMED arithmetic, INF that the pause sits between the two calls.
- So SetParametric is issued about once per second of game time, when a request is due. The per-tick work (flush sub-steps) runs every tick but issues the setter only on a pending request.

## 4. What the parameters do in the engine (measured in GF's engine, scratch experiments, reverted)
- Position x only changes the path, and it selects members exactly. Largest turn for the series at x: 0 gives 0.022976957 (member 0 alone: the same to 1e-17); 0.15 gives 0.000274 (member 1); 0.3 gives 0.021911 (member 2). CONFIRMED.
- Params blend 0, 0.1667, 0.3 and kind 0/1: no change to the turn. CONFIRMED (identical values to 17 digits).
- So the blend is not a steering input in this path. The steering input is the position.

## 5. The old test was measuring member 1 (CONFIRMED)
- IssueSeries wrote request.duration into descriptor +0x24. The test set duration 0.15 (seq002's captured position), so the "series" was member 3005 alone. The old assertion (series turns more than member 3633 alone) compared member 1 with member 0.
- Captured seq002 is a request on member 1 exactly (x = 0.15): PES plays 3005 alone for that request (weights 1.0/0.0 by FUN_141AEE840).

## 6. What GF does per tick
- The match never calls the setter: IssueSeries has no caller outside the tests (grep src/tests). CONFIRMED. The gdext PesMotionPlayer (src/gdext/pes_motion.cpp, Sample/Advance) stands in for SetParametric: two members around the rate, one phase advanced per tick. A stand-in, not PES's FUN_141AF52B0.
- Untranslated in GF (no pes_fn_*.cpp in src/pes/translated): FUN_1406E5FC0 (flush), FUN_1409F1BB0, FUN_1409F20C0, FUN_140A36ED0 (per-tick dispatchers), FUN_141EB5E70 (the anime tick: three FUN_141EB3AF0 on the adjuster sub-objects P+0x428/+0x498/+0x4D0, the adjuster vtable +0x18, FUN_141AF7A90, then A's update), FUN_141EB4650, FUN_1406F1280, FUN_1406B8340, FUN_1406B8761. FUN_1407BEC50 is translated (closure/); the translation status of its 12 callers is not checked.
- Translated: FUN_141AF7A90 (cutscene/), FUN_141EB3AF0, FUN_141EB39A0 (arm), FUN_141EB1BF0 (adjuster strike mode), FUN_1406E85B0 (displacement target), FUN_14079C2D0 (trap start), FUN_1406F14E0, FUN_141AEE840, FUN_1406E7CE0.

## 7. Where PES steers per tick (INF, from the code above; the adjuster is the consumer)
- Trap start FUN_14079C2D0 (translated) computes the target with FUN_1406E85B0 and arms the adjuster P+0x428 via FUN_141EB39A0 (translated). The per-tick consumer FUN_141EB5E70 (untranslated) ticks it through FUN_141EB3AF0 and the vtable, which adds the frame's root step into A+0x14 (FUN_141EB1BF0, translated).
- GF runs the arm but never the per-tick consumer, so GF's body never takes the per-tick root step. This is the known TrapContact gap (pes-port-TrapContact2/10).

## 8. Test change (commit 736c82d2)
- New test ASeriesFollowsTheMemberItsPositionNamesOnTheRateTable: a series at position 0, 0.15, 0.3 has the same largest turn (within 1e-6 rad) as member 0, 1, 2 alone; members 0 and 1 differ by more than 1e-3 rad. 14/14 anime tests pass; the mutation (position from request.duration) makes it fail at positions 0 and 0.3.
- Constants corrected: the captured params block is kind 0, blend 0.16666667 at P+0x3B84/+0x3B88 (r8 = call.bin+64). Commit 9f293a58 read call.bin+24, another object; corrected here.

## 9. Open
- OPEN: translate FUN_141EB5E70 (and the dispatcher FUN_1409F1BB0 that calls it per sub-step) with proofs, then drive GF's body root from the adjuster. This is the per-tick steering gap.
- OPEN (optional hook, Main's decision): `hook anime_tick_141EB5E70 0x141eb5e70 16 2 500 40534883ec30488bd90f297424204881` (no hook on the chain exists in hooks.cfg). The function runs up to 60 times per dispatcher call, so FIRST/EVERY must stay sparse or the snapshot pauses stall the match.
- OPEN: the request cadence and the blend formula constant K (DAT_142590D98) are read from the image, not measured at runtime.
