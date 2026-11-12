# PassReceiver: PES keeps the pass receiver while the pass ball lies at rest, exactly as GF does; no divergence in the receiver rule, no branch commit (12-11-26)

Worktree `<workdrive>/gf-PassReceiver` (detached at `origin/RootMotion` `89d0b5f0`, godot-cpp symlink, never built, no commits: nothing to fix, see section 4). Image `pes_full.bin`. CONFIRMED = decompile / image read / real capture read this session; INF = inference; OPEN = named.

## 1. The answer (PES, CONFIRMED by decompile)
1. **`T+0x78` stays.** `0x1404FB380` clears it only when the owner `MA+0x2601C <= 0x15` (`0x1404FB3E7`), the ball record is inactive (`FUN_140A57940`), or the frames since the newest event are <= 0 (`FUN_140A51320`); `iVar9 < 1` also clears. No term reads the ball's speed. A loose pass ball at rest keeps its receiver until a man touches it (owner becomes a man). The kick leaves the owner at 0xFF (live: seq732 loose, owner 0xFF, T+0x78 = 9; seq850 after the touch, owner 9, T+0x78 = 0xFF; `pes-port-TeamTick.md` section 1). GF runs PES's own translated `0x1404FB2A0 -> 0x1404FB380` on the window, so GF's `T+0x78` rule IS PES's (given the owner input, which GF stages from its ball retainer: a named `ponytail:` in `Team::RunPesPassTarget`).
2. **His decision does not change with ball speed.** The mover selector `FUN_140933450` takes `FUN_140931c60` (PASS_GET): `T+0x78 == me`, the newest event's key test (`FUN_140A60530`), not the owner (`MA+0x2600C`), anime class tests (`FUN_140A6E050/1C0/5E0`, `+0x1A/+0x1C` window), no teammate among the three nearest the ball (`MA+0x24840`) already owning the trap, the 135 deg / 3 m capsule arbitration against the other side's target (`0x14259BE60`). **No speed, height or rest test anywhere in it** (read in full, 99 lines). The `passget.json` speed constants `ballSpeedMin/Max` 4/42 sit in ActionPassGet's route bodies (not reduced by anyone).
3. **What he does with a resting ball: he walks to it (ActionPassGet `FUN_140945EC0`, route BallFollow) and the reaction plays it.** The receiving point is `FUN_140A4D340 = FUN_140A54910(B, me, 0, 0xF9)`: the nearest ball-prediction sample over the segments between bounces (translated, in the layer). `FUN_140944C90` then forces the arrival frame to 0 when that search returns > 0xF8 and the ball's horizontal speed is under `0x1425CB49C / (fps * 0x142598590)` = 5000 / (54 * 3600) = 0.0257 (both read from the image; B+0x2948 is m/s, so this is a standing-still test): a stopped ball means "the ball itself". For a resting ball every sample is the ball, so the point is the ball. GF's `ReceivingPoint` (`pesreceive.cpp:221`, nearest sample, same clamp 0.163 m inside the lines) gives the same point.
4. **Who plays it: reaction entry 3 `FUN_1409F8220`, every frame, for every man** (gates 1 to 4e, `pes-re-first-touch.md` 7.2), NOT the PASS_GET state: within 10 m (`ball distance^2 <= 100`), class-7 admission `0x14079BBE0` (gate 4a), not the designated owner, newest-event and re-touch lockouts, then the trap request. A stationary man's first touch is forced to kind 0 (`stopTrapStopTurnAngle` -1). The ball leaves the receiver's responsibility only at his touch (owner = him, `T+0x78` = 0xFF).

## 2. Real captures (CONFIRMED reads: `G = *0x1436F3DC8`, `T = *(G + 0x28 * (0xA1 + side))`, `man = *(G + 0x28 * (id + 0xBF))` (AI action word `+0x5D0`), `anime = *(G + 0x28 * (id + 0xA5))` (action `+0), ball `B = *(G+0x40)`, velocity `B+0x2948`; scratch `scanT2.py`, 45 captures of `live2/pid3332804` read through scratch copies; the live2 lock files are root-owned so nothing was unpacked in place)
One real pass episode, man 6 (`T+0x78 = 6` on all of them, side 0, `MI+0x116C = 1`):
| capture (hook) | ball speed | man 6 to the ball | `man+0x5D0` | anime action |
|---|---|---|---|---|
| seq051 (trapadmit_140789E60) | 17.4 m/s | 11.5 m | 5 PASS_GET | 2 MOVE |
| seq064 | 13.3 | 7.1 m | 5 | 3 STEP_MOVE |
| seq075 | 9.5 | 2.7 m | 5 | 3 |
| seq077 (trap_action_tick) | 9.2 | 1.8 m | 5 | 3 |
| seq083..088 (trapscan, in the Trap action) | 7.0 | 0.51 m | 3 | **7 TRAP** |
So PES's receiver is in PASS_GET (AI action 5) for the whole approach, `T+0x78` stays through the trap's flight (the owner is still 0xFF), the AI word leaves 5 when the man is in the Trap action, and the target is cleared only by the touch. Other pairs: seq117/137 (`T+0x78 = 2`, 5 at 2.8 m, ball 9.2), seq1452/1453 (side 1, man 14, 8.4 -> 8.0 m, ball 11.2). **No real capture exists of a pass ball at rest with a receiver**: in 45 captures none has a ball under 1 m/s with a valid `T+0x78` (live PES receivers reach the pass; the 40 live PASS_GET episodes last 0.02 to 2.94 s, `pes-re-receiving.md` 5.2). The at-rest answer is therefore the decompile (section 1), not a capture; a capture would need a hook on `0x1404FB380` predicated on `|v| < 1 and d(T+0x78) < 0x16` (ask Main only if the owner needs it; the other captures of the 262 on disk were not scanned: 217 are ball-physics / animation hooks, scanning each costs ~30 s).

## 3. GF against PES
| point | PES | GF | verdict |
|---|---|---|---|
| `T+0x78` kept while the ball rests | yes (no speed term) | yes (the same translated chain) | MATCH (RootMotion4's print is PES behaviour) |
| PASS_GET selection | no speed term | `PassGetSelected` (ported, oracle-proven) | MATCH |
| receiving point at rest | the ball | the ball | MATCH |
| the walk to it (BallFollow speed and facing slots; ThrowIn, NextPlay, BallAvoid, MovePositionAdjust, MoveStraightTrapStop, TargetEnemy routes) | slots `+0x40..+0x70` of the route objects (`slots.c` 6.2k lines, never reduced) | `_PassGetApproach`: distance-to-speed ramp, `ponytail:` | **GF lacks PES's route bodies** |
| who plays the ball | reaction entry 3, every frame, class-7 admission, trap chain | the same chain (`_ReceiveCandidate` with gate 4a from TrapAdmit, `_TrapCommand`) | MATCH after TrapAdmit |
| the touch itself | the body's contact scan `FUN_1406EDCE0` over the clip's bones (TrapContact: bit 9 of `P+0x2460`, the motion table, the slot `+0xB8`) | GF's `touchframe` touch with `GetTrapVector` | **GF lacks it** (TrapContact / TrapAdmit2 own it) |
| how fast he arrives | the 40 live episodes: 0.02 to 2.94 s | seven seed-7 stale spans of 3.3 to 5.4 s (TeamTick7 section 2): MOVESTEP start-up late (turn-handler re-ports, MoveStepCell3) | **GF slower; not the receiver's rule** |

## 4. Verdict and what is next
GF matches PES on every receiver rule that decides what happens with a resting pass ball (the target stays, he is selected without a speed test, he walks to the ball). So RootMotion4's idle stalls ("H6 still receiver at 0.28 m/s, `_PassGetApproach` keeps walking him") are PES behaviour; the defect is that the walk or the play is late or never completes, not that the receiver is kept. I made no code change and no branch commit (a "clear the receiver when the ball stops" fix would be a non-PES rule).
What PES does next that GF lacks, in this order of leverage:
1. **The trap's touch** (TrapContact / TrapAdmit2): the receiver beside a resting ball is admitted and starts the trap, and in GF the clip's `touchframe` touch is skipped when GF's own distance gate fails (`humanoid.cpp:689`, `fullBallDistance < touchableDistance * scale`), so a 0.4 to 0.5 m trap ends without a touch and `T+0x78` is never cleared (TeamTick6 section 3). PES's touch is the body scan (`FUN_1406EDCE0` -> slot `+0xB8`), no frame test and no ball-state test. The stale-target red clears when this lands.
2. **The arrival**: MOVESTEP's start-up (MoveStepCell3 Left 1) for the 3.3 to 5.4 s walks.
3. **The route bodies** of ActionPassGet (BallFollow speed and facing slots first, then MoveStraightTrapStop, which brakes him for the trap): the `ponytail:` ramp in `_PassGetApproach` names them; needs the 6.2k-line `slots.c` reduction and real captures of the route functions (no hook exists; `FUN_140945EC0` is hookable only if its prologue allows: not checked).
4. The owner input to `0x1404FB380` is GF's retainer, not PES's `MA+0x2601C` (W1's real write). PES's owner between kick and touch is 0xFF per the earlier captures (seq732/seq850); I did NOT read `MA+0x2601C` in this session's captures (INF that it is 0xFF in seq083..088, because `T+0x78` is still set there).

## 5. Tests / mutations
No test written (no source change). Nothing built. Captures read-only (scratch copies deleted).
