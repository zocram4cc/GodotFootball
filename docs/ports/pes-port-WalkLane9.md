# PES port: WalkLane9 (branch WalkLane; pushed 08d6a3ad8 = Integrate a316278d1 merged, no lane code; NOTHING LANDED)

## 0. Verdict
- Item 1 (writers and order of B+0x90 and O2+0xD0): DECODED. The warp GF reads is zero because PES's composer overwrites the root point O2+0xD0 with B+0x90 in the same driver call, before GF reads it. CONFIRMED by a scratch probe (section 2).
- Item 2 (port, re-measure): the composer handoff is ported as a probe-gated patch. ATrapsRoot 0.156216 m -> 0.0086346 m, still red (1 mm). PesStallQuick 53 -> 19 (passes). EveryPassGetEpisode stays red, median 1.63 s -> 2.19 s (worse). ALiveBall not run. NOT landable under M1 or M2 (target still red; ALiveBall unmeasured). Reverted. Patch kept: `gf-WalkLane-scratch/wl9-handoff-and-probes.patch`.
- Item 3 (capture): not needed. Main delivered 12 `adjto_141EB3360` captures (live2/pid<newest>) for WalkLane8's ask; not used yet (section 5).
- Scenario claimed: none. Ratchet on the pushed tip: unchanged (no behaviour change pushed).

## 1. Build blocker (reported to Main; not fixed here)
- origin/Integrate a316278d1 still tracks `src/pes/translated/out:_career/`. Make fails (`target pattern contains no '%'`). Ninja builds. This lane reconfigured its build dir to Ninja (`build/`, `-G Ninja`); the Make dir was deleted. Build targets then link.

## 2. Decode (ordering, per tick)
Field names: A = P+0x428 (strike adjuster), B = A+0xF0 (`kPhysiqueNextOffset`), O2 = A+0x188 (`kAChildO2Offset`), O2+0xD0 = layer root point (matrix translation, `matrix = O2+0xA0`).

PES (translated code, src/pes/translated):
- Driver FUN_141EB5E70 (gameplay/pes_fn_141EB5E70.cpp): steps A (+0x428), height (+0x498), yaw (+0x4D0) via FUN_141EB3AF0; then P.vt+0x18 = FUN_1406EEDF0; then FUN_141AF7A90; then chain vt+0x38 = runner FUN_141AFC4A0. CONFIRMED.
- FUN_1406EEDF0 (rootmotion/pes_fn_1406EEDF0.cpp): B+0x81 = strike-valid flag (lines 79-99 and 118); under the strike branch B+0x90 (x) and B+0x98 (z) = A+0x14 (lines 146, 160). Under the non-strike branch also B+0x90 is written (line 146). CONFIRMED.
- Composer FUN_141AFD400 (closure/pes_fn_141AFD400.cpp): reached through the layer vtable slot FUN_141AFD3C0 (cutscene/pes_fn_141AFD3C0.cpp:39). Under B+0x81 it replaces the matrix translation (`matrix+0x30`, = O2+0xD0) with B+0x90..0x98 (lines 1537-1555, masked). Otherwise it composes the clip step onto it. CONFIRMED by probe below.
- FUN_141AF3CA0 reads O2+0xD0 into A+0x14 at counter 1 (gameplay/pes_fn_141AF3CA0.cpp:24-33).

GF per tick (src/sim/onthepitch):
- match.cpp:896-898: WritePesMovementRecords (ManRecord <- body), WritePesTrapActors, RunPesTrapTicks.
- teamAIcontroller.cpp:3477-3511: WriteActorP (pestrapactor.cpp:250) writes O2+0xD0 <- ManRecord pos, then RunAdjusters (pesworldrecord.cpp:1245) = PES's driver. Same order as PES.
- player.cpp:469: StepAdjusters -> ReadAdjusterFrame (pesworldrecord.cpp:1212): reads `B+0x90 - O2+0xD0` AFTER the composer has written O2+0xD0 = B+0x90. Result: 0 on every composer tick. This is the bug.

Probe (scratch, `GF_WL9_PROBE` in RunAdjusters, 600 rows, ATrapsRoot seed 7, tip 08d6a3ad8):
- Strike rows (A.state == 2, 560 rows): A+0x14 == B+0x90 == O2post to 6 decimals on every row (e.g. count 2: A14 (-0.793575, 8.721437), B90 same, O2post same). O2pre (= ManRecord at WriteActorP) differs from A+0x14 by 0.0174 m on count 2 and 0.0168 m on count 3. So the warp the body should take is A+0x14 - O2pre, not 0.
- Idle rows (A.state 0 after the driver, B+0x81 = 1, 40 rows): O2post == B+0x90 again (B90 about -22.01, A14 = 0). So B+0x81 gates the composer's replacement, not only A's state 2. The port uses B+0x81.
- CONFIRMED: PES's composer writes O2+0xD0 = B+0x90 after B is set; GF's warp reads that overwritten value.

## 3. Port tried (patch: gf-WalkLane-scratch/wl9-handoff-and-probes.patch, reverted)
- `ReadAdjusterFrame` returns `strikeValid` (B+0x81) and the root point (B+0x90, B+0x98), not a displacement (pesworldrecord.hpp/.cpp).
- `pesmovestep::StepAdjusters`: `body->SetStrikeRoot(strikeValid, point)` replaces `WarpRoot`.
- `HumanoidBase::CalculateSpatialState`: while valid, startPos := point - clipOffset - smuggles; nextStartPos and previousPosition2D take the same shift (WarpRoot semantics, so the handover is not a step). position = point exactly, the clip track adds nothing (composer replacement). Model follows PassArrival round 3, which this lane re-implemented on the current tip.
- Probes in the patch: `GF_WL9_BASE` (disable the handoff: baseline) and `GF_WL9_OFF` (log offsets on handed-off bodies). Both env-gated, both to be removed.

## 4. Measurements (same binary, env toggles; seed 7)
| run | ATrapsRoot worst gap | ticks | EveryPassGetEpisode over 2.94 s | median / longest | PesStallQuick forced_live (bound 53) |
|---|---|---|---|---|---|
| baseline (`GF_WL9_BASE=1`, = tip) | 0.156216 m (red) | 222 | 10 of 30 | 1.63 s / 11.72 s (red) | 53 (PASS) |
| handoff (fix) | 0.0086346 m (red, 1 mm) | 320 | 9 of 26 | 2.19 s / 12.13 s (red) | 19 (PASS) |

- Mutation check: the baseline toggle reproduces 0.156 m, the fix gives 0.0086 m: the handoff is what moves the number.
- ALiveBall (bound 2229): NOT run. Required for any landing (M1/M2). Not measured on the fix.
- gen --check: NOT verified (timed out at 900 s; `--image $PES_EXE` was passed). The pushed tip is Integrate's own content plus a merge.

## 5. Residual (the 8.6 mm)
- Offset probe: 2254 `OffsetPosition` calls on handed-off bodies (strikeValid), most 1e-4..1e-3 m, some 4e-3..1.1e-2 m. The large ones have the size of the residual.
- Callers in GF: match.cpp:1586/1587 (player separation: `offset1/2` from the collision pass, after the humanoid step), match.cpp:1433 (ball bounce push). Not isolated per tick (INF that separation is the source).
- The separation push is a named GF stand-in (match.cpp:1577-1587, no PES writer decoded). It moves a handed-off trapping man after the composer has set his root.
- Next step: decide whether PES's own separation moves a trapping man (decode PES's contact/separation writer), or exempt handed-off bodies (that is tuning a stand-in: not allowed without PES evidence).

## 6. Open items
1. Residual source: GF-side probe per caller (separation vs bounce) on the ATrapsRoot ticks; then PES decode of whether separation applies to a man in the Trap action.
2. ALiveBall on the handoff (20 min, Main or the next lane). Required before any M2 raise.
3. EveryPassGet median rise (1.63 -> 2.19 s): trap-end timing after the strike ends (the clip continues from the absorbed point). Not decoded here.
4. The 12 `adjto_141EB3360` captures (live2/pid<newest>, Main's relaunch 4): check the counter/budget (A+0xC, A+0x10) against GF at the drop tick (WalkLane8's item 1 ask). Not used yet.
5. Make build of origin/Integrate: `out:_career` (Main's generator fix).

## 7. Next highest-value step
Item 1 above: the separation push on handed-off bodies is the only thing between this port and red-to-green on ATrapsRoot. Once it's settled, the handoff is a M2 candidate (named stand-in: match.cpp separation) with its own lane.

## 8. Tests run (this lane)
- `pes_pass_target_test --gtest_filter=PesPassTarget.*`: fix 1 PASS (IsSetInFlight skipped, named gap), 2 FAIL (ATrapsRoot 8.6 mm, EveryPassGet); baseline same 2 FAIL.
- `pes_restart_delay_test --gtest_filter=PesStallQuick.*`: fix PASS (forced_live=19), baseline PASS (forced_live=53).
- No test changed. Pushed: 08d6a3ad8 (ls-remote confirmed). Build dir `gf-WalkLane/build` (Ninja) kept until the end of this report.
