# pes-port-StallLive: the live-ball breaker census classified; the holder DRIBBLE port is blocked on an untranslated handler (StallLive, 19-11-26)

Status: NOT GREEN. `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` is still red at 606 live firings (baseline unchanged). No production change landed (both experiments were reverted). Landed on the branch: the env-gated per-firing census dump in `tests/sim/pes_stall_test.cpp` (`GF_STALL_DUMP=1`, test-only). Base `origin/OneWorld-series-wip` f2458223. Worktree `<workdrive>/gf-StallLive`, scratch `<workdrive>/gf-StallLive-scratch/`.

Tags: CONFIRMED = measured in this tree (census run, test output); INF = inference (reason given); OPEN = not settled (blocker named).

## 1. Classification of the 606 live firings (CONFIRMED, census1.log, seed 7, 2 min, PES loop)

Every firing has ball speed 0.08 m/s (< 0.5 breaker bound) and `GetBallRetainer()==0`. The dump line: `STALLDUMP t= man= team= clip= fn= frame= ac8= ac9= holder= retainer= ms= dist= ballv= owner= pesHolder=`.

**Episodes (7; a gap over 200 ms starts a new one):**

| # | t (s) | firings | forced man (team, id) | clip (fn) | ac8 |
|---|---|---|---|---|---|
| 1 | 160.3-164.8 | 248 | T0 #8, also T1 #21, #18, #10 | sliding (13), trip (12), idle run (14), shot (8) | 2 |
| 2 | 269.3-270.5 | 68 | T1 #20 | highpass (6) | 2 |
| 3 | 280.5-280.7 | 13 | T1 #19 | highpass (6) | 2 |
| 4 | 314.9-317.3 | 127 | T1 #22 | pass (4) | 2 |
| 5 | 429.6-430.2 | 36 | T0 #6 | trap clip (14) | 2 |
| 6 | 591.9-592.4 | 28 | T1 #28 | trip (12) | 2 |
| 7 | 639.9-641.5 | 86 | T1 #30 | sliding (13) and trap (14) | 2, then 7 |

**Forced man, by his clip and body action (CONFIRMED, pre-owner census0.log; same 606 ticks):**

| Class (fn, ac8) | Firings | of which distance < 1 m |
|---|---|---|
| MOVE walk/idle/run (fn 14, ac8 2) | 256 | 153 |
| Sliding (fn 13) | 111 | 47 |
| Trip (fn 12) | 95 | 15 |
| Highpass (fn 6) | 40 | 40 |
| Pass (fn 4) | 35 | 35 |
| Trap clip (fn 14, ac8 7) | 30 | 30 |
| Shot (fn 8) | 25 | 19 |
| Stand-up (fn 1) | 14 | 10 |

Distance: 349 firings under 1 m, 130 at 1-2 m, 127 at 2 m or more.

**Owner and holder (CONFIRMED, census1.log):**
- `owner=0` on 606 of 606: the forced man is never the PES designated owner (MA+0x2600C).
- `holder=1` (forced man is the PES holder) on only 13 firings (stand-up clips). On the other 593 a PES holder exists and is another man: `pesHolder` = team*100+id: 123 (248 firings, episode 1), 124 (127), 125 (81), 128 (86), 9 (36), 8 (28).
- So in every episode the PES holder is a man who stands beside the still ball in MOVE, and the forced man is a non-holder who is not the designated owner.

**Reading (INF, from the decodes):** the non-holder's clip is GF's own carrier brain at work. `hasIt` (`elizacontroller.cpp`, the possession band `AI_HasPossession` 1.0 m) is true for a man 1 m from a loose ball, so `GetOnTheBallCommands` queues shot, pass, trip and slide. PES never queues those for a non-holder: a non-holder touches through reaction entry 3's TRAP. The holder is PES's MOVE holder: he touches only on a DRIBBLE tick (pes-re-holder-clear.md section 3). GF never produces DRIBBLE, because `TrapFactsOf` (`teamAIcontroller.cpp:3575`) gives MOVE to every man, and `WriteActorP` stores that back into P+0xAC8 each tick (`pestrapactor.cpp:166`). So the body action byte is MOVE (2) for everyone except a live trap (7).

## 2. Experiments (CONFIRMED runs; reverted)

| Experiment | Result |
|---|---|
| E0: the owner's `hasIt = PES holder` (commit f2458223's message) | 606 to 1149 firings (commit message; not re-measured here). The holder stands in the carrier brain with no DRIBBLE. |
| E1: carry the stored action pair (P+0xAC8/0xAC9, prev pair) into the facts in `WritePesTrapActors`, for any stored action != 0 | Aborted at 53 s: `PES_DISPATCH_UNTRANSLATED: 0x1407c4fa0` (`pes_dispatch_missing`, `pes_dispatch_gen.cpp:20332`). Core dump. |
| E2: carry only DRIBBLE (4) | Aborted at 75 s with the same `PES_DISPATCH_UNTRANSLATED: 0x1407c4fa0`. Core dump. |

E2 shows the DRIBBLE state reaches a PES handler that the generated dispatcher does not translate. 0x1407C4FA0 is not in `src/pes/translated` (grep: no file). Which slot calls it is OPEN (decode the DRIBBLE action's vtable slots, or the handler it routes to). Translating it is a layer regeneration (`gen_translated.py --image`, about 10 min) plus a bit-exact check. That did not fit this run's budget.

## 3. Tests

- `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`: red, liveFirings 606 (unchanged from f2458223). The census reached full time: the only failing assertion is liveFirings == 0.
- Other `PesStall.*`, `pes_throwin_release_test`, `pes_pass_target_test`, movestep tests: NOT run (no production change remains on the branch, so the base result stands).
- Mutation: not run (no production fix).
- Fast gate: not run (no production change; agents do not gate; Main gates).

## 4. Next steps (in order)

1. Decode the caller and slot of 0x1407C4FA0 in the DRIBBLE chain (the `dribble_*` dispatch: `FUN_1407B37D0` slots at 0x1425E2460 and the DRIBBLE vtable base 0x1425E2440). Translate that function (regenerate the layer). Settles E2.
2. Re-run E2 (carry DRIBBLE only). If the holder's DRIBBLE tick admits TRAP on the resting ball, the 248-firing episode 1 should close first (the holder beside the ball, pesHolder 123).
3. Then the non-holder: the forced man's carrier-brain clips (the 1.0 m `hasIt` band). Make `hasIt` the PES holder once the holder's DRIBBLE exists (the f2458223 revert is the fix's other half).
4. Presser clips (MOVESTEP gate on the GF clip, pes-re-move-gate.md): 153 of the 256 walkers are under 1 m; lower priority than 1-3.

## 5. Blockers

None for the next step's decode. The translation regeneration needs a free build lock window (about 10 min).
