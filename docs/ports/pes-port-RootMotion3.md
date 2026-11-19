# RootMotion3: is GF's PES-clip advance slow? (12-11-26) -- DRAFT, updated as measurements land

Image `pes_full.bin`. CONFIRMED = decompile / constant read / tool run; INF = inference.

## 1. PES's clock advance per tick (CONFIRMED decode, captures read)
- The driver `FUN_141EB5E70(P, tick)` stores `P+0x41C = tick` and calls `FUN_141AF7A90(chain, tick)`, which writes the engine step size `*(*(*(chain+0x88)+0x10)+0x44) = tick * 300.0` (`0x1425A266C` = 300.0, read from the image). `tick` is `P+0x41C` = `0x14256AE1C / tickRate` = 1.0 / 54.0 s (constructor FUN_141EB42B0 at 0x141EB4333; `0x14351AE54` = 54.0).
- Layer position -> seconds: `FUN_141AF3100` returns `*(float*)(entry+0xC) * DAT_142AAAFA0`, `DAT_142AAAFA0` = 0.003333 (= 1/300, read from the image). So the engine steps clip time by `tick*300` units of 1/300 s = **1/54 s per tick at rate 1** (clip time is real time).
- The layer's own rate: `FUN_141AF6E40` (SetSpeed) stores the rate at `layer+0x14` and 1/rate at `+0x18`. Real calls (live2/pid4152866 `anime7_setspeed` seq002, 003, 004, 005, 009): rate (xmm1) = **1.0**; seq008: 0.9. So a nominal clip advances 1/54 s per tick.
- The elapsed counter ClipOwner ported (`FUN_1406E68C0` -> `54 * seconds + 0.5`) therefore advances **1.0 per tick** in PES.

## 2. GF's advance per tick (CONFIRMED code)
`Humanoid::Process` (`humanoid.cpp` ~292): `framePhase += playbackRate; frameNum += int(framePhase)`: **one 10 ms animation frame per tick at rate 1** (the clip text `PesClipAnimText` is sampled every `kGfFrame_s` = 0.01 s). A tick is `kTick_ms` = 1000/54 = 18.52 ms. So GF advances 0.01 s of clip time per tick against PES's 0.018519 s: **ratio 0.54** (INF -> pending the capture comparison in section 3).

## 3. Capture comparison (CONFIRMED, seed 7 pes-loop probe on the BJ-merged scratch tree, arm2.log)
For each trap start: PES's strike-adjuster arm `ticks` (translated FUN_141EB39A0 over the real motion table = PES ticks to the contact frame) against GF's `touchFrame` of the same clip and the clip's `playbackRate` (the trap rate 1.12):
| t, man | GF clip | PES arm ticks | GF touchFrame | GF ticks to touch = frame / rate |
|---|---|---|---|---|
| 195, p10 | trap_0_3_135_y0_in_ver12 | 17 | 33 | 29.5 |
| 349, p25 | trap_0_0_f045_y0_sole | 17 | 33 | 29.5 |
| 1161, p17 | trap_0_0_000_y0_in_mirror | 17 | 33 | 29.5 |
| 2493, p24 | trap_0_3_090_y0_in_ver12_mirror | 17 | 33 | 29.5 |
Expected from the decode: 33 frames * 0.01 s * 54 / 1.12 = 15.9 ticks, +1 (the arm's `iVar6 + 1`) = 17: **PES reaches the contact in 17 ticks, GF in 29.5: ratio 0.58 with the arm's +1 (0.54 of the clock itself). GF plays a PES clip at 0.54x PES's speed: CONFIRMED.** (The same arm state shows only one step per trap on this tree: it predates `93f5f144`, section 5 of RootMotion2.)

## 4. Consequence beyond the clip clock (CONFIRMED by reading, INF for the size)
`spatialState.actualMovement = step * 100` assumes a 10 ms tick, and a GF clip advances one 10 ms frame per tick, so for GF's OWN clips that product is the clip's m/s (and the man covers 0.54 of that distance per real second: every GF-native clip is in slow motion in the 1/54 s tick; not touched here). A PES clip advanced at real time must use `step * 54`, else the speed classes read 1.85x.

## 5. The fix (branch RootMotion, worktree gf-RootMotion2)
- `HumanoidBase::OnPesClip()` / `ClipFramesPerTick()` (humanoidbase.cpp): a PES clip (Special, dodge/tackle/locomotion/trap family) advances `kTick_ms / 1000 / pes::kGfAnimFrame_s` = 1.852 frames per tick at rate 1; a GF clip stays at 1.
- `Humanoid::Process`: `framePhase += playbackRate * ClipFramesPerTick()` (the issued rate keeps its PES meaning: tests that read `playbackRate` are unchanged).
- `CalculateSpatialState`: `actualMovement = step * (OnPesClip() ? 54 : 100)`.
- `pes::kGfAnimFrame_s` (pesclips.hpp) is the one 10 ms constant `pesclips.cpp` already used.
- Red test: `MoveStepWire.APesClipAdvancesATickOfRealTimePerTick` (mean frames per unit rate over locomotion-clip ticks away from the clip end must be 1.852 +-3 %; the base gives 1.0).

## 6. Root travel against a capture (Main's check; CONFIRMED for one clip)
- Unit: PES reads a root's speed as `|dxz| * 54` m/s (FUN_1406EA110: root history P+8 / P+0x14, pes-re-gaps-anime.md section D1). GF's `actualMovement` for a PES clip is now `step * 54`, `PositionAt` hands PES `GetMovement()/54` = the step.
- Capture `live2/pid3332804/seq077` (trap_action_tick, P = rdx): one entry in the motion table, motion 31 `backslant_1_1_relax`, seconds 0.58471, advance this tick 0.018981 s = rate 1.02495 / 54 (the clock decode read straight from the table: entry+0xC), root P+0x14 - P+8 = (0.02456, 0.02239) = **0.03323 m**. The clip's root track (GF's `PesClipAnimText` positions, 71 frames, interpolated) over [0.58471, 0.58471 + 0.018981] s = **0.03333 m: ratio 1.003**. `seq175` (two layers blending, motions 3555 and 2828, advances 0.020589 = 1.1118/54 and 0.018944 = 1.0230/54: the clock again exact) cannot be compared per clip (the root is the blend).
- Test `MoveStepWire.APesClipsMovementIsItsDisplacementPerTickTimesTheTickRate`: sum of GF's m/s / 54 over the men's displacement on PES locomotion clips = **1.0047** (701.8 m), mutation (x100) reads 1.858 (red).
- Commits on RootMotion: `8a50155f` (frame advance + scale + red test 1), `9b10d794` (constant aligned with ClipOwner), `253b17e8` (the movement test).

## 7. What the fix exposes (A/B on the RootMotion tip, env switches RM_NOF = old frame advance, RM_NOS = old x100 scale, uncommitted, removed)
1. **Penalty shootout never saves: NOT the fix.** `--seed 7 --minutes 1 --pes-loop --set penalty_shootout_only=true`: TeamTick base: 8 kicks, 2 saved, "won by HOM 4-2". RootMotion tip with BOTH switches off (pre-fix behaviour): 541 kicks, 541 goals. Fix only on: 482/482 goals; fix+ClipOwner: 242/242 and counting. The keeper stopped saving somewhere in the RootMotion branch's earlier commits (not bisected). A level pes-loop match never ends (tick cap); ClipOwner+fix seed-7 2-min smoke is level after regular time and enters that endless shootout, so "ticks to full time", census % and smoke hash CANNOT be read for it. OPEN, needs a bisect of RootMotion `f84b1d99` back to the base (shootout-only run, ~7 min each).
2. **First-stoppage stall: the fix.** RootMotion tip pes-loop seed 7 2 min: with the fix `[restart] stalled: no restart within 1758 ticks` at the first touchline throw-in (the taker H2 walks 33 m, is teleported back to (7.95, 21.11) and idles 15 m from the ball in GF's idle clip; A8 plays GF `sliding/idle` 0.8 m from the ball; `trace_fix.log`). With both switches off: no stall in 400 s. Clock only or scale only: no stall in 250 s. ClipOwner+fix passes that stoppage. Not yet attributed to a line (the taker's MOVESTEP is in state 9 <- 7 and idles; most likely a clip-end/arrival test now crossing in a different tick order, since PES clips end 1.85x sooner).
3. **Idle stalls 4 -> 17 (StepLand2's):** not traced. By reading: `Team::PassGetReceiver()` (team.cpp:449) holds a receiver while `newest.kind` is a Pass/HighPass of this side, with no test that the ball still moves, so after the pass dies (ball at rest, no newer event) the stored pass target stays the receiver, and `_PassGetApproach` (elizacontroller.cpp:1143) runs for the designated man instead of the press approach; `pes::ReceivingPoint` on a path whose 249 samples are all the resting ball gives a point at the ball, not "8 m ahead at 8 m/s", so the cmd=0 heading-90 request is still unexplained. The targeted print was not run (the sims at tip stall at the first stoppage with the fix, and the pre-fix run takes ~30 min under load).
