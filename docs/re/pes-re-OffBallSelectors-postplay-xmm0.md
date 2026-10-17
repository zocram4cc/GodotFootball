# PostPlay lane 0 (FUN_14061fa40): what xmm0 holds on each exit (decision 2)

Source: raw disassembly of 0x61fa40-0x62057f (single `ret` at 0x62057f; 0x620588-0x62058f is int3 padding). Dump: `<workdrive>/gf-OffBallSelectors-scratch/lane0.asm`. Win64 ABI: xmm6-xmm15 are callee-saved (prologue saves them at 0x80-0x110(%rsp)).

## Result

On every early exit xmm0 = **0.0**. On the one success path xmm0 = **10000 - D** (K = 10000 at VA 0x14259d634), where D = squared distance from the runner to the team's receiver, read from the runner's sorted distance list by `FUN_14061f8b0`. So the lane-0 score is either 0 or 10000 - d^2(runner, receiver). A runner with score 0 is not a candidate.

## Exit sites (each cited by its address)

| # | site | xmm0 source | value |
|---|---|---|---|
| E1 | 0x61faf1 `xorps %xmm0,%xmm0`; `jmp 0x620561` | explicit zero; reached when latest event targets the runner and `frames < int(fps+0.5)` (0x61fa40-0x61faef) | 0 |
| E2 | 0x61fb9b `xorps`; `jmp 0x620548` | explicit zero; runner not beyond the line test and `s*x` below `F6 - c` | 0 |
| E3 | 0x61fc31 `xorps`; `jmp 0x620540` | explicit zero; state gates (a40050, state 0x21, state 1 with record+0x29c) or `FUN_140a4e120(0x2b) < int(fps/2+0.5)` | 0 |
| E4 | 0x61fc93 `movaps %xmm7,%xmm0`; `jmp 0x620527` | xmm7 = 0 from `xorps %xmm7,%xmm7` at 0x61fc71 | 0 |
| E5 | 0x61fca8 `xorps`; `jmp 0x620527` | explicit zero; second `FUN_140a4c720` test false | 0 |
| E6 | 0x61fd1d `movaps %xmm7,%xmm0`; `jmp 0x62051e` | xmm7 = 0; the `comiss` band tests and the marker test (`FUN_140a62850 > 21`) | 0 |
| E7 | 0x61fe14 `movaps %xmm7,%xmm0`; `jmp 0x62050c` | xmm7 = 0; reached when the `comiss` at 0x61fe0f finds the speed-tier test false (`FUN_14043c430` at 0x61fe02, times 0x258c260) | 0 |
| E8 | 0x620580 `movaps %xmm7,%xmm0`; `jmp 0x6204fa` | xmm7 = 0; the common veto target: every `ja`/`jne`/`je` in the slot-loop and gate block that names 0x620580 (for example 0x61ff14, 0x61ff2d, 0x61ff4d, 0x61ff7e, 0x61ff8f, 0x61fff7, 0x620002, 0x61ffbf, 0x61ffc9, 0x6200cc, 0x620143, 0x6204d8) | 0 |

## The success path (score)

Loop over the 11 team slots, 0x620410-0x6204e5 (`inc %dil; cmp $0xb; jb 0x620410`). A veto in any slot goes to E8 (0). When the loop ends, 0x6204eb loads the runner id from `0x168(%rsp)` (the Win64 home slot of rdx, stored at 0x61fa40 `mov %edx,0x10(%rsp)`) and calls `FUN_14061f8b0` at 0x6204f5. The return value in xmm0 is the score.

`FUN_14061f8b0` (raw 0x61f8b0; tail at 0x61f925-0x61f93b: `movss K,%xmm0; subss %xmm1,%xmm0; ret`): `score = 10000 - fVar5`, where `fVar5` is the float at `MA+0x22e4c + 200*runner + 4 + 8*(rank + teamBase)`, `rank = byte at (MA+0x22e4c + 200*runner + 0xb2 + receiver)`, `teamBase = 0 if receiver < 11 else 11`. The list is the runner's squared-distance list, so `fVar5 = d^2(runner, receiver)`. `receiver = FUN_1405969b0(ctx, team)`. If the receiver is not a valid id (>= 22), `fVar5 = 0` and the score is 10000.

## Executor (FUN_14061f940, raw 0x61f940-0x61fa33)

`link = 0` at entry. Lane 1 test (0x620ca0) true: `link = 1`, score via 0x61f8b0. Else lane 2 test (0x6210e0) true: `link = 2`, score via 0x61f8b0. Else lane 0: `call 0x61fa40`, score is lane 0's xmm0 (above), link stays 0. The executor's own epilogue (0x61fa19-0x61fa33) writes no xmm0.

## What this means under the stand-ins

Lane 1 and lane 2 both start with `FUN_140a50250(team,k) >= 1` (k = 0 and k = 2). Those are the ball holder's in-match dwords, which GF does not carry (named stand-in: 0). So lane 1 and lane 2 are never taken, and PostPlay's score under GF is 10000 - d^2 exactly when lane 0's gates all pass, else 0. Lane 0 itself reads `FUN_140a50250(team,0)` only as the flag bVar3 (`setg`, 0x61fb2a), which is false under the stand-in, so lane 0's bVar3 branches take their false side.

## Open items

- The lane-0 gate predicates (the 14 exits' conditions) are read from asm, not yet ported. Helpers still needed by the gates: `FUN_140a4e120` (count), `FUN_140a4e720` (cone), `FUN_140a8ff10`, `FUN_140458740`, `FUN_140a62960`, `FUN_140a62690`, `FUN_140a53da0` (GF has A53da0), `FUN_140a64030` (skill test; GF `PlayerSkills::Has` plus the position table).
- The success score depends on the list, which GF builds from current positions (equivalent at tick time if the list is rebuilt each tick; not verified).
