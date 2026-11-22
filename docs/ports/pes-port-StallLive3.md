# pes-port-StallLive3: the trap-end DRIBBLE gate and the holder field it reads (StallLive, 19-11-26)

Status: candidate, NOT green. Branch `StallLive` at **7534f5b4ba388a9ede608f9288be0476bffd0cc2** (ls-remote confirmed; parent 1d93bdb8 = merged OneWorld-series-wip). The live-ball census goes **606 -> 36** firings. One regression on this candidate: `PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver` fails. Main decides the gate.

## 1. Which gate refuses the trapping man (CONFIRMED, traces in the census, then reverted)

Trap-end family, translated: `pes_fn_140796CE0` (closure) -> `pes_fn_14079EE70` (trapaction, admit at 0x14079F17D via `pes_fn_1406B4580(anime, 4)`) -> DRIBBLE handler `pes_fn_1407BA840` (via action object +0x68, `0x1407BA840`).

Census, 606 firings (base), labels traced:
- `14079EE70` ran **50** times: **1 admitted**, **49 refused**. 45 refusals leave right after `1406EC5F0` (route F2C-F4A-F52-L009-L01D-L327). 2 refusals went through the DRIBBLE handler and returned 0. 1407BA840 ran 3 times.
- `1406EC5F0` (closure, interface query at `anime+0xAB8`): result = (interface `vt+8` value B == `FUN_140A627E0(ctx,0)` value C) ? 1 : 0. C is MA+0x2601C (the PES holder). B is the trapping man's own slot. So the admission needs **trapper == MA+0x2601C**.
- Refusals are trapper != MA+0x2601C.

The forced men are non-holders (`owner=0`), so their refusals are the correct PES answer. The question is the holder: `pesHolder` (census) stood **>= 4 m** from the ball on 510 of 606 holder samples (STALLHOLDER lines, census7).

## 2. The input GF fed wrongly (CONFIRMED)

- W1 (0x140457070) is not translated and has no translated caller: nothing in the PES world refreshes MA+0x2601C from the holder (HolderRE section 2: "only W1 writes it; nothing clears it").
- GF wrote MA+0x2601C in two places from `GetBallRetainer()`: `team.cpp:434` (`SetMaOwner`) and `elizacontroller.cpp:2858`. GF's retainer clears on a loose ball (humanoid.cpp:1387), so the field read "no owner" while PES keeps the last toucher.
- Fix: both writers take `Match::GetPesHolder()` (GF's sticky holder, `RecomputePesHolder`, match.hpp:278-300).

## 3. Test (red first, then green)

- `PesStall.TheWorldHolderFieldIsThePesHolderEveryTick` (new probe, pes_stall_test.cpp): each tick both sides' MA+0x2601C must equal the holder's slot (or kMiNoSlot).
  - Before (pre-fix binary = the mutation, i.e. the fix reverted): **61,455 of 72,504** side-ticks mismatch. FAILED.
  - After: **PASSED** (476 s).

## 4. Measurements (census `PesStall.ALiveBall*`, seed 7, 2 min)

| Config | live firings | notes |
|---|---|---|
| base 1d93bdb8 | 606 | 7 episodes |
| fix (this commit) | **36** | 2 episodes (HighPass carrier-brain clip, 0.8-1.2 m, holder far) |
| fix + `hasIt = PES holder` (step 3, no carry) | match **does not reach full time** (tick cap, 280 firings before it) | rejected, reverted |
| DRIBBLE carry (earlier round) | 920 | rejected |

- DRIBBLE (stored ac8 = 4) never appears in the stall. Trap ends are non-holders (refused correctly). The holder does not trap, so no trap-end DRIBBLE is due for him. Step 2's carry is unnecessary; it is not in the branch.

## 5. Suites on 7534f5b4 (fix; hasIt reverted)

- `pes_restart_delay_test --gtest_filter=PesStall.*` (sim slot, 2,985 s with the throw-in and pass suites in parallel): **7 PASSED**, 1 FAILED: `ALiveBallNeverLiesStillLongEnoughForTheBreaker` (36, red, the open item). `ATrapTouchIsTheBodyScansGrantAndNothingElse` now PASSES (StallContact's probe fix from the merge).
- `pes_pass_target_test`: **FAILED** `PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver` ("PassGetReceiver() was null for every pass of the match", "no pass in flight"). It passed on 81edc15d. The seed-7 match now has no pass at all: the owner feed changes the trajectory, so this is a regression, not a fixed pass rule. **Open: Main must decide** (the test's precondition is the match having passes).
- `pes_throwin_release_test`: 5 of 6 run OK before the sim cap; `TheThrowInTakersAnklesStayBehindTheLine` FAILED: **2 of 109** taking ticks short by 0.0155 m (was 11 of 233 on the earlier round). Reported, not chased (seed lists are Main's).
- `PesHang` (`pes_hang_test`): built; not run (budget).
- Movestep unit tests: not rerun this round (no movestep change).

## 6. Mutation

The pre-fix binary is the mutation: reverting `team.cpp:434` and `elizacontroller.cpp:2858` to `GetBallRetainer()` turns `TheWorldHolderFieldIsThePesHolderEveryTick` red (61,455 side-ticks), and the census returns to 606.

## 7. Open items

1. `PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver`: the match has no pass after the fix. Find which pass reader expects the retainer in MA+0x2601C (`pesmovestep.cpp:145` reads it as the side's ball owner, `RunPesPassTarget`), and make it PES-faithful. Blocker for landing.
2. The 36 remaining firings: the HighPass carrier-brain clip of a non-holder 0.8-1.2 m from the ball (`hasIt` GF band). `hasIt = PES holder` alone stops the match (step 3). Needs the reaction TRAP for that man (PES entry 3, `_ReceiveCandidate`) to admit him before the band runs.
3. Throw-in ankle: 2 of 109 taking ticks (Main re-census).
4. Not run: PesHang, the fast gate.
